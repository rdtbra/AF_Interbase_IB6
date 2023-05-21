/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		mslan.c
 *	DESCRIPTION:	Microsoft LanManager communications module.
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
 
/* we can't #define INCL_BASE, because with nmpipe.h 2 function prototypes of several
   named pipe functions will be brought in, and errors will be generated.  So we have
   to define all the constants except one of the ones that would have been defined by
   INCL_BASE.
*/

#define INCL_SUB
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSDEVICES
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSSIGNALS
#define INCL_DOSMONITORS
#define INCL_DOSPROFILE
#define INCL_DOSPROCESS
#define INCL_DOSINFOSEG
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSMODULEMGR
#define INCL_DOSRESOURCES
#define INCL_DOSTRACE
#define INCL_DOSMEMMGR
#include <os2.h>
#include "../jrd/ib_stdio.h"

#include <netcons.h>
#include <nmpipe.h>
#include <server.h>
#include <shares.h>
#include <neterr.h>

#include "../jrd/gds.h"
#include "../jrd/thd.h"
#include "../remote/remote.h"
#include "../remote/lanentry.h"
#include "../remote/mslan_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_f_proto.h"

#define PTR		SLONG
#define MAXPATHLEN	1024
#define MAX_DATA	2048
#define BUFFER_SIZE	MAX_DATA
#define MAX_SEQUENCE	256

#define PROXY_FILE  		"c:/interbas/gdsproxy"
#define GDS_SERVER_PIPE	        "\\PIPE\\LSERVER"
#define GDS_EVENT_PIPE          "\\PIPE\\EVENT"
#define GDS_SERVER_SEMAPHORE	"\\SEM\\LSERVER"
#define GDS_EVENT_SEMAPHORE	"\\SEM\\EVENT"

#define PIPEOUTSIZE MAX_DATA  /* hint at outgoing buffer size */
#define PIPEINSIZE  MAX_DATA  /* hint at outgoing buffer size */

/*
#define DEBUG	1
#ifdef DEBUG
static	MSLAN_trace = 1;
#endif
*/

extern SLONG 	htonl(), ntohl();

static int	accept_connection (PORT, P_CNCT *);
static PORT	alloc_port (PORT);
static int	analyze_filename (TEXT *, TEXT *);
static PORT	aux_connect (PORT, PACKET *, VOID (*)());
static STR	aux_name (STR *);
static PORT	aux_request (PORT, PACKET *);
static int	check_host (PORT, TEXT *);
static int	check_proxy (TEXT *, TEXT *);
static void     cleanup_port (PORT);
static void	disconnect (PORT);
static void	gethostname (SCHAR *, USHORT);
static int	make_pipe (PORT, TEXT *);
static VOID	mslan_destroy (XDR *);
static int	mslan_error (PORT, TEXT *, int);
static bool_t	mslan_getbytes (XDR *, SCHAR *, int);
static bool_t	mslan_getlong (XDR *, SLONG *);
static u_int	mslan_getpostn (XDR *);
static caddr_t  mslan_inline (XDR *, u_int);
static int	mslan_netapifunc (void);
static bool_t	mslan_putbytes (XDR *, SCHAR *, int);
static bool_t	mslan_putlong (XDR *, SLONG *);
static bool_t	mslan_read (XDR *);
static bool_t	mslan_setpostn (XDR *, u_int);
static bool_t	mslan_write (XDR *, bool_t);
static void	packet_print (UCHAR *, UCHAR *, int);
static int	packet_receive (PORT, UCHAR *, SSHORT, SSHORT *);
static int	packet_send (PORT, UCHAR *, SSHORT);
static PORT	receive (PORT, PACKET *);
static int	send_full (PORT, PACKET *);
static int	send_partial (PORT, PACKET *);
static int	xdrmslan_create (XDR *, PORT, UCHAR *, USHORT, enum xdr_op);
static int	xdrmslan_endofrecord (XDR *, bool_t);

static struct xdr_ops	mslan_ops =
	{
	mslan_getlong,
	mslan_putlong,
	mslan_getbytes,
	mslan_putbytes,
	mslan_getpostn,
	mslan_setpostn,
	mslan_inline,
	mslan_destroy};

typedef struct mnt {
    UCHAR	*mnt_node;
    UCHAR	*mnt_mount;
    UCHAR	*mnt_path;
} MNT;

PORT MSLAN_analyze (
    UCHAR	*file_name,
    SSHORT	*file_length,
    STATUS	*status_vector,
    SSHORT	uv_flag)
{
/**************************************
 *
 *	M S L A N _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Determine whether the file name has a "\\nodename\sharename".  If so,
 *	establish an external connection to the node.
 *
 *	If a connection is established, return a port block, otherwise
 *	return NULL.
 *
 **************************************/
RDB		rdb;
PORT		port;
PACKET		*packet;
P_CNCT		*connect;
P_ATCH		*attach;
SSHORT		length, user_length;
TEXT		node_name [MAXPATHLEN], *p, *q, user_id [128];
STATUS		local_status [20];
struct p_cnct_repeat	*protocol;

/* Analyze the file name to see if a remote connection is required.  If not,
   quietly (sic) return. */

if (!analyze_filename (file_name, node_name))
    return NULL;

length = *file_length = strlen (file_name);

/* We need to establish a connection to a remote server.  Allocate the necessary
   blocks and get ready to go. */

rdb = (RDB) ALLOC (type_rdb);
rdb->rdb_status_vector = status_vector;
packet = &rdb->rdb_packet;

/* Pick up some user identification information */

strcpy (user_id, "USER");

user_length = strlen("USER");

/* Establish connection to server */

connect = &packet->p_cnct;
packet->p_operation = op_connect;
connect->p_cnct_operation = op_attach;
connect->p_cnct_cversion = CONNECT_VERSION2;
connect->p_cnct_client = ARCHITECTURE;
connect->p_cnct_file.cstr_length = *file_length;
connect->p_cnct_file.cstr_address  = file_name;
connect->p_cnct_count = 4;
connect->p_cnct_user_id.cstr_length = user_length;
connect->p_cnct_user_id.cstr_address = (UCHAR*) user_id;

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

/* If we can't talk to a server, punt.  Let somebody else generate
   an error. */

if (!(port = MSLAN_connect (node_name, packet, status_vector, FALSE)))
    {
    ALLR_release (rdb);
    return NULL;
    }

/* Get response packet from server. */

rdb->rdb_port = port;
port->port_context = rdb;
RECEIVE (port, packet);

if (packet->p_operation != op_accept)
   {
   *status_vector++ = gds_arg_gds;
   *status_vector++ = gds__connect_reject;
   *status_vector++ = 0;
   disconnect (port);
   return NULL;
   }

port->port_protocol = packet->p_acpt.p_acpt_version;

if (packet->p_acpt.p_acpt_architecture == ARCHITECTURE)
    port->port_flags |= PORT_symmetric;

if (packet->p_acpt.p_acpt_type == ptype_rpc)
    port->port_flags |= PORT_rpc;

return port;
}

PORT MSLAN_connect (
    TEXT	*name,
    PACKET	*packet,
    STATUS	*status_vector,
    int		flag)
{
/**************************************
 *
 *	M S L A N _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Establish half of a communication link.  If a connect packet is given,
 *	the connection is on behalf of a remote interface.  Other the connect
 *	is is for a server process.
 *
 **************************************/
int	status, openaction;
PORT	port;
TEXT	*p, pipe_name [64];

status_vector [0] = gds_arg_gds;
status_vector [1] = 0;

/* Try to get the entry points for the MS LAN dll. If cannot, return */

if (!mslan_netapifunc ())
   {
   status_vector [1] = gds__unavailable;
   status_vector [2] = 0;
   return NULL;
   }

port = alloc_port (NULL);
port->port_status_vector = status_vector;

/* Construct the port connection, depending on
   whether this pipe is local or remote */

if (name)
    {
    for (p = pipe_name; *name;)
	    *p++ = *name++;
    strcpy (p, GDS_SERVER_PIPE);
    port->port_connection = REMOTE_make_string (pipe_name);
    }
else
    port->port_connection = REMOTE_make_string (GDS_SERVER_PIPE);

/* If we're a host, just make the connection */

if (packet)
    {
    while (status = DosOpen (port->port_connection->str_data, 
	    &port->port_handle,
	    &openaction, 
	    0, 			/* normal file */
	    0x0000,			/* attributes */
	    0x0001,			/* just open the file, please */
	    0x4012,			/* no share, read/write */
	    0L))
	if (status == ERROR_PIPE_BUSY)
	    CALL (LAN_WAITNMPIPE) (port->port_connection->str_data, 0L);
	else
	    {
	    mslan_error (port, "DosOpen", status);
	    return NULL;
	    }
    if (status = CALL (LAN_SETNMPHANDSTATE) (port->port_handle, NP_RMESG))
	{
	mslan_error (port, "DosSetNmpHandState", status);
	return NULL;
	}
    send_full (port, packet);
    return port;
    }

/* We're a server, so wait for a host to show up */

port->port_server_flags = TRUE;

if (make_pipe (port, GDS_SERVER_SEMAPHORE))
    {
    disconnect (port);
    return NULL;
    }

return port;
}

static int accept_connection (
    PORT	port,
    P_CNCT	*connect)
{
/**************************************
 *
 *	a c c e p t _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Accept an incoming request for connection.  This is purely a lower
 *	level handshaking function, and does not constitute the server
 *	response for protocol selection.
 *
 **************************************/
UCHAR		name [32], password [32], host [32], *id, *end, *p, *q;
SLONG		length;

return TRUE;         /* Temporary, until account is incorporated */

/* Default account to "USER" */

strcpy (name, "USER");
password [0] = 0;

/* Pick up account and password, if given */

id = connect->p_cnct_user_id.cstr_address;
end = id + connect->p_cnct_user_id.cstr_length;

while (id < end)
    switch (*id++)
	{
	case CNCT_user:
	    p = name;
	    if (length = *id++)
		do *p++ = *id++; while (--length);
	    *p = 0;
	    break;

	case CNCT_passwd:
	    p = password;
	    if (length = *id++)
		do *p++ = *id++; while (--length);
	    *p = 0;
	    break;
	
	default:
	    p += *p + 1;
	}

/* See if user exists.  If not, reject connection */

if (!check_host (port, host))
    return FALSE;

if (!check_proxy (host, name))
   return FALSE;

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
UCHAR	*message;
PORT	port;
UCHAR	buffer [64];

port = (PORT) ALLOCV (type_port, BUFFER_SIZE * 2);
port->port_type = port_mslan;
port->port_state = state_pending;

gethostname (&buffer, sizeof (buffer));
port->port_host = REMOTE_make_string (buffer);
port->port_connection = REMOTE_make_string (buffer);
sprintf (buffer, "MSLan (%s)", port->port_host->str_data);
port->port_version = REMOTE_make_string (buffer);

if (parent)
    {
    port->port_parent = parent;
    port->port_next = parent->port_clients;
    parent->port_clients = port;
    port->port_handle = parent->port_handle;
    port->port_server = parent->port_server;
    port->port_server_flags = parent->port_server_flags;
    if (port->port_connection)
        ALLR_free (port->port_connection);
    port->port_connection = REMOTE_make_string (parent->port_connection->str_data);
    }

message = port->port_buffer;
port->port_accept = accept_connection;
port->port_disconnect = disconnect;
port->port_receive_packet = receive;
port->port_send_packet = send_full;
port->port_send_partial = send_partial;
port->port_connect = aux_connect;
port->port_request = aux_request;
port->port_buff_size = BUFFER_SIZE;

xdrmslan_create (&port->port_send, port, 
	&port->port_buffer [BUFFER_SIZE], BUFFER_SIZE,
	XDR_ENCODE);

xdrmslan_create (&port->port_receive, port, port->port_buffer, 0,
	XDR_DECODE);

return port;
}

static int analyze_filename (
    TEXT	*file_name,
    TEXT	*node_name)
{
/**************************************
 *
 *	a n a l y z e _ f i l e n a m e
 *
 **************************************
 *
 * Functional description
 *	Analyze a filename for a TCP node name on the front.  If
 *	one is found, extract the node name, compute the residual
 *	file name, and return TRUE.  Otherwise return FALSE.
 *
 **************************************/
TEXT	*p, *out, device [16], path [256], expanded_name [256];
USHORT	n;

ISC_expand_filename (file_name, 0, expanded_name);
ISC_parse_filename (expanded_name, node_name, device, path);
out = file_name;

if (node_name [0])
    {
    for (p = device; *p;)
	*out++ = *p++;
    for (p = path; *out++ = *p++;)
    	;
    return TRUE;
    }

return FALSE;
}         

static PORT aux_connect (
    PORT	port,
    PACKET	*packet,
    VOID	(*ast)())
{
/**************************************
 *
 *	a u x _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Try to establish an alternative connection.  Somebody has already
 *	done a successful connect request ("packet" contains the response).
 *
 **************************************/
int	openaction, status;
PORT	new_port;

/* If this is a server, we've got an auxiliary connection. */

if (port->port_server_flags)
    {
    port->port_flags |= PORT_async;
    return port;
    }

port->port_async = new_port = alloc_port (port->port_parent);
new_port->port_flags |= PORT_async;
new_port->port_status_vector = port->port_status_vector;    
new_port->port_connection = aux_name (port->port_connection);
                  
while (status = DosOpen (new_port->port_connection->str_data, 
    &new_port->port_handle,
    &openaction, 
    0, 			/* normal file */
    0x0000,			/* attributes */
    0x0001,			/* just open the file, please */
    0x4012,			/* no share, read/write */
    0L))
    if (status == ERROR_PIPE_BUSY)
        CALL (LAN_WAITNMPIPE) (new_port->port_connection->str_data, 0L);
	else
	    {
	    mslan_error (new_port, "DosOpen", status);
	    return NULL;
	    }

if (status = CALL (LAN_SETNMPHANDSTATE) (new_port->port_handle, NP_RMESG))
    {
    mslan_error (new_port, "DosSetNmpHandState", status);
    return NULL;
    }

new_port->port_flags = port->port_flags & PORT_no_oob;

return new_port;
}

static STR *aux_name (
    STR *connection)
{
/**************************************
 *
 *	a u x _ n a m e
 *
 **************************************
 *
 * Functional description
 *     Construct a name for the auxiliary pipe.
 *     Figure out whether we need a remote node name,
 *     and construct the pipe name accordingly.
 *
 **************************************/
TEXT    pipe_name[64], *p, *q;

p = connection->str_data;
q = pipe_name;

if ((*p++ == '\\') && (*p++ == '\\'))
    {
    *q++ = '\\';
    *q++ = '\\';
    while (*p != '\\')
        *q++ = *p++;
    }

strcpy (q, GDS_EVENT_PIPE);

return REMOTE_make_string (pipe_name);
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
PORT		new_port;

port->port_async = new_port = alloc_port (port->port_parent);
new_port->port_server_flags = port->port_server_flags;
new_port->port_flags = port->port_flags & PORT_no_oob;
new_port->port_connection = aux_name (port->port_connection);
new_port->port_status_vector = port->port_status_vector;

if (make_pipe (new_port, GDS_EVENT_SEMAPHORE))
    {
    disconnect (port);
    return NULL;
    }

return new_port;
}

static check_host (
    PORT	port,
    TEXT	*host_name)
{
/**************************************
 *
 *	c h e c k _ h o s t
 *
 **************************************
 *
 * Functional description
 *	Check the host on the other end of the socket to see it
 *	it's an equivalent host.
 *
 **************************************/

return TRUE;
}

static int check_proxy (
    TEXT	*host_name,
    TEXT	*user_name)
{
/**************************************
 *
 *	c h e c k _ p r o x y
 *
 **************************************
 *
 * Functional description
 *	Lookup <host_name, user_name> in proxy file.  If found,
 *	change user_name.
 *
 **************************************/
IB_FILE	*proxy;
TEXT	*p, source_user[32], source_host[32], target_user[32], line [128];
int	result, c;

if (!(proxy = ib_fopen (PROXY_FILE, "r")))
    return FALSE;

/* Read lines, scan, and compare */

result = FALSE;

for (;;)
    {
    for (p = line; (c = ib_getc (proxy)) && c != EOF && c != '\n';)
	*p++ = c;
    *p = 0;
    if (sscanf (line, "%[^:]:%s%s", source_host, source_user, target_user) >= 3)
	if ((!strcmp (source_host, host_name) ||
	     !strcmp (source_host, "*")) &&
	    (!strcmp (source_user, user_name) ||
	     !strcmp (source_user, "*")))
	    {
	    strcpy (user_name, target_user);
	    result = TRUE;
	    break;
	    }
    if (c == EOF)
	break;
    }
	
ib_fclose (proxy);

return result;
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
int	status;
PORT	*ptr;

/* If this is a sub-port, unlink it from it's parent */

if (port->port_parent)
    {
    if (port->port_async)
    	{
    	disconnect (port->port_async);
    	port->port_async = NULL;
    	}
    for (ptr = &port->port_parent->port_clients; *ptr; ptr = &(*ptr)->port_next)
	if (*ptr == port)
	    {
	    *ptr = port->port_next;
	    break;
	    }
    }
else
    if (port->port_async)
	{
#ifdef MULTI_THREAD
	port->port_async->port_flags |= PORT_disconnect;
#else
	disconnect (port->port_async);
	port->port_async = NULL;
#endif
	}

status = DosClose (port->port_handle);
cleanup_port (port);
}
^L
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

static void gethostname (
   SCHAR	*buffer,
   USHORT	buflen)
{
/**************************************
 *
 *	g e t h o s t n a m e
 *
 **************************************
 *
 * Functional description
 *    Return the local computer name.
 *
 **************************************/
USHORT	totalavail;

CALL (LAN_SERVERGETINFO) (NULL, 0, buffer, buflen, &totalavail);
}

static int make_pipe (
   PORT 	port,
   TEXT		*semaphore_name)
{
/**************************************
 *
 *	m a k e _ p i p e
 *
 **************************************
 *
 * Functional description
 *    Create a named pipe for a server.  Return status.
 *
 **************************************/
int	status;

/* If no semaphore, name one */

if (!port->port_semaphore)
    if (status = DosCreateSem (CSEM_PUBLIC, &port->port_semaphore, semaphore_name))
	{
	mslan_error (port, "DosCreateSem", status);
	return status;
	}

/* Create named pipe */

if (status = CALL (LAN_MAKENMPIPE)(port->port_connection->str_data, 
	&port->port_handle,
	0x4002,					/* don't inherit, full duplex */
	NP_NBLK | NP_WMESG | NP_RMESG | 0xFF,	/* non-block, message, lots connections */
	PIPEOUTSIZE, 
	PIPEINSIZE, 
	0L))
    {
    mslan_error (port, "DosMakeNmPipe", status);
    return status;
    }

if (status = CALL (LAN_SETNMPIPESEM) (port->port_handle, port->port_semaphore, 0))
    {
    mslan_error (port, "DosSetNmPipeSem", status);
    return status;
    }

return status;
}

static VOID mslan_destroy (
    XDR		*xdrs)
{
/**************************************
 *
 *	m s l a n _ d e s t r o y
 *
 **************************************
 *
 * Functional description
 *	Destroy a stream.  A no-op.
 *
 **************************************/

}

static int mslan_error (
    PORT	port,
    TEXT	*operation,
    int		status)
{
/**************************************
 *
 *	m s l a n _ e r r o r
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
STATUS	*status_vector;

port->port_flags |= PORT_broken;
port->port_state = state_broken;

if (rdb = port->port_context)
    status_vector = rdb->rdb_status_vector;
else
    status_vector = port->port_status_vector;

if (status_vector)
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__io_error;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) operation;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) port->port_connection->str_data;
    if (status)
	{
	*status_vector++ = gds_arg_dos;
	*status_vector++ = status;
	}
    *status_vector++ = 0;
    }

return 0;
}

static bool_t mslan_getbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    int		count)
{
/**************************************
 *
 *	m s l a n _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Get a bunch of bytes from a memory stream if it fits.
 *
 **************************************/
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
	if (!mslan_read (xdrs))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

if (!bytecount)
    return TRUE;

if (bytecount && xdrs->x_handy >= bytecount)
    {
    xdrs->x_handy -= bytecount;
    do *buff++ = *xdrs->x_private++; while (-- bytecount);
    return TRUE;
    }

while (--bytecount >= 0)
    {
    if (!xdrs->x_handy && !mslan_read (xdrs))
	return FALSE;
    *buff++ = *xdrs->x_private++;
    --xdrs->x_handy;
    }	

return TRUE;
}

static bool_t mslan_getlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	m s l a n _ g e t l o n g
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

static u_int mslan_getpostn (
    XDR		*xdrs)
{
/**************************************
 *
 *	m s l a n _ g e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Get the current position (which is also current length) from stream.
 *
 **************************************/

return (u_int) (xdrs->x_private - xdrs->x_base);
}

static caddr_t mslan_inline (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	m s l a n _  i n l i n e
 *
 **************************************
 *
 * Functional description
 *	Return a pointer to somewhere in the buffer.
 *
 **************************************/

if (bytecount > xdrs->x_handy)
    return FALSE;

return (caddr_t) (xdrs->x_base + bytecount);
}

static int mslan_netapifunc (void)
{
/**************************************
 *
 *	m s l a n _ n e t a p i f u n c
 *
 **************************************
 *
 * Functional description
 *   Find address of requested LAN Manager
 *    api function, loading dll if necessary.
 *	
 **************************************/
ENTRY	*entry, *end;

for (entry = entrypoints, end = entry + PROC_count; entry < end; entry++)
   if (!entry->address &&
       !(entry->address = ISC_lookup_entrypoint (entry->module, entry->name, NULL)))
	return FALSE;

return TRUE;
}

static bool_t mslan_putbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    int		count)
{
/**************************************
 *
 *	m s l a n _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Put a bunch of bytes to a memory stream if it fits.
 *
 **************************************/
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
	if (!mslan_write (xdrs))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

if (!bytecount)
    return TRUE;

if (bytecount && xdrs->x_handy >= bytecount)
    {
    xdrs->x_handy -= bytecount;
    do *xdrs->x_private++ = *buff++; while (-- bytecount);
    return TRUE;
    }

while (--bytecount >= 0)
    {
    if (xdrs->x_handy <= 0 && !mslan_write (xdrs))
	return FALSE;
    --xdrs->x_handy;
    *xdrs->x_private++ = *buff++;
    }

return TRUE;
}

static bool_t mslan_putlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	m s l a n _ p u t l o n g
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

static bool_t mslan_read (
    XDR		*xdrs)
{
/**************************************
 *
 *	m s l a n _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a buffer full of data.  If we receive a bad packet,
 *	send the moral equivalent of a NAK and retry.  ACK all
 *	partial packets.  Don't ACK the last packet -- the next
 *	message sent will handle this.
 *
 **************************************/
PORT	port;
SSHORT	length;
SCHAR	*p, *end;

port = (PORT) xdrs->x_public;
p = xdrs->x_base;
end = p + BUFFER_SIZE;

/* If buffer is not completely empty, slide down what what's left */

if (xdrs->x_handy > 0)
    {
    memmove (p, xdrs->x_private, xdrs->x_handy);
    p += xdrs->x_handy;
    }

/* If an ACK is pending, do an ACK.  The alternative is deadlock. */

/*
if (port->port_flags & PORT_pend_ack)
    if (!packet_send (port, NULL, 0))
	return FALSE;
*/

while (TRUE)
    {
    length = (SSHORT) (end - p);
    if (!packet_receive (port, p, length, &length))
	{
	return FALSE;
	/***
	if (!packet_send (port, NULL, 0))
	    return FALSE;
	continue;
	***/
	}
    if (length >= 0)
	{
	p += length;
	break;
	}
    p -= length;
    if (!packet_send (port, NULL, 0))
	return FALSE;
    }

port->port_flags |= PORT_pend_ack;
xdrs->x_handy = (SCHAR*) p - xdrs->x_base;
xdrs->x_private = xdrs->x_base;

return TRUE;
}

static bool_t mslan_setpostn (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	m s l a n _ s e t p o s t n
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

static bool_t mslan_write (
    XDR		*xdrs,
    bool_t	end_flag)
{
/**************************************
 *
 *	m s l a n _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Write a buffer fulll of data.  If the end_flag isn't set, indicate
 *	that the buffer is a fragment, and reset the XDR for another buffer
 *	load.
 *
 **************************************/
SCHAR	aux_buffer [BUFFER_SIZE], *p;
SCHAR	buffer [BUFFER_SIZE];
PORT	port;
SSHORT	l, l2;
SLONG length;

/* Encode the data portion of the packet */

port = (PORT) xdrs->x_public;
p = xdrs->x_base;
length = (SLONG) xdrs->x_private - (SLONG) p;

/* Send data in manageable hunks.  If a packet is partial, indicate
   that with a negative length.  A positive length marks the end. */

while (length)
    {
    port->port_misc1 = (port->port_misc1 + 1) % MAX_SEQUENCE;
    l = MIN (length, MAX_DATA);
    length -= l;
    if (!packet_send (port, p, (length) ? -l : l))
	 return FALSE;
    p += l;
    }

xdrs->x_private = xdrs->x_base;
xdrs->x_handy = BUFFER_SIZE;

return TRUE;

#ifdef PIGGYBACK
/* If the other end has not piggy-backed the next packet, we're done. */

if (!l2)
    return TRUE;

/* We've got a piggy-backed response.  If the packet is partial,
   send an ACK for part we did receive. */

p = aux_buffer;

while (l2 < 0)
    {
    if (!packet_send (port, NULL, 0))
	return FALSE;
    p -= l2;
    length = aux_buffer + sizeof (aux_buffer) - p;
    if (!packet_receive (port, p, length, &l2))
	{
	p += l2;
	continue;
	}
    }

length = p - aux_buffer + l2;

/* Now we're got a encode glump ready to stuff into the read buffer.
   Unfortunately, if we just add it to the read buffer, we will shortly
   overflow the buffer.  To avoid this, "scrumpf down" the active bits
   in the read buffer, then add out stuff at the end. */

xdrs = &port->port_receive;
p = xdrs->x_base;

if (xdrs->x_handy && p != xdrs->x_private)
    memmove (p, xdrs->x_private, xdrs->x_handy);

p += xdrs->x_handy;

xdrs->x_private = xdrs->x_base;

port->port_flags |= PORT_pend_ack;

return TRUE;
#endif
}

#ifdef DEBUG
static void packet_print (
    UCHAR	*string,
    UCHAR	*packet,
    int		length)
{
/**************************************
 *
 *	p a c k e t _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Print a summary of packet.
 *
 **************************************/
int	sum, l;

sum = 0;
if (l = length)
    do sum += *packet++; while (--l);

ib_printf ("%s\t: length = %d, checksum = %d\n", string, length, sum);
}
#endif

static int packet_receive (
    PORT	port,
    UCHAR	*buffer,
    SSHORT	buffer_length,
    SSHORT	*length)
{
/**************************************
 *
 *	p a c k e t _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a packet and pass on it's goodness.  If it's good,
 *	return TRUE and the reported length of the packet, and update
 *	the receive sequence number.  If it's bad, return FALSE.  If it's
 *	a duplicate message, just ignore it.
 *
 **************************************/
int	status;

THREAD_EXIT;
status = DosRead (port->port_handle, buffer, buffer_length, length);
THREAD_ENTER;
if (status)
    return mslan_error (port, "DosRead", status);

if (!*length)
    return mslan_error (port, "read end_of_file", 0);

#ifdef DEBUG
if (MSLAN_trace)
    packet_print ("receive", buffer, n);
#endif

return TRUE;
}

static int packet_send (
    PORT	port,
    UCHAR	*data,
    SSHORT	length)
{
/**************************************
 *
 *	p a c k e t _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Send some data on it's way.  
 *
 **************************************/
int	status, n;

TRHEAD_EXIT;
status = DosWrite (port->port_handle, data, length, &n);
THREAD_ENTER;
if (status)
    return mslan_error (port, "DosWrite", status);

if (n != length)
    return mslan_error (port, "DosWrite truncated", 0);

#ifdef DEBUG
if (MSLAN_trace)
    packet_print ("send", data, length);
#endif

port->port_flags &= ~PORT_pend_ack;

return TRUE;
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
PORT	port;
SLONG	value, *ptr;
int	status, n;

/* If this isn't a server, just do the operation and get it over with */

if (!main_port->port_server_flags)
    {
    if (!xdr_protocol (&main_port->port_receive, packet))
	return NULL;
    return main_port;
    }

/* Do a read on either the parent or any child ports */

ptr = &main_port->port_semaphore;

for (;;)
    {
    value = ISC_event_clear (main_port->port_semaphore);
    while (!(status = CALL (LAN_CONNECTNMPIPE) (main_port->port_handle)))
	{
	port = alloc_port (main_port);
	port->port_handle = main_port->port_handle;
	make_pipe (main_port, GDS_SERVER_SEMAPHORE);
	}
    for (port = main_port->port_clients; port; port = port->port_next)
	if (!(status = DosRead (port->port_handle, 
		port->port_buffer, 
		BUFFER_SIZE, 
		&n)))
	    break;
    if (port)
	break;
    THREAD_EXIT;
    ISC_event_wait (1, ptr, &value, -1, NULL_PTR, NULL_PTR);
    THREAD_ENTER;
    }

/* A read has completed for better or worse.  Find out which. */

if (!n)
    {
    packet->p_operation = op_exit;
    port->port_state = state_eof;
    return port;
    }

/* We've got data -- lap it up and use it */

xdrmslan_create (&port->port_receive, port, port->port_buffer, n, XDR_DECODE);

if (!xdr_protocol (&port->port_receive, packet))
    return NULL;

return port;
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

if (!xdr_protocol (&port->port_send, packet))
    return FALSE;

return xdrmslan_endofrecord (&port->port_send, TRUE);
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

return xdr_protocol (&port->port_send, packet);
}

static int xdrmslan_create (
    XDR			*xdrs,
    PORT		port,
    UCHAR		*buffer,
    USHORT		length,
    enum xdr_op		x_op)
{
/**************************************
 *
 *	x d r m s l a n _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Initialize an XDR stream for Apollo mailboxes.
 *
 **************************************/

xdrs->x_public = (caddr_t) port;
xdrs->x_base = xdrs->x_private = (SCHAR*) buffer;
xdrs->x_handy = length;
xdrs->x_ops = &mslan_ops;
xdrs->x_op = x_op;

return TRUE;
}

static int xdrmslan_endofrecord (
    XDR		*xdrs,
    bool_t	flushnow)
#endif
{
/**************************************
 *
 *	x d r m s l a n _ e n d o f r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Write out the rest of a record.
 *
 **************************************/

return mslan_write (xdrs, flushnow);
}
