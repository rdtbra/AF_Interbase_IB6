/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		exe.h
 *	DESCRIPTION:	Execution block definitions
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

#ifndef _JRD_EXE_H_
#define _JRD_EXE_H_

#define NODE(type, name, keyword) type,

typedef ENUM nod_t {
#include "../jrd/nod.h"
    nod_MAX
#undef NODE
} NOD_T;

#include "../jrd/dsc.h"

/* NOTE: The definition of structures rse and lit must be defined in
	 exactly the same way as structure nod through item nod_count.
	 If you change one, be sure to change all of them. */

typedef struct nod {
    struct blk	nod_header;
    struct nod	*nod_parent;
    SLONG	nod_impure;		/* Inpure offset from request block */
    NOD_T	nod_type;		/* Type of node */
    UCHAR	nod_flags;
    SCHAR	nod_scale;		/* Target scale factor */
    USHORT	nod_count;		/* Number of arguments */
    struct nod	*nod_arg [1];
} *NOD;

#define nod_comparison 	1
#define nod_id		1		/* marks a field node as a blr_fid guy */
#define nod_quad	2		/* compute in quad (default is long) */
#define nod_any_and     2		/* and node is mapping of quantified predicate */
#define nod_double	4
#define nod_date	8
#define nod_value	16		/* full value area required in impure space */
#define nod_evaluate	32		/* (Gateway only) */
#define nod_agg_dbkey	64		/* dbkey of an aggregate */
#define nod_invariant	128		/* node is recognized as being invariant */


/* Special RSE node */

typedef struct rse {
    struct blk	nod_header;
    struct nod	*nod_parent;
    SLONG	nod_impure;		/* Inpure offset from request block */
    NOD_T	nod_type;		/* Type of node */
    UCHAR	nod_flags;
    SCHAR	nod_scale;		/* Target scale factor */
    USHORT	nod_count;		/* Number of arguments */
    USHORT	rse_count;
    USHORT	rse_jointype;		/* inner, left, right, full */
    struct rsb	*rse_rsb;
    struct nod	*rse_first;
    struct nod	*rse_boolean;
    struct nod	*rse_sorted;
    struct nod	*rse_projection;
    struct nod	*rse_aggregate;	/* singleton aggregate for optimizing to index */
    struct nod	*rse_plan;		/* user-specified access plan */
#ifdef SCROLLABLE_CURSORS
    struct nod	*rse_async_message;	/* asynchronous message to send for scrolling */
#endif
    struct nod	*rse_relation [1];
} *RSE;

#define rse_stream	1		/* flags rse-type node as a blr_stream type */
#define rse_singular	2               /* flags rse-type node as from a singleton select */
#define rse_variant	4		/* flags rse as variant (not invariant?) */

#define rse_delta	(sizeof(struct rse)-sizeof(struct nod))/sizeof(((NOD) 0)->nod_arg[0])

/* Literal value */

typedef struct lit {
    struct blk	nod_header;
    struct nod	*nod_parent;
    SLONG	nod_impure;		/* Inpure offset from request block */
    NOD_T	nod_type;		/* Type of node */
    UCHAR	nod_flags;
    SCHAR	nod_scale;		/* Target scale factor */
    USHORT	nod_count;		/* Number of arguments */
    struct dsc	lit_desc;
    UCHAR	lit_data [1];
} *LIT;

#define lit_delta	(sizeof (struct dsc) / sizeof (NOD*))

/* Aggregate Sort Block (for DISTINCT aggregates) */

typedef struct asb {
    struct blk	nod_header;
    struct nod	*nod_parent;
    SLONG	nod_impure;		/* Impure offset from request block */
    NOD_T	nod_type;		/* Type of node */
    UCHAR	nod_flags;
    SCHAR	nod_scale;		
    USHORT	nod_count;
    struct dsc  asb_desc;
    struct skd	*asb_key_desc;          /* for the aggregate   */
    UCHAR       asb_key_data [1];             
} *ASB;

#define asb_delta	((sizeof(struct asb) - sizeof(struct nod)) / sizeof (NOD*))

/* Various structures in the impure area */

typedef struct sta {
    SSHORT	sta_state;
} *STA;

typedef struct vlu {
    struct dsc	vlu_desc;
    struct str	*vlu_string;
    union
    {
    SSHORT	vlu_short;
    SLONG	vlu_long;
    SINT64	vlu_int64;
    SQUAD	vlu_quad;
    SLONG	vlu_dbkey [2];
    float	vlu_float;
    double	vlu_double;
    GDS_TIMESTAMP vlu_timestamp;
    GDS_TIME	vlu_sql_time;
    GDS_DATE	vlu_sql_date;
    }		vlu_misc;
} *VLU;

typedef struct vlux {
    struct dsc	vlu_desc;
    struct str	*vlu_string;
    union
    {
    SSHORT	vlu_short;
    SLONG	vlu_long;
    SINT64	vlu_int64;
    SQUAD	vlu_quad;
    SLONG	vlu_dbkey [2];
    float	vlu_float;
    double	vlu_double;
    GDS_TIMESTAMP vlu_timestamp;
    GDS_TIME	vlu_sql_time;
    GDS_DATE	vlu_sql_date;
    }		vlu_misc;
    SLONG	vlux_count;
} *VLUX;


#ifdef GATEWAY
typedef struct vluk {
    struct dsc	vlu_desc;
    struct str	*vlu_string;
    union
    {
    SSHORT	vlu_short;
    SLONG	vlu_long;
    SINT64	vlu_int64;
    SQUAD	vlu_quad;
    SLONG	vlu_dbkey [2];
    float	vlu_float;
    double	vlu_double;
    SCHAR	vlu_rowid [18];
    GDS_TIMESTAMP vlu_timestamp;
    GDS_TIME	vlu_sql_time;
    GDS_DATE	vlu_sql_date;
    }		vlu_misc;
} *VLUK;
#endif

#define VLU_computed	1		/* An invariant sub-query has been computed */
#define VLU_null	2		/* An invariant sub-query computed to null */

/* Inversion (i.e. nod_index) impure area */

typedef struct inv {
    struct sbm	*inv_bitmap;
} *INV;

/* ASB impure area */

typedef struct iasb {
    SLONG       *iasb_sort_handle;
} *IASB;

/* Various field positions */

#define	e_for_re		0
#define	e_for_statement		1
#define	e_for_stall		2
#define	e_for_rsb		3
#define	e_for_length		4

#define	e_arg_flag		0
#define e_arg_indicator		1
#define	e_arg_message		2
#define	e_arg_number		3
#define	e_arg_length		4

#define	e_msg_number		0
#define	e_msg_format		1
#define	e_msg_next		2
#define	e_msg_length		3

#define	e_fld_stream		0
#define	e_fld_id		1
#define	e_fld_default_value	2  /* hold column default value info if any,
                                        (LIT) */
#define	e_fld_length		3

#define	e_sto_statement		0
#define	e_sto_statement2	1
#define e_sto_sub_store		2
#define e_sto_validate		3
#define	e_sto_relation		4
#define e_sto_stream		5
#ifndef GATEWAY
#define	e_sto_length		6
#else
#define e_sto_sql		6
#define	e_sto_length		7
#endif

#define e_erase_statement	0
#define e_erase_sub_erase 	1
#define	e_erase_stream		2
#define e_erase_rsb		3
#ifndef GATEWAY
#define	e_erase_length		4
#else
#define e_erase_sql		4
#define	e_erase_length		5
#endif

#define	e_mod_statement		0
#define e_mod_sub_mod		1
#define e_mod_validate		2
#define e_mod_map_view		3
#define	e_mod_org_stream	4
#define	e_mod_new_stream	5
#define e_mod_rsb		6
#ifndef GATEWAY
#define	e_mod_length		7
#else
#define e_mod_sql		7
#define	e_mod_length		8
#endif

#define	e_send_statement	0
#define	e_send_message		1
#define	e_send_length		2

#define	e_asgn_from		0
#define	e_asgn_to		1
#define e_asgn_missing		2	/* Value for comparison for missing */
#define e_asgn_missing2		3	/* Value for substitute for missing */
#define	e_asgn_length		4	

#define	e_rel_stream		0
#define	e_rel_relation		1
#define	e_rel_view		2	/* parent view for posting access */
#define e_rel_alias		3	/* SQL alias for the relation */
#define e_rel_context		4	/* user-specified context number for the relation reference */
#define	e_rel_length		5

#define	e_idx_retrieval		0
#define	e_idx_length		1

#define	e_lbl_statement		0
#define	e_lbl_label		1
#define	e_lbl_length		2

#define	e_any_rse		0
#define	e_any_rsb		1
#define	e_any_length		2

#define e_if_boolean		0
#define e_if_true		1
#define e_if_false		2
#define e_if_length		3

#define e_hnd_statement		0
#define e_hnd_length		1

#define e_val_boolean		0
#define e_val_value		1
#define e_val_length		2

#define e_uni_stream		0	/* Stream for union */
#define e_uni_clauses		1	/* rse's for union */
#define e_uni_length		2

#define e_agg_stream		0
#define e_agg_rse		1
#define e_agg_group		2
#define e_agg_map		3
#define e_agg_length		4

/* Statistical expressions */

#define	e_stat_rse		0
#define	e_stat_value		1
#define	e_stat_default		2
#define	e_stat_rsb		3
#define	e_stat_length		4

/* Execute stored procedure */

#define e_esp_inputs		0
#define e_esp_in_msg		1
#define e_esp_outputs		2
#define e_esp_out_msg		3
#define e_esp_procedure		4
#define e_esp_length		5

/* Stored procedure view */

#define e_prc_inputs		0
#define e_prc_in_msg		1
#define e_prc_stream		2
#define e_prc_procedure		3
#define e_prc_length		4

/* Function expression */

#define e_fun_args		0
#define e_fun_function		1
#define e_fun_length		2

/* Generate id */

#define e_gen_value		0
#define e_gen_relation		1
#define e_gen_id		1	/* Generator id (replaces e_gen_relation) */
#define e_gen_length		2

/* Protection mask */

#define e_pro_class		0
#define e_pro_relation		1
#define e_pro_length		2

/* Variable declaraction */

#define e_var_id		0
#define e_var_variable		1
#define e_var_length		2

#define e_dcl_id		0
#define e_dcl_desc		1
#define e_dcl_length		(1 + sizeof (DSC)/sizeof (NOD))	/* Room for descriptor */

#define e_dep_object		0	/* node for registering dependencies */
#define e_dep_object_type	1
#define e_dep_field		2
#define e_dep_length		3

#define e_scl_field		0	/* Scalar expression (blr_index) */
#define e_scl_subscripts	1
#define e_scl_length		2

#define	e_blk_action		0
#define	e_blk_handlers		1
#define	e_blk_length		2

#define	e_err_action		0
#define	e_err_conditions	1
#define	e_err_length		2

/* Datatype cast operator */

#define e_cast_source		0
#define e_cast_fmt		1
#define e_cast_length		2

/* IDAPI semantics nodes */

#define e_index_index		0	/* set current index (blr_set_index) */
#define e_index_stream		1
#define e_index_rsb		2
#define e_index_length		3

#define e_seek_offset		0	/* for seeking through a stream */
#define e_seek_direction	1
#define e_seek_rse		2
#define e_seek_length		3

#define e_find_args		0	/* for finding a key value in a stream */
#define e_find_operator		1
#define e_find_direction	2
#define e_find_stream		3
#define e_find_rsb		4
#define e_find_length		5

#define e_bookmark_id		0	/* nod_bookmark */
#define e_bookmark_length	1

#define e_setmark_id		0	/* nod_set_bookmark */
#define e_setmark_stream	1
#define e_setmark_rsb		2
#define e_setmark_length	3

#define e_getmark_stream	0	/* nod_get_bookmark */
#define e_getmark_rsb		1
#define e_getmark_length	2

#define e_relmark_id		0	/* nod_release_bookmark */
#define e_relmark_length	1

#define e_lockrel_relation	0       /* nod_lock_relation */
#define e_lockrel_level		1	
#define e_lockrel_length	2

#define e_lockrec_level		0	/* nod_lock_record */
#define e_lockrec_stream	1
#define e_lockrec_rsb		2
#define e_lockrec_length	3

#define e_brange_number		0	/* nod_begin_range */
#define e_brange_length		1

#define e_erange_number		0	/* nod_end_range */
#define e_erange_length		1

#define e_drange_number		0	/* nod_delete_range */
#define e_drange_length		1

#define e_rellock_lock		0	/* nod_release_lock */
#define e_rellock_length	1
                                                               
#define e_find_dbkey_dbkey	0	/* double duty for nod_find_dbkey and nod_find_dbkey_version */
#define e_find_dbkey_version	1
#define e_find_dbkey_stream	2
#define e_find_dbkey_rsb	3
#define e_find_dbkey_length	4

#define e_range_relation_number	  0	/* nod_range_relation */
#define e_range_relation_relation 1
#define e_range_relation_length	  2

#define e_retrieve_relation	0
#define e_retrieve_access_type	1
#define e_retrieve_length	2

#define e_reset_from_stream	0
#define e_reset_to_stream	1
#define e_reset_from_rsb	2
#define e_reset_length		3

#define e_card_stream		0
#define e_card_rsb		1
#define e_card_length		2

/* SQL Date supporting nodes */
#define e_extract_value		0	/* Node */
#define e_extract_part		1	/* Integer */
#define e_extract_count		1	/* Number of nodes */
#define e_extract_length	2	/* Number of entries in nod_args */

#define e_current_date_length	1
#define e_current_time_length	1
#define e_current_timestamp_length	1


/* Compile scratch block */

typedef struct csb {
    struct blk	csb_header;
    UCHAR	*csb_blr;
    UCHAR	*csb_running;
    struct nod	*csb_node;
    struct acc	*csb_access;		/* Access items to be checked */
    struct vec	*csb_variables;		/* Vector of variables, if any */
    struct rsc	*csb_resources;		/* Resources (relations and indexes) */    
    struct lls	*csb_dependencies;	/* objects this request depends upon */
    struct lls	*csb_fors;		/* stack of fors */
    struct lls	*csb_invariants;	/* stack of invariant nodes */
    struct lls	*csb_current_rses;	/* rse's within whose scope we are */
#ifdef SCROLLABLE_CURSORS
    struct rse  *csb_current_rse;	/* this holds the rse currently being processed; 
                                           unlike the current_rses stack, it references any expanded view rse */
#endif
    struct nod	*csb_async_message;	/* asynchronous message to send to request */
    USHORT	csb_count;		/* Current tail count */
    USHORT	csb_n_stream;		/* Next available stream */
    USHORT	csb_msg_number;		/* Highest used message number */
    SLONG	csb_impure;		/* Next offset into impure area */
    USHORT	csb_g_flags;
    struct	csb_repeat
    {
    UCHAR	csb_stream;		/* Map user context to internal stream */
    UCHAR	csb_view_stream;	/* stream number for view relation, below */
    USHORT	csb_flags;
    USHORT	csb_indices;		/* Number of indices */

    struct rel	*csb_relation;
    struct str	*csb_alias;		/* SQL alias name for this instance of relation */
    struct prc	*csb_procedure;
    struct rel	*csb_view;		/* parent view */

    struct idx	*csb_idx;		/* Packed description of indices */
    struct str	*csb_idx_allocation;	/* Memory allocated to hold index descriptions */
    struct nod	*csb_message;		/* Msg for send/receive */
    struct fmt	*csb_format;		/* Default fmt for stream */
    struct sbm	*csb_fields;		/* Fields referenced */
#ifndef GATEWAY
    float	csb_cardinality;	/* Cardinality of relation */
    struct nod	*csb_plan;		/* user-specified plan for this relation */
#else
    struct sbm	*csb_asgn_flds;		/* Fields that are assigned from */
    struct rsb	*csb_selct_rsb;		/* SELECT statement's rsb */
    struct nod	*csb_selct_bool;	/* SELECT statement's boolean */
    struct nod	*csb_selct_sort;	/* SELECT statement's sort clause */
    struct sql	*csb_sql_selct;		/* SELECT statement */
    struct nod	*csb_other_nod;		/* non-SELECT statement's node */
    struct sql	*csb_sql_other;		/* non-SELECT statement */
#endif
    UCHAR	*csb_map;		/* Stream map for views */
    struct rsb	**csb_rsb_ptr;		/* point to rsb for nod_stream */
    }		csb_rpt [1];
} *CSB;

#define csb_internal	     	0x1		/* "csb_g_flag" switch */
#define csb_get_dependencies 	0x2
#define csb_ignore_perm 	0x4		/* ignore permissions checks */
#define csb_blr_version4 	0x8		/* The blr is of version 4 */

#define csb_active 	1
#define csb_used	2
#define csb_view_update	4		/* View update w/wo trigger is in progress */
#define csb_trigger	8		/* NEW or OLD context in trigger */
#define csb_no_dbkey	16		/* Stream doesn't have a dbkey */
#define csb_validation	32		/* We're in a validation expression (RDB hack) */
#define csb_store	64		/* we are processing a store statement */
#define csb_modify	128		/* we are processing a modify */
#define csb_compute	256		/* compute cardinality for this stream */
#define csb_erase	512		/* we are processing an erase */
#define csb_unmatched	1024		/* stream has conjuncts unmatched by any index */

#define csb_dbkey	8192		/* Dbkey as been requested (Gateway only) */
#define csb_update	16384		/* Erase or modify for relation */
#define csb_made_river	32768		/* stream has been included in a river*/

/* Exception condition list */

typedef struct xcp {
    struct blk  xcp_header;
    SLONG	xcp_count;
    struct      xcp_repeat
    {
    SSHORT	xcp_type;
    SLONG	xcp_code;
    }           xcp_rpt [1];
} *XCP;

#define xcp_sql_code	1
#define xcp_gds_code	2
#define xcp_xcp_code	3
#define xcp_default	4

#endif /* _JRD_EXE_H_ */
