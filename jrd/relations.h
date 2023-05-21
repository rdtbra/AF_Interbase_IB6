/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		relations.h
 *	DESCRIPTION:	System relation definitions
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

RELATION(nam_pages,	rel_pages, ODS_8_0)
	FIELD(f_pag_page,	nam_p_number,	fld_p_number,	0,	0,	0,	0)
	FIELD(f_pag_id,		nam_r_id,	fld_r_id,	0,	0,	0,	0)
	FIELD(f_pag_seq,	nam_p_sequence,	fld_p_sequence, 0,	0,	0,	0)
	FIELD(f_pag_type,	nam_p_type,	fld_p_type,	0,	0,	0,	0)
END_RELATION

RELATION(nam_database,	rel_database, ODS_8_0)
	FIELD(f_dat_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_dat_id,		nam_r_id,	fld_r_id,	0,	0,	0,	0)
	FIELD(f_dat_class,	nam_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_dat_charset,	nam_charset_name,fld_charset_name,1,	0,	0,	0)	
END_RELATION

RELATION(nam_fields,	rel_fields, ODS_8_0)
	FIELD(f_fld_name,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_fld_qname,	nam_q_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_fld_v_blr,	nam_vl_blr,	fld_validation, 1,	0,	0,	0)
	FIELD(f_fld_v_source,	nam_vl_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_fld_computed,	nam_computed,	fld_value,	1,	0,	0,	0)
	FIELD(f_fld_csource,	nam_c_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_fld_default,	nam_default,	fld_value,	1,	0,	0,	0)
	FIELD(f_fld_dsource,	nam_d_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_fld_length,	nam_f_length,	fld_f_length,	1,	0,	0,	0)
	FIELD(f_fld_scale,	nam_f_scale,	fld_f_scale,	1,	0,	0,	0)
	FIELD(f_fld_type,	nam_f_type,	fld_f_type,	1,	0,	0,	0)
	FIELD(f_fld_sub_type,	nam_f_sub_type,	fld_sub_type,	1,	0,	0,	0)
	FIELD(f_fld_missing,	nam_missing,	fld_value,	1,	0,	0,	0)
	FIELD(f_fld_msource,	nam_m_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_fld_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_fld_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_fld_qheader,	nam_q_header,	fld_q_header,	1,	0,	0,	0)
	FIELD(f_fld_seg_len,	nam_s_length,	fld_s_length,	1,	0,	0,	0)
	FIELD(f_fld_estring,	nam_edit_string,fld_edit_string, 1,	0,	0,	0)
	FIELD(f_fld_ext_length,	nam_ext_length,	fld_f_length,	1,	0,	0,	0)
	FIELD(f_fld_ext_scale,	nam_ext_scale,	fld_f_scale,	1,	0,	0,	0)
	FIELD(f_fld_ext_type,	nam_ext_type,	fld_f_type,	1,	0,	0,	0)
	FIELD(f_fld_dimensions,	nam_dimensions,	fld_dimensions,	1,	0,	0,	0)
	FIELD(f_fld_null_flag,	nam_null_flag,	fld_null_flag,	1,	0,	0,	0)
	FIELD(f_fld_char_length,nam_char_length,fld_f_length,	1,	0,	0,	0)
	FIELD(f_fld_coll_id,	nam_collate_id,	fld_collate_id,	1,	0,	0,	0)
	FIELD(f_fld_charset_id,	nam_charset_id,	fld_charset_id,	1,	0,	0,	0)
	FIELD(f_fld_precision,	nam_f_precision, fld_f_precision, 1,	0,	0,	0)
END_RELATION

RELATION(nam_i_segments,	rel_segments, ODS_8_0)
	FIELD(f_seg_name,	nam_i_name,	fld_i_name,	1,	0,	0,	0)
	FIELD(f_seg_field,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_seg_position,	nam_f_position,	fld_f_position, 1,	0,	0,	0)
END_RELATION

RELATION(nam_indices,	rel_indices, ODS_8_0)
	FIELD(f_idx_name,	nam_i_name,	fld_i_name,	1,	0,	0,	0)
	FIELD(f_idx_relation,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_idx_id,		nam_i_id,	fld_i_id,	0,	0,	0,	0)
	FIELD(f_idx_flag,	nam_un_flag,	fld_flag,	1,	0,	0,	0)
	FIELD(f_idx_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_idx_count,	nam_s_count,	fld_s_count,	1,	0,	0,	0)
	FIELD(f_idx_inactive,	nam_i_inactive,	fld_flag,	1,	0,	0,	0)
	FIELD(f_idx_type,	nam_i_type,	fld_flag,	1,	0,	0,	0)
	FIELD(f_idx_foreign,	nam_i_foreign,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_idx_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_idx_exp_blr,	nam_exp_blr,	fld_value,	1,	0,	0,	0)
	FIELD(f_idx_exp_source,	nam_exp_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_idx_statistics,	nam_statistics,	fld_statistics,	1,	0,	0,	0)
END_RELATION

RELATION(nam_r_fields,	rel_rfr, ODS_8_0)
	FIELD(f_rfr_fname,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_rfr_rname,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_rfr_sname,	nam_f_source,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_rfr_qname,	nam_q_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_rfr_base,	nam_base,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_rfr_estring,	nam_edit_string,fld_edit_string, 1,	0,	0,	0)
	FIELD(f_rfr_position,	nam_f_position,	fld_f_position, 1,	0,	0,	0)
	FIELD(f_rfr_qheader,	nam_q_header,	fld_q_header,	1,	0,	0,	0)
	FIELD(f_rfr_flag,	nam_up_flag,	fld_flag,	1,	0,	0,	0)
	FIELD(f_rfr_id,		nam_f_id,	fld_f_id,	0,	0,	0,	0)
	FIELD(f_rfr_context,	nam_v_context,	fld_context,	1,	0,	0,	0)
	FIELD(f_rfr_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_rfr_default,	nam_default,	fld_value,	1,	0,	0,	0)
	FIELD(f_rfr_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_rfr_class,	nam_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_rfr_complex,	nam_cpx_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_rfr_null_flag,	nam_null_flag,	fld_null_flag,	1,	0,	0,	0)
	FIELD(f_rfr_dsource,	nam_d_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_rfr_coll_id,	nam_collate_id,	fld_collate_id,	1,	0,	0,	0)
END_RELATION

RELATION(nam_relations,	rel_relations, ODS_8_0)
	FIELD(f_rel_blr,	nam_v_blr,	fld_v_blr,	1,	0,	0,	0)
	FIELD(f_rel_source,	nam_v_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_rel_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_rel_id,		nam_r_id,	fld_r_id,	0,	0,	0,	0)
	FIELD(f_rel_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_rel_key_len,	nam_key_length,	fld_key_length, 0,	0,	0,	0)
	FIELD(f_rel_format,	nam_fmt,	fld_format,	0,	0,	0,	0)
	FIELD(f_rel_field_id,	nam_f_id,	fld_f_id,	0,	0,	0,	0)
	FIELD(f_rel_name,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_rel_class,	nam_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_rel_ext_file,	nam_ext_file,	fld_file_name,	1,	0,	0,	0)
	FIELD(f_rel_runtime,	nam_runtime,	fld_runtime,	1,	0,	0,	0)
	FIELD(f_rel_ext_desc,	nam_ext_desc,	fld_ext_desc,	1,	0,	0,	0)
	FIELD(f_rel_owner,	nam_owner,	fld_user,	1,	0,	0,	0)
	FIELD(f_rel_def_class,	nam_def_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_rel_flags,	nam_flags,	fld_flag,	0,	0,	0,	0)
END_RELATION

RELATION(nam_v_relations,	rel_vrel, ODS_8_0)
	FIELD(f_vrl_vname,	nam_v_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_vrl_rname,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_vrl_context,	nam_v_context,	fld_context,	1,	0,	0,	0)
	FIELD(f_vrl_cname,	nam_context,	fld_ctx_name,	1,	0,	0,	0)
END_RELATION

RELATION(nam_formats,	rel_formats, ODS_8_0)
	FIELD(f_fmt_rid,	nam_r_id,	fld_r_id,	0,	0,	0,	0)
	FIELD(f_fmt_format,	nam_fmt,	fld_format,	0,	0,	0,	0)
	FIELD(f_fmt_desc,	nam_desc,	fld_f_descr,	0,	0,	0,	0)
END_RELATION

RELATION(nam_classes, rel_classes, ODS_8_0)
	FIELD(f_cls_class,	nam_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_cls_acl,	nam_acl,	fld_acl,	1,	0,	0,	0)
	FIELD(f_cls_desc,	nam_description,fld_description, 1,	0,	0,	0)
END_RELATION

RELATION(nam_files, rel_files, ODS_8_0)
	FIELD(f_file_name,	nam_file_name,	fld_file_name,	1,	0,	0,	0)
	FIELD(f_file_seq,	nam_file_seq,	fld_file_seq,	1,	0,	0,	0)
	FIELD(f_file_start,	nam_file_start,	fld_file_start, 1,	0,	0,	0)
	FIELD(f_file_length,	nam_file_length,fld_file_length, 1,	0,	0,	0)
	FIELD(f_file_flags,	nam_file_flags,	fld_file_flags, 1,	0,	0,	0)
	FIELD(f_file_shad_num,	nam_shad_num,	fld_shad_num,	1,	0,	0,	0)
END_RELATION

RELATION(nam_types, rel_types, ODS_8_0)
	FIELD(f_typ_field,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_typ_type,	nam_type,	fld_gnr_type,	1,	0,	0,	0)
	FIELD(f_typ_name,	nam_typ_name,	fld_typ_name,	1,	0,	0,	0)
	FIELD(f_typ_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_typ_sys_flag,	nam_sys_flag,	fld_flag,	1,	0,	0,	0)
END_RELATION

RELATION(nam_trgs, rel_triggers, ODS_8_0)
	FIELD(f_trg_name,	nam_trg_name,	fld_trg_name,	1,	0,	0,	0)
	FIELD(f_trg_rname,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_trg_seq,	nam_trg_seq,	fld_trg_seq,	1,	0,	0,	0)
	FIELD(f_trg_type,	nam_trg_type,	fld_trg_type,	1,	0,	0,	0)
	FIELD(f_trg_source,	nam_trg_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_trg_blr,	nam_trigger,	fld_trigger,	1,	0,	0,	0)
	FIELD(f_trg_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_trg_inactive,	nam_trg_inactive,fld_flag,	1,	0,	0,	0)
	FIELD(f_trg_sys_flag,	nam_sys_flag,	fld_flag,	1,	0,	0,	0)
	FIELD(f_trg_flags,	nam_flags,	fld_flag,	1,	0,	0,	0)
END_RELATION

RELATION(nam_dpds, rel_dpds, ODS_8_0)
	FIELD(f_dpd_name,	nam_dpd_name,	fld_gnr_name,	1,	0,	0,	0)
	FIELD(f_dpd_o_name,	nam_dpd_o_name,	fld_gnr_name,	1,	0,	0,	0)
	FIELD(f_dpd_f_name,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_dpd_type,	nam_dpd_type,	fld_obj_type,	1,	0,	0,	0)
	FIELD(f_dpd_o_type,	nam_dpd_o_type,	fld_obj_type,	1,	0,	0,	0)
END_RELATION

RELATION(nam_funs, rel_funs, ODS_8_0)
	FIELD(f_fun_name,	nam_fun_name,	fld_fun_name,	1,	0,	0,	0)
	FIELD(f_fun_type,	nam_fun_type,	fld_fun_type,	1,	0,	0,	0)
	FIELD(f_fun_qname,	nam_q_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_fun_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_fun_module,	nam_mod_name,	fld_file_name,	1,	0,	0,	0)
	FIELD(f_fun_entry,	nam_entry,	fld_ext_name,	1,	0,	0,	0)
	FIELD(f_fun_ret_arg,	nam_ret_arg,	fld_f_position, 1,	0,	0,	0)
	FIELD(f_fun_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
END_RELATION

RELATION(nam_args, rel_args, ODS_8_0)
	FIELD(f_arg_fun_name,	nam_fun_name,	fld_fun_name,	1,	0,	0,	0)
	FIELD(f_arg_pos,	nam_arg_pos,	fld_f_position, 1,	0,	0,	0)
	FIELD(f_arg_mech,	nam_mechanism,	fld_mechanism,	1,	0,	0,	0)
	FIELD(f_arg_type,	nam_f_type,	fld_f_type,	0,	0,	0,	0)
	FIELD(f_arg_scale,	nam_f_scale,	fld_f_scale,	0,	0,	0,	0)
	FIELD(f_arg_length,	nam_f_length,	fld_f_length,	0,	0,	0,	0)
	FIELD(f_arg_sub_type,	nam_f_sub_type,	fld_sub_type,	0,	0,	0,	0)
	FIELD(f_arg_charset_id,	nam_charset_id,	fld_charset_id,	1,	0,	0,	0)
        FIELD(f_arg_precision,  nam_f_precision,fld_f_precision, 1,     0,      0,      0)
	FIELD(f_arg_char_length,nam_char_length,fld_f_length,	1,	0,	0,	0)
END_RELATION

RELATION(nam_filters, rel_filters, ODS_8_0)
	FIELD(f_flt_name,	nam_fun_name,	fld_fun_name,	1,	0,	0,	0)
	FIELD(f_flt_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_flt_module,	nam_mod_name,	fld_file_name,	1,	0,	0,	0)
	FIELD(f_flt_entry,	nam_entry,	fld_ext_name,	1,	0,	0,	0)
	FIELD(f_flt_input,	nam_in_type,	fld_sub_type,	1,	0,	0,	0)
	FIELD(f_flt_output,	nam_out_type,	fld_sub_type,	1,	0,	0,	0)
	FIELD(f_flt_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
END_RELATION

RELATION(nam_trg_msgs, rel_msgs, ODS_8_0)
	FIELD(f_msg_trigger,	nam_trg_name,	fld_trg_name,	1,	0,	0,	0)
	FIELD(f_msg_number,	nam_msg_num,	fld_msg_num,	1,	0,	0,	0)
	FIELD(f_msg_msg,	nam_msg,	fld_msg,	1,	0,	0,	0)
END_RELATION

RELATION(nam_user_privileges, rel_priv, ODS_8_0)
	FIELD(f_prv_user,	nam_user,	fld_user,	1,	0,	0,	0)
	FIELD(f_prv_grantor,	nam_grantor,	fld_user,	1,	0,	0,	0)
	FIELD(f_prv_priv,	nam_privilege,	fld_privilege,	1,	0,	0,	0)
	FIELD(f_prv_grant,	nam_grant,	fld_flag,	1,	0,	0,	0)
	FIELD(f_prv_rname,	nam_r_name,	fld_gnr_name,	1,	0,	0,	0)
	FIELD(f_prv_fname,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_prv_u_type,	nam_user_type,	fld_obj_type,	1,	0,	0,	0)
	FIELD(f_prv_o_type,	nam_obj_type,	fld_obj_type,	1,	0,	0,	0)
END_RELATION

RELATION(nam_trans, rel_trans, ODS_8_0)
	FIELD(f_trn_id,		nam_trans_id,	fld_trans_id,	1,	0,	0,	0)
	FIELD(f_trn_state,	nam_trans_state,fld_trans_state, 1,	0,	0,	0)
	FIELD(f_trn_time,	nam_time,	fld_time,	1,	0,	0,	0)
	FIELD(f_trn_desc,	nam_trans_desc,	fld_trans_desc, 1,	0,	0,	0)
END_RELATION

RELATION(nam_gens, rel_gens, ODS_8_0)
	FIELD(f_gen_name,	nam_gen_name,	fld_gen_name,	1,	0,	0,	0)
	FIELD(f_gen_id,		nam_gen_id,	fld_gen_id,	1,	0,	0,	0)
	FIELD(f_gen_sys_flag,	nam_sys_flag,	fld_flag,	1,	0,	0,	0)
END_RELATION

RELATION(nam_fld_dims, rel_dims, ODS_8_0)
	FIELD(f_dims_fname,	nam_f_name,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_dims_dim,	nam_dim,	fld_dim,	1,	0,	0,	0)
	FIELD(f_dims_lower,	nam_lower,	fld_bound,	1,	0,	0,	0)
	FIELD(f_dims_upper,	nam_upper,	fld_bound,	1,	0,	0,	0)
END_RELATION

RELATION(nam_rel_constr, rel_rcon, ODS_8_0)
	FIELD(f_rcon_cname,	nam_con_name,	fld_con_name,	1,	0,	0,	0)
	FIELD(f_rcon_ctype,	nam_con_type,	fld_con_type,	1,	0,	0,	0)
	FIELD(f_rcon_rname,	nam_r_name,	fld_r_name,	1,	0,	0,	0)
	FIELD(f_rcon_dfr,	nam_defer,	fld_defer,	1,	0,	0,	0)
	FIELD(f_rcon_idfr,	nam_init_defer,	fld_defer,	1,	0,	0,	0)
	FIELD(f_rcon_iname,	nam_i_name,	fld_i_name,	1,	0,	0,	0)
END_RELATION

RELATION(nam_ref_constr, rel_refc, ODS_8_0)
	FIELD(f_refc_cname,	nam_con_name,	fld_con_name,	1,	0,	0,	0)
	FIELD(f_refc_uq,	nam_con_uq,	fld_con_name,	1,	0,	0,	0)
	FIELD(f_refc_match,	nam_match,	fld_match,	1,	0,	0,	0)
	FIELD(f_refc_upd_rul,	nam_upd_rule,	fld_rule,	1,	0,	0,	0)
	FIELD(f_refc_del_rul,	nam_del_rule,	fld_rule,	1,	0,	0,	0)
END_RELATION

RELATION(nam_chk_constr, rel_ccon, ODS_8_0)
	FIELD(f_ccon_cname,	nam_con_name,	fld_con_name,	1,	0,	0,	0)
	FIELD(f_ccon_tname,	nam_trg_name,	fld_trg_name,	1,	0,	0,	0)
END_RELATION
 
RELATION(nam_log_files, rel_log, ODS_8_0)
	FIELD(f_log_name,       nam_file_name,  fld_file_name,	1,	0,	0,	0)
	FIELD(f_log_seq,        nam_file_seq,   fld_file_seq,	1,	0,	0,	0)
	FIELD(f_log_length,     nam_file_length,fld_file_length,1,	0,	0,	0)
	FIELD(f_log_partitions,	nam_file_partitions,fld_file_partitions,1,0,	0,	0)
	FIELD(f_log_p_offset,	nam_file_p_offset,fld_file_p_offset,1,	0,	0,	0)
	FIELD(f_log_flags,      nam_file_flags, fld_file_flags,	1,	0,	0,	0)
END_RELATION

RELATION(nam_procedures, rel_procedures, ODS_8_0)
	FIELD(f_prc_name,	nam_prc_name,	fld_prc_name,	1,	0,	0,	0)
	FIELD(f_prc_id,		nam_prc_id,	fld_prc_id,	0,	0,	0,	0)
	FIELD(f_prc_inputs,	nam_prc_inputs,	fld_prc_prm,	0,	0,	0,	0)
	FIELD(f_prc_outputs,	nam_prc_outputs, fld_prc_prm,	0,	0,	0,	0)
	FIELD(f_prc_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_prc_source,	nam_prc_source,	fld_source,	1,	0,	0,	0)
	FIELD(f_prc_blr,	nam_prc_blr,	fld_prc_blr,	1,	0,	0,	0)
	FIELD(f_prc_class,	nam_class,	fld_class,	1,	0,	0,	0)
	FIELD(f_prc_owner,	nam_owner,	fld_user,	1,	0,	0,	0)
	FIELD(f_prc_runtime,	nam_runtime,	fld_runtime,	1,	0,	0,	0)
	FIELD(f_prc_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
END_RELATION

RELATION(nam_proc_parameters, rel_prc_prms, ODS_8_0)
	FIELD(f_prm_name,	nam_prm_name,	fld_prm_name,	1,	0,	0,	0)
	FIELD(f_prm_procedure,	nam_prc_name,	fld_prc_name,	1,	0,	0,	0)
	FIELD(f_prm_number,	nam_prm_number,	fld_prm_number,	1,	0,	0,	0)
	FIELD(f_prm_type,	nam_prm_type,	fld_prm_type,	1,	0,	0,	0)
	FIELD(f_prm_sname,	nam_f_source,	fld_f_name,	1,	0,	0,	0)
	FIELD(f_prm_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_prm_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
END_RELATION

RELATION(nam_charsets,		rel_charsets, ODS_8_0)
	FIELD(f_cs_cs_name,	nam_charset_name,fld_charset_name,1,	0,	0,	0)
	FIELD(f_cs_form_of_use,	nam_form_of_use,fld_gnr_name,	1,	0,	0,	0)
	FIELD(f_cs_num_chars,	nam_num_chars,	fld_num_chars,	1,	0,	0,	0)
	FIELD(f_cs_def_collate,	nam_def_collate,fld_collate_name,1,	0,	0,	0)
	FIELD(f_cs_id,		nam_charset_id,	fld_charset_id,	1,	0,	0,	0)
	FIELD(f_cs_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_cs_desc,	nam_description,fld_description,1,	0,	0,	0)
	FIELD(f_cs_fun_name,	nam_fun_name,	fld_fun_name,	1,	0,	0,	0)
	FIELD(f_cs_bytes_char,	nam_bytes_per_char,fld_f_length,1,	0,	0,	0)
END_RELATION

RELATION(nam_collations,	rel_collations, ODS_8_0)
	FIELD(f_coll_name,	nam_collate_name,fld_collate_name,1,	0,	0,	0)
	FIELD(f_coll_id,	nam_collate_id,	fld_collate_id,	1,	0,	0,	0)
	FIELD(f_coll_cs_id,	nam_charset_id,	fld_charset_id,	1,	0,	0,	0)
	FIELD(f_coll_attr,	nam_collate_attr,fld_gnr_type,	1,	0,	0,	0)
	FIELD(f_coll_sys_flag,	nam_sys_flag,	fld_flag,	0,	0,	0,	0)
	FIELD(f_coll_desc,	nam_description,fld_description,1,	0,	0,	0)
	FIELD(f_coll_fun_name,	nam_fun_name,	fld_fun_name,	1,	0,	0,	0)
END_RELATION

RELATION(nam_exceptions, rel_exceptions, ODS_8_0)
	FIELD(f_xcp_name,	nam_xcp_name,	fld_xcp_name,	1,	0,	0,	0)
	FIELD(f_xcp_number,	nam_xcp_number,	fld_xcp_number,	1,	0,	0,	0)
	FIELD(f_xcp_msg,	nam_msg,	fld_msg,	1,	0,	0,	0)
	FIELD(f_xcp_desc,	nam_description,fld_description, 1,	0,	0,	0)
	FIELD(f_xcp_sys_flag,	nam_sys_flag,	fld_flag,	1,	0,	0,	0)
END_RELATION

RELATION(nam_roles, rel_roles, ODS_9_0)
    FIELD(f_rol_name,   nam_role_name,  fld_user, 1, 0, 0, 0)
    FIELD(f_rol_owner,  nam_owner,      fld_user, 1, 0, 0, 0)
END_RELATION
