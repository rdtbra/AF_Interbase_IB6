/*
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
#include <stdio.h>
#ifdef apollo
#include "/interbase/include/gds.ins.c"
#else
#ifdef VMS
#include "interbase:[syslib]gds.h"
#else
#include <gds.h>
#endif
#endif

#ifndef CHAR
#define CHAR        unsigned char
#endif

#ifndef SHORT
#define SHORT       unsigned short
#endif

static void	set_statistics();
static int	dump_text(), read_text();

typedef struct ctl {
    int		(*ctl_source)();		/* Source filter */
    struct ctl	*ctl_source_handle;		/* Argument to pass to source filter */
    short	ctl_to_sub_type;		/* Target type */
    short	ctl_from_sub_type;		/* Source type */
    SHORT	ctl_buffer_length;		/* Length of buffer */
    SHORT	ctl_segment_length;		/* Length of current segment */
    SHORT	ctl_bpb_length;			/* Length of blob parameter block */
    char	*ctl_bpb;			/* Address of blob parameter block */
    CHAR	*ctl_buffer;			/* Address of segment buffer */
    long	ctl_max_segment;		/* Length of longest segment */
    long	ctl_number_segments;		/* Total number of segments */
    long	ctl_total_length;		/* Total length of blob */
    long	*ctl_status;			/* Address of status vector */
    long	ctl_data [8];			/* Application specific data */
} *CTL;

#define ACTION_open		0
#define ACTION_get_segment	1
#define ACTION_close		2
#define ACTION_create		3
#define ACTION_put_segment	4

#define SUCCESS			0
#define FAILURE			1

#define BUFFER_LENGTH		512

#ifdef SHLIB_DEFS
#define system		(*_libfun_system)
#define fopen		(*_libfun_fopen)
#define unlink		(*_libfun_unlink)
#define fprintf		(*_libfun_fprintf)
#define fclose		(*_libfun_fclose)
#define fgetc		(*_libfun_fgetc)

extern int	system();
extern FILE	*fopen();
extern int	unlink();
extern int	fprintf();
extern int	fclose();
extern int	fgetc();
#endif

int nroff_filter (action, control)
    SHORT	action;
    CTL		control;
{
/**************************************
 *
 *	n r o f f _  f i l t e r 
 *
 **************************************
 *
 * Functional description
 *	Translate a blob from nroff markup
 *	to nroff output.  Read the blob
 *	into a file and process it on open,
 *	then read it back line by line in
 *	the get_segment loop.
 *
 **************************************/
int	status;
FILE	*text_file;
char	*out_buffer;

switch (action)
    {
    case ACTION_open:
	if (status = dump_text (action, control))
	    return status;
#ifndef M_UNIX
	system ("nroff foo.nr >foo.txt");
#else
	system ("deroff foo.nr >foo.txt");
#endif
	set_statistics("foo.txt", control);  /* set up stats in ctl struct */
#ifdef DEBUG
	printf("max_seg_length: %d, num_segs: %d, length: %d\n", 
	    control->ctl_max_segment, control->ctl_number_segments, 
	    control->ctl_total_length);
#endif
	break;

    case ACTION_get_segment:
	/* open the file first time through and save the file pointer */

	if (!control->ctl_data [0])
	    {
	    text_file = fopen ("foo.txt", "r");
	    control->ctl_data[0] = (long) text_file;
	    }
	if (status = read_text (action, control))
	    return status;
	break;

    case ACTION_close:
	/* don't need the temp files any more, so clean them up */

	unlink("foo.nr");
	unlink("foo.txt");
	break;

    case ACTION_create:
    case ACTION_put_segment:
	return gds__uns_ext;
    }

return SUCCESS;
}

static int caller (action, control, buffer_length, buffer, return_length)
    SHORT        action;
    CTL                control;
    SHORT        buffer_length;
    CHAR        *buffer;
    SHORT        *return_length;
{
/**************************************
 *
 *        c a l l e r
 *
 **************************************
 *
 * Functional description
 *        Call next source filter.  This 
 *        is a useful service routine for
 *        all blob filters.
 *
 **************************************/
int        status;
CTL        source;

source = control->ctl_source_handle;
source->ctl_status = control->ctl_status;
source->ctl_buffer = buffer;
source->ctl_buffer_length = buffer_length;

status = (*source->ctl_source) (action, source);

if (return_length)
    *return_length = source->ctl_segment_length;

return status;
}
 




static int dump_text (action, control)
    SHORT	action;
    CTL		control;
{
/**************************************
 *
 *	d u m p _ t e x t 
 *
 **************************************
 *
 * Functional description
 *	Open a blob and write the
 *	contents to a file
 *
 **************************************/
FILE	*text_file;
CHAR	buffer [512];
SHORT	length;
int	status;

 
if (!(text_file = fopen ("foo.nr", "w")))
    return FAILURE;

while (!(status = caller (ACTION_get_segment, control, sizeof(buffer) - 1, buffer, &length)))
    {
    buffer[length] = 0; 
    fprintf (text_file, buffer);
    }

fclose (text_file);

if (status != gds__segstr_eof)
    return status;

return SUCCESS;
}

static void set_statistics (filename, control)
    char	*filename;
    CTL		control;
{
/*************************************
 *
 *	s e t _ s t a t i s t i c s
 *
 *************************************
 *
 * Functional description
 *	Sets up the statistical fields
 *	in the passed in ctl structure.
 *	These fields are:
 *	  ctl_max_segment - length of
 *	     longest seg in blob (in
 *	     bytes)
 *	  ctl_number_segments - # of
 *	     segments in blob
 *	  ctl_total_length - total length
 *	     of blob in bytes.
 *	Since the nroff changed the
 *	values, we should reset the
 *	ctl structure, so that 
 *	blob_info calls get the right
 *	values.
 *
 *************************************/
register int	max_seg_length;
register int	length;
register int	num_segs;
register int	cur_length;
FILE		*file;
char		*p;
char		c;

num_segs = 0;
length = 0;
max_seg_length = 0;
cur_length = 0;

file = fopen ("foo.txt", "r");

while (1)
    {
    c = fgetc (file);
    if (feof (file))
	break;
    length++;
    cur_length++;

    if (c == '\n')    /* that means we are at end of seg */
	{
	if (cur_length > max_seg_length)
	    max_seg_length = cur_length;
	num_segs++;
	cur_length = 0;
	}
    }

control->ctl_max_segment = max_seg_length;
control->ctl_number_segments = num_segs;
control->ctl_total_length = length;
}

static int read_text (action, control)
    SHORT	action;
    CTL		control;
{
/**************************************
 *
 *	r e a d _ t e x t 
 *
 **************************************
 *
 * Functional description
 *	Reads a file one line at a time 
 *	and puts the data out as if it 
 *	were coming from a blob. 
 *
 **************************************/
CHAR	*p;
FILE	*file;
SHORT	length;
int	c, status;


p = control->ctl_buffer;
length = control->ctl_buffer_length;
file = (FILE *)control->ctl_data [0];

for (;;)
    {
    c = fgetc (file);
    if (feof (file))
	break;
    *p++ = c;
    if ((c == '\n')  || p >= control->ctl_buffer + length)
	{
	control->ctl_segment_length = p - control->ctl_buffer; 
	if (c == '\n')
	    return SUCCESS; 
	else
	    return gds__segment;
	}
    }

return gds__segstr_eof;
}
