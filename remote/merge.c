/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		merge.c
 *	DESCRIPTION:	Merge database/server inforation
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

#include <string.h>
#include "../jrd/gds.h"
#include "../remote/remote.h"
#include "../remote/merge_proto.h"
#include "../jrd/gds_proto.h"

#define PUT_WORD(ptr, value)	{*(ptr)++ = value; *(ptr)++ = value >> 8;}
#define PUT(ptr, value)		*(ptr)++ = value;

static SSHORT	convert (ULONG, UCHAR *);
static STATUS	merge_setup (UCHAR **, UCHAR **, UCHAR *, USHORT);

USHORT DLL_EXPORT MERGE_database_info (
    UCHAR	*in,
    UCHAR	*out,
    USHORT	out_length,
    USHORT	impl,
    USHORT	class,
    USHORT	base_level,
    UCHAR	*version,
    UCHAR	*id,
    ULONG	mask)
{
/**************************************
 *
 *	M E R G E _ d a t a b a s e _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Merge server / remote interface / Y-valve information into
 *	database block.  Return the actual length of the packet.
 *
 **************************************/
SSHORT	length, l;
UCHAR	*start, *end, *p;

start = out;
end = out + out_length;

for (;;)
    switch (*out++ = *in++)
	{
	case gds__info_end:
	case gds__info_truncated:
	    return out - start;
	
	case gds__info_version:
	    l = strlen ((SCHAR*) (p = version));
	    if (merge_setup (&in, &out, end, l + 1))
		return 0;
	    if ((*out++ = l) != NULL)
		do *out++ = *p++; while (--l);
	    break;

	case gds__info_db_id:
	    l = strlen ((SCHAR*) (p = id));
	    if (merge_setup (&in, &out, end, l + 1))
		return 0;
	    if ((*out++ = l) != NULL)
		do *out++ = *p++; while (--l);
	    break;

	case gds__info_implementation:
	    if (merge_setup (&in, &out, end, 2))
		return 0;
	    PUT (out, impl);
	    PUT (out, class);
	    break;

	case gds__info_base_level:
	    if (merge_setup (&in, &out, end, 1))
		return 0;
	    PUT (out, base_level);
	    break;

	default:
	    length = gds__vax_integer (in, 2);
	    in += 2;
	    if (out + length + 2 >= end)
		{
		out [-1] = gds__info_truncated;
		return 0;
		}
	    PUT_WORD (out, length);
	    if (length)
		do *out++ = *in++; while (--length);
	    break;
	}
}

static SSHORT convert (
    ULONG	number,
    UCHAR	*buffer)
{
/**************************************
 *
 *	c o n v e r t
 *
 **************************************
 *
 * Functional description
 *	Convert a number to VAX form -- least significant bytes first.
 *	Return the length.
 *
 **************************************/
ULONG	n;
UCHAR	*p;

#ifdef VAX
n = number;
p = (UCHAR*) &n;
*buffer++ = *p++;
*buffer++ = *p++;
*buffer++ = *p++;
*buffer++ = *p++;

#else

p = (UCHAR*) &number;
p += 3;
*buffer++ = *p--;
*buffer++ = *p--;
*buffer++ = *p--;
*buffer++ = *p;

#endif

return 4;
}

static STATUS merge_setup (
    UCHAR	**in,
    UCHAR	**out,
    UCHAR	*end,
    USHORT	delta_length)
{
/**************************************
 *
 *	m e r g e _ s e t u p
 *
 **************************************
 *
 * Functional description
 *	Get ready to toss new stuff onto an info packet.  This involves
 *	picking up and bumping the "count" field and copying what is
 *	already there.
 *
 **************************************/
USHORT	length, new_length, count;

length = gds__vax_integer (*in, 2);
new_length = length + delta_length;

if (*out + new_length + 2 >= end)
    {
    (*out) [-1] = gds__info_truncated;
    return FAILURE;
    }

*in += 2;
count = 1 + *(*in)++;
PUT_WORD (*out, new_length);
PUT (*out, count);

/* Copy data portion of information sans original count */

if (--length)
    do *(*out)++ = *(*in)++; while (--length);

return SUCCESS;
}
