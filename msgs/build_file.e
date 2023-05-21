/*
 *	PROGRAM:	JRD Message Facility
 *	MODULE:		build_file.e
 *	DESCRIPTION:	Build message file
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
#include "../jrd/gds.h"
#include "../jrd/common.h"
#include "../jrd/msg.h"
#include "../jrd/gds_proto.h"

#define MAX_LEVELS	4

DATABASE DB = "msg.gdb";

static void	ascii_str_to_upper (TEXT *);
static USHORT	do_msgs (TEXT *, TEXT *, USHORT);
static void	propogate (MSGNOD *, MSGNOD *, ULONG, ULONG);
static SLONG	write_bucket (MSGNOD, USHORT);
static void	sanitize (TEXT *);

static SLONG 	file_position;
static int	file;

/* keep the LOCALE_PAT names in sync with gds.h */

#if (defined WIN_NT || defined OS2_ONLY)
#include <io.h>
#define FILENAME	"interbas.msg"
#define LOCALE_PAT	"%.8s.msg"
#define PATH_SEPARATOR	'\\'
#endif

#ifdef PC_PLATFORM
#include <fcntl.h>
#include <io.h>
#define FILENAME	"interbas.msg"
#define LOCALE_PAT	"%.8s.msg"
#define PATH_SEPARATOR	'\\'
#endif

#ifdef mpexl
#define FILENAME	"message.data"
#define LOCALE_PAT	"%.8s.data"
#define PATH_SEPARATOR	'?'
#endif

#ifndef FILENAME
#define FILENAME	"interbase.msg"
#define LOCALE_PAT	"%.10s.msg"
#define PATH_SEPARATOR	'/'
#endif

#ifdef VMS
#include <file.h>
#else
#include <sys/types.h>
#ifndef DELTA
#ifndef XENIX
#ifndef PC_PLATFORM
#ifndef OS2_ONLY
#ifndef WIN_NT
#ifndef mpexl
#include <sys/file.h>
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#ifndef O_RDWR
#include <fcntl.h>
#endif

#ifndef O_BINARY
#define O_BINARY	0
#endif

int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Top level routine.
 *
 **************************************/
TEXT	*p, **end_args, db_file [256], filename [256],
	*pathname, pathbuffer [256], *locale;
USHORT	sw_bad;
USHORT	sw_warning;
USHORT	i, locale_count;
SSHORT	len;
BASED ON LOCALES.LOCALE	this_locale;

strcpy (db_file, "msg.gdb");
strcpy (filename, FILENAME);
pathname = NULL;
locale = NULL;
sw_warning = 0;

end_args = argv + argc;

for (++argv; argv < end_args;)
    {
    p = *argv++;
    sw_bad = FALSE;
    if (*p != '-')
	sw_bad = TRUE;
    else
	switch (UPPER (p [1]))
	    {
	    case 'D':
		strcpy (db_file, *argv++);
		break;
	    
	    case 'F':
		strcpy (filename, *argv++);
		break;
	    
	    case 'P':
		strcpy (pathbuffer, *argv++);
		pathname = pathbuffer;
		break;

	    case 'L':
		locale = *argv++;
		break;
	    
	    case 'W':
		sw_warning++;
		break;
	    
	    default:
		sw_bad = TRUE;
	    }
    if (sw_bad)
	{
	ib_printf ("Invalid option \"%s\".  Valid options are:\n", p);
	ib_printf ("\t-D\tDatabase name\n");
	ib_printf ("\t-F\tMessage file name\n");
	ib_printf ("\t-P\tMessage file path (should end with '/')\n");
	ib_printf ("\t-L\tLocale name\n");
	ib_printf ("\t-W\tVerbose warning messages\n");
	exit (FINI_ERROR);
	}
    }

/* Get db_file ready to go */

READY db_file AS DB;
START_TRANSACTION;

/* make sure the path name ends in a '/' */

if (pathname)
    {
    len = strlen (pathname);
    if (pathname [len - 1] != PATH_SEPARATOR)
	{
	pathname [len] = PATH_SEPARATOR;
	pathname [len + 1] = 0;
	}
    }

/* check for the locale option  */

if (!locale)	/* no locale given: do the regular US msgs  */
    {
    if (!pathname)
    	do_msgs (filename, NULL, 0);
    else
	{
    	strcat (pathname, filename);
    	do_msgs (pathname, NULL, 0);
	}
    }
else
    {
    int	got_one = 0;
    FOR FIRST 1 L IN LOCALES WITH L.LOCALE = locale
	got_one = 1;
    END_FOR;
    if (got_one)     /* do only 1 locale */
	{
	strcpy (this_locale, locale);
        sanitize (locale);
	if (pathname)
	    {
            sprintf (filename, LOCALE_PAT, locale);
            strcat (pathname, filename);
            do_msgs (pathname, this_locale, sw_warning);
	    }
	else
            do_msgs (filename, this_locale, sw_warning);
	}
    else
	{
	strncpy (this_locale, locale, sizeof (this_locale));
        ascii_str_to_upper (this_locale);
        if (!strcmp (this_locale, "ALL"))         
            {
            FOR LC IN LOCALES		/* do all locales except piglatin */
   		WITH LC.LOCALE != "pg_PG" AND LC.LOCALE != "piglatin"; 
		locale = LC.LOCALE;
                strcpy (this_locale, LC.LOCALE);
                ib_printf ("build_file: building locale %s", this_locale);
                sanitize (locale);
                sprintf (filename, LOCALE_PAT, locale);
		if (pathname)
		    {
                    strcat (pathname, filename);
                    ib_printf (" to file %s\n", pathname);
                    do_msgs (pathname, this_locale, sw_warning);
		    }
		else
		    {
                    ib_printf (" to file %s\n", filename);
                    do_msgs (filename, this_locale, sw_warning);
		    }
		pathname = NULL;
            END_FOR;
            }
        else
            {
            ib_printf ("build_file: Unknown locale: %s\n", locale); 
            ib_printf ("Valid options are:\n");
            FOR LO IN LOCALES
                ib_printf ("\t%s\n", LO.LOCALE);
            END_FOR;
            ib_printf ("\tall\n");
	    }
	COMMIT;
	FINISH DB;
	exit (FINI_ERROR);
	}
    }

COMMIT;
FINISH;
    
exit (FINI_OK);
}

static void ascii_str_to_upper (
    TEXT	*s)
{
/**************************************
 *
 *	a s c i i _ s t r _ t o _ u p p e r
 *
 **************************************
 *
 * Functional description
 *	change the a - z chars in string to upper case
 *
 **************************************/

while (*s)
    {
    *s >= 'a' && *s <= 'z' ? *s &= 0xDF : *s;
    *s++;
    }
}

static USHORT do_msgs (
    TEXT	*filename,
    TEXT	*locale,
    USHORT	sw_warning)
{
/**************************************
 *
 *	d o _ m s g s
 *
 **************************************
 *
 * Functional description
 *	Build the message file
 *
 **************************************/
ISC_MSGHDR  header;
TEXT    buffer [MAX_LEVELS * MSG_BUCKET], *p, *q, *end_leaf, msg_text [256];
MSGREC  leaf_node, leaf;
MSGNOD  buckets [MAX_LEVELS], *ptr, *ptr2, *end, nodes [MAX_LEVELS], node;
USHORT   l, n, levels;
ULONG    position, prior_code;
int	warning_counter;

warning_counter = 0;
 
/* Divy up memory among various buffers */

leaf_node = leaf = (MSGREC) gds__alloc ((SLONG) MSG_BUCKET);
end_leaf = (TEXT*) leaf + MSG_BUCKET;
nodes [0] = NULL;

/* Open output file */

#ifndef mpexl
if ((file = open (filename, O_WRONLY | O_CREAT | O_BINARY, 0666)) == -1)
#else
if ((file = open (filename, O_WRONLY | O_CREAT | O_MPEOPTS, 0666, "b Ds2 E32 S20000")) == -1)
#endif
    {
    ib_printf ("Can't open %s\n", filename);
    return FINI_ERROR;
    }
file_position = 0;

/* Format and write header */

header.msghdr_major_version = MSG_MAJOR_VERSION;
header.msghdr_minor_version = MSG_MINOR_VERSION;
header.msghdr_bucket_size = MSG_BUCKET;
write_bucket (&header, sizeof (header));

/* Write out messages building B-tree */

FOR X IN MESSAGES SORTED BY X.CODE

    /* pre-load with US English message - just in case we don't
     * have a translation available. */

    strcpy (msg_text, X.TEXT);
    l = strlen (msg_text);
    if (locale)					/* want translated output */
	{
	/* Overwrite English message with translation, if we find one */
	int found_one;
	found_one = 0;
	FOR Y IN TRANSMSGS WITH Y.FAC_CODE = X.FAC_CODE
	    AND Y.NUMBER = X.NUMBER
	    AND Y.LOCALE = locale
	    AND Y.TEXT NOT MISSING
	    AND Y.TEXT != " ";
	        strcpy (msg_text, Y.TEXT);
	        l = strlen (msg_text);
	        found_one++;
        END_FOR;
	if (!found_one && sw_warning)
	    ib_printf ("build_file: Warning - no %s translation of msg# %d\n", locale, X.CODE);
        if (found_one > 1 && sw_warning)
	   ib_printf ("build_file: Warning - multiple %s translations of msg# %d\n", locale, X.CODE);
	if (found_one != 1)
	   warning_counter++;
        }

    if (leaf_node->msgrec_text + l >= end_leaf)
	{
	position = write_bucket (leaf, n);
	propogate (buckets, nodes, prior_code, position);
	leaf_node = leaf;
	}
    leaf_node->msgrec_code = prior_code = MSG_NUMBER (X.FAC_CODE, X.NUMBER);

    leaf_node->msgrec_length = l;
    leaf_node->msgrec_flags = X.FLAGS;
    n = OFFSETA (MSGREC, msgrec_text) + l;
    p = leaf_node->msgrec_text;
    if (l)
	{
	q = msg_text;
	do *p++ = *q++; while (--l);
	}
    n = p - (SCHAR*) leaf;
    leaf_node = NEXT_LEAF (leaf_node);
END_FOR;

/* Write a high water mark on leaf node */

if (leaf_node->msgrec_text + l >= end_leaf)
    {
    n = (SCHAR*) leaf_node - (SCHAR*) leaf;
    position = write_bucket (leaf, n);
    propogate (buckets, nodes, prior_code, position);
    leaf_node = leaf;
    }

leaf_node->msgrec_code = -1;
leaf_node->msgrec_length = 0;
leaf_node->msgrec_flags = 0;
n = (SCHAR*) leaf_node - (SCHAR*) leaf;
position = write_bucket (leaf, n);
    
/* Finish off upper levels of tree */

header.msghdr_levels = 1;

for (ptr = nodes, ptr2 = buckets; node = *ptr; ptr++, ptr2++)
    {
    node->msgnod_code = -1;
    node->msgnod_seek = position;
    n = (SCHAR*) (node + 1) - (SCHAR*) *ptr2;
    position = write_bucket (*ptr2, n);
    ++header.msghdr_levels;
    }

header.msghdr_top_tree = position;

/* Re-write header record and finish */

lseek (file, (SLONG) 0, 0);
write (file, &header, sizeof (header));
close (file);
file = -1;

if (warning_counter)
    ib_printf ("build_file: %d messages lack translations in locale %s\n",
	     warning_counter, locale);
}

static void propogate (
    MSGNOD	*buckets,
    MSGNOD	*nodes,
    ULONG	prior_code,
    ULONG	position)
{
/**************************************
 *
 *	p r o p o g a t e
 *
 **************************************
 *
 * Functional description
 *	Propogate a full bucket upward.
 *
 **************************************/
MSGNOD	node, end;

/* Make sure current level has been allocated */

if (!*nodes)
    {
    *nodes = *buckets = (MSGNOD) gds__alloc ((SLONG) MSG_BUCKET);
    nodes [1] = NULL;
    }

/* Insert into current bucket */

node = (*nodes)++;
node->msgnod_code = prior_code;
node->msgnod_seek = position;

/* Check for full bucket.  If not, we're done */

end = (MSGNOD) ((SCHAR*) *buckets + MSG_BUCKET);

if (*nodes < end)
    return;

/* Bucket is full -- write it out, propogate the split, and re-initialize */

position = write_bucket (*buckets, MSG_BUCKET);
propogate (buckets + 1, nodes + 1, prior_code, position);
*nodes = *buckets;
}

static SLONG write_bucket (
    MSGNOD	bucket,
    USHORT	length)
{
/**************************************
 *
 *	w r i t e _ b u c k e t
 *
 **************************************
 *
 * Functional description
 *	Write stuff, return position of stuff written.
 *
 **************************************/
SLONG	position;
int	n;
USHORT	padded_length;
SLONG	zero_bytes = 0;

padded_length = ROUNDUP (length, sizeof (SLONG));
position = file_position;
n = write (file, bucket, length);
if (n == -1)
    {
    ib_fprintf (ib_stderr, "IO error on write()\n" );
    exit (FINI_ERROR);
    }
n = write (file, &zero_bytes, padded_length-length);
if (n == -1)
    {
    ib_fprintf (ib_stderr, "IO error on write()\n" );
    exit (FINI_ERROR);
    }
file_position += padded_length;

return position;
}

static void sanitize (
    TEXT        *locale)
{
/**************************************
 *
 *	s a n i t i z e
 *
 **************************************
 *
 * Functional description
 *	Clean up a locale to make it acceptable for use in file names
 *      for Windows NT, PC, and mpexl: remove any '.' or '_' for mpexl,
 *	replace any period with '_' for NT or PC.
 *      Keep this in sync with gds.c
 *
 **************************************/
SSHORT ch;

while (*locale)
    {
    ch = *locale;
#ifdef mpexl
    if (ch == '.' || ch == '_')
	{
	strcpy (locale, locale + 1);
        locale++;
	}
#else
    if (ch == '.')
        *locale = '_';
#endif
    locale++;
    }
}

