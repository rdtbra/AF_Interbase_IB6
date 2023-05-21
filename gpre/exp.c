/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		exp.c
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

#include <stdlib.h>
#include <string.h>
#include "../jrd/gds.h"
#include "../jrd/common.h"
#include "../gpre/gpre.h"
#include "../gpre/parse.h"
#include "../gpre/form.h"
#include "../jrd/intl.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/exp_proto.h"
#include "../gpre/form_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/hsh_proto.h"
#include "../gpre/met_proto.h"
#include "../gpre/msc_proto.h"
#include "../gpre/par_proto.h"
#include "../gpre/sqe_proto.h"

#define ZERO_BASED	0
#define ONE_BASED	1

#define MAX_USHORT	((USHORT)(0xFFFF))
#define MAX_SSHORT	((SSHORT)(0x7FFF))
#define MIN_SSHORT	((SSHORT)(0x8000))

static BOOLEAN	check_relation (void);
static NOD	lookup_field (CTX);
static NOD	make_and (NOD, NOD);
static NOD	make_list (LLS);
static NOD	normalize_index (DIM, NOD, USHORT);
static NOD	par_and (REQ);
static NOD	par_array (REQ, FLD, SSHORT, SSHORT);
static NOD	par_boolean (REQ);
static NOD	par_field (REQ);
static NOD	par_multiply (REQ, FLD);
static NOD	par_native_value (REQ, FLD);
static NOD	par_not (REQ);
static NOD	par_over (CTX);
static NOD	par_primitive_value (REQ, FLD);
static NOD	par_relational (REQ);
static NOD	par_udf (REQ, USHORT, FLD);
static NOD	par_value (REQ, FLD);

static FLD	count_field, subscript_field;

static struct rel_ops {
    enum nod_t		rel_op;
    enum kwwords	rel_kw;
    SSHORT		rel_args;
    } relops [] =
    {nod_eq, KW_EQ, 2,
     nod_eq, KW_EQUALS, 2,
     nod_ne, KW_NE, 2,
     nod_gt, KW_GT, 2,
     nod_ge, KW_GE, 2,
     nod_le, KW_LE, 2,
     nod_lt, KW_LT, 2,
     nod_containing, KW_CONTAINING, 2,
     nod_matches, KW_MATCHES, 2,
     nod_like, KW_LIKE, 2,
     nod_starting, KW_STARTING, 2,
     nod_missing, KW_MISSING, 1,
     nod_between, KW_BETWEEN, 3,
     nod_any, KW_none
};

static struct dtypes {
    enum kwwords	dtype_keyword;
    USHORT		dtype_dtype;
} data_types [] = {
	KW_CHAR, dtype_text,
	KW_VARYING, dtype_varying,
	KW_STRING, dtype_cstring,
	KW_SHORT, dtype_short,
	KW_LONG, dtype_long,
	KW_QUAD, dtype_quad,
	KW_FLOAT, dtype_real,
	KW_DOUBLE, dtype_double,
	KW_DATE, dtype_date,
	KW_none, 0
};

NOD EXP_array (
    REQ		request,
    FLD		field,
    SSHORT	subscript_flag,
    SSHORT	sql_flag)
{
/**************************************
 *
 *	E X P _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	Parse array subscript.
 *
 **************************************/

return par_array (request, field, subscript_flag, sql_flag);
}

FLD EXP_cast (
    FLD		field)
{
/**************************************
 *
 *	E X P _ c a s t
 *
 **************************************
 *
 * Functional description
 *	Parse a datatype cast (sans leading period).
 *
 **************************************/
struct dtypes	*dtype;
FLD		cast;

for (dtype = data_types;; dtype++)
    if ((int) dtype->dtype_keyword == (int) KW_none)
	return NULL;
    else if (MATCH (dtype->dtype_keyword))
	break;

cast = (FLD) ALLOC (FLD_LEN);
cast->fld_symbol = field->fld_symbol;

switch (cast->fld_dtype = dtype->dtype_dtype)
    {
    case dtype_varying:
	cast->fld_length++;

    case dtype_cstring:
	cast->fld_length++;

    case dtype_text:
	if (sw_cstring && !(cast->fld_dtype == dtype_cstring)) 
	    {
	    cast->fld_length++;
	    cast->fld_dtype = dtype_cstring;
	    }
	if (!MATCH (KW_L_BRCKET) && !MATCH (KW_LT))
	    SYNTAX_ERROR ("left bracket or <");
	cast->fld_length += EXP_pos_USHORT_ordinal (TRUE);
	if (!MATCH (KW_R_BRCKET) && !MATCH (KW_GT))
	    SYNTAX_ERROR ("right bracket or >");
	break;
    
    case dtype_quad:
	cast->fld_length = 4;
	break;

 /** Begin date/time/timestamp **/   
    case dtype_sql_date:
	cast->fld_length = sizeof (ISC_DATE);
	break;

    case dtype_sql_time:
	cast->fld_length = sizeof (ISC_TIME);
	break;

    case dtype_timestamp:
	cast->fld_length = sizeof (ISC_TIMESTAMP);
	break;
 /** End date/time/timestamp **/   

    case dtype_int64:
	cast->fld_length = sizeof (ISC_INT64);
	break;

    case dtype_long:
	cast->fld_length = sizeof (SLONG);
	if (MATCH (KW_SCALE))
	    cast->fld_scale = EXP_SSHORT_ordinal (TRUE);
	break;
    
    case dtype_short:
	cast->fld_length = sizeof (SSHORT);
	if (MATCH (KW_SCALE))
	    cast->fld_scale = EXP_SSHORT_ordinal (TRUE);
	break;
    
    case dtype_float:
	cast->fld_length = 4;
	break;
    
    case dtype_double:
	cast->fld_length = 8;
	break;

    }

return cast;
}

CTX EXP_context (
    REQ		request,
    SYM		initial_symbol)
{
/**************************************
 *
 *	E X P _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Parse a clause of the form "<context> IN <relation>".
 *	If the error flag is true, and the parse fails, quietly
 *	return NULL.  Otherwise issue error messages where appropriate,
 *	and return a CONTEXT block as value.
 *
 **************************************/
CTX	context;
SYM	symbol;
REL	relation;

/* Use the token (context name) to make up a symbol
   block.  Then check for the keyword IN.  If it's
   missing, either complain or punt, depending on the
   error flag.  In either case, be sure to get rid of
   the symbol.  If things look kosher, continue. */

if (!(symbol = initial_symbol))
    {
    symbol = PAR_symbol(SYM_context);
    if (!MATCH (KW_IN))
	{
	FREE (symbol);
	SYNTAX_ERROR ("IN");
	}
    }

symbol->sym_type = SYM_context;

relation = EXP_relation();
context = MAKE_CONTEXT (request);
context->ctx_symbol = symbol;
context->ctx_relation = relation;
symbol->sym_object = context;

return context;
}

FLD EXP_field (
    CTX	*rcontext)
{
/**************************************
 *
 *	E X P _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Parse a qualified field clause.  If recognized,
 *	return both the field block (as value) and the
 *	context block (by reference).
 *
 **************************************/
SYM	symbol;
CTX	context;
FLD	field;
REL	relation;
TEXT	s [128];

for (symbol = token.tok_symbol; symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_context)
	break;

if (!symbol)
    SYNTAX_ERROR ("context variable");
    
context = symbol->sym_object;
relation = context->ctx_relation;
ADVANCE_TOKEN;

if (!MATCH (KW_DOT))
    SYNTAX_ERROR ("dot after context variable");

SQL_resolve_identifier ("<Field Name>", s);
if (!(field = MET_field (relation, token.tok_string)))
    {
    sprintf (s, "field \"%s\" is not defined in relation %s",
	token.tok_string, relation->rel_symbol->sym_string);
    PAR_error (s);
    }

ADVANCE_TOKEN;
*rcontext = context;

return field;
}

FLD EXP_form_field (
    CTX		*rcontext)
{
/**************************************
 *
 *	E X P _ f o r m _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Parse a qualified form field clause.  If recognized,
 *	return both the field block (as value) and the
 *	context block (by reference).
 *
 **************************************/
SYM	symbol;
CTX	context;
FLD	field, child;
REF	reference, parent;
REQ	request;
FORM	form;
TEXT	s [128];

if (!(symbol = token.tok_symbol) ||
    symbol->sym_type != SYM_form_map)
    SYNTAX_ERROR ("context variable");
    
*rcontext = context = symbol->sym_object;
request = context->ctx_request;
form = request->req_form;
ADVANCE_TOKEN;

if (!MATCH (KW_DOT))
    SYNTAX_ERROR ("dot after context variable");

if (MATCH (KW_TERMINATOR))
    if (reference = request->req_eof)
	return reference->ref_field;
    else
	{
	field = MET_make_field ("TERMINATOR", dtype_short, sizeof (SSHORT), FALSE);
	reference = EXP_post_field (field, context, FALSE);
	reference->ref_flags |= REF_pseudo;
	request->req_eof = reference;
	return field;
	}

if (MATCH (KW_TERMINATING_FIELD))
    if (reference = request->req_term_field)
	return reference->ref_field;
    else
	{
	if (sw_cstring)
	    field = MET_make_field ("TERMINATING_FIELD", dtype_cstring, 32, FALSE);
	else
	    field = MET_make_field ("TERMINATING_FIELD", dtype_text, 31, FALSE);
	reference = EXP_post_field (field, context, FALSE);
	reference->ref_flags |= REF_pseudo;
	request->req_term_field = reference;
	return field;
	}

if (!(field = FORM_lookup_field (form, form->form_object, token.tok_string)))
    {
    sprintf (s, "field \"%s\" is not defined in form %s",
	token.tok_string, form->form_name->sym_string);
    PAR_error (s);
    }

ADVANCE_TOKEN;

/* Unless field is further qualified, we're done */

if (!MATCH (KW_DOT))
    return field;

/* Check for a sub-field reference */

parent = NULL;
if (field->fld_prototype &&
    (child = FORM_lookup_field (request->req_form, field->fld_prototype, token.tok_string)))
    {
    ADVANCE_TOKEN;
    parent = MAKE_REFERENCE (NULL_PTR);
    parent->ref_field = field;
    field = child;
    }

reference = EXP_post_field (field, context, FALSE);
if ((reference->ref_friend = parent) && !(MATCH (KW_DOT)))
    return reference->ref_field;

if (!MATCH (KW_STATE))
    SYNTAX_ERROR ("STATE");

reference = reference->ref_null;
return reference->ref_field;
}

void EXP_left_paren (
    TEXT	*string)
{
/**************************************
 *
 *	E X P _ l e f t _ p a r e n
 *
 **************************************
 *
 * Functional description
 *	Eat a left parenthesis, complain if not there.
 *
 **************************************/

if (!MATCH (KW_LEFT_PAREN))
    SYNTAX_ERROR ((string) ? string : "left parenthesis");
}

NOD EXP_literal (void)
{
/**************************************
 *
 *	E X P _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Parse a native literal constant value.
 *
 **************************************/
NOD	node;
REF	reference;
TEXT	*string;
SYM	symbol;

switch (sw_sql_dialect)
    {
    case 1:
        if (!(token.tok_type == tok_number || QUOTED(token.tok_type) ))
            return NULL;
	break;
    default:
        if (!(token.tok_type == tok_number || SINGLE_QUOTED(token.tok_type) ))
            return NULL;
    }

reference = (REF) ALLOC (REF_LEN);
node = MSC_unary (nod_literal, reference);
if (QUOTED(token.tok_type))
    {
    reference->ref_value = string = (TEXT*) ALLOC (token.tok_length+3);
    strcat (string, "\'");
    COPY (token.tok_string, token.tok_length, string+1);
    strcat ((string+token.tok_length+1), "\'");
    token.tok_length +=2;
    }
else
    {
    reference->ref_value = string = (TEXT*) ALLOC (token.tok_length+1);
    COPY (token.tok_string, token.tok_length, string);
    }

/** Begin date/time/timestamp **/
switch (token.tok_keyword)
    {
    case KW_DATE:
	reference->ref_flags |= REF_sql_date;
	break;

    case KW_TIME:
	reference->ref_flags |= REF_sql_time;
	break;

    case KW_TIMESTAMP:
	reference->ref_flags |= REF_timestamp;
	break;
    /** Do not put a default here **/
    }
/** End date/time/timestamp **/
if (((SINGLE_QUOTED(token.tok_type)) && (token.tok_charset)) ||
    ((QUOTED(token.tok_type) && (sw_sql_dialect == 1)) && (token.tok_charset)))
    {
    reference->ref_flags |= REF_ttype;
    symbol = token.tok_charset;
    reference->ref_ttype = ((INTLSYM) (symbol->sym_object))->intlsym_ttype;
    }
else if (sw_language == lan_internal)
    {
    /* literals referenced in an Internal request are always correct charset */
    reference->ref_flags |= REF_ttype;
    reference->ref_ttype = ttype_metadata;
    }
ADVANCE_TOKEN;
return node;
}

SINT64 EXP_SINT64_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ S I N T 6 4 _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *	Restrict to LONG range.
 *
 **************************************/
USHORT	negate;
SINT64	n;
UCHAR	buffer [64];
UCHAR   format[8];

#ifdef SOLARIS
#ifdef SOLARIS26
sprintf (format, "%c%sd", '%', QUADFORMAT);
#else
sprintf (format, "%s", "%lld");
#endif
#else
sprintf (format, "%c%sd", '%', QUADFORMAT);
#endif

negate = (MATCH (KW_MINUS)) ? TRUE : FALSE;

if (token.tok_type != tok_number)
    SYNTAX_ERROR ("<number>");

sscanf (token.tok_string,format, &n);
sprintf (buffer, format, n);
if (strcmp (buffer, token.tok_string) != 0)
   PAR_error ("Numeric value out of range");

if (advance_flag)
    ADVANCE_TOKEN;

return (negate) ? - n : n;
}
SLONG EXP_SLONG_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ S L O N G _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *	Restrict to LONG range.
 *
 **************************************/
USHORT	negate;
SLONG	n;
UCHAR	buffer [32];

negate = (MATCH (KW_MINUS)) ? TRUE : FALSE;

if (token.tok_type != tok_number)
    SYNTAX_ERROR ("<number>");

n = atoi (token.tok_string);
sprintf (buffer, "%d", n);
if (strcmp (buffer, token.tok_string) != 0)
   PAR_error ("Numeric value out of range");

if (advance_flag)
    ADVANCE_TOKEN;

return (negate) ? - n : n;
}

SSHORT EXP_SSHORT_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ S S H O R T _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *	A SSHORT is desired.
 *
 **************************************/
USHORT	negate;
SLONG	n;

negate = (MATCH (KW_MINUS)) ? TRUE : FALSE;

if (token.tok_type != tok_number)
    SYNTAX_ERROR ("<number>");

n = atoi (token.tok_string);
if (negate && n > -1L * MIN_SSHORT)
   PAR_error ("Numeric value out of range");
else if (!negate && n > (SLONG) MAX_SSHORT)
   PAR_error ("Numeric value out of range");

if (advance_flag)
    ADVANCE_TOKEN;

return (SSHORT) ((negate) ? - n : n);
}

ULONG EXP_ULONG_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ U L O N G _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *	Restrict to LONG range.
 *
 **************************************/
ULONG	n;
UCHAR	buffer [32];

if (token.tok_type != tok_number)
    SYNTAX_ERROR ("<unsigned number>");

n = atoi (token.tok_string);
sprintf (buffer, "%d", n);
if (strcmp (buffer, token.tok_string) != 0)
   PAR_error ("Numeric value out of range");

if (advance_flag)
    ADVANCE_TOKEN;

return n;
}

USHORT EXP_USHORT_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ U S H O R T _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *
 **************************************/
ULONG	n;

if (token.tok_type != tok_number)
    SYNTAX_ERROR ("<unsigned number>");

n = atoi (token.tok_string);
if (n > MAX_USHORT)
   PAR_error ("Numeric value out of range");

if (advance_flag)
    ADVANCE_TOKEN;

return (USHORT) n;
}

USHORT EXP_pos_USHORT_ordinal (
    USHORT	advance_flag)
{
/**************************************
 *
 *	E X P _ p o s _ U S H O R T _ o r d i n a l
 *
 **************************************
 *
 * Functional description
 *	Parse and convert to binary a numeric token.
 *	Which must be non-zero.
 *
 **************************************/
USHORT	n;

n = EXP_USHORT_ordinal (advance_flag);
if (n == 0)
    PAR_error ("Expected positive value");

return n;
}

void EXP_post_array (
    REF		reference)
{
/**************************************
 *
 *	E X P _ p o s t _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	We have a free reference to array.  Make sure the whole damn thing
 *	gets sucked up.
 *
 **************************************/
REQ	request;
REF	array_reference;
FLD	field;
BLB	blob;
CTX	context;

field = reference->ref_field;

if (!field->fld_array_info)
    return;

reference->ref_flags |= REF_fetch_array;

context = reference->ref_context;
request = context->ctx_request;
array_reference = MAKE_REFERENCE (&request->req_array_references);
array_reference->ref_context = context;
array_reference->ref_field = field;
array_reference->ref_level = request->req_level;
field->fld_array_info->ary_ident = CMP_next_ident();

blob = (BLB) ALLOC (BLB_LEN);
blob->blb_symbol = field->fld_symbol;
blob->blb_reference = reference;

if (!(blob->blb_seg_length = field->fld_seg_length))
    blob->blb_seg_length = 512;

blob->blb_request = request;
}

REF EXP_post_field (
    FLD		field,
    CTX		context,
    USHORT	null_flag)
{
/**************************************
 *
 *	E X P _ p o s t _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Post a field reference to a request.  This
 *	can be called from either par_variable (free
 *	standing field reference) or EXP\par_value
 *	(cross request field reference).
 *
 **************************************/
REQ	request;
REF	reference, control;
FLD	ref_field;
TEXT	s [128];

request = context->ctx_request;

/* If the reference is already posted, return the reference */

for (reference = request->req_references  ; reference; reference = reference->ref_next)
    if (reference->ref_context == context)
	{
	ref_field = reference->ref_field;
	if (ref_field == field ||
	    (ref_field->fld_symbol == field->fld_symbol && 
	     ref_field->fld_array == field->fld_array))
	    {
	    if (!null_flag && (ref_field->fld_dtype != field->fld_dtype ||
		    ref_field->fld_length != field->fld_length ||
		    ref_field->fld_scale != field->fld_scale))
		{
		if (reference->ref_flags & REF_null)
		    reference->ref_field = field;
		else
		    {
		    sprintf (s, "field %s is inconsistently cast",
			field->fld_symbol->sym_string);
		    PAR_error (s);
		    }
		}
	    if (request->req_level > reference->ref_level)
		reference->ref_level = request->req_level;

	    if (!null_flag)
		reference->ref_flags &= ~REF_null;
	    return reference;
	    }
	}

/* This is first occurrence of field, make a new reference */

reference = MAKE_REFERENCE (&request->req_references);
reference->ref_context = context;
reference->ref_field = field;
reference->ref_level = request->req_level;

if (null_flag)
    reference->ref_flags |= REF_null;

/* If this is a form request, make up control field for form field */

if (request->req_type == REQ_form)
    {
    if (field->fld_array_info)
	{
	sprintf (s, "Arrays are not supported in forms.  Field %s is an array.",
	    field->fld_symbol->sym_string);
	PAR_error (s);
	return NULL;
	}
    else
	{
	reference->ref_null = control = MAKE_REFERENCE (&request->req_references);
	control->ref_flags |= REF_pseudo;
	control->ref_field = MET_make_field ("gds__form_control", dtype_short, sizeof (SSHORT), FALSE);
	control->ref_context = context;
	control->ref_master = reference;
	}
    }

return reference;
}

BOOLEAN EXP_match_paren (void)
{
/**************************************
 *
 *	E X P _ m a t c h _ p a r e n
 *
 **************************************
 *
 * Functional description
 *	Match a trailing parenthesis.  If isn't one, generate an error
 *	and return FALSE.
 *
 **************************************/

if (MATCH (KW_RIGHT_PAREN))
    return TRUE;

SYNTAX_ERROR ("right parenthesis");
}

REL EXP_relation (void)
{
/**************************************
 *
 *	E X P _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Parse and look up a qualfied relation name.
 *
 **************************************/
SYM	symbol;
REL	relation, temp;
DBB	db;
TEXT	s [256];

if (!isc_databases)
   PAR_error ("no database for operation");

/* The current token is (i.e. should be) either a relation
   name or a database name.  If it's a database name, search
   it for the relation name.  If it's an unqualified relation
   name, search all databases for the name */

relation = NULL;

SQL_resolve_identifier ("<identifier>", s);
if (symbol = MSC_find_symbol (token.tok_symbol, SYM_database))
    {
    db = (DBB) symbol->sym_object;
    ADVANCE_TOKEN;
    if (!MATCH (KW_DOT))
	SYNTAX_ERROR ("period after database name");
    SQL_resolve_identifier ("<Table name>", s);
    relation = MET_get_relation (db, token.tok_string, "");
    }
else
    {
    for (db = isc_databases; db; db = db->dbb_next)
	if (temp = MET_get_relation (db, token.tok_string, ""))
	    {
	    if (!relation)
		relation = temp;
	    else
		{
		sprintf (s, "relation %s is ambiguous", token.tok_string);
		ADVANCE_TOKEN;
		PAR_error (s);
		}
	    }
   }

if (!relation)
    SYNTAX_ERROR ("relation name");

ADVANCE_TOKEN;

return relation;
}

RSE EXP_rse (
    REQ		request,
    SYM		initial_symbol)
{
/**************************************
 *
 *	E X P _ r s e
 *
 **************************************
 *
 * Functional description
 *	Parse a record selection expression.  If there is an
 *	error, return NULL.  This is slightly complicated by
 *	the fact that PASCAL and FORTRAN have a native FOR
 *	statement, and ADA has a FOR <variable> IN statement.
 *
 *	If an initial symbol is given, the caller has already
 *	parsed the <contect> IN part of the expression.
 *
 **************************************/
NOD	first, item, boolean, sort, *ptr, upcase;
RSE	rec_expr;
CTX	context;
LLS	items;
LLS	directions;
SSHORT	count, direction, insensitive;

first = boolean = NULL;

/* parse FIRST n clause, if present */

if (MATCH (KW_FIRST))
    {
    if (!count_field)
	count_field = MET_make_field ("jrd_count", dtype_long, 4, FALSE);
    first = par_value (request, count_field);
    }
    
/* parse first context clause */

if (initial_symbol && sw_language == lan_ada && !check_relation())
    return NULL;

context = EXP_context (request, initial_symbol);
count = 1;

/* parse subsequent context clauses if this is a join */

while (MATCH (KW_CROSS))
    {
    context = EXP_context (request, NULL_PTR);
    count++;
    if (MATCH (KW_OVER))
	boolean = make_and (boolean, par_over (context));
    }

/* bug_3380 - could have an "over" clause without a "cross" clause */

if (MATCH (KW_OVER))
	boolean = make_and (boolean, par_over (context));

/* build rse node */

rec_expr = (RSE) ALLOC (RSE_LEN(count));
rec_expr->rse_count = count;
rec_expr->rse_first = first;
rec_expr->rse_boolean = boolean;

while (count)
    {
    rec_expr->rse_context[--count] = context;
    HSH_insert (context->ctx_symbol);
    context = context->ctx_next;
    }

/* parse boolean, if any.  If there is an error, ignore the
   boolean, but keep the rse */

if (MATCH (KW_WITH))
    boolean = make_and (boolean, par_boolean (request));

rec_expr->rse_boolean = boolean;
    
/* Parse SORT clause, if any. */

direction = FALSE;
insensitive = FALSE;
while (TRUE)
    {
    if (MATCH (KW_SORTED))
	{
	MATCH (KW_BY);
	items = NULL;
	direction = 0;
	count = 0;
	while (TRUE)
	    {
	    if (MATCH (KW_ASCENDING))
		{
		direction = FALSE;
		continue;
		}
	    else if (MATCH (KW_DESCENDING))
		{
		direction = TRUE;
		continue;
		}
	    else if (MATCH (KW_EXACTCASE))
		{
		insensitive = FALSE;
		continue;
		}
	    else if (MATCH (KW_ANYCASE))
		{
		insensitive = TRUE;
		continue;
		}
	    item = par_value (request, NULL_PTR);
	    if (insensitive)
		{
		upcase = MAKE_NODE (nod_upcase, 1);
		upcase->nod_arg[0] = item;
		}
	    count++;
	    PUSH ((NOD) direction, &directions);
	    if (insensitive)
		PUSH (upcase, &items);
	    else
		PUSH (item, &items);
	    if (!MATCH (KW_COMMA))
		break;
	    }
	rec_expr->rse_sort = sort = MAKE_NODE (nod_sort, count * 2);
	sort->nod_count = count;
	ptr = sort->nod_arg + count * 2;
	while (--count >= 0)
	    {
	    *--ptr = (NOD) POP (&items);
	    *--ptr = (NOD) POP (&directions);
	    }
	}

    /* Parse REDUCED clause, if any. */

    else if (MATCH (KW_REDUCED))
	{
	MATCH (KW_TO);
	items = NULL;
	count = 0;
	while (TRUE)
	    {
	    item = par_value (request, NULL_PTR);
	    count++;
	    PUSH (item, &items);
	    if (!MATCH (KW_COMMA))
		break;
	    }
	rec_expr->rse_reduced = sort = MAKE_NODE (nod_projection, count);
	sort->nod_count = count;
	ptr = sort->nod_arg + count;
	while (--count >= 0)
	    *--ptr = (NOD) POP (&items);
	}
    else
	break;
    }

return rec_expr;
}

void EXP_rse_cleanup (
    RSE		rse)
{
/**************************************
 *
 *	E X P _ r s e _ c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Remove any context variables from hash table for a record
 *	selection expression.
 *
 **************************************/
NOD	node;
CTX	*context, *end;
USHORT	i;

/* Clean up simple context variables */

context = rse->rse_context;
end = context + rse->rse_count;

for (; context < end; context++)
    if ((*context)->ctx_symbol)
	HSH_remove ((*context)->ctx_symbol);

/* If this is an aggregate, clean up the underlying rse */

if (rse->rse_aggregate)
    EXP_rse_cleanup (rse->rse_aggregate);

/* If this is a union, clean up each of the primitive rse's */

if (node = rse->rse_union)
    for (i = 0; i < node->nod_count; i++)
	EXP_rse_cleanup (node->nod_arg [i]);
}

NOD EXP_subscript (
    REQ		request)
{
/**************************************
 *
 *	E X P _ s u b s c r i p t
 *
 **************************************
 *
 * Functional description
 *	Parse a subscript value.  This is called by PAR\par_slice.
 *
 **************************************/
NOD	node;
REF	reference;
TEXT	*string;

reference = (REF) ALLOC (REF_LEN);
node = MSC_unary (nod_value, reference);

/* Special case literals */

if (token.tok_type == tok_number)
    {
    node->nod_type = nod_literal;
    reference->ref_value = string = (TEXT*) ALLOC (token.tok_length+1);
    COPY (token.tok_string, token.tok_length, string);
    ADVANCE_TOKEN;
    return node;
    }

reference->ref_value = PAR_native_value (TRUE, FALSE);

if (request)
    {
    reference->ref_next = request->req_values;
    request->req_values = reference;
    }

return node;
}

static BOOLEAN check_relation (void)
{
/**************************************
 *
 *	c h e c k _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Check current token for either a relation or database name.
 *
 **************************************/
SYM	symbol;
DBB	db;

/* The current token is (i.e. should be) either a relation
   name or a database name.  If it's a database name, search
   it for the relation name.  If it's an unqualified relation
   name, search all databases for the name */

if ((symbol = token.tok_symbol) && symbol->sym_type == SYM_database)
    return TRUE;

for (db = isc_databases; db; db = db->dbb_next)
    if (MET_get_relation (db, token.tok_string, ""))
	return TRUE;

return FALSE;
}

static NOD lookup_field (
    CTX	context)
{
/**************************************
 *
 *	l o o k u p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Check to see if the current token is a field name corresponding
 *	to a given context.  If so, return a field block (with reference
 *	block) for field.
 *
 **************************************/
REF	reference;
FLD	field;
char	s[132];

SQL_resolve_identifier ("<Field Name>", s);
if (!(field = MET_field (context->ctx_relation, token.tok_string)))
    return NULL;

reference = (REF) ALLOC (REF_LEN);
reference->ref_field = field;
reference->ref_context = context;

return MSC_unary (nod_field, reference);
}

static NOD make_and (
    NOD		node1,
    NOD		node2)
{
/**************************************
 *
 *	m a k e _ a n d
 *
 **************************************
 *
 * Functional description
 *	Combine two (potention) conjuncts into a single, valid
 *	boolean.  Either or both on the conjunctions may be NULL.
 *	If both are null, return null.
 *
 **************************************/

if (!node1)
    return node2;

if (!node2)
    return NULL;

return MSC_binary (nod_and, node1, node2);
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
 *	Make a generic variable length node from a stack.
 *
 **************************************/
USHORT	count;
LLS	temp;
NOD	node, *ptr;

for (temp = stack, count = 0; temp; temp = temp->lls_next)
    ++count;

node = MAKE_NODE (nod_list, count);

for (ptr = node->nod_arg + count; stack;)
   *--ptr = POP (&stack);

return node;
}

static NOD normalize_index (
    DIM		dimension,
    NOD		user_index,
    USHORT	array_base)
{
/**************************************
 *
 *	n o r m a l i z e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	"Normalize" the array index so that
 *	the index used in the rse refers to
 *	the same relative position in the
 *	dimension in the database as it is
 *	in the user's program.
 *
 **************************************/
NOD    index_node, adjustment_node, negate_node;
REF    reference;
TEXT   string [33];
BOOLEAN negate;

negate = FALSE;
switch (array_base)
    {
    case ZERO_BASED:
	if (dimension->dim_lower < 0)
	    negate = TRUE;
	sprintf (string, "%d", abs (dimension->dim_lower));
	break;

    case ONE_BASED:
	if (dimension->dim_lower - 1 < 0)
	    negate = TRUE;
	sprintf (string, "%d", abs (dimension->dim_lower - 1));
	break;

    default:
	return user_index;
    }

reference = (REF) ALLOC (REF_LEN);
reference->ref_value = (TEXT*) ALLOC (strlen (string));
strcpy (reference->ref_value, string);
adjustment_node = MSC_unary (nod_literal, reference);

if (negate)
    negate_node = MSC_unary (nod_negate, adjustment_node);
else
    negate_node = adjustment_node;

index_node = MSC_binary (nod_plus, negate_node, user_index);

return index_node;
}

static NOD par_and (
    REQ	request)
{
/**************************************
 *
 *	p a r _ a n d 
 *
 **************************************
 *
 * Functional description
 *	Parse a boolean AND.
 *
 **************************************/
NOD	expr1;

expr1 = par_not (request);

if (!MATCH (KW_AND))
    return expr1;

return MSC_binary (nod_and, expr1, par_and (request));
}

static NOD par_array (
    REQ 	request,
    FLD		field,
    SSHORT	subscript_flag,
    SSHORT	sql_flag)
{
/**************************************
 *
 *	p a r _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	Parse a array element reference
 *	(array name and subscript list)
 *	in an RSE.
 *
 **************************************/
BOOLEAN	paren = FALSE, bracket = FALSE;
DIM	dimension;
NOD	node, index_node, array_node;
int	i, fortran_adjustment;

if (MATCH (KW_LEFT_PAREN))
    paren = TRUE;
else if (MATCH (KW_L_BRCKET))
    bracket = TRUE;
else if (!subscript_flag)
    SYNTAX_ERROR ("Missing parenthesis or bracket for array reference.");

array_node = MSC_node (nod_array, field->fld_array_info->ary_dimension_count + 1);

if (sql_flag && ((paren && MATCH (KW_RIGHT_PAREN)) || 
                 (bracket && MATCH (KW_R_BRCKET))))
    return array_node;

fortran_adjustment = array_node->nod_count;
if (paren || bracket)
   {
   if (!subscript_field)
	subscript_field = MET_make_field ("gds_array_subscript", dtype_long, 4, FALSE);

   /*  Parse a commalist of subscripts and build a tree of index nodes  */

   for (dimension = field->fld_array_info->ary_dimension, i = 1; dimension;
	 dimension = dimension->dim_next, i++)
	{
	if (!sql_flag)
	    node = par_value (request, subscript_field);
	else
	    {
	    node = SQE_value (request, FALSE, NULL_PTR, NULL_PTR);

	    /* For all values referenced, post the subscript field   */

	    SQE_post_field (node, subscript_field);
	    }

	index_node = MSC_unary (nod_index, node);

	/* Languages which can't handle negative or non-positive bounds need to
	   be accomodated with normalization of the indices.  */

	switch (sw_language)
	    {
	    case lan_c:
	    case lan_cxx:
	    case lan_basic:
		index_node->nod_arg[0] = normalize_index (dimension, index_node->nod_arg[0], ZERO_BASED);
		break;

	    case lan_cobol:
		index_node->nod_arg[0] = normalize_index (dimension, index_node->nod_arg[0], ONE_BASED);
		break;
	    }

	/*  Error checking of constants being out of range will be here in the future.  */

	/*  Good ole Fortran's column major order needs to be accomodated.  */

	if (sw_language == lan_fortran)
	    array_node->nod_arg [fortran_adjustment - i] = index_node;
	else
	    array_node->nod_arg [i] = index_node;

	if ((dimension->dim_next) && (!MATCH (KW_COMMA)))
	    SYNTAX_ERROR ("Adequate number of subscripts for this array reference.");
	}

    /*  Match the parenthesis or bracket  */

    if ((paren) && (!MATCH (KW_RIGHT_PAREN)))
	SYNTAX_ERROR ("Missing parenthesis for array reference.");
    else if ((bracket) && !MATCH (KW_R_BRCKET))
	SYNTAX_ERROR ("Missing right bracket for array reference.");
    }

return array_node;
}

static NOD par_boolean (
    REQ	request)
{
/**************************************
 *
 *	p a r _ b o o l e a n
 *
 **************************************
 *
 * Functional description
 *	Parse a boolean expression.  Actually, just parse
 *	an OR node or anything of lower precedence.
 *
 **************************************/
NOD	expr1;

expr1 = par_and (request);

if (!MATCH (KW_OR) && !MATCH (KW_OR1))
    return expr1;

return MSC_binary (nod_or, expr1, par_boolean (request));
}

static NOD par_field (
    REQ	request)
{
/**************************************
 *
 *	p a r _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Parse a field reference.  Anything else is an error.
 *
 **************************************/
SYM		symbol;
FLD		field, cast;
CTX		context;
REF		reference, value_reference;
NOD		node, prefix_node;
SSHORT		upcase_flag;

if (!(symbol = token.tok_symbol))
    SYNTAX_ERROR ("qualified field reference");

upcase_flag = FALSE;

if (MATCH (KW_UPPERCASE))
    {
    prefix_node = MAKE_NODE (nod_upcase, 1);
    upcase_flag = TRUE;
    if ( !MATCH (KW_LEFT_PAREN))
	SYNTAX_ERROR ("left parenthesis");
    if (!(symbol = token.tok_symbol))
	SYNTAX_ERROR ("qualified field reference");
    }

if (symbol->sym_type == SYM_context)
    {
    field = EXP_field (&context);
    if (field->fld_array_info)
	node = par_array (request, field, FALSE, FALSE);

    if (MATCH (KW_DOT) && (cast = EXP_cast (field)))
	field = cast;
    }
else if (symbol->sym_type == SYM_form_map)
    field = EXP_form_field (&context);

else
    SYNTAX_ERROR ("qualified field reference");

/* There is a legit field reference.  If the reference is
   to a field in this request, make up a reference block
   and a field node, and return. */

if (!field->fld_array_info)
    node = MAKE_NODE (nod_field, 1);

if (upcase_flag)
    prefix_node->nod_arg [0] = node;

if (context->ctx_request == request)
    {
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_field = field;
    reference->ref_context = context;
    if (node->nod_type == nod_array)
	reference->ref_flags |= REF_array_elem;
    node->nod_arg [0] = (NOD) reference;
    }
else
    {
    /* Field wants to straddle two requests.  We need to do
       two things.  First, post a reference to the field to
       the other request.  This is a variance on code found
       in par_variable in par.c */

    reference = EXP_post_field (field, context, FALSE);

    /* Next, make a value reference for this request */

    value_reference = MAKE_REFERENCE (&request->req_values);
    value_reference->ref_field = reference->ref_field;
    value_reference->ref_source = reference;

    node->nod_type = nod_value;
    node->nod_arg [0] = (NOD) value_reference;
    }

if (upcase_flag)
    {
    if (!MATCH (KW_RIGHT_PAREN))
	SYNTAX_ERROR ("right parenthesis");
    return prefix_node;
    }

return node;
}

static NOD par_multiply (
    REQ	request,
    FLD	field)
{
/**************************************
 *
 *	p a r _ m u l t i p l y
 *
 **************************************
 *
 * Functional description
 *	Parse a value expression.  In specific, handle the lowest
 *	precedence operator plus/minus.
 *
 **************************************/
NOD		node, arg;
enum nod_t	operator;

node = par_primitive_value (request, field);

while (TRUE)
    {
    if (MATCH (KW_ASTERISK))
	operator = nod_times;
    else if (MATCH (KW_SLASH))
	operator = nod_divide;
    else 
	return node;
    arg = node;
    node = MSC_binary (operator, arg, par_primitive_value (request, field));
    }
}

static NOD par_native_value (
    REQ	request,
    FLD	field)
{
/**************************************
 *
 *	p a r _ n a t i v e _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Parse a native C value.
 *
 **************************************/
NOD	node;
REF	reference;
TEXT	s [64];

/* Special case literals */

if (token.tok_type == tok_number || SINGLE_QUOTED(token.tok_type) || (DOUBLE_QUOTED(token.tok_type) && (sw_sql_dialect==1)) )
    {
    node = EXP_literal();
    return node;
    }

reference = (REF) ALLOC (REF_LEN);
node = MSC_unary (nod_value, reference);

/* Handle general native value references.  Since these values will need
   to be exported to the database system, make sure there is a reference
   field. */

reference->ref_value = PAR_native_value (FALSE, FALSE);

if (!field)
    {
    sprintf (s, "no reference field for %s", reference->ref_value);
    PAR_error (s);
    }

reference->ref_next = request->req_values;
request->req_values = reference;
reference->ref_field = field;

return node;
}

static NOD par_not (
    REQ	request)
{
/**************************************
 *
 *	p a r _ n o t 
 *
 **************************************
 *
 * Functional description
 *	Parse either a boolean NOT or a boolean parenthetical.
 *
 **************************************/
NOD		node;

if (MATCH (KW_LEFT_PAREN))
    {
    node = par_boolean (request);
    EXP_match_paren();
    return node;
    }

if (node = par_udf (request, UDF_boolean, NULL_PTR))
    return node;

if (!(MATCH (KW_NOT)))
    return par_relational (request);

return MSC_unary (nod_not, par_not (request));
}

static NOD par_over (
    CTX		context)
{
/**************************************
 *
 *	p a r _ o v e r
 *
 **************************************
 *
 * Functional description
 *	Parse the substance of an OVER clause (but not the leading keyword).
 *
 **************************************/
NOD	boolean, field1, field2;
CTX	next;
TEXT	s [64];

boolean = NULL;

do
    {
    if (!(field1 = lookup_field (context)))
	{
	sprintf (s, "OVER field %s undefined", token.tok_string);
	PAR_error (s);
	}
    field2 = NULL;
    for (next = context->ctx_next; next; next = next->ctx_next)
	if (field2 = lookup_field (next))
	    break;
    if (!field2)
	{
	sprintf (s, "OVER field %s undefined", token.tok_string);
	PAR_error (s);
	}
    boolean = make_and (boolean, MSC_binary (nod_eq, field1, field2));
    ADVANCE_TOKEN;
    }
while (MATCH (KW_COMMA));

return boolean;
}

static NOD par_primitive_value (
    REQ	request,
    FLD	field)
{
/**************************************
 *
 *	p a r _ p r i m i t i v e _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Parse a value expression.  In specific, handle the lowest
 *	precedence operator plus/minus.
 *
 **************************************/
NOD	node, sub;
SYM	symbol;

if (MATCH (KW_MINUS))
    return MSC_unary (nod_negate, par_primitive_value (request, field));

if (MATCH (KW_LEFT_PAREN))
    {
    node = par_value (request, field);
    EXP_match_paren();
    return node;
    }

if (MATCH (KW_UPPERCASE))
    {
    node = MAKE_NODE (nod_upcase, 1);
    sub = par_primitive_value (request, field);
    node->nod_arg [0] = sub;
    return node;
    }

if (MATCH (KW_USER_NAME))
    return MAKE_NODE (nod_user_name, 0);

/* Check for user defined functions */

if (node = par_udf (request, UDF_value, field))
    return node;

if (!(symbol = token.tok_symbol) ||
    (symbol->sym_type != SYM_context && symbol->sym_type != SYM_form_map))
    return par_native_value (request, field);

return par_field (request);
}

static NOD par_relational (
    REQ	request)
{
/**************************************
 *
 *	p a r _ r e l a t i o n a l
 *
 **************************************
 *
 * Functional description
 *	Parse a relational expression.
 *
 **************************************/
NOD		expr, expr1, expr2;
REF		reference;
FLD		field;
USHORT		negation;
struct rel_ops	*relop;

if (MATCH (KW_ANY))
    {
    expr = MAKE_NODE (nod_any, 1);
    expr->nod_count = 0;
    expr->nod_arg [0] = (NOD) EXP_rse (request, NULL_PTR);
    EXP_rse_cleanup (expr->nod_arg [0]);
    return expr;
    }

if (MATCH (KW_UNIQUE))
    {
    expr = MAKE_NODE (nod_unique, 1);
    expr->nod_count = 0;
    expr->nod_arg [0] = (NOD) EXP_rse (request, NULL_PTR);
    EXP_rse_cleanup (expr->nod_arg [0]);
    return expr;
    }

/*  That's right, three pointer dereferences to get to the reference
    structure if there's a UDF.  V3 bug#531.  MaryAnn  12/4/89  */

if (expr1 = par_udf (request, UDF_value, NULL_PTR))
    reference = (REF) (expr1->nod_arg[0]->nod_arg[0]->nod_arg[0]);
else
    {
    expr1 = par_field (request);
    reference = (REF) expr1->nod_arg [0];
    }

field = reference->ref_field;

/* Check for any of the binary guys */

negation = MATCH (KW_NOT);

for (relop = relops;; relop++)
    if ((int) relop->rel_kw == (int) KW_none)
	SYNTAX_ERROR ("relational operator");
    else if (MATCH (relop->rel_kw))
	break;

expr2 = NULL;
if ((int) relop->rel_kw == (int) KW_STARTING)
    {
    MATCH (KW_WITH);
    expr = MAKE_NODE (relop->rel_op, relop->rel_args);
    }
else if ((int) relop->rel_kw == (int) KW_MATCHES)
    {
    expr2 = par_value (request, field);
    if (MATCH (KW_USING))
	expr = MAKE_NODE (nod_sleuth, 3);
    else
	expr = MAKE_NODE (nod_matches, 2);
    }
else
    expr = MAKE_NODE (relop->rel_op, relop->rel_args);

expr->nod_arg [0] = expr1;

switch (expr->nod_type)
    {
    case nod_missing:
	break;

    case nod_between:
	expr->nod_arg [1] = expr2 = par_value (request, field);
	MATCH (KW_AND);
	expr->nod_arg [2] = par_value (request, field);
	break;

    case nod_matches:
	expr->nod_arg[1] = expr2;
	break;

    case nod_sleuth:
	expr->nod_arg [1] = expr2;
	expr->nod_arg [2] = par_value (request, field);
	break;

    default:
	expr->nod_arg [1] = expr2 = par_value (request, field);
    }

if (expr2)
    if (expr1->nod_type == nod_array && expr2->nod_type == nod_value)
	((REF) expr2->nod_arg [0])->ref_flags |=
	    ((REF) expr1->nod_arg [0])->ref_flags & REF_array_elem;
    else if (expr2->nod_type == nod_array && expr1->nod_type == nod_value)
	((REF) expr1->nod_arg [0])->ref_flags |=
	    ((REF) expr2->nod_arg [0])->ref_flags & REF_array_elem;

if (!negation)
    return expr;

return MSC_unary (nod_not, expr);
}

static NOD par_udf (
    REQ		request,
    USHORT	type,
    FLD		field)
{
/**************************************
 *
 *	p a r _ u d f
 *
 **************************************
 *
 * Functional description
 *	Parse a user defined function.  If the current token isn't one,
 *	return NULL.  Otherwise try to parse one.  If things go badly,
 *	complain bitterly.
 *
 **************************************/
NOD	node;
SYM	symbol;
LLS	stack;
UDF	udf;

if (!request)
    return NULL;

/* Check for user defined functions */

for (symbol = token.tok_symbol; symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_udf &&
	(udf = (UDF) symbol->sym_object) &&
	/* request->req_database == udf->udf_database && */
	udf->udf_type == type)
	{
	node = MAKE_NODE (nod_udf, 2);
	node->nod_count = 1;
	node->nod_arg [1] = (NOD) udf;
	ADVANCE_TOKEN;
	if (!MATCH (KW_LEFT_PAREN))
	    EXP_left_paren (NULL_PTR);
	stack = NULL;
	for (;;)
	    {
	    PUSH (par_value (request, field), &stack);
	    if (!MATCH (KW_COMMA))
		break;
	    }
	EXP_match_paren();
	node->nod_arg [0] = make_list (stack);
	return node;
	}

return NULL;
}

static NOD par_value (
    REQ	request,
    FLD	field)
{
/**************************************
 *
 *	p a r _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Parse a value expression.  In specific, handle the lowest
 *	precedence operator plus/minus.
 *
 **************************************/
NOD		node, arg;
enum nod_t	operator;

node = par_multiply (request, field);

while (TRUE)
    {
    if (MATCH (KW_PLUS))
	operator = nod_plus;
    else if (MATCH (KW_MINUS))
	operator = nod_minus;
    else 
	return node;
    arg = node;
    node = MSC_binary (operator, arg, par_value (request, field));
    }
}
