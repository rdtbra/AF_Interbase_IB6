/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		protocol.h
 *	DESCRIPTION:	Protocol Definition
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

#ifndef _REMOTE_PROTOCOL_H_
#define _REMOTE_PROTOCOL_H_

/* The protocol is defined blocks, rather than messages, to
   separate the protocol from the transport layer.  */

#define CONNECT_VERSION2	2

/* Protocol 4 is protocol 3 plus server management functions */

#define PROTOCOL_VERSION3	3
#define PROTOCOL_VERSION4	4

/* Protocol 5 includes support for a d_float data type */

#define PROTOCOL_VERSION5	5

/* Protocol 6 includes support for cancel remote events, blob seek,
   and unknown message type */

#define PROTOCOL_VERSION6	6

/* Protocol 7 includes DSQL support */

#define PROTOCOL_VERSION7	7

/* Protocol 8 includes collapsing first receive into a send, drop database,
   DSQL execute 2, DSQL execute immediate 2, DSQL insert, services, and
   transact request */

#define PROTOCOL_VERSION8	8

/* Protocol 9 includes support for SPX32 
   SPX32 uses WINSOCK instead of Novell SDK
   In order to differentiate between the old implementation 
   of SPX and this one, different PROTOCOL VERSIONS are used */

#define PROTOCOL_VERSION9	9

/* Protocol 10 includes support for warnings and removes the requirement for
 * encoding and decoding status codes.*/

#define PROTOCOL_VERSION10	10

#ifdef SCROLLABLE_CURSORS 
This Protocol includes support for scrollable cursors 
and is purposely being undefined so that changes can be made
to the remote protocol version to support new features without the 'fear' that
they will be turned off once SCROLLABLE_CURSORS is turned on.

#define PROTOCOL_SCROLLABLE_CURSORS	this needs to be defined

#endif

/* Architecture types */

typedef enum {
    arch_generic	= 1,	/* Generic -- always use cannonical forms */
    arch_apollo		= 2,
    arch_sun		= 3,
    arch_vms		= 4,
    arch_ultrix		= 5,
    arch_alliant	= 6,
    arch_msdos		= 7,
    arch_sun4		= 8,
    arch_sun386		= 9,
    arch_hpux		= 10,
    arch_hpmpexl	= 11,
    arch_mac		= 12,
    arch_macaux		= 13,
    arch_rt		= 14,
    arch_mips_ultrix	= 15,
    arch_hpux_68k	= 16,
    arch_xenix		= 17,
    arch_aviion		= 18,
    arch_sgi		= 19,
    arch_apollo_dn10k	= 20,
    arch_cray		= 21,
    arch_imp		= 22,
    arch_delta		= 23,
    arch_sco		= 24,
    arch_next		= 25,
    arch_next_386	= 26,
    arch_m88k		= 27,
    arch_unixware	= 28,
    arch_intel_32	= 29,	/* generic Intel chip w/32-bit compilation */
    arch_epson		= 30,
    arch_decosf		= 31,
    arch_ncr3000	= 32,
    arch_nt_ppc		= 33,
    arch_dg_x86		= 34,
    arch_sco_ev		= 35,
    arch_linux		= 36,

    arch_max		= 37	/* Keep this at the end */
} P_ARCH;

#ifdef DECOSF
#define xdr_long		xdr_int
#endif

/* Protocol Types */

#define ptype_page		1	/* Page server protocol */
#define ptype_rpc		2	/* Simple remote procedure call */
#define ptype_batch_send	3	/* Batch sends, no asynchrony */
#define ptype_out_of_band	4	/* Batch sends w/ out of band notification */

/* Generic object id */

typedef USHORT OBJCT;
#define MAX_OBJCT_HANDLES	65000

/* Operation (packet) types */

typedef enum {
    op_void			= 0,	/* Packet has been voided */
    op_connect			= 1,	/* Connect to remote server */
    op_exit			= 2,	/* Remote end has exitted */
    op_accept			= 3,	/* Server accepts connection */
    op_reject			= 4,	/* Server rejects connection */
    op_protocol			= 5,	/* Protocol selection */
    op_disconnect		= 6,	/* Connect is going away */
    op_credit			= 7,	/* Grant (buffer) credits */
    op_continuation		= 8,	/* Continuation packet */
    op_response			= 9,	/* Generic response block */

    /* Page server operations */

    op_open_file		= 10,	/* Open file for page service */
    op_create_file		= 11,	/* Create file for page service */
    op_close_file		= 12,	/* Close file for page service */
    op_read_page		= 13,	/* optionally lock and read page */
    op_write_page		= 14,	/* write page and optionally release lock */
    op_lock			= 15,	/* sieze lock */
    op_convert_lock		= 16,	/* convert existing lock */
    op_release_lock		= 17,	/* release existing lock */
    op_blocking			= 18,	/* blocking lock message */

    /* Full context server operations */

    op_attach			= 19,	/* Attach database */
    op_create			= 20,	/* Create database */
    op_detach			= 21,	/* Detach database */
    op_compile			= 22,	/* Request based operations */
    op_start			= 23,
    op_start_and_send		= 24,	
    op_send			= 25,
    op_receive			= 26,
    op_unwind			= 27,
    op_release			= 28,

    op_transaction		= 29,	/* Transaction operations */
    op_commit			= 30,
    op_rollback			= 31,
    op_prepare			= 32,
    op_reconnect		= 33,

    op_create_blob		= 34,	/* Blob operations */
    op_open_blob		= 35,
    op_get_segment		= 36,
    op_put_segment		= 37,
    op_cancel_blob		= 38,
    op_close_blob		= 39,

    op_info_database		= 40,	/* Information services */
    op_info_request		= 41,
    op_info_transaction		= 42,
    op_info_blob		= 43,

    op_batch_segments		= 44,	/* Put a bunch of blob segments */

    op_mgr_set_affinity		= 45,	/* Establish server affinity */
    op_mgr_clear_affinity	= 46,	/* Break server affinity */
    op_mgr_report		= 47,	/* Report on server */

    op_que_events		= 48,	/* Que event notification request */
    op_cancel_events		= 49,	/* Cancel event notification request */
    op_commit_retaining		= 50,	/* Commit retaining (what else) */
    op_prepare2			= 51,	/* Message form of prepare */
    op_event			= 52,	/* Completed event request (asynchronous) */
    op_connect_request		= 53,	/* Request to establish connection */
    op_aux_connect		= 54,	/* Establish auxiliary connection */
    op_ddl			= 55,	/* DDL call */
    op_open_blob2		= 56,
    op_create_blob2		= 57,
    op_get_slice		= 58,
    op_put_slice		= 59,
    op_slice			= 60,	/* Successful response to op_get_slice */
    op_seek_blob		= 61,	/* Blob seek operation */

    /* DSQL operations */

    op_allocate_statement 	= 62,	/* allocate a statment handle */
    op_execute			= 63,	/* execute a prepared statement */
    op_exec_immediate		= 64,	/* execute a statement */
    op_fetch			= 65,	/* fetch a record */
    op_fetch_response		= 66,	/* response for record fetch */
    op_free_statement		= 67,	/* free a statement */
    op_prepare_statement 	= 68,	/* prepare a statement */
    op_set_cursor		= 69,	/* set a cursor name */
    op_info_sql			= 70,

    op_dummy			= 71,	/* dummy packet to detect loss of client */

    op_response_piggyback 	= 72,	/* response block for piggybacked messages */
    op_start_and_receive 	= 73,
    op_start_send_and_receive 	= 74,

    op_exec_immediate2		= 75,	/* execute an immediate statement with msgs */
    op_execute2			= 76,	/* execute a statement with msgs */
    op_insert			= 77,
    op_sql_response		= 78,	/* response from execute, exec immed, insert */

    op_transact			= 79,
    op_transact_response 	= 80,
    op_drop_database		= 81,

    op_service_attach		= 82,
    op_service_detach		= 83,
    op_service_info		= 84,
    op_service_start		= 85,

    op_rollback_retaining	= 86,

    op_max
} P_OP;

/* Count String Structure */

typedef struct cstring {
    USHORT	cstr_length;
    USHORT	cstr_allocated;
    UCHAR	*cstr_address;
} CSTRING;

typedef struct lstring {
    ULONG	lstr_length;
    ULONG	lstr_allocated;
    UCHAR	*lstr_address;
} LSTRING;

#ifdef DEBUG_XDR_MEMORY
/* Debug xdr memory allocations */

#define P_MALLOC_SIZE	64	/* Xdr memory allocations per packet */

typedef struct p_malloc {
    P_OP	p_operation;	/* Operation/packet type */
    ULONG	p_allocated;	/* Memory length */
    UCHAR	*p_address;	/* Memory address */
/*  UCHAR	*p_xdrvar; */	/* XDR variable */
} P_MALLOC;
#endif

/* Connect Block (Client to server) */

typedef struct p_cnct {
    P_OP	p_cnct_operation;	/* OP_CREATE or OP_OPEN */
    USHORT	p_cnct_cversion;	/* Version of connect protocol */
    P_ARCH	p_cnct_client;		/* Architecture of client */
    CSTRING	p_cnct_file;		/* File name */
    USHORT	p_cnct_count;		/* Protocol versions understood */
    CSTRING	p_cnct_user_id;		/* User identification stuff */
    struct	p_cnct_repeat
    {
    USHORT	p_cnct_version;		/* Protocol version number */
    P_ARCH	p_cnct_architecture;	/* Architecture of client */
    USHORT	p_cnct_min_type;	/* Minimum type */
    USHORT	p_cnct_max_type;	/* Maximum type */
    USHORT	p_cnct_weight;		/* Preference weight */
    }		p_cnct_versions [10];
} P_CNCT;

/* User identification data, if any, is of form:

    <type> <length> <data>

where

     type	is a byte code
     length	is an unsigned byte containing length of data
     data	is 'type' specific

*/

#define CNCT_user		1	/* User name */
#define CNCT_passwd		2
#define CNCT_ppo		3	/* Apollo person, project, organization */
#define CNCT_host		4
#define CNCT_group		5	/* Effective Unix group id */
#define CNCT_user_verification	6	/* Attach/create using this connection
					   will use user verification */

/* Accept Block (Server response to connect block) */

typedef struct p_acpt {
    USHORT	p_acpt_version;		/* Protocol version number */
    P_ARCH	p_acpt_architecture;	/* Architecture for protocol */
    USHORT	p_acpt_type;		/* Minimum type */
} P_ACPT;

/* Generic Response block */

typedef struct p_resp {
    OBJCT	p_resp_object;		/* Object id */
    struct bid	p_resp_blob_id;		/* Blob id */
    CSTRING	p_resp_data;		/* Data */
    STATUS	*p_resp_status_vector;
    UCHAR	*p_resp_strings [10];
} P_RESP;

#define p_resp_partner	p_resp_blob_id.bid_number

/* Attach and create database */

typedef struct p_atch {
    OBJCT	p_atch_database;	/* Database object id */
    CSTRING	p_atch_file;		/* File name */
    CSTRING	p_atch_dpb;		/* Database parameter block */
} P_ATCH;

/* Compile request */

typedef struct p_cmpl {
    OBJCT	p_cmpl_database;	/* Database object id */
    CSTRING	p_cmpl_blr;		/* Request blr */
} P_CMPL;

/* Start Transaction */

typedef struct p_sttr {
    OBJCT	p_sttr_database;	/* Database object id */
    CSTRING	p_sttr_tpb;		/* Transaction parameter block */
} P_STTR;

/* Generic release block */

typedef struct p_rlse {
    OBJCT	p_rlse_object;		/* Object to be released */
} P_RLSE;

/* Data block (start, start and send, send, receive) */

typedef struct p_data {
    OBJCT	p_data_request;		/* Request object id */
    USHORT	p_data_incarnation;	/* Incarnation of request */
    OBJCT	p_data_transaction;	/* Transaction object id */
    USHORT	p_data_message_number;	/* Message number in request */
    USHORT	p_data_messages;	/* Number of messages */
#ifdef SCROLLABLE_CURSORS
    USHORT	p_data_direction;	/* direction to scroll before returning records */
    ULONG	p_data_offset;		/* offset to scroll before returning records */
#endif
} P_DATA;

/* Execute stored procedure block */

typedef struct p_trrq {
    OBJCT	p_trrq_database;	/* Database object id */
    OBJCT	p_trrq_transaction;	/* Transaction object id */
    CSTRING	p_trrq_blr;		/* Message blr */
    USHORT	p_trrq_in_msg_length;
    USHORT	p_trrq_out_msg_length;
    USHORT	p_trrq_messages;	/* Number of messages */
} P_TRRQ;

/* Blob (create/open) and segment blocks */

typedef struct p_blob {
    OBJCT	p_blob_transaction;	/* Transaction */
    struct bid	p_blob_id;		/* Blob id for open */
    CSTRING	p_blob_bpb;		/* Blob parameter block */
} P_BLOB;

typedef struct p_sgmt {
    OBJCT	p_sgmt_blob;		/* Blob handle id */
    USHORT	p_sgmt_length;		/* Length of segment */
    CSTRING	p_sgmt_segment;		/* Data segment */
} P_SGMT;

typedef struct p_seek {
    OBJCT	p_seek_blob;		/* Blob handle id */
    SSHORT	p_seek_mode;		/* mode of seek */
    SLONG	p_seek_offset;		/* Offset of seek */
} P_SEEK;

/* Information request blocks */

typedef struct p_info {
    OBJCT	p_info_object;		/* Object of information */
    USHORT	p_info_incarnation;	/* Incarnation of object */
    CSTRING	p_info_items;		/* Information */
    CSTRING	p_info_recv_items;	/* Receive information */
    USHORT	p_info_buffer_length;	/* Target buffer length */
} P_INFO;

/* Event request block */

typedef struct p_event {
    OBJCT	p_event_database;	/* Database object id */
    CSTRING	p_event_items;		/* Event description block */
    SLONG	p_event_ast;		/* Address of ast routine */
    SLONG	p_event_arg;		/* Argument to ast routine */
    SLONG	p_event_rid;		/* Client side id of remote event */
} P_EVENT;

/* Prepare block */

typedef struct p_prep {
    OBJCT	p_prep_transaction;
    CSTRING	p_prep_data;
} P_PREP;

/* Connect request block */

typedef struct p_req {
    USHORT	p_req_type;		/* Connection type */
    OBJCT	p_req_object;		/* Related object */
    ULONG	p_req_partner;		/* Partner identification */
} P_REQ;

#define P_REQ_async	1		/* Auxiliary asynchronous port */

/* DDL request */

typedef struct p_ddl {
     OBJCT	p_ddl_database;		/* Database object id */
     OBJCT	p_ddl_transaction;	/* Transaction */
     CSTRING	p_ddl_blr;		/* Request blr */
} P_DDL;

/* Slice Operation */

typedef struct p_slc {
    OBJCT	p_slc_transaction;	/* Transaction */
    struct bid	p_slc_id;		/* Slice id */
    CSTRING	p_slc_sdl;		/* Slice description language */
    CSTRING	p_slc_parameters;	/* Slice parameters */
    LSTRING	p_slc_slice;		/* Slice proper */
    ULONG	p_slc_length;		/* Number of elements */
} P_SLC;

/* Response to get_slice */

typedef struct p_slr {
    LSTRING	p_slr_slice;		/* Slice proper */
    ULONG	p_slr_length;		/* Total length of slice */
    UCHAR	*p_slr_sdl;		/* *** not transfered *** */
    USHORT	p_slr_sdl_length;	/* *** not transfered *** */
} P_SLR;
 
/* DSQL structure definitions */

typedef struct p_sqlst {
    OBJCT	p_sqlst_transaction;	/* transaction object */
    OBJCT	p_sqlst_statement;	/* statement object */
    USHORT	p_sqlst_SQL_dialect;	/* the SQL dialect */
    CSTRING	p_sqlst_SQL_str;	/* statement to be prepared */
    USHORT	p_sqlst_buffer_length;	/* Target buffer length */
    CSTRING	p_sqlst_items;		/* Information */
    CSTRING	p_sqlst_blr;		/* blr describing message */
    USHORT	p_sqlst_message_number;
    USHORT	p_sqlst_messages;	/* Number of messages */
    CSTRING	p_sqlst_out_blr;	/* blr describing output message */
    USHORT	p_sqlst_out_message_number;
} P_SQLST;

typedef struct p_sqldata {
    OBJCT	p_sqldata_statement;	/* statement object */
    OBJCT	p_sqldata_transaction;	/* transaction object */
    CSTRING	p_sqldata_blr;		/* blr describing message */
    USHORT	p_sqldata_message_number;
    USHORT	p_sqldata_messages;	/* Number of messages */
    CSTRING	p_sqldata_out_blr;	/* blr describing output message */
    USHORT	p_sqldata_out_message_number;
    ULONG	p_sqldata_status;	/* final eof status */
} P_SQLDATA;

typedef struct p_sqlfree {
    OBJCT	p_sqlfree_statement;	/* statement object */
    USHORT	p_sqlfree_option;	/* option */
} P_SQLFREE;

typedef struct p_sqlcur {
    OBJCT	p_sqlcur_statement;	/* statement object */
    CSTRING	p_sqlcur_cursor_name;	/* cursor name */
    USHORT	p_sqlcur_type;		/* type of cursor */
} P_SQLCUR;

/* Generalize packet (sic!) */

typedef struct packet {
#ifdef DEBUG_XDR_MEMORY
    /* When XDR memory debugging is enabled, p_malloc must be
       the first subpacket and be followed by p_operation (see
       server.c/zap_packet()) */

    P_MALLOC	p_malloc [P_MALLOC_SIZE]; /* Debug xdr memory allocations */
#endif
    P_OP	p_operation;	/* Operation/packet type */
    P_CNCT	p_cnct;		/* Connect block */
    P_ACPT	p_acpt;		/* Accept connection */
    P_RESP	p_resp;		/* Generic response to a call */
    P_ATCH	p_atch;		/* Attach or create database */
    P_RLSE	p_rlse;		/* Release object */
    P_DATA	p_data;		/* Data packet */
    P_CMPL	p_cmpl;		/* Compile request */
    P_STTR	p_sttr;		/* Start transactions */
    P_BLOB	p_blob;		/* Create/Open blob */
    P_SGMT	p_sgmt;		/* Put_segment */
    P_INFO	p_info;		/* Information */
    P_EVENT	p_event;	/* Que event */
    P_PREP	p_prep;		/* New improved prepare */
    P_REQ	p_req;		/* Connection request */
    P_DDL	p_ddl;		/* Data definition call */
    P_SLC	p_slc;		/* Slice operator */
    P_SLR	p_slr;		/* Slice response */
    P_SEEK	p_seek;		/* Blob seek */
    P_SQLST	p_sqlst;	/* DSQL Prepare & Execute immediate */
    P_SQLDATA	p_sqldata;	/* DSQL Open Cursor, Execute, Fetch */
    P_SQLCUR	p_sqlcur;	/* DSQL Set cursor name */
    P_SQLFREE	p_sqlfree;	/* DSQL Free statement */
    P_TRRQ	p_trrq;		/* Transact request packet */
} PACKET;

#endif /* _REMOTE_PROTOCOL_H_ */
