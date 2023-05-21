/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_ipc.c
 *	DESCRIPTION:	General purpose but non-user routines.
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

#ifdef SHLIB_DEFS
#define LOCAL_SHLIB_DEFS
#endif

#include "../jrd/ib_stdio.h"
#include <stdlib.h>
#include "../jrd/common.h"
#include "../jrd/codes.h"
#include "../jrd/isc.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_i_proto.h"
#include "../jrd/isc_s_proto.h"

#ifdef sparc
#ifdef SOLARIS
#define HANDLER_ADDR_ARG
#else
#include <vfork.h>
#endif
#endif

typedef struct sig {
    struct sig	*sig_next;
    int		sig_signal;
    void	(*sig_routine)();
    void	*sig_arg;
    SLONG	sig_count;
    IPTR	sig_thread_id;
    USHORT	sig_flags;
} *SIG;

#define SIG_client	1		/* Not our routine */
#define SIG_informs	2		/* routine tells us whether to chain */

#define SIG_informs_continue	0	/* continue on signal processing */
#define SIG_informs_stop	1	/* stop signal processing */

#ifdef apollo
typedef int	(* CLIB_ROUTINE SIG_FPTR)();
#else
#ifdef SUN3_3
typedef int	(* CLIB_ROUTINE SIG_FPTR)();
#else
#if (defined OS2_ONLY && defined __BORLANDC__)
typedef void	(CLIB_ROUTINE * SIG_FPTR)();
#else
typedef void	(* CLIB_ROUTINE SIG_FPTR)();
#endif
#endif
#endif

#ifdef DGUX
#define GT_32_SIGNALS
#endif
#if (defined AIX || defined AIX_PPC)
#define GT_32_SIGNALS
#endif
#ifdef M88K
#define GT_32_SIGNALS
#define HANDLER_ADDR_ARG
#endif
#ifdef EPSON
#define HANDLER_ADDR_ARG
#endif
#ifdef UNIXWARE
#define HANDLER_ADDR_ARG
#endif
#ifdef NCR3000
#define HANDLER_ADDR_ARG
#endif
#ifdef SCO_EV
#define HANDLER_ADDR_ARG
#endif

#ifndef REQUESTER
static USHORT	initialized_signals = FALSE;
static SIG	VOLATILE signals = NULL;
static USHORT	VOLATILE inhibit_count = 0;
static SLONG	VOLATILE overflow_count = 0;

#ifdef MULTI_THREAD
static MUTX_T   sig_mutex;
#endif

#ifdef GT_32_SIGNALS
static SLONG	VOLATILE pending_signals [2];
#else
static SLONG	VOLATILE pending_signals = 0;
#endif
static int	process_id = 0;

#endif /* of ifndef REQUESTER */

/* VMS Specific Stuff */

#ifdef VMS

#include <signal.h>
#endif

/* Unix specific stuff */

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <errno.h>
#ifndef NeXT
#include <unistd.h>
#endif

#ifndef O_RDWR
#include <fcntl.h>
#endif

#ifdef DGUX
#include <fcntl.h>
#endif

#ifdef NeXT
#include <mach/cthreads.h>
#include <mach/mach.h>
#include <servers/netname.h>

#define MAX_PORTS	20

typedef struct port {
    struct port	*port_next;
    int		port_pid;
    port_t	port_name;
    ULONG	port_age;
} *PORT;

static PORT	ports;
static USHORT	port_enabled;
static ULONG	port_clock;
#endif

#define LOCAL_SEMAPHORES 4

#ifdef DELTA
#include <sys/sysmacros.h>
#include <sys/param.h>
#endif

#ifdef IMP
typedef int		pid_t;

#define SHMEM_DELTA	(1 << 25)
#endif

#ifdef SYSV_SIGNALS
#define SIGVEC		FPTR_INT
#endif

#ifdef SIGACTION_SUPPORTED
#define SIGVEC		struct sigaction
#endif

#ifdef APOLLO
#define GDS_RELAY	"/com/gds_relay"
#endif

#ifndef GDS_RELAY
#define GDS_RELAY	"/bin/gds_relay"
#endif

#ifndef SHMEM_DELTA
#define SHMEM_DELTA	(1 << 22)
#endif

static int		VOLATILE relay_pipe = 0;
#endif

/* Windows NT */

#ifdef WIN_NT

#include <process.h>
#include <signal.h>
#include <windows.h>
#define TEXT		SCHAR

#ifndef NSIG
#define NSIG		100
#endif

#define SIGVEC		SIG_FPTR

#define MAX_OPN_EVENTS	40

typedef struct opn_event {
    SLONG	opn_event_pid;
    SLONG	opn_event_signal;	/* pseudo-signal number */
    HANDLE	opn_event_lhandle;	/* local handle to foreign event */
    ULONG	opn_event_age;
} *OPN_EVENT;

static struct opn_event	opn_events [MAX_OPN_EVENTS];
static USHORT		opn_event_count;
static ULONG		opn_event_clock;
#endif

/* OS2 Junk */

#ifdef OS2_ONLY

#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <process.h>
#include <signal.h>

#define SIGVEC		SIG_FPTR

typedef struct sigcontext {
    void	*dummy;
};

static HEV		existence_event;
#endif

/* NLM stuff */

#ifdef NETWARE_386
#define SIGVEC		SIG_FPTR
#endif

/* PC_PLATFORM stuff */

#if (defined PC_PLATFORM && !defined NETWARE_386)
#include <signal.h>

#define SIGVEC		SIG_FPTR
#endif

/* HP MPE/XL specific stuff */

#ifdef mpexl

#include <fcntl.h>
#include <mpe.h>
#include "../jrd/mpexl.h"

#define SIGVEC		SLONG
#define PENDING_OFFSET	10
#define MIN_SYNC_SIGNAL	32

static SLONG	comm_async;

#define SIGFPE	(1 << 4) | (1 << 1) | (1 << 16) | (1 << 5) | (1 << 3) |\
		(1 << 15) | (1 << 6) | (1 << 2) | (1 << 17) | (1 << 7) | (1 << 0)

static SLONG	sync_signals;
static SIGVEC	client_enable_mask;
static SIGVEC	client_trap_mask;
static USHORT	initialized_traps;
static SLONG	enter_count;
#endif

/* Apollo specific Stuff */

#ifdef APOLLO


#include <errno.h>
#include <apollo/base.h>
#include <apollo/pm.h>
#include <apollo/mutex.h>
#include <apollo/task.h>
#include <apollo/loader.h>
#include <apollo/ec2.h>
#include <apollo/ms.h>
#include <apollo/name.h>
#include <apollo/error.h>
#include <sys/signal.h>
#endif

static void	cleanup (void *);
static void	error (STATUS *, TEXT *, STATUS);
static void 	isc_signal2 (int, FPTR_VOID, void *, ULONG);
static SLONG 	overflow_handler (void *);
static SIG	que_signal (int, FPTR_VOID, void *, int);
#if !(defined mpexl || defined HANDLER_ADDR_ARG)
static void CLIB_ROUTINE signal_handler (int, int, struct sigcontext *);
#endif
#ifdef HANDLER_ADDR_ARG
static void CLIB_ROUTINE signal_handler (int, int, void *, void *);
#endif
#ifdef NeXT
static void	error_mach (STATUS *, TEXT *, kern_return_t);
static BOOLEAN	port_initialize (void);
static BOOLEAN	port_kill (int, int);
static void	port_watcher (port_all_t);
#endif
#ifdef mpexl
static void	cleanup_communications (void *);
static void CLIB_ROUTINE signal_handler (SLONG);
static SLONG	signal_process (SLONG);
#endif
#ifdef OLD_POSIX_THREADS
static void	sigwait_thread (int);
#endif

#ifndef sigvector
#ifndef hpux
#define sigvector	sigvec
#endif
#endif

#ifndef SIGVEC
#define SIGVEC		struct sigvec
#endif

#ifndef SIG_HOLD
#define SIG_HOLD	SIG_DFL
#endif

/* Not thread-safe */
ULONG			isc_enter_count = 0;

#if !(defined mpexl || defined NETWARE_386 || defined OS2_ONLY)
static SIGVEC		client_sigfpe;
#else
static SIG_FPTR		client_sigfpe = NULL;
#endif

#ifdef SHLIB_DEFS
#define sprintf		(*_libgds_sprintf)
#define strlen		(*_libgds_strlen)
#define strcpy		(*_libgds_strcpy)
#define exit		(*_libgds_exit)
#define _iob		(*_libgds__iob)
#define getpid		(*_libgds_getpid)
#define errno		(*_libgds_errno)
#define kill		(*_libgds_kill)
#define _exit           (*_libgds__exit)
#define pipe		(*_libgds_pipe)
#define fork		(*_libgds_fork)
#define write		(*_libgds_write)
#define _ctype		(*_libgds__ctype)
#define sigvector	(*_libgds_sigvec)
#define execl		(*_libgds_execl)
#define sigset		(*_libgds_sigset)
#define ib_fprintf		(*_libgds_fprintf)
#define close		(*_libgds_close)

extern int		sprintf();
extern int		strlen();
extern SCHAR		*strcpy();
extern void		exit();
extern IB_FILE		_iob [];
extern pid_t		getpid();
extern int		errno;
extern int		kill();
extern void             _exit();
extern int		pipe();
extern pid_t		fork();
extern int		write();
extern SCHAR		_ctype [];
extern int		sigvector();
extern int		execl();
extern void		(*sigset())();
extern int		ib_fprintf();
extern int		close();
#endif

#ifdef mpexl
int ISC_comm_init (
    STATUS	*status_vector,
    int		init_flag)
{
/**************************************
 *
 *	I S C _ c o m m _ i n i t		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT	*err_rout;
SLONG	process_pid, status, port, wait, priority, itemnum [3], ^item [3];

if (comm_async || init_flag)
    return TRUE;

/* Get the pid (different from the pin). */

AIF_get_pid (0, 0, &process_pid);

/* Open a port that can be used to receive asynchronous signals */

process_id = getpid();
if (status = ISC_open_port (process_id, process_pid, TRUE, TRUE, &port))
    err_rout = "AIFPORTOPEN";
else
    comm_async = port;

if (status)
    {
    /* Something failed.  Shutdown whatever we started. */

    AIF_close_port (comm_async);
    comm_async = 0;
    error (status_vector, err_rout, status);
    return FALSE;
    }

if (!init_flag)
    gds__register_cleanup (cleanup_communications, NULL_PTR);

return TRUE;
}
#endif

void DLL_EXPORT ISC_enter (void)
{
/**************************************
 *
 *	I S C _ e n t e r
 *
 **************************************
 *
 * Functional description
 *	Enter ISC world from caller.
 *
 **************************************/
#ifdef NETWARE_386
#define ISC_ENTER
#endif

#ifdef mpexl
#define ISC_ENTER
/* On MPE/XL, enabling traps seems to be an extremely expensive operation.
   Therefore, if we find that the caller has established a handler, we will
   enable and arm traps only once.  When we do so, we will save the previous
   handler.  Thereafter, if our handler is called and we are not in our code,
   we will call the saved handler. */

if (!initialized_traps)
    {
    HPENBLTRAP (0, &client_enable_mask);
    internal_sigfpe = client_enable_mask | SIGFPE;
    HPENBLTRAP (internal_sigfpe, &internal_sigfpe);
    internal_sigfpe = client_enable_mask | SIGFPE;
    XARITRAP (internal_sigfpe, (int) trap_handler, &client_trap_mask, (int*) &client_sigfpe);
    initialized_traps = (client_sigfpe != (SIG_FPTR) NULL);
    }
enter_count++;
#endif

#ifndef ISC_ENTER
/* Cancel our handler for SIGFPE - in case it was already there */
ISC_signal_cancel (SIGFPE, overflow_handler, NULL);

/* Setup overflow handler - with chaining to any user handler */
isc_signal2 (SIGFPE, overflow_handler, NULL, SIG_informs);
#endif

#ifdef DEBUG_FPE_HANDLING
/* Debug code to simulate an FPE occuring during DB Operation */
if (overflow_count < 100)
    (void) kill (getpid(), SIGFPE);
#endif
}

#ifndef REQUESTER
void DLL_EXPORT ISC_enable (void)
{
/**************************************
 *
 *	I S C _ e n a b l e
 *
 **************************************
 *
 * Functional description
 *	Enable signal processing.  Re-post any pending signals.
 *
 **************************************/
USHORT	n;
#ifdef GT_32_SIGNALS
SLONG	p;
USHORT	i;
#endif

if (inhibit_count)
    --inhibit_count;

if (inhibit_count)
    return;

#ifdef UNIX
#ifdef GT_32_SIGNALS
while (pending_signals [0] || pending_signals [1])
    for (i = 0; i < 2; i++)
	{
	for (n = 0, p = pending_signals [i]; p && n < 32; n++)
	    if (p & (1 << n))
		{
		p &= ~(1 << n);
		ISC_kill (process_id, n + 1 + i*32);
		}
	/* This looks like a danger point - if one of the bits
	 * was reset after we sent the signal then we will lose it.
	 */
	pending_signals [i] = 0;
	}
#else
while (pending_signals)
    for (n = 0; pending_signals && n < 32; n++)
	if (pending_signals & (1 << n))
	    {
	    pending_signals &= ~(1 << n);
	    ISC_kill (process_id, n + 1);
	    }
#endif
#endif

#ifdef mpexl
while (pending_signals)
    for (n = 0; pending_signals && n < 32; n++)
	if (pending_signals & (1 << n))
	    {
	    pending_signals &= ~(1 << n);
	    ISC_wait (n+1, 0);
	    }
#endif
}
#endif

void DLL_EXPORT ISC_exit (void)
{
/**************************************
 *
 *	I S C _ e x i t
 *
 **************************************
 *
 * Functional description
 *	Exit ISC world, return to caller.
 *
 **************************************/

#ifdef NETWARE_386
#define ISC_EXIT
#endif

#ifdef mpexl
#define ISC_EXIT
{
SLONG    dummy;
enter_count--;
if (!initialized_traps)
    {
    HPENBLTRAP (client_enable_mask, &dummy);
    XARITRAP (client_trap_mask, (int) client_sigfpe, &dummy, &dummy);
    }
}
#endif

#ifndef ISC_EXIT
/* No longer attempt to handle overflow internally */
ISC_signal_cancel (SIGFPE, overflow_handler, NULL_PTR);
#endif 
}

#ifdef mpexl
int ISC_get_port (
    int		flag)
{
/**************************************
 *
 *	I S C _ g e t _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Return the handle of a port.
 *
 **************************************/

return (flag) ? comm_async : comm_async;
}
#endif

#ifndef REQUESTER
void DLL_EXPORT ISC_inhibit (void)
{
/**************************************
 *
 *	I S C _ i n h i b i t
 *
 **************************************
 *
 * Functional description
 *	Inhibit process of signals.  Signals will be
 *	retained until signals are eventually re-enabled,
 *	then re-posted.
 *
 **************************************/

++inhibit_count;
}
#endif

#if (defined APOLLO || (defined VMS && defined __ALPHA))
int ISC_kill (
    SLONG	pid,
    SLONG	signal_number)
{
/**************************************
 *
 *	I S C _ k i l l		( A p o l l o  &  A l p h a / O p e n V M S )
 *
 **************************************
 *
 * Functional description
 *	Notify somebody else.
 *
 **************************************/

return kill (pid, signal_number);
}
#endif

#ifdef mpexl
int ISC_kill (
    SLONG	pid,
    SSHORT	type,
    SLONG	port)
{
/**************************************
 *
 *	I S C _ k i l l		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Wake another MPE/XL process.
 *
 **************************************/
SLONG	wait, priority, itemnum [3], ^item [3];
SCHAR	connectionless;
SLONG	status;

if (!pid)
    pid = process_id;

itemnum [0] = 11101;
item [0] = &wait;
itemnum [1] = 11103;
item [1] = &connectionless;
itemnum [2] = 0;

wait = -1;
priority = 1;
connectionless = 1;

if (!port)
    port = comm_async;

status = AIF_send_port (port, (SLONG) type, &pid, sizeof (pid), itemnum, item);

return 0;
}
#endif

#ifdef NETWARE_386
int ISC_kill (
    SLONG	pid,
    SLONG	signal_number)
{
/**************************************
 *
 *	I S C _ k i l l		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Notify somebody else.
 *
 **************************************/

return 0;
}
#endif

#ifdef OS2_ONLY
int API_ROUTINE ISC_kill (
    SLONG	pid,
    ULONG	object_hndl)
{
/**************************************
 *
 *	I S C _ k i l l		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Notify somebody else.
 *
 **************************************/
ULONG	status;

if (!(status = DosOpenEventSem (NULL, &object_hndl)))
    {
    DosPostEventSem (object_hndl);
    DosCloseEventSem (object_hndl);
    }

return status;
}
#endif

#ifdef UNIX
int ISC_kill (
    SLONG	pid,
    SLONG	signal_number)
{
/**************************************
 *
 *	I S C _ k i l l		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Notify somebody else.
 *
 **************************************/
SLONG	msg [3];
int	status, pipes [2];
TEXT	process [64], arg [10];

#ifdef NeXT
/* If not a UNIX signal, send to port watcher */

if (signal_number > NSIG)
    {
    if (!port_kill (pid, signal_number))
	return 0;

    status = kill (pid, 0);
    if (!status || errno == ESRCH)
	return status;
    return 0;
    }
#endif

for (;;)
    {
    status = kill (pid, signal_number);

    if (!status)
	return status;
    if (SYSCALL_INTERRUPTED(errno))
	continue;
    if (errno == EPERM)
	break;

    return status;
    }

/* Process is there, but we don't have the privilege to
   send to him.  */

if (!relay_pipe)
    {
    gds__prefix (process, GDS_RELAY);
    if (pipe (pipes))
	{
        gds__log ("ISC_kill: error %d creating gds_relay", errno);
	return -1;
	}
    sprintf (arg, "%d", pipes [0]);
    if (!vfork())
	{
	execl (process, process, arg, 0);
        gds__log ("ISC_kill: error %d starting gds_relay %s", errno, process);
	_exit (0);
	}
    relay_pipe = pipes [1];

    /* Don't need the READ pipe */
    close (pipes [0]);
    }

msg [0] = pid;
msg [1] = signal_number;
msg [2] = msg [0] ^ msg [1];		/* XOR for a consistancy check */
if (write (relay_pipe, msg, sizeof (msg)) != sizeof (msg))
    {
    gds__log ("ISC_kill: write to relay_pipe failed %d", errno);
    relay_pipe = 0;			/* try to restart next time */
    return -1;
    }

return 0;
}
#endif

#ifdef WIN_NT
int API_ROUTINE ISC_kill (
    SLONG	pid,
    SLONG	signal_number,
    void	*object_hndl)
{
/**************************************
 *
 *	I S C _ k i l l		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Notify somebody else.
 *
 **************************************/
ULONG		oldest_age;
OPN_EVENT	opn_event, end_opn_event, oldest_opn_event;

/* If we're simply trying to poke ourselves, do so directly. */

if (pid == process_id)
    {
    SetEvent (object_hndl);
    return 0;
    }

oldest_age = ~0;

opn_event = opn_events;
end_opn_event = opn_event + opn_event_count;
for (; opn_event < end_opn_event; opn_event++)
    {
    if (opn_event->opn_event_pid == pid &&
	opn_event->opn_event_signal == signal_number)
	break;
    if (opn_event->opn_event_age < oldest_age)
	{
	oldest_opn_event = opn_event;
	oldest_age = opn_event->opn_event_age;
	}
    }

if (opn_event >= end_opn_event)
    {
    HANDLE	lhandle;

    if (!(lhandle = ISC_make_signal (FALSE, FALSE, pid, signal_number)))
	return -1;

    if (opn_event_count < MAX_OPN_EVENTS)
	opn_event_count++;
    else
	{
	opn_event = oldest_opn_event;
	CloseHandle (opn_event->opn_event_lhandle);
	}

    opn_event->opn_event_pid = pid;
    opn_event->opn_event_signal = signal_number;
    opn_event->opn_event_lhandle = lhandle;
    }

opn_event->opn_event_age = ++opn_event_clock;

return (SetEvent (opn_event->opn_event_lhandle)) ? 0 : -1;
}
#endif

#ifdef mpexl
int ISC_open_port (
    SLONG	pin,
    SLONG	pid,
    int		create_flag,
    int		async_flag,
    SLONG	*port)
{
/**************************************
 *
 *	I S C _ o p e n _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Open a port.
 *
 **************************************/
TEXT	port_name [17], port_pass [16], *p, *q, *end;
SLONG	status, create_opt, max_msg, max_msgs, norm_msg, itemnum [7], ^item [7];
SCHAR	interrupt;
void	(*handler)();

if (pin || pid)
    sprintf (port_name, "STARBASE%04X%04X", pin, pid);
else
    strcpy (port_name, "STARBASEGLOBAL  ");
q = lockword;
for (p = port_pass, end = p + sizeof (port_pass); p < end && *q;)
    *p++ = (*q++) + 'A' - 1;
while (p < end)
    *p++ = ' ';

itemnum [0] = 11201;
item [0] = &create_opt;
itemnum [1] = 11202;
item [1] = &max_msg;
itemnum [2] = 11203;
item [2] = &norm_msg;
itemnum [3] = 11204;
item [3] = &max_msgs;
itemnum [4] = (create_flag && (pin || pid)) ? 11206 : 0;
item [4] = &handler;
itemnum [5] = 11207;
item [5] = &interrupt;
itemnum [6] = 0;

create_opt = (create_flag) ? 2 : 3;
norm_msg = max_msg = sizeof (SLONG);
max_msgs = 128;
handler = signal_handler;
interrupt = 1;

*port = 0;
if (status = AIF_open_port (port, port_name, port_pass, (create_flag) ? 1 : 2, itemnum, item))
    if (*port)
	{
	AIF_close_port (*port);
	*port = 0;
	}

return status;
}
#endif

#ifndef BRIDGE
void API_ROUTINE ISC_signal (
    int		signal_number,
    void	(*handler)(),
    void	*arg)
{
/**************************************
 *
 *	I S C _ s i g n a l
 *
 **************************************
 *
 * Functional description
 *	Multiplex multiple handers into single signal.
 *
 **************************************/
isc_signal2 (signal_number, handler, arg, 0);
}
#endif	/* BRIDGE */

#ifndef BRIDGE
#ifdef SYSV_SIGNALS
static void isc_signal2 (
    int		signal_number,
    void	(*handler)(),
    void	*arg,
    ULONG	flags)
{
/**************************************
 *
 *	i s c _ s i g n a l 2		( S Y S V _ S I G N A L S )
 *
 **************************************
 *
 * Functional description
 *	Multiplex multiple handers into single signal.
 *
 **************************************/
SIG		sig;
int		n;
FPTR_INT	ptr;

/* The signal handler needs the process id */

if (!process_id)
    process_id = getpid();

THD_MUTEX_LOCK (&sig_mutex);

/* See if this signal has ever been cared about before */

for (sig = signals; sig; sig = sig->sig_next)
    if (sig->sig_signal == signal_number)
	break;

/* If it hasn't been attach our chain handler to the signal,
   and queue up whatever used to handle it as a non-ISC
   routine (they are invoked differently).  Note that if
   the old action was SIG_DFL, SIG_HOLD, SIG_IGN or our
   multiplexor, there is no need to save it. */

if (!sig)
    {
    ptr = sigset (signal_number, signal_handler);
    if (ptr != SIG_DFL &&
	ptr != SIG_IGN &&
	ptr != SIG_HOLD &&
	ptr != signal_handler)
	que_signal (signal_number, ptr, arg, SIG_client);
    }

/* Que up the new ISC signal handler routine */

que_signal (signal_number, handler, arg, flags);

THD_MUTEX_UNLOCK (&sig_mutex);
}
#endif	/* SYSV */
#endif	/* BRIDGE */

#ifndef BRIDGE
#if (defined UNIX || defined APOLLO || defined WIN_NT || defined OS2_ONLY || \
	(defined PC_PLATFORM && !defined NETWARE_386))
#ifndef SYSV_SIGNALS
static void isc_signal2 (
    int		signal_number,
    void	(*handler)(),
    void	*arg,
    ULONG	flags)
{
/**************************************
 *
 *	i s c _ s i g n a l 2		( u n i x ,   W I N _ N T ,   O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Multiplex multiple handers into single signal.
 *
 **************************************/
SIGVEC		vec, old_vec;
SIG		sig;
SIG_FPTR	ptr;

/* The signal handler needs the process id */

#ifndef PC_PLATFORM
if (!process_id)
    process_id = getpid();
#endif

#if (defined NeXT || defined WIN_NT)
/* If not a UNIX signal, just queue for port watcher. */

if (signal_number > NSIG)
    {
    que_signal (signal_number, handler, arg, flags);
    return;
    }
#endif

THD_MUTEX_LOCK (&sig_mutex);

/* See if this signal has ever been cared about before */

for (sig = signals; sig; sig = sig->sig_next)
    if (sig->sig_signal == signal_number)
	break;

/* If it hasn't been attach our chain handler to the signal,
   and queue up whatever used to handle it as a non-ISC
   routine (they are invoked differently).  Note that if
   the old action was SIG_DFL, SIG_HOLD, SIG_IGN or our
   multiplexor, there is no need to save it. */

if (!sig)
    {
#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
    ptr = signal (signal_number, signal_handler);
#else
#ifndef SIGACTION_SUPPORTED
#ifdef OLD_POSIX_THREADS
    if (signal_number != SIGALRM)
	vec.sv_handler = SIG_DFL;
    else
#endif
    vec.sv_handler = (SIG_FPTR) signal_handler;
    vec.sv_mask = 0;
    vec.sv_onstack = 0;
    sigvector (signal_number, &vec, &old_vec);
    ptr = old_vec.sv_handler;
#else
#ifdef OLD_POSIX_THREADS
    if (signal_number != SIGALRM)
	vec.sv_handler = SIG_DFL;
    else
#endif
    vec.sa_handler = (SIG_FPTR) signal_handler;
    memset (&vec.sa_mask, 0, sizeof (vec.sa_mask));
    vec.sa_flags = SA_RESTART;
    sigaction (signal_number, &vec, &old_vec);
    ptr = old_vec.sa_handler;
#endif
#endif
    if (ptr != SIG_DFL &&
	ptr != SIG_HOLD &&
	ptr != SIG_IGN &&
	ptr != signal_handler)
	que_signal (signal_number, (FPTR_VOID) ptr, arg, SIG_client);
    }

/* Que up the new ISC signal handler routine */

que_signal (signal_number, handler, arg, flags);

THD_MUTEX_UNLOCK (&sig_mutex);
}
#endif
#endif
#endif

#ifndef BRIDGE
#ifdef mpexl
static void isc_signal2 (
    int		type,
    void	(*handler)(),
    void	*arg,
    ULONG	flags)
{
/**************************************
 *
 *	i s c _ s i g n a l 2		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Multiplex multiple handers into single signal.
 *
 **************************************/

/* Que up the new ISC signal handler routine */

que_signal (type, handler, arg, flags);
}
#endif 
#endif 

#ifndef BRIDGE
#ifndef REQUESTER
void API_ROUTINE ISC_signal_cancel (
    int		signal_number,
    void	(*handler)(),
    void	*arg)
{
/**************************************
 *
 *	I S C _ s i g n a l _ c a n c e l
 *
 **************************************
 *
 * Functional description
 *	Cancel a signal handler.
 *	If handler == NULL, cancel all handlers for a given signal.
 *
 **************************************/
SIG	sig, *ptr;

THD_MUTEX_LOCK (&sig_mutex);

for (ptr = &signals; sig = *ptr;)
    if (sig->sig_signal == signal_number &&
	(handler == NULL || 
	  (sig->sig_routine == handler && sig->sig_arg == arg)))
	{
	*ptr = sig->sig_next;
	gds__free (sig);
	}
    else 
	ptr = &(*ptr)->sig_next;

THD_MUTEX_UNLOCK (&sig_mutex);

#ifdef OLD_POSIX_THREADS
{
IPTR	thread_id;

/* UNSAFE CODE HERE - sig has been freed - rewrite should
 * this section ever be activated.
 */
deliberate_error_here_to_force_compile_error++;
if (!sig || signal_number == SIGALRM)
    return;

thread_id = sig->sig_thread_id;
for (sig = signals; sig; sig = sig->sig_next)
    if (sig->sig_signal == signal_number)
	return;

/* No more handlers exist for the signal.  Kill the thread that's
   been listening for the signal. */

pthread_cancel ((pthread_t*) thread_id);
}
#endif
}
#endif
#endif

void DLL_EXPORT ISC_signal_init (void)
{
/**************************************
 *
 *	I S C _ s i g n a l _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize any system signal handlers.
 *
 **************************************/

#ifndef REQUESTER
#ifndef PIPE_CLIENT
if (initialized_signals)
    return;

initialized_signals = TRUE;

overflow_count = 0;
gds__register_cleanup (cleanup, NULL_PTR);

#ifndef VMS
#ifndef mpexl
#ifndef NETWARE_386
#ifndef PC_PLATFORM
process_id = getpid();

THD_MUTEX_INIT (&sig_mutex);

isc_signal2 (SIGFPE, overflow_handler, NULL_PTR, SIG_informs);
#endif
#endif
#endif
#endif

#endif /* PIPE_CLIENT */
#endif /* REQUESTER */

#ifdef NeXT
port_initialize();	/* Startup port watcher */
#endif

#ifdef OS2_ONLY
{
TEXT	name [40];
sprintf (name, "\\SEM32\\interbase_process%u", process_id);
DosCreateEventSem (name, &existence_event, 0, 0);
}
#endif

#ifdef WIN_NT
ISC_get_security_desc();
#endif
}

#ifdef mpexl
int ISC_wait (
    SSHORT	type,
    SLONG	timeout)
{
/**************************************
 *
 *	I S C _ w a i t		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Wait for a synchronous signal or invoke the
 *	handler of a deferred asynchronous signal.
 *
 **************************************/
SIG	sig;
SLONG	mask, end_time, status;

/* If the signal has already gone off, do something.  If it's asynchronous,
   invoke the necessary handler.  Otherwise, just clear the indicator. */

if (type < MIN_SYNC_SIGNAL)
    {
    for (sig = signals; sig; sig = sig->sig_next)
	if (sig->sig_signal == type && sig->sig_count)
	    {
	    sig->sig_count = 0;
	    (*sig->sig_routine) (sig->sig_arg);
	    }

    return 0;
    }

mask = 1 << (type - MIN_SYNC_SIGNAL);
if (sync_signals & mask)
    {
    sync_signals &= ~mask;
    return 0;
    }

/* The signal hasn't gone off.  We must wait for it. */

end_time = (timeout) ? TIMER() + timeout : 0;

AIF_interrupt_port (comm_async, FALSE);

if (sync_signals & mask)
    {
    sync_signals &= ~mask;
    AIF_interrupt_port (comm_async, TRUE);
    return 0;
    }

for (;;)
    {
    if (!(timeout = (end_time) ? timeout / 1000 : 120))
	timeout = 1;
    status = signal_process (timeout);

    /* If we received the message we were waiting for, then we're done. */

    if ((sync_signals & mask) ||
	(end_time && status) ||
	(!end_time && status != 0xd4ec0204 && status != 0xff910204)) /* -11028 || -111 */
	break;

    if (end_time)
	if ((timeout = end_time - TIMER()) <= 0)
	    break;
    }

while (!signal_process (-1))
    ;

if (sync_signals & mask)
    {
    sync_signals &= ~mask;
    status = 0;
    }

AIF_interrupt_port (comm_async, TRUE);

return status;
}
#endif

#ifndef REQUESTER
static void cleanup (
    void	*arg)
{
/**************************************
 *
 *	c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Module level cleanup handler.
 *
 **************************************/
signals = NULL;

THD_MUTEX_DESTROY (&sig_mutex);

inhibit_count = 0;

#ifdef GT_32_SIGNALS
pending_signals [0] = pending_signals [1] = 0;
#else
pending_signals = 0;
#endif

process_id = 0;

#ifdef OS2_ONLY
DosCloseEventSem (existence_event);
#endif

#ifdef WIN_NT
{
OPN_EVENT	opn_event;

opn_event = opn_events + opn_event_count;
opn_event_count = 0;
while (opn_event-- > opn_events) 
    CloseHandle (opn_event->opn_event_lhandle);
}
#endif

initialized_signals = FALSE;
}
#endif

#ifdef mpexl
static void cleanup_communications (
    void	*arg)
{
/**************************************
 *
 *	c l e a n u p _ c o m m u n i c a t i o n s	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Close any open communication files.
 *
 **************************************/

AIF_close_port (comm_async);
comm_async = 0;
}
#endif

#ifndef REQUESTER
static void error (
    STATUS	*status_vector,
    TEXT	*string,
    STATUS	status)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	We've encountered an error, report it.
 *
 **************************************/

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__sys_request;
*status_vector++ = gds_arg_string;
*status_vector++ = (STATUS) string;
*status_vector++ = SYS_ARG;
*status_vector++ = status;
*status_vector++ = gds_arg_end;
}
#endif

#ifdef NeXT
static void error_mach (
    STATUS		*status_vector,
    TEXT		*string,
    kern_return_t	status)
{
/**************************************
 *
 *	e r r o r _ m a c h
 *
 **************************************
 *
 * Functional description
 *	We've encountered an error, report it.
 *
 **************************************/

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__sys_request;
*status_vector++ = gds_arg_string;
*status_vector++ = (STATUS) string;
*status_vector++ = gds_arg_next_mach;
*status_vector++ = status;
*status_vector++ = gds_arg_end;
}
#endif

#ifndef REQUESTER
static SLONG overflow_handler (
    void	*arg)
{
/**************************************
 *
 *	o v e r f l o w _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Somebody overflowed.  Ho hum.
 *
 **************************************/

#ifdef DEBUG_FPE_HANDLING
ib_fprintf (ib_stderr, "overflow_handler (%x)\n", arg);
#endif

/* If we're within ISC world (inside why-value) when the FPE occurs
 * we handle it (basically by ignoring it).  If it occurs outside of
 * ISC world, return back a code that tells signal_handler to call any
 * customer provided handler.
 */
if (isc_enter_count)
    {
    ++overflow_count;
#ifdef DEBUG_FPE_HANDLING
    ib_fprintf (ib_stderr, "SIGFPE in isc code ignored %d\n", overflow_count);
#endif
    /* We've "handled" the FPE - let signal_handler know not to chain
       the signal to other handlers */
    return SIG_informs_stop;
    }
else 
    {
    /* We've NOT "handled" the FPE - let signal_handler know to chain
       the signal to other handlers */
    return SIG_informs_continue;
    }
}
#endif

#ifdef NeXT
static BOOLEAN port_initialize (void)
{
/**************************************
 *
 *	p o r t _ i n i t i a l i z e
 *
 **************************************
 *
 * Functional description
 *	Allocate port and startup port watcher.
 *
 **************************************/
kern_return_t	r;
port_all_t	port;
TEXT		service [32];
STATUS		status_vector [20];

if (!port_enabled)
    {
    sprintf (service, "GDS_RELAY.%d", getpid());
    r = port_allocate (task_self(), &port);
    if (r != KERN_SUCCESS)
	{
	error_mach (status_vector, "port_allocate", r);
	gds__log_status (NULL, status_vector);
	return FAILURE;
	}
    r = netname_check_in (name_server_port, service, port, port);
    if  (r != NETNAME_SUCCESS)
	{
	error_mach (status_vector, "netname_check_in", r);
	gds__log_status (NULL, status_vector);
	return FAILURE;
	}

    /* Call cthread_fork directly instead of gds__thread_start() */
    cthread_fork ((cthread_fn_t) port_watcher, (any_t) port);
    port_enabled = TRUE;
    }

return SUCCESS;
}

static BOOLEAN port_kill (
    int		pid,
    int		signal_number)
{
/**************************************
 *
 *	p o r t _ k i l l
 *
 **************************************
 *
 * Functional description
 *	Signal another NeXT task.
 *
 **************************************/
msg_return_t	r;
msg_header_t	message;
TEXT		service [32];
PORT		port, oldest_port, *ptr;
USHORT		i;
ULONG		oldest_age;

oldest_age = ~0;

for (ptr = &ports, i = 0; port = *ptr; ptr = &(*ptr)->port_next, i++)
    {
    if (port->port_pid == pid)
	break;
    if (port->port_age < oldest_age)
	{
	oldest_port = port;
	oldest_age = port->port_age;
	}
    }

if (!port)
    {
    if (i < MAX_PORTS)
	{
	*ptr = port = (PORT) gds__alloc ((SLONG) sizeof (struct port));
	/* FREE: unknown */
	if (!port)
	    return FAILURE;		/* NOMEM: not a great handler */
	port->port_next = (PORT) NULL;
	}
    else
	{
	port_deallocate (task_self(), oldest_port->port_name);
	port = oldest_port;
	}

    port->port_pid = pid;
    sprintf (service, "GDS_RELAY.%d", pid);
    r = netname_look_up (name_server_port, "", service, &port->port_name);
    if (r != NETNAME_SUCCESS)
	{
	port->port_pid = 0;
	port->port_name = PORT_NULL;
	port->port_age = 0;
	return FAILURE;
	}
    }

port->port_age = ++port_clock;

message.msg_remote_port = port->port_name;
message.msg_local_port = PORT_NULL;
message.msg_size = sizeof (message);
message.msg_id = signal_number;

r = msg_send (&message, MSG_OPTION_NONE, 0);
if (r != SEND_SUCCESS)
    return FAILURE;

return SUCCESS;
}

static void port_watcher (
    port_all_t	port)
{
/**************************************
 *
 *	p o r t _ w a t c h e r
 *
 **************************************
 *
 * Functional description
 *	A message has arrived, execute the
 *	signal handlers associated with it.
 *
 **************************************/
msg_return_t	r;
msg_header_t	message;
STATUS		status_vector [20];
SIG		sig;
int		number;
TEXT		service [32];

for (;;)
    {
    message.msg_size = sizeof (message);
    message.msg_local_port = port;

    r = msg_receive (&message, MSG_OPTION_NONE, 0);
    if (r != RCV_SUCCESS)
    	{
    	error_mach (status_vector, "msg_receive", r);
    	gds__log_status (NULL, status_vector);

	/* the following is un-pretty but we want OUT, RIGHT NOW */

	exit (-1);
 	}
    number = message.msg_id;
    for (sig = signals; sig; sig = sig->sig_next)
	if (sig->sig_signal == number)
	    (*sig->sig_routine) (sig->sig_arg);
    }

sprintf (service, "GDS_RELAY.%d", getpid());
netname_check_out (name_server_port, service, port);
port_deallocate (task_self(), port);
port_enabled = FALSE;
}
#endif 

#ifndef REQUESTER
static SIG que_signal (
    int		signal_number,
    void	(*handler)(),
    void	*arg,
    int		flags)
{
/**************************************
 *
 *	q u e _ s i g n a l
 *
 **************************************
 *
 * Functional description
 *	Que signal for later action.
 *
 **************************************/
SIG	sig;
IPTR	thread_id;

#ifdef OLD_POSIX_THREADS
if (signal_number != SIGALRM)
    {
    for (sig = signals; sig; sig = sig->sig_next)
	if (sig->sig_signal == signal_number)
	    break;

    if (!sig)
	pthread_create ((pthread_t*) &thread_id, pthread_attr_default,
	    sigwait_thread, (void*) signal_number);
    else
	thread_id = sig->sig_thread_id;
    }
#endif

sig = (SIG) gds__alloc ((SLONG) sizeof (struct sig));
/* FREE: unknown */
if (!sig)	/* NOMEM: */
    {
    DEV_REPORT ("que_signal: out of memory");
    return NULL;			/* NOMEM: not handled, too difficult */
    }

#ifdef DEBUG_GDS_ALLOC
/* This will only be freed when a signal handler is de-registered
 * and we don't do that at process exit - so this not always
 * a freed structure.
 */
gds_alloc_flag_unfreed ((void *)sig);
#endif

sig->sig_signal = signal_number;
sig->sig_routine = handler;
sig->sig_arg = arg;
sig->sig_flags = flags;
sig->sig_thread_id = thread_id;
sig->sig_count = 0;

sig->sig_next = signals;
signals = sig;

return sig;
}
#endif

#ifndef mpexl
#ifndef REQUESTER
static void CLIB_ROUTINE signal_handler (
    int		number,
    int		code,
#ifdef HANDLER_ADDR_ARG
    void	*scp,
    void	*addr)
#else
    struct sigcontext *scp)
#endif
{
/**************************************
 *
 *	s i g n a l _ h a n d l e r	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Checkin with various signal handlers.
 *
 **************************************/
SIG	sig;

/* if there are no signals, who cares? */

if (signals == (SIG) NULL)
    return;

/* This should never happen, but if it does might as well not crash */

if (number == 0)
    return;

/* If signals are inhibited, save the signal for later reposting.
   Otherwise, invoke everybody who may have expressed an interest. */

#ifdef SIGALRM
if (inhibit_count && number != SIGALRM)
#else
if (inhibit_count)
#endif
#ifdef GT_32_SIGNALS
    pending_signals [(number-1)/32] |= 1L << ((number - 1) % 32);
#else
    pending_signals |= 1L << (number - 1);
#endif
else
    for (sig = signals; sig; sig = sig->sig_next)
	if (sig->sig_signal == number)
	    if (sig->sig_flags & SIG_client)
#ifdef HANDLER_ADDR_ARG
		(*sig->sig_routine) (number, code, scp, addr);
#else
		(*sig->sig_routine) (number, code, scp);
#endif
	    else if (sig->sig_flags & SIG_informs)
		{
		ULONG	res;
		/* Routine will tell us whether to chain the signal to other handlers */
		res = (SLONG) ((*((FPTR_INT)sig->sig_routine)) (sig->sig_arg));
		if (res == SIG_informs_stop)
		    break;
		}
	    else
		(*sig->sig_routine) (sig->sig_arg);

#ifdef SIG_RESTART
scp->sc_syscall_action = (!ISC_check_restart() || number == SIGALRM) ? SIG_RETURN : SIG_RESTART;
#endif
}
#endif
#endif

#ifdef mpexl
static void CLIB_ROUTINE signal_handler (
    SLONG	port)
{
/**************************************
 *
 *	s i g n a l _ h a n d l e r	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	A signal has arrived, go retrieve th
 *	message associated with it.
 *
 **************************************/

AIF_interrupt_port (port, FALSE);
signal_process (-1);
AIF_interrupt_port (port, TRUE);
}
#endif

#ifdef mpexl
static SLONG signal_process (
    SLONG	wait)
{
/**************************************
 *
 *	s i g n a l _ p r o c e s s
 *
 **************************************
 *
 * Functional description
 *	Process an incoming signal.
 *
 **************************************/
SLONG	status, msg_type, msg_from, msg_len, itemnum [2], ^item [2];
SIG	sig;

/* Complete the I/O */

itemnum [0] = 11002;
item [0] = &wait;
itemnum [1] = 0;

msg_len = 0;

if (status = AIF_receive_port (comm_async, &msg_type, &msg_from, &msg_len, itemnum, item))
    return status;

/* If signals are inhibited, save the signal for later reposting.
   Otherwise, invoke everybody who has expressed an interest. */

if (msg_type < MIN_SYNC_SIGNAL)
    {
    for (sig = signals; sig; sig = sig->sig_next)
	if (sig->sig_signal == msg_type)
	    if (!inhibit_count)
		(*sig->sig_routine) (sig->sig_arg);
	    else
		{
		pending_signals |= 1 << msg_type;
		sig->sig_count++;
		}
    }
else
    sync_signals |= 1 << (msg_type - MIN_SYNC_SIGNAL);

return status;
}
#endif

#ifdef OLD_POSIX_THREADS
static void sigwait_thread (
    int		signal_number)
{
/**************************************
 *
 *	s i g w a i t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *	This thread waits for a given signal
 *	and calls the all purpose signal
 *	handler whenever it arrives.
 *
 **************************************/
sigset_t	sigmask;
SIG		sig;

sigemptyset (&sigmask);
sigaddset (&sigmask, signal_number);

while (TRUE)
    {
    sigwait (&sigmask);

    /* If signals are inhibited, save the signal for later reposting.
       Otherwise, invoke everybody who may have expressed an interest. */

    if (inhibit_count && signal_number != SIGALRM)
#ifdef GT_32_SIGNALS
	pending_signals [(signal_number-1)/32] |= 1 << ((signal_number - 1) % 32);
#else
	pending_signals |= 1 << (signal_number - 1);
#endif
    else
	for (sig = signals; sig; sig = sig->sig_next)
	    if (sig->sig_signal == signal_number)
		if (sig->sig_flags & SIG_client)
#ifdef HANDLER_ADDR_ARG
		    (*sig->sig_routine) (number, 0, NULL, NULL);
#else
		    (*sig->sig_routine) (number, 0, NULL);
#endif
		else
		    (*sig->sig_routine) (sig->sig_arg);
    }
}
#endif

#ifdef mpexl
static trap_handler (trap)
    struct {
	SLONG	trap_instruction;
	SLONG	trap_offset;
	SLONG	trap_space_id;
	SLONG	trap_error;
    }		*trap;
{
/**************************************
 *
 *	t r a p _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Overload InterBase and user enabled traps.
 *
 **************************************/

if (enter_count && (trap->trap_error & SIGFPE))
    {
    /* A FPE trap occurred in our code.  Call our handler. */

    (void) overflow_handler (trap);
    return;
    }

if (client_sigfpe != (SIG_FPTR) NULL && (trap->trap_error & client_trap_mask))
    {
    /* An armed and enabled trap occurred in the user's code. Call their handler. */

    (*client_sigfpe) (trap);
    return;
    }

/* If the user didn't enable the trap, ignore it. */

if (!(trap->trap_error & client_enable_mask))
    return;

/* The trap is enabled but not armed.  WHAT DO WE DO? */

abort();
}
#endif
