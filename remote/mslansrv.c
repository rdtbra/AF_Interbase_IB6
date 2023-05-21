/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		mslansrv.c
 *	DESCRIPTION:	Microsoft LanManager remote server.
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

/* we can't #define INCL_BASE, because with nmpipe.h 2 function prototypes of several
   named pipe functions will be brought in, and errors will be generated.  So we have
   to define all the constants except one of the ones that would have been defined by
   INCL_BASE.
*/

#define INCL_SUB
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSDEVICES
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSSIGNALS
#define INCL_DOSMONITORS
#define INCL_DOSPROFILE
#define INCL_DOSPROCESS
#define INCL_DOSINFOSEG
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSMODULEMGR
#define INCL_DOSRESOURCES
#define INCL_DOSTRACE
#define INCL_DOSMEMMGR
#include <os2.h>

#include "../jrd/ib_stdio.h"
#include "../remote/remote.h"
#include "../remote/lanman.h"
#include "../jrd/license.h"
#include "../remote/mslan_proto.h"
#include "../remote/serve_proto.h"
#include "../jrd/gds_proto.h"

void main (
    int		argc,
    char	*argv[])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Run the server with MSLanManager named pipes.
 *
 **************************************/
STATUS	status_vector [20];
PORT	port;
int	debug, multi_thread;
TEXT	*p, c;
SLONG	debug_value;

argv++;
debug = multi_thread = FALSE;

while (--argc)
    {
    p = *argv++;
    if (*p++ = '-')
	while (c = *p++)
	    switch (UPPER (c))
		{
		case 'D':
		    debug = TRUE;
		    break;

		case 'M':
		    multi_thread = TRUE;
		    break;
		    
		case 'Z':
		    ib_printf ("Interbase MSLanManager server version %s\n", GDS_VERSION);
		    exit (FINI_OK);
      
		default:
		    ib_printf ("Invalid switch \"%c\"\m", c);
		    return;
		}
    }

ib_printf ("Interbase MSLanManager Server active\n");

port = MSLAN_connect (NULL_PTR, NULL_PTR, status_vector, 0);

if (!port)
    {
    gds__print_status (status_vector);
    exit (FINI_ERROR);
    }

if (multi_thread)
    SRVR_multi_thread (port, 1);
else
    SRVR_main (port, 1);

exit (FINI_OK);
}
