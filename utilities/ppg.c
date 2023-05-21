/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ppg.c
 *	DESCRIPTION:	Database page print module
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
#include "../jrd/common.h"
#include "../jrd/time.h"
#include "../jrd/gds.h"
#include "../jrd/ods.h"
#include "../jrd/gds_proto.h"

static CONST TEXT	months [][4] = {
		"Jan", "Feb", "Mar", 
		"Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", 
		"Oct", "Nov", "Dec" };

#ifdef SUPERSERVER
typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id_mod;
    USHORT	blk_length;
} *BLK;

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"
#include "../utilities/ppg_proto.h"

#define exit(code)	{service->svc_handle = 0; return (code);}
#define FPRINTF		SVC_fprintf
#endif

#ifndef FPRINTF
#define FPRINTF		ib_fprintf
#endif

void PPG_print_header (
    HDR		header,
    SLONG	page,
#ifdef SUPERSERVER
    SVC		outfile)
#else
    IB_FILE	*outfile)
#endif
{
/**************************************
 *
 *	P P G _ p r i n t _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Print database header page.
 *
 **************************************/
UCHAR		*p, *end;
SLONG		number;
struct tm	time;
SSHORT		flags;
TEXT		temp [257];
SSHORT		flag_count = 0;

if (page == HEADER_PAGE)
    FPRINTF (outfile, "Database header page information:\n");
else
    FPRINTF (outfile, "Database overflow header page information:\n");


if (page == HEADER_PAGE)
    {
    FPRINTF (outfile, "\tFlags\t\t\t%d\n", header->hdr_header.pag_flags);
    FPRINTF (outfile, "\tChecksum\t\t%d\n", header->hdr_header.pag_checksum);
    FPRINTF (outfile, "\tGeneration\t\t%d\n", header->hdr_header.pag_generation);
    FPRINTF (outfile, "\tPage size\t\t%d\n", header->hdr_page_size);
    FPRINTF (outfile, "\tODS version\t\t%d.%d\n", header->hdr_ods_version,
					header->hdr_ods_minor);
    FPRINTF (outfile, "\tOldest transaction\t%ld\n", header->hdr_oldest_transaction);
    FPRINTF (outfile, "\tOldest active\t\t%ld\n", header->hdr_oldest_active);
    FPRINTF (outfile, "\tOldest snapshot\t\t%ld\n", header->hdr_oldest_snapshot);
    FPRINTF (outfile, "\tNext transaction\t%ld\n", header->hdr_next_transaction);
    FPRINTF (outfile, "\tBumped transaction\t%ld\n", header->hdr_bumped_transaction);
    FPRINTF (outfile, "\tSequence number\t\t%d\n", header->hdr_sequence);

    FPRINTF (outfile, "\tNext attachment ID\t%ld\n", header->hdr_attachment_id);
    FPRINTF (outfile, "\tImplementation ID\t%ld\n", header->hdr_implementation);
    FPRINTF (outfile, "\tShadow count\t\t%ld\n", header->hdr_shadow_count);
    FPRINTF (outfile, "\tPage buffers\t\t%d\n", header->hdr_page_buffers);
    }

FPRINTF (outfile, "\tNext header page\t%d\n", header->hdr_next_page);
#ifdef DEV_BUILD
FPRINTF (outfile, "\tClumplet End\t\t%d\n", header->hdr_end);
#endif

if (page == HEADER_PAGE)
    {

    /* If the database dialect is not set to 3, then we need to
     * assume it was set to 1.  The reason for this is that a dialect
     * 1 database has no dialect information written to the header.
     */
    if (header->hdr_flags & hdr_SQL_dialect_3)
	FPRINTF (outfile, "\tDatabase dialect\t3\n");
    else
        FPRINTF (outfile, "\tDatabase dialect\t1\n");
    
    
    gds__decode_date (header->hdr_creation_date, &time);
    FPRINTF (outfile, "\tCreation date\t\t%s %d, %d %d:%02d:%02d\n",
	months [time.tm_mon], time.tm_mday, time.tm_year + 1900,
	time.tm_hour, time.tm_min, time.tm_sec);
    }

if ((page == HEADER_PAGE) && (flags = header->hdr_flags))
    {
    FPRINTF (outfile, "\tAttributes\t\t");
    if (flags & hdr_force_write)
	{
	FPRINTF (outfile, "force write");
	flag_count++;
	}
    if (flags & hdr_no_reserve)
	{
	if (flag_count++)
	    FPRINTF (outfile, ", ");
	FPRINTF (outfile, "no reserve");
	}

    if (flags & hdr_disable_cache)
	{
	if (flag_count++)
	    FPRINTF (outfile, ", ");
	FPRINTF (outfile, "shared cache disabled");
	}

    if (flags & hdr_active_shadow)
	{
	if (flag_count++)
	    FPRINTF (outfile, ", ");
	FPRINTF (outfile, "active shadow");
	}

    if (flags & hdr_shutdown)
	{
	if (flag_count++)
	    FPRINTF (outfile, ", ");
	FPRINTF (outfile, "database shutdown");
	}

#ifdef READONLY_DATABASE
    if (flags & hdr_read_only)
	{
    	if (flag_count++)
    	    FPRINTF (outfile, ", ");
	FPRINTF (outfile, "read only");
	}
#endif  /* READONLY_DATABASE */
    FPRINTF (outfile, "\n");
    }

FPRINTF (outfile, "\n    Variable header data:\n");

for (p = header->hdr_data, end = p + header->hdr_page_size; 
     p < end && *p != HDR_end; p += 2 + p [1])
    switch (*p)
	{
	case HDR_root_file_name:
	    memcpy (temp, p+2, p [1]);
	    temp [p[1]] = '\0';
	    FPRINTF (outfile, "\tRoot file name:\t\t%s\n", temp);
	    break;

	case HDR_journal_server:
	    memcpy (temp, p+2, p [1]);
	    temp [p[1]] = '\0';
	    FPRINTF (outfile, "\tJournal server:\t\t%s\n", temp);
	    break;

	case HDR_file:
	    memcpy (temp, p+2, p [1]);
	    temp [p[1]] = '\0';
	    FPRINTF (outfile, "\tContinuation file:\t\t%s\n", temp);
	    break;

	case HDR_last_page:
	    memcpy (&number, p+2, sizeof (number));
	    FPRINTF (outfile, "\tLast logical page:\t\t%ld\n", number);
	    break;

	case HDR_unlicensed:
	    memcpy (&number, p+2, sizeof (number));
	    FPRINTF (outfile, "\tUnlicensed accesses:\t\t%ld\n", number);
	    break;

	case HDR_sweep_interval:
	    memcpy (&number, p+2, sizeof (number));
	    FPRINTF (outfile, "\tSweep interval:\t\t%ld\n", number);
	    break;

	case HDR_log_name:
	    memcpy (temp, p+2, p [1]);
	    temp [p[1]] = '\0';
	    FPRINTF (outfile, "\tReplay logging file:\t\t%s\n", temp);
	    break;

	case HDR_cache_file:
	    FPRINTF (outfile, "\tShared Cache file:\t\t%s\n", p + 2);
	    break;

	default:
	    if (*p > HDR_max)
		FPRINTF (outfile, "\tUnrecognized option %d, length %d\n", p [0], p [1]);
	    else
		FPRINTF (outfile, "\tEncoded option %d, length %d\n", p [0], p [1]);
	    break;
	}

FPRINTF (outfile, "\t*END*\n");
}

void PPG_print_log (
    LIP		logp,
    SLONG	page,
#ifdef SUPERSERVER
    SVC		outfile)
#else
    IB_FILE	*outfile)
#endif
{
/**************************************
 *
 *	P P G _ p r i n t _ l o g
 *
 **************************************
 *
 * Functional description
 *	Print log page information
 *
 **************************************/
UCHAR 		*p;
SLONG		flags;
SLONG		ltemp;
SSHORT		stemp;
USHORT		ustemp;
struct tm	time;
TEXT		temp [257];

p = logp->log_data;

if (page == LOG_PAGE)
    FPRINTF (outfile, "Database log page information:\n");
else
    FPRINTF (outfile, "Database overflow log page information:\n");

if (page == LOG_PAGE)
    {
    if (logp->log_mod_tid)
	FPRINTF (outfile, "\tModified by transaction\t%ld\n", logp->log_mod_tid);
    if (logp->log_mod_tip)
	FPRINTF (outfile, "\tModified tip page\t%ld\n", logp->log_mod_tip);

    if (!logp->log_creation_date [0] && !logp->log_creation_date [1])
       	FPRINTF (outfile, "\tCreation date\n");
    else
    	{
    	gds__decode_date (logp->log_creation_date, &time);
    	FPRINTF (outfile, "\tCreation date\t%s %d, %d %d:%02d:%02d\n",
	    months [time.tm_mon], time.tm_mday, time.tm_year + 1900,
	    time.tm_hour, time.tm_min, time.tm_sec);
	}

    FPRINTF (outfile, "\tLog flags:\t%ld\n", logp->log_flags);

    if (flags = logp->log_flags)
	{
	if (flags & log_no_ail)
	    FPRINTF (outfile, "\t\tNo write ahead log\n");
	if (flags & log_add)
	    FPRINTF (outfile, "\t\tLog added\n");
	if (flags & log_delete)
	    FPRINTF (outfile, "\t\tLog deleted\n");
	if (flags & log_recover)
	    FPRINTF (outfile, "\t\tRecovery required\n");
	if (flags & log_rec_in_progress)
	    FPRINTF (outfile, "\t\tRecovery in progress\n");
	if (flags & log_partial_rebuild)
	    FPRINTF (outfile, "\t\tPartial recovery\n");
	FPRINTF (outfile, "\n");
	}
    }

FPRINTF (outfile, "\tNext log page:\t%ld\n", logp->log_next_page);
#ifdef DEV_BUILD
FPRINTF (outfile, "\tClumplet End\t%d\n", logp->log_end);
#endif

FPRINTF (outfile, "\n    Variable log data:\n");

for (p = logp->log_data; *p != HDR_end; p += 2 + p [1])
    {
    switch (*p)
	{
	case LOG_ctrl_file1:
	    FPRINTF (outfile, "\tControl Point 1:\n");
	    memcpy (temp, p+2, logp->log_cp_1.cp_fn_length);
	    temp [logp->log_cp_1.cp_fn_length] = '\0';
	    FPRINTF (outfile, "\t\tFile name:\t%s\n", temp);
	    FPRINTF (outfile, "\t\tPartition offset: %ld ", logp->log_cp_1.cp_p_offset);
	    FPRINTF (outfile, "\tSeqno: %ld ", logp->log_cp_1.cp_seqno);
	    FPRINTF (outfile, "\tOffset: %ld ", logp->log_cp_1.cp_offset);
	    FPRINTF (outfile, "\n");

	    break;

	case LOG_ctrl_file2:
	    FPRINTF (outfile, "\tControl Point 2:\n");
	    memcpy (temp, p+2, logp->log_cp_2.cp_fn_length);
	    temp [logp->log_cp_2.cp_fn_length] = '\0';
	    FPRINTF (outfile, "\t\tFile name:\t%s\n", temp);
	    FPRINTF (outfile, "\t\tPartition offset: %ld ", logp->log_cp_2.cp_p_offset);
	    FPRINTF (outfile, "\tSeqno: %ld ", logp->log_cp_2.cp_seqno);
	    FPRINTF (outfile, "\tOffset: %ld ", logp->log_cp_2.cp_offset);
	    FPRINTF (outfile, "\n");

	    break;

	case LOG_logfile:
	    FPRINTF (outfile, "\tCurrent File:\n");
	    memcpy (temp, p+2, logp->log_file.cp_fn_length);
	    temp [logp->log_file.cp_fn_length] = '\0';
	    FPRINTF (outfile, "\t\tFile name:\t\t%s\n", temp);
	    FPRINTF (outfile, "\t\tPartition offset: %ld ", logp->log_file.cp_p_offset);
	    FPRINTF (outfile, "\tSeqno: %ld ", logp->log_file.cp_seqno);
	    FPRINTF (outfile, "\tOffset: %ld ", logp->log_file.cp_offset);
	    FPRINTF (outfile, "\n");

	    break;

	case LOG_chkpt_len:
	    memcpy (&ltemp, p+2, p[1]);
	    FPRINTF (outfile, "\tCheck Point Length %ld\n", ltemp);
	    break;

	case LOG_num_bufs:
	    memcpy (&stemp, p+2, p[1]);
	    FPRINTF (outfile, "\tNumber of WAL buffers %d\n", stemp);
	    break;

	case LOG_bufsize:
	    memcpy (&ustemp, p+2, p[1]);
	    FPRINTF (outfile, "\tWAL buffer Size %ld\n", ustemp);
	    break;

	case LOG_grp_cmt_wait:
	    memcpy (&ltemp, p+2, p[1]);
	    FPRINTF (outfile, "\tGroup Commit Wait Time %ld\n", ltemp);
	    break;

	default:
	    if (*p > LOG_max)
		FPRINTF (outfile, "\tUnrecognized option %d, length %d\n", p [0], p [1]);
	    else
		FPRINTF (outfile, "\tEncoded option %d, length %d\n", p [0], p [1]);
	    break;
	}
    }
FPRINTF (outfile, "\t*END*\n");
}

