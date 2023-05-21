/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		jrd.c
 *	DESCRIPTION:	User visible entrypoints
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
#include "../jrd/ibsetjmp.h"
#include <string.h>
#include <stdlib.h>
#include "../jrd/common.h"
#include <stdarg.h>
#ifndef	WIN_NT
#include <unistd.h>
#include <pwd.h>
#endif
#include <errno.h>

#define JRD_MAIN
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/irq.h"
#include "../jrd/isc.h"
#include "../jrd/drq.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../jrd/blb.h"
#include "../jrd/lck.h"
#include "../jrd/scl.h"
#include "../jrd/license.h"
#include "../jrd/pio.h"
#include "../jrd/ods.h"
#include "../jrd/exe.h"
#include "../jrd/val.h"
#include "../jrd/rse.h"
#include "../jrd/jrn.h"
#include "../jrd/all.h"
#include "../jrd/log.h"
#include "../jrd/fil.h"
#include "../jrd/sbm.h"
#include "../jrd/svc.h"
#include "../jrd/sdw.h"
#include "../jrd/lls.h"
#include "../jrd/cch.h"
#include "../jrd/iberr.h"
#include "../jrd/time.h"
#include "../intl/charsets.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dls_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/fun_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/iberr_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/ini_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/inuse_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/jrn_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/log_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/pio_proto.h"
#include "../jrd/sbm_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/shut_proto.h"
#include "../jrd/sort_proto.h"
#include "../jrd/svc_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/val_proto.h"
#include "../jrd/file_params.h"
#ifndef WINDOWS_ONLY
#include "../jrd/ail_proto.h"
#include "../jrd/event_proto.h"
#include "../jrd/old_proto.h"
#endif

#ifdef SERVER_SHUTDOWN
typedef struct dbf {
    struct dbf	*dbf_next;
    USHORT	dbf_length;
    TEXT	dbf_data [2];
} *DBF;

#include "../jrd/sort.h"
#ifdef WIN_NT
static void ExtractDriveLetter(TEXT*, ULONG*);
#endif
#endif	/* SERVER_SHUTDOWN */

#define	WAIT_PERIOD	-1

#ifdef SUPERSERVER
#define V4_THREADING
#endif

#ifdef NETWARE_386
#include "../jrd/nlm_thd.h"
#else
#ifdef V4_THREADING
#undef V4_INIT
#undef V4_GLOBAL_MUTEX_LOCK
#undef V4_GLOBAL_MUTEX_UNLOCK
#undef V4_MUTEX_INIT
#undef V4_MUTEX_LOCK
#undef V4_MUTEX_UNLOCK
#undef V4_MUTEX_DESTROY
#undef V4_JRD_MUTEX_LOCK
#undef V4_JRD_MUTEX_UNLOCK
#define V4_INIT			THD_INIT
#define V4_GLOBAL_MUTEX_LOCK	{THREAD_EXIT; THD_GLOBAL_MUTEX_LOCK; THREAD_ENTER;}
#define V4_GLOBAL_MUTEX_UNLOCK	THD_GLOBAL_MUTEX_UNLOCK
#define V4_MUTEX_INIT(mutx)	THD_MUTEX_INIT (mutx)
#define V4_MUTEX_LOCK(mutx)	{THREAD_EXIT; THD_MUTEX_LOCK (mutx); THREAD_ENTER;}
#define V4_MUTEX_UNLOCK(mutx)	THD_MUTEX_UNLOCK (mutx)
#define V4_MUTEX_DESTROY(mutx)	THD_MUTEX_DESTROY (mutx)
#define V4_JRD_MUTEX_LOCK(mutx)	{THREAD_EXIT; THD_JRD_MUTEX_LOCK (mutx); THREAD_ENTER;}
#define V4_JRD_MUTEX_UNLOCK(mutx) THD_JRD_MUTEX_UNLOCK (mutx)
#endif
#endif

#ifdef SUPERSERVER

extern SLONG trace_pools;
static  REC_MUTX_T  databases_rec_mutex;

#define V4_JRD_MUTEX_LOCK(mutx)
#define V4_JRD_MUTEX_UNLOCK(mutx)

#define JRD_SS_INIT_MUTEX       THD_rec_mutex_init (&databases_rec_mutex)
#define JRD_SS_DESTROY_MUTEX    THD_rec_mutex_destroy (&databases_rec_mutex)
#define JRD_SS_MUTEX_LOCK       {THREAD_EXIT;\
                                 THD_rec_mutex_lock (&databases_rec_mutex);\
                                 THREAD_ENTER;}
#define JRD_SS_MUTEX_UNLOCK     THD_rec_mutex_unlock (&databases_rec_mutex)
#define JRD_SS_THD_MUTEX_LOCK   THD_rec_mutex_lock (&databases_rec_mutex)
#define JRD_SS_THD_MUTEX_UNLOCK THD_rec_mutex_unlock (&databases_rec_mutex)

#else
#define JRD_SS_INIT_MUTEX
#define JRD_SS_DESTROY_MUTEX
#define JRD_SS_MUTEX_LOCK
#define JRD_SS_MUTEX_UNLOCK
#define JRD_SS_THD_MUTEX_LOCK
#define JRD_SS_THD_MUTEX_UNLOCK
#endif

#ifdef  WIN_NT
/* these should stop a most annoying warning */
#undef TEXT
#include <windows.h>
#undef TEXT
#define TEXT    SCHAR
#endif

#ifdef WIN_NT
#define	SYS_ERR		gds_arg_win32
#endif


#ifdef WINDOWS_ONLY
#define	SYS_ERR		gds_arg_dos
#endif

#ifdef NETWARE_386
#define SYS_ERR		gds_arg_netware
#endif


#ifndef SYS_ERR
#define SYS_ERR		gds_arg_unix
#endif


/* Option block for database parameter block */

typedef struct dpb {
    TEXT	*dpb_sys_user_name;
    TEXT	*dpb_user_name;
    TEXT	*dpb_password;
    TEXT	*dpb_password_enc;
    TEXT	*dpb_role_name;
    TEXT	*dpb_journal;
#ifdef ISC_DATABASE_ENCRYPTION
    TEXT	*dpb_key;
#endif
    TEXT	*dpb_log;
    TEXT	*dpb_wal_backup_dir;
    USHORT	dpb_wal_action;
    USHORT	dpb_online_dump;
    ULONG	dpb_old_file_size;
    USHORT	dpb_old_num_files;
    ULONG	dpb_old_start_page;
    ULONG	dpb_old_start_seqno;
    USHORT	dpb_old_start_file;
    TEXT	*dpb_old_file [MAX_OLD_FILES];
    USHORT	dpb_old_dump_id;
    SLONG	dpb_wal_chkpt_len;
    SSHORT	dpb_wal_num_bufs;
    USHORT	dpb_wal_bufsize;
    SLONG	dpb_wal_grp_cmt_wait;
    SLONG	dpb_sweep_interval;
    ULONG	dpb_page_buffers;
    BOOLEAN	dpb_set_page_buffers;
    ULONG 	dpb_buffers;
    USHORT	dpb_debug;
    USHORT	dpb_verify;
    USHORT	dpb_sweep;
    USHORT	dpb_trace;
    USHORT	dpb_disable;
    USHORT	dpb_dbkey_scope;
    USHORT	dpb_page_size;
    USHORT	dpb_activate_shadow;
    USHORT	dpb_delete_shadow;
    USHORT	dpb_no_garbage;
    USHORT	dpb_quit_log;
    USHORT	dpb_shutdown;
    SSHORT	dpb_shutdown_delay;
    USHORT	dpb_online;
    SSHORT	dpb_force_write;
    UCHAR	dpb_set_force_write;
    UCHAR	dpb_no_reserve;
    UCHAR	dpb_set_no_reserve;
    SSHORT	dpb_interp;
    TEXT	*dpb_lc_messages;
    TEXT	*dpb_lc_ctype;
    USHORT	dpb_single_user;
    BOOLEAN	dpb_overwrite;
    BOOLEAN	dpb_sec_attach;
    BOOLEAN	dpb_disable_wal;
    SLONG	dpb_connect_timeout;
    SLONG	dpb_dummy_packet_interval;
    SSHORT	dpb_db_readonly;
    BOOLEAN	dpb_set_db_readonly;
    BOOLEAN	dpb_gfix_attach;
    BOOLEAN	dpb_gstat_attach;
    TEXT	*dpb_gbak_attach;
    TEXT	*dpb_working_directory;
    USHORT	dpb_sql_dialect;
    USHORT	dpb_set_db_sql_dialect;
} DPB;

static BLB	check_blob (TDBB, STATUS *, BLB *);
static STATUS	check_database (TDBB, ATT, STATUS *);
static void	cleanup (void *);
static STATUS	commit (STATUS *, TRA *, USHORT);
static STR	copy_string (TEXT *, USHORT);
static BOOLEAN	drop_files (FIL);
static STATUS	error (STATUS *);
static void	find_intl_charset (TDBB, ATT, DPB *);
static TRA	find_transaction (TDBB, TRA, STATUS);
static void	get_options (UCHAR *, USHORT, TEXT **, DPB *);
static SLONG	get_parameter (UCHAR **);
static TEXT	*get_string_parameter (UCHAR **, TEXT **);
static STATUS	handle_error (STATUS *, STATUS, TDBB);
#if defined (WIN_NT) && !defined(SERVER_SHUTDOWN)
static BOOLEAN  handler_NT (SSHORT);
#endif
static DBB	init (TDBB, STATUS *, TEXT *, USHORT);
static void	make_jrn_data (UCHAR *, USHORT *, TEXT *, USHORT, TEXT *, USHORT);
static STATUS	prepare (TDBB, TRA, STATUS *, USHORT, UCHAR *);
static void	release_attachment (ATT);
static STATUS	return_success (TDBB);
static BOOLEAN	rollback (TDBB, TRA, STATUS *, USHORT);
#if defined (WIN_NT) && !defined(SERVER_SHUTDOWN)
static void     setup_NT_handlers (void);
#endif
static void	shutdown_database (DBB, BOOLEAN);
static void	strip_quotes (TEXT *, TEXT *);
static void	purge_attachment (TDBB, STATUS *, ATT, BOOLEAN);

static int	initialized = 0;
static DBB	databases = NULL;
static MUTX_T	databases_mutex [1];
static ULONG	JRD_cache_default;
static struct ipccfg	JRD_hdrtbl [] = {
	ISCCFG_DBCACHE,	-1,	&JRD_cache_default,	0,	0,
	NULL,		0,	NULL,			0,	0
};

#ifdef GOVERNOR
#define ATTACHMENTS_PER_USER 1
static ULONG	JRD_max_users = 0;
static ULONG    num_attached = 0;
#endif /* GOVERNOR */

/* check whether we need to perform an autocommit;
   do it here to prevent committing every record update 
   in a statement */

#define CHECK_AUTOCOMMIT	if (request->req_transaction->tra_flags & TRA_perform_autocommit)\
				{ request->req_transaction->tra_flags &= ~TRA_perform_autocommit;\
				TRA_commit (tdbb, request->req_transaction, TRUE); }

#define ERROR_INIT(env)		tdbb->tdbb_setjmp = (UCHAR*) env;\
				tdbb->tdbb_status_vector = user_status;\
				if (SETJMP (env)) return error (user_status);

#define RETURN_SUCCESS		return return_success (tdbb)

#define API_ENTRY_POINT_INIT	user_status [0] = gds_arg_gds;\
				user_status [1] = SUCCESS;\
				user_status [2] = gds_arg_end

#define CHECK_HANDLE(blk,type,error) if (!blk || ((BLK) blk)->blk_type != (UCHAR) type) \
				return handle_error (user_status, error, tdbb)

#define NULL_CHECK(ptr,code)	if (*ptr) return handle_error (user_status, code, tdbb)

#define SWEEP_INTERVAL		20000
#define	DPB_EXPAND_BUFFER	2048

#define	DBL_QUOTE		'\042'
#define	SINGLE_QUOTE		'\''
#define	BUFFER_LENGTH128	128
BOOLEAN	invalid_client_SQL_dialect = FALSE;

#define GDS_ATTACH_DATABASE	jrd8_attach_database
#define GDS_BLOB_INFO		jrd8_blob_info
#define GDS_CANCEL_BLOB		jrd8_cancel_blob
#define GDS_CANCEL_EVENTS	jrd8_cancel_events
#define GDS_CANCEL_OPERATION	jrd8_cancel_operation
#define GDS_CLOSE_BLOB		jrd8_close_blob
#define GDS_COMMIT		jrd8_commit_transaction
#define GDS_COMMIT_RETAINING	jrd8_commit_retaining
#define GDS_COMPILE		jrd8_compile_request
#define GDS_CREATE_BLOB2	jrd8_create_blob2
#define GDS_CREATE_DATABASE	jrd8_create_database
#define GDS_DATABASE_INFO	jrd8_database_info
#define GDS_DDL			jrd8_ddl
#define GDS_DETACH		jrd8_detach_database
#define GDS_DROP_DATABASE	jrd8_drop_database
#define GDS_GET_SEGMENT		jrd8_get_segment
#define GDS_GET_SLICE		jrd8_get_slice
#define GDS_OPEN_BLOB2		jrd8_open_blob2
#define GDS_PREPARE		jrd8_prepare_transaction
#define GDS_PUT_SEGMENT		jrd8_put_segment
#define GDS_PUT_SLICE		jrd8_put_slice
#define GDS_QUE_EVENTS		jrd8_que_events
#define GDS_RECONNECT		jrd8_reconnect_transaction
#define GDS_RECEIVE		jrd8_receive
#define GDS_RELEASE_REQUEST	jrd8_release_request
#define GDS_REQUEST_INFO	jrd8_request_info
#define GDS_ROLLBACK		jrd8_rollback_transaction
#define GDS_ROLLBACK_RETAINING	jrd8_rollback_retaining
#define GDS_SEEK_BLOB		jrd8_seek_blob
#define GDS_SEND		jrd8_send
#define GDS_SERVICE_ATTACH	jrd8_service_attach
#define GDS_SERVICE_DETACH	jrd8_service_detach
#define GDS_SERVICE_QUERY	jrd8_service_query
#define GDS_SERVICE_START   jrd8_service_start
#define GDS_START_AND_SEND	jrd8_start_and_send
#define GDS_START		jrd8_start_request
#define GDS_START_MULTIPLE	jrd8_start_multiple
#define GDS_START_TRANSACTION	jrd8_start_transaction
#define GDS_TRANSACT_REQUEST	jrd8_transact_request
#define GDS_TRANSACTION_INFO	jrd8_transaction_info
#define GDS_UNWIND		jrd8_unwind_request

/* External hook definitions */

#ifdef ISC_DATABASE_ENCRYPTION
#ifdef WINDOWS_ONLY
#define ENCRYPT_IMAGE	"IUTLS.DLL"
#define ENCRYPT		"ISC_CRYPT"
#define DECRYPT		"ISC_CRYPT"
#else
#define ENCRYPT_IMAGE	"ISCCRYPT"
#define ENCRYPT		"ISC_ENCRYPT"
#define DECRYPT		"ISC_DECRYPT"
#endif /* WINDOWS_ONLY */
#endif

#ifdef SHLIB_DEFS
/**  Use shdef.h which has all shared lib defs **/
#include "shdef.h"
#endif

/* every GDS function now has a DLL_EXPORT */

STATUS DLL_EXPORT GDS_ATTACH_DATABASE (
    STATUS	*user_status,
    SSHORT	file_length,
    SCHAR	*file_name,
    ATT		*handle,
    SSHORT	dpb_length,
    SCHAR	*dpb,
    SCHAR	*expanded_filename)
{
/**************************************
 *
 *	g d s _ $ a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Attach a moldy, grungy, old database
 *	sullied by user data.
 *
 **************************************/
DBB		dbb;
#ifdef STACK_REDUCTION
TEXT		*allocated_space=NULL;
TEXT		*archive_name, *journal_name, *journal_dir;
UCHAR		*data;
#else
TEXT		opt_buffer [DPB_EXPAND_BUFFER], archive_name [MAX_PATH_LENGTH];
TEXT		journal_name [MAX_PATH_LENGTH], journal_dir [MAX_PATH_LENGTH];
UCHAR		data [MAX_PATH_LENGTH];
#endif
SSHORT		fl, length_expanded, first;
STATUS		temp_status [ISC_STATUS_LENGTH], *status;
TEXT		*opt_ptr;
DPB		options;
ATT		attachment;
SBM		sbm_recovery = NULL;
USHORT		d_len, jd_len;
#ifdef V4_THREADING
BOOLEAN		initing_security;
#endif
JMP_BUF		env;
#ifdef _PPC_
JMP_BUF		env1;
#endif
struct tdbb	thd_context, *tdbb = NULL;

TEXT		end_quote, local_role_name [BUFFER_LENGTH128];
UCHAR		*p1, *p2;
SSHORT		cnt, len;
BOOLEAN		delimited_done = FALSE;

API_ENTRY_POINT_INIT;

if (*handle)
    return handle_error (user_status, gds__bad_db_handle, NULL_PTR);

/* Get length of database file name, if not already known */

fl = (file_length) ? file_length : strlen (file_name);
length_expanded = strlen (expanded_filename);

#ifdef mpexl
/* See if the filename contains a node name - if so, don't access locally. */

strncpy (opt_buffer, file_name, fl);
opt_buffer [fl] = 0;
if (ISC_check_if_remote (opt_buffer, TRUE))
    return handle_error (user_status, gds__unavailable, NULL_PTR);
#endif

SET_THREAD_DATA;

/* Unless we're already attached, do some initialization */

if (!(dbb = init (tdbb, user_status, expanded_filename, TRUE)))
    {
    V4_JRD_MUTEX_UNLOCK (databases_mutex);
    JRD_SS_MUTEX_UNLOCK;
    RESTORE_THREAD_DATA;
    return user_status [1];
    }

dbb->dbb_flags |= DBB_being_opened;
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
V4_JRD_MUTEX_UNLOCK (databases_mutex);

tdbb->tdbb_database = dbb;

/* Initialize special error handling */

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = status = user_status;
tdbb->tdbb_attachment = attachment = NULL;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;

/* Count active thread in database */

++dbb->dbb_use_count;
#ifdef V4_THREADING
initing_security = FALSE;
#endif

/* The following line seems to fix a bug that appears on
   SCO EVEREST.  It probably has to do with the fact that
   the error handler below used to contain the first reference to
   variable transaction, which is actually initialized a few lines
   below that. */

attachment = *(&attachment);

if (SETJMP (env)) 
    {
#ifdef _PPC_
    tdbb->tdbb_setjmp = (UCHAR*) env1;
    if (!SETJMP (env1))
#else
    if (!SETJMP (env))
#endif
	{
#ifdef V4_THREADING
	if (initing_security)
	    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
#endif
	tdbb->tdbb_status_vector = temp_status;
	dbb->dbb_flags &= ~DBB_being_opened;
	release_attachment (attachment);

	/* At this point, mutex dbb->dbb_mutexes [DBB_MUTX_init_fini] has been
	   unlocked and mutex databases_mutex has been locked. */
#ifdef STACK_REDUCTION
        if (allocated_space) ALL_free (allocated_space);
#endif
	if (dbb->dbb_header.blk_type == (UCHAR) type_dbb)
	    if (!dbb->dbb_attachments)
		shutdown_database (dbb, TRUE);
	    else if (attachment)
		ALL_RELEASE (attachment);
	V4_JRD_MUTEX_UNLOCK (databases_mutex);
	}
    tdbb->tdbb_status_vector = status;
    JRD_SS_MUTEX_UNLOCK;
    return error (user_status);
    }

/* Allocate buffer space */

#ifdef STACK_REDUCTION
allocated_space = ALL_malloc (DPB_EXPAND_BUFFER + (4 * MAX_PATH_LENGTH), ERR_jmp);
opt_ptr         = allocated_space;
archive_name    = opt_ptr + MAX_PATH_LENGTH;
journal_name    = archive_name + MAX_PATH_LENGTH;
journal_dir     = journal_name + MAX_PATH_LENGTH;
data            = (UCHAR*)(journal_dir + MAX_PATH_LENGTH);
#else
opt_ptr = opt_buffer;
#endif

/* Process database parameter block */

get_options ((UCHAR *) dpb, dpb_length, &opt_ptr, &options);

#ifndef NO_NFS
/* Don't check nfs if single user */

if (!options.dpb_single_user)
#endif
    {
    /* Check to see if the database is truly local or if it just looks
       that way */

    if (ISC_check_if_remote (expanded_filename, TRUE))
	ERR_post (gds__unavailable, 0);
    }

#ifdef ISC_DATABASE_ENCRYPTION
/* Worry about encryption key */

if (dbb->dbb_decrypt)
    {
    if (dbb->dbb_filename)
	{
	if ((dbb->dbb_encrypt_key && !options.dpb_key) ||
	    (!dbb->dbb_encrypt_key && options.dpb_key) ||
	     strcmp (options.dpb_key, dbb->dbb_encrypt_key->str_data))
	    ERR_post (gds__no_priv,
		gds_arg_string, "encryption",
		gds_arg_string, "database",
		gds_arg_string, ERR_string (file_name, fl), 0);
	}
    else if (options.dpb_key)
	dbb->dbb_encrypt_key = copy_string (options.dpb_key, strlen (options.dpb_key));
    }
#endif

tdbb->tdbb_attachment = attachment = (ATT) ALLOCP (type_att);
attachment->att_database = dbb;

attachment->att_next = dbb->dbb_attachments;
dbb->dbb_attachments = attachment;
dbb->dbb_flags &= ~DBB_being_opened;
dbb->dbb_sys_trans->tra_attachment = attachment;
tdbb->tdbb_quantum = QUANTUM;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_inhibit = 0;
tdbb->tdbb_flags = NULL;

attachment->att_charset = options.dpb_interp;
if (options.dpb_lc_messages)
    attachment->att_lc_messages = copy_string (options.dpb_lc_messages, strlen (options.dpb_lc_messages)); 

if (options.dpb_no_garbage)
    attachment->att_flags |= ATT_no_cleanup;

if (options.dpb_gbak_attach)
    attachment->att_flags |= ATT_gbak_attachment;

if (options.dpb_gstat_attach)
    attachment->att_flags |= ATT_gstat_attachment;

if (options.dpb_gfix_attach)
    attachment->att_flags |= ATT_gfix_attachment;

if (options.dpb_working_directory)
    attachment->att_working_directory = copy_string (options.dpb_working_directory, strlen(options.dpb_working_directory));

/* If we're a not a secondary attachment, initialize some stuff */

first = FALSE;

LCK_init (tdbb, LCK_OWNER_attachment);   /* For the attachment */
attachment->att_flags |= ATT_lck_init_done;
if (!dbb->dbb_filename)
    {
    first = TRUE;
    dbb->dbb_filename = copy_string (expanded_filename, length_expanded);
   /* Extra LCK_init() done to keep the lock table until the
      database is shutdown() after the last detach. */
    LCK_init (tdbb, LCK_OWNER_database);
    dbb->dbb_flags |= DBB_lck_init_done;
    INI_init();
    FUN_init();
    SBM_init();
    dbb->dbb_file = PIO_open (dbb, expanded_filename, length_expanded, options.dpb_trace, NULL_PTR, file_name, fl);
    SHUT_init (dbb);
    PAG_header (file_name, fl);
    INI_init2();
    PAG_init();
    if (options.dpb_set_page_buffers)
	dbb->dbb_page_buffers = options.dpb_page_buffers;
    CCH_init (tdbb, (ULONG) options.dpb_buffers);
    PAG_init2 (0);
    if (options.dpb_disable_wal)
	{ /* Forcibly disable WAL before the next step.  
	     We have an exclusive access to the database. */
#ifndef WINDOWS_ONLY
	AIL_drop_log_force();
#endif
	options.dpb_disable_wal = FALSE;  /* To avoid further processing below */
	}
#ifdef WINDOWS_ONLY
    dbb->dbb_wal = 0;
#else
    AIL_init (expanded_filename, length_expanded, 0,
	options.dpb_activate_shadow, &sbm_recovery);
    AIL_set_log_options (
			options.dpb_wal_chkpt_len,
			options.dpb_wal_num_bufs,
			options.dpb_wal_bufsize,
			options.dpb_wal_grp_cmt_wait
			);
#endif
#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
    LOG_init (expanded_filename, length_expanded);
#endif

    /* initialize shadowing as soon as the database is ready for it
   	but before any real work is done */

    SDW_init (options.dpb_activate_shadow, options.dpb_delete_shadow, sbm_recovery);
    INI_update_database();
    }

#ifdef READONLY_DATABASE
/* Attachments to a ReadOnly database need NOT do garbage collection */
if (dbb->dbb_flags & DBB_read_only)
    attachment->att_flags |= ATT_no_cleanup;
#endif  /* READONLY_DATABASE */

if (options.dpb_disable_wal)
    ERR_post (gds__lock_timeout, gds_arg_gds, gds__obj_in_use,
              gds_arg_string, ERR_string (file_name, fl), 0);

if (options.dpb_buffers && !dbb->dbb_page_buffers)
    CCH_expand (tdbb, (ULONG) options.dpb_buffers);

if (!options.dpb_verify && CCH_exclusive (tdbb, LCK_PW, LCK_NO_WAIT))
    TRA_cleanup (tdbb);

#ifdef V4_THREADING
V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
initing_security = TRUE;
#endif

if (invalid_client_SQL_dialect)
    ERR_post (isc_inv_client_dialect_specified, isc_arg_number,
					    options.dpb_sql_dialect, 
	      isc_arg_gds, isc_valid_client_dialects,
					    isc_arg_string, "1, or 3", 0);
invalid_client_SQL_dialect = FALSE;

if (options.dpb_role_name)
    {
    switch (options.dpb_sql_dialect)
	{
	case 0:
	    {
	    /*
	    ** V6 Client --> V6 Server, dummy client SQL dialect 0 was passed
	    **   It means that client SQL dialect was not set by user
	    **   and takes DB SQL dialect as client SQL dialect
	    */
	    if (ENCODE_ODS(dbb->dbb_ods_version, dbb->dbb_minor_original)
		>= ODS_10_0)
		if ( dbb->dbb_flags & DBB_DB_SQL_dialect_3)
		    /*
		    ** DB created in IB V6.0 by client SQL dialect 3
		    */
		    options.dpb_sql_dialect = SQL_DIALECT_V6;
		else
		    /*
		    ** old DB was gbaked in IB V6.0
		    */
		    options.dpb_sql_dialect = SQL_DIALECT_V5;
	    else
		options.dpb_sql_dialect = SQL_DIALECT_V5;
	    }
	    break;
	case 99:
	    /*
	    ** V5 Client --> V6 Server, old client has no concept of dialect
	    */
	    options.dpb_sql_dialect = SQL_DIALECT_V5;
	    break;
	default:
	    /*
	    ** V6 Client --> V6 Server, but client SQL dialect was set
	    **   by user and was passed.
	    */
	    break;
	}

    switch (options.dpb_sql_dialect)
	{
	case SQL_DIALECT_V5:
	    {
	    strip_quotes (options.dpb_role_name, local_role_name);
	    len = strlen (local_role_name);
	    p1 = options.dpb_role_name;
	    for (cnt = 0; cnt < len; cnt++)
		{
		*p1++ = UPPER7 (local_role_name [cnt]);
		}
	    *p1 = '\0';
	    }
	    break;
	case SQL_DIALECT_V6_TRANSITION:
	case SQL_DIALECT_V6:
	    {
	    if (*options.dpb_role_name == DBL_QUOTE || 
		*options.dpb_role_name == SINGLE_QUOTE)
		{
		p1 = options.dpb_role_name;
		p2 = local_role_name;
		cnt = 1;
		delimited_done = FALSE;
		end_quote = *p1;
		*p1++;
		/* 
		** remove the delimited quotes and escape quote 
		** from ROLE name 
		*/
		while (*p1 && !delimited_done && cnt < BUFFER_LENGTH128-1)
		    {
		    if (*p1 == end_quote)
			{
			cnt++;
			*p2++ = *p1++;
			if (*p1 && *p1 == end_quote && cnt < BUFFER_LENGTH128-1)
			    *p1++; /* skip the escape quote here */
			else
			    {
			    delimited_done = TRUE;
			    p2--;
			    }
			}
		    else
			{
			cnt++;
			*p2++ = *p1++;
			}
		    }
		*p2 = '\0';
		strcpy (options.dpb_role_name, local_role_name);
		}
	    else 
		{
		strcpy (local_role_name, options.dpb_role_name);
		len = strlen (local_role_name);
		p1 = options.dpb_role_name;
		for (cnt = 0; cnt < len; cnt++)
		    {
		    *p1++ = UPPER7 (local_role_name [cnt]);
		    }
		*p1 = '\0';
		}
	    }
	    break;
	default:
	    break;
	}
    }

options.dpb_sql_dialect = 0;

SCL_init (FALSE, options.dpb_sys_user_name, options.dpb_user_name,
          options.dpb_password, options.dpb_password_enc,
          options.dpb_role_name, tdbb);
#ifdef V4_THREADING
initing_security = FALSE;
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
#endif

if (options.dpb_shutdown || options.dpb_online)
    {
    /* By releasing the DBB_MUTX_init_fini mutex here, we would be allowing
       other threads to proceed with their detachments, so that shutdown does
       not timeout for exclusive access and other threads don't have to wait 
       behind shutdown */

    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    JRD_SS_MUTEX_UNLOCK;
    if (!SHUT_database (dbb, options.dpb_shutdown, options.dpb_shutdown_delay))
	{
	JRD_SS_MUTEX_LOCK;
	V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
	if (user_status [1] != SUCCESS)
	    ERR_punt();
	else
	    ERR_post (gds__no_priv,
		gds_arg_string, "shutdown or online",
		gds_arg_string, "database",
		gds_arg_string, ERR_string (file_name, fl), 0);
	}
    JRD_SS_MUTEX_LOCK;
    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    }

#ifdef SUPERSERVER
/* Check if another attachment has or is requesting exclusive database access. 
   If this is an implicit attachment for the security (password) database, don't
   try to get exclusive attachment to avoid a deadlock condition which happens 
   when a client tries to connect to the security database itself. */

if (!options.dpb_sec_attach)
    {
    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    JRD_SS_MUTEX_UNLOCK;
    CCH_exclusive_attachment (tdbb, LCK_none, LCK_WAIT);
    JRD_SS_MUTEX_LOCK;
    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    if (attachment->att_flags & ATT_shutdown)
	ERR_post (gds__shutdown, gds_arg_string, ERR_string (file_name, fl), 0);
    }
#endif

/* If database is shutdown then kick 'em out. */

if (dbb->dbb_ast_flags & (DBB_shut_attach | DBB_shut_tran))
    ERR_post (gds__shutinprog, gds_arg_string, ERR_string (file_name, fl), 0);

if (dbb->dbb_ast_flags & DBB_shutdown &&
    !(attachment->att_user->usr_flags & (USR_locksmith | USR_owner)))
    ERR_post (gds__shutdown, gds_arg_string, ERR_string (file_name, fl), 0);

#ifndef WINDOWS_ONLY
if (options.dpb_disable)
    AIL_disable();
#endif

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
if (options.dpb_quit_log)
    LOG_disable();
#endif

/* Figure out what character set & collation this attachment prefers */

find_intl_charset (tdbb, attachment, &options);

if (options.dpb_verify)
    {
    if (!CCH_exclusive (tdbb, LCK_PW, WAIT_PERIOD))
	ERR_post (gds__bad_dpb_content, gds_arg_gds, gds__cant_validate, 0);

#ifdef GARBAGE_THREAD
    /* Can't allow garbage collection during database validation. */

    VIO_fini (tdbb);
#endif
    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    JRD_SS_MUTEX_UNLOCK;
    if (!VAL_validate (tdbb, options.dpb_verify))
	{
	JRD_SS_MUTEX_LOCK;
	V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
	ERR_punt();
	}
    JRD_SS_MUTEX_LOCK;
    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    }

if (options.dpb_journal)
    if (first)
	{
	archive_name [0] = 0;

	if (options.dpb_wal_backup_dir)
	    ISC_expand_filename (options.dpb_wal_backup_dir, 0, archive_name);

	make_jrn_data (data, &d_len, expanded_filename, length_expanded, 
	    archive_name, strlen (archive_name));

	ISC_expand_filename (options.dpb_journal, 0, journal_name);

#ifndef WINDOWS_ONLY
	AIL_enable (journal_name, strlen (journal_name),
		    data, d_len, strlen (archive_name));
#endif
	}
    else 
	ERR_post (gds__bad_dpb_content, gds_arg_gds, gds__cant_start_journal, 0);

if (options.dpb_wal_action)
    {
    /* Make sure WAL enabled before taking any WAL action. */

    if (!dbb->dbb_wal)
	ERR_post (gds__no_wal, 0);
    if (!CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
        ERR_post (gds__lock_timeout, gds_arg_gds, gds__obj_in_use,
	    gds_arg_string, ERR_string (file_name, fl), 0);

    /* journal has to be disabled before dropping WAL */

#ifdef WINDOWS_ONLY
    PAG_get_clump (HEADER_PAGE, HDR_journal_server, &jd_len, journal_dir);
#else
    if (PAG_get_clump (HEADER_PAGE, HDR_journal_server, &jd_len, journal_dir))
	AIL_disable();
#endif
    }

/*
 * if the attachment is through gbak and this the attachment is not by owner
 * or sysdba   then return error. This has been added here to allow for the
 * GBAK security feature of only allowing the owner or sysdba to backup a
 * database. smistry 10/5/98
 */

if (((attachment->att_flags & ATT_gbak_attachment) ||
     (attachment->att_flags & ATT_gfix_attachment) ||
     (attachment->att_flags & ATT_gstat_attachment)) && 
    !(attachment->att_user->usr_flags & (USR_locksmith | USR_owner)))
    {
    ERR_post (isc_adm_task_denied, 0);
    }
		    
if (options.dpb_online_dump)
    {
    CCH_release_exclusive (tdbb);

#ifndef WINDOWS_ONLY
    OLD_dump (expanded_filename, length_expanded, 
	    options.dpb_old_dump_id,
	    options.dpb_old_file_size,
	    options.dpb_old_start_page, 
	    options.dpb_old_start_seqno, 
	    options.dpb_old_start_file, 
	    options.dpb_old_num_files, 
	    options.dpb_old_file);
#endif
    }

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
if (options.dpb_log)
    if (first)
	{
        if (!CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
            ERR_post (gds__lock_timeout, gds_arg_gds, gds__obj_in_use,
	        gds_arg_string, ERR_string (file_name, fl), 0);
	LOG_enable (options.dpb_log, strlen (options.dpb_log));
	}
    else 
	ERR_post (gds__bad_dpb_content, gds_arg_gds, gds__cant_start_logging, 0);
#endif

if (options.dpb_set_db_sql_dialect)
    {
    PAG_set_db_SQL_dialect (dbb, options.dpb_set_db_sql_dialect);
    }

if (options.dpb_sweep_interval != -1)
    {
    PAG_sweep_interval (options.dpb_sweep_interval);
    dbb->dbb_sweep_interval = options.dpb_sweep_interval;
    }

if (options.dpb_set_force_write)
    PAG_set_force_write (dbb, options.dpb_force_write);

if (options.dpb_set_no_reserve)
    PAG_set_no_reserve (dbb, options.dpb_no_reserve);

if (options.dpb_set_page_buffers)
    PAG_set_page_buffers (options.dpb_page_buffers);

#ifdef READONLY_DATABASE
if (options.dpb_set_db_readonly)
    {
    if (!CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
	ERR_post (gds__lock_timeout, gds_arg_gds, gds__obj_in_use,
	    gds_arg_string, ERR_string (file_name, fl), 0);

    PAG_set_db_readonly (dbb, options.dpb_db_readonly);
    }
#endif

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
/* don't record the attach until now in case the log is added during the attach */

LOG_call (log_attach2, file_length, file_name, *handle, dpb_length, dpb, expanded_filename);
#endif

#ifdef GARBAGE_THREAD
VIO_init (tdbb);
#endif

CCH_release_exclusive (tdbb);

#ifdef GOVERNOR
if (!options.dpb_sec_attach) 
    {
    if (JRD_max_users)
        {
        if (num_attached < (JRD_max_users * ATTACHMENTS_PER_USER))
	    num_attached++;
        else
	    ERR_post (isc_max_att_exceeded, 0);
        }
    }
else
    {
    attachment->att_flags |= ATT_security_db;
    }
#endif /* GOVERNOR */

V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);

/* if there was an error, the status vector is all set */

if (options.dpb_sweep & gds__dpb_records)
    {
    JRD_SS_MUTEX_UNLOCK;
    if (!(TRA_sweep (tdbb, NULL_PTR)))
	{
	JRD_SS_MUTEX_LOCK;
	ERR_punt();
	}
    JRD_SS_MUTEX_LOCK;
    }

if (options.dpb_dbkey_scope)
    attachment->att_dbkey_trans = TRA_start (tdbb, 0, NULL_PTR);

JRD_SS_MUTEX_UNLOCK;

*handle = attachment;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *handle);
#endif

#ifdef STACK_REDUCTION
ALL_free (allocated_space);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_BLOB_INFO (
    STATUS	*user_status,
    BLB		*blob_handle,
    SSHORT	item_length,
    SCHAR	*items,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ b l o b _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on blob object.
 *
 **************************************/
BLB	blob;
JMP_BUF	env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (!(blob = check_blob (tdbb, user_status, blob_handle)))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_blob_info2, *blob_handle, item_length, items, buffer_length);
#endif

ERROR_INIT (env);

INF_blob_info (blob, items, item_length, buffer, buffer_length);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_CANCEL_BLOB (
    STATUS	*user_status,
    BLB		*blob_handle)
{
/**************************************
 *
 *	g d s _ $ c a n c e l _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
BLB		blob;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (*blob_handle)
    {
    if (!(blob = check_blob (tdbb, user_status, blob_handle)))
	return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
    LOG_call (log_cancel_blob, *blob_handle);
#endif

    ERROR_INIT (env);
    BLB_cancel (tdbb, blob);
    *blob_handle = NULL;
    }

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_CANCEL_EVENTS (
    STATUS	*user_status,
    ATT		*handle,
    SLONG	*id)
{
/**************************************
 *
 *	g d s _ $ c a n c e l _ e v e n t s
 *
 **************************************
 *
 * Functional description
 *	Cancel an outstanding event.
 *
 **************************************/
#ifndef WINDOWS_ONLY /* Events are not supported under LIBS */
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (check_database (tdbb, *handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_cancel_events, *handle, *id);
#endif

ERROR_INIT (env);
EVENT_cancel (*id);

RETURN_SUCCESS;
#else /* WINDOWS_ONLY */
user_status [0] = isc_arg_gds;
user_status [1] = isc_wish_list;
user_status [2] = isc_arg_end;

return user_status [1];
#endif /* WINDOWS_ONLY */
}

#ifdef CANCEL_OPERATION
STATUS DLL_EXPORT GDS_CANCEL_OPERATION (
    STATUS	*user_status,
    ATT		*handle,
    USHORT	option)
{
/**************************************
 *
 *	g d s _ $ c a n c e l _ o p e r a t i o n
 *
 **************************************
 *
 * Functional description
 *	Try to cancel an operation.
 *
 **************************************/
DBB	dbb;
ATT	attachment, attach;

API_ENTRY_POINT_INIT;

attachment = *handle;

/* Check out the database handle.  This is mostly code from
   the routine "check_database" */

if (!attachment || 
    (attachment->att_header.blk_type != (UCHAR) type_att) ||
    !(dbb = attachment->att_database) ||
    dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return handle_error (user_status, gds__bad_db_handle, NULL_PTR);

/* Make sure this is a valid attachment */

for (attach = dbb->dbb_attachments; attach; attach = attach->att_next)
    if (attach == attachment)
	break;

if (!attach)
    return handle_error (user_status, gds__bad_db_handle, NULL_PTR);

switch (option)
    {
    case CANCEL_disable:
	attachment->att_flags |= ATT_cancel_disable;
	break;

    case CANCEL_enable:
	attachment->att_flags &= ~ATT_cancel_disable;
	break;

    case CANCEL_raise:
	attachment->att_flags |= ATT_cancel_raise;
	break;

    default:
	assert (FALSE);
    }

return SUCCESS;
}
#endif

STATUS DLL_EXPORT GDS_CLOSE_BLOB (
    STATUS	*user_status,
    BLB		*blob_handle)
{
/**************************************
 *
 *	g d s _ $ c l o s e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
BLB		blob;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (!(blob = check_blob (tdbb, user_status, blob_handle)))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_close_blob, *blob_handle);
#endif

ERROR_INIT (env);
BLB_close (tdbb, blob);
*blob_handle = NULL;

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_COMMIT (
    STATUS	*user_status,
    TRA		*tra_handle)
{
/**************************************
 *
 *	g d s _ $ c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit a transaction.
 *
 **************************************/

API_ENTRY_POINT_INIT;

if (commit (user_status, tra_handle, FALSE))
    return user_status [1];

*tra_handle = NULL;

return SUCCESS;
}

STATUS DLL_EXPORT GDS_COMMIT_RETAINING (
    STATUS	*user_status,
    TRA		*tra_handle)
{
/**************************************
 *
 *	g d s _ $ c o m m i t _ r e t a i n i n g
 *
 **************************************
 *
 * Functional description
 *	Commit a transaction.
 *
 **************************************/

API_ENTRY_POINT_INIT;

return commit (user_status, tra_handle, TRUE);
}

STATUS DLL_EXPORT GDS_COMPILE (
    STATUS	*user_status,
    ATT		*db_handle,
    REQ		*req_handle,
    SSHORT	blr_length,
    SCHAR	*blr)
{
/**************************************
 *
 *	g d s _ $ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ		request;
ATT		attachment;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

NULL_CHECK (req_handle, gds__bad_req_handle);
attachment = *db_handle;

if (check_database (tdbb, attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_compile, *db_handle, *req_handle, blr_length, blr);
#endif

ERROR_INIT (env);
request = CMP_compile2 (tdbb, blr, FALSE);
request->req_attachment = attachment;
request->req_request = attachment->att_requests;
attachment->att_requests = request;

DEBUG;
*req_handle = request;
#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *req_handle);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_CREATE_BLOB2 (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    BLB		*blob_handle,
    BID		blob_id,
    USHORT	bpb_length,
    UCHAR	*bpb)
{
/**************************************
 *
 *	g d s _ $ c r e a t e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Open an existing blob.
 *
 **************************************/
TRA		transaction;
BLB		blob;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

NULL_CHECK (blob_handle, gds__bad_segstr_handle);

if (check_database (tdbb, *db_handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_create_blob2, *db_handle, *tra_handle, *blob_handle, blob_id, bpb_length, bpb);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__segstr_wrong_db);
blob = BLB_create2 (tdbb, transaction, blob_id, bpb_length, bpb);
*blob_handle = blob;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *blob_handle);
LOG_call (log_handle_returned, blob_id->bid_stuff.bid_blob);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_CREATE_DATABASE (
    STATUS	*user_status,
    USHORT	file_length,
    UCHAR	*file_name,
    ATT		*handle,
    USHORT	dpb_length,
    UCHAR	*dpb,
    USHORT	db_type,
    UCHAR	*expanded_filename)
{
/**************************************
 *
 *	g d s _ $ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Create a nice, squeeky clean database, uncorrupted by user data.
 *
 **************************************/
DBB		dbb;
TEXT		expanded_name [MAX_PATH_LENGTH];
#ifdef STACK_REDUCTION
TEXT		*allocated_space=NULL;
#else
TEXT		opt_buffer [DPB_EXPAND_BUFFER];
#endif
TEXT		*opt_ptr;
USHORT		length, page_size;
ATT		attachment;
STATUS		temp_status [ISC_STATUS_LENGTH], *status;
DPB		options;
#ifdef V4_THREADING
BOOLEAN		initing_security;
#endif
JMP_BUF		env;
#ifdef _PPC_
JMP_BUF		env1;
#endif
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

if (*handle)
    return handle_error (user_status, gds__bad_db_handle, NULL_PTR);

/* Get length of database file name, if not already known */

length = (file_length) ? file_length : strlen (file_name);
#if defined(mpexl) || defined(NETWARE_386)
if (strncmp (file_name, expanded_filename, length))
    {
    /* If the beginning of the expanded_filename isn't the same
       as the user's input, use the expanded value. */

    file_name = expanded_filename;
    length = strlen (file_name);
    }
#endif

MOVE_FAST (file_name, expanded_name, length);
expanded_name [length] = 0;

SET_THREAD_DATA;

if (!(dbb = init (tdbb, user_status, expanded_name, FALSE)))
    {
    V4_JRD_MUTEX_UNLOCK (databases_mutex);
    JRD_SS_MUTEX_UNLOCK;
    RESTORE_THREAD_DATA;
    return user_status [1];
    }

dbb->dbb_flags |= DBB_being_opened;
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
V4_JRD_MUTEX_UNLOCK (databases_mutex);

tdbb->tdbb_database = dbb;

/* Initialize error handling */

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = status = user_status;
tdbb->tdbb_attachment = attachment = NULL;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;

/* Count active thread in database */

++dbb->dbb_use_count;
#ifdef V4_THREADING
initing_security = FALSE;
#endif

if (SETJMP (env)) 
    {
#ifdef _PPC_
    tdbb->tdbb_setjmp = (UCHAR*) env1;
    if (!SETJMP (env1))
#else
    if (!SETJMP (env))
#endif
	{
#ifdef V4_THREADING
	if (initing_security)
	    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
#endif
	tdbb->tdbb_status_vector = temp_status;
	dbb->dbb_flags &= ~DBB_being_opened;
	release_attachment (attachment);

	/* At this point, mutex dbb->dbb_mutexes [DBB_MUTX_init_fini] has been
	   unlocked and mutex databases_mutex has been locked. */
#ifdef STACK_REDUCTION
	if (allocated_space) ALL_free (allocated_space);
#endif
	if (dbb->dbb_header.blk_type == (UCHAR) type_dbb)
	    if (!dbb->dbb_attachments)
		shutdown_database (dbb, TRUE);
	    else if (attachment)
		ALL_RELEASE (attachment);
	V4_JRD_MUTEX_UNLOCK (databases_mutex);
	}
    tdbb->tdbb_status_vector = status;
    JRD_SS_MUTEX_UNLOCK;
    return error (user_status);
    }

/* Process database parameter block */

#ifdef STACK_REDUCTION
allocated_space = ALL_malloc (DPB_EXPAND_BUFFER, ERR_jmp);
opt_ptr         = allocated_space;
#else
opt_ptr = opt_buffer;
#endif

get_options ((UCHAR *)dpb, dpb_length, &opt_ptr, &options);
if (invalid_client_SQL_dialect == FALSE && options.dpb_sql_dialect == 99)
    options.dpb_sql_dialect = 0;


#ifndef NO_NFS
/* Don't check nfs if single user */

if (!options.dpb_single_user)
#endif
    {
    /* Check to see if the database is truly local or if it just looks
       that way */

    if (ISC_check_if_remote (expanded_filename, TRUE))
	ERR_post (gds__unavailable, 0);
    }

#ifdef ISC_DATABASE_ENCRYPTION
if (options.dpb_key)
    dbb->dbb_encrypt_key = copy_string (options.dpb_key, strlen (options.dpb_key));
#endif

tdbb->tdbb_attachment = attachment = (ATT) ALLOCP (type_att);
attachment->att_database = dbb;
attachment->att_next = dbb->dbb_attachments;
dbb->dbb_attachments = attachment;
dbb->dbb_flags &= ~DBB_being_opened;
dbb->dbb_sys_trans->tra_attachment = attachment;
tdbb->tdbb_quantum = QUANTUM;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_inhibit = 0;
tdbb->tdbb_flags = NULL;

if (options.dpb_working_directory)
    attachment->att_working_directory = copy_string (options.dpb_working_directory, strlen(options.dpb_working_directory));

if (options.dpb_gbak_attach)
    attachment->att_flags |= ATT_gbak_attachment;

switch (options.dpb_sql_dialect)
    {
    case 0:
	/* This can be issued by QLI, GDEF and old BDE clients.  In this case
	 * assume dialect 1 */
	options.dpb_sql_dialect = SQL_DIALECT_V5;
    case SQL_DIALECT_V5:
	break;
    case SQL_DIALECT_V6:
	dbb->dbb_flags |= DBB_DB_SQL_dialect_3;
	break;
    default:
	ERR_post (isc_database_create_failed, isc_arg_string, expanded_filename, isc_arg_gds,
		  isc_inv_dialect_specified, isc_arg_number, options.dpb_sql_dialect,
		  isc_arg_gds, isc_valid_db_dialects, isc_arg_string, "1 and 3", 0);
	break;
    }

attachment->att_charset = options.dpb_interp;
if (options.dpb_lc_messages)
    attachment->att_lc_messages = copy_string (options.dpb_lc_messages, strlen (options.dpb_lc_messages)); 

if (!options.dpb_page_size)
    options.dpb_page_size = DEFAULT_PAGE_SIZE;

for (page_size = MIN_PAGE_SIZE; page_size < MAX_PAGE_SIZE; page_size <<= 1)
    if (options.dpb_page_size < page_size << 1)
	break;

dbb->dbb_page_size = (page_size > MAX_PAGE_SIZE) ? MAX_PAGE_SIZE : page_size;

LCK_init (tdbb, LCK_OWNER_attachment);   /* For the attachment */
attachment->att_flags |= ATT_lck_init_done;
/* Extra LCK_init() done to keep the lock table until the
   database is shutdown() after the last detach. */
LCK_init (tdbb, LCK_OWNER_database);
dbb->dbb_flags |= DBB_lck_init_done;
INI_init();
FUN_init();
PAG_init();
SBM_init();
#ifdef V4_THREADING
V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
initing_security = TRUE;
#endif
SCL_init (TRUE, options.dpb_sys_user_name, options.dpb_user_name,
          options.dpb_password, options.dpb_password_enc,
          options.dpb_role_name, tdbb);
#ifdef V4_THREADING
initing_security = FALSE;
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
#endif
length = PIO_expand (file_name, length, expanded_name);
dbb->dbb_file = PIO_create (dbb, expanded_name, length, options.dpb_overwrite);
if (options.dpb_set_page_buffers)
    dbb->dbb_page_buffers = options.dpb_page_buffers;
CCH_init (tdbb, (ULONG) options.dpb_buffers);
PAG_format_header();
INI_init2();
PAG_format_log();
PAG_format_pip();

if (options.dpb_set_page_buffers)
    PAG_set_page_buffers (options.dpb_page_buffers);

if (options.dpb_set_no_reserve)
    PAG_set_no_reserve (dbb, options.dpb_no_reserve);

INI_format (attachment->att_user->usr_user_name);

if (options.dpb_shutdown || options.dpb_online)
    {
    /* By releasing the DBB_MUTX_init_fini mutex here, we would be allowing
       other threads to proceed with their detachments, so that shutdown does
       not timeout for exclusive access and other threads don't have to wait 
       behind shutdown */

    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    if (!SHUT_database (dbb, options.dpb_shutdown, options.dpb_shutdown_delay))
	{
	V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
	ERR_post (gds__no_priv,
		gds_arg_string, "shutdown or online",
		gds_arg_string, "database",
		gds_arg_string, ERR_string (file_name, length), 0);
	}
    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    }

if (options.dpb_sweep_interval != -1)
    {
    PAG_sweep_interval (options.dpb_sweep_interval);
    dbb->dbb_sweep_interval = options.dpb_sweep_interval;
    }

if (options.dpb_set_force_write)
    PAG_set_force_write (dbb, options.dpb_force_write);

/* initialize shadowing semaphore as soon as the database is ready for it
   but before any real work is done */

SDW_init (options.dpb_activate_shadow, options.dpb_delete_shadow, NULL_PTR);

#ifndef WINDOWS_ONLY
AIL_set_log_options (
		     options.dpb_wal_chkpt_len,
		     options.dpb_wal_num_bufs,
		     options.dpb_wal_bufsize,
		     options.dpb_wal_grp_cmt_wait
		    );
#endif

#ifdef GARBAGE_THREAD
VIO_init (tdbb);
#endif

CCH_release_exclusive (tdbb);

/* Figure out what character set & collation this attachment prefers */

find_intl_charset (tdbb, attachment, &options);

dbb->dbb_filename = copy_string (expanded_name, length);

#ifdef GOVERNOR
if (!options.dpb_sec_attach) 
    {
    if (JRD_max_users)
        {
        if (num_attached < (JRD_max_users * ATTACHMENTS_PER_USER) )
	    num_attached++;
        else
	    ERR_post (isc_max_att_exceeded, 0);
        }
    }
else
    {
    attachment->att_flags |= ATT_security_db;
    }
#endif /* GOVERNOR */

V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);

JRD_SS_MUTEX_UNLOCK;

*handle = attachment;
CCH_flush (tdbb, (USHORT) FLUSH_FINI, 0);

#ifdef STACK_REDUCTION
ALL_free (allocated_space);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_DATABASE_INFO (
    STATUS	*user_status,
    ATT		*handle,
    SSHORT	item_length,
    SCHAR	*items,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ d a t a b a s e _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on database object.
 *
 **************************************/
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (check_database (tdbb, *handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_database_info2, *handle, item_length, items, buffer_length);
#endif

ERROR_INIT (env);
INF_database_info (items, item_length, 	buffer, buffer_length);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_DDL (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    USHORT	ddl_length,
    SCHAR	*ddl)
{
/**************************************
 *
 *	g d s _ $ d d l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
ATT		attachment;
TRA		transaction;
JMP_BUF		rollback_env, env;
struct tdbb	thd_context, *tdbb = NULL;
STATUS		temp_status [ISC_STATUS_LENGTH];

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

attachment = *db_handle;
if (check_database (tdbb, attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_ddl, *db_handle, *tra_handle, ddl_length, ddl);
#endif

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = user_status;
if (SETJMP (env))
    {
    if (tdbb->tdbb_status_vector == temp_status)
	tdbb->tdbb_status_vector = user_status;
    return error (user_status);
    }

transaction = find_transaction (tdbb, *tra_handle, gds__segstr_wrong_db);

DYN_ddl (attachment, transaction, ddl_length, ddl);

/* 
 * Perform an auto commit for autocommit transactions.
 * This is slightly tricky.  If the commit retain works,
 * all is well.  If TRA_commit () fails, we perform
 * a rollback_retain ().  This will backout the
 * effects of the transaction, mark it dead and 
 * start a new transaction.

 * For now only ExpressLink will use this feature.  Later
 * a new entry point may be added.
 */

if (transaction->tra_flags & TRA_perform_autocommit)
    {
    transaction->tra_flags &= ~TRA_perform_autocommit;
    tdbb->tdbb_setjmp = (UCHAR*) rollback_env;

    if (SETJMP (rollback_env)) 
	{
	tdbb->tdbb_setjmp = (UCHAR*) env;
	tdbb->tdbb_status_vector = temp_status;
	TRA_rollback (tdbb, transaction, TRUE);
	tdbb->tdbb_status_vector = user_status;
	tdbb->tdbb_setjmp = (UCHAR*) env;

	return error (user_status);
	}

    TRA_commit (tdbb, transaction, TRUE);
    tdbb->tdbb_setjmp = (UCHAR*) env;
    };

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_DETACH (
    STATUS	*user_status,
    ATT		*handle)
{
/**************************************
 *
 *	g d s _ $ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a database.
 *
 **************************************/
DBB		dbb;
ATT		attachment, attach;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;
#ifdef GOVERNOR
USHORT		attachment_flags;
#endif

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

attachment = *handle;

/* Check out the database handle.  This is mostly code from
   the routine "check_database" */

if (!attachment || 
    (attachment->att_header.blk_type != (UCHAR) type_att) ||
    !(dbb = attachment->att_database) ||
    dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

/* Make sure this is a valid attachment */

for (attach = dbb->dbb_attachments; attach; attach = attach->att_next)
    if (attach == attachment)
	break;

if (!attach)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

/* if this is the last attachment, mark dbb as not in use */

JRD_SS_MUTEX_LOCK;
V4_JRD_MUTEX_LOCK (databases_mutex);
if (dbb->dbb_attachments == attachment && !attachment->att_next &&
     !(dbb->dbb_flags & DBB_being_opened))
    dbb->dbb_flags |= DBB_not_in_use;
V4_JRD_MUTEX_UNLOCK (databases_mutex);

tdbb->tdbb_database = dbb;
tdbb->tdbb_attachment = attachment;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_default = NULL;

/* Count active thread in database */

++dbb->dbb_use_count;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_detach, *handle);
LOG_call (log_statistics, dbb->dbb_reads, dbb->dbb_writes, dbb->dbb_max_memory);
#endif

/* purge_attachment below can do an ERR_post */

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = user_status;
if (SETJMP (env))
    {
    JRD_SS_MUTEX_UNLOCK;
    return error (user_status);
    }

/* Purge attachment, don't rollback open transactions */

V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);

#ifdef CANCEL_OPERATION
attachment->att_flags |= ATT_cancel_disable;
#endif

#ifdef GOVERNOR
attachment_flags = attachment->att_flags;
#endif

purge_attachment (tdbb, user_status, attachment, FALSE);

#ifdef GOVERNOR
if (JRD_max_users)
    {
    if (!(attachment_flags & ATT_security_db))
        {
	assert (num_attached > 0);
        num_attached--;
        }
    }
#endif /* GOVERNOR */

V4_JRD_MUTEX_UNLOCK (databases_mutex);
JRD_SS_MUTEX_UNLOCK;

*handle = NULL;

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_DROP_DATABASE (
    STATUS	*user_status,
    ATT		*handle)
{
/**************************************
 *
 *	i s c _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Close down and purge a database.
 *
 **************************************/
DBB		dbb;
ATT		attachment, attach;
FIL		file;
WIN		window;
HDR		header;
SDW		shadow;
BOOLEAN		err;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

attachment = *handle;

/* Check out the database handle.  This is mostly code from
   the routine "check_database" */

if (!attachment || 
    (attachment->att_header.blk_type != (UCHAR) type_att) ||
    !(dbb = attachment->att_database) ||
    dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

/* Make sure this is a valid attachment */

for (attach = dbb->dbb_attachments; attach; attach = attach->att_next)
    if (attach == attachment)
	break;

if (!attach)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

tdbb->tdbb_database = dbb;
tdbb->tdbb_attachment = attachment;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_default = dbb->dbb_permanent;

/* Count active thread in database */

++dbb->dbb_use_count;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_drop_database, *handle);
LOG_call (log_statistics, dbb->dbb_reads, dbb->dbb_writes, dbb->dbb_max_memory);
#endif

ERROR_INIT (env);

if (!attachment->att_user->usr_flags & (USR_locksmith | USR_owner))
    ERR_post (gds__no_priv,
	    gds_arg_string, "drop",
	    gds_arg_string, "database",
	    gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string), 0);

if (attachment->att_flags & ATT_shutdown)
    ERR_post (gds__shutdown, gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string), 0);

if (!CCH_exclusive (tdbb, LCK_PW, WAIT_PERIOD))
    ERR_post (gds__lock_timeout, gds_arg_gds, gds__obj_in_use,
	gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string), 0);

JRD_SS_MUTEX_LOCK;
V4_JRD_MUTEX_LOCK (databases_mutex);
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);

if (SETJMP (env)) 
    {
    V4_JRD_MUTEX_UNLOCK (databases_mutex);
    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    JRD_SS_MUTEX_UNLOCK;
    return error (user_status);
    }

/* Check if same process has more attachments */

if ((attach = dbb->dbb_attachments) && (attach->att_next))
    ERR_post (gds__no_meta_update, gds_arg_gds, gds__obj_in_use,
	gds_arg_string, "DATABASE", 0);

#ifdef CANCEL_OPERATION
attachment->att_flags |= ATT_cancel_disable;
#endif

/* Here we have database locked in exclusive mode.
   Just mark the header page with an 0 ods version so that no other
   process can attach to this database once we release our exclusive
   lock and start dropping files. */

window.win_page = HEADER_PAGE;
window.win_flags = 0;
header = (HDR) CCH_FETCH (tdbb, &window, LCK_write, pag_header);
CCH_MARK_MUST_WRITE (tdbb, &window);
header->hdr_ods_version = 0;
CCH_RELEASE (tdbb, &window);

/* A default catch all */
if (SETJMP (env)) 
    {
    JRD_SS_MUTEX_UNLOCK;
    return error (user_status);
    }

/* This point on database is useless */

/* mark the dbb unusable */

dbb->dbb_flags |= DBB_not_in_use;
*handle = NULL;

V4_JRD_MUTEX_UNLOCK (databases_mutex);

file = dbb->dbb_file;
shadow = dbb->dbb_shadow;

#ifndef WINDOWS_ONLY
AIL_drop_log();
#endif

tdbb->tdbb_default = NULL;

#ifdef GOVERNOR
if (JRD_max_users)
    {
    assert (num_attached > 0);
    num_attached--;
    }
#endif /* GOVERNOR */

/* Unlink attachment from database */

release_attachment (attachment);

/* At this point, mutex dbb->dbb_mutexes [DBB_MUTX_init_fini] has been
   unlocked and mutex databases_mutex has been locked. */

shutdown_database (dbb, FALSE);

/* drop the files here. */

err = drop_files (file);
for (; shadow; shadow = shadow->sdw_next)
    err |= drop_files (shadow->sdw_file);

V4_JRD_MUTEX_UNLOCK (databases_mutex);
JRD_SS_MUTEX_UNLOCK;

ALL_fini();
tdbb->tdbb_database = NULL;
if (err)
    {
    user_status [0] = gds_arg_gds;
    user_status [1] = gds__drdb_completed_with_errs;
    user_status [2] = gds_arg_end; 
    return user_status [1];
    }

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_GET_SEGMENT (
    STATUS	*user_status,
    BLB		*blob_handle,
    USHORT	*length,
    USHORT	buffer_length,
    UCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
BLB		blob;
DBB		dbb;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (!(blob = check_blob (tdbb, user_status, blob_handle)))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_get_segment2, *blob_handle, length, buffer_length);
#endif

ERROR_INIT (env);
*length = BLB_get_segment (tdbb, blob, buffer, buffer_length);
tdbb->tdbb_status_vector [0] = gds_arg_gds;
tdbb->tdbb_status_vector [2] = gds_arg_end;
dbb = tdbb->tdbb_database;

if (blob->blb_flags & BLB_eof)
    {
    RESTORE_THREAD_DATA;
    --dbb->dbb_use_count;
    return (user_status [1] = gds__segstr_eof); 
    }
else if (blob->blb_fragment_size)
    {
    RESTORE_THREAD_DATA;
    --dbb->dbb_use_count;
    return (user_status [1] = gds__segment); 
    }

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_GET_SLICE (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    SLONG	*array_id,
    USHORT	sdl_length,
    UCHAR	*sdl,
    USHORT	param_length,
    UCHAR	*param,
    SLONG	slice_length,
    UCHAR	*slice,
    SLONG	*return_length)
{
/**************************************
 *
 *	g d s _ $ g e t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Snatch a slice of an array.
 *
 **************************************/
TRA		transaction;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (check_database (tdbb, *db_handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_get_slice2, *db_handle, *tra_handle, *array_id, sdl_length, sdl, param_length, param, slice_length);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__segstr_wrong_db);

if (!array_id[0] && !array_id[1])
    {
    MOVE_CLEAR (slice, slice_length);
    *return_length = 0;
    }
else
    *return_length = BLB_get_slice (tdbb, transaction, array_id, sdl, param_length, param, slice_length, slice);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_OPEN_BLOB2 (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    BLB		*blob_handle,
    BID		blob_id,
    USHORT	bpb_length,
    UCHAR	*bpb)
{
/**************************************
 *
 *	g d s _ $ o p e n _ b l o b 2
 *
 **************************************
 *
 * Functional description
 *	Open an existing blob.
 *
 **************************************/
BLB		blob;
TRA		transaction;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

NULL_CHECK (blob_handle, gds__bad_segstr_handle);

if (check_database (tdbb, *db_handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_open_blob2, *db_handle, *tra_handle, *blob_handle, blob_id, bpb_length, bpb);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__segstr_wrong_db);
blob = BLB_open2 (tdbb, transaction, blob_id, bpb_length, bpb);
*blob_handle = blob;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *blob_handle);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_PREPARE (
    STATUS	*user_status,
    TRA		*tra_handle,
    USHORT	length,
    UCHAR	*msg)
{
/**************************************
 *
 *	g d s _ $ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Prepare a transaction for commit.  First phase of a two
 *	phase commit.
 *
 **************************************/
TRA		transaction;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

CHECK_HANDLE ((*tra_handle), type_tra, gds__bad_trans_handle);
transaction = *tra_handle;

if (check_database (tdbb, transaction->tra_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_prepare2, *tra_handle, length, msg);
#endif

if (prepare (tdbb, transaction, user_status, length, msg))
    return error (user_status);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_PUT_SEGMENT (
    STATUS	*user_status,
    BLB		*blob_handle,
    USHORT	buffer_length,
    UCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Abort a partially completed blob.
 *
 **************************************/
BLB		blob;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (!(blob = check_blob (tdbb, user_status, blob_handle)))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_put_segment, *blob_handle, buffer_length, buffer);
#endif

ERROR_INIT (env);
BLB_put_segment (tdbb, blob, buffer, buffer_length);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_PUT_SLICE (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    SLONG	*array_id,
    USHORT	sdl_length,
    UCHAR	*sdl,
    USHORT	param_length,
    UCHAR	*param,
    SLONG	slice_length,
    UCHAR	*slice)
{
/**************************************
 *
 *	g d s _ $ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Snatch a slice of an array.
 *
 **************************************/
TRA		transaction;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (check_database (tdbb, *db_handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_put_slice, *db_handle, *tra_handle, *array_id, sdl_length, sdl, param_length, param, slice_length, slice);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__segstr_wrong_db);
BLB_put_slice (tdbb, transaction, array_id, sdl, param_length, param, slice_length, slice);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_QUE_EVENTS (
    STATUS	*user_status,
    ATT		*handle,
    SLONG	*id,
    SSHORT	length,
    UCHAR	*items,
    void	(*ast)(),
    void	*arg)
{
/**************************************
 *
 *	g d s _ $ q u e _ e v e n t s
 *
 **************************************
 *
 * Functional description
 *	Que a request for event notification.
 *
 **************************************/
#ifndef WINDOWS_ONLY /* Events are not supported under LIBS */
DBB		dbb;
ATT		attachment;
LCK		lock;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (check_database (tdbb, *handle, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_que_events, *handle, length, items);
#endif

ERROR_INIT (env);
dbb = tdbb->tdbb_database;
lock = dbb->dbb_lock;
attachment = tdbb->tdbb_attachment;

if (!attachment->att_event_session &&
    !(attachment->att_event_session = EVENT_create_session (user_status)))
    return error (user_status);

*id = EVENT_que (user_status, 
	attachment->att_event_session,
	lock->lck_length,
	(TEXT*) &lock->lck_key,
	length,
	items,
	ast,
	arg);

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *id);
#endif

RETURN_SUCCESS;
#else /* WINDOWS_ONLY */
user_status [0] = isc_arg_gds;
user_status [1] = isc_wish_list;
user_status [2] = isc_arg_end;

return user_status [1];
#endif /* WINDOWS_ONLY */
}

STATUS DLL_EXPORT GDS_RECEIVE (
    STATUS	*user_status,
    REQ		*req_handle,
    USHORT	msg_type,
    USHORT	msg_length,
    SCHAR	*msg,
    SSHORT	level
#ifdef SCROLLABLE_CURSORS
    , 
    USHORT	direction, 
    ULONG	offset
#endif
    )
{
/**************************************
 *
 *	g d s _ $ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
REQ		request;
VEC		vector;
USHORT		lev;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

CHECK_HANDLE ((*req_handle), type_req, gds__bad_req_handle);
request = *req_handle;

if (check_database (tdbb, request->req_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_receive2, *req_handle, msg_type, msg_length, level);
#endif

ERROR_INIT (env);

if (lev = level)
    if (!(vector = request->req_sub_requests) ||
	lev >= vector->vec_count ||
	!(request = (REQ) vector->vec_object [lev]))
	ERR_post (gds__req_sync, 0);

#ifdef SCROLLABLE_CURSORS
if (direction) 
    EXE_seek (tdbb, request, direction, offset); 
#endif

EXE_receive (tdbb, request, msg_type, msg_length, msg);

CHECK_AUTOCOMMIT;

if (request->req_flags & req_warning)
    {
    request->req_flags &= ~req_warning;
    return error (user_status);
    }
else
    RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_RECONNECT (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    SSHORT	length,
    UCHAR	*id)
{
/**************************************
 *
 *	g d s _ $ r e c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Connect to a transaction in limbo.
 *
 **************************************/
TRA		transaction;
ATT		attachment;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

NULL_CHECK (tra_handle, gds__bad_trans_handle);
attachment = *db_handle;

if (check_database (tdbb, attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_reconnect, *db_handle, *tra_handle, length, id);
#endif

ERROR_INIT (env);
transaction = TRA_reconnect (tdbb, id, length);
*tra_handle = transaction;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *tra_handle);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_RELEASE_REQUEST (
    STATUS	*user_status,
    REQ		*req_handle)
{
/**************************************
 *
 *	g d s _ $ r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request.
 *
 **************************************/
REQ		request;
ATT		attachment;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

CHECK_HANDLE ((*req_handle), type_req, gds__bad_req_handle);
request = *req_handle;
attachment = request->req_attachment;

if (check_database (tdbb, attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_release_request, *req_handle);
#endif

ERROR_INIT (env);
CMP_release (tdbb, request);
*req_handle = NULL;

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_REQUEST_INFO (
    STATUS	*user_status,
    REQ		*req_handle,
    SSHORT	level,
    SSHORT	item_length,
    SCHAR	*items,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ r e q u e s t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on blob object.
 *
 **************************************/
USHORT		lev;
REQ		request;
VEC		vector;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

request = *req_handle;
CHECK_HANDLE (request, type_req, gds__bad_req_handle);

if (check_database (tdbb, request->req_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_request_info2, *req_handle, level, item_length, items, buffer_length);
#endif

ERROR_INIT (env);

if (lev = level)
    if (!(vector = request->req_sub_requests) ||
	lev >= vector->vec_count ||
	!(request = (REQ) vector->vec_object [lev]))
	ERR_post (gds__req_sync, 0);

INF_request_info (request, items, item_length, buffer, buffer_length);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_ROLLBACK_RETAINING (
    STATUS	*user_status,
    TRA		*tra_handle)
{
/**************************************
 *
 *	i s c _ r o l l b a c k _ r e t a i n i n g
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction but keep the environment valid
 *
 **************************************/
TRA		transaction;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

transaction = *tra_handle;
CHECK_HANDLE (transaction, type_tra, gds__bad_trans_handle);

if (check_database (tdbb, transaction->tra_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_rollback, *tra_handle);
#endif

if (rollback (tdbb, transaction, user_status, TRUE))
    return error (user_status);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_ROLLBACK (
    STATUS	*user_status,
    TRA		*tra_handle)
{
/**************************************
 *
 *	g d s _ $ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction.
 *
 **************************************/
TRA		transaction;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

transaction = *tra_handle;
CHECK_HANDLE (transaction, type_tra, gds__bad_trans_handle);

if (check_database (tdbb, transaction->tra_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_rollback, *tra_handle);
#endif

if (rollback (tdbb, transaction, user_status, FALSE))
    return error (user_status);

*tra_handle = NULL;
RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SEEK_BLOB (
    STATUS	*user_status,
    BLB		*blob_handle,
    SSHORT	mode,
    SLONG	offset,
    SLONG	*result)
{
/**************************************
 *
 *	g d s _ $ s e e k _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Seek a stream blob.
 *
 **************************************/
BLB		blob;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

if (!(blob = check_blob (tdbb, user_status, blob_handle)))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_blob_seek, *blob_handle, mode, offset);
#endif

ERROR_INIT (env);
*result = BLB_lseek (blob, mode, offset);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SEND (
    STATUS	*user_status,
    REQ		*req_handle,
    USHORT	msg_type,
    USHORT	msg_length,
    SCHAR	*msg,
    SSHORT	level)
{
/**************************************
 *
 *	g d s _ $ s e n d
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
REQ		request;
VEC		vector;
USHORT		lev;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

CHECK_HANDLE ((*req_handle), type_req, gds__bad_req_handle);
request = *req_handle;

if (check_database (tdbb, request->req_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_send, *req_handle, msg_type, msg_length, msg, level);
#endif

ERROR_INIT (env);

if (lev = level)
    if (!(vector = request->req_sub_requests) ||
	lev >= vector->vec_count ||
	!(request = (REQ) vector->vec_object [lev]))
	ERR_post (gds__req_sync, 0);

EXE_send (tdbb, request, msg_type, msg_length, msg);

CHECK_AUTOCOMMIT;        

if (request->req_flags & req_warning)
    {
    request->req_flags &= ~req_warning;
    return error (user_status);
    }
else
    RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SERVICE_ATTACH (
    STATUS	*user_status,
    USHORT	service_length,
    TEXT	*service_name,
    SVC		*svc_handle,
    USHORT	spb_length,
    SCHAR	*spb)
{
/**************************************
 *
 *	g d s _ $ s e r v i c e _ a t t a c h
 *
 **************************************
 *
 * Functional description
 *	Connect to an Interbase service.
 *
 **************************************/
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

if (*svc_handle)
    return handle_error (user_status, gds__bad_svc_handle, NULL_PTR);

SET_THREAD_DATA;

ERROR_INIT (env);
tdbb->tdbb_database = NULL;

*svc_handle = SVC_attach (service_length, service_name, spb_length, spb);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SERVICE_DETACH (
    STATUS	*user_status,
    SVC		*svc_handle)
{
/**************************************
 *
 *	g d s _ $ s e r v i c e _ d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Close down a service.
 *
 **************************************/
SVC		service;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

service = *svc_handle;
CHECK_HANDLE (service, type_svc, gds__bad_svc_handle);

ERROR_INIT (env);
tdbb->tdbb_database = NULL;

SVC_detach (service);

*svc_handle = NULL;

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SERVICE_QUERY (
    STATUS	*user_status,
    SVC		*svc_handle,
    ULONG   *reserved,
    USHORT	send_item_length,
    SCHAR	*send_items,
    USHORT	recv_item_length,
    SCHAR	*recv_items,
    USHORT	buffer_length,
    SCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ s e r v i c e _ q u e r y
 *
 **************************************
 *
 * Functional description
 *	Provide information on service object.
 *
 *	NOTE: The parameter RESERVED must not be used
 *	for any purpose as there are networking issues
 *	involved (as with any handle that goes over the
 *	network).  This parameter will be implemented at 
 *	a later date.
 *
 **************************************/
SVC		service;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;
int		len, warning;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

service = *svc_handle;
CHECK_HANDLE (service, type_svc, gds__bad_svc_handle);

ERROR_INIT (env);
tdbb->tdbb_database = NULL;

if (service->svc_spb_version == isc_spb_version1)
    SVC_query (service, send_item_length, send_items, recv_item_length, recv_items, buffer_length, buffer);
else
    {
    /* For SVC_query2, we are going to completly dismantle user_status (since at this point it is 
     * meaningless anyway).  The status vector returned by this function can hold information about
     * the call to query the service manager and/or a service thread that may have been running.
     */

     SVC_query2 (service, tdbb, send_item_length, send_items, recv_item_length, recv_items, buffer_length, buffer);

    /* if there is a status vector from a service thread, copy it into the thread status */
    PARSE_STATUS(service->svc_status, len, warning);
    if (len)
        {
	MOVE_FASTER(service->svc_status, tdbb->tdbb_status_vector, sizeof(STATUS) * len);

        /* Empty out the service status vector */
        memset (service->svc_status, 0, ISC_STATUS_LENGTH * sizeof (STATUS));
	}

    if (user_status[1])
	return error (user_status);
    }
RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_SERVICE_START (
    STATUS	*user_status,
    SVC		*svc_handle,
    ULONG    	*reserved,
    USHORT	spb_length,
    SCHAR	*spb)
{
/**************************************
 *
 *	g d s _ s e r v i c e _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Start the specified service
 *
 *	NOTE: The parameter RESERVED must not be used
 *	for any purpose as there are networking issues
 *  	involved (as with any handle that goes over the
 *   	network).  This parameter will be implemented at 
 * 	a later date.
 **************************************/
SVC         service;
JMP_BUF     env;
struct tdbb thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

service = *svc_handle;
CHECK_HANDLE (service, type_svc, isc_bad_svc_handle);

ERROR_INIT (env);
tdbb->tdbb_database = NULL;

SVC_start (service, spb_length, spb);

if (service->svc_status[1])
    {
    STATUS *svc_status = service->svc_status,
           *tdbb_status = tdbb->tdbb_status_vector;
    
    while (*svc_status)
        *tdbb_status++ = *svc_status++;
    *tdbb_status = isc_arg_end;
    }

if (user_status[1])
    return error (user_status);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_START_AND_SEND (
    STATUS	*user_status,
    REQ		*req_handle,
    TRA		*tra_handle,
    USHORT	msg_type,
    USHORT	msg_length,
    SCHAR	*msg,
    SSHORT	level)
{
/**************************************
 *
 *	g d s _ $ s t a r t _ a n d _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
TRA		transaction;
REQ		request;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

request = *req_handle;
CHECK_HANDLE (request, type_req, gds__bad_req_handle);

if (check_database (tdbb, request->req_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_start_and_send, *req_handle, *tra_handle, msg_type, msg_length, msg, level);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__req_wrong_db);

if (level)
    request = CMP_clone_request (tdbb, request, level, FALSE);

EXE_unwind (tdbb, request);
EXE_start (tdbb, request, transaction);
EXE_send (tdbb, request, msg_type, msg_length, msg);

CHECK_AUTOCOMMIT;

if (request->req_flags & req_warning)
    {
    request->req_flags &= ~req_warning;
    return error (user_status);
    }
else
    RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_START (
    STATUS		*user_status,
    register REQ	*req_handle,
    register TRA	*tra_handle,
    SSHORT		level)
{
/**************************************
 *
 *	g d s _ $ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Get a record from the host program.
 *
 **************************************/
TRA		transaction;
REQ		request;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

request = *req_handle;
CHECK_HANDLE (request, type_req, gds__bad_req_handle);

if (check_database (tdbb, request->req_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_start, *req_handle, *tra_handle, level);
#endif

ERROR_INIT (env);
transaction = find_transaction (tdbb, *tra_handle, gds__req_wrong_db);

if (level)
    request = CMP_clone_request (tdbb, request, level, FALSE);

EXE_unwind (tdbb, request);
EXE_start (tdbb, request, transaction);

CHECK_AUTOCOMMIT;        

if (request->req_flags & req_warning)
    {
    request->req_flags &= ~req_warning;
    return error (user_status);
    }
else
    RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_START_MULTIPLE (
    STATUS	*user_status,
    TRA		*tra_handle,
    USHORT	count,
    TEB		*vector)
{
/**************************************
 *
 *	g d s _ $ s t a r t _ m u l t i p l e
 *
 **************************************
 *
 * Functional description
 *	Start a transaction.
 *
 **************************************/
VOLATILE TRA	transaction, prior;
STATUS		temp_status [ISC_STATUS_LENGTH];
TEB		*v, *end;
ATT		attachment;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;
DBB		dbb;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

NULL_CHECK (tra_handle, gds__bad_trans_handle);
end = vector + count;

for (v = vector; v < end ; v++)
    {
    if (check_database (tdbb, *v->teb_database, user_status))
	return user_status [1];
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    }

if (check_database (tdbb, *vector->teb_database, user_status))
    return user_status [1];

prior = NULL;

if (SETJMP (env))
    {
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    if (prior)
	rollback (tdbb, prior, temp_status, FALSE);
    return error (user_status);
    }

for (v = vector; v < end ; v++)
    {
    attachment = *v->teb_database;
    if (check_database (tdbb, attachment, user_status))
	return user_status [1];
#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
    LOG_call (log_start_multiple, *tra_handle, count, vector);
#endif
    tdbb->tdbb_setjmp = (UCHAR*) env;
    tdbb->tdbb_status_vector = user_status;
    transaction = TRA_start (tdbb, v->teb_tpb_length, v->teb_tpb);
    transaction->tra_sibling = prior;
    prior = transaction;
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    }

*tra_handle = transaction;
#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_handle_returned, *tra_handle);
#endif

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_START_TRANSACTION (
    STATUS	*user_status,
    TRA		*tra_handle,
    SSHORT	count,
    ...)
{
/**************************************
 *
 *	g d s _ $ s t a r t _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Start a transaction.
 *
 **************************************/
TEB	tebs [16], *teb, *end;
va_list	ptr;

API_ENTRY_POINT_INIT;

VA_START (ptr, count);

for (teb = tebs, end = teb + count; teb < end; teb++)
    {
    teb->teb_database = va_arg (ptr, ATT*);
    teb->teb_tpb_length = va_arg (ptr, int);
    teb->teb_tpb = va_arg (ptr, UCHAR*);
    }

return GDS_START_MULTIPLE (user_status, tra_handle, count, tebs);
}

STATUS DLL_EXPORT GDS_TRANSACT_REQUEST (
    STATUS	*user_status,
    ATT		*db_handle,
    TRA		*tra_handle,
    USHORT	blr_length,
    SCHAR	*blr,
    USHORT	in_msg_length,
    SCHAR	*in_msg,
    USHORT	out_msg_length,
    SCHAR	*out_msg)
{
/**************************************
 *
 *	i s c _ t r a n s a c t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Execute a procedure.
 *
 **************************************/
ATT		attachment;
TRA		transaction;
CSB		csb;
REQ		request;
NOD		in_message, out_message, node;
ACC		access;
SCL		class;
FMT		format;
PLB		old_pool, new_pool;
USHORT		i, len;
JMP_BUF		env;
#ifdef _PPC_
JMP_BUF		env1;
#endif
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

attachment = *db_handle;
if (check_database (tdbb, attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_transact_request, *db_handle, *tra_handle,
    blr_length, blr, in_msg_length, in_msg, out_msg_length);
#endif

new_pool = old_pool = NULL;
request = NULL;

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = user_status;
if (SETJMP (env))
    {
    /* Set up to trap error in case release pool goes wrong. */

#ifdef _PPC_
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env1))
#else
    if (SETJMP (env))
#endif
	return error (user_status);
    if (old_pool)
	tdbb->tdbb_default = old_pool;
    if (request)
	CMP_release (tdbb, request);
    else if (new_pool)
	ALL_rlpool (new_pool);
    return error (user_status);
    }

transaction = find_transaction (tdbb, *tra_handle, gds__req_wrong_db);

old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = new_pool = ALL_pool();

csb = PAR_parse (tdbb, blr, FALSE);
request = CMP_make_request (tdbb, &csb);

for (access = request->req_access; access; access = access->acc_next)
    {
    class = SCL_get_class (access->acc_security_name);
    SCL_check_access (class, access->acc_view, access->acc_trg_name,
	access->acc_prc_name, access->acc_mask,
	access->acc_type, access->acc_name);
    }

in_message = out_message = NULL;
for (i = 0; i < csb->csb_count; i++)
    if (node = csb->csb_rpt [i].csb_message)
	{
	if ((int) node->nod_arg [e_msg_number] == 0)
	    in_message = node;
	else if ((int) node->nod_arg [e_msg_number] == 1)
	    out_message = node;
	}

tdbb->tdbb_default = old_pool;
old_pool = NULL;

request->req_attachment = attachment;

if (in_msg_length)
    {
    if (in_message)
	{
	format = (FMT) in_message->nod_arg [e_msg_format];
	len = format->fmt_length;
	}
    else
	len = 0;
    if (in_msg_length != len)
	ERR_post (gds__port_len,
	    gds_arg_number, (SLONG) in_msg_length,
	    gds_arg_number, (SLONG) len, 0);
    if ((U_IPTR) in_msg & (ALIGNMENT - 1))
	MOVE_FAST (in_msg, (SCHAR*) request + in_message->nod_impure, in_msg_length);
    else
	MOVE_FASTER (in_msg, (SCHAR*) request + in_message->nod_impure, in_msg_length);
    }

EXE_start (tdbb, request, transaction);

if (out_message)
    {
    format = (FMT) out_message->nod_arg [e_msg_format];
    len = format->fmt_length;
    }
else
    len = 0;
if (out_msg_length != len)
    ERR_post (gds__port_len,
	gds_arg_number, (SLONG) out_msg_length,
	gds_arg_number, (SLONG) len, 0);

if (out_msg_length)
    if ((U_IPTR) out_msg & (ALIGNMENT - 1))
	MOVE_FAST ((SCHAR*) request + out_message->nod_impure, out_msg, out_msg_length);
    else
	MOVE_FASTER ((SCHAR*) request + out_message->nod_impure, out_msg, out_msg_length);

CHECK_AUTOCOMMIT;        

CMP_release (tdbb, request);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_TRANSACTION_INFO (
    STATUS	*user_status,
    TRA		*tra_handle,
    SSHORT	item_length,
    SCHAR	*items,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
/**************************************
 *
 *	g d s _ $ t r a n s a c t i o n _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Provide information on blob object.
 *
 **************************************/
TRA		transaction;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

transaction = *tra_handle;
CHECK_HANDLE (transaction, type_tra, gds__bad_trans_handle);

if (check_database (tdbb, transaction->tra_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_transaction_info2, *tra_handle, item_length, items, buffer_length);
#endif

ERROR_INIT (env);

INF_transaction_info (transaction, items, item_length, buffer, buffer_length);

RETURN_SUCCESS;
}

STATUS DLL_EXPORT GDS_UNWIND (
    STATUS	*user_status,
    REQ		*req_handle,
    SSHORT	level)
{
/**************************************
 *
 *	g d s _ $ u n w i n d
 *
 **************************************
 *
 * Functional description
 *	Unwind a running request.  This is potentially nasty since it can
 *	be called asynchronously.
 *
 **************************************/
DBB		dbb;
ATT		attachment, attach;
REQ		request;
VEC		vector;
USHORT		lev;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

API_ENTRY_POINT_INIT;

SET_THREAD_DATA;

CHECK_HANDLE ((*req_handle), type_req, gds__bad_req_handle);
request = *req_handle;

/* Make sure blocks look and feel kosher */

if (!(attachment = request->req_attachment) || 
    (attachment->att_header.blk_type != (UCHAR) type_att) ||
    !(dbb = attachment->att_database) ||
    dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

/* Make sure this is a valid attachment */

for (attach = dbb->dbb_attachments; attach; attach = attach->att_next)
    if (attach == attachment)
	break;

if (!attach)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

tdbb->tdbb_database = dbb;

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call (log_unwind, *req_handle, level);
#endif

/* Set up error handling to restore old state */

tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = user_status;
tdbb->tdbb_attachment = attachment;

if (SETJMP (env)) 
    {
    RESTORE_THREAD_DATA;
    return user_status [1];
    }

/* Pick up and validate request level */

if (lev = level)
    if (!(vector = request->req_sub_requests) ||
	lev >= vector->vec_count ||
	!(request = (REQ) vector->vec_object [lev]))
	ERR_post (gds__req_sync, 0);

tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;

/* Unwind request.  This just tweaks some bits */

EXE_unwind (tdbb, request);

/* Restore old state and get out */

RESTORE_THREAD_DATA;

user_status [0] = gds_arg_gds;
user_status [2] = gds_arg_end;

return (user_status [1] = SUCCESS);
}

#ifdef MULTI_THREAD
void JRD_blocked (
    ATT		blocking,
    BTB		*que)
{
/**************************************
 *
 *	J R D _ b l o c k e d
 *
 **************************************
 *
 * Functional description
 *	We're blocked by another thread.  Unless it would cause
 *	a deadlock, wait for the other other thread (it will
 *	wake us up.
 *
 **************************************/
TDBB	tdbb = NULL;
DBB	dbb;
ATT	attachment;
BTB	block;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

/* Check for deadlock.  If there is one, complain */

for (attachment = blocking; attachment; attachment = attachment->att_blocking)
    if (attachment == tdbb->tdbb_attachment)
	ERR_post (gds__deadlock, 0);

if (block = dbb->dbb_free_btbs)
    dbb->dbb_free_btbs = block->btb_next;
else
    block = (BTB) ALLOCP (type_btb);

block->btb_thread_id = SCH_current_thread();
block->btb_next = *que;
*que = block;
attachment = tdbb->tdbb_attachment;
attachment->att_blocking = blocking;

SCH_hiber();

attachment->att_blocking = NULL;
}
#endif

#ifdef SUPERSERVER
USHORT JRD_getdir (
    TEXT	*buf,
    USHORT 	len
    )
{
/**************************************
 *
 *	J R D _ g e t d i r
 *
 **************************************
 *
 * Functional description
 *	Current working directory is cached in the attachment
 *	block.  get it out. This function could be called before 
 *	an attachment is created. In such a case thread specific
 *	data (t_data) will hold the user name which will be used 
 *	to get the users home directory.
 *
 **************************************/
char *t_data = NULL;

assert (buf != NULL);
assert (len > 0);

THD_getspecific_data ((void**)&t_data);

if (t_data)
   {
#ifdef WIN_NT
   GetCurrentDirectory (len, buf);
#else
   struct passwd *pwd;
   strcpy (buf, t_data);
   pwd = getpwnam (buf);
   if (pwd)
       strcpy (buf, pwd->pw_dir);
   else /* No home dir for this users here. Default to server dir */
       getcwd (buf, len);
#endif
   }
else
   {
   TDBB tdbb = NULL;
   ATT  attachment;

   tdbb = GET_THREAD_DATA;

   /** If the server has not done a SET_THREAD_DATA prior to this call
       (which will be the case when connecting via IPC), tdbb will
       be NULL so do not attempt to get the attachment handle from
       tdbb. Just return 0 as described below.  NOTE:  The only time
       this code is entered via IPC is if the database name = "".
   **/

   /** In case of backup/restore APIs, SET_THREAD_DATA has been done but
       the thread's context is a 'gbak' specific, so don't try extract
       attachment from there.
   **/

   if (tdbb && (tdbb->tdbb_thd_data.thdd_type == THDD_TYPE_TDBB))
       attachment = tdbb->tdbb_attachment;
   else
       return 0;

   /** 
    An older version of client will not be sending isc_dpb_working directory
    so in all probabilities attachment->att_working_directory will be null. 
    return 0 so that ISC_expand_filename will create the file in ibserver's dir
   **/
   if (!attachment->att_working_directory || 
	len - 1 < attachment->att_working_directory->str_length)
       return 0;
   memcpy (buf, attachment->att_working_directory->str_data, 
		attachment->att_working_directory->str_length);
   buf [attachment->att_working_directory->str_length] = 0;
   }

return strlen (buf);
}
#endif	/* SUPERSERVER */

void JRD_mutex_lock (
    MUTX	mutex)
{
/**************************************
 *
 *	J R D _ m u t e x _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Lock a mutex and note this fact
 *	in the thread context block.
 *
 **************************************/
TDBB	tdbb = NULL;

tdbb = GET_THREAD_DATA;
INUSE_insert (&tdbb->tdbb_mutexes, (void*) mutex, TRUE);
THD_MUTEX_LOCK (mutex);
}

void JRD_mutex_unlock (
    MUTX	mutex)
{
/**************************************
 *
 *	J R D _ m u t e x _ u n l o c k
 *
 **************************************
 *
 * Functional description
 *	Unlock a mutex and note this fact
 *	in the thread context block.
 *
 **************************************/
TDBB	tdbb = NULL;

tdbb = GET_THREAD_DATA;
INUSE_remove (&tdbb->tdbb_mutexes, (void*) mutex, FALSE);
THD_MUTEX_UNLOCK (mutex);
}

#ifdef SUPERSERVER 
void JRD_print_all_counters (
    char 	*fname)
{
/*****************************************************
 *
 *	J R D _ p r i n t _ a l l _ c o u n t e r s
 *
 *****************************************************
 *
 * Functional description
 *	print memory counters from DSQL, ENGINE & REMOTE
 *      component
 *
 ******************************************************/
IB_FILE 		*fptr;

if ( !trace_pools )
    return;

if ( !(fptr=ib_fopen(fname, "a+")))
    {
    char buff[256];
    sprintf(buff, "Failed to open %s", fname);
    gds__log (buff, NULL_PTR);
    return;
    }
V4_JRD_MUTEX_LOCK (databases_mutex);

ib_fprintf(fptr, "\nPrinting Block type Report\n");
ib_fprintf(fptr, "==========================\n");
gds_print_delta_counters(fptr);
ALL_print_memory_pool_info(fptr, databases);
ALLD_print_memory_pool_info(fptr);

V4_JRD_MUTEX_UNLOCK (databases_mutex);

ib_fclose(fptr);
}
#ifdef DEBUG_PROCS
void JRD_print_procedure_info (
    TDBB 	tdbb,
    char	*mesg)
{
/*****************************************************
 *
 *	J R D _ p r i n t _ p r o c e d u r e _ i n f o
 *
 *****************************************************
 *
 * Functional description
 *	print name , use_count of all procedures in  
 *      cache
 *
 ******************************************************/
IB_FILE 	*fptr;
TEXT	fname[MAXPATHLEN];
PRC     procedure, *ptr, *end;
VEC     procedures;

gds__prefix (fname, "proc_info.log");
if ( !(fptr=ib_fopen(fname, "a+")))
    {
    char buff[256];
    sprintf(buff, "Failed to open %s\n", fname);
    gds__log (buff, NULL_PTR);
    return;
    }

    if (mesg) ib_fputs (mesg, fptr);
    ib_fprintf (fptr, "Prc Name      , prc id , flags  ,  Use Count , Alter Count\n");

V4_JRD_MUTEX_LOCK (databases_mutex);

    if (procedures = tdbb->tdbb_database->dbb_procedures)
	{
	for (ptr=(PRC*)procedures->vec_object, end=ptr+procedures->vec_count;
	                         ptr < end; ptr++)
	    if (procedure = *ptr)
		ib_fprintf (fptr, "%s  ,  %d,  %X,  %d, %d\n", 
		(procedure->prc_name) ? (char*)procedure->prc_name->str_data : "NULL", 
		procedure->prc_id, procedure->prc_flags, 
		procedure->prc_use_count,procedure->prc_alter_count);
	}
    else
	ib_fprintf(fptr, "No Cached Procedures\n");

V4_JRD_MUTEX_UNLOCK (databases_mutex);

ib_fclose(fptr);

}
#endif /* DEBUG_PROCS */
#endif /* SUPERSERVER */

#ifdef MULTI_THREAD
BOOLEAN JRD_reschedule (
    TDBB	tdbb,
    SLONG	quantum,
    BOOLEAN	punt)
{
/**************************************
 *
 *	J R D _ r e s c h e d u l e
 *
 **************************************
 *
 * Functional description
 *	Somebody has kindly offered to relinquish
 *	control so that somebody else may run.
 *
 **************************************/
DBB	dbb;
ATT	attachment;
STATUS	*status;

if (tdbb->tdbb_inhibit)
    return FALSE;

/* Force garbage colletion activity to yield the
   processor in case client threads haven't had
   an opportunity to enter the scheduling queue. */

if (!(tdbb->tdbb_flags & TDBB_sweeper))
    SCH_schedule();
else
    {
    THREAD_EXIT;
    THREAD_YIELD;
    THREAD_ENTER;
    }

/* If database has been shutdown then get out */

dbb = tdbb->tdbb_database;
if (attachment = tdbb->tdbb_attachment)
    {
    if (dbb->dbb_ast_flags & DBB_shutdown &&
    	attachment->att_flags & ATT_shutdown)
	{
	if (punt)
	    {
	    CCH_unwind (tdbb, FALSE);
    	    ERR_post (isc_shutdown, 0);
	    }
	else
	    {
	    status = tdbb->tdbb_status_vector;
	    *status++ = isc_arg_gds;
	    *status++ = isc_shutdown;
	    *status++ = isc_arg_end;
	    return TRUE;
	    }
	}   
#ifdef CANCEL_OPERATION

    /* If a cancel has been raised, defer its acknowledgement
       when executing in the context of an internal request or
       the system transaction. */

    if ((attachment->att_flags & ATT_cancel_raise) &&
    	!(attachment->att_flags & ATT_cancel_disable))
	{
	REQ	request;
	TRA	transaction;

	if ((!(request = tdbb->tdbb_request) ||
	     !(request->req_flags & (req_internal | req_sys_trigger))) &&
	    (!(transaction = tdbb->tdbb_transaction) ||
	     !(transaction->tra_flags & TRA_system)))
	    {
	    attachment->att_flags &= ~ATT_cancel_raise;
	    if (punt)
		{
	    	CCH_unwind (tdbb, FALSE);
	    	ERR_post (isc_cancelled, 0);
		}
	    else
		{
	    	status = tdbb->tdbb_status_vector;
	    	*status++ = isc_arg_gds;
	    	*status++ = isc_cancelled;
	    	*status++ = isc_arg_end;
	    	return TRUE;
		}
	    }
	}
#endif
    }

tdbb->tdbb_quantum = (tdbb->tdbb_quantum <= 0) ?
    ((quantum) ? quantum : QUANTUM) : tdbb->tdbb_quantum;

return FALSE;
}
#endif

void JRD_restore_context (void)
{
/**************************************
 *
 *	J R D _ r e s t o r e _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Restore the previous thread specific data
 *	and cleanup and objects that remain in use.
 *
 **************************************/
TDBB	tdbb = NULL;
BOOLEAN	cleaned_up;

tdbb = GET_THREAD_DATA;

cleaned_up = INUSE_cleanup (&tdbb->tdbb_mutexes, (FPTR_VOID) THD_mutex_unlock);
cleaned_up |= INUSE_cleanup (&tdbb->tdbb_rw_locks, (FPTR_VOID) THD_wlck_unlock);
/* Charlie will fill this in
cleaned_up |= INUSE_cleanup (&tdbb->tdbb_pages, (FPTR_VOID) CCH_?);
*/
THD_restore_specific();

#ifdef DEV_BUILD
if (tdbb->tdbb_status_vector && !tdbb->tdbb_status_vector [1] && cleaned_up)
    gds__log ("mutexes or pages in use on successful return");
#endif
}

void JRD_set_context (
    TDBB	tdbb)
{
/**************************************
 *
 *	J R D _ s e t _ c o n t e x t
 *
 **************************************
 *
 * Functional description
 *	Set the thread specific data.  Initialize
 *	the in-use blocks so that we can unwind
 *	cleanly if an error occurs.
 *
 **************************************/

INUSE_clear (&tdbb->tdbb_mutexes);
INUSE_clear (&tdbb->tdbb_rw_locks);
INUSE_clear (&tdbb->tdbb_pages);
tdbb->tdbb_status_vector = NULL;
THD_put_specific ((THDD) tdbb);
tdbb->tdbb_thd_data.thdd_type = THDD_TYPE_TDBB;
}

#ifdef MULTI_THREAD
void JRD_unblock (
    BTB		*que)
{
/**************************************
 *
 *	J R D _ u n b l o c k
 *
 **************************************
 *
 * Functional description
 *	Unblock a linked list of blocked threads.  Rather
 *	than worrying about which, let 'em all loose.
 *
 **************************************/
DBB	dbb;
BTB	block;

dbb = GET_DBB;

while (block = *que)
    {
    *que = block->btb_next;
    if (block->btb_thread_id)
	SCH_wake ((struct thread*) block->btb_thread_id);
    block->btb_next = dbb->dbb_free_btbs;
    dbb->dbb_free_btbs = block;
    }
}
#endif 

void jrd_vtof (
    SCHAR	*string,
    SCHAR	*field,
    SSHORT	length)
{
/**************************************
 *
 *	j r d _ v t o f
 *
 **************************************
 *
 * Functional description
 *	Move a null terminated string to a fixed length
 *	field.  The call is primarily generated  by the
 *	preprocessor.
 *
 *	This is the same code as gds__vtof but is used internally.
 *
 **************************************/

while (*string)
    {
    *field++ = *string++;
    if (--length <= 0)
	return;
    }

if (length)
    do *field++ = ' '; while (--length);
}

#if (defined JPN_SJIS || defined JPN_EUC)
void jrd_vtof2 (
    SCHAR	*string,
    SCHAR	*field,
    SSHORT	length,
    USHORT	interp)
{
/**************************************
 *
 *	j r d _ v t o f 2
 *
 **************************************
 *
 * Functional description
 *	Move a null terminated string to a fixed length
 *	field.  The call is primarily generated  by the
 *	preprocessor.
 *	Avoid truncation in between a double byte character.
 *
 *	This is the same code as gds__vtof but is used internally.
 *
 **************************************/
SSHORT	bytes_copied;
SCHAR	*saved_field;

bytes_copied = 0;
saved_field = field;

while (*string)
    {
    *field++ = *string++;
    bytes_copied++;
    if (--length <= 0)
	return;
    }

if (interp == gds__interp_jpn_sjis &&
    KANJI_check_sjis (saved_field, bytes_copied))
    *(field - 1) = ' ';
else if (interp == gds__interp_jpn_euc &&
    KANJI_check_euc (saved_field, bytes_copied))
    *(field - 1) = ' ';

if (length)
    do *field++ = ' '; while (--length);
}
#endif

static BLB check_blob (
    TDBB	tdbb,
    STATUS	*user_status,
    BLB		*blob_handle)
{
/**************************************
 *
 *	c h e c k _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Check out a blob handle for goodness.  Return
 *	the address of the blob if ok, NULL otherwise.
 *
 **************************************/
BLB	blob;
TRA	transaction;

SET_TDBB (tdbb);
blob = *blob_handle;

if (!blob || 
    (blob->blb_header.blk_type != (UCHAR) type_blb) ||
    check_database (tdbb, blob->blb_attachment, user_status) ||
    !(transaction = blob->blb_transaction) ||
    transaction->tra_header.blk_type != (UCHAR) type_tra)
    {
    handle_error (user_status, gds__bad_segstr_handle, tdbb);
    return NULL;
    }

tdbb->tdbb_transaction = transaction;

return blob;
}

static STATUS check_database (
    TDBB	tdbb,
    ATT		attachment,
    STATUS	*user_status)
{
/**************************************
 *
 *	c h e c k _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Check an attachment for validity, setting
 *	up environment.
 *
 **************************************/
DBB	dbb;
STATUS	*ptr;
TEXT    *string;
ATT	attach;

SET_TDBB (tdbb);

/* Make sure blocks look and feel kosher */

if (!attachment || 
    (attachment->att_header.blk_type != (UCHAR) type_att) ||
    !(dbb = attachment->att_database) ||
    dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return handle_error (user_status, gds__bad_db_handle, tdbb);

/* Make sure this is a valid attachment */

#ifndef SUPERSERVER
for (attach = dbb->dbb_attachments; attach; attach = attach->att_next)
    if (attach == attachment)
	break;

if (!attach)
    return handle_error (user_status, gds__bad_db_handle, tdbb);
#endif

tdbb->tdbb_database = dbb;
tdbb->tdbb_attachment = attachment;
tdbb->tdbb_quantum = QUANTUM;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_default = NULL;
tdbb->tdbb_inhibit = 0;
tdbb->tdbb_flags = NULL;

/* Count active threads in database */

++dbb->dbb_use_count;

if (dbb->dbb_flags & DBB_bugcheck)
    {
    tdbb->tdbb_status_vector = ptr = user_status;
    *ptr++ =  gds_arg_gds;
    *ptr++ =  gds__bug_check;
    *ptr++ =  gds_arg_string;
    string =  "can't continue after bugcheck";
    *ptr++ = (STATUS) string;
    *ptr = gds_arg_end;
    return error (user_status);
    }

if (attachment->att_flags & ATT_shutdown ||
    (dbb->dbb_ast_flags & DBB_shutdown &&
    !(attachment->att_user->usr_flags & (USR_locksmith | USR_owner))))
    {
    tdbb->tdbb_status_vector = ptr = user_status;
    *ptr++ =  gds_arg_gds;
    *ptr++ =  gds__shutdown;
    *ptr++ =  gds_arg_cstring;
    *ptr++ =  dbb->dbb_filename->str_length;
    string =  dbb->dbb_filename->str_data;
    *ptr++ = (STATUS) string;
    *ptr = gds_arg_end;
    return error (user_status);
    }

#ifdef CANCEL_OPERATION
if ((attachment->att_flags & ATT_cancel_raise) &&
    !(attachment->att_flags & ATT_cancel_disable))
    {
    attachment->att_flags &= ~ATT_cancel_raise;
    tdbb->tdbb_status_vector = ptr = user_status;
    *ptr++ = isc_arg_gds;
    *ptr++ = isc_cancelled;
    *ptr++ = isc_arg_end;
    return error (user_status);
    }
#endif

return SUCCESS;
}

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
 *	Exit handler for image exit.
 *
 **************************************/

V4_MUTEX_DESTROY (databases_mutex);
JRD_SS_DESTROY_MUTEX;
initialized = FALSE;
databases = NULL;
}

static STATUS commit (
    STATUS	*user_status,
    TRA		*tra_handle,
    USHORT	retaining_flag)
{
/**************************************
 *
 *	c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit a transaction.
 *
 **************************************/
TRA		transaction, next;
STATUS		*ptr;
DBB		dbb;
JMP_BUF		env;
struct tdbb	thd_context, *tdbb = NULL;

SET_THREAD_DATA;

CHECK_HANDLE ((*tra_handle), type_tra, gds__bad_trans_handle);
next = transaction = *tra_handle;

if (check_database (tdbb, transaction->tra_attachment, user_status))
    return user_status [1];

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
LOG_call ((retaining_flag) ? log_commit_retaining : log_commit, *tra_handle);
#endif

ptr = user_status;

if (transaction->tra_sibling && 
    !(transaction->tra_flags & TRA_prepared) &&
    prepare (tdbb, transaction, ptr, 0, NULL))
    return error (user_status);
    
if (SETJMP (env))
    {
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    return error (user_status);
    }

while (transaction = next)
    {
    next = transaction->tra_sibling;
    check_database (tdbb, transaction->tra_attachment, user_status);
    tdbb->tdbb_setjmp = (UCHAR*) env;
    tdbb->tdbb_status_vector = ptr;
    TRA_commit (tdbb, transaction, retaining_flag);
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    }

RETURN_SUCCESS;
}

static STR copy_string (
    TEXT	*ptr,
    USHORT	length)
{
/**************************************
 *
 *	c o p y _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Copy a string to a string block.
 *
 **************************************/
DBB	dbb;
STR	string;

dbb = GET_DBB;

string = (STR) ALLOCPV (type_str, length);
string->str_length = length;
MOVE_FAST (ptr, string->str_data, length);

return string;
}

static BOOLEAN drop_files (
    FIL		file)
{
/**************************************
 *
 *	d r o p _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	drop a linked list of files
 *
 **************************************/
DBB	dbb;
STATUS	status [ISC_STATUS_LENGTH];

status [1] = SUCCESS;

for (; file; file = file->fil_next)
    if (unlink (file->fil_string))
	{
 	IBERR_build_status (status, isc_io_error, 
	    gds_arg_string, "unlink", 
	    gds_arg_string, ERR_cstring (file->fil_string),
        isc_arg_gds, isc_io_delete_err,
	    SYS_ERR, errno, 0);
	dbb = GET_DBB;
	gds__log_status (dbb->dbb_file->fil_string, status);
	}

return status [1] ? TRUE : FALSE;
}

static TRA find_transaction (
    TDBB	tdbb,
    TRA		transaction,
    STATUS	error_code)
{
/**************************************
 *
 *	f i n d _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Find the element of a possible multiple database transaction
 *	that corresponds to the current database.
 *
 **************************************/

SET_TDBB (tdbb);

if (!transaction || transaction->tra_header.blk_type != (UCHAR) type_tra)
    ERR_post (gds__bad_trans_handle, 0);

for (; transaction; transaction = transaction->tra_sibling)
    if (transaction->tra_attachment == tdbb->tdbb_attachment)
	{
	tdbb->tdbb_transaction = transaction;
	return transaction;
	}

ERR_post (error_code, 0);
return ((TRA) NULL); 	/* Added to remove compiler warnings */
}

static STATUS error (
    STATUS	*user_status)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	An error returned has been trapped.  Return a status code.
 *
 **************************************/
TDBB	tdbb = NULL;
DBB	dbb;

tdbb = GET_THREAD_DATA;

/* Decrement count of active threads in database */

if (dbb = tdbb->tdbb_database)
    --dbb->dbb_use_count;

RESTORE_THREAD_DATA;

/* This is debugging code which is meant to verify that
   the database use count is cleared on exit from the
   engine. Database shutdown cannot succeed if the database
   use count is erroneously left set. */

#if (defined DEV_BUILD && !defined MULTI_THREAD)
if (dbb && dbb->dbb_use_count && !(dbb->dbb_flags & DBB_security_db))
    {
    STATUS	*p;

    dbb->dbb_use_count = 0;
    p = user_status;
    *p++ = gds_arg_gds;
    *p++ = gds__random;
    *p++ = gds_arg_string;
    *p++ = (STATUS) "database use count set on error return";
    *p = gds_arg_end;
    }
#endif

return user_status [1];
}

static void find_intl_charset (
    TDBB	tdbb,
    ATT		attachment,
    DPB		*options)
{
/**************************************
 *
 *	f i n d _ i n t l _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *	Attachment has declared it's prefered character set
 *	as part of LC_CTYPE, passed over with the attachment
 *	block.  Now let's resolve that to an internal subtype id.
 *
 **************************************/
SSHORT	id, len;
STATUS	local_status [ISC_STATUS_LENGTH];

SET_TDBB (tdbb);

if (!options->dpb_lc_ctype || (len = strlen (options->dpb_lc_ctype)) == 0)
    {
    /* No declaration of character set, act like 3.x Interbase */

    attachment->att_charset = CS_NONE;
    }
else if (MET_get_char_subtype (tdbb, &id, options->dpb_lc_ctype, len) &&
         INTL_defined_type (tdbb, local_status, id) &&
	 (id != CS_BINARY))
    attachment->att_charset = id;
else
    {
    /* Report an error - we can't do what user has requested */

    ERR_post (gds__bad_dpb_content, gds_arg_gds, gds__charset_not_found, 
	gds_arg_string, ERR_cstring (options->dpb_lc_ctype), 0);
    }
}

static void get_options (
    UCHAR	*dpb,
    USHORT	dpb_length,
    TEXT	**scratch,
    DPB		*options)
{
/**************************************
 *
 *	g e t _ o p t i o n s
 *
 **************************************
 *
 * Functional description
 *	Parse database parameter block picking up options and things.
 *
 **************************************/
DBB	dbb;
UCHAR	*p, *end_dpb, *single;
USHORT	l;
SSHORT	num_old_files = 0;

dbb = GET_DBB;

MOVE_CLEAR (options, (SLONG) sizeof (struct dpb));

options->dpb_buffers = JRD_cache_default;
options->dpb_sweep_interval = -1;
options->dpb_wal_grp_cmt_wait = -1;
options->dpb_overwrite = TRUE;
options->dpb_sql_dialect = 99;
invalid_client_SQL_dialect = FALSE;
p = dpb;
end_dpb = p + dpb_length;

if ((dpb == NULL) && (dpb_length > 0))
    ERR_post (gds__bad_dpb_form, 0);

if (p < end_dpb && *p++ != gds__dpb_version1)
    ERR_post (gds__bad_dpb_form, gds_arg_gds, gds__wrodpbver, 0);

while (p < end_dpb)
    switch (*p++)
	{
	case isc_dpb_working_directory:
	    {
	    char *t_data;
	    options->dpb_working_directory = get_string_parameter (&p, scratch);

	    THD_getspecific_data((void**)&t_data);

	    /* 
	       Null value for working_directory implies remote database. So get
	       the users HOME directory
	    */
#ifndef WIN_NT
	    if (!options->dpb_working_directory[0])
	        {
	        struct passwd *passwd = (struct passwd *) NULL;

		if (t_data)
		    passwd = getpwnam (t_data);
   		if (passwd)
		    {
		    strcpy (*scratch, passwd->pw_dir);
		    l = strlen (passwd->pw_dir) + 1;
		    }
                else /*No home dir for this users here. Default to server dir*/
		    {
                    getcwd (*scratch, MAXPATHLEN);
		    l = strlen (*scratch) + 1;
		    }
		options->dpb_working_directory = *scratch;
		*scratch += l;
	    }
#endif
	    /* Null out the thread local data so that further references will fail */
	    free(t_data);
	    t_data=NULL;
	    THD_putspecific_data((void*)t_data);
	    }
	    break;

	case isc_dpb_set_page_buffers:
	    options->dpb_page_buffers = get_parameter (&p);
	    if (options->dpb_page_buffers &&
		(options->dpb_page_buffers < MIN_PAGE_BUFFERS ||
		 options->dpb_page_buffers > MAX_PAGE_BUFFERS))
		ERR_post (gds__bad_dpb_content, 0);
	    options->dpb_set_page_buffers = TRUE;
	    break;

	case gds__dpb_num_buffers:
	    options->dpb_buffers = get_parameter (&p);
	    if (options->dpb_buffers < 10)
		ERR_post (gds__bad_dpb_content, 0);
	    break;

	case gds__dpb_page_size:
	    options->dpb_page_size = get_parameter (&p);
	    break;

	case gds__dpb_debug:
	    options->dpb_debug = get_parameter (&p);
	    break;

	case gds__dpb_sweep:
	    options->dpb_sweep = get_parameter (&p);
	    break;

	case gds__dpb_sweep_interval:
	    options->dpb_sweep_interval = get_parameter (&p);
	    break;

	case gds__dpb_verify:
	    options->dpb_verify = get_parameter (&p);
	    if (options->dpb_verify & gds__dpb_ignore)
		dbb->dbb_flags |= DBB_damaged;
	    break;

	case gds__dpb_trace:
	    options->dpb_trace = get_parameter (&p);
	    break;

	case gds__dpb_damaged:
	    if (get_parameter (&p) & 1)
		dbb->dbb_flags |= DBB_damaged;
	    break;

	case gds__dpb_enable_journal:
	    options->dpb_journal = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_wal_backup_dir:
	    options->dpb_wal_backup_dir = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_drop_walfile:
	    options->dpb_wal_action = get_parameter (&p);
	    break;

	case gds__dpb_old_dump_id:
	    options->dpb_old_dump_id = get_parameter (&p);
	    break;

	case gds__dpb_online_dump:
	    options->dpb_online_dump = get_parameter (&p);
	    break;

	case gds__dpb_old_file_size:
	    options->dpb_old_file_size = get_parameter (&p);
	    break;

	case gds__dpb_old_num_files:
	    options->dpb_old_num_files = get_parameter (&p);
	    break;

	case gds__dpb_old_start_page:
	    options->dpb_old_start_page = get_parameter (&p);
	    break;

	case gds__dpb_old_start_seqno:
	    options->dpb_old_start_seqno = get_parameter (&p);
	    break;

	case gds__dpb_old_start_file:
	    options->dpb_old_start_file = get_parameter (&p);
	    break;

	case gds__dpb_old_file:
	    if (num_old_files >= MAX_OLD_FILES)
		ERR_post (gds__num_old_files, 0);

	    options->dpb_old_file [num_old_files] = get_string_parameter (&p, scratch);
	    num_old_files++;
	    break;

	case gds__dpb_wal_chkptlen:
	    options->dpb_wal_chkpt_len = get_parameter (&p);
	    break;

	case gds__dpb_wal_numbufs:
	    options->dpb_wal_num_bufs = get_parameter (&p);
	    break;

	case gds__dpb_wal_bufsize:
	    options->dpb_wal_bufsize = get_parameter (&p);
	    break;

	case gds__dpb_wal_grp_cmt_wait:
	    options->dpb_wal_grp_cmt_wait = get_parameter (&p);
	    break;

	case gds__dpb_dbkey_scope:
	    options->dpb_dbkey_scope = get_parameter (&p);
	    break;

	case gds__dpb_sys_user_name:
	    options->dpb_sys_user_name = get_string_parameter (&p, scratch);
	    break;

	case isc_dpb_sql_role_name:
	    options->dpb_role_name = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_user_name:
	    options->dpb_user_name = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_password:
	    options->dpb_password = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_password_enc:
	    options->dpb_password_enc = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_encrypt_key:
#ifdef ISC_DATABASE_ENCRYPTION
	    options->dpb_key = get_string_parameter (&p, scratch);
#else
	    /* Just in case there WAS a customer using this unsupported
	     * feature - post an error when they try to access it in 4.0
	     */
	    ERR_post (gds__uns_ext, gds_arg_gds, gds__random,
			gds_arg_string, "Encryption not supported",
			0);
#endif
	    break;

	case gds__dpb_no_garbage_collect:
	    options->dpb_no_garbage = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_disable_journal:
	    options->dpb_disable = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_activate_shadow:
	    options->dpb_activate_shadow = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_delete_shadow:
	    options->dpb_delete_shadow = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_force_write:
	    options->dpb_set_force_write = TRUE;
	    options->dpb_force_write = get_parameter (&p);
	    break;

	case gds__dpb_begin_log:
	    options->dpb_log = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_quit_log:
	    options->dpb_quit_log = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_no_reserve:
	    options->dpb_set_no_reserve = TRUE;
	    options->dpb_no_reserve = get_parameter (&p);
	    break;

	case gds__dpb_interp:
	    options->dpb_interp = get_parameter (&p);
	    break;

	case gds__dpb_lc_messages:
	    options->dpb_lc_messages = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_lc_ctype:
	    options->dpb_lc_ctype = get_string_parameter (&p, scratch);
	    break;

	case gds__dpb_shutdown:
	    options->dpb_shutdown = get_parameter (&p);
	    break;

	case gds__dpb_shutdown_delay:
	    options->dpb_shutdown_delay = get_parameter (&p);
	    break;

	case gds__dpb_online:
	    options->dpb_online = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case isc_dpb_reserved:
	    single = get_string_parameter (&p, scratch);
	    if (!strcmp (single, "YES"))
		options->dpb_single_user = TRUE;
	    break;

	case isc_dpb_overwrite:
	    options->dpb_overwrite = get_parameter (&p);
	    break;

	case isc_dpb_sec_attach:
	    options->dpb_sec_attach = get_parameter (&p);
	    options->dpb_buffers = 50;
	    dbb->dbb_flags |= DBB_security_db;
	    break;

	case isc_dpb_gbak_attach:
	    options->dpb_gbak_attach = get_string_parameter (&p, scratch);
	    break;

	case isc_dpb_gstat_attach:
	    options->dpb_gstat_attach = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case isc_dpb_gfix_attach:
	    options->dpb_gfix_attach = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case isc_dpb_disable_wal:
	    options->dpb_disable_wal = TRUE;
	    l = *p++;
	    p += l;
	    break;

	case gds__dpb_connect_timeout:
	    options->dpb_connect_timeout = get_parameter (&p);
	    break;

        case gds__dpb_dummy_packet_interval:
	    options->dpb_dummy_packet_interval = get_parameter (&p);
	    break;

        case isc_dpb_sql_dialect:
	    options->dpb_sql_dialect = get_parameter (&p);
	    if (options->dpb_sql_dialect < 0 || 
		options->dpb_sql_dialect > SQL_DIALECT_V6)
		invalid_client_SQL_dialect = TRUE;
	    break;

        case isc_dpb_set_db_sql_dialect:
	    options->dpb_set_db_sql_dialect = get_parameter (&p);
	    break;
	    
#ifdef READONLY_DATABASE
	case isc_dpb_set_db_readonly:
	    options->dpb_set_db_readonly = TRUE;
	    options->dpb_db_readonly = get_parameter (&p);
	    break;
#endif

	default:
	    l = *p++;
	    p += l;
	}

if (p != end_dpb)
    ERR_post (gds__bad_dpb_form, 0);

}

static SLONG get_parameter (
    UCHAR	**ptr)
{
/**************************************
 *
 *	g e t _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Pick up a VAX format parameter from a parameter block, including the
 *	length byte.
 *
 **************************************/
SLONG	parameter;
SSHORT	l;

l = *(*ptr)++;
parameter = gds__vax_integer (*ptr, l);
*ptr += l;

return parameter;
}

static TEXT* get_string_parameter (
    UCHAR	**dpb_ptr,
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
TEXT	*opt;
UCHAR	*dpb;
USHORT	l;

opt = *opt_ptr;
dpb = *dpb_ptr;

if (l = *(dpb++))
    do *opt++ = *dpb++; while (--l);

*opt++ = 0;
*dpb_ptr = dpb;
dpb = (UCHAR *) *opt_ptr;
*opt_ptr = opt;

return (TEXT *) dpb;
}

static STATUS handle_error (
    STATUS	*user_status,
    STATUS	code,
    TDBB	tdbb)
{
/**************************************
 *
 *	h a n d l e _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	An invalid handle has been passed in.  If there is a user status
 *	vector, make it reflect the error.  If not, emulate the routine
 *	"error" and abort.
 *
 **************************************/
STATUS	*vector;

if (tdbb)
    RESTORE_THREAD_DATA;

vector = user_status;
*vector++ = gds_arg_gds;
*vector++ = code;
*vector = gds_arg_end;

return code;
}

#if defined (WIN_NT) && !defined(SERVER_SHUTDOWN)
static BOOLEAN  handler_NT (
    SSHORT      controlAction)
{
/**************************************
 *
 *      h a n d l e r _ N T
 *
 **************************************
 *
 * Functional description
 *	For some actions, get NT to issue a popup asking
 *	the user to delay.
 *
 **************************************/

switch( controlAction)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
	return TRUE;		/* NT will issue popup */

    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
	return FALSE;		/* let it go */
    }
}
#endif

static DBB init (
    TDBB	tdbb,
    STATUS	*user_status,
    TEXT	*expanded_filename,
    USHORT	attach_flag)
{
/**************************************
 *
 *	i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize for database access.  First call from both CREATE and
 *	OPEN.
 *
 **************************************/
DBB		dbb;
STR		string;
VEC		vector;
struct dbb	temp;
MUTX_T		temp_mutx [DBB_MUTX_max];
WLCK_T		temp_wlck [DBB_WLCK_max];
JMP_BUF		env;

SET_TDBB (tdbb);

/* If this is the first time through, initialize local mutexes and set
   up a cleanup handler.  Regardless, then lock the database mutex. */

if (!initialized)
    {
    THD_INIT;
    THREAD_EXIT;
    THD_GLOBAL_MUTEX_LOCK;
    THREAD_ENTER;
    if (!initialized)
	{
	V4_MUTEX_INIT (databases_mutex);
        JRD_SS_INIT_MUTEX;
        gds__register_cleanup (cleanup, NULL_PTR);
	initialized = TRUE;
	ISC_get_config(LOCK_HEADER, JRD_hdrtbl);
	if (JRD_cache_default < MIN_PAGE_BUFFERS)
	    JRD_cache_default = MIN_PAGE_BUFFERS;
	if (JRD_cache_default > MAX_PAGE_BUFFERS)
	    JRD_cache_default = MAX_PAGE_BUFFERS;
#if defined (WIN_NT) && !defined(SERVER_SHUTDOWN)
	setup_NT_handlers();
#endif
	}
    THD_GLOBAL_MUTEX_UNLOCK;
    }

JRD_SS_MUTEX_LOCK;
V4_JRD_MUTEX_LOCK (databases_mutex);

/* Check to see if the database is already actively attached */

for (dbb = databases; dbb; dbb = dbb->dbb_next)
    if (!(dbb->dbb_flags & (DBB_bugcheck | DBB_not_in_use)) && 
#ifndef SUPERSERVER
        !(dbb->dbb_ast_flags & DBB_shutdown && dbb->dbb_ast_flags & DBB_shutdown_locks) && 
#endif
	(string = dbb->dbb_filename) && 
	!strcmp (string->str_data, expanded_filename))
	return (attach_flag) ? dbb : NULL;

/* Clean up temporary DBB and initialize a SETJMP for error reporting */

MOVE_CLEAR (&temp, (SLONG) sizeof (struct dbb));
THD_MUTEX_INIT_N (temp_mutx, DBB_MUTX_max);
V4_RW_LOCK_INIT_N (temp_wlck, DBB_WLCK_max);
                                    
/* set up the temporary database block with fields that are 
   required for doing the ALL_init() */

temp.dbb_header.blk_type = type_dbb;
temp.dbb_mutexes = temp_mutx;
temp.dbb_rw_locks = temp_wlck;

tdbb->tdbb_database = dbb = &temp;
tdbb->tdbb_setjmp = (UCHAR*) env;
tdbb->tdbb_status_vector = user_status;
if (SETJMP (env))
    return NULL_PTR;
ALL_init();

tdbb->tdbb_database = dbb = (DBB) ALLOCP (type_dbb);

THD_MUTEX_DESTROY_N (temp_mutx, DBB_MUTX_max);
V4_RW_LOCK_DESTROY_N (temp_wlck, DBB_WLCK_max);

MOVE_FAST ((UCHAR*) &temp + sizeof (struct blk),
	  (UCHAR*) dbb  + sizeof (struct blk),
	  sizeof (struct dbb) - sizeof (struct blk));

dbb->dbb_next = databases;
databases = dbb;

string = (STR) ALLOCPV (type_str, THREAD_STRUCT_SIZE (MUTX_T, DBB_MUTX_max));
dbb->dbb_mutexes = (MUTX) THREAD_STRUCT_ALIGN (string->str_data);
THD_MUTEX_INIT_N (dbb->dbb_mutexes, DBB_MUTX_max);
string = (STR) ALLOCPV (type_str, THREAD_STRUCT_SIZE (WLCK_T, DBB_WLCK_max));
dbb->dbb_rw_locks = (WLCK) THREAD_STRUCT_ALIGN (string->str_data);
V4_RW_LOCK_INIT_N (dbb->dbb_rw_locks, DBB_WLCK_max);
dbb->dbb_internal = vector = (VEC) ALLOCPV (type_vec, irq_MAX);
vector->vec_count = (SSHORT) irq_MAX;
dbb->dbb_dyn_req = vector = (VEC) ALLOCPV (type_vec, drq_MAX);
vector->vec_count = (SSHORT) drq_MAX;
dbb->dbb_flags |= DBB_exclusive;
dbb->dbb_sweep_interval = SWEEP_INTERVAL;

/* Initialize a number of subsystems */

TRA_init (tdbb);

#ifdef ISC_DATABASE_ENCRYPTION
/* Lookup some external "hooks" */

#ifndef WINDOWS_ONLY
if (dbb->dbb_encrypt = ISC_lookup_entrypoint (ENCRYPT_IMAGE, ENCRYPT, NULL))
    dbb->dbb_decrypt = ISC_lookup_entrypoint (ENCRYPT_IMAGE, DECRYPT, NULL);
#endif 
#endif

INTL_init (tdbb);

return dbb;
}

static void make_jrn_data (
    UCHAR	*data,
    USHORT	*ret_len,
    TEXT	*db_name,
    USHORT	db_len,
    TEXT	*backup_dir,
    USHORT	b_length)
{
/**************************************
 *
 *	m a k e _ j r n _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Encode the database and backup information into a string.
 *	Returns the length of the string.
 *
 **************************************/
UCHAR	*q, *t;
int	len;

t = data;

if (len = b_length)
    {
    *t++ = gds__dpb_wal_backup_dir;
    *t++ = b_length;
    q = backup_dir;
    do *t++ = *q++; while (--len);
    }

if (len = db_len)
    {
    *t++ = JRNW_DB_NAME;
    *t++ = db_len;
    q = db_name;
    do *t++ = *q++; while (--len);
    }
*t++ = JRNW_END;

*ret_len = t - data;
}

static STATUS prepare (
    TDBB	tdbb,
    TRA		transaction,
    STATUS	*status_vector,
    USHORT	length,
    UCHAR	*msg)
{
/**************************************
 *
 *	p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Attempt to prepare a transaction.  If it fails at any point, return
 *	an error.
 *
 **************************************/
DBB	dbb;
JMP_BUF	env;

SET_TDBB (tdbb);

if (SETJMP (env))
    {
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    return status_vector [1];
    }

for (; transaction; transaction = transaction->tra_sibling)
    {
    check_database (tdbb, transaction->tra_attachment, status_vector);
    tdbb->tdbb_setjmp = (UCHAR*) env;
    tdbb->tdbb_status_vector = status_vector;
    TRA_prepare (tdbb, transaction, length, msg);
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    }

return SUCCESS;
}

static void release_attachment (
    ATT		attachment)
{
/**************************************
 *
 *	r e l e a s e _ a t t a c h m e n t
 *
 **************************************
 *
 * Functional description
 *	Disconnect attachment block from database block.
 *	NOTE:  This routine assumes that upon entry,
 *	mutex DBB_MUTX_init_fini will be locked.
 *	Before exiting, there is a handoff from this
 *	mutex to the databases_mutex mutex.  Upon exit,
 *	that mutex remains locked.  It is the
 *	responsibility of the caller to unlock it.
 *
 **************************************/
DBB	dbb;
ATT	*ptr;
VCL	*vector;
VEC	lock_vector;
LCK	*lock, record_lock;
USHORT	i;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

if (!attachment)
    {
    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
    V4_JRD_MUTEX_LOCK (databases_mutex);
    return;
    }

#ifndef WINDOWS_ONLY    /* Events are not supported under LIBS */
if (attachment->att_event_session)
    EVENT_delete_session (attachment->att_event_session);
#endif

if (attachment->att_id_lock)
    LCK_release (tdbb, attachment->att_id_lock);

for (vector = attachment->att_counts; vector < attachment->att_counts + DBB_max_count; ++vector)
    if (*vector)
	ALL_RELEASE (*vector);

if (attachment->att_working_directory)
    ALL_RELEASE (attachment->att_working_directory);

if (attachment->att_lc_messages)
    ALL_RELEASE (attachment->att_lc_messages);

/* Release any validation error vector allocated */

if (attachment->att_val_errors)
    {
    ALL_RELEASE (attachment->att_val_errors);
    attachment->att_val_errors = (VCL)0;
    }

/* Release the persistent locks taken out during the attachment */

if (lock_vector = attachment->att_relation_locks)
    {
    for (i = 0, lock = (LCK*) lock_vector->vec_object; i < lock_vector->vec_count; i++, lock++)
	if (*lock)
	    {
	    LCK_release (tdbb, *lock);
	    ALL_RELEASE (*lock);
	    }
    ALL_RELEASE (lock_vector);
    }

for (record_lock = attachment->att_record_locks; record_lock; record_lock = record_lock->lck_att_next)
    LCK_release (tdbb, record_lock);
        
/* bug #7781, need to null out the attachment pointer of all locks which 
   were hung off this attachment block, to ensure that the attachment 
   block doesn't get dereferenced after it is released */

for (record_lock = attachment->att_long_locks; record_lock; record_lock = record_lock->lck_next)
    record_lock->lck_attachment = NULL;

if (attachment->att_flags & ATT_lck_init_done)
    LCK_fini (tdbb, LCK_OWNER_attachment);    /* For the attachment */

if (attachment->att_compatibility_table)
    ALL_RELEASE (attachment->att_compatibility_table);

V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
V4_JRD_MUTEX_LOCK (databases_mutex);

if (dbb->dbb_header.blk_type != (UCHAR) type_dbb)
    return;

/* remove the attachment block from the dbb linked list */

for (ptr = &dbb->dbb_attachments; *ptr; ptr = &(*ptr)->att_next)
    if (*ptr == attachment)
	{
	*ptr = attachment->att_next;
	break;
	}
}

static STATUS return_success (
    TDBB	tdbb)
{
/**************************************
 *
 *	r e t u r n _ s u c c e s s
 *
 **************************************
 *
 * Functional description
 *	Set up status vector to reflect successful execution.
 *
 **************************************/
DBB	dbb;
STATUS	*p, *user_status;

SET_TDBB (tdbb);

/* Decrement count of active threads in database */

if (dbb = tdbb->tdbb_database)
    --dbb->dbb_use_count;

p = user_status = tdbb->tdbb_status_vector;

/* If the status vector has not been initialized, then 
   initilalize the status vector to indicate success.  
   Else pass the status vector along at it stands.  */
if (p [0] != gds_arg_gds ||
    p [1] != SUCCESS ||
    (p [2] != gds_arg_end && p [2] != gds_arg_gds && p [2] != isc_arg_warning))
    {
    *p++ = gds_arg_gds;
    *p++ = SUCCESS;
    *p = gds_arg_end;
    }

RESTORE_THREAD_DATA;

/* This is debugging code which is meant to verify that
   the database use count is cleared on exit from the
   engine. Database shutdown cannot succeed if the database
   use count is erroneously left set. */

#if (defined DEV_BUILD && !defined MULTI_THREAD)
if (dbb && dbb->dbb_use_count && !(dbb->dbb_flags & DBB_security_db))
    {
    dbb->dbb_use_count = 0;
    p = user_status;
    *p++ = gds_arg_gds;
    *p++ = gds__random;
    *p++ = gds_arg_string;
    *p++ = (STATUS) "database use count set on success return";
    *p = gds_arg_end;
    }
#endif

return user_status [1];
}

static BOOLEAN rollback (
    TDBB	tdbb,
    TRA		next,
    STATUS	*status_vector,
    USHORT	retaining_flag)
{
/**************************************
 *
 *	r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Abort a transaction.
 *
 **************************************/
DBB	dbb;
TRA	transaction;
STATUS	local_status [ISC_STATUS_LENGTH];
JMP_BUF	env;

SET_TDBB (tdbb);

while (transaction = next)
    {
    next = transaction->tra_sibling;
    check_database (tdbb, transaction->tra_attachment, status_vector);
    if (SETJMP (env))
	{
	status_vector = local_status;
	dbb = tdbb->tdbb_database;
	--dbb->dbb_use_count;
	continue;
	}
    tdbb->tdbb_setjmp = (UCHAR*) env;
    tdbb->tdbb_status_vector = status_vector;
    TRA_rollback (tdbb, transaction, retaining_flag);
    dbb = tdbb->tdbb_database;
    --dbb->dbb_use_count;
    }

return (status_vector == local_status);
}

#if defined (WIN_NT) && !defined(SERVER_SHUTDOWN)
static void setup_NT_handlers()
{
/**************************************
 *
 *      s e t u p _ N T _ h a n d l e r s
 *
 **************************************
 *
 * Functional description
 *      Set up Windows NT console control handlers for
 *      things that can happen.   The handler used for
 *      all cases, handler_NT(), will flush and close
 *      any open database files.
 *
 **************************************/

(void)SetConsoleCtrlHandler( (PHANDLER_ROUTINE)handler_NT, TRUE);
}
#endif

static void shutdown_database (
    DBB		dbb,
    BOOLEAN	release_pools)
{
/**************************************
 *
 *	s h u t d o w n _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Shutdown physical database environment.
 *	NOTE:  This routine assumes that upon entry,
 *	mutex databases_mutex will be locked.
 *
 **************************************/
TDBB	tdbb = NULL;
VEC	vector;
REL	*ptr, *end;
DBB	*d_ptr;
USHORT	i;

tdbb = GET_THREAD_DATA;

/* Shutdown file and/or remote connection */

#ifdef SUPERSERVER_V2
TRA_header_write (tdbb, dbb, 0L); /* Update transaction info on header page. */
#endif

#ifdef GARBAGE_THREAD
VIO_fini (tdbb);
#endif
CMP_fini (tdbb);
CCH_fini (tdbb);
FUN_fini (tdbb);

if (dbb->dbb_shadow_lock)
    LCK_release (tdbb, dbb->dbb_shadow_lock);

if (dbb->dbb_retaining_lock)
    LCK_release (tdbb, dbb->dbb_retaining_lock);

if (dbb->dbb_lock)
    LCK_release (tdbb, dbb->dbb_lock);

#ifndef WINDOWS_ONLY
if (dbb->dbb_wal)
    AIL_fini();

if (dbb->dbb_journal)
    JRN_fini (tdbb->tdbb_status_vector, &dbb->dbb_journal);
#endif

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
if (dbb->dbb_log)
    LOG_fini();
#endif

/* Shut down any extern relations */

if (vector = dbb->dbb_relations)
    for (ptr = (REL*) vector->vec_object, end = ptr + vector->vec_count; ptr < end; ptr++)
	if (*ptr && (*ptr)->rel_file)
	    EXT_fini (*ptr);

for (d_ptr = &databases; *(d_ptr); d_ptr = &(*d_ptr)->dbb_next)
    if (*d_ptr == dbb)
	{
	*d_ptr = dbb->dbb_next;
	break;
	}

if (dbb->dbb_flags & DBB_lck_init_done)
    {
    LCK_fini (tdbb, LCK_OWNER_database);    /* For the database */
    dbb->dbb_flags &= ~DBB_lck_init_done;
    }

/* Remove objects from the in-use strutures before destroying them */

for (i = 0; i < DBB_MUTX_max; i++)
    INUSE_remove (&tdbb->tdbb_mutexes, dbb->dbb_mutexes + i, TRUE);
for (i = 0; i < DBB_WLCK_max; i++)
    INUSE_remove (&tdbb->tdbb_rw_locks, dbb->dbb_rw_locks + i, TRUE);

THD_MUTEX_DESTROY_N (dbb->dbb_mutexes, DBB_MUTX_max);
V4_RW_LOCK_DESTROY_N (dbb->dbb_rw_locks, DBB_WLCK_max);
#ifdef SUPERSERVER
if (dbb->dbb_flags & DBB_sp_rec_mutex_init)
    THD_rec_mutex_destroy (&dbb->dbb_sp_rec_mutex);
#endif
if (release_pools)
    {
    ALL_fini();
    tdbb->tdbb_database = NULL;
    }
}

static void strip_quotes (
    TEXT	*in,
    TEXT	*out)
{
/**************************************
 *
 *	s t r i p _ q u o t e s
 *
 **************************************
 *
 * Functional description
 *	Get rid of quotes around strings
 *
 **************************************/
TEXT	*p;
TEXT	quote;

if (!in || !*in)
    {
    *out = 0;
    return;
    }

quote = 0;
/* Skip any initial quote */
if ((*in == DBL_QUOTE) || (*in == SINGLE_QUOTE))
    quote = *in++;
p = in;

/* Now copy characters until we see the same quote or EOS */
while (*p && (*p != quote))
    {
    *out++ = *p++;
    }
*out = 0;
}

void JRD_set_cache_default (ULONG *num_ptr)
{
/**************************************
 *
 *	J R D _ s e t _ c a c h e _ d e f a u l t
 *
 **************************************
 *
 * Functional description
 *	Set the number of cache pages to use for each
 *	database, but don't go less than MIN_PAGE_BUFFERS and
 *      more than MAX_PAGE_BUFFERS.
 *	Currently MIN_PAGE_BUFFERS = 50L,
 *		  MAX_PAGE_BUFFERS = 65535L.
 *
 **************************************/

if (*num_ptr < MIN_PAGE_BUFFERS)
    *num_ptr = MIN_PAGE_BUFFERS;
if (*num_ptr > MAX_PAGE_BUFFERS)
    *num_ptr = MAX_PAGE_BUFFERS;
JRD_cache_default = *num_ptr;
}

#ifdef SERVER_SHUTDOWN
TEXT *JRD_num_attachments (TEXT *buf, USHORT len, USHORT flag, USHORT *atts, USHORT *dbs)
{
/**************************************
 *
 *	J R D _ n u m _ a t t a c h m e n t s
 *
 **************************************
 *
 * Functional description
 *	Count the number of active databases and
 *	attachments.  If flag is set then put
 *	what it says into buf, if it fits. If it does not fit
 *	then allocate local buffer, put info into there, and
 *	return pointer to caller (in this case a caller must
 *	release memory allocated for local buffer).
 *
 **************************************/
USHORT		num_dbs = 0;
USHORT		num_att = 0;
USHORT		total = 0;
ULONG		drive_mask = 0L;
DBB    		dbb;
DBF		dbf = NULL, dbfp = NULL;
ATT		att;
TEXT		*lbuf, *lbufp;
#ifdef WIN_NT
MDLS		*ptr;
#endif

/* protect against NULL value for buf */

if (!(lbuf = buf))
    len = 0;

#ifdef WIN_NT
/* Check that the buffer is big enough for the requested
 * information.  If not, unset the flag
 */

if (flag == JRD_info_drivemask)
    if (len < sizeof (ULONG))
	if (!(lbuf = (TEXT *) gds__alloc ((SLONG)(sizeof(ULONG)))) )
	    flag = 0;
#endif

THREAD_ENTER;

/* Zip through the list of databases and count the number of local
 * connections.  If buf is not NULL then copy all the database names
 * that will fit into it. */

for (dbb = databases; dbb; dbb = dbb->dbb_next)
    {
#ifdef WIN_NT
    struct fil *files;
    struct dls *dirs;
    struct scb *scb;
    struct sfb *sfb;

    /* Get drive letters for db files */

    if (flag == JRD_info_drivemask)
	for (files = dbb->dbb_file; files; files = files->fil_next)
	    ExtractDriveLetter(files->fil_string, &drive_mask);
#endif

    if (!(dbb->dbb_flags & (DBB_bugcheck | DBB_not_in_use)) && 
	!(dbb->dbb_ast_flags & DBB_shutdown && dbb->dbb_ast_flags & DBB_shutdown_locks))
	{
	num_dbs++;
	if (flag == JRD_info_dbnames)
	    {
	    if (dbfp == NULL)
		{
		dbfp = (DBF) gds__alloc ((SLONG)(sizeof(struct dbf) +
			sizeof(TEXT) * dbb->dbb_filename->str_length));
		dbf = dbfp;
		}
	    else
		{
		dbfp->dbf_next = (DBF) gds__alloc ((SLONG)(sizeof(struct dbf) +
				 sizeof(TEXT) * dbb->dbb_filename->str_length));
		dbfp = dbfp->dbf_next;
		}
	    if (dbfp)
		{
		dbfp->dbf_length = dbb->dbb_filename->str_length;
		dbfp->dbf_next = NULL;
		MOVE_FAST(dbb->dbb_filename->str_data, dbfp->dbf_data,
			  dbfp->dbf_length);
		total += sizeof(USHORT) + dbfp->dbf_length;
		}
	    else
		flag = 0;
	    }
	    
	for (att = dbb->dbb_attachments; att; att = att->att_next)
	    {
	    num_att++;

#ifdef WIN_NT
	    /* Get drive letters for temp directories */

	    if (flag == JRD_info_drivemask)
		{
		ptr = DLS_get_access();
		for (dirs = ptr->mdls_dls; dirs; dirs = dirs->dls_next)
		    ExtractDriveLetter(dirs->dls_directory, &drive_mask);
		}

	    /* Get drive letters for sort files */

	    if (flag == JRD_info_drivemask)
		for (scb = att->att_active_sorts; scb; scb = scb->scb_next)
		    for (sfb = scb->scb_sfb; sfb; sfb = sfb->sfb_next)
			ExtractDriveLetter(sfb->sfb_file_name, &drive_mask);

#endif
	    }
	}
    }

THREAD_EXIT;

*atts = num_att;
*dbs = num_dbs;

if (dbf)
    {
    if (flag == JRD_info_dbnames)
	{
	if (len < (sizeof(USHORT) + total))
	    lbuf = (TEXT*) gds__alloc((SLONG)(sizeof(USHORT) + total));
	if (lbufp = lbuf)
	    {
	    /*	Put db info into buffer. Format is as follows:

		number of dbases	sizeof (USHORT)
		1st db name length	sizeof (USHORT)
		1st db name		sizeof (TEXT) * length
		2nd db name length
		2nd db name
		...
		last db name length
		last db name
	    */
		
	    lbufp += sizeof(USHORT);
	    total = 0;
	    for (dbfp = dbf; dbfp; dbfp = dbfp->dbf_next)
		{
		*lbufp++ = dbfp->dbf_length;
		*lbufp++ = dbfp->dbf_length >> 8;
		MOVE_FAST(dbfp->dbf_data, lbufp, dbfp->dbf_length);
		lbufp += dbfp->dbf_length;
		total++;
		}
	    assert (total == num_dbs);
	    lbufp = lbuf;
	    *lbufp++ = total;
	    *lbufp++ = total >> 8;
	    }
	}

    for (dbfp = dbf; dbfp;)
	{
	DBF	x = dbfp->dbf_next;
	gds__free(dbfp);
	dbfp = x;
	}
    }

#ifdef WIN_NT
if (flag == JRD_info_drivemask)
    *(ULONG*)lbuf = drive_mask;
#endif

if (num_dbs == 0)
    lbuf = NULL;

return lbuf;
}

#ifdef WIN_NT
static void ExtractDriveLetter(TEXT *file_name, ULONG *drive_mask)
{
/**************************************
 *
 *	E x t r a c t D r i v e L e t t e r
 *
 **************************************
 *
 * Functional description
 *	Determine the drive letter of file_name
 *	and set the proper bit in the bit mask.
 *		bit 0 = drive A
 *		bit 1 = drive B and so on...
 *	This function is used to determine drive
 *	usage for use with Plug and Play for
 *	MS Windows 4.0.
 *
 **************************************/
ULONG mask = 1;
SHORT shift;

shift = (*file_name - 'A');
mask <<= shift;
*drive_mask |= mask;
}
#endif

ULONG JRD_shutdown_all ()
{
/**************************************
 *
 *	J R D _ s h u t d o w n _ a l l
 *
 **************************************
 *
 * Functional description
 *	rollback every transaction,
 *	release every attachment,
 *	and shutdown every database.
 *
 **************************************/
STATUS		user_status[ISC_STATUS_LENGTH];
ATT		att, att_next;
DBB		dbb, dbb_next;
JMP_BUF         env;
struct tdbb     thd_context, *tdbb = NULL;

SET_THREAD_DATA;

if ( initialized)
    JRD_SS_MUTEX_LOCK;

for (dbb = databases; dbb; dbb = dbb_next)
    {
    dbb_next = dbb->dbb_next;
    if (!(dbb->dbb_flags & (DBB_bugcheck | DBB_not_in_use)) && 
	!(dbb->dbb_ast_flags & DBB_shutdown && dbb->dbb_ast_flags & DBB_shutdown_locks))
	{
	for (att = dbb->dbb_attachments; att; att = att_next)
	    {
	    att_next = att->att_next;
	    tdbb->tdbb_database = dbb;
	    tdbb->tdbb_attachment = att;
	    tdbb->tdbb_request = NULL;
	    tdbb->tdbb_transaction = NULL;
	    tdbb->tdbb_default = NULL;

	    ++dbb->dbb_use_count;

	    /* purge_attachment below can do an ERR_post */

            tdbb->tdbb_setjmp = (UCHAR*) env;
            tdbb->tdbb_status_vector = user_status;
            if (SETJMP (env))
            {
                if ( initialized)
                    JRD_SS_MUTEX_UNLOCK;
                return error (user_status);
            }

	    /* purge_attachment, rollback any open transactions */

	    V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_init_fini);
	    purge_attachment (tdbb, user_status, att, TRUE);
	    V4_JRD_MUTEX_UNLOCK (databases_mutex);
	    }
	}
    }

if ( initialized)
    JRD_SS_MUTEX_UNLOCK;

RESTORE_THREAD_DATA;

return SUCCESS;
}
#endif	/* SERVER_SHUTDOWN */

static void purge_attachment(
    TDBB	tdbb,
    STATUS	*user_status,
    ATT		attachment,
    BOOLEAN	force_flag)
{
/**************************************
 *
 *	p u r g e _ a t t a c h m e n t
 *
 **************************************
 *
 * Functional description
 *	Zap an attachment, shutting down the database
 *	if it is the last one.
 *	NOTE:  This routine assumes that upon entry,
 *	mutex databases_mutex will be locked.
 *
 **************************************/
BKM	bookmark;
DBB	dbb;
REQ	request;
SCL	class;
TRA	transaction, next;
USR	user;
USHORT	count;

SET_TDBB (tdbb);
dbb = attachment->att_database;

if (!(dbb->dbb_flags & DBB_bugcheck))
    {
    /* Check for any pending transactions */

    count = 0;

    for (transaction = attachment->att_transactions; transaction; 
	 transaction = next)
	{
	next = transaction->tra_next;
	if (transaction != attachment->att_dbkey_trans)
	    {
	    if (transaction->tra_flags & TRA_prepared ||
	        dbb->dbb_ast_flags & DBB_shutdown ||
		attachment->att_flags & ATT_shutdown)
	        TRA_release_transaction (tdbb, transaction);
	    else
		if (force_flag)
		    TRA_rollback (tdbb, transaction, FALSE);
		else
		    ++count;
	    }
	}

    if (count)
	ERR_post (gds__open_trans, gds_arg_number, (SLONG) count, 0);

    /* If there's a side transaction for db-key scope, get rid of it */

    if (transaction = attachment->att_dbkey_trans)
	{
	attachment->att_dbkey_trans = NULL;
	if (dbb->dbb_ast_flags & DBB_shutdown ||
	    attachment->att_flags & ATT_shutdown)
	    TRA_release_transaction (tdbb, transaction);
    	else
	    TRA_commit (tdbb, transaction, FALSE);
	}

    SORT_shutdown (attachment);
    }

/* Unlink attachment from database */

release_attachment (attachment);

/* At this point, mutex dbb->dbb_mutexes [DBB_MUTX_init_fini] has been
   unlocked and mutex databases_mutex has been locked. */

/* If there are still attachments, do a partial shutdown */

if (dbb->dbb_header.blk_type == (UCHAR) type_dbb)
    if (dbb->dbb_attachments || (dbb->dbb_flags & DBB_being_opened))
	{
	/* There are still attachments so do a partial shutdown */

	while (request = attachment->att_requests)
	    CMP_release (tdbb, request);
	while (class = attachment->att_security_classes)
	    SCL_release (class);
	if (user = attachment->att_user)
	    ALL_RELEASE (user);
	while (bookmark = attachment->att_bookmarks)
	    {
	    attachment->att_bookmarks = bookmark->bkm_next;
	    ALL_RELEASE (bookmark);
	    }
	if (attachment->att_bkm_quick_ref)
	    ALL_RELEASE (attachment->att_bkm_quick_ref);
	if (attachment->att_lck_quick_ref)
	    ALL_RELEASE (attachment->att_lck_quick_ref);

	ALL_RELEASE (attachment);
	}
    else
	shutdown_database (dbb, TRUE);
}
