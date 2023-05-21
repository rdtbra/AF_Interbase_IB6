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

#include <stdio.h>
#include <string.h>

typedef int	(*FUN_PTR)();

typedef struct {
    char	*fn_module;
    char	*fn_entrypoint;
    FUN_PTR	fn_function;
} FN;

static test();
extern int nroff_filter();

static FN	isc_functions [] = {
    "test_module", "test_function", test,
    "FILTERLIB", "nroff_filter", (FUN_PTR) nroff_filter,
    0, 0, 0};

#ifdef SHLIB_DEFS
#define strcmp		(*_libfun_strcmp)
#define sprintf		(*_libfun_sprintf)

extern int		strcmp();
extern int		sprintf();
#endif

FUN_PTR FUNCTIONS_entrypoint (module, entrypoint)
    char	*module;
    char	*entrypoint;
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



static test (n, result)
    int		n;
    char	*result;
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
}
