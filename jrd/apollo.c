/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		apollo.c
 *	DESCRIPTION:	Apollo specific physical IO
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

#include <apollo/base.h>
#include <apollo/ms.h>
#include <apollo/name.h>
#include <apollo/proc2.h>
#include <apollo/ipc.h>
#include <apollo/mutex.h>
#include "/sys/ins/streams.ins.c"

/***
#define TRACE	1
***/

#ifdef TRACE
#include "../jrd/ib_stdio.h"
#endif

#include "../jrd/jrd.h"
#include "../jrd/pio.h"
#include "../jrd/ods.h"
#include "../jrd/lck.h"
#include "../jrd/cch.h"
#include "../jrd/codes.h"
#include "../lock/plserver.h"
#include "../jrd/all_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/err_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pio_proto.h"
#include "../jrd/thd_proto.h"

/*
#define PLSERVER	1
*/

static SLONG	analyze (TEXT *, USHORT);
static int	compute_size (FIL, STATUS *);
static BOOLEAN	error (TEXT *, FIL, STATUS, status_$t, STATUS *);
static BOOLEAN	extend_file (DBB, FIL, ULONG, STATUS *);
static FIL	find_file (FIL, ULONG);
static PAG	map_file (DBB, FIL, ULONG, FWIN *, STATUS *);
static void	pls_close (FIL);
static void	pls_header (FIL, SCHAR *, int);
static void	pls_open (FIL, TEXT *, USHORT);
static BOOLEAN	pls_read (FIL, BDB, STATUS *);
static BOOLEAN	pls_write (FIL, BDB, STATUS *);
static FIL	setup_file (DBB, BLK, TEXT *, USHORT);
static void	setup_trace (FIL, SSHORT);
static void	trace_event (FIL, SSHORT, SCHAR *, SSHORT);

static USHORT	PL_force;

std_$call int	*kg_$lookup();
std_$call void	name_$resolve(), file_$read_lock_entryu();

typedef SLONG	node_t;
typedef SLONG	network_t;

typedef short enum {
    file_$nr_xor_lw,
    file_$cowriters
} file_$obj_mode_t;

typedef short enum {
    file_$all,
    file_$read,
    file_$read_intend_write,
    file_$chng_read_to_write,
    file_$write,
    file_$chng_write_to_read,
    file_$chng_read_to_riw,
    file_$chng_write_to_riw,
    file_$mark_for_del
} file_$acc_mode_t;

typedef struct {
    uid_$t		ouid;
    uid_$t		puid;
    file_$obj_mode_t	omode;
    file_$acc_mode_t	amode;
    SSHORT		transid;
    node_t		node;
} file_$lock_entry_t;

#define NODE_MASK	0xFFFFF

int PIO_add_file (
    DBB		dbb,
    FIL		main_file,
    TEXT	*file_name,
    SLONG	start)
{
/**************************************
 *
 *	P I O _ a d d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Add a file to an existing database.  Return the sequence
 *	number of the new file.  If anything goes wrong, return a
 *	sequence of 0.
 *	NOTE:  This routine does not lock any mutexes on
 *	its own behalf.  It is assumed that mutexes will
 *	have been locked before entry.
 *
 **************************************/
USHORT	sequence;
FIL	file, new_file;

if (!(new_file = PIO_create (dbb, file_name, strlen (file_name), FALSE)))
    return 0;

new_file->fil_min_page = start;
sequence = 1;

for (file = main_file; file->fil_next; file = file->fil_next)
    ++sequence;

file->fil_max_page = start - 1;
file->fil_next = new_file;

return sequence;
}

void PIO_close (
    FIL		main_file)
{
/**************************************
 *
 *	P I O _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	NOTE:  This routine does not lock any mutexes on
 *	its own behalf.  It is assumed that mutexes will
 *	have been locked before entry.
 *
 **************************************/
FWIN		window;
FIL		file;
status_$t	status;
USHORT		i;

for (file = main_file; file; file = file->fil_next)
    {
#ifdef PLSERVER
    if (file->fil_connect)
	pls_close (file);
    else
#endif
	for (i = 0, window = file->fil_windows; i < MAX_WINDOWS; i++, window++)
	    if (window->fwin_mapped)
	        ms_$unmap (window->fwin_address, (ULONG) window->fwin_length, &status);
    }

#ifdef TRACE
if (file = dbb->dbb_file)
    {
    trace_event (file, trace_close, 0, 0);
    if (file->fil_trace)
        ib_fclose (file->fil_trace);
    }
#endif
}

int PIO_connection (
    TEXT	*file_name,
    USHORT	*file_length)
{
/**************************************
 *
 *	P I O _ c o n n e c t i o n
 *
 **************************************
 *
 * Functional description
 *	Analyze a file specification and determine whether a page/lock
 *	server is required and available.  If so, return a "connection"
 *	block.  If not, return NULL.
 *
 *	Note: The file name must have been expanded prior to this call.
 *
 **************************************/
int	node;

#ifdef PLSERVER

/* Analyze the file name to see if a remote connection is required.  If not,
   quietly (sic) return. */

if (node = analyze (file_name, *file_length))
    return PLS_connection (node);

#endif

return NULL;
}

FIL PIO_create (
    DBB		dbb,
    TEXT	*string,
    SSHORT	length,
    BOOLEAN	overwrite)
{
/**************************************
 *
 *	P I O _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Create a new database file.
 *	NOTE:  This routine does not lock any mutexes on
 *	its own behalf.  It is assumed that mutexes will
 *	have been locked before entry.
 *
 **************************************/
FWIN		window;
FIL		file;
SCHAR		*address, expanded_name [256];
status_$t	status;

/* Start by creating and mapping the file */

address = ms_$crmapl (string, length, (ULONG) 0, (ULONG) WINDOW_LENGTH, 
	ms_$cowriters, &status);

if ((status.all & 0xffffff) == name_$already_exists)
    {
    unlink (string);
    address = ms_$crmapl (string, length, (ULONG) 0, (ULONG) WINDOW_LENGTH, 
	ms_$cowriters, &status);
    }

if (status.all)
    ERR_post (isc_io_error, 
	gds_arg_string, "ms_$crmapl", 
        gds_arg_cstring, length, ERR_string (string, length),
        isc_arg_gds, isc_io_create_err,
    	gds_arg_domain, status.all, 0);

/* File open succeeded.  Now expand the file name. */

length = PIO_expand (string, length, expanded_name);
file = setup_file (dbb, NULL_PTR, expanded_name, length);

window = file->fil_windows;
window->fwin_length = window->fwin_mapped = WINDOW_LENGTH;
window->fwin_address = (PAG) address;

return file;
}

int PIO_expand (
    TEXT	*file_name,
    USHORT	file_length,
    TEXT	*expanded_name)
{
/**************************************
 *
 *	P I O _ e x p a n d
 *
 **************************************
 *
 * Functional description
 *	Fully expand a file name.  If the file doesn't exist, do something
 *	intelligent. THIS CODE HAS BEEN REPLICATED IN ISC.C
 *
 **************************************/
USHORT		length;
status_$t	status;

if (!file_length)
    file_length = strlen (file_name);

if (kg_$lookup ("NAME_$GET_PATH_CC                 "))
    name_$get_path_cc (file_name, file_length, expanded_name,
                       &length, &status);
else
    name_$get_path (file_name, file_length, expanded_name,
                    &length, &status);

if (!status.all && length != 0)
    {
    expanded_name [length] = 0;
    return length;
    }

/* File name lookup failed.  Copy file name and return. */

MOVE_FAST (file_name, expanded_name, file_length);
expanded_name [file_length] = 0;

return file_length;
}

void PIO_flush (void)
{
/**************************************
 *
 *	P I O _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush data out to disk, if possible.
 *
 **************************************/
FIL		file;
status_$t	status;

/****
file = dbb->dbb_file;

ms_$fw_file (file->fil_windows [0].fwin_address, &status);
if (status.all)
    error ("ms_$fw_file", file, isc_io_write_err, status, NULL_PTR);
****/
}

void PIO_force_write (
    FIL		file,
    USHORT	flag)
{
/**************************************
 *
 *	P I O _ f o r c e _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Set (or clear) force write, if possible, for the database.
 *
 **************************************/
}

void PIO_header (
    DBB		dbb,
    SCHAR	*header,
    int		length)
{
/**************************************
 *
 *	P I O _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Read the page header.  This assumes that the file has not been
 *	repositioned since the file was originally mapped.
 *
 **************************************/
FIL	file;
FWIN	window;
PAG	address;

file = find_file (dbb->dbb_file, 0);

#ifdef PLSERVER
if (file->fil_connect)
    pls_header (file, header, length);
#endif

address = map_file (dbb, file, 0, &window, NULL_PTR);

#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    (*dbb->dbb_decrypt) (dbb->dbb_encrypt_key->str_data,
			 address, length, header);
else
#endif
    MOVE_FASTER (address, header, length);

--window->fwin_use_count;
pfm_$enable();

#ifdef TRACE
trace_event (file, trace_page_size, &header->hdr_page_size, sizeof (header->hdr_page_size));
#endif
}

SLONG PIO_max_alloc (
    DBB		dbb)
{
/**************************************
 *
 *	P I O _ m a x _ a l l o c
 *
 **************************************
 *
 * Functional description
 *	Compute last physically allocated page of database.
 *
 **************************************/
FIL		file;

for (file = dbb->dbb_file; file->fil_next; file = file->fil_next)
    ;

return file->fil_min_page - file->fil_fudge +
       (compute_size (file) + dbb->dbb_page_size - 1) / dbb->dbb_page_size;
}

FIL PIO_open (
    DBB		dbb,
    TEXT	*string,
    SSHORT	length,
    SSHORT	trace_flag,
    BLK		connection,
    TEXT	*file_name,
    SSHORT	file_length)
{
/**************************************
 *
 *	P I O _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Open a database file.  If a "connection" block is provided, use
 *	the connection to communication with a page/lock server.
 *
 **************************************/
FWIN		window;
FIL		file;
PAG		page;
UCHAR		temp [256];
status_$t	status;

file = setup_file (dbb, connection, string, length);

#ifdef PLSERVER
if (connection)
    pls_open (file, file_name, file_length);
    return file;
#endif

window = file->fil_windows;
(SCHAR*) window->fwin_address = ms_$mapl (file_name, file_length, 
	(ULONG) window->fwin_offset, (ULONG) WINDOW_LENGTH, 
	ms_$cowriters, ms_$wr, (SSHORT) -1,
	&window->fwin_length, &status);

window->fwin_mapped = window->fwin_length;

if (status.all)
    ERR_post (isc_io_error, 
	gds_arg_string, "ms_$mapl", 
        gds_arg_cstring, file_length, ERR_string (file_name, file_length),
        isc_arg_gds, isc_io_open_err,
    	gds_arg_domain, status.all, 0);

#ifdef TRACE
if (trace_flag)
    setup_trace (file, trace_open);
#endif

return file;
}

PIO_read (
    FIL		file,
    BDB		bdb,
    PAG		page,
    STATUS	*status_vector)
{
/**************************************
 *
 *	P I O _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a data page.  Oh wow.
 *
 **************************************/
DBB	dbb;
PAG	address;
FWIN	window;

dbb = bdb->bdb_dbb;
file = find_file (file, bdb->bdb_page);

#ifdef PLSERVER
if (file->fil_connect)
    return pls_read (file, bdb, status_vector);
#endif

if (!(address = map_file (dbb, file, bdb->bdb_page, &window, status_vector)))
    return FALSE;

#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    (*dbb->dbb_decrypt) (dbb->dbb_encrypt_key->str_data,
			 address, dbb->dbb_page_size, page);
else
#endif
    MOVE_FASTER (address, page, dbb->dbb_page_size);

--window->fwin_use_count;
pfm_$enable();

#ifdef TRACE
trace_event (file, trace_read, &bdb->bdb_page, sizeof (bdb->bdb_page));
#endif
return TRUE;
}

PIO_write (
    FIL		file,
    BDB		bdb,
    PAG		page,
    STATUS	*status_vector)
{
/**************************************
 *
 *	P I O _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Write a data page.  Oh wow.
 *
 **************************************/
DBB		dbb;
PAG		address;
SLONG		length, size;
ULONG		number, fudge, page_end;
FWIN		window;
status_$t	status;

dbb = bdb->bdb_dbb;
length = dbb->dbb_page_size;
number = fudge = bdb->bdb_page;
file = find_file (file, number);

#ifdef PLSERVER
if (file->fil_connect)
    return pls_write (file, bdb, status_vector);
#endif

fudge -= file->fil_min_page - file->fil_fudge;
page_end = (fudge + 1) * length;

if (file->fil_size < page_end)
    {
    if (status_vector)
        *status_vector = 0;
    size = compute_size (file, status_vector);
    if (!size && status_vector && *status_vector)
        return FALSE;
    if (size < page_end)
	if (!extend_file (dbb, file, page_end, status_vector))
	    return FALSE;
    }

if (!(address = map_file (dbb, file, number, &window, status_vector)))
    return FALSE;

#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    (*dbb->dbb_encrypt) (dbb->dbb_encrypt_key->str_data,
			 page, length, address);
else
#endif
    MOVE_FASTER (page, address, length);

--window->fwin_use_count;
pfm_$enable();

if (dbb->dbb_flags & DBB_force_write)
    {
    ms_$fw_partial (address, length, &status);
    if (status.all)
	return error ("ms_$fw_partial", file, isc_io_write_err, status,
            status_vector);
    }

#ifdef TRACE
trace_event (file, trace_write, &bdb->bdb_page, sizeof (bdb->bdb_page));
#endif
return TRUE;
}

#ifdef PLSERVER
static SLONG analyze (
    TEXT	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Sniff out a file and determine whether it can be successfully
 *	opened locally.  If so, return 0.  Otherwise, generate a 
 *	mailbox string to establish a connection to a server that 
 *	can open the file.  Return the length of the mailbox string.
 *
 **************************************/
uid_$t		uid, process_uid;
SLONG		node, target;
status_$t	status;
file_$lock_entry_t	entry;

/* If the force flag is set, connect to ourselves for debugging */

if (PL_force)
    {
    proc2_$who_am_i (&uid);
    return uid.low & NODE_MASK;
    }

/* Resolve the name to get the UID.  Again, if any error, just give up. */

name_$resolve (*file_name, file_length, uid, status);
if (status.all)
    return 0;

/* Attempt to read the file lock entry for the file.  If it isn't locked, it
   can be accessed locally. */

file_$read_lock_entryu (uid, entry, status);
if (status.all)
    return 0;

/* File is apparently locked.  If it's locked by our node, everything is
   fine.  */

proc2_$who_am_i (&process_uid);
node = process_uid.low & NODE_MASK;
target = entry.puid.low & NODE_MASK;

if (node == target)
    return 0;

return target;
}
#endif

static int compute_size (
    FIL		file,
    STATUS	*status_vector)
{
/**************************************
 *
 *	c o m p u t e _ s i z e
 *
 **************************************
 *
 * Functional description
 *	Find the file size (bytes).
 *
 **************************************/
ms_$attrib_t	attributes;
status_$t	status;
SSHORT		length;

ms_$attributes (file->fil_windows [0].fwin_address, &attributes, 
	    &length, sizeof (attributes), &status);

if (status.all)
    return error ("ms_$attributes", file, isc_io_write_err, status,
            status_vector);

return (file->fil_size = attributes.cur_len);
}

static BOOLEAN error (
    TEXT	*string,
    FIL		file,
    STATUS  operation,
    status_$t	status,
    STATUS	*status_vector)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	Somebody has noticed a file system error and expects error
 *	to do something about it.  Harumph!
 *
 **************************************/

if (status_vector)
    {
    *status_vector++ = isc_arg_gds;
    *status_vector++ = isc_io_error;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) ERR_cstring (string);
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) ERR_string (file->fil_string, file->fil_length);
    *status_vector++ = isc_arg_gds;
    *status_vector++ = operation;
    *status_vector++ = gds_arg_domain;
    *status_vector++ = status.all;
    *status_vector++ = gds_arg_end;
    return FALSE;
    }
else
    ERR_post (isc_io_error, 
	gds_arg_string, ERR_cstring (string), 
        gds_arg_string, ERR_string (file->fil_string, file->fil_length),
        isc_arg_gds, operation,
    	gds_arg_domain, status.all, 0);
}

static BOOLEAN extend_file (
    DBB		dbb,
    FIL		file,
    ULONG	size,
    STATUS	*status_vector)
{
/**************************************
 *
 *	e x t e n d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Extend the length of a file.
 *
 **************************************/
ms_$attrib_t	attributes;
status_$t	status;
SSHORT		length;
struct lck	temp_lock;

/* fill out a lock block, zeroing it out first */

MOVE_CLEAR (&temp_lock, sizeof (struct lck));
temp_lock.lck_header.blk_type = type_lck;
temp_lock.lck_type = LCK_file_extend;
temp_lock.lck_owner_handle = LCK_get_owner_handle (NULL_TDBB, temp_lock.lck_type);
temp_lock.lck_dbb = dbb;
temp_lock.lck_parent = dbb->dbb_lock;
temp_lock.lck_length = sizeof (SLONG);
temp_lock.lck_key.lck_long = file->fil_sequence;

if (LCK_lock_opt (NULL_TDBB, &temp_lock, LCK_EX, LCK_WAIT))
    {
    ms_$attributes (file->fil_windows [0].fwin_address, &attributes, 
		    &length, sizeof (attributes), &status);
    if (status.all)
	{
	LCK_release (NULL_TDBB, &temp_lock);
    	return error ("ms_$attributes", file, isc_io_access_err, status,
                status_vector);
	}

    if ((file->fil_size = attributes.cur_len) >= size)
	{
	LCK_release (NULL_TDBB, &temp_lock);
    	return TRUE;
	}

    size += EXTEND_SIZE;

    ms_$truncate (file->fil_windows [0].fwin_address, size, &status);
    if (status.all)
	{
	LCK_release (NULL_TDBB, &temp_lock);
    	return error ("ms_$truncate", file, isc_io_access_err, status,
                status_vector);
	}

    file->fil_size = size;
    LCK_release (NULL_TDBB, &temp_lock);
    }

return TRUE;
}

static FIL find_file (
    FIL		file,
    ULONG	page)
{
/**************************************
 *
 *	f i n d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Find the file block for a particular page.
 *
 **************************************/

for (; file; file = file->fil_next)
    if (page >= file->fil_min_page && page <= file->fil_max_page)
	return file;

CORRUPT (149); /* msg 149 cannot map page */
}

static PAG map_file (
    DBB		dbb,
    FIL		file,
    ULONG	page,
    FWIN	*pwindow,
    STATUS	*status_vector)
{
/**************************************
 *
 *	m a p _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Map a page in a file.
 *
 **************************************/
FWIN		window, win, end;
ULONG		offset, section;
status_$t	status;
SCHAR		*address;
TEXT		*op;

THD_MUTEX_LOCK (file->fil_mutex);
page -= file->fil_min_page - file->fil_fudge;
offset = page * dbb->dbb_page_size;
win = NULL;

/* Search for a window that is already mapped. */

do {
    for (window = file->fil_windows, end = window + MAX_WINDOWS;
	window < end; window++)
    	{
    	if (window->fwin_length &&
	    offset >= window->fwin_offset && 
	    offset < window->fwin_offset + window->fwin_length)
	    {
	    ++window->fwin_use_count;
	    THD_MUTEX_UNLOCK (file->fil_mutex);
	    if (!(window->fwin_flags & FWIN_in_progress))
		goto exit;
	    THD_MUTEX_LOCK (window->fwin_mutex);
	    if (window->fwin_flags & FWIN_in_progress)
		goto mapit;
	    else
		{
		THD_MUTEX_UNLOCK (window->fwin_mutex);
		goto exit;
		}
	    }
        if (!window->fwin_use_count &&
	    (!win || window->fwin_activity < win->fwin_activity))
	    win = window;
	}
} while (!win);

window = win;
++window->fwin_use_count;
THD_MUTEX_LOCK (window->fwin_mutex);

mapit:

section = offset & ~(WINDOW_LENGTH - 1);
window->fwin_offset = section;
window->fwin_length = WINDOW_LENGTH;
window->fwin_flags |= FWIN_in_progress;
THD_MUTEX_UNLOCK (file->fil_mutex);

/* If not all windows have been used, use another */

if (!window->fwin_mapped)
    {
    op = "ms_$mapl";
    address = ms_$mapl (file->fil_string, (SSHORT) file->fil_length, 
	    section, (SLONG) WINDOW_LENGTH, 
            ms_$cowriters, ms_$wr, (SSHORT) -1,
	    &window->fwin_length, &status);
    }
else
    {

    /* Window not found, remap it. */

    op = "ms_$remap";      
    address = ms_$remap (window->fwin_address, section, 
	    (SLONG) WINDOW_LENGTH, &window->fwin_length, &status);
    }

window->fwin_mapped = window->fwin_length;

if (status.all)
    {
    --window->fwin_use_count;
    THD_MUTEX_UNLOCK (window->fwin_mutex);
    return (PAG) error (op, file, isc_io_access_err, status, status_vector);
    }

/* Down grade activity after window shift */

for (win = file->fil_windows; win < end; win++)
    win->fwin_activity >>= 2;

window->fwin_flags &= ~FWIN_in_progress;
window->fwin_address = (PAG) address;
window->fwin_offset = section;
window->fwin_activity = 0;
THD_MUTEX_UNLOCK (window->fwin_mutex);

exit:

pfm_$inhibit();
++window->fwin_activity;
*pwindow = window;
return (PAG) ((SCHAR*) window->fwin_address + offset - window->fwin_offset);
}

#ifdef PLSERVER
static void pls_close (
    FIL		file)
{
/**************************************
 *
 *	p l s _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Close a connection;
 *
 **************************************/
PHD		packet, reply;
status_$t	status;

packet.phd_type = PL_DISCONNECT;
packet.phd_length = 0;

status.all = PLS_request (file->fil_connect,
	     &packet, sizeof (packet), 
	     &reply, sizeof (reply),
	     NULL, 0, 0);

if (reply.phd_type == PL_ERROR)
    error (reply.phd_string, file, isc_io_close_err, reply.phd_misc, NULL_PTR);

if (reply.phd_type != PL_REPLY)
    BUGCHECK (148); /* msg 148 wrong packet type */
}

static void pls_header (
    FIL		file,
    SCHAR	*address,
    int		length)
{
/**************************************
 *
 *	p l s _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Read a page from the remote system.
 *
 **************************************/
PHD		packet, reply;
status_$t	status;

packet.phd_type = PL_PAGE_READ;
packet.phd_handle = file->fil_desc;
packet.phd_length = length;
packet.phd_misc = 0;

status.all = PLS_request (file->fil_connect,
	     &packet, sizeof (packet), 
	     &reply, sizeof (reply),
	     address, 0, length);

if (reply.phd_type == PL_ERROR)
    error (reply.phd_string, file, isc_io_read_err, reply.phd_misc, NULL_PTR);

if (reply.phd_type != PL_REPLY)
    BUGCHECK (148); /* msg 148 wrong packet type */
}

static void pls_open (
    FIL		file,
    TEXT	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	p l s _ o p e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
PHD		packet, reply;
status_$t	status;

packet.phd_type = PL_PAGE_OPEN;
packet.phd_length = file->fil_length;

status.all = PLS_request (file->fil_connect,
	     &packet, sizeof (packet), 
	     &reply, sizeof (reply),
	     file->fil_string, file->fil_length, 0);

if (reply.phd_type == PL_ERROR)
    error (reply.phd_string, file, isc_io_open_err, reply.phd_misc, NULL_PTR);

if (reply.phd_type != PL_REPLY)
    BUGCHECK (148); /* msg 148 wrong packet type */

file->fil_desc = reply.phd_handle;
}

static BOOLEAN pls_read (
    FIL		file,
    BDB		bdb,
    STATUS	*status_vector)
{
/**************************************
 *
 *	p l s _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a page from the remote system.
 *
 **************************************/
DBB		dbb;
PHD		packet, reply;
status_$t	status;

dbb = bdb->bdb_dbb;
packet.phd_type = PL_PAGE_READ;
packet.phd_handle = file->fil_desc;
packet.phd_length = dbb->dbb_page_size;
packet.phd_misc = 
    (bdb->bdb_page  - file->fil_min_page + file->fil_fudge) * dbb->dbb_page_size;

status.all = PLS_request (file->fil_connect,
	     &packet, sizeof (packet), 
	     &reply, sizeof (reply),
	     bdb->bdb_buffer, 0, dbb->dbb_page_size);

if (reply.phd_type == PL_ERROR)
    return error (reply.phd_string, file, isc_io_read_errr, reply.phd_misc,
            status_vector);

if (reply.phd_type != PL_REPLY)
    BUGCHECK (148); /* msg 148 wrong packet type */

return TRUE;
}

static BOOLEAN pls_write (
    FIL		file,
    BDB		bdb,
    STATUS	*status_vector)
{
/**************************************
 *
 *	p l s _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Read a page from the remote system.
 *
 **************************************/
PHD		packet, reply;
status_$t	status;
DBB		dbb;

dbb = bdb->bdb_dbb;
packet.phd_type = PL_PAGE_WRITE;
packet.phd_handle = file->fil_desc;
packet.phd_length = dbb->dbb_page_size;
packet.phd_misc = 
    (bdb->bdb_page - file->fil_min_page + file->fil_fudge) * dbb->dbb_page_size;

status.all = PLS_request (file->fil_connect,
	     &packet, sizeof (packet), 
	     &reply, sizeof (reply),
	     bdb->bdb_buffer, dbb->dbb_page_size), 0;

if (reply.phd_type == PL_ERROR)
    return error (reply.phd_string, file, isc_io_write_err, reply.phd_misc,
            status_vector);

if (reply.phd_type != PL_REPLY)
    BUGCHECK (148); /* msg 148 wrong packet type */

return TRUE;
}
#endif

static FIL setup_file (
    DBB		dbb,
    BLK		connection,
    TEXT	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	s e t u p _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Set up file and lock blocks for a file.
 *
 **************************************/
FIL		file;
FWIN		window;
LCK		lock;
SSHORT		i;
status_$t	status;

file = (FIL) ALLOCPV (type_fil, file_length+1);
file->fil_length = file_length;
file->fil_max_page = -1;
MOVE_FAST (file_name, file->fil_string, file_length);
file->fil_string [file_length] = 0;
file->fil_connect = (struct plc*) connection;
THD_MUTEX_INIT (file->fil_mutex);

window = file->fil_windows;
for (i = 0; i < MAX_WINDOWS; i++, window++)
    THD_MUTEX_INIT (window->fwin_mutex);

/* If this isn't the primary file, we're done */

if (dbb->dbb_file)
    return file;

dbb->dbb_lock = lock = (LCK) ALLOCPV (type_lck, file_length);
lock->lck_type = LCK_database;
lock->lck_owner_handle = LCK_get_owner_handle (NULL_TDBB, lock->lck_type);
lock->lck_object = (BLK) dbb;
lock->lck_length = file_length;
lock->lck_ast = CCH_down_grade_dbb;
lock->lck_dbb = dbb;
MOVE_FAST (file_name, lock->lck_string, file_length);

dbb->dbb_flags |= DBB_exclusive;
if (!LCK_lock (NULL_TDBB, lock, LCK_EX, LCK_NO_WAIT, connection))
    {
    dbb->dbb_flags &= ~DBB_exclusive;
    LCK_lock (NULL_TDBB, lock,
	      (dbb->dbb_flags & DBB_cache_manager) ? LCK_SR : LCK_SW,
	      LCK_WAIT,
	      connection);
    }

return file;
}

#ifdef TRACE
static void setup_trace (
    FIL		file,
    SSHORT	event)
{
/**************************************
 *
 *	s e t u p _ t r a c e
 *
 **************************************
 *
 * Functional description
 *	Perform setup to create trace file.
 *
 **************************************/

(IB_FILE*) file->fil_trace = ib_fopen ("trace.log", "w");
trace_event (file, event, file->fil_string, file->fil_length);
}

static void trace_event (
    FIL		file,
    SSHORT	type,
    SCHAR	*ptr,
    SSHORT	length)
{
/**************************************
 *
 *	t r a c e _ e v e n t
 *
 **************************************
 *
 * Functional description
 *	Write trace event to the trace file.
 *
 **************************************/
IB_FILE	*trace;

if (!(trace = (IB_FILE*) file->fil_trace))
    return;

ib_putc (type, trace);
ib_putc (length, trace);

while (--length >= 0)
    ib_putc (*ptr++, trace);
}
#endif
