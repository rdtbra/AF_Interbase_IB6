/*
 *	PROGRAM:	ADA preprocesser
 *	MODULE:		ada.c
 *	DESCRIPTION:	Inserted text generator for ADA
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
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/gds.h"
#include "../gpre/gpre.h"
#include "../gpre/form.h"
#include "../gpre/pat.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/lang_proto.h"
#include "../gpre/pat_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"

static int	align (int);
static int	asgn_from (ACT, REF, int);
static int	asgn_sqlda_from (REF, int, TEXT *, int);
static int	asgn_to (ACT, REF, int);
static int	asgn_to_proc (REF, int);
static int	gen_any (ACT, int);
static int	gen_at_end (ACT, int);
static int	gen_based (ACT, int);
static int	gen_blob_close (ACT, USHORT);
static int	gen_blob_end (ACT, USHORT);
static int	gen_blob_for (ACT, USHORT);
static int	gen_blob_open (ACT, USHORT);
static int	gen_blr (int *, int, TEXT *);
static int	gen_clear_handles (ACT, int);
static int	gen_compatability_symbol (TEXT *, TEXT *, TEXT *);
static int	gen_compile (ACT, int);
static int	gen_create_database (ACT, int);
static int	gen_cursor_close (ACT, REQ, int);
static int	gen_cursor_init (ACT, int);
static int	gen_cursor_open (ACT, REQ, int);
static int	gen_database (ACT, int);
static int	gen_ddl (ACT, int);
static int	gen_drop_database (ACT, int);
static int	gen_dyn_close (ACT, int);
static int	gen_dyn_declare (ACT, int);
static int	gen_dyn_describe (ACT, int, BOOLEAN);
static int	gen_dyn_execute (ACT, int);
static int	gen_dyn_fetch (ACT, int);
static int	gen_dyn_immediate (ACT, int);
static int	gen_dyn_insert (ACT, int);
static int	gen_dyn_open (ACT, int);
static int	gen_dyn_prepare (ACT, int);
static int	gen_emodify (ACT, int);
static int	gen_estore (ACT, int);
static int	gen_endfor (ACT, int);
static int	gen_erase (ACT, int);
static SSHORT	gen_event_block (ACT);
static int	gen_event_init (ACT, int);
static int	gen_event_wait (ACT, int);
static int	gen_fetch (ACT, int);
static int	gen_finish (ACT, int);
static int	gen_for (ACT, int);
static int	gen_form_display (ACT, int);
static int	gen_form_end (ACT, int);
static int	gen_form_for (ACT, int);
static int	gen_function (ACT, int);
static int	gen_get_or_put_slice (ACT, REF, BOOLEAN, int);
static int	gen_get_segment (ACT, int);
static int	gen_item_end (ACT, int);
static int	gen_item_for (ACT, int);
static int	gen_loop (ACT, int);
static int	gen_menu (ACT, int);
static int	gen_menu_display (ACT, int);
static int	gen_menu_end (ACT, int);
static int	gen_menu_entree (ACT, int);
static int	gen_menu_entree_att (ACT, int);
static int	gen_menu_for (ACT, int);
static int	gen_menu_item_end (ACT, int);
static int	gen_menu_item_for (ACT, int);
static int	gen_menu_request (REQ, int);
static TEXT	*gen_name (TEXT *, REF, BOOLEAN);
static int	gen_on_error (ACT, USHORT);
static int	gen_procedure (ACT, int);
static int	gen_put_segment (ACT, int);
static int	gen_raw (UCHAR *, enum req_t, int, int);
static int	gen_ready (ACT, int);
static int	gen_receive (ACT, int, POR);
static int	gen_release (ACT, int);
static int	gen_request (REQ, int);
static int	gen_return_value (ACT, int);
static int	gen_routine (ACT, int);
static int	gen_s_end (ACT, int);
static int	gen_s_fetch (ACT, int);
static int	gen_s_start (ACT, int);
static int	gen_segment (ACT, int);
static int	gen_select (ACT, int);
static int	gen_send (ACT, POR, int);
static int	gen_slice (ACT, int);
static int	gen_start (ACT, POR, int);
static int	gen_store (ACT, int);
static int	gen_t_start (ACT, int);
static int	gen_tpb (TPB, int);
static int	gen_trans (ACT, int);
static int	gen_type (ACT, int);
static int	gen_update (ACT, int);
static int	gen_variable (ACT, int);
static int	gen_whenever (SWE, int);
static int	gen_window_create (ACT, int);
static int	gen_window_delete (ACT, int);
static int	gen_window_suspend (ACT, int);
static int	make_array_declaration (REF, int);
static int	make_cursor_open_test (enum act_t, REQ, int);
static TEXT	*make_name (TEXT *, SYM);
static int	make_ok_test (ACT, REQ, int);
static int	make_port (POR, int);
static int	make_ready (DBB, TEXT *, TEXT *, USHORT, REQ);
static int	printa (int, TEXT *, ...);
static int	printb (TEXT *, ...);
static TEXT	*request_trans (ACT, REQ);
static TEXT	*status_vector (ACT);
static int	t_start_auto (ACT, REQ, TEXT *, int, SSHORT);

static TEXT	output_buffer [512];
static int	first_flag;

#define COMMENT		"--- "
#define INDENT		3
#define ENDIF		printa (column, "end if;")
#define BEGIN		printa (column, "begin")
#define END		printa (column, "end;")

#define BYTE_DCL	"interbase.isc_byte"
#define BYTE_VECTOR_DCL	"interbase.isc_vector_byte"
#define SHORT_DCL	"interbase.isc_short"
#define USHORT_DCL	"interbase.isc_ushort"
#define LONG_DCL	"interbase.isc_long"
#define LONG_VECTOR_DCL	"interbase.isc_vector_long"
#define EVENT_LIST_DCL	"interbase.event_list"
#define FLOAT_DCL	"interbase.isc_float"
#define DOUBLE_DCL	"interbase.isc_double"

#define ADA_WINDOW_PACKAGE	(sw_window_scope == DBB_EXTERN) ? ada_package : ""

#define SET_SQLCODE     if (action->act_flags & ACT_sql) printa (column, "SQLCODE := interbase.sqlcode (isc_status);")

void ADA_action (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	A D A _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Code generator for ADA.  Not to be confused with
 *
 *
 **************************************/

switch (action->act_type) 
    {
    case ACT_alter_database:
    case ACT_alter_domain:
    case ACT_alter_table:
    case ACT_alter_index:
    case ACT_create_domain:
    case ACT_create_generator:
    case ACT_create_index:
    case ACT_create_shadow:
    case ACT_create_table:
    case ACT_create_view:
    case ACT_declare_filter:
    case ACT_declare_udf:
    case ACT_drop_domain:
    case ACT_drop_filter:
    case ACT_drop_index:
    case ACT_drop_shadow:
    case ACT_drop_table :
    case ACT_drop_udf:
    case ACT_statistics:
    case ACT_drop_view  : gen_ddl (action, column);			break;
    case ACT_any	: gen_any (action, column);			return;
    case ACT_at_end	: gen_at_end (action, column); 			return;
    case ACT_commit	: gen_trans (action, column); 			break;
    case ACT_commit_retain_context: gen_trans (action, column);		break;
    case ACT_b_declare	: gen_database (action, column); gen_routine (action, column);	return;
    case ACT_basedon	: gen_based (action, column);			return;
    case ACT_blob_cancel: gen_blob_close (action, column);		return;
    case ACT_blob_close	: gen_blob_close (action, column);		return;
    case ACT_blob_create: gen_blob_open (action, column); 		return;
    case ACT_blob_for	: gen_blob_for (action, column); 		return;
    case ACT_blob_handle: gen_segment (action, column); 		return;
    case ACT_blob_open	: gen_blob_open (action, column); 		return;
    case ACT_clear_handles : gen_clear_handles (action, column); 	break;
    case ACT_close	: gen_s_end (action, column); 			break;
    case ACT_create_database: gen_create_database (action, column);	break;
    case ACT_cursor	: gen_cursor_init (action, column); 		return;
    case ACT_database	: gen_database (action, column); 		return;
    case ACT_disconnect : gen_finish (action, column); 			break;
    case ACT_drop_database : gen_drop_database (action, column);	break;
    case ACT_dyn_close		: gen_dyn_close (action, column);	break;
    case ACT_dyn_cursor		: gen_dyn_declare (action, column);	break;
    case ACT_dyn_describe	: gen_dyn_describe (action, column, FALSE);	break;
    case ACT_dyn_describe_input	: gen_dyn_describe (action, column, TRUE);	break;
    case ACT_dyn_execute	: gen_dyn_execute (action, column);	break;
    case ACT_dyn_fetch		: gen_dyn_fetch (action, column);	break;
    case ACT_dyn_grant		: gen_ddl (action, column);		break;
    case ACT_dyn_immediate	: gen_dyn_immediate (action, column);	break;
    case ACT_dyn_insert		: gen_dyn_insert (action, column);	break;
    case ACT_dyn_open		: gen_dyn_open (action, column);	break;
    case ACT_dyn_prepare	: gen_dyn_prepare (action, column);	break;
    case ACT_procedure  	: gen_procedure (action, column);	break;
    case ACT_dyn_revoke 	: gen_ddl (action, column);     	break;
    case ACT_endblob	: gen_blob_end (action, column);		return;
    case ACT_enderror	: ENDIF; 					break;
    case ACT_endfor	: gen_endfor (action, column); 			break;
    case ACT_endmodify	: gen_emodify (action, column); 		break;
    case ACT_endstore	: gen_estore (action, column); 			break;
    case ACT_erase	: gen_erase (action, column); 			return;
    case ACT_event_init	: gen_event_init (action, column); 		break;
    case ACT_event_wait	: gen_event_wait (action, column); 		break;
    case ACT_fetch	: gen_fetch (action, column); 			break;
    case ACT_finish	: gen_finish (action, column); 			break;
    case ACT_for	: gen_for (action, column); 			return;
    case ACT_form_display : gen_form_display (action, column);		break;
    case ACT_form_end	: gen_form_end (action, column);		break;
    case ACT_form_for	: gen_form_for (action, column);		return;
    case ACT_function	: gen_function (action, column);		return;
    case ACT_get_segment: gen_get_segment (action, column);		return;
    case ACT_get_slice	: gen_slice (action, column);			return;
    case ACT_hctef	: ENDIF;					break;
    case ACT_insert	: gen_s_start (action, column); 		break;
    case ACT_item_for	:
    case ACT_item_put	: gen_item_for (action, column);		return;
    case ACT_item_end	: gen_item_end (action, column);		break;
    case ACT_loop	: gen_loop (action, column); 			break;
    case ACT_menu	: gen_menu (action, column);			return;
    case ACT_menu_display : gen_menu_display (action, column);		return;
    case ACT_menu_end	: gen_menu_end (action, column);		return;
    case ACT_menu_entree: gen_menu_entree (action, column);		return;
    case ACT_menu_for	: gen_menu_for (action, column);		return;

    case ACT_title_text:
    case ACT_title_length:
    case ACT_terminator:
    case ACT_entree_text:
    case ACT_entree_length:
    case ACT_entree_value:	gen_menu_entree_att (action, column);	return;

    case ACT_open	: gen_s_start (action, column); 		break;
    case ACT_on_error	: gen_on_error (action, column);		return;
    case ACT_ready	: gen_ready (action, column); 			break;
    case ACT_prepare	: gen_trans (action, column); 			break;
    case ACT_put_segment: gen_put_segment (action, column);		return;
    case ACT_put_slice	: gen_slice (action, column);			return;
    case ACT_release	: gen_release (action, column); 		break;
    case ACT_rfinish	: gen_finish (action, column); 			break;
    case ACT_rollback	: gen_trans (action, column); 			break;
    case ACT_routine	: gen_routine (action, column);			return;	
    case ACT_s_end	: gen_s_end (action, column);			return;
    case ACT_s_fetch	: gen_s_fetch (action, column);			return;
    case ACT_s_start	: gen_s_start (action, column); 		break;
    case ACT_select	: gen_select (action, column); 			break;
    case ACT_segment_length:  gen_segment (action, column);		return;
    case ACT_segment	: gen_segment (action, column);			return;
    case ACT_sql_dialect: sw_sql_dialect = ((SDT)action->act_object)->sdt_dialect; return;
    case ACT_start	: gen_t_start (action, column); 		break;
    case ACT_store	: gen_store (action, column);			return;
    case ACT_store2	: gen_return_value (action, column);		return;
    case ACT_type	: gen_type (action, column);			return;
    case ACT_update	: gen_update (action, column); 			break;
    case ACT_variable	: gen_variable (action, column);		return;
    case ACT_window_create	: gen_window_create (action,column);	return;
    case ACT_window_delete	: gen_window_delete (action,column);	return;
    case ACT_window_suspend	: gen_window_suspend (action,column);	return;
    default :
	return;
    }

/* Put in a trailing brace for those actions still with us */

if (action->act_flags & ACT_sql)
    gen_whenever (action->act_whenever, column);
}

void ADA_print_buffer (
    TEXT	*output_buffer,
    int		column)
{
/**************************************
 *
 *	A D A _ p r i n t _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Print a statment, breaking it into
 *      reasonable 120 character hunks.
 *
 **************************************/
TEXT	s [121], *p, *q;
int     i;
BOOLEAN multiline;

p = s;
multiline = FALSE;
for (q = output_buffer; *q; q++)
    {
    *p++ = *q;
    if (((p - s) + column) > 119)
	{
	for (p--; (*p != ',') && (*p != ' '); p--)
	    q--;
	*++p = 0;

	if (multiline)
	    {
	    for (i = column / 8; i; --i)
		ib_putc ('\t', out_file);

	    for (i = column % 8; i; --i)
		ib_putc (' ', out_file);
	    }
	ib_fprintf (out_file, "%s\n", s);
	s[0] = 0;
	p = s;
	multiline = TRUE;
	}
    } 
*p = 0;
if (multiline)
    {
    for (i = column / 8; i; --i)
	ib_putc ('\t', out_file);
    for (i = column % 8; i; --i)
	ib_putc (' ', out_file);
    }
ib_fprintf (out_file, "%s", s);
output_buffer[0] = 0;
}

static align (
    int	column)
{
/**************************************
 *
 *	a l i g n
 *
 **************************************
 *
 * Functional description
 *	Align output to a specific column for output.
 *
 **************************************/
int	i;

if (column < 0)
    return;

ib_putc ('\n', out_file);

for (i = column / 8; i; --i)
    ib_putc ('\t', out_file);

for (i = column % 8; i; --i)
    ib_putc (' ', out_file);
}

static asgn_from (
    ACT		action,
    REF		reference,
    int		column)
{
/**************************************
 *
 *	a s g n _ f r o m
 *
 **************************************
 *
 * Functional description
 *	Build an assignment from a host language variable to
 *	a port variable.
 *
 **************************************/
FLD     field;
TEXT	*value, name[64], variable [20], temp [20];

for (; reference; reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (field->fld_array_info)
	if (!(reference->ref_flags & REF_array_elem))
	    {
	    printa (column, "%s := interbase.null_blob;", gen_name (name, reference, TRUE));
	    gen_get_or_put_slice (action, reference, FALSE, column);
	    continue;
	    }

    if (!reference->ref_source && !reference->ref_value)
	continue;
    gen_name (variable, reference, TRUE);
    if (reference->ref_source)
	value = gen_name (temp, reference->ref_source, TRUE);
    else
	value = reference->ref_value;

    if (!reference->ref_master)
	printa (column, "%s := %s;", variable, value);
    else 
	{
	printa (column, "if %s < 0 then", value); 
	printa (column + INDENT,"%s := -1;", variable);
	printa (column, "else");
	printa (column + INDENT, "%s := 0;", variable);
    	ENDIF;
	} 
    }
}

#ifdef UNDEF
static void asgn_sqlda_from (
    REF         reference,
    int         number,
    SCHAR       *string,
    int         column)
{
/**************************************
 *
 *      a s g n _ s q l d a _ f r o m
 *
 **************************************
 *
 * Functional description
 *      Build an assignment from a host language variable to
 *      a sqlda variable.
 *
 **************************************/
SCHAR   *value, temp[20];

for (; reference; reference = reference->ref_next)
    {
    align (column);
    if (reference->ref_source)
        value = gen_name (temp, reference->ref_source, TRUE);
    else
        value = reference->ref_value;
    ib_fprintf (out_file, "isc_to_sqlda (isc_sqlda, %d, %s, sizeof(%s), %s);",
             number, value, value, string);
    }
}
#endif

static asgn_to (
    ACT		action,
    REF		reference,
    int		column)
{
/**************************************
 *
 *	a s g n _ t o
 *
 **************************************
 *
 * Functional description
 *	Build an assignment to a host language variable from
 *	a port variable.
 *
 **************************************/
FLD	field;
REF	source;
TEXT	s [20];

source = reference->ref_friend;
field = source->ref_field;

if (field->fld_array_info)
   {
   source->ref_value = reference->ref_value;
   gen_get_or_put_slice (action, source, TRUE, column);
   return;
   }
/*
if (field && (field->fld_flags & FLD_text))
    printa (column, "interbase.isc_ftof (%s, %d, %s, sizeof (%s));", 
	gen_name (s, source, TRUE),
	field->fld_length,
	reference->ref_value,
	reference->ref_value);
else
*/
    printa (column, "%s := %s;", reference->ref_value, gen_name (s, source, TRUE));

/* Pick up NULL value if one is there */
if (reference = reference->ref_null)
    printa (column, "%s := %s;", reference->ref_value, gen_name (s, reference, TRUE));
}

static asgn_to_proc (
    REF	reference,
    int column)
{
/**************************************
 *
 *	a s g n _ t o _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Build an assignment to a host language variable from
 *	a port variable.
 *
 **************************************/
SCHAR	s[64];

for (; reference; reference = reference->ref_next)
    {
    if (!reference->ref_value)
	continue;
    printa (column, "%s := %s;",
		reference->ref_value, gen_name (s, reference, TRUE));
    }
}

static int gen_any (
    ACT         action,
    int         column)
{
/**************************************
 *
 *      g e n _ a n y
 *
 **************************************
 *
 * Functional description
 *      Generate a function call for free standing ANY.  Somebody else
 *      will need to generate the actual function.
 *
 **************************************/
REQ     request;
POR     port;
REF     reference;

align (column);
request = action->act_request;

ib_fprintf (out_file, "%s_r (&%s, &%s",
    request->req_handle, request->req_handle, request->req_trans);

if (port = request->req_vport)
    for (reference = port->por_references; reference;
         reference = reference->ref_next)
        ib_fprintf (out_file, ", %s", reference->ref_value);

ib_fprintf (out_file, ")");
}

static int  gen_at_end (
    ACT         action,
    int         column)
{
/**************************************
 *
 *      g e n _ a t _ e n d
 *
 **************************************
 *
 * Functional description
 *      Generate code for AT END clause of FETCH.
 *
 **************************************/
REQ     request;
SCHAR   s[20];

request = action->act_request;
printa (column, "if %s = 0 then", gen_name (s, request->req_eof, TRUE));
}

static gen_based (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ b a s e d
 *
 **************************************
 *
 * Functional description
 *	Substitute for a BASED ON <field name> clause.
 *
 **************************************/
BAS	based_on;
FLD	field;
SSHORT   datatype, i, length;
DIM     dimension;
SCHAR    s[512], s2[128], *p, *q;

based_on  = (BAS) action->act_object;
field = based_on->bas_field;
q = s;

if (field->fld_array_info)
    {
    datatype = field->fld_array->fld_dtype;
    sprintf (s, "array (");
    for (q = s; *q; q++)
	;

    /*  Print out the dimension part of the declaration  */
    for (dimension = field->fld_array_info->ary_dimension, i = 1;
	    i < field->fld_array_info->ary_dimension_count;
		dimension = dimension->dim_next, i++)
	{
	sprintf (s2, "%s range %d..%d,\n                            ",
		 LONG_DCL,
		 dimension->dim_lower,
		 dimension->dim_upper);
	for (p = s2; *p; p++, q++)
	    *q = *p;
	}

    sprintf (s2, "%s range %d..%d) of ",
	     LONG_DCL,
	     dimension->dim_lower,
	     dimension->dim_upper);
    for (p = s2; *p; p++, q++)
	*q = *p;
    }
else
    datatype = field->fld_dtype;
   
switch (datatype) 
    {
    case dtype_short :
	sprintf (s2, "%s", SHORT_DCL);
	break;

    case dtype_long :
	sprintf (s2, "%s", LONG_DCL);
	break;

    case dtype_date :
    case dtype_blob :
    case dtype_quad :
	sprintf (s2, "interbase.quad");
	break;
	
    case dtype_text :
	length = (field->fld_array_info) ? field->fld_array->fld_length : field->fld_length;
	if (field->fld_sub_type == 1)
	    {
	    if (length == 1)
		sprintf (s2, "%s", BYTE_DCL);
	    else
		sprintf (s2, "%s (1..%d)", BYTE_VECTOR_DCL, length);
	    }
	else
	    if (length == 1)
		sprintf (s2, "interbase.isc_character");
	    else
		sprintf (s2, "string (1..%d)", length);
	break;

    case dtype_float:
	sprintf (s2, "%s", FLOAT_DCL);
	break;

    case dtype_double:
	sprintf (s2, "%s", DOUBLE_DCL);
	break;

    default :
	sprintf (s2, "datatype %d unknown\n",
	    field->fld_dtype);
	return;
    }
for (p = s2; *p; p++, q++)
    *q = *p;
if (!strcmp (based_on->bas_terminator, ";"))
    *q++ = ';';
*q = 0;
printa (column, s);
}

static gen_blob_close (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *	g e n _ b l o b _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Make a blob FOR loop.
 *
 **************************************/
TEXT	*command;
BLB	blob;

if (action->act_flags & ACT_sql)
    {
    column = gen_cursor_close (action, action->act_request, column);
    blob = (BLB) action->act_request->req_blobs;
    }
else
    blob = (BLB) action->act_object;

command = (action->act_type == ACT_blob_cancel) ? "CANCEL" : "CLOSE";
printa (column, "interbase.%s_BLOB (%s isc_%d);",
	command,
	status_vector (action),
	blob->blb_ident);

if (action->act_flags & ACT_sql)
    {
    ENDIF;
    column -= INDENT;
    }

SET_SQLCODE;
}
 
static gen_blob_end (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *	g e n _ b l o b _ e n d
 *
 **************************************
 *
 * Functional description
 *	End a blob FOR loop.
 *
 **************************************/
BLB	blob;

blob = (BLB) action->act_object;
printa (column, "end loop;");

if (action->act_error)
    printa (column, "interbase.CLOSE_BLOB (isc_status2, isc_%d);",
	blob->blb_ident);
else
    printa (column, "interbase.CLOSE_BLOB (%s isc_%d);",
	status_vector (NULL_PTR),
	blob->blb_ident);
}

static gen_blob_for (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *	g e n _ b l o b _ f o r
 *
 **************************************
 *
 * Functional description
 *	Make a blob FOR loop.
 *
 **************************************/

gen_blob_open (action, column);
if (action->act_error)
    printa (column, "if (isc_status(1) = 0) then");
printa (column, "loop");
gen_get_segment (action, column + INDENT);
printa (column + INDENT, 
	"exit when (isc_status(1) /= 0 and isc_status(1) /= interbase.isc_segment);");
}

#if 0	
/* This is the V4.0 version of the function prior to 18-January-95.
 * for unknown reasons it contains a core dump.  The V3.3 version of
 * this function appears to work fine, so we are using it instead.
 * Ravi Kumar
 */
static gen_blob_open (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *	g e n _ b l o b _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate the call to open (or create) a blob.
 *
 **************************************/
TEXT	*command;
BLB	blob;
PAT     args;
REF	reference;
TEXT	s [20];
TEXT	*pattern1 = "interbase.%IFCREATE%ELOPEN%EN_BLOB2 (%V1 %RF%DH, %RF%RT, %RF%BH, %RF%FR, %N1, %I1);",
	*pattern2 = "interbase.%IFCREATE%ELOPEN%EN_BLOB2 (%V1 %RF%DH, %RF%RT, %RF%BH, %RF%FR, 0, isc_null_bpb);";

if (action->act_flags & ACT_sql)
    {
    column = gen_cursor_open (action, action->act_request, column);
    blob = (BLB) action->act_request->req_blobs;
    reference = ((OPN) action->act_object)->opn_using;
    gen_name (s, reference, TRUE);
    }
else
    blob = (BLB) action->act_object;

args.pat_condition = action->act_type == ACT_blob_create; /*  open or create blob */
args.pat_vector1 = status_vector (action);                /*  status vector */
args.pat_database = blob->blb_request->req_database;      /*  database handle */
args.pat_request = blob->blb_request;                     /*  transaction handle */
args.pat_blob = blob;                                     /*  blob handle */
args.pat_reference = reference;                           /*  blob identifier */
args.pat_ident1 = blob->blb_bpb_ident;

if ((action->act_flags & ACT_sql) && action->act_type == ACT_blob_open)
    printa (column, "%s := %s;", s, reference->ref_value);

if (args.pat_value1 = blob->blb_bpb_length)
    PATTERN_expand (column, pattern1, &args);
else
    PATTERN_expand (column, pattern2, &args);

if (action->act_flags & ACT_sql)
    {
    ENDIF;
    column -= INDENT;
    ENDIF;
    column -= INDENT;
    SET_SQLCODE;
    if (action->act_type == ACT_blob_create)
	{
	printa (column, "if SQLCODE = 0 then");
	printa (column + INDENT, "%s := %s;", reference->ref_value, s);
	ENDIF;
	}
    ENDIF;
    }
}
#else
/* This is the version 3.3 of this routine - the original V4.0 version
 * has a core drop.
 * Ravi Kumar
 */
static gen_blob_open (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *      g e n _ b l o b _ o p e n
 *
 **************************************
 *
 * Functional description
 *      Generate the call to open (or create) a blob.
 *
 **************************************/
TEXT    *command;
BLB     blob;
REQ     request;
TEXT    s[20];
PAT     args;
TEXT    *pattern1 = "interbase.%IFCREATE%ELOPEN%EN_BLOB2 (%V1 %RF%DH, %RF%RT, %RF%BH, %RF%FR, %N1, %I1);";
TEXT    *pattern2 = "interbase.%IFCREATE%ELOPEN%EN_BLOB2 (%V1 %RF%DH, %RF%RT, %RF%BH, %RF%FR, 0, isc_null_bpb);";

blob = (BLB) action->act_object;
args.pat_condition = action->act_type == ACT_blob_create;    /*  open or create blob  */
args.pat_vector1 = status_vector (action);                   /*  status vector        */
args.pat_database = blob->blb_request->req_database;         /*  database handle      */
args.pat_request = blob->blb_request;                        /*  transaction handle   */
args.pat_blob = blob;                                        /*  blob handle          */
args.pat_reference = blob->blb_reference;                    /*  blob identifier      */
args.pat_ident1 = blob->blb_bpb_ident;

if (args.pat_value1 = blob->blb_bpb_length)
    PATTERN_expand (column, pattern1, &args);
else
    PATTERN_expand (column, pattern2, &args);
}
#endif

static gen_blr (
    int		*user_arg,
    int		offset,
    TEXT	*string)
{
/**************************************
 *
 *	g e n _ b l r
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/
int from, to, len, c_len;
SCHAR c;

c_len = strlen (COMMENT);
len = strlen (string);
from = 0;
to = 120 - c_len;

while (from < len)
    {
    if (to < len)
	{
	c = string[to];
	string[to] = 0;
	}
    ib_fprintf (out_file, "%s%s\n", COMMENT, &string[from]);
    if (to < len)
	string[to] = c;
    from = to;    
    to = to + 120 - c_len;
    }

return TRUE;
}

static int gen_clear_handles (
    ACT         action,
    int         column)
{
/**************************************
 *
 *      g e n _ c l e a r _ h a n d l e s
 *
 **************************************
 *
 * Functional description
 *      Zap all know handles.
 *
 **************************************/
REQ     request;

for (request = requests; request; request = request->req_next)
    {
    if (!(request->req_flags & REQ_exp_hand))
        printa (column, "%s = 0;", request->req_handle);
    if (request->req_form_handle &&
        !(request->req_flags & REQ_exp_form_handle))
        printa (column, "%s = 0;", request->req_form_handle);
    }
}

#if 0
static void gen_compatibility_symbol (
    TEXT        *symbol,
    TEXT        *v4_prefix,
    TEXT        *trailer)
{
/**************************************
 *
 *      g e n _ c o m p a t i b i l i t y _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *      Generate a symbol to ease compatibility with V3.
 *
 **************************************/
TEXT    *v3_prefix;

v3_prefix = (sw_language == lan_cxx) ? "gds_" : "gds__";

ib_fprintf (out_file, "#define %s%s\t%s%s%s\n", v3_prefix, symbol, v4_prefix,
    symbol, trailer);
}
#endif

static gen_compile (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *	Generate text to compile a request.
 *
 **************************************/
REQ	request;
DBB	db;
SYM	symbol;
TEXT	*filename;
BLB	blob;

request = action->act_request;
column += INDENT;
db = request->req_database;
symbol = db->dbb_name;

if (sw_auto)
    t_start_auto (action, request, status_vector (action), column, TRUE);

if ((action->act_error || (action->act_flags & ACT_sql)) && sw_auto)
    printa (column, "if (%s = 0) and (%s%s /= 0) then", request->req_handle, ada_package, request_trans (action, request));
else
    printa (column, "if %s = 0 then", request->req_handle);

column += INDENT;
#if 0
printa (column, "interbase.COMPILE_REQUEST%s (%s %s%s, %s%s, %d, isc_%d);",
	(request->req_flags & REQ_exp_hand) ? "" : "2",
	status_vector (action),
	ada_package, symbol->sym_string,
	request_trans (action, request), (request->req_flags & REQ_exp_hand) ? "" : "'address",
	request->req_length, request->req_ident);
#endif
printa (column, "interbase.COMPILE_REQUEST%s (%s %s%s, %s%s, %d, isc_%d);",
	(request->req_flags & REQ_exp_hand) ? "" : "2",
	status_vector (action),
	ada_package, symbol->sym_string,
	request->req_handle, (request->req_flags & REQ_exp_hand) ? "" : "'address",
	request->req_length, request->req_ident);

SET_SQLCODE;
column -= INDENT;
ENDIF;


if (blob = request->req_blobs)
    for (; blob; blob = blob->blb_next)
	printa (column - INDENT, "isc_%d := 0;", blob->blb_ident);
}

static gen_create_database (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to create a database.
 *
 **************************************/
REQ	request;
DBB	db;
USHORT   save_sw_auto;
TEXT	s1 [32], s2 [32];

request = ((MDBB) action->act_object)->mdbb_dpb_request;
db = (DBB) request->req_database;

sprintf (s1, "isc_%dl", request->req_ident);

if (request->req_flags & REQ_extend_dpb)
    {
    sprintf (s2, "isc_%dp", request->req_ident);
    if (request->req_length)
	printa (column, "%s = isc_to_dpb_ptr (isc_%d'address);", s2, request->req_ident);
    	if (db->dbb_r_user)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_user_name, %s, %d);\n",
	    	s2, s1, db->dbb_r_user,  (strlen(db->dbb_r_user) -2) );
    	if (db->dbb_r_password)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_password, %s, %d);\n",
	    	s2, s1, db->dbb_r_password,  (strlen(db->dbb_r_password)-2) );
	/*
	** ===========================================================
	** ==
	** ==   Process Role Name
	** ==
	** ===========================================================
	*/
    	if (db->dbb_r_sql_role)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_sql_role_name, %s, %d);\n",
	    	s2, s1, db->dbb_r_sql_role,  (strlen(db->dbb_r_sql_role)-2) );

    	if (db->dbb_r_lc_messages)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_lc_messages, %s, %d);\n",
	    	s2, s1, db->dbb_r_lc_messages,  ( strlen(db->dbb_r_lc_messages) -2) );
    	if (db->dbb_r_lc_ctype)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_lc_ctype, %s, %d);\n",
	    	s2, s1, db->dbb_r_lc_ctype,  ( strlen(db->dbb_r_lc_ctype) -2) );
	
    }
else
    sprintf (s2, "isc_to_dpb_ptr (isc_%d'address)", request->req_ident);

if (request->req_length)
    printa (column,
	     "interbase.CREATE_DATABASE (%s %d, \"%s\", %s%s, %s, %s, 0);",
	     status_vector (action),
	     strlen (db->dbb_filename),
	     db->dbb_filename,
	     ada_package, db->dbb_name->sym_string,
	     s1,	
	     s2);
else
    printa (column,
	     "interbase.CREATE_DATABASE (%s %d, \"%s\", %s%s, 0, interbase.null_dpb, 0);",
	     status_vector (action),
	     strlen (db->dbb_filename),
	     db->dbb_filename,
	     ada_package, db->dbb_name->sym_string);

if (request && request->req_flags & REQ_extend_dpb)
    {
    if (request->req_length)
	printa (column, "if (%s != isc_%d)", s2, request->req_ident);
    printa (column + (request->req_length ? 4 : 0), 
				"interbase.FREE (%s);", s2);

    /* reset the length of the dpb */
    if (request->req_length)
    printa (column, "%s = %d;", s1, request->req_length);
    }

printa (column, "if %sisc_status(1) = 0 then", ada_package);
column += INDENT;
save_sw_auto = sw_auto;
sw_auto = (USHORT) TRUE;
gen_ddl (action, column);
sw_auto = save_sw_auto;
column -= INDENT;
printa (column, "else");
column += INDENT;
SET_SQLCODE;
column -= INDENT;
printa (column, "end if;");
}

static gen_cursor_close (
    ACT		action,
    REQ		request,
    int		column)
{
/**************************************
 *
 *	g e n _ c u r s o r _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STREAM.
 *
 **************************************/

return column;
}

static gen_cursor_init (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ c u r s o r _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Generate text to initialize a cursor.
 *
 **************************************/

/* If blobs are present, zero out all of the blob handles.  After this
int, the handles are the user's responsibility */

if (action->act_request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
    printa (column, "gds_%d := 0;", action->act_request->req_blobs->blb_ident);
}

static gen_cursor_open (
    ACT		action,
    REQ		request,
    int		column)
{
/**************************************
 *
 *	g e n _ c u r s o r _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate text to open an embedded SQL cursor.
 *
 **************************************/

return column;
}

static gen_database (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate insertion text for the database statement,
 *	including the definitions of all requests, and blob
 *	and port declarations for requests in the main routine.
 *
 **************************************/
DBB		db;
REQ		request;
POR		port;
BLB		blob;
USHORT		count;
SSHORT		blob_subtype;
USHORT		event_count, max_count;
FORM		form;
LLS		stack_ptr;
TPB		tpb;
REF		reference;
FLD		field;
BOOLEAN		array_flag;

if (first_flag++ != 0)
    return;

ib_fprintf (out_file, "\n----- GPRE Preprocessor Definitions -----\n");

for (request = requests; request; request = request->req_next)
    {
    if (request->req_flags & REQ_local)
	continue;
    for (port = request->req_ports; port; port = port->por_next)
	make_port (port, column);
    }
ib_fprintf (out_file, "\n");

for (db = isc_databases; db; db = db->dbb_next)
    for (form = db->dbb_forms; form; form = form->form_next)
	printa (column, "%s\t: interbase.form_handle := 0;\t-- form %s --",
	    form->form_handle, form->form_name->sym_string);

for (request = requests; request; request = request->req_routine)
    {
    if (request->req_flags & REQ_local)
	continue;
    for (port = request->req_ports; port; port = port->por_next)
	printa  (column, "isc_%d\t: isc_%dt;\t\t\t-- message --", 
	    port->por_ident, 
	    port->por_ident);

    for (blob = request->req_blobs; blob; blob = blob->blb_next)
	{
	printa  (column, "isc_%d\t: interbase.blob_handle;\t-- blob handle --",
    	    blob->blb_ident);
	if (!(blob_subtype = blob->blb_const_to_type))
	    if (reference = blob->blb_reference)
		if (field = reference->ref_field)
		    blob_subtype = field->fld_sub_type;
	if (blob_subtype != 0 && blob_subtype != 1)
	    printa  (column, "isc_%d\t: %s (1 .. %d);\t\t-- blob segment --",
		blob->blb_buff_ident, BYTE_VECTOR_DCL, blob->blb_seg_length);
	else
	    printa  (column, "isc_%d\t: string (1 .. %d);\t\t-- blob segment --",
		blob->blb_buff_ident, blob->blb_seg_length);
	printa  (column, "isc_%d\t: %s;\t\t-- segment length --",
	    blob->blb_len_ident, USHORT_DCL);
	}
    }

/* generate event parameter block for each event in module */

max_count = 0;
for (stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next)
    {
    event_count = gen_event_block (stack_ptr->lls_object);
    max_count = MAX (event_count, max_count);
    }

if (max_count)
    printa (column, "isc_events: %s(1..%d);\t-- event vector --", 
		EVENT_LIST_DCL, max_count);

for (request = requests; request; request = request->req_next)
    {
    gen_request (request, column);

    /*  Array declarations  */
    if (port = request->req_primary)
	for (reference = port->por_references; reference; reference = reference->ref_next)
	    if (reference->ref_flags & REF_fetch_array)
		{
		make_array_declaration (reference, column);
		array_flag = TRUE;
		}
    }
if (array_flag)
    {
    printa (column, "isc_array_length\t: %s;\t-- slice return value --", LONG_DCL);
    printa (column, "isc_null_vector_l\t: %s (1..1);\t-- null long vector --", LONG_VECTOR_DCL);
    }

printa (column, "isc_null_bpb\t: interbase.blr (1..1);\t-- null blob parameter block --");

if (!ada_package[0])
    printa (column, "gds_trans\t: interbase.transaction_handle := 0;\t-- default transaction --");
printa (column, "isc_status\t: interbase.status_vector;\t-- status vector --");
printa (column, "isc_status2\t: interbase.status_vector;\t-- status vector --");
printa (column, "SQLCODE\t: %s := 0;\t\t\t-- SQL status code --", LONG_DCL);
if (sw_pyxis && ((sw_window_scope != DBB_EXTERN) || !ada_package[0]))
    {
    printa (column, "isc_window\t: interbase.window_handle := 0;\t-- window handle --");
    printa (column, "isc_width\t: %s := 80;\t-- window width --", USHORT_DCL);
    printa (column, "isc_height\t: %s := 24;\t-- window height --", USHORT_DCL);
    }

if (!ada_package[0])
    for (db = isc_databases; db; db = db->dbb_next)
	printa (column, "%s\t\t: interbase.database_handle%s;-- database handle --",
		db->dbb_name->sym_string,
		(db->dbb_scope == DBB_EXTERN) ? "" : " := 0");

printa (column, " ");

count = 0;
for (db = isc_databases; db; db = db->dbb_next)
    {
    count++;
    for (tpb = db->dbb_tpbs; tpb; tpb = tpb->tpb_dbb_next)
	gen_tpb (tpb, column + INDENT);
    }

printa (column,
	"isc_teb\t: array (1..%d) of interbase.isc_teb_t;\t-- transaction vector ",
	count);

printa (column, "----- end of GPRE definitions ");
}

static gen_ddl (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d d l
 *
 **************************************
 *
 * Functional description
 *	Generate a call to update metadata.
 *
 **************************************/
REQ	request;

/* Set up command type for call to RDB$DDL */

request = action->act_request;

/* Generate a call to RDB$DDL to perform action */

if (sw_auto)
    {
    t_start_auto (action, NULL_PTR, status_vector (action), column, TRUE);
    printa (column, "if %sgds_trans /= 0 then", ada_package);
    column += INDENT;
    }

printa (column, "interbase.DDL (%s %s%s, %s%s, %d, isc_%d'address);",
    status_vector (action),
    ada_package, request->req_database->dbb_name->sym_string,
    ada_package, request->req_trans,
    request->req_length,	
    request->req_ident);

if (sw_auto)
    {
    printa (column, "if %sisc_status(1) = 0 then", ada_package);
    printa (column + INDENT, "interbase.commit_TRANSACTION (%s %sgds_trans);",
	status_vector (action), ada_package);
    printa (column, "else", ada_package);
    printa (column + INDENT, "interbase.rollback_TRANSACTION (%sgds_trans);",
	ada_package);
    ENDIF;
    column -= INDENT;
    ENDIF;
    }

SET_SQLCODE;
}

static gen_drop_database (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to create a database.
 *
 **************************************/
REQ	request;
DBB	db;

db = (DBB) action->act_object;
request = action->act_request;

printa (column,
	"interbase.DROP_DATABASE (%s %d, \"%s\", RDBK_DB_TYPE_GDS);",
	status_vector (action),
	strlen (db->dbb_filename),
	db->dbb_filename);

SET_SQLCODE;
}

static gen_dyn_close (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s [64];

statement = (DYN) action->act_object;
printa (column, 
    "interbase.embed_dsql_close (%s %s);",
    status_vector (action),
    make_name (s, statement->dyn_cursor_name));
SET_SQLCODE;
}

static gen_dyn_declare (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ d e c l a r e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s1 [64], s2 [64];

statement = (DYN) action->act_object;
printa (column, 
    "interbase.embed_dsql_declare (%s %s, %s);",
    status_vector (action),
    make_name (s1, statement->dyn_statement_name),
    make_name (s2, statement->dyn_cursor_name));
SET_SQLCODE;
}

static gen_dyn_describe (
    ACT		action,
    int		column,
    BOOLEAN	input_flag)
{
/**************************************
 *
 *	g e n _ d y n _ d e s c r i b e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s[64];

statement = (DYN) action->act_object;
printa (column, 
    "interbase.embed_dsql_describe%s (%s %s, %d, %s'address);",
    input_flag ? "_bind" : "",
    status_vector (action),
    make_name (s, statement->dyn_statement_name),
    sw_sql_dialect,
    statement->dyn_sqlda);
SET_SQLCODE;
}

static gen_dyn_execute (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
TEXT		*transaction, s [64];
struct req	*request, req_const;
int		i;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds_trans";
    request = NULL;
    }

if (sw_auto)
    {
    t_start_auto (action, request, status_vector (action), column, TRUE);
    printa (column, "if %s%s /= 0 then", ada_package, transaction);
    column += INDENT;
    }

if (statement->dyn_sqlda2)
    printa (column, 
	"interbase.embed_dsql_execute2 (isc_status, %s%s, %s, %d, %s'address);",
	ada_package, transaction,
	make_name (s, statement->dyn_statement_name),
	sw_sql_dialect,
	(statement->dyn_sqlda) ? ada_null_address : statement->dyn_sqlda,
	statement->dyn_sqlda2);
else if (statement->dyn_sqlda)
    printa (column, 
	"interbase.embed_dsql_execute (isc_status, %s%s, %s, %d, %s'address);",
	ada_package, transaction,
	make_name (s, statement->dyn_statement_name),
	sw_sql_dialect,
	statement->dyn_sqlda);
else
    printa (column, 
	"interbase.embed_dsql_execute (isc_status, %s%s, %s, %d);",
	ada_package, transaction,
	make_name (s, statement->dyn_statement_name), sw_sql_dialect);

if (sw_auto)
    {
    column -= INDENT;
    ENDIF;
    }

SET_SQLCODE;
}

static gen_dyn_fetch (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s[64];

statement = (DYN) action->act_object;
if (statement->dyn_sqlda)
    printa (column, 
	"interbase.embed_dsql_fetch (isc_status, SQLCODE, %s, %d, %s'address);",
	make_name (s, statement->dyn_cursor_name),
	sw_sql_dialect,
	statement->dyn_sqlda);
else
    printa (column, 
	"interbase.embed_dsql_fetch (isc_status, SQLCODE, %s, %d);",
	make_name (s, statement->dyn_cursor_name), sw_sql_dialect);

printa (column, "if SQLCODE /= 100 then");
printa (column + INDENT, "SQLCODE := interbase.sqlcode (isc_status);");
ENDIF;
}

static gen_dyn_immediate (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ i m m e d i a t e
 *
 **************************************
 *
 * Functional description
 *	Generate code for an EXECUTE IMMEDIATE dynamic SQL statement.
 *
 **************************************/
DYN		statement;
DBB		database;
TEXT		*transaction, s [64];
struct req	*request, req_const;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds_trans";
    request = NULL;
    }

database = statement->dyn_database;

if (sw_auto)
    {
    t_start_auto (action, request, status_vector (action), column, TRUE);
    printa (column, "if %s%s /= 0 then", ada_package, transaction);
    column += INDENT;
    }

printa (column,
    "interbase.embed_dsql_execute_immed (isc_status, %s%s, %s%s, %s'length(1), %s, %d);",
    ada_package, database->dbb_name->sym_string,
    ada_package, transaction,
    statement->dyn_string,
    statement->dyn_string, sw_sql_dialect);

if (sw_auto)
    {
    column -= INDENT;
    ENDIF;
    }

SET_SQLCODE;
}

static gen_dyn_insert (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ i n s e r t
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s[64];

statement = (DYN) action->act_object;
if (statement->dyn_sqlda)
    printa (column, 
	"interbase.embed_dsql_insert (isc_status, %s, %d, %s'address);",
	make_name (s, statement->dyn_cursor_name),
	sw_sql_dialect,
	statement->dyn_sqlda);
else
    printa (column, 
	"interbase.embed_dsql_insert (isc_status, %s, %d);",
	make_name (s, statement->dyn_cursor_name), sw_sql_dialect);

SET_SQLCODE;
}

static gen_dyn_open (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
TEXT		*transaction, s [64];
struct req	*request, req_const;
int		i;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds_trans";
    request = NULL;
    }

make_name (s, statement->dyn_cursor_name);

if (sw_auto)
    {
    t_start_auto (action, request, status_vector (action), column, TRUE);
    printa (column, "if %s%s /= 0 then", ada_package, transaction);
    column += INDENT;
    }

if (statement->dyn_sqlda)
    printa (column, 
	"interbase.embed_dsql_open (isc_status, %s%s, %s, %d, %s'address);",
	ada_package, transaction,
	s,
	sw_sql_dialect,
	statement->dyn_sqlda);
else
    printa (column, 
	"interbase.embed_dsql_open (isc_status, %s%s, %s, %d);",
	ada_package, transaction, s, sw_sql_dialect);

if (sw_auto)
    {
    column -= INDENT;
    ENDIF;
    }

SET_SQLCODE;
}

static gen_dyn_prepare (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ d y n _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
DBB		database;
TEXT		*transaction, s[64];
struct req	*request, req_const;

statement = (DYN) action->act_object;
database = statement->dyn_database;

if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds_trans";
    request = NULL;
    }

if (sw_auto)
    {
    t_start_auto (action, request, status_vector (action), column, TRUE);
    printa (column, "if %s%s /= 0 then", ada_package, transaction);
    column += INDENT;
    }

if (statement->dyn_sqlda)
    printa (column, "interbase.embed_dsql_prepare (isc_status, %s%s, %s%s, %s, %s'length(1), %s, %d, %s'address);",
	ada_package, database->dbb_name->sym_string, ada_package, 
	transaction,
	make_name (s, statement->dyn_statement_name),
	statement->dyn_string,
	statement->dyn_string,
	sw_sql_dialect,
	statement->dyn_sqlda);
else
    printa (column, "interbase.embed_dsql_prepare (isc_status, %s%s, %s%s, %s, %s'length(1), %s, %d);",
	ada_package, database->dbb_name->sym_string, ada_package, 
	transaction,
	make_name (s, statement->dyn_statement_name),
	statement->dyn_string,
	statement->dyn_string, sw_sql_dialect);

if (sw_auto)
    {
    column -= INDENT;
    ENDIF;
    }

SET_SQLCODE;
}

static gen_emodify (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_MODIFY.
 *
 **************************************/
UPD	modify;
REF	reference, source;
FLD	field;
TEXT	s1[20], s2[20];

modify = (UPD) action->act_object;

for (reference= modify->upd_port->por_references; reference; 
    reference = reference->ref_next)	
    {
    if (!(source = reference->ref_source))
	continue;
    field = reference->ref_field;
    printa (column, "%s := %s;",
	    gen_name (s1, reference, TRUE),
	    gen_name (s2, source, TRUE));
    if (field->fld_array_info)
	gen_get_or_put_slice (action, reference, FALSE, column);
    }

gen_send (action, modify->upd_port, column);
}

static gen_estore (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STORE.
 *
 **************************************/
REQ	request;

request = action->act_request;

/* if we did a store ... returning_values aka store2
   just wrap up any dangling error handling */
if (request->req_type == REQ_store2)
    {
    if (action->act_error)
	ENDIF;
    return;
    }

gen_start (action, request->req_primary, column);
if (action->act_error)
    ENDIF;
}

static gen_endfor (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e n d f o r
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/
REQ	request;

request = action->act_request;

if (request->req_sync)
    gen_send (action, request->req_sync, column + INDENT);

printa (column, "end loop;");

if (action->act_error || (action->act_flags & ACT_sql))
    ENDIF;
}

static gen_erase (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for ERASE.
 *
 **************************************/
UPD	erase;

erase = (UPD) action->act_object;
gen_send (action, erase->upd_port, column);
}

static SSHORT gen_event_block (
    ACT		action)
{
/**************************************
 *
 *	g e n _ e v e n t _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Generate event parameter blocks for use
 *	with a particular call to isc_event_wait.
 *	For languages too dim to deal with variable
 *	arg lists, set up a vector for the event names.
 *
 **************************************/
NOD	init, list;
SYM	event_name;
PAT	args;
int	ident;

init = (NOD) action->act_object;
event_name = (SYM) init->nod_arg [0];

ident = CMP_next_ident();
init->nod_arg [2] = (NOD) ident;
list = init->nod_arg [1];

printa (0, "isc_%da\t\t: system.address;\t\t-- event parameter block --\n", 
		ident);
printa (0, "isc_%db\t\t: system.address;\t\t-- result parameter block --\n", 
		ident);
printa (0, "isc_%dc\t\t: %s;\t\t-- count of events --\n", 
		ident, USHORT_DCL);
printa (0, "isc_%dl\t\t: %s;\t\t-- length of event parameter block --\n", 
		ident, USHORT_DCL);
printa (0, "isc_%dv\t\t: interbase.isc_el_t (1..%d);\t\t-- vector for initialization --\n", 
		ident, list->nod_count);

return list->nod_count;
}

static gen_event_init (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e v e n t _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for EVENT_INIT.
 *
 **************************************/
NOD	init, event_list, *ptr, *end, node;
REF	reference;
PAT	args;
TEXT	variable [32];
USHORT	count;
TEXT	
 *pattern1 = "isc_%N1l := interbase.event_block (%RFisc_%N1a'address, %RFisc_%N1b'address, isc_%N1c, %RFisc_%N1v);", 
 *pattern2 = "interbase.event_wait (%V1 %RF%DH, isc_%N1l, isc_%N1a, isc_%N1b);",
 *pattern3 = "interbase.event_counts (isc_events, isc_%N1l, isc_%N1a, isc_%N1b);"; 

if (action->act_error)
    BEGIN;
BEGIN;

init = (NOD) action->act_object;
event_list = init->nod_arg [1];

args.pat_database = (DBB) init->nod_arg [3];
args.pat_vector1 = status_vector (action);
args.pat_value1 = (int) init->nod_arg [2];

/* generate call to dynamically generate event blocks */

for (ptr = event_list->nod_arg, count = 0, end = ptr + event_list->nod_count; ptr < end; ptr++ )
    {
    count++;
    node = *ptr;
    if (node->nod_type == nod_field)
	{
	reference = (REF) node->nod_arg [0];
	gen_name (variable, reference, TRUE);
	printa (column, "isc_%dv(%d) := %s'address;",
			args.pat_value1, count, variable);
	}
    else
	printa (column, "isc_%dv(%d) := %s'address;",
			args.pat_value1, count, node->nod_arg[0]);
    }

printa (column, "isc_%dc := %d;", args.pat_value1, (int)event_list->nod_count);
PATTERN_expand (column, pattern1, &args);

/* generate actual call to event_wait */

PATTERN_expand (column, pattern2, &args);

/* get change in event counts, copying event parameter block for reuse */

PATTERN_expand (column, pattern3, &args);
      
END;
SET_SQLCODE;
}

static gen_event_wait (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e v e n t _ w a i t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for EVENT_WAIT.
 *
 **************************************/
PAT	args;
NOD	event_init;
SYM	event_name, stack_name;
DBB	database;
LLS	stack_ptr;
ACT	event_action;
int	ident;
TEXT	s [64];
TEXT	
  *pattern1 = "interbase.event_wait (%V1 %RF%DH, isc_%N1l, isc_%N1a, isc_%N1b);",
  *pattern2 = "interbase.event_counts (isc_events, isc_%N1l, isc_%N1a, isc_%N1b);";

if (action->act_error)
    BEGIN;
BEGIN;    

event_name = (SYM) action->act_object;

/* go through the stack of events, checking to see if the
   event has been initialized and getting the event identifier */

ident = -1;
for (stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next)
    {
    event_action = (ACT) stack_ptr->lls_object;
    event_init = (NOD) event_action->act_object;
    stack_name = (SYM) event_init->nod_arg [0];
    if (!strcmp (event_name->sym_string, stack_name->sym_string))
	{
	ident = (int) event_init->nod_arg [2];
	database = (DBB) event_init->nod_arg [3];
	}
    }

if (ident < 0)
    {
    sprintf (s, "event handle \"%s\" not found", event_name->sym_string);
    return IBERROR (s);
    }

args.pat_database = database;
args.pat_vector1 = status_vector (action);
args.pat_value1 = (int) ident;

/* generate calls to wait on the event and to fill out the events array */

PATTERN_expand (column, pattern1, &args);
PATTERN_expand (column, pattern2, &args);

END;
SET_SQLCODE;
}


static gen_fetch (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate replacement text for the SQL FETCH statement.  The
 *	epilog FETCH statement is handled by GEN_S_FETCH (generate
 *	stream fetch).
 *
 **************************************/
REQ	request;
NOD	var_list;
POR	port;
REF	reference;
VAL	value;
int	i;
SCHAR	s[20], *direction, *offset;

request = action->act_request;

#ifdef SCROLLABLE_CURSORS
if (port = request->req_aport)
    {
    /* set up the reference to point to the correct value 
       in the linked list of values, and prepare for the 
       next FETCH statement if applicable */

    for (reference = port->por_references; reference; reference = reference->ref_next)
        {
        value = reference->ref_values;
        reference->ref_value = value->val_value;
	reference->ref_values = value->val_next;
	}

    /* find the direction and offset parameters */

    reference = port->por_references; 
    offset = reference->ref_value;
    reference = reference->ref_next; 
    direction = reference->ref_value; 

    /* the direction in which the engine will scroll is sticky, so check to see 
       the last direction passed to the engine; if the direction is the same and 
       the offset is 1, then there is no need to pass the message; this prevents 
       extra packets and allows for batch fetches in either direction */

    printa (column, "if isc_%ddirection MOD 2 /= %s or %s /= 1 then", request->req_ident, direction, offset);
    column += INDENT; 

    /* assign the direction and offset parameters to the appropriate message, 
       then send the message to the engine */ 

    asgn_from (action, port->por_references, column);
    gen_send (action, port, column);
    printa (column, "isc_%ddirection := %s;", request->req_ident, direction);
    column -= INDENT; 
    ENDIF;

    printa (column, "if SQLCODE = 0 then");
    column += INDENT;
    }    
#endif

if (request->req_sync)
    {
    gen_send (action, request->req_sync, column);
    printa (column, "if SQLCODE = 0 then");
    column += INDENT;
    }    

gen_receive (action, column, request->req_primary);
printa (column, "if SQLCODE = 0 then"); 
column += INDENT;
printa (column, "if %s /= 0 then", gen_name (s, request->req_eof, TRUE));
column += INDENT;

if (var_list = (NOD) action->act_object)
    {
    for (i = 0; i < var_list->nod_count; i++)
	asgn_to (action, var_list->nod_arg [i], column);
    }
else  /*  No WITH clause on the FETCH, so no assignments generated;  fix
          for bug#940.  mao 6/20/89  */
    {
    printa (column, "NULL;");
    }

column -= INDENT;
printa (column, "else");
printa (column + INDENT, "SQLCODE := 100;");
ENDIF;
column -= INDENT;
ENDIF;
column -= INDENT;

if (request->req_sync)
    ENDIF;

#ifdef SCROLLABLE_CURSORS
if (port)
    {
    column -= INDENT;
    ENDIF;
    }
#endif
}

static gen_finish (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f i n i s h
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FINISH.
 *
 **************************************/
DBB	db;
REQ	request;
RDY	ready;

db = NULL;

if (sw_auto || ((action->act_flags & ACT_sql) &&
		(action->act_type != ACT_disconnect))) 
    {
    printa (column, "if %sgds_trans /= 0 then", ada_package);
    printa (column + INDENT, "interbase.%s_TRANSACTION (%s %sgds_trans);",
	(action->act_type != ACT_rfinish) ? "COMMIT" : "ROLLBACK",
	status_vector (action), ada_package);
    ENDIF;
    }

/* the user supplied one or more db_handles */

for (ready = (RDY) action->act_object; ready; ready = ready->rdy_next)
    {
    db = ready->rdy_database;
    if (action->act_error || (action->act_flags & ACT_sql))
	    printa (column, "if %s%s /= 0 then", ada_package, db->dbb_name->sym_string);
    printa (column, "interbase.DETACH_DATABASE (%s %s%s);",
	    status_vector (action),
	    ada_package, db->dbb_name->sym_string);
    if (action->act_error || (action->act_flags & ACT_sql))
	ENDIF;
    }

/* no hanbdles, so we finish all known databases */

if (!db)
    {
    if (action->act_error || (action->act_flags & ACT_sql))
	printa (column, "if %sgds_trans = 0 then", ada_package);
    for (db = isc_databases; db; db  = db->dbb_next)
	{
	if ((action->act_error || (action->act_flags & ACT_sql)) && (db != isc_databases))
	    printa (column, "if %s%s /= 0 and isc_status(1) = 0 then", ada_package, db->dbb_name->sym_string);
	else
	    printa (column, "if %s%s /= 0 then", ada_package, db->dbb_name->sym_string);
	column += INDENT;
	printa (column, "interbase.DETACH_DATABASE (%s %s%s);",
		status_vector (action),
		ada_package, db->dbb_name->sym_string);
	for (request = requests; request; request = request->req_next)
	     if (!(request->req_flags & REQ_exp_hand) && 
		 request->req_type != REQ_slice && 
		 request->req_type != REQ_procedure &&
		 request->req_database == db)
		printa (column, "%s := 0;", request->req_handle);
	column -= INDENT;
	ENDIF;
	}
    if (action->act_error || (action->act_flags & ACT_sql))
	ENDIF;
    }

SET_SQLCODE;
}

static gen_for (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f o r 
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FOR statement.
 *
 **************************************/
POR	port;
REQ	request;
TEXT	s[20];
REF     reference;

gen_s_start (action, column);
request = action->act_request;
    
if (action->act_error || (action->act_flags & ACT_sql))
    printa (column, "if isc_status(1) = 0 then");

printa (column, "loop");
column += INDENT;
gen_receive (action, column, request->req_primary);

if (action->act_error || (action->act_flags & ACT_sql))
    printa (column, "exit when (%s = 0) or (isc_status(1) /= 0);",
	    gen_name (s, request->req_eof, TRUE));
else
    printa (column, "exit when %s = 0;", gen_name (s, request->req_eof, TRUE));

if (port = action->act_request->req_primary)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_field->fld_array_info)
	    gen_get_or_put_slice (action, reference, TRUE, column);
}

static gen_form_display (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f o r m _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form interaction.
 *
 **************************************/
FINT	display;
REQ	request;
REF	reference, master;
POR	port;
DBB	dbb;
USHORT	value;
TEXT	s [32], out [16];
int	code;

display = (FINT) action->act_object;
request = display->fint_request;
dbb = request->req_database;
port = request->req_ports;

/* Initialize field options */

for (reference = port->por_references; reference;
     reference = reference->ref_next)
    if ((master = reference->ref_master) &&
	(code = CMP_display_code (display, master)) >= 0)
	printa (column, "%s := %d;", gen_name (s, reference, TRUE), code);

if (display->fint_flags & FINT_no_wait)
    strcpy (out, "0");
else
    sprintf (out, "isc_%d", port->por_ident);

printa (column, "interbase.drive_form (%s %s%s, %s%s, %sisc_window, %s, isc_%d'address, %s'address)",
	status_vector (action),
	ada_package, dbb->dbb_name->sym_string,
	ada_package, request->req_trans,
	ADA_WINDOW_PACKAGE,
	request->req_handle, 
	port->por_ident, 
	out);
}

static gen_form_end (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f o r m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form block.
 *
 **************************************/
REQ	request;
FORM	form;
TEXT	*status;
DBB	dbb;

request = action->act_request;
printa (column, "interbase.pop_window (%sisc_window)", ADA_WINDOW_PACKAGE);
}

static gen_form_for (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ f o r m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form block.
 *
 **************************************/
REQ	request;
FORM	form;
TEXT	*status;
DBB	dbb;
int	indent;

indent = column + INDENT;
request = action->act_request;
form = request->req_form;
dbb = request->req_database;
status = status_vector (action);

/* Get database attach and transaction started */

if (sw_auto)
    t_start_auto (action, NULL_PTR, status_vector (action), indent, TRUE);

/* Get form loaded first */

printa (column, "if %s = 0 then", request->req_form_handle);
printa (column, "interbase.load_form (%s %s%s, %s%s, %s, %d, \"%s\");",
    status,
    ada_package, dbb->dbb_name->sym_string,
    ada_package, request->req_trans,
    request->req_form_handle,
    strlen (form->form_name->sym_string),
    form->form_name->sym_string);
ENDIF;

/* Get map compiled */

printa (column, "if %s = 0 then", request->req_handle);
printa (indent, "interbase.compile_map (%s %s, %s, %d, isc_%d);",
    status,
    request->req_form_handle,
    request->req_handle,
    request->req_length,
    request->req_ident);
ENDIF;

/* Reset form to known state */

printa (column, "interbase.reset_form (%s %s);",
    status,
    request->req_handle);
}

static int gen_function (
    ACT         function,
    int         column)
{
/**************************************
 *
 *      g e n _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *      Generate a function for free standing ANY or statistical.
 *
 **************************************/
REQ     request;
POR     port;
REF     reference;
FLD     field;
ACT     action;
TEXT    *dtype, s[64];

action = (ACT) function->act_object;

if (action->act_type != ACT_any)
    {
    IBERROR ("can't generate function");
    return;
    }

request = action->act_request;

ib_fprintf (out_file, "static %s_r (request, transaction",
request->req_handle, request->req_handle, request->req_trans);

if (port = request->req_vport)
    for (reference = port->por_references; reference;
         reference = reference->ref_next)
        ib_fprintf (out_file, ", %s", gen_name (s, reference->ref_source, TRUE));
ib_fprintf (out_file, ")\n    isc_req_handle\trequest;\n    isc_tr_handle\ttransaction;\n",
    request->req_handle, request->req_trans);

if (port)
    for (reference = port->por_references; reference;
         reference = reference->ref_next)
        {
        field = reference->ref_field;
        switch (field->fld_dtype)
            {
            case dtype_short:
                dtype = "short";
                break;

            case dtype_long:
                dtype = "long";
                break;

            case dtype_cstring:
            case dtype_text:
                dtype = "char*";
                break;

            case dtype_quad:
                dtype = "long";
                break;

            case dtype_date:
            case dtype_blob:
                dtype = "ISC_QUAD";
                break;

	    case dtype_real:
                dtype = "float";
                break;

            case dtype_double:
                dtype = "double";
                break;

            default:
                IBERROR ("gen_function: unsupported datatype");
                return;
            }
        ib_fprintf (out_file, "    %s\t%s;\n", dtype,
            gen_name (s, reference->ref_source, TRUE));
        }

ib_fprintf (out_file, "{\n");
for (port = request->req_ports; port; port = port->por_next)
    make_port (port, column);

ib_fprintf (out_file, "\n\n");
gen_s_start (action, 0);
gen_receive (action, column, request->req_primary);

for (port = request->req_ports; port; port = port->por_next)
    for (reference = port->por_references; reference; reference = reference->ref_next)
        if (reference->ref_field->fld_array_info)
            gen_get_or_put_slice (action, reference, TRUE, column);

port = request->req_primary;
ib_fprintf (out_file, "\nreturn %s;\n}\n", gen_name (s, port->por_references, TRUE)
);
}

static gen_get_or_put_slice (
    ACT		action,
    REF		reference,
    BOOLEAN	get,
    int		column)
{
/**************************************
 *
 *	g e n _ g e t _ o r _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to isc_get_slice
 *      or isc_put_slice for an array.
 *
 **************************************/
PAT	args;
TEXT    s1[25], s2[25], s3[25], s4[25], s5[25];
TEXT    *pattern1 = "interbase.GET_SLICE (%V1 %RF%DH%RE, %RF%S1%RE, %S2, %N1, %S3, %N2, %S4, %L1, %S5, %S6);";
TEXT    *pattern2 = "interbase.PUT_SLICE (%V1 %RF%DH%RE, %RF%S1%RE, %S2, %N1, %S3, %N2, %S4, %L1, %S5);";

if (!(reference->ref_flags & REF_fetch_array))
    return;

args.pat_vector1 = status_vector (action);             /* status vector */
args.pat_database = action->act_request->req_database; /* database handle */
args.pat_string1 = action->act_request->req_trans;     /* transaction handle */

gen_name (s1, reference, TRUE);                        /* blob handle */
args.pat_string2 = s1;

args.pat_value1 = reference->ref_sdl_length;           /* slice descr. length */

sprintf (s2, "isc_%d", reference->ref_sdl_ident);      /* slice description */
args.pat_string3 = s2;

args.pat_value2 = 0;                                   /* parameter length*/

sprintf (s3, "isc_null_vector_l");                     /* parameter block init */
args.pat_string4 = s3;

args.pat_long1 = reference->ref_field->fld_array_info->ary_size;
                                                       /*  slice size */
if (action->act_flags & ACT_sql)
   {
   sprintf (s4, "%s'address", reference->ref_value);
   }
else
   {
   sprintf (s4, "isc_%d'address", reference->ref_field->fld_array_info->ary_ident);

   }
args.pat_string5 = s4;                                 /* array name */

args.pat_string6 = "isc_array_length";                 /* return length */

if (get)
    PATTERN_expand (column, pattern1, &args);
else
    PATTERN_expand (column, pattern2, &args);
}

static gen_get_segment (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate the code to do a get segment.
 *
 **************************************/
BLB	blob;
REF	into;

if (action->act_flags & ACT_sql)
    blob = (BLB) action->act_request->req_blobs;
else
    blob = (BLB) action->act_object;

if (action->act_error || (action->act_flags & ACT_sql))
    printa (column,
	"interbase.ISC_GET_SEGMENT (isc_status, isc_%d, isc_%d, %d, isc_%d'address);",
	blob->blb_ident,
	blob->blb_len_ident,
	blob->blb_seg_length,
	blob->blb_buff_ident);
else
    printa (column,
	"interbase.ISC_GET_SEGMENT (isc_status(1), isc_%d, isc_%d, %d, isc_%d'address);",
	blob->blb_ident,
	blob->blb_len_ident,
	blob->blb_seg_length,
	blob->blb_buff_ident);

if (action->act_flags & ACT_sql)
    {
    into = action->act_object;
    SET_SQLCODE;
    printa (column, "if (SQLCODE = 0) or (SQLCODE = 101) then");
    printa (column + INDENT, "%s := isc_%d;",
	into->ref_value, blob->blb_buff_ident);
    if (into->ref_null_value)
	printa (column + INDENT, "%s := isc_%d;",
	    into->ref_null_value, blob->blb_len_ident);
    ENDIF;
    }
}

static gen_item_end (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ i t e m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate end of block for PUT_ITEM and FOR_ITEM.
 *
 **************************************/
ACT	org_action;
FINT	display;
REQ	request;
REF	reference, master;
POR	port;
DBB	dbb;
USHORT	value;
TEXT	s [32], *status, index [16];
int	code;

request = action->act_request;
if (request->req_type == REQ_menu)
    {
    gen_menu_item_end (action, column);
    return;
    }

if (action->act_pair->act_type == ACT_item_for)
    {
    column += INDENT;
    gen_name (index, request->req_index, TRUE);
    printa (column, "%s := %s + 1;", index, index);
    printa (column, "end loop");
    return;
    }

dbb = request->req_database;
port = request->req_ports;

/* Initialize field options */

for (reference = port->por_references; reference;
     reference = reference->ref_next)
    if (master = reference->ref_master)
	printa (column, "%s := %d;", gen_name (s, reference, TRUE), PYXIS__OPT_DISPLAY);

printa (column,
    "interbase.insert_sub_form (%s %s%s, %s%s, %s, isc_%d'address)",
	status_vector (action),
	ada_package, dbb->dbb_name->sym_string,
	ada_package, request->req_trans,
	request->req_handle, 
	port->por_ident);
}

static gen_item_for (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ i t e m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate insert text for FOR_ITEM and PUT_ITEM.
 *
 **************************************/
REQ	request, parent;
FORM	form;
TEXT	*status, index [30];

request = action->act_request;
if (request->req_type == REQ_menu)
    {
    gen_menu_item_for (action, column);
    return;
    }

column += INDENT;
form = request->req_form;
parent = form->form_parent;

status = status_vector(action);

/* Get map compiled */

printa (column, "if %s = 0 then", request->req_handle);
printa (column + INDENT, "interbase.compile_sub_map (%s %s, %s, %d, isc_%d);",
    status,
    parent->req_handle,
    request->req_handle,
    request->req_length,
    request->req_ident);
printa (column, "end if;");

if (action->act_type != ACT_item_for)
    return;

/* Build stuff for item loop */

gen_name (index, request->req_index, TRUE);
printa (column, "%s := 1;", index);
printa (column, "loop");
column += INDENT;
printa (column,
    "interbase.fetch_sub_form (%s %s%s, %s%s, %s, isc_%d'address);",
	status,
	ada_package, request->req_database->dbb_name->sym_string,
	ada_package, request->req_trans,
	request->req_handle, 
	request->req_ports->por_ident);
printa (column, "exit when %s = 0;", index);
}

static gen_loop (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ l o o p
 *
 **************************************
 *
 * Functional description
 *	Generate code to drive a mass update.  Just start a request
 *	and get the result.
 *
 **************************************/
REQ	request;
POR	port;
TEXT	name [20];

gen_s_start (action, column);
request = action->act_request;
port = request->req_primary;
printa (column, "if SQLCODE = 0 then");
column += INDENT;
gen_receive (action, column, port);
gen_name (name, port->por_references, TRUE);
printa (column, "if (SQLCODE = 0) and (%s = 0) then", name);
printa (column + INDENT, "SQLCODE := 100;");
ENDIF;
column -= INDENT;
ENDIF;
}

static gen_menu (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ	request;

request = action->act_request;
printa (column, "case interbase.menu (%sisc_window, %s, %d, isc_%d) is",
    ADA_WINDOW_PACKAGE,
    request->req_handle,
    request->req_length,
    request->req_ident);
}

static gen_menu_display (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Generate code for a menu interaction.
 *
 **************************************/
MENU		menu;
REQ		request, display_request;

request = action->act_request;
display_request = (REQ) action->act_object;
 
menu = NULL;

for (action = request->req_actions; action; action = action->act_next)
    if (action->act_type == ACT_menu_for)
	{
	menu = (MENU) action->act_object;
	break;
	}

printa (column,
    "interbase.drive_menu (%sisc_window, %s, %d, isc_%d, isc_%dl, isc_%d,",
	ADA_WINDOW_PACKAGE,
	request->req_handle, 
	display_request->req_length,
	display_request->req_ident,
	menu->menu_title,
	menu->menu_title);

printa (column,
	"\n\t\t\tisc_%d, isc_%dl, isc_%d, isc_%d);",
	menu->menu_terminator,
	menu->menu_entree_entree,
	menu->menu_entree_entree,
	menu->menu_entree_value);
}

static gen_menu_end (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ e n d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

REQ	request;

request = action->act_request;
if (request->req_flags & REQ_menu_for)
    return;

printa (column, "when others =>");
printa (column + INDENT, "null ;");
printa (column, "end case");
}

static gen_menu_entree (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ e n t r e e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ	request;

request = action->act_request;

printa (column, "when %d =>", action->act_count);
}

static gen_menu_entree_att (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ e n t r e e _ a t t
 *
 **************************************
 *
 * Functional description
 *   Generate code for a reference to a menu or entree attribute.
 *
 **************************************/
MENU	menu;
SSHORT	ident, length;

menu = (MENU) action->act_object;

length = FALSE;
switch (action->act_type)
    {
    case ACT_entree_text: 		ident = menu->menu_entree_entree;	break;
    case ACT_entree_length: 		ident = menu->menu_entree_entree;	length = TRUE;	break;
    case ACT_entree_value:	 	ident = menu->menu_entree_value;	break;
    case ACT_title_text: 		ident = menu->menu_title;		break;
    case ACT_title_length:	 	ident = menu->menu_title;		length = TRUE;	break;
    case ACT_terminator:	 	ident = menu->menu_terminator;		break;
    default:	ident = -1;	break;
    }

if (length)
    printa (column, "isc_%dl", ident);
else
    printa (column, "isc_%d", ident);
}

static gen_menu_for (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate code for a menu block.
 *
 **************************************/
REQ	request;

request = action->act_request;

/* Get menu created */

if (!(request->req_flags & REQ_exp_hand))
    printa (column, "interbase.initialize_menu (%s);",
	request->req_handle);
}

static gen_menu_item_end (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ i t e m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate end of block for PUT_ITEM and FOR_ITEM
 *	for a dynamic menu.
 *
 **************************************/
REQ	request;
ENTREE	entree;

if (action->act_pair->act_type == ACT_item_for)
    {
    column += INDENT;
    printa (column, "end loop");
    return;
    }

entree = (ENTREE) action->act_pair->act_object;
request = entree->entree_request;

align (column);
ib_fprintf (out_file,
    "interbase.put_entree (%s, isc_%dl, isc_%d, isc_%d);",
	request->req_handle,
	entree->entree_entree,
	entree->entree_entree,
	entree->entree_value);

}

static gen_menu_item_for (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ m e n u _ i t e m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate insert text for FOR_ITEM and PUT_ITEM
 *	for a dynamic menu.
 *
 **************************************/
ENTREE	entree;
REQ	request;

if (action->act_type != ACT_item_for)
    return;

/* Build stuff for item loop */

entree = (ENTREE) action->act_object;
request = entree->entree_request;

printa (column, "loop");
column += INDENT;
align (column);
printa (column,
    "interbase.get_entree (%s, isc_%dl, isc_%d, isc_%d, isc_%d);",
	request->req_handle,
	entree->entree_entree,
	entree->entree_entree,
	entree->entree_value,
	entree->entree_end);

printa (column, "exit when isc_%d /= 0;", entree->entree_end);

}

static gen_menu_request (
    REQ	request,
    int column)
{
/**************************************
 *
 *	g e n _ m e n u _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a dynamic menu request.
 *
 **************************************/
ACT		action;
MENU		menu;
ENTREE		entree;

menu = NULL;
entree = NULL;

for (action = request->req_actions; action; action = action->act_next)
    {
    if (action->act_type == ACT_menu_for)
	{
	menu = (MENU) action->act_object;
	break;
	} 
    else if (action->act_type == ACT_item_for || action->act_type == ACT_item_put)
	{
	entree = (ENTREE) action->act_object;
	break;
	}
    }

if (menu)
    {
    menu->menu_title = CMP_next_ident();
    menu->menu_terminator = CMP_next_ident();
    menu->menu_entree_value = CMP_next_ident();
    menu->menu_entree_entree = CMP_next_ident();
    printa (column, "isc_%dl\t: %s;\t\t-- TITLE_LENGTH --",
		menu->menu_title, USHORT_DCL);
    printa (column, "isc_%d\t: string (1..81);\t\t-- TITLE_TEXT --",
		menu->menu_title);
    printa (column, "isc_%d\t: %s;\t\t-- TERMINATOR --",
		menu->menu_terminator, USHORT_DCL);
    printa (column, "isc_%dl\t: %s;\t\t-- ENTREE_LENGTH --",
		menu->menu_entree_entree, USHORT_DCL);
    printa (column, "isc_%d\t: string (1..81);\t\t-- ENTREE_TEXT --",
		menu->menu_entree_entree);
    printa (column, "isc_%d\t: %s;\t\t-- ENTREE_VALUE --",
		menu->menu_entree_value, LONG_DCL);
    }

if (entree)
    {
    entree->entree_entree = CMP_next_ident();
    entree->entree_value = CMP_next_ident();
    entree->entree_end = CMP_next_ident();
    printa (column, "isc_%dl\t: %s;\t\t-- ENTREE_LENGTH --",
		entree->entree_entree, USHORT_DCL);
    printa (column, "isc_%d\t: string (1..81);\t\t-- ENTREE_TEXT --",
		entree->entree_entree);
    printa (column, "isc_%d\t: %s;\t\t-- ENTREE_VALUE --",
		entree->entree_value, LONG_DCL);
    printa (column, "isc_%d\t: %s;\t\t-- --",
		entree->entree_end, USHORT_DCL);
    }

}

static TEXT *gen_name (
    TEXT	*string,
    REF		reference,
    BOOLEAN     as_blob)
{
/**************************************
 *
 *	g e n _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name for a reference.  Name is constructed from
 *	port and parameter idents.
 *
 **************************************/

if (reference->ref_field->fld_array_info && !as_blob)
    sprintf (string, "isc_%d",
	reference->ref_field->fld_array_info->ary_ident);
else
    sprintf (string, "isc_%d.isc_%d", 
	reference->ref_port->por_ident, 
	reference->ref_ident);

return string;
}

static gen_on_error (
    ACT		action,
    USHORT	column)
{
/**************************************
 *
 *	g e n _ o n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Generate a block to handle errors.
 *
 **************************************/
ACT err_action;

err_action = (ACT) action->act_object;
if ((err_action->act_type == ACT_get_segment) ||
    (err_action->act_type == ACT_put_segment) ||
    (err_action->act_type == ACT_endblob))
    printa (column, "if (isc_status (1) /= 0) and (isc_status(1) /= interbase.isc_segment) and (isc_status(1) /= interbase.isc_segstr_eof) then");
else
    printa (column, "if (isc_status (1) /= 0) then");
}

static gen_procedure (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Generate code for an EXECUTE PROCEDURE.
 *
 **************************************/
PAT     args;
TEXT    *pattern;
REQ	request;
REF	reference;
POR	in_port, out_port;
TEXT	*status;
DBB	dbb;

request = action->act_request;
in_port = request->req_vport;
out_port = request->req_primary;

dbb = request->req_database;
args.pat_database = dbb;
args.pat_request = action->act_request;
args.pat_vector1 = status_vector (action);
args.pat_string2 = ada_null_address;
args.pat_request = request;
args.pat_port = in_port;
args.pat_port2 = out_port;
if (in_port && in_port->por_length)
    pattern = "interbase.gds_transact_request (%V1 %RF%DH%RE, %RF%RT%RE, %VF%RS%VE, %RF%RI%RE, %VF%PL%VE, %PI'address, %VF%QL%VE, %QI'address);\n";
else
    pattern = "interbase.gds_transact_request (%V1 %RF%DH%RE, %RF%RT%RE, %VF%RS%VE, %RF%RI%RE, %VF0%VE, %S2, %VF%QL%VE, %QI'address);\n";


/* Get database attach and transaction started */

if (sw_auto)
    t_start_auto (action, NULL_PTR, status_vector (action), column, TRUE);

/* Move in input values */

asgn_from (action, request->req_values, column);

/* Execute the procedure */

PATTERN_expand (column, pattern, &args);

SET_SQLCODE;

printa (column, "if SQLCODE = 0 then");

/* Move out output values */

asgn_to_proc (request->req_references, column);
ENDIF;
}

static gen_put_segment (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate the code to do a put segment.
 *
 **************************************/
BLB	blob;
REF	from;

if (action->act_flags & ACT_sql)
    {
    blob = (BLB) action->act_request->req_blobs;
    from = action->act_object;
    printa (column, "isc_%d := %s;",
	blob->blb_len_ident, from->ref_null_value);
    printa (column, "isc_%d := %s;",
	blob->blb_buff_ident, from->ref_value);
    }
else
    blob = (BLB) action->act_object;

printa (column,
    "interbase.ISC_PUT_SEGMENT (%s isc_%d, isc_%d, isc_%d'address);",
    status_vector (action),
    blob->blb_ident,
    blob->blb_len_ident,
    blob->blb_buff_ident);

SET_SQLCODE;
}

static gen_raw (
    UCHAR	*blr,
    enum req_t	request_type,
    int		request_length,
    int		column)
{
/**************************************
 *
 *	g e n _ r a w
 *
 **************************************
 *
 * Functional description
 *	Generate BLR in raw, numeric form.  Ughly but dense.
 *
 **************************************/
UCHAR	*end;
SCHAR	c;
TEXT	buffer[80], *p, *limit;

p = buffer;
limit = buffer + 60;

for (end = blr + request_length - 1; blr <= end; blr++)
    {
    c = *blr;
    sprintf (p, "%d", c);
    while (*p)
	p++;
    if (blr != end)
	*p++ = ',';
    if (p < limit)
	continue;
    *p = 0;
    printa (INDENT, buffer);
    p = buffer;
    }

*p = 0;
printa (INDENT, buffer);
}

static gen_ready (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for READY
 *
 **************************************/
RDY		ready;
DBB		db;
TEXT	*filename, *vector;

vector = status_vector (action);

for (ready = (RDY) action->act_object; ready; ready = ready->rdy_next)
    {
    db = ready->rdy_database;
    if (!(filename = ready->rdy_filename))
	filename = db->dbb_runtime;
    if ((action->act_error || (action->act_flags & ACT_sql)) &&
	ready != (RDY) action->act_object)
	printa (column, "if isc_status(1) = 0 then");
    make_ready (db, filename, vector, column, ready->rdy_request);
    if ((action->act_error || (action->act_flags & ACT_sql)) &&
	ready != (RDY) action->act_object)
	ENDIF;
    }
SET_SQLCODE;
}

static gen_receive (
    ACT		action,
    int		column,
    POR		port)
{
/**************************************
 *
 *	g e n _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Generate receive call for a port.
 *
 **************************************/
REQ	request;

request = action->act_request;
printa (column, "interbase.RECEIVE (%s %s, %d, %d, isc_%d'address, %s);",
    status_vector (action),
    request->req_handle,
    port->por_msg_number,
    port->por_length,
    port->por_ident,
    request->req_request_level);

SET_SQLCODE;
}

static gen_release (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for RELEASE_REQUESTS
 *      For active databases, call isc_release_request.
 *      for all others, just zero the handle.  For the
 *      release request calls, ignore error returns, which
 *      are likely if the request was compiled on a database
 *      which has been released and re-readied.  If there is
 *      a serious error, it will be caught on the next statement.
 *
 **************************************/
DBB	db, exp_db;
REQ	request;

exp_db = (DBB) action->act_object;

for (request = requests; request; request = request->req_next)
    {
    db = request->req_database;
    if (exp_db && db  != exp_db)
	continue;
    if (!(request->req_flags & REQ_exp_hand))
	{
	printa (column, "if %s%s /= 0 then", ada_package, db->dbb_name->sym_string);
	printa (column + INDENT, "interbase.release_request (isc_status, %s);",
		request->req_handle);
	printa (column, "end if;");
	printa (column, "%s := 0;", request->req_handle);
	}
    }
}

static gen_request (
    REQ		request,
    int		column)
{
/**************************************
 *
 *	g e n _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/
ACT	action;
UPD	modify;
REF     reference;
BLB	blob;
POR	port;
SSHORT	length;
TEXT	*string_type;

/* generate request handle, blob handles, and ports */

if (!(request->req_flags & 
      (REQ_exp_hand | REQ_menu_for_item | REQ_sql_blob_open |
       REQ_sql_blob_create)) &&
    request->req_type != REQ_slice &&
    request->req_type != REQ_procedure)
    {
    if (request->req_type == REQ_menu)
	printa (column, "%s\t: interbase.menu_handle := 0;-- menu handle --",
	    request->req_handle);
    else if (request->req_type == REQ_form)
	printa (column, "%s\t: interbase.map_handle := 0;-- map handle --",
	    request->req_handle);
    else
	printa (column, "%s\t: interbase.request_handle := 0;-- request handle --",
	    request->req_handle);
    }

if (request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
    printa (column, "isc_%do\t: %s;\t\t-- SQL CURSOR FLAG --",
	request->req_ident, SHORT_DCL);

/* check the case where we need to extend the dpb dynamically at runtime,
   in which case we need dpb length and a pointer to be defined even if 
   there is no static dpb defined */

if (request->req_flags & REQ_extend_dpb)
    {
    printa (column, "isc_%dp\t: interbase.dpb_ptr := 0;\t-- db parameter block --", request->req_ident);
    if (!request->req_length)
        printa (column, "isc_%dl\t: interbase.isc_ushort := 0;\t-- db parameter block --", request->req_ident);
    }

/* generate actual BLR string */

if (request->req_length)
    {
    length = request->req_length;
    switch (request->req_type)
	{
	case REQ_create_database:	
	    if (ada_flags & ADA_create_database)
		{
        	printa (column, "function isc_to_dpb_ptr is new unchecked_conversion (system.address, interbase.dpb_ptr);");
		ada_flags &= ~ADA_create_database;
		}
	    /* fall into ... */
	case REQ_ready:
            printa (column, "isc_%dl\t: interbase.isc_ushort := %d;\t-- db parameter block --", request->req_ident, length);
	    printa  (column, "isc_%d\t: CONSTANT interbase.dpb (1..%d) := (", 
		    request->req_ident, length);
		break;

	default:
	    if (request->req_flags & REQ_sql_cursor)
		printa (column, "isc_%do\t: %s;\t\t-- SQL CURSOR FLAG --",
		    request->req_ident, SHORT_DCL);
#ifdef SCROLLABLE_CURSORS
            if (request->req_flags & REQ_scroll)
	        printa (column, "isc_%ddirection\t: interbase.isc_ushort := 0;\t-- last direction sent to engine --", request->req_ident);
#endif
	    printa  (column, "isc_%d\t: CONSTANT interbase.blr (1..%d) := (", 
		    request->req_ident, length);
	}
    gen_raw (request->req_blr, request->req_type, request->req_length, column);
    printa  (column, ");\n");
    if (!sw_raw)
	{
	printa (column, "---");
	printa (column, "--- FORMATTED REQUEST BLR FOR GDS_%d = \n", request->req_ident);
	switch (request->req_type)
	    {
	    case REQ_create_database:	
	    case REQ_ready:
		string_type = "DPB";
		if (PRETTY_print_cdb (request->req_blr, gen_blr, 0, 0))
		    IBERROR ("internal error during parameter generation");
		break;
	
	    case REQ_ddl:	
		string_type = "DYN";
		if (PRETTY_print_dyn (request->req_blr, gen_blr, 0, 0))
		    IBERROR ("internal error during dynamic DDL generation");
		break;

	    case REQ_form:
		string_type = "form map";
		if (PRETTY_print_form_map (request->req_blr, gen_blr, 0, 0))
		    IBERROR ("internal error during form map generation");
		break;

	    case REQ_menu:
		string_type = "menu";
		if (PRETTY_print_menu (request->req_blr, gen_blr, 0, 1))
		    IBERROR ("internal error during menu generation");
		break;

	    case REQ_slice:
		string_type = "SDL";
		if (PRETTY_print_sdl (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during SDL generation");
		break;

	    default:
		string_type = "BLR";
		if (isc_print_blr (request->req_blr, gen_blr, 0, 0))
		    IBERROR ("internal error during BLR generation");
	    }
	}
    else
	{
	switch (request->req_type)
	    {
	    case REQ_create_database:	
	    case REQ_ready:
		string_type = "DPB";
		break;

	    case REQ_ddl:	
		string_type = "DYN";
		break;

	    case REQ_form:
		string_type = "form map";
		break;

	    case REQ_menu:
		string_type = "menu";
		break;

 	    case REQ_slice:
		string_type = "SDL";
 		break;

	    default:
		string_type = "BLR";
	    }
	}
    printa  (column, "\t-- end of %s string for request isc_%d --\n",
	    string_type, request->req_ident, request->req_length);
    }

/*  Print out slice description language if there are arrays associated with request  */
for (port = request->req_ports; port; port = port->por_next)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_sdl)
	    {
	    printa (column, "isc_%d\t: CONSTANT interbase.blr (1..%d) := (",
		    reference->ref_sdl_ident,
		    reference->ref_sdl_length);
	    gen_raw (reference->ref_sdl, REQ_slice, reference->ref_sdl_length,  column);
	    printa (column, "); \t--- end of SDL string for isc_%d\n",
		    reference->ref_sdl_ident);
	    if (!sw_raw)
		{
		printa (column, "---");
		printa (column, "--- FORMATTED REQUEST SDL FOR GDS_%d = \n", reference->ref_sdl_ident);
		if (PRETTY_print_sdl (reference->ref_sdl, gen_blr, NULL_PTR, 1))
		    IBERROR ("internal error during SDL generation");
		}
	    }

/* Print out any blob parameter blocks required */

for (blob = request->req_blobs; blob; blob = blob->blb_next)
    if (blob->blb_bpb_length)
	{
	printa (column, "isc_%d\t: CONSTANT interbase.blr (1..%d) := (",
		blob->blb_bpb_ident, blob->blb_bpb_length);
	gen_raw (blob->blb_bpb, request->req_type, blob->blb_bpb_length, column);
	printa (INDENT, ");\n");
	}

if (request->req_type == REQ_menu)
    gen_menu_request (request, column); 

/* If this is a GET_SLICE/PUT_slice, allocate some variables */

if (request->req_type == REQ_slice)
    {
    printa (INDENT, "isc_%ds\t: %s;\t\t",request->req_ident, LONG_DCL);
    printa (INDENT, "isc_%dv: %s (1..%d);", request->req_ident, 
	LONG_VECTOR_DCL, 
	MAX (request->req_slice->slc_parameters, 1));
    }
}

static gen_return_value (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ r e t u r n _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Generate receive call for a port
 *	in a store2 statement.
 *
 **************************************/
UPD	update;
REF	reference;
REQ	request;

request = action->act_request;
if (action->act_pair->act_error)
    column += INDENT;
align (column);

gen_start (action, request->req_primary, column);
update = (UPD) action->act_object;
reference = update->upd_references;
gen_receive (action, column, reference->ref_port);
}

static gen_routine (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ r o u t i n e
 *
 **************************************
 *
 * Functional description
 *	Process routine head.  If there are requests in the
 *	routine, insert local definitions.
 *
 **************************************/
BLB	blob;
REQ	request;
POR	port;
SSHORT	blob_subtype;
REF	reference;
FLD	field;

column += INDENT;

for (request = (REQ) action->act_object; request; request = request->req_routine)
    {
    for (port = request->req_ports; port; port = port->por_next)
	make_port (port, column);
    for (port = request->req_ports; port; port = port->por_next)
	printa  (column, "isc_%d\t: isc_%dt;\t\t\t-- message --", 
	    port->por_ident, 
	    port->por_ident);

    for (blob = request->req_blobs; blob; blob = blob->blb_next)
	{
	printa  (column, "isc_%d\t: interbase.blob_handle;\t-- blob handle --",
    	    blob->blb_ident);
	if (!(blob_subtype = blob->blb_const_to_type))
	    if (reference = blob->blb_reference)
		if (field = reference->ref_field)
		    blob_subtype = field->fld_sub_type;
	if (blob_subtype != 0 && blob_subtype != 1)
	    printa  (column, "isc_%d\t: %s (1..%d);\t-- blob segment --",
		blob->blb_buff_ident, BYTE_VECTOR_DCL, blob->blb_seg_length);
	else
	    printa  (column, "isc_%d\t: string (1..%d);\t-- blob segment --",
		blob->blb_buff_ident, blob->blb_seg_length);
	printa  (column, "isc_%d\t: %s;\t\t\t-- segment length --",
	    blob->blb_len_ident, USHORT_DCL);
	}
    }
column -= INDENT;
}

static gen_s_end (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STREAM.
 *
 **************************************/
REQ	request;

request = action->act_request;

if (action->act_type == ACT_close)
    {
    make_cursor_open_test (ACT_close, request, column);
    column += INDENT;
    }

printa (column, "interbase.UNWIND_REQUEST (%s %s, %s);", 
    status_vector (action),
    request->req_handle,
    request->req_request_level);

SET_SQLCODE;

if (action->act_type == ACT_close)
    {
    printa (column, "if (SQLCODE = 0) then");
    printa (column + INDENT, "isc_%do := 0;", request->req_ident);
    ENDIF;
    column -= INDENT;
    ENDIF;
    }
}

static gen_s_fetch (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FETCH.
 *
 **************************************/
REQ	request;

request = action->act_request;
if (request->req_sync)
    gen_send (action, request->req_sync, column);

gen_receive (action, column, request->req_primary);
}

static gen_s_start (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate text to compile and start a stream.  This is
 *	used both by START_STREAM and FOR
 *
 **************************************/
REQ	request;
POR	port;

request = action->act_request;
 
if ((action->act_type == ACT_open))
    {
    make_cursor_open_test (ACT_open, request, column);
    column += INDENT;
    }

gen_compile (action, column);

if (port = request->req_vport)
    asgn_from (action, port->por_references, column);

if (action->act_error || (action->act_flags & ACT_sql))
    {
    make_ok_test (action, request, column);
    gen_start (action, port, column + INDENT);
    ENDIF;
    }
else
    gen_start (action, port, column); 
 
if (action->act_type == ACT_open)
    {
    printa (column, "if (SQLCODE = 0) then");
    printa (column+INDENT, "isc_%do := 1;", request->req_ident);
    ENDIF;
    column -= INDENT;
    ENDIF;
    }
}

static gen_segment (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Substitute for a segment, segment length, or blob handle.
 *
 **************************************/
BLB	blob;

blob = (BLB) action->act_object;

printa (column, "isc_%d",
    (action->act_type == ACT_segment) ? blob->blb_buff_ident :
    (action->act_type == ACT_segment_length) ? blob->blb_len_ident :
    blob->blb_ident);
}

static gen_select (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s e l e c t
 *
 **************************************
 *
 * Functional description
 *	Generate code for standalone SELECT statement.
 *
 **************************************/
REQ	request;
POR	port;
NOD	var_list;
int	i;
TEXT	name[20];

request = action->act_request;
port = request->req_primary;
gen_name (name, request->req_eof, TRUE);

gen_s_start (action, column);
gen_receive (action, column, port);
printa (column, "if SQLCODE = 0 then", name);
column += INDENT;
printa (column, "if %s /= 0 then", name);
column += INDENT;

if (var_list = (NOD) action->act_object)
    for (i = 0; i < var_list->nod_count; i++)
	asgn_to (action, var_list->nod_arg [i], column);
if (request->req_database->dbb_flags & DBB_v3)
    {
    gen_receive (action, column, port);
    printa (column, "if (SQLCODE = 0) AND (%s /= 0) then", name);
    printa (column + INDENT, "SQLCODE := -1;");
    ENDIF;
    }

printa (column - INDENT, "else");
printa (column, "SQLCODE := 100;");
column -= INDENT;
ENDIF;
column -= INDENT;
ENDIF;
}

static gen_send (
    ACT		action,
    POR		port,
    int		column)
{
/**************************************
 *
 *	g e n _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Generate a send or receive call for a port.
 *
 **************************************/
REQ	request;

request = action->act_request;
printa (column, "interbase.SEND (%s %s, %d, %d, isc_%d'address, %s);",
    status_vector (action),
    request->req_handle,
    port->por_msg_number,
    port->por_length,
    port->por_ident,
    request->req_request_level);

SET_SQLCODE;
}

static gen_slice (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Generate support for get/put slice statement.
 *
 **************************************/
REQ	request, parent_request;
REF	reference, upper, lower;
SLC	slice;
PAT	args;
struct slc_repeat	*tail, *end;
TEXT    *pattern1 = "interbase.GET_SLICE (%V1 %RF%DH%RE, %RF%RT%RE, %RF%FR%RE, %N1, \
%I1, %N2, %I1v, %I1s, %RF%S5'address%RE, %RF%S6%RE);";
TEXT	*pattern2 = "interbase.PUT_SLICE (%V1 %RF%DH%RE, %RF%RT%RE, %RF%FR%RE, %N1, \
%I1, %N2, %I1v, %I1s, %RF%S5'address%RE);";

request = action->act_request;
slice = (SLC) action->act_object;
parent_request = slice->slc_parent_request;

/* Compute array size */

printa (column, "isc_%ds := %d", request->req_ident, slice->slc_field->fld_array->fld_length);

for (tail = slice->slc_rpt, end = tail + slice->slc_dimensions; tail < end; ++tail)
    if (tail->slc_upper != tail->slc_lower)
	{
	lower = (REF) tail->slc_lower->nod_arg [0];
	upper = (REF) tail->slc_upper->nod_arg [0];
	if (lower->ref_value)
	    ib_fprintf (out_file, " * ( %s - %s + 1)", upper->ref_value, lower->ref_value);
	else
	    ib_fprintf (out_file, " * ( %s + 1)", upper->ref_value);
	}

ib_fprintf (out_file, ";");

/* Make assignments to variable vector */

for (reference = request->req_values; reference; reference = reference->ref_next)
    printa (column, "isc_%dv (%d) := %s;", request->req_ident,
	    reference->ref_id + 1, reference->ref_value);

args.pat_reference = slice->slc_field_ref;
args.pat_request = parent_request;				/* blob id request */
args.pat_vector1 = status_vector (action);			/* status vector */
args.pat_database = parent_request->req_database;		/* database handle */
args.pat_value1 = request->req_length;				/* slice descr. length */
args.pat_ident1 = request->req_ident;				/* request name */
args.pat_value2 = slice->slc_parameters * sizeof (SLONG);	/* parameter length */

reference = (REF) slice->slc_array->nod_arg [0];
args.pat_string5 = reference->ref_value;			/* array name */
args.pat_string6 = "isc_array_length";

PATTERN_expand (column, (action->act_type == ACT_get_slice) ? pattern1 : pattern2, &args);
}

static gen_start (
    ACT	action,
    POR	port,
    int	column)
{
/**************************************
 *
 *	g e n _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate either a START or START_AND_SEND depending
 *	on whether or a not a port is present.
 *
 **************************************/
REQ	request;
TEXT	*vector;
REF     reference;

request = action->act_request;
vector = status_vector (action);
column += INDENT;

if (port)
    {
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_field->fld_array_info)
	    gen_get_or_put_slice (action, reference, FALSE, column);

    printa (column, "interbase.START_AND_SEND (%s %s, %s%s, %d, %d, isc_%d'address, %s);",
	vector,
	request->req_handle,
	ada_package, request_trans (action, request),
	port->por_msg_number,
	port->por_length,
	port->por_ident,
	request->req_request_level);
    }
else
    printa (column, "interbase.START_REQUEST (%s %s, %s%s, %s);",
	vector,
	request->req_handle,
	ada_package, request_trans (action, request),
	request->req_request_level);

SET_SQLCODE;
}

static gen_store (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate text for STORE statement.  This includes the compile
 *	call and any variable initialization required.
 *
 **************************************/
REQ	request;
REF	reference;
FLD	field;
POR	port;
TEXT	name[64];

request = action->act_request;
gen_compile (action, column);
if (action->act_error || (action->act_flags & ACT_sql))
    make_ok_test (action, request, column);

/* Initialize any blob fields */

port = request->req_primary;
for (reference = port->por_references; reference; 
     reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (field->fld_flags & FLD_blob)
	printa (column, "%s := interbase.null_blob;", gen_name (name, reference, TRUE));
    }
}

static gen_t_start (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ t _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for START_TRANSACTION.
 *
 **************************************/
DBB		db;
TRA		trans;
TPB		tpb;
SYM		symbol;
int		count;
TEXT		*filename;

/* for automatically generated transactions, and transactions that are
   explicitly started, but don't have any arguments so don't get a TPB,
   generate something plausible. */

if (!action || !(trans = (TRA) action->act_object))
    {
    t_start_auto (action, NULL_PTR, status_vector (action), column, FALSE);
    return;
    }  

/* build a complete statement, including tpb's.
   first generate any appropriate ready statements,
   and fill in the tpb vector (aka TEB). */

count = 0;
for (tpb = trans->tra_tpb; tpb; tpb = tpb->tpb_tra_next)
    {
    count++;
    db = tpb->tpb_database;
    if (sw_auto)
	if ((filename = db->dbb_runtime) ||
	    !(db->dbb_flags & DBB_sqlca))
	    {
	    printa (column, "if (%s%s = 0) then", ada_package, db->dbb_name->sym_string);
	    align (column + INDENT);
	    make_ready (db, filename, status_vector (action), column + INDENT, 0);
	    printa (column, "end if;");
	    }

    printa (column, "isc_teb(%d).tpb_len := %d;", 
	count, tpb->tpb_length);
    printa (column, "isc_teb(%d).tpb_ptr := isc_tpb_%d'address;", 
	count, tpb->tpb_ident);
    printa (column, "isc_teb(%d).dbb_ptr := %s%s'address;", 
	count, ada_package, db->dbb_name->sym_string);
    }

printa (column, "interbase.start_multiple (%s %s%s, %d, isc_teb'address);", 
    status_vector (action),
    ada_package, (trans->tra_handle) ? (SCHAR*) trans->tra_handle : "gds_trans",
    trans->tra_db_count);

SET_SQLCODE;
}

static gen_tpb (
    TPB	tpb,
    int	column)
{
/**************************************
 *
 *	g e n _ t p b
 *
 **************************************
 *
 * Functional description
 *	Generate a TPB in the output file
 *
 **************************************/
TEXT	*text, buffer[80], c, *p;
int	length;

printa (column, "isc_tpb_%d\t: CONSTANT interbase.tpb (1..%d) := (", 
	tpb->tpb_ident, tpb->tpb_length);

length = tpb->tpb_length;
text = (TEXT*) tpb->tpb_string;
p = buffer;

while (--length)
    {
    c = *text++;
    sprintf (p, "%d, ", c);
    while ( *p)
	p++;
    if (p - buffer > 60)
	{
	printa (column + INDENT, " %s", buffer);
	p = buffer;
	*p = 0;
	}
    }

/* handle the last character */

c = *text++;
sprintf (p, "%d", c);
printa (column + INDENT, "%s", buffer);
printa (column, ");");
}

static gen_trans (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ t r a n s
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for COMMIT, ROLLBACK, PREPARE, and SAVE
 *
 **************************************/

if (action->act_type == ACT_commit_retain_context)
    printa (column, "interbase.COMMIT_RETAINING (%s %s%s);",
	    status_vector (action),
	    ada_package,
	    (action->act_object) ? (TEXT*) action->act_object : "gds_trans");
else
    printa (column, "interbase.%s_TRANSACTION (%s %s%s);",
	    (action->act_type == ACT_commit) ? "COMMIT" :
		(action->act_type == ACT_rollback) ? "ROLLBACK" : "PREPARE",
	    status_vector (action),
	    ada_package,
	    (action->act_object) ? (TEXT*) action->act_object : "gds_trans");
	
SET_SQLCODE;
}


static int gen_type (
    ACT         action,
    int         column)
{
/**************************************
 *
 *      g e n _ t y p e
 *
 **************************************
 *
 * Functional description
 *      Substitute for a variable reference.
 *
 **************************************/

printa (column, "%ld", action->act_object);
}








static gen_update (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for UPDATE ... WHERE CURRENT OF ...
 *
 **************************************/
POR	port;
UPD	modify;

modify = (UPD) action->act_object;
port = modify->upd_port;
asgn_from (action, port->por_references, column);
gen_send (action, port, column);
}

static gen_variable (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ v a r i a b l e 
 *
 **************************************
 *
 * Functional description
 *	Substitute for a variable reference.
 *
 **************************************/
TEXT	s[20];

printa (column, gen_name (s, (REF) action->act_object, FALSE));
}

static gen_whenever (
    SWE		label,
    int		column)
{
/**************************************
 *
 *	g e n _ w h e n e v e r
 *
 **************************************
 *
 * Functional description
 *	Generate tests for any WHENEVER clauses that may have been declared.
 *
 **************************************/
TEXT	*condition;

while (label)
    {
    switch (label->swe_condition)
	{
	case SWE_error:
	    condition = "SQLCODE < 0";
	    break;

	case SWE_warning:
	    condition = "(SQLCODE > 0) AND (SQLCODE /= 100)";
	    break;

	case SWE_not_found:
	    condition = "SQLCODE = 100";
	    break;
	}
    printa (column, "if %s then goto %s; end if;", condition, label->swe_label);
    label = label->swe_next;
    }
}

static gen_window_create (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ w i n d o w _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Create a new window.
 *
 **************************************/

printa (column, "interbase.create_window (%sisc_window, 0, \" \", %sisc_width, %sisc_height)",
	ADA_WINDOW_PACKAGE, ADA_WINDOW_PACKAGE, ADA_WINDOW_PACKAGE);
}

static gen_window_delete (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ w i n d o w _ d e l e t e
 *
 **************************************
 *
 * Functional description
 *	Delete a window.
 *
 **************************************/

printa (column, "interbase.delete_window (%sisc_window)", ADA_WINDOW_PACKAGE);
}

static gen_window_suspend (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ w i n d o w _ s u s p e n d
 *
 **************************************
 *
 * Functional description
 *	Suspend a window.
 *
 **************************************/

printa (column, "interbase.suspend_window (%sisc_window)", ADA_WINDOW_PACKAGE);
}

static make_array_declaration (
    REF		reference,
    int		column)
{
/**************************************
 *
 *	m a k e _ a r r a y _ d e c l a r a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a declaration of an array in the
 *      output file.
 *
 **************************************/
FLD    field;
SCHAR   *name;
TEXT   s [64];
DIM    dimension;
int    i;

field = reference->ref_field;
name = field->fld_symbol->sym_string;

/* Don't generate multiple declarations for the array.  V3 Bug 569.  */

if (field->fld_array_info->ary_declared)
    return;

field->fld_array_info->ary_declared = TRUE;

if ((field->fld_array->fld_dtype <= dtype_varying) && (field->fld_array->fld_length != 1))
    {
    if (field->fld_array->fld_sub_type == 1)
	ib_fprintf (out_file, "subtype isc_%d_byte is %s(1..%d);\n",
		 field->fld_array_info->ary_ident,
		 BYTE_VECTOR_DCL,
		 field->fld_array->fld_length);
    else
	ib_fprintf (out_file, "subtype isc_%d_string is string(1..%d);\n",
	     field->fld_array_info->ary_ident,
	     field->fld_array->fld_length);
    }

ib_fprintf (out_file, "type isc_%dt is array (", field->fld_array_info->ary_ident);

/*  Print out the dimension part of the declaration  */
for (dimension = field->fld_array_info->ary_dimension, i = 1;
	i < field->fld_array_info->ary_dimension_count;
	    dimension = dimension->dim_next, i++)
    ib_fprintf (out_file, "%s range %d..%d,\n                        ",
		LONG_DCL,
		dimension->dim_lower,
		dimension->dim_upper);

ib_fprintf (out_file, "%s range %d..%d) of ",
	 LONG_DCL,
	 dimension->dim_lower,
	 dimension->dim_upper);

switch (field->fld_array_info->ary_dtype) 
    {
    case dtype_short:
	ib_fprintf (out_file, "%s", SHORT_DCL);
	break;

    case dtype_long:
	ib_fprintf (out_file, "%s", LONG_DCL);
	break;

    case dtype_cstring:
    case dtype_text:
    case dtype_varying:
	if (field->fld_array->fld_length == 1)
	    {
	    if (field->fld_array->fld_sub_type == 1)
		ib_fprintf (out_file, BYTE_DCL);
	    else
		ib_fprintf (out_file, "interbase.isc_character");
	    }
	else
	    {
	    if (field->fld_array->fld_sub_type == 1)
		ib_fprintf (out_file, "isc_%d_byte",
		     field->fld_array_info->ary_ident);
	    else
		ib_fprintf (out_file, "isc_%d_string",
		     field->fld_array_info->ary_ident);
	    }
	break;

    case dtype_date:
    case dtype_quad:
	ib_fprintf (out_file, "interbase.quad");
	break;
	    
    case dtype_float:
	ib_fprintf (out_file, "%s", FLOAT_DCL);
	break;

    case dtype_double:
	ib_fprintf (out_file, "%s", DOUBLE_DCL);
	break;

    default:
	printa (column, "datatype %d unknown for field %s",
		field->fld_array_info->ary_dtype, name);
	return;
    }

ib_fprintf (out_file, ";\n");

/*  Print out the database field  */

ib_fprintf (out_file, "isc_%d : isc_%dt;\t--- %s\n\n",
	field->fld_array_info->ary_ident,
	field->fld_array_info->ary_ident,
	name);
}

static make_cursor_open_test (
    enum act_t  type,
    REQ		request,
    int		column)
{
/**************************************
 *
 *	m a k e _ c u r s o r _ o p e n _ t e s t
 *
 **************************************
 *
 * Functional description
 *    Generate code to test existence 
 *    of open cursor and do the right thing:
 *    if type == ACT_open && isc_nl, error
 *    if type == ACT_close && !isc_nl, error
 *
 **************************************/

if (type == ACT_open)
    {
    printa (column, "if (isc_%do = 1) then", request->req_ident);
    printa (column + INDENT, "SQLCODE := -502;");
    }
else
    if (type == ACT_close)
	{
	printa (column, "if (isc_%do = 0) then", request->req_ident);
	printa (column + INDENT, "SQLCODE := -501;");
	}

printa (column, "else");
}

static TEXT *make_name (
    TEXT	*string,
    SYM		symbol)
{
/**************************************
 *
 *	m a k e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Turn a symbol into a varying string.
 *
 **************************************/

sprintf (string, "\"%s \"", symbol->sym_string);

return string;
}

static make_ok_test (
    ACT		action,
    REQ		request,
    int		column)
{
/**************************************
 *
 *	m a k e _ o k _ t e s t
 *
 **************************************
 *
 * Functional description
 *	Generate code to test existence of
 *	compiled request with active transaction.
 *
 **************************************/

if (sw_auto)
    printa (column, "if (%s%s /= 0) and (%s /= 0) then",
	ada_package, request_trans (action, request), request->req_handle);
else
    printa (column, "if (%s /= 0) then", request->req_handle);
}

static make_port (
    POR	port,
    int	column)
{
/**************************************
 *
 *	m a k e _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Insert a port record description in output.
 *
 **************************************/
FLD	field;
REF	reference;
SYM	symbol;
TEXT	*name, s [80];
int	pos;

printa (column, "type isc_%dt is record", port->por_ident);

for (reference = port->por_references; reference; 
    reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (symbol = field->fld_symbol)
	name = symbol->sym_string;
    else
	name = "<expression>";
    if (reference->ref_value && (reference->ref_flags & REF_array_elem))
	field = field->fld_array;
    switch (field->fld_dtype) 
	{
	case dtype_float:
	    printa (column + INDENT, "isc_%d\t: %s;\t-- %s --",
		reference->ref_ident, FLOAT_DCL, name);
	    break;

	case dtype_double:
	    printa (column + INDENT, "isc_%d\t: %s;\t-- %s --",
		reference->ref_ident, DOUBLE_DCL, name);
	    break;

	case dtype_short:
	    printa (column + INDENT, "isc_%d\t: %s;\t-- %s --",
		reference->ref_ident, SHORT_DCL, name);
	    break;

	case dtype_long:
	    printa (column + INDENT, "isc_%d\t: %s;\t-- %s --",
		reference->ref_ident, LONG_DCL, name);
	    break;

	case dtype_date:
	case dtype_quad:
	case dtype_blob:
	    printa (column + INDENT, "isc_%d\t: interbase.quad;\t-- %s --",
		reference->ref_ident, name);
	    break;
	    
	case dtype_text:
	    if (strcmp(name, "isc_slack"))
		{
		if (field->fld_sub_type == 1)
		    {
		    if (field->fld_length == 1)
			printa (column + INDENT, "isc_%d\t: %s;\t-- %s --",
			    reference->ref_ident, BYTE_DCL, name);
		    else
			printa (column + INDENT, "isc_%d\t: %s (1..%d);\t-- %s --",
			    reference->ref_ident, BYTE_VECTOR_DCL, field->fld_length, name);
		    }
		else
		    {
		    if (field->fld_length == 1)
			printa (column + INDENT, "isc_%d\t: interbase.isc_character;\t-- %s --",
			    reference->ref_ident, name);
		    else
			printa (column + INDENT, "isc_%d\t: string (1..%d);\t-- %s --",
			    reference->ref_ident, field->fld_length, name);
		    }
		}
	    break;

	default:
	    sprintf (s, "datatype %d unknown for field %s, msg %d",
		field->fld_dtype, name, port->por_msg_number);
	    IBERROR (s);
	    return;
	}
    }

printa (column, "end record;");

printa (column, "for isc_%dt use record at mod 4;", port->por_ident);

pos = 0;
for (reference = port->por_references; reference; 
    reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (reference->ref_value && field->fld_array_info)
	field = field->fld_array;
    printa (column + INDENT, "isc_%d at %d range 0..%d;",
	    reference->ref_ident, pos, 8 * field->fld_length - 1);
    pos += field->fld_length;
    }

printa (column, "end record;");
}

static make_ready (
    DBB		db,
    TEXT	*filename,
    TEXT	*vector,
    USHORT	column,
    REQ		request)
{
/**************************************
 *
 *	m a k e _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Generate the actual insertion text for a
 *      ready;
 *
 **************************************/
TEXT	s1 [32], s2 [32];

if (request)
    {
    sprintf (s1, "isc_%dl", request->req_ident);
    sprintf (s2, "isc_%d", request->req_ident);

    /* if the dpb needs to be extended at runtime to include items
       in host variables, do so here; this assumes that there is 
       always a request generated for runtime variables */

    if (request->req_flags & REQ_extend_dpb)
	{
    	sprintf (s2, "isc_%dp", request->req_ident);
	if (request->req_length)
	    printa (column, "%s = isc_%d;", s2, request->req_ident);
    	if (db->dbb_r_user)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_user_name, %s, %d);\n",
	    	s2, s1, db->dbb_r_user,  (strlen(db->dbb_r_user) -2) );
    	if (db->dbb_r_password)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_password, %s, %d);\n",
	    	s2, s1, db->dbb_r_password,  (strlen(db->dbb_r_password)-2) );
	/*
	** ===========================================================
	** ==
	** ==   Process Role Name
	** ==
	** ===========================================================
	*/
    	if (db->dbb_r_sql_role)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_sql_role_name, %s, %d);\n",
	    	s2, s1, db->dbb_r_sql_role,  (strlen(db->dbb_r_sql_role)-2) );

    	if (db->dbb_r_lc_messages)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_lc_messages, %s, %d);\n",
	    	s2, s1, db->dbb_r_lc_messages,  ( strlen(db->dbb_r_lc_messages) -2) );
    	if (db->dbb_r_lc_ctype)
       	   printa (column, 
		"interbase.MODIFY_DPB (%s, %s, interbase.isc_dpb_lc_ctype, %s, %d);\n",
	    	s2, s1, db->dbb_r_lc_ctype,  ( strlen(db->dbb_r_lc_ctype) -2) );
	}
    }

if (filename)
    if (*filename == '"')
	printa (column, "interbase.ATTACH_DATABASE (%s %d, %s, %s%s, %s, %s);",
	    vector,
	    strlen (filename) - 2,
	    filename,
	    ada_package, db->dbb_name->sym_string,
	    (request? s1 :  "0"),
	    (request? s2 : "interbase.null_dpb"));
    else
	printa (column, "interbase.ATTACH_DATABASE (%s %s'length(1), %s, %s%s, %s, %s);",
	    vector,
	    filename,
	    filename,
	    ada_package, db->dbb_name->sym_string,
	    (request? s1 : "0"),
	    (request? s2 : "interbase.null_dpb"));
else
    printa (column, "interbase.ATTACH_DATABASE (%s %d, \"%s\", %s%s, %s, %s);",
	vector,
	strlen (db->dbb_filename),
	db->dbb_filename,
	ada_package, db->dbb_name->sym_string,
	    (request? s1 : "0"),
	    (request? s2 : "interbase.null_dpb"));

if (request && request->req_flags & REQ_extend_dpb)
    {
    if (request->req_length)
	printa (column, "if (%s != isc_%d)", s2, request->req_ident);
    printa (column + (request->req_length ? 4 : 0), 
				"interbase.FREE (%s);", s2);

    /* reset the length of the dpb */
    if (request->req_length)
    printa (column, "%s = %d;", s1, request->req_length);
    }
}

static printa (
    int		column,
    TEXT	*string,
    ...)
{
/**************************************
 *
 *	p r i n t a
 *
 **************************************
 *
 * Functional description
 *	Print a fixed string at a particular column.
 *
 **************************************/
va_list	ptr;

VA_START (ptr, string);
align (column);
vsprintf (output_buffer, string, ptr);
ADA_print_buffer (output_buffer, column);
}

static TEXT *request_trans (
    ACT		action,
    REQ		request)
{
/**************************************
 *
 *	r e q u e s t _ t r a n s
 *
 **************************************
 *
 * Functional description
 *	Generate the appropriate transaction handle.
 *
 **************************************/
SCHAR	*trname;

if (action->act_type == ACT_open)
    {
    if (!(trname = ((OPN) action->act_object)->opn_trans))
	trname = "gds_trans";
    return trname;
    }
else
    return (request) ? request->req_trans : "gds_trans";
}

static TEXT *status_vector (
    ACT		action)
{
/**************************************
 *
 *	s t a t u s _ v e c t o r
 *
 **************************************
 *
 * Functional description
 *	Generate the appropriate status vector parameter for a gds
 *	call depending on where or not the action has an error clause.
 *
 **************************************/

if (action && 
    (action->act_error || (action->act_flags & ACT_sql)))
    return "isc_status,";

return "";
}

static t_start_auto (
    ACT 	action,
    REQ		request,
    TEXT	*vector,
    int		column,
    SSHORT	test)
{
/**************************************
 *
 *	t _ s t a r t _ a u t o
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for START_TRANSACTION.
 *
 **************************************/
DBB	db;
int	count, stat;
TEXT    *filename, buffer [256], temp [40], *trname;

trname = request_trans (action, request);

stat = !strcmp (vector, "isc_status");

BEGIN;

if (sw_auto)
    {
    buffer [0] = 0;
    for (count = 0, db = isc_databases; db; db = db->dbb_next, count++)
	if ((filename = db->dbb_runtime) ||
	    !(db->dbb_flags & DBB_sqlca))
	    {
	    align (column);
	    ib_fprintf (out_file, "if (%s%s = 0", ada_package, db->dbb_name->sym_string);
	    if (stat && buffer [0])
		ib_fprintf (out_file, "and %s(1) = 0", vector);
	    ib_fprintf (out_file, ") then");
	    make_ready (db, filename, vector, column + INDENT, 0);
	    printa (column, "end if;");
	    if (buffer [0])
		strcat (buffer, ") and (");
	    sprintf (temp, "%s%s /= 0", ada_package, db->dbb_name->sym_string);
	    strcat (buffer, temp);
	    }
    if (!buffer [0])
	strcpy (buffer, "True");
    if (test)
	printa (column, "if (%s) and (%s%s = 0) then", buffer, ada_package, trname);
    else
	printa (column, "if (%s) then", buffer);
    column += INDENT;
    }

for (count = 0, db = isc_databases; db; db = db->dbb_next)
    {
    count++;
    printa (column, "isc_teb(%d).tpb_len:= 0;", count);
    printa (column, "isc_teb(%d).tpb_ptr := interbase.null_tpb'address;", count);
    printa (column, "isc_teb(%d).dbb_ptr := %s%s'address;", count, ada_package, db->dbb_name->sym_string);
    }

printa (column, "interbase.start_multiple (%s %s%s, %d, isc_teb'address);", 
	vector, ada_package, trname, count);

if (sw_auto)
    {
    ENDIF;
    column -= INDENT;
    }

END;
}
