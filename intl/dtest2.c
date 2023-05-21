/*
 *	Test tool for Language Drivers.
 *
 *	This tool loads up a language driver using the dynamic link
 *	(or shared object) method and interrogates it's ID function.
 *	This tool is used to quickly verify the dynamic load ability
 *	of a newly created language driver.
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

static int	full_debug = 0;
#define	FULL_DEBUG	if (full_debug) ib_printf


#define DEBUG_INTL
#include "../jrd/intl.c"


#ifdef VMS
	char	*defaults[] = {
		"<null>",
		"ask", "ask", "ask", "ask", "ask", "ask", "ask"
	};
#endif


try( c, f )
char	*c;
FUN_PTR	f;
{
	unsigned char	buffer[200];
	int	res;
	int	i;
	res = (*f)( strlen( c ), c, sizeof( buffer ),  buffer );
	ib_printf( "%s => ", c );
	for (i = 0; i < res; i++)
	  ib_printf( "%d ", buffer[i] );
	ib_printf( "\n" );
};

my_err()
{
	ib_printf( "Error routine called!\n" );
};

main(argc, argv)
int	argc;
char	*argv[];
{
	struct	texttype	this_textobj;
	char	**vector;
	int	i;

#ifdef VMS
	vector = defaults;
 	argc = sizeof( defaults ) / sizeof( char *);
#else
	if (argc <= 1) {
		ib_printf( "usage: dtest Intl_module_name\n" );
		return( 1 );
	};
	vector = argv;
#endif

	for (i = 1; i < argc; i++) {

	int	t_type;
	t_type = atoi( vector[ i ] );
	INTL_fn_init( t_type, &this_textobj, my_err );
	};
	return( 0 );
}
