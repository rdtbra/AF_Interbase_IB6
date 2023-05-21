/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		perf.c
 *	DESCRIPTION:	Performance monitoring routines
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
#include <limits.h>
#include "../jrd/time.h"
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/perf.h"
#include "../jrd/gds_proto.h"
#include "../jrd/perf_proto.h"

#ifdef DELTA
#include <sys/param.h>
#endif

#ifdef NETWARE_386
#define NO_TIMES
#define TIMEZONE	timezon
#include <sys/timeb.h>
#undef timeszone

#else
#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
#include <sys/timeb.h>
#define NO_TIMES
#define NO_GETTIMEOFDAY
#endif 
#endif /* NETWARE_386 */

static SLONG	get_parameter (SCHAR **);
#ifdef NO_TIMES
static void	times (struct tms *);
#endif

static SCHAR items [] = {isc_info_reads, isc_info_writes, isc_info_fetches,
			isc_info_marks,
			isc_info_page_size, isc_info_num_buffers, 
			isc_info_current_memory, isc_info_max_memory};

static SCHAR *report =
"elapsed = !e cpu = !u reads = !r writes = !w fetches = !f marks = !m$";

#ifdef VMS
#define NO_GETTIMEOFDAY
#define TICK	100
extern void	ftime();
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#define TICK	100
#endif

#ifdef mpexl
#define TICK	100
#endif

#ifndef TICK
#define TICK	CLK_TCK
#endif

#ifdef SHLIB_DEFS
#ifdef IMP
typedef SLONG            clock_t;
#endif
#define times		(*_libgds_times)

extern clock_t		times();
#endif

API_ROUTINE perf_format (
    PERF	*before,
    PERF	*after,
    SCHAR	*string,
    SCHAR	*buffer,
    SSHORT	*buf_len)
{
/**************************************
 *
 *	P E R F _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *	Format a buffer with statistical stuff under control of formatting
 *	string.  Substitute in appropriate stuff.  Return the length of the
 *	formatting output.
 *
 **************************************/
SLONG	delta, buffer_length, length;
SCHAR	*p, c;

buffer_length = (buf_len) ? *buf_len : 0;
p = buffer;

while ((c = *string++) && c != '$')
    if (c != '!')
	*p++ = c;
    else
	{
	switch (c = *string++)
	    {
	    case 'r':
		delta = after->perf_reads - before->perf_reads;
		break;
	    case 'w':
		delta = after->perf_writes - before->perf_writes;
		break;
	    case 'f':
		delta = after->perf_fetches - before->perf_fetches;
		break;
	    case 'm':
		delta = after->perf_marks - before->perf_marks;
		break;
	    case 'd':
		delta = after->perf_current_memory - before->perf_current_memory;
		break;
	    case 'p':
		delta = after->perf_page_size;
		break;
	    case 'b':
		delta = after->perf_buffers;
		break;
	    case 'c':
		delta = after->perf_current_memory;
		break;
	    case 'x':
		delta = after->perf_max_memory;
		break;
	    case 'e':
		delta = after->perf_elapsed - before->perf_elapsed;
		break;
	    case 'u':
		delta = after->perf_times.tms_utime - 
			before->perf_times.tms_utime;
		break;
	    case 's':
		delta = after->perf_times.tms_stime - 
			before->perf_times.tms_stime;
		break;
	    default:
		sprintf (p, "?%c?", c);
		while (*p) p++;
	    }
	switch (c)
	    {
	    case 'r':
	    case 'w':
	    case 'f':
	    case 'm':
	    case 'd':
	    case 'p':
	    case 'b':
	    case 'c':
	    case 'x':
#ifdef PC_PLATFORM
		sprintf (p, "%ld", delta);
#else
		sprintf (p, "%d", delta);
#endif
		while (*p) p++;
		break;

	    case 'u':
	    case 's':
#ifdef VMS
		sprintf (p, "%d.%.2d", delta  / 100,
			(delta % 100));
#else
#ifdef PC_PLATFORM
		sprintf (p, "%ld.%.2ld", (SLONG) (delta / (SLONG) TICK),
			(SLONG) (((delta % (SLONG) TICK) * 100L) /(SLONG) TICK));
#else
		sprintf (p, "%d.%.2d", delta  / TICK,
			(delta % TICK) * 100 / TICK);
#endif
#endif
		while (*p) p++;
		break;

	    case 'e':
#ifdef PC_PLATFORM
		sprintf (p, "%ld.%.2ld", (SLONG) (delta / 100L),
			(SLONG) (delta % 100L));
#else
		sprintf (p, "%d.%.2d", delta / 100,
			delta % 100);
#endif
		while (*p) p++;
		break;
	    }
	}

*p = 0;
length = p - buffer;
if (buffer_length && (buffer_length -= length) >= 0)
    do *p++ = ' '; while (--buffer_length);

return length;
}

void API_ROUTINE perf_get_info (
    int		**handle,
    PERF	*perf)
{
/**************************************
 *
 *	P E R F _ g e t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Acquire timing and performance information.  Some info comes
 *	from the system and some from the database.
 *
 **************************************/
SCHAR		*p, buffer [256];
SSHORT		l, buffer_length, item_length;
STATUS		jrd_status [20];
#ifdef NO_GETTIMEOFDAY
struct	timeb	time_buffer;
#define LARGE_NUMBER 696600000	  /* to avoid overflow, get rid of decades) */
#else
#ifndef mpexl
struct timeval	tp;
#endif
#endif

/* If there isn't a database, zero everything out */

if (!*handle)
    {
    p = (SCHAR*) perf;
    l = sizeof (PERF);
    do *p++ = 0; while (--l);
    }

/* Get system times */

times (&perf->perf_times);

#ifdef mpexl

perf->perf_elapsed = time ((SLONG*) 0) * 100;

#else 
#ifdef NO_GETTIMEOFDAY
ftime (&time_buffer);
perf->perf_elapsed = (time_buffer.time - LARGE_NUMBER) * 100 + (time_buffer.millitm / 10);

#else 
#ifdef	M88K
gettimeofday (&tp);
#else
gettimeofday (&tp, 0L);
#endif  /* M88K */

perf->perf_elapsed = tp.tv_sec * 100 + tp.tv_usec / 10000;

#endif  /* NO_GETTIMEOFDAY */
#endif  /* mpexl  */

if (!*handle)
    return;

buffer_length = sizeof (buffer);
item_length = sizeof (items);
isc_database_info (jrd_status, 
	handle, 
	item_length, 
	items,
	buffer_length, 
	buffer);

p = buffer;

while (1)
    switch (*p++)
	{
	case isc_info_reads:
	    perf->perf_reads = get_parameter (&p);
	    break;

	case isc_info_writes:
	    perf->perf_writes = get_parameter (&p);
	    break;

	case isc_info_marks:
	    perf->perf_marks = get_parameter (&p);
	    break;

	case isc_info_fetches:
	    perf->perf_fetches = get_parameter (&p);
	    break;

	case isc_info_num_buffers:
	    perf->perf_buffers = get_parameter (&p);
	    break;

	case isc_info_page_size:
	    perf->perf_page_size = get_parameter (&p);
	    break;

	case isc_info_current_memory:
	    perf->perf_current_memory = get_parameter (&p);
	    break;

	case isc_info_max_memory:
	    perf->perf_max_memory = get_parameter (&p);
	    break;

	case isc_info_end:
	    return;

	case isc_info_error:
	    if (p [2] == isc_info_marks)
		perf->perf_marks = 0;
	    else if (p [2] == isc_info_current_memory)
		perf->perf_current_memory = 0;
	    else if (p [2] == isc_info_max_memory)
		perf->perf_max_memory = 0;
	    l = isc_vax_integer (p, 2);
	    p += l + 2;
	    perf->perf_marks = 0;
	    break;

	default:
	    return;
	}
}

void API_ROUTINE perf_report (
    PERF	*before,
    PERF	*after,
    SCHAR	*buffer,
    SSHORT	*buf_len)
{
/**************************************
 *
 *	p e r f _ r e p o r t
 *
 **************************************
 *
 * Functional description
 *	Report a common set of parameters.
 *
 **************************************/

perf_format (before, after, report, buffer, buf_len);
}

static SLONG get_parameter (
    SCHAR	**ptr)
{
/**************************************
 *
 *	g e t _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Get a parameter (length is encoded), convert to internal form,
 *	and return.
 *
 **************************************/
SLONG	parameter;
SCHAR	*p;
SSHORT	l;

l = *(*ptr)++;
l += (*(*ptr)++) << 8;
parameter = isc_vax_integer (*ptr, l);
*ptr += l;

return parameter;
}

#ifdef NO_TIMES
static void times (
    struct tms	*buffer)
{
/**************************************
 *
 *	t i m e s
 *
 **************************************
 *
 * Functional description
 *	Emulate the good old unix call "times".  Only both with user time.
 *
 **************************************/

buffer->tms_utime = clock();
}
#endif

