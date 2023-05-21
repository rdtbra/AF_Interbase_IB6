/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		llio.h
 *	DESCRIPTION:	Low-level I/O routines
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
#include <errno.h>
#include <string.h>
#include "../jrd/common.h"
#include "../jrd/llio.h"
#include "../jrd/codes.h"
#include "../jrd/iberr_proto.h"
#include "../jrd/llio_proto.h"

#ifdef NETWARE_386
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <fileengd.h>
#endif

#ifdef OS2_ONLY
#include <fcntl.h>
#include <io.h>
#endif

#ifndef NETWARE_386
#ifndef OS2_ONLY
#ifndef WIN_NT
#ifndef VMS
#include <fcntl.h>
#ifndef IMP
#ifdef DELTA
#include <unistd.h>
#include <sys/types.h>
#endif
#include <sys/file.h>
#endif
#else
#include <file.h>
#endif
#endif
#endif
#endif

#if (defined IMP || defined EPSON || defined sparc)
#include <unistd.h>
#endif

#ifdef WIN_NT
#include <windows.h>
#define TEXT		SCHAR
#endif

#ifndef O_SYNC
#define O_SYNC		0
#endif

#ifndef EINTR
#define EINTR		0
#endif

#define IO_RETRY	20
#define BUFSIZE		32768

static void		io_error (STATUS *, TEXT *, TEXT *, STATUS);

#ifdef SHLIB_DEFS
#define fsync		(*_libgds_fsync)

extern int		fsync();
#endif

int LLIO_allocate_file_space (
    STATUS	*status_vector,
    TEXT	*filename,
    SLONG	size,
    UCHAR	fill_char,
    USHORT	overwrite)
{
/**************************************
 *
 *        L L I O _ a l l o c a t e _ f i l e _ s p a c e
 *
 **************************************
 *
 * Functional description
 *        Open (create if necessary) the given file and write 'size'
 *        number of bytes to it.  Each byte is initialized to
 *        the passed fill_char.  If 'overwrite' is FALSE and the file
 *        already exists, return FAILURE. 
 *
 *        This routine may be used to make sure that the file already
 *        has enough space allocated to it.
 *
 *        Other side benefit of using this routine could be that once the
 *        space is allocated, file system would not update the inode every
 *        time something is written to the file unless the file size grows
 *        beyond the given 'size'.  So 'write' performance should improve.
 *
 *        If there is any error, return FAILURE else return SUCCESS.
 *        In case of an error, status_vector would be updated.
 *
 **************************************/
SLONG	fd;
SCHAR	buffer [BUFSIZE];
SLONG	times;
SLONG	last_size, length;
USHORT	open_mode;

if (overwrite)
    open_mode = LLIO_OPEN_RW;
else
    open_mode = LLIO_OPEN_NEW_RW;

if (LLIO_open (status_vector, filename, open_mode, TRUE, &fd))
    return FAILURE;

memset (buffer, (int) fill_char, BUFSIZE);
times = size / BUFSIZE + 1;  /* For the while loop below */
last_size = size % BUFSIZE;
while (times--)
    {
    length = (times != 0) ? BUFSIZE : last_size;
    if (LLIO_write (status_vector, fd, filename, 0L, LLIO_SEEK_NONE,
        buffer, length, NULL_PTR))
        {
        LLIO_close (NULL_PTR, fd);
        return FAILURE;
        }
    }

LLIO_close (NULL_PTR, fd);

return SUCCESS;
}

#ifdef WIN_NT
#define IO_DEFINED
int LLIO_close (
    STATUS	*status_vector,
    SLONG	file_desc)
{
/**************************************
 *
 *	L L I O _ c l o s e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Close a file.
 *
 **************************************/	

return (CloseHandle ((HANDLE) file_desc) ? SUCCESS : FAILURE);
}

int LLIO_open (
    STATUS	*status_vector,
    TEXT	*filename,
    USHORT	open_mode,
    USHORT	share_flag,
    SLONG	*file_desc)
{
/**************************************
 *
 *	L L I O _ o p e n		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Open a file.
 *
 **************************************/	
DWORD	access, create, attributes;

access = GENERIC_READ | GENERIC_WRITE;
attributes = 0;
switch (open_mode)
    {
    case LLIO_OPEN_R:
	access = GENERIC_READ;
	create = OPEN_EXISTING;
	break;
    case LLIO_OPEN_RW:
	create = OPEN_ALWAYS;
	break;
    case LLIO_OPEN_WITH_SYNC_RW:
	create = OPEN_ALWAYS;
	attributes = FILE_FLAG_WRITE_THROUGH;
	break;
    case LLIO_OPEN_WITH_TRUNC_RW:
	create = TRUNCATE_EXISTING;
	break;
    case LLIO_OPEN_EXISTING_RW:
	create = OPEN_EXISTING;
	break;
    case LLIO_OPEN_WITH_SYNC_W:
	access = GENERIC_WRITE;
	create = OPEN_ALWAYS;
	attributes = FILE_FLAG_WRITE_THROUGH;
	break;
    case LLIO_OPEN_NEW_RW:
	create = CREATE_NEW; 
	break;
    }

*file_desc = CreateFile (filename,
    access,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    create,
    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | attributes,
    0);
if ((HANDLE) *file_desc == INVALID_HANDLE_VALUE &&
    open_mode == LLIO_OPEN_WITH_TRUNC_RW)
    *file_desc = CreateFile (filename,
	access,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	CREATE_NEW,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | attributes,
	0);
if ((HANDLE) *file_desc == INVALID_HANDLE_VALUE)
    {
    if (status_vector)
	io_error (status_vector, "CreateFile", filename, isc_io_open_err);
    return FAILURE;
    }

return SUCCESS;
}

int LLIO_read (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence,
    UCHAR	*buffer,
    SLONG	length,
    SLONG	*length_read)
{
/**************************************
 *
 *	L L I O _ r e a d		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Read a record from a file.
 *
 **************************************/	
DWORD	method, len;

if ((whence != LLIO_SEEK_NONE) && 
    (LLIO_seek (status_vector, file_desc, filename, offset, whence) == FAILURE))
    return FAILURE;

if (buffer)
    if (!ReadFile ((HANDLE) file_desc, buffer, length, &len, NULL))
	{
	if (status_vector)
	    io_error (status_vector, "ReadFile", filename, isc_io_read_err);
	return FAILURE;
	}

if (length_read)
    *length_read = len;

return SUCCESS;
}

int LLIO_seek (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence)
{
/**************************************
 *
 *	L L I O _ s e e k		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Seek to the given offset.
 *
 **************************************/	
DWORD	method;

if (whence != LLIO_SEEK_NONE)
    {
    switch (whence)
	{
	case LLIO_SEEK_BEGIN:
	    method = FILE_BEGIN;
	    break;
	case LLIO_SEEK_CURRENT:
	    method = FILE_CURRENT;
	    break;
	case LLIO_SEEK_END:
	    method = FILE_END;
	    break;
	}
    if (SetFilePointer ((HANDLE) file_desc, offset, NULL, method) == -1)
	{
	if (status_vector)
	    io_error (status_vector, "SetFilePointer", filename, isc_io_access_err);
	return FAILURE;
	}
    }

return SUCCESS;
}

int LLIO_sync (
    STATUS	*status_vector,
    SLONG	file_desc)
{
/**************************************
 *
 *	L L I O _ s y n c		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Flush the buffers in a file.
 *
 **************************************/	

return (FlushFileBuffers ((HANDLE) file_desc) ? SUCCESS : FAILURE);
}

int LLIO_write (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence,
    UCHAR	*buffer,
    SLONG	length,
    SLONG	*length_written)
{
/**************************************
 *
 *	L L I O _ w r i t e		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Write a record to a file.
 *
 **************************************/	
DWORD	method, len;

if ((whence != LLIO_SEEK_NONE) && 
    (LLIO_seek (status_vector, file_desc, filename, offset, whence) == FAILURE))
    return FAILURE;

if (buffer)
    if (!WriteFile ((HANDLE) file_desc, buffer, length, &len, NULL))
	{
	if (status_vector)
	    io_error (status_vector, "WriteFile", filename, isc_io_write_err);
	return FAILURE;
	}

if (length_written)
    *length_written = len;

return SUCCESS;
}

static void io_error (
    STATUS	*status_vector,
    TEXT	*op,
    TEXT	*filename,
    STATUS  operation)
{
/**************************************
 *
 *	i o _ e r r o r		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Fill up the status_vector with error info.
 *
 **************************************/	

IBERR_build_status (status_vector, isc_io_error, gds_arg_string, op,
    gds_arg_string, filename, isc_arg_gds, operation,
    gds_arg_win32, GetLastError(), 0);
}
#endif

#ifndef IO_DEFINED
int LLIO_close (
    STATUS	*status_vector,
    SLONG	file_desc)

{
/**************************************
 *
 *	L L I O _ c l o s e		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Close a file.
 *
 **************************************/	

return ((close ((int) file_desc) != -1) ? SUCCESS : FAILURE);
}

#ifdef NETWARE_386
int LLIO_open (
    STATUS	*status_vector,
    TEXT	*filename,
    USHORT	open_mode,
    USHORT	share_flag,
    SLONG	*file_desc)
{
/**************************************
 *
 *	L L I O _ o p e n		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Open a file.
 *
 **************************************/	
int	oldmask;
int	access;
int	share;
int	permission;
int	flagBits;

access = O_BINARY;
share = SH_DENYNO;
permission = 0;
flagBits = 0;

switch (open_mode)
    {
    case LLIO_OPEN_R:
	access |= O_RDONLY;
	break;
    case LLIO_OPEN_RW:
	access |= O_RDWR | O_CREAT;
	break;
    case LLIO_OPEN_WITH_SYNC_RW:
	access |= O_RDWR;
   flagBits = FILE_WRITE_THROUGH_BIT;
	break;
    case LLIO_OPEN_WITH_TRUNC_RW:
	access |= O_RDWR | O_CREAT | O_TRUNC;
	break;
    case LLIO_OPEN_EXISTING_RW:
	access |= O_RDWR;
	break;
    case LLIO_OPEN_WITH_SYNC_W:
	access |= O_WRONLY | O_CREAT;
   flagBits = FILE_WRITE_THROUGH_BIT;
	break;
    case LLIO_OPEN_NEW_RW:
	access |= O_CREAT | O_EXCL | O_RDWR;
	break;
    }

if (share_flag)
    oldmask = umask (0);
*file_desc = FEsopen (filename, access, share, permission, 
                      flagBits, PrimaryDataStream);
if (share_flag)
    umask (oldmask);
if (*file_desc == -1)
    {
    if (status_vector)
	io_error (status_vector, "FEsopen", filename, isc_io_open_err);
    return FAILURE;
    }

return SUCCESS;
}
#endif

#ifndef NETWARE_386
int LLIO_open (
    STATUS	*status_vector,
    TEXT	*filename,
    USHORT	open_mode,
    USHORT	share_flag,
    SLONG	*file_desc)
{
/**************************************
 *
 *	L L I O _ o p e n		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Open a file.
 *
 **************************************/	
int	oldmask;
int	mode;

switch (open_mode)
    {
    case LLIO_OPEN_R:
	mode = O_RDONLY;
	break;
    case LLIO_OPEN_RW:
	mode = O_RDWR | O_CREAT;
	break;
    case LLIO_OPEN_WITH_SYNC_RW:
	mode = O_RDWR | O_CREAT | O_SYNC;
	break;
    case LLIO_OPEN_WITH_TRUNC_RW:
	mode = O_RDWR | O_CREAT | O_TRUNC;
	break;
    case LLIO_OPEN_EXISTING_RW:
	mode = O_RDWR;
	break;
    case LLIO_OPEN_WITH_SYNC_W:
	mode = O_WRONLY | O_CREAT | O_SYNC;
	break;
    case LLIO_OPEN_NEW_RW:
	mode = O_CREAT | O_EXCL | O_RDWR;
	break;
    }
if (share_flag)
    oldmask = umask (0);
*file_desc = open (filename, mode, 0666);
if (share_flag)
    umask (oldmask);
if (*file_desc == -1)
    {
    if (status_vector)
	io_error (status_vector, "open", filename, isc_io_open_err);
    return FAILURE;
    }

return SUCCESS;
}
#endif

int LLIO_read (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence,
    UCHAR	*buffer,
    SLONG	length,
    SLONG	*length_read)
{
/**************************************
 *
 *	L L I O _ r e a d		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Read a record from a file.
 *
 **************************************/	
int	len, i;
UCHAR	*p;

if ((whence != LLIO_SEEK_NONE) && 
    (LLIO_seek (status_vector, file_desc, filename, offset, whence) == FAILURE))
    return FAILURE;

if (p = buffer)
    for (i = 0; length && i++ < IO_RETRY;)
	{
	if ((len = read ((int) file_desc, p, (int) length)) == -1)
	    {
	    if (SYSCALL_INTERRUPTED(errno) && i < IO_RETRY)
		continue;
	    if (status_vector)
		io_error (status_vector, "read", filename, isc_io_read_err);
	    return FAILURE;
	    }
	if (!len)
	    break;
	length -= len;
	p += len;
	}

if (length_read)
    *length_read = p - buffer;

return SUCCESS;
}

int LLIO_seek (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence)
{
/**************************************
 *
 *	L L I O _ s e e k		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Seek to the given offset.
 *
 **************************************/	

if (whence != LLIO_SEEK_NONE)
    {
    switch (whence)
	{
	case LLIO_SEEK_BEGIN:
	    whence = SEEK_SET;
	    break;
	case LLIO_SEEK_CURRENT:
	    whence = SEEK_CUR;
	    break;
	case LLIO_SEEK_END:
	    whence = SEEK_END;
	    break;
	}
    if (lseek ((int) file_desc, offset, (int) whence) == -1)
	{
	if (status_vector)
	    io_error (status_vector, "lseek", filename, isc_io_access_err);
	return FAILURE;
	}
    }
return SUCCESS;
}

int LLIO_sync (
    STATUS	*status_vector,
    SLONG	file_desc)
{
/**************************************
 *
 *	L L I O _ s y n c		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Flush the buffers in a file.
 *
 **************************************/	

#if (defined DELTA || defined NETWARE_386 || defined OS2_ONLY)
return SUCCESS;
#else
return (fsync ((int) file_desc) != -1) ? SUCCESS : FAILURE;
#endif
}

int LLIO_write (
    STATUS	*status_vector,
    SLONG	file_desc,
    TEXT	*filename,
    SLONG	offset,
    USHORT	whence,
    UCHAR	*buffer,
    SLONG	length,
    SLONG	*length_written)
{
/**************************************
 *
 *	L L I O _ w r i t e		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Write a record to a file.
 *
 **************************************/	
int	len, i;
UCHAR	*p;

if ((whence != LLIO_SEEK_NONE) && 
    (LLIO_seek (status_vector, file_desc, filename, offset, whence) == FAILURE))
    return FAILURE;

if (p = buffer)
    for (i = 0; length && i++ < IO_RETRY;)
	{
	if ((len = write ((int) file_desc, p, (int) length)) == -1)
	    {
	    if (SYSCALL_INTERRUPTED(errno) && i < IO_RETRY)
		continue;
	    if (status_vector)
		io_error (status_vector, "write", filename, isc_io_write_err);
	    return FAILURE;
	    }
	if (!len)
	    break;
	length -= len;
	p += len;
	}

if (length_written)
    *length_written = p - buffer;

return SUCCESS;
}

static void io_error (
    STATUS	*status_vector,
    TEXT	*op,
    TEXT	*filename,
    STATUS  operation)
{
/**************************************
 *
 *	i o _ e r r o r		( g e n e r i c )
 *
 **************************************
 *
 * Functional description
 *	Fill up the status_vector with error info.
 *
 **************************************/	

IBERR_build_status (status_vector, isc_io_error, gds_arg_string, op,
    gds_arg_string, filename, isc_arg_gds, operation, gds_arg_unix, errno, 0);
}
#endif
