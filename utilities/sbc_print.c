/*
 *	PROGRAM:	JRD Cache Manager
 *	MODULE:		sbc_print.c
 *	DESCRIPTION:	Shared Cache printer
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
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include "../jrd/common.h"

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;

#include "../jrd/isc.h"
#include "../jrd/cash.h"
#include "../jrd/ods.h"
#include "../jrd/llio.h"
#include "../utilities/ppg_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/isc_s_proto.h"

#ifdef APOLLO
#include "/sys/ins/ms.ins.c"
#include "/sys/ins/error.ins.c"
#endif

#ifdef OS2_ONLY
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#include <os2.h>
#include <io.h>
#endif

#ifdef WIN_NT
#include <io.h>
#include <windows.h>
#define TEXT		SCHAR
#endif

#define BASE 			((UCHAR*) CASH_header)
#define REL_PTR(item)		((UCHAR*) item - BASE)
#define ABS_PTR(item)		(BASE + item)

#define QUE_INIT(que)		{que.srq_forward = que.srq_backward = REL_PTR (&que);}
#define QUE_EMPTY(que)		(que.srq_forward == REL_PTR (&que))
#define QUE_NOT_EMPTY(que)	(que.srq_forward != REL_PTR (&que))
#define QUE_NEXT(que)		ABS_PTR (que.srq_forward)
#define QUE_PREV(que)		ABS_PTR (que.srq_backward)

#define QUE_LOOP(que_head,que)	for (que = (SRQ) QUE_NEXT (que_head);\
	que != &que_head; que = (SRQ) QUE_NEXT ((*que)))
#define QUE_LOOP_BACK(que_head,que)	for (que = (SRQ) QUE_PREV (que_head);\
	que != &que_head; que = (SRQ) QUE_PREV ((*que)))

#define DEFAULT_SIZE	8192

#if !(defined WIN_NT || defined OS2_ONLY)
extern SCHAR     *sys_errlist[];
#endif

static void	cache_init (void);
static void	db_get_sbc (SCHAR *, SCHAR *, SLONG *, SSHORT *);
#ifdef APOLLO
static void	db_error (status_$t);
#else
#if (defined WIN_NT || defined OS2_ONLY || defined mpexl)
static void	db_error (SLONG);
#else
static void	db_error (int);
#endif
#endif
static void	db_open (UCHAR *, USHORT);
static PAG	db_read (SLONG);
static void	print_error (void);
static void	print_header (TEXT *);
static void	print_page_header (SDB);
static void	print_process (PRB);
static void	prt_que (UCHAR *, SRQ, FPTR_VOID, int, int);
static void	prt_que_back (UCHAR *, SRQ, FPTR_VOID, int, int);
static void	print_sccb (void);

static int	sw_mru, sw_nbuf, sw_header, sw_page, sw_user, sw_free, sw_all;
static TEXT	*dbname, sw_file [257];

static SHB	CASH_header;
static SCCB	sccb;
static SLONG	mapped_size = DEFAULT_SIZE;
static SLONG	page_no = -1;
static SSHORT    page_size = 1024;

static int	map_length, map_base, map_count;
static SCHAR	*map_region;
#ifdef WIN_NT
static void	*file;
#else
#ifdef OS2_ONLY
static SLONG	file;
#else
static int	file;	
#endif
#endif
SCHAR		global_buffer [MAX_PAGE_SIZE];

#ifdef NETWARE_386
static SVC	sw_outfile;
#else
static IB_FILE	*sw_outfile;
#endif

SCHAR *page_type [ ] = {
    "pag_undefined    ",
    "pag_header       ",       /* Database header page */
    "pag_pages        ",       /* Page inventory page */
    "pag_transactions ",       /* Transaction inventory page */
    "pag_pointer      ",       /* Pointer page */
    "pag_data         ",       /* Data page */
    "pag_root         ",       /* Index root page */
    "pag_index        ",       /* Index (B-tree) page */
    "pag_blob         ",       /* Blob data page */
    "pag_ids          ",       /* Gen-ids */
    "pag_log          "	       /* Log page */
};

int CLIB_ROUTINE main (
    int		argc,
    char	*argv[])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Initialize shared cache for process.
 *
 **************************************/
SLONG		length;
SCHAR		*p, c;
STATUS		status_vector [20];
TEXT		expanded_filename [256];
SH_MEM_T	shmem_data;
SLONG		cache_buffers;
SSHORT		cache_flags;
int		nbuf;
int		nfree_buf;
SLONG		redir_in, redir_out, redir_err;

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
    }
else if (argc > 4 && !strcmp (argv [1], "-svc_re"))
    {
    redir_in = atol (argv [2]);
    redir_out = atol (argv [3]);
    redir_err = atol (argv [4]);
#ifdef WIN_NT
    redir_in = _open_osfhandle (redir_in, 0);
    redir_out = _open_osfhandle (redir_out, 0);
    redir_err = _open_osfhandle (redir_err, 0);
#endif
    if (redir_in != 0)
	if (dup2 ((int) redir_in, 0))
	    close ((int) redir_in);
    if (redir_out != 1)
	if (dup2 ((int) redir_out, 1))
	    close ((int) redir_out);
    if (redir_err != 2)
	if (dup2 ((int) redir_err, 2))
	    close ((int) redir_err);
    argv += 4;
    argc -= 4;
    }

#ifdef NETWARE_386
sw_outfile = service;
#else
sw_outfile = ib_stderr;
#endif

/* Handle switches, etc. */

dbname = (SCHAR *) 0;
nbuf = 0;
nfree_buf = 0;
argv++;

while (--argc)
    {
    p = *argv++;

    if (p [0]  != '-')
	{
	dbname = p;
	break;
	}
    while (c = *p++)
	switch (UPPER (c))
	    {
	    case 'M':
		sw_mru = TRUE;
		break;

	    case 'N':
		sw_nbuf = TRUE;
		nbuf = atoi (*argv++);
		--argc;
		break;

	    case 'P':
		sw_page = TRUE;
		page_no = atoi (*argv++);
		--argc;
		break;

	    case 'F':
		sw_free = TRUE;
		nfree_buf = atoi (*argv++);
		--argc;
		break;

	    case 'H':
		sw_header = TRUE;
		break;

	    case 'A':
		sw_all = TRUE;
		break;

	    case 'U':
		sw_user = TRUE;
		break;

	    case '-':
		break;

	    default:
		print_error();
		exit (FINI_ERROR);
	    }
    }

if (!dbname)
    {
    print_error();
    exit (FINI_ERROR);
    }

db_get_sbc (dbname, sw_file, &cache_buffers, &cache_flags);

if (cache_flags || !cache_buffers)
    {
    ib_printf ("No shared Cache\n");
    exit (FINI_ERROR);
    }

ISC_expand_filename (sw_file, strlen (sw_file), expanded_filename);

#ifdef UNIX
shmem_data.sh_mem_semaphores = 0;
#endif
CASH_header = ISC_map_file (status_vector,
	expanded_filename,
	cache_init,
	NULL_PTR,
	-mapped_size,
	&shmem_data);

if (CASH_header && CASH_header->shb_length > mapped_size)
    {
    length = CASH_header->shb_length;
    ISC_unmap_file (status_vector, &shmem_data, FALSE);
    CASH_header = ISC_map_file (status_vector,
	expanded_filename,
	cache_init,
	NULL_PTR,
	length,
	&shmem_data);
    mapped_size = length;
    }

if (!CASH_header)
    {
    ib_printf ("Unable to access shared cache\n");
    gds__print_status (status_vector);
    exit (FINI_OK);
    }

if (sw_all)
    sw_page = sw_header = sw_user = TRUE;

/* print the shared cache header */

print_header (expanded_filename);

sccb = (SCCB) ABS_PTR (CASH_header->shb_sccb);

/* Print shared cache header block */

print_sccb();

/* print process queue or just count of processes */

if (sw_user)
    {
    prt_que ("\tProcesses", &CASH_header->shb_processes, print_process, 
	    OFFSET (PRB, prb_shb_processes), 0);
    prt_que ("\tFree Processes", &CASH_header->shb_free_processes, 
		print_process, OFFSET (PRB, prb_shb_processes), 0);
    }
else
    {
    prt_que ("\tProcesses", &CASH_header->shb_processes, 0, 0, 0);
    prt_que ("\tFree Processes", &CASH_header->shb_free_processes, 0, 0, 0);
    }

/* Print buffers in LRU or MRU order */

if (!sw_mru)
    {
    if (sw_header || sw_page)
	{
	prt_que ("\tLRU pages", &sccb->sccb_lru, print_page_header, 
	    OFFSET (SDB, sdb_lru_free), nbuf);
	}
    else
	{
	prt_que ("\tLRU pages", &sccb->sccb_lru, 0, 0, 0);
	}
    }
else
    {
    if (sw_header || sw_page)
	{
	prt_que_back ("\tLRU pages", &sccb->sccb_lru, print_page_header, 
	    OFFSET (SDB, sdb_lru_free), nbuf);
	}
    else
	{
	prt_que_back ("\tMRU pages", &sccb->sccb_lru, 0, 0, 0);
	}
    }

/* Print free buffer list */

if (sw_free)
    {
    if (sw_header || sw_page)
	{
	prt_que ("\tFree pages", &sccb->sccb_free, print_page_header, 
	    OFFSET (SDB, sdb_lru_free), nfree_buf);
	}
    else
	{
	prt_que ("\tFree pages", &sccb->sccb_free, 0, 0, 0);
	}
    }

/* Print free waiters */

prt_que ("\tFree waiters", &sccb->sccb_free_waiters, 0, 0, 0);

ib_printf ("\n");

exit (FINI_OK);
}

static void cache_init (void)
{
/**************************************
 *
 *	c a c h e _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize shared cache for looking 
 *	Just used as a param for ISC_ () call.
 *
 **************************************/
}

static void db_get_sbc (
    SCHAR	*db,
    SCHAR	*name,
    SLONG	*cache_buffers,
    SSHORT	*cache_flags)
{
/**************************************
 *
 *	d b _ g e t _ s b c
 *
 **************************************
 *
 * Functional description
 *	Get shared cache info.
 *
 **************************************/
HDR	hdr;

db_open (db, strlen (db));
hdr = (HDR) db_read ((SLONG) HEADER_PAGE);

*cache_buffers = hdr->hdr_cache_buffers;
*cache_flags   = hdr->hdr_flags & hdr_disable_cache;
strcpy (name, hdr->hdr_cache_file+2);

page_size = hdr->hdr_page_size;
}

#ifdef APOLLO
static void db_error (
    status_$t	status)
{
/**************************************
 *
 *	d b _ e r r o r   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

error_$print (status);
exit (FINI_ERROR);
}

static void db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
status_$t	status;

map_count = 64;
map_length = 32768; /*page_size * map_count;*/
map_base = 0;

map_region = ms_$mapl (*file_name, file_length, 
	(SLONG) 0, map_length, 
	ms_$cowriters, ms_$wr, (SSHORT) -1,
	map_length, status);

if (status.all)
    db_error (status);
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
status_$t	status;
SLONG		section, length;

if (page_number < map_base ||
    page_number - map_base >= map_count)
    {
    map_base = (page_number / map_count) * map_count;
    section = map_base * page_size;
    length = map_length;
    map_region = ms_$remap (map_region, section, 
	    map_length, length, status);
    if (status.all)
	db_error (status);
    }

return (PAG) (map_region + (page_number - map_base) * page_size);
}
#endif

#ifdef OS2_ONLY
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT		s [128];
SLONG		len;

if (DosGetMessage (0, 0, s, sizeof (s), status, "OSO001.MSG", &len))
    sprintf (s, "unknown OS/2 error %ld", status);

ib_printf (s);
exit (FINI_ERROR);
}

static void db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
ULONG	action, open_mode, status;

open_mode = OPEN_ACCESS_READWRITE |	/* Access mode -- read/write */
	OPEN_SHARE_DENYNONE |		/* Share mode -- permit read/write */
	OPEN_FLAGS_NOINHERIT |		/* Inheritance mode -- don't */
	OPEN_FLAGS_FAIL_ON_ERROR |	/* Fail errors -- return code */
	OPEN_FLAGS_WRITE_THROUGH |	/* Write thru -- absolutely */
	(0 << 15);			/* DASD open -- no */

status = DosOpen (
    file_name,		/* File pathname */
    &file,		/* File handle */
    &action,		/* Action taken */
    100,		/* Initial size */
    FILE_NORMAL,	/* File attributes */
    OPEN_ACTION_OPEN_IF_EXISTS,	/* Open Flag (truncate if file exists) */
    open_mode,		/* Open mode (see above) */
    0L);		/* Reserved */

if (status)
    db_error (status);
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
ULONG	status, new_offset, actual_length;

if (status = DosSetFilePtr (file, (ULONG) (page_number * page_size), FILE_BEGIN, &new_offset))
    db_error (status);

if (status = DosRead (file, global_buffer, page_size, &actual_length))
    db_error (status);
if (actual_length != page_size)
    {
    ib_printf ("Unexpected end of database file.\n");
    exit (FINI_ERROR);
    }

return global_buffer;
}
#endif

#ifdef WIN_NT
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT		s [128];

if (!FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	status,
	GetUserDefaultLangID(),
	s,
	sizeof (s),
	NULL))
    sprintf (s, "unknown Windows NT error %ld", status);

ib_printf (s);
exit (FINI_ERROR);
}

static void db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/

if ((file = CreateFile (file_name,
	GENERIC_READ | GENERIC_WRITE,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_EXISTING,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
	0)) == INVALID_HANDLE_VALUE)
    db_error (GetLastError());
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SLONG	actual_length;

if (SetFilePointer (file, (ULONG) (page_number * page_size), NULL, FILE_BEGIN) == -1)
    db_error (GetLastError());

if (!ReadFile (file, global_buffer, page_size, &actual_length, NULL) ||
    actual_length != page_size)
    db_error (GetLastError());

return global_buffer;
}
#endif

#ifdef mpexl
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SSHORT		l, errcode;
t_mpe_status	code_stat, errmsg_stat;
SLONG		found;
TEXT		s [128];

l = sizeof (s) - 1;
if (status & 0xFFFF)
    {
    code_stat.word = status;
    HPERRMSG (3, 1,, code_stat.decode, s, &l, &errmsg_stat.word);
    found = errmsg_stat.word;
    }
else
    {
    errcode = status >> 16;
    FERRMSG (&errcode, s, &l);
    found = (ccode() == CCE) ? 0 : 1;
    }
if (!found)
    s [l] = 0;
else
    {
    errcode = status >> 16;
    sprintf (s, "uninterpreted MPE/XL code %d from subsystem %d", errcode, (USHORT) status);
    }

ib_printf (s);
exit (FINI_ERROR);
}

static void db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
UCHAR	temp [128], *p;
SLONG	status, domain, access, exclusive, multi, random, buffering,
	disposition, filecode, privlev;

temp [0] = ' ';
for (p = &temp [1]; *p = *file_name++; p++)
    ;
*p = ' ';

domain = DOMAIN_OPEN_PERM;
access = ACCESS_RW;
exclusive = EXCLUSIVE_SHARE;
multi = MULTIREC_YES;
filecode = FILECODE_DB;
privlev = PRIVLEV_DB;
random = WILL_ACC_RANDOM;
buffering = INHIBIT_BUF_YES;
disposition = DISP_KEEP;

GET_PRIVMODE
HPFOPEN (&file, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_ACCESS_TYPE, &access,
    OPN_EXCLUSIVE, &exclusive,
    OPN_MULTIREC, &multi,
    OPN_FILECODE, &filecode,
    OPN_PRIVLEV, &privlev,
    OPN_WILL_ACCESS, &random,
    OPN_INHIBIT_BUF, &buffering,
    OPN_DISP, &disposition,
    ITM_END);
GET_USERMODE

if (status)
    db_error (status);
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SSHORT	errcode;

GET_PRIVMODE
FREADDIR ((SSHORT) file, (SCHAR*) global_buffer, (SSHORT) -page_size,
    page_number * ((page_size - 1) / 1024 + 1));
if (ccode() != CCE)
    {
    FCHECK ((SSHORT) file, &errcode, NULL, NULL, NULL);
    GET_USERMODE
    db_error ((SLONG) errcode << 16);
    }
GET_USERMODE

return global_buffer;
}
#endif

#ifndef APOLLO
#ifndef WIN_NT
#ifndef OS2_ONLY
#ifndef mpexl
static void db_error (
    int		status)
{
/**************************************
 *
 *	d b _ e r r o r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*p;

#ifndef VMS
ib_printf (sys_errlist [status]);
#else
if ((p = strerror (status)) ||
    (p = strerror (EVMSERR, status)))
    ib_printf ("%s\n", p);
else
    ib_printf ("uninterpreted code %x\n", status);
#endif
exit (FINI_ERROR);
}

static void db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/

if ((file = open (file_name, 2)) == -1)
    db_error (errno);
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SCHAR	*p;
SSHORT	length, l;

if (lseek (file, page_number * page_size, 0) == -1)
    db_error (errno);

for (p = (SCHAR*) global_buffer, length = page_size; length > 0;)
    {
    l = read (file, p, length);
    if (l < 0)
	db_error (errno);
    p += l;
    length -= l;
    }

return global_buffer;
}
#endif
#endif
#endif
#endif

static void print_error (void)
{
/**************************************
 *
 *	p r i n t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

ib_printf ("gds_cache_print <switches> <db name>\n");
ib_printf ("Switches are:\n");
ib_printf ("\t-m\t\t- mru order\n");
ib_printf ("\t-n <buffers>\t- number of buffers\n");
ib_printf ("\t-p <page>\t- specific page\n");
ib_printf ("\t-f <buffers>\t- number of free\n");
ib_printf ("\t-h \t\t- page headers\n");
ib_printf ("\t-a \t\t- all except free pages\n");
ib_printf ("\t-u \t\t- user process information\n");
}

static void print_header (
    TEXT	*expanded_filename)
{
/**************************************
 *
 *	p r i n t _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Print shared cache header.
 *
 **************************************/
float		cache_hit;

/* Print shared cache header block */

ib_printf ("\nSHARED_CACHE BLOCK \t%s\n", expanded_filename);
ib_printf ("\tVersion: %d.%d, Active process: %d, Length: %d, Used: %d\n",
	CASH_header->shb_major_version,
	CASH_header->shb_minor_version, 
	CASH_header->shb_active_process,
	CASH_header->shb_length,
	CASH_header->shb_used);

ib_printf ("\tBuffer waits: %d, IO waits: %d, Cache repairs: %d\n",
	CASH_header->shb_buffer_waits,
	CASH_header->shb_io_waits,
	CASH_header->shb_cache_repairs);
	     
ib_printf ("\tLast Checkpoint Seqno: %d, Offset: %d, Partition offset: %d\n",
	CASH_header->shb_last_chk_seqno,
	CASH_header->shb_last_chk_offset,
	CASH_header->shb_last_chk_p_offset);
	     
ib_printf ("\tShort recovers: %d, Long recovers: %d\n",
	CASH_header->shb_short_recovers,
	CASH_header->shb_long_recovers);

ib_printf ("\tLogical Reads: %d, Physical Reads: %d\n",
	CASH_header->shb_logical_reads,
	CASH_header->shb_physical_reads);
	     
ib_printf ("\tLogical Writes: %d, Physical Writes: %d\n",
	CASH_header->shb_logical_writes,
	CASH_header->shb_physical_writes);
	     
cache_hit = (float) (CASH_header->shb_logical_reads - CASH_header->shb_physical_reads) /
		     CASH_header->shb_logical_reads;

ib_printf ("\tCache hit ratio: %3.1f\n", cache_hit * 100.);
ib_printf ("\n");
}

static void print_page_header (
    SDB		sdb)
{
/**************************************
 *
 *	p r i n t _ p a g e _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Print database page header.  Print full page for
 *	header and log pages.
 *
 **************************************/
PAG	page;
SDB     journal_sdb;

if ((page_no >= 0) && (sdb->sdb_page != page_no))
    return;

/* print sdb information */

ib_printf ("\tPage: %ld Generation: %ld Length: %d SDB Flags: %d Precedence: %d\n",
		sdb->sdb_page, sdb->sdb_generation, sdb->sdb_length,
		sdb->sdb_flags, sdb->sdb_precedence);

page = (PAG) ABS_PTR (sdb->sdb_buffer);

/* Print page header */

ib_printf ("\tPage type: %s\n", page_type [(int)page->pag_type]);

ib_printf ("\tFlags: %d.  Generation: %d.  Seqno: %d.  Offset: %d\n",
		(int) page->pag_flags, page->pag_generation,
		page->pag_seqno, page->pag_offset);

/* Print full page information for header and log page */

if (page_no >= 0)
    {
    if (page_no == HEADER_PAGE)
	PPG_print_header (page, HEADER_PAGE, sw_outfile);
    else if (page_no == LOG_PAGE)
	PPG_print_log (page, LOG_PAGE, sw_outfile);
    }

if (sdb->sdb_flags)
    {
    ib_printf ("\tSDB Flags\n");

    if (sdb->sdb_flags & SDB_dirty)
	ib_printf ("\t\tPage Dirty\n");
    if (sdb->sdb_flags & SDB_marked)
	ib_printf ("\t\tPage Marked\n");
    if (sdb->sdb_flags & SDB_faked)
	ib_printf ("\t\tPage Faked\n");
    if (sdb->sdb_flags & SDB_journal)
	ib_printf ("\t\tJournal records present\n");
    if (sdb->sdb_flags & SDB_journal_page)
	ib_printf ("\t\tFull page journal\n");
    if (sdb->sdb_flags & SDB_write_pending)
	ib_printf ("\t\tWrite pending\n");
    if (sdb->sdb_flags & SDB_read_pending)
	ib_printf ("\t\tRead pending\n");
    if (sdb->sdb_flags & SDB_free)
	ib_printf ("\t\tFree\n");
    if (sdb->sdb_flags & SDB_log_recovery)
	ib_printf ("\t\tLog recovery\n");
    if (sdb->sdb_flags & SDB_check_use)
	ib_printf ("\t\tCheck use\n");
    }

if (sdb->sdb_journal)
    {
    journal_sdb = (SDB) ABS_PTR (sdb->sdb_journal);
    ib_printf ("\tJournal buffer information:\n");
    ib_printf ("\t\tCurrent Length %d\n", journal_sdb->sdb_length);
    }

ib_printf ("\n");
}

static void print_process (
    PRB		process)
{
/**************************************
 *
 *	p r i n t _ p r o c e s s
 *
 **************************************
 *
 * Functional description
 *	Print a formatted process block.
 *
 **************************************/

ib_printf ("PROCESS BLOCK %d\n", REL_PTR (process));

ib_printf ("\tProcess id: %d, IO wait: %d, IO read: %d, flags: %d\n", 
    process->prb_process_id, process->prb_waiter,
    process->prb_reader, process->prb_flags);

ib_printf ("\tPhysical Reads: %d, Logical Reads: %d\n",
	process->prb_physical_reads,
	process->prb_logical_reads);

ib_printf ("\tLogical Writes: %d\n", process->prb_logical_writes);

ib_printf ("\tWAL ib_puts: %d\n", process->prb_ail_puts);

ib_printf ("\n");
}

static void prt_que (
    UCHAR	*string,
    SRQ		que,
    void	(*routine)(),
    int		param1,
    int		param2)
{
/**************************************
 *
 *	p r t _ q u e
 *
 **************************************
 *
 * Functional description
 *	Print the contents of a self-relative que.
 *	If a routine is passed, print more information about the
 *	elements of the queue.
 *	param 1 - offset of the que structure within the element we
 *		  are traversing.
 *	param 2	- if specified, the number of entries to print.
 *
 **************************************/
SLONG	count, offset;
SRQ	next;

offset = REL_PTR (que);

if (offset == que->srq_forward && offset == que->srq_backward)
    {
    ib_printf ("%s: *empty*\n\n", string);
    return;
    }

count = 0;

QUE_LOOP ((*que), next)
    {
    ++count;

    if ((param2) && (count > param2))
	continue;

    if (routine)
	(*routine) ((UCHAR*) next-param1);
    }

ib_printf ("%s (%ld):\tforward: %d, backward: %d\n\n", 
    string, count, que->srq_forward, que->srq_backward);
}

static void prt_que_back (
    UCHAR	*string,
    SRQ		que,
    void	(*routine)(),
    int		param1,
    int		param2)
{
/**************************************
 *
 *	p r t _ q u e _ b a c k
 *
 **************************************
 *
 * Functional description
 *	Same as prt_que, but traverse in reverse order.
 *
 **************************************/
SLONG	count, offset;
SRQ	next;

offset = REL_PTR (que);

if (offset == que->srq_forward && offset == que->srq_backward)
    {
    ib_printf ("%s: *empty*\n\n", string);
    return;
    }

count = 0;

QUE_LOOP_BACK ((*que), next)
    {
    ++count;

    if ((param2) && (count > param2))
	continue;

    if (routine)
	(*routine) ((UCHAR*)next-param1);
    }

ib_printf ("%s (%ld):\tforward: %d, backward: %d\n\n", 
    string, count, que->srq_forward, que->srq_backward);
}

static void print_sccb (void)
{
/**************************************
 *
 *	p r i n t _ s c c b
 *
 **************************************
 *
 * Functional description
 *	Print sccb header
 *
 **************************************/

/* Print shared cache header block */

ib_printf ("\tBuffers: %d, Free: %d, Free min: %d, Checkpoint : %d\n",
	sccb->sccb_count,
	sccb->sccb_free_count,
	sccb->sccb_free_min,
	sccb->sccb_checkpoint);

ib_printf ("\tflags: %d\n\n", sccb->sccb_flags);
}
