/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		cntl.c
 *	DESCRIPTION:	Windows NT service control panel interface
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
#include "../jrd/common.h"
#include "../remote/remote.h"
#include "../jrd/thd.h"
#include "../utilities/install_nt.h"
#include "../remote/cntl_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/jrd_proto.h"

#ifdef WIN_NT
#include <windows.h>
#endif

#define  ERROR_BUFFER_LENGTH 1024

typedef struct thread {
    struct thread	*thread_next;
    HANDLE		thread_handle;
} *THREAD;

static void	control_thread (DWORD);
static void	cleanup_thread ();
static void	parse_switch (TEXT *, int *);
static USHORT	report_status (DWORD, DWORD, DWORD, DWORD);

static DWORD			current_state;
static void			(*main_handler)();
static SERVICE_STATUS_HANDLE	service_handle;
static TEXT			*service_name;
static HANDLE			stop_event_handle;
static MUTX_T			thread_mutex [1];
static THREAD			threads;
static HANDLE			hMutex = NULL;
static BOOLEAN			bGuarded = FALSE;

void CNTL_init (
    void	(*handler)(),
    TEXT	*name)
{
/**************************************
 *
 *	C N T L _ i n i t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

main_handler = handler;
service_name = name;
}

void *CNTL_insert_thread (void)
{
/**************************************
 *
 *	C N T L _ i n s e r t _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
THREAD	new_thread;

THREAD_ENTER;
new_thread = (THREAD) ALLR_alloc ((SLONG) sizeof (struct thread));
/* NOMEM: ALLR_alloc() handled */
/* FREE:  in CTRL_remove_thread() */

THREAD_EXIT;
DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
    &new_thread->thread_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);

THD_mutex_lock (thread_mutex);
new_thread->thread_next = threads;
threads = new_thread;
THD_mutex_unlock (thread_mutex);

return new_thread;
}

void CNTL_main_thread (
    SLONG	argc,
    SCHAR	*argv[])
{
/**************************************
 *
 *	C N T L _ m a i n _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int	flag;
TEXT	*p, default_mode [100];
DWORD	last_error;

HANDLE cleanup_thread_handle;
DWORD  count, return_from_wait, cleanup_thread_id;

service_handle = RegisterServiceCtrlHandler (service_name, 
                                           (LPHANDLER_FUNCTION) control_thread);
if (!service_handle)
    return;

THD_mutex_init (thread_mutex);

#if (defined SUPERCLIENT || defined SUPERSERVER)
flag = SRVR_multi_client;
#else
flag = 0;
#endif

/* Get the default client mode from the registry. */

if (ISC_get_registry_var ("DefaultClientMode",
	default_mode, sizeof (default_mode), NULL_PTR) != -1)
    if (default_mode [0] == '-')
	parse_switch (&default_mode [1], &flag);

/* Parse the command line looking for any additional arguments. */

argv++;

while (--argc)
    {
    p = *argv++;
    if (*p++ = '-')
	parse_switch (p, &flag);
    }

if (report_status (SERVICE_START_PENDING, NO_ERROR, 1, 3000) &&
    (stop_event_handle = CreateEvent (NULL, TRUE, FALSE, NULL)) != NULL &&
    report_status (SERVICE_START_PENDING, NO_ERROR, 2, 3000) &&
    !gds__thread_start ((FPTR_INT) main_handler, (void*) flag, 0, 0, NULL_PTR) &&
    report_status (SERVICE_RUNNING, NO_ERROR, 0, 0))
    WaitForSingleObject (stop_event_handle, INFINITE);

last_error = GetLastError();

if (stop_event_handle)
    CloseHandle (stop_event_handle);
/* set the status with the timer, start the cleanup thread and wait for the
 * cleanup thread to exit or the timer to expire, once we reach the max number
 * of loops or the thread exits set the state to shutdown and exit */
cleanup_thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) cleanup_thread,
	                    NULL, 0, &cleanup_thread_id);
count = 1;

do
{
    count++;
    report_status (SERVICE_STOP_PENDING, NO_ERROR, count, 60000); 
    return_from_wait = WaitForSingleObject (cleanup_thread_handle, 50000); 
} while (count < 10 && return_from_wait == WAIT_TIMEOUT);
/* loop for 10 times about 7 minutes should be enough time for the server to
 * cleanup */

report_status (SERVICE_STOPPED, last_error, 0, 0);
}

void CNTL_remove_thread (
    void	*thread)
{
/**************************************
 *
 *	C N T L _ r e m o v e _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
THREAD	*thread_ptr;
THREAD	this_thread;

THD_mutex_lock (thread_mutex);
for (thread_ptr = &threads; *thread_ptr; thread_ptr = &(*thread_ptr)->thread_next)
    if (*thread_ptr == (THREAD) thread)
	{
	*thread_ptr = ((THREAD) thread)->thread_next;
	break;
	}
THD_mutex_unlock (thread_mutex);

this_thread = (THREAD)thread;
CloseHandle( this_thread->thread_handle);

THREAD_ENTER;
ALLR_free (thread);
THREAD_EXIT;
}

void CNTL_shutdown_service (
    TEXT	*message)
{
/**************************************
 *
 *	C N T L _ s h u t d o w n _ s e r v i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
UCHAR	buffer [256], *strings [2];
HANDLE	event_source;

sprintf (buffer, "%s error: %d", service_name, GetLastError());

event_source = RegisterEventSource (NULL, service_name);
if (event_source)
    {
    strings [0] = buffer;
    strings [1] = message;
    ReportEvent (event_source, EVENTLOG_ERROR_TYPE, 0, 0, NULL,
	2, 0, strings, NULL);
    DeregisterEventSource (event_source);
    }

if (stop_event_handle)
    SetEvent (stop_event_handle);
}

static void control_thread (
    DWORD	action)
{
/**************************************
 *
 *	c o n t r o l _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *	Process a service control request.
 *
 **************************************/
DWORD	state;

state = SERVICE_RUNNING;

switch (action)
    {
    case SERVICE_CONTROL_STOP:
	report_status (SERVICE_STOP_PENDING, NO_ERROR, 1, 3000);
	if (bGuarded == TRUE)
	  ReleaseMutex (hMutex);
	SetEvent (stop_event_handle);
	return;

    case SERVICE_CONTROL_INTERROGATE:
	break;

    case SERVICE_CREATE_GUARDIAN_MUTEX:
	hMutex = OpenMutex (SYNCHRONIZE, FALSE, GUARDIAN_MUTEX);
	if (hMutex)
	    {
	    UINT error_mode = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | 
			      SEM_NOOPENFILEERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT;
	    SetErrorMode(error_mode);
	    bGuarded = TRUE;
	    WaitForSingleObject (hMutex, INFINITE);
	    }
	break;
    default:
	break;
    }

report_status (state, NO_ERROR, 0, 0);
}

static void cleanup_thread ()
{
/**************************************
 *
 *	c l e a n u p _ t h r e a d
 *
 **************************************
 *
 * Functional description
 *	This thread is responsible for the cleanup.
 *
 **************************************/
USHORT attach_count, database_count;
TEXT    return_buffer[ERROR_BUFFER_LENGTH], *buff_ptr = return_buffer;

/* find out if we have any attachments if we do then log a message to the log
 * file */
JRD_num_attachments(return_buffer, ERROR_BUFFER_LENGTH, JRD_info_dbnames, &attach_count, 
	&database_count);

/* if we have active attachments then log messages */
if (attach_count > 0)
    {
    USHORT i, j;
    TEXT    out_message[ERROR_BUFFER_LENGTH + 100];
    TEXT print_buffer[ERROR_BUFFER_LENGTH], *print_ptr;
    USHORT num_databases, database_name_length;

    sprintf (print_buffer, 
	    "Shutting down the InterBase service with %d active connection(s) to %d database(s)", 
	    attach_count, database_count);
    gds__log(print_buffer);

    /* just get the ushort value and increment it by a ushort length */
    num_databases = (USHORT) *((USHORT *)buff_ptr);
    buff_ptr += sizeof (USHORT);
    for (i = 0; i < num_databases; i++)
        {
	database_name_length = (USHORT) *((USHORT *)buff_ptr);
        buff_ptr += sizeof (USHORT);
        print_ptr = print_buffer;
	for (j = 0; j < database_name_length; j++)
    	    *print_ptr++ = *buff_ptr++;
	*print_ptr = '\0';
	sprintf (out_message, "The database %s was being accessed when the server was shutdown", 
		print_buffer);
	gds__log (out_message);
	}
    }


THREAD_ENTER;
JRD_shutdown_all();

/* There is no THREAD_EXIT to help ensure that no
 * threads will get in and try to access the data-
 * structures we released in JRD_shutdown_all()
 */

THD_mutex_destroy (thread_mutex);
}

static void parse_switch (
    TEXT	*switches,
    int		*flag)
{
/**************************************
 *
 *	p a r s e _ s w i t c h
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT	c;

while (c = *switches++)
    switch (UPPER (c))
	{
	case 'B':
	    *flag |= SRVR_high_priority;
	    break;

	case 'I':
	    *flag |= SRVR_inet;
	    break;

	case 'N':
	    *flag |= SRVR_pipe;
	    break;

	case 'R':
	    *flag &= ~SRVR_high_priority;
	    break;
	}

#if (defined SUPERCLIENT || defined SUPERSERVER)
*flag |= SRVR_multi_client;
#else
*flag &= ~SRVR_multi_client;
#endif
}

static USHORT report_status (
    DWORD	state,
    DWORD	exit_code,
    DWORD	checkpoint,
    DWORD	hint)
{
/**************************************
 *
 *	r e p o r t _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Report our status to the control manager.
 *
 **************************************/
SERVICE_STATUS	status;
USHORT		ret;

status.dwServiceType = (SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS);
status.dwServiceSpecificExitCode = 0;

if (state == SERVICE_START_PENDING)
    status.dwControlsAccepted = 0;
else
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

status.dwCurrentState = current_state = state;
status.dwWin32ExitCode = exit_code;
status.dwCheckPoint = checkpoint;
status.dwWaitHint = hint;

if (!(ret = SetServiceStatus (service_handle, &status)))
    CNTL_shutdown_service ("SetServiceStatus");

return ret;
}
