/*
 *	PROGRAM:	JRD Journalling Subsystem
 *	MODULE:		rebuild.e
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
#include <string.h>
#include <stdlib.h>
#include "../jrd/time.h"

/* MOVE_FAST & MOVE_CLEAR have to be defined before #include common.h */

#define MOVE_FAST(from,to,length)	memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)		memset (to, 0, (int) (length))

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH			512
#endif

#include "../jrd/common.h"

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;

#include "../jrd/ods.h"
#include "../jrd/gds.h"
#include "../jrd/license.h"
#include "../jrd/jrn.h"
#include "../journal/journal.h"
#include "../jrd/thd.h"
#include "../jrd/pio.h"
#include "../wal/wal.h"           
#include "../jrd/dsc.h"           
#include "../jrd/btr.h"           
#include "../jrd/old.h"           
#include "../jrd/llio.h"           
#include "../wal/walr_proto.h"           
#include "../journal/conso_proto.h"
#include "../journal/gjrn_proto.h"
#include "../journal/misc_proto.h"
#include "../journal/oldr_proto.h"
#include "../journal/rebui_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/llio_proto.h"

/* Apollo stuff */

#ifdef APOLLO
#define APOLLO_JOURNALLING
#define SYS_ERROR	gds_arg_domain
#include <apollo/error.h>
#include <sys/errno.h>

#define ERRNO	errno
#endif

/* UNIX Stuff */

#ifdef UNIX
#define UNIX_JOURNALLING
#define LIBRARY_IO
#define SYS_ERROR	gds_arg_unix
#define ERRNO		errno
#include <sys/types.h>

extern int	errno;
#endif

/* VMS Stuff */

#ifdef VMS
#include <rms.h>
#include <iodef.h>
#include <descrip.h>
#include <ssdef.h>
#define SYS_ERROR	gds_arg_vms
#endif

/* NETWARE Stuff */

#ifdef NETWARE_386
#define NETWARE_JOURNALLING
#define LIBRARY_IO
#include <errno.h>
#include <fcntl.h>
#define SYS_ERROR	gds_arg_dos
#define ERRNO		__get_errno_ptr()
#undef PC_PLATFORM
#endif

/* MPE/XL Stuff */

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"
#define SYS_ERROR	gds_arg_mpexl
#endif

/* OS/2 Stuff */

#ifdef OS2_ONLY
#include <sys/timeb.h>
#include <sys/time.h>
#include <errno.h>
#define SYS_ERROR	gds_arg_unix
#define ERRNO		errno
#endif

/* Windows NT Stuff */

#ifdef WIN_NT
#include <sys/types.h>
#include <sys/timeb.h>
#include <winsock.h>
#include <windows.h>
#define TEXT		SCHAR
#define SYS_ERROR	gds_arg_win32
#define ERRNO		GetLastError()
#endif

#define HIGH_WATER(x)		((int) &((DPG) 0)->dpg_rpt [x])
#define MOVE_BYTE(x_from,x_to)	*x_to++ = *x_from++;

DATABASE
    DB = STATIC COMPILETIME FILENAME "journal.gdb" RUNTIME FILENAME journal_dir;
DATABASE
    DB_NEW = STATIC COMPILETIME FILENAME "journal.gdb" RUNTIME FILENAME db_name;

/* Page cache */

typedef struct cache {
    struct cache	*cache_next;
    SSHORT		cache_flags;
    SSHORT		cache_use_count;
    SLONG		cache_page_number;
    ULONG		cache_age;
    PAG			cache_page;
} *CACHE;

/* cache_flags */

#define	PAGE_CLEAN	0
#define	PAGE_DIRTY	1
#define	PAGE_HEADER	2
#define PAGE_CORRUPT	-1

#define	MARK_PAGE(cch)		cch->cache_flags |= PAGE_DIRTY
#define MARK_HEADER(cch)	cch->cache_flags |= PAGE_DIRTY | PAGE_HEADER

/* Database recovery block */

#define CACHE_SIZE	100
#define INITIAL_ALLOC	16

typedef struct drb {
    struct drb	*drb_next;			/* Next database in chain */
    USHORT	drb_page_size;			/* Database page size */
    struct fil	*drb_file;			/* list of file pointers */
    ULONG	drb_max_page;			/* max page read */
    ULONG	drb_age;
    UCHAR	*drb_buffers;
    CACHE	drb_cache;
    TEXT	drb_filename [1];
} *DRB;

extern FILE	*msg_file;

static USHORT	add_file (DRB, FIL, UCHAR *, SSHORT, SLONG, SSHORT);
static void	add_time (struct timeval *, struct timeval *);
static void	apply_data (DRB, DPG, JRND *);
static void	apply_header (HDR, JRND *);
static void	apply_ids (PPG, JRND *);
static void	apply_index (BTR, JRND *);
static void	apply_log (LIP, JRND *);
static void	apply_pip (PIP, JRND *);
static void	apply_pointer (PPG, JRND *);
static void	apply_root (IRT, JRND *);
static void	apply_transaction (TIP, JRND *);
static USHORT	checksum (DRB, PAG);
static void	close_database (DRB);
static BOOLEAN	commit (DRB, LTJC *, USHORT);
static SSHORT	compress (DRB, DPG);
static void	disable (DRB, LTJC *);
static int	error (TEXT *, STATUS, TEXT *, STATUS);
static void	expand_num_alloc (SCHAR ***, SSHORT *);
static void	fixup_header (DRB);
static void	format_time (SLONG [2], TEXT *);
static CACHE	get_free_buffer (DRB, SLONG);
static void	get_log_files (SLONG, SSHORT, SCHAR ***, SLONG **, SSHORT *, SLONG);
static void	get_old_files (SLONG, SCHAR ***, SSHORT *, SLONG *, SLONG *);
static CACHE	get_page (DRB, SLONG, USHORT);
static JRNP	*next_clump (JRND *, void *);
static void	open_all_files (void);
static DRB	open_database (TEXT *, USHORT, SSHORT);
static BOOLEAN	open_database_file (DRB, TEXT *, SSHORT, SLONG *);
static int	open_journal (SCHAR *, SCHAR **, SSHORT, SLONG, SLONG *);
static int	process_online_dump (SCHAR *, SCHAR **, SSHORT);
static int	process_journal (SCHAR *, SCHAR **, SSHORT, SLONG, SLONG, SLONG *);
static SLONG	process_new_file (DRB, JRNF *);
static BOOLEAN	process_old_record (JRND *, USHORT);
static void	process_page (DRB, JRND *, ULONG, ULONG, USHORT);
static BOOLEAN	process_partial (SLONG, SCHAR *);
static BOOLEAN	process_record (JRNH *, USHORT, ULONG, ULONG, USHORT);
static void	quad_move (register UCHAR *, register UCHAR *);
static void	read_page (DRB, CACHE);
static void	rebuild_abort (SLONG);
static void	rebuild_partial (SLONG, SLONG, SCHAR *);
static void	rec_add_clump_entry (DRB, HDR, USHORT, USHORT, UCHAR *);
static BOOLEAN	rec_add_hdr_entry (DRB, USHORT, USHORT, UCHAR *, USHORT);
static BOOLEAN	rec_delete_hdr_entry (DRB, USHORT);
static void	rec_find_space (DRB, CACHE *, HDR *, USHORT, USHORT, UCHAR *);
static BOOLEAN	rec_find_type (DRB, CACHE *, HDR *, USHORT, UCHAR **, UCHAR **);
static BOOLEAN	rec_get_hdr_entry (DRB, USHORT, USHORT *, UCHAR *);
static void	rec_restore (SCHAR *, SCHAR *);
static void	rec_restore_manual (SCHAR *);
static void	release_db (DRB);
static void	release_page (DRB, CACHE);
static FIL	seek_file (DRB, FIL, CACHE, SLONG *);
static FIL	setup_file (UCHAR *, USHORT, int);
static BOOLEAN	test_partial_db (SLONG);
static void	update_rebuild_seqno (SLONG);
static void	write_page (DRB, CACHE);

static DRB	databases;
static USHORT	sw_verbose, sw_debug, sw_interact, sw_disable;
static USHORT	sw_partial, sw_activate, sw_first_recover;
static SSHORT	sw_trace;
static SLONG	until [2];
static SCHAR	journal_dir [MAX_PATH_LENGTH], db_name [MAX_PATH_LENGTH];
static USHORT	sw_journal = FALSE;
static USHORT	sw_db = FALSE;
static UCHAR	sec_file [MAX_PATH_LENGTH];
static SSHORT	sec_added = FALSE;
static SSHORT	sec_len = 0;
static SSHORT	sw_until = 0;
static SCHAR	*partial_db;
static SSHORT	max_seqno;
static SSHORT	dump_id = 0;
static SSHORT	end_reached = FALSE;

/* Time, size statistics of log records processed */

static SLONG	elapsed_time;
static SLONG	bytes_processed;
static SLONG	bytes_applied;
static SLONG	et_sec, et_msec;
static SLONG	*tr1 = 0;

typedef struct counter {
    SLONG	num_recs;
    SLONG	num_bytes;
} COUNTER;

static COUNTER	totals [JRNP_MAX + 1 - JRN_PAGE];

/* WAL defines */

static UCHAR	wal_buff [MAX_WALBUFLEN];
static WALRS	WALR_handle;
static STATUS	wal_status [20];

#ifndef FILE_OPEN_WRITE
#define FILE_OPEN_WRITE 	"w"
#endif

/* if new journal records are added in jrn.h, they should be added here */

static SCHAR *table [] =
    {
    "JRN_PAGE                ",     /* A full page WAL record */
    "JRNP_DATA_SEGMENT       ",     /* Add segment to data page */
    "JRNP_POINTER_SLOT       ",     /* Add pointer page to relation */
    "JRNP_TRANSACTION        ",     /* Journal transaction state */
    "JRNP_NULL               ",     /* Odd byte */
    "JRNP_PIP                ",     /* Page allocation/deallocation */
    "JRNP_BTREE_NODE         ",     /* Add/delete a node to BTREE */
    "JRNP_BTREE_SEGMENT      ",     /* B Tree split - valid part */
    "JRNP_BTREE_DELETE       ",     /* B Tree node delete - logical */
    "JRNP_INDEX_ROOT         ",     /* Add/drop an index    */
    "JRNP_DB_HEADER          ",     /* Modify db header page */
    "JRNP_GENERATOR          ",     /* generator            */
    "JRNP_ROOT_PAGE          ",     /* Index root page      */
    "JRNP_DB_ATTACHMENT      ",     /* next attachment */
    "JRNP_DB_HDR_PAGES       ",     /* Header pages         */
    "JRNP_DB_HDR_FLAGS       ",     /* Header flags */
    "JRNP_DB_HDR_SDW_COUNT   ",     /* Header shadow count */
    "JRNP_LOG_PAGE           ",     /* log page information */
    "JRNP_NEXT_TIP           ",     /* next tip page */
    "JRNP_MAX                "
    };

int REBUILD_start_restore (
    int		argc,
    SCHAR	**argv)
{
/**************************************
 *
 *	R E B U I L D _ s t a r t _ r e s t o r e
 *
 **************************************
 *
 * Functional description
 *	Parse switches and do work.
 *
 **************************************/
UCHAR	*db, string [512];
UCHAR	*journal, jrn [JOURNAL_PATH_LENGTH+1];
SSHORT	i;
SCHAR	*p;
SCHAR	*msg;

/* Start by parsing switches */

sw_trace = sw_disable  = sw_interact = FALSE;
sw_debug = sw_verbose = sw_partial = sw_activate = FALSE;
sw_first_recover = TRUE;

until [0] = until [1] = 0;

for (i = 0; i < (JRNP_MAX-JRN_PAGE); i++)
    {
    totals [i].num_bytes = 0;
    totals [i].num_recs  = 0;
    }

db 	  = NULL; 
databases = NULL; 
journal   = NULL;
partial_db = NULL;

argv++; 
while (--argc > 0)
    {
    if ((*argv) [0] != '-')
	{
	if (db)
	    {
	    GJRN_printf (12, db, NULL, NULL, NULL);
	    exit (FINI_ERROR);
	    }
	db = *argv++;
	continue;
	}

    MISC_down_case (*argv++, string);
    switch (string [1])
	{
	case 'v':
	    sw_verbose = TRUE;
	    break;

	case 'd':
	    sw_verbose = sw_debug = TRUE;
	    break;

	case 't':
	    sw_trace = TRUE;
	    break;

	case 'i':
	    sw_interact = TRUE;
	    break;

	case 'r':
	    break;

        case 'j':
	    if (--argc > 0)
		{
		strcpy (jrn, *argv++);
		journal = jrn;
		}
	    else
		MISC_print_journal_syntax ();
	    break;

        case 'm':
	    if (--argc > 0)
		{
		msg = (SCHAR*) *argv++;
		if (!(msg_file = fopen (msg, FILE_OPEN_WRITE)))
		    rebuild_abort (141);
		}
	    else
		MISC_print_journal_syntax ();
	    break;

	case 'a':
	    sw_activate = TRUE;
	    break;

        case 'p':
	    if (--argc > 0)
		{
		sw_partial = TRUE;
		partial_db = (SCHAR*) *argv++;
		}
	    else
		MISC_print_journal_syntax ();
	    break;

	case 'u':
	    sw_until = TRUE;

	    if (!(p = (SCHAR*) *argv++))
		rebuild_abort (18);

	    argc--;

	    for (i = strlen (p); i ; i--)
		{
		if (p [i-1] == '/')
		    p [i-1] = ' ';
		}

	    if (MISC_time_convert (p, strlen (p), until) == FAILURE)
		rebuild_abort (19);
	    break;

	default:
	    MISC_print_journal_syntax ();
	    break;
	}
    }

if (!journal || !db)
    sw_interact = TRUE;

rec_restore (journal, db);

if (sw_trace)
    {
    GJRN_printf (206, NULL, NULL, NULL, NULL);
    GJRN_printf (207, NULL, NULL, NULL, NULL);

    for (i = 0; i < (JRNP_MAX-JRN_PAGE); i++)
	GJRN_printf (208, table [i], (TEXT*) totals [i].num_recs, (TEXT*) totals [i].num_bytes, NULL);
    }

if (msg_file != stdout)
    fclose (msg_file);
}  

static USHORT add_file (
    DRB		database,
    FIL		main_file,
    UCHAR	*file_name,
    SSHORT	length,
    SLONG	start,
    SSHORT	new_file)
{
/**************************************
 *
 *	a d d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Add a file to an existing database.  Return the sequence
 *	number of the new file.  If anything goes wrong, return a
 *	sequence of 0.
 *
 **************************************/
USHORT	sequence;
FIL	file, next_file;
SLONG	file_handle;

file_name [length] = 0;

if (!(open_database_file (database, file_name, new_file, &file_handle)))
    return 0;

next_file = setup_file (file_name, strlen (file_name), (int)file_handle);

next_file->fil_min_page = start;
sequence = 1;

for (file = main_file; file->fil_next; file = file->fil_next)
    ++sequence;

file->fil_max_page = start - 1;
file->fil_next = next_file;

return sequence;
}

static void add_time (
    struct timeval	*first,
    struct timeval	*next)
{
/**************************************
 *
 *	a d d _ t i m e
 *
 **************************************
 *
 * Functional description
 *	Add time to elapsed time.
 *
 **************************************/

if (first->tv_usec > next->tv_usec) 
    {
    next->tv_usec += 1000000;
    next->tv_sec--;
    }
et_msec += next->tv_usec - first->tv_usec;
et_sec += next->tv_sec - first->tv_sec;
}

static void apply_data (
    DRB		database,
    DPG		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Apply incremental changes to a data page.
 *
 **************************************/
JRNP	temp, *clump;
SSHORT	space, l, top, used;
UCHAR	*p, *q;
struct dpg_repeat	*index, *end;

if (sw_debug)
    GJRN_printf (21, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

/* Process clumps */

for (clump = NULL; clump = next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNP_SIZE);

    if (temp.jrnp_type != JRNP_DATA_SEGMENT)
	{
	GJRN_printf (55, (TEXT*) temp.jrnp_type, NULL, NULL, NULL);
	rebuild_abort (72);
	}

    if (sw_trace)
	continue;

    /* Handle segment deletion */

    if (!temp.jrnp_length)
	{
	index = page->dpg_rpt + temp.jrnp_index;
	index->dpg_offset = 0;
	index->dpg_length = 0;
	}

    /* Re-compute page high water mark */

    index = page->dpg_rpt;
    end = index + page->dpg_count;
    page->dpg_count = 0;
    space = database->drb_page_size;

    for (l = 1, used = 0; index < end; index++, l++)
	if (index->dpg_length)
	    {
	    page->dpg_count = l;
	    space = MIN (space, index->dpg_offset);
	    used += ROUNDUP (index->dpg_length, ODS_ALIGNMENT);
	    }

    if (!temp.jrnp_length)
	continue;
    
    /* Handle segment addition */

    index = page->dpg_rpt + temp.jrnp_index;
    q = clump->jrnp_data;
    l = temp.jrnp_length;

    if (index < end && l <= index->dpg_length)
	{
	index->dpg_length = l;
	p = (UCHAR*) page + index->dpg_offset;
	do *p++ = *q++; while (--l);
	continue;
	}

    page->dpg_count = MAX (page->dpg_count, temp.jrnp_index + 1);
    top = HIGH_WATER (page->dpg_count);
    l = ROUNDUP (l, ODS_ALIGNMENT);
    space -= l;

    if (space < top)
	{
	index->dpg_length = 0;
	space = compress (database, page);
	space -= l;
	if (space < top)
	    rebuild_abort (56);
	}

    if (space + l > database->drb_page_size)
	rebuild_abort (57); 

    index->dpg_offset = space;
    index->dpg_length = temp.jrnp_length;
    p = (UCHAR*) page + space;
    do *p++ = *q++; while (--l);
    }
}

static void apply_header (
    HDR		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Apply changes to database header page 
 *
 **************************************/
JRNDH	temp1, *clump;
JRNDA   temp2;

if (sw_debug)
    GJRN_printf (22, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNDH*) next_clump (record, clump);)
    {
    if (clump->jrndh_type == JRNP_DB_HEADER)
	memcpy ((SCHAR*) &temp1, (SCHAR*) clump, JRNDH_SIZE);
    else
	memcpy ((SCHAR*) &temp2, (SCHAR*) clump, JRNDA_SIZE);

    if ((sw_debug) && (clump->jrndh_type == JRNP_DB_HEADER))
	GJRN_printf (203, (TEXT*) temp1.jrndh_nti, (TEXT*) temp1.jrndh_oat, (TEXT*) temp1.jrndh_oit, NULL);

    if (sw_trace)
	continue;

    switch (clump->jrndh_type)
	{
	case JRNP_DB_HEADER:
	    page->hdr_bumped_transaction = temp1.jrndh_nti;
	    page->hdr_oldest_transaction = temp1.jrndh_oit;
	    page->hdr_oldest_active = temp1.jrndh_oat;
	    break;

	case JRNP_DB_ATTACHMENT:
	    page->hdr_attachment_id = temp2.jrnda_data;
	    break;

	case JRNP_DB_HDR_PAGES:
	    page->hdr_PAGES = temp2.jrnda_data;
	    break;

	case JRNP_DB_HDR_FLAGS:
	    page->hdr_flags = temp2.jrnda_data;
	    break;

	case JRNP_DB_HDR_SDW_COUNT:
	    page->hdr_shadow_count = temp2.jrnda_data;
	    break;

	default:
	    rebuild_abort (58);
	    break;
	}
    }
}

static void apply_ids (
    PPG		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ i d s
 *
 **************************************
 *
 * Functional description
 *	Apply changes to gen ids page
 *
 **************************************/
JRNG	temp, *clump;
SLONG	*ptr;

if (sw_debug)
    GJRN_printf (23, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNG*) next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNG_SIZE);

    if (temp.jrng_type != JRNP_GENERATOR)
	rebuild_abort (59);

    if (sw_debug)
	GJRN_printf (204, (TEXT*) temp.jrng_offset, (TEXT*) temp.jrng_genval, NULL, NULL);

    if (sw_trace)
	continue;

    ptr = page->ppg_page + temp.jrng_offset;
    *ptr = temp.jrng_genval;
    }
}

static void apply_index (
    BTR		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Apply changes to b-tree pages
 *
 **************************************/
JRNB	temp, *clump;
SCHAR    *p, *q;
SLONG	l;
SLONG 	delta;
BTN     node, next;

if (sw_debug)
    GJRN_printf (24, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNB*) next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNB_SIZE);

    if (sw_trace)
	continue;

    switch (clump->jrnb_type)
	{
	case JRNP_BTREE_NODE:
	    if (sw_debug)
		{
		GJRN_printf (25, (TEXT*) temp.jrnb_offset, (TEXT*) temp.jrnb_length, NULL, NULL);
		GJRN_printf (27, (TEXT*) page->btr_length, NULL, NULL, NULL);
		}

	    /* slide down upper part by delta
	       add node and increment btr_length */

	    delta = temp.jrnb_delta;
	    p = (SCHAR*) ((UCHAR *) page + page->btr_length);
	    q = p + delta;
	    if (l = page->btr_length - temp.jrnb_offset)
	        do *--q = *--p; while (--l);

	    /* move in node , next BTN */

	    p = (SCHAR*) ((UCHAR *)page + temp.jrnb_offset);
	    q = (SCHAR*)  clump->jrnb_data;
	    l = temp.jrnb_length;

	    do MOVE_BYTE (q,p) while (--l);

	    page->btr_length += delta;
	    page->btr_prefix_total = temp.jrnb_prefix_total;

	    if (sw_debug)
		{
		GJRN_printf (26, (TEXT*) temp.jrnb_offset, (TEXT*) temp.jrnb_length, NULL, NULL);
		GJRN_printf (27, (TEXT*) page->btr_length, NULL, NULL, NULL);
		}
	    break;

	case JRNP_BTREE_SEGMENT:
	    if (sw_debug)
		{
		GJRN_printf (28, (TEXT*) page->btr_length, NULL, NULL, NULL);
		}

	    /* apply change directly */

	    p = (SCHAR*) page;
	    q = (SCHAR*) clump->jrnb_data;
	    if (l = temp.jrnb_length)
		do MOVE_BYTE (q,p) while (--l);

	    if (sw_debug)
		{
		GJRN_printf (29, (TEXT*) temp.jrnb_length, NULL, NULL, NULL);
		GJRN_printf (27, (TEXT*) page->btr_length, NULL, NULL, NULL);
		}
	    break;

	case JRNP_BTREE_DELETE:

	    /* delete a node entry */

	    node = (BTN)((UCHAR*)page + temp.jrnb_offset);
	    next = (BTN) (BTN_DATA (node) + BTN_LENGTH (node));
	    QUAD_MOVE (BTN_NUMBER (next), BTN_NUMBER (node));
	    p = (SCHAR*) BTN_DATA(node);
	    q = (SCHAR*) BTN_DATA(next);
	    l = BTN_LENGTH(next);
	    if (BTN_PREFIX(node) < BTN_PREFIX(next))
		{
		BTN_LENGTH(node) = BTN_LENGTH(next) + BTN_PREFIX(next) 
						- BTN_PREFIX(node);
		p += BTN_PREFIX(next) - BTN_PREFIX(node);
		}
	    else
		{
		BTN_LENGTH(node) = l;
		BTN_PREFIX(node) = BTN_PREFIX(next);
		}

	    if (l)
		do *p++ = *q++; while (--l);

	    /* Compute length of rest of bucket and move it down. */

	    l = page->btr_length - ((UCHAR*) q - (UCHAR*) page);

	    if (l)
		 do *p++ = *q++; while (--l);

	    page->btr_length = (UCHAR*) p - (UCHAR*) page;
	    page->btr_prefix_total = temp.jrnb_prefix_total;

	    /* Error Check */

	    if (BTN_PREFIX(node) != temp.jrnb_delta)
		{
		rebuild_abort (60);
		}
	    if (page->btr_length != temp.jrnb_length)
		{
		rebuild_abort (61); 
		}

	    if (sw_debug)
		{
		GJRN_printf (30, (TEXT*) temp.jrnb_offset, NULL, NULL, NULL);
		GJRN_printf (27, (TEXT*) page->btr_length, NULL, NULL, NULL);
		}

	    break;

	default:
	    rebuild_abort (72); 
	}
    }
}

static void apply_log (
    LIP         page,
    JRND        *record)
{
/**************************************
 *
 *      a p p l y _ l o g
 *
 **************************************
 *
 * Functional description
 *      Apply changes to database log page
 *
 **************************************/
JRNL    temp, *clump;

if (sw_debug)
    GJRN_printf (86, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNL*) next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNL_SIZE);

    if (sw_trace)
	continue;

    page->log_flags   = temp.jrnl_flags;
    page->log_mod_tid = temp.jrnl_tid;
    page->log_mod_tip = temp.jrnl_tip;
    }
}

static void apply_pip (
    PIP		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ p i p
 *
 **************************************
 *
 * Functional description
 *	Apply changes to page inventory page.
 *
 **************************************/
JRNA	temp, *clump;
UCHAR	bit;
USHORT	byte;

if (sw_debug)
    GJRN_printf (31, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNA*) next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, sizeof (struct jrna));

    if (temp.jrna_type != JRNP_PIP)
	rebuild_abort (63); 

    byte = temp.jrna_slot >> 3;
    bit = 1 << (temp.jrna_slot & 7);

    if (sw_debug)
	{
	if (temp.jrna_allocate)
	    GJRN_printf (183, (TEXT*) temp.jrna_slot, (TEXT*) record->jrnd_page, NULL, NULL);
	else
	    GJRN_printf (184, (TEXT*) temp.jrna_slot, (TEXT*) record->jrnd_page, NULL, NULL);
        }

    if (sw_trace)
	continue;

    if (temp.jrna_allocate)
	{
	page->pip_bits [byte] &= ~bit;
	}
    else
	{
	page->pip_bits [byte] |= bit;
	page->pip_min = MIN (page->pip_min, temp.jrna_slot);
	}
    }
}

static void apply_pointer (
    PPG		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ p o i n t e r
 *
 **************************************
 *
 * Functional description
 *	Apply incremental changes to a pointer page.
 *
 **************************************/
JRNP	temp, *clump;
SLONG	longword;

if (sw_debug)
    GJRN_printf (32, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNP_SIZE);

    if (temp.jrnp_type != JRNP_POINTER_SLOT)
	rebuild_abort (64);

    if (sw_trace)
	continue;

    if (temp.jrnp_length)
	{
	memcpy (&longword, clump->jrnp_data, sizeof (SLONG));
	page->ppg_page [temp.jrnp_index] = longword;
	page->ppg_count = MAX (page->ppg_count, temp.jrnp_index + 1);
	page->ppg_min_space = MIN (page->ppg_min_space, temp.jrnp_index);
	page->ppg_max_space = MAX (page->ppg_min_space, temp.jrnp_index);
	}
    else
	page->ppg_page [temp.jrnp_index] = 0;
    }
}

static void apply_root (
    IRT		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ r o o t
 *
 **************************************
 *
 * Functional description
 *	Apply changes to index root page
 *
 **************************************/
JRNRP	temp, *clump;

if (sw_debug)
    GJRN_printf (33, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

for (clump = NULL; clump = (JRNRP*) next_clump (record, clump);)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNRP_SIZE);

    if (temp.jrnrp_type != JRNP_ROOT_PAGE)
	rebuild_abort (65);

    if (sw_debug)
	GJRN_printf (205, (TEXT*) temp.jrnrp_id, (TEXT*) temp.jrnrp_page, NULL, NULL);

    if (sw_trace)
	continue;

    page->irt_rpt [temp.jrnrp_id].irt_root = temp.jrnrp_page;
    }
}

static void apply_transaction (
    TIP		page,
    JRND	*record)
{
/**************************************
 *
 *	a p p l y _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Apply incremental changes to a TIP page.
 *
 **************************************/
JRNI	*clump, *clump_end;
JRNI	temp;
JRND	rec;

if (sw_debug)
    GJRN_printf (34, (TEXT*) record->jrnd_page, NULL, NULL, NULL);

memcpy ((SCHAR*) &rec, (SCHAR*) record, JRND_SIZE);

clump = (JRNI*) record->jrnd_data;
clump_end = (JRNI*) (record->jrnd_data + rec.jrnd_length);

/* Process clumps */

for (; clump < clump_end; clump++)
    {
    memcpy ((SCHAR*) &temp, (SCHAR*) clump, JRNI_SIZE);

    if (sw_debug)
	{
	if (temp.jrni_type == JRNP_TRANSACTION)
	    GJRN_printf (181, (TEXT*) temp.jrni_transaction, NULL, NULL, NULL);
	else if (temp.jrni_type == JRNP_NEXT_TIP)
	    GJRN_printf (202, (TEXT*) temp.jrni_transaction, NULL, NULL, NULL);
	}

    if (sw_trace)
	{
	/* transaction loop does not use next_clump!! */

	totals [temp.jrni_type - JRN_PAGE].num_recs++;
	totals [temp.jrni_type - JRN_PAGE].num_bytes += JRNI_SIZE;
	continue;
	}

    if (temp.jrni_type == JRNP_TRANSACTION)
	page->tip_transactions [temp.jrni_position] = temp.jrni_states;
    else if (temp.jrni_type == JRNP_NEXT_TIP)
	page->tip_next  = temp.jrni_transaction;
    else
	rebuild_abort (66);
    }
}

static USHORT checksum (
    DRB		database,
    PAG		page)
{
/**************************************
 *
 *	c h e c k s u m
 *
 **************************************
 *
 * Functional description
 *	Compute the checksum of a page.
 *
 **************************************/
ULONG	chksum, *p, *end;
USHORT	old_chksum;

end = (ULONG*) ((SCHAR*) page + database->drb_page_size);
old_chksum = page->pag_checksum;
page->pag_checksum = 0;
p = (ULONG*) page;
chksum = 0;

do
    {
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    chksum += *p++;
    }
while (p < end);

page->pag_checksum = old_chksum;

if (chksum)
    return (USHORT) chksum;
    
/* If the page is all zeros, return an artificial checksum */

for (p = (ULONG*) page; p < end;)
    if (*p++)
	return chksum;

/* Page is all zeros -- invent a checksum */

return 12345;
}

static void close_database (
    DRB		database)
{
/**************************************
 *
 *	c l o s e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	We've reached end of processing this database
 *	Clean up database and close it.
 *
 **************************************/
DRB	*ptr;
CACHE	buffer;
FIL	fil;
SLONG	*tr2 = 0;
SSHORT	ret_val;
STATUS	status [20];

if (sw_trace)
    return;

fixup_header (database);

/* Flush and release cache */

while (buffer = database->drb_cache)
    {
    database->drb_cache = buffer->cache_next;
    if ((buffer->cache_page_number != PAGE_CORRUPT) &&
			(buffer->cache_flags & PAGE_DIRTY))
	write_page (database, buffer);
    MISC_free_jrnl (buffer);
    }

ret_val = SUCCESS;

for (fil = database->drb_file; fil; fil = fil->fil_next)
    {
    if (LLIO_close (status, fil->fil_desc) == FAILURE)
	ret_val = FAILURE;
    }

if (ret_val == FAILURE)
    rebuild_abort (233);

MISC_free_jrnl (database->drb_buffers);

/* Unlink from general data structures */

for (ptr = &databases; *ptr; ptr = &(*ptr)->drb_next)
    if (*ptr == database)
	{
	*ptr = database->drb_next;
	release_db (database);
	break;
	}

if ((sw_partial) && (!sw_activate))
    return;

/* 
 * Just attach to the database and detach.  This will update the
 * log page to have the correct checkpoint information
 */

READY db_name AS DB_NEW
    ON_ERROR
	rebuild_abort (153);
    END_ERROR;

/* cleanup rdb_$log_files for the recovered database */

if (sw_interact || sw_until || sw_partial)
    {
    START_TRANSACTION tr2 USING DB_NEW;

    FOR (TRANSACTION_HANDLE tr2) L IN DB_NEW.RDB$LOG_FILES
	ERASE L;
    END_FOR;
    COMMIT tr2
	ON_ERROR
	    gds__print_status (gds__status);
	    rebuild_abort (0);
	END_ERROR;
    }
FINISH DB_NEW;
}

static BOOLEAN commit (
    DRB		database,
    LTJC	*record,
    USHORT	length)
{
/**************************************
 *
 *	c o m m i t
 *
 **************************************
 *
 * Functional description
 *	A commit is about to happen.  If restore is to be terminated
 *	at a given time, this is a good time to check and stop.
 *
 **************************************/
TEXT	c_time [32];
SCHAR	buf [MSG_LENGTH];
SLONG	date [2];

if (until [0])
    {
    WALR_get_blk_timestamp (WALR_handle, date);
    format_time (date, c_time);
    GJRN_printf (87, c_time, NULL, NULL, NULL);
    if (sw_interact)
	{
	GJRN_get_msg (88, buf, NULL, NULL, NULL);
	if (!MISC_get_line (buf, c_time, sizeof (c_time)) ||
					    UPPER (c_time [0]) != 'Y')
	    {
	    close_database (database);
	    return FALSE;
	    }
	}
    }
else if (sw_verbose)
    {
    WALR_get_blk_timestamp (WALR_handle, date);
    format_time (date, c_time);
    GJRN_printf (87, c_time, NULL, NULL, NULL);
    }

return TRUE;
}

static SSHORT compress (
    DRB		database,
    DPG		page)
{
/**************************************
 *
 *	c o m p r e s s
 *
 **************************************
 *
 * Functional description
 *	Compress a data page.  Return the high water mark.
 *
 **************************************/
register SSHORT	space, l;
UCHAR		temp [MAX_PAGE_SIZE + 4];
register UCHAR	*p, *q;
struct dpg_repeat	*index, *end;

if (database->drb_page_size > sizeof (temp))
    rebuild_abort (67); 

p = (UCHAR*) temp;
q = (UCHAR*) page;
l = database->drb_page_size >> 2;

do {
    *(SLONG*) p = *(SLONG*) q;
    p += sizeof (SLONG);
    q += sizeof (SLONG);
    } while (--l);

space = database->drb_page_size;
end = page->dpg_rpt + page->dpg_count;

for (index = page->dpg_rpt; index < end; index++)
    if (index->dpg_offset && index->dpg_length)
	{
	l = index->dpg_length;
	space -= ROUNDUP (l, ODS_ALIGNMENT);
	q = temp + index->dpg_offset;
	index->dpg_offset = space;
	p = (UCHAR*) page + space;
	if (l) 
	    do *p++ = *q++; while (--l);
	}

return space;
}

static void disable (
    DRB		database,
    LTJC	*record)
{
/**************************************
 *
 *	d i s a b l e
 *
 **************************************
 *
 * Functional description
 *	We're encountered a database disable record.
 *	Clean up database and close it.
 *
 **************************************/
TEXT	ctime [32];
SCHAR	dbname [300], dir_name [300];
SLONG	date [2];

MISC_get_wal_info (record, dbname, dir_name);

WALR_get_blk_timestamp (WALR_handle, date);
format_time (date, ctime);

if (sw_verbose)
    GJRN_printf (35, (TEXT*) record->ltjc_length, dbname, 
			(TEXT*) record->ltjc_header.jrnh_handle, ctime);

sw_disable = 1;
close_database (database);
}

static int error (
    TEXT	*filename,
    STATUS	err_num,
    TEXT	*string,
    STATUS      operation)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	We've had an unexpected error -- punt.
 *
 **************************************/
STATUS	status_vector [20], *s;

s = status_vector;
*s++ = isc_arg_gds;
*s++ = isc_io_error,
*s++ = gds_arg_string;
*s++ = (STATUS) string;
*s++ = gds_arg_string;
*s++ = (STATUS) filename;
*s++ = isc_arg_gds;
*s++ = operation;
*s++ = SYS_ERROR;
*s++ = err_num;
*s = 0;

gds__print_status (status_vector);

return 0;
}

static void expand_num_alloc (
    SCHAR 	***files,
    SSHORT	*num_alloc)
{
/**************************************
 *
 *	e x p a n d _ n u m _ a l l o c
 *
 **************************************
 *
 * Functional description
 *	Expand the size of array
 *
 **************************************/
SCHAR	**temp, **temp1;
SSHORT	size;

size = *num_alloc;
temp1 = *files;

temp = (SCHAR**) MISC_alloc_jrnl (2 * size * sizeof (SLONG));
MOVE_FAST (temp1, temp, size * sizeof (SLONG));

MISC_free_jrnl (temp1);
*num_alloc = 2 * size;
*files = temp;
}

static void fixup_header (
    DRB		database)
{
/**************************************
 *
 *	f i x u p _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Fixup header page to not require recovery
 *
 **************************************/
LIP     page;
CACHE   cch;
SCHAR	*p;
USHORT	len;
HDR     hdr;

if (sw_trace)
    return;
/* 
 * After a restore, the database should be as close to the original
 * database as possible.  The exceptions are when we do a recovery
 * to point on time or when database name or any other information
 * is changed.  In that case, the journal information and all WAL 
 * information must be deleted.
 */

if (sw_interact || sw_until || sw_disable || sw_activate)
    {
    rec_delete_hdr_entry (database, HDR_journal_server);
    rec_delete_hdr_entry (database, HDR_backup_info);
    sw_disable = 0;
    }

if (sec_added)
    {
    rec_add_hdr_entry (database, HDR_file, sec_len, sec_file, CLUMP_REPLACE);
    }

/* Update the next transaction id from the bumped transaction id */
cch     = get_page (database, (SLONG) HEADER_PAGE, TRUE);
MARK_HEADER (cch);
hdr     = (HDR) cch->cache_page;
hdr->hdr_next_transaction = hdr->hdr_bumped_transaction;
release_page (database, cch);

/* Now update the log page */
cch     = get_page (database, (SLONG) LOG_PAGE, TRUE);
MARK_PAGE (cch);
page    = (LIP) cch->cache_page;

if (sw_interact || sw_until || sw_activate)
    {
    p = page->log_data;
    *p++ = LOG_ctrl_file1;
    *p++ = len = CTRL_FILE_LEN;
    do *p++ = 0; while (--len);

    /* Set control point 2 file name */

    *p++ = LOG_ctrl_file2;
    *p++ = len = CTRL_FILE_LEN;
    do *p++ = 0; while (--len);

    /* Set current log file */

    *p++ = LOG_logfile;
    *p++ = len = CTRL_FILE_LEN;
    do *p++ = 0; while (--len);

    *p = LOG_end;

    page->log_flags = log_no_ail;
    page->log_end   = (USHORT) (p - (SCHAR*)page);

    page->log_cp_1.cp_seqno         = 0;
    page->log_cp_1.cp_offset        = 0;
    page->log_cp_1.cp_p_offset      = 0;
    page->log_cp_1.cp_fn_length     = 0;

    page->log_cp_2.cp_seqno         = 0;
    page->log_cp_2.cp_offset        = 0;
    page->log_cp_2.cp_p_offset      = 0;
    page->log_cp_2.cp_fn_length     = 0;

    page->log_file.cp_seqno         = max_seqno;
    page->log_file.cp_offset        = 0;
    page->log_file.cp_p_offset      = 0;
    page->log_file.cp_fn_length     = 0;
    }

page->log_flags &= ~log_recover;

/* At the end of a partial recovery, we will turn off log and
 * set the next seqno.
 */

if ((sw_partial) && (!sw_activate))
    {
    page->log_flags = (log_no_ail | log_partial_rebuild);
    page->log_file.cp_seqno         = max_seqno;
    }

/* Set the log_recovery_done flag, so that a subsequent attachment by 
   the recovery process would not be denied because the shared cache 
   manager is not running. */
if (!(page->log_flags & log_no_ail))
    page->log_flags |= log_recovery_done;
    
release_page (database, cch);
}

static void format_time (
    SLONG	date [2],
    TEXT	*string)
{
/**************************************
 *
 *	f o r m a t _ t i m e
 *
 **************************************
 *
 * Functional description
 *	Format a date/time string.
 *
 **************************************/
struct tm	times;

gds__decode_date (date, &times);

sprintf (string, "%.2d:%.2d:%.2d %.2d/%.2d/%.2d",
    times.tm_hour, times.tm_min, times.tm_sec,
    times.tm_mon + 1, times.tm_mday, times.tm_year);
}

static CACHE get_free_buffer (
    DRB		database,
    SLONG	page)
{
/**************************************
 *
 *	g e t _ f r e e _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Find (or make) available buffer in cache. 
 *
 **************************************/
CACHE		best, buffer;

best = database->drb_cache;

for (buffer = database->drb_cache; buffer; buffer = buffer->cache_next)
    {
    if (buffer->cache_use_count)
	continue;
    if (buffer->cache_age < best->cache_age)
        best = buffer;
    if (buffer->cache_page_number == PAGE_CORRUPT)
	{
        best = buffer;
        break;
	}
    }

if (best->cache_use_count)
    rebuild_abort (73);

if ((best->cache_page_number != PAGE_CORRUPT) && 
		(best->cache_flags & PAGE_DIRTY))
    write_page (database, best);

    /* Unlink from old slot */

MOVE_CLEAR (best->cache_page, database->drb_page_size);
best->cache_page_number = page;

return best;
}

static void get_log_files (
    SLONG	db_id,
    SSHORT	use_archive,
    SCHAR	***file_list,
    SLONG	**p_offset,
    SSHORT	*number,
    SLONG	start_seqno)
{
/**************************************
 *
 *	g e t _ l o g _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	gets a list of files for the recovery start from start_seqno
 *
 **************************************/
SSHORT	num_files;
SSHORT	num_alloc, num_alloc1;
SCHAR	**files;
SLONG	*start_p_offset;

num_files = 0;
num_alloc1 = num_alloc = INITIAL_ALLOC;
files     = (SCHAR**) MISC_alloc_jrnl (num_alloc * sizeof (SLONG));
start_p_offset = (SLONG *) MISC_alloc_jrnl (num_alloc1 * sizeof (SLONG));

/* 
 * Note that we do not allow partition offset.  Partitions are
 * allowed only in raw files, and raw file have to be archived
 */

FOR (TRANSACTION_HANDLE tr1) J IN DB.JOURNAL_FILES WITH 
	    J.FILE_SEQUENCE >= start_seqno AND 
	    J.DB_ID EQ db_id SORTED BY J.FILE_SEQUENCE 

    if (num_files >= num_alloc)
	{
	expand_num_alloc (&files, &num_alloc);
	expand_num_alloc (&start_p_offset, &num_alloc1);
	}

    /* 
     * Stop at the first open file, but make sure there is 
     * atleast one file
     */

    if ((strcmp (J.FILE_STATUS, LOG_CLOSED)) &&
	    (((sw_partial) && (!sw_activate)) || (num_files)))
	break;

    files [num_files] = (SCHAR*) MISC_alloc_jrnl (MAX_PATH_LENGTH);

    if (use_archive)
	{
	if (strcmp (J.ARCHIVE_STATUS, ARCHIVED))
	    {
	    if (!num_files)
		{
		if ((sw_partial) && (!sw_activate))
		    break;

		/* Get partition offset if single file is used */
		strcpy (files [num_files], J.LOG_NAME);
		start_p_offset [num_files] = J.PARTITION_OFFSET;
		num_files++;
		}
	    break;
	    }

	strcpy (files [num_files], J.ARCHIVE_NAME);
	}
    else
	{
	/* Get partition offset if original files are used */
	strcpy (files [num_files], J.LOG_NAME);
	start_p_offset [num_files] = J.PARTITION_OFFSET;
	}

    max_seqno = J.FILE_SEQUENCE;

    num_files++;
END_FOR;

*file_list = files;
*p_offset  = start_p_offset;
*number    = num_files;
}

static void get_old_files (
    SLONG	db_id,
    SCHAR	***file_list,
    SSHORT	*number,
    SLONG	*start_seqno,
    SLONG	*start_offset)
{
/**************************************
 *
 *	g e t _ o l d _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Get a list of old files for this recovery
 *	Returns number of files, start_seqno and offset.
 *
 **************************************/
SSHORT	num_files;
SCHAR	dump_num [10];
SCHAR	buf [MSG_LENGTH];
SSHORT	num_alloc;
SCHAR	**files;
SLONG	end_seqno;

if (sw_interact)
    {
    sprintf (dump_num, "%d", dump_id);
    GJRN_get_msg (94, buf, NULL, NULL, NULL);
    MISC_get_new_value (buf, dump_num, sizeof (dump_num));
    dump_id = atoi (dump_num);

    /* Check if this dump is for this database */

    FOR (TRANSACTION_HANDLE tr1) OD IN DB.ONLINE_DUMP 
					    WITH OD.DUMP_ID EQ dump_id
	if (OD.DB_ID != db_id)
	    rebuild_abort (187);
    END_FOR;
    }

end_seqno = 0;

num_files = 0;
num_alloc = INITIAL_ALLOC;
files     = (SCHAR**) MISC_alloc_jrnl (num_alloc * sizeof (SLONG));

FOR (TRANSACTION_HANDLE tr1) OD IN DB.ONLINE_DUMP_FILES WITH 
		    OD.DUMP_ID EQ dump_id SORTED BY OD.FILE_SEQNO

    if (num_files >= num_alloc)
	expand_num_alloc (&files, &num_alloc);

    files [num_files++] = (SCHAR*) MISC_alloc_jrnl (MAX_PATH_LENGTH);
    strcpy (files [num_files-1], OD.DUMP_FILE_NAME);

END_FOR;

FOR (TRANSACTION_HANDLE tr1) O IN DB.ONLINE_DUMP WITH O.DUMP_ID EQ dump_id
    
    if ((O.START_SEQNO > O.END_SEQNO) || 
	       ((O.START_SEQNO == O.END_SEQNO) && 
		(O.START_OFFSET > O.END_OFFSET)))
	rebuild_abort (51);

    *start_offset = O.START_OFFSET;
    *start_seqno  = O.START_SEQNO;

    end_seqno = O.END_SEQNO;

END_FOR;

/* Check if online dump has been completed */

if ((sw_partial) && (!sw_activate))
    {
    FOR (TRANSACTION_HANDLE tr1) J IN DB.JOURNAL_FILES WITH J.DB_ID EQ db_id
		AND J.FILE_SEQUENCE EQ end_seqno

	if (strcmp (J.FILE_STATUS, LOG_CLOSED))
	    rebuild_abort (148);
    END_FOR;
    }

if (!num_files)
    rebuild_abort (53);

*file_list = files;
*number    = num_files;
}

static CACHE get_page (
    DRB		database,
    SLONG	page,
    USHORT	read_flag)
{
/**************************************
 *
 *	g e t _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Get address of page in cache.  If read_flag is set,
 *	read the page if necessary.
 *
 **************************************/
CACHE	buffer;
PAG	pg;

buffer = NULL;

for (buffer = database->drb_cache; buffer; buffer = buffer->cache_next)
    if (buffer->cache_page_number == page)
	break;

if (!buffer)
    {
    buffer = get_free_buffer (database, page);
    if (read_flag)
	{
	/* stuff something in to make sure that when we read beyond
	 * current end of file and not read any data, we will zero
	 * things out.
	 */

	pg = (PAG) buffer->cache_page;
	pg->pag_checksum = 12345;

	read_page (database, buffer);
	if (pg->pag_checksum != checksum (database, pg))
	    MOVE_CLEAR ((UCHAR*)pg, database->drb_page_size);
	}
    }

buffer->cache_age = ++database->drb_age;

if (buffer->cache_use_count)
    rebuild_abort (68);

buffer->cache_use_count++;

if (page > database->drb_max_page)
    database->drb_max_page = page;

return buffer;
}

static JRNP *next_clump (
    JRND	*record,
    void	*prior)
{
/**************************************
 *
 *	n e x t _ c l u m p
 *
 **************************************
 *
 * Functional description
 *	Given a prior clump, compute the address of the next
 *	clump on a data page.  If the prior clump is null,
 *	compute the address of the first clump.  If we run
 *	off the record, return NULL.
 *
 **************************************/
JRNB	temp1;
USHORT	offset, l;
JRNP	temp;

/* Compute the offset and length of prior clump */

if (prior)
    {
    offset = (SCHAR*) prior - (SCHAR*) record;
    memcpy ((SCHAR*) &temp, (SCHAR*) prior, JRNP_SIZE);
    }
else
    {
    offset = 0;
    memcpy ((SCHAR*) &temp, (SCHAR*) record->jrnd_data, JRNP_SIZE);
    }

switch (temp.jrnp_type)
    {
    case JRNP_DATA_SEGMENT:
    case JRNP_POINTER_SLOT:
	l = JRNP_SIZE + temp.jrnp_length;
	break;
    
    case JRNP_BTREE_SEGMENT:
    case JRNP_BTREE_NODE:

	if (prior)
	    memcpy ((SCHAR*) &temp1, (SCHAR*) prior, JRNB_SIZE);
	else
	    memcpy ((SCHAR*) &temp1, (SCHAR*) record->jrnd_data, JRNB_SIZE);

	l = JRNB_SIZE + temp1.jrnb_length;
	break;

    /*
     * currently DELETE node contains some debug info in jrnb_length
     * but data field is not used.
     */   
    case JRNP_BTREE_DELETE:
	l = JRNB_SIZE;
	break;
    
    case JRNP_PIP:
	l = sizeof (struct jrna);
	break;

    case JRNP_DB_HEADER:
	l = JRNDH_SIZE;
	break;

    case JRNP_LOG_PAGE:
	l = JRNL_SIZE;
	break;

    case JRNP_DB_ATTACHMENT:
    case JRNP_DB_HDR_PAGES:
    case JRNP_DB_HDR_FLAGS:
    case JRNP_DB_HDR_SDW_COUNT:
        l = JRNDA_SIZE;
        break;

    case JRNP_GENERATOR:
	l = JRNG_SIZE;
	break;

    case JRNP_ROOT_PAGE:
	l = JRNRP_SIZE;
	break;
    
    default:
	rebuild_abort (69);
    }

if (sw_trace)
    {
    totals [temp.jrnp_type - JRN_PAGE].num_recs++;
    totals [temp.jrnp_type - JRN_PAGE].num_bytes += l;
    }

/* If the prior pointer is null, just return the data area */

if (!prior)
    return (JRNP*) record->jrnd_data;

offset += l;

if (offset & 1)
    ++offset;

if (offset >= JRND_SIZE + record->jrnd_length)
    return 0;

return (JRNP*) ((SCHAR*) record + offset);
}

static void open_all_files (void)
{
/**************************************
 *
 *	o p e n _ a l l _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Open all secondary files of a database.
 *
 **************************************/
struct cache cch;
DRB	database;
FIL	file;
SCHAR	*file_name;
SSHORT	file_length;
HDR	hdr;
UCHAR	*p;
SLONG	last_page, next_page;
SCHAR	buf [MAX_PATH_LENGTH];
PAG	pg;

if (sw_trace)
    return;

database = databases;
file = database->drb_file;

/* allocate a temp page */

cch.cache_page = (PAG) MISC_alloc_jrnl (MAX_PAGE_SIZE);

for (;;)
    {
    file_name = NULL;
    cch.cache_page_number = file->fil_min_page;
    /*
     * Loop through all the header pages.  Only the primary database
     * page will have overflow header pages.
     */

    do
	{
	read_page (database, &cch);
	pg = (PAG)cch.cache_page;
	if (pg->pag_checksum != checksum (database, pg))
	    rebuild_abort (234);

	hdr = (HDR) cch.cache_page;

        for (p = hdr->hdr_data; *p != HDR_end; p += 2 + p [1])
	    switch (*p)
		{
		case HDR_file:
		    file_length = p [1];
		    file_name = buf;
		    MOVE_FAST (p + 2, buf, file_length);
		    break;

		case HDR_last_page:
		    MOVE_FAST (p + 2, &last_page, sizeof (last_page));
		    break;

		default:
		    ;
		}

	next_page = hdr->hdr_next_page;
	cch.cache_page_number = next_page;
        } while (next_page);

    if (file->fil_min_page)
	file->fil_fudge = 1;

    if (!file_name)
	break;

    add_file (database, database->drb_file, file_name, file_length, last_page + 1, FALSE);
    file = file->fil_next;
    }
MISC_free_jrnl (cch.cache_page);
}

static DRB open_database (
    TEXT	*name,
    USHORT	page_size,
    SSHORT	new_file)
{
/**************************************
 *
 *	o p e n _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Open database and create blocks.
 *
 **************************************/
DRB	database;
UCHAR	*buffers;
CACHE	buffer;
USHORT	n;
SLONG	file_handle;

if (sw_trace)
    return 0;

/* Allocate and partially populate database block */

database = (DRB) MISC_alloc_jrnl (sizeof (struct drb) + strlen (name));
strcpy (database->drb_filename, name);

/* Try to open file */

if (!open_database_file (database, name, new_file, &file_handle))
    rebuild_abort (199);

database->drb_file  = setup_file (name, strlen (name), (int)file_handle);

/* Link database block into world */

database->drb_next = databases;
databases = database;

database->drb_page_size = page_size;

/* Allocate and initialize cache */

buffers = database->drb_buffers = MISC_alloc_jrnl (page_size * CACHE_SIZE);

for (n = 0; n < CACHE_SIZE; n++)
    {
    buffer = (CACHE) MISC_alloc_jrnl (sizeof (struct cache));
    buffer->cache_next = database->drb_cache;
    database->drb_cache = buffer;
    buffer->cache_page_number = PAGE_CORRUPT;
    buffer->cache_page = (PAG) buffers;
    buffers += page_size;
    }

return database;
}

static BOOLEAN open_database_file (
    DRB		database,
    TEXT	*name,
    SSHORT	new_file,
    SLONG	*file_handle)
{
/**************************************
 *
 *	o p e n _ d a t a b a s e _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Open database and create blocks.
 *
 **************************************/
SSHORT 	ret_val;
STATUS	status [20];

if (new_file)
    {
    if (LLIO_open (status, name, LLIO_OPEN_NEW_RW, TRUE, 
		  file_handle) == FAILURE)
	return FALSE;
    }
else
    {
    if (LLIO_open (status, name, LLIO_OPEN_EXISTING_RW, TRUE, 
		  file_handle) == FAILURE)
	return FALSE;
    }

return TRUE;
}

static int open_journal (
    SCHAR	*dbname,
    SCHAR	**files,
    SSHORT	num_files,
    SLONG	start_offset,
    SLONG	*start_p_offset)
{
/**************************************
 *
 *	o p e n _ j o u r n a l
 *
 **************************************
 *
 * Functional description
 *	Prompt the user for a journal file name, and open the file.
 *
 **************************************/
int	i, n;
SCHAR	buf [MAX_PATH_LENGTH];
SSHORT	stop_flag;
SLONG	*until_ptr;

WALR_handle = 0;

if (sw_interact)
    for (i = 0; i < num_files; i++)
	{
	GJRN_get_msg (154, buf, NULL, NULL, NULL);
	MISC_get_new_value (buf, files [i], MAX_PATH_LENGTH);
	}
else if (sw_verbose)
    for (i = 0; i < num_files; i++)
	{
	GJRN_printf (20, files [i], NULL, NULL, NULL);
	}

stop_flag = (sw_partial && !sw_activate) ? TRUE : FALSE;
until_ptr = sw_until ? until : 0;

n = WALR_open (wal_status, &WALR_handle, dbname, num_files, files, 
		start_p_offset, start_offset, until_ptr, stop_flag);

if (n != SUCCESS)
    {
    error (files [0], (STATUS) ERRNO, "open", isc_io_open_err);
    return 0;
    }

return n;
}

static int process_online_dump (
    SCHAR	*dbname,
    SCHAR 	**files,
    SSHORT	num_files)
{
/**************************************
 *
 *	p r o c e s s _ o n l i n e _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Process a journal file until end of file.  
 *
 **************************************/
OLD		OLD_handle;
SLONG		ret_val;
SSHORT		len;
SSHORT		count;
SCHAR		buf [MSG_LENGTH];
struct timeval	first, next;

if (sw_verbose)
    GJRN_printf (37, NULL, NULL, NULL, NULL);

OLD_handle = 0;

/*
 * OLD_get () returns
 *	 0  	- OK
 *	OLD_EOD	- end of dump
 *	OLD_EOF	- EOF
 *      OLD_ERR	- Error
 */
count = 0;

while (TRUE)
    {
    if (sw_interact)
	{
	GJRN_get_msg (89, buf, NULL, NULL, NULL);
	MISC_get_new_value (buf, files [count], MAX_PATH_LENGTH);
	}

    if (sw_verbose)
	GJRN_printf (191, files [count], NULL, NULL, NULL);

    count++;

    if (OLDR_open (&OLD_handle, dbname, num_files, files) == FAILURE)
	return FAILURE;

    /* get start time */

    MISC_get_time (&first);

    while (TRUE)
	{
	ret_val	= OLDR_get (OLD_handle, wal_buff, &len);

	if (ret_val)
	    break;
    
	process_old_record ((JRND *)wal_buff, len);
	}
    /* get finish time */

    MISC_get_time (&next);

    add_time (&first, &next);

    if (ret_val == OLD_EOD)
	{
	ret_val = 0;
	break;
	}
    if (ret_val == OLD_ERR)
	{
	ret_val = -1;
	break;
	}
    }

OLDR_close (&OLD_handle);
OLD_handle = 0;

if (sw_verbose)
    GJRN_printf (38, NULL, NULL, NULL, NULL);

return ret_val;
}

static int process_journal (
    SCHAR	*dbname,
    SCHAR	**files,
    SSHORT	num_files,
    SLONG	start_seqno,
    SLONG	start_offset,
    SLONG	*start_p_offset)
{
/**************************************
 *
 *	p r o c e s s _ j o u r n a l
 *
 **************************************
 *
 * Functional description
 *	Process a journal file until end of file.  
 *
 **************************************/
USHORT		len;
SLONG		ret_val;
ULONG		seqno;
ULONG		offset;
struct timeval	first, next;
#ifdef DEV_BUILD
JRND    rec1;
SLONG    rec_checksum;
#endif

if (sw_verbose)
    GJRN_printf (39, NULL, NULL, NULL, NULL);

if ((open_journal (dbname, files, num_files, start_offset, start_p_offset)) < 0)
    return FAILURE;

seqno = offset = 0;

/* get start time */

MISC_get_time (&first);

max_seqno = start_seqno;

while (TRUE)
    {
    ret_val = WALR_get (wal_status, WALR_handle, wal_buff, &len, &seqno, 
			&offset);

    if (ret_val == -1)
	{
	ret_val = 0;
	break;
	}
    
    if (ret_val != SUCCESS)
	{
	ret_val = -1;
	break;
	}
    
    /* This the maximum seqno which is processed */

    max_seqno = seqno;

#ifdef DEV_BUILD
    /* take care of word alignment problems */
    MOVE_FAST ((SCHAR*) wal_buff, (SCHAR*) &rec1, JRND_SIZE);
    rec_checksum = rec1.jrnd_header.jrnh_series;
    rec1.jrnd_header.jrnh_series = 0;
    MOVE_FAST ((SCHAR*) &rec1, (SCHAR*) wal_buff, JRND_SIZE);

    if (rec_checksum != MISC_checksum_log_rec (wal_buff, len, 0, 0))
	rebuild_abort (214);

    rec1.jrnd_header.jrnh_series = rec_checksum;
    MOVE_FAST ((SCHAR*) &rec1, (SCHAR*) wal_buff, JRND_SIZE);
#endif
    /* Will return FALSE when JRN_DISABLE is encountered */

    if (!(ret_val = process_record (wal_buff, len, seqno, offset, 1)))
	break;
    }

/* get end time  and increment stats */

MISC_get_time (&next);

add_time (&first, &next);

if ((!end_reached) && (sw_first_recover))
    if (!sw_trace)
	rebuild_abort (148);

if (sw_verbose)
    GJRN_printf (40, NULL, NULL, NULL, NULL);

if (!(sw_interact || sw_until))
    WALR_fixup_log_header (wal_status, WALR_handle);
    
WALR_close (wal_status, &WALR_handle);

return ret_val;
}

static SLONG process_new_file (
    DRB		database,
    JRNF	*record)
{
/**************************************
 *
 *	p r o c e s s _ n e w _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Add a file to the current database.  
 *
 **************************************/
FIL	file, next;
HDR	header;
SLONG	sequence;
TEXT	*file_name;
SLONG	start;
CACHE	cch;
SSHORT	file_len;
TEXT	new_name [MAX_PATH_LENGTH];
SCHAR	buf [MSG_LENGTH];

start = record->jrnf_start;
file_name = record->jrnf_filename;
file_len = record->jrnf_length;

memcpy (new_name, file_name, file_len);
new_name[file_len] = 0;

if (sw_interact)
    {
    GJRN_get_msg (90, buf, NULL, NULL, NULL);
    MISC_get_new_value (buf, new_name, sizeof (new_name));
    file_name = new_name;
    file_len  = strlen (new_name);
    }

if (sw_verbose)
    GJRN_printf (180, file_name, (TEXT*) record->jrnf_sequence, NULL, NULL);

if (sw_trace)
    return 0;

/* Find current last file */

for (file = database->drb_file; file->fil_next; file = file->fil_next)
    ;

/* Create the file.  If the sequence number comes back zero, it didn't
   work, so punt */

if (!(sequence = add_file (database, database->drb_file, file_name, 
			file_len, start, TRUE)))
    rebuild_abort (232);

if (record->jrnf_sequence != sequence)
    rebuild_abort (71);

/* Create header page for new file */

next = file->fil_next;

cch 	= get_page (database, next->fil_min_page, FALSE);
header 	= (HDR) cch->cache_page;
header->hdr_header.pag_type = pag_header;
header->hdr_sequence = sequence;
header->hdr_page_size = database->drb_page_size;
header->hdr_data [0] = HDR_end;
next->fil_sequence = sequence;

MARK_HEADER (cch);
release_page (database, cch);
next->fil_fudge = 1;

/* Update the previous header page to point to new file */
/* Header page can be updated only by setting fil_fudge to 0 */

file->fil_fudge = 0;

cch	= get_page (database, file->fil_min_page, TRUE);
header  = (HDR) cch->cache_page;

/* Add clumplets for file name, starting page number */

if (!file->fil_min_page)
    {
    /* 
     * This will happen when the second file is added.  The primary
     * file header page is journalled when the file is added and that
     * record will be encountered before this new_file record is
     * added.  This code is just an error check
     */
    sec_added = TRUE;
    sec_len   = file_len;
    memcpy (sec_file, file_name, file_len);
    sec_file [sec_len] = 0;
    }
else
    {
    --start;
    rec_add_clump_entry (database, header, HDR_file, strlen (file_name), 
				file_name);
    rec_add_clump_entry (database, header, HDR_last_page, sizeof (start), 
				(UCHAR*)&start);
    }
    
MARK_HEADER (cch);
release_page (database, cch);

/* i.e if it is not the first file */

if (file->fil_min_page)
    file->fil_fudge = 1;

return sequence;
}

static BOOLEAN process_old_record (
    JRND	*record,
    USHORT	length)
{
/**************************************
 *
 *	p r o c e s s _ o l d _ r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Process a single online dump record record.  
 *	Generate a seqno/offset pair for data pages
 *
 **************************************/
PAG	page;
ULONG	seqno;
ULONG	offset;

if (record->jrnd_header.jrnh_type == JRN_PAGE)
    {
    page = (PAG)record->jrnd_data;
    offset = page->pag_offset;
    seqno  = page->pag_seqno;
    return process_record (record, length, seqno, offset, 0);
    }

offset = -1;
seqno  = -1;

return process_record (record, length, seqno, offset, 0);
}

static void process_page (
    DRB		database,
    JRND	*record,
    ULONG	seqno,
    ULONG	offset,
    USHORT	wal_flag)
{
/**************************************
 *
 *	p r o c e s s _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Update a database page.
 *	wal_flag indicates whether it is done for wal record or old record
 *
 **************************************/
PAG	page;
JRNP	*clump;
UCHAR	*p, *q;
CACHE	cch;
JRND	rec;

clump = (JRNP*) record->jrnd_data;
q = (UCHAR*) clump;

/* wal_flag is always true for clumps */

memcpy ((SCHAR*) &rec, (SCHAR*) record, JRND_SIZE);

/* Print out info */

if (sw_debug)
    GJRN_printf (182, (TEXT*) rec.jrnd_page, (TEXT*) seqno, (TEXT*) offset, NULL);

/* Handle zero length record.  This is an off shoot of PAG_release_page () */

if (rec.jrnd_length == 0)
    return;

/* Collect statistics */

bytes_processed += (rec.jrnd_length + JRND_SIZE);

if (!sw_trace)
    {
    cch  = get_page (database, rec.jrnd_page, wal_flag);
    page = cch->cache_page;
    }

if ((wal_flag) && (!sw_trace))
    {
    /* handle freed page problem.  Ignore clumps for pages
     * which are empty.  This may have to be fixed later
     */

    if (clump->jrnp_type > pag_max)
	{
	if ((page->pag_generation == 0) && (page->pag_seqno == 0) && (page->pag_offset == 0))
	    {
	    release_page (database, cch);
	    return;
	    }
	}

    if ((page->pag_seqno > seqno) ||
	((page->pag_seqno == seqno) && (page->pag_offset >= offset)))
	{
	release_page (database, cch);
	if (sw_debug)
	    {
	    if (clump->jrnp_type <= pag_max)
		GJRN_printf (41, (TEXT*) rec.jrnd_page, NULL, NULL, NULL);
	    else
		GJRN_printf (43, (TEXT*) rec.jrnd_page, NULL, NULL, NULL);
	    }
	return;
	}
    /* 
     * Error check to see if records are being skipped
     * Do not check for error if it is a full page, or clump type
     * is JRNP_BTREE_SEGMENT and the previous seqno & offset are 0
     */

    if (!((rec.jrnd_header.jrnh_prev_seqno == 0) && 
	       (rec.jrnd_header.jrnh_prev_offset == 0) &&
	       ((clump->jrnp_type <= pag_max) || (clump->jrnp_type == JRNP_BTREE_SEGMENT))))
	{
	if ((page->pag_seqno != rec.jrnd_header.jrnh_prev_seqno) ||
	    (page->pag_offset != rec.jrnd_header.jrnh_prev_offset))
	    {
	    release_page (database, cch);
	    rebuild_abort (140);
	    }
	}
    }

/* Collect statistics */

bytes_applied += (rec.jrnd_length + JRND_SIZE);

if (!sw_trace)
    MARK_PAGE (cch);

/* If record is a full replacement page, handle it */

if (clump->jrnp_type <= pag_max)
    {
    if (sw_debug)
	GJRN_printf (42, (TEXT*) rec.jrnd_page, NULL, NULL, NULL);

    if (sw_trace)
	{
	totals [0].num_recs++;
	totals [0].num_bytes += rec.jrnd_length;
	}
    else
	{
	p = (UCHAR*)page;
	memcpy (p, q, database->drb_page_size);
	page->pag_seqno  = seqno;
	page->pag_offset = offset;
	release_page (database, cch);
	}
    return;
    }

/* 
 * If we are doing a trace, there is no page, and we will just follow
 * the journal record type.  Else there is an error check.
 */

if (sw_trace)
    {
    switch (clump->jrnp_type)
	{
	case JRNP_POINTER_SLOT:
	    apply_pointer (page, record);
	    break;
	    
	case JRNP_DATA_SEGMENT:
	    apply_data (database, page, record);
	    break;
	    
	case JRNP_TRANSACTION:
	case JRNP_NEXT_TIP:
	    apply_transaction (page, record);
	    break;
	    
	case JRNP_PIP:
	    apply_pip (page, record);
	    break;
	case JRNP_DB_HEADER:
	case JRNP_DB_ATTACHMENT:
	case JRNP_DB_HDR_PAGES:
	case JRNP_DB_HDR_FLAGS:
	case JRNP_DB_HDR_SDW_COUNT:
	    apply_header (page, record);
	    break;
	case JRNP_ROOT_PAGE:
	    apply_root (page, record);
	    break;

	case JRNP_BTREE_NODE:
	case JRNP_BTREE_SEGMENT:
	case JRNP_BTREE_DELETE:
	    apply_index (page, record);
	    break;
	case  JRNP_GENERATOR:
	    apply_ids (page, record);
	    break;
	case JRNP_LOG_PAGE:
	    apply_log (page, record);
	    break;

	default:
	    if (sw_debug)
		GJRN_printf (44, (TEXT*) rec.jrnd_page, NULL, NULL, NULL);
	    rebuild_abort (72);
	}
    }
else
    {
    /* This allows some more error checking */

    switch (page->pag_type)
	{
	case pag_pointer:
	    apply_pointer (page, record);
	    break;
	    
	case pag_data:
	    apply_data (database, page, record);
	    break;
	    
	case pag_transactions:
	    apply_transaction (page, record);
	    break;
	    
	case pag_pages:
	    apply_pip (page, record);
	    break;
	case pag_header:
	    apply_header (page, record);
	    break;
	case pag_root:
	    apply_root (page, record);
	    break;
	case pag_index:
	    apply_index (page, record);
	    break;
	case pag_ids:
	    apply_ids (page, record);
	    break;
	case pag_log:
	    apply_log (page, record);
	    break;

	default:
	    if (sw_debug)
		GJRN_printf (44, (TEXT*) rec.jrnd_page, NULL, NULL, NULL);
	    rebuild_abort (72);
	}
    }

if (!sw_trace)
    {
    page->pag_seqno  = seqno;
    page->pag_offset = offset;
    page->pag_generation++;
    release_page (database, cch);
    }
}

static BOOLEAN process_partial (
    SLONG	db_id,
    SCHAR	*old_db_name)
{
/**************************************
 *
 *	p r o c e s s _ p a r t i a l
 *
 **************************************
 *
 * Functional description
 *	Processes a partial recovery.  If it is the first
 *	pass, store a record and return.  Let the general
 *	recovery code handle it
 *
 **************************************/
SCHAR	buf [MSG_LENGTH];
BOOLEAN	found;

found = FALSE;

if (!sw_partial)
    return found;

/* This is partial recovery - check if value exists in 
 * relation.  If not add one.
 */

if (!partial_db)
    rebuild_abort (195);

/* Use the partial_db_name as the new db_name */

strcpy (db_name, partial_db);

FOR (TRANSACTION_HANDLE tr1) P IN DB.PARTIAL_REBUILDS WITH
	P.DB_ID EQ db_id AND P.NEW_DB_NAME EQ partial_db
    if (P.LAST_LOG_SEQ == 0)
	{
	if (sw_interact)
	    {
	    GJRN_get_msg (192, P.NEW_DB_NAME, NULL, NULL, NULL);
	    if ((!MISC_get_line (buf,  buf, sizeof (buf))) ||
			    (UPPER (buf [0]) != 'Y'))
		rebuild_abort (193); /* Invalid entry */
	    ERASE P
		ON_ERROR
		    rebuild_abort (194);
		END_ERROR;
	    SAVE tr1
		ON_ERROR
		    rebuild_abort (194);
		END_ERROR;
	    break;
	    }
	else
	    rebuild_abort (193);
	}

    found = TRUE;
    sw_first_recover = FALSE;

    /* This will recovery the database fully */

    if (sw_verbose)
	GJRN_printf (201, partial_db, NULL, NULL, NULL);

    rebuild_partial (P.DB_ID, P.LAST_LOG_SEQ, old_db_name);

END_FOR;

if (found)
    return found;

/* Add entry to PARTIAL_REBUILDS */

STORE (TRANSACTION_HANDLE tr1) P IN DB.PARTIAL_REBUILDS
    P.DB_ID = db_id;
    strcpy (P.NEW_DB_NAME, partial_db);
    P.LAST_LOG_SEQ = 0;
END_STORE
    ON_ERROR
	rebuild_abort (194);
    END_ERROR;

if (sw_verbose)
    GJRN_printf (200, partial_db, NULL, NULL, NULL);

return found;
}

static BOOLEAN process_record (
    JRNH	*record,
    USHORT	length,
    ULONG	seqno,
    ULONG	offset,
    USHORT	wal_flag)
{
/**************************************
 *
 *	p r o c e s s _ r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Process a single journal record.  
 *	return FALSE when JRN_DISBALE is encountered.
 *
 **************************************/
DRB	database;
LTJW	*rec;

/* Handle particular record type */

database = databases;

switch (record->jrnh_type)
    {
    case JRN_PAGE:
	process_page (database, record, seqno, offset, wal_flag);
	break;
    
    case JRN_ENABLE:
	break;
    
    case JRN_NEW_FILE:
	process_new_file (database, (JRNF *)record);
	break;
    
    case JRN_DISABLE:
	disable (database, record);
	return FALSE;
    
    case JRN_SYNC:
	break;
    
    case JRN_CNTRL_PT:
	break;
    
    case JRN_COMMIT:
	return commit (database, record, length);
	break;

    case JRN_START_ONLINE_DMP:
	if (sw_debug)
	    GJRN_printf (45, (TEXT*) seqno, (TEXT*) offset, NULL, NULL);
	break;

    case JRN_END_ONLINE_DMP:

	/* Check if we reached the end of online dump */

	rec = (LTJW *) record;

	if (rec->ltjw_seqno == dump_id)
	    end_reached = TRUE;

	if (sw_debug)
	    GJRN_printf (46, (TEXT*) seqno, (TEXT*) offset, NULL, NULL);
	break;

    default:
	GJRN_printf (45, (TEXT*) record->jrnh_type, NULL, NULL, NULL);
	rebuild_abort (0);
    }

return TRUE;
}

static void quad_move (
    register UCHAR	*a,
    register UCHAR	*b)
{
/**************************************
 *
 *      q u a d _ m o v e
 *
 **************************************
 *
 * Functional description
 *      Move an unaligned longword ( 4 bytes).
 *
 **************************************/
MOVE_BYTE (a, b);
MOVE_BYTE (a, b);
MOVE_BYTE (a, b);
MOVE_BYTE (a, b);
}

static void read_page (
    DRB		database,
    CACHE	buffer)
{
/**************************************
 *
 *	r e a d _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
FIL	fil;
STATUS	status [20];
SLONG	len_read, new_offset;

fil = seek_file (database, database->drb_file, buffer, &new_offset);

if (!fil)
    rebuild_abort (48);

if (LLIO_read (status, fil->fil_desc, fil->fil_string, new_offset, 
	      LLIO_SEEK_NONE, buffer->cache_page, database->drb_page_size, 
	      &len_read) == FAILURE)
    {
    gds__print_status (status);
    rebuild_abort (234);
    }

if ((len_read != database->drb_page_size) && (len_read != 0))
    rebuild_abort (234);
}

static void rebuild_abort (
    SLONG	err_num)
{
/**************************************
 *
 *	r e b u i l d _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	Cleanup and exit
 *
 **************************************/
FIL	fil;
DRB	ptr;

if (!(sw_partial))
    for (ptr = databases; ptr; ptr = ptr->drb_next)
	{
	for (fil = ptr->drb_file; fil; fil = fil->fil_next)
	    unlink (fil->fil_string);
	}

if (tr1)
    ROLLBACK tr1
	ON_ERROR
	END_ERROR;

if (DB)
    FINISH DB
	ON_ERROR
	END_ERROR;

GJRN_abort (err_num);
}

static void rebuild_partial (
    SLONG	db_id,
    SLONG	last_log_seq,
    SCHAR	*old_db)
{
/**************************************
 *
 *	r e b u i l d _ p a r t i a l
 *
 **************************************
 *
 * Functional description
 *	Roll a partially rebuild database forward from a journal.
 *
 **************************************/
SLONG	*start_p_offset;
SCHAR	**files;
SSHORT	num_files;
SSHORT	use_archive;


/* statistics */

bytes_processed = bytes_applied = 0;
elapsed_time = et_sec = et_msec = 0; 

FOR (TRANSACTION_HANDLE tr1) Y IN DB.DATABASES WITH Y.DB_ID EQ db_id

    use_archive = (strcmp (Y.ARCHIVE_MODE, NEED_ARCH) == 0);

    open_database (db_name, Y.PAGE_SIZE, FALSE);
    open_all_files ();

    if (!test_partial_db (last_log_seq))
	rebuild_abort (198);
    /* 
     * Note that we do not allow partition offset.  Partitions are
     * allowed only in raw files, and raw file have to be archived
     */

    get_log_files (Y.DB_ID, use_archive, &files, &start_p_offset, &num_files, last_log_seq + 1);

    if (!num_files)
        break;		/* no work to be done */

    if (process_journal (Y.DB_NAME, files, num_files, 
		       last_log_seq + 1, 0, start_p_offset))
	rebuild_abort (54);

    for (;num_files; num_files--)
	MISC_free_jrnl (files [num_files-1]);

    MISC_free_jrnl (files);
    MISC_free_jrnl (start_p_offset);

    break;		/* recover one database at a time */
END_FOR;

if ((max_seqno > last_log_seq) || (sw_activate))
    update_rebuild_seqno (db_id);
}

static void rec_add_clump_entry (
    DRB		database,
    HDR		header,
    USHORT	type,
    USHORT	len,
    UCHAR	*entry)
{
/***********************************************
 *
 *	r e c _ a d d _ c l u m p _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *	Adds an entry to header page.
 *
 **************************************/
UCHAR 	*q, *p;
int	free_space;

q = entry;

for (p = header->hdr_data; ((*p != HDR_end) && (*p != type)); p += 2 + p [1])
    ;

/* We are at HDR_end, add the entry */

free_space = database->drb_page_size - header->hdr_end;

if (free_space > (2 + len))
    {
    *p++ = type;
    *p++ = len; 

    if (len)
	do *p++ = *q++; while (--len);

    *p++ = HDR_end;
    header->hdr_end = p - (UCHAR*)header;
    }
}

static BOOLEAN rec_add_hdr_entry (
    DRB		database,
    USHORT	type,
    USHORT	len,
    UCHAR	*entry,
    USHORT	mode)
{
/***********************************************
 *
 *	r e c _ a d d  _ h d r _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *	Adds an entry to header page.
 *	mode
 *		0 - add			CLUMP_ADD
 *		1 - replace		CLUMP_REPLACE
 *		2 - replace only!	CLUMP_REPLACE_ONLY
 *	returns
 *		TRUE - modified page
 *		FALSE - nothing done
 *
 *
 **************************************/
HDR	header;
UCHAR 	*q, *p;
int	found;
CACHE	cch;

cch	= get_page (database, HEADER_PAGE, TRUE);
header 	= (HDR)cch->cache_page;

if (mode != CLUMP_ADD)
    {
    found = rec_find_type (database, &cch, &header, type, &q, &p);

    /* If we did'nt find it and it is REPLACE_ONLY, return */

    if (!found && mode == CLUMP_REPLACE_ONLY)
        {
	release_page (database, cch);
        return FALSE;
        }
    if (found)
	{
        if (q [1] == len)
	    {
	    q += 2;
	    p = entry;
	    if (len)
		{
	        do *q++ = *p++; while (--len);
		MARK_HEADER (cch);
		}

	    release_page (database, cch);
	    return TRUE;
	    }
	release_page (database, cch);
        rec_delete_hdr_entry (database, type);
        return rec_add_hdr_entry (database, type, len, entry, CLUMP_ADD);
	}

    /* !found && REPLACE will fall through */

    if (cch->cache_page_number != HEADER_PAGE)
	{
        release_page (database, cch);
        cch	= get_page (database, HEADER_PAGE, TRUE);
        header 	= (HDR)cch->cache_page;
	}
    }

/* Add the entry */

rec_find_space (database, &cch, &header, type, len, entry);
MARK_HEADER (cch);
release_page (database, cch);

return TRUE;
}

static BOOLEAN rec_delete_hdr_entry (
    DRB		database,
    USHORT	type)
{
/***********************************************
 *
 *	r e c _ d e l e t e _ h d r _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *	Gets rid on the entry 'type' from header page.
 *
 **************************************/
HDR	header;
UCHAR 	*q, *p, *r;
USHORT	l;
CACHE	cch;

cch	= get_page (database, HEADER_PAGE, TRUE);
header 	= (HDR)cch->cache_page;

if (!rec_find_type (database, &cch, &header, type, &q, &p))
    {
    release_page (database, cch);
    return FALSE;
    }

header->hdr_end -=  (2 + q [1]);

r = q + 2 + q [1];
l = p - r + 1;

if (l)
    do *q++ = *r++; while (--l);

MARK_HEADER (cch);
release_page (database, cch);

return TRUE;
}

static void rec_find_space (
    DRB		database,
    CACHE	*cch_p,
    HDR		*hdr,
    USHORT	type,
    USHORT	len,
    UCHAR	*entry)
{
/***********************************************
 *
 *	r e c _ f i n d _ s p a c e
 *
 ***********************************************
 *
 * Functional description
 *		TRUE  - Found it
 *		FALSE - Not present
 *	Note: We cannot allocate space.  It should have been 
 *	      allocated before.
 *
 **************************************/
UCHAR 	*p, *ptr;
SLONG	next_page;
HDR	header;
SLONG	free_space;
CACHE	cch;

header = *hdr;
cch    = *cch_p;
ptr = entry;

while (TRUE)
    {

    p = (UCHAR*)header + header->hdr_end;

	/* We are at HDR_end, add the entry */

    free_space = database->drb_page_size - header->hdr_end;

    if (free_space > (2 + len))
	{
	 /* Space available, shuffle other entries down */

	*p++ = type;
	*p++ = len;
	if (len)
	    do *p++ = *ptr++; while (--len);

	*p = HDR_end;
	header->hdr_end = p - (UCHAR*)header;
	*cch_p = cch;
	*hdr   = header;
	return;
	}

    next_page = header->hdr_next_page;

    if (!next_page)
        break;

    release_page (database, cch);
    cch	= get_page (database, next_page, TRUE);
    header 	= (HDR)cch->cache_page;
    }

rebuild_abort (50);
}

static BOOLEAN rec_find_type (
    DRB		database,
    CACHE	*cch_p,
    HDR		*hdr,
    USHORT	type,
    UCHAR	**t_ptr,
    UCHAR	**l_ptr)
{
/***********************************************
 *
 *	r e c _ f i n d _ t y p e
 *
 ***********************************************
 *
 * Functional description
 *		TRUE  - Found it
 *		FALSE - Not present
 *
 **************************************/
UCHAR 	*q, *p;
SLONG	next_page;
CACHE	cch;
HDR	header;

q = 0;
cch    = *cch_p;
header = *hdr;

while (TRUE)
    {
    for (p = header->hdr_data; (*p != HDR_end); p += 2 + p [1])
	{
	if (*p == type)
	    q = p;
	}

    if (q)
	{
	*t_ptr = q;
	*l_ptr = p;
	*cch_p = cch;
	*hdr   = header;
	return TRUE;
	}

    /* Follow chain of header pages */

    next_page = header->hdr_next_page;

    if (next_page)
	{
	release_page (database, cch);
	cch	= get_page (database, next_page, TRUE);
	header 	= (HDR)cch->cache_page;
	}
    else
	{
	*hdr   = header;
	*cch_p = cch;
	return FALSE;
	}
    }
}

static BOOLEAN rec_get_hdr_entry (
    DRB		database,
    USHORT	type,
    USHORT	*len,
    UCHAR	*entry)
{
/***********************************************
 *
 *	r e c _ g e t _ h e a d e r _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *		TRUE  - Found it
 *		FALSE - Not present
 *		***!!! NOT USED !!!***
 *
 **************************************/
UCHAR 	*q, *p, *r;
USHORT	l;
HDR	header;
CACHE	cch;

cch	= get_page (database, HEADER_PAGE, TRUE);
header 	= (HDR)cch->cache_page;

q = entry;

if (!rec_find_type (database, &cch, &header, type, &p, &r))
    {
    release_page (database, cch);
    return FALSE;
    }

l = *(p+1);
*len = l;
p += 2;

if (l)
    do *q++ = *p++; while (--l);

release_page (database, cch);

return TRUE;
}

static void rec_restore (
    SCHAR 	*journal_name,
    SCHAR 	*dbn)
{
/**************************************
 *
 *	r e c _ r e s t o r e
 *
 **************************************
 *
 * Functional description
 *	Roll a database forward from a journal.
 *
 **************************************/
SLONG	start_seqno;
SLONG	start_offset;
SLONG	*start_p_offset;
SCHAR	**files;
SSHORT	num_files;
int	size;
SCHAR	buf [MSG_LENGTH];
SSHORT	use_archive;

if (journal_name)
    {
    sw_journal = TRUE; 
    strcpy (journal_dir, journal_name);
    }

if (dbn)
    {
    sw_db	= TRUE; 
    ISC_expand_filename (dbn, 0, db_name);
    dbn = db_name;
    }

/* statistics */

bytes_processed = bytes_applied = 0;
elapsed_time = et_sec = et_msec = 0; 

max_seqno = 0;

/* Scan journal looking for database to recover */

if ((sw_interact) && (!journal_name))
    {
    GJRN_get_msg (91, buf, NULL, NULL, NULL);
    if (!MISC_get_line (buf, journal_dir, sizeof (journal_dir)))
	{
	rec_restore_manual (dbn);
	return;
	}
    }

/* 
 * If running recovery journal server needs to be running if database
 * is fully recovered 
 */

if (!(sw_interact || sw_until || sw_partial ))
    {
    if (!(CONSOLE_test_server (journal_dir)))
	rebuild_abort (213);
    }

size = strlen (journal_dir);
if (!size)
    rebuild_abort (15);

if (journal_dir [size-1] == '/')
    size--;
#if (defined WIN_NT || defined OS2_ONLY)
else if (journal_dir [size-1] == '\\')
    size--;
#endif
strcpy (journal_dir + size, "/journal.gdb");

READY journal_dir AS DB
    ON_ERROR
	rebuild_abort (152);
    END_ERROR;

START_TRANSACTION tr1 USING DB;

FOR (TRANSACTION_HANDLE tr1) X IN DB.DATABASES WITH X.STATUS EQ DB_ACTIVE

    if ((sw_db) && (!sw_interact))
	{
	if (strcmp (db_name, X.DB_NAME))
	    continue;
	}
    else
	{
	GJRN_get_msg (92, buf, X.DB_NAME, NULL, NULL);
        if (!MISC_get_line (buf,  db_name, sizeof (db_name)))
	    continue;

        if ((UPPER (db_name[0])) != 'Y')
	    continue;

        strcpy (db_name, X.DB_NAME);
	}

    use_archive = (strcmp (X.ARCHIVE_MODE, NEED_ARCH) == 0);

    if (sw_partial)
	if (process_partial (X.DB_ID, X.DB_NAME))
	    break;

    if ((sw_interact) && (!sw_partial))
	{
	GJRN_get_msg (93, buf, NULL, NULL, NULL);
	MISC_get_new_value (buf, db_name, sizeof (db_name));
	}

    open_database (db_name, X.PAGE_SIZE, TRUE);

    dump_id   = X.LAST_DUMP_ID;

    get_old_files (X.DB_ID, &files, &num_files, &start_seqno, &start_offset);

    if (process_online_dump (X.DB_NAME, files, num_files) < 0)
	rebuild_abort (52);

    for (;num_files; num_files--)
	MISC_free_jrnl (files [num_files-1]);
    MISC_free_jrnl (files);

    /* 
     * Note that we do not allow partition offset.  Partitions are
     * allowed only in raw files, and raw file have to be archived
     */

    get_log_files (X.DB_ID, use_archive, &files, &start_p_offset, &num_files, start_seqno);

    if (!num_files)
	rebuild_abort (17);

    if (process_journal (X.DB_NAME, files, num_files, 
		       start_seqno, start_offset, start_p_offset))
	rebuild_abort (54);

    if ((!end_reached) && (sw_first_recover))
	if (!sw_trace)
	    rebuild_abort (148);

    for (;num_files; num_files--)
	MISC_free_jrnl (files [num_files-1]);

    MISC_free_jrnl (files);
    MISC_free_jrnl (start_p_offset);

    update_rebuild_seqno (X.DB_ID);

    break;		/* recover one database at a time */
END_FOR;

if ((sw_db) && (!databases) && (!sw_trace))
    {
    if (!((sw_disable) || (sw_interact)))
	rebuild_abort (185);
    }

/* Print out stats */

elapsed_time = et_sec + et_msec/1000000;

if ((sw_verbose) || (sw_trace))
    {
    GJRN_printf (155, NULL, NULL, NULL, NULL);
    GJRN_printf (156, (TEXT*) bytes_processed, NULL, NULL, NULL);
    GJRN_printf (157, (TEXT*) bytes_applied, NULL, NULL, NULL);
    GJRN_printf (158, (TEXT*) elapsed_time, NULL, NULL, NULL);
    }

while (databases)
    close_database (databases);

COMMIT tr1;
FINISH DB;
}

static void rec_restore_manual (
    SCHAR 	*dbn)
{
/**************************************
 *
 *	r e c _ r e s t o r e _ m a n u a l
 *
 **************************************
 *
 * Functional description
 *	Roll a database forward from a journal manual
 *
 **************************************/
SLONG	start_seqno;
SLONG	start_offset;
SLONG	*start_p_offset;
SCHAR	**files;
SCHAR	*msg;
SSHORT	num_files;
SCHAR	buf [MSG_LENGTH];
SSHORT	num_alloc;
OLD     OLD_handle = 0;
SCHAR	old_db_name [300];
SSHORT	page_size;

if (!dbn)
    {
    GJRN_get_msg (189, old_db_name, NULL, NULL, NULL);
    if (!MISC_get_line (old_db_name, db_name, sizeof (db_name)))
	rebuild_abort (185);
    }

strcpy (old_db_name, db_name);

if ((sw_interact) && (!sw_trace))
    {
    GJRN_get_msg (93, buf, NULL, NULL, NULL);
    MISC_get_new_value (buf, db_name, sizeof (db_name));
    }

/* get online dump information */

GJRN_get_msg (4, buf, NULL, NULL, NULL);
GJRN_output ("%s\n", buf);

num_files = 0;
num_alloc = INITIAL_ALLOC;
files     = (SCHAR**) MISC_alloc_jrnl (num_alloc * sizeof (SLONG));

while (TRUE)
    {
    msg = (SCHAR*) MISC_alloc_jrnl (MAX_PATH_LENGTH);
    GJRN_get_msg (6, msg, NULL, NULL, NULL);
    MISC_get_line (msg, msg, MAX_PATH_LENGTH);
    if (!strlen (msg))
	{
	MISC_free_jrnl (msg);
	break;
	}
    files [num_files++] = msg;
    if (num_files >= num_alloc)
	expand_num_alloc (&files, &num_alloc);
    }

if (!num_files)
    {
    if (sw_trace)
	{
	if (sw_debug)
	    GJRN_printf (209, NULL, NULL, NULL, NULL);
	}
    else
	rebuild_abort (53);

    page_size = MIN_PAGE_SIZE;
    }
else
    {
    /* Need to get page size before starting online dump */

    if (OLDR_open (&OLD_handle, old_db_name, num_files, files) == FAILURE)
	rebuild_abort (187);

    OLDR_get_info (OLD_handle, &page_size, &dump_id, &start_seqno, 
		   &start_offset, &start_p_offset);

    OLDR_close (&OLD_handle);
    }

if (sw_verbose)
    GJRN_printf (190, db_name, (TEXT*) page_size, NULL, NULL);

open_database (db_name, page_size, TRUE);

if (num_files)
    {
    if (process_online_dump (old_db_name, files, num_files) < 0)
	rebuild_abort (52);
    }

for (;num_files; num_files--)
    MISC_free_jrnl (files [num_files-1]);
MISC_free_jrnl (files);

/* Get log file information */

GJRN_get_msg (188, buf, NULL, NULL, NULL);
GJRN_output ("%s\n", buf);

num_files = 0;
num_alloc = INITIAL_ALLOC;
files     = (SCHAR**) MISC_alloc_jrnl (num_alloc * sizeof (SLONG));

while (TRUE)
    {
    msg = (SCHAR*) MISC_alloc_jrnl (MAX_PATH_LENGTH);
    GJRN_get_msg (6, msg, NULL, NULL, NULL);
    MISC_get_line (msg, msg, MAX_PATH_LENGTH);
    if (!strlen (msg))
	{
	MISC_free_jrnl (msg);
	break;
	}
    files [num_files++] = msg;

    if (num_files >= num_alloc)
	expand_num_alloc (&files, &num_alloc);
    }

if (!num_files)
    rebuild_abort (17);

start_p_offset = (SLONG *) MISC_alloc_jrnl (num_files * sizeof (SLONG));

if (process_journal (old_db_name, files, num_files, 
		   start_seqno, start_offset, start_p_offset))
    rebuild_abort (54);

if (!end_reached)
    if (!sw_trace)
	rebuild_abort (148);

elapsed_time = et_sec + et_msec/1000000;

if ((sw_verbose) || (sw_trace))
    {
    GJRN_printf (155, NULL, NULL, NULL, NULL);
    GJRN_printf (156, (TEXT*) bytes_processed, NULL, NULL, NULL);
    GJRN_printf (157, (TEXT*) bytes_applied, NULL, NULL, NULL);
    GJRN_printf (158, (TEXT*) elapsed_time, NULL, NULL, NULL);
    }

for (;num_files; num_files--)
    MISC_free_jrnl (files [num_files-1]);

MISC_free_jrnl (files);
MISC_free_jrnl (start_p_offset);

while (databases)
    close_database (databases);
}

static void release_db (
    DRB		drb)
{
/**************************************
 *
 *	r e l e a s e _ d b
 *
 **************************************
 *
 * Functional description
 *	Free up the database and all file blocks.
 *
 **************************************/
FIL	fil;

if (!drb)
    return;

while (drb->drb_file)
    {
    fil = drb->drb_file;
    drb->drb_file = fil->fil_next;
    MISC_free_jrnl (fil);
    }

MISC_free_jrnl (drb);
}

static void release_page (
    DRB		database,
    CACHE	buffer)
{
/**************************************
 *
 *	r e l e a s e _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Release a page.
 *
 **************************************/

buffer->cache_use_count--;

if (buffer->cache_use_count)
    rebuild_abort (68); 

if ((buffer->cache_page_number != HEADER_PAGE) && 
		(buffer->cache_flags & PAGE_HEADER))
    {
    write_page (database, buffer);
    buffer->cache_page_number = PAGE_CORRUPT;
    buffer->cache_flags = 0;
    }
}

static FIL seek_file (
    DRB		database,
    FIL		fil,
    CACHE	buffer,
    SLONG	*new_offset)
{
/**************************************
 *
 *	s e e k _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Given a buffer descriptor block, find the appropropriate
 *	file block and seek to the proper page in that file.
 *
 **************************************/
ULONG	page;
STATUS	status [20];

page = buffer->cache_page_number;

for (;; fil = fil->fil_next)
    if (!fil)
	rebuild_abort (74);
    else if (page >= fil->fil_min_page && page <= fil->fil_max_page)
	break;

page -= (fil->fil_min_page - fil->fil_fudge);

*new_offset = page * database->drb_page_size;

if (LLIO_seek (status, fil->fil_desc, fil->fil_string, *new_offset,
	       LLIO_SEEK_BEGIN) == FAILURE)
    {
    gds__print_status (status);
    return (FIL) NULL;
    }

return fil;
}

static FIL setup_file (
    UCHAR	*file_name,
    USHORT	file_length,
    int		desc)
{
/**************************************
 *
 *	s e t u p _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Set up file descriptor. 
 *
 **************************************/
FIL		file;

/* Allocate file block and copy file name string */

file = (FIL) MISC_alloc_jrnl (sizeof (struct fil) + file_length+1);
file->fil_desc = desc;
file->fil_length = file_length;
file->fil_max_page = -1;
MOVE_FAST (file_name, file->fil_string, file_length);
file->fil_string [file_length] = '\0';

return file;
}

static BOOLEAN test_partial_db (
    SLONG	expected_seqno)
{
/**************************************
 *
 *	t e s t _ p a r t i a l _ d b
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
LIP     page;
CACHE   cch;
BOOLEAN	ret;
DRB	database;

database = databases;

cch     = get_page (database, LOG_PAGE, TRUE);
page    = (LIP) cch->cache_page;

/* At the end of a partial recovery, we will turn off log and
 * set the next seqno.
 */

ret = FALSE;

if ((page->log_flags == (log_no_ail | log_partial_rebuild)) && 
			(page->log_file.cp_seqno == expected_seqno))
    ret = TRUE;

release_page (database, cch);

return ret;
}

static void update_rebuild_seqno (
    SLONG	db_id)
{
/**************************************
 *
 *	u p d a t e _ r e b u i l d _ s e q n o
 *
 **************************************
 *
 * Functional description
 *	Updates the highest seqno of partial recovery.
 *	If the database is being activated, delete the entry.
 *
 **************************************/

if (sw_partial)
    {
    if (sw_activate)
	{
	FOR (TRANSACTION_HANDLE tr1) P IN DB.PARTIAL_REBUILDS WITH
		P.DB_ID EQ db_id AND P.NEW_DB_NAME EQ partial_db
	    ERASE P
		ON_ERROR
		    rebuild_abort (194);
		END_ERROR;
	END_FOR;
	}
    else
	{
	FOR (TRANSACTION_HANDLE tr1) P IN DB.PARTIAL_REBUILDS WITH
		P.DB_ID EQ db_id AND P.NEW_DB_NAME EQ partial_db
	    MODIFY P
		P.LAST_LOG_SEQ = max_seqno;
	    END_MODIFY
		ON_ERROR
		    rebuild_abort (194);
		END_ERROR;
	END_FOR;
	}
    }

/* Do a commit retain to save changes */

SAVE tr1
    ON_ERROR
	rebuild_abort (194);
    END_ERROR;
}

static void write_page (
    DRB		database,
    CACHE	buffer)
{
/**************************************
 *
 *	w r i t e _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Write a database page.
 *
 **************************************/
PAG	page;
FIL	fil;
SLONG	len_written, new_offset;
STATUS	status [20];

page = buffer->cache_page;
page->pag_checksum = checksum (database, page);
fil = seek_file (database, database->drb_file, buffer, &new_offset);

if (!fil)
    rebuild_abort (49);

if (LLIO_write (status, fil->fil_desc, fil->fil_string, new_offset,
		LLIO_SEEK_NONE,  buffer->cache_page, database->drb_page_size,
		&len_written) == FAILURE)
    {
    gds__print_status (status);
    rebuild_abort (49);
    }

if (len_written != database->drb_page_size)
    rebuild_abort (49);
}
