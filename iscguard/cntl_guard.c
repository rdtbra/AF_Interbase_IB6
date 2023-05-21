/*
 *	PROGRAM:	Guardian CNTL function.
 *	MODULE:		cntl_guard.c
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
#include "../iscguard/cntlg_proto.h"
#include "../remote/remote.h"
#include "../jrd/thd.h"
#include "../jrd/isc_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/thd_proto.h"

#ifdef WIN_NT
#include <windows.h>
#include "../remote/window.rh"
#endif

typedef struct thread {
    struct thread	*thread_next;
    HANDLE		thread_handle;
} *THREAD;

static void	control_thread (DWORD);
static void	parse_switch (TEXT *, int *);
static USHORT	report_status (DWORD, DWORD, DWORD, DWORD);

static DWORD			current_state;
static void			(*main_handler)();
static SERVICE_STATUS_HANDLE	service_handle;
static TEXT			*service_name;
static HANDLE			stop_event_handle;
static MUTX_T			thread_mutex [1];
static THREAD			threads;

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


#ifdef REMOVE
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
#endif


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

service_handle = RegisterServiceCtrlHandler (service_name, 
                                           (LPHANDLER_FUNCTION) control_thread);
if (!service_handle)
    return;

/* THD_mutex_init (thread_mutex); */

#if (defined SUPERCLIENT || defined SUPERSERVER)
flag = SRVR_multi_client;
#else
flag = 0;
#endif

/* Get the default client mode from the registry. */

#ifdef ISC_CALLS
if (ISC_get_registry_var ("DefaultClientMode",
	default_mode, sizeof (default_mode), NULL_PTR) != -1)
#endif
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

/* start everything, and wait here for ever, or at
 * least until we get the stop event indicating that
 * the service is stoping. */

if (report_status (SERVICE_START_PENDING, NO_ERROR, 1, 3000) &&
    (stop_event_handle = CreateEvent (NULL, TRUE, FALSE, NULL)) != NULL &&
    report_status (SERVICE_START_PENDING, NO_ERROR, 2, 3000) &&
    !gds__thread_start ((FPTR_INT) main_handler, (void*) flag, 0, 0, NULL_PTR) &&
    report_status (SERVICE_RUNNING, NO_ERROR, 0, 0))
    WaitForSingleObject (stop_event_handle, INFINITE);

last_error = GetLastError();

if (stop_event_handle)
    CloseHandle (stop_event_handle);

/* ONce we are stopped, we will tell the server to
 * do the same.  We could not do this in the control_thread
 * since the Services Control Manager is single threaded,
 * and thus can only process one request at the time. */
{
    SERVICE_STATUS status_info;
    SC_HANDLE hScManager = 0, hService = 0;
    hScManager = OpenSCManager (NULL, NULL, GENERIC_READ | GENERIC_EXECUTE);
    hService = OpenService (hScManager, "InterBaseServer", SERVICE_STOP);
    ControlService (hService, SERVICE_CONTROL_STOP, &status_info);
    CloseServiceHandle (hScManager);
    CloseServiceHandle (hService);
}

report_status (SERVICE_STOPPED, last_error, 0, 0);

/* THD_mutex_destroy (thread_mutex); */
}

#ifdef REMOVE
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
#endif


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

void CNTL_stop_service (
    TEXT	*service)
{
/**************************************
 *
 *	C N T L _ s t o p _ s e r v i c e
 *
 **************************************
 *
 * Functional description
 *   This function is called to stop the service.
 *
 *
 **************************************/

SERVICE_STATUS status_info;
SC_HANDLE servicemgr_handle;
SC_HANDLE service_handle;

servicemgr_handle = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT);
if (servicemgr_handle == NULL)
    {
    // return error
    int error = GetLastError();
    gds__log ("SC manager error %d", error);
    return;
    }

service_handle = OpenService (servicemgr_handle, service_name, SERVICE_STOP);

if (service_handle == NULL)
    {
    // return error
    int error = GetLastError();
    gds__log ("open services error %d",  error);
    return;
    } 
else
    {
    if (!ControlService (service_handle, SERVICE_CONTROL_STOP, &status_info))
        {
        // return error
        int error = GetLastError();
        gds__log ("Control services error %d", error);
        return;
        }
    }
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
	SetEvent (stop_event_handle);
	return;

    case SERVICE_CONTROL_INTERROGATE:
	break;

    default:
	break;
    }

report_status (state, NO_ERROR, 0, 0);
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
