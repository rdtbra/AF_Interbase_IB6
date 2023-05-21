/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		utl.c
 *	DESCRIPTION:	User callable routines
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

#ifdef SHLIB_DEFS
#define LOCAL_SHLIB_DEFS
#endif

#include "../jrd/ib_stdio.h"
#include <stdlib.h>
#include <string.h>
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/time.h"
#include "../jrd/misc.h"

#ifdef APOLLO
#define NO_OSRI_ENTRYPOINTS
#endif
#include "../jrd/gds.h"
#ifdef APOLLO
#undef NO_OSRI_ENTRYPOINTS
#endif
#include "../jrd/msg.h"
#include "../jrd/gds_proto.h"
#include "../jrd/utl_proto.h"
#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
#include "../jrd/blb_proto.h"
#endif

#ifdef VMS
#include <file.h>
#include <ib_perror.h>
#include <descrip.h>
#include <types.h>
#include <stat.h>

#else  /* !VMS */

#ifdef mpexl
#include <sys/types.h>
#include <mpe.h>
#include "../jrd/mpexl.h"

#else  /* !mpexl */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef PC_PLATFORM
#include <fcntl.h>
#include <direct.h>
#ifdef DOS_ONLY
#include <process.h>
#define system(cmd)	spawnlp (P_WAIT, "command.com", "command.com", "/c", cmd, NULL)
#endif
#else
#ifdef NETWARE_386
#include <fcntl.h>
#else
#if (defined WIN_NT || defined OS2_ONLY)
#include <io.h>
#include <process.h>
#else
#include <sys/file.h>
#endif
#endif
#endif

#endif  /* mpexl */

#endif  /* VMS */

#define statistics		stat

#ifdef APOLLO
#include <apollo/base.h>
#include <apollo/error.h>
#include <apollo/pad.h>
#include "/sys/ins/streams.ins.c"
#include "/sys/ins/type_uids.ins.c"
#include "/sys/ins/ios.ins.c"

#include "../jrd/termtype.h"

std_$call gds__blob_info();
std_$call gds__cancel_blob();
std_$call gds__close_blob();
std_$call gds__create_blob();
std_$call gds__create_blob2();
std_$call gds__database_info();
std_$call gds__get_segment();
std_$call gds__open_blob();
std_$call gds__open_blob2();
std_$call gds__put_segment();
#endif  /* APOLLO */

#ifdef sparc
#ifndef SOLARIS
extern int	ib_printf();
#endif
#endif

#ifdef APOLLO
#define GDS_EDIT	static edit_dumb
#endif

#ifdef UNIX
#define GDS_EDIT	gds__edit
#endif

#ifdef PC_PLATFORM
#define GDS_EDIT	gds__edit
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#define GDS_EDIT	gds__edit
#endif

#ifdef NETWARE_386
#define GDS_EDIT        gds__edit
#endif

#ifdef VMS
#ifdef __ALPHA
#define EDT_IMAGE	"TPUSHR"
#define EDT_SYMBOL	"TPU$EDIT"
#else
#define EDT_IMAGE	"EDTSHR"
#define EDT_SYMBOL	"EDT$EDIT"
#endif
#endif

#ifdef mpexl
#define FOPEN_READ_TYPE		"r Tm"
#define FOPEN_WRITE_TYPE	"w Ds2 V E32 S256000"
#endif

/* Bug 7119 - BLOB_load will open external file for read in BINARY mode. */

#ifdef WIN_NT
#define FOPEN_READ_TYPE		"rb"
#define FOPEN_WRITE_TYPE	"wb"
#define FOPEN_READ_TYPE_TEXT	"rt"
#define FOPEN_WRITE_TYPE_TEXT	"wt"
#endif

#ifndef FOPEN_READ_TYPE
#define FOPEN_READ_TYPE		"r"
#endif

#ifndef FOPEN_WRITE_TYPE
#define FOPEN_WRITE_TYPE	"w"
#endif

#ifndef FOPEN_READ_TYPE_TEXT
#define FOPEN_READ_TYPE_TEXT	FOPEN_READ_TYPE 
#endif

#ifndef FOPEN_WRITE_TYPE_TEXT
#define FOPEN_WRITE_TYPE_TEXT	FOPEN_WRITE_TYPE
#endif

#define LOWER7(c) ( (c >= 'A' && c<= 'Z') ? c + 'a' - 'A': c )

/* Blob stream stuff */ 

#define BSTR_input	0
#define BSTR_output	1
#define BSTR_alloc	2

static int	dump (GDS__QUAD *, void *, void *, IB_FILE *);
static int	edit (GDS__QUAD *, void *, void *, SSHORT, SCHAR *);
static int	get_ods_version (void **, USHORT *, USHORT *);
static int	load (GDS__QUAD *, void *, void *, IB_FILE *);

#ifdef APOLLO
static int	edit_dumb (TEXT *, USHORT);
#endif
#ifdef mpexl
static int	stat (TEXT *, SLONG *, SSHORT *);
#endif
#ifdef VMS
static int	display (GDS__QUAD *, int *, int *);
#endif

/* Blob info stuff */

static CONST SCHAR blob_items [] =
	{ gds__info_blob_max_segment, gds__info_blob_num_segments, gds__info_blob_total_length };

/* gds__version stuff */

static CONST UCHAR	info [] =
	{ gds__info_version, gds__info_implementation, gds__info_end };

static CONST UCHAR	ods_info [] =
	{ gds__info_ods_version, gds__info_ods_minor_version, gds__info_end };

static CONST TEXT
    * CONST impl_class [] =
	{
	NULL,				/* 0 */
	"access method",		/* 1 */
	"Y-valve",			/* 2 */
	"remote interface",		/* 3 */
	"remote server",		/* 4 */
	NULL,				/* 5 */
	NULL,				/* 6 */
	"pipe interface",		/* 7 */
	"pipe server",			/* 8 */
	"central interface",		/* 9 */
	"central server",		/* 10 */
	"gateway",			/* 11 */
	},

    * CONST impl_implementation [] =
	{             	
	NULL,				/* 0 */
	"Rdb/VMS",			/* 1 */
	"Rdb/ELN target",		/* 2 */
	"Rdb/ELN development",		/* 3 */
	"Rdb/VMS Y",			/* 4 */
	"Rdb/ELN Y",			/* 5 */
	"JRI",				/* 6 */
	"JSV",				/* 7 */
	NULL,				/* 8 */
	NULL,				/* 9 */
	"InterBase/apollo",		/* 10 */
	"InterBase/ultrix",		/* 11 */
	"InterBase/vms",		/* 12 */
	"InterBase/sun",		/* 13 */
	"InterBase/OS2", 		/* 14 */
	NULL, 				/* 15 */
	NULL, 				/* 16 */
	NULL, 				/* 17 */
	NULL, 				/* 18 */
	NULL, 				/* 19 */
	NULL, 				/* 20 */
	NULL, 				/* 21 */
	NULL, 				/* 22 */
	NULL, 				/* 23 */
	NULL, 				/* 24 */
	"InterBase/apollo",		/* 25 */
	"InterBase/ultrix",		/* 26 */
	"InterBase/vms",		/* 27 */
	"InterBase/sun",		/* 28 */
	"InterBase/OS2", 		/* 29 */
	"InterBase/sun4",		/* 30 */
	"InterBase/hpux800",		/* 31 */
	"InterBase/sun386",		/* 32 */
	"InterBase:ORACLE/vms",		/* 33 */
	"InterBase/mac/aux",		/* 34 */
	"InterBase/ibm/aix",		/* 35 */
	"InterBase/mips/ultrix",	/* 36 */
	"InterBase/xenix",		/* 37 */
	"InterBase/AViiON",		/* 38 */
	"InterBase/hp/mpexl",		/* 39 */
	"InterBase/hp/ux300",		/* 40 */
	"InterBase/sgi",		/* 41 */
	"InterBase/sco/unix",		/* 42 */
	"InterBase/Cray",		/* 43 */
	"InterBase/imp",		/* 44 */
	"InterBase/delta",		/* 45 */
	"InterBase/NeXT",		/* 46 */
	"InterBase/DOS",		/* 47 */
	"InterBase/m88k",		/* 48 */
	"InterBase/UNIXWARE",		/* 49 */
	"InterBase/x86/Windows NT",	/* 50 */
	"InterBase/epson",		/* 51 */
	"InterBase/DEC/OSF",		/* 52 */
	"InterBase/Alpha/OpenVMS",	/* 53 */
        "InterBase/NetWare",            /* 54 */
	"InterBase/Windows",		/* 55 */
	"InterBase/NCR3000",		/* 56 */
	"InterBase/PPC/Windows NT",	/* 57 */
	"InterBase/DG_X86",		/* 58 */
	"InterBase/SCO_SV Intel",	/* 59 */ /* 5.5 SCO Port */ 
        "InterBase/linux Intel"         /* 60 */
	};


#ifdef SHLIB_DEFS
#define strlen		(*_libgds_strlen)
#define _iob		(*_libgds__iob)
#define ib_printf		(*_libgds_printf)
#define ib_fopen		(*_libgds_fopen)
#define ib_fclose		(*_libgds_fclose)
#define getenv		(*_libgds_getenv)
#define ib_fputc		(*_libgds_fputc)
#define mktemp		(*_libgds_mktemp)
#define unlink		(*_libgds_unlink)
#define statistics	(*_libgds_stat)
#define sprintf		(*_libgds_sprintf)
#define system		(*_libgds_system)
#define ib_fgetc		(*_libgds_fgetc)
#define ib_fgets		(*_libgds_fgets)
#define strcat		(*_libgds_strcat)
#define strcpy		(*_libgds_strcpy)
#define strncpy		(*_libgds_strncpy)
#define ib_fprintf		(*_libgds_fprintf)

extern int		strlen();
extern IB_FILE		_iob [];
extern int		ib_printf();
extern IB_FILE		*ib_fopen();
extern int		ib_fclose();
extern SCHAR		*getenv();
extern int		ib_fputc();
extern SCHAR		*mktemp();
extern int		unlink();
extern int		statistics();
extern int		sprintf();
extern int		system();
extern int		ib_fgetc();
extern SCHAR		*ib_fgets();
extern SCHAR		*strcat();
extern SCHAR		*strcpy();
extern SCHAR		*strncpy();
extern int		ib_fprintf();
#endif

#ifdef VMS
STATUS API_ROUTINE gds__attach_database_d (
    STATUS	*user_status,
    struct dsc$descriptor_s	*file_name,
    void	**handle,
    SSHORT	dpb_length,
    SCHAR	*dpb,
    SSHORT	db_type)
{
/**************************************
 *
 *	g d s _ $ a t t a c h _ d a t a b a s e _ d
 *
 **************************************
 *
 * Functional description
 *	An attach database for COBOL to call
 *
 **************************************/

return gds__attach_database (user_status, file_name->dsc$w_length, file_name->dsc$a_pointer,
    handle, dpb_length, dpb, db_type);
}
#endif

int API_ROUTINE gds__blob_size (
    SLONG	*b,
    SLONG	*size,
    SLONG	*seg_count,
    SLONG	*max_seg)
{
/**************************************
 *
 *	g d s _ $ b l o b _ s i z e 
 *
 **************************************
 *
 * Functional description
 *	Get the size, number of segments, and max
 *	segment length of a blob.  Return true
 *	if it happens to succeed.
 *
 **************************************/
STATUS	status_vector [20];
SLONG	n;
SSHORT	l;
SCHAR	*p, item, buffer [64];

if (gds__blob_info (status_vector, 
	GDS_VAL (b), 
	sizeof (blob_items), 
	blob_items, 
	sizeof (buffer), 
	buffer))
    {
    gds__print_status (status_vector); 
    return FALSE;
    }

p = buffer;

while ((item = *p++) != gds__info_end)
    {
    l = gds__vax_integer (p, 2);
    p += 2;
    n = gds__vax_integer (p, l);
    p += l;
    switch (item)
	{
	case gds__info_blob_max_segment:
	    if (max_seg)
		*max_seg = n;
	    break;

	case gds__info_blob_num_segments:
	    if (seg_count)
		*seg_count = n;
	    break;

	case gds__info_blob_total_length:
	    if (size)
		*size = n;
	    break;

	default:
	    return FALSE;
	}
    }

return TRUE;
}

void API_ROUTINE_VARARG isc_expand_dpb (
    SCHAR	**dpb,
    SSHORT	*dpb_size,
    ...)
{
/**************************************
 *
 *	i s c _ e x p a n d _ d p b
 *
 **************************************
 *
 * Functional description
 *	Extend a database parameter block dynamically
 *	to include runtime info.  Generated 
 *	by gpre to provide host variable support for 
 *	READY statement	options.
 *	This expects the list of variable args
 *	to be zero terminated.
 *
 *	Note: dpb_size is signed short only for compatibility
 *	with other calls (isc_attach_database) that take a dpb length.
 *
 **************************************/
SSHORT		length, new_dpb_length;
UCHAR		*new_dpb, *p, *q;
va_list		args;
USHORT		type;

/* calculate length of database parameter block, 
   setting initial length to include version */

if (!*dpb || !(new_dpb_length = *dpb_size))
    new_dpb_length = 1;

VA_START (args, dpb_size);

while (type = va_arg (args, int))
    switch (type)
	{
	case gds__dpb_user_name:
	case gds__dpb_password:
	case isc_dpb_sql_role_name:
	case gds__dpb_lc_messages:
	case gds__dpb_lc_ctype:
	case isc_dpb_reserved:
	    if (p = (UCHAR*) va_arg (args, SCHAR*))
		{
		length = strlen (p);
		new_dpb_length += 2 + length;
		}
	    break;

	default:
	    (void) va_arg (args, int);
	    break;
	}

/* if items have been added, allocate space
   for the new dpb and copy the old one over */

if (new_dpb_length > *dpb_size)
    {
    /* Note: gds__free done by GPRE generated code */

    p = new_dpb = gds__alloc ((SLONG) (sizeof(UCHAR) * new_dpb_length));
    /* FREE: done by client process in GPRE generated code */
    if (!new_dpb)	/* NOMEM: don't trash existing dpb */
	{
	DEV_REPORT ("isc_extend_dpb: out of memory");
	return;		/* NOMEM: not really handled */
	}

    q = (UCHAR*) *dpb;
    for (length = *dpb_size; length; length--)
	*p++ = *q++;

    }
else
    {
    new_dpb = (UCHAR*) *dpb;
    p = new_dpb + *dpb_size;
    }

if (!*dpb_size)
    *p++ = gds__dpb_version1;  

/* copy in the new runtime items */

VA_START (args, dpb_size);

while (type = va_arg (args, int))
    switch (type)
	{
	case gds__dpb_user_name:
	case gds__dpb_password:
	case isc_dpb_sql_role_name:
	case gds__dpb_lc_messages:
	case gds__dpb_lc_ctype:
	case isc_dpb_reserved:
	    if (q = (UCHAR*) va_arg (args, SCHAR*))
		{
		length = strlen (q);
		*p++ = type;
		*p++ = length;
		while (length--)
		    *p++ = *q++;
		}
	    break;

	default:
	    (void) va_arg (args, int);
	    break;
	}

*dpb_size = p - new_dpb;
*dpb = (SCHAR*) new_dpb;
}

int API_ROUTINE isc_modify_dpb (
    SCHAR	**dpb,
    SSHORT	*dpb_size,
    USHORT	type,
    SCHAR	*str,
    SSHORT	str_len
    )
{
/**************************************
 *
 *	i s c _ e x p a n d _ d p b
 *
 **************************************
 *
 * Functional description
 *	Extend a database parameter block dynamically
 *	to include runtime info.  Generated 
 *	by gpre to provide host variable support for 
 *	READY statement	options.
 *	This expects one arg at a time. 
 *      the length of the string is passed by the caller and hence
 * 	is not expected to be null terminated.
 * 	this call is a variation of isc_expand_dpb without a variable
 * 	arg parameters.
 * 	Instead, this function is called recursively
 *	Alternatively, this can have a parameter list with all possible
 *	parameters either nulled or with proper value and type.
 *
 *  	**** This can be modified to be so at a later date, making sure
 *	**** all callers follow the same convention 
 * 
 *	Note: dpb_size is signed short only for compatibility
 *	with other calls (isc_attach_database) that take a dpb length.
 *
 **************************************/
SSHORT		length, new_dpb_length;
UCHAR		*new_dpb, *p, *q;

/* calculate length of database parameter block, 
   setting initial length to include version */

if (!*dpb || !(new_dpb_length = *dpb_size))
    new_dpb_length = 1;

   switch (type)
       {
	case gds__dpb_user_name:
	case gds__dpb_password:
	case isc_dpb_sql_role_name:
	case gds__dpb_lc_messages:
	case gds__dpb_lc_ctype:
	case isc_dpb_reserved:
	   new_dpb_length += 2 + str_len;
	   break;

	default:
	    return FAILURE;
	}

/* if items have been added, allocate space
   for the new dpb and copy the old one over */

if (new_dpb_length > *dpb_size)
    {
    /* Note: gds__free done by GPRE generated code */

    p = new_dpb = gds__alloc ((SLONG) (sizeof(UCHAR) * new_dpb_length));
    /* FREE: done by client process in GPRE generated code */
    if (!new_dpb)	/* NOMEM: don't trash existing dpb */
	{
	DEV_REPORT ("isc_extend_dpb: out of memory");
	return FAILURE;		/* NOMEM: not really handled */
	}

    q = (UCHAR*) *dpb;
    for (length = *dpb_size; length; length--)
	*p++ = *q++;

    }
else
    {
    new_dpb = (UCHAR*) *dpb;
    p = new_dpb + *dpb_size;
    }

if (!*dpb_size)
    *p++ = gds__dpb_version1;  

/* copy in the new runtime items */

    switch (type)
	{
	case gds__dpb_user_name:
	case gds__dpb_password:
	case isc_dpb_sql_role_name:
	case gds__dpb_lc_messages:
	case gds__dpb_lc_ctype:
	case isc_dpb_reserved:
	    if (q = (UCHAR*) str)
		{
		length = str_len;
		*p++ = type;
		*p++ = length;
		while (length--)
		    *p++ = *q++;
		}
	    break;

	default:
	    return FAILURE;
	}

*dpb_size = p - new_dpb;
*dpb = (SCHAR*) new_dpb;
return SUCCESS;
}

#ifdef APOLLO
int API_ROUTINE gds__edit (
    TEXT	*file_name,
    USHORT	type)
{
/**************************************
 *
 *	g d s _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 **************************************/
TEXT			spare_name_buf [32]; 
USHORT			short_length;
pad_$type_t		edit_type;
pad_$window_desc_t	window;
stream_$id_t		stream;
status_$t		status, alt_status;

/* If this isn't the display manager, don't try to edit */

if (gds__termtype() != TERM_apollo_dm)
    return edit_dumb (file_name, type);

sprintf (spare_name_buf, "%s.bak", file_name);

/* Create an edit window for the file.  Then wait for the editting
   session to end.  If the wait failed, the user killed the edit */

window.top = window.left = window.width = window.height = 0;
edit_type = type ? pad_$edit : pad_$read_edit;
short_length = strlen (file_name);
pad_$create_window (file_name, short_length, edit_type, (SSHORT) 1, 
	window, &stream, &status);

if (status.all)
    {
    error_$print (status);
    return FALSE;
    }

if (type)
    pad_$edit_wait (stream, &status);

stream_$close (stream, alt_status);
unlink (spare_name_buf);

if (status.all)
    {
    if (status.all != pad_$edit_quit)
        error_$print (status);
    return FALSE;
    }

return type;
}
#endif
 
#ifdef GDS_EDIT
int API_ROUTINE GDS_EDIT (
    TEXT	*file_name,
    USHORT	type)
{
/**************************************
 *
 *	g d s _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit a file.
 *
 **************************************/
TEXT		*editor, buffer [256];
struct stat	before, after;

#ifndef WIN_NT
if (!(editor = getenv ("VISUAL")) &&
    !(editor = getenv ("EDITOR")))
    editor = "vi";
#else
if (!(editor = getenv ("EDITOR")))
    editor = "mep";
#endif

statistics (file_name, &before);
#ifdef APOLLO
sprintf (buffer, "%s %s%s", editor, (*file_name == '`') ? "\\" : "", file_name);
#else
sprintf (buffer, "%s %s", editor, file_name);
#endif

#ifndef WINDOWS_ONLY
system (buffer);
#endif

statistics (file_name, &after);

return (before.st_mtime != after.st_mtime || before.st_size != after.st_size);
}
#endif

#ifdef mpexl
int API_ROUTINE gds__edit (
    TEXT	*file_name,
    USHORT	type)
{
/**************************************
 *
 *	g d s _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit a file.
 *
 **************************************/
TEXT	*editor, buffer [64];
SLONG	start_time, end_time;
SSHORT	start_date, end_date, cmderror, parmnum;

if (!(editor = getenv ("ISC_EDIT")))
    {
    ib_printf ("MPE/XL variable ISC_EDIT has not been used to define an editor.\n");
    return 0;
    }

stat (file_name, &start_time, &start_date);
sprintf (buffer, "%s %s\015", editor, file_name);
HPCICOMMAND (buffer, &cmderror, &parmnum, 2);
stat (file_name, &end_time, &end_date);

return (start_time != end_time || start_date != end_date);
}
#endif

#ifdef VMS
int API_ROUTINE gds__edit (
    TEXT	*file_name,
    USHORT	type)
{
/**************************************
 *
 *	g d s _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 **************************************/
int	status, (*editor)();
struct dsc$descriptor_s	desc, symbol, image;
struct stat	before, after;

stat (file_name, &before);
ISC_make_desc (file_name, &desc, 0);
ISC_make_desc (EDT_SYMBOL, &symbol, 0);
ISC_make_desc (EDT_IMAGE, &image, 0);

lib$find_image_symbol (&image, &symbol, &editor);
status = (*editor) (&desc, &desc);
stat (file_name, &after);

return (before.st_ctime != after.st_ctime ||
    before.st_ino [0] != after.st_ino [0] ||
    before.st_ino [1] != after.st_ino [1] ||
    before.st_ino [2] != after.st_ino [2]);
}
#endif

SLONG API_ROUTINE gds__event_block (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    USHORT	count,
    ...)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Create an initialized event parameter block from a
 *	variable number of input arguments.
 *	Return the size of the block.
 *
 *	Return 0 as the size if the event parameter block cannot be
 *	created for any reason.
 *
 **************************************/
register SCHAR	*p, *q;
SCHAR		*end;
SLONG		length;
va_list		ptr;
USHORT		i;

VA_START (ptr, count);

/* calculate length of event parameter block, 
   setting initial length to include version
   and counts for each argument */

length = 1;
i = GDS_VAL (count);
while (i--)
    {
    q = va_arg (ptr, SCHAR*);
    length += strlen (q) + 5;
    }

p = *event_buffer = (SCHAR*) gds__alloc ((SLONG)(sizeof(SCHAR)*length));
/* FREE: unknown */
if (!*event_buffer)	/* NOMEM: */
    return 0;
*result_buffer = (SCHAR*) gds__alloc ((SLONG)(sizeof(SCHAR)*length));
/* FREE: unknown */
if (!*result_buffer)	/* NOMEM: */
    {
    gds__free (*event_buffer);
    *event_buffer = NULL;
    return 0;
    }

#ifdef DEBUG_GDS_ALLOC
/* I can't find anywhere these items are freed */
/* 1994-October-25 David Schnepper  */
gds_alloc_flag_unfreed ((void *) *event_buffer);
gds_alloc_flag_unfreed ((void *) *result_buffer);
#endif DEBUG_GDS_ALLOC
         
/* initialize the block with event names and counts */

*p++ = 1;  

VA_START (ptr, count);

i = GDS_VAL (count);
while (i--)
    {
    q = va_arg (ptr, SCHAR*);

    /* Strip trailing blanks from string */

    for (end = q + strlen (q); --end >= q && *end == ' ';)
	;
    *p++ = end - q + 1;
    while (q <= end)
	*p++ = *q++;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    }

return p - *event_buffer;
}

USHORT API_ROUTINE gds__event_block_a (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    SSHORT	GDS_VAL (count),
    SCHAR	**name_buffer)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k _ a 
 *
 **************************************
 *
 * Functional description
 *	Create an initialized event parameter block from a
 *	vector of input arguments. (Ada needs this)
 *	Assume all strings are 31 characters long.
 *	Return the size of the block.
 *
 **************************************/
register SCHAR	*p, *q;
SCHAR		*end, **nb;
SLONG		length;
USHORT		i;

/* calculate length of event parameter block, 
   setting initial length to include version
   and counts for each argument */

i = GDS_VAL (count);
nb = name_buffer;
length = 0;
while (i--)
    {
    q =  *nb++;

    /* Strip trailing blanks from string */

    for (end = q + 31; --end >= q && *end == ' ';)
	;
    length += end - q + 1 + 5;
    }

i = GDS_VAL (count);
p = *event_buffer = (SCHAR*) gds__alloc ((SLONG)(sizeof(SCHAR)*length));
/* FREE: unknown */
if (!*event_buffer)	/* NOMEM: */
    return 0;
*result_buffer = (SCHAR*) gds__alloc ((SLONG)(sizeof(SCHAR)*length));
/* FREE: unknown */
if (!*result_buffer)	/* NOMEM: */
    {
    gds__free (*event_buffer);
    *event_buffer = NULL;
    return 0;
    }

#ifdef DEBUG_GDS_ALLOC
/* I can't find anywhere these items are freed */
/* 1994-October-25 David Schnepper  */
gds_alloc_flag_unfreed ((void *) *event_buffer);
gds_alloc_flag_unfreed ((void *) *result_buffer);
#endif DEBUG_GDS_ALLOC

*p++ = 1;  
                
nb = name_buffer;

while (i--)
    {
    q =  *nb++;

    /* Strip trailing blanks from string */

    for (end = q + 31; --end >= q && *end == ' ';)
	;
    *p++ = end - q + 1;
    while (q <= end)
	*p++ = *q++;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    }

return (p - *event_buffer);
}

void API_ROUTINE gds__event_block_s (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    SSHORT	count,
    SCHAR	**name_buffer,
    SSHORT	*return_count)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ b l o c k _ s
 *
 **************************************
 *
 * Functional description
 *	THIS IS THE COBOL VERSION of gds__event_block_a for Cobols
 *	that don't permit return values.
 *
 **************************************/

*return_count = gds__event_block_a (event_buffer, result_buffer, count, name_buffer);
}

void API_ROUTINE gds__event_counts (
    ULONG	*result_vector,
    SSHORT	GDS_VAL (buffer_length),
    SCHAR	*event_buffer,
    SCHAR	*result_buffer)
{
/**************************************
 *
 *	g d s _ $ e v e n t _ c o u n t s
 *
 **************************************
 *
 * Functional description
 *	Get the delta between two events in an event
 *	parameter block.  Used to update gds__events
 *	for GPRE support of events.
 *
 **************************************/
register SCHAR	*p, *q, *end;
USHORT		i, length;
ULONG		*vec; 
ULONG		initial_count, new_count;

vec = result_vector;
p = event_buffer;
q = result_buffer;
length = GDS_VAL (buffer_length);
end = p + length;

/* analyze the event blocks, getting the delta for each event */

p++; q++;
while (p < end)
    {
    /* skip over the event name */

    i = (USHORT) *p++;
    p += i;
    q += i + 1;

    /* get the change in count */

    initial_count = gds__vax_integer (p, sizeof (SLONG));
    p += sizeof (SLONG);
    new_count = gds__vax_integer (q, sizeof (SLONG));
    q += sizeof (SLONG);
    *vec++ = new_count - initial_count;
    }

/* copy over the result to the initial block to prepare
   for the next call to gds__event_wait */

p = event_buffer;
q = result_buffer;
do *p++ = *q++; while (--length);
}

#ifndef PIPE_CLIENT
void API_ROUTINE gds__map_blobs (
    int		*handle1,
    int		*handle2)
{
/**************************************
 *
 *	g d s _ $ m a p _ b l o b s
 *
 **************************************
 *
 * Functional description
 *	Map an old blob to a new blob.
 *	This call is intended for use by REPLAY,
 *	and is probably not generally useful
 *	for anyone else.
 *
 **************************************/

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM
#ifndef SUPERCLIENT
/* Note: gds__map_blobs is almost like an API call,
   it needs a TDBB structure setup for it in order
   to function properly.  This currently does
   not function.
   1996-Nov-06 David Schnepper  */
deliberate_compile_error++;
BLB_map_blobs (NULL_TDBB, handle1, handle2);
#endif
#endif
}
#endif

#if !(defined REQUESTER || defined NETWARE_386)
void API_ROUTINE gds__set_debug (
    int		GDS_VAL (value))
{
/**************************************
 *
 *	G D S _ S E T _ D E B U G
 *
 **************************************
 *
 * Functional description
 *	Set debugging mode, whatever that may mean.
 *
 **************************************/

#ifdef APOLLO
AMBX_set_debug (GDS_VAL (value));
#endif
}                
#endif

void API_ROUTINE isc_set_login (
    UCHAR	**dpb,
    SSHORT	*dpb_size)
{
/**************************************
 *
 *	i s c _ s e t _ l o g i n
 *
 **************************************
 *
 * Functional description
 *	Pickup any ISC_USER and ISC_PASSWORD
 *	environment variables, and stuff them
 *	into the dpb (if there is no user name
 *	or password already referenced).
 *
 **************************************/
TEXT	*username, *password;
UCHAR	*p, *end_dpb;
BOOLEAN	user_seen = FALSE, password_seen = FALSE;
USHORT	l;
int	item;

/* look for the environment variables */

username = getenv ("ISC_USER");
password = getenv ("ISC_PASSWORD");

if (!username && !password)
    return;                     

/* figure out whether the username or 
   password have already been specified */

if (*dpb && *dpb_size)
    for (p = *dpb, end_dpb = p + *dpb_size; p < end_dpb;)
        {
	item = *p++;

	if (item == gds__dpb_version1)
	    continue;

        switch (item)
  	    {
	    case gds__dpb_sys_user_name:
	    case gds__dpb_user_name:
	        user_seen = TRUE;
	        break;

	    case gds__dpb_password:
	    case gds__dpb_password_enc:
	        password_seen = TRUE;
	        break;
	    }

	/* get the length and increment past the parameter. */
        l = *p++;
        p += l;
        }

if (username && !user_seen)
    {
    if (password && !password_seen)
        isc_expand_dpb (dpb, dpb_size, gds__dpb_user_name, username, gds__dpb_password, password, 0);
    else
        isc_expand_dpb (dpb, dpb_size, gds__dpb_user_name, username, 0);
    }
else if (password && !password_seen)
    isc_expand_dpb (dpb, dpb_size, gds__dpb_password, password, 0);
}

BOOLEAN API_ROUTINE isc_set_path (
    TEXT	*file_name,
    USHORT	file_length,
    TEXT	*expanded_name)
{
/**************************************
 *
 *	i s c _ s e t _ p a t h
 *
 **************************************
 *
 * Functional description
 *	Set a prefix to a filename based on 
 *	the ISC_PATH user variable.
 *
 **************************************/
TEXT	*pathname, *p;

/* look for the environment variables to tack 
   onto the beginning of the database path */

if (!(pathname = getenv ("ISC_PATH")))
    return FALSE;

if (!file_length)
    file_length = strlen (file_name);
file_name [file_length] = 0;

/* if the file already contains a remote node
   or any path at all forget it */

for (p = file_name; *p; p++)
    if (*p == ':' || *p == '/' || *p == '\\')
	return FALSE;

/* concatenate the strings */

strcpy (expanded_name, pathname);
strcat (expanded_name, file_name);

return TRUE;
}

void API_ROUTINE isc_set_single_user (
    UCHAR	**dpb,
    SSHORT	*dpb_size,
    TEXT	*single_user)
{
/****************************************
 *
 *      i s c _ s e t _ s i n g l e _ u s e r
 *
 ****************************************
 *
 * Functional description
 *      Stuff the single_user flag into the dpb
 *      if the flag doesn't already exist in the dpb.
 *
 ****************************************/

UCHAR	*p, *end_dpb;
BOOLEAN	single_user_seen = FALSE;
USHORT	l;
int	item;

/* Discover if single user access has already been specified */

if ((*dpb) && (*dpb_size))
    for ( p = *dpb, end_dpb = p + *dpb_size; p < end_dpb; )
	{

	item = *p++;

	if (item == gds__dpb_version1)
	    continue;

	switch (item)
	    {

	    case isc_dpb_reserved :

		single_user_seen = TRUE;
		break;

	    }

/* Get the length and increment past the parameter. */

	l = *p++;
	p += l;

	}

if (!single_user_seen)
    isc_expand_dpb (dpb, dpb_size, isc_dpb_reserved, single_user, 0);

}

int API_ROUTINE gds__version (
    void	**handle,
    void	(*routine)(),
    void	*user_arg)
{
/**************************************
 *
 *	g d s _ $ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Obtain and print information about a database.
 *
 **************************************/
STATUS	status_vector [20], count;
USHORT	n, buf_len, len, implementation, class, ods_version, ods_minor_version;
UCHAR	item, l, *buf, buffer [256], *p;
TEXT	*versions, *implementations,
	*class_string, *implementation_string, s [128];
BOOLEAN	redo;

if (!routine)
    {
    routine = (void (*)()) ib_printf;
    user_arg = "\t%s\n";
    }

buf = buffer;
buf_len = sizeof (buffer);

do
    {
    if (isc_database_info (status_vector, 
	    handle,
	    sizeof (info),
	    info,
	    buf_len,
	    buf))
	{
	if (buf != buffer)
	    gds__free ((SLONG*) buf);
	return FAILURE;
	}

    p = buf;
    redo = FALSE;

    while (!redo && *p != gds__info_end && p < buf + buf_len)
	{
	item = *p++;
	len = gds__vax_integer (p, 2);
	p += 2;
	switch (item)
	    {
	    case gds__info_version:
		versions = (TEXT*) p;
		break;

	    case gds__info_implementation:
		implementations = (TEXT*) p;
		break;
	
	    case gds__info_truncated:
		redo = TRUE;
		break;
	
	    default:
		if (buf != buffer)
		    gds__free ((SLONG*) buf);
		return FAILURE;
	    }
	p += len;
	}

    /* Our buffer wasn't large enough to hold all the information,
     * make a larger one and try again.
     */
    if (redo)
	{
	if (buf != buffer)
	    gds__free ((SLONG*) buf);
	buf_len += 1024;
	buf = (UCHAR*) gds__alloc ((SLONG)(sizeof(UCHAR)* buf_len));
	/* FREE: freed within this module */
	if (!buf)	/* NOMEM: */
	    return FAILURE;
	}
    } while (redo);

count = MIN (*versions, *implementations);
++versions;
++implementations;

while (--count >= 0)
     {
     implementation = *implementations++;
     class = *implementations++;
     l = *versions++;
     if (implementation >= ((sizeof (impl_implementation) / sizeof (impl_implementation [0]))) ||
        !(implementation_string = impl_implementation [implementation]))
	implementation_string = "**unknown**";
     if (class >= ((sizeof (impl_class) / sizeof (impl_class [0]))) ||
        !(class_string = impl_class [class]))
	class_string = "**unknown**";
     sprintf (s, "%s (%s), version \"%.*s\"", 
	implementation_string, class_string, l, versions);
    (*routine) (user_arg, s);
    versions += l;
    }
 
if (buf != buffer)
    gds__free ((SLONG*) buf);

if (get_ods_version (handle, &ods_version, &ods_minor_version) == FAILURE)
    return FAILURE;
    
sprintf (s, "on disk structure version %d.%d", ods_version, ods_minor_version); 
(*routine) (user_arg, s);

return SUCCESS;
}

void API_ROUTINE isc_format_implementation (
    USHORT	implementation,
    USHORT	ibuflen,
    TEXT	*ibuf,
    USHORT	class,
    USHORT	cbuflen,
    TEXT 	*cbuf)
{
/**************************************
 *
 *	i s c _ f o r m a t _ i m p l e m e n t a t i o n
 *
 **************************************
 *
 * Functional description
 *	Convert the implementation and class codes to strings
 * 	by looking up their values in the internal tables.
 *
 **************************************/
int	len;

if (ibuflen > 0)
    {   
    if ( implementation >= ((sizeof (impl_implementation) / 
    			 sizeof (impl_implementation [0]))) || 
         !(impl_implementation [implementation]) )
        {
        strncpy (ibuf, "**unknown**", ibuflen - 1);
        ibuf[MIN (11, ibuflen - 1)] = '\0';
        }
    else
        {
        strncpy (ibuf, impl_implementation[implementation], ibuflen - 1);
	len = strlen (impl_implementation [implementation]);
        ibuf [ MIN (len, ibuflen - 1) ] = '\0';
        }
    }

if (cbuflen > 0)
    {
    if ( class >= ((sizeof (impl_class) / sizeof (impl_class [0]))) ||
         !(impl_class [class]) )
        {
        strncpy (cbuf, "**unknown**", cbuflen - 1);
        ibuf[MIN (11, cbuflen - 1)] = '\0';
        }
    else
        {
        strncpy (cbuf, impl_class [class], cbuflen - 1);
	len = strlen (impl_class [class]);
        ibuf [ MIN (len, cbuflen - 1) ] = '\0';
        }
    }

}

U_IPTR API_ROUTINE isc_baddress (
    SCHAR	*object)
{
/**************************************
 *
 *        i s c _ b a d d r e s s
 *
 **************************************
 *
 * Functional description
 *      Return the address of whatever is passed in
 *
 **************************************/

return (U_IPTR) object;  
}

void API_ROUTINE isc_baddress_s (
    SCHAR	*object,
    U_IPTR	*address)
{
/**************************************
 *
 *        i s c _ b a d d r e s s _ s
 *
 **************************************
 *
 * Functional description
 *      Return the address of whatever is passed in
 *
 **************************************/

*address = (U_IPTR) object;  
}

#ifdef VMS
void API_ROUTINE gds__wake_init (void)
{
/**************************************
 *
 *	g d s _ $ w a k e _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Set up to be awakened by another process thru a blocking AST.
 *
 **************************************/

ISC_wake_init();
}
#endif

int API_ROUTINE BLOB_close (
    BSTREAM	*bstream)
{
/**************************************
 *
 *	B L O B _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Close a blob stream.
 *
 **************************************/
STATUS	status_vector [20];
USHORT	l;

if (!bstream->bstr_blob)
    return FALSE;

if (bstream->bstr_mode & BSTR_output)
    {
    l = (bstream->bstr_ptr - bstream->bstr_buffer);
    if (l > 0)
	if (gds__put_segment (status_vector,
	    GDS_REF (bstream->bstr_blob),
	    l,
	    GDS_VAL (bstream->bstr_buffer)))
	return FALSE;
    }

gds__close_blob (status_vector, GDS_REF (bstream->bstr_blob));

if (bstream->bstr_mode & BSTR_alloc)
    gds__free (bstream->bstr_buffer);

gds__free (bstream);

return TRUE;
}

int API_ROUTINE blob__display (
    SLONG	blob_id [2],
    void	**database,
    void	**transaction,
    TEXT	*field_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	b l o b _ $ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	PASCAL callable version of EDIT_blob.
 *
 **************************************/
TEXT	*p, *q, temp[32];
USHORT	l;

if ((l = *name_length) != NULL)
    {
    p = temp;
    q = field_name;
    do *p++ = *q++; while (--l);
    *p = 0;
    }     

return BLOB_display (blob_id, *database, *transaction, temp);
}

int API_ROUTINE BLOB_display (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    TEXT	*field_name)
{
/**************************************
 *
 *	B L O B _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to read the
 *	window. 
 *
 **************************************/

/* On VMS use the system library routines to do the output */

#ifdef VMS
return display (blob_id, database, transaction);
#else

/* On Apollo, just do any edit, no update */

#ifdef APOLLO
if (gds__termtype() == TERM_apollo_dm)
    return edit (blob_id, database, transaction, FALSE, field_name);
#endif

/* On UNIX, just dump the file to ib_stdout */

return dump (blob_id, database, transaction, ib_stdout);

#endif
}

int API_ROUTINE blob__dump (
    SLONG	blob_id [2],
    void	**database,
    void	**transaction,
    TEXT	*file_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	b l o b _ $ d u m p
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable dump
 *	into an internal dump call.
 *
 **************************************/
TEXT	*p, *q, temp[129];
USHORT	l;

if ((l = *name_length) != NULL)
    {
    p = temp;
    q = file_name;
    do *p++ = *q++; while (--l);
    *p = 0;
    }     

return BLOB_dump (blob_id, *database, *transaction, temp);
}

int API_ROUTINE BLOB_text_dump (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    SCHAR	*file_name)
{
/**************************************
 *
 *	B L O B _ t e x t _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *      This call does CR/LF translation on NT.
 *
 **************************************/
IB_FILE			*file;
int			ret;

if (!(file = ib_fopen (file_name, FOPEN_WRITE_TYPE_TEXT)))
    return FALSE;

ret = dump (blob_id, database, transaction, file);
ib_fclose (file);

return ret;
}

int API_ROUTINE BLOB_dump (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    SCHAR	*file_name)
{
/**************************************
 *
 *	B L O B _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *
 **************************************/
IB_FILE			*file;
int			ret;

if (!(file = ib_fopen (file_name, FOPEN_WRITE_TYPE)))
    return FALSE;

ret = dump (blob_id, database, transaction, file);
ib_fclose (file);

return ret;
}

int API_ROUTINE blob__edit (
    SLONG	blob_id [2],
    void	**database,
    void	**transaction,
    TEXT	*field_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	b l o b _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable edit
 *	into an internal edit call.
 *
 **************************************/
TEXT	*p, *q, temp[32];
USHORT	l;

if ((l = *name_length) != NULL)
    {
    p = temp;
    q = field_name;
    do *p++ = *q++; while (--l);
    *p = 0;
    }     

return BLOB_edit (blob_id, *database, *transaction, temp);
}

int API_ROUTINE BLOB_edit (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    SCHAR	*field_name)
{
/**************************************
 *
 *	B L O B _ e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 **************************************/

return edit (blob_id, database, transaction, TRUE, field_name);
}

int API_ROUTINE BLOB_get (
    BSTREAM	*bstream)
{
/**************************************
 *
 *	B L O B _ g e t
 *
 **************************************
 *
 * Functional description
 *	Return the next byte of a blob.  If the blob is exhausted, return
 *	EOF.
 *
 **************************************/
STATUS	status_vector [20];

if (!bstream->bstr_buffer)
    return EOF;

while (1)
    {
    if (--bstream->bstr_cnt >= 0)
	return *bstream->bstr_ptr++ & 0377;

    gds__get_segment (status_vector,
	    GDS_REF (bstream->bstr_blob),
	    GDS_REF (bstream->bstr_cnt),
	    bstream->bstr_length,
	    GDS_VAL (bstream->bstr_buffer));
    if (status_vector [1] && status_vector [1] != gds__segment)
	{
	bstream->bstr_ptr = 0;
	bstream->bstr_cnt = 0;
	if (status_vector [1] != gds__segstr_eof)
		gds__print_status (status_vector);
	return EOF;
	}
    bstream->bstr_ptr = bstream->bstr_buffer;
    }
}

int API_ROUTINE blob__load (
    SLONG	blob_id [2],
    void	**database,
    void	**transaction,
    TEXT	*file_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	b l o b _ $ l o a d
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable load
 *	into an internal load call.
 *
 **************************************/
TEXT	*p, *q, temp[129];
USHORT	l;

if ((l = *name_length) != NULL)
    {
    p = temp;
    q = file_name;
    do *p++ = *q++; while (--l);
    *p = 0;
    }     

return BLOB_load (blob_id, *database, *transaction, temp);
}

int API_ROUTINE BLOB_text_load (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    TEXT	*file_name)
{
/**************************************
 *
 *	B L O B _ t e x t _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Load a  blob with the contents of a file.  
 *      This call does CR/LF translation on NT.
 *      Return TRUE is successful.
 *
 **************************************/
IB_FILE	*file;
int     ret;

if (!(file = ib_fopen (file_name, FOPEN_READ_TYPE_TEXT)))
    return FALSE;

ret = load (blob_id, database, transaction, file);

ib_fclose (file);
 
return ret;
}

int API_ROUTINE BLOB_load (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    TEXT	*file_name)
{
/**************************************
 *
 *	B L O B _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Load a blob with the contents of a file.  Return TRUE is successful.
 *
 **************************************/
IB_FILE	*file;
int     ret;

if (!(file = ib_fopen (file_name, FOPEN_READ_TYPE)))
    return FALSE;

ret = load (blob_id, database, transaction, file);

ib_fclose (file);
 
return ret;
}

BSTREAM * API_ROUTINE Bopen (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    SCHAR	*mode)
{
/**************************************
 *
 *	B o p e n
 *
 **************************************
 *
 * Functional description
 *	Initialize a blob-stream block.
 *
 **************************************/
SLONG	*blob; 
STATUS	status_vector[20];
BSTREAM	*bstream;
USHORT	bpb_length;
UCHAR	*bpb;

bpb_length = 0;
bpb = NULL;

blob = NULL;

if (*mode == 'w' || *mode == 'W')
    {
    if (gds__create_blob2 (status_vector, 
	GDS_REF (database), 
	GDS_REF (transaction), 
	GDS_REF (blob), 
	GDS_VAL (blob_id),
	bpb_length,
	bpb))
   return NULL;
    }
else if (*mode == 'r' || *mode == 'R')
    {
    if (gds__open_blob2 (status_vector, 
	GDS_REF (database), 
	GDS_REF (transaction), 
	GDS_REF (blob), 
	GDS_VAL (blob_id),
	bpb_length,
	bpb))
    return NULL;
    }
else
    return NULL; 

bstream = BLOB_open ((int*) blob, (SCHAR*) 0, 0);

if (*mode == 'w' || *mode == 'W')
    {
    bstream->bstr_mode |= BSTR_output;
    bstream->bstr_cnt = bstream->bstr_length;
    bstream->bstr_ptr = bstream->bstr_buffer;
    }
else
    {
    bstream->bstr_cnt = 0;
    bstream->bstr_mode |= BSTR_input; 
    }

return bstream;
}

BSTREAM * API_ROUTINE BLOB_open (
    void	*blob,
    SCHAR	*buffer,
    int		length)
{
/**************************************
 *
 *	B L O B _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Initialize a blob-stream block.
 *
 **************************************/
BSTREAM	*bstream;

if (!blob)
    return NULL;

bstream = (BSTREAM*) gds__alloc ((SLONG) sizeof (BSTREAM));
/* FREE: This structure is freed by BLOB_close */
if (!bstream)	/* NOMEM: */
    return NULL;

#ifdef DEBUG_GDS_ALLOC
/* This structure is handed to the user process, we depend on the client
 * to call BLOB_close() for it to be freed.
 */
gds_alloc_flag_unfreed ((void *)bstream);
#endif

bstream->bstr_blob = blob;
bstream->bstr_length = (length) ? length : 512;
bstream->bstr_mode = 0;
bstream->bstr_cnt = 0;
bstream->bstr_ptr = 0;

if (!(bstream->bstr_buffer = buffer))
    {
    bstream->bstr_buffer = (SCHAR*) gds__alloc ((SLONG)(sizeof(SCHAR)*bstream->bstr_length));
    /* FREE: This structure is freed in BLOB_close() */
    if (!bstream->bstr_buffer)	/* NOMEM: */
	{
	gds__free (bstream);
	return NULL;
	}
#ifdef DEBUG_GDS_ALLOC
    /* This structure is handed to the user process, we depend on the client
     * to call BLOB_close() for it to be freed.
     */
    gds_alloc_flag_unfreed ((void *)bstream->bstr_buffer);
#endif

    bstream->bstr_mode |= BSTR_alloc;
    } 

return bstream;
}

int API_ROUTINE BLOB_put (
   SCHAR	x,
   BSTREAM	*bstream)
{
/**************************************
 *
 *	B L O B _ p u t
 *
 **************************************
 *
 * Functional description
 *	Write a segment to a blob. First
 *	stick in the final character, then
 *	compute the length and send off the
 *	segment.  Finally, set up the buffer
 *	block and retun TRUE if all is well. 
 *
 **************************************/
STATUS	status_vector [20];
USHORT	l;

if (!bstream->bstr_buffer)
    return FALSE;

*bstream->bstr_ptr++ = (x & 0377);
l = (bstream->bstr_ptr - bstream->bstr_buffer);    
if (gds__put_segment (status_vector,
	GDS_REF (bstream->bstr_blob),
	l,
	GDS_VAL (bstream->bstr_buffer)))
    {
    return FALSE;
    }
bstream->bstr_cnt = bstream->bstr_length;
bstream->bstr_ptr = bstream->bstr_buffer;
return TRUE;
}

#ifdef VMS
static display (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction)
{
/**************************************
 *
 *	d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Display a file on VMS
 *
 **************************************/
SCHAR			buffer [256], *p;
SSHORT			short_length, l;
STATUS			status_vector [20];
int			*blob;
struct dsc$descriptor_s	desc;

/* Open the blob.  If it failed, what the hell -- just return failure */

blob = NULL;
if (gds__open_blob (status_vector, 
	GDS_REF (database), 
	GDS_REF (transaction), 
	GDS_REF (blob), 
	GDS_VAL (blob_id)))
    {
    gds__print_status (status_vector);
    return FALSE;
    }

/* Copy data from blob to scratch file */

short_length = sizeof (buffer);

for (;;)
    {
    gds__get_segment (status_vector, 
	GDS_REF (blob), 
	GDS_REF (l), 
	short_length, 
	buffer);
    if (status_vector [1] && status_vector [1] != gds__segment)
	{
	if (status_vector [1] != gds__segstr_eof)
		gds__print_status (status_vector);
	break;
	}
    buffer[l] = 0;
    ISC_make_desc (buffer, &desc, 0);
    lib$put_output (&desc);
    }

/* Close the blob */

gds__close_blob (status_vector, GDS_REF (blob));

return TRUE;
}
#endif	VMS

static int dump (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    IB_FILE	*file)
{
/**************************************
 *
 *	d u m p
 *
 **************************************
 *
 * Functional description
 *	Dump a blob into a file.
 *
 **************************************/
SCHAR			buffer [256], *p;
SSHORT			short_length, l;
STATUS			status_vector [20];
int			*blob;
USHORT			bpb_length;
UCHAR			bpb2 [20], *r, *bpb;

bpb_length = 0;
bpb = NULL;

/* Open the blob.  If it failed, what the hell -- just return failure */

blob = NULL;
if (gds__open_blob2 (status_vector, 
	GDS_REF (database), 
	GDS_REF (transaction), 
	GDS_REF (blob), 
	GDS_VAL (blob_id),
	bpb_length,
	bpb))
    {
    gds__print_status (status_vector);
    return FALSE;
    }

/* Copy data from blob to scratch file */

short_length = sizeof (buffer);

for (;;)
    {
    gds__get_segment (status_vector, 
	GDS_REF (blob), 
	GDS_REF (l), 
	short_length, 
	buffer);
    if (status_vector [1] && status_vector [1] != gds__segment)
	{
	if (status_vector [1] != gds__segstr_eof)
		gds__print_status (status_vector);
	break;
	}
    p = buffer;
    if (l)
	do ib_fputc (*p++, file); while (--l);
    }

/* Close the blob */

gds__close_blob (status_vector, GDS_REF (blob));

return TRUE;
}

static int edit (
    GDS__QUAD	*blob_id,
    void	*database,
    void	*transaction,
    SSHORT	type,
    SCHAR	*field_name)
{
/**************************************
 *
 *	e d i t
 *
 **************************************
 *
 * Functional description
 *	Open a blob, dump it to a file, allow the user to edit the
 *	window, and dump the data back into a blob.  If the user
 *	bails out, return FALSE, otherwise return TRUE.
 *
 *	If the field name coming in is too big, truncate it.
 *
 **************************************/
TEXT	file_name [50], *p, *q;
#if (defined PC_PLATFORM || defined NETWARE_386 || defined OS2_ONLY || defined WIN_NT)
TEXT	buffer [9];
#else
#ifndef mpexl
TEXT	buffer [25];
#else
TEXT	buffer [3];
#endif
#endif
IB_FILE	*file;

if (!(q = field_name))
    q = "gds_edit";

#ifndef mpexl
for (p = buffer; *q && p < buffer + sizeof (buffer) - 1; q++)
    if (*q == '$')
	*p++ = '_';
    else
	*p++ = LOWER7 (*q);
*p = 0;

sprintf (file_name, "%s.XXX", buffer);
#else
for (p = buffer; *q && p < buffer + sizeof (buffer) - 1; q++)
    if (*q == '$' || *q == '_')
	*p++ = 'a';
    else
	*p++ = LOWER7 (*q);
*p = 0;

sprintf (file_name, "%sXXXXXX", buffer);
#endif

MKTEMP (file_name, "XXXXXXX");
if (!(file = ib_fopen (file_name, FOPEN_WRITE_TYPE)))
    return FALSE;
ib_fclose (file);

if (!(file = ib_fopen (file_name, FOPEN_WRITE_TYPE_TEXT)))
    return FALSE;

if (!dump (blob_id, database, transaction, file))
    {
    ib_fclose(file); 
    unlink (file_name);
    return FALSE;
    }

ib_fclose(file);

if (type = gds__edit (file_name, type)) 
{

    if (!(file = ib_fopen (file_name, FOPEN_READ_TYPE_TEXT)))
	{
	unlink (file_name);
        return FALSE;
	}

    load (blob_id, database, transaction, file);

    ib_fclose(file);

}

unlink (file_name);

return type;
}

static int get_ods_version (
    void	**handle,
    USHORT	*ods_version,
    USHORT	*ods_minor_version)
{
/**************************************
 *
 *	g e t _ o d s _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Obtain information about a on-disk structure (ods) versions
 *	of the database.
 *
 **************************************/
STATUS	status_vector [20];
USHORT	n, l;
UCHAR	item, buffer [16], *p;

isc_database_info (status_vector, 
	handle,
	sizeof (ods_info),
	ods_info,
	sizeof (buffer),
	buffer);

if (status_vector [1])
    return FAILURE;

p = buffer;

while ((item = *p++) != gds__info_end)
    {
    l = gds__vax_integer (p, 2);
    p += 2;
    n = gds__vax_integer (p, l);
    p += l;
    switch (item)
	{
	case gds__info_ods_version:
	    *ods_version = n;
	    break;

	case gds__info_ods_minor_version:
	    *ods_minor_version = n;
	    break;

	default:
	    return FAILURE;
	} 
    }

return SUCCESS;
}


static int load (
    GDS__QUAD   *blob_id,
    void        *database,
    void        *transaction,
    IB_FILE        *file)
{
/**************************************
 *
 *     l o a d  
 *
 **************************************
 *
 * Functional description
 *      Load a blob from a file.
 *
 **************************************/
TEXT    buffer [512], *p, *buffer_end;
SSHORT  l, c;
STATUS  status_vector [20];
int     *blob;

/* Open the blob.  If it failed, what the hell -- just return failure */
 
blob = NULL;
if (gds__create_blob (status_vector, 
        GDS_REF (database), 
        GDS_REF (transaction), 
        GDS_REF (blob), 
        GDS_VAL (blob_id)))
    {
    gds__print_status (status_vector);
    return FALSE;
    }
 
/* Copy data from file to blob.  Make up boundaries at end of of line. */
 
p = buffer;
buffer_end = buffer + sizeof (buffer);
 
for (;;)
    {
    c = ib_fgetc (file);
    if (ib_feof (file))
        break;
    *p++ = c;
    if ((c != '\n')  && p < buffer_end)
        continue;
    l = p - buffer;
    if (gds__put_segment (status_vector, GDS_REF (blob), l, buffer))
        {
        gds__print_status (status_vector);
        gds__close_blob (status_vector, GDS_REF (blob));
        return FALSE;
        }
    p = buffer;
    }
    
if ((l = p - buffer) != NULL)
    if (gds__put_segment (status_vector, GDS_REF (blob), l, buffer))
        {
        gds__print_status (status_vector);
        gds__close_blob (status_vector, GDS_REF (blob));
        return FALSE;
        }
 
gds__close_blob (status_vector, GDS_REF (blob));
 
return TRUE;
}

#ifdef mpexl
static stat (
    TEXT	*file_name,
    SLONG	*mod_time,
    SSHORT	*mod_date)
{
/**************************************
 *
 *	s t a t
 *
 **************************************
 *
 * Functional description
 *	Determine when a file was last modified.
 *
 **************************************/
TEXT	formal [64], *p;
SLONG	filenum, status, domain;

formal [0] = ' ';
for (p = &formal [1]; *p = *file_name++; p++)
    ;
*p = ' ';

domain = DOMAIN_OPEN;
HPFOPEN (&filenum, &status,
    OPN_FORMAL, formal,
    OPN_DOMAIN, &domain,
    ITM_END);
if (!status)
    {
    FFILEINFO ((SSHORT) filenum,
	INF_MOD_TIME, mod_time,
	INF_MOD_DATE, mod_date,
	ITM_END);
    status = (ccode() == CCE) ? 0 : 1;
    FCLOSE ((SSHORT) filenum, 0, 0);
    }
if (status)
    {
    *mod_time = 0;
    *mod_date = 0;
    }
}
#endif
