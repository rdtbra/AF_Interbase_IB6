/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		expand.c
 *	DESCRIPTION:	Expand field references to get context.
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

#include <setjmp.h>
#include <string.h>
#include "../dudley/ddl.h"
#include "../jrd/gds.h"
#include "../dudley/parse.h"
#include "../dudley/ddl_proto.h"
#include "../dudley/expan_proto.h"
#include "../dudley/hsh_proto.h"
#include "../dudley/parse_proto.h"
#include "../jrd/gds_proto.h"

static void	expand_action (ACT);
static void	expand_error (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
static void	expand_field (FLD);
static void	expand_global_field (FLD);
static void	expand_index (ACT);
static void	expand_relation (REL);
static void	expand_trigger (TRG);
static FLD	field_context (NOD, LLS, CTX *);
static FLD	field_search (NOD, LLS, CTX *);
static CTX	lookup_context (SYM, LLS);
static FLD	lookup_field (FLD);
static FLD	lookup_global_field (FLD);
static REL	lookup_relation (REL);
static TRG	lookup_trigger (TRG);
static CTX	make_context (TEXT *, REL, USHORT);
static NOD	resolve (NOD, LLS, LLS);
static void	resolve_rse (NOD, LLS *);

static SSHORT		context_id;
static GDS__QUAD	null_blob;
static LLS		request_context;
static jmp_buf		exp_env;

static TEXT	alloc_info [] = { gds__info_allocation, gds__info_end };

#define CMP_SYMBOL(sym1, sym2) strcmp (sym1->sym_string, sym2->sym_string)
#define MOVE_SYMBOL(symbol, field) move_symbol (symbol, field, sizeof (field))

void EXP_actions (void)
{
/**************************************
 *
 *	E X P _ a c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Expand the output of the parser.
 *	Look for field references and put
 *	them in appropriate context.
 *   
 **************************************/
ACT	action, temp, stack;

if (!DDL_actions)
    return;

for (action = DDL_actions; action; action = action->act_next)
    if (!(action->act_flags & ACT_ignore))
	expand_action (action);
}

static void expand_action (
    ACT	action)
{
/**************************************
 *
 *	e x p a n d _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate an error message.
 *
 **************************************/

/* Set up an environment to catch syntax errors.  If one occurs, scan looking
   for semi-colon to continue processing. */

if (setjmp (exp_env))
    return;

DDL_line = action->act_line;

switch (action->act_type)
    {
    case act_a_field:
	expand_field (action->act_object);
	break;

    case act_m_field:
    case act_d_field:
	lookup_field (action->act_object);
	break;

    case act_d_filter:
/*	lookup_filter (action->act_object);
*/	break;

    case act_a_function:
    case act_a_function_arg:
	break;

    case act_d_function:
/*	lookup_function (action->act_object);
*/	break;

    case act_a_gfield:
	expand_global_field (action->act_object);
	break;

    case act_m_gfield:
	lookup_global_field (action->act_object);
	expand_global_field (action->act_object);
	break;

    case act_d_gfield:
	lookup_global_field (action->act_object);
	break;

    case act_a_index:
	expand_index (action->act_object);
	break;

    case act_m_relation:
	lookup_relation (action->act_object);
    case act_a_relation:
	expand_relation (action->act_object);
	break;

    case act_d_relation:
	lookup_relation (action->act_object);
	break;

    case act_d_trigger:
	lookup_trigger (action->act_object);
	break;

    case act_m_trigger:
	lookup_trigger (action->act_object);

    case act_a_trigger: 
	expand_trigger (action->act_object);
	break;

    case act_c_database:
    case act_m_database:
    case act_a_filter:
    case act_m_index:
    case act_d_index:
    case act_a_security:
    case act_d_security:
    case act_a_trigger_msg: 
    case act_m_trigger_msg:
    case act_d_trigger_msg:
    case act_a_type: 
    case act_m_type:
    case act_d_type:
    case act_grant:
    case act_revoke:
    case act_a_shadow:
    case act_d_shadow:
    case act_a_generator:
    case act_s_generator:
	break;

    default:
	DDL_msg_put (97, NULL, NULL, NULL, NULL, NULL); /* msg 97: object can not be resolved */
    }
return;
} 

static void expand_error (
    USHORT	number,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3,
    TEXT	*arg4,
    TEXT	*arg5)
{
/**************************************
 *
 *	e x p a n d _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Generate an error message.
 *
 **************************************/

DDL_err (number, arg1, arg2, arg3, arg4, arg5);
longjmp (exp_env, TRUE);
}

static void expand_field (
    FLD		field)
{
/**************************************
 *
 *	e x p a n d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Expand a field definition, resolving references
 *	to its global field and other fields.
 *
 **************************************/
SYM	name;

if (!field->fld_source)
    {
    name = field->fld_name;
    if (!(field->fld_source = HSH_typed_lookup (name->sym_string, 
                                   name->sym_length, SYM_global)))
	expand_error (98, name->sym_string,0,0,0,0); /* msg 98: Global field %s is not defined */
    } 

if (!field->fld_source_field)
    field->fld_source_field = (FLD) field->fld_source->sym_object;
}

static void expand_global_field (
    FLD		field)
{
/**************************************
 *
 *	e x p a n d _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Expand a global field definition, resolving references
 *	to other fields and stuff. Mark the default context
 *	with this field so we can identify ourselves if we
 *	find ourselves in a computation or validation.
 *
 **************************************/
CTX	context;

if (!request_context || 
    !(context = lookup_context (NULL_PTR, request_context)))
    {
    context = (CTX) DDL_alloc (CTX_LEN); 
    LLS_PUSH (context, &request_context);
    }

context->ctx_field = field;

field->fld_computed = resolve (field->fld_computed, request_context, NULL_PTR);
field->fld_validation  = resolve (field->fld_validation, request_context, NULL_PTR);

context->ctx_field = 0;
}

static void expand_index (
    ACT		action)
{
/**************************************
 *
 *	e x p a n d _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Expand an index definition, resolving references
 *	to other fields.
 *
 **************************************/

request_context = NULL;
}

static void expand_relation (
    REL		relation)
{
/**************************************
 *
 *	e x p a n d _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Expand a relation definition, resolving references
 *	to fields and contexts.
 *
 **************************************/
LLS	contexts, stack;
CTX	my_context, context;
NOD	rse;

context_id = 0;
contexts = NULL;
request_context = NULL;

if (!relation->rel_rse)
    LLS_PUSH (make_context (NULL, relation, context_id++), &request_context);
else
    {
    rse = relation->rel_rse;
    contexts = (LLS) rse->nod_arg [s_rse_contexts];
    my_context = lookup_context (NULL, contexts);    
    my_context->ctx_view_rse = TRUE;

    /* drop view context from context stack & build the request stack */

    for (stack = NULL; contexts; contexts = contexts->lls_next)
	LLS_PUSH (contexts->lls_object, &stack);

    while (stack)
	{
	context = (CTX) LLS_POP (&stack);
	if (!context->ctx_view_rse)
	    LLS_PUSH (context, &request_context);
	}
    LLS_PUSH (my_context, &contexts);
    resolve_rse (relation->rel_rse, &contexts);

    /* Put view context back on stack for global field resolution to follow */

    LLS_PUSH (my_context, &request_context);
    my_context->ctx_view_rse = FALSE;
    }
}

static void expand_trigger (
    TRG		trigger)
{
/**************************************
 *
 *	e x p a n d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Expand a trigger definition, resolving references
 *	to fields and contexts.
 *
 **************************************/
LLS	contexts, update;
CTX	old, new;
REL	relation;
USHORT	old_id;

context_id = 2;
old = new = NULL;
request_context = contexts = update = NULL;

relation = trigger->trg_relation;

if ((trigger->trg_type != trg_store) && (trigger->trg_type != trg_post_store))
    {
    if (!old)
        old = make_context ("OLD", relation, 0);
    LLS_PUSH (old, &contexts);
    }

if ((trigger->trg_type != trg_erase) && (trigger->trg_type != trg_pre_erase))
    {
    if (!new)
    	new = make_context ("NEW", relation, 1);
    LLS_PUSH (new, &contexts);
    LLS_PUSH (new, &update);
    }

resolve (trigger->trg_statement, contexts, update);
context_id = 0;
}

static FLD field_context (
    NOD		node,
    LLS		contexts,
    CTX		*output_context)
{
/**************************************
 *
 *	f i e l d _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Lookup a field reference, guessing the
 *	context.  Since by now all field references 
 *	ought to be entered in the hash table, this is
 *	pretty easy.  We may be looking up a global
 *	field reference, in which case the context
 *	relation will be null. 
 *
 *
 **************************************/
FLD	field;
CTX	context;
SYM	name, symbol;

/* If the field is unqualified, resolve it to the default context */

if (node->nod_count == 1)
    {
    context = lookup_context (NULL_PTR, contexts);
    name = (SYM) node->nod_arg [0];
    }
else if (node->nod_count == 2)
    {
    context = lookup_context (node->nod_arg [0], contexts); 
    name = (SYM) node->nod_arg [1];
    }
else
    return NULL;

if (!context)
    return NULL;

*output_context = context;

symbol = HSH_lookup (name->sym_string, name->sym_length);

for (; symbol; symbol = symbol->sym_homonym)
    {
    if (context->ctx_relation) 
	{
	if (symbol->sym_type == SYM_field)
            {
	    field = (FLD) symbol->sym_object;
	    if (field->fld_relation == context->ctx_relation)    
		return field; 
	    }
	}	    
    else if (symbol->sym_type == SYM_global)
	    return (FLD) symbol->sym_object; 
    }

if (context->ctx_relation)
    expand_error (99, name->sym_string, 
		   context->ctx_relation->rel_name->sym_string,0,0,0);
	/* msg 99: field %s doesn't exist in relation %s */
else
    expand_error (100, name->sym_string,0,0,0,0); 
	/* msg 100: field %s doesn't exist */

return NULL;
} 

static FLD field_search (
    NOD		node,
    LLS		contexts,
    CTX		*output_context)
{
/**************************************
 *
 *	f i e l d _ s e a r c h
 *
 **************************************
 *
 * Functional description
 *	We have, or believe we have, a field
 *	reference that has to be resolved
 *	iteratively.  The context indicated is
 *	the current context.  Get to there, then
 *	work backward.
 * 
 **************************************/
FLD	field;
CTX	context, old_context;
SYM	name, symbol, ctx_name;

name = (SYM) node->nod_arg [1];
symbol = HSH_lookup (name->sym_string, name->sym_length);
ctx_name  = (SYM) node->nod_arg [0]; 

if (symbol)
    {
    /* wander down to the old context level */
    for (; contexts; contexts = contexts->lls_next)
	{
	old_context = (CTX) contexts->lls_object;
	if ((name = old_context->ctx_name) && 
	    !strcmp (ctx_name->sym_string, name->sym_string))
	    break;
	}

    name = symbol;
    for (contexts = contexts->lls_next; contexts; contexts = contexts->lls_next)
	{
	context = (CTX) contexts->lls_object;
	if (context->ctx_relation && !context->ctx_view_rse)
	    {
	    *output_context = context;
	    for (symbol = name; symbol; symbol = symbol->sym_homonym)
		{
		if (symbol->sym_type == SYM_field)
		    {
		    field = (FLD) symbol->sym_object;
		    if (field->fld_relation == context->ctx_relation)    
			return field; 
		    }
		}	    
            }
	}
    }

expand_error (101, name->sym_string,0,0,0,0); /* msg 101: field %s can't be resolved */
return NULL;
}

static CTX lookup_context (
    SYM		symbol,
    LLS		contexts)
{
/**************************************
 *
 *	l o o k u p _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Search the context stack.  If a context name is given, consider
 *	only named contexts; otherwise return the first unnamed context.
 *	In either case, if nothing matches, return NULL.
 *
 **************************************/
CTX	context;
SYM	name;

/* If no name is given, look for a nameless one. */

if (!symbol)
    {
    for (; contexts; contexts = contexts->lls_next)
	{
	context = (CTX) contexts->lls_object;
	if (!context->ctx_name && !context->ctx_view_rse)
	    return context;
	}
    return NULL;
    }

/* Other search by name */

for (; contexts; contexts = contexts->lls_next)
    {
    context = (CTX) contexts->lls_object;
    if ((name = context->ctx_name) && 
	!strcmp (name->sym_string, symbol->sym_string))
	return context;
    }

return NULL;
}

static FLD lookup_field (
    FLD	old_field)
{
/**************************************
 *
 *	l o o k u p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Lookup a field reference, from a modify or
 *	delete field statement, and make sure we
 *	found the thing originally.
 *	context.  Since by now all field references 
 *	ought to be entered in the hash table, this is
 *	pretty easy.
 *
 **************************************/
REL	relation;
SYM	symbol, name;
FLD	field;

if (old_field->fld_source)
    lookup_global_field (old_field);

name = old_field->fld_name;
relation = old_field->fld_relation;

symbol = HSH_lookup (name->sym_string, name->sym_length);

for (; symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_field)
        {
	field = (FLD) symbol->sym_object;
	if (field->fld_relation == relation)
	    return field; 
	}	    

expand_error (102, name->sym_string, relation->rel_name->sym_string,0,0,0);
    /* msg 102: field %s isn't defined in relation %s */

return NULL;
}

static FLD lookup_global_field (
    FLD		field)
{
/**************************************
 *
 *	l o o k u p _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SYM	symbol, name;

/* Find symbol */

name = (field->fld_source) ? field->fld_source : field->fld_name;
if (symbol = HSH_typed_lookup (name->sym_string, name->sym_length, SYM_global))
    return (FLD) symbol->sym_object;

expand_error (103, name->sym_string,0,0,0,0); /* msg 103: global field %s isn't defined */

return NULL;
}

static REL lookup_relation (
    REL	relation)
{
/**************************************
 *
 *	l o o k u p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SYM	symbol, name;

/* Find symbol */

name = (relation->rel_name);
if ((symbol = HSH_typed_lookup (name->sym_string, name->sym_length, SYM_relation)) &&
    symbol->sym_object)
    return (REL) symbol->sym_object;

expand_error (104, name->sym_string,0,0,0,0); /* msg 104: relation %s isn't defined */

return NULL;
}

static TRG lookup_trigger (
    TRG	trigger)
{
/**************************************
 *
 *	l o o k u p _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SYM	symbol, name;

/* Find symbol */

name = (trigger->trg_name);
if (symbol = HSH_typed_lookup (name->sym_string, name->sym_length, SYM_trigger))
    return (TRG) symbol->sym_object;

expand_error (105, name->sym_string,0,0,0,0); /* msg 105: trigger %s isn't defined */

return NULL;
}

static CTX make_context (
    TEXT	*string,
    REL		relation,
    USHORT	id)
{
/**************************************
 *
 *	m a k e _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Make context for trigger.
 *
 **************************************/
CTX	context;
SYM	symbol;

context = (CTX) DDL_alloc (CTX_LEN);
context->ctx_context_id = id;
context->ctx_relation = relation;

if (string)
    {
    context->ctx_name = symbol = (SYM) DDL_alloc (SYM_LEN);
    symbol->sym_object = context;
    symbol->sym_string = string;
    symbol->sym_length = strlen (string);
    symbol->sym_type = SYM_context;
    }

return context;
}

static NOD resolve (
    NOD		node,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	r e s o l v e
 *
 **************************************
 *
 * Functional description
 *	Resolve field names in an expression.  This is called after
 *	an expression has been parsed and after the appropriate context
 *	variables have been entered in the hash table.  If an unqualified
 *	field is to be resolved to a known relation, a relation block
 *	is passed in.
 *
 **************************************/
NOD	*arg, *end, field, sub;
SYM	symbol, name;
FLD	fld;
CTX	context, old_context;
TEXT	name_string [65], *p, *q;

if (!node)
    return NULL;
arg = node->nod_arg;
end = arg + node->nod_count;

switch (node->nod_type)
    {
    case nod_field_name:
    case nod_over:
	break;
    
    case nod_rse:
	expand_error (106,0,0,0,0,0); /* msg 106: bugcheck */
	return node;

    case nod_field:
    case nod_literal:
	return node;
    
    case nod_count:
    case nod_max:
    case nod_min:
    case nod_total:
    case nod_average:
    case nod_from:
	if (sub = node->nod_arg [s_stt_default])
	    node->nod_arg [s_stt_default] = resolve (sub, right, NULL_PTR);
	resolve_rse (node->nod_arg [s_stt_rse], &right);
	if (node->nod_arg [s_stt_value])
	    node->nod_arg [s_stt_value] = 
		resolve (node->nod_arg [s_stt_value], right, NULL_PTR);
	return node;

    case nod_unique:
    case nod_any:
	resolve_rse (node->nod_arg [s_stt_rse], &right);
	return node;

    case nod_for:
	resolve_rse (node->nod_arg [s_for_rse], &right);
	resolve (node->nod_arg [s_for_action], right, left);
	return node;

    case nod_store:
	context = (CTX) node->nod_arg [s_store_rel];
	context->ctx_context_id = ++context_id;
	name = context->ctx_relation->rel_name;
	if (!HSH_typed_lookup (name->sym_string, name->sym_length, SYM_relation))
	    expand_error (107, name->sym_string,0,0,0,0); /* msg 107: relation %s is not defined */
	LLS_PUSH (context, &left);
	sub = SYNTAX_NODE (nod_context, 1);
	sub->nod_arg [0] = (NOD) context;
	node->nod_arg [s_store_rel] = sub;
	resolve (node->nod_arg [s_store_action], right, left);
	return node;

    case nod_erase:
	symbol = (SYM) node->nod_arg [0];
	if (!(node->nod_arg [0] = (NOD) lookup_context (symbol, right)))
	    expand_error (108, symbol->sym_string,0,0,0,0); /* msg 108: context %s is not defined  */
	return node;

    case nod_modify:
	symbol = (SYM) node->nod_arg [s_mod_old_ctx];
	if (!(old_context = lookup_context (symbol, right)))
	    expand_error (108, symbol->sym_string,0,0,0,0); /* msg 108: context %s is not defined */
	node->nod_arg [s_mod_old_ctx] = (NOD) old_context;
	context = (CTX) DDL_alloc (CTX_LEN);
	node->nod_arg [s_mod_new_ctx] = (NOD) context; 
	context->ctx_name = symbol;
	context->ctx_context_id = ++context_id;
	context->ctx_relation = old_context->ctx_relation;
	LLS_PUSH (context, &left);
	node->nod_arg [s_mod_action] = resolve (node->nod_arg [s_mod_action], right, left);
	return node;

    case nod_index:
	sub = resolve (node->nod_arg [1], right, left);
	field = resolve (node->nod_arg [0], right, left);
	field->nod_arg [s_fld_subs] = sub;
	return field;

    case nod_assignment:
	node->nod_arg [1] = resolve (node->nod_arg [1], left, NULL_PTR);
	node->nod_arg [0] = resolve (node->nod_arg [0], right, NULL_PTR);
	return node;

    case nod_post:
	node->nod_arg [0] = resolve (node->nod_arg [0], right, NULL_PTR);
	return node;

    default:
	for (; arg < end; arg++)
	    *arg = resolve (*arg, right, left);
	return node;
    }
    
/* We've dropped thru to resolve a field name node.  If we can find it,
   make up a field node, otherwise generate an error.  We may be looking
   for either a local or a global field, which is known by whether the
   context has a relation or not.  Do something reasonable in either
   case */

switch (node->nod_type)
    {
    case nod_field_name:
	fld = field_context (node, right, &context);
	break;

    case nod_over:
	fld = field_search (node, right, &context);
	break;
    }

if (fld)
    {
    if (context->ctx_field &&
	((!context->ctx_relation && fld == context->ctx_field) ||
	(context->ctx_relation && fld->fld_source &&
	    !(strcmp (fld->fld_source->sym_string, context->ctx_field->fld_name->sym_string)))))
	return SYNTAX_NODE (nod_fid, 0); 
    field = SYNTAX_NODE (nod_field, s_fld_count);
    field->nod_arg [s_fld_name] = node;
    field->nod_arg [s_fld_field] =(NOD) fld;
    field->nod_arg [s_fld_context] = (NOD) context;
    return field;
    }

p = name_string;
for (; arg < end; arg++)
    {
    if (symbol = (SYM) *arg)
	{
	q = symbol->sym_string;
	while (*q)
	    *p++ = *q++;
	*p++ = '.';
	} 
    }

p [-1] = 0;
expand_error (109, name_string,0,0,0,0); /* msg 109:  Can't resolve field \"%s\" */

return node;
}

static void resolve_rse (
    NOD		rse,
    LLS		*stack)
{
/**************************************
 *
 *	r e s o l v e _ r s e
 *
 **************************************
 *
 * Functional description
 *	Resolve record selection expression, augmenting 
 *	context stack.  At the same time, put a context
 *	node in front of every context and build a list
 *	out of the whole thing;
 *
 **************************************/
NOD	sub, *arg, *end;
LLS	contexts, temp;
CTX     context;
SYM	name, symbol;

/* Handle FIRST <n> clause in original context */

if (sub = rse->nod_arg [s_rse_first])
    rse->nod_arg [s_rse_first] = resolve (sub, *stack, NULL_PTR);

contexts = (LLS) rse->nod_arg [s_rse_contexts];

for (temp = NULL; contexts;)
    LLS_PUSH (LLS_POP(&contexts), &temp);

for (contexts = temp; contexts; contexts = contexts->lls_next) 
    {
    context = (CTX) contexts->lls_object; 
    name = context->ctx_relation->rel_name;
    if (!(symbol = HSH_typed_lookup (name->sym_string, name->sym_length, SYM_relation)) ||
	!symbol->sym_object)
	expand_error (110, name->sym_string,0,0,0,0); /* msg 110: relation %s is not defined */
    else 
	context->ctx_relation = (REL) symbol->sym_object;
    LLS_PUSH (context, stack);
    }

if (sub = rse->nod_arg [s_rse_boolean])
    rse->nod_arg [s_rse_boolean] = resolve (sub, *stack, NULL_PTR);

if (sub = rse->nod_arg [s_rse_sort])
    for (arg = sub->nod_arg, end = arg + sub->nod_count; arg < end; arg += 2)
	*arg = resolve (*arg, *stack, NULL_PTR);

if (sub = rse->nod_arg [s_rse_reduced])
    for (arg = sub->nod_arg, end = arg + sub->nod_count; arg < end; arg += 2)
	*arg = resolve (*arg, *stack, NULL_PTR);

while (temp)
    {
    context = (CTX) LLS_POP (&temp);
    if (!context->ctx_view_rse)
	{
	context->ctx_context_id = ++context_id;
	sub = SYNTAX_NODE (nod_context, 1);
	sub->nod_arg[0] = (NOD) context;
	LLS_PUSH (sub, &contexts);    
	}
    }

rse->nod_arg [s_rse_contexts] = PARSE_make_list (contexts);
}
