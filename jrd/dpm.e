/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dpm.e
 *	DESCRIPTION:	Data page manager
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
#include "../jrd/ibsetjmp.h"
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/req.h"
#include "../jrd/gds.h"
#include "../jrd/sqz.h"
#include "../jrd/irq.h"
#include "../jrd/blb.h"
#include "../jrd/tra.h"
#include "../jrd/lls.h"
#include "../jrd/lck.h"
#include "../jrd/cch.h"
#include "../jrd/jrn.h"
#include "../jrd/pag.h"
#include "../jrd/sbm.h"
#include "../jrd/rse.h"
#ifdef VIO_DEBUG
#include "../jrd/vio_debug.h"
#endif
#include "../jrd/all_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/sbm_proto.h"
#include "../jrd/sqz_proto.h"
#include "../jrd/thd_proto.h"

DATABASE
    DB = FILENAME "ODS.RDB";

#define DECOMPOSE(n, divisor, q, r) {r = n % divisor; q = n / divisor;}
#define DECOMPOSE_QUOTIENT(n, divisor, q) {q = n / divisor;}
#define HIGH_WATER(x)	((SSHORT) sizeof (struct dpg) + (SSHORT) sizeof (struct dpg_repeat) * (x - 1))
#define SPACE_FUDGE	RHDF_SIZE

static void	delete_tail (TDBB, RHDF, USHORT);
static void	fragment (TDBB, RPB *, SSHORT, DCC, SSHORT, TRA);
static void	extend_relation (TDBB, REL, WIN *);
static UCHAR	*find_space (TDBB, RPB *, SSHORT, LLS *, REC, USHORT);
static BOOLEAN	get_header (WIN *, SSHORT, register RPB *);
static PPG	get_pointer_page (TDBB, REL, WIN *, USHORT, USHORT);
static void	journal_segment (TDBB, WIN *, USHORT);
static RHD	locate_space (TDBB, register RPB *, SSHORT, LLS *, REC, USHORT);
static void	mark_full (TDBB, register RPB *);
static void	release_dcc (DCC);
static void	store_big_record (TDBB, register RPB *, LLS *, DCC, USHORT);

PAG DPM_allocate (
    TDBB	tdbb,
    WIN		*window)
{
/**************************************
 *
 *	D P M _ a l l o c a t e
 *
 **************************************
 *
 * Functional description
 *	Allocate (and set up for journalling) a data page.
 *
 **************************************/
DBB	dbb;
PAG	page;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("DPM_allocate (window page %d)\n", window ? window->win_page : 0);
#endif

page = PAG_allocate (window);

if (dbb->dbb_wal)
    CCH_journal_page (tdbb, window);

return page;
}

void DPM_backout (
    TDBB	tdbb,
    RPB		*rpb)
{
/**************************************
 *
 *	D P M _ b a c k o u t
 *
 **************************************
 *
 * Functional description
 *	Backout a record where the record and previous version are on
 *	the same page.
 *
 **************************************/
DBB		dbb;
DPG		page;
RHD		header;
USHORT		n;
struct dpg_repeat *index1, *index2;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_backout (rpb %d)\n", rpb->rpb_number);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif

CCH_MARK (tdbb, &rpb->rpb_window);
page = (DPG) rpb->rpb_window.win_buffer;
index1 = page->dpg_rpt + rpb->rpb_line;
index2 = page->dpg_rpt + rpb->rpb_b_line;
*index1 = *index2;
index2->dpg_offset = index2->dpg_length = 0;

header = (RHD) ((SCHAR*) page + index1->dpg_offset);
header->rhd_flags &= ~(rhd_chain | rhd_gc_active);

if (dbb->dbb_wal)
    {
    journal_segment (tdbb, &rpb->rpb_window, rpb->rpb_b_line);
    journal_segment (tdbb, &rpb->rpb_window, rpb->rpb_line);
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    old record %d:%d, new record %d:%d, old dpg_count %d, ", rpb->rpb_page, rpb->rpb_line, rpb->rpb_b_page, rpb->rpb_b_line, page->dpg_count);
#endif

/* Check to see if the index got shorter */

for (n = page->dpg_count; --n;)
    if (page->dpg_rpt [n].dpg_length)
	break;

page->dpg_count = n + 1;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    new dpg_count %d\n", page->dpg_count);
#endif

CCH_RELEASE (tdbb, &rpb->rpb_window);
}

int DPM_chain (
    TDBB	tdbb,
    RPB		*org_rpb,
    RPB		*new_rpb)
{
/**************************************
 *
 *	D P M _ c h a i n
 *
 **************************************
 *
 * Functional description
 *	Start here with a plausible, but non-active RPB. 
 *
 *	We need to create a new version of a record.  If the new version
 *	fits on the same page as the old record, things are simple and
 *	quick.  If not, return FALSE and let somebody else suffer.
 *
 *	Note that we also return FALSE if the record fetched doesn't
 *	match the state of the input rpb, or if there is no record for
 *	that record number.  The caller has to check the results to 
 *	see what failed if FALSE is returned.  At the moment, there is
 *	only one caller, VIO_erase. 
 *
 **************************************/
DBB		dbb;
RHD		header;
RPB		temp;
DPG		page;
struct dcc	dcc;
UCHAR		*p;
SLONG		length, fill;  /* Accomodate max record size i.e. 64K */
USHORT		size;
SSHORT		top, offset, n, slot, available, space;
struct dpg_repeat *index, *end;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_chain (org_rpb %d, new_rpb %d)\n", org_rpb->rpb_number, new_rpb ? new_rpb->rpb_number : 0);
if (debug_flag > DEBUG_WRITES_INFO)
    {
    ib_printf ("    org record  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	org_rpb->rpb_page, org_rpb->rpb_line, org_rpb->rpb_transaction, org_rpb->rpb_b_page,
	org_rpb->rpb_b_line, org_rpb->rpb_f_page, org_rpb->rpb_f_line, org_rpb->rpb_flags);

    if (new_rpb)
	ib_printf ("    new record length %d transaction %d flags %d\n", 
	    new_rpb->rpb_length, new_rpb->rpb_transaction, new_rpb->rpb_flags);
    }
#endif

temp = *org_rpb;
size = SQZ_length (tdbb, new_rpb->rpb_address, (int) new_rpb->rpb_length, &dcc);

if (!DPM_get (tdbb, org_rpb, LCK_write))
    {
#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    record not found in DPM_chain\n"); 
#endif
    release_dcc (dcc.dcc_next);
    return FALSE;
    }

/* if somebody has modified the record since we looked last, stop now! */

if (temp.rpb_transaction != org_rpb->rpb_transaction ||
    temp.rpb_b_page != org_rpb->rpb_b_page ||
    temp.rpb_b_line != org_rpb->rpb_b_line)
    {
    CCH_RELEASE (tdbb, &org_rpb->rpb_window);
#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    record changed in DPM_chain\n"); 
#endif
    release_dcc (dcc.dcc_next);
    return FALSE;
    }


if ((org_rpb->rpb_flags & rpb_delta) && temp.rpb_prior)
    org_rpb->rpb_prior = temp.rpb_prior;
else if (org_rpb->rpb_flags & rpb_delta)
    {
    CCH_RELEASE (tdbb, &org_rpb->rpb_window);
#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    record delta state changed\n"); 
#endif
    release_dcc (dcc.dcc_next);
    return FALSE;
    }  

page = (DPG) org_rpb->rpb_window.win_buffer;

/* If the record obviously isn't going to fit, don't even try */

if (size > dbb->dbb_page_size - (sizeof (struct dpg) + RHD_SIZE))
    {
    CCH_RELEASE (tdbb, &org_rpb->rpb_window);
#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    insufficient room found in DPM_chain\n"); 
#endif
    release_dcc (dcc.dcc_next);
    return FALSE;
    }

/* The record must be long enough to permit fragmentation later.  If it's
   too small, compute the number of pad bytes required */

fill = (RHDF_SIZE - RHD_SIZE) - size;
if (fill < 0 || (new_rpb->rpb_flags & rpb_deleted))
    fill = 0;

length = ROUNDUP (RHD_SIZE + size + fill, ODS_ALIGNMENT);

/* Find space on page and open slot */

slot = page->dpg_count;
space = dbb->dbb_page_size;
top = HIGH_WATER (page->dpg_count);
available = dbb->dbb_page_size - top;

for (index = page->dpg_rpt, end = index + page->dpg_count, n = 0;
     index < end; index++, n++)
    {
    if (!index->dpg_length && slot == page->dpg_count)
	slot = n;
    if (index->dpg_length &&
	(offset = index->dpg_offset))
	{
	available -= ROUNDUP (index->dpg_length, ODS_ALIGNMENT);
	space = MIN (space, offset);
	}
    }

if (slot == page->dpg_count)
    {
    top += sizeof (struct dpg_repeat);
    available -= sizeof (struct dpg_repeat);
    }

/* If the record doesn't fit, punt */

if (length > available)
    {
    CCH_RELEASE (tdbb, &org_rpb->rpb_window);
#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    compressed page doesn't have room in DPM_chain\n"); 
#endif
    release_dcc (dcc.dcc_next);
    return FALSE;
    }

CCH_precedence (tdbb, &org_rpb->rpb_window, -org_rpb->rpb_transaction);
CCH_MARK (tdbb, &org_rpb->rpb_window);

/* Record fits, in theory.  Check to see if the page needs compression */

space -= length;
if (space < top)
    space = DPM_compress (tdbb, page) - length;

if (slot == page->dpg_count)
    ++page->dpg_count;

/* Swap the old record into the new slot and the new record into
   the old slot */

new_rpb->rpb_b_page = new_rpb->rpb_page = org_rpb->rpb_page;
new_rpb->rpb_b_line = slot;
new_rpb->rpb_line = org_rpb->rpb_line;

index = page->dpg_rpt + org_rpb->rpb_line;
header = (RHD) ((SCHAR*) page + index->dpg_offset);
header->rhd_flags |= rhd_chain;
page->dpg_rpt [slot] = *index;

index->dpg_offset = space;
index->dpg_length = RHD_SIZE + size + fill;

header = (RHD) ((SCHAR*) page + space);
header->rhd_flags = new_rpb->rpb_flags;
header->rhd_transaction = new_rpb->rpb_transaction;
header->rhd_format = new_rpb->rpb_format_number;
header->rhd_b_page = new_rpb->rpb_b_page;
header->rhd_b_line = new_rpb->rpb_b_line;

SQZ_fast (&dcc, new_rpb->rpb_address, header->rhd_data);
release_dcc (dcc.dcc_next);

if (fill)
    {
    p = header->rhd_data + size;
    do *p++ = 0; while (--fill);
    }

if (dbb->dbb_wal)
    {
    journal_segment (tdbb, &org_rpb->rpb_window, org_rpb->rpb_line);
    journal_segment (tdbb, &org_rpb->rpb_window, new_rpb->rpb_b_line);
    }

CCH_RELEASE (tdbb, &org_rpb->rpb_window);

return TRUE;
}

int DPM_compress (
    TDBB	tdbb,
    DPG		page)
{
/**************************************
 *
 *	D P M _ c o m p r e s s
 *
 **************************************
 *
 * Functional description
 *	Compress a data page.  Return the high water mark.
 *
 **************************************/
DBB		dbb;
register SSHORT	space, l;
#if (defined PC_PLATFORM && !defined NETWARE_386)
UCHAR		*temp_page = NULL;
#else
UCHAR		temp_page[MAX_PAGE_SIZE];
#endif
struct dpg_repeat *index, *end;
JMP_BUF		env, *old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("compress (page)\n");
if (debug_flag > DEBUG_TRACE_ALL_INFO)
    ib_printf ("    sequence %d\n", page->dpg_sequence);
#endif

#if (defined PC_PLATFORM && !defined NETWARE_386)
/* allocate a spare buffer which is large enough,
   and set up to release it in case of error */

temp_page = (UCHAR*) ALL_malloc ((SLONG) dbb->dbb_page_size, ERR_jmp);
old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (temp_page)
        ALL_free (temp_page);
    ERR_punt();
    }
#else
if (dbb->dbb_page_size > sizeof (temp_page))
    BUGCHECK (250); /* msg 250 temporary page buffer too small */
#endif

space = dbb->dbb_page_size;
end = page->dpg_rpt + page->dpg_count;

for (index = page->dpg_rpt; index < end; index++)
    if (index->dpg_offset)
	{
	l = index->dpg_length;
	space -= ROUNDUP (l, ODS_ALIGNMENT);
	MOVE_FAST ((UCHAR*) page + index->dpg_offset, temp_page + space, l);
	index->dpg_offset = space;
	}

MOVE_FASTER (temp_page + space, 
    (UCHAR*) page + space,
    dbb->dbb_page_size - space);

if (page->dpg_header.pag_type != pag_data)
    BUGCHECK (251); /* msg 251 damaged data page */

#if (defined PC_PLATFORM && !defined NETWARE_386)
if (temp_page)
   ALL_free (temp_page);
tdbb->tdbb_setjmp = (UCHAR*) old_env;
#endif

return space;
}

void DPM_create_relation (
    TDBB	tdbb,
    REL	relation)
{
/**************************************
 *
 *	D P M _ c r e a t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Create a new relation.
 *
 **************************************/
DBB	dbb;
WIN	window, root_window;
PPG	page;
VCL	vector;
IRT	root;
HDR	header;
JRNDA	record;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_create_relation (relation %d)\n", relation->rel_id);
#endif

/* Allocate first pointer page */

page = (PPG) DPM_allocate (tdbb, &window);
page->ppg_header.pag_type = pag_pointer;
page->ppg_relation = relation->rel_id;
page->ppg_header.pag_flags = ppg_eof;
CCH_RELEASE (tdbb, &window);

/* If this is relation 0 (RDB$PAGES), update the header */

if (relation->rel_id == 0)
    {
    root_window.win_page = HEADER_PAGE;
    root_window.win_flags = 0;
    header = (HDR) CCH_FETCH (tdbb, &root_window, LCK_write, pag_header);
    CCH_MARK (tdbb, &root_window);
    header->hdr_PAGES = window.win_page;

    if (dbb->dbb_wal)
	{
	record.jrnda_type   = JRNP_DB_HDR_PAGES;
	record.jrnda_data   = window.win_page;
	CCH_journal_record (tdbb, &root_window, &record, JRNDA_SIZE, NULL_PTR, 0);
	}
    CCH_RELEASE (tdbb, &root_window);
    }

/* Keep track in memory of the first pointer page */

relation->rel_pages = vector = (VCL) ALLOCPV (type_vcl, 1);
vector->vcl_count = 1;
vector->vcl_long[0] = window.win_page;

/* Create an index root page */

root = (IRT) DPM_allocate (tdbb, &root_window);
root->irt_header.pag_type = pag_root;
root->irt_relation = relation->rel_id;
/*root->irt_count = 0;*/
CCH_RELEASE (tdbb, &root_window);
relation->rel_index_root = root_window.win_page;

/* Store page numbers in RDB$PAGES */

DPM_pages (tdbb, relation->rel_id, pag_pointer, (ULONG) 0, window.win_page);
DPM_pages (tdbb, relation->rel_id, pag_root, (ULONG) 0, root_window.win_page);
}

SLONG DPM_data_pages (
    TDBB	tdbb,
    REL		relation)
{
/**************************************
 *
 *	D P M _ d a t a _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Compute and return the number of data pages in a relation.
 *	Some poor sucker is going to use this information to make
 *	an educated guess at the number of records in the relation.
 *
 **************************************/
USHORT	sequence;
WIN	window;
PPG	ppage;
SLONG	*page, *end_page, pages;

SET_TDBB (tdbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_data_pages (relation %d)\n", relation->rel_id);
#endif

if (!(pages = relation->rel_data_pages))
    {
    window.win_flags = 0;
    for (sequence = 0;; sequence++)
    	{
    	if (!(ppage = get_pointer_page (tdbb, relation, &window, sequence, LCK_read)))
	    BUGCHECK (243); /* msg 243 missing pointer page in DPM_data_pages */
    	page = ppage->ppg_page;
    	end_page = page + ppage->ppg_count;
    	while (page < end_page)
	    if (*page++)
	    	pages++;
    	if (ppage->ppg_header.pag_flags & ppg_eof)
	    break;
    	CCH_RELEASE (tdbb, &window);
    	}

    CCH_RELEASE (tdbb, &window);
#ifdef SUPERSERVER
    relation->rel_data_pages = pages;
#endif
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("    returned pages: %d\n", pages);
#endif                                       

return pages;
}

void DPM_delete (
    TDBB	tdbb,
    RPB		*rpb,
    SLONG	prior_page)
{
/**************************************
 *
 *	D P M _ d e l e t e
 *
 **************************************
 *
 * Functional description
 *	Delete a fragment from data page.  Assume the page has
 *	already been fetched (but not marked) for write.  Copy the
 *	record header into the record parameter block before deleting
 *	it.  If the record goes empty, release the page.  Release the
 *	page when we're done.
 *
 **************************************/
DBB		dbb;
WIN		*window, pwindow;
DPG		page;
PPG		ppage;
UCHAR		flags;
SSHORT		slot, count;
USHORT		pp_sequence;
SLONG		*ptr, number, sequence;
JRNP		journal;
REL		relation;
struct dpg_repeat	*index;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_delete (rpb %d, prior_page %d)\n", rpb->rpb_number, prior_page);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif

window = &rpb->rpb_window;
page = (DPG) window->win_buffer;
sequence = page->dpg_sequence;
index = &page->dpg_rpt [ rpb->rpb_line ];
number = rpb->rpb_number;

if (!get_header (window, rpb->rpb_line, rpb))
    {
    CCH_RELEASE (tdbb, window);
    BUGCHECK (244); /* msg 244 Fragment does not exist */
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record length as stored in record header: %d\n", rpb->rpb_length);
#endif

rpb->rpb_number = number;

CCH_precedence (tdbb, window, prior_page);
CCH_MARK (tdbb, window);
index->dpg_offset = 0;
index->dpg_length = 0;

if (dbb->dbb_wal)
    journal_segment (tdbb, window, rpb->rpb_line);

/* Compute the highest line number level on page */

for (index = &page->dpg_rpt [page->dpg_count];
     index > page->dpg_rpt; --index)
    if (index [-1].dpg_offset)
	break;

page->dpg_count = count = index - page->dpg_rpt;

/* If the page is not empty and used to be marked as full, change the
   state of both the page and the appropriate pointer page. */

if (count && (page->dpg_header.pag_flags & dpg_full))
    {
    DEBUG
    page->dpg_header.pag_flags &= ~dpg_full;
    mark_full (tdbb, rpb);

#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	ib_printf ("    page is no longer full\n");
#endif
    return;
    }

flags = page->dpg_header.pag_flags;
CCH_RELEASE (tdbb, window);

/* If the page is non-empty, we're done. */

if (count)
    return;

if (flags & dpg_orphan)
    {
    /* The page inventory page will be written after the page being
       released, which will be written after the pages from which earlier
       fragments were deleted, which will be written after the page from
       which the first fragment is deleted.
       The resulting 'must-be-written-after' graph is:
	  pip --> deallocated page --> prior_page */
    PAG_release_page (window->win_page, window->win_page);
    return;
    }

/* Data page has gone empty.  Refetch page thru pointer page.  If it's
   still empty, remove page from relation. */

DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);
retry_after_latch_timeout:
pwindow.win_flags = 0;
if (!(ppage = get_pointer_page (tdbb, rpb->rpb_relation, &pwindow, pp_sequence, LCK_write)))
    BUGCHECK (245); /* msg 245 pointer page disappeared in DPM_delete */


if (slot >= ppage->ppg_count ||
    !(window->win_page = ppage->ppg_page [slot]))
    {
    CCH_RELEASE (tdbb, &pwindow);
    return;
    }

/* Since this fetch for exclusive access follows a (pointer page) fetch for
   exclusive access, put a timeout on this fetch to be able to recover from
   possible deadlocks. */
page = (DPG) CCH_FETCH_TIMEOUT (tdbb, window, LCK_write, pag_data, -1);
if (!page)
    {
    CCH_RELEASE (tdbb, &pwindow);
    goto retry_after_latch_timeout;
    }
if (page->dpg_count)
    {
    CCH_RELEASE (tdbb, &pwindow);
    CCH_RELEASE (tdbb, window);
    return;
    }

/* Data page is still empty and still in the relation.  Eliminate the
   pointer to the data page then release the page. */

#ifdef VIO_DEBUG 
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("\tDPM_delete:  page %d is empty and about to be released from relation %d\n", 
	window->win_page, rpb->rpb_relation->rel_id);
#endif

/* Make sure that the pointer page is written after the data page.
   The resulting 'must-be-written-after' graph is:
      pip --> pp --> deallocated page --> prior_page */
CCH_precedence (tdbb, &pwindow, window->win_page);

CCH_MARK (tdbb, &pwindow);
ppage->ppg_page [slot] = 0;

for (ptr = &ppage->ppg_page [ppage->ppg_count]; ptr > ppage->ppg_page; --ptr)
    if (ptr [-1])
	break;

ppage->ppg_count = count = ptr - ppage->ppg_page;
if (count)
    count--;
ppage->ppg_min_space = MIN (ppage->ppg_min_space, count);
relation = rpb->rpb_relation;
relation->rel_slot_space = MIN (relation->rel_slot_space, pp_sequence);
if (relation->rel_data_pages)
    --relation->rel_data_pages;

/* If journalling is enabled, make of note */

if (dbb->dbb_wal)
    {
    journal.jrnp_type = JRNP_POINTER_SLOT;
    journal.jrnp_index = slot;
    journal.jrnp_length = 0;
    CCH_journal_record (tdbb, &pwindow, &journal, JRNP_SIZE, NULL_PTR, 0);
    }

CCH_RELEASE (tdbb, &pwindow);
CCH_RELEASE (tdbb, window);

/* Make sure that the page inventory page is written after the pointer page.
   Earlier, we make sure that the pointer page is written after the data
   page being released. */ 
PAG_release_page (window->win_page, pwindow.win_page);
}

void DPM_delete_relation (
    TDBB	tdbb,
    REL		relation)
{
/**************************************
 *
 *	D P M _ d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Get rid of an unloved, unwanted relation.
 *
 **************************************/
DBB		dbb;
USHORT		sequence, i;
WIN		window, data_window;
DPG		dpage;
PPG		ppage;
BLK		handle;
UCHAR		*flags, pag_flags;
SLONG		*page;
RHD		header;
struct dpg_repeat	*line, *end_line;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;
window.win_flags = data_window.win_flags = WIN_large_scan;
window.win_scans = data_window.win_scans = 1;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_delete_relation (relation %d)\n", relation->rel_id);
#endif

/* Delete all data and pointer pages */

for (sequence = 0; TRUE; sequence++)
    {
    if (!(ppage = get_pointer_page (tdbb, relation, &window, sequence, LCK_read)))
	BUGCHECK (246); /* msg 246 pointer page lost from DPM_delete_relation */
    page = ppage->ppg_page;
    flags = (UCHAR*) (ppage->ppg_page + dbb->dbb_dp_per_pp);
    for (i = 0; i < ppage->ppg_count; i++, page++)
	{
	if (!*page)
	    continue;
	if (flags [i >> 2] & (2 << ((i & 3) << 1)))
	    {
	    data_window.win_page = *page;
	    dpage = (DPG) CCH_FETCH (tdbb, &data_window, LCK_write, pag_data);
	    line = dpage->dpg_rpt;
	    end_line = line + dpage->dpg_count;
	    for (; line < end_line; line++)
		if (line->dpg_length)
		    {
		    header = (RHD) ((UCHAR*) dpage + line->dpg_offset);
		    if (header->rhd_flags & rhd_large)
			delete_tail (tdbb, header, line->dpg_length);
		    }
	    CCH_RELEASE_TAIL (tdbb, &data_window);
	    }
	PAG_release_page (*page, 0);
	}
    pag_flags = ppage->ppg_header.pag_flags;
    CCH_RELEASE_TAIL (tdbb, &window);
    PAG_release_page (window.win_page, 0);
    if (pag_flags & ppg_eof)
	break;
    }

ALL_RELEASE (relation->rel_pages);
relation->rel_pages = NULL;
relation->rel_data_pages = 0;

/* Now get rid of the index root page */

PAG_release_page (relation->rel_index_root, 0);
relation->rel_index_root = 0;

/* Next, cancel out stuff from RDB$PAGES */

handle = NULL;

FOR (REQUEST_HANDLE handle) X IN RDB$PAGES WITH 
	X.RDB$RELATION_ID EQ relation->rel_id
    ERASE X;
END_FOR;

CMP_release (tdbb, handle);
CCH_flush (tdbb, (USHORT) FLUSH_ALL, 0);
}

BOOLEAN DPM_fetch (
    TDBB	tdbb,
    register RPB	*rpb,
    USHORT		lock)
{
/**************************************
 *
 *	D P M _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Fetch a particular record fragment from page and line numbers.
 *	Get various header stuff, but don't change the record number.
 *
 *	return: TRUE if the fragment is returned.
 *		FALSE if the fragment is not found.
 *
 **************************************/
SLONG	number;

SET_TDBB (tdbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_fetch (rpb %d, lock %d)\n", rpb->rpb_number, lock);
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record %d:%d\n", rpb->rpb_page, rpb->rpb_line);
#endif

number = rpb->rpb_number;
rpb->rpb_window.win_page = rpb->rpb_page;
CCH_FETCH (tdbb, &rpb->rpb_window, lock, pag_data);

if (!get_header (&rpb->rpb_window, rpb->rpb_line, rpb))
    {
    CCH_RELEASE (tdbb, &rpb->rpb_window);
    return FALSE;
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif
rpb->rpb_number = number;

return TRUE;
}

SSHORT DPM_fetch_back (
    TDBB	tdbb,
    register RPB	*rpb,
    USHORT		lock,
    SSHORT		latch_wait)
{
/**************************************
 *
 *	D P M _ f e t c h _ b a c k
 *
 **************************************
 *
 * Functional description
 *	Chase a backpointer with a handoff.
 *
 * input:
 *      latch_wait:     1 => Wait as long as necessary to get the latch.
 *                              This can cause deadlocks of course.
 *                      0 => If the latch can't be acquired immediately,
 *                              give up and return 0.
 *                      <negative number> => Latch timeout interval in seconds.
 *
 * return:
 *	1:	fetch back was successful.
 *	0:	unsuccessful (only possible is latch_wait <> 1),
 *			The latch timed out.
 *			The latch on the fetched page is downgraded to shared.
 *			The fetched page is unmarked.
 *
 **************************************/
SLONG	number;

SET_TDBB (tdbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_fetch_back (rpb %d, lock %d)\n", rpb->rpb_number, lock);
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record  %d:%d transaction %d back %d:%d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line);
#endif

/* Possibly allow a latch timeout to occur.  Return error if that is the case. */

if (!(CCH_HANDOFF_TIMEOUT (tdbb, &rpb->rpb_window, rpb->rpb_b_page, lock, pag_data, latch_wait)))
    return 0;

number = rpb->rpb_number;
rpb->rpb_page = rpb->rpb_b_page;
rpb->rpb_line = rpb->rpb_b_line;
if (!get_header (&rpb->rpb_window, rpb->rpb_line, rpb))
    {
    CCH_RELEASE (tdbb, &rpb->rpb_window);
    BUGCHECK (291); /* msg 291 cannot find record back version */
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record fetched  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif


rpb->rpb_number = number;

return 1;
}

void DPM_fetch_fragment (
    TDBB	tdbb,
    register RPB	*rpb,
    USHORT		lock)
{
/**************************************
 *
 *	D P M _ f e t c h _f r a g m e n t
 *
 **************************************
 *
 * Functional description
 *	Chase a fragment pointer with a handoff.
 *
 **************************************/
SLONG	number;

SET_TDBB (tdbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_fetch_fragment (rpb %d, lock %d)\n", rpb->rpb_number, lock);
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record  %d:%d transaction %d back %d:%d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line);
#endif

number = rpb->rpb_number;
rpb->rpb_page = rpb->rpb_f_page;
rpb->rpb_line = rpb->rpb_f_line;
CCH_HANDOFF (tdbb, &rpb->rpb_window, rpb->rpb_page, lock, pag_data);

if (!get_header (&rpb->rpb_window, rpb->rpb_line, rpb))
    {
    CCH_RELEASE (tdbb, &rpb->rpb_window);
    BUGCHECK (248); /* msg 248 cannot find record fragment */
    }

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record fetched  %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif
rpb->rpb_number = number;
}

SINT64 DPM_gen_id (
    TDBB	tdbb,
    SLONG	generator,
    USHORT	initialize,
    SINT64      val)
{
/**************************************
 *
 *	D P M _ g e n _ i d
 *
 **************************************
 *
 * Functional description
 *	Generate relation specific value.
 *      If initialize is set then value of generator is made 
 *      equal to val else generator is incremented by val. 
 *      The resulting value is the result of the function.
 **************************************/
DBB	dbb;
PPG	page;
WIN	window;
VCL	vector;
USHORT	sequence, offset;
SINT64	value, *ptr;
SLONG   *lptr;
JRNG	journal;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_gen_id (generator %d, val %" QUADFORMAT "d)\n",
	    generator, val);
#endif

sequence = generator / dbb->dbb_pcontrol->pgc_ppp;
offset = generator % dbb->dbb_pcontrol->pgc_ppp;

if (!(vector = dbb->dbb_gen_id_pages) ||
    sequence >= vector->vcl_count)
    {
    DPM_scan_pages (tdbb);
    if (!(vector = dbb->dbb_gen_id_pages) ||
	sequence >= vector->vcl_count)
	{
	page = (PPG) DPM_allocate (tdbb, &window);
	page->ppg_header.pag_type = pag_ids;
	page->ppg_sequence = sequence;
	CCH_must_write (&window);
	CCH_RELEASE (tdbb, &window);
	DPM_pages (tdbb, 0, pag_ids, (ULONG) sequence, window.win_page);
	vector = (VCL) ALL_vector (dbb->dbb_permanent, &dbb->dbb_gen_id_pages, sequence + 1);
	vector->vcl_long [sequence] = window.win_page;
	}
    }

window.win_page = vector->vcl_long [sequence];
window.win_flags = 0;
#ifdef READONLY_DATABASE
if (dbb->dbb_flags & DBB_read_only)
    page = (PPG) CCH_FETCH (tdbb, &window, LCK_read, pag_ids);
else
    page = (PPG) CCH_FETCH (tdbb, &window, LCK_write, pag_ids);
#else
page = (PPG) CCH_FETCH (tdbb, &window, LCK_write, pag_ids);
#endif  /* READONLY_DATABASE */

/*  If we are in ODS >= 10, then we have a pointer to an int64 value in the
 *  generator page: if earlier than 10, it's a pointer to a long value.
 *  Pick up the right kind of pointer, based on the ODS version.
 *  The conditions were commented out 1999-05-14 by ChrisJ, because we
 *  decided that the V6 engine would only access an ODS-10 database.
 *  (and uncommented 2000-05-05, also by ChrisJ, when minds changed.)
 */
if ( dbb->dbb_ods_version >= ODS_VERSION10)
    ptr = ((SINT64 *)(page->ppg_page)) + offset;
else
    lptr = ((SLONG *)(page->ppg_page)) + offset;

if (val || initialize)
    {
#ifdef READONLY_DATABASE
    if (dbb->dbb_flags & DBB_read_only)
	ERR_post (isc_read_only_database, 0);
#endif  /* READONLY_DATABASE */
    CCH_MARK (tdbb, &window);

    /* Initialize or increment the quad value in an ODS 10 or later
     * generator page, or the long value in ODS <= 9.
     * The conditions were commented out 1999-05-14 by ChrisJ, because we
     * decided that the V6 engine would only access an ODS-10 database.
     * (and uncommented 2000-05-05, also by ChrisJ, when minds changed.)
     */
   if (dbb->dbb_ods_version >= ODS_VERSION10)
        if (initialize)
	    *ptr = val;
        else
            *ptr += val;
   else
        if (initialize)
	    *lptr = (SLONG)val;
        else
            *lptr += (SLONG)val;

    if (dbb->dbb_wal)
	{
	/******* MAKE journalling logical  *******/
	journal.jrng_type 	= JRNP_GENERATOR;
	journal.jrng_offset 	= offset;
	journal.jrng_genval	= *ptr;
	CCH_journal_record (tdbb, &window, &journal, JRNG_SIZE, NULL_PTR, 0);
	}

    if (tdbb->tdbb_transaction)
	tdbb->tdbb_transaction->tra_flags |= TRA_write;
    }

/*  If we are in ODS >= 10, then we have a pointer to an int64 value in the
 *  generator page: if earlier than 10, it's a pointer to a long value.
 *  We always want to return an int64, so convert the long value from the
 *  old ODS into an int64.
 * The conditions were commented out 1999-05-14 by ChrisJ, because we
 * decided that the V6 engine would only access an ODS-10 database.
 * (and uncommented 2000-05-05, also by ChrisJ, when minds changed.)
 */
if (dbb->dbb_ods_version >= ODS_VERSION10)
    value = *ptr;
else
    value = (SINT64) *lptr;

CCH_RELEASE (tdbb, &window);

return value;
}

int DPM_get (
    TDBB	tdbb,
    register RPB	*rpb,
    SSHORT		lock_type)
{
/**************************************
 *
 *	D P M _ g e t
 *
 **************************************
 *
 * Functional description
 *	Get a specific record in a relation.  If it doesn't exit,
 *	just return false.
 *
 **************************************/
DBB		dbb;
SLONG		sequence, page_number;
USHORT		pp_sequence;
register WIN	*window;
register PPG	page;
SSHORT		slot, line;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_get (rpb %d, lock type %d)\n", rpb->rpb_number, lock_type);
#endif

window = &rpb->rpb_window;
rpb->rpb_prior = NULL;

/* Find starting point */

DECOMPOSE (rpb->rpb_number, dbb->dbb_max_records, sequence, line);
DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);

/* Check if the record number is OK */

if ((slot < 0) || (line < 0))
    return FALSE;
    
/* Find the next pointer page, data page, and record */

if (!(page = get_pointer_page (tdbb, rpb->rpb_relation, window, pp_sequence, LCK_read)))
    return FALSE;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    record  %d:%d\n", page->ppg_page[slot], line);
#endif

if (page_number = page->ppg_page [slot])
    {
    CCH_HANDOFF (tdbb, window, page_number, lock_type, pag_data);
    if (get_header (window, line, rpb) &&
	!(rpb->rpb_flags & (rpb_blob | rpb_chained | rpb_fragment)))
	return TRUE;
    }
	    
CCH_RELEASE (tdbb, window);

return FALSE;
}

ULONG DPM_get_blob (
    TDBB	tdbb,
    BLB		blob,
    ULONG	record_number,
    USHORT	delete_flag,
    SLONG	prior_page)
{
/**************************************
 *
 *	D P M _ g e t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Given a blob block, find the associated blob.  If blob is level 0,
 *	get the data clump, otherwise get the vector of pointers.  
 *
 *	If the delete flag is set, delete the blob header after access
 *	and return the page number.  This is a kludge, but less code
 *	a completely separate delete blob call.
 *
 **************************************/
DBB		dbb;
ATT		attachment;
SLONG		sequence, page_number;
USHORT		pp_sequence;
register DPG	page;
PPG		ppage;
BLH		header;
USHORT		length, slot, line;
VCL		vector;
UCHAR		*p, *q;
RPB		rpb;
struct dpg_repeat	*index;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);
rpb.rpb_window.win_flags = WIN_secondary;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_get_blob (blob, record_number %d, delete_flag %d, prior_page %d)\n", 
	    record_number, delete_flag, prior_page);
#endif

/* Find starting point */

DECOMPOSE (record_number, dbb->dbb_max_records, sequence, line);
DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);

/* Find the next pointer page, data page, and record.  If the page or
   record doesn't exist, or the record isn't a blob, give up and
   let somebody else complain.  */

if (!(ppage = get_pointer_page (tdbb, blob->blb_relation, &rpb.rpb_window, 
	pp_sequence, LCK_read)))
    {
    blob->blb_flags |= BLB_damaged;
    return NULL;
    }

if (!(page_number = ppage->ppg_page [slot]))
    goto punt;

page = (DPG) CCH_HANDOFF (tdbb, &rpb.rpb_window, page_number, 
	(delete_flag) ? LCK_write : LCK_read, pag_data);
if (line >= page->dpg_count)
    goto punt;

index = &page->dpg_rpt [line];
if (index->dpg_offset == 0)
    goto punt;

header = (BLH) ((SCHAR*) page + index->dpg_offset);
if (!(header->blh_flags & rhd_blob))
    goto punt;

/* We've got the blob header and everything looks ducky.  Get the header
   fields. */

blob->blb_lead_page = header->blh_lead_page;
blob->blb_max_sequence = header->blh_max_sequence;
blob->blb_count = header->blh_count;
blob->blb_length = header->blh_length;
blob->blb_max_segment = header->blh_max_segment;
blob->blb_level = header->blh_level;
blob->blb_sub_type = header->blh_sub_type;

/* Unless this is the only attachment, don't allow the sequential scan
   of very large blobs to flush pages used by other attachments. */

if ((attachment = tdbb->tdbb_attachment) &&
    (attachment != dbb->dbb_attachments || attachment->att_next))
    {
    /* If the blob has more pages than the page buffer cache then mark
       it as large. If this is a database backup then mark any blob as
       large as the cumulative effect of scanning many small blobs is
       equivalent to scanning single large blobs. */

    if (blob->blb_max_sequence > dbb->dbb_bcb->bcb_count ||
	attachment->att_flags & ATT_gbak_attachment)
	blob->blb_flags |= BLB_large_scan;
    }

if (header->blh_flags & rhd_stream_blob)
    blob->blb_flags |= BLB_stream;

if (header->blh_flags & rhd_damaged)
    {
    blob->blb_flags |= BLB_damaged;
    CCH_RELEASE (tdbb, &rpb.rpb_window);
    return NULL;
    }

/* Retrieve the data either into page clump (level 0) or page vector (levels
   1 and 2). */

length = index->dpg_length - BLH_SIZE;
q = (UCHAR*) header->blh_data;

if (blob->blb_level == 0)
    {
    blob->blb_space_remaining = length;
    p = blob->blb_data;
    if (length)
	MOVE_FASTER (q, p, length);
    }
else
    {
    if (!(vector = blob->blb_pages))
	{
	TRA	transaction;

	transaction = blob->blb_transaction;
	vector = blob->blb_pages = (VCL) ALLOCTV (type_vcl, blob->blb_max_pages);
	}
    vector->vcl_count = length / sizeof (SLONG);
    MOVE_FASTER (q, vector->vcl_long, length);
    }

if (!delete_flag)
    {
    CCH_RELEASE (tdbb, &rpb.rpb_window);
    return NULL;
    }

/* We've been asked (nicely) to delete the blob.  So do so. */

rpb.rpb_relation = blob->blb_relation;
rpb.rpb_page = rpb.rpb_window.win_page;
rpb.rpb_line = line;
DPM_delete (tdbb, &rpb, prior_page);
return rpb.rpb_page;

punt:

CCH_RELEASE (tdbb, &rpb.rpb_window);
blob->blb_flags |= BLB_damaged;
return NULL;
}

BOOLEAN DPM_next (
    TDBB		tdbb,
    register RPB	*rpb,
    USHORT		lock_type,
    BOOLEAN		backwards,
    BOOLEAN		onepage)
{
/**************************************
 *
 *	D P M _ n e x t
 *
 **************************************
 *
 * Functional description
 *	Get the next record in a stream.
 *
 **************************************/
DBB		dbb;
SLONG		sequence, page_number;
USHORT		pp_sequence;
register WIN	*window;
register PPG	ppage;
register DPG	dpage;
SSHORT		slot, line, flags;
VCL		vector;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS)
    ib_printf ("DPM_next (rpb %d)\n", rpb->rpb_number);
#endif

window = &rpb->rpb_window;
if (window->win_flags & WIN_large_scan)
    {
    /* Try to account for staggered execution of large sequential scans. */

    window->win_scans = rpb->rpb_relation->rel_scan_count - rpb->rpb_org_scans;
    if (window->win_scans < 1)
	window->win_scans = rpb->rpb_relation->rel_scan_count;
    }
rpb->rpb_prior = NULL;

/* Find starting point */

if (backwards)
    {   
    if (rpb->rpb_number > 0)
	rpb->rpb_number--;
    else if (rpb->rpb_number < 0)
	{
	/*  if the stream was just opened, assume we want to start
	    at the end of the stream, so compute the last theoretically 
	    possible rpb_number and go down from there */
	/** for now, we must force a scan to make sure that we get
	    the last pointer page: this should be changed to use
	    a coordination mechanism (probably using a shared lock)
	    to keep apprised of when a pointer page gets added **/

	DPM_scan_pages (tdbb);   
	if (!(vector = rpb->rpb_relation->rel_pages))
	    return FALSE;
	pp_sequence = vector->vcl_count;
        rpb->rpb_number = (((SLONG) (pp_sequence * dbb->dbb_dp_per_pp) * dbb->dbb_max_records) - 1);
        line = dbb->dbb_max_records - 1;
	}
    else 
	return FALSE;
    }
else
    rpb->rpb_number++;

DECOMPOSE (rpb->rpb_number, dbb->dbb_max_records, sequence, line);
DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_READS_INFO)
    ib_printf ("    pointer, slot, and line  %d:%d%d\n", pp_sequence, slot, line);
#endif

/* Find the next pointer page, data page, and record */

while (TRUE)
    {
    if (!(ppage = get_pointer_page (tdbb, rpb->rpb_relation, window, pp_sequence, LCK_read)))
	BUGCHECK (249); /* msg 249 pointer page vanished from DPM_next */
    if (backwards && slot >= ppage->ppg_count)
	slot = ppage->ppg_count - 1;
    for (; slot >= 0 && slot < ppage->ppg_count;)
	{
	if (page_number = ppage->ppg_page [slot])
	    {
#ifdef SUPERSERVER_V2
	    /* Perform sequential prefetch of relation's data pages.
	       This may need more work for scrollable cursors. */
	       
	    if (!onepage && !line && !backwards)
		{
		USHORT	slot2, i;
		SLONG	pages [PREFETCH_MAX_PAGES + 1];

		if (!(slot % dbb->dbb_prefetch_sequence))
		    {
		    slot2 = slot;
		    for (i = 0; i < dbb->dbb_prefetch_pages && slot2 < ppage->ppg_count; )
			pages [i++] = ppage->ppg_page [slot2++];

		    /* If no more data pages, piggyback next pointer page. */
		    
		    if (slot2 >= ppage->ppg_count)
			pages [i++] = ppage->ppg_next;

		    CCH_PREFETCH (tdbb, pages, i);
		    }
		}
#endif
	    dpage = (DPG) CCH_HANDOFF (tdbb, window, page_number, lock_type, pag_data);
	    if (backwards && line >= dpage->dpg_count)
	        line = dpage->dpg_count - 1;

	    for (; line >= 0 && line < dpage->dpg_count; backwards ? line-- : line++)
		if (get_header (window, line, rpb) &&
		    !(rpb->rpb_flags & (rpb_blob | rpb_chained | rpb_fragment)))
		    {
		    rpb->rpb_number = (((SLONG) pp_sequence * dbb->dbb_dp_per_pp) + slot) *
				       dbb->dbb_max_records + line;
		    return TRUE;
		    }

	    /* Prevent large relations from emptying cache. When scrollable
	       cursors are surfaced, this logic may need to be revisited. */

	    if (window->win_flags & WIN_large_scan)
	    	CCH_RELEASE_TAIL (tdbb, window);
	    else if (window->win_flags & WIN_garbage_collector &&
		     window->win_flags & WIN_garbage_collect)
		{    
		CCH_RELEASE_TAIL (tdbb, window);
		window->win_flags &= ~WIN_garbage_collect;
		}
	    else
	    	CCH_RELEASE (tdbb, window);

	    if (onepage)
		return FALSE;

	    if (!(ppage = get_pointer_page (tdbb, rpb->rpb_relation, window, pp_sequence, LCK_read)))
		BUGCHECK (249); /* msg 249 pointer page vanished from DPM_next */
	    }

	if (onepage)
	    {
	    CCH_RELEASE (tdbb, window);
	    return FALSE;
	    }

        if (backwards)
	    {
	    slot--;
	    line = dbb->dbb_max_records - 1;
	    }
	else
	    {
	    slot++;
	    line = 0;
	    }
    	}

    flags = ppage->ppg_header.pag_flags;
    if (backwards)
	{
	pp_sequence--;
	slot = ppage->ppg_count - 1;
        line = dbb->dbb_max_records - 1;
	}
    else 
	{
	pp_sequence++;
	slot = 0;
	line = 0;
	}

    if (window->win_flags & WIN_large_scan)
	CCH_RELEASE_TAIL (tdbb, window);
    else
        CCH_RELEASE (tdbb, window);
    if (flags & ppg_eof || onepage)
	return FALSE;
    }
}

void DPM_pages (
    TDBB	tdbb,
    SSHORT	rel_id,
    int		type,
    ULONG	sequence,
    SLONG	page)
{
/**************************************
 *
 *	D P M _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Write a record in RDB$PAGES.
 *
 **************************************/
DBB	dbb;
BLK	request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_pages (rel_id %d, type %d, sequence %d, page %d)\n", rel_id, type, sequence, page);
#endif

request = (BLK) CMP_find_request (tdbb, irq_s_pages, IRQ_REQUESTS);

STORE (REQUEST_HANDLE request)
      X IN RDB$PAGES
    X.RDB$RELATION_ID = rel_id;
    X.RDB$PAGE_TYPE = type;
    X.RDB$PAGE_SEQUENCE = sequence;
    X.RDB$PAGE_NUMBER = page;
END_STORE

if (!REQUEST (irq_s_pages))
    REQUEST (irq_s_pages) = request;
}

#ifdef SUPERSERVER_V2
SLONG DPM_prefetch_bitmap (
    TDBB	tdbb,
    REL		relation,
    SBM		bitmap,
    SLONG	number)
{
/**************************************
 *
 *	D P M _ p r e f e t c h _ b i t m a p
 *
 **************************************
 *
 * Functional description
 *	Generate a vector of corresponding data page
 *	numbers from a bitmap of relation record numbers.
 *	Return the bitmap record number of where we left
 *	off.
 *
 **************************************/
DBB	dbb;
SLONG	dp_sequence, prefetch_number;
USHORT	pp_sequence, i;
SSHORT	line, slot;
WIN	window;
PPG	ppage;
SLONG	pages [PREFETCH_MAX_PAGES];

SET_TDBB (tdbb);

/* Empty and singular bitmaps aren't worth prefetch effort. */

if (!bitmap || bitmap->sbm_state != SBM_PLURAL)
    return number;

dbb = tdbb->tdbb_database;
window.win_flags = 0;

for (i = 0; i < dbb->dbb_prefetch_pages; )
    {
    DECOMPOSE (number, dbb->dbb_max_records, dp_sequence, line);
    DECOMPOSE (dp_sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);

    if (!(ppage = get_pointer_page (tdbb, relation, &window, pp_sequence, LCK_read)))
        BUGCHECK (249); /* msg 249 pointer page vanished from DPM_prefetch_bitmap */
    pages [i] = (slot >= 0 && slot < ppage->ppg_count) ? ppage->ppg_page [slot] : 0;
    CCH_RELEASE (tdbb, &window);
	
    if (i++ < dbb->dbb_prefetch_sequence)
	prefetch_number = number;
    number = ((dp_sequence + 1) * dbb->dbb_max_records) - 1;
    if (!SBM_next (bitmap, &number, RSE_get_forward))
    	break;
    }

CCH_PREFETCH (tdbb, pages, i);
return prefetch_number;
}
#endif

void DPM_scan_pages (
    TDBB	tdbb)
{
/**************************************
 *
 *	D P M _ s c a n _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Scan RDB$PAGES.
 *
 **************************************/
DBB	dbb;
BLK	request;
REL	relation;
VCL	*address, vector;
WIN	window;
PPG	ppage;
int	n, sequence;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("DPM_scan_pages ()\n");
#endif

/* Special case update of RDB$PAGES pointer page vector to avoid
   infinite recursion from this internal request when RDB$PAGES
   has been extended with another pointer page. */
   
relation = MET_relation (tdbb, 0);
address = &relation->rel_pages;
vector = *address;
sequence = vector->vcl_count - 1;
window.win_page = vector->vcl_long [sequence];
window.win_flags = 0;
ppage = (PPG) CCH_FETCH (tdbb, &window, LCK_read, pag_pointer);

while (ppage->ppg_next)
    {
    ++sequence;
    vector = (VCL) ALL_extend (address, sequence + 1);
    vector->vcl_long [sequence] = ppage->ppg_next;
    ppage = (PPG) CCH_HANDOFF (tdbb, &window, ppage->ppg_next, LCK_read, pag_pointer);
    }

CCH_RELEASE (tdbb, &window);

request = (BLK) CMP_find_request (tdbb, irq_r_pages, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) X IN RDB$PAGES

    if (!REQUEST (irq_r_pages))
	REQUEST (irq_r_pages) = request;
    relation = MET_relation (tdbb, X.RDB$RELATION_ID);
    sequence = X.RDB$PAGE_SEQUENCE;
    switch (X.RDB$PAGE_TYPE)
	{
	case pag_root:
	    relation->rel_index_root = X.RDB$PAGE_NUMBER;
	    continue;

	case pag_pointer:
	    address = &relation->rel_pages;
	    break;

	case pag_transactions:
	    address = &dbb->dbb_t_pages;
	    break;

	case pag_ids:
	    address = &dbb->dbb_gen_id_pages;
	    break;

	default:
	    CORRUPT (257); /* msg 257 bad record in RDB$PAGES */
	}
    n = sequence + 1;
    if (!(vector = *address))
	{
	vector = *address = (VCL) ALLOCPV (type_vcl, n);
	vector->vcl_count = n;
	}
    else 
	if (sequence >= vector->vcl_count)
	    vector = (VCL) ALL_extend (address, n);
    vector->vcl_long [sequence] = X.RDB$PAGE_NUMBER;
END_FOR;

if (!REQUEST (irq_r_pages))
    REQUEST (irq_r_pages) = request;
}

void DPM_store (
    TDBB	tdbb,
    register RPB	*rpb,
    LLS			*stack,
    USHORT		type)
{
/**************************************
 *
 *	D P M _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Store a new record in a relation.  If we can put it on a
 *	specific page, so much the better.
 *
 **************************************/
DBB		dbb;
register RHD	header;
struct dcc	dcc;
UCHAR		*p;
SLONG		fill, length;  /* Accomodate max record size i.e. 64K */
USHORT    	size;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_store (rpb %d, stack, type %d)\n", rpb->rpb_number, type);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record to store %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif

size = SQZ_length (tdbb, rpb->rpb_address, (int) rpb->rpb_length, &dcc);

/* If the record isn't going to fit on a page, even if fragmented,
   handle it a little differently. */

if (size > dbb->dbb_page_size - (sizeof (struct dpg) + RHD_SIZE))
    {
    store_big_record (tdbb, rpb, stack, &dcc, size);
    return;
    }

fill = (RHDF_SIZE - RHD_SIZE) - size;
if (fill < 0)
    fill = 0;

length = RHD_SIZE + size + fill;
header = locate_space (tdbb, rpb, length, stack, NULL_PTR, type);
 
header->rhd_flags = rpb->rpb_flags;
header->rhd_transaction = rpb->rpb_transaction;
header->rhd_format = rpb->rpb_format_number;
header->rhd_b_page = rpb->rpb_b_page;
header->rhd_b_line = rpb->rpb_b_line;

SQZ_fast (&dcc, rpb->rpb_address, header->rhd_data);
release_dcc (dcc.dcc_next);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record %d:%d, length %d, rpb_flags %d, f_page %d:%d, b_page %d:%d\n", 
		rpb->rpb_page, rpb->rpb_line, length, rpb->rpb_flags, 
		rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_b_page, rpb->rpb_b_line);
#endif

if (fill)
    {
    p = header->rhd_data + size;
    do *p++ = 0; while (--fill);
    }

if (dbb->dbb_wal)
    journal_segment (tdbb, &rpb->rpb_window, rpb->rpb_line);

CCH_RELEASE (tdbb, &rpb->rpb_window);
}

SLONG DPM_store_blob (
    TDBB	tdbb,
    BLB		blob,
    REC		record)
{
/**************************************
 *
 *	D P M _ s t o r e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Store a blob on a data page.  Not so hard, all in all.
 *
 **************************************/
DBB	dbb;
RPB	rpb;
BLH	header;
USHORT	length;
SLONG	*ptr, *end;
UCHAR	*p, *q;
VCL	vector;
DPG	page;
LLS	stack;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);
rpb.rpb_window.win_flags = 0;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_store_blob (blob, record)\n");
#endif

/* Figure out length of blob on page.  Remember that blob can either
   be a clump of data or a vector of page pointers. */

if (blob->blb_level == 0)
    {
    length = blob->blb_clump_size - blob->blb_space_remaining;
    q = (UCHAR*) ((BLP) blob->blb_data)->blp_data;
    }
else
    {
    vector = blob->blb_pages;
    length = vector->vcl_count * sizeof (SLONG);
    q = (UCHAR*) vector->vcl_long;
    }

/* Figure out precedence pages, if any */

stack = NULL;

if (blob->blb_level > 0)
    for (ptr = vector->vcl_long, end = ptr + vector->vcl_count; ptr < end;)
	LLS_PUSH ((BLK) *ptr++, &stack);

/* Locate space to store blob */

rpb.rpb_relation = blob->blb_relation;
header = (BLH) locate_space (tdbb, &rpb, BLH_SIZE + length, &stack, record, DPM_other);
header->blh_flags = rhd_blob;

if (blob->blb_flags & BLB_stream)
    header->blh_flags |= rhd_stream_blob;

if (blob->blb_level)
    header->blh_flags |= rhd_large;

header->blh_lead_page = blob->blb_lead_page;
header->blh_max_sequence = blob->blb_max_sequence;
header->blh_count = blob->blb_count;
header->blh_max_segment = blob->blb_max_segment;
header->blh_length = blob->blb_length;
header->blh_level = blob->blb_level;
header->blh_sub_type = blob->blb_sub_type;
p = (UCHAR*) header->blh_data;

if (length)
    MOVE_FASTER (q, p, length);

if (dbb->dbb_wal)
    journal_segment (tdbb, &rpb.rpb_window, rpb.rpb_line);

page = (DPG) rpb.rpb_window.win_buffer;
if (blob->blb_level && !(page->dpg_header.pag_flags & dpg_large))
    {
    page->dpg_header.pag_flags |= dpg_large;
    mark_full (tdbb, &rpb);
    }
else
    CCH_RELEASE (tdbb, &rpb.rpb_window);

return rpb.rpb_number;
}

void DPM_rewrite_header (
    TDBB	tdbb,
    RPB	*rpb)
{
/**************************************
 *
 *	D P M _ r e w r i t e _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Re-write benign fields in record header.  This is mostly used
 *	to purge out old versions.
 *
 **************************************/
DBB	dbb;
RHD	header;
WIN	*window;
DPG	page;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_rewrite_header (rpb %d)\n", rpb->rpb_number);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record  %d:%d\n", rpb->rpb_page, rpb->rpb_line);
#endif

window = &rpb->rpb_window;
page = (DPG) window->win_buffer;
header = (RHD) ((SCHAR*) page + page->dpg_rpt [ rpb->rpb_line ].dpg_offset);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    {
    ib_printf ("    old flags %d, old transaction %d, old format %d, old back record %d:%d\n",
	header->rhd_flags, header->rhd_transaction, header->rhd_format, header->rhd_b_page,
 	header->rhd_b_line);
    ib_printf ("    new flags %d, new transaction %d, new format %d, new back record %d:%d\n",
	rpb->rpb_flags, rpb->rpb_transaction, rpb->rpb_format_number, rpb->rpb_b_page,
	rpb->rpb_b_line); 
    }
#endif

header->rhd_flags = rpb->rpb_flags;
header->rhd_transaction = rpb->rpb_transaction;
header->rhd_format = rpb->rpb_format_number;
header->rhd_b_page = rpb->rpb_b_page;
header->rhd_b_line = rpb->rpb_b_line;
if (dbb->dbb_wal)
    journal_segment (tdbb, &rpb->rpb_window, rpb->rpb_line);
}

void DPM_update (
    TDBB	tdbb,
    register RPB	*rpb,
    LLS			*stack,
    TRA			transaction)
{
/**************************************
 *
 *	D P M _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Replace an existing record.
 *
 **************************************/
DBB		dbb;
RHD		header;
DPG		page;
struct dcc	dcc;
UCHAR		*p;
SLONG		length, fill;  /* Accomodate max record size i.e. 64K */
USHORT    	size;
SSHORT		top, offset, slot, available, space, old_length;
struct dpg_repeat *index, *end;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("DPM_update (rpb %d, stack, transaction %d)\n", rpb->rpb_number, transaction ? transaction->tra_number : 0);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif
                
/* Mark the page as modified, then figure out the compressed length of the
   replacement record. */

DEBUG
if (stack)
    while (*stack)
	CCH_precedence (tdbb, &rpb->rpb_window, (SLONG) LLS_POP (stack));

CCH_precedence (tdbb, &rpb->rpb_window, -rpb->rpb_transaction);
CCH_MARK (tdbb, &rpb->rpb_window);
page = (DPG) rpb->rpb_window.win_buffer;
size = SQZ_length (tdbb, rpb->rpb_address, (int) rpb->rpb_length, &dcc);

/* It is critical that the record be padded, if necessary, to the length of
   a fragmented record header.  Compute the amount of fill required.  */

fill = (RHDF_SIZE - RHD_SIZE) - size;
if (fill < 0)
    fill = 0;

length = ROUNDUP (RHD_SIZE + size + fill, ODS_ALIGNMENT);
slot = rpb->rpb_line;

/* Find space on page */

space = dbb->dbb_page_size;
top = HIGH_WATER (page->dpg_count);
available = dbb->dbb_page_size - top;
old_length = page->dpg_rpt[slot].dpg_length;
page->dpg_rpt[slot].dpg_length = 0;

for (index = page->dpg_rpt, end = index + page->dpg_count;
     index < end; index++)
    if (offset = index->dpg_offset)
	{
	available -= ROUNDUP (index->dpg_length, ODS_ALIGNMENT);
	space = MIN (space, offset);
	}

if (length > available)
    {
    fragment (tdbb, rpb, available, &dcc, old_length, transaction);
    return;
    }

space -= length;
if (space < top)
    space = DPM_compress (tdbb, page) - length;

page->dpg_rpt[slot].dpg_offset = space;
page->dpg_rpt[slot].dpg_length = RHD_SIZE + size + fill;

header = (RHD) ((SCHAR*) page + space);
header->rhd_flags = rpb->rpb_flags;
header->rhd_transaction = rpb->rpb_transaction;
header->rhd_format = rpb->rpb_format_number;
header->rhd_b_page = rpb->rpb_b_page;
header->rhd_b_line = rpb->rpb_b_line;

SQZ_fast (&dcc, rpb->rpb_address, header->rhd_data);
release_dcc (dcc.dcc_next);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record %d:%d, dpg_length %d, rpb_flags %d, rpb_f record %d:%d, rpb_b record %d:%d\n", 
	rpb->rpb_page, rpb->rpb_line,  page->dpg_rpt[slot].dpg_length,
	rpb->rpb_flags, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_b_page, rpb->rpb_b_line);
#endif

if (fill)
    {
    p = header->rhd_data + size;
    do *p++ = 0; while (--fill);
    }

if (dbb->dbb_wal)
    journal_segment (tdbb, &rpb->rpb_window, slot);

CCH_RELEASE (tdbb, &rpb->rpb_window);
}

static void delete_tail ( 
    TDBB	tdbb,
    RHDF	header,
    USHORT	length)
{
/**************************************
 *
 *	d e l e t e _ t a i l
 *
 **************************************
 *
 * Functional description
 *	Delete the tail of a large object.  This is called only from
 *	DPM_delete_relation.
 *
 **************************************/
BLH	blob;
USHORT	flags;
WIN	window;
DPG	dpage;
BLP	bpage;
SLONG	page_number, *page1, *end1, *page2, *end2;

SET_TDBB (tdbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("delete_tail (header, length)\n");
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    transaction %d flags %d fragment %d:%d back %d:%d\n", 
	header->rhdf_transaction, header->rhdf_flags, header->rhdf_f_page,
	header->rhdf_f_line, header->rhdf_b_page, header->rhdf_b_line);
#endif

window.win_flags = WIN_large_scan;
window.win_scans = 1;

/* If the object isn't a blob, things are a little simpler. */

if (!(header->rhdf_flags & rhd_blob))
    {
    page_number = header->rhdf_f_page;
    while (TRUE)
	{
	window.win_page = page_number;
	dpage = (DPG) CCH_FETCH (tdbb, &window, LCK_read, pag_data);
	header = (RHDF) ((UCHAR*) dpage + dpage->dpg_rpt [0].dpg_offset);
	flags = header->rhdf_flags;
	page_number = header->rhdf_f_page;
	CCH_RELEASE_TAIL (tdbb, &window);
	PAG_release_page (window.win_page, 0);
	if (!(flags & rhd_incomplete))
	    break;
	}
    return;
    }

/* Object is a blob, and a big one at that */

blob = (BLH) header;
page1 = blob->blh_page;
end1 = page1 + (length - BLH_SIZE) / sizeof (SLONG);

for (; page1 < end1; page1++)
    {
    if (blob->blh_level == 2)
	{
	window.win_page = *page1;
	bpage = (BLP) CCH_FETCH (tdbb, &window, LCK_read, pag_blob);
	page2 = bpage->blp_page;
	end2 = page2 + ((bpage->blp_length - BLP_SIZE) / sizeof (SLONG));
	while (page2 < end2)
	    PAG_release_page (*page2++, 0);
	CCH_RELEASE_TAIL (tdbb, &window);
	}
    PAG_release_page (*page1, 0);
    }
}

static void fragment ( 
    TDBB	tdbb,
    RPB		*rpb,
    SSHORT	available_space,
    DCC		dcc,
    SSHORT	length,
    TRA    	transaction)
{
/**************************************
 *
 *	f r a g m e n t
 *
 **************************************
 *
 * Functional description
 *	DPM_update tried to replace a record on a page, but it doesn't
 *	fit.  The record, obviously, needs to be fragmented.  Stick as
 *	much as fits on the page and put the rest elsewhere (though not
 *	in that order.
 *
 *	Doesn't sound so bad does it?
 *
 *	Little do you know.  The challenge is that we have to put the
 *	head of the record in the designated page:line space and put
 *	the tail on a different page.  To maintain on-disk consistency
 *	we have to write the tail first.  But we don't want anybody
 *	messing with the head until we get back, and, if possible, we'd
 *	like to keep as much space on the original page as we can get.
 *	Making matters worse, we may be storing a new version of the
 *	record or we may be backing out an old one and replacing it 
 *	with one older still (replacing a dead rolled back record with
 *	the preceding version).
 *
 *	Here is the order of battle.   If we're creating a new version,
 *	and the previous version is not a delta, we compress the page
 *	and create a new record which claims all the available space
 *	(but which contains garbage).  That record will have our transaction
 *	id (so it won't be committed for anybody) and it will have legitimate
 *	back pointers to the previous version.
 *
 *	If we're creating a new version and the previous version is a delta,
 *	we leave the current version in place, putting our transaction id
 *	on it.  We then point the back pointers to the delta version of the
 *	same generation of the record, which will have the correct transaction
 *	id.  Applying deltas to the expanded form of the same version of the
 *	record is a no-op -- but it doesn't cost much and the case is rare.
 *
 *	If we're backing out a rolled back version, we've got another 
 *	problem.  The rpb we've got is for record version n - 1, not version
 *	n + 1.  (e.g. I'm transaction 32 removing the rolled back record
 *	created by transaction 28 and reinstating the committed version
 *	created by transaction 26.  The rpb I've got is for the record
 *	written by transaction 26 -- the delta flag indicates whether the
 *	version written by transaction 24 was a delta, the transaction id
 *	is for a committed transaction, and the back pointers go back to
 *	transaction 24's version.  So in that case, we slap our transaction
 *	is on transaction 28's version (the one we'll be eliminating) and
 *	don't change anything else.
 *
 *	In all cases, once the tail is written and safe, we come back to
 *	the head, store the remaining data, fix up the record header and
 *	everything is done.
 *
 **************************************/
DBB	dbb;
DPG	page;
WIN	*window;
RPB	tail_rpb;
SSHORT	line,  space;
USHORT	pre_header_length, post_header_length;
RHDF	header;
LLS	stack;                                              

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES)
    ib_printf ("fragment (rpb %d, available_space %d, dcc, length %d, transaction %d)\n", rpb->rpb_number, available_space, length, transaction ? transaction->tra_number : 0);
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    record %d:%d transaction %d back %d:%d fragment %d:%d flags %d\n", 
	rpb->rpb_page, rpb->rpb_line, rpb->rpb_transaction, rpb->rpb_b_page,
	rpb->rpb_b_line, rpb->rpb_f_page, rpb->rpb_f_line, rpb->rpb_flags);
#endif

/* Start by claiming what space exists on the page.  Note that
   DPM_update has already set the length for this record to zero,
   so we're all set for a compress. However, if the current
   version was re-stored as a delta, or if we're restoring an
   older version, we must preserve it here so
   that others may reconstruct it during fragmentation of the
   new version. */

window = &rpb->rpb_window;
page = (DPG) window->win_buffer;
line = rpb->rpb_line;

if (transaction->tra_number != rpb->rpb_transaction)
    {
    header = (RHDF) ((SCHAR*) page + page->dpg_rpt [line].dpg_offset);
    header->rhdf_transaction = transaction->tra_number;
    header->rhdf_flags |= rhd_gc_active;
    page->dpg_rpt [line].dpg_length = available_space = length;
    }
else
    { 
    if (rpb->rpb_flags & rpb_delta) 
	{
	header = (RHDF) ((SCHAR*) page + page->dpg_rpt [line].dpg_offset);
	header->rhdf_flags |= rpb_delta;
	page->dpg_rpt [line].dpg_length = available_space = length;
	}
    else
	{
	space = DPM_compress (tdbb, page) - available_space;
	header = (RHDF) ((SCHAR*) page + space);
	header->rhdf_flags = rhd_deleted;
	header->rhdf_f_page = header->rhdf_f_line = 0;
	page->dpg_rpt [line].dpg_offset = space;
	page->dpg_rpt [line].dpg_length = available_space;
	}
    header->rhdf_transaction = rpb->rpb_transaction;
    header->rhdf_b_page = rpb->rpb_b_page;
    header->rhdf_b_line = rpb->rpb_b_line; 
    }

if (dbb->dbb_wal)
    journal_segment (tdbb, window, line);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    rhdf_transaction %d, window record %d:%d, available_space %d, rhdf_flags %d, rhdf_f record %d:%d, rhdf_b record %d:%d\n", 
	header->rhdf_transaction, window->win_page, line, available_space,
	header->rhdf_flags, header->rhdf_f_page, header->rhdf_f_line, header->rhdf_b_page, header->rhdf_b_line);
#endif

CCH_RELEASE (tdbb, window);

/* The next task is to store the tail where it fits.  To do this, we
   next to compute the size (compressed) of the tail.  This requires
   first figuring out how much of the original record fits on the
   original page.  */

pre_header_length = SQZ_compress_length (
	dcc, rpb->rpb_address,
	(int) (available_space - RHDF_SIZE));

tail_rpb = *rpb;
tail_rpb.rpb_flags = rpb_fragment;
tail_rpb.rpb_b_page = 0;
tail_rpb.rpb_b_line = 0;
tail_rpb.rpb_address = rpb->rpb_address + pre_header_length;
tail_rpb.rpb_length = rpb->rpb_length - pre_header_length;
tail_rpb.rpb_window.win_flags = 0;

stack = NULL;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    about to store tail\n");
#endif

DPM_store (tdbb, &tail_rpb, &stack, DPM_other);

/* That was unreasonablly easy.  Now re-fetch the original page and
   fill in the fragment pointer */

page = (DPG) CCH_FETCH (tdbb, window, LCK_write, pag_data);
CCH_precedence (tdbb, window, tail_rpb.rpb_page);
CCH_MARK (tdbb, window);

header = (RHDF) ((SCHAR*) page + page->dpg_rpt [line].dpg_offset);
header->rhdf_flags = rhd_incomplete | rpb->rpb_flags;
header->rhdf_transaction = rpb->rpb_transaction;
header->rhdf_format = rpb->rpb_format_number;
header->rhdf_f_page = tail_rpb.rpb_page;
header->rhdf_f_line = tail_rpb.rpb_line;

if (transaction->tra_number != rpb->rpb_transaction)
    {
    header->rhdf_b_page = rpb->rpb_b_page;
    header->rhdf_b_line = rpb->rpb_b_line; 
    }


post_header_length = SQZ_compress (
	dcc, rpb->rpb_address, header->rhdf_data,
	(int) (available_space - RHDF_SIZE));

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    {
    ib_printf ("    fragment head \n");
    ib_printf ("    rhdf_trans %d, window record %d:%d, dpg_length %d\n\trhdf_flags %d, rhdf_f record %d:%d, rhdf_b record %d:%d\n", 
	header->rhdf_transaction, window->win_page, line, 
	page->dpg_rpt [line].dpg_length, header->rhdf_flags, header->rhdf_f_page, 
	header->rhdf_f_line, header->rhdf_b_page, header->rhdf_b_line);
    }
#endif

if (pre_header_length != post_header_length)
    {
    release_dcc (dcc->dcc_next);
    CCH_RELEASE (tdbb, window);
    BUGCHECK (252); /* msg 252 header fragment length changed */
    }

if (dbb->dbb_wal)
    journal_segment (tdbb, window, line);

release_dcc (dcc->dcc_next);
CCH_RELEASE (tdbb, window);
}

static void extend_relation ( 
    TDBB	tdbb,
    REL		relation,
    WIN		*window)
{
/**************************************
 *
 *	e x t e n d _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Extend a relation with a given page.  The window points to an
 *	already allocated, fetched, and marked data page to be inserted
 *	into the pointer pages for a given relation. 
 *	This routine returns a window on the datapage locked for write
 *
 **************************************/
DBB		dbb;
SLONG		*slots;
USHORT		pp_sequence, slot;
WIN		pp_window, new_pp_window;
UCHAR		*bits;
register PPG	ppage;
DPG		dpage;
VCL		vector;
JRNP		journal;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);
pp_window.win_flags = new_pp_window.win_flags = 0;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("     extend_relation (relation %d, window)\n", relation->rel_id);
#endif

/* Release faked page before fetching pointer page to prevent deadlocks. This is only
   a problem for multi-threaded servers using internal latches. The faked page may be
   dirty from its previous incarnation and involved in a precedence relationship. This
   special case may need a more general solution. */
   
CCH_RELEASE (tdbb, window);

/* Search pointer pages for an empty slot.
   If we run out of pointer pages, allocate another one.  Note that the code below
   is not careful in preventing deadlocks when it allocates a new pointer page:
    - the last already-existing pointer page is fetched with an exclusive latch,
    - allocation of a new pointer page requires the fetching of the PIP page with
	an exlusive latch.
   This might cause a deadlock.  Fortunately, pointer pages don't need to be
   allocated often. */

retry_after_latch_timeout:

for (pp_sequence = relation->rel_slot_space;; pp_sequence++)
    {
    if (!(ppage = get_pointer_page (tdbb, relation, &pp_window, pp_sequence, LCK_write)))
	BUGCHECK (253); /* msg 253 pointer page vanished from extend_relation */
    slots = ppage->ppg_page;
    for (slot = 0; slot < ppage->ppg_count; slot++, slots++)
	if (*slots == 0)
	    break;
    if (slot < ppage->ppg_count)
	break;
    if ((pp_sequence && ppage->ppg_count < dbb->dbb_dp_per_pp) ||
	(ppage->ppg_count < dbb->dbb_dp_per_pp - 1))
	{
	slot = ppage->ppg_count;
	break;
	}
    if (ppage->ppg_header.pag_flags & ppg_eof)
	{
	ppage = (PPG) DPM_allocate (tdbb, &new_pp_window);
	ppage->ppg_header.pag_type = pag_pointer;
	ppage->ppg_header.pag_flags |= ppg_eof;
	ppage->ppg_relation = relation->rel_id;
	ppage->ppg_sequence = ++pp_sequence;
	slot = 0;
	CCH_must_write (&new_pp_window);	
	CCH_RELEASE (tdbb, &new_pp_window);

	vector = (VCL) ALL_extend (&relation->rel_pages, pp_sequence + 1);
	vector->vcl_long [pp_sequence] = new_pp_window.win_page;
	if (relation->rel_id)
	    DPM_pages (tdbb, relation->rel_id, pag_pointer, (SLONG) pp_sequence, 
		       new_pp_window.win_page);
        relation->rel_slot_space = pp_sequence;

	ppage = (PPG) pp_window.win_buffer;
	CCH_MARK (tdbb, &pp_window);
	ppage->ppg_header.pag_flags &= ~ppg_eof;
	ppage->ppg_next = new_pp_window.win_page;
	if (dbb->dbb_wal)
	    CCH_journal_page (tdbb, &pp_window);
	ppage = (PPG) CCH_HANDOFF (tdbb, &pp_window, new_pp_window.win_page, LCK_write, pag_pointer);
	break;
	}
    CCH_RELEASE (tdbb, &pp_window);
    }

/* We've found a slot.  Stick in the pointer to the data page */

if (ppage->ppg_page [slot])
    CORRUPT (258); /* msg 258 page slot not empty */

/* Refetch newly allocated page that was released above.
   To prevent possible deadlocks (since we own already an exlusive latch and we
   are asking for another exclusive latch), time out on the latch after 1 second. */

dpage = (DPG) CCH_FETCH_TIMEOUT (tdbb, window, LCK_write, pag_undefined, -1);

/* In the case of a timeout, retry the whole thing. */

if (!dpage)
    {
    CCH_RELEASE (tdbb, &pp_window);
    goto retry_after_latch_timeout;
    }


CCH_MARK (tdbb, window);
dpage->dpg_sequence = (SLONG) pp_sequence * dbb->dbb_dp_per_pp + slot;
dpage->dpg_relation = relation->rel_id;
dpage->dpg_header.pag_type = pag_data;
relation->rel_data_space = pp_sequence;

if (dbb->dbb_wal)
    CCH_journal_page (tdbb, window);

CCH_RELEASE (tdbb, window);

CCH_precedence (tdbb, &pp_window, window->win_page);
CCH_MARK_SYSTEM (tdbb, &pp_window);
ppage->ppg_page [slot] = window->win_page;
ppage->ppg_min_space = MIN (ppage->ppg_min_space, slot);
ppage->ppg_count = MAX (ppage->ppg_count, slot + 1);
bits = (UCHAR*) (ppage->ppg_page + dbb->dbb_dp_per_pp);
bits [slot >> 2] &= ~(1 << ((slot & 3) << 1));
if (relation->rel_data_pages)
    ++relation->rel_data_pages;

if (dbb->dbb_wal)
    {
    journal.jrnp_type = JRNP_POINTER_SLOT;
    journal.jrnp_index = slot;
    journal.jrnp_length = sizeof (window->win_page);
    CCH_journal_record (tdbb, &pp_window, &journal, JRNP_SIZE,
	&window->win_page, sizeof (window->win_page));
    }

*window = pp_window;
CCH_HANDOFF (tdbb, window, ppage->ppg_page [slot], LCK_write, pag_data);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("   extended_relation (relation %d, window_page %d)\n", relation->rel_id, window->win_page);
#endif
}

static UCHAR *find_space (
    TDBB	tdbb,
    RPB		*rpb,
    SSHORT	size,
    LLS		*stack,
    REC		record,
    USHORT	type)
{
/**************************************
 *
 *	f i n d _ s p a c e
 *
 **************************************
 *
 * Functional description
 *	Find space of a given size on a data page.  If no space, return
 *	null.  If space is found, mark the page, set up the line field
 *	in the record parameter block, set up the offset/length on the
 *	data page, and return a pointer to the space.
 *
 *	To maintain page precedence when objects point to objects, a stack
 *	of pages of high precedence may be passed in.  
 *
 **************************************/
DBB			dbb;
DPG			page;
SSHORT			i, slot, aligned_size;
SSHORT			used, space, reserving;
struct dpg_repeat	*index;
RHD			header;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

aligned_size = ROUNDUP (size, ODS_ALIGNMENT);
page = (DPG) rpb->rpb_window.win_buffer;

/* Scan allocated lines looking for an empty slot, the high water mark,
   and the amount of space potentially available on the page */

space = dbb->dbb_page_size;
slot = 0;
used = HIGH_WATER (page->dpg_count);
reserving = !(dbb->dbb_flags & DBB_no_reserve);

for (i = 0, index = page->dpg_rpt; i < page->dpg_count; i++, index++)
    if (index->dpg_offset)
	{
	space = MIN (space, index->dpg_offset);
	used += ROUNDUP (index->dpg_length, ODS_ALIGNMENT);
	if (type == DPM_primary && reserving)
	    {
	    header = (RHD) ((SCHAR*) page + index->dpg_offset);
	    if (!header->rhd_b_page &&
		!(header->rhd_flags & (rhd_chain   | rhd_blob | 
				       rhd_deleted | rhd_fragment)))
		used += SPACE_FUDGE;
	    }
	}
    else
	if (!slot)
	    slot = i;

if (!slot)
    used += sizeof (struct dpg_repeat);

/* If there isn't space, give up */

if (aligned_size > (int) dbb->dbb_page_size - used)
    {
    CCH_MARK (tdbb, &rpb->rpb_window);
    page->dpg_header.pag_flags |= dpg_full;
    mark_full (tdbb, rpb);
    return NULL;
    }

/* There's space on page.  If the line index needs expansion, do so.
   If the page need to be compressed, compress it. */

while (*stack)
    CCH_precedence (tdbb, &rpb->rpb_window, (SLONG) LLS_POP (stack));
CCH_MARK (tdbb, &rpb->rpb_window);
i = page->dpg_count + ((slot) ? 0 : 1);

if (aligned_size > space - HIGH_WATER (i))
    space = DPM_compress (tdbb, page);

if (!slot)
    slot = page->dpg_count++;

space -= aligned_size;
index = &page->dpg_rpt [slot];
index->dpg_length = size;
index->dpg_offset = space;
rpb->rpb_page = rpb->rpb_window.win_page;
rpb->rpb_line = slot;
rpb->rpb_number = (SLONG) page->dpg_sequence * dbb->dbb_max_records + slot;

if (record)
    LLS_PUSH ((BLK) rpb->rpb_page, &record->rec_precedence);

return (UCHAR*) page + space;
}

static BOOLEAN get_header (
    WIN			*window,
    SSHORT		line,
    register RPB	*rpb)
{
/**************************************
 *
 *	g e t _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Copy record header fields into a record parameter block.  If
 *	the line is empty, return false;
 *
 **************************************/
DPG		page;
register RHDF	header;
register struct dpg_repeat	*index;

page = (DPG) window->win_buffer;
if (line >= page->dpg_count)
    return FALSE;

index = &page->dpg_rpt [line];
if (index->dpg_offset == 0)
    return FALSE;

header = (RHDF) ((SCHAR*) page + index->dpg_offset);
rpb->rpb_page = window->win_page;
rpb->rpb_line = line;
rpb->rpb_flags = header->rhdf_flags;
	
if (!(rpb->rpb_flags & rpb_fragment))
    {
    rpb->rpb_b_page = header->rhdf_b_page;
    rpb->rpb_b_line = header->rhdf_b_line;
    rpb->rpb_transaction = header->rhdf_transaction;
    rpb->rpb_format_number = header->rhdf_format;
    }

if (rpb->rpb_flags & rpb_incomplete)
    {
    rpb->rpb_f_page = header->rhdf_f_page;
    rpb->rpb_f_line = header->rhdf_f_line;
    rpb->rpb_address = header->rhdf_data;
    rpb->rpb_length = index->dpg_length - RHDF_SIZE;
    }
else
    {
    rpb->rpb_address = ((RHD) header)->rhd_data;
    rpb->rpb_length = index->dpg_length - RHD_SIZE;
    }

return TRUE;
}

static PPG get_pointer_page (
    TDBB	tdbb,
    REL		relation,
    WIN		*window,
    USHORT	sequence,
    USHORT	lock)
{
/**************************************
 *
 *	g e t _ p o i n t e r _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Fetch a specific pointer page.  If we don't know about it,
 *	do a re-scan of RDB$PAGES to find it.  If that doesn't work,
 *	try the sibling pointer.  If that doesn't work, just stop,
 *	return NULL, and let our caller think about it.
 *
 **************************************/
PPG	page;
VCL	vector;
SLONG	next_ppg;

SET_TDBB (tdbb);

if (!(vector = relation->rel_pages) || sequence >= vector->vcl_count)
    {
    for (;;)
	{
	DPM_scan_pages (tdbb);
	/* If the relation is gone, then we can't do anything anymore. */
	if ((!relation) || (!(vector = relation->rel_pages)))
	    return NULL;
	if (sequence < vector->vcl_count)
	    break; /* we are in business again */
	window->win_page = vector->vcl_long [vector->vcl_count - 1];
    	page = (PPG) CCH_FETCH (tdbb, window, lock, pag_pointer);
	next_ppg = page->ppg_next;
	CCH_RELEASE (tdbb, window);
	if (!next_ppg)
	    return NULL;
	DPM_pages (tdbb, relation->rel_id,  pag_pointer, vector->vcl_count, next_ppg);
	}
    }
    
window->win_page = vector->vcl_long [sequence];
page = (PPG) CCH_FETCH (tdbb, window, lock, pag_pointer);

if (page->ppg_relation != relation->rel_id ||
    page->ppg_sequence != sequence)
    CORRUPT (259); /* msg 259 bad pointer page */

return page;
}

static void journal_segment (
    TDBB	tdbb,
    WIN		*window,
    USHORT	slot)
{
/**************************************
 *
 *	j o u r n a l _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	We just did something to a segment.  Make sure it gets added to
 *	the journal.
 *
 **************************************/
JRNP		journal;
DPG		page;
struct dpg_repeat	*index;

SET_TDBB (tdbb);

page = (DPG) window->win_buffer;
index = &page->dpg_rpt [slot];
journal.jrnp_type = JRNP_DATA_SEGMENT;
journal.jrnp_index = slot;
journal.jrnp_length = index->dpg_length;

CCH_journal_record (tdbb, window, &journal, JRNP_SIZE, 
	(UCHAR*) page + index->dpg_offset, index->dpg_length);
}

static RHD locate_space (
    TDBB	tdbb,
    register RPB	*rpb,
    SSHORT		size,
    LLS			*stack,
    REC			record,
    USHORT		type)
{
/**************************************
 *
 *	l o c a t e _ s p a c e
 *
 **************************************
 *
 * Functional description
 *	Find space in a relation for a record.  Find a likely data page
 *	and call find_space to see if there really is space there.  If
 *	we can't find any space, extend the relation.
 *
 **************************************/
DBB		dbb;
register WIN	*window;
REL		relation;
register PPG	ppage;
USHORT		pp_sequence, slot, i;
SLONG		dp_number, pp_number, sequence;
UCHAR		*bits, flags, *space;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

relation = rpb->rpb_relation;
window = &rpb->rpb_window;

/* If there is a preferred page, try there first */

if (type == DPM_secondary)
    {
    DECOMPOSE_QUOTIENT (rpb->rpb_number, dbb->dbb_max_records, sequence);
    DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);
    if (ppage = get_pointer_page (tdbb, relation, window, pp_sequence, LCK_read)) 
	if (slot < ppage->ppg_count && (dp_number = ppage->ppg_page [slot]))
	    {
	    CCH_HANDOFF (tdbb, window, dp_number, LCK_write, pag_data);
	    if (space = find_space (tdbb, rpb, size, stack, record, type))
		return (RHD) space;
	    }
	else
	    CCH_RELEASE (tdbb, window);
    }

/* Look for space anywhere */

for (pp_sequence = relation->rel_data_space;; pp_sequence++)
    {
    relation->rel_data_space = pp_sequence;
    if (!(ppage = get_pointer_page (tdbb, relation, window, pp_sequence, LCK_read)))
	BUGCHECK (254); /* msg 254 pointer page vanished from relation list in locate_space */
    pp_number = window->win_page;
    bits = (UCHAR*) (ppage->ppg_page + dbb->dbb_dp_per_pp);
    for (slot = ppage->ppg_min_space; slot < ppage->ppg_count; slot++)
	if ((dp_number = ppage->ppg_page [slot]) &&
	    ~bits [slot >> 2] & (1 << ((slot & 3) << 1)))
	    {
	    CCH_HANDOFF (tdbb, window, dp_number, LCK_write, pag_data);
	    if (space = find_space (tdbb, rpb, size, stack, record, type))
		 return (RHD) space;
	    window->win_page = pp_number;
	    ppage = (PPG) CCH_FETCH (tdbb, window, LCK_read, pag_pointer);
	    }
    flags = ppage->ppg_header.pag_flags;
    CCH_RELEASE (tdbb, window);
    if (flags & ppg_eof)
	break;
    }

/* Sigh.  No space.  Extend relation. Try for a while
   in case someone grabs the page before we can get it 
   locked, then give up on the assumption that things 
   are really screwed up. */

for (i = 0; i < 20; i++)
    {
    DPM_allocate (tdbb, window);
    extend_relation (tdbb, relation, window);
    if (space = find_space (tdbb, rpb, size, stack, record, type))
	break;
    }
if (i == 20)
    BUGCHECK (255); /* msg 255 cannot find free space */

if (record)
    LLS_PUSH ((BLK) window->win_page, &record->rec_precedence);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    ib_printf ("    extended relation %d with page %d to get %d bytes\n", relation->rel_id, window->win_page);
#endif

return (RHD) space;
}

static void mark_full (
    TDBB	tdbb,
    register RPB	*rpb)
{
/**************************************
 *
 *	m a r k _ f u l l
 *
 **************************************
 *
 * Functional description
 *	Mark a fetched page and it's pointer page to indicate the page
 *	is full.
 *
 **************************************/
DBB		dbb;
WIN		pp_window;
register DPG	dpage;
register PPG	ppage;
UCHAR		*byte, bit, flags;
USHORT		slot, pp_sequence;
SLONG		sequence;
REL		relation;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);
pp_window.win_flags = 0;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("mark_full ()\n");
#endif

/* We need to access the pointer page for write.  To avoid deadlocks,
   we need to release the data page, fetch the pointer page for write,
   and re-fetch the data page.  If the data page is still empty, set
   it's "full" bit on the pointer page. */

dpage = (DPG) rpb->rpb_window.win_buffer;
sequence = dpage->dpg_sequence;
CCH_RELEASE (tdbb, &rpb->rpb_window);

relation = rpb->rpb_relation;
DECOMPOSE (sequence, dbb->dbb_dp_per_pp, pp_sequence, slot);

/* Fetch the pointer page, then the data page.  Since this is a case of
   fetching a second page after having fetched the first page with an 
   exclusive latch, care has to be taken to prevent a deadlock.  This
   is accomplished by timing out the second latch request and retrying
   the whole thing. */

do
    {
    if (!(ppage = get_pointer_page (tdbb, relation, &pp_window, pp_sequence, LCK_write)))
    	BUGCHECK (256); /* msg 256 pointer page vanished from mark_full */

    /* If data page has been deleted from relation then there's nothing
       left to do. */
    if (slot >= ppage->ppg_count ||
    	rpb->rpb_window.win_page != ppage->ppg_page [slot])
    	{
    	CCH_RELEASE (tdbb, &pp_window);
    	return;
    	}

    /* Fetch the data page, but timeout after 1 second to break a possible deadlock. */
    dpage = (DPG) CCH_FETCH_TIMEOUT (tdbb, &rpb->rpb_window, LCK_read, pag_data, -1);

    /* In case of a latch timeout, release the latch on the pointer page and retry. */
    if (!dpage)
	CCH_RELEASE (tdbb, &pp_window);
    }
while (!dpage);


flags = dpage->dpg_header.pag_flags;
CCH_RELEASE (tdbb, &rpb->rpb_window);

CCH_precedence (tdbb, &pp_window, rpb->rpb_window.win_page);
CCH_MARK (tdbb, &pp_window);

bit = 1 << ((slot & 3) << 1);
byte = (UCHAR*) (&ppage->ppg_page [dbb->dbb_dp_per_pp]) + (slot >> 2);

if (flags & dpg_full)
    {
    *byte |= bit;
    ppage->ppg_min_space = MAX (slot + 1, ppage->ppg_min_space);
    }
else
    {
    *byte &= ~bit;
    ppage->ppg_min_space = MIN (slot, ppage->ppg_min_space);
    relation->rel_data_space = MIN (pp_sequence, relation->rel_data_space);
    }

/* Next, handle the "large object" bit */

bit <<= 1;

if (flags & dpg_large)
    *byte |= bit;
else
    *byte &= ~bit;

CCH_RELEASE (tdbb, &pp_window);
}

static void release_dcc (
    DCC		dcc)
{
/**************************************
 *
 *	r e l e a s e _ d c c
 *
 **************************************
 *
 * Functional description
 *	Release a chain of data compression control block.
 *
 **************************************/
DCC	temp;
PLB	pool;

while (dcc)
    {
    temp = dcc;
    dcc = temp->dcc_next;
    pool = temp->dcc_pool;
    temp->dcc_next = pool->plb_dccs;
    pool->plb_dccs = temp;
    }
}

static void store_big_record (
    TDBB	tdbb,
    register RPB	*rpb,
    LLS			*stack,
    DCC			head_dcc,
    USHORT		size)
{
/**************************************
 *
 *	s t o r e _ b i g _ r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Store a new record in a relation.
 *
 **************************************/
DBB		dbb;
DCC		dcc, temp_dcc;
DPG		page;
RHDF		header;
SCHAR		*out, *in;
SCHAR		*control, count;
USHORT		length, max_data, l, n;
SLONG		prior;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_TRACE_ALL)
    ib_printf ("store_big_record ()\n");
#endif

/* Start by finding the last data compression control block and set up
    to start decompression from the end. */

for (dcc = head_dcc; dcc->dcc_next; dcc = dcc->dcc_next)
    ;

control = dcc->dcc_end;
in = (SCHAR*) rpb->rpb_address + rpb->rpb_length;
prior = 0;
count = 0;
max_data = dbb->dbb_page_size - (sizeof (struct dpg) + RHDF_SIZE);

/* Fill up data pages tail first until what's left fits on a single
   page. */

while (size > max_data)
    {
    /* Allocate and format data page and fragment header */

    page = (DPG) DPM_allocate (tdbb, &rpb->rpb_window);
    page->dpg_header.pag_type = pag_data;
    page->dpg_header.pag_flags = dpg_orphan | dpg_full;
    page->dpg_relation = rpb->rpb_relation->rel_id;
    page->dpg_count = 1;
    header = (RHDF) &page->dpg_rpt [1];
    page->dpg_rpt [0].dpg_offset = (UCHAR*) header - (UCHAR*) page;
    page->dpg_rpt [0].dpg_length = max_data + RHDF_SIZE;
    header->rhdf_flags = (prior) ? 
		rhd_fragment | rhd_incomplete : rhd_fragment;
    header->rhdf_f_page = prior;
    length = max_data;
    size -= length;
    out = (SCHAR*) header->rhdf_data + length;

    /* Move compressed data onto page */

    while (length > 1)
	{
	/* Handle residual count, if any */
	if (count > 0)
	    {
	    n = l = MIN ((USHORT) count, length - 1);
	    do *--out = *--in; while (--n);
	    *--out = l;
	    length -= (SSHORT) (l + 1);	/* bytes remaining on page */
	    count -= (SSHORT) l;		/* bytes remaining in run */
	    continue;
	    }

	/* If we've exhausted this data compression block, find the prior one */

	if (control == dcc->dcc_string)
	    {
	    temp_dcc = dcc;
	    for (dcc = head_dcc; dcc->dcc_next != temp_dcc; dcc = dcc->dcc_next)
		;
	    control = dcc->dcc_string + sizeof (dcc->dcc_string);
	    }
	if ((count = *--control) < 0)
	    {
	    *--out = in [-1];
	    *--out = count;
	    in += count;
	    length -= 2;
	    }
	}

    /* Page is full.  If there is an odd byte left, fudge it. */

    if (length)
	{
	*--out = 0;
	++size;
	}
    else if (count > 0)
	++size;
    if (prior)
	CCH_precedence (tdbb, &rpb->rpb_window, prior);

#ifdef VIO_DEBUG
    if (debug_flag > DEBUG_WRITES_INFO)
	{
	ib_printf ("    back portion\n");
	ib_printf ("    rpb_window page %d, max_data %d, \n\trhdf_flags %d, prior %d\n", 
	    rpb->rpb_window.win_page, max_data, 
	    header->rhdf_flags, prior); 
	}
#endif

    if (dbb->dbb_wal)
	CCH_journal_page (tdbb, &rpb->rpb_window);

    CCH_RELEASE (tdbb, &rpb->rpb_window);
    prior = rpb->rpb_window.win_page;
    }

/* What's left fits on a page.  Luckily, we don't have to store it
   ourselves. */

release_dcc (head_dcc->dcc_next);
size = SQZ_length (tdbb, rpb->rpb_address, in - (SCHAR*) rpb->rpb_address, head_dcc);
header = (RHDF) locate_space (tdbb, rpb, RHDF_SIZE + size, stack, NULL_PTR, DPM_other);

header->rhdf_flags = rhd_incomplete | rhd_large | rpb->rpb_flags;
header->rhdf_transaction = rpb->rpb_transaction;
header->rhdf_format = rpb->rpb_format_number;
header->rhdf_b_page = rpb->rpb_b_page;
header->rhdf_b_line = rpb->rpb_b_line;
header->rhdf_f_page = prior;
header->rhdf_f_line = 0;
SQZ_fast (head_dcc, rpb->rpb_address, header->rhdf_data);
release_dcc (head_dcc->dcc_next);

if (dbb->dbb_wal)
    journal_segment (tdbb, &rpb->rpb_window, rpb->rpb_line);

page = (DPG) rpb->rpb_window.win_buffer;

#ifdef VIO_DEBUG
if (debug_flag > DEBUG_WRITES_INFO)
    {
    ib_printf ("    front part\n");
    ib_printf ("    rhdf_trans %d, rpb_window record %d:%d, dpg_length %d \n\trhdf_flags %d, rhdf_f record %d:%d, rhdf_b record %d:%d\n", 
	header->rhdf_transaction, rpb->rpb_window.win_page, rpb->rpb_line, 
	page->dpg_rpt[rpb->rpb_line].dpg_length, header->rhdf_flags, 
	header->rhdf_f_page, header->rhdf_f_line, header->rhdf_b_page, header->rhdf_b_line);
    }
#endif

if (!(page->dpg_header.pag_flags & dpg_large))
    {
    page->dpg_header.pag_flags |= dpg_large;
    mark_full (tdbb, rpb);
    }
else
    CCH_RELEASE (tdbb, &rpb->rpb_window);
}
