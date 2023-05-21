/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		blb.h
 *	DESCRIPTION:	Blob handling definitions
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

#ifndef _JRD_BLB_H_
#define _JRD_BLB_H_

/* Blob id.  A blob has two states -- temporary and permanent.  In each
   case, the blob id is 8 bytes (2 longwords) long.  In the case of a
   temporary blob, the first word is NULL and the second word points to
   an internal blob block.  In the case of a permanent blob, the first
   word contains the relation id of the blob and the second the record
   number of the first segment-clump.  The two types of blobs can be
   reliably distinguished by a zero or non-zero relation id. */

typedef struct bid {
#ifndef DECOSF
    ULONG	bid_relation_id;	/* Relation id (or null) */
    union
    {
    struct blb	*bid_blob;		/* Pointer to blob block */
    ULONG	bid_number;		/* Record number */
    }		bid_stuff;
#else
    union
    {
    ULONG	bid_blob;		/* Low end of pointer to blob block */
    ULONG	bid_number;		/* Record number */
    }		bid_stuff;
    USHORT	bid_blob2;		/* High end of pointer to blob block */
    USHORT	bid_relation_id;	/* Relation id (or null) */
#endif
} *BID;

/* Your basic blob block. */

typedef struct blb {
    struct blk	blb_header;
    struct att	*blb_attachment;	/* database attachment */
    struct rel	*blb_relation;		/* Relation, if known */
    struct tra	*blb_transaction;	/* Parent transaction block */
    struct blb	*blb_next;		/* Next blob in transaction */
    UCHAR	*blb_segment;		/* Next segment to be addressed */
    struct ctl	*blb_filter;		/* Blob filter control block, if any */
    struct bid	blb_blob_id;		/* Id of materialized blob */
    struct req	*blb_request;		/* request that assigned temporary blob */
#ifndef GATEWAY
    struct vcl	*blb_pages;		/* Vector of pages */
    USHORT	blb_pointers;		/* Max pointer on a page */
#else
    struct vec	*blb_pages;		/* Vector of pages */
#endif
    USHORT	blb_level;		/* Storage type */
    USHORT	blb_max_segment;	/* Longest segment */
    USHORT	blb_flags;		/* Interesting stuff (see below) */
    USHORT	blb_clump_size;		/* Size of data clump */
    USHORT	blb_space_remaining;	/* Data space left */
    USHORT	blb_max_pages;		/* Max pages in vector */
    USHORT	blb_fragment_size;	/* Residual fragment size */
    USHORT	blb_source_interp;	/* source interp (for writing) */
    USHORT	blb_target_interp;	/* destination interp (for reading) */
    SSHORT	blb_sub_type;		/* Blob's declared sub-type */
    ULONG	blb_sequence;		/* Blob page sequence */
    ULONG	blb_max_sequence;	/* Number of data pages */
    ULONG	blb_count;		/* Number of segments */
    ULONG	blb_length;		/* Total length of data sans segments */
    ULONG	blb_lead_page;		/* First page number */
    ULONG	blb_seek;		/* Seek location */
					/* blb_data must be longword aligned */
    UCHAR	blb_data [1];		/* A page's worth of blob */
} *BLB;

#define BLB_temporary	1		/* Newly created blob */
#define BLB_eof		2		/* This blob is exhausted */
#define BLB_stream	4		/* Stream style blob */
#define BLB_closed	8		/* Temporary blob has been closed */
#define BLB_damaged	16		/* Blob is busted */
#define BLB_seek	32		/* Seek is pending */
#define BLB_user_def	64		/* Blob is user created */
#define BLB_large_scan	128		/* Blob is larger than page buffer cache */

/* Blob levels are:

	0	small blob -- blob "record" is actual data
	1	medium blob -- blob "record" is pointer to pages
	2	large blob -- blob "record" is pointer to pages of pointers
*/

/* mapping blob ids for REPLAY */

typedef struct map {
    struct blk	map_header;
    struct map	*map_next;
    struct blb	*map_old_blob;
    struct blb	*map_new_blob;
} *MAP;

#endif /* _JRD_BLB_H_ */
