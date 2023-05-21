/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		remote.h
 *	DESCRIPTION:	Common descriptions
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

#ifndef _REMOTE_REMOTE_H_
#define _REMOTE_REMOTE_H_

#include "../jrd/common.h"
#include "../remote/remote_def.h"
#include "../jrd/ibsetjmp.h"

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

/* Include some apollo include files for tasking */

#if defined(apollo)
#include <apollo/base.h>
#include <apollo/task.h>
#include <apollo/ec2.h>
#endif

#if !(defined VMS || defined WIN_NT)
#if !(defined PC_PLATFORM || defined OS2_ONLY || defined NETWARE_386)
#include <pwd.h>
#include <signal.h>
#endif
#ifndef WINDOWS_ONLY
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#endif /* WINDOWS_ONLY */
#endif /* !VMS || !WIN_NT */

#ifdef DEV_BUILD
/* Debug packet/XDR memory allocation */

/* Temporarily disabling DEBUG_XDR_MEMORY */
/* #define DEBUG_XDR_MEMORY	*/

#endif

#define BLOB_LENGTH		16384   

#define ALLOC(type)		ALLR_block (type, 0)
#define ALLOCV(type, count)	ALLR_block (type, count)

typedef struct bid {
    ULONG	bid_relation_id;	/* Relation id (or null) */
    ULONG	bid_number;		/* Record number */
} *BID;

#include "../remote/protocol.h"

/* Block types */

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;


/* Block types */

typedef struct rdb {
    struct blk	rdb_header;
    USHORT	rdb_id;
    USHORT	rdb_flags;
    int		*rdb_handle;		/* database handle */
    struct port	*rdb_port;		/* communication port */
    struct rtr	*rdb_transactions;	/* linked list of transactions */
    struct rrq	*rdb_requests;		/* compiled requests */
    struct rvnt	*rdb_events;		/* known events */
    struct rsr	*rdb_sql_requests;	/* SQL requests */
    STATUS	*rdb_status_vector;
    UCHAR	*rdb_setjmp;
    PACKET	rdb_packet;		/* Communication structure */
} *RDB;

#define RDB_service	1		/* structure relates to a service */

typedef struct rtr {
    struct blk	rtr_header;
    struct rdb	*rtr_rdb;
    struct rtr	*rtr_next;
    struct rbl	*rtr_blobs;
    int		*rtr_handle;
    USHORT	rtr_flags;
    USHORT	rtr_id;
} *RTR;

#define RTR_limbo	1

typedef struct rbl {
    struct blk	rbl_header;
    struct rdb	*rbl_rdb;
    struct rtr	*rbl_rtr;
    struct rbl	*rbl_next;
    int		*rbl_handle;
    SLONG	rbl_offset;		/* Apparent (to user) offset in blob */
    USHORT	rbl_id;
    USHORT	rbl_flags;
    UCHAR	*rbl_ptr;
    UCHAR	*rbl_buffer;
    USHORT	rbl_buffer_length;
    USHORT	rbl_length;
    USHORT	rbl_fragment_length;
    USHORT	rbl_source_interp;	/* source interp (for writing) */
    USHORT	rbl_target_interp;	/* destination interp (for reading) */
    UCHAR	rbl_data [1];
} *RBL;

#define RBL_eof		1
#define RBL_segment	2
#define RBL_eof_pending	4
#define RBL_create	8

typedef struct rvnt {
    struct blk	rvnt_header;
    struct rvnt	*rvnt_next;
    RDB		rvnt_rdb;
    void	(*rvnt_ast)();
    void	*rvnt_arg;
    SLONG	rvnt_id;
    SLONG	rvnt_rid;	/* used by server to store client-side id */
    struct port *rvnt_port;	/* used to id server from whence async came */
    UCHAR	*rvnt_items;
    SSHORT	rvnt_length;
} *RVNT;

typedef struct vec {
    struct blk	vec_header;
    ULONG	vec_count;
    struct blk	*vec_object[1];
} *VEC;

typedef struct vcl {
    struct blk	vcl_header;
    ULONG	vcl_count;
    SLONG	vcl_long[1];
} *VCL;

/* Random string block -- jack of all kludges */

typedef struct str {
    struct blk	str_header;
    USHORT	str_length;
    SCHAR	str_data[2];
} *STR;

/* Include definition of descriptor */

#include "../jrd/dsc.h"

typedef struct vary {
    USHORT	vary_length;
    UCHAR	vary_string[1];
} *VARY;

typedef struct fmt {
    struct blk	fmt_header;
    USHORT	fmt_length;
    USHORT	fmt_net_length;
    USHORT	fmt_count;
    USHORT	fmt_version;
    USHORT	fmt_flags;
    struct dsc	fmt_desc[1];
} *FMT;

#define FMT_has_P10_specific_datatypes	0x1	/* datatypes don't exist in P9 */

/* Windows declares a msg structure, so rename the structure 
   to avoid overlap problems. */

typedef struct message {
    struct blk	msg_header;
    struct message *msg_next;		/* Next available message */
#ifdef SCROLLABLE_CURSORS
    struct message *msg_prior;		/* Next available message */
    ULONG	msg_absolute; 		/* Absolute record number in cursor result set */
#endif
    /* Please DO NOT re-arrange the order of following two fields.
       This could result in alignment problems while trying to access
       'msg_buffer' as a 'long', leading to "core" drops 
        Sriram - 04-Jun-97 */
    USHORT	msg_number;		/* Message number */
    UCHAR	*msg_address;		/* Address of message */
    UCHAR	msg_buffer[1];		/* Allocated message */
} *MSG;

/* remote stored procedure request */

typedef struct rpr {
    struct blk	rpr_header;
    struct rdb	*rpr_rdb;
    struct rtr	*rpr_rtr;
    int		*rpr_handle;
    struct message *rpr_in_msg;		/* input message */
    struct message *rpr_out_msg;		/* output message */
    struct fmt	*rpr_in_format;		/* Format of input message */
    struct fmt	*rpr_out_format;	/* Format of output message */
    USHORT	rpr_flags;
} *RPR;

#define RPR_eof		1		/* End-of-stream encountered */

typedef struct rrq {
    struct blk	rrq_header;
    struct rdb	*rrq_rdb;
    struct rtr	*rrq_rtr;
    struct rrq	*rrq_next;
    struct rrq	*rrq_levels;		/* RRQ block for next level */
    int		*rrq_handle;
    USHORT	rrq_id;
    USHORT	rrq_max_msg;
    USHORT	rrq_level;
    STATUS	rrq_status_vector[20];
    struct	rrq_repeat
    {
	struct fmt	*rrq_format;	/* format for this message */
	struct message	*rrq_message; 	/* beginning or end of cache, depending on whether it is client or server */
	struct message	*rrq_xdr;	/* point at which cache is read or written by xdr */ 
#ifdef SCROLLABLE_CURSORS
	struct message	*rrq_last;	/* last message returned */
	ULONG		rrq_absolute;	/* current offset in result set for record being read into cache */
    	USHORT		rrq_flags;
#endif
        USHORT	        rrq_msgs_waiting; /* count of full rrq_messages */
        USHORT	        rrq_rows_pending; /* How many rows in waiting */
	USHORT		rrq_reorder_level; /* Reorder when rows_pending < this level */
	USHORT		rrq_batch_count;   /* Count of batches in pipeline */

    }		rrq_rpt[1];
} *RRQ;

/* rrq flags */

#define RRQ_backward		1       /* the cache was created in the backward direction */ 
#define RRQ_absolute_backward	2	/* rrq_absolute is measured from the end of the stream */
#define RRQ_last_backward	4	/* last time, the next level up asked for us to scroll in the backward direction */

/* remote SQL request */

typedef struct rsr {
    struct blk	rsr_header;
    struct rsr  *rsr_next;
    struct rdb	*rsr_rdb;
    struct rtr	*rsr_rtr;
    int		*rsr_handle;
    struct fmt	*rsr_bind_format;	/* Format of bind message */
    struct fmt	*rsr_select_format;	/* Format of select message */
    struct fmt	*rsr_user_select_format; /* Format of user's select message */
    struct fmt	*rsr_format;		/* Format of current message */
    struct message *rsr_message;		/* Next message to process */
    struct message *rsr_buffer;		/* Next buffer to use */
    STATUS	rsr_status_vector [20]; /* saved status for buffered errors */
    USHORT	rsr_id;
    USHORT	rsr_flags;
    USHORT	rsr_fmt_length;

    ULONG	rsr_rows_pending;	/* How many rows are pending */
    USHORT	rsr_msgs_waiting; 	/* count of full rsr_messages */
    USHORT	rsr_reorder_level; 	/* Trigger pipelining at this level */
    USHORT	rsr_batch_count; 	/* Count of batches in pipeline */
} *RSR;

#define RSR_fetched	1		/* Cleared by execute, set by fetch */
#define RSR_eof		2		/* End-of-stream encountered */
#define RSR_blob	4		/* Statement relates to blob op */
#define RSR_no_batch	8		/* Do not batch fetch rows */
#define RSR_stream_err	16		/* There is an error pending in the batched rows */




#define BLKDEF(type, root, tail) type,
enum blk_t
    {
    type_MIN = 0,
#include "../remote/blk.h"
    type_MAX
    };
#undef BLKDEF

#include "../remote/xdr.h"

#define ACCEPT(port, cnct)	(*port->port_accept)(port, cnct)
#define DISCONNECT(port)	(*port->port_disconnect)(port)
#define RECEIVE(port, pckt)	(*port->port_receive_packet)(port, pckt)
#define SEND(port, pckt)	(*port->port_send_packet)(port, pckt)
#define SEND_PARTIAL(port, pckt) (*port->port_send_partial)(port, pckt)
#define CONNECT(port, pckt,ast)	(*port->port_connect)(port, pckt, ast)
#define REQUEST(port, pckt)	(*port->port_request)(port, pckt)

/* Generalized port definition. */

enum port_t {
    port_mailbox,		/* Apollo mailbox */
    port_pcic,			/* IBM PC interconnect */
    port_inet,			/* Internet (TCP/IP) */
    port_asyn_homebrew,		/* homebrew asynchronous connection */
    port_decnet,		/* DECnet connection */
    port_ipc,			/* NetIPC connection */
    port_pipe,			/* Windows NT named pipe connection */
    port_mslan,			/* Microsoft LanManager connection */
    port_spx,                   /* Novell SPX connection */
    port_ipserver,               /* InterBase interprocess server */
    port_xnet			/* Windows NT named xnet connection */
};

enum state_t {
    state_closed,		/* no connection */
    state_pending,		/* connection is pending */
    state_eof,			/* other side has shut down */
    state_broken,		/* connection is broken */
    state_active,		/* connection is complete */
    state_disconnected          /* port is disconnected */
};

#ifdef WINDOWS_ONLY
#define NOMSG
#include <windows.h>

#ifdef SPX
#define HANDLE SLONG
#endif /* SPX */
#else  /* !WINDOWS_ONLY */

#ifndef WIN_NT
#ifdef APOLLO
#define HANDLE	int*
#else
#define HANDLE	int
#endif  /* APOLLO */
#endif  /* WIN_NT */

#endif /* WINDOWS_ONLY */

typedef struct port {
    struct blk	port_header;
    enum port_t	port_type;		/* type of port */
    enum state_t port_state;		/* state of port */
    P_ARCH	port_client_arch;	/* so we can tell arch of client */
    struct port	*port_clients;		/* client ports */
    struct port	*port_next;		/* next client port */
    struct port	*port_parent;		/* parent port (for client ports) */
    struct port	*port_async;		/* asynchronous sibling port */
    struct srvr	*port_server;		/* server of port */
    USHORT	port_server_flags;	/* TRUE if server */
    USHORT	port_protocol;		/* protocol version number */
    USHORT	port_buff_size;		/* port buffer size (approx) */
    USHORT	port_flags;		/* Misc flags */
    SLONG       port_connect_timeout;   /* Connection timeout value */
    SLONG       port_dummy_packet_interval; /* keep alive dummy packet interval */
    SLONG	port_dummy_timeout;	/* time remaining until keepalive packet */
    STATUS	*port_status_vector;
    HANDLE	port_handle;		/* handle for connection (from by OS) */
    int		port_channel;		/* handle for connection (from by OS) */
    int		port_misc1;
    SLONG	port_semaphore;
    struct linger	port_linger;		/* linger value as defined by SO_LINGER */
    int		(*port_accept)();
    void	(*port_disconnect)();
    struct port	*(*port_receive_packet)();
    XDR_INT	(*port_send_packet)();
    XDR_INT	(*port_send_partial)();
    struct port	*(*port_connect)();	/* Establish secondary connection */
    struct port	*(*port_request)();	/* Request to establish secondary connection */
    struct rdb	*port_context;
    XDR_INT	(*port_ast)();		/* AST for events */
    XDR		port_receive;
    XDR		port_send;
#ifdef DEBUG_XDR_MEMORY
    VEC		port_packet_vector;	/* Vector of send/receive packets */
#endif
    VEC		port_object_vector;
    BLK		*port_objects;
    STR		port_version;
    STR		port_host;		/* Our name */
    STR		port_connection;	/* Name of connection */
    STR		port_user_name;
    STR		port_passwd;
    struct rpr	*port_rpr;		/* port stored procedure reference */
    struct rsr	*port_statement;	/* Statement for execute immediate */
    struct rmtque *port_receive_rmtque;	/* for client, responses waiting */
    USHORT	port_requests_queued;	/* requests currently queued */
#ifdef WINDOWS_ONLY
    HWND	port_msg_handle;	/* Handle to window for async msgs */
#endif
#ifdef VMS
    USHORT	port_iosb[4];
#endif
#ifdef mpexl
    ULONG	port_mpexl_align;
#endif
#ifdef NETWARE_386
    void        *port_fdset;
#endif
#ifdef XNET
    void        *port_xcc;              /* interprocess structure */
#endif
    UCHAR	port_buffer[1];
} *PORT;

#define PORT_symmetric		1	/* Server/client archiectures are symmetic */
#define PORT_rpc		2	/* Protocol is remote procedure call */
#define PORT_pend_ack		4	/* An ACK is pending on the port */
#define PORT_broken		8	/* Connect is broken */
#define PORT_async		16	/* Port is asynchronous channel for events */
#define PORT_no_oob		32	/* Don't send out of band data */
#define PORT_disconnect		64	/* Disconnect is in progress */
#define PORT_pend_rec		128	/* A record is pending on the port */
#define PORT_not_trusted	256	/* Connection is from an untrusted node */
#define PORT_impersonate	512	/* A remote user is being impersonated */
#define PORT_dummy_pckt_set	1024	/* A dummy packet interval is set  */


/* Misc declarations */

#include "../remote/allr_proto.h"
#include "../jrd/thd.h"

/* Thread specific remote database block */

typedef struct trdb {
    struct thdd	trdb_thd_data;
    struct rdb	*trdb_database;
    STATUS	*trdb_status_vector;
    JMP_BUF	*trdb_setjmp;
} *TRDB;

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#define GET_THREAD_DATA		((TRDB) THD_get_specific())



/* Queuing structure for Client batch fetches */

typedef struct rmtque {
    struct blk		rmtque_header;	/* Memory allocator header */
    struct rmtque	*rmtque_next;	/* Next entry in queue */
    void 		*rmtque_parm;/* What request has response in queue */
    struct rrq_repeat	*rmtque_message;/* What message is pending */
    struct rdb		*rmtque_rdb;	/* What database has pending msg */
    BOOLEAN		(*rmtque_function) /* Fn that receives queued entry */
				(struct trdb *, struct port *, 
					struct rmtque *, STATUS *, USHORT);
} *RMTQUE;

#endif /* _REMOTE_REMOTE_H_ */

