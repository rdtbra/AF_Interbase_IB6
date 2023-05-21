/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		compile.c
 *	DESCRIPTION:	Compile expanded statement into executable things
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

#include <string.h>
#include "../qli/dtr.h"
#include "../qli/compile.h"
#include "../qli/exe.h"
#include "../qli/report.h"
#include "../qli/form.h"
#include "../jrd/intl.h"
#include "../qli/all_proto.h"
#include "../qli/compi_proto.h"
#include "../qli/err_proto.h"
#include "../qli/forma_proto.h"
#include "../qli/meta_proto.h"
#include "../jrd/dsc_proto.h"

#define PROMPT_LENGTH	80

static NOD	compile_any (NOD, REQ, int);
static NOD	compile_assignment (NOD, REQ, int);
static void	compile_context (NOD, REQ, int);
static void	compile_control_break (BRK, REQ);
static NOD	compile_edit (NOD, REQ);
static NOD	compile_erase (NOD, REQ);
static NOD	compile_expression (NOD, REQ, int);
static NOD	compile_field (NOD, REQ, int);
static NOD	compile_for (NOD, REQ, int);
static NOD	compile_function (NOD, REQ, int);
static NOD	compile_if (NOD, REQ, int);
static NOD	compile_list_fields (NOD, REQ);
static NOD	compile_modify (NOD, REQ, int);
static NOD	compile_print (NOD, REQ);
static NOD	compile_print_list (NOD, REQ, LLS *);
static NOD	compile_prompt (NOD);
static NOD	compile_repeat (NOD, REQ, int);
static NOD	compile_report (NOD, REQ);
static REQ	compile_rse (NOD, REQ, int, MSG *, MSG *, DBB *);
static NOD	compile_statement (NOD, REQ, int);
static NOD	compile_statistical (NOD, REQ, int);
static NOD	compile_store (NOD, REQ, int);
static int	computable (NOD, REQ);
static void	make_descriptor (NOD, DSC *);
static MSG	make_message (REQ);
static void	make_missing_reference (PAR);
static PAR	make_parameter (MSG, NOD);
static NOD	make_reference (NOD, MSG);
static REQ	make_request (DBB);
static void	release_message (MSG);
static int	string_length (DSC *);

static LLS	print_items;
static TEXT	**print_header;
static BRK	report_control_break;

NOD CMPQ_compile (
    NOD	node)
{
/**************************************
 *
 *	C M P _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *	Compile a statement into something executable.
 *
 **************************************/

print_header = NULL;
print_items = NULL;
QLI_requests = NULL;
compile_statement (node, NULL_PTR, TRUE);

if (print_header)
    *print_header = FMT_format (print_items);

return node;
}

void CMP_alloc_temp (
    NOD		node)
{
/**************************************
 *
 *	C M P _ a l l o c _ t e m p
 *
 **************************************
 *
 * Functional description
 *	Allocate a data area for a node.
 *
 **************************************/
STR	string;

if (node->nod_desc.dsc_address)
    return;

string = (STR) ALLOCDV (type_str, node->nod_desc.dsc_length);
node->nod_desc.dsc_address = (UCHAR*) string->str_data;
}

int CMP_node_match (
    NOD		node1,
    NOD		node2)
{
/**************************************
 *
 *	C M P _ n o d e _ m a t c h
 *
 **************************************
 *
 * Functional description
 *	Compare two nodes for equality of value.
 *
 **************************************/
NOD	*ptr1, *ptr2, *end;
MAP	map1, map2;
USHORT	l;
UCHAR	*p1, *p2;

if (!node1 || !node2 || node1->nod_type != node2->nod_type)
    return FALSE;

switch (node1->nod_type)
    {
    case nod_field:
	{
	if (node1->nod_arg [e_fld_field] != node2->nod_arg [e_fld_field] ||
	    node1->nod_arg [e_fld_context] != node2->nod_arg [e_fld_context] ||
	    node1->nod_arg [e_fld_subs] != node2->nod_arg [e_fld_subs])
	    return FALSE;
	return TRUE;
	}

    case nod_constant:
	{
	if (node1->nod_desc.dsc_dtype != node2->nod_desc.dsc_dtype ||
	    node2->nod_desc.dsc_scale != node2->nod_desc.dsc_scale ||
	    node2->nod_desc.dsc_length != node2->nod_desc.dsc_length)
	    return FALSE;
	p1 = node1->nod_desc.dsc_address;
	p2 = node2->nod_desc.dsc_address;
	if (l = node1->nod_desc.dsc_length)
    	    do if (*p1++ != *p2++) return FALSE; while (--l);
	return TRUE;
	}

    case nod_map	:
	{
	map1 = (MAP) node1->nod_arg [e_map_map];
	map2 = (MAP) node2->nod_arg [e_map_map];
	return CMP_node_match (map1->map_node, map2->map_node);
	}

    case nod_agg_average	:
    case nod_agg_max	:
    case nod_agg_min	:
    case nod_agg_total	:
    case nod_agg_count	:
	return CMP_node_match (node1->nod_arg [e_stt_value], node2->nod_arg [e_stt_value]);

    case nod_function:
        if (node1->nod_arg [e_fun_function] != node1->nod_arg [e_fun_function])
            return FALSE;
        return CMP_node_match (node1->nod_arg [e_fun_args], node2->nod_arg [e_fun_args]);
    }

ptr1 = node1->nod_arg;
ptr2 = node2->nod_arg;

for (end = ptr1 + node1->nod_count; ptr1 < end; ptr1++, ptr2++)
    if (!CMP_node_match (*ptr1, *ptr2))
	return FALSE;

return TRUE;
}

static NOD compile_any (
    NOD		node,
    REQ		old_request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ a n y
 *
 **************************************
 *
 * Functional description
 *	Compile either an ANY or UNIQUE boolean expression.
 *
 **************************************/
MSG	send, receive, old_send = NULL, old_receive = NULL, next;
REQ	request;
PAR	parameter;

if (old_request)
    {
    old_send = old_request->req_send;
    old_receive = old_request->req_receive;
    }

if (request = compile_rse (node->nod_arg [e_any_rse], 
	old_request, FALSE, &send, &receive, NULL_PTR))
    node->nod_arg [e_any_request] = (NOD) request;
else
    request = old_request;

if (old_request)
    {
    old_request->req_send = old_send;
    old_request->req_receive = old_receive;
    }

if ((send != old_send) && !send->msg_parameters)
    {
    release_message (send);
    send = NULL;
    if (!receive)
	return NULL;
    }

if (old_request && request->req_database != old_request->req_database)
    IBERROR (357); /* Msg357 relations from multiple databases in single rse */

if (old_request && (!receive || !receive->msg_parameters))
    {
    if (receive)
	release_message (receive);
    receive = NULL;
    }

if (receive)
    {
    node->nod_import = parameter = make_parameter (receive, NULL_PTR);
    parameter->par_desc.dsc_dtype = dtype_short;
    parameter->par_desc.dsc_length = sizeof (SSHORT);
    }

node->nod_arg [e_any_send] = (NOD) send;
node->nod_arg [e_any_receive] = (NOD) receive;

return node;
}

static NOD compile_assignment (
    NOD		node,
    REQ		request,
    int		statement_internal)
{
/**************************************
 *
 *	c o m p i l e _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description:
 *	Compile an assignment statement, passing
 *	it to the database if possible.  Possibility
 *	happens when both sides of the assignment are
 *	computable in the request context.  (Following
 *	the logic of such things, the 'internal' flags
 *	mean external to qli, but internal to jrd.
 *	As is well known, the seat of the soul is the
 *	dbms). 
 *
 **************************************/
NOD	to, target, from, initial;
CTX	context;
USHORT	target_internal;

/* Start by assuming that the assignment will ultimately
   take place in the DBMS */

to = node->nod_arg [e_asn_to];
to->nod_flags |= NOD_parameter2;
from = node->nod_arg [e_asn_from];
from->nod_flags |= NOD_parameter2;

/* If the assignment is to a variable, the assignment is
   completely local */

if (to->nod_type == nod_variable ||
    to->nod_type == nod_form_field)
    {
    statement_internal = FALSE;
    node->nod_flags |= NOD_local;
    }

target_internal = computable (to, request);
statement_internal = statement_internal && request && target_internal && computable (from, request); 

node->nod_arg [e_asn_to] = target = compile_expression (to, request, target_internal);
node->nod_arg [e_asn_from] = compile_expression (from, request, statement_internal);
if (initial = node->nod_arg [e_asn_initial])
    node->nod_arg [e_asn_initial] = compile_expression (initial, request, FALSE);

if (statement_internal)
    {
    node->nod_arg [e_asn_valid] = NULL;  /* Memory reclaimed in the main loop */
    node->nod_flags |= NOD_remote;
    node = NULL;
    }
else 
	 {
    if (node->nod_arg [e_asn_valid])
	     compile_expression (node->nod_arg [e_asn_valid], request, FALSE);
    if (target->nod_type == nod_field)
        {
        if (!request)
            {
            context = (CTX) target->nod_arg [e_fld_context];
            request = context->ctx_request;
            }
        target->nod_arg [e_fld_reference] = make_reference (target, request->req_send);
        }
    }
    
return node;
}

static void compile_context (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Set all of the contexts in a request.
 *	This may require a recursive call.
 *
 **************************************/
CTX	context, *ctx_ptr, *ctx_end;

ctx_ptr = (CTX*) node->nod_arg + e_rse_count;
ctx_end = ctx_ptr + node->nod_count;

for (; ctx_ptr < ctx_end; ctx_ptr++)
    {
    context = *ctx_ptr;
    context->ctx_request = request;
    context->ctx_context = request->req_context++;
    context->ctx_message = request->req_receive;
    if (context->ctx_sub_rse)
	compile_rse (context->ctx_sub_rse, request, internal_flag, NULL_PTR, NULL_PTR, NULL_PTR);
    if (context->ctx_stream)
	compile_context (context->ctx_stream, request, internal_flag);
    }
}

static void compile_control_break (
    BRK		control,
    REQ		request)
{
/**************************************
 *
 *	c o m p i l e _ c o n t r o l _ b r e a k
 *
 **************************************
 *
 * Functional description
 *	Compile a control/page/report break.
 *
 **************************************/
NOD	temp;

for (; control; control = control->brk_next)
    {
    report_control_break = control;
    if (control->brk_field) 
/*
	control->brk_field  = (SYN) compile_expression (control->brk_field, request, FALSE);
*/
	{
	temp = (NOD) control->brk_field;;
	temp->nod_flags |= NOD_parameter2;
	temp = compile_expression (control->brk_field, request, FALSE); 
	if (temp->nod_type == nod_field) 
	    temp = temp->nod_arg [e_fld_reference];
	control->brk_field = (SYN) temp;
	}
    if (control->brk_line)
	compile_print_list (control->brk_line, request, NULL_PTR);
    report_control_break = NULL;
    }
}

static NOD compile_edit (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	c o m p i l e _ e d i t
 *
 **************************************
 *
 * Functional description
 *	Compile the "edit blob" expression.
 *
 **************************************/
NOD	value;
FLD	field;
PAR	parm;

/* Make sure there is a message.  If there isn't a message, we
   can't find the target database. */

if (!request)
    BUGCHECK (358); /* Msg358 can't find database for blob edit */

/* If there is an input blob, get it now. */

if (value = node->nod_arg [e_edt_input])
    {
    field = (FLD) value->nod_arg [e_fld_field];
    if (value->nod_type != nod_field || field->fld_dtype != dtype_blob)
	IBERROR (356); /* Msg356 EDIT argument must be a blob field */
    node->nod_arg [e_edt_input] = compile_expression (value, request, FALSE);
    }

node->nod_arg [e_edt_dbb] = (NOD) request->req_database;
node->nod_desc.dsc_dtype = dtype_blob;
node->nod_desc.dsc_length = 8;
node->nod_desc.dsc_address = (UCHAR*) &node->nod_arg [e_edt_id1];

return node;
}

static NOD compile_erase (
    NOD		node,
    REQ		org_request)
{
/**************************************
 *
 *	c o m p i l e _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Compile an ERASE statement.  This is so simple that nothing
 *	needs to be done.
 *
 **************************************/
REQ	request;
CTX	context;

context = (CTX) node->nod_arg [e_era_context];
request = context->ctx_request;
node->nod_arg [e_era_request] = (NOD) request;

request->req_database->dbb_flags |= DBB_updates;

if (org_request == request &&
    !request->req_continue)
    return NULL;

if (!request->req_continue)
    request->req_continue = make_message (request);

node->nod_arg [e_era_message] = (NOD) make_message (request);

return node;
}

static NOD compile_expression (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ e x p r e s s i o n
 *
 **************************************
 *
 * Functional description
 *	Compile a value.  The value may be used internally as part of
 *	another expression (internal_flag == TRUE) or may be referenced
 *	in the QLI context (internal_flag == FALSE).	
 *
 **************************************/
NOD	*ptr, *end, value;
MAP	map;
PAR	parm;
FLD	field;

switch (node->nod_type)
    {
    case nod_any:
    case nod_unique:
	return compile_any (node, request, internal_flag);

    case nod_max:
    case nod_min:
    case nod_count:
    case nod_average:
    case nod_total:
    case nod_from:
	node->nod_count = 0;
	return compile_statistical (node, request, internal_flag);

    case nod_rpt_max:
    case nod_rpt_min:
    case nod_rpt_count:
    case nod_rpt_average:
    case nod_rpt_total:
	if (report_control_break)
	    LLS_PUSH (node, &report_control_break->brk_statisticals);

    case nod_running_total:
    case nod_running_count:
	node->nod_count = 0;
	if (value = node->nod_arg [e_stt_value])
	    {
	    value->nod_flags |= NOD_parameter2;
	    node->nod_arg [e_stt_value] =
		compile_expression (value, request, FALSE);
	    }
	make_descriptor (node, &node->nod_desc);
	if (internal_flag)
	    {
	    node->nod_export = parm = make_parameter (request->req_send, NULL_PTR);
	    parm->par_desc = node->nod_desc;
	    parm->par_value = node;
	    }
	else 
	    {
	    CMP_alloc_temp (node);
	    if (request && value && computable (value, request))
		node->nod_arg [e_stt_value] = make_reference (value, request->req_receive);
	    }
	return node;

    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_count:
    case nod_agg_average:
    case nod_agg_total:
	node->nod_count = 0;
	if (value = node->nod_arg [e_stt_value])
	    {
	    value->nod_flags |= NOD_parameter2;
	    node->nod_arg [e_stt_value] =
		compile_expression (value, request, TRUE);
	    }
	make_descriptor (node, &node->nod_desc);
	if (!internal_flag && request)
	    return make_reference (node, request->req_receive);
	return node;

    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	map->map_node = value = compile_expression (map->map_node, request, TRUE);
	make_descriptor (value, &node->nod_desc);
	if (!internal_flag && request)
	    return make_reference (node, request->req_receive);
	return node;

    case nod_function:
	return compile_function (node, request, internal_flag);

    case nod_eql:
    case nod_neq:
    case nod_gtr:
    case nod_geq:
    case nod_leq:
    case nod_lss:
    case nod_between:
	node->nod_flags |= nod_comparison;

    case nod_list:
    case nod_matches:
    case nod_sleuth:
    case nod_like:
    case nod_containing:
    case nod_starts:
    case nod_missing:
    case nod_and:
    case nod_or:
    case nod_not:
	for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	    {
	    (*ptr)->nod_flags |= NOD_parameter2;
	    *ptr = compile_expression (*ptr, request, internal_flag);
	    }
	if (node->nod_type == nod_and ||
	    node->nod_type == nod_or)
	    node->nod_flags |= nod_partial;
	return node;

    case nod_null:
    case nod_add:
    case nod_subtract:
    case nod_multiply:
    case nod_divide:
    case nod_negate:
    case nod_concatenate:
    case nod_substr:
    case nod_user_name:
	if (!internal_flag && request && request->req_receive &&
	    computable (node, request))
	    {
	    compile_expression (node, request, TRUE);
	    return make_reference (node, request->req_receive);
	    }
	for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	    {
	    (*ptr)->nod_flags |= NOD_parameter2;
	    *ptr = compile_expression (*ptr, request, internal_flag);
            }
	make_descriptor (node, &node->nod_desc);
	if (!internal_flag)
	    {
	    node->nod_flags |= NOD_local;
	    CMP_alloc_temp (node);
	    }
	return node;

    case nod_format:
	value = node->nod_arg [e_fmt_value];
	node->nod_arg [e_fmt_value] = compile_expression (value, request, FALSE);
	node->nod_desc.dsc_length = FMT_expression (node);
	node->nod_desc.dsc_dtype = dtype_text;
	CMP_alloc_temp (node);
	if (!internal_flag)
	    node->nod_flags |= NOD_local;
	return node;

    case nod_constant:
	if (!internal_flag)
	    node->nod_flags |= NOD_local;
	return node;
    
    case nod_edit_blob:
	compile_edit (node, request);
	return node;

    case nod_prompt:
	node->nod_count = 0;
	compile_prompt (node);
	if (internal_flag)
	    {
	    node->nod_export = parm = make_parameter (request->req_send, node);
	    parm->par_value = node;
	    if (field = (FLD) node->nod_arg [e_prm_field])
		{
		parm->par_desc.dsc_dtype = field->fld_dtype;
		parm->par_desc.dsc_length = field->fld_length;
		parm->par_desc.dsc_scale = field->fld_scale;
		parm->par_desc.dsc_sub_type = field->fld_sub_type;
		if (parm->par_desc.dsc_dtype == dtype_text)
		    {
		    parm->par_desc.dsc_dtype = dtype_varying;
		    parm->par_desc.dsc_length += sizeof (SSHORT);
		    }
		}
	    else
		parm->par_desc = node->nod_desc;
	    }
	return node;

    case nod_field:
	if (value = node->nod_arg [e_fld_subs])
	    compile_expression (value, request, TRUE);
	return compile_field (node, request, internal_flag);

    case nod_variable:
	field = (FLD) node->nod_arg [e_fld_field];
	node->nod_desc.dsc_address = field->fld_data;

    case nod_form_field:
	make_descriptor (node, &node->nod_desc);
	if (internal_flag)
	    {
	    node->nod_export = parm = make_parameter (request->req_send, node);
	    parm->par_value = node;
	    parm->par_desc = node->nod_desc;
	    }
	return node;

    case nod_upcase:
	value = node->nod_arg[0];
	node->nod_arg[0] = compile_field (value, request, internal_flag);
	return node;

    default:
	BUGCHECK (359); /* Msg359 compile_expression: not yet implemented */
    }
}

static NOD compile_field (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Compile a field reference.
 *
 **************************************/
NOD	reference;
CTX	context;
PAR	parm;
MSG	message;
FLD	field;

/* Pick up field characteristics */

node->nod_count = 0;
field = (FLD) node->nod_arg [e_fld_field];
context = (CTX) node->nod_arg [e_fld_context];
if ((field->fld_flags & FLD_array) && !node->nod_arg [e_fld_subs])
    {
    node->nod_desc.dsc_dtype = dtype_quad;
    node->nod_desc.dsc_length = 8;
    node->nod_desc.dsc_scale = 0;
    node->nod_desc.dsc_sub_type = 0;
    }
else    
    {
    node->nod_desc.dsc_dtype = field->fld_dtype;
    node->nod_desc.dsc_length = field->fld_length;
    node->nod_desc.dsc_scale = field->fld_scale;
    node->nod_desc.dsc_sub_type = field->fld_sub_type;
    }
node->nod_flags |= NOD_parameter2;

/* If the reference is external, or the value is computable in the
   current request, there is nothing to do.  If the value is not computable,
   make up a parameter to send the value into the request. */

if (internal_flag)
    {
    if (computable (node, request))
	return node;
    node->nod_export = parm = make_parameter (request->req_send, node);
    parm->par_desc = node->nod_desc;
    parm->par_value = node;
    if (!(message = context->ctx_message))
	message = request->req_receive;
    node->nod_arg [e_fld_reference] = make_reference (node, message);
    return node;
    }

if (!(message = context->ctx_message) && request)
    message = request->req_receive;

node->nod_arg [e_fld_reference] = make_reference (node, message);
return node;
}

static NOD compile_for (
    NOD		node,
    REQ		old_request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ f o r
 *
 **************************************
 *
 * Functional description
 *	Compile a FOR loop.  If the loop can be done as part of the parent
 *	request, dandy.
 *
 **************************************/
MSG	send, receive, old_send = NULL, old_receive = NULL;
REQ	request;
PAR	parameter;

/* Compile rse.  This will set up both send and receive message.  If the
   messages aren't needed, we can release them later. */

if (old_request)
    {
    old_send = old_request->req_send;
    old_receive = old_request->req_receive;
    }

if (request = compile_rse (node->nod_arg [e_for_rse], 
	old_request, FALSE, &send, &receive, NULL_PTR))
    node->nod_arg [e_for_request] = (NOD) request;
else
    request = old_request;

/* If nothing is required for sub-statement, and no data is required in
   either direction, we don't need  to execute the statement. */

if (!compile_statement (node->nod_arg [e_for_statement], request, internal_flag) &&
    !receive->msg_parameters)
    {
    release_message (receive);
    receive = NULL;
    }

if (old_request)
    {
    old_request->req_send = old_send;
    old_request->req_receive = old_receive;
    }

if ((send != old_send) && !send->msg_parameters)
    {
    release_message (send);
    send = NULL;
    if (!receive)
	return NULL;
    }

if (receive)
    {
    parameter = make_parameter (receive, NULL_PTR);
    node->nod_arg [e_for_eof] = (NOD) parameter;
    parameter->par_desc.dsc_dtype = dtype_short;
    parameter->par_desc.dsc_length = sizeof (SSHORT);
    }

node->nod_arg [e_for_send] = (NOD) send;
node->nod_arg [e_for_receive] = (NOD) receive;

return node;
}

static NOD compile_function (
    NOD		node,
    REQ		old_request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Compile a database function reference.
 *
 **************************************/
NOD	value, *ptr, *end, list;
FUN	function;
MSG	send, receive, old_send = NULL, old_receive = NULL;
REQ	request;
PAR	parameter;

function = (FUN) node->nod_arg [e_fun_function];
node->nod_count = 0;
send = NULL;

if (!internal_flag)
    old_request = NULL;

if (old_request)
    {
    old_send = old_request->req_send;
    old_receive = old_request->req_receive;
    }

if (!old_request || old_request->req_database != function->fun_database)
    {
    request = make_request (function->fun_database);
    node->nod_arg [e_fun_request] = (NOD) request;
    request->req_send = send = make_message (request);
    request->req_receive = receive = make_message (request);
    }
else
    request = old_request;

/* If there is a value, compile it here */

if (!internal_flag)
    {
    node->nod_import = parameter = make_parameter (request->req_receive, NULL_PTR);
    make_descriptor (node, &parameter->par_desc);
    }

list = node->nod_arg [e_fun_args];

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    compile_expression (*ptr, request, TRUE);

if (old_request)
    {
    old_request->req_send = old_send;
    old_request->req_receive = old_receive;
    }

if (!internal_flag && request == old_request && computable (node, request))
    {
    make_descriptor (node, &node->nod_desc);
    return make_reference (node, request->req_receive);
    }

if (send && (send != old_send) && !send->msg_parameters)
    {
    release_message (send);
    send = NULL;
    }

node->nod_arg [e_fun_receive] = (NOD) receive;
node->nod_arg [e_fun_send] = (NOD) send;

return node;
}

static NOD compile_if (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ i f
 *
 **************************************
 *
 * Functional description
 *	Compile an IF statement.  Determine whether the
 *	statement is going to be executed in QLI or database
 *	context.  Try to make sure that the whole thing is
 *	executed one place or the other.
 *
 **************************************/
NOD	sub;
REQ	sub_req;

/* If the statement can't be executed in database context,
   make sure it gets executed locally */

if (!internal_flag || !computable (node, request))
    {
    internal_flag = FALSE;
    node->nod_flags |= NOD_local;
    request = NULL;
    }

sub = node->nod_arg [e_if_boolean];
compile_expression (sub, request, internal_flag);
compile_statement (node->nod_arg [e_if_true], request, internal_flag);

if (sub = node->nod_arg [e_if_false])
    compile_statement (sub, request, internal_flag);

if (internal_flag)
    return NULL;

return node;
}

static NOD compile_list_fields (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	c o m p i l e _ l i s t _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Compile a print node.
 *
 **************************************/
NOD	list;

list = node->nod_arg [e_prt_list];
compile_print_list (list, request, NULL_PTR);
node->nod_arg [e_prt_list] = FMT_list (list);
node->nod_type = nod_print;

return node;
}

static NOD compile_modify (
    NOD		node,
    REQ		org_request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Compile a modify statement.
 *
 **************************************/
REQ	request;
CTX	context;
MSG	send, old_send;
NOD	*ptr;
USHORT	i;

/* If this is a different request from the current one, this will require an
   optional action and a "continue" mesasge */

ptr = node->nod_arg + e_mod_count;
context = (CTX) *ptr;
request = context->ctx_source->ctx_request;
node->nod_arg [e_mod_request] = (NOD) request;

if ((request != org_request || !internal_flag) && !request->req_continue)
    request->req_continue = make_message (request);

old_send = request->req_send;
request->req_send = send = make_message (request);

for (i = 0; i < node->nod_count; i++)
    {
    context = (CTX) *ptr++;
    context->ctx_request = request;
    context->ctx_context = request->req_context++;
    context->ctx_message = send;
    }

/* If nothing is required for sub-statement, and no data is required in
   either direction, we don't need  to execute the statement. */

if (internal_flag)
    internal_flag = computable (node->nod_arg [e_mod_statement], request);

if (!compile_statement (node->nod_arg [e_mod_statement], request, internal_flag) &&
    (send != old_send) && !send->msg_parameters)
    {
    node->nod_flags |= NOD_remote;
    release_message (send);
    send = NULL;
    }

node->nod_arg [e_mod_send] = (NOD) send;
request->req_send = old_send;
request->req_database->dbb_flags |= DBB_updates;

if (internal_flag)
    return NULL;

return node;
}

static NOD compile_print (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	c o m p i l e _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Compile a print node.
 *
 **************************************/

if (!print_header)
    print_header = (TEXT**) &node->nod_arg [e_prt_header];

compile_print_list (node->nod_arg [e_prt_list], request, &print_items);

return node;
}

static NOD compile_print_list (
    NOD		list,
    REQ		request,
    LLS		*stack)
{
/**************************************
 *
 *	c o m p i l e _ p r i n t _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Compile a print node.
 *
 **************************************/
NOD	*ptr, *end, value;
ITM	item;

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    {
    item = (ITM) *ptr;
    if (stack)
	LLS_PUSH (item, stack);
    if (item->itm_type == item_value)
	{
	value = item->itm_value;
	value->nod_flags |= NOD_parameter2;
	item->itm_value = compile_expression (value, request, FALSE);
        if (item->itm_value->nod_type == nod_field)
	    item->itm_value = item->itm_value->nod_arg [e_fld_reference];
	if (!value->nod_desc.dsc_dtype)
	    make_descriptor (value, &value->nod_desc);
	}
    }

return list;
}

static NOD compile_prompt (
    NOD		node)
{
/**************************************
 *
 *	c o m p i l e _ p r o m p t
 *
 **************************************
 *
 * Functional description
 *	Set up a prompt expression for execution.
 *
 **************************************/
STR	string;
FLD	field;
USHORT	l, prompt_length;
UCHAR	*p, *prompt;

/* Make up a plausible prompt length */

if (!(field = (FLD) node->nod_arg [e_prm_field]))
    prompt_length = PROMPT_LENGTH;
else
    switch (field->fld_dtype)
	{
	case dtype_text:
	    prompt_length = field->fld_length;
	    break;

	case dtype_varying:
	    prompt_length = field->fld_length - sizeof (SSHORT);
	    break;

	case dtype_short:
	    prompt_length = 8;
	    break;

	case dtype_long:
	case dtype_real:
	    prompt_length = 15;
	    break;

	default:
	    prompt_length = 30;
	    break;
	}

/* Allocate string buffer to hold data, a two byte count, 
  a possible carriage return, and a null */

prompt_length += 2 + sizeof (SSHORT);
string = (STR) ALLOCDV (type_str, prompt_length);
node->nod_arg [e_prm_string] = (NOD) string;
node->nod_desc.dsc_dtype = dtype_varying;
node->nod_desc.dsc_length = prompt_length;
node->nod_desc.dsc_address = (UCHAR*) string->str_data;

return node;
}

static NOD compile_repeat (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ r e p e a t
 *
 **************************************
 *
 * Functional description
 *	Compile a REPEAT statement.
 *
 **************************************/

compile_expression (node->nod_arg [e_rpt_value], request, FALSE);
compile_statement (node->nod_arg [e_rpt_statement], NULL_PTR, internal_flag);

return node;
}

static NOD compile_report (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	c o m p i l e _ r e p o r t
 *
 **************************************
 *
 * Functional description
 *	Compile the body of a report specification.
 *
 **************************************/
RPT	report;
BRK	control, temp;
NOD	list;

report = (RPT) node->nod_arg [e_prt_list];

if (control = report->rpt_top_rpt)
    compile_control_break (control, request);

if (control = report->rpt_top_page)
    compile_control_break (control, request);

if (control = report->rpt_top_breaks)
    compile_control_break (control, request);

if (list = report->rpt_detail_line)
    compile_print_list (list, request, NULL_PTR);

if (control = report->rpt_bottom_breaks)
    {
    compile_control_break (control, request);
    report->rpt_bottom_breaks = NULL;
    while (control)
	{
	temp = control;
	control = control->brk_next;
	temp->brk_next = report->rpt_bottom_breaks;
	report->rpt_bottom_breaks = temp;
	}
     }

if (control = report->rpt_bottom_page)
    compile_control_break (control, request);

if (control = report->rpt_bottom_rpt)
    compile_control_break (control, request);

FMT_report (report);

return node;
}

static REQ compile_rse (
    NOD		node,
    REQ		old_request,
    int		internal_flag,
    MSG		*send,
    MSG		*receive,
    DBB		*database)
{
/**************************************
 *
 *	c o m p i l e _ r s e
 *
 **************************************
 *
 * Functional description
 *	Compile a record selection expression.  If it can't be processed
 *	as part of an existing parent request, make up a new request.  If
 *	data must be sent to start the loop, generate a send message.  Set
 *	up for a receive message as well.
 *
 **************************************/
NOD	list, *ptr, *end, *ptr2, rse;
REQ	request, original_request;
CTX	context, parent, *ctx_ptr, *ctx_end;
REL	relation;
DBB	local_dbb;
USHORT	l;

original_request = old_request;

if (!database)
    {
    local_dbb = NULL;
    database = &local_dbb;
    }

/* Loop thru relations to make sure only a single database is presented */

ctx_ptr = (CTX*) node->nod_arg + e_rse_count;
ctx_end = ctx_ptr + node->nod_count;

for (; ctx_ptr < ctx_end; ctx_ptr++)
    {
    context = *ctx_ptr;
    if (context->ctx_stream)
	{
	if (request = compile_rse (context->ctx_stream, old_request, internal_flag, send, receive, database))
	    old_request = request;
	}
    else
	{
	relation = context->ctx_relation;
	if (!*database)
	    *database = relation->rel_database;
	else if (*database != relation->rel_database)
	    IBERROR (357); /* Msg357 relations from multiple databases in single rse */
	}
    }

if (!old_request || old_request->req_database != *database)
    request = make_request (*database);
else
    request = old_request;

if (send)
    {
    if (old_request && request == old_request && 
	!(old_request->req_flags & REQ_rse_compiled))
	*send = request->req_send;
    else
	request->req_send = *send = make_message (request);
    request->req_receive = *receive = make_message (request);
    }

compile_context (node, request, internal_flag);

/* Process various clauses */

if (node->nod_arg [e_rse_first])
    compile_expression (node->nod_arg [e_rse_first], request, TRUE);

if (node->nod_arg [e_rse_boolean])
    compile_expression (node->nod_arg [e_rse_boolean], request, TRUE);

if (list = node->nod_arg [e_rse_sort])
    for (ptr = list->nod_arg, end = ptr + list->nod_count * 2; ptr < end; ptr += 2)
	compile_expression (*ptr, request, TRUE);

if (list = node->nod_arg [e_rse_reduced])
    for (ptr = list->nod_arg, end = ptr + list->nod_count * 2; ptr < end; ptr += 2)
	compile_expression (*ptr, request, TRUE);

if (list = node->nod_arg [e_rse_group_by])
    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	compile_expression (*ptr, request, TRUE);

if (node->nod_arg [e_rse_having])
    compile_expression (node->nod_arg [e_rse_having], request, TRUE);

/* If we didn't allocate a new request block, say so by returning NULL */

if (request == original_request)
    return NULL;

request->req_flags |= REQ_rse_compiled;

return request;
}

static NOD compile_statement (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Compile a statement.  Actually, just dispatch, passing along
 *	the parent request.
 *
 **************************************/
NOD	result, *ptr, *end, sub;

switch (node->nod_type)
    {
    case nod_assign:
	return compile_assignment (node, request, internal_flag);
    
    case nod_erase:
	return compile_erase (node, request);

    case nod_commit_retaining:
	return node;

    case nod_for:
    case nod_report_loop:
	return compile_for (node, request, internal_flag);
    
    case nod_list:
	result = NULL;
	for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	    if (compile_statement (*ptr, request, internal_flag))
		result = node;
	return result;

    case nod_modify:
	return compile_modify (node, request, internal_flag);

    case nod_output:
	compile_expression (node->nod_arg [e_out_file], request, FALSE);
	compile_statement (node->nod_arg [e_out_statement], request, FALSE);
	return node;

    case nod_print:
	return compile_print (node, request);

    case nod_list_fields:
	return compile_list_fields (node, request);
    
    case nod_repeat:
	return compile_repeat (node, request, internal_flag);

    case nod_report:
	return compile_report (node, request);

    case nod_store:
	return compile_store (node, request, internal_flag);

    case nod_if:
	return compile_if (node, request, internal_flag);

    case nod_form_for:
	compile_statement (node->nod_arg [e_ffr_statement], request, FALSE);
	return node;

    case nod_form_update:
	if (node->nod_arg [e_fup_tag])
	    compile_expression (node->nod_arg [e_fup_tag], NULL_PTR, FALSE);
	return node;

    case nod_abort:
	if (node->nod_count)
	    compile_expression (node->nod_arg [0], NULL_PTR, FALSE);
	return node;

    default:
	BUGCHECK (360); /* Msg360 not yet implemented (compile_statement) */
    }
}

static NOD compile_statistical (
    NOD		node,
    REQ		old_request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ s t a t i s t i c a l
 *
 **************************************
 *
 * Functional description
 *	Compile a statistical expression.  The expression may or may not
 *	request a separate request.
 *
 **************************************/
NOD	value;
MSG	send, receive, old_send = NULL, old_receive = NULL;
REQ	request;
PAR	parameter;

/* If a default value is present, compile it outside the context of
   the rse. */

if (value = node->nod_arg [e_stt_default])
    compile_expression (value, old_request, TRUE);

/* Compile rse.  This will set up both send and receive message.  If the
   messages aren't needed, we can release them later. */

if (!internal_flag)
    old_request = NULL;

if (old_request)
    {
    old_send = old_request->req_send;
    old_receive = old_request->req_receive;
    }

if (request = compile_rse (node->nod_arg [e_stt_rse], 
	old_request, internal_flag, &send, &receive, NULL_PTR)) 
    node->nod_arg [e_stt_request] = (NOD) request;
else
    request = old_request;

/* If there is a value, compile it here */

if (!internal_flag)
    {
    node->nod_import = parameter = make_parameter (request->req_receive, NULL_PTR);
    make_descriptor (node, &parameter->par_desc);
    }

if (value = node->nod_arg [e_stt_value])
    compile_expression (value, request, TRUE);

if (old_request)
    {
    old_request->req_send = old_send;
    old_request->req_receive = old_receive;
    }

if (!internal_flag && request == old_request && computable (node, request))
    {
    make_descriptor (node, &node->nod_desc);
    return make_reference (node, request->req_receive);
    }

if ((send != old_send) && !send->msg_parameters)
    {
    release_message (send);
    send = NULL;
    }

node->nod_arg [e_stt_receive] = (NOD) receive;
node->nod_arg [e_stt_send] = (NOD) send;

return node;
}

static NOD compile_store (
    NOD		node,
    REQ		request,
    int		internal_flag)
{
/**************************************
 *
 *	c o m p i l e _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Compile a STORE statement.
 *
 **************************************/
CTX	context;
REL	relation;
MSG	send;
REQ	new_request;
PAR	parameter;

/* Find or make up request for statement */

context = (CTX) node->nod_arg [e_sto_context];
relation = context->ctx_relation;

if (!request || request->req_database != relation->rel_database)
    {
    request = make_request (relation->rel_database);
    node->nod_arg [e_sto_request] = (NOD) request;
    }

request->req_database->dbb_flags |= DBB_updates;

context->ctx_request = request;
context->ctx_context = request->req_context++;
context->ctx_message = request->req_send = send = make_message (request);

/* If nothing is required for sub-statement, and no data is required in
   either direction, we don't need to execute the statement. */

if (internal_flag)
    internal_flag = computable (node->nod_arg [e_sto_statement], request);

if (!compile_statement (node->nod_arg [e_sto_statement], request, internal_flag) &&
    !send->msg_parameters)
    {
    node->nod_flags |= NOD_remote;
    release_message (send);
    return NULL;
    }

node->nod_arg [e_sto_send] = (NOD) send;

return node;
}

static int computable (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	c o m p u t a b l e
 *
 **************************************
 *
 * Functional description
 *	Check to see if a value is computable within the context of a
 *	given request.
 *
 **************************************/
NOD	*ptr, *end, sub;
CTX	context;
MAP	map;

switch (node->nod_type)
    {
    case nod_max:
    case nod_min:
    case nod_count:
    case nod_average:
    case nod_total:
    case nod_from:
	if ((sub = node->nod_arg [e_stt_rse]) && !computable (sub, request))
	    return FALSE;
	if ((sub = node->nod_arg [e_stt_value]) && !computable (sub, request))
	    return FALSE;
	if ((sub = node->nod_arg [e_stt_default]) && !computable (sub, request))
	    return FALSE;
	return TRUE;

    case nod_rse:
	if (!request)
	    return FALSE;
	if ((sub = node->nod_arg [e_rse_first]) && !computable (sub, request))
	    return FALSE;
	for (ptr = node->nod_arg + e_rse_count, end = ptr + node->nod_count; ptr < end; ptr++)
	    {
	    context = (CTX) *ptr;
	    if (context->ctx_stream)
		{
		if (!computable (context->ctx_stream, request))
		    return FALSE;
		}
	    else if (context->ctx_relation->rel_database != request->req_database)
		return FALSE;
	    context->ctx_request = request;
	    }
	if ((sub = node->nod_arg [e_rse_boolean]) && !computable (sub, request))
	    return FALSE;
	return TRUE;

    case nod_field:
	if (sub = node->nod_arg [e_fld_subs])
	    for (ptr = sub->nod_arg, end = ptr + sub->nod_count; ptr < end; ptr++)
		if (*ptr && !computable (*ptr, request))
		    return FALSE;
	context = (CTX) node->nod_arg [e_fld_context];
	return (request == context->ctx_request);
    
    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	return computable (map->map_node, request);

    case nod_print:
    case nod_report:
    case nod_abort:
    case nod_repeat:
    case nod_commit_retaining:

    case nod_rpt_average:
    case nod_rpt_max:
    case nod_rpt_min:
    case nod_rpt_total:
    case nod_rpt_count:
    case nod_running_total:
    case nod_running_count:
    case nod_edit_blob:
    case nod_prompt:
    case nod_variable:
    case nod_form_field:
    case nod_form_for:
    case nod_format:
	return FALSE;

    case nod_null:
    case nod_constant:
    case nod_user_name:
	return TRUE;

    case nod_for:
	if ((REQ) node->nod_arg [e_for_request] != request)
	    return FALSE;
	if (!computable (node->nod_arg [e_for_rse], request) ||
	    !computable (node->nod_arg [e_for_statement], request))
	    return FALSE;
	return TRUE;
    
    case nod_store:
	if ((REQ) node->nod_arg [e_sto_request] != request)
	    return FALSE;
	return computable (node->nod_arg [e_sto_statement], request);

    case nod_modify:
	context = (CTX) node->nod_arg [e_mod_count];
	if (context->ctx_source->ctx_request != request)
	    return FALSE;
	return computable (node->nod_arg [e_mod_statement], request);

    case nod_erase:
	context = (CTX) node->nod_arg [e_era_context];
	if (context->ctx_source->ctx_request != request)
	    return FALSE;
	return TRUE;

    case nod_unique:
    case nod_any:
	if (node->nod_arg [e_any_request] != (NOD) request)
	    return FALSE;
	return (computable (node->nod_arg [e_any_rse], request));

    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_count:
    case nod_agg_average:
    case nod_agg_total:
	if (sub = node->nod_arg [e_stt_value])
	    return (computable (sub, request));

    case nod_assign:
        if (node->nod_arg[e_asn_valid])
            {
            sub = node->nod_arg[e_asn_from];
            if (sub->nod_type == nod_prompt)
            /* Try to do validation in QLI as soon as 
               the user responds to the prompt */
             return FALSE;
            }
    case nod_list:
    case nod_if:

    case nod_eql:
    case nod_neq:
    case nod_gtr:
    case nod_geq:
    case nod_leq:
    case nod_lss:
    case nod_between:
    case nod_matches:
    case nod_sleuth:
    case nod_like:
    case nod_containing:
    case nod_starts:
    case nod_missing:
    case nod_and:
    case nod_or:
    case nod_not:
    case nod_add:
    case nod_subtract:
    case nod_multiply:
    case nod_divide:
    case nod_negate:
    case nod_concatenate:
    case nod_function:
    case nod_substr:
	for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	    if (*ptr && !computable (*ptr, request))
		return FALSE;
	return TRUE;

    default:
	BUGCHECK (361); /* Msg361 computable: not yet implemented */
    }
}

static void make_descriptor (
    NOD		node,
    DSC		*desc)
{
/**************************************
 *
 *	m a k e _ d e s c r i p t o r
 *
 **************************************
 *
 * Functional description
 *	Fill out a descriptor based on an expression.
 *
 **************************************/
DSC	desc1, desc2;
FLD	field;
PAR	parameter;
MAP	map;
FFL	ffield;
FUN	function;
USHORT	dtype;

desc1.dsc_dtype = 0;
desc1.dsc_scale = 0;
desc1.dsc_length = 0;
desc1.dsc_sub_type = 0;
desc1.dsc_address = NULL;
desc1.dsc_flags = 0;
desc2 = desc1;

switch (node->nod_type)
    {
    case nod_field:
    case nod_variable:
	field = (FLD) node->nod_arg [e_fld_field];
	desc->dsc_dtype = field->fld_dtype;
	desc->dsc_length = field->fld_length;
	desc->dsc_scale = field->fld_scale;
	desc->dsc_sub_type = field->fld_sub_type;
	return;

    case nod_reference:
	parameter = node->nod_import;
	*desc = parameter->par_desc;
	return;

    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	make_descriptor (map->map_node, desc);
	return;

    case nod_function:
	function = (FUN) node->nod_arg [e_fun_function];
	*desc = function->fun_return;
	return;

    case nod_constant:
    case nod_prompt:
    case nod_format:
	*desc = node->nod_desc;
	return;

    case nod_concatenate:
	make_descriptor (node->nod_arg [0], &desc1);
	make_descriptor (node->nod_arg [1], &desc2);
	desc->dsc_scale = 0;
	desc->dsc_dtype = dtype_varying;
	desc->dsc_length = sizeof (USHORT) +
		string_length (&desc1) + string_length (&desc2);
	if (desc1.dsc_dtype <= dtype_any_text)
	    desc->dsc_sub_type = desc1.dsc_sub_type;
	else
	    desc->dsc_sub_type = ttype_ascii;
	return;	

    case nod_add:
    case nod_subtract:
	make_descriptor (node->nod_arg [0], &desc1);
	make_descriptor (node->nod_arg [1], &desc2);
	if ((desc1.dsc_dtype == dtype_text && desc1.dsc_length >= 9) ||
	    (desc2.dsc_dtype == dtype_text && desc2.dsc_length >= 9))
	    dtype = dtype_double;
	else
	    dtype = MAX (desc1.dsc_dtype, desc2.dsc_dtype);
	switch (dtype)
	    {
	    case dtype_sql_time:
	    case dtype_sql_date:
	    case dtype_timestamp:
		node->nod_flags |= nod_date;
		if (node->nod_type == nod_add)
		    {
		    desc->dsc_dtype = dtype;
		    desc->dsc_length = (dtype == dtype_timestamp) ? 8 : 4;
		    break;
		    }

	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
	    case dtype_double:
	    case dtype_real:
	    case dtype_long:
		desc->dsc_dtype = dtype_double;
		desc->dsc_length = sizeof (double);
		break;

	    default:
		desc->dsc_dtype = dtype_long;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = MIN (desc1.dsc_scale, desc2.dsc_scale);
		break;
	    }
	return;

    case nod_multiply:
	make_descriptor (node->nod_arg [0], &desc1);
	make_descriptor (node->nod_arg [1], &desc2);
	if ((desc1.dsc_dtype == dtype_text && desc1.dsc_length >= 9) ||
	    (desc2.dsc_dtype == dtype_text && desc2.dsc_length >= 9))
	    dtype = dtype_double;
	else
	    dtype = MAX (desc1.dsc_dtype, desc2.dsc_dtype);
	switch (dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
	    case dtype_real:
	    case dtype_double:
	    case dtype_long:
		desc->dsc_dtype = dtype_double;
		desc->dsc_length = sizeof (double);
		break;

	    default:
		desc->dsc_dtype = dtype_long;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = desc1.dsc_scale + desc2.dsc_scale;
		break;
	    }
	return;

    case nod_agg_average:
    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_total:

    case nod_average:
    case nod_max:
    case nod_min:
    case nod_total:
    case nod_from:

    case nod_rpt_average:
    case nod_rpt_max:
    case nod_rpt_min:
    case nod_rpt_total:
    case nod_running_total:
	make_descriptor (node->nod_arg [e_stt_value], desc);
	if (desc->dsc_dtype == dtype_short)
	    {
	    desc->dsc_dtype = dtype_long;
	    desc->dsc_length = sizeof (SLONG);
	    }
	else if (desc->dsc_dtype == dtype_real)
	    {
	    desc->dsc_dtype = dtype_double;
	    desc->dsc_length = sizeof (double);
	    }
	return;

    case nod_null:
	desc->dsc_dtype = dtype_long;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_missing = DSC_missing;
	return;

    case nod_count:
    case nod_agg_count:
    case nod_running_count:
    case nod_rpt_count:
	desc->dsc_dtype = dtype_long;
	desc->dsc_length = sizeof (SLONG);
	return;

    case nod_divide:
	desc->dsc_dtype = dtype_double;
	desc->dsc_length = sizeof (double);
	return;

    case nod_negate:
	make_descriptor (node->nod_arg [0], desc);
	return;

    case nod_form_field:
	ffield = (FFL) node->nod_arg [e_ffl_field];
	desc->dsc_dtype = ffield->ffl_dtype;
	desc->dsc_scale = ffield->ffl_scale;
	desc->dsc_length = ffield->ffl_length;
	return;

    case nod_user_name:
	desc->dsc_dtype = dtype_varying;
	desc->dsc_scale = 0;
	desc->dsc_length = sizeof (USHORT) + 16;
	return;

    case nod_substr:
    default:
	BUGCHECK (362); /* Msg362 make_descriptor: not yet implemented */
    }
}

static MSG make_message (
    REQ		request)
{
/**************************************
 *
 *	m a k e _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Allocate a message block for a request.
 *
 **************************************/
MSG	message;

message = (MSG) ALLOCDV (type_msg, 0);
message->msg_request = request;
message->msg_next = request->req_messages;
request->req_messages = message;
message->msg_number = request->req_msg_number++;

return message;
}

static void make_missing_reference (
    PAR		parameter)
{
/**************************************
 *
 *	m a k e _ m i s s i n g _ r e f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	Make up a parameter to pass a missing value.
 *
 **************************************/
PAR	missing;

if (parameter->par_missing)
    return;

parameter->par_missing = missing = (PAR) ALLOCD (type_par);
missing->par_message = parameter->par_message;
missing->par_desc.dsc_dtype = dtype_short;
missing->par_desc.dsc_length = sizeof (SSHORT);
}

static PAR make_parameter (
    MSG		message,
    NOD		node)
{
/**************************************
 *
 *	m a k e _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Make a parameter block and hang it off a message block.
 *	To make prompts come out in the right order, insert the
 *	new prompt at the end of the prompt list.  Sigh.
 *
 **************************************/
PAR	parm, *ptr;

for (ptr = &message->msg_parameters; *ptr; ptr = &(*ptr)->par_next)
    ;

*ptr = parm = (PAR) ALLOCD (type_par);
parm->par_message = message;
if (node && (node->nod_flags & NOD_parameter2))
    make_missing_reference (parm);

return parm;
}

static NOD make_reference (
    NOD		node,
    MSG		message)
{
/**************************************
 *
 *	m a k e _ r e f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	Make a reference to a value to be computed in the 
 *	database context.  Since a field can be referenced
 *	several times, construct reference blocks linking
 *	the single field to the single parameter.  (I think.)
 *	In any event, if a parameter for a field exists, 
 *	use it rather than generating an new one.   Make it
 *	parameter2 style if necessary.
 *
 **************************************/
NOD	reference;
PAR	parm;

if (!message)
    BUGCHECK (363); /* Msg363 missing message */

/* Look for an existing field reference */

for (parm = message->msg_parameters; parm; parm = parm->par_next)
    if (CMP_node_match (parm->par_value, node))
	break;

/* Parameter doesn't exits -- make a new one. */

if (!parm)
    {
    parm = make_parameter (message, node);
    parm->par_value = node;
    parm->par_desc = node->nod_desc;
    }

reference = (NOD) ALLOCDV (type_nod, 1);
reference->nod_type = nod_reference;
reference->nod_arg [0] = node;
reference->nod_desc = parm->par_desc;
reference->nod_import = parm;

return reference;
}

static REQ make_request (
    DBB		dbb)
{
/**************************************
 *
 *	m a k e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Make a request block for a database.
 *
 **************************************/
REQ	request;

request = (REQ) ALLOCD (type_req);
request->req_database = dbb;
request->req_next = QLI_requests;
QLI_requests = request;
dbb->dbb_flags |= DBB_active;
if (!(dbb->dbb_transaction))
    MET_transaction (nod_start_trans, dbb);

return request;
}

static void release_message (
    MSG		message)
{
/**************************************
 *
 *	r e l e a s e _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	A message block is unneeded, so release it.
 *
 **************************************/
MSG	*ptr;
REQ	request;

request = message->msg_request;

for (ptr = &request->req_messages; *ptr; ptr = &(*ptr)->msg_next)
    if (*ptr == message)
	break;

if (!*ptr)
    BUGCHECK (364); /* Msg 364 lost message */

*ptr = message->msg_next;
ALL_release (message);
}

static int string_length (
    DSC		*desc)
{
/**************************************
 *
 *	s t r i n g _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Estimate length of string based on descriptor.
 *
 **************************************/

return DSC_string_length (desc);
}
