/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		msc.c
 *	DESCRIPTION:	Miscellaneous little stuff
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

/***************************************************
   THIS MODULE HAS SEVERAL KISSING COUSINS; IF YOU
   SHOULD CHANGE ONE OF THE MODULES IN THE FOLLOWING
   LIST, PLEASE BE SURE TO CHECK THE OTHERS FOR
   SIMILAR CHANGES:

        /gds/maint/pyxis/all.c
                  /dsql/all.c
                  /jrd/all.c
                  /pipe/allp.c
                  /qli/all.c
                  /remote/allr.c
                  /gpre/msc.c

   - THANK YOU
***************************************************/

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../gpre/gpre.h"
#include "../gpre/parse.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/msc_proto.h"
#include "../jrd/gds_proto.h"

extern ACT	cur_routine;

typedef struct spc {
    struct spc	*spc_next;
    SLONG	spc_remaining;
} *SPC;

static SPC	space, permanent_space;
static LLS	free_lls;

ACT MSC_action (
    REQ 	request,
    enum act_t	type)
{
/**************************************
 *
 *	M S C _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Make an action and link it to a request.
 *
 **************************************/
ACT	action;

action = (ACT) ALLOC (ACT_LEN);
action->act_type = type;

if (request)
    {
    action->act_next = request->req_actions;
    request->req_actions = action;
    action->act_request = request;
    }

return action;
}

UCHAR *MSC_alloc (
    int	size)
{
/**************************************
 *
 *	M S C _ a l l o c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SPC	next;
int	n;
UCHAR	*blk, *p, *end;

size = ALIGN (size, ALIGNMENT);

if (!space || size > space->spc_remaining)
    {
    n = MAX (size, 4096);
    if (!(next = (SPC) gds__alloc ((SLONG) (n + sizeof (struct spc)))))
	CPR_error ("virtual memory exhausted");
#ifdef DEBUG_GDS_ALLOC
    /* For V4.0 we don't care about gpre specific memory leaks */
    gds_alloc_flag_unfreed (next);
#endif
    next->spc_next = space;
    next->spc_remaining = n;
    space = next;
    }

space->spc_remaining -= size;
p = blk = ((UCHAR*) space + sizeof (struct spc) + space->spc_remaining);
end = p + size;

while (p < end)
    *p++ = 0;

return blk;
}

UCHAR *MSC_alloc_permanent (
    int	size)
{
/**************************************
 *
 *	M S C _ a l l o c _ p e r m a n e n t
 *
 **************************************
 *
 * Functional description
 *	Allocate a block in permanent memory.
 *
 **************************************/
SPC	next;
int	n;
UCHAR	*blk, *p, *end;

size = ALIGN (size, ALIGNMENT);

if (!permanent_space || size > permanent_space->spc_remaining)
    {
    n = MAX (size, 4096);
    if (!(next = (SPC) gds__alloc ((SLONG) (n + sizeof (struct spc)))))
	CPR_error ("virtual memory exhausted");
#ifdef DEBUG_GDS_ALLOC
    /* For V4.0 we don't care about gpre specific memory leaks */
    gds_alloc_flag_unfreed (next);
#endif
    next->spc_next = permanent_space;
    next->spc_remaining = n;
    permanent_space = next;
    }

permanent_space->spc_remaining -= size;
p = blk = ((UCHAR*) permanent_space + sizeof (struct spc) + permanent_space->spc_remaining);
end = p + size;

while (p < end)
    *p++ = 0;

return blk;
}

NOD MSC_binary (
    NOD_T	type,
    NOD		arg1,
    NOD		arg2)
{
/**************************************
 *
 *	M S C _ b i n a r y
 *
 **************************************
 *
 * Functional description
 *	Make a binary node.
 *
 **************************************/
NOD	node;

node = MSC_node (type, 2);
node->nod_arg [0] = arg1;
node->nod_arg [1] = arg2;

return node;
}

CTX MSC_context (
    REQ	request)
{
/**************************************
 *
 *	M S C _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Make a new context for a request and link it up to the request.
 *
 **************************************/
CTX	context;

/* allocate and initialize */

context = (CTX) ALLOC (CTX_LEN);
context->ctx_request = request;
context->ctx_internal = request->req_internal++;
context->ctx_scope_level = request->req_scope_level;

/* link in with the request block */

context->ctx_next = request->req_contexts;
request->req_contexts = context;

return context;
}

void MSC_copy (
    SCHAR	*from,
    int		length,
    SCHAR	*to)
{
/**************************************
 *
 *	M S C _ c o p y
 *
 **************************************
 *
 * Functional description
 *	Copy one string into another.
 *
 **************************************/

if (length)
    do *to++ = *from++; while (--length);

*to = 0;
}

SYM MSC_find_symbol (
    SYM		symbol,
    enum sym_t	type)
{
/**************************************
 *
 *	M S C _ f i n d _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Find a symbol of a particular type.
 *
 **************************************/

for (; symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == type)
	return symbol;

return NULL;
}

void MSC_free (
    SCHAR	*block)
{
/**************************************
 *
 *	M S C _ f r e e
 *
 **************************************
 *
 * Functional description
 *	Free a block.
 *
 **************************************/

}

void MSC_free_request (
    REQ		request)
{
/**************************************
 *
 *	M S C _ f r e e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Get rid of an erroroneously allocated request block.
 *
 **************************************/

requests = request->req_next;
cur_routine->act_object = (REF) request->req_routine;
MSC_free (request);
}

void MSC_init (void)
{
/**************************************
 *
 *	M S C _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize (or more properly, re-initialize) the memory
 *	allocator.
 *
 **************************************/
SPC	stuff;

free_lls = NULL;

while (stuff = space)
    {
    space = space->spc_next;
    gds__free (stuff);
    }
}

BOOLEAN MSC_match (
    KWWORDS	keyword)
{
/**************************************
 *
 *	M S C _ m a t c h
 *
 **************************************
 *
 * Functional description
 *	Match the current token against a keyword.  If successful,
 *	advance the token stream and return true.  Otherwise return
 *	false.
 *
 **************************************/

if (token.tok_keyword == KW_none && token.tok_symbol)
    {
    SYM sym;
    for (sym=token.tok_symbol->sym_collision; sym; sym=sym->sym_collision)
	if ( (strcmp(sym->sym_string, token.tok_string)==0) &&
	     sym->sym_keyword != KW_none)
	    { 
	    token.tok_symbol = sym; 
	    token.tok_keyword = sym->sym_keyword; 
	    }
    }

if ((int) token.tok_keyword != (int) keyword)
    return FALSE;

CPR_token();

return TRUE;
}

BOOLEAN MSC_member (
    NOD		object,
    LLS		stack)
{
/**************************************
 *
 *	M S C _ m e m b e r
 *
 **************************************
 *
 * Functional description
 *	Determinate where a specific object is
 *	represented on a linked list stack.
 *
 **************************************/

for (; stack; stack = stack->lls_next)
    if (stack->lls_object == object)
	return TRUE;

return FALSE;
}

NOD MSC_node (
    enum nod_t	type,
    SSHORT	count)
{
/**************************************
 *
 *	M S C _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Allocate an initialize a syntax node.
 *
 **************************************/
NOD	node;

node = (NOD) ALLOC (NOD_LEN(count));
node->nod_count = count;
node->nod_type = type;

return node;
}

NOD MSC_pop (
    LLS		*pointer)
{
/**************************************
 *
 *	M S C _ p o p
 *
 **************************************
 *
 * Functional description
 *	Pop an item off a linked list stack.  Free the stack node.
 *
 **************************************/
LLS	stack;
NOD	node;

stack = *pointer;
node = stack->lls_object;
*pointer = stack->lls_next;

stack->lls_next = free_lls;
free_lls = stack;

return node;
}

PRV MSC_privilege_block (void)
{
/*************************************
 *
 *      M S C _ p r i v i l e g e _ b l o c k
 *
 *************************************
 *
 * Functional description
 *      Allocate a new privilege (grant/revoke) block.
 *
 *************************************/
PRV     privilege_block;

privilege_block = (PRV) ALLOC (PRV_LEN);
privilege_block->prv_privileges = PRV_no_privs;
privilege_block->prv_username = 0;
privilege_block->prv_relation = 0;
privilege_block->prv_fields = 0;
privilege_block->prv_next = 0;

return privilege_block;
}

void MSC_push (
    NOD		object,
    LLS		*pointer)
{
/**************************************
 *
 *	M S C _ p u s h
 *
 **************************************
 *
 * Functional description
 *	Push an arbitrary object onto a linked list stack.
 *
 **************************************/
LLS	stack;

if (stack = free_lls)
    free_lls = stack->lls_next;
else
    stack = (LLS) ALLOC (LLS_LEN);

stack->lls_object = object;
stack->lls_next = *pointer;
*pointer = stack;
}

REF MSC_reference (
    REF		*link)
{
/**************************************
 *
 *	M S C _ r e f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	Generate a reference and possibly link the reference into
 *	a linked list.
 *
 **************************************/
REF	reference;

reference = (REF) ALLOC (REF_LEN);

if (link)
    {
    reference->ref_next = *link;
    *link = reference;
    }

return reference;
}

REQ MSC_request (
    enum req_t	type)
{
/**************************************
 *
 *	M S C _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Set up for a new request.  Make request and action
 *	blocks, all linked up and ready to go.
 *
 **************************************/
REQ	request;

request = (REQ) ALLOC (REQ_LEN);
request->req_type = type;
request->req_next = requests;
requests = request;

request->req_routine = (REQ) cur_routine->act_object;
cur_routine->act_object = (REF) request;

if (!(cur_routine->act_flags & ACT_main))
    request->req_flags |= REQ_local;
if (sw_sql_dialect <= SQL_DIALECT_V5)
    request->req_flags |= REQ_blr_version4;

return request;
}

SCHAR *MSC_string (
    TEXT	*input)
{
/**************************************
 *
 *	M S C _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Copy a string into a permanent block.
 *
 **************************************/
TEXT	*string;

string = (TEXT*) ALLOC (strlen (input) + 1);
strcpy (string, input);

return string;
}

SYM MSC_symbol (
    enum sym_t	type,
    TEXT	*string,
    USHORT	length,
    CTX		object)
{
/**************************************
 *
 *	M S C _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Allocate and initialize a symbol block.
 *
 **************************************/
SYM	symbol;
TEXT	*p;

symbol = (SYM) ALLOC (SYM_LEN + length);
symbol->sym_type = type;
symbol->sym_object = object;
p = symbol->sym_string = symbol->sym_name;

if (length)
    do *p++ = *string++; while (--length);

return symbol;
}

NOD MSC_unary (
    NOD_T	type,
    NOD		arg)
{
/**************************************
 *
 *	M S C _ u n a r y
 *
 **************************************
 *
 * Functional description
 *	Make a unary node.
 *
 **************************************/
NOD	node;

node = MSC_node (type, 1);
node->nod_arg [0] = arg;

return node;
}

USN MSC_username (
    SCHAR    *name,
    USHORT    name_dyn)
{
/**************************************
 *
 *	M S C _ u s e r n a m e
 *
 **************************************
 *
 * Functional description
 *	Set up for a new username.
 *
 **************************************/
USN	username;

username = (USN) MSC_alloc (USN_LEN);
username->usn_name = (SCHAR *) MSC_alloc (strlen (name) + 1);
strcpy (username->usn_name, name);
username->usn_dyn = name_dyn;

username->usn_next = 0;

return username;
}

