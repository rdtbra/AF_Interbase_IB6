/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		svc.c
 *	DESCRIPTION:	Service manager functions
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
#include <string.h>
#include "../jrd/ibsetjmp.h"
#include "../jrd/time.h"
#include "../jrd/common.h"
#include "../jrd/file_params.h"
#include <stdarg.h>
#include "../jrd/jrd.h"
#include "../jrd/svc.h"
#include "../jrd/pwd.h"
#include "../alice/aliceswi.h"
#include "../burp/burpswi.h"
#include "../jrd/ibase.h"
#include "../jrd/license.h"
#include "../jrd/err_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pwd_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/svc_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/why_proto.h"
#include "../jrd/utl_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/enc_proto.h"
#include "../utilities/gsecswi.h"
#include "../utilities/dbaswi.h"
#ifdef SERVER_SHUTDOWN
#include "../jrd/jrd_proto.h"
#endif

#ifdef sparc
#ifdef SOLARIS
#include <fcntl.h>
#else
#include <vfork.h>
#endif
#endif
#if (defined DELTA || defined IMP)
#include <fcntl.h>
#include <sys/types.h>
#endif
#if (defined EPSON || defined SCO_UNIX)
#include <fcntl.h>
#endif

/* This is defined in JRD/SCL.H, but including it causes
 * a linker warning.  
 */
#define SYSDBA_USER_NAME	"SYSDBA"
#define SVC_user_dba		2
#define SVC_user_any		1
#define SVC_user_none		0

#if !(defined WIN_NT || defined OS2_ONLY || defined WINDOWS_ONLY)
#include <signal.h>
#ifndef VMS
#include <sys/param.h>
#include <sys/stat.h>
#else
#include <stat.h>
#endif
#include <errno.h>
#endif

#ifdef WIN_NT
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#define SYS_ERR		isc_arg_win32
#endif

#ifdef WINDOWS_ONLY
#define SYS_ERR		isc_arg_dos
#endif

#ifdef OS2_ONLY
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#define INCL_DOSQUEUES
#define INCL_DOSERRORS
#include <os2.h>
#define SYS_ERR		isc_arg_dos
#endif

#ifdef NETWARE_386
#define SYS_ERR		isc_arg_netware
#endif

#ifndef SYS_ERR
#define SYS_ERR		isc_arg_unix
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	256
#endif

#if (defined DELTA || defined VMS || defined NeXT || defined IMP ) 
#define waitpid(x,y,z)	wait (y)
#endif

#define statistics	stat

#define GET_LINE	1
#define GET_EOF		2
#define GET_BINARY	4

#define	SVC_TRMNTR	'\377'

/* This checks if the service has forked a process.  If not,
   it will post the isc_svcnoexe error. */

#define IS_SERVICE_RUNNING(service) \
				if (!(service->svc_flags & SVC_forked)) {\
				    THREAD_ENTER; \
				    ERR_post (isc_svcnoexe, isc_arg_string, \
				    service->svc_service->serv_name, 0); } 

#define NEED_ADMIN_PRIVS(svc)	{*status++ = isc_insufficient_svc_privileges; \
		      	         *status++ = isc_arg_string; \
				 *status++ = (STATUS) ERR_string(svc,strlen(svc)); \
				 *status++ = isc_arg_end; }

#define ERR_FILE_IN_USE		{ TEXT buffer[256]; \
		                  gds__prefix (buffer, LOCK_HEADER); \
 		                  *status++ = isc_file_in_use; \
		                  *status++ = isc_arg_string; \
		                  *status++ = (STATUS) ERR_string (buffer, strlen(buffer)); \
                                  *status++ = isc_arg_end; }				

/* Option block for service parameter block */

typedef struct spb {
    TEXT	*spb_sys_user_name;
    TEXT	*spb_user_name;
    TEXT	*spb_password;
    TEXT	*spb_password_enc;
    TEXT	*spb_command_line;
    USHORT  	 spb_version;
} SPB;

static void	conv_switches (USHORT, USHORT, SCHAR *, TEXT **);
static TEXT	*find_switch (int, IN_SW_TAB);
static USHORT	process_switches (USHORT, SCHAR *, TEXT *);
static void	get_options (UCHAR *, USHORT, TEXT *, SPB *);
static TEXT	*get_string_parameter (UCHAR **, TEXT **);
static void	io_error (TEXT *, SLONG, TEXT *, STATUS, BOOLEAN);
static void	service_close (SVC);
static BOOLEAN	get_action_svc_bitmask (TEXT **, IN_SW_TAB, TEXT **, USHORT *, USHORT *);
static void	get_action_svc_string (TEXT **, TEXT **, USHORT *, USHORT *);
static void	get_action_svc_data (TEXT **, TEXT **, USHORT *, USHORT *);
static BOOLEAN	get_action_svc_parameter (TEXT **, IN_SW_TAB, TEXT **, USHORT *, USHORT *);

#ifdef SUPERSERVER
static UCHAR	service_dequeue_byte (SVC);
static void	service_enqueue_byte (UCHAR, SVC);
static USHORT service_add_one (USHORT i);
static USHORT service_empty (SVC service);
static USHORT service_full (SVC service);
static void	service_fork (void (*)(), SVC);
#else
static void	service_fork (TEXT *, SVC);
#endif
static void	service_get (SVC, SCHAR *, USHORT, USHORT, USHORT, USHORT *);
static void	service_put (SVC, SCHAR *, USHORT);
static void	timeout_handler (SVC);
#ifdef WIN_NT
static USHORT   service_read (SVC, SCHAR *, USHORT, USHORT);
#endif

#ifdef DEBUG
void test_thread (SVC);
void test_cmd (USHORT, SCHAR *, TEXT **);
#define TEST_THREAD test_thread
#define TEST_CMD test_cmd
#else
#define TEST_THREAD NULL
#define TEST_CMD NULL
#endif

#ifdef  __BORLANDC__
static void     (__stdcall *shutdown_fct)(UINT) = NULL;
#else
static void     (*shutdown_fct)() = NULL;
#endif
static ULONG    shutdown_param = 0L;

#ifdef WIN_NT
static SLONG    SVC_cache_default;
static SLONG    SVC_priority_class;
static SLONG    SVC_client_map;
static SLONG    SVC_working_set_min;
static SLONG    SVC_working_set_max;

static struct ipccfg	SVC_hdrtbl [] = {
	ISCCFG_DBCACHE,		0, &SVC_cache_default,		0,	0,
	ISCCFG_PRIORITY,	0, &SVC_priority_class,		0,	0,
	ISCCFG_IPCMAP,		0, &SVC_client_map,		0,	0,
	ISCCFG_MEMMIN,		0, &SVC_working_set_min,	0,	0,
	ISCCFG_MEMMAX,		0, &SVC_working_set_max,	0,	0,
	NULL,			0, NULL,			0,	0
};
#else
static SLONG	SVC_conn_timeout;
static SLONG	SVC_dbcache;
static SLONG	SVC_deadlock;
static SLONG	SVC_dummy_intrvl;
static SLONG	SVC_lockspin;
static SLONG	SVC_lockhash;
static SLONG	SVC_evntmem;
static SLONG	SVC_lockorder;
static SLONG	SVC_anylockmem;
static SLONG	SVC_locksem;
static SLONG	SVC_locksig;

static struct ipccfg    SVC_hdrtbl [] = {
	ISCCFG_CONN_TIMEOUT,	0, &SVC_conn_timeout,		0,	0,
	ISCCFG_DBCACHE,		0, &SVC_dbcache,		0,	0,
	ISCCFG_DEADLOCK,	0, &SVC_deadlock,		0,	0,
	ISCCFG_DUMMY_INTRVL,	0, &SVC_dummy_intrvl,		0,	0,
	ISCCFG_LOCKSPIN,	0, &SVC_lockspin,		0,	0,
	ISCCFG_LOCKHASH,	0, &SVC_lockhash,		0,	0,
	ISCCFG_EVNTMEM,		0, &SVC_evntmem,		0,	0,
	ISCCFG_LOCKORDER,	0, &SVC_lockorder,		0,	0,
	ISCCFG_ANYLOCKMEM,	0, &SVC_anylockmem,		0,	0,
	ISCCFG_ANYLOCKSEM,	0, &SVC_locksem,		0,	0,
	ISCCFG_ANYLOCKSIG,	0, &SVC_locksig,		0,	0,
	NULL,			0, NULL,			0,	0
};
#endif	/* WIN_NT */

#define SPB_SEC_USERNAME	"isc_spb_sec_username"
#define SPB_LIC_KEY		"isc_spb_lic_key"
#define SPB_LIC_ID		"isc_spb_lic_id"

static MUTX_T	svc_mutex [1], thd_mutex [1];
static BOOLEAN  svc_initialized = FALSE, thd_initialized = FALSE;

/* Service Functions */
#ifdef SUPERSERVER
extern          main_gbak();
extern		main_gfix();
extern          main_wal_print();
extern          main_lock_print();
extern          main_gstat();
extern		main_gsec();

#define MAIN_GBAK       main_gbak
#define MAIN_GFIX	main_gfix
#define MAIN_WAL_PRINT  main_wal_print
#define MAIN_LOCK_PRINT main_lock_print
#define MAIN_GSTAT      main_gstat
#define MAIN_GSEC	main_gsec
#else
#define MAIN_GBAK       NULL
#define MAIN_GFIX	NULL
#define MAIN_WAL_PRINT  NULL
#define MAIN_LOCK_PRINT NULL
#define MAIN_GSTAT      NULL
#define MAIN_GSEC	NULL
#endif

/* Entries which have a NULL serv_executable field will not fork
   a process on the server, but will establish a valid connection
   which can be used for isc_info_svc calls.

  The format of the structure is:

  isc_action_svc call,
  old service name (for compatibility),
  old cmd-line switches (for compatibility),
  executable to fork (for compatibility),
  thread to execute,
  in use flag (for compatibility)
*/

static CONST struct serv	services [] = {
#ifndef LINUX
#ifndef NETWARE386
#ifdef WIN_NT
    isc_action_max, "print_cache",      "-svc",             "bin/ibcachpr",          NULL,             0,
    isc_action_max, "print_locks",      "-svc",             "bin/iblockpr",          NULL,             0,
    isc_action_max, "start_cache",      "-svc",             "bin/ibcache",           NULL,             0,
#else
    isc_action_max, "print_cache",      "-svc",             "bin/gds_cache_print",   NULL,             0,
    isc_action_max, "print_locks",      "-svc",             "bin/gds_lock_print",    NULL,             0,
    isc_action_max, "start_cache",      "-svc",             "bin/gds_cache_manager", NULL,             0,
#endif /* WIN_NT */
    isc_action_max, "analyze_database", "-svc",             "bin/gstat",             NULL,             0,
    isc_action_max, "backup",           "-svc -b",          "bin/gbak",              MAIN_GBAK,        0,
    isc_action_max, "create",           "-svc -c",          "bin/gbak",              MAIN_GBAK,        0,
    isc_action_max, "restore",          "-svc -r",          "bin/gbak",              MAIN_GBAK,        0,
    isc_action_max, "gdef",             "-svc",             "bin/gdef",              NULL,             0,
    isc_action_max, "gsec",             "-svc",             "bin/gsec",              NULL,             0,
    isc_action_max, "disable_journal",  "-svc -disable",    "bin/gjrn",              NULL,             0,
    isc_action_max, "dump_journal",     "-svc -online_dump","bin/gjrn",              NULL,             0,
    isc_action_max, "enable_journal",   "-svc -enable",     "bin/gjrn",              NULL,             0,
    isc_action_max, "monitor_journal",  "-svc -console",    "bin/gjrn",              NULL,             0,
    isc_action_max, "query_server",     NULL,               NULL,                    NULL,             0,
    isc_action_max, "start_journal",    "-svc -server",     "bin/gjrn",              NULL,             0,
    isc_action_max, "stop_cache",       "-svc -shut -cache","bin/gfix",              NULL,             0,
    isc_action_max, "stop_journal",     "-svc -console",    "bin/gjrn",              NULL,             0,
#else
    isc_action_max, "analyze_database", "-svc",             NULL,                    MAIN_GSTAT,       0,
    isc_action_max, "backup",           "-svc -b",          NULL,                    MAIN_GSTAT,       0,
    isc_action_max, "create",           "-svc -c",          NULL,                    MAIN_GBAK,        0,
    isc_action_max, "restore",          "-svc -r",          NULL,                    MAIN_GBAK,        0,
    isc_action_max, "gdef",             "-svc",             NULL,                    NULL,             0,
    isc_action_max, "gsec",             "-svc",             NULL,                    NULL,             0,
    isc_action_max, "print_wal",        "-svc",             NULL,                    MAIN_WAL_PRINT,   0,
    isc_action_max, "print_locks",      "-svc",             NULL,                    MAIN_LOCK_PRINT,  0,
#endif /* NETWARE */
    isc_action_max, "anonymous",        NULL,		    NULL,                    NULL,             0,

/* NEW VERSION 2 calls, the name field MUST be different from those names above
 */
    isc_action_max,                "service_mgr",             NULL, NULL,        NULL,                 0,
    isc_action_svc_backup,         "Backup Database",         NULL, "bin/gbak",  MAIN_GBAK,            0,
    isc_action_svc_restore,        "Restore Database",        NULL, "bin/gbak",  MAIN_GBAK,            0,
    isc_action_svc_repair,         "Repair Database",         NULL, "bin/gfix",  MAIN_GFIX,            0,
    isc_action_svc_add_user,       "Add User",                NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_delete_user,    "Delete User",             NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_modify_user,    "Modify User",             NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_display_user,   "Display User",	      NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_properties,     "Database Properties",     NULL, "bin/gfix",  MAIN_GFIX,            0,
    isc_action_svc_lock_stats,     "Lock Stats",              NULL, NULL,        TEST_THREAD,          0,
    isc_action_svc_db_stats,	   "Database Stats",	      NULL, NULL,	 MAIN_GSTAT,	       0,
    isc_action_svc_get_ib_log,	   "Get Log File",	      NULL, NULL,	 SVC_read_ib_log,      0,
/* actions with no names are undocumented */
    isc_action_svc_set_config,     NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_default_config, NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env,        NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env_lock,   NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env_msg,    NULL, NULL, NULL, TEST_THREAD,  0,
    0,                             NULL, NULL, NULL, NULL,         0
};
#else	/* LINUX: disallow services API for 6.0 Linux Classic */
    isc_action_max, "anonymous",   NULL, NULL, NULL,  0,
#ifdef SUPERSERVER
    isc_action_max, "query_server", NULL, NULL, NULL, 0,
    isc_action_max,                "service_mgr",             NULL, NULL,        NULL,                 0,
    isc_action_svc_backup,         "Backup Database",         NULL, "bin/gbak",  MAIN_GBAK,            0,
    isc_action_svc_restore,        "Restore Database",        NULL, "bin/gbak",  MAIN_GBAK,            0,
    isc_action_svc_repair,         "Repair Database",         NULL, "bin/gfix",  MAIN_GFIX,            0,
    isc_action_svc_add_user,       "Add User",                NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_delete_user,    "Delete User",             NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_modify_user,    "Modify User",             NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_display_user,   "Display User",	      NULL, "bin/gsec",  MAIN_GSEC,            0,
    isc_action_svc_properties,     "Database Properties",     NULL, "bin/gfix",  MAIN_GFIX,            0,
    isc_action_svc_lock_stats,     "Lock Stats",              NULL, NULL,        TEST_THREAD,          0,
    isc_action_svc_db_stats,	   "Database Stats",	      NULL, NULL,	 MAIN_GSTAT,	       0,
    isc_action_svc_get_ib_log,	   "Get Log File",	      NULL, NULL,	 SVC_read_ib_log,      0,
/* actions with no names are undocumented */
    isc_action_svc_set_config,     NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_default_config, NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env,        NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env_lock,   NULL, NULL, NULL, TEST_THREAD,  0,
    isc_action_svc_set_env_msg,    NULL, NULL, NULL, TEST_THREAD,  0,
#endif
    0, NULL, NULL, NULL, NULL, 0};
#endif	/* LINUX */
   
/* The SERVER_CAPABILITIES_FLAG is used to mark architectural
** differences across servers.  This allows applications like server
** manager to disable features as necessary.  
*/

#ifdef SUPERSERVER
#define SERVER_CAPABILITIES     	REMOTE_HOP_SUPPORT | \
                                    MULTI_CLIENT_SUPPORT | \
			                		SERVER_CONFIG_SUPPORT
#endif /* SUPERSERVER */

#ifdef WINDOWS_ONLY
#define SERVICES_DEFINED
#define SERVER_CAPABILITIES_FLAG	MULTI_CLIENT_SUPPORT	| \
					REMOTE_HOP_SUPPORT	| \
					NO_SVR_STATS_SUPPORT	| \
					NO_DB_STATS_SUPPORT	| \
					LOCAL_ENGINE_SUPPORT	| \
					NO_SHUTDOWN_SUPPORT	| \
					NO_SERVER_SHUTDOWN_SUPPORT | \
					NO_FORCED_WRITE_SUPPORT

static CONST struct serv	services [] = {
    0, "query_server", NULL, NULL, NULL, NULL, 0,
    0, NULL,           NULL, NULL, NULL, NULL, 0
};
#else

#ifndef SERVER_CAPABILITIES
#define SERVER_CAPABILITIES_FLAG	REMOTE_HOP_SUPPORT | NO_SERVER_SHUTDOWN_SUPPORT 
#else

#ifdef WIN_NT
#define SERVER_CAPABILITIES_FLAG    SERVER_CAPABILITIES | QUOTED_FILENAME_SUPPORT
#else

#ifdef NETWARE386
#define SERVER_CAPABILITIES_FLAG    SERVER_CAPABILITIES | WAL_SUPPORT | NO_SERVER_SHUTDOWN_SUPPORT
#endif /* NETWARE386 */

#define SERVER_CAPABILITIES_FLAG    SERVER_CAPABILITIES | NO_SERVER_SHUTDOWN_SUPPORT
#endif /* WIN_NT */

#endif /* SERVER_CAPABILITIES */

#endif /* WINDOWS_ONLY */

#ifdef SHLIB_DEFS
#define pipe		(*_libgds_pipe)
#ifdef IMP
#define wait		(*_libgds_wait)
#else
#define waitpid		(*_libgds_waitpid)
#endif
#define _exit		(*_libgds__exit)
#define dup		(*_libgds_dup)
#define ib_fdopen		(*_libgds_fdopen)
#define execvp		(*_libgds_execvp)
#define statistics	(*_libgds_stat)

extern int		pipe();
#ifdef IMP
extern pid_t		wait();
#else
extern pid_t		waitpid();
#endif
extern void		_exit();
extern int		dup();
extern IB_FILE		*ib_fdopen();
extern int		execvp();
extern int		statistics();
#endif

SVC SVC_attach (
    USHORT	service_length,
    TEXT	*service_name,
    USHORT	spb_length,
    SCHAR	*spb)
{
/**************************************
 *
 *	S V C _ a t t a c h
 *
 **************************************
 *
 * Functional description
 *	Connect to an Interbase service.
 *
 **************************************/
TDBB	tdbb;
CONST   struct serv *serv;
TEXT	misc_buf [512];
TEXT	*switches, *misc;
#ifndef SUPERSERVER
TEXT	service_path [MAXPATHLEN];
#endif
SPB	options;
TEXT	name [129], project [33], organization [33];
int	id, group, node_id;
USHORT	len, user_flag;
SVC	service;
JMP_BUF	env, *old_env;

/* If the service name begins with a slash, ignore it. */

if (*service_name == '/' || *service_name == '\\')
    {
    service_name++;
    if (service_length)
	service_length--;
    }
if (service_length)
    {
    strncpy (misc_buf, service_name, (int) service_length);
    misc_buf [service_length] = 0;
    }
else
    strcpy (misc_buf, service_name);

/* Find the service by looking for an exact match. */

for (serv = services; serv->serv_name; serv++)
    if (!strcmp (misc_buf, serv->serv_name))
	break;

if (!serv->serv_name)
#if (defined LINUX && !defined SUPERSERVER)
    ERR_post (isc_service_att_err, isc_arg_gds, isc_service_not_supported, 0);
#else
    ERR_post (isc_service_att_err, isc_arg_gds, isc_svcnotdef, isc_arg_string,
	      SVC_err_string (misc_buf, strlen (misc_buf)), 0);
#endif

tdbb = GET_THREAD_DATA;

/* If anything goes wrong, we want to be able to free any memory
   that may have been allocated. */

misc = switches = NULL;
service = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if ((misc != NULL) && (misc != misc_buf))
	gds__free ((SLONG*)misc);
    if ((switches != NULL))
	gds__free ((SLONG*)switches);
    if ((service != NULL) && (service->svc_status != NULL))
	gds__free ((SLONG*)service->svc_status);
    if (service != NULL)
	gds__free ((SLONG*)service);
    ERR_punt();
    }

/* Process the service parameter block. */

if (spb_length > sizeof (misc_buf))
    misc = (TEXT*) gds__alloc ((SLONG) spb_length);
else
    misc = misc_buf;
/* FREE: by SETJMP handler */
if (!misc)	/* NOMEM: */
    ERR_post (isc_virmemexh, 0);
get_options (spb, spb_length, misc, &options);

/* Perhaps checkout the user in the security database. */

if (!strcmp(serv->serv_name, "anonymous"))
    user_flag = SVC_user_none;
else
    {
    if (!options.spb_user_name)
	ERR_post (isc_service_att_err, isc_arg_gds,
		  isc_svcnouser, 0); /* user name and password are required
					while attaching to the services
					manager */
#ifdef APOLLO
    if (options.spb_user_name || !project [0])
#else
    if (options.spb_user_name || id == -1)
#endif
	PWD_verify_user (name, options.spb_user_name, options.spb_password,
		options.spb_password_enc, &id, &group, &node_id);

/* Check that the validated user has the authority to access this service */

    if (STRICMP(options.spb_user_name, SYSDBA_USER_NAME))
	user_flag = SVC_user_any;
    else
	user_flag = SVC_user_dba | SVC_user_any;
    }

/* Allocate a buffer for the service switches, if any.  Then move them in. */

len = ((serv->serv_std_switches) ? strlen (serv->serv_std_switches) : 0) +
    ((options.spb_command_line) ? strlen (options.spb_command_line) : 0) + 1;

if (len > 1)
    if ( (switches = (TEXT*) gds__alloc ((SLONG) len)) == NULL)
	/* FREE: by SETJMP handler */
	ERR_post (isc_virmemexh, 0);

if (switches)
    *switches = 0;
if (serv->serv_std_switches)
    strcpy (switches, serv->serv_std_switches);
if (options.spb_command_line && serv->serv_std_switches)
    strcat (switches, " ");
if (options.spb_command_line)
    strcat (switches, options.spb_command_line);

/* Services operate outside of the context of databases.  Therefore
   we cannot use the JRD allocator. */

service = (SVC) gds__alloc ((SLONG) (sizeof (struct svc)));
/* FREE: by SETJMP handler */
if (!service)
    ERR_post (isc_virmemexh, 0);

memset((void *)service, 0, sizeof(struct svc));

service->svc_status = (STATUS) gds__alloc (ISC_STATUS_LENGTH * sizeof(STATUS));
/* FREE: by setjmp handler */
if (!service->svc_status)
    ERR_post (isc_virmemexh, 0);

memset ((void*)service->svc_status, 0, ISC_STATUS_LENGTH * sizeof(STATUS));

service->svc_header.blk_type = type_svc;
service->svc_header.blk_pool_id_mod = 0;
service->svc_header.blk_length = 0;
service->svc_service = serv;
service->svc_resp_buf = service->svc_resp_ptr = NULL;
service->svc_resp_buf_len = service->svc_resp_len = 0;
service->svc_flags = serv->serv_executable ? SVC_forked : 0;
service->svc_switches = switches;
service->svc_handle = 0;
service->svc_user_flag = user_flag;
service->svc_do_shutdown = FALSE;
#ifdef SUPERSERVER
service->svc_stdout_head = 1;
service->svc_stdout_tail = SVC_STDOUT_BUFFER_SIZE;
service->svc_stdout = NULL;
service->svc_argv = NULL;
#endif
service->svc_spb_version = options.spb_version;
if (options.spb_user_name)
    strcpy (service->svc_username, options.spb_user_name);

/* The password will be issued to the service threads on NT since 
 * there is no OS authentication.  If the password is not yet 
 * encrypted, then encrypt it before saving it (since there is no
 * decrypt function).
 */
if (options.spb_password)
    {
    UCHAR	*p, *s = service->svc_enc_password;
    int     l;

    p = (UCHAR *) ENC_crypt (options.spb_password, PASSWORD_SALT) + 2;
    l = strlen ((char*)p);
    MOVE_FASTER (p, s, l);
    service->svc_enc_password[l] = 0;
    }

if (options.spb_password_enc)
    strcpy (service->svc_enc_password, options.spb_password_enc);

/* If an executable is defined for the service, try to fork a new process. 
 * Only do this if we are working with a version 1 service */

#ifndef SUPERSERVER
if (serv->serv_executable && options.spb_version == isc_spb_version1)
#else
if (serv->serv_thd && options.spb_version == isc_spb_version1)
#endif
    {
#ifndef SUPERSERVER
    gds__prefix (service_path, serv->serv_executable);
    service_fork (service_path, service);
#else
    /* if service is single threaded, only call if not currently running */
    if (serv->in_use == NULL)
        { /* No worry for multi-threading */
        service_fork (serv->serv_thd, service);
        }
    else if (!*(serv->in_use))
        {
        *(serv->in_use) = TRUE;
        service_fork (serv->serv_thd, service);
        }
    else
        {
        ERR_post (isc_service_att_err, isc_arg_gds,
		  isc_svc_in_use, isc_arg_string, serv->serv_name, 0);
        }
#endif
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (misc != misc_buf)
    gds__free ((SLONG*)misc);

return service;
}

void SVC_detach (
    SVC		service)
{
/**************************************
 *
 *	S V C _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a service.
 *
 **************************************/

#ifdef SERVER_SHUTDOWN
if (service->svc_do_shutdown)
    {
    JRD_shutdown_all();
    if (shutdown_fct)
	shutdown_fct(shutdown_param);
    else
	exit(0);

    /* The shutdown operation is complete, just wait to die */
    while(1);
    }
#endif	/* SERVER_SHUTDOWN */

#ifdef SUPERSERVER

/* Mark service as detached. */
/* If service thread is finished, cleanup memory being used by service. */

SVC_finish (service, SVC_detached);

#else

/* Go ahead and cleanup memory being used by service */
SVC_cleanup (service);

#endif
}
TEXT *SVC_err_string (
	TEXT *data, 
	USHORT length)
{
/********************************************
 *
 *   S V C _ e r r _ s t r i n g
 *
 ********************************************
 *
 * Functional Description:
 *     Uses ERR_string to save string data for the
 *     status vector
 ********************************************/
 return ERR_string (data, length);
}

void SVC_shutdown_init(
#ifdef  __BORLANDC__
        void (__stdcall *fptr)(UINT),
#else
        void (*fptr)(),
#endif
        ULONG   param)
{
/**************************************
 *
 *	S V C _ s h u t d o w n _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Set the global function pointer to the shutdown function.
 *
 **************************************/

shutdown_fct = fptr;
shutdown_param = param;
}

#ifdef SUPERSERVER
void SVC_fprintf (
    SVC		service,
    const SCHAR	*format,
    ...)
{
/**************************************
 *
 *	S V C _ f p r i n t f
 *
 **************************************/
/* Ensure that service is not detached. */
if (!(service->svc_flags & SVC_detached))
    {
    va_list	arglist;
    UCHAR	buf [1000];
    ULONG	i = 0;
    ULONG	n;

    va_start (arglist, format);
    n = vsprintf (buf, format, arglist);
    va_end (arglist);

    while (n > 0 && !(service->svc_flags & SVC_detached))
        {
        service_enqueue_byte (buf [i], service);
        n--;
        i++;
        }
    }
}

void SVC_putc (
    SVC		service,
    UCHAR	ch)
{
/**************************************
 *
 *	S V C _ p u t c
 *
 **************************************/
/* Ensure that service is not detached. */
if (!(service->svc_flags & SVC_detached))
    service_enqueue_byte (ch, service);
}
#endif /*SUPERSERVER*/

STATUS SVC_query2 (
    SVC		service,
    TDBB	tdbb,
    USHORT	send_item_length,
    SCHAR	*send_items,
    USHORT	recv_item_length,
    SCHAR	*recv_items,
    USHORT	buffer_length,
    SCHAR	*info)
{
/**************************************
 *
 *	S V C _ q u e r y 2
 *
 **************************************
 *
 * Functional description
 *	Provide information on service object
 *
 **************************************/
SCHAR	item, *items, *end_items, *end; 
UCHAR	buffer [256], dbbuf [1024];
USHORT	l, length, version, get_flags;
STATUS	*status;
#ifndef WINDOWS_ONLY
USHORT	timeout;
#endif

THREAD_EXIT;

/* Setup the status vector */
status = tdbb->tdbb_status_vector;
*status++ = isc_arg_gds;

/* Process the send portion of the query first. */

#ifndef WINDOWS_ONLY
timeout = 0;
items = send_items;
end_items = items + send_item_length;
while (items < end_items && *items != isc_info_end)
    switch ((item = *items++))
	{
	case isc_info_end:
	    break;

	default:
	    if (items + 2 <= end_items)
		{
		l = (USHORT)gds__vax_integer (items, 2);
		items += 2;
		if (items + l <= end_items)
		    {
		    switch (item)
			{
			case isc_info_svc_line:
			    service_put (service, items, l);
			    break;
			case isc_info_svc_message:
			    service_put (service, items - 3, l + 3);
			    break;
			case isc_info_svc_timeout:
			    timeout = (USHORT)gds__vax_integer (items, l);
			    break;
			case isc_info_svc_version:
			    version = (USHORT)gds__vax_integer (items, l);
			    break;
			}
		    }
		items += l;
		}
	    else
		items += 2;
	    break;
	}
#endif  /* WINDOWS_ONLY */

/* Process the receive portion of the query now. */

end = info + buffer_length;

items = recv_items;
end_items = items + recv_item_length;
while (items < end_items && *items != isc_info_end)
    {
    /*
    if we attached to the "anonymous" service we allow only following queries:

    isc_info_svc_get_config     - called from within remote/ibconfig.c
    isc_info_svc_dump_pool_info - called from within utilities/print_pool.c
    isc_info_svc_user_dbpath    - called from within utilities/security.c
    */
    if (service->svc_user_flag == SVC_user_none)
	{ 
	switch (*items)
	    {
	    case isc_info_svc_get_config:
	    case isc_info_svc_dump_pool_info:
	    case isc_info_svc_user_dbpath:
		break;
	    default:
		ERR_post (isc_bad_spb_form, 0);
		break;
	    }
	}

    switch ((item = *items++))
	{
	case isc_info_end:
	    break;

#ifdef SERVER_SHUTDOWN
	case isc_info_svc_svr_db_info:
	    {
	    USHORT num_dbs = 0, num = 0;
	    USHORT num_att = 0;
	    TEXT   *ptr, *ptr2;
	    
            *info++ = item;
	    ptr = JRD_num_attachments (dbbuf, sizeof(dbbuf), JRD_info_dbnames,
				       &num_att, &num_dbs);
            /* Move the number of attachments into the info buffer */
	    CK_SPACE_FOR_NUMERIC;
	    *info++ = isc_spb_num_att;
	    ADD_SPB_NUMERIC(info, num_att);
        
            /* Move the number of databases in use into the info buffer */
	    CK_SPACE_FOR_NUMERIC;
	    *info++ = isc_spb_num_db;
	    ADD_SPB_NUMERIC(info, num_dbs);

	    /* Move db names into the info buffer */
	    if (ptr2 = ptr)
		{
		num = (USHORT)isc_vax_integer(ptr2, sizeof(USHORT));
		assert (num == num_dbs);
		ptr2 += sizeof(USHORT);
		for (; num; num--)
		    {
		    length = (USHORT)isc_vax_integer(ptr2, sizeof(USHORT));
		    ptr2 += sizeof(USHORT);
		    if (!(info = INF_put_item (isc_spb_dbname, length, ptr2, info, end)))
			{
			THREAD_ENTER;
			return 0;
			}
		    ptr2 += length;
		    }
		
		if (ptr != dbbuf)
		    gds__free(ptr); /* memory has been allocated by
				       JRD_num_attachments() */
		}

	    if (info < end)
		*info++ = isc_info_flag_end;
	    }

            break;

	case isc_info_svc_svr_online:
        *info++ = item;
        if (service->svc_user_flag & SVC_user_dba)
            {
            service->svc_do_shutdown = FALSE;
            WHY_set_shutdown(FALSE);
            }
        else
            NEED_ADMIN_PRIVS ("isc_info_svc_svr_online");
        break;

	case isc_info_svc_svr_offline:
        *info++ = item;
        if (service->svc_user_flag & SVC_user_dba)
            {
            service->svc_do_shutdown = TRUE;
            WHY_set_shutdown(TRUE);
            }
        else
            NEED_ADMIN_PRIVS ("isc_info_svc_svr_offline");
        break;
#endif	/* SERVER_SHUTDOWN */

	/* The following 3 service commands (or items) stuff the response
	   buffer 'info' with values of environment variable INTERBASE, 
	   INTERBASE_LOCK or INTERBASE_MSG. If the environment variable
	   is not set then default value is returned.
	*/
	case isc_info_svc_get_env:
	case isc_info_svc_get_env_lock:
	case isc_info_svc_get_env_msg:
	    switch (item)
		{
		case isc_info_svc_get_env:
		    gds__prefix (buffer, "");
		    break;
		case isc_info_svc_get_env_lock:
		    gds__prefix_lock (buffer, "");
		    break;
		case isc_info_svc_get_env_msg:
		    gds__prefix_msg (buffer, "");
		}

	    /* Note: it is safe to use strlen to get a length of "buffer"
		     because gds_prefix[_lock|_msg] return a zero-terminated
		     string
	    */ 
	    if (!(info = INF_put_item (item, strlen(buffer), buffer, info, end)))
	        {
	        THREAD_ENTER;
	        return 0;
	        }
	    break;

#ifdef SUPERSERVER
        case isc_info_svc_dump_pool_info:
            {
	    int length;
	    char fname[MAXPATHLEN];
	    length = isc_vax_integer(items, sizeof (USHORT) );
            items += sizeof (USHORT);
	    strncpy( fname, items, length);
            fname[length] = 0;
	    JRD_print_all_counters(fname);
            break;
            }
#endif

	case isc_info_svc_get_config:
	    if (SVC_hdrtbl)
		{
		IPCCFG	h;
		*info++ = item;
		ISC_get_config(LOCK_HEADER, SVC_hdrtbl);
		for (h = SVC_hdrtbl; h->ipccfg_keyword; h++)
		    {
		    CK_SPACE_FOR_NUMERIC;
		    *info++ = h->ipccfg_key;
		    ADD_SPB_NUMERIC (info, *h->ipccfg_variable);
		    }
	        if (info < end)
		    *info++ = isc_info_flag_end;
		}
	    break;

	case isc_info_svc_default_config:
	    *info++ = item;
	    if (service->svc_user_flag & SVC_user_dba)
		{
		THREAD_ENTER;
		if (ISC_set_config(LOCK_HEADER, NULL))
		    ERR_FILE_IN_USE;
		THREAD_EXIT;
		}
	    else
		NEED_ADMIN_PRIVS ("isc_info_svc_default_config");
	    break;

	case isc_info_svc_set_config:
	    *info++ = item;

	    length = (USHORT)isc_vax_integer(items, sizeof(USHORT));
	    items += sizeof(USHORT);

	    /* Check for proper user authority */

	    if (service->svc_user_flag & SVC_user_dba)
		{
		int n;
		UCHAR *p, *end;

		end = items + length;

		/* count the number of parameters being set */

		p = items;
		for (n = 0; p < end; n++)
		    p += p[1] + 2;

		/* if there is at least one then do the configuration */

		if (n++)
		    {
		    IPCCFG tmpcfg;
		    /* allocate a buffer big enough to n struct ipccfg and
		     * n ipccfg variables.
		     */
		    tmpcfg = (IPCCFG)gds__alloc(n * (sizeof(struct ipccfg) + ALIGNMENT) + length);
		    if (tmpcfg)
			{
			p = (UCHAR*)tmpcfg + n * sizeof(struct ipccfg);
			for (n = 0; (UCHAR*)items < end; n++)
			    {
			    int	nBytes;

			    tmpcfg[n].ipccfg_keyword = NULL;
			    tmpcfg[n].ipccfg_key = *items++;
			    tmpcfg[n].ipccfg_variable = (ULONG*)p;
			    nBytes = *items++;
			    *(ULONG*)p = isc_vax_integer (items, nBytes);
			    p += ROUNDUP(nBytes, ALIGNMENT);
			    items += nBytes;
			    }
			tmpcfg[n].ipccfg_keyword = NULL;
			tmpcfg[n].ipccfg_key = 0;
			tmpcfg[n].ipccfg_variable = NULL;

			THREAD_ENTER;
			if (ISC_set_config(LOCK_HEADER, tmpcfg))
			    ERR_FILE_IN_USE;
			THREAD_EXIT;
			gds__free((ULONG*)tmpcfg);
			}
		    else
			{
			/* Return an error if there is no more memory to
			 * access the ibconfig file.  Do not continue
			 * processing the query request.
			 */
		        THREAD_ENTER;
			*status++ = isc_virmemexh;
			*status++ = isc_arg_end;
			return tdbb->tdbb_status_vector[1];
			}
		    }
		}
	    else
		{
		items += length;
		NEED_ADMIN_PRIVS ("isc_info_svc_set_config");	
		}
	    break;

	case isc_info_svc_version:
	    /* The version of the service manager */
	    CK_SPACE_FOR_NUMERIC;
	    *info++ = item;
	    ADD_SPB_NUMERIC (info, SERVICE_VERSION);
	    break;

	case isc_info_svc_capabilities:
	    /* bitmask defining any specific architectural differences */
	    CK_SPACE_FOR_NUMERIC;
	    *info++ = item;
	    ADD_SPB_NUMERIC (info, SERVER_CAPABILITIES_FLAG);
	    break;

	case isc_info_svc_running:
	    /* Returns the status of the flag SVC_thd_running */
	    CK_SPACE_FOR_NUMERIC;
	    *info++ = item;
	    if (service->svc_flags & SVC_thd_running)
	        ADD_SPB_NUMERIC (info, TRUE)
            else
                ADD_SPB_NUMERIC (info, FALSE)

            break;
	    
	case isc_info_svc_server_version:
	    /* The version of the server engine */
	    if (!(info = INF_put_item (item, strlen(GDS_VERSION), GDS_VERSION, info, end)))
	    	{
    		THREAD_ENTER;
	    	return 0;
	    	}
	    break;

	case isc_info_svc_implementation:
	    /* The server implementation - e.g. Interbase/sun4 */
        isc_format_implementation (IMPLEMENTATION, sizeof(buffer), buffer, 0, 0, NULL);
        if (!(info = INF_put_item (item, strlen(buffer), buffer, info, end)))
	        {
	        THREAD_ENTER;
	        return 0;
	        }

	    break;


	case isc_info_svc_user_dbpath:
	    /* The path to the user security database (isc4.gdb) */
            PWD_get_user_dbpath (buffer);

            if (!(info = INF_put_item (item, strlen (buffer), buffer, 
            				info, end)))
                {
                THREAD_ENTER;
                return 0;
                }
            break;
#ifndef WINDOWS_ONLY
	case isc_info_svc_response:
	    service->svc_resp_len = 0;
	    if (info + 4 > end)
		{
		*info++ = isc_info_truncated;
		break;
		}
	    service_put (service, &item, 1);
	    service_get (service, &item, 1, GET_BINARY, 0, &length);
	    service_get (service, buffer, 2, GET_BINARY, 0, &length);
	    l = (USHORT)gds__vax_integer (buffer, 2);
	    length = MIN (end - (info + 4), l);
	    service_get (service, info + 3, length, GET_BINARY, 0, &length);
	    info = INF_put_item (item, length, info + 3, info, end);
	    if (length != l)
		{
		*info++ = isc_info_truncated;
		l -= length;
		if (l > service->svc_resp_buf_len)
		    {
		    THREAD_ENTER;
		    if (service->svc_resp_buf)
			gds__free ((SLONG*)service->svc_resp_buf);
		    service->svc_resp_buf = (UCHAR *) gds__alloc ((SLONG) l);
		    /* FREE: in SVC_detach() */
		    if (!service->svc_resp_buf)		/* NOMEM: */
			{
			DEV_REPORT ("SVC_query: out of memory");
			/* NOMEM: not really handled well */
			l = 0;			/* set the length to zero */
			}
		    service->svc_resp_buf_len = l;
		    THREAD_EXIT;
		    }
		service_get (service, service->svc_resp_buf, l, GET_BINARY, 0, &length);
		service->svc_resp_ptr = service->svc_resp_buf;
		service->svc_resp_len = l;
		}
	    break;

	case isc_info_svc_response_more:
	    if (l = length = service->svc_resp_len)
		length = MIN (end - (info + 4), l);
	    if (!(info = INF_put_item (item, length, service->svc_resp_ptr, info, end)))
		{
		THREAD_ENTER;
		return 0;
		}
	    service->svc_resp_ptr += length;
	    service->svc_resp_len -= length;
	    if (length != l)
		*info++ = isc_info_truncated;
	    break;

    case isc_info_svc_total_length:
        service_put (service, &item, 1);
        service_get (service, &item, 1, GET_BINARY, 0, &length);
        service_get (service, buffer, 2, GET_BINARY, 0, &length);
        l = (USHORT)gds__vax_integer (buffer, 2);
        service_get (service, buffer, l, GET_BINARY, 0, &length);
        if (!(info = INF_put_item (item, length, buffer, info, end)))
            {
            THREAD_ENTER;
            return 0;
            }
        break;

	case isc_info_svc_line:
	case isc_info_svc_to_eof:
	case isc_info_svc_limbo_trans:
	case isc_info_svc_get_users:
	    if (info + 4 > end)
		{
		*info++ = isc_info_truncated;
		break;
		}

	    if (item == isc_info_svc_line)
		get_flags = GET_LINE;
	    else
		{
		if (item == isc_info_svc_to_eof)
		    get_flags = GET_EOF;
		else
		    get_flags = GET_BINARY;
		}
	    
	    service_get (service, info + 3, end - (info + 4), get_flags, timeout, &length);
		 
	    /* If the read timed out, return the data, if any, & a timeout 
	       item.  If the input buffer was not large enough
	       to store a read to eof, return the data that was read along
	       with an indication that more is available. */

	    info = INF_put_item (item, length, info + 3, info, end);

	    if (service->svc_flags & SVC_timeout)
		*info++ = isc_info_svc_timeout;
	    else
		{
		if (!length && !(service->svc_flags & SVC_finished))
		    *info++ = isc_info_data_not_ready;
		else
		    if (item == isc_info_svc_to_eof &&
			!(service->svc_flags & SVC_finished))
			*info++ = isc_info_truncated;
		}
	    break;
#endif  /* WINDOWS_ONLY */
	}

    if (service->svc_user_flag == SVC_user_none)
	break;
    }

if (info < end)
    *info = isc_info_end;


THREAD_ENTER;
return tdbb->tdbb_status_vector[1];
}
void SVC_query (
    SVC		service,
    USHORT	send_item_length,
    SCHAR	*send_items,
    USHORT	recv_item_length,
    SCHAR	*recv_items,
    USHORT	buffer_length,
    SCHAR	*info)
{
/**************************************
 *
 *	S V C _ q u e r y
 *
 **************************************
 *
 * Functional description
 *	Provide information on service object.
 *
 **************************************/
SCHAR	item, *items, *end_items, *end, *p, *q; 
UCHAR	buffer [256];
USHORT	l, length, version, get_flags;
USHORT  num_att = 0;
USHORT  num_dbs = 0;
#ifndef WINDOWS_ONLY
USHORT	timeout;
#endif

THREAD_EXIT;

/* Process the send portion of the query first. */

#ifndef WINDOWS_ONLY
timeout = 0;
items = send_items;
end_items = items + send_item_length;
while (items < end_items && *items != isc_info_end)
    switch ((item = *items++))
	{
	case isc_info_end:
	    break;

	default:
	    if (items + 2 <= end_items)
		{
		l = (USHORT)gds__vax_integer (items, 2);
		items += 2;
		if (items + l <= end_items)
		    {
		    switch (item)
			{
			case isc_info_svc_line:
			    service_put (service, items, l);
			    break;
			case isc_info_svc_message:
			    service_put (service, items - 3, l + 3);
			    break;
			case isc_info_svc_timeout:
			    timeout = (USHORT)gds__vax_integer (items, l);
			    break;
			case isc_info_svc_version:
			    version = (USHORT)gds__vax_integer (items, l);
			    break;
			}
		    }
		items += l;
		}
	    else
		items += 2;
	    break;
	}
#endif  /* WINDOWS_ONLY */

/* Process the receive portion of the query now. */

end = info + buffer_length;

items = recv_items;
end_items = items + recv_item_length;
while (items < end_items && *items != isc_info_end)
    {
    switch ((item = *items++))
	{
	case isc_info_end:
	    break;

#ifdef SERVER_SHUTDOWN
	case isc_info_svc_svr_db_info:
	    JRD_num_attachments(NULL, 0, 0, &num_att, &num_dbs);
	    length = INF_convert (num_att, buffer);
	    if (!(info = INF_put_item (item, length, buffer, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    length = INF_convert (num_dbs, buffer);
	    if (!(info = INF_put_item (item, length, buffer, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;

	case isc_info_svc_svr_online:
	    *info++ = item;
	    if (service->svc_user_flag & SVC_user_dba)
		{
		service->svc_do_shutdown = FALSE;
		WHY_set_shutdown(FALSE);
		*info++ = 0;	/* Success */
		}
	    else
		*info++ = 2;	/* No user authority */
	    break;

	case isc_info_svc_svr_offline:
	    *info++ = item;
	    if (service->svc_user_flag & SVC_user_dba)
		{
		service->svc_do_shutdown = TRUE;
		WHY_set_shutdown(TRUE);
		*info++ = 0;	/* Success */
		}
	    else
		*info++ = 2;	/* No user authority */
	    break;
#endif	/* SERVER_SHUTDOWN */

	/* The following 3 service commands (or items) stuff the response
	   buffer 'info' with values of environment variable INTERBASE, 
	   INTERBASE_LOCK or INTERBASE_MSG. If the environment variable
	   is not set then default value is returned.
	*/
	case isc_info_svc_get_env:
	case isc_info_svc_get_env_lock:
	case isc_info_svc_get_env_msg:
	    switch (item)
		{
		case isc_info_svc_get_env:
		    gds__prefix (buffer, "");
		    break;
		case isc_info_svc_get_env_lock:
		    gds__prefix_lock (buffer, "");
		    break;
		case isc_info_svc_get_env_msg:
		    gds__prefix_msg (buffer, "");
		}

	    /* Note: it is safe to use strlen to get a length of "buffer"
		     because gds_prefix[_lock|_msg] return a zero-terminated
		     string
	    */ 
	    if (!(info = INF_put_item (item, strlen(buffer), buffer, info, end)))
	        {
	        THREAD_ENTER;
	        return;
	        }
	    break;

#ifdef SUPERSERVER
        case isc_info_svc_dump_pool_info:
            {
	    int length;
	    char fname[MAXPATHLEN];
	    length = isc_vax_integer(items, sizeof (USHORT) );
            items += sizeof (USHORT);
	    strncpy( fname, items, length);
            fname[length] = 0;
	    JRD_print_all_counters(fname);
            break;
            }
#endif

	case isc_info_svc_get_config:
	    if (SVC_hdrtbl)
		{
		IPCCFG	h;
		UCHAR	*p;
		ISC_get_config(LOCK_HEADER, SVC_hdrtbl);
		p = buffer;
		for (h = SVC_hdrtbl; h->ipccfg_keyword; h++)
		    {
		    *p++ = h->ipccfg_key;
		    length = INF_convert (*h->ipccfg_variable, p+1);
		    *p++ = (UCHAR)length;
		    p += length;
		    }
		if (!(info = INF_put_item(item, p-buffer, buffer, info, end)))
		    {
		    THREAD_ENTER;
		    return;
		    }
		}
	    break;

	case isc_info_svc_default_config:
	    *info++ = item;
	    if (service->svc_user_flag & SVC_user_dba)
		{
		THREAD_ENTER;
		if (ISC_set_config(LOCK_HEADER, NULL))
		    *info++ = 3;	/* File in use */
		else
		    *info++ = 0;	/* Success */
		THREAD_EXIT;
		}
	    else
		*info++ = 2;
	    break;

	case isc_info_svc_set_config:
	    *info++ = item;

	    length = (USHORT)isc_vax_integer(items, sizeof(USHORT));
	    items += sizeof(USHORT);

	    /* Check for proper user authority */

	    if (service->svc_user_flag & SVC_user_dba)
		{
		int n;
		UCHAR *p, *end;

		end = items + length;

		/* count the number of parameters being set */

		p = items;
		for (n = 0; p < end; n++)
		    p += p[1] + 2;

		/* if there is at least one then do the configuration */

		if (n++)
		    {
		    IPCCFG tmpcfg;
		    /* allocate a buffer big enough to n struct ipccfg and
		     * n ipccfg variables.
		     */
		    tmpcfg = (IPCCFG)gds__alloc(n * (sizeof(struct ipccfg) + ALIGNMENT) + length);
		    if (tmpcfg)
			{
			p = (UCHAR*)tmpcfg + n * sizeof(struct ipccfg);
			for (n = 0; (UCHAR*)items < end; n++)
			    {
			    int	nBytes;

			    tmpcfg[n].ipccfg_keyword = NULL;
			    tmpcfg[n].ipccfg_key = *items++;
			    tmpcfg[n].ipccfg_variable = (ULONG*)p;
			    nBytes = *items++;
			    *(ULONG*)p = isc_vax_integer (items, nBytes);
			    p += ROUNDUP(nBytes, ALIGNMENT);
			    items += nBytes;
			    }
			tmpcfg[n].ipccfg_keyword = NULL;
			tmpcfg[n].ipccfg_key = 0;
			tmpcfg[n].ipccfg_variable = NULL;

			THREAD_ENTER;
			if (ISC_set_config(LOCK_HEADER, tmpcfg))
			    *info++ = 3;	/* File in use */
			else
			    *info++ = 0;	/* Success */
			THREAD_EXIT;
			gds__free((ULONG*)tmpcfg);
			}
		    else
			{
			*info++ = 1;	/* No Memory */
			items = end;
			}

		    }
		*info++ = 0;	/* Success */
		}
	    else
		{
		items += length;
		*info++ = 2;	/* No user authority */
		}
	    break;

	case isc_info_svc_version:
	    /* The version of the service manager */

	    length = INF_convert (SERVICE_VERSION, buffer);
	    if (!(info = INF_put_item (item, length, buffer, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;

	case isc_info_svc_capabilities:
	    /* bitmask defining any specific architectural differences */

	    length = INF_convert (SERVER_CAPABILITIES_FLAG, buffer);
	    if (!(info = INF_put_item (item, length, buffer, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;

	case isc_info_svc_server_version:
	    /* The version of the server engine */

	    p = buffer;
	    *p++ = 1;			/* Count */
	    *p++ = sizeof (GDS_VERSION) - 1 ;
	    for (q = GDS_VERSION; *q; p++, q++)
		*p = *q;
	    if (!(info = INF_put_item (item, p - (SCHAR*)buffer, buffer, 
					info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;

	case isc_info_svc_implementation:
	    /* The server implementation - e.g. Interbase/sun4 */

	    p = buffer;
	    *p++ = 1;			/* Count */
	    *p++ = IMPLEMENTATION;
	    if (!(info = INF_put_item (item, p - (SCHAR*)buffer, buffer, 
					info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;


	case isc_info_svc_user_dbpath:
	    /* The path to the user security database (isc4.gdb) */
            PWD_get_user_dbpath (buffer);

            if (!(info = INF_put_item (item, strlen (buffer), buffer, 
            				info, end)))
                {
                THREAD_ENTER;
                return;
                }
            break;
	
#ifndef WINDOWS_ONLY
	case isc_info_svc_response:
	    service->svc_resp_len = 0;
	    if (info + 4 > end)
		{
		*info++ = isc_info_truncated;
		break;
		}
	    service_put (service, &item, 1);
	    service_get (service, &item, 1, GET_BINARY, 0, &length);
	    service_get (service, buffer, 2, GET_BINARY, 0, &length);
	    l = (USHORT)gds__vax_integer (buffer, 2);
	    length = MIN (end - (info + 4), l);
	    service_get (service, info + 3, length, GET_BINARY, 0, &length);
	    info = INF_put_item (item, length, info + 3, info, end);
	    if (length != l)
		{
		*info++ = isc_info_truncated;
		l -= length;
		if (l > service->svc_resp_buf_len)
		    {
		    THREAD_ENTER;
		    if (service->svc_resp_buf)
			gds__free ((SLONG*)service->svc_resp_buf);
		    service->svc_resp_buf = (UCHAR *) gds__alloc ((SLONG) l);
		    /* FREE: in SVC_detach() */
		    if (!service->svc_resp_buf)		/* NOMEM: */
			{
			DEV_REPORT ("SVC_query: out of memory");
			/* NOMEM: not really handled well */
			l = 0;			/* set the length to zero */
			}
		    service->svc_resp_buf_len = l;
		    THREAD_EXIT;
		    }
		service_get (service, service->svc_resp_buf, l, GET_BINARY, 0, &length);
		service->svc_resp_ptr = service->svc_resp_buf;
		service->svc_resp_len = l;
		}
	    break;

	case isc_info_svc_response_more:
	    if (l = length = service->svc_resp_len)
		length = MIN (end - (info + 4), l);
	    if (!(info = INF_put_item (item, length, service->svc_resp_ptr, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    service->svc_resp_ptr += length;
	    service->svc_resp_len -= length;
	    if (length != l)
		*info++ = isc_info_truncated;
	    break;

	case isc_info_svc_total_length:
	    service_put (service, &item, 1);
	    service_get (service, &item, 1, GET_BINARY, 0, &length);
	    service_get (service, buffer, 2, GET_BINARY, 0, &length);
	    l = (USHORT)gds__vax_integer (buffer, 2);
	    service_get (service, buffer, l, GET_BINARY, 0, &length);
	    if (!(info = INF_put_item (item, length, buffer, info, end)))
		{
		THREAD_ENTER;
		return;
		}
	    break;

	case isc_info_svc_line:
	case isc_info_svc_to_eof:
	    if (info + 4 > end)
		{
		*info++ = isc_info_truncated;
		break;
		}
	    get_flags = (item == isc_info_svc_line) ? GET_LINE : GET_EOF;
	    service_get (service, info + 3, end - (info + 4), get_flags, timeout, &length);
		 
	    /* If the read timed out, return the data, if any, & a timeout 
	       item.  If the input buffer was not large enough
	       to store a read to eof, return the data that was read along
	       with an indication that more is available. */

	    info = INF_put_item (item, length, info + 3, info, end);

	    if (service->svc_flags & SVC_timeout)
		*info++ = isc_info_svc_timeout;
	    else
		{
		if (!length && !(service->svc_flags & SVC_finished))
		    *info++ = isc_info_data_not_ready;
		else
		    if (item == isc_info_svc_to_eof &&
			!(service->svc_flags & SVC_finished))
			*info++ = isc_info_truncated;
		}
	    break;

#endif  /* WINDOWS_ONLY */
	}
    }

if (info < end)
    *info = isc_info_end;

THREAD_ENTER;
}

void *SVC_start (
    SVC     service,
    USHORT  spb_length,
    SCHAR   *spb)
{
/**************************************
 *
 *	S V C _ s t a r t
 *
 **************************************
 *
 * Functional description
 *      Start an InterBase service
 *
 **************************************/
TDBB    tdbb;
CONST   struct serv *serv;
USHORT  svc_id;
JMP_BUF env, *old_env;
USHORT	argc;
TEXT	**arg, *p, *q, *tmp_ptr = NULL;
USHORT	opt_switch_len = 0;
BOOLEAN	flag_spb_options = FALSE;
#ifndef SUPERSERVER
TEXT    service_path [MAXPATHLEN];
#endif

/* 	NOTE: The parameter RESERVED must not be used
 *	for any purpose as there are networking issues
 *	involved (as with any handle that goes over the
 *	network).  This parameter will be implemented at 
 *	a later date.
 */

isc_resv_handle    reserved = NULL;  /* Reserved for future functionality */

/* The name of the service is the first element of the buffer */
svc_id = *spb;

for (serv = services; serv->serv_action; serv++)
    if (serv->serv_action == svc_id)
    break;

if (!serv->serv_name)
    ERR_post (isc_svcnotdef, isc_arg_string,
	      SVC_err_string (serv->serv_name, strlen (serv->serv_name)), 0);

/* currently we do not use "anonymous" service for any purposes but
   isc_service_query() */
if (service->svc_user_flag == SVC_user_none)
    ERR_post (isc_bad_spb_form, 0);

if (!thd_initialized)
    {
    THD_MUTEX_INIT(thd_mutex);
    thd_initialized = TRUE;
    }

THD_MUTEX_LOCK(thd_mutex);
if (service->svc_flags & SVC_thd_running)
    {
    THD_MUTEX_UNLOCK(thd_mutex);
    ERR_post (isc_svc_in_use, isc_arg_string,
	      SVC_err_string (serv->serv_name, strlen (serv->serv_name)), 0);
    }
else
    {
    /* Another service may have been started with this service block.  If so,
     * we must reset the service flags.
     */
    if (!(service->svc_flags & SVC_detached))
        service->svc_flags = 0;
    service->svc_flags |= SVC_thd_running;
    if (service->svc_argc && service->svc_switches)
	{
	gds__free (service->svc_switches);
	service->svc_switches = NULL;
	}
    }
THD_MUTEX_UNLOCK(thd_mutex);

tdbb = GET_THREAD_DATA;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (service->svc_switches != NULL)
	gds__free ((SLONG*)service->svc_switches);
    if (service != NULL)
	gds__free ((SLONG*)service);
    ERR_punt();
    }

/* Only need to add username and password information to those calls which need 
 * to make a database connection
 */
if (*spb == isc_action_svc_backup  || 
    *spb == isc_action_svc_restore || 
    *spb == isc_action_svc_repair  ||
    *spb == isc_action_svc_add_user ||
    *spb == isc_action_svc_delete_user ||
    *spb == isc_action_svc_modify_user ||
    *spb == isc_action_svc_display_user ||
    *spb == isc_action_svc_db_stats ||
    *spb == isc_action_svc_properties)
    {
    /* the user issued a username when connecting to the service so
     * add the length of the username and switch to new_spb_length
     */
    
    if (*service->svc_username)
        opt_switch_len += (strlen(service->svc_username)+1+sizeof (USERNAME_SWITCH));
    
   /* the user issued a password when connecting to the service so add
    * the length of the password and switch to new_spb_length
    */
    if (*service->svc_enc_password)
        opt_switch_len += (strlen (service->svc_enc_password)+1+sizeof(PASSWORD_SWITCH));

    /* If svc_switches is not used -- call a command-line parsing utility */
    if (!service->svc_switches)
    	conv_switches (spb_length, opt_switch_len, spb, &service->svc_switches);
    else
	{
	/* Command line options (isc_spb_options) is used.
	 * Currently the only case in which it might happen is -- gbak utility
	 * is called with a "-server" switch.
	 */
	flag_spb_options = TRUE;

	tmp_ptr = (TEXT *) gds__alloc ((SLONG) (strlen (service->svc_switches) +
						+ 1 + opt_switch_len + 1));
	if (!tmp_ptr)	/* NOMEM: */
	    ERR_post (isc_virmemexh, 0);
	}

    /* add the username and password to the end of svc_switches if needed */
    if (service->svc_switches)
	{
	if (flag_spb_options)
	    strcpy (tmp_ptr, service->svc_switches);

	if (*service->svc_username)
	    {
	    if (!flag_spb_options)
		sprintf (service->svc_switches, "%s %s %s",
		         service->svc_switches, USERNAME_SWITCH,
			 service->svc_username);
	    else
		sprintf (tmp_ptr, "%s %s %s", tmp_ptr, USERNAME_SWITCH,
			 service->svc_username);
	    }

	if (*service->svc_enc_password)
	    {
	    if (!flag_spb_options)
		sprintf (service->svc_switches, "%s %s %s",
			 service->svc_switches, PASSWORD_SWITCH,
			 service->svc_enc_password);
	    else
		sprintf (tmp_ptr, "%s %s %s", tmp_ptr, PASSWORD_SWITCH,
			 service->svc_enc_password);
	    }

	if (flag_spb_options)
	    {
	    gds__free (service->svc_switches);
	    service->svc_switches = tmp_ptr;
	    }
	}
    }
else
    /* If svc_switches is not used -- call a command-line parsing utility */
    if (!service->svc_switches)
    	conv_switches (spb_length, opt_switch_len, spb, &service->svc_switches);
    else
    	{
	    assert (service->svc_switches == NULL);
	    }
/* All services except for get_ib_log require switches */
if (service->svc_switches == NULL && *spb != isc_action_svc_get_ib_log)
    ERR_post (isc_bad_spb_form, 0);

#ifndef SUPERSERVER
if (serv->serv_executable)
    {
    gds__prefix (service_path, serv->serv_executable);
    service->svc_flags = SVC_forked;
    service_fork (service_path, service);
    }

#else

/* Break up the command line into individual arguments. */
if (service->svc_switches)
    {
    for (argc = 2, p = service->svc_switches; *p;)
        {
        if (*p == ' ')
            {
            argc++;
            while (*p == ' ')
                p++;
            }
        else
	    {
            if (*p == SVC_TRMNTR)
                {
                while (*p++ && *p != SVC_TRMNTR);
		assert (*p == SVC_TRMNTR);
                }
	    p++;
	    }
        }

        service->svc_argc = argc;
        
        arg = (TEXT**) gds__alloc ((SLONG) ((argc + 1) * sizeof (TEXT*)));
	/*
	 * the service block can be reused hence free a memory from the
	 * previous usage if any.
         */
	if (service->svc_argv)
	    gds__free (service->svc_argv);
        service->svc_argv = arg;
        /* FREE: at SVC_detach() - Possible memory leak if ERR_post() occurs */
        if (!arg)               /* NOMEM: */
            ERR_post (isc_virmemexh, 0);
        
        *arg++ = (void*) serv->serv_thd;
        p = q = service->svc_switches;
       
        while (*q == ' ')
            q++;

        while (*q)
            {
            *arg = p;
            while (*p = *q++)
                {
                if (*p == ' ') break;

                if (*p == SVC_TRMNTR)
                    {
		    *arg = ++p;	/* skip terminator */
                    while (*p = *q++)
						/* If *q points to the last argument, then terminate the argument */
                        if ((*q == 0 || *q == ' ') && *p == SVC_TRMNTR)
                            {
                            *p = '\0';
                            break;
                            }
                        else
                            p++;
                    }

                if (*p == '\\' && *q == ' ')
                    {
                    *p = ' ';
                    q++;
                    }
                p++;
                }
	    arg++;
            if (!*p)
                break;
            *p++ = '\0';
            while (*q == ' ')
                q++;
            }
        *arg = NULL;
    }

/*
 * the service block can be reused hence free a memory from the
 * previous usage as well as init a status vector.
 */

memset ((void*)service->svc_status, 0, ISC_STATUS_LENGTH * sizeof(STATUS));

if (service->svc_stdout)
    gds__free (service->svc_stdout);

service->svc_stdout = gds__alloc ((SLONG) SVC_STDOUT_BUFFER_SIZE + 1);
/* FREE: at SVC_detach() */
if (!service->svc_stdout)               /* NOMEM: */
    ERR_post (isc_virmemexh, 0);

if (serv->serv_thd)
    {
    SLONG count;
    EVENT evnt_ptr = &(service->svc_start_event);

    THREAD_EXIT;
    /* create an event for the service.  The event will be signaled once the
     * particular service has reached a point in which it can start to return
     * information to the client.  This will allow isc_service_start to
     * include in its status vector information about the service's ability to
     * start.  This is needed since gds__thread_start will almost always
     * succeed.
     */
    ISC_event_init (evnt_ptr, 0, 0);
    count = ISC_event_clear (evnt_ptr);
    
    gds__thread_start (serv->serv_thd, service, 0, 0, (void*)&service->svc_handle);

    /* check for the service being detached.  This will prevent the thread
     * from waiting infinitely if the client goes away
     */
    while (!(service->svc_flags & SVC_detached))
        {
        if (ISC_event_wait (1, &evnt_ptr, &count, 60*1000, (FPTR_VOID)0, NULL_PTR))
	    continue;
        else
     	    /* the event was posted */
	    break;
        }

    ISC_event_fini (evnt_ptr);
    THREAD_ENTER;
    }
else
    ERR_post (isc_svcnotdef, isc_arg_string,
	      SVC_err_string (serv->serv_name, strlen (serv->serv_name)), 0);

#endif	/* SUPERSERVER */


tdbb->tdbb_setjmp = (UCHAR*) old_env;
return reserved;
}

void SVC_read_ib_log (
	SVC service)
{
/**************************************
 *
 *      S V C _ r e a d _ i b _ l o g
 *
 **************************************
 *
 * Functional description
 *   Service function which reads the InterBase
 *   log file into the service buffers.
 *
 **************************************/
IB_FILE	*file;
TEXT	name [MAXPATHLEN], buffer [100];
BOOLEAN     svc_started = FALSE;
#ifdef SUPERSERVER
STATUS	*status;

status = service->svc_status;
*status++ = isc_arg_gds;
#endif

gds__prefix (name, LOGFILE);
if ((file = ib_fopen (name, "r")) != NULL)
    {
#ifdef SUPERSERVER
    *status++ = SUCCESS;
    *status++ = isc_arg_end;
#endif
    SVC_STARTED (service);
    svc_started = TRUE;
    while (!ib_feof (file) && !ib_ferror (file))
       {
       ib_fgets (buffer, sizeof(buffer), file);
#ifdef SUPERSERVER
       SVC_fprintf (service, "%s", buffer);
#else
	service_put (service, buffer, sizeof (buffer));
#endif
       }
    }

if (!file || file && ib_ferror (file))
    {
#ifdef SUPERSERVER
    *status++ = isc_sys_request;
    if (!file) {
        SVC_STATUS_ARG (isc_arg_string, "ib_fopen");
    }
    else {
	SVC_STATUS_ARG (isc_arg_string, "ib_fgets");
    }
    *status++ = SYS_ARG;
    *status++ = errno;
    *status++ = isc_arg_end;
#endif
    if (!svc_started)
        SVC_STARTED (service);
    }

if (file)
    ib_fclose (file);

service->svc_handle = 0;
if (service->svc_service->in_use != NULL)
    *(service->svc_service->in_use) = FALSE;

#ifdef SUPERSERVER
SVC_finish (service, SVC_finished);
#else
SVC_cleanup (service);
#endif
}

static void get_options (
    UCHAR	*spb,
    USHORT	spb_length,
    TEXT	*scratch,
    SPB		*options)
{
/**************************************
 *
 *	g e t _ o p t i o n s
 *
 **************************************
 *
 * Functional description
 *	Parse service parameter block picking up options and things.
 *
 **************************************/
UCHAR	*p, *end_spb;
USHORT	l;

MOVE_CLEAR (options, (SLONG) sizeof (struct spb));
p = spb;
end_spb = p + spb_length;

if (p < end_spb && (*p != isc_spb_version1 && *p != isc_spb_version))
    ERR_post (isc_bad_spb_form, isc_arg_gds, isc_wrospbver, 0);

while (p < end_spb)
    switch (*p++)
	{
    case isc_spb_version1:
        options->spb_version = isc_spb_version1;
        break;

    case isc_spb_version:
        options->spb_version = *p++;
        break;

	case isc_spb_sys_user_name:
	    options->spb_sys_user_name = get_string_parameter (&p, &scratch);
	    break;

	case isc_spb_user_name:
	    options->spb_user_name = get_string_parameter (&p, &scratch);
	    break;

	case isc_spb_password:
	    options->spb_password = get_string_parameter (&p, &scratch);
	    break;

	case isc_spb_password_enc:
	    options->spb_password_enc = get_string_parameter (&p, &scratch);
	    break;

	case isc_spb_command_line:
	    options->spb_command_line = get_string_parameter (&p, &scratch);
	    break;

	default:
	    l = *p++;
	    p += l;
	}
}

static TEXT *get_string_parameter (
    UCHAR	**spb_ptr,
    TEXT	**opt_ptr)
{
/**************************************
 *
 *	g e t _ s t r i n g _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Pick up a string valued parameter, copy it to a running temp,
 *	and return pointer to copied string.
 *
 **************************************/
UCHAR	*spb;
TEXT	*opt;
USHORT	l;

opt = *opt_ptr;
spb = *spb_ptr;

if (l = *spb++)
    do *opt++ = *spb++; while (--l);

*opt++ = 0;
*spb_ptr = spb;
spb = (UCHAR *) *opt_ptr;
*opt_ptr = opt;

return (TEXT *) spb;
}

static void io_error (
    TEXT	*string,
    SLONG	status,
    TEXT	*filename,
    STATUS  operation,
    BOOLEAN	reenter_flag)
{
/**************************************
 *
 *	i o _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Report an I/O error.  If the reenter_flag
 *	is TRUE, re-enter the scheduler.
 *
 **************************************/

#ifdef MULTI_THREAD
if (reenter_flag)
    THREAD_ENTER;
#endif

ERR_post (isc_io_error, isc_arg_string, string, isc_arg_string, filename,
        isc_arg_gds, operation, SYS_ERR, status, 0);
}

#ifdef WINDOWS_ONLY
#ifndef SUPERSERVER
#define PIPE_OPERATIONS
static void service_close (
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ c l o s e		( W I N D O W S _ O N L Y )
 *
 **************************************
 *
 * Functional description
 *	Shutdown the connection to a service.
 *
 **************************************/
}


static void service_fork (
	TEXT	*service_path,
	SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ f o r k		( W I N D O W S _ O N L Y )
 *
 **************************************
 *
 * Functional description
 *	Startup a service.
 *
 **************************************/
/* Windows currently doesn't for any executables...just a stub */
}
#endif /* ifndef SUPERSERVER */
#endif /* WINDOWS_ONLY */

#ifdef WIN_NT
#ifndef SUPERSERVER
#define PIPE_OPERATIONS
static void service_close (
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ c l o s e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Shutdown the connection to a service.
 *	Simply close the input and output pipes.
 *
 **************************************/
CloseHandle ((HANDLE) service->svc_input);
CloseHandle ((HANDLE) service->svc_output);
CloseHandle ((HANDLE) service->svc_handle);
}

static void service_fork (
    TEXT	*service_path,
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ f o r k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Startup a service.
 *
 **************************************/
TEXT			*argv_data, argv_data_buf [512], *p, *q, *arg;
USHORT			len, quote_flag, user_quote;
HANDLE			my_input, my_output, pipe_input, pipe_output, pipe_error;
SECURITY_ATTRIBUTES	attr;
STARTUPINFO		start_crud;
PROCESS_INFORMATION	pi;
USHORT			ret;
BOOLEAN			svc_flag;
SLONG			status;
BOOLEAN         windows_nt = ISC_get_ostype();

/* Only Create the pipes on Windows NT.  There is a bug on Windows
   95 that prohibits these handles from being converted by the
   child process
*/
my_input = pipe_output = my_output = pipe_input = INVALID_HANDLE_VALUE;
if (windows_nt)
    {
    /* Set up input and output pipes and make them the ib_stdin, ib_stdout,
       and ib_stderr of the forked process. */

    attr.nLength = sizeof (SECURITY_ATTRIBUTES);
    attr.bInheritHandle = TRUE;
    attr.lpSecurityDescriptor = NULL;

    if (!CreatePipe (&my_input, &pipe_output, &attr, 0) ||
        !CreatePipe (&pipe_input, &my_output, &attr, 0) ||
        !DuplicateHandle (GetCurrentProcess(), pipe_output,
	    GetCurrentProcess(), &pipe_error, 0, TRUE, DUPLICATE_SAME_ACCESS))
        {
        status = GetLastError();
        CloseHandle (my_input);
        CloseHandle (pipe_output);
        CloseHandle (my_output);
        CloseHandle (pipe_input);
        ERR_post (isc_sys_request, isc_arg_string,
    	    (my_output != INVALID_HANDLE_VALUE) ? "CreatePipe" : "DuplicateHandle",
	    SYS_ERR, status, 0);
        }
    }
else
    {
    /* Create a temporary file and get the OS handle to
       the file created.  This handle will be used in subsequent
       calls to the windows API functions for working with the files
    */
    int tmp;
    char *fname;
    char tmpPath[MAX_PATH_LENGTH];

    GetTempPath (MAX_PATH_LENGTH, tmpPath);
    fname = _tempnam(tmpPath, "ibsvc" );
    tmp = _open (fname, _O_RDWR | _O_CREAT | _O_TEMPORARY, _S_IREAD | _S_IWRITE );
    my_input = _get_osfhandle(tmp);

    fname = _tempnam(tmpPath, "ibsvc" );
    tmp = _open (fname, _O_RDWR | _O_CREAT | _O_TEMPORARY, _S_IREAD | _S_IWRITE );
    my_output = _get_osfhandle(tmp);

    if (my_input == INVALID_HANDLE_VALUE ||
        my_output == INVALID_HANDLE_VALUE )
        {
        CloseHandle (my_input);
        CloseHandle (my_output);
        ERR_post (isc_sys_request, isc_arg_string, "CreateFile", SYS_ERR, errno, 0);
        }
    }

/* Make sure we have buffers that are large enough to hold the number
   and size of the command line arguments.  Add some extra space for
   the pipe's file handles. */

len = strlen (service_path) + strlen (service->svc_switches) + 16;
for (p = service->svc_switches; *p;)
    if (*p++ == ' ')
	len += 2;
if (len > sizeof (argv_data_buf))
    argv_data = gds__alloc ((SLONG) len);
else
    argv_data = argv_data_buf;
/* FREE: at procedure return */
if (!argv_data)		/* NOMEM: */
    ERR_post (isc_virmemexh, 0);

/* Create a command line. */

svc_flag = FALSE;

for (p = argv_data, q = service_path; *p = *q++; p++)
    ;

q = service->svc_switches;
if (*q)
    *p++ = ' ';

while (*q == ' ')
    q++;
user_quote = FALSE;
while (*q)
    {
    arg = p;
    *p++ = '\"';
    quote_flag = FALSE;
    while (((*p = *q++) && *p != ' ') || user_quote)
	{
	if (*p == '\\' && *q == ' ' && !user_quote)
	    {
	    *p = ' ';
	    q++;
	    quote_flag = TRUE;
	    }
	if (*p == '"')
	    {
	    user_quote = !user_quote;
	    p++;
	    continue;
	    }
	p++;
	}
    if (!quote_flag)
	{
	*arg = ' ';
	if (!strncmp (arg, " -svc", p - arg))
	    {
        if (windows_nt)
	        sprintf (p, "_re %d %d %d", pipe_input, pipe_output, pipe_error);
        else
            sprintf (p, "_re %d %d %d", my_output, my_input, my_output);
	    p += strlen (p);
	    *p = q [-1];
	    svc_flag = TRUE;
	    }
	}
    else
	{
	p [1] = p [0];
	*p++ = '\"';
	}
    if (!*p)
	break;
    *p++ = ' ';
    while (*q == ' ')
	q++;
    }

THREAD_EXIT;

start_crud.cb = sizeof (STARTUPINFO);
start_crud.lpReserved = NULL;
start_crud.lpReserved2 = NULL;
start_crud.cbReserved2 = 0;
start_crud.lpDesktop = NULL;
start_crud.lpTitle = NULL;
start_crud.dwFlags = STARTF_USESTDHANDLES;
start_crud.hStdInput =  my_output;
start_crud.hStdOutput = my_input;
start_crud.hStdError = my_input;

if (!(ret = CreateProcess (NULL,
	argv_data,
	NULL,
	NULL,
	TRUE,
	NORMAL_PRIORITY_CLASS | DETACHED_PROCESS,
	NULL,
	NULL,
	&start_crud,
	&pi)))
    status = GetLastError();

if (windows_nt)
    {
    CloseHandle (pipe_input);
    CloseHandle (pipe_output);
    CloseHandle (pipe_error);
    }
THREAD_ENTER;

if (argv_data != argv_data_buf)
    gds__free ((SLONG*)argv_data);

if (!ret)
    ERR_post (isc_sys_request, isc_arg_string, "CreateProcess", SYS_ERR, status, 0);

DuplicateHandle (GetCurrentProcess(), pi.hProcess,
    GetCurrentProcess(), (PHANDLE) &service->svc_handle,
    0, TRUE, DUPLICATE_SAME_ACCESS);

CloseHandle (pi.hThread);
CloseHandle (pi.hProcess);

service->svc_input = (void*) my_input;
service->svc_output = (void*) my_output;
}
static void service_get (
    SVC         service,
    SCHAR       *buffer,
    USHORT      length,
    USHORT      flags,
    USHORT      timeout,
    USHORT      *return_length)
{
/**************************************
 *
 *      s e r v i c e _ g e t           ( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *      Get input from a service.  It is assumed
 *      that we have checked out of the scheduler.
 *
 **************************************/
SHORT  iter;
SLONG   n = 0L;
SCHAR   *buf = buffer;
USHORT  bytes_read;
/* Kludge for PeekNamedPipe to work on Win NT 3.5 */
UCHAR   temp_buf[600];
SLONG   temp_len;
BOOLEAN windows_nt = ISC_get_ostype();

*return_length = 0;
service->svc_flags &= ~SVC_timeout;
IS_SERVICE_RUNNING (service)

if (timeout)
    {
    /* If a timeout period was given, check every .1 seconds to see if
       input is available from the pipe.  When something shows up, read
       what's available until all data has been read, or timeout occurs.  
       Otherwise, set the timeout flag and return.
       Fall out of the loop if a BROKEN_PIPE error occurs. 
    */
    iter = timeout * 10;
    while ((iter--) && ((buf - buffer) < length))
	{
	/* PeekNamedPipe sometimes return wrong &n, need to get real
	   length from &temp_len until it works */
	if (windows_nt && !PeekNamedPipe ((HANDLE) service->svc_input, temp_buf, 600,
		&temp_len, &n, NULL))
	    {
	    if (GetLastError() == ERROR_BROKEN_PIPE)
		{
		service->svc_flags |= SVC_finished;
		break;
		}
	    io_error ("PeekNamedPipe", GetLastError(), "service pipe",
            isc_io_read_err, TRUE);
	    }
    else
        {
        DWORD dwCurrentFilePosition;
        /* Set the file pointer to the beginning of the file if we are at the end of the
           file.*/
        temp_len = GetFileSize (service->svc_input, NULL);
        dwCurrentFilePosition = SetFilePointer(service->svc_input, 0, NULL, FILE_CURRENT);

        if (temp_len && temp_len == dwCurrentFilePosition)
            SetFilePointer(service->svc_input, 0, NULL, FILE_BEGIN);
        }

	if (temp_len)
	    {
	    /* If data is available, read as much as will fit in buffer */
	    temp_len = MIN (temp_len, length - (buf - buffer));
	    bytes_read = service_read (service, buf, (USHORT)temp_len, flags);
	    buf += bytes_read;
	    
	    if (bytes_read < temp_len || service->svc_flags & SVC_finished)
		{
		/* We stopped w/out reading full length, must have hit
		   a newline or special character. */
		break;
		}
	    }
	else    
	    {
		/* PeekNamedPipe() is not returning ERROR_BROKEN_PIPE in WIN95. So,
		   we are going to use WaitForSingleObject() to check if the process
		   on the other end of the pipe is still active. */
		if (!windows_nt && 
			WaitForSingleObject((HANDLE)service->svc_handle, 1) != WAIT_TIMEOUT)
			{
			service->svc_flags |= SVC_finished;
			break;
			}

	    /* No data, so wait a while */
	    Sleep (100);
	    }
	}
    /* If we timed out, set the appropriate flags */
    if (iter < 0 && !(service->svc_flags & SVC_finished))
	service->svc_flags |= SVC_timeout;
    }
else
    {
    buf += service_read (service, buf, length, flags);
    }

*return_length = buf - buffer;
}

static void service_put (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	s e r v i c e _ p u t		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Send output to a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/
SLONG	n;

IS_SERVICE_RUNNING (service)

while (length)
    {
    if (!WriteFile ((HANDLE) service->svc_output, buffer, (SLONG) length,
            &n, NULL))
	    io_error ("WriteFile", GetLastError(), "service pipe",
                isc_io_write_err, TRUE);
    length -= (USHORT)n;
    buffer += n;
    }

if (!FlushFileBuffers ((HANDLE) service->svc_output))
    io_error ("FlushFileBuffers", GetLastError(), "service pipe",
            isc_io_write_err, TRUE);
}

static USHORT service_read (
    SVC         service,
    SCHAR       *buffer,
    USHORT      length,
    USHORT      flags)
{
/**************************************
 *
 *      s e r v i c e _ r e a d         ( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *      Read data from the named pipe.
 *      Returns true if there is more data to be
 *      read, false if we found the newline or
 *      special character.
 *
 **************************************/
SLONG   len, n = 0L;
SCHAR   *buf;

buf = buffer;

while (length)
    {
    n = 0;
    len = (flags & GET_BINARY) ? length : 1;
    if (ReadFile ((HANDLE) service->svc_input, buf, len, &n, NULL) ||
        GetLastError() == ERROR_BROKEN_PIPE)
	{
	if (!n)
	    {
	    service->svc_flags |= SVC_finished;
	    break;
	    }
	length -= (USHORT)n;
	buf += n;
	if (((flags & GET_LINE) && buf [-1] == '\n') ||
	    (!(flags & GET_BINARY) && buf [-1] == '\001'))
	    break;
	}
    else
	io_error ("ReadFile", GetLastError(), "service pipe", isc_io_read_err,
            TRUE);
    }

return buf - buffer;
}
#endif	/* ifndef SUPERSERVER */
#endif	/* WIN_NT */

#ifdef OS2_ONLY
#ifndef SUPERSERVER
#define PIPE_OPERATIONS
static void service_close (
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ c l o s e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Shutdown the connection to a service.
 *	Simply close the input and output pipes.
 *
 **************************************/

DosClose ((HFILE) service->svc_input);
DosClose ((HFILE) service->svc_output);
}

static void service_fork (
    TEXT	*service_path,
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ f o r k		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Startup a service.
 *
 **************************************/
TEXT	*argv_data, argv_data_buf [512], *p, *q, *arg;
USHORT	len, quote_flag;
HFILE	save_input, save_output, save_error, my_input, my_output, my_error,
	pipe_input, pipe_output, temp_input, temp_output, temp_error;
SLONG	status, result_codes [2];
 
/* Set up input and output pipes */

my_input = pipe_output = my_output = pipe_input = (HFILE) -1;
if ((status = DosCreatePipe (&my_input, &pipe_output, 1024)) ||
    (status = DosCreatePipe (&pipe_input, &my_output, 1024)))
    {
    DosClose (my_input);
    DosClose (pipe_output);
    DosClose (my_output);
    DosClose (pipe_input);
    ERR_post (isc_sys_request, isc_arg_string, "DosCreatePipe",
	SYS_ERR, status, 0);
    }

/* Make sure we have buffers that are large enough to hold the number
   and size of the command line arguments.  Add some extra space for
   the pipe's file handles. */

len = strlen (service_path) + strlen (service->svc_switches) + 16;
for (p = service->svc_switches; *p;)
    if (*p++ == ' ')
	len += 2;
if (len > sizeof (argv_data_buf))
    argv_data = gds__alloc ((SLONG) len);
else
    argv_data = argv_data_buf;
/* FREE: at procedure return */ 
if (!argv_data) /* NOMEM: */
    ERR_post (isc_virmemexh, 0);

/* Create a command line. */

for (p = argv_data, q = service_path; *p++ = *q++;)
    ;

q = service->svc_switches;
while (*q == ' ')
    q++;
while (*q)
    {
    arg = p;
    *p++ = '\"';
    quote_flag = FALSE;
    while ((*p = *q++) && *p != ' ')
	{
	if (*p == '\\' && *q == ' ')
	    {
	    *p = ' ';
	    q++;
	    quote_flag = TRUE;
	    }
	p++;
	}
    if (!quote_flag)
	*arg = ' ';
    else
	{
	p [1] = p [0];
	*p++ = '\"';
	}
    if (!*p)
	break;
    *p++ = ' ';
    while (*q == ' ')
	q++;
    }

*p++ = 0;
*p = 0;

THREAD_EXIT;

/* Make the input and output pipes the ib_stdin, ib_stdout, and ib_stderr of
   the forked process */

save_input = save_output = save_error = (HFILE) -1;
DosDupHandle (0, &save_input);
DosDupHandle (1, &save_output);
DosDupHandle (2, &save_error);
temp_input = 0;
temp_output = 1;
temp_error = 2;
DosDupHandle (pipe_input, &temp_input);
DosDupHandle (pipe_output, &temp_output);
DosDupHandle (pipe_output, &temp_error);
DosClose (pipe_input);
DosClose (pipe_output);

status = DosExecPgm (NULL, 0, EXEC_BACKGROUND,
    argv_data, NULL, result_codes, service_path);

DosDupHandle (save_input, &temp_input);
DosDupHandle (save_output, &temp_output);
DosDupHandle (save_output, &temp_error);
DosClose (save_input);
DosClose (save_output);
DosClose (save_error);

THREAD_ENTER;

if (argv_data != argv_data_buf)
    gds__free (argv_data);

if (status)
    ERR_post (isc_sys_request, isc_arg_string, "DosExecPgm", SYS_ERR, status, 0);

service->svc_handle = result_codes [0];

service->svc_input = (void*) my_input;
service->svc_output = (void*) my_output;
}

static void service_get (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length,
    USHORT	flags,
    USHORT	timeout,
    USHORT	*return_length)
{
/**************************************
 *
 *	s e r v i c e _ g e t		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Get input from a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/
USHORT	iter;
ULONG	len, n, status;
SCHAR	*buf;

IS_SERVICE_RUNNING (service)

if (timeout)
    {
    /* If a timeout period was given, check every .1 seconds to see if
       input is available from the pipe.  When something shows up, break
       out of the loop.  Otherwise, set the timeout flag and return.
       Fall out of the loop if a BROKEN_PIPE error occurs. */

/****
    iter = timeout * 10;
    while (iter--)
	{
	if (status = PeekNamedPipe ((HFILE) service->svc_input, NULL, 0, NULL, &n))
	    {
	    if (status == ERROR_BROKEN_PIPE)
    		break;
	    io_error ("PeekNamedPipe", status, "service pipe", isc_io_read_err,
                TRUE);
	    }
	if (n)
	    break;
	DosSleep (100);
	}
    if (!n && status != ERROR_BROKEN_PIPE)
	{
	service->svc_flags |= SVC_timeout;
	return;
	}
****/
    }

service->svc_flags &= ~SVC_timeout;

buf = buffer;
while (length)
    {
    n = 0;
    len = (flags & GET_BINARY) ? length : 1;
    if (status = DosRead ((HFILE) service->svc_input, buf, len, &n))
	{
	if (!n)
	    {
	    service->svc_flags |= SVC_finished;
	    break;
	    }
	length -= n;
	buf += n;
	if (((flags & GET_LINE) && buf [-1] == '\n') ||
	    (!(flags & GET_BINARY) && buf [-1] == '\001'))
	    break;
	}
    else
    	io_error ("DosRead", status, "service pipe", isc_io_read_err, TRUE);
    }

*return_length = buf - buffer;
}

static void service_put (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	s e r v i c e _ p u t		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Send output to a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/
ULONG	n, status;

IS_SERVICE_RUNNING (service)

while (length)
    {
    if (status = DosWrite ((HFILE) service->svc_output, buffer, (ULONG) length, &n))
    	io_error ("DosWrite", status, "service pipe", isc_io_write_err, TRUE);
    length -= n;
    buffer += n;
    }

if (status = DosResetBuffer ((HFILE) service->svc_output))
    io_error ("DosResetBuffer", status, "service pipe", isc_io_write_err, TRUE);
}
#endif	/* ifndef SUPERSERVER */
#endif	/* OS2 */

#ifdef SUPERSERVER
#define PIPE_OPERATIONS

static USHORT service_add_one (USHORT i)
{
/**************************************
 *
 *	s e r v i c e _ a d d _ o n e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
return((i % SVC_STDOUT_BUFFER_SIZE) + 1);
}

static USHORT service_empty (SVC service)
{
/**************************************
 *
 *	s e r v i c e _ e m p t y
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
if (service_add_one(service->svc_stdout_tail) == service->svc_stdout_head)
 return(1);
else
 return(0);
}

static USHORT service_full (SVC service)
{
/**************************************
 *
 *	s e r v i c e _ f u l l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
if (service_add_one(service_add_one(service->svc_stdout_tail)) == 
    service->svc_stdout_head)
 return(1);
else
 return(0);
}

static UCHAR service_dequeue_byte (
    SVC		service)	
{
/**************************************
 *
 *	s e r v i c e _ d e q u e u e _ b y t e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
UCHAR	ch;

ch = service->svc_stdout [service->svc_stdout_head];
service->svc_stdout_head = service_add_one(service->svc_stdout_head);

return (ch);
}

static void service_enqueue_byte (
    UCHAR	ch,
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ e n q u e u e _ b y t e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
/* Wait for space in buffer while service is not detached. */
while (service_full(service) && !(service->svc_flags & SVC_detached))
    THREAD_YIELD;

/* Ensure that service is not detached. */
if (!(service->svc_flags & SVC_detached))
    {
    service->svc_stdout_tail = service_add_one(service->svc_stdout_tail);
    service->svc_stdout [service->svc_stdout_tail] = ch;
    }    
}

static void service_fork (
    void	(*service_executable)(),
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ f o r k
 *
 **************************************
 *
 * Functional description
 *	Startup a service.
 *
 **************************************/
USHORT	argc;
TEXT	**arg, *p, *q;

for (argc = 2, p = service->svc_switches; *p;)
    if (*p++ == ' ')
	argc++;

service->svc_argc = argc;

arg = (TEXT**) gds__alloc ((SLONG) ((argc + 1) * sizeof (TEXT*)));
service->svc_argv = arg;
/* FREE: at SVC_detach() - Possible memory leak if ERR_post() occurs */
if (!arg)		/* NOMEM: */
    ERR_post (isc_virmemexh, 0);

*arg++ = (void*) service_executable;

/* Break up the command line into individual arguments. */

p = q = service->svc_switches;

while (*q == ' ')
    q++;
while (*q)
    {
    *arg++ = p;
    while ((*p = *q++) && *p != ' ')
	{
	if (*p == '\\' && *q == ' ')
	    {
	    *p = ' ';
	    q++;
	    }
	p++;
	}
    if (!*p)
	break;
    *p++ = 0;
    while (*q == ' ')
	q++;
    }
*arg = NULL;

service->svc_stdout = gds__alloc ((SLONG) SVC_STDOUT_BUFFER_SIZE + 1);
/* FREE: at SVC_detach() */
if (!service->svc_stdout)		/* NOMEM: */
    ERR_post (isc_virmemexh, 0);

THREAD_EXIT;
gds__thread_start (service_executable, service, 0, 0, (void*)&service->svc_handle);
THREAD_ENTER;
}

static void service_get (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length,
    USHORT	flags,
    USHORT	timeout,
    USHORT	*return_length)
{
/**************************************
 *
 *	s e r v i c e _ g e t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
int     ch = 'Z';
ULONG   elapsed_time;
#ifdef NETWARE_386
ULONG   start_time;
ULONG   end_time;
ULONG   elapsed_seconds;
ULONG   elapsed_tenths;
#endif
#ifdef WIN_NT
time_t  start_time, end_time;
#else
struct timeval  start_time, end_time;
#endif

#ifdef NETWARE_386
start_time = GetCurrentTicks();
#else
#ifdef WIN_NT
time (&start_time);
#else
gettimeofday (&start_time, NULL);
#endif
#endif /* NETWARE */
*return_length = 0;
service->svc_flags &= ~SVC_timeout;

while (length)
    {
    if (service_empty (service))
	THREAD_YIELD;
#ifdef NETWARE_386
    end_time = GetCurrentTicks();
    elapsed_time = end_time - start_time;
    TicksToSeconds (elapsed_time, &elapsed_seconds, &elapsed_tenths);
#else
#ifdef WIN_NT
    time (&end_time);
    elapsed_time = end_time - start_time;
#else
    gettimeofday (&end_time, NULL);
    elapsed_time = end_time.tv_sec - start_time.tv_sec;
#endif
#endif /* NETWARE */

    if ((timeout) && (elapsed_time >= timeout))
	{
	service->svc_flags &= SVC_timeout;
        return;
        }

    while (!service_empty(service) && length > 0)
        {
	    ch = service_dequeue_byte (service);
	    length--;

	    /* If returning a line of information, replace all new line
	     * characters with a space.  This will ensure that the output is
	     * consistent when returning a line or to eof
	     */
	    if ((flags & GET_LINE) && (ch == '\n'))
		{
		buffer [(*return_length)++] = ' ';
		return;
		}
	    else
		buffer [(*return_length)++] = ch;
        }

    if (service_empty(service) && (service->svc_flags & SVC_finished)) 
        return;
    }
}

static void service_put (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	s e r v i c e _ p u t
 *
 **************************************
 *
 * Functional description
 *	Send output to a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/

/* Nothing */
}
#endif	/* SUPERSERVER */

#ifndef SUPERSERVER
#ifndef PIPE_OPERATIONS
static void service_close (
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ c l o s e		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Shutdown the connection to a service.
 *	Simply close the input and output pipes.
 *
 **************************************/

ib_fclose ((IB_FILE*) service->svc_input);
ib_fclose ((IB_FILE*) service->svc_output);
}

static void service_fork (
    TEXT	*service_path,
    SVC		service)
{
/**************************************
 *
 *	s e r v i c e _ f o r k		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Startup a service.
 *
 **************************************/
int		pair1 [2], pair2 [2], pid;
struct stat	stat_buf;
TEXT		**argv, *argv_buf [20], **arg,
		*argv_data, argv_data_buf [512], *p, *q;
USHORT		argc, len;

if (pipe (pair1) < 0 || pipe (pair2) < 0)
    io_error ("pipe", errno, "", isc_io_create_err, FALSE);

/* Probe service executable to see it if plausibly exists. */

if (statistics (service_path, &stat_buf) == -1)
    io_error ("stat", errno, service_path, isc_io_access_err, FALSE);

/* Make sure we have buffers that are large enough to hold the number
   and size of the command line arguments. */

for (argc = 2, p = service->svc_switches; *p;)
    if (*p++ == ' ')
	argc++;
if (argc > sizeof (argv_buf) / sizeof (TEXT*))
    argv = (TEXT**) gds__alloc ((SLONG) (argc * sizeof (TEXT*)));
else
    argv = argv_buf;
/* FREE: at procedure return */
if (!argv)			/* NOMEM: */
    ERR_post (isc_virmemexh, 0);

len = strlen (service->svc_switches) + 1;
if (len > sizeof (argv_data_buf))
    argv_data = (TEXT *) gds__alloc ((SLONG) len);
else
    argv_data = argv_data_buf;
/* FREE: at procedure return */
if (!argv_data)			/* NOMEM: */
    {
    if (argv != argv_buf)
	gds__free (argv);
    ERR_post (isc_virmemexh, 0);
    }

/* Break up the command line into individual arguments. */

arg = argv;
*arg++ = service_path;

p = argv_data;
q = service->svc_switches;

while (*q == ' ')
    q++;
while (*q)
    {
    *arg++ = p;
    while ((*p = *q++) && *p != ' ')
	{
	if (*p == '\\' && *q == ' ')
	    {
	    *p = ' ';
	    q++;
	    }
	p++;
	}
    if (!*p)
	break;
    *p++ = 0;
    while (*q == ' ')
	q++;
    }
*arg = NULL;

/* At last we can fork the sub-process.  If the fork succeeds, repeat
   it so that we don't have defunct processes hanging around. */

THREAD_EXIT;

switch (pid = vfork())
    {
    case -1:
	THREAD_ENTER;
	if (argv != argv_buf)
	    gds__free (argv);
	if (argv_data != argv_data_buf)
	    gds__free (argv_data);
	ERR_post (isc_sys_request, isc_arg_string, "vfork", SYS_ERR, errno, 0);
	break;

    case 0:
	if (vfork() != 0)
	    _exit (FINI_OK);

	close (pair1 [0]);
	close (pair2 [1]);
	if (pair2 [0] != 0)
	    {
	    close (0);
	    dup (pair2 [0]);
	    close (pair2 [0]);
	    }
	if (pair1 [1] != 1)
	    {
	    close (1);
	    dup (pair1 [1]);
	    close (pair1 [1]);
	    }
	close (2);
	dup (1);
	execvp (argv [0], argv);
	_exit (FINI_ERROR);
    }

close (pair1 [1]);
close (pair2 [0]);

waitpid (pid, NULL, 0);

THREAD_ENTER;

if (argv != argv_buf)
    gds__free (argv);
if (argv_data != argv_data_buf)
    gds__free (argv_data);

if (!(service->svc_input = (void*) ib_fdopen (pair1 [0], "r")) ||
    !(service->svc_output = (void*) ib_fdopen (pair2 [1], "w")))
    io_error ("ib_fdopen", errno, "service path", isc_io_access_err, FALSE);
}

static void service_get (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length,
    USHORT	flags,
    USHORT	timeout,
    USHORT	*return_length)
{
/**************************************
 *
 *	s e r v i c e _ g e t		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Get input from a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/
#ifdef SYSV_SIGNALS
SLONG			sv_timr;
void			*sv_hndlr;
#else
struct itimerval	sv_timr;
#ifndef SIGACTION_SUPPORTED
struct sigvec		sv_hndlr;
#else
struct sigaction	sv_hndlr;
#endif
#endif
int			c;
USHORT			timed_out;
SCHAR			*buf;
SSHORT			iter = 0;
int			errno_save;

IS_SERVICE_RUNNING (service)

errno = 0;
service->svc_flags &= ~SVC_timeout;
buf = buffer;

if (timeout)
    {
    ISC_set_timer ((SLONG) (timeout * 100000), timeout_handler, service,
        &sv_timr, &sv_hndlr);
    iter = timeout * 10;
    }

while (!timeout || iter)
    {
    if ((c = ib_getc ((IB_FILE*) service->svc_input)) != EOF)
	{
	*buf++ = c;
        if (!(--length))
	    break;
	if (((flags & GET_LINE) && c == '\n') ||
	    (!(flags & GET_BINARY) && c == '\001'))
	    break;
	}
    else
	if (!errno)
	    {
	    service->svc_flags |= SVC_finished;
	    break;
	    }
	else if (SYSCALL_INTERRUPTED(errno))
	    {
	    if (timeout)
		--iter;
	    }
	else
	    {
	    errno_save = errno;
    	    if (timeout)
	       ISC_reset_timer (timeout_handler, service, &sv_timr, &sv_hndlr);
	    io_error ("ib_getc", errno_save, "service pipe", isc_io_read_err, TRUE);
	    }
    }

if (timeout)
    {
    ISC_reset_timer (timeout_handler, service, &sv_timr, &sv_hndlr);
    if (!iter)
	service->svc_flags |= SVC_timeout;
    }

*return_length = buf - buffer;
}

static void service_put (
    SVC		service,
    SCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	s e r v i c e _ p u t		( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *	Send output to a service.  It is assumed
 *	that we have checked out of the scheduler.
 *
 **************************************/

IS_SERVICE_RUNNING (service)

while (length--)
    {
    if (ib_putc (*buffer, (IB_FILE*) service->svc_output) != EOF)
	buffer++;
    else
	if (SYSCALL_INTERRUPTED(errno))
	    {
	    ib_rewind ((IB_FILE*) service->svc_output);
	    length++;
	    }
	else
	    io_error ("ib_putc", errno, "service pipe", isc_io_write_err, TRUE);
    }

if (ib_fflush ((IB_FILE*) service->svc_output) == EOF)
    io_error ("ib_fflush", errno, "service pipe", isc_io_write_err, TRUE);
}
#endif	/* ifndef PIPE_OPERATIONS */
#endif	/* ifndef SUPERSERVER */

static void timeout_handler (
    SVC		service)
{
/**************************************
 *
 *	t i m e o u t _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Called when no input has been received
 *	through a pipe for a specificed period
 *	of time.
 *
 **************************************/
}

void SVC_cleanup (
    SVC		service)
{
/**************************************
 *
 *	S V C _ c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Cleanup memory used by service.
 *
 **************************************/

#ifndef SUPERSERVER
/* If we forked an executable, close down it's pipes */

if (service->svc_flags & SVC_forked)
    service_close (service);
#else

if (service->svc_stdout)
    {
    gds__free ((SLONG*) service->svc_stdout);
    service->svc_stdout = NULL;
    }
if (service->svc_argv)
    {
    gds__free ((SLONG*) service->svc_argv);
    service->svc_argv = NULL;
    }
#endif

if (service->svc_resp_buf)
    gds__free ((SLONG*)service->svc_resp_buf);

if (service->svc_switches != NULL)
    gds__free ((SLONG*)service->svc_switches);

if (service->svc_status != NULL)
    gds__free ((SLONG*)service->svc_status);

gds__free ((SLONG*)service);
}

void SVC_finish (
    SVC		service,
    USHORT	flag)
{
/**************************************
 *
 *	S V C _ f i n i s h
 *
 **************************************
 *
 * Functional description
 *	o Set the flag (either SVC_finished for the main service thread or
 *	  SVC_detached for the client thread).
 *	o If both main thread and client thread are completed that is main
 *	  thread is finished and client is detached then free memory
 *	  used by service.
 *
 **************************************/

if (!svc_initialized)
    {
    THD_MUTEX_INIT(svc_mutex);
    svc_initialized = TRUE;
    }

THD_MUTEX_LOCK(svc_mutex);
if (service && ((flag == SVC_finished) || (flag == SVC_detached)))
    {
    service->svc_flags |= flag;
    if ((service->svc_flags & SVC_finished) &&
	(service->svc_flags & SVC_detached))
	SVC_cleanup (service);
    else
        {
	if (service->svc_flags & SVC_finished)
            {
            service->svc_flags &= ~SVC_thd_running;
            service->svc_handle = 0;
            }
	}
    }
THD_MUTEX_UNLOCK(svc_mutex);
}

static void conv_switches (
    USHORT	spb_length,
    USHORT  opt_switch_len,
    SCHAR	*spb,
    TEXT	**switches )
{
/**************************************
 *
 *	c o n v _ s w i t c h e s
 *
 **************************************
 *
 * Functional description
 *	Convert spb flags to utility switches.
 *
 **************************************/
USHORT          total;
TEXT            *p;

p = spb;

if (*p < isc_action_min || *p > isc_action_max)
    return;  /* error action not defined */

/* Calculate the total length */
if ( (total = process_switches (spb_length, p, (TEXT*)NULL)) == 0)
    return;

*switches = (TEXT *) gds__alloc (total + opt_switch_len + sizeof (SERVICE_THD_PARAM) + 1);
/* NOMEM: return error */
if (!*switches)
    ERR_post (isc_virmemexh, 0);

strcpy (*switches, SERVICE_THD_PARAM);
strcat (*switches, " ");
process_switches (spb_length, p, *switches + strlen (SERVICE_THD_PARAM) + 1);
return;
}

static TEXT     *find_switch (
    int         in_spb_sw,
    IN_SW_TAB   table )
{
/**************************************
 *
 *      f i n d _ s w i t c h
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
IN_SW_TAB       in_sw_tab;

for (in_sw_tab = table; in_sw_tab->in_sw_name; in_sw_tab++)
    {
    if (in_spb_sw == in_sw_tab->in_spb_sw)
        return in_sw_tab->in_sw_name;
    }

return ((TEXT *)NULL);
}

static  USHORT  process_switches (
    USHORT      spb_length,
    SCHAR       *spb,
    TEXT        *switches )
{
/**************************************
 *
 *      p r o c e s s _ s w i t c h e s
 *
 **************************************
 *
 *Functional description
 *   Loop through the appropriate switch table
 *   looking for the text for the given command switch.
 *
 *   Calling this function with switches = NULL returns
 *   the number of bytes to allocate for the switches and
 *   parameters.
 *
 **************************************/
USHORT          len, total;
TEXT            *p, *sw;
ISC_USHORT      svc_action;
BOOLEAN         found = FALSE, lic_key = FALSE, lic_id = FALSE;

if (spb_length == 0)
    return 0;

p = spb;
len = spb_length;
sw = switches;

svc_action = *p;

if (len > 1)
    {
    ++p;
    --len;
    }

total = 0;      /* total length of the command line */

while (len > 0)
    {
    switch (svc_action)
	{
	case isc_action_svc_delete_user:
        case isc_action_svc_display_user:
	    if (!found)
		{
		if (spb != p)
		    {
		    --p;
		    ++len;
		    }
		assert (spb == p);
		if (!get_action_svc_parameter (&p, gsec_action_in_sw_table,
					       &sw, &total, &len))
		    return 0;
		else
		    {
		    found = TRUE;
		    /* in case of "display all users" the spb buffer contains 
		       nothing but isc_action_svc_display_user */
		    if (len == 0)
			break;
		    }
		}

	    switch (*p)
		{
		case isc_spb_sec_username:
		    ++p; --len;
		    get_action_svc_string (&p, &sw, &total, &len);
		    break;
                
		default:
		    return 0;
		    break;
		}
	    break;

	case isc_action_svc_add_user:
	case isc_action_svc_modify_user:
	    if (!found)
                {
		if (spb != p)
		    {
		    --p;
		    ++len;
		    }
		assert (spb == p);
                if (!get_action_svc_parameter (&p, gsec_action_in_sw_table,
					       &sw, &total, &len))
		    return 0;
                else
		    {
                    found = TRUE;
		    if (*p != isc_spb_sec_username)
			{
			/* unexpected service parameter block:
			   expected %d, encountered %d */
			ERR_post (isc_unexp_spb_form, isc_arg_string,
				  SVC_err_string (SPB_SEC_USERNAME, strlen (SPB_SEC_USERNAME)), 0);
			}
		    }
		}

	    switch (*p)
		{
		case isc_spb_sec_userid:
		case isc_spb_sec_groupid:
		if (!get_action_svc_parameter (&p, gsec_in_sw_table,
					       &sw, &total, &len))
		    return 0;	
		get_action_svc_data (&p, &sw, &total, &len);
                break;
                
            case isc_spb_sec_username:
		++p; --len;
		get_action_svc_string (&p,  &sw, &total, &len);
		break;
	    case isc_spb_sql_role_name:
            case isc_spb_sec_password:
            case isc_spb_sec_groupname:
            case isc_spb_sec_firstname:
            case isc_spb_sec_middlename:
            case isc_spb_sec_lastname:
		if (!get_action_svc_parameter (&p, gsec_in_sw_table,
					       &sw, &total, &len))
		    return 0;
		get_action_svc_string (&p, &sw, &total, &len);
                break;
                
            default:
                return 0;
                break;
            }
        break;
	
	case isc_action_svc_db_stats:
	    switch (*p)
		{
		case isc_spb_dbname:
		    ++p; --len;
		    get_action_svc_string (&p, &sw, &total, &len);
		    break;

		case isc_spb_options:
		    ++p; --len;
		    if (!get_action_svc_bitmask(&p, dba_in_sw_table,
						&sw, &total, &len))
			return 0;
		    break;
		default:
		    return 0;
		    break;
	        }
  	    break;
	
	case isc_action_svc_backup:
	case isc_action_svc_restore:
	    switch (*p)
		{
	        case isc_spb_bkp_file:
		case isc_spb_dbname:
		case isc_spb_sql_role_name:
		    ++p; --len;
		    get_action_svc_string (&p, &sw, &total, &len);
		    break;
                case isc_spb_options:
		    ++p; --len;
		    if (!get_action_svc_bitmask(&p, burp_in_sw_table,
						&sw, &total, &len))
			return 0;
		    break;
                case isc_spb_bkp_length:
                case isc_spb_res_length:
		    ++p; --len;
		    get_action_svc_data (&p, &sw, &total, &len);
		    break;
		case isc_spb_bkp_factor:
		case isc_spb_res_buffers:
		case isc_spb_res_page_size:
		    if (!get_action_svc_parameter (&p, burp_in_sw_table,
						   &sw, &total, &len))
			return 0;
		    get_action_svc_data (&p, &sw, &total, &len);
		    break;
		case isc_spb_res_access_mode:
		    ++p; --len;
		    if (!get_action_svc_parameter (&p, burp_in_sw_table,
						   &sw, &total, &len))
			return 0;
		    break;
                case isc_spb_verbose:
		    if (!get_action_svc_parameter (&p, burp_in_sw_table,
						   &sw, &total, &len))
			return 0;
		    break;
                default:
                    return 0;
                    break;
                }
	    break;

	case isc_action_svc_repair:
	case isc_action_svc_properties:
	    switch (*p)
		{
		case isc_spb_dbname:
		    ++p; --len;
		    get_action_svc_string (&p, &sw, &total, &len);
		    break;
		case isc_spb_options:
		    ++p; --len;
		    if (!get_action_svc_bitmask(&p, alice_in_sw_table,
						&sw, &total, &len))
			return 0;
		    break;
		case isc_spb_prp_page_buffers:
		case isc_spb_prp_sweep_interval:
		case isc_spb_prp_shutdown_db:
		case isc_spb_prp_deny_new_attachments:
		case isc_spb_prp_deny_new_transactions:
		case isc_spb_prp_set_sql_dialect:
		case isc_spb_rpr_commit_trans:
		case isc_spb_rpr_rollback_trans:
		case isc_spb_rpr_recover_two_phase:
		    if (!get_action_svc_parameter (&p, alice_in_sw_table,
						   &sw, &total, &len))
			return 0;
		    get_action_svc_data (&p, &sw, &total, &len);
		    break;
		case isc_spb_prp_write_mode:
		case isc_spb_prp_access_mode:
		case isc_spb_prp_reserve_space:
		    ++p; --len;
		    if (!get_action_svc_parameter (&p, alice_in_sw_table,
						   &sw, &total, &len))
			return 0;
		    break;
		default:
		    return 0;
		    break;
		}
	    break;
	default:
	    return 0;
	    break;
        }
    }

if (sw && *(sw - 1) == ' ')
    *--sw = '\0';

return total;
}

static BOOLEAN	get_action_svc_bitmask (
    TEXT	**spb,
    IN_SW_TAB	table,
    TEXT	**cmd,
    USHORT	*total,
    USHORT	*len)
{
/**************************************
 *
 *	g e t _ a c t i o n _ s v c _ b i t m a s k
 *
 **************************************
 *
 * Functional description
 *	Get bitmask from within spb buffer,
 *	find corresponding switches within specified table,
 *	add them to the command line,
 *	adjust pointers.
 *
 **************************************/
ISC_ULONG	opt, mask;
TEXT		*s_ptr;
ISC_USHORT	count;

opt = gds__vax_integer (*spb, sizeof (ISC_ULONG));
for (mask = 1, count = (sizeof (ISC_ULONG) * 8) -1; count; --count)
    {
    if (opt & mask)
	{
	if (!(s_ptr = find_switch ((opt & mask), table)))
	    return FALSE;
	else
	    {
	    if (*cmd)
		{
		sprintf (*cmd, "-%s ", s_ptr);
		*cmd += 1 + strlen(s_ptr) + 1;
		}
	    *total += 1 + strlen (s_ptr) + 1;
	    }
	}
    mask = mask << 1;
    }

*spb += sizeof (ISC_ULONG);
*len -= sizeof(ISC_ULONG);
return TRUE;
}

static void  get_action_svc_string (
    TEXT        **spb,
    TEXT        **cmd,
    USHORT      *total,
    USHORT      *len)
{
/**************************************
 *
 *      g e t _ a c t i o n _ s v c _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Get string from within spb buffer,
 *      add it to the command line, adjust pointers.
 *
 *      All string parameters are delimited by SVC_TRMNTR.  This
 *      is done to ensure that paths with spaces are handled correctly
 *      when creating the argc / argv paramters for the service.
 *
 **************************************/
ISC_USHORT      l;

l = gds__vax_integer (*spb, sizeof (ISC_USHORT));

/* Do not go beyond the bounds of the spb buffer */
if (l > *len)
    ERR_post (isc_bad_spb_form, 0);


*spb += sizeof (ISC_USHORT);
if (*cmd)
    {
    **cmd = SVC_TRMNTR;
    *cmd += 1;
    MOVE_FASTER (*spb, *cmd, l);
    *cmd += l;
    **cmd = SVC_TRMNTR;
    *cmd += 1;
    **cmd = ' ';
    *cmd += 1;
    }
*spb += l;
*total += l + 1 + 2;   /* Two SVC_TRMNTR for strings */
*len -= sizeof (ISC_USHORT) + l;
}

static void  get_action_svc_data (
    TEXT        **spb,
    TEXT        **cmd,
    USHORT      *total,
    USHORT      *len)
{
/**************************************
 *
 *      g e t _ a c t i o n _ s v c _ d a t a
 *
 **************************************
 *
 * Functional description
 *      Get data from within spb buffer,
 *      add it to the command line, adjust pointers.
 *
 **************************************/
ISC_ULONG	ll;
TEXT		buf[64];

ll = gds__vax_integer (*spb, sizeof (ISC_ULONG));
sprintf (buf, "%lu ", ll);
if (*cmd)
    {
    sprintf (*cmd, "%lu ", ll);
    *cmd += strlen(buf);
    }
*spb += sizeof (ISC_ULONG);
*total += strlen (buf) + 1;
*len -= sizeof (ISC_ULONG);
}

static BOOLEAN  get_action_svc_parameter (
    TEXT        **spb,
    IN_SW_TAB   table,
    TEXT        **cmd,
    USHORT      *total,
    USHORT      *len)
{
/**************************************
 *
 *      g e t _ a c t i o n _ s v c _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *      Get parameter from within spb buffer,
 *      find corresponding switch within specified table,
 *	add it to the command line,
 *	adjust pointers.
 *
 **************************************/
TEXT            *s_ptr;

if (!(s_ptr = find_switch (**spb, table)))
    return FALSE;

if (*cmd)
    {
    sprintf (*cmd, "-%s ", s_ptr);
    *cmd += 1 + strlen(s_ptr) + 1;
    }

*spb += 1;
*total += 1 + strlen(s_ptr) + 1;
*len -= 1;
return TRUE;
}

#ifdef DEBUG
/* The following two functions are temporary stubs and will be
 * removed as the services API takes shape.  They are used to 
 * test that the paths for starting services and parsing command-lines
 * are followed correctly.
 */
void test_thread (SVC service)
{
    gds__log("Starting service");  
}

void test_cmd (USHORT spb_length, SCHAR *spb, TEXT **switches)
{
    gds__log("test_cmd called");
}
#endif
