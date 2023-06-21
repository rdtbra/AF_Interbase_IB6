/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_sync.c
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
#include <string.h>

#ifdef SOLARIS
#ifndef DEV_BUILD
#define NDEBUG		/* Turn off assert() macros */
#endif
#include <assert.h>
#endif

#ifdef HP10
#include <sys/pstat.h>
#endif 

#include "../jrd/time.h"
#include "../jrd/common.h"
#include "../jrd/codes.h"
#include "../jrd/isc.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_i_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/file_params.h"
#include "../jrd/gdsassert.h"
#include "../jrd/jrd.h"
#include "../jrd/sch_proto.h"

#ifdef apollo
typedef int	(* CLIB_ROUTINE SIG_FPTR)();
#else
#ifndef SUN3_3
typedef void	(* CLIB_ROUTINE SIG_FPTR)();
#else
typedef int	(* CLIB_ROUTINE SIG_FPTR)();
#endif
#endif

#ifndef REQUESTER
static USHORT	inhibit_restart;
static int	process_id;
static UCHAR	*next_shared_memory;
#endif


/* WINDOWS Specific Stuff */

#ifdef WINDOWS_ONLY
#endif


/* VMS Specific Stuff */

#ifdef VMS

#include <rms.h>
#include <descrip.h>
#include <ssdef.h>
#include <jpidef.h>
#include <prvdef.h>
#include <secdef.h>
#include <lckdef.h>
#include "../jrd/lnmdef.h"
#include <signal.h>

#include "../jrd/prv_m_bypass.h"
#endif /* of ifdef VMS */

/* Unix specific stuff */

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <errno.h>
#ifndef NeXT
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#endif

#ifndef O_RDWR
#include <fcntl.h>
#endif

#ifdef MMAP_SUPPORTED
#include <sys/mman.h>
#endif

#ifdef DGUX
#include <fcntl.h>
#endif

#ifdef NeXT
#include <mach/cthreads.h>
#include <mach/mach.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define ISC_MUTEX	&isc_mutex
#define ISC_EVENT	&isc_condition

static struct mutex	isc_mutex = MUTEX_INITIALIZER;
static struct condition	isc_condition = CONDITION_INITIALIZER;

#endif

#define FTOK_KEY	15
#define PRIV		0666
#define LOCAL_SEMAPHORES 4

#ifdef DELTA
#include <sys/sysmacros.h>
#include <sys/param.h>
#endif

#ifdef IMP
typedef unsigned int    mode_t;
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

#ifdef sun
#ifndef SOLARIS
#define SEMUN
#endif
#endif

#ifdef LINUX
/*
 * NOTE: this definition is copied from linux/sem.h: if we do ...
 *    #include <linux/sem.h>
 * we get redefinition error messages for other structures, 
 * so try it this (ugly) way.
 */
/* arg for semctl system calls. */
union semun {
	int val;			/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
	unsigned short *array;		/* array for GETALL & SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
	void *__pad;
};
#define SEMUN
#endif

#ifdef XENIX
/* 5.5 SCO Port: SIGURG - is now available in SCO Openserver */
#ifndef SCO_EV
#define SIGURG		SIGUSR1
#endif
#endif

#ifndef SIGURG
#define SIGURG		SIGINT
#endif

#ifdef sgi
#define SEMUN
#endif

#ifdef SEMUN
#undef SEMUN
#else
union semun {
    int			val;
    struct semid_ds	*buf;
    ushort		*array;
};
#endif

#if !(defined M88K || defined hpux || defined DECOSF || defined SOLARIS || \
      defined DG_X86 || defined linux )
extern SLONG		ftok();
#endif
#endif

/* Windows NT */

#ifdef WIN_NT

#include <process.h>
#include <signal.h>
#include <windows.h>

#endif

/* OS2 Junk */

#ifdef OS2_ONLY

#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#include <os2.h>

#define MAXPATHLEN	512

#endif

/* NLM stuff */

#ifdef NETWARE_386
typedef struct shr_mem_hndl {
    struct shr_mem_hndl	*next;
    SH_MEM_T		shmem_data;
    TEXT		file_name [MAXPATHLEN];
} *SHR_MEM_HNDL;

static SHR_MEM_HNDL	shared_memory_handles = NULL;
static MTX_T		sync_mutex;
#define SYNC_MUTEX	&sync_mutex

#endif

/* HP MPE/XL specific stuff */

#ifdef mpexl

#include <fcntl.h>
#include <mpe.h>
#include "../jrd/mpexl.h"

#define MIN_SYNC_SIGNAL	32

#define MAPPED_FILE	".mapped.starbase"

static UCHAR	lockword [] = {'G'-'A'+1, 1, 'L'-'A'+1, 1, 'X'-'A'+1, 'Y'-'A'+1, 0};
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

static void	alarm_handler (void);
static void	error (STATUS *, TEXT *, STATUS);
static SLONG	find_key (STATUS *, TEXT *);
static SLONG	init_semaphores (STATUS *, SLONG, int);
#ifdef SUPERSERVER
#ifdef UNIX
static void	longjmp_sig_handler (int);
#endif /* UNIX */
#endif /* SUPERSERVER */
static BOOLEAN	semaphore_wait (int, int, int *);
#ifdef NETWARE_386
static void	cleanup_semaphores (ULONG);
#endif
#ifdef NeXT
static void	error_mach (STATUS *, TEXT *, kern_return_t);
static void	event_wakeup (void);
#endif
#ifdef VMS
static int	event_test (WAIT *);
static BOOLEAN	mutex_test (MTX);
#endif
#if (defined WIN_NT || defined OS2_ONLY)
static void	make_object_name (TEXT *, TEXT *, TEXT *);
#endif

#ifndef sigvector
#ifndef hpux
#define sigvector	sigvec
#endif
#endif

#ifndef SV_INTERRUPT
#define SV_INTERRUPT    0
#endif

#ifdef SHLIB_DEFS
#define sprintf		(*_libgds_sprintf)
#define strlen		(*_libgds_strlen)
#define strcmp		(*_libgds_strcmp)
#define strcpy		(*_libgds_strcpy)
#define _iob		(*_libgds__iob)
#define shmdt		(*_libgds_shmdt)
#define ib_fprintf		(*_libgds_fprintf)
#define errno		(*_libgds_errno)
#define ib_fopen		(*_libgds_fopen)
#define ib_fclose		(*_libgds_fclose)
#define open		(*_libgds_open)
#define semctl		(*_libgds_semctl)
#define semop		(*_libgds_semop)
#define umask		(*_libgds_umask)
#define close		(*_libgds_close)
#define _ctype		(*_libgds__ctype)
#define sigvector	(*_libgds_sigvec)
#define pause		(*_libgds_pause)
#define lockf		(*_libgds_lockf)
#define shmget		(*_libgds_shmget)
#define shmat		(*_libgds_shmat)
#define shmctl		(*_libgds_shmctl)
#define ftok		(*_libgds_ftok)
#define semget		(*_libgds_semget)
#define sigset		(*_libgds_sigset)
#define sbrk		(*_libgds_sbrk)
#define setitimer	(*_libgds_setitimer)
#define alarm		(*_libgds_alarm)
#ifndef IMP
#define sigprocmask	(*_libgds_sigprocmask)
#define sigsuspend	(*_libgds_sigsuspend)
#define sigaddset	(*_libgds_sigaddset)
#else
#define sigsetmask	(*_libgds_sigsetmask)
#define sigpause	(*_libgds_sigpause)
#define sigblock	(*_libgds_sigblock)
#endif

extern int		sprintf();
extern int		strlen();
extern int		strcmp();
extern SCHAR		*strcpy();
extern IB_FILE		_iob [];
extern int		shmdt();
extern int		ib_fprintf();
extern int		errno;
extern IB_FILE		*ib_fopen();
extern int		ib_fclose();
extern int		open();
extern int		semctl();
extern int		semop();
extern mode_t		umask();
extern int		close();
extern SCHAR		_ctype [];
extern int		sigvector();
extern int		pause();
extern int		lockf();
extern int		shmget();
extern int		shmctl();
extern key_t		ftok();
extern int		semget();
extern void		(*sigset())();
extern SCHAR		*sbrk();
extern int		setitimer();
extern int		alarm();
extern SCHAR		*shmat();
#ifndef IMP
extern int		sigprocmask();
extern int		sigsuspend();
extern int		sigaddset();
#else
extern int		sigsetmask();
extern int		sigpause();
extern int		sigblock();
#endif
#endif

#ifdef SIG_RESTART
BOOLEAN ISC_check_restart (void)
{
/**************************************
 *
 *	I S C _ c h e c k _ r e s t a r t
 *
 **************************************
 *
 * Functional description
 *	Return a flag that indicats whether
 *	or not to restart an interrupted
 *	system call.
 *
 **************************************/

return (inhibit_restart) ? FALSE : TRUE;
}
#endif	/* SIG_RESTART */

#ifndef PIPE_IS_SHRLIB
#ifdef APOLLO
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count; --count, events++, values++)
    if (ec2_$read ((*events)->event_eventcount) >=  *values)
	return FALSE;

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

return ec2_$read (event->event_eventcount) + 1;
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

ec2_$init (&event->event_eventcount);

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
status_$t	status;

ec2_$advance (&event->event_eventcount, &status);

return status.all;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
int		n;
status_$t	status;

for (;;)
    {
    task_$yield();
    n = ec2_$wait (events, values, count, &status) - 1;
    if (status.all || (events [n])->event_eventcount.value >= values [n])
	break;
    }

return status.all;
}
#endif

#ifdef mpexl
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count; --count, events++, values++)
    if ((*events)->event_count >= *values)
	return FALSE;

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

return event->event_count + 1;
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

event->event_pid = process_id = getpid();
event->event_count = 0;

return TRUE;
}

int ISC_event_post (
    EVENT	event,
    SSHORT	type,
    SLONG	port,
    int		flag)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/

++event->event_count;
if (flag)
    ISC_kill (event->event_pid, type, port);

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SSHORT	type,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
SLONG	status;

if (micro_seconds < 0)
    micro_seconds = 0;

for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	return 0;

    if (status = ISC_wait (type, micro_seconds / 1000))
	return status;
    }
}
#endif

#ifdef NeXT
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if ((*events)->event_count >= *values)
	return FALSE;

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

return event->event_count + 1;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

if (event->event_type)
    ISC_signal_cancel (event->event_type, event_wakeup, NULL_PTR);
}

int ISC_event_init (
    EVENT	event,
    int		type,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

event->event_pid = process_id = getpid();
event->event_count = 0;

if (event->event_type = type)
    ISC_signal (type, event_wakeup, NULL_PTR);

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/

++event->event_count;

if (event->event_pid != process_id)
    ISC_kill (event->event_pid, event->event_type);
else
    event_wakeup();

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/

ISC_mutex_lock (ISC_MUTEX);
while (ISC_event_blocked (count, events, values))
    condition_wait (ISC_EVENT, ISC_MUTEX);
ISC_mutex_unlock (ISC_MUTEX);

return 0;
}
#endif

#ifdef OS2_ONLY
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if (!(*events)->event_shared)
	{
	if ((*events)->event_count >= *values)
	    return FALSE;
	}
    else
	{
	if ((*events)->event_shared->event_count >= *values)
	    return FALSE;
	}

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/
ULONG	count;

DosResetEventSem (event->event_handle, &count);

if (!event->event_shared)
    return event->event_count + 1;
else
    return event->event_shared->event_count + 1;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

DosCloseEventSem (event->event_handle);
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

event->event_pid = process_id = getpid();
event->event_count = 0;
event->event_shared = NULL;

return (DosCreateEventSem (NULL, &event->event_handle, DC_SEM_SHARED, 0)) ?
    FALSE : TRUE;
}

int ISC_event_init_shared (
    EVENT	lcl_event,
    int		type,
    TEXT	*name,
    EVENT	shr_event,
    USHORT	init_flag)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t _ s h a r e	d	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/
TEXT	event_name [MAXPATHLEN], type_name [16];
ULONG	status;

lcl_event->event_pid = process_id = getpid();
lcl_event->event_count = 0;
lcl_event->event_handle = 0;
lcl_event->event_shared = shr_event;

sprintf (type_name, "\\SEM32\\evnt%d", type);
make_object_name (event_name, name, type_name);
while (TRUE)
    {
    status = DosCreateEventSem (event_name, &lcl_event->event_handle, 0, 0); 
    if (!status || status != ERROR_DUPLICATE_NAME)
	break;
    status = DosOpenEventSem (event_name, &lcl_event->event_handle); 
    if (!status || status != ERROR_SEM_NOT_FOUND)
	break;
    }

if (init_flag)
    {
    lcl_event->event_shared->event_pid = 0;
    lcl_event->event_shared->event_count = 0;
    lcl_event->event_shared->event_handle = 0;
    lcl_event->event_shared->event_shared = NULL;
    }

return (status) ? FALSE : TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/

if (!event->event_shared)
    ++event->event_count;
else
    ++event->event_shared->event_count;

if (event->event_pid != process_id)
    ISC_kill (event->event_pid, event->event_handle);
else
    DosPostEventSem (event->event_handle);

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
ULONG	timeout, status;

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return 0;

/* Go into wait loop */

if (micro_seconds > 0)
    timeout = micro_seconds / 1000;
else
    timeout = SEM_INDEFINITE_WAIT;

if (count == 1)
    {
    for (;;)
	{
	if (!ISC_event_blocked (count, events, values))
	    return 0;
	if (status = DosWaitEventSem ((*events)->event_handle, timeout))
	    return status;
	}
    }
else
    {
    SSHORT	i;
    SEMRECORD	semnums [16], *semnum;
    EVENT	*event;
    HMUX	handle;
    ULONG	user;

    for (i = 0, event = events, semnum = semnums; i < count; i++, semnum++)
	{
	semnum->hsemCur = (HSEM) (*event++)->event_handle;
	semnum->ulUser = i;
	}
    if (status = DosCreateMuxWaitSem (NULL, &handle, (ULONG) count, semnums,
	    DC_SEM_SHARED | DCMW_WAIT_ALL))
	return status;
    for (status = 0;;)
	{
	if (!ISC_event_blocked (count, events, values))
	    break;
	if (status = DosWaitMuxWaitSem (handle, timeout, &user))
	    break;
	}
    DosCloseMuxWaitSem (handle);
    return status;
    }
}
#endif

#endif /* PIPE_IS_SHRLIB */
#ifdef SOLARIS_MT
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if ((*events)->event_count >= *values)
        {
#ifdef ISC_SYNC_DEBUG
	ib_printf ("ISC_event_blocked: FALSE (eg something to report)\n");
	ib_fflush (ib_stdout);
#endif
	return FALSE;
	}

#ifdef ISC_SYNC_DEBUG
ib_printf ("ISC_event_blocked: TRUE (eg nothing happened yet)\n");
ib_fflush (ib_stdout);
#endif
return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/
SLONG	ret;

mutex_lock (event->event_mutex);
ret = event->event_count + 1;
mutex_unlock (event->event_mutex);
return ret;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

/* Inter-Process event's are destroyed only */
if (event->event_semid == -1)
    {
    mutex_destroy (event->event_mutex);
    cond_destroy (event->event_semnum);
    }
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/
SLONG	key, n;
union semun	arg;

event->event_count = 0;

#ifdef PIPE_IS_SHRLIB
assert (semnum == 0);
#endif /* PIPE_IS_SHRLIB */

if (!semnum)
    {
    /* Prepare an Inter-Thread event block */
    event->event_semid = -1;
    mutex_init (event->event_mutex, USYNC_THREAD, NULL);
    cond_init (event->event_semnum, USYNC_THREAD, NULL);
    }
#ifndef PIPE_IS_SHRLIB
else
    {
    /* Prepare an Inter-Process event block */
    event->event_semid = semid;
    mutex_init (event->event_mutex, USYNC_PROCESS, NULL);
    cond_init (event->event_semnum, USYNC_PROCESS, NULL);
    }
#endif /* PIPE_IS_SHRLIB */

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
int		ret;

#ifdef PIPE_IS_SHRLIB
assert (event->event_semid == -1);
#endif /* PIPE_IS_SHRLIB */

/* For Solaris, we use cond_broadcast rather than cond_signal so that
   all waiters on the event are notified and awakened */

mutex_lock (event->event_mutex);
++event->event_count;
ret = cond_broadcast (event->event_semnum);
mutex_unlock (event->event_mutex);
if (ret)
    gds__log ("ISC_event_post: cond_broadcast failed with errno = %d", ret);
return ret;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.  If timeout limit specified, return
 *	anyway after the timeout even if no event has
 *	happened.  If returning due to timeout, return
 *	FAILURE else return SUCCESS.
 *
 **************************************/
int			ret;
timestruc_t		timer;

/* While the API for ISC_event_wait allows for a list of events
   we never actually make use of it.  This implementation wont
   support it anyway as Solaris doesn't provide a "wait for one
   of a series of conditions" function */
assert (count == 1);

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return SUCCESS;

/* Set up timers if a timeout period was specified. */

if (micro_seconds > 0)
    {
    timer.tv_sec = time (NULL);
    timer.tv_sec += micro_seconds / 1000000;
    timer.tv_nsec = 1000 * (micro_seconds % 1000000);
    }

ret = SUCCESS;
mutex_lock ((*events)->event_mutex);
for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	{
	ret = SUCCESS;
	break;
	}

    /* The Solaris cond_wait & cond_timedwait calls atomically release
       the mutex and start a wait.  The mutex is reacquired before the
       call returns. */

    if (micro_seconds > 0)
    	ret = cond_timedwait ((*events)->event_semnum, (*events)->event_mutex, &timer);
    else
       	ret = cond_wait ((*events)->event_semnum, (*events)->event_mutex);
    if (micro_seconds > 0 && (ret == ETIME))
	{

	/* The timer expired - see if the event occured and return
	   SUCCESS or FAILURE accordingly. */

	if (ISC_event_blocked (count, events, values))
	    ret = FAILURE;
	else
	    ret = SUCCESS;
	break;
	}
    }
mutex_unlock ((*events)->event_mutex);
return ret;
}
#endif  /* SOLARIS_MT */

#ifdef POSIX_THREADS
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if ((*events)->event_count >= *values)
        {
#ifdef ISC_SYNC_DEBUG
	ib_printf ("ISC_event_blocked: FALSE (eg something to report)\n");
	ib_fflush (ib_stdout);
#endif
	return FALSE;
	}

#ifdef ISC_SYNC_DEBUG
ib_printf ("ISC_event_blocked: TRUE (eg nothing happened yet)\n");
ib_fflush (ib_stdout);
#endif
return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/
SLONG	ret;

pthread_mutex_lock (event->event_mutex);
ret = event->event_count + 1;
pthread_mutex_unlock (event->event_mutex);
return ret;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

/* Inter-Thread event's are destroyed only */
if (event->event_semid == -1)
    {
    pthread_mutex_destroy (event->event_mutex);
    pthread_cond_destroy (event->event_semnum);
    }
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/
SLONG	key, n;
union semun	arg;
pthread_mutexattr_t	mattr;
pthread_condattr_t	cattr;

event->event_count = 0;

#ifdef PIPE_IS_SHRLIB
assert (semnum == 0);
#endif /* PIPE_IS_SHRLIB */

if (!semnum)
    {
    /* Prepare an Inter-Thread event block */
    event->event_semid = -1;

    /* Default attribute objects initialize sync. primitives 
       to be used to sync thread within one process only.
    */
#ifdef HP10
    pthread_mutex_init (event->event_mutex, pthread_mutexattr_default);
    pthread_cond_init (event->event_semnum, pthread_condattr_default);
#else
    pthread_mutex_init (event->event_mutex, NULL);
    pthread_cond_init (event->event_semnum, NULL);
#endif /* HP10 */
    }
#ifndef PIPE_IS_SHRLIB
else
    {
    /* Prepare an Inter-Process event block */
    event->event_semid = semid;

    /* NOTE: HP's Posix threads implementation does not support thread
             synchronization in different processes. Thus the following
             fragment is just a temp. decision we could be use for super-
             server (until we are to implement local IPC using shared
             memory in which case we need interprocess thread sync.
    */
#ifdef HP10
    pthread_mutex_init (event->event_mutex, pthread_mutexattr_default);
    pthread_cond_init (event->event_semnum, pthread_condattr_default);
#else
#ifdef linux
    pthread_mutex_init (event->event_mutex, NULL);
    pthread_cond_init (event->event_semnum, NULL);
#else
    pthread_mutexattr_init (&mattr);
    pthread_mutexattr_setpshared (&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init (event->event_mutex, &mattr);
    pthread_mutexattr_destroy (&mattr);

    pthread_condattr_init (&cattr);
    pthread_condattr_setpshared (&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init (event->event_semnum, &cattr);
    pthread_condattr_destroy (&cattr);
#endif
#endif
    }
#endif /* PIPE_IS_SHRLIB */

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
int		ret;

#ifdef PIPE_IS_SHRLIB
assert (event->event_semid == -1);
#endif /* PIPE_IS_SHRLIB */

pthread_mutex_lock (event->event_mutex);
++event->event_count;
ret = pthread_cond_broadcast (event->event_semnum);
pthread_mutex_unlock (event->event_mutex);
if (ret)

#ifdef HP10

    {
    assert (ret == -1);
    gds__log ("ISC_event_post: pthread_cond_broadcast failed with errno = %d", errno);
    return errno;
    }
return 0;

#else

    gds__log ("ISC_event_post: pthread_cond_broadcast failed with errno = %d", ret);
return ret;

#endif /* HP10 */
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.  If timeout limit specified, return
 *	anyway after the timeout even if no event has
 *	happened.  If returning due to timeout, return
 *	FAILURE else return SUCCESS.
 *
 **************************************/
int			ret;
struct timespec		timer;

/* While the API for ISC_event_wait allows for a list of events
   we never actually make use of it.  This implementation wont
   support it anyway as Solaris doesn't provide a "wait for one
   of a series of conditions" function */
assert (count == 1);

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return SUCCESS;

/* Set up timers if a timeout period was specified. */

if (micro_seconds > 0)
    {
    timer.tv_sec = time (NULL);
    timer.tv_sec += micro_seconds / 1000000;
    timer.tv_nsec = 1000 * (micro_seconds % 1000000);
    }

ret = SUCCESS;
pthread_mutex_lock ((*events)->event_mutex);
for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	{
	ret = SUCCESS;
	break;
	}

    /* The Posix pthread_cond_wait & pthread_cond_timedwait calls
       atomically release the mutex and start a wait.
       The mutex is reacquired before the call returns.
    */
    if (micro_seconds > 0)
    	ret = pthread_cond_timedwait ((*events)->event_semnum, (*events)->event_mutex, &timer);
    else
       	ret = pthread_cond_wait ((*events)->event_semnum, (*events)->event_mutex);

#ifdef HP10
    if (micro_seconds > 0 && (ret == -1) && (errno == EAGAIN))
#else
#ifdef linux
    if (micro_seconds > 0 && (ret == ETIMEDOUT))
#else
    if (micro_seconds > 0 && (ret == ETIME))
#endif
#endif
	{

	/* The timer expired - see if the event occured and return
	   SUCCESS or FAILURE accordingly. */

	if (ISC_event_blocked (count, events, values))
	    ret = FAILURE;
	else
	    ret = SUCCESS;
	break;
	}
    }
pthread_mutex_unlock ((*events)->event_mutex);
return ret;
}
#endif  /* POSIX_THREADS */

#ifdef UNIX
#ifndef EVENTS
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if ((*events)->event_count >= *values)
        {
#ifdef ISC_SYNC_DEBUG
	ib_printf ("ISC_event_blocked: FALSE (eg something to report)\n");
	ib_fflush (ib_stdout);
#endif
	return FALSE;
	}

#ifdef ISC_SYNC_DEBUG
ib_printf ("ISC_event_blocked: TRUE (eg nothing happened yet)\n");
ib_fflush (ib_stdout);
#endif
return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/
int	ret;
union semun	arg;

#ifdef PIPE_IS_SHRLIB
assert (event->event_semid == -1);
#endif /* PIPE_IS_SHRLIB */

#ifndef PIPE_IS_SHRLIB
if (event->event_semid != -1)
    {
    arg.val = 1;
    ret = semctl (event->event_semid, event->event_semnum, SETVAL, arg);
    }
#endif /* PIPE_IS_SHRLIB */

return (event->event_count + 1);
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/
SLONG	key, n;
union semun	arg;

event->event_count = 0;

#ifdef PIPE_IS_SHRLIB
assert (semnum == 0);
#endif /* PIPE_IS_SHRLIB */

if (!semnum)
    {
    event->event_semid = -1;
    event->event_semnum = 0;
    }
#ifndef PIPE_IS_SHRLIB
else
    {
    event->event_semid = semid;
    event->event_semnum = semnum;
    arg.val = 0;
    n = semctl (semid, semnum, SETVAL, arg);
    }
#endif /* PIPE_IS_SHRLIB */

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
int		ret;
union semun	arg;

++event->event_count;

#ifdef PIPE_IS_SHRLIB

assert (event->event_semid == -1);

#else

while (event->event_semid != -1)
    {
    arg.val = 0;
    ret = semctl (event->event_semid, event->event_semnum, SETVAL, arg);
    if (ret != -1)
	return 0;
    if (!SYSCALL_INTERRUPTED(errno))
	{
	gds__log ("ISC_event_post: semctl failed with errno = %d", errno);
	return errno;
	}
    }

#endif /* PIPE_IS_SHRLIB */

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 **************************************
 *
 * Functional description
 *	Wait on an event.  If timeout limit specified, return
 *	anyway after the timeout even if no event has
 *	happened.  If returning due to timeout, return
 *	FAILURE else return SUCCESS.
 *
 **************************************/
int			ret;
sigset_t		mask, oldmask;
EVENT			*event;
int			semid, i;
int			semnums [16], *semnum;
#ifdef SYSV_SIGNALS
SLONG			user_timer;
void			*user_handler;
#else
struct itimerval	user_timer;
#ifndef SIGACTION_SUPPORTED
struct sigvec		user_handler;
#else
struct sigaction	user_handler;
#endif
#endif
#ifdef SOLARIS
timestruc_t		timer;
#endif

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return SUCCESS;

/* If this is a local semaphore, don't sweat the semaphore non-sense */

if ((*events)->event_semid == -1)
    {
    ++inhibit_restart;
#ifdef IMP
    oldmask = sigblock( sigmask(SIGUSR1) | sigmask(SIGUSR2) | sigmask(SIGURG));
#else
    sigprocmask(SIG_BLOCK, NULL, &oldmask);
    mask = oldmask;
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGURG);
    sigprocmask(SIG_BLOCK, &mask, NULL);
#endif
    for (;;)
	{
	if (!ISC_event_blocked (count, events, values))
	    {
	    --inhibit_restart;
#ifdef IMP
	    sigsetmask(oldmask);
#else
	    sigprocmask(SIG_SETMASK, &oldmask, NULL);
#endif
	    return SUCCESS;
	    }
#ifdef IMP
	mask=sigsetmask(oldmask);
	sigpause(0);
	oldmask=sigsetmask(mask);
#else
	sigsuspend(&oldmask);
#endif
	}
    }

#ifdef PIPE_IS_SHRLIB
assert (FALSE);
#else
/* Only the internal event work is available in the SHRLIB version of pipe server
 */

/* Set up for a semaphore operation */

semid = (int) (*events)->event_semid;

/* Collect the semaphore numbers in an array */

for (i = 0, event = events, semnum = semnums; i < count; i++ )
    *semnum++ = (*event++)->event_semnum;

/* Set up timers if a timeout period was specified. */

if (micro_seconds > 0)
    {
    if (!timeout_handler)
	timeout_handler = alarm_handler;

    ISC_set_timer (micro_seconds, timeout_handler, handler_arg,
	&user_timer, &user_handler);
    }

/* Go into wait loop */

for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	{
	if (micro_seconds <= 0)
	    return SUCCESS;
	ret = SUCCESS;
	break;
	}
    (void) semaphore_wait (count, semid, semnums);
    if (micro_seconds > 0)
	{
	/* semaphore_wait() routine may return SUCCESS if our timeout
	   handler poked the semaphore.  So make sure that the event 
	   actually happened.  If it didn't, indicate failure. */

	if (ISC_event_blocked (count, events, values))
	    ret = FAILURE;
	else
	    ret = SUCCESS;
	break;
	}
    }

/* Cancel the handler.  We only get here if a timeout was specified. */

ISC_reset_timer (timeout_handler, handler_arg, &user_timer, &user_handler);

return ret;
#endif /* PIPE_IS_SHRLIB */
}
#endif  /* EVENTS */
#endif  /* UNIX */

#ifndef PIPE_IS_SHRLIB
#ifdef VMS
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( V M S )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count; --count, events++, values++)
    if ((*events)->event_count >= *values)
	return FALSE;

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

return event->event_count + 1;
}

int ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

#ifndef GATEWAY
gds__wake_init();
#else
ISC_wake_init();
#endif
event->event_count = 0;
event->event_pid = getpid();

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
int	status;

++event->event_count;
ISC_wake (event->event_pid);

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
WAIT	wait;

if (!ISC_event_blocked (count, events, values))
    return 0;

wait.wait_count = count;
wait.wait_events = events;
wait.wait_values = values;
gds__thread_wait (event_test, &wait);

return 0;
}
#endif

#ifdef WIN_NT
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if (!(*events)->event_shared)
	{
	if ((*events)->event_count >= *values)
	    return FALSE;
	}
    else
	{
	if ((*events)->event_shared->event_count >= *values)
	    return FALSE;
	}

return TRUE;
}

SLONG DLL_EXPORT ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

ResetEvent ((HANDLE) event->event_handle);

if (!event->event_shared)
    return event->event_count + 1;
else
    return event->event_shared->event_count + 1;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

CloseHandle ((HANDLE) event->event_handle);
}

int DLL_EXPORT ISC_event_init (
    EVENT	event,
    int		type,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

event->event_pid = process_id = getpid();
event->event_count = 0;
event->event_type = type;
event->event_shared = NULL;

if (!(event->event_handle = ISC_make_signal (TRUE, TRUE, process_id, type)))
    return FALSE;

return TRUE;
}

int ISC_event_init_shared (
    EVENT	lcl_event,
    int		type,
    TEXT	*name,
    EVENT	shr_event,
    USHORT	init_flag)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t _ s h a r e	d	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/
TEXT	event_name [MAXPATHLEN], type_name [16];

lcl_event->event_pid = process_id = getpid();
lcl_event->event_count = 0;
lcl_event->event_type = type;
lcl_event->event_shared = shr_event;

sprintf (type_name, "_event%d", type);
make_object_name (event_name, name, type_name);
if (!(lcl_event->event_handle = CreateEvent (ISC_get_security_desc(), TRUE, FALSE, event_name)))
    return FALSE;

if (init_flag)
    {
    shr_event->event_pid = 0;
    shr_event->event_count = 0;
    shr_event->event_type = type;
    shr_event->event_handle = NULL;
    shr_event->event_shared = NULL;
    }

return TRUE;
}

int DLL_EXPORT ISC_event_post (
  EVENT event)
{
/* RDT: 20230620 - Este código usa um evento do Windows via SetEvent, para sinalizar determinada situação.
   O evento é passado como parâmetro, então a função vai chamar SetEvent, para sinalizar o mesmo. */
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/

  if (!event->event_shared)
    ++event->event_count;
  else
    ++event->event_shared->event_count;

  if (event->event_pid != process_id)
    ISC_kill (event->event_pid, event->event_type, event->event_handle);
  else
    /* RDT: 20230620 - Do que entendi, do comentário da função e da descrição de SetEvent, 
       na chamada abaixo o SetEvent informará ao scheduler do Windows, que a thread deseja
       parar sua execução. */
    SetEvent ((HANDLE) event->event_handle);

  return 0;
}

int DLL_EXPORT ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
EVENT	*ptr, *end;
HANDLE	handles [16], *handle_ptr;
SLONG	timeout, status;

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return 0;

for (ptr = events, end = events + count, handle_ptr = handles; ptr < end;)
    *handle_ptr++ = (*ptr++)->event_handle;

/* Go into wait loop */

if (micro_seconds > 0)
    timeout = micro_seconds / 1000;
else
    timeout = INFINITE;
for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	return 0;
    status = WaitForMultipleObjects ((SLONG) count, handles, TRUE, timeout);
    if (!((status >= WAIT_OBJECT_0) && (status < WAIT_OBJECT_0 + count)))
	return status;
    }
}
#endif

#ifdef NETWARE_386
#define EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/

for (; count > 0; --count, ++events, ++values)
    if ((*events)->event_count >= *values)
	return FALSE;

return TRUE;
}

SLONG ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/
SLONG	val;

EnterCritSec();
val = ExamineLocalSemaphore (event->event_semid);
while (val > 0)
    {
    WaitOnLocalSemaphore (event->event_semid);
    val--;
    }

ExitCritSec();

return event->event_count + 1;
}

void ISC_event_fini (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ f i n i	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Discard an event object.
 *
 **************************************/

if (event->event_semid == 0)
    return;

ISC_semaphore_close (event->event_semid);
}

int ISC_event_init (
    EVENT	event,
    ULONG	*semaphores,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.
 *
 **************************************/

if (semaphores == 0)
    ISC_semaphore_open (&event->event_semid, 0);
else
    event->event_semid = semaphores [semnum];
 
event->event_count = 0;

return TRUE;
}

int ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/
SLONG	val;

++event->event_count;
EnterCritSec();
val = ExamineLocalSemaphore (event->event_semid);
while (val < 0)
    {
    SignalLocalSemaphore (event->event_semid);
    val++;
    }  

ExitCritSec();

return 0;
}

int ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/
ULONG	timeout;
SLONG	val;

/* If we're not blocked, the rest is a gross waste of time */

if (!ISC_event_blocked (count, events, values))
    return SUCCESS;

for (;;)
    {
    if (!ISC_event_blocked (count, events, values))
	return SUCCESS;
    if (micro_seconds <= 0)
	WaitOnLocalSemaphore ((*events)->event_semid);
    else
	{
	timeout = micro_seconds / 1000;
	val = TimedWaitOnLocalSemaphore ((*events)->event_semid, timeout);
	if (val != 0)
	    if (ISC_event_blocked (count, events, values))
		return (FAILURE);
	}
    }
}
#endif

#ifndef REQUESTER
#ifndef EVENTS
int ISC_event_blocked (
    USHORT	count,
    EVENT	*events,
    SLONG	*values)
{
/**************************************
 *
 *	I S C _ e v e n t _ b l o c k e d	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	If a wait would block, return TRUE.
 *
 **************************************/
return 0;
}

SLONG DLL_EXPORT ISC_event_clear (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ c l e a r	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Clear an event preparatory to waiting on it.  The order of
 *	battle for event synchronization is:
 *
 *	    1.  Clear event.
 *	    2.  Test data structure for event already completed
 *	    3.  Wait on event.
 *
 **************************************/

return 0L;
}

int DLL_EXPORT ISC_event_init (
    EVENT	event,
    int		semid,
    int		semnum)
{
/**************************************
 *
 *	I S C _ e v e n t _ i n i t	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Prepare an event object for use.  Return FALSE if not
 *	supported.
 *
 **************************************/

return FALSE;
}

int DLL_EXPORT ISC_event_post (
    EVENT	event)
{
/**************************************
 *
 *	I S C _ e v e n t _ p o s t	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Post an event to wake somebody else up.
 *
 **************************************/

return 0;
}

int DLL_EXPORT ISC_event_wait (
    SSHORT	count,
    EVENT	*events,
    SLONG	*values,
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*handler_arg)
{
/**************************************
 *
 *	I S C _ e v e n t _ w a i t	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Wait on an event.
 *
 **************************************/

return 0;
}
#endif
#endif

#ifdef SUPERSERVER
#ifdef UNIX
void ISC_exception_post (
    ULONG  sig_num,
    TEXT *err_msg)
{
/**************************************
 *
 *	I S C _ e x c e p t i o n _ p o s t ( U N I X )  
 *
 **************************************
 *
 * Functional description
 *     When we got a sync exception, fomulate the error code	
 *     write it to the log file, and abort.
 *
 **************************************/
TEXT	*log_msg;

if (!SCH_thread_enter_check ()) 
    THREAD_ENTER;

if (err_msg)
    log_msg = (TEXT *) gds__alloc (strlen(err_msg)+256);

switch (sig_num)
    {
    case SIGSEGV:
	sprintf (log_msg,"%s Segmentation Fault.\n"
			"\t\tThe code attempted to access memory\n"
			"\t\twithout privilege to do so.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case SIGBUS:
	sprintf (log_msg,"%s Bus Error.\n"
			"\t\tThe code caused a system bus error.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case SIGILL:

	sprintf (log_msg,"%s Illegal Instruction.\n"
			"\t\tThe code attempted to perfrom an\n"
			"\t\tillegal operation."
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;

    case SIGFPE:
	sprintf (log_msg,"%s Floating Point Error.\n"
			"\t\tThe code caused an arithmetic exception\n"
			"\t\tor floating point exception."
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    default :
	sprintf (log_msg,"%s Unknown Exception.\n"
			"\t\tException number %d."
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg, sig_num);
	break; 
    }

if (err_msg)
    {
    gds__log (log_msg);
    gds__free (log_msg);
    }
abort ();
}
#endif /* UNIX */

#ifdef WIN_NT
ULONG ISC_exception_post (
    ULONG  except_code,
    TEXT *err_msg)
{
/**************************************
 *
 *	I S C _ e x c e p t i o n _ p o s t ( W I N _ N T )  
 *
 **************************************
 *
 * Functional description
 *     When we got a sync exception, fomulate the error code	
 *     write it to the log file, and abort. Note: We can not
 *     actually call "abort" since in windows this will cause
 *     a dialog to appear stating the obvious!  Since on NT we
 *     would not get a core file, there is actually no difference
 *     between abort() and exit(3). 
 *
 **************************************/
TEXT	*log_msg;

if (!SCH_thread_enter_check ())
    THREAD_ENTER;	

if (err_msg)
    log_msg = (TEXT *) gds__alloc (strlen(err_msg)+256);

switch (except_code) 
    {
    case EXCEPTION_ACCESS_VIOLATION:
	sprintf (log_msg,"%s Access violation.\n"
			"\t\tThe code attempted to access a virtual\n"
			"\t\taddress without privilege to do so.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
	sprintf (log_msg,"%s Datatype misalignment.\n"
			"\t\tThe attempted to read or write a value\n"
			"\t\tthat was not stored on a memory boundary.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	sprintf (log_msg,"%s Array bounds exceeded.\n"
			"\t\tThe code attempted to access an array\n"
			"\t\telement that is out of bounds.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
	sprintf (log_msg,"%s Float denormal operand.\n"
			"\t\tOne of the floating-point operands is too\n"
			"\t\tsmall to represent as a standard floating-point\n"
			"\t\tvalue.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	sprintf (log_msg,"%s Floating-point divide by zero.\n"
			"\t\tThe code attempted to divide a floating-point\n"
			"\t\tvalue by a floating-point divisor of zero.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_INEXACT_RESULT:
	sprintf (log_msg,"%s Floating-point inexact result.\n"
			"\t\tThe result of a floating-point operation cannot\n"
			"\t\tbe represented exactly as a decimal fraction.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_INVALID_OPERATION:
	sprintf (log_msg,"%s Floating-point invalid operand.\n"
			"\t\tAn indeterminant error occurred during a\n"
			"\t\tfloating-point operation.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_OVERFLOW:
	sprintf (log_msg,"%s Floating-point overflow.\n"
			"\t\tThe exponent of a floating-point operation\n"
			"\t\tis greater than the magnitude allowed.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_STACK_CHECK:
	sprintf (log_msg,"%s Floating-point stack check.\n"
			"\t\tThe stack overflowed or underflowed as the\n"
			"result of a floating-point operation.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_FLT_UNDERFLOW:
	sprintf (log_msg,"%s Floating-point underflow.\n"
			"\t\tThe exponent of a floating-point operation\n"
			"\t\tis less than the magnitude allowed.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
	sprintf (log_msg,"%s Integer divide by zero.\n"
			"\t\tThe code attempted to divide an integer value\n"
			"\t\tby an integer divisor of zero.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_INT_OVERFLOW:
	sprintf (log_msg,"%s Interger overflow.\n"
			"\t\tThe result of an integer operation caused the\n"
			"\t\tmost significant bit of the result to carry.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.", err_msg);
	break;
    case EXCEPTION_STACK_OVERFLOW:
        ERR_post (isc_exception_stack_overflow, 0);
	/* This will never be called, but to be safe it's here */
	return EXCEPTION_CONTINUE_EXECUTION;

    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_GUARD_PAGE:
	/* Pass these exception on to someone else,
           probably the OS or the debugger, since there
           isn't a dam thing we can do with them */
        free (log_msg);
	return EXCEPTION_CONTINUE_SEARCH;
    default:
	sprintf (log_msg,"%s An exception occurred that does\n"
			"\t\tnot have a description.  Exception number %X.\n"
			"\tThis exception will cause the InterBase server\n"
			"\tto terminate abnormally.",err_msg,except_code);
	break; 
    }
if (err_msg)
    {
    gds__log (log_msg);
    gds__free (log_msg);
    }
exit (3);
}

#endif /* WIN_NT */
#endif /* SUPERSERVER */

#ifdef WIN_NT
void *ISC_make_signal (
    BOOLEAN	create_flag,
    BOOLEAN	manual_reset,
    int		process_id,
    int		signal_number)
{
/**************************************
 *
 *	I S C _ m a k e _ s i g n a l		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Create or open a Windows/NT event.
 *	Use the signal number and process id
 *	in naming the object.
 *
 **************************************/
TEXT	event_name [64];

if (!signal_number)
    return CreateEvent (NULL, manual_reset, FALSE, NULL);

sprintf (event_name, "_interbase_process%u_signal%d", process_id, signal_number);
return (create_flag) ?
    CreateEvent (ISC_get_security_desc(), manual_reset, FALSE, event_name) :
    OpenEvent (EVENT_ALL_ACCESS, TRUE, event_name);
}
#endif

#ifdef APOLLO
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
UCHAR		*address;
SSHORT		name_length;
int		init_flag;
ULONG		mapped_length;
status_$t	status;

if (length < 0)
    length = -length;

if (length == 0)
    {
    /* Must be able to handle case where zero length passed in. */

    ib_fprintf (ib_stderr, "Unimplemented feature in ISC_map_file.\n");
    abort();
    }

/* Try to map file exclusively.  If successful, format it.  If it
   doesn't exist, try to create it.  */

name_length = strlen (filename);
mapped_length = length;
init_flag = FALSE;
address = (UCHAR*) ms_$mapl (filename, name_length, (SLONG) 0, (ULONG) length, 
	ms_$nr_xor_1w, ms_$wr, ms_$extend, &mapped_length, &status);

if (status.all == name_$not_found)
    {
    if (!init_routine)
	{
	error (status_vector, "ms_$mapl", status.all);
	return NULL;
	}
    address = (UCHAR*) ms_$crmapl (filename, name_length, (ULONG) 0, (ULONG) length, 
	ms_$nr_xor_1w, &status);
    }

if (!status.all)
    {
    init_flag = TRUE;
    if (init_routine)
	{
	shmem_data->sh_mem_address = address;
	shmem_data->sh_mem_length_mapped = mapped_length;
	(*init_routine) (init_arg, shmem_data, init_flag);
	}
    ms_$unmap (address, (ULONG) mapped_length, &status);
    if (!init_routine)
	{
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__unavailable;
	*status_vector++ = gds_arg_end;
	return NULL;
	}
    }
    
/* Region is known to exist, map it */

address = (UCHAR*) ms_$mapl (filename, name_length, (ULONG) 0, (ULONG) length, 
	ms_$cowriters, ms_$wr, ms_$extend, &mapped_length, &status);

if (status.all)
    {
    error (status_vector, "ms_$mapl", status.all);
    return NULL;
    }

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = mapped_length;

if (!init_flag && init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);

return address;
}
#endif

#ifdef mpexl
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT	expanded_name [48], command [64], *p, *q;
UCHAR	*address;
int	init_flag;
SLONG	status, domain, ftype, access, locking, exclusive, multiacc, nowait, disp,
	alloc, map_fid, recfm, lrecl, nrecs;
SSHORT	err, readers, writers, temp_flag;

if (length < 0)
    length = -length;

if (length == 0)
    {
    /* Must be able to handle case where zero length passed in. */

    ib_fprintf (ib_stderr, "Unimplemented feature in ISC_map_file.\n");
    abort();
    }

/* First try to create the mapped file.  Then release
   security on it so that everyone can access it. */

expanded_name [0] = ' ';
for (p = &expanded_name [1]; *filename && *filename != '.';)
    *p++ = *filename++;
temp_flag = (*filename == '.') ? TRUE : FALSE;
*p++ = '/';
for (q = lockword; *q;)
    *p++ = *q++ + 'A' - 1;
if (!temp_flag)
    for (q = MAPPED_FILE; *p = *q++; p++)
	;
*p++ = ' ';
*p = 0;

domain = (!temp_flag) ? DOMAIN_CREATE_PERM : 0 /* DOMAIN_CREATE_TEMP */;
recfm = RECFM_F;
lrecl = 1024;
nrecs = (length - 1) / lrecl + 1;
alloc = 1;
disp = (!temp_flag) ? 0 : 2 /* DISP_TEMP */;
HPFOPEN (&map_fid, &status,
    OPN_FORMAL, expanded_name,
    OPN_DOMAIN, &domain,
    OPN_RECFM, &recfm,
    OPN_LRECL, &lrecl,
    OPN_FILE_SIZE, &nrecs,
    OPN_ALLOCATE, &alloc,
    OPN_DISP, &disp,
    ITM_END);
if (!status)
    FCLOSE (map_fid, 0, 0);

if (!temp_flag)
    {
    sprintf (command, "RELEASE%s\r", expanded_name);
    HPCICOMMAND (command, &err, &err, 2);
    }

/* Now open the file for real.  Permit dynamic locking
   so that the region can be acquired and released. */

domain = (!temp_flag) ? DOMAIN_OPEN_PERM : 2 /* DOMAIN_OPEN_TEMP */;
access = ACCESS_RW;
locking = LOCKING_YES;
exclusive = EXCLUSIVE_SHARE;
disp = DISP_DELETE;
HPFOPEN (&map_fid, &status,
    OPN_FORMAL, expanded_name,
    OPN_DOMAIN, &domain,
    OPN_ACCESS_TYPE, &access,
    OPN_LOCKING, &locking,
    OPN_EXCLUSIVE, &exclusive,
    OPN_SMAP, &address,
    OPN_DISP, &disp,
    ITM_END);

if (status)
    {
    error (status_vector, "HPFOPEN", status);
    return NULL;
    }

/* Now check to see if we are the first user in.  If so,
   we'll have to initialize the mapped region. */

init_flag = FALSE;

FFILEINFO (map_fid,
    INF_LRECL, &lrecl,
    INF_FLIMIT, &nrecs,
    INF_NREADERS, &readers,
    INF_NWRITERS, &writers,
    ITM_END);

if (readers == 1 && writers == 1)
    {
    init_flag = TRUE;
    if (!init_routine)
	{
	FCLOSE (map_fid, 0, 0);
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__unavailable;
	*status_vector = gds_arg_end;
	return NULL;
	}
    FLOCK (map_fid, LOCKING_YES);
    }

length = lrecl * nrecs;

if (!ISC_comm_init (status_vector, (init_flag && !temp_flag) ? TRUE : FALSE))
    {
    /* Something failed.  Shut whatever we started down. */

    if (init_flag)
	FUNLOCK (map_fid);
    FCLOSE (map_fid, 0, 0);
    return NULL;
    }

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;
shmem_data->sh_mem_mutex_arg = map_fid;

if (init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);

if (init_flag)
    FUNLOCK (map_fid);

return address;
}
#endif

#ifdef OS2_ONLY
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT	expanded_filename [MAXPATHLEN], *p;
ULONG	status, event_handle;
int	init_flag;
UCHAR	*address;
SLONG	*header_address;

if (length < 0)
    length = -length;

if (length == 0)
    {
    /* Must be able to handle case where zero length passed in. */

    ib_fprintf (ib_stderr, "Unimplemented feature in ISC_map_file.\n");
    abort();
    }

/* Create an event that can be used to determine if someone has already
   initialized shared memory. */

event_handle = 0;
make_object_name (expanded_filename, filename, "\\SEM32\\evnt");
while (TRUE)
    {
    if (!(status = DosCreateEventSem (expanded_filename, &event_handle, 0, 0)))
	{
	init_flag = TRUE;
	break;
	}
    else if (status != ERROR_DUPLICATE_NAME)
	{
	error (status_vector, "DosCreateEventSem", status);
	return NULL;
	}
    if (!(status = DosOpenEventSem (expanded_filename, &event_handle)))
	{
	init_flag = FALSE;
	break;
	}
    else if (status != ERROR_SEM_NOT_FOUND)
	{
	error (status_vector, "DosOpenEventSem", status);
	return NULL;
	}
    }

if (init_flag && !init_routine)
    {
    DosCloseEventSem (event_handle);
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__unavailable;
    *status_vector++ = gds_arg_end;
    return NULL;
    }

/* All but the initializer will wait until the event is set.  That
   is done after initialization is complete. */

if (!init_flag)
    DosWaitEventSem (event_handle, SEM_INDEFINITE_WAIT);

/* Allocate a small amount of shared memory that will be used to
   make remapping possible.  The current length of real shared memory
   and its name are saved in it.  If we're the first one in, set
   the length of the real region.  Otherwise, get it. */

make_object_name (expanded_filename, filename, "\\SHAREMEM\\");
if (init_flag)
    {
    if (status = DosAllocSharedMem (&header_address, expanded_filename,
	    2 * sizeof (SLONG), PAG_COMMIT | PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosAllocSharedMem", status);
	DosCloseEventSem (event_handle);
	return NULL;
	}
    header_address [0] = length;
    header_address [1] = 0;
    }
else
    {
    if (status = DosGetNamedSharedMem (&header_address, expanded_filename,
	    PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosGetNamedSharedMem", status);
	DosCloseEventSem (event_handle);
	return NULL;
	}
    length = header_address [0];
    }

/* Create the real shared memory region. */

for (p = expanded_filename; *p; p++)
    ;
sprintf (p, "%d", header_address [1]);

if (init_flag)
    {
    if (status = DosAllocSharedMem (&address, expanded_filename,
	    length, PAG_COMMIT | PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosAllocSharedMem", status);
	DosFreeMem (header_address);
	DosCloseEventSem (event_handle);
	return NULL;
	}
    }
else
    if (status = DosGetNamedSharedMem (&address, expanded_filename,
	    PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosGetNamedSharedMem", status);
	DosFreeMem (header_address);
	DosCloseEventSem (event_handle);
	return NULL;
	}

*p = 0;

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;
shmem_data->sh_mem_interest = event_handle;
shmem_data->sh_mem_hdr_address = header_address;
strcpy (shmem_data->sh_mem_name, expanded_filename);

if (init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);

if (init_flag)
    DosPostEventSem (event_handle);

return address;
}
#endif

#ifdef VMS
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( V M S )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
SLONG		inadr [2], retadr [2], flags;
TEXT		section [64], *p, *q, expanded_filename [MAXPATHLEN], temp [MAXPATHLEN], hostname [64];
STATUS		status;
struct FAB	fab;
struct XABPRO	xab;
struct dsc$descriptor_s	desc;

if (length < 0)
    length = -length;

if (length == 0)
    {
    /* Must be able to handle case where zero length passed in. */

    ib_fprintf (ib_stderr, "Unimplemented feature in ISC_map_file.\n");
    abort();
    }

gds__prefix (temp, filename);
sprintf (expanded_filename, temp, ISC_get_host (hostname, sizeof (hostname)));

/* Find section name */

q = expanded_filename;

for (p = expanded_filename; *p; p++)
    if (*p == ':' || *p == ']')
	q = p + 1;

for (p = section; *q && *q != '.';)
    *p++ = *q++;

*p = 0;

/* Setup to open the file */

fab = cc$rms_fab;
fab.fab$l_fna = expanded_filename;
fab.fab$b_fns = strlen (expanded_filename);
fab.fab$l_fop = FAB$M_UFO;
fab.fab$l_alq = length / 512;
fab.fab$b_fac = FAB$M_UPD | FAB$M_PUT;
fab.fab$b_shr = FAB$M_SHRGET | FAB$M_SHRPUT | FAB$M_UPI;
fab.fab$b_rfm = FAB$C_UDF;

/* Setup to create or map the file */

inadr [0] = inadr [1] = 0;
ISC_make_desc (section, &desc, 0);
flags = SEC$M_GBL | SEC$M_EXPREG | SEC$M_WRT |
    ((shmem_data->sh_mem_system_flag) ? 0 : SEC$M_SYSGBL);

if (init_routine)
    {
    /* If we're a server, start by opening file.
       If we can't open it, create it. */

    status = sys$open (&fab);

    if (!(status & 1))
	{
	fab.fab$l_xab = &xab;
	xab = cc$rms_xabpro;
	xab.xab$w_pro = (XAB$M_NOEXE << XAB$V_SYS) | (XAB$M_NOEXE << XAB$V_OWN) |
	    (XAB$M_NOEXE << XAB$V_GRP) | (XAB$M_NODEL << XAB$V_GRP) |
	    (XAB$M_NOEXE << XAB$V_WLD) | (XAB$M_NODEL << XAB$V_WLD);

	status = sys$create (&fab);

	if (!(status & 1))
	    {
	    error (status_vector, "sys$create", status);
	    return NULL;
	    }
	}

    /* Create and map section */

    status = sys$crmpsc (
	inadr,
	retadr,
	0,		/* acmode */
	flags,		/* flags */
	&desc,		/* gsdnam */
	0,		/* ident */
	0,		/* relpag */
	fab.fab$l_stv,	/* chan */
	length / 512,
	0,		/* vbm */
	0,		/* prot */
	0);		/* pfc */

    if (!(status & 1))
	{
	if (status == SS$_CREATED)
	    sys$deltva (retadr, 0, 0);
	sys$dassgn (fab.fab$l_stv);
	error (status_vector, "sys$crmpsc", status);
	return NULL;
	}
    }
else
    {
    /* We're not a server, just map the global section */

    status = sys$mgblsc (
	inadr,
	retadr,
	0,		/* acmode */
	flags,		/* flags */
	&desc,		/* gsdnam */
	0,		/* ident */
	0);		/* relpag */

    if (!(status & 1))
	{
	error (status_vector, "sys$mgblsc", status);
	return NULL;
	}
    }

shmem_data->sh_mem_address = retadr [0];
shmem_data->sh_mem_length_mapped = length;
shmem_data->sh_mem_retadr [0] = retadr [0];
shmem_data->sh_mem_retadr [0] = retadr [1];
shmem_data->sh_mem_channel = fab.fab$l_stv;
strcpy (shmem_data->sh_mem_filename, expanded_filename);
if (init_routine)
    (*init_routine) (init_arg, shmem_data, status == SS$_CREATED);

return (UCHAR*) retadr [0];
}
#endif

#ifdef UNIX
#ifdef MMAP_SUPPORTED
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( U N I X - m m a p )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT		expanded_filename [MAXPATHLEN], hostname [64];
TEXT            tmp [MAXPATHLEN];
TEXT            init_filename [MAXPATHLEN]; /* to hold the complete filename
					      of the init file. */
UCHAR		*address;
SLONG		key, semid;
int		oldmask, fd, excl_flag;
int             fd_init; /* filedecr. for the init file */
USHORT		trunc_flag;
struct stat	file_stat;
union semun	arg;
#ifdef NeXT
kern_return_t	r;
#endif
#ifdef NO_FLOCK
struct flock	lock;
#endif

sprintf (expanded_filename, filename, ISC_get_host (hostname, sizeof (hostname)));

/* make the complete filename for the init file this file is to be used as a 
   master lock to eliminate possible race conditions with just a single file
   locking. The race condition is caused as the conversion of a EXCLUSIVE 
   lock to a SHARED lock is not atomic*/

gds__prefix_lock (tmp, INIT_FILE);
sprintf (init_filename, tmp, hostname); /* already have the hostname! */

oldmask = umask (0);
if (length < 0)
    {
    length = -length;
    trunc_flag = FALSE;
    }
else
    trunc_flag = TRUE;

/* Produce shared memory key for file */

#ifndef NeXT
if (!(key = find_key (status_vector, expanded_filename)))
    {
    umask (oldmask);
    return NULL;
    }
#endif


/* open the init lock file */
fd_init = open (init_filename, O_RDWR | O_CREAT, 0666);
if (fd_init == -1)
    {
    error (status_vector, "open", errno);
    return NULL;
    }

#ifdef NO_FLOCK
/* get an exclusive lock on the INIT file with a block */
lock.l_type = F_WRLCK;
lock.l_whence = 0;
lock.l_start = 0;
lock.l_len = 0;
if (fcntl (fd_init, F_SETLKW, &lock) == -1)
    {
    error (status_vector, "fcntl", errno);
    close (fd_init);
    return NULL;
    }
#else
/* get an flock exclusive on the INIT file with blocking */
if (flock (fd_init, LOCK_EX))
    {
    /* we failed to get an exclusive lock return back */ 
    error (status_vector, "flock", errno);
    close (fd_init);
    return NULL;
    }
#endif
/* open the file to be inited */
fd = open (expanded_filename, O_RDWR | O_CREAT, 0666);
umask (oldmask);

if (fd == -1)
    {
    error (status_vector, "open", errno);
#ifndef NO_FLOCK
    /* unlock init file */
    flock (fd_init, LOCK_UN);
#else
    lock.l_type = F_UNLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl (fd_init, F_SETLK, &lock);
#endif
    close (fd_init);
    return NULL;
    }

if (length == 0)
    {
    /* Get and use the existing length of the shared segment */

    if (fstat (fd, &file_stat) == -1)
	{
	error (status_vector, "fstat", errno);
	close (fd);
#ifndef NO_FLOCK
        /* unlock init file */
        flock (fd_init, LOCK_UN);
#else
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock);
#endif
	close (fd_init); /* while we are at it close the init file also */
	return NULL;
	}
    length = file_stat.st_size;
    }


#ifdef DECOSF
/* Try to get an exclusive lock on the lock file.  If we succeed,
   then try to set the length of the file.  Otherwise the mmap call
   that follows will fail. */

excl_flag = flock (fd, LOCK_EX | LOCK_NB);
if (!excl_flag && init_routine && trunc_flag)
    ftruncate (fd, length);
#endif

#ifdef NeXT
r = vm_allocate (task_self(), (vm_address_t*) &address, length, TRUE);
if (r != KERN_SUCCESS)
    {
    error_mach (status_vector, "vm_allocate", r);
    close (fd);
#ifndef NO_FLOCK
        /* unlock init file */
        flock (fd_init, LOCK_UN);
#else
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock)
#endif
    close (fd_init);
    return NULL;
    }
if (mmap (address, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) < 0)
    {
    error (status_vector, "mmap", errno);
    vm_deallocate (task_self(), (vm_address_t) address, length);
    close (fd);
#ifndef NO_FLOCK
        /* unlock init file */
        flock (fd_init, LOCK_UN);
#else
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock)
#endif
    close (fd_init);
    return NULL;
    }
#else
address = (UCHAR*) mmap (NULL_PTR, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

if ((U_IPTR) address == -1)
    {
    error (status_vector, "mmap", errno);
#ifdef DECOSF
    flock (fd, LOCK_UN);
#endif
    close (fd);
#ifndef NO_FLOCK
    /* unlock init file */
    flock (fd_init, LOCK_UN);
#else
    lock.l_type = F_UNLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl (fd_init, F_SETLK, &lock);
#endif

    close (fd_init);
    return NULL;
    }

/* Get semaphore for mutex */

#if !(defined SOLARIS_MT || defined POSIX_THREADS)
if (shmem_data->sh_mem_semaphores &&
    (semid = init_semaphores (status_vector, key, shmem_data->sh_mem_semaphores)) < 0)
    {
#ifdef DECOSF
    flock (fd, LOCK_UN);
#endif
    close (fd);
#ifndef NO_FLOCK
    /* unlock init file */
    flock (fd_init, LOCK_UN);
#else
    lock.l_type = F_UNLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl (fd, F_SETLK, &lock);
#endif
    close (fd_init);
    return NULL;
    }
#endif
#endif

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;

#if !(defined SOLARIS_MT || defined POSIX_THREADS)
shmem_data->sh_mem_mutex_arg = semid;
#else
shmem_data->sh_mem_mutex_arg = 0;
#endif

shmem_data->sh_mem_handle = fd;

/* Try to get an exclusive lock on the lock file.  This will
   fail if somebody else has the exclusive lock */

#ifndef NO_FLOCK
#ifndef DECOSF
if (!flock (fd, LOCK_EX | LOCK_NB))
#else
if (!excl_flag)
#endif
    {
    if (!init_routine)
	{
	/* unlock both files */
	flock (fd, LOCK_UN);
	flock (fd_init, LOCK_UN);
#else
lock.l_type = F_WRLCK;
lock.l_whence = 0;
lock.l_start = 0;
lock.l_len = 0;
if (fcntl (fd, F_SETLK, &lock) != -1)
    {
    if (!init_routine)
	{
        /* unlock the file and the init file to release the other process */
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd, F_SETLK, &lock);

        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock);
#endif
#ifndef NeXT
	munmap ((char *) address, length);
#else
	vm_deallocate (task_self(), (vm_address_t) address, length);
#endif
	close (fd);
	close (fd_init);
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__unavailable;
	*status_vector++ = gds_arg_end;
	return NULL;
	}
    if (trunc_flag)
	ftruncate (fd, length);
    (*init_routine) (init_arg, shmem_data, TRUE);
#ifndef NO_FLOCK
    if (flock (fd, LOCK_SH))
	{
	error (status_vector, "flock", errno);
	flock (fd, LOCK_UN);
	flock (fd_init, LOCK_UN);
#else
#ifdef DGUX
    /* Allow for DG bug which doesn't convert the write lock to a
       read lock.  Therefore, release the lock completely and then
       wait for a read lock. */

    lock.l_type = F_UNLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl (fd, F_SETLK, &lock) == -1)
	{
	error (status_vector, "fcntl", errno);
	munmap (address, length);
	close (fd_init);
	close (fd);
	return NULL;
	}
#endif
    lock.l_type = F_RDLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
#ifdef DGUX
    if (fcntl (fd, F_SETLKW, &lock) == -1)
#else
    if (fcntl (fd, F_SETLK, &lock) == -1)
#endif
	{
	error (status_vector, "fcntl", errno);
        /* unlock the file */
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd, F_SETLK, &lock);

        /* unlock the init file to release the other process */
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock);
#endif
#ifndef NeXT
	munmap ((char *) address, length);
#else
	vm_deallocate (task_self(), (vm_address_t) address, length);
#endif


	close (fd_init);
	close (fd);
	return NULL;
	}
    }
else
    {
#ifndef NO_FLOCK
    if (flock (fd, LOCK_SH))
	{
	error (status_vector, "flock", errno);
	flock (fd, LOCK_UN);
	flock (fd_init, LOCK_UN);
#else
     lock.l_type = F_RDLCK;
     lock.l_whence = 0;
     lock.l_start = 0;
     lock.l_len = 0;
     if (fcntl (fd, F_SETLK, &lock) == -1)
        {
	error (status_vector, "fcntl", errno);
        /* unlock the file */
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd, F_SETLK, &lock);

        /* unlock the init file to release the other process */
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        fcntl (fd_init, F_SETLK, &lock);
#endif
#ifndef NeXT
	munmap ((char *) address, length);
#else
	vm_deallocate (task_self(), (vm_address_t) address, length);
#endif
	close (fd_init);
	close (fd);
	return NULL;
	}
    if (init_routine)
	(*init_routine) (init_arg, shmem_data, FALSE);
    }

#ifndef NO_FLOCK
/* unlock the init file to release the other process */
flock(fd_init, LOCK_UN);
#else
/* unlock the init file to release the other process */
lock.l_type = F_UNLCK;
lock.l_whence = 0;
lock.l_start = 0;
lock.l_len = 0;
fcntl (fd_init, F_SETLK, &lock);
#endif
/* close the init file_decriptor */
close (fd_init);
return address;
}
#endif
#endif

#ifdef UNIX
#ifndef MMAP_SUPPORTED
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( U N I X - s h m a t )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT		expanded_filename [512], hostname [64];
UCHAR		*address;
SSHORT		count;
int		init_flag, oldmask;
SLONG		key, shmid, semid;
struct shmid_ds	buf;
IB_FILE		*fp;

#ifdef NOHOSTNAME
strcpy (expanded_filename, filename);
#else
sprintf (expanded_filename, filename, ISC_get_host (hostname, sizeof (hostname)));
#endif
oldmask = umask (0);
init_flag = FALSE;
if (length < 0)
    length = -length;

/* Produce shared memory key for file */

if (!(key = find_key (status_vector, expanded_filename)))
    {
    umask (oldmask);
    return NULL;
    }

/* Write shared memory key into expanded_filename file */

fp = ib_fopen (expanded_filename, "w");
umask (oldmask);

if (!fp)
    {
    error (status_vector, "ib_fopen", errno);
    return NULL;
    }

ib_fprintf (fp, "%ld", key);

/* Get an exclusive lock on the file until the initialization process
   is complete.  That way potential race conditions are avoided. */

#ifndef sun
if (lockf (ib_fileno (fp), F_LOCK, 0))
    {
    error (status_vector, "lockf", errno);
#else
if (flock (ib_fileno (fp), LOCK_EX))
    {
    error (status_vector, "flock", errno);
#endif
    ib_fclose (fp);
    return NULL;
    }

/* Create the shared memory region if it doesn't already exist. */

if ((shmid = shmget (key, length, IPC_CREAT | PRIV)) == -1)
#ifdef SUPERSERVER
    if (errno == EINVAL) 
        {
	/* There are two cases when shmget() returns EINVAL error:

	   - "length" is less than the system-imposed minimum or
	     greater than the system-imposed maximum.

	   - A shared memory identifier exists for "key" but the
	     size of the segment associated with it is less
	     than "length" and "length" is not equal to zero.

	   Let's find out what the problem is by getting the 
	   system-imposed limits.
	*/

#ifdef HP10
        struct pst_ipcinfo pst;
 
        if (pstat_getipc (&pst ,sizeof (struct pst_ipcinfo) , 1, 0) == -1)
	    {
	    error (status_vector, "pstat_getipc", errno);
	    ib_fclose (fp);
	    return NULL;
	    }

	if ((length < pst.psi_shmmin) || (length > pst.psi_shmmax))
	    {
	    /* If pstat_getipc did not return an error "errno"
	       is still EINVAL, exactly what we want.
	    */
	    error (status_vector, "shmget", errno);
	    ib_fclose (fp);
	    return NULL;
	    }
#endif /* HP10 */

	/* If we are here then the shared memory segment already
	   exists and the "length" we specified in shmget() is
	   bigger than the size of the existing segment. 

	   Because the segment has to exist at this point the
	   following shmget() does not have IPC_CREAT flag set.

	   We need to get shmid to delete the segment. Because we
	   do not know the size of the existing segment the easiest
	   way to get shmid is to attach to the segment with zero
	   length
	*/ 
        if ((shmid = shmget (key, 0, PRIV)) == -1)
	    {
	    error (status_vector, "shmget", errno);
	    ib_fclose (fp);
	    return NULL;
            }
 
	if (shmctl (shmid, IPC_RMID, &buf) == -1)
	    {
	    error (status_vector, "shmctl/IPC_RMID", errno);
	    ib_fclose (fp);
	    return NULL;
	    }

	/* We have just deleted shared memory segment and current
	   code fragment is an atomic operation (we are holding an
	   exclusive lock on the "isc_lock1.<machine>" file), so
	   we use IPC_EXCL flag to get an error if by some miracle
	   the sagment with the same key is already exists
	*/
	if ((shmid = shmget (key, length, IPC_CREAT | IPC_EXCL | PRIV)) == -1)
	    {
	    error (status_vector, "shmget", errno);
	    ib_fclose (fp);
	    return NULL;
	    }
	}
    else	/* if errno != EINVAL) */
#endif /* SUPERSERVER */
	{
	error (status_vector, "shmget", errno);
	ib_fclose (fp);
	return NULL;
	}

#ifdef SUPERSERVER
/* If we are here there are two possibilities:

   1. we mapped the shared memory segment of the "length" size;
   2. we mapped the segment of the size less than "length" (but
      not zero length and bigger than system-imposed minimum);

   We want shared memory segment exactly of the "length" size.
   Let's find out what the size of the segment is and if it is
   bigger than length, we remove it and create new one with the
   size "length".
   Also, if "length" is zero (that means we have already mapped
   the existing segment with the zero size) remap the segment 
   with the existing size 
*/
if (shmctl (shmid, IPC_STAT, &buf) == -1)
    {
    error (status_vector, "shmctl/IPC_STAT", errno);
    ib_fclose (fp);
    return NULL;
    }

assert (length <= buf.shm_segsz);
if (length < buf.shm_segsz)
    if (length)
	{
	if (shmctl (shmid, IPC_RMID, &buf) == -1)
	    {
	    error (status_vector, "shmctl/IPC_RMID", errno);
	    ib_fclose (fp);
	    return NULL;
	    }
	
	if ((shmid = shmget (key, length, IPC_CREAT | IPC_EXCL | PRIV)) == -1)
	    {
	    error (status_vector, "shmget", errno);
	    ib_fclose (fp);
	    return NULL;
	    }
	}
    else 
	{
	length = buf.shm_segsz;
	if ((shmid = shmget (key, length, PRIV)) == -1)
	    {
	    error (status_vector, "shmget", errno);
	    ib_fclose (fp);
	    return NULL;
	    }
	}
#else /* !SUPERSERVER */

if (length == 0)
    {
    /* Use the existing length.  This should not happen for the 
       very first attachment to the shared memory.  */

    if (shmctl (shmid, IPC_STAT, &buf) == -1)
	{
	error (status_vector, "shmctl/IPC_STAT", errno);
	ib_fclose (fp);
	return NULL;
	}
    length = buf.shm_segsz;

    /* Now remap with the new-found length */

    if ((shmid = shmget (key, length, PRIV)) == -1)
	{
	error (status_vector, "shmget", errno);
	ib_fclose (fp);
	return NULL;
	}
    }
#endif /* SUPERSERVER */


#ifdef SHMEM_PICKY
if (!next_shared_memory)
    next_shared_memory = (UCHAR*) sbrk (0) + SHMEM_DELTA;
address = (UCHAR*) shmat (shmid, next_shared_memory, SHM_RND);
if ((U_IPTR) address != -1)
#ifndef SYSV_SHMEM
    next_shared_memory = address + length;
#else
#ifndef IMP
    next_shared_memory = address + length + SHMLBA;
#else
    next_shared_memory = address + length + 0x100000;
#endif
#endif
#else
address = (UCHAR*) shmat (shmid, NULL, 0);
#endif

if ((U_IPTR) address == -1)
    {
    error (status_vector, "shmat", errno);
    ib_fclose (fp);
    return NULL;
    }

if (shmctl (shmid, IPC_STAT, &buf) == -1)
    {
    error (status_vector, "shmctl/IPC_STAT", errno);
    shmdt (address);
    next_shared_memory -= length;
    ib_fclose (fp);
    return NULL;
    }

/* Get semaphore for mutex */

#ifndef POSIX_THREADS
if (shmem_data->sh_mem_semaphores &&
    (semid = init_semaphores (status_vector, key, shmem_data->sh_mem_semaphores)) < 0)
    {
    shmdt (address);
    next_shared_memory -= length;
    ib_fclose (fp);
    return NULL;
    }
#endif /* POSIX_THREADS */

/* If we're the only one with shared memory mapped, see if
   we can initialize it.  If we can't, return failure. */

if (buf.shm_nattch == 1)
    {
    if (!init_routine)
	{
	shmdt (address);
	next_shared_memory -= length;
	ib_fclose (fp);
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__unavailable;
	*status_vector++ = gds_arg_end;
	return NULL;
	}
    buf.shm_perm.mode = 0666;
    shmctl (shmid, IPC_SET, &buf);
    init_flag = TRUE;
    }

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;

#ifndef POSIX_THREADS
shmem_data->sh_mem_mutex_arg = semid;
#else
shmem_data->sh_mem_mutex_arg = 0;
#endif

shmem_data->sh_mem_handle = shmid;

if (init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);
    
/* When the mapped file is closed here, the lock we applied for
   synchronization will be released. */

ib_fclose (fp);

return address;
}
#endif

#endif

#ifdef WIN_NT
#define ISC_MAP_FILE_DEFINED
UCHAR * DLL_EXPORT ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
TEXT	expanded_filename [MAXPATHLEN], hostname [64], *p;
TEXT	map_file [MAXPATHLEN];
HANDLE	file_handle, file_obj, event_handle, header_obj;
int	file_exists, init_flag;
UCHAR	*address;
SLONG	*header_address;
DWORD	ret_event, fdw_create;
int	retry_count = 0;

/* To maintain compatibility with the FAT file system we will truncate
   the host name to 8 characters.  Heaven help us if this creates
   an ambiguity. */


/* retry to attach to mmapped file if the process initializing
 * dies during initialization.
 */

retry:
retry_count++;

ISC_get_host (hostname, sizeof (hostname));
hostname [8] = 0;
sprintf (map_file, filename, hostname);

if (length < 0)
    length = -length;

file_handle = CreateFile (map_file,
	GENERIC_READ | GENERIC_WRITE,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_ALWAYS,
	FILE_ATTRIBUTE_NORMAL,
	NULL);
if (file_handle == INVALID_HANDLE_VALUE)
    {
    error (status_vector, "CreateFile", GetLastError());
    return NULL;
    }

/* Check if file already exists */

file_exists = (GetLastError() == ERROR_ALREADY_EXISTS) ? TRUE : FALSE;

/* Create an event that can be used to determine if someone has already
   initialized shared memory. */

make_object_name (expanded_filename, filename, "_event");
#ifndef CHICAGO_FIXED
if (!ISC_get_ostype())
    event_handle = CreateMutex (ISC_get_security_desc(), TRUE, expanded_filename);
else
#endif
event_handle = CreateEvent (ISC_get_security_desc(), TRUE, FALSE, expanded_filename);
if (!event_handle)
    {
#ifndef CHICAGO_FIXED
    if (!ISC_get_ostype())
	error (status_vector, "CreateMutex", GetLastError());
    else
#endif
    error (status_vector, "CreateEvent", GetLastError());
    CloseHandle (file_handle);
    return NULL;
    }

init_flag = (GetLastError() == ERROR_ALREADY_EXISTS) ? FALSE : TRUE;

if (init_flag && !init_routine)
    {
    CloseHandle (event_handle);
    CloseHandle (file_handle);
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__unavailable;
    *status_vector++ = gds_arg_end;
    return NULL;
    }

if (length == 0)
    {
    /* Get and use the existing length of the shared segment */

    if ((length = GetFileSize (file_handle, NULL)) == -1)
	{
	error (status_vector, "GetFileSize", GetLastError());
	CloseHandle (event_handle);
	CloseHandle (file_handle);
	return NULL;
	}
    }

/* All but the initializer will wait until the event is set.  That
 * is done after initialization is complete.
 * Close the file and wait for the event to be set or time out.
 * The file may be truncated.
 */

CloseHandle (file_handle);

if (!init_flag)
    {
    /* Wait for 10 seconds.  Then retry */

#ifndef CHICAGO_FIXED
    if (!ISC_get_ostype())
	{
	ret_event = WaitForSingleObject (event_handle, 10000);
	ReleaseMutex (event_handle);
	}
    else
#endif
    ret_event = WaitForSingleObject (event_handle, 10000);

    /* If we timed out, just retry.  It is possible that the
     * process doing the initialization died before setting the
     * event.
     */

    if (ret_event == WAIT_TIMEOUT)
        {
	CloseHandle (event_handle);
	if (retry_count > 10)
	    {
	    error (status_vector, "WaitForSingleObject", GetLastError());
	    return NULL;
	    }
        goto retry;
	}
    }

if (init_flag && file_exists)
    fdw_create = TRUNCATE_EXISTING | OPEN_ALWAYS;
else
    fdw_create = OPEN_ALWAYS;

file_handle = CreateFile (map_file,
	GENERIC_READ | GENERIC_WRITE,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	fdw_create,
	FILE_ATTRIBUTE_NORMAL,
	NULL);
if (file_handle == INVALID_HANDLE_VALUE)
    {
    CloseHandle (event_handle);
    error (status_vector, "CreateFile", GetLastError());
    return NULL;
    }

/* Create a file mapping object that will be used to make remapping possible.
   The current length of real mapped file and its name are saved in it. */

make_object_name (expanded_filename, filename, "_mapping");

header_obj = CreateFileMapping ((HANDLE) -1,
	ISC_get_security_desc(),
	PAGE_READWRITE,
	0,
	2 * sizeof (SLONG),
	expanded_filename);
if (header_obj == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    CloseHandle (event_handle);
    CloseHandle (file_handle);
    return NULL;
    }

header_address = MapViewOfFile (header_obj, FILE_MAP_WRITE, 0, 0, 0);
if (header_address == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    CloseHandle (header_obj);
    CloseHandle (event_handle);
    CloseHandle (file_handle);
    return NULL;
    }

/* Set or get the true length of the file depending on whether or not
   we are the first user. */

if (init_flag)
    {
    header_address [0] = length;
    header_address [1] = 0;
    }
else
    length = header_address [0];

/* Create the real file mapping object. */

for (p = expanded_filename; *p; p++)
    ;
sprintf (p, "%d", header_address [1]);

file_obj = CreateFileMapping (file_handle,
	ISC_get_security_desc(),
	PAGE_READWRITE,
	0,
	length,
	expanded_filename);
if (file_obj == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    UnmapViewOfFile (header_address);
    CloseHandle (header_obj);
    CloseHandle (event_handle);
    CloseHandle (file_handle);
    return NULL;
    }

address = MapViewOfFile (file_obj, FILE_MAP_WRITE, 0, 0, 0);
if (address == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    CloseHandle (file_obj);
    UnmapViewOfFile (header_address);
    CloseHandle (header_obj);
    CloseHandle (event_handle);
    CloseHandle (file_handle);
    return NULL;
    }

*p = 0;

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;
shmem_data->sh_mem_handle = file_handle;
shmem_data->sh_mem_object = file_obj;
shmem_data->sh_mem_interest = event_handle;
shmem_data->sh_mem_hdr_object = header_obj;
shmem_data->sh_mem_hdr_address = header_address;
strcpy (shmem_data->sh_mem_name, expanded_filename);

if (init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);

if (init_flag)
    {
    FlushViewOfFile (address, 0);
#ifndef CHICAGO_FIXED
    if (!ISC_get_ostype())
	ReleaseMutex (event_handle);
    else
#endif
    SetEvent (event_handle);
    if (SetFilePointer (shmem_data->sh_mem_handle, length, NULL, FILE_BEGIN) == 0xFFFFFFFF ||
	!SetEndOfFile (shmem_data->sh_mem_handle) ||
	!FlushViewOfFile (shmem_data->sh_mem_address, 0))
	{
	error (status_vector, "SetFilePointer", GetLastError());
	return NULL;
	}
    }

return address;
}
#endif

#ifdef NETWARE_386
#define ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
void		*address;
ULONG		*semaphores;
ULONG		i;
SHR_MEM_HNDL	handle;
int		init_flag;

if (length < 0)
    length = -length;

ISC_mutex_lock (SYNC_MUTEX);

for (handle = shared_memory_handles; handle != NULL; handle = handle->next)
    if (strcmp (handle->file_name, filename) == 0)
	break;

if (handle != NULL)
    init_flag = FALSE;
else
    {
    if (length == 0)
	{
	ISC_mutex_unlock (SYNC_MUTEX);
	status_vector [0] = gds_arg_gds;
	status_vector [1] = gds__virmemexh;
	status_vector [2] = gds_arg_end;
	return (NULL);
	}

    init_flag = TRUE;

    address = gds__alloc (length);
    /* FREE: ?by ISC_remap_file? */
    if (!address)	/* NOMEM: */
	{
	status_vector [0] = gds_arg_gds;
	status_vector [1] = gds__virmemexh;
	status_vector [2] = gds_arg_end;
	ISC_mutex_unlock (SYNC_MUTEX);
	return (NULL);
	}

    if (shmem_data->sh_mem_semaphores > 0)
	{
	semaphores = gds__alloc (shmem_data->sh_mem_semaphores * sizeof (ULONG));
	/* FREE: unknown */
	if (!semaphores)	/* NOMEM: */
	    {
	    gds__free (address);
	    status_vector [0] = gds_arg_gds;
	    status_vector [1] = gds__virmemexh;
	    status_vector [2] = gds_arg_end;
	    ISC_mutex_unlock (SYNC_MUTEX);
	    return (NULL);
	    }
	for (i = 0; i < shmem_data->sh_mem_semaphores; i++)
	    ISC_semaphore_open (&semaphores[i], 1);
	}
    else 
	semaphores = NULL;

    handle = gds__alloc (sizeof (struct shr_mem_hndl));
    /* FREE: ?by ISC_remap_file? */
    if (!handle)	/* NOMEM: */
	{
	gds__free (address);
	if (semaphores)
	    gds__free (semaphores);
	status_vector [0] = gds_arg_gds;
	status_vector [1] = gds__virmemexh;
	status_vector [2] = gds_arg_end;
	ISC_mutex_unlock (SYNC_MUTEX);
	return (NULL);
	}
    handle->shmem_data.sh_mem_address = address;
    handle->shmem_data.sh_mem_length_mapped = length;
    handle->shmem_data.sh_mem_mutex_arg = semaphores;
    strcpy (handle->file_name, filename);

    handle->next = shared_memory_handles;
    shared_memory_handles = handle;
    }

shmem_data->sh_mem_address = handle->shmem_data.sh_mem_address;
shmem_data->sh_mem_length_mapped = handle->shmem_data.sh_mem_length_mapped;
shmem_data->sh_mem_mutex_arg = handle->shmem_data.sh_mem_mutex_arg;

if (init_routine)
    (*init_routine) (init_arg, shmem_data, init_flag);

ISC_mutex_unlock (SYNC_MUTEX);

return (shmem_data->sh_mem_address);
}
#endif

#ifdef WINDOWS_ONLY
#define ISC_MAP_FILE_DEFINED
UCHAR * DLL_EXPORT ISC_map_file (
	STATUS	*status_vector,
	TEXT	*filename,
	void	(*init_routine)(void *, struct sh_mem *, int),
	void	*init_arg,
	SLONG	length,
	SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/
void			*address;
ULONG			*semaphores;
ULONG			i;
int				init_flag;
static ULONG	*map_semaphores;
static TEXT		map_filename [256] = {0};
static void		*map_address;

if (length < 0)
	length = -length;

/* ISC_mutex_lock (SYNC_MUTEX);	  */

if (strcmp (map_filename, filename) == 0)
	{
	address = map_address;
	semaphores = map_semaphores;
	init_flag = FALSE;
	}
else
	{
	strcpy (map_filename, filename);

	if (length == 0)
		{
		/* ISC_mutex_unlock (SYNC_MUTEX); */
		status_vector [0] = gds_arg_gds;
		status_vector [1] = gds__virmemexh;
		status_vector [2] = gds_arg_end;
		return (NULL);
		}

	init_flag = TRUE;

	address = gds__alloc (length);
        if (!address)
	{
	status_vector [0] = gds_arg_gds;
	status_vector [1] = gds__virmemexh;
	status_vector [2] = gds_arg_end;
	/* ISC_mutex_unlock (SYNC_MUTEX); */
	return (NULL);
	}
	map_address = address;

	if (shmem_data->sh_mem_semaphores > 0)
		{
		semaphores = gds__alloc (shmem_data->sh_mem_semaphores * sizeof (ULONG));
                if (!semaphores)
	            {
	            status_vector [0] = gds_arg_gds;
	            status_vector [1] = gds__virmemexh;
	            status_vector [2] = gds_arg_end;
	            /* ISC_mutex_unlock (SYNC_MUTEX); */
	            return (NULL);
	            }
		for (i = 0; i < shmem_data->sh_mem_semaphores; i++)
			ISC_semaphore_open (&semaphores[i], 1);
		}
	else
		semaphores = NULL;
	map_semaphores = semaphores;
	}

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = length;
shmem_data->sh_mem_mutex_arg = semaphores;

if (init_routine)
	(*init_routine) (init_arg, shmem_data, init_flag);

/* ISC_mutex_unlock (SYNC_MUTEX); */

return (shmem_data->sh_mem_address);
}
#endif


#ifndef REQUESTER
#ifndef ISC_MAP_FILE_DEFINED
UCHAR *ISC_map_file (
    STATUS	*status_vector,
    TEXT	*filename,
    void	(*init_routine)(void *, struct sh_mem *, int),
    void	*init_arg,
    SLONG	length,
    SH_MEM	shmem_data)
{
/**************************************
 *
 *	I S C _ m a p _ f i l e		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Try to map a given file.  If we are the first (i.e. only)
 *	process to map the file, call a given initialization
 *	routine (if given) or punt (leaving the file unmapped).
 *
 **************************************/

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__unavailable;
*status_vector++ = gds_arg_end;

return NULL;
}
#endif
#endif

#ifdef MMAP_SUPPORTED
#define ISC_MAP_OBJECT_DEFINED
UCHAR *ISC_map_object (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	object_offset,
    SLONG	object_length)
{
/**************************************
 *
 *	I S C _ m a p _ o b j e c t	
 *
 **************************************
 *
 * Functional description
 *	Try to map an object given a file mapping.
 *
 **************************************/
UCHAR		*address;
int		fd;
SLONG		page_size, start, end, length;

/* Get system page size as this is the unit of mapping. */

#if (defined SOLARIS || defined NCR3000)
if ((page_size = sysconf (_SC_PAGESIZE)) == -1)
    {
    error (status_vector, "sysconf", errno);
    return NULL;
    }
#else
if ((page_size = (int) getpagesize ()) == -1)
    {
    error (status_vector, "getpagesize", errno);
    return NULL;
    }
#endif

/* Compute the start and end page-aligned offsets which
   contain the object being mapped. */

start = (object_offset / page_size) * page_size;
end = (((object_offset + object_length) / page_size) + 1) * page_size;
length = end - start;
fd = shmem_data->sh_mem_handle;

address = (UCHAR*) mmap (NULL_PTR, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, start);

if ((U_IPTR) address == -1)
    {
    error (status_vector, "mmap", errno);
    return NULL;
    }

/* Return the virtual address of the mapped object. */

return (address + (object_offset - start));
}
#endif

#ifdef MMAP_SUPPORTED
#define ISC_UNMAP_OBJECT_DEFINED
BOOLEAN ISC_unmap_object (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    UCHAR	**object_pointer,
    SLONG	object_length)
{
/**************************************
 *
 *	I S C _ u n m a p _ o b j e c t	
 *
 **************************************
 *
 * Functional description
 *	Try to unmap an object given a file mapping.
 *	Zero the object pointer after a successful unmap.
 *
 **************************************/
SLONG		page_size, length;
UCHAR		*start, *end;

/* Get system page size as this is the unit of mapping. */

#if (defined SOLARIS || defined NCR3000)
if ((page_size = sysconf (_SC_PAGESIZE)) == -1)
    {
    error (status_vector, "sysconf", errno);
    return NULL;
    }
#else
if ((page_size = (int) getpagesize ()) == -1)
    {
    error (status_vector, "getpagesize", errno);
    return NULL;
    }
#endif

/* Compute the start and end page-aligned addresses which
   contain the mapped object. */

start = (UCHAR*) ((U_IPTR) *object_pointer & ~(page_size - 1));
end = (UCHAR*) ((U_IPTR) ((*object_pointer + object_length) + (page_size - 1)) & ~(page_size - 1));
length = end - start;

if (munmap ((char *) start, length) == -1)
    {
    error (status_vector, "munmap", errno);
    return FALSE;
    }

*object_pointer = NULL_PTR;
return TRUE;
}
#endif

#ifdef APOLLO
#define MUTEX
int ISC_mutex_init (
    mutex_$lock_rec_t	*mutex,
    SLONG		dummy)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

/* for the DN10000 parallel processing machine, we
   need to use the apollo mutexes because they can
   guarantee an atomic operation whereas our own cannot */

mutex_$init (mutex);

return 0;
}

int ISC_mutex_lock (
    mutex_$lock_rec_t	*mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

mutex_$lock (mutex, mutex_$wait_forever);

return 0;
}

int ISC_mutex_unlock (
    mutex_$lock_rec_t	*mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

mutex_$unlock (mutex);

return 0;
}
#endif

#ifdef mpexl 
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	fn_port)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( M P E / X L ) 
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

*mutex = fn_port;

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( M P E / X L ) 
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

return ISC_file_lock ((SSHORT) *mutex);
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( M P E / X L ) 
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

return ISC_file_unlock ((SSHORT) *mutex);
}
#endif

#ifdef NeXT
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	dummy)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

mutex_init (mutex);

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

mutex_lock (mutex);

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

mutex_unlock (mutex);

return 0;
}
#endif

#ifdef VMS
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	event_flag)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

mutex->mtx_use_count = 0;
mutex->mtx_event_count [0] = 0;
mutex->mtx_event_count [1] = event_flag;
mutex->mtx_wait = 0;

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
SLONG	bit, status;

bit = 0;
++mutex->mtx_wait;

if (lib$bbssi (&bit, mutex->mtx_event_count))
    for (;;)
	{
	if (!lib$bbssi (&bit, mutex->mtx_event_count))
	    break;
	gds__thread_wait (mutex_test, mutex);
	}

--mutex->mtx_wait;

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( V M S )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/
SLONG	status, bit;

bit = 0;
lib$bbcci (&bit, mutex->mtx_event_count);
#ifndef __ALPHA
sys$wake (0, 0);
#else
THREAD_wakeup();
#endif

return 0;
}
#endif

#ifdef SOLARIS_MT
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	semaphore)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

return mutex_init (mutex->mtx_mutex, USYNC_PROCESS, NULL);
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
int		state;

for (;;)
    {
    state = mutex_lock (mutex->mtx_mutex);
    if (!state)
	break;
    if (!SYSCALL_INTERRUPTED(state))
       	return state;
    }

return 0;
}

int ISC_mutex_lock_cond (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k _ c o n d	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Conditionally sieze a mutex.
 *
 **************************************/
int		state;

for (;;)
    {
    state = mutex_trylock (mutex->mtx_mutex);
    if (!state)
	break;
    if (!SYSCALL_INTERRUPTED(state))
       	return state;
    }

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/
int		state;

for (;;)
    {
    /* Note use of undocumented lwp_mutex_unlock call 
     * due to Solaris 2.4 bug */
    state = _lwp_mutex_unlock (mutex->mtx_mutex);
    if (!state)
       	break;
    if (!SYSCALL_INTERRUPTED(state))
       	return state;
    }

return 0;
}
#endif	/* SOLARIS_MT */

#ifdef POSIX_THREADS
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	semaphore)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/
int	state;

#if (!defined HP10 && !defined linux)

pthread_mutexattr_t     mattr;

state = pthread_mutexattr_init (&mattr);
if (state)
    return state;
pthread_mutexattr_setpshared (&mattr, PTHREAD_PROCESS_SHARED);
state = pthread_mutex_init (mutex->mtx_mutex, &mattr);
pthread_mutexattr_destroy (&mattr);
return state;

#else

/* NOTE: HP's Posix threads implementation does not support thread
	 synchronization in different processes. Thus the following
	 fragment is just a temp. decision we could be use for super-
	 server (until we are to implement local IPC using shared
	 memory in which case we need interprocess thread sync.
*/
#ifdef linux
return pthread_mutex_init (mutex->mtx_mutex, NULL);
#else
state = pthread_mutex_init (mutex->mtx_mutex, pthread_mutexattr_default);
if (!state)
    return 0;
assert (state == -1);	/* if state is not 0, it should be -1 */
return errno;

#endif /* linux */
#endif /* HP10 */

}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
#ifdef HP10

int		state;

state = pthread_mutex_lock (mutex->mtx_mutex);
if (!state)
    return 0;
assert (state == -1);	/* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_lock (mutex->mtx_mutex);

#endif /* HP10 */
}

int ISC_mutex_lock_cond (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k _ c o n d	( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Conditionally sieze a mutex.
 *
 **************************************/
#ifdef HP10

int		state;

state = pthread_mutex_trylock (mutex->mtx_mutex);

/* HP's interpretation of return codes is different than Solaris
   (and Posix Standard?). Usually in case of error they return
   -1 and set errno to whatever error number is.
   pthread_mutex_trylock() is a special case:

	return	errno	description
	  1		Success
	  0		mutex has alreary been locked. Could not get it
	 -1	EINVAL	invalid value of mutex
*/
if (!state)
    return -99;		/* we did not get the mutex for it had already      */
			/* been locked, let's return something which is     */
			/* not zero and negative (errno values are positive)*/
if (state == 1)
    return 0;

assert (state == -1);	/* if state is not 0 or 1, it should be -1 */
return errno;

#else

return pthread_mutex_trylock (mutex->mtx_mutex);

#endif /* HP10 */
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( P O S I X _ T H R E A D S )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/
#ifdef HP10

int		state;

state = pthread_mutex_unlock (mutex->mtx_mutex);
if (!state)
    return 0;
assert (state == -1);	/* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_unlock (mutex->mtx_mutex);

#endif /* HP10 */
}
#endif /* POSIX_THREADS */

#ifdef UNIX
#ifndef MUTEX
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	semaphore)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/
int		state;
union semun	arg;

mutex->mtx_semid = semaphore;
mutex->mtx_semnum = 0;

arg.val = 1;
state = semctl ((int) semaphore, 0, SETVAL, arg);
if (state == -1)
    return errno;

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
int		state;
struct sembuf	sop;

sop.sem_num = mutex->mtx_semnum;
sop.sem_op = -1;
sop.sem_flg = SEM_UNDO;

for (;;)
    {
    state = semop (mutex->mtx_semid, &sop, 1);
    if (state != -1)	
	break;
    if (!SYSCALL_INTERRUPTED(errno))
	return errno;
    }

return 0;
}

int ISC_mutex_lock_cond (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k _ c o n d	( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 *
 **************************************
 *
 * Functional description
 *	Conditionally sieze a mutex.
 *
 **************************************/
int		state;
struct sembuf	sop;

sop.sem_num = mutex->mtx_semnum;
sop.sem_op = -1;
sop.sem_flg = SEM_UNDO | IPC_NOWAIT;

for (;;)
    {
    state = semop (mutex->mtx_semid, &sop, 1);
    if (state != -1)	
	break;
    if (!SYSCALL_INTERRUPTED(errno))
	return errno;
    }

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( U N I X )
 *                                             not NeXT
 *                                             not SOLARIS
 *                                             not POSIX_THREADS
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/
int		state;
struct sembuf	sop;

sop.sem_num = mutex->mtx_semnum;
sop.sem_op = 1;
sop.sem_flg = SEM_UNDO;

for (;;)
    {
    state = semop (mutex->mtx_semid, &sop, 1);
    if (state != -1)	
	break;
    if (!SYSCALL_INTERRUPTED(errno))
	return errno;
    }

return 0;
}
#endif
#endif

#ifdef OS2_ONLY
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    TEXT	*mutex_name)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/
TEXT	name_buffer [MAXPATHLEN];
ULONG	status;

make_object_name (name_buffer, mutex_name, "\\SEM32\\mutx");
mutex->mtx_handle = 0;
while (TRUE)
    {
    status = DosCreateMutexSem (name_buffer, &mutex->mtx_handle, 0, 0);
    if (!status || status != ERROR_DUPLICATE_NAME)
	break;
    status = DosOpenMutexSem (name_buffer, &mutex->mtx_handle);
    if (!status || status != ERROR_SEM_NOT_FOUND)
	break;
    }

return status;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
ULONG	status;

status = DosRequestMutexSem (mutex->mtx_handle, SEM_INDEFINITE_WAIT);

return (!status || status == ERROR_SEM_OWNER_DIED) ? 0 : status;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

return DosReleaseMutexSem (mutex->mtx_handle);
}
#endif

#ifdef WIN_NT
#define MUTEX
int DLL_EXPORT ISC_mutex_init (
    MTX		mutex,
    TEXT	*mutex_name)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/
TEXT	name_buffer [MAXPATHLEN];

make_object_name (name_buffer, mutex_name, "_mutex");
mutex->mtx_handle = CreateMutex (ISC_get_security_desc(), FALSE, name_buffer);

return (mutex->mtx_handle) ? 0 : 1;
}

int DLL_EXPORT ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/
SLONG	status;

status = WaitForSingleObject (mutex->mtx_handle, INFINITE);

return (!status || status == WAIT_ABANDONED) ? 0 : 1;
}

int DLL_EXPORT ISC_mutex_lock_cond (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k _ c o n d	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Conditionally sieze a mutex.
 *
 **************************************/
SLONG	status;

status = WaitForSingleObject (mutex->mtx_handle, 0L);

return (!status || status == WAIT_ABANDONED) ? 0 : 1;
}

int DLL_EXPORT ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

return !ReleaseMutex (mutex->mtx_handle);
}
#endif
 
#ifdef NETWARE_386
#define MUTEX
int ISC_mutex_init (
    MTX		mutex,
    ULONG	*semaphores)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( N E T W A R E  _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

(*mutex) = semaphores [0];

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( N E T W A R E  _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

WaitOnLocalSemaphore (*mutex);

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( N E T W A R E  _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

SignalLocalSemaphore (*mutex);

return 0;
}
#endif
 
#ifdef WINDOWS_ONLY
#define MUTEX
int DLL_EXPORT ISC_mutex_lock (
	struct mtx *m)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

return 0;
}

int DLL_EXPORT ISC_mutex_init (
	struct mtx 	*mutex,
	ULONG		*dummy)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

return 0;
}

int DLL_EXPORT ISC_mutex_unlock (
	struct mtx	*mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

return 0;
}
#endif
 
#ifndef MUTEX
int ISC_mutex_init (
    MTX		mutex,
    SLONG	dummy)
{
/**************************************
 *
 *	I S C _ m u t e x _ i n i t	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex.
 *
 **************************************/

return 0;
}

int ISC_mutex_lock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ l o c k	( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Sieze a mutex.
 *
 **************************************/

return 0;
}

int ISC_mutex_unlock (
    MTX		mutex)
{
/**************************************
 *
 *	I S C _ m u t e x _ u n l o c k		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Release a mutex.
 *
 **************************************/

return 0;
}
#endif

#ifdef NETWARE_386
#define ISC_REMAP_FILE_DEFINED
UCHAR *ISC_remap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	new_length,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ r e m a p _ f i l e		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Try to re-map a given region.
 *
 **************************************/
void		*address;
SHR_MEM_HNDL	handle;

ISC_mutex_lock (SYNC_MUTEX);

/* First find the global handle with the address of the passed region. */

for (handle = shared_memory_handles; handle != NULL; handle = handle->next)
    if (handle->shmem_data.sh_mem_address == shmem_data->sh_mem_address)
	break;

if (handle == NULL)
    {
    /* Some error message */

    ISC_mutex_unlock (SYNC_MUTEX);
    return NULL;
    }

address = gds__alloc (new_length);
if (!address)	/* NOMEM: */
    {
    status_vector [0] = gds_arg_gds;
    status_vector [1] = gds__virmemexh;
    status_vector [2] = gds_arg_end;
    ISC_mutex_unlock (SYNC_MUTEX);
    return (NULL);
    }

memcpy (address, shmem_data->sh_mem_address, shmem_data->sh_mem_length_mapped);
gds__free (shmem_data->sh_mem_address);
handle->shmem_data.sh_mem_address = shmem_data->sh_mem_address = address;
handle->shmem_data.sh_mem_length_mapped = shmem_data->sh_mem_length_mapped = new_length;

ISC_mutex_unlock (SYNC_MUTEX);

return address;
}
#endif

#ifdef OS2_ONLY
#define ISC_REMAP_FILE_DEFINED
UCHAR *ISC_remap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	new_length,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ r e m a p _ f i l e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Try to re-map a given file.
 *
 **************************************/
TEXT	expanded_filename [MAXPATHLEN];
UCHAR	*address;
ULONG	status;

/* THE CURRENTLY IMPLEMENTED MECHANISM FOR EXPANDING SHARED MEMORY
   ON OS/2 DOESN'T WORK.  UNTIL SOMEONE CAN DEBUG IT, IT IS BEING
   DISABLED AND THE DEFAULT SHARED MEMORY SIZE IS BEING SET TO 96K. */

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__unavailable;
*status_vector++ = gds_arg_end;

return NULL;

sprintf (expanded_filename, "%s%d", shmem_data->sh_mem_name,
    shmem_data->sh_mem_hdr_address [1] + 1);

if (flag)
    {
    if (status = DosAllocSharedMem (&address, expanded_filename,
	    new_length, PAG_COMMIT | PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosAllocSharedMem", status);
	return NULL;
	}

    /* We're the one expanding memory so copy the existing
       region into the new one. */

    memcpy (address, shmem_data->sh_mem_address, shmem_data->sh_mem_length_mapped);
    }
else
    if (status = DosGetNamedSharedMem (&address, expanded_filename,
	    PAG_READ | PAG_WRITE))
	{
	error (status_vector, "DosGetNamedSharedMem", status);
	return NULL;
	}

if (flag)
    {
    shmem_data->sh_mem_hdr_address [0] = new_length;
    shmem_data->sh_mem_hdr_address [1]++;
    }

DosFreeMem (shmem_data->sh_mem_address);

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = new_length;

return address;
}
#endif

#ifdef UNIX
#ifdef MMAP_SUPPORTED
#define ISC_REMAP_FILE_DEFINED
UCHAR *ISC_remap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	new_length,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ r e m a p _ f i l e		( U N I X - m m a p )
 *
 **************************************
 *
 * Functional description
 *	Try to re-map a given file.
 *
 **************************************/
UCHAR	*address;

if (flag)
    ftruncate (shmem_data->sh_mem_handle, new_length);

address = (UCHAR*) mmap (NULL_PTR, new_length, PROT_READ | PROT_WRITE, MAP_SHARED,
    shmem_data->sh_mem_handle, 0);
if ((U_IPTR) address == -1)
    return NULL;

#ifndef NeXT
munmap ((char *) shmem_data->sh_mem_address, shmem_data->sh_mem_length_mapped);
#else
vm_deallocate (task_self(), (vm_address_t) shmem_data->sh_mem_address,
    shmem_data->sh_mem_length_mapped);
#endif

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = new_length;

return address;
}
#endif
#endif

#ifdef WIN_NT
#define ISC_REMAP_FILE_DEFINED
UCHAR * DLL_EXPORT ISC_remap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	new_length,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ r e m a p _ f i l e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Try to re-map a given file.
 *
 **************************************/
TEXT	expanded_filename [MAXPATHLEN];
HANDLE	file_obj;
UCHAR	*address;

if (flag)
    if (SetFilePointer (shmem_data->sh_mem_handle, new_length, NULL, FILE_BEGIN) == 0xFFFFFFFF ||
	!SetEndOfFile (shmem_data->sh_mem_handle) ||
	!FlushViewOfFile (shmem_data->sh_mem_address, 0))
	{
	error (status_vector, "SetFilePointer", GetLastError());
	return NULL;
	}

/* If the remap file exists, remap does not occur correctly.
 * The file number is local to the process and when it is 
 * incremented and a new filename is created, that file may
 * already exist.  In that case, the file is not expanded.
 * This will happen when the file is expanded more than once
 * by concurrently running processes.
 * 
 * The problem will be fixed by making sure that a new file name
 * is generated with the mapped file is created.
 */

while (1)
    {
    sprintf (expanded_filename, "%s%d", shmem_data->sh_mem_name,
	shmem_data->sh_mem_hdr_address [1] + 1);

    file_obj = CreateFileMapping (shmem_data->sh_mem_handle,
	    ISC_get_security_desc(),
	    PAGE_READWRITE,
	    0,
	    new_length,
	    expanded_filename);

    if (!((GetLastError () == ERROR_ALREADY_EXISTS) && flag))
	break;

    CloseHandle (file_obj);
    shmem_data->sh_mem_hdr_address [1] ++;
    }

if (file_obj == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    return NULL;
    }

address = MapViewOfFile (file_obj, FILE_MAP_WRITE, 0, 0, 0);
if (address == NULL)
    {
    error (status_vector, "CreateFileMapping", GetLastError());
    CloseHandle (file_obj);
    return NULL;
    }

if (flag)
    {
    shmem_data->sh_mem_hdr_address [0] = new_length;
    shmem_data->sh_mem_hdr_address [1]++;
    }

UnmapViewOfFile (shmem_data->sh_mem_address);
CloseHandle (shmem_data->sh_mem_object);

shmem_data->sh_mem_address = address;
shmem_data->sh_mem_length_mapped = new_length;
shmem_data->sh_mem_object = file_obj;

return address;
}
#endif

#ifndef ISC_REMAP_FILE_DEFINED
UCHAR * DLL_EXPORT ISC_remap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    SLONG	new_length,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ r e m a p _ f i l e		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Try to re-map a given file.
 *
 **************************************/

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__unavailable;
*status_vector++ = gds_arg_end;

return NULL;
}
#endif

#if (defined UNIX || defined APOLLO)
void ISC_reset_timer (
    void	(*timeout_handler)(),
    void	*timeout_arg,
    SLONG	*client_timer,
    void	**client_handler)
{
/**************************************
 *
 *	I S C _ r e s e t _ t i m e r
 *
 **************************************
 *
 * Functional description
 *	Clear a previously established timer and restore
 *	the previous context.
 *
 **************************************/
#ifndef SYSV_SIGNALS
struct itimerval	internal_timer;
#endif

ISC_signal_cancel (SIGALRM, timeout_handler, timeout_arg);

/* Cancel the timer, then restore the previous handler and alarm */

#ifdef SYSV_SIGNALS
alarm (0);
sigset (SIGALRM, *client_handler);
alarm (*client_timer);
#else
timerclear (&internal_timer.it_interval);
timerclear (&internal_timer.it_value);
setitimer (ITIMER_REAL, &internal_timer, NULL);
#ifndef SIGACTION_SUPPORTED
sigvector (SIGALRM, client_handler, NULL);
#else
sigaction (SIGALRM, client_handler, NULL);
#endif
setitimer (ITIMER_REAL, client_timer, NULL);
#endif
}
#endif

#if (defined UNIX || defined APOLLO)
void ISC_set_timer (
    SLONG	micro_seconds,
    void	(*timeout_handler)(),
    void	*timeout_arg,
    SLONG	*client_timer,
    void	**client_handler)
{
/**************************************
 *
 *	I S C _ s e t _ t i m e r
 *
 **************************************
 *
 * Functional description
 *	Set a timer for the specified amount of time.
 *
 **************************************/
#ifndef SYSV_SIGNALS
struct itimerval	internal_timer;
#ifndef SIGACTION_SUPPORTED
struct sigvec		internal_handler;
#else
struct sigaction	internal_handler;
#endif
#endif
SLONG			d1;
void			*d2;

/* Start by cancelling any existing timer */

#ifdef SYSV_SIGNALS
if (!client_timer)
    client_timer = &d1;
*client_timer = alarm (0);
#else
timerclear (&internal_timer.it_interval);
timerclear (&internal_timer.it_value);
setitimer (ITIMER_REAL, &internal_timer, (struct itimerval *) client_timer);
#endif

/* Now clear the signal handler while saving the existing one */

#ifdef SYSV_SIGNALS
if (!client_handler)
    client_handler = &d2;
*client_handler = (void *) sigset (SIGALRM, SIG_DFL);
#else
#ifndef SIGACTION_SUPPORTED
internal_handler.sv_handler = SIG_DFL;
internal_handler.sv_mask = 0;
internal_handler.sv_flags = SV_INTERRUPT;
sigvector (SIGALRM, &internal_handler, (struct sigvec*) client_handler);
#else
internal_handler.sa_handler = (SIG_FPTR) SIG_DFL;
memset (&internal_handler.sa_mask, 0, sizeof (internal_handler.sa_mask));
internal_handler.sa_flags = SA_RESTART;
sigaction (SIGALRM, &internal_handler, (struct sigaction*) client_handler);
#endif
#endif

if (!micro_seconds)
    return;

/* Next set the new alarm handler and finally set the new timer */

ISC_signal (SIGALRM, timeout_handler, timeout_arg);

#ifdef SYSV_SIGNALS
if (micro_seconds < 1000000)
    alarm (1);
else
    alarm (micro_seconds / 1000000);
#else
if (micro_seconds < 1000000)
    internal_timer.it_value.tv_usec = micro_seconds;
else
    {
    internal_timer.it_value.tv_sec = micro_seconds / 1000000;
    internal_timer.it_value.tv_usec = micro_seconds % 1000000;
    }
setitimer (ITIMER_REAL, &internal_timer, NULL);
#endif
}
#endif

#ifdef SUPERSERVER
#ifdef UNIX
void ISC_sync_signals_set ()
{
/**************************************
 *
 *	I S C _ s y n c _ s i g n a l s _ s e t( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Set all the synchronous signals for a particular thread
 *
 **************************************/

sigset (SIGILL, longjmp_sig_handler);
sigset (SIGFPE, longjmp_sig_handler);
sigset (SIGBUS, longjmp_sig_handler);
sigset (SIGSEGV, longjmp_sig_handler);
}
#endif /* UNIX */
#endif /* SUPERSERVER */

#ifdef SUPERSERVER
#ifdef UNIX
void ISC_sync_signals_reset ()
{
/**************************************
 *
 *	I S C _ s y n c _ s i g n a l s _ r e s e t ( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Reset all the synchronous signals for a particular thread
 * to default.
 *
 **************************************/

sigset (SIGILL, SIG_DFL);
sigset (SIGFPE, SIG_DFL);
sigset (SIGBUS, SIG_DFL);
sigset (SIGSEGV, SIG_DFL);
}
#endif /* UNIX */
#endif /* SUPERSERVER */
#ifdef NETWARE_386
void ISC_semaphore_close (
    ULONG	semid)
{
/**************************************
 *
 *	I S C _ s e m a p h o r e _ c l o s e	(N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Unregister and close the passed semaphore. 
 *
 **************************************/

if (semid)
    {
    gds__unregister_cleanup (cleanup_semaphores, semid);
    CloseLocalSemaphore (semid);
    }
}

void ISC_semaphore_open (
    ULONG	*semid,
    ULONG	init_value
    )
{
/**************************************
 *
 *	I S C _ s e m a p h o r e _ o p e n 	(N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Open and register a semaphore. 
 *
 **************************************/

*semid = OpenLocalSemaphore (init_value);
gds__register_cleanup (cleanup_semaphores, *semid);
}
#endif


#ifdef WINDOWS_ONLY
void ISC_semaphore_open(
	ULONG	*semid,
	ULONG	init_value
	)
{
/**************************************
 *
 *	I S C _ s e m a p h o r e _ o p e n 	( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *	Open and register a semaphore.
 *
 **************************************/

/*** *semid = OpenLocalSemaphore (init_value);
gds__register_cleanup (cleanup_semaphores, *semid);	  ***/
}
#endif


#ifdef NETWARE_386
void ISC_sync_init (void)
{
/**************************************
 *
 *	I S C _ s y n c _ i n i t		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Initialize a mutex for this module.
 *
 **************************************/

ISC_semaphore_open (SYNC_MUTEX, 1);
}
#endif

#ifdef mpexl
#define UNMAP_FILE
void ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Detach from the shared memory.  Depending upon the flag,
 *	get rid of the semaphore and/or get rid of shared memory.
 *
 **************************************/

FCLOSE (shmem_data->sh_mem_mutex_arg, 0, 0);
}
#endif

#ifdef NETWARE_386
#define UNMAP_FILE
void ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Detach from the shared memory.  Depending upon the flag,
 *	get rid of the semaphore and/or get rid of shared memory.
 *
 **************************************/
SHR_MEM_HNDL	handle, *handle_ptr;
ULONG		i;

ISC_mutex_lock (SYNC_MUTEX);

if (flag & ISC_SEM_REMOVE)
    {
    for (i = 0; i < shmem_data->sh_mem_semaphores; i++)
	{
	ISC_semaphore_close (shmem_data->sh_mem_mutex_arg [i]);
	shmem_data->sh_mem_mutex_arg [i] = 0;
	}
    if (shmem_data->sh_mem_semaphores > 0)
	gds__free (shmem_data->sh_mem_mutex_arg);
    shmem_data->sh_mem_mutex_arg = NULL;
    }

if (flag & ISC_MEM_REMOVE)
    {
    handle_ptr = &shared_memory_handles;
    for (; handle = *handle_ptr; handle_ptr = &handle->next)
	if (handle->shmem_data.sh_mem_address == shmem_data->sh_mem_address)
	    {
	    *handle_ptr = handle->next;
	    gds__free (handle);
	    break;
	    }

    gds__free (shmem_data->sh_mem_address);
    }

ISC_mutex_unlock (SYNC_MUTEX);

}
#endif

#ifdef OS2_ONLY
#define UNMAP_FILE
void ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Detach from the shared memory.  Depending upon the flag,
 *	get rid of the semaphore and/or get rid of shared memory.
 *
 **************************************/

DosCloseEventSem (shmem_data->sh_mem_interest);
DosFreeMem (shmem_data->sh_mem_address);
DosFreeMem (shmem_data->sh_mem_hdr_address);
}
#endif

#ifdef UNIX
#ifdef MMAP_SUPPORTED
#define UNMAP_FILE
void ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( U N I X - m m a p )
 *
 **************************************
 *
 * Functional description
 *	Unmap a given file.  Depending upon the flag,
 *	get rid of the semaphore and/or truncate the file.
 *
 **************************************/
union semun	arg;

#ifndef NeXT
munmap ((char *)shmem_data->sh_mem_address, shmem_data->sh_mem_length_mapped);
#else
vm_deallocate (task_self(), (vm_address_t) shmem_data->sh_mem_address,
					 shmem_data->sh_mem_length_mapped);
#endif

#ifndef NeXT
if (flag & ISC_SEM_REMOVE)
    semctl (shmem_data->sh_mem_mutex_arg, 0, IPC_RMID, arg); 
#endif
if (flag & ISC_MEM_REMOVE)
    ftruncate (shmem_data->sh_mem_handle, 0L);
close (shmem_data->sh_mem_handle);
}
#endif
#endif

#ifdef UNIX
#ifndef MMAP_SUPPORTED
#define UNMAP_FILE
void ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( U N I X - s h m a t )
 *
 **************************************
 *
 * Functional description
 *	Detach from the shared memory.  Depending upon the flag,
 *	get rid of the semaphore and/or get rid of shared memory.
 *
 **************************************/
struct shmid_ds	buf;
union semun	arg;

shmdt (shmem_data->sh_mem_address);
if (flag & ISC_SEM_REMOVE)
    semctl (shmem_data->sh_mem_mutex_arg, 0, IPC_RMID, arg); 
if (flag & ISC_MEM_REMOVE)
    shmctl (shmem_data->sh_mem_handle, IPC_RMID, &buf);
}
#endif
#endif

#ifdef WIN_NT
#define UNMAP_FILE
void DLL_EXPORT ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Detach from the shared memory.  Depending upon the flag,
 *	get rid of the semaphore and/or get rid of shared memory.
 *
 **************************************/

CloseHandle (shmem_data->sh_mem_interest);
CloseHandle ((HANDLE) shmem_data->sh_mem_mutex_arg);
UnmapViewOfFile (shmem_data->sh_mem_address);
CloseHandle (shmem_data->sh_mem_object);
if (flag & ISC_MEM_REMOVE)
    if (SetFilePointer (shmem_data->sh_mem_handle, 0, NULL, FILE_BEGIN) != 0xFFFFFFFF)
	SetEndOfFile (shmem_data->sh_mem_handle);
CloseHandle (shmem_data->sh_mem_handle);
UnmapViewOfFile (shmem_data->sh_mem_hdr_address);
CloseHandle (shmem_data->sh_mem_hdr_object);
}
#endif

#ifdef  WINDOWS_ONLY
#define UNMAP_FILE
void DLL_EXPORT ISC_unmap_file (
    STATUS      *status_vector,
    SH_MEM      shmem_data,
    USHORT      flag)
{
/**************************************
 *
 *      I S C _ u n m a p _ f i l e             ( W I N D O W S )
 *
 **************************************
 *
 * Functional description
 *      unmap a given file or shared memory.
 *
 **************************************/

if ( shmem_data->sh_mem_address)
    gds__free( (SLONG*)( shmem_data->sh_mem_address)); 
if ( shmem_data->sh_mem_mutex_arg)
    gds__free( (SLONG*)( shmem_data->sh_mem_mutex_arg));
shmem_data->sh_mem_address = NULL;
shmem_data->sh_mem_mutex_arg = NULL;
}
#endif

#ifndef UNMAP_FILE
void DLL_EXPORT ISC_unmap_file (
    STATUS	*status_vector,
    SH_MEM	shmem_data,
    USHORT	flag)
{
/**************************************
 *
 *	I S C _ u n m a p _ f i l e		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	unmap a given file or shared memory.
 *
 **************************************/

*status_vector++ = gds_arg_gds;
*status_vector++ = gds__unavailable;
*status_vector++ = gds_arg_end;
}
#endif

#ifdef UNIX
static void alarm_handler (void)
{
/**************************************
 *
 *	a l a r m _ h a n d l e r	( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Handle an alarm clock interrupt.  
 *
 **************************************/	
}
#endif

#ifdef NETWARE_386
static void cleanup_semaphores (
    ULONG	semid)
{
/**************************************
 *
 *	c l e a n u p _ s e m a p h o r e s	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Close an open semaphore.
 *
 **************************************/

if (semid != 0)
    CloseLocalSemaphore (semid);
}
#endif

#ifndef WINDOWS_ONLY
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

#ifdef VMS
static int event_test (
    WAIT	*wait)
{
/**************************************
 *
 *	e v e n t _ t e s t
 *
 **************************************
 *
 * Functional description
 *	Callback routine from thread package for VMS.  Returns
 *	TRUE if wait is satified, otherwise FALSE.
 *
 **************************************/

return !ISC_event_blocked (wait->wait_count, wait->wait_events, wait->wait_values);
}
#endif

#ifdef NeXT
static void event_wakeup (void)
{
/**************************************
 *
 *	e v e n t _ w a k e u p
 *
 **************************************
 *
 * Functional description
 *	Signal handler to wakeup from external events.
 *
 **************************************/

ISC_mutex_lock (ISC_MUTEX);
condition_broadcast (ISC_EVENT);
ISC_mutex_unlock (ISC_MUTEX);
}
#endif

#ifdef UNIX
#ifndef NeXT
static SLONG find_key (
    STATUS	*status_vector,
    TEXT	*filename)
{
/**************************************
 *
 *	f i n d _ k e y 
 *
 **************************************
 *
 * Functional description
 *	Find the semaphore/shared memory key for a file.
 *
 **************************************/
int		fd;
SLONG		key;

/* Produce shared memory key for file */

if ((key = ftok (filename, FTOK_KEY)) == -1)
    {
    if ((fd = open (filename, O_RDWR | O_CREAT | O_TRUNC, PRIV)) == -1)
	{
	error (status_vector, "open", errno);
	return NULL;
	}
    close (fd);
    if ((key = ftok (filename, FTOK_KEY)) == -1)
	{
	error (status_vector, "ftok", errno);
	return NULL;
	}
    }

return key;
}
#endif
#endif

#ifdef UNIX
#ifndef NeXT
static SLONG init_semaphores (
    STATUS	*status_vector,
    SLONG	key,
    int		semaphores)
{
/**************************************
 *
 *	i n i t _ s e m a p h o r e s		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Create or find a block of semaphores.
 *
 **************************************/
SLONG		semid;

semid = semget (key, semaphores, IPC_CREAT | IPC_EXCL | PRIV);

if (semid == -1)
    {
    semid = semget (key, semaphores, PRIV);
    if (semid == -1)
	error (status_vector, "semget", errno);
    }

return semid;
}
#endif
#endif

#ifdef SUPERSERVER
#ifdef UNIX
void longjmp_sig_handler (
    int		sig_num)
{
/**************************************
 *
 *	l o n g j m p _ s i g _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	The generic signal handler for all signals in a thread.
 *
 **************************************/
TDBB	tdbb;

/* Note: we can only do this since we know that we
   will only be going to JRD, specifically fun and blf.
   If we were to make this generic, we would need to 
   actally hang the sigsetjmp menber off of THDD, and
   make sure that it is set properly for all sub-systems. */

tdbb = GET_THREAD_DATA; 

siglongjmp (tdbb->tdbb_sigsetjmp, sig_num); 
}
#endif /* UNIX */
#endif /* SUPERSERVER */

#ifdef VMS
static BOOLEAN mutex_test (
    MTX		mutex)
{
/**************************************
 *
 *	m u t e x _ t e s t
 *
 **************************************
 *
 * Functional description
 *	Callback routine from thread package for VMS.  Returns
 *	TRUE if mutex has been granted, otherwise FALSE.
 *
 **************************************/

return (mutex->mtx_event_count [0] & 1) ? FALSE : TRUE;
}
#endif

#ifdef OS2_ONLY
static void make_object_name (
    TEXT	*buffer,
    TEXT	*object_name,
    TEXT	*object_type)
{
/**************************************
 *
 *	m a k e _ o b j e c t _ n a m e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Create an object name from a name and type.
 *	Also replace the file separator with "_".
 *
 **************************************/
TEXT	*p, *q, *r, c;

strcpy (buffer, object_type);
p = buffer + strlen (object_type);
sprintf (p, object_name, "local");
if (strlen (buffer) > 255)
    {
    q = p + strlen (p) - (255 - (p - buffer));
    r = p;
    while (*r++ = *q++)
	;
    }
for (; c = *p; p++)
    if (c == '/' || c == '\\' || c == ':')
	*p = '_';
}
#endif

#ifdef WIN_NT
static void make_object_name (
    TEXT	*buffer,
    TEXT	*object_name,
    TEXT	*object_type)
{
/**************************************
 *
 *	m a k e _ o b j e c t _ n a m e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Create an object name from a name and type.
 *	Also replace the file separator with "_".
 *
 **************************************/
TEXT	hostname [64], *p, c;

sprintf (buffer, object_name, ISC_get_host (hostname, sizeof (hostname)));
for (p = buffer; c = *p; p++)
    if (c == '/' || c == '\\' || c == ':')
	*p = '_';
strcpy (p, object_type);
}
#endif

#ifdef UNIX
#ifndef NeXT
static BOOLEAN semaphore_wait (
    int		count,
    int		semid,
    int		*semnums)
{
/**************************************
 *
 *	s e m a p h o r e _ w a i t 
 *
 **************************************
 *
 * Functional description
 *	Wait on the given semaphores.  Return FAILURE if
 *	interrupted (including timeout) before any
 *	semaphore was poked else return SUCCESS.
 *	
 **************************************/
int		i, ret;
struct sembuf	semops [16], *semptr;

for (semptr = semops, i = 0; i < count; ++semptr, i++)
    {
    semptr->sem_op = 0;
    semptr->sem_flg = 0;
    semptr->sem_num = *semnums++;
    }
ret = semop (semid, semops, count);
 
if (ret == -1 && SYSCALL_INTERRUPTED(errno))
    return FAILURE;
else
    return SUCCESS;
}
#endif
#endif

#endif /* PIPE_IS_SHRLIB */
