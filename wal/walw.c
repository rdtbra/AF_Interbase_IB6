/*
 *        PROGRAM:        JRD Write Ahead Log Writer
 *        MODULE:         walw.c
 *        DESCRIPTION:    Write Ahead Log Writer
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
#include <string.h>
#include "../jrd/time.h"
#include "../jrd/jrd.h"
#include "../jrd/isc.h"
#include "../wal/wal.h"
#include "../jrd/dsc.h"
#include "../jrd/jrn.h"
#include "../jrd/iberr.h"
#include "../jrd/llio.h"
#include "../jrd/codes.h"
#include "../jrd/license.h"
#include "../wal/wal_proto.h"
#include "../wal/walc_proto.h"
#include "../wal/walf_proto.h"
#include "../wal/walw_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/iberr_proto.h"
#include "../jrd/isc_i_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/jrn_proto.h"
#include "../jrd/llio_proto.h"
#include "../jrd/misc_proto.h"

#include <setjmp.h>
#if !(defined WIN_NT || defined OS2_ONLY)
#ifndef VMS
#include <sys/types.h>
#ifndef IMP
#ifndef NETWARE_386
#include <sys/file.h>
#endif
#endif
#else
#include <types.h>
#include <file.h>
#endif
#include <errno.h>
#include <signal.h>
#endif

#ifdef IMP
#include <unistd.h>
#endif
#ifdef DELTA
#include <unistd.h>
#endif
#ifdef sparc
#include <unistd.h>
#endif
#ifdef UNIX
#include <sys/stat.h>
#endif

#ifdef OS2_ONLY
#include <process.h>
#endif

#ifdef WIN_NT
#include <process.h>
#include <windows.h>
#define TEXT	SCHAR
#define ERRNO	GetLastError()
#endif

#ifdef SUPERSERVER
#define exit(code)	return (code)
#ifdef NETWARE_386
#define getpid          GetThreadID
#endif
#ifdef WIN_NT
#define getpid          GetCurrentThreadId
#endif
#endif

#ifndef ERRNO
#define ERRNO	errno
#endif

typedef struct walwl {
    jmp_buf	walwl_env;
    SLONG	walwl_log_fd;
    struct jrn	*walwl_journal_handle;
    SLONG	walwl_local_time [2];
    SLONG	walwl_flags;
    IB_FILE	*walwl_debug_fd;
} *WALWL;

#define WALW_WRITER_TIMEOUT_USECS	3000000
#define WALW_DISABLING_JRN		1
#define MAXLOGS				32
#define WALW_ERROR(status_vector)	IBERR_build_status (status_vector, gds__walw_err, 0)

#define ENV			((WALWL)WAL_handle->wal_local_info_ptr)->walwl_env
#define LOG_FD			((WALWL)WAL_handle->wal_local_info_ptr)->walwl_log_fd
#define JOURNAL_HANDLE		((WALWL)WAL_handle->wal_local_info_ptr)->walwl_journal_handle
#define LOCAL_TIME		((WALWL)WAL_handle->wal_local_info_ptr)->walwl_local_time
#define LOCAL_FLAGS		((WALWL)WAL_handle->wal_local_info_ptr)->walwl_flags   
#define DEBUG_FD		((WALWL)WAL_handle->wal_local_info_ptr)->walwl_debug_fd
#define DBNAME			WAL_handle->wal_dbname
#define PRINT_DEBUG_MSGS	(WAL_segment->wals_flags2 & WALS2_DEBUG_MSGS)

#define PRINT_TIME(fd,t)	{ time((time_t*) t); ib_fprintf (fd, "%s", ctime((time_t*) t)); }

static void	close_log (STATUS *, WAL, SCHAR *, WALFH, SLONG);
static SSHORT	discard_prev_logs (STATUS *, SCHAR *, SCHAR *, SLONG, SSHORT);
static void	finishup_checkpoint (WALS);
static SSHORT	flush_all_buffers (STATUS *, WAL);
static SSHORT	get_logfile_index (WALS, SCHAR *);
static BOOLEAN	get_log_usability (STATUS *, SCHAR *, SCHAR *, SLONG);
static SSHORT	get_next_logname (STATUS *, WALS, SCHAR *, SLONG *, SLONG *);
static SSHORT	get_next_prealloc_logname (STATUS *, WALS, SCHAR *, SLONG *, SLONG *);
static SSHORT	get_next_serial_logname (STATUS *, WALS, SCHAR *, SLONG *, SLONG *);
static BOOLEAN	get_next_usable_partition (STATUS *, SCHAR *, SCHAR *, SLONG *);
static SSHORT	get_overflow_logname (STATUS *, WALS, SCHAR *, SLONG *, SLONG *);
static void	get_time_stamp (SLONG *);
static SSHORT	increase_buffers (STATUS *, WAL, SSHORT);
static SSHORT	init_raw_partitions (STATUS *, WAL);
static SSHORT	journal_connect (STATUS *, WAL);
static void	journal_disable (STATUS *, WAL, WALFH);
static SSHORT	journal_enable (STATUS *, WAL);
static void	prepare_wal_block (WALS, WALBLK *);
static void	release_wal_block (WALS, WALBLK *);
static void	report_walw_bug_or_error (STATUS *, struct wal *, SSHORT, STATUS);
static SSHORT	rollover_log (STATUS *, WAL, WALFH);
static void	setup_for_checkpoint (WALS);
static SSHORT	setup_log (STATUS *, WAL, SCHAR *, SLONG, SLONG, SLONG *, WALFH, SSHORT, SCHAR *, SLONG);
static SSHORT	setup_log_header_info (STATUS *, WAL, SCHAR *, SLONG, SLONG, SLONG *, WALFH, SSHORT, SCHAR *, SLONG, SSHORT *);
static SSHORT	write_log_header_and_reposition (STATUS *, SCHAR *, SLONG, WALFH);
static SSHORT	write_wal_block (STATUS *, WALBLK *, SCHAR *, SLONG);
static void	write_wal_statistics (WAL);

static WAL_TERMINATOR (log_terminator_block);

#ifdef SUPERSERVER
int main_walw (
    char	**argv)

#else

int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
#endif
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	WAL_writer is used only in the privileged WAL writer 
 *	process.  One WAL writer process is started per database.
 *
 **************************************/
STATUS		status_vector [20];
WAL		WAL_handle;
SCHAR		dbg_file [MAXPATHLEN];
IB_FILE		*debug_fd;
struct walwl	local_info;
SCHAR		*dbname, c, *p, **end;
#ifdef SUPERSERVER
int		argc;

argc = (int) argv [0];
#endif

dbname = "";
for (end = argv++ + argc; argv < end;)
    {
    p = *argv++;
    if (*p != '-')
	dbname = p;
    else
	{
        while (c = *++p)
            switch (UPPER (c))
                {
		case 'D':
		    dbname = *argv++;
		    break;

		case 'Z':
		    ib_printf ("WAL writer version %s\n", GDS_VERSION);
		    exit (FINI_OK);
		}
	}
    }

#ifdef UNIX
/* WAL writer has to run as 'root' to be able to access a
   raw device logs.  */

if (setreuid (0, 0) == -1)
   {
   ib_printf ("WAL writer: couldn't set uid to 0 for database %s, errno=%d\n",
	dbname, errno);
   exit (FINI_ERROR);
   }

divorce_terminal (0);
#endif

/* Create a debug file if not already present */

WALC_build_dbg_filename (dbname, dbg_file);

debug_fd = ib_fopen (dbg_file, "a"); /* Do it after divorce_terminal() */

WAL_handle = NULL;
if (WALC_init (status_vector, &WAL_handle, dbname, 0, 
               NULL, 0L, FALSE, 1L, 
               0, NULL, FALSE) != SUCCESS)
    {
    gds__log_status (dbname, status_vector);
    gds__print_status (status_vector);
    exit (FINI_ERROR);
    }

ISC_signal_init();
WAL_handle->wal_local_info_ptr = (UCHAR *)&local_info;
JOURNAL_HANDLE = NULL;
LOCAL_FLAGS = 0L;

DEBUG_FD = debug_fd;

WALW_writer (status_vector, WAL_handle);

WALC_fini (status_vector, &WAL_handle);

ib_fclose (debug_fd);

exit (FINI_OK);
}

#ifdef VMS
void ERR_post (stuff)
    STATUS	stuff;
{
/**************************************
 *
 *	E R R _ p o s t
 *
 **************************************
 *
 * Functional description
 *	Post an error sequence to the status vector.  Since an error
 *	sequence can, in theory, be arbitrarily lock, pull a cheap
 *	trick to get the address of the argument vector.
 *
 **************************************/
STATUS	*p, *q, status_vector [20];
int	type;

/* Get the addresses of the argument vector and the status vector, and do
   word-wise copy. */

p = status_vector;
q = &stuff;

/* Copy first argument */

*p++ = gds_arg_gds;
*p++ = *((SLONG*) q)++;

/* Pick up remaining args */

while (*p++ = type = *((int*) q)++)
    switch (type)
	{
	case gds_arg_gds:
	    *p++ = *((STATUS*) q)++;
	    break;

	case gds_arg_number:
	    *p++ = *((SLONG*) q)++;
	    break;

	case gds_arg_vms:
	case gds_arg_unix:
	case gds_arg_domain:
	case gds_arg_mpexl:
	default:
	    *p++ = *((int*) q)++;
	    break;

	case gds_arg_string:
	case gds_arg_interpreted:
	    *p++ = *q++;
	    break;

	case gds_arg_cstring:
	    *p++ = *((int*) q)++;
	    *p++ = *q++;
	    break;
	}

gds__print_status (status_vector);

exit (FINI_ERROR);
}
#endif

SSHORT WALW_writer (
    STATUS	*status_vector,
    WAL		WAL_handle)
{
/**************************************
 *
 *	W A L W _ w r i t e r 
 *
 **************************************
 *
 * Functional description
 *	This is the main processing routine for the WAL writer.
 *	This routine simply waits on its semaphore in a loop.  If
 *	the semaphore is poked, or an alarm expires, it wakes up,
 *	looks for buffers to be written to the log file, writes 
 *	them, and wakes up everyone waiting for a free buffer 
 *	before going back to sleep.
 *
 *	In addition to writing WAL buffers to log files, it also 
 *	takes care of such functions as checkpointing, rollover,
 *	enabling and disabling journalling and WAL shutdown.
 *
 **************************************/
SSHORT	ret;
WALS	WAL_segment;
WALBLK	*wblk;
SSHORT	bufnum, first_logfile;
WALFH	log_header;
int	buffer_full, journal_enable_or_disable, rollover_required;
BOOLEAN	acquired;
EVENT	ptr;
SLONG	value;
SLONG	log_type;
#define WALW_WRITER_RETURN(code) {gds__free((SLONG*)log_header); return(code);}

acquired = FALSE;
log_header = (WALFH) gds__alloc (WALFH_LENGTH);
/* NOMEM: return failure, FREE: error returns & macro WALW_WRITER_RETURN */
if (!log_header)
    return FAILURE;
if (setjmp (ENV))
    {
    if (acquired)
	WALC_release (WAL_handle);
    WALW_WRITER_RETURN (FAILURE);
    }

/* If there already is a WAL writer running, return quietly. */

if (WALC_check_writer (WAL_handle) == SUCCESS) 
    WALW_WRITER_RETURN (SUCCESS);
 
/* Now we are the WAL writer. */

ret = SUCCESS;
WAL_TERMINATOR_SET (log_terminator_block);

WALC_acquire (WAL_handle, &WAL_segment);
acquired = TRUE;
if (PRINT_DEBUG_MSGS)
    {
    ib_fprintf (DEBUG_FD,"====================================================\n");
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "WAL writer for database %s starting, pid=%d.\n", 
             DBNAME, getpid());
    }

WAL_segment->wals_writer_pid = getpid();
WAL_segment->wals_flags &= ~WALS_ERROR_HAPPENED;
WAL_segment->wals_last_err = 0;
WAL_CHECK_BUG_ERROR (WAL_handle, WAL_segment);

first_logfile = (WAL_segment->wals_flags & WALS_FIRST_TIME_LOG) ? TRUE : FALSE;
log_type = 0L;
if (first_logfile)
    { 
    /* Initialize raw partitions which need root permission */

    init_raw_partitions (status_vector, WAL_handle);

    /* Get the new log name and partition */

    if (get_next_logname (status_vector, WAL_segment, 
                          WAL_segment->wals_logname,
                          &WAL_segment->wals_log_partition_offset,
                          &log_type) != SUCCESS)
	report_walw_bug_or_error (status_vector, WAL_handle, FAILURE, 
                                  (STATUS) gds__wal_err_rollover2);
    }

if (strlen (WAL_segment->wals_jrn_dirname))
     journal_connect (status_vector, WAL_handle);

if (WAL_segment->wals_ckpted_log_seqno == 0L)
    {
    /* Initialize checkpoint info.  Current logfile may be assumed
       to have the latest checkpoint */

    WAL_segment->wals_ckpted_log_seqno = WAL_segment->wals_log_seqno;
    strcpy (WAL_segment->wals_ckpt_logname, WAL_segment->wals_logname);
    WAL_segment->wals_ckpt_log_p_offset = WAL_segment->wals_log_partition_offset;
    WAL_segment->wals_ckpted_offset = 0;

    WAL_segment->wals_prev_ckpted_log_seqno = WAL_segment->wals_log_seqno;
    strcpy (WAL_segment->wals_prev_ckpt_logname, WAL_segment->wals_logname);
    WAL_segment->wals_prev_ckpt_log_p_offset = WAL_segment->wals_log_partition_offset;
    WAL_segment->wals_prev_ckpted_offset = 0;
    }

ret = setup_log (status_vector, WAL_handle, WAL_segment->wals_logname, 
                 WAL_segment->wals_log_partition_offset, log_type, 
                 &(LOG_FD), log_header, first_logfile, "", 0L);
if (ret == SUCCESS)
    {
    WAL_segment->wals_flags |= WALS_WRITER_INITIALIZED;
    WAL_segment->wals_flags &= ~WALS_FIRST_TIME_LOG;
    }
WALC_release (WAL_handle);
acquired = FALSE;

if (ret != SUCCESS)
    {
    report_walw_bug_or_error (status_vector, WAL_handle, ret, 
                              (STATUS) gds__wal_err_setup);
    if (JOURNAL_HANDLE)
	JRN_fini (status_vector, &(JOURNAL_HANDLE));
    WALW_WRITER_RETURN (ret);
    }

for (;;)
    {
    WALC_acquire (WAL_handle, &WAL_segment);
    acquired = TRUE;
    WAL_CHECK_BUG_ERROR (WAL_handle, WAL_segment);

    /* First find out the work to be done. */
    
    if (WAL_segment->wals_flags & WALS_SHUTDOWN_WRITER)
	break;   /* This is the only way to get out of this loop. */

    /* Check the next buffer to be written. */

    bufnum = (WAL_segment->wals_last_flushed_buf + 1) % WAL_segment->wals_maxbufs;
    wblk = WAL_BLOCK (bufnum);
    buffer_full = wblk->walblk_flags & WALBLK_to_be_written;

    /* Check if journalling is enabled or disabled */

    journal_enable_or_disable =
	(WAL_segment->wals_flags & WALS_ENABLE_JOURNAL) ||
	(WAL_segment->wals_flags & WALS_DISABLE_JOURNAL);
 
    /* Check if rollover to a new log file is required */

    rollover_required = WAL_segment->wals_flags & WALS_ROLLOVER_REQUIRED;

    if (!buffer_full && !journal_enable_or_disable && !rollover_required)
	{
	/* Nothing to do.  Prepare to wait for a timeout or a wakeup 
	   from somebody else. */

	ptr = &WAL_EVENTS [WAL_WRITER_WORK_SEM];
	value = ISC_event_clear (ptr);
	WAL_segment->wals_flags &= ~WALS_WRITER_INFORMED;
	WALC_release (WAL_handle);
	acquired = FALSE;
	ISC_event_wait (1, &ptr, &value, WALW_WRITER_TIMEOUT_USECS,
	    WALC_alarm_handler, ptr);
	continue;
	}

    /* Now do the work */
    
    if (buffer_full)
	{
	/* Prepare and flush the associated buffer to log file. */

	prepare_wal_block (WAL_segment, wblk);

	/* Do the write after releasing the shared segment so that 
	   other concurrent activities may go on in the meantime. */

	WALC_release (WAL_handle); 
	acquired = FALSE;

	if ((ret = write_wal_block (status_vector, wblk, 
	               WAL_segment->wals_logname, LOG_FD)) != SUCCESS)
	    report_walw_bug_or_error (status_vector, WAL_handle, ret, 
	                              (STATUS) gds__wal_err_logwrite);

	WALC_acquire (WAL_handle, &WAL_segment);
	acquired = TRUE;

	wblk = WAL_BLOCK (bufnum);
	release_wal_block (WAL_segment, wblk);

	/* If the WAL segment needs to be expanded and can be expanded
	   try to do it now because no WAL I/O is in progress and we 
	   can move things around. */

	if (!(WAL_segment->wals_flags2 & WALS2_CANT_EXPAND) &&
	     (WAL_segment->wals_flags2 & WALS2_EXPAND))
	    {
	    increase_buffers (status_vector, WAL_handle, 1);
	    WAL_segment = WAL_handle->wal_segment;
	    WAL_segment->wals_buf_waiting_count = 0;
	    WAL_segment->wals_flags2 &= ~(WALS2_EXPAND);
	    }

	/* If it is time for checkpointing, then initiate one. */

	if ((WAL_segment->wals_cur_ckpt_intrvl > WAL_segment->wals_thrshold_intrvl) &&
	     !(WAL_segment->wals_flags & WALS_CKPT_START))
	    {
	    setup_for_checkpoint (WAL_segment);
	    }

	WAL_segment->wals_buf_waiters = 0;
	ISC_event_post (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	ISC_event_clear (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	}
 
    /* if a checkpoint has been recorded, discard old log files. */

    if ((WAL_segment->wals_flags & WALS_CKPT_START)  && 
	(WAL_segment->wals_flags & WALS_CKPT_RECORDED))
	{
	discard_prev_logs (status_vector, WAL_segment->wals_dbname, 
	                   WAL_segment->wals_prev_ckpt_logname, 
			   WAL_segment->wals_prev_ckpt_log_p_offset, FALSE);
#ifdef SUPERSERVER
	/* In Netware, file handles are shared if the file is reopened in the
	   same thread.  discard_prev_log() may open the current log file to
	   check its header for previously linked log files.  That would change
	   the file pointer to WALFH_LENGTH (2048) causing us to loose log records
	   in the current log file unless we do reseeking. This action won't 
	   hurt on other platforms.*/
	lseek (LOG_FD, WAL_segment->wals_flushed_offset, SEEK_SET);
#endif
	WAL_segment->wals_flags &= ~(WALS_CKPT_START + WALS_CKPT_RECORDED);
	}

    /* If it is time to rollover to the next log file then do it.  But
       make sure that the last rollover information, if any, has 
       already been recorded. */ 

    if (rollover_required)
	{
	rollover_log (status_vector, WAL_handle, log_header); 
	WAL_segment->wals_buf_waiters = 0;
	WAL_segment->wals_flags &= ~WALS_ROLLOVER_REQUIRED;
	ISC_event_post (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	ISC_event_clear (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	}

    /* If journalling has been enabled or disabled, take care of that 
       situation now. */

    if (journal_enable_or_disable)
	{
	if ((WAL_segment->wals_flags & WALS_ENABLE_JOURNAL) &&
	    !JOURNAL_HANDLE)
	    journal_enable (status_vector, WAL_handle);
	else if ((WAL_segment->wals_flags & WALS_DISABLE_JOURNAL) &&
            JOURNAL_HANDLE)
	    journal_disable (status_vector, WAL_handle, log_header);

	ISC_event_post (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	ISC_event_clear (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);
	}

    WALC_release (WAL_handle);
    acquired = FALSE;
    }

/* Do cleanup and shutdown. */

log_header->walfh_length = WAL_segment->wals_flushed_offset;
log_header->walfh_hibsn = WAL_segment->wals_blkseqno - 1;
close_log (status_vector, WAL_handle, WAL_segment->wals_logname, log_header,
    WAL_segment->wals_flags & WALS_INFORM_CLOSE_TO_JOURNAL);
write_wal_statistics (WAL_handle);
WAL_segment->wals_flags |= WALS_WRITER_DONE;
WAL_segment->wals_writer_pid = 0;
WALC_release (WAL_handle);
acquired = FALSE;

if (JOURNAL_HANDLE)
    JRN_fini (status_vector, &(JOURNAL_HANDLE));

ISC_event_clear (WAL_EVENTS + WAL_WRITER_WORK_DONE_SEM);

WALW_WRITER_RETURN (SUCCESS);
}

static void close_log (
    STATUS	*status_vector,
    WAL		WAL_handle,
    SCHAR	*logname,
    WALFH	log_header,
    SLONG	journal_flag)
{
/**************************************
 *
 *	c l o s e _ l o g
 *
 **************************************
 *
 * Functional description
 *	Closes the log file after updating its header with info from
 *	the passed log_header pointers.
 *
 **************************************/
SSHORT	ret;
WALS	WAL_segment;

WAL_segment = WAL_handle->wal_segment;
log_header->walfh_flags &= ~WALFH_OPEN;   
log_header->walfh_data_len = MISC_build_parameters_block (log_header->walfh_data, 
    PARAM_BYTE (WALFH_dbname), PARAM_STRING (log_header->walfh_dbname),
    PARAM_BYTE (WALFH_prev_logname), PARAM_STRING (log_header->walfh_prev_logname),
    PARAM_BYTE (WALFH_next_logname), PARAM_STRING (log_header->walfh_next_logname),
    PARAM_BYTE (WALFH_end),
    0);
if (log_header->walfh_dbname)
    gds__free ((SLONG*) log_header->walfh_dbname);
if (log_header->walfh_prev_logname)
    gds__free ((SLONG*) log_header->walfh_prev_logname);
if (log_header->walfh_next_logname)
    gds__free ((SLONG*) log_header->walfh_next_logname);

if ((ret = write_log_header_and_reposition (status_vector, logname, LOG_FD, 
                                    log_header)) != SUCCESS)
    report_walw_bug_or_error (status_vector, WAL_handle, ret, 
                              (STATUS) gds__wal_err_logwrite);
LLIO_close (NULL_PTR, LOG_FD);

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "Closed seqno=%d, log %s, p_offset=%d, length=%d\n", 
                   log_header->walfh_seqno, logname,  
                   log_header->walfh_log_partition_offset, 
                   log_header->walfh_length);
    }

if (journal_flag && JOURNAL_HANDLE)
    {
    ret = JRN_put_wal_name (status_vector, JOURNAL_HANDLE, 
                            logname, strlen (logname),
                            log_header->walfh_seqno, log_header->walfh_length,
                            log_header->walfh_log_partition_offset,
                            JRNW_CLOSE);

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "After calling JRN_put_wal_name() for seqno=%d, ret=%d\n",
            log_header->walfh_seqno, ret);
    }

    if (ret != SUCCESS)
        report_walw_bug_or_error (status_vector, WAL_handle, ret, 
                                  (STATUS) gds__wal_err_jrn_comm);
    }
}

static SSHORT discard_prev_logs ( 
    STATUS	*status_vector,
    SCHAR	*dbname,
    SCHAR	*starting_logname,
    SLONG	starting_log_partition_offset,
    SSHORT	delete_flag)
{
/**************************************
 *
 *	d i s c a r d _ p r e v _ l o g s
 *
 **************************************
 *
 * Functional description
 *	From the starting_logname backwards, excluding the starting
 *	one, mark all the log files as NOT needed for short-term
 *	recovery.  Delete those files if appropriate AND/OR if 
 *	delete_flag is TRUE.
 *
 *	If there is any error, return FAILURE else return SUCCESS.
 *	In case of error, status_vector would be updated.
 *
 **************************************/
SCHAR	s_logname[MAXPATHLEN];
SLONG	s_log_partition_offset;
SCHAR	lognames_buffer[MAXLOGS*MAXPATHLEN];  /* To hold all the lognames */
SCHAR	*logs_names[MAXLOGS];
SLONG	log_partitions_offsets[MAXLOGS];
SLONG	logs_seqnos[MAXLOGS];
SLONG	logs_lengths[MAXLOGS];
SLONG	logs_flags[MAXLOGS];
int	log_count, count;
SCHAR	*log_name;
SLONG	log_partition_offset;
SLONG	log_flag;
SSHORT	ret;
SLONG	old_seqno, new_seqno;

strcpy (s_logname, starting_logname);
s_log_partition_offset = starting_log_partition_offset;

old_seqno = 0;
while (TRUE)
    {
    /* Exhaust checking all the previously linked log files */

    log_count = 0;
    ret = WALF_get_all_next_logs_info (status_vector, dbname, 
                        s_logname, s_log_partition_offset,
                        MAXLOGS,
                        lognames_buffer, &log_count, logs_names,
                        log_partitions_offsets, 
                        logs_seqnos, logs_lengths, logs_flags, -1);
    if (ret != SUCCESS)
        return ret;

    if (log_count < MAXLOGS)
	count = 0; /* to start deleting log files */
    else
        {
	count = 1; /* to start deleting log files */

	/* setup for the next iteration */

        strcpy (s_logname, logs_names [0]);
        s_log_partition_offset = log_partitions_offsets[0];
        }

    /* Now see if any old log files may be deleted. */
    for (;count < log_count; count++)
        {
        new_seqno = logs_seqnos[count];
        if (new_seqno <= old_seqno)
            return SUCCESS;  /* We are done with the log list */
        old_seqno = new_seqno;
        log_name = logs_names [count];
        log_partition_offset = log_partitions_offsets [count];
        log_flag = logs_flags [count];

        /* We can delete a log file if it is NOT needed for SLONG-term
           recovery AND it is NOT a pre-allocated file AND (EITHER it
           is an overflow file OR the delete flag is true). */

        if (!(log_flag & WALFH_KEEP_FOR_LONG_TERM_RECV) &&
            !(log_flag & WALFH_PREALLOCATED) &&
            ((log_flag & WALFH_OVERFLOW) || delete_flag))
            { 
            /* We may delete this file. */
            WALF_delink_log (status_vector, dbname, log_name, 
                             log_partition_offset);
            if (unlink (log_name) != SUCCESS)
                {
                WAL_IO_ERROR (status_vector, "logfile unlink", log_name,
                        isc_io_delete_err, ERRNO);
		WALC_save_status_strings (status_vector);
                return FAILURE;
                }
            }
        else	
            {
            /* Just update the header flag stating that we don't need this 
               file for short-term recovery. */
             WALF_set_log_header_flag (status_vector, dbname, log_name, 
                                       log_partition_offset,
                                       WALFH_KEEP_FOR_SHORT_TERM_RECV, 0);
            }
        }

    if (log_count < MAXLOGS)
        break; /* No more previous log files to check */
    }
            
return SUCCESS;
}

static void finishup_checkpoint (
    WALS	WAL_segment)
{
/**************************************
 *
 *	f i n i s h u p _ c h e c k p o i n t
 *
 **************************************
 *
 * Functional description
 *	Store latest checkpoint information.
 *
 **************************************/

WAL_segment->wals_prev_ckpted_log_seqno = WAL_segment->wals_ckpted_log_seqno;
strcpy (WAL_segment->wals_prev_ckpt_logname, WAL_segment->wals_ckpt_logname); 
WAL_segment->wals_prev_ckpt_log_p_offset = WAL_segment->wals_ckpt_log_p_offset;
WAL_segment->wals_prev_ckpted_offset = WAL_segment->wals_ckpted_offset;

WAL_segment->wals_ckpted_log_seqno = WAL_segment->wals_log_seqno;
strcpy (WAL_segment->wals_ckpt_logname, WAL_segment->wals_logname);
WAL_segment->wals_ckpt_log_p_offset = WAL_segment->wals_log_partition_offset;

/* The checkpoint record is the last "empty" record in the current block.
   Its offset has already been saved for us, just use it. */

WAL_segment->wals_ckpted_offset = WAL_segment->wals_saved_ckpted_offset;
WAL_segment->wals_saved_ckpted_offset = 0L;

WAL_segment->wals_cur_ckpt_intrvl = 0L;
}

static SSHORT flush_all_buffers (
    STATUS	*status_vector,
    WAL		WAL_handle)
{
/**************************************
 *
 *	f l u s h _ a l l _ b u f f e r s 
 *
 **************************************
 *
 * Functional description
 *	Flush all the non-empty buffer blocks to the current
 *	log file.  We need not worry about rollover because we
 *	have already given its log file sequence number for
 *	the records in those buffers. 
 *
 **************************************/
WALS	WAL_segment;
SSHORT	bufnum, lastbuf, maxbufs;
WALBLK	*wblk;
SSHORT	ret;

WAL_segment = WAL_handle->wal_segment;

maxbufs = WAL_segment->wals_maxbufs;
bufnum  = (WAL_segment->wals_last_flushed_buf + 1) % maxbufs;
lastbuf = CUR_BUF;
if (lastbuf == -1)  /* Meaning all the buffers are full */
    lastbuf = (bufnum + maxbufs - 1)  % maxbufs;

do  {
    wblk = WAL_BLOCK (bufnum);
    if (wblk->walblk_cur_offset > BLK_HDROVHD)
	{
	/* Prepare and flush the associated buffer to log file. */

	if (!wblk->walblk_flags & WALBLK_to_be_written)
  	    WALC_setup_buffer_block (WAL_segment, wblk, 0);

        prepare_wal_block (WAL_segment, wblk);
        if ((ret = write_wal_block (status_vector, wblk,
                       WAL_segment->wals_logname, LOG_FD)) != SUCCESS)
            {
            report_walw_bug_or_error (status_vector, WAL_handle, ret,
                                      gds__wal_err_logwrite);
            return FAILURE;
            }
        release_wal_block (WAL_segment, wblk);
        }
    bufnum = (bufnum+1) % maxbufs;
    } while (bufnum != lastbuf);

/* Since all the log buffers are flushed and empty now, we can start 
   fresh. */

WAL_segment->wals_last_flushed_buf = -1;
CUR_BUF = 0;    

return SUCCESS;
}

static SSHORT get_logfile_index(
    WALS	WAL_segment, 
    SCHAR	*logname)
{
/**************************************
 *
 *	g e t _ l o g f i l e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Get the index of the given logname in the list of pre-allocated
 *	log files.  If not found, return -1.
 *
 **************************************/
LOGF	*logf;
SSHORT	i, j, count;

if (WAL_segment->wals_max_logfiles == 0)
    return -1;

/* Start searching from the next log file onwards */

i = j = (WAL_segment->wals_cur_logfile + 1) % WAL_segment->wals_max_logfiles;
count = 0;

while (TRUE)
    {
    if ((j == i) && (count > 1))
        return -1; /* We have gone through the whole list */
    logf = LOGF_INFO (j);
    if (!strcmp (logname, LOGF_NAME (logf)))
        return j;
    j = (j+1) % WAL_segment->wals_max_logfiles;
    count++;
    } 
}

static BOOLEAN get_log_usability (
    STATUS	*status_vector,
    SCHAR	*dbname,
    SCHAR	*logname,
    SLONG	log_partition_offset)
{
/**************************************
 *
 *	g e t _ l o g _ u s a b i l i t y
 *
 **************************************
 *
 * Functional description
 *	Returns TRUE if the given logname is usable else returns FALSE.
 *	If logname is usable, new_logname and new_offset are updated.
 *
 **************************************/
BOOLEAN	found;
SLONG	log_seqno;
SLONG	log_length;
SLONG	log_flag;

found = FALSE;
log_flag = 0;
if (WALF_get_log_info (status_vector, dbname, logname, log_partition_offset, 
                       &log_seqno, &log_length, &log_flag) == SUCCESS)
    {
    /* Check the log flag */

    if (!(log_flag & WALFH_KEEP_FOR_RECV))
	found = TRUE;
    }
else /* The log file has not apparently been used for logging purpose
        before or it is brand new.  So we can use it. */ 
    found = TRUE;

if (found)
    {
    /* We have found a pre-allocated file which can be reused. */

    WALF_delink_log (status_vector, dbname, logname, log_partition_offset);
    }

return found;
}
 
static SSHORT get_next_logname (  
    STATUS	*status_vector,
    WALS	WAL_segment, 
    SCHAR	*new_logname,
    SLONG	*new_offset,
    SLONG	*log_type)
{
/**************************************
 *
 *	g e t _ n e x t _ l o g n a m e 
 *
 **************************************
 *
 * Functional description
 *	Returns SUCCESS if a usable log filename is found else
 *	returns FAILURE.
 *
 **************************************/

if (WAL_segment->wals_max_logfiles > 0)
    /* Check the pre-allocated files or overflow files */
    return get_next_prealloc_logname (status_vector, WAL_segment, 
                                      new_logname, new_offset,
                                      log_type);
else
    /* Get a new serial log file */
    return get_next_serial_logname (status_vector, WAL_segment, 
                                    new_logname, new_offset,
                                    log_type);
}

static SSHORT get_next_prealloc_logname (  
    STATUS	*status_vector,
    WALS	WAL_segment,
    SCHAR	*new_logname,
    SLONG	*new_offset,
    SLONG	*log_type)
{
/**************************************
 *
 *	g e t _ n e x t _ p r e a l l o c _ l o g n a m e 
 *
 **************************************
 *
 * Functional description
 *	Get the name of the next usable log file from the list of 
 *	the preallocated log files.  If preallocated files are not
 *	available, try an overflow log file.
 *
 *	Returns SUCCESS if a usable log filename is found else
 *	returns FAILURE.
 *
 **************************************/
LOGF	*logf;
SSHORT	i, j, count, found;
SLONG	p_offset;

i = j = (WAL_segment->wals_cur_logfile + 1) % WAL_segment->wals_max_logfiles;
count = 0;
found = FALSE;
p_offset = 0L;

while (TRUE)
    {
    if ((j == i) && (count > 1)) /* We have exhausted all the preallocated files */
        break; 
    logf = LOGF_INFO(j);
    if (logf->logf_flags & LOGF_PARTITIONED) 
        found = get_next_usable_partition (status_vector, 
                                  WAL_segment->wals_dbname,
                                  LOGF_NAME (logf), &p_offset);
    else
        found = get_log_usability (status_vector, 
                                  WAL_segment->wals_dbname, LOGF_NAME (logf),
                                  p_offset);
    if (found)
        {
        strcpy (new_logname, LOGF_NAME(logf));
        *new_offset = p_offset;
        WAL_segment->wals_cur_logfile = j;
        *log_type = *log_type | WALFH_PREALLOCATED;
        if (logf->logf_flags & LOGF_PARTITIONED)
            *log_type = *log_type | WALFH_PARTITIONED;
        if (logf->logf_flags & LOGF_RAW)
            *log_type = *log_type | WALFH_RAW;
        return SUCCESS;
        }

    /* Check for the next one */

    j = (j + 1) % WAL_segment->wals_max_logfiles;
    count++;
    }

/* No pre-allocated file is avaialable, so look for an overflow file */

return get_overflow_logname (status_vector, WAL_segment, new_logname, 
                             new_offset, log_type);
}

static SSHORT get_next_serial_logname (
    STATUS  *status_vector,
    WALS    WAL_segment,
    SCHAR    *new_logname,
    SLONG    *new_offset,
    SLONG    *log_type)
{
/**************************************
 *
 *	g e t _ n e x t _ s e r i a l _ l o g n a m e 
 *
 **************************************
 *
 * Functional description
 *	Returns SUCCESS if a usable log filename is found else
 *	returns FAILURE.
 *
 **************************************/
int	prev_logs_count; 
SCHAR	last_logname [MAXPATHLEN];
SLONG	last_log_partition_offset;
SLONG	last_log_flags;
SSHORT	any_log_to_be_archived;
LOGF	*logf;
SLONG	fd;
int	retry_count;
#define MAX_RETRIES	1000

logf = &WAL_segment->wals_log_serial_file_info;
if (logf->logf_name_offset == 0)
    return FAILURE;

/* The next log name is formed in the following fashion :
   <basename>.log.<seqno>  */

if (logf->logf_fname_seqno == 0L) /* Initialize for the first time */
    logf->logf_fname_seqno = WAL_segment->wals_log_seqno + 1;

for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++)
    {
    /* Now find a non-existent (i.e. new) serial log file. */

    WALC_build_logname (new_logname, LOGF_NAME (logf), logf->logf_fname_seqno);
    logf->logf_fname_seqno++;
    if (LLIO_open (status_vector, new_logname, LLIO_OPEN_NEW_RW, TRUE, &fd) == SUCCESS)
	{
	/* Found one */

	LLIO_close (status_vector, fd);
	*new_offset = 0;
	break;
	}
    }

if (retry_count >= MAX_RETRIES)
   return FAILURE; 

/* Now see if we can reuse an old log file. */

prev_logs_count = 0;
WALF_get_linked_logs_info (status_vector, WAL_segment->wals_dbname, 
    WAL_segment->wals_logname, WAL_segment->wals_log_partition_offset,
    &prev_logs_count, 
    last_logname, &last_log_partition_offset, 
    &last_log_flags, &any_log_to_be_archived);

if ((prev_logs_count > 0) && !(last_log_flags & WALFH_KEEP_FOR_RECV)) 
    {
    /* We can reuse this log file */

    WALF_delink_log (status_vector, WAL_segment->wals_dbname, last_logname,
                    last_log_partition_offset);
#ifdef WIN_NT
    unlink (new_logname);
    MoveFile (last_logname, new_logname);
#else
#ifdef NETWARE_386
    unlink (new_logname);
    rename (last_logname, new_logname);
#else
    rename (last_logname, new_logname);
#endif
#endif
    }

#ifdef UNIX
/* WAL writer would typically be running as root.  So give the 
   ownership of the new log file to the database owner. */
if (chown (new_logname, WAL_segment->wals_owner_id, 
           WAL_segment->wals_group_id) == -1)
    {
    WAL_IO_ERROR (status_vector, "logfile chown", new_logname,
            isc_io_open_err, errno);
    return FAILURE;
    }
#endif

*log_type |= WALFH_SERIAL;      

return SUCCESS;
}

static BOOLEAN get_next_usable_partition (
    STATUS	*status_vector,
    SCHAR	*dbname,
    SCHAR	*master_logname,
    SLONG	*new_offset)
{
/**************************************
 *
 *	g e t _ n e x t _ u s a b l e _ p a r t i t i o n
 *
 **************************************
 *
 * Functional description
 *	Tries to find a usable partition in the master_logname.
 *	Returns TRUE if a usable partition is found else returns FALSE.
 *	new_logname and new_offset are updated in case of success.
 *
 **************************************/
P_LOGFH	p_log_header;
SLONG	p_log_fd;
int	i, j, count;
BOOLEAN	found;
SLONG	p_offset; 

p_log_header = (P_LOGFH) gds__alloc (P_LOGFH_LENGTH);
/* NOMEM: return failure, FREE: by returns in this procedure */
if (!p_log_header)
    return FALSE;

if (WALF_open_partitioned_log_file (status_vector, dbname, master_logname, 
                                    p_log_header, &p_log_fd) != SUCCESS)
    {
    gds__free ((SLONG*) p_log_header);
    return FALSE;
    }

/* Now check for a free partition */

i = j = (p_log_header->p_logfh_curp + 1) % p_log_header->p_logfh_maxp;
count = 0;
found = FALSE;
while (TRUE)
    {
    if ((j == i) && (count > 1)) /* We have exhausted all the partitions */
        break; 
    p_offset = PARTITION_OFFSET(p_log_header, j);
    if (found = get_log_usability (status_vector, dbname, master_logname, 
                                   p_offset))
        {
        p_log_header->p_logfh_curp = j;
        *new_offset = p_offset;
        WALF_update_partitioned_log_hdr (status_vector, master_logname, 
                                         p_log_header, p_log_fd);
        break;
        }        
    
    /* Check for the next partition */

    j = (j + 1) % p_log_header->p_logfh_maxp;
    count++;
    }

LLIO_close (NULL_PTR, p_log_fd);
gds__free ((SLONG*) p_log_header);

return found;
}

static SSHORT get_overflow_logname (
    STATUS  *status_vector,
    WALS    WAL_segment, 
    SCHAR    *new_logname,
    SLONG    *new_offset,
    SLONG    *log_type)
{
/**************************************
 *
 *	g e t _ o v e r f l o w _ l o g n a m e 
 *
 **************************************
 *
 * Functional description
 *	Get the name of the next overflow log file, if base name specified.
 *	Returns SUCCESS if an overflow log file name can be created else
 *	returns FAILURE.
 *
 **************************************/
LOGF	*logf;
SLONG	fd;

logf = &WAL_segment->wals_log_ovflow_file_info;
if (logf->logf_name_offset != 0)
    {
    if (logf->logf_fname_seqno <= WAL_segment->wals_log_seqno)
	logf->logf_fname_seqno = WAL_segment->wals_log_seqno + 1;

    for (;;)
	{
	/* Now find a non-existent (i.e. new) overflow log file. */

	WALC_build_logname (new_logname, LOGF_NAME (logf), logf->logf_fname_seqno);
	logf->logf_fname_seqno++;
	if (LLIO_open (status_vector, new_logname, LLIO_OPEN_NEW_RW, TRUE, &fd) == SUCCESS)
	    {
	    LLIO_close(status_vector, fd);
	    *new_offset = 0;
	    break;
	    }
	}

    *log_type = *log_type | WALFH_OVERFLOW;

#ifdef UNIX
    /* WAL writer would typically be running as root.  So give the 
       ownership of the new log file to the database owner. */
    if (chown (new_logname, WAL_segment->wals_owner_id, 
               WAL_segment->wals_group_id) == -1)
	{
	WAL_IO_ERROR (status_vector, "logfile chown", new_logname,
            isc_io_open_err, errno);
	return FAILURE;
	}
#endif
    return SUCCESS;
    }

return FAILURE;
}

static void get_time_stamp (
    SLONG    *date)
{
/**************************************
 *
 *	g e t _ t i m e _ s t a m p
 *
 **************************************
 *
 * Functional description
 *	Get the current time in gds format.
 *	(Copied from mov.c)
 *
 **************************************/
SLONG		clock;
struct tm	times;

clock = (SLONG) time ((time_t*) NULL);
times = *localtime ((time_t*) &clock);
isc_encode_date (&times, (GDS__QUAD *) date);
}

static SSHORT increase_buffers (
    STATUS  *status_vector,
    WAL     WAL_handle,
    SSHORT   num_buffers)
{
/**************************************
 *
 *	i n c r e a s e _ b u f f e r s
 *
 **************************************
 *
 * Functional description
 *	This function would try to increase the number of buffers in
 *	the shared WAL segment by the given number of buffers.  May 
 *	involve remapping (or reallocating) the WAL segment.
 *	Used for performance tuning.   Assumes that acquire() has been 
 *	done before calling this routine.
 *
 **************************************/
WALS	WAL_segment;
WALBLK	*wblk;
SLONG	old_wal_length, new_wal_length;
SSHORT	i, old_num_buffers;
SSHORT	ret;

/* First flush all the filled buffer blocks to the current log file */

if ((ret = flush_all_buffers (status_vector, WAL_handle)) != SUCCESS)
    return ret;

WAL_segment = WAL_handle->wal_segment;

#ifdef NETWARE_386
WAL_segment->wals_flags2 |= WALS2_CANT_EXPAND;
return SUCCESS;
#else
old_wal_length = WAL_segment->wals_length;
new_wal_length = old_wal_length + num_buffers * WAL_segment->wals_blksize;
if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "In increase_buffers(), before expansion:\n");
    ib_fprintf (DEBUG_FD, "WAL length =%ld, WAL segment flags=%ld, flags2=%ld\n",
             WAL_segment->wals_length, WAL_segment->wals_flags, WAL_segment->wals_flags2);
    ib_fprintf (DEBUG_FD, "maxbufs=%d, cur_buf=%d, last_flushed_buf=%d\n",
             WAL_segment->wals_maxbufs, CUR_BUF, WAL_segment->wals_last_flushed_buf);
    }
WAL_segment = (WALS) ISC_remap_file (status_vector,
    &WAL_handle->wal_shmem_data, new_wal_length, TRUE);

if (status_vector[1] == gds__unavailable)
    {
    WAL_segment = WAL_handle->wal_segment;
    WAL_segment->wals_flags2 |= WALS2_CANT_EXPAND;
    return SUCCESS;
    }
if (WAL_segment == (WALS) NULL)
    {
    WAL_ERROR (status_vector, gds__wal_cant_expand, DBNAME); 
    WAL_handle->wal_segment = NULL;
    report_walw_bug_or_error (status_vector, WAL_handle, FAILURE,
	(STATUS) gds__wal_err_expansion);
    }

old_num_buffers = WAL_segment->wals_maxbufs;
WAL_segment->wals_maxbufs += num_buffers;
	    
/* initialize the newly allocated buffer blocks. */

for (i = old_num_buffers; i < WAL_segment->wals_maxbufs; i++)
    {
    wblk = WAL_BLOCK(i);
    wblk->walblk_number = i;
    wblk->walblk_flags = 0;
    wblk->walblk_cur_offset = 0;
    wblk->walblk_roundup_offset = 0;
    }

WAL_segment->wals_length = new_wal_length;
WAL_handle->wal_segment = WAL_segment;

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "In increase_buffers(), after expansion:\n");
    ib_fprintf (DEBUG_FD, "WAL length =%ld, WAL segment flags=%ld, flags2=%ld\n",
             WAL_segment->wals_length, WAL_segment->wals_flags, WAL_segment->wals_flags2);
    ib_fprintf (DEBUG_FD, "maxbufs=%d, cur_buf=%d, last_flushed_buf=%d\n",
             WAL_segment->wals_maxbufs, CUR_BUF, WAL_segment->wals_last_flushed_buf);
    ib_fflush (DEBUG_FD);
    }

return SUCCESS;
#endif
}

static SSHORT init_raw_partitions ( 
    STATUS	*status_vector,
    WAL		WAL_handle)  
{
/**************************************
 *
 *	i n i t _ r a w _ p a r t i t i o n s
 *
 **************************************
 *
 * Functional description
 *	Initialize all the configured raw partitioned log files.
 *
 **************************************/
WALS	WAL_segment;
LOGF	*logf;
SSHORT	i, ret_val;

WAL_segment = WAL_handle->wal_segment;

for (i = 0; i < WAL_segment->wals_max_logfiles; i++)
    {
    logf = LOGF_INFO(i);
    if (logf->logf_flags & LOGF_RAW) 
	{
	ret_val = WALF_init_p_log (status_vector, DBNAME, LOGF_NAME(logf),
	    PARTITIONED_LOG_TOTAL_SIZE(logf->logf_partitions, logf->logf_max_size),
	    logf->logf_partitions);
	if (ret_val != SUCCESS)
	    report_walw_bug_or_error (status_vector, WAL_handle, ret_val,
	                              (STATUS) gds__wal_err_logwrite);
	}
    }

return SUCCESS;
}

static SSHORT journal_connect (
    STATUS  *status_vector,
    WAL     WAL_handle)
{
/**************************************
 *
 *	j o u r n a l _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Connects to the journal server.
 *	Returns SUCCESS or FAILURE.
 *
 **************************************/
SSHORT	ret;
WALS	WAL_segment;

WAL_segment = WAL_handle->wal_segment;

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "Calling JRN_init for database %s\n", DBNAME);
    }

JOURNAL_HANDLE = NULL;
ret = JRN_init (status_vector, &(JOURNAL_HANDLE), WAL_segment->wals_dbpage_size, 
                WAL_segment->wals_jrn_dirname, strlen (WAL_segment->wals_jrn_dirname), 
                WAL_segment->wals_jrn_init_data, WAL_segment->wals_jrn_init_data_len);

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "After calling JRN_init(), ret=%d\n", ret); 
    }

if (ret == SUCCESS)
    WAL_segment->wals_flags |= WALS_JOURNAL_ENABLED;
else
    report_walw_bug_or_error (status_vector, WAL_handle, ret,
	(STATUS) gds__wal_err_jrn_comm);

return ret;
}

static void journal_disable (
    STATUS      *status_vector,
    WAL         WAL_handle,
    WALFH       log_header)
{
/**************************************
 *
 *	j o u r n a l _ d i s a b l e 
 *
 **************************************
 *
 * Functional description
 *	Disconnects from the journal server after rolling over to a new 
 *	log file so that the current log file does not get deleted by 
 *	the journal server and the user can at least recover upto journal
 *	disable point. 
 *
 **************************************/
WALS	WAL_segment;

LOCAL_FLAGS |= WALW_DISABLING_JRN;
rollover_log (status_vector, WAL_handle, log_header);
LOCAL_FLAGS &= ~(WALW_DISABLING_JRN);
JRN_fini (status_vector, &(JOURNAL_HANDLE));

if (!JOURNAL_HANDLE)
    {
    /* Disable succeeded */

    WAL_segment = WAL_handle->wal_segment;
    WAL_segment->wals_flags &= ~WALS_JOURNAL_ENABLED;
    WAL_segment->wals_flags &= ~WALS_DISABLE_JOURNAL;
    }
}

static SSHORT journal_enable (
    STATUS  *status_vector,
    WAL     WAL_handle)
{
/**************************************
 *
 *	j o u r n a l _ e n a b l e 
 *
 **************************************
 *
 * Functional description
 *	Connects to the journal server and sends it the current WAL file
 *	info.  Returns SUCCESS or FAILURE.
 *
 **************************************/
SSHORT	ret;
WALS	WAL_segment;

WAL_segment = WAL_handle->wal_segment;
ret = journal_connect (status_vector, WAL_handle);
if (ret == SUCCESS)
    {
    if (PRINT_DEBUG_MSGS)
	{
	PRINT_TIME (DEBUG_FD, LOCAL_TIME);
	ib_fprintf (DEBUG_FD, "Enabling journaling for database %s\n", DBNAME);
	ib_fprintf (DEBUG_FD, "Sending open for seqno=%d, log %s, p_offset=%d, length=%d\n", 
                   WAL_segment->wals_flushed_log_seqno,
                   WAL_segment->wals_logname,
                   WAL_segment->wals_log_partition_offset,
                   WAL_segment->wals_flushed_offset);
	}

    ret = JRN_put_wal_name (status_vector, JOURNAL_HANDLE, 
                           WAL_segment->wals_logname, strlen (WAL_segment->wals_logname),
		           WAL_segment->wals_flushed_log_seqno, 
                           WAL_segment->wals_flushed_offset,
                           WAL_segment->wals_log_partition_offset,
		           JRNW_OPEN);

    if (PRINT_DEBUG_MSGS)
	{
	PRINT_TIME (DEBUG_FD, LOCAL_TIME);
	ib_fprintf (DEBUG_FD, "After calling JRN_put_wal_name() for seqno=%d, ret=%d\n",
            WAL_segment->wals_flushed_log_seqno, ret);
	}

    }
if (ret != SUCCESS)
    {
    WAL_segment->wals_flags &= ~WALS_JOURNAL_ENABLED;
    report_walw_bug_or_error (status_vector, WAL_handle, ret, 
	(STATUS) gds__wal_err_jrn_comm);
    }
else
    {
    WAL_segment->wals_flags &= ~WALS_ENABLE_JOURNAL;
    }

return ret;
}

static void prepare_wal_block (
    WALS	WAL_segment,
    WALBLK	*wblk)
{
/**************************************
 *
 *	p r e p a r e _ w a l _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Initialize block sequence number, timestamp and length bytes
 *	of the WAL buffer in the given WAL block.
 *
 **************************************/
WALBLK_HDR  header;
USHORT       len;    /* The header and trailer length for the buffer block */
SCHAR        *p, *q;

len = (USHORT) wblk->walblk_roundup_offset; /* Already includes the block trailing bytes */

/* The following way of initializing and then copying the header to 
   walblk_buf is used to avoid datatype misalignment problem on some machines. */

header.walblk_hdr_blklen = len;
header.walblk_hdr_len = wblk->walblk_cur_offset;
header.walblk_hdr_bsn = WAL_segment->wals_blkseqno++;
get_time_stamp (header.walblk_hdr_timestamp); 
memcpy(wblk->walblk_buf, (SCHAR*)&header, BLK_HDROVHD);

p = (SCHAR*)&wblk->walblk_buf[(int)len-BLK_TAILOVHD];
q = (SCHAR*)&len;
*p++ = *q++;
*p++ = *q++;
}

static void release_wal_block (
    WALS    WAL_segment,
    WALBLK  *wblk)
{
/**************************************
 *
 *	r e l e a s e _ w a l _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	A block has just been written to disk.  Update statistics and 
 *	make this block available to other users.
 *
 **************************************/
USHORT   len;      /* Number of bytes written to log file */

len = (USHORT)wblk->walblk_roundup_offset;
wblk->walblk_cur_offset = 0;
wblk->walblk_roundup_offset = 0;
wblk->walblk_flags &= ~WALBLK_to_be_written;

WAL_segment->wals_last_flushed_buf = wblk->walblk_number;
WAL_segment->wals_flushed_offset += len;
WAL_segment->wals_cur_ckpt_intrvl += len;
WAL_segment->wals_total_IO_bytes += len;
WAL_segment->wals_IO_count++;

if (wblk->walblk_flags & WALBLK_checkpoint)
    {
    /* This buffer finished a checkpoint */

    finishup_checkpoint(WAL_segment);
    wblk->walblk_flags &= ~WALBLK_checkpoint;
    }
    
if (CUR_BUF == -1)
    CUR_BUF = wblk->walblk_number;
}

static void report_walw_bug_or_error (
    STATUS	*status_vector,
    WAL		WAL_handle,
    SSHORT	failure_type,
    STATUS	code)
{
/**************************************
 *
 *	 r e p o r t _ w a l w _ b u g _ o r _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Handle bug or error for the WAL writer.
 *
 **************************************/
STATUS	local_status [20];
TEXT	errbuf [MAX_ERRMSG_LEN];
WALS	WAL_segment;

WAL_segment = WAL_handle->wal_segment;

/* First give a generic WAL writer error message */

WALW_ERROR (local_status);
gds__log_status (DBNAME, local_status);
gds__print_status (local_status);

/* Now give the error message for the particular error or bug */

if (failure_type > 0)
    {
    /* An error happened. status_vector is already inititalized */

    if (WAL_segment) 
	{
	WAL_segment->wals_flags |= WALS_ERROR_HAPPENED;
	WAL_segment->wals_last_err = code;
	}
    gds__log_status (DBNAME, status_vector);
    }
else if (failure_type < 0)
    {
    /* A bug happened. status_vector is already inititalized */

    if (WAL_segment) 
	{
	WAL_segment->wals_flags |= WALS_BUG_HAPPENED;
	WAL_segment->wals_last_bug = code;
	}
    IBERR_bugcheck (status_vector, DBNAME, NULL, -1*failure_type, errbuf);
    WALC_save_status_strings (status_vector);
    }
else
    {
    /* An error happened. status_vector needs to be inititalized */

    if (WAL_segment)
	{
	WAL_segment->wals_flags |= WALS_ERROR_HAPPENED;
	WAL_segment->wals_last_err = code;
	}
    IBERR_error (status_vector, DBNAME, NULL, code, errbuf);
    WALC_save_status_strings (status_vector);
    gds__log_status (DBNAME, status_vector);
    }
gds__print_status (status_vector);

if (PRINT_DEBUG_MSGS)
    {
    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
    ib_fprintf (DEBUG_FD, "WAL writer encountered error, code=%d\n", code);
    }

longjmp (ENV, (int) status_vector [1]);
}

static SSHORT rollover_log (
    STATUS      *status_vector,
    WAL         WAL_handle,
    WALFH       log_header)
{
/**************************************
 *
 *	r o l l o v e r _ l o g
 *
 **************************************
 *
 * Functional description
 *	The current log file has reached its limit length.  So move on 
 *	to the next one.  Update WAL_segment and log_header.  WAL_segment
 *	is assumed to have already been acquired. 
 *
 **************************************/
WALFH	new_log_header;
SCHAR	saved_logname [MAXPATHLEN];
SCHAR	new_logname [MAXPATHLEN];
SLONG	new_log_fd;
SSHORT	ret;
SLONG	saved_flushed_offset;
SLONG	new_log_partition_offset;
WALS	WAL_segment;
SLONG	log_type;
#define ROLLOVER_LOG_RETURN(code) {gds__free((SLONG*)new_log_header); return(code);}

/* First flush all the filled buffer blocks to the current log file
   because we have already given its log file sequence number for
   the records in those buffers. */

if ((ret = flush_all_buffers (status_vector, WAL_handle)) != SUCCESS)
    return ret;

WAL_segment = WAL_handle->wal_segment;
new_log_header = (WALFH) gds__alloc (WALFH_LENGTH);
/* NOMEM: return failure, FREE: by macro ROLLOVER_LOG_RETURN() */
if (!new_log_header)
    return FAILURE;

saved_flushed_offset = WAL_segment->wals_flushed_offset;
strcpy (saved_logname, WAL_segment->wals_logname);

log_type = 0L;
if (get_next_logname (status_vector, WAL_segment, new_logname, 
	             &new_log_partition_offset,
	             &log_type) != SUCCESS)
    {
    WAL_ERROR_APPEND (status_vector, gds__wal_err_rollover, new_logname);
    report_walw_bug_or_error (status_vector, WAL_handle, FAILURE,
	                      gds__wal_err_rollover2);
    ROLLOVER_LOG_RETURN (FAILURE);
    }

ret = setup_log (status_vector, WAL_handle, new_logname, 
	         new_log_partition_offset, log_type,
	         &new_log_fd, new_log_header, TRUE, WAL_segment->wals_logname,
	         WAL_segment->wals_log_partition_offset);
if (ret == SUCCESS)
    {
    /* Update the current log file with the name of the next log file name
       and close it. */

    if (log_header->walfh_next_logname)
	gds__free ((SLONG*) log_header->walfh_next_logname);
    STRING_DUP (log_header->walfh_next_logname, new_logname);
    /* NOMEM: failure return  FREE: unknown */
    if (!log_header->walfh_next_logname)
	ROLLOVER_LOG_RETURN (FAILURE);

    log_header->walfh_next_log_partition_offset = new_log_partition_offset;

    /* The previous log name of the current log file may happen to be the 
       new_logname if there are 2 round-robin log files and we are re-using
       the new_logname.  The same could happen even if we have one partitioned
       log file with only 2 partitions.  In those cases, make the prev_logname
       of the current log file to be empty to avoid a potential infinite loop. */

    if ((strcmp (log_header->walfh_prev_logname, new_logname) == 0) &&
        (log_header->walfh_prev_log_partition_offset == new_log_partition_offset))
        {
        *log_header->walfh_prev_logname = 0;
        log_header->walfh_prev_log_partition_offset = 0L;
        }

    log_header->walfh_length = saved_flushed_offset;
    log_header->walfh_hibsn = WAL_segment->wals_blkseqno - 1;
    close_log (status_vector, WAL_handle, saved_logname, log_header, TRUE);

    /* This is a good place to inform the long term journal server 
       that we have rolled over to a new log file.  Note that the close
       info of the previous log file has already been sent during 
       close_log() call made above. */

    if (JOURNAL_HANDLE && !(LOCAL_FLAGS & WALW_DISABLING_JRN))
	{
	if (PRINT_DEBUG_MSGS)
 	    {
	    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
	    ib_fprintf (DEBUG_FD, "Opened seqno=%d, log %s, p_offset=%d, length=%d\n", 
		new_log_header->walfh_seqno, new_logname,  
		new_log_header->walfh_log_partition_offset, 
		new_log_header->walfh_length);
	    }

	ret = JRN_put_wal_name (status_vector, JOURNAL_HANDLE, 
	                        new_logname, strlen (new_logname),
	                        new_log_header->walfh_seqno, 
	                        new_log_header->walfh_length,
	                        new_log_header->walfh_log_partition_offset,
	                        JRNW_OPEN);

	if (PRINT_DEBUG_MSGS)
 	    {
	    PRINT_TIME (DEBUG_FD, LOCAL_TIME);
	    ib_fprintf (DEBUG_FD, "After calling JRN_put_wal_name() for seqno=%d, ret=%d\n",
	            new_log_header->walfh_seqno, ret);
	    }

	if (ret != SUCCESS)
	    report_walw_bug_or_error (status_vector, WAL_handle, ret,
	                              gds__wal_err_jrn_comm);
	}

    WAL_segment->wals_flags |= WALS_ROLLOVER_HAPPENED;
    
    /* Update the context variables for the caller. */

    LOG_FD = new_log_fd;
    memcpy((SCHAR*)log_header, (SCHAR*)new_log_header, WALFH_LENGTH);
    
    ROLLOVER_LOG_RETURN (SUCCESS);
    }
else
    {
    report_walw_bug_or_error (status_vector, WAL_handle, ret,
                              gds__wal_err_rollover2);
    ROLLOVER_LOG_RETURN (FAILURE);
    }
}

static void setup_for_checkpoint (
    WALS    WAL_segment)
{
/**************************************
 *
 *	s e t u p _ f o r _ c h e c k p o i n t 
 *
 **************************************
 *
 * Functional description
 *	Inform the Asynchronous Buffer writer that it is time to 
 *	start checkpointing.   Assumes that acquire() has 
 *	been done before calling this routine.
 ***************************************/

/* AB writer checks this condition by calling WAL_start_checkpoiting() 
   in its main loop. */

WAL_segment->wals_flags |= WALS_CKPT_START;
WAL_segment->wals_flags &= ~WALS_CKPT_RECORDED;  /* Force this anyway */
}

static SSHORT setup_log (
    STATUS	*status_vector,
    WAL		WAL_handle,
    SCHAR	*logname,
    SLONG	log_partition_offset, 
    SLONG	log_type,
    SLONG	*logfile_fd,
    WALFH	log_header,
    SSHORT	rollover,
    SCHAR	*prev_logname,
    SLONG	prev_log_partition_offset)
{
/**************************************
 *
 *	s e t u p _ l o g 
 *
 **************************************
 *
 * Functional description
 *	Initialize the log file header information and 
 *	the corresponding WAL_segment information.
 *	Returns SUCCESS or an errno.
 *
 **************************************/
SSHORT	ret;
LOGF	*logf;
SSHORT	takeover;
WALS	WAL_segment;

WAL_segment = WAL_handle->wal_segment;

ret = setup_log_header_info (status_vector, WAL_handle, logname, 
                             log_partition_offset, log_type, logfile_fd, log_header,
                             rollover, prev_logname, prev_log_partition_offset,
                             &takeover);
if (ret == SUCCESS)
    {
    /* Now updates the appropriate fields of WAL_segment from the given 
       log_header.  */

    strcpy (WAL_segment->wals_logname, logname);
    WAL_segment->wals_log_partition_offset = log_header->walfh_log_partition_offset;
    if (!takeover)
        {
        /* If a new WAL writer is taking over, we don't want to change the
           wals_buf_offset etc. which might reflect the uptodate WAL buffer 
           position. */

        WAL_segment->wals_log_seqno = log_header->walfh_seqno;   
        WAL_segment->wals_buf_offset = log_header->walfh_length;
        WAL_segment->wals_blkseqno = log_header->walfh_hibsn + 1;   
        WAL_segment->wals_flushed_offset = log_header->walfh_length;
        WAL_segment->wals_flushed_log_seqno = log_header->walfh_seqno;
        }

    if (WAL_segment->wals_max_logfiles > 0)
       {
        /* Get the index of this logfile in the list of pre-allocated 
           log files. */

        WAL_segment->wals_cur_logfile = get_logfile_index(WAL_segment, logname);
        if (WAL_segment->wals_cur_logfile == -1)
            /* we are using an overflow log */
            logf = &WAL_segment->wals_log_ovflow_file_info;
        else
            logf = LOGF_INFO(WAL_segment->wals_cur_logfile);
        }
    else
        logf = &WAL_segment->wals_log_serial_file_info;

    WAL_segment->wals_rollover_threshold = (logf->logf_max_size ? 
               logf->logf_max_size : WAL_segment->wals_max_log_length);
    WAL_segment->wals_roundup_size = logf->logf_roundup_size;

    if ((PRINT_DEBUG_MSGS) && rollover)  
        {
        PRINT_TIME (DEBUG_FD, LOCAL_TIME);
        ib_fprintf (DEBUG_FD, "new log %s, partition offset=%ld\n",
                WAL_segment->wals_logname, WAL_segment->wals_log_partition_offset);
        }

    }

return ret;
}

static SSHORT setup_log_header_info ( 
    STATUS	*status_vector,
    WAL		WAL_handle,
    SCHAR	*logname,
    SLONG	log_partition_offset,
    SLONG        log_type,
    SLONG	*logfile_fd,
    WALFH	log_header,
    SSHORT	rollover,
    SCHAR	*prev_logname,
    SLONG	prev_log_partition_offset,
    SSHORT	*takeover)
{
/**************************************
 *
 *	s e t u p _ l o g _ h e a d e r _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Open a given log file. 
 *	Initialize the log file header information.
 *	If 'rollover' flag is TRUE, then we are going to open a new log
 *	file.  So use the 'prev_logname' as the previous log file 
 *	name for the new log file.
 *	If we are opening an existing log file and it has a 'next' log
 *	file, then open the 'next' log file automatically using a recursive
 *	call.  
 *   
 *	If we determine that this is a takeover situation, set the takeover
 *	parameter to TRUE.
 *   
 *	Returns SUCCESS or FAILURE.
 *
 **************************************/
WALS	WAL_segment;
SLONG	log_fd, read_len;
SCHAR	prev_logfname [MAXPATHLEN];
SLONG	prev_logfpartition_offset;

WAL_segment = WAL_handle->wal_segment;

/* Open the Log file */

if (LLIO_open (status_vector, logname, LLIO_OPEN_WITH_SYNC_RW, TRUE, &log_fd))
    return FAILURE;

*takeover = FALSE;

if (!rollover)
    {
    /* Try to read the logfile header and take appropriate steps 
       afterwards. */

    if (LLIO_read (status_vector, log_fd, logname,
	log_partition_offset, LLIO_SEEK_BEGIN,
	(SCHAR*) log_header, WALFH_LENGTH, &read_len))
	{
	LLIO_close (NULL_PTR, log_fd);
	return FAILURE;
	}
    }

if (rollover || (read_len == 0))
    {
    /* The logfile is brand new */

    log_header->walfh_version = WALFH_VERSION; 
    log_header->walfh_wals_id = WAL_segment->wals_id; 
    log_header->walfh_seqno = WAL_segment->wals_log_seqno + 1;
    log_header->walfh_log_partition_offset = log_partition_offset;
    log_header->walfh_length = (SLONG)WALFH_LENGTH;
    log_header->walfh_lowbsn = WAL_segment->wals_blkseqno;  /* What the lowbsn would be */
    log_header->walfh_hibsn = WAL_segment->wals_blkseqno - 1;   /* So far */
    log_header->walfh_flags = log_type + WALFH_OPEN;   
    log_header->walfh_offset = (SLONG)WALFH_LENGTH;  
    log_header->walfh_ckpt1 = (SLONG) 0;    
    log_header->walfh_ckpt2 = (SLONG) 0;    
    log_header->walfh_reserved1 = (SLONG) 0;
    log_header->walfh_reserved2 = (SLONG) 0;
    log_header->walfh_data[0] = 0;
    STRING_DUP (log_header->walfh_dbname, WAL_segment->wals_dbname);  
    /* NOMEM: failure, FREE: unknown */
    if (!log_header->walfh_dbname)
	return FAILURE;
    STRING_DUP (log_header->walfh_prev_logname, prev_logname);  
    /* NOMEM: failure, FREE: unknown */
    if (!log_header->walfh_prev_logname)
	return FAILURE;
    log_header->walfh_prev_log_partition_offset = prev_log_partition_offset;
    STRING_DUP (log_header->walfh_next_logname, "");  
    /* NOMEM: failure, FREE: unknown */
    if (!log_header->walfh_next_logname)  
	return FAILURE;
	
    log_header->walfh_data_len = 
       MISC_build_parameters_block (log_header->walfh_data, 
	    PARAM_BYTE(WALFH_dbname), PARAM_STRING(log_header->walfh_dbname),
	    PARAM_BYTE(WALFH_prev_logname), PARAM_STRING(log_header->walfh_prev_logname),
	    PARAM_BYTE(WALFH_next_logname), PARAM_STRING(log_header->walfh_next_logname),
	    PARAM_BYTE(WALFH_end),
	    0);
    }
else if (read_len < sizeof (struct walfh))
    {
    LLIO_close (NULL_PTR, log_fd);
    WAL_ERROR (status_vector, gds__logh_small, logname);
    return FAILURE;
    }
else if (log_header->walfh_version != WALFH_VERSION) 
    {
    LLIO_close (NULL_PTR, log_fd);
    WAL_ERROR (status_vector, gds__logh_inv_version, logname);
    return FAILURE;
    }
else if (log_header->walfh_flags & WALFH_OPEN)
    {
    if ((log_header->walfh_wals_id == WAL_segment->wals_id) && 
	(log_header->walfh_seqno == WAL_segment->wals_log_seqno))
	{
	/* The file had been in use, may be the previous WAL writer process
	     died.  So, take over.  Do some sanity checks though. */

	*takeover = TRUE;
	log_header->walfh_length = WAL_segment->wals_flushed_offset;
	log_header->walfh_dbname = NULL;        /* will be updated below */
	log_header->walfh_prev_logname = NULL;  /* will be updated below */
	log_header->walfh_next_logname = NULL;     /* will be updated below */
	WALF_upd_log_hdr_frm_walfh_data(log_header, log_header->walfh_data); 

	/* Well, in this case, walfh_next_logname should be empty. Make
	  sure that it is, else give an error message for using an earlier
	  log file. */

	if (strlen (log_header->walfh_next_logname) != 0)
	    {
	    LLIO_close (NULL_PTR, log_fd);
	    WAL_ERROR (status_vector, gds__logh_open_flag, logname);
	    return FAILURE;
	    }
	}
    else
	{
	/* This situation should not arise.   Before this, a recovery should 
	   have taken place.  However, a graceful way of handling it
	   could be to scan the log and position at the end of the last 
	   'valid looking' block. */

	LLIO_close (NULL_PTR, log_fd);
	WAL_ERROR (status_vector, gds__logh_open_flag2, logname);
	return FAILURE;
	}
    }
else
    {
    /* We are opening a logfile after it was properly shutdown last time.
       Prepare the existing log file for new blocks. */

    log_header->walfh_dbname = NULL;
    log_header->walfh_prev_logname = NULL;
    log_header->walfh_next_logname = NULL;
    WALF_upd_log_hdr_frm_walfh_data(log_header, log_header->walfh_data); 

    /* In this case, walfh_next_logname should be empty.  But there is 
       a small window wherein the rollover to a new logfile occurred but
       that fact was not recorded at higher level (in database header page). 
       In that case,  walfh_next_logname would not be empty and it would be 
       reasonable for us to open and use the next log file. */

    if (strlen (log_header->walfh_next_logname) != 0)
	{
	LLIO_close (NULL_PTR, log_fd);
	strcpy (prev_logfname, logname);
	strcpy (logname, log_header->walfh_next_logname);
	prev_logfpartition_offset = log_partition_offset;
	return setup_log_header_info (status_vector, WAL_handle, logname, 
	                      log_header->walfh_next_log_partition_offset, 0L,
	                      logfile_fd, log_header, rollover, 
	                      prev_logfname, prev_logfpartition_offset,
	                      takeover);
	}
    }

*logfile_fd = log_fd;
log_header->walfh_wals_id = WAL_segment->wals_id; 
log_header->walfh_flags |= WALFH_OPEN;   
log_header->walfh_flags |= WALFH_KEEP_FOR_SHORT_TERM_RECV;   
if (JOURNAL_HANDLE && !(LOCAL_FLAGS & WALW_DISABLING_JRN))
    log_header->walfh_flags |= WALFH_KEEP_FOR_LONG_TERM_RECV;   

return write_log_header_and_reposition(status_vector, logname, log_fd,
	                               log_header);
}

static SSHORT write_log_header_and_reposition (
    STATUS	*status_vector,
    SCHAR	*logname,
    SLONG	log_fd,
    WALFH	log_header)
{
/**************************************
 *
 *	w r i t e _ l o g _ h e a d e r _ a n d _ r e p o s i t i o n
 *
 **************************************
 *
 * Functional description
 *	Write the passed log_header at the beginning of the log file
 *	and then seek it to the end as per log_header->walfh_length value.
 *	Returns SUCCESS or FAILURE.
 *
 **************************************/

if (LLIO_write (status_vector, log_fd, logname,
    log_header->walfh_log_partition_offset, LLIO_SEEK_BEGIN,
    (SCHAR*) log_header, WALFH_LENGTH, NULL_PTR))
    return FAILURE;

/* Let's write an invalid block header at the end so that even if we 
   are reusing an old log file, we don't use its old data */

if (LLIO_write (status_vector, log_fd, logname,
    log_header->walfh_log_partition_offset + log_header->walfh_length, LLIO_SEEK_BEGIN,
    (SCHAR*) log_terminator_block, WAL_TERMINATOR_BLKLEN, NULL_PTR))
    return FAILURE;

if (LLIO_seek (status_vector, log_fd, logname, -1*WAL_TERMINATOR_BLKLEN, 
               LLIO_SEEK_CURRENT))
    return FAILURE;

return SUCCESS;
}

static SSHORT write_wal_block (
    STATUS	*status_vector,
    WALBLK	*wblk,
    SCHAR	*logname,
    SLONG	log_fd)
{
/**************************************
 *
 *	w r i t e _ w a l _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Writes the prepared wal buffer to WAL file. 
 *
 **************************************/

if (LLIO_write (status_vector, log_fd, logname, 0L, LLIO_SEEK_NONE,
    wblk->walblk_buf, wblk->walblk_roundup_offset, NULL_PTR))
    return FAILURE;

return SUCCESS;
}

static void write_wal_statistics (
    WAL		WAL_handle)
{
/**************************************
 *
 *	w r i t e _ w a l _ s t a t i s t i c s
 *
 **************************************
 *
 * Functional description
 *	Writes the performance statistics for the current WAL session.
 *
 *	these messages were considered for translation but left as is for now
 *	since they are likely only to be seen by DBA's and technical types
 *
 **************************************/
WALS	WAL_segment;
IB_FILE	*stat_file;

WAL_segment = WAL_handle->wal_segment;
if (PRINT_DEBUG_MSGS)  
    {
    stat_file   = DEBUG_FD;

    ib_fprintf (stat_file,"-----------------------------------------------\n");
    PRINT_TIME (stat_file, LOCAL_TIME);
    ib_fprintf (stat_file, "WAL writer (pid=%d) for database %s, shutdown statistics:\n",
             getpid(), DBNAME);
    ib_fprintf (stat_file, "WAL buffer size=%d, total buffers=%d, original buffers=%d\n",
        WAL_segment->wals_bufsize, WAL_segment->wals_maxbufs, WAL_segment->wals_initial_maxbufs); 
    ib_fprintf (stat_file, "Max ckpt interval=%ld\n",
            WAL_segment->wals_max_ckpt_intrvl);
    ib_fprintf (stat_file, "WAL segment acquire count=%ld, number of WAL_put() calls=%ld\n",
            WAL_segment->wals_acquire_count, WAL_segment->wals_put_count);
    ib_fprintf (stat_file, "Total IOs=%ld, Avg IO size=%d, last blk_seqno=%ld\n",
            WAL_segment->wals_IO_count, 
            WAL_segment->wals_total_IO_bytes /
	        (WAL_segment->wals_IO_count ? WAL_segment->wals_IO_count : 1),
            WAL_segment->wals_blkseqno-1);
    ib_fprintf (stat_file, "grpc wait micro-seconds=%d\n", 
            WAL_segment->wals_grpc_wait_usecs); 
    ib_fprintf (stat_file, "Total commits=%ld, Number of group-commits=%d, Avg grpc size=%f\n",
            WAL_segment->wals_commit_count,
            WAL_segment->wals_grpc_count, 
            (WAL_segment->wals_commit_count * 1.0) /
        	(WAL_segment->wals_grpc_count ? WAL_segment->wals_grpc_count : 
		                                WAL_segment->wals_commit_count));
    ib_fprintf (stat_file, "current log seqno=%ld, logfile=%s\n",
            WAL_segment->wals_log_seqno, WAL_segment->wals_logname);
        ib_fprintf (stat_file, "log partition offset=%ld, current offset=%ld\n",
            WAL_segment->wals_log_partition_offset, WAL_segment->wals_buf_offset);
    ib_fprintf (stat_file,"-----------------------------------------------\n");
    }
}
