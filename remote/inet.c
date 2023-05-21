/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		inet.c
 *	DESCRIPTION:	TCP/UCP/IP Communications module.
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
#include <string.h>
#include <stdlib.h>
#include "../jrd/common.h"
#include "../jrd/file_params.h"
#include <stdarg.h>

#include "../jrd/time.h"

#ifdef SUPERSERVER
#ifdef	WIN_NT
#define FD_SETSIZE 256
#endif
#endif /* SUPERSERVER */

#if !(defined VMS || defined WIN_NT || defined WINDOWS_ONLY)
#include <netdb.h>
#if !(defined PC_PLATFORM || defined OS2_ONLY || defined NETWARE_386)
#include <sys/param.h>
#endif
#endif

#ifdef NETWARE_386
NETDB_DEFINE_CONTEXT

#include "../remote/faux.h"

#define GETHOSTBYNAME FAUX_gethostbyname
#define GETSERVBYNAME FAUX_getservbyname
#define GETHOSTNAME FAUX_gethostname

/* These need to be defined for NetWare.  They
 * See the comment in INET_CONNECT for details
 */
#define INET_RETRY_ERRNO	0
#define INET_RETRY_CALL		0
#else

#define GETHOSTBYNAME gethostbyname
#define GETSERVBYNAME getservbyname
#define GETHOSTNAME gethostname
#define INET_RETRY_CALL		5
#endif

#include "../remote/remote.h"
#include "../jrd/gds.h"
#include "../jrd/iberr.h"
#include "../jrd/thd.h"
#include "../remote/inet_proto.h"
#include "../remote/proto_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#ifndef REQUESTER
#include "../jrd/isc_i_proto.h"
#include "../jrd/sch_proto.h"
#endif  /* REQUESTER */

#ifdef WINDOWS_ONLY
#include "../jrd/loiter.h"
#endif

#ifdef IMP
int	h_errno; /* In Other platforms one of the system header files declares this */
#endif 

#ifdef APOLLO
#define MAX_PTYPE	ptype_batch_send
#include <apollo/base.h>
#endif

#ifdef DOS_ONLY
#define NO_ITIMER
#define SOCLOSE		closesocket
#define SYS_ERR		gds_arg_dos
#include <direct.h>
#endif
 
#if (defined hpux || defined SCO_UNIX || defined mips)
extern int	h_errno;
#endif

#if (defined EPSON || defined UNIXWARE || defined NCR3000 || defined M88K)
#include <sys/sockio.h>
#endif

#ifdef NCR3000
#include <sys/filio.h>
#endif

#ifdef NETWARE_386
#define NO_ITIMER
#define NO_FORK
#include <direct.h>
#endif

#ifdef OS2_ONLY
#define INCL_DOSPROCESS
#include <os2.h>
#include <sys/time.h>
#include <utils.h>
#define NO_ITIMER
#define NO_FORK
#define MAX_PTYPE	ptype_batch_send
#define ERRNO		sock_errno()
#define H_ERRNO		ERRNO
#define SOCLOSE		soclose
#endif

#ifdef VMS
#include "../remote/tcptypes.h"
#include <ib_perror.h>
#include <socket.h>
#define NO_ITIMER
#define NO_FORK
#define MAX_PTYPE	ptype_batch_send
#define PROXY_FILE	"[sysmgr]gds_proxy.dat"
#define ERRNO		ISC_tcp_geterrno()
#endif

#ifdef WINDOWS_ONLY
#define NOMSG
#define MAX_PTYPE	ptype_batch_send
#include <winsock.h>
/* 
** Winsock has a typedef for socket, so #define SOCKET to the typedef here
** so that it won't be redefined below.
*/
#define SOCKET		SOCKET
#define ERRNO		WSAGetLastError()
#define H_ERRNO		WSAGetLastError()
#endif

#ifdef WIN_NT
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include "../utilities/install_nt.h"
#define NO_ITIMER
#define ERRNO		WSAGetLastError()
#define H_ERRNO		WSAGetLastError()
#define SOCLOSE		closesocket
#define SYS_ERR		gds_arg_win32
#define INET_RETRY_ERRNO	WSAEINPROGRESS
#endif

#ifdef PC_PLATFORM
#define NO_FORK
#endif

#ifdef SYSV_SIGNALS
#ifndef IMP
#define NO_ITIMER
#endif
#endif

#ifndef SYS_ERR
#define SYS_ERR		gds_arg_unix
#endif

#ifndef SV_INTERRUPT
#define SV_INTERRUPT	0
#endif

#ifndef EINTR
#define EINTR		0
#endif

#ifdef XENIX
/* 5.5 SCO Port: SIGURG is now available in SCO Openserver */
#ifndef SCO_EV
#define SIGURG		SIGUSR1
#endif
#endif

#ifndef hpux
#define sigvector	sigvec
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  -1
#endif

#ifndef SOCKET
#define SOCKET  int
#endif

#ifndef SOCLOSE
#define SOCLOSE	close
#endif

#ifndef ERRNO
#define ERRNO	errno
#endif

#ifndef INET_RETRY_ERRNO
#define INET_RETRY_ERRNO TRY_AGAIN
#endif

#ifndef INET_RETRY_CALL
#define INET_RETRY_CALL 5
#endif

#ifndef H_ERRNO
#define H_ERRNO	h_errno
#endif

#ifndef SIGURG
#define SIGURG	SIGINT
#endif

#ifndef ENOBUFS
#define ENOBUFS	0
#endif

SLONG  INET_remote_buffer;
SLONG  INET_max_data;
static BOOLEAN first_time = TRUE;

static struct ipccfg   INET_tcp_buffer [] = {
   ISCCFG_REMOTE_BUFFER, 0, &INET_remote_buffer, 0, 0,
   NULL,                0, NULL,            0, 0
};

/*
#define DEBUG	1
*/

#ifndef REQUESTER
#ifdef DEBUG
#include <sys/timeb.h>
#define TRACE_packets		(1 << 0)	/* bit 0 */
#define TRACE_operations	(1 << 1)	/* bit 1 */
#define TRACE_summary		(1 << 2)	/* bit 2 */

static	int	INET_trace 	= TRACE_summary | TRACE_packets | TRACE_operations; 
static  time_t	INET_start_time = 0;
	SLONG	INET_force_error= -1;	/* simulate a network error */
static ULONG	INET_count_send = 0;
static ULONG	INET_count_recv = 0;
static ULONG	INET_bytes_send = 0;
static ULONG	INET_bytes_recv = 0;

static ULONG inet_debug_timer(void)
{
/**************************************
 *
 *	i n e t _ d e b u g _ t i m e r
 *
 **************************************
 *
 * Functional description
 *	Utility function used in DEBUG mode only to put a timestamp
 *	since start of connection on each debug output message.
 *
 *	This has been implemented and tested on SOLARIS, and may
 *	need tweeking on any other platform where DEBUG is needed.
 *
 **************************************/
struct timeb	now;
(void) ftime (&now);
return (now.time*1000 + now.millitm - INET_start_time);
}
#endif
#endif



#ifndef PROXY_FILE
#define PROXY_FILE	"/etc/gds_proxy"
#endif

#ifndef HOSTS_FILE
#define HOSTS_FILE	"/etc/hosts.equiv"
#endif

#ifndef GDS_HOSTS_FILE
#ifdef SMALL_FILE_NAMES
#define GDS_HOSTS_FILE	"/etc/gdshosts.eqv"
#else
#define GDS_HOSTS_FILE	"/etc/gds_hosts.equiv"
#endif
#endif

#define PROTOCOL_NAME   "gds_db"


#define MAX_DATA_LW	1448  /* Low  Water mark */
#define MAX_DATA_HW	32768 /* High Water mark */
#define DEF_MAX_DATA	8192

#define MAX_SEQUENCE	256

SLONG	INET_max_data;
SLONG 	INET_remote_buffer;

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#ifndef MAXHOSTLEN
#define MAXHOSTLEN	64
#endif

#ifdef VMS
#define ERRNOCODE(symbol, code, text)	text,

static CONST TEXT * CONST win_errlist [] = {
    NULL,
#include "../remote/winerr.h"
};
#undef ERRNOCODE
#endif

#define	SELECT_TIMEOUT	60	/* Dispatch thread select timeout (sec) */

typedef struct slct {
    int		slct_width;
    int		slct_count;
    SLONG	slct_time;
    fd_set	slct_fdset;
} SLCT;

static int	accept_connection 	(PORT, 
						P_CNCT *);
static void	alarm_handler 	(int);
static PORT	alloc_port 	(PORT);
static PORT	aux_connect 	(PORT, 
					PACKET *, 
					int (*)(void));
static PORT	aux_request 	(PORT, 
					PACKET *);
#ifndef VMS
static int	check_host 	(PORT, 
					TEXT *, 
					TEXT *, 
					struct passwd *);
#else
static int	check_host 	(PORT, 
					TEXT *, 
					TEXT *);
#endif
static BOOLEAN	check_proxy 	(PORT, 
					TEXT *, 
					TEXT *);
static void	cleanup_port    (PORT);
static void	disconnect 	(PORT);
static void	exit_handler 	(void *);
#ifdef NO_FORK
static int	fork 		(void);
#endif
#ifdef WIN_NT
static int	fork 		(SOCKET, 
					USHORT);
#endif
static void	inet_copy 	(SCHAR *, 
					SCHAR *, 
					int);
static int	inet_destroy 	(XDR *);
static void 	inet_gen_error 	(PORT, 
					STATUS, 
					...);
static bool_t	inet_getbytes 	(XDR *, 
					SCHAR *, 
					u_int);
static bool_t	inet_getlong 	(XDR *, 
					SLONG *);
static u_int	inet_getpostn 	(XDR *);
static void	inet_handler 	(PORT);
static caddr_t  inet_inline 	(XDR *, 
					u_int);
static int	inet_error 	(PORT, 
					CONST TEXT *,
					STATUS,
					int);
static bool_t	inet_putlong 	(XDR *, 
					SLONG *);
static bool_t	inet_putbytes 	(XDR *, 
					SCHAR *, 
					u_int);
static bool_t	inet_read 	(XDR *);
static bool_t	inet_setpostn 	(XDR *, 
					u_int);
static PORT 	inet_try_connect	(PACKET *, 
						RDB, 
						USHORT, 
						TEXT *, 
						TEXT *, 
						STATUS *,
                                                SCHAR*,
                                                SSHORT);
static bool_t	inet_write 	(XDR *, 
					int);
static void	inet_zero 	(SCHAR *, 
					int);
static int	initWSA 	(PORT);
static int	parse_hosts 	(TEXT *, 
					TEXT *, 
					TEXT *);
static int	parse_line 	(TEXT *, 
					TEXT *, 
					TEXT *, 
					TEXT *);
#ifdef DEBUG 
static void 	packet_print 	(TEXT *, 
					UCHAR *, 
					int,
					int);
#endif
static bool_t	packet_receive 	(PORT, 
					UCHAR *, 
					SSHORT, 
					SSHORT *);
static bool_t	packet_send 	(PORT, 
					SCHAR *, 
					SSHORT);
static PORT	receive 	(PORT, 
					PACKET *);
static PORT	select_accept 	(PORT);

#ifdef NETWARE_386
static PORT	select_port 	(PORT);
static int	select_wait 	(PORT);
#else
static PORT	select_port 	(PORT, SLCT  *);
static int	select_wait 	(PORT, SLCT  *);
#endif

static int	send_full 	(PORT, PACKET *);
static int	send_partial 	(PORT, PACKET *);
#ifdef  SUPERSERVER
static void     unhook_disconnected_ports (PORT);
#endif
static void     unhook_port     (PORT, PORT);
static int	xdrinet_create 	(XDR *,
					PORT, 
					UCHAR *, 
					USHORT, 
					enum xdr_op);
static bool_t	xdrinet_endofrecord 	(XDR *, int);

#ifdef WINDOWS_ONLY
static int FAR PASCAL	blocking_stub   (void);
static void 	NetworkLibraryCleanup 	(void);
LRESULT FAR PASCAL inet_wndproc (HWND, 
					UINT, 
					WPARAM, 
					LPARAM);
static void	cleanup_library (void *);
#endif

static CONST struct xdr_ops	inet_ops =
	{
	inet_getlong,
	inet_putlong,
	inet_getbytes,
	inet_putbytes,
	inet_getpostn,
	inet_setpostn,
	inet_inline,
	inet_destroy};

#ifndef NOFILE
#ifdef VMS
#define NOFILE		32
#else
#define NOFILE		20
#endif
#endif 

#ifndef MAX_PTYPE
#define MAX_PTYPE	ptype_out_of_band
#endif

#define MAXCLIENTS	NOFILE - 10
 
/* Select uses bit masks of file descriptors in longs. */

#ifndef NBBY
#define	NBBY	8
#endif

#ifndef NFDBITS
#if (!defined WIN_NT && !defined WINDOWS_ONLY)
#define NFDBITS		(sizeof(SLONG) * NBBY)

#ifndef NETWARE_386
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	inet_zero((SCHAR *)(p), sizeof(*(p)))
#endif
#endif
#endif


#ifdef NETWARE_386
#define MAX_FD_SETS 10
static fd_set fd_sets[MAX_FD_SETS];
static int select_count = 0;
#endif

static SLCT	INET_select = {0, 0, 0};
static int	INET_max_clients;
#ifdef WIN_NT
static int	INET_initialized = FALSE;
static WSADATA	INET_wsadata;
static TEXT	INET_command_line [MAXPATHLEN + 32], *INET_p;
#endif

#ifdef WINDOWS_ONLY
#define MAX_TASKS 64

typedef struct TaskInfo {
    HTASK	hId;
    int	nUseCount;
} TaskInfoType;

static TaskInfoType     Tasks[MAX_TASKS];
static HINSTANCE  	hWinsockDLL;
#endif


#ifdef SHLIB_DEFS
#define select		(*_libgds_select)
#define gethostname	(*_libgds_gethostname)
#define connect		(*_libgds_connect)
#define execl		(*_libgds_execl)
#define getservbyname	(*_libgds_getservbyname)
#define gethostbyname	(*_libgds_gethostbyname)
#define setsockopt	(*_libgds_setsockopt)
#define bind		(*_libgds_bind)
#define listen		(*_libgds_listen)
#define accept		(*_libgds_accept)
#define setreuid	(*_libgds_setreuid)
#define setregid	(*_libgds_setregid)
#define chdir		(*_libgds_chdir)
#define getsockname	(*_libgds_getsockname)
#define getpeername	(*_libgds_getpeername)
#define gethostbyaddr	(*_libgds_gethostbyaddr)
#define xdrmem_create	(*_libgds_xdrmem_create)
#define innetgr		(*_libgds_innetgr)
#define getppid		(*_libgds_getppid)
#define getgroups	(*_libgds_getgroups)
#define bzero		(*_libgds_bzero)
#define shutdown	(*_libgds_shutdown)
#define setitimer	(*_libgds_setitimer)
#define pause		(*_libgds_pause)
#define getenv		(*_libgds_getenv)
#ifndef SCO
#define initgroups      (*_libgds_initgroups)
#define sigvector       (*_libgds_sigvec)
#else
#define htonl		(*_libgds_htonl)
#define ntohl		(*_libgds_ntohl)
#endif  /* SCO */
#define h_errno		(*_libgds_h_errno)
#define errno		(*_libgds_errno)

extern int		select();
extern void		gethostname();
extern int		connect();
extern int		execl();
extern struct servent	*getservbyname();
extern struct hostent	*gethostbyname();
extern int		setsockopt();
extern int		bind();
extern int		listen();
extern int		accept();
extern int		setreuid();
extern int		setregid();
extern int		chdir();
extern int		getsockname();
extern int		getpeername();
extern struct hostent	*gethostbyaddr();
extern int		xdrmem_create();
extern int		innetgr();
extern int		getgroups();
extern int		bzero();
extern int		shutdown();
extern int		setitimer();
extern int		pause();
extern char		*getenv();
#ifndef SCO
extern int		initgroups();
extern int		sigvector();
extern int		getppid();
#else
extern ulong		htonl();
extern ulong		ntohl();
extern pid_t		getppid();
#endif /* SCO */
extern int		h_errno;
extern int		errno;
#endif /* SHLIB_DEFS */

#ifdef WINDOWS_ONLY
/* 
** The windows network libraries have to be dynamically loaded because the
** libraries may not be installed if the user does not have TCP (e.g. if
** they are using SPX).  This section defines the winsock functions to
** reference function pointers which will be set using the windows
** GetProcAddress function at startup.
*/

#define accept		acceptProcPtr
#define bind		bindProcPtr
#define closesocket	closesocketProcPtr
#define connect		connectProcPtr
#define gethostbyname	gethostbynameProcPtr
#define gethostname	gethostnameProcPtr
#define getsockname	getsocknameProcPtr
#define getservbyname	getservbynameProcPtr
#define htonl		htonlProcPtr
#define listen		listenProcPtr
#define ntohl		ntohlProcPtr
#define recv		recvProcPtr
#define select		selectProcPtr
#define send		sendProcPtr
#define setsockopt	setsockoptProcPtr
#define shutdown	shutdownProcPtr
#define	socket		socketProcPtr
#define WSAAsyncSelect	WSAAsyncSelectProcPtr
#define WSACleanup	WSACleanupProcPtr
#define WSAGetLastError	WSAGetLastErrorProcPtr
#define WSASetBlockingHook	WSASetBlockingHookProcPtr
#define WSAStartup	WSAStartupProcPtr
#ifdef FD_ISSET
#undef FD_ISSET
#endif
#define FD_ISSET	WSAFDIsSetProcPtr

/*
** Declare function pointers, using function prototypes for type checking 
*/
SOCKET	(PASCAL FAR *acceptProcPtr) (SOCKET, struct sockaddr FAR *, int FAR *);
int	(PASCAL FAR *bindProcPtr) (SOCKET, const struct sockaddr FAR *, int);
int	(PASCAL FAR *closesocketProcPtr) (SOCKET);
int	(PASCAL FAR *connectProcPtr) (SOCKET, const struct sockaddr FAR *, int);
struct hostent FAR * (PASCAL FAR *gethostbynameProcPtr) (const char FAR *);
int	(PASCAL FAR *gethostnameProcPtr) (char FAR *, int);
int	(PASCAL FAR *getsocknameProcPtr) 
		     	(SOCKET, struct sockaddr FAR *, int FAR *);
u_long	(PASCAL FAR *htonlProcPtr) (u_long);
struct servent FAR * (PASCAL FAR *getservbynameProcPtr) 
			(const char FAR *, const char FAR *);
int	(PASCAL FAR *listenProcPtr) (SOCKET, int);
u_long	(PASCAL FAR *ntohlProcPtr) (u_long);
int	(PASCAL FAR *recvProcPtr) (SOCKET, char FAR *, int, int);
int	(PASCAL FAR *selectProcPtr) (int, fd_set FAR *, fd_set FAR *,
			fd_set FAR *, const struct timeval FAR *);
int	(PASCAL FAR *sendProcPtr) (SOCKET, const char FAR *, int, int);
int	(PASCAL FAR *setsockoptProcPtr)
			(SOCKET, int, int, const char FAR *, int);
int	(PASCAL FAR *shutdownProcPtr) (SOCKET, int);
int	(PASCAL FAR *socketProcPtr) (int, int, int);
int	(PASCAL FAR *WSAAsyncSelectProcPtr) (SOCKET, HWND, u_int, long);
int	(PASCAL FAR *WSACleanupProcPtr) (void);
int	(PASCAL FAR *WSAFDIsSetProcPtr) (SOCKET, fd_set FAR *);
int	(PASCAL FAR *WSAGetLastErrorProcPtr) (void);
FARPROC	(PASCAL FAR *WSASetBlockingHookProcPtr) (FARPROC);
int	(PASCAL FAR *WSAStartupProcPtr) (WORD, LPWSADATA);

static int	INET_initialized = FALSE;

#endif /* WINDOWS_ONLY */

#ifdef VMS
#define accept		ISC_tcp_accept
#define bind		ISC_tcp_bind
#define connect		ISC_tcp_connect
#define getaddr 	ISC_tcp_getaddr
#define gethostbyname	ISC_tcp_gethostbyname
#define gethostname	ISC_tcp_gethostname
#define gethosts	ISC_tcp_gethosts
#define getnamebyaddr	ISC_tcp_getnamebyaddr
#define getpeername	ISC_tcp_getpeername
#define getservbyname	ISC_tcp_getservbyname
#define getservport	ISC_tcp_getservport
#define getsockname	ISC_tcp_getsockname
#define listen		ISC_tcp_listen
#define recv		ISC_tcp_recv
#define select		ISC_tcp_select
#define send		ISC_tcp_send
#define setsockopt	ISC_tcp_setsockopt

#define socket		ISC_tcp_socket
extern			ISC_wait();
extern			gds__completion_ast();

extern			ISC_tcp_accept();
extern			ISC_tcp_bind();
extern			ISC_tcp_connect();
extern			ISC_tcp_getnamebyaddr();
extern			ISC_tcp_getpeername();
extern			ISC_tcp_getservbyname();
extern			ISC_tcp_getservport();
extern			ISC_tcp_getsockname();
extern			ISC_tcp_listen();
extern			ISC_tcp_recv();
extern			ISC_tcp_select();
extern			ISC_tcp_send();
extern			ISC_tcp_setsockopt();
extern			ISC_tcp_socket();
#endif  /* VMS */

#ifdef	WIN_NT
#define	INTERRUPT_ERROR(x)	(SYSCALL_INTERRUPTED(x) || (x) == WSAEINTR)
#else
#define	INTERRUPT_ERROR(x)	(SYSCALL_INTERRUPTED(x))
#endif

#ifdef  START_PORT_CRITICAL
#undef  START_PORT_CRITICAL
#undef  STOP_PORT_CRITICAL
#undef  DEFER_PORT_CLEANUP
#endif

#ifdef  SUPERSERVER
static  MUTX_T  port_mutex;
static  BOOLEAN    port_mutex_inited = 0;

#define DEFER_PORT_CLEANUP

#define START_PORT_CRITICAL     if (!port_mutex_inited)                \
                                    {                                  \
                                    port_mutex_inited = 1;             \
                                    THD_mutex_init (&port_mutex);      \
                                    }                                  \
                                THREAD_EXIT;                           \
                                THD_mutex_lock (&port_mutex);          \
                                THREAD_ENTER

#define STOP_PORT_CRITICAL      THREAD_EXIT;                           \
                                THD_mutex_unlock (&port_mutex);        \
                                THREAD_ENTER

#endif

#ifndef START_PORT_CRITICAL
#define START_PORT_CRITICAL
#define STOP_PORT_CRITICAL
#endif




PORT INET_analyze (
    TEXT	*file_name, 
    USHORT	*file_length, 
    STATUS	*status_vector,
    TEXT	*node_name,
    TEXT	*user_string,
    USHORT	uv_flag,
    SCHAR       *dpb, 
    SSHORT      dpb_length)
{
/**************************************
 *
 *	I N E T _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	File_name is on node_name.
 *	Establish an external connection to node_name.
 *
 *	If a connection is established, return a port block, otherwise
 *	return NULL.
 *
 **************************************/
RDB		rdb;
PORT		port;
PACKET		*packet;
P_CNCT		*cnct;
int		eff_gid, eff_uid;
SSHORT		user_length;
UCHAR		*p, user_id [200];
TEXT		buffer [64];
struct p_cnct_repeat	*protocol;

*file_length = strlen (file_name);

/* We need to establish a connection to a remote server.  Allocate the necessary
   blocks and get ready to go. */

rdb = (RDB) ALLOC (type_rdb);
packet = &rdb->rdb_packet;

/* Pick up some user identification information */

user_id [0] = CNCT_user;
p = user_id + 2;
ISC_get_user (p, &eff_uid, &eff_gid, NULL_PTR, NULL_PTR, NULL_PTR, user_string);
user_id [1] = (UCHAR) strlen ((SCHAR*) p);
p = p + user_id[1];

*p++ = CNCT_host;
p++;
ISC_get_host (p, 64);
p [-1] = (UCHAR) strlen ((SCHAR*) p);

for (; *p; p++)
    if (*p >= 'A' && *p <= 'Z')
	*p = *p - 'A' + 'a';

if ((eff_uid == -1) || uv_flag)
    {
    *p++ = CNCT_user_verification;
    *p++ = 0;
    }
#if !(defined VMS || defined NETWARE_386 || defined OS2_ONLY || defined PC_PLATFORM)
else
    {
    /* Communicate group id info to server, as user maybe running under group
       id other than default specified in /etc/passwd. */

    *p++ = CNCT_group;
    *p++ = sizeof (SLONG);
    eff_gid = htonl (eff_gid);
    memcpy (p, AOF32L (eff_gid), sizeof (SLONG));
    p += sizeof (SLONG);
    }
#endif

user_length = (SSHORT) (p - user_id);

/* Establish connection to server */

/* Note: prior to V3.1E a recievers could not in truth handle more
   then 5 protocol descriptions, so we try them in chunks of 5 or less */

/* If we want user verification, we can't speak anything less than version 7 */

cnct = &packet->p_cnct;
#ifdef SCROLLABLE_CURSORS
cnct->p_cnct_count = 6;
#else
cnct->p_cnct_count = 4;
#endif
cnct->p_cnct_user_id.cstr_length = user_length;
cnct->p_cnct_user_id.cstr_address = user_id;

protocol = cnct->p_cnct_versions;

protocol->p_cnct_version = PROTOCOL_VERSION8;
protocol->p_cnct_architecture = arch_generic;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 2;

++protocol;

protocol->p_cnct_version = PROTOCOL_VERSION8;
protocol->p_cnct_architecture = ARCHITECTURE;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 3;

++protocol;

protocol->p_cnct_version = PROTOCOL_VERSION10;
protocol->p_cnct_architecture = arch_generic;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 4;

++protocol;

protocol->p_cnct_version = PROTOCOL_VERSION10;
protocol->p_cnct_architecture = ARCHITECTURE;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 5;

#ifdef SCROLLABLE_CURSORS
++protocol;

protocol->p_cnct_version = PROTOCOL_SCROLLABLE_CURSORS;
protocol->p_cnct_architecture = arch_generic;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 6;

++protocol;

protocol->p_cnct_version = PROTOCOL_SCROLLABLE_CURSORS;
protocol->p_cnct_architecture = ARCHITECTURE;
protocol->p_cnct_min_type = ptype_rpc;
protocol->p_cnct_max_type = MAX_PTYPE;
protocol->p_cnct_weight = 7;
#endif

/* Try connection using first set of protocols.  punt if error */

port = inet_try_connect (packet, rdb, *file_length, file_name, 
			node_name, status_vector,
                        dpb, dpb_length);
if (!port)
    return NULL;

if (packet->p_operation == op_reject && !uv_flag)
    {
    disconnect (port);

    /* try again with next set of known protocols */

    cnct->p_cnct_count = 4;
    cnct->p_cnct_user_id.cstr_length = user_length;
    cnct->p_cnct_user_id.cstr_address = user_id;

    protocol = cnct->p_cnct_versions;

    protocol->p_cnct_version = PROTOCOL_VERSION6;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 2;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION6;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 3;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION7;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = MAX_PTYPE;
    protocol->p_cnct_weight = 4;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION7;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = MAX_PTYPE;
    protocol->p_cnct_weight = 5;

    port = inet_try_connect (packet, rdb, *file_length, file_name, 
					    node_name, status_vector,
                                            dpb, dpb_length);
    if (!port)
	return NULL;
    }

if (packet->p_operation == op_reject && !uv_flag)
    {
    disconnect (port);

    /* try again with next set of known protocols */

    cnct->p_cnct_count = 4;
    cnct->p_cnct_user_id.cstr_length = user_length;
    cnct->p_cnct_user_id.cstr_address = user_id;

    protocol = cnct->p_cnct_versions;

    protocol->p_cnct_version = PROTOCOL_VERSION3;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 2;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION3;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 3;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION4;
    protocol->p_cnct_architecture = arch_generic;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 4;

    ++protocol;

    protocol->p_cnct_version = PROTOCOL_VERSION4;
    protocol->p_cnct_architecture = ARCHITECTURE;
    protocol->p_cnct_min_type = ptype_rpc;
    protocol->p_cnct_max_type = ptype_batch_send;
    protocol->p_cnct_weight = 5;

    port = inet_try_connect (packet, rdb, *file_length, file_name, 
					    node_name, status_vector,
                                            dpb, dpb_length);
    if (!port)
	return NULL;
    }

if (packet->p_operation != op_accept)
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__connect_reject;
    *status_vector++ = 0;
    disconnect (port);
    return NULL;
    }

port->port_protocol = packet->p_acpt.p_acpt_version;

/* once we've decided on a protocol, concatenate the version 
   string to reflect it...  */

sprintf (buffer, "%s/P%d", port->port_version->str_data, port->port_protocol);
ALLR_free (port->port_version);
port->port_version = REMOTE_make_string (buffer);

if (packet->p_acpt.p_acpt_architecture == ARCHITECTURE)
    port->port_flags |= PORT_symmetric;

if (packet->p_acpt.p_acpt_type == ptype_rpc)
    port->port_flags |= PORT_rpc;

if (packet->p_acpt.p_acpt_type != ptype_out_of_band)
    port->port_flags |= PORT_no_oob;

return port;
}

PORT DLL_EXPORT INET_connect (
    TEXT	*name,
    PACKET	*packet,
    STATUS	*status_vector,
    USHORT	flag,
    SCHAR       *dpb, 
    SSHORT      dpb_length)
{
/**************************************
 *
 *	I N E T _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Establish half of a communication link.  If a connect packet is given,
 *	the connection is on behalf of a remote interface.  Otherwise the connect
 *	is for a server process.
 *
 **************************************/
int			l, n;
SOCKET 			s;
PORT			port;
TEXT			*protocol, temp [128], *p;
struct sockaddr_in	address;
#ifndef VMS
struct hostent		*host;
struct servent		*service;
TEXT			msg [64];
#endif
#ifdef DGUX
SCHAR			optval;
#else
int			optval;
#endif
 
#ifdef DEBUG
{
UCHAR	*p;
if (INET_trace & TRACE_operations)
    {
    ib_fprintf (ib_stdout,"INET_connect\n");
    ib_fflush (ib_stdout);
    };
INET_start_time = inet_debug_timer();
if ((p = getenv ("INET_force_error")) != NULL) 
    INET_force_error = atoi (p);
}
#endif

port = alloc_port (NULL_PTR);
port->port_status_vector = status_vector;
REMOTE_get_timeout_params (port, dpb, dpb_length);
status_vector [0] = gds_arg_gds;
status_vector [1] = 0;
status_vector [2] = gds_arg_end;
protocol = PROTOCOL_NAME;
#ifdef VMS
ISC_tcp_setup (ISC_wait, gds__completion_ast);
#endif

if (name)
    {
    strcpy (temp, name);
    for (p = temp; *p;)
	if (*p++ == '/')
	    {
	    p [-1] = 0;
	    name = temp;
	    protocol = p;
	    break;
	    }
    }

if (name && *name)
    {
    if (port->port_connection)
	ALLR_free (port->port_connection);
    port->port_connection = REMOTE_make_string (name);
    }
else
    name = port->port_host->str_data;

/* Set up Inter-Net socket address */

inet_zero ((SCHAR*) &address, sizeof (address));

/* V M S */

#ifdef VMS
if (getservport (protocol, "tcp", &address.sin_port) == -1)
    {
    inet_error (port, "getservbyname", isc_net_connect_err, 0);
    return NULL;
    }
if (packet)
    {
    if (getaddr (name, &address) == -1)
	{
	inet_error (port, "gethostbyname", isc_net_connect_err, 0);
	return NULL;
	}
    }
else
    address.sin_addr.s_addr = INADDR_ANY;
#else

#ifdef WINDOWS_ONLY
if (initWSA(port))
    return NULL;
#endif  /* WINDOWS_ONLY */

/* U N I X style sockets */
THREAD_EXIT;
host = GETHOSTBYNAME (name);

/* On Windows NT/9x, gethostbyname can only accomodate
 * 1 call at a time.  In this case it returns the error
 * WSAEINPROGRESS. On UNIX systems, this call may not succeed
 * because of a temporary error.  In this case, it returns
 * h_error set to TRY_AGAIN.  When these errors occur,
 * retry the operation a few times.
 * NOTE: This still does not guarantee success, but helps.
 */
if (!host)
    {
    int retry;
    if (H_ERRNO == INET_RETRY_ERRNO)
        {
        for (retry = 0; retry < INET_RETRY_CALL; retry++)
            {
	    host = GETHOSTBYNAME (name);
	    if (host) break;
            }
	}
    }
THREAD_ENTER;
if (!host)
    {
    sprintf (msg, 
	      "INET/INET_connect: gethostbyname failed, error code = %d", 
	      H_ERRNO);
    gds__log (msg, NULL_PTR);
    inet_gen_error (port,
            isc_network_error,
		    isc_arg_string, 
		    port->port_connection->str_data, 
		    isc_arg_gds, 
		    isc_net_lookup_err,
            isc_arg_gds,
		    isc_host_unknown, 
		    0);
#ifdef WINDOWS_ONLY
    NetworkLibraryCleanup ();
#endif /* WINDOWS_ONLY */	
    return NULL;
    }

/* Copy info from host struct before making another socket call for
   Winsock compatibility */

address.sin_family = host->h_addrtype;
if (packet)
    inet_copy (host->h_addr, &address.sin_addr, sizeof (address.sin_addr));
else
    address.sin_addr.s_addr = INADDR_ANY;

THREAD_EXIT;
service = GETSERVBYNAME (protocol, "tcp");
#ifdef WIN_NT
/* On Windows NT/9x, getservbyname can only accomodate
 * 1 call at a time.  In this case it returns the error
 * WSAEINPROGRESS. 
 * If this happens, retry the operation a few times.
 * NOTE: This still does not guarantee success, but helps.
 */
 if (!service)
    {
    int retry;
    if (H_ERRNO == INET_RETRY_ERRNO)
        {
        for (retry = 0; retry < INET_RETRY_CALL; retry++)
            {
	    service = GETSERVBYNAME (protocol, "tcp");
    	    if (service) break;
            }
    	}
    }
#endif /* WIN_NT */
THREAD_ENTER;
if (!service)
    {
    sprintf (msg, 
	      "INET/INET_connect: getservbyname failed, error code = %d", 
	      H_ERRNO);
    gds__log (msg, NULL_PTR);
    inet_gen_error (port, 
            isc_network_error,
		    isc_arg_string, 
		    port->port_connection->str_data, 
		    isc_arg_gds,
		    isc_net_lookup_err, 
		    isc_arg_gds, 
		    isc_service_unknown,
 		    isc_arg_string, 
		    protocol,
		    isc_arg_string, 
		    "tcp", 
		    0);
#ifdef WINDOWS_ONLY
    NetworkLibraryCleanup ();
#endif /* WINDOWS_ONLY */	 
   return NULL;
    }

#ifdef NETWARE_386
/* LATER, bug in NOVELL getservbyname() */

service->s_port = 3050;
address.sin_port = htons (service->s_port);
#else
address.sin_port = service->s_port;
#endif

#endif  /* VMS */

/* Allocate a port block and initialize a socket for communications */

#ifdef APOLLO
port->port_handle = (int*) socket (AF_INET, SOCK_STREAM, SO_REUSEADDR);
#else
port->port_handle = (HANDLE) socket (AF_INET, SOCK_STREAM, 0);
#endif

if ((SOCKET) port->port_handle == INVALID_SOCKET)
    {
    inet_error (port, "socket", isc_net_connect_err, ERRNO);
#ifdef WINDOWS_ONLY
    NetworkLibraryCleanup ();
#endif /* WINDOWS_ONLY */	
    return NULL;
    }

/* If we're a host, just make the connection */

if (packet)
    {
    THREAD_EXIT;
    n = connect ((SOCKET) port->port_handle, 
		 (struct sockaddr *) &address, sizeof (address));
    THREAD_ENTER;
    if (n != -1 && send_full (port, packet))
       	return port;
    else
	{
	inet_error (port, "connect", isc_net_connect_err, ERRNO);
	disconnect (port);
	return NULL;
	}
    }

/* We're a server, so wait for a host to show up */

if (flag & SRVR_multi_client)
    {
    int optlen;
    struct linger lingerInfo;

    lingerInfo.l_onoff = 0;
    lingerInfo.l_linger = 0;

    optval = TRUE;
    n = setsockopt ((SOCKET) port->port_handle, SOL_SOCKET, SO_REUSEADDR,
		    (SCHAR*) &optval, sizeof (optval));
    if (n == -1)
        {
        inet_error (port, "setsockopt REUSE", isc_net_connect_listen_err, ERRNO);
        disconnect (port);
        return NULL;
        }
    
    /* Get any values for SO_LINGER so that they can be reset during
     * disconnect.  SO_LINGER should be set by default on the socket
     */
    optlen = sizeof (port->port_linger);
    n = getsockopt ((SOCKET) port->port_handle, SOL_SOCKET, SO_LINGER,
                    (SCHAR*) &port->port_linger, &optlen);

    if (n != 0) /* getsockopt failed */
        port->port_linger.l_onoff = 0;

    n = setsockopt ((SOCKET) port->port_handle, SOL_SOCKET, SO_LINGER,
                    (SCHAR*) &lingerInfo, sizeof (lingerInfo));
    if (n == -1)
        {
    	inet_error (port, "setsockopt LINGER", isc_net_connect_listen_err, ERRNO);
        disconnect (port);
        return NULL;
        }
    }

n = bind ((SOCKET) port->port_handle, 
	  (struct sockaddr *) &address, sizeof (address));

if (n == -1)
    {
    inet_error (port, "bind", isc_net_connect_listen_err, ERRNO);
    disconnect (port);
    return NULL;
    }

n = listen ((SOCKET) port->port_handle, 5);

if (n == -1)
    {
    inet_error (port, "listen", isc_net_connect_listen_err, ERRNO);
    return NULL;
    }

if (flag & SRVR_multi_client)
    {
    /* Prevent the generation of dummy keepalive packets on the
       connect port. */

    port->port_dummy_packet_interval = 0;
    port->port_dummy_timeout = 0;
    port->port_server_flags |= (SRVR_server | SRVR_multi_client);
    gds__register_cleanup (exit_handler, (void*) port);
    return port;
    }


while (TRUE)
    {
    THREAD_EXIT;
    l = sizeof (address);
    s = accept ((SOCKET) port->port_handle, 
		(struct sockaddr *) &address, &l);
    if (s == INVALID_SOCKET)
	{
	THREAD_ENTER;
	inet_error (port, "accept", isc_net_connect_err, ERRNO);
	disconnect (port);
	return NULL;
	}
#ifdef WIN_NT
    if ((flag & SRVR_debug) || !fork (s, flag))
#else
    if ((flag & SRVR_debug) || !fork())
#endif
	{
	THREAD_ENTER;
	SOCLOSE ((SOCKET) port->port_handle);
	port->port_handle = (HANDLE) s;
	port->port_server_flags |= SRVR_server;
	return port;
	}
    THREAD_ENTER;
    SOCLOSE (s);
    }
}

PORT INET_reconnect (
    HANDLE	handle,
    TEXT	*name,
    STATUS	*status_vector)
{
/**************************************
 *
 *	I N E T _ r e c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	A communications link has been established by another
 *	process.  We have inheritted the handle.  Set up
 *	a port block.
 *
 **************************************/
PORT			port;
 
port = alloc_port (NULL_PTR);
port->port_status_vector = status_vector;
status_vector [0] = gds_arg_gds;
status_vector [1] = 0;
status_vector [2] = gds_arg_end;

port->port_handle = handle;
port->port_server_flags |= SRVR_server;

return port;
}

PORT DLL_EXPORT INET_server (
    int		sock)
{
/**************************************
 *
 *	I N E T _ s e r v e r
 *
 **************************************
 *
 * Functional description
 *	We have been spawned by a master server with a connection
 *	established.  Set up port block with the appropriate socket.
 *
 **************************************/
PORT	port;

#ifdef VMS
ISC_tcp_setup (ISC_wait, gds__completion_ast);
#endif
port = alloc_port (NULL_PTR);
port->port_server_flags |= SRVR_server;
port->port_handle = (HANDLE) sock;

return port;
}

void INET_set_clients (
    int		count)
{
/**************************************
 *
 *	I N E T _ s e t _ c l i e n t s
 *
 **************************************
 *
 * Functional description
 *	Set maxinum number of clients served before
 *	starting new server
 *
 **************************************/

INET_max_clients = (count && count < MAXCLIENTS) ? count : MAXCLIENTS;
}

static int accept_connection (
    PORT	port,
    P_CNCT	*cnct)
{
/**************************************
 *
 *	a c c e p t _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Accept an incoming request for connection.  This is purely a lower
 *	level handshaking function, and does not constitute the server
 *	response for protocol selection.
 *
 **************************************/
TEXT		name [64], password [64], *id, *end, *p;
STR		string;
SLONG		eff_gid, eff_uid;
int		length, l;
#ifndef WINDOWS_ONLY
BOOLEAN		user_verification;
#endif
#ifdef APOLLO
BOOLEAN		user_set;
#endif
#if !(defined OS2_ONLY || defined PC_PLATFORM)
int		trusted;
SLONG		gids [128];
TEXT		host [MAXHOSTLEN];
struct passwd   *passwd;
int		i, gid_count;
#endif

/* Default account to "guest" (in theory all packets contain a name) */

strcpy (name, "guest");
password [0] = 0;

/* Pick up account and password, if given */

id = (TEXT*) cnct->p_cnct_user_id.cstr_address;
end = id + cnct->p_cnct_user_id.cstr_length;

eff_uid = eff_gid = -1;
#ifndef WINDOWS_ONLY
user_verification = FALSE;
#endif
while (id < end)
    switch (*id++)
	{
	case CNCT_user:
	    length = l = *id++;
	    port->port_user_name = string = (STR) ALLOCV (type_str, (int) length);
	    string->str_length = length;
	    if (length)
		{
		p = (TEXT*) string->str_data;
		do *p++ = *id++; while (--l);
		}
	    strncpy (name, string->str_data, length);
	    name[length] = (TEXT) 0;
	    break;

	case CNCT_passwd:
	    p = password;
	    if ((length = *id++) != NULL)
		do *p++ = *id++; while (--length);
	    *p = 0;
	    break;

	case CNCT_group:
	    p = (TEXT *)&eff_gid;
	    if ((length = *id++) != NULL)
		do *p++ = *id++; while (--length);
	    eff_gid = ntohl (eff_gid);
	    break;

	/* this case indicates that the client has requested that
	   we force the user name/password to be verified against
	   the security database */
	
	case CNCT_user_verification:
#ifndef WINDOWS_ONLY
	    user_verification = TRUE;
#endif
	    id++;
	    break;

	default:
	    id += *id + 1;
	}

/* See if user exists.  If not, reject connection */

#ifdef APOLLO
user_set = FALSE;
#endif

#ifdef NETWARE_386
/* force all clients of Novell to use the security database */

user_verification = TRUE;
#endif

#ifndef WINDOWS_ONLY
if (user_verification)
    {
    eff_gid = eff_uid = -1;
    port->port_flags |= PORT_not_trusted;
    }
#ifndef NETWARE_386
#ifndef OS2_ONLY
#ifndef WIN_NT
#ifdef VMS
else
    {
    if (!(trusted = check_host (port, host, name)))
	return FALSE;

    if (trusted == -1)
	{
	eff_gid = eff_uid = -1;
	port->port_flags |= PORT_not_trusted;
	}
    else
	{
	check_proxy (port, host, name);

	if (!port->port_parent)
	    {
	    if (!chuser (name))
		return FALSE;
	    ISC_get_user (name, &eff_uid, &eff_gid, 0, 0, 0, 0);
	    }
	/* ??? How to get uic for multi-client server case ??? */
	}
    }
#else
else
    {
    /* Security check should be against remote name passed - so do
       check_host first */

    passwd = getpwnam (name);

    if (!(trusted = check_host (port, host, name, passwd)))
	return FALSE;

    if (trusted == -1)
	{
	eff_gid = eff_uid = -1;
	port->port_flags |= PORT_not_trusted;
	}
    else
	{
	if (check_proxy (port, host, name))
	    passwd = getpwnam (name);
	if (!passwd)
	    return FALSE;
#ifdef NOINITGROUPS
	eff_gid = passwd->pw_gid;
#else
	initgroups (passwd->pw_name, passwd->pw_gid);
	if (eff_gid != -1)
	    {
	    gid_count = getgroups (sizeof (gids) / sizeof (SLONG), gids);
	    for (i = 0; i < gid_count; i++)
		if (gids[i] == eff_gid)
		    break;
	    if ((i == gid_count) && passwd)
		eff_gid = passwd->pw_gid;
	    }
	else
	    eff_gid = passwd->pw_gid;
#endif /* NOINITGROUPS */
	eff_uid = passwd->pw_uid;

	/* if not multi-client: set uid, gid and home directory */

	if (!port->port_parent)
	    {
	    if (!eff_gid || setregid (passwd->pw_gid, eff_gid) == -1)
		setregid (passwd->pw_gid, passwd->pw_gid);
	    if (!setreuid (passwd->pw_uid, passwd->pw_uid))
		{
#ifdef APOLLO
		user_set = TRUE;
#endif
		(void) chdir (passwd->pw_dir);
		}
	    }
	}
    }

{
/* If the environment varible ISC_INET_SERVER_HOME is set, 
 * change the home directory to the specified directory.
 * Note that this will overrule the normal setting of
 * the current directory to the effective user's home directory.
 * This feature was added primarily for testing via remote
 * loopback - but does seem to be of good general use, so
 * is activiated for the production product.
 * 1995-February-27 David Schnepper 
 */
char	*home;
if ((home = getenv ("ISC_INET_SERVER_HOME")) != NULL_PTR)
    {
    if (chdir (home))
	{
	gds__log ("inet_server: unable to cd to %s errno %d\n", home, ERRNO);
	/* We continue after the error */
	}
    }
}
#endif /* VMS */
#endif /* OS2_ONLY */
#endif /* WIN_NT */
#endif /* NETWARE_386 */
#endif /* WINDOWS_ONLY */

/* store FULL user identity in port_user_name for security purposes */

#ifdef APOLLO
if (user_set)
    {
    ALLR_free (port->port_user_name);
    port->port_user_name = NULL_PTR;
    }
else
#endif /* APOLLO */
    {
    strncpy (name, port->port_user_name->str_data, port->port_user_name->str_length);
    p = &name [port->port_user_name->str_length];
    sprintf (p, ".%ld.%ld", eff_gid, eff_uid);
    ALLR_free ((UCHAR *) port->port_user_name);
    port->port_user_name = REMOTE_make_string (name);
    port->port_user_name->str_length = strlen (name);
    }

return TRUE;
}

static  PORT alloc_port (
    PORT	parent)
{
/**************************************
 *
 *	a l l o c _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Allocate a port block, link it in to parent (if there is a parent),
 *	and initialize input and output XDR streams.
 *
 **************************************/
PORT	port;
TEXT	buffer [64];
#ifdef NETWARE_386
SSHORT  rval;
#endif

#ifdef WIN_NT
if (!INET_initialized)
    {
    WORD	version;

    version = MAKEWORD (1, 1);
    if (WSAStartup (version, &INET_wsadata))
	{
	if (parent)
	    inet_error (parent, "WSAStartup", isc_net_init_error, ERRNO);
	else
	    {
	    sprintf (buffer,
		"INET/alloc_port: WSAStartup failed, error code = %d", ERRNO);
	    gds__log (buffer, NULL_PTR);
	    }
	return NULL;
	}
    gds__register_cleanup (exit_handler, NULL_PTR);
    INET_initialized = TRUE;
    }
#endif

if (first_time == TRUE)
    {char messg[128];
    ISC_get_config (LOCK_HEADER, INET_tcp_buffer);
    if (INET_remote_buffer < MAX_DATA_LW || INET_remote_buffer > MAX_DATA_HW)
        INET_remote_buffer = DEF_MAX_DATA; 
    INET_max_data = INET_remote_buffer; 
#ifdef DEBUG
    sprintf(messg, " Info: Remote Buffer Size set to %d", INET_remote_buffer);
    gds__log(messg, NULL_PTR);
#endif
    first_time = FALSE;
    }
port = (PORT) ALLOCV (type_port, INET_remote_buffer * 2);
port->port_type = port_inet;
port->port_state = state_pending;
REMOTE_get_timeout_params (port, NULL_PTR, 0);

#if (defined PC_PLATFORM) && !(defined NETWARE_386)
strcpy (buffer, "localhost");
#else
#ifdef NETWARE_386
rval = GETHOSTNAME (buffer, sizeof (buffer));
if(rval == -1) 
    {
    GetFileServerName(0, buffer);
    strlwr(buffer);
    }
#else /* !NETWARE_386 */
GETHOSTNAME (buffer, sizeof (buffer));
#endif /* !NETWARE_386 */
#endif

port->port_host = REMOTE_make_string (buffer);
port->port_connection = REMOTE_make_string (buffer);
sprintf (buffer, "tcp (%s)", port->port_host->str_data);
port->port_version = REMOTE_make_string (buffer);

START_PORT_CRITICAL;
if (parent && !(parent->port_server_flags & SRVR_thread_per_port))
    {
    port->port_parent = parent;
    port->port_next = parent->port_clients;
    parent->port_clients = parent->port_next = port;
    port->port_handle = parent->port_handle;
    port->port_server = parent->port_server;
    port->port_server_flags = parent->port_server_flags;
    }
STOP_PORT_CRITICAL;

port->port_accept = accept_connection;
port->port_disconnect = disconnect;
port->port_receive_packet = receive;
port->port_send_packet = send_full;
port->port_send_partial = send_partial;
port->port_connect = aux_connect;
port->port_request = aux_request;
port->port_buff_size = INET_remote_buffer;

xdrinet_create (&port->port_send, port, 
	&port->port_buffer [INET_remote_buffer], INET_remote_buffer,
	XDR_ENCODE);

xdrinet_create (&port->port_receive, port, port->port_buffer, 0,
	XDR_DECODE);

return port;
}

static  PORT aux_connect (
    PORT	port,
    PACKET	*packet,
    XDR_INT	(*ast)())
{
/**************************************
 *
 *	a u x _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Try to establish an alternative connection.  Somebody has already
 *	done a successfull connect request ("packet" contains the response).
 *
 **************************************/
P_RESP			*response;
SOCKET			n;
int			l, status;
PORT			new_port;
struct sockaddr_in	address;
int			arg;

/* If this is a server, we're got an auxiliary connection.  Accept it */

if (port->port_server_flags)
    {
    l = sizeof (address);
    n = accept (port->port_channel, (struct sockaddr *) &address, &l);
    if (n == INVALID_SOCKET)
	{
	inet_error (port, "accept", isc_net_event_connect_err, ERRNO);
	SOCLOSE (port->port_channel);
	return NULL;
	}
    SOCLOSE (port->port_channel);
    port->port_handle = (HANDLE) n;
    port->port_flags |= PORT_async;
    return port;
    }

port->port_async = new_port = alloc_port (port->port_parent);
new_port->port_dummy_packet_interval = port->port_dummy_packet_interval;
new_port->port_dummy_timeout = new_port->port_dummy_packet_interval;

new_port->port_flags |= PORT_async;
response = &packet->p_resp;

/* Set up new socket */

if ((n = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
    inet_error (port, "socket", isc_net_event_connect_err, ERRNO);
    return NULL;
    }

inet_zero ((SCHAR*) &address, sizeof (address));
inet_copy (response->p_resp_data.cstr_address, &address,
       response->p_resp_data.cstr_length);
address.sin_family = AF_INET;

THREAD_EXIT;
status = connect (n, (struct sockaddr *) &address, sizeof (address));
THREAD_ENTER;

if (status < 0)
    {
    inet_error (port, "connect", isc_net_event_connect_err, ERRNO);
    SOCLOSE (n);
    return NULL;
    }

#ifdef WINDOWS_ONLY
if (ast)
    {
    extern HINSTANCE	hInstance;
    HWND		hwnd;

    /* Create the window which will get the AsyncSelect messages */

    hwnd = CreateWindow ("RemoteMsgClass", "RemoteMsgWnd",
			  WS_POPUP,
			  CW_USEDEFAULT, CW_USEDEFAULT,
			  CW_USEDEFAULT, CW_USEDEFAULT,
			  NULL, NULL, hInstance, NULL) ;

    /* Save port in the window structure so we can get it in winproc */

    SetWindowLong (hwnd, 0, (long) new_port);
    new_port->port_msg_handle = hwnd;

    /* Save event handler which will be called when event arrives */

    new_port->port_ast = ast;

    /* Ask winsock to post async message to our window when data arrives */

    if (WSAAsyncSelect(n, hwnd, WM_USER + 1, FD_READ) == -1)
	{
	inet_error (port, "AsyncSelect", isc_net_event_connect_err, ERRNO);
	return NULL;
	}
    }
#else

#if !(defined VMS || defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
if (ast)
    {
#ifdef APOLLO
    if (fcntl (n, F_SETOWN, -getpid()) < 0)
	{
	inet_error (port, "fcntl/F_SETOWN", isc_net_event_connect_err, ERRNO);
	SOCLOSE (port->port_channel);
	return NULL;
	}
    if (fcntl (n, F_SETFL, FASYNC) < 0)
	{
	inet_error (port, "fcntl/FASYNC", isc_net_event_connect_err, ERRNO);
	SOCLOSE (port->port_channel);
	return NULL;
	}
#else
#ifdef SUN3_3
    arg = -getpid();
#else
#ifdef SOLARIS
    arg = getpgrp();
#else
    arg = getpid();
#endif
#endif
    if (ioctl (n, SIOCSPGRP, &arg) < 0)
	{
	inet_error (port, "ioctl/SIOCSPGRP", isc_net_event_connect_err, ERRNO);
	SOCLOSE (port->port_channel);
	return NULL;
	}
#endif  /* APOLLO */
    new_port->port_ast = ast;
    ISC_signal (SIGURG, inet_handler, new_port);
    }
#endif
#endif /* WINDOWS_ONLY */

new_port->port_handle = (HANDLE) n;
new_port->port_flags |= port->port_flags & PORT_no_oob;

return new_port;
}

static PORT aux_request (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	a u x _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	A remote interface has requested the server prepare an auxiliary 
 *	connection; the server calls aux_request to set up the connection.
 *
 **************************************/
PORT			new_port;
P_RESP			*response;
SOCKET			n;
int			length;
struct sockaddr_in	address;
#ifndef VMS
struct hostent		*host;
TEXT			msg [64];
#endif

/* Set up new socket */

if ((n = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
    inet_error (port, "socket", isc_net_event_listen_err, ERRNO);
    return NULL;
    }

address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = 0;

if (bind (n, (struct sockaddr *) &address, sizeof (address)) < 0)
    {
    inet_error (port, "bind", isc_net_event_listen_err, ERRNO);
    return NULL;
    }

length = sizeof (address);

if (getsockname (n, (struct sockaddr *) &address, &length) < 0)
    {
    inet_error (port, "getsockname", isc_net_event_listen_err, ERRNO);
    return NULL;
    }

if (listen (n, 1) < 0)
    {
    inet_error (port, "listen", isc_net_event_listen_err, ERRNO);
    return NULL;
    }

port->port_async = new_port = alloc_port (port->port_parent);
new_port->port_dummy_packet_interval = port->port_dummy_packet_interval;
new_port->port_dummy_timeout = new_port->port_dummy_packet_interval;

new_port->port_server_flags = port->port_server_flags;
new_port->port_channel = (int) n;
new_port->port_flags = port->port_flags & PORT_no_oob;

response = &packet->p_resp;

#ifdef VMS
if (getaddr (port->port_host->str_data, &address) == -1)
    {
    inet_error (port, "gethostbyname", isc_net_event_listen_err, 0);
    return NULL;
    }
#else
THREAD_EXIT;
host = GETHOSTBYNAME (port->port_host->str_data);
THREAD_ENTER;
if (!host)
    {
    sprintf (msg, "INET/aux_request: gethostbyname failed, error code = %d", 
	H_ERRNO);
    gds__log (msg, NULL_PTR);
    inet_gen_error (port,
                    isc_network_error,
	                isc_arg_string,
                    port->port_host->str_data, 
	                isc_arg_gds,
                    isc_net_lookup_err, 
	                isc_arg_gds,
                    isc_host_unknown,
                    0);
    return NULL;
    }
inet_copy (host->h_addr, &address.sin_addr, sizeof (address.sin_addr));
#endif

response->p_resp_data.cstr_address = (UCHAR*) &response->p_resp_blob_id;
response->p_resp_data.cstr_length = sizeof (response->p_resp_blob_id);
inet_copy (&address, response->p_resp_data.cstr_address, 
       response->p_resp_data.cstr_length);

return new_port;
}

#ifdef VMS
static check_host (
    PORT	port,
    TEXT	*host_name,
    TEXT	*user_name)
{
/**************************************
 *
 *	c h e c k _ h o s t	( V M S )
 *
 **************************************
 *
 * Functional description
 *	Check the host on the other end of the socket to see it
 *	it's an equivalent host.
 *
 **************************************/
int			length,result;
struct sockaddr_in	address;
TEXT			hosts_file [MAXPATHLEN];

length = sizeof (address);

if (getpeername ((int) port->port_handle, &address, &length) == -1)
    return FALSE;

if (getnamebyaddr (&address, sizeof (address), host_name) == -1)
    return FALSE;

gethosts (hosts_file);
if ((result = parse_hosts (hosts_file, host_name, user_name)) == -1)
    result = FALSE;

return result;
}
#endif

#if !(defined VMS || defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
static int check_host (
    PORT	port,
    TEXT	*host_name,
    TEXT	*user_name,
    struct passwd *passwd)
{
/**************************************
 *
 *	c h e c k _ h o s t 	 ( n o n - V M S )
 *
 **************************************
 *
 * Functional description
 *	Check the host on the other end of the socket to see it
 *	it's an equivalent host.
 * NB.: First check the ~/.rhosts then the HOSTS_FILE - both have
 *	the same line formats (see parse_line)
 *
 **************************************/
TEXT			user [64], rhosts [MAXPATHLEN],*hosts_file;
IB_FILE 			*fp;
int			length,result;
struct sockaddr_in	address;
struct hostent		*host;

length = sizeof (address);

if (getpeername ((int) port->port_handle, &address, &length) == -1)
    return 0;


if (!(host = gethostbyaddr ((SCHAR *) &address.sin_addr, sizeof (address.sin_addr), address.sin_family)))
    return 0;

result = -1;

strcpy (host_name, host->h_name);

if (passwd)
    {
    strcpy (user, passwd->pw_name);
    strcpy (rhosts, passwd->pw_dir);
    strcat (rhosts, "/.rhosts");
    result = parse_hosts (rhosts, host_name, user);
    }
else
    strcpy (user, user_name);

if (result == -1)
   {
   fp = ib_fopen (GDS_HOSTS_FILE, "r");
   hosts_file = fp ? GDS_HOSTS_FILE : HOSTS_FILE;
   if (fp)
     ib_fclose (fp);

    if ((result = parse_hosts (hosts_file, host_name, user)) == -1)
	result = FALSE;
    }
return result;
}
#endif

#if !(defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
static BOOLEAN check_proxy (
    PORT	port,
    TEXT	*host_name,
    TEXT	*user_name)
{
/**************************************
 *
 *	c h e c k _ p r o x y
 *
 **************************************
 *
 * Functional description
 *	Lookup <host_name, user_name> in proxy file.  If found,
 *	change user_name.
 *
 **************************************/
IB_FILE	*proxy;
TEXT	*p, proxy_file [64], source_user [64], source_host [MAXHOSTLEN],
	target_user [64], line [128];
int	c;
BOOLEAN	result;
SLONG	length;
STR	string;

#ifndef VMS
strcpy (proxy_file, PROXY_FILE);
#else
gds__prefix (proxy_file, PROXY_FILE);
#endif
if (!(proxy = ib_fopen (proxy_file, "r")))
    return FALSE;

/* Read lines, scan, and compare */

result = FALSE;

for (;;)
    {
    for (p = line; ((c = ib_getc (proxy)) != NULL) && c != EOF && c != '\n';)
	*p++ = c;
    *p = 0;
    if (sscanf (line, " %[^:]:%s%s", source_host, source_user, target_user) >= 3)
	if ((!strcmp (source_host, host_name) ||
	     !strcmp (source_host, "*")) &&
	    (!strcmp (source_user, user_name) ||
	     !strcmp (source_user, "*")))
	    {
	    ALLR_free (port->port_user_name);
	    length = strlen (target_user);
	    port->port_user_name = string = (STR) ALLOCV (type_str, (int) length);
	    string->str_length = length;
	    strncpy (string->str_data, target_user, length);
	    strcpy (user_name, target_user);
	    result = TRUE;
	    break;
	    }
    if (c == EOF)
	break;
    }
	
ib_fclose (proxy);

return result;
}
#endif

static void disconnect (
    PORT	port)
{
/**************************************
 *
 *	d i s c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Break a remote connection.
 *
 **************************************/
PORT	parent, *ptr;
int	n;
#ifdef	DEFER_PORT_CLEANUP
SSHORT	defer_cleanup = 0;
#endif

#ifndef VMS
/* SO_LINGER was turned off on the initial bind when the server was started.
 * This will force a reset to be sent to the client when the socket is closed.
 * We only want this behavior in the case of the server terminiating
 * abnormally and not on an orderly shut down.  Because of this, turn the
 * SO_LINGER option back on for the socket.  The result of setsockopt isn't
 * too important at this stage since we are closing the socket anyway.  This
 * is an attempt to return the socket to a state where a graceful shutdown can
 * occur.
 */

if (port->port_linger.l_onoff)
    {
    n = setsockopt ((SOCKET) port->port_handle, SOL_SOCKET, SO_LINGER,
                    (SCHAR*)&port->port_linger, sizeof (port->port_linger));
    }

shutdown ((int) port->port_handle, 2);
#endif

#if !(defined VMS || defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
if (port->port_ast)
    ISC_signal_cancel (SIGURG, inet_handler, port);
#endif

/* If this is a sub-port, unlink it from it's parent */

#ifdef  DEFER_PORT_CLEANUP
port->port_state = state_disconnected;
#endif
if ((parent = port->port_parent) != NULL)
    {
    if (port->port_async)
    	{
    	disconnect (port->port_async);
    	port->port_async = NULL;
    	}
#ifdef	DEFER_PORT_CLEANUP
    defer_cleanup = 1;
#else
    unhook_port (port, parent);
#endif
    }
else
    if (port->port_async)
	{
#ifdef MULTI_THREAD
	port->port_async->port_flags |= PORT_disconnect;
#else
	disconnect (port->port_async);
	port->port_async = NULL;
#endif
	}

SOCLOSE ((SOCKET) port->port_handle);

#ifdef WINDOWS_ONLY
NetworkLibraryCleanup ();
if (port->port_msg_handle)
    /* close socket should flush any async postings, so its safe to destroy */
    DestroyWindow (port->port_msg_handle);
#endif

gds__unregister_cleanup (exit_handler, (void*) port);

#ifdef DEFER_PORT_CLEANUP
if (!defer_cleanup)
#endif
    cleanup_port (port);

#ifdef DEBUG
if (INET_trace & TRACE_summary)
    {
    ib_fprintf (ib_stdout,"INET_count_send = %d packets\n", INET_count_send);
    ib_fprintf (ib_stdout,"INET_bytes_send = %d bytes\n", INET_bytes_send);
    ib_fprintf (ib_stdout,"INET_count_recv = %d packets\n", INET_count_recv);
    ib_fprintf (ib_stdout,"INET_bytes_recv = %d bytes\n", INET_bytes_recv);
    ib_fflush(ib_stdout);
    }
#endif

return;
}

#ifdef WINDOWS_ONLY
static void NetworkLibraryCleanup ()
{
/***********************************************
 *
 *	N e t w o r k L i b r a r y C l e a n u p 
 *
 ***********************************************
 *
 * Functional description
 *	This function is called whenever the user terminates
 *	his/her conversation with the server (partially or
 *	completely).  We get the current task ID and then
 *	decrement its use count.  When that count reaches 
 *	zero, we call WSACleanup to cleanup the network layer
 *	on behalf of the task.  We then check to see if that
 *	was the last task in our task array, if so we call
 *	FreeLibrary to unload WINSOCK.DLL
 *
 ***********************************************/
HINSTANCE  	hTask;
SSHORT		n, i;

extern TaskInfoType  Tasks[MAX_TASKS];

hTask = GetCurrentTask();

for (i = 0; i < MAX_TASKS && Tasks[i].hId != 0; i++)
    {
    if (Tasks[i].hId == hTask)
	/* We found our task, now lets call WSACleanup */
	{
	if (--Tasks[i].nUseCount == 0)
	    {
	    Tasks[i].hId = 0;
	    WSACleanup ();
	    }	
	break;
	}
    }

if (i < MAX_TASKS && Tasks[i].nUseCount == 0)
    {
    for (n = i + 1; n < MAX_TASKS && Tasks[n].hId != 0; n++)
    	{
    	Tasks[i].hId = Tasks[n].hId;
    	Tasks[i].nUseCount = Tasks[n].nUseCount;
    	Tasks[n].hId = 0;
    	Tasks[n].nUseCount = 0;
    	i = n;
    	}
    }
return;
}
#endif /* WINDOWS_ONLY */

#ifdef WINDOWS_ONLY
void inet_cleanup ()
{
/***********************************************
 *
 *	i n e t _ c l e a n u p 
 *
 ***********************************************
 *
 * Functional description
 *	Call FreeLibrary to unload WINSOCK.DLL
 *
 ***********************************************/
extern HINSTANCE	hWinsockDLL;
extern int		INET_initialized;

if (INET_initialized)
    FreeLibrary (hWinsockDLL);
return;
}
#endif /* WINDOWS_ONLY */

static void cleanup_port (
    PORT	port)
{
/**************************************
 *
 *	c l e a n u p _ p o r t 
 *
 **************************************
 *
 * Functional description
 *	Walk through the port structure freeing
 * 	allocated memory and then free the port.
 *
 **************************************/

if (port->port_version)
    ALLR_free ((UCHAR*) port->port_version);

if (port->port_connection)
    ALLR_free ((UCHAR*) port->port_connection);
	
if (port->port_user_name)
    ALLR_free ((UCHAR*) port->port_user_name);

if (port->port_host)
    ALLR_free ((UCHAR*) port->port_host);

if (port->port_object_vector)
    ALLR_free ((UCHAR*) port->port_object_vector);

#ifdef DEBUG_XDR_MEMORY
if (port->port_packet_vector)
    ALLR_free ((UCHAR*) port->port_packet_vector);
#endif

ALLR_release ((UCHAR*) port);
return;
}

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
 *	Shutdown all active connections
 *	to allow restart.
 *
 **************************************/
PORT	port, main_port;

main_port = (PORT) arg;

#ifdef WIN_NT
if (!main_port)
    {
    WSACleanup();
    return;
    }
#endif

#ifndef VMS
for (port = main_port; port; port = port->port_next)
    {
    shutdown ((int) port->port_handle, 2);
    SOCLOSE ((SOCKET) port->port_handle);
    }
#endif
}

#ifdef NO_FORK
static int fork (void)
{
/**************************************
 *
 *	f o r k		( N O _ F O R K )
 *
 **************************************
 *
 * Functional description
 *	Hmmm.
 *
 **************************************/

return 1;
}
#endif

#ifdef WIN_NT
static int fork (
    SOCKET	old_handle,
    USHORT	flag)
{
/**************************************
 *
 *	f o r k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Create a child process.
 *
 **************************************/
HANDLE	 		new_handle;
USHORT			ret;
STARTUPINFO		start_crud;
PROCESS_INFORMATION	pi;

if (!INET_command_line [0])
    {
    SC_HANDLE	manager, service;

    if ((manager = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT)) &&
	(service = OpenService (manager, REMOTE_SERVICE, SERVICE_QUERY_CONFIG)))
	{
	LPQUERY_SERVICE_CONFIG	config;
	SCHAR			buffer [1024];
	DWORD			config_len;

	config = (LPQUERY_SERVICE_CONFIG) buffer;
	if (!QueryServiceConfig (service, config, sizeof (buffer), &config_len))
	    {
	    THREAD_ENTER;
	    config = (LPQUERY_SERVICE_CONFIG) ALLR_alloc (config_len);
	    /* NOMEM: ALLR_alloc handled */
	    /* FREE:  later in this block */
	    QueryServiceConfig (service, config, config_len, &config_len);
	    }
	strcpy (INET_command_line, config->lpBinaryPathName);
	if ((SCHAR*) config != buffer)
	    {
	    ALLR_free (config);
	    THREAD_EXIT;
	    }
	CloseServiceHandle (service);
	}
    else
	strcpy (INET_command_line, GetCommandLine());
    CloseServiceHandle (manager);
    INET_p = INET_command_line + strlen (INET_command_line);
    }

DuplicateHandle (GetCurrentProcess(), (HANDLE) old_handle,
    GetCurrentProcess(), &new_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);

sprintf (INET_p, " -s -i -h %d", (SLONG) new_handle);
start_crud.cb = sizeof (STARTUPINFO);
start_crud.lpReserved = NULL;
start_crud.lpReserved2 = NULL;
start_crud.cbReserved2 = 0;
start_crud.lpDesktop = NULL;
start_crud.lpTitle = NULL;
start_crud.dwFlags = 0;
if (ret = CreateProcess (NULL,
	INET_command_line,
	NULL,
	NULL,
	TRUE,
	(flag & SRVR_high_priority ?
	HIGH_PRIORITY_CLASS   | DETACHED_PROCESS :
	NORMAL_PRIORITY_CLASS | DETACHED_PROCESS),
	NULL,
	NULL,
	&start_crud,
	&pi))
	{
	CloseHandle (pi.hThread);
	CloseHandle (pi.hProcess);
	}
CloseHandle (new_handle);

return 1;
}
#endif

static void inet_copy (
    SCHAR	*from,
    SCHAR	*to,
    int		length)
{
/**************************************
 *
 *	i n e t _ c o p y
 *
 **************************************
 *
 * Functional description
 *	Copy a number of bytes;
 *
 **************************************/

if (length)
    do *to++ = *from++; while ((--length) != 0);
}

static void inet_zero (
    SCHAR	*address,
    int		length)
{
/**************************************
 *
 *	i n e t _  z e r o
 *
 **************************************
 *
 * Functional description
 *	Zero a block of memory.
 *
 **************************************/

if (length)
    do *address++ = 0; while ((--length) != 0);
}

#if !(defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
static int parse_hosts (
    TEXT	*file_name,
    TEXT	*host_name,
    TEXT	*user_name)
{
/*****************************************************************
 *
 *	p a r s e _ h o s t s
 *
 *****************************************************************
 *
 * Functional description:
 *	Parse hosts file (.rhosts or hosts.equiv) to determine
 *	if user_name on host_name should be allowed access.
 *
 *****************************************************************/
IB_FILE	*fp;
int	c, result;
TEXT	*p, line [256], entry1 [256], entry2[256];

result = -1;
fp = ib_fopen (file_name, "r");

if (fp)
    {
    for (;;)
	{
	entry1[0] = entry2[0] = 0;
	entry1[1] = entry2[1] = 0;
	for (p = line;(c = ib_getc (fp)) != EOF && c != '\n';)
	    *p++ = c;
	*p = 0;
	sscanf (line, "%s", entry1);
	sscanf (&line [strlen (entry1)], "%s", entry2);
	result = parse_line (entry1, entry2, host_name, user_name);
	if (c == EOF || result > -1)
	    break;
	}
    ib_fclose (fp);
    }

return result;
}
#endif

#if !(defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
static int parse_line (
    TEXT	*entry1,
    TEXT	*entry2,
    TEXT	*host_name,
    TEXT	*user_name)
{
/*****************************************************************
 *
 *	p a r s e _ l i n e 
 *
 *****************************************************************
 *
 * Functional description:
 *	Parse hosts file (.rhosts or hosts.equiv) to determine
 *	if user_name on host_name should be allowed access.
 *
 * only supporting:
 *    + - anybody on any machine
 *    -@machinegroup [2nd entry optional] - nobody on that machine
 *    +@machinegroup - anybody on that machine
 *    +@machinegroup username - this person on these machines
 *    +@machinegroup +@usergroup - these people  on these machines
 *    +@machinegroup -@usergroup - anybody on these machines EXCEPT these people
 *    machinename - anyone on this machine
 *    machinename username - this person on this machine
 *    machinename +@usergroup - these people on this machine
 *    machinename -@usergroup - anybody but these people on this machine
 *    machinename + - anybody on this machine
 *
 ******************************************************************/

/* if entry1 is simply a "+" - everyone's in */

if (!strcmp (entry1, "+"))
    return TRUE;

/* if we don't have a host_name match, don't bother */

if (strcmp (entry1, host_name))
#ifdef UNIX
#if !(defined XENIX || defined EPSON || defined UNIXWARE || defined NCR3000)
    if (entry1[1] == '@')
	{
	if (!innetgr (&entry1[2], host_name, 0, 0))
	    return -1;
	}
    else
#endif
#endif
	return -1;

/* if we have a host_name match and an exclude symbol - they're out */

if (entry1[0] == '-')
    return FALSE;

/* if we matched the machine and seen a + after the machine
   name they are in (eg: <machinename> + ) */

if ((entry2[0] == '+') && (strlen (entry2) == 1))
    return TRUE;

/* if we don't have a second entry OR it matches the user_name - they're in */

if (!entry2[0] || !strcmp (entry2, user_name))
    return TRUE;

/* if they're in the user group: + they're in, - they're out */
      
#ifdef UNIX
#if !(defined XENIX || defined EPSON || defined UNIXWARE || defined NCR3000)
if (entry2[1] == '@') 
    {
    if (innetgr (&entry2[2], 0, user_name, 0))
	{
	if (entry2[0] == '+')
	    return TRUE;
	else
	    return FALSE;
	}
    else
	{
	/* if they're NOT in the user group AND we're excluding it 
	   - they're in */

	if (entry2[0] == '-')
	    return TRUE;
	}
    }
#endif
#endif

/* if we get here - we matched machine but not user - maybe next line ... */

return -1;
}
#endif

static PORT receive (
    PORT	main_port,
    PACKET	*packet)
{
/**************************************
 *
 *	r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a message from a port or clients of a port.  If the process
 *	is a server and a connection request comes in, generate a new port
 *	block for the client.
 *
 **************************************/
PORT	port;

/* If this isn't a multi-client server, just do the operation and get it
   over with */

if (!(main_port->port_server_flags & SRVR_multi_client))
    {
    /* loop as long as we are receiving dummy packets, just
       throwing them away--note that if we are a server we won't 
       be receiving them, but it is better to check for them at 
       this level rather than try to catch them in all places where 
       this routine is called */

    do  {
	if (!xdr_protocol (&main_port->port_receive, packet))
	    return NULL;
#ifdef DEBUG
	{
	static ULONG	op_rec_count = 0;
	op_rec_count++;
	if (INET_trace & TRACE_operations)
	    {
	    ib_fprintf (ib_stdout,"%04d: OP Recd %5d opcode %d\n", 
			inet_debug_timer(),
			op_rec_count, packet->p_operation);
	    ib_fflush (ib_stdout);
	    }
	}
#endif
	} 
	while (packet->p_operation == op_dummy);

    return main_port;
    }

/* Multi-client server multiplexes all known ports for incoming packets. */

for (;;)
    {
#ifdef NETWARE_386
    port = select_port (main_port);
#else
    port = select_port (main_port, &INET_select);
#endif
    if (port == main_port)
	{
	if (port = select_accept (main_port))
	    return port;
	continue;
	}
    if (port)
    	{
	if (port->port_dummy_timeout < 0)
	    {
	    port->port_dummy_timeout = port->port_dummy_packet_interval;
	    if (port->port_flags & PORT_async ||
		port->port_protocol < PROTOCOL_VERSION8)
		continue;
	    packet->p_operation = op_dummy;
	    return port;
	    }
	/* We've got data -- lap it up and use it */

	if (!xdr_protocol (&port->port_receive, packet))
	    packet->p_operation = op_exit;
#ifdef DEBUG
	{
	static ULONG op_rec_count = 0;
	op_rec_count++;
	if (INET_trace & TRACE_operations)
	    {
	    ib_fprintf (ib_stdout,"%05d: OP Recd %5d opcode %d\n", 
			inet_debug_timer(),
			op_rec_count, packet->p_operation);
	    ib_fflush (ib_stdout);
	    }
	};
#endif

/*  Make sure that there are no more messages in this port before blocking
    ourselves in select_wait. If there are more messages then set the flag
    corresponding to this port which was cleared in select_port routine.
*/
#ifndef NETWARE_386
	if (port->port_receive.x_handy)
	    {
	    FD_SET ((SLONG) port->port_handle, &INET_select.slct_fdset);
	    ++INET_select.slct_count;
	    }
#else
        /* DGUX compiler does not treat correctly abbreviation in such a constructions so
           "we've" was changed to "we have" */
		/* 5.5 SCO Port: Used #error to generate compiler error */
#error	Generated a compiler error since we have not put in the NetWare specific code here.
#endif
	if (packet->p_operation == op_dummy)
	    continue;

	return port;
	}
#ifdef NETWARE_386
    if (!select_wait (main_port))
#else
    if (!select_wait (main_port, &INET_select))
#endif
	return NULL;
    }
}

static PORT select_accept (
    PORT	main_port)
{
/**************************************
 *
 *	s e l e c t _ a c c e p t
 *
 **************************************
 *
 * Functional description
 *	Accept a new connection request.
 *	If client load is exceeded then
 *	handoff connection responsibility
 *	to a fresh server.
 *
 **************************************/
PORT			port;
int			l;
struct sockaddr_in      address;
int			optval = 1;
#if !(defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
TEXT			msg [64];
int			n;
#endif

port = alloc_port (main_port);
l = sizeof (address);

port->port_handle = (HANDLE) accept ((SOCKET) main_port->port_handle, 
		   		     (struct sockaddr *) &address, &l);
if ((SOCKET) port->port_handle == INVALID_SOCKET)
    {
    inet_error (port, "accept", isc_net_connect_err, ERRNO);
    disconnect (port);
    return NULL_PTR;
    }

setsockopt ((SOCKET) port->port_handle, SOL_SOCKET, SO_KEEPALIVE,
            (SCHAR*) &optval, sizeof (optval));

#ifdef WINDOWS_ONLY
/* count this for the PC router! */

initWSA(port);
#endif  /* WINDOWS_ONLY */

#if !(defined SUPERSERVER || defined VMS || defined NETWARE_386 || \
      defined PC_PLATFORM || defined mpexl || defined OS2_ONLY || defined WIN_NT)
for (n = 0, port = main_port->port_clients; port; n++, port = port->port_next)
    ;
if (n >= INET_max_clients)
    {
    main_port->port_state = state_closed;
    SOCLOSE ((int) main_port->port_handle);

    sprintf (msg, "INET/select_accept: exec new server at client limit: %d", n);
    gds__log (msg, NULL_PTR); 

    setreuid (0, 0);
    kill (getppid(), SIGUSR1);
    }
#endif

if (main_port->port_server_flags & SRVR_thread_per_port)
    {
    port->port_server_flags = (SRVR_server | SRVR_inet | SRVR_thread_per_port);
    return port;
    }

return NULL_PTR;
}

#ifdef NETWARE_386
static PORT select_port (PORT	main_port)
{
/**************************************
 *
 *	s e l e c t _ p o r t  ( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Select a descriptor that is ready to read
 *	and return the port block. Return NULL if
 *	none are active.
 *
 **************************************/
PORT	port;
int	n;
int     i;

if (!select_count)
    return NULL;

for (port = main_port; port; port = port->port_next)
 if ((port->port_fdset) && FD_ISSET (port->port_handle, port->port_fdset))
 {
  FD_CLR (port->port_handle, (fd_set *)port->port_fdset);
  --select_count;
  return port;
 } 

/* if we cannot find any ports data in fd_set is bad reset it */
for (i = 0; i < MAX_FD_SETS; i++)
 FD_ZERO(&(fd_sets[i]));
select_count = 0;
return NULL;
}

static int select_wait (PORT	main_port)
{
/**************************************
 *
 *	s e l e c t _ w a i t    ( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Select interesting descriptors from
 *	port blocks and wait for something
 *	to read from them.
 *
 **************************************/
PORT	port;
USHORT i;
USHORT found;
USHORT set_number;
int count = 0;

struct timeval	timeout;

timeout.tv_sec = 0;
timeout.tv_usec = 0;

THREAD_EXIT;

while (!select_count)
{
 for (i = 0; i < MAX_FD_SETS; i++)
  FD_ZERO (&(fd_sets[i]));

 found = 0;
 set_number = 0;
 for (port = main_port; port; port = port->port_next)
  if ((port->port_state == state_active) ||
      (port->port_state == state_pending))
  {
   if (((found % FD_SETSIZE) == 0) && (found != 0))
    set_number++;

   FD_SET ((SLONG) port->port_handle, &(fd_sets[set_number]));
   port->port_fdset = &(fd_sets[set_number]);
   found++;
  }

if (set_number == 0)
    {
    count = select (FD_SETSIZE, &(fd_sets[0]), 0, 0, NULL);
    if (count != -1)
        select_count += count;
    }
else
    {
    for (i = 0; i <= set_number; i++)
        {
        count = select (FD_SETSIZE, &(fd_sets[i]), 0, 0, &timeout);
        if (count != -1)
            select_count += count;
        }
    }

 ThreadSwitch();
}

THREAD_ENTER;

return(TRUE);
}
#endif

#ifndef NETWARE_386
static PORT select_port (
    PORT	main_port,
    SLCT	*selct)
{
/**************************************
 *
 *	s e l e c t _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Select a descriptor that is ready to read
 *	and return the port block. Additionally,
 *	check if a port's keepalive timer has
 *	expired and return the port block so that
 *	a keepalive packet can be queued. Return
 *	NULL if	none are active.
 *
 **************************************/
PORT	port;
int	n;

#ifdef WIN_NT

/* NT's socket handles are addresses */

START_PORT_CRITICAL;
#ifdef  DEFER_PORT_CLEANUP
unhook_disconnected_ports (main_port);
#endif
for (port = main_port; port; port = port->port_next)
    if (FD_ISSET (port->port_handle, &selct->slct_fdset))
        {
	port->port_dummy_timeout = port->port_dummy_packet_interval;
        FD_CLR (port->port_handle, &selct->slct_fdset);
        --selct->slct_count;
        STOP_PORT_CRITICAL;
        return port;
        }
    else if (port->port_dummy_timeout < 0)
	{
	STOP_PORT_CRITICAL;
	return port;
	}

STOP_PORT_CRITICAL;
#else
#ifdef PC_PLATFORM
/* Check for handle in the FD array */

for (port = main_port; port; port = port->port_next)
    for (n = 0; n < selct->slct_fdset.fd_count; n++)
	if (selct->slct_fdset.fd_array [n] == (int) (port->port_handle))
	    {
	    port->port_dummy_timeout = port->port_dummy_packet_interval;
	    FD_CLR ((int) (port->port_handle), &selct->slct_fdset);
	    --selct->slct_count;
	    return port;
	    }
	else if (port->port_dummy_timeout < 0)
	    return port;
#else
START_PORT_CRITICAL;
#ifdef  DEFER_PORT_CLEANUP
unhook_disconnected_ports (main_port);
#endif
for (port = main_port; port; port = port->port_next)
    {
    n = (int) port->port_handle;
    if (n < selct->slct_width && FD_ISSET (n, &selct->slct_fdset))
	{
	port->port_dummy_timeout = port->port_dummy_packet_interval;
	FD_CLR (n, &selct->slct_fdset);
	--selct->slct_count;
        STOP_PORT_CRITICAL;
	return port;
	}
    else if (port->port_dummy_timeout < 0)
        {
        STOP_PORT_CRITICAL;
	return port;
        }
    }
STOP_PORT_CRITICAL;
#endif
#endif

return NULL;
}

static int select_wait (
    PORT	main_port,
    SLCT	*selct)
{
/**************************************
 *
 *	s e l e c t _ w a i t
 *
 **************************************
 *
 * Functional description
 *	Select interesting descriptors from
 *	port blocks and wait for something
 *	to read from them.
 *
 **************************************/
PORT	port;
USHORT	found;
TEXT	msg [64];
SLONG	delta_time;
struct timeval	timeout;

timeout.tv_sec = SELECT_TIMEOUT;
timeout.tv_usec = 0;

for (;;)
    {
    selct->slct_count = selct->slct_width = 0;
    FD_ZERO (&selct->slct_fdset);
    found = FALSE;

    /* Use the time interval between select() calls to expire
       keepalive timers on all ports.*/

    if (selct->slct_time)
	{
    	delta_time = (SLONG) time (NULL) - selct->slct_time;
    	selct->slct_time += delta_time;
	}
    else
	{
	delta_time = 0;
	selct->slct_time = (SLONG) time (NULL);
	}

    START_PORT_CRITICAL;
#ifdef  DEFER_PORT_CLEANUP
    unhook_disconnected_ports (main_port);
#endif
    for (port = main_port; port; port = port->port_next)
    	if ((port->port_state == state_active) ||
	    (port->port_state == state_pending))
	    {
	    /* Adjust down the port's keepalive timer.*/

	    if (port->port_dummy_packet_interval)
	    	port->port_dummy_timeout -= delta_time;

	    FD_SET ((SLONG) port->port_handle, &selct->slct_fdset);
#ifdef WIN_NT
	    ++selct->slct_width;
#else
	    selct->slct_width = MAX (selct->slct_width, (int) port->port_handle);
#endif
	    found = TRUE;
	    }
    STOP_PORT_CRITICAL;

    if (!found)
    	{
    	gds__log ("INET/select_wait: client rundown complete, server exiting", NULL_PTR);
    	return FALSE;
    	}

    THREAD_EXIT;
    ++selct->slct_width;

    for (;;)
    	{
#ifdef WIN_NT
    	selct->slct_count = select (FD_SETSIZE, &selct->slct_fdset,
	    (fd_set*) NULL, (fd_set*) NULL, &timeout);
#else
    	selct->slct_count = select (selct->slct_width, &selct->slct_fdset,
	    (fd_set*) NULL, (fd_set*) NULL, &timeout);
#endif
    	if (selct->slct_count != -1)
	    {
            /* if selct->slct_count is zero it means that we timed out of
               select with nothing to read or accept, so clear the fd_set 
               bit as this value is undefined on some platforms (eg. HP-UX), 
               when the select call times out. Once these bits are cleared
               they can be used in select_port() */
            if (selct->slct_count == 0)
                {
                for (port = main_port; port; port = port->port_next)
                    FD_CLR (port->port_handle, &selct->slct_fdset);
                }
	    THREAD_ENTER;
	    return TRUE;
	    }
	else if (INTERRUPT_ERROR(ERRNO))
	    continue;
#ifndef WIN_NT
	else if (ERRNO == EBADF)
	    break;
#else
	else if (ERRNO == WSAENOTSOCK)
	    break;
#endif
	else
	    {
	    THREAD_ENTER;
	    sprintf (msg, "INET/select_wait: select failed, errno = %d", ERRNO);
	    gds__log (msg, NULL_PTR);
	    return FALSE;
	    }
    	}

    THREAD_ENTER;
    }
}
#endif

static int send_full (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	s e n d _ f u l l
 *
 **************************************
 *
 * Functional description
 *	Send a packet across a port to another process.
 *
 **************************************/

if (!xdr_protocol (&port->port_send, packet))
    return FALSE;

#ifdef DEBUG
{
static ULONG op_sent_count = 0;
op_sent_count++;
if (INET_trace & TRACE_operations)
    {
    ib_fprintf (ib_stdout,"%05d: OP Sent %5d opcode %d\n", 
		inet_debug_timer(),
		op_sent_count, packet->p_operation);
    ib_fflush (ib_stdout);
    };
};
#endif

return xdrinet_endofrecord (&port->port_send, TRUE);
}

static int send_partial (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	s e n d _ p a r t i a l
 *
 **************************************
 *
 * Functional description
 *	Send a packet across a port to another process.
 *
 **************************************/

#ifdef DEBUG
{
static ULONG	op_sentp_count = 0;
op_sentp_count++;
if (INET_trace & TRACE_operations)
    {
    ib_fprintf (ib_stdout,"%05d: OP Sent %5d opcode %d (partial)\n", 
		inet_debug_timer(),
		op_sentp_count, packet->p_operation);
    ib_fflush (ib_stdout);
    };
};
#endif

return xdr_protocol (&port->port_send, packet);
}


static int xdrinet_create (
    XDR			*xdrs,
    PORT		port,
    UCHAR		*buffer,
    USHORT		length,
    enum xdr_op		x_op)
{
/**************************************
 *
 *	x d r i n e t _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Initialize an XDR stream for Apollo mailboxes.
 *
 **************************************/

xdrs->x_public = (caddr_t) port;
xdrs->x_base = xdrs->x_private = (SCHAR*) buffer;
xdrs->x_handy = length;
xdrs->x_ops = &inet_ops;
xdrs->x_op = x_op;

return TRUE;
}

static bool_t xdrinet_endofrecord (
    XDR		*xdrs,
    bool_t	flushnow)
{
/**************************************
 *
 *	x d r i n e t _ e n d o f r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Write out the rest of a record.
 *
 **************************************/

return inet_write (xdrs, flushnow);
}

static void alarm_handler (
    int		x)
{
/**************************************
 *
 *	a l a r m _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Handle an alarm clock interrupt.  If we were waiting on
 *	a semaphone, zap it.
 *
 **************************************/
}

static XDR_INT inet_destroy (
    XDR		*xdrs)
{
/**************************************
 *
 *	i n e t _ d e s t r o y
 *
 **************************************
 *
 * Functional description
 *	Destroy a stream.  A no-op.
 *
 **************************************/

return NULL;
}

static void inet_gen_error (
    PORT	port,
    STATUS	status,
    ...)
{
/**************************************
 *
 *	i n e t _ g e n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	An error has occurred.  Mark the port as broken.
 *	Format the status vector if there is one and
 *	save the status vector strings in a permanent place.
 *
 **************************************/
STATUS	*status_vector;

port->port_flags |= PORT_broken;
port->port_state = state_broken;

status_vector = NULL;
if (port->port_context != NULL)
    status_vector = port->port_context->rdb_status_vector;
if (status_vector == NULL)
    status_vector = port->port_status_vector;
if (status_vector != NULL)
    {
    STUFF_STATUS (status_vector, status)
    REMOTE_save_status_strings (status_vector);
    }
}

static bool_t inet_getbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    u_int	count)
{
/**************************************
 *
 *	i n e t _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Get a bunch of bytes from a memory stream if it fits.
 *
 **************************************/
SLONG	bytecount = count;

/* Use memcpy to optimize bulk transfers. */

while (bytecount > sizeof (GDS_QUAD))
    {
    if (xdrs->x_handy >= bytecount)
	{
	memcpy (buff, xdrs->x_private, bytecount);
	xdrs->x_private += bytecount;
	xdrs->x_handy -= bytecount;
	return TRUE;
	}
    else
	{
	if (xdrs->x_handy > 0)
	    {
	    memcpy (buff, xdrs->x_private, xdrs->x_handy);
	    xdrs->x_private += xdrs->x_handy;
	    buff += xdrs->x_handy;
	    bytecount -= xdrs->x_handy;
	    xdrs->x_handy = 0;
	    }
	if (!inet_read (xdrs))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

if (!bytecount)
    return TRUE;

if (xdrs->x_handy >= bytecount)
    {
    xdrs->x_handy -= bytecount;
    do *buff++ = *xdrs->x_private++; while (--bytecount);
    return TRUE;
    }

while (--bytecount >= 0)
    {
    if (!xdrs->x_handy && !inet_read (xdrs))
	return FALSE;
    *buff++ = *xdrs->x_private++;
    --xdrs->x_handy;
    }

return TRUE;
}

static bool_t inet_getlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	i n e t _ g e t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
#ifdef _CRAY
SCHAR	stuff [4];

if (!(*xdrs->x_ops->x_getbytes) (xdrs, stuff, 4))
   return FALSE;

*lp = CTO32L (stuff);
#else

SLONG	l;

if (!(*xdrs->x_ops->x_getbytes) (xdrs, &l, 4))
   return FALSE;

*lp = ntohl (l);
#endif

return TRUE;
}

static u_int inet_getpostn (
    XDR		*xdrs)
{
/**************************************
 *
 *	i n e t _ g e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Get the current position (which is also current length) from stream.
 *
 **************************************/

return (u_int) (xdrs->x_private - xdrs->x_base);
}

#if !(defined NETWARE_386 || defined PC_PLATFORM || \
	defined OS2_ONLY || defined WIN_NT)
static void inet_handler (
    PORT	port)
{
/**************************************
 *
 *	i n e t _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Inet_handler is called the the signal handler on receipt of
 *	a SIGURG signal indicating out-of-band data.  Since SIGURG
 *	may be noisy, check to see if any IO is pending on the channel.
 *	If not, ignore the signal.  If so, call the port specific 
 *	handler to do something appropriate.
 *
 **************************************/
int	n;
UCHAR	junk;

/* If there isn't any out of band data, this signal isn't for us */

if ((n = recv ((SOCKET) port->port_handle, &junk, 1, MSG_OOB)) < 0)
    {

#ifdef NCR3000
    /* this is all being added for ncr's pecular behaviour with
       SIGURG.  As if we get a sigurg and a receive with OOB fails
       we need to do a regular recv call to enable SIGURG
       signals on further OOB data. */
    if (errno == EWOULDBLOCK)
        {
        /* set the flag to non blocking and attempt a normal recv */
        char arg = 1;
        int n_recv;

        if (ioctl (port->port_handle, FIONBIO, &arg) < 0)
            {
                /* donot want to peek if we cannot set the socket to
                   non blocking as then recv() with PEEK waits till
                   data arrives, and we donot want ot block in a
                   signal handler that long. In fact we always expect
                   recv to fail. */

                return;
            }
        /* attempt normal receive this is done inorder to make sigurg
           work correctly on the ncr system. */
        n_recv = recv (port->port_handle, &junk, 1, MSG_PEEK);
        /* set socket to blocking again */
        arg = 0;
        ioctl (port->port_handle, FIONBIO, &arg);
        }
#endif

    return;
    }
(*port->port_ast) (port);
}
#endif

static caddr_t inet_inline (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	i n e t _  i n l i n e
 *
 **************************************
 *
 * Functional description
 *	Return a pointer to somewhere in the buffer.
 *
 **************************************/

if (bytecount > xdrs->x_handy)
    return FALSE;

return xdrs->x_base + bytecount;
}

static int inet_error (
    PORT	port,
    CONST TEXT	*function,
    STATUS	operation,
    int		status)
{
/**************************************
 *
 *	i n e t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	An I/O error has occurred.  Call
 * 	inet_gen_error with the appropriate args
 *	to format the status vector if any.
 *	In any case, return NULL, which
 *	is used to indicate an error.
 *
 **************************************/
TEXT	msg [64];

if (status)
    {
#ifdef VMS
    if ((status >= sys_nerr || !sys_errlist [status]) &&
	status < WIN_NERR && win_errlist [status])
	inet_gen_error (port, isc_network_error, 
	    gds_arg_string, (STATUS) port->port_connection->str_data,
        isc_arg_gds, operation,
	    gds_arg_string, (STATUS) win_errlist [status], 
	    0);
    else
#endif
	inet_gen_error (port, isc_network_error, 
	    gds_arg_string, (STATUS) port->port_connection->str_data,
	    isc_arg_gds, operation, 
	    SYS_ERR, status, 
	    0);

    sprintf (msg, "INET/inet_error: %s errno = %d", function, status);
    gds__log (msg, NULL_PTR);
    }
else
    {
    /* No status value, just format the basic arguments. */

    inet_gen_error (port, isc_network_error, 
	gds_arg_string, (STATUS) port->port_connection->str_data,
	isc_arg_gds, operation, 
	0);
    }

return (int) NULL;
}

static bool_t inet_putbytes (
    XDR		*xdrs,
    SCHAR	*buff,
    u_int	count)
{
/**************************************
 *
 *	i n e t _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Put a bunch of bytes to a memory stream if it fits.
 *
 **************************************/
SLONG	bytecount = count;

/* Use memcpy to optimize bulk transfers. */

while (bytecount > sizeof (GDS_QUAD))
    {
    if (xdrs->x_handy >= bytecount)
	{
	memcpy (xdrs->x_private, buff, bytecount);
	xdrs->x_private += bytecount;
	xdrs->x_handy -= bytecount;
	return TRUE;
	}
    else
	{
	if (xdrs->x_handy > 0)
	    {
	    memcpy (xdrs->x_private, buff, xdrs->x_handy);
	    xdrs->x_private += xdrs->x_handy;
	    buff += xdrs->x_handy;
	    bytecount -= xdrs->x_handy;
	    xdrs->x_handy = 0;
	    }
	if (!inet_write (xdrs, 0))
	    return FALSE;
	}
    }

/* Scalar values and bulk transfer remainder fall thru
   to be moved byte-by-byte to avoid memcpy setup costs. */

if (!bytecount)
    return TRUE;

if (xdrs->x_handy >= bytecount)
    {
    xdrs->x_handy -= bytecount;
    do *xdrs->x_private++ = *buff++; while (--bytecount);
    return TRUE;
    }

while (--bytecount >= 0)
    {
    if (xdrs->x_handy <= 0 && !inet_write (xdrs, 0))
	return FALSE;
    --xdrs->x_handy;
    *xdrs->x_private++ = *buff++;
    }

return TRUE;
}

static bool_t inet_putlong (
    XDR		*xdrs,
    SLONG	*lp)
{
/**************************************
 *
 *	i n e t _ p u t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
SLONG	l;

l = htonl (*lp);
return (*xdrs->x_ops->x_putbytes) (xdrs, AOF32L(l), 4);
}

static bool_t inet_read (
    XDR		*xdrs)
{
/**************************************
 *
 *	i n e t _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a buffer full of data.  If we receive a bad packet,
 *	send the moral equivalent of a NAK and retry.  ACK all
 *	partial packets.  Don't ACK the last packet -- the next
 *	message sent will handle this.
 *
 **************************************/
PORT	port;
SSHORT	length;
SCHAR	*p, *end;

port = (PORT) xdrs->x_public;
p = xdrs->x_base;
end = p + INET_remote_buffer;

/* If buffer is not completely empty, slide down what's left */

if (xdrs->x_handy > 0)
    {
    memmove (p, xdrs->x_private, xdrs->x_handy);
    p += xdrs->x_handy;
    }

/* If an ACK is pending, do an ACK.  The alternative is deadlock. */

/*
if (port->port_flags & PORT_pend_ack)
    if (!packet_send (port, NULL_PTR, 0))
	return FALSE;
*/

while (TRUE)
    {
    length = end - p;
    if (!packet_receive (port, p, length, &length))
	{
	return FALSE;
	/***
	if (!packet_send (port, NULL_PTR, 0))
	    return FALSE;
	continue;
	***/
	}
    if (length >= 0)
	{
	p += length;
	break;
	}
    p -= length;
    if (!packet_send (port, NULL_PTR, 0))
	return FALSE;
    }

port->port_flags |= PORT_pend_ack;
xdrs->x_handy = (int) ((SCHAR*) p - xdrs->x_base);
xdrs->x_private = xdrs->x_base;

return TRUE;
}

static bool_t inet_setpostn (
    XDR		*xdrs,
    u_int	bytecount)
{
/**************************************
 *
 *	i n e t _ s e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Set the current position (which is also current length) from stream.
 *
 **************************************/

if (bytecount > xdrs->x_handy)
    return FALSE;

xdrs->x_private = xdrs->x_base + bytecount;

return TRUE;
}

static PORT inet_try_connect (
    PACKET	*packet,
    RDB		rdb,
    USHORT	file_length,
    TEXT 	*file_name, 
    TEXT	*node_name,
    STATUS	*status_vector,
    SCHAR       *dpb, 
    SSHORT      dpb_length)
{
/**************************************
 *
 *	i n e t _ t r y _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Given a packet with formatted protocol infomation,
 *	set header information and try the connection.
 *
 *	If a connection is established, return a port block, otherwise
 *	return NULL.
 *
 **************************************/
PORT	port;
P_CNCT *cnct;

cnct = &packet->p_cnct;
packet->p_operation = op_connect;
cnct->p_cnct_operation = op_attach;
cnct->p_cnct_cversion = CONNECT_VERSION2;
cnct->p_cnct_client = ARCHITECTURE;
cnct->p_cnct_file.cstr_length = file_length;
cnct->p_cnct_file.cstr_address = (UCHAR*) file_name;

/* If we can't talk to a server, punt.  Let somebody else generate
   an error.  status_vector will have the network error info. */

port = INET_connect (node_name, packet, status_vector, FALSE, dpb, dpb_length);
if (!port)
    {
    ALLR_release ((BLK) rdb);
    return NULL;
    }

/* Get response packet from server. */

rdb->rdb_port = port;
port->port_context = rdb;
if (!RECEIVE (port, packet))
    {
    inet_error (port, "receive in try_connect", isc_net_connect_err, ERRNO);
    disconnect (port);
    ALLR_release ((BLK) rdb);
    return NULL;
    }

return port;
}

static bool_t inet_write (
    XDR		*xdrs,
    bool_t	end_flag)
{
/**************************************
 *
 *	i n e t _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Write a buffer full of data.  If the end_flag isn't set, indicate
 *	that the buffer is a fragment, and reset the XDR for another buffer
 *	load.
 *
 **************************************/
SCHAR    *p;
PORT    port;
SSHORT   l, length;
#ifdef PIGGYBACK
SCHAR    aux_buffer [BUFFER_SIZE];
SSHORT   l2;
#endif

/* Encode the data portion of the packet */

port = (PORT) xdrs->x_public;
p = xdrs->x_base;
length = xdrs->x_private - p;

/* Send data in manageable hunks.  If a packet is partial, indicate
   that with a negative length.  A positive length marks the end. */

p = xdrs->x_base;

while (length)
    {
    port->port_misc1 = (port->port_misc1 + 1) % MAX_SEQUENCE;
    l = MIN (length, INET_max_data);
    length -= l;
    if (!packet_send (port, p, (length) ? -l : l))
	 return FALSE;
    p += l;
    }

xdrs->x_private = xdrs->x_base;
xdrs->x_handy = INET_remote_buffer;

return TRUE;

#ifdef PIGGYBACK
/* If the other end has not piggy-backed the next packet, we're done. */

if (!l2)
    return TRUE;

/* We've got a piggy-backed response.  If the packet is partial,
   send an ACK for part we did receive. */

p = aux_buffer;

while (l2 < 0)
    {
    if (!packet_send (port, NULL_PTR, 0))
	return FALSE;
    p -= l2;
    length = aux_buffer + sizeof (aux_buffer) - p;
    if (!packet_receive (port, p, length, &l2))
	{
	p += l2;
	continue;
	}
    }

length = p - aux_buffer + l2;

/* Now we're got a encode glump ready to stuff into the read buffer.
   Unfortunately, if we just add it to the read buffer, we will shortly
   overflow the buffer.  To avoid this, "scrumpf down" the active bits
   in the read buffer, then add out stuff at the end. */

xdrs = &port->port_receive;
p = xdrs->x_base;

if (xdrs->x_handy && p != xdrs->x_private)
    memmove (p, xdrs->x_private, xdrs->x_handy);

p += xdrs->x_handy;

xdrs->x_private = xdrs->x_base;
/*
xdrs->x_handy += JAP_decode (aux_buffer, length, p);
*/
port->port_flags |= PORT_pend_ack;

return TRUE;
#endif
}

#ifdef DEBUG
static void packet_print (
    TEXT	*string,
    UCHAR	*packet,
    int		length,
    int		counter)
{
/**************************************
 *
 *	p a c k e t _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Print a summary of packet.
 *
 **************************************/
int	sum, l;

sum = 0;
if (l = length)
    do sum += *packet++; while (--l);

ib_fprintf (ib_stdout,"%05d:    PKT %s\t(%4d): length = %4d, checksum = %d\n", 
	inet_debug_timer(),
	string, counter, length, sum);
ib_fflush (ib_stdout);
}
#endif

static int packet_receive (
    PORT	port,
    UCHAR	*buffer,
    SSHORT	buffer_length,
    SSHORT	*length)
{
/**************************************
 *
 *	p a c k e t _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Receive a packet and pass on it's goodness.  If it's good,
 *	return TRUE and the reported length of the packet, and update
 *	the receive sequence number.  If it's bad, return FALSE.  If it's
 *	a duplicate message, just ignore it.
 *
 **************************************/
int		n;
#ifndef REQUESTER
struct timeval	timeout, *time_ptr;
int		ph;
fd_set		slct_fdset;
int		slct_count;
PACKET		packet;

/* set the time interval for sending dummy packets to the client */

timeout.tv_sec = port->port_dummy_packet_interval;
timeout.tv_usec = 0;

if (port->port_protocol >= PROTOCOL_VERSION8 &&
    port->port_dummy_packet_interval > 0)
    time_ptr = &timeout;
else
    time_ptr = NULL;

/* If the protocol is 0 we are still in the process of establishing
   a connection. Add a time out to the wait */
if (port->port_protocol == 0)
   {
   timeout.tv_sec = port->port_connect_timeout;
   time_ptr = &timeout;
   }

ph = (int) port->port_handle;
#endif

#ifdef VMS
if (ISC_tcp_gettype() == TCP_TYPE_MULTINET)
    ph = ph / 16;
#endif

for (;;)
    {
#ifndef REQUESTER

    /* Implement an error-detection protocol to ensure that the client
       is still there.  Use the select() call with a timeout to wait on 
       the connection for an incoming packet.  If none comes within a
       suitable time interval, write a dummy packet on the connection.
       If the client is not there, an error will be returned on the write.
       If the client is there, the dummy packet will be ignored by all 
       InterBase clients V4 or greater.  This protocol will detect when 
       clients are lost abnormally through reboot or network disconnect. */

    /* Don't send op_dummy packets on aux port; the server won't
       read them because it only writes to aux ports. */
    
    if (port->port_flags & PORT_async)
       	;
    else
#ifdef VMS
	if (ISC_tcp_gettype() == TCP_TYPE_MULTINET)
#endif
	{
    	FD_ZERO (&slct_fdset);
    	FD_SET (ph, &slct_fdset);
    
    	THREAD_EXIT;
    	for (;;)
	    {
#if (defined NETWARE_386 || defined WIN_NT)
	    slct_count = select (FD_SETSIZE, &slct_fdset, 
			     (fd_set*) NULL, (fd_set*) NULL, time_ptr);
#else
	    slct_count = select ((SOCKET) port->port_handle + 1, &slct_fdset, 
			     (fd_set*) NULL, (fd_set*) NULL, time_ptr);
#endif
	    if (slct_count != -1 || !INTERRUPT_ERROR(ERRNO))
   	    	break;
	    }
    	THREAD_ENTER;

    	if (slct_count == -1)
	    return inet_error (port, "select in packet_receive",
                isc_net_read_err, ERRNO);

    	if (!slct_count && port->port_protocol >= PROTOCOL_VERSION8)
	    {
#ifdef DEBUG
	    if (INET_trace & TRACE_operations)
		{
		ib_fprintf (ib_stdout,"%05d: OP Sent: op_dummy\n", 
			inet_debug_timer());
		ib_fflush (ib_stdout);
		};
#endif
	    packet.p_operation = op_dummy;
	    if (!send_full (port, &packet))
	       	return FALSE;
	    continue;
	    }
	
    	if (!slct_count && port->port_protocol == 0)
	    return FALSE;
    	}
#endif  /* REQUESTER */

    THREAD_EXIT;
    n = recv ((SOCKET) port->port_handle, buffer, buffer_length, 0);
    THREAD_ENTER;
    if (n != -1 || !INTERRUPT_ERROR(ERRNO))
	break;
    }

if (n <= 0 && (port->port_flags & PORT_async))
    return FALSE;

if (n == -1)
    return inet_error (port, "read", isc_net_read_err, ERRNO);

if (!n)
    return inet_error (port, "read end_of_file", isc_net_read_err, 0);

#ifndef REQUESTER
#ifdef DEBUG
{
INET_count_recv++;
INET_bytes_recv += n;
if (INET_trace & TRACE_packets)
    packet_print ("receive", buffer, n, INET_count_recv);
INET_force_error--;
if (INET_force_error == 0)
    {
    INET_force_error = 1;
    return inet_error (port, "simulated error - read", isc_net_read_err, 0);
    }
}
#endif
#endif

*length = n;

return TRUE;
}

static bool_t packet_send (
    PORT	port,
    SCHAR	*buffer,
    SSHORT	buffer_length)
{
/**************************************
 *
 *	p a c k e t _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Send some data on it's way.  
 *
 **************************************/
SSHORT	n, length, count;
SCHAR	*data;
#ifndef NO_ITIMER
struct itimerval	internal_timer, client_timer;
#ifndef SIGACTION_SUPPORTED
struct sigvec		internal_handler, client_handler;
#else
struct sigaction	internal_handler, client_handler;
#endif
#endif

data = buffer;
length = buffer_length;

while (length)
    {
    THREAD_EXIT;
#ifdef DEBUG
    if (INET_trace & TRACE_operations)
        {
	ib_fprintf (ib_stdout,"Before Send\n");
	ib_fflush (ib_stdout);
	}
#endif
    n = -1;
    n = send ((SOCKET) port->port_handle, data, length, 0);
#ifdef DEBUG
    if (INET_trace & TRACE_operations)
        {
	ib_fprintf (ib_stdout,"After Send n is %d\n",n);
	ib_fflush (ib_stdout);
	}
#endif
    THREAD_ENTER;
    if (n == length)
	break;
    else if (n == -1)
	{
	if (INTERRUPT_ERROR(ERRNO))
	    continue;
	else
	    return inet_error (port, "send", isc_net_write_err, ERRNO);
	}
    data += n;
    length -= n;
    }

if ((port->port_flags & PORT_async) && !(port->port_flags & PORT_no_oob))
    {
    THREAD_EXIT;
    count = 0;
    while ((n = send ((SOCKET) port->port_handle, buffer, 1, MSG_OOB)) == -1 &&
	(ERRNO == ENOBUFS || INTERRUPT_ERROR(ERRNO)))
	{
	if (count++ > 20)
	    break;

#ifdef NO_ITIMER
#ifdef WIN_NT
	SleepEx (50, TRUE);
#else
#ifdef OS2_ONLY
	DosSleep (50);
#else
#ifndef WINDOWS_ONLY
	sleep (1);
#endif
#endif
#endif
	}
#else
	if (count == 1)
	    {
	    /* Wait in a loop until the lock becomes available */

	    internal_timer.it_interval.tv_sec = 0;
	    internal_timer.it_interval.tv_usec = 0;
	    internal_timer.it_value.tv_sec = 0;
	    internal_timer.it_value.tv_usec = 0;
	    setitimer (ITIMER_REAL, &internal_timer, &client_timer);
#ifndef SIGACTION_SUPPORTED
	    internal_handler.sv_handler = alarm_handler;
	    internal_handler.sv_mask = 0;
	    internal_handler.sv_flags = SV_INTERRUPT;
	    sigvector (SIGALRM, &internal_handler, &client_handler);
#else
	    internal_handler.sa_handler = alarm_handler;
	    memset (&internal_handler.sa_mask, 0, sizeof (internal_handler.sa_mask));
	    internal_handler.sa_flags = SA_RESTART;
	    sigaction (SIGALRM, &internal_handler, &client_handler);
#endif
	    }

	internal_timer.it_value.tv_sec = 0;
	internal_timer.it_value.tv_usec = 50000;
	setitimer (ITIMER_REAL, &internal_timer, NULL);
	pause();
	}

    if (count)
	{
	/* Restore user's outstanding alarm request and handler */

	internal_timer.it_value.tv_sec = 0;
	internal_timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &internal_timer, NULL);
#ifndef SIGACTION_SUPPORTED
	sigvector (SIGALRM, &client_handler, NULL);
#else
	sigaction (SIGALRM, &client_handler, NULL);
#endif
	setitimer (ITIMER_REAL, &client_timer, NULL);
	}
#endif

    THREAD_ENTER;
    if (n == -1)
	return inet_error (port, "send/oob", isc_net_write_err, ERRNO);
    }

#ifndef REQUESTER
#ifdef DEBUG
{
INET_count_send++;
INET_bytes_send += buffer_length;
if (INET_trace & TRACE_packets)
    packet_print ("send", buffer, buffer_length, INET_count_send);
INET_force_error--;
if (INET_force_error == 0)
    {
    INET_force_error = 1;
    return inet_error (port, "simulated error - send", isc_net_write_err, 0);
    }
}
#endif
#endif

port->port_flags &= ~PORT_pend_ack;

return TRUE;
}

#ifdef WINDOWS_ONLY
static int FAR PASCAL blocking_stub (void)
{
/**************************************
 *
 *	b l o c k i n g _ s t u b
 *
 **************************************
 *
 * Functional description:
 *	Defines an empty blocking stub so that by default we do not
 *	yield the CPU when winsock is waiting on network I/O.
 *	The user must register a blocking function with our DLL
 *	before we will allow any yields (protects ODAPI from re-entrancy).
 *
 **************************************/

return FALSE;
}

static void cleanup_library (
    void	*arg)
{
/**************************************
 *
 *	c l e a n u p _ l i b r a r y
 *
 **************************************
 *
 * Functional description:
 *	If an arg is passed in, it is the handle to a dynamically loaded
 *	library, so free it.  Otherwise, it is final cleanup so
 * 	call WSACleanup for winsock cleanup before exiting.
 *
 **************************************/

if (arg != NULL)
    FreeLibrary ((HINSTANCE) arg);
else
    WSACleanup();
}

LRESULT FAR PASCAL inet_wndproc (
    HWND	hwnd,
    UINT	message,
    WPARAM	wParam,
    LPARAM	lParam)
{
/**************************************
 *
 *	i n e t _ w n d p r o c
 *
 **************************************
 *
 * Functional description:
 *	This is the windows equivalent of inet_handler.
 *	When an async request completes, a message will be posted
 *	to this window.  The function then calls the ast function
 *	in the port structure (saved in the window extra bytes)
 *	and re-initiates the async listen on the port.
 *
 **************************************/
PORT	port;
SCHAR	msgbuf [200];

switch (message)
    {
    case WM_USER + 1:
	/* Check the lParam for any errors on the AsyncSelect call */

	if (WSAGETSELECTERROR (lParam))
	    {
	    sprintf(msgbuf, "Select returned %d", WSAGETSELECTERROR (lParam));
	    MessageBox(NULL, msgbuf, "EVENT", MB_ICONINFORMATION | MB_OK);

	    /* ***CM Add some error handling here */
	    }
	else
	    {
	    /* Call the ast function stored in the port structure */

	    port = (PORT) GetWindowLong (hwnd, 0);
	    (port->port_ast) (port);

	    /* Re-initiate the async listen on this socket */

	    if (WSAAsyncSelect ((SOCKET) wParam, hwnd, WM_USER+1, FD_READ) ==
		    SOCKET_ERROR)
		{
		/* ***CM Add some logging here */
		}
	    }

	/* Tell Windows it does not need to process the message */

	return FALSE;
    }

return DefWindowProc (hwnd, message, wParam, lParam);
}

#define GET_WIN_ENTRYPOINT(funcp,funcname) \
	funcp = GetProcAddress (hWinsockDLL, funcname); \
	if (funcp == NULL) { inet_gen_error (port, \
					     isc_network_error, \
					     isc_arg_string, \
					     port->port_host->str_data, \
					     isc_arg_gds, \
					     isc_net_init_error, \
					     isc_arg_gds, \
					     isc_loadlib_failure, \
					     isc_arg_string, \
					     "winsock.dll", \
					     0); \
			     FreeLibrary (hWinsockDLL); \
			     hWinsockDLL = 0; \
			     return FAILURE; }

static int initWSA (PORT port)
{
/**************************************
 *
 *	i n i t W S A
 *
 **************************************
 *
 * Functional description:
 *	Initializes the winsock library.
 *
 **************************************/
struct WSAData	W;
int		i, ret;
HTASK		hTask;

extern HINSTANCE      LoadDll (UCHAR *);
extern TaskInfoType   Tasks[MAX_TASKS];
extern HINSTANCE      hWinsockDLL;

#if defined(__WIN32__)
#define GetCurrentTask GetCurrentProcess
#endif

if (!INET_initialized)
    {
    hWinsockDLL = LoadDll ("winsock.dll");

    if (hWinsockDLL)
	{
	GET_WIN_ENTRYPOINT (WSAStartupProcPtr, "WSAStartup");
	GET_WIN_ENTRYPOINT (acceptProcPtr, "accept");
	GET_WIN_ENTRYPOINT (bindProcPtr, "bind");
	GET_WIN_ENTRYPOINT (closesocketProcPtr, "closesocket");
	GET_WIN_ENTRYPOINT (connectProcPtr, "connect");
	GET_WIN_ENTRYPOINT (gethostbynameProcPtr, "gethostbyname");
	GET_WIN_ENTRYPOINT (gethostnameProcPtr, "gethostname");
	GET_WIN_ENTRYPOINT (getsocknameProcPtr, "getsockname");
	GET_WIN_ENTRYPOINT (getservbynameProcPtr, "getservbyname");
	GET_WIN_ENTRYPOINT (htonlProcPtr, "htonl");
	GET_WIN_ENTRYPOINT (listenProcPtr, "listen");
	GET_WIN_ENTRYPOINT (ntohlProcPtr, "ntohl");
	GET_WIN_ENTRYPOINT (recvProcPtr, "recv");
	GET_WIN_ENTRYPOINT (selectProcPtr, "select");
	GET_WIN_ENTRYPOINT (sendProcPtr, "send");
	GET_WIN_ENTRYPOINT (setsockoptProcPtr, "setsockopt");
	GET_WIN_ENTRYPOINT (shutdownProcPtr, "shutdown");
	GET_WIN_ENTRYPOINT (socketProcPtr, "socket");
	GET_WIN_ENTRYPOINT (WSAAsyncSelectProcPtr, "WSAAsyncSelect");
	GET_WIN_ENTRYPOINT (WSACleanupProcPtr, "WSACleanup");
	GET_WIN_ENTRYPOINT (WSAFDIsSetProcPtr, "WSAGetLastError");
	GET_WIN_ENTRYPOINT (WSAGetLastErrorProcPtr, "WSAGetLastError");
	GET_WIN_ENTRYPOINT (WSASetBlockingHookProcPtr, "WSASetBlockingHook");
        }
    else
        {
        /* There was error, so return it */
        inet_gen_error (port,
                        isc_network_error,
	                isc_arg_string,
                        port->port_host->str_data, 
			isc_arg_gds,
                        isc_net_init_error, 
			isc_arg_gds,
                        isc_loadlib_failure,
			isc_arg_string,
                        "winsock.dll",
                        0);
        return FAILURE;
        }
    INET_initialized = TRUE;
    }

hTask  = GetCurrentTask();

for (i = 0; i < MAX_TASKS && Tasks[i].hId != 0; i++)
    if (Tasks[i].hId == hTask)
	/* If this task has already come in before, don't call startup again */
    	{
	Tasks[i].nUseCount++;	
	return 0;
	}

/* Call WSA Startup with version highword=1, low word=1, i.e. ver 1.1 */

ret = WSAStartup (((1 << 8) | 1), &W);
if (ret)
    { 
    inet_error (port, "WSAStartup", isc_net_init_error, ret);
    ret = FAILURE;
    }
else
    {
#ifndef HSNOBLOCKING
    if (WSASetBlockingHook (blocking_stub) == NULL)
    	{
	WSACleanup ();
    	inet_error (port, 
		    "WSASetBlockingHook", 
		    isc_net_init_error,
                    WSAGetLastError());
    	ret = FAILURE;
    	}
#endif
    }

if (ret == SUCCESS)
    {
    Tasks[i].hId = hTask;
    Tasks[i].nUseCount = 1;	
    }

return ret;
}
#endif /* WINDOWS_ONLY */

static void unhook_port (
    PORT        port,
    PORT        parent)
{
/**************************************
 *
 *      u n h o o k _ p a r e n t
 *
 **************************************
 *
 * Functional description
 *      Disconnect a port from its parent
 *      This must be done under
 *      START_PORT_CRITICAL/STOP_PORT_CRITICAL control.
 *
 **************************************/
PORT    *ptr;

for (ptr = &parent->port_clients; *ptr; ptr = &(*ptr)->port_next)
    if (*ptr == port)
        {
        *ptr = port->port_next;
        if (ptr == &parent->port_clients)
            parent->port_next = *ptr;
        break;
        }
}

#ifdef  SUPERSERVER
static void unhook_disconnected_ports (
    PORT        main_port)
{
/**************************************
 *
 *      u n h o o k _ d i s c o n n e c t e d _ p o r t s
 *
 **************************************
 *
 * Functional description
 *      Go through a list of ports and get rid of any
 *      that are disconnected.   Note that thhis must be
 *      done under START_PORT_CRITICAL/STOP_PORT_CRITICAL
 *      control.
 *
 **************************************/
PORT	port;
int	n;

n = 1;
while (n)
    {
    n = 0;
    for (port = main_port; port; port = port->port_next)
        if (port->port_state == state_disconnected)
            {
            n = 1;
            unhook_port (port, port->port_parent);
            cleanup_port (port);
            break;
            }
    }
}
#endif
