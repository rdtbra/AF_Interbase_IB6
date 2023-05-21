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

typedef unsigned char	*(*FUN_PTR)();

typedef struct {
    char	*fn_module;
    char	*fn_entrypoint;
    FUN_PTR	fn_function;
} FN;

extern unsigned char	*c_convert_865_to_latin1();
extern unsigned char	*c_convert_437_to_latin1();
extern unsigned char	*c_convert_437_to_865();
extern unsigned char	*c_convert_latin1_to_865();
extern unsigned char	*c_convert_latin1_to_437();
extern unsigned char	*c_convert_865_to_437();
extern unsigned char	*n_convert_865_to_latin1();
extern unsigned char	*n_convert_437_to_latin1();
extern unsigned char	*n_convert_437_to_865();
extern unsigned char	*n_convert_latin1_to_865();
extern unsigned char	*n_convert_latin1_to_437();
extern unsigned char	*n_convert_865_to_437();

static FN	isc_functions [] = {
	"convert.o", "c_convert_865_to_latin1",	c_convert_865_to_latin1,
	"convert.o", "c_convert_437_to_latin1", c_convert_437_to_latin1,
	"convert.o", "c_convert_437_to_865", 	c_convert_437_to_865,
	"convert.o", "c_convert_latin1_to_865", c_convert_latin1_to_865,
	"convert.o", "c_convert_latin1_to_437", c_convert_latin1_to_437,
	"convert.o", "c_convert_865_to_437", 	c_convert_865_to_437,
	"convert.o", "n_convert_865_to_latin1",	n_convert_865_to_latin1,
	"convert.o", "n_convert_437_to_latin1",	n_convert_437_to_latin1,
	"convert.o", "n_convert_437_to_865",	n_convert_437_to_865,
	"convert.o", "n_convert_latin1_to_865",	n_convert_latin1_to_865,
	"convert.o", "n_convert_latin1_to_437",	n_convert_latin1_to_437,
	"convert.o", "n_convert_865_to_437",	n_convert_865_to_437,
	 0, 0, 0};

#ifdef SHLIB_DEFS
#define strcmp		(*_libfun_strcmp)

extern int		strcmp();
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
char	*p, temp [64], *ep;

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
