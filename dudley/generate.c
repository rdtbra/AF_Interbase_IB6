/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		generate.c
 *	DESCRIPTION:	Blr generator
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
#include "../jrd/gds.h"
#include "../dudley/ddl.h"
#include "../jrd/acl.h"
#include "../dudley/gener_proto.h"
#include "../jrd/gds_proto.h"
#include "../dudley/trn_proto.h"

static void	generate (STR, NOD);
static void	get_set_generator (STR, NOD);

#define STUFF(c)	*blr->str_current++ = c
#define STUFF_WORD(c)	{STUFF (c); STUFF (c >> 8);}
#define CHECK_BLR(l)	{if ( !(blr->str_current - blr->str_start + l <= \
                                blr->str_length) && !TRN_get_buffer (blr, l) )\
                             DDL_err (289, NULL, NULL, NULL, NULL, NULL);}

int GENERATE_acl (
    SCL		class,
    UCHAR	*buffer)
{
/**************************************
 *
 *	G E N E R A T E _ a c l
 *
 **************************************
 *
 * Functional description
 *	Generate an access control list.
 *
 **************************************/
UCHAR	*p, *q, **id, c;
SCE	item;
USHORT	i;

#if (defined JPN_SJIS || defined JPN_EUC)

/* This does not need Japanization because Kanji User names are not supported */

#endif

p = buffer;
*p++ = ACL_version;

for (item = class->scl_entries; item; item = item->sce_next)
    {
    *p++ = ACL_id_list;
    for (i = 0, id = item->sce_idents; i < id_max; id++, i++)
	if (q = *id)
	    {
	    *p++ = i;
	    *p++ = strlen (q);
	    while (c = *q++)
		*p++ = UPPER (c);
	    }
    *p++ = priv_end;
    *p++ = ACL_priv_list;
    for (i = 0; i < priv_max; i++)
	if (item->sce_privileges & (1 << i))
	    *p++ = i;
    *p++ = id_end;
    }

*p++ = ACL_end;

return p - buffer;
}

void GENERATE_blr (
    STR		blr,
    NOD		node)
{
/**************************************
 *
 *	G E N E R A T E _ b l r
 *
 **************************************
 *
 * Functional description
 *	Compile a node tree into a blr string.
 *
 **************************************/

CHECK_BLR(1);
STUFF (blr_version4);
generate (blr, node);
CHECK_BLR(1);
STUFF (blr_eoc);
}

static void generate (
    STR		blr,
    NOD		node)
{
/**************************************
 *
 *	g e n e r a t e
 *
 **************************************
 *
 * Functional description
 *	Generate blr.
 *
 **************************************/
FLD	field;
REL	relation;
SYM	symbol;
CON	constant;
NOD	sub, *arg, *end;
CTX	context;
SCHAR	operator, *p;
SLONG 	value;
USHORT	l;

arg = node->nod_arg;
end = arg + node->nod_count;

switch (node->nod_type)
    {
    case nod_fid:
	CHECK_BLR(4);
	STUFF (blr_fid);
	STUFF (0);
        STUFF_WORD (0);
	return;

    case nod_field:
	CHECK_BLR(4);
	if (sub = node->nod_arg [s_fld_subs])
	    STUFF (blr_index);
	STUFF (blr_field);
	field = (FLD) node->nod_arg [s_fld_field];
	context = (CTX) node->nod_arg [s_fld_context];
	STUFF (context->ctx_context_id);
	symbol = field->fld_name;
	STUFF (l = symbol->sym_length);
	p = (SCHAR*) symbol->sym_string;
	if (l)
	    {
	    CHECK_BLR(l);
	    do STUFF (*p++); while (--l);
	    }
	if (sub)
	    {
	    CHECK_BLR(1);
	    STUFF (sub->nod_count);
	    for (arg = sub->nod_arg, end = arg + sub->nod_count; arg < end; arg++)
		generate (blr, *arg);
	    }
	return;
    
    case nod_function:
	CHECK_BLR(2);
	STUFF (blr_function);
	symbol = (SYM) node->nod_arg [1];
	STUFF (l = symbol->sym_length);
	p = (SCHAR*) symbol->sym_string;
	if (l)
	    {
	    CHECK_BLR(l);
	    do STUFF (*p++); while (--l);
	    }
	sub = node->nod_arg [0];
	CHECK_BLR(1);
	STUFF (sub->nod_count);
	for (arg = sub->nod_arg, end = arg + sub->nod_count; arg < end; arg++)
	    generate (blr, *arg);
	return;
    
    case nod_context:
	context = (CTX) node->nod_arg [0];
	relation = context->ctx_relation;
	CHECK_BLR(2);
	STUFF (blr_relation);
	symbol = relation->rel_name;
	STUFF (l = symbol->sym_length);
	p = (SCHAR*) symbol->sym_string;
	if (l)
	    {
	    CHECK_BLR(l);
	    do STUFF (*p++); while (--l);
	    }
	CHECK_BLR(1);
	STUFF (context->ctx_context_id);
	return;
    
    case nod_gen_id:
	CHECK_BLR(2);
	STUFF (blr_gen_id);
	symbol = (SYM) node->nod_arg [1];
	STUFF (l = symbol->sym_length);
	p = (SCHAR*) symbol->sym_string;
	if (l)
	    {
	    CHECK_BLR(l);
	    do STUFF (*p++); while (--l);
	    }
	generate (blr, node->nod_arg [0]);
	return;
    
    case nod_set_generator:
        get_set_generator (blr, node);
        return;

    case nod_user_name:
	CHECK_BLR(1);
	STUFF (blr_user_name);
	return;

    case nod_null:
	CHECK_BLR(1);
	STUFF (blr_null);
	return;

    case nod_literal:
	CHECK_BLR(6);
	STUFF (blr_literal);
	constant = (CON) node->nod_arg [0];
	l = constant->con_desc.dsc_length;
	switch (constant->con_desc.dsc_dtype)
	    {
	    case dtype_text:
#if (!(defined JPN_SJIS || defined JPN_EUC))

		STUFF (blr_text);

#else

		STUFF (blr_text2);
		STUFF_WORD (DDL_interp)

#endif
		STUFF_WORD (l);
		break;
	    
	    case dtype_short:
		CHECK_BLR(2);
		STUFF (blr_short);
		STUFF (constant->con_desc.dsc_scale);
		break;

	    case dtype_long:
		CHECK_BLR(2);
		STUFF (blr_long);
		STUFF (constant->con_desc.dsc_scale);
		break;

	    case dtype_quad:
		CHECK_BLR(2);
		STUFF (blr_quad);
		STUFF (constant->con_desc.dsc_scale);
		break;

	    case dtype_real:
		CHECK_BLR(1);
		STUFF (blr_float);
		break;

	    case dtype_double:
		CHECK_BLR(1);
		STUFF (blr_double);
		break;
	    
	    case dtype_timestamp:
		CHECK_BLR(1);
		STUFF (blr_timestamp);
		break;

	    case dtype_int64:
	    case dtype_sql_time:
	    case dtype_sql_date:
	    default:
		DDL_err (95, NULL, NULL, NULL, NULL, NULL);/* msg 95: GENERATE_blr: dtype not supported */
	    }
	p = (SCHAR*) constant->con_data;
	switch (constant->con_desc.dsc_dtype)
	    {
	    case dtype_short:
		value = *(SSHORT*) p;
		CHECK_BLR(2);
		STUFF_WORD (value);
		break;

	    case dtype_long:
		value = *(SLONG*) p;
		CHECK_BLR(4);
		STUFF_WORD (value);
		STUFF_WORD (value >> 16);
		break;

	    case dtype_quad:
	    case dtype_timestamp:
		value = *(SLONG*) p;
		CHECK_BLR(8);
		STUFF_WORD (value);
		STUFF_WORD (value >> 16);
		value = *(SLONG*) (p + 4);
		STUFF_WORD (value);
		STUFF_WORD (value >> 16);
		break;

	    default:
		if (l)
		    {
		    CHECK_BLR(l);
		    do STUFF (*p++); while (--l);
		    }
	    }
	return;
    
    case nod_rse:
	CHECK_BLR(2);
	STUFF (blr_rse);
	sub = node->nod_arg [s_rse_contexts];
	STUFF (sub->nod_count);
	for (l = 0, arg = sub->nod_arg; l < sub->nod_count; l++, arg++)
	    generate (blr, *arg);
	if (sub = node->nod_arg [s_rse_first])
	    {
	    CHECK_BLR(1);
	    STUFF (blr_first);
	    generate (blr, sub);
	    }
	if (sub = node->nod_arg [s_rse_boolean])
	    {
	    CHECK_BLR(1);
	    STUFF (blr_boolean);
	    generate (blr, sub);
	    }
	if (sub = node->nod_arg [s_rse_sort])
	    {
	    CHECK_BLR(2);
	    STUFF (blr_sort);
	    STUFF (sub->nod_count / 2);
	    for (arg = sub->nod_arg, end = arg + sub->nod_count; 
		 arg < end; arg += 2)
		{
		CHECK_BLR(1);
		STUFF ((arg [1]) ? blr_descending : blr_ascending);
		generate (blr, arg [0]);
		}
	    }
	if (sub = node->nod_arg [s_rse_reduced])
	    {
	    CHECK_BLR(2);
	    STUFF (blr_project);
	    STUFF (sub->nod_count / 2);
	    for (arg = sub->nod_arg, end = arg + sub->nod_count; 
		 arg < end; arg += 2)
		generate (blr, arg [0]);
	    }
	CHECK_BLR(1);
	STUFF (blr_end);
	return;

    case nod_count:
    case nod_max:
    case nod_min:
    case nod_total:
    case nod_average:
    case nod_from:
	switch (node->nod_type)
	    {
	    case nod_count:	operator = blr_count;	break;
	    case nod_max:	operator = blr_maximum;	break;
	    case nod_min:	operator = blr_minimum;	break;
	    case nod_total:	operator = blr_total;	break;
	    case nod_average:	operator = blr_average;	break;
	    case nod_from:	
		operator = (node->nod_arg [s_stt_default]) ? blr_via : blr_from;
		break;
	    }
	CHECK_BLR(1);
	STUFF (operator);
	generate (blr, node->nod_arg [s_stt_rse]);
	if (sub = node->nod_arg [s_stt_value])
	    generate (blr, sub);
	if (sub = node->nod_arg [s_stt_default])
	    generate (blr, sub);
	return;

     case nod_any:
	CHECK_BLR(1);
	STUFF (blr_any);
	generate (blr, node->nod_arg [0]);
	return;

    case nod_unique:
	CHECK_BLR(1);
	STUFF (blr_unique);
	generate (blr, node->nod_arg [0]);
	return;
  
    case nod_if:
	CHECK_BLR(1);
	STUFF (blr_if);
	generate (blr, *arg++);
	generate (blr, *arg++);
	if (*arg)
	    generate (blr, *arg);
	else
	    {
	    CHECK_BLR(1);
	    STUFF (blr_end);
	    }
	return;

    case nod_list:
	CHECK_BLR(1);
	STUFF (blr_begin);
	for (; arg < end; arg++)
	    generate (blr, *arg);
	CHECK_BLR(1);
	STUFF (blr_end);
	return;

    case nod_abort:
	CHECK_BLR(2);
	STUFF (blr_leave);
	STUFF ((int) node->nod_arg [0]);
	return;

    case nod_erase:
	CHECK_BLR(2);
	STUFF (blr_erase);
	context = (CTX) node->nod_arg [0];
	STUFF (context->ctx_context_id);
	return;

    case nod_modify:
	CHECK_BLR(3);
	STUFF (blr_modify);
	context = (CTX) node->nod_arg [s_mod_old_ctx];
	STUFF (context->ctx_context_id);
	context = (CTX) node->nod_arg [s_mod_new_ctx];
	STUFF (context->ctx_context_id);
	generate (blr, node->nod_arg [s_mod_action]);
	return;

    case nod_eql:		operator = blr_eql; break;
    case nod_neq:		operator = blr_neq; break;
    case nod_gtr:		operator = blr_gtr; break;
    case nod_geq:		operator = blr_geq; break;
    case nod_leq:		operator = blr_leq; break;
    case nod_lss:		operator = blr_lss; break;
    case nod_between:		operator = blr_between; break;
    case nod_matches:		operator = blr_matching; break;
    case nod_containing:	operator = blr_containing; break;
    case nod_starts:		operator = blr_starting; break;
    case nod_missing:		operator = blr_missing; break;
    case nod_and:		operator = blr_and; break;
    case nod_or:		operator = blr_or; break;
    case nod_not:		operator = blr_not; break;
    case nod_add:		operator = blr_add; break;
    case nod_subtract:		operator = blr_subtract; break;
    case nod_multiply:		operator = blr_multiply; break;
    case nod_divide:		operator = blr_divide; break;
    case nod_negate:		operator = blr_negate; break;
    case nod_concatenate:	operator = blr_concatenate; break;
    case nod_for:		operator = blr_for; break;
    case nod_assignment:	operator = blr_assignment; break;
    case nod_store:		operator = blr_store; break;
    case nod_post:		operator = blr_post; break;
    case nod_uppercase:		operator = blr_upcase; break;
    case nod_sleuth:		operator = blr_matching2; break;
    /*case nod_substr:		operator = blr_substring; break;*/

    default:
	DDL_err (96, NULL, NULL, NULL, NULL, NULL); /* msg 96: GENERATE_blr: node not supported */
	return;
    }

/* If the user has given us something that has the form

       field {EQ} NULL
             {NE}

   transform it into

       field [NOT] MISSING */

if ((operator == blr_eql || operator == blr_neq) &&
    (arg [0]->nod_type == nod_null || arg [1]->nod_type == nod_null))
    {
    if (operator == blr_neq)
	{
	CHECK_BLR(1);
	STUFF (blr_not);
	}
    operator = blr_missing;
    if (arg [0]->nod_type == nod_null)
	arg++;
    end = arg + 1;
    }

/* Fall thru on reasonable stuff */

CHECK_BLR(1);
STUFF (operator);
for (; arg < end; arg++)
    generate (blr, *arg);
}

static void get_set_generator (
    STR		blr,
    NOD		node)
{
/**************************************
 *
 *	g e t _ s e t _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	produce blr for set_generator. The
 *	mutable characteristics are the
 *	generator name and the increment.
 *	The rest is canned.
 **************************************/
SCHAR  *p;
SYM    symbol;
static SCHAR    gen_prologue [] = {
blr_begin, blr_message, 0, 1,0, blr_long, 0, 
blr_send, 0, blr_begin, blr_assignment, blr_gen_id,}; 
static SCHAR    gen_epilogue [] = {
blr_parameter, 0, 0,0, blr_end, blr_end};
int    l;
CON	constant;
SLONG	value;

/* copy the beginning of the blr into the buffer */

l = sizeof (gen_prologue);
p = gen_prologue;
CHECK_BLR(l);
do STUFF (*p++); while (--l);

/* stuff in the name length and the name */

symbol = (SYM) node->nod_arg [1];
CHECK_BLR(1);
STUFF (l = symbol->sym_length);
p = (SCHAR*) symbol->sym_string;
if (l)
    {
    CHECK_BLR(l);
    do STUFF (*p++); while (--l);
    }

/* now for the increvent value */

CHECK_BLR(7);
STUFF (blr_literal);
constant = (CON) node->nod_arg [0]->nod_arg[0];
STUFF (blr_long);
STUFF (constant->con_desc.dsc_scale);
p = (SCHAR*) constant->con_data;
value = *(SLONG*) p;
STUFF_WORD (value);
STUFF_WORD (value >> 16);

/* complete the string */
l = sizeof (gen_epilogue);
p = (SCHAR*) gen_epilogue;
CHECK_BLR(l);
do STUFF (*p++); while (--l);
}
