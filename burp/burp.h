/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		burp.h
 *	DESCRIPTION:	Burp file format
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

#ifndef _BURP_BURP_H_
#define _BURP_BURP_H_

#include "../jrd/ib_stdio.h"
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/dsc.h"
#include "../burp/burp_proto.h"
#include "../burp/misc_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/thd.h"

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

#define BURP_ALLOC(size)	 MISC_alloc_burp((ULONG) size)
#define BURP_ALLOC_ZERO(size)    MISC_alloc_burp((ULONG) size)
#define BURP_FREE(block)	 MISC_free_burp(block)

#define	GDS_NAME_LEN		32
typedef TEXT			GDS_NAME [GDS_NAME_LEN];

#define NOOUTPUT        2   

/* Record types in backup file */

typedef enum rec_type {
    rec_burp,			/* Restore program attributes */
    rec_database,		/* Logical database parameters */
    rec_global_field,		/* Global field description */
    rec_relation,		/* Relation description */
    rec_field,			/* Local field description */
    rec_index,			/* Index description */
    rec_data,			/* Data for relation */
    rec_blob,			/* Blob */
    rec_relation_data,		/* Standalone data header */
    rec_relation_end,		/* End of data for relation */
    rec_end,			/* End of file */
    rec_view,			/* View attributes */
    rec_security_class,		/* Security class acl */
    rec_trigger,		/* Trigger definition */
    rec_physical_db,		/* Physical database parameters */
    rec_function,		/* Function description */
    rec_function_arg,		/* Function arguement description */
    rec_function_end,           /* End of function and its args */
    rec_gen_id,                 /* From blr_gen_id */
    rec_type,			/* Type of field */
    rec_filter,			/* Filter */
    rec_trigger_message,	/* Trigger message texts */
    rec_user_privilege,		/* User privilege */
    rec_array,		/* 23*/	/* Array blob */
    rec_field_dimensions,	/* Array field dimensions */
    rec_files,			/* files for shadowing */
    rec_generator,		/* another format for gen-ids */
    rec_procedure,		/* Stored procedure */
    rec_procedure_prm,		/* Stored procedure parameters */
    rec_procedure_end,          /* End of procedure and its args */
    rec_exception,              /* Exception */
    rec_rel_constraint,		/* Relation constraints          */
    rec_ref_constraint,		/* Referential constraints       */
    rec_chk_constraint,		/* Check constraints          */
    rec_charset,		/* Character sets */
    rec_collation,		/* Collations */
    rec_sql_roles		/* SQL roles */
} REC_TYPE;


/* The order of battle for major records is:

    [<rec_physical_database>] <rec_database> <global fields> <field dimensions> <relation> <function> 
     <types> <filters> <relation data> <trigger-new> <trigger messages> <user privileges> <security> rec_end
       
where each relation is:

    <rec_relation> <rel name> <att_end> <local fields> <view> <rec_relation_end>
                                                  
where local fields is:
    <rec_field> <att_field_... att_field_dimensions, att_field_range_low, att_field_range_high...> <att_end>

where each relation data is:

    <rec_relation_data> <rel attributes> <gen id> <indices> <data> <trigger-old> <rec_relation_end>

where data is:
    <rec_data> <rec length> [<xdr_length>]  <data attr>  [ <blob>, <array>...]

and <blob> is

    <rec_blob> <blob_field_number> <max_sigment> <blob_type> <number_segments> <blob_data>

and <array> is

    <rec_array> <blob_field_number> <att_array_dimensions> <att_array_range_low> 
	<att_array_range_high> <blob_data> [<att_xdr_array>]

                                                              
where each function is:

    <rec_function> <function attributes> <att_end> [<function_arguments>] <rec_function_end> 

and <function_arguments> is

    <rec_function_arg> <function argument attributes> <att_end>

where trigger-old is:
    <rec_trigger> <trig_type> <trig_blr> <trig_source> <att_end>

and trigger-new is:
    <rec_trigger> <trig_type> <trig_blr> <trig_source> <trigger name> <relation name> <trig_seqence>
                  <description> <system flag> <att_end>


*/

/* Attributes within major record */

/* CAREFUL not to pull the lastest version into maint version without
   modifying the att_backup_format to be one version back */

/* ATT_BACKUP_FORMAT has been increased to 5. It allows us to distinguish
   backup format between IB3.3/IB4.0 and IB4.5 in case of migration
   problem */
/* Version 6:		Supports SQL Time & Date columns.
			RDB$FIELD_PRECISION
			SQL Dialect from database header
			SQL_INT64 columns and generator values
 */
#define ATT_BACKUP_FORMAT	6

/* format version number for ranges for arrays */

#define GDS_NDA_VERSION		1

/* max array dimension */

#define MAX_DIMENSION		16

#define SERIES	1

enum att_type {
    att_end = 0,		/* end of major record */

    /* Backup program attributes */

    att_backup_date = SERIES,	/* date of backup */
    att_backup_format,		/* backup format version */
    att_backup_os,		/* backup operating system */
    att_backup_compress,
    att_backup_transportable,	/* XDR datatypes for user data */
    att_backup_blksize,		/* backup block size */
    att_backup_file,		/* database file name */
    att_backup_volume,		/* backup volume number */

    /* Database attributes  */

    att_file_name = SERIES,	/* database file name (physical) */
    att_file_size,		/* size of original database (physical) */
    att_jrd_version,		/* jrd version (physical) */
    att_creation_date,		/* database creation date (physical) */
    att_page_size,		/* page size of original database (physical) */
    att_database_description,	/* description from RDB$DATABASE (logical) */
    att_database_security_class,	/* database level security (logical) */
    att_sweep_interval,		/* sweep interval */
    att_no_reserve,		/* don't reserve space for versions */
    att_database_description2,
    att_database_dfl_charset,	/* default character set from RDB$DATABASE */
    att_forced_writes,		/* syncronous writes flag */
    att_page_buffers,		/* page buffers for buffer cache */
    att_SQL_dialect,		/* SQL dialect that it speaks */
    att_db_read_only,		/* Is the database ReadOnly? */

    /* Relation attributes */

    att_relation_name = SERIES,
    att_relation_view_blr,
    att_relation_description,
    att_relation_record_length,	/* Record length in file */
    att_relation_view_relation,
    att_relation_view_context,
    att_relation_system_flag,
    att_relation_security_class,
    att_relation_view_source,
    att_relation_dummy,         /* this space available */
    att_relation_ext_description,
    att_relation_owner_name,
    att_relation_description2,
    att_relation_view_source2,
    att_relation_ext_description2,
    att_relation_flags,  
    att_relation_ext_file_name, /* name of file for external tables */

    /* Field attributes (used for both global and local fields) */

    att_field_name = SERIES,	/* name of field */
    att_field_source,		/* Global field name for local field */

    att_base_field,		/* Source field for view */
    att_view_context,		/* Context variable for view definition */

    att_field_query_name,	/* Query attributes */
    att_field_query_header,
    att_field_edit_string,

    att_field_type,		/* Physical attributes */
    att_field_sub_type,
    att_field_length,		/* 10 */
    att_field_scale,		
    att_field_segment_length,
    att_field_position,		/* Field position in relation (not in file) */
    att_field_offset,		/* Offset in data record (local fields only) */

    att_field_default_value,	/* Fluff */
    att_field_description,	
    att_field_missing_value,
    att_field_computed_blr,
    att_field_computed_source,
    att_field_validation_blr,	/* 20 */
    att_field_validation_source,
    att_field_number,		/* Field number to match up blobs */
    att_field_computed_flag,	/* Field is computed, not real */
    att_field_system_flag,	/* Interesting system flag */
    att_field_security_class,
    
    att_field_external_length,
    att_field_external_type,
    att_field_external_scale,
    att_field_dimensions,       /* 29 */
    att_field_ranges,           /* this space for rent */
    att_field_complex_name,     /* relation field attribute */
    att_field_range_low,        /* low range for array */
    att_field_range_high,       /* high range for array */
    att_field_update_flag,
    att_field_description2,
    att_field_validation_source2,
    att_field_computed_source2,
    att_field_null_flag,	/* If field can be null    */
    att_field_default_source,	/* default source for field (new fmt only) */
    att_field_missing_source,	/* missing source for field (new fmt only) */
    att_field_character_length,	/* length of field in characters */
    att_field_character_set,	/* Charset id of field */
    att_field_collation_id,	/* Collation id of field */
    att_field_precision,	/* numeric field precision of RDB$FIELDS */

    /* Index attributes */

    att_index_name = SERIES,
    att_segment_count,
    att_index_inactive,
    att_index_unique_flag,
    att_index_field_name,
    att_index_description,
    att_index_type,
    att_index_foreign_key,
    att_index_description2,
    att_index_expression_source,
    att_index_expression_blr,

    /* Data record */

    att_data_length = SERIES,
    att_data_data,

    /* Blob record */

    att_blob_field_number = SERIES + 2,	/* Field number of blob field */
    att_blob_type,			/* Segmented = 0, stream = 1 */
    att_blob_number_segments,		/* Number of segments */
    att_blob_max_segment,		/* Longest segment */
    att_blob_data,

    /* View attributes */

    att_view_relation_name = SERIES + 7,
    att_view_context_id,
    att_view_context_name,

    /* Security class attributes */

    att_class_security_class = SERIES + 10,
    att_class_acl,
    att_class_description,           


    /* Array attributes */

    att_array_dimensions = SERIES + 13,
    att_array_range_low,
    att_array_range_high,

    /* XDR encoded data attributes */

    att_xdr_length = SERIES + 16,
    att_xdr_array, 
    att_class_description2,

    /* Trigger attributes */

    att_trig_type = SERIES,
    att_trig_blr,
    att_trig_source,
    att_trig_name,
    att_trig_relation_name,
    att_trig_sequence,
    att_trig_description,
    att_trig_system_flag,     
    att_trig_inactive,
    att_trig_source2,
    att_trig_description2,
    att_trig_flags,     

    /* Function attributes */

    att_function_name = SERIES,
    att_function_description,
    att_function_class,
    att_function_module_name,
    att_function_entrypoint,
    att_function_return_arg,
    att_function_query_name,
    att_function_type,
    att_function_description2,

    /* Function argument attributes */

    att_functionarg_name = SERIES,
    att_functionarg_position,
    att_functionarg_mechanism,
    att_functionarg_field_type,
    att_functionarg_field_scale,
    att_functionarg_field_length,
    att_functionarg_field_sub_type,
    att_functionarg_character_set,
    att_functionarg_field_precision,
 
    /* TYPE relation attributes */
    att_type_name = SERIES,
    att_type_type,
    att_type_field_name,
    att_type_description,
    att_type_system_flag,
    /* Also see att_type_description2 below! */

    /* Filter attributes */
    att_filter_name,
    att_filter_description,
    att_filter_module_name,
    att_filter_entrypoint,
    att_filter_input_sub_type,
    att_filter_output_sub_type,
    att_filter_description2,
    att_type_description2,

    /* Trigger message attributes */
    att_trigmsg_name = SERIES,
    att_trigmsg_number,
    att_trigmsg_text,

    /* User privilege attributes */
    att_priv_user = SERIES,
    att_priv_grantor,
    att_priv_privilege,
    att_priv_grant_option,
    att_priv_object_name,
    att_priv_field_name,
    att_priv_user_type,
    att_priv_obj_type,

    /* files for shadowing purposes */
    att_file_filename = SERIES,
    att_file_sequence,
    att_file_start,
    att_file_length,
    att_file_flags,
    att_shadow_number,

    /* Attributes for gen_id */
    att_gen_generator = SERIES,
    att_gen_value,
    att_gen_value_int64,

    /* Stored procedure attributes */

    att_procedure_name = SERIES,
    att_procedure_inputs,
    att_procedure_outputs,
    att_procedure_description,
    att_procedure_description2,
    att_procedure_source,
    att_procedure_source2,
    att_procedure_blr,
    att_procedure_security_class,
    att_procedure_owner_name,

    /* Stored procedure parameter attributes */

    att_procedureprm_name = SERIES,
    att_procedureprm_number,
    att_procedureprm_type,
    att_procedureprm_field_source,
    att_procedureprm_description,
    att_procedureprm_description2,

    /* Exception attributes */

    att_exception_name = SERIES,
    att_exception_msg,
    att_exception_description,
    att_exception_description2,

    /* Relation constraints attributes */

    att_rel_constraint_name = SERIES,
    att_rel_constraint_type,
    att_rel_constraint_rel_name,
    att_rel_constraint_defer,
    att_rel_constraint_init,
    att_rel_constraint_index,

   /* Referential constraints attributes */
   
   att_ref_constraint_name = SERIES,
   att_ref_unique_const_name,
   att_ref_match_option,
   att_ref_update_rule,
   att_ref_delete_rule,

   /* SQL roles attributes */
   att_role_name = SERIES,
   att_role_owner_name,

   /* Check constraints attributes */
   att_chk_constraint_name = SERIES,
   att_chk_trigger_name,

   /* Character Set attributes */
   att_charset_name = SERIES,
   att_charset_form,
   att_charset_numchar,
   att_charset_coll,
   att_charset_id,
   att_charset_sysflag,
   att_charset_description,
   att_charset_funct,
   att_charset_bytes_char,
   
   att_coll_name = SERIES,
   att_coll_id,
   att_coll_cs_id,
   att_coll_attr,
   att_coll_subtype,		/* Unused: 93-11-12 Daves */
   att_coll_sysflag,
   att_coll_description,
   att_coll_funct
};

#define ATT_TYPE	enum att_type

typedef SCHAR		att_type;

/* Trigger types */

enum trig_t {
    trig_pre_store = 1,   /* default */
    trig_pre_modify,      /* default */
    trig_post_erase       /* default */
};                  
                 
/* these types to go away when recognized by gpre as
   <relation>.<field>.<type>  some time in the future  */

#define TRIG_TYPE_PRE_STORE  1
#define TRIG_TYPE_PRE_MODIFY 3
#define TRIG_TYPE_POST_ERASE 6

/* default trigger name templates */

#define TRIGGER_SEQUENCE_DEFAULT	0

/* common structure definitions */

/* field block, used to hold local field definitions */

typedef struct fld {
    struct fld	*fld_next;
    SSHORT	fld_type;
    SSHORT	fld_sub_type;
    FLD_LENGTH	fld_length;
    SSHORT	fld_scale;
    SSHORT	fld_position;
    SSHORT	fld_parameter;
    SSHORT	fld_missing_parameter;
    SSHORT	fld_id;
    RCRD_OFFSET	fld_offset;
    RCRD_OFFSET	fld_old_offset;
    SSHORT	fld_number;
    SSHORT	fld_system_flag;
    SSHORT	fld_name_length;
    TEXT	fld_name [32];
    TEXT	fld_source [32];
    TEXT	fld_base [32];
    TEXT	fld_query_name [32];
    TEXT	fld_security_class [32];
    SSHORT	fld_edit_length;
    SSHORT	fld_view_context;
    SSHORT	fld_update_flag;
    SSHORT	fld_flags;
    TEXT	fld_edit_string [256];
    GDS__QUAD	fld_description;
    GDS__QUAD   fld_query_header;
    TEXT	fld_complex_name [32];
    SSHORT	fld_dimensions;
    SLONG	fld_ranges [2 * MAX_DIMENSION];
    SSHORT	fld_null_flag;
    GDS__QUAD	fld_default_value;
    GDS__QUAD	fld_default_source;
    SSHORT	fld_character_length;
    SSHORT	fld_character_set_id;
    SSHORT	fld_collation_id;
} *FLD;

#define FLD_computed		1
#define FLD_position_missing	2
#define FLD_array		4
#define FLD_update_missing	8
#define FLD_null_flag		16
#define FLD_charset_flag	32	/* column has global charset */
#define FLD_collate_flag	64	/* local column has specific collation */

/* relation definition - holds useful relation type stuff */

typedef struct rel {
    struct rel		*rel_next;
    struct fld		*rel_fields;
    SSHORT		rel_flags;
    SSHORT		rel_id;
    SSHORT		rel_name_length;
    GDS_NAME		rel_name;
    GDS_NAME		rel_owner;		/* relation owner, if not us */
    GDS__QUAD		rel_store_blr;		/* trigger blr blob id */
    GDS__QUAD		rel_store_source;	/* trigger source blob id */
    GDS__QUAD		rel_modify_blr;		/* trigger blr blob id */
    GDS__QUAD		rel_modify_source;	/* trigger source blob id */
    GDS__QUAD		rel_erase_blr;		/* trigger blr blob id */
    GDS__QUAD		rel_erase_source;	/* trigger source blob id */
} *REL;

#define REL_view	1
#define REL_external	2

/* procedure definition - holds useful procedure type stuff */

typedef struct prc {
    struct prc	*prc_next;
    SSHORT	prc_name_length;
    GDS_NAME	prc_name;
    GDS_NAME	prc_owner;		/* relation owner, if not us */
} *PRC;


typedef struct gfld {
    TEXT	gfld_name [32];
    ISC_QUAD	gfld_vb;
    ISC_QUAD	gfld_vs;
    ISC_QUAD	gfld_vs2;
    struct gfld	*gfld_next;
    USHORT	gfld_flags;
} *GFLD;

#define GFLD_validation_blr	1
#define GFLD_validation_source	2
#define GFLD_validation_source2	4

#define MAX_FILE_NAME_LENGTH	256

/* Note that this typedef is also defined in JRD.H and REMOTE.H */
/* but for some reason we are avoiding including JRD.H          */
/* and this typedef is needed to include SVC.H                  */
#if !(defined _REMOTE_REMOTE_H_ || defined _JRD_JRD_H_)
typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id_mod;
    USHORT	blk_length;
} *BLK;
#endif

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"

/* typedef the file descriptor type to make things cleaner */

#ifdef WIN_NT
typedef void *DESC;
#define CLOSE	CloseHandle 
#define UNLINK  DeleteFile
#define FLUSH(x) FlushFileBuffers(x)
#else
typedef int DESC;
#define CLOSE	close
#define UNLINK  unlink
#define FLUSH(x) /* nothing */
#define INVALID_HANDLE_VALUE    -1
#endif /* WIN_NT */

/* File block -- for multi-file databases */

typedef enum {
    size_n = 0,		/* none */
    size_k,		/* k = 1024 */
    size_m,		/* m = k x 1024 */
    size_g,		/* g = m x 1024 */
    size_e		/* error */
} SIZE_CODE;

typedef struct fil {
    struct fil	*fil_next;
    TEXT	*fil_name;
    ULONG	fil_length;
    DESC	fil_fd;
    USHORT	fil_seq;
    SIZE_CODE	fil_size_code;
} *FIL;

#define FIL_LEN		sizeof (struct fil)

/* Split & Join stuff */

typedef enum act_t {
    ACT_unknown,        /* action is unknown */
    ACT_backup,
    ACT_backup_split,
    ACT_restore,
    ACT_restore_join
} ACT_T;

typedef struct act {
        USHORT  act_total;
        FIL     act_file;
        ACT_T   act_action;
} *ACT;

#define ACT_LEN         sizeof (struct act)

#define MAX_LENGTH	-1

/* This structure has been cloned from spit.c */

typedef struct hdr_split {
        TEXT    hdr_split_tag[18];
        TEXT    hdr_split_timestamp[30];
        TEXT    hdr_split_text1[11];
        TEXT    hdr_split_sequence[4];  /* File sequence number */
        TEXT    hdr_split_text2[4];
        TEXT    hdr_split_total[4];     /* Total number of files */
        TEXT    hdr_split_text3[2];
        TEXT    hdr_split_name[27];     /* File name */
}       *HDR_SPLIT;

/* NOTE: size of the hdr_split_tag and HDR_SPLIT_TAG must be the same and equal
   to 18. Otherwise we will not be able to join the gbk files v5.x */

#define HDR_SPLIT_SIZE          sizeof (struct hdr_split)
#define HDR_SPLIT_TAG5          "InterBase/gsplit, "
#define HDR_SPLIT_TAG6          "InterBase/gbak,   "
#define HDR_SPLIT_TAG           HDR_SPLIT_TAG6
#define MIN_SPLIT_SIZE          2048    /* bytes */

/* Global switches and data */

typedef struct tgbl {
    struct thdd tgbl_thd_data;
    TEXT	*gbl_database_file_name;
    TEXT	gbl_backup_start_time [30];
    USHORT	gbl_sw_verbose;
    USHORT	gbl_sw_ignore_limbo;
    USHORT	gbl_sw_meta;
    USHORT	gbl_sw_novalidity; 
    USHORT	gbl_sw_page_size;
    USHORT	gbl_sw_compress;
    USHORT	gbl_sw_version;
    USHORT	gbl_sw_transportable;
    USHORT	gbl_sw_incremental;
    USHORT	gbl_sw_deactivate_indexes;
    USHORT	gbl_sw_kill;
    USHORT	gbl_sw_blk_factor;
    USHORT	gbl_sw_no_reserve;
    USHORT	gbl_sw_old_descriptions;
    USHORT	gbl_sw_service_gbak;
    USHORT	gbl_sw_service_thd;
    USHORT	gbl_sw_bug8183;
    USHORT	gbl_sw_convert_ext_tables;
    BOOLEAN	gbl_sw_mode;
    BOOLEAN	gbl_sw_mode_val;
    SCHAR	*gbl_sw_sql_role;
    SCHAR	*gbl_sw_user;
    SCHAR	*gbl_sw_password;
    SLONG	gbl_sw_skip_count;
    SLONG	gbl_sw_page_buffers;
    FIL		gbl_sw_files;
    FIL		gbl_sw_backup_files;
    GFLD	gbl_global_fields;
    ACT		action;
    ULONG	io_buffer_size;
    USHORT	sw_redirect;
    UCHAR	dpb_string [100];
    SSHORT	dpb_length;
    UCHAR	*burp_env;
    UCHAR	*io_ptr;
    int	   	io_cnt;
    REL	   	relations;
    PRC	   	procedures;
    SLONG   	BCK_capabilities;
    USHORT	RESTORE_format;
    ULONG	mvol_io_buffer_size;
    ULONG	mvol_actual_buffer_size;
    ULONG   	mvol_cumul_count;
    UCHAR	*mvol_io_ptr;
    int	   	mvol_io_cnt;
    UCHAR   	*mvol_io_buffer;
    UCHAR	*mvol_io_volume;
    UCHAR   	*mvol_io_header;
    UCHAR   	*mvol_io_data;
    TEXT	mvol_db_name_buffer [MAX_FILE_NAME_LENGTH];
    SCHAR	mvol_old_file [MAX_FILE_NAME_LENGTH];
    int	   	mvol_volume_count;
    USHORT	mvol_empty_file;
    isc_db_handle db_handle;
    isc_tr_handle tr_handle;
    isc_tr_handle global_trans;
    DESC    	file_desc;
    long	*status;		/* points to either the tdgbl status or service status */
    long    	status_vector [ISC_STATUS_LENGTH];
    int     	exit_code;
    UCHAR   	*head_of_mem_list;
    OUTPUTPROC  output_proc;
    SLONG       output_data;
    IB_FILE        *output_file;
    SVC		service_blk;
    /*
     * Link list of global fields that were converted from V3 sub_type
     * to V4 char_set_id/collate_id. Needed for local fields conversion.
     */
    FLD		v3_cvt_fld_list;

    isc_req_handle	handles_get_character_sets_req_handle1;
    isc_req_handle	handles_get_chk_constraint_req_handle1;
    isc_req_handle	handles_get_collation_req_handle1;
    isc_req_handle	handles_get_exception_req_handle1;
    isc_req_handle	handles_get_field_dimensions_req_handle1;
    isc_req_handle	handles_get_field_req_handle1;
    isc_req_handle	handles_get_fields_req_handle1;
    isc_req_handle	handles_get_fields_req_handle2;
    isc_req_handle	handles_get_fields_req_handle3;
    isc_req_handle	handles_get_fields_req_handle4;
    isc_req_handle	handles_get_fields_req_handle5;
    isc_req_handle	handles_get_fields_req_handle6;
    isc_req_handle	handles_get_files_req_handle1;
    isc_req_handle	handles_get_filter_req_handle1;
    isc_req_handle	handles_get_function_arg_req_handle1;
    isc_req_handle	handles_get_function_req_handle1;
    isc_req_handle	handles_get_global_field_req_handle1;
    isc_req_handle	handles_get_index_req_handle1;
    isc_req_handle	handles_get_index_req_handle2;
    isc_req_handle	handles_get_index_req_handle3;
    isc_req_handle	handles_get_index_req_handle4;
    isc_req_handle	handles_get_procedure_prm_req_handle1;
    isc_req_handle	handles_get_procedure_req_handle1;
    isc_req_handle	handles_get_ranges_req_handle1;
    isc_req_handle	handles_get_ref_constraint_req_handle1;
    isc_req_handle	handles_get_rel_constraint_req_handle1;
    isc_req_handle	handles_get_relation_req_handle1;
    isc_req_handle	handles_get_security_class_req_handle1;
    isc_req_handle	handles_get_sql_roles_req_handle1;
    isc_req_handle	handles_get_trigger_message_req_handle1;
    isc_req_handle	handles_get_trigger_message_req_handle2;
    isc_req_handle	handles_get_trigger_old_req_handle1;
    isc_req_handle	handles_get_trigger_req_handle1;
    isc_req_handle	handles_get_type_req_handle1;
    isc_req_handle	handles_get_user_privilege_req_handle1;
    isc_req_handle	handles_get_view_req_handle1;
    isc_req_handle	handles_put_index_req_handle1;
    isc_req_handle	handles_put_index_req_handle2;
    isc_req_handle	handles_put_index_req_handle3;
    isc_req_handle	handles_put_index_req_handle4;
    isc_req_handle	handles_put_index_req_handle5;
    isc_req_handle	handles_put_index_req_handle6;
    isc_req_handle	handles_put_index_req_handle7;
    isc_req_handle	handles_put_relation_req_handle1;
    isc_req_handle	handles_put_relation_req_handle2;
    isc_req_handle	handles_store_blr_gen_id_req_handle1;
    isc_req_handle	handles_write_function_args_req_handle1;
    isc_req_handle	handles_write_function_args_req_handle2;
    isc_req_handle	handles_write_procedure_prms_req_handle1;
    USHORT	hdr_forced_writes;
    TEXT	database_security_class[32]; /* To save database security class for deferred update */
} *TGBL;

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#ifdef SUPERSERVER
#define GET_THREAD_DATA	        ((TGBL) THD_get_specific())
#define SET_THREAD_DATA         THD_put_specific ((THDD) tdgbl); \
				tdgbl->tgbl_thd_data.thdd_type = THDD_TYPE_TGBL
#define RESTORE_THREAD_DATA     THD_restore_specific();
#else
extern struct tgbl *gdgbl;

#define GET_THREAD_DATA	        (gdgbl)
#define SET_THREAD_DATA         gdgbl = tdgbl; \
				tdgbl->tgbl_thd_data.thdd_type = THDD_TYPE_TGBL
#define RESTORE_THREAD_DATA     
#endif

#define EXIT(code)	            {  tdgbl->exit_code = (code);          \
												if (tdgbl->burp_env != NULL)        \
											  		LONGJMP(tdgbl->burp_env, 1);  }

#define FINI_DB_NOT_ONLINE	2	/* database is not on-line due to
                                           failure to activate one or more
                                           indices */

/* I/O definitions */

#ifndef IO_BUFFER_SIZE
#ifdef BUFSIZ
#define IO_BUFFER_SIZE		BUFSIZ
#else
#define IO_BUFFER_SIZE		1024
#endif
#endif

#define GBAK_IO_BUFFER_SIZE	(16 * (IO_BUFFER_SIZE))

#ifdef APOLLO
#define CT_BUFFER_SIZE		512
#endif

/* Burp will always write a backup in multiples of the following number
 * of bytes.  The initial value is the smallest which ensures that writes
 * to fixed-block SCSI tapes such as QIC-150 will work.  The value should
 * always be a multiple of 512 for that reason.
 * If you change to a value which is NOT a power of 2, then change the
 * BURP_UP_TO_BLOCK macro to use division and multiplication instead of
 * bit masking.
 */

#define BURP_BLOCK		512
#define BURP_UP_TO_BLOCK(n)	(((n) + BURP_BLOCK - 1) & ~(BURP_BLOCK - 1))

/* Move the read and write mode declarations in here from burp.c
   so that other files can see them for multivolume opens */

#ifdef NETWARE_386
#define MODE_WRITE	O_RDWR | O_CREAT | O_TRUNC
#endif

#ifdef PC_PLATFORM
#ifndef NETWARE_386
#define MODE_READ	O_RDONLY | O_BINARY
#define MODE_WRITE	O_WRONLY | O_BINARY | O_CREAT
#endif
#endif

#ifdef OS2_ONLY
#define MODE_READ	O_RDONLY | O_BINARY
#define MODE_WRITE	O_WRONLY | O_BINARY | O_CREAT
#endif

#ifdef WIN_NT
#define MODE_READ	GENERIC_READ
#define MODE_WRITE	GENERIC_WRITE
#endif

#ifdef VMS
#define MODE_WRITE	O_WRONLY | O_CREAT | O_TRUNC
#endif

#ifndef MODE_READ
#define MODE_READ	O_RDONLY
#endif

#ifndef MODE_WRITE
#define MODE_WRITE	O_WRONLY | O_CREAT
#endif

/* Burp Messages */

#define msgVerbose_write_charsets	211
#define msgVerbose_write_collations	212
#define msgErr_restore_charset		213
#define msgVerbose_restore_charset	214
#define msgErr_restore_collation	215
#define msgVerbose_restore_collation	216


#endif /* _BURP_BURP_H_ */
