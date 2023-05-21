/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		hsh.c
 *	DESCRIPTION:	Hash table and symbol manager
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

#include "../gpre/gpre.h"
#include "../gpre/parse.h"
#ifdef JPN_SJIS
#include "../jrd/kanji.h"
#endif
#include "../gpre/hsh_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/msc_proto.h"

static int	hash (register SCHAR *);
static BOOLEAN	scompare (register SCHAR *, register SCHAR *);
static BOOLEAN	scompare2 (register SCHAR *, register SCHAR *);

#define HASH_SIZE 211

static SYM hash_table [HASH_SIZE];
static SYM key_symbols;

static struct word {
    SCHAR		*keyword;
    enum kwwords	id;
} keywords [] = {
#include "../gpre/hsh.h"
};

#define NUMWORDS (sizeof (keywords) / sizeof (struct word))

void HSH_fini (void)
{
/**************************************
 *
 *	H S H _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Release space used by keywords.
 *
 **************************************/
register SYM	symbol;

while (key_symbols)
    {
    symbol = key_symbols;
    key_symbols = (SYM) key_symbols->sym_object;
    HSH_remove (symbol);
    FREE (symbol);
    }
}

void HSH_init (void)
{
/**************************************
 *
 *	H S H _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize the hash table.  This mostly involves
 *	inserting all known keywords.
 *
 **************************************/
register SCHAR		*string;
register SYM		symbol, *ptr;
SSHORT			i;
register struct word	*word;

for (ptr = hash_table, i = 0; i < HASH_SIZE; i++)
    *ptr++ = NULL;

for (i = 0, word = keywords; i < NUMWORDS; i++, word++)
    {
    for (string = word->keyword; *string; string++)
	;
    symbol = (SYM) ALLOC (SYM_LEN);
    symbol->sym_type = SYM_keyword;
    symbol->sym_string = word->keyword;
    symbol->sym_keyword = (int) word->id;
    HSH_insert (symbol);
    symbol->sym_object = (CTX) key_symbols;
    key_symbols = symbol;
    }
 }

void HSH_insert (
    register SYM	symbol)
{
/**************************************
 *
 *	H S H _ i n s e r t
 *
 **************************************
 *
 * Functional description
 *	Insert a symbol into the hash table.
 *
 **************************************/
register int	h;
register SYM	*next;
SYM	ptr;
    
h = hash (symbol->sym_string);

for (next = &hash_table [h]; *next; next = &(*next)->sym_collision)
    {
    for (ptr = *next; ptr; ptr = ptr->sym_homonym)
	if (ptr == symbol)
	    return;

    if (scompare (symbol->sym_string, (*next)->sym_string))
	{
	/* insert in most recently seen order; 
	   This is important for alias resolution in subqueries.
	   BUT insert tokens AFTER KEYWORD!
	   In a lookup, keyword should be found first.
	   This assumes that KEYWORDS are inserted before any other token.
	   No one should be using a keywords as an alias anyway. */

	if ((*next)->sym_type == SYM_keyword)
	    {
	    symbol->sym_homonym = (*next)->sym_homonym;
	    symbol->sym_collision = NULL;
	    (*next)->sym_homonym = symbol;
	    }
	else
	    {
	    symbol->sym_homonym = *next;
	    symbol->sym_collision = (*next)->sym_collision;
	    (*next)->sym_collision = NULL;
	    *next = symbol;
	    }
	return;
	}      
   } 

symbol->sym_collision = hash_table [h];
hash_table [h] = symbol;
}

SYM HSH_lookup (
    register SCHAR	*string)
{
/**************************************
 *
 *	H S H _ l o o k u p
 *
 **************************************
 *
 * Functional description
 *	Perform a string lookup against hash table.
 *
 **************************************/
register SYM	symbol;
    
for (symbol = hash_table [hash (string)]; symbol;
     symbol = symbol->sym_collision)
    if (scompare (string, symbol->sym_string))
	return symbol;

return NULL;
}
SYM HSH_lookup2 (
    register SCHAR	*string)
{
/**************************************
 *
 *	H S H _ l o o k u p 2
 *
 **************************************
 *
 * Functional description
 *	Perform a string lookup against hash table.
 *      Calls scompare2 which performs case insensitive
 *	compare.
 *
 **************************************/
register SYM	symbol;
    
for (symbol = hash_table [hash (string)]; symbol;
     symbol = symbol->sym_collision)
    if (scompare2 (string, symbol->sym_string))
	return symbol;

return NULL;
}

void HSH_remove (
    register SYM	symbol)
{
/**************************************
 *
 *	H S H _ r e m o v e 
 *
 **************************************
 *
 * Functional description
 *	Remove a symbol from the hash table.
 *
 **************************************/
int		h;
register SYM	*next, *ptr, homonym;
    
h = hash (symbol->sym_string);

for (next = &hash_table [h]; *next; next = &(*next)->sym_collision)
    if (symbol == *next)
	if (homonym = symbol->sym_homonym)
	    {
	    homonym->sym_collision = symbol->sym_collision;
	    *next = homonym;
	    return;
	    }
	else
	    {
	    *next = symbol->sym_collision;
	    return;
	    }
    else
	for (ptr = &(*next)->sym_homonym; *ptr; ptr = &(*ptr)->sym_homonym)
	    if (symbol == *ptr)
		{
		*ptr = symbol->sym_homonym;
		return;
		}

CPR_error ("HSH_remove failed");
}

static hash (
    register SCHAR	*string)
{
/**************************************
 *
 *	h a s h
 *
 **************************************
 *
 * Functional description
 *	Returns the hash function of a string.
 *
 **************************************/
register SLONG	value;
SCHAR		c;
    
value = 0;

while (c = *string++)
    value = (value << 1) + UPPER(c);

return ((value >= 0) ? value : - value) % HASH_SIZE;
}

static BOOLEAN scompare (
    register SCHAR	*string1,
    register SCHAR	*string2)
{
/**************************************
 *
 *	s c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	case sensitive Compare 
 *
 **************************************/
SCHAR	c1, c2;

while (*string1)
    if (*string1++ != *string2++)
	return FALSE;    
    
if (*string2)
    return FALSE;

return TRUE;
}

static BOOLEAN scompare2 (
    register SCHAR	*string1,
    register SCHAR	*string2)
{
/**************************************
 *
 *	s c o m p a r e 2
 *	 Case insensitive compare
 *
 **************************************
 *
 * Functional description
 *	Compare two strings
 *
 **************************************/
SCHAR	c1, c2;

while (c1 = *string1++)
#ifndef JPN_SJIS
	if (!(c2 = *string2++) || (UPPER (c1) != UPPER (c2)))
	    return FALSE;
#else 
	{
	/* Do not upcase second byte of a sjis kanji character */

	if (!(c2 = *string2++) || (UPPER (c1) != UPPER (c2)))
	    return FALSE;

	if (SJIS1(c1) && (c1 = *string1++))
	    if (!(c2 = *string2++) || c1 != c2)
		return FALSE;
	}
#endif
    
if (*string2)
    return FALSE;

return TRUE;
}
