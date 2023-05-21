/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		exe.c
 *	DESCRIPTION:	Execution phase
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
#include <setjmp.h>

#define REQUESTS_MAIN
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../qli/exe.h"
#include "../qli/all_proto.h"
#include "../qli/err_proto.h"
#include "../qli/eval_proto.h"
#include "../qli/exe_proto.h"
#include "../qli/form_proto.h"
#include "../qli/forma_proto.h"
#include "../qli/mov_proto.h"
#include "../qli/repor_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/utl_proto.h"

#ifdef sparc
#ifndef SOLARIS
#include <vfork.h>
#endif
#endif

#ifdef mpexl
#define FOPEN_WRITE_TYPE	"w Ds2 V E32 S256000"
#endif

#ifndef FOPEN_WRITE_TYPE
#define FOPEN_WRITE_TYPE	"w"
#endif

extern int	*QLI_env;
extern USHORT	QLI_prompt_count, QLI_reprompt;

typedef struct vary {
    USHORT	vary_length;
    UCHAR	vary_string[1];
} VARY;

static DSC	*assignment (NOD, DSC *, NOD, NOD, PAR);
static void	commit_retaining (NOD);
static int	copy_blob (NOD, PAR);
static void	db_error (REQ, STATUS *);
static void	execute_abort (NOD);
static void	execute_assignment (NOD);
static void	execute_for (NOD);
static void	execute_modify (NOD);
static void	execute_output (NOD);
static void	execute_print (NOD);
static void	execute_repeat (NOD);
static void	execute_store (NOD);
static void	map_data (MSG);
static void	print_counts (REQ);
static void	set_null (MSG);
static void	transaction_state (NOD, DBB);

/* definitions for SET COUNT */

#define COUNT_ITEMS	4

static SCHAR count_info [] = { 
	gds__info_req_select_count, 
	gds__info_req_insert_count,
	gds__info_req_update_count,
        gds__info_req_delete_count };

void EXEC_abort (void)
{
/**************************************
 *
 *	E X E C _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	An asynchrous abort has been requested.  Request that all
 *	requests be unwound and set flag.
 *
 **************************************/
STATUS	status_vector [20];
REQ	request;

for (request = QLI_requests; request; request = request->req_next)
    if (request->req_handle)
	gds__unwind_request (status_vector, 
		GDS_REF (request->req_handle),
		0);

QLI_abort = TRUE;
}

void EXEC_execute (
    NOD		node)
{
/**************************************
 *
 *	E X E C _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Execute a node.
 *
 **************************************/
MSG	message;
NOD	*ptr;
USHORT	i;

if (QLI_abort)
    EXEC_poll_abort();

if (node)
    switch (node->nod_type)
    {
    case nod_abort:
	execute_abort (node);

    case nod_assign:
	execute_assignment (node);
	return;

    case nod_commit_retaining:
	commit_retaining (node);
	return;

    case nod_erase:
	if (message = (MSG) node->nod_arg [e_era_message])
	    EXEC_send (message);
	return;

    case nod_for:
	execute_for (node);
	return;
    
    case nod_form_for:
	FORM_display (node);
	return;

    case nod_form_update:
	FORM_update (node);
	return;

    case nod_list:
	ptr = node->nod_arg;
	for (i = 0; i < node->nod_count; i++)
	    EXEC_execute (*ptr++);
	return;

    case nod_modify:
	execute_modify (node);
	return;
    
    case nod_output:
	execute_output (node);
	return;

    case nod_print:
	execute_print (node);
	return;
    
    case nod_repeat:
	execute_repeat (node);
	return;

    case nod_store:
	execute_store (node);
	return;

    case nod_if:
	if (EVAL_boolean (node->nod_arg [e_if_boolean]))
	    {
	    EXEC_execute (node->nod_arg [e_if_true]);
	    return;
	    }
	if (node->nod_arg [e_if_false])
	    {
	    EXEC_execute (node->nod_arg [e_if_false]);
	    return;
	    }
	return;

    case nod_report_loop:
	RPT_report (node);
	return;

    default:
	BUGCHECK (33); /* Msg33 EXEC_execute: not implemented */
    }
}

void *EXEC_open_blob (
    NOD		node)
{
/**************************************
 *
 *	E X E C _ o p e n _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Given a blob field node, open and return the blob.
 *
 **************************************/
FLD	field;
CTX	context;
REQ	request;
DBB	dbb;
DSC	*desc, static_desc;
UCHAR	segment[20], bpb [20], *p;
USHORT	bpb_length;
void	*blob;
STATUS	status_vector [20];

desc = EVAL_value (node);
if (!desc)
    return FALSE;

/* Starting from the print item, work our way back to the database block */

if (node->nod_type == nod_reference)
    node = node->nod_arg [0];

if (node->nod_type != nod_field)
    BUGCHECK (34); /* Msg34 print_blob: expected field node */

context = (CTX) node->nod_arg [e_fld_context];
request = context->ctx_request;
dbb = request->req_database;
blob = NULL;

/* Format blob parameter block */

p = bpb;
*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_type;
*p++ = 2;
*p++ = desc->dsc_sub_type;
*p++ = desc->dsc_sub_type >> 8;
*p++ = gds__bpb_target_type;
*p++ = 1;
*p++ = 1;

#if (defined JPN_EUC || defined JPN_SJIS)
if (desc->dsc_sub_type == 1)
    {
    *p++ = gds__bpb_target_interp;
    *p++ = 2;
    *p++ = QLI_interp;
    *p++ = QLI_interp >> 8;
    }
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

bpb_length = p - bpb;

if (gds__open_blob2 (status_vector, 
	GDS_REF (dbb->dbb_handle), 
	GDS_REF (dbb->dbb_transaction),
	GDS_REF (blob), 
	GDS_VAL (desc->dsc_address),
	bpb_length,
	bpb))
    ERRQ_database_error (dbb, status_vector);

return blob;
}

struct file *EXEC_open_output (
    NOD		node)
{
/**************************************
 *
 *	E X E C _ o p e n _ o u t p u t
 *
 **************************************
 *
 * Functional description
 *	Open output stream to re-direct output.
 *
 **************************************/
IB_FILE	*file;
DSC	*desc;
TEXT	filename [256], temp [64], *p, *q, *argv [20], **arg;
int	pair [2];
SSHORT	l;

/* Evaluate filename and copy to a null terminated string */

desc = EVAL_value (node->nod_arg [e_out_file]);
l = MOVQ_get_string (desc, &p, temp, sizeof (temp));
q = filename;
if (l)
    do *q++ = *p++; while (--l);
*q = 0;

/* If output is to a file, just do it */

if (!node->nod_arg [e_out_pipe])
    {
    if (file = ib_fopen (filename, FOPEN_WRITE_TYPE))
	return (struct file*) file;

    ERRQ_print_error (42, filename, NULL, NULL, NULL, NULL); /* Msg42 Can't open output file %s */
    }

/* Output is to a file.  Setup file and fork process */

#ifdef VMS
    IBERROR (35); /* Msg35 output pipe is not supported on VMS */
#else

#ifdef mpexl
    IBERROR (453); /* Msg453 output pipe is not supported on MPE/XL */
#else
#if (defined WIN_NT || defined OS2_ONLY)
if (file = _popen (filename, "w"))
    return (struct file *)file;
#else
arg = argv;
p = filename;
while (*p)
    {
    *arg++ = p;
    while (*p && *p != ' ')
	p++;
    if (!*p)
	break;
    *p++ = 0;
    while (*p == ' ')
	p++;
    }
*arg = NULL;

if (pipe (pair) < 0)
    IBERROR (36); /* Msg36 couldn't create pipe */

if (!vfork())
    {
    close (pair [1]);
    close (0);
    dup (pair [0]);
    close (pair [0]);
    execvp (argv [0], argv);
    ERRQ_msg_put (43, filename, NULL, NULL, NULL, NULL); /* Msg43 Couldn't run %s */
    _exit (-1);
    }

close (pair [0]);

if (file = ib_fdopen (pair [1], "w"))
    return (struct file *)file;
#endif

IBERROR (37); /* Msg37 ib_fdopen failed */
#endif
#endif
}

void EXEC_poll_abort (void)
{
/**************************************
 *
 *	E X E C _ p o l l _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	Poll for abort flag (actually, most routines will check the
 *	flag before calling to save time).
 *
 **************************************/

if (!QLI_abort)
    return;

IBERROR (38); /* Msg38 execution terminated by signal */
}

DSC *EXEC_receive (
    MSG		message,
    PAR		parameter)
{
/**************************************
 *
 *	E X E C _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a message from a running request.
 *
 **************************************/
REQ	request;
STATUS	status_vector [20];

request = message->msg_request;

if (gds__receive (status_vector, 
		GDS_REF (request->req_handle),
		message->msg_number,
		message->msg_length,
		GDS_VAL (message->msg_buffer),
		0))
    db_error (request, status_vector);

if (!parameter)
    return NULL;

return EVAL_parameter (parameter);
}

void EXEC_send (
    MSG		message)
{
/**************************************
 *
 *	E X E C _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Send a message to a running request.  First, however, map
 *	any data to the message.
 *
 **************************************/
REQ	request;
STATUS	status_vector [20];

request = message->msg_request;

map_data (message);
if (gds__send (status_vector, 
		GDS_REF (request->req_handle),
		message->msg_number,
		message->msg_length,
		GDS_VAL (message->msg_buffer),
		0))
    db_error (request, status_vector);
}

void EXEC_start_request (
    REQ		request,
    MSG		message)
{
/**************************************
 *
 *	E X E C _ s t a r t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Start a request running.  If there is a message to be sent, do
 *	a start and send.
 *
 **************************************/
STATUS	status_vector [20];

if (message)
    {
    map_data (message);
    if (!gds__start_and_send (status_vector, 
		GDS_REF (request->req_handle),
		GDS_REF (request->req_database->dbb_transaction),
		message->msg_number,
		message->msg_length,
		GDS_VAL (message->msg_buffer),
		0))
	return;
    }
else
    if (!gds__start_request (status_vector, 
		GDS_REF (request->req_handle),
		GDS_REF (request->req_database->dbb_transaction),
		0))
	return;

db_error (request, status_vector);
}

void EXEC_top (
    NOD		node)
{
/**************************************
 *
 *	E X E C _ t o p
 *
 **************************************
 *
 * Functional description
 *	Execute a node type.
 *
 **************************************/

EXEC_execute (node);
}

static DSC *assignment (
    NOD		from_node,
    DSC		*to_desc,
    NOD		validation,
    NOD		initial,
    PAR		parameter)
{
/**************************************
 *
 *	a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Evaluate an expression and perform an assignment.  If something
 *	goes wrong and there was a prompt, try again.
 *
 **************************************/
DSC	*from_desc;
UCHAR	*p;
MSG	message;
USHORT	l, *missing_flag, trash;
int	*old_env, status;
jmp_buf	env;

old_env = QLI_env;
QLI_reprompt = FALSE;
QLI_prompt_count = 0;
missing_flag = &trash;

if (parameter)
    {
    message = parameter->par_message;
    missing_flag = (USHORT*) (message->msg_buffer + parameter->par_offset);
    }

status = setjmp (env);

if (status)
     if (QLI_abort || !QLI_prompt_count)
	{
	QLI_env = old_env;
	longjmp (QLI_env, status);
	}
    else
	{
	QLI_reprompt = TRUE;
	QLI_prompt_count = 0;
	}

#if (defined JPN_EUC || defined JPN_SJIS)
if (from_node->nod_type == nod_edit_blob)
   {
   from_node->nod_desc.dsc_scale = to_desc->dsc_scale;
   from_node->nod_desc.dsc_sub_type = to_desc->dsc_sub_type;
   }
#endif /* (defined JPN_EUC || defined JPN_SJIS) */


QLI_env = env;
from_desc = EVAL_value (from_node);

if (from_desc->dsc_missing & DSC_initial)
    from_desc = EVAL_value (initial);

/* If there is a value present, do any assignment; otherwise null fill */

if (*missing_flag = to_desc->dsc_missing = from_desc->dsc_missing)
    {
    p = from_desc->dsc_address;
    if (l = from_desc->dsc_length)
	do *p++ = 0; while (--l);
    }
else
    MOVQ_move (from_desc, to_desc);

if (validation && EVAL_boolean (validation) <= 0)
    IBERROR (39); /* Msg39 field validation error */

QLI_reprompt = FALSE;
QLI_env = old_env;

return from_desc;
}

static void commit_retaining (
    NOD		node)
{
/**************************************
 *
 *	c o m m i t _ r e t a i n i n g
 *
 **************************************
 *
 * Functional description
 *	Execute commit retaining statement for
 *	one or more named databases or all databases. 
 *	If there's more than one, prepare it.
 *
 **************************************/
DBB	database;
STATUS	status [20];
NOD	*ptr, *end;

/* If there aren't any open databases then obviously
   there isn't anything to commit. */

if (node->nod_count == 0 && !QLI_databases)
    return;

if (node->nod_type == nod_commit_retaining && 
    ((node->nod_count > 1) ||
    (node->nod_count == 0 && QLI_databases->dbb_next)))
    {
    node->nod_type = nod_prepare;
    commit_retaining (node);
    node->nod_type = nod_commit_retaining;
    } 
else if (node->nod_count == 1) 
    {
    database = (DBB) node->nod_arg[0];
    database->dbb_flags |= DBB_prepared;
    }
else
    QLI_databases->dbb_flags |= DBB_prepared;


if (node->nod_count == 0)
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	{
	if ((node->nod_type == nod_commit_retaining) &&
	    !(database->dbb_flags & DBB_prepared))
		ERRQ_msg_put (465, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
	else if (node->nod_type == nod_prepare)
	    database->dbb_flags |= DBB_prepared;
	if (database->dbb_transaction)
	    transaction_state (node, database);
	}
    return;
    }

for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
    {
    database = (DBB) *ptr;
    if ((node->nod_type == nod_commit_retaining) &&    
	!(database->dbb_flags & DBB_prepared))
	    ERRQ_msg_put (465, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
    else if (node->nod_type == nod_prepare)
	database->dbb_flags |= DBB_prepared;
    if (database->dbb_transaction)
	transaction_state (node, database);
    }
}


static int copy_blob (
    NOD		value,
    PAR		parameter)
{
/**************************************
 *
 *	 c o p y _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Copy a blob from one database to another.  If the blob can be
 *	successfully assigned, return FALSE and let somebody else just
 *	copy the blob ids.
 *
 **************************************/
CTX	context;
MSG	message;
REQ	from_request, to_request;
DBB	to_dbb, from_dbb;
DSC	*from_desc, *to_desc;
NOD	field;
SLONG	*from_blob, *to_blob, size, segment_count, max_segment;
STATUS	status_vector [20];
USHORT	bpb_length, length, buffer_length;
UCHAR	bpb [20], *p, fixed_buffer [4096], *buffer;
#if (defined JPN_EUC || defined JPN_SJIS)
USHORT	bpb2_length;
UCHAR	bpb2x[20], *p2, *bpb2;
#endif

if (value->nod_type == nod_form_field)
    return FORM_get_blob (value, parameter);

/* If assignment isn't from a field, there isn't a blob copy, so
   do a dumb assignment. */

if (value->nod_type != nod_field)
    return FALSE; 

/* Find the sending and receiving requests.  If they are the same
   and no filtering is necessary, a simple assignment will suffice. */

context = (CTX) value->nod_arg [e_fld_context];
from_request = context->ctx_request;
from_dbb = from_request->req_database;
message = parameter->par_message;
to_request = message->msg_request;
to_dbb = to_request->req_database;

from_desc = EVAL_value (value);
to_desc = EVAL_parameter (parameter);

if (to_dbb == from_dbb &&
    (!to_desc->dsc_sub_type ||
    from_desc->dsc_sub_type == to_desc->dsc_sub_type))
    return FALSE;

/* We've got a blob copy on our hands. */

if (!from_desc)
    {
    *to_desc->dsc_address = 0;
    return TRUE;
    }

to_blob = from_blob = NULL;

/* Format blob parameter block for the existing blob */

p = bpb;
*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_type;
*p++ = 2;
*p++ = from_desc->dsc_sub_type;
*p++ = from_desc->dsc_sub_type >> 8;
*p++ = gds__bpb_target_type;
*p++ = 2;
*p++ = to_desc->dsc_sub_type;
*p++ = to_desc->dsc_sub_type >> 8;
bpb_length = p - bpb;

#if (defined JPN_EUC || defined JPN_SJIS)
if ((to_desc->dsc_sub_type == 1) &&
    (to_desc->dsc_scale != from_desc->dsc_scale))
    {
    p2 = bpb2x;
    *p2++ = gds__bpb_version1;
    *p2++ = gds__bpb_source_interp;
    *p2++ = 2;
    *p2++ = from_desc->dsc_scale;
    *p2++ = from_desc->dsc_scale >> 8;
    bpb2_length = p2 - bpb2x;
    bpb2 = bpb2x;
    }
else
    {
    bpb2_length = 0;
    bpb2 = NULL;
    }

if (gds__create_blob2 (status_vector,
	GDS_REF (to_dbb->dbb_handle),
	GDS_REF (to_dbb->dbb_transaction),
	GDS_REF (to_blob),
	GDS_VAL (to_desc->dsc_address),
	bpb2_length,
	bpb2))
#else
if (gds__create_blob (status_vector,
	GDS_REF (to_dbb->dbb_handle),
	GDS_REF (to_dbb->dbb_transaction),
	GDS_REF (to_blob),
	GDS_VAL (to_desc->dsc_address)))
#endif
    ERRQ_database_error (to_dbb, status_vector);

if (gds__open_blob2 (status_vector,
	GDS_REF (from_dbb->dbb_handle),
	GDS_REF (from_dbb->dbb_transaction),
	GDS_REF (from_blob),
	GDS_VAL (from_desc->dsc_address),
	bpb_length,
	bpb))
    ERRQ_database_error (from_dbb, status_vector);

gds__blob_size (&from_blob, &size, &segment_count, &max_segment);

if (max_segment < sizeof (fixed_buffer))
    {
    buffer_length = sizeof (fixed_buffer);
    buffer = fixed_buffer;
    }
else
    {
    buffer_length = max_segment;
    buffer = gds__alloc (buffer_length);
#ifdef DEBUG_GDS_ALLOC
    /* We don't care about QLI specific memory leaks for V4.0 */
    gds_alloc_flag_unfreed ((void *) buffer);	/* QLI: don't care */
#endif

    }

while (!gds__get_segment (status_vector,
	    GDS_REF (from_blob),
	    GDS_REF (length),
	    buffer_length,
	    GDS_VAL (buffer)))
    if (gds__put_segment (status_vector,
	    GDS_REF (to_blob),
	    length,
	    GDS_VAL (buffer)))
	ERRQ_database_error (to_dbb, status_vector);

if (buffer != fixed_buffer)
    gds__free (buffer);

if (gds__close_blob (status_vector,
	GDS_REF (from_blob)))
    ERRQ_database_error (from_dbb, status_vector);

if (gds__close_blob (status_vector,
	GDS_REF (to_blob)))
    ERRQ_database_error (to_dbb, status_vector);

return TRUE;
}

static void db_error (
    REQ		request,
    STATUS	*status_vector)
{
/**************************************
 *
 *	d b _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	A database error has occurred.  Unless it was the result of
 *	an unwind, generate an error.
 *
 **************************************/

EXEC_poll_abort();
ERRQ_database_error (request->req_database, status_vector);
}

static void execute_abort (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	Abort a statement.
 *
 **************************************/
USHORT	l;
UCHAR	*ptr, temp [80], msg [128];

if (node->nod_count)
    {
    l = MOVQ_get_string (EVAL_value (node->nod_arg [0]), &ptr, temp, sizeof (temp));
    MOVQ_terminate (ptr, msg, l, sizeof (msg));
    ERRQ_error(40, msg, NULL, NULL, NULL, NULL); /* Msg40 Request terminated by statement: %s */
    }

IBERROR (41); /* Msg41 Request terminated by statement */
}

static void execute_assignment (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
NOD	to, from, reference, initial;
FLD	field;
PAR	parameter;

if (node->nod_flags & NOD_remote)
    return;

to = node->nod_arg [e_asn_to];
from = node->nod_arg [e_asn_from];  
initial = node->nod_arg [e_asn_initial];

if (to->nod_type == nod_field)
    {
    reference = to->nod_arg [e_fld_reference];
    parameter = reference->nod_import;
    if (to->nod_desc.dsc_dtype == dtype_blob &&
	from->nod_desc.dsc_dtype == dtype_blob &&
	copy_blob (from, parameter))
	return;
    }
else if (to->nod_type == nod_form_field)
    {
    FORM_put_field (from, to);
    return;
    }
else
    parameter = node->nod_import;

if (parameter)
    parameter = parameter->par_missing;

assignment (from, EVAL_value (to), 
	    node->nod_arg [e_asn_valid], initial, parameter);

/* propagate the missing flag in variable assignments */

if (to->nod_type == nod_variable)
    {
    field = (FLD) to->nod_arg [e_fld_field];
    if (to->nod_desc.dsc_missing & DSC_missing)
	field->fld_flags |= FLD_missing;
    else
	field->fld_flags &= ~FLD_missing;
    }
}

static void execute_for (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ f o r
 *
 **************************************
 *
 * Functional description
 *	Execute a FOR loop.  This may require that a request get
 *	started, a message sent, and a message received for each
 *	record.  At the other end of the spectrum, there may be
 *	absolutely nothing to do.
 *
 **************************************/
REQ	request;
MSG	message;
DSC	*desc;

/* If there is a request associated  with the node, start it and possibly
   send a message along with it. */

if (request = (REQ) node->nod_arg [e_for_request])
    EXEC_start_request (request, node->nod_arg [e_for_send]);
else if (message = (MSG) node->nod_arg [e_for_send])
    EXEC_send (message);

/* If there isn't a receive message, the body of the loop has been
   optimized out of existance.  So skip it. */

if (!(message = (MSG) node->nod_arg [e_for_receive]))
    goto count;

/* Receive messages in a loop until the end of file field comes up
   true. */

while (TRUE)
    {
    desc = EXEC_receive (message, node->nod_arg [e_for_eof]);
    if (*(USHORT*) desc->dsc_address)
	break;
    EXEC_execute (node->nod_arg [e_for_statement]);
    if (request && request->req_continue)
	EXEC_send (request->req_continue);
    } 

count:
if (QLI_count)
    print_counts (request);
}

static void execute_modify (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Execute a modify statement.  This probably involves sending a
 *	message to the running request and executing a sub-statement.
 *
 **************************************/
MSG	message;


if (node->nod_flags & NOD_remote)
    return;

if (message = (MSG) node->nod_arg [e_mod_send]) 
    set_null (message);

EXEC_execute (node->nod_arg [e_mod_statement]);

if (message)
    EXEC_send (message);
}

static void execute_output (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ o u t p u t
 *
 **************************************
 *
 * Functional description
 *	Open output stream to re-direct output.
 *
 **************************************/
PRT	print;
int	*old_env, status;
jmp_buf	env;

print = (PRT) node->nod_arg [e_out_print];
print->prt_file = EXEC_open_output (node);

/* Set up error handling */

old_env = QLI_env;
QLI_env = env;

status = setjmp (QLI_env);

if (status)
    {
    if (print->prt_file)
	ib_fclose ((IB_FILE*) print->prt_file);
    QLI_env = old_env;
    longjmp (QLI_env, status);
    }

/* Finally, execute the query */

EXEC_execute (node->nod_arg [e_out_statement]);
QLI_env = old_env;
ib_fclose ((IB_FILE*) print->prt_file);
}

static void execute_print (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Execute a print statement.  Since the loop has been factored
 *	out, this should be easy.
 *
 **************************************/

if (node->nod_arg [e_prt_header])
    {
    FMT_put (node->nod_arg [e_prt_header], node->nod_arg [e_prt_output]);
    node->nod_arg [e_prt_header] = NULL;
    }

FMT_print (node->nod_arg [e_prt_list], node->nod_arg [e_prt_output]);
}

static void execute_repeat (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ r e p e a t 
 *
 **************************************
 *
 * Functional description
 *	Execute a REPEAT statement.  In short, loop.
 *
 **************************************/
SLONG	count;

count = MOVQ_get_long (EVAL_value (node->nod_arg [e_rpt_value]), 0);

while (--count >= 0)
    EXEC_execute (node->nod_arg [e_rpt_statement]);
}

static void execute_store (
    NOD		node)
{
/**************************************
 *
 *	e x e c u t e _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Execute a STORE statement.  This may involve starting
 *	a request, sending a message, and executing a sub-statement.
 *	On the other hand, it may be a mody no-op if the statement
 *	is part of another request and doesn't need any data from
 *	here.
 *
 **************************************/
REQ	request;
MSG	message;

if (message = (MSG) node->nod_arg [e_sto_send])
    set_null (message);

if (!(node->nod_flags & NOD_remote))
    EXEC_execute (node->nod_arg [e_sto_statement]);

/* If there is a request associated  with the node, start it and possibly
   send a message along with it. */

if (request = (REQ) node->nod_arg [e_sto_request])
    EXEC_start_request (request, node->nod_arg [e_sto_send]);
else if (message)
    EXEC_send (message);

if (QLI_count)
    print_counts (request);
}

static void map_data (
    MSG		message)
{
/**************************************
 *
 *	m a p _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Map data to a message in preparation for sending.
 *
 **************************************/
PAR	parameter, missing_parameter;
NOD	from;
DSC	*desc;
USHORT	*missing_flag;

for (parameter = message->msg_parameters; parameter; 
    parameter = parameter->par_next)
    {
    desc = &parameter->par_desc;
    desc->dsc_address = message->msg_buffer + parameter->par_offset;
    if (missing_parameter = parameter->par_missing)
	{
	missing_flag = (USHORT*) (message->msg_buffer + missing_parameter->par_offset);
	*missing_flag = (desc->dsc_missing & DSC_missing) ? DSC_missing : 0;
	}

    from = parameter->par_value;
    if (desc->dsc_dtype == dtype_blob && copy_blob (from, parameter))
	continue;
    assignment (from, desc, NULL_PTR, NULL_PTR, parameter->par_missing);
    }
}

static void print_counts (
    REQ		request)
{
/**************************************
 *
 *	p r i n t _ c o u n t s
 *
 **************************************
 *
 * Functional description
 *	Print out the count of records affected 
 *	by the last statement.
 *
 **************************************/
UCHAR	item;
int	length;
ULONG	number;
STATUS	status_vector[20];
SCHAR	count_buffer [COUNT_ITEMS * 7 + 1], *c;

if (gds__request_info (
	status_vector,
	GDS_REF (request->req_handle),
	0,
	sizeof (count_info),
	count_info,
	sizeof (count_buffer),
	count_buffer))
    return;

/* print out the counts of any records affected */

for (c = count_buffer; *c != gds__info_end; c += length)
    {
    item = *c++;
    length = gds__vax_integer (c, 2);
    c += 2;
    number = gds__vax_integer (c, length);

    if (number) 
        switch (item)
            {
            case gds__info_req_select_count:
 	        ib_printf ("\nrecords selected: %ld\n", number);
	        break;
	
            case gds__info_req_insert_count:
 	        ib_printf ("records inserted: %ld\n", number);
	        break;
	
            case gds__info_req_update_count:
 	        ib_printf ("records updated: %ld\n", number);
	        break;
	
            case gds__info_req_delete_count:
 	        ib_printf ("records deleted: %ld\n", number);
	        break;
	
            default:	
                return;
            }                  
    }
}

static void set_null (
    MSG		message)
{
/**************************************
 *
 *	s e t _ n u l l
 *
 **************************************
 *
 * Functional description
 *	Set all parameters of an outgoing
 *	message to null - this allows conditional
 *	assignments to work in store and modify
 *	statements.
 *
 **************************************/
PAR	parameter;
NOD	from;
DSC	*desc;
UCHAR	*p;

for (parameter = message->msg_parameters; parameter; 
    parameter = parameter->par_next)
    {
    from = parameter->par_value;
    if (from->nod_type == nod_field)
	{
	desc = EVAL_value (from);
	desc->dsc_missing |= DSC_missing;
	}
    else
	from->nod_desc.dsc_missing |= DSC_missing;
    }
}


static void transaction_state (
    NOD	node,
    DBB	database)
{
/**************************************
 *
 *	t r a n s a c t i o n _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	
 *	set the state of the working transaction
 *	in a particular database to prepared or
 *	committed.
 *
 **************************************/
STATUS	status [20];

if (database->dbb_transaction)
    {
    if (node->nod_type == nod_commit_retaining)
        {
	if (gds__commit_retaining (status,
	    GDS_REF (database->dbb_transaction)))
	ERRQ_database_error (database, status);
        }
    else if (node->nod_type == nod_prepare)
	{
	if (gds__prepare_transaction (status,
	    GDS_REF (database->dbb_transaction)))
	ERRQ_database_error (database, status);
        }
    }
}
