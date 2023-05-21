/*
 *	PROGRAM:	InterBase Access Method
 *	MODULE:		qatest.c
 *	DESCRIPTION:	Entry points for QA testibility library
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

/*

QA Testability Module

Rich Damon in QA requested the addition of a system UDF that would
enable automated tested of certain error conditions.  Previously
this was done with a user UDF, but with NLM no longer supporting
user UDF's we are including it in the system.

I have therefore extended the BUILTIN_entrypoint() method to include
a system UDF entry, QATEST_entrypoint, which will do various 
nasty things to the system when invoked.  

-------------
THIS FUNCTION IS NOT TO BE DOCUMENTED OR USED BY CUSTOMERS
-------------

To use the UDF, it must be declared to a database, thusly:

    declare external function QAPOKE 
	    int, int 
        returns int by value
        entry_point "XYZZY_PLUGH_PLOVER!" 
        module_name "INTERBASE_INTERNAL_QA!";

Note the special magic values for entry_point & module_name.  Any
number of parameters can be declared as part of the UDF definition and
will be passed to the entrypoint - the first parameter selects the 
subfunction to invoke.

Currently defined subfunctions, and expected parameters, are:

   0	QATEST_testing (SLONG)
   
	Tests that the mechanism is working properly.
  	returns 2*arg1;

   1	QATEST_delete_database (void)		
   
	Deletes the current database file.
	Returns 0 on success
	Returns -1 if database doesn't have a file (when can this happen?)

   2	QATEST_delete_shadow (ULONG)		

	Deletes shadow #arg1.
	Returns	0 on success
	Returns -1 if shadowing not enabled
	Returns -2 if shadow #arg1 not found.

 999	QATEST_exit (void)

	Performs exit() on the server.
	Note that this will crash an NLM server - and is not suitable
	for fully automated testing on that platform.


Code for the UDF is in jrd/qatest.c.

The R&D TCS test case is QATEST_METHOD, which exercises all the 
defined APIs for this function.

1994-July-14 David Schnepper
*/



#include "../jrd/ib_stdio.h"
#include <stdlib.h>
#include <string.h>
#include "../jrd/common.h"
#include "../jrd/jrd.h"
#include "../jrd/sdw.h"
#include "../jrd/pio.h"
#include "../jrd/codes.h"
#include "../jrd/err_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/sch_proto.h"
#ifdef WIN_NT
#include <windows.h>
#define TEXT	char
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#define QATEST_testing		0
#define QATEST_delete_database	1
#define QATEST_delete_shadow	2
#define QATEST_exit		999


int	 QATEST_entrypoint (
    ULONG	*function,
    void	*arg1,
    void	*arg2,
    void	*arg3)
{
/**************************************
 *
 *	Q A T E S T _ e n t r y p o i n t
 *
 **************************************
 *
 * Functional description
 *
 *	Perform various random nastiness to the system in order
 *	to test database recovery.
 *
 *	These entrypoints are *NOT* designed for customer use!
 *
 **************************************/
TDBB		tdbb;
UCHAR		filename [MAXPATHLEN];
SDW		shadow;
#ifdef WIN_NT
HANDLE          desc; 
#endif
FIL             file;


switch (*function)
    {
    case QATEST_testing:
	/* Parameter 1: SLONG *testvalue */
	/* Entrypoint for testing the QA entrypoint method */

	return 2* *(SLONG *) arg1;

    case QATEST_delete_database:
	/* Parameters: NONE */
	/* Close current database file & delete */

	tdbb = GET_THREAD_DATA;
	if (!(file = tdbb->tdbb_attachment->att_database->dbb_file))
	    return -1;

#ifdef WIN_NT

	desc = (HANDLE ) (( file->fil_flags & FIL_force_write ) ? file->fil_force_write_desc : file->fil_desc);   

	CloseHandle (desc);
	desc = INVALID_HANDLE_VALUE;
#else
	close (file->fil_desc);
#endif
	strncpy ( (char *) filename, file->fil_string, file->fil_length);
        filename[file->fil_length] = 0;

#ifdef WIN_NT 
         DeleteFile(filename); 
#else
	unlink (filename);
#endif
	return 0;

    case QATEST_delete_shadow:
	/* Parameter 1: ULONG *shadow_number */
	/* Close & delete specified shadow file */

	tdbb = GET_THREAD_DATA;
	if (!(shadow = tdbb->tdbb_attachment->att_database->dbb_shadow))
	    return -1;
	for (;shadow; shadow = shadow->sdw_next)
	    if (shadow->sdw_number == *(ULONG *) arg1)
		{
#ifdef WIN_NT

                desc = (HANDLE) (( shadow->sdw_file->fil_flags & FIL_force_write ) ? shadow->sdw_file->fil_force_write_desc : shadow->sdw_file->fil_desc);

		CloseHandle (desc);
		desc = INVALID_HANDLE_VALUE;
#else
		close (shadow->sdw_file->fil_desc);
#endif
		strncpy ( (char *) filename, shadow->sdw_file->fil_string,
	                   shadow->sdw_file->fil_length);
                filename[shadow->sdw_file->fil_length] = 0;
#ifdef WIN_NT
                DeleteFile(filename);
#else
		unlink (filename);
#endif
		return 0;
		}
	return -2;

    case QATEST_exit:
	/* Parameters: NONE */
	/* do a process exit - very nasty - crashes MU server */

	exit (1);
	return 0;

    default:
	sprintf (filename, "Unknown QATEST_entrypoint #%lu",  /* TXNN */
		*function);
	THREAD_ENTER;
	ERR_post (gds__random, 
		  gds_arg_string, ERR_cstring ( (TEXT *) filename), 0);
	THREAD_EXIT;
	return 0;
    }
}
