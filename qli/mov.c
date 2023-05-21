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

#include "../jrd/ib_stdio.h"
#include <string.h>
#ifndef PYXIS
#include "../qli/dtr.h"
#else
#include "../pyxis/pyxis.h"
#endif
#include "../jrd/time.h"
#include "../jrd/intl.h"
#if (defined JPN_SJIS || defined JPN_EUC)
#include "../jrd/kanji.h"
#endif
#ifndef PYXIS
#include "../qli/err_proto.h"
#endif
#include "../jrd/gds_proto.h"

#ifdef apollo
#include <apollo/base.h>
#include <apollo/cal.h>
#endif

#ifdef PYXIS
#define MOVQ_compare		MOVP_compare
#define MOVQ_date_to_double	MOVP_date_to_double
#define MOVQ_decompose		MOVP_decompose
#define MOVQ_double_to_date	MOVP_double_to_date
#define MOVQ_fast		MOVP_fast
#define MOVQ_get_double		MOVP_get_double
#define MOVQ_get_long		MOVP_get_long
#define MOVQ_get_string		MOVP_get_string
#define MOVQ_move		MOVP_move
#define MOVQ_terminate		MOVP_terminate
#endif

typedef struct vary {
    USHORT	vary_length;
    TEXT	vary_string[1];
} VARY;

#include "../qli/mov_proto.h"

static void	date_error (TEXT *, USHORT);
static void	timestamp_to_text (SLONG [2], DSC *);
static void	sql_time_to_text (ULONG [1], DSC *);
static void	sql_date_to_text (SLONG [1], DSC *);
#ifndef PYXIS
static void	mover_error (int, USHORT, USHORT);
#endif
static void	now_to_date (struct tm *, SLONG [2]);
static void	numeric_to_text (DSC *, DSC *);
static void	string_to_date (TEXT *, USHORT, SLONG [2]);
static void	string_to_time (TEXT *, USHORT, SLONG [2]);
#ifndef PYXIS
static TEXT	*type_name (USHORT);
#endif

#define LETTER(c)	(c >= 'A' && c <= 'Z')
#define DIGIT(c)	(c >= '0' && c <= '9')
#define TODAY		"TODAY"
#define NOW		"NOW"
#define TOMORROW	"TOMORROW"
#define YESTERDAY	"YESTERDAY"

#define PRECISION	10000

static TEXT	*months [] =
    {
    "JANUARY",
    "FEBRUARY",
    "MARCH",
    "APRIL",
    "MAY",
    "JUNE",
    "JULY",
    "AUGUST",
    "SEPTEMBER",
    "OCTOBER",
    "NOVEMBER",
    "DECEMBER",
    0
    };

typedef struct dtypes_t {
    USHORT	type;
    TEXT	*description;
} *DTYPES;

static struct dtypes_t dtypes_table [] =
    {
    dtype_null, 	"NULL",
    dtype_text, 	"character string",
    dtype_cstring,	"character string",
    dtype_varying,	"varying string",
    dtype_packed,	"packed decimal",
    dtype_byte,		"byte integer",
    dtype_short,	"short integer",
    dtype_long,		"long integer",
    dtype_quad,		"quadword integer",
    dtype_real,		"single precision floating",
    dtype_double,	"double precision floating",
    dtype_d_float,	"double precision floating",
    dtype_timestamp,	"date",
    dtype_sql_date,	"SQL date",
    dtype_sql_time,	"SQL time",
    dtype_blob,		"blob",
    0, 0
    };

#ifndef PYXIS
int MOVQ_compare (
    DSC	*arg1,
    DSC	*arg2)
{
/**************************************
 *
 *	M O V Q _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *	Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/
DSC	desc;
UCHAR	*p1, *p2;
SSHORT	length, length2, fill;
SLONG	date[2];

/* Handle the simple (matched) ones first */

if (arg1->dsc_dtype == arg2->dsc_dtype && 
    arg1->dsc_scale == arg2->dsc_scale)
    {
    p1 = arg1->dsc_address;
    p2 = arg2->dsc_address;

    switch (arg1->dsc_dtype)
	{
	case dtype_short:
	    if (*(SSHORT *) p1 == *(SSHORT *) p2)
		return 0;
	    if (*(SSHORT *) p1 > *(SSHORT *) p2)
		return 1;
	    return -1;

	case dtype_long:
	case dtype_sql_date:
	case dtype_sql_time:
	    if (*(SLONG*) p1 == *(SLONG*) p2)
		return 0;
	    if (*(SLONG*) p1 > *(SLONG*) p2)
		return 1;
	    return -1;

	case dtype_timestamp:
	case dtype_quad:
	    if (((SLONG*) p1) [0] < ((SLONG*) p2) [0])
		return -1;
	    if (((SLONG*) p1) [0] > ((SLONG*) p2) [0])
		return 1;
	    if (((ULONG*) p1) [1] < ((ULONG*) p2) [1])
		return -1;
	    if (((ULONG*) p1) [1] > ((ULONG*) p2) [1])
		return 1;
	    return 0;

	case dtype_real:
	    if (*(float *) p1 == *(float *) p2)
		return 0;
	    if (*(float *) p1 > *(float *) p2)
		return 1;
	    return -1;

	case dtype_double:
	    if (*(double *) p1 == *(double *) p2)
		return 0;
	    if (*(double *) p1 > *(double *) p2)
		return 1;
	    return -1;

	case dtype_text:
	    if (arg1->dsc_length >= arg2->dsc_length)
		{
		if (length = arg2->dsc_length)
		    do
			if (*p1++ != *p2++)
			    if (p1 [-1] > p2 [-1])
				return 1;
			    else
				return -1;
		    while (--length);
		if (length = arg1->dsc_length - arg2->dsc_length)
		    do
			if (*p1++ != ' ')
			    if (p1 [-1] > ' ')
				return 1;
			    else
				return -1;
		    while (--length);
		return 0;
		}
	    if (length = arg1->dsc_length)
		do
		    if (*p1++ != *p2++)
			if (p1 [-1] > p2 [-1])
			    return 1;
			else
			    return -1;
		while (--length);
	    length = arg2->dsc_length - arg1->dsc_length;
	    do
		if (*p2++ != ' ')
		    if (' ' > p2 [-1])
			return 1;
		    else
			return -1;
	    while (--length);
	    return 0;
	}
    }

/* Handle mixed string comparisons */

if (arg1->dsc_dtype <= dtype_varying && arg2->dsc_dtype <= dtype_varying)
    {
    length = MOVQ_get_string (arg1, &p1, NULL_PTR, 0);
    length2 = MOVQ_get_string (arg2, &p2, NULL_PTR, 0);
    fill = length - length2;
    if (length >= length2)
	{
	if (length2)
	    do
		if (*p1++ != *p2++)
		    if (p1 [-1] > p2 [-1])
			return 1;
		    else
			return -1;
	    while (--length2);
	if (fill > 0)
	    do
		if (*p1++ != ' ')
		    if (p1 [-1] > ' ')
			return 1;
		    else
			return -1;
	    while (--fill);
	return 0;
	}
    if (length) 
	do
	    if (*p1++ != *p2++)
		if (p1 [-1] > p2 [-1])
		    return 1;
		else
		    return -1;
	while (--length);
    do
	if (*p2++ != ' ')
	    if (' ' > p2 [-1])
		return 1;
	    else
		return -1;
    while (++fill);
    return 0;
    }

/* Handle hetergeneous compares */

if (arg1->dsc_dtype < arg2->dsc_dtype)
    return (- MOVQ_compare (arg2, arg1));

switch (arg1->dsc_dtype)
    {
    case dtype_timestamp:
	desc.dsc_dtype = dtype_timestamp;
	desc.dsc_length = sizeof (date);
	desc.dsc_scale = 0;
	desc.dsc_address = (UCHAR*) date;
	desc.dsc_sub_type = 0;
	MOVQ_move (arg2, &desc);
	return MOVQ_compare (arg1, &desc);

    case dtype_sql_time:
	desc.dsc_dtype = dtype_sql_time;
	desc.dsc_length = sizeof (date[0]);
	desc.dsc_scale = 0;
	desc.dsc_address = (UCHAR*) date;
	desc.dsc_sub_type = 0;
	MOVQ_move (arg2, &desc);
	return MOVQ_compare (arg1, &desc);

    case dtype_sql_date:
	desc.dsc_dtype = dtype_sql_date;
	desc.dsc_length = sizeof (date[0]);
	desc.dsc_scale = 0;
	desc.dsc_address = (UCHAR*) date;
	desc.dsc_sub_type = 0;
	MOVQ_move (arg2, &desc);
	return MOVQ_compare (arg1, &desc);

    case dtype_short:
    case dtype_long:
	{
	SSHORT	scale;
	SLONG	temp1, temp2;

#if (defined JPN_SJIS || defined JPN_EUC)

	/* For character data, scale contains interpretation of the string */

        SSHORT scale1, scale2;

        scale1 = arg1->dsc_scale;
        scale2 = arg2->dsc_scale;
        if (arg1->dsc_dtype <= dtype_varying) scale1 = 0;
        if (arg2->dsc_dtype <= dtype_varying) scale2 = 0;
        scale = MIN (scale1, scale2);

#else

        scale = MIN (arg1->dsc_scale, arg2->dsc_scale);


#endif
	temp1 = MOVQ_get_long (arg1, scale);
	temp2 = MOVQ_get_long (arg2, scale);
	if (temp1 == temp2)
	    return 0;
	if (temp1 > temp2)
	    return 1;
	return -1;
	}

    case dtype_real:
	{
	float	temp1, temp2;

	temp1 = MOVQ_get_double (arg1);
	temp2 = MOVQ_get_double (arg2);
	if (temp1 == temp2)
	    return 0;
	if (temp1 > temp2)
	    return 1;
	return -1;
	}

     case dtype_double:
	{
	double	temp1, temp2;

	temp1 = MOVQ_get_double (arg1);
	temp2 = MOVQ_get_double (arg2);
	if (temp1 == temp2)
	    return 0;
	if (temp1 > temp2)
	    return 1;
	return -1;
	}
    
    case dtype_blob:
	IBERROR (48); /* Msg 48 Blob conversion is not supported */

    default: 
	mover_error (410, arg1->dsc_dtype, arg2->dsc_dtype);
 
   }
}
#endif

double PASCAL_ROUTINE MOVQ_date_to_double (
    DSC		*desc)
{
/**************************************
 *
 *	M O V Q _ d a t e _ t o _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *    Convert a date to double precision for
 *    date arithmetic routines.
 *
 **************************************/
DSC	temp_desc;
SLONG	temp [2], *date;

/* If the input descriptor is not in date form, convert it. */

if (desc->dsc_dtype == dtype_timestamp)
    date = (SLONG*) desc->dsc_address;
else
    {
    temp_desc.dsc_dtype = dtype_timestamp;
    temp_desc.dsc_length = sizeof (temp);
    temp_desc.dsc_scale = 0;
    temp_desc.dsc_sub_type = 0;
    date = temp;
    temp_desc.dsc_address = (UCHAR*) date;
    MOVQ_move (desc, &temp_desc);
    }

return date [0] + (double) date [1] / (24. * 60. * 60. * PRECISION);
}

int MOVQ_decompose (
    TEXT	*string,
    USHORT	length,
    SLONG	*return_value)
{
/**************************************
 *
 *	M O V Q _ d e c o m p o s e
 *
 **************************************
 *
 * Functional description
 *	Decompose a numeric string in mantissa and exponent.
 *
 **************************************/
TEXT	*p, *end;
SSHORT	scale, exp, fraction, sign;
SLONG	value;
SCHAR	temp [128];

scale = fraction = sign = value = 0;

for (p = string, end = p + length; p < end; p++)
    {
    if (*p == ',')
	continue;
    else if (DIGIT (*p))
	{
	value = value * 10 + *p - '0';
	if (fraction)
	    --scale;
	}
    else if (*p == '.')
	if (fraction)
	    {
#ifndef PYXIS
	    MOVQ_terminate (string, temp, length, sizeof (temp));
	    ERRQ_error (411, temp, NULL, NULL, NULL, NULL);
#else
	    IBERROR ("conversion error");
#endif
	    }
	else
	    fraction = TRUE;
    else if (*p == '-' && !value && !sign)
	sign = TRUE;
    else if (*p == '+' && !value && !sign)
	sign = FALSE;
    else if (*p == 'e' || *p == 'E')
	break;
    else if (*p != ' ')
        {
#ifndef PYXIS
	MOVQ_terminate (string, temp, length, sizeof (temp));
	ERRQ_error (411, temp, NULL, NULL, NULL, NULL);
#else
	IBERROR ("conversion error");
#endif
        }
    }

if (sign)
    value = -value;

/* If there's still something left, there must be an explicit
   exponent */

if (p < end)
    {
    sign = exp = 0;
    for (p++; p < end; p++)
	{
	if (DIGIT (*p))
	    exp = exp * 10 + *p - '0';
	else if (*p == '-' && !exp)
	    sign = TRUE;
	else if (*p == '+' && !exp && !sign)
	    continue;
	else if (*p != ' ')
	    {
#ifndef PYXIS
	    MOVQ_terminate (string, temp, length, sizeof (temp));
	    ERRQ_error (411, temp, NULL, NULL, NULL, NULL);
#else
	    IBERROR ("conversion error");
#endif
	    }
	}
    if (sign)
	scale -= exp;
    else
	scale += exp;
    }

*return_value = value;

return scale;
}

void MOVQ_double_to_date (
    double	real,
    SLONG	fixed [2])
{
/**************************************
 *
 *	M O V Q _ d o u b l e _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert a double precision representation of a date
 *	to a fixed point representation.   Double is used for
 *      date arithmetic.
 *
 **************************************/

fixed [0] = real;
fixed [1] = (real - fixed [0]) * 24. * 60. * 60. * PRECISION;
}

void MOVQ_fast (
    register SCHAR	*from,
    register SCHAR	*to,
    register ULONG	length)
{
/**************************************
 *
 *	M O V Q _ f a s t
 *
 **************************************
 *
 * Functional description
 *	Move a byte string as fast as possible.
 *
 **************************************/

if (length)
    do *to++ = *from++; while (--length);
}

double PASCAL_ROUTINE MOVQ_get_double (
    DSC		*desc)
{
/**************************************
 *
 *	M O V Q _ g e t _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a
 *	double_precision representation. 
 *
 **************************************/
double	value;
SSHORT	scale, fraction, sign, exp, length;
TEXT	*p, *end;

switch (desc->dsc_dtype)
    {
    case dtype_short:
	value = *((SSHORT *) desc->dsc_address);
	break;

    case dtype_long:
	value = *((SLONG *) desc->dsc_address);
	break;

    case dtype_real:
	return *((float *) desc->dsc_address);
    
    case dtype_double:
	return *((double *) desc->dsc_address);
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	length = MOVQ_get_string (desc, &p, NULL_PTR, 0);
	scale = fraction = sign = value = 0;
	for (end = p + length; p < end; p++)
	    {
	    if (*p == ',')
		continue;
	    else if (DIGIT (*p))
		{
		value = value * 10. + (*p - '0');
		if (fraction)
		    scale++;
		}
	    else if (*p == '.')
		if (fraction)
#ifndef PYXIS
		    IBERROR (52);   /* Msg 52 conversion error */
#else
		    IBERROR ("conversion error");
#endif
		else
		    fraction = 1;
	    else if (!value && *p == '-')
		sign = 1; 
	    else if (!value && *p == '+')
		continue;
	    else if (*p == 'e' || *p == 'E')
		break;
	    else if (*p != ' ')
#ifndef PYXIS
		IBERROR (53);   /* Msg 53 conversion error */
#else
		IBERROR ("conversion error");
#endif
	    }

	if (sign)
	    value = -value;

	/* If there's still something left, there must be an explicit
	   exponent */

	if (p < end)
	    {
	    sign = exp = 0;
	    for (p++; p < end; p++)
		{
		if (DIGIT (*p))
		    exp = exp * 10 + *p - '0';
		else if (*p == '-' && !exp)
		    sign = TRUE;
		else if (*p == '+' && !exp)
		    ;
		else if (*p != ' ')
#ifndef PYXIS
		    IBERROR (54);  /* Msg 54 conversion error */
#else
		    IBERROR ("conversion error");
#endif
		}
	    if (sign)
		scale += exp;
	    else
		scale -= exp;
	    }

	if (scale > 0)
	    do value /= 10.; while (--scale);
	else if (scale)
	    do value *= 10.; while (++scale);

	return value;
    
    default:
#ifndef PYXIS
	mover_error (410, desc->dsc_dtype, dtype_double);
#else
	BUGCHECK ("conversion not implemented");
#endif
    }

/* Last, but not least, adjust for scale */

if ((scale = desc->dsc_scale) == 0)
    return value;

if (scale > 0)
    do value *= 10.; while (--scale);
else
    do value /= 10.; while (++scale);

return value;
}

SLONG MOVQ_get_long (
    DSC		*desc,
    SSHORT	scale)
{
/**************************************
 *
 *	M O V Q _ g e t _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Convert something arbitrary to a long (32 bit) integer of given
 *	scale.
 *
 **************************************/
SLONG	value;
double	d;
SSHORT	fraction, sign, l, length;
TEXT	*p;

#if (!(defined JPN_SJIS || defined JPN_EUC))

scale -= (SSHORT) desc->dsc_scale;

#else

/* For character data, scale field is used for interpretation of the string */

if (desc->dsc_dtype > dtype_varying)
    scale -= desc->dsc_scale;

#endif
p = (TEXT*) desc->dsc_address;
switch (desc->dsc_dtype)
    {
    case dtype_short:
	value = *((SSHORT *) p);
	break;

    case dtype_long:
	value = *((SLONG *) p);
	break;

    case dtype_real:
	d = *((float *) p);
	if (scale > 0)
	    do d /= 10.; while (--scale);
	else if (scale < 0)
	    do d *= 10.; while (++scale);
	if (d > 0)
	    d += 0.5;
	else
	    d -= 0.5;
	return (SLONG) d;
    
    case dtype_double:
	d = *((double *) p);
	if (scale > 0)
	    do d /= 10.; while (--scale);
	else if (scale < 0)
	    do d *= 10.; while (++scale);
	if (d > 0)
	    d += 0.5;
	else
	    d -= 0.5;
	return (SLONG) d;
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	length = MOVQ_get_string (desc, &p, NULL_PTR, 0);
	scale -= MOVQ_decompose (p, length, &value);
	break;
    
    default:
#ifndef PYXIS
	mover_error (410, desc->dsc_dtype, dtype_long);
#else
	BUGCHECK ("conversion not implemented");
#endif
    }

/* Last, but not least, adjust for scale */

if (scale == 0)
    return value;

if (scale > 0)
    {
    if ((desc->dsc_dtype == dtype_short) || (desc->dsc_dtype == dtype_long))
        {
        fraction = 0;
        do {
            if (scale == 1)
                fraction = value % 10;
            value /= 10;
           } while (--scale);
        if (fraction > 4)
            value++;
        }
    else
        do value /= 10; while (--scale);
    }
else
    do value *= 10; while (++scale);

return value;
}

int MOVQ_get_string (
    DSC		*desc,
    TEXT	**address,
    VARY	*temp,
    USHORT	length)
{
/**************************************
 *
 *	M O V Q _  g e t _ s t r i n g
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
VARY	*varying;
DSC	temp_desc;

/* If the value is already a string (fixed or varying), just return
   the address and length. */

if (desc->dsc_dtype == dtype_text)
    {
    *address = (TEXT*) desc->dsc_address;
    return desc->dsc_length;
    }

/* Perhaps it a "C" type string? */

if (desc->dsc_dtype == dtype_cstring)
    {
    *address = (TEXT*) desc->dsc_address;
    return MIN (strlen (desc->dsc_address), desc->dsc_length - 1);
    }

/* No luck -- convert value to varying string. */

if (desc->dsc_dtype == dtype_varying)
    {
    varying = (VARY*) desc->dsc_address;
    *address = varying->vary_string;
    return varying->vary_length;
    }

temp_desc.dsc_length = length;
temp_desc.dsc_address = (UCHAR*) temp;
temp_desc.dsc_scale = 0;
temp_desc.dsc_dtype = dtype_varying;
temp_desc.dsc_sub_type = ttype_ascii;
MOVQ_move (desc, &temp_desc);
*address = temp->vary_string;

return temp->vary_length;
}

void MOVQ_move (
     DSC	*from,
     DSC	*to)
{
/**************************************
 *
 *	M O V Q _ m o v e
 *
 **************************************
 *
 * Functional description
 *	Move (and possible convert) something to something else.
 *
 **************************************/
register UCHAR	*p, *q;
register SSHORT	length, fill;
SLONG		l;
UCHAR		*ptr;

length = from->dsc_length;
p = to->dsc_address;
q = from->dsc_address;

/* If the datatypes and lengths are identical, just move the
   stuff byte by byte.  Although this may seem slower than 
   optimal, it would cost more to find the fast move than the
   fast move would gain. */

if (DSC_EQUIV (from, to))
/*
if (((ALT_DSC*) from)->dsc_combined_type == ((ALT_DSC*) to)->dsc_combined_type)
*/
    {
    if (length)
	do *p++ = *q++; while (--length);
    return;
    }

/* Do data type by data type conversions.  Not all are supported,
   and some will drop out for additional handling. */

switch (to->dsc_dtype)
    {
    case dtype_timestamp:
	switch (from->dsc_dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
		length = MOVQ_get_string (from, &ptr, NULL_PTR, 0);
		string_to_date (ptr, length, to->dsc_address);
		return;
	    case dtype_sql_date:
		((SLONG *)to->dsc_address)[0] = *(SLONG *)from->dsc_address;
		((SLONG *)to->dsc_address)[1] = 0;
		return;
	    case dtype_sql_time:
		((SLONG *)to->dsc_address)[0] = 0;
		((SLONG *)to->dsc_address)[1] = *(SLONG *)from->dsc_address;
		return;
	    }
	break;

    case dtype_sql_date:
	switch (from->dsc_dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
		{
		SLONG	date [2];
		length = MOVQ_get_string (from, &ptr, NULL_PTR, 0);
		string_to_date (ptr, length, date);
		((SLONG *)to->dsc_address)[0] = date [0];
		}
		return;
	    case dtype_timestamp:
		((SLONG *)to->dsc_address)[0] = ((SLONG *)from->dsc_address)[0];
		return;
	    case dtype_sql_time:
		/* Error situation */
		break;
	    }
	break;

    case dtype_sql_time:
	switch (from->dsc_dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
		{
		SLONG	date [2];
		length = MOVQ_get_string (from, &ptr, NULL_PTR, 0);
		string_to_time (ptr, length, date);
		((SLONG *)to->dsc_address)[0] = date [1];
		}
		return;
	    case dtype_timestamp:
		((SLONG *)to->dsc_address)[0] = ((SLONG *)from->dsc_address)[1];
		return;
	    case dtype_sql_date:
		/* Error situation */
		break;
	    }
	break;

    case dtype_text:
    case dtype_cstring:
    case dtype_varying:
	switch (from->dsc_dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
		length = MOVQ_get_string (from, &ptr, NULL_PTR, 0);
		q = ptr;
		switch (to->dsc_dtype)
		    {
		    case dtype_text:
			length = MIN (length, to->dsc_length);
#if (defined JPN_SJIS || defined JPN_EUC)

                        /* Check truncation of a double byte character */

                        if (KANJI_check(q,length))
			   length -= 1;
#endif
			fill = to->dsc_length - length;
			if (length)
			    do *p++ = *q++; while (--length);
			if (fill > 0)
			    do *p++ = ' '; while (--fill);
			return;

		    case dtype_cstring:
			length = MIN (length, to->dsc_length - 1);
#if (defined JPN_SJIS || defined JPN_EUC)

                        /* Check truncation of a double byte character */

                        if (KANJI_check(q,length))
			   length -= 1;
#endif
			if (length)
			    do *p++ = *q++; while (--length);
			*p = 0;
			return;

		    case dtype_varying:
			length = MIN (length, to->dsc_length - sizeof (SSHORT));
#if (defined JPN_SJIS || defined JPN_EUC)

                        /* Check truncation of a double byte character */

                        if (KANJI_check(q,length))
			    length -= 1;
#endif
			((VARY*) p)->vary_length = length;
			p = (UCHAR*) ((VARY*) p)->vary_string;
			if (length)
			    do *p++ = *q++; while (--length);
			return;
		    }

	    case dtype_short:
	    case dtype_long:
	    case dtype_real:
	    case dtype_double:
		numeric_to_text (from, to);
		return;

	    case dtype_sql_date:
		sql_date_to_text (from->dsc_address, to);
		return;
	    case dtype_sql_time:
		sql_time_to_text (from->dsc_address, to);
		return;
	    case dtype_timestamp:
		timestamp_to_text (from->dsc_address, to);
		return;
	    }
	break;

    case dtype_blob:
	if (from->dsc_dtype == dtype_quad)
	    {
	    ((SLONG *) p)[0] = ((SLONG *) q)[0];
	    ((SLONG *) p)[1] = ((SLONG *) q)[1];
	    return;
	    }
	break;

    case dtype_quad:
	if (from->dsc_dtype == dtype_blob)
	    {
	    ((SLONG *) p)[0] = ((SLONG *) q)[0];
	    ((SLONG *) p)[1] = ((SLONG *) q)[1];
	    return;
	    }
	break;

    case dtype_short:
	*(SSHORT *) p = l = MOVQ_get_long (from, to->dsc_scale);
	if (*(SSHORT*) p != l)
#ifndef PYXIS
	    IBERROR (14); /* Msg14 integer overflow */
#else
	    IBERROR ("integer overflow");
#endif
	return;

    case dtype_long:
	*(SLONG *) p  = MOVQ_get_long (from, to->dsc_scale);
	return;

    case dtype_real:
	*(float *) p = MOVQ_get_double (from);
	return;

    case dtype_double:
	*(double *) p = MOVQ_get_double (from);
	return;
    }

if (to->dsc_dtype == dtype_blob || from->dsc_dtype == dtype_blob)
#ifndef PYXIS
    IBERROR (55); /* Msg 55 Blob conversion is not supported */

mover_error (410, from->dsc_dtype, to->dsc_dtype);
#else
    IBERROR ("Blob conversion is not supported");

BUGCHECK ("MOVP_move: conversion not done");
#endif
}

void MOVQ_terminate (
    register SCHAR	*from,
    register SCHAR	*to,
    register SSHORT	length,
    SSHORT		max_length)
{
/**************************************
 *
 *	M O V Q _ t e r m i n a t e
 *
 **************************************
 *
 * Functional description
 *	Null-terminate a possibly non-
 *	null-terminated string with max
 *	buffer room.
 *
 **************************************/

#if (defined JPN_SJIS || defined JPN_EUC)

/* No need to fix this routine.
   It is used safely everywhere.  */

#endif
             
if (length)                   
    {
    length = MIN (length, max_length - 1);
    do *to++ = *from++; while (--length);
    *to++ = '\0';
    }
else
    {
    while (max_length-- && (*to++ = *from++))
	;
    *--to = '\0';
    }
}

static void date_error (
    TEXT	*string,
    USHORT	length)
{
/**************************************
 *
 *	d a t e _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	A date conversion error occurred.  Complain.
 *
 **************************************/
#ifndef PYXIS
SCHAR	temp [128];

MOVQ_terminate (string, temp, length, sizeof (temp));
ERRQ_error (56, temp, NULL, NULL, NULL, NULL); /* Msg 56 Error converting string \"%s\" to date */
#else
SCHAR	s [256];

sprintf (s, "Error converting string \"%.*s\" to date", length, string);
IBERROR (s);
#endif
}

static void sql_date_to_text (
    SLONG	date [1],
    DSC		*to)
{
/**************************************
 *
 *	s q l _ d a t e _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Convert date to text.
 *
 **************************************/
DSC		desc;
TEXT		*p, temp[35];
struct tm	times;
SLONG		date2[2];

date2 [0] = date;
date2 [1] = 0;
isc_decode_date (date2, &times);
sprintf (temp, "%2d-%.3s-%04d", times.tm_mday,
        months [times.tm_mon], times.tm_year + 1900);

for (p = temp; *p; p++)
    ;

desc.dsc_length = p - temp;
desc.dsc_address = (UCHAR*) temp;
desc.dsc_dtype = dtype_text;
desc.dsc_scale = 0;
desc.dsc_sub_type = ttype_ascii;

MOVQ_move (&desc, to);
}

static void sql_time_to_text (
    ULONG	date [1],
    DSC		*to)
{
/**************************************
 *
 *	s q l _ t i m e _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Convert sql time to text.
 *
 **************************************/
DSC		desc;
TEXT		*p, temp[35];
struct tm	times;
SLONG		date2[2];

date2 [0] = 0;
date2 [1] = date;

isc_decode_date (date2, &times);

sprintf (temp, " %2d:%.2d:%.2d.%.4d", times.tm_hour, times.tm_min, times.tm_sec,
		date2[1] % PRECISION);

for (p = temp; *p; p++)
    ;

desc.dsc_length = p - temp;
desc.dsc_address = (UCHAR*) temp;
desc.dsc_dtype = dtype_text;
desc.dsc_scale = 0;
desc.dsc_sub_type = ttype_ascii;

MOVQ_move (&desc, to);
}

static void timestamp_to_text (
    SLONG	date [2],
    DSC		*to)
{
/**************************************
 *
 *	t i m e s t a m p _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Convert date to text.
 *
 **************************************/
DSC		desc;
TEXT		*p, temp[35], time[15];
struct tm	times;

isc_decode_date (date, &times);
#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

/* By default the Japanese date format is yyyy.mm.dd */

sprintf (temp, "%04d.%02d.%02d", times.tm_year + 1900, times.tm_mon + 1,
        times.tm_mday);

#else

sprintf (temp, "%2d-%.3s-%04d", times.tm_mday,
        months [times.tm_mon], times.tm_year + 1900);
#endif

if (times.tm_hour || times.tm_min || times.tm_sec || date[1])
    {
    sprintf (time, " %2d:%.2d:%.2d.%.4d", times.tm_hour, times.tm_min, times.tm_sec,
		date[1] % PRECISION);
    strcat (temp, time);
    }

for (p = temp; *p; p++)
    ;

desc.dsc_length = p - temp;
desc.dsc_address = (UCHAR*) temp;
desc.dsc_dtype = dtype_text;
desc.dsc_scale = 0;
desc.dsc_sub_type = ttype_ascii;

MOVQ_move (&desc, to);
}

#ifndef PYXIS
static void mover_error (
    int		pattern,
    USHORT	in_type,
    USHORT	out_type)
{
/**************************************
 *
 *	m o v e r _ e r r o r 
 *
 **************************************
 *
 * Functional description
 *	Return a reasonable error for
 *	unreasonable conversions and
 *	comparisons.
 *
 **************************************/
TEXT	*in, *out, in_name [25], out_name [25], msg_unknown [40];

ERRQ_msg_get (504, msg_unknown);	/* Msg504 unknown datatype %d	*/

if (!(in = type_name (in_type)))
    {
    in = in_name;
    sprintf (in_name, msg_unknown, in_type); 
    }

if (!(out = type_name (out_type)))
    {
    out = out_name;
    sprintf (out, msg_unknown, out_type); 
    }

ERRQ_error (pattern, (TEXT*) in, (TEXT*) out, NULL, NULL, NULL);
}
#endif
 
static void now_to_date (
    struct tm	*time,
    SLONG	date [2])
{
/**************************************
 *
 *	n o w _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert "now" (or best guess) to
 *	a date attempting to get millisecond
 *	precision.  This unfortunately means
 *	that we use one routine for VAX, one
 *	for Apollo, and a third for Unix.
 *
 **************************************/

#ifndef apollo
isc_encode_date (time, date);

#else

time_$clock_t		clock, clock1, clock2;
cal_$timedate_rec_t	d_clock;

cal_$get_local_time (&clock1);
cal_$decode_time (clock1, &d_clock);
cal_$encode_time (d_clock, &clock2);
cal_$sub_clock (&clock1, clock2);

time->tm_year = d_clock.year - 1900;
time->tm_mon = d_clock.month -1;
time->tm_mday = d_clock.day;
time->tm_hour = d_clock.hour;
time->tm_min = d_clock.minute;
time->tm_sec = d_clock.second;
time->tm_wday = 0;
time->tm_yday = 0;

isc_encode_date (time, date);
date[1] += (clock1.c2.low32 * 4) / 100; 

#endif
}

static void numeric_to_text (
    DSC	*from,
    DSC	*to)
{
/**************************************
 *
 *	n u m e r i c _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Convert your basic number to nice, formatted text.
 *
 **************************************/
DSC	intermediate;
SLONG	n;
TEXT	temp[32], *p, *q;
SSHORT	l, scale, neg, pad, decimal, length;

pad = decimal = neg = 0;

/* Save (or compute) scale of source.  Then convert source to ordinary
   longword. */

scale = from->dsc_scale;

if (scale > 0)
    pad = scale;
else if (scale < 0)
    decimal = 1;

intermediate.dsc_dtype = dtype_long;
intermediate.dsc_length = sizeof (SLONG);
intermediate.dsc_scale = scale;
intermediate.dsc_sub_type = 0;
intermediate.dsc_address = (UCHAR*) &n;

MOVQ_move (from, &intermediate);

/* Check for negation, then convert the number to a string of digits */

if (n < 0)
    {
    neg = 1;
    n = -n;
    }

p = temp;

do {
    *p++ = n % 10 + '0';
    n /= 10;
} while (n);

/* Compute the total length off the field formatted.  Make sure it
   fits.  Keep in mind that routine handles both string and varying
   string fields. */

l = p - temp;
length = l + neg + decimal + pad;

if ((to->dsc_dtype == dtype_text    && length > to->dsc_length) ||
    (to->dsc_dtype == dtype_cstring && length >= to->dsc_length) ||
    (to->dsc_dtype == dtype_varying && length > to->dsc_length - sizeof (SSHORT)))
#ifndef PYXIS
    IBERROR (57); /* Msg 57 overflow during conversion */
#else
    IBERROR ("overflow during conversion");
#endif

q = (TEXT*) ((to->dsc_dtype == dtype_text) ? to->dsc_address : to->dsc_address + sizeof (SSHORT));

/* If negative, put in minus sign */

if (neg)
    *q++ = '-';

/* If a decimal point is required, do the formatting.  Otherwise just
   copy number */

if (scale < 0)
    {
    if ((l += scale) > 0)
	do *q++ = *--p; while (--l);
    *q++ = '.';
    do *q++ = *--p; while (++scale);
    }
else
    do *q++ = *--p; while (--l);

/* If padding is required, do it now. */

if (pad)
    do *q++ = '0'; while (--pad);

/* Finish up by padding (if fixed) or computing the actual length
   (varying string) */

if (to->dsc_dtype == dtype_text)
    {
    if (l = to->dsc_length - length)
	do *q++ = ' '; while (--l);
    return;
    }

if (to->dsc_dtype == dtype_cstring)
    {
    *q = 0;
    return;
    }

*(SSHORT*) (to->dsc_address) = (UCHAR*) q - to->dsc_address - sizeof (SSHORT);
}

static void string_to_date (
    TEXT	*string,
    USHORT	length,
    SLONG	date [2])
{
/**************************************
 *
 *	s t r i n g _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Convert an arbitrary string to a date.
 *
 **************************************/
TEXT		c, *p, temp [15], *t, *end, **month_ptr, *m;
USHORT		n, month_position, i, components [7], precision, year;
SLONG		clock;
struct tm	times, times2, *today;

if (!length)
    {
    date [0] = date [1] = 0;
    return;
    }

p = string;

#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

/* Make sure there are no Japanese characters */

n = length;
end = p;

while (n--) if (*end++ & KANJI_MASK)
    {
    date_error (string,length);
    return;
    }

#endif

end = p + length;
year = month_position = 0;

clock = time (NULL_PTR);
today = localtime (&clock);

for (i = 0; i < 7; i++)
    components [i] = 0;

/* Parse components */

for (i = 0; i < 7; i++)
    {

    /* Skip leading blanks.  If we run out of characters, we're done
       with parse.  */

    while (p < end && *p == ' ')
	p++;
    if (p == end)
	break;

    /* Handle digit or character strings */

    c = UPPER (*p);
    if (DIGIT (c))
	{
	precision = n = 0;
	while (p < end && DIGIT (*p))
	    {
	    n = n * 10 + *p++ - '0';
	    precision++;
	    }
#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE)
        if (i == 0)
#else
        if (i == 2)
#endif
	    year = TRUE;
	}
    else if (LETTER (c))
	{
	t = temp;
	while (p < end && LETTER (c))
	    {
	    c = UPPER (*p);
	    if (!LETTER (c))
		break;
	    *t++ = c;
	    p++;
	    }
	*t = 0;
	month_ptr = months;
	while (TRUE)
	    {
	    if (!*month_ptr)
		{
		while (++p < end)
		    if (*p != ' ' && *p != '\t' && *p != 0)
			date_error (string, length);

		if (strcmp (temp, NOW) == 0)
		    {
		    now_to_date (today, date);
		    return;
		    }
		today->tm_hour = today->tm_min = today->tm_sec = 0;
		isc_encode_date (today, date);
		if (strcmp (temp, TODAY) == 0)
		    return;
		if (strcmp (temp, TOMORROW) == 0)
		    {
		    ++date [0];
		    return;
		    }
		if (strcmp (temp, YESTERDAY) == 0)
		    {
		    --date [0];
		    return;
		    }
		date_error (string, length);
		}
	    for (t = temp, m = *month_ptr++; *t && *t == *m; t++, m++)
		;
	    if (!*t)
		break;
	    }
	n = month_ptr - months;
	month_position = i;
	}
    else
	{
	date_error (string, length);
	return;
	}
    components [i] = n;
    while (p < end && *p == ' ')
	p++;
#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

    if (*p == '/' || *p == '-' || *p == ',' || *p == ':' || *p == '.')
        {
        p++;
        continue;
        }

#else

    if (*p == '/' || *p == '-' || *p == ',' || *p == ':')
	{
	p++;
	continue;
	}
    if (*p == '.')
	{
	if (!month_position && i < 2)
	    month_position = 1;
	p++;
	continue;
	}

#endif
    }

/* Slide things into day, month, year form */

#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

times.tm_mon = components [1];
times.tm_mday = components [2];

#else

if (month_position)
    {
    times.tm_mon = components [1];
    times.tm_mday = components [0];
    }
else
    {
    times.tm_mon = components [0];
    times.tm_mday = components [1];
    }

#endif

/* Handle defaulting of year */

#if ((defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

if (((times.tm_year = components[0]) == 0) && !year)

#else

if (((times.tm_year = components [2]) == 0) && !year)

#endif
    times.tm_year = today->tm_year + 1900;
else if (times.tm_year < 100)
    {
    if (times.tm_year < (today->tm_year - 50) % 100)
	times.tm_year += 2000;
    else
	times.tm_year += 1900;
    }

times.tm_year -= 1900;
times.tm_mon -= 1;
times.tm_hour = components [3];
times.tm_min = components [4];
times.tm_sec = components [5];

/* convert day/month/year to Julian and validate result */

isc_encode_date (&times, date);
isc_decode_date (date, &times2);

if (times.tm_year != times2.tm_year ||
    times.tm_mon != times2.tm_mon ||
    times.tm_mday != times2.tm_mday)
    date_error (string, length);

while (precision++ < 4)
    components[6] *= 10;

date[1] += components [6];

}

static void string_to_time (
    TEXT	*string,
    USHORT	length,
    SLONG	date [2])
{
/**************************************
 *
 *	s t r i n g _ t o _ t i m e
 *
 **************************************
 *
 * Functional description
 *	Convert an arbitrary string to a t i m e.
 *
 **************************************/
TEXT		c, *p, temp [15], *t, *end;
USHORT		n, i, components [7], precision;
SLONG		clock;
struct tm	times, times2, *today;

if (!length)
    {
    date [0] = date [1] = 0;
    return;
    }

p = string;

end = p + length;

clock = time (NULL_PTR);
today = localtime (&clock);

for (i = 0; i < 7; i++)
    components [i] = 0;

/* Parse components */

for (i = 3; i < 7; i++)
    {

    /* Skip leading blanks.  If we run out of characters, we're done
       with parse.  */

    while (p < end && *p == ' ')
	p++;
    if (p == end)
	break;

    /* Handle digit or character strings */

    c = UPPER (*p);
    if (DIGIT (c))
	{
	precision = n = 0;
	while (p < end && DIGIT (*p))
	    {
	    n = n * 10 + *p++ - '0';
	    precision++;
	    }
	}
    else if (LETTER (c))
	{
	t = temp;
	while (p < end && LETTER (c))
	    {
	    c = UPPER (*p);
	    if (!LETTER (c))
		break;
	    *t++ = c;
	    p++;
	    }
	*t = 0;
	while (++p < end)
	    if (*p != ' ' && *p != '\t' && *p != 0)
		date_error (string, length);

	if (strcmp (temp, NOW) == 0)
	    {
	    now_to_date (today, date);
	    return;
	    }
	date_error (string, length);
	}
    else
	{
	date_error (string, length);
	return;
	}
    components [i] = n;
    while (p < end && *p == ' ')
	p++;

    if (*p == '/' || *p == '-' || *p == ',' || *p == ':' || *p == '.')
        {
        p++;
        continue;
        }

    }

times.tm_hour = components [3];
times.tm_min = components [4];
times.tm_sec = components [5];

/* convert day/month/year to Julian and validate result */

isc_encode_date (&times, date);

while (precision++ < 4)
    components[6] *= 10;

date[1] += components [6];

}

#ifndef PYXIS
static TEXT *type_name (
    USHORT	dtype)
{
/**************************************
 *
 *	t y p e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Return the name of a data type.
 *
 **************************************/
DTYPES	names;

for (names = dtypes_table; names->description; names++)
    if (names->type == dtype)
        return names->description;
return NULL;
}
#endif
