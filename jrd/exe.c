/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		exe.c
 *	DESCRIPTION:	Statement execution
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
#include "../jrd/common.h"
#include "../jrd/ibsetjmp.h"
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../jrd/tra.h"
#include "../jrd/codes.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/rse.h"
#include "../jrd/lck.h"
#include "../jrd/intl.h"
#include "../jrd/rng.h"
#include "../jrd/sbm.h"
#include "../jrd/blb.h"
#include "../jrd/blr.h"
#include "../jrd/all_proto.h"
#include "../jrd/bkm_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/jrd_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/opt_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/rse_proto.h"
#include "../jrd/rng_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/vio_proto.h"
#ifdef HSDEBUGSTACK
#include "TestStck.h"
#endif

#ifdef SHLIB_DEFS
#undef send
#endif

static NOD	erase			(TDBB, NOD, SSHORT);
static void 	execute_looper		(TDBB, REQ, TRA,
						ENUM req_s);
static void	execute_procedure	(TDBB, NOD);
static REQ	execute_triggers	(TDBB, VEC *, REC, REC);
static NOD	looper			(TDBB, REQ, NOD);
static NOD	modify			(TDBB, register NOD, SSHORT);
static NOD	receive_msg		(TDBB, register NOD);
static void	release_blobs		(TDBB, REQ);
static void 	release_proc_save_points (REQ);
static void	release_triggers 	(TDBB, VEC);
#ifdef SCROLLABLE_CURSORS
static NOD 	seek_rse		(TDBB, REQ, NOD);
static void 	seek_rsb		(TDBB, REQ, RSB, USHORT, SLONG);
#endif
static NOD	selct			(TDBB, register NOD);
static NOD	send_msg		(TDBB, register NOD);
static void	set_error		(TDBB, XCP);
static NOD	stall			(TDBB, register NOD);
static NOD	store			(TDBB, register NOD,
						SSHORT);
static BOOLEAN	test_error		(TDBB, XCP);
static void	trigger_failure		(TDBB, REQ);
static void	validate		(TDBB, NOD);

#ifdef PC_ENGINE
static BOOLEAN	check_crack (RSB, USHORT);
static NOD	find			(TDBB, register NOD);
static NOD	find_dbkey		(TDBB, register NOD);
static LCK	implicit_record_lock (TRA, RPB *);
static NOD	release_bookmark	(TDBB, NOD);
static NOD	set_bookmark		(TDBB, NOD);
static NOD	set_index		(TDBB, register NOD);
static NOD	stream			(TDBB, register NOD);
#endif

#ifdef DEBUG_GDS_ALLOC
static SLONG	memory_debug = 1;
static SLONG	memory_count = 0;
#endif DEBUG_GDS_ALLOC

/* macro definitions */

#define NULL_STRING	"*** null ***"
#ifdef PC_PLATFORM
/* The PC cannot support that many recursions */
#define	MAX_CLONES	75
#endif

#if (defined SUPERSERVER) && (defined WIN_NT || defined SOLARIS_MT)
#define MAX_CLONES	750
#endif

#if defined (HP10) && defined (SUPERSERVER)
#define MAX_CLONES	110
#endif

#ifdef DECOSF
#define MAX_CLONES	200
#endif

#ifndef MAX_CLONES
#define MAX_CLONES	1000
#endif

#define ALL_TRIGS	0
#define PRE_TRIG	1
#define POST_TRIG	2

/* this constant defines how many records are locked
   before we check whether record locking has been
   turned off for a given relation; if we set the 
   constant to a low number, we will do too much 
   locking in the case where record locking is always
   turned on; too high and we will do too much record
   locking in the case where someone is only occasionally
   locking a record */

#define RECORD_LOCK_CHECK_INTERVAL	10


void EXE_assignment (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	E X E _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Perform an assignment
 *
 **************************************/
register REQ	request;
DSC		*to_desc, *from_desc, *missing, *indicator, temp;
register NOD	to;
REC		record;
SSHORT		null, id, len;

DEV_BLKCHK (node, type_nod);

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

/* Get descriptors of receiving and sending fields/parmaters, variables, etc. */

missing = NULL;
if (node->nod_arg [e_asgn_missing])
    missing = EVL_expr (tdbb, node->nod_arg [e_asgn_missing]);

to = node->nod_arg [e_asgn_to];
to_desc = EVL_assign_to (tdbb, to);
request->req_flags &= ~req_null;
from_desc = EVL_expr (tdbb, node->nod_arg [e_asgn_from]);
null = (request->req_flags & req_null) ? -1 : 0;

if (!null && missing && MOV_compare (missing, from_desc) == 0)
    null = -1;

/* If the value is non-missing, move/convert it.  Otherwise fill the
   field with appropriate nulls. */

if (!null)
    {
    /* if necessary and appropriate, use the indicator variable */

    if (to->nod_type == nod_argument && to->nod_arg [e_arg_indicator])
	{
	indicator = EVL_assign_to (tdbb, to->nod_arg[e_arg_indicator]); 
	temp.dsc_dtype = dtype_short;
	temp.dsc_length = sizeof (SSHORT);
	temp.dsc_scale = 0;
	temp.dsc_sub_type = 0;

	if ((from_desc->dsc_dtype <= dtype_varying) &&
	    (to_desc->dsc_dtype <= dtype_varying) &&
	    (TEXT_LEN (from_desc) > TEXT_LEN (to_desc)))
 	    len = TEXT_LEN (from_desc);
 	else
	    len = 0;

	temp.dsc_address = (UCHAR*) &len; 
	MOV_move (&temp, indicator);

	if (len)
	    {
	    temp = *from_desc;
	    temp.dsc_length = TEXT_LEN (to_desc); 
	    if (temp.dsc_dtype == dtype_cstring)
		temp.dsc_length += 1;
	    else if (temp.dsc_dtype == dtype_varying)
		temp.dsc_length += 2;
	    from_desc = &temp;
	    }
	} 

#ifndef VMS
    if (DTYPE_IS_BLOB (to_desc->dsc_dtype))
#else
    if (DTYPE_IS_BLOB (to_desc->dsc_dtype) && to_desc->dsc_dtype != dtype_d_float)
#endif
	BLB_move (tdbb, from_desc, to_desc, to);

    else if (!DSC_EQUIV (from_desc, to_desc))
	MOV_move (from_desc, to_desc);

    else if (from_desc->dsc_dtype == dtype_short)
	*((SSHORT*) to_desc->dsc_address) = *((SSHORT*) from_desc->dsc_address);

    else if (from_desc->dsc_dtype == dtype_long)
	*((SLONG*) to_desc->dsc_address) = *((SLONG*) from_desc->dsc_address);

    else if (from_desc->dsc_dtype == dtype_int64)
	*((SINT64*) to_desc->dsc_address) = *((SINT64*) from_desc->dsc_address);

    else if (((U_IPTR) from_desc->dsc_address & (ALIGNMENT - 1)) ||
	((U_IPTR) to_desc->dsc_address & (ALIGNMENT - 1)))
	MOVE_FAST (from_desc->dsc_address, to_desc->dsc_address,
		  from_desc->dsc_length);

    else
	MOVE_FASTER (from_desc->dsc_address, to_desc->dsc_address,
		  from_desc->dsc_length);
    to_desc->dsc_flags &= ~DSC_null;
    }
else if (node->nod_arg [e_asgn_missing2] &&
    (missing = EVL_expr (tdbb, node->nod_arg [e_asgn_missing2])))
    {
    MOV_move (missing, to_desc);
    to_desc->dsc_flags |= DSC_null;
    }
else
    {
    UCHAR	*p;
    USHORT	l;

    l = to_desc->dsc_length;
    p = to_desc->dsc_address;
    switch (to_desc->dsc_dtype)
	{
	case dtype_text:
		/* YYY - not necessarily the right thing to do */
		/* for text formats that don't have trailing spaces */
	    if (l) do *p++ = ' '; while (--l);
	    break;

	case dtype_cstring:
	    *p = 0;
	    break;

	case dtype_varying:
	    *(SSHORT *) p = 0;
	    break;

	default:
	    do *p++ = 0; while (--l);
	    break;
	}
    to_desc->dsc_flags |= DSC_null;
    }

/* Handle the null flag as appropriate for fields and message arguments. */

if (to->nod_type == nod_field)
    {
    id = (USHORT) to->nod_arg [e_fld_id];
    record = request->req_rpb [(int) to->nod_arg [e_fld_stream]].rpb_record;
    if (null)
	SET_NULL (record, id);
    else
	CLEAR_NULL (record, id);
    }
else if (to->nod_type == nod_argument && to->nod_arg [e_arg_flag])
    {
    to_desc = EVL_assign_to (tdbb, to->nod_arg [e_arg_flag]);

    /* If the null flag is a string with an effective length of one,
       then -1 will not fit.  Therefore, store 1 instead. */

    if (null &&
	to_desc->dsc_dtype <= dtype_varying &&
	to_desc->dsc_length <=
	    ((to_desc->dsc_dtype == dtype_text) ? 1 : ((to_desc->dsc_dtype == dtype_cstring) ? 2 : 3)))
	null = 1;

    temp.dsc_dtype = dtype_short;
    temp.dsc_length = sizeof (SSHORT);
    temp.dsc_scale = 0;
    temp.dsc_sub_type = 0;
    temp.dsc_address = (UCHAR*) &null;
    MOV_move (&temp, to_desc);
    if (null && to->nod_arg[e_arg_indicator])
	{
	to_desc = EVL_assign_to (tdbb, to->nod_arg [e_arg_indicator]);
	MOV_move (&temp, to_desc); 
	}
    }

request->req_operation = req_return;
}

#ifdef PC_ENGINE
BOOLEAN EXE_crack (
    TDBB	tdbb,
    RSB		rsb,
    USHORT	flags)
{
/**************************************
 *
 *	E X E _ c r a c k
 *
 **************************************
 *
 * Functional description
 *	Check whether stream is on a crack, BOF
 *	or EOF, according to the flags passed.
 *
 **************************************/
REQ	request;
RPB	*rpb;
IRSB    impure;


DEV_BLKCHK (rsb, type_rsb);

SET_TDBB (tdbb);
request = tdbb->tdbb_request;

/* correct boolean rsbs to point to the "real" rsb */

if (rsb->rsb_type == rsb_boolean)
    rsb = rsb->rsb_next;
impure = (IRSB) ((UCHAR*) request + rsb->rsb_impure);

/* if any of the passed flags are set, return TRUE */

if (impure->irsb_flags & flags)
    return TRUE;
else
    return FALSE;
}
#endif

REQ EXE_find_request (
    TDBB	tdbb,
    REQ 	request,
    BOOLEAN	validate)
{
/**************************************
 *
 *	E X E _ f i n d _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Find an inactive incarnation of a trigger request.  If necessary,
 *	clone it.
 *
 **************************************/
#ifdef ANY_THREADING
DBB	dbb;
#endif
REQ	clone, next;
USHORT	n, clones, count;
VEC	vector;

DEV_BLKCHK (request, type_req);

SET_TDBB (tdbb);
#ifdef ANY_THREADING
dbb = tdbb->tdbb_database;
#endif

/* I found a core file from my test runs that came from a NULL request -
 * but have no idea what test was running.  Let's bugcheck so we can
 * figure it out
 */
if (!request)
    BUGCHECK /* REQUEST */ (167); /* msg 167 invalid SEND request */

THD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_clone);
clone = NULL;
count = 0;
if (!(request->req_flags & req_in_use))
    clone = request;
else 
    {
    if (request->req_attachment == tdbb->tdbb_attachment)
	count++;

    /* Request exists and is in use.  Search clones for one in use by
    this attachment. If not found, return first inactive request. */

    clones = (vector = request->req_sub_requests) ? (vector->vec_count - 1) : 0;

    for (n = 1; n <= clones; n++)
	{
	next = CMP_clone_request (tdbb, request, n, validate);
	if (next->req_attachment == tdbb->tdbb_attachment)
	    {
	    if (!(next->req_flags & req_in_use))
		{
		clone = next;
		break;
		}
	    else
		count++;
	    }
	else 
	    if (!(next->req_flags & req_in_use) && !clone)
		clone = next;
	}

    if (count > MAX_CLONES)
	{
	THD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_clone);
	ERR_post (gds__req_max_clones_exceeded, 0);
	}
    if (!clone)
	clone = CMP_clone_request (tdbb, request, n, validate);
    }
clone->req_attachment = tdbb->tdbb_attachment;
clone->req_flags |= req_in_use;
THD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_clone);
return clone;
}

#ifdef PC_ENGINE
void EXE_mark_crack (
    TDBB	tdbb,
    RSB		rsb,
    USHORT	flag)
{
/**************************************
 *
 *	E X E _ m a r k _ c r a c k
 *
 **************************************
 *
 * Functional description
 *	Mark a stream as being at a crack,
 *	plus report the fact in the status
 *	vector.
 *
 **************************************/

SET_TDBB (tdbb);
DEV_BLKCHK (rsb, type_rsb);

/* correct boolean rsbs to point to the "real" rsb */

if (rsb->rsb_type == rsb_boolean)
    rsb = rsb->rsb_next;

RSE_MARK_CRACK (tdbb, rsb, flag);

if (flag == irsb_eof)
    ERR_warning (gds__stream_eof, 0);
else if (flag == irsb_bof)
    ERR_warning (gds__stream_bof, 0);
else if (flag & irsb_crack)
    ERR_warning (gds__stream_crack, 0);
}
#endif

void EXE_receive (
    TDBB		tdbb,
    register REQ	request,
    USHORT		msg,
    USHORT		length,
    UCHAR		*buffer)
{
/**************************************
 *
 *	E X E _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Move a message from JRD to the host program.  This corresponds to
 *	a JRD BLR/NOD send.
 *
 **************************************/
NOD	message;
FMT	format;
TRA     transaction;
SAV	save_sav_point;
JMP_BUF env, *old_env;

SET_TDBB (tdbb);

DEV_BLKCHK (request, type_req);

transaction = request->req_transaction;

if (!(request->req_flags & req_active))
    ERR_post (gds__req_sync, 0);

if (request->req_flags & req_proc_fetch)
    {
    /* request->req_proc_sav_point stores all the request savepoints.
       When going to continue execution put request save point list
       into transaction->tra_save_point so that it is used in looper.
       When we come back to EXE_receive() restore
       transaction->tra_save_point and merge all work done under
       stored procedure savepoints into the current transaction
       savepoint, which is the savepoint for fetch. */

    save_sav_point = transaction->tra_save_point;
    transaction->tra_save_point = request->req_proc_sav_point;
    request->req_proc_sav_point = save_sav_point;
    if (!transaction->tra_save_point)
	VIO_start_save_point (tdbb, transaction);
    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;

    if (SETJMP (env))
	{
	tdbb->tdbb_setjmp = (UCHAR*) old_env;
	save_sav_point = transaction->tra_save_point;
	transaction->tra_save_point = request->req_proc_sav_point;
	request->req_proc_sav_point = save_sav_point;
	release_proc_save_points (request);
	LONGJMP (old_env, -1);
	}
    }

if (request->req_message->nod_type == nod_stall
#ifdef SCROLLABLE_CURSORS
|| request->req_flags & req_fetch_required
#endif
)
    execute_looper (tdbb, request, transaction, req_sync);

if (!(request->req_flags & req_active) ||
    request->req_operation != req_send)
    ERR_post (gds__req_sync, 0);

message = request->req_message;
format = (FMT) message->nod_arg [e_msg_format];

if (msg != (USHORT) message->nod_arg [e_msg_number])
    ERR_post (gds__req_sync, 0);

if (length != format->fmt_length)
    ERR_post (gds__port_len, 
	gds_arg_number, (SLONG) length, 
	gds_arg_number, (SLONG) format->fmt_length, 0);

if ((U_IPTR) buffer & (ALIGNMENT - 1))
    MOVE_FAST ((SCHAR*) request + message->nod_impure, buffer, length);
else
    MOVE_FASTER ((SCHAR*) request + message->nod_impure, buffer, length);

execute_looper (tdbb, request, transaction, req_proceed);

if (request->req_flags & req_proc_fetch)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    save_sav_point = transaction->tra_save_point;
    transaction->tra_save_point = request->req_proc_sav_point;
    request->req_proc_sav_point = save_sav_point;	
    VIO_merge_proc_sav_points (tdbb, transaction, &request->req_proc_sav_point);
    }
}

#ifdef SCROLLABLE_CURSORS
void EXE_seek (
    TDBB	tdbb,
    REQ		request, 
    USHORT	direction, 
    ULONG	offset)
{
/**************************************
 *
 *      E X E _ s e e k
 *
 **************************************
 *
 * Functional description
 *	Seek a given request in a particular direction 
 *	for offset records. 
 *
 **************************************/
VEC		vector;
RSB		rsb;
SLONG		i;

SET_TDBB (tdbb);
DEV_BLKCHK (request, type_req);

/* loop through all RSEs in the request, 
   and describe the rsb tree for that rsb;
   go backwards because items were popped
   off the stack backwards */

vector = request->req_fors;
if (!vector)
    return FALSE;

/* find the top-level rsb in the request and seek it */

for (i = vector->vec_count - 1; i >= 0; i--)
    if (rsb = (RSB) vector->vec_object [i])
        {
	seek_rsb (tdbb, request, rsb, direction, offset);
	break;
	}
}
#endif

void EXE_send (
    TDBB	tdbb,
    REQ		request,
    USHORT	msg,
    USHORT	length,
    UCHAR	*buffer)
{
/**************************************
 *
 *	E X E _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Send a message from the host program to the engine.  
 *	This corresponds to a blr_receive or blr_select statement.
 *
 **************************************/
NOD	node, message, *ptr, *end;
FMT	format;
TRA     transaction;
#ifdef SCROLLABLE_CURSORS
USHORT	save_operation;
NOD 	save_next = NULL, save_message;
#endif

SET_TDBB (tdbb);
DEV_BLKCHK (request, type_req);

if (!(request->req_flags & req_active))
    ERR_post (gds__req_sync, 0);

#ifdef SCROLLABLE_CURSORS
/* look for an asynchronous send message--if such 
   a message was defined, we allow the user to send 
   us a message at any time during request execution */

if ((message = request->req_async_message) &&
    (node = message->nod_arg [e_send_message]) &&
    (msg == (USHORT) node->nod_arg [e_msg_number]))
    {
    /* save the current state of the request so we can go 
       back to what was interrupted */

    save_operation = request->req_operation;
    save_message = request->req_message;
    save_next = request->req_next;

    request->req_operation = req_receive;
    request->req_message = node;
    request->req_next = message->nod_arg [e_send_statement];

    /* indicate that we are processing an asynchronous message */

    request->req_flags |= req_async_processing;
    }
else
    {
#endif
    if (request->req_operation != req_receive)
        ERR_post (gds__req_sync, 0);
    node = request->req_message;
#ifdef SCROLLABLE_CURSORS
    }
#endif

transaction = request->req_transaction;

if (node->nod_type == nod_message)
    message = node;
else if (node->nod_type == nod_select)
    for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	{
	message = (*ptr)->nod_arg [e_send_message];
	if ((USHORT) message->nod_arg [e_msg_number] == msg)
	    {
	    request->req_next = *ptr;
	    break;
	    }
	}
else
    BUGCHECK (167); /* msg 167 invalid SEND request */

format = (FMT) message->nod_arg [e_msg_format];

if (msg != (USHORT) message->nod_arg [e_msg_number])
    ERR_post (gds__req_sync, 0);

if (length != format->fmt_length)
    ERR_post (gds__port_len, 
	gds_arg_number, (SLONG) length, 
	gds_arg_number, (SLONG) format->fmt_length, 0);

if ((U_IPTR) buffer & (ALIGNMENT - 1))
    MOVE_FAST (buffer, (SCHAR*) request + message->nod_impure, length);
else
    MOVE_FASTER (buffer, (SCHAR*) request + message->nod_impure, length);

execute_looper (tdbb, request, transaction, req_proceed);

#ifdef SCROLLABLE_CURSORS
if (save_next)
    {
    /* if the message was sent asynchronously, restore all the 
       previous values so that whatever we were trying to do when 
       the message came in is what we do next */

    request->req_operation = save_operation;
    request->req_message = save_message;
    request->req_next = save_next;
    }    
#endif
}

#ifdef GATEWAY
void EXE_set_fields_null (
    TDBB	tdbb,
    REC		record,
    FMT		format)
{
/**************************************
 *
 *	E X E _ s e t _ f i e l d s _ n u l l
 *
 **************************************
 *
 * Functional description
 *	Initialize all fields in a record to missing.
 *
 **************************************/
USHORT	count;

SET_TDBB (tdbb);
DEV_BLKCHK (record, type_rec);
DEV_BLKCHK (format, type_fmt);

if ((count = format->fmt_count) <= 1)
    ((SSHORT*) record->rec_data) [0] = -1;
else
    {
    ((SSHORT*) record->rec_data) [0] = ((SSHORT*) record->rec_data) [1] = -1;
    MOVE_FAST (record->rec_data, record->rec_data + (2 * sizeof (SSHORT)), (USHORT) ((count - 2) * sizeof (SSHORT)));
    }
}
#endif

void EXE_start (
    TDBB	tdbb,
    REQ		request,
    TRA		transaction)
{
/**************************************
 *
 *	E X E _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start an execution running.
 *
 **************************************/
DBB	dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

BLKCHK (request, type_req);
BLKCHK (transaction, type_tra);

if (request->req_flags & req_active)
    ERR_post (gds__req_sync, gds_arg_gds, gds__reqinuse, 0);

if (transaction->tra_flags & TRA_prepared)
    ERR_post (gds__req_no_trans, 0);

/* Post resources to transaction block.  In particular, the interest locks
   on relations/indices are copied to the transaction, which is very
   important for (short-lived) dynamically compiled requests.  This will
   provide transaction stability by preventing a relation from being
   dropped after it has been referenced from an active transaction. */

TRA_post_resources (tdbb, transaction, request->req_resources);

request->req_transaction = transaction;
request->req_flags &= REQ_FLAGS_INIT_MASK;
request->req_flags |= req_active;
request->req_flags &= ~req_reserved;
request->req_operation = req_evaluate;

/* set up to count records affected by request */

request->req_flags |= req_count_records;
request->req_records_selected = 0;
request->req_records_updated = 0;
request->req_records_inserted = 0;
request->req_records_deleted = 0;

/* Store request start time for timestamp work */
if (!request->req_timestamp)
    request->req_timestamp = time (NULL);

if (request->req_invariants)
    {
    /* Set all invariants to not computed. */

    NOD		*ptr, *end;
    USHORT	*invariant_flags;
    VLU		impure;

    ptr = (NOD*) request->req_invariants->vec_object;
    end = ptr + request->req_invariants->vec_count;
    for (; ptr < end; ptr++)
	if (*ptr)
	    {
	    impure = (VLU) ((SCHAR*) request + (*ptr)->nod_impure);
	    invariant_flags = (USHORT*) &impure->vlu_string;
	    *invariant_flags = 0;
	    }
    }

/* Start a save point if not in middle of one  */

if (transaction && (transaction != dbb->dbb_sys_trans))
    {
    VIO_start_save_point (tdbb, transaction);
    }
#ifdef WIN_NT
START_CHECK_FOR_EXCEPTIONS(NULL);
#endif
      looper (tdbb, request, request->req_top_node);
#ifdef WIN_NT
END_CHECK_FOR_EXCEPTIONS(NULL);
#endif

/* If any requested modify/delete/insert ops have completed, forget them */

if (transaction && (transaction != dbb->dbb_sys_trans) &&
    transaction->tra_save_point &&
    !transaction->tra_save_point->sav_verb_count)
    {
    /* Forget about any undo for this verb */

    VIO_verb_cleanup (tdbb, transaction);
    }
}

void EXE_unwind (
    TDBB	tdbb,
    REQ	request)
{
/**************************************
 *
 *	E X E _ u n w i n d
 *
 **************************************
 *
 * Functional description
 *	Unwind a request, maybe active, maybe not.  This is particlarly
 *	simple since nothing really needs to be done.
 *
 **************************************/
BLK		*ptr, *end;
struct plb	*old_pool;
REQ		old_request;

DEV_BLKCHK (request, type_req);

SET_TDBB (tdbb);

if (request->req_flags & req_active)
    {
    if (request->req_fors)
	{
	old_pool = tdbb->tdbb_default;
    	tdbb->tdbb_default = request->req_pool;
    	old_request = tdbb->tdbb_request;
    	tdbb->tdbb_request = request;
    	tdbb->tdbb_transaction = request->req_transaction;
    	for (ptr = request->req_fors->vec_object, end = ptr + request->req_fors->vec_count; ptr < end; ptr++)
	    if (*ptr)
	    	RSE_close (tdbb, *ptr);
    	tdbb->tdbb_default = old_pool;
    	tdbb->tdbb_request = old_request;
	}
    release_blobs (tdbb, request);
    }

if (request->req_proc_sav_point && (request->req_flags & req_proc_fetch))
    release_proc_save_points (request); 
    
request->req_flags &= ~(req_active | req_proc_fetch | req_reserved);
request->req_flags |= req_abort | req_stall;
request->req_timestamp = 0;

#ifdef GATEWAY
/* Unwind request from connected DBMS's point of view */

FRGN_unwind (request);
#endif
}


static NOD erase (
    TDBB	tdbb,
    NOD		node,
    SSHORT	which_trig)
{
/**************************************
 *
 *	e r a s e
 *
 **************************************
 *
 * Functional description
 *	Perform erase operation.
 *
 **************************************/
DBB	dbb;
REQ	request, trigger;
RPB	*rpb;
REL	relation;
REC	record;
FMT	format;
TRA	transaction;
#ifdef PC_ENGINE
LCK	implicit_lock = NULL, record_locking;
RSB	rsb = NULL;
JMP_BUF	env, *old_env;
IRSB    impure;
#endif

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;
BLKCHK (node, type_nod);

request = tdbb->tdbb_request;
transaction = request->req_transaction;
rpb = &request->req_rpb [(int) node->nod_arg [e_erase_stream]];
relation = rpb->rpb_relation;

#ifdef PC_ENGINE
/* for navigational streams, retrieve the rsb */

if (node->nod_arg [e_erase_rsb])
    {
    rsb = *(RSB*) node->nod_arg [e_erase_rsb];
    impure = (IRSB) ((UCHAR*) request + rsb->rsb_impure);
    }
#endif

switch (request->req_operation)
    {
    case req_evaluate:
	if (!node->nod_arg [e_erase_statement])
	    break;
	format = MET_current (tdbb, rpb->rpb_relation);
	record = VIO_record (tdbb, rpb, format, tdbb->tdbb_default);
	rpb->rpb_address = record->rec_data;
	rpb->rpb_length = format->fmt_length;
	rpb->rpb_format_number = format->fmt_version;
	return node->nod_arg [e_erase_statement];

    case req_return:
	break;

    default:
	return node->nod_parent;
    }	

#ifdef PC_ENGINE
/* if we are on a crack in a navigational stream, erase 
   is not a valid operation */

if (rsb && EXE_crack (tdbb, rsb, irsb_bof | irsb_eof | irsb_crack))
    {
    EXE_mark_crack (tdbb, rsb, impure->irsb_flags);
    request->req_operation = req_return;
    return node->nod_parent;
    }
#endif
	
request->req_operation = req_return;
RLCK_reserve_relation (tdbb, transaction, relation, TRUE, TRUE);

/* If the stream was sorted, the various fields in the rpb are
   probably junk.  Just to make sure that everything is cool,
   refetch and release the record. */

if (rpb->rpb_stream_flags & RPB_s_refetch)
    {
    SLONG	tid_fetch;

    tid_fetch = rpb->rpb_transaction;
    if ((!DPM_get (tdbb, rpb, LCK_read)) ||
	(!VIO_chase_record_version (tdbb, rpb, NULL, transaction, tdbb->tdbb_default)))
	ERR_post (gds__deadlock, gds_arg_gds, gds__update_conflict, 0);
    VIO_data (tdbb, rpb, tdbb->tdbb_request->req_pool);

    /* If record is present, and the transaction is read committed,
     * make sure the record has not been updated.  Also, punt after
     * VIO_data () call which will release the page.
     */

    if ((transaction->tra_flags & TRA_read_committed) &&
	(tid_fetch != rpb->rpb_transaction)) 
	ERR_post (gds__deadlock, gds_arg_gds, gds__update_conflict, 0);

    rpb->rpb_stream_flags &= ~RPB_s_refetch;
    }

#ifndef GATEWAY
     
#ifdef PC_ENGINE
/* set up to do record locking; in case of a consistency
   mode transaction, we already have an exclusive lock on
   the table, so don't bother */

if (!(transaction->tra_flags & TRA_degree3))
    {
    /* check whether record locking is turned on */

    record_locking = RLCK_record_locking (relation);
    if (record_locking->lck_physical != LCK_PR)
	{
	/* get an implicit lock on the record */

	implicit_lock = implicit_record_lock (transaction, rpb);

	/* set up to catch any errors so that we can 
	   release the implicit lock */

	old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
	tdbb->tdbb_setjmp = (UCHAR*) env;
	if (SETJMP (env))
  	    {
	    tdbb->tdbb_setjmp = (UCHAR*) old_env;
	    RLCK_unlock_record_implicit (implicit_lock, NULL_PTR);
	    LONGJMP (old_env, -1);
	    }
	}
    }
#endif

if (transaction != dbb->dbb_sys_trans)
    ++transaction->tra_save_point->sav_verb_count;

/* Handle pre-operation trigger */

if (relation->rel_pre_erase &&
    which_trig != POST_TRIG &&
    (trigger = execute_triggers (tdbb, &relation->rel_pre_erase, rpb->rpb_record, NULL_PTR)))
    trigger_failure (tdbb, trigger);

if (relation->rel_file)
    EXT_erase (rpb, transaction);
else if (!relation->rel_view_rse)
    VIO_erase (tdbb, rpb, transaction);

/* Handle post operation trigger */

if (relation->rel_post_erase &&
    which_trig != PRE_TRIG &&
    (trigger = execute_triggers (tdbb, &relation->rel_post_erase,
				 rpb->rpb_record, NULL_PTR)))
    trigger_failure (tdbb, trigger);

/* call IDX_erase (which checks constraints) after all post erase triggers 
   have fired. This is required for cascading referential integrity, which 
   can be implemented as post_erase triggers */

if (!relation->rel_file & !relation->rel_view_rse)
    {
    REL		bad_relation;
    USHORT	bad_index;
    IDX_E	error_code;

    if (error_code = IDX_erase (tdbb, rpb, transaction, &bad_relation, &bad_index))
        ERR_duplicate_error (error_code, bad_relation, bad_index);
    }

request->req_records_deleted++;

if (transaction != dbb->dbb_sys_trans)
    --transaction->tra_save_point->sav_verb_count;

#ifdef PC_ENGINE
if (implicit_lock)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    RLCK_unlock_record_implicit (implicit_lock, NULL_PTR);
    }

/* if the stream is navigational, it is now positioned on a crack */

if (rsb)
    RSE_MARK_CRACK (tdbb, rsb, irsb_crack);
#endif
#else
VIO_erase (rpb, transaction, node->nod_arg [e_erase_sql]);
#endif

return node->nod_parent;
}

static void execute_looper (
    TDBB	tdbb,
    REQ		request,
    TRA 	transaction,
    ENUM req_s	next_state)
{
/**************************************
 *
 *	e x e c u t e _ l o o p e r
 *
 **************************************
 *
 * Functional description
 *	Wrapper around looper. This will execute
 *	looper with the save point mechanism.
 *
 **************************************/
DBB		dbb;

DEV_BLKCHK (request, type_req);

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

/* Start a save point */

if (!(request->req_flags & req_proc_fetch) && request->req_transaction)
    if (transaction && (transaction != dbb->dbb_sys_trans))
	VIO_start_save_point (tdbb, transaction);

request->req_flags &= ~req_stall;
request->req_operation = next_state;
looper (tdbb, request, request->req_next);

/* If any requested modify/delete/insert ops have completed, forget them */

if (!(request->req_flags & req_proc_fetch) && request->req_transaction)
    if  (transaction && (transaction != dbb->dbb_sys_trans) &&
    	transaction->tra_save_point  &&
    	!transaction->tra_save_point->sav_verb_count)
	{
    	/* Forget about any undo for this verb */
	
    	VIO_verb_cleanup (tdbb, transaction);
    	}
}

static void execute_procedure (
    TDBB	tdbb,
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Execute a stored procedure.  Begin by
 *	assigning the input parameters.  End
 *	by assigning the output parameters.
 *
 **************************************/
REQ	request, proc_request;
NOD	in_message, out_message, temp;
FMT	format;
USHORT	in_msg_length, out_msg_length;
SCHAR	*in_msg, *out_msg;
PRC	procedure;
STR	temp_buffer = NULL;
SLONG	save_point_number;
TRA	transaction;
JMP_BUF	env, *old_env;
struct plb	*old_pool;

SET_TDBB (tdbb);
BLKCHK (node, type_nod);

request = tdbb->tdbb_request;

if (temp = node->nod_arg [e_esp_inputs])
    {
    NOD	*ptr, *end;

    for (ptr = temp->nod_arg, end = ptr + temp->nod_count; ptr < end; ptr++)
	EXE_assignment (tdbb, *ptr);
    }

if (in_message = node->nod_arg [e_esp_in_msg])
    {
    format = (FMT) in_message->nod_arg [e_msg_format];
    in_msg_length = format->fmt_length;
    in_msg = (SCHAR*) request + in_message->nod_impure;
    }
if (out_message = node->nod_arg [e_esp_out_msg])
    {
    format = (FMT) out_message->nod_arg [e_msg_format];
    out_msg_length = format->fmt_length;
    out_msg = (SCHAR*) request + out_message->nod_impure;
    }

procedure = (PRC) node->nod_arg [e_esp_procedure];
proc_request = EXE_find_request (tdbb, procedure->prc_request, FALSE);

if (!out_message)
    {
    format = (FMT) procedure->prc_output_msg->nod_arg [e_msg_format];
    out_msg_length = format->fmt_length;
    temp_buffer = (STR) ALLOCDV (type_str, out_msg_length + DOUBLE_ALIGN - 1);
    out_msg = (SCHAR*) ALIGN ((U_IPTR) temp_buffer->str_data, DOUBLE_ALIGN);
    }

/* Save the old pool */

old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = proc_request->req_pool;

/* Catch errors so we can unwind cleanly */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    tdbb->tdbb_default = old_pool;
    tdbb->tdbb_request = request;
    EXE_unwind (tdbb, proc_request);
    proc_request->req_flags &= ~(req_in_use | req_proc_fetch);
    proc_request->req_timestamp = 0;
    if (temp_buffer)
	ALL_release (temp_buffer);
    LONGJMP (old_env, -1);
    }

transaction = request->req_transaction;
save_point_number = transaction->tra_save_point->sav_number;

proc_request->req_timestamp = request->req_timestamp;
EXE_start (tdbb, proc_request, transaction);
if (in_message)
    EXE_send (tdbb, proc_request, 0, in_msg_length, in_msg);

EXE_receive (tdbb, proc_request, 1, out_msg_length, out_msg);

/* Clean up all savepoints started during execution of the
   procedure */

if (transaction != tdbb->tdbb_database->dbb_sys_trans)
    {
    SAV	save_point;

    for (save_point = transaction->tra_save_point; 
	 save_point && save_point_number < save_point->sav_number;
	 save_point = transaction->tra_save_point)
	VIO_verb_cleanup (tdbb, transaction);
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
tdbb->tdbb_default = old_pool;
EXE_unwind (tdbb, proc_request);
tdbb->tdbb_request = request;
if (temp = node->nod_arg [e_esp_outputs])
    {
    NOD	*ptr, *end;

    for (ptr = temp->nod_arg, end = ptr + temp->nod_count; ptr < end; ptr++)
	EXE_assignment (tdbb, *ptr);
    }
if (temp_buffer)
    ALL_release (temp_buffer);
proc_request->req_attachment = NULL;
proc_request->req_flags &= ~(req_in_use | req_proc_fetch);
proc_request->req_timestamp = 0;
}

#ifndef GATEWAY
static REQ execute_triggers (
    TDBB	tdbb,
    VEC		*triggers,
    REC		old_rec,
    REC		new_rec)
{
/**************************************
 *
 *	e x e c u t e _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Execute group of triggers.  Return pointer to failing trigger
 *	if any blow up.
 *
 **************************************/
REQ	*ptr, *end, result;
VOLATILE REQ     trigger = NULL;
TRA	transaction;
VOLATILE VEC	vector;
JMP_BUF env, *old_env;

DEV_BLKCHK (*triggers, type_vec);
DEV_BLKCHK (old_rec, type_rec);
DEV_BLKCHK (new_rec, type_rec);

if (!*triggers)
    return NULL;

SET_TDBB (tdbb);

transaction = tdbb->tdbb_request->req_transaction;
vector = *triggers;
result = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (vector != *triggers)
	release_triggers (tdbb, vector);
    if (!trigger)
	LONGJMP (old_env, -1);
    else
	return trigger;
    }

for (ptr = (REQ*) vector->vec_object, end = ptr + vector->vec_count;
     ptr < end; ptr++)
    {
    trigger = EXE_find_request (tdbb, *ptr, FALSE);
    trigger->req_rpb [0].rpb_record = old_rec;
    trigger->req_rpb [1].rpb_record = new_rec;
    trigger->req_timestamp = tdbb->tdbb_request->req_timestamp;
    EXE_start (tdbb, trigger, transaction);
    trigger->req_attachment = NULL;
    trigger->req_flags &= ~req_in_use;
    trigger->req_timestamp = 0;
    if (trigger->req_operation == req_unwind)
	{
	result = trigger;
	break;
	}
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (vector != *triggers)
    release_triggers (tdbb, vector);

return result;
}
#endif

#ifndef GATEWAY
#ifdef PC_ENGINE
static NOD find (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	f i n d
 *
 **************************************
 *
 * Functional description
 *	Find the given key value in a stream.
 *	Assume that the stream is open.
 *
 **************************************/
register REQ	request;
RSB		rsb;
USHORT		operator, direction;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

if (request->req_operation == req_evaluate)
    {
    rsb = *((RSB*) node->nod_arg [e_find_rsb]);   

    operator = (USHORT) MOV_get_long (EVL_expr (tdbb,
						node->nod_arg[e_find_operator]),
				      0);
    if (operator != blr_eql && 
	operator != blr_leq && operator != blr_lss &&
	operator != blr_geq && operator != blr_gtr)
	ERR_post (gds__invalid_operator, 0);

    direction = (USHORT) MOV_get_long(EVL_expr(tdbb,
					       node->nod_arg[e_find_direction]),
				      0);
    if (direction != blr_backward          && direction != blr_forward &&
        direction != blr_backward_starting && direction != blr_forward_starting)
	ERR_post (gds__invalid_direction, 0);

    /* try to find the record; the position is defined to be on a crack 
       regardless of whether we are at BOF or EOF; also be sure to perpetuate
       the forced crack (bug #7024) */

    if (!(RSE_find_record (tdbb, rsb, operator, direction, node->nod_arg [e_find_args])))
        if (EXE_crack (tdbb, rsb, irsb_bof | irsb_eof | irsb_crack))
            if (EXE_crack (tdbb, rsb, irsb_forced_crack))
                EXE_mark_crack (tdbb, rsb, irsb_crack | irsb_forced_crack);
            else if (EXE_crack (tdbb, rsb, irsb_bof))
                EXE_mark_crack (tdbb, rsb, irsb_bof);
            else if (EXE_crack (tdbb, rsb, irsb_eof))
                EXE_mark_crack (tdbb, rsb, irsb_eof);
	    else
                EXE_mark_crack (tdbb, rsb, irsb_crack);

    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif
#endif

#ifndef GATEWAY
#ifdef PC_ENGINE
static NOD find_dbkey (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	f i n d _ d b k e y
 *
 **************************************
 *
 * Functional description
 *	Find the given dbkey in a navigational stream,
 *	resetting the position of the stream to that record.
 *
 **************************************/
register REQ	request;
RSB		rsb;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

if (request->req_operation == req_evaluate)
    {
    rsb = *((RSB*) node->nod_arg [e_find_dbkey_rsb]);

    if (!(RSE_find_dbkey (tdbb, rsb, node->nod_arg [e_find_dbkey_dbkey], node->nod_arg [e_find_dbkey_version])))
        EXE_mark_crack (tdbb, rsb, irsb_crack); 

    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif
#endif


#ifdef PC_ENGINE
static LCK implicit_record_lock (
    TRA		transaction,
    RPB		*rpb)
{
/**************************************
 *
 *	i m p l i c i t _ r e c o r d _ l o c k
 *
 **************************************
 *
 * Functional description
 *	An update to a record is being attempted and
 *	record locking has been initiated.  Take out
 *	an implicit record lock to prevent updating
 *	a record that someone has explicitly locked.
 *
 **************************************/
LCK	lock, record_locking;
REL	relation;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;

DEV_BLKCHK (transaction, type_tra);

relation = rpb->rpb_relation;
record_locking = relation->rel_record_locking;

/* occasionally we should check whether we really still need to 
   do record locking; this is defined as RECORD_LOCK_CHECK_INTERVAL--
   if we can get a PR on the record locking lock there is no need
   to do implicit locking anymore */

if ((record_locking->lck_physical == LCK_none) &&
    !(relation->rel_lock_total % RECORD_LOCK_CHECK_INTERVAL) &&
    LCK_lock_non_blocking (tdbb, record_locking, LCK_PR, FALSE))
    return NULL;

if (!(lock = RLCK_lock_record_implicit (transaction, rpb, LCK_SW, NULL_PTR, NULL_PTR)))
    ERR_post (gds__record_lock, 0);

return lock;
}
#endif

static NOD looper (
    TDBB	tdbb,
    REQ		request,
    NOD		in_node)
{
/**************************************
 *
 *	l o o p e r
 *
 **************************************
 *
 * Functional description
 *	Cycle thru request execution tree.  Return next node for
 *	execution on stall or request complete.
 *
 **************************************/
DBB	dbb;
STA	impure;
SSHORT	error_pending;
SLONG	save_point_number;
SSHORT	which_erase_trig = 0, which_sto_trig = 0, which_mod_trig = 0;
REQ	old_request;
VOLATILE NOD	node, top_node = NULL, prev_node;
TRA	transaction;
JMP_BUF	env, *old_env;
struct plb *old_pool;
#if defined(DEBUG_GDS_ALLOC) && FALSE
int	node_type;
#endif


/* If an error happens during the backout of a savepoint, then the transaction
   must be marked 'dead' because that is the only way to clean up after a
   failed backout.  The easiest way to do this is to kill the application
   by calling bugcheck.
   To facilitate catching errors during VIO_verb_cleanup, the following
   define is used. */
#define VERB_CLEANUP							\
	{								\
	JMP_BUF env2, *old_env2;					\
	old_env2 = (JMP_BUF*) tdbb->tdbb_setjmp;			\
	tdbb->tdbb_setjmp = (UCHAR*) env2;				\
	if (!SETJMP (env2))						\
	    {								\
	    VIO_verb_cleanup (tdbb, transaction);				\
	    tdbb->tdbb_setjmp = (UCHAR*) old_env2; /* restore old env */\
	    }								\
	else								\
	    {								\
	    /* Cause bugcheck to longjmp out of looper. */		\
	    tdbb->tdbb_setjmp = (UCHAR*) old_env; 			\
	    if (dbb->dbb_flags & DBB_bugcheck)				\
		LONGJMP (old_env, (int) tdbb->tdbb_status_vector [1]);	\
	    else							\
	    	BUGCHECK (290); /* msg 290 error during savepoint backout */\
	    }								\
	}

#ifdef WINDOWS_ONLY
// try and avoid a stack overflow
if (_SP < 0x1000)
      ERR_post (gds__req_depth_exceeded,
              gds_arg_string, "looper stack about to overflow",
              0 );
#endif


if (!(transaction = request->req_transaction))
    ERR_post (gds__req_no_trans, 0);

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
BLKCHK (in_node, type_nod);

/* Save the old pool and request to restore on exit */

old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = request->req_pool;
old_request = tdbb->tdbb_request;
tdbb->tdbb_request = request;
tdbb->tdbb_transaction = transaction;
save_point_number = (transaction->tra_save_point) ?
		transaction->tra_save_point->sav_number : 0;

node = in_node;

/* Catch errors so we can unwind cleanly */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
error_pending = FALSE;

if (SETJMP (env))
    {
    /* If the database is already bug-checked, then get out. */
    if (dbb->dbb_flags & DBB_bugcheck)
	LONGJMP (old_env, (int) tdbb->tdbb_status_vector [1]);

#ifdef GATEWAY
    /* Unwind request from connected DBMS's point of view.
       Set up special error handling in case something
       happens there. */

    if (!SETJMP (env))
	FRGN_unwind (request);

    /* Continue with normal JRD unwind */
#endif

    /* Since an error happened, the current savepoint needs to be undone. */
    if (transaction != dbb->dbb_sys_trans)
	{
	++transaction->tra_save_point->sav_verb_count;
	VERB_CLEANUP;
	}

    error_pending = TRUE;
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    request->req_operation = req_unwind;
    request->req_label = 0;
    }

/* Execute stuff until we drop */

while (node && !(request->req_flags & req_stall))
    {
#ifdef MULTI_THREAD

    if (request->req_operation == req_evaluate &&
	(--tdbb->tdbb_quantum < 0) &&
	!tdbb->tdbb_inhibit)
	(void) JRD_reschedule (tdbb, 0, TRUE);

#endif
#if defined(DEBUG_GDS_ALLOC) && FALSE
    node_type = node->nod_type;
#endif
    switch (node->nod_type)
	{
	case nod_asn_list:
	    if (request->req_operation == req_evaluate)
		{
		VOLATILE NOD	*ptr, *end;

		for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
		    EXE_assignment (tdbb, *ptr);
		request->req_operation = req_return; 
		}
	    node = node->nod_parent;
	    break;

	case nod_assignment:
	    if (request->req_operation == req_evaluate)
		EXE_assignment (tdbb, node);
	    node = node->nod_parent;
	    break;

	case nod_dcl_variable:
	    {
	    VLU	variable;

	    variable = (VLU) ((SCHAR*) request + node->nod_impure);
	    variable->vlu_desc = *(DSC*) (node->nod_arg + e_dcl_desc);
	    variable->vlu_desc.dsc_flags = 0;
	    variable->vlu_desc.dsc_address = (UCHAR*) &variable->vlu_misc;
	    if (variable->vlu_desc.dsc_dtype <= dtype_varying &&
		!variable->vlu_string)
		{
		variable->vlu_string = (STR) ALLOCDV (type_str, variable->vlu_desc.dsc_length);
		variable->vlu_string->str_length = variable->vlu_desc.dsc_length;
		variable->vlu_desc.dsc_address = variable->vlu_string->str_data;
		}
	    request->req_operation = req_return;
	    node = node->nod_parent;
	    }
	    break;

	case nod_erase:
	    if ((request->req_operation == req_return) &&
		(node->nod_arg[e_erase_sub_erase]))
		{
		if (!top_node)
		    {
		    top_node = node;
		    which_erase_trig = PRE_TRIG;
		    }
		prev_node = node;
		node = erase (tdbb, node, which_erase_trig);
		if (which_erase_trig == PRE_TRIG)
		    {
		    node = prev_node->nod_arg[e_erase_sub_erase];
		    node->nod_parent = prev_node;
		    }
		if (top_node == prev_node && which_erase_trig == POST_TRIG)
		    {
		    top_node = NULL;
		    which_erase_trig = ALL_TRIGS;
		    }
		else
		    request->req_operation = req_evaluate;
		}
	    else
		{
		prev_node = node;
		node = erase (tdbb, node, ALL_TRIGS);
		if (!(prev_node->nod_arg[e_erase_sub_erase]) &&
		    which_erase_trig == PRE_TRIG)
		    which_erase_trig = POST_TRIG;
		}
	    break;

	case nod_exec_proc:
	    if (request->req_operation == req_unwind)
		{
		node = node->nod_parent;
		break;
		}
	    execute_procedure (tdbb, node);
	    node = node->nod_parent;
	    request->req_operation = req_return;
	    break;

	case nod_for:
	    switch (request->req_operation)
		{
		case req_evaluate:
		    RSE_open (tdbb, (RSB) node->nod_arg [e_for_rsb]);
		case req_return:
		    if (node->nod_arg [e_for_stall])
			{
			node = node->nod_arg [e_for_stall];
			break;
			}
		case req_sync:
		    if (RSE_get_record (tdbb, (RSB) node->nod_arg [e_for_rsb], 
#ifdef SCROLLABLE_CURSORS
RSE_get_next))
#else
RSE_get_forward))
#endif
			{
			node = node->nod_arg [e_for_statement];
			request->req_operation = req_evaluate;
			break;
			}
		    request->req_operation = req_return;
		default:
		    RSE_close (tdbb, (RSB) node->nod_arg [e_for_rsb]);
		    node = node->nod_parent;
		}
	    break;

	case nod_abort:
	    switch (request->req_operation)
		{
		case req_evaluate:
		    set_error (tdbb, node->nod_arg [0]);

		default:
		    node = node->nod_parent;
		}
	    break;

	case nod_start_savepoint:
	    switch (request->req_operation)
		{
		case req_evaluate:
		    /* Start a save point */

		    if (transaction != dbb->dbb_sys_trans)
			VIO_start_save_point (tdbb, transaction);

		default:
		    node = node->nod_parent;
		    request->req_operation = req_return;
		}
	    break;

	case nod_end_savepoint:
	    switch (request->req_operation)
		{
		case req_evaluate:
		case req_unwind:
		    /* If any requested modify/delete/insert
		       ops have completed, forget them */
		    if (transaction != dbb->dbb_sys_trans)
			{
			/* If an error is still pending when the savepoint is 
			   supposed to end, then the application didn't handle the
			   error and the savepoint should be undone. */
			if (error_pending)
			    ++transaction->tra_save_point->sav_verb_count;
			VERB_CLEANUP;
			}

		default:
		    node = node->nod_parent;
		    request->req_operation = req_return;
		}
	    break;

	case nod_handler:
	    switch (request->req_operation)
		{
		case req_evaluate:
		    node = node->nod_arg [0];
		    break;

		case req_unwind:
		    if (!request->req_label)
			request->req_operation = req_return;

		default:
		    node = node->nod_parent;
		}
	    break;

	case nod_block:
	    switch (request->req_operation)
		{
		SLONG	count;
		SAV	save_point;

		case req_evaluate:
		    if (transaction != dbb->dbb_sys_trans)
			{
			VIO_start_save_point (tdbb, transaction);
			save_point = transaction->tra_save_point;
			count = save_point->sav_number;
			MOVE_FAST (&count,
			    (SCHAR*) request + node->nod_impure, sizeof (SLONG));
			}
		    node = node->nod_arg [e_blk_action];
		    break;

		case req_unwind:
		    {
		    VOLATILE NOD	handlers, *ptr, *end;

		    if (request->req_flags & req_leave)
			{
			/** Although the req_operation is set to req_unwind
			    it is not an error case if req_leave bit is set.
			    req_leave bit indicates that we hit an EXIT 
			    statement in the procedure code.
			    Do not perform the error handling stuff.
			**/
			node = node->nod_parent;
			break;
			}
		    if (transaction != dbb->dbb_sys_trans)
			{
			MOVE_FAST ((SCHAR*) request + node->nod_impure,
			    &count, sizeof (SLONG));
			/* Since there occurred an error (req_unwind), undo all savepoints
			   up to, but not including, the savepoint of this block.  The
			   savepoint of this block will be dealt with below. */
			for (save_point = transaction->tra_save_point;
			     save_point && count < save_point->sav_number;
			     save_point = transaction->tra_save_point)
			    {
			    ++transaction->tra_save_point->sav_verb_count;
			    VERB_CLEANUP;
			    }
			}
		    if (handlers = node->nod_arg [e_blk_handlers])
			{
			ULONG	prev_req_error_handler;

			node = node->nod_parent;
			for (ptr = handlers->nod_arg,
			     end = ptr + handlers->nod_count; ptr < end; ptr++)
			    if (test_error (tdbb, (*ptr)->nod_arg [e_err_conditions]))
				{
				request->req_operation = req_evaluate;
				node = (*ptr)->nod_arg [e_err_action];
				error_pending = FALSE;
				/* On entering looper old_request etc. are saved.
				   On recursive calling we will loose the actual old
				   request for that invocation of looper. Avoid this. */
				tdbb->tdbb_default = old_pool;
				tdbb->tdbb_request = old_request;

				/* Save the previous state of req_error_handler
				   bit. We need to restore it later. This is
				   necessary if the error handler is deeply 
				   nested. */

				prev_req_error_handler = 
				     request->req_flags & req_error_handler;
				request->req_flags |= req_error_handler;
				node = looper (tdbb, request, node);
				request->req_flags &= ~(req_error_handler);
				request->req_flags |= prev_req_error_handler;

				/* Note: Previously the above call
				    "node = looper (tdbb, request, node);"
				   never returned back till the node tree
				   was executed completely. Now that the looper
				   has changed its behaviour such that it
				   returns back after handling error. This 
				   makes it necessary that the jmpbuf be reset
				   so that looper can proceede with the 
				   preocessing of execution tree. If this is
				   not done then anymore errors will take the
				   engine out of looper there by abruptly
				   terminating the processing. */

				tdbb->tdbb_setjmp = (UCHAR*) env;
				tdbb->tdbb_default = request->req_pool;
				tdbb->tdbb_request = request;
				/* The error is dealt with by the application, cleanup
				   this block's savepoint. */
				if (transaction != dbb->dbb_sys_trans)
				    {
				    for (save_point = transaction->tra_save_point;
			     			save_point && count <= save_point->sav_number;
			     			save_point = transaction->tra_save_point)
			    		VERB_CLEANUP; 
				    }
				}		
			}
		    else
			node = node->nod_parent;

		    /* If the application didn't have an error handler, then
		       the error will still be pending.  Undo the block by
		       using its savepoint. */

		    if (error_pending && transaction != dbb->dbb_sys_trans)
			{
			++transaction->tra_save_point->sav_verb_count;
			VERB_CLEANUP;
			}
		    }
		    break;

		case req_return:
		    if (transaction != dbb->dbb_sys_trans)
			{
			MOVE_FAST ((SCHAR*) request + node->nod_impure,
				&count, sizeof (SLONG));
			for (save_point = transaction->tra_save_point;
				  save_point && count <= save_point->sav_number;
				  save_point = transaction->tra_save_point)
			    VERB_CLEANUP;
			}
		default:
		    node = node->nod_parent;
		}
	    break;

	case nod_error_handler:
	    if (request->req_flags & req_error_handler && !error_pending)
		{
		tdbb->tdbb_setjmp = (UCHAR*) old_env;
		return node;
		}
	    node = node->nod_parent;
	    node = node->nod_parent;
	    if (request->req_operation == req_unwind)
		node = node->nod_parent;
	    break;

	case nod_label:
	    switch (request->req_operation)
		{
		case req_evaluate:
		    node = node->nod_arg [e_lbl_statement];
		    break;

		case req_unwind:
		    if (request->req_label == (USHORT) node->nod_arg [e_lbl_label])
			{
			request->req_flags &= ~req_leave;
			request->req_operation = req_return;
			}

		default:
		    node = node->nod_parent;
		}
	    break;

	case nod_leave:
	    request->req_flags |= req_leave;
	    request->req_operation = req_unwind;
	    request->req_label = (USHORT) node->nod_arg [0];
	    node = node->nod_parent;
	    break;

	case nod_list:
	    impure = (STA) ((SCHAR*) request + node->nod_impure);
	    switch (request->req_operation)
		{
		case req_evaluate:
		    impure->sta_state = 0;
		case req_return:
		case req_sync:
		    if (impure->sta_state < node->nod_count)
			{
			request->req_operation = req_evaluate;
			node = node->nod_arg [impure->sta_state++];
			break;
			}
		    request->req_operation = req_return;
		default :
		    node = node->nod_parent;
		}
	    break;

	case nod_loop:
	    switch (request->req_operation)
		{
		case req_evaluate:
		case req_return:
		    node = node->nod_arg [0];
		    request->req_operation = req_evaluate;
		    break;

		default:
		    node = node->nod_parent;
		}
	    break;

	case nod_if:
	    if (request->req_operation == req_evaluate)
		if (EVL_boolean (tdbb, node->nod_arg [e_if_boolean]))
		    {
		    node = node->nod_arg [e_if_true];
		    break;
		    }
		else if (node->nod_arg [e_if_false])
		    {
		    node = node->nod_arg [e_if_false];
		    break;
		    }
		else
		    request->req_operation = req_return;
	    node = node->nod_parent;
	    break;

	case nod_modify:
	    impure = (STA) ((SCHAR*) request + node->nod_impure);
	    if ((request->req_operation == req_return) && 
		(!impure->sta_state) && 
		(node->nod_arg[e_mod_sub_mod]))
		{
		if (!top_node)
		    {
		    top_node = node;
		    which_mod_trig = PRE_TRIG;
		    }
		prev_node = node;
		node = modify (tdbb, node, which_mod_trig);
		if (which_mod_trig == PRE_TRIG)
		    {
		    node = prev_node->nod_arg[e_mod_sub_mod];
		    node->nod_parent = prev_node;
		    }
		if (top_node == prev_node && which_mod_trig == POST_TRIG)
		    {
		    top_node = NULL;
		    which_mod_trig = ALL_TRIGS;
		    }
		else
		    request->req_operation = req_evaluate;
		}
	    else
		{
		prev_node = node;
		node = modify (tdbb, node, ALL_TRIGS);
		if (!(prev_node->nod_arg[e_mod_sub_mod]) &&
		    which_mod_trig == PRE_TRIG)
		    which_mod_trig = POST_TRIG;
		}

	    break;

	case nod_nop:
	    request->req_operation = req_return;
	    node = node->nod_parent;
	    break;

	case nod_receive:
	    node = receive_msg (tdbb, node);
	    break;

	case nod_post:
	    DFW_post_work (transaction, dfw_post_event,
		EVL_expr (tdbb, node->nod_arg [0]), 0);

	    /* for an autocommit transaction, events can be posted
	     * without any updates */

	    if (transaction->tra_flags & TRA_autocommit)
		transaction->tra_flags |= TRA_perform_autocommit;
	case nod_message:
	    if (request->req_operation == req_evaluate)
		request->req_operation = req_return;
	    node = node->nod_parent;
	    break;

	case nod_stall:
	    node = stall (tdbb, node);
	    break;

	case nod_select:
	    node = selct (tdbb, node);
	    break;

	case nod_send:
	    node = send_msg (tdbb, node);
	    break;

	case nod_store:
	   impure = (STA) ((SCHAR*) request + node->nod_impure);
	   if ((request->req_operation == req_return) &&
		(!impure->sta_state) && 
		(node->nod_arg[e_sto_sub_store]))
		{
		if (!top_node)
		    {
		    top_node = node;
		    which_sto_trig = PRE_TRIG;
		    }
		prev_node = node;
		node = store (tdbb, node, which_sto_trig);
		if (which_sto_trig == PRE_TRIG)
		    {
		    node = prev_node->nod_arg[e_sto_sub_store];
		    node->nod_parent = prev_node;
		    }
		if (top_node == prev_node && which_sto_trig == POST_TRIG)
		    {
		    top_node = NULL;
		    which_sto_trig = ALL_TRIGS;
		    }
		else
		    request->req_operation = req_evaluate;
		}
	    else
		{
		prev_node = node;
		node = store (tdbb, node, ALL_TRIGS);
		if (!(prev_node->nod_arg[e_sto_sub_store]) &&
		    which_sto_trig == PRE_TRIG)
		    which_sto_trig = POST_TRIG;
		}

	    break;

#ifdef SCROLLABLE_CURSORS
	case nod_seek:
	    node = seek_rse (tdbb, request, node);
	    break;
#endif

#ifdef PC_ENGINE
	case nod_stream:
	    node = stream (tdbb, node);
	    break;

	case nod_find:
	    node = find (tdbb, node);
	    break;

	case nod_find_dbkey:
	case nod_find_dbkey_version:
	    node = find_dbkey (tdbb, node);
	    break;

	case nod_set_index:
	    node = set_index (tdbb, node);
	    break;

	case nod_set_bookmark:
	    node = set_bookmark (tdbb, node);
	    break;

	case nod_release_bookmark:
	    node = release_bookmark (tdbb, node);
	    break;     

	case nod_end_range:
	    node = RNG_end (node);
	    break; 

	case nod_delete_range:
	    node = RNG_delete (node);
	    break;     
	
	case nod_delete_ranges:
	    if (request->req_operation == req_evaluate)
		{
		RNG_delete_ranges (request);
		request->req_operation = req_return;
		}
	    node = node->nod_parent;
	    break;     
	
	case nod_range_relation:
	    node = RNG_add_relation (node);
	    break;

	case nod_release_lock:
	    if (request->req_operation == req_evaluate)
		{
		DSC	*desc;

		desc = EVL_expr (tdbb, node->nod_arg [e_rellock_lock]);
#if !defined(HAS_64BIT_POINTERS)
		    RLCK_release_lock (*(LCK*) desc->dsc_address);
#else
		    {
		    ATT		attachment;
		    LCK		lock;
		    ULONG	slot;
		    VEC		vector;

		    attachment = tdbb->tdbb_attachment;

		    lock = NULL;
		    slot = *(ULONG*) desc->dsc_address;
		    if ((vector = attachment->att_lck_quick_ref) &&
			slot < vector->vec_count)
			lock = (LCK) vector->vec_object [slot];
		    RLCK_release_lock (lock);
		    vector->vec_object [slot] = NULL;
		    }
#endif
		request->req_operation = req_return;
		}
	    node = node->nod_parent;
	    break; 

	case nod_release_locks:
	    if (request->req_operation == req_evaluate)
		{
		RLCK_release_locks (request->req_attachment);
		request->req_operation = req_return;
		}
	    node = node->nod_parent;
	    break; 

	case nod_force_crack:
	    if (request->req_operation == req_evaluate)
		{
                RSE_MARK_CRACK (tdbb, *(RSB*) node->nod_arg [1], irsb_crack | irsb_forced_crack);
		request->req_operation = req_return;
		}
	    node = node->nod_parent;
	    break; 

	case nod_reset_stream:
	    if (request->req_operation == req_evaluate)
		{
		RSE_reset_position (tdbb, *(RSB*) node->nod_arg [e_reset_from_rsb],
			request->req_rpb + (USHORT) node->nod_arg [e_reset_to_stream]);
		request->req_operation = req_return;
		}
	    node = node->nod_parent;
	    break;
#endif

	case nod_set_generator:
	    if (request->req_operation == req_evaluate)
		{
		DSC	*desc;

		desc = EVL_expr (tdbb, node->nod_arg [e_gen_value]);
		(void) DPM_gen_id (tdbb, (SLONG) node->nod_arg [e_gen_id], 1,
					MOV_get_int64 (desc, 0));	
		request->req_operation = req_return; 
	 	}	
	    node = node->nod_parent;
	    break;

	case nod_set_generator2:
	    if (request->req_operation == req_evaluate)
		{
		DSC	*desc;

		desc = EVL_expr (tdbb, node->nod_arg [e_gen_value]);
		(void) DPM_gen_id (tdbb, (SLONG) node->nod_arg [e_gen_id], 1,
					MOV_get_int64 (desc, 0));	
		request->req_operation = req_return; 
	 	}	
	    node = node->nod_parent;
	    break;

	default:
	    BUGCHECK (168); /* msg 168 looper: action not yet implemented */
	}
#ifdef HSDEBUGSTACK
      TestStack ();
#endif
#if defined(DEBUG_GDS_ALLOC) && defined(PROD_BUILD)
    memory_count++;
    if ((memory_count % memory_debug) == 0)
	ALL_check_memory ();
#endif
    }

/* if there is no node, assume we have finished processing the 
   request unless we are in the middle of processing an asynchronous message */

if (!node
#ifdef SCROLLABLE_CURSORS
 && !(request->req_flags & req_async_processing)
#endif
)
    {
    request->req_flags &= ~(req_active | req_reserved);
    request->req_timestamp = 0;
    release_blobs (tdbb, request);
    }

request->req_next = node;
tdbb->tdbb_default = old_pool;
tdbb->tdbb_transaction = (tdbb->tdbb_request = old_request) ?
    old_request->req_transaction : NULL;
tdbb->tdbb_setjmp = (UCHAR*) old_env;

/* in the case of a pending error condition (one which did not
   result in a longjmp to the top of looper), we need to delete
   the last savepoint */

if (error_pending)
    {
    if (transaction != dbb->dbb_sys_trans) 
	{
	SAV	save_point;

	for (save_point = transaction->tra_save_point;
	     ((save_point) && (save_point_number <= save_point->sav_number));
	     save_point = transaction->tra_save_point)
	    {
	    if (error_pending)
		++transaction->tra_save_point->sav_verb_count;
	    VERB_CLEANUP;
	    }
	}

    ERR_punt();
    }

/* if the request was aborted, assume that we have already
   longjmp'ed to the top of looper, and therefore that the last 
   savepoint has already been deleted */

if (request->req_flags & req_abort)
    ERR_post (gds__req_sync, 0);

return node;
}

static NOD modify (
    TDBB		tdbb,
    register NOD	node,
    SSHORT		which_trig)
{
/**************************************
 *
 *	m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Execute a MODIFY statement.
 *
 **************************************/
DBB		dbb;
register REQ	request, trigger;
STA		impure;
FMT		org_format, new_format;
SSHORT		org_stream, new_stream;
REC		org_record, new_record;
RPB		*org_rpb, *new_rpb;
REL		relation;
TRA		transaction;
#ifdef PC_ENGINE
RSB		rsb = NULL;
LCK 		implicit_lock = NULL, record_locking;
JMP_BUF		env, *old_env;
IRSB            irsb;
#endif

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
BLKCHK (node, type_nod);

request = tdbb->tdbb_request;
transaction = request->req_transaction;
impure = (STA) ((SCHAR*) request + node->nod_impure);

org_stream = (USHORT) node->nod_arg [e_mod_org_stream];
org_rpb = &request->req_rpb [org_stream];
relation = org_rpb->rpb_relation;

new_stream = (USHORT) node->nod_arg [e_mod_new_stream];
new_rpb = &request->req_rpb [new_stream];

#ifdef PC_ENGINE
/* for navigational streams, retrieve the rsb */

if (node->nod_arg [e_mod_rsb])
    {
    rsb = *(RSB*) node->nod_arg [e_mod_rsb];
    irsb = (IRSB) ((UCHAR*) request + rsb->rsb_impure);
    }

/* if we are on a crack in a navigational stream, modify is an illegal operation */

if (rsb && EXE_crack (tdbb, rsb, irsb_bof | irsb_eof | irsb_crack))
    {
    EXE_mark_crack (tdbb, rsb, irsb->irsb_flags);
    request->req_operation = req_return;
    return node->nod_parent;
    }
#endif

/* If the stream was sorted, the various fields in the rpb are
   probably junk.  Just to make sure that everything is cool,
   refetch and release the record. */

if (org_rpb->rpb_stream_flags & RPB_s_refetch)
    {
    SLONG	tid_fetch;

    tid_fetch = org_rpb->rpb_transaction;
    if ((!DPM_get (tdbb, org_rpb, LCK_read)) ||
	(!VIO_chase_record_version (tdbb, org_rpb, NULL, transaction, tdbb->tdbb_default)))
	ERR_post (gds__deadlock, gds_arg_gds, gds__update_conflict, 0);
    VIO_data (tdbb, org_rpb, tdbb->tdbb_request->req_pool);

    /* If record is present, and the transaction is read committed,
     * make sure the record has not been updated.  Also, punt after
     * VIO_data () call which will release the page.
     */

    if ((transaction->tra_flags & TRA_read_committed) &&
	(tid_fetch != org_rpb->rpb_transaction)) 
	ERR_post (gds__deadlock, gds_arg_gds, gds__update_conflict, 0);

    org_rpb->rpb_stream_flags &= ~RPB_s_refetch;
    }

switch (request->req_operation)
    {
    case req_evaluate:
	break;

    case req_return:
	if (impure->sta_state)
	    {
	    impure->sta_state = 0;
	    org_record = org_rpb->rpb_record;
	    new_record = new_rpb->rpb_record;
	    MOVE_FASTER (new_record->rec_data, org_record->rec_data, new_record->rec_length);
	    request->req_operation = req_evaluate;
	    return node->nod_arg [e_mod_statement];
	    }

#ifndef GATEWAY
#ifdef PC_ENGINE
	/* check to see if record locking has been initiated in this database;
	   if so then lock the record for shared write so that normal processing
	   will be able to read or write the record but not when an explicit
	   lock has been taken out */

	if (!(transaction->tra_flags & TRA_degree3))
	    {
	    record_locking = RLCK_record_locking (relation);
	    if (record_locking->lck_physical != LCK_PR)
		{
		implicit_lock = implicit_record_lock (transaction, org_rpb);

		/* set up to catch any errors so that we can release the lock */

		old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
		tdbb->tdbb_setjmp = (UCHAR*) env;
		if (SETJMP (env))
		    {
		    tdbb->tdbb_setjmp = (UCHAR*) old_env;
		    RLCK_unlock_record_implicit (implicit_lock, NULL_PTR);
		    LONGJMP (old_env, -1);
		    }
		}
	    }
#endif

	if (transaction != dbb->dbb_sys_trans)
	    ++transaction->tra_save_point->sav_verb_count;

	if (relation->rel_pre_modify &&
	    which_trig != POST_TRIG &&
	    (trigger = execute_triggers (tdbb, &relation->rel_pre_modify,
					 org_rpb->rpb_record,
					 new_rpb->rpb_record)))
	    trigger_failure (tdbb, trigger);

	if (node->nod_arg [e_mod_validate])
	    validate (tdbb, node->nod_arg [e_mod_validate]);

	if (relation->rel_file)
	    EXT_modify (org_rpb, new_rpb, transaction);
	else if (!relation->rel_view_rse)
	    {
	    SSHORT	bad_index;
	    REL		bad_relation;
	    IDX_E	error_code;

	    VIO_modify (tdbb, org_rpb, new_rpb, transaction);
	    if (error_code = IDX_modify (tdbb, org_rpb, new_rpb, transaction, &bad_relation, &bad_index))
		ERR_duplicate_error (error_code, bad_relation, bad_index);
	    }

	if (relation->rel_post_modify &&
	    which_trig != PRE_TRIG &&
	    (trigger = execute_triggers (tdbb, &relation->rel_post_modify,
					 org_rpb->rpb_record,
					 new_rpb->rpb_record)))
	    trigger_failure (tdbb, trigger);

	/* now call IDX_modify_check_constrints after all post modify triggers 
	   have fired.  This is required for cascading referential integrity, 
	   which can be implemented as post_erase triggers */

	if (!relation->rel_file && !relation->rel_view_rse)
	    {
	    SSHORT	bad_index;
	    REL		bad_relation;
	    IDX_E	error_code;

	    if (error_code = IDX_modify_check_constraints (tdbb, org_rpb, new_rpb, transaction, &bad_relation, &bad_index))
                ERR_duplicate_error (error_code, bad_relation, bad_index);
	    }

	if (transaction != dbb->dbb_sys_trans)
	    --transaction->tra_save_point->sav_verb_count;

#ifdef PC_ENGINE
	if (implicit_lock)
	    {
	    tdbb->tdbb_setjmp = (UCHAR*) old_env;
	    RLCK_unlock_record_implicit (implicit_lock, NULL_PTR);
	    }

	/* if the stream is navigational, we must position the stream on the new 
	   record version, but first set the record number  */

	new_rpb->rpb_number = org_rpb->rpb_number;
	if (rsb)
	    RSE_reset_position (tdbb, rsb, new_rpb);
#endif
#else
	if (node->nod_arg [e_mod_validate])
	    validate (tdbb, node->nod_arg [e_mod_validate]);
	VIO_modify (tdbb, org_rpb, new_rpb, transaction, node->nod_arg [e_mod_sql]);
#endif

	request->req_records_updated++;

	if (which_trig != PRE_TRIG)
	    {
	    org_record = org_rpb->rpb_record;
	    org_rpb->rpb_record = new_rpb->rpb_record;
	    new_rpb->rpb_record = org_record;
	    }

    default:
	return node->nod_parent;
    }

impure->sta_state = 0;
RLCK_reserve_relation (tdbb, transaction, relation, TRUE, TRUE);

/* Fall thru on evaluate to set up for modify before executing sub-statement.
   This involves finding the appropriate format, making sure a record block
   exists for the stream and is big enough, and copying fields from the
   original record to the new record. */

new_format = MET_current (tdbb, new_rpb->rpb_relation);
new_record = VIO_record (tdbb, new_rpb, new_format, tdbb->tdbb_default);
new_rpb->rpb_address = new_record->rec_data;
new_rpb->rpb_length = new_format->fmt_length;
new_rpb->rpb_format_number = new_format->fmt_version;

if (!(org_record = org_rpb->rpb_record))
    {
    org_record = VIO_record (tdbb, org_rpb, new_format, tdbb->tdbb_default);
    org_format = org_record->rec_format;
    org_rpb->rpb_address = org_record->rec_data;
    org_rpb->rpb_length = org_format->fmt_length;
    org_rpb->rpb_format_number = org_format->fmt_version;
    }
else
    org_format = org_record->rec_format;

/* Copy the original record to the new record.  If the format hasn't changed,
   this is a simple move.  If the format has changed, each field must be
   fetched and moved separately, remembering to set the missing flag. */

if (new_format->fmt_version == org_format->fmt_version)
    MOVE_FASTER (org_record->rec_data, new_rpb->rpb_address, new_rpb->rpb_length);
else
    {
    SSHORT	i;
    DSC		org_desc, new_desc;

    for (i = 0; i < new_format->fmt_count; i++)
	{
	/* In order to "map a null to a default" value (in EVL_field()), 
	 * the relation block is referenced. 
	 * Reference: Bug 10116, 10424 
	 */
	CLEAR_NULL (new_record, i);
	if (EVL_field (new_rpb->rpb_relation, new_record, i, &new_desc))
	    {
	    if (EVL_field (org_rpb->rpb_relation, org_record, i, &org_desc))
		MOV_move (&org_desc, &new_desc);
	    else
		{
		SET_NULL (new_record, i);
		if (new_desc.dsc_dtype)
		    {
		    UCHAR	*p;
		    USHORT	n;

		    p = new_desc.dsc_address;
		    n = new_desc.dsc_length;
		    do *p++ = 0; while (--n);
		    }
		}  /* if (org_record) */
	    }  /* if (new_record) */
	}  /* for (fmt_count) */
    }

if (node->nod_arg [e_mod_map_view])
    {
    impure->sta_state = 1;
    return node->nod_arg [e_mod_map_view];
    }

return node->nod_arg [e_mod_statement];					
}

static NOD receive_msg (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	r e c e i v e _ m s g
 *
 **************************************
 *
 * Functional description
 *	Execute a RECEIVE statement.  This can be entered either
 *	with "req_evaluate" (ordinary receive statement) or
 *	"req_proceed" (select statement).  In the latter case,
 *	the statement isn't every formalled evaluated.
 *
 **************************************/
register REQ	request;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

switch (request->req_operation)
    {
    case req_evaluate:
	request->req_operation = req_receive;
	request->req_message = node->nod_arg [e_send_message];
	request->req_flags |= req_stall;
	return node;
    
    case req_proceed:
	request->req_operation = req_evaluate;
	return (node->nod_arg [e_send_statement]);

    default:
	return (node->nod_parent);
    }
}

static void release_blobs (
    TDBB	tdbb,
    REQ		request)
{
/**************************************
 *
 *	r e l e a s e _ b l o b s
 *
 **************************************
 *
 * Functional description
 *	Release temporary blobs assigned by this request.
 *
 **************************************/
TRA	transaction;
BLB	*blob;
ARR	*array;

SET_TDBB (tdbb);
DEV_BLKCHK (request, type_req);

if (transaction = request->req_transaction)
    {
    DEV_BLKCHK (transaction, type_tra);

    /* Release blobs assigned by this request */

    for (blob = &transaction->tra_blobs; *blob;)
	{
	DEV_BLKCHK (*blob, type_blb);
	if ((*blob)->blb_request == request)
	    BLB_cancel (tdbb, *blob);
	else
	    blob = &(*blob)->blb_next;
        }

    /* Release arrays assigned by this request */

    for (array = &transaction->tra_arrays; *array;)
	{
	DEV_BLKCHK (*array, type_arr);
	if ((*array)->arr_request == request)
	    BLB_release_array (*array);
	else
	    array = &(*array)->arr_next;
        }
    }
}

#ifdef PC_ENGINE
static NOD release_bookmark (
    TDBB	tdbb,
    NOD		node)
{
/**************************************
 *
 *	r e l e a s e _ b o o k m a r k
 *
 **************************************
 *
 * Functional description
 *	Deallocate the passed bookmark.
 *
 **************************************/
REQ	request;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

if (request->req_operation == req_evaluate)
    {
    BKM_release (node->nod_arg [e_relmark_id]);
    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif

static void release_proc_save_points (
    REQ	request)
{
/**************************************
 *
 *	r e l e a s e _ p r o c _ s a v e _ p o i n t s
 *
 **************************************
 *
 * Functional description
 *	Release temporary blobs assigned by this request.
 *
 **************************************/
TRA	transaction;
SAV	sav_point, temp_sav_point;

/* Release savepoints assigned by this request */

if ((transaction = request->req_transaction) &&
    (sav_point = request->req_proc_sav_point))
    {
    for (temp_sav_point = sav_point; temp_sav_point->sav_next; 
    		temp_sav_point = temp_sav_point->sav_next)
	;
    temp_sav_point->sav_next = transaction->tra_save_free;
    transaction->tra_save_free = sav_point;	
    }
request->req_proc_sav_point = NULL;
}

static void release_triggers (
    TDBB	tdbb,
    VEC		vector)
{
/**************************************
 *
 *	r e l e a s e _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Release vector of triggers if inactive.
 *
 **************************************/
REQ	*ptr, *end;

SET_TDBB (tdbb);

for (ptr = (REQ*) vector->vec_object, end = ptr + vector->vec_count;
     ptr < end; ptr++)
    if (*ptr && CMP_clone_active (*ptr))
	return;

for (ptr = (REQ*) vector->vec_object, end = ptr + vector->vec_count;
     ptr < end; ptr++)
    if (*ptr)
	CMP_release (tdbb, *ptr);

ALL_release (vector);
}

#ifdef SCROLLABLE_CURSORS
static NOD seek_rse (
    TDBB	tdbb,
    REQ		request, 
    NOD         node)
{
/**************************************
 *
 *      s e e k _ r s e
 *
 **************************************
 *
 * Functional description
 *	Execute a nod_seek, which specifies 
 *	a direction and offset in which to 
 *	scroll a record selection expression.
 *
 **************************************/
USHORT          direction;
SLONG           offset;
RSE		rse;

SET_TDBB (tdbb);
DEV_BLKCHK (node, type_nod);

if (request->req_operation == req_proceed)
    {
    /* get input arguments */

    direction = MOV_get_long (EVL_expr (tdbb,
					node->nod_arg [e_seek_direction]), 
			      0);
    offset = MOV_get_long (EVL_expr (tdbb, node->nod_arg [e_seek_offset]), 0);

    rse = (RSE) node->nod_arg [e_seek_rse];

    seek_rsb (tdbb, request, rse->rse_rsb, direction, offset);

    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif

#ifdef SCROLLABLE_CURSORS
static void seek_rsb (
    TDBB	tdbb,
    REQ		request, 
    RSB		rsb,
    USHORT	direction,
    SLONG	offset)
{
/**************************************
 *
 *      s e e k _ r s b
 *
 **************************************
 *
 * Functional description
 *	Allow scrolling through a stream as defined 
 *	by the input rsb.  Handles cracks, refresh 
 *	ranges, and multiple seeking.  Uses RSE_get_record ()
 *	to do the actual work.
 *
 **************************************/
USHORT          crack_flag = 0;
IRSB            impure;
IRSB            next_impure;

SET_TDBB (tdbb);
DEV_BLKCHK (rsb, type_rsb);
impure = (IRSB) ((UCHAR*) request + rsb->rsb_impure);

/* look past any boolean to the actual stream */

if (rsb->rsb_type == rsb_boolean)
    {
    seek_rsb (tdbb, request, rsb->rsb_next, direction, offset);
	   
    /* set the backwards flag */

    next_impure = (IRSB) ((UCHAR*) request + rsb->rsb_next->rsb_impure);

    if (next_impure->irsb_flags & irsb_last_backwards)
        impure->irsb_flags |= irsb_last_backwards;
    else 
        impure->irsb_flags &= ~irsb_last_backwards;
    return;
    }
	
/* do simple boundary checking for bof and eof */

switch (direction)
    {
    case blr_forward:
        if (impure->irsb_flags & irsb_eof)
	    ERR_post (isc_stream_eof, 0);
	break;

    case blr_backward: 
        if (impure->irsb_flags & irsb_bof)
	    ERR_post (isc_stream_bof, 0);
	break;

    case blr_bof_forward:
    case blr_eof_backward:
        break;

    default:
    	BUGCHECK (232);
    }

/* the actual offset to seek may be one less because the next time 
   through the blr_for loop we will seek one record--flag the fact 
   that a fetch is required on this stream in case it doesn't happen 
   (for example when GPRE generates BLR which does not stall prior to 
   the blr_for, as DSQL does) */

if (offset > 0)
    switch (direction) 
        {
	case blr_forward: 
	case blr_bof_forward: 
	    if (!(impure->irsb_flags & irsb_last_backwards))
	        {
                offset--;
		if (!(impure->irsb_flags & irsb_bof)) 
                    request->req_flags |= req_fetch_required; 
                }
	    break;

	case blr_backward: 
	case blr_eof_backward: 
	    if (impure->irsb_flags & irsb_last_backwards)
	        {
                offset--;
		if (!(impure->irsb_flags & irsb_eof)) 
                    request->req_flags |= req_fetch_required; 
                }
	    break;
	}			    

/* now do the actual seek */
		  
switch (direction)
    {
    case blr_forward:    /* go forward from the current location */

#ifdef PC_ENGINE
	if ((offset == 1) && request->req_begin_ranges)
            impure->irsb_flags |= irsb_refresh;
#endif

    /* the rsb_backwards flag is used to indicate the direction to seek in; 
       this is sticky in the sense that after the user has seek'ed in the 
       backward direction, the next retrieval from a blr_for loop will also 
       be in the backward direction--this allows us to continue scrolling 
       without constantly sending messages to the engine */

    impure->irsb_flags &= ~irsb_last_backwards;

    while (offset)
        {
        offset--;
        if (!(RSE_get_record (tdbb, rsb, RSE_get_next)))
	    break;
	}
    break;

    case blr_backward:    /* go backward from the current location */
    
#ifdef PC_ENGINE
	if ((offset == 1) && request->req_begin_ranges)
            impure->irsb_flags |= irsb_refresh;
#endif

        impure->irsb_flags |= irsb_last_backwards;

	while (offset)
	    {
	    offset--;
	    if (!(RSE_get_record (tdbb, rsb, RSE_get_next)))
		break;
	    }
	break;

    case blr_bof_forward:    /* go forward from the beginning of the stream */

	RSE_close (tdbb, rsb);
	RSE_open (tdbb, rsb);

        impure->irsb_flags &= ~irsb_last_backwards;

	while (offset)
	    {
	    offset--;
	    if (!(RSE_get_record (tdbb, rsb, RSE_get_next)))
		break;
	    }
	break;

    case blr_eof_backward:    /* go backward from the end of the stream */

	RSE_close (tdbb, rsb);
	RSE_open (tdbb, rsb);    

	/* if this is a stream type which uses bof and eof flags, 
           reverse the sense of bof and eof in this case */

        if (impure->irsb_flags & irsb_bof)
	    {
            impure->irsb_flags &= ~irsb_bof;
            impure->irsb_flags |= irsb_eof;
	    }

        impure->irsb_flags |= irsb_last_backwards;

	while (offset)
	    {
	    offset--;
	    if (!(RSE_get_record (tdbb, rsb, RSE_get_next)))
		break;
	    }
	break;

    default:
	BUGCHECK (232);
    }

#ifdef PC_ENGINE
impure->irsb_flags &= ~irsb_refresh;
#endif
}
#endif

static NOD selct (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	s e l e c t
 *
 **************************************
 *
 * Functional description
 *	Execute a SELECT statement.  This is more than a little
 *	obscure.  We first set up the SELECT statement as the
 *	"message" and stall on receive (waiting for user send).
 *	EXE_send will then loop thru the sub-statements of select
 *	looking for the appropriate RECEIVE statement.  When (or if)
 *	it finds it, it will set it up the next statement to be
 *	executed.  The RECEIVE, then, will be entered with the
 *	operation "req_proceed."
 *
 **************************************/
register REQ	request;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

switch (request->req_operation)
    {
    case req_evaluate:
	request->req_message = node;
	request->req_operation = req_receive;
	request->req_flags |= req_stall;
	return node;
    
    default:
	return node->nod_parent;
    }
}


static NOD send_msg (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	s e n d _ m s g
 *
 **************************************
 *
 * Functional description
 *	Execute a SEND statement.
 *
 **************************************/
register REQ	request;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

switch (request->req_operation)
    {
    case req_evaluate:
	return (node->nod_arg [e_send_statement]);

    case req_return:
	request->req_operation = req_send;
	request->req_message = node->nod_arg [e_send_message];
	request->req_flags |= req_stall;
	return node;
    
    case req_proceed:
	request->req_operation = req_return;
	return node->nod_parent;

    default:
	return (node->nod_parent);
    }
}    

#ifdef PC_ENGINE
static NOD set_bookmark (
    TDBB	tdbb,
    NOD		node)
{
/**************************************
 *
 *	s e t _ b o o k m a r k
 *
 **************************************
 *
 * Functional description
 *	Set a stream to the location of the
 *	specified bookmark.
 *
 **************************************/
REQ	request;
BKM	bookmark;
USHORT	stream;
RPB	*rpb;
RSB	rsb;
IRSB    impure;


SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

if (request->req_operation == req_evaluate)
    {
    bookmark = BKM_lookup (node->nod_arg [e_setmark_id]);
    stream = (USHORT) node->nod_arg [e_setmark_stream];
    rpb = &request->req_rpb [stream];
    rsb = *((RSB*) node->nod_arg [e_setmark_rsb]);
    impure = (IRSB) ((UCHAR*) request + rsb->rsb_impure);

    /* check if the bookmark was at beginning or end of file 
       and flag the rsb accordingly */

    RSE_MARK_CRACK (tdbb, rsb, 0);
    if (bookmark->bkm_flags & bkm_bof)
        RSE_MARK_CRACK (tdbb, rsb, irsb_bof); 
    else if (bookmark->bkm_flags & bkm_eof)
        RSE_MARK_CRACK (tdbb, rsb, irsb_eof);
    else if (bookmark->bkm_flags & bkm_crack)
	{
        RSE_MARK_CRACK (tdbb, rsb, irsb_crack); 
	if (bookmark->bkm_flags & bkm_forced_crack)
            RSE_MARK_CRACK (tdbb, rsb, irsb_forced_crack); 
	}

    if (!RSE_set_bookmark (tdbb, rsb, rpb, bookmark))
        EXE_mark_crack (tdbb, rsb, impure->irsb_flags & (irsb_crack | irsb_eof | irsb_bof));

    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif

static void set_error (
    TDBB	tdbb,
    XCP condition)
{
/**************************************
 *
 *	s e t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Set status vector according to specified error condition
 *	and jump to handle error accordingly.
 *
 **************************************/
register REQ	request;
TEXT		name[32], relation_name [32], message[82], *s, *r;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;

switch (condition->xcp_rpt [0].xcp_type)
    {
    case xcp_sql_code:
	ERR_post (gds__sqlerr,
		gds_arg_number, (SLONG) condition->xcp_rpt [0].xcp_code, 0);

    case xcp_gds_code:
	if (condition->xcp_rpt [0].xcp_code == gds__check_constraint)
	    {
	    MET_lookup_cnstrt_for_trigger (tdbb, name, relation_name, request->req_trg_name);
	    s = (name [0]) ? name : "";
	    r = (relation_name [0]) ? relation_name : "";
	    ERR_post (condition->xcp_rpt [0].xcp_code, 
		gds_arg_string, ERR_cstring (s),
                gds_arg_string, ERR_cstring (r),  
                0);
	    }
	else
	    ERR_post (condition->xcp_rpt [0].xcp_code, 0);

    case xcp_xcp_code:
	MET_lookup_exception (tdbb, condition->xcp_rpt [0].xcp_code,
				name, message);
	if (message [0])
	    s = message;
	else if (name [0])
	    s = name;
	else
	    s = NULL;
	if (s)
	    ERR_post (gds__except,
		gds_arg_number, (SLONG) condition->xcp_rpt [0].xcp_code,
		 gds_arg_gds, gds__random, gds_arg_string, ERR_cstring (s), 0);
	else
	    ERR_post (gds__except,
		gds_arg_number, (SLONG) condition->xcp_rpt [0].xcp_code, 0);
    }
}

#ifdef PC_ENGINE
static NOD set_index (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	s e t _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Execute a SET INDEX statement.
 *
 **************************************/
register REQ	request;
USHORT		stream, id;
RPB		*rpb;
REL		relation;
IDX		idx;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

if (request->req_operation == req_evaluate)
    {
    stream = (USHORT) node->nod_arg [e_index_stream];

    rpb = &request->req_rpb [stream];
    relation = rpb->rpb_relation;

    /* if id is non-zero, get the index definition;
       otherwise it indicates revert to natural order */

    id = MOV_get_long (EVL_expr (tdbb, node->nod_arg [e_index_index]), 0);
    if (id && BTR_lookup (tdbb, relation, id - 1, &idx))
	ERR_post (gds__indexnotdefined, gds_arg_string, relation->rel_name,
		gds_arg_number, (SLONG) id, 0);	

    /* generate a new rsb in place of the old */

    RSE_close (tdbb, *(RSB*) node->nod_arg [e_index_rsb]);
    OPT_set_index (tdbb, request, (RSB*) node->nod_arg [e_index_rsb], relation, id ? &idx : (IDX*) 0);
    RSE_open (tdbb, *(RSB*) node->nod_arg [e_index_rsb]);

    request->req_operation = req_return; 
    }

return node->nod_parent;
}
#endif

static NOD stall (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	s t a l l 
 *
 **************************************
 *
 * Functional description
 *	Execute a stall statement.
 *	This is like a blr_receive, except that there is no
 *	need for a gds__send () from the user (i.e. EXE_send () in the engine).
 *	A gds__receive () will unblock the user.
 *
 **************************************/
register REQ	request;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

switch (request->req_operation)
    {
    case req_sync:
	return node->nod_parent;
    
    case req_proceed:
	request->req_operation = req_return;
	return node->nod_parent;

    default:
	request->req_message = node;
	request->req_operation = req_return;
	request->req_flags |= req_stall;
	return node;
    }
}

static NOD store (
    TDBB		tdbb,
    register NOD	node,
    SSHORT		which_trig)
{
/**************************************
 *
 *	s t o r e
 *
 **************************************
 *
 * Functional description
 *	Execute a STORE statement.
 *
 **************************************/
DBB		dbb;
register REQ	request;
STA		impure;
REQ		trigger;
FMT		format;
SSHORT		stream, n;
USHORT		length;
REC		record;
RPB		*rpb;
DSC		*desc;
REL		relation;
TRA		transaction;
register UCHAR	*p;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
BLKCHK (node, type_nod);

request = tdbb->tdbb_request;
transaction = request->req_transaction;
impure = (STA) ((SCHAR*) request + node->nod_impure);
stream = (USHORT) node->nod_arg [e_sto_relation]->nod_arg [e_rel_stream];
rpb = &request->req_rpb [stream];
relation = rpb->rpb_relation;

switch (request->req_operation)
    {
    case req_evaluate:
	impure->sta_state = 0;
	RLCK_reserve_relation (tdbb, transaction, relation, TRUE, TRUE);
	break;

    case req_return:
	if (impure->sta_state)
	    return node->nod_parent;
	record = rpb->rpb_record;
	format = record->rec_format;

#ifndef GATEWAY
	if (transaction != dbb->dbb_sys_trans)
	    ++transaction->tra_save_point->sav_verb_count;

	if (relation->rel_pre_store &&
	    (which_trig != POST_TRIG) &&
	    (trigger = execute_triggers (tdbb, &relation->rel_pre_store,
					 NULL_PTR, record)))
	    trigger_failure (tdbb, trigger);
#endif

	if (node->nod_arg [e_sto_validate])
	    validate (tdbb, node->nod_arg [e_sto_validate]);
	
	/* For optimum on-disk record compression, zero all unassigned
	   fields. In addition, zero the tail of assigned varying fields
	   so that previous remnants don't defeat compression efficiency. */
	
	for (n = 0; n < format->fmt_count; n++)
	    {
	    desc = &format->fmt_desc [n];
	    if (!desc->dsc_address)
		continue;
	    p = record->rec_data + (SLONG) desc->dsc_address;
	    if (TEST_NULL (record, n))
	    	{
		if (length = desc->dsc_length)
		    do *p++ = 0; while (--length);
		}
	    else if (desc->dsc_dtype == dtype_varying)
	    	{
		VARY	*vary;

		vary = (VARY*) p;
		if ((length = desc->dsc_length - sizeof (USHORT)) > vary->vary_length)
		    {
		    p = vary->vary_string + vary->vary_length;
		    length -= vary->vary_length;
	    	    do *p++ = 0; while (--length);
		    }
		}
	    }

#ifndef GATEWAY
	if (relation->rel_file)
	    EXT_store (rpb, transaction);
	else if (!relation->rel_view_rse)
	    {
	    SSHORT	bad_index;
	    REL		bad_relation;
	    IDX_E	error_code;

	    VIO_store (tdbb, rpb, transaction);
	    if ((error_code = IDX_store (tdbb, rpb, transaction, &bad_relation, &bad_index)))
		ERR_duplicate_error (error_code, bad_relation, bad_index);
	    }

	if (relation->rel_post_store &&
	    (which_trig != PRE_TRIG) &&
	    (trigger = execute_triggers (tdbb, &relation->rel_post_store,
					 NULL_PTR, record)))
	    trigger_failure (tdbb, trigger);

	request->req_records_inserted++;

	if (transaction != dbb->dbb_sys_trans)
	    --transaction->tra_save_point->sav_verb_count;
#else
	VIO_store (tdbb, rpb, transaction, node->nod_arg [e_sto_sql]);
#endif

	if (node->nod_arg [e_sto_statement2])
	    {
	    impure->sta_state = 1;
	    request->req_operation = req_evaluate;
	    return node->nod_arg [e_sto_statement2];
	    }

    default:
	return node->nod_parent;
    }

/* Fall thru on evaluate to set up for store before executing sub-statement.
   This involves finding the appropriate format, making sure a record block
   exists for the stream and is big enough, and initialize all null flags
   to "missing." */

format = MET_current (tdbb, relation);
record = VIO_record (tdbb, rpb, format, tdbb->tdbb_default);

rpb->rpb_address = record->rec_data;
rpb->rpb_length = format->fmt_length;
rpb->rpb_format_number = format->fmt_version;

/* Initialize all fields to missing */

#ifndef GATEWAY
p = record->rec_data;
if (n = (format->fmt_count + 7) >> 3)
    do
	*p++ = 0xff;
    while (--n);
#else
EXE_set_fields_null (tdbb, record, format);
#endif

return node->nod_arg [e_sto_statement];
}

#ifdef PC_ENGINE
static NOD stream (
    TDBB		tdbb,
    register NOD	node)
{
/**************************************
 *
 *	s t r e a m
 *
 **************************************
 *
 * Functional description
 *	Execute a STREAM statement.
 *
 **************************************/
register REQ	request;
RSB		rsb;

SET_TDBB (tdbb);
request = tdbb->tdbb_request;
BLKCHK (node, type_nod);

rsb = ((RSE) node)->rse_rsb;

switch (request->req_operation)
    {
    case req_evaluate:
	RSE_open (tdbb, rsb);
	request->req_operation = req_return; 

    case req_return:
	node = node->nod_parent;
	break;

    default:
	RSE_close (tdbb, rsb);
	node = node->nod_parent;
    }
    
return node;
}
#endif

static BOOLEAN test_error (
    TDBB	tdbb,
    XCP	conditions)
{
/**************************************
 *
 *	t e s t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Test for match of current state with list of error conditions.
 *
 **************************************/
SSHORT	i, sqlcode;
STATUS	*status_vector;

SET_TDBB (tdbb);
status_vector = tdbb->tdbb_status_vector;
sqlcode = gds__sqlcode (status_vector);

for (i = 0; i < conditions->xcp_count; i++)
    {
    switch (conditions->xcp_rpt [i].xcp_type)
	{
	case xcp_sql_code:
	    if (sqlcode == conditions->xcp_rpt [i].xcp_code)
		{
		status_vector [0] = 0;
		status_vector [1] = 0;
		return TRUE;
		}
	    break;

	case xcp_gds_code:
	    if (status_vector [1] == conditions->xcp_rpt [i].xcp_code)
		{
		status_vector [0] = 0;
		status_vector [1] = 0;
		return TRUE;
		}
	    break;

	case xcp_xcp_code:
	    if ((status_vector [1] == gds__except) &&
		(status_vector [3] == conditions->xcp_rpt [i].xcp_code))
		{
		status_vector [0] = 0;
		status_vector [1] = 0;
		return TRUE;
		}
	    break;

	case xcp_default:
	    status_vector [0] = 0;
	    status_vector [1] = 0;
	    return TRUE;
	}
    }

return FALSE;
}

#ifndef GATEWAY
static void trigger_failure (
    TDBB	tdbb,
    REQ		trigger)
{
/**************************************
 *
 *	t r i g g e r _ f a i l u r e
 *
 **************************************
 *
 * Functional description
 *	Trigger failed, report error.
 *
 **************************************/
TEXT	*msg;
STATUS	code;

SET_TDBB (tdbb);
EXE_unwind (tdbb, trigger);
trigger->req_attachment = NULL;
trigger->req_flags &= ~req_in_use;
trigger->req_timestamp = 0;
if (trigger->req_flags & req_leave)
    {
    trigger->req_flags &= ~req_leave;
    if (trigger->req_trg_name &&
	(msg = MET_trigger_msg (tdbb, trigger->req_trg_name, trigger->req_label)))
	{
	if (trigger->req_flags & req_sys_trigger)
	    {
	    code = PAR_symbol_to_gdscode (msg);
	    if (code)
		ERR_post (gds__integ_fail,
		    gds_arg_number, (SLONG) trigger->req_label, 
		    gds_arg_gds, code, 0);
	    }
	ERR_post (gds__integ_fail,
		gds_arg_number, (SLONG) trigger->req_label, 
		gds_arg_gds, gds__random, gds_arg_string, msg, 0);
	}
    else
	ERR_post (gds__integ_fail, gds_arg_number, (SLONG) trigger->req_label, 0);
    }
else
    ERR_punt();
}
#endif

static void validate (
    TDBB	tdbb,
    NOD		list)
{
/**************************************
 *
 *	v a l i d a t e
 *
 **************************************
 *
 * Functional description
 *	Execute a list of validation expressions.
 *
 **************************************/
NOD	*ptr1, *ptr2;

SET_TDBB (tdbb);
BLKCHK (list, type_nod);

for (ptr1 = list->nod_arg, ptr2 = ptr1 + list->nod_count;
     ptr1 < ptr2; ptr1++)
    {
    if (!EVL_boolean (tdbb, (*ptr1)->nod_arg [e_val_boolean]))
	{
    	/* Validation error -- report result */

	NOD	node;
	VEC	vector;
	REL	relation;
	REQ	request;
	FLD	field;
	TEXT	*value, temp [128], *name;
	USHORT	length, stream, id;

    	node = (*ptr1)->nod_arg [e_val_value];
    	request = tdbb->tdbb_request;
    	length = MOV_make_string (EVL_expr (tdbb, node), ttype_dynamic, 
				&value, temp, sizeof (temp));

    	if (request->req_flags & req_null ||
    	    request->req_flags & req_clone_data_from_default_clause)
	    value = "*** null ***";
    	else if (!length)
	    value = "";
    	else
	    value = ERR_string (value, length);

    	if (node->nod_type == nod_field)
	    {
	    stream = (USHORT) node->nod_arg [e_fld_stream];
	    id = (USHORT) node->nod_arg [e_fld_id];
	    relation = request->req_rpb [stream].rpb_relation;
	    if ((vector = relation->rel_fields) &&
	    	id < vector->vec_count &&
	    	(field = (FLD) vector->vec_object [id]))
	    	name = field->fld_name;
	    }

    	if (!name)
	    name = "*** unknown ***";

    	ERR_post (gds__not_valid, gds_arg_string, name, 
	    gds_arg_string, value, 0);
	}
    }
}
