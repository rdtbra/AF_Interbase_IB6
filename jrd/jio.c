/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		jio.c
 *	DESCRIPTION:	Journal file i/o
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

/* NOTE that this module is used only on platforms that use
   VMS style journaling.   At the moment this is limited to
   VMS and MPE XL. */

#include "../jrd/ib_stdio.h"
#include "../jrd/jrd.h"
#include "../jrd/jrn.h"
#include "../jrd/codes.h"

#ifdef VMS
#include <file.h>
#include <lckdef.h>
#include "../jrd/vms.h"
#define EVENT_FLAG	15
#define SYS_ERROR	gds_arg_vms
#endif

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"
#define SYS_ERROR	gds_arg_mpexl
#endif

#ifndef VMS
#ifndef mpexl
#include <sys/types.h>

#ifdef SOLARIS
#include <fcntl.h>
#endif

#ifdef SCO_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#endif

#ifdef DELTA
#include <fcntl.h>
#endif

#ifdef M88K
#include <fcntl.h>
#endif

#ifdef EPSON
#include <fcntl.h>
#endif

#ifdef PC_PLATFORM
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#define FILE_OPEN(name,access,mask)	open (name, access)
#else
#ifdef CRAY
#include <fcntl.h>
#endif
#include <sys/file.h>
#endif

#ifdef NETWARE_386
#include <errno.h>
#endif

#define SYS_ERROR	gds_arg_unix
extern int	errno;
#endif
#endif

#ifndef FILE_OPEN
#define FILE_OPEN(name,access,mask)	open (name, access, mask)
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

#define BLOCKS(length)	((length + BLOCK_SIZE - 1) / BLOCK_SIZE)

extern SLONG LOCK_enq();
extern SLONG LOCK_read_data();
DIR		JIO_next_dir();
SLONG		JIO_rewind();
static SLONG	add_database();
static SLONG	block_number();
static DIR	locate_dir();
static SLONG	lookup_id();

/* Lock Block data */

typedef union {
    struct 
	{
	SSHORT	segment;
	SSHORT	block;
	}	data_items;
    SLONG	data_long;
    } LDATA;

#ifdef mpexl
static LDATA	^jrn_lock_data;
#endif

JIO_fini (journal)
    JRN		journal;
{
/**************************************
 *
 *	J I O _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Signoff journal file.
 *
 **************************************/
LDATA		data;
struct jfh	header;
int		status;

acquire (journal, &data);
position_and_read (journal, 0L, &header, sizeof (header));
header.jfh_write_block = data.data_items.block;
position_and_write (journal, 0L, &header, sizeof (header));

lock_release (journal);

#ifdef VMS
status = sys$dassgn (journal->jrn_channel);

if (!(status & 1))
    error (journal, status, "sys$dassgn", isc_io_access_err);

#endif

#ifdef mpexl
FCLOSE ((SSHORT) journal->jrn_channel, 0, 0);
#endif

#ifndef VMS
#ifndef mpexl
close ((int) journal->jrn_channel);
#endif
#endif
}

JIO_get_position (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	J I O _ g e t _ p o s i t i o n
 *
 **************************************
 *
 * Functional description
 *	Get current output position in journal file.  This is
 *	used to implement FLUSH verb in journal server.
 *
 **************************************/

acquire (journal, data);
release (journal, data);
}

DIR JIO_next_dir (dir)
    DIR		dir;
{
/**************************************
 *
 *	J I O _ n e x t _ d i r
 *
 **************************************
 *
 * Functional description
 *	Compute address of next directory entry.
 *
 **************************************/
USHORT	length;

length = sizeof (struct dir) - 1 + dir->dir_length;
length = (length + 1) & ~1;

return (DIR) ((TEXT*) dir + length);
}

JIO_open (journal)
    JRN		journal;
{
/**************************************
 *
 *	J I O _ o p e n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT		*file_name;
USHORT		new;
LDATA		data;
struct jfh	header;
int		n, mask;

/* Try to open the journal file.  If we can't, try to create it.  If we
   create it, format it.  If we can't open or create, try to open one
   last time. */

if (new = journal_open (journal, &data))
    {
    clear (&header, sizeof (header));
    header.jfh_version = JFH_VERSION;
    header.jfh_free = SEGMENT_SLOTS;
    n = header.jfh_segments [0] = header.jfh_allocation++;
    position_and_write (journal, 0L, &header, sizeof (header));
    }

/* Get intermediate file header */

position_and_read (journal, 0L, &header, sizeof (header));

if (header.jfh_version != JFH_VERSION)
    {
#ifdef mpexl
    lock_release (journal);
#endif
    ERR_post (gds__jrn_format_err, 0);
    /* Msg366 journal file wrong format */
    }

/* Try to get exclusive lock.  If so, do a little cleanup */

if (lock_exclusive (journal, FALSE))
    {
    data.data_items.segment = header.jfh_segments [header.jfh_data];
    if (!new)
	header.jfh_write_block = high_water (journal, &header);
    data.data_items.block = header.jfh_write_block;
    ++header.jfh_series;
    position_and_write (journal, 0L, &header, sizeof (header));
    }
    
journal->jrn_series = header.jfh_series;
release (journal, &data);
}

JIO_put (journal, message, size)
    JRN		journal;
    LTJC	*message;
    USHORT	size;
{
/**************************************
 *
 *	J I O _ p u t
 *
 **************************************
 *
 * Functional description
 *	Write a record to the journal file.
 *
 **************************************/
SLONG		block, blocks;
SSHORT		n, fudge;
struct jrnh	dummy;
LDATA		data;
USHORT		waited;

/* Get write lock on journal file.  The lock block will tell us where
   to write next */

acquire (journal, &data);

/* Do any message processing necessary */

switch (message->ltjc_header.jrnh_type)
    {
    case JRN_ENABLE:
	journal->jrn_handle = add_database (journal, 
		message->ltjc_data, message->ltjc_length);
	message->ltjc_header.jrnh_handle = journal->jrn_handle;
	message->ltjc_header.jrnh_series = journal->jrn_series;
	break;

    case JRN_SIGN_ON:
	journal->jrn_handle = lookup_id (journal, 
		message->ltjc_data, message->ltjc_length);
	if (!journal->jrn_handle)
	    {
	    journal->jrn_handle = add_database (journal, 
		message->ltjc_data, message->ltjc_length);
	    journal->jrn_dump = -1;
	    }
	message->ltjc_header.jrnh_handle = journal->jrn_handle;
	message->ltjc_header.jrnh_series = journal->jrn_series;
	break;
    }

/* Compute next available block.  If there isn't room in this segment, write a 
   dummy block to take the extra space, then allocate new data segment. */

block = block_number (data.data_items.segment, data.data_items.block);
blocks = (SLONG) ( BLOCKS (size) );

if (data.data_items.block + (SSHORT) blocks > SEGMENT_SIZE)
    {
    if (fudge = SEGMENT_SIZE - data.data_items.block)
	{
	dummy.jrnh_type = JRN_DUMMY;
	dummy.jrnh_length = BLOCK_SIZE * fudge;
	dummy.jrnh_handle = journal->jrn_handle;
	dummy.jrnh_series = journal->jrn_series;
	position_and_write (journal, block, &dummy, sizeof (struct jrnh));
	}

    waited = 0;
    while ((n = extend_segments (journal)) == -1 && waited < 300)
	{
	release (journal, &data);
	waited += 10;
	sleep (10);
	acquire (journal, &data);
	}
    if (n == -1)
	ERR_post (gds__jrn_file_full, 0);
	/* Msg840 intermediate journal file full */
    data.data_items.segment = n;
    data.data_items.block = 0;
    block = block_number (data.data_items.segment, data.data_items.block);
    }

n = (size + 1) & ~1;
position_and_write (journal, block, message, n);
data.data_items.block += (SSHORT) blocks;
release (journal, &data);
}

JIO_read (journal, position, buffer, size)
    JRN		journal;
    USHORT	*position;
    JRNH	*buffer;
    USHORT	size;
{
/**************************************
 *
 *	J I O _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a record from the journal file.  Returns the number of bytes read.
 *
 **************************************/
USHORT		length, n, *ptr;
SLONG		block;
struct jfh	header;
LDATA		data;

/* Acquire lock file */

acquire (journal, &data);

/* Make sure we don't pass writers */

for (;;)
    {
    if (position [0] == data.data_items.segment &&
	position [1] >= data.data_items.block)
	{
	release (journal, &data);
	return 0;
	}
    if (position [1] < SEGMENT_SIZE)
	break;
    position_and_read (journal, 0L, &header, sizeof (header));
    for (ptr = header.jfh_segments; *ptr != position [0]; ++ptr)
	;
    position [0] = ptr [1];
    position [1] = 0;
    }

/* Read first block of record.  This may be too much or too little,
   so we may have to read more */

block = block_number (position [0], position [1]);
position_and_read (journal, block, buffer, BLOCK_SIZE);
length = buffer->jrnh_length;

if (length > BLOCK_SIZE)
    {
    n = length - BLOCK_SIZE;
    n = (n + 1 ) & ~ 1;
    position_and_read (journal, block + 1, (UCHAR*) buffer + BLOCK_SIZE, n);
    }

/* Update cursor */

position [1] += BLOCKS (length);

/* Release file and return */

release (journal, &data);

return length;
}

JIO_read_header (journal, header, h_length, id_space, i_length)
    JRN		journal;
    JFH		header;
    USHORT	h_length;
    TEXT	*id_space;
    USHORT	i_length;
{
/**************************************
 *
 *	J I O _ r e a d _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Read header data.  If lengths are wrong, return FALSE and
 *	give up, otherwise return TRUE.
 *
 **************************************/
LDATA		data;
USHORT		n;

if (h_length != sizeof (struct jfh)) 
    return FALSE;

acquire (journal, &data);
position_and_read (journal, 0L, header, h_length);

if (id_space)
    if (n = MIN (i_length, header->jfh_id_space))
	position_and_read (journal, (SLONG) ID_OFFSET, id_space, n);

release (journal, &data);

return TRUE;
}

SLONG JIO_rewind (journal, position)
    JRN		journal;
    USHORT	*position;
{
/**************************************
 *
 *	J I O _ r e w i n d
 *
 **************************************
 *
 * Functional description
 *	Prepare to read from journal.  Return current
 *	sequence number.
 *
 **************************************/
USHORT		block, length, n, *ptr;
struct jfh	header;
LDATA		data;
SLONG		sequence;

/* Acquire lock file */

acquire (journal, &data);
position_and_read (journal, 0L, &header, sizeof (header));
position [0] = header.jfh_segments [0];
position [1] = header.jfh_copy_block;
sequence = header.jfh_sequence;
release (journal, &data);

return sequence;
}

JIO_truncate (journal, position, sequence)
    JRN		journal;
    USHORT	*position;
    SLONG	sequence;
{
/**************************************
 *
 *	J I O _ t r u n c a t e
 *
 **************************************
 *
 * Functional description
 *	Release any data pages up to a given point.
 *
 **************************************/
USHORT		n, length, *ptr, *ptr2, slots [SEGMENT_SLOTS];
struct jfh	header;
LDATA		data;

/* Acquire lock file */

acquire (journal, &data);
position_and_read (journal, 0L, &header, sizeof (header));

for (ptr = header.jfh_segments, ptr2 = slots; *ptr != position [0];)
    *ptr2++ = *ptr++;

if (n = ptr2 - slots)
    {
    header.jfh_data -= n;
    length = (header.jfh_data + 1) * sizeof (USHORT);
    move (ptr, header.jfh_segments, length);
    header.jfh_free -= n;
    move (slots, header.jfh_segments + header.jfh_free, n * sizeof (USHORT));
    }

header.jfh_copy_block = position [1];
header.jfh_sequence = sequence;
position_and_write (journal, 0L, &header, sizeof (header));

release (journal, &data);
}

static acquire (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	a c q u i r e
 *
 **************************************
 *
 * Functional description
 *	Acquire write lock on journal file, getting lock data block.
 *
 **************************************/

#ifdef VMS
vms_convert (journal, data, LCK$K_PWMODE, TRUE);
#endif

#ifdef mpexl
ISC_file_lock ((SSHORT) journal->jrn_channel);
data->data_long = jrn_lock_data->data_long;
#endif

#ifndef VMS
#ifndef mpexl
LOCK_convert (journal->jrn_lock_id, LCK_PW, TRUE, NULL_PTR, NULL_PTR, journal->jrn_status_vector);
data->data_long = LOCK_read_data (journal->jrn_lock_id);
#endif
#endif
}

static SLONG add_database (journal, name_string, name_length)
    JRN		journal;
    TEXT	*name_string;
    USHORT	name_length;
{
/**************************************
 *
 *	a d d _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Add a new database to journal file.  Return new id.
 *
 **************************************/
TEXT		id_space [ID_SPACE];
DIR		dir, next;
struct jfh	header;
ULONG		id;

position_and_read (journal, 0L, &header, sizeof (header));
position_and_read (journal, (SLONG) ID_OFFSET, id_space, header.jfh_id_space);
delete_dir (&header, id_space, name_string, name_length);
dir = (DIR) (id_space + header.jfh_id_space);

dir->dir_id = id = ++header.jfh_next_id;
dir->dir_length = name_length;
strncpy (dir->dir_name, name_string, name_length);
dir->dir_last_page = 0;

next = JIO_next_dir (dir);
header.jfh_id_space = (TEXT*) next - id_space;
position_and_write (journal, (SLONG) ID_OFFSET, id_space, header.jfh_id_space);
position_and_write (journal, 0L, &header, sizeof (header));

return id;
}

static SLONG block_number (segment, block)
    USHORT	segment;
    USHORT	block;
{
/**************************************
 *
 *	b l o c k _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Translate from (segment, block) to physical block number.
 *
 **************************************/

return segment * SEGMENT_SIZE + block + SEGMENT_OFFSET;
}

static clear (block, size)
    UCHAR	*block;
    USHORT	size;
{
/**************************************
 *
 *	c l e a r
 *
 **************************************
 *
 * Functional description
 *	Clear a hunk of memory.
 *
 **************************************/

if (size)
    do *block++ = 0; while (--size);
}

static delete_dir (header, id_space, name_string, name_length)
    JFH		header;
    TEXT	*id_space;
    TEXT	*name_string;
    USHORT	name_length;
{
/**************************************
 *
 *	d e l e t e _ d i r
 *
 **************************************
 *
 * Functional description
 *	Delete a database entry from the journal file.
 *
 **************************************/
DIR	dir, next;
USHORT	l;

if (!(dir = locate_dir (header, id_space, name_string, name_length)))
    return FAILURE;

next = JIO_next_dir (dir);
l = (TEXT*) next - id_space;
move (next, dir, header->jfh_id_space - l);
header->jfh_id_space -= (TEXT*) next - (TEXT*) dir;

return SUCCESS;
}

static error (journal, value, string, operation)
    JRN		journal;
    int		value;
    TEXT	*string;
    STATUS  operation;
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	We've had an unexpected error -- punt.
 *
 **************************************/
SSHORT	errcode;

#ifdef mpexl
if (!value)
    {
    FCHECK ((SSHORT) journal->jrn_channel, &errcode, NULL, NULL, NULL);
    value = (SLONG) errcode << 16;
    }
#endif

ERR_post (isc_io_error,
	gds_arg_string, string,
	gds_arg_string, journal->jrn_server,
    isc_arg_gds, operation,
	SYS_ERROR, value,
	0);
}

#ifdef VMS
static extend_file (journal)
    JRN		journal;
{
/**************************************
 *
 *	e x t e n d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Extend the length of a file on VMS using QIOs.  This is
 *	a royal pain in the tush.
 *
 **************************************/
USHORT			iosb [4], l;
struct dsc$descriptor_s	desc;
ATR$			atr;
FIB$			fib;
FAT$			fat;
int			status, old_size, new_size, delta;
SCHAR			*p;

ISC_make_desc (&fib, &desc, sizeof (fib));
atr.atr$w_size = ATR$S_RECATTR;
atr.atr$w_type = ATR$C_RECATTR;
atr.atr$l_addr = &fat;
atr.atr$l_zero = 0;

/* Get current size */

p = &fib;
l = sizeof (fib);
do *p++ = 0; while (--l);

move (journal->jrn_did, fib.fib$w_did, sizeof (fib.fib$w_did));
move (journal->jrn_fid, fib.fib$w_fid, sizeof (fib.fib$w_fid));
fib.fib$w_nmctl = FIB$M_FINDFID;

status = sys$qiow (
	15, 				/* Event flag */
	journal->jrn_channel,		/* Channel */
	IO$_ACCESS,			/* Function */
	iosb,				/* IO status block */
	NULL, 				/* AST address */
	NULL,				/* AST parameter */
	&desc,				/* P1 (buffer) */
	NULL,				/* P2 (length) */
	NULL,				/* P3 (virtual block) */
	NULL, 				/* P4 */
	&atr, 				/* P5 */
	NULL);				/* P6 */

if (!(status & 1) || !((status = iosb [0]) & 1))
    error (journal, status, "sys$enqw (journal io$_access)",
            isc_io_access_err);

old_size = fat.fat$w_hiblk [1] + (fat.fat$w_hiblk [0] << 16);
new_size = old_size + MIN (old_size, 16000);
delta = new_size - old_size;

/* Extend file */

fib.fib$l_exsz = delta;
fib.fib$l_exvbn = 0;
fib.fib$l_acctl = FIB$M_WRITETHRU;
fib.fib$w_exctl = FIB$M_EXTEND | FIB$M_ALDEF | FIB$M_ALCONB;

fat.fat$w_efblk [0] = (new_size + 1) >> 16;
fat.fat$w_efblk [1] = (new_size + 1);

status = sys$qiow (
	15, 				/* Event flag */
	journal->jrn_channel,		/* Channel */
	IO$_MODIFY,			/* Function */
	iosb,				/* IO status block */
	NULL, 				/* AST address */
	NULL,				/* AST parameter */
	&desc,				/* P1 (buffer) */
	NULL,				/* P2 (length) */
	NULL,				/* P3 (virtual block) */
	NULL, 				/* P4 */
	&atr, 				/* P5 */
	NULL);				/* P6 */

if (!(status & 1) || !((status = iosb [0]) & 1))
    error (journal, status, "sys$enqw (journal io$_modify)",
            isc_io_access_err);
}
#endif

static extend_segments (journal)
    JRN		journal;
{
/**************************************
 *
 *	e x t e n d _ s e g m e n t s
 *
 **************************************
 *
 * Functional description
 *	Allocate new data segment for journal.  If the file
 *	is full, return -1.
 *
 **************************************/
USHORT		segment, slots;
struct jfh	header;

position_and_read (journal, 0L, &header, sizeof (header));

/* Find a free segment.  Allocate one if nothing is available */

if (header.jfh_free < SEGMENT_SLOTS)
    segment = header.jfh_segments [header.jfh_free++];
else if (header.jfh_data + 1 >= header.jfh_free)
    return -1;
else
    segment = header.jfh_allocation++;

header.jfh_segments [++header.jfh_data] = segment;
header.jfh_write_block = 0;
position_and_write (journal, 0L, &header, sizeof (header));

return segment;
}

#ifdef VMS
static journal_open (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	j o u r n a l _ o p e n   ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Open the journal file.  If a new one is created
 *	return TRUE, otherwise return FALSE.
 *
 **************************************/
USHORT		new, l, iosb [4];
int		status;
TEXT		*address, expanded_name [NAM$C_MAXRSS], *op, 
		lock_string [64], *p, *q;
struct FAB	fab;
struct NAM	nam;
LKSB		lksb;
ITM		items [2];
struct dsc$descriptor_s	string;

new = FALSE;

fab = cc$rms_fab;
nam = cc$rms_nam;
fab.fab$l_alq = (SEGMENT_OFFSET + SEGMENT_SIZE * 10);
fab.fab$w_deq = SEGMENT_SIZE * 10;
fab.fab$l_nam = &nam;
fab.fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
fab.fab$l_fop = FAB$M_UFO;
fab.fab$l_fna = journal->jrn_server;
fab.fab$b_fns = strlen (fab.fab$l_fna);
fab.fab$b_rfm = FAB$C_UDF;
fab.fab$b_shr = FAB$M_SHRGET | FAB$M_SHRPUT | FAB$M_UPI;

nam.nam$l_rsa = expanded_name;
nam.nam$b_rss = sizeof (expanded_name);

op = "sys$open";
status = sys$open (&fab);

/* If the open failed, by creating the file instead */

if (status == RMS$_FNF)
    {
    op = "sys$create";
    status = sys$create (&fab);
    new = TRUE;
    }

/* If neither worked, punt */

if (!(status & 1))
    error (journal, status, op, isc_io_open_err);

journal->jrn_channel = fab.fab$l_stv;
move (nam.nam$w_did, journal->jrn_did, sizeof (journal->jrn_did));
move (nam.nam$w_fid, journal->jrn_fid, sizeof (journal->jrn_fid));

/* Create lock string for journal file.  This is truely a
   pain in the ass since this must work anywhere on a
   cluster */

items[0].itm_code = DVI$_DEVLOCKNAM;
items[0].itm_length = sizeof (lock_string);
items[0].itm_buffer = lock_string;
items[0].itm_return_length = &l;

items [1].itm_code = 0;
items [1].itm_length = 0;
items [1].itm_buffer = 0;

status = sys$getdviw (15, journal->jrn_channel, NULL,
    	items, iosb, NULL, NULL, NULL);

if (status != 1)
    error (journal, status, "sys$getdviw", isc_io_access_err);

move (nam.nam$w_fid, lock_string + l, sizeof (nam.nam$w_fid));
ISC_make_desc (lock_string, &string, l + sizeof (nam.nam$w_fid));

status = sys$enqw (
    EVENT_FLAG,
    LCK$K_PWMODE,
    &lksb,
    LCK$M_SYSTEM | LCK$M_VALBLK,
    &string,
    0,				/* parent lock id */
    NULL,			/* AST routine when granted */
    NULL,			/* ast_argument */
    NULL,			/* ast_routine */
    NULL,
    NULL);

if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

status = lksb.lksb_status;

if (status == SS$_VALNOTVALID)
    data->data_long = 0;
else if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

journal->jrn_lock_id = lksb.lksb_lock_id;

if (data)
    data->data_long = lksb.lksb_value [0];

return new;
}
#endif

#ifdef mpexl
static journal_open (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	j o u r n a l _ o p e n   ( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Open the journal file.  If a new one is created
 *	return TRUE, otherwise return FALSE.  On MPE/XL,
 *	leave with a write lock acquired on the file.
 *
 **************************************/
USHORT	new;
TEXT	temp [256], *p, *q;
SLONG	filenum, status, domain, recfm, access, locking, exclusive,
	multi, lrecl, random, buffering, disposition, binary;
struct jfh	^mapped_ptr;

new = FALSE;

temp [0] = ' ';
for (p = &temp [1], q = journal->jrn_server; *p = *q++; p++)
    ;
*p = ' ';

domain = DOMAIN_OPEN;
access = ACCESS_RW;
locking = LOCKING_YES;
exclusive = EXCLUSIVE_SHARE;
multi = MULTIREC_YES;
random = WILL_ACC_RANDOM;
buffering = INHIBIT_BUF_YES;

HPFOPEN (&filenum, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_ACCESS_TYPE, &access,
    OPN_LOCKING, &locking,
    OPN_EXCLUSIVE, &exclusive,
    OPN_MULTIREC, &multi,
    OPN_LMAP, &mapped_ptr,
    OPN_WILL_ACCESS, &random,
    OPN_INHIBIT_BUF, &buffering,
    ITM_END);

if (status)
    {
    domain = DOMAIN_CREATE_PERM;
    recfm = RECFM_F;
    lrecl = BLOCK_SIZE;
    disposition = DISP_KEEP;
    binary = FORMAT_BINARY;
    HPFOPEN (&filenum, &status,
	OPN_FORMAL, temp,
	OPN_DOMAIN, &domain,
	OPN_RECFM, &recfm,
	OPN_ACCESS_TYPE, &access,
	OPN_LOCKING, &locking,
	OPN_EXCLUSIVE, &exclusive,
	OPN_MULTIREC, &multi,
	OPN_LRECL, &lrecl,
	OPN_LMAP, &mapped_ptr,
	OPN_WILL_ACCESS, &random,
	OPN_INHIBIT_BUF, &buffering,
	OPN_DISP, &disposition,
	OPN_FORMAT, &binary,
	ITM_END);
    new = TRUE;
    }

if (status)
    error (journal, status, "HPFOPEN", isc_io_open_err);

/* Get a write lock on the journal file because on MPE/XL,
   the lock data is in the file. */

ISC_file_lock ((SSHORT) filenum);

journal->jrn_channel = filenum;

jrn_lock_data = &mapped_ptr->jfh_lock_data;
if (new)
    jrn_lock_data->data_long = 0;
if (data)
    data->data_long = jrn_lock_data->data_long;

return new;
}
#endif

#ifndef VMS
#ifndef mpexl
static journal_open (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	j o u r n a l _ o p e n   ( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Open the journal file.  If a new one is created
 *	return TRUE, otherwise return FALSE.
 *
 **************************************/
USHORT		new;
int		n, mask;

new = FALSE;

/* Try to open the journal file.  If we can't, try to create it.  If we
   create it, format it.  If we can't open or create, try to open one
   last time. */

n = FILE_OPEN (journal->jrn_server, O_RDWR | O_BINARY, 0666);

if (n == -1)
    {
    mask = umask (0);
    n = FILE_OPEN (journal->jrn_server, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0666);
    umask (mask);
    if (n == -1)
	n = FILE_OPEN (journal->jrn_server, O_RDWR | O_BINARY, 0666);
    else
	new = TRUE;
    }

if (n == -1)
    error (journal, errno, "open", isc_io_open_err);

journal->jrn_channel = (int*) n;
lock_create (journal, data);

return new;
}
#endif
#endif

static DIR locate_dir (header, id_space, name_string, name_length)
    JFH		header;
    TEXT	*id_space;
    TEXT	*name_string;
    USHORT	name_length;
{
/**************************************
 *
 *	l o c a t e _ d i r
 *
 **************************************
 *
 * Functional description
 *	Find database entry in database id record.
 *
 **************************************/
DIR	dir, end;

end = (DIR) (id_space + header->jfh_id_space);

for (dir = (DIR) id_space; dir < end; dir = JIO_next_dir (dir))
    if (name_length == dir->dir_length && !strncmp (dir->dir_name, name_string, name_length))
	return dir;

return NULL;
}

#ifdef VMS
static lock_create (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	l o c k _ c r e a t e   ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Create lock for journal file.
 *
 **************************************/
struct dsc$descriptor	string;
TEXT	filename [256];
SLONG	status;
LKSB	lksb;

getname (journal->jrn_channel, filename, 1);
filename [31] = 0;
ISC_make_desc (filename, &string, 0);

status = sys$enqw (
    EVENT_FLAG,
    LCK$K_PWMODE,
    &lksb,
    LCK$M_SYSTEM | LCK$M_VALBLK,
    &string,
    0,				/* parent lock id */
    NULL,			/* AST routine when granted */
    NULL,			/* ast_argument */
    NULL,			/* ast_routine */
    NULL,
    NULL);

if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

status = lksb.lksb_status;

if (status == SS$_VALNOTVALID)
    data->data_long = 0;
else if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

journal->jrn_lock_id = lksb.lksb_lock_id;

if (data)
    data->data_long = lksb.lksb_value [0];
}
#endif

#ifndef VMS
#ifndef mpexl
static lock_create (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	l o c k _ c r e a t e    ( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Create lock for journal file.
 *
 **************************************/

journal->jrn_lock_id = LOCK_enq (
	0L,				/* Prior */
	0L, 				/* Parent */
	0, 				/* Series */
	journal->jrn_server, 		/* Lock string */
	strlen (journal->jrn_server),	/* length of lock string */
	LCK_PW, 
	NULL_PTR, NULL_PTR, 0L, 1,
	journal->jrn_status_vector);

data->data_long = LOCK_read_data (journal->jrn_lock_id);
}
#endif
#endif

static lock_exclusive (journal, flag)
    JRN		journal;
    SSHORT	flag;
{
/**************************************
 *
 *	l o c k _ e x c l u s i v e
 *
 **************************************
 *
 * Functional description
 *	Try to get an exclusive lock on the journal file, but
 *	don't block trying.  Return TRUE if we succeed, FALSE
 *	otherwise.
 *
 **************************************/
SSHORT	readers, writers;

#ifdef VMS
return vms_convert (journal, NULL, LCK$K_EXMODE, FALSE);
#endif

#ifdef mpexl
if (flag)
    ISC_file_lock ((SSHORT) journal->jrn_channel);
FFILEINFO ((SSHORT) journal->jrn_channel,
    INF_NREADERS, &readers,
    INF_NWRITERS, &writers,
    ITM_END);

return (readers == 1 && writers == 1);
#endif

#ifndef VMS
#ifndef mpexl
return LOCK_convert (journal->jrn_lock_id, LCK_EX, 0, NULL_PTR, NULL_PTR,
		     journal->jrn_status_vector);
#endif
#endif
}

static lock_release (journal)
    JRN		journal;
{
/**************************************
 *
 *	l o c k _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Release the lock on the journal file.
 *
 **************************************/

#ifdef VMS
sys$setast (1);
sys$deq (journal->jrn_lock_id, NULL, NULL, NULL);
#endif

#ifdef mpexl
ISC_file_unlock ((SSHORT) journal->jrn_channel);
#endif

#ifndef VMS
#ifndef mpexl
LOCK_deq (journal->jrn_lock_id);
#endif
#endif
}

static SLONG lookup_id (journal, name_string, name_length)
    JRN		journal;
    TEXT	*name_string;
{
/**************************************
 *
 *	l o o k u p _ i d
 *
 **************************************
 *
 * Functional description
 *	Lookup database id.
 *
 **************************************/
struct jfh	header;
TEXT		id_space [ID_SPACE];
DIR		dir;

position_and_read (journal, 0L, &header, sizeof (header));
position_and_read (journal, (SLONG) ID_OFFSET, id_space, header.jfh_id_space);

if (dir = locate_dir (&header, id_space, name_string, name_length))
    return dir->dir_id;

return 0L;
}

static high_water (journal, header)
    JRN		journal;
    JFH		header;
{
/**************************************
 *
 *	h i g h _ w a t e r
 *
 **************************************
 *
 * Functional description
 *	We have exclusive access to an existing journal file.  It
 *	is possible that the last guy didn't update the "last write"
 *	slot before going away.  Try to find the last valid block
 *	written, and from that, the next block available.
 *
 **************************************/
USHORT		block, segment;
struct jrnh	record;

segment = header->jfh_segments [header->jfh_data];

for (block = header->jfh_write_block; block < SEGMENT_SIZE;
     block += BLOCKS (record.jrnh_length))
    {
    position_and_read (journal, block_number (segment, block), &record, sizeof (record));
    if (record.jrnh_type < JRN_ENABLE ||
	record.jrnh_type > JRN_DUMMY ||
	record.jrnh_series != header->jfh_series ||
	record.jrnh_handle > header->jfh_next_id)
	return block;
    }

return SEGMENT_SIZE;
}

static move (from, to, length)
    UCHAR	*from;
    UCHAR	*to;
    USHORT	length;
{
/**************************************
 *
 *	m o v e
 *
 **************************************
 *
 * Functional description
 *	Move some bytes.
 *
 **************************************/

if (length)
    do *to++ = *from++; while (--length);
}

#ifdef VMS
static position_and_read (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    USHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ r e a d   ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and read record.
 *
 **************************************/
SSHORT	iosb [4];
int	status;

if (!length)
    return;

status = sys$qiow (
	15, 				/* Event flag */
	journal->jrn_channel,		/* Channel */
	IO$_READVBLK,			/* Function */
	iosb,				/* IO status block */
	NULL, 				/* AST address */
	NULL,				/* AST parameter */
	buffer,				/* P1 (buffer) */
	length,				/* P2 (length) */
	block + 1,			/* P3 (virtual block) */
	NULL, NULL, NULL);

if (status == SS$_ENDOFFILE)
    {
    do *buffer++ = 0; while (--length);
    return;
    }

if (!(status & 1))
    error (journal, status, "sys$qiow (journal read)", isc_io_read_err);
}
#endif

#ifdef mpexl
static position_and_read (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    SSHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ r e a d   ( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and read record.
 *
 **************************************/

if (!length)
    return;

FREADDIR ((SSHORT) journal->jrn_channel, buffer, -length, block);
if (ccode() == CCL)
    error (journal, 0, "FREADDIR (journal read)", isc_io_read_err);
else if (ccode() == CCG)
    {
    do *buffer++ = 0; while (--length);
    return;
    }
}
#endif

#ifndef VMS
#ifndef mpexl
static position_and_read (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    USHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ r e a d   ( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and read record.
 *
 **************************************/
int	n;
SLONG	position;

position = (SLONG) ( block * BLOCK_SIZE );
n = lseek ((int) journal->jrn_channel, position, 0);

if (n == -1)
    error (journal, errno, "lseek", isc_io_read_err);

n = read ((int) journal->jrn_channel, buffer, length);

if (n == -1)
    error (journal, errno, "read", isc_io_read_err);
}
#endif
#endif

#ifdef VMS
static position_and_write (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    USHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ w r i t e   ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and write record.
 *
 **************************************/
SSHORT	iosb [4];
int	status;

for (;;)
    {
    status = sys$qiow (
	15, 				/* Event flag */
	journal->jrn_channel,		/* Channel */
	IO$_WRITEVBLK,			/* Function */
	iosb,				/* IO status block */
	NULL, 				/* AST address */
	NULL,				/* AST parameter */
	buffer,				/* P1 (buffer) */
	length,				/* P2 (length) */
	block + 1,			/* P3 (virtual block) */
	NULL, NULL, NULL);

    if ((status & 1) && ((status = iosb [0]) & 1))
	break;

    if (status == SS$_ENDOFFILE)
	extend_file (journal);
    else
	error (journal, status, "sys$qiow (journal write)", isc_io_write_err);
    }
}
#endif

#ifdef mpexl
static position_and_write (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    SSHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ w r i t e   ( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and write record.
 *
 **************************************/

FWRITEDIR ((SSHORT) journal->jrn_channel, buffer, -length, block);
if (ccode() != CCE)
    error (journal, 0, "FWRITEDIR (journal write)", isc_io_write_err);
}
#endif

#ifndef VMS
#ifndef mpexl
static position_and_write (journal, block, buffer, length)
    JRN		journal;
    ULONG	block;
    UCHAR	*buffer;
    USHORT	length;
{
/**************************************
 *
 *	p o s i t i o n _ a n d _ w r i t e   ( u n i x )
 *
 **************************************
 *
 * Functional description
 *	Position journal file to particular block and write record.
 *
 **************************************/
int	position, n;

position = block * BLOCK_SIZE;
n = lseek ((int) journal->jrn_channel, position, 0);

if (n == -1)
    error (journal, errno, "lseek", isc_io_write_err);

n = write ((int) journal->jrn_channel, buffer, length);

if (n == -1)
    error (journal, errno, "write", isc_io_write_err);
}
#endif
#endif

static release (journal, data)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Down grade file and and update lock block.
 *
 **************************************/

#ifdef VMS
vms_convert (journal, data, LCK$K_CRMODE, TRUE);
#endif

#ifdef mpexl
jrn_lock_data->data_long = data->data_long;
ISC_file_unlock ((SSHORT) journal->jrn_channel);
#endif

#ifndef VMS
#ifndef mpexl
LOCK_write_data (journal->jrn_lock_id, data->data_long);
LOCK_convert (journal->jrn_lock_id, LCK_SR, TRUE, NULL_PTR, NULL_PTR, journal->jrn_status_vector);
#endif
#endif
}

#ifdef VMS
static vms_convert (journal, data, mode, wait)
    JRN		journal;
    LDATA	*data;
{
/**************************************
 *
 *	v m s _ c o n v e r t
 *
 **************************************
 *
 * Functional description
 *	Acquire write lock on journal file, getting lock data block.
 *
 **************************************/
SLONG			status, flags;
LKSB			lksb;

lksb.lksb_lock_id = journal->jrn_lock_id;

if (data)
    {
    if (mode == LCK$K_PWMODE || mode == LCK$K_EXMODE)
	sys$setast (0);
    lksb.lksb_value [0] = data->data_long;
    }

flags = LCK$M_CONVERT;

if (data)
     flags |= LCK$M_VALBLK;

if (!wait)
    flags |= LCK$M_NOQUEUE;

status = sys$enqw (
    EVENT_FLAG,
    mode,
    &lksb,
    flags,
    NULL,
    NULL,
    NULL,			/* AST routine when granted */
    NULL,			/* ast_argument */
    NULL,			/* ast_routine */
    NULL,
    NULL);

if (!wait && status == SS$_NOTQUEUED)
    return FALSE;

if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

status = lksb.lksb_status;

if (!(status & 1))
    error (journal, status, "sys$enqw", isc_io_access_err);

if (data)
    {
    data->data_long = lksb.lksb_value [0];
    if (!(mode == LCK$K_PWMODE || mode == LCK$K_EXMODE))
	sys$setast (1);
    }

return TRUE;
}
#endif
