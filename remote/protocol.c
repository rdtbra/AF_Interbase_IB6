/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		protocol.c
 *	DESCRIPTION:	Protocol data structure mapper
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
#include <string.h>
#include "../remote/remote.h"
#include "../jrd/codes.h"
#include "../jrd/sdl.h"
#include "../jrd/gdsassert.h"
#include "../remote/parse_proto.h"
#include "../remote/proto_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/sdl_proto.h"

#ifdef DEBUG_XDR_MEMORY
#define P_TRUE			xdr_debug_packet (xdrs, XDR_FREE, p)
#define P_FALSE			!xdr_debug_packet (xdrs, XDR_FREE, p)	
#define DEBUG_XDR_PACKET	(void) xdr_debug_packet (xdrs, XDR_DECODE, p)
#define DEBUG_XDR_ALLOC(xdrvar, addr, len) \
			xdr_debug_memory (xdrs, XDR_DECODE, xdrvar, addr, (ULONG) len)
#define DEBUG_XDR_FREE(xdrvar, addr, len) \
			xdr_debug_memory (xdrs, XDR_FREE, xdrvar, addr, (ULONG) len) 
#else
#define P_TRUE		TRUE
#define P_FALSE		FALSE
#define DEBUG_XDR_PACKET
#define DEBUG_XDR_ALLOC(xdrvar, addr, len)
#define DEBUG_XDR_FREE(xdrvar, addr, len)
#endif /* DEBUG_XDR_MEMORY */

#define MAP(routine, ptr)	if (!routine (xdrs, &ptr)) return P_FALSE;
#define MAX_OPAQUE		32768

typedef enum {
    TYPE_IMMEDIATE,
    TYPE_PREPARED
} SQL_STMT_TYPE;

static BOOLEAN	alloc_cstring (XDR *, CSTRING *);
static void	free_cstring (XDR *, CSTRING *);
static RSR	get_statement (XDR *, SSHORT);
static bool_t	xdr_cstring (XDR *, CSTRING *);
static bool_t	xdr_datum (XDR *, DSC *, BLOB_PTR *);
static bool_t	xdr_debug_packet (XDR *, enum xdr_op, PACKET *);

#ifndef SOLARIS  /* On SOLARIS, xdr_hyper() is provided by system XDR library */
static bool_t	xdr_hyper (register XDR *, SINT64 *);
#endif

static bool_t	xdr_longs (XDR *, CSTRING *);
static bool_t	xdr_message (XDR *, MSG, FMT);
static bool_t	xdr_quad (register XDR *, register struct bid *);
static bool_t	xdr_request (XDR *, USHORT, USHORT, USHORT);
static bool_t	xdr_semi_opaque (XDR *, MSG, FMT);
static bool_t	xdr_semi_opaque_slice (XDR *, LSTRING *);
static bool_t	xdr_slice (XDR *, LSTRING *, USHORT, BLOB_PTR *);
static bool_t	xdr_status_vector (XDR *, STATUS *, TEXT *strings[]);
static bool_t	xdr_sql_blr (XDR *, SLONG, CSTRING *, int, SQL_STMT_TYPE);
static bool_t	xdr_sql_message (XDR *, SLONG);
static bool_t	xdr_trrq_blr (XDR *, CSTRING *);
static bool_t	xdr_trrq_message (XDR *, USHORT);

extern bool_t	xdr_enum();
extern bool_t	xdr_short();
extern bool_t	xdr_u_short();
extern bool_t	xdr_long();
#ifdef SOLARIS
extern bool_t	xdr_hyper();
#endif
extern bool_t	xdr_opaque();
extern bool_t	xdr_string();
extern bool_t	xdr_float();
extern bool_t	xdr_double();
extern bool_t	xdr_wrapstring();

#ifndef IMP
extern bool_t	xdr_free();
#endif

#ifdef VMS
extern double	MTH$CVT_D_G(), MTH$CVT_G_D();

static STR	gfloat_buffer;
#endif  

#ifdef	mips
#define	LOC_DOUBLE
#endif
#ifdef	NeXT
#define	LOC_DOUBLE
#endif

#ifdef SHLIB_DEFS
#undef connect
#define xdr_enum	(*_libgds_xdr_enum)
#define xdr_short	(*_libgds_xdr_short)
#define xdr_u_short	(*_libgds_xdr_u_short)
#define xdr_long	(*_libgds_xdr_long)
#define xdr_opaque	(*_libgds_xdr_opaque)
#define xdr_string	(*_libgds_xdr_string)
#ifndef sgi
#define xdr_float       (*_libgds_xdr_float)
#define xdr_double      (*_libgds_xdr_double)
#endif
#define	xdr_wrapstring	(*_libgds_xdr_wrapstring)
#ifndef IMP
#define	xdr_free	(*_libgds_xdr_free)
#endif

extern bool_t	xdr_enum();
extern bool_t	xdr_short();
extern bool_t	xdr_u_short();
extern bool_t	xdr_long();
extern bool_t	xdr_opaque();
extern bool_t	xdr_string();
#ifndef sgi
extern bool_t	xdr_float();
extern bool_t	xdr_double();
#endif
extern bool_t	xdr_wrapstring();
#ifndef IMP
extern bool_t	xdr_free();
#endif
#endif

#ifdef DEBUG
static ULONG	xdr_save_size = 0;
#define DEBUG_PRINTSIZE(p)  ib_fprintf (ib_stderr, "xdr_protocol: %s op %d size %d\n", \
		((xdrs->x_op == XDR_FREE)   ? "free" : \
		 (xdrs->x_op == XDR_ENCODE) ? "enc " : \
		 (xdrs->x_op == XDR_DECODE) ? "dec " : \
	                                      "othr"), \
		(p),                                   \
		((xdrs->x_op == XDR_ENCODE)            \
		     ? (xdrs->x_handy - xdr_save_size) \
		     : (xdr_save_size - xdrs->x_handy)))
#endif
#ifndef DEBUG_PRINTSIZE
#define DEBUG_PRINTSIZE(p)	/* nothing */
#endif

#ifdef DEBUG_XDR_MEMORY
void xdr_debug_memory (
    XDR		*xdrs,
    enum xdr_op	xop,
    void	*xdrvar,
    void	*address,
    ULONG	length)
{
/**************************************
 *
 *	x d r _ d e b u g _ m e m o r y
 *
 **************************************
 *
 * Functional description
 *	Track memory allocation patterns of XDR aggregate
 *	types (i.e. xdr_cstring, xdr_string, etc.) to
 *	validate that memory is not leaked by overwriting
 *	XDR aggregate pointers and that freeing a packet
 *	with REMOTE_free_packet() does not miss anything.
 *
 *	All memory allocations due to marshalling XDR
 *	variables are recorded in a debug memory alloca-
 *	tion table stored at the front of a packet.
 *
 *	Once a packet is being tracked it is an assertion
 *	error if a memory allocation can not be recorded
 *	due to space limitations or if a previous memory
 *	allocation being freed cannot be found. At most
 *	P_MALLOC_SIZE entries can be stored in the memory
 *	allocation table. A rough estimate of the number
 *	of XDR aggregates that can hang off a packet can
 *	be obtained by examining the subpackets defined
 *	in <remote/protocol.h>: A guestimate of 36 at this
 *	time includes 10 strings used to decode an xdr
 *	status vector.
 *
 **************************************/
PACKET	*packet;
VEC	vector;
PORT	port;
ULONG	i, j;

port = (PORT) xdrs->x_public;
assert (port != NULL_PTR);
assert (port->port_header.blk_type == type_port);

/* Compare the XDR variable address with the lower and upper bounds
   of each packet to determine which packet contains it. Record or
   delete an entry in that packet's memory allocation table. */

if (!(vector = port->port_packet_vector))  /* Not tracking port's protocol */
    return;

for (i = 0; i < vector->vec_count; i++)
    {
    if (packet = (PACKET*) vector->vec_object [i])
	{
	assert (packet->p_operation > op_void && packet->p_operation < op_max);

	if ((SCHAR*) xdrvar >= (SCHAR*) packet &&
	    (SCHAR*) xdrvar <  (SCHAR*) packet + sizeof (PACKET))
	    {
	    for (j = 0; j < P_MALLOC_SIZE; j++)
		{
		if (xop == XDR_FREE)
		    {
		    if ((SCHAR*) packet->p_malloc [j].p_address == (SCHAR*) address)
			{
			packet->p_malloc [j].p_operation = op_void;
			packet->p_malloc [j].p_allocated = NULL;
			packet->p_malloc [j].p_address = NULL_PTR;
		    /*  packet->p_malloc [j].p_xdrvar = NULL_PTR; */
			return;
			}
		    }
		else    /* XDR_ENCODE or XDR_DECODE */
		    {
		    assert (xop == XDR_ENCODE || xop == XDR_DECODE);
		    if (packet->p_malloc [j].p_operation == op_void)
			{
			packet->p_malloc [j].p_operation = packet->p_operation;
			packet->p_malloc [j].p_allocated = length;
			packet->p_malloc [j].p_address = address;
		    /*	packet->p_malloc [j].p_xdrvar = xdrvar; */
			return;
			}
		    }
		}
	    /* Assertion failure if not enough entries to record every xdr
	       memory allocation or an entry to be freed can't be found. */

	    assert (j < P_MALLOC_SIZE);	/* Increase P_MALLOC_SIZE if necessary */
	    }
	}
    }
assert (i < vector->vec_count); /* Couldn't find packet for this xdr arg */
}
#endif

bool_t xdr_protocol (
    XDR		*xdrs,
    PACKET	*p)
{
/**************************************
 *
 *	x d r _ p r o t o c o l
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free a protocol packet.
 *
 **************************************/
USHORT		i;
struct p_cnct_repeat	*tail;
PORT		port;
P_CNCT		*connect;
P_ACPT		*accept;
P_ATCH		*attach;
P_RESP		*response;
P_CMPL		*compile;
P_STTR		*transaction;
P_DATA		*data;
P_RLSE		*release;
P_BLOB		*blob;
P_SGMT		*segment;
P_INFO		*info;
P_PREP		*prepare;
P_EVENT		*event;
P_REQ		*request;
P_DDL		*ddl;
P_SLC		*slice;
P_SLR		*slice_response;
P_SEEK		*seek;
P_SQLFREE	*free_stmt;
P_SQLCUR	*sqlcur;
P_SQLST		*prep_stmt;
P_SQLDATA	*sqldata;
P_TRRQ		*trrq;
#ifdef DEBUG
xdr_save_size = xdrs->x_handy;
#endif

DEBUG_XDR_PACKET;

if (!xdr_enum (xdrs, &p->p_operation))
    return P_FALSE;

switch (p->p_operation)
    {
    case op_reject:
    case op_disconnect:
    case op_dummy:
	return P_TRUE;

    case op_connect:
	connect = &p->p_cnct;
	MAP (xdr_enum, connect->p_cnct_operation);
	MAP (xdr_short, connect->p_cnct_cversion);
	MAP (xdr_enum, connect->p_cnct_client);
	MAP (xdr_cstring, connect->p_cnct_file);
	MAP (xdr_short, connect->p_cnct_count);
	MAP (xdr_cstring, connect->p_cnct_user_id);
	for (i = 0, tail = connect->p_cnct_versions; 
	     i < connect->p_cnct_count; i++, tail++)
	    {
	    MAP (xdr_short, tail->p_cnct_version);
	    MAP (xdr_enum, tail->p_cnct_architecture);
	    MAP (xdr_u_short, tail->p_cnct_min_type);
	    MAP (xdr_u_short, tail->p_cnct_max_type);
	    MAP (xdr_short, tail->p_cnct_weight);
	    }
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_accept:
	accept = &p->p_acpt;
	MAP (xdr_short, accept->p_acpt_version);
	MAP (xdr_enum, accept->p_acpt_architecture);
	MAP (xdr_u_short, accept->p_acpt_type);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_connect_request:
    case op_aux_connect:
	request = &p->p_req;
	MAP (xdr_short, request->p_req_type);
	MAP (xdr_short, request->p_req_object);
	MAP (xdr_long, request->p_req_partner);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_attach:
    case op_create:
    case op_service_attach:
	attach = &p->p_atch;
	MAP (xdr_short, attach->p_atch_database);
	MAP (xdr_cstring, attach->p_atch_file);
	MAP (xdr_cstring, attach->p_atch_dpb);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_compile:
	compile = &p->p_cmpl;
	MAP (xdr_short, compile->p_cmpl_database);
	MAP (xdr_cstring, compile->p_cmpl_blr);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_receive:
    case op_start:
    case op_start_and_receive:
	data = &p->p_data;
	MAP (xdr_short, data->p_data_request);
	MAP (xdr_short, data->p_data_incarnation);
	MAP (xdr_short, data->p_data_transaction);
	MAP (xdr_short, data->p_data_message_number);
	MAP (xdr_short, data->p_data_messages);
#ifdef SCROLLABLE_CURSORS
	port = (PORT) xdrs->x_public;
	if ((p->p_operation == op_receive) && 
	    (port->port_protocol > PROTOCOL_VERSION8))
	    {
	    MAP (xdr_short, data->p_data_direction);
	    MAP (xdr_long, data->p_data_offset);
	    }

#endif	 
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_send:
    case op_start_and_send:
    case op_start_send_and_receive:
	data = &p->p_data;
	MAP (xdr_short, data->p_data_request);
	MAP (xdr_short, data->p_data_incarnation);
	MAP (xdr_short, data->p_data_transaction);
	MAP (xdr_short, data->p_data_message_number);
	MAP (xdr_short, data->p_data_messages);

	/* Changes to this op's protocol must mirror in xdr_protocol_overhead */

	return xdr_request (xdrs, data->p_data_request,
			    data->p_data_message_number,
			    data->p_data_incarnation) ? P_TRUE : P_FALSE;

    case op_response:
    case op_response_piggyback:

	/* Changes to this op's protocol must be mirrored 
	   in xdr_protocol_overhead */

	response = &p->p_resp;
	MAP (xdr_short, response->p_resp_object);
	MAP (xdr_quad, response->p_resp_blob_id);
	MAP (xdr_cstring, response->p_resp_data);
	return xdr_status_vector (xdrs, response->p_resp_status_vector,
				  response->p_resp_strings) ? P_TRUE : P_FALSE;

    case op_transact:
	trrq = &p->p_trrq;
	MAP (xdr_short, trrq->p_trrq_database);
	MAP (xdr_short, trrq->p_trrq_transaction);
	xdr_trrq_blr (xdrs, &trrq->p_trrq_blr);
	MAP (xdr_cstring, trrq->p_trrq_blr);
	MAP (xdr_short, trrq->p_trrq_messages);
	if (trrq->p_trrq_messages)
	    return xdr_trrq_message (xdrs, 0) ? P_TRUE : P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_transact_response:
	data = &p->p_data;
	MAP (xdr_short, data->p_data_messages);
	if (data->p_data_messages)
	    return xdr_trrq_message (xdrs, 1) ? P_TRUE : P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_open_blob2:
    case op_create_blob2:
	blob = &p->p_blob;
	MAP (xdr_cstring, blob->p_blob_bpb);

    case op_open_blob:
    case op_create_blob:
	blob = &p->p_blob;
	MAP (xdr_short, blob->p_blob_transaction);
	MAP (xdr_quad, blob->p_blob_id);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_get_segment:
    case op_put_segment:
    case op_batch_segments:
	segment = &p->p_sgmt;
	MAP (xdr_short, segment->p_sgmt_blob);
	MAP (xdr_short, segment->p_sgmt_length);
	MAP (xdr_cstring, segment->p_sgmt_segment);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_seek_blob:
	seek = &p->p_seek;
	MAP (xdr_short, seek->p_seek_blob);
	MAP (xdr_short, seek->p_seek_mode);
	MAP (xdr_long, seek->p_seek_offset);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_reconnect:
    case op_transaction:
	transaction = &p->p_sttr;
	MAP (xdr_short, transaction->p_sttr_database);
	MAP (xdr_cstring, transaction->p_sttr_tpb);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_info_blob:
    case op_info_database:
    case op_info_request:
    case op_info_transaction:
    case op_service_info:
    case op_info_sql:
	info = &p->p_info;
	MAP (xdr_short, info->p_info_object);
	MAP (xdr_short, info->p_info_incarnation);
	MAP (xdr_cstring, info->p_info_items);
	if (p->p_operation == op_service_info)
	    MAP (xdr_cstring, info->p_info_recv_items);
	MAP (xdr_short, info->p_info_buffer_length);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_service_start:
	info = &p->p_info;
	MAP (xdr_short, info->p_info_object);
	MAP (xdr_short, info->p_info_incarnation);
	MAP (xdr_cstring, info->p_info_items);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_commit:
    case op_prepare:
    case op_rollback:
    case op_unwind:
    case op_release:
    case op_close_blob:
    case op_cancel_blob:
    case op_detach:
    case op_drop_database:
    case op_service_detach:
    case op_commit_retaining:
    case op_rollback_retaining:
    case op_allocate_statement:
	release = &p->p_rlse;
	MAP (xdr_short, release->p_rlse_object);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_prepare2:
	prepare = &p->p_prep;
	MAP (xdr_short, prepare->p_prep_transaction);
	MAP (xdr_cstring, prepare->p_prep_data);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
    
    case op_que_events:
    case op_event:
	event = &p->p_event;
	MAP (xdr_short, event->p_event_database);
	MAP (xdr_cstring, event->p_event_items);
	MAP (xdr_long, event->p_event_ast);
	MAP (xdr_long, event->p_event_arg);
	MAP (xdr_long, event->p_event_rid);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_cancel_events:
	event = &p->p_event;
	MAP (xdr_short, event->p_event_database);
	MAP (xdr_long, event->p_event_rid);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_ddl:
	ddl = &p->p_ddl;
	MAP (xdr_short, ddl->p_ddl_database);
	MAP (xdr_short, ddl->p_ddl_transaction);
	MAP (xdr_cstring, ddl->p_ddl_blr);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_get_slice:
    case op_put_slice:
	slice = &p->p_slc;
	MAP (xdr_short, slice->p_slc_transaction);
	MAP (xdr_quad, slice->p_slc_id);
	MAP (xdr_long, slice->p_slc_length);
	MAP (xdr_cstring, slice->p_slc_sdl);
	MAP (xdr_longs, slice->p_slc_parameters);
	slice_response = &p->p_slr;
	if (slice_response->p_slr_sdl)
	    {
	    if (!xdr_slice (xdrs, &slice->p_slc_slice, 
			    slice_response->p_slr_sdl_length, slice_response->p_slr_sdl))
		return P_FALSE;
	    }
	else
	    if (!xdr_slice (xdrs, &slice->p_slc_slice, 
			    slice->p_slc_sdl.cstr_length, slice->p_slc_sdl.cstr_address))
		return P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;
	
    case op_slice:
	slice_response = &p->p_slr;
	MAP (xdr_long, slice_response->p_slr_length);
	if (!xdr_slice (xdrs, &slice_response->p_slr_slice, 
			slice_response->p_slr_sdl_length, slice_response->p_slr_sdl))
	    return P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_execute:
    case op_execute2:
   	sqldata = &p->p_sqldata;
	MAP (xdr_short, sqldata->p_sqldata_statement);
	MAP (xdr_short, sqldata->p_sqldata_transaction);
        if (xdrs->x_op == XDR_DECODE)
            {
	    /* the statement should be reset for each execution so that 
               all prefetched information from a prior execute is properly 
               cleared out.  This should be done before fetching any message 
               information (for example: blr info)
	    */

            RSR statement = NULL;
            statement = get_statement (xdrs, sqldata->p_sqldata_statement);
            if (statement)
		REMOTE_reset_statement (statement);
            }

	xdr_sql_blr (xdrs, (SLONG) sqldata->p_sqldata_statement,
		     &sqldata->p_sqldata_blr, FALSE, TYPE_PREPARED);
	MAP (xdr_short, sqldata->p_sqldata_message_number);
	MAP (xdr_short, sqldata->p_sqldata_messages);
	if (sqldata->p_sqldata_messages)
	    {
	    if (!xdr_sql_message (xdrs, (SLONG) sqldata->p_sqldata_statement))
	        return P_FALSE;
	    }
	if (p->p_operation == op_execute2)
	    {
	    xdr_sql_blr (xdrs, (SLONG) -1, &sqldata->p_sqldata_out_blr, TRUE, TYPE_PREPARED);
	    MAP (xdr_short, sqldata->p_sqldata_out_message_number);
	    }
  	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_exec_immediate2:
	prep_stmt = &p->p_sqlst;
	xdr_sql_blr (xdrs, (SLONG) -1, &prep_stmt->p_sqlst_blr, FALSE, TYPE_IMMEDIATE);
	MAP (xdr_short, prep_stmt->p_sqlst_message_number);
	MAP (xdr_short, prep_stmt->p_sqlst_messages);
	if (prep_stmt->p_sqlst_messages)
	    {
	    if (!xdr_sql_message (xdrs, (SLONG) -1))
		return P_FALSE;
	    }
	xdr_sql_blr (xdrs, (SLONG) -1, &prep_stmt->p_sqlst_out_blr, TRUE, TYPE_IMMEDIATE);
	MAP (xdr_short, prep_stmt->p_sqlst_out_message_number);
	/* Fall into ... */

    case op_exec_immediate:
    case op_prepare_statement:
	prep_stmt = &p->p_sqlst;
	MAP (xdr_short, prep_stmt->p_sqlst_transaction);
	MAP (xdr_short, prep_stmt->p_sqlst_statement);
	MAP (xdr_short, prep_stmt->p_sqlst_SQL_dialect);
	MAP (xdr_cstring, prep_stmt->p_sqlst_SQL_str);
	MAP (xdr_cstring, prep_stmt->p_sqlst_items);
	MAP (xdr_short, prep_stmt->p_sqlst_buffer_length);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_fetch:
	sqldata = &p->p_sqldata;
	MAP (xdr_short, sqldata->p_sqldata_statement);
	xdr_sql_blr (xdrs, (SLONG) sqldata->p_sqldata_statement,
		     &sqldata->p_sqldata_blr, TRUE, TYPE_PREPARED);
	MAP (xdr_short, sqldata->p_sqldata_message_number);
	MAP (xdr_short, sqldata->p_sqldata_messages);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_fetch_response:
	sqldata = &p->p_sqldata;
	MAP (xdr_long, sqldata->p_sqldata_status);
	MAP (xdr_short, sqldata->p_sqldata_messages);

	/* Changes to this op's protocol must mirror in xdr_protocol_overhead */

	port = (PORT) xdrs->x_public;
	if ((port->port_protocol > PROTOCOL_VERSION7 && sqldata->p_sqldata_messages) ||
	    (port->port_protocol <= PROTOCOL_VERSION7 && !sqldata->p_sqldata_status))
	    return xdr_sql_message (xdrs, (SLONG) sqldata->p_sqldata_statement) ? P_TRUE : P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_free_statement:
	free_stmt = &p->p_sqlfree;
	MAP (xdr_short, free_stmt->p_sqlfree_statement);
	MAP (xdr_short, free_stmt->p_sqlfree_option);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_insert:
	sqldata = &p->p_sqldata;
	MAP (xdr_short, sqldata->p_sqldata_statement);
	xdr_sql_blr (xdrs, (SLONG) sqldata->p_sqldata_statement,
		     &sqldata->p_sqldata_blr, FALSE, TYPE_PREPARED);
	MAP (xdr_short, sqldata->p_sqldata_message_number);
	MAP (xdr_short, sqldata->p_sqldata_messages);
	if (sqldata->p_sqldata_messages)
	    return xdr_sql_message (xdrs, (SLONG) sqldata->p_sqldata_statement) ? P_TRUE : P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_set_cursor:
	sqlcur = &p->p_sqlcur;
	MAP (xdr_short, sqlcur->p_sqlcur_statement);
	MAP (xdr_cstring, sqlcur->p_sqlcur_cursor_name);
	MAP (xdr_short, sqlcur->p_sqlcur_type);
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    case op_sql_response:
	sqldata = &p->p_sqldata;
	MAP (xdr_short, sqldata->p_sqldata_messages);
	if (sqldata->p_sqldata_messages)
	    return xdr_sql_message (xdrs, (SLONG) -1) ? P_TRUE : P_FALSE;
	DEBUG_PRINTSIZE (p->p_operation);
	return P_TRUE;

    default:
#ifdef DEBUG
	if (xdrs->x_op != XDR_FREE)
	    ib_fprintf (ib_stderr, "xdr_packet: operation %d not recognized\n",
		    p->p_operation);
#endif
	assert (xdrs->x_op == XDR_FREE);
	return P_FALSE;
    }
}

ULONG xdr_protocol_overhead (
    P_OP	op)
{
/**************************************
 *
 *	x d r _ p r o t o c o l _ o v e r h e a d
 *
 **************************************
 *
 * Functional description
 *	Report the overhead size of a particular packet.
 *	NOTE: This is not the same as the actual size to
 *	send the packet - as this figure discounts any data
 *	to be sent with the packet.  It's purpose is to figure
 *	overhead for deciding on a batching window count.
 *
 *	A better version of this routine would use xdr_sizeof - but
 *	it is unknown how portable that Solaris call is to other
 *	OS.
 *
 **************************************/
ULONG	size;

size = 4  /* xdr_sizeof (xdr_enum, p->p_operation) */;

switch (op)
    {
    case op_fetch_response:
	size += 4 /* xdr_sizeof (xdr_long, sqldata->p_sqldata_status) */
	      + 4 /* xdr_sizeof (xdr_short, sqldata->p_sqldata_messages) */;
	break;

    case op_send:
    case op_start_and_send:
    case op_start_send_and_receive:
	size += 4 /* xdr_sizeof (xdr_short, data->p_data_request) */
	      + 4 /* xdr_sizeof (xdr_short, data->p_data_incarnation) */
	      + 4 /* xdr_sizeof (xdr_short, data->p_data_transaction) */
	      + 4 /* xdr_sizeof (xdr_short, data->p_data_message_number) */
	      + 4 /* xdr_sizeof (xdr_short, data->p_data_messages) */;
	break;

    case op_response:
    case op_response_piggyback:
	/* Note: minimal amounts are used for cstring & status_vector */
	size += 4 /* xdr_sizeof (xdr_short, response->p_resp_object) */
	      + 8 /* xdr_sizeof (xdr_quad, response->p_resp_blob_id) */
	      + 4 /* xdr_sizeof (xdr_cstring, response->p_resp_data) */
	      + 3 * 4 /* xdr_sizeof (xdr_status_vector (xdrs, response->p_resp_status_vector */;
	break;

    default:
	assert (FALSE);		/* Not supported operation */
	return 0;
    }
return size;
}

static BOOLEAN alloc_cstring (
    XDR		*xdrs,
    CSTRING	*cstring)
{
/**************************************
 *
 *	a l l o c _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Handle allocation for cstring.
 *
 **************************************/

if (!cstring->cstr_length)
    return TRUE;

if (cstring->cstr_length > cstring->cstr_allocated &&
    cstring->cstr_allocated)
    free_cstring (xdrs, cstring);

if (!cstring->cstr_address)
    {
    if (!(cstring->cstr_address = ALLR_alloc ((SLONG) cstring->cstr_length)))
	/* NOMEM: handled by ALLR_alloc() */
	/* FREE:  in realloc case above & free_cstring() */
	return FALSE;
	
    cstring->cstr_allocated = cstring->cstr_length;
    DEBUG_XDR_ALLOC (cstring, cstring->cstr_address, cstring->cstr_allocated);
    }

return TRUE;
}

static void free_cstring (
    XDR		*xdrs,
    CSTRING	*cstring)
{
/**************************************
 *
 *	f r e e _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Free any memory allocated for a cstring.
 *
 **************************************/

if (cstring->cstr_allocated)
    {
    ALLR_free (cstring->cstr_address);
    DEBUG_XDR_FREE (cstring, cstring->cstr_address, cstring->cstr_allocated);
    }

cstring->cstr_address = NULL;
cstring->cstr_allocated = 0;
}

static bool_t xdr_cstring (
    XDR		*xdrs,
    CSTRING	*cstring)
{
/**************************************
 *
 *	x d r _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Map a counted string structure.
 *
 **************************************/
SLONG	l;
SCHAR	trash[4];
static CONST SCHAR	filler[4] = {0,0,0,0};

if (!xdr_short (xdrs, &cstring->cstr_length))
    return FALSE;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	if (cstring->cstr_length && 
	    !(*xdrs->x_ops->x_putbytes) (xdrs, cstring->cstr_address, cstring->cstr_length))
	    return FALSE;
	l = (4 - cstring->cstr_length) & 3;
	if (l)
	    return (*xdrs->x_ops->x_putbytes) (xdrs, filler, l);
	return TRUE;
    
    case XDR_DECODE:
	if (!alloc_cstring (xdrs, cstring))
	    return FALSE;
	if (!(*xdrs->x_ops->x_getbytes) (xdrs, cstring->cstr_address,
		cstring->cstr_length))
	    return FALSE;
	l = (4 - cstring->cstr_length) & 3;
	if (l)
	    return (*xdrs->x_ops->x_getbytes) (xdrs, trash, l);
	return TRUE;
    
    case XDR_FREE:
	free_cstring (xdrs, cstring);
	return TRUE;
    }

return FALSE;	
}

static bool_t xdr_datum (
    XDR		*xdrs,
    DSC		*desc,
    BLOB_PTR	*buffer)
{
/**************************************
 *
 *	x d r _ d a t u m
 *
 **************************************
 *
 * Functional description
 *	Handle a data item by relative descriptor and buffer.
 *
 **************************************/
BLOB_PTR        *p;
SSHORT		n;

p = buffer + (ULONG) desc->dsc_address;

switch (desc->dsc_dtype)
    {
    case dtype_text:
	if (!xdr_opaque (xdrs, p, desc->dsc_length))
	    return FALSE;
	break;
    
    case dtype_varying:
	assert (desc->dsc_length >= sizeof (USHORT));
	if (!xdr_short (xdrs, &((VARY) p)->vary_length))
	    return FALSE;
	if (!xdr_opaque (xdrs, ((VARY) p)->vary_string,
	    MIN ((USHORT) (desc->dsc_length - 2), ((VARY) p)->vary_length)))
	    return FALSE;
	break;
    
    case dtype_cstring:
	if (xdrs->x_op == XDR_ENCODE)
	    n = MIN (strlen (p), (ULONG) (desc->dsc_length - 1));
	if (!xdr_short (xdrs, &n))
	    return FALSE;
	if (!xdr_opaque (xdrs, p, n))
	    return FALSE;
	if (xdrs->x_op == XDR_DECODE)
	    p [n] = 0;
	break;
    
    case dtype_short:
	assert (desc->dsc_length >= sizeof (SSHORT));
	if (!xdr_short (xdrs, p))
	    return FALSE;
	break;

    case dtype_sql_time:
    case dtype_sql_date:
    case dtype_long:
	assert (desc->dsc_length >= sizeof (SLONG));
	if (!xdr_long (xdrs, p))
	    return FALSE;
	break;

    case dtype_real:
	assert (desc->dsc_length >= sizeof (float));
	if (!xdr_float (xdrs, p))
	    return FALSE;
	break;

    case dtype_double:
	assert (desc->dsc_length >= sizeof (double));
	if (!xdr_double (xdrs, p))
	    return FALSE;
	break;

#ifdef VMS
    case dtype_d_float:
	assert (desc->dsc_length >= sizeof (d_float));
	if (!xdr_d_float (xdrs, p))
	    return FALSE;
	break;
#endif

    case dtype_timestamp:
	assert (desc->dsc_length >= 2* sizeof (SLONG));
	if (!xdr_long (xdrs, &((SLONG*) p) [0]))
	    return FALSE;
	if (!xdr_long (xdrs, &((SLONG*) p) [1]))
	    return FALSE;
	break;

    case dtype_int64:
	assert (desc->dsc_length >= sizeof (SINT64));
	if (!xdr_hyper (xdrs, (SINT64*) p))
	    return FALSE;
	break;

    case dtype_array:
    case dtype_quad:
    case dtype_blob:
	assert (desc->dsc_length >= sizeof (struct bid));
	if (!xdr_quad (xdrs, (struct bid *) p))
	    return FALSE;
	break;
    
    default:
	assert (FALSE);
	return FALSE;
    }

return TRUE;
}

#ifdef DEBUG_XDR_MEMORY
static bool_t xdr_debug_packet (
    XDR		*xdrs,
    enum xdr_op	xop,
    PACKET	*packet)
{
/**************************************
 *
 *	x d r _ d e b u g _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Start/stop monitoring a packet's memory allocations by
 *	entering/removing from a port's packet tracking vector.
 *
 **************************************/
VEC	vector;
PORT	port;
ULONG	i;

port = (PORT) xdrs->x_public;
assert (port != NULL_PTR);
assert (port->port_header.blk_type == type_port);

if (xop == XDR_FREE)
    {
    /* Free a slot in the packet tracking vector */

    if (vector = port->port_packet_vector)
	for (i = 0; i < vector->vec_count; i++)
	    if (vector->vec_object [i] == (BLK) packet)
		{
	    	vector->vec_object [i] = NULL_PTR;
		return TRUE;
		}
    }
else    /* XDR_ENCODE or XDR_DECODE */
    {
    /* Allocate an unused slot in the packet tracking vector
       to start recording memory allocations for this packet. */

    assert (xop == XDR_ENCODE || xop == XDR_DECODE);
    vector = ALLR_vector (&port->port_packet_vector, 0);

    for (i = 0; i < vector->vec_count; i++)
	if (vector->vec_object [i] == (BLK) packet)
	    return TRUE;

    for (i = 0; i < vector->vec_count; i++)
	if (vector->vec_object [i] == NULL_PTR)
	    break;

    if (i >= vector->vec_count)
	vector = ALLR_vector (&port->port_packet_vector, i);

    vector->vec_object [i] = (BLK) packet;
    }

return TRUE;
}
#endif

#ifndef SOLARIS
static bool_t xdr_hyper (
    register XDR	*xdrs,
             SINT64	*pi64)
{
/**************************************
 *
 *	x d r _ h y p e r       ( n o n - S O L A R I S )
 *
 **************************************
 *
 * Functional description
 *	Map a 64-bit Integer from external to internal representation 
 *      (or vice versa).
 *      
 *      Enable this for all platforms except Solaris (since it is
 *      available in the XDR library on Solaris). This function (normally) 
 *      would have been implemented in REMOTE/xdr.c. Since some system 
 *      XDR libraries (HP-UX) do not implement this function, we have it 
 *      in this module. At a later date, when the function is available 
 *      on all platforms, we can start using the system-provided version.
 *      
 *      Handles "swapping" of the 2 long's to be "Endian" sensitive. 
 *
 **************************************/
union
    {
    SINT64	temp_int64;
    SLONG	temp_long [2];
    }		temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp.temp_int64 = *pi64;
#ifdef VAX
	if ((*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [1]) && 
	    (*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [0]))
	    return TRUE;
#else
	if ((*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [0]) && 
	    (*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [1]))
	    return TRUE;
#endif
	return FALSE;

    case XDR_DECODE:
#ifdef VAX
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [1]) ||
	    !(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [0]))
	    return FALSE;
#else
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [0]) ||
	    !(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [1]))
	    return FALSE;
#endif
	*pi64 = temp.temp_int64;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }
}
#endif  /* SOLARIS */

static bool_t xdr_longs (
    XDR		*xdrs,
    CSTRING	*cstring)
{
/**************************************
 *
 *	x d r _ l o n g s
 *
 **************************************
 *
 * Functional description
 *	Pass a vector of longs.
 *
 **************************************/
ULONG	n;
SLONG	*next, *end;

if (!xdr_short (xdrs, &cstring->cstr_length))
    return FALSE;

/* Handle operation specific stuff, particularly memory allocation/deallocation */

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	break;
    
    case XDR_DECODE:
	if (!alloc_cstring (xdrs, cstring))
	    return FALSE;
	break;
    
    case XDR_FREE:
	free_cstring (xdrs, cstring);
	return TRUE;
    }

n = cstring->cstr_length / sizeof (SLONG);

for (next = (SLONG*) cstring->cstr_address, end = next + (int) n; next < end; next++)
    if (!xdr_long (xdrs, next))
	return FALSE;

return TRUE;
}

static bool_t xdr_message (
    XDR		*xdrs,
    MSG		message,
    FMT		format)
{
/**************************************
 *
 *	x d r _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted message.
 *
 **************************************/
PORT		port;
DSC		*desc, *end;

if (xdrs->x_op == XDR_FREE)
    return TRUE;

port = (PORT) xdrs->x_public;

/* If we are running a symmetric version of the protocol, just slop
   the bits and don't sweat the translations */

if (port->port_flags & PORT_symmetric)
#ifndef VMS
    return xdr_opaque (xdrs, message->msg_address, format->fmt_length);
#else
    if (port->port_protocol >= PROTOCOL_VERSION5)
	return xdr_opaque (xdrs, message->msg_address, format->fmt_length);
    else
	return xdr_semi_opaque (xdrs, message, format);
#endif

for (desc = format->fmt_desc, end = desc + format->fmt_count; desc < end; desc++)
    if (!xdr_datum (xdrs, desc, message->msg_address))
	return FALSE;

DEBUG_PRINTSIZE (0);
return TRUE;
}

static bool_t xdr_quad (
    register XDR	*xdrs,
    register struct bid	*ip)
{
/**************************************
 *
 *	x d r _ q u a d
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *	A "quad" is represented by two longs.
 *	Currently used only for blobs
 *
 **************************************/

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	if ((*xdrs->x_ops->x_putlong) (xdrs, &ip->bid_relation_id) &&
	    (*xdrs->x_ops->x_putlong) (xdrs, &ip->bid_number))
	    return TRUE;
	return FALSE;

    case XDR_DECODE:
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &ip->bid_relation_id))
	    return FALSE;
	return (*xdrs->x_ops->x_getlong) (xdrs, &ip->bid_number);

    case XDR_FREE:
	return TRUE;
    } 

return FALSE;
}

static bool_t xdr_request (
    XDR		*xdrs,
    USHORT	request_id,
    USHORT	message_number,
    USHORT	incarnation)
{
/**************************************
 *
 *	x d r _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Map a formatted message.
 *
 **************************************/
PORT		port;
MSG		message;
FMT		format;
RRQ		request;
struct rrq_repeat	*tail;

if (xdrs->x_op == XDR_FREE)
    return TRUE;

port = (PORT) xdrs->x_public;
request = (RRQ) port->port_objects [request_id];

if (!request)
    return FALSE;

if (incarnation && !(request = REMOTE_find_request (request, incarnation)))
    return FALSE;

tail = &request->rrq_rpt [message_number];

if (!(message = tail->rrq_xdr))
    return FALSE;

tail->rrq_xdr = message->msg_next;
format = tail->rrq_format;

/* Find the address of the record */

if (!message->msg_address)
    message->msg_address = message->msg_buffer;

return xdr_message (xdrs, message, format);
}

#ifdef VMS
static bool_t xdr_semi_opaque (
    XDR		*xdrs,
    MSG		message,
    FMT		format)
{
/**************************************
 *
 *	x d r _ s e m i _ o p a q u e
 *
 **************************************
 *
 * Functional description
 *	Move data while checking for doubles in d_float format.
 *
 **************************************/
DSC		*desc, *end;
BLOB_PTR        *msg_address;
double		*convert;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	for (desc = format->fmt_desc, end = desc + format->fmt_count; desc < end; desc++)
	    if (desc->dsc_dtype == dtype_d_float)
		break;

	if (desc >= end)
	    return xdr_opaque (xdrs, message->msg_address, format->fmt_length);

	if (!gfloat_buffer || gfloat_buffer->str_length < format->fmt_length)
	    {
	    if (gfloat_buffer)
		ALLR_free (gfloat_buffer);
	    gfloat_buffer = (STR) ALLOCV (type_str, format->fmt_length);
	    gfloat_buffer->str_length = format->fmt_length;
	    }

	msg_address = gfloat_buffer->str_data;
	memcpy (msg_address, message->msg_address, format->fmt_length);

	for (desc = format->fmt_desc, end = desc + format->fmt_count; desc < end; desc++)
	    if (desc->dsc_dtype == dtype_d_float)
		{
		convert = (double*) (msg_address + (int) desc->dsc_address);
		*convert = MTH$CVT_D_G (convert);
		}

	return xdr_opaque (xdrs, msg_address, format->fmt_length);

    case XDR_DECODE:
	if (!xdr_opaque (xdrs, message->msg_address, format->fmt_length))
	    return FALSE;

	for (desc = format->fmt_desc, end = desc + format->fmt_count; desc < end; desc++)
	    if (desc->dsc_dtype == dtype_d_float)
		{
		convert = (double*) (message->msg_address + (int) desc->dsc_address);
		*convert = MTH$CVT_G_D (convert);
		}
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }
}
#endif

#ifdef VMS
static bool_t xdr_semi_opaque_slice (
    XDR		*xdrs,
    LSTRING	*slice)
{
/**************************************
 *
 *	x d r _ s e m i _ o p a q u e _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Move data while converting for doubles in d_float format.
 *
 **************************************/
ULONG		n, msg_len;
BLOB_PTR        *p, *msg_addr;
double		*convert, *end;

p = slice->lstr_address;
for (n = slice->lstr_length; n; n -= msg_len, p += msg_len)
    {
    msg_len = MIN (n, MAX_OPAQUE);

    if (xdrs->x_op == XDR_ENCODE)
	{
	/* Using an str structure is fine as long as MAX_OPAQUE < 64K */

	if (!gfloat_buffer || gfloat_buffer->str_length < msg_len)
	    {
	    if (gfloat_buffer)
		ALLR_free (gfloat_buffer);
	    gfloat_buffer = (STR) ALLOCV (type_str, msg_len);
	    gfloat_buffer->str_length = msg_len;
	    }

	msg_addr = gfloat_buffer->str_data;
	memcpy (msg_addr, p, msg_len);

	for (convert = (double*) msg_addr,
		end = (double*) (msg_addr + msg_len); convert < end; convert++)
	    *convert = MTH$CVT_D_G (convert);
	}
    else
	msg_addr = p;

    if (!xdr_opaque (xdrs, msg_addr, msg_len))
	return FALSE;

    if (xdrs->x_op == XDR_DECODE)
	{
	for (convert = (double*) msg_addr,
		end = (double*) (msg_addr + msg_len); convert < end; convert++)
	    *convert = MTH$CVT_G_D (convert);
	}
    }

return TRUE;
}
#endif

static bool_t xdr_slice (
    XDR		*xdrs,
    LSTRING	*slice,
    USHORT	sdl_length,
    BLOB_PTR	*sdl)
{
/**************************************
 *
 *	x d r _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Move a slice of an array under
 *
 **************************************/
STATUS		status_vector [ISC_STATUS_LENGTH];
PORT		port;
ULONG		n;
BLOB_PTR        *p;
DSC		*desc;
struct sdl_info info;

if (!xdr_long (xdrs, &slice->lstr_length))
    return FALSE;

/* Handle operation specific stuff, particularly memory allocation/deallocation */

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	break;
    
    case XDR_DECODE:
	if (!slice->lstr_length)
	    return TRUE;
	if (slice->lstr_length > slice->lstr_allocated &&
	    slice->lstr_allocated)
	    {
	    ALLR_free (slice->lstr_address);
	    DEBUG_XDR_FREE (slice, slice->lstr_address, slice->lstr_allocated);
	    slice->lstr_address = NULL;
	    }
	if (!slice->lstr_address)
	    {
	    if (!(slice->lstr_address = ALLR_alloc ((SLONG) slice->lstr_length)))
		/* NOMEM: handled by ALLR_alloc() */
		/* FREE:  in realloc case above & XDR_FREE case of this routine */
		return FALSE;

	    slice->lstr_allocated = slice->lstr_length;
	    DEBUG_XDR_ALLOC (slice, slice->lstr_address, slice->lstr_allocated);
	    }
	break;
    
    case XDR_FREE:
	if (slice->lstr_allocated)
	    {
	    ALLR_free (slice->lstr_address);
	    DEBUG_XDR_FREE (slice, slice->lstr_address, slice->lstr_allocated);
	    }
	slice->lstr_address = NULL;
	slice->lstr_allocated = 0;
	return TRUE;
    }

/* Get descriptor of array element */

if (SDL_info (status_vector, sdl, &info, NULL_PTR))
    return FALSE;

desc = &info.sdl_info_element;
port = (PORT) xdrs->x_public;
p = (BLOB_PTR *)slice->lstr_address;

if (port->port_flags & PORT_symmetric)
    {
#ifdef VMS
    if (port->port_protocol < PROTOCOL_VERSION5 &&
	desc->dsc_dtype == dtype_d_float)
	return xdr_semi_opaque_slice (xdrs, slice);
#endif

    for (n = slice->lstr_length; n > MAX_OPAQUE; n -= MAX_OPAQUE, p += (int) MAX_OPAQUE)
	if (!xdr_opaque (xdrs, p, MAX_OPAQUE))
	    return FALSE;
    if (n)
	if (!xdr_opaque (xdrs, p, n))
	    return FALSE;
    }
else
    {
    for (n = 0; n < slice->lstr_length / desc->dsc_length; n++) 
	{
	if (!xdr_datum (xdrs, desc, p))
	    return FALSE;
	p = p + (ULONG) desc->dsc_length;
	}
    }

return TRUE;
}

static bool_t xdr_sql_blr (
    XDR		*xdrs,
    SLONG	statement_id,
    CSTRING	*blr,
    int		direction,
    SQL_STMT_TYPE	stmt_type)
{
/**************************************
 *
 *	x d r _ s q l _ b l r
 *
 **************************************
 *
 * Functional description
 *	Map an sql blr string.  This work is necessary because
 *	we will use the blr to read data in the current packet.
 *
 **************************************/
PORT	port;
RSR	statement;
FMT	*fmt_ptr;
MSG	message;

if (!xdr_cstring (xdrs, blr))
    return FALSE;

/* We care about all receives and sends from fetch */

if (xdrs->x_op == XDR_FREE)
    return TRUE;

port = (PORT) xdrs->x_public;
if (statement_id >= 0)
    {
    if (!(statement = (RSR) port->port_objects [statement_id]))
	return FALSE;
    }
else
    {
    if (!(statement = port->port_statement))
	statement = port->port_statement = (RSR) ALLOC (type_rsr); 
    }

if ((xdrs->x_op == XDR_ENCODE) && !direction)
    {
    if (statement->rsr_bind_format)
	statement->rsr_format = statement->rsr_bind_format;
    return TRUE;
    }

/* Parse the blr describing the message. */

fmt_ptr = (direction) ?
	&statement->rsr_select_format : &statement->rsr_bind_format;

if (xdrs->x_op == XDR_DECODE)
    {
    /* For an immediate statement, flush out any previous format information
     * that might be hanging around from an earlier execution.
     * For all statements, if we have new blr, flush out the format information
     * for the old blr.
     */
    if (*fmt_ptr && ((stmt_type == TYPE_IMMEDIATE) || blr->cstr_length != 0))
	{
	ALLR_release (*fmt_ptr);
	*fmt_ptr = NULL;
	}

    /* If we have BLR describing a new input/output message, get ready by
     * setting up a format 
     */
    if (blr->cstr_length)
	{
        if ((message = (MSG) PARSE_messages (blr->cstr_address, blr->cstr_length)) != (MSG) -1)
	    {
	    *fmt_ptr = (FMT) message->msg_address;
	    ALLR_release (message);
	    }
	}
    }

/* If we know the length of the message, make sure there is a buffer
   large enough to hold it. */

if (!(statement->rsr_format = *fmt_ptr))
    return TRUE;

if (!(message = statement->rsr_buffer) ||
    statement->rsr_format->fmt_length > statement->rsr_fmt_length)
    {
    REMOTE_release_messages (message);
    statement->rsr_fmt_length = statement->rsr_format->fmt_length;
    statement->rsr_buffer = message = (MSG) ALLOCV (type_msg, statement->rsr_fmt_length);
    statement->rsr_message = message;
    message->msg_next = message;
#ifdef SCROLLABLE_CURSORS
    message->msg_prior = message;
#endif
    }

return TRUE;
}

static bool_t xdr_sql_message (
    XDR		*xdrs,
    SLONG	statement_id)
{
/**************************************
 *
 *	x d r _ s q l _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted sql message.
 *
 **************************************/
PORT		port;
MSG		message;
RSR		statement;

if (xdrs->x_op == XDR_FREE)
    return TRUE;

port = (PORT) xdrs->x_public;
if (statement_id >= 0)
    {
    if (!(statement = (RSR) port->port_objects [statement_id]))
	return FALSE;
    }
else
    statement = port->port_statement;

if (message = statement->rsr_buffer)
    {
    statement->rsr_buffer = message->msg_next;
    if (!message->msg_address)
	message->msg_address = message->msg_buffer;
    }

return xdr_message (xdrs, message, statement->rsr_format);
}

static bool_t xdr_status_vector (
    XDR		*xdrs,
    STATUS	*vector,
    TEXT	*strings[])
{
/**************************************
 *
 *	x d r _ s t a t u s _ v e c t o r
 *
 **************************************
 *
 * Functional description
 *	Map a status vector.  This is tricky since the status vector
 *	may contain argument types, numbers, and strings.
 *
 **************************************/
TEXT	**sp, **end;
SLONG	vec;
XDR	temp_xdrs;

/* If this is a free operation, release any allocated strings */

if (xdrs->x_op == XDR_FREE)
    {
    for (sp = strings, end = strings + 10; sp < end; sp++)
	if (*sp && !xdr_wrapstring (xdrs, sp))
	    return FALSE;
    return TRUE;
    }

while (TRUE)
    {
    if (xdrs->x_op == XDR_ENCODE)
	vec = (SLONG) *vector++;
    if (!xdr_long (xdrs, &vec))
	return FALSE;
    if (xdrs->x_op == XDR_DECODE)
	*vector++ = (STATUS) vec;
    switch ((USHORT) vec)
	{
	case gds_arg_end:
	    return TRUE;
	
	case gds_arg_interpreted:
	case gds_arg_string:
	    if (xdrs->x_op == XDR_ENCODE)
		{
		if (!xdr_wrapstring (xdrs, vector++))
		    return FALSE;
		}
	    else
		{
		/* Use the first slot in the strings table */
		sp = strings;
		if (*sp)
		    {
		    /* Slot is used, by a string passed in a previous
		     * status vector.  Free that string, and allocate
		     * a new one to prevent any size mismatches trashing
		     * memory.
		     */
		    temp_xdrs.x_public = xdrs->x_public;
		    temp_xdrs.x_op = XDR_FREE;
		    if (!xdr_wrapstring (&temp_xdrs, sp))
			return FALSE;
		    *sp = NULL;
		    }
		if (!xdr_wrapstring (xdrs, sp))
		    return FALSE;
		*vector++ = (STATUS) *sp;
		strings++;
		}
	    break;
	
	case gds_arg_number:
	default:
	    if (xdrs->x_op == XDR_ENCODE)
		vec = (SLONG) *vector++;
	    if (!xdr_long (xdrs, &vec))
		return FALSE;
	    if (xdrs->x_op == XDR_DECODE)
		*vector++ = (STATUS) vec;
	    break;
	}
    }
}

static bool_t xdr_trrq_blr (
    XDR		*xdrs,
    CSTRING	*blr)
{
/**************************************
 *
 *	x d r _ t r r q  _ b l r
 *
 **************************************
 *
 * Functional description
 *	Map a message blr string.  This work is necessary because
 *	we will use the blr to read data in the current packet.
 *
 **************************************/
PORT	port;
RPR	procedure;
MSG	message, temp;

if (!xdr_cstring (xdrs, blr))
    return FALSE;

/* We care about all receives and sends from fetch */

if (xdrs->x_op == XDR_FREE || xdrs->x_op == XDR_ENCODE)
    return TRUE;

port = (PORT) xdrs->x_public;
if (!(procedure = port->port_rpr))
    procedure = port->port_rpr = (RPR) ALLOC (type_rpr); 

/* Parse the blr describing the message. */

if (procedure->rpr_in_msg)
    {
    ALLR_release (procedure->rpr_in_msg);
    procedure->rpr_in_msg = NULL;
    }
if (procedure->rpr_in_format)
    {
    ALLR_release (procedure->rpr_in_format);
    procedure->rpr_in_format = NULL;
    }
if (procedure->rpr_out_msg)
    {
    ALLR_release (procedure->rpr_out_msg);
    procedure->rpr_out_msg = NULL;
    }
if (procedure->rpr_out_format)
    {
    ALLR_release (procedure->rpr_out_format);
    procedure->rpr_out_format = NULL;
    }
if ((message = PARSE_messages (blr->cstr_address, blr->cstr_length)) != (MSG) -1)
    {
    while (message)
	{
	if (message->msg_number == 0)
	    {
	    procedure->rpr_in_msg = message;
	    procedure->rpr_in_format = (FMT) message->msg_address;
	    message->msg_address = message->msg_buffer;
	    message = message->msg_next;
	    procedure->rpr_in_msg->msg_next = NULL;
	    }
	else if (message->msg_number == 1)
	    {
	    procedure->rpr_out_msg = message;
	    procedure->rpr_out_format = (FMT) message->msg_address;
	    message->msg_address = message->msg_buffer;
	    message = message->msg_next;
	    procedure->rpr_out_msg->msg_next = NULL;
	    }
	else
	    {
	    temp = message;
	    message = message->msg_next;
	    ALLR_release (temp);
	    }
	}
    }
else
   assert (FALSE);

return TRUE;
}

static bool_t xdr_trrq_message (
    XDR		*xdrs,
    USHORT	msg_type)
{
/**************************************
 *
 *	x d r _ t r r q _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Map a formatted transact request message.
 *
 **************************************/
PORT		port;
RPR		procedure;

if (xdrs->x_op == XDR_FREE)
    return TRUE;

port = (PORT) xdrs->x_public;
procedure = port->port_rpr;

if (msg_type == 1)
    return xdr_message (xdrs, procedure->rpr_out_msg,
			procedure->rpr_out_format);
else
    return xdr_message (xdrs, procedure->rpr_in_msg,
			procedure->rpr_in_format);
}

#ifdef LOC_DOUBLE
static bool_t xdr_double (
    register XDR	*xdrs,
    register double	*ip)
{
/**************************************
 *
 *	x d r _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SSHORT		t1;
union
    {
    double	temp_double;
    SLONG	temp_long [2];
    SSHORT	temp_short [4];
    }		temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
#ifdef	WINDOWS_ONLY
	memcpy ((UCHAR*) (&temp.temp_double), (UCHAR*) ip, sizeof (double));
#else
	temp.temp_double = *ip;
#endif
#ifndef IEEE
	if (temp.temp_double != 0)
	    temp.temp_short [0] -= 0x20;
	t1 = temp.temp_short [0];
	temp.temp_short [0] = temp.temp_short [1];
	temp.temp_short [1] = t1;
	t1 = temp.temp_short [2];
	temp.temp_short [2] = temp.temp_short [3];
	temp.temp_short [3] = t1;
#endif
#ifdef VAX
	if ((*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [1]) && 
	    (*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [0]))
	    return TRUE;
#else
	if ((*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [0]) && 
	    (*xdrs->x_ops->x_putlong) (xdrs, &temp.temp_long [1]))
	    return TRUE;
#endif
	return FALSE;

    case XDR_DECODE:
#ifdef VAX
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [1]) ||
	    !(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [0]))
	    return FALSE;
#else
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [0]) ||
	    !(*xdrs->x_ops->x_getlong) (xdrs, &temp.temp_long [1]))
	    return FALSE;
#endif
#ifndef IEEE
	t1 = temp.temp_short [0];
	temp.temp_short [0] = temp.temp_short [1];
	temp.temp_short [1] = t1;
	t1 = temp.temp_short [2];
	temp.temp_short [2] = temp.temp_short [3];
	temp.temp_short [3] = t1;
	if (temp.temp_double != 0)
	    temp.temp_short [0] += 0x20;
#endif
#ifdef	WINDOWS_ONLY
	memcpy ((UCHAR*) ip, (UCHAR*) (&temp.temp_double), sizeof (double));
#else
	*ip = temp.temp_double;
#endif
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }
}

static bool_t xdr_float (
    register XDR	*xdrs,
    register float	*ip)
{
/**************************************
 *
 *	x d r _ f l o a t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SSHORT		t1;
union {
    float		temp_float;
    unsigned short	temp_short [2];
    }		temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
#ifndef IEEE
	temp.temp_float = *ip;
	if (temp.temp_float)
	    {
	    t1 = temp.temp_short [0];
	    temp.temp_short [0] = temp.temp_short [1];
	    temp.temp_short [1] = t1 - 0x100;
	    }
	if (!(*xdrs->x_ops->x_putlong) (xdrs, &temp))
	    return FALSE;
	return TRUE;
#else
	return (*xdrs->x_ops->x_putlong) (xdrs, ip); 
#endif

    case XDR_DECODE:
#ifndef IEEE
	if (!(*xdrs->x_ops->x_getlong) (xdrs, &temp))
	    return FALSE;
	if (temp.temp_short [0] | temp.temp_short [1])
	    {
	    t1 = temp.temp_short [1];
	    temp.temp_short [1] = temp.temp_short [0];
	    temp.temp_short [0] = t1 + 0x100;
	    }
	*ip = temp.temp_float;
	return TRUE;
#else
	return (*xdrs->x_ops->x_getlong) (xdrs, ip);
#endif

    case XDR_FREE:
	return TRUE;
    }
}
#endif

#ifdef IMP
/**
	xdr_free is not available on Motorola IMP 5.3 1.0 mc68060 as system
	call. This routine is copied from the file remote/xdr.c.     
**/

bool_t xdr_free (
    xdrproc_t	proc,
    SCHAR	*objp)
{
/**************************************
 *
 *	x d r _ f r e e
 *
 **************************************
 *
 * Functional description
 *	Perform XDR_FREE operation on an XDR structure
 *
 **************************************/
XDR	xdrs;

xdrs.x_op = XDR_FREE;

return (*proc) (&xdrs, objp);
}
#endif


static RSR get_statement (
    XDR	     *xdrs,
    SSHORT   statement_id)
{
/**************************************
 *
 *	g e t _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Returns the statement based upon the statement id
 *      if statement_id = -1 then statement = port_statement
 *      otherwise, the statement comes from port_objects[statement_id]
 *      if there are no port_objects, then statement = NULL
 *
 **************************************/

RSR  statement = NULL;
PORT port = (PORT) xdrs->x_public;

/* if the statement ID is -1, this seems to indicate that we are 
   re-executing the previous statement.  This is not a
   well-understood area of the implementation.

if (statement_id == -1)
    statement = port->port_statement;
else
*/

assert (statement_id >= -1);

if ((port->port_objects) &&
    ((SLONG) statement_id < (SLONG) port->port_object_vector->vec_count) &&
    (statement_id >= 0))
	statement = (RSR) port->port_objects [(SLONG) statement_id];

/* Check that what we found really is a statement structure */
assert (!statement || (statement->rsr_header.blk_type == type_rsr));
return statement;
}
