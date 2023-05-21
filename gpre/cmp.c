/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		cmp.c
 *	DESCRIPTION:	Request compiler
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

#include <stdlib.h>
#include <string.h>
#include "../jrd/gds.h"
#include "gpre.h"
#include "../gpre/form.h"
#include "../jrd/align.h"
#include "../gpre/cmd_proto.h"
#include "../gpre/cme_proto.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/met_proto.h"
#include "../gpre/msc_proto.h"
#include "../gpre/par_proto.h"

extern TEXT	*ident_pattern, *utility_name, *count_name, *slack_name,
		*transaction_name;

static void	cmp_any (register REQ);
static void	cmp_assignment (register NOD, register REQ);
static void	cmp_blob (BLB, BOOLEAN);
static void	cmp_blr (REQ);
static void	cmp_erase (ACT, REQ);
static void	cmp_fetch (ACT);
static void	cmp_field (REQ, FLD, REF);
static void	cmp_for (register REQ);
static void	cmp_form (REQ);
static void	cmp_loop (register REQ);
static void	cmp_menu (REQ);
static void	cmp_modify (ACT, register REQ);
static void	cmp_port (register POR, register REQ);
static void	cmp_procedure (REQ);
static void	cmp_ready (REQ);
static void	cmp_sdl_fudge (REQ, SLONG);
static BOOLEAN	cmp_sdl_loop (REQ, USHORT, SLC, ARY);
static void	cmp_sdl_number (REQ, SLONG);
static void	cmp_sdl_subscript (REQ, USHORT, SLC, ARY);
static void	cmp_sdl_value (REQ, NOD);
static void	cmp_set_generator (REQ); 
static void	cmp_slice (REQ);
static void	cmp_store (register REQ);
static void	expand_references (register REF);
static POR	make_port (register REQ, register REF);
static void	make_receive (register POR, register REQ);
static void	make_send (register POR, register REQ);
static void	update_references (register REF);

static NOD	lit0, lit1;
static FLD	eof_field, count_field, slack_byte_field;
static USHORT	next_ident;

#define STUFF(blr)	*request->req_blr++ = (blr)
#define STUFF_WORD(blr) {STUFF (blr); STUFF (blr >> 8);}
#define STUFF_LONG(blr) {STUFF (blr); STUFF (blr >> 8); STUFF (blr >>16); STUFF (blr >> 24);}
#define STUFF_INT(blr)	STUFF (blr); STUFF (blr >> 8); STUFF (blr >> 16); STUFF (blr >> 24)

#define MAX_TPB		4000
#define BLR_LENGTH	request->req_length = request->req_blr - request->req_base; request->req_blr = request->req_base

void CMP_check (
    register REQ	request,
    SSHORT		min_reqd)
{
/**************************************
 *
 *	C M P _ c h e c k 
 *
 **************************************
 *
 * Functional description
 *	Check to make sure that generated blr string is not about to
 *	over the memory allocated for it.  If so, allocate an extra
 *	couple of hundred bytes to be safe.
 *
 **************************************/
UCHAR	*p, *q, *old;
int	length, n;

length = request->req_blr - request->req_base;
if (!(min_reqd) && (length < request->req_length - 100))
    return;

n = ((length + min_reqd + 100) > request->req_length * 2) ?
    length + min_reqd + 100 : request->req_length * 2;

q = old = request->req_base;
request->req_base = p = (UCHAR*) ALLOC (n);
request->req_length = n;
request->req_blr = request->req_base + length;

do *p++ = *q++; while (--length);

FREE (old);
}

void CMP_compile_request (
    register REQ	request)
{
/**************************************
 *
 *	C M P _ c o m p i l e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Compile a single request, but do not generate any text.
 *	Generate port blocks, assign parameter numbers, message
 *	numbers, and internal idents.  Compute length of request.
 *
 **************************************/
POR	port;
REF	reference;
ACT	action;
BLB	blob;
UPD	update;
FLD	direction_field, offset_field;

/* if there isn't a request handle specified, make one! */

if (!request->req_handle && (request->req_type != REQ_procedure))
    {
    request->req_handle = (TEXT*) ALLOC (20);
    sprintf (request->req_handle, ident_pattern, CMP_next_ident());
    }

if (!request->req_trans)
    request->req_trans = transaction_name;

if (!request->req_request_level)
    request->req_request_level = "0";

request->req_ident = CMP_next_ident();

/* If this is an SQL blob cursor, compile the blob and get out fast. */

if (request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
    {
    for (blob = request->req_blobs; blob; blob = blob->blb_next)
	cmp_blob (blob, TRUE);
    return;
    }
			
/* Before we get too far, make sure an eof field has been
   constructed.  If not, do so now. */

if (!eof_field)
    {
    eof_field = MET_make_field (utility_name, dtype_short, sizeof (SSHORT), FALSE);
    count_field = MET_make_field (count_name, dtype_long, sizeof (SLONG), FALSE);
    slack_byte_field = MET_make_field (slack_name, dtype_text, 1, FALSE);
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_value = "0";
    lit0 = MSC_unary (nod_literal, reference);
    
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_value = "1";
    lit1 = MSC_unary (nod_literal, reference);
    }

/* Handle different request types differently */

switch (request->req_type)
    {
    case REQ_create_database:
    case REQ_ddl:
	CMD_compile_ddl (request);
	return;

    case REQ_menu:
	cmp_menu (request);
	return;

    case REQ_slice:
	cmp_slice (request);
	return;

    case REQ_ready:
	cmp_ready (request);
	return;

    case REQ_form:
	cmp_form (request);
	return;

    case REQ_procedure:
	cmp_procedure (request);
	return;
    }
/* expand any incomplete references or values */

expand_references (request->req_references);
expand_references (request->req_values);

/* Initialize the blr string */

request->req_blr = request->req_base = (UCHAR*) ALLOC (500);
request->req_length = 500;
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);

/* If there are values to be transmitted, make a port
   to hold them */

if (request->req_values)
    request->req_vport = make_port (request, request->req_values);

#ifdef SCROLLABLE_CURSORS
/* If there is an asynchronous message to be sent, make a port for it */

if (request->req_flags & REQ_sql_cursor &&
    request->req_database->dbb_base_level >= 5)
    {
    direction_field = MET_make_field ("direction", dtype_short, sizeof (SSHORT), FALSE);
    offset_field = MET_make_field ("offset", dtype_long, sizeof (SLONG), FALSE);

    reference = request->req_avalues;
    reference->ref_field = direction_field;
    reference = reference->ref_next;
    reference->ref_field = offset_field;

    request->req_aport = make_port (request, request->req_avalues);
    }
#endif

/* If this is a FOR type request, an eof field reference needs
   to be generated.  Do it. */

if (request->req_type == REQ_for ||
    request->req_type == REQ_cursor ||
    request->req_type == REQ_any)
    {
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_field = eof_field;
    reference->ref_next = request->req_references;
    }
else if (request->req_type == REQ_mass_update)
    {
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_field = count_field;
    reference->ref_next = request->req_references;
    }
else
    reference = request->req_references;

/* Assume that a general port needs to be constructed. */

if ((request->req_type != REQ_insert) && (request->req_type != REQ_store2)
	&& (request->req_type != REQ_set_generator))
    request->req_primary = port = make_port (request, reference);

/* Loop thru actions looking for something interesting to do.  Not
   all action types are "interesting", so don't worry about missing
   ones. */

for (action = request->req_actions; action; action = action->act_next)
    switch (action->act_type)
	{
	case ACT_modify:
	case ACT_erase:
	case ACT_update:
	    update = (UPD) action->act_object;
 	    expand_references (update->upd_references);
	    update->upd_port = make_port (request, update->upd_references);
	    if (!request->req_sync)
		request->req_sync = make_port (request, NULL_PTR);
	    break;
	case ACT_store:
	    if (request->req_type == REQ_store2)
		{
		request->req_primary = make_port (request, action->act_object);
		update_references (request->req_primary->por_references);
		}
	    break;
	case ACT_store2:
	    update = (UPD) action->act_object;
 	    expand_references (update->upd_references);
	    update->upd_port = make_port (request, update->upd_references);
	    update_references (update->upd_port->por_references);
	    break;
	case ACT_select:
	case ACT_fetch:
	    cmp_fetch (action);
	    break; 

	}
    
cmp_blr (request);
STUFF (blr_eoc);

/* Compute out final blr lengths */

request->req_length = request->req_blr - request->req_base;
request->req_blr = request->req_base;

/* Finally, assign identifiers to any blobs that may have been referenced */

for (blob = request->req_blobs; blob; blob = blob->blb_next)
    cmp_blob (blob, FALSE);
}

CMP_display_code (
    FINT	display,
    REF		reference)
{
/**************************************
 *
 *	C M P _ d i s p l a y _ c o d e
 *
 **************************************
 *
 * Functional description
 *	CMP_display_code is called by the code generators to compute
 *	the option words code for the form DISPLAY statement on a
 *	field by field basis.  If the user has specificed explicit
 *	override of the field, -1 is returned.
 *
 **************************************/
int	code;

if (MSC_member (reference, display->fint_override_fields))
    return -1;

code = 0;

/* Handle display attribute */

if (display->fint_flags & FINT_display_all)
    code |= PYXIS__OPT_DISPLAY;
else
    if (MSC_member (reference, display->fint_display_fields))
	code |= PYXIS__OPT_DISPLAY;

/* Handle update attribute */

if (display->fint_flags & FINT_update_all)
    code |= PYXIS__OPT_UPDATE;
else
    if (MSC_member (reference, display->fint_update_fields))
	code |= PYXIS__OPT_UPDATE;

/* Handle wakeup attribute */

if (display->fint_flags & FINT_wakeup_all)
    code |= PYXIS__OPT_WAKEUP;
else
    if (MSC_member (reference, display->fint_wakeup_fields))
	code |= PYXIS__OPT_WAKEUP;

/* Handle cursor position attribute */

if (MSC_member (reference, display->fint_position_fields))
     code |= PYXIS__OPT_POSITION;

return code;
}

void CMP_external_field (
    REQ		request,
    FLD		field)
{
/**************************************
 *
 *	C M P _ e x t e r n a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Stuff field datatype info into request.
 *	Text fields are not remapped to process text type.
 *	This is used by the CAST & COLLATE operators to
 *	indicate cast datatypes.
 *
 **************************************/

switch (field->fld_dtype)
    {
    case dtype_cstring:
	STUFF (blr_text2);
	STUFF_WORD (field->fld_ttype);
	STUFF_WORD (field->fld_length - 1);
	break;

    case dtype_text:
	STUFF (blr_text2);
	STUFF_WORD (field->fld_ttype);
	STUFF_WORD (field->fld_length);
	break;

    case dtype_varying:
	STUFF (blr_varying2);
	STUFF_WORD (field->fld_ttype);
	STUFF_WORD (field->fld_length);
	break;

    default:
	cmp_field (request, field, NULL_PTR);
	break;
    }
}

void CMP_init (void)
{
/**************************************
 *
 *	C M P _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize (or re-initialize) for request compilation.  This is
 *	called at most once per module.
 *
 **************************************/

lit0 = lit1 = NULL;
count_field = slack_byte_field = eof_field = NULL;
next_ident = 0;
}

USHORT CMP_next_ident (void)
{
/**************************************
 *
 *	C M P _ n e x t _ i d e n t
 *
 **************************************
 *
 * Functional description
 *	Give out next identifier.
 *
 **************************************/

return next_ident++;
}

void CMP_stuff_symbol (
    REQ		request,
    SYM		symbol)
{
/**************************************
 *
 *	C M P _ s t u f f _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Stuff a symbol.
 *
 **************************************/
TEXT	*p;

STUFF (strlen (symbol->sym_string));

for (p = symbol->sym_string; *p; p++)
    STUFF (*p);
}

void CMP_t_start (
    register TRA	trans)
{
/**************************************
 *
 *	C M P _ t _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Take a transaction block with (potentially) a
 *	lot of relation lock blocks, and generate TPBs
 *
 *	We'll always generate TPB's, and link them
 *	into the DBB for that database so they get
 *	generated.  If there's no lock list, we generate
 *	a simple TPB for every database in the program.  
 *	If there is a lock list, we generate a more complex
 *	TPB for each database referenced.
 *
 **************************************/
UCHAR	*text, *p, *q, rrl_buffer[MAX_TPB], tpb_buffer [1 + MAX_TRA_OPTIONS];
DBB	database;
RRL	lock_block;
USHORT	buff_len, tpb_len;
TPB	tpb;

/* fill out a standard tpb buffer ahead of time so we know
   how large it is */

text = tpb_buffer;
*text++ = gds__tpb_version1;
*text++ = (trans->tra_flags & TRA_ro) ? gds__tpb_read : gds__tpb_write;
if (trans->tra_flags & TRA_con)
    *text++ = gds__tpb_consistency;
else if (trans->tra_flags & TRA_read_committed)
    *text++ = gds__tpb_read_committed;
else
    *text++ = gds__tpb_concurrency;
*text++ = (trans->tra_flags & TRA_nw) ? gds__tpb_nowait : gds__tpb_wait;

if (trans->tra_flags & TRA_read_committed)
    *text++ = (trans->tra_flags & TRA_rec_version) ? gds__tpb_rec_version : gds__tpb_no_rec_version;

if (trans->tra_flags & TRA_autocommit)
    *text++ = gds__tpb_autocommit;
if (trans->tra_flags & TRA_no_auto_undo)
    *text++ = gds__tpb_no_auto_undo;
*text = 0;
tpb_len = text - tpb_buffer;    

for (database = isc_databases; database; database = database->dbb_next)
    {
    /* 
     * figure out if this is a simple transaction or a reserving
     * transaction.  Allocate a TPB of the right size in either
     * case.
     */

    if (trans->tra_flags & TRA_inc) 
	{
	if (database->dbb_flags & DBB_in_trans)
	    {
	    tpb = (TPB) ALLOC (TPB_LEN (tpb_len));
	    tpb->tpb_length = tpb_len;
	    database->dbb_flags &= ~DBB_in_trans;
	    }
	else
	    continue;
	}
    else if (!(trans->tra_flags & TRA_rrl))
	{
	tpb = (TPB) ALLOC (TPB_LEN (tpb_len));
	tpb->tpb_length = tpb_len;
	}
    else if (database->dbb_rrls)
	{
	p = rrl_buffer;
	for (lock_block = database->dbb_rrls; lock_block; 
	     lock_block = lock_block->rrl_next)
	    {
	    *p++ = lock_block->rrl_lock_mode;
	    q = (UCHAR*) lock_block->rrl_relation->rel_symbol->sym_string;
	    *p++ = strlen (q);
	    while (*q)
		*p++ = *q++;
	    *p++ = lock_block->rrl_lock_level;
	    }
	*p = 0;
	buff_len  = (p - rrl_buffer); 
	tpb = (TPB) ALLOC (TPB_LEN (buff_len + tpb_len));
	tpb->tpb_length = buff_len + tpb_len;
	database->dbb_rrls = NULL;
	}
    else			/* this database isn't referenced */ 
	continue;

    /* link this into the TPB chains (TRA and DBB)   */

    tpb->tpb_database = database;
    tpb->tpb_dbb_next = database->dbb_tpbs;
    database->dbb_tpbs = tpb;
    tpb->tpb_tra_next = trans->tra_tpb;
    trans->tra_tpb = tpb;
    trans->tra_db_count++;

    /* fill in the standard TPB and concatenate the relation names */
 
    tpb->tpb_ident = CMP_next_ident();

    text = tpb->tpb_string;
    for (p = tpb_buffer; *p;)
	*text++ = *p++;
    if (trans->tra_flags & TRA_rrl)
	strcpy (text, rrl_buffer);
    }
}

static void cmp_any (
   register REQ	request)
{
/**************************************
 *
 *	c m p _ a n y
 *
 **************************************
 *
 * Functional description
 *	Generate blr tree for free standing ANY expression.
 *
 **************************************/
NOD		value;
POR		port;
REF		reference, source;

/* Build the primary send statement */

port = request->req_primary;
make_send (port, request);

STUFF (blr_if);
STUFF (blr_any);
CME_rse (request->req_rse, request);

value = MSC_unary (nod_value, port->por_references);

/* Make a send to signal end of file */

STUFF (blr_assignment);
CME_expr (lit1, request);
CME_expr (value, request);

STUFF (blr_assignment);
CME_expr (lit0, request);
CME_expr (value, request);

if (port = request->req_vport)
    for (reference = port->por_references; reference; 
	 reference = reference->ref_next)
	{
	reference->ref_source = source = (REF) ALLOC (REF_LEN);
	source->ref_ident = CMP_next_ident();
	}
}

static void cmp_assignment (
    register NOD	node,
    register REQ	request)
{
/**************************************
 *
 *	c m p _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Compile a build assignment statement.
 *
 **************************************/

CMP_check (request, 0);
STUFF (blr_assignment);
CME_expr (node->nod_arg [0], request);
CME_expr (node->nod_arg [1], request);
}

static void cmp_blob (
    BLB		blob,
    BOOLEAN	sql_flag)
{
/**************************************
 *
 *	c m p _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Compile a blob parameter block, if required.
 *
 **************************************/
UCHAR	*p;
REF	reference;

blob->blb_ident = CMP_next_ident();
blob->blb_buff_ident = CMP_next_ident();
blob->blb_len_ident = CMP_next_ident();

if (sql_flag)
    for (reference = blob->blb_reference; reference; reference = reference->ref_next)
	reference->ref_port = make_port (reference->ref_context->ctx_request, reference);

if (!blob->blb_const_to_type && !blob->blb_type)
    return;

blob->blb_bpb_ident = CMP_next_ident();
p = blob->blb_bpb;
*p++ = gds__bpb_version1;

if (blob->blb_const_to_type)
    {
    *p++ = gds__bpb_target_type;
    *p++ = 2;
    *p++ = blob->blb_const_to_type;
    *p++ = blob->blb_const_to_type >> 8;
    }

if (blob->blb_const_from_type)
    {
    *p++ = gds__bpb_source_type;
    *p++ = 2;
    *p++ = blob->blb_const_from_type;
    *p++ = blob->blb_const_from_type >> 8;
    }

if (blob->blb_type)
    {
    *p++ = gds__bpb_type;
    *p++ = 1;
    *p++ = blob->blb_type;
    }

if (blob->blb_from_charset)
    {
    /* create bpb instruction for source character set */

    *p++ = gds__bpb_source_interp;
    *p++ = 2;
    *p++ = blob->blb_from_charset;
    *p++ = blob->blb_from_charset >> 8;
    }

if (blob->blb_to_charset)
    {

    /* create bpb instruction for target character set */

    *p++ = gds__bpb_target_interp;
    *p++ = 2;
    *p++ = blob->blb_to_charset;
    *p++ = blob->blb_to_charset >> 8;
    }

blob->blb_bpb_length = p - blob->blb_bpb;
}

static void cmp_blr (
    REQ		request)
{
/**************************************
 *
 *	c m p _ b l r
 *
 **************************************
 *
 * Functional description
 *	Build a request tree for a request.
 *
 **************************************/
POR	port;

STUFF (blr_begin);

/* build message definition for each port */

for (port = request->req_ports; port; port = port->por_next)
    cmp_port (port, request);

/* See if there is a receive to be built */

if ((request->req_type == REQ_store) || (request->req_type == REQ_store2))
    port = request->req_primary;
else
    port = request->req_vport;

if (port)
    make_receive (port, request);

/* Compile up request */

switch (request->req_type)
    {
    case REQ_cursor:
    case REQ_for :
	cmp_for (request);
	break;

    case REQ_insert:
    case REQ_store :
    case REQ_store2 :
	cmp_store (request);
	break;

    case REQ_mass_update :
	cmp_loop (request);
	break;

    case REQ_any:
	cmp_any (request);
	break;

    case REQ_set_generator:
	cmp_set_generator (request);
	break;

    default :
	CPR_error ("*** unknown request type node ***");
    }

STUFF (blr_end);
}

static void cmp_erase (
    ACT			action,
    REQ			request)
{
/**************************************
 *
 *	c m p _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Generate blr for ERASE action.
 *
 **************************************/
UPD	update;
CTX	source;

update = (UPD) action->act_object;
source = update->upd_source;
make_receive (update->upd_port, request);

if (action->act_error)
    STUFF (blr_handler);

STUFF (blr_erase); STUFF (source->ctx_internal);
}

static void cmp_fetch (
    ACT		action)
{
/**************************************
 *
 *	c m p _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Go over an SQL fetch list and expand
 *	references to indicator variables.
 *	Not tough.
 *
 **************************************/
REF	var;
NOD	*ptr, *end, list;

if (!(list = (NOD) action->act_object))
    return;

if (list->nod_type != nod_list)
    CPR_error ("invalid fetch action");

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    {
    var = (REF) *ptr;
    if (var->ref_null_value && !var->ref_null)
	if (var->ref_friend)
	    var->ref_null = var->ref_friend->ref_null;
    }
}

static void cmp_field (
    REQ		request,
    FLD		field,
    REF		reference)
{
/**************************************
 *
 *	c m p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Stuff field datatype info into request.
 *
 **************************************/
TEXT	s [50];

if (reference && reference->ref_value && (reference->ref_flags & REF_array_elem))
    field = field->fld_array;

assert (field != NULL);
assert (field->fld_dtype);
assert (field->fld_length);

switch (field->fld_dtype)
    {
    case dtype_cstring:
	if (!(field->fld_flags & FLD_charset))
	    {
	    STUFF (blr_cstring);
	    STUFF_WORD (field->fld_length);
	    }
	else
	    {
	    /* 3.2j has new, tagged blr instruction for cstring */

	    STUFF (blr_cstring2);
	    STUFF_WORD (field->fld_ttype);
	    STUFF_WORD (field->fld_length);
	    }
	break;

    case dtype_text:
	if (!(field->fld_flags & FLD_charset))
	    {
	    STUFF (blr_text);
	    STUFF_WORD (field->fld_length);
	    }
	else
	    {
	    /* 3.2j has new, tagged blr instruction for text too */

	    STUFF (blr_text2);
	    STUFF_WORD (field->fld_ttype);
	    STUFF_WORD (field->fld_length);
	    }
	break;

    case dtype_varying:
	if (!(field->fld_flags & FLD_charset))
	    {
	    STUFF (blr_varying);
	    STUFF_WORD (field->fld_length);
	    }
	else
	    {
	    /* 3.2j has new, tagged blr instruction for varying also */

	    STUFF (blr_varying2);
	    STUFF_WORD (field->fld_ttype);
	    STUFF_WORD (field->fld_length);
	    }
	break;

    case dtype_short:
	STUFF (blr_short);
	STUFF (field->fld_scale);
	break;

    case dtype_long:
	STUFF (blr_long);
	STUFF (field->fld_scale);
	break;

    case dtype_blob:
	if (request->req_type == REQ_form)
	    {
	    STUFF (blr_blob_id);
	    STUFF_WORD (field->fld_sub_type);
	    break;
	    }

    case dtype_quad:
	STUFF (blr_quad);
	STUFF (field->fld_scale);
	break;

/** Begin sql date/time/timestamp **/
    case dtype_sql_date:
	STUFF (blr_sql_date);
	break;

    case dtype_sql_time:
	STUFF (blr_sql_time);
	break;

    case dtype_timestamp:
	STUFF (blr_timestamp);
	break;
/** Begin sql date/time/timestamp **/

    case dtype_int64:
	STUFF (blr_int64);
	STUFF (field->fld_scale);
	break;

    case dtype_real:
	STUFF (blr_float);
	break;

    case dtype_double:
	if (sw_d_float)
	    STUFF (blr_d_float);
	else
	    STUFF (blr_double);
	break;

    default:
	sprintf (s, "datatype %d not understood", field->fld_dtype);
	CPR_error (s);
    }
}

static void cmp_for (
   register REQ	request)
{
/**************************************
 *
 *	c m p _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate blr tree for for statement
 *
 **************************************/
NOD		field, value;
POR		port;
register ACT	action;
register REF	reference;
BOOLEAN		updates;
CTX		context;

STUFF (blr_begin);
if (request->req_type == REQ_cursor && request->req_sync)
    make_receive (request->req_sync, request);

STUFF (blr_for);

if (request->req_flags & REQ_sql_cursor)
    {
     if (!request->req_rse->rse_sort && !request->req_rse->rse_reduced)
        for (context = (CTX) request->req_contexts; context;
                context = context->ctx_next)
            {
            if ((context->ctx_scope_level == request->req_scope_level) &&
            		context->ctx_procedure)
		{
		STUFF (blr_stall);
		break;
		}
            }
    }

CME_rse (request->req_rse, request);

/* Loop thru actions looking for primary port.  While we're at it,
   count the number of update actions. */

updates = FALSE;
for (action = request->req_actions; action; action = action->act_next)
    switch (action->act_type)
	{
	case ACT_modify :
	case ACT_update :
	case ACT_erase :
	    updates = TRUE;
	}
if (updates)
    STUFF (blr_begin);

/* Build the primary send statement */

port = request->req_primary;
make_send (port, request);

field = MAKE_NODE (nod_field, 1);
value = MAKE_NODE (nod_value, 1);
STUFF (blr_begin);

for (reference = port->por_references; reference;
    reference = reference->ref_next)
    {
    CMP_check (request, 0);
    if (reference->ref_master)
	continue;
    STUFF (blr_assignment);
    if (reference->ref_field == eof_field)
	{
	request->req_eof = reference;
	CME_expr (lit1, request);
	}
    else if (reference->ref_field == slack_byte_field)
	CME_expr (lit0, request);
    else
	{
	if (reference->ref_expr)
	    CME_expr (reference->ref_expr, request);
	else
	    {
	    field->nod_arg [0] = (NOD) reference;
	    CME_expr (field, request);
	    }
	}
    value->nod_arg [0] = (NOD) reference;
    CME_expr (value, request);
    }
STUFF (blr_end);

/* If there are any actions, handle them here */

if (updates)
    {
    STUFF (blr_label); STUFF (0); STUFF (blr_loop);
    STUFF (blr_select);
    make_receive (request->req_sync, request);
    STUFF (blr_leave); STUFF (0);

    for (action = request->req_actions; action; action = action->act_next)
	switch (action->act_type)
	    {
	    case ACT_endmodify :
	    case ACT_update :
		cmp_modify (action, request);
		break;
	    case ACT_erase :
		cmp_erase (action, request);
		break;
	    }
    STUFF (blr_end);
    STUFF (blr_end);
    }

/* Make a send to signal end of file */

make_send (port, request);
STUFF (blr_assignment);
CME_expr (lit0, request);
value->nod_arg [0] = (NOD) request->req_eof;
CME_expr (value, request);

STUFF (blr_end);
}

static void cmp_form (
    REQ		request)
{
/**************************************
 *
 *	c m p _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Compile a FORM request.  
 *	This has very little to do with a BLR request.
 *
 **************************************/
REF	reference, option, parent;
POR	port;
FLD	field;
FORM	form;

form = request->req_form;

if (!form->form_handle)
    {
    form->form_handle = (TEXT*) ALLOC (20);
    sprintf (form->form_handle, ident_pattern, CMP_next_ident());
    }

if (!request->req_form_handle)
    request->req_form_handle = form->form_handle;

request->req_blr = request->req_base = (UCHAR*) ALLOC (500);
request->req_length = 500;
STUFF (PYXIS__MAP_VERSION1);  

make_port (request, request->req_references);
port = request->req_ports;

/*  If the request had no references, we need to generate the 
    bogus, one-field reference structure within the routine
    but NOT generate the field into the blr message.  Gawd. */

reference = port->por_references;
if (reference->ref_field == eof_field)
    {
    STUFF (blr_message);
    STUFF (0);             /* port->por_msg_number */
    STUFF_WORD (0);        /* port->por_count      */
    }
else
    cmp_port (port, request);

if (field = form->form_field)
    {
    STUFF (PYXIS__MAP_SUB_FORM);
    CMP_stuff_symbol (request, field->fld_symbol);
    }

if (request->req_flags & REQ_transparent)
    STUFF (PYXIS__MAP_TRANSPARENT);

if (request->req_flags & REQ_form_tag)
    STUFF (PYXIS__MAP_TAG);

reference = port->por_references;
if (!(reference->ref_field == eof_field))
    for (; reference; reference = reference->ref_next)
	if (!(reference->ref_flags & REF_pseudo))
	    {
	    CMP_check (request, 0);
	    field = reference->ref_field;
	    option = reference->ref_null;
	    if (parent = reference->ref_friend)
		{
		STUFF (PYXIS__MAP_SUB_FIELD);
		CMP_stuff_symbol (request, parent->ref_field->fld_symbol);
		}
	    else
		STUFF ((option) ? PYXIS__MAP_FIELD2 : PYXIS__MAP_FIELD1);
	    CMP_stuff_symbol (request, field->fld_symbol);
	    STUFF_WORD (reference->ref_parameter);
	    if (option)
		STUFF_WORD (option->ref_parameter);
	    }

if (reference = request->req_eof)
    {
    STUFF (PYXIS__MAP_TERMINATOR);
    STUFF_WORD (reference->ref_parameter);
    }

if (reference = request->req_term_field)
    {
    STUFF (PYXIS__MAP_TERMINATING_FIELD);
    STUFF_WORD (reference->ref_parameter);
    }

if (reference = request->req_index)
    {
    STUFF (PYXIS__MAP_ITEM_INDEX);
    STUFF_WORD (reference->ref_parameter);
    }

STUFF (PYXIS__MAP_END);
request->req_length = request->req_blr - request->req_base;
request->req_blr = request->req_base;
}

static void cmp_loop (
    register REQ	request)
{
/**************************************
 *
 *	c m p _ l o o p
 *
 **************************************
 *
 * Functional description
 *	Compile a mass looping update statement.
 *
 **************************************/
NOD		node, list, *ptr, *end, counter;
POR		primary;
CTX		for_context, update_context;
register RSE	rse;
REF		reference;

node = request->req_node;
rse = request->req_rse;

primary = request->req_primary;
make_send (primary, request);
STUFF (blr_begin);
counter = MAKE_NODE (nod_value, 1);
for (reference = primary->por_references; reference; reference = reference->ref_next) 
    if (reference->ref_field == count_field)
	counter->nod_arg [0] = (NOD) reference;

STUFF (blr_assignment);
    CME_expr (lit0, request);
    CME_expr (counter, request);
    
for_context = rse->rse_context[0];
update_context = request->req_update;

STUFF (blr_for);
CME_rse (rse, request);
STUFF (blr_begin);

if (node->nod_type == nod_modify)
    {
    STUFF (blr_modify); STUFF (for_context->ctx_internal);
    STUFF (update_context->ctx_internal);
    list = node->nod_arg [0];
    STUFF (blr_begin);
    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	cmp_assignment (*ptr, request);
    STUFF (blr_end);
    }
else if (node->nod_type == nod_store)
    {
    update_context = (CTX) node->nod_arg [0];
    STUFF (blr_store);
    CME_relation (update_context, request);
    list = node->nod_arg1;
    STUFF (blr_begin);
    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	cmp_assignment (*ptr, request);
    STUFF (blr_end);
    }
else if (node->nod_type == nod_erase)
    {
    STUFF (blr_erase); 
    STUFF (for_context->ctx_internal);
    }

STUFF (blr_assignment);
    STUFF (blr_add);
	CME_expr (lit1, request);
	CME_expr (counter, request);
    CME_expr (counter, request);

STUFF (blr_end);
STUFF (blr_end);
}

static void cmp_menu (
    REQ		request)
{
/**************************************
 *
 *	c m p _ m e n u
 *
 **************************************
 *
 * Functional description
 *	Compile a MENU request.  This has very little to do
 *	with a BLR request.
 *
 **************************************/
ACT	action, temp;
TEXT	*p;
USHORT	l, count;

/* Do not generate anything for for_menu and form_menu_item requests.  */

if ( (request->req_flags & REQ_menu_for) || 
     (request->req_flags & REQ_menu_for_item) )
    return;

request->req_blr = request->req_base = (UCHAR*) ALLOC (500);
request->req_length = 500;
STUFF (PYXIS__MENU_VERSION1);
count = 0;

/* Reverse actions so menu will come out in right order */

for (temp = NULL; action = request->req_actions;)
    {
    request->req_actions = action->act_next;
    action->act_next = temp;
    temp = action;
    }

request->req_actions = temp;
    
if (request->req_flags & REQ_menu_tag)
    STUFF (PYXIS__MENU_HORIZONTAL);
else if (request->req_flags & REQ_menu_pop_up)
    STUFF (PYXIS__MENU_VERTICAL);

if (request->req_flags & REQ_transparent)
    STUFF (PYXIS__MENU_TRANSPARENT);

for (action = request->req_actions; action; action = action->act_next)
    switch (action->act_type)
	{
	case ACT_menu_entree:
	    STUFF (PYXIS__MENU_ENTREE);
	    action->act_count = ++count;
	    STUFF (action->act_count);
	    p = (TEXT*) action->act_object;
	    l = strlen (p);
	    STUFF (l);
	    while (*p)
		STUFF (*p++);
	    break;
	
	case ACT_menu:
	    STUFF (PYXIS__MENU_LABEL);
	    p = (TEXT*) action->act_object;
	    l = strlen (p);
	    STUFF (l);
	    while (*p)
		STUFF (*p++);
	    break;
	}

STUFF (PYXIS__MENU_END);
request->req_length = request->req_blr - request->req_base;
request->req_blr = request->req_base;
}

static void cmp_modify (
    ACT			action,
    register REQ	request)
{
/**************************************
 *
 *	c m p _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Generate a receive and modify tree for a modify action.
 *
 **************************************/
UPD	update;
NOD	list, field_node, *ptr, *end;
REF	reference;
POR	port;

update = (UPD) action->act_object;
port = update->upd_port;
make_receive (port, request);

if (action->act_error)
    STUFF (blr_handler);

STUFF (blr_modify); 
STUFF (update->upd_source->ctx_internal);
STUFF (update->upd_update->ctx_internal);

/* count the references and build an assignment block */

list = update->upd_assignments;
STUFF (blr_begin);

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    {
    field_node = (*ptr)->nod_arg [1];
    reference = (REF) field_node->nod_arg [0];
    if ((field_node->nod_type != nod_field) || !reference->ref_master)
	cmp_assignment (*ptr, request);
    }

STUFF (blr_end);
}

static void cmp_port (
    register POR	port,
    register REQ	request)
{
/**************************************
 *
 *	c m p _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Build a request tree for a request.
 *
 **************************************/
REF	reference;

/* build message definition for each port */

STUFF (blr_message);
STUFF (port->por_msg_number);
STUFF_WORD (port->por_count);

for (reference = port->por_references; reference;
    reference = reference->ref_next)
    {
    CMP_check (request, 0);
    cmp_field (request, reference->ref_field, reference);
    }
}

static void cmp_procedure (
    REQ		request)
{
/**************************************
 *
 *	c m p _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Compile a EXECUTE PROCEDURE request.  
 *
 **************************************/
ACT	action;
POR	port;
PRC	procedure;
REF	reference;
LLS	outputs, *list;
NOD	*ptr, *end, node;

action = request->req_actions;
procedure = (PRC) action->act_object;

/* Remember the order of the references.  The exec_proc blr verb
   requires parameters to be in parameter order which may be changed
   when there references are expanded. */

outputs = NULL;
if (reference = request->req_references)
    for (list = &outputs; reference; reference = reference->ref_next)
	{
	PUSH (reference, list);
	list = &(*list)->lls_next;
	}

/* Expand any incomplete references or values. */

expand_references (request->req_values);
expand_references (request->req_references);

request->req_blr = request->req_base = (UCHAR*) ALLOC (500);
request->req_length = 500;
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
STUFF (blr_begin);

if (request->req_values)
    {
    request->req_vport = make_port (request, request->req_values);
    request->req_values = request->req_vport->por_references;
    }
else
    request->req_count++;
request->req_primary = make_port (request, request->req_references);
request->req_references = request->req_primary->por_references;

/* build message definition for each port */

for (port = request->req_ports; port; port = port->por_next)
    cmp_port (port, request);

if (request->req_values)
    {
    STUFF (blr_begin);
    make_send (request->req_vport, request);
    }

if (sw_ids)
    {
    STUFF (blr_exec_pid);
    STUFF_WORD (procedure->prc_id);
    }
else
    {
    STUFF (blr_exec_proc);
    CMP_stuff_symbol (request, procedure->prc_symbol);
    }

if (procedure->prc_in_count)
    {
    STUFF_WORD (procedure->prc_in_count);
    ptr = request->req_node->nod_arg;
    for (end = ptr + procedure->prc_in_count; ptr < end; ptr++)
	CME_expr (*ptr, request);
    }
else
    STUFF_WORD (0);

if (request->req_references)
    {
    node = MAKE_NODE (nod_value, 1);
    STUFF_WORD (procedure->prc_out_count);
    for (; outputs; outputs = outputs->lls_next)
	{
	node->nod_arg [0] = outputs->lls_object;
	CME_expr (node, request);
	}
    }
else
    STUFF_WORD (0);

if (request->req_values)
    STUFF (blr_end);
STUFF (blr_end);
STUFF (blr_eoc);
request->req_length = request->req_blr - request->req_base;
request->req_blr = request->req_base;
}

static void cmp_ready (
    REQ	request)
{
/**************************************
 *
 *	c m p _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Generate parameter buffer for READY with
 *      buffercount.
 *
 **************************************/
DBB	db;
ACT	action;
TEXT	*p;
SSHORT	l;

db = request->req_database;

action = request->req_actions;
request->req_blr = request->req_base = ALLOC (250);
request->req_length = 250;
request->req_flags |= REQ_exp_hand;

STUFF (gds__dpb_version1);

if (db->dbb_allocation)
    {
    STUFF (gds__dpb_allocation);
    STUFF (4);
    STUFF_INT (db->dbb_allocation);
    }

if (db->dbb_pagesize)
    {
    STUFF (gds__dpb_page_size);
    STUFF (4);
    STUFF_INT (db->dbb_pagesize);
    }

if (db->dbb_buffercount)
    {
    STUFF (gds__dpb_num_buffers);
    STUFF (4);
    STUFF_INT (db->dbb_buffercount);
    }

if (db->dbb_buffersize)
    {
    STUFF (gds__dpb_buffer_length);
    STUFF (4);
    STUFF_INT (db->dbb_buffersize);
    }

if (db->dbb_users)
    {
    STUFF (gds__dpb_number_of_users);
    STUFF (4);
    STUFF_INT (db->dbb_users);
    }

if (db->dbb_c_user && !db->dbb_r_user)
    {
    STUFF (gds__dpb_user_name);
    l = strlen (db->dbb_c_user);
    STUFF (l);
    p = db->dbb_c_user;
    while (l--)
	STUFF (*p++);
    }

if (db->dbb_c_password && !db->dbb_r_password)
    {
    STUFF (gds__dpb_password);
    l = strlen (db->dbb_c_password);
    STUFF (l);
    p = db->dbb_c_password;
    while (l--)
	STUFF (*p++);
    }

if (db->dbb_c_sql_role && !db->dbb_r_sql_role)
    {
    STUFF (isc_dpb_sql_role_name);
    l = strlen (db->dbb_c_sql_role);
    STUFF (l);
    p = db->dbb_c_sql_role;
    while (l--)
	STUFF (*p++);
    }

if (db->dbb_c_lc_messages && !db->dbb_r_lc_messages)
    {
    /* Language must be an ASCII string */
    STUFF (gds__dpb_lc_messages);
    l = strlen (db->dbb_c_lc_messages);
    STUFF (l);
    p = db->dbb_c_lc_messages;
    while (l--)
	STUFF (*p++);
    }
    
if (db->dbb_c_lc_ctype && !db->dbb_r_lc_ctype)
    {
    /* Character Format must be an ASCII string */
    STUFF (gds__dpb_lc_ctype);
    l = strlen (db->dbb_c_lc_ctype);
    STUFF (l);
    p = db->dbb_c_lc_ctype;
    while (l--)
	STUFF (*p++);
    }

*request->req_blr = 0;
request->req_length = request->req_blr - request->req_base; 
if (request->req_length == 1)
    request->req_length = 0;
request->req_blr = request->req_base;
}

static void cmp_sdl_fudge (
    REQ		request,
    SLONG	lower_bound)
{
/**************************************
 *
 *	c m p _ s d l _ f u d g e
 *
 **************************************
 *
 * Functional description
 *	Build in a fudge to bias the language specific subscript to the
 *	declared array subscript [i.e. C subscripts are zero based].
 *
 **************************************/

switch (sw_language)
    {
    case lan_c:
    case lan_cxx:
    case lan_basic:
	if (!lower_bound)
	    break;
	STUFF (gds__sdl_add);
	cmp_sdl_number (request, lower_bound);
	break;

    case lan_fortran:
	if (lower_bound == 1)
	    break;
	STUFF (gds__sdl_add);
	cmp_sdl_number (request, lower_bound - 1);
	break;
    }
}

static BOOLEAN cmp_sdl_loop (
    REQ		request,
    USHORT	index,
    SLC		slice,
    ARY		array)
{
/**************************************
 *
 *	c m p _ s d l _ l o o p
 *
 **************************************
 *
 * Functional description
 *	Build an SDL loop for GET_SLICE/PUT_SLICE unless the upper and
 *	lower bounds are constant.  Return TRUE if a loop has been built,
 *	otherwise FALSE.
 *
 **************************************/
struct ary_repeat	*bounds;
struct slc_repeat	*ranges;

ranges = slice->slc_rpt + index;

if (ranges->slc_upper == ranges->slc_lower)
    return FALSE;

bounds = array->ary_rpt + index;
STUFF (gds__sdl_do2);
STUFF (slice->slc_parameters + index);
cmp_sdl_fudge (request, bounds->ary_lower);
cmp_sdl_value (request, ranges->slc_lower);
cmp_sdl_fudge (request, bounds->ary_lower);
cmp_sdl_value (request, ranges->slc_upper);

return TRUE;
}

static void cmp_sdl_number (
    REQ		request,
    SLONG	number)
{
/**************************************
 *
 *	c m p _ s d l _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Write the number in the 'smallest'
 *      form possible to the SDL string.
 *
 **************************************/

if ((number > -16) && (number < 15))
    {
    STUFF (gds__sdl_tiny_integer);
    STUFF (number);
    }
else if ((number > -32768) && (number < 32767))
    {
    STUFF (gds__sdl_short_integer);
    STUFF_WORD (number);
    }
else
    {
    STUFF (gds__sdl_long_integer);
    STUFF_LONG (number);
    }
}

static void cmp_sdl_subscript (
    REQ		request,
    USHORT	index,
    SLC		slice,
    ARY		array)
{
/**************************************
 *
 *	c m p _ s d l _ s u b s c r i p t
 *
 **************************************
 *
 * Functional description
 *	Build an SDL loop for GET_SLICE/PUT_SLICE unless the upper and
 *	lower bounds are constant.
 *
 **************************************/
struct slc_repeat	*ranges;

ranges = slice->slc_rpt + index;

if (ranges->slc_upper == ranges->slc_lower)
    {
    cmp_sdl_value (request, ranges->slc_upper);
    return;
    }

STUFF (gds__sdl_variable);
STUFF (slice->slc_parameters + index);
}

static void cmp_sdl_value (
    REQ		request,
    NOD		node)
{
/**************************************
 *
 *	c m p _ s d l _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Stuff a slice description language value.
 *
 **************************************/
REF	reference;

reference = (REF) node->nod_arg [0];

switch (node->nod_type)
    {
    case nod_literal:
	cmp_sdl_number (request, atoi (reference->ref_value));
	return;

    case nod_value:
	STUFF (gds__sdl_variable);
	STUFF (reference->ref_id);
	return;
    
    default:
	CPR_error ("cmp_sdl_value: node not understood");
    }
}

static void cmp_set_generator (
    REQ		request)
{
/**************************************
 *
 *	c m p _ s e t _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	generate blr for set generator
 *
 **************************************/
SGEN	setgen;
TEXT	*string;
SLONG	value;
SINT64	int64value;
ACT	action;

action = request->req_actions;

STUFF (blr_set_generator);
setgen = (SGEN) action->act_object;
string = setgen->sgen_name;
value = setgen->sgen_value;
int64value = setgen->sgen_int64value;
STUFF (strlen (string));
while (*string)
    STUFF (*string++);
STUFF (blr_literal);
if (setgen->sgen_dialect == SQL_DIALECT_V5)
    {
    STUFF (blr_long);
    STUFF (0);
    STUFF_WORD (value);
    STUFF_WORD (value >> 16);
    }
else
    {
    STUFF (blr_int64);
    STUFF (0);
    STUFF_WORD (int64value);
    STUFF_WORD (int64value>>16);
    STUFF_WORD (int64value>>32);
    STUFF_WORD (int64value>>48);
    }
}

static void cmp_slice (
    REQ		request)
{
/**************************************
 *
 *	c m p _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SLC	slice;
FLD	field, element;
ARY	array;
REF	reference;
USHORT	n;
BOOLEAN	loop_flags [16], *p;

slice = request->req_slice;
field = slice->slc_field;
element = field->fld_array;
array = field->fld_array_info;

/* Process variable references */

for (reference = request->req_values; reference; reference = reference->ref_next)
    reference->ref_id = slice->slc_parameters++;

request->req_blr = request->req_base = (UCHAR*) ALLOC (500);
request->req_length = 500;

STUFF (gds__sdl_version1);
STUFF (gds__sdl_struct);
STUFF (1);
cmp_field (request, element, NULL_PTR);

STUFF (gds__sdl_relation);
CMP_stuff_symbol (request, field->fld_relation->rel_symbol);

STUFF (gds__sdl_field);
CMP_stuff_symbol (request, field->fld_symbol);

for (n = 0, p = loop_flags; n < slice->slc_dimensions; n++, p++)
    *p = cmp_sdl_loop (request, n, slice, array);

STUFF (gds__sdl_element);
STUFF (1);
STUFF (gds__sdl_scalar);
STUFF (0);
STUFF (slice->slc_dimensions);

for (n = 0, p = loop_flags; n < slice->slc_dimensions; n++, p++)
    {
    if (!*p)
	cmp_sdl_fudge (request, array->ary_rpt [n].ary_lower);
    cmp_sdl_subscript (request, n, slice, array);
    }

STUFF (gds__sdl_eoc);
request->req_length = request->req_blr - request->req_base;
request->req_blr = request->req_base;
}

static void cmp_store (
    register REQ	request)
{
/**************************************
 *
 *	c m p _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate blr for a store request.
 *
 **************************************/
register POR	port;
register NOD	list, *ptr, *end;
register ACT	action;
register UPD	update;
SSHORT		count;

/* Make the store statement under the receive */

if (request->req_type == REQ_store2)
    STUFF (blr_store2);
else
    STUFF (blr_store);

CME_relation (request->req_contexts, request);

/* Make an assignment list */

list = request->req_node;
STUFF (blr_begin);

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    cmp_assignment (*ptr, request);

STUFF (blr_end);

if (request->req_type == REQ_store2)
    {
    /* whip through actions to find return list */

    for (action = request->req_actions;;action = action->act_next)
	if (action->act_type == ACT_store2)
	    break;
    STUFF (blr_begin);
    update = (UPD)action->act_object;
    port = update->upd_port;
    make_send (port, request);
    list = update->upd_assignments;
    if ((count = list->nod_count) > 1)
	STUFF (blr_begin);
    for (ptr = list->nod_arg, end = ptr + count; ptr < end; ptr++)
	cmp_assignment (*ptr, request);
    if (count > 1)
	STUFF (blr_end);
    STUFF (blr_end);
    }
}

static void expand_references (
    register REF	reference)
{
/**************************************
 *
 *	e x p a n d _ r e f e r e n c e s
 *
 **************************************
 *
 * Functional description
 *	During the parsing of an SQL statement
 *	we may have run into an indicator variable.
 *	If so, all we've got now is its name, and
 *	we really ought to build a full reference
 *	block for it before we forget.
 *
 **************************************/
REF	flag;

for (; reference; reference = reference->ref_next)
    {
    if (reference->ref_null_value && !reference->ref_null)
	{
	flag = (REF) ALLOC (REF_LEN);
	flag->ref_next = reference->ref_next;
	reference->ref_next = reference->ref_null = flag;
	flag->ref_master = reference;
	flag->ref_value = reference->ref_null_value;
	flag->ref_field = PAR_null_field();
	}
    }
}

static POR make_port (
    register REQ	request,
    register REF	reference)
{
/**************************************
 *
 *	m a k e _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Make up a port block and process a linked list
 *	of field references.
 *
 **************************************/
register POR	port;
FLD	field;
REF	misc, temp, alignments [3];
SSHORT	i;

port = (POR) ALLOC (POR_LEN);
port->por_ident = CMP_next_ident();
port->por_msg_number = request->req_count++;
port->por_next = request->req_ports;
request->req_ports = port;

/* Hmmm -- no references.  Not going to fly.  
   Make up a dummy reference. */

if (!reference)
    {
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_field = eof_field;
    }

misc = alignments [0] = alignments [1] = alignments [2] = NULL;

while (temp = reference)
    {
    reference = reference->ref_next;
    if (!(field = temp->ref_field))
	CPR_bugcheck ("missing prototype field for value");
    if (temp->ref_value && (temp->ref_flags & REF_array_elem))
	field = field->fld_array;
    if ((field->fld_length & 7) == 0)
	{
	temp->ref_next = alignments [2];
	alignments [2] = temp;
	}
    else if ((field->fld_length & 3) == 0)
	{
	temp->ref_next = alignments [1];
	alignments [1] = temp;
	}
    else if ((field->fld_length & 1) == 0)
	{
	temp->ref_next = alignments [0];
	alignments [0] = temp;
	}
    else
	{
	temp->ref_next = misc;
	misc = temp;
	}
    }

for (i = 0; i <= 2; i++)
    while (reference = alignments [i])
	{
	alignments [i] = reference->ref_next;
	reference->ref_next = misc;
	misc = reference;
	}

port->por_references = misc;

for (reference = misc; reference; reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (reference->ref_value && (reference->ref_flags & REF_array_elem))
	field = field->fld_array;
    reference->ref_ident = CMP_next_ident();
    reference->ref_parameter = port->por_count++;
    reference->ref_port = port;
    reference->ref_offset = port->por_length;
    port->por_length += field->fld_length;
    }

return port;
}

static void make_receive (
    register POR	port,
    register REQ	request)
{
/**************************************
 *
 *	m a k e _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Make a receive node for a given port.
 *
 **************************************/

STUFF (blr_receive); STUFF (port->por_msg_number);
}

static void make_send (
    register POR	port,
    register REQ	request)
{
/**************************************
 *
 *	m a k e _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Make a receive node for a given port.
 *
 **************************************/

STUFF (blr_send); STUFF (port->por_msg_number);
}

static void update_references (
    register REF	references)
{
/**************************************
 *
 *	u p d a t e _ r e f e r e n c e s
 *
 **************************************
 *
 * Functional description
 *	where a duplicate reference list is used
 *	to build the port, fix request_references
 *
 **************************************/
register REF	ref, source;

for (ref = references; ref; ref = ref->ref_next)
    {
    source = ref->ref_source;
    source->ref_port = ref->ref_port;
    source->ref_parameter = ref->ref_parameter;
    source->ref_ident = ref->ref_ident;
    }
}
