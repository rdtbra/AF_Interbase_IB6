/*
 *	PROGRAM:	JRD Shared Cache Manager
 *	MODULE:		cache_manager.c
 *	DESCRIPTION:	Manage shared database cache
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
#include <signal.h>
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/license.h"
#include "../jrd/gds_proto.h"
#include "../jrd/why_proto.h"

#if (defined WIN_NT || defined OS2_ONLY)
#include <io.h>
#endif

static UCHAR	cache_dpb [] = { gds__dpb_version1, gds__dpb_cache_manager };

int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Run the shared cache manager.
 *
 **************************************/
TEXT	*sw_database, c, *p, **end;
SLONG	db_handle;
STATUS	status, status_vector [20];
SLONG	redir_in, redir_out, redir_err;

#ifdef VMS
argc = VMS_parse (&argv, argc);
#endif

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
    }
else if (argc > 4 && !strcmp (argv [1], "-svc_re"))
    {
    redir_in = atol (argv [2]);
    redir_out = atol (argv [3]);
    redir_err = atol (argv [4]);
#ifdef WIN_NT
    redir_in = _open_osfhandle (redir_in, 0);
    redir_out = _open_osfhandle (redir_out, 0);
    redir_err = _open_osfhandle (redir_err, 0);
#endif
    if (redir_in != 0)
	if (dup2 ((int) redir_in, 0))
	    close ((int) redir_in);
    if (redir_out != 1)
	if (dup2 ((int) redir_out, 1))
	    close ((int) redir_out);
    if (redir_err != 2)
	if (dup2 ((int) redir_err, 2))
	    close ((int) redir_err);
    argv += 4;
    argc -= 4;
    }

#ifdef UNIX
if (setreuid (0, 0) < 0)
    ib_printf ("Shared cache manager: couldn't set uid to superuser\n");

SETPGRP;

signal (SIGINT, SIG_IGN);
signal (SIGQUIT, SIG_IGN);
#endif

sw_database = "";

for (end = argv++ + argc; argv < end;)
    {
    p = *argv++;
    if (*p != '-')
	sw_database = p;
    else
	{
	while (c = *++p)
	    switch (UPPER (c))
		{
		case 'D':	
		    sw_database = *argv++;
		    break;

		case 'Z':
		    ib_printf ("Shared cache manager version %s\n", GDS_VERSION);
		    exit (FINI_OK);
		}
	}
    }

gds__disable_subsystem ("REMINT");
gds__disable_subsystem ("CSI");
gds__disable_subsystem ("GDSSHR5");
gds__disable_subsystem ("PIPE5");

do {
    db_handle = NULL;
    status = gds__attach_database (status_vector,
		0,
		GDS_VAL (sw_database),
		GDS_REF (db_handle),
		sizeof (cache_dpb),
		GDS_VAL (cache_dpb));

    if (status && status != gds__cache_restart)
    	{
    	gds__print_status (status_vector);
    	gds__log_status (sw_database, status_vector);
    	}

    if (db_handle)
    	gds__detach_database (status_vector, GDS_REF (db_handle));

} while (status == gds__cache_restart);

exit (status ? FINI_ERROR : FINI_OK);
}
