/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		gener.c
 *	DESCRIPTION:	BLR Generation Module
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
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../jrd/align.h"
#include "../qli/exe.h"
#include "../qli/compile.h"
#include "../qli/report.h"
#include "../qli/all_proto.h"
#include "../qli/compi_proto.h"
#include "../qli/err_proto.h"
#include "../qli/gener_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/mov_proto.h"
#include "../jrd/gds_proto.h"

static void	explain (UCHAR *);
static void	explain_index_tree (SSHORT, TEXT *, SCHAR **, SSHORT *);
static void	explain_printf (SSHORT, TEXT *, TEXT *);
static void	gen_any (NOD, REQ);
static void	gen_assignment (NOD, REQ);
static void	gen_control_break (BRK, REQ);
static void	gen_compile (REQ);
static void	gen_descriptor (DSC *, REQ);
static void	gen_erase (NOD, REQ);
static void	gen_expression (NOD, REQ);
static void	gen_field (NOD, REQ);
static void	gen_for (NOD, REQ);
static void	gen_function (NOD, REQ);
static void	gen_if (NOD, REQ);
static void	gen_literal (DSC *, REQ);
static void	gen_map (MAP, REQ);
static void	gen_modify (NOD, REQ);
static void	gen_parameter (PAR, REQ);
static void	gen_print_list (NOD, REQ);
static void	gen_report (NOD, REQ);
static void	gen_request (REQ);
static void	gen_rse (NOD, REQ);
static void	gen_send_receive (MSG, USHORT);
static void	gen_sort (NOD, REQ, UCHAR);
static void	gen_statement (NOD, REQ);
static void	gen_statistical (NOD, REQ);
static void	gen_store (NOD, REQ);

#ifdef DEV_BUILD
static SCHAR explain_info [] = { gds__info_access_path };
#endif

NOD GEN_generate (
    NOD		node)
{
/**************************************
 *
 *	G E N _ g e n e r a t e
 *
 **************************************
 *
 * Functional description
 *	Generate and compile BLR.
 *
 **************************************/

gen_statement (node, NULL_PTR);

return node;
}

void GEN_release (void)
{
/**************************************
 *
 *	G E N _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Release any compiled requests following execution or abandonment
 *	of a request.  Just recurse around and release requests.
 *
 **************************************/
STATUS	status_vector [20];
RLB	rlb;

for (; QLI_requests; QLI_requests = QLI_requests->req_next)
    {
    if (QLI_requests->req_handle)
	gds__release_request (status_vector, 
		GDS_REF (QLI_requests->req_handle));

    rlb = QLI_requests->req_blr;
    RELEASE_RLB;
    }
}

RLB GEN_rlb_extend (
    RLB	rlb)
{
/**************************************
 *
 *	G E N _ r l b _ e x t e n d
 *
 **************************************
 *
 * Functional description
 *	
 *	Allocate a new larger request language buffer
 *	and copy generated request language into it
 *
 **************************************/
UCHAR	*new_string, *old_string;
ULONG	l, size;

if (!rlb)
    rlb =  (RLB) ALLOCD (type_rlb);

old_string = rlb->rlb_base;
l = (UCHAR*) rlb->rlb_data - (UCHAR*) rlb->rlb_base;
rlb->rlb_length += RLB_BUFFER_SIZE;
new_string = ALLQ_malloc ((SLONG) rlb->rlb_length);
if (old_string)
    {
    MOVQ_fast (old_string, new_string, l);
    ALLQ_free (old_string); 
    }
rlb->rlb_base = new_string;
rlb->rlb_data = new_string + l;
rlb->rlb_limit = rlb->rlb_data + RLB_BUFFER_SIZE - RLB_SAFETY_MARGIN;

return (rlb);
}

void GEN_rlb_release (
    RLB	rlb)
{
/**************************************
 *
 *	G E N _ r l b _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	
 *	Release the request language buffer
 *	contents.  The buffer itself will go away
 *	with the default pool.
 *
 **************************************/

if (!rlb)
    return;

if (rlb->rlb_base)
    {
    ALLQ_free (rlb->rlb_base);
    rlb->rlb_base = NULL; 
    rlb->rlb_length = 0;
    rlb->rlb_data = NULL;
    rlb->rlb_limit = NULL;
    }
}

#ifdef DEV_BUILD
static void explain (
    UCHAR	*explain_buffer)
{
/**************************************
 *
 *	e x p l a i n
 *
 **************************************
 *
 * Functional description
 *	Print out the access path for a blr 
 *	request in pretty-printed form.
 *
 **************************************/
SSHORT	buffer_length, length, level = 0;
SCHAR	relation_name [32], *r;

if (*explain_buffer++ != gds__info_access_path)
    return;

buffer_length = *explain_buffer++;
buffer_length += (*explain_buffer++) << 8;

while (buffer_length > 0)
    {
    buffer_length--;
    switch (*explain_buffer++)
	{
	case gds__info_rsb_begin:
	    explain_printf (level, "gds__info_rsb_begin,\n", NULL_PTR);
	    level++;
	    break;

	case gds__info_rsb_end:
	    explain_printf (level, "gds__info_rsb_end,\n", NULL_PTR);
	    level--;
	    break;  

	case gds__info_rsb_relation:
	    explain_printf (level, "gds__info_rsb_relation, ", NULL_PTR);

	    buffer_length--;
	    buffer_length -= (length = *explain_buffer++);
		
	    r = relation_name;
	    while (length--) 
		{
		*r++ = *explain_buffer;
		ib_putchar (*explain_buffer++);
		}
	    ib_printf (",\n");
	    *r++ = 0;
	    break;

	case gds__info_rsb_type:
	    buffer_length--;
	    explain_printf (level, "gds__info_rsb_type, ", NULL_PTR);
	    switch (*explain_buffer++)
		{
		case gds__info_rsb_unknown:
		    ib_printf ("unknown type\n");
		    break;

		case gds__info_rsb_indexed:
		    ib_printf ("gds__info_rsb_indexed,\n");
  		    level++;
		    explain_index_tree (level, relation_name, &explain_buffer, &buffer_length);
	            level--;
		    break;

		/* for join types, advance past the count byte
                   of substreams; we don't need it */

		case gds__info_rsb_merge:
		    buffer_length--;
		    ib_printf ("gds__info_rsb_merge, %d,\n", *explain_buffer++);
		    break;

		case gds__info_rsb_cross:
		    buffer_length--;
		    ib_printf ("gds__info_rsb_cross, %d,\n", *explain_buffer++);
		    break;

		case gds__info_rsb_navigate:
		    ib_printf ("gds__info_rsb_navigate,\n");
  		    level++;
		    explain_index_tree (level, relation_name, &explain_buffer, &buffer_length);
	            level--;
		    break;

		case gds__info_rsb_sequential:
		    ib_printf ("gds__info_rsb_sequential,\n");
		    break;

		case gds__info_rsb_sort:
		    ib_printf ("gds__info_rsb_sort,\n");
		    break;

		case gds__info_rsb_first:
		    ib_printf ("gds__info_rsb_first,\n");
		    break;

		case gds__info_rsb_boolean:
		    ib_printf ("gds__info_rsb_boolean,\n");
		    break;

		case gds__info_rsb_union:
		    ib_printf ("gds__info_rsb_union,\n");
		    break;

		case gds__info_rsb_aggregate:
		    ib_printf ("gds__info_rsb_aggregate,\n");
		    break;

		case gds__info_rsb_ext_sequential:
		    ib_printf ("gds__info_rsb_ext_sequential,\n");
		    break;

		case gds__info_rsb_ext_indexed:
		    ib_printf ("gds__info_rsb_ext_indexed,\n");
		    break;

		case gds__info_rsb_ext_dbkey:
		    ib_printf ("gds__info_rsb_ext_dbkey,\n");
		    break;

		case gds__info_rsb_left_cross:
		    ib_printf ("gds__info_rsb_left_cross,\n");
		    break;

		case gds__info_rsb_select:
		    ib_printf ("gds__info_rsb_select,\n");
		    break;

		case gds__info_rsb_sql_join:
		    ib_printf ("gds__info_rsb_sql_join,\n");
		    break;

		case gds__info_rsb_simulate:
		    ib_printf ("gds__info_rsb_simulate,\n");
		    break;

		case gds__info_rsb_sim_cross:
		    ib_printf ("gds__info_rsb_sim_cross,\n");
		    break;

		case gds__info_rsb_once:
		    ib_printf ("gds__info_rsb_once,\n");
		    break;

		case gds__info_rsb_procedure:
		    ib_printf ("gds__info_rsb_procedure,\n");
		    break;

		}
	    break;

	default:
	    explain_printf (level, "unknown info item\n", NULL_PTR);
	    break;
	}
    }

}
#endif

#ifdef DEV_BUILD
static void explain_index_tree (
    SSHORT	level,
    TEXT	*relation_name,
    SCHAR	**explain_buffer_ptr,
    SSHORT	*buffer_length)
{
/**************************************
 *
 *	e x p l a i n _ i n d e x _ t r e e
 *
 **************************************
 *
 * Functional description
 *	Print out an index tree access path.
 *
 **************************************/
SCHAR	*explain_buffer; 
TEXT	index_name [32];
SCHAR	index_info [256];
SSHORT	length;

explain_buffer = *explain_buffer_ptr;

(*buffer_length)--;

switch (*explain_buffer++)
    {
    case gds__info_rsb_and:
        explain_printf (level, "gds__info_rsb_and,\n", NULL_PTR);
        level++;
	explain_index_tree (level, relation_name, &explain_buffer, buffer_length);
	explain_index_tree (level, relation_name, &explain_buffer, buffer_length);
        level--;
        break;

    case gds__info_rsb_or:
        explain_printf (level, "gds__info_rsb_or,\n", NULL_PTR);
        level++;
	explain_index_tree (level, relation_name, &explain_buffer, buffer_length);
	explain_index_tree (level, relation_name, &explain_buffer, buffer_length);
        level--;
        break;

    case gds__info_rsb_dbkey:
        explain_printf (level, "gds__info_rsb_dbkey,\n", NULL_PTR);
        break;
            
    case gds__info_rsb_index:
        explain_printf (level, "gds__info_rsb_index, ", NULL_PTR);
        (*buffer_length)--;

	length = (SSHORT) *explain_buffer++;
	strncpy (index_name, explain_buffer, length);
        index_name [length] = 0;

	*buffer_length -= length;
	explain_buffer += length;

	MET_index_info (relation_name, index_name, index_info);
        ib_printf ("%s\n", index_info);
        break;
    }

if (*explain_buffer == gds__info_rsb_end)
    {
    explain_buffer++;
    (*buffer_length)--;
    }    

*explain_buffer_ptr = explain_buffer;
}
#endif


#ifdef DEV_BUILD
static void explain_printf (
    SSHORT	level,
    TEXT	*control,
    TEXT	*string)
{
/**************************************
 *
 *	e x p l a i n _ p r i n t f
 *
 **************************************
 *
 * Functional description
 *	Do a ib_printf with formatting.
 *
 **************************************/
                
while (level--)
    ib_printf ("   ");

if (string) 
    ib_printf (control, string);
else
    ib_printf (control);
}
#endif

static void gen_any (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ a n y
 *
 **************************************
 *
 * Functional description
 *	Generate the BLR for a statistical expresionn.
 *
 **************************************/
REQ	new_request;
RLB	rlb;
FLD	field;
CTX	context;
MSG	send, receive;
DSC	desc;
USHORT	i, value;

/* If there is a request associated with the statement, prepare to
   generate BLR.  Otherwise assume that a request is alrealdy initialized. */

if (new_request = (REQ) node->nod_arg [e_any_request])
    {
    request = new_request;
    gen_request (request);
    if (receive = (MSG) node->nod_arg [e_any_send])
	gen_send_receive (receive, blr_receive);
    send = (MSG) node->nod_arg [e_any_receive];
    gen_send_receive (send, blr_send);
    rlb = CHECK_RLB (request->req_blr);
    STUFF (blr_if);
    }
else
    rlb = CHECK_RLB (request->req_blr);

STUFF (blr_any);
gen_rse (node->nod_arg [e_any_rse], request);

if (new_request)
    {
    desc.dsc_dtype = dtype_short;
    desc.dsc_length = sizeof (SSHORT);
    desc.dsc_scale = 0;
    desc.dsc_sub_type = 0;
    desc.dsc_address = (UCHAR*) &value;

    STUFF (blr_assignment);
    value = TRUE;
    gen_literal (&desc, request);
    gen_parameter (node->nod_import, request);

    STUFF (blr_assignment);
    value = FALSE;
    gen_literal (&desc, request);
    gen_parameter (node->nod_import, request);
    gen_compile (request);
    }
}

static void gen_assignment (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate an assignment statement.
 *
 **************************************/
NOD	target, from, reference, validation;
RLB	rlb;

from = node->nod_arg [e_asn_from];

/* Handle a local expression locally */

if (node->nod_flags & NOD_local)
    {
    gen_expression (from, NULL_PTR);
    return;
    }

/* Generate a remote assignment */

rlb = CHECK_RLB (request->req_blr);

STUFF (blr_assignment);
target = node->nod_arg [e_asn_to];

/* If we are referencing a parameter of another request, compile
   the request first, the make a reference.  Otherwise, just compile
   the expression in the context of this request */

if (reference = target->nod_arg [e_fld_reference])
    {
    gen_expression (from, NULL_PTR);
    gen_parameter (reference->nod_import, request);
    }
else
    gen_expression (from, request);

if (validation = node->nod_arg [e_asn_valid])
    gen_expression (validation, NULL_PTR);

gen_expression (target, request);
}

static void gen_control_break (
    BRK		control,
    REQ		request)
{
/**************************************
 *
 *	g e n _ c o n t r o l _ b r e a k
 *
 **************************************
 *
 * Functional description
 *	Process report writer control breaks.
 *
 **************************************/

for (; control; control = control->brk_next)
    {
    if (control->brk_field)
	gen_expression (control->brk_field, request);
    if (control->brk_line)
	gen_print_list (control->brk_line, request);
    }
}

static void gen_compile (
    REQ		request)
{
/**************************************
 *
 *	g e n _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *	Finish off BLR generation for a request, and get it compiled.
 *
 **************************************/
DBB	dbb;
STATUS	status_vector [20];
USHORT	length;
RLB	rlb;
#ifdef DEV_BUILD
SCHAR	explain_buffer [256];
#endif

rlb = CHECK_RLB (request->req_blr);
STUFF (blr_end);
STUFF (blr_eoc);

if (QLI_blr)
    gds__print_blr (rlb->rlb_base, NULL_PTR, NULL_PTR, 0);
              
length = (UCHAR*) rlb->rlb_data - (UCHAR*) rlb->rlb_base;

dbb = request->req_database;

if (gds__compile_request (status_vector, 
	GDS_REF (dbb->dbb_handle), 
	GDS_REF (request->req_handle),
    	length, 
	GDS_VAL (rlb->rlb_base)))
    {
    RELEASE_RLB;
    ERRQ_database_error (dbb, status_vector);
    }

#ifdef DEV_BUILD
if (QLI_explain && 
    !gds__request_info (
	status_vector,
	GDS_REF (request->req_handle),
	0,
	sizeof (explain_info),
	explain_info,
	sizeof (explain_buffer),
	explain_buffer))
   explain (explain_buffer);
#endif

RELEASE_RLB;
}

static void gen_descriptor (
    DSC		*desc,
    REQ		request)
{
/**************************************
 *
 *	g e n _ d e s c r i p t o r
 *
 **************************************
 *
 * Functional description
 *	Generator a field descriptor.  This is used for generating both
 *	message declarations and literals.
 *
 **************************************/
RLB    rlb;

rlb = CHECK_RLB (request->req_blr);

switch (desc->dsc_dtype)
    {
#if (defined JPN_SJIS || defined JPN_EUC)

    case dtype_text:
        STUFF (blr_text2);
        STUFF_WORD(QLI_interp);
        STUFF_WORD (desc->dsc_length);
        break;

    case dtype_varying:
        STUFF (blr_varying2);
        STUFF_WORD(QLI_interp);
        STUFF_WORD (desc->dsc_length - sizeof (SSHORT));
        break;

    case dtype_cstring:
        STUFF (blr_cstring2);
        STUFF_WORD(QLI_interp);
        STUFF_WORD (desc->dsc_length);
        break;

#else

    case dtype_text:
	STUFF (blr_text);
	STUFF_WORD (desc->dsc_length);
	break;

    case dtype_varying:
	STUFF (blr_varying);
	STUFF_WORD (desc->dsc_length - sizeof (SSHORT));
	break;

    case dtype_cstring:
	STUFF (blr_cstring);
	STUFF_WORD (desc->dsc_length);
	break;

#endif
    
    case dtype_short:
	STUFF (blr_short);
	STUFF (desc->dsc_scale);
	break;

    case dtype_long:
	STUFF (blr_long);
	STUFF (desc->dsc_scale);
	break;

    case dtype_blob:
    case dtype_quad:
	STUFF (blr_quad);
	STUFF (desc->dsc_scale);
	break;

    case dtype_real:
	STUFF (blr_float);
	break;

    case dtype_double:
	STUFF (blr_double);
	break;
    
    case dtype_timestamp:
	STUFF (blr_timestamp);
	break;

    case dtype_sql_date:
	STUFF (blr_sql_date);
	break;

    case dtype_sql_time:
	STUFF (blr_sql_time);
	break;

    default:
	BUGCHECK ( 352 ); /* Msg352 gen_descriptor: dtype not recognized */
    }
}

static void gen_erase (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Generate the BLR for an erase statement.  Pretty simple.
 *
 **************************************/
CTX	context;
MSG	message;
RLB	rlb;

if (message = (MSG) node->nod_arg [e_era_message])
    {
    request = (REQ) node->nod_arg [e_era_request];
    gen_send_receive (message, blr_receive);
    }

rlb = CHECK_RLB (request->req_blr);
context = (CTX) node->nod_arg [e_era_context];
STUFF (blr_erase);
STUFF (context->ctx_context);
}

static void gen_expression (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ e x p r e s s i o n
 *
 **************************************
 *
 * Functional description
 *	Generate the BLR for a boolean or value expression.
 *
 **************************************/
NOD	*ptr, *end;
FLD	field;
CTX	context;
MAP	map;
USHORT	operator;
RLB	rlb;

if (node->nod_flags & NOD_local)
    request = NULL;
/*    return;*/
else 
    if (request)
	rlb = CHECK_RLB (request->req_blr);

switch (node->nod_type)
    {
    case nod_any:
	gen_any (node, request);
	return;

    case nod_unique:
	if (request)
	    {
	    STUFF (blr_unique);
	    gen_rse (node->nod_arg [e_any_rse], request);
	    }
	return;

    case nod_constant:
	if (request)
	    gen_literal (&node->nod_desc, request);
	return;
    
    case nod_field:
	gen_field (node, request);
	return;
    
    case nod_format:
	gen_expression (node->nod_arg [e_fmt_value], request);
	return;

    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	context = (CTX) node->nod_arg [e_map_context];
	if (context->ctx_request != request &&
	    map->map_node->nod_type == nod_field)
	    {
	    gen_field (map->map_node, request);
	    return;
	    }
	STUFF (blr_fid);
	STUFF (context->ctx_context);
	STUFF_WORD (map->map_position);
	return;

    case nod_eql:
	operator = blr_eql;
	break;

    case nod_neq:
	operator = blr_neq;
	break;

    case nod_gtr:
	operator = blr_gtr;
	break;

    case nod_geq:
	operator = blr_geq;
	break;

    case nod_leq:
	operator = blr_leq;
	break;

    case nod_lss:
	operator = blr_lss;
	break;

    case nod_containing:
	operator = blr_containing;
	break;

    case nod_matches:
	operator = blr_matching;
	break;

    case nod_sleuth:
	operator = blr_matching2;
	break;

    case nod_like:
	operator = (node->nod_count == 2) ? blr_like : blr_ansi_like;
	break;

    case nod_starts:
	operator = blr_starting;
	break;

    case nod_missing:
	operator = blr_missing;
	break;

    case nod_between:
	operator = blr_between;
	break;
    
    case nod_and:
	operator = blr_and;
	break;

    case nod_or:
	operator = blr_or;
	break;

    case nod_not:
	operator = blr_not;
	break;

    case nod_add:
	operator = blr_add;
	break;

    case nod_subtract:
	operator = blr_subtract;
	break;

    case nod_multiply:
	operator = blr_multiply;
	break;

    case nod_divide:
	operator = blr_divide;
	break;

    case nod_negate:
	operator = blr_negate;
	break;

    case nod_concatenate:
	operator = blr_concatenate;
	break;

    case nod_substr:
	operator = blr_substring;
	break;

    case nod_function:
	gen_function (node, request);
	return;

    case nod_agg_average:
    case nod_agg_count:
    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_total:

    case nod_average:
    case nod_count:
    case nod_max:
    case nod_min:
    case nod_total:
    case nod_from:
	gen_statistical (node, request);
	return;

    case nod_rpt_average:
    case nod_rpt_count:
    case nod_rpt_max:
    case nod_rpt_min:
    case nod_rpt_total:

    case nod_running_total:
    case nod_running_count:
	if (node->nod_arg [e_stt_value])
	    gen_expression (node->nod_arg [e_stt_value], request);
	if (node->nod_export)
	    gen_parameter (node->nod_export, request);
	request = NULL;
	break;

    case nod_prompt:
    case nod_variable:
    case nod_form_field:
	if (node->nod_export)
	    gen_parameter (node->nod_export, request);
	return;

    case nod_edit_blob:
    case nod_reference:
	return;

    case nod_null:
	operator = blr_null;
	break;

    case nod_user_name:
	operator = blr_user_name;
	break;

    case nod_upcase:
	operator = blr_upcase;
	break;

    default:
	if (request && node->nod_export)
	    {
	    gen_parameter (node->nod_export, request);
	    return;
	    }
	BUGCHECK ( 353 ); /* Msg353 gen_expression: not understood */
    }

if (request)
    {
    rlb = CHECK_RLB (request->req_blr);
    STUFF (operator);
    }

for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
    gen_expression (*ptr, request);

if (!node->nod_desc.dsc_address && node->nod_desc.dsc_length)
    CMP_alloc_temp (node);
}

static void gen_field (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Compile BLR to handle a field reference.  Things varying
 *	substantially depend on whether the field reference spans
 *	requests or not.
 *
 **************************************/
FLD	field;
CTX	context;
RLB	rlb;
NOD	args, *ptr, *end;

/* If there isn't a request specified, this is a reference. */

if (!request)
    return;

rlb = CHECK_RLB (request->req_blr);

field = (FLD) node->nod_arg [e_fld_field];
context = (CTX) node->nod_arg [e_fld_context];

/* If the field referenced is in this request, just generate a field
   reference. */

if (context->ctx_request == request)
    {
    if (args = node->nod_arg [e_fld_subs])
	STUFF (blr_index);
    STUFF (blr_fid);
    STUFF (context->ctx_context);
    STUFF_WORD (field->fld_id);
    if (args)
	{
	STUFF (args->nod_count);
	for (ptr = args->nod_arg, end = ptr + args->nod_count; ptr < end; ptr++)
	    gen_expression (*ptr, request);
	}
    return;
    }

/* The field is in a different request.  Make a parameter reference
   instead. */

gen_parameter (node->nod_export, request);
}

static void gen_for (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate BLR for a FOR loop, included synchronization messages.
 *
 **************************************/
MSG	message, continuation;
PAR	parameter, eof;
DSC	desc;
STR	string;
NOD	sub, *ptr, *end;
RPT	report;
USHORT	value, label;
RLB	rlb;

/* If there is a request associated with the statement, prepare to
   generate BLR.  Otherwise assume that a request is alrealdy initialized. */

if (node->nod_arg [e_for_request])
    {
    request = (REQ) node->nod_arg [e_for_request];
    gen_request (request);
    }
   
rlb = CHECK_RLB (request->req_blr);

/* If the statement requires an end of file marker, build a BEGIN/END around
   the whole statement. */

if (message = (MSG) node->nod_arg [e_for_receive])
    STUFF (blr_begin);

/* If there is a message to be sent, build a receive for it */

if (node->nod_arg [e_for_send])
    gen_send_receive (node->nod_arg [e_for_send], blr_receive);

/* Generate the FOR loop proper. */

STUFF (blr_for);
gen_rse (node->nod_arg [e_for_rse], request);
STUFF (blr_begin);

/* If data is to be received (included EOF), build a send */

if (message)
    {
    gen_send_receive (message, blr_send);
    STUFF (blr_begin);

    /* Build assigments for all values referenced. */

    for (parameter = message->msg_parameters; parameter; 
	parameter = parameter->par_next)
	if (parameter->par_value)
	    {
	    STUFF (blr_assignment);
	    gen_expression (parameter->par_value, request);
	    gen_parameter (parameter, request);
	    }

    /* Next, make a FALSE for the end of file parameter */

    eof = (PAR) node->nod_arg [e_for_eof];
    desc.dsc_dtype = dtype_short;
    desc.dsc_length = sizeof (SSHORT);
    desc.dsc_scale = 0;
    desc.dsc_sub_type = 0;
    desc.dsc_address = (UCHAR*) &value;

    STUFF (blr_assignment);
    value = FALSE;
    gen_literal (&desc, request);
    gen_parameter (eof, request);

    STUFF (blr_end);
    }

/* Build  the body of the loop. */

if (continuation = request->req_continue)
    {
    STUFF (blr_label);
    label = request->req_label++;
    STUFF (label);
    STUFF (blr_loop);
    STUFF (blr_select);
    gen_send_receive (continuation, blr_receive);
    STUFF (blr_leave);
    STUFF (label);
    }

sub = node->nod_arg [e_for_statement];
gen_statement (sub, request);

STUFF (blr_end);

if (continuation)
    STUFF (blr_end);

/* Finish off by building a SEND to indicate end of file */

if (message)
    {
    gen_send_receive (message, blr_send);
    STUFF (blr_assignment);
    value = TRUE;
    gen_literal (&desc, request);
    gen_parameter (eof, request);
    STUFF (blr_end);
    }

/* If this is our request, compile it. */

if (node->nod_arg [e_for_request])
    gen_compile (request);
}

static void gen_function (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate blr for a function reference.
 *
 **************************************/
REQ	new_request;
NOD	args, *ptr, *end;
FUN	function;
SYM	symbol;
CTX	context;
MSG	send, receive;
USHORT	i;
UCHAR	*p;
RLB	rlb;

/* If there is a request associated with the statement, prepare to
   generate BLR.  Otherwise assume that a request is already initialized. */

if (request && (request->req_flags & (REQ_project | REQ_group_by)))
    {
    new_request = NULL;
    rlb = CHECK_RLB (request->req_blr);
    }
else if (new_request = (REQ) node->nod_arg [e_fun_request])
    {
    request = new_request;
    gen_request (request);
    if (receive = (MSG) node->nod_arg [e_fun_send])
	gen_send_receive (receive, blr_receive);
    send = (MSG) node->nod_arg [e_fun_receive];
    gen_send_receive (send, blr_send);
    rlb = CHECK_RLB (request->req_blr);
    STUFF (blr_assignment);
    }
else
    rlb = CHECK_RLB (request->req_blr);

/* Generate function body */

STUFF (blr_function);
function = (FUN) node->nod_arg [e_fun_function];
symbol = function->fun_symbol;
STUFF (symbol->sym_length);
for (p = (UCHAR*) symbol->sym_string; *p;)
    STUFF (*p++);

/* Generate function arguments */

args = node->nod_arg [e_fun_args];
STUFF (args->nod_count);

for (ptr = args->nod_arg, end = ptr + args->nod_count; ptr < end; ptr++)
    gen_expression (*ptr, request);

if (new_request)
    {
    gen_parameter (node->nod_import, request);
    gen_compile (request);
    }
}

static void gen_if (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ i f
 *
 **************************************
 *
 * Functional description
 *	Generate code for an IF statement.  Depending
 *	on the environment, this may be either a QLI
 *	context IF or a database context IF.
 *
 **************************************/
RLB	rlb;

/* If the statement is local to QLI, force the sub-
   statements/expressions to be local also.  If not
   local, generate BLR. */

if (node->nod_flags & NOD_local)
    {
    gen_expression (node->nod_arg [e_if_boolean], NULL_PTR);
    gen_statement (node->nod_arg [e_if_true], request);
    if (node->nod_arg [e_if_false])
	gen_statement (node->nod_arg [e_if_false], request);
    }
else
    {
    rlb = CHECK_RLB (request->req_blr);
    STUFF (blr_if);
    gen_expression (node->nod_arg [e_if_boolean], request);
    STUFF (blr_begin);
    gen_statement (node->nod_arg [e_if_true], request);
    STUFF (blr_end);
    if (node->nod_arg [e_if_false])
	{
	STUFF (blr_begin);
	gen_statement (node->nod_arg [e_if_false], request);
	STUFF (blr_end);  
	}
    else
	STUFF (blr_end);
    }
}

static void gen_literal (
    DSC		*desc,
    REQ		request)
{
/**************************************
 *
 *	g e n _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Generate a literal value expression.
 *
 **************************************/
SLONG	value;
USHORT	l;
UCHAR	*p;
RLB	rlb;

rlb = CHECK_RLB (request->req_blr);

STUFF (blr_literal);
gen_descriptor (desc, request);
p = desc->dsc_address;
l = desc->dsc_length;

switch (desc->dsc_dtype)
    {
    case dtype_short:
	value = *(SSHORT*) p;
	STUFF_WORD (value);
	break;

    case dtype_long:
    case dtype_sql_time:
    case dtype_sql_date:
	value = *(SLONG*) p;
	STUFF_WORD (value);
	STUFF_WORD (value >> 16);
	break;

    case dtype_quad:
    case dtype_timestamp:
	value = *(SLONG*) p;
	STUFF_WORD (value);
	STUFF_WORD (value >> 16);
	value = *(SLONG*) (p + 4);
	STUFF_WORD (value);
	STUFF_WORD (value >> 16);
	break;

    default:
	if (l)
	    do STUFF (*p++); while (--l);
    }
}

static void gen_map (
    MAP		map,
    REQ		request)
{
/**************************************
 *
 *	g e n _ m a p
 *
 **************************************
 *
 * Functional description
 *	Generate a value map for a record selection expression.
 *
 **************************************/
USHORT	count;
MAP	temp;
RLB	rlb;

rlb = CHECK_RLB (request->req_blr);

count = 0;
for (temp = map; temp; temp = temp->map_next)
    if (temp->map_node->nod_type != nod_function)
	temp->map_position = count++;

STUFF (blr_map);
STUFF_WORD (count);

for (temp = map; temp; temp = temp->map_next)
    if (temp->map_node->nod_type != nod_function)
	{
	STUFF_WORD (temp->map_position);
	gen_expression (temp->map_node, request);
	}
}

static void gen_modify (
    NOD		node,
    REQ		org_request)
{
/**************************************
 *
 *	g e n _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Generate a MODIFY statement.  
 *
 **************************************/
REQ	request;
NOD	*ptr, *end;
CTX	context;
RLB    rlb;

request = (REQ) node->nod_arg [e_mod_request];

rlb = CHECK_RLB (request->req_blr);

if (node->nod_arg [e_mod_send])
    gen_send_receive (node->nod_arg [e_mod_send], blr_receive);

for (ptr = &node->nod_arg [e_mod_count], end = ptr + node->nod_count; ptr < end; ptr++)
    {
    context = (CTX) *ptr;
    STUFF (blr_modify);
    STUFF (context->ctx_source->ctx_context);
    STUFF (context->ctx_context);
    }
STUFF (blr_begin);
gen_statement (node->nod_arg [e_mod_statement], request);
STUFF (blr_end);
}


static void gen_parameter (
    PAR		parameter,
    REQ		request)
{
/**************************************
 *
 *	g e n _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Generate a simple parameter reference.
 *
 **************************************/
MSG	message;
RLB	rlb;

rlb = CHECK_RLB (request->req_blr);

message = parameter->par_message;
if (!parameter->par_missing)
    {
    STUFF (blr_parameter);
    STUFF (message->msg_number);
    STUFF_WORD (parameter->par_parameter);
    }
else
    {
    STUFF (blr_parameter2);
    STUFF (message->msg_number);
    STUFF_WORD (parameter->par_parameter);
    STUFF_WORD (parameter->par_missing->par_parameter);
    }
}

static void gen_print_list (
    NOD		list,
    REQ		request)
{
/**************************************
 *
 *	g e n _ p r i n t _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Generate BLR for a print list.
 *
 **************************************/
ITM	item;
NOD	*ptr, *end;

for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
    {
    item = (ITM) *ptr;
    if (item->itm_type == item_value)
	gen_expression (item->itm_value, request);
    }
}

static void gen_report (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ r e p o r t
 *
 **************************************
 *
 * Functional description
 *	Compile the body of a report specification.
 *
 **************************************/
RPT	report;
BRK	control;
NOD	list;

report = (RPT) node->nod_arg [e_prt_list];

gen_control_break (report->rpt_top_rpt, request);
gen_control_break (report->rpt_top_page, request);
gen_control_break (report->rpt_top_breaks, request);

if (list = report->rpt_detail_line)
    gen_print_list (list, request);

gen_control_break (report->rpt_bottom_breaks, request);
gen_control_break (report->rpt_bottom_page, request);
gen_control_break (report->rpt_bottom_rpt, request);
}

static void gen_request (
    REQ		request)
{
/**************************************
 *
 *	g e n _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Prepare to generation and compile a request.
 *
 **************************************/
MSG	message;
PAR	param, missing_param;
STR	string;
DSC	*desc;
USHORT	alignment;
RLB	rlb;

rlb = CHECK_RLB (request->req_blr);

STUFF (blr_version4);
STUFF (blr_begin);

/* Build declarations for all messages. */

for (message = request->req_messages; message; message = message->msg_next)
    {
    message->msg_length = 0;
    for (param = message->msg_parameters; param; param = param->par_next)
	{
	desc = &param->par_desc;
	param->par_parameter = message->msg_parameter++;
	alignment = type_alignments [desc->dsc_dtype];
	if (alignment)
	    message->msg_length = ALIGN (message->msg_length, alignment);
	param->par_offset = message->msg_length;
	message->msg_length += desc->dsc_length;
        if (missing_param = param->par_missing)
	    {
	    missing_param->par_parameter = message->msg_parameter++;
            message->msg_length = ALIGN (message->msg_length, sizeof (USHORT));
            desc = &missing_param->par_desc;
	    missing_param->par_offset = message->msg_length;
	    message->msg_length += desc->dsc_length;
	    }
	}

    STUFF (blr_message);
    STUFF (message->msg_number);
    STUFF_WORD (message->msg_parameter);

    string = (STR) ALLOCDV (type_str, message->msg_length);
    message->msg_buffer = (UCHAR*) string->str_data;

    for (param = message->msg_parameters; param; param = param->par_next)
	{
	gen_descriptor (&param->par_desc, request);
        if (missing_param = param->par_missing)
	    gen_descriptor (&missing_param->par_desc, request);
	}
    }
}

static void gen_rse (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ r s e
 *
 **************************************
 *
 * Functional description
 *	Generate a record selection expression.
 *
 **************************************/
NOD	list, *ptr, *end;
CTX	context;
REL	relation;
USHORT	i;
RLB	rlb;
NOD_T	join_type;

rlb = CHECK_RLB (request->req_blr);

if ((NOD_T) node->nod_arg [e_rse_join_type] == (NOD_T) 0)
    STUFF (blr_rse);
else
    STUFF (blr_rs_stream);
STUFF (node->nod_count);

/* Check for aggregate case */

context = (CTX) node->nod_arg [e_rse_count];

if (context->ctx_sub_rse)
    {
    STUFF (blr_aggregate);
    STUFF (context->ctx_context);
    gen_rse (context->ctx_sub_rse, request);
    STUFF (blr_group_by); 
    if (list = node->nod_arg [e_rse_group_by])
	{
	request->req_flags |= REQ_group_by;
	STUFF (list->nod_count);
	for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	    gen_expression (*ptr, request);
	request->req_flags &= ~REQ_group_by;
	}
    else
	STUFF (0);
    gen_map (context->ctx_map, request);
    if (list = node->nod_arg [e_rse_having])
	{
	STUFF (blr_boolean);
	gen_expression (list, request);
	}
    if (list = node->nod_arg [e_rse_sort])
	gen_sort (list, request, blr_sort);
    STUFF (blr_end);
    return;
    }

/* Make relation clauses for all relations */

for (ptr = &node->nod_arg [e_rse_count], end = ptr + node->nod_count;
     ptr < end; ptr++)
    {
    context = (CTX) *ptr;
    if (context->ctx_stream)
	gen_rse (context->ctx_stream, request);
    else
	{
	relation = context->ctx_relation;
	STUFF (blr_rid);
	STUFF_WORD (relation->rel_id);
	STUFF (context->ctx_context);
	}
    }

/* Handle various clauses */

if (list = node->nod_arg [e_rse_first])
    {
    STUFF (blr_first);
    gen_expression (list, request);
    }

if (list = node->nod_arg [e_rse_boolean])
    {
    STUFF (blr_boolean);
    gen_expression (list, request);
    }

if (list = node->nod_arg [e_rse_sort])
    gen_sort (list, request, blr_sort);

if (list = node->nod_arg [e_rse_reduced])
    gen_sort (list, request, blr_project);

join_type = (NOD_T) node->nod_arg [e_rse_join_type];
if (join_type != (NOD_T) 0 && join_type != nod_join_inner)
    {
    STUFF (blr_join_type);
    if (join_type == nod_join_left)
	STUFF (blr_left);
    else if (join_type == nod_join_right)
	STUFF (blr_right);
    else
	STUFF (blr_full);
    }

STUFF (blr_end);
}

static void gen_send_receive (
    MSG		message,
    USHORT	operator)
{
/**************************************
 *
 *	g e n _ s e n d _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Generate a send or receive statement, excluding body.
 *
 **************************************/
REQ	request;
RLB	rlb;

request = message->msg_request;
rlb = CHECK_RLB (request->req_blr);
STUFF (operator);
STUFF (message->msg_number);
}

static void gen_sort (
    NOD		node,
    REQ		request,
    UCHAR	operator)
{
/**************************************
 *
 *	g e n _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Generate sort or reduced clause.
 *
 **************************************/
NOD	*ptr, *end;
RLB	rlb;

rlb = CHECK_RLB (request->req_blr);

STUFF (operator);
STUFF (node->nod_count);

request->req_flags |= REQ_project;
for (ptr = node->nod_arg, end = ptr + node->nod_count * 2; ptr < end; ptr += 2)
    {
    if (operator == blr_sort)
	STUFF ( (ptr [1]) ? blr_descending : blr_ascending);
    gen_expression (ptr [0], request);
    }
request->req_flags &= ~REQ_project;
}

static void gen_statement (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate BLR for statement.
 *
 **************************************/
NOD	*ptr, *end;
RLB	rlb;

if (request)
    CHECK_RLB (request->req_blr);

switch (node->nod_type)
    {
    case nod_abort:
	if (node->nod_count)
	    gen_expression (node->nod_arg [0], NULL_PTR);
	return;

    case nod_assign:
	gen_assignment (node, request);
	return;

    case nod_commit_retaining:
	return;

    case nod_erase:
	gen_erase (node, request);
	return;

    case nod_for:
    case nod_report_loop:
	gen_for (node, request);
	return;

    case nod_list:
	for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	    gen_statement (*ptr, request);
	return;

    case nod_modify:
	gen_modify (node, request);
	return;

    case nod_output:
	gen_statement (node->nod_arg [e_out_statement], request);
	return;

    case nod_print:
	gen_print_list (node->nod_arg [e_prt_list], request);
	return;
    
    case nod_repeat:
	gen_statement (node->nod_arg [e_rpt_statement], request);
	return;

    case nod_report:
	gen_report (node, request);
	return;

    case nod_store:
	gen_store (node, request);
	return;

    case nod_if:
	gen_if (node, request);
	return;

    case nod_form_for:
	gen_statement (node->nod_arg [e_ffr_statement], request);
	return;

    case nod_form_update:
	if (node->nod_arg [e_fup_tag])
	    gen_expression (node->nod_arg [e_fup_tag], NULL_PTR);
	return;

    default:
	BUGCHECK ( 354 ); /* Msg354 gen_statement: not yet implemented */
    }
}

static void gen_statistical (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ s t a t i s t i c a l
 *
 **************************************
 *
 * Functional description
 *	Generate the BLR for a statistical expresionn.
 *
 **************************************/
REQ	new_request;
FLD	field;
CTX	context;
MSG	send, receive;
USHORT	operator, i;
RLB	rlb;

switch (node->nod_type)
    {
    case nod_average:
	operator = blr_average;
	break;

    case nod_count:
/* count2
	operator = node->nod_arg [e_stt_value] ? blr_count2 : blr_count;
*/
	operator = blr_count;
	break;

    case nod_max:
	operator = blr_maximum;
	break;

    case nod_min:
	operator = blr_minimum;
	break;

    case nod_total:
	operator = blr_total;
	break;

    case nod_agg_average:
	operator = blr_agg_average;
	break;

    case nod_agg_count:
/* count2
	operator = node->nod_arg [e_stt_value] ? blr_agg_count2 : blr_agg_count;
*/
	operator = blr_agg_count;
	break;

    case nod_agg_max:
	operator = blr_agg_max;
	break;

    case nod_agg_min:
	operator = blr_agg_min;
	break;

    case nod_agg_total:
	operator = blr_agg_total;
	break;

    case nod_from:
	operator = (node->nod_arg [e_stt_default]) ? blr_via : blr_from;
	break;

    default:
	BUGCHECK ( 355 ); /* Msg355 gen_statistical: not understood */
    }

/* If there is a request associated with the statement, prepare to
   generate BLR.  Otherwise assume that a request is alrealdy initialized. */

if (new_request = (REQ) node->nod_arg [e_stt_request])
    {
    request = new_request;
    gen_request (request);
    if (receive = (MSG) node->nod_arg [e_stt_send])
	gen_send_receive (receive, blr_receive);
    send = (MSG) node->nod_arg [e_stt_receive];
    gen_send_receive (send, blr_send);
    rlb = CHECK_RLB (request->req_blr);
    STUFF (blr_assignment);
    }
else     
    rlb = CHECK_RLB (request->req_blr);

STUFF (operator);

if (node->nod_arg [e_stt_rse])
    gen_rse (node->nod_arg [e_stt_rse], request);

/*count 2
if (node->nod_arg [e_stt_value])
*/
if (node->nod_arg [e_stt_value] &&
    node->nod_type != nod_agg_count)
    gen_expression (node->nod_arg [e_stt_value], request);

if (node->nod_arg [e_stt_default])
    gen_expression (node->nod_arg [e_stt_default], request);

if (new_request)
    {
    gen_parameter (node->nod_import, request);
    gen_compile (request);
    }
}

static void gen_store (
    NOD		node,
    REQ		request)
{
/**************************************
 *
 *	g e n _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate code for STORE statement.
 *
 **************************************/
MSG	message;
REL	relation;
CTX	context;
RLB	rlb;

/* If there is a request associated with the statement, prepare to
   generate BLR.  Otherwise assume that a request is alrealdy initialized. */

if (node->nod_arg [e_sto_request])
    {
    request = (REQ) node->nod_arg [e_sto_request];
    gen_request (request);
    }
    
rlb = CHECK_RLB (request->req_blr);

/* If there is a message to be sent, build a receive for it */

if (node->nod_arg [e_sto_send])
    gen_send_receive (node->nod_arg [e_sto_send], blr_receive);

/* Generate the STORE statement proper. */

STUFF (blr_store);
context = (CTX) node->nod_arg [e_sto_context];
relation = context->ctx_relation;
STUFF (blr_rid);
STUFF_WORD (relation->rel_id);
STUFF (context->ctx_context);

/* Build  the body of the loop. */

STUFF (blr_begin);
gen_statement (node->nod_arg [e_sto_statement], request);
STUFF (blr_end);

/* If this is our request, compile it. */

if (node->nod_arg [e_sto_request])
    gen_compile (request);
}

