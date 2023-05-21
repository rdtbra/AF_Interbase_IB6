/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		fields.h
 *	DESCRIPTION:	Global system relation fields
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

#ifndef BLOB_SIZE
#ifdef _CRAY
#define BLOB_SIZE	16
#else
#define BLOB_SIZE	8
#endif
#endif

#define TIMESTAMP_SIZE	8

FIELD (fld_context,	nam_v_context,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_ctx_name,	nam_context,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_description,	nam_description, dtype_blob,	BLOB_SIZE, BLOB_text, 0, NULL)
FIELD (fld_edit_string,	nam_edit_string, dtype_varying,	127, 0, 0, NULL)
FIELD (fld_f_id,	nam_f_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_f_name,	nam_f_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_flag,	nam_sys_flag,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_i_id,	nam_i_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_i_name,	nam_i_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_f_length,	nam_f_length,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_f_position,	nam_f_position,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_f_scale,	nam_f_scale,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_f_type,	nam_f_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_format,	nam_fmt,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_key_length,	nam_key_length,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_p_number,	nam_p_number,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_p_sequence,	nam_p_sequence,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_p_type,	nam_p_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_q_header,	nam_q_header,	dtype_blob,	BLOB_SIZE, BLOB_text, 0, NULL)
FIELD (fld_r_id,	nam_r_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_r_name,	nam_r_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_s_count,	nam_s_count,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_s_length,	nam_s_length,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_source,	nam_source,	dtype_blob,	BLOB_SIZE, BLOB_text, 0, NULL)
FIELD (fld_sub_type,	nam_f_sub_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_v_blr,	nam_v_blr,	dtype_blob,	BLOB_SIZE, BLOB_blr, 0, NULL)
FIELD (fld_validation,	nam_vl_blr,	dtype_blob,	BLOB_SIZE, BLOB_blr, 0, NULL)
FIELD (fld_value,	nam_value,	dtype_blob,	BLOB_SIZE, BLOB_blr, 0, NULL)
FIELD (fld_class,	nam_class,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_acl,		nam_acl,	dtype_blob,	BLOB_SIZE, BLOB_acl, 0, NULL)
FIELD (fld_file_name,	nam_file_name,	dtype_varying,	255, 0, 0, NULL)
FIELD (fld_file_seq,	nam_file_seq,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_file_start,	nam_file_start,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_file_length,	nam_file_length, dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_file_flags,	nam_file_flags,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_trigger,	nam_trigger,	dtype_blob,	BLOB_SIZE, BLOB_blr, 0, NULL)

FIELD (fld_trg_name,	nam_trg_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_gnr_name,	nam_gnr_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_fun_name,	nam_fun_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_ext_name,	nam_ext_name,	dtype_text,	31, 0, 0, NULL)
FIELD (fld_typ_name,	nam_typ_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_dimensions,	nam_dimensions,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_runtime,	nam_runtime,	dtype_blob,	BLOB_SIZE, BLOB_summary, 0, NULL)
FIELD (fld_trg_seq,	nam_trg_seq,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_gnr_type,	nam_gnr_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_trg_type,	nam_trg_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_obj_type,	nam_obj_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_mechanism,	nam_mechanism,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_f_descr,	nam_desc,	dtype_blob,	BLOB_SIZE, BLOB_format, 0, NULL)
FIELD (fld_fun_type,	nam_fun_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_trans_id,	nam_trans_id,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_trans_state,	nam_trans_state, dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_time,	nam_time,	dtype_timestamp,TIMESTAMP_SIZE, 0, 0, NULL)
FIELD (fld_trans_desc,	nam_trans_desc,	dtype_blob,	BLOB_SIZE, BLOB_tra, 0, NULL)
FIELD (fld_msg,		nam_msg,	dtype_varying,	80, 0, 0, NULL)
FIELD (fld_msg_num,	nam_msg_num,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_user,	nam_user,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_privilege,	nam_privilege,	dtype_text,	6, 0, 0, NULL)
FIELD (fld_ext_desc,	nam_ext_desc,	dtype_blob,	BLOB_SIZE, BLOB_extfile, 0, NULL)
FIELD (fld_shad_num,	nam_shad_num,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_gen_name,	nam_gen_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_gen_id,	nam_gen_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_bound,	nam_bound,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_dim,		nam_dim,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_statistics,	nam_statistics,	dtype_double,	sizeof (double), 0, 0, NULL)
FIELD (fld_null_flag,	nam_null_flag,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_con_name,	nam_con_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_con_type,	nam_con_type,	dtype_text,	11, 0, 0, NULL)
FIELD (fld_defer,	nam_defer,	dtype_text,	3, 0, 0, dflt_no)
FIELD (fld_match,	nam_match,	dtype_text,	7, 0, 0, dflt_full)
FIELD (fld_rule,	nam_rule,	dtype_text,	11, 0, 0, dflt_restrict)
FIELD (fld_file_partitions,nam_file_partitions,dtype_short,sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_prc_blr,	nam_prc_blr,	dtype_blob,	BLOB_SIZE, BLOB_blr, 0, NULL)
FIELD (fld_prc_id,	nam_prc_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_prc_prm,	nam_prc_prm,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_prc_name,	nam_prc_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_prm_name,	nam_prm_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_prm_number,	nam_prm_number,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_prm_type,	nam_prm_type,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)

FIELD (fld_charset_name,nam_charset_name,dtype_text,	31, dsc_text_type_metadata, 0, NULL )
FIELD (fld_charset_id,	nam_charset_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_collate_name,nam_collate_name,dtype_text,	31, dsc_text_type_metadata, 0, NULL )
FIELD (fld_collate_id,	nam_collate_id,	dtype_short,	sizeof (SSHORT), 0, 0, NULL)
FIELD (fld_num_chars,	nam_num_chars,	dtype_long,	sizeof (SLONG),  0, 0, NULL)
FIELD (fld_xcp_name,	nam_xcp_name,	dtype_text,	31, dsc_text_type_metadata, 0, NULL)
FIELD (fld_xcp_number,	nam_xcp_number,	dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_file_p_offset,nam_file_p_offset,dtype_long,	sizeof (SLONG), 0, 0, NULL)
FIELD (fld_f_precision,	nam_f_precision,dtype_short,	sizeof (SSHORT), 0, 0, NULL)
