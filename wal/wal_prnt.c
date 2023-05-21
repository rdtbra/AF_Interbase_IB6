/*
 *        PROGRAM:        JRD Write Ahead Log APIs
 *        MODULE:         wal_prnt.c
 *        DESCRIPTION:    Main routine for printing WAL segment status
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

#include <stdlib.h>
#include <string.h>
#include "../wal/wal.h"  
#include "../wal/wal_proto.h"  
#include "../wal/wstat_proto.h"  
#include "../jrd/gds_proto.h"  
#include "../jrd/isc_f_proto.h"  

#if (defined WIN_NT || defined OS2_ONLY)
#include <io.h>
#endif

#ifdef NETWARE_386
typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id_mod;
    USHORT	blk_length;
} *BLK;

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"
#define exit(code)	{service->svc_handle = 0;                           \
                                                                            \
                    /* Mark service thread as finished. */                  \
		    /* If service is detached, cleanup memory being used    \
		       by service. */                                       \
		    SVC_finish (service, SVC_finished);			    \
                                                                            \
                    return (code);}

#define FPRINTF		SVC_netware_fprintf
#endif

#ifndef FPRINTF
#define FPRINTF 	ib_fprintf
#endif

#ifdef NETWARE_386
static SVC	outfile;
#else
static IB_FILE	*outfile;
#endif


#ifdef NETWARE_386

int main_wal_print (
    SVC service)

#else /* Non-Netware declaration */

int CLIB_ROUTINE main (
    int		argc,
    SCHAR	**argv)

#endif
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/	
STATUS	status_vector [20];
WAL	WAL_handle;
SCHAR	dbname [256];
int	ret;
SLONG	redir_in, redir_out, redir_err;
#ifdef NETWARE_386
int		argc;
char		**argv;
#endif

#ifdef NETWARE_386
argc = service->svc_argc;
argv = service->svc_argv;
#endif

#ifdef NETWARE_386
outfile = service;
#else
outfile = ib_stdout;
#endif

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
    }
#ifndef NETWARE_386
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
#endif

if (argc < 2)
    {
    FPRINTF (outfile, "\nUsage: gds_wal_print <database_name>\n");
	/* this message was considered for translation but left for now   */ 
	/* since it is likely only to be seen by DBA's and technical type */
    exit (FINI_ERROR);
    }

ISC_expand_filename (argv [1], 0, dbname);

WAL_handle = NULL;
ret = WAL_attach (status_vector, &WAL_handle, dbname);
if (ret != SUCCESS)
    {
#ifdef NETWARE_386
    FPRINTF (outfile, "Either no current attachment or no WAL configured for %s\n", dbname);
#else
    gds__print_status (status_vector);
#endif
    exit (FINI_ERROR);
    }

WSTATUS_dump_wal_segment (WAL_handle, FALSE, outfile);
WAL_fini (status_vector, &WAL_handle); 

exit (FINI_OK);
}
