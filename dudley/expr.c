/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		expr.c
 *	DESCRIPTION:	Expression parser
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

#include "../jrd/gds.h"
#include "../dudley/ddl.h"
#include "../dudley/parse.h"
#include "../jrd/acl.h"
#include "../jrd/intl.h"
#include "../dudley/ddl_proto.h"
#include "../dudley/expr_proto.h"
#include "../dudley/lex_proto.h"
#include "../dudley/parse_proto.h"

extern jmp_buf	parse_env;

static CON	make_numeric_constant (TEXT *, USHORT);
static NOD	parse_add (USHORT *, USHORT *);
static NOD	parse_and (USHORT *);
static NOD	parse_field (void);
static NOD	parse_from (USHORT *, USHORT *);
static NOD	parse_function (void);
static NOD	parse_gen_id (void);
static CON	parse_literal (void);
static int	parse_matching_paren (void);
static NOD	parse_multiply (USHORT *, USHORT *);
static NOD	parse_not (USHORT *);
static NOD	parse_primitive_value (USHORT *, USHORT *);
static CTX	parse_relation (void);
static NOD	parse_relational (USHORT *);
static NOD	parse_sort (void);
static NOD	parse_statistical (void);
static int	parse_terminating_parens (USHORT *, USHORT *);

static struct nod_types {
    enum kwwords	nod_t_keyword;
    enum nod_t		nod_t_node;
} statisticals [] = {
	KW_MAX, nod_max,
	KW_MIN, nod_min,
	KW_COUNT, nod_count,
	KW_AVERAGE, nod_average,
	KW_TOTAL, nod_total
};

static enum nod_t relationals [] = {
	nod_eql, nod_neq, nod_leq, nod_lss, nod_gtr, nod_geq, nod_containing,
	nod_starts, nod_missing, nod_between, nod_sleuth, nod_matches,
	nod_and, nod_or, nod_not, nod_any, nod_unique, (enum nod_t) 0
};

NOD EXPR_boolean (
    USHORT	*paren_count)
{
/**************************************
 *
 *	E X P R _ b o o l e a n
 *
 **************************************
 *
 * Functional description
 *	Parse a general boolean expression.  By precedence, handle an OR
 *	here.
 *
 **************************************/
NOD	expr, node;
USHORT	local_count;

if (!paren_count)
    {
    local_count = 0;
    paren_count = &local_count;
    }

expr = parse_and (paren_count);

if (!MATCH (KW_OR))
    {
    parse_terminating_parens (paren_count, &local_count);
    return expr;
    }

node = SYNTAX_NODE (nod_or, 2);
node->nod_arg [0] = expr;
node->nod_arg [1] = EXPR_boolean (paren_count);
parse_terminating_parens (paren_count, &local_count);

return node;
}

NOD EXPR_rse (
    USHORT	view_flag)
{
/**************************************
 *
 *	E X P R _ r s e
 *
 **************************************
 *
 * Functional description
 *	Parse a record selection expression.
 *
 **************************************/
NOD	node, boolean, field_name, a_boolean, temp;
LLS	stack, field_stack;
CTX	context;
SYM	field_sym;

node = SYNTAX_NODE (nod_rse, s_rse_count);
stack = NULL;
LEX_real();

if (MATCH (KW_ALL))
    LEX_real();

if (MATCH (KW_FIRST))
    node->nod_arg [s_rse_first] = EXPR_value (NULL_PTR, NULL_PTR);

while (TRUE)
    {
    LLS_PUSH (parse_relation(), &stack);
    if (MATCH (KW_OVER))
        {
	for (;;)
	    {
	    if (!stack->lls_next)
		{
                PARSE_error (234, NULL, NULL); /* msg 234: OVER can only be used in CROSS expressions */
                }
	    boolean = SYNTAX_NODE (nod_eql, 2);
	    field_sym = PARSE_symbol(tok_ident);
	    field_name = SYNTAX_NODE (nod_field_name, 2);
	    context = (CTX) stack->lls_object;
	    field_name->nod_arg[0] = (NOD) context->ctx_name;
	    field_name->nod_arg[1] = (NOD) field_sym;
	    boolean->nod_arg [0] = field_name;
	    field_name = SYNTAX_NODE (nod_over, 2);
	    field_name->nod_arg[0] = (NOD) context->ctx_name;
	    field_name->nod_arg[1] = (NOD) field_sym;
	    boolean->nod_arg [1] = field_name;
	    if (node->nod_arg [s_rse_boolean])
		{
		a_boolean = SYNTAX_NODE (nod_and, 2);
		a_boolean->nod_arg [0] = boolean;
		a_boolean->nod_arg [1] = node->nod_arg [s_rse_boolean];
		node->nod_arg [s_rse_boolean] = a_boolean;
		}
	    else
		node->nod_arg [s_rse_boolean] = boolean;
	    if (!MATCH (KW_COMMA))
		break;
	    }
        }
    if (!MATCH (KW_CROSS))
	break;
    }

node->nod_arg [s_rse_contexts] = (NOD) stack;

/* Pick up various other clauses */

if (MATCH (KW_WITH))
    if (boolean = EXPR_boolean (NULL_PTR))
       {
        if (node->nod_arg [s_rse_boolean])
            {
            a_boolean = SYNTAX_NODE (nod_and, 2);
            a_boolean->nod_arg [0] = boolean;
            a_boolean->nod_arg [1] = node->nod_arg [s_rse_boolean];
            node->nod_arg [s_rse_boolean] = a_boolean;
            }
        else
            node->nod_arg [s_rse_boolean] = boolean;
        }
if (MATCH (KW_SORTED))
    {
    if (view_flag)
        PARSE_error (319, NULL, NULL); /* msg 234: Unexpected sort clause */
    MATCH (KW_BY);
    node->nod_arg [s_rse_sort] = parse_sort();
    }

if (MATCH (KW_REDUCED))
    {
    MATCH (KW_TO);
    node->nod_arg [s_rse_reduced] = parse_sort();
    }

return node;
}

NOD EXPR_statement (void)
{
/**************************************
 *
 *	E X P R _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Parse a single trigger statement.
 *
 **************************************/
LLS	stack;
NOD	node, list;
int	number;

if (MATCH (KW_BEGIN))
    {
    stack = NULL;
    while (!MATCH (KW_END))
	LLS_PUSH (EXPR_statement(), &stack);
    node = PARSE_make_list (stack);
    }
else if (MATCH (KW_IF))
    {
    node = SYNTAX_NODE (nod_if, 3);
    node->nod_arg [s_if_boolean] = EXPR_boolean (NULL_PTR);
    MATCH (KW_THEN);
    node->nod_arg [s_if_true] = EXPR_statement();
    if (MATCH (KW_ELSE))
	node->nod_arg [s_if_false] = EXPR_statement();
    else
	node->nod_count = 2;
    }
else if (MATCH (KW_FOR))
    {
    node = SYNTAX_NODE (nod_for, 2);
    node->nod_arg [s_for_rse] = EXPR_rse (FALSE);
    stack = NULL;
    while (!MATCH (KW_END_FOR))
	LLS_PUSH (EXPR_statement(), &stack);
    node->nod_arg [s_for_action] = PARSE_make_list (stack);
    }
else if (MATCH (KW_STORE))
    {
    node = SYNTAX_NODE (nod_store, 2);
    node->nod_arg [s_store_rel] = (NOD) parse_relation();
    MATCH (KW_USING);
    stack = NULL;
    while (!MATCH (KW_END_STORE))
	LLS_PUSH (EXPR_statement(), &stack);
    node->nod_arg [s_store_action] = PARSE_make_list (stack);
    }
else if (MATCH (KW_ABORT))
    {
    node = SYNTAX_NODE (nod_abort, 1);
    node->nod_count = 0;
    number = PARSE_number();
    if (number > 255)
	PARSE_error (235, NULL, NULL); /* msg 235: abort code cannot exceed 255 */
    node->nod_arg [0] = (NOD) (SLONG) number;
    }
else if (MATCH (KW_ERASE))
    {
    node = SYNTAX_NODE (nod_erase, 1);
    node->nod_count = 0;
    node->nod_arg [0] = (NOD) PARSE_symbol (tok_ident);
    }
else if (MATCH (KW_MODIFY))
    {
    MATCH (KW_USING);
    node = SYNTAX_NODE (nod_modify, 3);
    node->nod_count = 0;
    node->nod_arg [s_mod_old_ctx] = (NOD) PARSE_symbol (tok_ident);
    MATCH (KW_USING);
    stack = NULL;
    while (!MATCH (KW_END_MODIFY))
	LLS_PUSH (EXPR_statement(), &stack);
    node->nod_arg [s_mod_action] = PARSE_make_list (stack);
    }
else if (MATCH (KW_POST))
    {
    node = SYNTAX_NODE (nod_post, 1);
    node->nod_count = 1;
    node->nod_arg [0] = EXPR_value (NULL_PTR, NULL_PTR);
    }
else
    {
    node = SYNTAX_NODE (nod_assignment, 2);
    node->nod_arg [1] = parse_field();
    if (!MATCH (KW_EQUALS))
        PARSE_error (236, DDL_token.tok_string, NULL); /* msg 236: expected =, encountered \"%s\"*/
    node->nod_arg [0] = EXPR_value (NULL_PTR, NULL_PTR);
    }

MATCH (KW_SEMI);

return node;
}

NOD EXPR_value (
    USHORT	*paren_count,
    USHORT	*bool_flag)
{
/**************************************
 *
 *	E X P R _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Parse a general value expression.  In practice, this means parse the
 *	lowest precedence operator CONCATENATE.
 *
 **************************************/
NOD		node, arg;
enum nod_t	operator;
USHORT		local_count, local_flag;

if (!paren_count)
    {
    local_count = 0;
    paren_count = &local_count;
    }
if (!bool_flag)
    {
    local_flag = FALSE;
    bool_flag = &local_flag;
    }

node = parse_add (paren_count, bool_flag);

while (TRUE)
    {
    if (!MATCH (KW_BAR))
	{
	parse_terminating_parens (paren_count, &local_count);
	return node;
	}
    arg = node;
    node = SYNTAX_NODE (nod_concatenate, 2);
    node->nod_arg [0] = arg;
    node->nod_arg [1] = parse_add (paren_count, bool_flag);
    }
}

static CON make_numeric_constant (
    TEXT	*string,
    USHORT	length)
{
/**************************************
 *
 *	m a k e _ n u m e r i c _ c o n s t a n t
 *
 **************************************
 *
 * Functional description
 *	Build a constant block for a numeric
 *	constant.  Numeric constants are normally
 *	stored as long words, but especially large
 *	ones become text.  They ought to become
 *      double precision, one would think, but they'd
 *      have to be VAX style double precision which is
 *      more pain than gain.
 *
 **************************************/
CON	constant;
TEXT	*p, *q, c;
USHORT   scale, l, fraction;
SLONG	*number;

p = string;
l = length;

constant = (CON) DDL_alloc (CON_LEN + sizeof (SLONG));
constant->con_desc.dsc_dtype = dtype_long;
constant->con_desc.dsc_length = sizeof (SLONG);
number = (SLONG*) constant->con_data;
constant->con_desc.dsc_address = (UCHAR*) number;
scale = 0;

do {
    if ((c = *p++) == '.')
	scale = 1;
    else
	{
	/* The right side of the following comparison had originally been:

	       ((1 << 31) -1 ) / 10

	   For some reason, this doesn't work on 64-bit platforms.  Its
	   replacement does. */

        if (*number > ((SLONG) ~(1 << 31)) / 10)
	    break;
	*number *= 10;
	*number += c - '0';
	constant->con_desc.dsc_scale -= scale;
	}
    } while (--l);

/* if there was an overflow (indicated by the fact that we have
 * some length left over), switch back to text.  Do check that
 * the thing is a plausible number.  Also leave a space so we
 * can negate it later if necessary.
 */

if (l)
    {
    length++;
    constant = (CON) DDL_alloc (CON_LEN + length);
    constant->con_desc.dsc_dtype = dtype_text;
    constant->con_desc.dsc_ttype = ttype_ascii;
    constant->con_desc.dsc_length = length;
    constant->con_desc.dsc_address = constant->con_data;
    q = (TEXT*) constant->con_desc.dsc_address;
    p = string;
    *q++ = ' ';

    fraction = 0;
    for (l = 1; l < length; p++, l++)
	if (*p >= '0' && *p <= '9')
	    *q++ = *p;
	else if (*p == '.')
	    {
            *q++ = *p;
	    if (fraction)
		DDL_err (237, NULL, NULL, NULL, NULL, NULL); /* msg 237: too many decimal points */
	    else
		fraction = 1;
	    }
	else 
	    DDL_err (238, NULL, NULL, NULL, NULL, NULL); /* msg 238: unrecognized character in numeric string */
    }
return constant;
}    

static NOD parse_add (
    USHORT	*paren_count,
    USHORT	*bool_flag)
{
/**************************************
 *
 *	p a r s e _ a d d
 *
 **************************************
 *
 * Functional description
 *	Parse a general value expression.  In practice, this means parse the
 *	lowest precedence operators, ADD and SUBTRACT.
 *
 **************************************/
NOD		node, arg;
enum nod_t	operator;

node = parse_multiply (paren_count, bool_flag);

while (TRUE)
    {
    if (MATCH (KW_PLUS))
	operator = nod_add;
    else if (MATCH (KW_MINUS))
	operator = nod_subtract;
    else
	return node;
    arg = node;
    node = SYNTAX_NODE (operator, 2);
    node->nod_arg [0] = arg;
    node->nod_arg [1] = parse_multiply (paren_count, bool_flag);
    }
}

static NOD parse_and (
    USHORT	*paren_count)
{
/**************************************
 *
 *	p a r s e _ a n d
 *
 **************************************
 *
 * Functional description
 *	Parse an AND expression.
 *
 **************************************/
NOD	expr, node;

expr = parse_not (paren_count);

if (!MATCH (KW_AND))
    return expr;

node = SYNTAX_NODE (nod_and, 2);
node->nod_arg [0] = expr;
node->nod_arg [1] = parse_and (paren_count);

return node;
}

static NOD parse_field (void)
{
/**************************************
 *
 *	p a r s e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Parse a qualified field name.
 *
 **************************************/
NOD	field, array;
LLS	stack;

stack = NULL;

while (TRUE)
    {
    LEX_real();
    LLS_PUSH (PARSE_symbol(tok_ident), &stack);
    if (!MATCH (KW_DOT))
	break;
    }

field = PARSE_make_list (stack);
field->nod_type = nod_field_name;

if (!MATCH (KW_L_BRCKET))
    return field;

/* Parse an array reference */

stack = NULL;
for (;;)
    {
    LLS_PUSH (EXPR_value (NULL_PTR, NULL_PTR), &stack);
    if (MATCH (KW_R_BRCKET))
	break;
    if (!MATCH (KW_COMMA))
	PARSE_error (317, DDL_token.tok_string, NULL); /* msg 317, expected comma or right bracket, encountered \"%s\" */
    }

array = SYNTAX_NODE (nod_index, 2);
array->nod_arg [0] = field;
array->nod_arg [1] = PARSE_make_list (stack);

return array;
}

static NOD parse_from (
    USHORT	*paren_count,
    USHORT	*bool_flag)
{
/**************************************
 *
 *	p a r s e _ f r o m
 *
 **************************************
 *
 * Functional description
 *	Parse either an explicit or implicit FIRST ... FROM statement.
 *
 **************************************/
NOD	node, value;

if (MATCH (KW_FIRST))
    {
    value = parse_primitive_value (NULL_PTR, NULL_PTR);
    if (!MATCH (KW_FROM))
        PARSE_error (239, DDL_token.tok_string, NULL);
	    /* msg 239: expected FROM rse clause, encountered \"%s\" */
    }
else
    {
    value = parse_primitive_value (paren_count, bool_flag);
    if (!MATCH (KW_FROM))
	return value;
    }

node = SYNTAX_NODE (nod_from, s_stt_count);
node->nod_arg [s_stt_value] = value;
node->nod_arg [s_stt_rse] = EXPR_rse (FALSE);

if (MATCH (KW_ELSE))
    node->nod_arg [s_stt_default] = EXPR_value (NULL_PTR, NULL_PTR);

return node;
}

static NOD parse_function (void)
{
/**************************************
 *
 *	p a r s e _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse a function reference.  If not a function, return NULL.
 *
 **************************************/
SYM	symbol;
NOD	node;
LLS	stack;

/* Look for symbol of type function.  If we don't find it, give up */

for (symbol = DDL_token.tok_symbol; symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_function)
	break;

if (!symbol)
    return NULL;

node = SYNTAX_NODE (nod_function, 3);
node->nod_count = 1;
node->nod_arg [1] = (NOD) PARSE_symbol (tok_ident);
node->nod_arg [2] = (NOD) symbol->sym_object;
stack = NULL;

if (MATCH (KW_LEFT_PAREN) && !MATCH (KW_RIGHT_PAREN))
    for (;;)
	{
	LLS_PUSH (EXPR_value (NULL_PTR, NULL_PTR), &stack);
	if (MATCH (KW_RIGHT_PAREN))
	    break;
	if (!MATCH (KW_COMMA))
	    PARSE_error (240, DDL_token.tok_string, NULL);
	    /* msg 240: expected comma or right parenthesis, encountered \"%s\" */
	}

node->nod_arg [0] = PARSE_make_list (stack);

return node;
}

static NOD parse_gen_id (void)
{
/**************************************
 *
 *	p a r s e _ g e n _ i d
 *
 **************************************
 *
 * Functional description
 *	Parse GEN_ID expression.  Syntax is:
 *
 *		GEN_ID (<relation_name>, <value>)
 *
 **************************************/
NOD	node;

LEX_token();

if (!MATCH (KW_LEFT_PAREN))
    PARSE_error (241, DDL_token.tok_string, NULL); /* msg 241: expected left parenthesis, encountered \"%s\" */

node = SYNTAX_NODE (nod_gen_id, 2);
node->nod_count = 1;
node->nod_arg [1] = (NOD) PARSE_symbol (tok_ident);
MATCH (KW_COMMA);
node->nod_arg [0] = EXPR_value (NULL_PTR, NULL_PTR);
parse_matching_paren();

return node;
}

static CON parse_literal (void)
{
/**************************************
 *
 *	p a r s e _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Parse a literal expression.
 *
 **************************************/
CON	constant;
USHORT	l, scale;
TEXT	*p, *q, c;
SLONG	*number;

q = DDL_token.tok_string;
l = DDL_token.tok_length;

if (DDL_token.tok_type == tok_quoted)
    {
    q++;
    l -= 2;
    constant = (CON) DDL_alloc (CON_LEN + l);
    constant->con_desc.dsc_dtype = dtype_text;
    constant->con_desc.dsc_ttype = ttype_ascii;
    p = (TEXT *)constant->con_data;
    constant->con_desc.dsc_address = (UCHAR*) p;
    if (constant->con_desc.dsc_length = l)
	do *p++ = *q++; while (--l);
    }
else if (DDL_token.tok_type == tok_number)
    {
    constant = make_numeric_constant (DDL_token.tok_string, 
				       DDL_token.tok_length);
    }
else
    PARSE_error (242, DDL_token.tok_string, NULL); /* msg 242: expected value expression, encountered \"%s\" */

LEX_token();

return constant;
}

static parse_matching_paren (void)
{
/**************************************
 *
 *	p a r s e _ m a t c h i n g _ p a r e n
 *
 **************************************
 *
 * Functional description
 *	Check for balancing parenthesis.  If missing, barf.
 *
 **************************************/

if (!MATCH (KW_RIGHT_PAREN))
    PARSE_error (243, DDL_token.tok_string, NULL); /* msg 243: expected right parenthesis, encountered \"%s\" */
}

static NOD parse_multiply (
    USHORT	*paren_count,
    USHORT	*bool_flag)
{
/**************************************
 *
 *	p a r s e _ m u l t i p l y
 *
 **************************************
 *
 * Functional description
 *	Parse the operators * and /.
 *
 **************************************/
NOD		node, arg;
enum nod_t	operator;

node = parse_from (paren_count, bool_flag);

while (TRUE)
    {
    if (MATCH (KW_ASTERISK))
	operator = nod_multiply;
    else if (MATCH (KW_SLASH))
	operator = nod_divide;
    else
	return node;
    arg = node;
    node = SYNTAX_NODE (operator, 2);
    node->nod_arg [0] = arg;
    node->nod_arg [1] = parse_from (paren_count, bool_flag);
    }
}

static NOD parse_not (
    USHORT	*paren_count)
{
/**************************************
 *
 *	p a r s e _ n o t
 *
 **************************************
 *
 * Functional description
 *	Parse a prefix NOT expression.
 *
 **************************************/
NOD	node;

LEX_real();

if (!MATCH (KW_NOT))
    return parse_relational (paren_count);

node = SYNTAX_NODE (nod_not, 1);
node->nod_arg [0] = parse_not (paren_count);

return node;
}

static NOD parse_primitive_value (
    USHORT	*paren_count,
    USHORT	*bool_flag)
{
/**************************************
 *
 *	p a r s e _ p r i m i t i v e _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Pick up a value expression.  This may be either a field reference
 *	or a constant value.
 *
 **************************************/
NOD	node, sub;
CON	constant;
SYM	symbol;
TEXT	*p;
USHORT	local_count;

if (!paren_count)
    {
    local_count = 0;
    paren_count = &local_count;
    }

switch (PARSE_keyword())
    {
    case KW_LEFT_PAREN:
	LEX_token();
	(*paren_count)++;
	if (bool_flag && *bool_flag)
	    node = EXPR_boolean (paren_count);
	else
	    node = EXPR_value (paren_count, bool_flag);
	parse_matching_paren();
	(*paren_count)--;
	break;

    case KW_MINUS:
	LEX_token();
	sub = parse_primitive_value (paren_count, NULL_PTR);
	if (sub->nod_type == nod_literal)
	    {
	    constant = (CON) sub->nod_arg [0];
	    p = (TEXT*) constant->con_desc.dsc_address;
	    switch (constant->con_desc.dsc_dtype)
		{
		case dtype_long:
		    *(SLONG*) p = - *(SLONG*) p;
		    return sub;
		case dtype_text:
		    *p = '-';
		    return sub;			
		}
	    }
	node = SYNTAX_NODE (nod_negate, 1);
	node->nod_arg [0] = sub;
	break;

    case KW_COUNT:
    case KW_MAX:
    case KW_MIN:
    case KW_AVERAGE:
    case KW_TOTAL:
	node = parse_statistical();
	break;

    case KW_NULL:
	LEX_token();
	node = SYNTAX_NODE (nod_null, 0);
	break;
     
    case KW_USER_NAME:
	LEX_token();
	node = SYNTAX_NODE (nod_user_name, 0);
	break;
     
    case KW_GEN_ID:
	node = parse_gen_id();
	break;
            
    case KW_UPPERCASE:
	LEX_token();
	sub = parse_primitive_value (NULL_PTR, NULL_PTR);
	node = SYNTAX_NODE (nod_uppercase, 1);
	node->nod_arg [0] = sub;
	break;

    default:
	if (DDL_token.tok_type == tok_ident)
	    {
	    if (!(node = parse_function()))
		node = parse_field();
	    break;
	    }
	node = SYNTAX_NODE (nod_literal, 1);
	node->nod_arg [0] = (NOD) parse_literal();
    }
    
return node;
}

static CTX parse_relation (void)
{
/**************************************
 *
 *	p a r s e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse a relation expression.  Syntax is:
 *
 *	[ <context_variable> IN ] <relation>
 *
 **************************************/
SYM	symbol;
CTX	context;
TEXT	s [128];

context = (CTX) DDL_alloc (CTX_LEN);
context->ctx_name = symbol = PARSE_symbol (tok_ident);
symbol->sym_type = SYM_context;
symbol->sym_object = context;

if (!MATCH (KW_IN))
    PARSE_error (244, DDL_token.tok_string, NULL); /* msg 244: expected IN, encountered \"%s\" */

context->ctx_relation = PARSE_relation();

return context;
}

static NOD parse_relational (
    USHORT	*paren_count)
{
/**************************************
 *
 *	p a r s e _ r e l a t i o n a l
 *
 **************************************
 *
 * Functional description
 *	Parse a relational expression.
 *
 **************************************/
NOD		node, expr1, expr2, or_node;
LLS		stack;
USHORT		count, negation;
enum nod_t	operator, *rel_ops;
USHORT		local_flag;

local_flag = TRUE;

if (MATCH (KW_ANY))
    {
    node = SYNTAX_NODE (nod_any, 1);
    node->nod_arg [0] = EXPR_rse (FALSE);
    return node;
    }

if (MATCH (KW_UNIQUE))
    {
    node = SYNTAX_NODE (nod_unique, 1);
    node->nod_arg [0] = EXPR_rse (FALSE);
    return node;
    }

expr1 = EXPR_value (paren_count, &local_flag);
if (KEYWORD (KW_RIGHT_PAREN))
    return expr1;

negation = FALSE;
node = NULL;
LEX_real();

if (MATCH (KW_NOT))
    {
    negation = TRUE;
    LEX_real();
    }

switch (PARSE_keyword())
    {
    case KW_EQUALS:
    case KW_EQ:
	operator = (negation) ? nod_neq : nod_eql;
	negation = FALSE;
	break;

    case KW_NE:
	operator = (negation) ? nod_eql : nod_neq;
	negation = FALSE;
	break;

    case KW_GT:
	operator = (negation) ? nod_leq : nod_gtr;
	negation = FALSE;
	break;

    case KW_GE:
	operator = (negation) ? nod_lss : nod_geq;
	negation = FALSE;
	break;

    case KW_LE:
	operator = (negation) ? nod_gtr : nod_leq;
	negation = FALSE;
	break;

    case KW_LT:
	operator = (negation) ? nod_geq : nod_lss;
	negation = FALSE;
	break;
    
    case KW_CONTAINING:
	operator = nod_containing;
	break;

    case KW_MATCHES:
        LEX_token();
        expr2 = EXPR_value (NULL_PTR, NULL_PTR);
        if (MATCH (KW_USING))
            {
            operator = nod_sleuth;
            node = SYNTAX_NODE (operator, 3);
            node->nod_arg [2] = EXPR_value (NULL_PTR, NULL_PTR);
            }
        else
            {
            operator = nod_matches;
            node = SYNTAX_NODE (operator, 2);
            }
        node->nod_arg [0] = expr1;
        node->nod_arg [1] = expr2;
	break;

    case KW_STARTS:
	LEX_token();
	MATCH (KW_WITH);
	node = SYNTAX_NODE (nod_starts, 2);
	node->nod_arg [0] = expr1;
	node->nod_arg [1] = EXPR_value (NULL_PTR, NULL_PTR);
	break;

    case KW_MISSING:
	LEX_token();
	node = SYNTAX_NODE (nod_missing, 1);
	node->nod_arg [0] = expr1;
	break;

    case KW_BETWEEN:
	LEX_token();
	node = SYNTAX_NODE (nod_between, 3);
	node->nod_arg [0] = expr1;
	node->nod_arg [1] = EXPR_value (NULL_PTR, NULL_PTR);
	MATCH (KW_AND);
	node->nod_arg [2] = EXPR_value (NULL_PTR, NULL_PTR);
	break;
    
    default:
	for (rel_ops = relationals; *rel_ops != (enum nod_t) 0; rel_ops++)
	    if (expr1->nod_type == *rel_ops)
		return expr1;
	PARSE_error (245, DDL_token.tok_string, NULL); /* msg 245: expected relational operator, encountered \"%s\" */
    }

/* If we haven't already built a node, it must be an ordinary binary operator.
   Build it. */

if (!node)
    {
    LEX_token();
    node = SYNTAX_NODE (operator, 2);
    node->nod_arg [0] = expr1;
    node->nod_arg [1] = EXPR_value (paren_count, &local_flag);
    }

/* If a negation remains to be handled, zap the node under a NOT. */

if (negation)
    {
    expr1 = SYNTAX_NODE (nod_not, 1);
    expr1->nod_arg [0] = node;
    node = expr1;
    }

/*  If the node isn't an equality, we've done.  Since equalities can be structured
    as implicit ORs, build them here. */

if (operator != nod_eql)
    return node;

/* We have an equality operation, which can take a number of values.  Generate
   implicit ORs */

while (MATCH (KW_COMMA))
    {
    MATCH (KW_OR);
    or_node = SYNTAX_NODE (nod_or, 2);
    or_node->nod_arg [0] = node;
    or_node->nod_arg [1] = node = SYNTAX_NODE (nod_eql, 2);
    node->nod_arg [0] = expr1;
    node->nod_arg [1] = EXPR_value (paren_count, &local_flag);
    node = or_node;
    }

return node;
}

static NOD parse_sort (void)
{
/**************************************
 *
 *	p a r s e _ s o r t
 *
 **************************************
 *
 * Functional description
 *	Parse a sort list.
 *
 **************************************/
NOD	node, *ptr;
LLS	stack;
SSHORT	direction;

direction = 0;
stack = NULL;

while (TRUE)
    {
    LEX_real();
    if (MATCH (KW_ASCENDING))
	direction = 0;
    else if (MATCH (KW_DESCENDING))
	direction = 1;
    LLS_PUSH (EXPR_value (NULL_PTR, NULL_PTR), &stack);
    LLS_PUSH (direction, &stack);
    if (!MATCH (KW_COMMA))
	break;
    }

return PARSE_make_list (stack);
}

static NOD parse_statistical (void)
{
/**************************************
 *
 *	p a r s e _ s t a t i s t i c a l
 *
 **************************************
 *
 * Functional description
 *	Parse statistical expression.
 *
 **************************************/
struct nod_types *types;
NOD		node;
enum kwwords	keyword;

keyword = PARSE_keyword();
LEX_token();

for (types = statisticals;; types++)
    if (types->nod_t_keyword == keyword)
	break;

node = SYNTAX_NODE (types->nod_t_node, s_stt_count);

if (node->nod_type != nod_count)
    node->nod_arg [s_stt_value] = EXPR_value (NULL_PTR, NULL_PTR);

if (!MATCH (KW_OF))
    PARSE_error (246, DDL_token.tok_string, NULL); /* msg 246: expected OF, encountered \"%s\" */ 

node->nod_arg [s_stt_rse] = EXPR_rse (FALSE);

return node;
}

static int parse_terminating_parens (
    USHORT	*paren_count,
    USHORT	*local_count)
{
/**************************************
 *
 *	p a r s e _ t e r m i n a t i n g _ p a r e n s
 *
 **************************************
 *
 * Functional description
 *	Check for balancing parenthesis.  If missing, barf.
 *
 **************************************/

if (*paren_count && paren_count == local_count)
    do parse_matching_paren(); while (--(*paren_count));
}
