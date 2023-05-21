/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		mpexl.c
 *	DESCRIPTION:	MPE/XL specific physical IO
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

#include <mpe.h>
#include "../jrd/jrd.h"
#include "../jrd/pio.h"
#include "../jrd/cch.h"
#include "../jrd/ods.h"
#include "../jrd/lck.h"
#include "../jrd/codes.h"
#include "../jrd/mpexl.h"
#include "../jrd/all_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/err_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/pio_proto.h"
#include "../jrd/thd_proto.h"

static SLONG	open_file (TEXT *, USHORT, SSHORT);
static FIL	seek_file (FIL, BDB, SLONG *);
static FIL	setup_file (DBB, TEXT *, USHORT, SLONG);
static BOOLEAN	mpexl_error (TEXT *, FIL, STATUS, STATUS *);

#define RECORD_LENGTH	1024
#define SYNC		(1 << 1)

#ifdef PRIVMODE_CODE
#define GET_PRIVMODE	if (privmode_flag < 0) privmode_flag = ISC_check_privmode();\
			if (privmode_flag > 0) GETPRIVMODE();
#define GET_USERMODE	if (privmode_flag > 1) GETUSERMODE();

static SSHORT	privmode_flag = -1;
#else
#define GET_PRIVMODE
#define GET_USERMODE
#endif

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
FIL		file;
USHORT		i;

for (file = main_file; file; file = file->fil_next)
    if (file->fil_desc)
	{
	GET_PRIVMODE
	FCLOSE ((SSHORT) file->fil_desc, 0, 0);
	GET_USERMODE
	V4_MUTEX_DESTROY (file->fil_mutex);
	}
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
TEXT	expanded_name [64];
SLONG	desc;

desc = open_file (string, length, DOMAIN_CREATE_PERM);

length = PIO_expand (string, length, expanded_name);
return setup_file (dbb, expanded_name, length, desc);
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
 *	intelligent.
 *
 **************************************/
SLONG	filenum, status, domain, filecode;
TEXT	temp [128], *p, *q, *end;

if (!file_length)
    file_length = strlen (file_name);

temp [0] = ' ';
MOVE_FAST (file_name, &temp [1], file_length);
temp [file_length + 1] = ' ';

GET_PRIVMODE
domain = DOMAIN_OPEN;
filecode = FILECODE_DB;
HPFOPEN (&filenum, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_FILECODE, &filecode,
    ITM_END);
if (status)
    {
    GET_USERMODE
    domain = DOMAIN_CREATE;
    HPFOPEN (&filenum, &status,
	OPN_FORMAL, temp,
	OPN_DOMAIN, &domain,
	ITM_END);
    }

if (!status)
    {
    FFILEINFO ((SSHORT) filenum,
	INF_FID, expanded_name,
	ITM_END);
    status = (ccode() == CCE) ? 0 : 1;
    FCLOSE ((SSHORT) filenum, 0, 0);
    GET_USERMODE
    }

if (status)
    {
    strncpy (expanded_name, file_name, file_length);
    expanded_name [file_length] = 0;
    return file_length;
    }

for (p = expanded_name, end = p + 28; p < end && *p && *p != ' '; p++)
    ;
*p = 0;

return p - expanded_name;
}

void PIO_flush (
    FIL		file)
{
/**************************************
 *
 *	P I O _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush the operating system cache back to good, solid oxide.
 *
 **************************************/
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
USHORT	control;

control = (flag) ? SYNC : 0;

GET_PRIVMODE
FSETMODE ((SSHORT) file->fil_desc, control);
if (ccode() != CCE)
    mpexl_error ("FSETMODE", file, isc_io_access_err, NULL_PTR);
GET_USERMODE
}

void PIO_header (
    DBB		dbb,
    SCHAR	*address,
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

file = dbb->dbb_file;

GET_PRIVMODE

#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    {
    SLONG	spare_buffer [MAX_PAGE_SIZE / sizeof (SLONG)];

    FREADDIR ((SSHORT) file->fil_desc, spare_buffer, (SSHORT) -length, 0L);
    if (ccode() != CCE)
    	mpexl_error ("FREADDIR", file, isc_io_read_err, NULL_PTR);

    (*dbb->dbb_decrypt) (dbb->dbb_encrypt_key->str_data,
			 spare_buffer, length, address);
    }
else
#endif
    {
    FREADDIR ((SSHORT) file->fil_desc, address, (SSHORT) -length, 0L);
    if (ccode() != CCE)
    	mpexl_error ("FREADDIR", file, isc_io_read_err, NULL_PTR);
    }

GET_USERMODE
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
FIL	file;
SLONG	numrecs, lrecl;

for (file = dbb->dbb_file; file->fil_next; file = file->fil_next)
    ;

GET_PRIVMODE
FFILEINFO ((SSHORT) file->fil_desc,
    INF_NUMRECS, &numrecs,
    INF_LRECL, &lrecl,
    ITM_END);
if (ccode() != CCE)
    mpexl_error ("FFILEINFO", file, isc_io_access_err, NULL_PTR);
GET_USERMODE

return file->fil_min_page - file->fil_fudge +
    numrecs / ((dbb->dbb_page_size - 1) / lrecl + 1);
}

FIL PIO_open (
    DBB		dbb,
    TEXT	*string,
    SSHORT	length,
    SSHORT	trace_flag,
    BLK		connection,
    TEXT	*file_name,
    USHORT	file_length)
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
TEXT	expanded_name [64];
SLONG	desc;

desc = open_file (string, length, DOMAIN_OPEN);

length = PIO_expand (string, length, expanded_name);
return setup_file (dbb, expanded_name, length, desc);
}

int PIO_read (
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
SLONG	block;
DBB	dbb;

dbb = bdb->bdb_dbb;
file = seek_file (file, bdb, &block);

GET_PRIVMODE
#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    {
    SLONG	spare_buffer [MAX_PAGE_SIZE / sizeof (SLONG)];

    FREADDIR ((SSHORT) file->fil_desc, spare_buffer, (SSHORT) -dbb->dbb_page_size, block);
    if (ccode() != CCE)
    	return mpexl_error ("FREADDIR", file, isc_io_read_err, status_vector);

    GET_USERMODE
    (*dbb->dbb_decrypt) (dbb->dbb_encrypt_key->str_data,
		 	 spare_buffer, dbb->dbb_page_size, page);
    }
else
#endif
    {
    FREADDIR ((SSHORT) file->fil_desc, page, (SSHORT) -dbb->dbb_page_size, block);
    if (ccode() != CCE)
    	return mpexl_error ("FREADDIR", file, isc_io_read_err, status_vector);
    GET_USERMODE
    }

return TRUE;
}

int PIO_unlink (
    TEXT	*file_name)
{
/**************************************
 *
 *	P I O _ u n l i n k
 *
 **************************************
 *
 * Functional description
 *	Delete a database file.
 *
 **************************************/
TEXT	temp [128], *p;
SLONG	filenum, status, domain, disposition, filecode;

temp [0] = ' ';
for (p = &temp [1]; *p = *file_name++; p++)
    ;
*p = ' ';

GET_PRIVMODE
domain = DOMAIN_OPEN_PERM;
filecode = FILECODE_DB;
disposition = DISP_DELETE;
HPFOPEN (&filenum, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_FILECODE, &filecode,
    OPN_DISP, &disposition,
    ITM_END);
if (!status)
    FCLOSE ((SSHORT) filenum, 0, 0);
GET_USERMODE

return status ? -1 : 0;
}

int PIO_write (
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
 *	Read a data page.  Oh wow.
 *
 **************************************/
SLONG	block;
DBB	dbb;

dbb = bdb->bdb_dbb;
file = seek_file (file, bdb, &block);

#ifdef ISC_DATABASE_ENCRYPTION
if (dbb->dbb_encrypt_key)
    {
    SLONG	spare_buffer [MAX_PAGE_SIZE / sizeof (SLONG)];

    (*dbb->dbb_encrypt) (dbb->dbb_encrypt_key->str_data,
			 page, dbb->dbb_page_size, spare_buffer);
    GET_PRIVMODE
    FWRITEDIR ((SSHORT) file->fil_desc, spare_buffer, (SSHORT) -dbb->dbb_page_size, block);
    }
else
#endif
    {
    GET_PRIVMODE
    FWRITEDIR ((SSHORT) file->fil_desc, page, (SSHORT) -dbb->dbb_page_size, block);
    }

if (ccode() != CCE)
    return mpexl_error ("FWRITEDIR", file, isc_io_write_err, status_vector);
GET_USERMODE
return TRUE;
}

static SLONG open_file (
    TEXT	*string,
    USHORT	length,
    SSHORT	mode)
{
/**************************************
 *
 *	o p e n _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
TEXT	temp [256], *p;
SLONG	filenum, status, domain, recfm, access, exclusive, multi,
	lrecl, random, buffering, disposition, binary, filecode, privlev;
USHORT	aoptions;

if (!length)
    length = strlen (string);

temp [0] = ' ';
MOVE_FAST (string, &temp [1], length);
temp [length + 1] = ' ';

/* If we're doing a create, first delete any existing file */

GET_PRIVMODE
if (mode == DOMAIN_CREATE_PERM)
    {
    domain = DOMAIN_OPEN_PERM;
    filecode = FILECODE_DB;
    disposition = DISP_DELETE;
    HPFOPEN (&filenum, &status,
	OPN_FORMAL, temp,
	OPN_DOMAIN, &domain,
	OPN_FILECODE, &filecode,
	OPN_DISP, &disposition,
	ITM_END);
    if (!status)
    	FCLOSE ((SSHORT) filenum, 0, 0);
    }

domain = mode;
recfm = RECFM_F;
access = ACCESS_RW;
exclusive = EXCLUSIVE_SHARE;
multi = MULTIREC_YES;
lrecl = RECORD_LENGTH;
filecode = FILECODE_DB;
privlev = PRIVLEV_DB;
random = WILL_ACC_RANDOM;
buffering = INHIBIT_BUF_YES;
disposition = DISP_KEEP;
binary = FORMAT_BINARY;

HPFOPEN (&filenum, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_RECFM, &recfm,
    OPN_ACCESS_TYPE, &access,
    OPN_EXCLUSIVE, &exclusive,
    OPN_MULTIREC, &multi,
    OPN_LRECL, &lrecl,
    OPN_FILECODE, &filecode,
    OPN_PRIVLEV, &privlev,
    OPN_WILL_ACCESS, &random,
    OPN_INHIBIT_BUF, &buffering,
    OPN_DISP, &disposition,
    OPN_FORMAT, &binary,
    ITM_END);

p = "HPFOPEN";
if (!status)
    {
    /* This crazy system may not open the file with the type of
       access we requested.  Here we check to see if it did. */

    FFILEINFO ((SSHORT) filenum,
	INF_AOPTIONS, &aoptions,
	ITM_END);
    if (ccode() == CCE)
	{
	if ((aoptions &15) != ACCESS_RW)
	    status = ((SLONG) -248 << 16) | 143;
	}
    else
	{
	FCHECK ((SSHORT) filenum, (USHORT*) &status, NULL, NULL, NULL);
	p = "FFILEINFO";
	}

    if (status)
	FCLOSE ((SSHORT) filenum, 0, 0);
    }
GET_USERMODE

if (status)
    ERR_post (isc_io_error, 
	gds_arg_string, p,
        gds_arg_cstring, length, ERR_string (string, length),
        isc_arg_gds, isc_io_open_err,
    	gds_arg_mpexl, status, 0);

return filenum;
}

static FIL seek_file (
    FIL		file,
    BDB		bdb,
    SLONG	*block)
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
DBB	dbb;

dbb = bdb->bdb_dbb;
page = bdb->bdb_page;

for (;; file = file->fil_next)
    if (!file)
	CORRUPT (158); /* msg 158 cannot sort on a field that does not exist */
    else if (page >= file->fil_min_page && page <= file->fil_max_page)
	break;

page -= file->fil_min_page - file->fil_fudge;
*block = page * ((dbb->dbb_page_size - 1) / file->fil_lrecl + 1);

return file;
}

static FIL setup_file (
    DBB		dbb,
    TEXT	*file_name,
    USHORT	file_length,
    SLONG	fnum)
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
FIL	file;
LCK	lock;
TEXT	*p, *q, lock_string [20];
USHORT	l;
SLONG	lrecl, status;
SSHORT	errcode;

/* Allocate file block and copy file name string */

file = (FIL) ALLOCPV (type_fil, file_length+1);
file->fil_desc = fnum;
file->fil_length = file_length;
file->fil_max_page = -1;
MOVE_FAST (file_name, file->fil_string, file_length);
file->fil_string [file_length] = '\0';
V4_MUTEX_INIT (file->fil_mutex);

GET_PRIVMODE
FFILEINFO ((SSHORT) file->fil_desc,
    INF_LRECL, &lrecl,
    INF_UFID, lock_string,
    ITM_END);
if (ccode() != CCE)
    {
    FCHECK ((SSHORT) file->fil_desc, &errcode, NULL, NULL, NULL);
    GET_USERMODE
    status = (SLONG) errcode << 16;
    ERR_post (isc_io_error, 
	gds_arg_string, "FFILEINFO",
        gds_arg_cstring, file->fil_length, ERR_string (file->fil_string, file->fil_length),
        isc_arg_gds, isc_io_access_err,
    	gds_arg_mpexl, status, 0);
    }
GET_USERMODE

file->fil_lrecl = lrecl;

/* If this isn't the primary file, we're done */

if (dbb->dbb_file)
    return file;

/* Build unique lock string for file and construct lock block */

l = sizeof (lock_string);

dbb->dbb_lock = lock = (LCK) ALLOCPV (type_lck, l);
lock->lck_type = LCK_database;
lock->lck_owner_handle = LCK_get_owner_handle (NULL_TDBB, lock->lck_type);
lock->lck_object = (BLK) dbb;
lock->lck_length = l;
lock->lck_ast = CCH_down_grade_dbb;
lock->lck_dbb = dbb;
MOVE_FAST (lock_string, lock->lck_key.lck_string, l);

/* Try to get an exclusive lock on database.  If this fails, insist
   on at least a shared lock */

dbb->dbb_flags |= DBB_exclusive;
if (!LCK_lock (NULL_TDBB, lock, LCK_EX, LCK_NO_WAIT))
    {
    dbb->dbb_flags &= ~DBB_exclusive;
    LCK_lock (NULL_TDBB, lock,
	      (dbb->dbb_flags & DBB_cache_manager) ? LCK_SR : LCK_SW,
	      LCK_WAIT);
    }

return file;
}

static BOOLEAN mpexl_error (
    SCHAR	*string,
    FIL		file,
    STATUS  operation,
    STATUS	*status_vector)
{
/**************************************
 *
 *	m p e x l _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Somebody has noticed a file system error and expects error
 *	to do something about it.  Harumph!
 *
 **************************************/
SSHORT	errcode;
SLONG	status;

FCHECK ((SSHORT) file->fil_desc, &errcode, NULL, NULL, NULL);
GET_USERMODE
status = (SLONG) errcode << 16;
if (status_vector)
    {
    *status_vector++ = isc_arg_gds;
    *status_vector++ = isc_io_error;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) string;
    *status_vector++ = gds_arg_string;
    *status_vector++ = (STATUS) ERR_string (file->fil_string, file->fil_length);
    *status_vector++ = isc_arg_gds;
    *status_vector++ = operation;
    *status_vector++ = gds_arg_mpexl;
    *status_vector++ = status;
    *status_vector++ = gds_arg_end;
    return FALSE;
    }
else
    ERR_post (isc_io_error, 
	gds_arg_string, string, 
        gds_arg_string, ERR_string (file->fil_string, file->fil_length),
        isc_arg_gds, operation,
    	gds_arg_mpexl, status, 0);
}
