/*
 *	PROGRAM:	Database file mapper
 *	MODULE:		mapper.c
 *	DESCRIPTION:	Ties down a database file to a node
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

#ifdef SR95
#include "/sys/ins/base.ins.c"
#include "/sys/ins/ms.ins.c"
#include "/sys/ins/error.ins.c"
#include "/sys/ins/proc2.ins.c"
#define SR10_REF(a) a
#define SR10_VAL(a) *a
#else
#include <apollo/base.h>
#include <apollo/ms.h>
#include <apollo/error.h>
#include <apollo/proc2.h>
#define SR10_REF(a) &a
#define SR10_VAL(a) a
#endif

#ifdef __STDC__
main (
    int		argc,
    char	*argv[])
#else
main (argc, argv)
    int		argc;
    char	*argv[];
#endif
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Parse one or more databases from
 *	the command line, lock them for
 *	co-writing, and go to sleep.
 *	This has the effect of locking the
 *	file on a specific node so that 
 *	database attaches will always go
 *	through a server on that node.
 *
 **************************************/
status_$t	apollo_status;
unsigned long	mapped_length, window_length, offset;
short		filename_length;
char		*filename;

offset = 0;
window_length = 32768;

/* get the filename from the command line */

argv++;
if (!--argc)
    {
    ib_printf ("Please retry, specifying one or more databases.\n");
    return;
    }

while (argc--)
    {
    filename = *argv;
    filename_length = strlen (filename);

    /* map the file for co-write access */

    ms_$mapl (
        SR10_VAL (filename), 
        filename_length, 
        offset, 
        window_length, 
        ms_$cowriters, 
        ms_$wr, 
        (SSHORT) -1,
        SR10_REF (mapped_length), 
        SR10_REF (apollo_status));

    if (apollo_status.all != status_$ok)
        {
        error_$print (apollo_status);
        return;
        }
    }

name_process ("mapper");

/* sleep forever */

for (;;)
    sleep (200000000);
}  

#ifdef __STDC__
static void name_process (
    char	*name)
#else
static void name_process (name)
    char	*name;
#endif
{
/**************************************
 *
 *	n a m e _ p r o c e s s
 *
 **************************************
 *
 * Functional description
 *	Name ourselves.
 *
 **************************************/
uid_$t		uid;
short		length;
status_$t	status;

proc2_$who_am_i (SR10_REF (uid));
length = strlen (name);
pm_$set_name (name, &length, &uid, &status);
}
