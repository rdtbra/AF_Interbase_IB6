/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		par.c
 *	DESCRIPTION:	BLR Parser
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
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/jrd.h"
#include "../jrd/gds.h"
#include "../jrd/val.h"
#include "../jrd/align.h"
#include "../jrd/exe.h"
#include "../jrd/lls.h"

#include "../jrd/scl.h"
#include "../jrd/all.h"
#include "../jrd/req.h"
#include "../jrd/blb.h"
#include "../jrd/intl.h"
#include "../jrd/met.h"
#include "../jrd/all_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/fun_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/thd_proto.h"

/* blr type classes */

#define OTHER		0
#define STATEMENT	1
#define BOOL		2
#define VALUE		3
#define TYPE_RSE	4
#define RELATION	5
#define ACCESS_TYPE	6

#include "../jrd/blrtable.h"

static CONST TEXT	elements [][10] = { "", "statement", "boolean", "value", "RSE", "TABLE"};
static CONST struct {
    CONST SCHAR	*code_string;
    SLONG	code_number;
} FAR_VARIABLE codes [] = {
#include "../jrd/codetext.h"
    NULL, 0
};

static void	error (CSB, ...);
static SSHORT	find_proc_field (PRC, TEXT *);
static NOD	par_args (TDBB, CSB *, USHORT);
static NOD	par_cast (TDBB, CSB *);
static XCP	par_condition (TDBB, CSB *);
static XCP	par_conditions (TDBB, CSB *);
static SSHORT	par_context (CSB *, SSHORT *);
static void	par_dependency (TDBB, CSB *, SSHORT, SSHORT, TEXT *);
static NOD	par_exec_proc (TDBB, CSB *, SSHORT);
static NOD	par_fetch (TDBB, CSB *, NOD);
static NOD	par_field (TDBB, CSB *, SSHORT);
static NOD	par_function (TDBB, CSB *);
static NOD	par_literal (TDBB, register CSB *);
static NOD	par_map (TDBB, CSB *, USHORT);
static NOD	par_message (TDBB, CSB *);
static NOD	par_modify (TDBB, CSB *);
static USHORT	par_name (CSB *, TEXT *);
static NOD	par_plan (TDBB, CSB *);
static NOD	par_procedure (TDBB, CSB *, SSHORT);
static void	par_procedure_parms (TDBB, CSB *, PRC, NOD *, NOD *, USHORT);
static NOD	par_relation (TDBB, CSB *, SSHORT, BOOLEAN);
static NOD	par_rse (TDBB, CSB *, SSHORT);
static NOD	par_sort (TDBB, CSB *, BOOLEAN);
static NOD	par_stream (TDBB, CSB *);
static NOD	par_union (TDBB, CSB *);
static USHORT	par_word (CSB *);
static NOD	parse (TDBB, register CSB *, USHORT);
static void	syntax_error (CSB, CONST TEXT *);
static void	warning (CSB, ...);

#define BLR_PEEK	*((*csb)->csb_running)
#define BLR_BYTE	*((*csb)->csb_running)++
#define BLR_PUSH	((*csb)->csb_running)--
#define BLR_WORD	par_word (csb)

NOD PAR_blr (
    TDBB	tdbb,
    REL		relation,
    UCHAR	*blr,
    CSB		view_csb,
    CSB		*csb_ptr,
    REQ		*request_ptr,
    BOOLEAN	trigger,
    USHORT	flags)
{
/**************************************
 *
 *	P A R _ b l r
 *
 **************************************
 *
 * Functional description
 *	Parse blr, returning a compiler scratch block with the results.
 *	Caller must do pool handling.
 *
 **************************************/
CSB		csb;
NOD		node;
SSHORT		version, stream, count;
struct csb_repeat	*t1, *t2, *end;

SET_TDBB (tdbb);

if (!(csb_ptr && (csb = *csb_ptr)))
    {
    count = 5;
    if (view_csb) 
	count += view_csb->csb_count;
    csb = (CSB) ALLOCDV (type_csb, count);
    csb->csb_count = count;
    csb->csb_g_flags |= flags;
    }

/* If there is a request ptr, this is a trigger.  Set up contexts 0 and 1 for
   the target relation */

if (trigger)
    {
    stream = csb->csb_n_stream++;
    t1 = CMP_csb_element (&csb, 0);
    t1->csb_flags |= csb_used | csb_active | csb_trigger;
    t1->csb_relation = relation;
    t1->csb_stream = (UCHAR) stream;

    stream = csb->csb_n_stream++;
    t1 = CMP_csb_element (&csb, 1);
    t1->csb_flags |= csb_used | csb_active | csb_trigger;
    t1->csb_relation = relation;
    t1->csb_stream = (UCHAR) stream;
    }
else
    {
    ++csb->csb_n_stream;
    csb->csb_rpt [0].csb_relation = relation;
    }

csb->csb_running = csb->csb_blr = blr;

if (view_csb)
    {
    t1 = view_csb->csb_rpt;
    end = t1 + view_csb->csb_n_stream;
    for (stream = 0; t1 < end; t1++, stream++)
	{
	t2 = CMP_csb_element (&csb, stream);
	t2->csb_relation = t1->csb_relation;
	t2->csb_stream = t1->csb_stream;
	}
    csb->csb_n_stream = view_csb->csb_n_stream;
    }

version = *csb->csb_running++;

if (version != blr_version4 && version != blr_version5)
   error (csb, gds__metadata_corrupt,
	gds_arg_gds, gds__wroblrver, 
	gds_arg_number, (SLONG) blr_version4, 
	gds_arg_number, (SLONG) version,
	0);

if (version == blr_version4)
    csb->csb_g_flags |= csb_blr_version4;

node = parse (tdbb, &csb, OTHER);
csb->csb_node = node;

if (*csb->csb_running++ != (UCHAR) blr_eoc)
   syntax_error (csb, "end_of_command");

if (request_ptr)
    *request_ptr = CMP_make_request (tdbb, &csb);

if (csb_ptr)
    *csb_ptr = csb;
else
    ALL_RELEASE (csb);

return node;
}

int PAR_desc (
    CSB		*csb,
    DSC		*desc)
{
/**************************************
 *
 *	P A R _ d e s c 
 *
 **************************************
 *
 * Functional description
 *	Parse a BLR descriptor.  Return the alignment requirements
 *	of the datatype.
 *
 **************************************/
USHORT	dtype;

desc->dsc_scale = 0;
desc->dsc_sub_type = 0;
desc->dsc_address = (UCHAR*) NULL_PTR;
desc->dsc_flags = 0;

switch (dtype = BLR_BYTE)
    {
    case blr_text:
	desc->dsc_dtype = dtype_text;
	desc->dsc_flags |= DSC_no_subtype;
	desc->dsc_length = BLR_WORD;
	INTL_ASSIGN_TTYPE (desc, ttype_dynamic);
	break;

    case blr_cstring:
	desc->dsc_dtype = dtype_cstring;
	desc->dsc_flags |= DSC_no_subtype;
	desc->dsc_length = BLR_WORD;
	INTL_ASSIGN_TTYPE (desc, ttype_dynamic);
	break;

    case blr_varying:
	desc->dsc_dtype = dtype_varying;
	desc->dsc_flags |= DSC_no_subtype;
	desc->dsc_length = BLR_WORD + sizeof (USHORT);
	INTL_ASSIGN_TTYPE (desc, ttype_dynamic);
	break;

    case blr_text2:
	desc->dsc_dtype = dtype_text;
	INTL_ASSIGN_TTYPE (desc, BLR_WORD);
	desc->dsc_length = BLR_WORD;
	break;

    case blr_cstring2:
	desc->dsc_dtype = dtype_cstring;
	INTL_ASSIGN_TTYPE (desc, BLR_WORD);
	desc->dsc_length = BLR_WORD;
	break;

    case blr_varying2:
	desc->dsc_dtype = dtype_varying;
	INTL_ASSIGN_TTYPE (desc, BLR_WORD);
	desc->dsc_length = BLR_WORD + sizeof (USHORT);
 	break;

    case blr_short:
	desc->dsc_dtype = dtype_short;
	desc->dsc_length = sizeof (SSHORT);
	desc->dsc_scale = (int) BLR_BYTE;
	break;

    case blr_long:
	desc->dsc_dtype = dtype_long;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_scale = (int) BLR_BYTE;
	break;

    case blr_int64:
	desc->dsc_dtype = dtype_int64;
	desc->dsc_length = sizeof (SINT64);
	desc->dsc_scale = (int) BLR_BYTE;
	break;

    case blr_quad:
	desc->dsc_dtype = dtype_quad;
	desc->dsc_length = sizeof (GDS__QUAD);
	desc->dsc_scale = (int) BLR_BYTE;
	break;

    case blr_float:
	desc->dsc_dtype = dtype_real;
	desc->dsc_length = sizeof (float);
	break;

    case blr_timestamp:
	desc->dsc_dtype = dtype_timestamp;
	desc->dsc_length = sizeof (GDS__QUAD);
	break;

    case blr_sql_date:
	desc->dsc_dtype = dtype_sql_date;
	desc->dsc_length = type_lengths [dtype_sql_date];
	break;

    case blr_sql_time:
	desc->dsc_dtype = dtype_sql_time;
	desc->dsc_length = type_lengths [dtype_sql_time];
	break;

    case blr_double:
#ifndef VMS
    case blr_d_float:
#endif
	desc->dsc_dtype = dtype_double;
	desc->dsc_length = sizeof (double);
	break;

#ifdef VMS
    case blr_d_float:
	desc->dsc_dtype = dtype_d_float;
	desc->dsc_length = sizeof (double);
	break;
#endif

    default:
	if (dtype == blr_blob)
	    {
	    desc->dsc_dtype = dtype_blob;
	    desc->dsc_length = sizeof (GDS__QUAD);
	    break;
	    }
	error (*csb, gds__datnotsup, 0);
    }

return type_alignments [desc->dsc_dtype];
}

NOD PAR_gen_field (
    TDBB	tdbb,
    USHORT	stream,
    USHORT	id)
{
/**************************************
 *
 *	P A R _ g e n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate a field block.
 *
 **************************************/
NOD	node;

SET_TDBB (tdbb);

node = (NOD) ALLOCDV (type_nod, e_fld_length);
node->nod_type = nod_field;
node->nod_arg [e_fld_id] = (NOD) (SLONG) id;
node->nod_arg [e_fld_stream] = (NOD) (SLONG) stream;

return node;
}

NOD PAR_make_field (
    TDBB	tdbb,
    CSB		csb,
    USHORT	context,
    TEXT	*base_field)
{
/**************************************
 *
 *	P A R _ m a k e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make up a field node in the permanent pool.  This is used
 *	by MET_scan_relation to handle view fields.
 *
 **************************************/
SSHORT	id;
USHORT	stream;
TEXT	name [32], *p;
FLD     field;
REL     temp_rel;
NOD     temp_node;

SET_TDBB (tdbb);

stream = csb->csb_rpt [context].csb_stream;

for (p = name; *base_field && *base_field != ' ';)
    *p++ = *base_field++;
*p = 0;

id = MET_lookup_field (tdbb, csb->csb_rpt [stream].csb_relation, name);

if (id < 0)
    return NULL;

temp_rel = csb->csb_rpt [stream].csb_relation;

/* If rel_fields is NULL this means that the relation is
 * in a temporary state (partially loaded).  In this case
 * there is nothing we can do but post an error and exit.
 * Note: This will most likely happen if we have a large list
 * of deffered work which can not complete because of some
 * error, and while we are trying to commit, we find 
 * that we have a dependency on something later in the list.
 * IF there were no error, then the dependency woyld have
 * been resolved, because we would have fully loaded the
 * relation, but if it can not be loaded, then we have this
 * problem. The only thing that can be done to remedy this
 * problem is to rollback.  This will clear the dfw list and
 * allow the user to remedy the original error.  Note: it would
 * be incorrect for us (the server) to perform the rollback
 * implicitly, because this is a task for the user to do, and
 * should never be decided by the server. This fixes bug 10052 */
if (!temp_rel->rel_fields)
    {
    ERR_post(isc_depend_on_uncommitted_rel, 0);
    }

temp_node = PAR_gen_field (tdbb, stream, id);

if (field = (FLD) temp_rel->rel_fields->vec_object[id])
    {
    if (field->fld_default_value && field->fld_not_null)
	temp_node->nod_arg [e_fld_default_value] = field->fld_default_value;
    }

return temp_node;
}

NOD PAR_make_list (
    TDBB	tdbb,
    LLS		stack)
{
/**************************************
 *
 *	P A R _ m a k e _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Make a list node out of a stack.
 *
 **************************************/
NOD	node, *ptr;
LLS	temp;
USHORT	count;

SET_TDBB (tdbb);

/* Count the number of nodes */

for (temp = stack, count = 0; temp; count++)
    temp = temp->lls_next;

node = PAR_make_node (tdbb, count);
node->nod_type = nod_list;
ptr = node->nod_arg + count;

while (stack)
    *--ptr = (NOD) LLS_POP (&stack);

return node;
}

NOD PAR_make_node (
    TDBB	tdbb,
    int	size)
{
/**************************************
 *
 *	P A R _ m a k e _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Make a node element and pass it back.
 *
 **************************************/
NOD	node;

SET_TDBB (tdbb);

node = (NOD) ALLOCDV (type_nod, size);
node->nod_count = size;

return node;
}

CSB PAR_parse (
    TDBB	tdbb,
    UCHAR	*blr,
    USHORT	internal_flag)
{
/**************************************
 *
 *	P A R _ p a r s e
 *
 **************************************
 *
 * Functional description
 *	Parse blr, returning a compiler scratch block with the results.
 *
 **************************************/
NOD	node;
CSB	csb;
SSHORT	version;

SET_TDBB (tdbb);

csb = (CSB) ALLOCDV (type_csb, 5);
csb->csb_count = 5;
csb->csb_running = csb->csb_blr = blr;
version = *csb->csb_running++;
if (internal_flag)
    csb->csb_g_flags |= csb_internal;

if (version != blr_version4 && version!= blr_version5)
   error (csb, gds__wroblrver, 
	gds_arg_number, (SLONG) blr_version4, 
	gds_arg_number, (SLONG) version, 
	0);

if (version == blr_version4)
    csb->csb_g_flags |= csb_blr_version4;

node = parse (tdbb, &csb, OTHER);
csb->csb_node = node;

if (*csb->csb_running++ != (UCHAR) blr_eoc)
   syntax_error (csb, "end_of_command");

return csb;
}

SLONG PAR_symbol_to_gdscode (
    SCHAR	*name)
{
/**************************************
 *
 *	P A R _ s y m b o l _ t o _ g d s c o d e
 *
 **************************************
 *
 * Functional description
 *	Symbolic ASCII names are used in blr for posting and handling
 *	exceptions.  They are also used to identify error codes
 *	within system triggers in a database.
 *
 *	Returns the gds error status code for the given symbolic
 *	name, or 0 if not found.
 *
 *	Symbolic names may be null or space terminated.
 *
 **************************************/
int	i;
int	length;
SCHAR	*p;

p = name;
while (*p && *p != ' ')
    p++;
length = p - name;
for (i = 0; codes [i].code_number; i++)
    if (!strncmp (name, codes[i].code_string, length))
	return codes [i].code_number;

return 0;
}

static void error (
    CSB		csb,
    ...)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	We've got a blr error other than a syntax error.  Handle it.
 *
 **************************************/
TDBB	tdbb;
STATUS	*p;
USHORT	offset; 
int	type;
va_list	args;

/* Don't bother to pass tdbb for error handling */
tdbb = GET_THREAD_DATA;

VA_START (args, csb);

csb->csb_running--;
offset = csb->csb_running - csb->csb_blr;
p = tdbb->tdbb_status_vector;
*p++ = gds_arg_gds;
*p++ = gds__invalid_blr;
*p++ = gds_arg_number;
*p++ = offset;

*p++ = gds_arg_gds;
*p++ = va_arg (args, STATUS);

/* Pick up remaining args */

while (*p++ = type = va_arg (args, int))
    switch (type)
	{
	case gds_arg_gds:
	    *p++ = (STATUS) va_arg (args, STATUS);
	    break;

	case gds_arg_string:
	case gds_arg_interpreted:
	    *p++ = (STATUS) va_arg (args, TEXT*);
	    break;

	case gds_arg_cstring:
	    *p++ = (STATUS) va_arg (args, int);
	    *p++ = (STATUS) va_arg (args, TEXT*);
	    break;

	case gds_arg_number:
	    *p++ = (STATUS) va_arg (args, SLONG);
	    break;

	default:
	    assert (FALSE);
	case gds_arg_vms:
	case gds_arg_unix:
	case gds_arg_win32:
	case gds_arg_dos:
	case gds_arg_netware:
	case gds_arg_domain:
	case gds_arg_mpexl:
	    *p++ = va_arg (args, int);
	    break;
	}

/* Give up whatever we were doing and return to the user. */

ERR_punt();
}

static SSHORT find_proc_field (
    PRC		procedure,
    TEXT	*name)
{
/**************************************
 *
 *	f i n d _ p r o c _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Look for named field in procedure output fields.
 *
 **************************************/
VEC	list;
BLK	*ptr, *end;
PRM	param;

list = procedure->prc_output_fields;
for (ptr = list->vec_object, end = ptr + list->vec_count - 1; ptr < end; ptr++)
    {
    param = (PRM) *ptr;
    if (!strcmp (name, param->prm_name))
	return param->prm_number;
    }

return -1;
}

static NOD par_args (
    TDBB	tdbb,
    CSB		*csb,
    USHORT	expected)
{
/**************************************
 *
 *	p a r _ a r g s
 *
 **************************************
 *
 * Functional description
 *	Parse a counted argument list.
 *
 **************************************/
NOD	node, *ptr;
USHORT	count;

SET_TDBB (tdbb);


count = BLR_BYTE;
node = PAR_make_node (tdbb, count);
node->nod_type = nod_list;
ptr = node->nod_arg;

if (count)
    do *ptr++ = parse (tdbb, csb, expected); while (--count);

return node;
}

static NOD par_cast (
    TDBB	tdbb,
    CSB		*csb)
{
/**************************************
 *
 *	p a r _ c a s t
 *
 **************************************
 *
 * Functional description
 *	Parse a datatype cast
 *
 **************************************/
NOD	node;
FMT	format;
DSC	*desc;

SET_TDBB (tdbb);

node = PAR_make_node (tdbb, e_cast_length);
node->nod_count = count_table [blr_cast];

format = (FMT) ALLOCDV (type_fmt, 1);
format->fmt_count = 1;
node->nod_arg [e_cast_fmt] = (NOD) format;

desc = &format->fmt_desc[0];
PAR_desc (csb, desc);
format->fmt_length = desc->dsc_length;

node->nod_arg [e_cast_source] = parse (tdbb, csb, VALUE);

return node;
}

static XCP par_condition (
    TDBB	tdbb,
    CSB	*csb)
{
/**************************************
 *
 *	p a r _ c o n d i t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse an error conditions list.
 *
 **************************************/
XCP	exception_list;
NOD	dep_node;
USHORT	code_type;
TEXT	name[32], *p;
SLONG	code_number;

SET_TDBB (tdbb);

/* allocate a node to represent the conditions list */

exception_list = (XCP) ALLOCDV (type_xcp, 1);
exception_list->xcp_count = 1;
code_type = BLR_BYTE;
switch (code_type)
    {
    case blr_sql_code:
	exception_list->xcp_rpt[0].xcp_type = xcp_sql_code;
	exception_list->xcp_rpt[0].xcp_code = (SSHORT) BLR_WORD;
	break;

    case blr_gds_code:
	exception_list->xcp_rpt[0].xcp_type = xcp_gds_code;
	par_name (csb, name);
	for (p = name; *p; *p++)
	    *p = LOWWER (*p);
	code_number = PAR_symbol_to_gdscode (name);
	if (code_number)
	    exception_list->xcp_rpt[0].xcp_code = code_number;
	else
	    error (*csb, gds__codnotdef, 
		gds_arg_string, ERR_cstring (name), 
		0);
	break;

    case blr_exception:
	exception_list->xcp_rpt[0].xcp_type = xcp_xcp_code;
	par_name (csb, name);
	if ( !(exception_list->xcp_rpt[0].xcp_code =
		    MET_lookup_exception_number (tdbb, name)))
	    error (*csb, gds__xcpnotdef, 
		gds_arg_string, ERR_cstring (name), 
		0);
	dep_node = PAR_make_node (tdbb, e_dep_length);
	dep_node->nod_type = nod_dependency;
	dep_node->nod_arg [e_dep_object] =
		(NOD) exception_list->xcp_rpt[0].xcp_code;
	dep_node->nod_arg [e_dep_object_type] = (NOD) obj_exception;
	LLS_PUSH (dep_node, &(*csb)->csb_dependencies);
	break;

    default:
	assert (FALSE);
	break;
    }

return exception_list;
}

static XCP par_conditions (
    TDBB	tdbb,
    CSB	*csb)
{
/**************************************
 *
 *	p a r _ c o n d i t i o n s
 *
 **************************************
 *
 * Functional description
 *	Parse an error conditions list.
 *
 **************************************/
XCP	exception_list;
NOD	dep_node;
USHORT	n, i, code_type;
TEXT	name[32], *p;
SLONG	code_number;

SET_TDBB (tdbb);

/* allocate a node to represent the conditions list */

n = BLR_WORD;
exception_list = (XCP) ALLOCDV (type_xcp, n);
exception_list->xcp_count = n;
for (i = 0; i < n; i++)
    {
    code_type = BLR_BYTE;
    switch (code_type)
	{
	case blr_sql_code:
	    exception_list->xcp_rpt[i].xcp_type = xcp_sql_code;
	    exception_list->xcp_rpt[i].xcp_code = (SSHORT) BLR_WORD;
	    break;

	case blr_gds_code:
	    exception_list->xcp_rpt[i].xcp_type = xcp_gds_code;
	    par_name (csb, name);
	    for (p = name; *p; *p++)
		*p = LOWWER (*p);
	    code_number = PAR_symbol_to_gdscode (name);
	    if (code_number)
		exception_list->xcp_rpt[i].xcp_code = code_number;
	    else
		error (*csb, gds__codnotdef, 
		    gds_arg_string, ERR_cstring (name), 
		    0);
	    break;

	case blr_exception:
	    exception_list->xcp_rpt[i].xcp_type = xcp_xcp_code;
	    par_name (csb, name);
	    if ( !(exception_list->xcp_rpt[i].xcp_code =
			MET_lookup_exception_number (tdbb, name)))
		error (*csb, gds__xcpnotdef, 
		    gds_arg_string, ERR_cstring (name), 
		    0);
	    dep_node = PAR_make_node (tdbb, e_dep_length);
	    dep_node->nod_type = nod_dependency;
	    dep_node->nod_arg [e_dep_object] =
		    (NOD) exception_list->xcp_rpt[0].xcp_code;
	    dep_node->nod_arg [e_dep_object_type] = (NOD) obj_exception;
	    LLS_PUSH (dep_node, &(*csb)->csb_dependencies);
	    break;

	case blr_default_code:
	    exception_list->xcp_rpt[i].xcp_type = xcp_default;
	    exception_list->xcp_rpt[i].xcp_code = 0;
	    break;

	default:
	    assert (FALSE);
	    break;
	}
    }

return exception_list;
}

static SSHORT par_context (
    CSB		*csb,
    SSHORT	*context_ptr)
{
/**************************************
 *
 *	p a r _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Introduce a new context into the system.  This involves 
 *	assigning a stream and possibly extending the compile 
 *	scratch block.
 *
 **************************************/
SSHORT			context, stream;
struct csb_repeat	*tail;

stream = (*csb)->csb_n_stream++;
context = (unsigned int) BLR_BYTE;
CMP_csb_element (csb, stream);
tail = CMP_csb_element (csb, context);

if (tail->csb_flags & csb_used)
    error (*csb, gds__ctxinuse, 0);

tail->csb_flags |= csb_used;
tail->csb_stream = (UCHAR) stream;

if (context_ptr)
    *context_ptr = context;
                 
return stream;
}

static void par_dependency (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT	stream,
    SSHORT	id,
    TEXT	*field_name)
{
/**************************************
 *
 *	p a r _ d e p e n d e n c y
 *
 **************************************
 *
 * Functional description
 *	Register a field, relation, procedure or exception reference
 *	as a dependency.
 *
 **************************************/
NOD	node, field_node;
STR	string;
int	length;

SET_TDBB (tdbb);

node = PAR_make_node (tdbb, e_dep_length);
node->nod_type = nod_dependency;
if ((*csb)->csb_rpt [stream].csb_relation)
    {
    node->nod_arg [e_dep_object] = (NOD) (*csb)->csb_rpt [stream].csb_relation;
    node->nod_arg [e_dep_object_type] = (NOD) obj_relation;
    }
else if ((*csb)->csb_rpt [stream].csb_procedure)
    {
    node->nod_arg [e_dep_object] = (NOD) (*csb)->csb_rpt [stream].csb_procedure;
    node->nod_arg [e_dep_object_type] = (NOD) obj_procedure;
    }

if (field_name)
    {
    node->nod_arg [e_dep_field] = field_node = PAR_make_node (tdbb, 1);
    field_node->nod_type = nod_literal;
    length = strlen (field_name);
    string = (STR) ALLOCDV (type_str, length);
    string->str_length = length;
    strcpy (string->str_data, field_name);
    field_node->nod_arg [0] = (NOD) string->str_data;
    }
else if (id >= 0)
    {
    node->nod_arg [e_dep_field] = field_node = PAR_make_node (tdbb, 1);
    field_node->nod_type = nod_field;
    field_node->nod_arg [0] = (NOD) (SLONG) id;
    }

LLS_PUSH (node, &(*csb)->csb_dependencies);
}

static NOD par_exec_proc (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT	operator)
{
/**************************************
 *
 *	p a r _ e x e c _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Parse an execute procedure  reference.
 *
 **************************************/
NOD	node, dep_node;
PRC	procedure;

SET_TDBB (tdbb);

procedure = NULL;
{
TEXT	name [32];
USHORT	pid;

if (operator == blr_exec_pid)
    {
    pid = BLR_WORD;
    if (!(procedure = MET_lookup_procedure_id (tdbb, pid, FALSE, 0)))
	sprintf (name, "id %d", pid);
    }
else
    {
    par_name (csb, name);
    procedure = MET_lookup_procedure (tdbb, name);
    }
if (!procedure)
    error (*csb,
	gds__prcnotdef, 
	gds_arg_string, ERR_cstring (name), 
	0);
}

node = PAR_make_node (tdbb, e_esp_length);
node->nod_type = nod_exec_proc;
node->nod_count = count_table [blr_exec_proc];
node->nod_arg [e_esp_procedure] = (NOD) procedure;

par_procedure_parms (tdbb, csb, procedure, &node->nod_arg [e_esp_in_msg],
    &node->nod_arg [e_esp_inputs], TRUE);
par_procedure_parms (tdbb, csb, procedure, &node->nod_arg [e_esp_out_msg],
    &node->nod_arg [e_esp_outputs], FALSE);

dep_node = PAR_make_node (tdbb, e_dep_length);
dep_node->nod_type = nod_dependency;
dep_node->nod_arg [e_dep_object] = (NOD) procedure;
dep_node->nod_arg [e_dep_object_type] = (NOD) obj_procedure;

LLS_PUSH (dep_node, &(*csb)->csb_dependencies);

return node;
}

static NOD par_fetch (
    TDBB	tdbb,
    CSB		*csb,
    NOD		for_node)
{
/**************************************
 *
 *	p a r _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Parse a FETCH statement, and map it into
 *
 *	    FOR x IN relation WITH x.DBKEY EQ value ...
 *
 **************************************/
RSE	rse;
NOD	relation, node;

SET_TDBB (tdbb);


/* Fake RSE */

for_node->nod_arg [e_for_re] = PAR_make_node (tdbb, 1 + rse_delta + 2);
rse = (RSE) for_node->nod_arg [e_for_re];
rse->nod_type = nod_rse;
rse->nod_count = 0;
rse->rse_count = 1;
rse->rse_relation [0] = relation = parse (tdbb, csb, RELATION);

#ifdef GATEWAY
if (!(((REL) relation->nod_arg [e_rel_relation])->rel_flags & REL_dbkey))
    error (*csb, gds__no_dbkey, 0);
	    /* Msg365 dbkey not available for multi-table views */
#endif

/* Fake boolean */

node = rse->rse_boolean = PAR_make_node (tdbb, 2);
node->nod_type = nod_eql;
node->nod_flags = nod_comparison;
node->nod_arg [1] = parse (tdbb, csb, VALUE);
node->nod_arg [0] = PAR_make_node (tdbb, 1);
node = node->nod_arg [0];
node->nod_type = nod_dbkey;
node->nod_count = 0;
node->nod_arg [0] = relation->nod_arg [e_rel_stream];

/* Pick up statement */

for_node->nod_arg [e_for_statement] = parse (tdbb, csb, STATEMENT);

return for_node;
}

static NOD par_field (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT	operator)
{
/**************************************
 *
 *	p a r _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Parse a field.
 *
 **************************************/
REL	relation;
PRC	procedure;
TEXT	name[32];
NOD	node;
SSHORT	stream, id, context, flags;
struct csb_repeat	*tail;
PRC     scan_proc;
FLD     field;
REL	temp_rel;
BOOLEAN is_column = FALSE;

SET_TDBB (tdbb);


context = (unsigned int) BLR_BYTE;

if (context >= (*csb)->csb_count)
    error (*csb, gds__ctxnotdef, 0);

stream = (*csb)->csb_rpt [context].csb_stream;
flags = 0;

if (operator == blr_fid)
    {
    id = BLR_WORD;
    flags = nod_id;
    is_column = TRUE;
    }
else if (operator == blr_field)
    {
    tail = &(*csb)->csb_rpt [stream];
    procedure = tail->csb_procedure;

    /* make sure procedure has been scanned before using it */

    if ( procedure &&
         (!(procedure->prc_flags & PRC_scanned) || (procedure->prc_flags & PRC_being_scanned) || (procedure->prc_flags & PRC_being_altered)))
        {
        scan_proc = MET_procedure (tdbb, procedure->prc_id, 0);
        if ( scan_proc != procedure)
            procedure = NULL;
        }

    if (procedure)
        {
	par_name (csb, name);
	if ((id = find_proc_field (procedure, name)) == -1)
	    error (*csb, 
		gds__fldnotdef, 
		gds_arg_string, ERR_cstring (name), 
		gds_arg_string, procedure->prc_name->str_data, 
		0);
	}
    else
	{
        if ( !(relation = tail->csb_relation))
	    error (*csb, gds__ctxnotdef, 0);
        
        /* make sure relation has been scanned before using it */

        if (!(relation->rel_flags & REL_scanned) ||
             (relation->rel_flags & REL_being_scanned))
            MET_scan_relation (tdbb, relation);

	par_name (csb, name);
	if ((id = MET_lookup_field (tdbb, relation, name)) < 0) 
	    if ((*csb)->csb_g_flags & csb_validation)
		{
		id = 0;
		flags |= nod_id;
		is_column = TRUE;
		}
	    else
		{
		if (tdbb->tdbb_attachment->att_flags & ATT_gbak_attachment)
		    warning (*csb, 
			gds__fldnotdef, 
			gds_arg_string, ERR_cstring (name), 
			gds_arg_string, relation->rel_name, 
			0);
		else if (relation->rel_name)
		    error (*csb, 
			gds__fldnotdef, 
			gds_arg_string, ERR_cstring (name), 
			gds_arg_string, relation->rel_name, 
			0);
		else
		    error (*csb, gds__ctxnotdef, 0);
		}
	}
    }
    
/* check for dependencies -- if a field name was given,
   use it because when restoring the database the field
   id's may not be valid yet */

if ((*csb)->csb_g_flags & csb_get_dependencies)
    if (operator == blr_fid)
	par_dependency (tdbb, csb, stream, id, NULL_PTR);
    else
	par_dependency (tdbb, csb, stream, id, name);

node = PAR_gen_field (tdbb, stream, id);
node->nod_flags |= flags;

if (is_column == TRUE)
    {
    if (temp_rel = (*csb)->csb_rpt [stream].csb_relation)
	{
	if (field = (FLD) temp_rel->rel_fields->vec_object[id])
	    {
	    if (field->fld_default_value && field->fld_not_null)
		node->nod_arg [e_fld_default_value] = field->fld_default_value;
	    }
	}
    }

return node;
}

static NOD par_function (
    TDBB	tdbb,
    CSB		*csb)
{
/**************************************
 *
 *	p a r _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse a function reference.
 *
 **************************************/
NOD	node;
FUN	function, homonyms;
TEXT	name [32];
USHORT	count;

SET_TDBB (tdbb);


count = par_name (csb, name);


function = FUN_lookup_function (name);
if (!function)
    {
    if (tdbb->tdbb_flags & TDBB_prc_being_dropped)
	{
        node = PAR_make_node (tdbb, e_fun_length);
        node->nod_count = 1;
        node->nod_arg [e_fun_function] = (NOD) NULL;
        node->nod_arg [e_fun_args] = par_args (tdbb, csb, VALUE);
        return node;
	}
    else
        {
        (*csb)->csb_running -= count;
        error (*csb, gds__funnotdef, 
	    gds_arg_string, ERR_cstring (name), 
	    0); 
        }
    }

for (homonyms = function; homonyms; homonyms = homonyms->fun_homonym)
    if (homonyms->fun_entrypoint)
	break;

if (!homonyms) 
    if (tdbb->tdbb_attachment->att_flags & ATT_gbak_attachment)
	warning (*csb, gds__funnotdef, 
	    gds_arg_string, ERR_cstring (name),
	    gds_arg_interpreted, "module name or entrypoint could not be found", 
	    0);
    else
	{
	(*csb)->csb_running -= count;
	error (*csb, gds__funnotdef, 
	    gds_arg_string, ERR_cstring (name),
	    gds_arg_interpreted, "module name or entrypoint could not be found", 
	    0);
	}

node = PAR_make_node (tdbb, e_fun_length);
node->nod_count = 1;
node->nod_arg [e_fun_function] = (NOD) function;
node->nod_arg [e_fun_args] = par_args (tdbb, csb, VALUE);

return node;
}

static NOD par_literal (
    TDBB	tdbb,
    register CSB	*csb)
{
/**************************************
 *
 *	p a r _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Parse a literal value.
 *
 **************************************/
register LIT	literal;
NOD		node;
DSC		desc;
UCHAR		*p, *q;
SSHORT		count, l, scale;
UCHAR		dtype;

SET_TDBB (tdbb);


PAR_desc (csb, &desc);
count = lit_delta + (desc.dsc_length + sizeof (int) - 1) / sizeof (int);
node = PAR_make_node (tdbb, count);
literal = (LIT) node;
node->nod_count = 0;
literal->lit_desc = desc;
literal->lit_desc.dsc_address = p = literal->lit_data;
literal->lit_desc.dsc_flags = 0;
q = (*csb)->csb_running;
l = desc.dsc_length;

switch (desc.dsc_dtype)
    {
    case dtype_short:
	l = 2;
	*(SSHORT*) p = (SSHORT) gds__vax_integer (q, l);
	break;

    case dtype_long:
    case dtype_sql_date:
    case dtype_sql_time:
	l = 4;
	*(SLONG*) p = (SLONG) gds__vax_integer (q, l);
	break;

    case dtype_timestamp:
	l = 8;
	*(SLONG*) p = (SLONG) gds__vax_integer (q, 4);
	p += 4;
	q += 4;
	*(SLONG*) p = (SLONG) gds__vax_integer (q, 4);
	break;

    case dtype_int64:
	l = sizeof (SINT64);
	*(SINT64*) p = (SINT64) isc_portable_integer(q, l);
	break;

    case dtype_double:
	/* the double literal could potentially be used for any
           numeric literal - the value is passed as if it were a
           text string. Convert the numeric string to its binary
           value (int64, long or double as appropriate). */
	l = BLR_WORD;
        q = (*csb)->csb_running;
	dtype = CVT_get_numeric (q, l, &scale, (double*) p, (FPTR_VOID) ERR_post);
	literal->lit_desc.dsc_dtype = dtype;
	if (dtype == dtype_double) 
	    literal->lit_desc.dsc_length = sizeof(double);
	else if (dtype == dtype_long) 
	    {
	    literal->lit_desc.dsc_length = sizeof(SLONG);
	    literal->lit_desc.dsc_scale = (SCHAR) scale;
	    }
	else
	    {
	    literal->lit_desc.dsc_length = sizeof(SINT64);
	    literal->lit_desc.dsc_scale = (SCHAR) scale;
	    }
	break;

    default:
	assert (FALSE);
    case dtype_text:
	if (count = l)
	    do *p++ = *q++; while (--count);
	break;
    }

(*csb)->csb_running += l;

return node;
}

static NOD par_map (
    TDBB	tdbb,
    CSB		*csb,
    USHORT	stream)
{
/**************************************
 *
 *	p a r _ m a p
 *
 **************************************
 *
 * Functional description
 *	Parse a MAP clause for a union or global aggregate expression.
 *
 **************************************/
NOD	assignment, node;
SSHORT	count;
LLS	map;

SET_TDBB (tdbb);


if (BLR_BYTE != blr_map)
    syntax_error (*csb, "blr_map");

count = BLR_WORD;
map = NULL;

while (--count >= 0)
    {
    assignment = PAR_make_node (tdbb, e_asgn_length);
    assignment->nod_type = nod_assignment;
    assignment->nod_count = e_asgn_length;
    assignment->nod_arg [e_asgn_to] = PAR_gen_field (tdbb, stream, BLR_WORD);
    assignment->nod_arg [e_asgn_from] = parse (tdbb, csb, VALUE);
    LLS_PUSH (assignment, &map);
    }

node = PAR_make_list (tdbb, map);
node->nod_type = nod_map;

return node;
}

static NOD par_message (
    TDBB	tdbb,
    CSB	*csb)
{
/**************************************
 *
 *	p a r _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Parse a message declaration, including operator byte.
 *
 **************************************/
NOD			node;
FMT			format;
DSC			*desc, *end;
USHORT			n, alignment;
ULONG			offset;
struct csb_repeat	*tail;

SET_TDBB (tdbb);

/* Get message number, register it in the compile scratch block, and
   allocate a node to represent the message */

n = (unsigned int) BLR_BYTE;
tail = CMP_csb_element (csb, n);
tail->csb_message = node = PAR_make_node (tdbb, e_msg_length);
node->nod_count = 0;
node->nod_arg [e_msg_number] = (NOD) (SLONG) n;
if (n > (*csb)->csb_msg_number)
    (*csb)->csb_msg_number = n;

/* Get the number of parameters in the message and prepare to fill
   out the format block */

n = BLR_WORD;
format = (FMT) ALLOCDV (type_fmt, n);
node->nod_arg [e_msg_format] = (NOD) format;
format->fmt_count = n;
offset = 0;

for (desc = format->fmt_desc, end = desc + n; desc < end; desc++)
    {
    alignment = PAR_desc (csb, desc);
    if (alignment)
	offset = ALIGN (offset, alignment);
    desc->dsc_address = (UCHAR*) (SLONG) offset;
    offset += desc->dsc_length;
    }

if (offset > MAX_FORMAT_SIZE)
    error (*csb, gds__imp_exc, gds_arg_gds, gds__blktoobig, 0);

format->fmt_length = (USHORT) offset;

return node;
}

static NOD par_modify (
    TDBB	tdbb,
    CSB			*csb)
{
/**************************************
 *
 *	p a r _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Parse a modify statement.
 *
 **************************************/
NOD			node;
SSHORT			context, new_stream, org_stream;
struct csb_repeat	*tail;

SET_TDBB (tdbb);

/* Parse the original and new contexts */

context = (unsigned int) BLR_BYTE;
if (context >= (*csb)->csb_count)
    error (*csb, gds__ctxnotdef, 0);
org_stream = (*csb)->csb_rpt [context].csb_stream;
new_stream = (*csb)->csb_n_stream++;
context = (unsigned int) BLR_BYTE;

/* Make sure the compiler scratch block is big enough to hold
   everything */

tail = CMP_csb_element (csb, context);
tail->csb_stream = (UCHAR) new_stream;

tail = CMP_csb_element (csb, new_stream);
tail->csb_relation = (*csb)->csb_rpt [org_stream].csb_relation;

/* Make the node and parse the sub-expression */

node = PAR_make_node (tdbb, e_mod_length);
node->nod_count = 1;
node->nod_arg [e_mod_org_stream] = (NOD) (SLONG) org_stream;
node->nod_arg [e_mod_new_stream] = (NOD) (SLONG) new_stream;
node->nod_arg [e_mod_statement] = parse (tdbb, csb, STATEMENT);

return node;
}

static USHORT par_name (
    CSB		*csb,
    TEXT	*string)
{
/**************************************
 *
 *	p a r _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Parse a counted string, returning count.
 *
 **************************************/
USHORT	count, l;

if (count = l = BLR_BYTE)
    do *string++ = BLR_BYTE; while (--l);

*string = 0;

return count;
}

static NOD par_plan (
    TDBB	tdbb,
    CSB		*csb)
{
/**************************************
 *
 *	p a r _ p l a n
 *
 **************************************
 *
 * Functional description
 *	Parse an access plan expression.  
 *	At this stage we are just generating the 
 *	parse tree and checking contexts 
 *	and indices.
 *
 **************************************/
NOD	plan, access_type, relation_node, *arg;
USHORT	node_type, count;

SET_TDBB (tdbb);

node_type = (USHORT) BLR_BYTE;
                
/* a join type indicates a cross of two or more streams */

if (node_type == blr_join ||
    node_type == blr_merge)
    {                        
    count = (USHORT) BLR_BYTE;
    plan = PAR_make_node (tdbb, count);
    plan->nod_type = (NOD_T) (USHORT) blr_table [node_type];

    for (arg = plan->nod_arg; count--;)
	*arg++ = par_plan (tdbb, csb);
    return plan;
    }

/* we have hit a stream; parse the context number and access type */

if (node_type == blr_retrieve)
    {
    TEXT	name [32], *p;
    SSHORT	stream, n;
    SLONG	index_id, relation_id;
    REL	relation;

    plan = PAR_make_node (tdbb, e_retrieve_length);
    plan->nod_type = (NOD_T) (USHORT) blr_table [node_type];
        
    /* parse the relation name and context--the relation 
       itself is redundant except in the case of a view,
       in which case the base relation (and alias) must be specified */

    n = BLR_BYTE;
    if (n != blr_relation && n != blr_relation2 &&
        n != blr_rid && n != blr_rid2)
        syntax_error (*csb, elements [RELATION]);
                                               
    /* don't have par_relation() parse the context, because
       this would add a new context; while this is a reference to 
       an existing context */

    relation_node = par_relation (tdbb, csb, n, FALSE);
    plan->nod_arg [e_retrieve_relation] = relation_node;
    relation = (REL) relation_node->nod_arg [e_rel_relation];

    n = BLR_BYTE;
    if (n >= (*csb)->csb_count)
        error (*csb, gds__ctxnotdef, 0);
    stream = (*csb)->csb_rpt [n].csb_stream;

    relation_node->nod_arg [e_rel_stream] = (NOD) (SLONG) stream;
    relation_node->nod_arg [e_rel_context] = (NOD) (SLONG) n;

    /* Access plan types (sequential is default) */

    node_type = (USHORT) BLR_BYTE;
    if (node_type == blr_navigational)
	{
	SSHORT idx_status;

        access_type = plan->nod_arg [e_retrieve_access_type] = PAR_make_node (tdbb, 3);
	access_type->nod_type = nod_navigational;

	/* pick up the index name and look up the appropriate ids */

	par_name (csb, name);
	for (p = name; *p; *p++)
	    *p = UPPER (*p);
	index_id = MET_lookup_index_name (tdbb, name, &relation_id, &idx_status);

        if (idx_status == MET_object_unknown ||
	    idx_status == MET_object_inactive) 
	    if (tdbb->tdbb_attachment->att_flags & ATT_gbak_attachment)
		warning (*csb, gds__indexname, 
		    gds_arg_string, ERR_cstring (name),
		    gds_arg_string, relation->rel_name,
		    0);  
	    else
		error (*csb, gds__indexname, 
		    gds_arg_string, ERR_cstring (name),
		    gds_arg_string, relation->rel_name,
		    0);  

 	/* save both the relation id and the index id, since 
	   the relation could be a base relation of a view;
           save the index name also, for convenience */

	access_type->nod_arg [0] = (NOD) relation_id;
	access_type->nod_arg [1] = (NOD) index_id;
	access_type->nod_arg [2] = (NOD) ALL_cstring (name);
	}
    else if (node_type == blr_indices)
	{
	SSHORT idx_status;

	count = (USHORT) BLR_BYTE;
        access_type = plan->nod_arg [e_retrieve_access_type] = PAR_make_node (tdbb, count * 3);
	access_type->nod_type = nod_indices;

	/* pick up the index names and look up the appropriate ids */

        for (arg = access_type->nod_arg; count--;)
	    {
  	    par_name (csb, name);
	    for (p = name; *p; *p++)
	        *p = UPPER (*p);

  	    index_id = MET_lookup_index_name (tdbb, name, &relation_id, &idx_status);

	    if (idx_status == MET_object_unknown ||
	        idx_status == MET_object_inactive) 
	        if (tdbb->tdbb_attachment->att_flags & ATT_gbak_attachment)
		    warning (*csb, gds__indexname, 
			gds_arg_string, ERR_cstring (name),
			gds_arg_string, relation->rel_name,
			0);
		else
		    error (*csb, gds__indexname, 
			gds_arg_string, ERR_cstring (name),
			gds_arg_string, relation->rel_name,
			0);

 	    /* save both the relation id and the index id, since 
	       the relation could be a base relation of a view;
               save the index name also, for convenience */

 	    *arg++ = (NOD) relation_id;
 	    *arg++ = (NOD) index_id;
	    *arg++ = (NOD) ALL_cstring (name);
	    }
	}
    else if (node_type != blr_sequential)
	syntax_error (*csb, "access type");
    
    return plan;
    }

syntax_error (*csb, "plan item");
return ((NOD) 0);	/* Added to remove compiler warning */
}

static NOD par_procedure (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT	operator)
{
/**************************************
 *
 *	p a r _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Parse an procedural view reference.
 *
 **************************************/
NOD	node;
PRC	procedure;
USHORT	stream;

SET_TDBB (tdbb);

{
TEXT	name [32];
SSHORT	pid;

if (operator == blr_procedure)
    {
    par_name (csb, name);
    procedure = MET_lookup_procedure (tdbb, name);
    }
else
    {
    pid = BLR_WORD;
    if (!(procedure = MET_lookup_procedure_id (tdbb, pid, FALSE, 0)))
	sprintf (name, "id %d", pid);
    }
if (!procedure)
    error (*csb,
	gds__prcnotdef, 
	gds_arg_string, ERR_cstring (name), 
	0);
}

node = PAR_make_node (tdbb, e_prc_length);
node->nod_type = nod_procedure;
node->nod_count = count_table [blr_procedure];
node->nod_arg [e_prc_procedure] = (NOD) procedure;

stream = par_context (csb, NULL_PTR);
node->nod_arg [e_prc_stream] = (NOD) (SLONG) stream;
(*csb)->csb_rpt [stream].csb_procedure = procedure;

par_procedure_parms (tdbb, csb, procedure, &node->nod_arg [e_prc_in_msg],
    &node->nod_arg [e_prc_inputs], TRUE);

if ((*csb)->csb_g_flags & csb_get_dependencies)
    par_dependency (tdbb, csb, stream, (SSHORT) -1, NULL_PTR);

return node;
}

static void par_procedure_parms (
    TDBB	tdbb,
    CSB		*csb,
    PRC		procedure,
    NOD		*message_ptr,
    NOD		*parameter_ptr,
    USHORT	input_flag)
{
/**************************************
 *
 *	p a r _ p r o c e d u r e _ p a r m s
 *
 **************************************
 *
 * Functional description
 *	Parse some procedure parameters.
 *
 **************************************/
NOD	message, list, *ptr, asgn, prm, prm_f;
FMT	format;
USHORT	count, n, i, asgn_arg1, asgn_arg2;
struct csb_repeat	*tail;
BOOLEAN	mismatch = FALSE;

SET_TDBB (tdbb);

count = BLR_WORD ;

/** Check to see if the parameter count matches **/
if (count != (input_flag ? procedure->prc_inputs : procedure->prc_outputs))
    {
    /** They don't match...Hmmm...Its OK if we were dropping the procedure **/
    if (!(tdbb->tdbb_flags & TDBB_prc_being_dropped))
        error (*csb,
          gds__prcmismat,
          gds_arg_string, ERR_cstring (procedure->prc_name->str_data),
	  0);
    else
	mismatch = TRUE;
    }

if (count)
    {
    /** We have a few parameters. Get on with creating the message block **/
    if ((n = ++(*csb)->csb_msg_number) < 2)
	(*csb)->csb_msg_number = n = 2;
    tail = CMP_csb_element (csb, n);
    tail->csb_message = message = PAR_make_node (tdbb, e_msg_length);
    message->nod_type = nod_message;
    message->nod_count = count_table [blr_message];
    *message_ptr = message;
    message->nod_count = 0;
    message->nod_arg [e_msg_number] = (NOD) (SLONG) n;
    format = input_flag ? procedure->prc_input_fmt : procedure->prc_output_fmt;
    message->nod_arg [e_msg_format] = (NOD) format;
    if (!mismatch)
        n = format->fmt_count / 2;
    else 
	/*  There was a parameter mismatch hence can't depend upon the format's
	    fmt_count. Use count instead.
	*/
	n = count;
    list = *parameter_ptr = PAR_make_node (tdbb, n);
    list->nod_type = nod_list;
    list->nod_count = n;
    ptr = list->nod_arg;
    if (input_flag)
	{
	asgn_arg1 = e_asgn_from;
	asgn_arg2 = e_asgn_to;
	}
    else
	{
	asgn_arg1 = e_asgn_to;
	asgn_arg2 = e_asgn_from;
	}
    for (i = 0; count; count--)
	{
	asgn = PAR_make_node (tdbb, e_asgn_length);
	*ptr++ = asgn;
	asgn->nod_type = nod_assignment;
	asgn->nod_count = count_table [blr_assignment];
	asgn->nod_arg [asgn_arg1] = parse (tdbb, csb, VALUE);
	prm = asgn->nod_arg [asgn_arg2] = PAR_make_node (tdbb, e_arg_length);
	prm->nod_type = nod_argument;
	prm->nod_count = 1;
	prm->nod_arg [e_arg_message] = message;
	prm->nod_arg [e_arg_number] = (NOD) i++;
	prm_f = prm->nod_arg [e_arg_flag] = PAR_make_node (tdbb, e_arg_length);
	prm_f->nod_type = nod_argument;
	prm_f->nod_count = 0;
	prm_f->nod_arg [e_arg_message] = message;
	prm_f->nod_arg [e_arg_number] = (NOD) i++;
	}
    }
else if ((input_flag ? procedure->prc_inputs : procedure->prc_outputs) && 
	!mismatch)
    error (*csb,
	gds__prcmismat,
	gds_arg_string, ERR_cstring (procedure->prc_name->str_data),
	0);
}

static NOD par_relation (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT 	operator,
    BOOLEAN	parse_context)
{
/**************************************
 *
 *	p a r _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse a relation reference.
 *
 **************************************/
#ifdef GATEWAY
DBB	dbb;
#endif
NOD	node;
REL	relation;
TEXT	name[32];
STR	alias_string = NULL;
SSHORT	id, stream, length, context;
        
SET_TDBB (tdbb);
#ifdef GATEWAY
dbb = tdbb->tdbb_database;
#endif

/* Make a relation reference node */

node = PAR_make_node (tdbb, e_rel_length);
node->nod_count = 0;

/* Find relation either by id or by name */

if (operator == blr_rid || operator == blr_rid2)
    {
    id = BLR_WORD;
    if (operator == blr_rid2)
	{
	length = BLR_PEEK;
	alias_string = (STR) ALLOCDV (type_str, length + 1);
	alias_string->str_length = length;
	par_name (csb, alias_string->str_data);
	}
#ifndef GATEWAY
    if (!(relation = MET_lookup_relation_id (tdbb, id, FALSE)))
	{
	sprintf (name, "id %d", id);
	error (*csb, gds__relnotdef, gds_arg_string, ERR_cstring (name), 0);
	}
#else
    /* To use rid, the relation slot must already have been filled in */

    if (id >= dbb->dbb_relations->vec_count || !dbb->dbb_relations->vec_object [id])
	error (*csb, gds__ctxnotdef, 0);
    relation = MET_relation (tdbb, id);
#endif
    }
else if (operator == blr_relation || operator == blr_relation2)
    {
    par_name (csb, name);
    if (operator == blr_relation2)
	{
	length = BLR_PEEK;
	alias_string = (STR) ALLOCDV (type_str, length + 1);
	alias_string->str_length = length;
	par_name (csb, alias_string->str_data);
	}
    if (!(relation = MET_lookup_relation (tdbb, name)))
	error (*csb, gds__relnotdef, 
		gds_arg_string, ERR_cstring (name), 
		0);
    }

/* if an alias was passed, store with the relation */

if (alias_string)
    node->nod_arg [e_rel_alias] = (NOD) alias_string;

/* Scan the relation if it hasn't already been scanned for meta data */

if ((!(relation->rel_flags & REL_scanned) || (relation->rel_flags & REL_being_scanned)) &&
    ((relation->rel_flags & REL_force_scan) || !((*csb)->csb_g_flags & csb_internal)))
    {
    relation->rel_flags &= ~REL_force_scan;
    MET_scan_relation (tdbb, relation);
    }
else if (relation->rel_flags & REL_sys_triggers)
    MET_parse_sys_trigger (tdbb, relation);

/* generate a stream for the relation reference, 
   assuming it is a real reference */

if (parse_context)
    {
    stream = par_context (csb, &context);
    node->nod_arg [e_rel_stream] = (NOD) (SLONG) stream;
    node->nod_arg [e_rel_context] = (NOD) (SLONG) context;

    (*csb)->csb_rpt [stream].csb_relation = relation;
    (*csb)->csb_rpt [stream].csb_alias = alias_string;

    if ((*csb)->csb_g_flags & csb_get_dependencies)
        par_dependency (tdbb, csb, stream, (SSHORT) -1, NULL_PTR);
    }

node->nod_arg [e_rel_relation] = (NOD) relation;

return node;
}

static NOD par_rse (
    TDBB	tdbb,
    CSB		*csb,
    SSHORT	rse_op)
{
/**************************************
 *
 *	p a r _ r s e
 *
 **************************************
 *
 * Functional description
 *	Parse a record selection expression.
 *
 **************************************/
register RSE	rse;
NOD		*ptr;
register SSHORT	count;
USHORT		jointype;
UCHAR		op;

SET_TDBB (tdbb);

count = (unsigned int) BLR_BYTE;
rse = (RSE) PAR_make_node (tdbb, count + rse_delta + 2);
rse->nod_count = 0;
rse->rse_count = count;
ptr = rse->rse_relation;

while (--count >= 0)
    *ptr++ = parse (tdbb, csb, RELATION);

while (TRUE)
    switch (op = BLR_BYTE)
	{
	case blr_boolean:
	    rse->rse_boolean = parse (tdbb, csb, BOOL);
	    break;

	case blr_first:
	    if (rse_op == blr_rs_stream)
		syntax_error (*csb, "rse stream clause");
	    rse->rse_first = parse (tdbb, csb, VALUE);
	    break;

	case blr_sort:
	    if (rse_op == blr_rs_stream)
		syntax_error (*csb, "rse stream clause");
	    rse->rse_sorted = par_sort (tdbb, csb, TRUE);
	    break;

	case blr_project:
	    if (rse_op == blr_rs_stream)
		syntax_error (*csb, "rse stream clause");
	    rse->rse_projection = par_sort (tdbb, csb, FALSE);
	    break;

	case blr_join_type:
	    if ((rse->rse_jointype = jointype = (USHORT) BLR_BYTE) != blr_inner &&
		jointype != blr_left &&
		jointype != blr_right &&
		jointype != blr_full)
		syntax_error (*csb, "join type clause");
	    break;

	case blr_plan:
	    rse->rse_plan = par_plan (tdbb, csb);
	    break;
	
#ifdef SCROLLABLE_CURSORS
	/* if a receive is seen here, then it is intended to be an asynchronous 
           receive which can happen at any time during the scope of the rse-- 
           this is intended to be a more efficient mechanism for scrolling through 
           a record stream, to prevent having to send a message to the engine 
	   for each record */

	case blr_receive:
	    BLR_PUSH;
	    rse->rse_async_message = parse (tdbb, csb, STATEMENT);
	    break;
#endif

	default:
	    if (op == (UCHAR) blr_end)
		{
		/* An outer join is only allowed when the stream count is 2
		   and a boolean expression has been supplied */

		if (!rse->rse_jointype ||
		    (rse->rse_count == 2 && rse->rse_boolean))
		    return (NOD) rse;
		}
	    syntax_error (*csb, (rse_op == blr_rs_stream) ?
		"rse stream clause" : "record selection expression clause");
	}
}

static NOD par_sort (
    TDBB	tdbb,
    CSB		*csb,
    BOOLEAN	flag)
{
/**************************************
 *
 *	p a r _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Parse a sort clause (sans header byte).  This is used for
 *	BLR_SORT, BLR_PROJECT, and BLR_GROUP.
 *
 **************************************/
NOD	clause, *ptr, *ptr2;
SSHORT	count;

SET_TDBB (tdbb);

count = (unsigned int) BLR_BYTE;
clause = PAR_make_node (tdbb, count * 2);
clause->nod_type = nod_sort;
clause->nod_count = count;
ptr = clause->nod_arg;
ptr2 = ptr + count;

while (--count >= 0)
    {
    if (flag)
	*ptr2++ = (NOD) (SLONG) ((BLR_BYTE == blr_descending) ? TRUE : FALSE);
    *ptr++ = parse (tdbb, csb, VALUE);
    }

return clause;
}

static NOD par_stream (
    TDBB	tdbb,
    CSB	*csb)
{
/**************************************
 *
 *	p a r _ s t r e a m
 *
 **************************************
 *
 * Functional description
 *	Parse a stream expression.
 *
 **************************************/
register RSE	rse;
UCHAR		op;

SET_TDBB (tdbb);

rse = (RSE) PAR_make_node (tdbb, 1 + rse_delta + 2);
rse->nod_count = 0;
rse->rse_count = 1;
rse->rse_relation [0] = parse (tdbb, csb, RELATION);

while (TRUE)
    switch (op = BLR_BYTE)
	{
	case blr_boolean:
	    rse->rse_boolean = parse (tdbb, csb, BOOL);
	    break;

	default:
	    if (op == (UCHAR) blr_end)
  		return (NOD) rse;
	    syntax_error (*csb, "stream_clause");
	}
}

static NOD par_union (
    TDBB	tdbb,
    CSB		*csb)
{
/**************************************
 *
 *	p a r _ u n i o n
 *
 **************************************
 *
 * Functional description
 *	Parse a union reference.
 *
 **************************************/
NOD	node;
SSHORT	count;
USHORT	stream;
LLS	clauses;

SET_TDBB (tdbb);

/* Make the node, parse the context number, get a stream assigned,
   and get the number of sub-rse's. */

node = PAR_make_node (tdbb, e_uni_length);
node->nod_count = 2;
stream = par_context (csb, NULL_PTR);
node->nod_arg [e_uni_stream] = (NOD) (SLONG) stream;
count = (unsigned int) BLR_BYTE;

/* Pick up the sub-rse's and maps */

clauses = NULL;

while (--count >= 0)
    {
    LLS_PUSH (parse (tdbb, csb, TYPE_RSE), &clauses);
    LLS_PUSH (par_map (tdbb, csb, stream), &clauses);
    }

node->nod_arg [e_uni_clauses] = PAR_make_list (tdbb, clauses);

return node;
}

static USHORT par_word (
    CSB	*csb)
{
/**************************************
 *
 *	p a r _ w o r d
 *
 **************************************
 *
 * Functional description
 *	Pick up a BLR word.
 *
 **************************************/
UCHAR	low, high;

low = BLR_BYTE;
high = BLR_BYTE;

return high * 256 + low;
}

static NOD parse (
    TDBB		tdbb,
    register CSB	*csb,
    USHORT		expected)
{
/**************************************
 *
 *	p a r s e
 *
 **************************************
 *
 * Functional description
 *	Parse a BLR expression.
 *
 **************************************/
register NOD	node, *arg;
SSHORT		sub_type, operator;
USHORT		n;

SET_TDBB (tdbb);

operator = BLR_BYTE;

if (!(operator >= 0 && operator < sizeof(type_table)/sizeof(type_table[0])))
    syntax_error (*csb, "Invalid BLR code");

sub_type = sub_type_table [operator];

if (expected && expected != type_table [operator])
    syntax_error (*csb, elements [expected]);

/* If there is a length given in the length table, pre-allocate
   the node and set its count.  This saves an enormous amount of
   repetitive code. */

if (n = length_table [operator])
    {
    node = PAR_make_node (tdbb, n);
    node->nod_count = count_table [operator];
    arg = node->nod_arg;
    }    
else
    {
    node = NULL;
    arg = NULL;
    };

/* Dispatch on operator type. */

switch (operator)
    {
    case blr_any:
    case blr_unique:
    case blr_ansi_any:
    case blr_ansi_all:
    case blr_exists:
	node->nod_arg [e_any_rse] = parse (tdbb, csb, sub_type);
	break;

    /* Boring operators -- no special handling req'd */

    case blr_value_if:
    case blr_substring:
    case blr_matching2:
    case blr_ansi_like:
	*arg++ = parse (tdbb, csb, sub_type);
	*arg++ = parse (tdbb, csb, sub_type);
	*arg++ = parse (tdbb, csb, sub_type);
	break;

    case blr_and:
    case blr_or:

    case blr_prot_mask:
    case blr_containing:
    case blr_matching:
    case blr_like:
    case blr_starting:
    case blr_add:
    case blr_subtract:
    case blr_multiply:
    case blr_divide:
    case blr_add2:
    case blr_subtract2:
    case blr_multiply2:
    case blr_divide2:
    case blr_concatenate:
    
    case blr_assignment:
	*arg++ = parse (tdbb, csb, sub_type);
	/* Fall into ... */

    case blr_handler:
    case blr_loop:

#ifndef GATEWAY
    case blr_lock_state:
#endif
    case blr_upcase:
    case blr_negate:
    case blr_not:
    case blr_missing:
    case blr_agg_count2:
    case blr_agg_max:
    case blr_agg_min:
    case blr_agg_total:
    case blr_agg_average:
    case blr_agg_count_distinct:
    case blr_agg_total_distinct:
    case blr_agg_average_distinct:
    case blr_agg_total2:
    case blr_agg_average2:
    case blr_agg_total_distinct2:
    case blr_agg_average_distinct2:
    case blr_post:
	*arg++ = parse (tdbb, csb, sub_type);
	break;

    case blr_null:
    case blr_agg_count:
    case blr_user_name:
    case blr_current_date:
    case blr_current_time:
    case blr_current_timestamp:
    case blr_start_savepoint:
    case blr_end_savepoint:
	break;

    case blr_store:
    case blr_store2:
	node->nod_arg [e_sto_relation] = parse (tdbb, csb, RELATION);
	node->nod_arg [e_sto_statement] = parse (tdbb, csb, sub_type);
	if (operator == blr_store2)
	    node->nod_arg [e_sto_statement2] = parse (tdbb, csb, sub_type);
	break;

    /* Comparison operators */

    case blr_between:
	*arg++ = parse (tdbb, csb, sub_type);

    case blr_eql:
    case blr_neq:
    case blr_geq:
    case blr_gtr:
    case blr_leq:
    case blr_lss:
	*arg++ = parse (tdbb, csb, sub_type);
	*arg++ = parse (tdbb, csb, sub_type);
	node->nod_flags = nod_comparison;
	break;

    case blr_erase:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_erase_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;

    case blr_modify:
	node = par_modify (tdbb, csb);
	break;

    case blr_exec_proc:
    case blr_exec_pid:
	node = par_exec_proc (tdbb, csb, operator);
	break;

    case blr_pid:
    case blr_procedure:
	node = par_procedure (tdbb, csb, operator);
	break;

    case blr_function:
	node = par_function (tdbb, csb);
	break;

    case blr_index:
#ifndef GATEWAY
	node->nod_arg [0] = parse (tdbb, csb, sub_type);
	node->nod_arg [1] = par_args (tdbb, csb, sub_type);
#else
	error (*csb, gds__wish_list, 
		gds_arg_interpreted, "blr_index not supported", 
		0);
#endif
	break;
	
    case blr_for:
	if (BLR_PEEK == (UCHAR) blr_stall)
	    node->nod_arg [e_for_stall] = parse (tdbb, csb, STATEMENT);

	if (BLR_PEEK == (UCHAR) blr_rse ||
	    BLR_PEEK == (UCHAR) blr_singular ||
	    BLR_PEEK == (UCHAR) blr_stream)
	    node->nod_arg [e_for_re] = parse (tdbb, csb, TYPE_RSE);
	else 
	    node->nod_arg [e_for_re] = par_rse (tdbb, csb, operator);
	node->nod_arg [e_for_statement] = parse (tdbb, csb, sub_type);
	break;

    case blr_rse:  
    case blr_rs_stream:  
	node = par_rse (tdbb, csb, operator);
	break;

    case blr_singular:  
	node = parse (tdbb, csb, TYPE_RSE);
	((RSE) node)->nod_flags |= rse_singular;
	break;

    case blr_relation:
    case blr_rid:
    case blr_relation2:
    case blr_rid2:
	node = par_relation (tdbb, csb, operator, TRUE);
	break;

    case blr_union:
	node = par_union (tdbb, csb);
	break;

    case blr_aggregate:
	node->nod_arg [e_agg_stream] = (NOD) (SLONG) par_context (csb, NULL_PTR);
	node->nod_arg [e_agg_rse] = parse (tdbb, csb, TYPE_RSE);
	node->nod_arg [e_agg_group] = parse (tdbb, csb, OTHER);
	node->nod_arg [e_agg_map] = par_map (tdbb, csb, (USHORT) node->nod_arg [e_agg_stream]);
	break;

    case blr_group_by:
	node = par_sort (tdbb, csb, FALSE);
	return (node->nod_count) ? node : NULL;

    case blr_field:
    case blr_fid :
	node = par_field (tdbb, csb, operator);
	break;

#ifndef GATEWAY
    case blr_gen_id:
    case blr_set_generator:
    case blr_gen_id2:
    case blr_set_generator2:
	{
	SLONG	tmp;
	TEXT	name [32];

	par_name (csb, name);
	tmp = MET_lookup_generator (tdbb, name);
	if (tmp < 0)
	   error (*csb, gds__gennotdef, 
		gds_arg_string, ERR_cstring (name), 
		0);
	node->nod_arg [e_gen_relation] = (NOD) tmp;
	node->nod_arg [e_gen_value] = parse (tdbb, csb, VALUE);	
	}
	break;
#else
    case blr_gen_id:
	error (*csb, gds__wish_list, 
		gds_arg_interpreted, "blr_gen_id not supported", 
		0);
	break;
    case blr_set_generator:
	error (*csb, gds__wish_list, 
		gds_arg_interpreted, "blr_set_generator not supported", 
		0);
	break;
#endif

    case blr_record_version:
    case blr_dbkey:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [0] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;

    case blr_fetch:
	par_fetch (tdbb, csb, node);
	break;

    case blr_send:
    case blr_receive:
	n = BLR_BYTE;
	node->nod_arg [e_send_message] = (*csb)->csb_rpt [n].csb_message;
	node->nod_arg [e_send_statement] = parse (tdbb, csb, sub_type);
	break;

    case blr_message:
	node = par_message (tdbb, csb);
	break;

    case blr_literal:
	node = par_literal (tdbb, csb);
	break;

    case blr_cast:
	node = par_cast (tdbb, csb);
	break;

    case blr_extract:
	node->nod_arg [e_extract_part] = BLR_BYTE;
	node->nod_arg [e_extract_value] = parse (tdbb, csb, sub_type);
	node->nod_count = e_extract_count;
	break;

    case blr_dcl_variable:
	{
	VEC	vector;

	n = BLR_WORD;
	node->nod_arg [e_dcl_id] = (NOD) (SLONG) n;
	PAR_desc (csb, (DSC*) (node->nod_arg + e_dcl_desc));
	vector = ALL_vector (tdbb->tdbb_default, &(*csb)->csb_variables, n);
	vector->vec_object [n] = (BLK) node;
	}
	break;

    case blr_variable:
	{
	VEC	vector;

	n = BLR_WORD;
	node->nod_arg [e_var_id] = (NOD) (SLONG) n;
	if (!(vector = (*csb)->csb_variables) ||
	    n >= vector->vec_count ||
	    !(node->nod_arg [e_var_variable] = (NOD) vector->vec_object [n]))
	    syntax_error (*csb, "variable identifier");
	}
	break;

    case blr_parameter:
    case blr_parameter2:
    case blr_parameter3:
	{
	NOD	message, temp;
	FMT	format;

	n = (USHORT) BLR_BYTE;
	if (n >= (*csb)->csb_count ||
	    !(message = (*csb)->csb_rpt [n].csb_message))
	    error (*csb, gds__badmsgnum, 0);
	node->nod_arg [e_arg_message] = message;
	n = BLR_WORD;
	node->nod_arg [e_arg_number] = (NOD) (SLONG) n;
	format = (FMT) message->nod_arg [e_msg_format];
	if (n >= format->fmt_count)
	    error (*csb, gds__badparnum, 0);
	if (operator != blr_parameter)
	    {
	    node->nod_arg [e_arg_flag] = temp = PAR_make_node (tdbb, e_arg_length);
	    node->nod_count = 1;
	    temp->nod_count = 0;
	    temp->nod_type = nod_argument;
	    temp->nod_arg [e_arg_message] = message;
	    n = BLR_WORD;
	    temp->nod_arg [e_arg_number] = (NOD) (SLONG) n;
	    if (n >= format->fmt_count)
		error (*csb, gds__badparnum, 0);
	    }
	if (operator == blr_parameter3)
	    {
	    node->nod_arg [e_arg_indicator] = temp = PAR_make_node (tdbb, e_arg_length);
	    node->nod_count = 2;
	    temp->nod_count = 0;
	    temp->nod_type = nod_argument;
	    temp->nod_arg [e_arg_message] = message;
	    n = BLR_WORD;
	    temp->nod_arg [e_arg_number] = (NOD) (SLONG) n;
	    if (n >= format->fmt_count)
		error (*csb, gds__badparnum, 0);
	    }
	}
	break;

    case blr_stall:
	break;

    case blr_select:
    case blr_begin:
	{
	LLS	stack = NULL;

	while (BLR_PEEK != (UCHAR) blr_end)
	    {
	    if (operator == blr_select && BLR_PEEK != blr_receive)
		syntax_error (*csb, "blr_receive");
	    LLS_PUSH (parse (tdbb, csb, sub_type), &stack);
	    }
	(void) BLR_BYTE;
	node = PAR_make_list (tdbb, stack);
	}
	break;

    case blr_block:
	{
	LLS	stack = NULL;

	node->nod_arg [e_blk_action] = parse (tdbb, csb, sub_type);
	while (BLR_PEEK != (UCHAR) blr_end)
	    LLS_PUSH (parse (tdbb, csb, sub_type), &stack);
	(void) BLR_BYTE;
	node->nod_arg [e_blk_handlers] = PAR_make_list (tdbb, stack);
	}
	break;

    case blr_error_handler:
	node->nod_arg [e_err_conditions] = (NOD) par_conditions (tdbb, csb);
	node->nod_arg [e_err_action] = parse (tdbb, csb, sub_type);
	break;

    case blr_abort:
	node->nod_arg [0] = (NOD) par_condition (tdbb, csb);
	break;

    case blr_if:
	node->nod_arg [e_if_boolean] = parse (tdbb, csb, BOOL);
	node->nod_arg [e_if_true] = parse (tdbb, csb, sub_type);
	if (BLR_PEEK == (UCHAR) blr_end)
	    {
	    node->nod_count = 2;
    	(void) BLR_BYTE;
	    break;
	    }
	node->nod_arg [e_if_false] = parse (tdbb, csb, sub_type);
	break;

    case blr_label:
	node->nod_arg [e_lbl_label] = (NOD) (SLONG) BLR_BYTE;
	node->nod_arg [e_lbl_statement] = parse (tdbb, csb, sub_type);
	break;

    case blr_leave:
	node->nod_arg [0] = (NOD) (SLONG) BLR_BYTE;
	break;

#ifdef GATEWAY
    case blr_lock_state:
	error (*csb, gds__wish_list, 
		gds_arg_interpreted, "blr_lock_state not supported", 
		0);
	break;
#endif

    case blr_maximum:
    case blr_minimum:
    case blr_count:
/* count2
    case blr_count2:
*/
    case blr_average:
    case blr_average2:
    case blr_total:
    case blr_from:
    case blr_via:
 	if (BLR_PEEK == (UCHAR) blr_stream)
 	    node->nod_arg [e_stat_rse] = parse (tdbb, csb, OTHER);
 	else
 	    node->nod_arg [e_stat_rse] = parse (tdbb, csb, TYPE_RSE);
	if (operator != blr_count)
	    node->nod_arg [e_stat_value] = parse (tdbb, csb, VALUE);
	if (operator == blr_via)
	    node->nod_arg [e_stat_default] = parse (tdbb, csb, VALUE);
	break;

    /* Client/Server Express Features */

    case blr_stream:
#ifdef PROD_BUILD
        error (*csb, isc_cse_not_supported, 0);
#endif
	node = par_stream (tdbb, csb);
	break;

#ifdef PC_ENGINE
    case blr_set_index:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_index_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	node->nod_arg [e_index_index] = parse (tdbb, csb, VALUE);
	break;

    case blr_find:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_find_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	node->nod_arg [e_find_operator] = parse (tdbb, csb, VALUE);
	node->nod_arg [e_find_direction] = parse (tdbb, csb, VALUE);
	node->nod_arg [e_find_args] = par_args (tdbb, csb, VALUE);
	break;

    case blr_find_dbkey:
    case blr_find_dbkey_version:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_find_dbkey_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	node->nod_arg [e_find_dbkey_dbkey] = parse (tdbb, csb, VALUE);

	if (operator == blr_find_dbkey_version)
	    node->nod_arg [e_find_dbkey_version] = parse (tdbb, csb, VALUE);
	break;

    case blr_get_bookmark:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_getmark_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;

    case blr_set_bookmark:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_setmark_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	node->nod_arg [e_setmark_id] = parse (tdbb, csb, VALUE);
	break;

    case blr_release_bookmark:
	node->nod_arg [e_relmark_id] = parse (tdbb, csb, VALUE);
	break;

    case blr_bookmark:
	node->nod_arg [e_bookmark_id] = parse (tdbb, csb, VALUE);
	break;

    case blr_force_crack:
    case blr_crack:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [0] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;

    case blr_reset_stream:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_reset_from_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;

	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_reset_to_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;

    case blr_release_lock:
	node->nod_arg [e_rellock_lock] = parse (tdbb, csb, VALUE);
	break;

    case blr_lock_relation:
	n = BLR_BYTE;
	if (n != blr_relation && n != blr_relation2 &&
	    n != blr_rid && n != blr_rid2)
	    syntax_error (*csb, elements [RELATION]);
	node->nod_arg [e_lockrel_relation] = par_relation (tdbb, csb, n, FALSE);
	node->nod_arg [e_lockrel_level] = parse (tdbb, csb, VALUE);
	break;

    case blr_lock_record:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_lockrec_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	node->nod_arg [e_lockrec_level] = parse (tdbb, csb, VALUE);
	break;

    case blr_begin_range:
	node->nod_arg [e_brange_number] = parse (tdbb, csb, VALUE);
	break;

    case blr_end_range:
	node->nod_arg [e_erange_number] = parse (tdbb, csb, VALUE);
	break;

    case blr_delete_range:
	node->nod_arg [e_drange_number] = parse (tdbb, csb, VALUE);
	break;

    case blr_range_relation:
	node->nod_arg [e_range_relation_number] = parse (tdbb, csb, VALUE);
	n = BLR_BYTE;
	if (n != blr_relation && n != blr_relation2 &&
	    n != blr_rid && n != blr_rid2)
	    syntax_error (*csb, elements [RELATION]);
	node->nod_arg [e_range_relation_relation] = par_relation (tdbb, csb, n, FALSE);
	break;

    case blr_release_locks:
    case blr_delete_ranges:
	node = PAR_make_node (tdbb, 0);
	node->nod_count = 0;
	break;

    case blr_cardinality:
	n = BLR_BYTE;
	if (n >= (*csb)->csb_count)
	    error (*csb, gds__ctxnotdef, 0);
	node->nod_arg [e_card_stream] = (NOD) (SLONG) (*csb)->csb_rpt [n].csb_stream;
	break;
#endif

#ifdef SCROLLABLE_CURSORS
    case blr_seek:
    case blr_seek_no_warn:
	node->nod_arg [e_seek_direction] = parse (tdbb, csb, VALUE);
	node->nod_arg [e_seek_offset] = parse (tdbb, csb, VALUE);
	break;
#endif

    default:
	syntax_error (*csb, elements [expected]);
    }

if ((*csb)->csb_g_flags & csb_blr_version4)
    node->nod_type = (NOD_T) (USHORT) blr_table4 [(int) operator];
else
    node->nod_type = (NOD_T) (USHORT) blr_table [(int) operator];

return node;
}

static void syntax_error (
    CSB		csb,
    CONST TEXT	*string)
{
/**************************************
 *
 *	s y n t a x _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Post a syntax error message.
 *
 **************************************/

error (csb, gds__syntaxerr, 
	gds_arg_string, string, 
	gds_arg_number, (SLONG) (csb->csb_running - csb->csb_blr - 1),
	gds_arg_number, (SLONG) csb->csb_running [-1], 
	0);
}

static void warning (
    CSB		csb,
    ...)
{
/**************************************
 *
 *	w a r n i n g
 *
 **************************************
 *
 * Functional description
 *      This is for GBAK so that we can pass warning messages
 *      back to the client.  DO NOT USE this function until we
 *      fully implement warning at the engine level.
 *
 *	We will use the status vector like a warning vector.  What
 *	we are going to do is leave the [1] position of the vector 
 *	as 0 so that this will not be treated as an error, and we
 *	will place our warning message in the consecutive positions.
 *	It will be up to the caller to check these positions for
 *	the message.
 *
 **************************************/
TDBB	tdbb;
STATUS	*p;
int	type;
va_list	args;

tdbb = GET_THREAD_DATA;

VA_START (args, csb);

p = tdbb->tdbb_status_vector;

/* Make sure that the [1] position is 0
   indicating that no error has occured */ 

*p++ = gds_arg_gds;
*p++ = 0;

/* Now place your warning messages starting
   with position [2] */ 

*p++ = gds_arg_gds;
*p++ = va_arg (args, STATUS);

/* Pick up remaining args */

while (*p++ = type = va_arg (args, int))
    switch (type)
	{
	case gds_arg_gds:
	    *p++ = (STATUS) va_arg (args, STATUS);
	    break;

	case gds_arg_string:
	case gds_arg_interpreted:
	    *p++ = (STATUS) va_arg (args, TEXT*);
	    break;

	case gds_arg_cstring:
	    *p++ = (STATUS) va_arg (args, int);
	    *p++ = (STATUS) va_arg (args, TEXT*);
	    break;

	case gds_arg_number:
	    *p++ = (STATUS) va_arg (args, SLONG);
	    break;

	default:
	    assert (FALSE);
	case gds_arg_vms:
	case gds_arg_unix:
	case gds_arg_win32:
	case gds_arg_dos:
	case gds_arg_netware:
	case gds_arg_domain:
	case gds_arg_mpexl:
	    *p++ = va_arg (args, int);
	    break;
	}
}
