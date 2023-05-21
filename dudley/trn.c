/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		trn.c
 *	DESCRIPTION:	Translate meta-data change to dynamic meta data
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
#include "../dudley/ddl.h"
#include "../jrd/license.h"
#include "../dudley/gener_proto.h"
#include "../dudley/lex_proto.h"
#include "../dudley/trn_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/gdsassert.h"

static void	add_dimensions (STR, FLD);
static void	add_field (STR, FLD, REL);
static void	add_files (STR, FIL, DBB);
static void	add_filter (STR, FILTER);
static void	add_function (STR, FUNC);
static void	add_function_arg (STR, FUNCARG);
static void	add_generator (STR, SYM);
static void	add_global_field (STR, FLD);
static void	add_index (STR, DUDLEY_IDX);
static void	add_relation (STR, REL);
static void	add_security_class (STR, SCL);
static void	add_trigger (STR, TRG);
static void	add_trigger_msg (STR, TRGMSG);
static void	add_view (STR, REL);
static void	drop_field (STR, FLD);
static void	drop_filter (STR, FILTER);
static void	drop_function (STR, FUNC);
static void	drop_global_field (STR, FLD);
static void	drop_index (STR, DUDLEY_IDX);
static void	drop_relation (STR, REL);
static void	drop_security_class (STR, SCL);
static void	drop_shadow (STR, SLONG);
static void	drop_trigger (STR, TRG);
static void	drop_trigger_msg (STR, TRGMSG);
static int	gen_dyn_c (int *, int, SCHAR *);
static int	gen_dyn_cxx (int *, int, SCHAR *);
static int	gen_dyn_pas (int *, int, SCHAR *);
static void	modify_database (STR, DBB);
static void	modify_field (STR, FLD, REL);
static void	modify_global_field (STR, FLD);
static void	modify_index (STR, DUDLEY_IDX);
static void	modify_relation (STR, REL);
static void	modify_trigger (STR, TRG);
static void	modify_trigger_msg (STR, TRGMSG);
static void	put_acl (STR, UCHAR, SCL);
static void	put_blr (STR, UCHAR, REL, NOD);
static void	put_number (STR, TEXT, SSHORT);
static void	put_query_header (STR, TEXT, NOD);
static void	put_symbol (STR, TEXT, SYM);
static void	put_text (STR, UCHAR, TXT);
static void	raw_ada (STR);
static void	raw_basic (STR);
static void	raw_cobol (STR);
static void	raw_ftn (STR);
static void	raw_pli (STR);

static IB_FILE	*output_file;

#define STUFF(c)	*dyn->str_current++ = c
#define STUFF_WORD(c)	{STUFF (c); STUFF (c >> 8);}
#define CHECK_DYN(l)	{if ( !(dyn->str_current - dyn->str_start + l <= \
                                dyn->str_length) && !TRN_get_buffer (dyn, l) ) \
                             DDL_error_abort ( NULL, 320, NULL, NULL, \
                                               NULL, NULL, NULL );}

#ifdef mpexl
#define FOPEN_WRITE_TYPE	"w Ds2 V E32 S256000"
#endif

#ifndef FOPEN_WRITE_TYPE
#define FOPEN_WRITE_TYPE	"w"
#endif

void TRN_translate (void)
{
/**************************************
 *
 *	T R N _ t r a n s l a t e
 *
 **************************************
 *
 * Functional description
 *	Translate from internal data structures into dynamic DDL.
 *
 **************************************/
ACT	action, temp;
TEXT	*p;
USHORT	length;
struct str d, *dyn;

/* Start by reversing the set of actions */
dyn = &d;
dyn->str_current = dyn->str_start = (SCHAR*) gds__alloc ((SLONG) 8192);
dyn->str_length = 8192;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
gds_alloc_flag_unfreed (dyn->str_start);
#endif

if (!DYN_file_name[0])
    output_file = ib_stdout;
else if (!(output_file = ib_fopen (DYN_file_name, FOPEN_WRITE_TYPE)))
    {
    DDL_msg_put (281, DYN_file_name, NULL, NULL, NULL, NULL); /* msg 281: gdef: can't open DYN output file: %s */
    DDL_exit (FINI_ERROR);
    }

CHECK_DYN(2);
STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);

for (action = DDL_actions; action; action = action->act_next)
    if (!(action->act_flags & ACT_ignore))
	switch (action->act_type)
	    {
	    case act_c_database:
	    case act_m_database:
		modify_database (dyn, action->act_object);
		break;

	    case act_a_relation:
		add_relation (dyn, action->act_object);
		break;

	    case act_m_relation:
		modify_relation (dyn, action->act_object);
		break;

	    case act_d_relation:
		drop_relation (dyn, action->act_object);
		break;

	    case act_a_field:
		add_field (dyn, action->act_object, NULL);
		break;

	    case act_m_field:
		modify_field (dyn, action->act_object, NULL);
		break;

 	    case act_d_field:
		drop_field (dyn, action->act_object);
		break;

	    case act_a_filter:
		add_filter (dyn, action->act_object);
		break;

 	    case act_d_filter:
		drop_filter (dyn, action->act_object);
		break;

	    case act_a_function:
		add_function (dyn, action->act_object);
		break;

	    case act_a_function_arg:
		add_function_arg (dyn, action->act_object);
		break;

 	    case act_d_function:
		drop_function (dyn, action->act_object);
		break;

	    case act_a_generator:
		add_generator (dyn, action->act_object);
		break;

	    case act_a_gfield:
		add_global_field (dyn, action->act_object);
		break;

	    case act_m_gfield:
		modify_global_field (dyn, action->act_object);
		break;

	    case act_d_gfield:
		drop_global_field (dyn, action->act_object);
		break;

	    case act_a_index:
		add_index (dyn, action->act_object);
		break;

	    case act_m_index:
		modify_index (dyn, action->act_object);
		break;

	    case act_d_index:
		drop_index (dyn, action->act_object);
		break;

	    case act_a_security:
		add_security_class (dyn, action->act_object);
		break;

	    case act_d_security:
		drop_security_class (dyn, action->act_object);
		break;

	    case act_a_trigger:
		add_trigger (dyn, action->act_object);
		break;

	    case act_d_trigger:
		drop_trigger (dyn, action->act_object);
		break;

	    case act_m_trigger:
		modify_trigger (dyn, action->act_object);
		break;

	    case act_a_trigger_msg:
		add_trigger_msg (dyn, action->act_object);
		break;

	    case act_d_trigger_msg:
		drop_trigger_msg (dyn, action->act_object);
		break;

	    case act_a_shadow:
		add_files (dyn, action->act_object, NULL);
		break;

	    case act_d_shadow:
		drop_shadow (dyn, (SLONG)(action->act_object));
		break;

	    case act_m_trigger_msg:
		modify_trigger_msg (dyn, action->act_object);
		break;

	    default:
		DDL_err (282, NULL, NULL, NULL, NULL, NULL); /* msg 282: action not implemented yet */
	    }

CHECK_DYN(2);
STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

switch (language)
    {
    case lan_undef:
    case lan_c:
        if (PRETTY_print_dyn (dyn->str_start, gen_dyn_c, NULL_PTR, 0))
            DDL_err (283, NULL, NULL, NULL, NULL, NULL); /*msg 283: internal error during DYN pretty print */
        break;

    case lan_cxx:
        if (PRETTY_print_dyn (dyn->str_start, gen_dyn_cxx, NULL_PTR, 0))
            DDL_err (283, NULL, NULL, NULL, NULL, NULL); /*msg 283: internal error during DYN pretty print */
        break;

    case lan_pascal:
        length = dyn->str_current - dyn->str_start;
#ifdef apollo
        ib_fprintf (output_file, "   gds__dyn_length	: integer16 := %d;\n", length);
        ib_fprintf (output_file, "   gds__dyn	: array [1..%d] of char := [\n", length);
        if (PRETTY_print_dyn (dyn->str_start, gen_dyn_pas, NULL_PTR, 1))
            DDL_err (243, NULL, NULL, NULL, NULL, NULL); /*msg 284: internal error during DYN pretty print */
        ib_fprintf (output_file, "   ];	(* end of DYN string *)\n");
#else
        ib_fprintf (output_file, "   gds__dyn_length	: gds__short := %d;\n", length);
        ib_fprintf (output_file, "   gds__dyn	: packed array [1..%d] of char := (\n", length);
        if (PRETTY_print_dyn (dyn->str_start, gen_dyn_pas, NULL_PTR, 1))
            DDL_err (285, NULL, NULL, NULL, NULL, NULL); /*msg 285: internal error during DYN pretty print */
        ib_fprintf (output_file, "   );	(* end of DYN string *)\n");
#endif
        break;

    case lan_fortran:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file, "       INTEGER*2 GDS__DYN_LENGTH\n");
        ib_fprintf (output_file, "       INTEGER*4 GDS__DYN(%d)\n", (int) ((length + 3) / 4));
        ib_fprintf (output_file, "       DATA GDS__DYN_LENGTH  /%d/\n", length);
        ib_fprintf (output_file, "       DATA (GDS__DYN(I), I=1,%d)  /\n", (int) ((length + 3) / 4));
        raw_ftn (dyn);
        break;

    case lan_pli:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file,
          "DECLARE gds__dyn_length FIXED BINARY (15) STATIC INITIAL (%d);\n",
          length);
        ib_fprintf (output_file,
          "DECLARE gds__dyn (%d) FIXED BINARY (7) STATIC INITIAL (\n",
          length);
        raw_pli (dyn);
        break;

    case lan_basic:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file,
          "       DECLARE WORD CONSTANT gds__dyn_length = %d\n",
          length);
        ib_fprintf (output_file,
          "       DECLARE STRING CONSTANT gds__dyn =&\n",
          length);
        raw_basic (dyn);
        break;

    case lan_ada:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file,
          "gds_dyn_length: short_integer := %d;\n",
          length);
        ib_fprintf (output_file,
          "gds_dyn	: CONSTANT interbase.blr (1..%d) := (\n",
          length);
        raw_ada (dyn);
        break;

    case lan_ansi_cobol:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file,
          "       01  GDS-DYN-LENGTH PIC S9(4) USAGE COMP VALUE IS %d.\n",
          length);
        ib_fprintf (output_file, "       01  GDS-DYN.\n");
        raw_cobol (dyn);
        break;
    case lan_cobol:
        length = dyn->str_current - dyn->str_start;
        ib_fprintf (output_file,
          "       01  GDS__DYN_LENGTH PIC S9(4) USAGE COMP VALUE IS %d.\n",
          length);
        ib_fprintf (output_file, "       01  GDS__DYN.\n");
        raw_cobol (dyn);
        break;
    }

if (output_file != ib_stdout)
    ib_fclose (output_file);

gds__free (dyn->str_start);
}  

static void add_dimensions (
    STR		dyn,
    FLD		field)
{
/**************************************
 *
 *	a d d _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create dimensions. 
 *	First get rid of any old ones.
 *
 **************************************/
SLONG	*range;
USHORT	n;

put_symbol (dyn, gds__dyn_delete_dimensions, field->fld_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);

for (range = field->fld_ranges, n = 0; n < field->fld_dimension; range += 2, ++n)
    {
    put_number (dyn, gds__dyn_def_dimension, n);
    put_symbol (dyn, gds__dyn_fld_name, field->fld_name);
    put_number (dyn, gds__dyn_dim_lower, (SSHORT)range[0]);
    put_number (dyn, gds__dyn_dim_upper, (SSHORT)range[1]);
    CHECK_DYN(1);
    STUFF (gds__dyn_end);
    } 
}

static void add_field (
    STR		dyn,
    FLD		field,
    REL		view)
{
/**************************************
 *
 *	a d d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a 
 *	local field.
 *
 **************************************/
SYM	name, symbol;
REL	relation;
FLD	source_field;
int	n;

name = field->fld_name;
if (view)
    relation = view;
else
    relation = field->fld_relation;

put_symbol (dyn, gds__dyn_def_local_fld, name);
put_symbol (dyn, gds__dyn_rel_name, relation->rel_name);

if ((symbol = field->fld_source) &&
    strcmp (symbol->sym_string, name->sym_string) &&
    !field->fld_computed)
    put_symbol (dyn, gds__dyn_fld_source, symbol);

put_symbol (dyn, gds__dyn_security_class, field->fld_security_class);
put_symbol (dyn, gds__dyn_fld_edit_string, field->fld_edit_string);
put_symbol (dyn, gds__dyn_fld_query_name, field->fld_query_name);
put_query_header (dyn, gds__dyn_fld_query_header, field->fld_query_header);

if (field->fld_system)
    put_number (dyn, gds__dyn_system_flag, field->fld_system);

put_symbol (dyn, gds__dyn_fld_base_fld, field->fld_base);

if (field->fld_context)
    put_number (dyn, gds__dyn_view_context, field->fld_context->ctx_context_id);

if (field->fld_computed)
    {
    if (!field->fld_context && view)
        put_number (dyn, gds__dyn_view_context, 0);
    put_blr (dyn, gds__dyn_fld_computed_blr, NULL, field->fld_computed);
    source_field = field->fld_source_field;
    put_number (dyn, gds__dyn_fld_type, source_field->fld_dtype);
    put_number (dyn, gds__dyn_fld_length, source_field->fld_length);
    put_number (dyn, gds__dyn_fld_scale, source_field->fld_scale);
    put_number (dyn, gds__dyn_fld_sub_type, source_field->fld_sub_type);
    if (n = source_field->fld_segment_length)
        put_number (dyn, gds__dyn_fld_segment_length, n);
    }

put_text (dyn, gds__dyn_fld_computed_source, field->fld_compute_src);

if (field->fld_flags & fld_explicit_position)
    put_number (dyn, gds__dyn_fld_position, field->fld_position);

put_text (dyn, gds__dyn_description, field->fld_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_files (
    STR		dyn,
    FIL		files,
    DBB		database)
{
/**************************************
 *
 *	a d d _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add database
 *	or shadow files.
 *
 **************************************/
FIL	file, next;

/* Reverse the order of files (parser left them backwards) */

for (file = files, files = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = files;
    files = file;
    }

if (!database && files)
    put_number (dyn, gds__dyn_def_shadow, files->fil_shadow_number);

for (file = files; file; file = file->fil_next)
    {
    put_symbol (dyn, gds__dyn_def_file, file->fil_name);
    put_number (dyn, gds__dyn_file_start, (SSHORT)(file->fil_start));
    put_number (dyn, gds__dyn_file_length, (SSHORT)(file->fil_length));
    put_number (dyn, gds__dyn_shadow_man_auto, file->fil_manual);
    put_number (dyn, gds__dyn_shadow_conditional, file->fil_conditional);
    CHECK_DYN(1);
    STUFF (gds__dyn_end);
    }
if (!database && files)
    {
    CHECK_DYN(1);
    STUFF (gds__dyn_end);
    }
}

static void add_filter (
    STR		dyn,
    FILTER	filter)
{
/**************************************
 *
 *	a d d _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a 
 *	blob filter.
 *
 **************************************/

put_symbol (dyn, gds__dyn_def_filter, filter->filter_name);
put_number (dyn, gds__dyn_filter_in_subtype, filter->filter_input_sub_type);
put_number (dyn, gds__dyn_filter_out_subtype, filter->filter_output_sub_type);
put_symbol (dyn, gds__dyn_func_entry_point, filter->filter_entry_point);
put_symbol (dyn, gds__dyn_func_module_name, filter->filter_module_name);
put_text (dyn, gds__dyn_description, filter->filter_description);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_function (
    STR		dyn,
    FUNC	function)
{
/**************************************
 *
 *	a d d _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a 
 *	user define function.
 *
 **************************************/

put_symbol (dyn, gds__dyn_def_function, function->func_name);
put_symbol (dyn, gds__dyn_fld_query_name, function->func_query_name);
put_symbol (dyn, gds__dyn_func_entry_point, function->func_entry_point);
put_number (dyn, gds__dyn_func_return_argument, function->func_return_arg);
put_symbol (dyn, gds__dyn_func_module_name, function->func_module_name);
put_text (dyn, gds__dyn_description, function->func_description);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_function_arg (
    STR		dyn,
    FUNCARG	func_arg)
{
/**************************************
 *
 *	a d d _ f u n c t i o n _ a r g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a 
 *	user defined function.
 *
 **************************************/

put_number (dyn, gds__dyn_def_function_arg, func_arg->funcarg_position);
put_symbol (dyn, gds__dyn_function_name, func_arg->funcarg_funcname);
put_number (dyn, gds__dyn_func_mechanism, func_arg->funcarg_mechanism);
put_number (dyn, gds__dyn_fld_type, func_arg->funcarg_dtype);
put_number (dyn, gds__dyn_fld_scale, func_arg->funcarg_scale);
if (func_arg->funcarg_has_sub_type)
    put_number (dyn, gds__dyn_fld_sub_type, func_arg->funcarg_sub_type);
put_number (dyn, gds__dyn_fld_length, func_arg->funcarg_length);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_generator (
    STR		dyn,
    SYM		symbol)
{
/**************************************
 *
 *	a d d _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a 
 *	generator.
 *
 **************************************/

put_symbol (dyn, gds__dyn_def_generator, symbol);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_global_field (
    STR		dyn,
    FLD		field)
{
/**************************************
 *
 *	a d d _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a relation.
 *
 **************************************/
SYM	name, symbol;
DSC	desc;
USHORT	dtype;
int	n;

if (field->fld_computed)
    return;

name = field->fld_name;

put_symbol (dyn, gds__dyn_def_global_fld, name);
put_symbol (dyn, gds__dyn_fld_edit_string, field->fld_edit_string);
put_symbol (dyn, gds__dyn_fld_query_name, field->fld_query_name);
put_query_header (dyn, gds__dyn_fld_query_header, field->fld_query_header);

if (field->fld_system)
    put_number (dyn, gds__dyn_system_flag, field->fld_system);

put_number (dyn, gds__dyn_fld_type, field->fld_dtype);
put_number (dyn, gds__dyn_fld_length, field->fld_length);
put_number (dyn, gds__dyn_fld_scale, field->fld_scale);
put_number (dyn, gds__dyn_fld_sub_type, field->fld_sub_type);

if (n = field->fld_segment_length)
    put_number (dyn, gds__dyn_fld_segment_length, n);

put_blr (dyn, gds__dyn_fld_missing_value, NULL, field->fld_missing);

put_blr (dyn, gds__dyn_fld_validation_blr, NULL, field->fld_validation);
put_text (dyn, gds__dyn_fld_validation_source, field->fld_valid_src);
put_text (dyn, gds__dyn_description, field->fld_description);

if (field->fld_dimension)
    {
    put_number (dyn, gds__dyn_fld_dimensions, field->fld_dimension);
    add_dimensions (dyn, field);
    }

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_index (
    STR		dyn,
    DUDLEY_IDX		index)
{
/**************************************
 *
 *	a d d _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create an index.
 *
 **************************************/
TEXT	i;

put_symbol (dyn, gds__dyn_def_idx, index->idx_name);
put_symbol (dyn, gds__dyn_rel_name, index->idx_relation);
put_number (dyn, gds__dyn_idx_unique, index->idx_unique);
if (index->idx_inactive)
    put_number (dyn, gds__dyn_idx_inactive, TRUE);
else
    put_number (dyn, gds__dyn_idx_inactive, FALSE);
if (index->idx_type)
    put_number (dyn, gds__dyn_idx_type, index->idx_type);

put_text (dyn, gds__dyn_description, index->idx_description);

for (i = 0; i < index->idx_count; i++)
    put_symbol (dyn, gds__dyn_fld_name, index->idx_field[i]);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_relation (
    STR		dyn,
    REL		relation)
{
/**************************************
 *
 *	a d d _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a relation.
 *
 **************************************/

if (relation->rel_rse)
    {
    add_view (dyn, relation);
    return;
    }

put_symbol (dyn, gds__dyn_def_rel, relation->rel_name);
put_symbol (dyn, gds__dyn_security_class, relation->rel_security_class);
put_symbol (dyn, gds__dyn_rel_ext_file ,relation->rel_filename);
if (relation->rel_system)
    put_number (dyn, gds__dyn_system_flag, relation->rel_system);
put_text (dyn, gds__dyn_description, relation->rel_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_security_class (
    STR		dyn,
    SCL		class)
{
/**************************************
 *
 *	a d d _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a security class.
 *
 **************************************/

put_symbol (dyn, gds__dyn_def_security_class, class->scl_name);

put_acl (dyn, gds__dyn_scl_acl, class);

put_text (dyn, gds__dyn_description, class->scl_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_trigger (
    STR		dyn,
    TRG		trigger)
{
/**************************************
 *
 *	a d d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a trigger for a relation.
 *
 **************************************/
SYM	name;
REL	relation;

relation = trigger->trg_relation;
name = trigger->trg_name;

put_symbol (dyn, gds__dyn_def_trigger, name);
put_symbol (dyn, gds__dyn_rel_name, relation->rel_name);
put_number (dyn, gds__dyn_trg_type, trigger->trg_type);
put_number (dyn, gds__dyn_trg_sequence, trigger->trg_sequence);
put_number (dyn, gds__dyn_trg_inactive, trigger->trg_inactive);
put_blr (dyn, gds__dyn_trg_blr, relation, trigger->trg_statement);
put_text (dyn, gds__dyn_trg_source, trigger->trg_source);
put_text (dyn, gds__dyn_description, trigger->trg_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_trigger_msg (
    STR		dyn,
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	a d d _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a trigger message.
 *
 **************************************/

put_number (dyn, gds__dyn_def_trigger_msg, trigmsg->trgmsg_number);
put_symbol (dyn, gds__dyn_trg_name, trigmsg->trgmsg_trg_name);
put_symbol (dyn, gds__dyn_trg_msg, trigmsg->trgmsg_text);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void add_view (
    STR		dyn,
    REL		relation)
{
/**************************************
 *
 *	a d d _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a view.
 *
 **************************************/
FLD	field;
CTX	context;
NOD	*arg, contexts;
SSHORT	i;

put_symbol (dyn, gds__dyn_def_view, relation->rel_name);
put_symbol (dyn, gds__dyn_security_class, relation->rel_security_class);
if (relation->rel_system)
    put_number (dyn, gds__dyn_system_flag, relation->rel_system);
put_text (dyn, gds__dyn_description, relation->rel_description);
put_blr (dyn, gds__dyn_view_blr, relation, relation->rel_rse);
put_text (dyn, gds__dyn_view_source, relation->rel_view_source);

contexts = relation->rel_rse->nod_arg [s_rse_contexts];
for (i = 0, arg = contexts->nod_arg; i < contexts->nod_count; i++, arg++)
    {
    context = (CTX) (*arg)->nod_arg [0];
    put_symbol (dyn, gds__dyn_view_relation, context->ctx_relation->rel_name);
    put_number (dyn, gds__dyn_view_context, context->ctx_context_id);
    put_symbol (dyn, gds__dyn_view_context_name, context->ctx_name);
    CHECK_DYN(1);
    STUFF (gds__dyn_end);
    }

for (field = relation->rel_fields; field; field = field->fld_next)
    add_field (dyn, field, relation);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

BOOLEAN TRN_get_buffer (
    STR		dyn,
    USHORT	length)
{
/**************************************
 *
 *	g e t _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Generated DYN string is about to over the memory allocated for it.
 *	So, allocate an extra memory.
 *
 **************************************/
SCHAR	*p, *q, *old;
USHORT	len, n;

len = dyn->str_current - dyn->str_start;

/* Compute the next incremental string len */

n = MIN (dyn->str_length * 2, 65536 - 4);

/* If we're in danger of blowing the limit, stop right here */

if (n < len + length)
    return FALSE;

q = old = dyn->str_start;
dyn->str_start = p = (SCHAR*) gds__alloc ((SLONG) n);

if (!p)
    return FALSE;

dyn->str_length = n;
dyn->str_current = dyn->str_start + len;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
gds_alloc_flag_unfreed (dyn->str_start);
#endif

memmove (p, q, len);
gds__free (old);
return TRUE;
}

static void drop_field (
    STR		dyn,
    FLD		field)
{
/**************************************
 *
 *	d r o p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to eliminate 
 *	a local field.
 *
 **************************************/
SYM	name, symbol;
REL	relation;

name = field->fld_name;
relation = field->fld_relation;

put_symbol (dyn, gds__dyn_delete_local_fld, name);
put_symbol (dyn, gds__dyn_rel_name, relation->rel_name);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}



static void drop_filter (
    STR		dyn,
    FILTER	filter)
{
/**************************************
 *
 *	d r o p _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete a 
 *	blob filter.
 *
 **************************************/

put_symbol (dyn, gds__dyn_delete_filter, filter->filter_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_function (
    STR		dyn,
    FUNC	function)
{
/**************************************
 *
 *	d r o p _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete a 
 *	user defined function.
 *
 **************************************/

put_symbol (dyn, gds__dyn_delete_function, function->func_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_global_field (
    STR		dyn,
    FLD		field)
{
/**************************************
 *
 *	d r o p _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to eliminate 
 *	a global field.
 *
 **************************************/
SYM	name, symbol;

name = field->fld_name;

put_symbol (dyn, gds__dyn_delete_global_fld, name);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_index (
    STR		dyn,
    DUDLEY_IDX		index)
{
/**************************************
 *
 *	d r o p  _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an index.
 *
 **************************************/

put_symbol (dyn, gds__dyn_delete_idx, index->idx_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_relation (
    STR		dyn,
    REL		relation)
{
/**************************************
 *
 *	d r o p  _ r e l a t i o n 
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an relation.
 *
 **************************************/

put_symbol (dyn, gds__dyn_delete_rel, relation->rel_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_security_class (
    STR		dyn,
    SCL		class)
{
/**************************************
 *
 *	d r o p  _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an securtiy_class.
 *
 **************************************/

put_symbol (dyn, gds__dyn_delete_security_class, class->scl_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_shadow (
    STR		dyn,
    SLONG	shadow_number)
{
/**************************************
 *
 *	d r o p _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to drop a shadow.
 *
 **************************************/

put_number (dyn, gds__dyn_delete_shadow, (SSHORT)shadow_number);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_trigger (
    STR		dyn,
    TRG		trigger)
{
/**************************************
 *
 *	d r o p _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to delete a trigger.
 *
 **************************************/
SYM	name, symbol;
REL	relation;
USHORT	dtype;
int	n;

name = trigger->trg_name;

put_symbol (dyn, gds__dyn_delete_trigger, name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void drop_trigger_msg (
    STR		dyn,
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	d r o p _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to delete a trigger message.
 *
 **************************************/

put_number (dyn, gds__dyn_delete_trigger_msg, trigmsg->trgmsg_number);
put_symbol (dyn, gds__dyn_trg_name, trigmsg->trgmsg_trg_name);
CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static int gen_dyn_c (
    int		*user_arg,
    int		offset,
    SCHAR	*string)
{
/**************************************
 *
 *	g e n _ d y n _ c
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/

ib_fprintf (output_file, "    %s\n", string);

return TRUE;
}

static int gen_dyn_cxx (
    int		*user_arg,
    int		offset,
    SCHAR	*string)
{
/**************************************
 *
 *	g e n _ d y n _ c x x
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/
SCHAR *p, *q, *r;

q = p = string;

ib_fprintf (output_file, "    ");
for (; *q; *q++)
    {
    if ((*q == '$') || (*q == '_'))
        {
        r = q;
        if ((*--r == '_') && (*--r == 's') && (*--r == 'd') && (*--r == 'g'))
            {
            *q = 0;
            ib_fprintf (output_file, "%s", p);
            p = ++q;
            }
        }
    }

ib_fprintf (output_file, "%s\n", p);

return TRUE;
}

static int gen_dyn_pas (
    int		*user_arg,
    int		offset,
    SCHAR	*string)
{
/**************************************
 *
 *	g e n _ d y n _ p a s
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/
ib_fprintf (output_file, "      %s\n", string);

return TRUE;
}

static void modify_database (
    STR		dyn,
    DBB		database)
{
/**************************************
 *
 *	m o d i f y _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a database.
 *
 **************************************/

if (!(database->dbb_files ||
    database->dbb_security_class ||
    database->dbb_description ||
    (database->dbb_flags & (DBB_null_security_class | DBB_null_description))))
    return;

CHECK_DYN(1);
STUFF (gds__dyn_mod_database);

add_files (dyn, database->dbb_files, database);
if (database->dbb_flags & DBB_null_security_class)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_security_class);
    STUFF_WORD (0);
    }
else 
    put_symbol (dyn, gds__dyn_security_class, database->dbb_security_class);
if (database->dbb_flags & DBB_null_description)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_description);
    STUFF_WORD (0);
    }
else if (database->dbb_description)
    put_text (dyn, gds__dyn_description, database->dbb_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_field (
    STR		dyn,
    FLD		field,
    REL		view)
{
/**************************************
 *
 *	m o d i f y _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a 
 *	local field.
 *
 **************************************/
SYM	name, symbol;
REL	relation;
FLD	source_field;
int	n;

name = field->fld_name;
if (view)
    relation = view;
else
    relation = field->fld_relation;

put_symbol (dyn, gds__dyn_mod_local_fld, name);
put_symbol (dyn, gds__dyn_rel_name, relation->rel_name);

if ((symbol = field->fld_source) &&
    strcmp (symbol->sym_string, name->sym_string) &&
    !field->fld_computed)
    put_symbol (dyn, gds__dyn_fld_source, symbol);

if (field->fld_flags & fld_null_security_class)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_security_class);
    STUFF_WORD (0);
    }
else 
    put_symbol (dyn, gds__dyn_security_class, field->fld_security_class);
if (field->fld_flags & fld_null_edit_string)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_edit_string);
    STUFF_WORD (0);
    }
else
    put_symbol (dyn, gds__dyn_fld_edit_string, field->fld_edit_string);
if (field->fld_flags & fld_null_query_name)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_query_name);
    STUFF_WORD (0);
    }
else
    put_symbol (dyn, gds__dyn_fld_query_name, field->fld_query_name);
if (field->fld_flags & fld_null_query_header)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_query_header);
    STUFF_WORD (0);
    }
else
    put_query_header (dyn, gds__dyn_fld_query_header, field->fld_query_header);

if (field->fld_system)
    put_number (dyn, gds__dyn_system_flag, field->fld_system);

if (field->fld_flags & fld_explicit_position)
    put_number (dyn, gds__dyn_fld_position, field->fld_position);

if (field->fld_flags & fld_null_description)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_description);
    STUFF_WORD (0);
    }
else if (field->fld_description)
    put_text (dyn, gds__dyn_description, field->fld_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_global_field (
    STR		dyn,
    FLD		field)
{
/**************************************
 *
 *	m o d i f y _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to
 *	modify a global field.  This gets
 *	more and more tiresome.
 *
 **************************************/
SYM	name, symbol;
DSC	desc;
USHORT	dtype;
int	n;

if (field->fld_computed)
    return;

name = field->fld_name;

put_symbol (dyn, gds__dyn_mod_global_fld, name);
if (field->fld_flags & fld_null_edit_string)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_edit_string);
    STUFF_WORD (0);
    }
else
    put_symbol (dyn, gds__dyn_fld_edit_string, field->fld_edit_string);
if (field->fld_flags & fld_null_query_name)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_query_name);
    STUFF_WORD (0);
    }
else
    put_symbol (dyn, gds__dyn_fld_query_name, field->fld_query_name);
if (field->fld_flags & fld_null_query_header)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_query_header);
    STUFF_WORD (0);
    }
else
    put_query_header (dyn, gds__dyn_fld_query_header, field->fld_query_header);

if (field->fld_system & fld_explicit_system)
    put_number (dyn, gds__dyn_system_flag, field->fld_system);

if (field->fld_dtype)
    {
    put_number (dyn, gds__dyn_fld_type, field->fld_dtype);
    put_number (dyn, gds__dyn_fld_length, field->fld_length);
    put_number (dyn, gds__dyn_fld_scale, field->fld_scale);
    }

if (field->fld_has_sub_type)
    put_number (dyn, gds__dyn_fld_sub_type, field->fld_sub_type);

if (n = field->fld_segment_length)
    put_number (dyn, gds__dyn_fld_segment_length, n);

if (field->fld_flags & fld_null_missing_value)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_fld_missing_value);
    STUFF_WORD (0);
    }
else if (field->fld_missing)
    put_blr (dyn, gds__dyn_fld_missing_value, NULL, field->fld_missing);

if (field->fld_flags & fld_null_validation)
    {
    CHECK_DYN(6);
    STUFF (gds__dyn_fld_validation_blr);
    STUFF_WORD (0);
    STUFF (gds__dyn_fld_validation_source);
    STUFF_WORD (0)
    }
else if (field->fld_validation)
    {
    put_blr (dyn, gds__dyn_fld_validation_blr, NULL, field->fld_validation);
    put_text (dyn, gds__dyn_fld_validation_source, field->fld_valid_src);
    }

if (field->fld_flags & fld_null_description)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_description);
    STUFF_WORD (0);
    }
else if (field->fld_description)
    put_text (dyn, gds__dyn_description, field->fld_description);

if (field->fld_dimension)
    {
    put_number (dyn, gds__dyn_fld_dimensions, field->fld_dimension);
    add_dimensions (dyn, field);
    }

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_index (
    STR		dyn,
    DUDLEY_IDX		index)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify an index.
 *
 **************************************/
TEXT	i;

put_symbol (dyn, gds__dyn_mod_idx, index->idx_name);

if (index->idx_flags & IDX_unique_flag)
    put_number (dyn, gds__dyn_idx_unique, index->idx_unique);

if (index->idx_flags & IDX_active_flag)
    put_number (dyn, gds__dyn_idx_inactive, (index->idx_inactive) ? TRUE : FALSE);

if (index->idx_flags & IDX_type_flag)
    put_number (dyn, gds__dyn_idx_type, index->idx_type);

if (index->idx_flags & IDX_null_description)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_description);
    STUFF_WORD (0);
    }
else if (index->idx_description)
    put_text (dyn, gds__dyn_description, index->idx_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_relation (
    STR		dyn,
    REL		relation)
{
/**************************************
 *
 *	m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a relation.
 *
 **************************************/

put_symbol (dyn, gds__dyn_mod_rel, relation->rel_name);
if (relation->rel_flags & rel_null_security_class)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_security_class);
    STUFF_WORD (0);
    }
else 
    put_symbol (dyn, gds__dyn_security_class, relation->rel_security_class);
if (relation->rel_system)
    put_number (dyn, gds__dyn_system_flag, relation->rel_system);
if (relation->rel_flags & rel_null_description)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_description);
    STUFF_WORD (0);
    }
else if (relation->rel_description)
    put_text (dyn, gds__dyn_description, relation->rel_description);
if (relation->rel_flags & rel_null_ext_file)
    {
    CHECK_DYN(3);
    STUFF (gds__dyn_rel_ext_file);
    STUFF_WORD (0);
    }
else
    put_symbol (dyn, gds__dyn_rel_ext_file, relation->rel_filename);


CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_trigger (
    STR		dyn,
    TRG		trigger)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to modify a trigger for a relation.
 *
 **************************************/
SYM	name, symbol;
REL	relation;
USHORT	dtype;
int	n;

relation = trigger->trg_relation;
name = trigger->trg_name;

put_symbol (dyn, gds__dyn_mod_trigger, name);
put_symbol (dyn, gds__dyn_rel_name, relation->rel_name);
if (trigger->trg_mflag & trg_mflag_onoff)
    put_number (dyn, gds__dyn_trg_inactive, trigger->trg_inactive);
if (trigger->trg_mflag & trg_mflag_type) 
    put_number (dyn, gds__dyn_trg_type, trigger->trg_type);
if (trigger->trg_mflag & trg_mflag_seqnum)
    put_number (dyn, gds__dyn_trg_sequence, trigger->trg_sequence);
if (trigger->trg_statement)
    put_blr (dyn, gds__dyn_trg_blr, relation, trigger->trg_statement);
if (trigger->trg_source)
    put_text (dyn, gds__dyn_trg_source, trigger->trg_source);
if (trigger->trg_description)
    put_text (dyn, gds__dyn_description, trigger->trg_description);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void modify_trigger_msg (
    STR		dyn,
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to modify a trigger message.
 *
 **************************************/

put_number (dyn, gds__dyn_mod_trigger_msg, trigmsg->trgmsg_number);
put_symbol (dyn, gds__dyn_trg_name, trigmsg->trgmsg_trg_name);
put_symbol (dyn, gds__dyn_trg_msg, trigmsg->trgmsg_text);

CHECK_DYN(1);
STUFF (gds__dyn_end);
}

static void put_acl (
    STR		dyn,
    UCHAR	attribute,
    SCL		class)
{
/**************************************
 *
 *	p u t _ a c l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
USHORT	length, offset;
TEXT	buffer [4096], *p;

if (!class)
    return;

length = GENERATE_acl (class, buffer);
assert (length <= 4096); /* to make sure buffer is big enough */

CHECK_DYN(3 + length);
STUFF (attribute);
STUFF_WORD (length);
p = buffer;
while (length--)
    STUFF (*p++);
}

static void put_blr (
    STR		dyn,
    UCHAR	attribute,
    REL		relation,
    NOD		node)
{
/**************************************
 *
 *	p u t _ b l r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*p;
USHORT	length, offset;

if (!node)
    return;

CHECK_DYN(3);
STUFF (attribute);

/* Skip over space to put count later, generate blr, then
   go bad and fix up length */

offset = dyn->str_current - dyn->str_start;
dyn->str_current = dyn->str_current + 2;
GENERATE_blr (dyn, node);
p = dyn->str_start + offset;
length = (dyn->str_current - p) - 2;
*p++ = length;
*p = (length >> 8);
}

static void put_number (
    STR		dyn,
    TEXT	attribute,
    SSHORT	number)
{
/**************************************
 *
 *	p u t _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT	*p;

CHECK_DYN(5);
STUFF (attribute);

/* Assume number can be expressed in two bytes */

STUFF_WORD (2);
STUFF_WORD (number);
}

static void put_query_header (
    STR		dyn,
    TEXT	attribute,
    NOD		node)
{
/**************************************
 *
 *	p u t _ q u e r y _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
NOD	*ptr, *end;
SYM	symbol;
SCHAR	*s;
USHORT	length, offset;

if (!node)
    return;

#if (!(defined JPN_SJIS || defined JPN_EUC))

CHECK_DYN(3);    
STUFF (attribute);

#else

/* Put tagged version of the dynamic verb */

CHECK_DYN(5);
STUFF ((DDL_tagged_verbs[attribute]));
STUFF_WORD(DDL_interp);

#endif


offset = dyn->str_current - dyn->str_start;
dyn->str_current = dyn->str_current + 2;
for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
    {
    symbol = (SYM) *ptr;
    s = (SCHAR*) symbol->sym_string;
    CHECK_DYN(symbol->sym_length);
    while (*s)
        STUFF (*s++);
    }
s = dyn->str_start + offset;
length = (dyn->str_current - s) - 2;
*s++ = length;
*s = (length >> 8);
} 

static void put_symbol (
    STR		dyn,
    TEXT	attribute,
    SYM		symbol)
{
/**************************************
 *
 *	p u t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT	*p, *string;
USHORT	l;

if (!symbol)
    return;

l = symbol->sym_length;

CHECK_DYN(l + 5);
#if (! (defined JPN_SJIS || defined JPN_EUC) )

STUFF (attribute);

#else

/* If a taggged version of the dynamic verb exists then put the tagged verb */

if (DDL_tagged_verbs[attribute])
    {
    STUFF ((DDL_tagged_verbs[attribute]));
    STUFF_WORD(DDL_interp);
    }
else
    STUFF (attribute);

#endif
STUFF_WORD (l);

for (string = symbol->sym_string; *string;)
    STUFF (*string++);
}

static void put_text (
    STR		dyn,
    UCHAR	attribute,
    TXT		text)
{
/**************************************
 *
 *	p u t _ t e x t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT	*p, *start;
USHORT	length;

if (!text)
    return;

length = text->txt_length;

if (!length)
    return;

CHECK_DYN(length + 5);
#if (! (defined JPN_SJIS || defined JPN_EUC) )

STUFF (attribute);

#else

/* If a taggged version of the dynamic verb exists then put the tagged verb */

if (DDL_tagged_verbs[attribute])
    {
    STUFF (DDL_tagged_verbs[attribute]);
    STUFF_WORD(DDL_interp);
    }
else
    STUFF (attribute);

#endif

STUFF_WORD (length);
LEX_get_text (dyn->str_current, text);
dyn->str_current += length;
}

static void raw_ada (
    STR		dyn)
{
/**************************************
 *
 *	r a w _ a d a
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*p, c;
USHORT	n;

n = 0;
for (p = dyn->str_start; p < dyn->str_current; p++)
    {
    if (p < (dyn->str_current - 1))
        {
        ib_fprintf (output_file ,"%d,", *p);
        n += 4;
        }
    else
        {
        ib_fprintf (output_file ,"%d", *p);
        n += 3;
        }
    if (n > 60)
        {
        ib_fprintf (output_file ,"\n");
        n = 0;
        }
    }
ib_fprintf (output_file ,");\n");
}

static void raw_basic (
    STR		dyn)
{
/**************************************
 *
 *	r a w _ b a s i c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*blr;
TEXT	buffer[80], *p;
UCHAR	c;
int	blr_length;

blr = dyn->str_start;
blr_length = dyn->str_current - dyn->str_start;
p = buffer;

while (--blr_length)
    {
    c = *blr++;
    if (c >= ' ' && c <= 'z')
	sprintf (p, "'%c' + ", c);
    else
	sprintf (p, "'%d'C + ", c);
    while (*p)
	p++;
    if (p - buffer > 60)
	{
	ib_fprintf (output_file, "   %s &\n", buffer);
	p = buffer;
	*p = 0;
	}
    }

c = *blr++;
if (c >= ' ' && c <= 'z')
    sprintf (p, "'%c'", c);
else
    sprintf (p, "'%d'C", c);
ib_fprintf (output_file, "   %s\n", buffer);
}

static void raw_cobol (
    STR		dyn)
{
/**************************************
 *
 *	r a w _ c o b o l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*blr, *c;
int	blr_length, i, length;
union	{
	SCHAR	bytewise_blr[4];
	SLONG	longword_blr;
	} blr_hunk;

length = 1; 
blr = dyn->str_start;
blr_length = dyn->str_current - dyn->str_start;

while (blr_length)
    {
    for (c = blr_hunk.bytewise_blr; 
         c < blr_hunk.bytewise_blr + sizeof(SLONG); c++)
 	{
        *c = *blr++;
        if (!(--blr_length))
	    break;
	}
    if (language == lan_ansi_cobol)
        ib_fprintf (output_file, "           03  GDS-DYN-%d PIC S9(10) USAGE COMP VALUE IS %d.\n", 
                 length++, blr_hunk.longword_blr);
    else
        ib_fprintf (output_file, "           03  GDS__DYN_%d PIC S9(10) USAGE COMP VALUE IS %d.\n", 
                 length++, blr_hunk.longword_blr);
    }
}

static void raw_ftn (
    STR		dyn)
{
/**************************************
 *
 *	r a w _ f t n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
UCHAR	*blr, *c;
TEXT	buffer[80], *p;
int	blr_length, i;
union	{
	UCHAR bytewise_blr[4];
	SLONG longword_blr;
	} blr_hunk;

blr = (UCHAR*) dyn->str_start;
blr_length = dyn->str_current - dyn->str_start;
p = buffer;

while (blr_length)
    {
    for (c = blr_hunk.bytewise_blr, blr_hunk.longword_blr = 0; 
         c < blr_hunk.bytewise_blr + sizeof(SLONG); c++)
 	{
        *c = *blr++;
        if (!(--blr_length))
	    break;
	}
    if (blr_length)
	sprintf (p, "%d,", blr_hunk.longword_blr);
    else
	sprintf (p, "%d/", blr_hunk.longword_blr);
    while (*p)
	p++;
    if (p - buffer > 50)
	{
	ib_fprintf (output_file, "%s%s\n", "     +   ", buffer);
	p = buffer;
	*p = 0;
	}
    }

if (p != buffer)
    ib_fprintf (output_file, "%s%s\n", "     +   ", buffer);
}

static void raw_pli (
    STR		dyn)
{
/**************************************
 *
 *	r a w _ p l i
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*blr, c;
TEXT	buffer [80], *p;
int	blr_length;

blr = dyn->str_start;
blr_length = dyn->str_current - dyn->str_start;
p = buffer;

while (--blr_length)
    {
    c = *blr++;
    sprintf (p, "%d,", c);
    while (*p)
	p++;
    if (p - buffer > 60)
	{
	ib_fprintf (output_file, "   %s\n", buffer);
	p = buffer;
	*p = 0;
	}
    }

c = *blr++;
sprintf (p, "%d);", c);
ib_fprintf (output_file, "   %s", buffer);
}
