/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_signal.h
 *	DESCRIPTION:	InterBase pseudo-signal definitions
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

#ifndef _JRD_ISC_SIGNAL_H_
#define _JRD_ISC_SIGNAL_H_

#ifdef NeXT
/* These pseudo-signals cause messages to be sent over ports
   managed by a port watcher thread in lieu of UNIX signal delivery.
   The only requirement is that signal definitions be greater than
   the highest numbered UNIX signal. This value is usually defined
   as NSIG in <sys/signal.h>. */

#define WAKEUP_SIGNAL		11111	/* Lock manager wakeup signal */
#define BLOCKING_SIGNAL		11112	/* Lock manager blocking signal */
#define EVENT_SIGNAL		11113	/* Event manager signal */
#endif

#ifdef OS2_ONLY
/* Define dummy signal values on OS/2. */

#define BLOCKING_SIGNAL		0	/* Lock manager */
#define EVENT_SIGNAL		0	/* Event manager */
#define CACHE_SIGNALS		0	/* Shared cache */
#define WAL_SIGNALS		0	/* Write ahead log */
#endif

#ifdef WIN_NT
/* There is no interprocess signaling on Windows NT.  We simulate it
   by sending messages through named pipes. */

#define BLOCKING_SIGNAL		1000	/* Lock manager */
#define WAKEUP_SIGNAL		1100	/* Lock manager */
#define EVENT_SIGNAL		1200	/* Event manager */
#define CACHE_SIGNALS		1300	/* Shared cache */
#define WAL_SIGNALS		1400	/* Write ahead log */
#endif

#ifdef mpexl
/* There is no signaling on MPE XL.  We simulate it by sending messages
   to AIF ports.  Pseudo-signals are divided into types: synchronous
   and asynchrous.  Synchronous signals have values >= 32. */

#define BLOCKING_SIGNAL		1
#define WAKEUP_SIGNAL		32
#define EVENT_WAIT_SIGNAL	35
#define UTILITY_SIGNAL		-1

#ifndef SERVER_SIGNALS
#define EVENT_SIGNAL		2
#else
#ifndef PIPE_SERVER
#define CSV_SIGNAL		33
#define EVENT_SIGNAL		3
#else
#define CSV_SIGNAL		34
#define EVENT_SIGNAL		4
#endif
#endif
#endif

#endif /* _JRD_ISC_SIGNAL_H_ */
