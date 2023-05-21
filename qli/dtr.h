/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		dtr.h
 *	DESCRIPTION:	General definitions, etc.
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

#ifndef _QLI_DTR_H_
#define _QLI_DTR_H_

#include "../jrd/common.h"
#include "../jrd/dsc.h"

#define QLI_MSG_FAC		1
#define BUGCHECK(number)	ERRQ_bugcheck (number)
#define SYNTAX_ERROR(number)	ERRQ_syntax (number)
#define IBERROR(number)		ERRQ_error (number, NULL, NULL, NULL, NULL, NULL)
#define BLKCHK(blk,type)	if (blk->blk_type != (SCHAR) type) BUGCHECK (0)
				/* MSG 0 - expected type */

#define ALL_release(blk)	ALLQ_release (blk)
#define ALLOCD(type)		ALLQ_alloc (QLI_default_pool, type, 0)
#define ALLOCDV(type,repeat)	ALLQ_alloc (QLI_default_pool, type, repeat)
#define ALLOCP(type)		ALLQ_alloc (QLI_permanent_pool, type, 0)
#define ALLOCPV(type,repeat)	ALLQ_alloc (QLI_permanent_pool, type, repeat)

#define BLKDEF(type, root, tail) type,
ENUM blk_t
    {
    type_MIN = 0,
#include "../qli/blk.h"
    type_MAX
    };
#undef BLKDEF

/* Block types */

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;

typedef struct vec {
    struct blk	vec_header;
    ULONG	vec_count;
    struct blk	*vec_object[1];
} *VEC;

typedef struct vcl {
    struct blk	vcl_header;
    ULONG	vcl_count;
    SLONG	vcl_long[1];
} *VCL;

/* Database block */

typedef struct dbb {
    struct blk	dbb_header;
    struct dbb	*dbb_next;		/* Next database in system */
    struct rel	*dbb_relations;		/* Linked list of relations */
    struct frm	*dbb_forms;		/* Known forms in database */
    struct fun	*dbb_functions;		/* Known functions in database */
    void	*dbb_handle;		/* database handle */
    void	*dbb_transaction;	/* transaction handle */
    void	*dbb_proc_trans;	/* procedure transaction */
    void	*dbb_meta_trans;	/* metadata update transaction */
    void	*dbb_field_request;	/* "get fields" request handle */
    struct sym	*dbb_symbol;		/* Database variable */
    struct con	*dbb_user;		/* user name */
    struct con	*dbb_password;		/* password */
    USHORT	dbb_filename_length;	/* Length of filename */
    void	*dbb_lookup_blob;	/* Request to look up blob */
    void	*dbb_store_blob;	/* Request to store blob */
    void	*dbb_edit_blob;
    void	*dbb_edit_store;
    void	*dbb_scan_blobs;
    void	*dbb_delete_blob;
    USHORT	dbb_flags;
    USHORT	dbb_type;		/* Friend or foe? */
    USHORT	dbb_pagesize;		/* For SQL metadata */
    ULONG	dbb_capabilities;	/* Special characteristics to look out for */
    int		*dbb_statistics;	/* Statistics memory */
    void	*dbb_requests [96];	/* Misc meta-data requests */
    TEXT	dbb_filename [2];	/* Filename of database */
} *DBB;

/* Bits in dbb_flags */

#define DBB_procedures	1		/* Procedures relation found */
#define DBB_active	2		/* Database is active in request */
#define DBB_updates	8		/* Updates in current transaction */
#define DBB_form_init	16		/* Forms are initialize for database */
#define DBB_prepared	32		/* User transaction has been prepared */

/* Bits in dbb_capabilities */

#define DBB_cap_extern_file	1	/* supports the concept of external files */
#define DBB_cap_files		2	/* uses the Interbase concept of multi-file */
#define DBB_cap_security	4	/* InterBase style security classes */
/* #define DBB_cap_triggers	8	-- OBSOLETE: triggers (old style) */
#define DBB_cap_idx_inactive	16	/* Interbase index inactive flag */
#define DBB_cap_multi_trans	32	/* Database supports multiple transactions */
#define DBB_cap_single_trans	64	/* Database supports only a single transaction */
#define DBB_cap_dudley		128	/* We're authorized for meta-data updates */
#define DBB_cap_functions	256	/* The DBMS knows about function */
#define DBB_cap_new_triggers	512	/* triggers (new V3 style) */
#define DBB_cap_shadowing	1024	/* Database supports shadowing */
#define DBB_cap_types		2048	/* Database has RDB$TYPES relation */
#define DBB_cap_dimensions	4096	/* Database supports arrays -- What a database! */ 
#define DBB_cap_external_type	8192	/* Database supports external datatypes */
#define DBB_cap_rfr_sys_flag	16384  	/* Database hasn't forgotten about the system flag */
#define DBB_cap_filters		32768	/* Database supports filters */
#define DBB_cap_index_type	65536	/* Database has too damn much stuff */
/* Relation block */

typedef struct rel {
    struct blk	rel_header;
    struct rel	*rel_next;		/* Next relation in database */
    struct dbb	*rel_database;		/* Parent database */
    struct sym	*rel_symbol;		/* Hash symbol for relation */
    struct fld	*rel_fields;		/* Field block */
    struct frm	*rel_form;		/* Default for for relation */
    USHORT	rel_id;			/* Relation id */
    USHORT	rel_flags;		/* Misc flags */
    USHORT	rel_system_flag;	/* System flag */
    USHORT	rel_max_field_pos;	/* highest used field position */
} *REL;

#define REL_fields	1		/* Fields have been fetched */
#define REL_system	2		/* Relation is system relation */
#define REL_view	4		/* Relation is a view */

/* Field block */

typedef struct fld {
    struct blk	fld_header;
    struct fld	*fld_next;		/* Next field in relation */
    struct rel	*fld_relation;		/* Parent relation */
    struct sym	*fld_name;		/* Field name */
    struct sym	*fld_query_name;	/* Field query name */
    struct ffl	*fld_form;		/* Field form, if known */
    struct sym	*fld_based;		/* Name of global field, if we care */
    TEXT	*fld_edit_string;	/* Edit string, if any */
    TEXT	*fld_query_header;	/* Query header, if any */
    USHORT	fld_flags;
    USHORT	fld_id;			/* Field in in database */
    USHORT	fld_dtype;		/* Data type of field */
    FLD_LENGTH	fld_length;		/* Length of field */
    USHORT	fld_segment_length;	/* Segment length for blobs */
    SSHORT	fld_scale;		/* Scale factor of field */
    struct syn	*fld_validation;	/* Validation expression */
    struct syn	*fld_computed;		/* Computed by expression */
    struct con  *fld_missing;		/* Missing value defined in system relation */
    SSHORT	fld_sub_type;		/* Subtype for blobs and text */
    SSHORT	fld_sub_type_missing;	/* Subtype missing flag */
    USHORT	fld_system_flag;	/* System flag */
    double	fld_dummy;		/* Force fld_data to a nice boundary word boundary */
    UCHAR	fld_data [1];
} *FLD;

#define FLD_computed	1
#define FLD_drop	2
#define FLD_modify	4
#define FLD_missing	8
#define FLD_not_null	16
#define FLD_add		32
#define FLD_unique	64
#define FLD_array	128

/* Privileges for SQL metadata */

#define PRV_select	1
#define PRV_insert	2
#define PRV_delete	4
#define PRV_update	8
#define PRV_all		PRV_select + PRV_insert + PRV_delete + PRV_update
#define PRV_grant_option	16

#define LLS_PUSH(object,stack)		ALLQ_push ((BLK) object, stack)
#define LLS_POP(stack)			ALLQ_pop (stack)

typedef struct lls {
    struct blk	lls_header;
    struct blk	*lls_object;
    struct lls	*lls_next;
} *LLS;

/* Random string block -- jack of all kludges */

typedef struct str {
    struct blk	str_header;
    USHORT	str_length;
    USHORT	str_fluff;
    TEXT	str_data[2];
} *STR;

/* Symbol types */

typedef ENUM sym_t {
    SYM_keyword,
    SYM_context,
    SYM_database,
    SYM_relation,
    SYM_field,
    SYM_stream,
    SYM_cursor,
    SYM_form,
    SYM_function
} SYM_T;

typedef struct sym {
    struct blk	sym_header;
    TEXT	*sym_string;	/* address of asciz string */
    USHORT	sym_length;	/* length of string (exc. term.) */
    SYM_T	sym_type;	/* symbol type */
    USHORT	sym_keyword;	/* keyword number, if keyword */
    struct blk	*sym_object;	/* general pointer to object */
    struct sym	*sym_collision;	/* collision pointer */
    struct sym	*sym_homonym;	/* homonym pointer */
    TEXT	sym_name[2];	/* space for name, if necessary */
} *SYM ;

/* Free block */

typedef struct frb {
    struct blk	frb_header;
    struct frb	*frb_next;	/* Next free block in pool */
} *FRB;

/* Hunk blocks */

typedef struct hnk {
    struct blk	hnk_header;
    SCHAR	*hnk_address;	/* start of memory hunk */
    int		hnk_length;	/* length of memory hunk */
    struct hnk	*hnk_next;	/* next memory hunk in structure */
} *HNK;

/* Pool block */

typedef struct plb {
    struct blk	plb_header;
    USHORT	plb_pool_id;	/* pool id */
    FRB		plb_free;	/* first free block */
    HNK		plb_hunks;	/* first hunk block */
    LLS		plb_lls;	/* avaiable linked list stack nodes */
} *PLB;

typedef ENUM nod_t {

/* Commands, not executed. */

    nod_ready = 1,
    nod_finish,
    nod_commit,
    nod_rollback,
    nod_prepare,
    nod_start_trans,
    nod_show,
    nod_exit,
    nod_quit,
    nod_create,
    nod_define,
    nod_delete_proc,
    nod_rename_proc,
    nod_edit_proc,
    nod_copy_proc,
    nod_edit_form,
    nod_del_form,
    nod_extract,
    nod_edit,
    nod_set,
    nod_help,
    nod_shell,
    nod_def_database,
    nod_del_database,
    nod_def_relation,
    nod_mod_relation,
    nod_del_relation,
    nod_def_field,
    nod_mod_field,
    nod_del_field,
    nod_def_index,
    nod_del_index,
    nod_mod_index,

/* SQL statements, mapped into GDML statements */

    nod_select,
    nod_insert,
    nod_delete,
    nod_fetch,
    nod_close,
    nod_open,

    nod_for,
    nod_restructure,
    nod_print,
    nod_list_fields,
    nod_list,
    nod_store,
    nod_modify,
    nod_erase,
    nod_assign,
    nod_report,
    nod_report_loop,
    nod_repeat,
    nod_if,
    nod_form_for,
    nod_form_update,
    nod_output,
    nod_declare,
    nod_variable,
    nod_abort,
    nod_label,
    nod_leave,
    nod_menu,
    nod_sync,
    nod_commit_retaining,

    nod_relation,
    nod_name,
    nod_rse,
    nod_union,
    nod_aggregate,
    nod_form,

    nod_print_item,
    nod_star,			/* Bunch of items */
    nod_column,
    nod_tab,
    nod_skip,
    nod_space,
    nod_new_page,
    nod_report_header,
    nod_column_header,

    nod_eql,
    nod_neq,
    nod_gtr,
    nod_geq,
    nod_leq,
    nod_lss,
    nod_between,
    nod_containing,
    nod_matches,
    nod_like,
    nod_starts,
    nod_missing,
    nod_and,
    nod_or,
    nod_any,
    nod_not,
    nod_unique,

    nod_reference,
    nod_field,
    nod_form_field,
    nod_prompt,
    nod_prompt2,
    nod_edit_blob,
    nod_parameter,
    nod_constant,
    nod_position,
    nod_map,

    nod_add,
    nod_subtract,
    nod_multiply,
    nod_divide,
    nod_negate,
    nod_concatenate,
    nod_substr,
    nod_null,
    nod_user_name,
    nod_format,
    nod_choice,
    nod_function,

    nod_average,
    nod_max,
    nod_min,
    nod_total,
    nod_count,

    nod_agg_average,
    nod_agg_max,
    nod_agg_min,
    nod_agg_total,
    nod_agg_count,

    nod_rpt_average,
    nod_rpt_max,
    nod_rpt_min,
    nod_rpt_total,
    nod_rpt_count,

    nod_running_count,
    nod_running_total,
    nod_from,
    nod_via,

    nod_upcase,				/* New blr for v3 */
    nod_sleuth,
    nod_index,

    nod_join_inner,
    nod_join_left,
    nod_join_right,
    nod_join_full,

    nod_sql_create,			/* SQL metadata */
    nod_sql_database,
    nod_sql_dr_index,
    nod_sql_al_table,
    nod_sql_cr_table,
    nod_sql_dr_table,
    nod_sql_cr_view,
    nod_sql_dr_view,
    nod_sql_grant,
    nod_sql_revoke

} NOD_T;

/* Equivalence label dsc_missing to field dsc_flags in the dsc structure */

#define dsc_missing	dsc_flags
                        
/* State flags for dsc_missing */

#define DSC_missing	1
#define DSC_initial	2

/* Constant block */

typedef struct con {
    struct blk	con_header;
    DSC		con_desc;
    UCHAR	con_data[1];
} *CON;

/* Function description */

typedef struct fun {
    struct blk	fun_header;
    struct fun	*fun_next;		/* Next function in database */
    DBB		fun_database;
    SYM		fun_symbol;		/* Associated symbol block */
    SYM		fun_query_name;
    DSC		fun_return;		/* Return descriptor */
    USHORT	fun_args;		/* Number of arguments */
    DSC		fun_arg [1];		/* Data type of arguments */
} *FUN;

/* Program wide globals */

#ifdef QLI_MAIN
#define EXTERN
#else
#define EXTERN	extern
#endif

EXTERN DBB	QLI_databases;
EXTERN PLB	QLI_permanent_pool, QLI_default_pool;
EXTERN FLD	QLI_variables;
EXTERN TEXT	QLI_prompt_string [32], QLI_cont_string [32];
EXTERN TEXT	QLI_default_user [32], QLI_default_password [32];
EXTERN TEXT	QLI_charset [32];
EXTERN CON	QLI_matching_language;

EXTERN USHORT	QLI_eof, QLI_blr, QLI_skip_line, QLI_statistics, QLI_semi;
EXTERN USHORT	QLI_abort, QLI_echo, QLI_form, QLI_form_mode, QLI_trace;
EXTERN USHORT	QLI_count;
#ifdef DEV_BUILD
EXTERN USHORT	QLI_explain;
EXTERN USHORT	QLI_hex_output;
#endif

EXTERN USHORT   QLI_euc_justify;
EXTERN USHORT   QLI_interp;

#undef EXTERN

#include "../qli/all_proto.h"

#endif /* _QLI_DTR_H_ */
