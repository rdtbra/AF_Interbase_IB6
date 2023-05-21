/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		blr.h
 *	DESCRIPTION:	BLR constants
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

#ifndef _JRD_BLR_H_
#define _JRD_BLR_H_

/*  WARNING: if you add a new BLR representing a data type, and the value
 *           is greater than the numerically greatest value which now
 *           represents a data type, you must change the define for
 *           DTYPE_BLR_MAX in jrd/align.h, and add the necessary entries
 *           to all the arrays in that file.
 */

#define blr_text		14
#define blr_text2       	15	/* added in 3.2 JPN */
#define blr_short		7
#define blr_long		8
#define blr_quad		9
#define blr_float		10
#define blr_double		27
#define blr_d_float		11
#define blr_timestamp		35
#define blr_varying		37
#define blr_varying2    	38	/* added in 3.2 JPN */
#define blr_blob		261
#define blr_cstring		40     	
#define blr_cstring2    	41	/* added in 3.2 JPN */
#define blr_blob_id     	45	/* added from gds.h */
#define blr_sql_date		12
#define blr_sql_time		13
#define blr_int64               16

/* Historical alias for pre V6 applications */
#define blr_date		blr_timestamp

#define blr_inner		0
#define blr_left		1
#define blr_right		2
#define blr_full		3

#define blr_gds_code		0
#define blr_sql_code		1
#define blr_user_code		2
#define blr_trigger_code 	3
#define blr_default_code 	4

#define blr_version4		4
#define blr_version5		5
#define blr_eoc			76
#define blr_end			255	/* note: defined as -1 in gds.h */

#define blr_assignment		1
#define blr_begin		2
#define blr_dcl_variable  	3	/* added from gds.h */
#define blr_message		4
#define blr_erase		5
#define blr_fetch		6
#define blr_for			7
#define blr_if			8
#define blr_loop		9
#define blr_modify		10
#define blr_handler		11
#define blr_receive		12
#define blr_select		13
#define blr_send		14
#define blr_store		15
#define blr_label		17
#define blr_leave		18
#define blr_store2		19
#define blr_literal		21
#define blr_dbkey		22
#define blr_field		23
#define blr_fid			24
#define blr_parameter		25
#define blr_variable		26
#define blr_average		27
#define blr_count		28
#define blr_maximum		29
#define blr_minimum		30
#define blr_total		31
/* count 2
#define blr_count2		32
*/
#define blr_add			34
#define blr_subtract		35
#define blr_multiply		36
#define blr_divide		37
#define blr_negate		38
#define blr_concatenate		39
#define blr_substring		40
#define blr_parameter2		41
#define blr_from		42
#define blr_via			43
#define blr_parameter2_old	44	/* Confusion */
#define blr_user_name   	44	/* added from gds.h */
#define blr_null		45

#define blr_eql			47
#define blr_neq			48
#define blr_gtr			49
#define blr_geq			50
#define blr_lss			51
#define blr_leq			52
#define blr_containing		53
#define blr_matching		54
#define blr_starting		55
#define blr_between		56
#define blr_or			57
#define blr_and			58
#define blr_not			59
#define blr_any			60
#define blr_missing		61
#define blr_unique		62
#define blr_like		63

#define blr_stream      	65	/* added from gds.h */
#define blr_set_index   	66	/* added from gds.h */

#define blr_rse			67
#define blr_first		68
#define blr_project		69
#define blr_sort		70
#define blr_boolean		71
#define blr_ascending		72
#define blr_descending		73
#define blr_relation		74
#define blr_rid			75
#define blr_union		76
#define blr_map			77
#define blr_group_by		78
#define blr_aggregate		79
#define blr_join_type		80

#define blr_agg_count		83
#define blr_agg_max		84
#define blr_agg_min		85
#define blr_agg_total		86
#define blr_agg_average		87
#define	blr_parameter3		88	/* same as Rdb definition */
#define blr_run_max		89
#define blr_run_min		90
#define blr_run_total		91
#define blr_run_average		92
#define blr_agg_count2		93
#define blr_agg_count_distinct	94
#define blr_agg_total_distinct	95
#define blr_agg_average_distinct 96

#define blr_function		100
#define blr_gen_id		101
#define blr_prot_mask		102
#define blr_upcase		103
#define blr_lock_state		104
#define blr_value_if		105
#define blr_matching2		106
#define blr_index		107
#define blr_ansi_like		108
#define blr_bookmark		109
#define blr_crack		110
#define blr_force_crack		111
#define blr_seek		112
#define blr_find		113
                                 
/* these indicate directions for blr_seek and blr_find */

#define blr_continue		0
#define blr_forward		1
#define blr_backward		2
#define blr_bof_forward		3
#define blr_eof_backward	4

#define blr_lock_relation 	114
#define blr_lock_record		115
#define blr_set_bookmark 	116
#define blr_get_bookmark 	117

#define blr_run_count		118	/* changed from 88 to avoid conflict with blr_parameter3 */
#define blr_rs_stream		119
#define blr_exec_proc		120
#define blr_begin_range 	121
#define blr_end_range 		122
#define blr_delete_range 	123
#define blr_procedure		124
#define blr_pid			125
#define blr_exec_pid		126
#define blr_singular		127
#define blr_abort		128
#define blr_handle_error 	129
#define blr_conditions		130

#define blr_cast		131
#define blr_release_lock	132
#define blr_release_locks	133
#define blr_start_savepoint	134
#define blr_end_savepoint	135
#define blr_find_dbkey		136
#define blr_range_relation	137
#define blr_delete_ranges	138

#define blr_plan		139	/* access plan items */
#define blr_merge		140
#define blr_join		141
#define blr_sequential		142
#define blr_navigational	143
#define blr_indices		144
#define blr_retrieve		145

#define blr_relation2		146
#define blr_rid2		147
#define blr_reset_stream	148
#define blr_release_bookmark	149

#define blr_set_generator       150

#define blr_ansi_any		151   /* required for NULL handling */
#define blr_exists		152   /* required for NULL handling */
#define blr_cardinality		153

#define blr_record_version	154	/* get tid of record */
#define blr_stall		155	/* fake server stall */

#define blr_seek_no_warn	156	
#define blr_find_dbkey_version	157   /* find dbkey with record version */
#define blr_ansi_all		158   /* required for NULL handling */

#define blr_extract		159

/* sub parameters for blr_extract */

#define blr_extract_year	0
#define blr_extract_month	1
#define blr_extract_day		2
#define blr_extract_hour	3
#define blr_extract_minute	4
#define blr_extract_second	5
#define blr_extract_weekday	6
#define blr_extract_yearday	7

#define blr_current_date	160
#define blr_current_timestamp	161
#define blr_current_time	162

/* These verbs were added in 6.0, primarily to support 64-bit integers */

#define blr_add2	          163
#define blr_subtract2	          164
#define blr_multiply2             165
#define blr_divide2	          166
#define blr_agg_total2            167
#define blr_agg_total_distinct2   168
#define blr_agg_average2          169
#define blr_agg_average_distinct2 170
#define blr_average2		  171
#define blr_gen_id2		  172
#define blr_set_generator2        173

#endif /* _JRD_BLR_H_ */
