/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		opt.c
 *	DESCRIPTION:	Optimizer / record selection expression compiler
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
#include "../jrd/ibsetjmp.h"
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/align.h"
#include "../jrd/val.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/lls.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/sort.h"
#include "../jrd/rse.h"
#include "../jrd/intl.h"
#include "../jrd/thd.h"
#include "../jrd/gdsassert.h"
#include "../jrd/all_proto.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/intl_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/opt_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/sbm_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/gds_proto.h"

static BOOLEAN	augment_stack (NOD, LLS *);
static void	check_indices (struct csb_repeat *);
static BOOLEAN	check_relationship (OPT, USHORT, USHORT);
static void	check_sorts (RSE);
static void	class_mask (USHORT, NOD *, ULONG *);
static void	clear_bounds (OPT, IDX *);
static NOD	compose (NOD *, NOD, NOD_T);
static BOOLEAN	computable (CSB, register NOD, SSHORT, BOOLEAN);
static void	compute_dependencies (NOD, ULONG *);
static void	compute_dbkey_streams (CSB, NOD, UCHAR *);
static void	compute_rse_streams (CSB, RSE, UCHAR *);
static SLONG	decompose (TDBB, register NOD, LLS *, CSB);
static USHORT	distribute_equalities (LLS *, CSB);
static BOOLEAN	dump_index (NOD, SCHAR **, SSHORT *);
static BOOLEAN	dump_rsb (REQ, RSB, SCHAR **, SSHORT *);
static BOOLEAN	estimate_cost (TDBB, OPT, USHORT, double *, double *);
#ifdef EXPRESSION_INDICES
static BOOLEAN	expression_equal (TDBB, NOD, NOD);
#endif
static void	find_best (TDBB, register OPT, USHORT, USHORT, UCHAR *, NOD, double, double);
static NOD	find_dbkey (NOD, USHORT, SLONG *);
static USHORT	find_order (TDBB, register OPT, UCHAR *, NOD);
static void	find_rsbs (RSB, LLS *, LLS *);
static void	form_rivers (TDBB, OPT, UCHAR *, LLS *, NOD *, NOD *, NOD);
static BOOLEAN	form_river (TDBB, OPT, USHORT, UCHAR *, UCHAR *, LLS *, NOD *, NOD *, NOD);
static RSB	gen_aggregate (TDBB, OPT, NOD);
static RSB	gen_boolean (TDBB, register OPT, RSB, NOD);
static RSB	gen_first (TDBB, register OPT, RSB, NOD);
static void	gen_join (TDBB, OPT, UCHAR *, LLS *, NOD *, NOD *, NOD);
static RSB	gen_navigation (TDBB, OPT, USHORT, REL, STR, IDX *, NOD *);
#ifdef SCROLLABLE_CURSORS
static RSB	gen_nav_rsb (TDBB, OPT, USHORT, REL, STR, IDX *, RSE_GET_MODE);
#else
static RSB	gen_nav_rsb (TDBB, OPT, USHORT, REL, STR, IDX *);
#endif
static RSB	gen_outer (TDBB, OPT, RSE, LLS, NOD *, NOD *);
static RSB	gen_procedure (TDBB, OPT, NOD);
static RSB	gen_residual_boolean (TDBB, register OPT, RSB);
static RSB	gen_retrieval (TDBB, OPT, SSHORT, NOD *, NOD *, BOOLEAN, BOOLEAN, NOD *);
static RSB	gen_rsb (TDBB, OPT, RSB, NOD, SSHORT, REL, STR, NOD, float);
static RSB	gen_sort (TDBB, OPT, UCHAR *, UCHAR *, RSB, NOD, USHORT);
static BOOLEAN	gen_sort_merge (TDBB, OPT, LLS *);
static RSB	gen_union (TDBB, OPT, NOD, UCHAR *, USHORT);
static void	get_inactivities (CSB, ULONG *);
static IRL	indexed_relationship (TDBB, OPT, USHORT);
static STR	make_alias (TDBB, CSB, struct csb_repeat *);
static NOD	make_binary_node (NOD_T, NOD, NOD, USHORT);
static RSB	make_cross (TDBB, OPT, LLS);
static NOD	make_index_node (TDBB, REL, CSB, IDX *);
static NOD 	make_inference_node (CSB,NOD,NOD,NOD);
static NOD	make_inversion (TDBB, register OPT, NOD, USHORT);
static NOD	make_missing (TDBB, register OPT, REL, NOD, USHORT, IDX *);
static NOD	make_starts (TDBB, register OPT, REL, NOD, USHORT, IDX *);
static BOOLEAN 	map_equal (NOD, NOD, NOD); 
static void	mark_indices (struct csb_repeat *, SSHORT);
static SSHORT	match_index (TDBB, register OPT, SSHORT, register NOD, IDX *);
static USHORT	nav_rsb_size (RSB, USHORT, USHORT);
static BOOLEAN	node_equality (NOD, NOD);
static NOD	optimize_like (TDBB, NOD);
static void	print_order (OPT, USHORT, double, double);
static USHORT	river_count (USHORT, NOD *);
static BOOLEAN	river_reference (RIV, NOD);
static BOOLEAN	search_stack (NOD, LLS);
static void	set_active (OPT, RIV);
static void	set_direction (NOD, NOD);
static void	set_inactive (OPT, RIV);
static void 	set_made_river (OPT, RIV);
static void	set_position (NOD, NOD, NOD);
static void 	set_rse_inactive (CSB, RSE);

/* macro definitions */

#ifdef DEV_BUILD
#define OPT_DEBUG
#endif

#ifdef OPT_DEBUG
#define DEBUG_PUNT		5
#define DEBUG_RELATIONSHIPS	4
#define DEBUG_ALL		3
#define DEBUG_CANDIDATE		2
#define DEBUG_BEST		1
#define DEBUG_NONE		0

IB_FILE *opt_debug_file = NULL_PTR;
static int opt_debug_flag=DEBUG_NONE;
#endif

#define	SET_DEP_BIT(array, bit)	array[(bit)/32] |= (1L << ((bit) % 32))
#define	CLEAR_DEP_BIT(array, bit)	array[(bit)/32] &= ~(1L << ((bit) % 32))
#define	TEST_DEP_BIT(array, bit)	(array[(bit)/32] & (1L << ((bit) % 32)))
#define	TEST_DEP_ARRAYS(ar1, ar2)	((ar1[0] & ar2[0]) || (ar1[1] & ar2[1]) || \
					 (ar1[2] & ar2[2]) || (ar1[3] & ar2[3]) || \
					 (ar1[4] & ar2[4]) || (ar1[5] & ar2[5]) || \
					 (ar1[6] & ar2[6]) || (ar1[7] & ar2[7]))

#define STREAM_INDEX(node)	(node->nod_type == nod_relation) ? e_rel_stream : \
	      	     		(node->nod_type == nod_procedure) ? e_prc_stream : \
	      	     		(node->nod_type == nod_union) ? e_uni_stream : e_agg_stream

/* some arbitrary fudge factors for calculating costs, etc.--
   these could probably be better tuned someday */

#define ESTIMATED_SELECTIVITY	0.01
#define INVERSE_ESTIMATE	10
#define INDEX_COST		30.0
#define CACHE_PAGES_PER_STREAM	15

#define SQL_MATCH_1_CHAR	'_'		/* Not translatable */
#define SQL_MATCH_ANY_CHARS	'%'		/* Not translatable */

/* enumeration of sort datatypes */

static CONST UCHAR	sort_dtypes [] = {
	0,		/* dtype_null */
	SKD_text,	/* dtype_text */
	SKD_cstring,	/* dtype_cstring */
	SKD_varying,	/* dtype_varying */
	0,
	0,
	0,		/* dtype_packed */
	0,		/* dtype_byte */
	SKD_short,	/* dtype_short */
	SKD_long,	/* dtype_long */
	SKD_quad,	/* dtype_quad */
	SKD_float,	/* dtype_real */
	SKD_double,	/* dtype_double */
	SKD_double,	/* dtype_d_float */
	SKD_sql_date,	/* dtype_sql_date */
	SKD_sql_time,	/* dtype_sql_time */
	SKD_timestamp2,	/* dtype_timestamp */
	0,		/* dtype_blob */
	0,		/* dtype_array */
	SKD_int64	/* dtype_int64 */
};

BOOLEAN OPT_access_path (
    REQ		request,
    SCHAR	*buffer,
    SSHORT	buffer_length, 
    USHORT	*return_length)
{
/**************************************
 *
 *	O P T _ a c c e s s _ p a t h
 *
 **************************************
 *
 * Functional description
 *	Returns a formatted access path for all
 *	rse's in the specified request.
 *
 **************************************/
VEC	vector;
RSB	rsb;
SLONG	i;
SCHAR	*begin;

DEV_BLKCHK (request, type_req);

begin = buffer;

/* loop through all RSEs in the request, 
   and describe the rsb tree for that rsb;
   go backwards because items were popped
   off the stack backwards */

vector = request->req_fors;
if (!vector)
    return FALSE;

for (i = vector->vec_count - 1; i >= 0; i--)
    {
    rsb = (RSB) vector->vec_object [i]; 
    if (rsb && !dump_rsb (request, rsb, &buffer, &buffer_length))
	break;
    }

*return_length = buffer - begin;

if (i >= 0)
    return FALSE;
else 
    return TRUE;
}

RSB OPT_compile (
    TDBB		tdbb,
    register CSB	csb,
    register RSE	rse,
    LLS			parent_stack)
{
/**************************************
 *
 *	O P T _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *	Compile and optimize a record selection expression into a
 *	set of record source blocks (rsb's).
 *
 **************************************/
DBB		dbb;
OPT		opt;
IDX		*idx;
RIV		river;
NOD		node, *ptr, *end, sort, project, aggregate;
LLS		conjunct_stack, rivers_stack, *stack_end;
RSB		rsb;
REL		relation;
SLONG		idx_size, conjunct_count;
SSHORT		i, stream;
#ifndef STACK_REDUCTION
UCHAR		*p, *q, streams [MAX_STREAMS], beds [MAX_STREAMS], 
		*k, *b_end, *k_end, key_streams [MAX_STREAMS],
		local_streams [MAX_STREAMS];
#else
UCHAR		*local_streams;
UCHAR		*p, *q, *streams, *beds,
		*k, *b_end, *k_end, *key_streams;
#endif
JMP_BUF		env, *old_env;

DEV_BLKCHK (csb, type_csb);
DEV_BLKCHK (rse, type_nod);
DEV_BLKCHK (parent_stack, type_lls);

#if defined(NETWARE_386) || defined(WINDOWS_ONLY)
if ((unsigned) stackavail () < (unsigned) STACK_SAFE_LIMIT)
    ERR_post (isc_err_stack_limit, 0);
#endif

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

#ifdef OPT_DEBUG
if (opt_debug_flag != DEBUG_NONE && !opt_debug_file)
   opt_debug_file = ib_fopen( "opt_debug.out", "w" );
#endif
   

/* If there is a boolean, there is some work to be done.  First,
   decompose the boolean into conjunctions.  Then get descriptions
   of all indices for all relations in the rse.  This will give
   us the info necessary to allocate a optimizer block big
   enough to hold this crud. */


/* Do not allocate the IDX struct. Let BTR_all do the job. The allocated
   memory will then be in csb->csb_rpt[stream].csb_idx_allocation, which
   gets cleaned up before this function exits. */

idx = (IDX*)NULL;
idx_size = 0;

#ifdef STACK_REDUCTION

/* allocate local memory, make a block allocation for similar items */

streams = (UCHAR *)ALLOC_LIB_MEMORY ((DWORD)(sizeof (UCHAR) * MAX_STREAMS));
beds = (UCHAR *)ALLOC_LIB_MEMORY ((DWORD)(sizeof (UCHAR) * MAX_STREAMS));
key_streams = (UCHAR *)ALLOC_LIB_MEMORY ((DWORD)(sizeof (UCHAR) * MAX_STREAMS));
local_streams = (UCHAR *)ALLOC_LIB_MEMORY ((DWORD)(sizeof (UCHAR) * MAX_STREAMS));
opt = (OPT) ALLOC_LIB_MEMORY((DWORD)( sizeof( struct opt)));
if (streams == NULL || beds == NULL || key_streams == NULL ||
        local_streams == NULL || opt == NULL)
    {
    if (local_streams != NULL)
        FREE_LIB_MEMORY (local_streams);
    if (key_streams != NULL)
        FREE_LIB_MEMORY (key_streams);
    if (beds != NULL)
        FREE_LIB_MEMORY (beds);
    if (streams != NULL)
        FREE_LIB_MEMORY (streams);
    if (opt != NULL)
        FREE_LIB_MEMORY (opt);
    ERR_post (isc_virmemexh, 0);
    }

/* clear block and set block type */

MOVE_CLEAR (opt, sizeof( struct opt));
((BLK)opt)->blk_type = type_opt;

#else

opt = (OPT) ALLOCP (type_opt);

#endif

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    for (i = 0; i < streams [0]; i++)
    	{
    	stream = streams [i+1];
    	if (csb->csb_rpt [stream].csb_idx_allocation)
	    ALL_RELEASE (csb->csb_rpt [stream].csb_idx_allocation);
    	csb->csb_rpt [stream].csb_idx_allocation = NULL_PTR;
    	}
#ifdef STACK_REDUCTION
    FREE_LIB_MEMORY (local_streams);
    FREE_LIB_MEMORY (key_streams);
    FREE_LIB_MEMORY (beds);
    FREE_LIB_MEMORY (streams);
    FREE_LIB_MEMORY (opt);
#else
    ALL_RELEASE (opt);
#endif
    ERR_punt();
    }

opt->opt_csb = csb;
if (rse->nod_flags & rse_stream)
    opt->opt_g_flags |= opt_g_stream;

beds [0] = streams [0] = key_streams [0] = 0;
conjunct_stack = rivers_stack = NULL;
conjunct_count = 0;

check_sorts (rse);
sort = rse->rse_sorted;
project = rse->rse_projection;
aggregate = rse->rse_aggregate;

/* put any additional booleans on the conjunct stack, and see if we 
   can generate additional booleans by associativity--this will help 
   to utilize indices that we might not have noticed */

if (rse->rse_boolean)
    conjunct_count = decompose (tdbb, rse->rse_boolean, &conjunct_stack, csb);

conjunct_count += distribute_equalities (&conjunct_stack, csb);

/* find the end of the conjunct stack. */

for (stack_end = &conjunct_stack; *stack_end; stack_end = &(*stack_end)->lls_next)
    ;

/* clear the csb_active flag of all streams in the rse */
set_rse_inactive (csb, rse);

p = streams + 1;

/* go through the record selection expression generating 
   record source blocks for all streams */

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    {
    node = *ptr;

    /* find the stream number and place it at the end of the beds array
       (if this is really a stream and not another rse) */

    if (node->nod_type != nod_rse)
	{                                   
	stream = (USHORT) node->nod_arg [STREAM_INDEX(node)];
	beds [++beds [0]] = (UCHAR) stream;
	}

    /* for nodes which are not relations, generate an rsb to
       represent that work has to be done to retrieve them;
       find all the substreams involved and compile them as well */

    rsb = NULL;
    local_streams [0] = 0;
    if (node->nod_type == nod_union)
	{
	i = (SSHORT) key_streams [0];
	compute_dbkey_streams (csb, node, key_streams);
	rsb = gen_union (tdbb, opt, node, key_streams + i + 1, (USHORT) (key_streams [0] - i));
	local_streams [++local_streams [0]] = (UCHAR) node->nod_arg [e_uni_stream];
	}
    else if (node->nod_type == nod_aggregate)
	{
	rsb = gen_aggregate (tdbb, opt, node);
	local_streams [++local_streams [0]] = (UCHAR) node->nod_arg [e_agg_stream];
	}
    else if (node->nod_type == nod_procedure)
	{
	rsb = gen_procedure (tdbb, opt, node);
	local_streams [++local_streams [0]] = (UCHAR) node->nod_arg [e_prc_stream];
	}
    else if (node->nod_type == nod_rse)
	{
	compute_rse_streams (csb, (RSE) node, beds);
	compute_rse_streams (csb, (RSE) node, local_streams);
	compute_dbkey_streams (csb, node, key_streams);
	*stack_end = parent_stack;
	rsb = OPT_compile (tdbb, csb, (RSE) node, conjunct_stack);
	*stack_end = NULL;
	}

    /* if an rsb has been generated, we have a non-relation;
       so it forms a river of its own since it is separately 
       optimized from the streams in this rsb */

    if (rsb)
	{
	i = local_streams [0];
	river = (RIV) ALLOCDV (type_riv, i);
	river->riv_count = (UCHAR) i;
	river->riv_rsb = rsb;
	MOVE_FAST (local_streams + 1, river->riv_streams, i);
	set_made_river (opt, river);
	set_inactive (opt, river);
	LLS_PUSH (river, &rivers_stack);
	continue;
	}

    /* we have found a base relation; record its stream 
       number in the streams array as a candidate for 
       merging into a river */

    ++streams [0];
    *p++ = (UCHAR) stream;

    /* if we have seen any booleans or sort fields, we may be able to
       use an index to optimize them; retrieve the current format of 
       all indices at this time so we can determine if it's possible */

    csb->csb_rpt [stream].csb_idx_allocation = NULL_PTR;
    if (conjunct_count || sort || project || aggregate)
	{
	if ((relation = (REL) node->nod_arg [e_rel_relation]) && !relation->rel_file)
	    {
	    csb->csb_rpt [stream].csb_indices = BTR_all (tdbb, relation, 
			&idx, 
			&csb->csb_rpt[stream].csb_idx,
			&csb->csb_rpt[stream].csb_idx_allocation, 
			&idx_size);

	    mark_indices (&csb->csb_rpt [stream], relation->rel_id);
	    }
	else
	    csb->csb_rpt [stream].csb_indices = 0;
	}
    }
  
/* this is an attempt to make sure we have a large enough cache to 
   efficiently retrieve this query; make sure the cache has a minimum
   number of pages for each stream in the RSE (the number is just a guess) */

if (streams [0] > 5)
    CCH_expand (tdbb, (ULONG) (streams [0] * CACHE_PAGES_PER_STREAM));

/* At this point we are ready to start optimizing.  
   We will use the opt block to hold information of
   a global nature, meaning that it needs to stick 
   around for the rest of the optimization process. */

/* first fill out the conjuncts at the end of opt */

opt->opt_count = (SSHORT) conjunct_count;

/* Check if size of optimizer block exceeded. */

if (opt->opt_count > MAX_CONJUNCTS)
    ERR_post (isc_optimizer_blk_exc, 0);
    /* Msg442: size of optimizer block exceeded */

for (i = 0; i < conjunct_count; i++)
    {
    opt->opt_rpt [i].opt_conjunct = node = (NOD) LLS_POP (&conjunct_stack);
    compute_dependencies (node, opt->opt_rpt [i].opt_dependencies);
    }

/* Store the conjuncts from the parent rse.  But don't fiddle with
   the parent's stack itself. */

for (; parent_stack && conjunct_count < MAX_CONJUNCTS
     ; parent_stack = parent_stack->lls_next, conjunct_count++)
    {
    opt->opt_rpt [conjunct_count].opt_conjunct = node = (NOD) parent_stack->lls_object;
    compute_dependencies (node, opt->opt_rpt [conjunct_count].opt_dependencies);
    }

opt->opt_parent_count = (SSHORT) conjunct_count;

/* Check if size of optimizer block exceeded. */

if (parent_stack)
    ERR_post (isc_optimizer_blk_exc, 0);
    /* Msg442: size of optimizer block exceeded */

/* attempt to optimize aggregates via an index, if possible */

if (aggregate && !sort && !project)
    sort = aggregate;
else 
    rse->rse_aggregate = aggregate = NULL;

/* outer joins require some extra processing */

if (rse->rse_jointype != blr_inner)
    rsb = gen_outer (tdbb, opt, rse, rivers_stack, &sort, &project);
else
    {
    /* attempt to form joins in decreasing order of desirability */

    gen_join (tdbb, opt, streams, &rivers_stack, &sort, &project, rse->rse_plan);

    /* If there are multiple rivers, try some sort/merging */

    while (rivers_stack->lls_next && gen_sort_merge (tdbb, opt, &rivers_stack))
	;

    rsb = make_cross (tdbb, opt, rivers_stack);

    /* Pick up any residual boolean that may have fallen thru the cracks */

    rsb = gen_residual_boolean (tdbb, opt, rsb);
    }

/* if the aggregate was not optimized via an index, get rid of the 
   sort and flag the fact to the calling routine */

if (aggregate && sort)
    {
    rse->rse_aggregate = NULL;
    sort = NULL;
    }

/* check index usage in all the base streams to ensure
   that any user-specified access plan is followed */

for (i = 1; i <= streams [0]; i++)
    check_indices (&csb->csb_rpt [streams [i]]);

if (project || sort)
    {
    /* Eliminate any duplicate dbkey streams */

    b_end = beds + beds [0];
    k_end = key_streams + key_streams [0];
    for (p = k = &key_streams [1]; p <= k_end; p++)
	{
	for (q = &beds [1]; q <= b_end && *q != *p; q++)
	    ;
	if (q > b_end)
	    *k++ = *p;
	}
    key_streams [0] = k - &key_streams [1];

    /* Handle project clause, if present. */

    if (project)
	rsb = gen_sort (tdbb, opt, beds, key_streams, rsb, project, TRUE);

    /* Handle sort clause if present */

    if (sort)
	rsb = gen_sort (tdbb, opt, beds, key_streams, rsb, sort, FALSE);
    }

/* If there's a FIRST n clause, handle it */

if (rse->rse_first)
    rsb = gen_first (tdbb, opt, rsb, rse->rse_first);

/* release memory allocated for index descriptions */

for (i = 0; i < streams [0]; i++)
    {
    stream = streams [i+1];
    if (csb->csb_rpt [stream].csb_idx_allocation)
	ALL_RELEASE (csb->csb_rpt [stream].csb_idx_allocation);
    csb->csb_rpt [stream].csb_idx_allocation = NULL_PTR;
    }

DEBUG

/* free up memory for optimizer structures */

#ifdef STACK_REDUCTION

FREE_LIB_MEMORY (local_streams);
FREE_LIB_MEMORY (key_streams);
FREE_LIB_MEMORY (beds);
FREE_LIB_MEMORY (streams);
FREE_LIB_MEMORY (opt);

#else

ALL_RELEASE (opt);

#endif

#ifdef OPT_DEBUG
if (opt_debug_file)
	{
    ib_fflush (opt_debug_file);
    ib_fclose (opt_debug_file);
    opt_debug_file = NULL_PTR;
	}
#endif

tdbb->tdbb_setjmp = (UCHAR*) old_env;
return rsb;
}

NOD OPT_make_dbkey (
    register OPT	opt,
    NOD			boolean,
    USHORT		stream)
{
/**************************************
 *
 *	O P T _ m a k e _ d b k e y
 *
 **************************************
 *
 * Functional description
 *	If boolean is an equality comparison on the proper dbkey,
 *	make a "bit_dbkey" operator (makes bitmap out of dbkey
 *	expression.
 *
 *	This is a little hairy, since view dbkeys are expressed as
 *	concatenations of primitive dbkeys.
 *
 **************************************/
CSB	csb;
NOD	value, dbkey;
SLONG	n;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;


DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (boolean, type_nod);

/* If this isn't an equality, it isn't even interesting */

if (boolean->nod_type != nod_eql)
    return NULL;

/* Find the side of the equality that is potentially a dbkey.  If
   neither, make the obvious deduction */

dbkey = boolean->nod_arg [0];
value = boolean->nod_arg [1];
n = 0;

if (dbkey->nod_type != nod_dbkey &&
    dbkey->nod_type != nod_concatenate)
    {
    if (value->nod_type != nod_dbkey &&
	value->nod_type != nod_concatenate)
	return NULL;
    dbkey = value;
    value = boolean->nod_arg [0];
    }

/* If the value isn't computable, this has been a waste of time */

csb = opt->opt_csb;
if (!computable (csb, value, stream, FALSE))
    return NULL;

/* If this is a concatenation, find an appropriate dbkey */

if (dbkey->nod_type == nod_concatenate)
    {
    dbkey = find_dbkey (dbkey, stream, &n);
    if (!dbkey)
	return NULL;
    }

/* Make sure we have the correct stream */

if ((USHORT) dbkey->nod_arg [0] != stream)
    return NULL;

/* If this is a dbkey for the appropriate stream, it's invertable */

dbkey = PAR_make_node (tdbb, 2);
dbkey->nod_count = 1;
dbkey->nod_type = nod_bit_dbkey;
dbkey->nod_arg [0] = value;
dbkey->nod_arg [1] = (NOD) n;
dbkey->nod_impure = CMP_impure (csb, sizeof (struct inv));

return dbkey;
}

NOD OPT_make_index (
    TDBB	tdbb,
    OPT		opt,
    REL		relation,
    IDX		*idx)
{
/**************************************
 *
 *	O P T _ m a k e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Build node for index scan.
 *
 **************************************/
IRB	retrieval;
NOD	node, *lower, *upper, *end_node;
struct opt_repeat *tail, *end;

SET_TDBB (tdbb);

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (relation, type_rel);

/* Allocate both a index retrieval node and block. */

node = make_index_node (tdbb, relation, opt->opt_csb, idx);
retrieval = (IRB) node->nod_arg [e_idx_retrieval];
retrieval->irb_relation = relation;

/* Pick up lower bound segment values */

lower = retrieval->irb_value;
upper = retrieval->irb_value + idx->idx_count;
end = opt->opt_rpt + idx->idx_count;

if (idx->idx_flags & idx_descending)
    {
    for (tail = opt->opt_rpt; tail->opt_lower && tail < end; tail++)
	*upper++ = tail->opt_lower;
    for (tail = opt->opt_rpt; tail->opt_upper && tail < end; tail++)
	*lower++ = tail->opt_upper;
    retrieval->irb_generic |= irb_descending;
    }
else
    {
    for (tail = opt->opt_rpt; tail->opt_lower && tail < end; tail++)
	*lower++ = tail->opt_lower;
    for (tail = opt->opt_rpt; tail->opt_upper && tail < end; tail++)
	*upper++ = tail->opt_upper;
    }

retrieval->irb_lower_count = lower - retrieval->irb_value;
retrieval->irb_upper_count = (upper - retrieval->irb_value) - idx->idx_count;

#ifdef IGNORE_NULL_IDX_KEY
/* when lower bound is given, and upper bound is not,
  do not look at index keys which have <null> value in their first
  segment. This is because these records will not satisfy the query
  since <null> is a state and not a value, and thus cannot be compared
  to a value. 
*/
if (retrieval->irb_lower_count && retrieval->irb_upper_count == 0)
    {
    retrieval->irb_generic |= irb_ignore_null_value_key;
    }
#endif  /* IGNORE_NULL_IDX_KEY */

/* Check to see if this is really an equality retrieval */

if (retrieval->irb_lower_count == retrieval->irb_upper_count)
    {
    retrieval->irb_generic |= irb_equality;
    lower = retrieval->irb_value;
    upper = retrieval->irb_value + idx->idx_count;
    for (end_node = lower + retrieval->irb_lower_count; lower < end_node;)
	if (*upper++ != *lower++)
	    {
	    retrieval->irb_generic &= ~irb_equality;
	    break;
	    }
    }

/* If we are matching less than the full index, this is a partial match */

if (retrieval->irb_upper_count < idx->idx_count)
    retrieval->irb_generic |= irb_partial;

/* mark the index as utilized for the purposes of this compile */

idx->idx_runtime_flags |= idx_used;

return node;
}

int OPT_match_index (
    OPT		opt,
    USHORT	stream,
    IDX		*idx)
{
/**************************************
 *
 *	O P T _ m a t c h _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Match any active (computable but not consumed) boolean
 *	conjunctions against a given index.  This is used by
 *	the external relation modules to do index optimization.
 *	Return the number of matching items.
 *
 **************************************/
TDBB		tdbb;
register CSB	csb;
NOD		node;
USHORT		n;
register struct opt_repeat	*tail, *opt_end;

tdbb = GET_THREAD_DATA;
DEV_BLKCHK (opt, type_opt);

/* If there are not conjunctions, don't waste our time */

if (!opt->opt_count)
    return 0;

csb = opt->opt_csb;
opt_end = opt->opt_rpt + opt->opt_count;
n = 0;
clear_bounds (opt, idx);

for (tail = opt->opt_rpt; tail < opt_end; tail++)
    {
    node = tail->opt_conjunct;
    if (!(tail->opt_flags & opt_used) && computable (csb, node, -1, TRUE))
	n += match_index (tdbb, opt, stream, node, idx);
    }

return n;
}


void OPT_set_index (
    TDBB	tdbb,
    REQ		request,
    RSB		*rsb_ptr,
    REL		relation,
    IDX		*idx)
{
/**************************************
 *
 *	O P T _ s e t _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Reset the navigational order of a stream.
 *	Given a sequential or navigational rsb,
 *	reset the rsb to be a navigational rsb
 *	on the specified index.
 *
 **************************************/
RSB		old_rsb, new_rsb; 
USHORT		index_id, i;
OPT		opt;
NOD		inversion = NULL;
NOD		index_node, new_index_node;
IRB		retrieval;
IDL		index;
VEC		vector;
DBB		dbb;

DEV_BLKCHK (request, type_req);
DEV_BLKCHK (*rsb_ptr, type_rsb);
DEV_BLKCHK (relation, type_rel);

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

old_rsb = *rsb_ptr;

/* fix up a boolean rsb to point to the actual stream rsb */

if (old_rsb->rsb_type == rsb_boolean)
    {
    rsb_ptr = &old_rsb->rsb_next;
    old_rsb = old_rsb->rsb_next;
    }
                                                   
/* check if there is an existing inversion for
   a boolean expression mapped to an index */

if (old_rsb->rsb_type == rsb_indexed)
    inversion = (NOD) old_rsb->rsb_arg [0];
else if (old_rsb->rsb_type == rsb_navigate)
    inversion = (NOD) old_rsb->rsb_arg [RSB_NAV_inversion];

/* set up a dummy optimizer block just for the purposes 
   of the set index, to pass information to subroutines */

opt = (OPT) ALLOCP (type_opt);
opt->opt_g_flags |= opt_g_stream;

/* generate a new rsb for the retrieval, making sure to 
   preserve the inversion generated for the last rsb; note
   that if the bitmap for the inversion has already been
   generated, it will be reused since it is already part of
   the impure area--I can't think of any reason not to reuse it--deej */

if (idx)
    {
    new_rsb = gen_nav_rsb (tdbb, opt, old_rsb->rsb_stream, relation, NULL_PTR, idx
#ifdef SCROLLABLE_CURSORS
    , RSE_get_forward
#endif
    );
    new_rsb->rsb_arg [RSB_NAV_inversion] = (RSB) inversion;
    new_rsb->rsb_cardinality = old_rsb->rsb_cardinality;
    }
else
    new_rsb = gen_rsb (tdbb, opt, NULL_PTR, inversion, old_rsb->rsb_stream,
			relation, NULL_PTR, NULL_PTR, (float) old_rsb->rsb_cardinality);

/* point the impure area of the new rsb to the impure area of the
   old; since impure area is pre-allocated it would be difficult 
   to change now, so just use the same area; NOTE: this implies
   that we must take some pains to ensure that the impure area is
   always large enough to handle a maximum-key index */

new_rsb->rsb_impure = old_rsb->rsb_impure;

/* find index node if the old rsb was navigational */

if (old_rsb->rsb_type == rsb_navigate)
    index_node = (NOD) old_rsb->rsb_arg [RSB_NAV_index];
else
    index_node = NULL;

/* if the new rsb is navigational, set up impure space in request 
   for new index node; to convert from non-navigational to navigational,
   we need to adjust the impure area upwards to make room for an INV 
   structure, and vice versa to convert the other way */

if (idx)
    {
    new_index_node = (NOD) new_rsb->rsb_arg [RSB_NAV_index];
    if (old_rsb->rsb_type == rsb_navigate)
        new_index_node->nod_impure = index_node->nod_impure;
    else
        {
        new_index_node->nod_impure = old_rsb->rsb_impure;    
        new_rsb->rsb_impure += sizeof (struct inv);
        }
    }
else if (old_rsb->rsb_type == rsb_navigate)
    new_rsb->rsb_impure -= sizeof (struct inv);

/* if there was a previous index, release its lock 
   and remove its resource from the request */

if (old_rsb->rsb_type == rsb_navigate)
    {
    retrieval = (IRB) index_node->nod_arg [e_idx_retrieval];
    index_id = retrieval->irb_index;
    index = CMP_get_index_lock (tdbb, relation, index_id);
    if (!--index->idl_count)
	LCK_release (tdbb, index->idl_lock);
    CMP_release_resource (&request->req_resources, rsc_index, index_id);
    }

/* get lock on new index */

if (idx)
    {
    index = CMP_get_index_lock (tdbb, relation, idx->idx_id);
    if (!index->idl_count)
        LCK_lock_non_blocking (tdbb, index->idl_lock, LCK_SR, TRUE);
    ++index->idl_count;
    }

/* go out to the vector which stores all rsbs for the 
   request, and replace the old with the new */

vector = request->req_fors;
for (i = 0; i < vector->vec_count; i++)
    if (vector->vec_object [i] == (BLK) old_rsb)
	{     
	vector->vec_object [i] = (BLK) new_rsb;	
	break;
	}

/* release unneeded blocks */

ALL_RELEASE (opt);
if (index_node)
    ALL_RELEASE (index_node);
ALL_RELEASE (old_rsb);

*rsb_ptr = new_rsb;
}

static BOOLEAN augment_stack (
    NOD		node,
    LLS		*stack)
{
/**************************************
 *
 *	a u g m e n t _ s t a c k
 *
 **************************************
 *
 * Functional description
 *	Add node to stack unless node is already on stack.
 *
 **************************************/
LLS	temp;

DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (*stack, type_lls);

for (temp = *stack; temp; temp = temp->lls_next)
    if (node_equality (node, (NOD) temp->lls_object))
	return FALSE;

LLS_PUSH (node, stack);

return TRUE;
}

static void check_indices (
    struct csb_repeat		*csb_tail)
{
/**************************************
 *
 *	c h e c k _ i n d i c e s
 *
 **************************************
 *
 * Functional description
 *	Check to make sure that the user-specified
 *	indices were actually utilized by the optimizer.
 *         
 **************************************/
TEXT 	index_name [32];
NOD	plan, access_type;
REL 	relation;
IDX	*idx;
USHORT	i;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;

if (!(plan = csb_tail->csb_plan))
    return;
         
if (plan->nod_type != nod_retrieve)
    return;

relation = csb_tail->csb_relation;

/* if there were no indices fetched at all but the
   user specified some, error out using the first
   index specified */

if (!csb_tail->csb_indices && 
    (access_type = plan->nod_arg [e_retrieve_access_type]))

    /* index %s cannot be used in the specified plan */
    ERR_post (gds__index_unused, gds_arg_string, access_type->nod_arg [2], 0);

/* check to make sure that all indices are either used or marked not to be used,
   and that there are no unused navigational indices */

idx = csb_tail->csb_idx;
for (i = 0; i < csb_tail->csb_indices; i++)
    {
    if (!(idx->idx_runtime_flags & (idx_plan_dont_use | idx_used)) ||
	((idx->idx_runtime_flags & idx_plan_navigate) && !(idx->idx_runtime_flags & idx_navigate)))
	if (!(idx->idx_runtime_flags & (idx_plan_missing | idx_plan_starts)))
	{
        if (relation)
     	    MET_lookup_index (tdbb, index_name, relation->rel_name, (USHORT) (idx->idx_id + 1));
        else
  	    index_name [0] = 0;

        /* index %s cannot be used in the specified plan */
	ERR_post (gds__index_unused, gds_arg_string, ERR_cstring (index_name), 0);
	}

    idx = NEXT_IDX (idx->idx_rpt, idx->idx_count);
    }
}

static BOOLEAN check_relationship (
    OPT		opt,
    USHORT	position,
    USHORT	stream)
{
/**************************************
 *
 *	c h e c k _ r e l a t i o n s h i p 
 *
 **************************************
 *
 * Functional description
 *	Check for a potential indexed relationship.
 *
 **************************************/
IRL	relationship;
USHORT	n;
struct opt_repeat	*tail, *end;

DEV_BLKCHK (opt, type_opt);

for (tail = opt->opt_rpt, end = tail + position; tail < end; tail++)
    {
    n = tail->opt_stream;
    for (relationship = opt->opt_rpt[n].opt_relationships; relationship;
	 relationship = relationship->irl_next)
	if (stream == relationship->irl_stream)
	    return TRUE;
    }

return FALSE;
}

static void check_sorts (
    RSE 	rse)
{
/**************************************
 *
 *	c h e c k _ s o r t s
 *
 **************************************
 *
 * Functional description
 *	Try to optimize out unnecessary sorting. 
 *
 **************************************/  
NOD	sort, project, group, sub_rse; 
NOD	*group_ptr, *project_ptr, *sort_ptr;
NOD	*group_end, *project_end, *sort_end;

DEV_BLKCHK (rse, type_nod);

sort = rse->rse_sorted;
project = rse->rse_projection;

/* check if a GROUP BY exists using the same fields as the project or sort:
   if so, the projection can be eliminated; if no projection exists, then 
   the sort can be eliminated */

if ((project || sort) &&
    (rse->rse_count == 1) && 
    (sub_rse = rse->rse_relation [0]) &&
    (sub_rse->nod_type == nod_aggregate) &&
    (group = sub_rse->nod_arg [e_agg_group]))
    {
    /* if all the fields of the project are the same as all the fields 
       of the group by, get rid of the project */

    if (project && (project->nod_count == group->nod_count))
        {
        for (project_ptr = project->nod_arg, project_end = project_ptr + project->nod_count; project_ptr < project_end; project_ptr++)
	    {
	    for (group_ptr = group->nod_arg, group_end = group_ptr + group->nod_count; group_ptr < group_end; group_ptr++)
	        if (map_equal (*group_ptr, *project_ptr, sub_rse->nod_arg [e_agg_map]))
	            break;
	    
	    if (group_ptr == group_end)
	    	break;
	    }

        /* we can now ignore the project, but in case the project is being done 
	   in descending order because of an order by, do the group by the same way */

        if (project_ptr == project_end)
            {
	    set_direction (project, group);
            project = rse->rse_projection = NULL;
	    }
	}

    /* if there is no projection, then we can make a similar optimization 
       for sort, except that sort may have fewer fields than group by */

    if (!project && sort && (sort->nod_count <= group->nod_count))
        {
        for (sort_ptr = sort->nod_arg, sort_end = sort_ptr + sort->nod_count; sort_ptr < sort_end; sort_ptr++)
	    {
	    for (group_ptr = group->nod_arg, group_end = group_ptr + sort->nod_count; group_ptr < group_end; group_ptr++)
	        if (map_equal (*group_ptr, *sort_ptr, sub_rse->nod_arg [e_agg_map]))
	            break;
	    if (group_ptr == group_end)
	        break;
	    }

        /* if all the fields in the sort list match the first n fields in the 
           project list, we can ignore the sort, but update the sort order 
           (ascending/descending) to match that in the sort list */

        if (sort_ptr == sort_end)
            {
	    set_direction (sort, group);
	    set_position (sort, group, sub_rse->nod_arg [e_agg_map]);
            sort = rse->rse_sorted = NULL;
	    }
	}

    } 

/* examine the ORDER BY and DISTINCT clauses; if all the fields in the 
   ORDER BY match the first n fields in the DISTINCT in any order, the 
   ORDER BY can be removed, changing the fields in the DISTINCT to match 
   the ordering of fields in the ORDER BY */

if (sort && project && (sort->nod_count <= project->nod_count))
    {
    for (sort_ptr = sort->nod_arg, sort_end = sort_ptr + sort->nod_count; sort_ptr < sort_end; sort_ptr++)
	{
        for (project_ptr = project->nod_arg, project_end = project_ptr + sort->nod_count; project_ptr < project_end; project_ptr++)
	    if ((*sort_ptr)->nod_type == nod_field &&
	        (*project_ptr)->nod_type == nod_field && 
	        (*sort_ptr)->nod_arg [e_fld_stream] == (*project_ptr)->nod_arg [e_fld_stream] &&
	        (*sort_ptr)->nod_arg [e_fld_id] == (*project_ptr)->nod_arg [e_fld_id])
	        break;
	
	if (project_ptr == project_end)
	    break;
	}

    /* if all the fields in the sort list match the first n fields in the 
       project list, we can ignore the sort, but update the project to match the sort */

    if (sort_ptr == sort_end)
        {
	set_direction (sort, project);
	set_position (sort, project, NULL);
        sort = rse->rse_sorted = NULL;
	}
    }
}

static void class_mask (
    USHORT	count,
    NOD		*class,
    ULONG	*mask)
{
/**************************************
 *
 *	c l a s s _ m a s k
 *
 **************************************
 *
 * Functional description
 *	Given an sort/merge join equivalence class (vector of node pointers
 *	of representative values for rivers), return a bit mask of rivers
 *	with values.
 *
 **************************************/
SLONG	i;
              
#ifdef DEV_BUILD
if (*class)
    DEV_BLKCHK (*class, type_nod);
#endif

if (count > MAX_CONJUNCTS)
    ERR_post (isc_optimizer_blk_exc, 0);
    /* Msg442: size of optimizer block exceeded */

for (i = 0; i < OPT_BITS; i++)
    mask[i] = 0;

for (i = 0; i < count; i++, class++)
    if (*class)
	{
	SET_DEP_BIT(mask, i);
	DEV_BLKCHK (*class, type_nod);
	}
}

static void clear_bounds (
    OPT		opt,
    IDX		*idx)
{
/**************************************
 *
 *	c l e a r _ b o u n d s
 *
 **************************************
 *
 * Functional description
 *	Clear upper and lower value slots before matching booleans to
 *	indices.
 *
 **************************************/
register struct opt_repeat	*tail, *opt_end;

DEV_BLKCHK (opt, type_opt);

opt_end = &opt->opt_rpt [idx->idx_count];

for (tail = opt->opt_rpt; tail < opt_end; tail++)
    {
    tail->opt_lower = NULL;
    tail->opt_upper = NULL;
    tail->opt_match = NULL;
    }
}

static NOD compose (
    NOD		*node1,
    NOD		node2,
    NOD_T	node_type)
{
/**************************************
 *
 *	c o m p o s e
 *
 **************************************
 *
 * Functional description
 *	Build and AND out of two conjuncts.
 *
 **************************************/

DEV_BLKCHK (*node1, type_nod);
DEV_BLKCHK (node2, type_nod);

if (!node2)
    return *node1;

if (!*node1)
    return (*node1 = node2);

return *node1 = make_binary_node (node_type, *node1, node2, FALSE);
}

static BOOLEAN computable (
    CSB			csb,
    register NOD	node,
    SSHORT		stream,
    BOOLEAN		idx_use)
{
/**************************************
 *
 *	c o m p u t a b l e
 *
 **************************************
 *
 * Functional description
 *	See if node is presently computable.  
 *	Note that a field is not computable
 *	with respect to its own stream.
 *
 * There are two different uses of computable(). 
 * (a) idx_use == FALSE: when an unused conjunct is to be picked for making
 *     into a boolean and in making a db_key. 
 *     In this case, a node is said to be computable, if all the streams 
 *     involved in that node are csb_active. The csb_active flag 
 *     defines all the streams available in the current scope of the
 *     query.
 * (b) idx_use == TRUE: to determine if we can use an
 *     index on the conjunct we have already chosen.
 *     In order to use an index on a conjunct, it is required that the
 *     all the streams involved in the conjunct are currently active
 *     or have been already processed before and made into rivers.
 *     Because, here we want to differentiate between streams we have
 *     not yet worked on and those that we have worked on or are currently
 *     working on.
 *
 **************************************/
RSE	rse;
NOD	*ptr, *end, sub, value;
USHORT	n;
BOOLEAN	result;

DEV_BLKCHK (csb, type_csb);
DEV_BLKCHK (node, type_nod);

/* Recurse thru interesting sub-nodes */

ptr = node->nod_arg;

if (node->nod_type == nod_procedure)
    return FALSE;

for (end = ptr + node->nod_count; ptr < end; ptr++)
    if (!computable (csb, *ptr, stream, idx_use))
	return FALSE;

switch (node->nod_type)
    {
    case nod_field:
	if ((n = (USHORT) node->nod_arg [e_fld_stream]) == stream)
	    return FALSE;
	if (idx_use && !(csb->csb_rpt [n].csb_flags & (csb_made_river | csb_active))) 
	    return FALSE;
	if (!idx_use && !(csb->csb_rpt [n].csb_flags & csb_active)) 	   
	   return FALSE;
	return TRUE;

    case nod_dbkey:
	if ((n = (USHORT) node->nod_arg [0]) == stream) 
	    return FALSE;
	if (idx_use  && !(csb->csb_rpt [n].csb_flags & (csb_made_river|csb_active))) 
	    return FALSE;
	if (!idx_use && !(csb->csb_rpt [n].csb_flags & csb_active)) 
	    return FALSE;
	return TRUE;

    case nod_min:
    case nod_max:
    case nod_average:
    case nod_total:
    case nod_count:
    case nod_from:
	if ((sub = node->nod_arg [e_stat_default]) &&
	    !computable (csb, sub, stream, idx_use))
	    return FALSE;
	rse = (RSE) node->nod_arg [e_stat_rse];
	value = node->nod_arg [e_stat_value];
	break;

    case nod_rse:
	rse = (RSE) node;
	value = NULL;
	break;
    
    case nod_aggregate:
	rse = (RSE) node->nod_arg [e_agg_rse];
	rse->rse_sorted = node->nod_arg [e_agg_group];
	value = NULL;
	break;

    default:
	return TRUE;
    }

/* Node is a record selection expression. */

result = TRUE;

if ((sub = rse->rse_first) && !computable (csb, sub, stream, idx_use))
    return FALSE;
    
/* Set sub-streams of rse active */

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    if ((*ptr)->nod_type != nod_rse)
        {
        n = (USHORT) (*ptr)->nod_arg [STREAM_INDEX((*ptr))];
        csb->csb_rpt [n].csb_flags |= csb_active;
        }

/* Check sub-stream */

if (((sub = rse->rse_boolean) && !computable (csb, sub, stream, idx_use)) ||
    ((sub = rse->rse_sorted) && !computable (csb, sub, stream, idx_use)) ||
    ((sub = rse->rse_projection) && !computable (csb, sub, stream, idx_use)))
    result = FALSE;

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; 
				((ptr < end) && (result)); ptr++)
    if ((*ptr)->nod_type != nod_rse)
	{
	if (!computable (csb, (*ptr), stream, idx_use))
	    result = FALSE;
	}

/* Check value expression, if any */

if (result && value && !computable (csb, value, stream, idx_use))
    result = FALSE;

/* Reset streams inactive */

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    if ((*ptr)->nod_type != nod_rse)
        {
        n = (USHORT) (*ptr)->nod_arg [STREAM_INDEX((*ptr))];
        csb->csb_rpt [n].csb_flags &= ~csb_active;
        }

return result;
}

static void compute_dependencies (
    NOD		node,
    ULONG	*dependencies)
{
/**************************************
 *
 *	c o m p u t e _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *	Compute stream dependencies for evaluation of an expression.
 *
 **************************************/
RSE	rse;
NOD	*ptr, *end, sub, value;
USHORT	n;

DEV_BLKCHK (node, type_nod);

/* Recurse thru interesting sub-nodes */

ptr = node->nod_arg;

if (node->nod_type == nod_procedure)
    return;

for (end = ptr + node->nod_count; ptr < end; ptr++)
    compute_dependencies (*ptr, dependencies);

switch (node->nod_type)
    {
    case nod_field:
	n = (USHORT) node->nod_arg [e_fld_stream];
	SET_DEP_BIT (dependencies, n);
	return;

    case nod_dbkey:
	n = (USHORT) node->nod_arg [0];
	SET_DEP_BIT (dependencies, n);
	return;

    case nod_min:
    case nod_max:
    case nod_average:
    case nod_total:
    case nod_count:
    case nod_from:
	if (sub = node->nod_arg [e_stat_default])
	    compute_dependencies (sub, dependencies);
	rse = (RSE) node->nod_arg [e_stat_rse];
	value = node->nod_arg [e_stat_value];
	break;

    case nod_rse:
	rse = (RSE) node;
	value = NULL;
	break;
    
    default:
	return;
    }

/* Node is a record selection expression.  Groan.  Ugh.  Yuck. */

if (sub = rse->rse_first)
    compute_dependencies (sub, dependencies);
    
/* Check sub-expressions */

if (sub = rse->rse_boolean)
    compute_dependencies (sub, dependencies);
    
if (sub = rse->rse_sorted)
    compute_dependencies (sub, dependencies);
    
if (sub = rse->rse_projection)
    compute_dependencies (sub, dependencies);
    
/* Check value expression, if any */

if (value)
    compute_dependencies (value, dependencies);

/* Reset streams inactive */

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    if ((*ptr)->nod_type != nod_rse)
        {
        n = (USHORT) (*ptr)->nod_arg [STREAM_INDEX((*ptr))];
        CLEAR_DEP_BIT (dependencies, n);
        }
}

static void compute_dbkey_streams (
    CSB		csb,
    NOD		node,
    UCHAR	*streams)
{
/**************************************
 *
 *	c o m p u t e _ d b k e y _ s t r e a m s
 *
 **************************************
 *
 * Functional description
 *	Identify all of the streams for which a
 *	dbkey may need to be carried through a sort.
 *
 **************************************/
NOD	clauses, *ptr, *end;
RSE	rse;

DEV_BLKCHK (csb, type_csb);
DEV_BLKCHK (node, type_nod);

if (node->nod_type == nod_relation)
    streams [++streams [0]] = (UCHAR) node->nod_arg [e_rel_stream];
else if (node->nod_type == nod_union)
    {
    clauses = node->nod_arg [e_uni_clauses];
    if (clauses->nod_type != nod_procedure)
	for (ptr = clauses->nod_arg, end = ptr + clauses->nod_count; 
						 ptr < end; ptr += 2)
	    compute_dbkey_streams (csb, *ptr, streams);
    }
else if (node->nod_type == nod_rse)
    {
    rse = (RSE) node;
    for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
	compute_dbkey_streams (csb, *ptr, streams);
    }
}

static void compute_rse_streams (
    CSB		csb,
    RSE		rse,
    UCHAR	*streams)
{
/**************************************
 *
 *	c o m p u t e _ r s e _ s t r e a m s
 *
 **************************************
 *
 * Functional description
 *	Identify the streams that make up an rse.
 *
 **************************************/
NOD	*ptr, *end, node;

DEV_BLKCHK (csb, type_csb);
DEV_BLKCHK (rse, type_nod);

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    {
    node = *ptr;
    if (node->nod_type != nod_rse)
	streams [++streams [0]] = (UCHAR) node->nod_arg [STREAM_INDEX(node)];
    else
	compute_rse_streams (csb, (RSE) node, streams);
    }
}

static SLONG decompose (
    TDBB		tdbb,
    register NOD	boolean_node,
    LLS			*stack,
    CSB			csb)
{
/**************************************
 *
 *	d e c o m p o s e
 *
 **************************************
 *
 * Functional description
 *	Decompose a boolean into a stack of conjuctions.
 *
 **************************************/
NOD	arg, node;

DEV_BLKCHK (boolean_node, type_nod);
DEV_BLKCHK (*stack, type_lls);
DEV_BLKCHK (csb, type_csb);

if (boolean_node->nod_type == nod_and)
    return decompose (tdbb, boolean_node->nod_arg [0], stack, csb) +
	   decompose (tdbb, boolean_node->nod_arg [1], stack, csb);

/* turn a between into (a greater than or equal) AND (a less than  or equal) */

if (boolean_node->nod_type == nod_between)
    {
    arg = boolean_node->nod_arg [0];
    if (arg->nod_type == nod_from)
	{
	/* Without this ERR_punt(), server was crashing with sub queries 
	 * under "between" predicate, Bug No. 73766 */
        ERR_post (isc_optimizer_between_err, 0);
	/* Msg 493: Unsupported field type specified in BETWEEN predicate */
	}
    node = make_binary_node (nod_geq, arg, boolean_node->nod_arg [1], TRUE);
    LLS_PUSH (node, stack);
    arg = CMP_clone_node (tdbb, csb, arg);
    node = make_binary_node (nod_leq, arg, boolean_node->nod_arg [2], TRUE);
    LLS_PUSH (node, stack);
    return 2;
    }

/* turn a LIKE into a LIKE and a STARTING WITH, if it starts
   with anything other than a pattern-matching character */

if ((boolean_node->nod_type == nod_like) &&
    (arg = optimize_like (tdbb, boolean_node)))
    {
    node = make_binary_node (nod_starts, boolean_node->nod_arg [0], arg, FALSE);
    LLS_PUSH (node, stack);
    LLS_PUSH (boolean_node, stack);
    return 2;
    }

LLS_PUSH (boolean_node, stack);

return 1;
}

static USHORT distribute_equalities (
    LLS		*org_stack,
    CSB		csb)
{
/**************************************
 *
 *	d i s t r i b u t e _ e q u a l i t i e s
 *
 **************************************
 *
 * Functional description
 *	Given a stack of conjunctions, generate some simple
 *	inferences.  In general, find classes of equalities,
 *	then find operations based on members of those classes.
 *	If we find any, generate additional conjunctions.  In
 *	SSHORT:
 *
 *		If (a == b) and (a $ c) --> (b $ c) for any
 *		operation '$'.
 *
 **************************************/
LLS	classes [MAX_OPT_ITEMS], *class, *class2, *end, stack, temp;
NOD	boolean, node1, node2, new_node, arg1, arg2;
USHORT	reverse, count, n;

DEV_BLKCHK (*org_stack, type_lls);
DEV_BLKCHK (csb, type_csb);

/* Zip thru stack of booleans looking for field equalities */

end = classes;

for (stack = *org_stack; stack; stack = stack->lls_next)
    {
    boolean = (NOD) stack->lls_object;
    if (boolean->nod_type != nod_eql)
	continue;
    node1 = boolean->nod_arg [0];
    if (node1->nod_type != nod_field)
	continue;
    node2 = boolean->nod_arg [1];
    if (node2->nod_type != nod_field)
	continue;
    for (class = classes; class < end; class++)
	if (search_stack (node1, *class))
	    {
	    augment_stack (node2, class);
	    break;
	    }
	else if (search_stack (node2, *class))
	    {
	    LLS_PUSH (node1, class);
	    break;
	    }
    if (class == end)
	{
	*class = NULL;
	++end;
	LLS_PUSH (node1, class);
	LLS_PUSH (node2, class);
	}
    }

if (end == classes)
    return 0;

/* Make another pass looking for any equality relationships that may
   have crept in between classes (this could result from the
   sequence (A = B, C = D, B = C) */

for (class = classes; class < end; class++)
    for (stack = *class; stack; stack = stack->lls_next)
	for (class2 = class + 1; class2 < end; class2++)
	    if (search_stack ((NOD) stack->lls_object, *class2))
		{
		DEBUG;
		while (*class2)
		    augment_stack ((NOD) LLS_POP (class2), class);
		}

count = 0;

/* Start by making a pass distributing field equalities */

for (class = classes; class < end; class++)
    {
    for (stack = *class, n = 0; stack; stack = stack->lls_next)
	n++;
    if (n >= 3)
	for (stack = *class; stack; stack = stack->lls_next)
	    for (temp = stack->lls_next; temp; temp = temp->lls_next)
		{
		boolean = make_binary_node (nod_eql, (NOD) stack->lls_object, (NOD) temp->lls_object, TRUE);
		if (augment_stack (boolean, org_stack))
		    {
		    DEBUG;
		    count++;
		    }
		else
		    ALL_RELEASE (boolean);
		}
    }
	
/* Now make a second pass looking for non-field equalities */

for (stack = *org_stack; stack; stack = stack->lls_next)
    {
    boolean = (NOD) stack->lls_object;
    if (boolean->nod_type != nod_eql &&
	boolean->nod_type != nod_gtr &&
	boolean->nod_type != nod_geq &&
	boolean->nod_type != nod_leq &&
	boolean->nod_type != nod_lss &&
	boolean->nod_type != nod_matches &&
	boolean->nod_type != nod_contains &&
	boolean->nod_type != nod_like)
	continue;
    node1 = boolean->nod_arg [0];
    node2 = boolean->nod_arg [1];
    reverse = FALSE;
    if (node1->nod_type != nod_field)
	{
	new_node = node1;
	node1 = node2;
	node2 = new_node;
	reverse = TRUE;
	}
    if (node1->nod_type != nod_field)
	continue;
    if (node2->nod_type != nod_literal &&
	node2->nod_type != nod_variable &&
	node2->nod_type != nod_argument)
	continue;
    for (class = classes; class < end; class++)
	if (search_stack (node1, *class))
	    {
	    for (temp = *class; temp; temp = temp->lls_next)
		if (!node_equality (node1, (NOD) temp->lls_object))
		    {
		    if (reverse)
			{
			arg1 = boolean->nod_arg [0];
			arg2 = (NOD) temp->lls_object;
			}
		    else
			{
			arg1 = (NOD) temp->lls_object;
			arg2 = boolean->nod_arg [1];
			}

		    /* From the conjuncts X(A,B) and A=C, infer the
		     * conjunct X(C,B)
		     */
		    new_node = make_inference_node (csb, boolean, arg1, arg2);
		    if (augment_stack (new_node, org_stack))
			count++;
		    }
	    break;
	    }
    }

return count;
}

static BOOLEAN dump_index (
    NOD 	node,
    SCHAR	**buffer_ptr,
    SSHORT	*buffer_length)
{
/**************************************
 *
 *	d u m p _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Dump an index inversion tree to 
 *	an info buffer.
 *
 **************************************/
SCHAR	*buffer;
IRB	retrieval;
TEXT	index_name [32], *i;
SSHORT	length;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;

DEV_BLKCHK (node, type_nod);

buffer = *buffer_ptr;

if (--(*buffer_length) < 0)
    return FALSE;

/* spit out the node type */

if (node->nod_type == nod_bit_and)
    *buffer++ = gds__info_rsb_and;
else if (node->nod_type == nod_bit_or)
    *buffer++ = gds__info_rsb_or;
else if (node->nod_type == nod_bit_dbkey)
    *buffer++ = gds__info_rsb_dbkey;
else if (node->nod_type == nod_index)
    *buffer++ = gds__info_rsb_index;

/* dump sub-nodes or the actual index info */

if ((node->nod_type == nod_bit_and) ||
    (node->nod_type == nod_bit_or))
    {
    if (!dump_index (node->nod_arg [0], &buffer, buffer_length))
	return FALSE;
    if (!dump_index (node->nod_arg [1], &buffer, buffer_length))
	return FALSE;
    }
else if (node->nod_type == nod_index)
    {
    retrieval = (IRB) node->nod_arg [e_idx_retrieval];
    MET_lookup_index (tdbb, index_name, retrieval->irb_relation->rel_name, (USHORT) (retrieval->irb_index + 1));
    length = strlen (index_name);

    if ((*buffer_length -= (length + 1)) >= 0)
	{
	*buffer++ = (SCHAR) length;
	i = index_name;
	while (length--)
	    *buffer++ = *i++;
	}
    }

*buffer_ptr = buffer;

return TRUE;
}

static BOOLEAN dump_rsb (
    REQ		request, 
    RSB 	rsb,
    SCHAR	**buffer_ptr,
    SSHORT	*buffer_length)
{
/**************************************
 *
 *	d u m p _ r s b
 *
 **************************************
 *
 * Functional description
 *	Returns a formatted access path for
 *	a particular rsb.
 *
 **************************************/
USHORT	length;
REL	relation;
STR	alias;
SCHAR	*name, *buffer;
RSB	*ptr, *end;
SSHORT	return_length;
PRC	procedure;

DEV_BLKCHK (rsb, type_rsb);

buffer = *buffer_ptr;

/* leave room for the rsb begin, type, and end */

if ((*buffer_length -= 4) < 0)
    return FALSE;
*buffer++ = gds__info_rsb_begin;

/* dump out the alias or relation name if it exists */
               
name = NULL;
relation = rsb->rsb_relation;

if (alias = rsb->rsb_alias)
    {                                
    length = alias->str_length;
    name = (SCHAR*) alias->str_data;
    }
else if (relation)
    {
    length = strlen (relation->rel_name);
    name = relation->rel_name;
    }

if (name)
    {
    *buffer_length -= 2 + length;
    if (*buffer_length < 0)
	return FALSE;
    *buffer++ = gds__info_rsb_relation;
    *buffer++ = (SCHAR) length;
    while (length--)
	*buffer++ = *name++;
    }

/* print out the type followed immediately by any
   type-specific data */

*buffer++ = gds__info_rsb_type;
switch (rsb->rsb_type)
    {
    case rsb_indexed:
	*buffer++ = gds__info_rsb_indexed;
	if (!dump_index ((NOD) rsb->rsb_arg [0], &buffer, buffer_length))
	    return FALSE;
	if (--(*buffer_length) < 0)
	    return FALSE;
	break;

    case rsb_navigate:
	*buffer++ = gds__info_rsb_navigate;
	if (!dump_index ((NOD) rsb->rsb_arg [RSB_NAV_index], &buffer, buffer_length))
	    return FALSE;
	if (--(*buffer_length) < 0)
	    return FALSE;
	break;

    case rsb_sequential:
	*buffer++ = gds__info_rsb_sequential;
	break;
    
    case rsb_cross:
	*buffer++ = gds__info_rsb_cross;
	break;

    case rsb_sort:
	*buffer++ = gds__info_rsb_sort;
	break;

    case rsb_procedure:
	*buffer++ = gds__info_rsb_procedure;

	/* don't try to print out plans of procedures called by procedures, since 
	   we could get into a recursive situation; if the customer wants to know 
	   the plan produced by the sub-procedure, they can invoke it directly */

	if (request->req_procedure) 
	    break;
	
	procedure = rsb->rsb_procedure;
	if (!procedure || !procedure->prc_request)
	    return FALSE;
	if (!OPT_access_path (procedure->prc_request, buffer, *buffer_length, &return_length))
	    return FALSE;
	if ((*buffer_length -= return_length) < 0)
	    return FALSE;
	buffer += return_length;
	break;
    
    case rsb_first:
	*buffer++ = gds__info_rsb_first;
	break;

    case rsb_boolean:
	*buffer++ = gds__info_rsb_boolean;
	break;

    case rsb_union:
	*buffer++ = gds__info_rsb_union;
	break;

    case rsb_aggregate:
	*buffer++ = gds__info_rsb_aggregate;
	break;

    case rsb_merge:
	*buffer++ = gds__info_rsb_merge;
	break;

    case rsb_ext_sequential:
	*buffer++ = gds__info_rsb_ext_sequential;
	break;

    case rsb_ext_indexed:
	*buffer++ = gds__info_rsb_ext_indexed;
	break;

    case rsb_ext_dbkey:
	*buffer++ = gds__info_rsb_ext_dbkey;
	break;

    case rsb_left_cross:
	*buffer++ = gds__info_rsb_left_cross;
	break;

    case rsb_select:
	*buffer++ = gds__info_rsb_select;
	break;

    case rsb_sql_join:
	*buffer++ = gds__info_rsb_sql_join;
	break;
 
    case rsb_simulate:
	*buffer++ = gds__info_rsb_simulate;
	break;

    case rsb_sim_cross:
	*buffer++ = gds__info_rsb_sim_cross;
	break;

    case rsb_once:
	*buffer++ = gds__info_rsb_once;
	break;
    
    default:
	*buffer++ = gds__info_rsb_unknown;
	break;
    }

/* dump out any sub-rsbs; for join-type rses like cross
   and merge, dump out the count of streams first, then
   loop through the substreams and dump them out */

switch (rsb->rsb_type)
    {
    case rsb_cross:
	*buffer++ = (UCHAR) rsb->rsb_count;
	for (ptr = rsb->rsb_arg, end = ptr + rsb->rsb_count; ptr < end; ptr++)
	    if (!dump_rsb (request, *ptr, &buffer, buffer_length))
   		return FALSE;
	break;

    case rsb_union:
	*buffer++ = rsb->rsb_count / 2;
	for (ptr = rsb->rsb_arg, end = ptr + rsb->rsb_count; ptr < end; ptr++)
	{
	    if (!dump_rsb (request, *ptr, &buffer, buffer_length))
   		return FALSE;
	    ptr++;
	}
	break;

    case rsb_merge:
	*buffer++ = (SCHAR) rsb->rsb_count;
	for (ptr = rsb->rsb_arg, end = ptr + rsb->rsb_count * 2; ptr < end; ptr += 2)
	    if (!dump_rsb (request, *ptr, &buffer, buffer_length))
   		return FALSE;
	break;

    case rsb_left_cross:
	*buffer++ = 2;
	if (!dump_rsb (request, rsb->rsb_arg [RSB_LEFT_outer], &buffer, buffer_length))
   	    return FALSE;
	if (!dump_rsb (request, rsb->rsb_arg [RSB_LEFT_inner], &buffer, buffer_length))
	    return FALSE;
	break;
    }
    
/* dump out the next rsb */

if (rsb->rsb_next)
    if (!dump_rsb (request, rsb->rsb_next, &buffer, buffer_length))
	return FALSE;

*buffer++ = gds__info_rsb_end;

*buffer_ptr = buffer;

return TRUE;
}

static BOOLEAN estimate_cost (
    TDBB	tdbb,
    OPT		opt,
    USHORT	stream,
    double	*cost,
    double	*resulting_cardinality)
{
/**************************************
 *
 *	e s t i m a t e _ c o s t
 *
 **************************************
 *
 * Functional description
 *	Make an estimate of the cost to fetch a stream.  The cost
 *	is a function of estimated cardinality of the relation, index
 *	selectivity, and total boolean selectivity.  Since none of
 *	this information is available, the estimates are likely to
 *	be a bit weak.  Return TRUE if the relation is index 
 *	retrievable.
 *
 **************************************/
CSB		csb;
IDX		*idx;
NOD		node;
USHORT		indexes, i, unique, equalities, inequalities, index_hits, count;
SSHORT		n;
ULONG		inactivities[OPT_BITS];
double		s, selectivity, cardinality, index_selectivity;
register struct opt_repeat	*tail, *opt_end;
struct csb_repeat		*csb_tail;

DEV_BLKCHK (opt, type_opt);
SET_TDBB (tdbb);

csb = opt->opt_csb;
csb_tail = &csb->csb_rpt [stream];
csb_tail->csb_flags |= csb_active;
cardinality = MAX (csb_tail->csb_cardinality, 10);
index_selectivity = 1.0;
indexes = equalities = inequalities = index_hits = 0;
unique = FALSE;
get_inactivities (csb, inactivities);

/* Compute index selectivity.  This involves finding the indices
   to be utilized and making a crude guess of selectivities.  */

if (opt->opt_count)
    {
    idx = csb_tail->csb_idx;
    for (i = 0; i < csb_tail->csb_indices; i++)
	{
	n = 0;
	clear_bounds (opt, idx);
	opt_end = &opt->opt_rpt [opt->opt_count];
	for (tail = opt->opt_rpt; tail < opt_end; tail++)
	    {
	    node = tail->opt_conjunct;
	    if (!(tail->opt_flags & opt_used) &&
		!(TEST_DEP_ARRAYS(tail->opt_dependencies, inactivities)))
		n += match_index (tdbb, opt, stream, node, idx);
	    }
	tail = opt->opt_rpt;
	if (tail->opt_lower || tail->opt_upper)
	    {
	    indexes++;
	    opt_end = opt->opt_rpt + idx->idx_count;
	    for (count = 0; tail < opt_end; tail++, count++)
		if (!tail->opt_lower || tail->opt_lower != tail->opt_upper)
		    break;
	    s = idx->idx_selectivity;
	    if (s <= 0 || s >= 1)
		s = ESTIMATED_SELECTIVITY;
	    if (count == idx->idx_count)
		{
		if (idx->idx_flags & idx_unique)
		    {
		    unique = TRUE;
		    s = 1 / cardinality;
		    }
		}
	    else
		s *= INVERSE_ESTIMATE;
	    index_selectivity *= s;
	    index_hits += MAX (count, n);
	    }
	idx = NEXT_IDX (idx->idx_rpt, idx->idx_count);
	}
    }
	
/* We now known the relation cardinality, the combined index selectivity,
   and the number of index lookups required.  From this we can compute the
   cost of executing the record selection expression (cost of index lookups
   plus the number of records fetched). */

if (indexes)
    *cost = cardinality * index_selectivity + indexes * INDEX_COST;
else
    *cost = cardinality;

/* Next, we need to estimate the number of records coming out of the
   record stream.  This is based on conjunctions without regard to whether
   or not they were the result of index operations. */

opt_end = opt->opt_rpt + opt->opt_count;

for (tail = opt->opt_rpt; tail < opt_end; tail++)
    {
    node = tail->opt_conjunct;
    if (!(tail->opt_flags & opt_used) &&
	!(TEST_DEP_ARRAYS(tail->opt_dependencies, inactivities)))
	{
	if (node->nod_type == nod_eql)
	    ++equalities;
	else
	    ++inequalities;
	tail->opt_flags |= opt_used;
	}
    }

if ((n = inequalities + 3 * (equalities - index_hits)) > 0)
    {
    selectivity = 0.3 / n;
    if (selectivity > index_selectivity)
	selectivity = index_selectivity;
    }
else
    selectivity = index_selectivity;

cardinality *= selectivity;

if (unique)
    *resulting_cardinality = cardinality;
else
    *resulting_cardinality = MAX (cardinality, 1.0);

csb_tail->csb_flags |= csb_active;

return (indexes != 0);
}

#ifdef EXPRESSION_INDICES
static BOOLEAN expression_equal (
    TDBB	tdbb, 
    NOD		node1, 
    NOD		node2)
{
/**************************************
 *
 *      e x p r e s s i o n _ e q u a l
 *
 **************************************
 *
 * Functional description
 *      Determine if two expression trees are the same for 
 *	the purposes of matching one half of a boolean expression 
 *	to an index.
 *
 **************************************/
DSC	*desc1, *desc2; 

DEV_BLKCHK (node1, type_nod);
DEV_BLKCHK (node2, type_nod);

SET_TDBB (tdbb);

if (!node1 || !node2)
    BUGCHECK (303); /* msg 303 Invalid expression for evaluation */

if (node1->nod_type != node2->nod_type)
    return FALSE; 

switch (node1->nod_type)
    {
    case nod_add:
    case nod_multiply:
    case nod_add2:
    case nod_multiply2:
	/* A+B is equivilant to B+A, ditto A*B==B*A */
	/* Note: If one expression is A+B+C, but the other is B+C+A we won't
	   necessarily match them */
	if (expression_equal (tdbb, node1->nod_arg [0], node2->nod_arg [1]) &&  
	    expression_equal (tdbb, node1->nod_arg [1], node2->nod_arg [0])) 
	     return TRUE;
	/* Fall into ... */
    case nod_subtract:
    case nod_divide:
    case nod_subtract2:
    case nod_divide2:
    case nod_concatenate:
	if (expression_equal (tdbb, node1->nod_arg [0], node2->nod_arg [0]) &&  
	    expression_equal (tdbb, node1->nod_arg [1], node2->nod_arg [1])) 
	     return TRUE;
	break; 

    case nod_rec_version:
    case nod_dbkey:
    	if (node1->nod_arg [0] == node2->nod_arg [1]) 
	    return TRUE; 
	break;

    case nod_field:
        /* don't compare stream id's because we will use computable() to make sure the 
           field is in the same stream as the index; the stream id of the index expression 
           is always 0 so it isn't usable for comparison */

	if (node1->nod_arg [e_fld_id] == node2->nod_arg [e_fld_id])
	    return TRUE; 
	break;

    case nod_function:
        if ((node1->nod_arg [e_fun_function] == node2->nod_arg [e_fun_function]) && 
	    expression_equal (tdbb, node1->nod_arg [e_fun_args], node2->nod_arg [e_fun_args]))
	    return TRUE;
	break;

    case nod_literal:
	desc1 = EVL_expr (tdbb, node1);
	desc2 = EVL_expr (tdbb, node2);
	if (!MOV_compare (desc1, desc2)) 
	    return TRUE;
	break;

    case nod_null:
    case nod_user_name:
    case nod_current_time:
    case nod_current_date:
    case nod_current_timestamp:
	return TRUE;

    case nod_value_if:
    case nod_substr:
	if (expression_equal (tdbb, node1->nod_arg [0], node2->nod_arg [0]) &&  
	    expression_equal (tdbb, node1->nod_arg [1], node2->nod_arg [1]) &&
	    expression_equal (tdbb, node1->nod_arg [2], node2->nod_arg [2]))  
	     return TRUE;
	break;

    case nod_gen_id:
    case nod_gen_id2:
	if (node1->nod_arg [e_gen_id] == node2->nod_arg [e_gen_id])
	    return TRUE; 
	break;

    case nod_negate:
    case nod_upcase:
	if (expression_equal (tdbb, node1->nod_arg [0], node2->nod_arg [0]))
	    return TRUE; 
	break;

    case nod_cast:
	desc1 = &((FMT) node1->nod_arg [e_cast_fmt])->fmt_desc [0]; 
	desc2 = &((FMT) node2->nod_arg [e_cast_fmt])->fmt_desc [0]; 
	if (DSC_EQUIV (desc1, desc2) && 
	    expression_equal (tdbb, node1->nod_arg [0], node2->nod_arg [0]))
	    return TRUE;  
	break;

    case nod_extract:
	if ((node1->nod_arg [e_extract_part] == node2->nod_arg [e_extract_part)
		&& expression_equal (tdbb, node1->nod_arg [e_extract_value],
					   node2->nod_arg [e_extract_value]))
	    return TRUE;
	break;

    default:
	break;
    }

return FALSE;
}
#endif

static void find_best ( 
    TDBB		tdbb, 
    register OPT	opt,
    USHORT		stream,
    USHORT		position,
    UCHAR		*streams,
    NOD			plan_node,
    double		cost,
    double		cardinality)
{
/**************************************
 *
 *	f i n d _ b e s t
 *
 **************************************
 *
 * Functional description
 *	Find the best join from the passed "stream" to 
 *	the remaining "streams" in the rse.  This routine
 *	uses recursion to successively consider all 
 *	possible join orders which use indexed 
 *	relationships to form joins.
 *
 **************************************/
CSB			csb;
UCHAR			*ptr, *stream_end;
double			position_cost, position_cardinality, new_cost, new_cardinality;
USHORT			flag_vector [MAX_STREAMS], *fv;
BOOLEAN			done;
IRL			relationship;
struct opt_repeat	*tail, *opt_end, *order_end, *stream_data;

SET_TDBB (tdbb);
DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (plan_node, type_nod);

#ifdef OPT_DEBUG
/* this is used only in development so is not in the message file */

if (opt_debug_flag >= DEBUG_PUNT)
    ERR_post (gds__random, gds_arg_string, "punt", 0);
#endif

/* if a plan was specified, check that this order matches the order
   that the user provided; this may seem like an ass-backwards way to 
   enforce ordering, but I think it is important to follow the same
   code path for SET PLAN as for a normal optimization--it reduces 
   chances for bugs to be introduced, and forces the person maintaining
   the optimizer to think about SET PLAN when new features are added --deej */

if (plan_node && (streams [position + 1] != stream))
    return;

/* do some initializations */

csb = opt->opt_csb;
csb->csb_rpt [stream].csb_flags |= csb_active;
stream_end = &streams [1] + streams [0];
opt_end = opt->opt_rpt + MAX (opt->opt_count, csb->csb_n_stream);
opt->opt_rpt [position].opt_stream = stream;

++position;
order_end = opt->opt_rpt + position;
stream_data = opt->opt_rpt + stream;

/* Save the various flag bits from the optimizer block to reset its
   state after each test. */

for (tail = opt->opt_rpt, fv = flag_vector; tail < opt_end; tail++)
    *fv++ = tail->opt_flags & (opt_stream_used | opt_used);

/* Compute delta and total estimate cost to fetch this stream;
   if a plan was specified, don't bother */

if (!plan_node)
    {
    estimate_cost (tdbb, opt, stream, &position_cost, &position_cardinality);
    new_cost = cost + cardinality * position_cost;
    new_cardinality = position_cardinality * cardinality;
    }

++opt->opt_combinations;

/* If the partial order is either longer than any previous partial order,
   or the same length and cheap, save order as "best" */

if (position > opt->opt_best_count ||
    (position == opt->opt_best_count && new_cost < opt->opt_best_cost))
    {
    opt->opt_best_count = position;
    opt->opt_best_cost = new_cost;
    for (tail = opt->opt_rpt; tail < order_end; tail++)
	tail->opt_best_stream = tail->opt_stream;
#ifndef OPT_DEBUG
    }
#else
    if (opt_debug_flag >= DEBUG_CANDIDATE)
	print_order (opt, position, new_cardinality, new_cost);
    }
else
    if (opt_debug_flag >= DEBUG_ALL)
	print_order (opt, position, new_cardinality, new_cost);
#endif

/* mark this stream as "used" in the sense that it is already included 
   in this particular proposed stream ordering */

stream_data->opt_flags |= opt_stream_used;
done = FALSE;

/* We need to prune the combinations to avoid spending all of our time 
   recursing through find_best().  Based on experimentation, the cost of 
   recursion becomes significant at about a 7 table join.  Therefore, 
   make a simplifying assumption that if we have already seen a join 
   ordering that is lower cost than this one, give up. */
   
if (position > 4)
    {
    tail = opt->opt_rpt + position;

    /* If we are the new low-cost join ordering, record that 
       fact.  Otherwise, give up. */

    if (tail->opt_best_stream_cost == 0 || new_cost < tail->opt_best_stream_cost)
        tail->opt_best_stream_cost = new_cost;
    else if (!plan_node)
        done = TRUE;
    }

/* if we've used up all the streams there's no reason to go any further */

if (position == streams [0])
    done = TRUE;

/* First, handle any streams that have direct unique indexed relationships to this
   stream.  If there are any, we won't consider (now) indirect relationships */

if (!done)
    for (relationship = stream_data->opt_relationships; relationship;
	 relationship = relationship->irl_next)
	if (relationship->irl_unique &&
	    (!(opt->opt_rpt [relationship->irl_stream].opt_flags & opt_stream_used)))
	    for (ptr = streams + 1; ptr < stream_end; ptr++)
		if (*ptr == relationship->irl_stream)
		    {
		    if (!plan_node)
		        done = TRUE;
		    find_best (tdbb, opt, relationship->irl_stream, position, 
			streams, plan_node, new_cost, new_cardinality);
		    break;
		    }

/* Next, handle any streams that have direct indexed relationships to this
   stream.  If there are any, we won't consider (now) indirect relationships */

if (!done)
    for (relationship = stream_data->opt_relationships; relationship;
	 relationship = relationship->irl_next)
	if (!(opt->opt_rpt [relationship->irl_stream].opt_flags & opt_stream_used))
	    for (ptr = streams + 1; ptr < stream_end; ptr++)
		if (*ptr == relationship->irl_stream)
		    {
		    if (!plan_node)
		        done = TRUE;
		    find_best (tdbb, opt, relationship->irl_stream, position, 
			streams, plan_node, new_cost, new_cardinality);
		    break;
		    }

/* If there were no direct relationships, look for indirect relationships */

if (!done)
    for (ptr = streams + 1; ptr < stream_end; ptr++)
	if (!(opt->opt_rpt [*ptr].opt_flags & opt_stream_used) &&
	    check_relationship (opt, position, *ptr))
	    find_best (tdbb, opt, *ptr, position, 
		streams, plan_node, new_cost, new_cardinality);

/* Clean up from any changes made for compute the cost for this stream */

csb->csb_rpt [stream].csb_flags &= ~csb_active;

for (tail = opt->opt_rpt, fv = flag_vector; tail < opt_end; tail++, fv++)
    tail->opt_flags &= *fv;
}

static NOD find_dbkey (
    NOD			dbkey,
    USHORT		stream,
    SLONG		*position)
{
/**************************************
 *
 *	f i n d _ d b k e y
 *
 **************************************
 *
 * Functional description
 *	Search a dbkey (possibly a concatenated one) for
 *	a dbkey for specified stream.
 *
 **************************************/
NOD	dbkey_temp, *ptr, *end;

DEV_BLKCHK (dbkey, type_nod);

if (dbkey->nod_type == nod_dbkey)
    {
    if ((USHORT) dbkey->nod_arg [0] == stream)
	return dbkey;
    else
	{
	*position = *position + 1;
	return NULL;
	}
    }
else if (dbkey->nod_type == nod_concatenate)
    for (ptr = dbkey->nod_arg, end = ptr + dbkey->nod_count; 
	 ptr < end; ptr++)
	if (dbkey_temp = find_dbkey (*ptr, stream, position))
	    return dbkey_temp;

return NULL;
}

static USHORT find_order (
    TDBB		tdbb, 
    register OPT	opt,
    UCHAR		*streams,
    NOD			plan_node)
{
/**************************************
 *
 *	f i n d _ o r d e r
 *
 **************************************
 *
 * Functional description
 *	Given a set of streams, select the "best order" to join them.
 *	The "best order" is defined as longest, cheapest join order
 *	(length, of course, takes precedence over cost).  The best
 *	order is developed and returned in the optimization block.
 *
 **************************************/
UCHAR	*stream, *stream_end;

SET_TDBB (tdbb);
DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (plan_node, type_nod);

opt->opt_best_count = 0;         

/* if a plan was specified, the order is already 
   present in the streams vector, so we only want
   to try one order */

if (plan_node)
    stream_end = &streams [1] + 1;
else
    stream_end = &streams [1] + streams [0];

/* Consider each stream as the leftmost stream in the join order;
   for each stream, the best order from that stream is considered,
   and the one which is best is placed into the opt block.  Thus
   at the end of this loop the opt block holds the best order */

for (stream = streams + 1; stream < stream_end; stream++)
    find_best (tdbb, opt, *stream, 0, streams, plan_node, (double) 0, (double) 1);

#ifdef OPT_DEBUG
if (opt_debug_flag >= DEBUG_BEST)
    {
    struct opt_repeat	*order_end, *tail;

	order_end = opt->opt_rpt + opt->opt_best_count;
    ib_fprintf (opt_debug_file, "find_order()  -- best_count: %2.2d, best_streams: ", opt->opt_best_count);
    for (tail = opt->opt_rpt; tail < order_end; tail++)
	ib_fprintf (opt_debug_file, "%2.2d ", tail->opt_best_stream);
    ib_fprintf (opt_debug_file, "\n\t\t\tbest_cost: %g\tcombinations: %ld\n", opt->opt_best_cost, opt->opt_combinations);
    }
#endif

return opt->opt_best_count;
}

static void find_rsbs (
    RSB		rsb,
    LLS		*stream_list,
    LLS		*rsb_list)
{
/**************************************
 *
 *	f i n d _ r s b s
 *
 **************************************
 *
 * Functional description
 *	Find all rsbs at or below the current one that map
 *	to a single stream.  Save the stream numbers in a list.
 *	For unions and aggregates also save the rsb pointer.
 *
 **************************************/
RSB	*ptr, *end;

#ifdef DEV_BUILD
DEV_BLKCHK (rsb, type_rsb);
DEV_BLKCHK (*stream_list, type_lls);
if (rsb_list)
    DEV_BLKCHK (*rsb_list, type_lls);
#endif

if (!rsb)
    return;

switch (rsb->rsb_type)
    {
    case rsb_union:
    case rsb_aggregate:
	if (rsb_list)
	    LLS_PUSH (rsb, rsb_list);

    case rsb_indexed:
    case rsb_sequential:
    case rsb_ext_sequential:
    case rsb_ext_indexed:
    case rsb_procedure:
	/* No need to go any farther down with these */

	LLS_PUSH ((BLK) rsb->rsb_stream, stream_list);
	return;

    case rsb_cross:
	/* Loop through the sub-streams */

	for (ptr = rsb->rsb_arg, end = ptr + rsb->rsb_count; ptr < end; ptr++)
	    find_rsbs (*ptr, stream_list, rsb_list);
	break;

    case rsb_left_cross:
	find_rsbs (rsb->rsb_arg [RSB_LEFT_outer], stream_list, rsb_list);
	find_rsbs (rsb->rsb_arg [RSB_LEFT_inner], stream_list, rsb_list);
	break;

    case rsb_merge:
	/* Loop through the sub-streams */

	for (ptr = rsb->rsb_arg, end = ptr + rsb->rsb_count * 2; ptr < end; ptr += 2)
	    find_rsbs (*ptr, stream_list, rsb_list);
	break;
    }

find_rsbs (rsb->rsb_next, stream_list, rsb_list);
}

static void form_rivers (
    TDBB	tdbb,
    OPT		opt,
    UCHAR	*streams,
    LLS		*river_stack,
    NOD		*sort_clause,
    NOD		*project_clause,
    NOD		plan_clause)
{
/**************************************
 *
 *	f o r m _ r i v e r s
 *
 **************************************
 *
 * Functional description
 *	Form streams into rivers according
 *	to the user-specified plan.
 *
 **************************************/
NOD	*ptr, *end, plan_node, relation_node;
USHORT	count; 
UCHAR	temp [MAX_STREAMS];

SET_TDBB (tdbb);

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (*river_stack, type_lls);
if (sort_clause)
    DEV_BLKCHK (*sort_clause, type_nod);
if (project_clause)
    DEV_BLKCHK (*project_clause, type_nod);
DEV_BLKCHK (plan_clause, type_nod);

temp [0] = 0;
count = plan_clause->nod_count;

/* this must be a join or a merge node, so go through
   the substreams and place them into the temp vector
   for formation into a river */

for (ptr = plan_clause->nod_arg, end = ptr + count; ptr < end; ptr++)
    {
    plan_node = *ptr;

    if (plan_node->nod_type == nod_merge ||
	plan_node->nod_type == nod_join)
	{
	form_rivers (tdbb, opt, streams, river_stack, sort_clause, project_clause, plan_node);
	continue;
	}

    /* at this point we must have a retrieval node, so put
       the stream into the river */

    temp [0]++;

    relation_node = plan_node->nod_arg [e_retrieve_relation];
    temp [temp [0]] = (UCHAR) relation_node->nod_arg [e_rel_stream];
    }
            
/* just because the user specified a join does not mean that 
   we are able to form a river;  thus form as many rivers out
   of the join are as necessary to exhaust the streams */
          
do
    count = find_order (tdbb, opt, temp, plan_node);
while (form_river (tdbb, opt, count, streams, temp, river_stack, sort_clause, project_clause, NULL_PTR));
}

static BOOLEAN form_river (
    TDBB	tdbb,
    OPT		opt,
    USHORT	count,
    UCHAR	*streams,
    UCHAR	*temp,
    LLS		*river_stack,
    NOD		*sort_clause,
    NOD		*project_clause,
    NOD		plan_clause)
{
/**************************************
 *
 *	f o r m _ r i v e r
 *
 **************************************
 *
 * Functional description
 *	Form streams into rivers (combinations of streams). 
 *
 **************************************/
RSB		rsb, *ptr;
CSB		csb;
RIV		river;
UCHAR		*stream, *end_stream, *t2;
struct opt_repeat	*tail, *opt_end;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (*river_stack, type_lls);
if (sort_clause)
    DEV_BLKCHK (*sort_clause, type_nod);
if (project_clause)
    DEV_BLKCHK (*project_clause, type_nod);
DEV_BLKCHK (plan_clause, type_nod);

SET_TDBB (tdbb);
csb = opt->opt_csb;

/* Allocate a river block and move the best order into it */

river = (RIV) ALLOCDV (type_riv, count);
LLS_PUSH (river, river_stack);
river->riv_count = (UCHAR) count;
if (count == 1)
    {
    rsb = NULL;
    ptr = &river->riv_rsb;
    }
else
    {
    river->riv_rsb = rsb = (RSB) ALLOCDV (type_rsb, count);
    rsb->rsb_type = rsb_cross;
    rsb->rsb_count = count;
    rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb));
    ptr = rsb->rsb_arg;
    }

stream = river->riv_streams;
opt_end = opt->opt_rpt + count;

if (count != streams [0])
    sort_clause = project_clause = NULL;

for (tail = opt->opt_rpt; tail < opt_end; tail++, stream++, ptr++)
    {
    *stream = (UCHAR) tail->opt_best_stream;
    *ptr = gen_retrieval (tdbb, opt, *stream, sort_clause, project_clause, FALSE, FALSE, NULL);
    sort_clause = project_clause = NULL;
    }

/* determine whether the rsb we just made should be marked as a projection */

if (rsb && rsb->rsb_arg [0] && ((RSB) rsb->rsb_arg [0])->rsb_flags & rsb_project)
    rsb->rsb_flags |= rsb_project;

set_made_river (opt, river);
set_inactive (opt, river);

/* Reform "temp" from streams not consumed */

stream = temp + 1;
end_stream = stream + temp [0];

if (!(temp [0] -= count))
    return FALSE;

for (t2 = stream; t2 < end_stream; t2++)
    {
    for (tail = opt->opt_rpt; tail < opt_end; tail++)
        if (*t2 == tail->opt_best_stream)
            goto used;
    *stream++ = *t2;
    used:;
    }

return TRUE;
}


static RSB gen_aggregate (
    TDBB	tdbb,
    OPT		opt,
    NOD		node)
{
/**************************************
 *
 *	g e n _ a g g r e g a t e
 *
 **************************************
 *
 * Functional description
 *	Generate an RSB for each aggregate operation.
 *      Generate an ASB for each DISTINCT aggregate.
 *      
 **************************************/
RSE	rse;
RSB	rsb;
CSB	csb;
USHORT	count;
NOD	map, operator, aggregate;
NOD	*ptr, *end, from;
DSC	descriptor, *desc;
ASB     asb;
SKD     *sort_key;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (node, type_nod);

SET_TDBB (tdbb);
csb = opt->opt_csb;

rse = (RSE) node->nod_arg [e_agg_rse];
rse->rse_sorted = node->nod_arg [e_agg_group];

/* try to optimize MAX and MIN to use an index; for now, optimize 
   only the simplest case, although it is probably possible 
   to use an index in more complex situations */

map = node->nod_arg [e_agg_map];
if ((map->nod_count == 1) &&
    (ptr = map->nod_arg) &&
    (operator = (*ptr)->nod_arg [e_asgn_from]) &&
    (operator->nod_type == nod_agg_min 
    || operator->nod_type == nod_agg_max))
    {
    /* generate a sort block which the optimizer will try to map to an index */

    aggregate = PAR_make_node (tdbb, 2);
    aggregate->nod_type = nod_sort;
    aggregate->nod_count = 1;
    aggregate->nod_arg [0] = operator->nod_arg [e_asgn_from];

    /* in the max case, flag the sort as descending */

    if (operator->nod_type == nod_agg_max)
        aggregate->nod_arg [1] = (NOD) TRUE;
    
    rse->rse_aggregate = aggregate;
    }

/* allocate and optimize the record source block */

rsb = (RSB) ALLOCDV (type_rsb, 1);
rsb->rsb_type = rsb_aggregate;
rsb->rsb_stream = (UCHAR) node->nod_arg [e_agg_stream];
rsb->rsb_format = csb->csb_rpt [rsb->rsb_stream].csb_format;
rsb->rsb_next = OPT_compile (tdbb, csb, rse, NULL);
rsb->rsb_arg [0] = (RSB) node;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb));

/* if the rse_aggregate is still set, that means the optimizer  
   was able to match the field to an index, so flag that fact 
   so that it can be handled in EVL_group */

if (rse->rse_aggregate)
    {
    if (operator->nod_type == nod_agg_min)
        operator->nod_type = nod_agg_min_indexed;
    else if (operator->nod_type == nod_agg_max)
        operator->nod_type = nod_agg_max_indexed;
    }

/* Now generate a separate ASB for each distinct operation;
   note that this should be optimized to use indices if possible */

desc = &descriptor;

for (ptr = map->nod_arg, end = ptr + map->nod_count; ptr < end; ptr++)
    {
    from = (*ptr)->nod_arg [e_asgn_from];
    if ((from->nod_type == nod_agg_count_distinct)    ||
        (from->nod_type == nod_agg_total_distinct)    ||
        (from->nod_type == nod_agg_total_distinct2)   ||
        (from->nod_type == nod_agg_average_distinct2) ||
        (from->nod_type == nod_agg_average_distinct)) 
        {
        count = asb_delta + 1 + (sizeof (SKD) + sizeof (NOD*) - 1) / sizeof (NOD*);
        asb = (ASB) PAR_make_node (tdbb, count);
        asb->nod_type = nod_asb;
        asb->nod_count = 0;
        
        /* build the sort key definition. Turn varying text and
           cstrings into text */

        CMP_get_desc (tdbb, csb, from->nod_arg[0], desc);
        if (desc->dsc_dtype == dtype_varying)
            {
            desc->dsc_dtype = dtype_text;
            desc->dsc_length -= sizeof (USHORT);
            }
        else if (desc->dsc_dtype == dtype_cstring)
            {
            desc->dsc_dtype = dtype_text;
            desc->dsc_length--;
            }

        sort_key = asb->asb_key_desc = (SKD*) asb->asb_key_data;
        sort_key->skd_offset = 0;
	assert (desc->dsc_dtype >= 0 && 
		desc->dsc_dtype < sizeof (sort_dtypes)/sizeof (sort_dtypes[0]));
        sort_key->skd_dtype = sort_dtypes [desc->dsc_dtype];
	assert (sort_key->skd_dtype != 0);

        sort_key->skd_length = desc->dsc_length;
        sort_key->skd_flags = SKD_ascending;

        asb->nod_impure = CMP_impure (csb, sizeof (struct iasb));
        asb->asb_desc = *desc;

        from->nod_arg[1] = (NOD) asb;
        from->nod_count = 2;
        }
    }

return rsb;
}

static RSB gen_boolean (
    TDBB		tdbb,
    register OPT	opt,
    RSB			prior_rsb,
    NOD			node)
{
/**************************************
 *
 *	g e n _ b o o l e a n
 *
 **************************************
 *
 * Functional description
 *	Compile and optimize a record selection expression into a
 *	set of record source blocks (rsb's).
 *
 **************************************/
register CSB	csb;
register RSB	rsb;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (prior_rsb, type_rsb);

SET_TDBB (tdbb);

csb = opt->opt_csb;
rsb = (RSB) ALLOCDV (type_rsb, 1);
rsb->rsb_count = 1;
rsb->rsb_type = rsb_boolean;
rsb->rsb_next = prior_rsb;
rsb->rsb_arg [0] = (RSB) node;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb));

return rsb;
}

static RSB gen_first (
    TDBB		tdbb,
    register OPT	opt,
    RSB			prior_rsb,
    NOD			node)
{
/**************************************
 *
 *	g e n _ f i r s t
 *
 **************************************
 *
 * Functional description
 *	Compile and optimize a record selection expression into a
 *	set of record source blocks (rsb's).
 *
 **************************************/
register CSB	csb;
register RSB	rsb;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (prior_rsb, type_rsb);
DEV_BLKCHK (node, type_nod);

SET_TDBB (tdbb);

csb = opt->opt_csb;
rsb = (RSB) ALLOCDV (type_rsb, 1);
rsb->rsb_count = 1;
rsb->rsb_type = rsb_first;
rsb->rsb_next = prior_rsb;
rsb->rsb_arg [0] = (RSB) node;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb_first_n));

return rsb;
}

static void gen_join (
    TDBB	tdbb,
    OPT		opt,
    UCHAR	*streams,
    LLS		*river_stack,
    NOD		*sort_clause,
    NOD		*project_clause,
    NOD		plan_clause)
{
/**************************************
 *
 *	g e n _ j o i n
 *
 **************************************
 *
 * Functional description
 *	Find all indexed relationships between streams, 
 *	then form streams into rivers (combinations of 
 * 	streams).    
 *
 **************************************/
DBB		dbb;
FMT		format;
REL		relation;
CSB		csb;
RIV		river;
UCHAR		temp [MAX_STREAMS], *stream, *end_stream, *t2;
IRL		relationship;
struct opt_repeat	*tail;
struct csb_repeat	*csb_tail, *csb_tail2;
USHORT		count;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (*river_stack, type_lls);
DEV_BLKCHK (*sort_clause, type_nod);
DEV_BLKCHK (*project_clause, type_nod);
DEV_BLKCHK (plan_clause, type_nod); 

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;
csb = opt->opt_csb;

if (!streams [0])
    return;

/* If there is only a single stream, don't bother with a join */

if (streams [0] == 1)
    {
    /* if a nod_cardinality references this stream,
       compute the cardinality even though we don't
       need it to optimize retrieval */

    csb_tail = &csb->csb_rpt [streams [1]];
    if (csb_tail->csb_flags & csb_compute)
	{
        relation = csb_tail->csb_relation;
        format = CMP_format (tdbb, csb, streams [1]);
	csb_tail->csb_cardinality = (float) DPM_data_pages (tdbb, relation) *
	    dbb->dbb_page_size / format->fmt_length;
	}

    river = (RIV) ALLOCDV (type_riv, 1);
    river->riv_count = 1;
    river->riv_rsb = gen_retrieval (tdbb, opt, streams [1], sort_clause, project_clause, FALSE, FALSE, NULL);
    river->riv_streams [0] = streams [1];
    LLS_PUSH (river, river_stack);
    return;
    }

/* Compute cardinality and indexed relationships for all streams */

end_stream = streams + 1 + streams [0];

for (stream = streams + 1; stream < end_stream; stream++)
    {
    csb_tail = &csb->csb_rpt [*stream];
    relation = csb_tail->csb_relation;
    format = CMP_format (tdbb, csb, *stream);

    /* if this is an external file, set an arbitrary cardinality; 
       if a plan was specified, don't bother computing cardinality;
       otherwise give a rough estimate based on the number of data
       pages times the estimated number of records per page -- note
       this is an upper limit since all pages are probably not full
       and many of the records on page may be back versions */

    if (relation->rel_file)
	csb_tail->csb_cardinality = (float) 10000;
    else if (plan_clause)
	csb_tail->csb_cardinality = (float) 0;
    else
	csb_tail->csb_cardinality = (float) DPM_data_pages (tdbb, relation) *
	    dbb->dbb_page_size / format->fmt_length;

    /* find indexed relationships from this stream to every other stream */

    tail = opt->opt_rpt + *stream;
    csb_tail->csb_flags |= csb_active;
    for (t2 = streams + 1; t2 < end_stream; t2++)
	if (*t2 != *stream)
	    {
	    csb_tail2 = &csb->csb_rpt [*t2];
	    csb_tail2->csb_flags |= csb_active;
	    if (relationship = indexed_relationship (tdbb, opt, *t2))
		{
		relationship->irl_next = tail->opt_relationships;
		tail->opt_relationships = relationship;
		relationship->irl_stream = *t2;
		}
	    csb_tail2->csb_flags &= ~csb_active;
	    }
    csb_tail->csb_flags &= ~csb_active;

#ifdef OPT_DEBUG
    if (opt_debug_flag >= DEBUG_RELATIONSHIPS)
	{
	ib_fprintf (opt_debug_file, "gen_join () -- relationships from stream %2.2d: ", *stream);
	for (relationship = tail->opt_relationships; relationship; relationship = relationship->irl_next)
	    ib_fprintf (opt_debug_file, "%2.2d %s ", relationship->irl_stream, (relationship->irl_unique) ? "(unique)" : "");
	ib_fprintf (opt_debug_file, "\n");
	}
#endif
    }

/* if the user specified a plan, force a join order;
   otherwise try to find one */

if (plan_clause)
    form_rivers (tdbb, opt, streams, river_stack, sort_clause, project_clause, plan_clause);
else
    {
    /* copy the streams vector to a temporary space to be used
       to form rivers out of streams */

    MOVE_FAST (streams, temp, streams [0] + 1);
    do
	count = find_order (tdbb, opt, temp, NULL_PTR);
    while (form_river (tdbb, opt, count, streams, temp, river_stack, sort_clause, project_clause, NULL_PTR));
    }
}

static RSB gen_navigation (
    TDBB	tdbb,
    OPT		opt,
    USHORT	stream,
    REL		relation,
    STR		alias,
    IDX		*idx,
    NOD		*sort_ptr)
{
/**************************************
 *
 *	g e n _ n a v i g a t i o n
 *
 **************************************
 *
 * Functional description
 *	See if a navigational walk of an index is in order.  If so,
 *	generate the appropriate RSB and zap the sort pointer.  If
 *	not, return NULL.   Prior to ODS7, missing values sorted in
 *	the wrong place for ascending indices, so don't use them.
 *
 **************************************/
NOD			node, *ptr, *end, sort;
struct idx_repeat	*idx_tail;
#ifdef SCROLLABLE_CURSORS
RSE_GET_MODE		last_mode, mode;
#endif

SET_TDBB (tdbb);

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (relation, type_rel); 
DEV_BLKCHK (alias, type_str);
DEV_BLKCHK (*sort_ptr, type_nod);

/* Check sort order against index.  If they don't match, give up and
   go home.  Also don't bother if we have a non-unique index.
   This is because null values aren't placed in a "good" spot in
   the index in versions prior to V3.2. */

sort = *sort_ptr;

/* if the number of fields in the sort is greater than the number of 
   fields in the index, the index will not be used to optimize the    
   sort--note that in the case where the first field is unique, this 
   could be optimized, since the sort will be performed correctly by 
   navigating on a unique index on the first field--deej */

if (sort->nod_count > idx->idx_count)
    return NULL;

/* not sure the significance of this magic number; if it's meant to 
   signify that we shouldn't navigate on a system table, our catalog 
   has grown beyond 16 tables--it doesn't seem like a problem 
   to allow navigating through system tables, so I won't bump the 
   number up, but I'll leave it at 16 for safety's sake--deej */

if (relation->rel_id <= 16)
    return NULL;

/* if the user-specified access plan for this request didn't
   mention this index, forget it */

if ((idx->idx_runtime_flags & idx_plan_dont_use) &&
   !(idx->idx_runtime_flags & idx_plan_navigate))
    return NULL;

/* check to see if the fields in the sort match the fields in the index 
   in the exact same order--we used to check for ascending/descending prior 
   to SCROLLABLE_CURSORS, but now descending sorts can use ascending indices 
   and vice versa */

#ifdef SCROLLABLE_CURSORS
last_mode = RSE_get_next;
#endif
for (ptr = sort->nod_arg, end = ptr + sort->nod_count, idx_tail = idx->idx_rpt;
     ptr < end; ptr++, idx_tail++)
    {
    node = *ptr;
    if (node->nod_type != nod_field ||
	(USHORT) node->nod_arg [e_fld_stream] != stream ||
	(USHORT) node->nod_arg [e_fld_id] != idx_tail->idx_field
#ifdef SCROLLABLE_CURSORS
        )
#else
     || (ptr [sort->nod_count] && !(idx->idx_flags & idx_descending)) ||
	(!ptr [sort->nod_count] && (idx->idx_flags & idx_descending)))
#endif
	return NULL;

#ifdef SCROLLABLE_CURSORS
    /* determine whether we ought to navigate backwards or forwards through 
       the index--we can't allow navigating one index in two different directions 
       on two different fields at the same time! */

    mode = ((ptr [sort->nod_count] && !(idx->idx_flags & idx_descending)) ||
	    (!ptr [sort->nod_count] && (idx->idx_flags & idx_descending))) 
         ? RSE_get_backward : RSE_get_forward;

    if (last_mode == RSE_get_next)
	last_mode = mode;
    else if (last_mode != mode)
	return NULL;
#endif
    }


/* Looks like we can do a navigational walk.  Flag that
   we have used this index for navigation, and allocate
   a navigational rsb for it. */

*sort_ptr = NULL;
idx->idx_runtime_flags |= idx_navigate;

return gen_nav_rsb (tdbb, opt, stream, relation, alias, idx
#ifdef SCROLLABLE_CURSORS
    , mode
#endif
    );
}

static RSB gen_nav_rsb (
    TDBB	tdbb,
    OPT		opt,
    USHORT	stream,
    REL		relation,
    STR		alias,
    IDX		*idx
#ifdef SCROLLABLE_CURSORS
    , RSE_GET_MODE mode
#endif
    )
{
/**************************************
 *
 *	g e n _ n a v _ r s b
 *
 **************************************
 *
 * Functional description
 *	Generate a navigational rsb, either
 *	for a compile or for a set index.
 *
 **************************************/
RSB	rsb;
USHORT	key_length, size;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (relation, type_rel); 
DEV_BLKCHK (alias, type_str);

SET_TDBB (tdbb);

key_length = ROUNDUP (BTR_key_length (relation, idx), sizeof (SLONG));

rsb = (RSB) ALLOCDV (type_rsb, RSB_NAV_count);
rsb->rsb_type = rsb_navigate;
rsb->rsb_relation = relation;
rsb->rsb_stream = (UCHAR) stream;
rsb->rsb_alias = alias;
rsb->rsb_arg [RSB_NAV_index] = (RSB) OPT_make_index (tdbb, opt, relation, idx);
rsb->rsb_arg [RSB_NAV_key_length] = (RSB) (SLONG) key_length;

#ifdef SCROLLABLE_CURSORS
/* indicate that the index needs to be navigated in a mirror-image   
   fashion; that when the user wants to go backwards we actually go 
   forwards and vice versa */

if (mode == RSE_get_backward) 
    rsb->rsb_flags |= rsb_descending;
#endif

/* if this is a blr_stream, adjust the allocated impure area 
   to be based on the maximum key size so that the index may be
   reset at any time to another index of larger key length
   without adjusting the impure area offsets */

if (opt->opt_g_flags & opt_g_stream)
    key_length = MAX_KEY;

size = nav_rsb_size (rsb, key_length, 0);
rsb->rsb_impure = CMP_impure (opt->opt_csb, size);

return rsb;
}

static RSB gen_outer (
    TDBB	tdbb,
    OPT		opt,
    RSE		rse,
    LLS		river_stack,
    NOD		*sort_clause, 
    NOD		*project_clause)
{
/**************************************
 *
 *	g e n _ o u t e r
 *
 **************************************
 *
 * Functional description
 *	Generate a top level outer join.  The "outer" and "inner"
 *	sub-streams must be handled differently from each other.
 *	The inner is like other streams.  The outer stream isn't
 *	because conjuncts may not eliminate records from the
 *	stream.  They only determine if a join with an inner
 *	stream record is to be attempted.
 *
 **************************************/
RSB		rsb;
struct {
	RSB	stream_rsb;
	USHORT	stream_num;
}		stream_o, stream_i, *stream_ptr [2];
RIV		river;
NOD		node, boolean, inner_boolean;
SSHORT		i;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (rse, type_nod);
DEV_BLKCHK (river_stack, type_lls);
DEV_BLKCHK (*sort_clause, type_nod);

SET_TDBB (tdbb);

/* Determine which stream should be outer and which is inner.
   In the case of a left join, the syntactically left stream is the 
   outer, and the right stream is the inner.  For all others, swap 
   the sense of inner and outer, though for a full join it doesn't 
   matter and we should probably try both orders to see which is 
   more efficient. */

if (rse->rse_jointype != blr_left)
    {
    stream_ptr [1] = &stream_o;
    stream_ptr [0] = &stream_i;
    }
else
    {
    stream_ptr [0] = &stream_o;
    stream_ptr [1] = &stream_i;
    }

/* Loop through the outer join sub-streams in
   reverse order because rivers may have been PUSHed */

for (i = 1; i >= 0; i--)
    {
    node = rse->rse_relation [i];
    if (node->nod_type == nod_union || 
	node->nod_type == nod_aggregate ||
	node->nod_type == nod_procedure ||
	node->nod_type == nod_rse)
	{
	river = (RIV) LLS_POP (&river_stack);
	stream_ptr [i]->stream_rsb = river->riv_rsb;
	}
    else
	{
	stream_ptr [i]->stream_rsb = NULL;
	stream_ptr [i]->stream_num = (USHORT) node->nod_arg [STREAM_INDEX(node)];
	}
    }

/* Generate rsbs for the sub-streams.  For the left sub-stream
   we also will get a boolean back */

boolean = inner_boolean = NULL;
if (!stream_o.stream_rsb)
    stream_o.stream_rsb = gen_retrieval (tdbb, opt, stream_o.stream_num, sort_clause, project_clause, TRUE, FALSE, &boolean);

/* in the case of a full join, we must make sure we don't exclude record from 
   the inner stream; otherwise just retrieve it as we would for an inner join */

if (!stream_i.stream_rsb)
    stream_i.stream_rsb = (rse->rse_jointype == blr_full) ?
	gen_retrieval (tdbb, opt, stream_i.stream_num, sort_clause, project_clause, TRUE, TRUE, &inner_boolean) :
	gen_retrieval (tdbb, opt, stream_i.stream_num, sort_clause, project_clause, FALSE, TRUE, NULL);

/* generate a parent boolean rsb for any remaining booleans that 
   were not satisfied via an index lookup */

stream_i.stream_rsb = gen_residual_boolean (tdbb, opt, stream_i.stream_rsb);

/* Allocate and fill in the rsb */

rsb = (RSB) ALLOCDV (type_rsb, RSB_LEFT_count);
rsb->rsb_type = rsb_left_cross;
rsb->rsb_count = 2;
rsb->rsb_impure = CMP_impure (opt->opt_csb, sizeof (struct irsb));
rsb->rsb_arg [RSB_LEFT_outer] = stream_o.stream_rsb;
rsb->rsb_arg [RSB_LEFT_inner] = stream_i.stream_rsb;
rsb->rsb_arg [RSB_LEFT_boolean] = (RSB) boolean;
rsb->rsb_arg [RSB_LEFT_streams] = NULL;
rsb->rsb_arg [RSB_LEFT_rsbs] = NULL;
rsb->rsb_arg [RSB_LEFT_inner_boolean] = (RSB) inner_boolean;
rsb->rsb_arg [RSB_LEFT_inner_streams] = NULL;

/* find all the outer and inner substreams and push them on a stack. */

find_rsbs (stream_i.stream_rsb, (LLS *) &rsb->rsb_arg [RSB_LEFT_streams], (LLS *) &rsb->rsb_arg [RSB_LEFT_rsbs]);
if (rse->rse_jointype == blr_full)
    find_rsbs (stream_o.stream_rsb, (LLS *) &rsb->rsb_arg [RSB_LEFT_inner_streams], NULL);

return rsb;
}

static RSB gen_procedure (
    TDBB	tdbb,
    OPT		opt,
    NOD		node)
{
/**************************************
 *
 *	g e n _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Compile and optimize a record selection expression into a
 *	set of record source blocks (rsb's).
 *
 **************************************/
register CSB	csb;
PRC		procedure;
RSB		rsb;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (node, type_nod);

SET_TDBB (tdbb);

csb = opt->opt_csb;
procedure = (PRC) node->nod_arg [e_prc_procedure];

rsb = (RSB) ALLOCDV (type_rsb, RSB_PRC_count);
rsb->rsb_type = rsb_procedure;
rsb->rsb_stream = (UCHAR) node->nod_arg [e_prc_stream];
rsb->rsb_procedure = procedure;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb_procedure));
rsb->rsb_arg [RSB_PRC_inputs] = (RSB) node->nod_arg [e_prc_inputs];
rsb->rsb_arg [RSB_PRC_in_msg] = (RSB) node->nod_arg [e_prc_in_msg];

return rsb;
}

static RSB gen_residual_boolean (
    TDBB		tdbb,
    register OPT	opt,
    RSB			prior_rsb)
{
/**************************************
 *
 *	g e n _ r e s i d u a l _ b o o l e a n
 *
 **************************************
 *
 * Functional description
 *	Pick up any residual boolean remaining,
 *	meaning those that have not been used 
 *	as part of some join.  These booleans
 *	must still be applied to the result stream.
 *
 **************************************/
NOD		node, boolean;
register struct opt_repeat	*tail, *opt_end;

SET_TDBB (tdbb);


DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (prior_rsb, type_rsb);

boolean = NULL;
opt_end = opt->opt_rpt + opt->opt_count;

for (tail = opt->opt_rpt; tail < opt_end; tail++)
    {
    node = tail->opt_conjunct;
    if (!(tail->opt_flags & opt_used))
	{
	compose (&boolean, node, nod_and);
	tail->opt_flags |= opt_used;
	}
    }

if (!boolean)
    return prior_rsb;

return gen_boolean (tdbb, opt, prior_rsb, boolean);
}

static RSB gen_retrieval (
    TDBB	tdbb,
    OPT		opt,
    SSHORT	stream,
    NOD		*sort_ptr,
    NOD		*project_ptr,
    BOOLEAN	outer_flag, 
    BOOLEAN	inner_flag, 
    NOD		*return_boolean)
{
/**************************************
 *
 *	g e n _ r e t r i e v a l
 *
 **************************************
 *
 * Functional description
 *	Compile and optimize a record selection expression into a
 *	set of record source blocks (rsb's).
 *
 **************************************/
register CSB	csb;
REL		relation;
STR		alias;
RSB		rsb;
IDX		*idx;
NOD		node, opt_boolean, inversion; 
USHORT		segments;
SSHORT		i;
register struct opt_repeat	*tail, *opt_end, *idx_tail, *idx_end;
struct csb_repeat		*csb_tail;
BOOLEAN		full = FALSE;

SET_TDBB (tdbb);

#ifdef DEV_BUILD
DEV_BLKCHK (opt, type_opt);
if (sort_ptr) 
    DEV_BLKCHK (*sort_ptr, type_nod);
if (project_ptr)
    DEV_BLKCHK (*project_ptr, type_nod);
if (return_boolean)
    DEV_BLKCHK (*return_boolean, type_nod);
#endif

/* since a full outer join is a special case for us, as we have 2 outer
 * streams, recoginze this condition and set the full flag, also reset the
 * inner flag. This condition is only statisfied for the second stream in
 * the full join. This condition is only set from the call in gen_outer() in
 * case of a full join. 
 */

if ((inner_flag == TRUE) && (outer_flag == TRUE))
    {
    /* the inner flag back to false and set the full flag */
    inner_flag = FALSE; 
    full = TRUE;
    }

csb = opt->opt_csb;
csb_tail = &csb->csb_rpt [stream];
relation = csb_tail->csb_relation;
alias = make_alias (tdbb, csb, csb_tail);
csb_tail->csb_flags |= csb_active;

/* bug #8180 reported by Bill Karwin: when a DISTINCT and an ORDER BY 
   are done on different fields, and the ORDER BY can be mapped to an 
   index, then the records are returned in the wrong order because the 
   DISTINCT sort is performed after the navigational walk of the index; 
   for that reason, we need to de-optimize this case so that the ORDER 
   BY does not use an index; if desired, we could re-optimize by doing 
   the DISTINCT first, using a sparse bit map to store the DISTINCT 
   records, then perform the navigational walk for the ORDER BY and 
   filter the records out with the sparse bitmap.  However, that is a 
   task for another day.  --deej */

/* Bug #8958: comment out anything having to do with mapping a DISTINCT 
   to an index, for now.  The fix for this bug was going to be so involved 
   that it made more sense to deoptimize this case for the time being until 
   we can determine whether it really makes sense to optimize a DISTINCT, 
   or for that matter, and ORDER BY, via an index--there is a case 
   to be made that it is a deoptimization and more testing needs to be done 
   to determine that; see more comments in the bug description
 */

if (sort_ptr && *sort_ptr && project_ptr && *project_ptr) 
    sort_ptr = NULL;

/* Time to find inversions.  For each index on the relation
   match all unused booleans against the index looking for upper
   and lower bounds that can be computed by the index.  When
   all unused conjunctions are exhausted, see if there is enough
   information for an index retrieval.  If so, build up an
   inversion component of the boolean. */

inversion = NULL;
opt_end = opt->opt_rpt + (inner_flag ? opt->opt_count : opt->opt_parent_count);
rsb = NULL;

if (relation->rel_file)
    rsb = EXT_optimize (opt, stream, sort_ptr ? sort_ptr : project_ptr);
else if (opt->opt_parent_count || (sort_ptr && *sort_ptr)
	 /***|| (project_ptr && *project_ptr)***/
	 )
    {
    /* we want to start with indices which have more index segments, attempting to match 
       all the conjuncts possible to these indices, on the theory that one index matched 
       to n booleans is more selective and uses less resources than using n indices;
       therefore find out which index has the most segments and proceed backwards from there; 
       NOTE: it's possible that a boolean might be matched to an index and then later it could 
       have been paired with another boolean to match another index such that both booleans 
       could be calculated via the index; currently we won't detect that case
     */

    segments = 0;
    for (i = 0, idx = csb_tail->csb_idx; i < csb_tail->csb_indices; i++, idx = NEXT_IDX (idx->idx_rpt, idx->idx_count))
	segments = MAX (segments, idx->idx_count);
        
    for (; segments; segments--)
    for (i = 0, idx = csb_tail->csb_idx; i < csb_tail->csb_indices; i++, idx = NEXT_IDX (idx->idx_rpt, idx->idx_count))
	{
	/* skip this index if it doesn't have the right number of segments */

	if (idx->idx_count != segments)
	    continue;

	/* skip this part if the index wasn't specified for indexed 
	   retrieval (still need to look for navigational retrieval) */

	if ((idx->idx_runtime_flags & idx_plan_dont_use) &&
	   !(idx->idx_runtime_flags & idx_plan_navigate))
	    continue;

        /* go through all the unused conjuncts and see if 
           any of them are computable using this index */

        clear_bounds (opt, idx);
	tail = opt->opt_rpt;
	if (outer_flag)
	    tail += opt->opt_count;
        for (; tail < opt_end; tail++)
            {
	    if (tail->opt_flags & opt_matched)
		continue;

            node = tail->opt_conjunct;
            if (!(tail->opt_flags & opt_used) && computable (csb, node, -1, (BOOLEAN) (inner_flag || outer_flag) ? TRUE : FALSE))
                match_index (tdbb, opt, stream, node, idx);

	    if (node->nod_type == nod_starts)
	        compose (&inversion,
	            make_starts (tdbb, opt, relation, node, stream, idx), nod_bit_and);
	    if (node->nod_type == nod_missing)
	        compose (&inversion,
	  	    make_missing (tdbb, opt, relation, node, stream, idx), nod_bit_and);
	    }

	/* look for a navigational retrieval (unless one was already found or
           there is no sort block); if no navigational retrieval on this index,
           add an indexed retrieval to the inversion tree */

	if (!rsb)
	    {
	    if (sort_ptr && *sort_ptr) 
	        {
	        if (rsb = gen_navigation (tdbb, opt, stream, relation, alias, idx, sort_ptr))
	            continue;
		}
	    
		/* for now, make sure that we only map a DISTINCT to an index if they contain 
	           the same number of fields; it should be possible to map a DISTINCT to an 
	           index which has extra fields to the right, but we need to add some code 
	           in NAV_get_record() to check when the relevant fields change, rather than 
	           the whole index key */
	    
	    /***if (project_ptr && *project_ptr)
     
	        if ((idx->idx_count == (*project_ptr)->nod_count) &&
	            (rsb = gen_navigation (tdbb, opt, stream, relation, alias, idx, project_ptr)))
		    {
	            rsb->rsb_flags |= rsb_project;
	            continue;
	            }***/
	    }

	if (opt->opt_rpt [0].opt_lower || opt->opt_rpt [0].opt_upper)
	    {
	    compose (&inversion, OPT_make_index (tdbb, opt, relation, idx), nod_bit_and);

	    if (!outer_flag)
	        {
	        /* Mark conjuncts as matched if they actually participate in
	           the indexed retrieval. A conjunct matches against an index
	           segment only if all more major index segment positions have
	           been matched by conjuncts. */

	        idx_tail = opt->opt_rpt;
	        idx_end = idx_tail + idx->idx_count;
	        for (; idx_tail < idx_end && (idx_tail->opt_lower || idx_tail->opt_upper); idx_tail++)
    	    	    for (tail = opt->opt_rpt; tail < opt_end; tail++)
		        if (idx_tail->opt_match == tail->opt_conjunct)
	    	    	    tail->opt_flags |= opt_matched;
		}
	    }
	}
    }

if (outer_flag)
    {
    /* Now make another pass thru the outer conjuncts only, finding unused,
       computable booleans.  When one is found, roll it into a final
       boolean and mark it used. */

    *return_boolean = NULL;
    opt_end = opt->opt_rpt + opt->opt_count;

    for (tail = opt->opt_rpt; tail < opt_end; tail++)
        {
        node = tail->opt_conjunct;
        if (!(tail->opt_flags & opt_used) && computable (csb, node, -1, FALSE))
	    {
	    compose (return_boolean, node, nod_and);
	    tail->opt_flags |= opt_used;
	    }
        }
    }

/* Now make another pass thru the conjuncts finding unused, computable
   booleans.  When one is found, roll it into a final boolean and mark
   it used. If a computable boolean didn't match against an index then
   mark the stream to denote unmatched booleans. */

opt_boolean = NULL;
opt_end = opt->opt_rpt + (inner_flag ? opt->opt_count : opt->opt_parent_count);

tail = opt->opt_rpt;
if (outer_flag)
    tail += opt->opt_count;

for (; tail < opt_end; tail++)
    {
    node = tail->opt_conjunct;
    if (!relation->rel_file)
	compose (&inversion, OPT_make_dbkey (opt, node, stream), nod_bit_and);
    if (!(tail->opt_flags & opt_used) && computable (csb, node, -1, FALSE))
	{
	if (node->nod_type == nod_or)
	    compose (&inversion, make_inversion (tdbb, opt, node, stream), nod_bit_and);
	compose (&opt_boolean, node, nod_and);
	tail->opt_flags |= opt_used;

	if (!outer_flag && !(tail->opt_flags & opt_matched))
	    csb_tail->csb_flags |= csb_unmatched;
	}
    }

if (full)
    return gen_rsb (tdbb, opt, rsb, inversion, stream, relation, alias, 
        *return_boolean, csb_tail->csb_cardinality);
else
    return gen_rsb (tdbb, opt, rsb, inversion, stream, relation, alias, 
        opt_boolean, csb_tail->csb_cardinality);
}

static RSB gen_rsb (
    TDBB	tdbb,
    OPT		opt,
    RSB		rsb,
    NOD		inversion,
    SSHORT	stream,
    REL		relation,
    STR		alias,
    NOD		boolean,
    float	cardinality)
{
/**************************************
 *
 *	g e n _ r s b
 *
 **************************************
 *
 * Functional description
 *	Generate a record source block to handle either a sort or a project.
 *
 **************************************/
SSHORT	size;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (rsb, type_rsb);
DEV_BLKCHK (inversion, type_nod);
DEV_BLKCHK (relation, type_rel);
DEV_BLKCHK (alias, type_str);
DEV_BLKCHK (boolean, type_nod);

SET_TDBB (tdbb);

if (rsb)
    {
    if (rsb->rsb_type == rsb_navigate && inversion)
	rsb->rsb_arg [RSB_NAV_inversion] = (RSB) inversion;
    }
else
    {
    if (inversion)
	{
	rsb = (RSB) ALLOCDV (type_rsb, 1);
	rsb->rsb_type = rsb_indexed;
	rsb->rsb_count = 1;
	size = sizeof (struct irsb_index);
	rsb->rsb_arg [0] = (RSB) inversion;
	}
    else
	{
	rsb = (RSB) ALLOCDV (type_rsb, 0);
	rsb->rsb_type = rsb_sequential;
	size = sizeof (struct irsb);
	if (boolean)
	    opt->opt_csb->csb_rpt [stream].csb_flags |= csb_unmatched;	
	}

    rsb->rsb_stream = (UCHAR) stream;
    rsb->rsb_relation = relation;
    rsb->rsb_alias = alias;

    /* if this is a blr_stream, we need to leave room
       in the impure area for a navigational-type rsb;
       even if this is not currently a navigational rsb, 
       OPT_set_index() could be used to convert it to one */
              
    if (opt->opt_g_flags & opt_g_stream)
       	{
       	size = sizeof (struct inv);
       	size = nav_rsb_size (rsb, MAX_KEY, size);
       	}

    rsb->rsb_impure = CMP_impure (opt->opt_csb, size);
    }

if (boolean)
    rsb = gen_boolean (tdbb, opt, rsb, boolean);
                                   
/* retain the cardinality for use at runtime by blr_cardinality */

rsb->rsb_cardinality = (ULONG) cardinality;

return rsb;
}

static RSB gen_sort (
    TDBB	tdbb,
    OPT		opt,
    UCHAR	*streams,
    UCHAR	*dbkey_streams,
    RSB		prior_rsb,
    NOD		sort,
    USHORT	project_flag)
{
/**************************************
 *
 *	g e n _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Generate a record source block to handle either a sort or a project.
 *	The two case are virtual identical -- the only difference is that
 *	project eliminates duplicates.  However, since duplicates are
 *	recognized and handled by sort, the JRD processing is identical.
 *
 **************************************/
RSB	rsb;
CSB	csb;
SLONG	id;
ULONG	map_length = 0;
NOD	node, *node_ptr, *end_node;
UCHAR	*ptr, *end_ptr;
USHORT	stream, items, count, flag_offset;
SMB	map;
SKD	*sort_key, *end_key;
DSC	*desc, descriptor;
FMT	format;
LLS	id_stack, stream_stack;
struct smb_repeat	*map_item;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (prior_rsb, type_rsb);
DEV_BLKCHK (sort, type_nod);

SET_TDBB (tdbb);

/* We already know the number of keys, but we also need to compute the
   total number of fields, keys and non-keys, to be pumped thru sort.  Starting
   with the number of keys, count the other field referenced.  Since a field
   is often a key, check for overlap to keep the length of the sort record
   down. */

   /* Along with the record number, the transaction id of the
    * record will also be stored in the sort file.  This will
    * be used to detect update conflict in read committed 
    * transactions.
    */

csb = opt->opt_csb;
items = sort->nod_count + (streams [0] * 2) + (dbkey_streams ? dbkey_streams [0] : 0);
end_ptr = streams + streams [0];
end_node = sort->nod_arg + sort->nod_count;
id_stack = stream_stack = NULL;

for (ptr = &streams [1]; ptr <= end_ptr; ptr++)
    {
    id = -1;
    while (SBM_next (csb->csb_rpt [*ptr].csb_fields, &id, RSE_get_forward))
	{
	items++;
	LLS_PUSH ((BLK) id, &id_stack);
	LLS_PUSH ((BLK) *ptr, &stream_stack);
	for (node_ptr = sort->nod_arg; node_ptr < end_node; node_ptr++)
	    {
	    node = *node_ptr;
	    if (node->nod_type == nod_field &&
		(USHORT) node->nod_arg [e_fld_stream] == *ptr &&
		(USHORT) node->nod_arg [e_fld_id] == id)
		{
		desc = &descriptor;
		CMP_get_desc (tdbb, csb, node, desc);

		/* International type text has a computed key */

		if (IS_INTL_DATA (desc))
		    break;
		--items;
		LLS_POP (&id_stack);
		LLS_POP (&stream_stack);
		break;
		}
	    }
	}
    }

/* Now that we know the number of items, allocate a sort map block.  Allocate
   it sufficiently large that there is room for a sort key descriptor on the
   end. */

count = items + 
	(sizeof (SKD) * 2 * sort->nod_count + sizeof (struct smb_repeat) - 1) /
	sizeof (struct smb_repeat);
map = (SMB) ALLOCDV (type_smb, count);
map->smb_keys = sort->nod_count * 2;
map->smb_count = items;
if (project_flag)
    map->smb_flags |= SMB_project;

/* Loop thru sort keys building sort keys.  Actually, to handle null values
   correctly, two sort keys are made for each field, one for the null flag
   and one for field itself. */

map_item = map->smb_rpt;
sort_key = (SKD*) &map->smb_rpt [items];
map->smb_key_desc = sort_key;

for (node_ptr = sort->nod_arg; node_ptr < end_node; node_ptr++, map_item++)
    {
    /* Pick up sort key expression. */

    node = *node_ptr;
    desc = &descriptor;
    CMP_get_desc (tdbb, csb, node, desc);

    /* Allow for "key" forms of International text to grow */

    if (IS_INTL_DATA (desc))
	{
	/* Turn varying text and cstrings into text. */

    	if (desc->dsc_dtype == dtype_varying)
	    {
	    desc->dsc_dtype = dtype_text;
	    desc->dsc_length -= sizeof (USHORT);
	    }
    	else if (desc->dsc_dtype == dtype_cstring)
	    {
	    desc->dsc_dtype = dtype_text;
	    desc->dsc_length--;
	    }
	desc->dsc_length = INTL_key_length (tdbb, INTL_INDEX_TYPE (desc), desc->dsc_length);
	}

    /* Make key for null flag */

#ifdef VAX
    map_length = ROUNDUP (map_length, sizeof (SLONG));
#endif
    sort_key->skd_offset = map_item->smb_flag_offset = (USHORT) map_length++;
    sort_key->skd_dtype = SKD_text;
    sort_key->skd_length = 1;
    sort_key->skd_flags = SKD_ascending;
    ++sort_key;

    /* Make key for sort key proper */

#ifdef VAX
    map_length = ROUNDUP (map_length, sizeof (SLONG));
#else
    if (desc->dsc_dtype >= dtype_aligned)
	map_length = ALIGN (map_length, type_alignments [desc->dsc_dtype]);
#endif
    sort_key->skd_offset = (USHORT) map_length;
    sort_key->skd_flags = SKD_ascending;
    if (*(node_ptr + sort->nod_count))
	sort_key->skd_flags |= SKD_descending;

    assert (desc->dsc_dtype >= 0 && 
	    desc->dsc_dtype < sizeof (sort_dtypes)/sizeof (sort_dtypes[0]));
    sort_key->skd_dtype = sort_dtypes [desc->dsc_dtype];
    assert (sort_key->skd_dtype != 0);
    if (sort_key->skd_dtype == SKD_varying ||
	sort_key->skd_dtype == SKD_cstring)
	{
	if (desc->dsc_ttype == ttype_binary)
	    sort_key->skd_flags |= SKD_binary;
	}
    sort_key->skd_length = desc->dsc_length;
    ++sort_key;

    map_item->smb_node = node;
    map_item->smb_desc = *desc;
    map_item->smb_desc.dsc_address = (UCHAR*) map_length;
    map_length += desc->dsc_length;
    if (node->nod_type == nod_field)
	{
	map_item->smb_stream = (USHORT) node->nod_arg [e_fld_stream];
	map_item->smb_field_id = (USHORT) node->nod_arg [e_fld_id];
	}
    }

map_length = ROUNDUP (map_length, sizeof (SLONG));
map->smb_key_length = (USHORT) map_length >> SHIFTLONG;
flag_offset = (USHORT) map_length;
map_length += items - sort->nod_count;

/* Now go back and process all to fields involved with the sort.  If the
   field has already been mentioned as a sort key, don't bother to repeat
   it. */

while (stream_stack)
    {
    id = (USHORT) LLS_POP (&id_stack);
    stream = (USHORT) LLS_POP (&stream_stack);
    format = CMP_format (tdbb, csb, stream);
    desc = &format->fmt_desc [id];

    if (id >= format->fmt_count || desc->dsc_length == 0)
	IBERROR (157); /* msg 157 cannot sort on a field that does not exist */
    if (desc->dsc_dtype >= dtype_aligned)
	map_length = ALIGN (map_length, type_alignments [desc->dsc_dtype]);

    map_item->smb_field_id = (SSHORT) id;
    map_item->smb_stream = stream;
    map_item->smb_flag_offset = flag_offset++;
    map_item->smb_desc = *desc;
    map_item->smb_desc.dsc_address = (UCHAR*) map_length;
    map_length += desc->dsc_length;
    map_item++;
    }

/* Make fields for record numbers record for all streams */

map_length = ROUNDUP (map_length, sizeof (SLONG));

for (ptr = &streams [1]; ptr <= end_ptr; ptr++, map_item++)
    {
    map_item->smb_field_id = SMB_DBKEY;
    map_item->smb_stream = *ptr;
    desc = &map_item->smb_desc;
    desc->dsc_dtype = dtype_long;
    desc->dsc_length = sizeof (SLONG);
    desc->dsc_address = (UCHAR*) map_length;
    map_length += desc->dsc_length;
    }

/* Make fields for transaction id of record for all streams */

for (ptr = &streams [1]; ptr <= end_ptr; ptr++, map_item++)
    {
    map_item->smb_field_id = SMB_TRANS_ID;
    map_item->smb_stream = *ptr;
    desc = &map_item->smb_desc;
    desc->dsc_dtype = dtype_long;
    desc->dsc_length = sizeof (SLONG);
    desc->dsc_address = (UCHAR*) map_length;
    map_length += desc->dsc_length;
    }

if (dbkey_streams)
    for (ptr = &dbkey_streams [1], end_ptr = dbkey_streams + dbkey_streams [0]; ptr <= end_ptr; ptr++, map_item++)
	{
	map_item->smb_field_id = SMB_DBKEY;
	map_item->smb_stream = *ptr;
	desc = &map_item->smb_desc;
	desc->dsc_dtype = dtype_long;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_address = (UCHAR*) map_length;
	map_length += desc->dsc_length;
	}

/* Make fields to store varying and cstring length. */

end_key = sort_key;
for (sort_key = map->smb_key_desc; sort_key < end_key; sort_key++)
    {
    assert (sort_key->skd_dtype != 0);
    if (sort_key->skd_dtype == SKD_varying ||
	sort_key->skd_dtype == SKD_cstring)
	{
	sort_key->skd_vary_offset = (USHORT) map_length;
	map_length += sizeof (USHORT);
	}
    }

if (map_length > MAX_SORT_RECORD)
    ERR_post (isc_sort_rec_size_err, isc_arg_number, map_length, 0);
	/* Msg438: sort record size of %ld bytes is too big */

map->smb_length = (USHORT) map_length;

/* That was most unpleasant.  Never the less, it's done (except for
   the debugging).  All that remains is to build the record source
   block for the sort. */

rsb = (RSB) ALLOCDV (type_rsb, 1);
rsb->rsb_type = rsb_sort;
rsb->rsb_next = prior_rsb;
rsb->rsb_arg [0] = (RSB) map;

rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb_sort));

return rsb;
}

static BOOLEAN gen_sort_merge (
    TDBB	tdbb,
    OPT		opt,
    LLS		*org_rivers)
{
/**************************************
 *
 *	g e n _ s o r t _ m e r g e
 *
 **************************************
 *
 * Functional description
 *	We've got a set of rivers that may or may not be amenable to
 *	a sort/merge join, and it's time to find out.  If there are,
 *	build a sort/merge RSB, push it on the rsb stack, and update
 *	rivers accordingly.  If two or more rivers were successfully
 *	joined, return TRUE.  If the whole things is a moby no-op,
 *	return FALSE.
 *
 **************************************/
DBB		dbb;
RIV		river1, river2;
LLS		stack1, stack2;
USHORT		i, cnt, class_cnt, river_cnt, stream_cnt;
ULONG		selected_rivers[OPT_BITS], selected_rivers2[OPT_BITS];
UCHAR		*stream;
VEC		scratch;
NOD		*classes, *class, *selected_classes [MAX_OPT_ITEMS], **selected_class,
		*last_class, node, node1, node2, sort, *ptr;
RSB		rsb, merge_rsb;
RSB		*rsb_tail;
struct opt_repeat *tail, *end;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (*org_rivers, type_lls);

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

/* Count the number of "rivers" involved in the operation, then allocate
   a scratch block large enough to hold values to compute equality
   classes */

for (cnt = 0, stack1 = *org_rivers; stack1; stack1 = stack1->lls_next)
    {
    river1 = (RIV) stack1->lls_object;
    river1->riv_number = cnt++;
    }

scratch = (VEC) ALLOCPV (type_vec, opt->opt_count * cnt);
classes = (NOD*) scratch->vec_object;

/* Compute equivalence classes among streams.  This involves finding groups
   of streams joined by field equalities.  */

last_class = classes;

for (tail = opt->opt_rpt, end = tail + opt->opt_count; tail < end; tail++)
    {
    if (tail->opt_flags & opt_used)
	continue;
    node = tail->opt_conjunct;
    if (node->nod_type != nod_eql)
	continue;
    node1 = node->nod_arg [0];
    node2 = node->nod_arg [1];
    for (stack1 = *org_rivers; stack1; 	 stack1 = stack1->lls_next)
	{
	river1 = (RIV) stack1->lls_object;
	if (!river_reference (river1, node1))
	    if (river_reference (river1, node2))
		{
		node = node1;
		node1 = node2;
		node2 = node;
		}
	    else
		continue;
	for (stack2 = stack1->lls_next; stack2; stack2 = stack2->lls_next)
	    {
	    river2 = (RIV) stack2->lls_object;
	    if (river_reference (river2, node2))
		{
		for (class = classes; class < last_class; class += cnt)
		    if (node_equality (node1, classes [river1->riv_number]) ||
		        node_equality (node2, classes [river2->riv_number]))
			break;
		class [river1->riv_number] = node1;
		class [river2->riv_number] = node2;
		if (class == last_class)
		    last_class += cnt;
		}
	    }
	}
    }
	
/* Pick both a set of classes and a set of rivers on which to join with
   sort merge.  Obviously, if the set of classes is empty, return FALSE
   to indicate that nothing could be done. */

river_cnt = stream_cnt = 0;

for (class = classes; class < last_class; class += cnt)
    {
    i = river_count (cnt, class);
    if (i > river_cnt)
	{
	river_cnt = i;
	selected_class = selected_classes;
	*selected_class++ = class;
	class_mask (cnt, class, selected_rivers);
	}
    else
	{
	class_mask(cnt, class, selected_rivers2);
	for (i = 0; i < OPT_BITS; i++)
	    if ((selected_rivers[i] & selected_rivers2[i]) != selected_rivers[i])
		break;
 	if (i == OPT_BITS)
	    *selected_class++ = class;
	}
    }

if (!river_cnt)
    {
    ALL_RELEASE (scratch);
    return FALSE;
    }

*selected_class = NULL;
class_cnt = selected_class - selected_classes;

/* Build a sort stream */

merge_rsb = (RSB) ALLOCDV (type_rsb, river_cnt * 2);
merge_rsb->rsb_count = river_cnt;
merge_rsb->rsb_type = rsb_merge;
merge_rsb->rsb_impure = CMP_impure (opt->opt_csb,
	(USHORT) (sizeof (struct irsb_mrg) + river_cnt * sizeof (struct irsb_mrg_repeat)));

rsb_tail = merge_rsb->rsb_arg;
stream_cnt = 0;

for (stack1 = *org_rivers; stack1; stack1 = stack1->lls_next)
    {
    river1 = (RIV) stack1->lls_object;
    if (!(TEST_DEP_BIT(selected_rivers, river1->riv_number)))
	continue;
    stream_cnt += river1->riv_count;
    sort = (NOD) ALLOCDV (type_nod, class_cnt * 2);
    sort->nod_type = nod_sort;
    sort->nod_count = class_cnt;
    for (selected_class = selected_classes, ptr = sort->nod_arg; 
	 *selected_class; selected_class++)
	*ptr++ = (*selected_class) [river1->riv_number];
    rsb = gen_sort (tdbb, opt, &river1->riv_count, NULL, river1->riv_rsb, sort, FALSE);
    *rsb_tail++ = rsb;
    *rsb_tail++ = (RSB) sort;
    }

/* Finally, merge selected rivers into a single river, and rebuild original 
   river stack */

river1 = (RIV) ALLOCDV (type_riv, stream_cnt);
river1->riv_count = (UCHAR) stream_cnt;
river1->riv_rsb = merge_rsb;
stream = river1->riv_streams;
stack1 = NULL;
LLS_PUSH (river1, &stack1);

while (*org_rivers)
    {
    river2 = (RIV) LLS_POP (org_rivers);
    if (TEST_DEP_BIT(selected_rivers, river2->riv_number))
	{
	MOVE_FAST (river2->riv_streams, stream, river2->riv_count);
	stream += river2->riv_count;
	}
    else
	LLS_PUSH (river2, &stack1);
    }

/* Pick up any boolean that may apply */

set_active (opt, river1);
node = NULL;

for (tail = opt->opt_rpt; tail < end; tail++)
    {
    node1 = tail->opt_conjunct;
    if (!(tail->opt_flags & opt_used) && computable (opt->opt_csb, node1, -1, FALSE))
	{
	compose (&node, node1, nod_and);
	tail->opt_flags |= opt_used;
	}
    }

if (node)
    river1->riv_rsb = gen_boolean (tdbb, opt, river1->riv_rsb, node);

set_inactive (opt, river1);
*org_rivers = stack1;
ALL_RELEASE (scratch);

return TRUE;
}

static RSB gen_union (
    TDBB	tdbb,
    OPT		opt,
    NOD		union_node,
    UCHAR	*streams,
    USHORT	nstreams)
{
/**************************************
 *
 *	g e n _ u n i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a union complex.
 *
 **************************************/
RSB	rsb, *rsb_ptr;
USHORT	count;
NOD	clauses, *ptr, *end;
CSB	csb;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (union_node, type_nod);

SET_TDBB (tdbb);

clauses = union_node->nod_arg [e_uni_clauses];
count = clauses->nod_count;
csb = opt->opt_csb;

rsb = (RSB) ALLOCDV (type_rsb, count + nstreams + 1);
rsb->rsb_type = rsb_union;
rsb->rsb_count = count;
rsb->rsb_stream = (UCHAR) union_node->nod_arg [e_uni_stream];
rsb->rsb_format = csb->csb_rpt [rsb->rsb_stream].csb_format;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb));

rsb_ptr = rsb->rsb_arg;

for (ptr = clauses->nod_arg, end = ptr + count; ptr < end;)
    {
    *rsb_ptr++ = OPT_compile (tdbb, csb, (RSE) *ptr++, NULL);
    *rsb_ptr++ = (RSB) *ptr++;
    }

/* Save the count and numbers of the streams that make up the union */

*rsb_ptr++ = (RSB) nstreams;
while (nstreams--)
    *rsb_ptr++ = (RSB) *streams++;

return rsb;
}

static void get_inactivities (
    CSB		csb,
    ULONG	*dependencies)
{
/**************************************
 *
 *	g e t _ i n a c t i v i t i e s
 *
 **************************************
 *
 * Functional description
 *	Find any streams not explicitily active.
 *
 **************************************/
USHORT	n;
struct csb_repeat	*tail, *end;

DEV_BLKCHK (csb, type_csb);

for (n = 0; n < OPT_BITS; n++)
    dependencies[n] = (ULONG) -1;

for (tail = csb->csb_rpt, end = tail + csb->csb_n_stream, n = 0; 
     tail < end; n++, tail++)
    if (tail->csb_flags & csb_active)
	CLEAR_DEP_BIT (dependencies, n);
}

static IRL indexed_relationship (
    TDBB	tdbb,
    OPT		opt,
    USHORT	stream)
{
/**************************************
 *
 *	i n d e x e d _ r e l a t i o n s h i p
 *
 **************************************
 *
 * Functional description
 *	See if two streams are related by an index.
 *	An indexed relationship is a means of joining two 
 *	streams via an index, which is possible when a field from
 *	each of the streams is compared with a field from the other,
 *	and there is an index on one stream to retrieve the value
 *	of the other field.
 *
 **************************************/
CSB		csb;
IDX		*idx;
NOD		node;
IRL		relationship;
USHORT		i;
register struct opt_repeat	*tail, *opt_end;
struct csb_repeat		*csb_tail;

DEV_BLKCHK (opt, type_opt);

SET_TDBB (tdbb);

if (!opt->opt_count)
    return NULL;

csb = opt->opt_csb;
csb_tail = &csb->csb_rpt [stream];
opt_end = &opt->opt_rpt [opt->opt_count];
relationship = NULL;

/* Loop thru indexes looking for a match */

idx = csb_tail->csb_idx;

for (i = 0; i < csb_tail->csb_indices; i++, idx = NEXT_IDX (idx->idx_rpt, idx->idx_count))
    {
    /* skip this part if the index wasn't specified for indexed retrieval */

    if (idx->idx_runtime_flags & idx_plan_dont_use)
        continue;

    clear_bounds (opt, idx);
    for (tail = opt->opt_rpt; tail < opt_end; tail++)
	{
	node = tail->opt_conjunct;
	if (!(tail->opt_flags & opt_used) && computable (csb, node, -1, FALSE))
	    match_index (tdbb, opt, stream, node, idx);
	}

    tail = opt->opt_rpt;
    if (tail->opt_lower || tail->opt_upper)
	{
	if (!relationship)
	    relationship = (IRL) ALLOCD (type_irl);
	if (idx->idx_flags & idx_unique)
	    {
	    relationship->irl_unique = TRUE;
	    break;
	    }
	}
    }

return relationship;
}

static STR make_alias (
    TDBB		tdbb,
    CSB			csb,
    struct csb_repeat	*base_tail)
{
/**************************************
 *
 *	m a k e _ a l i a s
 *
 **************************************
 *
 * Functional description
 *	Make an alias string suitable for printing
 *	as part of the plan.  For views, this means
 *	multiple aliases to distinguish the base 
 *	table.
 *
 **************************************/
STR	alias;
USHORT	alias_length = 0;
TEXT	*p, *q;
struct csb_repeat *csb_tail;

DEV_BLKCHK (csb, type_csb);

SET_TDBB (tdbb);

if (!base_tail->csb_view &&
    !base_tail->csb_alias)
    return NULL;

/* calculate the length of the alias by going up through
   the view stack to find the lengths of all aliases;
   adjust for spaces and a null terminator */

for (csb_tail = base_tail; ; csb_tail = &csb->csb_rpt [csb_tail->csb_view_stream])
    {
    if (csb_tail->csb_alias)
        alias_length += csb_tail->csb_alias->str_length;
    else
	alias_length += 
	    (!(csb_tail->csb_relation) || !(csb_tail->csb_relation ->rel_name))
	    ? 0 : strlen (csb_tail->csb_relation->rel_name);

    alias_length++;

    if (!csb_tail->csb_view)
	break;
    }
                      
/* allocate a string block to hold the concatenated alias */

alias = (STR) ALLOCDV (type_str, alias_length);
alias->str_length = alias_length - 1;

/* now concatenate the individual aliases into the string block, 
   beginning at the end and copying back to the beginning */

p = (TEXT*) alias->str_data + alias->str_length;
*p-- = 0;

for (csb_tail = base_tail; ; csb_tail = &csb->csb_rpt [csb_tail->csb_view_stream])
    {
    if (csb_tail->csb_alias)
	q = (TEXT*) csb_tail->csb_alias->str_data;
    else
	q = (!(csb_tail->csb_relation) || !(csb_tail->csb_relation ->rel_name))
	    ? NULL : csb_tail->csb_relation->rel_name;

    /* go to the end of the alias and copy it backwards */

    if (q) 
        {
	for (alias_length = 0; *q; alias_length++)
	    q++;
        while (alias_length--)
	    *p-- = *--q;
	}

    if (!csb_tail->csb_view)
	break;
    *p-- = ' ';
    }          

return alias;
}

static NOD make_binary_node (
    NOD_T	type,
    NOD		arg1,
    NOD		arg2,
    USHORT	flag)
{
/**************************************
 *
 *	m a k e _ b i n a r y _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Make a binary node.
 *
 **************************************/
NOD	node;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;


DEV_BLKCHK (arg1, type_nod);
DEV_BLKCHK (arg2, type_nod);
        
node = PAR_make_node (tdbb, 2);
node->nod_type = type;
node->nod_arg [0] = arg1;
node->nod_arg [1] = arg2;

if (flag)
    node->nod_flags |= nod_comparison;

return node;
}

static RSB make_cross (
    TDBB	tdbb,
    OPT		opt,
    LLS		stack)
{
/**************************************
 *
 *	m a k e _ c r o s s
 *
 **************************************
 *
 * Functional description
 *	Generate a cross block.
 *
 **************************************/
RIV	river;
RSB	rsb, *ptr;
CSB	csb;
LLS	temp;
USHORT	count;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (stack, type_lls);

SET_TDBB (tdbb);

for (temp = stack, count = 0; temp; count++)
    temp = temp->lls_next;

if (count == 1)
    {
    river = (RIV) LLS_POP (&stack);
    return river->riv_rsb;
    }

csb = opt->opt_csb;
rsb = (RSB) ALLOCDV (type_rsb, count);
rsb->rsb_type = rsb_cross;
rsb->rsb_count = count;
rsb->rsb_impure = CMP_impure (csb, sizeof (struct irsb));
ptr = rsb->rsb_arg + count;

while (stack)
    {
    river = (RIV) LLS_POP (&stack);
    *--ptr = river->riv_rsb;
    }

return rsb;
}

static NOD make_index_node (
    TDBB	tdbb,
    REL		relation,
    CSB		csb,
    IDX		*idx)
{
/**************************************
 *
 *	m a k e _ i n d e x _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Make an index node and an index retrieval block.
 *
 **************************************/
NOD	node;
IRB	retrieval;

DEV_BLKCHK (relation, type_rel);
DEV_BLKCHK (csb, type_csb);

SET_TDBB (tdbb);

/* check whether this is during a compile or during
   a SET INDEX operation */

if (csb)
    CMP_post_resource (tdbb, &csb->csb_resources, relation, rsc_index, idx->idx_id);
else 
    CMP_post_resource (tdbb, &tdbb->tdbb_request->req_resources, relation, rsc_index, idx->idx_id);

node = PAR_make_node (tdbb, e_idx_length);
node->nod_type = nod_index;
node->nod_count = 0;
retrieval= (IRB) ALLOCDV (type_irb, idx->idx_count * 2);
node->nod_arg [e_idx_retrieval] = (NOD) retrieval;
retrieval->irb_index = idx->idx_id;
MOVE_FAST (idx, &retrieval->irb_desc, sizeof (retrieval->irb_desc));

if (csb)
    node->nod_impure = CMP_impure (csb, sizeof (struct inv));

return node;
}

static NOD make_inference_node (
    CSB		csb,
    NOD		boolean,
    NOD		arg1,
    NOD		arg2)
{
/**************************************
 *
 *	m a k e _ i n f e r e n c e _ n o d e
 *
 **************************************
 *
 * Defined
 *	1996-Jan-15 David Schnepper 
 *
 * Functional description
 *	From the predicate, boolean, and infer a new
 *	predicate using arg1 & arg2 as the first two
 *	parameters to the predicate.
 *
 *	This is used when the engine knows A<B and A=C, and
 *	creates a new node to represent the infered knowledge C<B.
 *
 *	Note that this may be sometimes incorrect with 3-value
 *	logic (per Chris Date's Object & Relations seminar).
 *	Later stages of query evaluation evaluate exactly
 *	the originally specified query, so 3-value issues are
 *	caught there.  Making this inference might cause us to
 *	examine more records than needed, but would not result
 *	in incorrect results.
 *
 *	Note that some nodes, specifically nod_like, have
 *	more than two parameters for a boolean operation.
 *	(nod_like has an optional 3rd parameter for the ESCAPE character
 *	 option of SQL)
 *	Nod_sleuth also has an optional 3rd parameter (for the GDML
 *	matching ESCAPE character language).  But nod_sleuth is 
 *	(apparently) not considered during optimization.
 *
 *
 **************************************/
NOD	node;
USHORT	n;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;


DEV_BLKCHK (csb, type_csb);
DEV_BLKCHK (boolean, type_nod);
DEV_BLKCHK (arg1, type_nod);
DEV_BLKCHK (arg2, type_nod);

assert (boolean->nod_count >= 2);	/* must be a conjunction boolean */
        
/* Clone the input predicate */
node = PAR_make_node (tdbb, boolean->nod_count);
node->nod_type = boolean->nod_type;
node->nod_flags = boolean->nod_flags;

/* But substitute new values for some of the predicate arguments */
node->nod_arg [0] = CMP_clone_node (tdbb, csb, arg1);
node->nod_arg [1] = CMP_clone_node (tdbb, csb, arg2);

/* Arguments after the first two are just cloned (eg: LIKE ESCAPE clause) */
for (n = 2; n < boolean->nod_count; n++)
    node->nod_arg [n] = CMP_clone_node (tdbb, csb, boolean->nod_arg [n]);

return node;
}

static NOD make_inversion (
    TDBB		tdbb,
    register OPT	opt,
    NOD			boolean,
    USHORT		stream)
{
/**************************************
 *
 *	m a k e _ i n v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Build an inversion for a boolean, if possible.  Otherwise,
 *	return NULL.  Make inversion is call initially from 
 *	gen_retrieval to handle "or" nodes, but may be called
 *	recursively for almost anything.
 *
 **************************************/
REL		relation;
IDX		*idx;
NOD		inversion, inversion2;
SSHORT		i;
struct csb_repeat		*csb_tail;

SET_TDBB (tdbb);

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (boolean, type_nod);

csb_tail = &opt->opt_csb->csb_rpt [stream];
relation = csb_tail->csb_relation;

if ((!relation) || (relation->rel_file))
    return NULL;

/* Handle the "OR" case up front */

if (boolean->nod_type == nod_or)
    {
    if (!(inversion = make_inversion (tdbb, opt, boolean->nod_arg [0], stream)))
	return NULL;
    if (inversion2 = make_inversion (tdbb, opt, boolean->nod_arg [1], stream))
	return compose (&inversion, inversion2, nod_bit_or);
    if (inversion->nod_type == nod_index)
	ALL_RELEASE (inversion->nod_arg [e_idx_retrieval]);
    ALL_RELEASE (inversion);
    return NULL;
    }

/* Time to find inversions.  For each index on the relation
   match all unused booleans against the index looking for upper
   and lower bounds that can be computed by the index.  When
   all unused conjunctions are exhausted, see if there is enough
   information for an index retrieval.  If so, build up and
   inversion component of the boolean. */

idx = csb_tail->csb_idx;
inversion = NULL;

if (opt->opt_count)
    for (i = 0; i < csb_tail->csb_indices; i++)
	{
	clear_bounds (opt, idx);

	/* skip this part if the index wasn't specified for indexed 
	   retrieval (still need to look for navigational retrieval) */

	if (idx->idx_runtime_flags & idx_plan_dont_use) 
	    continue;

	match_index (tdbb, opt, stream, boolean, idx);
	if (opt->opt_rpt [0].opt_lower || opt->opt_rpt [0].opt_upper)
	    compose (&inversion, OPT_make_index (tdbb, opt, relation, idx), nod_bit_and);
	if (boolean->nod_type == nod_starts)
	    compose (&inversion, 
		    make_starts (tdbb, opt, relation, boolean, stream, idx), nod_bit_and);
	if (boolean->nod_type == nod_missing)
	    compose (&inversion, 
		    make_missing (tdbb, opt, relation, boolean, stream, idx), nod_bit_and);
	idx = NEXT_IDX (idx->idx_rpt, idx->idx_count);
	}
	
if (!inversion)
    inversion = OPT_make_dbkey (opt, boolean, stream);

return inversion;
}

static NOD make_missing (
    TDBB		tdbb,
    register OPT	opt,
    REL			relation,
    NOD			boolean,
    USHORT		stream,
    IDX			*idx)
{
/**************************************
 *
 *	m a k e _ m i s s i n g
 *
 **************************************
 *
 * Functional description
 *	If the a given boolean is an index optimizable, build and 
 *	return a inversion type node.  Indexes built before minor
 *	version 3 (V3.2) have unreliable representations for missing
 *	character string fields, so they won't be used.
 *
 **************************************/
NOD	field, node, value;
IRB	retrieval;

SET_TDBB (tdbb);


DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (relation, type_rel);
DEV_BLKCHK (boolean, type_nod);

field = boolean->nod_arg [0];

if (field->nod_type != nod_field)
    return NULL;

if ((USHORT) field->nod_arg [e_fld_stream] != stream ||
    (USHORT) field->nod_arg [e_fld_id] != idx->idx_rpt [0].idx_field)
    return NULL;

node = make_index_node (tdbb, relation, opt->opt_csb, idx);
retrieval = (IRB) node->nod_arg [e_idx_retrieval];
retrieval->irb_relation = relation;
retrieval->irb_generic = irb_starting;
retrieval->irb_lower_count = retrieval->irb_upper_count = 1;
/* If we are matching less than the full index, this is a partial match */
if (retrieval->irb_upper_count < idx->idx_count)
    retrieval->irb_generic |= irb_partial;
retrieval->irb_value [0] = retrieval->irb_value [idx->idx_count] = value = PAR_make_node (tdbb, 0);
value->nod_type = nod_null;
idx->idx_runtime_flags |= idx_plan_missing;

return node;
}

static NOD make_starts (
    TDBB		tdbb,
    register OPT	opt,
    REL			relation,
    NOD			boolean,
    USHORT		stream,
    IDX			*idx)
{
/**************************************
 *
 *	m a k e _ s t a r t s
 *
 **************************************
 *
 * Functional description
 *	If the given boolean is an index optimizable, build and 
 *	return a inversion type node.
 *
 **************************************/
NOD	value, field, node;
IRB	retrieval;

SET_TDBB (tdbb);

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (relation, type_rel);
DEV_BLKCHK (boolean, type_nod);

if (boolean->nod_type != nod_starts)
    return NULL;

field = boolean->nod_arg [0];
value = boolean->nod_arg [1];

if (field->nod_type != nod_field)
    {
    if (value->nod_type != nod_field)
	return NULL;
    field = value;
    value = boolean->nod_arg [0];
    }

/* Every string starts with an empty string so
   don't bother using an index in that case. */

if (value->nod_type == nod_literal)
    {
    DSC	*literal_desc;

    literal_desc = &((LIT) value)->lit_desc;
    if ((literal_desc->dsc_dtype == dtype_text &&
	 literal_desc->dsc_length == 0) ||
	(literal_desc->dsc_dtype == dtype_varying &&
	 literal_desc->dsc_length == sizeof (USHORT)))
	return NULL;
    }

if ((USHORT) field->nod_arg [e_fld_stream] != stream ||
    (USHORT) field->nod_arg [e_fld_id] != idx->idx_rpt [0].idx_field ||
    !(idx->idx_rpt [0].idx_itype == idx_string     ||
      idx->idx_rpt [0].idx_itype == idx_byte_array ||
      idx->idx_rpt [0].idx_itype == idx_metadata   ||
      idx->idx_rpt [0].idx_itype >= idx_first_intl_string) ||
    !computable (opt->opt_csb, value, stream, FALSE))
    return NULL;

node = make_index_node (tdbb, relation, opt->opt_csb, idx);
retrieval = (IRB) node->nod_arg [e_idx_retrieval];
retrieval->irb_relation = relation;
retrieval->irb_generic = irb_starting;
retrieval->irb_lower_count = retrieval->irb_upper_count = 1;
/* If we are matching less than the full index, this is a partial match */
if (retrieval->irb_upper_count < idx->idx_count)
    retrieval->irb_generic |= irb_partial;
retrieval->irb_value [0] = retrieval->irb_value [idx->idx_count] = value;
idx->idx_runtime_flags |= idx_plan_starts;

return node;
}

static BOOLEAN map_equal (
    NOD		field1, 
    NOD		field2, 
    NOD		map)
{
/**************************************
 *
 *	m a p _ e q u a l
 *
 **************************************
 *
 * Functional description
 *	Test to see if two fields are equal, where the fields 
 *	are in two different streams possibly mapped to each other.
 *	Order of the input fields is important.
 *
 **************************************/  
NOD	*map_ptr, *map_end, map_from, map_to;

DEV_BLKCHK (field1, type_nod);
DEV_BLKCHK (field2, type_nod);
DEV_BLKCHK (map, type_nod);

if (field1->nod_type != nod_field)
    return FALSE;

if (field2->nod_type != nod_field)
    return FALSE;

/* look through the mapping and see if we can find an equivalence */

for (map_ptr = map->nod_arg, map_end = map_ptr + map->nod_count; map_ptr < map_end; map_ptr++)
    {
    map_from = (*map_ptr)->nod_arg [e_asgn_from];
    map_to = (*map_ptr)->nod_arg [e_asgn_to];

    if (map_from->nod_type != nod_field || 
        map_to->nod_type != nod_field)
        continue; 

    if (field1->nod_arg [e_fld_stream] != map_from->nod_arg [e_fld_stream] ||
        field1->nod_arg [e_fld_id] != map_from->nod_arg [e_fld_id])
        continue;

    if (field2->nod_arg [e_fld_stream] != map_to->nod_arg [e_fld_stream] ||
	field2->nod_arg [e_fld_id] != map_to->nod_arg [e_fld_id])
	continue;

    return TRUE;
    }

return FALSE;
}


static void mark_indices (
    struct csb_repeat	*csb_tail,
    SSHORT		relation_id)
{
/**************************************
 *
 *	m a r k _ i n d i c e s
 *
 **************************************
 *
 * Functional description
 *	Mark indices that were not included 
 *	in the user-specified access plan.
 *
 **************************************/
NOD	access_type, plan, *arg, *end;
IDX	*idx;
USHORT	i, plan_count = 0;

if (!(plan = csb_tail->csb_plan))
    return;
         
if (plan->nod_type != nod_retrieve)
    return;

/* find out how many indices were specified; if
   there were none, this is a sequential retrieval */

if (access_type = plan->nod_arg [e_retrieve_access_type])
    plan_count = access_type->nod_count;

/* go through each of the indices and mark it unusable
   for indexed retrieval unless it was specifically mentioned 
   in the plan; also mark indices for navigational access */

idx = csb_tail->csb_idx;
for (i = 0; i < csb_tail->csb_indices; i++)
    {
    if (access_type)
	{
        for (arg = access_type->nod_arg, end = arg + plan_count; arg < end; arg += 3)
	    {
            if (relation_id != (SSHORT) *arg)
		/* index %s cannot be used in the specified plan */
		ERR_post (gds__index_unused, gds_arg_string, *(arg + 2), 0);

	    if (idx->idx_id == (USHORT) *(arg + 1))
		if (access_type->nod_type == nod_navigational)
		    idx->idx_runtime_flags |= idx_plan_navigate;
		else /* nod_indices */
		    break;
	    }

        if (arg == end)
            idx->idx_runtime_flags |= idx_plan_dont_use;
	}
    else
        idx->idx_runtime_flags |= idx_plan_dont_use;

    idx = NEXT_IDX (idx->idx_rpt, idx->idx_count);
    }
}

static SSHORT match_index (
    TDBB		tdbb, 
    register OPT	opt,
    SSHORT		stream,
    register NOD	boolean,
    IDX			*idx)
{
/**************************************
 *
 *	m a t c h _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Match a boolean against an index location lower and upper
 *	bounds.  Return the number of relational nodes that were
 *	matched.  In ODS versions prior to 7, descending indexes
 *	were not reliable and will not be used.
 *
 **************************************/
NOD		match, value;
SSHORT		i, forward, count;
struct opt_repeat	*ptr;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (boolean, type_nod);

SET_TDBB (tdbb);

if (boolean->nod_count < 2)
    return 0;

if (boolean->nod_type == nod_and)
    return match_index (tdbb, opt, stream, boolean->nod_arg [0], idx) +
	   match_index (tdbb, opt, stream, boolean->nod_arg [1], idx);

forward = TRUE;
count = 0;

match = boolean->nod_arg [0];
value = boolean->nod_arg [1];

#ifdef EXPRESSION_INDICES 
if (idx->idx_expression)
    {
    /* see if one side or the other is matchable to the index expression */

    if (!expression_equal (tdbb, idx->idx_expression, match) ||
        !computable (opt->opt_csb, value, stream, TRUE))
	{
        if (expression_equal (tdbb, idx->idx_expression, value) && 
            computable (opt->opt_csb, match, stream, TRUE))
            {
	    match = boolean->nod_arg [1]; 
	    value = boolean->nod_arg [0]; 
	    }
        else
            return 0;
	}
    }
else 
    {
#endif
    /* If left side is not a field, swap sides. 
       If left side is still not a field, give up */

    if (match->nod_type != nod_field || 
        (USHORT) match->nod_arg [e_fld_stream] != stream ||
        !computable (opt->opt_csb, value, stream, TRUE))
        {
        match = value;
        value = boolean->nod_arg [0];
        if (match->nod_type != nod_field || 
	    (USHORT) match->nod_arg [e_fld_stream] != stream ||
	    !computable (opt->opt_csb, value, stream, TRUE))
	    return 0;
        forward = FALSE;
        }
#ifdef EXPRESSION_INDICES
    }
#endif

/* match the field to an index, if possible, and save the value to be matched 
   as either the lower or upper bound for retrieval, or both */

for (i = 0, ptr = opt->opt_rpt; i < idx->idx_count; i++, ptr++)
    if 
#ifdef EXPRESSION_INDICES
       (idx->idx_expression ||
#endif
        ((USHORT) match->nod_arg [e_fld_id] == idx->idx_rpt [i].idx_field)
#ifdef EXPRESSION_INDICES
       )
#endif
    {
    ++count;
    switch (boolean->nod_type)
        {
        case nod_between:
	    if (!forward || 
	        !computable (opt->opt_csb, boolean->nod_arg [2], stream, TRUE))
	        return 0;
	    ptr->opt_lower = value;
	    ptr->opt_upper = boolean->nod_arg [2];
	    ptr->opt_match = boolean;
	    break;

        case nod_eql:
 	    ptr->opt_lower = ptr->opt_upper = value;
	    ptr->opt_match = boolean;
	    break;
	    
        case nod_gtr:
        case nod_geq:
	    if (forward)
	        ptr->opt_lower = value;
	    else
	        ptr->opt_upper = value;
 	    ptr->opt_match = boolean;
	    break;

        case nod_lss:
        case nod_leq:
	    if (forward)
	        ptr->opt_upper = value;
	    else
	        ptr->opt_lower = value;
	    ptr->opt_match = boolean;
	    break;
        }
    }

return count;
}

static USHORT nav_rsb_size (
    RSB		rsb,
    USHORT	key_length,
    USHORT	size)
{
/**************************************
 *
 *	n a v _ r s b _ s i z e
 *
 **************************************
 *
 * Functional description
 *	Calculate the size of a navigational rsb.  
 *
 **************************************/
    
DEV_BLKCHK (rsb, type_rsb);

#ifdef SCROLLABLE_CURSORS
/* allocate extra impure area to hold the current key, 
   plus an upper and lower bound key value, for a total 
   of three times the key length for the index */

size += sizeof (struct irsb_nav) + 3 * key_length;
#else
size += sizeof (struct irsb_nav) + 2 * key_length;
#endif
size = ALIGN (size, ALIGNMENT);

/* make room for an idx structure to describe the index
   that was used to generate this rsb */

if (rsb->rsb_type == rsb_navigate)
    rsb->rsb_arg [RSB_NAV_idx_offset] = (RSB) (SLONG) size;
size += sizeof (struct idx);

return size;
}

static BOOLEAN node_equality (
    NOD		node1,
    NOD		node2)
{
/**************************************
 *
 *	n o d e _ e q u a l i t y
 *
 **************************************
 *
 * Functional description
 *	Test two field node pointers for symbolic equality.
 *
 **************************************/

DEV_BLKCHK (node1, type_nod);
DEV_BLKCHK (node2, type_nod);

if (!node1 || !node2)
    return FALSE;

if (node1->nod_type != node2->nod_type)
    return FALSE;

if (node1 == node2)
    return TRUE;

switch (node1->nod_type)
    {
    case nod_field:
	return (node1->nod_arg [e_fld_stream] == node2->nod_arg [e_fld_stream] &&
		node1->nod_arg [e_fld_id] == node2->nod_arg [e_fld_id]);
    
    case nod_eql:
	if (node_equality (node1->nod_arg [0], node2->nod_arg [0]) &&
	    node_equality (node1->nod_arg [1], node2->nod_arg [1])) 
	    return TRUE;
	if (node_equality (node1->nod_arg [0], node2->nod_arg [1]) &&
	    node_equality (node1->nod_arg [1], node2->nod_arg [0])) 
	    return TRUE;
	return FALSE;

    case nod_gtr:
    case nod_geq:
    case nod_leq:
    case nod_lss:
    case nod_matches:
    case nod_contains:
    case nod_like:
    default:
	break;
    }
	
return FALSE;
}

static NOD optimize_like (
    TDBB	tdbb,
    NOD		like_node)
{
/**************************************
 *
 *	o p t i m i z e _ l i k e
 *
 **************************************
 *
 * Functional description
 *	Optimize a LIKE expression, if possible,
 *	into a "starting with" AND a "like".  This
 *	will allow us to use the index for the
 *	starting with, and the LIKE can just tag
 *	aSLONG for the ride.
 *	But on the ride it does useful work, consider
 *	match LIKE "ab%c".  This is optimized by adding
 *	AND starting_with "ab", but the LIKE clause is
 *	still needed.
 *
 **************************************/
NOD	search_node, escape_node, node;
DSC	*search_desc, *escape_desc;
UCHAR	*p, *end, *q, *p_start;
SSHORT	count;
LIT	literal;
USHORT	ch, escape_ch;
USHORT	p_count;
UCHAR	tmp_buffer [32];	/* large enough to hold 1 ch of escape string */
BLK	text_obj;

SET_TDBB (tdbb);

DEV_BLKCHK (like_node, type_nod);

search_node = like_node->nod_arg [1];
escape_node = (like_node->nod_count > 2) ? like_node->nod_arg [2] : NULL;
                
/* if the matching string or the escape string can't 
   be evaluated at compile time, forget it */

if ((search_node->nod_type != nod_literal) ||
    (escape_node && escape_node->nod_type != nod_literal))
    return NULL;

search_desc = &((LIT) search_node)->lit_desc;
if (escape_node)
    escape_desc = &((LIT) escape_node)->lit_desc;

/* if either is not a character expression, forget it */                

if ((search_desc->dsc_dtype > dtype_any_text) ||
    (escape_node && escape_desc->dsc_dtype > dtype_any_text))
    return NULL;

/* Get the escape character, if any */

text_obj = NULL;
if (escape_node)
    {
    /* Ensure escape string is same character set as search string 
     * Error report on Overflow is OK, as sql only allows a single
     * character escape string */

    p_count = MOV_make_string (escape_desc, INTL_TTYPE (search_desc), &p, tmp_buffer, sizeof(tmp_buffer));

    /* Now get first character from escape string */

    escape_ch = INTL_getch (tdbb, &text_obj, INTL_TTYPE (search_desc), &p, &p_count);
    }

/* if the first character is a wildcard char, forget it */

p = search_desc->dsc_address;
p_count = search_desc->dsc_length;
ch = INTL_getch (tdbb, &text_obj, INTL_TTYPE (search_desc), &p, &p_count);
if ((!escape_node || ch != escape_ch) &&
    (ch == SQL_MATCH_1_CHAR || ch == SQL_MATCH_ANY_CHARS))
    return NULL;
                 
/* allocate a literal node to store the starting with string;
   assume it will be shorter than the search string */

count = lit_delta + (search_desc->dsc_length + sizeof (int) - 1) / sizeof (int);
node = PAR_make_node (tdbb, count);
node->nod_type = nod_literal;
node->nod_count = 0;

literal = (LIT) node;
literal->lit_desc = *search_desc;
literal->lit_desc.dsc_address = q = literal->lit_data;

/* copy the string into the starting with literal, up to the first wildcard character */

p_count = search_desc->dsc_length;
for (p = search_desc->dsc_address, end = p + search_desc->dsc_length; p < end;)
    {
    /* if there are escape characters, skip past them and 
       don't treat the next char as a wildcard */

    p_start = p;
    ch = INTL_getch (tdbb, &text_obj, INTL_TTYPE (search_desc), &p, &p_count);
    if (escape_node && (ch == escape_ch))
	{
	/* Check for Escape character at end of string */

	if (!(p < end))
	    break;
        p_start = p;
    	ch = INTL_getch (tdbb, &text_obj, INTL_TTYPE (search_desc), &p, &p_count);
	}
    else if (ch == SQL_MATCH_1_CHAR || ch == SQL_MATCH_ANY_CHARS)
	break;

    /* copy the bytes of the character */
    while (p_start < p)
       *q++ = *p_start++;
    }

literal->lit_desc.dsc_length = q - literal->lit_desc.dsc_address;
                       
return node;
}


#ifdef OPT_DEBUG
static void print_order (
    OPT		opt,
    USHORT	position,
    double	cardinality,
    double	cost)
{
/**************************************
 *
 *	p r i n t _ o r d e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
struct opt_repeat	*tail, *order_end;

DEV_BLKCHK (opt, type_opt);

order_end = opt->opt_rpt + position;
ib_fprintf (opt_debug_file, "print_order() -- position %2.2d: ", position);

for (tail = opt->opt_rpt; tail < order_end; tail++)
    ib_fprintf (opt_debug_file, "stream %2.2d, ", tail->opt_stream);

ib_fprintf (opt_debug_file, "\n\t\t\tcardinality: %g\tcost: %g\n", cardinality, cost);
}
#endif

static USHORT river_count (
    USHORT	count,
    NOD		*class)
{
/**************************************
 *
 *	r i v e r _ c o u n t
 *
 **************************************
 *
 * Functional description
 *	Given an sort/merge join equivalence class (vector of node pointers
 *	of representative values for rivers), return the count of rivers
 *	with values.
 *
 **************************************/
USHORT	i, cnt;

#ifdef DEV_BUILD
if (*class)
    DEV_BLKCHK (*class, type_nod);
#endif

cnt = 0;

for (i = 0; i < count; i++, class++)
    if (*class)
	{
	cnt++;
	DEV_BLKCHK (*class, type_nod);
	}

return cnt;
}

static BOOLEAN river_reference (
    RIV		river,
    NOD		node)
{
/**************************************
 *
 *	r i v e r _ r e f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	See if a value node is a reference to a given river.  Return
 *	TRUE or FALSE.
 *
 **************************************/
UCHAR	*streams, *end;

DEV_BLKCHK (river, type_riv);
DEV_BLKCHK (node, type_nod);

if (node->nod_type != nod_field)
    return FALSE;

for (streams = river->riv_streams, end = streams + river->riv_count; 
     streams < end; streams++)
    if ((USHORT) node->nod_arg [e_fld_stream] == *streams)
	return TRUE;

return FALSE;
}

static BOOLEAN search_stack (
    NOD		node,
    LLS		stack)
{
/**************************************
 *
 *	s e a r c h _ s t a c k
 *
 **************************************
 *
 * Functional description
 *	Search a stack for the presence of a particular value.
 *
 **************************************/

DEV_BLKCHK (node, type_nod);
DEV_BLKCHK (stack, type_lls);

for (; stack; stack = stack->lls_next)
    if (node_equality (node, (NOD) stack->lls_object))
	return TRUE;

return FALSE;
}

static void set_active (
    OPT		opt,
    RIV		river)
{
/**************************************
 *
 *	s e t _ a c t i v e
 *
 **************************************
 *
 * Functional description
 *	Set a group of streams active.
 *
 **************************************/
CSB	csb;
UCHAR	*streams, *end;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (river, type_riv);

csb = opt->opt_csb;

for (streams = river->riv_streams, end = streams + river->riv_count;
     streams < end; streams++)
    csb->csb_rpt [*streams].csb_flags |= csb_active;
}

static void set_direction (
    NOD		from_clause, 
    NOD		to_clause)
{
/**************************************
 *
 *	s e t _ d i r e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Update the direction of a GROUP BY, DISTINCT, or ORDER BY 
 *	clause to the same direction as another clause. 	
 *
 **************************************/  
NOD	*from_ptr, *to_ptr, *end;

DEV_BLKCHK (from_clause, type_nod);
DEV_BLKCHK (to_clause, type_nod);

/* all three clauses are allocated with twice the number of arguments to 
   leave room at the end for an ascending/descending flag, one for each field */

from_ptr = from_clause->nod_arg + from_clause->nod_count;
to_ptr = to_clause->nod_arg + to_clause->nod_count;

for (end = from_ptr + from_clause->nod_count; from_ptr < end; from_ptr++)
    *to_ptr++ = *from_ptr;
}


static void set_inactive (
    OPT		opt,
    RIV		river)
{
/**************************************
 *
 *	s e t _ i n a c t i v e
 *
 **************************************
 *
 * Functional description
 *	Set a group of streams inactive.
 *
 **************************************/
CSB	csb;
UCHAR	*streams, *end;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (river, type_riv);

csb = opt->opt_csb;

for (streams = river->riv_streams, end = streams + river->riv_count;
     streams < end; streams++)
    csb->csb_rpt [*streams].csb_flags &= ~csb_active;
}

static void set_made_river (
    OPT		opt,
    RIV		river)
{
/**************************************
 *
 *	s e t _ m a d e _ r i v e r
 *
 **************************************
 *
 * Functional description
 *      Mark all the streams in a river with the csb_made_river flag.
 * 
 *      A stream with this flag set, incicates that this stream has
 *      already been made into a river. Currently, this flag is used
 *      in computable() to decide if we can use the an index to
 *      optimise retrieving the streams involved in the conjunct.
 *
 *      We can use an index in retrieving the streams involved in a
 *      conjunct if both of the streams are currently active or have
 *      been processed (and made into rivers) before.
 *
 **************************************/
CSB	csb;
UCHAR	*streams, *end;

DEV_BLKCHK (opt, type_opt);
DEV_BLKCHK (river, type_riv);

csb = opt->opt_csb;

for (streams = river->riv_streams, end = streams + river->riv_count;
     streams < end; streams++)
    csb->csb_rpt [*streams].csb_flags |= csb_made_river;
}

static void set_position (
    NOD		from_clause, 
    NOD		to_clause,
    NOD		map)
{
/**************************************
 *
 *	s e t _ p o s i t i o n
 *
 **************************************
 *
 * Functional description
 *	Update the fields in a GROUP BY, DISTINCT, or ORDER BY 
 *	clause to the same position as another clause, possibly 
 *	using a mapping between the streams.
 *
 **************************************/  
NOD	*from_ptr, *to_ptr;
NOD	*from_end, *to_end;
NOD	*to_swap, swap;

DEV_BLKCHK (from_clause, type_nod);
DEV_BLKCHK (to_clause, type_nod);
DEV_BLKCHK (map, type_nod);

/* Track the position in the from list with "to_swap", and find the corresponding 
   field in the from list with "to_ptr", then swap the two fields.  By the time 
   we get to the end of the from list, all fields in the to list will be reordered. */

to_swap = to_clause->nod_arg;
for (from_ptr = from_clause->nod_arg, from_end = from_ptr + from_clause->nod_count; from_ptr < from_end; from_ptr++)
    {
    for (to_ptr = to_clause->nod_arg, to_end = to_ptr + from_clause->nod_count; to_ptr < to_end; to_ptr++)
        {
	if ((map && map_equal (*to_ptr, *from_ptr, map)) ||
	    (!map && (*from_ptr)->nod_arg [e_fld_stream] == (*to_ptr)->nod_arg [e_fld_stream] &&
	             (*from_ptr)->nod_arg [e_fld_id] == (*to_ptr)->nod_arg [e_fld_id]))
	    {
	    swap = *to_swap;
	    *to_swap = *to_ptr;
	    *to_ptr = swap;
	    }
	}

    to_swap++;
    }

}

static void set_rse_inactive (
    CSB        csb,
    RSE        rse)
{
/***************************************************
 *
 *  s e t _ r s e _ i n a c t i v e
 *
 ***************************************************
 *
 * Functional Description:
 *    Set all the streams involved in an rse as inactive. Do it recursively.
 *
 ***************************************************/
NOD   node, *ptr, *end;
SSHORT  stream;

for (ptr = rse->rse_relation, end = ptr + rse->rse_count; ptr < end; ptr++)
    {
    node = *ptr;
    if (node->nod_type != nod_rse)
      {
      stream = (USHORT) node->nod_arg [STREAM_INDEX(node)];
      csb->csb_rpt [stream].csb_flags &= ~csb_active;
      }
    else
      set_rse_inactive (csb, (RSE) node);
    }
}
