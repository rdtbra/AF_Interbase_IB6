/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		divorce.c
 *	DESCRIPTION:	Divorce process from controlling terminal
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

#ifdef SUPERSERVER
#define FD_SETSIZE 256
#endif /* SUPERSERVER */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include "../jrd/common.h"

#ifndef NOFILE
#define NOFILE		20
#endif

#ifndef NBBY
#define	NBBY	8
#endif

#ifndef NFDBITS
#define NFDBITS		(sizeof(SLONG) * NBBY)
#ifndef NETWARE_386
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((SCHAR *)(p), sizeof(*(p)))
#endif
#endif

void divorce_terminal (
    fd_set	*mask)
{
/**************************************
 *
 *	d i v o r c e _ t e r m i n a l
 *
 **************************************
 *
 * Functional description
 *	Clean up everything in preparation to become an independent
 *	process.  Close all files except for marked by the input mask.
 *
 **************************************/
int	s, fid;

/* Close all files other than those explicitly requested to stay open */

for (fid = 0; fid < NOFILE; fid++)
    if (!(FD_ISSET(fid, mask)))
	close (fid);

#ifdef SIGTTOU
   /* ignore all the teminal related signal if define */
   signal (SIGTTOU, SIG_IGN);
   signal (SIGTTIN, SIG_IGN);
   signal (SIGTSTP, SIG_IGN);
#endif


/* Perform terminal divorce */

fid = open ("/dev/tty", 2);

if (fid >= 0)
    {
#ifdef TIOCNOTTY
    ioctl (fid, TIOCNOTTY, 0);
#endif
    close (fid);
    }

/* Finally, get out of the process group */

#ifndef NETWARE_386
setpgrp (0, 0);
#endif
}
