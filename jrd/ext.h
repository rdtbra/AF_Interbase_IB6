/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ext.h
 *	DESCRIPTION:	External file access definitions
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

#ifndef _JRD_EXT_H_
#define _JRD_EXT_H_

/* External file access block */

typedef struct ext {
    struct blk	ext_header;
    struct fmt	*ext_format;		/* External format */
    UCHAR	*ext_stuff;		/* Random stuff */
    USHORT	ext_flags;		/* Misc and cruddy flags */
#ifdef VMS
    int		ext_ifi;		/* Internal file identifier */
    int		ext_isi;		/* Internal stream (default) */
#else
    int		*ext_ifi;		/* Internal file identifier */
    int		*ext_isi;		/* Internal stream (default) */
#endif
    USHORT	ext_record_length;	/* Record length */
    USHORT	ext_file_type;		/* File type */
    USHORT	ext_index_count;	/* Number of indices */
    UCHAR	*ext_indices;		/* Index descriptions */
    UCHAR	ext_dbkey [8];		/* DBKEY */
    UCHAR	ext_filename [1];
} *EXT;

#define EXT_opened	1		/* File has been opened */
#define EXT_eof		2		/* Positioned at EOF */
#define EXT_readonly	4		/* File could only be opened for read */

typedef struct irsb_ext {
    USHORT	irsb_flags;		/* flags (a whole word!) */
    UCHAR	irsb_ext_dbkey [8];	/* DBKEY */
} *IRSB_EXT;

/* Overload record parameter block with external file stuff */

#define rpb_ext_pos	rpb_page
#define rpb_ext_isi	rpb_f_page
#define rpb_ext_dbkey	rpb_b_page

#endif /* _JRD_EXT_H_ */
