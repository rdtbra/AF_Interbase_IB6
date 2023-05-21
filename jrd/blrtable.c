/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		blrtable.c
 *	DESCRIPTION:	blr to internal conversion table generator
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
#include "../jrd/gds.h"  
#include "../jrd/common.h"

#define NODE(type, name, keyword) type,
enum {
#include "../jrd/nod.h"
};

/* NOTE: length == 0 implies the node will not be created
         by jrd/par.c.
	 length == Actual length of the nod_args array.
	 count  == How many of the nods_args array represent nodes
	 internal == Name of the internal representing node.
	 internal2 == Name of the internal representing node in blr_version5.
	 blr      == Name of the BLR to make into this node
         type     == Type of expression this Node produces
	 sub_type == Expected type of parameter Nodes
*/

#if defined(__STDC__) && !defined(apollo)
#define STRINGIZER(s) #s

#define PAIR(internal, blr, length, count, type, sub_type) \
	{ blr, #internal, 0, #length, #count, \
	 #type, #sub_type },

#define PAIR2(internal, blr, length, count, type, sub_type) \
	{ blr, #internal, STRINGIZER(internal##2), #length, #count, \
	 #type, #sub_type },
#else
#define PAIR(internal, blr, length, count, type, sub_type) \
	{ blr, "internal", 0, "length", "count",\
	 "type", "sub_type" },

#define PAIR2(internal, blr, length, count, type, sub_type) \
	{ blr, "internal", "internal2", "length", "count",\
	 "type", "sub_type" },
#endif

#ifdef __STDC__
static void	print (CONST SCHAR **, int, CONST SCHAR *);
#else
static void	print();
#endif

typedef struct {
    UCHAR	blr;
    SCHAR CONST	*internal;
    SCHAR CONST	*internal2;
    SCHAR CONST	*length;
    SCHAR CONST	*count;
    SCHAR CONST	*type;
    SCHAR CONST	*sub_type;
} VERB;

static CONST VERB verbs [] = {
    PAIR (nod_assignment, blr_assignment, e_asgn_length, 2, STATEMENT, VALUE)
    PAIR (nod_erase, blr_erase, e_erase_length, 0, STATEMENT, OTHER)
    PAIR (nod_dcl_variable, blr_dcl_variable, e_dcl_length, 0, STATEMENT, OTHER)
    PAIR (nod_for, blr_fetch, e_for_length, 3, STATEMENT, STATEMENT)
    PAIR (nod_for, blr_for, e_for_length, 3, STATEMENT, STATEMENT)
    PAIR (nod_handler, blr_handler, 1, 1, STATEMENT, OTHER)
    PAIR (nod_if, blr_if, e_if_length, 3, STATEMENT, STATEMENT)
    PAIR (nod_label, blr_label, e_lbl_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_leave, blr_leave, 1, 0, STATEMENT, OTHER)
    PAIR (nod_list, blr_begin, 0, 0, STATEMENT, STATEMENT)
    PAIR (nod_loop, blr_loop, 1, 1, STATEMENT, STATEMENT)
    PAIR (nod_message, blr_message, 0, 0, STATEMENT, OTHER)
    PAIR (nod_modify, blr_modify, 0, 0, STATEMENT, STATEMENT)
    PAIR (nod_receive, blr_receive, e_send_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_select, blr_select, 0, 0, STATEMENT, STATEMENT)
    PAIR (nod_send, blr_send, e_send_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_store, blr_store, e_sto_length, e_sto_length - 1, STATEMENT, STATEMENT)
    PAIR (nod_store, blr_store2, e_sto_length, e_sto_length - 1, STATEMENT, STATEMENT)
    PAIR (nod_post, blr_post, 1, 1, STATEMENT, VALUE)
    PAIR2 (nod_add, blr_add, 2, 2, VALUE, VALUE)
    PAIR (nod_agg_count, blr_agg_count, 1, 0, VALUE, VALUE)
    PAIR (nod_agg_count2, blr_agg_count2, 1, 1, VALUE, VALUE)
    PAIR (nod_agg_count_distinct, blr_agg_count_distinct, 2, 1, VALUE, VALUE)
    PAIR (nod_agg_max, blr_agg_max, 1, 1, VALUE, VALUE)
    PAIR (nod_agg_min, blr_agg_min, 1, 1, VALUE, VALUE)
    PAIR2 (nod_agg_total, blr_agg_total, 1, 1, VALUE, VALUE)
    PAIR2 (nod_agg_total_distinct, blr_agg_total_distinct, 2, 1, VALUE, VALUE)
    PAIR2 (nod_agg_average, blr_agg_average, 1, 1, VALUE, VALUE)
    PAIR2 (nod_agg_average_distinct, blr_agg_average_distinct, 2, 1, VALUE, VALUE)
    PAIR (nod_argument, blr_parameter, e_arg_length, 0, VALUE, VALUE)
    PAIR (nod_argument, blr_parameter2, e_arg_length, 0, VALUE, VALUE)
    PAIR (nod_argument, blr_parameter3, e_arg_length, 0, VALUE, VALUE)
    PAIR (nod_variable, blr_variable, e_var_length, 0, VALUE, VALUE)
    PAIR (nod_user_name, blr_user_name, 1, 0, VALUE, VALUE)

    PAIR2 (nod_average, blr_average, e_stat_length, 2, VALUE, VALUE)
    PAIR (nod_concatenate, blr_concatenate, 2, 2, VALUE, VALUE)
    PAIR (nod_count, blr_count, e_stat_length, 1, VALUE, VALUE)
/* count2
    PAIR (nod_count2, blr_count2, e_stat_length, 2, VALUE, VALUE)
*/
    PAIR (nod_dbkey, blr_dbkey, 1, 0, VALUE, VALUE)
    PAIR2 (nod_divide, blr_divide, 2, 2, VALUE, VALUE)
    PAIR (nod_field, blr_fid, 0, 0, VALUE, VALUE)
    PAIR (nod_field, blr_field, 0, 0, VALUE, VALUE)
    PAIR (nod_from, blr_via, e_stat_length, 3, VALUE, OTHER)
    PAIR (nod_from, blr_from, e_stat_length, 2, VALUE, OTHER)
    PAIR (nod_function, blr_function, 0, 0, VALUE, VALUE)
    PAIR (nod_literal, blr_literal, 0, 0, VALUE, OTHER)
    PAIR (nod_scalar, blr_index, 2, 2, VALUE, VALUE)
    PAIR (nod_max, blr_maximum, e_stat_length, 2, VALUE, VALUE)
    PAIR (nod_min, blr_minimum, e_stat_length, 2, VALUE, VALUE)
    PAIR (nod_null, blr_null, 1, 0, VALUE, VALUE)
    PAIR2 (nod_multiply, blr_multiply, 2, 2, VALUE, VALUE)
    PAIR (nod_negate, blr_negate, 1, 1, VALUE, VALUE)
    PAIR2 (nod_gen_id, blr_gen_id, e_gen_length, 1, VALUE, VALUE)
    PAIR (nod_prot_mask, blr_prot_mask, e_pro_length, 2, VALUE, VALUE)
    PAIR (nod_upcase, blr_upcase, 1, 1, VALUE, VALUE)
    PAIR (nod_lock_state, blr_lock_state, 1, 1, VALUE, VALUE)
    PAIR (nod_substr, blr_substring, 3, 3, VALUE, VALUE)
    PAIR2 (nod_subtract, blr_subtract, 2, 2, VALUE, VALUE)
    PAIR2 (nod_total, blr_total, e_stat_length, 2, VALUE, VALUE)
    PAIR (nod_value_if, blr_value_if, 3, 3, VALUE, OTHER)
    PAIR (nod_eql, blr_eql, 2, 2, BOOL, VALUE)
    PAIR (nod_neq, blr_neq, 2, 2, BOOL, VALUE)
    PAIR (nod_geq, blr_geq, 2, 2, BOOL, VALUE)
    PAIR (nod_gtr, blr_gtr, 2, 2, BOOL, VALUE)
    PAIR (nod_lss, blr_lss, 2, 2, BOOL, VALUE)
    PAIR (nod_leq, blr_leq, 2, 2, BOOL, VALUE)
    PAIR (nod_matches, blr_matching, 2, 2, BOOL, VALUE)
    PAIR (nod_sleuth, blr_matching2, 3, 3, BOOL, VALUE)
    PAIR (nod_like, blr_like, 2, 2, BOOL, VALUE)
    PAIR (nod_like, blr_ansi_like, 3, 3, BOOL, VALUE)
    PAIR (nod_contains, blr_containing, 2, 2, BOOL, VALUE)
    PAIR (nod_missing, blr_missing, 1, 1, BOOL, VALUE)
    PAIR (nod_between, blr_between, 3, 3, BOOL, VALUE)
    PAIR (nod_starts, blr_starting, 2, 2, BOOL, VALUE)
    PAIR (nod_unique, blr_unique, e_any_length, 1, BOOL, OTHER)
    PAIR (nod_any, blr_any, e_any_length, 1, BOOL, TYPE_RSE)
    PAIR (nod_and, blr_and, 2, 2, BOOL, BOOL)
    PAIR (nod_or, blr_or, 2, 2, BOOL, BOOL)
    PAIR (nod_not, blr_not, 1, 1, BOOL, BOOL)
    PAIR (nod_rse, blr_rse, 0, 0, TYPE_RSE, OTHER)

    /* nodes for ODAPI support */

    PAIR (nod_stream, blr_stream, 0, 0, STATEMENT, STATEMENT)
    PAIR (nod_set_index, blr_set_index, e_index_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_seek, blr_seek, e_seek_length, 2, STATEMENT, VALUE)
    PAIR (nod_seek_no_warn, blr_seek_no_warn, e_seek_length, 2, STATEMENT, VALUE)
    PAIR (nod_find, blr_find, e_find_length, 3, STATEMENT, STATEMENT)
    PAIR (nod_get_bookmark, blr_get_bookmark, e_getmark_length, 0, VALUE, VALUE)
    PAIR (nod_set_bookmark, blr_set_bookmark, e_setmark_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_bookmark, blr_bookmark, e_bookmark_length, 1, VALUE, VALUE)
    PAIR (nod_lock_relation, blr_lock_relation, e_lockrel_length, 2, VALUE, VALUE)
    PAIR (nod_lock_record, blr_lock_record, e_lockrec_length, 1, VALUE, VALUE)
    PAIR (nod_release_lock, blr_release_lock, e_rellock_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_release_locks, blr_release_locks, 0, 0, STATEMENT, STATEMENT)
    PAIR (nod_begin_range, blr_begin_range, e_brange_length, 1, VALUE, OTHER)
    PAIR (nod_end_range, blr_end_range, e_erange_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_delete_range, blr_delete_range, e_drange_length, 1, STATEMENT, STATEMENT)

    PAIR (nod_map, blr_map, 0, 0, OTHER, OTHER)
    PAIR (nod_union, blr_union, 0, 0, RELATION, OTHER)
    PAIR (nod_aggregate, blr_aggregate, e_agg_length, 0, RELATION, OTHER)
    PAIR (nod_relation, blr_relation, 0, 0, RELATION, OTHER)
    PAIR (nod_relation, blr_rid, 0, 0, RELATION, OTHER)
    PAIR (nod_rse, blr_rs_stream, 0, 0, RELATION, OTHER)
    PAIR (nod_exec_proc, blr_exec_proc, e_esp_length, 4, STATEMENT, OTHER)
    PAIR (nod_procedure, blr_procedure, e_prc_length, 2, RELATION, OTHER)
    PAIR (nod_procedure, blr_pid, e_prc_length, 2, RELATION, OTHER)
    PAIR (nod_exec_proc, blr_exec_pid, e_esp_length, 4, STATEMENT, OTHER)
    PAIR (nod_block, blr_block, e_blk_length, e_blk_length, STATEMENT, STATEMENT)
    PAIR (nod_error_handler, blr_error_handler, e_err_length, 1, STATEMENT, OTHER)
    PAIR (nod_abort, blr_abort, 1, 0, STATEMENT, OTHER)
    PAIR (nod_cast, blr_cast, e_cast_length, 1, VALUE, VALUE)
    PAIR (nod_rse, blr_singular, 0, 0, TYPE_RSE, OTHER)
    PAIR (nod_start_savepoint, blr_start_savepoint, 1, 0, STATEMENT, OTHER)
    PAIR (nod_end_savepoint, blr_end_savepoint, 1, 0, STATEMENT, OTHER)

    /* more nodes for ODAPI support */

    PAIR (nod_find_dbkey, blr_find_dbkey, e_find_dbkey_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_find_dbkey_version, blr_find_dbkey_version, e_find_dbkey_length, 2, STATEMENT, STATEMENT)
    PAIR (nod_range_relation, blr_range_relation, e_range_relation_length, 1, STATEMENT, STATEMENT)
    PAIR (nod_delete_ranges, blr_delete_ranges, 0, 0, STATEMENT, STATEMENT)

    /* nodes for set plan */

    PAIR (nod_plan, blr_plan, 1, 1, VALUE, VALUE)
    PAIR (nod_merge, blr_merge, 0, 0, VALUE, VALUE)
    PAIR (nod_join, blr_join, 0, 0, VALUE, VALUE)
    PAIR (nod_sequential, blr_sequential, 0, 0, ACCESS_TYPE, OTHER)
    PAIR (nod_navigational, blr_navigational, 1, 1, ACCESS_TYPE, VALUE)
    PAIR (nod_indices, blr_indices, 1, 0, ACCESS_TYPE, VALUE)
    PAIR (nod_retrieve, blr_retrieve, 2, 0, ACCESS_TYPE, VALUE)
    PAIR (nod_relation, blr_relation2, 0, 0, RELATION, OTHER)
    PAIR (nod_relation, blr_rid2, 0, 0, RELATION, OTHER)

    /* yet more nodes for ODAPI */

    PAIR (nod_crack, blr_crack, 2, 0, VALUE, OTHER)
    PAIR (nod_force_crack, blr_force_crack, 2, 0, STATEMENT, OTHER)
    PAIR (nod_reset_stream, blr_reset_stream, e_reset_length, 0, STATEMENT, OTHER)
    PAIR (nod_release_bookmark, blr_release_bookmark, e_relmark_length, 1, STATEMENT, OTHER)

    PAIR2 (nod_set_generator, blr_set_generator, e_gen_length, 1, STATEMENT, VALUE)
    PAIR (nod_ansi_any, blr_ansi_any, e_any_length, 1, BOOL, TYPE_RSE)
    PAIR (nod_exists, blr_exists, e_any_length, 1, BOOL, TYPE_RSE)
    PAIR (nod_cardinality, blr_cardinality, e_card_length, 0, VALUE, OTHER)
    PAIR (nod_rec_version, blr_record_version, 1, 0, VALUE, VALUE)
    PAIR (nod_stall, blr_stall, 1, 0, STATEMENT, STATEMENT)
    PAIR (nod_ansi_all, blr_ansi_all, e_any_length, 1, BOOL, TYPE_RSE)

    /* Improved Date Support */
    PAIR (nod_extract, blr_extract, 
		e_extract_length, e_extract_count, VALUE, VALUE)
    PAIR (nod_current_date, blr_current_date, 
		e_current_date_length, 0, VALUE, OTHER)
    PAIR (nod_current_time, blr_current_time, 
		e_current_time_length, 0, VALUE, OTHER)
    PAIR (nod_current_timestamp, blr_current_timestamp, 
		e_current_timestamp_length, 0, VALUE, OTHER)

    0
};

CONST SCHAR	*table[256], *table2[256], *lengths [256], *counts [256], *types [256], *sub_types [256];

#ifdef __STDC__
int main (
    int		argc,
    char	*argv [])
#else
int main (argc, argv)
    int		argc;
    char	*argv [];
#endif
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Spit out a conversion table.
 *
 **************************************/
CONST   VERB	*verb;
int	max, *stuff, blr;
SCHAR	buffer[100], *s;

for (blr = 0; blr < (sizeof(table)/sizeof(table[0])); blr++)
    {
    table [blr] = NULL;
    table2 [blr] = NULL;
    lengths [blr] = NULL;
    counts [blr] = NULL;
    counts [blr] = NULL;
    types [blr] = NULL;
    sub_types [blr] = NULL;
    }

for (max = 0, verb = verbs; verb->internal; ++verb)
    {
    blr = verb->blr;
    if (table [blr])
	{
	ib_fprintf (ib_stderr, "BLRTABLE: duplicate blr %d\n", blr );
	exit (1);
	}
    table [blr] = verb->internal;
    table2 [blr] = (verb->internal2==0) ? verb->internal : verb->internal2;
    lengths [blr] = verb->length;
    counts [blr] = verb->count;
    types [blr] = verb->type;
    sub_types [blr] = verb->sub_type;
    if (blr > max)
	max = blr;
    }

ib_printf ("static CONST UCHAR blr_table4 [] = {\n");
print (table, max, "(UCHAR) ");

ib_printf ("static CONST UCHAR blr_table [] = {\n");
print (table2, max, "(UCHAR) ");

ib_printf ("static CONST SCHAR length_table [] = {\n");
print (lengths, max, "");

ib_printf ("static CONST SCHAR count_table [] = {\n");
print (counts, max, "");

ib_printf ("static CONST SCHAR type_table [] = {\n");
print (types, max, "");

ib_printf ("static CONST SCHAR sub_type_table [] = {\n");
print (sub_types, max, "");

return 0;
}

#ifdef __STDC__
static void print (
    CONST SCHAR	**table,
    int		max,
    CONST SCHAR	*fudge)
#else
static void print (table, max, fudge)
    SCHAR	**table;
    int		max;
    SCHAR	*fudge;
#endif
{
/**************************************
 *
 *	p r i n t
 *
 **************************************
 *
 * Functional description
 *	Spit out a conversion table.
 *
 **************************************/
int	blr;
SCHAR	buffer[100], *s;

s = buffer;

for (blr = 0; blr <= max; blr++, table++)
    {
    if (*table)
	sprintf (s, "%s%s, ", fudge, *table);
    else
	sprintf (s, " 0, ");
    while (*s)
	s++;
    if (s > buffer + 50)
	{
	ib_printf ("\t%s\n/*%3d*/", buffer, blr+1);
	s = buffer;
	*s = 0;
	}
    }

ib_printf ("\t%s 0};\n", buffer);
}
