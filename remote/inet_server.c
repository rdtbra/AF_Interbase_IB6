/*
 *	PROGRAM:	JRD Remote Server
 *	MODULE:		inet_server.c
 *	DESCRIPTION:	Internet remote server.
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
#include <stdlib.h>
#include "../jrd/common.h"
#if !(defined VMS || defined sgi || defined PC_PLATFORM || defined OS2_ONLY)
#include <sys/param.h>
#endif

#ifdef WINDOWS_ROUTER
#define MAX_ARGS	6
#endif	/* WINDOWS_ROUTER */

#ifdef VMS
#include <descrip.h>
#endif

#ifdef SUPERSERVER
#include <unistd.h>
#include <errno.h>
#include "../jrd/gds.h"
#include "../jrd/pwd.h"
#endif

#include "../remote/remote.h"
#include "../jrd/license.h"
#include "../jrd/thd.h"
#include "../jrd/file_params.h"
#include "../remote/inet_proto.h"
#include "../remote/serve_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/sch_proto.h"

#ifdef NETWARE_386
#include <signal.h>
#endif
 
#ifdef APOLLO
#include <apollo/base.h>
#include <apollo/proc2.h>
#include <sys/signal.h>
#endif

#ifdef UNIX
#include <sys/signal.h>
#endif

#ifndef hpux
#define sigvector	sigvec
#endif

#ifndef NOFILE
#ifdef VMS
#define NOFILE		32
#else
#define NOFILE		20
#endif
#endif

#ifndef NBBY
#define	NBBY		8
#endif

#ifndef SV_INTERRUPT
#define SV_INTERRUPT	0
#endif

#ifndef NFDBITS
#define NFDBITS		(sizeof(SLONG) * NBBY)

#if !(defined NETWARE_386 || defined OS2_ONLY || defined DECOSF)
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((SCHAR *)(p), sizeof(*(p)))
#endif
#endif

#ifdef SUPERSERVER
#ifndef WIN_NT
#define TEMP_DIR "/tmp"
#define CHANGE_DIR chdir
#endif
#define INTERBASE_USER          "interbase"
#define INTERBASE_USER_SHORT    "interbas"
#endif

static int	assign (SCHAR *);
#ifndef DECOSF
static int	bzero (SCHAR *, int);
#endif
static void	name_process (UCHAR *);
static void	signal_handler (void);
#ifdef SUPERSERVER
static void	signal_sigpipe_handler (void);
#endif
static int	set_signal (int, FPTR_VOID);

#ifdef WINDOWS_ROUTER
static int	atov (UCHAR *, UCHAR**, SSHORT);
#endif	/* WINDOWS_ROUTER */

#ifdef SUPERSERVER
extern SLONG    free_map_debug;
extern SLONG    trace_pools;
static struct ipccfg   trace_pooltbl [] = {
   ISCCFG_TRACE_POOLS, 0, &trace_pools,   0, 0,
   NULL,                0, NULL,            0, 0
};
#endif

static TEXT	protocol [128];
static int	INET_SERVER_start = 0;
static USHORT	INET_SERVER_flag = 0;

#ifdef WINDOWS_ROUTER
int PASCAL WinMain (
    HINSTANCE	hInstance,
    HINSTANCE	hPrevInstance,
    LPSTR	lpszCmdLine,
    int		nCmdShow)
#else	/* WINDOWS_ROUTER */
int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
#endif	/* WINDOWS_ROUTER */
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
int	n, clients;
PORT	port;
int	child, debug, channel, standalone, multi_threaded, multi_client;
TEXT	*p, **end, c;
int	done = FALSE;
#if !(defined VMS || defined PC_PLATFORM || defined OS2_ONLY)
fd_set	mask;
#endif

#ifdef WINDOWS_ROUTER
/*
 *	Construct an argc, argv so we can use the old parse code.
 */
int	argc;
char	**argv, *argv2[MAX_ARGS];

argv = argv2;
argv[0] = "IB_server";
argc = 1 + atov (lpszCmdLine, argv+1, MAX_ARGS-1);

#endif	/* WINDOWS_ROUTER */


#ifdef VMS
argc = VMS_parse (&argv, argc);
#endif

end = argc + argv;
argv++;
debug = standalone = INET_SERVER_flag = FALSE;
channel = 0;
protocol [0] = 0;
multi_client = multi_threaded = FALSE;

#ifdef APOLLO
standalone = TRUE;
#endif

#ifdef PC_PLATFORM
INET_SERVER_flag |= SRVR_multi_client;
standalone = TRUE;
#endif

#ifdef SUPERSERVER
INET_SERVER_flag |= SRVR_multi_client;
multi_client = multi_threaded = standalone = TRUE;
#endif

clients = 0;

while (argv < end)
    {
    p = *argv++;
    if (*p++ == '-')
	while (c = *p++)
            {
	    switch (UPPER (c))
		{
		case 'D':
		    INET_SERVER_flag |= SRVR_debug;
		    debug = standalone = TRUE;
#ifdef SUPERSERVER
		    free_map_debug = 1;
#endif
		    break;
#ifndef SUPERSERVER

		case 'M':
		    INET_SERVER_flag |= SRVR_multi_client;
		    if (argv < end)
			if (clients = atoi (*argv))
			    argv++;
#ifdef APOLLO
		    multi_threaded = TRUE;
#endif
		    multi_client = standalone = TRUE;
		    break;            

		case 'S':
		    standalone = TRUE;
		    break;

		case 'I':
		    standalone = FALSE;
		    break;

#ifndef	PC_PLATFORM
		case 'T':
		    multi_threaded = TRUE;
		    break;
#endif

		case 'U':
		    multi_threaded = FALSE;
		    break;
#endif /* SUPERSERVER */

		case 'H':
                    if (ISC_get_prefix (p) == -1) 
		        ib_printf ("Invalid argument Ignored\n");
                    else
	                argv++;  /* donot skip next argument if this one 
                                    is invalid */
                    done = TRUE;
		    break;

		case 'P':
		    sprintf (protocol, "/%s", *argv++);
		    break;

		case 'Z':
		    ib_printf ("InterBase TCP/IP server version %s\n", GDS_VERSION);
		    exit (FINI_OK);			
		}
         if (done) break;
         }
    }
#if (defined SUPERSERVER && defined UNIX )
    set_signal (SIGPIPE, signal_sigpipe_handler);
    set_signal (SIGUSR1, signal_handler);
    set_signal (SIGUSR2, signal_handler);
#endif

/* Fork off a server, wait for it to die, then fork off another,
   but give up after 100 tries */

#if !(defined SUPERSERVER || defined VMS || defined PC_PLATFORM || defined OS2_ONLY)
if (multi_client && !debug)
    {
#ifdef APOLLO
    name_process ("gds_inet_guardian");
#endif
    set_signal (SIGUSR1, signal_handler);
    for (n = 0; n < 100; n++)
	{
	INET_SERVER_start = 0;
	if (!(child = fork()))
	    break;
	while (wait (0) != child)
	    if (INET_SERVER_start)
		{
		n = 0;		/* reset error counter on "real" signal */
		break;
		}
	gds__log ("INET_SERVER/main: gds_inet_server restarted");
	}
    set_signal (SIGUSR1, SIG_DFL);
    }
#endif

#ifdef APOLLO
name_process ("gds_inet_server");
#endif

if (standalone)
    {
    if (multi_client)
	{
#ifdef SUPERSERVER
        TEXT    user_name[256]; /* holds the user name */
        /* check user id */
        ISC_get_user (user_name, NULL, NULL, NULL, NULL, NULL, NULL);

        if (strcmp (user_name, INTERBASE_USER) && strcmp (user_name, "root")
            && strcmp (user_name, INTERBASE_USER_SHORT))
            {
            /* invalid user -- bail out */
            ib_fprintf (ib_stderr, "ibserver: Invalid user (must be %s, %s, or root)\n",
                        INTERBASE_USER, INTERBASE_USER_SHORT);
            exit (STARTUP_ERROR);
            }
#else
	if (setreuid (0, 0) < 0)
	    ib_printf ("Inet_server: couldn't set uid to superuser.\n");
#endif
	INET_set_clients (clients);
	}

    if (!debug)
	{
	FD_ZERO (&mask);
	FD_SET (2, &mask);
	divorce_terminal (&mask);
	}
    {
    STATUS	status_vector [ISC_STATUS_LENGTH];
    THREAD_ENTER;
    port = INET_connect (protocol, NULL_PTR, status_vector, INET_SERVER_flag,
			 NULL_PTR, 0);
    THREAD_EXIT;
    if (!port)
	{
	gds__print_status (status_vector);
	exit (STARTUP_ERROR);
	}
    }
    }
else
    {
#ifdef VMS
    channel = assign ("SYS$INPUT");
#endif
    THREAD_ENTER;
    port = INET_server (channel);
    THREAD_EXIT;
    if (!port)
	{
	ib_fprintf (ib_stderr, "ibserver: Unable to start INET_server\n");
	exit (STARTUP_ERROR);
	}
    }

#ifdef SUPERSERVER

/* before starting the superserver stuff change directory to tmp */
if (CHANGE_DIR (TEMP_DIR))
    {
    char err_buf [1024];

    /* error on changing the directory */
    sprintf (err_buf, "Could not change directory to %s due to errno %d",
	     TEMP_DIR, errno);
    gds__log (err_buf);
    }

/* Server tries to attash to isc4.gdb to make sure everything is OK
   This code fixes bug# 8429 + all other bug of that kind - from 
   now on the server exits if it cannot attach to the database
   (wrong or no license, not enough memory, etc.
*/
{
TEXT	path [MAXPATHLEN];
STATUS	status [20];
isc_db_handle   db_handle = 0L;

gds__prefix (path, USER_INFO_NAME);
isc_attach_database (status, strlen(path), path, &db_handle, 0, NULL);
if (status[0] == 1 && status[1] > 0)
    {
    gds__log_status (USER_INFO_NAME, status);
    isc_print_status (status);
    exit (STARTUP_ERROR);
    }
isc_detach_database (status, &db_handle);
if (status[0] == 1 && status[1] > 0)
    {
    gds__log_status (USER_INFO_NAME, status);
    isc_print_status (status);
    exit (STARTUP_ERROR);
    }
}

ISC_get_config (LOCK_HEADER, trace_pooltbl);

#endif


if (multi_threaded)
    SRVR_multi_thread (port, INET_SERVER_flag);
else
#ifdef WINDOWS_ROUTER
    SRVR_WinMain (port, INET_SERVER_flag, hInstance, hPrevInstance, nCmdShow);
#else
    SRVR_main (port, INET_SERVER_flag);
#endif

#ifdef DEBUG_GDS_ALLOC
/* In Debug mode - this will report all server-side memory leaks
 * due to remote access
 */
gds_alloc_report (0, __FILE__, __LINE__);
#endif

exit (FINI_OK);
}

#ifdef WINDOWS_ROUTER
static int atov(
    UCHAR	*str,
    UCHAR	**vec,
    SSHORT	len)
{
/**************************************
 *
 *	a t o v
 *
 **************************************
 *
 * Functional description
 *	Take a string and convert it to a vector.
 *	White space delineates, but things in quotes are
 *	kept together.
 *
 **************************************/
int	i  = 0, qt = 0, qq;
char	*p1, *p2;

vec[0] = str;
for (p1 = p2 = str; i < len; i++)
    {
    while ( *p1 == ' ' || *p1 == '\t' )
	p1++;
    while ( qt || (*p1 != ' ' && *p1 != '\t'))
	{
	qq = qt;
	if (*p1 == '\'')
	    if (!qt)
		qt = -1;
	    else
		if (qt == -1)
		    qt = 0;
	if (*p1 == '"')
	    if (!qt)
		qt = 1;
	    else
		if (qt == 1)
		    qt = 0;
	if (*p1 == '\0' || *p1 == '\n')
	    {
	    *p2++ = '\0';
	    vec[++i] = 0;
	    return i;
	    }
	if (qq == qt)
	    *p2++ = *p1;
	p1++;
	}
    p1++;
    *p2++ = '\0';
    vec[i + 1] = p2;
    }
*p2 = '\0';
vec[i] = 0;
return i - 1;
}
#endif	/* WINDOWS_ROUTER */

#ifdef VMS
static int assign (
    SCHAR	*string)
{
/**************************************
 *
 *	a s s i g n
 *
 **************************************
 *
 * Functional description
 *	Assign a channel for communications.
 *
 **************************************/
SSHORT		channel;
int		status;
struct dsc$descriptor_s	desc;

desc.dsc$b_dtype = DSC$K_DTYPE_T;
desc.dsc$b_class = DSC$K_CLASS_S;
desc.dsc$w_length = strlen (string);
desc.dsc$a_pointer = string;

status = sys$assign (&desc, &channel, NULL, NULL);

return (status & 1) ? channel : 0;
}
#endif

#if !(defined OS2_ONLY || defined DECOSF)
static int bzero (
    SCHAR	*address,
    int		length)
{
/**************************************
 *
 *	b z e r o
 *
 **************************************
 *
 * Functional description
 *	Zero a block of memory.
 *
 **************************************/

if (length)
    do *address++ = 0; while (--length);
}
#endif


#if !(defined VMS || defined NETWARE_386 || defined PC_PLATFORM || defined OS2_ONLY)
static int set_signal (
    int		signal_number,
    void	(*handler)(void))
{
/**************************************
 *
 *	s e t _ s i g n a l
 *
 **************************************
 *
 * Functional description
 *	Establish signal handler.
 *
 **************************************/
#ifdef SYSV_SIGNALS
sigset (signal_number, handler);
#else

#ifndef SIGACTION_SUPPORTED
struct sigvec	vec;
struct sigvec	old_vec;

vec.sv_handler = handler;
vec.sv_mask = 0;
vec.sv_flags = SV_INTERRUPT;
sigvector (signal_number, &vec, &old_vec);
#else
struct sigaction	vec, old_vec;

vec.sa_handler = handler;
memset (&vec.sa_mask, 0, sizeof (vec.sa_mask));
vec.sa_flags = 0;
sigaction (signal_number, &vec, &old_vec);
#endif

#endif
}
#endif

static void signal_handler (void)
{
/**************************************
 *
 *	s i g n a l _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Dummy signal handler.
 *
 **************************************/

++INET_SERVER_start;
}
#if (defined SUPERSERVER && defined UNIX )
static void signal_sigpipe_handler (void)
{
/****************************************************
 *
 *	s i g n a l _ s i g p i p e _ h a n d l e r
 *
 ****************************************************
 *
 * Functional description
 *	Dummy signal handler.
 *
 **************************************/

++INET_SERVER_start;
gds__log ("Super Server/main: Bad client socket, send() resulted in SIGPIPE, caught by server\n                   client exited improperly or crashed ????");
}
#endif
