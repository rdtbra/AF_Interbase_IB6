/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		ambx_server.c
 *	DESCRIPTION:	Apollo mailbox server.
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
#include "/sys/ins/proc2.ins.c"
#define SR10_REF(a) a
#define SR10_VAL(a) *a
#else
#include <apollo/base.h>
#include <apollo/proc2.h>
#define SR10_REF(a) &a
#define SR10_VAL(a) a
#endif

#include "../jrd/common.h"
#include "../jrd/license.h"
#include "../remote/remote.h"
#include "../remote/ambx_proto.h"
#include "../remote/serve_proto.h"

static void	name_process (UCHAR *);

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
 *	Run the server with apollo mailboxes.
 *
 **************************************/
int		n, child, *port, debug, multi_thread, status_vector [20];
SCHAR		*p, c;
uid_$t		uid;
USHORT		length;
status_$t	status;
SLONG		debug_value;

argv++;
multi_thread = TRUE;
debug = FALSE;

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

		case 'S':
		    multi_thread = FALSE;
		    break;

		case 'Z':
		    ib_printf ("Interbase mailbox server version %s\n", GDS_VERSION);
		    exit (FINI_OK);			
		}
    }

/* Fork off a server, wait for it to die, then fork off another */

if (!debug)
    for (n = 0; n < 100; n++)
	{
	if (!(child = fork()))
	    break;
	name_process ("gds_guardian");
	while (wait (0) != child)
	    ;
	gds__log ("AMBX_SERVER/main: gds_server restarted");
	}

name_process ("gds_server");

port = AMBX_connect (0, NULL_PTR, status_vector, 0);

if (!port)
    {
    gds__print_status (status_vector);
    exit (FINI_ERROR);
    }

if (multi_thread)
    SRVR_multi_thread (port, SRVR_multi_client | SRVR_server);
else
    SRVR_main (port, SRVR_multi_client | SRVR_server);

exit (FINI_OK);
}

static void name_process (
    UCHAR	*name)
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
USHORT		length;
status_$t	status;

proc2_$who_am_i (SR10_REF (uid));
length = strlen (name);
pm_$set_name (name, &length, &uid, &status);
}
