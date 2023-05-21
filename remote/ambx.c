/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		ambx.c
 *	DESCRIPTION:	Apollo mailbox communication routines
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "../jrd/ib_stdio.h"

#ifdef SR95
#include "/sys/ins/base.ins.c"
#include "/sys/ins/mbx.ins.c"
#include "/sys/ins/error.ins.c"
#include "/sys/ins/ms.ins.c"
#include "/sys/ins/name.ins.c"
#include "/sys/ins/proc2.ins.c"
#include "/sys/ins/pm.ins.c"
#define SR10_REF(a) a
#define SR10_VAL(a) *a
#else
#include <apollo/base.h>
#include <apollo/mbx.h>
#include <apollo/error.h>
#include <apollo/loader.h>
#include <apollo/ms.h>
#include <apollo/name.h>
#include <apollo/proc2.h>
#include <apollo/pm.h>
#include <apollo/ec2.h>
#include <apollo/task.h>
#define SR10_REF(a) &a
#define SR10_VAL(a) a
#endif

#include "../remote/remote.h"
#include "../jrd/gds.h"
#include "../jrd/thd.h"
#include "../remote/ambx_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"

#define SERVER_NAME	"gds.server"

extern TEXT	*getenv();

static USHORT	AMBX_force, initialized;

#define BUFFER_SIZE	1024
#define	MAXCHAN		64
#define DATAGRAM_RETRY	10

static int	accept 		(PORT, P_CNCT *);
static int	accept_connection (PORT);
static PORT	alloc_port 	(PORT);
static int	analyze 	(UCHAR *, USHORT, UCHAR *);
static PORT	aux_connect 	(PORT, PACKET *);
static PORT	aux_request 	(PORT, PACKET *);
static void	cleanup 	(void *);
static void     cleanup_port    (PORT);
static void	disconnect 	(PORT);
static int	expand 		(TEXT *, USHORT, TEXT *);
static int	make_local_mailbox (TEXT *, SLONG, TEXT *);
static int	make_mailbox 	(TEXT *, SLONG, TEXT *);
static XDR_INT	mbx_destroy 	(XDR *);
static int	mbx_error 	(PORT, TEXT *, STATUS);
static bool_t	mbx_getbytes 	(XDR *, SCHAR *, int);
static bool_t	mbx_getlong 	(XDR *, SLONG *);
static u_int	mbx_getpostn 	(XDR *);
static caddr_t  mbx_inline 	(XDR *, u_int);
static void	mbx_log_error 	(STATUS *);
static bool_t	mbx_putbytes 	(XDR *, SCHAR *, int);
static bool_t	mbx_putlong 	(XDR *, SLONG *);
static bool_t	mbx_read 	(XDR *);
static bool_t	mbx_setpostn 	(XDR *, u_int);
static bool_t	mbx_write 	(XDR *, bool_t);
static void	move 		(UCHAR *, UCHAR *, USHORT);
static PORT	receive 	(PORT, PACKET *);
static int	send_full 	(PORT, PACKET *);
static int	send_partial 	(PORT, PACKET *);
static int	xdrmbx_create 	(XDR *, 	
					SCHAR *, 
					int, 
					PORT, 
					enum xdr_op);
static int	xdrmbx_endofrecord (XDR *, bool_t);

static struct xdr_ops	mbx_ops =
	{
	mbx_getlong,
	mbx_putlong,
	mbx_getbytes,
	mbx_putbytes,
	mbx_getpostn,
	mbx_setpostn,
	mbx_inline,
	mbx_destroy};

std_$call void	name_$resolve(),
		file_$read_lock_entryu();

typedef SLONG	node_t;
typedef SLONG	network_t;

typedef short enum {
    file_$nr_xor_lw,
    file_$cowriters
} file_$obj_mode_t;

typedef short enum {
    file_$all,
    file_$read,
    file_$read_intend_write,
    file_$chng_read_to_write,
    file_$write,
    file_$chng_write_to_read,
    file_$chng_read_to_riw,
    file_$chng_write_to_riw,
    file_$mark_for_del
} file_$acc_mode_t;

typedef struct {
    uid_$t		ouid;
    uid_$t		puid;
    file_$obj_mode_t	omode;
    file_$acc_mode_t	amode;
    SSHORT		transid;
    node_t		node;
} file_$lock_entry_t;

#define CHECK		if (status.all) error (status);
#define NODE_MASK	0xFFFFF

/* Asknode Stuff */

std_$call void	asknode_$info();

typedef short enum {
    ask_who, who_r,
    ask_time, time_r,
    ask_node_root, node_root_r
} asknode_$kind_t;

#define asknode_$version	2

typedef struct {
    pinteger		version;
    asknode_$kind_t	kind;
    status_$t		status;
    uid_$t		uid;
    SCHAR		stuff [256];	/* Random and various */
} asknode_$reply_t;

/* name_$ stuff */

std_$call void	name_$gpath_dir ();
#ifdef SR95
std_$call void	name_$get_path_cc();
std_$call int	*kg_$lookup();
#endif

short enum name_$namtyp_t {
    name_$junk_flag,
    name_$wdir_flag,
    name_$ndir_flag,
    name_$node_flag,
    name_$root_flag,
    name_$node_data_flag
};

PORT AMBX_analyze (
    UCHAR	*file_name,
    USHORT	*file_length,
    STATUS	*status_vector,
    TEXT	*user_string,
    USHORT	uv_flag)
{
/**************************************
 *
 *	A M B X _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Analyze a file specification and determine whether a remote
 *	server is required, and if so, what protocol to use.  If the
 *	database can be accessed locally, return the value FALSE with a
 *	NULL connection block.  If a "full context" server is to be used,
 *	return TRUE and the address of a port block with which to communicate
 *	with the server.  If a page/lock server is to be used, return the
 *	value FALSE and the address of a connection block.
 *
 *	Note: The file name must have been expanded prior to this call.
 *
 **************************************/
RDB		rdb;
PORT		port;
PACKET		*packet;
P_CNCT		*connect;
P_ATCH		*attach;
UCHAR		mailbox [256], ident [256], expanded_name [256], *p, *q;
UCHAR		name [128], proj [32], org [32];
TEXT		buffer[64];
int		uid, gid;
USHORT		length, expanded_length;
SSHORT		ident_length;
STATUS		local_status [20];
struct p_cnct_repeat	*protocol;

/* Analyze the file name to see if a remote connection is required.  If not,
   quietly (sic) return. */

expanded_length = expand (file_name, *file_length, expanded_name);

if (!analyze (expanded_name, expanded_length, mailbox))
    {
    if (!AMBX_force)
	return NULL;
    make_local_mailbox (mailbox);
    }

/* We need to establish a connection to a remote server.  Allocate the necessary
   blocks and get ready to go. */

rdb = (RDB) ALLOC (type_rdb);
#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("AMBX_analyze              allocate rdb     %x\n", rdb);
#endif
packet = &rdb->rdb_packet;

/* Get user identification info */

p = ident;
*p++ = CNCT_ppo;
ISC_get_user (name, NULL_PTR, NULL_PTR, proj, org, NULL_PTR, user_string);
q = ++p;
strcpy (p, name);
while (*p)
    p++;
*p++ = '.';
strcpy (p, proj);
while (*p)
    p++;
*p++ = '.';
strcpy (p, org);
while (*p)
    p++;
length = strlen (q);
*--q = length;

*p++ = CNCT_host;
p++;
ISC_get_host (p, 64);
p [-1] = (UCHAR) strlen ((SCHAR*) p);

for (; *p; p++)
    if (*p >= 'A' && *p <= 'Z')
        *p = *p - 'A' + 'a';

if (!proj [0] || uv_flag)
    {
    *p++ = CNCT_user_verification;
    *p++ = 0;
    }

ident_length = p - ident;

/* Establish connection to server.  Start by telling the server about
   the protocols that we know.  The server will then pick at most one. */

connect = &packet->p_cnct;
packet->p_operation = op_connect;
connect->p_cnct_operation = op_attach;
connect->p_cnct_cversion = CONNECT_VERSION2;
connect->p_cnct_client = ARCHITECTURE;
connect->p_cnct_file.cstr_length = expanded_length;
connect->p_cnct_file.cstr_address  = expanded_name;

/* Note: prior to V3.1E a recievers could not in truth handle more
   then 5 protocol descriptions, so we try them in chunks of 5 or less */

/* If we need user verification, cannot talk less than protocol 7 */

#ifdef SCROLLABLE_CURSORS
connect->p_cnct_count = 4;
#else
connect->p_cnct_count = 2;
#endif
connect->p_cnct_user_id.cstr_length = ident_length;
connect->p_cnct_user_id.cstr_address = ident;

protocol = connect->p_cnct_versions;

protocol->p_cnct_version = PROTOCOL_VERSION8;
protocol->p_cnct_architecture = arch_generic;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = ptype_batch_send;
protocol->p_cnct_weight = 2;

++protocol;

protocol->p_cnct_version = PROTOCOL_VERSION8;
protocol->p_cnct_architecture = ARCHITECTURE;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = ptype_batch_send;
protocol->p_cnct_weight = 3;

#ifdef SCROLLABLE_CURSORS
++protocol;

protocol->p_cnct_version = PROTOCOL_SCROLLABLE_CURSORS;
protocol->p_cnct_architecture = arch_generic;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = ptype_batch_send;
protocol->p_cnct_weight = 4;

++protocol;

protocol->p_cnct_version = PROTOCOL_SCROLLABLE_CURSORS;
protocol->p_cnct_architecture = ARCHITECTURE;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = ptype_batch_send;
protocol->p_cnct_weight = 5;
#endif

/* If we can't talk to a server, punt.  Let somebody else generate
   an error. */

if (!(port = AMBX_connect (mailbox, packet, status_vector)))
    {
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("AMBX_analyze              free rdb         %x\n", rdb);
#endif
    ALLR_release (rdb);
    return NULL;
    }

/* Get response packet from server. */

rdb->rdb_port = port;
port->port_context = rdb;
RECEIVE (port, packet);

if (packet->p_operation == op_reject && !uv_flag)
    {
    disconnect (port);
    packet->p_operation = op_connect;
    connect->p_cnct_operation = op_attach;
    connect->p_cnct_cversion = CONNECT_VERSION2;
    connect->p_cnct_client = ARCHITECTURE;
    connect->p_cnct_file.cstr_length = expanded_length;
    connect->p_cnct_file.cstr_address  = expanded_name;
    connect->p_cnct_user_id.cstr_length = ident_length;
    connect->p_cnct_user_id.cstr_address = ident;

    /* try again with next set of known protocols */

    connect->p_cnct_count = 4;
    protocol = connect->p_cnct_versions;

    protocol->p_cnct_version = PROTOCOL_VERSION6;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 2;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION6;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 3;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION7;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 4;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION7;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 5;

    if (!(port = AMBX_connect (mailbox, packet, status_vector)))
        {
#ifdef REMOTE_DEBUG_MEMORY
        ib_printf ("AMBX_analyze              free rdb         %x\n", rdb);
#endif
        ALLR_release (rdb);
        return NULL;
        }

    /* Get response packet from server. */

    rdb->rdb_port = port;
    port->port_context = rdb;
    RECEIVE (port, packet);
    }

if (packet->p_operation == op_reject && !uv_flag)
    {
    disconnect (port);
    packet->p_operation = op_connect;
    connect->p_cnct_operation = op_attach;
    connect->p_cnct_cversion = CONNECT_VERSION2;
    connect->p_cnct_client = ARCHITECTURE;
    connect->p_cnct_file.cstr_length = expanded_length;
    connect->p_cnct_file.cstr_address  = expanded_name;
    connect->p_cnct_user_id.cstr_length = ident_length;
    connect->p_cnct_user_id.cstr_address = ident;

    /* try again with next set of known protocols */

    connect->p_cnct_count = 4;
    protocol = connect->p_cnct_versions;

    protocol->p_cnct_version = PROTOCOL_VERSION3;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 2;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION3;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 3;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION4;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 4;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION4;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 5;

    if (!(port = AMBX_connect (mailbox, packet, status_vector)))
        {
#ifdef REMOTE_DEBUG_MEMORY
        ib_printf ("AMBX_analyze              free rdb         %x\n", rdb);
#endif
        ALLR_release (rdb);
        return NULL;
        }

    /* Get response packet from server. */

    rdb->rdb_port = port;
    port->port_context = rdb;
    RECEIVE (port, packet);
    }

if (packet->p_operation != op_accept)
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__connect_reject;
    *status_vector++ = 0;
    disconnect (port);
    return NULL;
    }

/* The server has picked a protocol.  Find out which. */

port->port_protocol = packet->p_acpt.p_acpt_version;

/* once we've decided on a protocol, concatenate the version
   string to reflect it...  */

sprintf (buffer, "%s/P%d", port->port_version->str_data, port->port_protocol);
ALLR_free (port->port_version);
port->port_version = REMOTE_make_string (buffer);

if (packet->p_acpt.p_acpt_architecture == ARCHITECTURE)
    port->port_flags |= PORT_symmetric;

if (packet->p_acpt.p_acpt_type == ptype_rpc)
    port->port_flags |= PORT_rpc;

/* Return expanded file name */

move (expanded_name, file_name, expanded_length);
*file_length = expanded_length;

return port;
}

PORT AMBX_connect (
    TEXT	*name,
    PACKET	*packet,
    STATUS	*status_vector)
{
/**************************************
 *
 *	A M B X _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Establish half of a communication link.  If a connect packet is given,
 *	the connection is on behalf of a remote interface.  Other the connect
 *	is is for a server process.
 *
 **************************************/
mbx_$server_msg_t	buffer, *data;
XDR		xdr;
SLONG		length;
PORT		port;
TEXT		*operation, mailbox [256], *p, expanded_name [256];
SSHORT		l, name_length;
status_$t	status;

port = alloc_port (NULL);
port->port_status_vector = status_vector;

if (packet)
    {
    data = (mbx_$server_msg_t*) buffer.data;
    xdrmem_create (&xdr, data, mbx_$msg_max, XDR_ENCODE);
    if (!xdr_protocol (&xdr, packet))
	return NULL;
    length = xdr_getpostn (&xdr);
    name_length = strlen (name);
    THREAD_EXIT;
    mbx_$open (SR10_VAL (name), name_length, data, length, SR10_REF (port->port_handle), SR10_REF (status));
    if (status.all)
	{
	for (p = name + 2, l = name_length - 2; l > 0; p++, l--)
	    if (*p >= 'A' && *p <= 'Z')
		*p = *p - 'A' + 'a';
	    else if (*p == '/')
		break;
	mbx_$open (SR10_VAL (name), name_length, data, length, SR10_REF (port->port_handle), SR10_REF (status));
	}
    THREAD_ENTER;
    operation = "mbx_$open";
    }
else
    {
    name = mailbox;
    name_length = make_local_mailbox (name);
    port->port_server_flags = TRUE;
    mbx_$create_server (SR10_VAL (name), name_length, (SSHORT) sizeof (buffer), 
	(SSHORT) mbx_$chn_max, SR10_REF (port->port_handle), SR10_REF (status));
    operation = "mbx_$create_server";
    expand (name, name_length, expanded_name);
    if (port->port_connection)
	ALLR_free (port->port_connection);
    port->port_connection = REMOTE_make_string (expanded_name);
    }

if (status.all)
    {
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("AMBX_connect              free port        %x\n", port);
#endif
    cleanup_port (port);
    mbx_error (port, operation, status);
    return NULL;
    }

return port;
}

int AMBX_mailbox (
    UCHAR	*name,
    SLONG	target,
    UCHAR	*mailbox)
{
/**************************************
 *
 *	A M B X _ m a i l b o x
 *
 **************************************
 *
 * Functional description
 *	Make up a mailbox name to talk to a server.  The
 *	connection may be either by node name or by
 *	UID.
 *
 **************************************/
SCHAR		root_name [256];
uid_$t		uid;
USHORT		length;
status_$t	status;
asknode_$reply_t	reply;

/* Resolve the name to get the UID.  Again, if any error, just give up. */

if (name)
    {
    length = strlen (name);
    name_$resolve (*name, length, uid, status);
    if (status.all)
	return 0;
    target = uid.low & NODE_MASK;
    }

/* We've got server UID, get pathname */

asknode_$info (ask_node_root, target, 0, reply, status);
if (status.all)
    {
    error_$print (status);
    return 0;
    }

if (reply.status.all)
    {
    error_$print (reply.status);
    return 0;
    }

name_$gpath_dir (reply.uid, name_$root_flag, root_name, length, status);
if (status.all)
    {
    error_$print (status);
    return 0;
    }

root_name [length] = 0;

return make_mailbox (root_name, target, mailbox);
}

void AMBX_set_debug (
    int		value)
{
/**************************************
 *
 *	A M B X _ s e t _ d e b u g
 *
 **************************************
 *
 * Functional description
 *	Set debugging mode.
 *
 **************************************/

if (!initialized)
    {
    gds__register_cleanup (cleanup, NULL_PTR);
    initialized = 1;
    }
AMBX_force = (value & 1);
}

static int accept (
    PORT	port,
    P_CNCT	*connect)
{
/**************************************
 *
 *	a c c e p t
 *
 **************************************
 *
 * Functional description
 *	Accept an incoming request for connection.  This is purely a lower
 *	level handshaking function, and does not constitute the server
 *	response for protocol selection.
 *
 **************************************/
UCHAR			*id, *end, *p, *q;
STR			string;
SLONG			length, user_verification;

id = connect->p_cnct_user_id.cstr_address;
end = id + connect->p_cnct_user_id.cstr_length;

user_verification = 0;
while (id < end)
    switch (*id++)
	{
	case CNCT_ppo:
	    length = *id++;
	    port->port_user_name = string = (STR) ALLOCV (type_str, length);
#ifdef REMOTE_DEBUG_MEMORY
            ib_printf ("accept(ambx)              allocate string  %x\n", string);
#endif
	    string->str_length = length;
	    if (length)
		{
		p = (UCHAR*) string->str_data;
		do *p++ = *id++; while (--length);
		}
	    break;
	
	case CNCT_user_verification:
	    user_verification = 1;
	    id++;
	    break;

	default:
	    id += *id + 1;
	}

return accept_connection (port);
}

static accept_connection (
    PORT	port)
{
/**************************************
 *
 *	a c c e p t _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Accept physical connection.
 *
 **************************************/
status_$t		status;
mbx_$server_msg_t	message, *msg;

message.chan = port->port_channel;
message.cnt = sizeof (mbx_$msg_hdr_t);
message.mt = mbx_$accept_open_mt;
msg = &message;

mbx_$put_rec (port->port_handle, msg, (SLONG) sizeof (mbx_$msg_hdr_t), SR10_REF (status));

if (status.all)
    {
    mbx_error (port, "mbx_$put_rec", status);
    return FALSE;
    }

return TRUE;
}

static PORT alloc_port (
    PORT	parent)
{
/**************************************
 *
 *	a l l o c _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Allocate a port block, link it in to parent (if there is a parent),
 *	and initialize input and output XDR streams.
 *
 **************************************/
mbx_$server_msg_t	*message;
uid_$t			uid;
PORT			port;
TEXT			version [32];

port = (PORT) ALLOCV (type_port, sizeof (mbx_$server_msg_t) * 2);
#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("alloc_port(ambx)          allocate port    %x\n", port);
#endif
port->port_type = port_mailbox;
port->port_state = state_pending;

proc2_$who_am_i (SR10_REF (uid));
sprintf (version, "mbx (%x)", uid.low & NODE_MASK);
port->port_version = REMOTE_make_string (version);
port->port_host = REMOTE_make_string ("");

if (parent)
    {
    port->port_parent = parent;
    port->port_next = parent->port_clients;
    parent->port_clients = port;
    port->port_handle = parent->port_handle;
    port->port_server = parent->port_server;
    port->port_server_flags = parent->port_server_flags;
    }

(UCHAR*) message = port->port_buffer;
port->port_accept = accept;
port->port_disconnect = disconnect;
port->port_receive_packet = receive;
port->port_send_packet = send_full;
port->port_send_partial = send_partial;
port->port_connect = aux_connect;
port->port_request = aux_request;
port->port_buff_size = mbx_$msg_max;

xdrmbx_create (&port->port_send, &message [1], mbx_$msg_max,
	port, XDR_ENCODE);

xdrmbx_create (&port->port_receive, port->port_buffer, 0, 
	port, XDR_DECODE);

return port;
}

static int analyze (
    UCHAR	*file_name,
    USHORT	file_length,
    UCHAR	*mailbox)
{
/**************************************
 *
 *	a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Sniff out a file and determine whether it can be successfully
 *	opened locally.  If so, return 0.  Otherwise, generate a 
 *	mailbox string to establish a connection to a server that 
 *	can open the file.  Return the length of the mailbox string.
 *
 **************************************/
uid_$t		uid, process_uid;
SLONG		node, target;
status_$t	status;
file_$lock_entry_t	entry;

/* Resolve the name to get the UID.  Again, if any error, just give up. */

name_$resolve (*file_name, file_length, uid, status);
if (status.all)
    return 0;

/* Attempt to read the file lock entry for the file.  If it isn't locked, it
   can be accessed locally. */

file_$read_lock_entryu (uid, entry, status);
if (status.all)
    return 0;

/* File is apparently locked.  If it's locked by our node, everything is
   fine.  */

proc2_$who_am_i (SR10_REF (process_uid));
node = process_uid.low & NODE_MASK;
target = entry.puid.low & NODE_MASK;

if (node == target)
    return 0;

/* File is locked by somebody else.  Find who */

return AMBX_mailbox (NULL, target, mailbox);
}

static PORT aux_connect (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	a u x _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Try to establish an alternative connection.  Somebody has already
 *	done a successfull connect request ("packet" contains the response).
 *
 **************************************/
P_RESP	*response;
P_REQ	*request;
USHORT	l;
TEXT	mailbox [256];
PORT	new_port;

/* If this is a server, we're got an auxiliary connection.  Accept it */

if (port->port_server_flags)
    {
    if (!accept_connection (port))
	return NULL;
    port->port_flags |= PORT_async;
    return port;
    }

response = &packet->p_resp;
request = &packet->p_req;
l = response->p_resp_data.cstr_length;
strncpy (mailbox, response->p_resp_data.cstr_address, l);
mailbox [l] = 0;

packet->p_operation = op_aux_connect;
request->p_req_partner = response->p_resp_partner;

if (!(new_port = AMBX_connect (mailbox, packet, port->port_status_vector)))
    return NULL;

new_port->port_flags |= PORT_async;
port->port_async = new_port;

return new_port;
}

static PORT aux_request (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	a u x _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	A remote interface has requested the server prepare an auxiliary 
 *	connection; the server calls aux_request to set up the connection.
 *
 **************************************/
PORT	parent;
P_RESP	*response;

response = &packet->p_resp;
parent = port->port_parent;
response->p_resp_data.cstr_address = (UCHAR*) parent->port_connection->str_data;
response->p_resp_data.cstr_length = strlen (parent->port_connection->str_data);
response->p_resp_partner = (ULONG) port;

return NULL;
}

static void cleanup (
    void	*arg)
{
/**************************************
 *
 *	c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Module level cleanup handler.
 *
 **************************************/

AMBX_force = initialized = 0;
}

static void disconnect (
    PORT	port)
{
/**************************************
 *
 *	d i s c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Break a remote connection.
 *
 **************************************/
status_$t	status;
PORT		*p;

/* Close a client channel */

if (port->port_parent)
    {
    if (port->port_async)
	{
	disconnect (port->port_async);
	port->port_async = NULL;
	}
    mbx_$deallocate (port->port_handle, (SSHORT) port->port_channel, SR10_REF (status));
    for (p = &port->port_parent->port_clients; *p; p = &(*p)->port_next)
	if (*p == port)
	    {
	    *p = port->port_next;
	    break;
	    }
    }
else
    {
    if (port->port_async)
	port->port_async->port_flags |= PORT_disconnect;
    mbx_$close (port->port_handle, SR10_REF (status));
    }

#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("disconnect(ambx)          free port        %x\n", port);
#endif
cleanup_port (port);
}

static void cleanup_port (
    PORT        port)
{
/**************************************
 *
 *      c l e a n u p _ p o r t
 *
 **************************************
 *
 * Functional description
 *      Walk through the port structure freeing
 *      allocated memory and then free the port.
 *
 **************************************/

if (port->port_version)
    ALLR_free ((UCHAR *) port->port_version);

if (port->port_connection)
    ALLR_free ((UCHAR *) port->port_connection);

if (port->port_user_name)
    ALLR_free ((UCHAR *) port->port_user_name);

if (port->port_host)
    ALLR_free ((UCHAR *) port->port_host);

if (port->port_object_vector)
    ALLR_free ((UCHAR *) port->port_object_vector);

#ifdef DEBUG_XDR_MEMORY
if (port->port_packet_vector)
    ALLR_free ((UCHAR *) port->port_packet_vector);
#endif

ALLR_release ((UCHAR *) port);
return;
}

static int expand (
    TEXT	*file_name,
    USHORT	file_length,
    TEXT	*expanded_name)
{
/**************************************
 *
 *	e x p a n d
 *
 **************************************
 *
 * Functional description
 *	Fully expand a file name.  If the file doesn't exist, do something
 *	intelligent.
 *
 **************************************/
USHORT		length;
status_$t	status;

if (kg_$lookup ("NAME_$GET_PATH_CC                 "))
    name_$get_path_cc (SR10_VAL (file_name), file_length, SR10_VAL (expanded_name), SR10_REF (length), SR10_REF (status));
else
    name_$get_path (SR10_VAL (file_name), file_length, SR10_VAL (expanded_name), SR10_REF (length), SR10_REF (status));

if (!status.all && length != 0)
    {
    expanded_name [length] = 0;
    return length;
    }

/* File name lookup failed.  Copy file name and return. */

move (file_name, expanded_name, file_length);

return file_length;
}

static int make_local_mailbox (
    TEXT	*mailbox)
{
/**************************************
 *
 *	m a k e _ l o c a l _ m a i l b o x
 *
 **************************************
 *
 * Functional description
 *	Make up the appropriate mailbox name for this node.  This
 *	is called only by the server process to generate a mailbox
 *	string on which to listen to connections.
 *
 **************************************/
uid_$t		process_uid;
SLONG		node;

proc2_$who_am_i (SR10_REF (process_uid));
node = process_uid.low & NODE_MASK;

return make_mailbox ("", node, mailbox);
}

static int make_mailbox (
    TEXT	*root,
    SLONG	id,
    TEXT	*mailbox)
{
/**************************************
 *
 *	m a k e _ m a i l b o x
 *
 **************************************
 *
 * Functional description
 *	Generate a mailbox name for a server on a given disk name.
 *
 **************************************/
TEXT	*server_name;

if (!(server_name = getenv ("AMBX_DEBUG")))
    server_name = SERVER_NAME;

#ifdef DEBUG
sprintf (mailbox, "%s_%x", server_name, id);
#else
sprintf (mailbox, "%s/sys/%s_%x", root, server_name, id);
#endif

return strlen (mailbox);
}

static PORT receive (
    PORT	main_port,
    PACKET	*packet)
{
/**************************************
 *
 *	r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a message from a port or clients of a port.  If the process
 *	is a server and a connection request comes in, generate a new port
 *	block for the client.
 *
 **************************************/
PORT			port;
TEXT			msg [64];
SLONG			length, error_count;
status_$t		status;
mbx_$server_msg_t	*data;

port = main_port;
error_count = 10;

/* If this isn't the server, this is all quite simple */

if (!port->port_server_flags)
    {
    if (!xdr_protocol (&port->port_receive, packet))
	return NULL;
    return port;
    }

/* This is a server read, handle a multipled channel */

for (;;)
    {
    (UCHAR*) data = main_port->port_buffer;
    THREAD_EXIT;
    mbx_$get_rec (main_port->port_handle, data, (SLONG) mbx_$msg_max,
	    SR10_REF (data), SR10_REF (length), SR10_REF (status));
    THREAD_ENTER;

    if (status.all && status.all != mbx_$partial_record)
	{
	mbx_error (main_port, "mbx_$get_record", status);
	return NULL;
	}

    if (length < 0)
	length = mbx_$msg_max;

    for (port = main_port->port_clients; port; port = port->port_next)
	if (port->port_channel == data->chan)
	    break;

    switch (data->mt)
	{
	case mbx_$data_mt:
	case mbx_$data_partial_mt:
	    if (!port)
		{
		gds__log ("AMBX/receive: data from unknown client");
		if (!--error_count)
		    return NULL;
		continue;
		}
	    break;
	    
	case mbx_$channel_open_mt:
	    port = alloc_port (main_port);
	    port->port_channel = data->chan;
	    break;
	    
	case mbx_$eof_mt:
	    if (!port)
		{
		/***
		gds__log ("AMBX/receive: eof from unknown client");
		if (!--error_count)
		    return NULL;
		***/
		continue;
		}
	    packet->p_operation = op_exit;
	    port->port_state = state_eof;
	    return port;

	default:
	    sprintf (msg, "AMBX/receive: expected message type %d\n", data->mt);
	    gds__log (msg);
	    if (!--error_count)
		return NULL;
	    continue;
	}

    data = (mbx_$server_msg_t*) data->data;
    length -= sizeof (mbx_$msg_hdr_t);

    if (!port)
	{
	gds__log ("AMBX/receive: data from unknown client");
	if (!--error_count)
	    return NULL;
	continue;
	}

    xdrmbx_create (&port->port_receive, data, length, port, XDR_DECODE);

    if (!xdr_protocol (&port->port_receive, packet))
	packet->p_operation = op_disconnect;

    return port;
    }
}

static int send_full (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	s e n d _ f u l l
 *
 **************************************
 *
 * Functional description
 *	Send a packet across a port to another process.
 *
 **************************************/
mbx_$server_msg_t	message, *data;

if (!xdr_protocol (&port->port_send, packet))
    return FALSE;

return xdrmbx_endofrecord (&port->port_send, TRUE);
}

static int send_partial (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	s e n d _ p a r t i a l
 *
 **************************************
 *
 * Functional description
 *	Send a packet across a port to another process.
 *
 **************************************/
mbx_$server_msg_t	message, *data;

return xdr_protocol (&port->port_send, packet);
}

static int xdrmbx_create (
    XDR			*xdrs,
    SCHAR		*message,
    int			length,
    PORT		port,
    enum xdr_op		x_op)
{
/**************************************
 *
 *	x d r m b x _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Initialize an XDR stream for Apollo mailboxes.
 *
 **************************************/

(PORT) xdrs->x_public = port;
xdrs->x_base = xdrs->x_private = message;
xdrs->x_ops = &mbx_ops;
xdrs->x_op = x_op;
xdrs->x_handy = length;

return TRUE;
}

static int xdrmbx_endofrecord (
    XDR		*xdrs,
    bool_t	flushnow)
{
/**************************************
 *
 *	x d r m b x _ e n d o f r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Write out the rest of a record.
 *
 **************************************/

return mbx_write (xdrs, flushnow);
}

static XDR_INT mbx_destroy (
    XDR		*xdrs)
{
/**************************************
 *
 *	m b x _ d e s t r o y
 *
 **************************************
 *
 * Functional description
 *	Destroy a stream.  A no-op.
 *
 **************************************/
}

static int mbx_error (
    PORT	port,
    TEXT	*operation,
    STATUS	status)
{
/**************************************
 *
 *	m b x _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	An I/O error has occurred.  If a status vector is present,
 *	generate an error return.  In any case, return NULL, which
 *	is used to indicate and error.
 *
 **************************************/
RDB	rdb;
STATUS	*status_vector, *sv, status_local [20];

port->port_flags |= PORT_broken;
port->port_state = state_broken;

if (rdb = port->port_context)
    status_vector = sv = rdb->rdb_status_vector;
else
    status_vector = sv = port->port_status_vector;

if (!status_vector)
    status_vector = sv = status_local;

if (status_vector)
    {
    *sv++ = gds_arg_gds;
    *sv++ = gds__io_error;
    *sv++ = gds_arg_string;
    (TEXT*) *sv++ = operation;
    *sv++ = gds_arg_string;
    (TEXT*) *sv++ = "mailbox connection";
    if (status)
	{
	*sv++ = gds_arg_domain;
	*sv++ = status;
	}
    *sv++ = 0;
    }

mbx_log_error (status_vector);

return 0;
}

static bool_t mbx_getbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    int		count)
{
/**************************************
 *
 *	m b x _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Get a bunch of bytes from a memory stream if it fits.
 *
 **************************************/
SCHAR	*p;
SSHORT	n;
SLONG	bytecount = count;

/* Use memcpy to optimize bulk transfers. */

while (bytecount > sizeof (GDS_QUAD))
    {
    if (xdrs->x_handy >= bytecount)
	{
	memcpy (buff, xdrs->x_private, bytecount);
	xdrs->x_private += bytecount;
	xdrs->x_handy -= bytecount;
	return TRUE;
	}
    else
	{
	if (xdrs->x_handy > 0)
	    {
	    memcpy (buff, xdrs->x_private, xdrs->x_handy);
	    xdrs->x_private += xdrs->x_handy;
	    buff += xdrs->x_handy;
	    bytecount -= xdrs->x_handy;
	    xdrs->x_handy = 0;
	    }
	if (!mbx_read (xdrs))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

while (bytecount)
    {
    p = xdrs->x_private;
    n = MIN (bytecount, xdrs->x_handy);
    bytecount -= n;
    xdrs->x_handy -= n;
    if (n)
	do *buff++ = *p++; while (--n);
    xdrs->x_private = p;
    if (!bytecount)
	break;
    if (!mbx_read (xdrs))
	return FALSE;
    }

return TRUE;
}

static bool_t mbx_getlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	m b x _ g e t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
SLONG	l;

if (!(*xdrs->x_ops->x_getbytes) (xdrs, &l, 4))
   return FALSE;

*lp = ntohl (l);

return TRUE;
}

static u_int mbx_getpostn (
    XDR		*xdrs)
{
/**************************************
 *
 *	m b x _ g e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Get the current position (which is also current length) from stream.
 *
 **************************************/

return xdrs->x_private - xdrs->x_base;
}

static caddr_t mbx_inline (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	m b x _  i n l i n e
 *
 **************************************
 *
 * Functional description
 *	Return a pointer to somewhere in the buffer.
 *
 **************************************/

if (bytecount > xdrs->x_handy)
    return FALSE;

return xdrs->x_base + bytecount;
}

static void mbx_log_error (
    STATUS	*status)
{
/**************************************
 *
 *	m b x _ l o g _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Log error to error log.
 *
 **************************************/
TEXT	buffer [2048], *p;

if (!status)
    return;

p = buffer;
sprintf (p, "AMBX/mbx_error:");

do {
    while (*p)
	p++;
    *p++ = '\n';
    *p++ = '\t';
} while (gds__interprete (p, &status));

p [-2] = 0;
gds__log (buffer);
}


static bool_t mbx_putbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    int		count)
{
/**************************************
 *
 *	m b x _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Put a bunch of bytes to a memory stream if it fits.
 *
 **************************************/
SCHAR	*p;
SSHORT	n;
SLONG	bytecount = count;

/* Use memcpy to optimize bulk transfers. */

while (bytecount > sizeof (GDS_QUAD))
    {
    if (xdrs->x_handy >= bytecount)
	{
	memcpy (xdrs->x_private, buff, bytecount);
	xdrs->x_private += bytecount;
	xdrs->x_handy -= bytecount;
	return TRUE;
	}
    else
	{
	if (xdrs->x_handy > 0)
	    {
	    memcpy (xdrs->x_private, buff, xdrs->x_handy);
	    xdrs->x_private += xdrs->x_handy;
	    buff += xdrs->x_handy;
	    bytecount -= xdrs->x_handy;
	    xdrs->x_handy = 0;
	    }
	if (!mbx_write (xdrs, FALSE))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

while (bytecount)
    {
    n = MIN (bytecount, xdrs->x_handy);
    p = xdrs->x_private;
    xdrs->x_handy -= n;
    bytecount -= n;
    if (n)
	do *p++ = *buff++; while (--n);
    xdrs->x_private = p;
    if (!bytecount)
	break;
    if (!mbx_write (xdrs, FALSE))
	return FALSE;
    }

return TRUE;
}

static bool_t mbx_putlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	m b x _ p u t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
SLONG	l;

l = htonl (*lp);
return (*xdrs->x_ops->x_putbytes) (xdrs, &l, 4);
}

static bool_t mbx_read (
    XDR		*xdrs)
{
/**************************************
 *
 *	m b x _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a buffer full of data.
 *
 **************************************/
PORT		port;
SLONG		length;
USHORT		port_flags;
status_$t	status;
mbx_$server_msg_t	*message;

port = (PORT) xdrs->x_public;
port_flags = port->port_flags;
length = mbx_$msg_max;
message = (mbx_$server_msg_t*) port->port_buffer;
xdrs->x_private = xdrs->x_base = message->data;
THREAD_EXIT;

/* Handle client stuff first */

if (!port->port_server_flags)
    {
    mbx_$get_rec (port->port_handle, xdrs->x_base, length, 
	SR10_REF (xdrs->x_base), SR10_REF (length), SR10_REF (status));
    THREAD_ENTER;
    if (!status.all || status.all == mbx_$partial_record)
	{
	if (length < 0)
	    length = mbx_$msg_max;
	xdrs->x_handy = length;
	return TRUE;
	}
    if (port_flags & PORT_async)
	return FALSE;
    if (status.all == mbx_$eof)
	{
	port->port_state = state_eof;
	return FALSE;
	}
    port->port_state = state_broken;
    return mbx_error (port, "mbx_$get_rec", status);
    }

/* Handle server reads */

(UCHAR*) message = (UCHAR*) xdrs->x_base - sizeof (mbx_$msg_hdr_t);
mbx_$get_rec_chan (port->port_handle, (SSHORT) port->port_channel, message, length,
    SR10_REF (message), SR10_REF (length), SR10_REF (status));
THREAD_ENTER;

if (status.all)
    return mbx_error (port, "mbx_$rec_chan", status);

if (length < 0)
    length = mbx_$msg_max;

xdrs->x_handy = length - sizeof (mbx_$msg_hdr_t);

if (message->mt == mbx_$data_mt || message->mt == mbx_$data_partial_mt)
    return TRUE;

if (message->mt == mbx_$eof_mt)
    {
    port->port_state = state_eof;
    return FALSE;
    }

ib_fprintf (ib_stderr, "unexpected message type %d from mbx_$get_rec_chan\n", message->mt);
port->port_state = state_broken;

return FALSE;
}

static bool_t mbx_setpostn (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	m b x _ s e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Set the current position (which is also current length) from stream.
 *
 **************************************/

if (bytecount > xdrs->x_handy)
    return FALSE;

xdrs->x_private = xdrs->x_base + bytecount;

return TRUE;
}

static bool_t mbx_write (
    XDR		*xdrs,
    bool_t	end_flag)
{
/**************************************
 *
 *	m b x _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Write a buffer fulll of data.  If the end_flag isn't set, indicate
 *	that the buffer is a fragment, and reset the XDR for another buffer
 *	load.
 *
 **************************************/
PORT		port;
SLONG		length;
TEXT		*operation;
status_$t	status;
mbx_$server_msg_t	*message;

port = (PORT) xdrs->x_public;
operation = "mbx_$put_rec";
THREAD_EXIT;

/* Handle client stuff first */

if (!port->port_server_flags)
    {
    length = xdrs->x_private - xdrs->x_base;
    if (end_flag)
	mbx_$put_rec (port->port_handle, xdrs->x_base, length, SR10_REF (status));
    else
	{
	mbx_$put_chr (port->port_handle, xdrs->x_base, length, SR10_REF (status));
	operation = "mbx_$put_chr";
	}
    }
else
    {
    message = (mbx_$server_msg_t*) ((SCHAR*) xdrs->x_base - sizeof (mbx_$msg_hdr_t));
    length = xdrs->x_private - (SCHAR*) message;
    if (end_flag)
	message->mt = mbx_$data_mt;
    else
	message->mt = mbx_$data_partial_mt;
    message->cnt = length;
    message->chan = port->port_channel;
    if (port->port_state == state_eof ||
	port->port_state == state_broken)
	{
	THREAD_ENTER;
	return FALSE;
	}
    mbx_$put_rec (port->port_handle, message, length, SR10_REF (status));
    }

THREAD_ENTER;

if (status.all)
    return mbx_error (port, operation, status);

xdrs->x_private = xdrs->x_base;
xdrs->x_handy = (port->port_server_flags) ? mbx_$msg_max : mbx_$msg_max - sizeof (mbx_$msg_hdr_t);

return TRUE;
}

static void move (
    UCHAR	*from,
    UCHAR	*to,
    USHORT	length)
{
/**************************************
 *
 *	m o v e
 *
 **************************************
 *
 * Functional description
 *	Move a bunch of bytes.
 *
 **************************************/

if (length)
    do *to++ = *from++; while (--length);
}
