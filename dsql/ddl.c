/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		ddl.c
 *	DESCRIPTION:	Utilities for generating ddl
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
#include "../dsql/dsql.h"
#include "../dsql/node.h"
#include "../jrd/gds.h"
#include "../jrd/thd.h"
#include "../jrd/intl.h"
#include "../jrd/flags.h"
#include "../jrd/constants.h"
#include "../jrd/codes.h"
#include "../dsql/alld_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/thd_proto.h"

#define BLOB_BUFFER_SIZE   4096   /* to read in blr blob for default values */

static void	begin_blr (REQ, UCHAR);
static USHORT	check_array_or_blob (NOD);
static void	check_constraint (REQ, NOD, SSHORT);
static void	create_view_triggers (REQ, NOD, NOD);
static void	define_computed (REQ, NOD, FLD, NOD);
static void	define_constraint_trigger (REQ, NOD);
static void	define_database (REQ);
static void	define_del_cascade_trg (REQ, NOD, NOD, NOD, TEXT *, TEXT *);
static void     define_del_default_trg (REQ, NOD, NOD, NOD, TEXT *, TEXT *);
static void	define_dimensions (REQ, FLD);
static void	define_domain (REQ);
static void	define_exception (REQ, NOD_TYPE);
static void	define_field (REQ, NOD, SSHORT, STR);
static void	define_filter (REQ);
static void	define_generator (REQ);
static void	define_role      (REQ);
static void	define_index (REQ);
static NOD	define_insert_action (REQ);
static void	define_procedure (REQ, NOD_TYPE);
static void	define_rel_constraint (REQ, NOD);
static void	define_relation (REQ);
static void	define_set_null_trg (REQ, NOD, NOD, NOD, TEXT *, TEXT *, BOOLEAN);
static void	define_shadow (REQ);
static void	define_trigger (REQ, NOD);
static void	define_udf (REQ);
static void	define_update_action (REQ, NOD *, NOD *);
static void	define_upd_cascade_trg (REQ, NOD, NOD, NOD, TEXT *, TEXT *);
static void	define_view (REQ);
static void	define_view_trigger (REQ, NOD, NOD, NOD);
static void	end_blr (REQ);
static void	foreign_key (REQ, NOD);
static void	generate_dyn (REQ, NOD);
static void	grant_revoke (REQ);
static void	make_index (REQ, NOD, NOD, NOD, SCHAR *);
static void	make_index_trg_ref_int (REQ, NOD, NOD, NOD, SCHAR *);
static void	modify_database (REQ);
static void	modify_domain (REQ);
static void	modify_field (REQ, NOD, SSHORT, STR);
static void	modify_index (REQ);
static void	modify_privilege (REQ, NOD_TYPE, SSHORT, UCHAR *, NOD, NOD, STR);
static void	process_role_nm_list (REQ, SSHORT, NOD, NOD, NOD_TYPE);
static SCHAR	modify_privileges (REQ, NOD_TYPE, SSHORT, NOD, NOD, NOD);
static void	modify_relation (REQ);
static void	put_cstring (REQ, UCHAR, UCHAR *);
static void	put_descriptor (REQ, DSC *);
static void	put_dtype (REQ, FLD, USHORT);
static void	put_field (REQ, FLD, BOOLEAN);
static void	put_local_variable (REQ, VAR);
static SSHORT	put_local_variables (REQ, NOD, SSHORT);
static void	put_msg_field (REQ, FLD);
static void	put_number (REQ, UCHAR, SSHORT);
static void	put_string (REQ, UCHAR, UCHAR *, USHORT);
static NOD	replace_field_names (NOD, NOD, NOD, SSHORT);
static void	reset_context_stack (REQ);
static void	save_field (REQ, SCHAR *);
static void	save_relation (REQ, STR);
static void	set_statistics (REQ);
static void     stuff_default_blr (REQ, TEXT *, USHORT);
static void	stuff_matching_blr (REQ, NOD, NOD);
static void	stuff_trg_firing_cond (REQ, NOD);
static void	set_nod_value_attributes (NOD, FLD);

#ifdef BLKCHK
#undef BLKCHK
#endif

#ifdef DEV_BUILD
#define BLKCHK(blk, typ)	\
	{ \
	if ((blk) && (((BLK) (blk))->blk_type != (typ))) \
	    ERRD_bugcheck ("Invalid block type");	/* NTX: dev build */ \
	}
#else
#define BLKCHK(blk, typ)
#endif


/* STUFF is defined in dsql.h for use in common with gen.c */

#define STUFF_WORD(n)		{STUFF (n); STUFF ((n) >> 8);}
#define STUFF_DWORD(n)		{STUFF (n); STUFF ((n) >> 8);\
				 STUFF ((n) >> 16); STUFF ((n) >> 24);}

#define PRE_STORE_TRIGGER	1
#define POST_STORE_TRIGGER	2
#define PRE_MODIFY_TRIGGER	3
#define POST_MODIFY_TRIGGER	4
#define PRE_ERASE_TRIGGER	5
#define POST_ERASE_TRIGGER	6

#define OLD_CONTEXT		"OLD"
#define NEW_CONTEXT		"NEW"
#define TEMP_CONTEXT		"TEMP"

#define DEFAULT_BUFFER		2048

#define DEFAULT_BLOB_SEGMENT_SIZE	80	/* bytes */

static CONST USHORT	blr_dtypes [] = {
	0,
	blr_text,	/* dtype_text */
	blr_cstring,	/* dtype_cstring */
	blr_varying,	/* dtype_varying */
	0,
	0,
	0,		/* dtype_packed */
	0,		/* dtype_byte */
	blr_short,	/* dtype_short */
	blr_long,	/* dtype_long */
	blr_quad,	/* dtype_quad */
	blr_float,	/* dtype_real */
	blr_double,	/* dtype_double */
	blr_double,	/* dtype_d_float */
	blr_sql_date,	/* dtype_sql_date */
	blr_sql_time,	/* dtype_sql_time */
	blr_timestamp,	/* dtype_timestamp */
	blr_blob,	/* dtype_blob */
	blr_short,	/* dtype_array */
	blr_int64       /* dtype_int64 */
	};

static CONST UCHAR	nonnull_validation_blr [] = 
   {
   blr_version5,
   blr_not, 
      blr_missing, 
         blr_fid, 0, 0,0, 
   blr_eoc
   };

ASSERT_FILENAME

void DDL_execute (
    REQ		request)
{
/**************************************
 *
 *	D D L _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Call access method layered service DYN
 *	to interpret dyn string and perform 
 *	metadata updates.
 *
 **************************************/
USHORT	length;
STR	string;
STATUS	s;
NOD	relation_node;
TSQL	tdsql;

tdsql = GET_THREAD_DATA;

#ifdef DEBUG
#if !(defined REQUESTER && defined SUPERCLIENT)
if (DSQL_debug > 0)
    PRETTY_print_dyn (request->req_blr_string->str_data, NULL, "%4d %s\n", NULL);
#endif
#endif

length = request->req_blr - request->req_blr_string->str_data;

THREAD_EXIT;

s = isc_ddl (GDS_VAL(tdsql->tsql_status),
	GDS_REF (request->req_dbb->dbb_database_handle),
	GDS_REF (request->req_trans),
	length,
	GDS_VAL (request->req_blr_string->str_data));

THREAD_ENTER;

/* for delete & modify, get rid of the cached relation metadata */

if ((request->req_ddl_node->nod_type == nod_mod_relation) ||
    (request->req_ddl_node->nod_type == nod_del_relation))
    {
    if (request->req_ddl_node->nod_type == nod_mod_relation)
	{
	relation_node = request->req_ddl_node->nod_arg [e_alt_name];
	string = (STR) relation_node->nod_arg [e_rln_name];
	}
    else
	string = (STR) request->req_ddl_node->nod_arg [e_alt_name];
    METD_drop_relation (request, string); 
    }

/* for delete & modify, get rid of the cached procedure metadata */

if ((request->req_ddl_node->nod_type == nod_mod_procedure) ||
    (request->req_ddl_node->nod_type == nod_del_procedure))
    {
    string = (STR) request->req_ddl_node->nod_arg [e_prc_name];
    METD_drop_procedure (request, string); 
    }

if (s)
    LONGJMP (tdsql->tsql_setjmp, (int) tdsql->tsql_status [1]);
}

void DDL_generate (
    REQ		request,   
    NOD		node)
{
/**************************************
 *
 *	D D L _ g e n e r a t e
 *
 **************************************
 *
 * Functional description
 *	Generate the DYN string for a
 *	metadata update.  Done during the
 *	prepare phase.
 *
 **************************************/

#ifdef READONLY_DATABASE
if (request->req_dbb->dbb_flags & DBB_read_only)
    {
    ERRD_post (isc_read_only_database, 0);
    return;
    }
#endif  /* READONLY_DATABASE */

STUFF (gds__dyn_version_1);
generate_dyn (request, node);
STUFF (gds__dyn_eoc);
}

int DDL_ids (
    REQ		request)   
{
/**************************************
 *
 *	D D L _ i d s
 *
 **************************************
 *
 * Functional description
 *	Determine whether ids or names should be
 *	referenced when generating blr for fields
 *	and relations.
 *
 **************************************/
NOD	ddl_node;

if (!(ddl_node = request->req_ddl_node))
    return TRUE;

if (ddl_node->nod_type == nod_def_view ||
    ddl_node->nod_type == nod_def_constraint ||
    ddl_node->nod_type == nod_def_trigger ||
    ddl_node->nod_type == nod_mod_trigger ||
    ddl_node->nod_type == nod_def_procedure ||
    ddl_node->nod_type == nod_def_computed ||
    ddl_node->nod_type == nod_mod_procedure)
    return FALSE;

return TRUE;
}

void DDL_put_field_dtype (
    REQ		request,   
    FLD		field,
    USHORT	use_subtype)
{
/**************************************
 *
 *	D D L _ p u t _ f i e l d _ d t y p e 
 *
 **************************************
 *
 * Functional description
 *	Emit blr that describes a descriptor.
 *	Note that this depends on the same STUFF variant
 *	as used in gen.c
 *
 **************************************/

put_dtype (request, field, use_subtype);
}

void DDL_resolve_intl_type (
    REQ		request,
    FLD		field,
    STR		collation_name)
{
/**************************************
 *
 *	D D L _ r e s o l v e _ i n t l _ t y p e
 *
 **************************************
 *
 * Function
 *	If the field is defined with a character set or collation,
 *	resolve the names to a subtype now.
 *
 *	Also resolve the field length & whatnot.
 *
 *	For International text fields, this is a good time to calculate
 *	their actual size - when declared they were declared in
 *	lengths of CHARACTERs, not BYTES.
 *
 **************************************/
UCHAR	*charset_name;
INTLSYM	resolved_type;
INTLSYM	resolved_charset;
INTLSYM resolved_collation;
STR	dfl_charset;
SSHORT	blob_sub_type;
ULONG	field_length;

if ((field->fld_dtype > dtype_any_text) && field->fld_dtype != dtype_blob)
    {
    if (field->fld_character_set || collation_name ||
	field->fld_flags & FLD_national)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
	    gds_arg_gds, gds__collation_requires_text,
    	    0);
    
    return;
    }

if (field->fld_dtype == dtype_blob)
    {
    if (field->fld_sub_type_name)
	{
	if (!METD_get_type (request, field->fld_sub_type_name, 
				"RDB$FIELD_SUB_TYPE", &blob_sub_type))
	    {
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    	gds_arg_gds, gds__dsql_datatype_err, 
	    	gds_arg_gds, gds__dsql_blob_type_unknown,
		gds_arg_string, ((STR)field->fld_sub_type_name)->str_data,
    	    	0);
	    }
	field->fld_sub_type = blob_sub_type;
	}
    if (field->fld_character_set && (field->fld_sub_type == BLOB_untyped))
	field->fld_sub_type = BLOB_text;
    if (field->fld_character_set && (field->fld_sub_type != BLOB_text))
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
	    gds_arg_gds, gds__collation_requires_text,
    	    0);
    if (collation_name)
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
	    gds_arg_gds, gds__collation_requires_text,
    	    0);
    if (field->fld_sub_type != BLOB_text)
	return;
    }

if (field->fld_character_set_id != 0 &&
    !collation_name)
    {
    /* This field has already been resolved once, and the collation hasn't
     * changed.  Therefore, no need to do it again.
     */
    return;
    }

if (!(field->fld_character_set || 
      field->fld_character_set_id ||		/* set if a domain */
      (field->fld_flags & FLD_national)))
    {

    /* Attach the database default character set, if not otherwise specified */

    if (dfl_charset = METD_get_default_charset (request))
	{
	field->fld_character_set = (NOD) dfl_charset;
	}
    else
	{
        /* If field is not specified with NATIONAL, or CHARACTER SET
         * treat it as a single-byte-per-character field of character set NONE.
         */
        if (field->fld_character_length)
	    {
            field_length = field->fld_character_length;
	    if (field->fld_dtype == dtype_varying)
		field_length += sizeof (USHORT);
	    if (field_length > (ULONG) MAX_COLUMN_SIZE)
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
		    gds_arg_gds, gds__dsql_datatype_err,
		    gds_arg_gds, gds__imp_exc, 
		    gds_arg_gds, gds__field_name, gds_arg_string, field->fld_name,
		    0);
	    field->fld_length = (USHORT) field_length;
	    };
        field->fld_ttype = 0;
	if (!collation_name)
	    return;
	}
    }

if (field->fld_flags & FLD_national)
    charset_name = (UCHAR*) NATIONAL_CHARACTER_SET;
else if (field->fld_character_set)
    charset_name = (UCHAR*) ((STR) field->fld_character_set)->str_data;
else
    charset_name = (UCHAR*) NULL;


/* Find an intlsym for any specified character set name & collation name */

resolved_charset = NULL;
if (charset_name)
    {
    resolved_charset = METD_get_charset (request, strlen (charset_name), charset_name);

    /* Error code -204 (IBM's DB2 manual) is close enough */
    if (!resolved_charset)
	/* specified character set not found */
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
    	    gds_arg_gds, gds__charset_not_found, gds_arg_string, charset_name,
    	    0);
    field->fld_character_set_id = resolved_charset->intlsym_charset_id;
    resolved_type = resolved_charset;
    }

resolved_collation = NULL;
if (collation_name)
    {
    resolved_collation = METD_get_collation (request, collation_name);
    if (!resolved_collation)
	/* Specified collation not found */
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
    	    gds_arg_gds, gds__collation_not_found, gds_arg_string, collation_name->str_data,
    	    0);

    /* If both specified, must be for same character set */
    /* A "literal constant" must be handled (charset as ttype_dynamic) */

    resolved_type = resolved_collation;
    if ((field->fld_character_set_id != resolved_type->intlsym_charset_id) &&
        (field->fld_character_set_id != ttype_dynamic))
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
    	    gds_arg_gds, gds__collation_not_for_charset, gds_arg_string, collation_name->str_data,
    	    0);
    }


if (field->fld_character_length)
    {
    field_length = (ULONG) resolved_type->intlsym_bytes_per_char * 
    			field->fld_character_length;

    if (field->fld_dtype == dtype_varying)
        field_length += sizeof (USHORT);
    if (field_length > (ULONG) MAX_COLUMN_SIZE)
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
		gds_arg_gds, gds__dsql_datatype_err,
		gds_arg_gds, gds__imp_exc, 
		gds_arg_gds, gds__field_name, gds_arg_string, field->fld_name,
		0);
    field->fld_length = (USHORT) field_length;
    };

field->fld_ttype = resolved_type->intlsym_ttype;
field->fld_character_set_id = resolved_type->intlsym_charset_id;
field->fld_collation_id = resolved_type->intlsym_collate_id;
}

static void begin_blr (
    REQ		request,
    UCHAR	verb)    
{
/**************************************
 *
 *	b e g i n _ b l r
 *
 **************************************
 *
 * Function
 *	Write out a string of blr as part of a ddl string,
 *	as in a view or computed field definition.
 *
 **************************************/

if (verb)
    STUFF (verb);

request->req_base_offset = request->req_blr - request->req_blr_string->str_data;

/* put in a place marker for the size of the blr, since it is unknown */

STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
}

static USHORT check_array_or_blob (
    NOD		node)
{
/**************************************
 *
 *	c h e c k _ a r r a y _ o r _ b l o b
 *
 **************************************
 *
 * Functional description
 *	return TRUE if there is an array or blob in expression, else FALSE.
 *	Array and blob expressions have limited usefullness in a computed
 *	expression - so we detect it here to report a syntax error at 
 *	definition time, rather than a runtime error at execution.
 *
 **************************************/
MAP	map;
UDF	udf;
FLD	fld;
NOD	*ptr, *end;

BLKCHK (node, type_nod);

switch (node->nod_type)
    {
    case nod_agg_count:
    case nod_count:
    case nod_gen_id:
    case nod_gen_id2:
    case nod_dbkey:
    case nod_current_date:
    case nod_current_time:
    case nod_current_timestamp:
    case nod_constant:
    case nod_via:
	return FALSE;

    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	return check_array_or_blob (map->map_node);

    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_average:
    case nod_agg_total:
    case nod_agg_average2:
    case nod_agg_total2:
    case nod_upcase:
    case nod_negate:
	return check_array_or_blob (node->nod_arg [0]);

    case nod_cast:
	fld = (FLD) node->nod_arg [e_cast_target];
	if ((fld->fld_dtype == dtype_blob) || (fld->fld_dtype == dtype_array))
	    return TRUE;

	return check_array_or_blob (node->nod_arg [e_cast_source]);

    case nod_add:
    case nod_subtract:
    case nod_concatenate:
    case nod_multiply:
    case nod_divide:
    case nod_add2:
    case nod_subtract2:
    case nod_multiply2:
    case nod_divide2:

	if (check_array_or_blob (node->nod_arg [0]))
	    return TRUE;
	return check_array_or_blob (node->nod_arg [1]);

    case nod_alias:
	return check_array_or_blob (node->nod_arg [e_alias_value]);

    case nod_udf:
	udf = (UDF) node->nod_arg [0];
	if ((udf->udf_dtype == dtype_blob) || (udf->udf_dtype == dtype_array))
	    return TRUE;

	/* parameters to UDF don't need checking, an blob or array can be passed */
	return FALSE;

    case nod_extract:
    case nod_list:
	for (ptr = node->nod_arg, end = ptr + node->nod_count; 
	     ptr < end; ptr++)
		if (check_array_or_blob (*ptr))
		    return TRUE;

	return FALSE;

    case nod_field:
	if ((node->nod_desc.dsc_dtype == dtype_blob) ||
	    (node->nod_desc.dsc_dtype == dtype_array))
	    return TRUE;

	return FALSE;

    default:
	assert (FALSE);
        return FALSE;
    }
}

static void check_constraint (
    REQ		request,
    NOD		element,
    SSHORT	delete_trigger_required)
{
/* *************************************
 *
 *	c h e c k _ c o n s t r a i n t
 *
 **************************************
 *
 * Function
 *	Generate triggers to implement the CHECK
 *	clause, either at the field or table level.
 *
 **************************************/
NOD	ddl_node, list_node;
NOD	*errorcode_node;

ddl_node = request->req_ddl_node;
if (!(element->nod_arg [e_cnstr_table]))
    element->nod_arg [e_cnstr_table] = ddl_node->nod_arg [e_drl_name];

/* specify that the trigger should abort if the condition is not met */

element->nod_arg [e_cnstr_actions] = list_node = MAKE_node (nod_list, (int) 1);
list_node->nod_arg[0] = MAKE_node (nod_gdscode, (int) 1);
errorcode_node = &list_node->nod_arg[0]->nod_arg[0];
*errorcode_node = (NOD) MAKE_cstring ("check_constraint");
element->nod_arg [e_cnstr_message] = NULL;

/* create the INSERT trigger */

/* element->nod_arg [e_cnstr_message] =
 *  (NOD) MAKE_cstring ("insert violates CHECK constraint on table");
 */
element->nod_arg [e_cnstr_type] = MAKE_constant ((STR) PRE_STORE_TRIGGER, 1);
define_constraint_trigger (request, element);

/* create the UPDATE trigger */

/* element->nod_arg [e_cnstr_message] =
 *  (NOD) MAKE_cstring ("update violates CHECK constraint on table");
 */
element->nod_arg [e_cnstr_type] = MAKE_constant ((STR) PRE_MODIFY_TRIGGER, 1);
define_constraint_trigger (request, element);

/* create the DELETE trigger, if required   */

if (delete_trigger_required)
   {
/*
 * element->nod_arg [e_cnstr_message] =
 *  (NOD) MAKE_cstring ("delete violates CHECK constraint on table");
 */
   element->nod_arg [e_cnstr_type] = MAKE_constant ((STR) PRE_ERASE_TRIGGER, 1);
   define_constraint_trigger (request, element);
   }

STUFF (gds__dyn_end);  /* For CHECK constraint definition  */
}

static void create_view_triggers (
    REQ		request,
    NOD		element,
    NOD		items)   /* Fields in the VIEW actually  */
{
/* *************************************
 *
 *	c r e a t e _ v i e w _ t r i g g e r s 
 *
 **************************************
 *
 * Function
 *	Generate triggers to implement the WITH CHECK OPTION
 *	clause for a VIEW
 *
 **************************************/
NOD	temp, rse, base_relation, base_and_node, ddl_node, list_node;
NOD	*errorcode_node;

ddl_node = request->req_ddl_node;

if (!(element->nod_arg [e_cnstr_table]))
    element->nod_arg [e_cnstr_table] = ddl_node->nod_arg [e_drl_name];

/* specify that the trigger should abort if the condition is not met */

element->nod_arg [e_cnstr_actions] = list_node = MAKE_node (nod_list, (int) 1);
list_node->nod_arg[0] = MAKE_node (nod_gdscode, (int) 1);
errorcode_node = &list_node->nod_arg[0]->nod_arg[0];
*errorcode_node = (NOD) MAKE_cstring ("check_constraint");
element->nod_arg [e_cnstr_message] = NULL;

/* create the UPDATE trigger */

/* element->nod_arg [e_cnstr_message] =
 *   (NOD) MAKE_cstring ("update violates CHECK constraint on view");
 */
element->nod_arg [e_cnstr_type] = MAKE_constant ((STR) PRE_MODIFY_TRIGGER, 1);
define_update_action (request, &base_and_node, &base_relation);

rse = MAKE_node (nod_rse, e_rse_count);
rse->nod_arg [e_rse_boolean] = base_and_node;
rse->nod_arg [e_rse_streams] = temp = MAKE_node (nod_list, 1);
temp->nod_arg [0] = base_relation;
define_view_trigger (request, element, rse, items);

/* create the INSERT trigger */

/* element->nod_arg [e_cnstr_message] =
 *   (NOD) MAKE_cstring ("insert violates CHECK constraint on view");
 */
element->nod_arg [e_cnstr_type] = MAKE_constant ((STR) PRE_STORE_TRIGGER, 1);
define_view_trigger (request, element, NULL, items);

STUFF (gds__dyn_end);  /* For triggers definition  */
}

static void define_computed (
    REQ		request,
    NOD		relation_node, 
    FLD		field,
    NOD		node)
{
/**************************************
 *
 *	d e f i n e _ c o m p u t e d
 *
 **************************************
 *
 * Function
 *	Create the ddl to define a computed field 
 *	or an expression index. 
 *
 **************************************/
NOD	input, ddl_node;
STR	source;
DSC	save_desc, desc;

ddl_node = request->req_ddl_node;
request->req_ddl_node = node;

/* Get the table node & set up correct context */

if (request->req_context_number)
    reset_context_stack (request);

/* Save the size of the field if it is specified */

save_desc.dsc_dtype  = 0;

if (field && field->fld_dtype)
    {
    save_desc.dsc_dtype  = field->fld_dtype;
    save_desc.dsc_length = field->fld_length;
    save_desc.dsc_scale  = field->fld_scale;

    field->fld_dtype  = 0;
    field->fld_length = 0;
    field->fld_scale  = 0;
    }

PASS1_make_context (request, relation_node);

input = PASS1_node (request, node->nod_arg [e_cmp_expr], 0);

/* check if array or blobs are used in expression */

if (check_array_or_blob (input))
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
	gds_arg_gds, gds__dsql_no_blob_array,
	0);

/* generate the blr expression */

STUFF (gds__dyn_fld_computed_blr);
begin_blr (request, 0);
GEN_expr (request, input);
end_blr (request);

/* try to calculate size of the computed field. The calculated size
   may be ignored, but it will catch self references */

MAKE_desc (&desc, input);

if (save_desc.dsc_dtype)
    {
    /* restore the field size/type overrides */

    field->fld_dtype  = save_desc.dsc_dtype;
    field->fld_length = save_desc.dsc_length;
    field->fld_scale  = save_desc.dsc_scale;
    }
else if (field)
    {
    /* use size calculated */

    field->fld_dtype  = desc.dsc_dtype;
    field->fld_length = desc.dsc_length;
    field->fld_scale  = desc.dsc_scale;
    }

request->req_type = REQ_DDL;
request->req_ddl_node = ddl_node;
reset_context_stack (request);

/* generate the source text */

source = (STR) node->nod_arg [e_cmp_text];
put_string (request, gds__dyn_fld_computed_source, source->str_data, 
	    source->str_length);
}

static void define_constraint_trigger (
    REQ		request,
    NOD		node)
{
/**************************************
 *
 *	d e f i n e _ c o n s t r a i n t _ t r i g g e r
 *
 **************************************
 *
 * Function
 *	Create the ddl to define or alter a constraint trigger.
 *
 **************************************/
STR	trigger_name, relation_name, source, message;
NOD	ddl_node, actions, *ptr, *end, constant, relation_node;
SSHORT	type;

/* make the "define trigger" node the current request ddl node so
   that generating of BLR will be appropriate for trigger */

ddl_node = request->req_ddl_node;
request->req_ddl_node = node;

trigger_name = (STR) node->nod_arg [e_cnstr_name];

if (node->nod_type == nod_def_constraint)
    {
    put_string (request, gds__dyn_def_trigger, trigger_name->str_data, 
			trigger_name->str_length);
    relation_node = node->nod_arg [e_cnstr_table];
    relation_name = (STR) relation_node->nod_arg [e_rln_name];
    put_string (request, gds__dyn_rel_name, relation_name->str_data, 
			relation_name->str_length);
    }
else
    return;

if ((source = (STR) node->nod_arg [e_cnstr_source]) != NULL)
    put_string (request, gds__dyn_trg_source, source->str_data, source->str_length);

if ((constant = node->nod_arg [e_cnstr_position]) != NULL)
    put_number (request, gds__dyn_trg_sequence, constant ? 
				(SSHORT) constant->nod_arg [0] : 0);

if ((constant = node->nod_arg [e_cnstr_type]) != NULL)
    {
    type = (SSHORT) constant->nod_arg [0];
    put_number (request, gds__dyn_trg_type, type);
    }

STUFF (gds__dyn_sql_object);

if ((message = (STR) node->nod_arg [e_cnstr_message]) != NULL)
    {
    put_number (request, gds__dyn_def_trigger_msg, 0);
    put_string (request, gds__dyn_trg_msg, message->str_data, message->str_length);
    STUFF (gds__dyn_end);
    }

/* generate the trigger blr */

if (node->nod_arg [e_cnstr_condition] && node->nod_arg [e_cnstr_actions])
    {
    begin_blr (request, gds__dyn_trg_blr);
    STUFF (blr_begin);

    /* create the "OLD" and "NEW" contexts for the trigger --
       the new one could be a dummy place holder to avoid resolving
       fields to that context but prevent relations referenced in
       the trigger actions from referencing the predefined "1" context */

    if (request->req_context_number)
	reset_context_stack (request);
    relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (OLD_CONTEXT);
    PASS1_make_context (request, relation_node);
    relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (NEW_CONTEXT);
    PASS1_make_context (request, relation_node);

    /* generate the condition for firing the trigger */

    STUFF (blr_if);
    GEN_expr (request, PASS1_node (request, node->nod_arg [e_cnstr_condition], 0));

    STUFF (blr_begin);
    STUFF (blr_end); /* of begin */

    /* generate the action statements for the trigger */
    actions = node->nod_arg [e_cnstr_actions];
    for (ptr = actions->nod_arg, end = ptr + actions->nod_count; ptr < end; ptr++)
	GEN_statement(request, PASS1_statement (request, *ptr, 0));

    /* generate the action statements for the trigger */

    if ((actions = node->nod_arg [e_cnstr_else]) != NULL)
	{
	STUFF (blr_begin);
	for (ptr = actions->nod_arg, end = ptr + actions->nod_count; ptr < end; ptr++)
	    GEN_statement(request, PASS1_statement (request, *ptr, 0));
	STUFF (blr_end); /* of begin */
	}
    else
	STUFF (blr_end); /* of if */
    end_blr (request);
    }

STUFF (gds__dyn_end);
			    
/* the request type may have been set incorrectly when parsing
   the trigger actions, so reset it to reflect the fact that this
   is a data definition request; also reset the ddl node */

request->req_type = REQ_DDL;
request->req_ddl_node = ddl_node;
reset_context_stack (request);
}

static void define_database (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ d a t a b a s e
 *
 **************************************
 *
 * Function
 *	Create a database. Assumes that 
 *	database is created elsewhere with 
 *	initial options. Modify the 
 *	database using DYN to add the remaining
 *	options.
 *
 **************************************/
NOD	ddl_node, elements, element, *ptr, *end;
STR	name;
SLONG	start = 0;
FIL	file;
SSHORT 	number = 0;
SLONG	temp_long;
SSHORT	temp_short;

ddl_node = request->req_ddl_node;

STUFF (gds__dyn_mod_database);
/*
put_number (request, gds__dyn_rel_sql_protection, 1);
*/

elements = ddl_node->nod_arg [e_database_initial_desc];

if (elements)
  for (ptr = elements->nod_arg, end = ptr + elements->nod_count; 
	ptr < end; ptr++)
    {
    element = *ptr; 

    switch (element->nod_type)
	{
	case nod_file_length:
	    start = (SLONG) (element->nod_arg [0]) + 1;
	    break;

	}
    }

elements = ddl_node->nod_arg [e_database_rem_desc]; 
if (elements)
  for (ptr = elements->nod_arg, end = ptr + elements->nod_count;
	ptr < end; ptr++)
    {
    element = *ptr; 

    switch (element->nod_type)
	{
	case nod_file_desc:
	    file = (FIL) element->nod_arg [0];
	    put_cstring (request, gds__dyn_def_file, file->fil_name->str_data);
	    STUFF (gds__dyn_file_start);
	    STUFF_WORD (4);
	    start = MAX (start, file->fil_start);
	
	    STUFF_DWORD (start) ;
	
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF(gds__dyn_end);
	    start += file->fil_length;
	    break;

	case nod_log_file_desc:
	    file = (FIL) element->nod_arg [0];
	
	    if (file->fil_flags & LOG_default)
		{
		STUFF (gds__dyn_def_default_log);
		break;
		}
	    put_cstring (request, gds__dyn_def_log_file, 
			file->fil_name->str_data);
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF (gds__dyn_log_file_sequence);
	    STUFF_WORD (2);
	    STUFF_WORD (number);
	    number++;
	    STUFF (gds__dyn_log_file_partitions);
	    STUFF_WORD (2);
	    STUFF_WORD (file->fil_partitions);
	    if (file->fil_flags & LOG_serial)
	        STUFF (gds__dyn_log_file_serial);
	    if (file->fil_flags & LOG_overflow)
	        STUFF (gds__dyn_log_file_overflow);
	    if (file->fil_flags & LOG_raw)
	        STUFF (gds__dyn_log_file_raw);
	    STUFF(gds__dyn_end);
	    break;

	case nod_cache_file_desc:
	    file = (FIL) element->nod_arg [0];
	    put_cstring (request, gds__dyn_def_cache_file,
		file->fil_name->str_data);
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF(gds__dyn_end);
	    break;

	case nod_group_commit_wait:
	    STUFF (gds__dyn_log_group_commit_wait);
	    temp_long = (SLONG) (element->nod_arg [0]);
	    STUFF_WORD (4);
	    STUFF_DWORD (temp_long);
	    break;

	case nod_check_point_len:
	    STUFF (gds__dyn_log_check_point_length);
	    temp_long = (SLONG) (element->nod_arg [0]);
	    STUFF_WORD (4);
	    STUFF_DWORD (temp_long);
	    break;

	case nod_num_log_buffers:
	    STUFF (gds__dyn_log_num_of_buffers);
	    temp_short = (SSHORT) (element->nod_arg [0]);
	    STUFF_WORD (2);
	    STUFF_WORD (temp_short);
	    break;

	case nod_log_buffer_size:
	    STUFF (gds__dyn_log_buffer_size);
	    temp_short = (SSHORT) (element->nod_arg [0]);
	    STUFF_WORD (2);
	    STUFF_WORD (temp_short);
	    break;

	case nod_dfl_charset:
	    name = (STR) element->nod_arg [0];
	    put_cstring (request, gds__dyn_fld_character_set_name, name->str_data);
	    break;
	}
    }

STUFF (gds__dyn_end);
}

static void define_del_cascade_trg (
    REQ         request,
    NOD         element,
    NOD		for_columns,
    NOD         prim_columns,
    TEXT        *prim_rel_name,
    TEXT        *for_rel_name
    )
{
/*****************************************************
 *
 *	d e f i n e _ d e l _ c a s c a d e _ t r g
 *
 *****************************************************
 *
 * Function
 *	define "on delete cascade" trigger (for referential integrity)
 *      along with its blr
 *
 *****************************************************/
USHORT num_fields = 0;

if (element->nod_type != nod_foreign)
    return;

/* stuff a trigger_name of size 0. So the dyn-parser will make one up.  */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_number (request, gds__dyn_trg_type, (SSHORT) POST_ERASE_TRIGGER);

STUFF(gds__dyn_sql_object);
put_number (request, gds__dyn_trg_sequence, (SSHORT)1);
put_number (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, prim_rel_name);

/* the trigger blr */
begin_blr (request, gds__dyn_trg_blr);
STUFF (blr_for);
STUFF (blr_rse); 

/* the context for the prim. key relation */
STUFF (1);

STUFF(blr_relation);
put_cstring (request, 0, for_rel_name);
/* the context for the foreign key relation */
STUFF (2);

stuff_matching_blr(request, for_columns, prim_columns);

STUFF (blr_erase); STUFF ((SSHORT)2);
end_blr (request);
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);

}

static void define_set_default_trg (
    REQ         request,
    NOD         element,
    NOD		for_columns,
    NOD         prim_columns,
    TEXT        *prim_rel_name,
    TEXT        *for_rel_name,
    BOOLEAN     on_upd_trg
    )
{
/*****************************************************
 *
 *	d e f i n e _ s e t _ d e f a u l t _ t r g
 *
 *****************************************************
 *
 * Function
 *	define "on delete|update set default" trigger (for 
 *      referential integrity) along with its blr
 *
 *****************************************************/
NOD       *for_key_flds;
USHORT    num_fields = 0;
STR       for_key_fld_name_str;
UCHAR     default_val[BLOB_BUFFER_SIZE];
BOOLEAN   found_default, search_for_default;

NOD       ddl_node, elem, *ptr, *end, default_node;
NOD       domain_node, tmp_node;
FLD       field;
STR       domain_name_str;
TEXT      *domain_name;

if (element->nod_type != nod_foreign)
    return;

/* stuff a trigger_name of size 0. So the dyn-parser will make one up.  */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_number (request, gds__dyn_trg_type,
    on_upd_trg ? (SSHORT)POST_MODIFY_TRIGGER : (SSHORT)POST_ERASE_TRIGGER);

STUFF(gds__dyn_sql_object);
put_number (request, gds__dyn_trg_sequence, (SSHORT)1);
put_number (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, prim_rel_name);

/* the trigger blr */
begin_blr (request, gds__dyn_trg_blr);

/* for ON UPDATE TRIGGER only: generate the trigger firing condition:
   if prim_key.old_value != prim_key.new value.
   Note that the key could consist of multiple columns */

if (on_upd_trg)
    {
    stuff_trg_firing_cond (request, prim_columns);
    STUFF (blr_begin);
    STUFF (blr_begin);
    }

STUFF (blr_for);
STUFF (blr_rse); 

/* the context for the prim. key relation */
STUFF (1);
STUFF(blr_relation);
put_cstring (request, 0, for_rel_name);
/* the context for the foreign key relation */
STUFF (2);

stuff_matching_blr(request, for_columns, prim_columns);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

num_fields = 0;
for_key_flds = for_columns->nod_arg;

ddl_node = request->req_ddl_node;
    
do  {
    /* for every column in the foreign key .... */
    for_key_fld_name_str = (STR) (*for_key_flds)->nod_arg[1];

    STUFF (blr_assignment);

    /* here stuff the default value as blr_literal .... or blr_null
       if this col. does not have an applicable default */

    /* the default is determined in many cases:
       (1) the info. for the column is in memory. (This is because
	   the column is being created in this ddl statement)
	   (1-a) the table has a column level default. We get this by
	   searching the dsql parse tree starting from the ddl node.
	   (1-b) the table does not have a column level default, but
	   has a domain default. We get the domain name from the dsql
	   parse tree and call METD_get_domain_default to read the
	   default from the system tables.
       (2) The default-info for this column is not in memory (This is
           because this is an alter table ddl statement). The table
           already exists; therefore we get the column and/or domain
	   default value from the system tables by calling: 
	   METD_get_col_default().  */

    found_default = FALSE;
    search_for_default = TRUE;

    /* search the parse tree to find the column */

    elem = ddl_node->nod_arg [e_drl_elements];
    for (ptr = elem->nod_arg, end = ptr + elem->nod_count; ptr < end; ptr++)
        {
        elem = *ptr;
        if (elem->nod_type != nod_def_field) 
    	    continue;
        field = (FLD) elem->nod_arg [e_dfl_field];
        if (strcmp(field->fld_name, for_key_fld_name_str->str_data))
    	    continue;

	/* Now, we have the right column in the parse tree. case (1) above */

        if ((default_node = elem->nod_arg [e_dfl_default]) != NULL)
    	    {
	    /* case (1-a) above: there is a col. level default */
            GEN_expr (request, default_node);
    	    found_default = TRUE;
	    search_for_default = FALSE;
    	    }
	else
	    {
	    if ( !(domain_node = elem->nod_arg [e_dfl_domain])  ||
	         !(tmp_node = domain_node->nod_arg [e_dom_name]) ||
	         !(domain_name_str = (STR) tmp_node->nod_arg [e_fln_name]) ||
	         !(domain_name =  domain_name_str->str_data) )
	        break;

	    /* case: (1-b): domain name is available. Column level default 
	       is not declared. so get the domain default */
            METD_get_domain_default (request, domain_name, &found_default,
	        default_val, sizeof(default_val));
	
	    search_for_default = FALSE;
            if (found_default)
                stuff_default_blr (request, default_val, sizeof(default_val));
	    else
		/* neither col level nor domain level default exists */
		STUFF (blr_null);
	    }
	break;
	}

    if (search_for_default)
	{
        /* case 2: see if the column/domain has already been created */
	
        METD_get_col_default (request, for_rel_name, 
	    for_key_fld_name_str->str_data, &found_default, 
	    default_val, sizeof(default_val));

        if (found_default)
	    stuff_default_blr (request, default_val, sizeof(default_val));
        else 
	    STUFF (blr_null);
    
	}

    /* the context for the foreign key relation */
    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name_str->str_data);
 
    num_fields++;
    for_key_flds++;
    }
while (num_fields < for_columns->nod_count);
STUFF (blr_end);

if (on_upd_trg)
    {
    STUFF (blr_end);
    STUFF (blr_end);
    STUFF (blr_end);
    }

end_blr (request);
/* no trg_source and no trg_description */
STUFF (gds__dyn_end);
}

static void define_dimensions (
    REQ request,
    FLD field)
{
/*****************************************
 *
 *	d e f i n e _ d i m e n s i o n s
 *
 *****************************************
 *
 * Function
 *	Define dimensions of an array 
 *
 **************************************/
NOD    elements, element, *ptr, *end;
SSHORT  position;
USHORT  dims;
SLONG   lrange, hrange;

elements = field->fld_ranges;
dims = elements->nod_count / 2;

if (dims > MAX_ARRAY_DIMENSIONS)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -604, 
        gds_arg_gds, gds__dsql_max_arr_dim_exceeded,
        0);

put_number (request, gds__dyn_fld_dimensions, (SSHORT) dims);

for (ptr = elements->nod_arg, end = ptr + elements->nod_count, position=0 
					; ptr < end; ptr++,position++)
    {
    put_number (request, gds__dyn_def_dimension, position);
    element = *ptr++;
    STUFF (gds__dyn_dim_lower);
    lrange = (SLONG) (element->nod_arg [0]);
    STUFF_WORD (4);
    STUFF_DWORD (lrange);
    element = *ptr;
    STUFF (gds__dyn_dim_upper);
    hrange = (SLONG) (element->nod_arg [0]);
    STUFF_WORD (4);
    STUFF_DWORD (hrange);
    STUFF (gds__dyn_end);	
    if (lrange >= hrange)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -604, 
            gds_arg_gds, gds__dsql_arr_range_error, 
            0);
    }	
}

static void define_domain (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ d o m a i n
 *
 **************************************
 *
 * Function
 *	Define a domain (global field)  
 *
 **************************************/
NOD	element, node, node1, *ptr, *end_ptr;
FLD	field;
STR	string;
BOOLEAN null_flag = FALSE, check_flag = FALSE;

element = request->req_ddl_node;
field = (FLD) element->nod_arg [e_dom_name];

put_cstring (request, gds__dyn_def_global_fld, field->fld_name);

DDL_resolve_intl_type (request, field, (STR) element->nod_arg [e_dom_collate]);
put_field (request, field, FALSE);

/* check for a default value */

if ((node = element->nod_arg [e_dom_default]) != NULL)
    {
    begin_blr (request, gds__dyn_fld_default_value);
    GEN_expr (request, node);
    end_blr (request);
    if ((string = (STR) element->nod_arg [e_dom_default_source]) != NULL)
	put_string (request, gds__dyn_fld_default_source, string->str_data, 
						string->str_length);	
    }

if (field->fld_ranges)
    define_dimensions (request, field);

/* check for constraints */

if (node = element->nod_arg [e_dom_constraint])
    {
    for (ptr = node->nod_arg, end_ptr = ptr + node->nod_count; ptr < end_ptr; ptr++)  
	{
	if ((*ptr)->nod_type == nod_rel_constraint)
	    {
	    node1 = (*ptr)->nod_arg [e_rct_type];
	    if (node1->nod_type == nod_null)
		{
                if (!(null_flag))
                    {
		    STUFF (gds__dyn_fld_not_null);
                    null_flag = TRUE;
                    }
                else
                    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -637,
	                gds_arg_gds, gds__dsql_duplicate_spec, 
		        gds_arg_string, "NOT NULL", 
                        0);
		}
	    else if (node1->nod_type == nod_def_constraint)
		{
                if (check_flag)
                    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -637,
                        gds_arg_gds, gds__dsql_duplicate_spec,
                        gds_arg_string, "DOMAIN CHECK CONSTRAINT",
                        0);
                check_flag = TRUE;
                if ((string = (STR) node1->nod_arg [e_cnstr_source]) != NULL)
                    put_string (request, gds__dyn_fld_validation_source,
                                string->str_data, string->str_length);
                begin_blr (request, gds__dyn_fld_validation_blr);
                
		/* Set any VALUE nodes to the type of the domain being defined. */
		if (node1->nod_arg [e_cnstr_condition])
		    set_nod_value_attributes (node1->nod_arg [e_cnstr_condition],
					      field);

		/* Increment the context level for this request, so
		   that the context number for any RSE generated for a
		   SELECT within the CHECK clause will be greater than
		   0.  In the environment of a domain check
		   constraint, context number 0 is reserved for the
		   "blr_fid, 0, 0,0," which is emitted for a
		   nod_dom_value, corresponding to an occurance of the
		   VALUE keyword in the bod of the check constraint.
		   -- chrisj 1999-08-20 */

		request->req_context_number++;

		GEN_expr (request, 
                  PASS1_node (request, node1->nod_arg [e_cnstr_condition], 0));

                end_blr (request);
		}
	    }
	}
    }

STUFF (gds__dyn_end);
}

static void define_exception (
    REQ		request,
    NOD_TYPE	op)
{
/**************************************
 *
 *	d e f i n e _ e x c e p t i o n
 *
 **************************************
 *
 * Function
 *	Generate ddl to create an exception code.
 *
 **************************************/
NOD	ddl_node;
STR	text, name;

ddl_node = request->req_ddl_node;
name = (STR) ddl_node->nod_arg [e_xcp_name];
text = (STR) ddl_node->nod_arg [e_xcp_text];

if (op == nod_def_exception)
    put_cstring (request, gds__dyn_def_exception, name->str_data);
else if (op == nod_mod_exception)
    put_cstring (request, gds__dyn_mod_exception, name->str_data);
else
    put_cstring (request, gds__dyn_del_exception, name->str_data);

if (op != nod_del_exception)
    {
    put_string (request, gds__dyn_xcp_msg, text->str_data, text->str_length);
    STUFF (gds__dyn_end);
    }
}

static void define_field (
    REQ		request,
    NOD		element,
    SSHORT	position,
    STR		relation_name)
{
/**************************************
 *
 *	d e f i n e _ f i e l d
 *
 **************************************
 *
 * Function
 *	Define a field, either as part of a create
 *	table or an alter table statement.
 *
 **************************************/
NOD	domain_node, node, node1, *ptr, *end_ptr;
FLD	field;
DSQL_REL	relation;
STR	string, domain_name;
USHORT	cnstrt_flag = FALSE;
NOD	computed_node;
BOOLEAN	default_null_flag = FALSE;

field = (FLD) element->nod_arg [e_dfl_field];

/* add the field to the relation being defined for parsing purposes */

if ((relation = request->req_relation) != NULL)
    {
    field->fld_next = relation->rel_fields;
    relation->rel_fields = field;
    }

if (domain_node = element->nod_arg [e_dfl_domain])
    {
    put_cstring (request, gds__dyn_def_local_fld, field->fld_name);
    node1 = domain_node->nod_arg [e_dom_name];
    domain_name = (STR) node1->nod_arg [e_fln_name];
    put_cstring (request, gds__dyn_fld_source, domain_name->str_data);

    /* Get the domain information */

    if (!(METD_get_domain (request, field, domain_name->str_data)))
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__dsql_domain_not_found, 
            /* Specified domain or source field does not exist */
            0);
       
    DDL_resolve_intl_type (request, field, element->nod_arg [e_dfl_collate]);
    if (element->nod_arg [e_dfl_collate])
	{
        put_number (request, gds__dyn_fld_collation, field->fld_collation_id);
	}
    }
else
    {
    put_cstring (request, gds__dyn_def_sql_fld, field->fld_name);
    if (relation_name)
	put_cstring (request, gds__dyn_rel_name, relation_name->str_data);

    if (element->nod_arg [e_dfl_computed])
	{
	field->fld_flags |= FLD_computed;
	computed_node = element->nod_arg [e_dfl_computed];
	define_computed (request, request->req_ddl_node->nod_arg [e_drl_name], field, computed_node);
	}

    DDL_resolve_intl_type (request, field, element->nod_arg [e_dfl_collate]);
    put_field (request, field, FALSE);
    }

if (position != -1)
    put_number (request, gds__dyn_fld_position, position);

/* check for a default value */

if ((node = element->nod_arg [e_dfl_default]) != NULL)
    {
    begin_blr (request, gds__dyn_fld_default_value);
    if (node->nod_type == nod_null) 
	default_null_flag = TRUE;
    GEN_expr (request, node);
    end_blr (request);
    if ((string = (STR) element->nod_arg [e_dfl_default_source]) != NULL)
	put_string (request, gds__dyn_fld_default_source, string->str_data, 
						string->str_length);	
    }

if (field->fld_ranges)
    define_dimensions (request, field);

/* check for constraints */

if (node = element->nod_arg [e_dfl_constraint])
    {
    for (ptr = node->nod_arg, end_ptr = ptr + node->nod_count; ptr < end_ptr; ptr++)  
	{
	if ((*ptr)->nod_type == nod_rel_constraint)
	    {
	    string = (STR) (*ptr)->nod_arg [e_rct_name];
	    node1 = (*ptr)->nod_arg [e_rct_type];
	    
	    if (node1->nod_type == nod_null)
		{
		if (default_null_flag == TRUE)
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
			       gds_arg_gds, isc_bad_default_value,
			       gds_arg_gds, isc_invalid_clause,
			       gds_arg_string, "default null not null",0);
		STUFF (gds__dyn_fld_not_null);
		if (cnstrt_flag == FALSE)
		    {
		    STUFF (gds__dyn_end);  /* For field definition  */
		    cnstrt_flag = TRUE;
		    }
		put_cstring (request, gds__dyn_rel_constraint, (string) ? string->str_data : NULL);
		STUFF (gds__dyn_fld_not_null); 
		STUFF (gds__dyn_end);  /* For NOT NULL Constraint definition  */
		}
	    else if (node1->nod_type == nod_primary || node1->nod_type == nod_unique)
		{
		if (cnstrt_flag == FALSE)
		    {
		    STUFF (gds__dyn_end);  /* For field definition  */
		    cnstrt_flag = TRUE;
		    }
		put_cstring (request, gds__dyn_rel_constraint, (string) ? string->str_data : NULL);
		if (node1->nod_type == nod_primary)
		    STUFF (gds__dyn_def_primary_key);
		else if (node1->nod_type == nod_unique)
		    STUFF (gds__dyn_def_unique);

		STUFF_WORD (0);  /* So index name is generated   */
		put_number (request, gds__dyn_idx_unique, 1);
		put_cstring (request, gds__dyn_fld_name, field->fld_name);
		STUFF (gds__dyn_end);
		}
	    else if (node1->nod_type == nod_foreign)
		{
		if (cnstrt_flag == FALSE)
		    {
		    STUFF (gds__dyn_end);  /* For field definition  */
		    cnstrt_flag = TRUE;
		    }
		put_cstring (request, gds__dyn_rel_constraint, (string) ? string->str_data : NULL);
		foreign_key (request, node1);
		}
	    else if (node1->nod_type == nod_def_constraint)
		{
		if (cnstrt_flag == FALSE)
		    {
		    STUFF (gds__dyn_end);  /* For field definition  */
		    cnstrt_flag = TRUE;
		    }
		put_cstring (request, gds__dyn_rel_constraint, (string) ? string->str_data : NULL);
		check_constraint (request, node1, FALSE /* No delete trigger */);
		}
	    }
	}
    }

if (cnstrt_flag == FALSE)
    STUFF (gds__dyn_end);   
}

static void define_filter (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ f i l t e r
 *
 **************************************
 *
 * Function
 *	define a filter to the database.
 *
 **************************************/
NOD	*ptr, filter_node;

filter_node = request->req_ddl_node;
ptr = filter_node->nod_arg;
put_cstring (request, gds__dyn_def_filter, 
			((STR) (ptr [ e_filter_name]))->str_data);
put_number (request, gds__dyn_filter_in_subtype, 
		(SSHORT)	((ptr[e_filter_in_type])->nod_arg[0]));
put_number (request, gds__dyn_filter_out_subtype, 
		(SSHORT)	((ptr[e_filter_out_type])->nod_arg[0]));
put_cstring (request, gds__dyn_func_entry_point, 
			((STR) (ptr [ e_filter_entry_pt]))->str_data);
put_cstring (request, gds__dyn_func_module_name, 
			((STR) (ptr [ e_filter_module]))->str_data);

STUFF (gds__dyn_end);
}

static void define_generator (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ g e n e r a t o r
 *
 **************************************
 *
 * Function
 *	create a generator.
 *
 **************************************/
STR	gen_name;

gen_name = (STR) request->req_ddl_node->nod_arg [e_gen_name];
put_cstring (request, gds__dyn_def_generator, gen_name->str_data);
STUFF (gds__dyn_end);
}

static void define_index (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ i n d e x
 *
 **************************************
 *
 * Function
 *	Generate ddl to create an index.
 *
 **************************************/
NOD	ddl_node, relation_node, field_list, *ptr, *end;
STR	relation_name, index_name;

STUFF (gds__dyn_begin);

ddl_node = request->req_ddl_node;
relation_node = (NOD) ddl_node->nod_arg [e_idx_table];
relation_name = (STR) relation_node->nod_arg [e_rln_name];
field_list = ddl_node->nod_arg [e_idx_fields];
index_name = (STR) ddl_node->nod_arg [e_idx_name];

put_cstring (request, gds__dyn_def_idx, index_name->str_data);
put_cstring (request, gds__dyn_rel_name, relation_name->str_data);

/* go through the fields list, making an index segment for each field, 
   unless we have a computation, in which case generate an expression index */

if (field_list->nod_type == nod_list)
    for (ptr = field_list->nod_arg, end = ptr + field_list->nod_count; ptr < end; ptr++)
        put_cstring (request, gds__dyn_fld_name, ((STR) (*ptr)->nod_arg [1])->str_data);
#ifdef EXPRESSION_INDICES
else if (field_list->nod_type == nod_def_computed)
    define_computed (request, relation_node, NULL, field_list);
#endif

/* check for a unique index */

if (ddl_node->nod_arg [e_idx_unique])
    put_number (request, gds__dyn_idx_unique, 1);

if (ddl_node->nod_arg [e_idx_asc_dsc])
    put_number (request, gds__dyn_idx_type, 1);

STUFF (gds__dyn_end); /* of define index */
STUFF (gds__dyn_end); /* of begin */
}

static NOD define_insert_action (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ i n s e r t _ a c t i o n
 *
 **************************************
 *
 * Function
 *	Define an action statement which, given a view 
 *	definition, will store a record from
 *	a view of a single relation into the 
 *	base relation.
 *
 **************************************/
NOD	ddl_node, action_node, insert_node;
NOD	select_node, select_expr, from_list, relation_node;
NOD	fields_node, values_node, field_node, value_node;
NOD	*ptr, *end, *ptr2, *end2;
LLS	field_stack, value_stack;
DSQL_REL	relation;
FLD	field;

ddl_node = request->req_ddl_node;

/* check whether this is an updatable view definition */

if (ddl_node->nod_type != nod_def_view ||
    !(select_node = ddl_node->nod_arg [e_view_select]) ||
    /* 
       Handle VIEWS with UNION : nod_select now points to nod_list
       which in turn points to nod_select_expr
    */
    !(select_expr = select_node->nod_arg [0]->nod_arg [0]) ||
    !(from_list = select_expr->nod_arg [e_sel_from]) ||
    from_list->nod_count != 1)
    return NULL;

/* make up an action node consisting of a list of 1 insert statement */

action_node = MAKE_node (nod_list, (int) 1);
action_node->nod_arg [0] = insert_node = MAKE_node (nod_insert, (int) e_ins_count);

/* use the relation referenced in the select statement to insert into */

relation_node = MAKE_node (nod_relation_name, (int) e_rln_count);
insert_node->nod_arg [e_ins_relation] = relation_node;
relation_node->nod_arg [e_rln_name] = from_list->nod_arg [0]->nod_arg [e_rln_name];
relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (TEMP_CONTEXT);

/* get the list of values and fields to assign to -- if there is 
   no list of fields, get all fields in the base relation that
   are not computed */

values_node = ddl_node->nod_arg [e_view_fields];
if (!(fields_node = select_expr->nod_arg [e_sel_list]))
    {
    relation = METD_get_relation (request, relation_node->nod_arg [e_rln_name]);
    field_stack = NULL;
    for (field = relation->rel_fields; field; field = field->fld_next)
	{
	if (field->fld_flags & FLD_computed)
	    continue;
	field_node = MAKE_node (nod_field_name, (int) e_fln_count);
	field_node->nod_arg [e_fln_name] = (NOD) MAKE_cstring (field->fld_name);
	LLS_PUSH (field_node, &field_stack);
	}
    fields_node = MAKE_list (field_stack);
    }

if (!values_node)
    values_node = fields_node;

/* generate the list of assignments to fields in the base relation */

ptr = fields_node->nod_arg; 
end = ptr + fields_node->nod_count; 
ptr2 = values_node->nod_arg; 
end2 = ptr2 + values_node->nod_count; 
value_stack = field_stack = NULL;
for (; (ptr < end) && (ptr2 < end2); ptr++, ptr2++)
    {
    field_node = *ptr;
    if (field_node->nod_type == nod_alias)
	field_node = field_node->nod_arg [e_alias_value];

    /* generate the actual assignment, assigning from a field in the "NEW" context */

    if (field_node->nod_type == nod_field_name)
	{
	field_node->nod_arg [e_fln_context] = (NOD) MAKE_cstring (TEMP_CONTEXT);
	LLS_PUSH (field_node, &field_stack);

	value_node = MAKE_node (nod_field_name, (int) e_fln_count);
	value_node->nod_arg [e_fln_name] = (*ptr2)->nod_arg [e_fln_name];
	value_node->nod_arg [e_fln_context] = (NOD) MAKE_cstring (NEW_CONTEXT);
	LLS_PUSH (value_node, &value_stack);
	}
    }

insert_node->nod_arg [e_ins_values] = MAKE_list (value_stack);
insert_node->nod_arg [e_ins_fields] = MAKE_list (field_stack);

return action_node;
}

static void define_procedure (
    REQ		request,
    NOD_TYPE	op)
{
/**************************************
 *
 *	d e f i n e _ p r o c e d u r e
 *
 **************************************
 *
 * Function
 *	Create DYN to store a procedure
 *
 **************************************/
NOD	parameters, parameter, *ptr, *end, procedure_node;
STR	procedure_name, source;
PRC	procedure;
FLD	field, *field_ptr;
SSHORT	position, inputs, outputs, locals;
VAR	variable;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

inputs = outputs = locals = 0;
procedure_node = request->req_ddl_node;
procedure_name = (STR) procedure_node->nod_arg [e_prc_name];
if (op == nod_def_procedure)
    {
    put_cstring (request, gds__dyn_def_procedure, procedure_name->str_data);
    put_number (request, gds__dyn_rel_sql_protection, 1);
    }
else
    {
    put_cstring (request, gds__dyn_mod_procedure, procedure_name->str_data);
    if (procedure = METD_get_procedure (request, procedure_name))
	{
	for (field = procedure->prc_inputs; field; field = field->fld_next)
	    {
	    put_cstring (request, gds__dyn_delete_parameter, field->fld_name);
	    STUFF (gds__dyn_end);
	    }
	for (field = procedure->prc_outputs; field; field = field->fld_next)
	    {
	    put_cstring (request, gds__dyn_delete_parameter, field->fld_name);
	    STUFF (gds__dyn_end);
	    }
	}
    }
if ((source = (STR) procedure_node->nod_arg [e_prc_source]) != NULL)
    put_string (request, gds__dyn_prc_source, source->str_data,
		source->str_length);

/* Fill req_procedure to allow procedure to self reference */
procedure = (PRC) ALLOCDV (type_prc, strlen (procedure_name->str_data) + 1);
procedure->prc_name = procedure->prc_data;
procedure->prc_owner = procedure->prc_data + procedure_name->str_length + 1;
strcpy (procedure->prc_name, (SCHAR*) procedure_name->str_data);
*procedure->prc_owner = '\0';
request->req_procedure = procedure;


/* now do the input parameters */

field_ptr = &procedure->prc_inputs;

if (parameters = procedure_node->nod_arg [e_prc_inputs]) 
    {
    position = 0;
    for (ptr = parameters->nod_arg, end = ptr + parameters->nod_count;
	 ptr < end; ptr++)
	{
	parameter = *ptr; 
	field = (FLD) parameter->nod_arg [e_dfl_field];

	put_cstring (request, gds__dyn_def_parameter, field->fld_name);
	put_number (request, gds__dyn_prm_number, position);
	put_number (request, gds__dyn_prm_type, 0);
	DDL_resolve_intl_type (request, field, NULL);
	put_field (request, field, FALSE);

	*ptr = MAKE_variable (field, field->fld_name,
	    VAR_input, 0, 2 * position, locals);
	/* Put the field in a field list which will be stored to allow
	   procedure self referencing */
	*field_ptr = field;
	field_ptr = &field->fld_next;
	position++;

	STUFF (gds__dyn_end);
	put_number (request, gds__dyn_prc_inputs, position);
	}
    inputs = position;
    }

/* Terminate the input list */

*field_ptr = NULL;

/* now do the output parameters */
field_ptr = &procedure->prc_outputs;

if (parameters = procedure_node->nod_arg [e_prc_outputs]) 
    {
    position = 0;
    for (ptr = parameters->nod_arg, end = ptr + parameters->nod_count;
	 ptr < end; ptr++)
	{
	parameter = *ptr; 
	field = (FLD) parameter->nod_arg [e_dfl_field];
	put_cstring (request, gds__dyn_def_parameter, field->fld_name);
	put_number (request, gds__dyn_prm_number, position);
	put_number (request, gds__dyn_prm_type, 1);
	DDL_resolve_intl_type (request, field, NULL);
	put_field (request, field, FALSE);

	*ptr = MAKE_variable (field, field->fld_name,
	    VAR_output, 1, 2 * position, locals);
	*field_ptr = field;
	field_ptr = &field->fld_next;
	position++;
	locals++;

	STUFF (gds__dyn_end);
	put_number (request, gds__dyn_prc_outputs, position);
	}
    outputs = position;
    }

*field_ptr = NULL;
procedure->prc_out_count = outputs;
procedure->prc_in_count = inputs ;

begin_blr (request, gds__dyn_prc_blr);
STUFF (blr_begin);
if (inputs)
    {
    STUFF (blr_message);
    STUFF (0);
    STUFF_WORD (2 * inputs);
    parameters = procedure_node->nod_arg [e_prc_inputs]; 
    for (ptr = parameters->nod_arg, end = ptr + parameters->nod_count;
	 ptr < end; ptr++)
	{
	parameter = *ptr;
	variable = (VAR) parameter->nod_arg [e_var_variable];
	field = variable->var_field;
	put_msg_field (request, field);
	}
    }
STUFF (blr_message);
STUFF (1);
STUFF_WORD (2 * outputs + 1);
if (outputs)
    {
    parameters = procedure_node->nod_arg [e_prc_outputs]; 
    for (ptr = parameters->nod_arg, end = ptr + parameters->nod_count;
	 ptr < end; ptr++)
	{
	parameter = *ptr; 
	variable = (VAR) parameter->nod_arg [e_var_variable];
	field = variable->var_field;
	put_msg_field (request, field);
	}
    }

/* add slot for EOS */

STUFF (blr_short);
STUFF (0);

if (inputs)
    {
    STUFF (blr_receive);
    STUFF (0);
    }
STUFF (blr_begin);
if (outputs)
    {
    parameters = procedure_node->nod_arg [e_prc_outputs]; 
    for (ptr = parameters->nod_arg, end = ptr + parameters->nod_count;
	 ptr < end; ptr++)
	{
	parameter = *ptr; 
	variable = (VAR) parameter->nod_arg [e_var_variable];
	put_local_variable (request, variable);
	}
    }

locals = put_local_variables (request, procedure_node->nod_arg [e_prc_dcls],
    locals);

STUFF (blr_stall);
/* Put a label before body of procedure, so that
   any exit statement can get out */
STUFF (blr_label);
STUFF (0);
request->req_loop_number = 1;
GEN_statement (request,
    PASS1_statement (request, procedure_node->nod_arg [e_prc_body], 1));
request->req_type = REQ_DDL;
STUFF (blr_end);
GEN_return (request, procedure_node, TRUE);
STUFF (blr_end);
end_blr (request);

STUFF (gds__dyn_end);
}

static void define_rel_constraint (
    REQ		request,
    NOD		element)
{
/**************************************
 *
 *	d e f i n e _ r e l _ c o n s t r a i n t 
 *
 **************************************
 *
 * Function
 *	Define a constraint , either as part of a create
 *	table or an alter table statement.
 *
 **************************************/
NOD	node;
STR	string;

string = (STR) element->nod_arg [e_rct_name];
put_cstring (request, gds__dyn_rel_constraint, (string) ? string->str_data : NULL);
node = element->nod_arg [e_rct_type];

if (node->nod_type == nod_unique || node->nod_type == nod_primary)
    make_index (request, node, node->nod_arg [0], NULL_PTR, NULL_PTR);
else if (node->nod_type == nod_foreign)
    foreign_key (request, node);
else if (node->nod_type == nod_def_constraint)
    check_constraint (request, node, FALSE /* No delete trigger */);
}

static void define_relation (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ r e l a t i o n
 *
 **************************************
 *
 * Function
 *	Create an SQL table, relying on DYN to generate
 *	global fields for the local fields. 
 *
 **************************************/
NOD	ddl_node, elements, element, *ptr, *end, relation_node;
STR	relation_name, external_file;
SSHORT	position;

ddl_node = request->req_ddl_node;

relation_node = ddl_node->nod_arg [e_drl_name];
relation_name = (STR) relation_node->nod_arg [e_rln_name];
put_cstring (request, gds__dyn_def_rel, relation_name->str_data);
if (external_file = (STR) ddl_node->nod_arg [e_drl_ext_file])
    put_cstring (request, gds__dyn_rel_ext_file, external_file->str_data);
save_relation (request, relation_name);
put_number (request, gds__dyn_rel_sql_protection, 1);

/* now do the actual metadata definition */

elements = ddl_node->nod_arg [e_drl_elements]; 
for (ptr = elements->nod_arg, end = ptr + elements->nod_count, position = 0;
	ptr < end; ptr++)
    {
    element = *ptr; 
    switch (element->nod_type)
	{
	case nod_def_field:
	    define_field (request, element, position, relation_name);
	    position++;
	    break;

	case nod_rel_constraint:
	   define_rel_constraint (request, element);
	   break;
	}
    }

STUFF (gds__dyn_end);
}

static void define_role (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ r o l e
 *
 **************************************
 *
 * Function
 *	create a SQL role.
 *
 **************************************/
STR	gen_name;

gen_name = (STR) request->req_ddl_node->nod_arg [e_gen_name];
put_cstring (request, isc_dyn_def_sql_role, gen_name->str_data);
STUFF (gds__dyn_end);
}

static void define_set_null_trg (
    REQ         request,
    NOD         element,
    NOD		for_columns,
    NOD         prim_columns,
    TEXT        *prim_rel_name,
    TEXT        *for_rel_name,
    BOOLEAN     on_upd_trg
    )
{
/*****************************************************
 *
 *	d e f i n e _ s e t _ n u l l _ t r g
 *
 *****************************************************
 *
 * Function
 *	define "on delete/update set null" trigger (for referential integrity)
 *      The trigger blr is the same for both the delete and update
 *      cases. Only difference is its TRIGGER_TYPE (ON DELETE or ON UPDATE)
 *      The on_upd_trg parameter == TRUE is an update trigger.
 *
 *****************************************************/
NOD *for_key_flds;
USHORT num_fields = 0;
STR  for_key_fld_name_str;


if (element->nod_type != nod_foreign)
    return;

/* count of foreign key columns */
assert (prim_columns->nod_count == for_columns->nod_count);
assert (prim_columns->nod_count != 0);

/* no trigger name. It is generated by the engine */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_number (request, gds__dyn_trg_type, 
    on_upd_trg ? (SSHORT)POST_MODIFY_TRIGGER : (SSHORT)POST_ERASE_TRIGGER);

STUFF(gds__dyn_sql_object);
put_number (request, gds__dyn_trg_sequence, (SSHORT)1);
put_number (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, prim_rel_name);

/* the trigger blr */
begin_blr (request, gds__dyn_trg_blr);

/* for ON UPDATE TRIGGER only: generate the trigger firing condition: 
   if prim_key.old_value != prim_key.new value.
   Note that the key could consist of multiple columns */

if (on_upd_trg)
    {
    stuff_trg_firing_cond (request, prim_columns);
    STUFF (blr_begin);
    STUFF (blr_begin);
    }

STUFF (blr_for);
STUFF (blr_rse); 

/* the context for the prim. key relation */
STUFF (1);

STUFF(blr_relation);
put_cstring (request, 0, for_rel_name);
/* the context for the foreign key relation */
STUFF (2);

stuff_matching_blr(request, for_columns, prim_columns);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

num_fields = 0;
for_key_flds = for_columns->nod_arg;

do {
   for_key_fld_name_str = (STR) (*for_key_flds)->nod_arg[1];

   STUFF (blr_assignment);
   STUFF (blr_null);
   STUFF (blr_field); STUFF ((SSHORT)2);
   put_cstring (request, 0, for_key_fld_name_str->str_data);

   num_fields++;
   for_key_flds++;
   }
while (num_fields < for_columns->nod_count);
STUFF (blr_end);

if (on_upd_trg) 
    {
    STUFF (blr_end); 
    STUFF (blr_end); 
    STUFF (blr_end);
    }
end_blr (request);
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);

}

static void define_shadow (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ s h a d o w
 *
 **************************************
 *
 * Function
 * 	create a shadow for the database
 *
 **************************************/
NOD	shadow_node, elements, element, *ptr, *end;
SLONG	start;
FIL	file;
SLONG	length;

shadow_node = request->req_ddl_node;
ptr = shadow_node->nod_arg;
if (!ptr [e_shadow_number])
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__dsql_shadow_number_err, 
        0);

put_number (request, gds__dyn_def_shadow, 
			(SSHORT) (ptr[e_shadow_number]));
put_cstring (request, gds__dyn_def_file,
			((STR) (ptr [ e_shadow_name]))->str_data);
put_number (request, gds__dyn_shadow_man_auto,
			(SSHORT) ((ptr[e_shadow_man_auto])->nod_arg[0]));
put_number (request, gds__dyn_shadow_conditional,
			(SSHORT) ((ptr[e_shadow_conditional])->nod_arg[0]));

STUFF (gds__dyn_file_start);
STUFF_WORD (4);
STUFF_DWORD (0);  

length = (SLONG) ptr [e_shadow_length];
STUFF (gds__dyn_file_length);
STUFF_WORD (4);
STUFF_DWORD (length);  

STUFF (gds__dyn_end);
elements = ptr [e_shadow_sec_files];
if (elements)
    for ( ptr = elements->nod_arg, end = ptr + elements->nod_count;
		   ptr < end; ptr++)
        {
	element = *ptr;
        file = (FIL) element->nod_arg [0];
        put_cstring (request, gds__dyn_def_file, file->fil_name->str_data);

	if (!length && !file->fil_start)
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
                gds_arg_gds, gds__dsql_command_err, 
                gds_arg_gds, gds__dsql_file_length_err, 
		gds_arg_number, (SLONG) file->fil_name->str_data,
	        /* Preceding file did not specify length, so %s must include starting page number */ 
                0);

        STUFF (gds__dyn_file_start);
        STUFF_WORD (4);
        start = file->fil_start;
        STUFF_DWORD (start) ;
        STUFF (gds__dyn_file_length);
        STUFF_WORD (4);
	length = file->fil_length;
        STUFF_DWORD (length);
        STUFF(gds__dyn_end);
        }

STUFF (gds__dyn_end);
}

static void define_trigger (
    REQ		request,
    NOD		node)
{
/**************************************
 *
 *	d e f i n e _ t r i g g e r
 *
 **************************************
 *
 * Function
 *	Create the ddl to define or alter a trigger.
 *
 **************************************/
STR	trigger_name, relation_name, source, message_text;
NOD	temp, actions, *ptr, *end, constant, relation_node, message;
SSHORT	number;
USHORT	trig_type;
TSQL	tdsql;

tdsql = GET_THREAD_DATA;

/* make the "define trigger" node the current request ddl node so
   that generating of BLR will be appropriate for trigger */

request->req_ddl_node = node;

trigger_name = (STR) node->nod_arg [e_trg_name];

if (node->nod_type == nod_def_trigger)
    {
    put_string (request, gds__dyn_def_trigger, trigger_name->str_data, 
			trigger_name->str_length);
    relation_node = node->nod_arg [e_trg_table];
    relation_name = (STR) relation_node->nod_arg [e_rln_name];
    put_string (request, gds__dyn_rel_name, relation_name->str_data, 
			relation_name->str_length);
    STUFF (gds__dyn_sql_object);
    }
else /* if (node->nod_type == nod_mod_trigger) */
    {
    assert (node->nod_type == nod_mod_trigger);
    put_string (request, gds__dyn_mod_trigger, trigger_name->str_data, 
			trigger_name->str_length);
    if (node->nod_arg [e_trg_actions])
	{
	/* Since we will be updating the body of the trigger, we need
	   to know what relation the trigger relates to. */

	if (!(relation_name = METD_get_trigger_relation (request, trigger_name, &trig_type)))
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
	        gds_arg_gds, gds__dsql_trigger_err, 
                gds_arg_gds, gds__random, 
		gds_arg_string, trigger_name->str_data, 
                0);
	relation_node = (NOD) ALLOCDV (type_nod, e_rln_count);
	node->nod_arg [e_trg_table] = relation_node;
	relation_node->nod_type = nod_relation_name;
	relation_node->nod_count = e_rln_count;
	relation_node->nod_arg [e_rln_name] = (NOD) relation_name;
	}
    }

source = (STR) node->nod_arg [e_trg_source];
actions = (node->nod_arg [e_trg_actions]) ?
    node->nod_arg [e_trg_actions]->nod_arg [1] : NULL;

if (source && actions)
    put_string (request, gds__dyn_trg_source, source->str_data, source->str_length);
				    
if (constant = node->nod_arg [e_trg_active])
    put_number (request, gds__dyn_trg_inactive, (SSHORT) constant->nod_arg [0]);

if (constant = node->nod_arg [e_trg_position])
    put_number (request, gds__dyn_trg_sequence, (SSHORT) constant->nod_arg [0]);

if (constant = node->nod_arg [e_trg_type])
    {
    put_number (request, gds__dyn_trg_type, (SSHORT) constant->nod_arg [0]);
    trig_type = (USHORT) constant->nod_arg [0];
    }
else
    {
    assert (node->nod_type == nod_mod_trigger);
    }

if (actions)
    {
    /* create the "OLD" and "NEW" contexts for the trigger --
       the new one could be a dummy place holder to avoid resolving
       fields to that context but prevent relations referenced in
       the trigger actions from referencing the predefined "1" context */

    if (request->req_context_number)
	reset_context_stack (request);

    temp = relation_node->nod_arg [e_rln_alias];
    if ((trig_type != PRE_STORE_TRIGGER) && (trig_type != POST_STORE_TRIGGER))
	{
	relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (OLD_CONTEXT);
	PASS1_make_context (request, relation_node);
	}
    else
        request->req_context_number++;

    if ((trig_type != PRE_ERASE_TRIGGER) && (trig_type != POST_ERASE_TRIGGER))
	{
	relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (NEW_CONTEXT);
	PASS1_make_context (request, relation_node);
	}
    else
        request->req_context_number++;

    relation_node->nod_arg [e_rln_alias] = temp;

    /* generate the trigger blr */

    begin_blr (request, gds__dyn_trg_blr);
    STUFF (blr_begin);

    put_local_variables (request,
	node->nod_arg [e_trg_actions]->nod_arg [0], 0);

    request->req_scope_level++;
    GEN_statement (request, PASS1_statement (request, actions, 1));
    request->req_scope_level--;
    STUFF (blr_end);
    end_blr (request);

    /* the request type may have been set incorrectly when parsing
       the trigger actions, so reset it to reflect the fact that this
       is a data definition request; also reset the ddl node */

    request->req_type = REQ_DDL;
    }

if (temp = node->nod_arg [e_trg_messages])
    for (ptr = temp->nod_arg, end = ptr + temp->nod_count; ptr < end; ptr++)
	{
	message = *ptr;
	number = (SSHORT) message->nod_arg [e_msg_number];
	if (message->nod_type == nod_del_trigger_msg)
	    {
	    put_number (request, gds__dyn_delete_trigger_msg, number);
	    STUFF (gds__dyn_end);
	    }
	else
	    {
	    message_text = (STR) message->nod_arg [e_msg_text];
	    if (message->nod_type == nod_def_trigger_msg)
		put_number (request, gds__dyn_def_trigger_msg, number);
	    else
		put_number (request, gds__dyn_mod_trigger_msg, number);
	    put_string (request, gds__dyn_trg_msg, message_text->str_data,
			message_text->str_length);
	    STUFF (gds__dyn_end);
	    }
	}

STUFF (gds__dyn_end);
}

static void define_udf(
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ u d f
 *
 **************************************
 *
 * Function
 *	define a udf to the database.
 *
 **************************************/
NOD	*ptr, *end, *ret_val_ptr, arguments, udf_node;
UCHAR	*udf_name;
FLD	field;
SSHORT  position, blob_position;

udf_node = request->req_ddl_node;
arguments = udf_node->nod_arg [e_udf_args];
ptr = udf_node->nod_arg;
udf_name = ((STR) (ptr [e_udf_name]))->str_data;
put_cstring (request, gds__dyn_def_function, udf_name);
put_cstring (request, gds__dyn_func_entry_point, 
			((STR) (ptr [ e_udf_entry_pt]))->str_data);
put_cstring (request, gds__dyn_func_module_name, 
			((STR) (ptr [ e_udf_module]))->str_data);

ret_val_ptr = ptr [e_udf_return_value]->nod_arg;


if (field = (FLD) ret_val_ptr [0])
    {

    /* Some data types can not be returned as value */
       
    if ((ret_val_ptr[1]->nod_arg [0] == FUN_value) &&
	   (field->fld_dtype == dtype_text ||
            field->fld_dtype == dtype_varying ||
            field->fld_dtype == dtype_cstring ||
            field->fld_dtype == dtype_blob ||
            field->fld_dtype == dtype_timestamp))
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__return_mode_err,
                /* Return mode by value not allowed for this data type */
	    0);
    
    /* For functions returning a blob, coerce return argument position to
       be the last parameter. */
    
    if (field->fld_dtype == dtype_blob)
    	{
	blob_position = (arguments) ? arguments->nod_count + 1 : 1;
	if (blob_position > MAX_UDF_ARGUMENTS)
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
                gds_arg_gds, gds__dsql_command_err, 
                gds_arg_gds, gds__extern_func_err,
		/* External functions can not have more than 10 parameters */
		/* Or 9 if the function returns a BLOB */
		0);
		
    	put_number (request, gds__dyn_func_return_argument, blob_position);
	}
    else
    	put_number (request, gds__dyn_func_return_argument, (SSHORT) 0);
    
    position = 0;     
    }
else
    {
    position = (SSHORT) (ret_val_ptr [1]->nod_arg [0]);
    /* Function modifies an argument whose value is the function return value */

    if (!arguments || position > arguments->nod_count || position < 1)
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
	    gds_arg_gds, gds__dsql_command_err, 
	    gds_arg_gds, gds__extern_func_err,
	    /* External functions can not have more than 10 parameters */
	    /* Not strictly correct -- return position error */
	    0);
		
    put_number (request, gds__dyn_func_return_argument, position);
    position = 1;
    }

/* Now define all the arguments */
if (!position)
    {
    if (field->fld_dtype == dtype_blob)
    	{
        BOOLEAN free_it = ((SSHORT) ret_val_ptr [1]->nod_arg [0] < 0);
    	put_number (request, gds__dyn_def_function_arg, blob_position);
    	put_number (request, gds__dyn_func_mechanism, 
                     (SSHORT) ((free_it ? -1 : 1) * FUN_blob_struct) );
                     /* if we have the free_it set then the blob has
                        to be freed on return */
	}
    else
    	{
        put_number (request, gds__dyn_def_function_arg, (SSHORT) 0);
    	put_number (request, gds__dyn_func_mechanism, 
                       (SSHORT) (ret_val_ptr [1]->nod_arg [0]));
	}
    
    put_cstring (request, gds__dyn_function_name, udf_name);
    DDL_resolve_intl_type (request, field, NULL);
    put_field (request, field, TRUE);
    STUFF (gds__dyn_end);
    position = 1;
    }

assert (position == 1);
if (arguments)
    for (ptr = arguments->nod_arg, end = ptr + arguments->nod_count; 
	ptr < end; ptr++, position++)
        {
	if (position > MAX_UDF_ARGUMENTS)
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
                gds_arg_gds, gds__dsql_command_err, 
                gds_arg_gds, gds__extern_func_err,
		/* External functions can not have more than 10 parameters */
		0);
		
        field = (FLD) *ptr; 
	put_number (request, gds__dyn_def_function_arg, (SSHORT) position);

	if (field->fld_dtype == dtype_blob)
	    put_number (request, gds__dyn_func_mechanism, 
			 (SSHORT) FUN_blob_struct);
	else
	    put_number (request, gds__dyn_func_mechanism,(SSHORT) FUN_reference);

	put_cstring (request, gds__dyn_function_name, udf_name);
        DDL_resolve_intl_type (request, field, NULL);
        put_field (request, field, TRUE);
        STUFF (gds__dyn_end);
	}

STUFF (gds__dyn_end);
}


static void define_update_action (
    REQ		request,
    NOD		*base_and_node,
    NOD		*base_relation)
{
/* *************************************
 *
 *	d e f i n e _ u p d a t e _ a c t i o n
 *
 **************************************
 *
 * Function
 *	Define an action statement which, given a view 
 *	definition, will map a update to a  record from
 *	a view of a single relation into the 
 *	base relation.
 *
 **************************************/
NOD	ddl_node, eql_node, and_node, old_and;
NOD	select_node, select_expr, from_list, relation_node;
NOD	fields_node, values_node, field_node, value_node, old_value_node;
NOD	*ptr, *end, *ptr2, *end2;
NOD	iand_node, or_node, anull_node, bnull_node;
LLS	field_stack;
DSQL_REL	relation;
FLD	field;
SSHORT	and_arg = 0;

ddl_node = request->req_ddl_node;

/* check whether this is an updatable view definition */

if (ddl_node->nod_type != nod_def_view ||
    !(select_node = ddl_node->nod_arg [e_view_select]) ||
    /* 
       Handle VIEWS with UNION : nod_select now points to nod_list
       which in turn points to nod_select_expr
    */
    !(select_expr = select_node->nod_arg [0]->nod_arg [0]) ||
    !(from_list = select_expr->nod_arg [e_sel_from]) ||
    from_list->nod_count != 1)
    return;

/* use the relation referenced in the select statement for rse*/

relation_node = MAKE_node (nod_relation_name, (int) e_rln_count);
relation_node->nod_arg [e_rln_name] = from_list->nod_arg [0]->nod_arg [e_rln_name];
relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (TEMP_CONTEXT);
*base_relation = relation_node;

/* get the list of values and fields to compare to -- if there is 
   no list of fields, get all fields in the base relation that
   are not computed */

values_node = ddl_node->nod_arg [e_view_fields];
if (!(fields_node = select_expr->nod_arg [e_sel_list]))
    {
    relation = METD_get_relation (request, relation_node->nod_arg [e_rln_name]);
    field_stack = NULL;
    for (field = relation->rel_fields; field; field = field->fld_next)
	{
	if (field->fld_flags & FLD_computed)
	    continue;
	field_node = MAKE_node (nod_field_name, (int) e_fln_count);
	field_node->nod_arg [e_fln_name] = (NOD) MAKE_cstring (field->fld_name);
	LLS_PUSH (field_node, &field_stack);
	}
    fields_node = MAKE_list (field_stack);
    }
if (!values_node)
    values_node = fields_node;

/* generate the list of assignments to fields in the base relation */

ptr = fields_node->nod_arg; 
end = ptr + fields_node->nod_count; 
ptr2 = values_node->nod_arg; 
end2 = ptr2 + values_node->nod_count; 
field_stack = NULL;
and_node = MAKE_node (nod_and, (int) 2);
and_arg = 0;
for (; (ptr < end) && (ptr2 < end2); ptr++, ptr2++)
    {
    field_node = *ptr;
    if (field_node->nod_type == nod_alias)
	field_node = field_node->nod_arg [e_alias_value];

    /* generate the actual comparisons */ 
    
    if (field_node->nod_type == nod_field_name)
	{
	field_node->nod_arg [e_fln_context] = (NOD) MAKE_cstring (TEMP_CONTEXT);

	value_node = MAKE_node (nod_field_name, (int) e_fln_count);
	value_node->nod_arg [e_fln_name] = (*ptr2)->nod_arg [e_fln_name];
	value_node->nod_arg [e_fln_context] = (NOD) MAKE_cstring (NEW_CONTEXT);

	old_value_node = MAKE_node (nod_field_name, (int) e_fln_count);
	old_value_node->nod_arg [e_fln_name] = (*ptr2)->nod_arg [e_fln_name];
	old_value_node->nod_arg [e_fln_context] = (NOD) MAKE_cstring (OLD_CONTEXT);
	eql_node = MAKE_node (nod_eql, (int) 2);
	eql_node->nod_arg [0] = old_value_node; 
	eql_node->nod_arg [1] = field_node; 

	anull_node = MAKE_node (nod_missing, 1);
	anull_node->nod_arg [0] = old_value_node;
	bnull_node = MAKE_node (nod_missing, 1);
	bnull_node->nod_arg [0] = field_node;

	iand_node = MAKE_node (nod_and, (int) 2);
	iand_node->nod_arg[0] = anull_node;
	iand_node->nod_arg[1] = bnull_node;

	or_node = MAKE_node (nod_or, (int) 2);
	or_node->nod_arg [0] = eql_node;
	or_node->nod_arg [1] = iand_node;

	if (and_arg <= 1) 
	   and_node->nod_arg [and_arg++] = or_node;
	else
	   {
	   old_and = and_node;
	   and_node = MAKE_node (nod_and, (int) 2);
	   and_node->nod_arg [0] = old_and;
	   and_node->nod_arg [1] = or_node;
	   }
	}
    }

if (and_arg <= 1)
    and_node->nod_arg [and_arg] = select_expr->nod_arg [e_sel_where];
else
    {
    old_and = and_node;
    and_node = MAKE_node (nod_and, (int) 2);
    and_node->nod_arg [0] = old_and;
    and_node->nod_arg [1] = select_expr->nod_arg [e_sel_where];
    }
*base_and_node = and_node;
}

static void define_upd_cascade_trg (
    REQ         request,
    NOD         element,
    NOD		for_columns,
    NOD         prim_columns,
    TEXT        *prim_rel_name,
    TEXT        *for_rel_name
    )
{
/*****************************************************
 *
 *	d e f i n e _ u p d _ c a s c a d e _ t r g
 *
 *****************************************************
 *
 * Function
 *	define "on update cascade" trigger (for referential integrity)
 *      along with the trigger blr.
 *
 *****************************************************/
NOD *for_key_flds, *prim_key_flds;
USHORT num_fields = 0;
STR  for_key_fld_name_str,  prim_key_fld_name_str;

if (element->nod_type != nod_foreign)
    return;

/* count of foreign key columns */
assert (prim_columns->nod_count == for_columns->nod_count);
assert (prim_columns->nod_count != 0);

/* no trigger name is generated here. Let the engine make one up */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_number (request, gds__dyn_trg_type, (SSHORT) POST_MODIFY_TRIGGER);

STUFF(gds__dyn_sql_object);
put_number (request, gds__dyn_trg_sequence, (SSHORT)1);
put_number (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, prim_rel_name);

/* the trigger blr */
begin_blr (request, gds__dyn_trg_blr);

/* generate the trigger firing condition: foreign_key == primary_key */
stuff_trg_firing_cond (request, prim_columns);

STUFF (blr_begin);
STUFF (blr_begin);

STUFF (blr_for);
STUFF (blr_rse); 

/* the new context for the prim. key relation */
STUFF (1);

STUFF(blr_relation);
put_cstring (request, 0, for_rel_name);
/* the context for the foreign key relation */
STUFF (2);

/* generate the blr for: foreign_key == primary_key */
stuff_matching_blr (request, for_columns, prim_columns);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

num_fields = 0;
for_key_flds = for_columns->nod_arg;
prim_key_flds = prim_columns->nod_arg;

do {
   for_key_fld_name_str = (STR) (*for_key_flds)->nod_arg[1];
   prim_key_fld_name_str = (STR) (*prim_key_flds)->nod_arg[1];

   STUFF (blr_assignment);
   STUFF (blr_field); STUFF ((SSHORT)1);
   put_cstring (request, 0, prim_key_fld_name_str->str_data);
   STUFF (blr_field); STUFF ((SSHORT)2);
   put_cstring (request, 0, for_key_fld_name_str->str_data);

   num_fields++;
   prim_key_flds++;
   for_key_flds++;
   }
while (num_fields < for_columns->nod_count);

STUFF (blr_end);
STUFF (blr_end); STUFF (blr_end); STUFF (blr_end);
end_blr (request);
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);

}

static void define_view (
    REQ		request)
{
/**************************************
 *
 *	d e f i n e _ v i e w
 *
 **************************************
 *
 * Function
 *	Create the ddl to define a view, using a SELECT
 *	statement as the source of the view.
 *
 **************************************/
NOD	node, select, select_expr, rse, field_node;
NOD	check, relation_node;
NOD	view_fields, *ptr, *end;
NOD	items, *i_ptr, *i_end;
DSQL_REL	relation;
FLD	field;  
CTX	context;
STR	view_name, field_name, source;
SSHORT	position, updatable = TRUE;
TEXT	*field_string;
LLS	temp;

node = request->req_ddl_node;
view_name = (STR) node->nod_arg [e_view_name];
put_cstring (request, gds__dyn_def_view, view_name->str_data);
save_relation (request, view_name);
put_number (request, gds__dyn_rel_sql_protection, 1);

/* compile the SELECT statement into a record selection expression,
   making sure to bump the context number since view contexts start
   at 1 (except for computed fields)  -- note that calling PASS1_rse
   directly rather than PASS1_statement saves the context stack */

if (request->req_context_number)
    reset_context_stack (request);
request->req_context_number++;
select = node->nod_arg [e_view_select];
select_expr = select->nod_arg [0];
rse = PASS1_rse (request, select_expr, select->nod_arg [1]);

/* store the blr and source string for the view definition */

begin_blr (request, gds__dyn_view_blr);
GEN_expr (request, rse);
end_blr (request);

/* Store source for view. gdef -e cannot cope with it.
   We need to add something to rdb$views to indicate source type.
   Source will be for documentation purposes. */

source = (STR) node->nod_arg [e_view_source];
put_string (request, gds__dyn_view_source, source->str_data, source->str_length);

/* define the view source relations from the request contexts */

for (temp = request->req_context; temp; temp = temp->lls_next)
    {
    context = (CTX) temp->lls_object;
    if (relation = context->ctx_relation)
	{
	put_cstring (request, gds__dyn_view_relation, relation->rel_name);
	put_number (request, gds__dyn_view_context, context->ctx_context);
	put_cstring (request, gds__dyn_view_context_name, 
		context->ctx_alias ? context->ctx_alias : relation->rel_name);
	STUFF (gds__dyn_end);
	}
    }

/* if there are field names defined for the view, match them in order
   with the items from the SELECT.  Otherwise use all the fields from
   the rse node that was created from the select expression */

items = rse->nod_arg [e_rse_items];
i_ptr = items->nod_arg;
i_end = i_ptr + items->nod_count;

ptr = end = NULL;
if ((view_fields = node->nod_arg [e_view_fields]) != NULL)
    {
    ptr = view_fields->nod_arg;
    end = ptr + view_fields->nod_count;
    }

/* go through the fields list, defining the local fields;
   if an expression is specified rather than a field, define
   a global field for the computed value as well */

for (position = 0; i_ptr < i_end; i_ptr++, position++)
    {
    field_node = *i_ptr; 

    /* check if this is a field or an expression */
 
    field = NULL;
    context = NULL;
    if (field_node->nod_type == nod_field)
	{
	field = (FLD) field_node->nod_arg [e_fld_field];
	context = (CTX) field_node->nod_arg [e_fld_context];
	}
    else
	updatable = FALSE;

    /* if this is an expression, check to make sure there is a name specified */

    if (!ptr && !field)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__specify_field_err,
                /* must specify field name for view select expression */
	    0);

    /* determine the proper field name, replacing the default if necessary */

    if (field)
	field_string = field->fld_name;
    if (ptr && ptr < end)
	{
	field_name = (STR) (*ptr)->nod_arg [1];
	field_string = (TEXT*) field_name->str_data;
	ptr++;
	}

    /* if not an expression, point to the proper base relation field,
       else make up an SQL field with generated global field for calculations */

    if (field)
	{
	put_cstring (request, gds__dyn_def_local_fld, field_string);
	put_cstring (request, gds__dyn_fld_base_fld, field->fld_name);
	put_number (request, gds__dyn_view_context, context->ctx_context);
	}
    else
	{
	put_cstring (request, gds__dyn_def_sql_fld, field_string);
	MAKE_desc (&field_node->nod_desc, field_node);
	put_descriptor (request, &field_node->nod_desc); 
	begin_blr (request, gds__dyn_fld_computed_blr);
	GEN_expr (request, field_node);
	end_blr (request);
	put_number (request, gds__dyn_view_context, (SSHORT) 0);
	}

    save_field (request, field_string);

    put_number (request, gds__dyn_fld_position, position);
    STUFF (gds__dyn_end);
    }

if (ptr != end)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__num_field_err,
            /* number of fields does not match select list */
	0);
  
/* setup to define triggers for WITH CHECK OPTION */

if ((check = node->nod_arg [e_view_check]) != NULL) 
    {
    if (!updatable)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__col_name_err,
            /* Only simple column names permitted for VIEW WITH CHECK OPTION */
	0);

    if (select_expr->nod_count != 1)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__table_view_err,
                /* Only one table allowed for VIEW WITH CHECK OPTION */
	    0);
    /* 
       Handle VIEWS with UNION : nod_select now points to nod_list
       which in turn points to nod_select_expr
    */
    else if (select_expr->nod_arg[0]->nod_arg [e_sel_from]->nod_count != 1)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__table_view_err,
                /* Only one table allowed for VIEW WITH CHECK OPTION */
	    0);

    /* 
       Handle VIEWS with UNION : nod_select now points to nod_list
       which in turn points to nod_select_expr
    */
    select_expr = select_expr->nod_arg[0];
    if (!(select_expr->nod_arg [e_sel_where]))
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__where_err,
                /* No where clause for VIEW WITH CHECK OPTION */
	    0);
    

    if (select_expr->nod_arg [e_sel_distinct] ||
	select_expr->nod_arg [e_sel_group] ||
	select_expr->nod_arg [e_sel_having])
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__distinct_err,
            /* DISTINCT, GROUP or HAVING not permitted for VIEW WITH CHECK OPTION */
	    0);

    relation_node = MAKE_node (nod_relation_name, e_rln_count);
    relation_node->nod_arg [e_rln_name] = (NOD) view_name;
    check->nod_arg [e_cnstr_table] = relation_node; 

    check->nod_arg [e_cnstr_source] = (NOD) source;
 
    /* the condition for the trigger is the converse of the selection 
       criteria for the view, suitably fixed up so that the fields in 
       the view are referenced */

    check->nod_arg [e_cnstr_condition] = select_expr->nod_arg [e_sel_where]; 

    /* Define the triggers   */

    create_view_triggers (request, check, rse->nod_arg [e_rse_items]);
    }

STUFF (gds__dyn_end);
reset_context_stack(request);
}

static void define_view_trigger (
    REQ		request,
    NOD		node,
    NOD		rse,
    NOD		items)  /* The fields in VIEW actually  */
{
/**************************************
 *
 *	d e f i n e _ v i e w _ t r i g g e r
 *
 **************************************
 *
 * Function
 *	Create the ddl to define a trigger for a VIEW WITH CHECK OPTION.
 *
 **************************************/
STR	trigger_name, relation_name, message;
NOD	temp_rse, temp, ddl_node, actions, *ptr, *end, constant;
NOD	relation_node;
USHORT	trig_type;
NOD	action_node, condition, select, select_expr, view_fields;
CTX     context, sav_context = NULL_PTR;
LLS     stack;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

ddl_node = request->req_ddl_node;

select = ddl_node->nod_arg [e_view_select];
/* 
   Handle VIEWS with UNION : nod_select now points to nod_list
   which in turn points to nod_select_expr
*/
select_expr = select->nod_arg [0]->nod_arg [0];
view_fields = ddl_node->nod_arg [e_view_fields];

/* make the "define trigger" node the current request ddl node so
   that generating of BLR will be appropriate for trigger */

request->req_ddl_node = node;

trigger_name = (STR) node->nod_arg [e_cnstr_name];

if (node->nod_type == nod_def_constraint)
    {
    put_string (request, gds__dyn_def_trigger, trigger_name->str_data, 
			trigger_name->str_length);
    relation_node = node->nod_arg [e_cnstr_table];
    relation_name = (STR) relation_node->nod_arg [e_rln_name];
    put_string (request, gds__dyn_rel_name, relation_name->str_data, 
			relation_name->str_length);
    }
else
    return;

if ((constant = node->nod_arg [e_cnstr_position]) != NULL)
    put_number (request, gds__dyn_trg_sequence,
				constant ? (SSHORT) constant->nod_arg [0] : 0);

if ((constant = node->nod_arg [e_cnstr_type]) != NULL)
    {
    trig_type = (USHORT) constant->nod_arg [0];
    put_number (request, gds__dyn_trg_type, trig_type);
    }
else
    {
    /* If we don't have a trigger type assigned, then this is just a template
       definition for use with domains.  The real triggers are defined when
       the domain is used. */
    trig_type = 0;
    }

STUFF (gds__dyn_sql_object);

if ((message = (STR) node->nod_arg [e_cnstr_message]) != NULL)
    {
    put_number (request, gds__dyn_def_trigger_msg, 0);
    put_string (request, gds__dyn_trg_msg, message->str_data, message->str_length);
    STUFF (gds__dyn_end);
    }

/* generate the trigger blr */

if (node->nod_arg [e_cnstr_condition] && node->nod_arg [e_cnstr_actions])
    {
    begin_blr (request, gds__dyn_trg_blr);
    STUFF (blr_begin);

    /* create the "OLD" and "NEW" contexts for the trigger --
       the new one could be a dummy place holder to avoid resolving
       fields to that context but prevent relations referenced in
       the trigger actions from referencing the predefined "1" context */

    if (request->req_context_number)
        {
        /* If an alias is specified for the single base table involved,
           save and then add the context                               */

        stack = request->req_context;
        context = (CTX) stack->lls_object;
        if (context->ctx_alias)
            {
            sav_context = (CTX) ALLOCD (type_ctx);
            *sav_context = *context;
            }
        }
    reset_context_stack (request);
    temp = relation_node->nod_arg [e_rln_alias];
    relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (OLD_CONTEXT);
    PASS1_make_context (request, relation_node);
    relation_node->nod_arg [e_rln_alias] = (NOD) MAKE_cstring (NEW_CONTEXT);
    PASS1_make_context (request, relation_node);
    relation_node->nod_arg [e_rln_alias] = temp;

    if (sav_context)
        {
        sav_context->ctx_context = request->req_context_number++;
        context->ctx_scope_level = request->req_scope_level;
        LLS_PUSH (sav_context, &request->req_context);
        }

    if (trig_type == PRE_MODIFY_TRIGGER)
	{
	STUFF (blr_for);
	temp = rse->nod_arg [e_rse_streams];
	temp->nod_arg [0] = PASS1_node (request, temp->nod_arg [0], 0);
	temp = rse->nod_arg [e_rse_boolean];
	rse->nod_arg [e_rse_boolean] = PASS1_node (request, temp, 0); 
	GEN_expr (request, rse);

	condition = MAKE_node (nod_not, 1);
	condition->nod_arg [0] = replace_field_names (select_expr->nod_arg [e_sel_where], items, view_fields, FALSE);
	STUFF (blr_begin);
	STUFF (blr_if);
	GEN_expr (request, PASS1_node (request, condition->nod_arg [0], 0));
	STUFF (blr_begin);
	STUFF (blr_end);
	}

    if (trig_type == PRE_STORE_TRIGGER)
	{
	condition = MAKE_node (nod_not, 1);
	condition->nod_arg [0] = replace_field_names (select_expr->nod_arg [e_sel_where], items, view_fields, TRUE);
	STUFF (blr_if);
	GEN_expr (request, PASS1_node (request, condition->nod_arg[0], 0));
	STUFF (blr_begin);
	STUFF (blr_end);
	}

    /* generate the action statements for the trigger */

    actions = node->nod_arg [e_cnstr_actions];
    for (ptr = actions->nod_arg, end = ptr + actions->nod_count; ptr < end; ptr++)
	GEN_statement (request, PASS1_statement (request, *ptr, 0));

    /* generate the action statements for the trigger */

    if ((actions = node->nod_arg [e_cnstr_else]) != NULL)
	{
	STUFF (blr_begin);
	for (ptr = actions->nod_arg, end = ptr + actions->nod_count; ptr < end; ptr++)
	    {
	    action_node = PASS1_statement (request, *ptr, 0);
	    if (action_node->nod_type == nod_modify)
		{
		temp_rse = action_node->nod_arg [e_mod_rse];
		temp_rse->nod_arg [e_rse_first] = MAKE_constant ((STR) 1,1);
		}
	    GEN_statement(request, action_node);
	    }
	STUFF (blr_end); /* of begin */
	}

    STUFF (blr_end); /* of if */
    if (trig_type == PRE_MODIFY_TRIGGER)
	STUFF (blr_end); /* of for  */
    end_blr (request);
    }

STUFF (gds__dyn_end);
			    
/* the request type may have been set incorrectly when parsing
   the trigger actions, so reset it to reflect the fact that this
   is a data definition request; also reset the ddl node */

request->req_type = REQ_DDL;
request->req_ddl_node = ddl_node;
reset_context_stack (request);
}

static void end_blr (
    REQ		request)
{
/**************************************
 *
 *	e n d _ b l r
 *
 **************************************
 *
 * Function
 *	Complete the stuffing of a piece of
 *	blr by going back and inserting the length.
 *
 **************************************/
UCHAR	*blr_base;
USHORT	length;

STUFF (blr_eoc);

/* go back and stuff in the proper length */

blr_base = request->req_blr_string->str_data + request->req_base_offset;
length = (SSHORT) (request->req_blr - blr_base) - 2;
*blr_base++ = length;
*blr_base = length >> 8;
}

static void foreign_key (
    REQ		request,
    NOD		element)
{
/* *************************************
 *
 *	f o r e i g n _ k e y
 *
 **************************************
 *
 * Function
 *	Generate ddl to create a foreign key
 *	constraint.
 *
 **************************************/
NOD	relation2_node;
NOD	columns1, columns2;
STR	relation2;

columns1 = element->nod_arg [e_for_columns];

relation2_node = element->nod_arg [e_for_reftable];
relation2 = (STR) relation2_node->nod_arg [e_rln_name];

/* If there is a referenced table name but no referenced field names, the
   primary key of the referenced table designates the referenced fields. */
if (!(columns2 = element->nod_arg [e_for_refcolumns]))
{
    element->nod_arg [e_for_refcolumns] =
      columns2 = METD_get_primary_key (request, relation2);

    /* If there is NEITHER an explicitly referenced field name, NOR does
       the referenced table have a primary key to serve as the implicitly
       referenced field, fail. */
    if (! columns2)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__reftable_requires_pk,
                /* "REFERENCES table" without "(column)" requires PRIMARY
                    KEY on referenced table */
	    0);
}

if (columns2 && (columns1->nod_count != columns2->nod_count))
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__key_field_count_err,
            /* foreign key field count does not match primary key */
	0);

/* define the foreign key index and the triggers that may be needed
   for referential integrity action. */

make_index_trg_ref_int (request, element, columns1, 
   element->nod_arg [e_for_refcolumns], relation2->str_data);
}

static void generate_dyn (
    REQ		request,   
    NOD		node)
{
/**************************************
 *
 *	g e n e r a t e _ d y n
 *
 **************************************
 *
 * Functional description
 *	Switch off the type of node to generate a
 *	DYN string.
 *
 **************************************/
STR	string;

request->req_ddl_node = node;

switch (node->nod_type)
    {
    case nod_def_domain:
        define_domain (request);
        break;

    case nod_mod_domain:
	modify_domain (request);
        break;

    case nod_def_index:
	define_index (request);
	break;

    case nod_def_relation:
	define_relation (request);
	break;

    case nod_def_view:
	define_view (request);
	break;

    case nod_def_exception:
    case nod_mod_exception:
    case nod_del_exception:
	define_exception (request, node->nod_type);
	break;

    case nod_def_procedure:
    case nod_mod_procedure:
	define_procedure (request, node->nod_type);
	break;

    case nod_def_constraint:
	define_constraint_trigger (request, node);
	break;

    case nod_def_trigger:
    case nod_mod_trigger:
	define_trigger (request, node);
	break;

    case nod_mod_relation:
	modify_relation (request);
	break;

    case nod_del_domain:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_global_fld, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_del_index:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_idx, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_del_relation:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_rel, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_del_procedure:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_procedure, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_del_trigger:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_trigger, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_del_role:
	string = (STR) node->nod_arg [0];
	put_cstring (request, isc_dyn_del_sql_role, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_grant:
    case nod_revoke:
	grant_revoke (request);
	break;

    case nod_def_generator:
	define_generator (request);
	break;

    case nod_def_role:
        define_role (request);
        break;

    case nod_def_filter:
	define_filter (request);
	break;

    case nod_del_generator:
	string = (STR) node->nod_arg [0];
	/**********FIX -- nothing like delete_generator exists as yet
	put_cstring (request, gds__dyn_def_generator, string->str_data);
	STUFF (gds__dyn_end); */
	break; 

    case nod_del_filter:
	string = (STR) node->nod_arg [0];
	put_cstring (request, gds__dyn_delete_filter, string->str_data);
	STUFF (gds__dyn_end);
	break;

    case nod_def_udf:
	define_udf (request);
        break;

    case nod_del_udf:
        string = (STR) node->nod_arg [0];
        put_cstring (request, gds__dyn_delete_function, string->str_data);
        STUFF (gds__dyn_end);
        break;

    case nod_def_shadow:
	define_shadow (request);
	break;

    case nod_del_shadow:
        put_number (request, gds__dyn_delete_shadow, 
			(SSHORT) (node->nod_arg[0]));
        STUFF (gds__dyn_end);
        break;

    case nod_mod_database:
	modify_database (request);
	break;

    case nod_def_database:
	define_database (request);
	break;

    case nod_mod_index:
	modify_index (request);
	break;

    case nod_set_statistics:
	set_statistics (request);
	break;
    }
}

static void grant_revoke (
    REQ		request)
{
/**************************************
 *
 *	g r a n t _ r e v o k e
 *
 **************************************
 *
 * Functional description
 *	Build DYN string for GRANT/REVOKE statements
 *
 **************************************/
NOD	ddl_node, privs, table;
NOD	users, *uptr, *uend;
SSHORT	option;
NOD     role_list, *role_ptr, *role_end;
BOOLEAN process_grant_role = FALSE;

option = FALSE;
ddl_node = request->req_ddl_node;

privs = ddl_node->nod_arg [e_grant_privs];

if (privs->nod_arg [0] != NULL)
    { 
    if (privs->nod_arg [0]->nod_type == nod_role_name)   
        process_grant_role = TRUE; 
    }

if (!process_grant_role)
{
table = ddl_node->nod_arg [e_grant_table];
users = ddl_node->nod_arg [e_grant_users];
if (ddl_node->nod_arg [e_grant_grant])
    option = TRUE;

STUFF (gds__dyn_begin);

for (uptr = users->nod_arg, uend = uptr + users->nod_count; uptr < uend; uptr++)
    modify_privileges (request, ddl_node->nod_type, option, privs,
			table, *uptr);
}
else
    {
    role_list = ddl_node->nod_arg [0];
    users     = ddl_node->nod_arg [1];
    if (ddl_node->nod_arg [3])
        option = 2;
    STUFF (isc_dyn_begin);

    for (role_ptr = role_list->nod_arg, 
         role_end = role_ptr + role_list->nod_count; 
         role_ptr < role_end; role_ptr++)
        {
        for (uptr = users->nod_arg, uend = uptr + users->nod_count;
             uptr < uend; uptr++)
            {
            process_role_nm_list (request, option, *uptr, *role_ptr,
                                  ddl_node->nod_type);
            }
        }
    }

STUFF (gds__dyn_end);
}

static void make_index (
    REQ		request,
    NOD		element,
    NOD		columns,
    NOD		referenced_columns,
    TEXT	*relation_name)
{
/* *************************************
 *
 *	m a k e _ i n d e x
 *
 **************************************
 *
 * Function
 *	Generate ddl to create an index for a unique 
 *	or primary key constraint. 
 *      This is not called for a foreign key constraint.
 *      The func. make_index_trf_ref_int handles foreign key constraint
 *
 **************************************/
NOD	*ptr, *end;
STR	field_name;

/* stuff a zero-length name, indicating that an index
   name should be generated */

assert(element->nod_type != nod_foreign)

if (element->nod_type == nod_primary)
    STUFF (gds__dyn_def_primary_key); 
else if (element->nod_type == nod_unique)
    STUFF (gds__dyn_def_unique);
STUFF_WORD (0);

put_number (request, gds__dyn_idx_unique, 1);

for (ptr = columns->nod_arg, end = ptr + columns->nod_count; ptr < end; ptr++)
    {
    field_name = (STR) (*ptr)->nod_arg [1];
    put_cstring (request, gds__dyn_fld_name, field_name->str_data);
    }

STUFF (gds__dyn_end);
}

static void make_index_trg_ref_int (
    REQ		request,
    NOD		element,
    NOD		columns,
    NOD		referenced_columns,
    TEXT	*relation_name)
{
/******************************************************
 *
 *	m a k e _ i n d e x _ t r g _ r e f _ i n t
 *
 ******************************************************
 *
 * Function
 *      This is called only when the element->nod_type == nod_foreign_key
 *      
 *     o Generate ddl to create an index for a unique
 *       or primary key constraint.
 *     o Also make an index for the foreign key constraint
 *     o in the caase of foreign key, also generate an appropriate trigger for
 *       cascading referential integrity.
 *
 *
 *****************************************************/
NOD	*ptr, *end;
STR	field_name;
NOD     for_rel_node, ddl_node;
STR     for_rel_name_str;
NOD     nod_for_action, nod_ref_upd_action, nod_ref_del_action;

assert (element->nod_type == nod_foreign)

/* for_rel_name_str is the name of the relation on which the ddl operation
   is being done, in this case the foreign key table  */
 
ddl_node = request->req_ddl_node;
for_rel_node = ddl_node->nod_arg [e_drl_name];
for_rel_name_str = (STR) for_rel_node->nod_arg [e_rln_name];


/* stuff a zero-length name, indicating that an index
   name should be generated */

STUFF (gds__dyn_def_foreign_key); 
STUFF_WORD (0);


if (element->nod_arg [e_for_action])
    {
    nod_for_action = element->nod_arg[e_for_action];
    assert (nod_for_action->nod_type == nod_ref_upd_del);

    nod_ref_upd_action = nod_for_action->nod_arg[e_ref_upd];
    if (nod_ref_upd_action)
        {
        assert (nod_ref_upd_action->nod_type == nod_ref_trig_action);

        STUFF (gds__dyn_foreign_key_update);
        switch (nod_ref_upd_action->nod_flags)
            {
            case REF_ACTION_CASCADE:
                STUFF (gds__dyn_foreign_key_cascade);
    	        define_upd_cascade_trg (request, element, columns,
    	            referenced_columns, relation_name, 
    	            for_rel_name_str->str_data);
    	        break;
            case REF_ACTION_SET_DEFAULT:
                STUFF (gds__dyn_foreign_key_default);
                define_set_default_trg (request, element, columns,
                    referenced_columns, relation_name,
                    for_rel_name_str->str_data, TRUE);
    	        break;
            case REF_ACTION_SET_NULL:
                STUFF (gds__dyn_foreign_key_null);
    	        define_set_null_trg (request, element, columns,
    	            referenced_columns, relation_name, 
    	            for_rel_name_str->str_data, TRUE);
    	        break;
            case REF_ACTION_NONE:
    	        STUFF (gds__dyn_foreign_key_none);
    	        break;
            default:
    	        assert (0);
    	        STUFF (gds__dyn_foreign_key_none); /* just in case */
    	        break;
            }
        }

    nod_ref_del_action = nod_for_action->nod_arg[e_ref_del];
    if (nod_ref_del_action)
        {
        assert (nod_ref_del_action->nod_type == nod_ref_trig_action);

        STUFF (gds__dyn_foreign_key_delete);
        switch (nod_ref_del_action->nod_flags)
            {
            case REF_ACTION_CASCADE:
                STUFF (gds__dyn_foreign_key_cascade);
    	        define_del_cascade_trg (request, element, columns,
    	            referenced_columns, relation_name, 
    	            for_rel_name_str->str_data);
    	        break;
            case REF_ACTION_SET_DEFAULT:
                STUFF (gds__dyn_foreign_key_default);
                define_set_default_trg (request, element, columns,
                    referenced_columns, relation_name,
                    for_rel_name_str->str_data, FALSE);
    	        break;
            case REF_ACTION_SET_NULL:
                STUFF (gds__dyn_foreign_key_null);
    	        define_set_null_trg (request, element, columns,
    	            referenced_columns, relation_name, 
    	            for_rel_name_str->str_data, FALSE);
    	        break;
            case REF_ACTION_NONE:
    	        STUFF (gds__dyn_foreign_key_none);
    	        break;
            default:
    	        assert (0);
    	        STUFF (gds__dyn_foreign_key_none); /* just in case */
    	        break;
    	        /* Error */
            }
        }
    }


for (ptr = columns->nod_arg, end = ptr + columns->nod_count; ptr < end; ptr++)
    {
    field_name = (STR) (*ptr)->nod_arg [1];
    put_cstring (request, gds__dyn_fld_name, field_name->str_data);
    }

put_cstring (request, gds__dyn_idx_foreign_key, relation_name);
if (referenced_columns)
    for (ptr = referenced_columns->nod_arg, end = ptr + referenced_columns->nod_count; ptr < end; ptr++)
    {
    field_name = (STR) (*ptr)->nod_arg [1];
    put_cstring (request, gds__dyn_idx_ref_column, field_name->str_data);
    }

STUFF (gds__dyn_end);
}

static void modify_database (
    REQ		request)
{
/**************************************
 *
 *	m o d i f y _ d a t a b a s e
 *
 **************************************
 *
 * Function
 *	Modify a database. 
 *
 **************************************/
NOD	ddl_node, elements, element, *ptr, *end;
SLONG	start = 0;
FIL	file;
SSHORT 	number = 0;
SLONG	temp_long;
SSHORT	temp_short;
SSHORT   drop_log = FALSE;
SSHORT   drop_cache = FALSE;

ddl_node = request->req_ddl_node;

STUFF (gds__dyn_mod_database);
/*
put_number (request, gds__dyn_rel_sql_protection, 1);
*/
elements = ddl_node->nod_arg [e_adb_all];
for (ptr = elements->nod_arg, end = ptr + elements->nod_count;
	ptr < end; ptr++)
    {
    element = *ptr; 
    switch (element->nod_type)
	{
	case nod_drop_log:
		drop_log = TRUE;
		break;
	case nod_drop_cache:
		drop_cache = TRUE;
		break;
	}
    }

if (drop_log)
    STUFF (gds__dyn_drop_log);
if (drop_cache)
    STUFF (gds__dyn_drop_cache);
		
elements = ddl_node->nod_arg [e_adb_all];
for (ptr = elements->nod_arg, end = ptr + elements->nod_count;
	ptr < end; ptr++)
    {
    element = *ptr; 

    switch (element->nod_type)
	{
	case nod_file_desc:
	    file = (FIL) element->nod_arg [0];
	    put_cstring (request, gds__dyn_def_file, file->fil_name->str_data);
	    STUFF (gds__dyn_file_start);
	    STUFF_WORD (4);
	    start = MAX (start, file->fil_start);
	
	    STUFF_DWORD (start) ;
	
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF(gds__dyn_end);
	    start += file->fil_length;
	    break;

	case nod_log_file_desc:
	    file = (FIL) element->nod_arg [0];
	
	    if (file->fil_flags & LOG_default)
		{
		STUFF (gds__dyn_def_default_log);
		break;
		}
	    put_cstring (request, gds__dyn_def_log_file, 
		    file->fil_name->str_data);
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF (gds__dyn_log_file_sequence);
	    STUFF_WORD (2);
	    STUFF_WORD (number);
	    number++;
	    STUFF (gds__dyn_log_file_partitions);
	    STUFF_WORD (2);
	    STUFF_WORD (file->fil_partitions);
	    if (file->fil_flags & LOG_serial)
	    	STUFF (gds__dyn_log_file_serial);
	    if (file->fil_flags & LOG_overflow)
	    	STUFF (gds__dyn_log_file_overflow);
	    if (file->fil_flags & LOG_raw)
	    	STUFF (gds__dyn_log_file_raw);
	    STUFF(gds__dyn_end);
	    break;

	case nod_cache_file_desc:
	    file = (FIL) element->nod_arg [0];
	    put_cstring (request, gds__dyn_def_cache_file,
		file->fil_name->str_data);
	    STUFF (gds__dyn_file_length);
	    STUFF_WORD (4);
	    STUFF_DWORD (file->fil_length);
	    STUFF(gds__dyn_end);
	    break;

	case nod_group_commit_wait:
	    STUFF (gds__dyn_log_group_commit_wait);
	    temp_long = (SLONG) (element->nod_arg [0]);
	    STUFF_WORD (4);
	    STUFF_DWORD (temp_long);
	    break;

	case nod_check_point_len:
	    STUFF (gds__dyn_log_check_point_length);
	    temp_long = (SLONG) (element->nod_arg [0]);
	    STUFF_WORD (4);
	    STUFF_DWORD (temp_long);
	    break;

	case nod_num_log_buffers:
	    STUFF (gds__dyn_log_num_of_buffers);
	    temp_short = (SSHORT) (element->nod_arg [0]);
	    STUFF_WORD (2);
	    STUFF_WORD (temp_short);
	    break;

	case nod_log_buffer_size:
	    STUFF (gds__dyn_log_buffer_size);
	    temp_short = (SSHORT) (element->nod_arg [0]);
	    STUFF_WORD (2);
	    STUFF_WORD (temp_short);
	    break;
	case nod_drop_log:
	case nod_drop_cache:
	    break;
	}
    }

STUFF (gds__dyn_end);
}

static void modify_domain (
    REQ		request)
{
/**************************************
 *
 *	m o d i f y _ d o m a i n 
 *
 **************************************
 *
 * Function
 *	Alter an SQL domain.
 *
 **************************************/
NOD	ddl_node, domain_node, ops, element, *ptr, *end;
STR	string, domain_name;
FLD	field;
struct fld local_field;

ddl_node = request->req_ddl_node;

domain_node = ddl_node->nod_arg [e_alt_dom_name];
domain_name = (STR) domain_node->nod_arg [e_fln_name];  

put_cstring (request, gds__dyn_mod_global_fld, domain_name->str_data);

ops = ddl_node->nod_arg [e_alt_dom_ops];
for (ptr = ops->nod_arg, end = ptr + ops->nod_count; ptr < end; ptr++)
    {
    element = *ptr; 
    switch (element->nod_type)
	{
	case nod_def_default:
    	    begin_blr (request, gds__dyn_fld_default_value);
            GEN_expr (request, element->nod_arg[e_dft_default]);
            end_blr (request);
            if ((string = (STR) element->nod_arg [e_dft_default_source]) != NULL)
            put_string (request, gds__dyn_fld_default_source, string->str_data,
                                                string->str_length);
	    break;

	case nod_def_constraint:
            STUFF (gds__dyn_single_validation);
            begin_blr (request, gds__dyn_fld_validation_blr);
            
	    /* Get the attributes of the domain, and set any occurances of
	       nod_dom_value (corresponding to the keyword VALUE) to the
	       correct type, length, scale, etc. */
	    if (! METD_get_domain (request, &local_field,
				   domain_name->str_data))
	        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
			   gds_arg_gds, gds__dsql_command_err, 
			   gds_arg_gds, gds__dsql_domain_not_found, 
		      /* Specified domain or source field does not exist */
			   0);
	    if(element->nod_arg [e_cnstr_condition])
	        set_nod_value_attributes (element->nod_arg [e_cnstr_condition],
					  &local_field);
	    
	    /* Increment the context level for this request, so that
	       the context number for any RSE generated for a SELECT
	       within the CHECK clause will be greater than 0.  In the
	       environment of a domain check constraint, context
	       number 0 is reserved for the "blr_fid, 0, 0,0," which
	       is emitted for a nod_dom_value, corresponding to an
	       occurance of the VALUE keyword in the body of the check
	       constraint.  -- chrisj 1999-08-20 */
	    
	    request->req_context_number++;

	    GEN_expr (request,
            PASS1_node (request, element->nod_arg [e_cnstr_condition], 0));
	    
            end_blr (request);
            if ((string = (STR) element->nod_arg [e_cnstr_source]) != NULL)
                 put_string (request, gds__dyn_fld_validation_source,
                             string->str_data, string->str_length);
	    break;

	case nod_mod_domain_type:
	    field = (FLD) element->nod_arg[e_mod_dom_new_dom_type];
    	    DDL_resolve_intl_type (request, field, NULL);
            put_field (request, field, FALSE);
	    break;

	case nod_field_name:
           {
	    NOD nod_new_domain;
	    STR new_dom_name;

	    new_dom_name =  (STR) element->nod_arg [e_fln_name];
	    put_cstring (request, gds__dyn_fld_name, new_dom_name->str_data);
	    break;
            }

	case nod_delete_rel_constraint:
            STUFF (gds__dyn_del_validation);
	    break;

	case nod_del_default:
           STUFF (gds__dyn_del_default);
	   break;
	}
    }

STUFF (gds__dyn_end);
}

static void modify_index (
    REQ		request)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Function
 *	Alter an index (only active or inactive for now)
 *
 **************************************/
NOD	ddl_node, index_node;
STR	index_name;

ddl_node = request->req_ddl_node;

index_node = ddl_node->nod_arg [e_alt_index];
index_name = (STR) index_node->nod_arg [e_alt_idx_name];  

put_cstring (request, gds__dyn_mod_idx, index_name->str_data);

if (index_node->nod_type == nod_idx_active)
    put_number (request, gds__dyn_idx_inactive, 0);
else if (index_node->nod_type == nod_idx_inactive)
    put_number (request, gds__dyn_idx_inactive, 1);

STUFF (gds__dyn_end);
}

static void modify_privilege (
    REQ		request,
    NOD_TYPE	type,
    SSHORT	option,
    UCHAR	*privs,
    NOD		table,
    NOD		user,
    STR		field_name)
{
/**************************************
 *
 *	m o d i f y _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *	Stuff a single grant/revoke verb and all its options.
 *
 **************************************/
SSHORT	priv_count, i;
UCHAR	*dynsave;
STR	name;

if (type == nod_grant)
    STUFF (gds__dyn_grant);
else
    STUFF (gds__dyn_revoke);

/* stuff the privileges string */

priv_count = 0;
STUFF_WORD (0);
for (; *privs; privs++)
    {
    priv_count++;
    STUFF (*privs);  
    }

dynsave = request->req_blr;
for (i = priv_count + 2; i; i--)
    --dynsave;

*dynsave++ = priv_count;
*dynsave = priv_count >> 8;
    
name = (STR) table->nod_arg [0];
if (table->nod_type == nod_procedure_name)
    put_cstring (request, gds__dyn_prc_name, name->str_data);
else
    put_cstring (request, gds__dyn_rel_name, name->str_data);
name = (STR) user->nod_arg [0];
switch (user->nod_type)
    {
    case nod_user_group:		/* GRANT priv ON tbl TO GROUP unix_group */
        put_cstring (request, isc_dyn_grant_user_group, name->str_data);
        break;

    case nod_user_name:
	put_cstring (request, gds__dyn_grant_user, name->str_data);
	break;

    case nod_proc_obj:
	put_cstring (request, gds__dyn_grant_proc, name->str_data);
	break;

    case nod_trig_obj:
	put_cstring (request, gds__dyn_grant_trig, name->str_data);
	break;

    case nod_view_obj:
	put_cstring (request, gds__dyn_grant_view, name->str_data);
	break;
    }

if (field_name)
    put_cstring (request, gds__dyn_fld_name, field_name->str_data);

if ((option) && ((type == nod_grant) ||
    (!(request->req_dbb->dbb_flags & DBB_v3))))
    put_number (request, gds__dyn_grant_options, option);
    
STUFF (gds__dyn_end);
}


static SCHAR modify_privileges (
    REQ		request,
    NOD_TYPE	type,
    SSHORT	option,
    NOD		privs,
    NOD		table,
    NOD		user)
{
/**************************************
 *
 *	m o d i f y _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *     Return a SCHAR indicating the privilege to be modified
 *
 **************************************/
SCHAR	privileges [10], *p;
NOD	fields, *ptr, *end;

switch (privs->nod_type)
    {
    case nod_all:
	p = "A";	
	break;

    case nod_select:
	return 'S';

    case nod_execute:
	return 'X';

    case nod_insert:
	return 'I';

    case nod_references:
    case nod_update:    
	p = (privs->nod_type == nod_references) ? "R" : "U";
	if (!(fields = privs->nod_arg [0]))
	    return *p;

	for (ptr = fields->nod_arg, end = ptr + fields->nod_count; ptr < end; ptr++)
	    modify_privilege (request, type, option, p, table, user, (*ptr)->nod_arg [1]);
	return 0;

    case nod_delete:
	return 'D';

    case nod_list:
	p = privileges;
	for (ptr = privs->nod_arg, end = ptr + privs->nod_count; ptr < end; ptr++)
	    if (*p = modify_privileges (request, type, option, *ptr, table, user))
		p++;
	*p = 0;
	p = privileges;
	break;
    }

if (*p)
    modify_privilege (request, type, option, p, table, user, NULL_PTR);

return 0;
}

static void modify_relation (
    REQ		request)
{
/**************************************
 *
 *	m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Function
 *	Alter an SQL table, relying on DYN to generate
 *	global fields for the local fields. 
 *
 **************************************/
NOD	ddl_node, ops, element, *ptr, *end, relation_node, field_node;
STR	relation_name, field_name;
FLD	field;
JMP_BUF	*old_env, env;
TSQL	tdsql;


tdsql = GET_THREAD_DATA;

ddl_node = request->req_ddl_node;

relation_node = ddl_node->nod_arg [e_alt_name];
relation_name = (STR) relation_node->nod_arg [e_rln_name];  

put_cstring (request, gds__dyn_mod_rel, relation_name->str_data);
save_relation (request, relation_name);

/* need to handle error that occur in generating dyn string.
 * If there is an error, get rid of the cached data
 */

old_env = (JMP_BUF *) tdsql->tsql_setjmp;
tdsql->tsql_setjmp = (UCHAR *) env;

if (SETJMP (tdsql->tsql_setjmp))
    {
    METD_drop_relation (request, relation_name);
    request->req_relation = NULL_PTR;
    tdsql->tsql_setjmp = (UCHAR *) old_env;
    LONGJMP (tdsql->tsql_setjmp, (int) tdsql->tsql_status [1]);
    }

ops = ddl_node->nod_arg [e_alt_ops];
for (ptr = ops->nod_arg, end = ptr + ops->nod_count; ptr < end; ptr++)
    {
    element = *ptr; 
    switch (element->nod_type)
	{
	case nod_mod_field_name:
	    {
	    NOD old_field, new_field;
	    STR old_field_name, new_field_name;

	    old_field = element->nod_arg [e_mod_fld_name_orig_name];
	    old_field_name =  (STR) old_field->nod_arg [e_fln_name];
	    put_cstring (request, gds__dyn_mod_local_fld, old_field_name->str_data);

	    new_field = element->nod_arg [e_mod_fld_name_new_name];
	    new_field_name =  (STR) new_field->nod_arg [e_fln_name];
	    put_cstring (request, gds__dyn_rel_name, relation_name->str_data);
	    put_cstring (request, isc_dyn_new_fld_name, new_field_name->str_data);
	    STUFF (gds__dyn_end);
	    break;
	    }

	case nod_mod_field_pos:
	    {
	    SSHORT constant;
	    NOD const_node;

	    field_node = element->nod_arg [e_mod_fld_pos_orig_name];
	    field_name =  (STR) field_node->nod_arg [e_fln_name];
	    put_cstring (request, gds__dyn_mod_local_fld, field_name->str_data);
	
            const_node = element->nod_arg [e_mod_fld_pos_new_position];
	    constant = (SSHORT) const_node->nod_arg [0];
	    put_cstring (request, gds__dyn_rel_name, relation_name->str_data);
	    put_number (request, gds__dyn_fld_position, constant);
	    STUFF (gds__dyn_end);
	    break;
	    }


	case nod_mod_field_type:
	    modify_field (request, element, (SSHORT) -1, relation_name);
	    break;

	case nod_def_field:
	    define_field (request, element, (SSHORT) -1, relation_name);
	    break;

	case nod_del_field:

	    /* Fix for bug 8054:

               [CASCADE | RESTRICT] syntax is available in IB4.5, but not
               required until v5.0. 

               Option CASCADE causes an error :
                 unsupported DSQL construct

               Option RESTRICT is default behaviour.
            */

	    field_node = element->nod_arg [0];
	    field_name =  (STR) field_node->nod_arg [e_fln_name];

	    if ( (element->nod_arg [1])->nod_type == nod_cascade )
                /* Unsupported DSQL construct */
	        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901,
	                   gds_arg_gds, gds__dsql_command_err,
			   gds_arg_gds, gds__dsql_construct_err, 0);

	    assert( (element->nod_arg [1])->nod_type == nod_restrict );
	    put_cstring (request, gds__dyn_delete_local_fld, field_name->str_data);
	    STUFF (gds__dyn_end);
	    break; 

	case nod_delete_rel_constraint:
	    field_name = (STR) element->nod_arg [0];
	    put_cstring (request, gds__dyn_delete_rel_constraint,
			 field_name->str_data);
	    break;

	case nod_rel_constraint:
	   define_rel_constraint (request, element);
	   break;
	}
    }

STUFF (gds__dyn_end);

/* restore the setjmp pointer */

tdsql->tsql_setjmp = (UCHAR *) old_env;
}

static void process_role_nm_list (
    REQ         request,
    SSHORT      option,
    NOD         user_ptr,
    NOD         role_ptr,
    NOD_TYPE    type)
{
/**************************************
 *
 *  p r o c e s s _ r o l e _ n m _ l i s t
 *
 **************************************
 *
 * Functional description
 *     Build req_blr for grant & revoke role stmt
 *
 **************************************/
STR     role_nm, user_nm;

if (type == nod_grant)
    STUFF (isc_dyn_grant);
else
    STUFF (isc_dyn_revoke);

STUFF_WORD (1);
STUFF ('M');

role_nm = (STR) role_ptr->nod_arg [0];
put_cstring (request, isc_dyn_sql_role_name, role_nm->str_data);

user_nm = (STR) user_ptr->nod_arg [0];
put_cstring (request, isc_dyn_grant_user, user_nm->str_data);

if (option)
    put_number (request, isc_dyn_grant_admin_options, option);

STUFF (gds__dyn_end);
}

static void put_descriptor (
    REQ	request,
    DSC		*desc)
{
/**************************************
 *
 *	p u t _ d e s c r i p t o r
 *
 **************************************
 *
 * Function
 *	Write out field description in ddl, given the
 *	input descriptor.
 *
 **************************************/

put_number (request, gds__dyn_fld_type, blr_dtypes [desc->dsc_dtype]);
if (desc->dsc_dtype == dtype_varying)
    put_number (request, gds__dyn_fld_length, desc->dsc_length - sizeof (USHORT));
else
    put_number (request, gds__dyn_fld_length, desc->dsc_length);
put_number (request, gds__dyn_fld_scale, desc->dsc_scale);
}

static void put_dtype (
    REQ		request,
    FLD		field,
    USHORT	use_subtype)
{
/**************************************
 *
 *	p u t _ d t y p e 
 *
 **************************************
 *
 * Function
 *	Write out field data type
 *	Taking special care to declare international text.
 *
 **************************************/

#ifdef DEV_BUILD
/* Check if the field describes a known datatype */

if (field->fld_dtype > sizeof (blr_dtypes) / sizeof (blr_dtypes [0]) ||
    !blr_dtypes [field->fld_dtype]) 
    {
    SCHAR buffer[100];

    sprintf (buffer, "Invalid dtype %d in put_dtype", field->fld_dtype);
    ERRD_bugcheck (buffer);
    }
#endif

if (field->fld_dtype == dtype_cstring ||
    field->fld_dtype == dtype_text    ||
    field->fld_dtype == dtype_varying)
    {
    if (!use_subtype || (request->req_dbb->dbb_flags & DBB_v3))
	{
	STUFF (blr_dtypes [field->fld_dtype]);
	}
    else if (field->fld_dtype == dtype_varying)
	{
	STUFF (blr_varying2);
	STUFF_WORD (field->fld_ttype);
	}
    else if (field->fld_dtype == dtype_cstring)
	{
	STUFF (blr_cstring2);
	STUFF_WORD (field->fld_ttype);
	}
    else
	{
	STUFF (blr_text2);
	STUFF_WORD (field->fld_ttype);
	}

    if (field->fld_dtype == dtype_varying)
	STUFF_WORD (field->fld_length - sizeof (USHORT))
    else 
	STUFF_WORD (field->fld_length);
    }
else
    {
    STUFF (blr_dtypes [field->fld_dtype]);
    if (DTYPE_IS_EXACT(field->fld_dtype) ||
	(dtype_quad == field->fld_dtype) )
        {
	STUFF(field->fld_scale);
        }
    }
}

static void put_field (
    REQ		request,
    FLD		field,
    BOOLEAN	udf_flag)
{
/**************************************
 *
 *	p u t _ f i e l d 
 *
 **************************************
 *
 * Function
 *	Emit dyn which describes a field data type.
 *	This field could be a column, a procedure input,
 *	or a procedure output.
 *
 **************************************/

put_number (request, gds__dyn_fld_type, blr_dtypes [field->fld_dtype]);
if (field->fld_dtype == dtype_blob)
    {
    put_number (request, gds__dyn_fld_sub_type, field->fld_sub_type);
    put_number (request, gds__dyn_fld_scale, 0);
    if (!udf_flag)
	{
	if (!field->fld_seg_length)
	    field->fld_seg_length = DEFAULT_BLOB_SEGMENT_SIZE;
    	put_number (request, gds__dyn_fld_segment_length, field->fld_seg_length);
	}
    if (!(request->req_dbb->dbb_flags & DBB_v3))
        if (field->fld_sub_type == BLOB_text)
            put_number (request, gds__dyn_fld_character_set, field->fld_character_set_id);
    }
else if (field->fld_dtype <= dtype_any_text)
    {
    if (field->fld_sub_type)
        put_number (request, gds__dyn_fld_sub_type, field->fld_sub_type);
    put_number (request, gds__dyn_fld_scale, 0);
    if (field->fld_dtype == dtype_varying)
	put_number (request, gds__dyn_fld_length, field->fld_length - sizeof (USHORT));
    else
        put_number (request, gds__dyn_fld_length, field->fld_length);
    if (!(request->req_dbb->dbb_flags & DBB_v3))
	{
	put_number (request, gds__dyn_fld_char_length, field->fld_character_length);
	put_number (request, gds__dyn_fld_character_set, field->fld_character_set_id);
	if (!udf_flag)
	    put_number (request, gds__dyn_fld_collation, field->fld_collation_id);
	}
    }
else
    {
    put_number (request, gds__dyn_fld_scale, field->fld_scale);
    put_number (request, gds__dyn_fld_length, field->fld_length);
    if (DTYPE_IS_EXACT(field->fld_dtype))
	{
	put_number (request, isc_dyn_fld_precision, field->fld_precision);
	if (field->fld_sub_type)
	    put_number (request, isc_dyn_fld_sub_type, field->fld_sub_type);
	}
    }
}

static void put_local_variable (
    REQ	request,
    VAR variable)
{
/**************************************
 *
 *	p u t _ l o c a l _ v a r i a b l e
 *
 **************************************
 *
 * Function
 *	Write out local variable field data type
 *
 **************************************/
FLD	field;
USHORT	dtype;

field = variable->var_field;
STUFF (blr_dcl_variable);
STUFF_WORD (variable->var_variable_number);
DDL_resolve_intl_type (request, field, NULL);
if ((dtype = field->fld_dtype) == dtype_blob)
    field->fld_dtype = dtype_quad;
put_dtype (request, field, TRUE);
field->fld_dtype = dtype;

/* Initialize variable to NULL */

STUFF (blr_assignment);
STUFF (blr_null);
STUFF (blr_variable);
STUFF_WORD (variable->var_variable_number);
}

static SSHORT put_local_variables (
    REQ		request,
    NOD		parameters,
    SSHORT	locals)
{
/**************************************
 *
 *	p u t _ l o c a l _ v a r i a b l e s
 *
 **************************************
 *
 * Function
 *	Emit dyn for the local variables declared
 *	in a procedure or trigger.
 *
 **************************************/
NOD	*rest, parameter, *ptr, *end, var_node;
FLD	field, rest_field;
VAR	variable;

if (parameters)
    {
    ptr = parameters->nod_arg;
    for (end = ptr + parameters->nod_count; ptr < end; ptr++)
	{
	parameter = *ptr; 
	field = (FLD) parameter->nod_arg [e_dfl_field];
        rest = ptr;
        while ((++rest) != end)
            {
            rest_field = (FLD) (*rest)->nod_arg [e_dfl_field];
            if (!strcmp (field->fld_name, rest_field->fld_name))
                ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -637,
                        gds_arg_gds, gds__dsql_duplicate_spec,
                        gds_arg_string, field->fld_name,
                        0);
            }
	*ptr = var_node = MAKE_variable (field, field->fld_name, VAR_output,
	    0, 0, locals);
	variable = (VAR) var_node->nod_arg [e_var_variable];
	put_local_variable (request, variable);
	locals++;
	}
    }

return locals;
}

static void put_msg_field (
    REQ	request,
    FLD	field)
{
/**************************************
 *
 *	p u t _ m s g _ f i e l d
 *
 **************************************
 *
 * Function
 *	Write out message field data type
 *
 **************************************/
USHORT	dtype;

if ((dtype = field->fld_dtype) == dtype_blob)
    field->fld_dtype = dtype_quad;
put_dtype (request, field, TRUE);
field->fld_dtype = dtype;

/* add slot for null flag (parameter2) */

STUFF (blr_short);
STUFF (0);
}

static void put_number (
    REQ 	request,
    UCHAR	verb,
    SSHORT	number)
{
/**************************************
 *
 *	p u t _ n u m b e r
 *
 **************************************
 *
 * Input
 *	blr_ptr: current position of blr being generated
 *	verb: blr byte of which number is an argument
 *	number: value to be written to blr
 * Function
 *	Write out a numeric valued attribute.
 *
 **************************************/

if (verb)
    STUFF (verb);

STUFF_WORD (2);
STUFF_WORD (number);
}

static void put_cstring (
    REQ		request,
    UCHAR	verb,
    UCHAR	*string)
{
/**************************************
 *
 *	p u t _ c s t r i n g
 *
 **************************************
 *
 * Function
 *	Write out a string valued attribute.
 *
 **************************************/
USHORT	length;

if (string)
    length = strlen (string);
else 
    length = 0;

put_string (request, verb, string, length);
}

static void put_string (
    REQ		request,
    UCHAR	verb,
    UCHAR	*string,
    USHORT	length)
{
/**************************************
 *
 *	p u t _ s t r i n g
 *
 **************************************
 *
 * Function
 *	Write out a string valued attribute.
 *
 **************************************/

if (verb)
    {
    STUFF (verb);
    STUFF_WORD (length);
    }
else
    STUFF (length);

if (string)
    for (; *string && length--; string++)
	STUFF (*string);
}

static NOD replace_field_names (
    NOD		input,
    NOD		search_fields,
    NOD		replace_fields,
    SSHORT	null_them)
{
/* *************************************
 *
 *	r e p l a c e _ f i e l d _ n a m e s
 *
 **************************************
 *
 * Function
 *	Given an input node tree, find any field name nodes
 *	and replace them according to the mapping provided.
 *	Used to create view  WITH CHECK OPTION.
 *
 **************************************/
NOD	*endo, field_node, *ptr, *end, *search, *replace;
STR	field_name, replace_name;
TEXT	*search_name;
FLD	field;
SSHORT	found;

if (!input) 
    return input;

if (input->nod_header.blk_type != type_nod)
    return input;

for (ptr = input->nod_arg, endo = ptr + input->nod_count; ptr < endo; ptr++)
    {

    if ((*ptr)->nod_type == nod_select_expr)
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__subquery_err,
                /* No subqueries permitted for VIEW WITH CHECK OPTION */
	    0);

    if ((*ptr)->nod_type == nod_field_name)
	{
	/* found a field node, check if it needs to be replaced */

	field_name = (STR) (*ptr)->nod_arg [e_fln_name];    

	search = search_fields->nod_arg;
	end = search + search_fields->nod_count; 
	if (replace_fields)
	    replace = replace_fields->nod_arg;
	found = FALSE;
	for (; search < end; search++, (replace_fields) ? replace++ : NULL)
	    {
	    if (replace_fields)
		{
		replace_name = (STR) (*replace)->nod_arg [e_fln_name];
		}
	    field_node = *search;
	    field = (FLD) field_node->nod_arg [e_fld_field];
	    search_name = field->fld_name;
	    if (!strcmp ((SCHAR*) field_name->str_data, (SCHAR*) search_name))
		{
		found = TRUE;
		if (replace_fields)
		    (*ptr)->nod_arg [e_fln_name] = (*replace)->nod_arg [e_fln_name];
		(*ptr)->nod_arg [e_fln_context] = (NOD) MAKE_cstring (NEW_CONTEXT);

		 }
	    if (null_them &&
		replace_fields && 
		!strcmp ((SCHAR*) field_name->str_data, (SCHAR*) replace_name->str_data))
		found = TRUE;
	    }
	if (null_them && !found)
	    {
	    (*ptr) = MAKE_node (nod_null, (int) 0);
	    }
	 }
    else 
	/* recursively go through the input tree looking for field name nodes */
	replace_field_names (*ptr, search_fields, replace_fields, null_them);
    }

return input;
}

static void reset_context_stack (
    REQ		request)
{
/**************************************
 *
 *	r e s e t _ c o n t e x t _ s t a c k
 *
 **************************************
 *
 * Function
 *	Get rid of any predefined contexts created 
 *	for a view or trigger definition.
 *
 **************************************/
    
while (request->req_context)
    LLS_POP (&request->req_context);
request->req_context_number = 0;
}

static void save_field (
    REQ		request,
    TEXT	*field_name)
{
/**************************************
 *
 *	s a v e _ f i e l d
 *
 **************************************
 *
 * Function
 *	Save the name of a field in the relation or view currently
 *	being defined.  This is done to support definition
 *	of triggers which will depend on the metadata created 
 *	in this request.
 *
 **************************************/
DSQL_REL	relation;
FLD	field;
TSQL	tdsql;

tdsql = GET_THREAD_DATA;

if (!(relation = request->req_relation))
    return;

field = (FLD) ALLOCDV (type_fld, strlen (field_name) + 1);
strcpy (field->fld_name, field_name);
field->fld_next = relation->rel_fields;
relation->rel_fields = field;
}

static void save_relation (
    REQ		request,
    STR		relation_name)
{
/**************************************
 *
 *	s a v e _ r e l a t i o n
 *
 **************************************
 *
 * Function
 *	Save the name of the relation or view currently
 *	being defined.  This is done to support definition
 *	of triggers which will depend on the metadata created 
 *	in this request.
 *
 **************************************/
DSQL_REL	relation;
NOD	ddl_node;
TSQL	tdsql;

tdsql = GET_THREAD_DATA;

if (request->req_flags & REQ_save_metadata)
    return;

request->req_flags |= REQ_save_metadata;
ddl_node = request->req_ddl_node;

if (ddl_node->nod_type == nod_mod_relation)
   relation = METD_get_relation (request, relation_name);
else
   { 
   relation = (DSQL_REL) ALLOCDV (type_dsql_rel, relation_name->str_length);
   relation->rel_name = relation->rel_data;
   relation->rel_owner = relation->rel_data + relation_name->str_length + 1;
   strcpy (relation->rel_name, (SCHAR*) relation_name->str_data);
   *relation->rel_owner = 0;
   }
request->req_relation = relation;
}

static void set_statistics (
    REQ		request)
{
/**************************************
 *
 *	s e t _ s t a t i s t i c s
 *
 **************************************
 *
 * Function
 *	Alter an index/.. statistics
 *
 **************************************/
NOD	ddl_node;
STR	index_name;

ddl_node = request->req_ddl_node;

index_name = (STR) ddl_node->nod_arg [e_stat_name];

put_cstring (request, gds__dyn_mod_idx, index_name->str_data);

STUFF (gds__dyn_idx_statistic);

STUFF (gds__dyn_end);
}

static void stuff_default_blr (
    REQ         request,
    TEXT        *default_buff,
    USHORT      buff_size
    )
{
/********************************************
 *
 *      s t u f f _ d e f a u l t _ b l r
 *
 ********************************************
 * Function:
 *   The default_blr is passed in default_buffer. It is of the form:
 *   blr_version4 blr_literal ..... blr_eoc.
 *   strip the blr_version4 and blr_eoc verbs and stuff the remaining
 *   blr in the blr stream in the request.
 *
 *********************************************/
unsigned int i;
assert ((*default_buff == blr_version4) || (*default_buff == blr_version5));
   
for (i = 1;
    ( (i< buff_size) && (default_buff[i] != blr_eoc) ); 
    i++)
   STUFF (default_buff[i]);

assert (default_buff[i] == blr_eoc);
}

static void stuff_matching_blr (
    REQ         request,
    NOD         for_columns,
    NOD         prim_columns
    )
{
/********************************************
 *
 *      s t u f f _ m a t c h i n g _ b l r
 *
 ********************************************
 *
 * Function
 *   Generate blr to express: foreign_key == primary_key
 *   ie.,  for_key.column_1 = prim_key.column_1 and
 *         for_key.column_2 = prim_key.column_2 and ....  so on..  
 *
 **************************************/
NOD *for_key_flds, *prim_key_flds;
USHORT num_fields = 0;
STR  for_key_fld_name_str,  prim_key_fld_name_str;

/* count of foreign key columns */
assert (prim_columns->nod_count == for_columns->nod_count);
assert (prim_columns->nod_count != 0);

STUFF (blr_boolean);
if (prim_columns->nod_count > 1)
    STUFF (blr_and);

num_fields = 0;
for_key_flds = for_columns->nod_arg;
prim_key_flds = prim_columns->nod_arg;

do  {
    STUFF (blr_eql);

    for_key_fld_name_str = (STR) (*for_key_flds)->nod_arg[1];
    prim_key_fld_name_str = (STR) (*prim_key_flds)->nod_arg[1];

    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name_str->str_data);
    STUFF (blr_field); STUFF ((SSHORT)0);
    put_cstring (request, 0, prim_key_fld_name_str->str_data);

    num_fields++;

    if (prim_columns->nod_count - num_fields >= (unsigned) 2)
	STUFF (blr_and);

    for_key_flds++;
    prim_key_flds++;

    }
while (num_fields < for_columns->nod_count);

STUFF (blr_end);
}

static void stuff_trg_firing_cond (
    REQ         request,
    NOD         prim_columns
    )
{
/********************************************
 *
 *      s t u f f _ t r g _ f i r i n g _ c o n d
 *
 ********************************************
 *
 * Function
 *   Generate blr to express: if (old.primary_key != new.primary_key).
 *   do a column by column comparison.
 *
 **************************************/
NOD    *prim_key_flds;
USHORT num_fields = 0;
STR    prim_key_fld_name_str;

STUFF (blr_if);
if (prim_columns->nod_count > 1)
   STUFF (blr_or);

num_fields = 0;
prim_key_flds = prim_columns->nod_arg;
do  {
    STUFF (blr_neq);

    prim_key_fld_name_str = (STR) (*prim_key_flds)->nod_arg[1];

    STUFF (blr_field); STUFF ((SSHORT)0);
    put_cstring (request, 0, prim_key_fld_name_str->str_data);
    STUFF (blr_field); STUFF ((SSHORT)1);
    put_cstring (request, 0, prim_key_fld_name_str->str_data);

    num_fields++;

    if (prim_columns->nod_count - num_fields >= (unsigned) 2)
        STUFF (blr_or);
    
    prim_key_flds++;
    
    }
while (num_fields < prim_columns->nod_count);
}


static void modify_field (
    REQ		request,
    NOD		element,
    SSHORT	position,
    STR		relation_name)
{
/**************************************
 *
 *	m o d i f y _ f i e l d
 *
 **************************************
 *
 * Function
 *	Modify a field, either as part of an
 *	alter table or alter domain statement.
 *
 **************************************/
NOD	domain_node;
FLD	field;
DSQL_REL	relation;

field = (FLD) element->nod_arg [e_dfl_field];
put_cstring (request, isc_dyn_mod_sql_fld, field->fld_name);

/* add the field to the relation being defined for parsing purposes */
if ((relation = request->req_relation) != NULL)
    {
    field->fld_next = relation->rel_fields;
    relation->rel_fields = field;
    }

if (domain_node = element->nod_arg [e_mod_fld_type_dom_name])
    {
    NOD		node1;
    STR		domain_name;

    node1 = domain_node->nod_arg [e_dom_name];
    domain_name = (STR) node1->nod_arg [e_fln_name];
    put_cstring (request, gds__dyn_fld_source, domain_name->str_data);

    /* Get the domain information */

    if (!(METD_get_domain (request, field, domain_name->str_data)))
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607, 
            gds_arg_gds, gds__dsql_command_err, 
            gds_arg_gds, gds__dsql_domain_not_found, 
            /* Specified domain or source field does not exist */
            0);
    DDL_resolve_intl_type (request, field, NULL);
    }
else
    {
    if (relation_name)
	put_cstring (request, gds__dyn_rel_name, relation_name->str_data);

    DDL_resolve_intl_type (request, field, NULL);
    put_field (request, field, FALSE);
    }
STUFF (gds__dyn_end);   
}

static void set_nod_value_attributes (
    NOD		node,
    FLD	        field)
{
/**************************************
 *
 *	s e t _ n o d _ v a l u e _ a t t r i b u t e s
 *
 **************************************
 *
 * Function
 *	Examine all the children of the argument node:
 *	if any is a nod_dom_value, set its dtype, size, and scale
 *      to those of the field argument
 *
 **************************************/
  ULONG child_number;
  NOD   child;

  for (child_number = 0 ; child_number < node->nod_count ; ++child_number)
    {
      child = node->nod_arg[child_number];
      if (child && (type_nod == child->nod_header.blk_type))
	{
	  if ( nod_dom_value == child->nod_type )
	    {
	      child->nod_desc.dsc_dtype  = field->fld_dtype;
	      child->nod_desc.dsc_length = field->fld_length;
	      child->nod_desc.dsc_scale  = field->fld_scale;
	    }
	  else if ( (nod_constant     != child->nod_type) &&
		    (child->nod_count >  0              ) )
	    {
	      /* A nod_constant can have nod_arg entries which are not really
		 pointers to other nodes, but rather integer values, so
		 it is not safe to scan through its children.  Fortunately,
		 it cannot have a nod_dom_value as a child in any case, so
		 we lose nothing by skipping it.
		 */
	      
	      set_nod_value_attributes (child, field);
	    }
	} /* if it's a node */
    } /* for (child_number ... */
  return;
}
