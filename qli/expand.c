/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		expand.c
 *	DESCRIPTION:	Expand syntax tree -- first phase of compiler
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

#include <string.h>
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../qli/compile.h"
#include "../qli/exe.h"
#include "../qli/report.h"
#include "../qli/form.h"
#ifdef JPN_SJIS
#include "../jrd/kanji.h"
#endif
#include "../qli/all_proto.h"
#include "../qli/comma_proto.h"
#include "../qli/compi_proto.h"
#include "../qli/err_proto.h"
#include "../qli/expan_proto.h"
#include "../qli/form_proto.h"
#include "../qli/help_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/show_proto.h"

extern USHORT	QLI_columns, QLI_lines;

#define MAKE_NODE(type,count)	make_node (type, count)

static int	compare_names (NAM, SYM);
static int	compare_symbols (SYM, SYM);
static SYM	copy_symbol (SYM);
static void	declare_global (FLD, SYN);
static SYN	decompile_field (FLD, CTX);
static NAM	decompile_symbol (SYM);
static NOD	expand_assignment (SYN, LLS, LLS);
static NOD	expand_any (SYN, LLS);
static NOD	expand_boolean (SYN, LLS);
static void	expand_control_break (BRK *, LLS);
static void	expand_distinct (NOD, NOD);
static void	expand_edit_string (NOD, ITM);
static NOD	expand_erase (SYN, LLS, LLS);
static NOD	expand_expression (SYN, LLS);
static NOD	expand_field (SYN, LLS, SYN);
static NOD	expand_for (SYN, LLS, LLS);
static FRM	expand_form (SYN, REL);
static NOD	expand_form_for (SYN, LLS, LLS);
static NOD	expand_form_update (SYN, LLS, LLS);
static NOD	expand_function (SYN, LLS);
static NOD	expand_group_by (SYN, LLS, CTX);
static NOD	expand_menu (SYN, LLS, LLS);
static NOD	expand_modify (SYN, LLS, LLS);
static NOD	expand_output (SYN, LLS, PRT *);
static NOD	expand_print (SYN, LLS, LLS);
static NOD	expand_print_form (SYN, LLS, LLS);
static ITM	expand_print_item (SYN, LLS);
static NOD	expand_print_list (SYN, LLS);
static NOD	expand_report (SYN, LLS, LLS);
static NOD	expand_restructure (SYN, LLS, LLS);
static NOD	expand_rse (SYN, LLS *);
static NOD	expand_sort (SYN, LLS, NOD);
static NOD	expand_statement (SYN, LLS, LLS);
static NOD	expand_store (SYN, LLS, LLS);
static void	expand_values (SYN, LLS);
static CTX	find_context (NAM, LLS);
static int	generate_fields (CTX, LLS, SYN);
static int	generate_items (SYN, LLS, LLS, NOD);
static SLONG	global_agg (SYN, SYN);
static int	invalid_nod_field (NOD, NOD);
static int	invalid_syn_field (SYN, SYN);
static NOD	make_and (NOD, NOD);
static NOD	make_assignment (NOD, NOD, LLS);
static NOD	make_form_body (LLS, LLS, SYN);
static NOD	make_form_field (FRM, FFL);
static NOD	make_field (FLD, CTX);
static NOD	make_list (LLS);
static NOD	make_node (NOD_T, USHORT);
static NOD	negate (NOD);
static NOD	possible_literal (SYN, LLS, USHORT);
static NOD	post_map (NOD, CTX);
static FLD	resolve (SYN, LLS, CTX *);
static FLD	resolve_name (SYM, LLS, CTX *);
static void	resolve_really (FLD, SYN);

static LLS	output_stack;

NOD EXP_expand (
    SYN		node)
{
/**************************************
 *
 *	E X P _ e x p a n d
 *
 **************************************
 *
 * Functional description
 *	Expand a syntax tree into something richer and more complete.
 *
 **************************************/
NOD	expanded, output;
NAM	name;
CTX	context;
LLS	right, left;

switch (node->syn_type)
    {
    case nod_commit:
    case nod_prepare:
    case nod_rollback:
	CMD_transaction (node);
	return NULL;

    case nod_copy_proc:
	CMD_copy_procedure (node);
	return NULL;

    case nod_declare:                                    
	declare_global (node->syn_arg [0], node->syn_arg [1]);
	return NULL;
    
    case nod_define:
	CMD_define_procedure (node);
	return NULL;

    case nod_delete_proc:
	CMD_delete_proc (node);
	return NULL;

    case nod_def_database:
    case nod_sql_database:
	MET_ready (node, TRUE);
	return NULL;

    case nod_def_field:
	MET_define_field (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_def_index:
	MET_define_index (node);
	return NULL;

    case nod_def_relation:
	MET_define_relation (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_del_relation:
	MET_delete_relation (node->syn_arg [0]);
	return NULL;

    case nod_del_field:
	MET_delete_field (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_del_index:
	MET_delete_index (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_del_database:
	MET_delete_database (node->syn_arg [0]);
	return NULL;

    case nod_edit_proc:
	CMD_edit_proc (node);
	return NULL;

    case nod_edit_form:
	name = (NAM) node->syn_arg [1];
	FORM_edit (node->syn_arg [0], name->nam_string);
	return NULL;

    case nod_extract:
	node->syn_arg [1] = (SYN) expand_output (node->syn_arg [1], NULL_PTR, NULL_PTR);
	CMD_extract (node);
	return NULL;

    case nod_finish:
	CMD_finish (node);
	return NULL;

    case nod_help:
	HELP_help (node);
	return NULL;

    case nod_mod_field:
	MET_modify_field (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_mod_relation:
	MET_modify_relation (node->syn_arg [0], node->syn_arg [1]);
	return NULL;

    case nod_mod_index:
	MET_modify_index (node);
	return NULL;

    case nod_ready:
	MET_ready (node, FALSE);
	return NULL;

    case nod_rename_proc:
	CMD_rename_proc (node);
	return NULL;

    case nod_set:
	CMD_set (node);
	return NULL;

    case nod_show:
	SHOW_stuff (node);
	return NULL;

    case nod_shell:
	CMD_shell (node);
	return NULL;    

    case nod_sql_grant:
	MET_sql_grant (node);
	return NULL;

    case nod_sql_revoke:
	MET_sql_revoke (node);
	return NULL;

    case nod_sql_cr_table:
	MET_define_sql_relation (node->syn_arg [0]);
	return NULL;

/****
    case nod_sql_cr_view:
	MET_sql_cr_view (node);
	GEN_release();
	return NULL;
****/

    case nod_sql_al_table:
	MET_sql_alter_table (node->syn_arg [0], node->syn_arg [1]);
	return NULL;
    }

/* If there are any variables, make up a context now */

output_stack = right = left = NULL;

if (QLI_variables)
    {
    context = (CTX) ALLOCD (type_ctx);
    context->ctx_type = CTX_VARIABLE;
    context->ctx_variable = QLI_variables;
    LLS_PUSH (context, &right);
    LLS_PUSH (context, &left);
    }

if (!(expanded = expand_statement (node, right, left)))
    return NULL;

while (output_stack)
    {
    output = (NOD) LLS_POP (&output_stack);
    output->nod_arg [e_out_statement] = expanded;
    expanded = output;
    }

return expanded;
}    

static int compare_names (
    NAM		name,
    SYM		symbol)
{
/**************************************
 *
 *	c o m p a r e _ n a m e s
 *
 **************************************
 *
 * Functional description
 *	Compare a name node to a symbol.  If they are equal, return TRUE.
 *
 **************************************/
USHORT	l;
TEXT	*p, *q;

if (!symbol || (l = name->nam_length) != symbol->sym_length)
    return FALSE;

p = symbol->sym_string;
q = name->nam_string;

if (l)
    do if (*p++ != *q++) return FALSE; while (--l);

return TRUE;
}

static int compare_symbols (
    SYM		symbol1,
    SYM		symbol2)
{
/**************************************
 *
 *	c o m p a r e _ s y m b o l s
 *
 **************************************
 *
 * Functional description
 *	Compare two symbols (either may be NULL_PTR).
 *
 **************************************/
USHORT	l;
TEXT	*p, *q;

if (!symbol1 || !symbol2)
    return FALSE;

if ((l = symbol1->sym_length) != symbol2->sym_length)
    return FALSE;

p = symbol1->sym_string;
q = symbol2->sym_string;

if (l)
    do if (*p++ != *q++) return FALSE; while (--l);

return TRUE;
}

static SYM copy_symbol (
    SYM		old)
{
/**************************************
 *
 *	c o p y _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Copy a symbol into the permanent pool.
 *
 **************************************/
SYM	new;

new = (SYM) ALLOCPV (type_sym, old->sym_length);
new->sym_length = old->sym_length;
new->sym_type = old->sym_type;
new->sym_string = new->sym_name;
strcpy (new->sym_name, old->sym_name);

return new;
}

static void declare_global (
    FLD		variable,
    SYN		field_node)
{
/**************************************
 *
 *	d e c l a r e _ g l o b a l
 *
 **************************************
 *
 * Functional description
 *	Copy a variable block into the permanent pool as a field.  
 *	Resolve BASED_ON references.  
 *	Allocate all strings off the field block.
 *
 **************************************/
TEXT	*p, *q;
USHORT	l;
FLD	new, field, *ptr; 

/* If it's based_on, flesh it out & check datatype.  */

if (field_node) 
    {
    if (field_node->syn_type == nod_index)
	field_node = field_node->syn_arg [s_idx_field];
    resolve_really (variable, field_node);        
    if (variable->fld_dtype == dtype_blob)
        IBERROR (137);   /* Msg137 variables may not be based on blob fields. */
    }
    
/* Get rid of any other variables of the same name */

for (ptr = &QLI_variables; field = *ptr; ptr = &field->fld_next)
    if (!strcmp (field->fld_name->sym_string, variable->fld_name->sym_string))
	{
	*ptr = field->fld_next;
	ALLQ_release (field->fld_name);
	if (field->fld_query_name)
	    ALLQ_release (field->fld_query_name);
	ALLQ_release (field);
	break;
	}

/* Next, copy temporary field block into permanent pool.  Fold edit_string
   query_header into main block to save space and complexity. */

l = variable->fld_length;
if (q = variable->fld_edit_string)
    l += strlen (q);
if (q = variable->fld_query_header)
    l += strlen (q); 

new = (FLD) ALLOCPV (type_fld, l);
new->fld_name = copy_symbol (variable->fld_name);
new->fld_dtype = variable->fld_dtype;
new->fld_length = variable->fld_length;
new->fld_scale = variable->fld_scale;
new->fld_sub_type = variable->fld_sub_type;
new->fld_sub_type_missing = variable->fld_sub_type_missing;
new->fld_flags = variable->fld_flags | FLD_missing;

/* Copy query_name, edit string, query header */

p = (TEXT*) new->fld_data + new->fld_length;
if (q = variable->fld_edit_string)
    {
    new->fld_edit_string = p;
    while (*p++ = *q++)
	;
    }
if (variable->fld_query_name)
    new->fld_query_name = copy_symbol (variable->fld_query_name);
if (q = variable->fld_query_header)
    {
    new->fld_query_header = p;
    while (*p++ = *q++)
	;
    }

/* Link new variable into variable chain */

new->fld_next = QLI_variables;
QLI_variables = new;
}

static SYN decompile_field (
    FLD		field,
    CTX		context)
{
/**************************************
 *
 *	d e c o m p i l e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Take a perfectly good, completely compiled
 *	field block and regress to a SYN node and
 *	and a NAM block.  
 *	(Needed to support SQL idiocies)
 *
 **************************************/
SYN	node;
NAM	name;
int	args;

args = (context) ? 2 : 1;

node = (SYN) ALLOCDV (type_syn, args);
node->syn_type = nod_field;
node->syn_count = args;

name = decompile_symbol (field->fld_name);
node->syn_arg[0] = (SYN) name;

if (context) 
    {
    node->syn_arg[1] = node->syn_arg[0];
    if (context->ctx_symbol)
	name = decompile_symbol (context->ctx_symbol);
    else
	name = decompile_symbol (context->ctx_relation->rel_symbol);
    node->syn_arg[0] = (SYN) name; 
    }    

return node;
}

static NAM decompile_symbol (
    SYM		symbol)
{
/**************************************
 *
 *	d e c o m p i l e _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Turn a symbol back into a name
 *	(Needed to support SQL idiocies)
 *
 **************************************/
NAM	name;
int	l;
TEXT	*p, *q, c;

l =  symbol->sym_length;

name = (NAM) ALLOCDV (type_nam, l);
name->nam_length = l;
name->nam_symbol = symbol;

p = name->nam_string;
q = symbol->sym_string;

if (l)
   do {
	c = *q++;
	*p++ = UPPER (c);

#ifdef JPN_SJIS
    
 	/* Do not upcase second byte of a sjis kanji character */

        if ((KANJI1(c)) && (l > 1))
            {
            *p++ = *q++;
            --l;
            }

#endif

	} while (--l);

return name;
}

static NOD expand_assignment (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Expand an assigment statement.  All in all, not too tough.
 *
 **************************************/
NOD	node, from, to;
FLD	field;
TEXT	s[80];

node = MAKE_NODE (input->syn_type, e_asn_count);
node->nod_arg [e_asn_to] = to =
    expand_expression (input->syn_arg [s_asn_to], left);
node->nod_arg [e_asn_from] = from =
    expand_expression (input->syn_arg [s_asn_from], right);

if (to->nod_type == nod_field || to->nod_type == nod_variable)
    {
    field = (FLD) to->nod_arg [e_fld_field];
    if (field->fld_flags & FLD_computed)
	{
	ERRQ_print_error (138, field->fld_name->sym_string, NULL, NULL, NULL, NULL); /* Msg138 can't do assignment to computed field */
	}
    if (from->nod_type == nod_prompt)
	from->nod_arg [e_prm_field] = to->nod_arg [e_fld_field];
    if (field->fld_validation)
	node->nod_arg [e_asn_valid] = expand_expression (field->fld_validation, left);
    }

if (!node->nod_arg [e_asn_valid])
    --node->nod_count;

return node;
}

static NOD expand_any (
    SYN		input,
    LLS		stack)
{
/**************************************
 *
 *	e x p a n d _ a n y
 *
 **************************************
 *
 * Functional description
 *	Expand an any expression.  This would be trivial were it not
 *	for a funny SQL case when an expression needs to be checked
 *	for existence.
 *
 **************************************/
SYN	sub;
NOD	rse, node, negation, boolean;

node = MAKE_NODE (input->syn_type, e_any_count);
node->nod_count = 0;
node->nod_arg [e_any_rse] = rse = expand_rse (input->syn_arg [0], &stack);

if (input->syn_count >= 2 && input->syn_arg [1])
    {
    boolean = MAKE_NODE (nod_missing, 1);
    boolean->nod_arg [0] = expand_expression (input->syn_arg [1], stack);
    negation = MAKE_NODE (nod_not, 1);
    negation->nod_arg [0] = boolean;
    rse->nod_arg [e_rse_boolean] = make_and (rse->nod_arg [e_rse_boolean], negation);
    }

return node;
}

static NOD expand_boolean (
    SYN		input,
    LLS		stack)
{
/**************************************
 *
 *	e x p a n d _ b o o l e a n
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
NOD	node, *ptr, value;
FLD	field;
SSHORT	i;

/* Make node and process arguments */

node = MAKE_NODE (input->syn_type, input->syn_count);
ptr = node->nod_arg;
*ptr++ = value = expand_expression (input->syn_arg [0], stack);

for (i = 1; i < input->syn_count; i++, ptr++)
    if (!(*ptr = possible_literal (input->syn_arg [i], stack, TRUE)))
	*ptr = expand_expression (input->syn_arg [i], stack);

/* Try to match any prompts against fields to determine prompt length */

if (value->nod_type != nod_field)
    return node;

field = (FLD) value->nod_arg [e_fld_field];
ptr = &node->nod_arg [1];

for (i = 1; i < node->nod_count; i++, ptr++)
    if ((*ptr)->nod_type == nod_prompt)
	(*ptr)->nod_arg [e_prm_field] = (NOD) field;

return node;
}

static void expand_control_break (
    BRK		*ptr,
    LLS		right)
{
/**************************************
 *
 *	e x p a n d _ c o n t r o l _ b r e a k
 *
 **************************************
 *
 * Functional description
 *	Work on a report writer control break.  This is called recursively
 *	to handle multiple breaks.
 *
 **************************************/
BRK	control, list;

list = NULL;

while (control = *ptr)
    {
    *ptr = control->brk_next;
    control->brk_next = list;
    list = control;
    if (control->brk_field)
	control->brk_field = (SYN) expand_expression (control->brk_field, right);
    if (control->brk_line)
	control->brk_line = (SYN) expand_print_list (control->brk_line, right);
    }

*ptr = list;
}

static void expand_distinct (
    NOD		rse,
    NOD		node)
{
/**************************************
 *
 *	e x p a n d _ d i s t i n c t
 *
 **************************************
 *
 * Functional description
 *	We have run into a distinct count.  Add a reduced
 *	clause to it's parent.
 *
 **************************************/
NOD	list;
LLS	stack;

if (rse->nod_arg [e_rse_reduced])
    return;

stack = NULL;
LLS_PUSH (node, &stack);
LLS_PUSH (NULL_PTR, &stack);
rse->nod_arg [e_rse_reduced] = list = make_list (stack);
list->nod_count = 1;
}

static void expand_edit_string (
    NOD		node,
    ITM		item)
{
/**************************************
 *
 *	e x p a n d _ e d i t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Default edit_string and query_header.
 *
 **************************************/
FLD	field;
FUN	function;
MAP	map;

switch (node->nod_type)
    {
    case nod_min:
    case nod_rpt_min:
    case nod_agg_min:
	if (!item->itm_query_header)
	    item->itm_query_header = "MIN";

    case nod_total:
    case nod_running_total:
    case nod_rpt_total:
    case nod_agg_total:
	if (!item->itm_query_header)
	    item->itm_query_header = "TOTAL";

    case nod_average:
    case nod_rpt_average:
    case nod_agg_average:
	if (!item->itm_query_header)
	    item->itm_query_header = "AVG";

    case nod_max:
    case nod_rpt_max:
    case nod_agg_max:
	if (!item->itm_query_header)
	    item->itm_query_header = "MAX";
	expand_edit_string (node->nod_arg [e_stt_value], item);
	return;
    
    case nod_count:
    case nod_running_count:
    case nod_rpt_count:
    case nod_agg_count:
	if (!item->itm_edit_string)
	    item->itm_edit_string = "ZZZ,ZZZ,ZZ9";
	if (!item->itm_query_header)
	    item->itm_query_header = "COUNT";
	break;
	
    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	expand_edit_string (map->map_node, item);
	return;

    case nod_field:
    case nod_variable:
	break;

    case nod_function:
 	function = (FUN)node->nod_arg [e_fun_function];
	if (!item->itm_query_header)
	    item->itm_query_header = function->fun_symbol->sym_string;
	return;

    default:
	return;
    }

/* Handle fields */

field = (FLD) node->nod_arg [e_fld_field];

if (!item->itm_edit_string)
    item->itm_edit_string = field->fld_edit_string;

if (!item->itm_query_header)
    if (!(item->itm_query_header = field->fld_query_header))
	item->itm_query_header = field->fld_name->sym_string;
}

static NOD expand_erase (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
NOD	node, loop;
LLS	contexts;
CTX	context;
USHORT	count, i;

loop = NULL;
count = 0;

/* If there is an rse, make up a FOR loop */

if (input->syn_arg [s_era_rse])
    {
    loop = MAKE_NODE (nod_for, e_for_count);
    loop->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_era_rse],
					    &right);
    }

/* Loop thru contexts counting them. */

for (contexts = right; contexts; contexts = contexts->lls_next)
    {
    context = (CTX) contexts->lls_object;
    if (context->ctx_variable)
	continue;
    count++;
    if (context->ctx_rse)
	break;
    }

if (count == 0)
    IBERROR (139); /* Msg139 no context for ERASE */
else if (count > 1)
    IBERROR (140); /* Msg140 can't erase from a join */

/* Make up node big enough to hold fixed fields plus all contexts */

node = MAKE_NODE (nod_erase, e_era_count);
node->nod_arg [e_era_context] = (NOD) context;

if (!loop)
    return node;

loop->nod_arg [e_for_statement] = node;

return loop;
}

static NOD expand_expression (
    SYN		input,
    LLS		stack)
{
/**************************************
 *
 *	e x p a n d _ e x p r e s s i o n
 *
 **************************************
 *
 * Functional description
 *	Expand an expression.
 *
 **************************************/
NOD	*ptr, node;
STR	string;
CON	constant;
CTX	context;
NAM	name;
SYN	value;
SCHAR	s[80];
SSHORT	i;

switch (input->syn_type)
    {
    case nod_field:
	return expand_field (input, stack, NULL_PTR);

    case nod_null:
    case nod_user_name:
	return MAKE_NODE (input->syn_type, 0);

    case nod_any:
    case nod_unique:
	return expand_any (input, stack);

    case nod_max:
    case nod_min:
    case nod_count:
    case nod_average:
    case nod_total:
    case nod_from:

    case nod_rpt_max:
    case nod_rpt_min:
    case nod_rpt_count:
    case nod_rpt_average:
    case nod_rpt_total:

    case nod_running_total:
    case nod_running_count:
	node = MAKE_NODE (input->syn_type, e_stt_count);
	if (value = input->syn_arg [s_stt_rse])
	    node->nod_arg [e_stt_rse] = expand_rse (value, &stack);
	if (value = input->syn_arg [s_stt_value])
	    node->nod_arg [e_stt_value] = expand_expression (value, stack);
	if (value = input->syn_arg [s_stt_default])
	    node->nod_arg [e_stt_default] = expand_expression (value, stack);
	if (input->syn_arg [s_prt_distinct] && node->nod_arg [e_stt_rse] && node->nod_arg [e_stt_value]) 
	    expand_distinct (node->nod_arg [e_stt_rse], node->nod_arg [e_stt_value]);
/* count2 next 2 lines go */
	if (input->syn_type == nod_count)
	    node->nod_arg [e_stt_value] = 0;
    	return node;

    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_count:
    case nod_agg_average:
    case nod_agg_total:
	node = MAKE_NODE (input->syn_type, e_stt_count);
	for (; stack; stack = stack->lls_next)
	    {
	    context = (CTX) stack->lls_object;
	    if (context->ctx_type == CTX_AGGREGATE)
		break;
	    }
	if (!stack)
	    ERRQ_print_error (454, NULL, NULL, NULL, NULL, NULL); /* could not resolve context for aggregate */
/* count2
	if (value = input->syn_arg [s_stt_value])
	    {
	    node->nod_arg [e_stt_value] = expand_expression (value, stack->lls_next);
	    if (input->syn_arg [s_prt_distinct])
		expand_distinct (context->ctx_sub_rse, node->nod_arg [e_stt_value]);
	    }
*/
	if ((value = input->syn_arg [s_stt_value]) &&
            (input->syn_arg [s_prt_distinct] || (input->syn_type != nod_agg_count)))
	    {
	    node->nod_arg [e_stt_value] = expand_expression (value, stack->lls_next);
	    if (input->syn_arg [s_prt_distinct] || (input->syn_type == nod_agg_count &&
		context->ctx_sub_rse))
		expand_distinct (context->ctx_sub_rse, node->nod_arg [e_stt_value]);
	    }
	return post_map (node, context);

    case nod_index:
	value = input->syn_arg [s_idx_field];
	if (value->syn_type != nod_field)
            IBERROR (466);    /* Msg466 Only fields may be subscripted */
	return expand_field (value, stack, input->syn_arg [s_idx_subs]);

    case nod_list:
    case nod_upcase:

    case nod_and:
    case nod_or:
    case nod_not:
    case nod_missing:
    case nod_add:
    case nod_subtract:
    case nod_multiply:
    case nod_divide:
    case nod_negate:
    case nod_concatenate:
    case nod_substr:
	break;

    case nod_eql:
    case nod_neq:
    case nod_gtr:
    case nod_geq:
    case nod_leq:
    case nod_lss:
    case nod_between:
    case nod_matches:
    case nod_sleuth:
    case nod_like:
    case nod_starts:
    case nod_containing:
	return expand_boolean (input, stack);

    case nod_edit_blob:
	node = MAKE_NODE (input->syn_type, e_edt_count);
	node->nod_count = 0;
	if (input->syn_arg [0])
	    {
	    node->nod_count = 1;
	    node->nod_arg [0] = expand_expression (input->syn_arg [0], stack);
	    }
	return node;

    case nod_format:
	node = MAKE_NODE (input->syn_type, e_fmt_count);
	node->nod_count = 1;
	node->nod_arg [e_fmt_value] = 
		expand_expression (input->syn_arg [s_fmt_value], stack);
	node->nod_arg [e_fmt_edit] = (NOD) input->syn_arg [s_fmt_edit];
	return node;

    case nod_function:
	return expand_function (input, stack);

    case nod_constant:
	node = MAKE_NODE (input->syn_type, 0);
	constant = (CON) input->syn_arg [0];
	node->nod_desc = constant->con_desc;
	return node;

    case nod_prompt:
	node = MAKE_NODE (input->syn_type, e_prm_count);
	node->nod_arg [e_prm_prompt] = (NOD) input->syn_arg [0];
	return node;

    case nod_star:
	name = (NAM) input->syn_arg[0];
	ERRQ_print_error (141, name->nam_string, NULL, NULL, NULL, NULL); /* Msg141 can't be used when a single element is required */

    default:
	BUGCHECK (135); /* Msg135 expand_expression: not yet implemented */
    }

node = MAKE_NODE (input->syn_type, input->syn_count);
ptr = node->nod_arg;

for (i = 0; i < input->syn_count; i++)
    *ptr++ = expand_expression (input->syn_arg [i], stack);

return node;
}

static NOD expand_field (
    SYN		input,
    LLS		stack,
    SYN		subs)
{
/**************************************
 *
 *	e x p a n d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Expand a field reference.  Error if it can't be resolved.
 *	If the field belongs to a group by SQL expression, make
 *	sure it goes there.
 *
 **************************************/
NOD	node;
FLD	field;
CTX	context, parent, stream_context, *ptr, *end;
NAM	name;
USHORT	i, l;
TEXT	*p, *q, s [160];
LLS	save_stack;

if (!(field = resolve (input, stack, &context)) ||
   (subs && (context->ctx_type == CTX_FORM || context->ctx_variable)))
    {
    p = s;
    for (i = 0; i < input->syn_count; i++)
	{
	name = (NAM) input->syn_arg [i];
	q = name->nam_string;
	if (l = name->nam_length)
	    do *p++ = *q++; while (--l);
	*p++ = '.';
	}
    *--p = 0;
    if (field)
	ERRQ_print_error (467, s, NULL, NULL, NULL, NULL); /* Msg467 "%s" is not a field and so may not be subscripted */
    else
	ERRQ_print_error (142, s, NULL, NULL, NULL, NULL); /* Msg142 "%s" is undefined or used out of context */
    }

if (context->ctx_type == CTX_FORM)
    return make_form_field (context->ctx_form, field);

node = make_field (field, context);
if (subs)
    node->nod_arg [e_fld_subs] = expand_expression (subs, stack);

for (save_stack = stack; stack; stack = stack->lls_next)
    {
    parent = (CTX) stack->lls_object;
    if (parent->ctx_type == CTX_AGGREGATE)
	break;
    }

if (!parent)
    return node;
else if (context->ctx_parent != parent)
    {
    /* The parent context may be hidden because we are part of
       a stream context.  Check out this possibility. */

    for (; save_stack; save_stack = save_stack->lls_next)
	{
	stream_context = (CTX) save_stack->lls_object;
	if (stream_context->ctx_type != CTX_STREAM ||
	    stream_context->ctx_stream->nod_type != nod_rse)
	    continue;

	ptr = stream_context->ctx_stream->nod_arg + e_rse_count;
	end = ptr + stream_context->ctx_stream->nod_count;
	for (; ptr < end; ptr++)
	    if (*ptr == context)
		break;
	if (ptr < end && stream_context->ctx_parent == parent)
	    break;
	}

    if (!save_stack)
	return node;
    }

return post_map (node, parent);
}

static NOD expand_for (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ f o r
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
NOD	node;

node = MAKE_NODE (input->syn_type, e_for_count);
node->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_for_rse], &right);
node->nod_arg [e_for_statement] = 
	expand_statement (input->syn_arg [s_for_statement], right, left);

return node;
}

static FRM expand_form (
    SYN		input,
    REL		relation)
{
/**************************************
 *
 *	e x p a n d _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Lookup form name.  If there isn't a form name and a relation
 *	has been given, use the relation name.
 *
 **************************************/
DBB	database;
NAM	name;
FRM	form;
TEXT	*string;

/* Figure out form name */

if (input && (name = (NAM) input->syn_arg [s_frm_form]))
    string = name->nam_string;
else if (relation)
    string = relation->rel_symbol->sym_string;
else
    IBERROR (143); /* Msg143 no default form name */

/* Now figure out which database to look in */

database = NULL;

if (input)
    database = (DBB) input->syn_arg [s_frm_database]; 

if (!database && relation)
    database = relation->rel_database;

/* Look for form either in explicit or any database */

if (database)
    {
    if (form = FORM_lookup_form (database, string))
	return form;
    }
else
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	if (form = FORM_lookup_form (database, string))
	    return form;
    ERRQ_print_error (144, string, NULL, NULL, NULL, NULL); /* Msg144 No database for form */
    }

/* Form doesn't exist, try to make up a default */

if (relation && 
    !(input && input->syn_arg [s_frm_form]) &&
    (form = FORM_default_form (database, string)))
    return form;
    
ERRQ_print_error (145, string, database->dbb_filename, NULL, NULL, NULL); /* Msg145 form is not defined in database */

}

static NOD expand_form_for (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ f o r m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Expand a form invocation.  This involve augmenting both the
 *	right and left context stacks.
 *
 **************************************/
SYN	form_name;
SYM	symbol;
CTX	context, secondary;
FRM	form;
NOD	node;
NAM	ctx_name;

form_name = input->syn_arg [s_ffr_form];
context = (CTX) ALLOCD (type_ctx);
context->ctx_type = CTX_FORM;
context->ctx_form = form = expand_form (form_name, NULL_PTR);
LLS_PUSH (context, &left);

if (ctx_name = (NAM) form_name->syn_arg [s_frm_context])
    {
    context->ctx_symbol = symbol = (SYM) ALLOCDV (type_sym, 0);
    symbol->sym_string = ctx_name->nam_string;
    symbol->sym_length = ctx_name->nam_length;
    symbol->sym_type = SYM_form;
    }

secondary = (CTX) ALLOCD (type_ctx);
secondary->ctx_type = CTX_FORM;
secondary->ctx_primary = context;
LLS_PUSH (secondary, &right);

node = MAKE_NODE (input->syn_type, e_ffr_count);
node->nod_arg [e_ffr_form] = (NOD) form;
node->nod_arg [e_ffr_statement] = 
	expand_statement (input->syn_arg [s_ffr_statement], right, left);

return node;
}

static NOD expand_form_update (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ f o r m _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Expand a "form update" statement.
 *
 **************************************/
FRM	form;
FFL	field;
LLS	stack;
CTX	context;
NAM	name;
SYN	list, *ptr, *end;
NOD	node;

/* Find form context.  If there isn't one, punt */

for (stack = left; stack; stack = stack->lls_next)
    {
    context = (CTX) stack->lls_object;
    if (context->ctx_type == CTX_FORM)
	break;
    }

if (!stack)
    IBERROR (146); /* Msg146 no context for form ACCEPT statement */

form = context->ctx_form;
stack = NULL;

if (list = input->syn_arg [0])
    for (ptr = list->syn_arg, end = ptr + list->syn_count; ptr < end; ptr++)
	{
	name = (NAM) *ptr;
	if (!(field = FORM_lookup_field (form, name->nam_string)))
	    ERRQ_print_error (147, name->nam_string, form->frm_name, NULL, NULL, NULL); /* Msg147 field is not defined in form */
	LLS_PUSH (field, &stack);
	}

node = MAKE_NODE (input->syn_type, e_fup_count);
node->nod_arg [e_fup_form] = (NOD) form;

if (stack)
    node->nod_arg [e_fup_fields] = make_list (stack);

if (input->syn_arg [1])
    node->nod_arg [e_fup_tag] = expand_expression (input->syn_arg [1], right);

return node;
} 

static NOD expand_function (
    SYN		input,
    LLS		stack)
{
/**************************************
 *
 *	e x p a n d _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Expand a functionn reference.
 *	For a field or expression reference
 *	tied to a relation in a database
 *	use only that database.  For a variable
 *      reference, use any database that matches.
 *
 **************************************/
NOD	node;
SYM	symbol;
FUN	function;
CTX	context;
DBB	database;

node = MAKE_NODE (input->syn_type, e_fun_count);
node->nod_count = 1;
if (stack && (context = (CTX) stack->lls_object) && (context->ctx_type == CTX_RELATION)) 
    {
    if (context->ctx_primary)
	context = context->ctx_primary;
    database = context->ctx_relation->rel_database;
    for (symbol = (SYM) input->syn_arg [s_fun_function]; symbol; symbol = symbol->sym_homonym)
	if (symbol->sym_type == SYM_function)
	    {
	    function = (FUN) symbol->sym_object; 
	    if (function->fun_database == database)
		break;
	    }
    }
else
    for (database = QLI_databases; database; database = database->dbb_next)
	{
	for (symbol = (SYM) input->syn_arg [s_fun_function]; symbol; symbol = symbol->sym_homonym)
	    if (symbol->sym_type == SYM_function)
		{
		function = (FUN) symbol->sym_object; 
		if (function->fun_database == database)
		    break;
		}
	if (symbol)
	    break;
	}


if (!symbol)
    {
    symbol = (SYM) input->syn_arg [s_fun_function];
    ERRQ_error (412, symbol->sym_string, database->dbb_filename, NULL, NULL, NULL);
    } 

node->nod_arg [e_fun_function] = (NOD) function;

node->nod_arg [e_fun_args] = 
    expand_expression (input->syn_arg [s_fun_args], stack);

return node;
}

static NOD expand_group_by (
    SYN		input,
    LLS		stack,
    CTX		context)
{
/**************************************
 *
 *	e x p a n d _ g r o u p _ b y
 *
 **************************************
 *
 * Functional description
 *	Expand a GROUP BY clause.
 *
 **************************************/
SYN	*ptr, *end;
NOD	node, *ptr2;
USHORT	i;

node = MAKE_NODE (input->syn_type, input->syn_count);
ptr2 = node->nod_arg;

for (ptr = input->syn_arg, end = ptr + input->syn_count; ptr < end; ptr++, ptr2++)
    {
    *ptr2 = expand_expression (*ptr, stack);
    post_map (*ptr2, context);
    }

return node;
}

static NOD expand_menu (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ m e n u
 *
 **************************************
 *
 * Functional description
 *	Expand menu statement.
 *
 **************************************/
NOD	node;
SYN	sub, *ptr;

node = MAKE_NODE (input->syn_type, e_men_count);
node->nod_arg [e_men_string] = expand_expression (input->syn_arg [s_men_string], NULL_PTR);
node->nod_arg [e_men_labels] = expand_expression (input->syn_arg [s_men_labels], NULL_PTR);
node->nod_arg [e_men_statements] = expand_statement (input->syn_arg [s_men_statements], left, right);

return node;
}

static NOD expand_modify (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
NOD	node, loop, assignment, prompt, list, *ptr;
SYN	value, syn_list, *syn_ptr;
FLD	field;
LLS	contexts;
CTX	new_context, context;
USHORT	count, i;

loop = NULL;
count = 0;

/* If there is an rse, make up a FOR loop */

if (input->syn_arg [s_mod_rse])
    {
    loop = MAKE_NODE (nod_for, e_for_count);
    loop->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_mod_rse],
					    &right);
    }

/* Loop thru contexts counting them. */

for (contexts = right; contexts; contexts = contexts->lls_next)
    {
    context = (CTX) contexts->lls_object;
    if (context->ctx_variable)
	continue;
    count++;
    if (context->ctx_rse)
	break;
    }

if (!count)
    IBERROR (148); /* Msg148 no context for modify */

/* Make up node big enough to hold fixed fields plus all contexts */

node = MAKE_NODE (nod_modify, (int) e_mod_count + count);
node->nod_count = count;
ptr = &node->nod_arg [e_mod_count];

/* Loop thru contexts augmenting left context */

for (contexts = right; contexts; contexts = contexts->lls_next)
    {
    context = (CTX) contexts->lls_object;
    if (context->ctx_variable)
	continue;
    new_context = (CTX) ALLOCD (type_ctx);
    *ptr++ = (NOD) new_context;
    new_context->ctx_type = CTX_RELATION;
    new_context->ctx_source = context;
    new_context->ctx_symbol = context->ctx_symbol;
    new_context->ctx_relation = context->ctx_relation;
    LLS_PUSH (new_context, &left);
    if (context->ctx_rse)
	break;
    }

/* Process sub-statement, list of fields, or, sigh, none of the above */

if (input->syn_arg [s_mod_statement])
    node->nod_arg [e_mod_statement] = 
	expand_statement (input->syn_arg [s_mod_statement], right, left);
else if ((value = input->syn_arg [s_mod_form]) ||
    (QLI_form_mode && !input->syn_arg [s_mod_list]))
    node->nod_arg [e_mod_statement] = make_form_body (right, left, value);
else if (syn_list = input->syn_arg [s_mod_list])
    {
    node->nod_arg [e_mod_statement] = list = 
	MAKE_NODE (nod_list, syn_list->syn_count);
    ptr = list->nod_arg;
    syn_ptr = syn_list->syn_arg;
    for (i = 0; i < syn_list->syn_count; i++, syn_ptr++)
	*ptr++ = make_assignment (expand_expression (*syn_ptr, left), *syn_ptr, right);
    }
else
    IBERROR (149); /* Msg149 field list required for modify */

if (!loop)
    return node;

loop->nod_arg [e_for_statement] = node;

return loop;
}

static NOD expand_output (
    SYN		input,
    LLS		right,
    PRT		*print)
{
/**************************************
 *
 *	e x p a n d _ o u t p u t
 *
 **************************************
 *
 * Functional description
 *	Handle the presence (or absence) of an output specification clause.
 *
 **************************************/
NOD	output, node;

if (print)
    *print = (PRT) ALLOCD (type_prt);

if (!input)
    return NULL;

output = MAKE_NODE (nod_output, e_out_count);
LLS_PUSH (output, &output_stack);

if (!(node = possible_literal (input->syn_arg [s_out_file], right, FALSE)))
    node = expand_expression (input->syn_arg [s_out_file], right);

output->nod_arg [e_out_file] = node;
output->nod_arg [e_out_pipe] = (NOD) input->syn_arg [s_out_pipe];

if (print)
    output->nod_arg [e_out_print] = (NOD) *print;
    
return output;
}

static NOD expand_print (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
SYN	syn_list, syn_item, syn_rse, *sub, *end;
ITM	item;
NOD	node, loop, list, *ptr, rse; 
NOD	group_list, reduced;
LLS	items, new_right;
CTX	context;
REL	relation;
FLD	field;
PRT	print;
USHORT	i, count;
UCHAR	valid;

syn_rse = input->syn_arg [s_prt_rse];

/* Check to see if form mode is appropriate */

if (input->syn_arg [s_prt_form] ||
    (QLI_form_mode &&
       !input->syn_arg [s_prt_list] &&
       !input->syn_arg [s_prt_output] &&
       !(syn_rse && syn_rse->syn_count != 1)))
    return expand_print_form (input, right, left);

loop = NULL;
items = NULL;
rse = NULL;
count = 0;
new_right = right;

/* If an output file or pipe is present, make up an output node */

expand_output (input->syn_arg [s_prt_output], right, &print);

/* If a record select expression is present, expand it and build a FOR
   statement. */

if (syn_rse)
    {
    loop = MAKE_NODE (nod_for, e_for_count);
    loop->nod_arg [e_for_rse] = rse = expand_rse (syn_rse, &new_right);
    }

/* If there were any print items, process them now.  Look first for things that
   look like items, but are actually lists.  If there aren't items of any kind,
   pick up all fields in the relations from the record selection expression. */

if (syn_list = input->syn_arg [s_prt_list])
    for (sub = syn_list->syn_arg, end = sub + syn_list->syn_count; sub < end; sub++)
	{
	if (((*sub)->syn_type == nod_print_item) && 
		(syn_item = (*sub)->syn_arg [s_itm_value]) &&
		(syn_item->syn_type == nod_star))
	    count += generate_items (syn_item, new_right, &items, rse);
	else
	    {
	    LLS_PUSH (expand_print_item (*sub, new_right), &items);
	    count++;
	    }
	}
else if (syn_rse && (syn_list = syn_rse->syn_arg [s_rse_reduced]))
    for (sub = syn_list->syn_arg, end = sub + syn_list->syn_count; sub < end; sub += 2)
	{
	item = (ITM) ALLOCD (type_itm);
	item->itm_type = item_value;
	item->itm_value = expand_expression (*sub, new_right);
	expand_edit_string (item->itm_value, item);
	LLS_PUSH (item, &items);
	count++;
	}
else
    for (; new_right; new_right = new_right->lls_next)
	{
	context = (CTX) new_right->lls_object;
	relation = context->ctx_relation;
	if (!relation || context->ctx_sub_rse)
	    continue;
	for (field = relation->rel_fields; field; field = field->fld_next)
	    {
	    if ((field->fld_system_flag && field->fld_system_flag != relation->rel_system_flag) ||
		field->fld_flags & FLD_array)
		continue;
	    node = make_field (field, context);
	    if (rse && rse->nod_arg [e_rse_group_by] && 
		invalid_nod_field (node, rse->nod_arg [e_rse_group_by]))
		continue;
	    item = (ITM) ALLOCD (type_itm);
	    item->itm_type = item_value;
	    item->itm_value = make_field (field, context);
	    expand_edit_string (item->itm_value, item);
	    LLS_PUSH (item, &items);
	    count++;
	    }
	if (rse = context->ctx_rse)
	    break;
	}

/* If no print object showed up, complain! */

if (!count)
    IBERROR (150); /* Msg150 No items in print list */

/* Build new print statement.  Unlike the syntax node, the print statement
   has only print items in it. */

node = MAKE_NODE (input->syn_type, e_prt_count);
node->nod_arg [e_prt_list] = list = make_list (items);
node->nod_arg [e_prt_output] = (NOD) print;

/* If DISTINCT was requested, make up a reduced list. */

if (rse && input->syn_arg [s_prt_distinct])
    {
    reduced = MAKE_NODE (nod_list, list->nod_count * 2);
    reduced->nod_count = 0;
    for (i =0, ptr = reduced->nod_arg; i < list->nod_count; i++)
	{
	item = (ITM) list->nod_arg [i];
	if (item->itm_value)
	    {
	    *ptr++ = item->itm_value;
	    ptr++;
	    reduced->nod_count++;
	    }
	}
    if (reduced->nod_count)
	rse->nod_arg [e_rse_reduced] = reduced;
    }

/* If a FOR loop was generated, splice it in here. */

if (loop)
    {
    loop->nod_arg [e_for_statement] = node;
    node = loop;
    if (input->syn_arg [s_prt_order])
	rse->nod_arg [e_rse_sort] = expand_sort (input->syn_arg [s_prt_order],
		new_right, list);
    }

return node;
}

static NOD expand_print_form (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ p r i n t _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Expand a PRINT ... USING FORM <form> statement.
 *
 **************************************/
SYN	syn_rse;
NOD	node, loop, rse;
LLS	new_right;
REL	relation;

loop = rse = NULL;
new_right = right;

/* If a record select expression is present, expand it and build a FOR
   statement. */

if (syn_rse = input->syn_arg [s_prt_rse])
    {
    loop = MAKE_NODE (nod_for, e_for_count);
    loop->nod_arg [e_for_rse] = rse = expand_rse (syn_rse, &new_right);
    }


/* If no print object showed up, complain! */

if (!new_right)
    IBERROR (151); /* Msg151 No items in print list */

node = make_form_body (new_right, NULL_PTR, input->syn_arg [s_prt_form]);

/* If a FOR loop was generated, splice it in here. */

if (loop)
    {
    loop->nod_arg [e_for_statement] = node;
    node = loop;
    if (input->syn_arg [s_prt_order])
	rse->nod_arg [e_rse_sort] = expand_sort (input->syn_arg [s_prt_order],
		new_right, NULL_PTR);
    }

return node;
}

static ITM expand_print_item (
    SYN		syn_item,
    LLS		right)
{
/**************************************
 *
 *	e x p a n d _ p r i n t _ i t e m
 *
 **************************************
 *
 * Functional description
 *	Expand a print item.  A print item can either be a value or a format
 *	specifier.
 *
 **************************************/
ITM	item;
SYN	syn_expr;
NOD	node;
FLD	field;

item = (ITM) ALLOCD (type_itm);

switch (syn_item->syn_type)
    {
    case nod_print_item:
	item->itm_type = item_value;
	syn_expr = syn_item->syn_arg [s_itm_value];
	node = item->itm_value = expand_expression (syn_expr, right);
	item->itm_edit_string = (TEXT*) syn_item->syn_arg [s_itm_edit_string];
	item->itm_query_header = (TEXT*) syn_item->syn_arg [s_itm_header];
	expand_edit_string (node, item);
	return item;

    case nod_column:
	item->itm_type = item_column;
	break;

    case nod_tab:
	item->itm_type = item_tab;
	break;

    case nod_space:
	item->itm_type = item_space;
	break;

    case nod_skip:
	item->itm_type = item_skip;
	break;

    case nod_new_page:
	item->itm_type = item_new_page;
	break;

    case nod_column_header:
	item->itm_type = item_column_header;
	break;

    case nod_report_header:
	item->itm_type = item_report_header;
	break;

    }

item->itm_count = (int) syn_item->syn_arg [0];
return item;
}

static NOD expand_print_list (
    SYN		input,
    LLS		stack)
{
/**************************************
 *
 *	e x p a n d _ p r i n t _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Expand a print list.
 *
 **************************************/
LLS	items;
SYN	*ptr, *end;

items = NULL;

for (ptr = input->syn_arg, end = ptr + input->syn_count; ptr < end; ptr++)
    LLS_PUSH (expand_print_item (*ptr, stack), &items);

return make_list (items);
}

static NOD expand_report (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ r e p o r t
 *
 **************************************
 *
 * Functional description
 *	Expand a report specification.
 *
 **************************************/
RPT	report;
BRK	control;
SYN	sub;
NOD	node, loop;
PRT	print;

/* Start by processing record selection expression */

expand_output (input->syn_arg [s_prt_output], right, &print);
report = print->prt_report = (RPT) input->syn_arg [s_prt_list];

if (!(print->prt_lines_per_page = report->rpt_lines))
    print->prt_lines_per_page = QLI_lines;

if (!report->rpt_columns)
    report->rpt_columns = QLI_columns;

loop = MAKE_NODE (nod_report_loop, e_for_count);
loop->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_prt_rse], &right);
loop->nod_arg [e_for_statement] = node = MAKE_NODE (nod_report, e_prt_count);
node->nod_arg [e_prt_list] = (NOD) report;
node->nod_arg [e_prt_output] = (NOD) print;

/* Process clauses where they exist */

expand_control_break (&report->rpt_top_rpt, right);
expand_control_break (&report->rpt_top_page, right);
expand_control_break (&report->rpt_top_breaks, right);

if (sub = (SYN) report->rpt_detail_line)
    report->rpt_detail_line = expand_print_list (sub, right);

expand_control_break (&report->rpt_bottom_breaks, right);
expand_control_break (&report->rpt_bottom_page, right);
expand_control_break (&report->rpt_bottom_rpt, right);

return loop;
}

static NOD expand_restructure (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ r e s t r u c t u r e
 *
 **************************************
 *
 * Functional description
 *	Transform a restructure statement into a FOR <rse> STORE.
 *
 **************************************/
NOD	node, assignment, loop;
SYN	rel_node;
REL	relation;
FLD	field, fld;
CTX	context, ctx;
LLS	stack, search;

/* Make a FOR loop to drive the restructure */

loop = MAKE_NODE (nod_for, e_for_count);
loop->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_asn_from], &right);

/* Make a STORE node. */

loop->nod_arg [e_for_statement] = node = MAKE_NODE (nod_store, e_sto_count);
rel_node = input->syn_arg [s_asn_to];
context = (CTX) ALLOCD (type_ctx);
node->nod_arg [e_sto_context] = (NOD) context;
context->ctx_type = CTX_RELATION;
context->ctx_rse = (NOD) -1;
relation = context->ctx_relation = (REL) rel_node->syn_arg [s_rel_relation];

/* If we don't already know about the relation, find out now. */

if (!(relation->rel_flags & REL_fields))
    MET_fields (relation);

/* Match fields in target relation against fields in the input rse.  Fields
   may match on either name or query name. */

stack = NULL;

for (field = relation->rel_fields; field; field = field->fld_next)
    if (!(field->fld_flags & FLD_computed))
	{
	for (search = right; search; search = search->lls_next)
	    {
	    ctx = (CTX) search->lls_object;

	    /* First look for an exact field name match */

	    for (fld = ctx->ctx_relation->rel_fields; fld; fld = fld->fld_next)
		if (compare_symbols (field->fld_name, fld->fld_name))
		    break;

	    /* Next try, target field name matching source query name */

	    if (!fld)
		for (fld = ctx->ctx_relation->rel_fields; fld; fld = fld->fld_next)
		    if (compare_symbols (field->fld_name, fld->fld_query_name))
			break;

	    /* If nothing yet, look for any old match */

	    if (!fld)
		for (fld = ctx->ctx_relation->rel_fields; fld; fld = fld->fld_next)
		    if (compare_symbols (field->fld_query_name, fld->fld_name) ||
			compare_symbols (field->fld_query_name, fld->fld_query_name))
			break;

	    if (fld)
		{
		assignment = MAKE_NODE (nod_assign, e_asn_count);
		assignment->nod_count = e_asn_count - 1;
		assignment->nod_arg [e_asn_to] = make_field (field, context);
		assignment->nod_arg [e_asn_from] = make_field (fld, ctx);
		LLS_PUSH (assignment, &stack);
		goto found_field;
		}

	    if (ctx->ctx_rse)
		break;
	    }
	found_field: ;
	}

node->nod_arg [e_sto_statement] = make_list (stack);

return loop;
}

static NOD expand_rse (
    SYN		input,
    LLS		*stack)
{
/**************************************
 *
 *	e x p a n d _ r s e
 *
 **************************************
 *
 * Functional description
 *	Expand a record selection expression, returning an updated context
 *	stack.
 *
 **************************************/
NOD	node, boolean, eql_node, *ptr2, *end, parent_rse, temp;
CTX	context, parent_context;
SYN	*ptr, over, field, rel_node, list, value, stream;
SYM	symbol;
REL	relation;
LLS	old_stack, new_stack, short_stack;
USHORT	i, j;

old_stack = new_stack = *stack;
boolean = NULL;
node = MAKE_NODE (input->syn_type, (int) e_rse_count + input->syn_count);
node->nod_count = input->syn_count;
ptr2 = &node->nod_arg [e_rse_count];

/* Decide whether or not this is a GROUP BY, real or imagined
   If it is, disallow normal field type references */

parent_context = NULL;
parent_rse = NULL;

if (input->syn_arg [s_rse_group_by] || input->syn_arg [s_rse_having])
    parent_context = (CTX) ALLOCD (type_ctx);
if (list = input->syn_arg [s_rse_list])
    {
    for (i = 0; i < list->syn_count; i++)
	{
	value = list->syn_arg [i];
	if (!(field = value->syn_arg [e_itm_value]))
	    continue;
	if (global_agg (field, input->syn_arg[s_rse_group_by]))
	    {
	    if (!parent_context)
		parent_context = (CTX) ALLOCD (type_ctx); 
	    }
	else
	    if (parent_context) 
		if (invalid_syn_field (field, input->syn_arg [s_rse_group_by]))
		    IBERROR (451);
        }
    }

if (parent_context)
    {
    parent_context->ctx_type = CTX_AGGREGATE;
    parent_rse = MAKE_NODE (nod_rse, e_rse_count + 1);
    parent_rse->nod_count = 1;
    parent_rse->nod_arg [e_rse_count] = (NOD) parent_context;
    parent_context->ctx_sub_rse = node;
    }

/* Process the FIRST clause before the context gets augmented */

if (input->syn_arg [s_rse_first])
    node->nod_arg [e_rse_first] = 
	expand_expression (input->syn_arg [e_rse_first], old_stack);

/* Process relations */

ptr = input->syn_arg + s_rse_count;

for (i = 0; i < input->syn_count; i++)
    {
    rel_node = *ptr++;
    over = *ptr++;
    context = (CTX) ALLOCD (type_ctx);
    *ptr2++ = (NOD) context;
    if (i == 0)
	context->ctx_rse = node;
    if (rel_node->syn_type == nod_rse)
	{
	context->ctx_type = CTX_STREAM;
	context->ctx_stream = expand_rse (rel_node, &new_stack);
	}
    else
	{
	context->ctx_type = CTX_RELATION;
	relation = context->ctx_relation = (REL) rel_node->syn_arg [s_rel_relation];
	if (!(relation->rel_flags & REL_fields))
	    MET_fields (relation);
	symbol = context->ctx_symbol = (SYM) rel_node->syn_arg [s_rel_context];
	if (symbol)
	    symbol->sym_object = (BLK) context;
	if (over)
	    {
	    short_stack = NULL;
	    LLS_PUSH (context, &short_stack);
	    for (j = 0; j < over->syn_count; j++)
		{
		field = over->syn_arg [j];
		eql_node = MAKE_NODE (nod_eql, 2);
		eql_node->nod_arg [0] = expand_expression (field, short_stack);
		eql_node->nod_arg [1] = expand_expression (field, new_stack);
		boolean = make_and (eql_node, boolean);
		}
	    LLS_POP (&short_stack);
	    }
	}
    LLS_PUSH (context, &new_stack);
    }

/* Handle explicit boolean */

if (input->syn_arg [e_rse_boolean])
    boolean = make_and (boolean, 
	expand_expression (input->syn_arg [e_rse_boolean], new_stack));

/* Handle implicit boolean from SQL xxx IN (yyy FROM relation) */

if (input->syn_arg [s_rse_outer])
    {
    eql_node = MAKE_NODE ((NOD_T) input->syn_arg [s_rse_op], 2);
    eql_node->nod_arg [0] = 
	expand_expression (input->syn_arg [s_rse_outer], old_stack);
    eql_node->nod_arg [1] = 
	expand_expression (input->syn_arg [s_rse_inner], new_stack);
    if (input->syn_arg [s_rse_all_flag])
	eql_node = negate (eql_node);
    boolean = make_and (eql_node, boolean);
    }

node->nod_arg [e_rse_boolean] = boolean;

if (input->syn_arg [s_rse_sort])
    {
    temp = expand_sort (input->syn_arg [e_rse_sort], new_stack, NULL_PTR);
    if (parent_rse)
	parent_rse->nod_arg [e_rse_sort] = temp;
    else
	node->nod_arg [e_rse_sort] = temp;
    }                      
#ifdef PC_ENGINE
else if (input->syn_arg [s_rse_index])
    node->nod_arg [e_rse_index] = (NOD) input->syn_arg [s_rse_index];
#endif

if (input->syn_arg [s_rse_reduced])
    node->nod_arg [e_rse_reduced] = 
	expand_sort (input->syn_arg [e_rse_reduced], new_stack, NULL_PTR);

if (input->syn_arg [s_rse_group_by])
    parent_rse->nod_arg [e_rse_group_by] = 
	expand_group_by (input->syn_arg [s_rse_group_by], new_stack, parent_context);

node->nod_arg [e_rse_join_type] = (NOD) input->syn_arg [s_rse_join_type];

/* If there is a parent context, set it up here */

*stack = new_stack;

if (!parent_context)
    return node;

for (ptr2 = node->nod_arg + e_rse_count, end = ptr2 + node->nod_count; ptr2 < end; ptr2++)
    {
    context = (CTX) *ptr2;
    context->ctx_parent = parent_context;
    }

if (!(parent_context->ctx_relation = context->ctx_relation))
    parent_context->ctx_stream = context->ctx_stream;
LLS_PUSH (parent_context, stack);

if (input->syn_arg [s_rse_having])
    parent_rse->nod_arg [e_rse_having] = 
	expand_expression (input->syn_arg [s_rse_having], *stack);

return parent_rse;
}

static NOD expand_sort (
    SYN		input,
    LLS		stack,
    NOD		list)
{
/**************************************
 *
 *	e x p a n d _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Expand a sort or reduced clause.  This is more than a little
 *	kludgy.  For pure undiscipled, pragmatic reasons, the count for
 *	a sort/ reduced clause for a syntax node is twice the number of
 *	actual keys.  For node nodes, however, the count is the accurate
 *	number of keys.  So be careful.
 *
 **************************************/
ITM	item;
SYN	*syn_ptr, expr;
NOD	node, *ptr;
USHORT	i, position;

node = MAKE_NODE (nod_list, input->syn_count);
node->nod_count = input->syn_count / 2;
ptr = node->nod_arg;
syn_ptr = input->syn_arg;

for (i = 0; i < node->nod_count; i++)
    {
    expr = *syn_ptr++;
    if (expr->syn_type == nod_position)
	{
	position = (USHORT) expr->syn_arg [0];
	if (!list || !position || position > list->nod_count)
	    IBERROR (152); /* Msg152 invalid ORDER BY ordinal */
	item = (ITM) list->nod_arg [position - 1];
	*ptr++ = item->itm_value;
	}
    else
	*ptr++ = expand_expression (expr, stack);
    *ptr++ = (NOD) *syn_ptr++;
    }

return node;
}

static NOD expand_statement (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Expand a statement.
 *
 **************************************/
SYN	*syn_ptr, syn_node, field_node;
CTX	context;
NOD	node, (*routine)();
LLS	stack;
USHORT	i;

switch (input->syn_type)
    {
    case nod_abort:
	node = MAKE_NODE (input->syn_type, input->syn_count);
	if (input->syn_arg [0])
	    node->nod_arg [0] = expand_expression (input->syn_arg [0], right);
	return node;

    case nod_assign:
	routine = expand_assignment;
	break;

    case nod_commit_retaining:
	node = MAKE_NODE (input->syn_type, input->syn_count);
	for (i = 0; i < input->syn_count; i++)
	    node->nod_arg[i] = (NOD) input->syn_arg[i]; 
        return node;

    case nod_erase:
	routine = expand_erase;
	break;

    case nod_form_for:
	routine = expand_form_for;
	break;

    case nod_form_update:
	routine = expand_form_update;
	break;

    case nod_for:
	routine = expand_for;
	break;
    
    case nod_if:
	node = MAKE_NODE (input->syn_type, input->syn_count);
	node->nod_arg [e_if_boolean] = expand_expression (
		input->syn_arg [s_if_boolean], right);
	node->nod_arg [e_if_true] = expand_statement (
		input->syn_arg [s_if_true], right, left);
	if (input->syn_arg [s_if_false])
	    node->nod_arg [e_if_false] = expand_statement (
		input->syn_arg [s_if_false], right, left);
	else 
	    node->nod_count = 2;
	return node;

    case nod_menu:
	routine = expand_menu;
	break;

    case nod_modify:
	routine = expand_modify;
	break;

    case nod_print:
    case nod_list_fields:
	routine = expand_print;
	break;
    
    case nod_report:
	routine = expand_report;
	break;

    case nod_restructure:
	routine = expand_restructure;
	break;

    case nod_store:
	routine = expand_store;
	break;

    case nod_repeat:
	node = MAKE_NODE (input->syn_type, input->syn_count);
	node->nod_arg [e_rpt_value] = expand_expression (
		input->syn_arg [s_rpt_value], left);
	node->nod_arg [e_rpt_statement] = expand_statement (
		input->syn_arg [s_rpt_statement], right, left);
	return node;

    case nod_list:
	syn_ptr = input->syn_arg;
	stack = NULL;
	for (i = 0; i < input->syn_count; i++)
	    {
	    syn_node = *syn_ptr++;
	    if (syn_node->syn_type == nod_declare)
		{
		context = (CTX) ALLOCD (type_ctx);
		context->ctx_type = CTX_VARIABLE;
		if (field_node = syn_node->syn_arg[1])
		    {
		    if (field_node->syn_type == nod_index)
			field_node = field_node->syn_arg [s_idx_field];
		    resolve_really (syn_node->syn_arg [0], field_node);
		    }        
		context->ctx_variable = (FLD) syn_node->syn_arg [0];
		LLS_PUSH (context, &right);                              
		LLS_PUSH (context, &left);
		}
	    else
		if (node = expand_statement (syn_node, right, left))
		    LLS_PUSH (node, &stack);
	    }
	return make_list (stack);

    case nod_declare:
	return NULL;

    default:
	BUGCHECK (136); /* Msg136 expand_statement: not yet implemented */
    }

return (*routine) (input, right, left);
}

static NOD expand_store (
    SYN		input,
    LLS		right,
    LLS		left)
{
/**************************************
 *
 *	e x p a n d _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Process, yea expand, on a mere STORE statement.  Make us
 *	something neat if nothing looks obvious.
 *
 **************************************/
NOD	node, assignment, prompt, loop;
SYN	sub, rel_node;
REL	relation;
FLD	field;
SYM	symbol;
CTX	context, secondary;
LLS	stack;
SSHORT	field_count, value_count;

loop = NULL;

/* If there is an rse, make up a FOR loop */

if (input->syn_arg [s_sto_rse])
    {
    loop = MAKE_NODE (nod_for, e_for_count);
    loop->nod_arg [e_for_rse] = expand_rse (input->syn_arg [s_sto_rse],
					    &right);
    }

node = MAKE_NODE (input->syn_type, e_sto_count);

rel_node = input->syn_arg [s_sto_relation];
context = (CTX) ALLOCD (type_ctx);
node->nod_arg [e_sto_context] = (NOD) context;
context->ctx_type = CTX_RELATION;
context->ctx_rse = (NOD) -1;
relation = context->ctx_relation = (REL) rel_node->syn_arg [s_rel_relation];

if (!(relation->rel_flags & REL_fields))
    MET_fields (relation);

if (symbol = context->ctx_symbol = (SYM) rel_node->syn_arg [s_rel_context])
    symbol->sym_object = (BLK) context;

LLS_PUSH (context, &left);

/*  If there are field and value lists, process them  */

if (input->syn_arg [s_sto_values])
    {
    if (!input->syn_arg [s_sto_fields])
	{
	stack = NULL;
	for (field = relation->rel_fields; field; field = field->fld_next)
	    LLS_PUSH (decompile_field (field, NULL_PTR), &stack);
	input->syn_arg [s_sto_fields] = (SYN) stack;
	}
    expand_values (input, right);
    }     

/* Process sub-statement.  If there isn't one, make up a series of
   assignments. */

if (input->syn_arg [s_sto_statement])
    {
    secondary = (CTX) ALLOCD (type_ctx);
    secondary->ctx_type = CTX_RELATION;
    secondary->ctx_primary = context;
    LLS_PUSH (secondary, &right);
    node->nod_arg [e_sto_statement] = 
	expand_statement (input->syn_arg [s_sto_statement], right, left);
    }
else if ((sub = input->syn_arg [s_sto_form]) || QLI_form_mode)
    node->nod_arg [e_sto_statement] = make_form_body (NULL_PTR, left, sub);
else
    {
    stack = NULL;
    for (field = relation->rel_fields; field; field = field->fld_next)
	{
	if (field->fld_flags & FLD_computed)
	    continue;
	if ((field->fld_system_flag && field->fld_system_flag != relation->rel_system_flag) ||
	    field->fld_flags & FLD_array)
	    continue;
	assignment = make_assignment (make_field (field, context), NULL_PTR, NULL_PTR);
	LLS_PUSH (assignment, &stack);
	}
    node->nod_arg [e_sto_statement] = make_list (stack);
    }

if (!loop)
    return node;

loop->nod_arg [e_for_statement] = node;

return loop;
}

static void expand_values (
    SYN		input,
    LLS		right)
{
/**************************************
 *
 *	e x p a n d _ v a l u e s
 *
 **************************************
 *
 * Functional description
 *	We've got a grungy SQL insert, and we have
 *	to make the value list match the field list.
 *	On the way in, we got the right number of 
 *	fields.  Now all that's needed is the values
 *	and matching the two lists, and generating 
 *	assignments.  If the input is from a select,
 *	things may be harder, and if there are wild cards
 *	things will be harder still.  Wild cards come in
 *	two flavors * and <context>.*.  The first is
 *	a nod_prompt, the second a nod_star.
 *
 **************************************/
SSHORT	field_count, value_count, group;
SYN	value, *ptr, list, assignment;
CTX	context;
LLS	stack, fields, values, temp;
                                                                   
field_count = value_count = 0;

/* fields have already been checked and expanded.  Just count them  */

fields = (LLS) input->syn_arg [s_sto_fields];
for (stack = fields; stack; stack = stack->lls_next)
    field_count++;

/* We're going to want the values in the order listed in the command */ 

values = (LLS) input->syn_arg [s_sto_values];
for (; values; LLS_PUSH (LLS_POP (&values), &stack))
    ;

/* now go through, count, and expand where needed */

while (stack)
    {
    value = (SYN) LLS_POP (&stack);
    if (input->syn_arg [s_sto_rse] && value->syn_type == nod_prompt)
	{
	if (value->syn_arg [0] == 0)
	    {
	    temp = NULL;
	    for (; right; right = right->lls_next) 
		LLS_PUSH (right->lls_object, &temp);
	    while (temp)
		{
		context = (CTX) LLS_POP (&temp);
		value_count += generate_fields (context, &values, input->syn_arg [s_sto_rse]); 
		}
	    }
	else
	    IBERROR (542); /* this was a prompting expression.  won't do at all */
	}
    else if (input->syn_arg[s_sto_rse] && (value->syn_type == nod_star))	    
	{
	if (!(context = find_context (value->syn_arg [0], right)))
	    IBERROR (154); /* Msg154 unrecognized context */ 
	value_count += generate_fields (context, &values, input->syn_arg [s_sto_rse]); 
	}
    else
	{
	LLS_PUSH (value, &values);
	value_count++;
	}
    }

/* Make assignments from values to fields */

if (field_count != value_count)
    IBERROR (189); /* Msg189 the number of values do not match the number of fields */

list = (SYN) ALLOCDV (type_syn, value_count);
list->syn_type = nod_list;
list->syn_count = value_count;
input->syn_arg [s_sto_statement] = list;
ptr = list->syn_arg + value_count;

while (values)
    {
    *--ptr = assignment = (SYN) ALLOCDV (type_syn, s_asn_count);
    assignment->syn_type = nod_assign;
    assignment->syn_count = s_asn_count;
    assignment->syn_arg [s_asn_to] = (SYN) LLS_POP (&fields);
    assignment->syn_arg [s_asn_from] = (SYN) LLS_POP (&values);
    }
}

static CTX find_context (
    NAM		name,
    LLS		contexts)
{
/**************************************
 *
 *	f i n d _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	We've got a context name and we need to return
 *	the context block implicated.
 *
 **************************************/
REL	relation;
CTX	context;

for (; contexts; contexts = contexts->lls_next)
    {
    context = (CTX) contexts->lls_object;
    relation = context->ctx_relation;
    if (compare_names (name, relation->rel_symbol))
        break;
    if (compare_names (name, context->ctx_symbol))
	break;
    }
return (contexts) ? context : NULL;
}

static int generate_fields (
    CTX		context,
    LLS		values,
    SYN		rse)
{
/**************************************
 *
 *	g e n e r a t e _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Expand an asterisk expression, which
 *	could be <relation>.* or <alias>.* or <context>.*
 *	into a list of non-expanded field blocks for
 *	input to a store or update.
 *
 **************************************/
int	count;
REL	relation;
FLD	field;
NOD	temp;
SYN	value, group_list;

if (context->ctx_type == CTX_AGGREGATE)
    return NULL;
group_list = rse->syn_arg [s_rse_group_by];
relation = context->ctx_relation;
count = 0;

for (field = relation->rel_fields; field; field = field->fld_next)
    {
    if ((field->fld_system_flag && field->fld_system_flag != relation->rel_system_flag) ||
	field->fld_flags & FLD_array)
	    continue;
    value = decompile_field (field, context);
    if (group_list && invalid_syn_field (value, group_list))
	continue;
    LLS_PUSH (value, values);
    count++;
    }

return count;
}

static int generate_items (
    SYN		symbol,
    LLS		right,
    LLS		items,
    NOD		rse)
{
/**************************************
 *
 *	g e n e r a t e _ i t e m s
 *
 **************************************
 *
 * Functional description
 *	Expand an asterisk expression, which
 *	could be <relation>.* or <alias>.* or <context>.*
 *	into a list of reasonable print items. 
 *
 *      If the original request included a group by,
 *	include only the grouping fields.
 *
 **************************************/
int	count;
REL	relation;
CTX	context;
NAM	name;
ITM	item;
FLD	field;
NOD	node, group_list;

count = 0;
group_list = (rse) ? rse->nod_arg [e_rse_group_by] : NULL;

/* first identify the relation or context */

if (symbol->syn_count == 1)
    name = (NAM) symbol->syn_arg [0];
else
    IBERROR (153); /* Msg153 asterisk expressions require exactly one qualifying context */

if (!(context = find_context (name, right)))
    IBERROR (154); /* Msg154 unrecognized context */

relation = context->ctx_relation;

for (field = relation->rel_fields; field; field = field->fld_next)
    {
    if ((field->fld_system_flag && field->fld_system_flag != relation->rel_system_flag) ||
	field->fld_flags & FLD_array)
	continue;
    node = make_field (field, context);
    if (group_list && invalid_nod_field (node, group_list))
	continue;
    item = (ITM) ALLOCD (type_itm);
    item->itm_type = item_value;
    item->itm_value = make_field (field, context);
    expand_edit_string (item->itm_value, item);
    LLS_PUSH (item, items);
    count++;
    }

return count;
}

static SLONG global_agg (
    SYN		item,
    SYN		group_list)
{
/**************************************
 *
 *	g l o b a l _ a g g
 *
 **************************************
 *
 * Functional description
 *	We've got a print list item that may contain
 *	a sql global aggregate.  If it does, we're
 *	going to make the whole thing a degenerate
 *	group by.  Anyway.  Look for aggregates buried
 *	deep within printable things. 
 *
 *	This recurses.  If it finds a mixture of normal
 *	and aggregates it complains.
 *
 **************************************/
SYN	*ptr, *end;
int	normal_field, aggregate;

normal_field = FALSE;
aggregate = FALSE;
                                       
switch (item->syn_type)
    {
    case nod_agg_average:
    case nod_agg_max:
    case nod_agg_min:
    case nod_agg_total:
    case nod_agg_count:
    case nod_running_total:
    case nod_running_count:
	return TRUE;

    case nod_upcase:
    case nod_add:
    case nod_subtract:
    case nod_multiply:
    case nod_divide:
    case nod_negate:
    case nod_concatenate:
    case nod_substr:
	{
	for (ptr = item->syn_arg, end = ptr + item->syn_count; ptr < end; ptr++)
	    {
	    if ((*ptr)->syn_type == nod_constant)
		continue;
	    if (global_agg (*ptr, group_list)) 
		aggregate = TRUE;
	    else
		if (!group_list || invalid_syn_field (*ptr, group_list))
		    normal_field = TRUE; 
	    }
	}

    default:
	break;
    }

if (normal_field && aggregate)
    IBERROR (451);

return aggregate;
}

static int invalid_nod_field (
    NOD		node,
    NOD		list)
{
/**************************************
 *
 *	i n v a l i d _ n o d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *
 *	Validate that an expanded field / context
 *	pair is in a specified list.  Thus is used
 *	in one instance to check that a simple field selected 
 *	through a grouping rse is a grouping field - 
 *	thus a valid field reference.
 *
 **************************************/
FLD		field;
CTX		context; 
SCHAR		invalid;
NOD		*ptr, *end;	

if (!list)
    return TRUE;

invalid = FALSE;

if (node->nod_type == nod_field)
    {
    field = (FLD) node->nod_arg[e_fld_field];
    context = (CTX) node->nod_arg[e_fld_context];
    for (ptr = list->nod_arg, end = ptr + list->nod_count; ptr < end; ptr++)
	if (field == (FLD) (*ptr)->nod_arg[e_fld_field] &&
	    context == (CTX) (*ptr)->nod_arg[e_fld_context])
	    return FALSE;
    return TRUE; 
    }
else
    for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
	switch ((*ptr)->nod_type)
	    {
	    case nod_field:
	    case nod_add:
	    case nod_subtract:
	    case nod_multiply:
	    case nod_divide:
	    case nod_negate:
	    case nod_concatenate:
	    case nod_substr:
	    case nod_format:
	    case nod_choice:
	    case nod_function:
	    case nod_upcase:
		invalid |= invalid_nod_field (*ptr, list);
	    } 

return invalid;
}

static int invalid_syn_field (
    SYN		syn_node,
    SYN		list)
{
/**************************************
 *
 *	i n v a l i d _ s y n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make sure an unexpanded simple field selected 
 *	through a grouping rse is a grouping field - 
 *	thus a valid field reference.  For the sake of
 *      argument, we'll match qualified to unqualified
 *	reference, but qualified reference must match
 *	completely. 
 *
 *	One more thought.  If this miserable thing is
 *	a wild card, let it through and expand it 
 *	correctly later.
 *
 **************************************/
SYN	element, *ptr, *end;
NAM	gname, fname, gctx, fctx;
SSHORT	count;
SCHAR	invalid;


if (syn_node->syn_type == nod_star)
    return FALSE;

if (!list)
    return TRUE;
 
invalid = FALSE;

if (syn_node->syn_type == nod_field)
    {
    fctx = NULL;
    fname = (NAM) syn_node->syn_arg[0];
    if (syn_node->syn_count == 2)
	{
	fctx = fname;
	fname = (NAM) syn_node->syn_arg[1];
	}

    for (count = list->syn_count; count;)
	{
	gctx = NULL;
	element = list->syn_arg [--count];
	gname = (NAM) element->syn_arg[0];
	if (element->syn_count == 2)
	    {
	    gctx = gname; 
	    gname = (NAM) element->syn_arg[1];
	    }
	if (!strcmp (fname->nam_string, gname->nam_string))
	    if (!gctx || !fctx ||(!strcmp (fctx->nam_string, gctx->nam_string))) 
		return FALSE;
	}
    return TRUE;
    }
else
    for (ptr = syn_node->syn_arg, end = ptr + syn_node->syn_count; ptr < end; ptr++)
	switch ((*ptr)->syn_type)
	{
	case nod_field:
	case nod_add:
	case nod_subtract:
	case nod_multiply:
	case nod_divide:
	case nod_negate:
	case nod_concatenate:
	case nod_substr:
	case nod_format:
	case nod_choice:
	case nod_function:
	case nod_upcase:
	    invalid |= invalid_syn_field (*ptr, list);
	} 

return invalid;
}		

static NOD make_and (
    NOD		expr1,
    NOD		expr2)
{
/**************************************
 *
 *	m a k e _ a n d 
 *
 **************************************
 *
 * Functional description
 *	Combine two expressions, each possible null, into at most
 *	a single boolean.
 *
 **************************************/
NOD	node;

if (!expr1)
    return expr2;

if (!expr2)
    return expr1;

node = MAKE_NODE (nod_and, 2);
node->nod_arg [0] = expr1;
node->nod_arg [1] = expr2;

return node;
}

static NOD make_assignment (
    NOD		target,
    NOD		initial,
    LLS		right)
{
/**************************************
 *
 *	m a k e _ a s s i g n m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate a prompt and assignment to a field.
 *
 **************************************/
NOD	assignment, prompt;
FLD	field;
LLS	stack;

field = (FLD) target->nod_arg [e_fld_field];
stack = NULL;
LLS_PUSH (target->nod_arg [e_fld_context], &stack);

if (field->fld_dtype == dtype_blob)
    {
    prompt = MAKE_NODE (nod_edit_blob, e_edt_count);
    prompt->nod_count = 0;
    prompt->nod_arg [e_edt_name] = (NOD) field->fld_name->sym_string;
    if (initial)
	{
	prompt->nod_count = 1;
	prompt->nod_arg [e_edt_input] = expand_expression (initial, right);
	}
    }
else
    {
    prompt = MAKE_NODE (nod_prompt, e_prm_count);
    prompt->nod_arg [e_prm_prompt] = (NOD) field->fld_name->sym_string;
    prompt->nod_arg [e_prm_field] = (NOD) field;
    }

assignment = MAKE_NODE (nod_assign, e_asn_count);
assignment->nod_arg [e_asn_to] = target;
assignment->nod_arg [e_asn_from] = prompt;

if (field->fld_validation)
    assignment->nod_arg [e_asn_valid] = 
	    expand_expression (field->fld_validation, stack);
else
    --assignment->nod_count;

LLS_POP (&stack);

return assignment;
}

static NOD make_form_body (
    LLS		right,
    LLS		left,
    SYN		form_node)
{
/**************************************
 *
 *	m a k e _ f o r m _ b o d y
 *
 **************************************
 *
 * Functional description
 *	Build the form body for a form STORE, MODIFY, or PRINT.
 *
 **************************************/
CTX	tmp_context;
FFL	ffl;
FLD	field, initial;
FRM	form;
LLS	stack, fields;
NOD	node, assignment;
REL	relation;

tmp_context = right ? (CTX) right->lls_object : (CTX) left->lls_object;
if (right)
    tmp_context = (CTX) right->lls_object;
else
    tmp_context = (CTX) left->lls_object;

relation = tmp_context->ctx_relation;
form = expand_form (form_node, relation);                  

/* Get stack of database fields and form fields */

stack = fields = NULL;

if (right)
    for (ffl = form->frm_fields; ffl; ffl = ffl->ffl_next)
	if (field = resolve_name (ffl->ffl_symbol, right, &tmp_context))
	    {
	    assignment = MAKE_NODE (nod_assign, e_asn_count);
	    assignment->nod_arg [e_asn_from] = make_field (field, tmp_context);
	    assignment->nod_arg [e_asn_to] = make_form_field (form, ffl);
	    LLS_PUSH (assignment, &stack);
	    }

/* Build form update */

node = MAKE_NODE (nod_form_update, e_fup_count);
node->nod_count = 0;
node->nod_arg [e_fup_form] = (NOD) form;
LLS_PUSH (node, &stack);

/* Unless this is a print, be prepared to update fields */

if (left)
    {
    for (ffl = form->frm_fields; ffl; ffl = ffl->ffl_next)
	if ((field = resolve_name (ffl->ffl_symbol, left, &tmp_context)) &&
	    !(field->fld_flags & FLD_computed))
	    {
	    assignment = MAKE_NODE (nod_assign, e_asn_count);
	    assignment->nod_arg [e_asn_to] = make_field (field, tmp_context);
	    assignment->nod_arg [e_asn_from] = make_form_field (form, ffl);
            if (right)
               {
               initial = resolve_name (ffl->ffl_symbol, right, &tmp_context);
               assignment->nod_arg [e_asn_initial] = make_field (initial, tmp_context);
               }
	    if (field->fld_validation)
		assignment->nod_arg [e_asn_valid] = expand_expression (field->fld_validation, left);
	    LLS_PUSH (ffl, &fields);
	    LLS_PUSH (assignment, &stack);
	    }
    node->nod_arg [e_fup_fields] = make_list (fields);
    }

node = MAKE_NODE (nod_form_for, e_ffr_count);
node->nod_arg [e_ffr_form] = (NOD) form;
node->nod_arg [e_ffr_statement] = make_list (stack);

return node;
}

static NOD make_form_field (
    FRM		form,
    FFL		field)
{
/**************************************
 *
 *	m a k e _ f o r m _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate a form field node.
 *
 **************************************/
NOD	node;

node = MAKE_NODE (nod_form_field, e_ffl_count);
node->nod_count = 0;
node->nod_arg [e_ffl_form] = (NOD) form;
node->nod_arg [e_ffl_field] = (NOD) field;

return node;
}

static NOD make_field (
    FLD		field,
    CTX		context)
{
/**************************************
 *
 *	m a k e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make a field block.  Not too tough.
 *
 **************************************/
NOD	node;

node = MAKE_NODE (nod_field, e_fld_count);
node->nod_count = 0;
node->nod_arg [e_fld_field] = (NOD) field;
node->nod_arg [e_fld_context] = (NOD) context;

if (context->ctx_variable)
    node->nod_type = nod_variable;

return node;
}

static NOD make_list (
    LLS		stack)
{
/**************************************
 *
 *	m a k e _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Dump a stack of junk into a list node.  Best count
 *	them first.
 *
 **************************************/
NOD	node, *ptr;
LLS	temp;
USHORT	count;

temp = stack;
count = 0;

while (temp)
    {
    count++;
    temp = temp->lls_next;
    }

node = MAKE_NODE (nod_list, count);
ptr = &node->nod_arg [count];

while (stack)
    *--ptr = (NOD) LLS_POP (&stack);

return node;
}

static NOD make_node (
    NOD_T	type,
    USHORT	count)
{
/**************************************
 *
 *	m a k e _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Allocate a node and fill in some basic stuff.
 *
 **************************************/
NOD	node;

node = (NOD) ALLOCDV (type_nod, count);
node->nod_type = type;
node->nod_count = count;

return node;
}

static NOD negate (
    NOD		expr)
{
/**************************************
 *
 *	n e g a t e
 *
 **************************************
 *
 * Functional description
 *	Build negation of expression.
 *
 **************************************/
NOD	node;

node = MAKE_NODE (nod_not, 1);
node->nod_arg [0] = expr;

return node;
}

static NOD possible_literal (
    SYN		input,
    LLS		stack,
    USHORT	upper_flag)
{
/**************************************
 *
 *	p o s s i b l e _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Check to see if a value node is an unresolved name.  If so,
 *	transform it into a constant expression.  This is used to
 *	correct "informalities" in relational expressions.
 *
 **************************************/
NAM	name;
CON	constant;
NOD	node;
CTX	context;
USHORT	l;
TEXT	*p, *q, c;

/* If the value isn't a field, is qualified, or can be resolved,
   it doesn't qualify for conversion.  Return NULL. */

if (input->syn_type != nod_field || 
    input->syn_count != 1 ||
    resolve (input, stack, &context))
    return NULL;

name = (NAM) input->syn_arg [0];
l = name->nam_length;
constant = (CON) ALLOCDV (type_con, l);
constant->con_desc.dsc_dtype = dtype_text;
constant->con_desc.dsc_length = l;
p = (TEXT*) constant->con_data;
constant->con_desc.dsc_address = (UCHAR*) p;
q = name->nam_string;

if (upper_flag)
    {
    if (l)
#ifdef JPN_SJIS
       do {
           c = *q++;
           *p++ = UPPER (c);

    	   /* Do not upcase second byte of a sjis kanji character */

           if ((KANJI1(c)) && (l > 1))
                {
                *p++ = *q++;
                --l;
                }
        } while (--l);

#else
	do {c = *q++; *p++ = UPPER (c);} while (--l);
#endif

    }
else
    if (l)
	do {
	    c = *q++; 
	    *p++ = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;

#ifdef JPN_SJIS

    	   /* Do not upcase second byte of a sjis kanji character */

           if ((KANJI1(c)) && (l > 1))
                {
                *p++ = *q++;
                --l;
                }
#endif
	} while (--l);

node = MAKE_NODE (nod_constant, 0);
node->nod_desc = constant->con_desc;

return node;
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

/* Check to see if the item has already been posted */

for (map = context->ctx_map; map; map = map->map_next)
    if (CMP_node_match (node, map->map_node))
	break;

if (!map)
    {
    map = (MAP) ALLOCD (type_map);
    map->map_next = context->ctx_map;
    context->ctx_map = map;
    map->map_node = node;
    }

new_node = MAKE_NODE (nod_map, e_map_count);
new_node->nod_count = 0;
new_node->nod_arg [e_map_context] = (NOD) context;
new_node->nod_arg [e_map_map] = (NOD) map;
new_node->nod_desc = node->nod_desc;

return new_node;
}

static FLD resolve (
    SYN		node,
    LLS		stack,
    CTX		*out_context)
{
/**************************************
 *
 *	r e s o l v e
 *
 **************************************
 *
 * Functional description
 *	Resolve a field node against a context stack.  Return both the
 *	field block (by value)  and the corresponding context block (by
 *	reference).  Return NULL if field can't be resolved.
 *
 **************************************/
NAM	name, *base, *ptr;
CTX	context;
REL	relation;
FLD	field;
FRM	form;
FFL	ffield;

/* Look thru context stack looking for a context that will resolve
   all name segments.  If the context is a secondary context, require
   that the context name be given explicitly (used for special STORE
   context). */

base = (NAM*) node->syn_arg;

for (; stack; stack = stack->lls_next)
    {
    *out_context = context = (CTX) stack->lls_object;
    ptr = base + node->syn_count;
    name = *--ptr;

    switch (context->ctx_type)
	{
	case CTX_FORM:
	    if (context->ctx_primary)
		*out_context = context = context->ctx_primary;
	    form = context->ctx_form;
	    for (ffield = form->frm_fields; ffield; ffield = ffield->ffl_next)
		if (compare_names (name, ffield->ffl_symbol))
		    {
		    if (ptr == base)
			return (FLD) ffield;
		    name = *--ptr;
		    
		    if (compare_names (name, form->frm_symbol))
			if (ptr == base)
			    return (FLD) ffield;
		    
		    if (compare_names (name, context->ctx_symbol))
			if (ptr == base)
			    return (FLD) ffield;
		    break;
		    }
	    break;

	case CTX_VARIABLE:
	    if (ptr == base)
		for (field = context->ctx_variable; field; field = field->fld_next)
		    if (compare_names (name, field->fld_name) ||
			compare_names (name, field->fld_query_name))
			    return field;
	    break;

	case CTX_RELATION:
	    if (context->ctx_primary)
		{
		*out_context = context = context->ctx_primary;
		if (!compare_names (node->syn_arg [0], context->ctx_symbol))
		    break;
		}
	    relation = context->ctx_relation;

	    for (field = relation->rel_fields; field; field = field->fld_next)
		if (compare_names (name, field->fld_name) ||
		    compare_names (name, field->fld_query_name))
		    {
		    if (ptr == base)
			return field;
		    name = *--ptr;
		    
		    if (compare_names (name, relation->rel_symbol))
			if (ptr == base)
			    return field;
			else
			    name = *--ptr;
		    
		    if (compare_names (name, context->ctx_symbol))
			if (ptr == base)
			    return field;
		    break;
		    }
	    break;
	}
    }

/* We didn't resolve all name segments.  Let somebody else worry about it. */

return NULL;
}

static FLD resolve_name (
    SYM		name,
    LLS		stack,
    CTX		*out_context)
{
/**************************************
 *
 *	r e s o l v e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Resolve a field node against a context stack.  Return both the
 *	field block (by value)  and the corresponding context block (by
 *	reference).  Return NULL if field can't be resolved.
 *
 **************************************/
CTX	context;
REL	relation;
FLD	field;
FRM	form;
FFL	ffield;

/* Look thru context stack looking for a context that will resolve
   all name segments.  If the context is a secondary context, require
   that the context name be given explicitly (used for special STORE
   context). */


for (; stack; stack = stack->lls_next)
    {
    *out_context = context = (CTX) stack->lls_object;
    switch (context->ctx_type)
	{
	case CTX_FORM:
	    if (context->ctx_primary)
		*out_context = context = context->ctx_primary;
	    form = context->ctx_form;
	    for (ffield = form->frm_fields; ffield; ffield = ffield->ffl_next)
		if (compare_symbols (name, ffield->ffl_symbol))
		    return (FLD) ffield;
	    break;

	case CTX_VARIABLE:
	     for (field = context->ctx_variable; field; field = field->fld_next)
		if (compare_symbols (name, field->fld_name) ||
		    compare_symbols (name, field->fld_query_name))
			return field;
	    break;

	case CTX_RELATION:
	    relation = context->ctx_relation;

	    for (field = relation->rel_fields; field; field = field->fld_next)
		if (compare_symbols (name, field->fld_name) ||
		    compare_symbols (name, field->fld_query_name))
		    return field;

	    break;
	}
    }

/* We didn't resolve all name segments.  Let somebody else worry about it. */

return NULL;
}

static void resolve_really (
    FLD		variable,
    SYN		field_node)
{
/**************************************
 *
 *	r e s o l v e _ r e a l l y
 *
 **************************************
 *
 * Functional description
 *	Resolve a field reference entirely.  
 *
 **************************************/
USHORT	offset;
BOOLEAN resolved, local;
NAM	fld_name, rel_name, db_name;
SYM	symbol;
FLD	field; 
REL	relation;
DBB	dbb;

db_name = rel_name = fld_name = NULL;
field = NULL;
resolved = local = FALSE;

/* For ease, break down the syntax block.
   It should contain at least one name; two names are a  potential ambiguity:
   check for a dbb (<db>.<glo_fld>), then for a rel (<rel>.<fld>). */

offset = field_node->syn_count;
fld_name = (NAM) field_node->syn_arg [--offset];

if (offset)
    {
    rel_name = (NAM) field_node->syn_arg [--offset];
    if (offset)
	db_name = (NAM) field_node->syn_arg [--offset];
    }       

if (field_node->syn_count == 1) 
    resolved = MET_declare (NULL_PTR, variable, fld_name);
else
    if (field_node->syn_count == 2)
    {   
    for (symbol = rel_name->nam_symbol; symbol; symbol = symbol->sym_homonym) 
	if (symbol->sym_type == SYM_database)
	    {
	    dbb = (DBB) symbol->sym_object;
	    resolved = MET_declare (dbb, variable, fld_name);
	    break;			/* should be only one db in homonym list */
	    }

    if (!resolved)
	{
	for (dbb = QLI_databases; dbb && !resolved; dbb = dbb->dbb_next)
	    for (symbol = rel_name->nam_symbol; symbol; symbol = symbol->sym_homonym)
		if (symbol->sym_type == SYM_relation && 
		    (relation = (REL) symbol->sym_object) &&
		    relation->rel_database == dbb)
		    {
		    if (!relation->rel_fields)
			MET_fields (relation);
		    for (field = relation->rel_fields; field; field = field->fld_next)
			if (resolved = local = compare_names (fld_name, field->fld_name)) 
			    break;
		    break;		/* should be only one rel in homonym list for each db */
		    } 
        }
    }
else
    {    
    relation = variable->fld_relation;
    if (!relation->rel_fields)
	MET_fields (relation);
    for (field = relation->rel_fields; field; field = field->fld_next)
	if (resolved = local = compare_names (fld_name, field->fld_name))
	    break;
    }      

if (!resolved)
    IBERROR (155); /* Msg155 field referenced in BASED ON can not be resolved against readied databases */

if (local)
   {
   variable->fld_dtype = field->fld_dtype;
   variable->fld_length = field->fld_length;
   variable->fld_scale = field->fld_scale;
   variable->fld_sub_type = field->fld_sub_type;
   variable->fld_sub_type_missing = field->fld_sub_type_missing;
   if (!variable->fld_edit_string)
	variable->fld_edit_string = field->fld_edit_string;
   if (!variable->fld_query_header)
        variable->fld_query_header = field->fld_query_header;
   if (!variable->fld_query_name) 
	variable->fld_query_name = field->fld_query_name;
   }
}
