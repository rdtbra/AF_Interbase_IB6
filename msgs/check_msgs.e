/*
 *	PROGRAM:	Interbase Message Facility
 *	MODULE:		check_messages.e
 *	DESCRIPTION:	Check whether any messages have been updated
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
#include <stdlib.h>
#include <string.h>
#ifdef VMS
#include <types.h>
#include <stat.h>
#else
#include <sys/types.h>
#ifndef mpexl
#include <sys/stat.h>
#endif
#endif
#include <time.h>

#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/gds_proto.h"

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"

struct stat {
    struct tm	st_mtime;
};

static int		fstat (int, struct stat *);
static struct tm	*localtime (struct tm *);
#endif

DATABASE DB = "msg.gdb";

#ifndef mpexl
#define INCLUDE_INDICATOR	"indicator.incl"
#define MESSAGE_INDICATOR	"indicator.msg"
#define LOCALE_INDICATOR	"indicator.loc"
#else
#define INCLUDE_INDICATOR	"indincl.bin"
#define MESSAGE_INDICATOR	"indmsg.bin"
#define LOCALE_INDICATOR	"indloc.bin"
#endif

int CLIB_ROUTINE main (
    int		argc,
    char	*argv [])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Top level routine.  
 *
 **************************************/
TEXT		*p, **end_args, db_file [256];
USHORT		flag_jrd, flag_msg, flag_loc, sw_bad, do_locales;
IB_FILE		*ind_jrd, *ind_msg, *ind_loc;
ISC_TIMESTAMP	date_jrd, date_msg, date_loc;
struct stat	file_stat;

strcpy (db_file, "msg.gdb");
do_locales = FALSE;

end_args = argv + argc;

for (++argv; argv < end_args;)
    {
    p = *argv++;
    sw_bad = FALSE;
    if (*p != '-')
	sw_bad = TRUE;
    else
	switch (UPPER (p [1]))
	    {
	    case 'D':
		strcpy (db_file, *argv++);
		break;
	    
	    case 'L':
		do_locales = TRUE;
		break;

	    default:
		sw_bad = TRUE;
	    }
    if (sw_bad)
	{
	ib_printf ("Invalid option \"%s\".  Valid options are:\n", p);
	ib_printf ("\t-D\tDatabase name\n");
	ib_printf ("\t-L\tCheck all locales\n");
	exit (FINI_ERROR);
	}
    }

flag_jrd = flag_msg = TRUE;

if (ind_jrd = ib_fopen (INCLUDE_INDICATOR, "r"))
    {
    if (!fstat (ib_fileno (ind_jrd), &file_stat))
	{
	gds__encode_date (localtime (&file_stat.st_mtime), &date_jrd);
	flag_jrd = FALSE;
	}
    ib_fclose (ind_jrd);
    }

if (ind_msg = ib_fopen (MESSAGE_INDICATOR, "r"))
    {
    if (!fstat (ib_fileno (ind_msg), &file_stat))
	{
	gds__encode_date (localtime (&file_stat.st_mtime), &date_msg);
	flag_msg = FALSE;
	}
    ib_fclose (ind_msg);
    }

if (do_locales)
    {
    flag_loc = TRUE;
    if (ind_loc = ib_fopen (LOCALE_INDICATOR, "r"))
   	{
    	if (!fstat (ib_fileno (ind_loc), &file_stat))
    	    {
    	    gds__encode_date (localtime (&file_stat.st_mtime), &date_loc);
    	    flag_loc = FALSE;
    	    }
/* Earlier the following ib_fclose was closing ind_msg again, due to which 
   check_messages was crashing with SEGV signal - */
    	ib_fclose (ind_loc); /* A bug in check_messages is fixed */
    	}
    }

READY db_file AS DB;
START_TRANSACTION;

if (!flag_jrd)
    {
    FOR FIRST 1 X IN FACILITIES WITH
	X.FACILITY EQ "JRD" AND X.LAST_CHANGE GE date_jrd
	flag_jrd = TRUE;
    END_FOR;
    }

if (!flag_msg)
    {
    FOR FIRST 1 X IN FACILITIES WITH
	X.LAST_CHANGE GE date_msg
	flag_msg = TRUE;
    END_FOR;
    }

if (!flag_loc)
    {
    FOR FIRST 1 T IN TRANSMSGS WITH 
	T.TRANS_DATE GE date_loc
   	flag_loc = TRUE;
    END_FOR;
    }

COMMIT;
FINISH;

if (flag_jrd)
    if (ind_jrd = ib_fopen (INCLUDE_INDICATOR, "w"))
	{
	ib_fputc (' ', ind_jrd);
	ib_fclose (ind_jrd);
	}

if (flag_msg)
    if (ind_msg = ib_fopen (MESSAGE_INDICATOR, "w"))
	{
	ib_fputc (' ', ind_msg);
	ib_fclose (ind_msg);
	}

if (flag_loc)
    if (ind_loc = ib_fopen (LOCALE_INDICATOR, "w"))
	{
	ib_fputc (' ', ind_loc);
	ib_fclose (ind_loc);
	}

exit (FINI_OK);
}

#ifdef mpexl
static int fstat (
    int		file_desc,
    struct stat *file_stat)
{
/**************************************
 *
 *	f s t a t
 *
 **************************************
 *
 * Functional description
 *	Determine when a file was last modified.
 *
 **************************************/
struct {
    unsigned long	hour : 8;
    unsigned long	minute : 8;
    unsigned long	second : 8;
    unsigned long	tenths : 8;
}	mod_time;
SSHORT	mod_date, daterror [2], yearnum, monthnum, daynum, weekdaynum;

FFILEINFO (_mpe_fileno (file_desc), 52, &mod_time, 53, &mod_date, 0);
if (ccode() != CCE)
    return -1;

file_stat->st_mtime.tm_sec = mod_time.second;
file_stat->st_mtime.tm_min = mod_time.minute;
file_stat->st_mtime.tm_hour = mod_time.hour;
ALMANAC (mod_date, daterror, &yearnum, &monthnum, &daynum, &weekdaynum);
file_stat->st_mtime.tm_mday = daynum;
file_stat->st_mtime.tm_mon = monthnum - 1;
file_stat->st_mtime.tm_year = yearnum;

return 0;
}
#endif

#ifdef mpexl
static struct tm *localtime (
    struct tm	*time)
{
/**************************************
 *
 *	l o c a l t i m e
 *
 **************************************
 *
 * Functional description
 *	A dummy routine for MPE/XL
 *
 **************************************/

return time;
}
#endif
