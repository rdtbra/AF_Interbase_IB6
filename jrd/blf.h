/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		blf.h
 *	DESCRIPTION:	Blob filter interface definitions
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

#ifndef _JRD_BLF_H_
#define _JRD_BLF_H_

/* Note: The CTL structure is the internal version of the
 * blob control structure (ISC_BLOB_CTL) which is in ibase.h.
 * Therefore this structure should be kept in sync with that one,
 * with the exception of the internal members, which are all the
 * once which appear after ctl_internal. */

typedef struct ctl {
    STATUS	(*ctl_source)();		/* Source filter */
    struct ctl	*ctl_source_handle;		/* Argument to pass to source filter */
    SSHORT	ctl_to_sub_type;		/* Target type */
    SSHORT	ctl_from_sub_type;		/* Source type */
    USHORT	ctl_buffer_length;		/* Length of buffer */
    USHORT	ctl_segment_length;		/* Length of current segment */
    USHORT	ctl_bpb_length;			/* Length of blob parameter block */
    SCHAR	*ctl_bpb;			/* Address of blob parameter block */
    UCHAR	*ctl_buffer;			/* Address of segment buffer */
    SLONG	ctl_max_segment;		/* Length of longest segment */
    SLONG	ctl_number_segments;		/* Total number of segments */
    SLONG	ctl_total_length;		/* Total length of blob */
    STATUS	*ctl_status;			/* Address of status vector */
    IPTR	ctl_data [8];			/* Application specific data */
    void	*ctl_internal [3];		/* InterBase internal-use only */
    UCHAR	*ctl_exception_message;		/* Message to use in case of filter exception */
} *CTL;

typedef STATUS	(*PTR)();

/* Blob filter management */

typedef struct blf {
    struct blk	blf_header;
    struct blf	*blf_next;		/* Next known filter */
    SSHORT	blf_from;		/* Source sub-type */
    SSHORT	blf_to;			/* Target sub-type */
    STATUS	(*blf_filter)();	/* Entrypoint of filter */
    STR		blf_exception_message;	/* message to be used in case of filter exception */
} *BLF;

#define ACTION_open		0
#define ACTION_get_segment	1
#define ACTION_close		2
#define ACTION_create		3
#define ACTION_put_segment	4
#define ACTION_alloc		5
#define ACTION_free		6
#define ACTION_seek		7

#define EXCEPTION_MESSAGE "The blob filter: \t\t%s\n\treferencing entrypoint: \t%s\n\t             in module: \t%s\n\tcaused the fatal exception:"

#endif /* _JRD_BLF_H_ */
