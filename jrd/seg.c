/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         seg.c
 *      DESCRIPTION:    Segmented memory data mover
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

#include <dos.h>
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/seg_proto.h"

#ifdef	memset
#undef	memset
#endif
#ifdef	memcpy
#undef	memcpy
#endif
#ifdef	memcmp
#undef	memcmp
#endif

int	SEG_compare(
    UCHAR	*op1,
    UCHAR	*op2,
    ULONG	 length)
{
/**************************************
 *
 *      S E G _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *      Compare segmented memory.
 *
 **************************************/
UCHAR HUGE_PTR		*o1;
UCHAR HUGE_PTR		*o2;
ULONG			 count;
ULONG			 o1count;
ULONG			 o2count;
int			 result;

    if ( !length)
	return 0;
    o1 = (UCHAR HUGE_PTR*)op1;
    o2 = (UCHAR HUGE_PTR*)op2;
    while ( length)
    {

	/* figure out which operand is closer to seg boundary */

	o1count = 65536L - (ULONG)( FP_OFF( o1));
	o2count = 65536L - (ULONG)( FP_OFF( o2));
	count = o1count;
	if ( count > o2count)
	    count = o2count;

	/* only compare as much as needed */

	if ( count > length)
	    count = length;

	/* next, check for worst case - both are at segment boundary,
	   in which case we do two operations to avoid calling memcmp
	   with length of 0 (65536 in 16 bits) */

	if ( count == 65536L)
	{
	    result = memcmp( o1, o2, 32768L);
	    if ( result)
		return result;
	    o1 += 32768L;
	    o2 += 32768L;
	    length -= 32768L;    
	    count -= 32768L;
	}
	result = memcmp( o1, o2, (unsigned)count);
	if ( result)
	    return result;
	o1 += count;
	o2 += count;
	length -= count;

	/* at this point, either the comparison is complete, or at least
	   one pointer is at the start of the next segment */

    }
    return 0;
}

void	*SEG_move(
    UCHAR	*to,
    UCHAR	*from,
    ULONG	 length)
{
/**************************************
 *
 *      S E G _ m o v e
 *
 **************************************
 *
 * Functional description
 *      Move segmented memory.
 *
 **************************************/
UCHAR HUGE_PTR		*src;
UCHAR HUGE_PTR		*dst;
ULONG			 count;
ULONG			 scount;
ULONG			 dcount;

    src = (UCHAR HUGE_PTR*)from;
    dst = (UCHAR HUGE_PTR*)to;
    while ( length)
    {

	/* figure out which  (source or dest) is closer to seg boundary */

	scount = 65536L - (ULONG)( FP_OFF( src));
	dcount = 65536L - (ULONG)( FP_OFF( dst));
	count = scount;
	if ( count > dcount)
	    count = dcount;

	/* only copy as much as needed */

	if ( count > length)
	    count = length;

	/* next, check for worst case - both are at segment boundary,
	   in which case we do two operations to avoid calling memcpy
	   with length of 0 (65536 in 16 bits) */

	if ( count == 65536L)
	{
	    memcpy( dst, src, 32768L);
	    dst += 32768L;
	    src += 32768L;
	    length -= 32768L;    
	    count -= 32768L;
	}
	memcpy( dst, src, (unsigned)count);
	dst += count;
	src += count;
	length -= count;

	/* at this point, either the copy is complete, or at least
	   one pointer is at the start of the next segment */

    }
    return to;
}

void	*SEG_set(
    UCHAR	*to,
    UCHAR	 value,
    ULONG	 length)
{
/**************************************
 *
 *      S E G _ s e t
 *
 **************************************
 *
 * Functional description
 *      Set segmented memory to value.
 *
 **************************************/

UCHAR HUGE_PTR		*dst;
ULONG			 count;

    dst = (UCHAR HUGE_PTR*)to;
    while ( length)
    {

	/* get count left in current segment */

	count = 65536L - (ULONG)( FP_OFF( dst));

	/* only initialize as much as needed */

	if ( count > length)
	    count = length;

	/* next, check to see if this starts on a segment boundary,
	   in which case we do two operations to avoid calling memset
	   with length of 0 (65536 in 16 bits) */

	if ( count == 65536L)
	{
	    memset( dst, value, 32768L);
	    dst += 32768L;
	    length -= 32768L;
	    count -= 32768L;
	}
	memset( dst, value, (unsigned)count);
	dst += count;
	length -= count;

	/* dst now either points to the end of the area to be filled,
	   in which case we're done, or it's at the start of the next
	   segment and ready for more filling */

    }
    return to;
}
