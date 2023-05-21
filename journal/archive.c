/*
 *	PROGRAM:	JRD Journal Server
 *	MODULE:		archive.c
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

#include "../jrd/ib_stdio.h"
#include <stdlib.h>
#include <string.h>
#include "../jrd/common.h"

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id;
    USHORT	blk_length;
} *BLK;

#include "../wal/wal.h"
#include "../jrd/jrn.h"
#include "../jrd/llio.h"
#include "../jrd/jrn_proto.h"
#include "../jrd/llio_proto.h"

#ifdef NETWARE_386
#define exit(code)	return
#endif

static USHORT	copy_file (SLONG, SLONG, SLONG);
static void	error_exit (STATUS *, JRN *, SLONG, SLONG, SLONG);
static USHORT	open_file (TEXT *, SLONG, USHORT, SLONG *);

#ifdef NETWARE_386
int main_archive (
#else
int CLIB_ROUTINE main (
    int		argc,
#endif
    char	*argv [])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *    backup a file to another disk, update journal database.
 *
 **************************************/
TEXT	*d_file;
TEXT	*s_file;
TEXT	*j_dir;
SLONG	file_id;
SLONG	s_fd, d_fd;
SLONG	p_offset;
USHORT	j_length;
USHORT	ret_val;
SLONG	size;
SLONG	db_id;
STATUS	status [20];
JRN	journal;
#ifdef NETWARE_386
int	argc;
#endif

journal = (JRN) 0;

#ifdef NETWARE_386
argc = argv [0];
#endif
argv++;

db_id    = atoi (*argv++);
file_id  = atoi (*argv++);
p_offset = atoi (*argv++);
size     = atoi (*argv++);
s_file	 = *argv++;
d_file   = *argv++;
j_dir    = *argv++;
j_length = strlen (j_dir);

/* Attach with journal server */

if (JRN_archive_begin (status, &journal, db_id, file_id, j_dir, j_length) != SUCCESS)
    error_exit (status, &journal, db_id, file_id, 235); /* msg 235: Archive process unable to attach to journal server. */

/* Check in with database */

if (open_file (s_file, p_offset, LLIO_OPEN_R, &s_fd) == FAILURE)
    error_exit (status, &journal, db_id, file_id, 236); /* msg 236: Archive process unable to open log file. */

if (open_file (d_file, 0L, LLIO_OPEN_NEW_RW, &d_fd) == FAILURE)
    {
    LLIO_close (NULL_PTR, s_fd);
    error_exit (status, &journal, db_id, file_id, 237); /* msg 237: Archive process unable to create archive file. */
    }

ret_val = copy_file (s_fd, d_fd, size);

ret_val |= LLIO_close (NULL_PTR, s_fd);
ret_val |= LLIO_close (NULL_PTR, d_fd);

if (ret_val)
    error_exit (status, &journal, db_id, file_id, 238); /* msg 238: Archive process unable to close log and/or archive files. */

/* Now mark the file for deletion */

JRN_archive_end (status, &journal, db_id, file_id); 

exit (FINI_OK);
}

static USHORT copy_file (
    SLONG	s_fd,
    SLONG	d_fd,
    SLONG	size)
{
/**************************************
 *
 *	c o p y _ f i l e
 *
 **************************************
 *
 * Functional description
 *	copy source file to destination file 
 *
 **************************************/
SCHAR	buff [1024];
SLONG	l, len, read, written;

len = size;

l = 1024;

while (len)
    {
    if (len < 1024)
	l = len;

    len -= l;

    if (LLIO_read (NULL_PTR, s_fd, NULL_PTR, 0L, LLIO_SEEK_NONE, buff, l, &read) ||
	l != read)
	return FAILURE;
    if (LLIO_write (NULL_PTR, d_fd, NULL_PTR, 0L, LLIO_SEEK_NONE, buff, l, &written) ||
	l != written)
	return FAILURE;
    }

return SUCCESS;
}

static void error_exit (
    STATUS	*status_vector,
    JRN		*ret_journal,
    SLONG	db_id,
    SLONG	file_id,
    SLONG	error_code)
{
/**************************************
 *
 *	e r r o r _ e x i t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

JRN_archive_error (status_vector, ret_journal, db_id, file_id, error_code);
exit (FINI_ERROR);
}

static USHORT open_file (
    TEXT	*full_name,
    SLONG	p_offset,
    USHORT	mode,
    SLONG	*fd)
{
/**************************************
 *
 *	o p e n _ f i l e
 *
 **************************************
 *
 * Functional description
 *	open a file with a given mask
 *	Return file descriptor.
 *
 **************************************/

if (LLIO_open (NULL_PTR, full_name, mode, FALSE, fd))
    return FAILURE;

if (p_offset)
    {
    if (LLIO_seek (NULL_PTR, *fd, NULL_PTR, p_offset, LLIO_SEEK_BEGIN))
	return FAILURE;
    }

return SUCCESS;
}
