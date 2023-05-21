/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		mov.c
 *	DESCRIPTION:	Data mover and converter and comparator, etc.
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

#include "../jrd/gdsassert.h"
#include "../jrd/time.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/intl.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/cvt2_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/mov_proto.h"


int MOV_compare (
    DSC	*arg1,
    DSC	*arg2)
{
/**************************************
 *
 *	M O V _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/

return CVT2_compare (arg1, arg2, (FPTR_VOID) ERR_post);
}

double MOV_date_to_double (
    DSC		*desc)
{
/**************************************
 *
 *	M O V _ d a t e _ t o _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *    Convert a date to double precision for
 *    date arithmetic routines.  
 *
 **************************************/

return CVT_date_to_double (desc, (FPTR_VOID) ERR_post);
}

void MOV_double_to_date2 (
    double	real,
    DSC		*dsc)
{
/**************************************
 *
 *	M O V _ d o u b l e _ t o _ d a t e 2
 *
 **************************************
 *
 * Functional description
 *	Move a double to one of the many forms of a date
 *
 **************************************/
SLONG	fixed [2];

MOV_double_to_date (real, fixed);
switch (dsc->dsc_dtype)
    {
    case dtype_timestamp:
	((SLONG *)dsc->dsc_address) [0] = fixed [0];
	((SLONG *)dsc->dsc_address) [1] = fixed [1];
	break;
    case dtype_sql_time:
	((SLONG *)dsc->dsc_address) [0] = fixed [1];
	break;
    case dtype_sql_date:
	((SLONG *)dsc->dsc_address) [0] = fixed [0];
	break;
    default:
	assert (FALSE);
	break;
    }
}

void MOV_double_to_date (
    double	real,
    SLONG	fixed [2])
{
/**************************************
 *
 *	M O V _ d o u b l e _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert a double precision representation of a date
 *	to a fixed point representation.   Double is used for
 *      date arithmetic.
 *
 **************************************/

CVT_double_to_date (real, fixed, (FPTR_VOID) ERR_post);
}

#ifndef VMS
void MOV_fast (
    register SCHAR	*from,
    register SCHAR	*to,
    register ULONG	length)
{
/**************************************
 *
 *	M O V _ f a s t
 *
 **************************************
 *
 * Functional description
 *	Move a byte string as fast as possible.
 *
 **************************************/
ULONG	l;

if (l = (length >> 4))
    do {
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    	*to++ = *from++;
    } while (--l);

if (length &= 15)
    do *to++ = *from++; while (--length);
}

void MOV_faster (
    register SLONG	*from,
    register SLONG	*to,
    ULONG		length)
{
/**************************************
 *
 *	M O V _ f a s t e r
 *
 **************************************
 *
 * Functional description
 *	Do a long move, already aligned, as quickly as possible.
 *
 **************************************/
register ULONG	l;
register UCHAR	*p, *q;

assert (!((U_IPTR) to & (sizeof (ULONG) - 1)));  /* ULONG alignment required */
assert (!((U_IPTR) from & (sizeof (ULONG) - 1)));/* ULONG alignment required */

/* copy by chunks of 8 longwords == 32 bytes == 2**5 bytes */
if (l = (length >> 5))
    {
    do {
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;
    } while (--l);
    length &= (8*sizeof(ULONG)-1);
    }

/* Copy by longwords */
if (l = (length >> 2))
    do *to++ = *from++; while (--l);

/* Finally, copy any trailing bytes */
if (l = (length & 3))
    {
    p = (UCHAR*) to;
    q = (UCHAR*) from;
    do *p++ = *q++; while (--l);
    }
}
#endif

void MOV_fill (
    register SLONG	*to,
    ULONG		length)
{
/**************************************
 *
 *	M O V _ f i l l
 *
 **************************************
 *
 * Functional description
 *	Clear out a block.
 *
 **************************************/
register ULONG	l;
register UCHAR	*p;

/* If not longword aligned, fill bytewise until it is */

if (l = (((U_IPTR) to) & (sizeof (ULONG) - 1)))
    {
    l = sizeof (ULONG) - l;
    if (length < l)
	l = length;
    length -= l;
    p = (UCHAR*) to;
    while (l--)
	*p++ = 0;
    to = (SLONG *) p;
    assert (!(((U_IPTR) to)&(sizeof (ULONG) - 1)) /* We're now aligned ULONG */
	    || !length); 			  /* Or already completed */
    }

/* Fill in chunks of 8 longwords == 32 bytes == 2**5 bytes */
if (l = (length >> 5))
    {
    do {
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
	*to++ = 0;
    } while (--l);
    length &= (8*sizeof(ULONG)-1);
    }

/* Fill by longwords */
if (l = (length >> 2))
    do *to++ = 0; while (--l);

/* Finally, fill any trailing bytes */
if (l = (length & 3))
    {
    p = (UCHAR*) to;
    do *p++ = 0; while (--l);
    }
}

double MOV_get_double (
    DSC		*desc)
{
/**************************************
 *
 *	M O V _ g e t _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a double precision number
 *
 **************************************/

return CVT_get_double (desc, (FPTR_VOID) ERR_post);
}

SLONG MOV_get_long (
    DSC		*desc,
    SSHORT	scale)
{
/**************************************
 *
 *	M O V _ g e t _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a long (32 bit) integer of given
 *	scale.
 *
 **************************************/

return CVT_get_long (desc, scale, (FPTR_VOID) ERR_post);
}

SINT64 MOV_get_int64 (
    DSC		*desc,
    SSHORT	scale)
{
/**************************************
 *
 *	M O V _ g e t _ i n t 6 4
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a 64 bit integer of given
 *	scale.
 *
 **************************************/

return CVT_get_int64 (desc, scale, (FPTR_VOID) ERR_post);
}

void MOV_get_metadata_ptr (
    DSC		*desc,
    TEXT	**ptr)
{
/**************************************
 *
 *	M O V _ g e t _ m e t a d a t a _ p t r 
 *
 **************************************
 *
 * Functional description
 *	Get address and length of string, which will afterward's
 *	be treated as an Metadata name value.
 *	Note that any length-of-string worries belong
 *	to the caller.
 *
 **************************************/
USHORT	dummy_type;

CVT_get_string_ptr (desc, &dummy_type, ptr, NULL, 0, (FPTR_VOID) ERR_post); 

#ifdef DEV_BUILD
if ((dummy_type != ttype_metadata) &&
    (dummy_type != ttype_none) &&
    (dummy_type != ttype_ascii))
    ERR_bugcheck_msg ("Expected METADATA name");
#endif
}

void MOV_get_name (
    DSC		*desc,
    TEXT	*string)
{
/**************************************
 *
 *	M O V _ g e t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Get a name (max length 31, blank terminated) from a descriptor.
 *
 **************************************/

CVT2_get_name (desc, string, (FPTR_VOID) ERR_post);
}

SQUAD MOV_get_quad (
    DSC		*desc,
    SSHORT	scale)
{
/**************************************
 *
 *	M O V _ g e t _ q u a d
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a quad 
 *	Note: a quad is NOT the same as a 64 bit integer
 *
 **************************************/

return CVT_get_quad (desc, scale, (FPTR_VOID) ERR_post);
}

int MOV_get_string_ptr (
    DSC		*desc,
    USHORT	*ttype,
    UCHAR	**address,
    VARY	*temp,
    USHORT	length)
{
/**************************************
 *
 *	M O V _ g e t _ s t r i n g _ p t r
 *
 **************************************
 *
 * Functional description
 *	Get address and length of string, converting the value to
 *	string, if necessary.  The caller must provide a sufficiently
 *	large temporary.  The address of the resultant string is returned
 *	by reference.  Get_string returns the length of the string.
 *
 *	Note: If the descriptor is known to be a string type, the third
 *	argument (temp buffer) may be omitted.
 *
 **************************************/

return CVT_get_string_ptr (desc, ttype, address, temp, length, (FPTR_VOID) ERR_post);
}

int MOV_get_string (
    DSC		*desc,
    UCHAR	**address,
    VARY	*temp,
    USHORT	length)
{
/**************************************
 *
 *	M O V _ g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
USHORT	ttype;

return MOV_get_string_ptr (desc, &ttype, address, temp, length);
}

GDS_DATE MOV_get_sql_date (
    DSC		*desc)
{
/**************************************
 *
 *	M O V _ g e t _ s q l _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a SQL date
 *
 **************************************/

return CVT_get_sql_date (desc, (FPTR_VOID) ERR_post);
}

GDS_TIME MOV_get_sql_time (
    DSC		*desc)
{
/**************************************
 *
 *	M O V _ g e t _ s q l _ t i m e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a SQL time
 *
 **************************************/

return CVT_get_sql_time (desc, (FPTR_VOID) ERR_post);
}

GDS_TIMESTAMP MOV_get_timestamp (
    DSC		*desc)
{
/**************************************
 *
 *	M O V _ g e t _ t i m e s t a m p
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a timestamp
 *
 **************************************/

return CVT_get_timestamp (desc, (FPTR_VOID) ERR_post);
}

int MOV_make_string (
    DSC		*desc,
    USHORT	ttype,
    UCHAR	**address,
    VARY	*temp,
    USHORT	length)
{
/**************************************
 *
 *	M O V _ m a k e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Make a string, in a specified text type, out of a descriptor.
 *	The caller must provide a sufficiently
 *	large temporary.  The address of the resultant string is returned
 *	by reference.  
 *	MOV_make_string returns the length of the string in bytes.
 *
 *	Note: If the descriptor is known to be a string type in the
 *	given ttype the argument (temp buffer) may be omitted.
 *	But this would be a bad idea in general.
 *
 **************************************/

return CVT_make_string (desc, ttype, address, temp, length, (FPTR_VOID) ERR_post);
}

int MOV_make_string2 (
    DSC		*desc,
    USHORT	ttype,
    UCHAR	**address,
    VARY	*temp,
    USHORT	length,
    STR		*ptr)
{
/**************************************
 *
 *	M O V _ m a k e _ s t r i n g 2
 *
 **************************************
 *
 * Functional description
 *	Make a string, in a specified text type, out of a descriptor.
 *	The caller provides a temporary, and a pointer to a pointer
 *	to hold a CVT_ allocated data structure.
 *	Should CVT_ allocate memory, it is the caller's responsibility to
 *	free it with gds__free().
 *	The address of the resultant string is returned.
 *	MOV_make_string2 returns the length of the string in bytes.
 *
 *	Note: If the descriptor is known to be a string type in the
 *	given ttype the argument (temp buffer) may be omitted.
 *	But this would be a bad idea in general.
 *
 **************************************/

return CVT2_make_string2 (desc, ttype, address, temp, length, ptr, (FPTR_VOID) ERR_post);
}

void MOV_move (
     DSC	*from,
     DSC	*to)
{
/**************************************
 *
 *	M O V _ m o v e
 *
 **************************************
 *
 * Functional description
 *	Move (and possible convert) something to something else.
 *
 **************************************/

CVT_move (from, to, (FPTR_VOID) ERR_post);
}

void MOV_time_stamp (
    GDS_TIMESTAMP	*date)
{
/**************************************
 *
 *	M O V _ t i m e _ s t a m p
 *
 **************************************
 *
 * Functional description
 *	Get the current timestamp in gds format.
 *
 **************************************/
SLONG		clock;
struct tm	times;

clock = time (NULL);
times = *localtime (&clock);
isc_encode_timestamp (&times, date);
}
