/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		thd.c
 *	DESCRIPTION:	Thread support routines
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
#include <errno.h>
#include "../jrd/common.h"
#include "../jrd/thd.h"
#include "../jrd/isc.h"
#include "../jrd/thd_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/gdsassert.h"

/* Define a structure that can be used to keep
   track of thread specific data. */

typedef struct tctx {
    struct tctx	*tctx_next;
    SLONG	tctx_thread_id;
    void	*tctx_thread_data;
} *TCTX;

#ifdef APOLLO
#include <apollo/error.h>
#include <apollo/task.h>
#include <apollo/pfm.h>
#include <apollo/errlog.h>
#endif

#ifdef WIN_NT
#include <process.h>
#include <windows.h>
#define TEXT		SCHAR
#endif

#ifdef NETWARE_386
extern int	BeginThread (void (* __func)(void *), void *__stackP, unsigned __stackSize, void *__arg);
#define STACK_SIZE	128000
#endif

#ifdef OS2_ONLY
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <process.h>

#define STACK_SIZE	32000
#endif

#ifdef UNIX
#include <unistd.h>
#endif

#ifndef ANY_THREADING
THDD			gdbb;
#endif
#ifdef VMS
/* THE SOLE PURPOSE OF THE FOLLOWING DECLARATION IS TO ALLOW THE VMS KIT TO
   COMPILE.  IT IS NOT CORRECT AND MUST BE REMOVED AT SOME POINT. */
THDD			gdbb;
#endif

static TCTX	allocate_context (SLONG);
static void	init (void);
static void	init_tkey (void);
static void	put_specific (THDD);
static int	thread_start (int (*)(void *), void *, int, int, void *);
#ifdef APOLLO
static int	thread_starter (int (*)(void));
#endif

static USHORT		initialized = FALSE;
static USHORT		t_init = FALSE;

#ifdef ANY_THREADING
static MUTX_T		ib_mutex;
#if (defined APOLLO || defined OS2_ONLY)
static TCTX		thread_contexts;
static TCTX		*thread_contexts_end = &thread_contexts;
static MUTX_T		thread_create_mutex;
#endif
#ifdef APOLLO
static void		*thread_arg;
#endif
#ifdef POSIX_THREADS
static pthread_key_t	specific_key;
static pthread_key_t	t_key;
#endif
#ifdef SOLARIS_MT
static thread_key_t	specific_key;
static thread_key_t	t_key;
#endif
#ifdef WIN_NT
static DWORD		specific_key;
static DWORD		t_key;
#endif
#endif

int API_ROUTINE gds__thread_start (
    int		(*entrypoint)(void *),
    void	*arg,
    int		priority,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	g d s _ $ t h r e a d _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start a thread.
 *
 **************************************/

return thread_start (entrypoint, arg, priority, flags, thd_id);
}

long THD_get_thread_id (void)
{
/**************************************
 *
 *	T H D _ g e t _ t h r e a d _ i d
 *
 **************************************
 *
 * Functional description
 *	Get platform's notion of a thread ID.
 *
 **************************************/
long id = 1;
#ifdef OS2_ONLY
PTIB	pptib;
PPIB	pppib;
#endif

#ifdef APOLLO
id = task_$get_handle();
#endif
#ifdef OS2_ONLY
DosGetInfoBlocks (&pptib, &pppib);
id = pptib->tib_ptib2->tib2_ultid;
#endif
#ifdef WIN_NT
id = GetCurrentThreadId();
#endif
#ifdef NETWARE_386
id = GetThreadID();
#endif
#ifdef SOLARIS_MT
id = thr_self();
#endif
#ifdef POSIX_THREADS

/* The following is just a temp. decision.
*/
#ifdef HP10

id = (long)(pthread_self().field1);

#else

id = (long) pthread_self();

#endif /* HP10 */
#endif /* POSIX_THREADS */

return id;
}

#ifdef ANY_THREADING
#ifdef APOLLO
#define GET_SPECIFIC_DEFINED
THDD  THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
task_$handle_t	thread_id;
TCTX		thread;

thread_id = task_$get_handle();

for (thread = thread_contexts; thread; thread = thread->tctx_next)
    if (thread->tctx_thread_id == thread_id)
	break;

/* if we don't know anything about this thread yet,
   allocate a context block for it */

if (!thread)
    thread = allocate_context (thread_id);

return (THDD) thread->tctx_thread_data;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef NeXT
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( N e X T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return (THDD) cthread_data (cthread_self());
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef OS2_ONLY
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
PTIB	pptib;
PPIB	pppib;
TCTX	thread;

DosGetInfoBlocks (&pptib, &pppib);

for (thread = thread_contexts; thread; thread = thread->tctx_next)
    if (thread->tctx_thread_id == pptib->tib_ptib2->tib2_ultid)
	break;

/* if we don't know anything about this thread yet,
   allocate a context block for it */

if (!thread)
    thread = allocate_context (pptib->tib_ptib2->tib2_ultid);

return (THDD) thread->tctx_thread_data;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef POSIX_THREADS
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Gets thread specific data and returns
 * a pointer to it.
 *
 **************************************/
#ifdef HP10

THDD	current_context;

pthread_getspecific (specific_key, (pthread_addr_t *) &current_context);
return current_context;

#else
#ifdef DGUX

THDD   current_context;

pthread_getspecific (specific_key, (void **) &current_context);
return current_context;

#else

return ((THDD) pthread_getspecific (specific_key));

#endif /* DGUX */
#endif /* HP10 */
}
#endif /* POSIX_THREADS */
#endif /* ANY_THREADING */

#ifdef ANY_THREADING
#ifdef SOLARIS_MT
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
THDD	current_context;

if (thr_getspecific (specific_key, &current_context))
    {
    ib_perror ("thr_getspecific");
    exit (1);
    }

return current_context;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef WIN_NT
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return (THDD) TlsGetValue (specific_key);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef NETWARE_386
#define GET_SPECIFIC_DEFINED
THDD THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
return((THDD)GetThreadDataAreaPtr());
}
#endif
#endif

#ifndef GET_SPECIFIC_DEFINED
THDD DLL_EXPORT THD_get_specific (void)
{
/**************************************
 *
 *	T H D _ g e t _ s p e c i f i c		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return gdbb;
}
#endif

void THD_getspecific_data (void **t_data)
{
/**************************************
 *
 *	T H D _ g e t s p e c i f i c _ d a t a
 *
 **************************************
 *
 * Functional description
 *	return the previously stored t_data.
 *
 **************************************/

#ifdef ANY_THREADING

/* There are some circumstances in which we do not call THD_putspecific_data(),
   such as services API, and local access on NT. As result of that, t_init
   does not get initialized. So don't use an assert in here but rather do
   the work only if t_init is initialised */
if (t_init)
    {
#ifdef POSIX_THREADS
#ifdef HP10
pthread_getspecific (t_key,  t_data);
#else
#ifdef DGUX
pthread_getspecific (t_key,  t_data);
#else
*t_data =  (void *) pthread_getspecific (t_key);
#endif /* DGUX */
#endif /* HP10 */
#endif /* POSIX_THREADS */

#ifdef SOLARIS_MT
thr_getspecific (t_key, t_data);
#endif

#ifdef WIN_NT
*t_data = (void*) TlsGetValue (t_key);
#endif
    }
#endif
}

void DLL_EXPORT THD_cleanup (void)
{
/**************************************
 *
 *	T H D _ c l e a n u p
 *
 **************************************
 *
 * Functional description
 * This is the cleanup function called from the DLL
 * cleanup function. This helps to remove the allocated
 * thread specific key.
 *
 **************************************/

if (initialized)
    {
    initialized--;
#if (defined APOLLO || defined OS2_ONLY)
    THD_mutex_destroy (&thread_create_mutex); /* is this really needed seems like only on 
						 Apollo and OS2 */
#endif
#ifdef WIN_NT
    TlsFree(specific_key);
#endif
#ifdef POSIX_THREADS
#endif
#ifdef SOLARIS_MT
#endif
#ifdef ANY_THREADING
    /* destroy the mutex ib_mutex which was created */
    THD_mutex_destroy (&ib_mutex);
#endif
    }
}

void DLL_EXPORT THD_init (void)
{
/**************************************
 *
 *	T H D _ i n i t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
#ifdef POSIX_THREADS

/* In case of Posix threads we take advantage of using function
   pthread_once. This function makes sure that init() routine
   will be called only once by the first thread to call pthread_once.
*/
#ifdef HP10
static pthread_once_t once = pthread_once_init;
#else
static pthread_once_t once = PTHREAD_ONCE_INIT;
#endif /* HP10 */

pthread_once (&once, init);

#else

init();

#endif /* POSIX_THREADS */
}

void DLL_EXPORT THD_init_data (void)
{
/**************************************
 *
 *	T H D _ i n i t _ d a t a
 *
 **************************************
 *
 * Functional description
 *	init function for t_key. This is called 
 *	to ensure that the key is created.
 *
 **************************************/
#ifdef POSIX_THREADS

/* In case of Posix threads we take advantage of using function
   pthread_once. This function makes sure that init_tkey() routine
   will be called only once by the first thread to call pthread_once.
*/
#ifdef HP10
static pthread_once_t once = pthread_once_init;
#else
static pthread_once_t once = PTHREAD_ONCE_INIT;
#endif /* HP10 */

pthread_once (&once, init_tkey);

#else

init_tkey();

#endif /* POSIX_THREADS */
}

#ifdef ANY_THREADING
#ifdef APOLLO
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( A p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( A p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

mutex_$init (mutex);

return 0;
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( A p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

mutex_$lock (mutex, mutex_$wait_forever);

return 0;
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( A p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

mutex_$unlock (mutex);

return 0;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef OS2_ONLY
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return DosCloseMutexSem (mutex->mutx_mutex);
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return DosCreateMutexSem (NULL, &mutex->mutx_mutex, 0, 0);
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return DosRequestMutexSem (mutex->mutx_mutex, SEM_INDEFINITE_WAIT);
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return DosReleaseMutexSem (mutex->mutx_mutex);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef POSIX_THREADS
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y 	( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Tries to destroy mutex and returns 0 if success,
 * error number in case of error
 *
 **************************************/
#ifdef HP10

int             state;

state = pthread_mutex_destroy (mutex);
if (!state)
    return 0;
assert (state == -1);   /* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_destroy (mutex);

#endif /* HP10 */

}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Tries to initialize mutex and returns 0 if success,
 * error number in case of error
 *
 **************************************/
#ifdef HP10

int     state;

state = pthread_mutex_init (mutex, pthread_mutexattr_default);
if (!state)
    return 0;
assert (state == -1);   /* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_init (mutex, NULL);

#endif /* HP10 */
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Tries to lock mutex and returns 0 if success,
 * error number in case of error
 *
 **************************************/
#ifdef HP10

int     state;

state = pthread_mutex_lock (mutex);
if (!state)
    return 0;
assert (state == -1);   /* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_lock (mutex);

#endif /* HP10 */
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Tries to unlock mutex and returns 0 if success,
 * error number in case of error
 *
 **************************************/
#ifdef HP10

int     state;

state = pthread_mutex_unlock (mutex);
if (!state)
    return 0;
assert (state == -1);   /* if state is not 0, it should be -1 */
return errno;

#else

return pthread_mutex_unlock (mutex);

#endif /* HP10 */
}
#endif /* POSIX_THREADS */
#endif /* ANY_THREADING */

#ifdef ANY_THREADING
#ifdef SOLARIS_MT
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y 	( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return mutex_destroy (mutex);
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return mutex_init (mutex, USYNC_THREAD, NULL);
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return mutex_lock (mutex);
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return mutex_unlock (mutex);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef VMS
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( V M S )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( V M S )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ISC_mutex_init (mutex, 0);
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( V M S )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ISC_mutex_lock (mutex);
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( V M S )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ISC_mutex_unlock (mutex);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef WIN_NT
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

DeleteCriticalSection (mutex);

return 0;
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

InitializeCriticalSection (mutex);

return 0;
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

EnterCriticalSection (mutex);

return 0;
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

LeaveCriticalSection (mutex);

return 0;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef NETWARE_386
#define THREAD_MUTEXES_DEFINED
int THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (mutex->mutx_mutex != 0)
    ISC_semaphore_close (mutex->mutx_mutex);

return 0;
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

ISC_semaphore_open (&mutex->mutx_mutex, 1);
return 0;
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ISC_mutex_lock (&mutex->mutx_mutex);
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return ISC_mutex_unlock (&mutex->mutx_mutex);
}
#endif
#endif

#ifndef THREAD_MUTEXES_DEFINED
int DLL_EXPORT THD_mutex_destroy (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y	( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
 
return 0;
}

int THD_mutex_init (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int THD_mutex_lock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int THD_mutex_unlock (
    MUTX_T	*mutex)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}
#endif

int THD_mutex_destroy_n (
    MUTX_T	*mutexes,
    USHORT	n)
{
/**************************************
 *
 *	T H D _ m u t e x _ d e s t r o y _ n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

while (n--)
    THD_mutex_destroy (mutexes++);

return 0;
}

int THD_mutex_init_n (
    MUTX_T	*mutexes,
    USHORT	n)
{
/**************************************
 *
 *	T H D _ m u t e x _ i n i t _ n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

while (n--)
    THD_mutex_init (mutexes++);

return 0;
}

#ifdef ANY_THREADING
int THD_mutex_lock_global (void)
{
/**************************************
 *
 *	T H D _ m u t e x _ l o c k _ g l o b a l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return THD_mutex_lock (&ib_mutex);
}

int THD_mutex_unlock_global (void)
{
/**************************************
 *
 *	T H D _ m u t e x _ u n l o c k _ g l o b a l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return THD_mutex_unlock (&ib_mutex);
}
#endif

#ifdef V4_THREADING
#ifdef SOLARIS_MT
#ifdef THD_RWLOCK_STRUCT
#define RW_LOCK_DEFINED
int THD_wlck_destroy (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ d e s t r o y		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status;

#ifdef THREAD_DEBUG
ib_fprintf (ib_stderr, "calling rwlock_destroy %x\n", wlock);
#endif

status = rwlock_destroy (wlock);

#ifdef THREAD_DEBUG
if (status)
    ib_fprintf (ib_stderr, "status = %d errno = %d\n", status, errno);
#endif

return status;
}

int  THD_wlck_init (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ i n i t		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status;

#ifdef THREAD_DEBUG
ib_fprintf (ib_stderr, "calling rwlock_init %x\n", wlock);
#endif

status = rwlock_init (wlock, USYNC_THREAD, NULL);

#ifdef THREAD_DEBUG
if (status)
    ib_fprintf (ib_stderr, "status = %d errno = %d\n", status, errno);
#endif

return status;
}

int THD_wlck_lock (
    WLCK_T	*wlock,
    USHORT	type)
{
/**************************************
 *
 *	T H D _ w l c k _ l o c k		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status;

#ifdef THREAD_DEBUG
if (type == WLCK_read)
    ib_fprintf (ib_stderr, "calling rwlock_rdlock %x\n", wlock);
else
    ib_fprintf (ib_stderr, "calling rwlock_wrlock %x\n", wlock);
#endif

if (type == WLCK_read)
    status = rw_rdlock (wlock);
else
    status = rw_wrlock (wlock);

#ifdef THREAD_DEBUG
if (status)
    ib_fprintf (ib_stderr, "status = %d errno = %d\n", status, errno);
#endif

return status;
}

int THD_wlck_unlock (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ u n l o c k	( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status;

#ifdef THREAD_DEBUG
ib_fprintf (ib_stderr, "calling rwlock_unlock %x\n", wlock);
#endif

status = rw_unlock (wlock);

#ifdef THREAD_DEBUG
if (status)
    ib_fprintf (ib_stderr, "status = %d errno = %d\n", status, errno);
#endif

return status;
}
#endif
#endif
#endif

#ifdef V4_THREADING
#ifndef THD_RWLOCK_STRUCT
#define RW_LOCK_DEFINED
int THD_wlck_destroy (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ d e s t r o y 	( s i m u l a t e )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return cond_destroy (&wlock->wlck_cond);
}

int THD_wlck_init (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ i n i t		( s i m u l a t e )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

wlock->wlck_count = 0;
wlock->wlck_waiters = 0;

return cond_init (&wlock->wlck_cond);
}

int THD_wlck_lock (
    WLCK_T	*wlock,
    USHORT	type)
{
/**************************************
 *
 *	T H D _ w l c k _ l o c k		( s i m u l a t e )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status, incr;

#ifdef THREAD_DEBUG
if (type == WLCK_read)
    ib_fprintf (ib_stderr, "calling rwlock_rdlock %x\n", wlock);
else
    ib_fprintf (ib_stderr, "calling rwlock_wrlock %x\n", wlock);
#endif

if (status = THD_mutex_lock (&wlock->wlck_cond.cond_mutex))
    return status;

if (type == WLCK_read)
    {
    if (wlock->wlck_count < 0)
	incr = ++wlock->wlck_waiters;
    else
	incr = 0;

    while (wlock->wlck_count < 0)
	cond_wait (&wlock->wlck_cond, NULL);

    wlock->wlck_count++;
    }
else
    {
    if (wlock->wlck_count > 0)
	incr = ++wlock->wlck_waiters;
    else
	incr = 0;

    while (wlock->wlck_count > 0)
	cond_wait (&wlock->wlck_cond, NULL);

    wlock->wlck_count = -1;
    }

if (incr)
    wlock->wlck_waiters--;

return THD_mutex_unlock (&wlock->wlck_cond.cond_mutex);
}

int THD_wlck_unlock (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ u n l o c k	( s i m u l a t e )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	status;

if (status = THD_mutex_lock (&wlock->wlck_cond.cond_mutex))
    return status;

if (wlock->wlck_count > 0)
    wlock->wlck_count--;
else
    wlock->wlck_count = 0;

if (wlock->wlck_waiters)
    cond_broadcast (&wlock->wlck_cond);

return THD_mutex_unlock (&wlock->wlck_cond.cond_mutex);
}
#endif
#endif

#ifndef RW_LOCK_DEFINED
int DLL_EXPORT THD_wlck_destroy (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ d e s t r o y		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int DLL_EXPORT THD_wlck_init (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ i n i t		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int DLL_EXPORT THD_wlck_lock (
    WLCK_T	*wlock,
    USHORT	type)
{
/**************************************
 *
 *	T H D _ w l c k _ l o c k		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}

int DLL_EXPORT THD_wlck_unlock (
    WLCK_T	*wlock)
{
/**************************************
 *
 *	T H D _ w l c k _ u n l o c k	( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return 0;
}
#endif

void THD_wlck_destroy_n (
    WLCK_T	*wlocks,
    USHORT	n)
{
/**************************************
 *
 *	T H D _ w l c k _ d e s t r o y _ n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

while (n--)
    THD_wlck_destroy (wlocks++);
}

void THD_wlck_init_n (
    WLCK_T	*wlocks,
    USHORT	n)
{
/**************************************
 *
 *	T H D _ w l c k _ i n i t _ n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

while (n--)
    THD_wlck_init (wlocks++);
}

void DLL_EXPORT THD_put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	T H D _ p u t _ s p e c i f i c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (!initialized)
    THD_init();

/* Save the current context */

new_context->thdd_prior_context = THD_get_specific();

put_specific (new_context);
}

void DLL_EXPORT THD_putspecific_data (
    void	*t_data)
{
/**************************************
 *
 *	T H D _ p u t s p e c i f i c _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Store the passed t_data using the ket t_key
 *
 **************************************/

if (!t_init)
    THD_init_data();
#ifdef ANY_THREADING

#ifdef POSIX_THREADS
pthread_setspecific (t_key, t_data);
#endif

#ifdef SOLARIS_MT
thr_setspecific (t_key, t_data);
#endif

#ifdef WIN_NT
TlsSetValue (t_key, (LPVOID) t_data);
#endif

#endif
}

THDD DLL_EXPORT THD_restore_specific (void)
{
/**************************************
 *
 *	T H D _ r e s t o r e _ s p e c i f i c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
THDD	current_context;

current_context = THD_get_specific();
put_specific (current_context->thdd_prior_context);

return current_context->thdd_prior_context;
}

#ifdef SUPERSERVER
int THD_rec_mutex_destroy (
    REC_MUTX_T	*rec_mutex)
{
/**************************************
 *
 *	T H D _ r e c _ m u t e x _ d e s t r o y   ( S U P E R S E R V E R )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return THD_mutex_destroy (rec_mutex->rec_mutx_mtx);
}

int THD_rec_mutex_init (
    REC_MUTX_T	*rec_mutex)
{
/**************************************
 *
 *	T H D _ r e c _ m u t e x _ i n i t   ( S U P E R S E R V E R )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

THD_mutex_init (rec_mutex->rec_mutx_mtx);
rec_mutex->rec_mutx_id = 0;
rec_mutex->rec_mutx_count = 0;
return 0;
}

int THD_rec_mutex_lock (
    REC_MUTX_T	*rec_mutex)
{
/**************************************
 *
 *	T H D _ r e c _ m u t e x _ l o c k   ( S U P E R S E R V E R )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	ret;

if (rec_mutex->rec_mutx_id == THD_get_thread_id())
    rec_mutex->rec_mutx_count++;
else
    {
    if (ret = THD_mutex_lock (rec_mutex->rec_mutx_mtx)) 
	return ret;
    rec_mutex->rec_mutx_id = THD_get_thread_id();
    rec_mutex->rec_mutx_count= 1;
    }
return 0;
}

int THD_rec_mutex_unlock (
    REC_MUTX_T	*rec_mutex)
{
/**************************************
 *
 *	T H D _ r e c _ m u t e x _ u n l o c k   ( S U P E R S E R V E R )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	ret;

if (rec_mutex->rec_mutx_id != THD_get_thread_id())
    return FAILURE;

rec_mutex->rec_mutx_count--;

if (rec_mutex->rec_mutx_count)
    return 0;
else
    {
    rec_mutex->rec_mutx_id = 0;
    return THD_mutex_unlock (rec_mutex->rec_mutx_mtx);
    }
}
#endif  /* SUPERSERVER */

#ifdef WIN_NT
#define THREAD_SUSPEND_DEFINED
int THD_resume (
    THD_T	thread)
{
/**************************************
 *
 *	T H D _ r e s u m e			( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Resume execution of a thread that has been
 *	suspended.
 *
 **************************************/

if (ResumeThread (thread) == 0xFFFFFFFF)
    return GetLastError();

return 0;
}

int THD_suspend (
    THD_T	thread)
{
/**************************************
 *
 *	T H D _ s u s p e n d			( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Suspend execution of a thread.
 *
 **************************************/

if (SuspendThread (thread) == 0xFFFFFFFF)
    return GetLastError();

return 0;
}
#endif

#ifndef THREAD_SUSPEND_DEFINED
int THD_resume (
    THD_T	thread)
{
/**************************************
 *
 *	T H D _ r e s u m e			( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Resume execution of a thread that has been
 *	suspended.
 *
 **************************************/

return 0;
}

int THD_suspend (
    THD_T	thread)
{
/**************************************
 *
 *	T H D _ s u s p e n d			( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Suspend execution of a thread.
 *
 **************************************/

return 0;
}
#endif

void THD_sleep (
    ULONG	milliseconds)
{
/**************************************
 *
 *	T H D _ s l e e p
 *
 **************************************
 *
 * Functional description
 *	Thread sleeps for requested number
 *	of milliseconds.
 *
 **************************************/
#ifdef ANY_THREADING

#ifdef WIN_NT
SleepEx (milliseconds, FALSE);
#else
EVENT_T	timer;
EVENT	timer_ptr = &timer;
SLONG	count;

ISC_event_init (&timer, 0, 0);
count = ISC_event_clear (&timer);
(void) ISC_event_wait (1, &timer_ptr, &count, milliseconds*1000, (FPTR_VOID) 0, NULL_PTR);
ISC_event_fini (&timer);
#endif

#else	/* !ANY_THREADING */
int	seconds;

/* Insure that process sleeps some amount of time. */

if (!(seconds = milliseconds / 1000))
    ++seconds;

/* Feedback unslept time due to premature wakeup from signals. */

while (seconds = sleep (seconds));

#endif	/* ANY_THREADING */
}

void THD_yield (void)
{
/**************************************
 *
 *	T H D _ y i e l d
 *
 **************************************
 *
 * Functional description
 *	Thread relinquishes the processor.
 *
 **************************************/
#ifdef ANY_THREADING

#ifdef POSIX_THREADS
/* use sched_yield() instead of pthread_yield(). Because pthread_yield() 
   is not part of the (final) POSIX 1003.1c standard. Several drafts of 
   the standard contained pthread_yield(), but then the POSIX guys 
   discovered it was redundant with sched_yield() and dropped it. 
   So, just use sched_yield() instead. POSIX systems on which  
   sched_yield() is available define _POSIX_PRIORITY_SCHEDULING 
   in <unistd.h>.
*/
#ifdef _POSIX_PRIORITY_SCHEDULING
sched_yield();
#else
pthread_yield();
#endif  /* _POSIX_PRIORITY_SCHEDULING */
#endif

#ifdef SOLARIS
thr_yield();
#endif

#ifdef WIN_NT
SleepEx (1, FALSE);
#endif

#ifdef NETWARE_386
ThreadSwitch();
#endif

#endif	/* ANY_THREADING */
}

#ifdef ANY_THREADING
#if (defined APOLLO || defined OS2_ONLY)
static TCTX allocate_context (
    SLONG	thread_id)
{
/**************************************
 *
 *	a l l o c a t e _ c o n t e x t 	( A P O L L O   &   O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Allocate a thread context for an existing
 *	thread and place it in the thread context list.
 *
 **************************************/
TCTX			context;

context = (TCTX) gds__alloc ((SLONG) sizeof (struct tctx));
/* FREE: unknown */
if (!context)		/* NOMEM: */
    {
    DEV_REPORT ("allocate_context: out of memory");
    /* NOMEM: no real error handling performed here */
    return NULL;
    }
context->tctx_next = NULL;
context->tctx_thread_id = thread_id;
context->tctx_thread_data = NULL;

THD_mutex_lock (&thread_create_mutex);
*thread_contexts_end = context;
thread_contexts_end = &context->tctx_next;
THD_mutex_unlock (&thread_create_mutex);

return context;
}
#endif
#endif

#ifdef V4_THREADING
#ifdef SOLARIS_MT
#define CONDITION_DEFINED
static int cond_broadcast (
    COND_T	*cond)
{
/**************************************
 *
 *	c o n d _ b r o a d c a s t	( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return cond_broadcast (&cond->cond_cond);
}

static int cond_destroy (
    COND_T	*cond)
{
/**************************************
 *
 *	c o n d _ d e s t r o y		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

THD_mutex_destroy (&cond->cond_mutex);

return cond_destroy (&cond->cond_cond);
}

static int cond_init (
    COND_T	*cond)
{
/**************************************
 *
 *	c o n d _ i n i t		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

THD_mutex_init (&cond->cond_mutex);

return cond_init (&cond->cond_cond, USYNC_THREAD, NULL);
}

static int cond_wait (
    COND_T	*cond,
    timestruc_t	*time)
{
/**************************************
 *
 *	c o n d _ w a i t		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (time)
    return cond_timedwait (&cond->cond_cond, &cond->cond_mutex, time);
else
    return cond_wait (&cond->cond_cond, &cond->cond_mutex);
}
#endif
#endif

static void init (void)
{
/**************************************
 *
 *	i n i t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (initialized)
    return;

initialized = TRUE;

#ifdef ANY_THREADING
THD_mutex_init (&ib_mutex);

#if (defined APOLLO || defined OS2_ONLY)
THD_mutex_init (&thread_create_mutex);
#endif /* APOLLO || OS2_ONLY */

#ifdef POSIX_THREADS
#ifdef HP10

pthread_keycreate (&specific_key, NULL);

#else

pthread_key_create (&specific_key, NULL);

#endif /* HP10 */
#endif /* POSIX_THREADS */

#ifdef SOLARIS_MT
if (thr_keycreate (&specific_key, NULL))
    {
    /* This call to thr_min_stack exists simply to force a link error
     * for a client application that neglects to include -lthread.
     * Solaris, for unknown reasons, includes stubs for all the
     * thread functions in libC.  Should the stubs be linked in
     * there is no compile error, no runtime error, and InterBase
     * will core drop.
     * Including this call gives an undefined symbol if -lthread is
     * omitted, looking at the man page will inform the client programmer
     * that -lthread is needed to resolve the symbol.
     * Note that we don't care if this thr_min_stack() is called or
     * not, we just need to have a reference to it to force a link error.
     */
    (void) thr_min_stack();
    ib_perror ("thr_keycreate");
    exit (1);
    }
#endif /* SOLARIS _MT */

#ifdef WIN_NT
specific_key = TlsAlloc();
#endif /* WIN_NT */
gds__register_cleanup (THD_cleanup, NULL_PTR);
#endif /* ANY_THREADING */
}

static void init_tkey (void)
{
/**************************************
 *
 *	i n i t
 *
 **************************************
 *
 * Functional description
 *	Function which actually creates the key which
 *	can be used by the threads to store t_data
 *
 **************************************/

if (t_init)
    return;

t_init = TRUE;

#ifdef ANY_THREADING

#ifdef POSIX_THREADS
#ifdef HP10

pthread_keycreate (&t_key, NULL);

#else

pthread_key_create (&t_key, NULL);

#endif /* HP10 */
#endif /* POSIX_THREADS */

#ifdef SOLARIS_MT
thr_keycreate (&t_key, NULL);
#endif

#ifdef WIN_NT
t_key = TlsAlloc();
#endif
#endif
}

#ifdef ANY_THREADING
#ifdef APOLLO
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( A p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
task_$handle_t	thread_id;
TCTX		thread;

thread_id = task_$get_handle();

for (thread = thread_contexts; ; thread = thread->tctx_next)
    if (thread->tctx_thread_id == thread_id)
	break;

thread->tctx_thread_data = new_context;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef NeXT
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( N e X T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

cthread_set_data (cthread_self(), new_context);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef OS2_ONLY
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
PTIB	pptib;
PPIB	pppib;
TCTX	thread;

DosGetInfoBlocks (&pptib, &pppib);

for (thread = thread_contexts; ; thread = thread->tctx_next)
    if (thread->tctx_thread_id == pptib->tib_ptib2->tib2_ultid)
	break;

thread->tctx_thread_data = new_context;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef POSIX_THREADS
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( P O S I X )
 *
 **************************************
 *
 * Functional description
 * Puts new thread specific data
 *
 **************************************/

pthread_setspecific (specific_key, new_context);
}
#endif /* ANY_THREADING */
#endif /* POSIX_THREADS */

#ifdef ANY_THREADING
#ifdef SOLARIS_MT
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (thr_setspecific (specific_key, new_context))
    {
    ib_perror ("thr_setspecific");
    exit (1);
    }
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef WIN_NT
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

TlsSetValue (specific_key, (LPVOID) new_context);
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef NETWARE_386
#define PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

SaveThreadDataAreaPtr (new_context);
}
#endif
#endif

#ifndef PUT_SPECIFIC_DEFINED
static void put_specific (
    THDD	new_context)
{
/**************************************
 *
 *	p u t _ s p e c i f i c		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

gdbb = new_context;
}
#endif

#ifdef ANY_THREADING
#ifdef APOLLO
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/
SLONG			priority, stack_size;
TCTX			context;
task_$option_set_t	options;
status_$t		status;
task_$handle_t		thread_id;

switch (priority_arg)
    {
    case THREAD_high:
	priority = 9;
	break;

    case THREAD_medium_high:
	priority = 7;
	break;

    case THREAD_medium:
	priority = 5;
	break;

    case THREAD_medium_low:
	priority = 3;
	break;

    case THREAD_low:
    default:
	priority = 1;
	break;
    }

stack_size = 64000;
options = (flags & THREAD_wait) ? task_$intend_to_wait : 0;

/* Create and initialize a structure that can hold thread specific data */

context = (TCTX) gds__alloc ((SLONG) sizeof (struct tctx));
/* FREE: unknown */
if (!context)
    return 1;		/* NOMEM: create of thread failed */
context->tctx_next = NULL;
context->tctx_thread_id = 0;
context->tctx_thread_data = NULL;

THD_mutex_lock (&thread_create_mutex);

/* put the context into the context queue before the thread
   is created to avoid the race condition of the thread trying
   to reference its own context before we insert it in the queue */

*thread_contexts_end = context;

thread_arg = arg;
context->tctx_thread_id = task_$create (thread_starter, routine, 0,
    stack_size, priority, task_$until_exec_or_level_exit, options, &status);

if (context->tctx_thread_id)
    {
    /* The thread was successfully created.  Finished linking context
       structure into list. */

    thread_contexts_end = &context->tctx_next;
    THD_mutex_unlock (&thread_create_mutex);
    return 0;
    }

/* Creation of the thread failed.  Back out the context structure. */

*thread_contexts_end = NULL;
THD_mutex_unlock (&thread_create_mutex);
gds__free (context);

return 1;
}
#endif
#endif

#ifdef NETWARE_386
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *      t h r e a d _ s t a r t         ( N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *      Start a new thread.  Return 0 if successful,
 *      status if not.
 *
 **************************************/
int ret;

flags = flags;
priority = priority;

ret = BeginThread (routine, NULL, STACK_SIZE, arg);

if (thd_id)
    *(int *) thd_id = ret;

if (ret)
    return (0);
else
    return (1);
}
#endif

#ifdef ANY_THREADING
#ifdef NeXT
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( N e X T )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/

cthread_fork ((cthread_fn_t) routine, (any_t) arg);
return 0;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef OS2_ONLY
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/
TID	thread_id;
ULONG	status;
SLONG	delta;

#ifdef __BORLANDC__
if (_beginthread ((void (_USERENTRY *) (void *)) routine, STACK_SIZE, arg) == -1)
    return errno;
#else
if (status = DosCreateThread (&thread_id, (PFNTHREAD) routine, (ULONG) arg,
	0, STACK_SIZE))
    return status;

switch (priority_arg)
    {
    case THREAD_high:
	delta = 25;
	break;
    case THREAD_medium_high:
	delta = 12;
	break;
    case THREAD_medium:
	delta = 0;
	break;
    case THREAD_medium_low:
	delta = -12;
	break;
    case THREAD_low:
    default:
	delta = -25;
	break;
    }

DosSetPriority (PRTYS_THREAD, PRTYC_NOCHANGE, delta, thread_id);
#endif

return 0;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef POSIX_THREADS
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( P O S I X )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/
pthread_t	thread;
pthread_attr_t	pattr;
int		state;

#ifdef DGUX
/* DGUX POSIX threads implementation does not define PTHREAD_CREATE_DETACHED and
   pthread_attr_setdetachstate requires pointer to int as the second argument,
   which should contain new state.
*/
int 		detach_state=PTHREAD_CREATE_DETACHED
#endif

#if ( !defined HP10 && !defined linux )

state = pthread_attr_init (&pattr);
if (state)
    return state;

/* Do not make thread bound for superserver/client */

#if (!defined (SUPERCLIENT) && !defined (SUPERSERVER))
pthread_attr_setscope (&pattr, PTHREAD_SCOPE_SYSTEM);
#endif

#ifdef DGUX
pthread_attr_setdetachstate (&pattr,&detach_state);
#else
pthread_attr_setdetachstate (&pattr, PTHREAD_CREATE_DETACHED);
#endif

state = pthread_create (&thread, &pattr, routine, arg);
pthread_attr_destroy (&pattr);
return state;

#else
#ifdef linux
if (state = pthread_create (&thread, NULL, routine, arg))
    return state;
return pthread_detach (thread);
#else
long            stack_size;
 
state = pthread_attr_create (&pattr);
if (state)
    {
    assert (state == -1);
    return errno;
    }
 
/* The default HP's stack size is too small. HP's documentation
   says it is "machine specific". My test showed it was less
   than 64K. We definitly need more stack to be able to execute
   concurrently many (at least 100) copies of the same request
   (like, for example in case of recursive stored prcedure).
   The following code sets threads stack size up to 256K if the
   default stack size is less than this number
*/
stack_size = pthread_attr_getstacksize (pattr);
if (stack_size == -1)
   return errno;
 
if (stack_size < 0x40000L)
    {
    state = pthread_attr_setstacksize (&pattr, 0x40000L);
    if (state)
        {
        assert (state == -1);
        return errno;
        }
    }

/* HP's Posix threads implementation does not support
   bound attribute. It just a user level library.
*/
state = pthread_create (&thread, pattr, routine, arg);
if (state)
    {
    assert (state == -1);
    return errno;
    }
state = pthread_detach (&thread);
if (state)
    {
    assert (state == -1);
    return errno;
    }
state = pthread_attr_delete (&pattr);
if (state)
    {
    assert (state == -1);
    return errno;
    }
return 0;
 
#endif /* linux */
#endif /* HP10 */
}
#endif /* POSIX_THREADS */
#endif /* ANY_THREADING */

#ifdef ANY_THREADING
#ifdef SOLARIS_MT
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( S o l a r i s )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/
int             rval;
thread_t	thread_id;
sigset_t        new_mask, orig_mask;

(void) sigfillset(&new_mask);
(void) sigdelset(&new_mask, SIGALRM);
if (rval = thr_sigsetmask(SIG_SETMASK, &new_mask, &orig_mask))
    return rval;
#if (defined SUPERCLIENT || defined SUPERSERVER)
rval = thr_create (NULL, 0, routine, arg, THR_DETACHED, &thread_id);
#else
rval = thr_create (NULL, 0, routine, arg, (THR_BOUND | THR_DETACHED), &thread_id);
#endif
(void) thr_sigsetmask(SIG_SETMASK, &orig_mask, NULL);

return rval;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef WIN_NT
#define START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/
HANDLE	handle;
DWORD	create, thread_id;
int	priority;

create = (flags & THREAD_wait) ? CREATE_SUSPENDED : 0;

#ifdef __BORLANDC__
if ((handle = (HANDLE) _beginthreadNT ((void (_USERENTRY *) (void *)) routine, 0, arg,
	NULL, create, &thread_id)) == (HANDLE) -1)
    return errno;
#else
/* I have changed the CreateThread here to _beginthreadex() as using
 * CreateThread() can lead to memory leaks caused by C-runtime library.
 * Advanced Windows by Richter pg. # 109. */
if (!(handle = _beginthreadex (NULL, 0,
	(LPTHREAD_START_ROUTINE) routine, (LPVOID) arg, create, &thread_id)))
    return GetLastError();
#endif

switch (priority_arg)
    {
    case THREAD_critical:
	priority = THREAD_PRIORITY_TIME_CRITICAL;
	break;
    case THREAD_high:
	priority = THREAD_PRIORITY_HIGHEST;
	break;
    case THREAD_medium_high:
	priority = THREAD_PRIORITY_ABOVE_NORMAL;
	break;
    case THREAD_medium:
	priority = THREAD_PRIORITY_NORMAL;
	break;
    case THREAD_medium_low:
	priority = THREAD_PRIORITY_BELOW_NORMAL;
	break;
    case THREAD_low:
    default:
	priority = THREAD_PRIORITY_LOWEST;
	break;
    }

SetThreadPriority (handle, priority);
if ( thd_id)
    *(HANDLE*)thd_id = handle;
else
    CloseHandle (handle);

return 0;
}
#endif
#endif

#ifdef ANY_THREADING
#ifdef VMS
#ifndef POSIX_THREADS
#define START_THREAD
/**************************************
 *
 *	t h r e a d _ s t a r t		( V M S )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.  This routine is coded in macro.
 *
 **************************************/
#endif
#endif
#endif

#ifndef START_THREAD
static int thread_start (
    int		(*routine)(void *),
    void	*arg,
    int		priority_arg,
    int		flags,
    void	*thd_id)
{
/**************************************
 *
 *	t h r e a d _ s t a r t		( G e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Start a new thread.  Return 0 if successful,
 *	status if not.
 *
 **************************************/

return 1;
}
#endif

#ifdef APOLLO
static thread_starter (
    int		(*routine)(void))
{
/**************************************
 *
 *	t h r e a d _ s t a r t e r
 *
 **************************************
 *
 * Functional description
 *	Set up a cleanup handler and start a thread.
 *
 **************************************/
pfm_$cleanup_rec	rec;
status_$t		status;
USHORT			abexit;
SSHORT			stream;

abexit = TRUE;
status = pfm_$cleanup (&rec);

if (status.all != pfm_$cleanup_set)
    {
    if (abexit)
	{
    	gds__log ("(THD)thread_starter: unexpected thread exit");
	stream = errlog_$open (
		    "/interbase/interbase.log",
		    strlen ("/interbase/interbase.log"),
		    0,
		    5,
		    NULL_PTR,
		    NULL,
		    status.all,
		    errlog_$silent | errlog_$no_header | errlog_$no_divert,
		    &status
		    );

	errlog_$traceback (stream, errlog_$fault_tb, 0);
	errlog_$fault_status (stream);
	errlog_$close (stream);
	}
    SCH_abort();
    return;
    }

(*routine) (thread_arg);
abexit = FALSE;
}
#endif
