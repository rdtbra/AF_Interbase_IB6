/*
 *	PROGRAM:	JRD Journal Server
 *	MODULE:		oldr.c
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
#ifdef VMS
#include <file.h>
#else
#include <fcntl.h>
#endif
#ifdef DELTA
#include <unistd.h>
#endif
#include "../jrd/old.h" 
#include "../jrd/llio.h" 
#include "../journal/misc_proto.h"
#include "../journal/oldr_proto.h"
#include "../jrd/llio_proto.h"

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH         512
#endif

static int	close_cur_file (OLD);
static int	open_next_file (OLD);
static SLONG	oldr_open_file (OLD);

int OLDR_close (
    OLD  *OLD_handle)
{
/**************************************
 *
 *	O L D R _ c l o s e 
 *
 **************************************
 *
 * Functional description
 *	Close the OLD file after at end of scan.
 *
 **************************************/
OLD	old;

old = *OLD_handle;

if (old->old_fd)
    {
    if (LLIO_close (NULL_PTR, old->old_fd))
	return FAILURE;
    }

if (old)
    {
    MISC_free_jrnl ((int*) old->old_block->ob_hdr);
    MISC_free_jrnl ((int*) old->old_block);
    MISC_free_jrnl ((int*) old);
    }

*OLD_handle = NULL;

return SUCCESS;
}

int OLDR_get (
    OLD		OLD_handle,
    SCHAR	*logrec,
    SSHORT	*len)
{
/**************************************
 *
 *	O L D R _ g e t
 *
 **************************************
 *
 * Functional description
 *	Get a record from the OLD file.
 *	return 	
 *		0	OK
 *		OLD_EOD	Done
 *		OLD_EOF	EOF
 *	        OLD_ERR	error
 *
 **************************************/
OLD_HDR	hdr;
OLDBLK	ob;
SLONG	read_len;

ob  = OLD_handle->old_block;
hdr = ob->ob_hdr;

if (LLIO_read (NULL_PTR, OLD_handle->old_fd, NULL_PTR, 0L, LLIO_SEEK_NONE,
    (SCHAR*) hdr, OLD_handle->old_rec_size, &read_len))
    return (OLD_ERR); 

if (hdr->oh_type == OLD_EOD)
    return OLD_EOD;	/* all done */

if ((hdr->oh_type != OLD_DATA) ||
    (hdr->oh_seqno != OLD_handle->old_cur_seqno)) 
    return OLD_EOF;

read_len = hdr->oh_hdr_len;
memcpy (logrec, hdr->oh_buf, read_len);

OLD_handle->old_cur_seqno++;
*len = read_len;

return 0;
}

void OLDR_get_info (
    OLD		OLD_handle,
    SSHORT	*db_page_size,
    SSHORT	*dump_id,
    SLONG	*log_seqno,
    SLONG	*log_offset,
    SLONG	*log_p_offset)
{
/**************************************
 *
 *	O L D R _ g e t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Get old information
 *
 **************************************/
if (!OLD_handle)
    return;

*db_page_size = OLD_handle->old_db_page_size;
*dump_id      = OLD_handle->old_dump_id;
*log_seqno    = OLD_handle->old_log_seqno;
*log_offset   = OLD_handle->old_log_offset;
*log_p_offset = OLD_handle->old_log_p_offset;
}

int OLDR_open (
    OLD		*OLD_handle,
    SCHAR	*dbname,
    SSHORT 	num_files,
    SCHAR	**files)
{
/**************************************
 *
 *	O L D R _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Open a OLD file for reading. Used during recovery
 *	If a non NULL OLD is passed in, start from where we
 *	left off.
 *
 **************************************/
OLD	old;

if (*OLD_handle == NULL)
    {
    *OLD_handle = old = (OLD) MISC_alloc_jrnl (sizeof (struct old));
    old->old_block = (OLDBLK) MISC_alloc_jrnl (sizeof (struct oldblk));
    old->old_block->ob_hdr = (OLD_HDR) MISC_alloc_jrnl (MAX_OLDBUFLEN);

    old->old_num_files 	= num_files;
    old->old_files 	= files;
    old->old_file_size 	= 0;
    old->old_db		= dbname;
    }
else 
    {
    old = *OLD_handle;
    }

/* Open the first file */

if (open_next_file (old) == FAILURE)
    return FAILURE;

return SUCCESS;
}

static int close_cur_file (
    OLD	old)
{
/**************************************
 *
 *	c l o s e _ c u r _ f i l e
 *
 **************************************
 *
 * Functional description
 *	returns SUCCESS - if things work
 *		FAILURE - close fails
 *
 **************************************/

if (LLIO_close (NULL_PTR, old->old_fd))
    return FAILURE;

old->old_fd       = 0;
old->old_cur_size = 0;

return SUCCESS;
}

static int open_next_file (
    OLD	old)
{
/**************************************
 *
 *	o p e n _ n e x t _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Opens the next OnLine Dump file and writes out the header.
 *
 *	returns SUCCESS - if things work
 *		FAILURE - open fails, no more file etc.
 *
 **************************************/
int	fd;

if (old->old_fd)
    {
    if (close_cur_file (old) == FAILURE)
	return FAILURE;
    }

if (old->old_cur_file >= old->old_num_files)
    return FAILURE;

if ((fd = oldr_open_file (old)) == FAILURE)
    return FAILURE;

old->old_cur_file++;
old->old_file_seqno++;

old->old_fd = fd;

return SUCCESS;
}

static SLONG oldr_open_file (
    OLD	old)
{
/**************************************
 *
 *	o l d r _ o p e n _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Open next file and read header 
 *
 *	returns SUCCESS - if things work
 *		FAILURE - open fails/out of sequence etc.
 *
 **************************************/
OLD_HDR	hdr;
OLD_HDR_PAGE hp;
OLDBLK	ob;
SLONG	fd;
SLONG	len;
SCHAR	buf [MAX_PATH_LENGTH];
struct hdr_page header;

ob  = old->old_block;
hdr = ob->ob_hdr;

if (LLIO_open (NULL_PTR, old->old_files [old->old_cur_file], LLIO_OPEN_R,
    TRUE, &fd))
    return FAILURE;

if (LLIO_read (NULL_PTR, fd, NULL_PTR, 0L, LLIO_SEEK_NONE,
    (SCHAR*) hdr, OLD_HEADER_SIZE, &len) ||
    len != OLD_HEADER_SIZE)
    return FAILURE;

hp = (OLD_HDR_PAGE) hdr->oh_buf;
memcpy ((SCHAR*)&header, (SCHAR*)hp, sizeof (struct hdr_page));

old->old_file_size = header.hp_file_size;

if (header.hp_file_seqno != old->old_cur_file)
    return FAILURE;

if (header.hp_version != OLD_VERSION)
    return FAILURE;

if (header.hp_start_block != old->old_cur_seqno)
    return FAILURE;

if (old->old_block->ob_hdr->oh_type != OLD_HEADER)
    return FAILURE;

old->old_rec_size      = header.hp_rec_size;
old->old_dump_id       = header.hp_dump_id;
old->old_log_seqno     = header.hp_log_seqno;
old->old_log_offset    = header.hp_log_offset;
old->old_log_p_offset  = header.hp_log_p_offset;
old->old_db_page_size  = header.hp_db_page_size;

len = strlen (old->old_db);
if (len != header.hp_length)
    return FAILURE;

/* compare database name */

memcpy (buf, hp->hp_db, len);
buf [len] = 0;
if (strcmp (buf, old->old_db))
    return FAILURE;

old->old_cur_size = OLD_HEADER_SIZE; 

return fd;
}
