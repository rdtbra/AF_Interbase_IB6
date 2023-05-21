/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		jrn.c
 *	DESCRIPTION:	Journalling interface for database system.
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

#include "../jrd/time.h"
#include "../jrd/common.h"
#include <string.h>

#ifdef APOLLO
#define APOLLO_JOURNALLING
#define status_t	status_$t
#define SYS_ERROR	gds_arg_domain
#endif

#ifdef UNIX
#define UNIX_JOURNALLING
#define BSD_SOCKETS
#define SYS_ERROR	gds_arg_unix
#endif

#ifdef APOLLO_JOURNALLING
#include <apollo/base.h>
#include <apollo/mbx.h>
#include <apollo/error.h>
#endif

#ifndef status_t
#define status_t	int
#endif

#ifdef OS2_ONLY
#define INCL_DOSFILEMGR
#include <os2.h>
#define SYS_ERROR	gds_arg_dos
#endif

#ifdef NETWARE_386
#define NETWARE_JOURNALLING
#define BSD_SOCKETS
#define EINTR		0
#define SYS_ERROR       gds_arg_dos
#endif

#ifdef VMS
#define SYS_ERROR	gds_arg_vms
#endif

#ifdef mpexl
#define SYS_ERROR	gds_arg_mpexl
#endif

#ifdef WIN_NT
#include <windows.h>
#define TEXT		SCHAR
#define SYS_ERROR	gds_arg_win32
#endif

#ifdef BSD_SOCKETS
#include "../jrd/ib_stdio.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif
                 
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/jrn.h"
#include "../jrd/iberr_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/jrn_proto.h"

static int	do_connect (JRN *, STATUS *, TEXT *, USHORT, LTJC *, USHORT, UCHAR *, USHORT, USHORT);
static void	error (STATUS *, JRN, int, TEXT *);
static int	find_address (STATUS *, JRN, struct sockaddr_in *);
static int	get_reply (STATUS *, JRN, JRNR *);
static int	journal_close (STATUS *, JRN);
static int	jrn_put (STATUS *, JRN, JRNH *, USHORT, UCHAR *, USHORT);
static int	retry_connect (STATUS *, JRN *, TEXT *, USHORT, LTJC *, USHORT, UCHAR *, USHORT);

#ifdef SHLIB_DEFS
#define strlen		(*_libgds_strlen)
#define strcpy		(*_libgds_strcpy)
#define errno		(*_libgds_errno)
#define ib_fopen		(*_libgds_fopen)
#define ib_fclose		(*_libgds_fclose)
#define close		(*_libgds_close)
#define ib_fscanf		(*_libgds_fscanf)
#define connect		(*_libgds_connect)
#define send		(*_libgds_send)
#define socket		(*_libgds_socket)
#define recv		(*_libgds_recv)
#define sleep		(*_libgds_sleep)
#define memcpy		(*_libgds_memcpy)

extern int		strlen();
extern SCHAR		*strcpy();
extern int		errno;
extern IB_FILE		*ib_fopen();
extern int		ib_fclose();
extern int		close();
extern int		ib_fscanf();
extern int		connect();
extern int		send();
extern int		socket();
extern int		recv();
extern uint		sleep();
#ifndef IMP
extern void		*memcpy();
#else
extern SCHAR		*memcpy();
#endif
#endif

/*************************************************************************
Error Handling:
*	The error handling has been changed for journal routines.  The
*	routines now require a status vector and a journal handle to be
*	passed in for all calls.  The error is returned via the status
*	vector.  The caller has to call the appropriate error routine.
*	In case of jrd -
*
* Return values:
*	SUCCESS	- everything is fine.
*	FAILURE - typically some kind of OS error.
*	< 0	- BUGCHECK errors, unexpected values.
*	> 1	- ERROR conditions.
*************************************************************************
*/

int JRN_archive_begin (
    STATUS	*status_vector,
    JRN		*ret_journal,
    SLONG	db_id,
    SLONG	file_id,
    TEXT	*journal_name,
    USHORT	j_length)
{
/**************************************
 *
 *	J R N _ a r c h i v e _ b e g i n
 *
 **************************************
 *
 * Functional description
 *	Sign on with the journal server for starting archive
 * Returns:
 *	journal descriptor in ret_journal.
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/
LTJA	record;

*ret_journal = (JRN) NULL;

record.ltja_header.jrnh_type = JRN_ARCHIVE_BEGIN;
record.ltja_db_id	= db_id;
record.ltja_file_id	= file_id;
record.ltja_error_code	= 0;
record.ltja_length 	= 0;

/* Establish contact with the journal server */

return retry_connect (status_vector, ret_journal, journal_name, j_length,
	(LTJC *)&record, LTJA_SIZE, (UCHAR*)NULL_PTR, 0);
}

int JRN_archive_end (
    STATUS	*status_vector,
    JRN		*ret_journal,
    SLONG	db_id,
    SLONG	file_id)
{
/**************************************
 *
 *	J R N _ a r c h i v e _ e n d
 *
 **************************************
 *
 * Functional description
 *	Sign off with the journal server after archive
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/
LTJA	record;
JRN	journal;
int	ret_val;

journal = *ret_journal;
*ret_journal = (JRN) NULL;

/* If there is either a null journal block or
   a zero channel, there is no connection to
   server */

if (!(journal))
    return SUCCESS;

if (!journal->jrn_channel)
    {
    gds__free (journal);
    return SUCCESS;
    }

record.ltja_header.jrnh_type = JRN_ARCHIVE_END;
record.ltja_db_id	= db_id;
record.ltja_file_id	= file_id;
record.ltja_error_code 	= 0;
record.ltja_length 	= 0;

if ((ret_val = jrn_put (status_vector, journal, (JRNH*)&record, LTJA_SIZE, 
			(UCHAR*)NULL_PTR, 0)) != SUCCESS)
    return ret_val;

if ((ret_val = journal_close (status_vector, journal)) != SUCCESS)
    return ret_val;

/* Free up the space */

gds__free (journal);

return SUCCESS;
}

int JRN_archive_error (
    STATUS	*status_vector,
    JRN		*ret_journal,
    SLONG	db_id,
    SLONG	file_id,
    SLONG	error_code)
{
/**************************************
 *
 *	J R N _ a r c h i v e _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Sign off with the journal server after an error during archive
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/
LTJA	record;
JRN	journal;
int	ret_val;

journal = *ret_journal;
*ret_journal = (JRN) NULL;

if (!(journal))
    return SUCCESS;

if (!journal->jrn_channel)
    {
    gds__free (journal);
    return SUCCESS;
    }

record.ltja_header.jrnh_type = JRN_ARCHIVE_ERROR;
record.ltja_db_id	= db_id;
record.ltja_file_id	= file_id;
record.ltja_error_code 	= error_code;
record.ltja_length 	= 0;

if ((ret_val = jrn_put (status_vector, journal, (JRNH*)&record, LTJA_SIZE, 
			(UCHAR*)NULL_PTR, 0)) != SUCCESS)
    return ret_val;

if ((ret_val = journal_close (status_vector, journal)) != SUCCESS)
    return ret_val;

/* Free up the space */

gds__free (journal);

return SUCCESS;
}

int JRN_disable (
    STATUS	*status_vector,
    JRN		journal,
    JRNH	*header,
    UCHAR	*data,
    USHORT	d_len)
{
/**************************************
 *
 *	J R N _ d i s a b l e
 *
 **************************************
 *
 * Functional description
 * 	Journal server entry should be delete from header page before this.  
 *	Disable journalling, if enabled.
 *
 **************************************/

if (journal->jrn_channel)
    return jrn_put (status_vector, journal, header, LTJC_SIZE, data, d_len);

return SUCCESS;
}

void JRN_dump_page (void)
{
/**************************************
 *
 *	J R N _ d u m p _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Dump a page to the journal.  If necessary, fetch it.
 *
 **************************************/

/* Stub function for now */
}

int JRN_enable (
    STATUS	*status_vector,
    JRN		*ret_journal,
    TEXT	*journal_name,
    USHORT	j_length,
    UCHAR	*data,
    USHORT	d_len,
    LTJC	*control)
{
/**************************************
 *
 *	J R N _ e n a b l e
 *
 **************************************
 *
 * Functional description
 *	Enable journalling for the database.
 * Returns	
 *	journal descriptor in ret_journal.
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/

return retry_connect (status_vector, ret_journal, journal_name, j_length,
		     control, LTJC_SIZE, data, d_len);
}

int JRN_fini (
    STATUS	*status_vector,
    JRN		*jrn)
{
/**************************************
 *
 *	J R N _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Check out with journal system.
 *
 **************************************/
#ifdef APOLLO_JOURNALLING
mbx_$server_msg_t		*retptr, *reply;
status_t	status;
SLONG		retlen;
#endif
LTJC		record;
JRN		journal;
int		ret_val;

journal = *jrn;
*jrn = (JRN) NULL;

/* If there is either a null journal block or
   a zero channel, there is no connection to
   server */

if (!(journal))
    return SUCCESS;

if (!journal->jrn_channel) 
    {
    gds__free (journal);
    return SUCCESS;
    }

/* Send a signoff record */

record.ltjc_header.jrnh_type = JRN_SIGN_OFF;
record.ltjc_length = 0;

if ((ret_val = jrn_put (status_vector, journal, (JRNH*)&record, LTJC_SIZE, 
			(UCHAR*)NULL_PTR, 0)) != SUCCESS)
    return ret_val;

/* Read reply.  This will fail since the server will break the connection.
   Don't worry about it. */

if ((ret_val = journal_close (status_vector, journal)) != SUCCESS)
    return ret_val;

/* Free up the space */

gds__free (journal);

return SUCCESS;
}

int JRN_init (
    STATUS	*status_vector,
    JRN		*ret_journal,
    USHORT	page_size,
    UCHAR	*journal_dir,
    USHORT	jd_len,
    UCHAR	*data,
    USHORT	d_len)
{
/**************************************
 *
 *	J R N _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize the journalling system.  
 *	If journal directory is specified, set up a connection to the 
 *	journal server.
 * Returns:
 *	journal descriptor in ret_journal.
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/
TEXT	server_name [MAX_PATH_LENGTH];
LTJC	control;

if (!journal_dir)
    {
    *ret_journal = (JRN) NULL;
    return FAILURE;
    }

memcpy (server_name, journal_dir, jd_len);

server_name [jd_len] = 0;

control.ltjc_header.jrnh_type = JRN_SIGN_ON;
control.ltjc_page_size 	= page_size;
control.ltjc_seqno	= 0;
control.ltjc_offset	= 0;
control.ltjc_length 	= d_len;

/* Establish contact with the journal server */

return retry_connect (status_vector, ret_journal, server_name, jd_len, 
		     &control, LTJC_SIZE, data, d_len);
}

void JRN_make_init_data (
    UCHAR	*data,
    SSHORT	*ret_len,
    UCHAR	*db_name,
    USHORT	db_len,
    UCHAR	*backup_dir,
    USHORT	b_length)
{
/**************************************
 *
 *	J R N _ m a k e _ i n i t _ d a t a
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

int JRN_put_wal_name (
    STATUS	*status_vector,
    JRN		journal,
    TEXT	*walname,
    USHORT	w_length,
    SLONG	seqno,
    SLONG	offset,
    SLONG	p_offset,
    USHORT	mode)
{
/**************************************
 *
 *	J R N _ p u t _ w a l _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Put in WAL file name to the journal server.
 *	mode is used for JRN_WAL_FILE (JRNW_OPEN/JRNW_CLOSE)
 * Input Params:
 *	status_vector 
 *	journal		- journal descriptor
 *	walname, w_length -	wal file name and length
 *	seqno, offset	- seqno/offset pair
 *	p_offset	- partition offset
 *	mode		- mode of operation open/close of file
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/

return JRN_put_wal_info (status_vector, journal, walname, w_length, seqno, 
		offset, p_offset, mode, 1, (USHORT*)NULL_PTR, JRN_WAL_NAME);
}

int JRN_put_old_start (
    STATUS	*status_vector,
    JRN		journal,
    SLONG	seqno,
    SLONG	offset,
    SLONG	p_offset,
    USHORT	*dump_id)
{
/**************************************
 *
 *	J R N _ p u t _ o l d _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Put in Online Dump information
 * Input Params:
 *	status_vector 
 *	journal		- journal descriptor
 *	seqno, offset	- seqno/offset pair.
 *	p_offset	- partition offset
 *	dump_id		- address of dump id.
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *	For start online dump, returns the dump id.
 *
 **************************************/

return JRN_put_wal_info (status_vector, journal, (TEXT*)0, 0, seqno, 
	    offset, p_offset, 0, 0, dump_id, JRN_START_ONLINE_DMP);
}

int JRN_put_old_end (
    STATUS	*status_vector,
    JRN		journal,
    SLONG	seqno,
    SLONG	offset,
    SLONG	p_offset,
    USHORT	dump_id)
{
/**************************************
 *
 *	J R N _ p u t _ o l d _ e n d
 *
 **************************************
 *
 * Functional description
 *	Put in Online Dump information
 * Input Params:
 *	status_vector 
 *	journal		- journal descriptor
 *	seqno, offset	- BSN of a wal record (0 if not needed)
 *	p_offset	- partition offset
 *	dump_id		- id of dump.
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 **************************************/

return JRN_put_wal_info (status_vector, journal, (TEXT*)0, 0, seqno, 
		offset, p_offset, dump_id, 0, (USHORT*)NULL_PTR, JRN_END_ONLINE_DMP);
}

int JRN_put_old_file (
    STATUS	*status_vector,
    JRN		journal,
    TEXT	*old_file_name,
    USHORT	file_length,
    SLONG	file_size,
    USHORT	file_seqno,
    USHORT	dump_id)
{
/**************************************
 *
 *	J R N _ p u t _ o l d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Put in online dump file information.
 * Input Params:
 *	status_vector 
 *	journal		- journal descriptor
 *	old_file_name, file_length -	file name and name length
 *	file_size	- file size.
 *	file_count	- sequence number of file.
 *	dump_id		- id of dump.
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *	For start online dump, returns the dump id.
 *
 **************************************/

return JRN_put_wal_info (status_vector, journal, old_file_name, file_length, 
		(SLONG)0, file_size, (SLONG)0, dump_id, file_seqno,
		(USHORT*)NULL_PTR, JRN_ONLINE_DMP_FILE);
}

int JRN_put_wal_info (
    STATUS	*status_vector,
    JRN		journal,
    TEXT	*walname,
    USHORT	w_length,
    SLONG	seqno,
    SLONG	offset,
    SLONG	p_offset,
    USHORT	mode,
    USHORT	file_count,
    USHORT	*dump_id,
    USHORT	type)
{
/**************************************
 *
 *	J R N _ p u t _ w a l _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Put in some kind of WAL info (Control point, New WAL file etc)
 *	mode is used for JRN_WAL_FILE (=JRNW_OPEN)
 * Input Params:
 *	status_vector 
 *	journal		- journal descriptor
 *	walname, w_length -	wal file name and length
 *	seqno, offset	- BSN of a wal record (0 if not needed)
 *	p_offset	- partition offset
 *	mode		- mode of operation open/close of file
 *	file_count	- number of files (for start online dump )
 *	type		- type of record (JRN_WAL_NAME/JRN_CNTRL_PT etc.)
 *			  JNR_START_ONLINE_DMP/JRN_ONLINE_DUMP_FILE
 * Returns:
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *	For start online dump, returns the dump id.
 *
 **************************************/
LTJW		jrnwal;
struct jrnr	reply;
int 		ret_val;

jrnwal.ltjw_header.jrnh_type	= type;
jrnwal.ltjw_mode 		= mode;
jrnwal.ltjw_seqno 		= seqno;
jrnwal.ltjw_offset 		= offset;
jrnwal.ltjw_p_offset 		= p_offset;
jrnwal.ltjw_count 		= file_count;
jrnwal.ltjw_length 		= w_length;

if ((ret_val = jrn_put (status_vector, journal, (JRNH*)&jrnwal, 
			LTJW_SIZE, (UCHAR*)walname, w_length)) != SUCCESS)
    return ret_val;

#ifndef VMS
#ifndef mpexl
if ((ret_val = get_reply (status_vector, journal, &reply)) != SUCCESS)
    return ret_val;

switch (reply.jrnr_response)
    {
    case jrnr_ack:
	break;

    case jrnr_start_dump:

	if (dump_id)
	    *dump_id = (USHORT) reply.jrnr_page;
	break;

    case jrnr_end_dump:
	break;
    
    default:
	return (-170); /* msg 170 unexpected reply from journal server */
    }
#endif
#endif

return SUCCESS;
}

void JRN_sync (void)
{
/**************************************
 *
 *	J R N _ s y n c
 *
 **************************************
 *
 * Functional description
 *	Synchronize with the journal -- wait until all bits have
 *	been flushed to oxide.
 *
 **************************************/

/* stub function for now */
}

static int do_connect (
    JRN		*ret_jrn,
    STATUS	*status_vector,
    TEXT	*journal_name,
    USHORT	j_length,
    LTJC	*control,
    USHORT	control_length,
    UCHAR	*data,
    USHORT	d_length,
    USHORT	retry)
{
/**************************************
 *
 *	d o _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Connect to a journal server and get response.  Return the journal
 *	block.
 * Returns
 *	SUCCESS/FAILURE/BUGCHECK/ERROR.
 *
 **************************************/
#ifdef BSD_SOCKETS
struct	sockaddr_in	address;
#endif
#ifdef APOLLO_JOURNALLING
status_t	status;
#endif
#ifdef OS2_ONLY
HFILE		handle;
ULONG		state, status;
#endif
TEXT		*p, *q, name [MAX_PATH_LENGTH];
SSHORT		l;
JRN		journal;
struct jrnr	reply;
int		loop, ret_val;


/* Make sure we don't return a bogus journal handle */

*ret_jrn = NULL;

/* Start by copying file name */

p = name;

#ifdef mpexl
strcpy (p, JOURNAL_FILE);
p += strlen (p);
if (j_length && *journal_name != '.')
    *p++ = '.';
#endif

#ifdef OS2_ONLY
strcpy (p, "\\PIPE\\");
p += strlen (p);

/* For now, ignore the input file path */

j_length = 0;
#endif

#ifdef WIN_NT
strcpy (p, "\\\\.\\pipe");
p += strlen (p);
if (!j_length || (*journal_name != '/' && *journal_name != '\\'))
    *p++ = '\\';
#endif

q = journal_name;
if (l = j_length)
    do *p++ = *q++; while (--l);

/* Generate connect string */

#ifdef APOLLO_JOURNALLING
if (j_length && p [-1] != '/')
    *p++ = '/';
strcpy (p, JOURNAL_FILE);
#endif
	 
#ifdef UNIX_JOURNALLING
if (j_length && p [-1] != '/')
    *p++ = '/';
strcpy (p, JOURNAL_ADDR);
#endif

#ifdef NETWARE_JOURNALLING
for (q = name; q < p; q++)
    if (*q == '/')
        *q = '\\';
if (p [-1] != '\\')
    *p++ = '\\';
strcpy (p, JOURNAL_ADDR);
#endif

#ifdef OS2_ONLY
strcpy (p, JOURNAL_FILE);
#endif

#ifdef VMS
if (j_length && p [-1] != ':' && p [-1] != ']')
    *p++ = ':';
strcpy (p, JOURNAL_FILE);
#endif

#ifdef WIN_NT
for (q = name; q < p; q++)
    if (*q == ':')
	*q = '_';
    else if (*q == '/')
	*q = '\\';
if (p [-1] != '\\')
    *p++ = '\\';
strcpy (p, JOURNAL_FILE);
#endif

#ifdef mpexl
*p = 0;
#endif

/* Make up the block */

l = strlen (name);

journal = (JRN) gds__alloc ((SLONG)((sizeof (struct jrn)) + l));
/* FREE: by error returns from this module, otherwise by JRN_fini & friends */
if (!journal)	/* NOMEM: */
    {
    error (status_vector, NULL, 0, "gds__alloc");
    return FAILURE;
    }

strcpy (journal->jrn_server, name);

#ifdef APOLLO_JOURNALLING
mbx_$open (journal->jrn_server, 
       l, 
       NULL, 
       (SLONG) 0, 
       &journal->jrn_channel, 
       &status);

if (status.all)
    {
    error (status_vector, journal, status, "mbx_$open");
    gds__free (journal);
    return FAILURE;
    }
#endif

#ifdef mpexl
journal->jrn_status_vector = status_vector;
JIO_open (journal);
#endif

#ifdef OS2_ONLY
if (status = DosOpen (
	journal->jrn_server,
	&handle,
	&state,
	0L,			/* allocation -- not used */
	0L,			/* attributes */
	0x00000001,		/* Open pipe if it exists */
	0x00004012,		/* open mode r/w, nosharing */
	0L))			/* extended attributes */
    {
    error (status_vector, journal, status, "DosOpen");
    gds__free (journal);
    return FAILURE;
    }

journal->jrn_channel = (int*) (SLONG) handle;
#endif

#ifdef BSD_SOCKETS
for (loop = 0; loop < 20; loop++)
    {
    for (;;)
	{
	journal->jrn_channel = (int*) socket (AF_INET, SOCK_STREAM, 0);
	if ((int) journal->jrn_channel != -1)
		break;
	if (!SYSCALL_INTERRUPTED(errno))
	    {
	    error (status_vector, journal, errno, "socket");
	    gds__free (journal);
	    return FAILURE;
	    }
	}

    /* Zero out the structure */

    p = (TEXT *) &address;
    l = sizeof (address);
    do *p++ = 0; while (--l);

    if ((ret_val = find_address (status_vector, journal, &address)) != SUCCESS)
	{
	gds__free (journal);
	return ret_val;
	}

    if (!connect ((int) journal->jrn_channel, &address, sizeof (address)))
	break;

    sleep (3);

    if (loop < 16)
	{
	close ((int) journal->jrn_channel);
	continue;
	}
    if (!SYSCALL_INTERRUPTED(errno))
	{
	if (retry)
	    {
	    close ((int) journal->jrn_channel);
	    gds__free (journal);
	    *ret_jrn  = (JRN) NULL;
	    return SUCCESS;
	    }
	error (status_vector, journal, errno, "connect (journal server)");
	gds__free (journal);
	return FAILURE;
	}
    close ((int) journal->jrn_channel);
    }
#endif

#ifdef VMS
journal->jrn_status_vector = status_vector;
JIO_open (journal);
#endif

#ifdef WIN_NT
journal->jrn_channel = (int*) CreateFile (journal->jrn_server,
	GENERIC_READ | GENERIC_WRITE,
	0,
	NULL,
	OPEN_EXISTING,
	0,
	NULL);
if (journal->jrn_channel == INVALID_HANDLE_VALUE)
    {
    error (status_vector, journal, GetLastError(), "CreateFile");
    gds__free (journal);
    return FAILURE;
    }
#endif

/* Send an enable message */

if ((ret_val = jrn_put (status_vector, journal, (JRNH*)control, 
			control_length, data, d_length)) != SUCCESS)
    {
    gds__free (journal);
    return ret_val;
    }


#ifndef VMS
#ifndef mpexl
if ((ret_val = get_reply (status_vector, journal, &reply)) != SUCCESS)
    {
    gds__free (journal);
    return ret_val;
    }

journal->jrn_handle = reply.jrnr_header.jrnh_handle;

if (reply.jrnr_header.jrnh_version != JOURNAL_VERSION)
    {
    gds__free (journal);
    return (171); /* msg 171 journal server is incompatible version */
    }

switch (reply.jrnr_response)
    {
    case jrnr_accepted:
    case jrnr_enabled:
	break;
    
    case jrnr_archive_error:
	gds__free (journal);
	return (282);

    case jrnr_rejected:
	gds__free (journal);
	return (172); /* msg 172 journal server refused connection */
    
    default:
        if (retry)
	    {
#ifdef APOLLO_JOURNALLING
	    ib_printf ("APOLLO_JOURNALLING unimplemented feature\n");
#endif
#ifdef OS2_ONLY
	    DosClose ((HFILE) journal->jrn_channel);
#endif
#ifdef BSD_SOCKETS
	    close ((int) journal->jrn_channel);
#endif
#ifdef WIN_NT
	    CloseHandle ((HANDLE) journal->jrn_channel);
#endif
	    gds__free (journal);
	    *ret_jrn  = (JRN) NULL;
	    return SUCCESS;
	    }
	gds__free (journal);
	return (-170); /* msg 170 unexpected reply from journal server */
    }
#endif
#endif

*ret_jrn = journal;

return SUCCESS;
}

static void error (
    STATUS	*status_vector,
    JRN		journal,
    status_t	status,
    TEXT	*string)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	An operating system error occurred somewhere.
 *	Fill up the status vector.
 *
 **************************************/

IBERR_build_status (status_vector, gds__sys_request, 
		    gds_arg_string, string,
		    SYS_ERROR, status, 
		    gds_arg_gds, gds__journerr, 
		    gds_arg_string, (journal) ? journal->jrn_server : "",
		    0);
}

#ifdef BSD_SOCKETS
static int find_address (
    STATUS		*status_vector,
    JRN			journal,
    struct sockaddr_in	*address)
{
/**************************************
 *
 *	f i n d _ a d d r e s s		( U N I X   &   N E T W A R E )
 *
 **************************************
 *
 * Functional description
 *	Get the socket address established by the server by
 *	looking in the designated files.
 *
 **************************************/
IB_FILE	*file;
SLONG	addr, family, port, version;
TEXT	*filename;
                                     
filename = journal->jrn_server;

if (!(file = ib_fopen (filename, "r")))
    {
    error (status_vector, journal, errno, "ib_fopen");
    return FAILURE;
    }

if (ib_fscanf (file, "%ld %ld %ld %ld", &version, &addr, &family, &port) != 4)
    {
    error (status_vector, journal, 0, "journal socket file format");
    return FAILURE;
    }
ib_fclose (file);

if (version != JOURNAL_VERSION)
    {
    error (status_vector, journal, 0, " address version");
    return FAILURE;
    }

address->sin_addr.s_addr = addr;
address->sin_family = family;
address->sin_port = port;

return SUCCESS;
}
#endif

static int get_reply (
    STATUS	*status_vector,
    JRN         journal,
    JRNR	*reply)
{
/**************************************
 *
 *	g e t _ r e p l y
 *
 **************************************
 *
 * Functional description
 *	Get reply from database.
 *
 **************************************/
#ifdef APOLLO_JOURNALLING
JRNR		*retptr;
status_t	status;
#endif
#ifdef OS2_ONLY
ULONG		l1, status;
#endif
SLONG		l;

#ifdef APOLLO_JOURNALLING
mbx_$get_rec (journal->jrn_channel, reply, 
	(SLONG) sizeof (struct jrnr), &retptr, &l, &status);
if (status.all)
    {
    error (status_vector, journal, status, "mbx_$get_rec");
    return FAILURE;
    }

if ((UCHAR*) retptr != (UCHAR*) reply)
    memcpy (reply, retptr, l);
#endif

#ifdef OS2_ONLY
if (status = DosRead ((HFILE) journal->jrn_channel, reply, sizeof (struct jrnr), &l1))
    {
    error (status_vector, journal, status, "DosRead");
    return FAILURE;
    }
#endif

#ifdef BSD_SOCKETS
/*  If the recv call is interrupted, another process is in contention
    with us, so keep trying until successful.  */
    
do {
   l = recv ((int) journal->jrn_channel, reply, sizeof (struct jrnr), 0);
   } while (l < 0 && SYSCALL_INTERRUPTED(errno));

if (l < 0)
    {
    error (status_vector, journal, errno, "socket recv");
    return FAILURE;
    }
#endif

#ifdef WIN_NT
if (!ReadFile ((HANDLE) journal->jrn_channel, reply, sizeof (struct jrnr), &l, NULL) ||
    l != sizeof (struct jrnr))
    {
    error (status_vector, journal, GetLastError(), "ReadFile");
    return FAILURE;
    }
#endif

return SUCCESS;
}

static int journal_close (
    STATUS	*status_vector,
    JRN		journal)
{
/**************************************
 *
 *	j o u r n a l _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Close network connection
 *	Has to be valid handle.
 *
 **************************************/
#ifdef APOLLO_JOURNALLING
mbx_$server_msg_t		*retptr, *reply;
status_t	status;
SLONG		retlen;
#endif

#ifdef APOLLO_JOURNALLING
mbx_$get_rec (journal->jrn_channel, reply, 
	(SLONG) sizeof (struct jrnr), &retptr, &retlen, &status);
mbx_$close (journal->jrn_channel, &status);
#endif

#ifdef mpexl
journal->jrn_status_vector = status_vector;
JIO_fini (journal);
#endif

#ifdef OS2_ONLY
DosClose ((HFILE) journal->jrn_channel);
#endif

#ifdef BSD_SOCKETS
if (close ((int) journal->jrn_channel) < 0)
    {
    error (status_vector, journal, errno, "close");
    return FAILURE;
    }
#endif

#ifdef VMS
journal->jrn_status_vector = status_vector;
JIO_fini (journal);
#endif

#ifdef WIN_NT
CloseHandle ((HANDLE) journal->jrn_channel);
#endif

return SUCCESS;
}

static int jrn_put (
    STATUS	*status_vector,
    JRN		journal,
    JRNH	*header,
    USHORT	h_length,
    UCHAR	*data,
    USHORT	d_length)
{
/**************************************
 *
 *	j r n _ p u t
 *
 **************************************
 *
 * Functional description
 *	Write a journal record.
 *
 **************************************/
#ifdef APOLLO_JOURNALLING
status_t	status;
#endif
USHORT		length;
SLONG		l;
UCHAR		*p, *q;
#ifdef OS2_ONLY
ULONG		l1, status;
#endif
#ifndef mpexl
UCHAR		buffer [MAX_RECORD];
#else
SLONG		force [MAX_RECORD / 4 + 1];
UCHAR		*buffer;
#endif

#ifdef mpexl
buffer = (UCHAR*) force;
#endif

/* Prepare journal message for sending */

length = h_length + d_length;
header->jrnh_handle = journal->jrn_handle;
header->jrnh_length = length;
header->jrnh_series = journal->jrn_series;
header->jrnh_version = JOURNAL_VERSION;

header->jrnh_prev_seqno  = 0;
header->jrnh_prev_offset = 0;

/* Concatenate header and data.  This is boring but necessary. */

p = buffer;
q = (UCHAR*) header;
do *p++ = *q++; while (--h_length);

if (d_length)
    do *p++ = *data++; while (--d_length);

/* Send message */

#ifdef APOLLO_JOURNALLING
p = buffer;
mbx_$put_rec (journal->jrn_channel, p, (SLONG) length, &status);
if (status.all)
    {
    error (status_vector, journal, status, "mbx_$put_rec");
    return FAILURE;
    }
#endif

#ifdef mpexl
journal->jrn_status_vector = status_vector;
JIO_put (journal, buffer, length);
#endif

#ifdef OS2_ONLY
if (status = DosWrite ((HFILE) journal->jrn_channel, buffer, (ULONG) length, &l1))
    {
    error (status_vector, journal, status, "DosWrite");
    return FAILURE;
    }
#endif
    
#ifdef BSD_SOCKETS
/*  If the send call is interrupted, another process is in contention
    with us, so keep trying until successful.  */
/*  In case of NETWARE, send call would not be interrupted because
    of contention.  Since EINTR has been defined as 0, the code below
    would work for NETWARE. */
    
do {
   l = send ((int)journal->jrn_channel, buffer, (int) length, 0);
   } while (l < 0 && SYSCALL_INTERRUPTED(errno));

if (l < 0)
    {
    error (status_vector, journal, errno, "journal send");
    return FAILURE;
    }

if (l != length)
    {
    error (status_vector, journal, 0, "journal send truncated");
    return FAILURE;
    }
#endif

#ifdef VMS
journal->jrn_status_vector = status_vector;
JIO_put (journal, buffer, length);
#endif

#ifdef WIN_NT
if (!WriteFile ((HANDLE) journal->jrn_channel, buffer, (SLONG) length, &l, NULL) ||
    l != length)
    {
    error (status_vector, journal, GetLastError(), "WriteFile");
    return FAILURE;
    }
#endif

return SUCCESS;
}

static int retry_connect (
    STATUS	*status_vector,
    JRN		*journal,
    TEXT	*journal_name,
    USHORT	j_length,
    LTJC	*control,
    USHORT	control_length,
    UCHAR	*data,
    USHORT	d_length)
{
/**************************************
 *
 *      r e t r y _ c o n n e c t
 *
 **************************************
 *
 * Functional description
 *      Wrapper around do_connect ().  Tries to connect 5 times.
 *	If SUCCESS and journal handle is still null, keep trying.
 *
 **************************************/
SSHORT   count;
int	ret_val;

for (count = 4; count >=  0; count--)
    {
    if ((ret_val = do_connect (journal, status_vector, journal_name, 
		    j_length, control, control_length, data, d_length,
		    count)) != SUCCESS)
	return ret_val;

    if (*journal != (JRN) NULL)
	break;
    }

return SUCCESS;
}
