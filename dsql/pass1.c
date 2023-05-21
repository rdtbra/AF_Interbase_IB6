/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		pass1.c
 *	DESCRIPTION:	First-pass compiler for request trees.
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
#include "../dsql/sym.h"
#include "../jrd/codes.h"
#include "../jrd/thd.h"
#include "../jrd/intl.h"
#include "../jrd/blr.h"
#include "../jrd/gds.h"
#include "../dsql/alld_proto.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/hsh_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../jrd/dsc_proto.h"
#include "../jrd/thd_proto.h"

ASSERT_FILENAME				/* Define things assert() needs */

static BOOLEAN	aggregate_found (REQ, NOD, NOD *);
static BOOLEAN	aggregate_found2 (REQ, NOD, NOD *, BOOLEAN *);
static void	assign_fld_dtype_from_dsc (FLD, DSC *);
static NOD	compose (NOD, NOD, NOD_TYPE);
static NOD	copy_field (NOD, CTX);
static NOD	copy_fields (NOD, CTX);
static void	explode_asterisk (NOD, NOD, LLS *);
static NOD	explode_outputs (REQ, PRC);
static void	field_error (TEXT *, TEXT *);
static PAR	find_dbkey (REQ, NOD);
static PAR	find_record_version (REQ, NOD);
static BOOLEAN	invalid_reference (NOD, NOD);
static void 	mark_ctx_outer_join (NOD);
static BOOLEAN	node_match (NOD, NOD);
static NOD	pass1_alias_list (REQ, NOD);
static CTX	pass1_alias (REQ, STR);
static NOD	pass1_any (REQ, NOD, NOD_TYPE);
static DSQL_REL	pass1_base_table (REQ, DSQL_REL, STR);
static void	pass1_blob (REQ, NOD);
static NOD	pass1_collate (REQ, NOD, STR);
static NOD	pass1_constant (REQ, NOD);
static NOD	pass1_cursor (REQ, NOD, NOD);
static CTX	pass1_cursor_context (REQ, NOD, NOD);
static NOD	pass1_dbkey (REQ, NOD);
static NOD	pass1_delete (REQ, NOD);
static NOD	pass1_field (REQ, NOD, USHORT);
static NOD	pass1_insert (REQ, NOD);
static NOD	pass1_relation (REQ, NOD);
static NOD	pass1_rse (REQ, NOD, NOD);
static NOD	pass1_sel_list ( REQ, NOD);
static NOD	pass1_sort (REQ, NOD, NOD);
static NOD	pass1_udf (REQ, NOD, USHORT);
static void	pass1_udf_args (REQ, NOD, LLS *, USHORT);
static NOD	pass1_union (REQ, NOD, NOD);
static NOD	pass1_update (REQ, NOD);
static NOD	pass1_variable (REQ, NOD);
static NOD	post_map (NOD, CTX);
static void 	remap_streams_to_parent_context (NOD, CTX);
static FLD	resolve_context (REQ, STR, STR, CTX);
static BOOLEAN	set_parameter_type (NOD, NOD, BOOLEAN);
static void	set_parameters_name (NOD, NOD);
static void	set_parameter_name (NOD, NOD, DSQL_REL);

STR	temp_collation_name = NULL;

#define DB_KEY_STRING	"DB_KEY"		/* NTX: pseudo field name */
#define MAX_MEMBER_LIST	1500		/* Maximum members in "IN" list.
					 * For eg. SELECT * FROM T WHERE
					 *         F IN (1, 2, 3, ...)
					 *
					 * Bug 10061, bsriram - 19-Apr-1999
					 */


CTX PASS1_make_context (
    REQ		request,
    NOD		relation_node)
{
/**************************************
 *
 *	P A S S 1 _ m a k e _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Generate a context for a request.
 *
 **************************************/
CTX		context, conflict;
STR		relation_name, string;
DSQL_REL	relation;
PRC		procedure;
FLD		field;
NOD		*input;
LLS		stack;
TEXT		*conflict_name;
STATUS		error_code;
struct nod	desc_node;
TSQL		tdsql;
USHORT		count;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (relation_node, type_nod);

tdsql = GET_THREAD_DATA;

relation = NULL;
procedure = NULL;

/* figure out whether this is a relation or a procedure
   and give an error if it is neither */

if (relation_node->nod_type == nod_rel_proc_name)
    relation_name = (STR) relation_node->nod_arg [e_rpn_name];
else
    relation_name = (STR) relation_node->nod_arg [e_rln_name];

DEV_BLKCHK (relation_name, type_str);

if ((relation_node->nod_type == nod_rel_proc_name) &&
	relation_node->nod_arg [e_rpn_inputs])
    {
    if (!(procedure = METD_get_procedure (request, relation_name)))
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
            gds_arg_gds, gds__dsql_procedure_err,
            gds_arg_gds, gds__random,
            gds_arg_string, relation_name->str_data,
            0);
    }
else
    {
    if (!(relation = METD_get_relation (request, relation_name)) &&
        (relation_node->nod_type == nod_rel_proc_name))
        procedure = METD_get_procedure (request, relation_name);
    if (!relation && !procedure) 
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204, 
	gds_arg_gds, gds__dsql_relation_err, 
	gds_arg_gds, gds__random, 
	gds_arg_string, relation_name->str_data, 
	0);
    }

if (procedure && !procedure->prc_out_count)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -84, 
	gds_arg_gds, gds__dsql_procedure_use_err,
	gds_arg_string, relation_name->str_data, 
	0);

/* Set up context block */

context = (CTX) ALLOCD (type_ctx);
context->ctx_relation = relation;
context->ctx_procedure = procedure;
context->ctx_request = request;
context->ctx_context = request->req_context_number++;
context->ctx_scope_level = request->req_scope_level;

/* find the context alias name, if it exists */

if (relation_node->nod_type == nod_rel_proc_name)
    string = (STR) relation_node->nod_arg [e_rpn_alias];
else
    string = (STR) relation_node->nod_arg [e_rln_alias];

DEV_BLKCHK (string, type_str);

if (string)
    {
    context->ctx_alias = (TEXT*) string->str_data;

    /* check to make sure the context is not already used at this same  
       query level (if there are no subqueries, this checks that the
       alias is not used twice in the request) */

    for (stack = request->req_context; stack; stack = stack->lls_next)
	{
	conflict = (CTX) stack->lls_object;
	if (conflict->ctx_scope_level != context->ctx_scope_level)
	    continue;

	if (conflict->ctx_alias)
	    {
	    conflict_name = conflict->ctx_alias;
	    error_code = gds__alias_conflict_err;
	    /* alias %s conflicts with an alias in the same statement */
	    }
	else if (conflict->ctx_procedure)
	    {
	    conflict_name = conflict->ctx_procedure->prc_name;
	    error_code = gds__procedure_conflict_error;
	    /* alias %s conflicts with a procedure in the same statement */
	    }
	else if (conflict->ctx_relation)
	    {
	    conflict_name = conflict->ctx_relation->rel_name;
	    error_code = gds__relation_conflict_err;
	    /* alias %s conflicts with a relation in the same statement */
	    }
	else
	    continue;

    	if (!strcmp (conflict_name, context->ctx_alias))
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204, 
		gds_arg_gds, error_code, 
		gds_arg_string, conflict_name,
		0);
	}
    }
    
if (procedure)
    {
    if (request->req_scope_level == 1)
	request->req_flags |= REQ_no_batch;	

    if (relation_node->nod_arg [e_rpn_inputs])
	{
    	context->ctx_proc_inputs = PASS1_node (request, 
				relation_node->nod_arg [e_rpn_inputs], 0);
    	count = context->ctx_proc_inputs->nod_count;
	}
    else
	count = 0;

    if (!(request->req_type & REQ_procedure))
	{	
    	if (count != procedure->prc_in_count)
	    ERRD_post (gds__prcmismat, gds_arg_string, relation_name->str_data, 0);

    	if (count)
	    {
	    /* Initialize this stack variable, and make it look like a node */
	    memset ((SCHAR *) &desc_node, 0, sizeof (desc_node));
	    desc_node.nod_header.blk_type = type_nod;
            for (input = context->ctx_proc_inputs->nod_arg,
	 		field = procedure->prc_inputs;
	 		field;
	 		input++, field = field->fld_next)
	    	{
		DEV_BLKCHK (field, type_fld);
		DEV_BLKCHK (*input, type_nod);
            	MAKE_desc_from_field (&desc_node.nod_desc, field);
	    	set_parameter_type (*input, &desc_node, FALSE);
	    	}
	    }
	}
    }

/* push the context onto the request context stack 
   for matching fields against */

LLS_PUSH (context, &request->req_context);

return context;
}

NOD PASS1_node (
    REQ		request,
    NOD		input,
    USHORT	proc_flag)
{
/**************************************
 *
 *	P A S S 1 _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Compile a parsed request into something more interesting.
 *
 **************************************/
NOD	node, temp, *ptr, *end, *ptr2, rse, sub1, sub2, sub3;
LLS	base;
FLD	field;
CTX	agg_context;
CTX	context;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

if (input == NULL)
   return NULL;

/* Dispatch on node type.  Fall thru on easy ones */

switch (input->nod_type)
    {
    case nod_alias:
	node = MAKE_node (input->nod_type, e_alias_count);
	node->nod_arg [e_alias_value] = sub1 = 
			PASS1_node (request, input->nod_arg [e_alias_value], proc_flag);
	node->nod_arg [e_alias_alias] = input->nod_arg [e_alias_alias];
	node->nod_desc = sub1->nod_desc;
	return node;

    case nod_cast:
	node = MAKE_node (input->nod_type, e_cast_count);
	node->nod_arg [e_cast_source] = sub1 = 
			PASS1_node (request, input->nod_arg [e_cast_source], proc_flag);
	node->nod_arg [e_cast_target] = input->nod_arg [e_cast_target];
	field = (FLD) node->nod_arg [e_cast_target];
	DEV_BLKCHK (field, type_fld);
	DDL_resolve_intl_type (request, field, NULL);
	MAKE_desc_from_field (&node->nod_desc, field);
        /* If the source is nullable, so is the target       */
	MAKE_desc (&sub1->nod_desc, sub1);
        if (sub1->nod_desc.dsc_flags & DSC_nullable)
            node->nod_desc.dsc_flags |= DSC_nullable;
	return node;


    case nod_gen_id:
    case nod_gen_id2:
	node = MAKE_node (input->nod_type, e_gen_id_count);
	node->nod_arg [e_gen_id_value] = 
	       PASS1_node (request, input->nod_arg [e_gen_id_value], proc_flag);
	node->nod_arg [e_gen_id_name] = input->nod_arg [e_gen_id_name];
	return node;

    case nod_collate:
	temp_collation_name = (STR) input->nod_arg [e_coll_target];
	sub1 = PASS1_node (request, input->nod_arg [e_coll_source], proc_flag);
	temp_collation_name = NULL;
	node = pass1_collate (request, sub1, (STR) input->nod_arg [e_coll_target]);
	return node;

    case nod_extract:

	/* Figure out the data type of the sub parameter, and make
	   sure the requested type of information can be extracted */

	sub1 = PASS1_node (request, input->nod_arg [e_extract_value], proc_flag);
	MAKE_desc (&sub1->nod_desc, sub1);
	switch (*(SLONG *)input->nod_arg[e_extract_part]->nod_desc.dsc_address)
	    {
	    case blr_extract_year:
	    case blr_extract_month:
	    case blr_extract_day:
	    case blr_extract_weekday:
	    case blr_extract_yearday:
		if (sub1->nod_desc.dsc_dtype != dtype_sql_date &&
		    sub1->nod_desc.dsc_dtype != dtype_timestamp)
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -105, 
			gds_arg_gds, gds__extract_input_mismatch,
			0);
		break;
	    case blr_extract_hour:
	    case blr_extract_minute:
	    case blr_extract_second:
		if (sub1->nod_desc.dsc_dtype != dtype_sql_time &&
		    sub1->nod_desc.dsc_dtype != dtype_timestamp)
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -105, 
			gds_arg_gds, gds__extract_input_mismatch,
			0);
		break;
	    default:
		assert (FALSE);
		break;
	    }
	node = MAKE_node (input->nod_type, e_extract_count);
	node->nod_arg [e_extract_part] = input->nod_arg [e_extract_part];
	node->nod_arg [e_extract_value] = sub1;
        if (sub1->nod_desc.dsc_flags & DSC_nullable)
            node->nod_desc.dsc_flags |= DSC_nullable;
	return node;

    case nod_delete:
    case nod_insert:
    case nod_order:
    case nod_select:
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
	    gds_arg_gds, gds__dsql_command_err, 
	    0);

    case nod_select_expr:
	if (proc_flag)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
		    gds_arg_gds, gds__dsql_subselect_err,
		    0);

	base = request->req_context;
	node = MAKE_node (nod_via, e_via_count);
	node->nod_arg [e_via_rse] = rse =
				PASS1_rse (request, input, NULL);
	node->nod_arg [e_via_value_1] =
				rse->nod_arg[e_rse_items]->nod_arg[0];
	node->nod_arg [e_via_value_2] = MAKE_node (nod_null, (int) 0);

	/* Finish off by cleaning up contexts */

 	while (request->req_context != base)
	    LLS_POP (&request->req_context);
	return node;

    case nod_exists:
    case nod_singular:
	base = request->req_context;
	node = MAKE_node (input->nod_type, 1);
	node->nod_arg[0] = PASS1_rse (request, input->nod_arg[0], NULL);

	/* Finish off by cleaning up contexts */

	while (request->req_context != base)
	    LLS_POP (&request->req_context);
	return node;

    case nod_field_name:
	if (proc_flag)
	    return pass1_variable (request, input);
	else
	    return pass1_field (request, input, 0);

    case nod_array:
	if (proc_flag)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_invalid_array, 
		0);
	else
	    return pass1_field (request, input, 0);

    case nod_variable:
	return node;

    case nod_var_name:
	return pass1_variable (request, input);

    case nod_dbkey:
	return pass1_dbkey (request, input);

    case nod_relation_name:
    case nod_rel_proc_name:              
	return pass1_relation (request, input);

    case nod_constant:
	return pass1_constant (request, input);

    case nod_parameter:
	if (proc_flag)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		0);
	node = MAKE_node (input->nod_type, e_par_count);
	node->nod_count = 0;
	node->nod_arg [e_par_parameter] =
		(NOD) MAKE_parameter (request->req_send, TRUE, TRUE);
	return node;

    case nod_udf:
	return pass1_udf (request, input, proc_flag);

    case nod_eql:
    case nod_neq:
    case nod_gtr:
    case nod_geq:
    case nod_lss:
    case nod_leq:
    case nod_eql_any:
    case nod_neq_any:
    case nod_gtr_any:
    case nod_geq_any:
    case nod_lss_any:
    case nod_leq_any:
    case nod_eql_all:
    case nod_neq_all:
    case nod_gtr_all:
    case nod_geq_all:
    case nod_lss_all:
    case nod_leq_all:
	sub2 = input->nod_arg [1];
	if (sub2->nod_type == nod_list)
	    {
	    USHORT list_item_count=0;

	    node = NULL;
	    for (ptr = sub2->nod_arg, end = ptr + sub2->nod_count; ptr < end && list_item_count < MAX_MEMBER_LIST; list_item_count++, ptr++)
		{
		DEV_BLKCHK (*ptr, type_nod);
		temp = MAKE_node (input->nod_type, 2);
		temp->nod_arg [0] = input->nod_arg [0];
		temp->nod_arg [1] = *ptr;
		node = compose (node, temp, nod_or);
		}
	    if (list_item_count >= MAX_MEMBER_LIST)
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901, 
		    gds_arg_gds, isc_imp_exc,
		    gds_arg_gds, gds__random,
		    gds_arg_string, "too many values (more than 1500) in member list to match against",
		    0);
	    return PASS1_node (request, node, proc_flag);
	    }
	if (sub2->nod_type == nod_select_expr)
	    {
	    if (proc_flag)
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
		    gds_arg_gds, gds__dsql_subselect_err,
		    0);

	    if (sub2->nod_arg [e_sel_singleton])
		{
		base = request->req_context;
		node = MAKE_node (input->nod_type, 2);
		node->nod_arg [0] = PASS1_node (request, input->nod_arg [0], 0);
		node->nod_arg [1] = temp = MAKE_node (nod_via, e_via_count);
		temp->nod_arg [e_via_rse] = rse =
				PASS1_rse (request, sub2, NULL);
		temp->nod_arg [e_via_value_1] =
				rse->nod_arg[e_rse_items]->nod_arg[0];
		temp->nod_arg [e_via_value_2] = MAKE_node (nod_null, (int) 0);

		/* Finish off by cleaning up contexts */

 		while (request->req_context != base)
		    LLS_POP (&request->req_context);
		return node;
		}
	    else
		{
		switch (input->nod_type)
		    {
		    case nod_eql:
		    case nod_neq:
		    case nod_gtr:
		    case nod_geq:
		    case nod_lss:
		    case nod_leq:
			return pass1_any (request, input, nod_any);

		    case nod_eql_any:
		    case nod_neq_any:
		    case nod_gtr_any:
		    case nod_geq_any:
		    case nod_lss_any:
		    case nod_leq_any:
			return pass1_any (request, input, nod_ansi_any);

		    case nod_eql_all:
		    case nod_neq_all:
		    case nod_gtr_all:
		    case nod_geq_all:
		    case nod_lss_all:
		    case nod_leq_all:
			return pass1_any (request, input, nod_ansi_all);
		    }
		}
	    }
	break;

    case nod_agg_count:
    case nod_agg_min:
    case nod_agg_max:
    case nod_agg_average:
    case nod_agg_total:
    case nod_agg_average2:
    case nod_agg_total2:
	if (proc_flag)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		0);
	if (request->req_inhibit_map || !(request->req_in_select_list ||
            request->req_in_having_clause || request->req_in_order_by_clause))
	    /* either nested aggregate, or not part of a select
               list, having clause, or order by clause
	     */
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_agg_ref_err, 
		0);
	++request->req_inhibit_map;
	node = MAKE_node (input->nod_type, input->nod_count);
        node->nod_flags = input->nod_flags;
	if (input->nod_count)
	    node->nod_arg [0] = PASS1_node (request, input->nod_arg [0], proc_flag);
	if (request->req_outer_agg_context)
	    {
	    /* it's an outer reference, post map to the outer context */
	    agg_context = request->req_outer_agg_context;
	    request->req_outer_agg_context = NULL;
	    }
	else
	    /* post map to the current context */
	    agg_context = request->req_context->lls_object;

	temp = post_map (node, agg_context);
	--request->req_inhibit_map;
	return temp;

    /* access plan node types */

    case nod_plan_item:
	node = MAKE_node (input->nod_type, 2);
	node->nod_arg [0] = sub1 = MAKE_node (nod_relation, e_rel_count);
        sub1->nod_arg [e_rel_context] = pass1_alias_list (request, input->nod_arg [0]);
	node->nod_arg [1] = PASS1_node (request, input->nod_arg [1], proc_flag);
	return node;

    case nod_index:
    case nod_index_order:
	node = MAKE_node (input->nod_type, 1);
	node->nod_arg [0] = input->nod_arg [0];
	return node;

    case nod_dom_value:
        node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_desc = input->nod_desc;
	return node;
    
    default:
	break;
    }

/* Node is simply to be rebuilt -- just recurse merrily */

node = MAKE_node (input->nod_type, input->nod_count);
ptr2 = node->nod_arg;

for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++)
    {
    DEV_BLKCHK (*ptr, type_nod);
    *ptr2++ = PASS1_node (request, *ptr, proc_flag);
    DEV_BLKCHK (*(ptr2-1), type_nod);
    }

/* Mark relations as "possibly NULL" if they are in outer joins */ 
switch (node->nod_type)
    {
    case nod_join:
	switch (node->nod_arg [e_join_type]->nod_type)
	    {
	    case nod_join_inner:
		/* Not an outer join - no work required */
		break;
	    case nod_join_left:
		mark_ctx_outer_join (node->nod_arg [e_join_rght_rel]);
		break;
	    case nod_join_right:
		mark_ctx_outer_join (node->nod_arg [e_join_left_rel]);
		break;
	    case nod_join_full:
		mark_ctx_outer_join (node->nod_arg [e_join_rght_rel]);
		mark_ctx_outer_join (node->nod_arg [e_join_left_rel]);
		break;
	    default:
		ASSERT_FAIL;		/* join type expected */
		break;
	    }
	break;	

    default:
	break;
    }

/* Try to match parameters against things of known data type */

sub3 = NULL;
switch (node->nod_type)
    {
    case nod_between:
	sub3 = node->nod_arg [2];
	/* FALLINTO */
    case nod_assign:
    case nod_eql:
    case nod_gtr:
    case nod_geq:
    case nod_leq:
    case nod_lss:
    case nod_neq:
    case nod_eql_any:
    case nod_gtr_any:
    case nod_geq_any:
    case nod_leq_any:
    case nod_lss_any:
    case nod_neq_any:
    case nod_eql_all:
    case nod_gtr_all:
    case nod_geq_all:
    case nod_leq_all:
    case nod_lss_all:
    case nod_neq_all:
	sub1 = node->nod_arg [0];
	sub2 = node->nod_arg [1];

	/* Try to force sub1 to be same type as sub2 eg: ? = FIELD case */
	if (set_parameter_type (sub1, sub2, FALSE))
	    /* null */;
	else
	    /* That didn't work - try to force sub2 same type as sub 1 eg: FIELD = ? case */
	    (void) set_parameter_type (sub2, sub1, FALSE);
	if (sub3)
	    /* X BETWEEN Y AND ? case */
	    (void) set_parameter_type (sub3, sub2, FALSE);
	break;

    case nod_like:
	if (node->nod_count == 3)
	    {
	    sub3 = node->nod_arg[2];
	    }
	/* FALLINTO */
    case nod_containing:
    case nod_starting:
	sub1 = node->nod_arg [0];
	sub2 = node->nod_arg [1];

	/* Try to force sub1 to be same type as sub2 eg: ? LIKE FIELD case */
	if (set_parameter_type (sub1, sub2, TRUE))
	    /* null */;
	else
	    /* That didn't work - try to force sub2 same type as sub 1 eg: FIELD LIKE ? case */
	    (void) set_parameter_type (sub2, sub1, TRUE);
	if (sub3)
	    /* X LIKE Y ESCAPE ? case */
	    (void) set_parameter_type (sub3, sub2, TRUE);
	break;

    default:
	break;
    }

return node;
}

NOD PASS1_rse (
    REQ		request,
    NOD		input,
    NOD		order)
{
/**************************************
 *
 *	P A S S 1 _ r s e
 *
 **************************************
 *
 * Functional description
 *	Compile a record selection expression, 
 *	bumping up the request scope level 
 *	everytime an rse is seen.  The scope
 *	level controls parsing of aliases.
 *
 **************************************/
NOD	node;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);
DEV_BLKCHK (order, type_nod);

request->req_scope_level++;
node = pass1_rse (request, input, order);
request->req_scope_level--;

return node;
}

NOD PASS1_statement (
    REQ		request,
    NOD		input,
    USHORT	proc_flag)
{
/**************************************
 *
 *	P A S S 1 _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Compile a parsed request into something more interesting.
 *
 **************************************/
NOD		node, *ptr, *end, *ptr2, *end2, into_in, into_out, procedure,
		cursor, temp, parameters, variables;
LLS		base;
FLD		field, field2;
STR		name;
struct nod	desc_node;
USHORT		count;
CTX		context;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

#ifdef DEV_BUILD
if (DSQL_debug > 2)
    DSQL_pretty (input, 0);
#endif


base = request->req_context;

/* Dispatch on node type.  Fall thru on easy ones */

switch (input->nod_type)
    {      
    case nod_def_relation:
    case nod_def_index:
    case nod_def_view:
    case nod_def_constraint:
    case nod_def_exception:
    case nod_mod_relation:
    case nod_mod_exception:
    case nod_del_relation:
    case nod_del_index:
    case nod_del_exception:
    case nod_grant:
    case nod_revoke:
    case nod_def_database:
    case nod_mod_database:
    case nod_def_generator:
    case nod_def_role:
    case nod_del_role:
    case nod_del_generator:
    case nod_def_filter:
    case nod_del_filter:
    case nod_def_domain:
    case nod_del_domain:
    case nod_mod_domain:
    case nod_def_udf:
    case nod_del_udf:
    case nod_def_shadow:
    case nod_del_shadow:
    case nod_mod_index:
    case nod_set_statistics:
	request->req_type = REQ_DDL;
	return input;

    case nod_def_trigger:
    case nod_mod_trigger:
    case nod_del_trigger:
	request->req_type = REQ_DDL;
	request->req_flags |= REQ_procedure;
	request->req_flags |= REQ_trigger;
	return input;

    case nod_del_procedure:
	request->req_type = REQ_DDL;
	request->req_flags |= REQ_procedure;
	return input;

    case nod_def_procedure:
    case nod_mod_procedure:
	request->req_type = REQ_DDL;
	request->req_flags |= REQ_procedure;

	/* Insure that variable names do not duplicate parameter names */

	if (variables = input->nod_arg [e_prc_dcls])
	    {
	    for (ptr = variables->nod_arg, end = ptr + variables->nod_count;
		 ptr < end; ptr++)
		{
		field = (FLD) (*ptr)->nod_arg [e_dfl_field];
		DEV_BLKCHK (field, type_fld);
		if (parameters = input->nod_arg [e_prc_inputs])
		    for (ptr2 = parameters->nod_arg, end2 = ptr2 + parameters->nod_count;
			 ptr2 < end2; ptr2++)
			{
			field2 = (FLD) (*ptr2)->nod_arg [e_dfl_field];
			DEV_BLKCHK (field2, type_fld);
			if (!strcmp (field->fld_name, field2->fld_name))
			    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901, 
				gds_arg_gds, gds__dsql_var_conflict,
				gds_arg_string, field->fld_name, 
				0);
			}
		if (parameters = input->nod_arg [e_prc_outputs])
		    for (ptr2 = parameters->nod_arg, end2 = ptr2 + parameters->nod_count;
			 ptr2 < end2; ptr2++)
			{
			field2 = (FLD) (*ptr2)->nod_arg [e_dfl_field];
			DEV_BLKCHK (field2, type_fld);
			if (!strcmp (field->fld_name, field2->fld_name))
			    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901, 
				gds_arg_gds, gds__dsql_var_conflict,
				gds_arg_string, field->fld_name, 
				0);
			}
		}
	    }
	return input;

    case nod_assign:
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [0] = PASS1_node (request, input->nod_arg [0], proc_flag);
	node->nod_arg [1] = PASS1_node (request, input->nod_arg [1], proc_flag);
	break;

    case nod_commit:
	if ((input->nod_arg [e_commit_retain]) && 
	    (input->nod_arg [e_commit_retain]->nod_type == nod_commit_retain))
	    request->req_type = REQ_COMMIT_RETAIN;
	else
	    request->req_type = REQ_COMMIT;
	return input;

    case nod_delete:
	node = pass1_delete (request, input);
	if (request->req_error_handlers)
	    {
	    temp = MAKE_node (nod_list, 3);
	    temp->nod_arg [0] = MAKE_node (nod_start_savepoint, 0);
	    temp->nod_arg [1] = node;
	    temp->nod_arg [2] = MAKE_node (nod_end_savepoint, 0);
	    node = temp;
	    }
	break;

    case nod_exec_procedure:
	if (!proc_flag)
	    {
	    name = (STR) input->nod_arg [e_exe_procedure];
	    DEV_BLKCHK (name, type_str);
	    if (!(request->req_procedure = METD_get_procedure (request, name)))
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
		    gds_arg_gds, gds__dsql_procedure_err, 
		    gds_arg_gds, gds__random, 
		    gds_arg_string, name->str_data, 
		    0);
	    request->req_type = REQ_EXEC_PROCEDURE;
	    }
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [e_exe_procedure] = input->nod_arg [e_exe_procedure];
	node->nod_arg [e_exe_inputs] = PASS1_node (request,
				   input->nod_arg [e_exe_inputs], proc_flag);
	if (temp = input->nod_arg [e_exe_outputs])
	    node->nod_arg [e_exe_outputs] = (temp->nod_type == nod_all) ?
		explode_outputs (request, request->req_procedure) :
		PASS1_node (request, temp, proc_flag);
	if (!proc_flag)
	    {
	    if (node->nod_arg [e_exe_inputs])
		count = node->nod_arg [e_exe_inputs]->nod_count;
	    else 
		count = 0;
    	    if (count != request->req_procedure->prc_in_count)
		ERRD_post (gds__prcmismat, gds_arg_string, name->str_data, 0);
	    if (count)
	        {
		/* Initialize this stack variable, and make it look like a node */
		memset ((SCHAR *)&desc_node, 0, sizeof (desc_node));
		desc_node.nod_header.blk_type = type_nod;
	        for (ptr = node->nod_arg [e_exe_inputs]->nod_arg,
		 	field = request->req_procedure->prc_inputs;
		 	field;
		 	ptr++, field = field->fld_next)
		    {
		    DEV_BLKCHK (field, type_fld);
		    DEV_BLKCHK (*ptr, type_nod);
		    MAKE_desc_from_field (&desc_node.nod_desc, field);
		    set_parameter_type (*ptr, &desc_node, FALSE);
		    }
		}
	    }
	break;

    case nod_for_select:
	node = MAKE_node (input->nod_type, input->nod_count);
        node->nod_flags = input->nod_flags;
	cursor = node->nod_arg [e_flp_cursor] = input->nod_arg [e_flp_cursor];
	if (cursor && (procedure = request->req_ddl_node) &&
		    ((procedure->nod_type == nod_def_procedure) ||
		     (procedure->nod_type == nod_mod_procedure)))
	    {
	    cursor->nod_arg [e_cur_next] = procedure->nod_arg [e_prc_cursors];
	    procedure->nod_arg [e_prc_cursors] = cursor;
	    cursor->nod_arg [e_cur_context] = node;
	    }
	node->nod_arg [e_flp_select] = PASS1_statement (request,
				   input->nod_arg [e_flp_select], proc_flag);
	into_in = input->nod_arg [e_flp_into];
	node->nod_arg [e_flp_into] = into_out =
	    MAKE_node (into_in->nod_type, into_in->nod_count);
	ptr2 = into_out->nod_arg;
	for (ptr = into_in->nod_arg, end = ptr + into_in->nod_count;
	     ptr < end; ptr++)
	    {
	    DEV_BLKCHK (*ptr, type_nod);
	    *ptr2++ = PASS1_node (request, *ptr, proc_flag);
	    DEV_BLKCHK (*(ptr2-1), type_nod);
	    }
	if (input->nod_arg [e_flp_action])
	    node->nod_arg [e_flp_action] = PASS1_statement (request,
				   input->nod_arg [e_flp_action], proc_flag);
	if (cursor &&
	    procedure &&
	    ((procedure->nod_type == nod_def_procedure) ||
	     (procedure->nod_type == nod_mod_procedure)))
	    procedure->nod_arg [e_prc_cursors] = cursor->nod_arg [e_cur_next];

	if (request->req_error_handlers && 
		(input->nod_flags & NOD_SINGLETON_SELECT))
	    {
	    temp = MAKE_node (nod_list, 3);
	    temp->nod_arg [0] = MAKE_node (nod_start_savepoint, 0);
	    temp->nod_arg [1] = node;
	    temp->nod_arg [2] = MAKE_node (nod_end_savepoint, 0);
	    node = temp;
	    }
	break;

    case nod_get_segment:
    case nod_put_segment:
	pass1_blob (request, input);
	return input;

    case nod_if:
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [e_if_condition] = PASS1_node (request,
			 input->nod_arg [e_if_condition], proc_flag);
	node->nod_arg [e_if_true] = PASS1_statement (request,
				   input->nod_arg [e_if_true], proc_flag);
	if (input->nod_arg [e_if_false])
	    node->nod_arg [e_if_false] =
		PASS1_statement (request, input->nod_arg [e_if_false],
				 proc_flag);
	else
	    node->nod_arg [e_if_false] = NULL;
	break;

    case nod_exception_stmt:
	node = input;
	if (request->req_error_handlers)
	    {
	    temp = MAKE_node (nod_list, 3);
	    temp->nod_arg [0] = MAKE_node (nod_start_savepoint, 0);
	    temp->nod_arg [1] = input;
	    temp->nod_arg [2] = MAKE_node (nod_end_savepoint, 0);
	    node = temp;
	    }
	return node;

    case nod_insert:
	node = pass1_insert (request, input);
	if (request->req_error_handlers)
	    {
	    temp = MAKE_node (nod_list, 3);
	    temp->nod_arg [0] = MAKE_node (nod_start_savepoint, 0);
	    temp->nod_arg [1] = node;
	    temp->nod_arg [2] = MAKE_node (nod_end_savepoint, 0);
	    node = temp;
	    }
	break;

    case nod_block:
	if (input->nod_arg [e_blk_errs])
	    request->req_error_handlers++;
	else
	    { 
	    input->nod_count = 1;
	    if (!request->req_error_handlers)
		input->nod_type = nod_list;
	    }
    case nod_list:
	node = MAKE_node (input->nod_type, input->nod_count);
	ptr2 = node->nod_arg;
	for (ptr = input->nod_arg, end = ptr + input->nod_count;
	     ptr < end; ptr++)
	    {
	    DEV_BLKCHK (*ptr, type_nod);
	    if ((*ptr)->nod_type == nod_assign)
		*ptr2++ = PASS1_node (request, *ptr, proc_flag);
	    else
		*ptr2++ = PASS1_statement (request, *ptr, proc_flag);
	    DEV_BLKCHK (*(ptr2-1), type_nod);
	    }
	if (input->nod_type == nod_block && input->nod_arg [e_blk_errs])
	    request->req_error_handlers--;
	return node;

    case nod_on_error:
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [e_err_errs] = input->nod_arg [e_err_errs];
	node->nod_arg [e_err_action] = PASS1_statement (request,
				input->nod_arg [e_err_action], proc_flag);
	return node;

    case nod_post:
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [e_pst_event] = PASS1_node (request,
				input->nod_arg [e_pst_event], proc_flag);
	return node;

    case nod_rollback:
	request->req_type = REQ_ROLLBACK;
	return input;

    case nod_exit:
	if (request->req_flags & REQ_trigger)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__token_err,    /* Token unknown */
		gds_arg_gds, gds__random, 
		gds_arg_string, "EXIT", 0);
	return input;

    case nod_return:
	if (request->req_flags & REQ_trigger)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__token_err,    /* Token unknown */
		gds_arg_gds, gds__random, 
		gds_arg_string, "RETURN", 0);

	input->nod_arg [e_rtn_procedure] = request->req_ddl_node;
	return input;

    case nod_select:
	node = PASS1_rse (request, input->nod_arg [0], input->nod_arg [1]);
	if (input->nod_arg [2])
	    {
	    request->req_type = REQ_SELECT_UPD;
	    request->req_flags |= REQ_no_batch;
	    break;
	    }
	/*
	** If there is a union without ALL or order by or a select distinct 
 	** buffering is OK even if stored procedure occurs in the select
	** list. In these cases all of stored procedure is executed under
	** savepoint for open cursor.
	*/
	if (((temp = node->nod_arg [e_rse_streams]) 
			&& (temp->nod_type == nod_union) 
			&& temp->nod_arg [e_rse_reduced]) ||
                node->nod_arg [e_rse_sort] ||
		node->nod_arg [e_rse_reduced])
            request->req_flags &= ~REQ_no_batch;

	break;

    case nod_trans:
	request->req_type = REQ_START_TRANS;
	return input;

    case nod_update:
	node = pass1_update (request, input);
	if (request->req_error_handlers)
	    {
	    temp = MAKE_node (nod_list, 3);
	    temp->nod_arg [0] = MAKE_node (nod_start_savepoint, 0);
	    temp->nod_arg [1] = node;
	    temp->nod_arg [2] = MAKE_node (nod_end_savepoint, 0);
	    node = temp;
	    }
	break;

    case nod_while:
	node = MAKE_node (input->nod_type, input->nod_count);
	node->nod_arg [e_while_cond] = PASS1_node (request,
				   input->nod_arg [e_while_cond], proc_flag);
	node->nod_arg [e_while_action] = PASS1_statement (request,
				   input->nod_arg [e_while_action], proc_flag);
	node->nod_arg [e_while_number] = (NOD) request->req_loop_number++;
	break;

    case nod_abort:
    case nod_exception:
    case nod_sqlcode:
    case nod_gdscode:
	return input;

    case nod_null:
	return NULL;

    case nod_set_generator:
        node = MAKE_node (input->nod_type, e_gen_id_count);
        node->nod_arg [e_gen_id_value] =
                PASS1_node (request, input->nod_arg [e_gen_id_value],proc_flag);
        node->nod_arg [e_gen_id_name] = input->nod_arg [e_gen_id_name];
        request->req_type = REQ_SET_GENERATOR;
        break;

    case nod_set_generator2:
        node = MAKE_node (input->nod_type, e_gen_id_count);
        node->nod_arg [e_gen_id_value] =
                PASS1_node (request, input->nod_arg [e_gen_id_value],proc_flag);
        node->nod_arg [e_gen_id_name] = input->nod_arg [e_gen_id_name];
        request->req_type = REQ_SET_GENERATOR;
        break;

    case nod_union:
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901, 
	    gds_arg_gds, gds__dsql_command_err, 
	    gds_arg_gds, gds__union_err,    /* union not supported */
	    0);
	break;

    default:
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -901, 
	    gds_arg_gds, gds__dsql_command_err, 
	    gds_arg_gds, gds__dsql_construct_err, /* Unsupported DSQL construct */
	    0);
	break;
    }

/* Finish off by cleaning up contexts */

while (request->req_context != base)
    LLS_POP (&request->req_context);

#ifdef DEV_BUILD
if (DSQL_debug > 1)
    DSQL_pretty (node, 0);
#endif

return node;
} 

static BOOLEAN aggregate_found (
    REQ		request,
    NOD		sub,
    NOD		*proj)
{
/**************************************
 *
 *	a g g r e g a t e _ f o u n d
 *
 **************************************
 *
 * Functional description
 *	Check for an aggregate expression in an 
 *	rse select list.  It could be buried in 
 *	an expression tree.
 *
 **************************************/
BOOLEAN	aggregate, field;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (sub, type_nod);

field = FALSE;

aggregate = aggregate_found2 (request, sub, proj, &field);

if (field && aggregate)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
	gds_arg_gds, gds__field_aggregate_err,  /* field used with aggregate */
	0);

return aggregate;
}

static BOOLEAN aggregate_found2 (
    REQ		request,
    NOD		sub,
    NOD		*proj,
    BOOLEAN	*field)
{
/**************************************
 *
 *	a g g r e g a t e _ f o u n d 2
 *
 **************************************
 *
 * Functional description
 *	Check for an aggregate expression in an 
 *	rse select list.  It could be buried in 
 *	an expression tree.
 *
 *	field is true if a non-aggregate field reference is seen.
 *
 **************************************/
BOOLEAN	aggregate;
NOD	*ptr, *end;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (sub, type_nod);

switch (sub->nod_type)
    {

    /* handle the simple case of a straightforward aggregate */

    case nod_agg_average:
    case nod_agg_average2:
    case nod_agg_total2:
    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_total:
    case nod_agg_count:
	if ((sub->nod_flags & NOD_AGG_DISTINCT) &&   
              (request->req_dbb->dbb_flags & DBB_v3))
	    {
            /* NOTE: leave the v3 behaviour intact, even though it  */
            /* is incorrect in most cases!                          */

    	    /* this aggregate requires a projection to be generated */

	    *proj = PASS1_node (request, sub->nod_arg [0], 0);
	    }
	return TRUE;

    case nod_field:
	*field = TRUE;
	return FALSE;

    case nod_constant:
	return FALSE;

    /* for expressions in which an aggregate might
       be buried, recursively check for one */

    case nod_add:
    case nod_concatenate:
    case nod_divide:
    case nod_multiply:
    case nod_negate:
    case nod_substr:
    case nod_subtract:
    case nod_add2:
    case nod_divide2:
    case nod_multiply2:
    case nod_subtract2:
    case nod_upcase:
    case nod_extract:
    case nod_list:
	aggregate = FALSE;
	for (ptr = sub->nod_arg, end = ptr + sub->nod_count; ptr < end; ptr++)
	    {
	    DEV_BLKCHK (*ptr, type_nod);
	    aggregate |= aggregate_found2 (request, *ptr, proj, field);
	    }
	return aggregate;

    case nod_cast:
    case nod_udf:
    case nod_gen_id:
    case nod_gen_id2:
	if (sub->nod_count == 2)
            return (aggregate_found2 (request, sub->nod_arg[1], proj, field));
	else 
	    return FALSE;

    default:
	return FALSE;
    }
}

static void assign_fld_dtype_from_dsc (
    FLD		field,
    DSC		*nod_desc)
{
/**************************************
 *
 *	a s s i g n _ f l d _ d t y p e _ f r o m _ d s c
 *
 **************************************
 *
 * Functional description
 *	Set a field's descriptor from a DSC
 *	(If FLD is ever redefined this can be removed)
 *
 **************************************/

DEV_BLKCHK (field, type_fld);

field->fld_dtype = nod_desc->dsc_dtype;
field->fld_scale = nod_desc->dsc_scale;
field->fld_sub_type = nod_desc->dsc_sub_type;
field->fld_length = nod_desc->dsc_length;
if (nod_desc->dsc_dtype <= dtype_any_text)
    {
    field->fld_collation_id = DSC_GET_COLLATE (nod_desc);
    field->fld_character_set_id = DSC_GET_CHARSET (nod_desc);
    }
else if (nod_desc->dsc_dtype == dtype_blob)
    field->fld_character_set_id = nod_desc->dsc_scale;
}

static NOD compose (
    NOD		expr1,
    NOD		expr2,
    NOD_TYPE	operator)
{
/**************************************
 *
 *	c o m p o s e
 *
 **************************************
 *
 * Functional description
 *	Compose two booleans.
 *
 **************************************/
NOD	node;

DEV_BLKCHK (expr1, type_nod);
DEV_BLKCHK (expr2, type_nod);

if (!expr1)
    return expr2;

if (!expr2)
    return expr1;

node = MAKE_node (operator, 2);
node->nod_arg [0] = expr1;
node->nod_arg [1] = expr2;

return node;
}

static NOD copy_field (
    NOD		field,
    CTX		context)
{
/**************************************
 *
 *	c o p y _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Copy a field list for a SELECT against an artificial context.
 *
 **************************************/
NOD	temp, actual, *ptr, *end, *ptr2;
MAP	map;
STR	alias;

DEV_BLKCHK (field, type_nod);
DEV_BLKCHK (context, type_ctx);

switch (field->nod_type)
    {
    case nod_map:
	map = (MAP) field->nod_arg[e_map_map];
	DEV_BLKCHK (map, type_map);
	temp = map->map_node;
	return post_map (temp, context);

    case nod_alias:
	actual = field->nod_arg [e_alias_value];
	alias = (STR) field->nod_arg[e_alias_alias];	
	DEV_BLKCHK (alias, type_str);
	temp = MAKE_node (nod_alias, e_alias_count);
	temp->nod_arg [e_alias_value] = copy_field (actual, context);
	temp->nod_arg [e_alias_alias] = (NOD) alias;
	return temp;

    case nod_add:
    case nod_add2:
    case nod_concatenate:
    case nod_divide:
    case nod_divide2:
    case nod_multiply:
    case nod_multiply2:
    case nod_negate:
    case nod_substr:
    case nod_subtract:
    case nod_subtract2:
    case nod_upcase:
    case nod_extract:
    case nod_list:
	temp = MAKE_node (field->nod_type, field->nod_count);
	ptr2 = temp->nod_arg;
	for (ptr = field->nod_arg, end = ptr + field->nod_count; ptr < end; ptr++)
	    *ptr2++ = copy_field (*ptr, context);
	return temp;

    case nod_cast:
    case nod_gen_id:
    case nod_gen_id2:
    case nod_udf:
        temp = MAKE_node (field->nod_type, field->nod_count);
        temp->nod_arg[0] = field->nod_arg[0];
        if (field->nod_count == 2)
            temp->nod_arg[1] = copy_field (field->nod_arg[1], context);
        return temp;

    default:
	return post_map (field, context);
    }
}

static NOD copy_fields (
    NOD		fields,
    CTX		context)
{
/**************************************
 *
 *	c o p y _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Copy a field list for a SELECT against an artificial context.
 *
 **************************************/
NOD	list, temp;
USHORT	i;

DEV_BLKCHK (fields, type_nod);
DEV_BLKCHK (context, type_ctx);

list = MAKE_node (nod_list, fields->nod_count);

for (i = 0; i < fields->nod_count; i++)
    list->nod_arg[i] = copy_field (fields->nod_arg[i], context);

return list;
}

static void explode_asterisk (
    NOD		node,
    NOD		aggregate,
    LLS		*stack)
{
/**************************************
 *
 *	 e x p l o d e _ a s t e r i s k
 *
 **************************************
 *
 * Functional description
 *	Expand an '*' in a field list to the corresponding fields.
 *
 **************************************/
CTX	context;
DSQL_REL	relation;
PRC	procedure;
FLD	field;

DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (aggregate, type_nod);

if (node->nod_type == nod_join)
    {
    explode_asterisk (node->nod_arg [e_join_left_rel], aggregate, stack);
    explode_asterisk (node->nod_arg [e_join_rght_rel], aggregate, stack);
    }
else
    {
    context = (CTX) node->nod_arg [e_rel_context];    
    DEV_BLKCHK (context, type_ctx);
    if (relation = context->ctx_relation)
	for (field = relation->rel_fields; field; field = field->fld_next)
	    {
	    DEV_BLKCHK (field, type_fld);
	    node = MAKE_field (context, field, NULL_PTR);
	    if (aggregate) 
                {
		if (invalid_reference (node, aggregate->nod_arg [e_agg_group]))
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		        gds_arg_gds, gds__field_ref_err,
		    	    /* invalid field reference */
		        0);
                }
 	    LLS_PUSH (MAKE_field (context, field, NULL_PTR), stack);  
	    }
    else if (procedure = context->ctx_procedure)
	for (field = procedure->prc_outputs; field; field = field->fld_next)
	    { DEV_BLKCHK (field, type_fld);
	    node = MAKE_field (context, field, NULL_PTR);
	    if (aggregate) 
                {
		if (invalid_reference (node, aggregate->nod_arg [e_agg_group]))
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		        gds_arg_gds, gds__field_ref_err,
		    	    /* invalid field reference */
		        0);
                }
	    LLS_PUSH (MAKE_field (context, field, NULL_PTR), stack);  
	    }
    }
}

static NOD explode_outputs (
    REQ request,
    PRC procedure)
{
/**************************************
 *
 *	 e x p l o d e _ o u t p u t s
 *
 **************************************
 *
 * Functional description
 *	Generate a parameter list to correspond to procedure outputs.
 *
 **************************************/
NOD	node, *ptr, p_node;
FLD	field;
PAR	parameter;
SSHORT	count;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (procedure, type_prc);

count = procedure->prc_out_count;
node = MAKE_node (nod_list, count);
for (field = procedure->prc_outputs, ptr = node->nod_arg; field;
     field = field->fld_next, ptr++)
    {
    DEV_BLKCHK (field, type_fld);
    DEV_BLKCHK (*ptr, type_nod);
    *ptr = p_node = MAKE_node (nod_parameter, e_par_count);
    p_node->nod_count = 0;
    parameter = MAKE_parameter (request->req_receive, TRUE, TRUE);
    p_node->nod_arg [e_par_parameter] = (NOD) parameter;
    MAKE_desc_from_field (&parameter->par_desc, field);
    parameter->par_name = parameter->par_alias = field->fld_name;
    parameter->par_rel_name = procedure->prc_name;
    parameter->par_owner_name = procedure->prc_owner;
    }

return node;
}

static void field_error (
    TEXT	*qualifier_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	f i e l d _ e r r o r	
 *
 **************************************
 *
 * Functional description
 *	Report a field parsing recognition error.
 *
 **************************************/
TEXT	field_string [64];

if (qualifier_name)
    {
    sprintf (field_string, "%s.%s", qualifier_name, 
		field_name ? field_name : "*");
    field_name = field_string;
    }

if (field_name)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
	gds_arg_gds, gds__dsql_field_err, 
	gds_arg_gds, gds__random, 
	gds_arg_string, field_name,
	0);
else
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
	gds_arg_gds, gds__dsql_field_err, 
	0);
}


static PAR find_dbkey (
    REQ		request,
    NOD		relation_name)
{
/**************************************
 *
 *	f i n d _ d b k e y
 *
 **************************************
 *
 * Functional description
 *	Find dbkey for named relation in request's saved dbkeys.
 *
 **************************************/
CTX	context;
MSG	message;
PAR	parameter, candidate;
DSQL_REL	relation;
STR	rel_name;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (relation_name, type_nod);

message = request->req_receive;
candidate = NULL;
rel_name = (STR) relation_name->nod_arg [e_rln_name];
DEV_BLKCHK (rel_name, type_str);
for (parameter = message->msg_parameters; parameter;
    parameter = parameter->par_next)
    {
    DEV_BLKCHK (parameter, type_par);
    if (context = parameter->par_dbkey_ctx)
	{
	DEV_BLKCHK (context, type_ctx);
	relation = context->ctx_relation;
	if (!strcmp (rel_name->str_data, relation->rel_name))
	    {
	    if (candidate)
		return NULL;
	    else
		candidate = parameter;
	    }
	}
    }
return candidate;
}

static PAR find_record_version (
    REQ		request,
    NOD		relation_name)
{
/**************************************
 *
 *	f i n d _ r e c o r d _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Find record version for relation in request's saved record version
 *
 **************************************/
CTX	context;
MSG	message;
PAR	parameter, candidate;
DSQL_REL	relation;
STR	rel_name;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (relation_name, type_nod);

message = request->req_receive;
candidate = NULL;
rel_name = (STR) relation_name->nod_arg [e_rln_name];
DEV_BLKCHK (rel_name, type_str);
for (parameter = message->msg_parameters; parameter;
    parameter = parameter->par_next)
    {
    DEV_BLKCHK (parameter, type_par);
    if (context = parameter->par_rec_version_ctx)
	{
	DEV_BLKCHK (context, type_ctx);
	relation = context->ctx_relation;
	if (!strcmp (rel_name->str_data, relation->rel_name))
	    {
	    if (candidate)
		return NULL;
	    else
		candidate = parameter;
	    }
	}
    }
return candidate;
}

static BOOLEAN invalid_reference (
    NOD		node,
    NOD		list)
{
/**************************************
 *
 *	i n v a l i d _ r e f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	Validate that an expanded field / context
 *	pair is in a specified list.  Thus is used
 *	in one instance to check that a simple field selected 
 *	through a grouping rse is a grouping field - 
 *	thus a valid field reference.  For the sake of
 *      argument, we'll match qualified to unqualified
 *	reference, but qualified reference must match
 *	completely. 
 *
 *	A list element containing a simple CAST for collation purposes
 *	is allowed.
 *
 **************************************/
NOD	*ptr, *end;	
BOOLEAN	invalid;

DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (list, type_nod);

if (node == NULL)
    {
    return FALSE;
    }

invalid = FALSE;

if (node->nod_type == nod_field)
    {
    FLD	field;
    CTX	context;
    NOD	reference;

    if (!list)
        return TRUE;
    field = (FLD) node->nod_arg[e_fld_field];
    context = (CTX) node->nod_arg[e_fld_context];

    DEV_BLKCHK (field, type_fld);
    DEV_BLKCHK (context, type_ctx);

    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	{
	DEV_BLKCHK (*ptr, type_nod);
	reference = *ptr;
	if ((*ptr)->nod_type == nod_cast)
	    {
	    reference = (*ptr)->nod_arg[e_cast_source];
	    }
	DEV_BLKCHK (reference, type_nod);
	if (reference->nod_type == nod_field &&
	    field == (FLD) reference->nod_arg[e_fld_field] &&
	    context == (CTX) reference->nod_arg[e_fld_context])
	    return FALSE;
	}
    return TRUE; 
    }
else
    {
    if ((node->nod_type == nod_gen_id) || 
	(node->nod_type == nod_udf)    || 
	(node->nod_type == nod_cast))
	{
	if (node->nod_count == 2)
            invalid |= invalid_reference (node->nod_arg [1], list);
	}
#ifdef	CHECK_HAVING
/*
******************************************************

 taken out to fix a more serious bug

******************************************************
*/
    else if (node->nod_type == nod_map)
	{
	MAP	map;
	map = (MAP) node->nod_arg [e_map_map];
	DEV_BLKCHK (map, type_map);
	invalid |= invalid_reference (map->map_node, list);
	}
#endif
    else
	switch (node->nod_type)
	    {
	    default:
		ASSERT_FAIL;
		/* FALLINTO */
	    case nod_add:
	    case nod_concatenate:
	    case nod_divide:
	    case nod_multiply:
	    case nod_negate:
	    case nod_substr:
	    case nod_subtract:
	    case nod_upcase:
	    case nod_extract:
	    case nod_add2:
	    case nod_divide2:
	    case nod_multiply2:
	    case nod_subtract2:
	    case nod_eql:
	    case nod_neq:
	    case nod_gtr:
	    case nod_geq:
	    case nod_leq:
	    case nod_lss:
	    case nod_eql_any:
	    case nod_neq_any:
	    case nod_gtr_any:
	    case nod_geq_any:
	    case nod_leq_any:
	    case nod_lss_any:
	    case nod_eql_all:
	    case nod_neq_all:
	    case nod_gtr_all:
	    case nod_geq_all:
	    case nod_leq_all:
	    case nod_lss_all:
	    case nod_between:
	    case nod_like:
	    case nod_missing:
	    case nod_and:
	    case nod_or:
	    case nod_any:
	    case nod_ansi_any:
	    case nod_ansi_all:
	    case nod_not:
	    case nod_unique:
	    case nod_containing:
	    case nod_starting:
	    case nod_rse:
	    case nod_list:
	    case nod_via:
		for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
		    invalid |= invalid_reference (*ptr, list);
		break;

	    case nod_alias:
		invalid |= invalid_reference (node->nod_arg [e_alias_value], list);
		break;

		/* An embedded aggregate, even of an expression, is OK */
	    case nod_agg_average:
	    case nod_agg_count:
	    case nod_agg_max:
	    case nod_agg_min:
	    case nod_agg_total:
	    case nod_agg_average2:
	    case nod_agg_total2:
		/* Fall into ... */

		/* misc other stuff */
	    case nod_aggregate:
	    case nod_map:
	    case nod_relation:
	    case nod_variable:
	    case nod_constant:
	    case nod_null:
	    case nod_current_date:
	    case nod_current_time:
	    case nod_current_timestamp:
	    case nod_user_name:
		return FALSE;
	    } 
    }

return invalid;
}

static void mark_ctx_outer_join (
    NOD		node)
{
/**************************************
 *
 *	m a r k _ c t x _ o u t e r _ j o i n 
 *
 **************************************
 *
 * Functional description
 *	Mark the context blocks of relations in an RSE as
 *	participating in an Outer Join of some sort.
 *	This is important when we are deciding whether
 *	a particular field reference can be NULL or not.
 *	If the field is declared NOT NULL, it normally cannot
 *	be NULL - however, if the base relation reference is 
 *	within the "outside" part of an outer join rse,
 *	it CAN be null.
 *
 *	Our input RSE can be either a relation (table, view, or proc) 
 *	reference or a JOIN expression.
 *	
 **************************************/
CTX	context;

DEV_BLKCHK (node, type_nod);

switch (node->nod_type)
    {
    case nod_relation:
	context = (CTX) node->nod_arg [e_rel_context];
	DEV_BLKCHK (context, type_ctx);
	assert (context);
	context->ctx_flags |= CTX_outer_join;
	break;

    case nod_join:
	mark_ctx_outer_join (node->nod_arg [e_join_left_rel]);
	mark_ctx_outer_join (node->nod_arg [e_join_rght_rel]);
	break;

    default:
	ASSERT_FAIL;		        /* only join & relation expected */
	break;
    }
}

static BOOLEAN node_match (
    NOD		node1,
    NOD		node2)
{
/**************************************
 *
 *	n o d e _ m a t c h
 *
 **************************************
 *
 * Functional description
 *	Compare two nodes for equality of value.
 *
 **************************************/
NOD	*ptr1, *ptr2, *end;
MAP	map1, map2;
USHORT	l;
UCHAR	*p1, *p2;

DEV_BLKCHK (node1, type_nod);
DEV_BLKCHK (node2, type_nod);

if ((!node1) && (!node2))
    return TRUE;

if ((!node1) || (!node2) || (node1->nod_type != node2->nod_type)
    || (node1->nod_count != node2->nod_count))
    return FALSE;

/* This is to get rid of assertion failures when trying
   to node_match nod_aggregate's children. This was happening because not
   all of the chilren are of type "struct nod". Pointer to the first child
   (argument) is actually a pointer to context structure.
   To compare two nodes of type nod_aggregate we need first to see if they
   both refer to same context structure. If they do not they are different
   nodes, if they point to the same context they are the same (because
   nod_aggregate is created for an rse that have aggregate expression, 
   group by or having clause and each rse has its own context). But just in
   case we compare two other subtrees.
*/

if (node1->nod_type == nod_aggregate)
   if (node1->nod_arg[e_agg_context] != node2->nod_arg[e_agg_context])
	return FALSE;
   else
	return (node_match (node1->nod_arg[e_agg_group],
			    node2->nod_arg[e_agg_group]) &&
		node_match (node1->nod_arg[e_agg_rse],
			    node2->nod_arg[e_agg_rse]));

if (node1->nod_type == nod_alias)
    return node_match (node1->nod_arg [e_alias_value], node2);

if (node2->nod_type == nod_alias)
    return node_match (node1, node2->nod_arg [e_alias_value]);

if (node1->nod_type == nod_field)
    {
    if (node1->nod_arg [e_fld_field] != node2->nod_arg [e_fld_field] ||
	node1->nod_arg [e_fld_context] != node2->nod_arg [e_fld_context])
	return FALSE;
    if (node1->nod_arg [e_fld_indices] || node2->nod_arg [e_fld_indices])
	return node_match (node1->nod_arg [e_fld_indices],
			   node2->nod_arg [e_fld_indices]);
    return TRUE;
    }

if (node1->nod_type == nod_constant)
    {
    if (node1->nod_desc.dsc_dtype != node2->nod_desc.dsc_dtype ||
	node1->nod_desc.dsc_length != node2->nod_desc.dsc_length ||
	node1->nod_desc.dsc_scale != node2->nod_desc.dsc_scale)
	return FALSE;
    p1 = node1->nod_desc.dsc_address;
    p2 = node2->nod_desc.dsc_address;
    for (l = node1->nod_desc.dsc_length; l > 0; l--)
	if (*p1++ != *p2++)
	    return FALSE;
    return TRUE;
    }

if (node1->nod_type == nod_map)
    {
    map1 = (MAP) node1->nod_arg [e_map_map];
    map2 = (MAP) node2->nod_arg [e_map_map];
    DEV_BLKCHK (map1, type_map);
    DEV_BLKCHK (map2, type_map);
    return node_match (map1->map_node, map2->map_node);
    }

if ((node1->nod_type == nod_gen_id) ||
    (node1->nod_type == nod_udf)    ||
    (node1->nod_type == nod_cast))
    {
    if (node1->nod_arg [0] != node2->nod_arg [0])
        return FALSE;
    if (node1->nod_count == 2)
        return node_match (node1->nod_arg [1], node2->nod_arg [1]);
    else
	return TRUE;
    }

if ((node1->nod_type == nod_agg_count) || (node1->nod_type == nod_agg_total)
    || (node1->nod_type == nod_agg_total2)
    || (node1->nod_type == nod_agg_average2)
    || (node1->nod_type == nod_agg_average))
    {
    if ((node1->nod_flags & NOD_AGG_DISTINCT) != (node2->nod_flags & NOD_AGG_DISTINCT))
        return FALSE;
    }

if (node1->nod_type == nod_variable)
    {
    VAR var1, var2;
    if (node1->nod_type != node2->nod_type)
	return FALSE;
    var1 = node1->nod_arg [e_var_variable];
    var2 = node2->nod_arg [e_var_variable];
    DEV_BLKCHK (var1, type_var);
    DEV_BLKCHK (var2, type_var);
    if ( (strcmp (var1->var_name, var2->var_name)) ||
	 (var1->var_field != var2->var_field)      ||
	 (var1->var_variable_number != var2->var_variable_number) ||
	 (var1->var_msg_item != var2->var_msg_item) ||
	 (var1->var_msg_number != var2->var_msg_number) )
	 return FALSE;
    return TRUE;
    }

ptr1 = node1->nod_arg;
ptr2 = node2->nod_arg;

for (end = ptr1 + node1->nod_count; ptr1 < end; ptr1++, ptr2++)
    if (!node_match (*ptr1, *ptr2))
	return FALSE;

return TRUE;
}

static NOD pass1_any (
    REQ		request,
    NOD		input,
    NOD_TYPE	ntype)
{
/**************************************
 *
 *	p a s s 1 _ a n y
 *
 **************************************
 *
 * Functional description
 *	Compile a parsed request into something more interesting.
 *
 **************************************/
NOD	node, temp, rse, select;
LLS	base;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

select = input->nod_arg [1];
base = request->req_context;
temp = MAKE_node (input->nod_type, 2);
temp->nod_arg [0] = PASS1_node (request, input->nod_arg [0], 0);

node = MAKE_node (ntype, 1);
node->nod_arg [0] = rse = PASS1_rse (request, select, NULL);

/* adjust the scope level back to the sub-rse, so that 
   the fields in the select list will be properly recognized */

request->req_scope_level++;
request->req_in_select_list++;
temp->nod_arg [1] = PASS1_node (request, select->nod_arg [e_sel_list]->nod_arg[0], 0);
request->req_in_select_list--;
request->req_scope_level--;

rse->nod_arg [e_rse_boolean] =
	compose (rse->nod_arg [e_rse_boolean], temp, nod_and);

while (request->req_context != base)
    LLS_POP (&request->req_context);

return node;
}

static void pass1_blob (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Process a blob get or put segment.
 *
 **************************************/
NOD	field, list;
BLB	blob;
PAR	parameter;
TSQL	tdsql;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

tdsql = GET_THREAD_DATA;


(void) PASS1_make_context (request, input->nod_arg [e_blb_relation]);
field = pass1_field (request, input->nod_arg [e_blb_field], 0);
if (field->nod_desc.dsc_dtype != dtype_blob)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
	gds_arg_gds, gds__dsql_blob_err, 
	0);

request->req_type =
    (input->nod_type == nod_get_segment) ? REQ_GET_SEGMENT : REQ_PUT_SEGMENT;
request->req_blob = blob = (BLB) ALLOCDV (type_blb, 0);
blob->blb_field = field;
blob->blb_open_in_msg = request->req_send;
blob->blb_open_out_msg = (MSG) ALLOCD (type_msg);
blob->blb_segment_msg = request->req_receive;

/* Create a parameter for the blob segment */

blob->blb_segment = parameter =
    MAKE_parameter (blob->blb_segment_msg, TRUE, TRUE);
parameter->par_desc.dsc_dtype = dtype_text;
parameter->par_desc.dsc_ttype = ttype_binary;
parameter->par_desc.dsc_length =
    ((FLD) field->nod_arg [e_fld_field])->fld_seg_length;
DEV_BLKCHK (field->nod_arg [e_fld_field], type_fld);

/* The Null indicator is used to pass back the segment length,
 * set DSC_nullable so that the SQL_type is set to SQL_TEXT+1 instead
 * of SQL_TEXT.
 */
if (input->nod_type == nod_get_segment)
    parameter->par_desc.dsc_flags |= DSC_nullable;

/* Create a parameter for the blob id */

blob->blb_blob_id = parameter = MAKE_parameter (
    (input->nod_type == nod_get_segment) ?
	blob->blb_open_in_msg : blob->blb_open_out_msg,
    TRUE, TRUE);
parameter->par_desc = field->nod_desc;
parameter->par_desc.dsc_dtype = dtype_quad;
parameter->par_desc.dsc_scale = 0;

if (list = input->nod_arg [e_blb_filter])
    {
    if (list->nod_arg [0])
	blob->blb_from = PASS1_node (request, list->nod_arg [0], FALSE);
    if (list->nod_arg [1])
	blob->blb_to = PASS1_node (request, list->nod_arg [1], FALSE);
    }
if (!blob->blb_from)
    blob->blb_from = MAKE_constant ((STR) 0, 1);
if (!blob->blb_to)
    blob->blb_to = MAKE_constant ((STR) 0, 1);

for (parameter = blob->blb_open_in_msg->msg_parameters; parameter; parameter = parameter->par_next)
    if (parameter->par_index > (input->nod_type == nod_get_segment) ? 1 : 0)
	{
	parameter->par_desc.dsc_dtype = dtype_short;
	parameter->par_desc.dsc_scale = 0;
	parameter->par_desc.dsc_length = sizeof (SSHORT);
	}
}

static NOD pass1_collate (
    REQ		request,
    NOD		sub1,
    STR		collation)
{
/**************************************
 *
 *	p a s s 1 _  c o l l a t e
 *
 **************************************
 *
 * Functional description
 *	Turn a collate clause into a cast clause.
 *	If the source is not already text, report an error.
 *	(SQL 92: Section 13.1, pg 308, item 11)
 *
 **************************************/
NOD	node;
FLD	field;
TSQL	tdsql;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (sub1, type_nod);
DEV_BLKCHK (collation, type_str);

tdsql = GET_THREAD_DATA;


node = MAKE_node (nod_cast, e_cast_count);
field = (FLD) ALLOCDV (type_fld, 1);
field->fld_name[0] = 0;
node->nod_arg [e_cast_target] = (NOD) field;
node->nod_arg [e_cast_source] = sub1;
MAKE_desc (&sub1->nod_desc, sub1);
if (sub1->nod_desc.dsc_dtype <= dtype_any_text)
    {
    assign_fld_dtype_from_dsc (field, &sub1->nod_desc);
    field->fld_character_length = 0;
    if (sub1->nod_desc.dsc_dtype == dtype_varying)
	field->fld_length += sizeof (USHORT);
    }
else
    {
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204,
    	    gds_arg_gds, gds__dsql_datatype_err, 
	    gds_arg_gds, gds__collation_requires_text,
    	    0);
    }
DDL_resolve_intl_type (request, field, collation);
MAKE_desc_from_field (&node->nod_desc, field);
return node;
}

static NOD pass1_constant (
    REQ		request,
    NOD		constant)
{
/**************************************
 *
 *	p a s s 1 _ c o n s t a n t
 *
 **************************************
 *
 * Functional description
 *	Turn an international string reference into internal
 *	subtype ID.
 *
 **************************************/
STR	string;
INTLSYM	resolved;
INTLSYM	resolved_collation = NULL;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (constant, type_nod);

if (constant->nod_desc.dsc_dtype > dtype_any_text)
    return constant;
string = (STR) constant->nod_arg[0];
DEV_BLKCHK (string, type_str);
if (!string || !string->str_charset)
    return constant;
resolved = METD_get_charset (request, strlen (string->str_charset), string->str_charset);
if (!resolved)
    /* character set name is not defined */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -504, gds_arg_gds, 
	       gds__charset_not_found, gds_arg_string, string->str_charset, 0);

if (temp_collation_name)
    {
    resolved_collation = METD_get_collation (request, temp_collation_name);
    if (!resolved_collation)
	/* 
	** Specified collation not found 
	*/
        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -204, gds_arg_gds, 
		   gds__dsql_datatype_err, gds_arg_gds, 
		   gds__collation_not_found, gds_arg_string, 
		   temp_collation_name->str_data, 0);

    resolved = resolved_collation;
    }

INTL_ASSIGN_TTYPE (&constant->nod_desc, resolved->intlsym_ttype);

return constant;
}

static NOD pass1_cursor (
    REQ		request,
    NOD		cursor,
    NOD		relation_name)
{
/**************************************
 *
 *	p a s s 1 _ c u r s o r
 *
 **************************************
 *
 * Functional description
 *	Turn a cursor reference into a record selection expression.
 *
 **************************************/
SYM	symbol;
NOD	rse, node, temp, relation_node;
REQ	parent;
STR	string;
PAR	parameter, source, rv_source;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (cursor, type_nod);
DEV_BLKCHK (relation_name, type_nod);

/* Lookup parent request */

string = (STR) cursor->nod_arg [e_cur_name];
DEV_BLKCHK (string, type_str);

symbol = HSHD_lookup (request->req_dbb, string->str_data, 
			string->str_length, SYM_cursor, 0);

if (!symbol)
    /* cursor is not defined */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -504, 
	gds_arg_gds, gds__dsql_cursor_err, 
	0);

parent = (REQ) symbol->sym_object;

/* Verify that the cursor is appropriate and updatable */

rv_source = find_record_version (parent, relation_name);

if (parent->req_type != REQ_SELECT_UPD ||
    !(source = find_dbkey (parent, relation_name)) ||
    (!rv_source && !(request->req_dbb->dbb_flags & DBB_v3)))
    /* cursor is not updatable */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -510, 
	gds_arg_gds, gds__dsql_cursor_update_err, 
	0);

request->req_parent = parent;
request->req_parent_dbkey = source;
request->req_parent_rec_version = rv_source;
request->req_sibling = parent->req_offspring;
parent->req_offspring = request;

/* Build record selection expression */

rse = MAKE_node (nod_rse, e_rse_count);
rse->nod_arg [e_rse_streams] = temp = MAKE_node (nod_list, 1);
temp->nod_arg [0] = relation_node = pass1_relation (request, relation_name);
rse->nod_arg [e_rse_boolean] = node = MAKE_node (nod_eql, 2);
node->nod_arg [0] = temp = MAKE_node (nod_dbkey, 1);
temp->nod_arg [0] = relation_node;

node->nod_arg [1] = temp = MAKE_node (nod_parameter, e_par_count);
temp->nod_count = 0;
parameter = request->req_dbkey =
    MAKE_parameter (request->req_send, FALSE, FALSE);
temp->nod_arg [e_par_parameter] = (NOD) parameter;
parameter->par_desc = source->par_desc;

/* record version will be set only for V4 - for the parent select cursor */
if (rv_source)
    {
    node = MAKE_node (nod_eql, 2);
    node->nod_arg [0] = temp = MAKE_node (nod_rec_version, 1);
    temp->nod_arg [0] = relation_node;
    node->nod_arg [1] = temp = MAKE_node (nod_parameter, e_par_count);
    temp->nod_count = 0;
    parameter = request->req_rec_version =
	MAKE_parameter (request->req_send, FALSE, FALSE);
    temp->nod_arg [e_par_parameter] = (NOD) parameter;
    parameter->par_desc = rv_source->par_desc;

    rse->nod_arg [e_rse_boolean] = compose (rse->nod_arg [e_rse_boolean], node, nod_and);
    }

return rse;
}

static CTX pass1_cursor_context (
    REQ		request,
    NOD		cursor,
    NOD		relation_name)
{
/**************************************
 *
 *	p a s s 1 _ c u r s o r _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Turn a cursor reference into a record selection expression.
 *
 **************************************/
NOD	node, temp, *ptr, *end, procedure, r_node;
CTX	context, candidate;
STR	string, cname, rname;
DSQL_REL	relation;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (cursor, type_nod);
DEV_BLKCHK (relation_name, type_nod);

string = (STR) cursor->nod_arg [e_cur_name];
DEV_BLKCHK (string, type_str);

procedure = request->req_ddl_node;
context = NULL;
for (node = procedure->nod_arg [e_prc_cursors]; node;
     node = node->nod_arg [e_cur_next])
    {
    DEV_BLKCHK (node, type_nod);
    cname = (STR) node->nod_arg [e_cur_name];
    DEV_BLKCHK (cname, type_str);
    if (!strcmp (string->str_data, cname->str_data))
	{
	temp = node->nod_arg [e_cur_context];
	temp = temp->nod_arg [e_flp_select];
	temp = temp->nod_arg [e_rse_streams];
	for (ptr = temp->nod_arg, end = ptr + temp->nod_count;
	     ptr < end; ptr++)
	    {
	    DEV_BLKCHK (*ptr, type_nod);
	    r_node = *ptr;
	    if (r_node->nod_type == nod_relation)
		{
		candidate = (CTX) r_node->nod_arg [e_rel_context];
		DEV_BLKCHK (candidate, type_ctx);
		relation = candidate->ctx_relation;
		rname = (STR) relation_name->nod_arg [e_rln_name];
		DEV_BLKCHK (rname, type_str);
		if (!strcmp (rname->str_data, relation->rel_name))
		    {
		    if (context)
			ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -504,
			    gds_arg_gds, gds__dsql_cursor_err, 
			    0);
		    else
			context = candidate;
		    }
		}
	    }
	if (context)
	    return context;
	else
	    break;
	}
    }

ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -504, 
	gds_arg_gds, gds__dsql_cursor_err, 
	0);
return (NULL);	/* Added to remove compiler warnings */ 
}

static NOD pass1_dbkey (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ d b k e y
 *
 **************************************
 *
 * Functional description
 *	Resolve a dbkey to an available context.
 *
 **************************************/
NOD	node, rel_node;
STR	qualifier;
LLS	stack;
CTX	context;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

if (!(qualifier = (STR) input->nod_arg [0]))
    {
    DEV_BLKCHK (qualifier, type_str);
    /* No qualifier, if only one context then use, else error */

    if ((stack = request->req_context) && !stack->lls_next)
	{
	context = (CTX) stack->lls_object;
	DEV_BLKCHK (context, type_ctx);
	node = MAKE_node (nod_dbkey, 1);
	rel_node = MAKE_node (nod_relation, e_rel_count);
	rel_node->nod_arg[0] = (NOD) context;
	node->nod_arg[0] = rel_node;
	return node;
	}
    }
else
    {
    for (stack = request->req_context; stack; stack = stack->lls_next)
	{
	context = (CTX) stack->lls_object;
	DEV_BLKCHK (context, type_ctx);
	if (strcmp (qualifier->str_data, context->ctx_relation->rel_name) && ( 
	    !(context->ctx_alias) || 
		    strcmp (qualifier->str_data, context->ctx_alias)))
	   continue;
	node = MAKE_node (nod_dbkey, 1);
	rel_node = MAKE_node (nod_relation, e_rel_count);
	rel_node->nod_arg[0] = (NOD) context;
	node->nod_arg[0] = rel_node;
	return node;
	}
     }

/* field unresolved */

field_error (qualifier ? qualifier->str_data : (UCHAR *) NULL_PTR, DB_KEY_STRING); 

return NULL;
}

static NOD pass1_delete (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ d e l e t e
 *
 **************************************
 *
 * Functional description
 *	Process INSERT statement.
 *
 **************************************/
NOD	rse, node, temp, cursor, relation;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

cursor = input->nod_arg [e_del_cursor];
relation = input->nod_arg [e_del_relation];
if (cursor && (request->req_flags & REQ_procedure))
    {
    node = MAKE_node (nod_erase_current, e_erc_count);
    node->nod_arg [e_erc_context] = (NOD) pass1_cursor_context (request, cursor, relation);
    return node;
    }

request->req_type = (cursor) ? REQ_DELETE_CURSOR : REQ_DELETE;
node = MAKE_node (nod_erase, e_era_count);

/* Generate record selection expression */

if (cursor)
    rse = pass1_cursor (request, cursor, relation);
else
    {
    rse = MAKE_node (nod_rse, e_rse_count);
    rse->nod_arg [e_rse_streams] = temp = MAKE_node (nod_list, 1);
    temp->nod_arg [0] = PASS1_node (request, relation, 0);
    if (temp = input->nod_arg [e_del_boolean])
	rse->nod_arg [e_rse_boolean] = PASS1_node (request, temp, 0);
    }

node->nod_arg [e_era_rse] = rse;
temp = rse->nod_arg [e_rse_streams];
node->nod_arg [e_era_relation] = temp->nod_arg [0];

LLS_POP (&request->req_context);
return node;
}

static NOD pass1_field (
    REQ		request,
    NOD		input,
    USHORT	list)
{
/**************************************
 *
 *	p a s s 1 _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Resolve a field name to an available context.
 *	If list is TRUE, then this function can detect and
 *	return a relation node if there is no name.   This
 *	is used for cases of "SELECT <table_name>.* ...".
 *
 **************************************/
NOD	node, indices;
STR	name, qualifier;
FLD	field;
LLS	stack;
CTX	context;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

/* handle an array element */

if (input->nod_type == nod_array)
    {
    indices = input->nod_arg [e_ary_indices];
    input = input->nod_arg [e_ary_array];
    }
else indices = NULL;

if (input->nod_count == 1)
    {
    name = (STR) input->nod_arg [0];
    qualifier = NULL;
    }
else
    {
    name = (STR) input->nod_arg [1];
    qualifier = (STR) input->nod_arg [0];
    }
DEV_BLKCHK (name, type_str);
DEV_BLKCHK (qualifier, type_str);

/* Try to resolve field against various contexts;
   if there is an alias, check only against the first matching */

for (stack = request->req_context; stack; stack = stack->lls_next)
    if (field = resolve_context (request, name, qualifier, stack->lls_object))
	{
	if (list && !name)
	    {
	    node = MAKE_node( nod_relation, e_rel_count);
	    node->nod_arg[e_rel_context] = (CTX) stack->lls_object;
	    return node;
	    }

	if (!name)
	    break;

	for (; field; field = field->fld_next)
	    if ( !strcmp (name->str_data, field->fld_name))
		break;

	if (qualifier && !field)
	    break;

	if (field)
	    {
	    /* Intercept any reference to a field with datatype that
	       did not exist prior to V6 and post an error */

	    if (request->req_client_dialect <= SQL_DIALECT_V5)
		if (field->fld_dtype == dtype_sql_date ||
		    field->fld_dtype == dtype_sql_time ||
		    field->fld_dtype == dtype_int64)
		    {
		    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -206, 
			gds_arg_gds, gds__dsql_field_err, 
			gds_arg_gds, gds__random, 
			gds_arg_string, field->fld_name,
			gds_arg_gds, isc_sql_dialect_datatype_unsupport,
			gds_arg_number, request->req_client_dialect,
			gds_arg_string, DSC_dtype_tostring (field->fld_dtype),
			0);
		    return NULL;
		    };
	    context = (CTX) stack->lls_object;
	    if (indices)
		indices = PASS1_node (request, indices, FALSE);
	    node = MAKE_field (context, field, indices);
	    if (context->ctx_parent)
		{
	        if (!request->req_inhibit_map)
		    node = post_map (node, context->ctx_parent);
		else if (request->req_context != stack)
		    /* remember the agg context so that we can post
                       the agg to its map.     
 		     */
		    request->req_outer_agg_context = context->ctx_parent;
		}
	    return node;
	    }
	}
    

field_error (qualifier ? (TEXT *) qualifier->str_data : (TEXT *) NULL_PTR, 
	     name ? (TEXT *) name->str_data : (TEXT *) NULL_PTR);

return NULL;
}

static NOD pass1_insert (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ i n s e r t
 *
 **************************************
 *
 * Functional description
 *	Process INSERT statement.
 *
 **************************************/
NOD	rse, node, *ptr, *ptr2, *end, fields, values, temp;
FLD	field;
DSQL_REL	relation;
CTX	context;
LLS	stack;
PAR	parameter;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

request->req_type = REQ_INSERT;
node = MAKE_node (nod_store, e_sto_count);

/* Process SELECT expression, if present */

if (rse = input->nod_arg [e_ins_select])
    {
    node->nod_arg [e_sto_rse] = rse = PASS1_rse (request, rse, NULL_PTR);
    values = rse->nod_arg [e_rse_items];
    }
else
    values = PASS1_node (request, input->nod_arg [e_ins_values], 0);

/* Process relation */

node->nod_arg [e_sto_relation] = temp = 
	pass1_relation (request, input->nod_arg [e_ins_relation]);
context = (CTX) temp->nod_arg [0];
DEV_BLKCHK (context, type_ctx);
relation = context->ctx_relation;

/* If there isn't a field list, generate one */

if (fields = input->nod_arg [e_ins_fields])
    fields = PASS1_node (request, fields, 0);
else
    {
    stack = NULL;
    for (field = relation->rel_fields; field; field = field->fld_next)
	LLS_PUSH (MAKE_field (context, field, NULL_PTR), &stack);
    fields = MAKE_list (stack);
    }

/* Match field fields and values */

if (fields->nod_count != values->nod_count)
    /* count of column list and value list don't match */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -804, 
	gds_arg_gds, gds__dsql_var_count_err, 
	0);

stack = NULL;

for (ptr = fields->nod_arg, end = ptr + fields->nod_count, ptr2 = values->nod_arg;
     ptr < end; ptr++, ptr2++)
    {
    DEV_BLKCHK (*ptr, type_nod);
    DEV_BLKCHK (*ptr2, type_nod);
    temp = MAKE_node (nod_assign, 2);
    temp->nod_arg [0] = *ptr2;
    temp->nod_arg [1] = *ptr;
    LLS_PUSH (temp, &stack);
    temp = *ptr2;
    set_parameter_type (temp, *ptr, FALSE);
    }

node->nod_arg [e_sto_statement] = MAKE_list (stack);

set_parameters_name (node->nod_arg [e_sto_statement],
                     node->nod_arg [e_sto_relation]);

return node;
}

static NOD pass1_relation (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Prepare a relation name for processing.  
 *	Allocate a new relation node.
 *
 **************************************/
NOD	node;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

node = MAKE_node (nod_relation, e_rel_count);

node->nod_arg [e_rel_context] = 
   (NOD) PASS1_make_context (request, input);

return node;
}

static NOD pass1_alias_list (
    REQ		request,
    NOD		alias_list)
{
/**************************************
 *
 *	p a s s 1 _ a l i a s _ l i s t
 *
 **************************************
 *
 * Functional description
 *	The passed alias list fully specifies a relation.
 *	The first alias represents a relation specified in
 *	the from list at this scope levels.  Subsequent
 *	contexts, if there are any, represent base relations
 *	in a view stack.  They are used to fully specify a 
 *	base relation of a view.  The aliases used in the 
 *	view stack are those used in the view definition.
 *
 **************************************/
CTX	context, new_context;
DSQL_REL	relation;
NOD	*arg, *end;
USHORT	alias_length;
TEXT	*p, *q;
LLS	stack;
STR	alias;
TSQL	tdsql;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (alias_list, type_nod);

tdsql = GET_THREAD_DATA;

arg = alias_list->nod_arg;
end = alias_list->nod_arg + alias_list->nod_count;
                              
/* check the first alias in the list with the relations
   in the current context for a match */
                                        
if (context = pass1_alias (request, (STR) *arg))
    {
    if (alias_list->nod_count == 1)
        return (NOD) context;
    relation = context->ctx_relation;
    }

/* if the first alias didn't specify a table in the context stack, 
   look through all contexts to find one which might be a view with
   a base table having a matching table name or alias */

if (!context)
    for (stack = request->req_context; stack; stack = stack->lls_next)
	{
	context = (CTX) stack->lls_object;
        if ( context->ctx_scope_level == request->req_scope_level &&
	     context->ctx_relation)
	    if (relation = pass1_base_table (request, context->ctx_relation, (STR) *arg))
 	    	break;
	context = NULL;
	}

if (!context)
    /* there is no alias or table named %s at this scope level */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__dsql_no_relation_alias,
        gds_arg_string, ((STR) *arg)->str_data,
        0);

/* find the base table using the specified alias list, skipping the first one
   since we already matched it to the context */

for (arg++; arg < end; arg++)
    if (!(relation = pass1_base_table (request, relation, (STR) *arg)))
	break;

if (!relation)
    /* there is no alias or table named %s at this scope level */
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
        gds_arg_gds, gds__dsql_command_err, 
        gds_arg_gds, gds__dsql_no_relation_alias,
        gds_arg_string, ((STR) *arg)->str_data,
        0);
                                                                         
/* make up a dummy context to hold the resultant relation */

new_context = (CTX) ALLOCD (type_ctx);
new_context->ctx_context = context->ctx_context;
new_context->ctx_relation = relation;

/* concatenate all the contexts to form the alias name;
   calculate the length leaving room for spaces and a null */

alias_length = alias_list->nod_count;
for (arg = alias_list->nod_arg; arg < end; arg++)
    {
    DEV_BLKCHK (*arg, type_str);
    alias_length += ((STR) *arg)->str_length;
    }

alias = (STR) ALLOCDV (type_str, alias_length);
alias->str_length = alias_length;

p = new_context->ctx_alias = (TEXT*) alias->str_data;
for (arg = alias_list->nod_arg; arg < end; arg++)
    {
    DEV_BLKCHK (*arg, type_str);
    for (q = (TEXT*) ((STR) *arg)->str_data; *q;)
	*p++ = *q++;
    *p++ = ' ';    
    }

p [-1] = 0;

return (NOD) new_context;
}

static CTX pass1_alias (
    REQ		request,
    STR		alias)
{
/**************************************
 *
 *	p a s s 1 _ a l i a s
 *
 **************************************
 *
 * Functional description
 *	The passed relation or alias represents 
 *	a context which was previously specified
 *	in the from list.  Find and return the 
 *	proper context.
 *
 **************************************/
LLS	stack;
CTX	context, relation_context = NULL;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (alias, type_str);

/* look through all contexts at this scope level
   to find one that has a relation name or alias
   name which matches the identifier passed */

for (stack = request->req_context; stack; stack = stack->lls_next)
    {
    context = (CTX) stack->lls_object;
    if (context->ctx_scope_level != request->req_scope_level)
	continue;

    /* check for matching alias */

    if (context->ctx_alias &&
	!strcmp (context->ctx_alias, alias->str_data))
	return context;

    /* check for matching relation name; aliases take priority so
       save the context in case there is an alias of the same name;
       also to check that there is no self-join in the query */

    if (context->ctx_relation &&
	!strcmp (context->ctx_relation->rel_name, alias->str_data))
	{
	if (relation_context)
	    /* the table %s is referenced twice; use aliases to differentiate */
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104,
		gds_arg_gds, gds__dsql_command_err,
		gds_arg_gds, gds__dsql_self_join,
		gds_arg_string, alias->str_data,
		0);
	relation_context = context;
	}
    }

return relation_context;
}


static DSQL_REL pass1_base_table (
    REQ		request,
    DSQL_REL	relation, 
    STR		alias)
{
/**************************************
 *
 *	p a s s 1 _ b a s e _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Check if the relation in the passed context
 *	has a base table which matches the passed alias.
 *
 **************************************/

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (relation, type_dsql_rel);
DEV_BLKCHK (alias, type_str);

return METD_get_view_relation (request, relation->rel_name, alias->str_data, 0);
}

static NOD pass1_rse (
    REQ		request,
    NOD		input,
    NOD		order)
{
/**************************************
 *
 *	p a s s 1 _ r s e
 *
 **************************************
 *
 * Functional description
 *	Compile a record selection expression.  
 *	The input node may either be a "select_expression" 
 *	or a "list" (an implicit union).
 *
 **************************************/
NOD	rse, parent_rse, target_rse, aggregate, node, list, sub, *ptr, *end, proj;
LLS	stack;
CTX	context, parent_context;
TSQL	tdsql;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);
DEV_BLKCHK (order, type_nod);

tdsql = GET_THREAD_DATA;

aggregate = proj = NULL;

/* Handle implicit union case first.  Maybe it's not a union */

if (input->nod_type == nod_list)
    {
    if (input->nod_count == 1)
	return PASS1_rse (request, input->nod_arg [0], order);
    else 
	return pass1_union (request, input, order);
    }

/* Save the original base of the context stack and process relations */

parent_context = NULL;
parent_rse = NULL;
rse = target_rse = MAKE_node (nod_rse, e_rse_count);
rse->nod_arg [e_rse_streams] = PASS1_node (request, input->nod_arg [e_sel_from], 0);

/* A GROUP BY, HAVING, or any aggregate function in the select list will
   force an aggregate */

if ((node = input->nod_arg [e_sel_list]) && (node->nod_type == nod_list))
    for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr <end; ptr++)
	{
	sub = *ptr;
	if (sub->nod_type == nod_alias)
	    sub = sub->nod_arg [e_alias_value];

	if (aggregate_found (request, sub, &proj))
   	    {
	    parent_context = (CTX) ALLOCD (type_ctx);
	    break;
	    }
	}

if (!parent_context && (input->nod_arg [e_sel_group] || 
			input->nod_arg [e_sel_having]))
    parent_context = (CTX) ALLOCD (type_ctx);

if (parent_context)
    {
    parent_context->ctx_context = request->req_context_number++;
    aggregate = MAKE_node (nod_aggregate, e_agg_count);
    if (proj)
	{
	rse->nod_arg [e_rse_reduced] = MAKE_node (nod_list, 1);
	rse->nod_arg [e_rse_reduced]->nod_arg [0] = proj;
	}
    aggregate->nod_arg [e_agg_context] = (NOD) parent_context;
    aggregate->nod_arg [e_agg_rse] = rse;
    parent_rse = target_rse = MAKE_node (nod_rse, e_rse_count);
    parent_rse->nod_arg [e_rse_streams] = list = MAKE_node (nod_list, 1);
    list->nod_arg [0] = aggregate;
    }

/* Process boolean, if any */

if (node = input->nod_arg [e_sel_where])
    {
    ++request->req_in_where_clause;
    rse->nod_arg [e_rse_boolean] = PASS1_node (request, node, 0);
    --request->req_in_where_clause;
    }

/* Process GROUP BY clause, if any */

if (node = input->nod_arg [e_sel_group])
    aggregate->nod_arg [e_agg_group] = PASS1_node (request, node, 0);

if (parent_context)
    LLS_PUSH (parent_context, &request->req_context);
	
/* Process select list, if any.  If not, generate one */

if (node = input->nod_arg [e_sel_list])
    {
    ++request->req_in_select_list;
    target_rse->nod_arg [e_rse_items] = list = pass1_sel_list (request, node); 
    --request->req_in_select_list;
    if (aggregate)
	for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	    if (invalid_reference (*ptr, aggregate->nod_arg [e_agg_group]))
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		    gds_arg_gds, gds__field_ref_err,
			/* invalid field reference */
		    0);
    }
else
    {
    stack = NULL;
    list = rse->nod_arg [e_rse_streams];
    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	explode_asterisk (*ptr, aggregate, &stack);
    target_rse->nod_arg [e_rse_items] = MAKE_list (stack);
    }

/* parse a user-specified access plan */

if (node = input->nod_arg [e_sel_plan])
    {                
    /* disallow plans in a trigger for the short term,
       until we can figure out why they don't work: bug #6057 */

    if (request->req_flags & REQ_trigger)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__token_err,    /* Token unknown */
		gds_arg_gds, gds__random, 
		gds_arg_string, "PLAN", 0);
	
    rse->nod_arg [e_rse_plan] = PASS1_node (request, node, 0);
    }

/* If there is a parent context, replace original contexts with parent context */

if (parent_context)
    {
    remap_streams_to_parent_context (rse->nod_arg [e_rse_streams], parent_context);
    }

if (input->nod_arg [e_sel_distinct])
    target_rse->nod_arg [e_rse_reduced] = target_rse->nod_arg [e_rse_items];

/* Process ORDER clause, if any */

if (order)
    {
    ++request->req_in_order_by_clause;
    target_rse->nod_arg [e_rse_sort] =
	pass1_sort (request, order, input->nod_arg [e_sel_list]); 
    --request->req_in_order_by_clause;
    }

/* Unless there was a parent, we're done */

if (!parent_context)
    {
    rse->nod_arg [e_rse_singleton] = input->nod_arg [e_sel_singleton];
    return rse;
    }

/* Reset context of select items to point to the parent stream */
parent_rse->nod_arg [e_rse_items] = 
    copy_fields (parent_rse->nod_arg [e_rse_items], parent_context);

/* And, of course, reduction clauses must also apply to the parent */
if (input->nod_arg [e_sel_distinct])
    parent_rse->nod_arg [e_rse_reduced] = parent_rse->nod_arg [e_rse_items];

/* Process HAVING clause, if any */

if (node = input->nod_arg [e_sel_having])
    {
    ++request->req_in_having_clause;
    parent_rse->nod_arg [e_rse_boolean] = PASS1_node (request, node, 0);
    --request->req_in_having_clause;
#ifdef	CHECK_HAVING
    if (aggregate)
	if (invalid_reference (parent_rse->nod_arg [e_rse_boolean],
			       aggregate->nod_arg [e_agg_group]))
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__field_ref_err,
		    /* invalid field reference */
		    0);
#endif
    }

parent_rse->nod_arg [e_rse_singleton] = input->nod_arg [e_sel_singleton];
return parent_rse;
}

static NOD pass1_sel_list (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ s e l _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Compile a select list, which may contain things
 *	like "<table_name>.*".
 *
 **************************************/
NOD	node, *ptr, *end, frnode;
CTX	context;
LLS	stack;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

/*
   For each node in the list, if it's a field node, see if it's of
   the form <tablename>.*.   If so, explode the asterisk.   If not,
   just stack up the node.
*/

stack = NULL;
for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++)
    {
    DEV_BLKCHK (*ptr, type_nod);
    if ( (*ptr)->nod_type == nod_field_name)
	{

	/* check for field or relation node */

	frnode = pass1_field( request, *ptr, 1);
	if ( frnode->nod_type == nod_field)
	    LLS_PUSH( frnode, &stack);
	else
	    explode_asterisk( frnode, NULL, &stack);
	}
    else
	LLS_PUSH( PASS1_node (request, *ptr, 0), &stack);
    }
    node = MAKE_list (stack);

return node;
}

static NOD pass1_sort (
    REQ		request,
    NOD		input,
    NOD		s_list)
{
/**************************************
 *
 *	p a s s 1 _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Compile a parsed sort list
 *
 **************************************/
NOD	node, *ptr, *end, *ptr2, node1, node2;
ULONG	position;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);
DEV_BLKCHK (s_list, type_nod);

if (input->nod_type != nod_list)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
	gds_arg_gds, gds__dsql_command_err, 
	gds_arg_gds, gds__order_by_err,     /* invalid ORDER BY clause */
	0);

/* Node is simply to be rebuilt -- just recurse merrily */

node = MAKE_node (input->nod_type, input->nod_count);
ptr2 = node->nod_arg;

for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++)
    {
    DEV_BLKCHK (*ptr, type_nod);
    node1 = *ptr;
    if (node1->nod_type != nod_order)
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
	    gds_arg_gds, gds__dsql_command_err, 
	    gds_arg_gds, gds__order_by_err,     /* invalid ORDER BY clause */
	    0);
    node2 = MAKE_node (nod_order, e_order_count);
    node2->nod_arg[1] = node1->nod_arg[1];
    node1 = node1->nod_arg[0];
    if (node1->nod_type == nod_field_name)
	node2->nod_arg[0] = pass1_field (request, node1, 0);
    else if (node1->nod_type == nod_position)
	{
	position =  (ULONG) (node1->nod_arg[0]);
	if ((position < 1) || !s_list || (position > (ULONG) s_list->nod_count))
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		gds_arg_gds, gds__order_by_err,   /* invalid ORDER BY clause */
		0);
	node2->nod_arg[0] = PASS1_node (request, s_list->nod_arg[position - 1], 0);
	}
    else
	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
	    gds_arg_gds, gds__dsql_command_err, 
	    gds_arg_gds, gds__order_by_err,     /* invalid ORDER BY clause */
	    0);

    if ((*ptr)->nod_arg [e_order_collate])
	{
	DEV_BLKCHK ((*ptr)->nod_arg [e_order_collate], type_str);
	node2->nod_arg[0] = pass1_collate (request, node2->nod_arg[0], (STR) (*ptr)->nod_arg [e_order_collate]);
	}
    *ptr2++ = node2;
    }

return node;
}

static NOD pass1_udf (
    REQ		request,
    NOD		input,
    USHORT	proc_flag)
{
/**************************************
 *
 *	p a s s 1 _ u d f
 *
 **************************************
 *
 * Functional description
 *	Handle a reference to a user defined function.
 *
 **************************************/
STR	name;
UDF	udf;
NOD	node;
LLS	stack;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

name = (STR) input->nod_arg [0];
DEV_BLKCHK (name, type_str);
udf = METD_get_function (request, name);
if (!udf)
    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -804, 
	gds_arg_gds, gds__dsql_function_err, 
	gds_arg_gds, gds__random, 
	gds_arg_string, name->str_data, 
	0);

node = MAKE_node (nod_udf, input->nod_count);
node->nod_arg [0] = (NOD) udf;
if (input->nod_count == 2)
    {
    stack = NULL;
    pass1_udf_args (request, input->nod_arg [1], &stack, proc_flag);
    node->nod_arg [1] = MAKE_list (stack);
    }

return node;
}

static void pass1_udf_args (
    REQ		request,
    NOD		input,
    LLS		*stack,
    USHORT	proc_flag)
{
/**************************************
 *
 *	p a s s 1 _ u d f _ a r g s
 *
 **************************************
 *
 * Functional description
 *	Handle references to function arguments.
 *
 **************************************/
NOD	*ptr, *end;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

if (input->nod_type != nod_list)
    {
    LLS_PUSH (PASS1_node (request, input, proc_flag), stack);
    return;
    }

for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++)
    pass1_udf_args (request, *ptr, stack, proc_flag);
}

static NOD pass1_union (
    REQ		request,
    NOD		input,
    NOD		order_list)
{
/**************************************
 *
 *	p a s s 1 _ u n i o n
 *
 **************************************
 *
 * Functional description
 *	Handle a UNION of substreams, generating
 *	a mapping of all the fields and adding an
 *	implicit PROJECT clause to ensure that all 
 *	the records returned are unique.
 *
 **************************************/
NOD	map_node, *ptr, *end, items, union_node, *uptr, nod1;
NOD	union_rse, union_items, order1, order2, sort, position;
CTX	union_context;
MAP	map;
SSHORT	count = 0;
ULONG	number;
SSHORT	i, j;		/* for-loop counters */
TSQL	tdsql;
LLS	base;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);
DEV_BLKCHK (order_list, type_nod);

tdsql = GET_THREAD_DATA;

/* set up the rse node for the union */

union_rse = MAKE_node (nod_rse, e_rse_count);
union_rse->nod_arg [e_rse_streams] = union_node = MAKE_node (nod_union, input->nod_count);

/* process all the sub-rse's */
     
uptr = union_node->nod_arg;
base = request->req_context;
for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++, uptr++)
    {
    *uptr = PASS1_rse (request, *ptr, NULL_PTR);
     /**
     Do Not pop-off the sub-rse's from the context. This will be
     needed to generate the correct dyn for view with  unions in
     it{dsql/ddl.c define_view()}. Note that the rse's will be 
     popped off later in reset_context_stack(){dsql/ddl.c} after 
     the dyn generation and at the end of PASS1_statement() after 
     blr generation.
     -Sudesh 06/03/1999
     while (request->req_context != base)
	LLS_POP (&request->req_context);
     **/
    }

/* generate a context for the union itself */

union_context = (CTX) ALLOCDV (type_ctx, 0);
union_context->ctx_context = request->req_context_number++;

/* generate the list of fields to select */

items = union_node->nod_arg [0]->nod_arg [e_rse_items];

/* loop through the list nodes, checking to be sure that they have the same 
   number and type of items */

for ( i = 1; i < union_node->nod_count; i++ )
	{
	nod1 = union_node->nod_arg [i]->nod_arg [e_rse_items];
	if ( items->nod_count != nod1->nod_count )
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		gds_arg_gds, gds__dsql_count_mismatch,	/* overload of msg */
		0);

	for ( j = 0; j < nod1->nod_count; j++ )
	    {
	    MAKE_desc (&items->nod_arg [j]->nod_desc, items->nod_arg [j]);
	    MAKE_desc ( &nod1->nod_arg [j]->nod_desc,  nod1->nod_arg [j]);

	    /* SQL II, section 9.3, pg 195 governs which data types
	     * are considered equivilant for a UNION
	     * The following restriction is in some ways more restrictive
	     *  (cannot UNION CHAR with VARCHAR for instance)
	     *  (or cannot union CHAR of different lengths)
	     * and in someways less restrictive
	     *  (SCALE is not looked at)
	     * Workaround: use a direct CAST() statement in the SQL
	     * statement to force desired datatype.
	     */
	    if ((( nod1->nod_arg [j]->nod_desc.dsc_dtype) != 
		 (items->nod_arg [j]->nod_desc.dsc_dtype)) ||
		(( nod1->nod_arg [j]->nod_desc.dsc_length) != 
		 (items->nod_arg [j]->nod_desc.dsc_length)))
		ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		    gds_arg_gds, gds__dsql_command_err, 
		    gds_arg_gds, gds__dsql_datatype_err, /* overload of msg */
		    0);
	    /** 
		We look only at the items->nod_arg[] when creating the
		output descriptors. Make sure that the sub-rses
		descriptor flags are copied onto items->nod_arg[]->nod_desc.
		Bug 65584.
		-Sudesh 07/28/1999.
	    **/
	    if (nod1->nod_arg [j]->nod_desc.dsc_flags & DSC_nullable)
		items->nod_arg [j]->nod_desc.dsc_flags |= DSC_nullable;
	    }
	}

union_items = MAKE_node (nod_list, items->nod_count);
     
uptr = items->nod_arg;
for (ptr = union_items->nod_arg, end = ptr + union_items->nod_count; ptr < end; ptr++)
    {
    *ptr = map_node = MAKE_node (nod_map, e_map_count);
    map_node->nod_arg [e_map_context] = (NOD) union_context;
    map = (MAP) ALLOCD (type_map);
    map_node->nod_arg [e_map_map] = (NOD) map;

    /* set up the MAP between the sub-rses and the union context */

    map->map_position = count++;
    map->map_node = *uptr++;
    map->map_next = union_context->ctx_map;
    union_context->ctx_map = map;
    }

union_rse->nod_arg [e_rse_items] = union_items;

/* Process ORDER clause, if any */

if (order_list)
    {
    sort = MAKE_node (nod_list, order_list->nod_count);
    uptr = sort->nod_arg;
    for (ptr = order_list->nod_arg, end = ptr + order_list->nod_count; ptr < end; ptr++, uptr++)
	{
	order1 = *ptr;
	position = order1->nod_arg [0];
	if (position->nod_type != nod_position)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		gds_arg_gds, gds__order_by_err, /* invalid ORDER BY clause */
		0);
       	number = (ULONG) position->nod_arg [0];
	if (number < 1 || number > union_items->nod_count)
	    ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -104, 
		gds_arg_gds, gds__dsql_command_err, 
		gds_arg_gds, gds__order_by_err, /* invalid ORDER BY clause */
		0);

	/* make a new order node pointing at the Nth item in the select list */

	*uptr = order2 = MAKE_node (nod_order, 2);
	order2->nod_arg [0] = union_items->nod_arg [number-1];
	order2->nod_arg [1] = order1->nod_arg [1];
	}
    union_rse->nod_arg [e_rse_sort] = sort;
    }

/* PROJECT on all the select items unless UNION ALL was specified */

if (!(input->nod_flags & NOD_UNION_ALL))
    union_rse->nod_arg [e_rse_reduced] = union_items;

return union_rse;
}

static NOD pass1_update (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Process UPDATE statement.
 *
 **************************************/
NOD	rse, node, temp, relation, cursor;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

cursor = input->nod_arg [e_upd_cursor];
relation = input->nod_arg [e_upd_relation];
if (cursor && (request->req_flags & REQ_procedure))
    {
    node = MAKE_node (nod_modify_current, e_mdc_count);
    node->nod_arg [e_mdc_context] = (NOD) pass1_cursor_context (request, cursor, relation);
    node->nod_arg [e_mdc_update] = PASS1_node (request, relation, 0);
    node->nod_arg [e_mdc_statement] =
		PASS1_statement (request, input->nod_arg [e_upd_statement], 0);
    LLS_POP (&request->req_context);
    return node;
    }

request->req_type = (cursor) ? REQ_UPDATE_CURSOR : REQ_UPDATE;
node = MAKE_node (nod_modify, e_mod_count);
node->nod_arg [e_mod_update] = PASS1_node (request, relation, 0);
node->nod_arg [e_mod_statement] = PASS1_statement (request, input->nod_arg [e_upd_statement], 0);

set_parameters_name (node->nod_arg [e_mod_statement],
                     node->nod_arg [e_mod_update]);

/* Generate record selection expression */

if (cursor)
    rse = pass1_cursor (request, cursor, relation);
else
    {
    rse = MAKE_node (nod_rse, e_rse_count);
    rse->nod_arg [e_rse_streams] = temp = MAKE_node (nod_list, 1);
    temp->nod_arg [0] = node->nod_arg [e_mod_update];
    if (temp = input->nod_arg [e_upd_boolean])
	rse->nod_arg [e_rse_boolean] = PASS1_node (request, temp, 0);
    }

temp = rse->nod_arg [e_rse_streams];
node->nod_arg [e_mod_source] = temp->nod_arg [0];
node->nod_arg [e_mod_rse] = rse;

LLS_POP (&request->req_context);
return node;
}

static NOD pass1_variable (
    REQ		request,
    NOD		input)
{
/**************************************
 *
 *	p a s s 1 _ v a r i a b l e
 *
 **************************************
 *
 * Functional description
 *	Resolve a variable name to an available variable.
 *
 **************************************/
NOD	procedure_node, var_nodes, var_node, *ptr, *end;
STR	var_name;
VAR	var;
SSHORT	position;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (input, type_nod);

if (input->nod_type == nod_field_name)
    {
    if (input->nod_arg[e_fln_context])
	{
	if (request->req_flags & REQ_trigger)
	    return pass1_field (request, input, 0);
	else
	    field_error (NULL_PTR, NULL_PTR);
	}
    var_name = (STR) input->nod_arg[e_fln_name];
    }
else
    var_name = (STR) input->nod_arg[e_vrn_name];
DEV_BLKCHK (var_name, type_str);

if ((procedure_node = request->req_ddl_node) &&
    (procedure_node->nod_type == nod_def_procedure ||
     procedure_node->nod_type == nod_mod_procedure ||
     procedure_node->nod_type == nod_def_trigger ||
     procedure_node->nod_type == nod_mod_trigger))
    {
    if (procedure_node->nod_type == nod_def_procedure ||
	procedure_node->nod_type == nod_mod_procedure)
	{
	/* Try to resolve variable name against input, output
	   and local variables */

	if (var_nodes = procedure_node->nod_arg [e_prc_inputs])
	    for (position = 0, ptr = var_nodes->nod_arg, end = ptr + var_nodes->nod_count;
		 ptr < end; ptr++, position++)
		{
		var_node = *ptr;
		var = (VAR) var_node->nod_arg [e_var_variable];
		DEV_BLKCHK (var, type_var);
		if (!strcmp (var_name->str_data, var->var_name))
		    return var_node;
		}
	if (var_nodes = procedure_node->nod_arg [e_prc_outputs])
	    for (position = 0, ptr = var_nodes->nod_arg, end = ptr + var_nodes->nod_count;
		 ptr < end; ptr++, position++)
		{
		var_node = *ptr;
		var = (VAR) var_node->nod_arg [e_var_variable];
		DEV_BLKCHK (var, type_var);
		if (!strcmp (var_name->str_data, var->var_name))
		    return var_node;
		}
	var_nodes = procedure_node->nod_arg [e_prc_dcls];
	}
    else
	var_nodes = procedure_node->nod_arg [e_trg_actions]->nod_arg [0];

    if (var_nodes)
	for (position = 0, ptr = var_nodes->nod_arg, end = ptr + var_nodes->nod_count;
	     ptr < end; ptr++, position++)
	    {
	    var_node = *ptr;
	    var = (VAR) var_node->nod_arg [e_var_variable];
	    DEV_BLKCHK (var, type_var);
	    if (!strcmp (var_name->str_data, var->var_name))
		return var_node;
	    }
    }

/* field unresolved */

field_error (NULL_PTR, NULL_PTR);

return NULL;
}

static NOD post_map (
    NOD		node,
    CTX		context)
{
/**************************************
 *
 *	p o s t _ m a p
 *
 **************************************
 *
 * Functional description
 *	Post an item to a map for a context.
 *
 **************************************/
NOD	new_node;
MAP	map;
USHORT	count;
TSQL	tdsql;

DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (context, type_ctx);

tdsql = GET_THREAD_DATA;

/* Check to see if the item has already been posted */

for (map = context->ctx_map, count = 0; map; map = map->map_next, count++)
    if (node_match (node, map->map_node))
	break;

if (!map)
    {
    map = (MAP) ALLOCD (type_map);
    map->map_position = count;
    map->map_next = context->ctx_map;
    context->ctx_map = map;
    map->map_node = node;
    }

new_node = MAKE_node (nod_map, e_map_count);
new_node->nod_count = 0;
new_node->nod_arg [e_map_context] = (NOD) context;
new_node->nod_arg [e_map_map] = (NOD) map;
new_node->nod_desc = node->nod_desc;

return new_node;
}

static void remap_streams_to_parent_context (
    NOD		input,
    CTX		parent_context)
{
/**************************************
 *
 *      r e m a p _ s t r e a m s _ t o _ p a r e n t _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	For each relation in the list, flag the relation's context
 *	as having a parent context.  Be sure to handle joins
 *	(Bug 6674).
 *
 **************************************/
CTX	context;
NOD	*ptr, *end;

DEV_BLKCHK (input, type_nod);
DEV_BLKCHK (parent_context, type_ctx);

switch (input->nod_type)
    {
    case nod_list:
	for (ptr = input->nod_arg, end = ptr + input->nod_count; ptr < end; ptr++)
	    remap_streams_to_parent_context (*ptr, parent_context);
	break;

    case nod_relation:
	context = (CTX) input->nod_arg [e_rel_context];
	DEV_BLKCHK (context, type_ctx);
	context->ctx_parent = parent_context;
	break;

    case nod_join:
	remap_streams_to_parent_context (input->nod_arg [e_join_left_rel], parent_context);
	remap_streams_to_parent_context (input->nod_arg [e_join_rght_rel], parent_context);
	break;

    default:
	ASSERT_FAIL;
	break;
    }
}

static FLD resolve_context (
    REQ		request,
    STR		name,
    STR		qualifier,
    CTX		context)
{
/**************************************
 *
 *	r e s o l v e _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Attempt to resolve field against context.  
 *	Return first field in context if successful, 
 *	NULL if not.
 *
 **************************************/
DSQL_REL	relation;
PRC	procedure;
FLD	field;
TEXT	*table_name;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (name, type_str);
DEV_BLKCHK (qualifier, type_str);
DEV_BLKCHK (context, type_ctx);

relation = context->ctx_relation;
procedure = context->ctx_procedure;
if (!relation && !procedure)
    return NULL;

/* if there is no qualifier, then we cannot match against
   a context of a different scoping level */

if (!qualifier && context->ctx_scope_level != request->req_scope_level)
    return NULL;

if (relation)
    table_name = relation->rel_name;
else
    table_name = procedure->prc_name;

/* If a context qualifier is present, make sure this is the
   proper context */

if (qualifier &&
    strcmp (qualifier->str_data, table_name) &&
    (!context->ctx_alias || strcmp (qualifier->str_data, context->ctx_alias)))
    return NULL;

/* Lookup field in relation or procedure */

if (relation)
    field = relation->rel_fields;
else
    field = procedure->prc_outputs;

return field;
}

static BOOLEAN set_parameter_type (
    NOD		in_node,
    NOD		node,
    BOOLEAN	force_varchar)
{
/**************************************
 *
 *	s e t _ p a r a m e t e r _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Setup the datatype of a parameter.
 *
 **************************************/  
NOD	*ptr, *end;
PAR	parameter;
BOOLEAN	result = 0;

DEV_BLKCHK (in_node, type_nod);
DEV_BLKCHK (node, type_nod);

if (in_node == NULL)
    return FALSE;

switch (in_node->nod_type)
    {
    case nod_parameter:
	MAKE_desc (&in_node->nod_desc, node);
	parameter = (PAR) in_node->nod_arg [e_par_parameter];
	DEV_BLKCHK (parameter, type_par);
	parameter->par_desc = in_node->nod_desc;
	parameter->par_node = in_node;

	/* Parameters should receive precisely the data that the user
	   passes in.  Therefore for text strings lets use varying
	   strings to insure that we don't add trailing blanks. 
	   
	   However, there are situations this leads to problems - so
	   we use the force_varchar parameter to prevent this
	   datatype assumption from occuring.
	*/

	if (force_varchar)
	    {
	    if (parameter->par_desc.dsc_dtype == dtype_text)
		{
		parameter->par_desc.dsc_dtype = dtype_varying;
		parameter->par_desc.dsc_length += sizeof (USHORT);
		}
	    else if (parameter->par_desc.dsc_dtype > dtype_any_text)
		{
		/* The LIKE & similar parameters must be varchar type
		 * strings - so force this parameter to be varchar
		 * and take a guess at a good length for it.
		 */
		parameter->par_desc.dsc_dtype = dtype_varying;
		parameter->par_desc.dsc_length = 30 + sizeof (USHORT);
		parameter->par_desc.dsc_sub_type = 0;
		parameter->par_desc.dsc_scale = 0;
		parameter->par_desc.dsc_ttype = ttype_dynamic;
		}
	    }
	return TRUE;

    case nod_cast:
	/* Unable to guess parameters within a CAST() statement - as
	 * any datatype could be the input to cast.
	 */
	return FALSE;

    case nod_add:
    case nod_add2:
    case nod_concatenate:
    case nod_divide:
    case nod_divide2:
    case nod_multiply:
    case nod_multiply2:
    case nod_negate:
    case nod_substr:
    case nod_subtract:
    case nod_subtract2:
    case nod_upcase:
    case nod_extract:
	result = 0;
	for (ptr = in_node->nod_arg, end = ptr + in_node->nod_count; ptr < end; ptr++)
	    result |= set_parameter_type (*ptr, node, force_varchar);
	return result;

    default:
	return FALSE;
    }
}

static void set_parameters_name (
    NOD         list_node,
    NOD		rel_node)
{
/**************************************
 *
 *      s e t _ p a r a m e t e r s _ n a m e
 *
 **************************************
 *
 * Functional description
 *      Setup parameter parameters name.
 *
 **************************************/
NOD     *ptr, *end;
CTX	context;
DSQL_REL	relation;

DEV_BLKCHK (list_node, type_nod);
DEV_BLKCHK (rel_node, type_nod);

context = (CTX) rel_node->nod_arg [0];
DEV_BLKCHK (context, type_ctx);
relation = context->ctx_relation;

for (ptr = list_node->nod_arg, end = ptr+list_node->nod_count; ptr < end; ptr++)
    {
    DEV_BLKCHK (*ptr, type_nod);
    if ((*ptr)->nod_type == nod_assign)
	set_parameter_name ((*ptr)->nod_arg[0],
			    (*ptr)->nod_arg[1], relation);
    else
	assert (FALSE);
    }
}

static void set_parameter_name (
    NOD         par_node,
    NOD         fld_node,
    DSQL_REL	relation)
{
/**************************************
 *
 *      s e t _ p a r a m e t e r _ n a m e
 *
 **************************************
 *
 * Functional description
 *      Setup parameter parameter name.
 *	This function was added as a part of array data type
 *	support for InterClient. It is	called when either
 *	"insert" or "update" statements are parsed. If the
 *	statements have input parameters, than the parameter
 *	is assigned the name of the field it is being inserted
 *	(or updated). The same goes to the name of a relation.
 *	The names are assigned to the parameter only if the
 *	field is of array data type.
 *
 **************************************/
NOD     *ptr, *end;
PAR     parameter;
FLD	field;

DEV_BLKCHK (par_node, type_nod);
DEV_BLKCHK (fld_node, type_nod);
DEV_BLKCHK (relation, type_dsql_rel);

/* Could it be something else ??? */
assert (fld_node->nod_type == nod_field);

if (fld_node->nod_desc.dsc_dtype != dtype_array)
    return;
 
switch (par_node->nod_type)
    {
    case nod_parameter:
	parameter = (PAR) par_node->nod_arg [e_par_parameter];
	DEV_BLKCHK (parameter, type_par);
	field = (FLD) fld_node->nod_arg [e_fld_field];
	DEV_BLKCHK (field, type_fld);
	parameter->par_name = field->fld_name;
	parameter->par_rel_name = relation->rel_name;
	return;

    case nod_add:
    case nod_add2:
    case nod_concatenate:
    case nod_divide:
    case nod_divide2:
    case nod_multiply:
    case nod_multiply2:
    case nod_negate:
    case nod_substr:
    case nod_subtract:
    case nod_subtract2:
    case nod_upcase:
        for (ptr = par_node->nod_arg, end = ptr + par_node->nod_count;
             ptr < end; ptr++)
            set_parameter_name (*ptr, fld_node, relation);
        return;
 
    default:
        return;
    }
}
