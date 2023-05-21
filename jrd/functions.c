/*
 *	PROGRAM:	InterBase Access Method
 *	MODULE:		functions.c
 *	DESCRIPTION:	External entrypoint definitions
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

/* defined in common.h, which is included by ib_stdio.h: typedef int	(*FPTR_INT)(); */

typedef struct {
    char	*fn_module;
    char	*fn_entrypoint;
    FPTR_INT	fn_function;
} FN;

#ifdef __STDC__
FPTR_INT	FUNCTIONS_entrypoint (char *, char *);
static int	test (long, char *);
#else
FPTR_INT	FUNCTIONS_entrypoint();
static int	test();
#endif

static FN	isc_functions [] = {
    "test_module", "test_function", test,
    0, 0, 0};

#ifdef SHLIB_DEFS
#define strcmp		(*_libfun_strcmp)
#define sprintf		(*_libfun_sprintf)

extern int		strcmp();
extern int		sprintf();
#endif

#ifdef __STDC__
FPTR_INT FUNCTIONS_entrypoint (
    char	*module,
    char	*entrypoint)
#else
FPTR_INT FUNCTIONS_entrypoint (module, entrypoint)
    char	*module;
    char	*entrypoint;
#endif
{
/**************************************
 *
 *	F U N C T I O N S _ e n t r y p o i n t
 *
 **************************************
 *
 * Functional description
 *	Lookup function in hardcoded table.  The module and
 *	entrypoint names are null terminated, but may contain
 *	insignificant trailing blanks.
 *
 **************************************/
FN	*function;
char	*p, temp [128], *ep;

p = temp;

while (*module && *module != ' ')
    *p++ = *module++;

*p++ = 0;
ep = p;

while (*entrypoint && *entrypoint != ' ')
    *p++ = *entrypoint++;

*p = 0;

for (function = isc_functions; function->fn_module; ++function)
    if (!strcmp (temp, function->fn_module) && !strcmp (ep, function->fn_entrypoint))
	return function->fn_function;

return 0;
}

#ifdef __STDC__
static int test (
    long	n,
    char	*result)
#else
static int test (n, result)
    long	n;
    char	*result;
#endif
{
/**************************************
 *
 *	t e s t
 *
 **************************************
 *
 * Functional description
 *	Sample extern function.  Defined in database by:
 *
 *	define function test module_name "test_module" entry_point "test_function"
 *	    long by value,
 *	    char [20] by reference return_argument;
 *
 **************************************/
char	*end;

sprintf (result, "%d is a number", n);
end = result + 20;

while (*result)
    result++;

while (result < end)
    *result++ = ' ';

return 0;
}
