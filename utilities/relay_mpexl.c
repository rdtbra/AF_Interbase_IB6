/*
 *	PROGRAM:	MPE/XL signal relay program
 *	MODULE:		relay_mpexl.c
 *	DESCRIPTION:	Signal relay program
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
#include <mpe.h>
#include "../jrd/common.h"
#include "../jrd/mpexl.h"
#include "../jrd/license.h"
#include "../jrd/isc_signal.h"
#include "../jrd/aif_proto.h"
#include "../jrd/event_proto.h"
#include "../jrd/isc_proto.h"
#include "../isc_lock/lock_proto.h"

/* Block types */

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;

#define KILL_COMMAND	2
#define STOP_COMMAND	4

static void	looper (void);
static BOOLEAN	process_utility (USHORT);
static void	signal_utility (USHORT);

/* Signal data record */

typedef union sdr {
    SLONG	sdr_to_pin;
    USHORT	sdr_utility;
    SLONG	sdr_port;
} SDR;

static SLONG	sync_p;

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
 *	Wait on a pipe for a message, then forward a signal.
 *
 **************************************/
UCHAR	**end, *p;
USHORT	parm, sw_utility, sw_version;
STATUS	status_vector [20];
SLONG	status;

sw_utility = sw_version = FALSE;
end = argv + argc;
while (++argv < end)
    if (**argv == '-')
	for (p = *argv + 1; *p; p++)
	    switch (UPPER (*p))
		{
		case 'K':
		    sw_utility |= KILL_COMMAND;
		    break;

		case 'S':
		    sw_utility |= STOP_COMMAND;
		    break;

		case 'Z':
		    sw_version = TRUE;
		    break;

		default:
		    ib_fprintf (ib_stderr, "gdsrelay: ignoring unknown switch \"%c\"\n", *p);
		}

if (sw_version)
    ib_fprintf (ib_stderr, "gdsrelay version %s\n", GDS_VERSION);

if (sw_utility)
    {
    /* Signal the relay process to execute a utility command */

    signal_utility (sw_utility);
    exit (FINI_OK);
    }

/* Open up the mapped and message files for the lock manager,
   central server, and events */

status_vector [1] = 0;
if (LOCK_init (status_vector, FALSE))
    p = "lock manager";
/****
else if (!CSS_init (status_vector, TRUE))
    p = "central server";
****/
else if (!EVENT_init (status_vector, TRUE))
    p = "events";
if (status_vector [1])
    {
    ib_fprintf (ib_stderr, "error: Unable to initialize %s files.\n", p);
    gds__print_status (status_vector);
    exit (FINI_ERROR);
    }

/* Open the global port and wait for a shutdown message */

if (!(status = ISC_open_port (0, 0, TRUE, FALSE, &sync_p)))
    {
    looper();

    /* Close the port and exit */

    AIF_close_port (sync_p);
    }
else
    {
    ib_fprintf (ib_stderr, "error: AIFPORTOPEN error code %ld. This job must be aborted to shut it down.\n", status >> 16);
    for (;;)
	sleep (300);
    }

exit (FINI_OK);
}

static void looper (void)
{
/**************************************
 *
 *	l o o p e r
 *
 **************************************
 *
 * Functional description
 *	Receive and re-transmit messages.
 *
 **************************************/
SLONG	status, itemnum [2], ^item [2];
SLONG	msg_len, signal, wait;
SLONG	port;
SDR	sync_data;

for (;;)
    {
    itemnum [0] = 11002;
    item [0] = &wait;
    itemnum [1] = 0;

    wait = 300;
    msg_len = sizeof (sync_data);

    status = AIF_receive_port (sync_p, &signal, &sync_data, &msg_len, itemnum, item);

    if (status)
	{
	status >>= 16;
	if (status == -11028 || status == -111)
	    continue;
	else
	    {
	    ib_fprintf (ib_stderr, "error: AIFPORTRECEIVE error code %ld. Shutting down.\n", status);
	    return;
	    }
	}

    switch (signal)
	{
	case UTILITY_SIGNAL:
	    if (process_utility (sync_data.sdr_utility))
		return;
	}
    }
}

static BOOLEAN process_utility (
    USHORT	cmd)
{
/**************************************
 *
 *	p r o c e s s _ u t i l i t y
 *
 **************************************
 *
 * Functional description
 *	Process a utility message.
 *
 **************************************/

return TRUE;
}

static void signal_utility (
    USHORT	sw_utility)
{
/**************************************
 *
 *	s i g n a l _ u t i l i t y
 *
 **************************************
 *
 * Functional description
 *	Signal the relay process to execute a utility command.
 *
 **************************************/
SLONG	status, port, wait, itemnum [2], ^item [2];
TEXT	*p;

/* Open the global port and send a message */

if (!(status = ISC_open_port (0, 0, FALSE, FALSE, &port)))
    {
    itemnum [0] = 11101;
    item [0] = &wait;
    itemnum [1] = 0;

    wait = -1;

    status = AIF_send_port (port, (SLONG) UTILITY_SIGNAL, &sw_utility, sizeof (sw_utility), itemnum, item);
    AIF_close_port (port);
    p = "AIFPORTSEND";
    }
else
    p = "AIFPORTOPEN";

if (status)
    ib_fprintf (ib_stderr, "error: %s error %ld while trying to communicate with the gdsrelay process.\n",
	p, status >> 16);
}
