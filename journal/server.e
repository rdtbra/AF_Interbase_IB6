/*
 *	PROGRAM:	JRD Journal Server
 *	MODULE:		server.e
 *	DESCRIPTION:	
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "../jrd/time.h"
#ifndef VMS
#include <sys/types.h>
#else
#include <file.h>
#include <types.h>
#endif
#ifdef _AIX
#include <sys/select.h>
#endif
#ifdef EPSON
#include <sys/select.h>
#endif
#include "../jrd/common.h"

/* Block types */

typedef struct blk {
    SCHAR	blk_type;
    SCHAR	blk_pool_id_mod;
    SSHORT	blk_length;
} *BLK;

#include "../jrd/jrn.h"
#include "../jrd/license.h"
#include "../jrd/thd.h"
#include "../jrd/isc.h"
#include "../jrd/pio.h"
#include "../jrd/gds.h"
#include "../jrd/llio.h"
#include "../journal/journal.h"
#include "../wal/wal.h"
#include "../journal/conso_proto.h"
#include "../journal/gjrn_proto.h"
#include "../journal/misc_proto.h"
#include "../journal/serve_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../jrd/llio_proto.h"
#include "../wal/walf_proto.h"

#undef MOVE_FAST
#undef MOVE_CLEAR
#define MOVE_FAST(from,to,length)	memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)		memset (to, 0, (int) (length))

#ifdef APOLLO
#define APOLLO_JOURNALLING
#endif

#ifdef UNIX
#include <sys/stat.h>
#define UNIX_JOURNALLING
#define BSD_SOCKETS
#ifdef SUN3_3
typedef SLONG	fd_mask;
#endif
#endif

#ifdef NETWARE_386
#define NETWARE_JOURNALLING
#define BSD_SOCKETS
#define EINTR			0
#endif

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH		512
#endif

/* Apollo stuff */

#ifdef APOLLO_JOURNALLING
#include <sys/stat.h>
#include <apollo/base.h>
#include <apollo/mbx.h>
#include <apollo/error.h>
#include <apollo/ec2.h>
#include "/sys/ins/streams.ins.c"
#define HANDLE	SLONG*
#endif

/* OS2 Stuff */

#ifdef OS2_ONLY
#define INCL_DOSSEMAPHORES
#define INCL_DOSNMPIPES
#define INCL_DOSFILEMGR
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#include <os2.h>
#include <process.h>

#define HANDLE		HFILE
typedef SLONG		f_mask;
#endif

/* UNIX and NETWARE JOURNALLING stuff */

#ifdef BSD_SOCKETS 
#define BSD_INCLUDES
#ifndef FCNTL_INCLUDED
#include <fcntl.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#define MAXHOSTNAMELEN	64	/* should be in <sys/param.h> */
#define HANDLE		int
#endif

/* Windows NT stuff */

#ifdef WIN_NT
#include <process.h>
#include <windows.h>
#define TEXT		SCHAR
#endif

#ifdef DELTA
#define waitpid(x,y,z)	wait (y)
#endif

#ifndef WIN_NT
#ifndef HANDLE
#define HANDLE		SSHORT
#endif
#endif

#define BITS			(sizeof (fd_mask) * 8)
#ifndef NETWARE_386
#define SET_BIT(fd,mask)	((mask)->fds_bits[fd/BITS] |= (1 << (fd % BITS)))
#define TEST_BIT(fd,mask)	((mask)->fds_bits[fd/BITS] & (1 << (fd % BITS)))
#else
#define SET_BIT(fd,mask)	FD_SET (fd, mask)
#define TEST_BIT(fd,mask)	FD_ISSET (fd, mask)
#endif

DATABASE
    DB = STATIC COMPILETIME FILENAME "journal.gdb" RUNTIME FILENAME journal_db;

#define LOGFILE		"journal.log"
#define MAX_CHANNEL	64

#ifdef WIN_NT
#define ARCHIVE		"bin/ibarchiv"
#endif

#ifndef ARCHIVE
#define ARCHIVE		"bin/gds_archive"
#endif

/* List of console commands */

CMDS	commands [] =
    {
    "DISABLE <db name>	",	cmd_disable,	163, "\tdisable database",
    "DROP <db name>		",		cmd_drop,	164, "\tdrop all files for a database",
    "ERASE <db id>		",		cmd_erase,	165, "\tdrop all files for non active database",
    "EXIT			",			cmd_exit,	166, "\texit journal console program",
    "HELP			",			cmd_help,	167, "\tdisplay this text",
    "RESET DELETE <db name>	",	cmd_reset,	168, "\treset delete flag for database",
    "SET DELETE <db name>	",	cmd_set,	169, "\tset delete flag for database",
    "SHUTDOWN		",			cmd_shutdown,	170, "\tshutdown journal server",
    "STATUS			",			cmd_status,	171, "\tdisplay status of journal server",
    "VERSION			",  		cmd_version,	172, "\tdisplay software version number",
    "ARCHIVE <db name>	",  		cmd_archive,	239, "\trestart archive for database",
    "QUIT			",			cmd_exit,	0, NULL,
    NULL			,			cmd_none,	0
    };
typedef struct backup_info  {
    SLONG	bi_pid;
    SLONG	bi_fid;
} BINFO;

#define MAX_ARCHIVE	16
#define MAX_RETRY	16

/* Active database block */

typedef struct djb {
    struct djb	*djb_next;		/* Next block */
    struct cnct	*djb_connections;	/* List of connections */
    USHORT	djb_flags;		/* flags - see values below */
    USHORT	djb_id;			/* Id of database */
    USHORT	djb_use_count;		/* Number of connections */
    SSHORT	djb_retry;		/* number of retries */
    SSHORT	djb_bfi_count;
    BINFO	djb_bfi [MAX_ARCHIVE +1];/* backup file info */
    UCHAR	*djb_dir_name;		/* backup directory name */
    UCHAR	djb_db_name [1];	/* Database filename */
} *DJB;

#define DJB_disabled		1
#define DJB_archive		2
#define DJB_set_delete		4

/* Individual journal connections */

typedef struct cnct {
    struct cnct	*cnct_next;		/* Next connection for system */
    struct cnct	*cnct_db_next;		/* Next connection for database */
    struct djb	*cnct_database;		/* Active database block */
    HANDLE	cnct_handle;		/* Parent mailbox */
    SLONG	cnct_channel;		/* Channel for communication */
    USHORT	cnct_flags;
    USHORT	cnct_version;		/* Protocol version */
#ifdef WIN_NT
    OVERLAPPED	cnct_overlapped;
    UCHAR	cnct_data [sizeof (struct jrnh)];
    USHORT	cnct_read_length;
#endif
} *CNCT;

#define CNCT_console		1	/* Connection is to a console */
#define CNCT_pending_io		2	/* I/O or connection is pending */

#ifdef NETWARE_386
extern void	main_archive();
#endif

static void	add_backup_entry (DJB, SLONG, SLONG);
static CNCT	alloc_connect (HANDLE, USHORT);
static void	backup_wal (DJB, SCHAR *, SLONG, SLONG, SLONG, SLONG);
static void	build_backup_array (DJB);
static void	build_indexed_name (SCHAR *, SCHAR *, SLONG);
static void	close_mailbox (int);
static void	commit (void);
static void	delete_backup_entry (DJB, SLONG);
static void	delete_connection (CNCT);
static void	delete_database (DJB);
static void	delete_history_files (CNCT, SCHAR *, SLONG, SLONG);
static void	delete_wal (void);
static void	disable (CNCT, LTJC *);
static void	disconnect (CNCT);
#if !(defined VMS || defined OS2_ONLY || defined WIN_NT || defined NETWARE_386)
static void	divorce_terminal (fd_set *);
#endif
static void	enable (CNCT, LTJC *);
#ifdef APOLLO_JOURNALLING
static void	error (status_$t, TEXT *);
#else
static void	error (STATUS, TEXT *);
#endif
static void	error_exit (STATUS, TEXT *);
static void	free_database_entry (DJB);
static void	get_message (JRNH *, int);
#ifdef BSD_SOCKETS
static void	init_handles (fd_set *);
#endif
static DJB	lookup_database (SCHAR *, USHORT, SSHORT);
static DJB	make_database (CNCT, SCHAR *, SCHAR *);
static void	open_mailbox (SCHAR *, USHORT);
#ifdef BSD_SOCKETS
static void	post_address (USHORT, struct sockaddr_in);
#endif
static void	process_command (CNCT, TEXT *);
static void	process_archive_begin (CNCT, LTJA *);
static void	process_archive_end (CNCT, LTJA *);
static void	process_archive_error (CNCT, LTJA *);
static void	process_control_point (CNCT, LTJW *);
static void	process_disable_database (CNCT, TEXT *);
static void	process_drop_database (CNCT, SCHAR *);
static void	process_dump_file (CNCT, LTJW *);
static void	process_end_dump (CNCT, LTJW *);
static void	process_erase_database (CNCT, USHORT);
static void	process_message (CNCT, JRNH *, USHORT);
static void	process_reset_database (CNCT, SCHAR *);
static void	process_restart_archive (CNCT, SCHAR *);
static void	process_set_database (CNCT, SCHAR *);
static void	process_start_dump (CNCT, LTJW *);
static void	process_wal_file (CNCT, LTJW *);
static void	put_line (CNCT, SSHORT, TEXT *, TEXT *, TEXT *);
static void	put_message (SSHORT, DJB);
static void	read_asynchronous (CNCT);
static void	report_status (CNCT);
static void	restart_all_backups (int);
static void	restart_backup (DJB, int);
static void	send_reply (CNCT, JRNH *, USHORT);
static void	set_use_count (DJB);
static void	sign_off (CNCT, LTJC *);
static void	sign_on (CNCT, LTJC *);
static void	signal_child (void);
static void CLIB_ROUTINE signal_quit (void);
static DJB	start_database (CNCT, SCHAR *, SCHAR *, SSHORT, SLONG, ULONG);
static int	start_server (UCHAR *, USHORT, USHORT, USHORT);
static BOOLEAN	test_dup_entry (DJB, SLONG);
static void	time_stamp (GDS__QUAD *);

static TEXT	journal_directory [MAX_PATH_LENGTH];
static DJB	databases;
static CNCT	connections;
static FILE	*log;
static HANDLE	handles [2];
#ifdef APOLLO
static EVENT	semaphores [2];
#endif

#ifdef OS2_ONLY
static EVENT_T	semaphore [1];
#endif

#ifdef WIN_NT
static struct cnct	pending_connections [2];
static HANDLE	*wait_vec;
static USHORT	wait_vec_length;
#endif

static TEXT	*address_files [2] = { CONSOLE_ADDR, JOURNAL_ADDR };
static USHORT	state;
static SSHORT	archives_running = 0;
static SLONG	max_retry = MAX_RETRY;

#define slot_console		0
#define slot_journal		1

#define state_shut_pending	1	/* Shutdown has been requested */
#define state_suspend		4	/* Suspend operation */
#define state_initialize	8	/* Initializing operation */
#define state_reset		16	/* Reset after improper close */
#define state_status		32	/* Report status */

int SERVER_start_server (
    int		argc,
    SCHAR	**argv)
{
/**************************************
 *
 *	S E R V E R _ s t a r t _ s e r v e r
 *
 **************************************
 *
 * Functional description
 *	Parse switches and do work.
 *
 **************************************/
UCHAR	string [MAX_PATH_LENGTH], *journal;
USHORT	sw_i, sw_d, sw_v;

sw_i = sw_v = sw_d = FALSE;

if (argc <= 0)
    sw_i = TRUE;

/* Start by parsing switches */

journal = NULL;
argv++; 
while (--argc > 0)
    {
    if ((*argv) [0] != '-')
	{
	argv++;
	continue;
	}

    MISC_down_case (*argv++, string);
    switch (string [1])
	{
	case 'v':
	    sw_v = TRUE;
	    break;

	case 'd':
	    sw_d = TRUE;
	    break;

	case 'i':
	    sw_i = TRUE;
	    break;

	case 's':
	    break;

	case 'j':
	    if (--argc > 0)
		journal = *argv++;
	    else
		MISC_print_journal_syntax();
	    break;

	default :
	    MISC_print_journal_syntax();
	    break;
	}
    }

return start_server (journal, sw_d, sw_v, sw_i);
}  

static void add_backup_entry (
    DJB		dbb,
    SLONG	pid,
    SLONG	fid)
{
/**************************************
 *
 *	a d d _ b a c k u p _ e n t r y
 *
 **************************************
 *
 * Functional description
 *	Add an new entry to the backup array.
 *
 **************************************/
SSHORT	i;

for (i = 0; ((i < MAX_ARCHIVE) && (dbb->djb_bfi [i].bi_pid > 0)); i++)
    ;

if (i == MAX_ARCHIVE)
    {
    put_line (NULL_PTR, 104, 0, 0, 0);
    return;
    }

dbb->djb_bfi [i].bi_pid = pid;
dbb->djb_bfi [i].bi_fid = fid;
dbb->djb_bfi_count++;
}

static CNCT alloc_connect (
    HANDLE	handle,
    USHORT	channel)
{
/**************************************
 *
 *	a l l o c _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Accept an incoming connection.
 *
 **************************************/
CNCT	connection;

connection = (CNCT) MISC_alloc_jrnl (sizeof (struct cnct));
connection->cnct_next     = connections;
connection->cnct_channel  = channel;
connection->cnct_handle   = handle;
connection->cnct_database = NULL;
connections               = connection;

return connection;
}

static void backup_wal (
    DJB		database,
    SCHAR	*file_name,
    SLONG	file_id,
    SLONG	file_seq,
    SLONG	file_size,
    SLONG	p_offset)
{
/**************************************
 *
 *	b a c k u p _ w a l
 *
 **************************************
 *
 * Functional description
 *	Fork process to backup log file.
 *	Enter record in the LOG_ARCHIVE_STATUS table which will be deleted 
 *	by the backup process.
 *	Add entry to in-memory array for restart purposes.
 *
 **************************************/
SLONG		proc_id;
SCHAR		db_id [10], f_id [10], csize [10], cp_offset [10];
SLONG		*lts_tr;
SCHAR		name [MAX_PATH_LENGTH], dname [MAX_PATH_LENGTH];
TEXT		*argv [10];
#if (defined UNIX || defined APOLLO)
struct stat	stat_buf;
#endif

if (test_dup_entry (database, file_id))
    return;

sprintf (f_id, "%d", file_id);
sprintf (db_id, "%d", database->djb_id);
sprintf (csize, "%d", file_size);
sprintf (cp_offset, "%d", p_offset);
build_indexed_name (dname, database->djb_dir_name, file_seq);

gds__prefix (name, ARCHIVE);

#ifdef NETWARE_386
argv [1] = name;
argv [2] = db_id;
argv [3] = f_id;
argv [4] = cp_offset;
argv [5] = csize;
argv [6] = file_name;
argv [7] = dname;
argv [0] = 7;
gds__thread_start (main_archive, argv, 0, 0, NULL_PTR);
#else

#if (defined WIN_NT || defined OS2_ONLY)
proc_id = spawnl (P_NOWAITO, name, ARCHIVE, db_id, f_id, cp_offset, csize,
    file_name, dname, journal_directory, NULL);
#else
if (stat (name, &stat_buf) == -1)
    proc_id = -1;
else if (!(proc_id = vfork()))
    {
    /* child process */

    execl (name, name, db_id, f_id, cp_offset, csize, file_name, dname,
	journal_directory, NULL);
    _exit (FINI_OK);
    }
#endif

if (proc_id == -1 || !ISC_check_process_existence (proc_id, 0, FALSE))
    {
    put_line (NULL, 106, 0, 0, 0);
    return;
    }
#endif

lts_tr = (SLONG*) 0;
START_TRANSACTION lts_tr;

STORE (TRANSACTION_HANDLE lts_tr) X IN LOG_ARCHIVE_STATUS
    X.FILE_ID    = file_id;
    X.PROCESS_ID = proc_id;
END_STORE
ON_ERROR
    ROLLBACK lts_tr;
    put_line (NULL_PTR, 105, 0, 0, 0);
    return;
END_ERROR;

COMMIT lts_tr
    ON_ERROR
	ROLLBACK lts_tr
	    ON_ERROR
	    END_ERROR;
	put_line (NULL_PTR, 210, 0, 0, 0);
	return;
    END_ERROR;

/* Keep track of total number of archives running at any time */

archives_running++;

put_line (NULL_PTR, 107, (TEXT*) file_id, (TEXT*) file_name, 0);

/* If there is space in the array, add it to the array.  Otherwise return */

if (database->djb_bfi_count >= MAX_ARCHIVE)
    return;
add_backup_entry (database, proc_id, file_id);
}

static void build_backup_array (
    DJB	dbb)
{
/**************************************
 *
 *	b u i l d _ b a c k u p _ a r r a y
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

FOR X IN LOG_ARCHIVE_STATUS 
    if (X.PROCESS_ID > 0)
	{
	if (!ISC_check_process_existence ((SLONG) X.PROCESS_ID, 0, TRUE))
	    {
	    /* Process has died and/or another process has reused the
	       process id */

	    if (!test_dup_entry (dbb, X.FILE_ID))
		add_backup_entry (dbb, X.PROCESS_ID, X.FILE_ID);

	    ERASE X
	    ON_ERROR
		break;
	    END_ERROR;
	    }
	}
END_FOR;
}

static void build_indexed_name (
    SCHAR	*filename,
    SCHAR	*basename,
    SLONG	seqno)
{
/**************************************
 *
 *	b u i l d _ i n d e x e d _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a file name using a sequence number
 *	as the extension.  Try to keep the extension
 *	to three characters.
 *
 **************************************/	
static SCHAR	letters [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
SCHAR		extension [11];

if (seqno < 1000 || seqno > 18575)
    sprintf (extension, "%d", seqno);
else
    {
    seqno -= 1000;
    extension [0] = letters [seqno % 26];
    extension [1] = letters [seqno / 26 % 26];
    extension [2] = letters [seqno / 26 / 26];
    extension [3] = 0;
    }

sprintf (filename, "%s.%s", basename, extension);
}

#ifdef APOLLO_JOURNALLING
static void close_mailbox (
    int		slot)
{
/**************************************
 *
 *	c l o s e _ m a i l b o x	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Shut down connections in preparation of shutting down.
 *
 **************************************/
HANDLE		handle;
status_$t	status;

handle = handles [slot];
mbx_$close (handle, &status);

if (status.all)
    error (status, "mbx_$close");
}
#endif

#ifdef OS2_ONLY
static void close_mailbox (
    int		slot)
{
/**************************************
 *
 *	c l o s e _ m a i l b o x	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Shut down connections in preparation of shutting down.
 *
 **************************************/
ULONG	status;

if (status = DosClose (handles [slot]))
    error (status, "DosClose");
}
#endif

#ifdef BSD_SOCKETS
static void close_mailbox (
    int		slot)
{
/**************************************
 *
 *	c l o s e _ m a i l b o x	( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Shut down connections in preparation of shutting down.
 *
 **************************************/

unlink (address_files [slot]);

if (close (handles [slot]) < 0)
    error ((STATUS) errno, "close socket");
}
#endif

#ifdef WIN_NT
static void close_mailbox (
    int		slot)
{
/**************************************
 *
 *	c l o s e _ m a i l b o x	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Shut down connections in preparation of shutting down.
 *
 **************************************/

if (!CloseHandle (pending_connections [slot].cnct_handle) ||
    !CloseHandle (pending_connections [slot].cnct_overlapped.hEvent))
    error (GetLastError(), "CloseHandle");
}
#endif

static void commit (void)
{
/**************************************
 *
 *	c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit the current transaction and start another.
 *
 **************************************/

COMMIT;
START_TRANSACTION;
}

static void delete_backup_entry (
    DJB		dbb,
    SLONG	fid)
{
/**************************************
 *
 *	d e l e t e _ b a c k u p _ e n t r y
 *
 **************************************
 *
 * Functional description
 *	Delete an entry from log_transfer status and backup array.
 *
 **************************************/
SSHORT	i;

for (i = 0; i < MAX_ARCHIVE; i++)
    if (dbb->djb_bfi [i].bi_fid == fid)
	break;

if (i == MAX_ARCHIVE)
    return;

dbb->djb_bfi [i].bi_pid = -1;
dbb->djb_bfi [i].bi_fid = -1;
dbb->djb_bfi_count--;
}

static void delete_connection (
    CNCT	connection)
{
/**************************************
 *
 *	d e l e t e _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Closeout and delete a connection.  This may be either
 *	unilateral or bilateral.
 *
 **************************************/
DJB	database;
CNCT	*ptr;

/* If this is a console, log it */

if (connection->cnct_flags & CNCT_console)
    put_message (95, (DJB) NULL_PTR);
    
/* Disconnect from database */

if (database = connection->cnct_database)
    {
    if (!--database->djb_use_count)
	set_use_count (database);
    for (ptr = &database->djb_connections; *ptr; ptr = &(*ptr)->cnct_db_next)
	if (*ptr == connection)
	    {
	    *ptr = connection->cnct_db_next;
	    break;
	    }
    }

/* Shutdown physical connection */

disconnect (connection);

/* Unlink from system */

for (ptr = &connections;; ptr = &(*ptr)->cnct_next)
    if (!*ptr)
	{
	put_line (NULL_PTR, 108, 0, 0, 0);
	return;
	}
    else if (*ptr == connection)
	{
	*ptr = connection->cnct_next;
	break;
	}

MISC_free_jrnl (connection);

if ((database) &&
    (database->djb_flags & DJB_disabled) &&
    (database->djb_use_count == 0))
    delete_database (database);
}

static void delete_database (
    DJB		database)
{
/**************************************
 *
 *	d e l e t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	We've lost interest in a database.  Get rid of its block and
 *	all connections.
 *
 **************************************/
CNCT	connection;

/* mark the database for deletion */

database->djb_flags |= DJB_disabled;

/* Check if there is still work that needs to be done */

if ((database->djb_flags & DJB_archive) && (database->djb_bfi_count > 0))
    return;

if (database->djb_use_count > 1)
    return;

/* Start by getting rid all of all remaining connections */

while (connection = database->djb_connections)
    {
    put_message (96, connection->cnct_database);
    delete_connection (connection);
    }

free_database_entry (database);
}

static void delete_history_files (
    CNCT	connection,
    SCHAR	*dbname,
    SLONG	db_id,
    SLONG	last_dump)
{
/**************************************
 *
 *	d e l e t e _ h i s t o r y _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	We are interested only in the last completed online dump for 
 *	this database.  Get rid of older files.
 *
 **************************************/
SLONG	start_seqno;
int	ret;

commit();

start_seqno = 0;

FOR OD IN ONLINE_DUMP WITH OD.DUMP_ID EQ last_dump
    start_seqno = OD.START_SEQNO;
END_FOR;

if (!start_seqno)
    return;

/* for all completed online dumps with id < dump_id, unlink all
   archive files and online dump files. */

FOR OD IN ONLINE_DUMP WITH OD.DB_ID EQ db_id AND 
			OD.DUMP_ID LT last_dump AND 
			OD.END_SEQNO NE 0

    FOR ODF IN ONLINE_DUMP_FILES WITH ODF.DUMP_ID EQ OD.DUMP_ID
	put_line (connection, 145, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
	ret = unlink (ODF.DUMP_FILE_NAME);
	if ((ret == -1) && (errno != ENOENT))
	    {
	    put_line (connection, 147, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	    }
	ERASE ODF
	ON_ERROR
	    put_line (connection, 144, (TEXT*) dbname, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	END_ERROR;
    END_FOR;

    ERASE OD
    ON_ERROR
	put_line (connection, 144, (TEXT*) dbname, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	return;
    END_ERROR;

    FOR JF IN JOURNAL_FILES WITH JF.DB_ID EQ db_id AND
			    JF.FILE_SEQUENCE LT start_seqno AND
			    JF.ARCHIVE_NAME NOT MISSING
	put_line (connection, 145, (TEXT*) JF.ARCHIVE_NAME, 0, 0);
	ret = unlink (JF.ARCHIVE_NAME);
	if ((ret == -1) && (errno != ENOENT))
	    {
	    put_line (connection, 147, (TEXT*) JF.ARCHIVE_NAME, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	    }
	ERASE JF
	ON_ERROR
	    put_line (connection, 144, (TEXT*) dbname, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	END_ERROR;
    END_FOR;
END_FOR;

/* For all completed online dumps with dump id > last dump, remove the 
   entries from the database.  The online dump files are deleted
   the journal files are not.

   These online dumps started after before last_dump, but completed
   before last_dump. */

FOR OD IN ONLINE_DUMP WITH OD.DB_ID EQ db_id AND OD.DUMP_ID GT last_dump 
					AND OD.END_SEQNO NE 0
    FOR ODF IN ONLINE_DUMP_FILES WITH ODF.DUMP_ID EQ OD.DUMP_ID
	put_line (connection, 145, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
	ret = unlink (ODF.DUMP_FILE_NAME);
	if ((ret == -1) && (errno != ENOENT))
	    {
	    put_line (connection, 147, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	    }
	ERASE ODF
	ON_ERROR
	    put_line (connection, 144, (TEXT*) dbname, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	END_ERROR;
    END_FOR;

    ERASE OD
    ON_ERROR
	put_line (connection, 144, (TEXT*) dbname, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	return;
    END_ERROR;
END_FOR;

commit();
}

static void delete_wal (void)
{
/**************************************
 *
 *	d e l e t e _ w a l
 *
 **************************************
 *
 * Functional description
 *	Mark wal files for deletion.
 *
 **************************************/
DJB	database;
STATUS	status [20];

commit();

for (database = databases; database; database = database->djb_next)
    {
    if (!(database->djb_flags & DJB_archive))
	continue;

    /* Mark the file for deletion */

    FOR J IN JOURNAL_FILES WITH J.DB_ID EQ database->djb_id
			    AND J.DELETE_STATUS EQ LOG_NOT_DELETED
			    AND J.ARCHIVE_STATUS EQ ARCHIVED
	WALF_set_log_header_flag (status, database->djb_db_name, J.LOG_NAME, 
			J.PARTITION_OFFSET, WALFH_KEEP_FOR_LONG_TERM_RECV, 0);
	MODIFY J USING
	    strcpy (J.DELETE_STATUS, LOG_DELETED);
	END_MODIFY
	ON_ERROR
	    put_line (NULL_PTR, 118, 0, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    break;
	END_ERROR;
    END_FOR;

    commit();
    }
}

static void disable (
    CNCT	connection,
    LTJC	*msg)
{
/**************************************
 *
 *	d i s a b l e
 *
 **************************************
 *
 * Functional description
 *	Disable journalling for a database.
 *
 **************************************/
DJB	database;

database = connection->cnct_database;

/* Journal the message */

msg->ltjc_header.jrnh_handle = (SLONG) connection;
put_message (97, database);

/* Before disable, wal writer will rollover to a new file.
   The open of the new file will be recorded before the previous
   file is closed.  Get rid of the latest entry as it should not
   contain any valid records. */

FOR D IN DATABASES WITH D.DB_ID EQ database->djb_id
    MODIFY D USING
	strcpy (D.STATUS, DB_DISABLED);
    END_MODIFY
    ON_ERROR
	put_line (NULL_PTR, 109, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;
END_FOR;

commit();

delete_connection (connection);
delete_database (database);
}

#ifdef APOLLO_JOURNALLING
static void disconnect (
    CNCT	connection)
{
/**************************************
 *
 *	d i s c o n n e c t	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Break physical connection to database or whatever.
 *
 **************************************/
status_$t	status;
DJB		database;
SSHORT		channel;
mbx_$msg_hdr_t	message, *msg;

/* Shut down connection */

message.chan = connection->cnct_channel;
message.cnt = sizeof (message);
message.mt = mbx_$eof_mt;
msg = &message;
mbx_$put_rec (connection->cnct_handle, msg, (SLONG) sizeof (message), &status);

channel = connection->cnct_channel;
mbx_$deallocate (connection->cnct_handle, channel, &status);

if (status.all)
    error_exit (status, "mbx_$deallocate");
}
#endif

#ifdef OS2_ONLY
#ifndef NETWARE_JOURNALLING
static void disconnect (
    CNCT	connection)
{
/**************************************
 *
 *	d i s c o n n e c t	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Break physical connection to database or whatever.
 *
 **************************************/
ULONG	status;

if (status = DosClose (connection->cnct_handle))
    error (status, "DosClose");
}
#endif
#endif
       
#ifdef BSD_SOCKETS
static void disconnect (
    CNCT	connection)
{
/**************************************
 *
 *	d i s c o n n e c t	( U N I X  &  N E T W A R E)
 *
 **************************************
 *
 * Functional description
 *	Break physical connection to database or whatever.
 *
 **************************************/

if (close (connection->cnct_handle) < 0)
    error ((STATUS) errno, "close connection");
}
#endif

#ifdef WIN_NT
static void disconnect (
    CNCT	connection)
{
/**************************************
 *
 *	d i s c o n n e c t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Break physical connection to database or whatever.
 *
 **************************************/

if (!FlushFileBuffers (connection->cnct_handle))
    error (GetLastError(), "FlushFileBuffers");
if (!DisconnectNamedPipe (connection->cnct_handle))
    error (GetLastError(), "DisconnectNamedPipe");
if (!CloseHandle (connection->cnct_handle) ||
    !CloseHandle (connection->cnct_overlapped.hEvent))
    error (GetLastError(), "CloseHandle");
}
#endif

#ifndef VMS
#ifndef OS2_ONLY
#ifndef WIN_NT
#ifndef NETWARE_386
static void divorce_terminal (
    fd_set	*mask)
{
/**************************************
 *
 *	d i v o r c e _ t e r m i n a l
 *
 **************************************
 *
 * Functional description
 *	Clean up everything in preparation to become an independent
 *	process.  Close all files except for marked by the input mask.
 *
 **************************************/
int	s, fid;

/* Close all files other than those explicitly requested to stay open */

for (fid = 0; fid < BITS; fid++)
    if (!mask || !(TEST_BIT (fid, mask)))
	s = close (fid);

/* Perform terminal divorce */

fid = open ("/dev/tty", 2);

if (fid >= 0)
    {
#ifdef TIOCNOTTY
    s = ioctl (fid, TIOCNOTTY, 0);
#endif
    s = close (fid);
    }

/* Finally, get out of the process group */

s = setpgrp (0, 0);
}
#endif
#endif
#endif
#endif

static void enable (
    CNCT	connection,
    LTJC	*msg)
{
/**************************************
 *
 *	e n a b l e
 *
 **************************************
 *
 * Functional description
 *	Enable journalling for a database.
 *
 **************************************/
DJB	database;
JRNR	reply;
SCHAR	db_name[MAX_PATH_LENGTH], dir_name[MAX_PATH_LENGTH];
SCHAR	arch_name [MAX_PATH_LENGTH];
SLONG	fd;

MISC_get_wal_info (msg, db_name, dir_name);
connection->cnct_version = msg->ltjc_header.jrnh_version;

/* Check validity of archive directory */

if ((dir_name) && strlen (dir_name))
    {
    build_indexed_name (arch_name, dir_name, msg->ltjc_seqno);
    if (LLIO_open (NULL_PTR, arch_name, LLIO_OPEN_NEW_RW, FALSE, &fd))
	{
	reply.jrnr_response = jrnr_archive_error;
	send_reply (connection, (JRNH*) &reply, sizeof (reply));
	delete_connection (connection);
	return;
	}

    LLIO_close (NULL_PTR, fd);

    /* get rid of file created */

    unlink (arch_name);
    }

/* If database already exists, wipe out old connections and start again */

if (database = lookup_database (db_name, 0, 0))
    {
    database->djb_use_count++;
    delete_database (database);
    }

/* Make up database block */

start_database (connection, db_name, dir_name, 
		msg->ltjc_page_size, msg->ltjc_seqno, msg->ltjc_offset);
reply.jrnr_response = jrnr_enabled;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

#ifdef APOLLO_JOURNALLING
static void error (
    status_$t	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT	buffer [MAX_PATH_LENGTH];
SSHORT	l;

put_line (NULL_PTR, GEN_ERR, (TEXT*) string, 0, 0);
error_$print (status);
}
#endif

#ifdef OS2_ONLY
static void error (
    STATUS	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT	buffer [MAX_PATH_LENGTH];
SLONG	l;

put_line (NULL_PTR, GEN_ERR, (TEXT*) string, 0, 0);

if (DosGetMessage (0, 0, buffer, sizeof (buffer) - 1, status, "OSO001.MSG", &l))
    put_line (NULL_PTR, 173, (TEXT*) status, 0, 0);
else
    {
    buffer [l] = 0;
    put_line (NULL_PTR, 174, (TEXT*) buffer, 0, 0);
    }
}
#endif

#ifdef BSD_SOCKETS
static void error (
    STATUS	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r	( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/

put_line (NULL_PTR, GEN_ERR, (TEXT*) string, 0, 0);
perror (string);
}
#endif

#ifdef WIN_NT
static void error (
    STATUS	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT	buffer [MAX_PATH_LENGTH];
SSHORT	l;

put_line (NULL_PTR, GEN_ERR, (TEXT*) string, 0, 0);

if (!(l = FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	status,
	GetUserDefaultLangID(),
	buffer,
	sizeof (buffer),
	NULL)))
    put_line (NULL_PTR, 211, (TEXT*) status, 0, 0);
else
    put_line (NULL_PTR, 212, (TEXT*) buffer, 0, 0);
}
#endif

static void error_exit (
    STATUS	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r _ e x i t
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/

error (status, string);
exit (FINI_ERROR);
}

static void free_database_entry (
    DJB		database)
{
/**************************************
 *
 *      f r e e _ d a t a b a s e _ e n t r y
 *
 **************************************
 *
 * Functional description
 *      Remove database block from list and free up memory
 *
 **************************************/
DJB	*ptr;

/* Disconnect block from data structures */

for (ptr = &databases; *ptr; ptr = &(*ptr)->djb_next)
    if (*ptr == database)
	{
	*ptr = database->djb_next;
	break;
	}

if (database->djb_dir_name)
    MISC_free_jrnl (database->djb_dir_name);
MISC_free_jrnl (database);
}

#ifdef APOLLO_JOURNALLING
static void get_message (
    JRNH	*buffer,
    int		length)
{
/**************************************
 *
 *	g e t _ m e s s a g e		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Get message if one is waiting.
 *
 **************************************/
USHORT		n, count;
UCHAR		*mbx_ptr;
CNCT		connection;
HANDLE		handle;
SLONG		retlen, values [2];
status_$t	status;
mbx_$server_msg_t	*header, *retptr;
stream_$sk_t		seek_key;

for (;;)
    {
    count = (!(state & state_suspend)) ? 2 : 1;
    for (n = 0; n < count; n++)
	{
	values [n] = ISC_event_clear (semaphores [n]);
	handle = handles [n];
	mbx_$get_conditional (handle, buffer, length, &retptr, &retlen, &status);
	if (!status.all)
	    break;
	handle = NULL;
	}
    if (handle)
	break;
    ISC_event_wait (count, semaphores, values, 0, NULL_PTR, NULL_PTR);
    }

header = retptr;

for (connection = connections; connection; connection = connection->cnct_next)
    if (connection->cnct_handle == handle && connection->cnct_channel == header->chan)
	break;

switch (header->mt)
    {
    case mbx_$data_mt:
	process_message (connection, header->data, header->cnt - 6);
	break;
    
    case mbx_$channel_open_mt:
	if (state & state_shut_pending)
	    {
	    header->mt = mbx_$reject_open_mt;
	    mbx_$put_rec (handle, header, (SLONG) 6, &status);
	    break;
	    }
	header->mt = mbx_$accept_open_mt;
	mbx_$put_rec (handle, header, (SLONG) 6, &status);
	if (!status.all)
	    {
	    connection = alloc_connect (handle, header->chan);
	    if (n == 0)
		connection->cnct_flags = CNCT_console;
	    }
	break;
    
    case mbx_$eof_mt:
	if (connection)
	    delete_connection (connection);
	else
	    mbx_$deallocate (handle, header->chan, &status);
	break;

    default:
	put_line (NULL_PTR, 175, (TEXT*) header->mt, 0, 0);
    }
}
#endif

#ifdef OS2_ONLY
static void get_message (
    JRNH	*buffer,
    int		length)
{
/**************************************
 *
 *	g e t _ m e s s a g e		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Get message if one is waiting.
 *
 **************************************/
ULONG	n, active, l1;
CNCT	connection;
ULONG	status;
SLONG	value;
EVENT	ptr;

active = 1;

for (;;)
    {
    value = ISC_event_clear (semaphore);
    if (!DosConnectNPipe (handles [slot_console]))
    	{
	connection = alloc_connect (handles [slot_console], 0);
	connection->cnct_flags |= CNCT_console;
	open_mailbox (CONSOLE_FILE, 0);
	put_message (146, (DJB) NULL_PTR);
	}
    if (!DosConnectNPipe (handles [slot_journal]))
    	{
	connection = alloc_connect (handles [slot_journal], 0);
	open_mailbox (JOURNAL_FILE, 1);
	}
    for (connection = connections; connection; 
	 connection = connection->cnct_next)	
	if (active || (connection->cnct_flags & CNCT_console))
	    {
 	    status = DosRead (connection->cnct_handle, buffer, length, &l1);
	    if (!status)
		break;
	    }
    if (connection)
    	break;
    ptr = semaphore;
    ISC_event_wait (1, &ptr, &value, 0, NULL_PTR, NULL_PTR);
    }
    
if (!l1)
    {
    delete_connection (connection);
    return;
    }

if (status)
    error_exit (status, "DosRead");

process_message (connection, buffer, l1);
}
#endif

#ifdef BSD_SOCKETS
static void get_message (
    JRNH	*buffer,
    int		length)
{
/**************************************
 *
 *	g e t _ m e s s a g e		( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *      Poll sockets and connections.  If we get a 'select', read one
 *      and return later for the rest.
 *      Connections has the linked list of all FDs created by 'accept', 
 *      but those of original sockets.
 *
 **************************************/
SLONG	len, nfd, ready_cnt;
CNCT	connection;
HANDLE	handle;
fd_set	ready;
UCHAR	*p;
       
handle = 0;

init_handles (&ready);

/* set timeout to NULL_PTR to wait until we get something */
/* want more than 32 later */

for (;;)
    {
#ifdef UNIX_JOURNALLING
    ready_cnt = select (32, &ready, NULL_PTR, NULL_PTR, NULL_PTR);
#else
    ready_cnt = select (FD_SETSIZE, &ready, NULL_PTR, NULL_PTR, NULL_PTR);
#endif
    if (ready_cnt > 0)
	break;
    if (ready_cnt < 0 && errno != EINTR)
	error ((STATUS) errno, "select");     
    }

if (ready_cnt)
    {
    nfd = 0;
    for (connection = connections; connection; connection = connection->cnct_next)
	{
	handle = connection->cnct_handle;
	if (TEST_BIT (handle, &ready))
	    {
	    nfd = handle;
	    break;
	    }
	}

    /* if the select wasn't on a connection, check both sockets */

    if (!nfd)
	{
	if (TEST_BIT (handles [slot_console], &ready))
	    handle = handles [slot_console];
	else if (TEST_BIT (handles [slot_journal], &ready))
	    handle = handles [slot_journal];
	if (handle)
	    {
	    nfd = accept (handle, NULL_PTR, NULL);
	    if (nfd < 0)
		error ((STATUS) errno, "accept");
	    connection = alloc_connect (nfd, 0);
	    if (handle == handles [slot_console])
		{
		connection->cnct_flags = CNCT_console;    
		put_message (146, (DJB) NULL_PTR);
		}
	    }
	}

    len = recv (nfd, (SCHAR*) buffer, sizeof (JRNH), 0);
    p = (UCHAR*) buffer + len;
    length = buffer->jrnh_length - len;
    while (len > 0 && length)
	{
	len = recv (nfd, p, length, 0);
	length -= len;
	p += len;
	}
    if (len < 0)
	error ((STATUS) errno, "recv");
    else if (!len)
	{
	if (!(connection->cnct_flags & CNCT_console))
	    put_message (96, connection->cnct_database);
	delete_connection (connection); 
	}
    else
	process_message (connection, buffer, buffer->jrnh_length);
    }
   
}
#endif

#ifdef WIN_NT
static void get_message (
    JRNH	*buffer,
    int		length)
{
/**************************************
 *
 *	g e t _ m e s s a g e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Get message if one is waiting.
 *
 **************************************/
CNCT	connection, new_connection;
HANDLE	*wait_events;
SLONG	wait_count, n, len;
UCHAR	*p;

wait_events = wait_vec;
*wait_events++ = pending_connections [slot_console].cnct_overlapped.hEvent;
*wait_events++ = pending_connections [slot_journal].cnct_overlapped.hEvent;
for (connection = connections; connection; connection = connection->cnct_next)	
    *wait_events++ = connection->cnct_overlapped.hEvent;

wait_count = wait_events - wait_vec;
if ((n = WaitForMultipleObjects (wait_count, wait_vec, FALSE, INFINITE)) < 0)
    error (GetLastError(), "WaitForMultipleObjects");

if (n < 2)
    connection = (n == 0) ? &pending_connections [slot_console] :
	&pending_connections [slot_journal];
else
    for (connection = connections; n != 2; n--)
	connection = connection->cnct_next;

if (connection->cnct_flags & CNCT_pending_io)
    {
    if (!GetOverlappedResult (connection->cnct_handle,
	&connection->cnct_overlapped, &len, TRUE) &&
	GetLastError() != ERROR_BROKEN_PIPE)
	error (GetLastError(), "GetOverlappedResult");
    connection->cnct_read_length = len;
    connection->cnct_flags &= ~CNCT_pending_io;
    }

if (n < 2)
    {
    new_connection = alloc_connect (connection->cnct_handle, 0);
    new_connection->cnct_overlapped = connection->cnct_overlapped;
    if (connection == &pending_connections [slot_console])
	{
	new_connection->cnct_flags |= CNCT_console;
	put_message (146, (DJB) NULL_PTR);
	open_mailbox (CONSOLE_FILE, slot_console);
	}
    else
	open_mailbox (JOURNAL_FILE, slot_journal);
    connection = new_connection;
    read_asynchronous (connection);
    return;
    }

if (!connection->cnct_read_length)
    {
    if (!(connection->cnct_flags & CNCT_console))
	put_message (96, connection->cnct_database);
    delete_connection (connection);
    return;
    }

memcpy (buffer, connection->cnct_data, (int) connection->cnct_read_length);
p = (UCHAR*) buffer + connection->cnct_read_length;
length = buffer->jrnh_length - connection->cnct_read_length;
if (!ReadFile (connection->cnct_handle, p, length, &len,
    &connection->cnct_overlapped))
    {
    if (GetLastError() != ERROR_IO_PENDING)
	error (GetLastError(), "ReadFile");
     else if (!GetOverlappedResult (connection->cnct_handle,
	&connection->cnct_overlapped, &len, TRUE))
	error (GetLastError(), "GetOverlappedResult");
    }
if (length != len)
    error (GetLastError(), "ReadFile");

read_asynchronous (connection);

process_message (connection, buffer, buffer->jrnh_length);
}
#endif

#ifdef BSD_SOCKETS
static void init_handles (
    fd_set	*mask_ptr)
{
/**************************************
 *
 *	i n i t _ h a n d l e s     ( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Get sockets ready for select.
 *
 **************************************/
CNCT	connection;

memset (mask_ptr, 0, sizeof (*mask_ptr));

/* set mask for each accepted connections */

for (connection = connections; connection; connection = connection->cnct_next)
    SET_BIT (connection->cnct_handle, mask_ptr);

/* now for the sockets */

SET_BIT (handles [slot_console], mask_ptr);
SET_BIT (handles [slot_journal], mask_ptr);
}
#endif

static DJB lookup_database (
    SCHAR	*string,
    USHORT	id,
    SSHORT	find_disabled)
{
/**************************************
 *
 *	l o o k u p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Find the database if it is active.
 *	if find_deleted flag is true, it will find disabled databases also.
 *
 **************************************/
DJB	database;
UCHAR	*q;

if (string)
    for (database = databases; database; database = database->djb_next)
	{
	if ((database->djb_flags & DJB_disabled) && (!find_disabled))
	    continue;

	q = database->djb_db_name;
	if (!strcmp (q, string))
	    return database;
	}

if (id)
    for (database = databases; database; database = database->djb_next)
	{
	if ((database->djb_flags & DJB_disabled) && (!find_disabled))
	    continue;

	if (database->djb_id == id)
	    return database;
	}

return NULL;
}

static DJB make_database (
    CNCT	connection,
    SCHAR	*db_name,
    SCHAR	*dir_name)
{
/**************************************
 *
 *	m a k e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Make up a database block.
 *
 **************************************/
DJB	database;
SSHORT	i, ldb, ldir;

ldb = strlen (db_name);
ldir = strlen (dir_name);

database = (DJB) MISC_alloc_jrnl (sizeof (struct djb) + ldb);

if (ldb)
    strcpy (database->djb_db_name, db_name);

if (ldir)
    {
    database->djb_dir_name = MISC_alloc_jrnl (ldir+1);
    strcpy (database->djb_dir_name, dir_name);
    }

database->djb_next = databases;
databases = database;

for (i = 0; i <= MAX_ARCHIVE; i++)
    {
    database->djb_bfi [i].bi_pid = -1;
    database->djb_bfi [i].bi_fid = -1;
    }

if (connection)
    {
    connection->cnct_db_next = database->djb_connections;
    database->djb_connections = connection;
    database->djb_use_count = 1;
    connection->cnct_database = database;
    }

return database;
}

#ifdef APOLLO_JOURNALLING
static void open_mailbox (
    SCHAR	*filename,
    USHORT	slot)
{
/**************************************
 *
 *	o p e n _ m a i l b o x		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Open multiplexed connection.
 *
 **************************************/
SSHORT		l;
HANDLE		*handle;
status_$t	status;
SCHAR		full_name [MAX_PATH_LENGTH];

strcpy (full_name, journal_directory);
strcat (full_name, filename);

l = strlen (full_name);

/* Open a mailbox to get things going */

mbx_$create_server (full_name, l, (SSHORT) (MAX_RECORD + 6),
	MAX_CHANNEL, &handle, &status);

if (status.all)
    error_exit (status, "mbx_$create_server");

mbx_$get_ec (handle, 
	mbx_$getrec_ec_key,
	&(ec2_$ptr_t) semaphores [slot], 
	&status);

if (status.all)
    error_exit (status, "mbx_$get_ec");

handles [slot] = (HANDLE) handle;
}
#endif

#ifdef OS2_ONLY
static void open_mailbox (
    SCHAR	*filename,
    USHORT	slot)
{
/**************************************
 *
 *	o p e n _ m a i l b o x		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Open multiplexed connection.
 *
 **************************************/
TEXT	full_name [MAX_PATH_LENGTH];
ULONG	status;

strcpy (full_name, "\\PIPE\\");
strcat (full_name, journal_directory);
strcat (full_name, filename);

if (status = DosCreateNPipe (full_name, &handles [slot],
	NP_NOWRITEBEHIND | NP_ACCESS_DUPLEX,
	NP_NOWAIT | NP_WMESG | NP_RMESG | NP_UNLIMITED_INSTANCES,
	sizeof (JRNH) + MSG_LENGTH,
	MAX_RECORD + 6,
	0))
    error_exit (status, "DosCreateNPipe");
	
if (!semaphore [0].event_handle)
    if (status = DosCreateEventSem ("\\SEM32\\ISCJRN", &semaphore [0].event_handle, 0, 0))
    	error_exit (status, "DosCreateEventSem");
    
if (status = DosSetNPipeSem (handles [slot], (HSEM) semaphore [0].event_handle, slot))
    error_exit (status, "DosSetNPipeSem");
}
#endif

#ifdef BSD_SOCKETS
static void open_mailbox (
    SCHAR	*filename,
    USHORT	slot)
{
/**************************************
 *
 *	o p e n _ m a i l b o x		( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Open multiplexed connection.
 *
 **************************************/
struct sockaddr_in	address;
HANDLE			handle;
struct hostent		*host;
TEXT			name[MAXHOSTNAMELEN];
int			length, namelen;

/* Set up Inter-Net socket address */

namelen = MAXHOSTNAMELEN;

if ((gethostname (name, namelen) == -1))
    error_exit ((STATUS) errno, "gethostname");    

if (!(host = gethostbyname (name)))
    error_exit ((STATUS) errno, "gethostbyname");

/* Allocate a port block and initialize a socket for communications */

handle = (SLONG) socket (AF_INET, SOCK_STREAM, 0);

if (handle == -1)
    error_exit ((STATUS) errno, "socket");

memset (&address, 0, sizeof (address));
memcpy (&address.sin_addr, host->h_addr, sizeof (address.sin_addr));

#ifdef UNIX_JOURNALLING
address.sin_family = host->h_addrtype;
#else
address.sin_family = AF_INET;
#endif

address.sin_port = 0; /* let system choose an unused port no. */
length = sizeof (address);

if (bind ((int) handle, (struct sockaddr*) &address, length) < 0)
    error_exit ((STATUS) errno, "bind");

if (getsockname ((int) handle, (struct sockaddr*) &address, &length) < 0)
    error_exit ((STATUS) errno, "getsockname");

/* now that we've settled on the socket, go advertize the address */

post_address (slot, address);

/* current system limit for queue = 5 */

if (listen (handle, 5) < 0)
    error_exit ((STATUS) errno, "listen");

handles [slot] = handle;
}
#endif

#ifdef WIN_NT
static void open_mailbox (
    SCHAR	*filename,
    USHORT	slot)
{
/**************************************
 *
 *	o p e n _ m a i l b o x		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Open multiplexed connection.
 *
 **************************************/
TEXT	name [MAX_PATH_LENGTH], *p, *q, c, full_name [MAX_PATH_LENGTH];
HANDLE	phandle, ehandle;
USHORT	n;
CNCT	cnct;

strcpy (full_name, journal_directory);
strcat (full_name, filename);
filename = full_name;

p = name;
strcpy (p, "\\\\.\\pipe");
p += strlen (p);

if (*filename != '/' && *filename != '\\')
    *p++ = '\\';

while (*filename)
    *p++ = ((c = *filename++) == ':') ? '_' : ((c == '/') ? '\\' : c);
*p = 0;

memset (&pending_connections [slot], 0, sizeof (struct cnct));
phandle = CreateNamedPipe (name,
	PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
	PIPE_WAIT | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
	PIPE_UNLIMITED_INSTANCES,
	sizeof (JRNH) + MSG_LENGTH,
	MAX_RECORD + 6,
	0,
	ISC_get_security_desc());
if (phandle == INVALID_HANDLE_VALUE)
    error_exit (GetLastError(), "CreateNamedPipe");

pending_connections [slot].cnct_handle = phandle;

ehandle = CreateEvent (NULL, TRUE, FALSE, NULL);
if (ehandle == NULL)
    error_exit (GetLastError(), "CreateEvent");

pending_connections [slot].cnct_overlapped.hEvent = ehandle;

if (ConnectNamedPipe (phandle, &pending_connections [slot].cnct_overlapped) ||
    GetLastError() == ERROR_PIPE_CONNECTED)
    SetEvent (ehandle);
else if (GetLastError() == ERROR_IO_PENDING)
    pending_connections [slot].cnct_flags |= CNCT_pending_io;
else
    error_exit (GetLastError(), "ConnectNamedPipe");

for (n = 2, cnct = connections; cnct; cnct = cnct->cnct_next)
    n++;

if (n > wait_vec_length)
    {
    if (wait_vec)
	MISC_free_jrnl (wait_vec);
    wait_vec_length += 10;
    wait_vec = (HANDLE*) MISC_alloc_jrnl ((SLONG) (wait_vec_length * sizeof (HANDLE)));
    }
}
#endif

#ifdef BSD_SOCKETS
static void post_address (
    USHORT		slot,
    struct sockaddr_in	address)
{
/**************************************
 *
 *	p o s t _ a d d r e s s		( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Write out socket info. to a known file so that any
 *      client can find it.
 *
 **************************************/
FILE	*file;
SCHAR	full_name [MAX_PATH_LENGTH];

strcpy (full_name, journal_directory);
strcat (full_name, address_files [slot]);

if (!(file = fopen (full_name, "w")))
    error_exit ((STATUS) errno, "socket address file open");

fprintf (file, "%d %d %d %d", 
	JOURNAL_VERSION, 
	address.sin_addr.s_addr,
	address.sin_family,
	address.sin_port);

fclose (file);
}
#endif

static void process_command (
    CNCT	connection,
    TEXT	*string)
{
/**************************************
 *
 *	p r o c e s s _ c o m m a n d
 *
 **************************************
 *
 * Functional description
 *	Process an operator command.
 *
 **************************************/
CMDS	*command;
TEXT	operand1 [JOURNAL_PATH_LENGTH], operand2 [JOURNAL_PATH_LENGTH];
TEXT	cmd [32], *p, *q;
USHORT	n;
JRNR	response;
SSHORT	db_id;

response.jrnr_header.jrnh_type = JRN_RESPONSE;
n = sscanf (string, "%s%s%s", cmd, operand1, operand2);

for (command = commands; p = command->cmds_string; command++)
    {
    q = cmd;
    while (*q && UPPER (*q) == *p++)
	q++;
    if (!*q)
	break;
    }

switch (command->cmds_command)
    {
    case cmd_shutdown:
	put_message (98, (DJB) NULL_PTR);
	state |= state_shut_pending;
	response.jrnr_response = jrnr_shutdown;
	send_reply (connection, (JRNH*) &response, sizeof (response));
#ifdef APOLLO_JOURNALLING	
	sleep (5);
#endif
#ifdef OS2_ONLY	
	DosSleep (5000);
#endif
#ifdef WIN_NT	
	Sleep (5000);
#endif
	delete_connection (connection);
	return;
    
    case cmd_exit:
	response.jrnr_response = jrnr_exit;
	send_reply (connection, (JRNH*) &response, sizeof (response));
	return;
    
    case cmd_status:
	report_status (connection);
	break;

    case cmd_help:
	put_line (connection, 112, 0, 0, 0);
	for (command = commands; p = command->cmds_string; command++)
	    if (command->cmds_msg)
		put_line (connection, command->cmds_msg, (TEXT*) command->cmds_string, 0, 0);
	break;

    case cmd_version:
	p = GDS_VERSION;
	put_line (connection, 113, p, 0, 0);
	break;

    case cmd_drop:
	put_line (connection, 114, (TEXT*) operand1, 0, 0);
	process_drop_database (connection, operand1);
	break;

    case cmd_erase:
	db_id = atoi (operand1);
	put_line (connection, 162, (TEXT*) db_id, 0, 0);
	process_erase_database (connection, db_id);
	break;

    case cmd_reset:
	put_line (connection, 151, (TEXT*) operand2, 0, 0);
	process_reset_database (connection, operand2);
	break;

    case cmd_set:
	put_line (connection, 115, (TEXT*) operand2, 0, 0);
	process_set_database (connection, operand2);
	break;

    case cmd_disable:
	put_line (connection, 159, (TEXT*) operand1, 0, 0);
	process_disable_database (connection, operand1);
	break;

    case cmd_archive:
	put_line (connection, 240, (TEXT*) operand1, 0, 0);
	process_restart_archive (connection, operand1);
	break;

    default:
	put_line (connection, 116, (TEXT*) string, 0, 0);
    }

response.jrnr_response = jrnr_end_msg;
send_reply (connection, (JRNH*) &response, sizeof (response));
}

static void process_archive_begin (
    CNCT	connection,
    LTJA	*msg)
{
/**************************************
 *
 *	p r o c e s s _ a r c h i v e _ b e g i n
 *
 **************************************
 *
 * Functional description
 *	Process archive start.
 *
 **************************************/
DJB	database;
SLONG	db_id, file_id;
JRNR	reply;

db_id   = msg->ltja_db_id;
file_id = msg->ltja_file_id;

connection->cnct_version = msg->ltja_header.jrnh_version;

if (database = lookup_database ((SCHAR*) 0, db_id, 1))
    {
    put_message (100, database);
    connection->cnct_db_next = database->djb_connections;
    database->djb_connections = connection;
    connection->cnct_database = database;
    if (!database->djb_use_count++)
	set_use_count (database);
    reply.jrnr_response = jrnr_accepted;
    send_reply (connection, (JRNH*) &reply, sizeof (reply));
    return;
    }

/* Check if the database has been disabled already.  If so let the
 * archive run.
 */

FOR D IN DATABASES WITH D.DB_ID EQ db_id AND D.STATUS EQ DB_DISABLED
    reply.jrnr_response = jrnr_accepted;
    send_reply (connection, (JRNH*) &reply, sizeof (reply));
    connection->cnct_database = (DJB) 0;
    return;
END_FOR;

/* Error situation */

reply.jrnr_response = jrnr_rejected;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

static void process_archive_end (
    CNCT	connection,
    LTJA	*msg)
{
/**************************************
 *
 *	p r o c e s s _ a r c h i v e _ e n d
 *
 **************************************
 *
 * Functional description
 *	process archive end
 *
 **************************************/
DJB	database;
SLONG	file_id, db_id;
SCHAR	name [MAX_PATH_LENGTH], file_name [MAX_PATH_LENGTH];
STATUS	status [20];
SCHAR	db [MAX_PATH_LENGTH], arch [MAX_PATH_LENGTH];

db_id   = msg->ltja_db_id;
file_id = msg->ltja_file_id;

commit();

database = connection->cnct_database;

/* Handle situation where the database is already disabled */

if (database)
    {
    strcpy (db, database->djb_db_name);
    strcpy (arch, database->djb_dir_name);
    database->djb_retry = 0; 	/* archive is working */
    }
else
    {
    FOR D IN DATABASES WITH D.DB_ID EQ db_id
	strcpy (db, D.DB_NAME);
	strcpy (arch, D.ARCHIVE_BASE_NAME);
    END_FOR;
    }

FOR J IN JOURNAL_FILES WITH J.FILE_ID EQ file_id
    WALF_set_log_header_flag (status, db, J.LOG_NAME, 
			J.PARTITION_OFFSET, WALFH_KEEP_FOR_LONG_TERM_RECV, 0);
    strcpy (file_name, J.LOG_NAME);
    build_indexed_name (name, arch, J.FILE_SEQUENCE);
    MODIFY J USING
	strcpy (J.DELETE_STATUS, LOG_DELETED);
	strcpy (J.ARCHIVE_STATUS, ARCHIVED);
	strcpy (J.ARCHIVE_NAME, name);
    END_MODIFY
    ON_ERROR
	put_line (NULL_PTR, 126, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;
END_FOR;

FOR T IN LOG_ARCHIVE_STATUS WITH T.FILE_ID = file_id
    ERASE T
	ON_ERROR
	    ROLLBACK;
	    START_TRANSACTION;
	    break;
	END_ERROR;
END_FOR;

if (database)
    delete_backup_entry (database, file_id);

commit();

put_message (99, database);

put_line (NULL_PTR, 178, file_name, (TEXT*) file_id, 0);

delete_connection (connection);
archives_running--;
}

static void process_archive_error (
    CNCT	connection,
    LTJA	*msg)
{
/**************************************
 *
 *	p r o c e s s _ a r c h i v e _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	process archive error
 *
 **************************************/
DJB	database;
SLONG	file_id;

file_id = msg->ltja_file_id;

database = connection->cnct_database;

put_line (NULL_PTR, 179, (TEXT*) file_id, 0, 0);
put_line (NULL_PTR, (SSHORT)msg->ltja_error_code, 0, 0, 0);

put_message (99, database);

delete_connection (connection);

commit();

FOR T IN LOG_ARCHIVE_STATUS WITH T.FILE_ID = file_id
    ERASE T
	ON_ERROR
	    ROLLBACK;
	    START_TRANSACTION;
	    break;
	END_ERROR;
END_FOR;

commit();

archives_running--;

/* If database is already disabled */

if (!database)
    return;

delete_backup_entry (database, file_id);
database->djb_retry++;

if (database->djb_retry > max_retry)
    return;

/* Restart backup. */

FOR J IN JOURNAL_FILES WITH J.FILE_ID EQ msg->ltja_file_id
    backup_wal (database, J.LOG_NAME, J.FILE_ID, J.FILE_SEQUENCE, J.FILE_SIZE,
		J.PARTITION_OFFSET);
END_FOR;
}

static void process_control_point (
    CNCT	connection,
    LTJW	*msg)
{
/**************************************
 *
 *	p r o c e s s _ c o n t r o l _ p o i n t
 *
 **************************************
 *
 * Functional description
 *	Update control point information.
 *
 **************************************/
DJB	database;
JRNR	reply;

database = connection->cnct_database;

put_line (NULL_PTR, 117, (TEXT*) msg->ltjw_seqno, (TEXT*) msg->ltjw_offset, 0);

commit();

reply.jrnr_response = jrnr_ack;
send_reply (connection, (JRNH*) &reply, sizeof (reply));

/* 
 * Restart backup.
 * Go through WAL file list for database and fire off backup process. 
 */

if (database->djb_flags & DJB_archive)
    restart_backup (database, 1);

/* Mark the file for deletion */

delete_wal();

commit();
}

static void process_disable_database (
    CNCT	connection,
    TEXT	*dbname)
{
/**************************************
 *
 *	p r o c e s s _ d i s a b l e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Disable journalling for a database.
 *
 **************************************/
DJB	database;

commit();

if (!(database = lookup_database (dbname, 0, 0)))
    {
    put_line (connection, 143, (TEXT*) dbname, 0, 0);
    return;
    }

if (database->djb_use_count)
    {
    put_line (connection, 161, (TEXT*) database->djb_id, 0, 0);
    return;
    }

/* show our interest in the database */
database->djb_use_count++;

put_message (97, database);

/* 
 * Before disable, wal writer will rollover to a new file.
 * The open of the new file will be recorded before the previous
 * file is closed.  Get rid of the latest entry as it should not
 * contain any valid records.
 */

FOR D IN DATABASES WITH D.DB_ID EQ database->djb_id
    MODIFY D USING
	strcpy (D.STATUS, DB_DISABLED);
    END_MODIFY
    ON_ERROR
	put_line (NULL_PTR, 109, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;
END_FOR;

commit();

delete_database (database);
}

static void process_drop_database (
    CNCT	connection,
    SCHAR	*dbname)
{
/**************************************
 *
 *	p r o c e s s _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *
 *	Drop all online dump and archived files for database.
 *	Delete all records for database.
 **************************************/
DJB	database;
SSHORT	db_id;

commit();

if (!(database = lookup_database (dbname, 0, 0)))
    {
    put_line (connection, 143, (TEXT*) dbname, 0, 0);
    return;
    }

db_id = database->djb_id;

if (database->djb_use_count)
    {
    put_line (connection, 161, (TEXT*) db_id, 0, 0);
    return;
    }

process_erase_database (connection, db_id);
}

static void process_dump_file (
    CNCT	connection,
    LTJW	*msg)
{
/**************************************
 *
 *	p r o c e s s _ d u m p _ f i l e
 *
 **************************************
 *
 * Functional description
 *
 *	Store dump file info in online_dump_file.
 *	Dump file sequence is ltjw_count.
 *	Dump id is ltjw_mode.
 **************************************/
JRNR	reply;

msg->ltjw_data[msg->ltjw_length] = 0;

STORE X IN ONLINE_DUMP_FILES
    strcpy (X.DUMP_FILE_NAME, msg->ltjw_data);
    X.FILE_SEQNO = msg->ltjw_count;
    X.DUMP_ID 	 = msg->ltjw_mode;
    X.FILE_SIZE  = msg->ltjw_offset;
    time_stamp (&X.CREATION_DATE);
END_STORE
ON_ERROR
    put_line (NULL_PTR, 119, 0, 0, 0);
    ROLLBACK;
    START_TRANSACTION;
END_ERROR;

commit();

put_line (NULL_PTR, 120, (TEXT*) msg->ltjw_mode, (TEXT*) msg->ltjw_data, 0);

reply.jrnr_response = jrnr_ack;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

static void process_end_dump (
    CNCT	connection,
    LTJW	*msg)
{
/**************************************
 *
 *	p r o c e s s _ e n d _ d u m p
 *
 **************************************
 *
 * Functional description
 *	update end of dump record
 *	The dump id comes in overloaded in the ltjw_mode field.
 *	Time stamp the end of dump.
 *
 **************************************/
JRNR	reply;

/* Trigger will update DATABASE.LAST_DUMP_ID */

FOR X IN ONLINE_DUMP WITH X.DUMP_ID = msg->ltjw_mode
    MODIFY X USING
	X.END_SEQNO    = msg->ltjw_seqno;
	X.END_OFFSET   = msg->ltjw_offset;
	X.END_PARTITION_OFFSET = msg->ltjw_p_offset;
	time_stamp (&X.DUMP_DATE);
    END_MODIFY
    ON_ERROR
	put_line (NULL_PTR, 121, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;
END_FOR;

commit();

put_line (NULL_PTR, 122, (TEXT*) msg->ltjw_mode, 0, 0);

reply.jrnr_response = jrnr_end_dump;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

static void process_erase_database (
    CNCT	connection,
    USHORT	db_id)
{
/**************************************
 *
 *	p r o c e s s _ e r a s e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *
 *	Drop all online dump and archived files for database.
 *	Delete all records for database.
 **************************************/
DJB	database;
int	ret;
SCHAR	dbname [MAX_PATH_LENGTH];
int	found = 0;

commit();

if (!(database = lookup_database ((SCHAR*) 0, db_id, 0)))
    {
    FOR D IN DATABASES WITH D.DB_ID EQ db_id
	strcpy (dbname, D.DB_NAME);
	found = 1;
	break;
    END_FOR;

    if (!found)
	{
	put_line (connection, 177, (TEXT*) db_id, 0, 0);
	return;
	}
    }
else
    {
    strcpy (dbname, database->djb_db_name);

    if (database->djb_use_count)
	{
	put_line (connection, 161, (TEXT*) db_id, 0, 0);
	return;
	}

    /* show our interest in the database */

    database->djb_use_count++;

    delete_database (database);
    }

/* erase all online dump files and all archived log files */

FOR D IN DATABASES WITH D.DB_ID EQ db_id

    FOR OD IN ONLINE_DUMP WITH OD.DB_ID EQ D.DB_ID

	FOR ODF IN ONLINE_DUMP_FILES WITH ODF.DUMP_ID EQ OD.DUMP_ID
	    put_line (connection, 145, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
	    ret = unlink (ODF.DUMP_FILE_NAME);
	    if ((ret == -1) && (errno != ENOENT))
		{
		put_line (connection, 147, (TEXT*) ODF.DUMP_FILE_NAME, 0, 0);
		ROLLBACK;
		START_TRANSACTION;
		return;
		}
	    ERASE ODF
	    ON_ERROR
		put_line (connection, 144, (TEXT*) dbname, 0, 0);
		ROLLBACK;
		START_TRANSACTION;
		return;
	    END_ERROR;
	END_FOR;

	ERASE OD
	ON_ERROR
	    put_line (connection, 144, (TEXT*) dbname, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	END_ERROR;

    END_FOR;

    FOR JF IN JOURNAL_FILES WITH JF.DB_ID = D.DB_ID
	
	if (strlen (JF.ARCHIVE_NAME))
	    {
	    put_line (connection, 145, (TEXT*) JF.ARCHIVE_NAME, 0, 0);
	    ret = unlink (JF.ARCHIVE_NAME);
	    if ((ret == -1) && (errno != ENOENT))
		{
		put_line (connection, 147, (TEXT*) JF.ARCHIVE_NAME, 0, 0);
		ROLLBACK;
		START_TRANSACTION;
		return;
		}
	    }
	ERASE JF
	ON_ERROR
	    put_line (connection, 144, (TEXT*) dbname, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    return;
	END_ERROR;
    END_FOR;
    
    ERASE D
    ON_ERROR
	put_line (connection, 144, (TEXT*) dbname, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	return;
    END_ERROR;
END_FOR;

commit();
}

static void process_message (
    CNCT	connection,
    JRNH	*msg,
    USHORT	length)
{
/**************************************
 *
 *	p r o c e s s _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Process a journal message.
 *
 **************************************/
TEXT	*p;

switch (msg->jrnh_type)
    {
    case JRN_ENABLE:
	enable (connection, (LTJC*) msg);
	break;
    
    case JRN_DISABLE:
	disable (connection, (LTJC*) msg);
	break;
    
    case JRN_SIGN_ON:
	sign_on (connection, (LTJC*) msg);
	break;
    
    case JRN_SIGN_OFF:
	sign_off (connection, (LTJC*) msg);
	break;
    
    case JRN_WAL_NAME:
	process_wal_file (connection, (LTJW*) msg);
	break;

    case JRN_CONSOLE:
	p = (TEXT*) msg;
	p [length] = 0;
	process_command (connection, (TEXT*) (p + sizeof (JRNH)));
	break;

    case JRN_START_ONLINE_DMP:
	process_start_dump (connection, (LTJW*) msg);
	break;

    case JRN_END_ONLINE_DMP:
	process_end_dump (connection, (LTJW*) msg);
	break;

    case JRN_ONLINE_DMP_FILE:
	process_dump_file (connection, (LTJW*) msg);
	break;

    case JRN_ARCHIVE_BEGIN:
	process_archive_begin (connection, (LTJA*) msg);
	break;

    case JRN_ARCHIVE_END:
	process_archive_end (connection, (LTJA*) msg);
	break;

    case JRN_ARCHIVE_ERROR:
	process_archive_error (connection, (LTJA*) msg);
	break;

    case JRN_CNTRL_PT:
	process_control_point (connection, (LTJW*) msg);
	break;

    default:
	put_line (NULL_PTR, 123, (TEXT*) msg->jrnh_type, 0, 0);
    }
}

static void process_reset_database (
    CNCT	connection,
    SCHAR	*dbname)
{
/**************************************
 *
 *	p r o c e s s _ r e s e t _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Reset delete flag for database.
 *
 **************************************/
DJB	database;
USHORT	db_id;

commit();

if (!(database = lookup_database (dbname, 0, 0)))
    {
    put_line (connection, 143, (TEXT*) dbname, 0, 0);
    return;
    }
else
    {
    db_id = database->djb_id;
    }

/* reset delete flag */

FOR D IN DATABASES WITH D.DB_ID EQ db_id
    MODIFY D USING
	strcpy (D.DELETE_MODE, DB_RETAIN);
    END_MODIFY
    ON_ERROR
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;

    database->djb_flags &= ~DJB_set_delete;
END_FOR;

commit();
}

static void process_restart_archive (
    CNCT	connection,
    SCHAR	*dbname)
{
/**************************************
 *
 *	p r o c e s s _ r e s t a r t _ a r c h i v e
 *
 **************************************
 *
 * Functional description
 *	Reset archive for a database.
 *
 **************************************/
DJB	database;
USHORT	db_id;

commit();

if (!(database = lookup_database (dbname, 0, 0)))
    {
    put_line (connection, 143, (TEXT*) dbname, 0, 0);
    return;
    }
else
    {
    db_id = database->djb_id;
    }

database->djb_retry = 0; 	/* set archive to be working */

/* Restart backup. */

restart_backup (database, MAX_ARCHIVE);

commit();
}

static void process_set_database (
    CNCT	connection,
    SCHAR	*dbname)
{
/**************************************
 *
 *	p r o c e s s _ s e t _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Set delete flag for database.
 *	Delete all 'older' files.
 *
 **************************************/
DJB	database;
SLONG	last_dump, db_id;

commit();

if (!(database = lookup_database (dbname, 0, 0)))
    {
    put_line (connection, 143, (TEXT*) dbname, 0, 0);
    return;
    }
else
    {
    db_id = database->djb_id;
    }

/* set delete flag */

FOR D IN DATABASES WITH D.DB_ID EQ db_id
    MODIFY D USING
	strcpy (D.DELETE_MODE, DB_DELETE);
    END_MODIFY
    ON_ERROR
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;

    last_dump = D.LAST_DUMP_ID;
    database->djb_flags |= DJB_set_delete;
END_FOR;

commit();

delete_history_files (connection, dbname, db_id, last_dump);
}

static void process_start_dump (
    CNCT	connection,
    LTJW	*msg)
{
/**************************************
 *
 *	p r o c e s s _ s t a r t _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Store a record in the online_dump table.
 *	Return the dump_id overloaded on the page number field of reply.
 **************************************/
JRNR	reply;
SSHORT	dump_id = -1;

msg->ltjw_data[msg->ltjw_length] = 0;

FOR D IN DATABASES WITH D.DB_ID EQ connection->cnct_database->djb_id
    STORE X IN ONLINE_DUMP
	X.DB_ID		= D.DB_ID;
	X.START_SEQNO 	= msg->ltjw_seqno;
	X.START_OFFSET 	= msg->ltjw_offset;
	X.START_PARTITION_OFFSET = msg->ltjw_p_offset;
	X.END_SEQNO    	= 0;
	X.END_OFFSET   	= 0;
	X.END_PARTITION_OFFSET = 0;
    END_STORE
    ON_ERROR
	put_line (NULL_PTR, 124, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	goto end_proc;
    END_ERROR;
END_FOR;

commit();

/* Get the newly generated dump_id */

FOR X IN ONLINE_DUMP WITH X.DB_ID EQ connection->cnct_database->djb_id
		AND X.START_SEQNO EQ msg->ltjw_seqno
		AND X.START_OFFSET EQ msg->ltjw_offset

    dump_id = X.DUMP_ID;

END_FOR;

put_line (NULL_PTR, 125, (TEXT*) dump_id, 0, 0);

end_proc:

reply.jrnr_response = jrnr_start_dump;
reply.jrnr_page     = (SLONG) dump_id;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

static void process_wal_file (
    CNCT	connection,
    LTJW	*msg)
{
/**************************************
 *
 *	p r o c e s s _ w a l _ f i l e
 *
 **************************************
 *
 * Functional description
 *	start storing wal file information
 *
 **************************************/
DJB	database;
JRNR	reply;
int	repeat;
SLONG	file_id;
SLONG	last_dump;
SLONG	file_seq, file_size, p_offset;

msg->ltjw_data[msg->ltjw_length] = 0;
database = connection->cnct_database;

FOR D IN DATABASES WITH D.DB_ID EQ connection->cnct_database->djb_id

    last_dump = D.LAST_DUMP_ID;
    repeat = FALSE;

    FOR X IN JOURNAL_FILES WITH X.DB_ID EQ D.DB_ID AND 
			X.FILE_SEQUENCE EQ msg->ltjw_seqno
	repeat = TRUE;
    END_FOR;

    /* Handle close file record for files without open */

    if (repeat == FALSE)
	{
	STORE X IN JOURNAL_FILES
	    X.DB_ID	= D.DB_ID;
	    X.PARTITION_OFFSET = msg->ltjw_p_offset;
	    strcpy (X.FILE_STATUS, LOG_OPENED);
	    strcpy (X.DELETE_STATUS, LOG_NOT_DELETED);
	    strcpy (X.ARCHIVE_STATUS, NOT_ARCHIVED);
	    X.FILE_SEQUENCE  = msg->ltjw_seqno;
	    strcpy (X.LOG_NAME, msg->ltjw_data);
	    MOVE_CLEAR (X.ARCHIVE_NAME, sizeof (X.ARCHIVE_NAME));
	    time_stamp (&X.ARCHIVE_DATE);
	    X.PARTITION_OFFSET = msg->ltjw_p_offset;
	    X.FILE_SIZE = msg->ltjw_offset;
	END_STORE
	ON_ERROR
	    put_line (NULL_PTR, 126, 0, 0, 0);
	    ROLLBACK;
	    START_TRANSACTION;
	    break;
	END_ERROR;
	}

    if (msg->ltjw_mode != JRNW_OPEN)
	{
	FOR X IN JOURNAL_FILES WITH X.FILE_STATUS EQ LOG_OPENED AND 
				X.DB_ID EQ D.DB_ID AND
				X.FILE_SEQUENCE EQ msg->ltjw_seqno
	    file_id = X.FILE_ID;
	    MODIFY X USING
		X.FILE_SIZE = msg->ltjw_offset;
		strcpy (X.FILE_STATUS, LOG_CLOSED);
	    END_MODIFY
		ON_ERROR
		    put_line (NULL_PTR, 126, 0, 0, 0);
		    ROLLBACK;
		    START_TRANSACTION;
		    break;
		END_ERROR
	END_FOR;
	}
END_FOR;

if (msg->ltjw_mode == JRNW_OPEN) 
    put_line (NULL_PTR, 127, (TEXT*) msg->ltjw_data, 0, 0);
else
    put_line (NULL_PTR, 128, (TEXT*) msg->ltjw_data, 0, 0);

commit();

reply.jrnr_response = jrnr_ack;
send_reply (connection, (JRNH*) &reply, sizeof (reply));

/* restart failed backups */

if (database->djb_flags & DJB_archive)
    restart_backup (database, 1);

commit();

if (database->djb_flags & DJB_archive)
    {
    if (msg->ltjw_mode == JRNW_CLOSE)
	{
	FOR X IN JOURNAL_FILES WITH X.FILE_ID EQ file_id
	    file_size = X.FILE_SIZE;
	    p_offset = X.PARTITION_OFFSET;
	    file_seq = X.FILE_SEQUENCE;
	    MODIFY X USING
		strcpy (X.ARCHIVE_STATUS, ARCHIVE_PENDING);
	    END_MODIFY
	    ON_ERROR
		ROLLBACK;
		START_TRANSACTION;
		break;
	    END_ERROR;

	END_FOR;

	commit();

	backup_wal (database, msg->ltjw_data, file_id, file_seq, file_size, p_offset);
	}
    }

/* Mark files for deletion */

delete_wal();

commit();

/* if database is configured for deletion of 'older' files, do so now */

if (database->djb_flags & DJB_set_delete)
    delete_history_files ((CNCT) NULL, database->djb_db_name, 
			  database->djb_id, last_dump);
}

static void put_line (
    CNCT	connection,
    SSHORT	msg,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3)
{
/**************************************
 *
 *	p u t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Print a line of text to the log or to
 *	a given connection.
 *
 **************************************/
TEXT	buffer [MAX_PATH_LENGTH], *p;
JRNR	*response;
TEXT	line [MSG_LENGTH];

GJRN_get_msg (msg, line, arg1, arg2, arg3);

if (!connection)
    {
    if (!log)
	return;
    fprintf (log, line);
    fprintf (log, "\n");
    fflush (log);
    return;
    }

response = (JRNR*) buffer;
response->jrnr_header.jrnh_type = JRN_RESPONSE;
response->jrnr_response = jrnr_msg;
p = buffer + sizeof (JRNR);
sprintf (p, line, arg1, arg2, arg3);
response->jrnr_page = strlen (p);
send_reply (connection, (JRNH*) buffer, (USHORT)(sizeof (JRNR) + response->jrnr_page));
}

static void put_message (
    SSHORT	msg_num,
    DJB		database)
{
/**************************************
 *
 *	p u t _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Write out informational message to the log.
 *
 **************************************/
SLONG		clock;
struct tm	times;
UCHAR		message [MSG_LENGTH];

if (!log)
    return;

GJRN_get_msg (msg_num, message, NULL, NULL, NULL);

clock = time (NULL);
#ifdef NETWARE_JOURNALLING
times = *localtime ((time_t*) &clock);
#else
times = *localtime (&clock);
#endif

fprintf (log, "%.2d:%.2d:%.2d %.2d/%.2d/%.2d %s",
    times.tm_hour, times.tm_min, times.tm_sec,
    times.tm_mon + 1, times.tm_mday, times.tm_year,
    message);

if (database)
    fprintf (log, " for \"%s\" (%d)\n", database->djb_db_name, database->djb_id);
else
    fprintf (log, "\n");

fflush (log);
}

#ifdef WIN_NT
static void read_asynchronous (
    CNCT	connection)
{
/**************************************
 *
 *	r e a d _ a s y n c h r o n o u s
 *
 **************************************
 *
 * Functional description
 *	Start an asynchronous read on a named pipe.
 *	If it completes immediately, save the length.
 *	Otherwise, set a pending flag.
 *
 **************************************/
SLONG	len;

if (ReadFile (connection->cnct_handle, connection->cnct_data,
	sizeof (connection->cnct_data), &len, &connection->cnct_overlapped))
    {
    connection->cnct_read_length = len;
    SetEvent (connection->cnct_overlapped.hEvent);
    }
else if (GetLastError() == ERROR_IO_PENDING)
    connection->cnct_flags |= CNCT_pending_io;
else if (GetLastError() == ERROR_BROKEN_PIPE)
    {
    connection->cnct_read_length = 0;
    SetEvent (connection->cnct_overlapped.hEvent);
    }
else
    error (GetLastError(), "ReadFile");
}
#endif

static void report_status (
    CNCT	connection)
{
/**************************************
 *
 *	r e p o r t _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Report current state of journalling sub-system.
 *
 **************************************/
DJB	database;
SSHORT	count;

/* Report on current state */

commit();

put_line (connection, 130, 0, 0, 0);

count = 0;

for (database = databases; database; database = database->djb_next)
    {
    if (database->djb_flags & DJB_disabled)
	continue;
    count++;
    }

if (!count)
    {
    put_line (connection, 133, 0, 0, 0);
    return;
    }

put_line (connection, 131, 0, 0, 0);
for (database = databases; database; database = database->djb_next)
    {
    if (database->djb_flags & DJB_disabled)
	continue;

    put_line (connection, 132,
	database->djb_db_name, (TEXT*) database->djb_id, 
	(TEXT*) database->djb_use_count);
    }
}

static void restart_all_backups (
    int	count)
{
/**************************************
 *
 *	r e s t a r t _ a l l _ b a c k u p s
 *
 **************************************
 *
 * Functional description
 *	Restart backup processes for all databases.
 *
 **************************************/
DJB	database;

if (!count)
    return;

/* restart back up of files which were marked pending, but never started */

FOR J IN JOURNAL_FILES WITH J.ARCHIVE_STATUS EQ ARCHIVE_PENDING

    if (!(database = lookup_database ((SCHAR*) 0, J.DB_ID, 1)))
	continue;

    if (database->djb_retry > max_retry)
	continue;

    backup_wal (database, J.LOG_NAME, J.FILE_ID, J.FILE_SEQUENCE, 
		J.FILE_SIZE, J.PARTITION_OFFSET);
    if (!--count)
	return;
END_FOR;
}

static void restart_backup (
    DJB	database,
    int	count)
{
/**************************************
 *
 *	r e s t a r t _ b a c k u p
 *
 **************************************
 *
 * Functional description
 *	Restart backup processes for a database.
 *
 **************************************/
int	ret;
SSHORT	i;
SLONG	fid;

if (!database)
    {
    restart_all_backups (count);
    return;
    }

if (!(database->djb_flags & DJB_archive))
    return;
 
for (i = 0; ((i < MAX_ARCHIVE) && (count)) ; i++)
    {
    if (database->djb_bfi [i].bi_pid <= 0)
	continue;

    if (!ISC_check_process_existence (database->djb_bfi [i].bi_pid, 0, TRUE))
	{
	/* Process is not running and/or another process has reused the
	   process id */

	fid = database->djb_bfi [i].bi_fid;

	database->djb_bfi [i].bi_pid = -1;
	database->djb_bfi [i].bi_fid = -1;
	database->djb_bfi_count--;
	FOR J IN JOURNAL_FILES WITH J.FILE_ID EQ fid AND
					J.ARCHIVE_STATUS EQ ARCHIVE_PENDING
	    backup_wal (database, J.LOG_NAME, J.FILE_ID, J.FILE_SEQUENCE,
					J.FILE_SIZE, J.PARTITION_OFFSET);
	    count--;
	END_FOR;

	}
    }

/* restart back up of files which were marked pending, but never started */

restart_all_backups (count);
}

#ifdef APOLLO_JOURNALLING
static void send_reply (
    CNCT	connection,
    JRNH	*msg,
    USHORT	length)
{
/**************************************
 *
 *	s e n d _ r e p l y	( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Send a reply to a database process.
 *
 **************************************/
SLONG			l;
status_$t		status;
mbx_$server_msg_t	message;

msg->jrnh_handle = (SLONG) connection;
msg->jrnh_version = connection->cnct_version;

message.cnt = l = mbx_$serv_msg_hdr_len + length;
message.mt = mbx_$data_mt;
message.chan = connection->cnct_channel;
memcpy (message.data, msg, length);

mbx_$put_rec (connection->cnct_handle, &message, l, &status);

if (status.all)
    {
    GJRN_get_msg (GEN_ERR, msg, "mbx_$put_rec", NULL);    /* Error occurred during \"%s\" */
    error (status, "%s\n", msg);
    }
}
#endif

#ifdef OS2_ONLY
static void send_reply (
    CNCT	connection,
    JRNH	*msg,
    USHORT	length)
{
/**************************************
 *
 *	s e n d _ r e p l y	( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Send a reply to a database process.
 *
 **************************************/
ULONG	l, status;

msg->jrnh_handle = (SLONG) connection;
msg->jrnh_version = connection->cnct_version;

/* length is used by the journal server for reading
   one message at a time from the socket connections */

msg->jrnh_length = length;

if ((status = DosWrite (connection->cnct_handle, msg, length, &l)) ||
    length != l)
    error (status, "DosWrite");
}
#endif

#ifdef BSD_SOCKETS
static void send_reply (
    CNCT	connection,
    JRNH	*msg,
    USHORT	length)
{
/**************************************
 *
 *	s e n d _ r e p l y	( U N I X  &  N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Send a reply to a database process.
 *
 **************************************/

msg->jrnh_handle = (SLONG) connection;
msg->jrnh_version = connection->cnct_version;

/* length is used by the journal server for reading
   one message at a time from the socket connections */

msg->jrnh_length = length;

if (send (connection->cnct_handle, (SCHAR*) msg, length, 0) < 0)
    error ((STATUS) errno, "send journal");
}
#endif

#ifdef WIN_NT
static void send_reply (
    CNCT	connection,
    JRNH	*msg,
    USHORT	length)
{
/**************************************
 *
 *	s e n d _ r e p l y	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Send a reply to a database process.
 *
 **************************************/
SLONG	l;

msg->jrnh_handle = (SLONG) connection;
msg->jrnh_version = connection->cnct_version;

/* length is used by the journal server for reading
   one message at a time from the socket connections */

msg->jrnh_length = length;

if (!WriteFile ((HANDLE) connection->cnct_handle, msg, length, &l, NULL) ||
    length != l)
    error (GetLastError(), "WriteFile");
}
#endif

static void set_use_count (
    DJB		database)
{
/**************************************
 *
 *	s e t _ u s e _ c o u n t
 *
 **************************************
 *
 * Functional description
 *	Set "use_count" for database.  This is called only for significant
 *	use count transitions.
 *
 **************************************/

FOR D IN DATABASES WITH 
	D.STATUS EQ DB_ACTIVE AND D.DB_NAME EQ database->djb_db_name
    MODIFY D USING
	D.USE_COUNT = database->djb_use_count;
    END_MODIFY;
END_FOR;

commit();
}

static void sign_off (
    CNCT	connection,
    LTJC	*msg)
{
/**************************************
 *
 *	s i g n _ o f f
 *
 **************************************
 *
 * Functional description
 *	Sign off a database.
 *
 **************************************/
DJB	database;

if (database = connection->cnct_database)
    put_message (99, database);

delete_connection (connection);
}

static void sign_on (
    CNCT	connection,
    LTJC	*msg)
{
/**************************************
 *
 *	s i g n _ o n
 *
 **************************************
 *
 * Functional description
 *	Sign a database process onto the journal system.
 *
 **************************************/
DJB	database;
JRNR	reply;
SCHAR	db_name[MAX_PATH_LENGTH], dir_name[MAX_PATH_LENGTH];

MISC_get_wal_info (msg, db_name, dir_name);

connection->cnct_version = msg->ltjc_header.jrnh_version;

/* If we already know about the database, this is easy */

if (database = lookup_database (db_name, 0, 0))
    {
    put_message (100, database);
    connection->cnct_db_next = database->djb_connections;
    database->djb_connections = connection;
    connection->cnct_database = database;
    if (!database->djb_use_count++)
	set_use_count (database);
    reply.jrnr_response = jrnr_accepted;
    send_reply (connection, (JRNH*) &reply, sizeof (reply));
    return;
    }

/* Make up database block */

start_database (connection, db_name, dir_name, 
		msg->ltjc_page_size, msg->ltjc_seqno, msg->ltjc_offset);
reply.jrnr_response = jrnr_enabled;
send_reply (connection, (JRNH*) &reply, sizeof (reply));
}

#if (defined UNIX_JOURNALLING || defined APOLLO_JOURNALLING)
static void signal_child (void)
{
/**************************************
 *
 *      s i g n a l _ c h i l d
 *
 **************************************
 *
 * Functional description
 *      Do not do anything.  SUN is leaving <defunct> processes
 *      with an ignore.
 *
 **************************************/
int	location;

wait (&location);
}
#endif

static void CLIB_ROUTINE signal_quit (void)
{
/**************************************
 *
 *	s i g n a l _ q u i t
 *
 **************************************
 *
 * Functional description
 *	Stop whatever we happened to be doing.
 *
 **************************************/

state |= state_shut_pending;

#ifdef SIGQUIT
signal (SIGQUIT, NULL);
#endif

signal (SIGINT, NULL);
}

static DJB start_database (
    CNCT	connection,
    SCHAR	*db_name,
    SCHAR	*dir_name,
    SSHORT	page_size,
    SLONG	seqno,
    ULONG	offset)
{
/**************************************
 *
 *	s t a r t _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Start journaling database.
 *
 **************************************/
DJB	database;

database = make_database (connection, db_name, dir_name);

/* Start by marking any other databases of same name as inactive */

FOR D IN DATABASES WITH D.DB_NAME EQ database->djb_db_name
    MODIFY D USING
	strcpy (D.STATUS, DB_INACTIVE);
    END_MODIFY
    ON_ERROR
	put_line (NULL_PTR, 134, 0, 0, 0);
	ROLLBACK;
	START_TRANSACTION;
	break;
    END_ERROR;
END_FOR;

/* Store new database record */

STORE X IN DATABASES
    strcpy (X.DB_NAME, database->djb_db_name);
    if (database->djb_dir_name)
	strcpy (X.ARCHIVE_BASE_NAME, database->djb_dir_name);
    else
	MOVE_CLEAR (X.ARCHIVE_BASE_NAME, sizeof (X.ARCHIVE_BASE_NAME));
    time_stamp (&X.DATE_ENABLED);
    strcpy (X.STATUS, DB_ACTIVE);

    strcpy (X.DEVICE_TYPE, MEDIA_DISK);
    X.USE_COUNT = 1;
    X.PAGE_SIZE	= page_size;
    X.ENABLE_SEQNO = seqno;
    X.ENABLE_OFFSET = offset;
    strcpy (X.DELETE_MODE, DB_RETAIN);

    if (database->djb_dir_name)
	{
	database->djb_flags |= DJB_archive;
	strcpy (X.ARCHIVE_MODE, NEED_ARCH);
	}
    else
	strcpy (X.ARCHIVE_MODE, NO_ARCH);
END_STORE
ON_ERROR
    put_line (NULL_PTR, 135, 0, 0, 0);
    ROLLBACK;
    START_TRANSACTION;
END_ERROR;

/* Get the database id which the trigger put in */

FOR X IN DATABASES WITH X.DB_NAME EQ database->djb_db_name AND
			X.STATUS EQ DB_ACTIVE
    database->djb_id = X.DB_ID;
END_FOR;

put_message (101, database);

commit();

return database;
}

static int start_server (
    UCHAR	*journal_dir,
    USHORT	sw_d,
    USHORT	sw_v,
    USHORT	sw_i)
{
/**************************************
 *
 *	s t a r t _ s e r v e r
 *
 **************************************
 *
 * Functional description
 *	Open server mailbox, ready server database, and wait for something
 *	to happen.
 *
 **************************************/
DJB	database;
USHORT	sw_debug, len;
TEXT	logfile [MAX_PATH_LENGTH], journal_db [MAX_PATH_LENGTH];
SLONG	message_buffer [(MAX_RECORD + 6 + sizeof (SLONG) - 1) / sizeof (SLONG)];
STATUS	status [20];
SSHORT	msg_count;

#ifdef SIGQUIT
signal (SIGQUIT, signal_quit);
#endif
signal (SIGINT, signal_quit);

state = state_status;
connections = NULL;
sw_debug = sw_d;

/* Expand the journal directory name and insure that it ends with a slash. */

if (!journal_dir)
    journal_dir = ".";
len = ISC_expand_filename (journal_dir, 0, journal_directory);
#ifndef WIN_NT
if (journal_directory [len-1] != '/')
#else
if (journal_directory [len-1] != '/' && journal_directory [len-1] != '\\')
#endif
    {
    journal_directory [len++] = '/';
    journal_directory [len] = 0;
    }

if (CONSOLE_test_server (journal_directory))
    GJRN_abort (186);

if (sw_debug)
    log = stdout;
else
    {
#ifndef VMS
#ifndef NETWARE_386
#ifndef OS2_ONLY
#ifndef WIN_NT
    /* close message file before calling divorce terminal as we do not
       know the file id of the message file and will end up closing
       all files.  However the static variable which holds the file
       descriptor still has a value. */

    gds__msg_close (NULL_PTR);
    divorce_terminal (NULL_PTR);
#endif
#endif
#endif
#endif
    strcpy (logfile, journal_directory);
    strcpy (logfile + len, LOGFILE);
    log = fopen (logfile, "a");
    }

put_message (102, (DJB) NULL);

open_mailbox (CONSOLE_FILE, slot_console);
open_mailbox (JOURNAL_FILE, slot_journal);

#if (defined UNIX_JOURNALLING || defined APOLLO_JOURNALLING)
/* Ignore end of child process for UNIX and APOLLO only */
/* SUN leaves behind <defunct> processes if SIG_IGN is used */

ISC_signal (SIGCLD, signal_child, NULL_PTR);
#endif

/* Check in with database */

strcpy (journal_db, journal_directory);
strcpy (journal_db + len - 1, "/journal.gdb");

READY
    ON_ERROR
    put_line (NULL_PTR, 152, 0, 0, 0);
    GJRN_abort (152);
    END_ERROR;

START_TRANSACTION;
state |= state_initialize;

FOR X IN JOURNAL
    state &= ~state_initialize;
    if (X.USE_COUNT)
	{
	put_line (NULL_PTR, 136, 0, 0, 0);
	FOR D IN DATABASES WITH D.STATUS EQ DB_ACTIVE AND D.USE_COUNT NE 0
	    put_line (NULL_PTR, 137, (TEXT*) D.DB_NAME, (TEXT*) D.DB_ID, 0);
	    MODIFY D USING
		D.USE_COUNT = 0;
	    END_MODIFY;
	END_FOR;
	state |= state_reset;
	}
    if (X.MAX_RETRY)
	max_retry = X.MAX_RETRY;

    MODIFY X USING
	X.USE_COUNT = 1;
	if (!X.MAX_RETRY)
	    X.MAX_RETRY = max_retry;
    END_MODIFY;
END_FOR;

if (state & state_initialize)
    STORE X IN JOURNAL
	strcpy (X.STATUS, DB_ACTIVE);
	X.USE_COUNT = 1;
	time_stamp (&X.DATE_INIT);
    END_STORE;

/* Initialize list of valid databases */

FOR D IN DATABASES WITH D.STATUS EQ DB_ACTIVE
    database = make_database ((CNCT) NULL, D.DB_NAME, D.ARCHIVE_BASE_NAME);

    database->djb_id  = D.DB_ID;

    if (strcmp (D.DELETE_MODE, DB_RETAIN))
	database->djb_flags |= DJB_set_delete;

    if (strcmp (D.ARCHIVE_MODE, NO_ARCH))
	{
	database->djb_flags |= DJB_archive;
	build_backup_array (database);

	restart_backup (database, MAX_ARCHIVE);

	FOR J IN JOURNAL_FILES WITH J.DB_ID EQ D.DB_ID 
				AND J.FILE_STATUS EQ LOG_CLOSED
				AND J.ARCHIVE_STATUS EQ NOT_ARCHIVED
				AND J.DELETE_STATUS EQ LOG_NOT_DELETED
	    MODIFY J USING
		strcpy (J.ARCHIVE_STATUS, ARCHIVE_PENDING);
	    END_MODIFY
	    ON_ERROR
		put_line (NULL_PTR, 138, 0, 0, 0);
		ROLLBACK;
		START_TRANSACTION;
		break;
	    END_ERROR;
	    SAVE;
	    backup_wal (database, J.LOG_NAME, J.FILE_ID, J.FILE_SEQUENCE, J.FILE_SIZE, J.PARTITION_OFFSET);
	END_FOR;
	}

END_FOR;

/* Mark the file for deletion */

delete_wal();

commit();

/* Sit in loop waiting for either command input or message from client */

msg_count = 0;
for (;;)
    {
    if (state & state_shut_pending) 
	{
	if (!(connections || archives_running))
	    break;
	}
    if (state & state_status)
	{
	state &= ~state_status;
	report_status ((CNCT) NULL_PTR);
	}
    commit();
    get_message ((JRNH*) message_buffer, sizeof (message_buffer));

    /* restart archives every max_retry messages */

    if (msg_count++ > max_retry)
	{
	msg_count = 0;
	restart_all_backups (max_retry);
	}
    commit();
    }

/* 
 * Fork off processes to back up WAL files which
 * have'nt been backed up yet.
 */

for (database = databases; database; database = database->djb_next)
    {
    if (database->djb_flags & DJB_archive)
	{
	/* restart backups if required */

	restart_backup (database, MAX_ARCHIVE);

	FOR J IN JOURNAL_FILES WITH J.DB_ID EQ database->djb_id
				AND J.FILE_STATUS EQ LOG_CLOSED
				AND J.ARCHIVE_STATUS EQ NOT_ARCHIVED
				AND J.DELETE_STATUS EQ LOG_NOT_DELETED
	    MODIFY J USING
		strcpy (J.ARCHIVE_STATUS, ARCHIVE_PENDING);
	    END_MODIFY
	    ON_ERROR
		put_line (NULL_PTR, 138, 0, 0, 0);
		ROLLBACK;
		START_TRANSACTION;
		break;
	    END_ERROR;
	    SAVE;
	    backup_wal (database, J.LOG_NAME, J.FILE_ID, J.FILE_SEQUENCE, J.FILE_SIZE, J.PARTITION_OFFSET);
	END_FOR;
	}
    }

/* Sit in loop waiting for message from archives */

for (;;)
    {
    if (!(connections || archives_running))
	break;

    if (state & state_status)
	{
	state &= ~state_status;
	report_status ((CNCT) NULL_PTR);
	}
    commit();
    get_message ((JRNH*) message_buffer, sizeof (message_buffer));
    commit();
    }

/* Mark the file for deletion */

delete_wal();

commit();

/* Check out with database */

close_mailbox (slot_journal);
close_mailbox (slot_console);

FOR Y IN JOURNAL
    MODIFY Y USING
	Y.USE_COUNT = 0;
    END_MODIFY;
END_FOR;

for (database = databases; database; database = database->djb_next)
    if (database->djb_use_count)
	{
	database->djb_use_count = 0;
	set_use_count (database);
	}

put_message (103, (DJB) NULL);

if (log)
    fclose (log);

COMMIT;
FINISH;

return 0;
}

static BOOLEAN test_dup_entry (
    DJB		dbb,
    SLONG	file_id)
{
/**************************************
 *
 *	t e s t _ d u p _ e n t r y
 *
 **************************************
 *
 * Functional description
 *	Check if file_id is already present in the back up array.
 *	Returns 
 *		TRUE if present
 *		FALSE if not.
 *
 **************************************/
SSHORT	i;

for (i = 0; i < MAX_ARCHIVE; i++)
    if (dbb->djb_bfi [i].bi_fid == file_id)
	return TRUE;

return FALSE;
}

static void time_stamp (
    GDS__QUAD	*date)
{
/**************************************
 *
 *	t i m e _ s t a m p
 *
 **************************************
 *
 * Functional description
 *	Get the current time in gds format.
 *
 **************************************/
SLONG		clock;
struct tm	times;

clock = time (NULL);
#ifdef NETWARE_JOURNALLING
times = *localtime ((time_t*) &clock);
#else
times = *localtime (&clock);
#endif
gds__encode_date (&times, date);
}
