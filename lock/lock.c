/*
 *	PROGRAM:	JRD Lock Manager
 *	MODULE:		lock.c
 *	DESCRIPTION:	Generic ISC Lock Manager
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

#include "../jrd/time.h"
#include "../jrd/ib_stdio.h"
#include "../jrd/common.h"
#include "../jrd/thd.h"
#include "../jrd/isc.h"
#ifdef LINKS_EXIST
#include "../isc_lock/lock.h"
#include "../isc_lock/lock_proto.h"
#else
#include "../lock/lock.h"
#include "../lock/lock_proto.h"
#endif /* LINKS_EXIST */  
#include "../jrd/codes.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_i_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/thd_proto.h"
#include <errno.h>

#ifdef WINDOWS_ONLY
#include "../jrd/loiter.h"
#define getpid          GetCurrentTask
#endif
   
#ifdef sparc
#ifndef SOLARIS
#include <vfork.h>
#endif
#endif

#ifdef NeXT
#include <mach/message.h>
#include <mach/port.h>
#include <mach/kern_return.h>
#endif

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"

#define MUTEX		lock_manager_mutex
#endif

#if ( defined DELTA || defined IMP )
#define waitpid(x,y,z)	wait (y)
#endif

#ifdef NETWARE_386
#define getpid		GetNLMID
#define USE_EVENTS
#endif

#ifdef OS2_ONLY
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#include <os2.h>
#include <process.h>
#define MUTEX		lock_manager_mutex
#define USE_EVENTS
#endif

#if (defined UNIX && !defined NeXT)
#define USE_EVENTS
#ifndef ANY_THREADING   /* #if !(defined SOLARIS || defined POSIX_THREADS) */
#define STATIC_SEMAPHORES
#endif /* ANY_THREADING */
#endif /* (defined UNIX && !defined NeXT) */

#ifdef WIN_NT
#include <process.h>
#define MUTEX		lock_manager_mutex
#define ERRNO		GetLastError()
#endif

#ifdef MANAGER_PROCESS
#include <sys/stat.h>
#define statistics	stat
static BOOLEAN	LOCK_post_manager;
#endif

#ifdef DEV_BUILD
#define ASSERT_ACQUIRED current_is_active_owner (TRUE,  __LINE__)
#define ASSERT_RELEASED current_is_active_owner (FALSE, __LINE__)
#define VALIDATE_LOCK_TABLE
#if ((defined MMAP_SUPPORTED) && !(defined SUPERSERVER))
#define LOCK_DEBUG_ACQUIRE
#endif
#endif

#ifdef LOCK_DEBUG_ACQUIRE
#define DEBUG_ACQUIRE_INTERVAL 5000
static ULONG	debug_acquire_count = 0;
#endif

#ifndef ASSERT_ACQUIRED
#define	ASSERT_ACQUIRED	/* nothing */
#endif

#ifndef ASSERT_RELEASED
#define	ASSERT_RELEASED	/* nothing */
#endif

#ifdef DEV_BUILD
#define CHECK(x)	{ if (!(x)) bug_assert ("consistency check", __LINE__); }
#endif

#ifndef CHECK
#define CHECK(x)	/* nothing */
#endif

#ifdef DEBUG
#define DEBUG_MANAGER "manager"	
#define DEBUG_TRACE
#endif

#ifdef DEBUG_TRACE
#define LOCK_TRACE(x)	{ time_t t; (void) time (&t); ib_printf ("%s", ctime(&t) ); ib_printf x ; ib_fflush (ib_stdout); }
#endif

#ifdef DEBUG
SSHORT	LOCK_debug_level = 0;
#define DEBUG_MSG(l,x)	if ((l) <= LOCK_debug_level) { time_t t; (void) time (&t); ib_printf ("%s", ctime(&t) ); ib_printf x ; ib_fflush (ib_stdout); gds__log x ; }
#endif

#ifndef DEBUG_MSG
#define DEBUG_MSG(l,x)	/* nothing */
#endif

#ifndef LOCK_TRACE
#define LOCK_TRACE(x)	/* nothing */
#endif

/* Debug delay is used to create nice big windows for signals or other
 * events to occur in - eg: slow down the code to try and make
 * timing race conditions show up
 */
#ifdef DEBUG
#define	DEBUG_DELAY	debug_delay (__LINE__)
#endif

#ifndef DEBUG_DELAY
#define DEBUG_DELAY	/* nothing */
#endif

#ifndef ERRNO
#define ERRNO		errno
#endif

#ifndef MUTEX
#define MUTEX		LOCK_header->lhb_mutex
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	256
#endif

#ifndef SV_INTERRUPT
#define SV_INTERRUPT	0
#endif

#define DUMMY_OWNER_CREATE	((PTR) -1)
#define DUMMY_OWNER_DELETE	((PTR) -2)
#define DUMMY_OWNER_SHUTDOWN	((PTR) -3)

#if !(defined linux || defined NETWARE_386 || defined WIN_NT || defined OS2_ONLY || defined mpexl)
extern SCHAR	*sys_errlist[];
#endif

static void	acquire (PTR);
static UCHAR	*alloc (SSHORT, STATUS *);
static LBL	alloc_lock (USHORT, STATUS *);
static USHORT	alloc_semaphore (OWN, STATUS *);
static void	blocking_action (PTR);
static void     blocking_action2 (PTR, PTR);
static void THREAD_ROUTINE blocking_action_thread (PTR *);
static void	bug (STATUS *, CONST TEXT *);
#ifdef DEV_BUILD
static void	bug_assert (CONST TEXT *, ULONG);
#endif
static BOOLEAN	convert (PTR, UCHAR, SSHORT, FPTR_INT, int *, STATUS *);
static USHORT	create_owner (STATUS *, SLONG, UCHAR, SLONG *);
#ifdef DEV_BUILD
static void	current_is_active_owner (BOOLEAN, ULONG);
#endif
static void	deadlock_clear (void);
static LRQ	deadlock_scan (OWN, LRQ);
static LRQ	deadlock_walk (LRQ, BOOLEAN *);
static void	dequeue (PTR);
#ifdef DEBUG
static void	debug_delay (ULONG);
#endif
static void	exit_handler (void *);
static LBL	find_lock (PTR, USHORT, UCHAR *, USHORT, USHORT *);
static USHORT	fork_lock_manager (STATUS *);
static OWN	get_manager (BOOLEAN);
static LRQ	get_request (PTR);
static void	grant (LRQ, LBL);
static PTR	grant_or_que (LRQ, LBL, SSHORT);
static STATUS	init_lock_table (STATUS *);
static void	init_owner_block (OWN, UCHAR, ULONG, USHORT);
static void	lock_alarm_handler (EVENT);
static void	lock_initialize (void *, SH_MEM, int);
static void	insert_data_que (LBL);
static void	insert_tail (SRQ, SRQ);
static USHORT	lock_state (LBL);
static void	port_waker (PTR *);
static void	post_blockage (LRQ, LBL, BOOLEAN);
static void	post_history (USHORT, PTR, PTR, PTR, BOOLEAN);
static void	post_pending (LBL);
static void	post_wakeup (OWN);
#ifndef SUPERSERVER
static BOOLEAN	probe_owners (PTR);
#endif
static void	purge_owner (PTR, OWN);
static void	remove_que (SRQ);
static void	release (PTR);
static void	release_mutex (void);
static void	release_request (LRQ);
static void	release_semaphore (OWN);
static void	shutdown_blocking_thread (STATUS *);
static int      signal_owner (OWN, PTR);
#ifdef VALIDATE_LOCK_TABLE
static void	validate_lhb (LHB);
static void	validate_shb (PTR);
static void	validate_owner (PTR, USHORT);
static void	validate_lock (PTR, USHORT, PTR);
static void	validate_request (PTR, USHORT, USHORT);
static void	validate_block (PTR);
#endif
static USHORT	wait_for_request (LRQ, SSHORT, STATUS *);
static void	wakeup_action (PTR *);

static struct own	LOCK_process_owner;  /* Place holder */
static SSHORT	LOCK_bugcheck = 0;
static LHB	VOLATILE LOCK_header = NULL;
static PTR	LOCK_owner_offset = 0;
static OWN	LOCK_owner = NULL_PTR;
static SSHORT	VOLATILE LOCK_asts = -1;
static int	LOCK_pid = 0, LOCK_version = 0;
static SLONG	LOCK_shm_size = 0, LOCK_sem_count = 0;
#if (defined SOLARIS_MT && !defined SUPERSERVER)
static SLONG    LOCK_solaris_stall = 60; /* seconds - def value for solaris stall */
#endif
static SLONG	LOCK_block_signal = 0;
#ifdef WAKEUP_SIGNAL
static SLONG	LOCK_wakeup_signal = 0;
#endif
static SLONG	LOCK_ordering = 0;
static SLONG	LOCK_hash_slots;
static SLONG	LOCK_scan_interval;
static SLONG	LOCK_acquire_spins = 0;
static SH_MEM_T	LOCK_data;
static BOOLEAN	start_manager = FALSE;
static	TEXT	LOCK_bug_buffer [128];

#ifdef mpexl
static MTX_T	lock_manager_mutex [1];
#endif

#ifdef NeXT
static int	condition_set;		/* flag used between threads */
static port_t	alarm_port;		/* alarm thread's port */
static SCHAR	message_buf [24];	/* used to send messages to alarm thd */
static SCHAR	message_buf1 [24];	/* used to receive above messages */
static msg_header_t *null_msg;
static msg_header_t *null_msg1;
static int	VOLATILE getout = 0;	/* flag used to stop alarm and blocking
					   threads from continuing after the
					   main thread shuts down (LOCK_fini) */
#endif

#ifdef OS2_ONLY
static MTX_T	lock_manager_mutex [1];
static HEV	blocking_event [1];
#endif

#ifdef WIN_NT
static MTX_T	lock_manager_mutex [1];
#ifndef SUPERSERVER
static HANDLE	blocking_event [1], wakeup_event [1];
#endif
#endif

static struct ipccfg	LOCK_hdrtbl [] = {
/* 5.5 SCO Port: Classic server on SCO - following is not supported */
#if (!(defined WIN_NT) && !(defined SCO_EV))
	"V4_LOCK_MEM_SIZE",	-1,	&LOCK_shm_size,		0,	0,
	"ANY_LOCK_MEM_SIZE",	-1,	&LOCK_shm_size,		-1,	0,
#endif
#ifdef STATIC_SEMAPHORES
	"V4_LOCK_SEM_COUNT",	-1,	&LOCK_sem_count,	0,	0,
	"ANY_LOCK_SEM_COUNT",	-1,	&LOCK_sem_count,	-1,	0,
#endif
	"V4_LOCK_SIGNAL",	-1,	&LOCK_block_signal,	0,	0,
	"ANY_LOCK_SIGNAL",	-1,	&LOCK_block_signal,	-1,	0,
	"V4_LOCK_GRANT_ORDER",	-1,	&LOCK_ordering,		0,	0,
	"LOCK_HASH_SLOTS",	-1,	&LOCK_hash_slots,	0,	0,
	"DEADLOCK_TIMEOUT",	-1,	&LOCK_scan_interval,	0,	0,
	"LOCK_ACQUIRE_SPINS",	-1,	&LOCK_acquire_spins,	0,	0,
#if (defined SOLARIS_MT && !defined SUPERSERVER)
        "V4_SOLARIS_STALL_VALUE",-1,	&LOCK_solaris_stall,   	0,      0,
#endif
	NULL,			0,	NULL,			0,	0
};

#ifndef mpexl
#define GET_TIME	time (NULL)
#else
#define GET_TIME	TIMER()
#endif

#define WAIT_TIME	10
#define HASH_MIN_SLOTS	101
#define HASH_MAX_SLOTS	2048
#define HISTORY_BLOCKS	256
#define LOCKMANTIMEOUT	300

#if (defined SOLARIS_MT && !defined SUPERSERVER)
#define STARVATION_THRESHHOLD	500	/* acquires of lock table */
#define SOLARIS_MIN_STALL       0
#define SOLARIS_MAX_STALL       200
#define SOLARIS_MAX_STALL       200	/* Seconds */
#endif

#define OWN_BLOCK_new		1
#define OWN_BLOCK_reused	2
#define OWN_BLOCK_dummy		3

#ifndef LOCK_MANAGER
#define LOCK_MANAGER	"bin/gds_lock_mgr"
#endif

#ifdef SHLIB_DEFS
#define sys_errlist	(*_libgds_sys_errlist)
#ifdef IMP
#define wait		(*_libgds_wait)
#else
#define waitpid		(*_libgds_waitpid)
#endif
#define execl		(*_libgds_execl)
#define _exit		(*_libgds__exit)
#define statistics	(*_libgds_stat)
#define chmod		(*_libgds_chmod)

extern SCHAR		*sys_errlist [];
#ifdef IMP
extern int		wait();
#else
extern int		waitpid();
#endif
extern int		execl();
extern void		_exit();
extern int		statistics();
extern int		chmod();
#endif

static CONST UCHAR	compatibility[] = {

/*				Shared	Prot	Shared	Prot
		none	null	 Read	Read	Write	Write	Exclusive */

/* none */	1,	  1,	   1,	  1,	  1,	  1,	  1,
/* null */	1,	  1,	   1,	  1,	  1,	  1,	  1,
/* SR */	1,	  1,	   1,     1,	  1,	  1,	  0,
/* PR */	1,	  1,	   1,	  1,	  0,	  0,	  0,
/* SW */	1,	  1,	   1,	  0,	  1,	  0,	  0,
/* PW */	1,	  1,	   1,	  0,	  0,	  0,	  0,
/* EX */	1,	  1,	   0,	  0,	  0,	  0,	  0 };

#define COMPATIBLE(st1, st2)	compatibility [st1 * LCK_max + st2]

int LOCK_convert (
    PTR		request_offset,
    UCHAR	type,
    SSHORT	lck_wait,
    int		(*ast_routine)(void *),
    void	*ast_argument,
    STATUS	*status_vector)
{
/**************************************
 *
 *	L O C K _ c o n v e r t
 *
 **************************************
 *
 * Functional description
 *	Perform a lock conversion, if possible.
 *
 **************************************/
LRQ	request;
OWN	owner;
LBL	lock;

#ifdef  WINDOWS_ONLY
lck_wait = 0;		/* LIBS is always no wait */
#endif

LOCK_TRACE (("LOCK_convert (%d, %d)\n", type, lck_wait));

request = get_request (request_offset);
owner = (OWN) ABS_PTR (request->lrq_owner);
if (!owner->own_count)
    return FALSE;

acquire (request->lrq_owner);
owner = NULL;					/* remap */
++LOCK_header->lhb_converts;
request = (LRQ) ABS_PTR (request_offset);	/* remap */
lock = (LBL) ABS_PTR (request->lrq_lock);
if (lock->lbl_series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [lock->lbl_series];
else
    ++LOCK_header->lhb_operations [0];

return (convert (request_offset, type, lck_wait, ast_routine, ast_argument, status_vector));
}

int LOCK_deq (
    PTR		request_offset)
{
/**************************************
 *
 *	L O C K _ d e q 
 *
 **************************************
 *
 * Functional description
 *	Release an outstanding lock.
 *
 **************************************/
LRQ	request;
OWN	owner;
PTR	owner_offset;
LBL	lock;

LOCK_TRACE (("LOCK_deq (%d)\n", request_offset));

request = get_request (request_offset);
owner_offset = request->lrq_owner;
owner = (OWN) ABS_PTR (owner_offset);
if (!owner->own_count)
    return FALSE;

acquire (owner_offset);
owner = NULL;					/* remap */
++LOCK_header->lhb_deqs;
request = (LRQ) ABS_PTR (request_offset);	/* remap */
lock = (LBL) ABS_PTR (request->lrq_lock);
if (lock->lbl_series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [lock->lbl_series];
else
    ++LOCK_header->lhb_operations [0];

dequeue (request_offset);
release (owner_offset);
return TRUE;
}

UCHAR LOCK_downgrade (
    PTR		request_offset,
    STATUS	*status_vector)
{
/**************************************
 *
 *	L O C K _ d o w n g r a d e
 *
 **************************************
 *
 * Functional description
 *	Downgrade an existing lock returning
 *	its new state.
 *
 **************************************/
LBL	lock;
LRQ	request, pending;
SRQ	que;
UCHAR	state, pending_state;
OWN	owner;
PTR	owner_offset;

LOCK_TRACE (("LOCK_downgrade (%d)\n", request_offset));

request = get_request (request_offset);
owner_offset = request->lrq_owner;
owner = (OWN) ABS_PTR (owner_offset);
if (!owner->own_count)
    return FALSE;

acquire (owner_offset);
owner = NULL;				/* remap */
++LOCK_header->lhb_downgrades;

request = (LRQ) ABS_PTR (request_offset);/* Re-init after a potential remap */
lock = (LBL) ABS_PTR (request->lrq_lock);
pending_state = LCK_none;

/* Loop thru requests looking for pending conversions
   and find the highest requested state */

QUE_LOOP (lock->lbl_requests, que)
    {
    pending = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
    if (pending->lrq_flags & LRQ_pending && pending != request)
	{
	pending_state = MAX (pending->lrq_requested, pending_state);
	if (pending_state == LCK_EX)
	    break;
	}
    }

for (state = request->lrq_state;
     state > LCK_none && !COMPATIBLE (pending_state, state);
     --state);

if (state == LCK_none || state == LCK_null)
    {
    dequeue (request_offset);
    release (owner_offset);
    state = LCK_none;
    }
else
    convert (request_offset, state, FALSE,
	     request->lrq_ast_routine, request->lrq_ast_argument, status_vector);

return state;
}

SLONG LOCK_enq (
    PTR		prior_request,
    PTR		parent_request,
    USHORT	series,
    UCHAR	*value,
    USHORT	length,
    UCHAR	type,
    int		(*ast_routine)(void *),
    void	*ast_argument,
    SLONG	data,
    SSHORT	lck_wait,
    STATUS	*status_vector,
    PTR		owner_offset)
{
/**************************************
 *
 *	L O C K _ e n q
 *
 **************************************
 *
 * Functional description
 *	Enque on a lock.  If the lock can't be granted immediately,
 *	return an event count on which to wait.  If the lock can't
 *	be granted because of deadlock, return NULL.
 *
 **************************************/
LBL	lock;
LRQ	request;
OWN	owner;
PTR	parent, request_offset;
UCHAR	*p;
SLONG	lock_id;
USHORT	hash_slot, *ps;
SSHORT	l;


#ifdef  WINDOWS_ONLY
lck_wait = 0;		/* LIBS is always no-wait */
#endif

LOCK_TRACE (("LOCK_enq (%d)\n", parent_request));

owner = (OWN) ABS_PTR (owner_offset);
if (!owner_offset || !owner->own_count)
    return 0;

acquire (owner_offset);
owner = NULL;				/* remap */

ASSERT_ACQUIRED;
++LOCK_header->lhb_enqs;

#ifdef VALIDATE_LOCK_TABLE
if ((LOCK_header->lhb_enqs % 50) == 0)
    validate_lhb (LOCK_header);
#endif

if (prior_request)
    dequeue (prior_request);

if (parent_request)
    {
    request = get_request (parent_request);
    parent = request->lrq_lock;
    }
else
    parent = 0;

/* Allocate or reuse a lock request block */

ASSERT_ACQUIRED;
if (QUE_EMPTY (LOCK_header->lhb_free_requests))
    {
    if (!(request = (LRQ) alloc (sizeof (struct lrq), status_vector)))
	{
	release (owner_offset);
	return 0;
	}
    }
else
    {
    ASSERT_ACQUIRED;
    request = (LRQ) ((UCHAR*) QUE_NEXT (LOCK_header->lhb_free_requests) - 
	OFFSET (LRQ, lrq_lbl_requests));
    remove_que (&request->lrq_lbl_requests);
    }

owner = (OWN) ABS_PTR (owner_offset);/* Re-init after a potential remap */
post_history (his_enq, owner_offset, NULL, REL_PTR (request), TRUE);

request->lrq_type = type_lrq;
request->lrq_flags = 0;
request->lrq_requested = type;
request->lrq_state = LCK_none;
request->lrq_data = 0;
request->lrq_owner = owner_offset;
request->lrq_ast_routine = ast_routine;
request->lrq_ast_argument = ast_argument;
insert_tail (&owner->own_requests, &request->lrq_own_requests);
QUE_INIT (request->lrq_own_blocks);

/* See if the lock already exits */

/* #ifdef WINDOWS_ONLY */
/* while the access is denied, wait around. If LOITER_loiter signals
   to break out of the lock, return with an error indicator. */
/*
while (lock = find_lock (parent, series, value, length, &hash_slot))
    if (!LOITER_loiter ())
	{
	ASSERT_RELEASED;
	return 0;
	}

#else
*/
if (lock = find_lock (parent, series, value, length, &hash_slot))
    {
    if (series < LCK_MAX_SERIES)
        ++LOCK_header->lhb_operations [series];
    else
        ++LOCK_header->lhb_operations [0];
    
    insert_tail (&lock->lbl_requests, &request->lrq_lbl_requests);
    request->lrq_data = data;
    if (!(lock_id = grant_or_que (request, lock, lck_wait)))
	{
	*status_vector++ = gds_arg_gds;
	*status_vector++ = (lck_wait > 0) ? gds__deadlock :
			((lck_wait < 0) ? gds__lock_timeout : gds__lock_conflict);
	*status_vector++ = gds_arg_end;
	}
    ASSERT_RELEASED;
    return lock_id;
    }
/* #endif */

/* Lock doesn't exist.  Allocate lock block and set it up. */

request_offset = REL_PTR (request);

if (!(lock = alloc_lock (length, status_vector)))
    {
    /* lock table is exhausted */
    /* release request gracefully */
    remove_que (&request->lrq_own_requests);
    request->lrq_type = type_null;
    insert_tail (&LOCK_header->lhb_free_requests, &request->lrq_lbl_requests);
    release (owner_offset); 
    return 0;
    }
lock->lbl_state = type;
lock->lbl_parent = parent;
lock->lbl_series = series;

/* Maintain lock series data queue */

QUE_INIT (lock->lbl_lhb_data);
if (lock->lbl_data = data)
   insert_data_que (lock);

if (series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [series];
else
    ++LOCK_header->lhb_operations [0];

#ifdef NeXT
lock->lbl_eventcount = 0;
#endif
lock->lbl_flags = 0;
lock->lbl_pending_lrq_count = 0;

#ifndef mpexl
for (l = LCK_max, ps = lock->lbl_counts; l--;)
    *ps++ = 0;
#else
for (l = LCK_max, p = lock->lbl_counts; l--;)
    *p++ = 0;
#endif

if (lock->lbl_length = length)
    {
    p = lock->lbl_key;
    do *p++ = *value++; while (--length);
    }

request = (LRQ) ABS_PTR (request_offset);

QUE_INIT (lock->lbl_requests);
ASSERT_ACQUIRED;
insert_tail (&LOCK_header->lhb_hash [hash_slot], &lock->lbl_lhb_hash);
insert_tail (&lock->lbl_requests, &request->lrq_lbl_requests);
request->lrq_lock = REL_PTR (lock);
grant (request, lock);
lock_id = REL_PTR (request);
release (request->lrq_owner);

return lock_id;
}

void LOCK_fini (
    STATUS	*status_vector,
    PTR		*owner_offset)
{
/**************************************
 *
 *	L O C K _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Release the owner block and any outstanding locks.
 *	The exit handler will unmap the shared memory.
 *
 **************************************/
OWN		owner;
PTR		offset;

LOCK_TRACE (("LOCK_fini(%ld)\n", *owner_offset));

offset = *owner_offset;
owner = (OWN) ABS_PTR (offset);
if (!offset || !owner->own_count)
    return;

if (--owner->own_count > 0 || !LOCK_header)
    return;

#ifndef SUPERSERVER
#if (defined WIN_NT || defined OS2_ONLY || defined NEXT || defined SOLARIS_MT)
shutdown_blocking_thread (status_vector);
#else
#ifdef MMAP_SUPPORTED
if (LOCK_owner)
    { 
    ISC_unmap_object (status_vector, &LOCK_data, &LOCK_owner, sizeof (struct own));
    LOCK_owner_offset=0;
    }
#endif
#endif
LOCK_owner = NULL_PTR;
#endif

#ifndef NeXT
if (LOCK_header->lhb_active_owner != offset)
#endif
    {
    acquire (offset);
    owner = (OWN) ABS_PTR (offset);	/* Re-init after a potential remap */
    }

if (LOCK_pid == owner->own_process_id)
    purge_owner (offset, owner);

release_mutex();

#if !(defined NETWARE_386 || defined WIN_NT || defined OS2_ONLY || defined SOLARIS_MT || POSIX_THREADS)
ISC_signal_cancel (LOCK_block_signal, blocking_action, (void*) offset);
#endif

*owner_offset = NULL;
}

int LOCK_init (
    STATUS	*status_vector,
    SSHORT	owner_flag,
    SLONG	owner_id,
    UCHAR	owner_type,
    SLONG	*owner_handle)
{
/**************************************
 *
 *	L O C K _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize lock manager for the given owner, if not already done.
 *
 *	Initialize an owner block in the lock manager, if not already
 *	initialized.
 *
 *	Return the offset of the owner block through owner_handle.
 *
 *	Return SUCCESS or FAILURE.
 *
 **************************************/
OWN		owner;
#ifdef NeXT
cthread_t	port_waker_c_thread;
#endif
#if (defined OS2_ONLY || defined SOLARIS_MT)
ULONG		status;
#endif

LOCK_TRACE (("LOCK_init (ownerid=%ld)\n", owner_id));

/* If everything is already initialized, just bump the use count. */

if (*owner_handle)
    {
    owner = (OWN) ABS_PTR (*owner_handle);
    owner->own_count++;
    return SUCCESS;
    }

if (!LOCK_header)
    {
    /* We haven't yet mapped the shared region.  Do so now. */

    start_manager = FALSE;
    if (init_lock_table (status_vector) != SUCCESS)
	return FAILURE;
    } 
if (owner_flag && 
    create_owner (status_vector, owner_id, owner_type, owner_handle) != SUCCESS)
    return FAILURE;

#ifndef  SUPERSERVER
#if defined(SCO_EV) || defined(LINUX)
/* 5.5 SCO port: might also help other classic ports, but not sure. This
   and a few subsequent pieces of code later, solve problem of gds_drop 
   giving following message 
Fatal lock manager error: semop failed (acquire), errno: 22
--Invalid argument
If this happens on another classic platform add that platform too. - Shailesh
*/
if ( LOCK_owner_offset = *owner_handle )  
#else
    LOCK_owner_offset = *owner_handle;
#endif
    LOCK_owner = (OWN) ABS_PTR (*owner_handle);
#endif

#if !(defined NETWARE_386 || defined WIN_NT || defined OS2_ONLY || defined SOLARIS_MT || POSIX_THREADS)
#if defined(SCO_EV) || defined(LINUX)
if (LOCK_owner_offset )   /* 5.5 SCO port: gds_drop */
#endif
    ISC_signal (LOCK_block_signal, blocking_action, (void*) LOCK_owner_offset);
#endif
    
/* Initialize process level stuffs for different platforms.
   This should be done after the call to create_owner() that 
   initializes owner_handle. */

#if (defined WIN_NT && !defined SUPERSERVER)
owner = (OWN) ABS_PTR (*owner_handle);
wakeup_event [0] = owner->own_wakeup_hndl;
blocking_event [0] = ISC_make_signal (TRUE, FALSE, LOCK_pid, LOCK_block_signal);
owner->own_blocking_hndl = blocking_event [0];
AST_ALLOC;
if (gds__thread_start ((FPTR_INT) blocking_action_thread,
    &LOCK_owner_offset, THREAD_critical, 0, NULL_PTR))
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__lockmanerr;
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__sys_request;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) "CreateThread";
    *status_vector++ = gds_arg_win32;
    *status_vector++ = GetLastError();
    *status_vector++ = gds_arg_end;
    CloseHandle (blocking_event [0]);
    CloseHandle (wakeup_event [0]);
    return FAILURE;
    }
#endif

#ifdef OS2_ONLY
DosCreateEventSem (NULL, blocking_event, DC_SEM_SHARED, 0);
owner = (OWN) ABS_PTR (*owner_handle);
owner->own_blocking_hndl = blocking_event [0];
if (status = gds__thread_start ((FPTR_INT) blocking_action_thread,
    &LOCK_owner_offset, THREAD_medium_high, 0, NULL_PTR))
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__lockmanerr;
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__sys_request;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) "DosCreateThread";
    *status_vector++ = gds_arg_dos;
    *status_vector++ = status;
    *status_vector++ = gds_arg_end;
    DosCloseEventSem (blocking_event [0]);
    return FAILURE;
    }
#endif

#ifdef NeXT
port_allocate (task_self(), &alarm_port);
null_msg = (msg_header_t *) message_buf;
null_msg->msg_remote_port = alarm_port;
null_msg->msg_size = sizeof (msg_header_t);
null_msg1 = (msg_header_t *) message_buf1;
null_msg1->msg_local_port = alarm_port;
null_msg1->msg_size = sizeof (msg_header_t);
port_waker_c_thread = cthread_fork ((cthread_fn_t) port_waker, (any_t) &LOCK_owner_offset);
cthread_detach (port_waker_c_thread);
cthread_thread (port_waker_c_thread);

ISC_signal (LOCK_wakeup_signal, wakeup_action, (void*) &LOCK_owner_offset);

condition_set = 0;
owner = (OWN) ABS_PTR (*owner_handle);
condition_init (&owner->own_NeXT_semaphore);
#endif

#ifndef SUPERSERVER
#ifdef MMAP_SUPPORTED
#ifdef SOLARIS_MT
/* Map the owner block separately so that threads waiting
   on synchronization variables embedded in the owner block
   don't have to coordinate during lock table unmapping. */

if (!(LOCK_owner = (OWN) ISC_map_object (status_vector, &LOCK_data,
	    LOCK_owner_offset, sizeof (struct own))))
    return FAILURE;
AST_ALLOC;
if (status = gds__thread_start ((FPTR_INT) blocking_action_thread,
    &LOCK_owner_offset, THREAD_high, 0, NULL_PTR))
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__lockmanerr;
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__sys_request;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) "thr_create";
    *status_vector++ = gds_arg_unix;
    *status_vector++ = status;
    *status_vector++ = gds_arg_end;
    return FAILURE;
    }
#else
#if defined(SCO_EV) || defined(LINUX)
if ( LOCK_owner_offset ) /* 5.5 SCO Port: gds_drop */
#endif
	if (!(LOCK_owner = (OWN) ISC_map_object (status_vector, &LOCK_data,
            LOCK_owner_offset, sizeof (struct own))))
		return FAILURE;
#endif
#endif
#endif
    
#ifdef MANAGER_PROCESS
if (start_manager)
    {
    start_manager = FALSE;
    if (!fork_lock_manager (status_vector))
	return FAILURE;
    }
#endif

return SUCCESS;
}

#ifdef MANAGER_PROCESS
void LOCK_manager (
    PTR		manager_owner_offset)
{
/**************************************
 *
 *	L O C K _ m a n a g e r
 *
 **************************************
 *
 * Functional description
 *	LOCK_manager is used only in the privileged lock manager
 *	process.  The routine simply waits on its semaphore.  It
 *	the semaphore is poked, it wakes up, looks for blocking
 *	signals to be delivered, delivers them, and goes back to
 *	sleep.
 *
 **************************************/
OWN		manager_owner, owner;
SRQ		que;
int		ret = FAILURE;
STATUS		local_status [20];
SLONG		value;
USHORT		semaphore;
EVENT		event_ptr;
#ifdef DEBUG
ULONG		signal_counter = 0;
#endif
#ifdef VALIDATE_LOCK_TABLE
ULONG		manager_counter = 0;
#endif


LOCK_TRACE (("LOCK_manager\n"));

acquire (manager_owner_offset);

#ifdef VALIDATE_LOCK_TABLE
validate_lhb (LOCK_header);
#endif


/* If there already is a lock manager running, quietly return */

while (owner = get_manager (FALSE))
    if (signal_owner (owner, (PTR)NULL))
	purge_owner (manager_owner_offset, owner);
    else
	{
	DEBUG_MSG (0,("LOCK_manager, pid %ld quiting, pid %ld is already manager\n", LOCK_pid, owner->own_process_id));
	release (manager_owner_offset);
	return;
	}

/* Declare ourselves to be lock manager process */

DEBUG_MSG (0,("LOCK_manager, pid %ld becoming manager\n", LOCK_pid));

manager_owner = (OWN) ABS_PTR (manager_owner_offset);
manager_owner->own_flags |= OWN_manager;
LOCK_process_owner.own_flags |= OWN_manager;
#ifdef STATIC_SEMAPHORES
semaphore = alloc_semaphore (manager_owner, NULL);
#else
manager_owner->own_semaphore = 1;
#endif
ASSERT_ACQUIRED;
LOCK_header->lhb_manager = manager_owner_offset;
LOCK_header->lhb_flags &= ~LHB_shut_manager;
release (manager_owner_offset);

/* Loop, waiting for something to happen */

chmod (LOCK_HEADER, 0444);

for (;;)
    {
    acquire (manager_owner_offset);
#ifdef VALIDATE_LOCK_TABLE
    if ((manager_counter++ % 100) == 0)
	validate_lhb (LOCK_header);
#endif
    manager_owner = (OWN) ABS_PTR (manager_owner_offset);
    if (LOCK_header->lhb_flags & LHB_shut_manager)
    	{
	purge_owner (manager_owner_offset, manager_owner);
	release_mutex();
	break;
	}
    ASSERT_ACQUIRED;
    QUE_LOOP (LOCK_header->lhb_owners, que)
	{
	owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
	if (owner->own_flags & OWN_signal)
            if (signal_owner (owner, (PTR)NULL))
		{
		que = (SRQ) ABS_PTR (que->srq_backward);
		purge_owner (manager_owner_offset, owner);
		}
	    else
		{
		owner->own_flags &= ~OWN_signal;
#ifdef DEBUG
		if ((++signal_counter % 1000) == 0)
		    DEBUG_MSG (1,("LOCK_manager: delivered %ld signals\n", signal_counter));
#endif
		}
	}
    event_ptr = manager_owner->own_wakeup;
    value = ISC_event_clear (manager_owner->own_wakeup);
    release (manager_owner_offset);

    /* Prepare to wait for a timeout or a wakeup from somebody else.  Start
       by setting an alarm clock. */

    ret = ISC_event_wait (1, &event_ptr, &value, 
                LOCKMANTIMEOUT*1000000, lock_alarm_handler, event_ptr);

#ifdef DEBUG
    if (ret == FAILURE)
	DEBUG_MSG (1,("LOCK_manager timer wakeup\n"));
#endif

#ifdef TERMINATE_IDLE_LOCK_MANAGER
    /* The code to terminate an idle lock manager
     * was disabled around Dec 1993 as part of the Solaris port.
     * This was due to customer complaints about running InterBase and
     * Oracle on the same machine.  When Oracle starts up it grabs all
     * the system resources, leaving none for InterBase.  And if we
     * release our resources we won't be able to restart.
     * 1995-March-20 David Schnepper 
     */

    /* If we're the only process mapping the lock table when the alarm 
       expires, exit. */

    if (ret == FALSE)
	if (!flock ((int) LOCK_data.sh_mem_handle, LOCK_EX | LOCK_NB))
	    break;
	else
	    flock ((int) LOCK_data.sh_mem_handle, LOCK_SH);
#endif
    }

/* Release system resources for semaphores. */

LOCK_header = NULL;	
#ifdef TERMINATE_IDLE_LOCK_MANAGER
ISC_unmap_file (local_status, &LOCK_data, ISC_SEM_REMOVE);
chmod (LOCK_HEADER, 0664);
#else
ISC_unmap_file (local_status, &LOCK_data, NULL);
#endif

}
#endif

SLONG LOCK_query_data (
    PTR		parent_request,
    USHORT	series,
    USHORT	aggregate)
{
/**************************************
 *
 *	L O C K _ q u e r y _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Query lock series data with respect to a rooted
 *	lock hierarchy calculating aggregates as we go.
 *
 **************************************/
LBL	lock;
LRQ	parent;
SLONG	data, count;
SRQ	data_header, que;

/* Get root of lock hierarchy */

if (parent_request && series < LCK_MAX_SERIES)
    parent = get_request (parent_request);
else
    {
    CHECK (FALSE);
    return 0;
    }

acquire (parent->lrq_owner);
parent = (LRQ) ABS_PTR (parent_request);	/* remap */

++LOCK_header->lhb_query_data;
data_header = &LOCK_header->lhb_data [series];
data = count = 0;

/* Simply walk the lock series data queue forward for the minimum
   and backward for the maximum -- it's maintained in sorted order. */

switch (aggregate)
    {
    case LCK_MIN:
    case LCK_CNT:
    case LCK_AVG:
    case LCK_SUM:
    case LCK_ANY:
    	for (que = (SRQ) ABS_PTR (data_header->srq_forward);
     	     que != data_header; que = (SRQ) ABS_PTR (que->srq_forward))
    	    {
    	    lock = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_data));
	    CHECK (lock->lbl_series == series);
    	    if (lock->lbl_parent != parent->lrq_lock)
	    	continue;

	    switch (aggregate)
	    	{
		case LCK_MIN:
    	    	    data = lock->lbl_data;
    	    	    break;

		case LCK_ANY:
		case LCK_CNT:
		    ++count;
		    break;

		case LCK_AVG:
		    ++count;
		    
		case LCK_SUM:
		    data += lock->lbl_data;
		    break;
		}
	    
	    if (aggregate == LCK_MIN || aggregate == LCK_ANY)
	       	break;
    	    }
	
	if (aggregate == LCK_CNT || aggregate == LCK_ANY)
	    data = count;
	else if (aggregate == LCK_AVG)
	    data = (count) ? data/count : 0;
	break;

    case LCK_MAX:
        for (que = (SRQ) ABS_PTR (data_header->srq_backward);
     	     que != data_header; que = (SRQ) ABS_PTR (que->srq_backward))
    	    {
    	    lock = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_data));
	    CHECK (lock->lbl_series == series);
    	    if (lock->lbl_parent != parent->lrq_lock)
	    	continue;
    
    	    data = lock->lbl_data;
    	    break;
    	    }
	break;
	
    default:
       	CHECK (FALSE);
    }

release (parent->lrq_owner);
return data;
}

SLONG LOCK_read_data (
    PTR		request_offset)
{
/**************************************
 *
 *	L O C K _ r e a d _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Read data associated with a lock.
 *
 **************************************/
LBL	lock;
LRQ	request;
SLONG	data;

LOCK_TRACE (("LOCK_read_data(%ld)\n", request_offset));

request = get_request (request_offset);
acquire (request->lrq_owner);
++LOCK_header->lhb_read_data;
request = (LRQ) ABS_PTR (request_offset);/* Re-init after a potential remap */
lock = (LBL) ABS_PTR (request->lrq_lock);
data = lock->lbl_data;
if (lock->lbl_series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [lock->lbl_series];
else
    ++LOCK_header->lhb_operations [0];

release (request->lrq_owner);

return data;
}

SLONG LOCK_read_data2 (
    PTR		parent_request,
    USHORT	series,
    UCHAR	*value,
    USHORT	length,
    PTR		owner_offset)
{
/**************************************
 *
 *	L O C K _ r e a d _ d a t a 2
 *
 **************************************
 *
 * Functional description
 *	Read data associated with transient locks.
 *
 **************************************/
PTR	parent;
LRQ	request;
LBL	lock;
SLONG	data;
USHORT	junk;

LOCK_TRACE (("LOCK_read_data2(%ld)\n", parent_request));

acquire (owner_offset);
++LOCK_header->lhb_read_data;
if (series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [series];
else
    ++LOCK_header->lhb_operations [0];

if (parent_request)
    {
    request = get_request (parent_request);
    parent = request->lrq_lock;
    }
else
    parent = 0;

if (lock = find_lock (parent, series, value, length, &junk))
    data = lock->lbl_data;
else
    data = 0;

release (owner_offset);

return data;
}

void LOCK_re_post (
    int		(*ast)(void *),
    void	*arg,
    PTR		owner_offset)
{
/**************************************
 *
 *	L O C K _ r e _ p o s t
 *
 **************************************
 *
 * Functional description
 *	Re-post an AST that was previously blocked.
 *	It is assumed that the routines that look
 *	at the re-post list only test the ast element.
 *
 **************************************/
OWN	owner;
LRQ	request;

LOCK_TRACE (("LOCK_re_post(%ld)\n", owner_offset));

acquire (owner_offset);

/* Allocate or reuse a lock request block */

ASSERT_ACQUIRED;
if (QUE_EMPTY (LOCK_header->lhb_free_requests))
    {
    if (!(request = (LRQ) alloc (sizeof (struct lrq), NULL)))
	{
	release (owner_offset);
	return;
	}
    }
else
    {
    ASSERT_ACQUIRED;
    request = (LRQ) ((UCHAR*) QUE_NEXT (LOCK_header->lhb_free_requests) - 
	OFFSET (LRQ, lrq_lbl_requests));
    remove_que (&request->lrq_lbl_requests);
    }

owner = (OWN) ABS_PTR (owner_offset);
request->lrq_type = type_lrq;
request->lrq_flags = LRQ_repost;
request->lrq_ast_routine = ast;
request->lrq_ast_argument = arg;
request->lrq_requested = LCK_none;
request->lrq_state = LCK_none;
request->lrq_owner = owner_offset;
request->lrq_lock = (PTR) 0;
insert_tail (&owner->own_blocks, &request->lrq_own_blocks);

DEBUG_DELAY;

#ifdef ANY_THREADING
signal_owner ((OWN) ABS_PTR (owner_offset), (PTR)NULL);
#else
/* The deadlock detection looks at the OWN_signaled bit to decide
 * whether processes have things to look at - as we're putting
 * a repost item on the blocking queue, we DO have additional work
 * to do, so set the flag to indicate so.
 */
owner->own_flags &= ~OWN_signal;
owner->own_ast_flags |= OWN_signaled;
DEBUG_DELAY;
ISC_inhibit();
DEBUG_DELAY;
blocking_action2 (owner_offset, (PTR)NULL);
DEBUG_DELAY;
ISC_enable();
DEBUG_DELAY;
#endif
release (owner_offset);
}

BOOLEAN LOCK_shut_manager (void)
{
/**************************************
 *
 *	L O C K _ s h u t _ m a n a g e r
 *
 **************************************
 *
 * Functional description
 *	Set a shutdown flag and post the lock
 *	manager process to exit.
 *
 **************************************/
#ifdef MANAGER_PROCESS
OWN	manager;

acquire (DUMMY_OWNER_SHUTDOWN);
if (manager = get_manager (FALSE))
    {
    LOCK_header->lhb_flags |= LHB_shut_manager;
    post_wakeup (manager);
    release_mutex();
    sleep (5);
    acquire (DUMMY_OWNER_SHUTDOWN);
    LOCK_header->lhb_flags &= ~LHB_shut_manager;
    manager = get_manager (FALSE);
    }

release_mutex();
return (manager ? FALSE : TRUE);
#else
return TRUE;
#endif
}

#ifdef  WINDOWS_ONLY
void    LOCK_wep( void)
{
/**************************************
 *
 *      L O C K _ w e p
 *
 **************************************
 *
 * Functional description
 *      Call exit_handler for WEP.
 *
 **************************************/

exit_handler( NULL);
}
#endif

SLONG LOCK_write_data (
    PTR		request_offset,
    SLONG	data)
{
/**************************************
 *
 *	L O C K _ w r i t e _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Write a longword into the lock block.
 *
 **************************************/
LBL	lock;
LRQ	request;

LOCK_TRACE (("LOCK_write_data (%ld)\n", request_offset));

request = get_request (request_offset);
acquire (request->lrq_owner);
++LOCK_header->lhb_write_data;
request = (LRQ) ABS_PTR (request_offset);/* Re-init after a potential remap */
lock = (LBL) ABS_PTR (request->lrq_lock);
remove_que (&lock->lbl_lhb_data);
if (lock->lbl_data = data)
   insert_data_que (lock);

if (lock->lbl_series < LCK_MAX_SERIES)
    ++LOCK_header->lhb_operations [lock->lbl_series];
else
    ++LOCK_header->lhb_operations [0];

release (request->lrq_owner);

return data;
}

static void acquire (
    PTR		owner_offset)
{
/**************************************
 *
 *	a c q u i r e
 *
 **************************************
 *
 * Functional description
 *	Acquire the lock file.  It it's busy, wait for it.
 *
 **************************************/
PTR		prior_active;
SLONG		length, spins, status;
LHB		header;
STATUS		status_vector [20];
OWN		owner;

#if (defined SOLARIS_MT && !defined SUPERSERVER)
acquire_retry:
#endif

/* Check that we aren't trying to acquire when we already own it! */
/* ASSERT_RELEASED; This will not work, when the current active owner
   of the lock table falls in the remapped portion of the map
   file, which we are yet to expand (remap) to */

#ifndef ANY_THREADING   
++LOCK_asts;
DEBUG_DELAY;
#endif

/* Measure the impact of the lock table resource as an overall
   system bottleneck. This will be useful metric for lock
   improvements and as a limiting factor for SMP. A conditional
   mutex would probably be more accurate but isn't worth the
   effort. */

prior_active = LOCK_header->lhb_active_owner;

#ifndef SUPERSERVER
#ifdef MMAP_SUPPORTED
if (LOCK_owner)
    {
    /* Record a "timestamp" of when this owner requested the lock table */
    LOCK_owner->own_acquire_time = LOCK_header->lhb_acquires;
#ifdef DEV_BUILD
    /* Due to the system overhead of acquiring the actual time-of-day,
       we only get the realtime for DEV_BUILD */
    LOCK_owner->own_acquire_realtime = GET_TIME;
#endif
    LOCK_owner->own_ast_hung_flags |= OWN_hung;
    }
#endif
#endif

/* Perform a spin wait on the lock table mutex. This should only
   be used on SMP machines; it doesn't make much sense otherwise. */

status = FAILURE;
#if (defined UNIX || defined WIN_NT)
for (spins = 0; spins < LOCK_acquire_spins; ++spins)
    if ((status = ISC_mutex_lock_cond (MUTEX)) == SUCCESS)
       	break;
#endif

/* If the spin wait didn't succeed then wait forever. */

if (status != SUCCESS)
    if (ISC_mutex_lock (MUTEX))
    	bug (NULL, "semop failed (acquire)");

++LOCK_header->lhb_acquires;
if (prior_active)
    ++LOCK_header->lhb_acquire_blocks;

#if (defined UNIX || defined WIN_NT)
if (spins)
    {
    ++LOCK_header->lhb_acquire_retries;
    if (spins < LOCK_acquire_spins)
       	++LOCK_header->lhb_retry_success;
    }
#endif

prior_active = LOCK_header->lhb_active_owner;
LOCK_header->lhb_active_owner = owner_offset;

#ifdef MANAGER_PROCESS
LOCK_post_manager = FALSE;
#endif

#ifndef SUPERSERVER
#ifndef ANY_THREADING
if (LOCK_owner)
    {
    LOCK_owner->own_ast_hung_flags &= ~OWN_hung;  /* Can't be hung by OS if we got here */
    }
#endif
#endif

if (LOCK_header->lhb_length > LOCK_data.sh_mem_length_mapped
#ifdef LOCK_DEBUG_ACQUIRE
/* If we're debugging remaps occuring during acquire, force 
   a remap every-so-often. */
    || ((debug_acquire_count++ % DEBUG_ACQUIRE_INTERVAL) == 0) 
#endif
    )
    {

    length = LOCK_header->lhb_length;
/* Do not do Lock table remapping for SUPERSERVER. Specify required
   lock table size in the configuration file */
#if !((defined SUPERSERVER) && (defined MMAP_SUPPORTED))
    header = (LHB) ISC_remap_file (status_vector, &LOCK_data, length, FALSE);
    if (!header)
#endif
	{
	bug (NULL, "remap failed");
	return;
	}
    LOCK_header = header;
    }

/* If we were able to acquire the MUTEX, but there is an prior owner marked
 * in the the lock table, it means that someone died while owning
 * the lock mutex.  In that event, lets see if there is any unfinished work
 * left around that we need to finish up.
 */
if (prior_active)
    {
    SHB	recover;
    SRQ	que;

    post_history (his_active, owner_offset, prior_active, (PTR) 0, FALSE);
    recover = (SHB) ABS_PTR (LOCK_header->lhb_secondary);
    if (recover->shb_remove_node)
	{
	/* There was a remove_que operation in progress when the prior_owner died */
	DEBUG_MSG (0,("Got to the funky shb_remove_node code\n"));
	remove_que ((SRQ)ABS_PTR (recover->shb_remove_node));
	}
    else if (recover->shb_insert_que && recover->shb_insert_prior)
	{
	/* There was a insert_que operation in progress when the prior_owner died */
	DEBUG_MSG (0,("Got to the funky shb_insert_que code\n"));

	que = (SRQ) ABS_PTR (recover->shb_insert_que);
	que->srq_backward = recover->shb_insert_prior;
	que = (SRQ) ABS_PTR (recover->shb_insert_prior);
	que->srq_forward = recover->shb_insert_que;
	recover->shb_insert_que = 0;
	recover->shb_insert_prior = 0;
	}
    }
#if (defined SOLARIS_MT && !defined SUPERSERVER)
if (LOCK_solaris_stall)
    {
    if (owner_offset > 0)
        {
        OWN         first_owner;
        EVENT       event_ptr;
        SLONG       value;

        owner = (OWN) ABS_PTR (owner_offset);    /* Can't be hung by OS if we got he
re */
        owner->own_ast_hung_flags &= ~OWN_hung;
        first_owner = (OWN) ((UCHAR*) QUE_NEXT (LOCK_header->lhb_owners) -
                OFFSET (OWN, own_lhb_owners));
        if (first_owner->own_ast_hung_flags & OWN_hung &&
	    ((LOCK_header->lhb_acquires - first_owner->own_acquire_time)
		> STARVATION_THRESHHOLD))
            {
            first_owner->own_flags |= OWN_starved;
            if (owner->own_flags & OWN_blocking)
                {
                probe_owners (owner_offset);
                owner->own_flags &= ~OWN_blocking;
                release_mutex();
                }
            else
                {
		SLONG	ret;
                owner->own_flags |= OWN_blocking;
                owner->own_flags &= ~OWN_wakeup;
                owner->own_semaphore = 1;
                event_ptr = owner->own_stall;
                value = ISC_event_clear (event_ptr);
                release_mutex();
                ret = ISC_event_wait (1, &event_ptr, &value,
                            LOCK_solaris_stall * 1000000,
                            lock_alarm_handler, event_ptr);
#ifdef DEV_BUILD
		if (ret != SUCCESS)
		    gds__log ("LOCK: owner %d timed out while stalling for benefit of owner %d", 
		        owner_offset, REL_PTR (first_owner));
#endif
                }

            goto acquire_retry;
            }

        owner->own_flags &= ~OWN_blocking;
        }
    }
#endif

}

static UCHAR *alloc (
    SSHORT	size,
    STATUS	*status_vector)
{
/**************************************
 *
 *	a l l o c
 *
 **************************************
 *
 * Functional description
 *	Allocate a block of given size.
 *
 **************************************/
ULONG		length, block;
LHB		header;
OWN		owner;

size = ALIGN (size, sizeof (IPTR));
ASSERT_ACQUIRED;
block = LOCK_header->lhb_used;
LOCK_header->lhb_used += size;

/* Make sure we haven't overflowed the lock table.  If so, bump the size of
   the table */

if (LOCK_header->lhb_used > LOCK_header->lhb_length)
    {
    LOCK_header->lhb_used -= size;
/* Do not do Lock table remapping for SUPERSERVER. Specify required
   lock table size in the configuration file */
#if (!(defined SUPERSERVER) && (defined MMAP_SUPPORTED)) || defined (WIN_NT)
    length = LOCK_data.sh_mem_length_mapped + EXTEND_SIZE;
    header = (LHB) ISC_remap_file (status_vector, &LOCK_data, length, TRUE);
    if (header)
	{
	LOCK_header = header;
	ASSERT_ACQUIRED;
	LOCK_header->lhb_length = LOCK_data.sh_mem_length_mapped;
	LOCK_header->lhb_used += size;
	}
    else
#endif
	{
	/* Do not do abort in case if there is not enough room -- just 
	   return an error */

	if (status_vector)
	    {
	    *status_vector++ = gds_arg_gds;
	    *status_vector++ = gds__random;
	    *status_vector++ = gds_arg_string;
	    *status_vector++ = (STATUS) "lock manager out of room";
	    *status_vector++ = gds_arg_end;
	    }

	return (UCHAR*) NULL;
	}
    }

#ifdef DEV_BUILD
/* This version of alloc() doesn't initialize memory.  To shake out
   any bugs, in DEV_BUILD we initialize it to a "funny" pattern */
memset (ABS_PTR (block), 0xFD, size);
#endif

return ABS_PTR (block);
}

static LBL alloc_lock (
    USHORT	length,
    STATUS	*status_vector)
{
/**************************************
 *
 *	a l l o c _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Allocate a lock for a key of a given length.  Look first to see
 *	if a spare of the right size is sitting around.  If not, allocate
 *	one.
 *
 **************************************/
LBL	lock;
SRQ	que;

length = (length + 3) & ~3;

ASSERT_ACQUIRED;
QUE_LOOP (LOCK_header->lhb_free_locks, que)
    {
    lock = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_hash));
    if (lock->lbl_size == length)
	{
	remove_que (&lock->lbl_lhb_hash);
	lock->lbl_type = type_lbl;
	return lock;
	}
    }

if (lock = (LBL) alloc (sizeof (struct lbl) + length, status_vector))
    {
    lock->lbl_size = length;
    lock->lbl_type = type_lbl;
    }

/* NOTE: if the above alloc() fails do not release mutex here but rather
         release it in LOCK_enq() (as of now it is the only function that
         calls alloc_lock()). We need to hold mutex to be able
         to release a lock request block */
   

return lock;
}

#ifdef STATIC_SEMAPHORES
static USHORT alloc_semaphore (
    OWN		owner,
    STATUS	*status_vector)
{
/**************************************
 *
 *	a l l o c _ s e m a p h o r e
 *
 **************************************
 *
 * Functional description
 *	Allocate an unused semaphore.
 *
 **************************************/
USHORT	h, i;
SRQ	que;
OWN	other;
SMB	semaphores;

for (h = 0; h < 2; h++)
    {
    ASSERT_ACQUIRED;
    semaphores = (SMB) ABS_PTR (LOCK_header->lhb_mask);
    for (i = 1; i < (USHORT) LOCK_sem_count; i++)
	if (semaphores->smb_mask [i / BITS_PER_LONG] & (1L << (i % BITS_PER_LONG)))
	    {
	    semaphores->smb_mask [i / BITS_PER_LONG] &= ~(1L << (i % BITS_PER_LONG));
	    owner->own_semaphore = i;
	    owner->own_wakeup [0].event_semnum = i;
	    return i;
	    }

    /* Loop thru owners to see if a semaphore is available */

    ASSERT_ACQUIRED;
    QUE_LOOP (LOCK_header->lhb_owners, que)
	{
	other = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
	if (other->own_semaphore & OWN_semavail)
	    release_semaphore (other);
	}
    }

bug (status_vector, "semaphores are exhausted");

return 0;
}
#endif

static void blocking_action (
    PTR		owner_offset)
{
/**************************************
 *
 *	b l o c k i n g _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Fault hander for a blocking signal.  A blocking signal
 *	is an indication (albeit a strong one) that a blocking
 *	AST is pending for the owner.  Check in with the data
 *	structure for details.
 *	The re-post code in this routine assumes that no more
 *	than one thread of execution can be running in this
 *	routine at any time.
 *
 *      NOTE: This is a wrapper for calling blocking_action2() where 
 *		   the real action is.  This routine would be called 
 *		   from a signal_handler or blocking_action_thread()
 *		   or LOCK_re_post() where acquire() would not have 
 *		   been done.
 *
 **************************************/

/* Ignore signals that occur when executing in lock manager
   or when there is no owner block set up */

#ifndef ANY_THREADING   
if (++LOCK_asts || !owner_offset)
    {
    DEBUG_DELAY;
    if (owner_offset)
	{
	OWN	owner;
	/* This operation is done WITHOUT acquiring the lock table.
	 * As this is done at the signal level, we might be resignalling
	 * ourself and already have the lock table acquired at this
	 * point.  If we already have it we'll hang when trying to acquire
	 * it.  The use of LOCK_asts indicates, somewhat imprecisely,
	 * when we are resignalling ourselves when we have the table
	 * acquired already.
	 */
	owner = (OWN) ABS_PTR (owner_offset);
	owner->own_ast_flags &= ~OWN_signaled;
	}
    DEBUG_DELAY;
    --LOCK_asts;
    DEBUG_DELAY;
    return;
    }
#else
if (!owner_offset)
    return;
#endif

DEBUG_DELAY;

acquire (owner_offset);
blocking_action2 (owner_offset, (PTR)NULL);
#ifdef NeXT
/* make sure we're not shut down */

if (!getout)
#endif
release (owner_offset);

#ifndef ANY_THREADING
DEBUG_DELAY;
--LOCK_asts;
DEBUG_DELAY;
#endif
}

static void blocking_action2 (
    PTR         blocking_owner_offset,
    PTR         blocked_owner_offset)
{
/**************************************
 *
 *	b l o c k i n g _ a c t i o n 2
 *
 **************************************
 *
 * Functional description
 *	Fault hander for a blocking signal.  A blocking signal
 *	is an indication (albeit a strong one) that a blocking
 *	AST is pending for the owner.  Check in with the data
 *	structure for details.
 *	The re-post code in this routine assumes that no more
 *	than one thread of execution can be running in this
 *	routine at any time.
 *
 *      IMPORTANT: Before calling this routine, acquire() should
 *	           have already been done.
 *
 *      Note that both a blocking owner offset and blocked owner
 *      offset are passed to this function.   This is for those
 *      cases where the owners are not the same.   If they are
 *      the same, then the blocked owner offset will be NULL.
 *
 **************************************/
OWN	owner;
SRQ	que;
LRQ	request;
int	(*routine)(), *arg;

ASSERT_ACQUIRED;
owner = (OWN) ABS_PTR (blocking_owner_offset);

if (!blocked_owner_offset)
    blocked_owner_offset = blocking_owner_offset;

while (owner->own_count)
    {
    que = QUE_NEXT (owner->own_blocks);
    if (que == &owner->own_blocks)
	{
	/* We've processed the own_blocks queue, reset the "we've been
	 * signaled" flag and start winding out of here
	 */
	owner->own_ast_flags &= ~OWN_signaled;
        /*post_history (his_leave_ast, blocking_owner_offset, 0, 0, TRUE);*/
	break;
	}
    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_own_blocks));
    routine = request->lrq_ast_routine;
    arg = request->lrq_ast_argument;
    remove_que (&request->lrq_own_blocks);
    if (request->lrq_flags & LRQ_blocking)
    	{
	request->lrq_flags &= ~LRQ_blocking;
	request->lrq_flags |= LRQ_blocking_seen;
    	++LOCK_header->lhb_blocks;
        post_history (his_post_ast, blocking_owner_offset, request->lrq_lock, REL_PTR (request), TRUE);
	}
    else if (request->lrq_flags & LRQ_repost)
	{
	request->lrq_type = type_null;
	insert_tail (&LOCK_header->lhb_free_requests, &request->lrq_lbl_requests);
	}
       	
    if (routine)
	{
        release (blocked_owner_offset);
	(*routine) (arg);
#ifdef NeXT
	/* make sure we're not shut down */

	if (getout)
	    break;
#endif
        acquire (blocked_owner_offset);
        owner = (OWN) ABS_PTR (blocking_owner_offset);
	}
    }
}

#if (defined WIN_NT && !defined SUPERSERVER)
static void THREAD_ROUTINE blocking_action_thread (
    PTR		*owner_offset_ptr)
{
/**************************************
 *
 *	b l o c k i n g _ a c t i o n _ t h r e a d	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Thread to handle blocking signals that a process
 *	will send to itself.
 *
 **************************************/
SLONG	ret;
OWN	owner;

AST_INIT;	/* Check into scheduler as AST thread */

while (TRUE)
    {
    ret = WaitForSingleObject (blocking_event [0], INFINITE);
    AST_ENTER;
    owner = (OWN) ABS_PTR (*owner_offset_ptr);
    if ((ret && ret != WAIT_ABANDONED) || 
	!*owner_offset_ptr ||
        owner->own_process_id != LOCK_pid ||
        owner->own_owner_id == 0)
	break;
    blocking_action (*owner_offset_ptr);
    AST_EXIT;
    }

AST_EXIT;
AST_FINI;	/* Check out of scheduler as AST thread */
}
#endif

#ifdef OS2_ONLY
static void THREAD_ROUTINE blocking_action_thread (
    PTR		*owner_offset_ptr)
{
/**************************************
 *
 *	b l o c k i n g _ a c t i o n _ t h r e a d	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Thread to handle blocking signals that a process
 *	will send to itself.
 *
 **************************************/
ULONG	status, count;
OWN	owner;

while (TRUE)
    {
    status = DosWaitEventSem (blocking_event [0], SEM_INDEFINITE_WAIT);
    owner = (OWN) ABS_PTR (*owner_offset_ptr);
    if ((status && status != ERROR_INTERRUPT) || 
	!*owner_offset_ptr ||
        owner->own_process_id != LOCK_pid ||
        owner->own_owner_id == 0)
	break;
    DosResetEventSem (blocking_event [0], &count);
    blocking_action (*owner_offset_ptr);
    }
}
#endif

#if (defined SOLARIS_MT && !defined SUPERSERVER)
static void THREAD_ROUTINE blocking_action_thread (
    PTR		*owner_offset_ptr)
{
/**************************************
 *
 *	b l o c k i n g _ a c t i o n _ t h r e a d	( S O L A R I S _ M T )
 *
 **************************************
 *
 * Functional description
 *	Thread to handle blocking signals.
 *
 **************************************/
SLONG	value;
EVENT	event_ptr;

AST_INIT;	/* Check into scheduler as AST thread */

while (TRUE)
    {
    AST_ENTER;

     /* See if main thread has requested us to go away */
    if (!*owner_offset_ptr ||
        LOCK_owner->own_process_id != LOCK_pid ||
        !LOCK_owner->own_owner_id)
	break;

    value = ISC_event_clear (LOCK_owner->own_blocking);
    blocking_action (*owner_offset_ptr);
    AST_EXIT;
    event_ptr = LOCK_owner->own_blocking;
    ISC_event_wait (1, &event_ptr, &value, 0, NULL, NULL_PTR);
    }

/* Main thread asked us to go away, do our cleanup, then tell
 * main thread we're done (see shutdown_blocking_action()).
 */

AST_EXIT;
AST_FINI;	/* Check out of scheduler as AST thread */

/* Wakeup the main thread waiting for our exit. */
/* Main thread won't wait forever, so check LOCK_owner is still mapped */

if (LOCK_owner)
    ISC_event_post (LOCK_owner->own_wakeup);
}
#endif

#ifdef DEV_BUILD
static void bug_assert (
    CONST TEXT	*string,
    ULONG	line)
{
/**************************************
 *
 *      b u g _ a s s e r t
 *
 **************************************
 *
 * Functional description
 *	Disasterous lock manager bug.  Issue message and abort process.
 *
 **************************************/
TEXT	buffer [100];
struct	lhb	LOCK_header_copy;

sprintf ((char *) buffer, "%s %ld: lock assertion failure: %s\n", 
	 __FILE__, line, string);

/* Copy the shared memory so we can examine its state when we crashed */
LOCK_header_copy = *LOCK_header;

bug (NULL, buffer);		/* Never returns */
}
#endif

static void bug (
    STATUS	*status_vector,
    CONST TEXT	*string)
{
/**************************************
 *
 *	b u g
 *
 **************************************
 *
 * Functional description
 *	Disasterous lock manager bug.  Issue message and abort process.
 *
 **************************************/
TEXT	s [128];
OWN	owner;

if (!LOCK_bugcheck++)
    {

#ifdef DEV_BUILD
#if !(defined WIN_NT || defined WINDOWS_ONLY)
    /* The lock file has some problem - copy it for later analysis */
    {
    TEXT	*lock_file;
    TEXT	buffer [256];	
    TEXT	buffer2 [256];	
    TEXT	hostname [64];
    gds__prefix_lock (buffer, LOCK_FILE);
    lock_file = buffer;
    sprintf (buffer2, lock_file, ISC_get_host (hostname, sizeof (hostname)));
    sprintf (buffer, "cp %s isc_lock1.%d", buffer2, getpid());
    system (buffer);
    }
#endif	/* WIN_NT || WINDOWS_ONLY */
#endif	/* DEV_BUILD */

    /* If the current mutex acquirer is in the same process, 
       release the mutex */

    if (LOCK_header && (LOCK_header->lhb_active_owner > 0))
	{
	owner = (OWN) ABS_PTR (LOCK_header->lhb_active_owner);
	if (owner->own_process_id == LOCK_pid) 
	    release (LOCK_header->lhb_active_owner);
	}

    if (status_vector)
	{
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__lockmanerr;
	*status_vector++ = gds_arg_gds;
	*status_vector++ = gds__random;
	*status_vector++ = gds_arg_string;
	*status_vector++ = (STATUS) string;
	*status_vector++ = gds_arg_end;
	return;
	}
    }

sprintf (s, "Fatal lock manager error: %s, errno: %d", string, ERRNO);
gds__log (s);
ib_fprintf (ib_stderr, "%s\n", s);

#if !(defined NETWARE_386 || defined WIN_NT || defined OS2_ONLY || defined mpexl)
if (errno > 0)
    ib_fprintf (ib_stderr, "--%s\n", sys_errlist [errno]);
#endif

#ifdef DEV_BUILD
/* Make a core drop - we want to LOOK at this failure! */
abort();
#endif

exit (FINI_ERROR);
}

static BOOLEAN convert (
    PTR		request_offset,
    UCHAR	type,
    SSHORT	lck_wait,
    int		(*ast_routine)(),
    int		*ast_argument,
    STATUS	*status_vector)
{
/**************************************
 *
 *	c o n v e r t
 *
 **************************************
 *
 * Functional description
 *	Perform a lock conversion, if possible.  If the lock cannot be
 *	granted immediately, either return immediately or wait depending
 *	on a wait flag.  If the lock is granted return TRUE, otherwise
 *	return FALSE.  Note: if the conversion would cause a deadlock,
 *	FALSE is returned even if wait was requested.
 *
 **************************************/
LBL	lock;
LRQ	request;
UCHAR	temp;
BOOLEAN	new_ast;
PTR	owner_offset;

ASSERT_ACQUIRED;
request = get_request (request_offset);
lock = (LBL) ABS_PTR (request->lrq_lock);
owner_offset = request->lrq_owner;
post_history (his_convert, owner_offset, request->lrq_lock, request_offset, TRUE);
request->lrq_requested = type;
request->lrq_flags &= ~LRQ_blocking_seen;

/* Compute the state of the lock without the request. */

--lock->lbl_counts [request->lrq_state];
temp = lock_state (lock);

/* If the requested lock level is compatible with the current state
   of the lock, just grant the request.  Easy enough. */

if (COMPATIBLE (type, temp))
    {
    request->lrq_ast_routine = ast_routine;
    request->lrq_ast_argument = ast_argument;
    grant (request, lock);
    post_pending (lock);
    release (owner_offset);
    return TRUE;
    }

++lock->lbl_counts [request->lrq_state];

/* If we weren't requested to wait, just forget about the whole thing. 
   Otherwise wait for the request to be granted or rejected */

if (lck_wait)
    {
    if (request->lrq_ast_routine != ast_routine ||
	request->lrq_ast_argument != ast_argument)
	new_ast = TRUE;
    else
       	new_ast = FALSE;

    if (wait_for_request (request, lck_wait, status_vector))
	{
	ASSERT_RELEASED;
	return FALSE;
	}
    request = (LRQ) ABS_PTR (request_offset);
    if (!(request->lrq_flags & LRQ_rejected))
    	{
	if (new_ast)
	    {
	    acquire (owner_offset);
    	    request = (LRQ) ABS_PTR (request_offset);
	    request->lrq_ast_routine = ast_routine;
	    request->lrq_ast_argument = ast_argument;
	    release (owner_offset);
	    }
	ASSERT_RELEASED;
	return TRUE;
	}
    acquire (owner_offset);
    request = get_request (request_offset);
    lock = (LBL) ABS_PTR (request->lrq_lock);
    post_pending (lock);
    }

request = (LRQ) ABS_PTR (request_offset);
request->lrq_requested = request->lrq_state;
ASSERT_ACQUIRED;
++LOCK_header->lhb_denies;
if (lck_wait < 0)
    ++LOCK_header->lhb_timeouts;
release (owner_offset);
*status_vector++ = gds_arg_gds;
*status_vector++ = (lck_wait > 0) ? gds__deadlock :
			((lck_wait < 0) ? gds__lock_timeout : gds__lock_conflict);
*status_vector++ = gds_arg_end;

return FALSE;
}

static USHORT create_owner (
    STATUS	*status_vector,
    SLONG	owner_id,
    UCHAR	owner_type,
    SLONG	*owner_handle)
{
/**************************************
 *
 *	c r e a t e _ o w n e r 
 *
 **************************************
 *
 * Functional description
 *	Create an owner block.
 *
 **************************************/
SRQ	que;
OWN	owner;
USHORT	new_block;

LOCK_version = LOCK_header->lhb_version;
if (LOCK_version != LHB_VERSION)
    {

    sprintf (LOCK_bug_buffer, "inconsistent lock table version number; found %d, expected %d",
	LOCK_version, LHB_VERSION);
    bug (status_vector, LOCK_bug_buffer);
    return FAILURE;
    }

acquire (DUMMY_OWNER_CREATE);		/* acquiring owner is being created */

/* Look for a previous instance of owner.  If we find one, get rid of it. */

QUE_LOOP (LOCK_header->lhb_owners, que)
    {
    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    if (owner->own_owner_id == owner_id && owner->own_owner_type == owner_type)
	{
	purge_owner (DUMMY_OWNER_CREATE, owner); /* purging owner_offset has not been set yet */
	break;
	}
    }

/* Allocate an owner block */

if (QUE_EMPTY (LOCK_header->lhb_free_owners))
    {
    if (!(owner = (OWN) alloc (sizeof (struct own), status_vector)))
	{
	release_mutex();
	return FAILURE;
	}
    new_block = OWN_BLOCK_new;
    }
else
    {
    owner = (OWN) ((UCHAR*) QUE_NEXT (LOCK_header->lhb_free_owners) - 
	OFFSET (OWN, own_lhb_owners));
    remove_que (&owner->own_lhb_owners);
    new_block = OWN_BLOCK_reused;
    }

init_owner_block (owner, owner_type, owner_id, new_block);

/* cannot ASSERT_ACQUIRED; here - owner not setup */
insert_tail (&LOCK_header->lhb_owners, &owner->own_lhb_owners);

#ifndef SUPERSERVER
(void) probe_owners (REL_PTR (owner));      
#endif

*owner_handle = REL_PTR (owner);
LOCK_header->lhb_active_owner = *owner_handle;

#ifdef VALIDATE_LOCK_TABLE
validate_lhb (LOCK_header);
#endif

release (*owner_handle);

return SUCCESS;
}

#ifdef DEV_BUILD
static void current_is_active_owner (
    BOOLEAN	expect_acquired,
    ULONG	line )
{
/**************************************
 *
 *	c u r r e n t _ i s _ a c t i v e _ o w n e r
 *
 **************************************
 *
 * Functional description
 *	Decide if the current process is the active owner
 *	for the lock table.  Used in assertion checks.
 *
 **************************************/
OWN	owner;
PTR	owner_ptr;
struct	own	owner_copy;

/* Do not ASSERT_ACQUIRED in this routine */

/* If there's no header, we must be setting up in init somewhere */
if (!LOCK_header)
    return;

/* Use a local copy of lhb_active_owner.  We're viewing the lock table
   without being acquired in the "expect_acquired FALSE" case, so it
   can change out from under us.  We don't care that it changes, but
   if it gets set to DUMMY_OWNER_SHUTDOWN, DUMMY_OWNER_CREATE or 
   DUMMY_OWNER_DELETE it can lead to a core drop when we try to map 
   the owner pointer */

owner_ptr = LOCK_header->lhb_active_owner;

/* If no active owner, then we certainly aren't the active owner */
if (!owner_ptr)
    {
    if (!expect_acquired)
        return;
    bug_assert ("not acquired", line);
    }

/* When creating or deleting an owner the owner offset is set such that
 * we can't be sure if WE have the lock table acquired, or someone else 
 * has it, and they just happen to be doing the same operation.  So, we 
 * don't try to report any problems when the lock table is in that state
 */

/* If active owner is DUMMY_OWNER_SHUTDOWN, then we're in process of shutting
   down the lock system. */
if (owner_ptr == DUMMY_OWNER_SHUTDOWN)
    return;

/* If active owner is DUMMY_OWNER_CREATE, then we're creating a new owner */
if (owner_ptr == DUMMY_OWNER_CREATE)
    return;

/* If active owner is DUMMY_OWNER_DELETE, then we're deleting an owner */
if (owner_ptr == DUMMY_OWNER_DELETE)
    return;

/* Find the active owner, and see if it is us */
owner = (OWN) ABS_PTR (owner_ptr);


/* SUPERSERVER uses the same pid for all threads, so the tests
   below are of limited utility and can cause bogus errors */

#ifndef SUPERSERVER
if (owner->own_process_id == LOCK_pid) 
    {
    if (expect_acquired)
	return;
    /* Save a copy for the debugger before we abort */
    memcpy (&owner_copy, owner, sizeof (owner_copy));
    bug_assert ("not acquired", line);
    }
else
    {
    if (!expect_acquired)
	return;
    /* Save a copy for the debugger before we abort */
    memcpy (&owner_copy, owner, sizeof (owner_copy));
    bug_assert ("not released", line);
    }
#endif
}
#endif

static void deadlock_clear (void)
{
/**************************************
 *
 *	d e a d l o c k _ c l e a r
 *
 **************************************
 *
 * Functional description
 *	Clear deadlock and scanned bits for pending requests
 *	in preparation for a deadlock scan.
 *
 **************************************/
SRQ	que;
LRQ	pending;
OWN	owner;
PTR	pending_offset;

ASSERT_ACQUIRED;
QUE_LOOP (LOCK_header->lhb_owners, que)
    {
    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    if (!(pending_offset = owner->own_pending_request))
       	continue;
    pending = (LRQ) ABS_PTR (pending_offset);
    pending->lrq_flags &= ~(LRQ_deadlock | LRQ_scanned);
    }
}

static LRQ deadlock_scan (
    OWN		owner,
    LRQ		request)
{
/**************************************
 *
 *	d e a d l o c k _ s c a n
 *
 **************************************
 *
 * Functional description
 *	Given an owner block that has been stalled for some time, find
 *	a deadlock cycle if there is one.  If a deadlock is found, return
 *	the address of a pending lock request in the deadlock request.
 *	If no deadlock is found, return null.
 *
 **************************************/
LRQ	victim;
BOOLEAN	maybe_deadlock = FALSE;

LOCK_TRACE (("deadlock_scan: owner %ld request %ld\n", REL_PTR (owner), REL_PTR (request)));

ASSERT_ACQUIRED;
++LOCK_header->lhb_scans;
post_history (his_scan, request->lrq_owner, request->lrq_lock, REL_PTR (request), TRUE);
deadlock_clear();

#ifdef VALIDATE_LOCK_TABLE
validate_lhb (LOCK_header);
#endif

victim = deadlock_walk (request, &maybe_deadlock);

/* Only when it is certain that this request is not part of a deadlock do we
   mark this request as 'scanned' so that we will not check this request again. 
   Note that this request might be part of multiple deadlocks. */

if (!victim && !maybe_deadlock)
    owner->own_flags |= OWN_scanned;
#ifdef DEBUG
else if (!victim && maybe_deadlock)
    DEBUG_MSG (0,("deadlock_scan: not marking due to maybe_deadlock\n"));
#endif

return victim;
}

static LRQ deadlock_walk (
    LRQ		request,
    BOOLEAN	*maybe_deadlock)
{
/**************************************
 *
 *	d e a d l o c k _ w a l k
 *
 **************************************
 *
 * Functional description
 *	Given a request that is waiting, determine whether a deadlock has
 *	occured.
 *
 **************************************/
SRQ	que;
LRQ	block, target;
LBL	lock;
OWN	owner;
PTR	pending_offset;
BOOLEAN	conversion;

/* If this request was scanned for deadlock earlier than don't
   visit it again. */

if (request->lrq_flags & LRQ_scanned)
    return NULL;

/* If this request has been seen already during this deadlock-walk, then we
   detected a circle in the wait-for graph.  Return "deadlock". */

if (request->lrq_flags & LRQ_deadlock)
    return request;

/* Remember that this request is part of the wait-for graph. */

request->lrq_flags |= LRQ_deadlock;

/* Check if this is a conversion request. */

conversion = (request->lrq_state > LCK_null) ? TRUE : FALSE;
    
/* Find the parent lock of the request */

lock = (LBL) ABS_PTR (request->lrq_lock);

/* Loop thru the requests granted against the lock.  If any granted request is
   blocking the request we're handling, recurse to find what's blocking him. */

QUE_LOOP (lock->lbl_requests, que)
    {
    block = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));

    /* Note that the default for LOCK_ordering is 1, and the default can be
       changed with the isc_config modifier 'V4_LOCK_GRANT_ORDER'. */

    if (!LOCK_ordering || conversion)
	{
    	/* Don't pursue our own lock-request again. */

    	if (request == block)
	    continue;

	/* Since lock conversions can't follow the fairness rules (to avoid
	   deadlocks), only granted lock requests need to be examined. */
	/* If lock-ordering is turned off (opening the door for starvation),
	   only granted requests can block our request. */

	if (COMPATIBLE (request->lrq_requested, block->lrq_state))
	    continue;
	}
    else
	{
    	/* Don't pursue our own lock-request again.  In addition, don't look
	   at requests that arrived after our request because lock-ordering
	   is in effect. */

    	if (request == block)
	    break;

	/* Since lock ordering is in effect, granted locks and waiting
	   requests that arrived before our request could block us. */

	if (COMPATIBLE (request->lrq_requested, MAX (block->lrq_state, block->lrq_requested)))
	    continue;
	}

    /* Don't pursue lock owners that are not blocked themselves 
       (they can't cause a deadlock). */

    owner = (OWN) ABS_PTR (block->lrq_owner);

    /* Don't pursue lock owners that still have to finish processing their AST.
       If the blocking queue is not empty, then the owner still has some
       AST's to process (or lock reposts).
       Remember this fact because they still might be part of a deadlock. */

    if (owner->own_ast_flags & OWN_signaled ||
	!QUE_EMPTY ((owner->own_blocks)))
	{
	*maybe_deadlock = TRUE;
	continue;
	}

		/* YYY: Note: can the below code be moved to the
		start of this block?  Before the OWN_signaled check?
		*/

    /* Get pointer to the waiting request whose owner also owns a lock
       that blocks the input request. */

    if (!(pending_offset = owner->own_pending_request))
       	continue;
    
    target = (LRQ) ABS_PTR (pending_offset);

    /* If this waiting request is not pending anymore, then things are changing
       and this request cannot be part of a deadlock. */

    if (!(target->lrq_flags & LRQ_pending))
	continue;

    /* Check who is blocking the request whose owner is blocking the input
       request. */

    if (target = deadlock_walk (target, maybe_deadlock))
	return target;
    }

/* This branch of the wait-for graph is exhausted, the current waiting
   request is not part of a deadlock. */

request->lrq_flags &= ~LRQ_deadlock;
request->lrq_flags |= LRQ_scanned;
return NULL;
}


static void dequeue (
    PTR		request_offset)
{
/**************************************
 *
 *	d e q u e u e
 *
 **************************************
 *
 * Functional description
 *	Release an outstanding lock.
 *
 **************************************/
LRQ		request;

/* Acquire the data structure, and compute addresses of both lock
   request and lock */

request = get_request (request_offset);
post_history (his_deq, request->lrq_owner, request->lrq_lock, request_offset, TRUE);
request->lrq_ast_routine = NULL;
release_request (request);
}

#ifdef DEBUG

static ULONG delay_count = 0;
static ULONG last_signal_line = 0;
static ULONG last_delay_line = 0;

static void debug_delay (
    ULONG	lineno)
{
/**************************************
 *
 *	d e b u g _ d e l a y
 *
 **************************************
 *
 * Functional description
 *	This is a debugging routine, the purpose of which is to slow
 *	down operations in order to expose windows of critical 
 *	sections.
 *
 **************************************/

ULONG	i;

/* Delay for a while */

last_delay_line = lineno;
for (i = 0; i < 10000; i++)
   /* Nothing */;


/* Occasionally post a signal to ourselves, provided we aren't
 * in the signal handler already and we've gotten past the initialization code.
 */
if (LOCK_asts < 1)
    if (((delay_count++ % 20) == 0) && LOCK_block_signal && LOCK_owner_offset)
	{
	last_signal_line = lineno;
	kill (getpid(), LOCK_block_signal);
	}


/* Occasionally crash for robustness testing */
/*
if ((delay_count % 500) == 0)
    exit (-1);
*/

for (i = 0; i < 10000; i++)
   /* Nothing */;

}
#endif

static void exit_handler (
    void	*arg)
{
/**************************************
 *
 *	e x i t _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Release the process block, any outstanding locks,
 *	and unmap the lock manager.  This is usually called
 *	by the cleanup handler.
 *
 **************************************/
STATUS	local_status [20];
SRQ	que;
OWN	owner;
PTR	owner_offset;

if (!LOCK_header)
    return;

#ifndef SUPERSERVER
/* For a superserver (e.g. Netware), since the server is going away, 
   the semaphores cleanups are also happening; things are in a flux; 
   all the threads are going away -- so don't get into any trouble 
   by doing purge_owner() below. */

/* Get rid of all the owners belonging to the current process */

if (owner_offset = LOCK_owner_offset)
    {
#if (defined WIN_NT || defined OS2_ONLY ||defined NEXT || defined SOLARIS_MT)
    shutdown_blocking_thread (local_status);
#else
#ifdef MMAP_SUPPORTED
    if (LOCK_owner)
        ISC_unmap_object (local_status, &LOCK_data, &LOCK_owner, sizeof (struct own));
#endif
#endif
    if (owner_offset != LOCK_header->lhb_active_owner)
    	acquire (DUMMY_OWNER_DELETE);

    QUE_LOOP (LOCK_header->lhb_owners, que)
    	{
    	owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    	if (owner->own_process_id == LOCK_pid)
	    {
	    que = (SRQ) ABS_PTR (que->srq_backward);
	    purge_owner (REL_PTR (owner), owner);
	    break;
	    }
    	}
    release_mutex();
    LOCK_owner_offset = 0;
    }

#endif

ISC_unmap_file (local_status, &LOCK_data, 0);
}

static LBL find_lock (
    PTR		parent,
    USHORT	series,
    UCHAR	*value,
    USHORT	length,
    USHORT	*slot)
{
/**************************************
 *
 *	f i n d _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Find a lock block given a resource
 *	name. If it doesn't exist, the hash
 *	slot will be useful for enqueing a
 *	lock.
 *
 **************************************/
LBL	lock;
SRQ	que, hash_header;
UCHAR	*p, *q;
ULONG	hash_value;
USHORT	hash_slot;
USHORT	l;

/* Hash the value preserving its distribution as much as possible */

hash_value = 0;
q = value;

for (l = 0, q = value; l < length; l++)
    {
    if (!(l & 3))
	p = (UCHAR*) &hash_value;
    *p++ = *q++;
    }

/* See if the lock already exists */

hash_slot = *slot = (USHORT) (hash_value % LOCK_header->lhb_hash_slots);
ASSERT_ACQUIRED;
hash_header = &LOCK_header->lhb_hash [hash_slot];

for (que = (SRQ) ABS_PTR (hash_header->srq_forward);
     que != hash_header; que = (SRQ) ABS_PTR (que->srq_forward))
    {
    lock = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_hash));
    if (lock->lbl_series != series ||
	lock->lbl_length != length ||
	lock->lbl_parent != parent)
	continue;
    if (l = length)
	{
	p = value;
	q = lock->lbl_key;
	do if (*p++ != *q++) break; while (--l);
	}
    if (!l)
	return lock;
    }

return NULL;
}

#ifdef MANAGER_PROCESS
static USHORT fork_lock_manager (
    STATUS	*status_vector)
{
/**************************************
 *
 *	f o r k _ l o c k _ m a n a g e r
 *
 **************************************
 *
 * Functional description
 *	Fork lock manager process.
 *
 **************************************/
TEXT		string [256];
struct stat	stat_buf;
int		pid;

/* Probe the lock manager executable to see if it plausibly exists. */

#ifdef DEBUG_MANAGER
strcpy (string, DEBUG_MANAGER);
#else
gds__prefix (string, LOCK_MANAGER);
#endif
if (statistics (string, &stat_buf) == -1)
    {
    bug (status_vector, "can't start lock manager");
    return FALSE;
    }

if (!(pid = vfork()))
    {
    if (!vfork())
	{
	execl (string, string, (SCHAR*) 0);
	_exit (FINI_ERROR);
	}
    _exit (FINI_OK);
    }

if (pid == -1)
    {
    bug (status_vector, "can't start lock manager");
    return FALSE;
    }

while (waitpid (pid, NULL, 0) == -1 && SYSCALL_INTERRUPTED(errno))
    /* do nothing */;

return TRUE;
}
#endif

#ifdef MANAGER_PROCESS
static OWN get_manager (
    BOOLEAN	flag)
{
/**************************************
 *
 *	g e t _ m a n a g e r
 *
 **************************************
 *
 * Functional description
 *	Find lock manager process.  If we can't find one, start
 *	one.
 *
 **************************************/
OWN		owner;
SRQ		que;

/* Look for lock manager */

ASSERT_ACQUIRED;
/* Let's perform a quick search if possible */

if (LOCK_header->lhb_manager)
    {
    owner = (OWN) ABS_PTR (LOCK_header->lhb_manager);
    if (owner->own_flags & OWN_manager)
       	return owner;
    }

/* Perform a long, hard search thru the owner blocks. */

QUE_LOOP (LOCK_header->lhb_owners, que)
    {
    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    if (owner->own_flags & OWN_manager)
        {
	LOCK_header->lhb_manager = REL_PTR (owner);
	return owner;
	}
    }

/* Oops -- no lock manager.  */

if (flag)
    fork_lock_manager ((STATUS*) NULL);

return NULL;
}
#endif

static LRQ get_request (
    PTR		offset)
{
/**************************************
 *
 *	g e t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Locate and validate user supplied request offset.
 *
 **************************************/
LRQ	request;
LBL	lock;
TEXT	s [32];

request = (LRQ) ABS_PTR (offset);
if ((SLONG) offset == -1 || request->lrq_type != type_lrq)
    {
    sprintf (s, "invalid lock id (%d)", offset);
    bug (NULL, s);
    }

lock = (LBL) ABS_PTR (request->lrq_lock);
if (lock->lbl_type != type_lbl)
    {
    sprintf (s, "invalid lock (%d)", offset);
    bug (NULL, s);
    }

return request;
}

static void grant (
    LRQ		request,
    LBL		lock)
{
/**************************************
 *
 *	g r a n t
 *
 **************************************
 *
 * Functional description
 *	Grant a lock request.  If the lock is a conversion, assume the caller
 *	has already decremented the former lock type count in the lock block.
 *
 **************************************/

/* Request must be for THIS lock */
CHECK (REL_PTR (lock) == request->lrq_lock);

post_history (his_grant, request->lrq_owner, request->lrq_lock, REL_PTR (request), TRUE);

#ifdef NeXT
++lock->lbl_eventcount;
#endif
++lock->lbl_counts [request->lrq_requested];
request->lrq_state = request->lrq_requested;
if (request->lrq_data)
    {
    remove_que (&lock->lbl_lhb_data);
    if (lock->lbl_data = request->lrq_data)
       insert_data_que (lock);
    request->lrq_data = 0;
    }

lock->lbl_state = lock_state (lock);
if (request->lrq_flags & LRQ_pending)
    {
    request->lrq_flags &= ~LRQ_pending;
    lock->lbl_pending_lrq_count--;
    }
post_wakeup ((OWN) ABS_PTR (request->lrq_owner));
}

static PTR grant_or_que (
    LRQ		request,
    LBL		lock,
    SSHORT	lck_wait)
{
/**************************************
 *
 *	g r a n t _ o r _ q u e
 *
 **************************************
 *
 * Functional description
 *	There is a request against an existing lock.  If the request
 *	is compatible with the lock, grant it.  Otherwise que it.
 *	If the lock is que-ed, set up the machinery to do a deadlock
 *	scan in awhile.
 *
 **************************************/
PTR	request_offset;
PTR	owner_offset;

request_offset = REL_PTR (request);
request->lrq_lock = REL_PTR (lock);

/* Compatible requests are easy to satify.  Just post the request
   to the lock, update the lock state, release the data structure,
   and we're done. */

if (COMPATIBLE (request->lrq_requested, lock->lbl_state))
    if (!LOCK_ordering ||
	request->lrq_requested == LCK_null ||
	(lock->lbl_pending_lrq_count == 0))
	{
	grant (request, lock);
	post_pending (lock);
	release (request->lrq_owner);
	return request_offset;
	}

/* The request isn't compatible with the current state of the lock.
 * If we haven't be asked to wait for the lock, return now. 
 */

if (lck_wait)
    {

#ifdef WINDOWS_ONLY
    /* 
     * In IB16, map wait transactions to nowait by setting wait to -1 
     * to force wait_for_request to timeout after 10 seconds. 
     */
    lck_wait = -1;
#endif 

    (void) wait_for_request (request, lck_wait, (STATUS*) NULL);

    /* For performance reasons, we're going to look at the 
     * request's status without re-acquiring the lock table.
     * This is safe as no-one can take away the request, once
     * granted, and we're doing a read-only access
     */
    request = (LRQ) ABS_PTR (request_offset);

    /* Request HAS been resolved */
    CHECK (!(request->lrq_flags & LRQ_pending));

    if (!(request->lrq_flags & LRQ_rejected))
	return request_offset;
    acquire (request->lrq_owner);
    }

request = (LRQ) ABS_PTR (request_offset);
post_history (his_deny, request->lrq_owner, request->lrq_lock, REL_PTR (request), TRUE);
ASSERT_ACQUIRED;
++LOCK_header->lhb_denies;
if (lck_wait < 0)
    ++LOCK_header->lhb_timeouts;
owner_offset = request->lrq_owner;
release_request (request);
release (owner_offset);

return NULL;
}

static STATUS init_lock_table (
    STATUS	*status_vector)
{
/**************************************
 *
 *	i n i t _ l o c k _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Initialize the global lock table for the first time.
 *	Read the config file, map the shared region, etc. 
 *
 **************************************/
TEXT		*lock_file;
#ifdef WIN_NT
TEXT		buffer [256];
#else
#ifdef WINDOWS_ONLY
TEXT		*buffer = (TEXT*) gds__alloc ((SLONG) BUFFER_MEDIUM);
#else
TEXT		buffer [256];
#endif
#endif

LOCK_shm_size = DEFAULT_SIZE;
LOCK_sem_count = SEMAPHORES;
LOCK_block_signal = BLOCKING_SIGNAL;
#ifdef WAKEUP_SIGNAL
LOCK_wakeup_signal = WAKEUP_SIGNAL;
#endif
LOCK_hash_slots = HASH_MIN_SLOTS;
LOCK_scan_interval = WAIT_TIME;
LOCK_acquire_spins = 0;

/* LOCK_ordering is TRUE (ON) by default.  It may be changed 
   by the V4_LOCK_GRANT_ORDER option in the configuration file.
   A value of 0 for that option turns lock ordering off */

LOCK_ordering = TRUE;  

ISC_get_config (LOCK_HEADER, LOCK_hdrtbl);

if (LOCK_hash_slots < HASH_MIN_SLOTS)
    LOCK_hash_slots = HASH_MIN_SLOTS;
if (LOCK_hash_slots > HASH_MAX_SLOTS)
    LOCK_hash_slots = HASH_MAX_SLOTS;

#if (defined SOLARIS_MT && !defined SUPERSERVER)
if (LOCK_solaris_stall < SOLARIS_MIN_STALL)
    LOCK_solaris_stall = SOLARIS_MIN_STALL;
if (LOCK_solaris_stall > SOLARIS_MAX_STALL)
    LOCK_solaris_stall = SOLARIS_MAX_STALL;
#endif

LOCK_pid = getpid(); 

gds__prefix_lock (buffer, LOCK_FILE);
lock_file = buffer;
#ifdef UNIX
LOCK_data.sh_mem_semaphores = LOCK_sem_count;
#endif
#ifdef NETWARE_386
LOCK_data.sh_mem_semaphores = 1;
#endif
if (!(LOCK_header = (LHB) ISC_map_file (status_vector, lock_file,
	lock_initialize, NULL_PTR, LOCK_shm_size, &LOCK_data)))
    {
#ifdef WINDOWS_ONLY
    if (buffer)
       	gds__free ((SLONG*) buffer);
#endif

/* In Superserver lock table init happens only once and 
   if error occurs it could be considered a startup error.
   There is almost nothing the server could do without
   the lock table, let's exit
*/
#if (defined SUPERSERVER && defined UNIX)
    gds__log_status (NULL, status_vector);
    exit (STARTUP_ERROR);
#endif
    return status_vector [1];
    }

/* Now get the globally consistent value of LOCK_ordering from the 
   shared lock table. */

LOCK_ordering = (LOCK_header->lhb_flags & LHB_lock_ordering) ? TRUE : FALSE;

gds__register_cleanup (exit_handler, NULL_PTR);

/* Define a place holder process owner block */

init_owner_block (&LOCK_process_owner, 255 /*LCK_OWNER_dummy_process*/, 
    LOCK_header->lhb_process_count++, OWN_BLOCK_dummy);

#ifdef WINDOWS_ONLY
if (buffer)
    gds__free ((SLONG*) buffer);
#endif

return SUCCESS;
}

static void init_owner_block (
    OWN		owner,
    UCHAR	owner_type,
    ULONG	owner_id,
    USHORT	new_block)
{
/**************************************
 *
 *	i n i t _ o w n e r _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Initialize the passed owner block nice and new. 
 *
 **************************************/

owner->own_type = type_own;
owner->own_owner_type = owner_type;
owner->own_flags = 0;
owner->own_ast_flags = 0;
owner->own_ast_hung_flags = 0;
owner->own_count = 1;
owner->own_owner_id = owner_id;
QUE_INIT (owner->own_lhb_owners);
QUE_INIT (owner->own_requests);
QUE_INIT (owner->own_blocks);
owner->own_pending_request = 0;

owner->own_process_id = LOCK_pid;
#ifdef UNIX
owner->own_process_uid = getuid();
#endif
#ifdef mpexl
AIF_get_pid (0, 0, &owner->own_process_uid);
owner->own_mpexl_async_port = ISC_get_port (TRUE);
owner->own_mpexl_sync_port = ISC_get_port (FALSE);
#endif
#if (defined WIN_NT || defined OS2_ONLY)
owner->own_process_uid = 0;
#endif

owner->own_acquire_time = 0;
owner->own_acquire_realtime = 0;
owner->own_semaphore = 0;

#ifdef USE_EVENTS
#ifdef OS2_ONLY
if (new_block != OWN_BLOCK_dummy)
    ISC_event_init (owner->own_wakeup, 0, 0);
#endif
#endif

if (new_block == OWN_BLOCK_new)
    {
#ifdef WIN_NT
    owner->own_wakeup_hndl = ISC_make_signal (TRUE, TRUE, LOCK_pid, LOCK_wakeup_signal);
#endif
#ifdef USE_EVENTS
#ifdef NETWARE_386
    ISC_event_init (owner->own_wakeup, NULL_PTR, 0);
#endif
#if (defined WIN_NT && defined SUPERSERVER)
    ISC_event_init (owner->own_wakeup, 0, 0);
#endif
#ifdef SOLARIS
    ISC_event_init (owner->own_wakeup, 0, 1);
#endif
#ifdef SOLARIS_MT
    ISC_event_init (owner->own_blocking, 0, 1);
    ISC_event_init (owner->own_stall, 0, 1);
#endif
#if (defined POSIX_THREADS && defined SUPERSERVER)
    ISC_event_init (owner->own_wakeup, 0, 0);
#endif

#ifdef UNIX
#if !(defined SOLARIS || defined POSIX_THREADS)

    owner->own_wakeup [0].event_semid = LOCK_data.sh_mem_mutex_arg;
    owner->own_wakeup [0].event_semnum = 0;
    owner->own_wakeup [0].event_count = 0;
#endif
#endif
#endif
    }
}

#ifdef USE_EVENTS
static void lock_alarm_handler (
    EVENT       event)
{
/**************************************
 *
 *      l o c k _ a l a r m _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *      This handler would just post the passed event.
 *      This is to take care of the situation when an alarm handler
 *      expires before the wait on the samaphore starts.  This way, we can
 *      avoid an indefinite wait when there is nobody to poke the
 *      semaphore and the timer also expired becuase of some asynchronous
 *      event (signal) handling.
 *
 **************************************/

ISC_event_post (event);
}
#endif

static void lock_initialize (
    void	*arg,
    SH_MEM	shmem_data,
    int		initialize)
{
/**************************************
 *
 *	l o c k _ i n i t i a l i z e
 *
 **************************************
 *
 * Functional description
 *	Initialize the lock header block.  The caller is assumed
 *	to have an exclusive lock on the lock file.
 *
 **************************************/
SSHORT		length;
USHORT		i, j;
SRQ		que;
SHB		shb;
HIS		history;
PTR		*prior;
SMB		semaphores;

#if (defined WIN_NT || defined OS2_ONLY)
if (ISC_mutex_init (MUTEX, LOCK_FILE))
    bug (NULL, "mutex init failed");
#endif
#ifdef mpexl
if (ISC_mutex_init (MUTEX, shmem_data->sh_mem_mutex_arg))
    bug (NULL, "mutex init failed");
#endif

LOCK_header = (LHB) shmem_data->sh_mem_address;

if (!initialize)
    return;

start_manager = TRUE;

memset (LOCK_header, 0, sizeof (struct lhb));
LOCK_header->lhb_type = type_lhb;
LOCK_header->lhb_version = LHB_VERSION;

/* Mark ourselves as active owner to prevent assert() checks */
LOCK_header->lhb_active_owner = DUMMY_OWNER_CREATE;	/* In init of lock system */

QUE_INIT (LOCK_header->lhb_owners);
QUE_INIT (LOCK_header->lhb_free_owners);
QUE_INIT (LOCK_header->lhb_free_locks);
QUE_INIT (LOCK_header->lhb_free_requests);
#if !(defined WIN_NT || defined OS2_ONLY || defined mpexl)
if (ISC_mutex_init (MUTEX, shmem_data->sh_mem_mutex_arg))
    bug (NULL, "mutex init failed");
#endif

LOCK_header->lhb_hash_slots = (USHORT) LOCK_hash_slots;
LOCK_header->lhb_scan_interval = LOCK_scan_interval;
LOCK_header->lhb_acquire_spins = LOCK_acquire_spins;

/* Initialize lock series data queues and lock hash chains. */

for (i = 0, que = LOCK_header->lhb_data; i < LCK_MAX_SERIES; i++, que++)
    QUE_INIT ((*que));
for (i = 0, que = LOCK_header->lhb_hash; i < LOCK_header->lhb_hash_slots; i++, que++)
    QUE_INIT ((*que));

/* Set lock_ordering flag for the first time */

if (LOCK_ordering)
    LOCK_header->lhb_flags |= LHB_lock_ordering;

length = sizeof (struct lhb) + (LOCK_header->lhb_hash_slots * sizeof (LOCK_header->lhb_hash [0]));
LOCK_header->lhb_length = shmem_data->sh_mem_length_mapped;
LOCK_header->lhb_used = ALIGN (length, sizeof (IPTR));

if (!(shb = (SHB) alloc (sizeof (struct shb), NULL)))
    {
    gds__log ("Fatal lock manager error: lock manager out of room");
    exit (STARTUP_ERROR);
    }
LOCK_header->lhb_secondary = REL_PTR (shb);
shb->shb_type = type_shb;
shb->shb_flags = 0;
shb->shb_remove_node = 0;
shb->shb_insert_que = 0;
shb->shb_insert_prior = 0;
for (i = sizeof (shb->shb_misc) / sizeof (shb->shb_misc [0]); i--;)
    shb->shb_misc [i] = 0;

/* Allocate a sufficiency of history blocks */

for (j = 0; j < 2; j++)
    {
    prior = (j == 0) ? &LOCK_header->lhb_history : &shb->shb_history;

    for (i = 0; i < HISTORY_BLOCKS; i++)
	{
	if (!(history = (HIS) alloc (sizeof (struct his), NULL)))
	    {
	    gds__log ("Fatal lock manager error: lock manager out of room");
	    exit (STARTUP_ERROR);
	    }
	*prior = REL_PTR (history);
	history->his_type = type_his;
	history->his_operation = 0;
	prior = &history->his_next;
	}

    history->his_next = (j == 0) ? LOCK_header->lhb_history : shb->shb_history;
    }

#ifdef STATIC_SEMAPHORES
/* Initialize the semaphores. Allocate semaphore block. */

if (!(semaphores = (SMB) alloc (sizeof (struct smb) + 
    (LOCK_sem_count / BITS_PER_LONG) * sizeof (ULONG), NULL)))
    {
    gds__log ("Fatal lock manager error: lock manager out of room");
    exit (STARTUP_ERROR);
    }
LOCK_header->lhb_mask = REL_PTR (semaphores);
semaphores->smb_type = type_smb;
for (i = LOCK_sem_count / BITS_PER_LONG + 1; i--;)
    semaphores->smb_mask [i] = 0;
for (i = 1; i < (USHORT) LOCK_sem_count; i++)
    {
    EVENT_T	local_event;

    ISC_event_init (&local_event, shmem_data->sh_mem_mutex_arg, i);
    semaphores->smb_mask [i / BITS_PER_LONG] |= 1L << (i % BITS_PER_LONG);
    }
#endif

/* Done initializing, unmark owner information */
LOCK_header->lhb_active_owner = 0;
}

static void insert_data_que (
    LBL		lock)
{
/**************************************
 *
 *	i n s e r t _ d a t a _ q u e
 *
 **************************************
 *
 * Functional description
 *	Insert a node in the lock series data queue
 *	in sorted (ascending) order by lock data.
 *
 **************************************/
LBL	lock2;
SRQ	data_header, que;

if (lock->lbl_series < LCK_MAX_SERIES && lock->lbl_parent && lock->lbl_data)
    {
    data_header = &LOCK_header->lhb_data [lock->lbl_series];

    for (que = (SRQ) ABS_PTR (data_header->srq_forward);
     	 que != data_header; que = (SRQ) ABS_PTR (que->srq_forward))
    	{
    	lock2 = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_data));
    	CHECK (lock2->lbl_series == lock->lbl_series);
    	if (lock2->lbl_parent != lock->lbl_parent)
	    continue;

    	if (lock->lbl_data <= lock2->lbl_data)
	    break;
    	}
    
    insert_tail (que, &lock->lbl_lhb_data);
    }
}

static void insert_tail (
    SRQ		que,
    SRQ		node)
{
/**************************************
 *
 *	i n s e r t _ t a i l
 *
 **************************************
 *
 * Functional description
 *	Insert a node at the tail of a que.
 *
 *	To handle the event of the process terminating during
 *	the insertion of the node, we set values in the shb to
 *	indicate the node being inserted.
 *	Then, should we be unable to complete
 *	the node insert, the next process into the lock table
 *	will notice the uncompleted work and undo it,
 *	eg: it will put the queue back to the state
 *	prior to the insertion being started.
 *
 **************************************/
SRQ	prior;
SHB	recover;

ASSERT_ACQUIRED;
recover = (SHB) ABS_PTR (LOCK_header->lhb_secondary);
DEBUG_DELAY;
recover->shb_insert_que = REL_PTR (que);
DEBUG_DELAY;
recover->shb_insert_prior = que->srq_backward;
DEBUG_DELAY;

node->srq_forward = REL_PTR (que);
DEBUG_DELAY;
node->srq_backward = que->srq_backward;

DEBUG_DELAY;
prior = (SRQ) ABS_PTR (que->srq_backward);
DEBUG_DELAY;
prior->srq_forward = REL_PTR (node);
DEBUG_DELAY;
que->srq_backward = REL_PTR (node);
DEBUG_DELAY;

recover->shb_insert_que = 0;
DEBUG_DELAY;
recover->shb_insert_prior = 0;
DEBUG_DELAY;
}

static USHORT lock_state (
    LBL		lock)
{
/**************************************
 *
 *	l o c k _ s t a t e
 *
 **************************************
 *
 * Functional description
 *	Compute the current state of a lock.
 *
 **************************************/

if (lock->lbl_counts [LCK_EX])
    return LCK_EX;
else if (lock->lbl_counts [LCK_PW])
    return LCK_PW;
else if (lock->lbl_counts [LCK_SW])
    return LCK_SW;
else if (lock->lbl_counts [LCK_PR])
    return LCK_PR;
else if (lock->lbl_counts [LCK_SR])
    return LCK_SR;
else if (lock->lbl_counts [LCK_null])
    return LCK_null;

return LCK_none;
}

#ifdef NeXT
static void port_waker (
    PTR		*owner_offset_ptr)
{
/**************************************
 *
 *      p o r t _ w a k e r
 *
 **************************************
 *
 * Functional description
 *      A thread whose sole purpose is to get woken up by
 *      the alarm handler and set the condition.   This is
 *      because the alarm handler operates in the same thread
 *      that is waiting for the condition, and NeXT doesn't
 *      let you do that in a foolproof way.
 *
 **************************************/
kern_return_t   r;

for (;;)
    {
    if ((r = msg_receive (null_msg1, MSG_OPTION_NONE, 0)) != RCV_SUCCESS)
	{
	if (r == RCV_INVALID_PORT)
	    cthread_exit (thread_self());
	else
	    bug (NULL, "port waker msg_receive");
	}
    wakeup_action (owner_offset_ptr);
    }
}
#endif

static void post_blockage (
    LRQ		request,
    LBL		lock,
    BOOLEAN	force)
{
/**************************************
 *
 *	p o s t _ b l o c k a g e
 *
 **************************************
 *
 * Functional description
 *	The current request is blocked.  Post blocking notices to
 *	any process blocking the request.
 *
 **************************************/
OWN		owner, blocking_owner, manager;
SRQ		que, next_que;
PTR		next_que_offset;
LRQ		block;

owner = (OWN) ABS_PTR (request->lrq_owner);

ASSERT_ACQUIRED;
CHECK (owner->own_pending_request == REL_PTR (request));
CHECK (request->lrq_flags & LRQ_pending);

for (que = QUE_NEXT (lock->lbl_requests); que != &lock->lbl_requests; que = (SRQ) ABS_PTR (next_que_offset))
    {
    next_que = QUE_NEXT ((*que));
    next_que_offset = REL_PTR (next_que);
    block = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));

    /* Figure out if this lock request is blocking our own lock request.
       Of course, our own request cannot block ourselves.  Compatible
       requests don't block us, and if there is no AST routine for the
       request the block doesn't matter as we can't notify anyone.
       If the owner has marked the request with "LRQ_blocking_seen 
       then the blocking AST has been delivered and the owner promises
       to release the lock as soon as possible (so don't bug the owner) */

    if (block == request ||
	COMPATIBLE (request->lrq_requested, block->lrq_state) ||
	!block->lrq_ast_routine ||
	((block->lrq_flags & LRQ_blocking_seen) && !force))
	continue;

    blocking_owner = (OWN) ABS_PTR (block->lrq_owner);

    /* Add the blocking request to the list of blocks if it's not
       there already (LRQ_blocking) */

    if (!(block->lrq_flags & LRQ_blocking))
    	{
    	insert_tail (&blocking_owner->own_blocks, &block->lrq_own_blocks);
    	block->lrq_flags |= LRQ_blocking;
    	block->lrq_flags &= ~LRQ_blocking_seen;
	}

    if (force)
       	blocking_owner->own_ast_flags &= ~OWN_signaled;
    if (blocking_owner != owner &&
        signal_owner (blocking_owner, REL_PTR (owner)))
	{
	/* We can't signal the blocking_owner, assume he has died
	   and purge him out */

#ifndef mpexl
	que = (SRQ) ABS_PTR (que->srq_backward);
	purge_owner (REL_PTR (owner), blocking_owner);
#endif
	}
    if (block->lrq_state == LCK_EX)
        break;
    }
}

static void post_history (
    USHORT	operation,
    PTR		process,
    PTR		lock,
    PTR		request,
    BOOLEAN	old_version)
{
/**************************************
 *
 *	p o s t _ h i s t o r y
 *
 **************************************
 *
 * Functional description
 *	Post a history item.
 *
 **************************************/
HIS	history;

if (old_version)
    {
    history = (HIS) ABS_PTR (LOCK_header->lhb_history);
    ASSERT_ACQUIRED;
    LOCK_header->lhb_history = history->his_next;
    }
else
    {
    SHB		shb;

    ASSERT_ACQUIRED;
    shb = (SHB) ABS_PTR (LOCK_header->lhb_secondary);
    history = (HIS) ABS_PTR (shb->shb_history);
    shb->shb_history = history->his_next;
    }

history->his_operation = operation;
history->his_process = process;
history->his_lock = lock;
history->his_request = request;
}

static void post_pending (
    LBL		lock)
{
/**************************************
 *
 *	p o s t _ p e n d i n g
 *
 **************************************
 *
 * Functional description
 *	There has been a change in state of a lock.  Check pending
 *	requests to see if something can be granted.  If so, do it.
 *
 **************************************/
LRQ	request;
SRQ	que;
UCHAR	temp_state;
OWN	owner;
#ifdef DEV_BUILD
USHORT	pending_counter = 0;
#endif

if (lock->lbl_pending_lrq_count == 0)
    return;

/* Loop thru granted requests looking for pending conversions.  If one
   is found, check to see if it can be granted.  Even if a request cannot
   be granted for compatibility reason, post_wakeup () that owner so that
   it can post_blockage() to the newly granted owner of the lock. */

QUE_LOOP (lock->lbl_requests, que)
    {
    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
    if (!(request->lrq_flags & LRQ_pending))
	continue;
    if (request->lrq_state)
	{
	--lock->lbl_counts [request->lrq_state];
	temp_state = lock_state (lock);
	if (COMPATIBLE (request->lrq_requested, temp_state))
	    grant (request, lock);
	else
	    {
#ifdef DEV_BUILD
	    pending_counter++;
#endif
	    ++lock->lbl_counts [request->lrq_state];
	    owner = (OWN) ABS_PTR (request->lrq_owner);
	    post_wakeup (owner);
	    if (LOCK_ordering)
		{
		CHECK (lock->lbl_pending_lrq_count >= pending_counter);
		return;
		}
	    }
	}
    else
	if (COMPATIBLE (request->lrq_requested, lock->lbl_state))
	    grant (request, lock);
	else
	    {
#ifdef DEV_BUILD
	    pending_counter++;
#endif
	    owner = (OWN) ABS_PTR (request->lrq_owner);
	    post_wakeup (owner);
	    if (LOCK_ordering)
		{
		CHECK (lock->lbl_pending_lrq_count >= pending_counter);
		return;
		}
	    }
    }
    
CHECK (lock->lbl_pending_lrq_count == pending_counter);
}

#ifdef USE_EVENTS
static void post_wakeup (
    OWN		owner)
{
/**************************************
 *
 *	p o s t _ w a k e u p		( U S E _ E V E N T S )
 *
 **************************************
 *
 * Functional description
 *	Wakeup whoever is waiting on a lock.
 *
 **************************************/
USHORT	semaphore;

/* Note: own_semaphore is set to 1 if we are in wait_for_request() */

if (!(semaphore = owner->own_semaphore) || (semaphore & OWN_semavail))
    return;

++LOCK_header->lhb_wakeups;
owner->own_flags |= OWN_wakeup;
ISC_event_post (owner->own_wakeup);
}
#endif

#ifndef USE_EVENTS
static void post_wakeup (
    OWN		owner)
{
/**************************************
 *
 *	p o s t _ w a k e u p		( n o n - U S E _ E V E N T S )
 *
 **************************************
 *
 * Functional description
 *	Wakeup whoever is waiting on a lock.
 *
 **************************************/

if (!owner->own_semaphore)
    return;

if (owner->own_flags & OWN_wakeup)
    return;

++LOCK_header->lhb_wakeups;
owner->own_flags |= OWN_wakeup;

#ifdef mpexl
ISC_kill (owner->own_process_id, (SSHORT) LOCK_wakeup_signal,
    owner->own_mpexl_sync_port);
#else
#ifdef WIN_NT
ISC_kill (owner->own_process_id, LOCK_wakeup_signal, owner->own_wakeup_hndl);
#else
#ifndef WINDOWS_ONLY
ISC_kill (owner->own_process_id, LOCK_wakeup_signal);
#endif
#endif
#endif
}
#endif

#ifndef SUPERSERVER
static BOOLEAN probe_owners (
    PTR		probing_owner_offset)
{
/**************************************
 *
 *	p r o b e _ o w n e r s
 *
 **************************************
 *
 * Functional description
 *	Probe an owner to see if it still exists.  If it doesn't, get
 *	rid of it.
 *
 **************************************/
SRQ	que;
OWN	owner;
BOOLEAN	purged;

purged = FALSE;

ASSERT_ACQUIRED;
QUE_LOOP (LOCK_header->lhb_owners, que)
    {
    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    if (owner->own_flags & OWN_signal)
        signal_owner (owner, (PTR)NULL);
    if (owner->own_process_id != LOCK_pid &&
	!ISC_check_process_existence (owner->own_process_id, owner->own_process_uid, FALSE))
	{
	que = (SRQ) ABS_PTR (que->srq_backward);
	purge_owner (probing_owner_offset, owner);
	purged = TRUE;
	}
    }

return purged;
}
#endif /* SUPERSERVER */

static void purge_owner (
    PTR		purging_owner_offset,
    OWN		owner)
{
/**************************************
 *
 *	p u r g e _ o w n e r    
 *
 **************************************
 *
 * Functional description
 *	Purge an owner and all of its associated locks.
 *
 **************************************/
SRQ	que;
LRQ	request;

LOCK_TRACE (("purge_owner (%ld)\n", purging_owner_offset));

post_history (his_del_owner, purging_owner_offset, REL_PTR (owner), 0, FALSE);

#ifdef STATIC_SEMAPHORES
release_semaphore (owner);
#endif

/* Release any locks that are active. */

while ((que = QUE_NEXT (owner->own_requests)) != &owner->own_requests)
    {
    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_own_requests));
    release_request (request);
    }

/* Release any repost requests left dangling on blocking queue. */

while ((que = QUE_NEXT (owner->own_blocks)) != &owner->own_blocks)
    {
    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_own_blocks));
    remove_que (&request->lrq_own_blocks);
    request->lrq_type = type_null;
    insert_tail (&LOCK_header->lhb_free_requests, &request->lrq_lbl_requests);
    }

#ifdef USE_EVENTS
#ifdef OS2_ONLY
if (owner->own_process_id == LOCK_pid)
    ISC_event_fini (owner->own_wakeup);
#endif
#endif

#ifdef MANAGER_PROCESS
if (owner->own_flags & OWN_manager)
    LOCK_header->lhb_manager = 0;
#endif

/* Release owner block */

remove_que (&owner->own_lhb_owners);
insert_tail (&LOCK_header->lhb_free_owners, &owner->own_lhb_owners);

owner->own_owner_type = 0;
owner->own_owner_id = 0;
owner->own_process_id = 0;
owner->own_flags = 0;
}

static void remove_que (
    SRQ		node)
{
/**************************************
 *
 *	r e m o v e _ q u e
 *
 **************************************
 *
 * Functional description
 *	Remove a node from a self-relative que.
 *
 *	To handle the event of the process terminating during
 *	the removal of the node, we set shb_remove_node to the
 *	node to be removed.  Then, should we be unsuccessful
 *	in the node removal, the next process into the lock table
 *	will notice the uncompleted work and complete it.
 *
 *	Note that the work is completed by again calling this routine,
 *	specifing the node to be removed.  As the prior and following
 *	nodes may have changed prior to the crash, we need to redo the
 *	work only based on what is in <node>.  
 *
 **************************************/
SRQ	que;
SHB	recover;

ASSERT_ACQUIRED;
recover = (SHB) ABS_PTR (LOCK_header->lhb_secondary);
DEBUG_DELAY;
recover->shb_remove_node = REL_PTR (node);
DEBUG_DELAY;

que = (SRQ) ABS_PTR (node->srq_forward);

/* The next link might point back to us, or our prior link should
 * the backlink change occur before a crash
 */
CHECK (que->srq_backward == REL_PTR (node) || 
       que->srq_backward == node->srq_backward);
DEBUG_DELAY;
que->srq_backward = node->srq_backward;

DEBUG_DELAY;
que = (SRQ) ABS_PTR (node->srq_backward);

/* The prior link might point forward to us, or our following link should
 * the change occur before a crash
 */
CHECK (que->srq_forward == REL_PTR (node) || 
       que->srq_forward == node->srq_forward);
DEBUG_DELAY;
que->srq_forward = node->srq_forward;

DEBUG_DELAY;
recover->shb_remove_node = 0;
DEBUG_DELAY;

/* To prevent trying to remove this entry a second time, which could occur
 * for instance, when we're removing an owner, and crash while removing
 * the owner's blocking requests, reset the que entry in this node.
 * Note that if we get here, shb_remove_node has been cleared, so we
 * no longer need the queue information.
 */

QUE_INIT ((*node));
}

static void release (
    PTR		owner_offset)
{
/**************************************
 *
 *	r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Release the mapped lock file.  Advance the event count to wake up
 *	anyone waiting for it.  If there appear to be blocking items
 *	posted.
 *
 **************************************/
OWN	owner;

#ifndef ANY_THREADING
if (owner_offset)
    {
    owner = (OWN) ABS_PTR (owner_offset);
    if (!QUE_EMPTY (owner->own_blocks))
        signal_owner (owner, (PTR)NULL);
    }
#endif

if (owner_offset && LOCK_header->lhb_active_owner != owner_offset)
    bug (NULL, "release when not owner");

#ifdef MANAGER_PROCESS
if (LOCK_post_manager && (owner = get_manager (TRUE)))
    post_wakeup (owner);
#endif

#if (defined SOLARIS_MT && !defined SUPERSERVER)

if (LOCK_solaris_stall)
    {
    OWN     first_owner;

    if (owner_offset)
	owner = (OWN) ABS_PTR (owner_offset);

    /* Rotate first owner to tail of active owners' queue
       in search of mutex-starved owners. */

    first_owner = (OWN) ((UCHAR*) QUE_NEXT (LOCK_header->lhb_owners) -
                OFFSET (OWN, own_lhb_owners));
    remove_que (&first_owner->own_lhb_owners);
    insert_tail (&LOCK_header->lhb_owners, &first_owner->own_lhb_owners);

    /* If others stepped aside to let us run then wake them up now. */

    if (owner_offset && owner->own_flags & OWN_starved)
        {
        SRQ         que;

        owner->own_flags &= ~(OWN_starved | OWN_blocking);
        QUE_LOOP (LOCK_header->lhb_owners, que)
            {
            owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
            if (owner->own_flags & OWN_blocking)
                {
                owner->own_flags &= ~OWN_blocking;
		ISC_event_post (owner->own_stall);
                }
            }
        }

    }
#endif /* SOLARIS_MT */

#ifdef VALIDATE_LOCK_TABLE
/* Validate the lock table occasionally (every 500 releases) */
if ((LOCK_header->lhb_acquires % (HISTORY_BLOCKS/2)) == 0)
    validate_lhb (LOCK_header);
#endif

release_mutex();
}

static void release_mutex (void)
{
/**************************************
 *
 *	r e l e a s e _ m u t e x
 *
 **************************************
 *
 * Functional description
 *	Release the mapped lock file.  Advance the event count to wake up
 *	anyone waiting for it.  If there appear to be blocking items
 *	posted.
 *
 **************************************/

DEBUG_DELAY;
ISC_inhibit();
DEBUG_DELAY;

if (!LOCK_header->lhb_active_owner)
    bug (NULL, "release when not active");

LOCK_header->lhb_active_owner = 0;

if (ISC_mutex_unlock (MUTEX))
    bug (NULL, "semop failed (release)");

#ifndef ANY_THREADING
DEBUG_DELAY;
--LOCK_asts;
#endif

DEBUG_DELAY;
ISC_enable();
DEBUG_DELAY;
}

static void release_request (
    LRQ		request)
{
/**************************************
 *
 *	r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request.  This is called both by release lock
 *	and by the cleanup handler.
 *
 **************************************/
LBL		lock;

ASSERT_ACQUIRED;

/* Start by disconnecting request from both lock and process */

remove_que (&request->lrq_lbl_requests);
remove_que (&request->lrq_own_requests);

request->lrq_type = type_null;
insert_tail (&LOCK_header->lhb_free_requests, &request->lrq_lbl_requests);
lock = (LBL) ABS_PTR (request->lrq_lock);

/* If the request is marked as blocking, clean it up. */

if (request->lrq_flags & LRQ_blocking)
    {
    remove_que (&request->lrq_own_blocks);
    request->lrq_flags &= ~LRQ_blocking;
    }

request->lrq_flags &= ~LRQ_blocking_seen;

/* Update counts if we are cleaning up something we're waiting on!
   This should only happen if we are purging out an owner that died */
if (request->lrq_flags & LRQ_pending)
    {
    request->lrq_flags &= ~LRQ_pending;
    lock->lbl_pending_lrq_count--;
    }

/* If there are no outstanding requests, release the lock */

if (QUE_EMPTY (lock->lbl_requests))
    {
    CHECK (lock->lbl_pending_lrq_count == 0);
    remove_que (&lock->lbl_lhb_hash);
    remove_que (&lock->lbl_lhb_data);
    lock->lbl_type = type_null;
    insert_tail (&LOCK_header->lhb_free_locks, &lock->lbl_lhb_hash);
    return;
    }

/* Re-compute the state of the lock and post any compatible pending
   requests. */

if ((request->lrq_state != LCK_none) && 
    !(--lock->lbl_counts [request->lrq_state]))
    {
    lock->lbl_state = lock_state (lock);
    if (request->lrq_state != LCK_null)
	{
    	post_pending (lock);
	return;
	}
    }

/* If a lock enqueue failed because of a deadlock or timeout, the pending
   request may be holding up compatible, pending lock requests queued
   behind it. 
   Or, even if this request had been granted, it's now being released,
   so we might be holding up requests or conversions queued up behind. */

post_pending (lock);
}

#ifdef STATIC_SEMAPHORES
static void release_semaphore (
    OWN		owner)
{
/**************************************
 *
 *	r e l e a s e _ s e m a p h o r e
 *
 **************************************
 *
 * Functional description
 *	Release an un-used and unloved semaphore.
 *
 **************************************/
USHORT	semaphore;
SMB	semaphores;

if (semaphore = owner->own_semaphore)
    {
    semaphore &= ~OWN_semavail;
    semaphores = (SMB) ABS_PTR (LOCK_header->lhb_mask);
    semaphores->smb_mask [semaphore / BITS_PER_LONG] |= 1L << (semaphore % BITS_PER_LONG);
    owner->own_semaphore = 0;
    owner->own_wakeup [0].event_semnum = 0;
    }
}
#endif

static void shutdown_blocking_thread (
    STATUS	*status_vector)
{
/**************************************
 *
 *	s h u t d o w n _ b l o c k i n g _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *	Perform a controlled shutdown of blocking thread
 *	to avoid tragic misunderstandings when unmapping
 *	memory.
 *
 **************************************/

/* Zeroing the owner offset implicitly flags the
   blocking thread to exit. */
   
LOCK_owner_offset = 0;

#ifdef SUPERSERVER
return;
#endif

#ifdef NeXT
getout = 1;		/* set getout flag, cancel alarms and deallcate port */
alarm (0);
port_deallocate (task_self(), alarm_port);
#endif

#if (defined WIN_NT && !defined SUPERSERVER)
SetEvent (blocking_event [0]);
CloseHandle (blocking_event [0]);
CloseHandle (wakeup_event [0]);
#endif

#ifdef OS2_ONLY
DosPostEventSem (blocking_event [0]);
DosCloseEventSem (blocking_event [0]);
#endif

#ifndef SUPERSERVER
#ifdef SOLARIS_MT
/* Poke the blocking action thread to wakeup and notice
   the exit flag. In turn, wait on an event ourselves
   which the blocking action thread should post on
   the way out. If it doesn't post, wakeup after a
   timeout and cleanup anyway. */

if (LOCK_owner)
    {
    EVENT	event_ptr;
    SLONG	value;
    
    /* Set a marker for the AST thread to know it's time to cleanup */
    LOCK_owner->own_semaphore = 1;

    event_ptr = LOCK_owner->own_wakeup;
    value = ISC_event_clear (event_ptr);

    /* Wakeup the AST thread - it might be blocking or stalled */
    ISC_event_post (LOCK_owner->own_blocking);
    ISC_event_post (LOCK_owner->own_stall);

    /* Tell the scheduler to allow AST's to run */
    AST_ENABLE;

    /* Wait for the AST thread to finish cleanup or for 10 seconds */
    ISC_event_wait (1, &event_ptr, &value, 10 * 1000000, lock_alarm_handler, event_ptr);

    /* Either AST thread terminated, or our timeout expired */

    /* Finish our cleanup */
    ISC_unmap_object (status_vector, &LOCK_data, &LOCK_owner, sizeof (struct own));
    }
#endif
#endif
}

static int signal_owner (
    OWN         blocking_owner,
    PTR         blocked_owner_offset)
{
/**************************************
 *
 *	s i g n a l _ o w n e r
 *
 **************************************
 *
 * Functional description
 *	Send a signal to a process.
 *
 *      The second parameter is a possible offset to the
 *      blocked owner (or NULL), which is passed on to
 *      blocking_action2().
 *
 **************************************/
OWN	owner;


/*post_history (his_signal, LOCK_header->lhb_iactive_owner, REL_PTR (blocking_owner), 0, TRUE);*/

ASSERT_ACQUIRED;

/* If a process, other than ourselves, hasn't yet seen a signal
   that was sent, don't bother to send another one. */

DEBUG_DELAY;

#if (!defined ANY_THREADING && !defined SUPERSERVER)
if ((blocking_owner->own_ast_flags & OWN_signaled) ||
    (blocking_owner->own_ast_hung_flags & OWN_hung))
#else
if (blocking_owner->own_ast_flags & OWN_signaled)
#endif
    {
    DEBUG_MSG (1,("signal_owner (%ld) - skipped OWN_signaled\n", blocking_owner->own_process_id));
    return SUCCESS;
    }

blocking_owner->own_ast_flags |= OWN_signaled;
DEBUG_DELAY;
blocking_owner->own_flags &= ~OWN_signal;
#if (defined SUPERSERVER || !defined ANY_THREADING)
if (blocking_owner->own_process_id == LOCK_pid)
    {
    DEBUG_DELAY;
    ISC_inhibit();
    DEBUG_DELAY;
    blocking_action2 (REL_PTR (blocking_owner), blocked_owner_offset);
    DEBUG_DELAY;
    ISC_enable();
    DEBUG_DELAY;
    return SUCCESS;
    }
#endif

#ifdef MANAGER_PROCESS
/* If we can deliver the signal directly, do so.  */

if ((LOCK_process_owner.own_flags & OWN_manager) ||
    blocking_owner->own_process_uid == LOCK_process_owner.own_process_uid)
    {
    if (LOCK_process_owner.own_flags & OWN_manager)
       	++LOCK_header->lhb_indirect_sigs;
    else
       	++LOCK_header->lhb_direct_sigs;
    
    /* Following cases are included */
#endif

DEBUG_DELAY;

#if !(defined mpexl || defined OS2_ONLY || defined WIN_NT || defined SOLARIS_MT || POSIX_THREADS)
#ifndef WINDOWS_ONLY
    if (ISC_kill (blocking_owner->own_process_id, LOCK_block_signal) != -1)
       return SUCCESS;
#endif
#endif

#ifdef mpexl
    if (!(errno = ISC_kill (blocking_owner->own_process_id,
	(SSHORT) LOCK_block_signal, blocking_owner->own_mpexl_async_port)))
       return SUCCESS;
#endif

#ifdef OS2_ONLY
    if (!(errno = ISC_kill (blocking_owner->own_process_id, blocking_owner->own_blocking_hndl)))
       return SUCCESS;
#endif

#if (defined WIN_NT && !defined SUPERSERVER)
    if (ISC_kill (blocking_owner->own_process_id, LOCK_block_signal, blocking_owner->own_blocking_hndl) != -1)
       return SUCCESS;
#endif
    
#if (defined SOLARIS_MT && !defined SUPERSERVER)
    if (!(errno = ISC_event_post (blocking_owner->own_blocking)))
       return SUCCESS;
#endif

#ifdef MANAGER_PROCESS
    /* End block started above */
    }
#endif

DEBUG_MSG (1,("signal_owner - direct delivery failed\n"));

blocking_owner->own_ast_flags &= ~OWN_signaled;
DEBUG_DELAY;
blocking_owner->own_flags |= OWN_signal;

#ifdef MANAGER_PROCESS
/* If we can't deliver the signal, direct it toward the lock manager
   process for delivery. */

if (!(LOCK_process_owner.own_flags & OWN_manager))
    {
    LOCK_post_manager = TRUE;
    return SUCCESS;
    }
#endif

/* Conclude that the owning process is dead */
return FAILURE;
}

#ifdef VALIDATE_LOCK_TABLE


#define	EXPECT_inuse	0
#define	EXPECT_freed	1

#define	RECURSE_yes	0
#define	RECURSE_not	1

static void validate_history (
    PTR		history_header)
{
/**************************************
 *
 *	v a l i d a t e _ h i s t o r y
 *
 **************************************
 *
 * Functional description
 *	Validate a circular list of history blocks.
 *
 **************************************/
HIS	history;
USHORT	count = 0;

LOCK_TRACE (("validate_history: %ld\n", history_header));

for (history = (HIS) ABS_PTR (history_header); TRUE;
     history = (HIS) ABS_PTR (history->his_next))
    {
    count++;
    CHECK (history->his_type == type_his);
    CHECK (history->his_operation >= 0);
    CHECK (history->his_operation <= his_MAX);
    if (history->his_next == history_header)
	break;
    CHECK (count <= HISTORY_BLOCKS);
    }
}
#endif

#ifdef VALIDATE_LOCK_TABLE
static void validate_lhb (
    LHB		lhb)
{
/**************************************
 *
 *	v a l i d a t e _ l h b
 *
 **************************************
 *
 * Functional description
 *	Validate the LHB structure and everything that hangs off of it.
 *
 **************************************/

SRQ	que, que_next;
OWN	owner;
LBL	lock;
LRQ	request;

LOCK_TRACE (("validate_lhb:\n"));

/* Prevent recursive reporting of validation errors */
if (LOCK_bugcheck)
    return;

ISC_inhibit();

CHECK (lhb != NULL);

CHECK (lhb->lhb_type == type_lhb);
CHECK (lhb->lhb_version == LHB_VERSION);

validate_shb (lhb->lhb_secondary);
if (lhb->lhb_active_owner > 0)
    validate_owner (lhb->lhb_active_owner, EXPECT_inuse);

QUE_LOOP (lhb->lhb_owners, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    validate_owner (REL_PTR(owner), EXPECT_inuse);
    }

QUE_LOOP (lhb->lhb_free_owners, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    owner = (OWN) ((UCHAR*) que - OFFSET (OWN, own_lhb_owners));
    validate_owner (REL_PTR(owner), EXPECT_freed);
    }

QUE_LOOP (lhb->lhb_free_locks, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    lock = (LBL) ((UCHAR*) que - OFFSET (LBL, lbl_lhb_hash));
    validate_lock (REL_PTR(lock), EXPECT_freed, (PTR) 0);
    }

QUE_LOOP (lhb->lhb_free_requests, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
    validate_request (REL_PTR(request), EXPECT_freed, RECURSE_not);
    }

CHECK (lhb->lhb_used <= lhb->lhb_length);

#ifdef UNIX
#ifndef NeXT
#ifndef ANY_THREADING
CHECK (lhb->lhb_mutex[0].mtx_semid == LOCK_data.sh_mem_mutex_arg);
#endif
#endif
#endif

validate_history (lhb->lhb_history);
/* validate_semaphore_mask (lhb->lhb_mask); */

CHECK (lhb->lhb_reserved[0] == 0);
CHECK (lhb->lhb_reserved[1] == 0);

DEBUG_MSG (0, ("validate_lhb completed:\n"));

ISC_enable();

}
#endif

#ifdef VALIDATE_LOCK_TABLE
static void validate_lock (
    PTR		lock_ptr,
    USHORT	freed,
    PTR		lrq_ptr)
{
/**************************************
 *
 *	v a l i d a t e _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Validate the lock structure and everything that hangs off of it.
 *
 **************************************/
LBL	lock;
SRQ	que, que_next;
LRQ	request;
USHORT	found;
UCHAR	highest_request;
USHORT	found_pending;
USHORT	i;
USHORT	direct_counts [LCK_max];
struct lbl lock_copy;

LOCK_TRACE (("validate_lock: %ld\n", lock_ptr));

lock = (LBL) ABS_PTR (lock_ptr);
lock_copy = *lock;

if (freed == EXPECT_freed)
    CHECK (lock->lbl_type == type_null)
else
    CHECK (lock->lbl_type == type_lbl);

CHECK (lock->lbl_state >= LCK_none);
CHECK (lock->lbl_state <  LCK_max);

CHECK (lock->lbl_length <= lock->lbl_size);

/* The lbl_count's should never roll over to be negative */
for (i = 0; i < sizeof (lock->lbl_counts)/sizeof (lock->lbl_counts[0]); i++)
    CHECK (!(lock->lbl_counts [i] & 0x8000))

/* The count of pending locks should never roll over to be negative */
CHECK (!(lock->lbl_pending_lrq_count & 0x8000));

memset (direct_counts, 0, sizeof (direct_counts));

found = 0;
found_pending = 0;
highest_request = LCK_none;
QUE_LOOP (lock->lbl_requests, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    /* Any requests of a freed lock should also be free */
    CHECK (freed == EXPECT_inuse);

    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
    /* Note: Don't try to validate_request here, it leads to recursion */

    if (REL_PTR (request) == lrq_ptr)
	found++;

    CHECK (found <= 1);			/* check for a loop in the queue */

    /* Request must be for this lock */
    CHECK (request->lrq_lock == lock_ptr);

    if (request->lrq_requested > highest_request)
	highest_request = request->lrq_requested;

    /* If the request is pending, then it must be incompatible with current
       state of the lock - OR lock_ordering is enabled and there is at 
       least one pending request in the queue (before this request 
       but not including it). */

    if (request->lrq_flags & LRQ_pending)
	{
	CHECK (!COMPATIBLE (request->lrq_requested, lock->lbl_state) ||
		(LOCK_ordering && found_pending));

	/* The above condition would probably be more clear if we 
	   wrote it as the following:

	   CHECK (!COMPATIBLE (request->lrq_requested, lock->lbl_state) ||
		   (LOCK_ordering && found_pending &&
		    COMPATIBLE (request->lrq_requested, lock->lbl_state)));

	   but that would be redundant
	*/

	found_pending++;
	}

    /* If the request is NOT pending, then it must be rejected or 
       compatible with the current state of the lock */
    if (!(request->lrq_flags & LRQ_pending))
        {
	CHECK ((request->lrq_flags & LRQ_rejected) || 
	       (request->lrq_requested == lock->lbl_state) ||
               COMPATIBLE (request->lrq_requested, lock->lbl_state));

	}

    direct_counts [request->lrq_state]++;
    }

if ((freed == EXPECT_inuse) && (lrq_ptr != 0))
    CHECK (found == 1);			/* request is in lock's queue */


if (freed == EXPECT_inuse)
    {
    CHECK (found_pending == lock->lbl_pending_lrq_count);

    /* The counter in the lock header should match the actual counts
       lock->lbl_counts [LCK_null] isn't maintained, so don't check it */
    for (i = LCK_null; i < LCK_max; i++)
	CHECK (direct_counts [i] == lock->lbl_counts [i]);
    }

if (lock->lbl_parent && (freed == EXPECT_inuse))
    validate_lock (lock->lbl_parent, EXPECT_inuse, (PTR) 0);

}
#endif

#ifdef VALIDATE_LOCK_TABLE
static void validate_owner (
    PTR		own_ptr,
    USHORT	freed)
{
/**************************************
 *
 *	v a l i d a t e _ o w n e r
 *
 **************************************
 *
 * Functional description
 *	Validate the owner structure and everything that hangs off of it.
 *
 **************************************/
OWN	owner;
SRQ	que, que_next;
SRQ	que2, que2_next;
LRQ	request;
LRQ	request2;
USHORT	found;
PTR	owner_own_pending_request;
struct own	owner_copy;

LOCK_TRACE (("validate_owner: %ld\n", own_ptr));

owner = (OWN) ABS_PTR (own_ptr);
owner_copy = *owner;

/* Note that owner->own_pending_request can be reset without the lock
 * table being acquired - eg: by another process.  That being the case,
 * we need to stash away a copy of it for validation.
 */
owner_own_pending_request = owner->own_pending_request;

CHECK (owner->own_type == type_own);
if (freed == EXPECT_freed)
    CHECK (owner->own_owner_type == 0)
else
    {
    CHECK (owner->own_owner_type > 0);
    CHECK (owner->own_owner_type <= 4  	  /* LCK_OWNER_transaction */
	|| owner->own_owner_type == 255); /* LCK_OWNER_dummy_process */ 
    }

CHECK (owner->own_acquire_time <= LOCK_header->lhb_acquires);
CHECK (owner->own_acquire_realtime == 0 ||
       owner->own_acquire_realtime <= GET_TIME);

/* Check that no invalid flag bit is set */
CHECK (!(owner->own_flags & ~(OWN_blocking | OWN_scanned | OWN_manager | OWN_signal |
	OWN_wakeup | OWN_starved)));

/* Check that no invalid flag bit is set */
CHECK (!(owner->own_ast_flags & ~(OWN_signaled)));

/* Check that no invalid flag bit is set */
CHECK (!(owner->own_ast_hung_flags & ~(OWN_hung)));

/* Can't both be signal & signaled */
if (owner->own_flags & OWN_signal)
    CHECK (!(owner->own_ast_flags & OWN_signaled));

if (owner->own_ast_flags & OWN_signaled)
    CHECK (!(owner->own_flags & OWN_signal));

QUE_LOOP (owner->own_requests, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    CHECK (freed == EXPECT_inuse); /* should not be in loop for freed owner */

    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_own_requests));
    validate_request (REL_PTR(request), EXPECT_inuse, RECURSE_not);
    CHECK (request->lrq_owner == own_ptr);

    /* Make sure that request marked as blocking also exists in the blocking list */

    if (request->lrq_flags & LRQ_blocking)
	{
	found = 0;
	QUE_LOOP (owner->own_blocks, que2)
	    {
	    /* Validate that the next backpointer points back to us */
	    que2_next = QUE_NEXT ((*que2));
	    CHECK (que2_next->srq_backward == REL_PTR (que2));

	    request2 = (LRQ) ((UCHAR*) que2 - OFFSET (LRQ, lrq_own_blocks));
	    CHECK (request2->lrq_owner == own_ptr);

	    if (REL_PTR (request2) == REL_PTR (request))
		found++;

	    CHECK (found <= 1);	/* watch for loops in queue */
	    }
	CHECK (found == 1);	/* request marked as blocking must be in blocking queue */
	}
    }

/* Check each item in the blocking queue */

QUE_LOOP (owner->own_blocks, que)
    {
    /* Validate that the next backpointer points back to us */
    que_next = QUE_NEXT ((*que));
    CHECK (que_next->srq_backward == REL_PTR (que));

    CHECK (freed == EXPECT_inuse); /* should not be in loop for freed owner */

    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_own_blocks));
    validate_request (REL_PTR(request), EXPECT_inuse, RECURSE_not);

    LOCK_TRACE (("Validate own_block: %ld\n", REL_PTR (request)));

    CHECK (request->lrq_owner == own_ptr);

    /* A repost won't be in the request list */

    if (request->lrq_flags & LRQ_repost)
	continue;

    /* Make sure that each block also exists in the request list */

    found = 0;
    QUE_LOOP (owner->own_requests, que2)
	{
	/* Validate that the next backpointer points back to us */
	que2_next = QUE_NEXT ((*que2));
	CHECK (que2_next->srq_backward == REL_PTR (que2));

	request2 = (LRQ) ((UCHAR*) que2 - OFFSET (LRQ, lrq_own_requests));
	CHECK (request2->lrq_owner == own_ptr);

	if (REL_PTR (request2) == REL_PTR (request))
	    found++;

	CHECK (found <= 1);	/* watch for loops in queue */
	}
    CHECK (found == 1);		/* blocking request must be in request queue */
    }

/* If there is a pending request, make sure it is valid, that it
 * exists in the queue for the lock.
 */
if (owner_own_pending_request && (freed == EXPECT_inuse))
    {
    BOOLEAN	found_pending;
    BOOLEAN	found;
    LRQ		request;
    LBL		lock;
    LRQ		pending, blocked;
    SRQ		que_of_lbl_requests;
    SRQ		que_of_own_blocks;
    OWN		pending_owner;

    /* Make sure pending request is valid, and we own it */
    request = (LRQ) ABS_PTR (owner_own_pending_request);
    validate_request (REL_PTR(request), EXPECT_inuse, RECURSE_not);
    CHECK (request->lrq_owner == own_ptr);

    /* Make sure the lock the request is for is valid */
    lock = (LBL) ABS_PTR (request->lrq_lock);
    validate_lock (REL_PTR (lock), EXPECT_inuse, (PTR) 0);

    /* Make sure the pending request is on the list of requests for the lock */
     
    found_pending = FALSE;
    QUE_LOOP (lock->lbl_requests, que_of_lbl_requests)
	{
	pending = (LRQ) ((UCHAR*) que_of_lbl_requests - OFFSET (LRQ, lrq_lbl_requests));
	if (REL_PTR (pending) == owner_own_pending_request)
	    {
	    found_pending = TRUE;
	    break;
	    }
	}

    /* pending request must exist in the lock's request queue */
    CHECK (found_pending);

    /* Either the pending request is the same as what we stashed away, or it's
     * been cleared by another process without the lock table acquired.  */
    CHECK ((owner_own_pending_request == owner->own_pending_request) ||
	   !owner->own_pending_request);
    }
#ifdef  STATIC_SEMAPHORES
{
USHORT	semaphore;
if ((semaphore = owner->own_semaphore) && (freed == EXPECT_inuse))
    {
    SMB	semaphores;
    if (!(semaphore & OWN_semavail))
	{
	/* Check that the semaphore allocated is flagged as in use */
        semaphores = (SMB) ABS_PTR (LOCK_header->lhb_mask);
        CHECK (!(semaphores->smb_mask [semaphore / BITS_PER_LONG] & (1L << (semaphore % BITS_PER_LONG))));
	}
    }
}
#endif
}
#endif

#ifdef VALIDATE_LOCK_TABLE
static void validate_request (
    PTR		lrq_ptr,
    USHORT	freed,
    USHORT	recurse)
{
/**************************************
 *
 *	v a l i d a t e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Validate the request structure and everything that hangs off of it.
 *
 **************************************/
LRQ	request;
struct lrq	request_copy;

LOCK_TRACE (("validate_request: %ld\n", lrq_ptr));

request = (LRQ) ABS_PTR (lrq_ptr);
request_copy = *request;

if (freed == EXPECT_freed)
    CHECK (request->lrq_type == type_null)
else
    CHECK (request->lrq_type == type_lrq);

/* Check that no invalid flag bit is set */
CHECK (!(request->lrq_flags & ~(LRQ_blocking| LRQ_pending| LRQ_converting| LRQ_rejected| LRQ_timed_out| LRQ_deadlock| LRQ_repost| LRQ_scanned |LRQ_blocking_seen)));

/* LRQ_converting & LRQ_timed_out are defined, but never actually used */
CHECK (!(request->lrq_flags & (LRQ_converting|LRQ_timed_out)));

/* Once a request is rejected, it CAN'T be pending any longer */
if (request->lrq_flags & LRQ_rejected)
    CHECK (!(request->lrq_flags & LRQ_pending));

/* Can't both be scanned & marked for deadlock walk */
CHECK ((request->lrq_flags & (LRQ_deadlock|LRQ_scanned)) != (LRQ_deadlock|LRQ_scanned));

CHECK (request->lrq_requested < LCK_max);
CHECK (request->lrq_state < LCK_max);

if (freed == EXPECT_inuse)
    {
    if (recurse == RECURSE_yes)
	validate_owner (request->lrq_owner, EXPECT_inuse);

    /* Reposted request are pseudo requests, not attached to any real lock */
    if (!(request->lrq_flags & LRQ_repost))
	validate_lock (request->lrq_lock, EXPECT_inuse, REL_PTR (request));
    }
}
#endif

#ifdef VALIDATE_LOCK_TABLE
static void validate_shb (
    PTR		shb_ptr)
{
/**************************************
 *
 *	v a l i d a t e _ s h b
 *
 **************************************
 *
 * Functional description
 *	Validate the SHB structure and everything that hangs off of
 *	it.
 *	Of course, it would have been a VERY good thing if someone
 *	had moved this into lhb when we made a unique v4 lock
 *	manager....
 *	1995-April-13 David Schnepper 
 *
 **************************************/
SHB	shb;
USHORT	i;

LOCK_TRACE (("validate_shb: %ld\n", shb_ptr));

shb = (SHB) ABS_PTR (shb_ptr);

CHECK (shb->shb_type == type_shb);

validate_history (shb->shb_history);

for (i = 0; i < sizeof (shb->shb_misc)/sizeof (shb->shb_misc[0]); i++)
    CHECK (shb->shb_misc [i] == 0);
}
#endif

static USHORT wait_for_request (
    LRQ		request,
    SSHORT	lck_wait,
    STATUS	*status_vector)
{
/**************************************
 *
 *	w a i t _ f o r _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	There is a request that needs satisfaction, but is waiting for
 *	somebody else.  Mark the request as pending and go to sleep until
 *	the lock gets poked.  When we wake up, see if somebody else has
 *	cleared the pending flag.  If not, go back to sleep.
 * Returns
 *	SUCCESS	- we waited until the request was granted or rejected
 * 	FAILURE - Insufficient resouces to wait (eg: no semaphores)
 *
 **************************************/
#ifndef WINDOWS_ONLY
LBL		lock;
PTR		owner_offset, request_offset, lock_offset;
OWN		owner, blocking_owner;
LRQ		blocking_request;
int		ret;
SLONG		timeout, current_time, scan_interval, deadlock_timeout, lock_timeout;
USHORT		semaphore, eventcount;
SLONG		value;
EVENT		event_ptr;
#ifdef DEV_BUILD
ULONG		repost_counter = 0;
#endif

ASSERT_ACQUIRED;

++LOCK_header->lhb_waits;
scan_interval = LOCK_header->lhb_scan_interval;
#ifdef mpexl
scan_interval *= 1000;
#endif

/* lrq_count will be off if we wait for a pending request */
CHECK (!(request->lrq_flags & LRQ_pending));

request->lrq_flags &= ~LRQ_rejected;
request->lrq_flags |= LRQ_pending;
owner_offset = request->lrq_owner;
lock_offset = request->lrq_lock;
lock = (LBL) ABS_PTR (lock_offset);
lock->lbl_pending_lrq_count++;
if (LOCK_ordering)
    {
    if (!request->lrq_state)
	{
	/* If ordering is in effect, and this is a conversion of
	   an existing lock in LCK_none state - put the lock to the
           end of the list so it's not taking cuts in the lineup */
	remove_que (&request->lrq_lbl_requests);
	insert_tail (&lock->lbl_requests, &request->lrq_lbl_requests);
	}
    }

owner = (OWN) ABS_PTR (owner_offset);
owner->own_pending_request = request_offset = REL_PTR (request);
#ifndef NeXT
owner->own_flags &= ~(OWN_scanned | OWN_wakeup);
#else
owner->own_flags &= ~OWN_scanned;
#endif

#ifdef STATIC_SEMAPHORES
/* If semaphore hasn't been allocated for lock, allocate it now. */

semaphore = owner->own_semaphore &= ~OWN_semavail;

if (!semaphore)
    if (!(semaphore = alloc_semaphore (owner, status_vector)))
	return FAILURE;
#else
/* On non-static semaphore systems, set a flag to indicate that we're
   in wait_for_request.  Post_wakeup will check this flag. */

owner->own_semaphore = 1;
#endif /* STATIC_SEMAPHORES */

#ifdef NeXT
eventcount = lock->lbl_eventcount;
#endif
#ifdef WIN_NT
#ifdef SUPERSERVER
ResetEvent (owner->own_wakeup_hndl);
#else
ResetEvent (wakeup_event [0]);
#endif
#endif

/* Post blockage.  If the blocking owner has disappeared, the blockage
   may clear spontaneously. */

post_blockage (request, lock, FALSE);
post_history (his_wait, owner_offset, lock_offset, REL_PTR (request), TRUE);
release (owner_offset);

current_time = GET_TIME;

/* If a lock timeout was requested (wait < 0) then figure
   out the time when the lock request will timeout */

if (lck_wait < 0)
    lock_timeout = current_time + (-lck_wait);
deadlock_timeout = current_time + scan_interval;

/* Wait in a loop until the lock becomes available */

while (TRUE)
    {
    /* NOTE: Many operations in this loop are done without having
     *	     the lock table acquired - for performance reasons
     */

    /* Before starting to wait - look to see if someone resolved
       the request for us - if so we're out easy! */

    ISC_inhibit ();
    request = (LRQ) ABS_PTR (request_offset);
    if (!(request->lrq_flags & LRQ_pending))
        {
        ISC_enable ();
        break;
        }
    ISC_enable ();

    /* recalculate when we next want to wake up, the lesser of a
       deadlock scan interval or when the lock request wanted a timeout */

    timeout = deadlock_timeout;
    if (lck_wait < 0 && lock_timeout < deadlock_timeout)
	timeout = lock_timeout;

    /* Prepare to wait for a timeout or a wakeup from somebody else.  */

#ifdef USE_EVENTS
    owner = (OWN) ABS_PTR (owner_offset);
#if (defined MMAP_SUPPORTED && !defined SUPERSERVER)
    if (!(LOCK_owner->own_flags & OWN_wakeup))
#else
    if (!(owner->own_flags & OWN_wakeup))
#endif
	{
	/* Re-initialize value each time thru the loop to make sure that the
	   semaphore looks 'un-poked'. */

	/* YYY: NOTE - couldn't there be "missing events" here? */
	/* We don't own the lock_table at this point, but we modify it! */

#if (defined MMAP_SUPPORTED && !defined SUPERSERVER)
        event_ptr = LOCK_owner->own_wakeup;
        value = ISC_event_clear (event_ptr);
        event_ptr = LOCK_owner->own_wakeup;
#else
	event_ptr = owner->own_wakeup;
	value = ISC_event_clear (event_ptr);
	owner = (OWN) ABS_PTR (owner_offset);
	event_ptr = owner->own_wakeup;
#endif
#if (defined MMAP_SUPPORTED && !defined SUPERSERVER)
        if (!(LOCK_owner->own_flags & OWN_wakeup))
#else
	if (!(owner->own_flags & OWN_wakeup))
#endif
	    {
/* Until here we've been the only thread in the engine (We no longer
   release engine in LCK.C module to avoid problems with more than
   one thread running in out not-yet-MT-safe engine). We already
   tried to execute AST routine provided (sometimes) by blocking
   owner hoping our lock request would be satisfied. Did not help!
   The only thing we could do now is to wait. But let's do it without
   monopolizing the engine
*/
#ifdef SUPERSERVER
	    THREAD_EXIT;
#endif
	    AST_ENABLE;
	    ret = ISC_event_wait (1, &event_ptr, &value, 
               (timeout-current_time)*1000000, lock_alarm_handler, event_ptr);
	    AST_DISABLE;
#ifdef SUPERSERVER
	    THREAD_ENTER;
#endif
	    }
	}
#endif
#ifdef NeXT
    lock = (LBL) ABS_PTR (lock_offset);
    if (lock->lbl_eventcount != eventcount)
	ret = SUCCESS;
    else
	{
	ISC_mutex_lock (MUTEX);
	lock = (LBL) ABS_PTR (lock_offset);
	if (lock->lbl_eventcount != eventcount)
	    ret = SUCCESS;
	else
	    {
            condition_set = 0;
            while (!condition_set)
	        condition_wait (&owner->own_NeXT_semaphore, (mutex_t) MUTEX);
	    lock = (LBL) ABS_PTR (lock_offset);
	    ret = (lock->lbl_eventcount != eventcount) ? SUCCESS : FAILURE;
	    eventcount = lock->lbl_eventcount;
	    }
	ISC_mutex_unlock (MUTEX);
	}
#endif
#ifdef mpexl
    ret = ISC_wait (LOCK_wakeup_signal, timeout - current_time) ? FAILURE : SUCCESS;
#endif
#ifdef WIN_NT
AST_ENABLE;
#ifdef SUPERSERVER

/* Until here weve been the only thread in the engine (We no longer
   release engine in LCK.C module to avoid problems with more than
   one thread running in out not-yet-MT-safe engine). We already
   tried to execute AST routine provided (sometimes) by blocking
   owner hoping our lock request would be satisfied. Did not help!
   The only thing we could do now is to wait. But let's do it without
   monopolizing the engine
*/
    THREAD_EXIT;
    ret = WaitForSingleObject (owner->own_wakeup_hndl, (timeout-current_time)*1000);
    THREAD_ENTER;
#else
    ret = WaitForSingleObject (wakeup_event [0], (timeout-current_time)*1000);
#endif
    AST_DISABLE;
    if (!ret || ret == WAIT_ABANDONED)
	ret = SUCCESS;
    else /* if (ret == WAIT_TIMEOUT) */
	ret = FAILURE;
#endif

    /* We've worken up from the wait - now look around and see
       why we wokeup */

    /* If somebody else has resolved the lock, we're done */

    ISC_inhibit ();
    request = (LRQ) ABS_PTR (request_offset); 
    if (!(request->lrq_flags & LRQ_pending)) 
	{
	ISC_enable (); 
	break;
	}
    ISC_enable ();

    /* See if we wokeup due to another owner deliberately waking us up
	ret==SUCCESS --> we were deliberately worken up
	ret==FAILURE --> we still don't know why we work up */

#ifndef NeXT
    /* Only if we came out of the ISC_event_wait() because of a post_wakeup()
       by another owner is OWN_wakeup set.  This is the only SUCCESS case. */

#if (defined MMAP_SUPPORTED && !defined SUPERSERVER)
    if (LOCK_owner->own_flags & OWN_wakeup)
#else
    owner = (OWN) ABS_PTR (owner_offset);
    if (owner->own_flags & OWN_wakeup)
#endif
	ret = SUCCESS;
#ifdef USE_EVENTS
    else
	ret = FAILURE;
#endif
#endif

    current_time = GET_TIME;

    /* See if we workup for a bogus reason - if so 
       go right back to sleep.  We wokeup bogus unless
	- we weren't deliberatly woken up
	- it's not time for a deadlock scan.
	- it's not time for the lock timeout to expire.
       Bogus reasons for wakeups include signal reception on some
       platforms (eg: SUN4)
       Note: we allow a 1 second leaway on declaring a bogus
       wakeup due to timing differences (we use seconds here,
       ISC_event_wait() uses finer granularity) */

    if ((ret != SUCCESS) && 
	  (current_time+1 < timeout))
	{
	continue;
	}

    /* We apparently woke up for some real reason.  
       Make sure everyone is still with us.  Then see if we're still
       blocked. */

    acquire (owner_offset);
    request = (LRQ) ABS_PTR (request_offset);/* Re-init after potential remap */
    lock = (LBL) ABS_PTR (lock_offset);
    owner = (OWN) ABS_PTR (owner_offset);
#ifndef NeXT
    owner->own_flags &= ~OWN_wakeup;
#endif

    /* Now that we own the lock table, see if the request was resolved
       while we were waiting for the lock table */

    if (!(request->lrq_flags & LRQ_pending))
	{
	release (owner_offset);
	break;
	}

    current_time = GET_TIME;			/* refetch due to wait in acquire() */

    /* See if we've waited beyond the lock timeout - if so we
       mark our own request as rejected */

    if (lck_wait < 0 && lock_timeout <= current_time)
	{
	/* We're going to reject our lock - it's the callers responsibility
	   to do cleanup and make sure post_pending() is called to wakeup
	   other owners we might be blocking */
	request->lrq_flags |= LRQ_rejected;
	request->lrq_flags &= ~LRQ_pending;
	lock->lbl_pending_lrq_count--;
	release (owner_offset);
	break;
	}

    /* We're going to do some real work - reset when we next want to
       do a deadlock scan */
    deadlock_timeout = current_time + scan_interval;


    /* Handle lock event first */
    if (ret == SUCCESS)
	{
	/* Someone posted our wakeup event, but DIDN'T grant our request.
	   Re-post what we're blocking on and continue to wait.
	   This could happen if the lock was granted to a different request,
	   we have to tell the new owner of the lock that they are blocking us. */ 

	post_blockage (request, lock, FALSE);
#ifdef WIN_NT
#ifdef SUPERSERVER
	ResetEvent (owner->own_wakeup_hndl);
#else
	ResetEvent (wakeup_event [0]);
#endif
#endif
	}

#ifndef SUPERSERVER
    /* See if all the other owners are still alive.  Dead ones will be purged,
       purging one might resolve our lock request. */

    else if (probe_owners (owner_offset) &&
	!(request->lrq_flags & LRQ_pending))
	{
	release (owner_offset);
	break;
	}
#endif /* SUPERSERVER */

    /* If we've not previously been scanned for a deadlock, go do a
       deadlock scan */

    else if (!(owner->own_flags & OWN_scanned) &&
	(blocking_request = deadlock_scan (owner, request)))
	{
	LBL	blocking_lock;

	/* Something has been selected for rejection to prevent a
	   deadlock.  Clean things up and go on.  We still have to
	   wait for our request to be resolved. */

	DEBUG_MSG (0,("wait_for_request: selecting something for deadlock kill\n"));

	++LOCK_header->lhb_deadlocks;
	blocking_request->lrq_flags |= LRQ_rejected;
	blocking_request->lrq_flags &= ~LRQ_pending;
	blocking_lock = (LBL) ABS_PTR (blocking_request->lrq_lock);
	blocking_lock->lbl_pending_lrq_count--;

	blocking_owner = (OWN) ABS_PTR (blocking_request->lrq_owner);
	blocking_owner->own_pending_request = 0;
	blocking_owner->own_flags &= ~OWN_scanned;
	if (blocking_request != request)
	    post_wakeup (blocking_owner);
	/* else
	    We rejected our own request to avoid a deadlock.
	    When we get back to the top of the master loop we
	    fall out and start cleaning up */
	}
    else
	{
	/* Our request is not resolved, all the owners are alive, there's
	   no deadlock -- there's nothing else to do.  Let's
	   make sure our request hasn't been forgotten by reminding
	   all the owners we're waiting - some plaforms under CLASSIC
	   architecture had problems with "missing signals" - which is
	   another reason to repost the blockage.
	   Also, the ownership of the lock could have changed, and we
	   weren't woken up because we weren't next in line for the lock.
	   We need to inform the new owner. */

	DEBUG_MSG (0,("wait_for_request: forcing a resignal of blockers\n"));
	post_blockage (request, lock, FALSE);
#ifdef DEV_BUILD
	repost_counter++;
	if (repost_counter % 50 == 0)
	    {
	    gds__log ("wait_for_request: owner %d reposted %ld times for lock %d",
			owner_offset, repost_counter, lock_offset);
	    DEBUG_MSG (0,("wait_for_request: reposted %ld times for this lock!\n", repost_counter));
	    }
#endif	/* DEV_BUILD */
	}
    release (owner_offset);
    }

/* NOTE: lock table is not acquired at this point */

#ifdef DEV_BUILD
request = (LRQ) ABS_PTR (request_offset);
CHECK (!(request->lrq_flags & LRQ_pending));
#endif /* DEV_BUILD */

owner = (OWN) ABS_PTR (owner_offset);
owner->own_pending_request = 0;

#ifdef STATIC_SEMAPHORES
owner->own_semaphore |= OWN_semavail;
#else
owner->own_semaphore = 0;
#endif

#endif /* !WINDOWS_ONLY */

return SUCCESS;
}

#ifdef NeXT
static void wakeup_action (
    PTR		*owner_offset_ptr)
{
/**************************************
 *
 *	w a k e u p _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Fault hander for a wakeup signal.  A waking signal
 *	is an indication that there has been activity against
 *	a lock for which there is a pending request.
 *
 **************************************/
OWN	owner;

if (*owner_offset_ptr)
    {
    acquire (*owner_offset_ptr);
    owner = (OWN) ABS_PTR (*owner_offset_ptr);
    owner->own_flags &= ~OWN_wakeup;
    condition_set = 1;
    release (*owner_offset_ptr);
    condition_broadcast (&owner->own_NeXT_semaphore);
    }
}
#endif
