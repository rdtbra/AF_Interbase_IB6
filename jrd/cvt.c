/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         cvt.c
 *      DESCRIPTION:    Data mover and converter and comparator, etc.
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
#include <stdlib.h>
#include "../jrd/time.h"
#ifndef REQUESTER
#include "../jrd/jrd.h"
#else
#include "../jrd/common.h"
#endif
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "../jrd/quad.h"
#include "../jrd/codes.h"
#include "../jrd/intl.h"
#include "../jrd/gdsassert.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/intl_proto.h"

#if !(defined REQUESTER && defined SUPERCLIENT)
#include "../jrd/intlobj.h"
#endif

#ifdef apollo
#include <apollo/base.h>
#include <apollo/cal.h>
#endif

#ifdef sun
#define DBL_MAX_10_EXP          308
#endif
#ifdef DELTA
#define DBL_MAX_10_EXP          308
#endif
#ifdef IMP
#define DBL_MAX_10_EXP          308
#endif
#ifdef SCO_UNIX
#define DBL_MAX_10_EXP          308
#endif
#ifndef DBL_MAX_10_EXP
#include <float.h>
#endif

#ifdef VMS
extern double   MTH$CVT_D_G(), MTH$CVT_G_D();
#endif

/* ERR_post is used to flag whether we were called from mov.c or
   anywhere else CVT is used from (by comparing with param err) */

/* normally the following two definitions are part of limits.h
   but due to a compiler bug on Apollo casting LONG_MIN to be a
   double, these have to be defined as double numbers...

   Also, since SunOS4.0 doesn't want to play with the rest of
   the ANSI world, these definitions have to be included explicitly.
   So, instead of some including <limits.h> and others using these
   definitions, just always use these definitions (huh?) */

#define LONG_MIN_real   -2147483648.    /* min decimal value of an "SLONG" */
#define LONG_MAX_real   2147483647.     /* max decimal value of an "SLONG" */
#define LONG_MIN_int    -2147483648     /* min integer value of an "SLONG" */
#define LONG_MAX_int    2147483647      /* max integer value of an "SLONG" */

/* It turns out to be tricky to write the INT64 versions of those constant in
   a way that will do the right thing on all platforms.  Here we go. */

#define LONG_MAX_int64 ((SINT64)2147483647)  /* max int64 value of an SLONG */
#define LONG_MIN_int64 (-LONG_MAX_int64-1)   /* min int64 value of an SLONG */

#define QUAD_MIN_real   -9223372036854775808.   /* min decimal value of quad */
#define QUAD_MAX_real   9223372036854775807.    /* max decimal value of quad */

#define QUAD_MIN_int    quad_min_int            /* min integer value of quad */
#define QUAD_MAX_int    quad_max_int            /* max integer value of quad */

#define FLOAT_MAX       3.4e38                  /* max float (32 bit) value  */

#ifdef VAX
#define LOW_WORD        0
#define HIGH_WORD       1
#else
#define LOW_WORD        1
#define HIGH_WORD       0
#endif

#define LETTER7(c)      ((c) >= 'A' && (c) <= 'Z')
#define DIGIT(c)        ((c) >= '0' && (c) <= '9')
#define ABSOLUT(x)      ((x) < 0 ? -(x) : (x))

/* The expressions for SHORT_LIMIT, LONG_LIMIT, INT64_LIMIT and
 * QUAD_LIMIT return the largest value that permit you to multiply by
 * 10 without getting an overflow.  The right operand of the << is two
 * less than the number of bits in the type: one bit is for the sign,
 * and the other is because we divide by 5, rather than 10.  */

#define SHORT_LIMIT     ((1 << 14) / 5)
#define LONG_LIMIT      ((1L << 30) / 5)

#define COMMA           ','

/* NOTE: The syntax for the below line may need modification to ensure
 *	 the result of 1 << 62 is a quad
 */
#define QUAD_LIMIT      ((((SINT64) 1) << 62) / 5)
#define INT64_LIMIT     ((((SINT64) 1) << 62) / 5)
#define NUMERIC_LIMIT   (INT64_LIMIT)

#define TODAY           "TODAY"
#define NOW             "NOW"
#define TOMORROW        "TOMORROW"
#define YESTERDAY       "YESTERDAY"

#define CVT_FAILURE_SPACE       128

#define CVT_COPY_BUFF(from,to,len) \
{if (len) {MOVE_FAST(from,to,len); from +=len; to+=len; len=0;} }

typedef enum {
	expect_timestamp,
	expect_sql_date,
	expect_sql_time
	} EXPECT_DATETIME;

#if (defined REQUESTER || defined SUPERCLIENT)
static  TEXT    cvt_failures[CVT_FAILURE_SPACE]; 
static  TEXT    *cvt_failures_ptr = NULL;
static TEXT     *error_string ( TEXT *, SSHORT);
#endif

static void     conversion_error (DSC *, FPTR_VOID);
static void     datetime_to_text (DSC *, DSC *, FPTR_VOID);
static SSHORT   decompose (UCHAR *, USHORT, SSHORT, SLONG *, FPTR_VOID);
static void     float_to_text (DSC *, DSC *, FPTR_VOID);
static void     integer_to_text (DSC *, DSC *, FPTR_VOID);
static void     string_to_datetime (DSC *, GDS_TIMESTAMP *, EXPECT_DATETIME, FPTR_VOID);
static double   power_of_ten (int);

#ifndef NATIVE_QUAD
#ifdef VAX
static CONST SQUAD    quad_min_int = {0, LONG_MIN_int};
static CONST SQUAD    quad_max_int = {-1, LONG_MAX_int};
#else
static CONST SQUAD    quad_min_int = {LONG_MIN_int, 0};
static CONST SQUAD    quad_max_int = {LONG_MAX_int, -1};
#endif
#endif

#ifdef DGUX
#undef CONST
#define CONST
#endif

static CONST TEXT * CONST months [] = {
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
    NULL
};

#ifdef DGUX
#undef CONST
#define CONST const
#endif

#if !defined (NATIVE_QUAD) 
#include "../jrd/quad.c"
#endif

double PASCAL_ROUTINE CVT_date_to_double (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ d a t e _ t o _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *    Convert a date to double precision for
 *    date arithmetic routines.  
 *
 **************************************/
SLONG   temp [2], *date;
volatile double retval;

/* If the input descriptor is not in date form, convert it. */

if (desc->dsc_dtype == dtype_timestamp)
    date = (SLONG*) desc->dsc_address;
else if (desc->dsc_dtype == dtype_sql_time)
    {
	/* Temporarily convert the time to a timestamp for conversion */
    date = temp;
    date [0] = 0;
    date [1] = *(SLONG*) desc->dsc_address;
    }
else if (desc->dsc_dtype == dtype_sql_date)
    {
	/* Temporarily convert the date to a timestamp for conversion */
    date = temp;
    date [0] = *(SLONG*) desc->dsc_address;
    date [1] = 0;
    }
else
    {
	/* Unknown type - most likely a string.  Try to convert it to a 
	   timestamp -- or die trying (reporting an error).
	   Don't worry about users putting a TIME or DATE here - this
	   conversion is occuring in really flexible spots - we don't
	   want to overdo it. */

    DSC     temp_desc;
    MOVE_CLEAR (&temp_desc, sizeof (temp_desc));

    temp_desc.dsc_dtype = dtype_timestamp;
    temp_desc.dsc_length = sizeof (temp);
    date = temp;
    temp_desc.dsc_address = (UCHAR*) date;
    CVT_move (desc, &temp_desc, err);
    }

/* Instead of returning the calculated double value in the return
statement, am assigning the value to a local volatile double
variable and returning that. This is to prevent a specific kind of
precision error caused on Intel platforms (SCO and Linux) due
to FPU register being 80 bits long and double being 64 bits long */ 

retval = date [0] + (double) date [1] / (24. * 60. * 60. * ISC_TIME_SECONDS_PRECISION);
return retval;
}

void CVT_double_to_date (
    double      real,
    SLONG       fixed [2],
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ d o u b l e _ t o _ d a t e
 *
 **************************************
 *
 * Functional description
 *      Convert a double precision representation of a date
 *      to a fixed point representation.   Double is used for
 *      date arithmetic.
 *
 **************************************/

fixed [0] = real;
fixed [1] = (real - fixed [0]) * 24. * 60. * 60. * ISC_TIME_SECONDS_PRECISION;
}

double PASCAL_ROUTINE CVT_get_double (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a double precision number
 *
 **************************************/
double  value;
SSHORT	scale;


switch (desc->dsc_dtype)
    {
    case dtype_short:
	value = *((SSHORT*) desc->dsc_address);
	break;

    case dtype_long:
	value = *((SLONG*) desc->dsc_address);
	break;

    case dtype_quad:
#ifdef NATIVE_QUAD
	value = *((SQUAD*) desc->dsc_address);
#else
	value = ((SLONG*) desc->dsc_address) [HIGH_WORD];
	value *= -((double) LONG_MIN_real);
	if (value < 0)
	    value -= ((ULONG*) desc->dsc_address) [LOW_WORD];
	else
	    value += ((ULONG*) desc->dsc_address) [LOW_WORD];
#endif
	break;

    case dtype_int64:
        value = (double) *((SINT64 *) desc->dsc_address);
	break;

    case dtype_real:
	return *((float*) desc->dsc_address);
    
    case DEFAULT_DOUBLE:
	/* a MOVE_FAST is done in case dsc_address is on a platform dependant
	   invalid alignment address for doubles */
	MOVE_FAST( desc->dsc_address, &value, sizeof( double));
	return value;

#ifdef VMS
    case SPECIAL_DOUBLE:
	return CNVT_TO_DFLT ((double*) desc->dsc_address);
#endif
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	{
	TEXT    buffer [50];    /* must hold ascii of largest double */
	SSHORT  fraction, sign, exp, length, past_sign, digit_seen;
	SCHAR   *p, *end;

	length = CVT_make_string (desc, ttype_ascii, &p, buffer, sizeof (buffer), err);
	digit_seen = past_sign = scale = fraction = sign = value = exp = 0;
	for (end = p + length; p < end; p++)
	    if (*p == COMMA)
		continue;
	    else if (DIGIT (*p))
		{      
		digit_seen = 1;
		past_sign = 1;
		if (fraction)
		    scale ++;
		value = value * 10. + (*p - '0');
		}
	    else if (*p == '.')
		{
		past_sign = 1;
		if (fraction)
		    conversion_error (desc, err);
		else
		    fraction = 1;
		}
	    else if (!past_sign && *p == '-')
		{
		sign = -1;
		past_sign = 1;
		}
	    else if (!past_sign && *p == '+')
		{
		sign = 1;
		past_sign = 1;
		}
	    else if (*p == 'e' || *p == 'E')
		break;
	    else if (*p != ' ')
		conversion_error (desc, err);

	/* If we didn't see a digit then must be a funny string like "    ".  */
	if (!digit_seen)
	    conversion_error (desc, err);

	if (sign == -1)
	    value = -value;

	/* If there's still something left, there must be an explicit
	   exponent */

	if (p < end)
	    {
	    digit_seen = sign = exp = 0;
	    for (p++; p < end; p++)
		{
		if (DIGIT (*p))
		    {
		    digit_seen = 1;
		    exp = exp * 10 + *p - '0';

		    /* The following is a 'safe' test to prevent overflow of
		       exp here and of scale below. A more precise test occurs 
		       later in this routine. */

		    if (exp >= SHORT_LIMIT)
			(*err) (gds__arith_except, 0);
		    }
		else if (*p == '-' && !digit_seen && !sign)
		    sign = -1;
		else if (*p == '+' && !digit_seen && !sign)
		    sign = 1;
		else if (*p != ' ')
		    conversion_error (desc, err);
		}
	    if (!digit_seen)
		conversion_error (desc, err);

	    if (sign == -1)
		scale += exp;
	    else
		scale -= exp;
	    }

	/* if the scale is greater than the power of 10 representable
	   in a double number, then something has gone wrong... let
	   the user know...  */

#ifdef APOLLO
       /* this hack is needed because APOLLO has DBL_MAX_10_EXP defined
	  as +300 but some versions of their compiler will not allow the
	  test x > +300 */

	if (ABSOLUT (scale) > 0 DBL_MAX_10_EXP)
#else
	if (ABSOLUT (scale) > DBL_MAX_10_EXP)
#endif
	    (*err) (gds__arith_except, 0);

/*
  Repeated division is a good way to mung the least significant bits
  of your value, so we have replaced this iterative multiplication/division
  by a single multiplication or division, depending on sign(scale).
	if (scale > 0)
 	    do value /= 10.; while (--scale);
 	else if (scale)
 	    do value *= 10.; while (++scale);
*/
	if (scale > 0)
	    value /= power_of_ten(scale);
	else if (scale < 0)
	    value *= power_of_ten(-scale);
	}
	return value;

    case dtype_timestamp:    
    case dtype_sql_date:    
    case dtype_sql_time:    
    case dtype_blob:
    case dtype_array:
	conversion_error (desc, err);
	break;

    default:  
	(*err) (gds__badblk, 0);        /* internal error */
	break;
    }

/* Last, but not least, adjust for scale */

if ((scale = desc->dsc_scale) == 0)
    return value;

/* if the scale is greater than the power of 10 representable
   in a double number, then something has gone wrong... let
   the user know... */

#ifdef APOLLO
/* this hack is needed because APOLLO has DBL_MAX_10_EXP defined
   as +300 but some versions of their compiler will not allow the
   test x > +300 */

if (ABSOLUT (scale) > 0 DBL_MAX_10_EXP)
#else
if (ABSOLUT (scale) > DBL_MAX_10_EXP)
#endif
    (*err) (gds__arith_except, 0);

if (scale > 0)
    value *= power_of_ten( scale );
else if (scale < 0)
    value /= power_of_ten( -scale );

return value;
}

SLONG CVT_get_long (
    DSC         *desc,
    SSHORT      scale,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ l o n g
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a long (32 bit) integer of given
 *      scale.
 *
 **************************************/
SLONG   value, high, fraction;

double  d;
SINT64  val64;
USHORT  length;
UCHAR   *p;
TEXT    buffer [50];    /* long enough to represent largest long in ASCII */

/* adjust exact numeric values to same scaling */

if (DTYPE_IS_EXACT(desc->dsc_dtype))
    scale -= desc->dsc_scale;

p = desc->dsc_address;

switch (desc->dsc_dtype)
    {
    case dtype_short:
	value = *((SSHORT*) p);
	break;

    case dtype_long:
	value = *((SLONG*) p);
	break;

    case dtype_int64:
	val64 = *((SINT64*) p);

	/* adjust for scale first, *before* range-checking the value. */
	if (scale > 0)
	    {
	    fraction = 0;
	    do {
		if (scale == 1)
		    fraction = val64 % 10;
		val64 /= 10;
	    } while (--scale);
	    if (fraction > 4)
		val64++;
	    /*
	     * The following 2 lines are correct for platforms where
	     * ((-85 / 10 == -8) && (-85 % 10 == -5)).  If we port to
	     * a platform where ((-85 / 10 == -9) && (-85 % 10 == 5)),
	     * we'll have to change this depending on the platform.
	     */
	    else if (fraction < -4)
	        val64--;
	    }
	else if (scale < 0)
	  do {
	    if ( (val64 > INT64_LIMIT) || (val64 < -INT64_LIMIT) )
		(*err) (gds__arith_except, 0);
	    val64 *= 10;
	  } while (++scale);

	if ( (val64 > LONG_MAX_int64) || (val64 < LONG_MIN_int64) )
	    (*err) (gds__arith_except, 0);
	return (SLONG) val64;

    case dtype_quad:
	value = ((SLONG*) p) [LOW_WORD];
	high = ((SLONG*) p) [HIGH_WORD];
	if ((value >= 0 && !high) || (value < 0 && high == -1))
	    break;
	(*err) (gds__arith_except, 0);
	break;

    case dtype_real:
    case dtype_double:
#ifdef VMS
    case dtype_d_float:
#endif
	if (desc->dsc_dtype == dtype_real)
	    d = *((float*) p);
	else if (desc->dsc_dtype == DEFAULT_DOUBLE)
	    d = *((double*) p);
#ifdef VMS
	else
	    d = CNVT_TO_DFLT ((double*) p);
#endif
	if (scale > 0)
	    do d /= 10.; while (--scale);
	else if (scale < 0)
	    do d *= 10.; while (++scale);
	if (d > 0)
	    d += 0.5;
	else
	    d -= 0.5;

	/* make sure the cast will succeed - different machines 
	   do different things if the value is larger than a long
	   can hold */
	/* If rounding would yield a legitimate value, permit it */

	if (d < (double) LONG_MIN_real)
	    {
	    if (d > (double) LONG_MIN_real - 1.)
		return LONG_MIN_int;
	    (*err) (gds__arith_except, 0);
	    }
	if (d > (double) LONG_MAX_real)
	    {
	    if (d < (double) LONG_MAX_real + 1.)
		return LONG_MAX_int;
	    (*err) (gds__arith_except, 0);
	    }
	return (SLONG) d;
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	length = CVT_make_string (desc, ttype_ascii, &p, buffer, sizeof (buffer), err);
	scale -= decompose (p, length, dtype_long, &value, err);
	break;

    case dtype_blob:
    case dtype_sql_date:
    case dtype_sql_time:
    case dtype_timestamp:
    case dtype_array:
	conversion_error (desc, err);
	break;

    default:
	(*err) (gds__badblk, 0);        /* internal error */
	break;
    }

/* Last, but not least, adjust for scale */

if (scale > 0)
    {
    if (DTYPE_IS_EXACT(desc->dsc_dtype))
	{
	fraction = 0;
	do {
	    if (scale == 1)
		fraction = value % 10;
	    value /= 10;
	   } while (--scale);
	if (fraction > 4)
	    value++;
	/*
	 * The following 2 lines are correct for platforms where
	 * ((-85 / 10 == -8) && (-85 % 10 == -5)).  If we port to
	 * a platform where ((-85 / 10 == -9) && (-85 % 10 == 5)),
	 * we'll have to change this depending on the platform.
	 */
	else if (fraction < -4)
	    value--;
	}
    else
	do value /= 10; while (--scale);
    }
else if (scale < 0)
    do {
	if (value > LONG_LIMIT || value < -LONG_LIMIT)
	    (*err) (gds__arith_except, 0);
	value *= 10;
    } while (++scale);
 
return value;
}

UCHAR CVT_get_numeric (
    UCHAR       *string,
    USHORT      length,
    SSHORT      *scale,
    double      *ptr,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *      Convert a numeric literal (string) to its binary value.
 *
 *	If the literal contains an exponent or is too large to fit
 *      in an int64, return a double, else if the literal is too
 *      large to fit in a long, return an int64, else return a long.
 *
 *      The return value from the function is set to dtype_double,
 *      dtype_int64, or dtype_long depending on the conversion performed.
 *      The binary value (long, int64, or double) is stored at the
 *      address given by ptr.
 *              
 *
 **************************************/
UCHAR   *p, *end;
SSHORT  local_scale, fraction, sign, digit_seen;
DSC     desc;
SINT64  value;


MOVE_CLEAR (&desc, sizeof (desc));
desc.dsc_dtype   = dtype_text;
desc.dsc_ttype   = ttype_ascii;
desc.dsc_length  = length;
desc.dsc_address = string;

digit_seen = local_scale = fraction = sign = value = 0;

for (p = string, end = p + length; p < end; p++)
    {
    if (DIGIT (*p))
	{
	digit_seen = 1;

	/* Before computing the next value, make sure there will be
	   no overflow. Trying to detect overflow after the fact is
	   tricky: the value doesn't always become negative after an
	   overflow!  */

	if (value >= NUMERIC_LIMIT)  
	    {
	    /* possibility of an overflow */
	    if (value > NUMERIC_LIMIT)
		break;
	    else if (((*p > '8') && (sign == -1)) || ((*p > '7') && (sign != -1)))
		break;
	    }

 	/* Force the subtraction to be performed before the addition,
	   thus preventing a possible signed arithmetic overflow.    */
	value = value * 10 + (*p - '0');
	if (fraction)
	    --local_scale;
	}
    else if (*p == '.')
	{
	if (fraction)
	    conversion_error (&desc, err);
	else
	    fraction = TRUE;
	}
    else if (*p == '-' && !digit_seen && !sign && !fraction)
	sign = -1;
    else if (*p == '+' && !digit_seen && !sign && !fraction)
	sign = 1;
    else if (*p == 'e' || *p == 'E')
	break;
    else if (*p != ' ')
	conversion_error (&desc, err);
    }

if (!digit_seen)
    conversion_error (&desc, err);

if ((p < end) ||		   /* there is an exponent */
    ((value < 0) && (sign != -1))) /* MAX_SINT64+1 wrapped around */
    {
    /* convert to double */
    *ptr = CVT_get_double (&desc, err);
    return dtype_double;
    }

*scale = local_scale;

/* The literal has already been converted to a 64-bit integer: return
   a long if the value fits into a long, else return an int64. */

if ((value <= LONG_MAX_int64) && (value >= 0))
  {
    *(SLONG*) ptr = (SLONG) ((sign == -1) ? -value : value);
    return dtype_long;
  }
else if ((sign == -1) && (-value == LONG_MIN_int64))
  {
    *(SLONG*) ptr = LONG_MIN_int;
    return dtype_long;
  }
else
  {
    /* Either MAX_SLONG < value <= MAX_SINT64, or
       ((value == MIN_SINT64) && (sign == -1)).
       In the first case, the number can be negated, while in the second
       negating the value will not change it on a 2s-complement system. */
    
    *(SINT64*) ptr = ((sign == -1) ? -value : value );
    return dtype_int64;
  }
}

SQUAD CVT_get_quad (
    DSC         *desc,
    SSHORT      scale,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ q u a d
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a quad (64 bit) integer of given
 *      scale.
 *
 **************************************/
SQUAD   value;
SLONG   high, fraction;
double  d;
USHORT  length;
UCHAR   *p;
TEXT    buffer [50];    /* long enough to represent largest quad in ASCII */

/* adjust exact numeric values to same scaling */

if (DTYPE_IS_EXACT(desc->dsc_dtype))
    scale -= desc->dsc_scale;

p = desc->dsc_address;

switch (desc->dsc_dtype)
    {
    case dtype_short:
	((SLONG*) &value) [LOW_WORD] = *((SSHORT*) p);
	((SLONG*) &value) [HIGH_WORD] = (*((SSHORT*) p) < 0) ? -1 : 0;
	break;

    case dtype_long:
	((SLONG*) &value) [LOW_WORD] = *((SLONG*) p);
	((SLONG*) &value) [HIGH_WORD] = (*((SLONG*) p) < 0) ? -1 : 0;
	break;

    case dtype_quad:
	value = *((SQUAD*) p);
	break;

    case dtype_int64:
	((SLONG*) &value) [LOW_WORD] = (SLONG) (*((SINT64 *)p) & 0xffffffff);
        ((SLONG*) &value) [HIGH_WORD] = (SLONG) (*((SINT64 *)p) >> 32);
	break;
    
    case dtype_real:
    case dtype_double:
#ifdef VMS
    case dtype_d_float:
#endif
	if (desc->dsc_dtype == dtype_real)
	    d = *((float*) p);
	else if (desc->dsc_dtype == DEFAULT_DOUBLE)
	    d = *((double*) p);
#ifdef VMS
	else
	    d = CNVT_TO_DFLT ((double*) p);
#endif
	if (scale > 0)
	    do d /= 10.; while (--scale);
	else if (scale < 0)
	    do d *= 10.; while (++scale);
	if (d > 0)
	    d += 0.5;
	else
	    d -= 0.5;

	/* make sure the cast will succeed - different machines 
	   do different things if the value is larger than a quad
	   can hold */

	if (d < (double) QUAD_MIN_real || (double) QUAD_MAX_real < d)
	    {
	    /* If rounding would yield a legitimate value, permit it */

	    if (d > (double) QUAD_MIN_real - 1.)
		return QUAD_MIN_int;
	    else if (d < (double) QUAD_MAX_real + 1.)
		return QUAD_MAX_int;
	    (*err) (gds__arith_except, 0);
	    }
	return QUAD_FROM_DOUBLE (d, err);
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	length = CVT_make_string (desc, ttype_ascii, &p, buffer, sizeof (buffer), err);
	scale -= decompose (p, length, dtype_quad, &value.high, err);
	break;

    case dtype_blob:
    case dtype_sql_date:
    case dtype_sql_time:
    case dtype_timestamp:
    case dtype_array:
	conversion_error (desc, err);
	break;

    default:
	(*err) (gds__badblk, 0);        /* internal error */
	break;
    }

/* Last, but not least, adjust for scale */

if (scale == 0)
    return value;

#ifndef NATIVE_QUAD
(*err) (gds__badblk, 0);        /* internal error */
#else
if (scale > 0)
    {
    if (desc->dsc_dtype == dtype_short ||
	desc->dsc_dtype == dtype_long ||
	desc->dsc_dtype == dtype_quad)
	{
	fraction = 0;
	do {
	    if (scale == 1)
		fraction = value % 10;
	    value /= 10;
	   } while (--scale);
	if (fraction > 4)
	    value++;
	/*
	 * The following 2 lines are correct for platforms where
	 * ((-85 / 10 == -8) && (-85 % 10 == -5)).  If we port to
	 * a platform where ((-85 / 10 == -9) && (-85 % 10 == 5)),
	 * we'll have to change this depending on the platform.
	 */
	else if (fraction < -4)
	  value--;
	}
    else
	do value /= 10; while (--scale);
    }
else
    do {
	if (value > QUAD_LIMIT || value < -QUAD_LIMIT)
	    (*err) (gds__arith_except, 0);
	value *= 10;
    } while (++scale);
#endif
 
return value;
}

SINT64 CVT_get_int64 (
    DSC         *desc,
    SSHORT      scale,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ i n t 6 4
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to an SINT64 (64 bit integer)
 *      of given scale.
 *
 **************************************/
SINT64   value;
SLONG   fraction;
double  d;
USHORT  length;
UCHAR   *p;
TEXT    buffer [50];    /* long enough to represent largest SINT64 in ASCII */

/* adjust exact numeric values to same scaling */

if (DTYPE_IS_EXACT(desc->dsc_dtype))
    scale -= desc->dsc_scale;

p = desc->dsc_address;

switch (desc->dsc_dtype)
    {
    case dtype_short:
        value = *((SSHORT*) p);
	break;

    case dtype_long:
        value = *((SLONG*) p);
	break;

    case dtype_int64:
	value = *((SINT64*) p);
	break;

    case dtype_quad:
	value = ((SINT64) ((SLONG*) &value) [HIGH_WORD]) << 32 +
	        (((ULONG*) &value) [LOW_WORD]);
	break;

    case dtype_real:
    case dtype_double:
#ifdef VMS
    case dtype_d_float:
#endif
	if (desc->dsc_dtype == dtype_real)
	    d = *((float*) p);
	else if (desc->dsc_dtype == DEFAULT_DOUBLE)
	    d = *((double*) p);
#ifdef VMS
	else
	    d = CNVT_TO_DFLT ((double*) p);
#endif
	if (scale > 0)
	    do d /= 10.; while (--scale);
	else if (scale < 0)
	    do d *= 10.; while (++scale);
	if (d > 0)
	    d += 0.5;
	else
	    d -= 0.5;

	/* make sure the cast will succeed - different machines 
	   do different things if the value is larger than a quad
	   can hold.

	   Note that adding or subtracting 0.5, as we do in CVT_get_long,
	   will never allow the rounded value to fit into an int64,
	   because when the double value is too large in magnitude
	   to fit, 0.5 is less than half of the least significant bit
	   of the significant (sometimes miscalled "mantissa") of the
	   double, and thus will have no effect on the sum. */

	if (d < (double) QUAD_MIN_real || (double) QUAD_MAX_real < d)
	    	    (*err) (gds__arith_except, 0);

	return (SINT64) d;
    
    case dtype_varying:
    case dtype_cstring:
    case dtype_text:
	length = CVT_make_string (desc, ttype_ascii, &p, buffer, sizeof (buffer), err);
	scale -= decompose (p, length, dtype_int64, (SLONG *) &value, err);
	break;

    case dtype_blob:
    case dtype_sql_date:
    case dtype_sql_time:
    case dtype_timestamp:
    case dtype_array:
	conversion_error (desc, err);
	break;

    default:
	(*err) (gds__badblk, 0);        /* internal error */
	break;
    }

/* Last, but not least, adjust for scale */

if (scale > 0)
    {
    if (desc->dsc_dtype == dtype_short ||
	desc->dsc_dtype == dtype_long ||
	desc->dsc_dtype == dtype_int64)
	{
	fraction = 0;
	do {
	    if (scale == 1)
		fraction = value % 10;
	    value /= 10;
	   } while (--scale);
	if (fraction > 4)
	    value++;
	    /*
	     * The following 2 lines are correct for platforms where
	     * ((-85 / 10 == -8) && (-85 % 10 == -5)).  If we port to
	     * a platform where ((-85 / 10 == -9) && (-85 % 10 == 5)),
	     * we'll have to change this depending on the platform.
	     */
	    else if (fraction < -4)
	        value--;
	}
    else
	do value /= 10; while (--scale);
    }
else if (scale < 0)
    do {
	if (value > INT64_LIMIT || value < -INT64_LIMIT)
	    (*err) (gds__arith_except, 0);
	value *= 10;
    } while (++scale);
 
return value;
}

USHORT CVT_get_string_ptr (
    DSC         *desc,
    USHORT      *ttype,
    UCHAR       **address,
    VARY        *temp,
    USHORT      length,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ s t r i n g _ p t r
 *
 **************************************
 *
 * Functional description
 *      Get address and length of string, converting the value to
 *      string, if necessary.  The caller must provide a sufficiently
 *      large temporary.  The address of the resultant string is returned
 *      by reference.  Get_string returns the length of the string (in bytes).
 *
 *      The text-type of the string is returned in ttype.
 *
 *      Note: If the descriptor is known to be a string type, the fourth
 *      argument (temp buffer) may be NULL.
 *
 *      If input (desc) is already a string, the output pointers
 *      point to the data of the same text_type.  If (desc) is not
 *      already a string, output pointers point to ttype_ascii.
 *
 **************************************/
VARY    *varying;
DSC     temp_desc;

assert (desc != NULL);
assert (ttype != NULL);
assert (address != NULL);
assert (err != NULL);
assert ((((temp != NULL) && (length > 0)) || (desc->dsc_dtype == dtype_text) || (desc->dsc_dtype == dtype_cstring) || (desc->dsc_dtype == dtype_varying)));

/* If the value is already a string (fixed or varying), just return
   the address and length. */

if (desc->dsc_dtype <= dtype_any_text)
    {
    *address = desc->dsc_address;
    *ttype = INTL_TTYPE (desc);
    if (desc->dsc_dtype == dtype_text)
	return desc->dsc_length;
    if (desc->dsc_dtype == dtype_cstring)
	return MIN (strlen ((char *)desc->dsc_address), desc->dsc_length - 1);
    if (desc->dsc_dtype == dtype_varying)
	{
	varying = (VARY*) desc->dsc_address;
	*address = varying->vary_string;
	return MIN (varying->vary_length, desc->dsc_length - sizeof (USHORT));
	}
    }

/* No luck -- convert value to varying string. */

MOVE_CLEAR (&temp_desc, sizeof (temp_desc));
temp_desc.dsc_length = length;
temp_desc.dsc_address = (UCHAR*) temp;
INTL_ASSIGN_TTYPE (&temp_desc, ttype_ascii);
temp_desc.dsc_dtype = dtype_varying;
CVT_move (desc, &temp_desc, err);
*address = temp->vary_string;
*ttype = INTL_TTYPE (&temp_desc);

return temp->vary_length;
}

GDS_DATE PASCAL_ROUTINE CVT_get_sql_date (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ s q l _ d a t e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL date value
 *
 **************************************/
DSC		temp_desc;
GDS_DATE	value;

if (desc->dsc_dtype == dtype_sql_date)
    return *((GDS_DATE *)desc->dsc_address);

memset (&temp_desc, 0, sizeof (temp_desc));
temp_desc.dsc_dtype = dtype_sql_date;
temp_desc.dsc_address = (void *) &value;
CVT_move (desc, &temp_desc, err);
return value;
}

GDS_TIME PASCAL_ROUTINE CVT_get_sql_time (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ s q l _ t i m e
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL time value
 *
 **************************************/
DSC		temp_desc;
GDS_TIME	value;

if (desc->dsc_dtype == dtype_sql_time)
    return *((GDS_TIME *)desc->dsc_address);

memset (&temp_desc, 0, sizeof (temp_desc));
temp_desc.dsc_dtype = dtype_sql_time;
temp_desc.dsc_address = (void *) &value;
CVT_move (desc, &temp_desc, err);
return value;
}

GDS_TIMESTAMP PASCAL_ROUTINE CVT_get_timestamp (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ g e t _ t i m e s t a m p
 *
 **************************************
 *
 * Functional description
 *      Convert something arbitrary to a SQL timestamp
 *
 **************************************/
DSC		temp_desc;
GDS_TIMESTAMP	value;

if (desc->dsc_dtype == dtype_timestamp)
    return *((GDS_TIMESTAMP *)desc->dsc_address);

memset (&temp_desc, 0, sizeof (temp_desc));
temp_desc.dsc_dtype = dtype_timestamp;
temp_desc.dsc_address = (void *) &value;
CVT_move (desc, &temp_desc, err);
return value;
}

USHORT CVT_make_string (
    DSC         *desc,
    USHORT      to_interp,
    UCHAR       **address,
    VARY        *temp,
    USHORT      length,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ m a k e _ s t r i n g 
 *
 **************************************
 *
 * Functional description
 *     Convert the data from the desc to a string in the specified interp.
 *     The pointer to this string is returned in address.
 *
 **************************************/
VARY    *varying;
DSC     temp_desc;
USHORT  from_len;

assert (desc != NULL);
assert (address != NULL);
assert (err != NULL);
assert ((((temp != NULL) && (length > 0)) || ((INTL_TTYPE(desc) <= dtype_any_text) && (INTL_TTYPE(desc) == to_interp))));

if (desc->dsc_dtype <= dtype_any_text && INTL_TTYPE (desc) == to_interp)
    {
    *address = desc->dsc_address;
    from_len = desc->dsc_length;
    if (desc->dsc_dtype == dtype_text)
	return from_len;
    if (desc->dsc_dtype == dtype_cstring)
	return MIN (strlen ((char *)desc->dsc_address), from_len - 1);
    if (desc->dsc_dtype == dtype_varying)
	{
	varying = (VARY*) desc->dsc_address;
	*address = varying->vary_string;
	return MIN (varying->vary_length, from_len - sizeof (USHORT));
	}
    }

/* Not string data, then  -- convert value to varying string. */

MOVE_CLEAR (&temp_desc, sizeof (temp_desc));
temp_desc.dsc_length = length;
temp_desc.dsc_address = (UCHAR*) temp;
INTL_ASSIGN_TTYPE (&temp_desc, to_interp);
temp_desc.dsc_dtype = dtype_varying;
CVT_move (desc, &temp_desc, err);
*address = temp->vary_string;

return temp->vary_length;
}

void DLL_EXPORT CVT_move (
    DSC         *from,
    DSC         *to,
    FPTR_VOID   err)
{
/**************************************
 *
 *      C V T _ m o v e
 *
 **************************************
 *
 * Functional description
 *      Move (and possible convert) something to something else.
 *
 **************************************/
register UCHAR  *p, *q;
register SSHORT fill;
SLONG           l, length;
UCHAR           *ptr;
USHORT          strtype;
CHARSET_ID      charset1, charset2;
UCHAR 		fill_char;

length = from->dsc_length;
p = to->dsc_address;
q = from->dsc_address;

/* If the datatypes and lengths are identical, just move the
   stuff byte by byte.  Although this may seem slower than 
   optimal, it would cost more to find the fast move than the
   fast move would gain. */

if (DSC_EQUIV (from, to))
    {
    if (length)
	{
	MOVE_FAST( q, p, length);
	}
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
		{
		GDS_TIMESTAMP	date;
		struct tm		times;

		string_to_datetime (from, &date, expect_timestamp, err);

   	    isc_decode_timestamp (&date, &times);
		if ((times.tm_year+1900) < MIN_YEAR || (times.tm_year)+1900 > MAX_YEAR)
			(*err) (isc_date_range_exceeded, 0);

		((GDS_TIMESTAMP*) to->dsc_address)->timestamp_date = date.timestamp_date;
		((GDS_TIMESTAMP*) to->dsc_address)->timestamp_time = date.timestamp_time;
		}
		return;

	    case dtype_sql_date:
		((GDS_TIMESTAMP *)(to->dsc_address))->timestamp_date = 
			*(GDS_DATE *)(from->dsc_address);
		((GDS_TIMESTAMP *)(to->dsc_address))->timestamp_time =  0;
		return;

	    case dtype_sql_time:
		((GDS_TIMESTAMP *)(to->dsc_address))->timestamp_date =  0;
		((GDS_TIMESTAMP *)(to->dsc_address))->timestamp_time = 
			*(GDS_TIME *)(from->dsc_address);

		/* Per SQL Specs, we need to set the DATE
		   portion to the current date */
		{
		TDBB 		tdbb=NULL;
		ULONG		clock;
		struct tm	times;
		GDS_TIMESTAMP	enc_times;

		/** Cannot call GET_THREAD_DATA because that macro calls 
		    BUGCHECK i.e. ERR_bugcheck() which is not part of 
		    client library **/
		tdbb = PLATFORM_GET_THREAD_DATA;

		/* If we're in the engine, then the THDD type must
		   be a THDD_TYPE_TDBB.  So, if we're in the engine
		   and have a request, pull the effective date out
		   of the request timestamp.
		   Otherwise, take the CURRENT date to populate the 
		   date portion of the timestamp */

		if ((tdbb) && 
		    (((THDD)tdbb)->thdd_type == THDD_TYPE_TDBB) &&
                    tdbb->tdbb_request)
		    {
		    if (tdbb->tdbb_request->req_timestamp)
			clock = tdbb->tdbb_request->req_timestamp;
		    else
			{
			/* All requests should have a timestamp */
			assert (FALSE);	
			clock = time (NULL_PTR);
			}
		    }
		else
		    clock = time (NULL_PTR);
		times = *localtime (&clock);
		isc_encode_timestamp (&times, &enc_times);
		((GDS_TIMESTAMP *)(to->dsc_address))->timestamp_date = 
					enc_times.timestamp_date;
		}
		return;

	    default:
		assert (FALSE); 	/* Fall into ... */
	    case dtype_short:
	    case dtype_long:
	    case dtype_int64:
	    case dtype_quad:
	    case dtype_real:
	    case dtype_double: 
#ifdef VMS
	    case dtype_d_float:
#endif
		conversion_error (from, err);
		break;
	    }
	break;

    case dtype_sql_date:
	switch (from->dsc_dtype)
	    {
	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
		{
		GDS_TIMESTAMP	date;
		struct tm		times;

		string_to_datetime (from, &date, expect_sql_date, err);
   	    isc_decode_timestamp (&date, &times);
		if ((times.tm_year+1900) < MIN_YEAR || (times.tm_year)+1900 > MAX_YEAR)
			(*err) (isc_date_range_exceeded, 0);

		*((GDS_DATE *)to->dsc_address) = date.timestamp_date;
		}
		return;

	    case dtype_timestamp:
		{
		GDS_TIMESTAMP new_date;
		struct tm	  times;

		new_date.timestamp_date = ((GDS_TIMESTAMP *) from->dsc_address)->timestamp_date;
		new_date.timestamp_time = 0;

		isc_decode_timestamp (&new_date, &times);
		if ((times.tm_year+1900) < MIN_YEAR || (times.tm_year)+1900 > MAX_YEAR)
			(*err) (isc_date_range_exceeded, 0);

		*((GDS_DATE *)to->dsc_address) = ((GDS_TIMESTAMP *) from->dsc_address)->timestamp_date;
		}
		return;

	    default:
		assert (FALSE); 	/* Fall into ... */
	    case dtype_sql_time:
	    case dtype_short:
	    case dtype_long:
	    case dtype_int64:
	    case dtype_quad:
	    case dtype_real:
	    case dtype_double: 
#ifdef VMS
	    case dtype_d_float:
#endif
		conversion_error (from, err);
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
		GDS_TIMESTAMP	date;
		string_to_datetime (from, &date, expect_sql_time, err);
		*(GDS_TIME *)to->dsc_address = date.timestamp_time;
		}
		return;

	    case dtype_timestamp:
		*((GDS_TIME *)to->dsc_address) = ((GDS_TIMESTAMP *) from->dsc_address)->timestamp_time;
		return;

	    default:
		assert (FALSE); 	/* Fall into ... */
	    case dtype_sql_date:
	    case dtype_short:
	    case dtype_long:
	    case dtype_int64:
	    case dtype_quad:
	    case dtype_real:
	    case dtype_double: 
#ifdef VMS
	    case dtype_d_float:
#endif
		conversion_error (from, err);
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
		/* If we are within the engine, INTL_convert_string 
		 * will convert the string between character sets
		 * (or die trying).
		 * This module, however, can be called from outside
		 * the engine (for instance, moving values around for
		 * DSQL).
		 * In that event, we'll move the values if we think
		 * they are compatible text types, otherwise fail.
		 * eg: Simple cases can be handled here (no
		 * character set conversion).
		 *
		 * a charset type binary is compatible with all other types.
		 * if a charset involved is ttype_dynamic, we must look up
		 *    the charset of the attachment (only if we are in the 
                 *    engine). If we are outside the engine, the
		 *    assume that the engine has converted the values
		 *    previously in the request.
		 *
		 * Even within the engine, not calling INTL_convert_string
		 * unless really required is a good optimization.
		 */

#ifndef REQUESTER
#ifndef SUPERCLIENT
		if ( (INTL_TTYPE (from) == ttype_dynamic) &&
                     (err == (FPTR_VOID) ERR_post) )
		   charset1 = INTL_charset (NULL_TDBB, INTL_TTYPE(from), err);
		else
		   charset1 = INTL_TTYPE(from);

		if ( (INTL_TTYPE (to) == ttype_dynamic) &&
                     (err == (FPTR_VOID) ERR_post) )
		   charset2 = INTL_charset (NULL_TDBB, INTL_TTYPE(to), err);
		else
		   charset2 = INTL_TTYPE(to);

		/* The charset[12] can be ttype_dynamic only if we are
		outside the engine. Within the engine INTL_charset
		would have set it to the ttype of the attachment */

                if ( (charset1 != charset2)     &&
                     (charset2 != ttype_none)   &&
                     (charset1 != ttype_binary) &&
                     (charset2 != ttype_binary) &&
                     (charset1 != ttype_dynamic) &&
                     (charset2 != ttype_dynamic) )
		    {
		    if (err == (FPTR_VOID) ERR_post)
			{
			INTL_convert_string (to, from, err);
			return;
			}
		    else
#endif
#endif
		        (*err) (gds__arith_except, 0);
#ifndef REQUESTER
#ifndef SUPERCLIENT
		    }
#endif
#endif

		length = l = CVT_get_string_ptr (from, &strtype, &ptr, NULL, 0, err);
		q = ptr;
		switch (to->dsc_dtype)
		    {
		    case dtype_text:
			length = MIN (length, to->dsc_length);
			l -= length;
			fill = to->dsc_length - length;

			CVT_COPY_BUFF (q,p,length);
			if (fill > 0)
			    {
#ifndef REQUESTER
#ifndef SUPERCLIENT
			    if (charset2 == ttype_binary)
				fill_char = 0x00;
			    else
#endif
#endif
				fill_char = ASCII_SPACE;
			    do *p++ = fill_char; while (--fill);
			    /* Note: above is correct only for narrow 
				     and multi-byte character sets which 
				     use ASCII for the SPACE character.  */
			    }
			break;

		    case dtype_cstring:
			/* Note: Following is only correct for narrow and
				 multibyte character sets which use a zero
				 byte to represent end-of-string */

			length = MIN (length, to->dsc_length - 1);
			l -= length;
			CVT_COPY_BUFF (q,p,length);
			*p = 0;
			break;

		    case dtype_varying:
			length = MIN (length, to->dsc_length - sizeof (USHORT));
			l -= length;
			((VARY*) p)->vary_length = length;
			p = ((VARY*) p)->vary_string;
			CVT_COPY_BUFF (q,p,length);
			break;
		    }

		if (l)
		    {
		    /* scan the truncated string to ensure only spaces lost */
		    /* Note: it  is correct only for narrow and multi-byte
			     character sets which use ASCII for the SPACE
			     character. */

		    do if (*q++ != ASCII_SPACE)
			(*err) (gds__arith_except, 0);
		    while (--l)
			;
		    }
		return;

	    case dtype_short:
	    case dtype_long:
	    case dtype_int64:
	    case dtype_quad:
		integer_to_text (from, to, err);
		return;

	    case dtype_real:
	    case dtype_double:
#ifdef VMS
	    case dtype_d_float:
#endif
		float_to_text (from, to, err);
		return;

	    case dtype_sql_date:
	    case dtype_sql_time:
	    case dtype_timestamp:
		datetime_to_text (from, to, err);
		return;

	    default:
		assert (FALSE);		/* Fall into ... */
	    case dtype_blob:
		conversion_error (from, err);
		return;
	    }
	break;

    case dtype_blob:
    case dtype_array:
	if (from->dsc_dtype == dtype_quad)
	    {
	    ((SLONG*) p) [0] = ((SLONG*) q) [0];
	    ((SLONG*) p) [1] = ((SLONG*) q) [1];
	    return;
	    }

	if (to->dsc_dtype != from->dsc_dtype)
	    (*err) (gds__wish_list, 
		    gds_arg_gds, gds__blobnotsup, 
		    gds_arg_string, "move", 0);

	/* Note: DSC_EQUIV failed above as the blob sub_types were different,
	 * or their character sets were different.  In V4 we aren't trying
	 * to provide blob type integrity, so we just assign the blob id
	 */ 

	/* Move blob_id byte-by-byte as that's the way it was done before */
	CVT_COPY_BUFF (q,p,length);
	return;

    case dtype_short:
	l = CVT_get_long (from, (SSHORT) to->dsc_scale, err);
	*(SSHORT*) p = l;
	if (*(SSHORT*) p != l)
	    (*err) (gds__arith_except, 0);
	return;

    case dtype_long:
	*(SLONG*) p = CVT_get_long (from, (SSHORT) to->dsc_scale, err);
	return;

    case dtype_int64:
        *(SINT64*) p = CVT_get_int64(from, (SSHORT) to->dsc_scale, err);
	return;

    case dtype_quad:
	if (from->dsc_dtype == dtype_blob ||
	    from->dsc_dtype == dtype_array)
	    {
	    ((SLONG*) p) [0] = ((SLONG*) q) [0];
	    ((SLONG*) p) [1] = ((SLONG*) q) [1];
	    return;
	    }
	*(SQUAD*) p = CVT_get_quad (from, (SSHORT) to->dsc_scale, err);
	return;

    case dtype_real:
	{
	double	d_value;
	d_value = CVT_get_double (from, err);
	if (ABSOLUT(d_value) > FLOAT_MAX)
	    (*err) (gds__arith_except, 0);
	*(float*) p = d_value;
	}
	return;

    case DEFAULT_DOUBLE:
	*(double*) p = CVT_get_double (from, err);
	return;

#ifdef VMS
    case SPECIAL_DOUBLE:
	*(double*) p = CVT_get_double (from, err);
	*(double*) p = CNVT_FROM_DFLT ((double*) p);
	return;
#endif
    }

if (from->dsc_dtype == dtype_array || from->dsc_dtype == dtype_blob)
    (*err) (gds__wish_list, 
	    gds_arg_gds, gds__blobnotsup, 
	    gds_arg_string, "move", 0);

(*err) (gds__badblk, 0);        /* internal error */
}

static void conversion_error (
    DSC         *desc,
    FPTR_VOID   err)
{
/**************************************
 *
 *      c o n v e r s i o n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *      A data conversion error occurred.  Complain.
 *
 **************************************/
TEXT	*p, s [40], *string;
USHORT  length;

if (desc->dsc_dtype == dtype_blob)
    p = "BLOB";
else if (desc->dsc_dtype == dtype_array)
    p = "ARRAY";
else
    {
    length = CVT_make_string (desc, ttype_ascii, &string, s, sizeof (s), err);
#if (defined REQUESTER || defined SUPERCLIENT)
    p = error_string (string, length);
#else
    p = ERR_string (string, length);
#endif    
    }

(*err) (gds__convert_error, gds_arg_string, p, 0);
}

static void datetime_to_text (
    DSC		*from,
    DSC         *to,
    FPTR_VOID   err)
{
/**************************************
 *
 *       d a t e t i m e _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert a timestamp, date or time value to text.
 *
 **************************************/
DSC             desc;
TEXT            *p, temp [30];	/* yyyy-mm-dd hh:mm:ss.tttt  OR
				   dd-MMM-yyyy hh:mm:ss.tttt */
struct tm       times;
GDS_TIMESTAMP	date;
TDBB		tdbb=NULL;
SSHORT		version4 = TRUE;

assert (DTYPE_IS_TEXT (to->dsc_dtype));

/* Convert a date or time value into a timestamp for manipulation */


switch (from->dsc_dtype)
    {
    case dtype_sql_time:
	date.timestamp_date = 0;
	date.timestamp_time = *(GDS_TIME *) from->dsc_address;
	break;
    case dtype_sql_date:
	date.timestamp_date = *(GDS_DATE *) from->dsc_address;
	date.timestamp_time = 0;
	break;
    case dtype_timestamp:
        /** Cannot call GET_THREAD_DATA because that macro calls 
            BUGCHECK i.e. ERR_bugcheck() which is not part of 
	    client library **/
        tdbb = PLATFORM_GET_THREAD_DATA;
	if ((tdbb) && 
	    (((THDD)tdbb)->thdd_type == THDD_TYPE_TDBB) &&
	    tdbb->tdbb_request)
            version4 = (tdbb->tdbb_request->req_flags & req_blr_version4) ? TRUE : FALSE;
	date = *(GDS_TIMESTAMP *) from->dsc_address;
	break;
    default:
	assert (FALSE);
        (*err) (gds__badblk, 0);    /* internal error */
	break;
    }

/* Decode the timestamp into human readable terms */

isc_decode_timestamp (&date, &times);
p = temp;

/* Make a textual date for data types that include it */

if (from->dsc_dtype != dtype_sql_time)
    {

    if (from->dsc_dtype == dtype_sql_date || !version4)
	sprintf (p, "%4.4d-%2.2d-%2.2d", 
		    times.tm_year + 1900,
		    times.tm_mon + 1,
		    times.tm_mday);
    else
	/* Prior to BLR version 5 - timestamps where converted to
	   text in the dd-Mon-yyyy format */
	sprintf (p, "%d-%.3s-%d", 
		times.tm_mday,
		months [times.tm_mon],
		times.tm_year + 1900);

    while (*p)
	p++;
    };

/* Put in a space to separate date & time components */

if ((from->dsc_dtype == dtype_timestamp) && (!version4))
    *p++ = ' ';

/* Add the time part for data types that include it */

if (from->dsc_dtype != dtype_sql_date)
    {
    if (from->dsc_dtype == dtype_sql_time || !version4)
	sprintf (p, "%2.2d:%2.2d:%2.2d.%4.4d", 
		times.tm_hour, times.tm_min, times.tm_sec, 
		(USHORT) (date.timestamp_time % ISC_TIME_SECONDS_PRECISION));
    else if (times.tm_hour || times.tm_min || 
	     times.tm_sec  || date.timestamp_time)
	/* Timestamp formating prior to BLR Version 5 is slightly
	   different */
	sprintf (p, " %d:%.2d:%.2d.%.4d", 
		times.tm_hour, times.tm_min, times.tm_sec, 
		(USHORT) (date.timestamp_time % ISC_TIME_SECONDS_PRECISION));
    while (*p)
	p++;
    };

/* Move the text version of the date/time value into the destination */

MOVE_CLEAR (&desc, sizeof (desc));
desc.dsc_address = (UCHAR *) temp;
desc.dsc_dtype = dtype_text;
desc.dsc_ttype = ttype_ascii;
desc.dsc_length = (p - temp);
if (from->dsc_dtype == dtype_timestamp && version4)
    {
    USHORT	l;
    /* Prior to BLR Version5, when a timestamp is converted to a string it
       is silently truncated if the destination string is not large enough */

    assert (to->dsc_dtype <= dtype_any_text);

    l = (to->dsc_dtype == dtype_cstring) ? 1 :
	(to->dsc_dtype == dtype_varying) ? sizeof (USHORT)  :
	0;
    desc.dsc_length = MIN (desc.dsc_length, (to->dsc_length - l));
    }

CVT_move (&desc, to, err);
}

static SSHORT decompose (
    UCHAR       *string,
    USHORT      length,
    SSHORT      dtype,
    SLONG       *return_value,
    FPTR_VOID   err)
{
/**************************************
 *
 *      d e c o m p o s e
 *
 **************************************
 *
 * Functional description
 *      Decompose a numeric string in mantissa and exponent.
 *
 **************************************/
UCHAR   *p, *end;
SSHORT  scale, exp, fraction, sign, digit_seen;
DSC     errd;
SINT64  value, lower_limit, upper_limit, limit_by_10;

#ifndef NATIVE_QUAD
/* For now, this routine does not handle quadwords unless this is
   supported by the platform as a native datatype. */

if (dtype == dtype_quad)
    (*err) (gds__badblk, 0);    /* internal error */
#endif

MOVE_CLEAR (&errd, sizeof (errd));
errd.dsc_dtype   = dtype_text;
errd.dsc_ttype   = ttype_ascii;
errd.dsc_length  = length;
errd.dsc_address = string;

digit_seen = scale = fraction = sign = value = 0;
lower_limit = (dtype == dtype_long) ? MIN_SLONG : MIN_SINT64;
upper_limit = (dtype == dtype_long) ? MAX_SLONG : MAX_SINT64;

limit_by_10 = upper_limit / 10;   /* used to check for overflow */

for (p = string, end = p + length; p < end; p++)
    {
    if (*p == ',')
	continue;
    else if (DIGIT (*p))
	{
	digit_seen = 1;

	/* Before computing the next value, make sure there will be
	   no overflow. Trying to detect overflow after the fact is
	   tricky: the value doesn't always become negative after an
	   overflow!  */

	if (value >= limit_by_10)  
	    {
	    /* possibility of an overflow */
	    if (value > limit_by_10)
		(*err) (gds__arith_except, 0);
	    else if (((*p > '8') && (sign == -1)) || ((*p > '7') && (sign != -1)))
		(*err) (gds__arith_except, 0);
	    }

	value = value * 10 + *p - '0';
	if (fraction)
	    --scale;
	}
    else if (*p == '.')
	{
	if (fraction)
	    conversion_error (&errd, err);
	else
	    fraction = TRUE;
	}
    else if (*p == '-' && !digit_seen && !sign && !fraction)
	sign = -1;
    else if (*p == '+' && !digit_seen && !sign && !fraction)
	sign = 1;
    else if (*p == 'e' || *p == 'E')
	break;
    else if (*p != ' ')
	conversion_error (&errd, err);
    }


if (!digit_seen)
    conversion_error (&errd, err);

if ((sign == -1) && value != lower_limit)
    value = -value;

/* If there's still something left, there must be an explicit exponent */

if (p < end)
    {
    digit_seen = sign = exp = 0;
    for (p++; p < end; p++)
	{
	if (DIGIT (*p))
	    {
	    digit_seen = 1;
	    exp = exp * 10 + *p - '0';

	    /* The following is a 'safe' test to prevent overflow of
	       exp here and of scale below. A more precise test will
	       occur in the calling routine when the scale/exp is
	       applied to the value. */

	    if (exp >= SHORT_LIMIT)
		(*err) (gds__arith_except, 0);
	    }
	else if (*p == '-' && !digit_seen && !sign)
	    sign = -1;
	else if (*p == '+' && !digit_seen && !sign)
	    sign = 1;
	else if (*p != ' ')
	    conversion_error (&errd, err);
	}
    if (sign == -1)
	scale -= exp;
    else
	scale += exp;

    if (!digit_seen)
       conversion_error (&errd, err);

    }

if (dtype == dtype_long)
    *return_value = value;
else
    *((SINT64*) return_value) = value;

return scale;
}

#if (defined REQUESTER || defined SUPERCLIENT)
static  TEXT *error_string (
    TEXT        *in_string,
    SSHORT       length)
{
/**************************************
 *
 *      e r r o r _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Copy a stack local string into an area that is
 *      guaranteed to exist for a reasonable length of time.
 *      A circular buffer is used to hold these strings.  It
 *      is independent of the JRD allocator mechanism.
 *
 **************************************/
TEXT    *new_string;
 
if (!cvt_failures_ptr)
    cvt_failures_ptr = cvt_failures;

/* If there isn't any more room in the buffer, start at the beginning again */

if (cvt_failures_ptr + length + 1 > cvt_failures + CVT_FAILURE_SPACE)
    cvt_failures_ptr = cvt_failures;

new_string = cvt_failures_ptr;

while (length-- && (cvt_failures_ptr < cvt_failures + CVT_FAILURE_SPACE - 1))
    *cvt_failures_ptr++ = *in_string++;
*cvt_failures_ptr++ = 0;

return new_string;
}
#endif

static void float_to_text (
    DSC         *from,
    DSC         *to,
    FPTR_VOID   err)
{
/**************************************
 *
 *      f l o a t _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert an arbitrary floating type number to text.
 *      To avoid messiness, convert first into a temp (fixed) then
 *      move to the destination.
 *      Never print more digits than the source type can actually
 *      provide: instead pad the output with blanks on the right if
 *      needed.
 *
 **************************************/
double       d;
DSC          intermediate;
int          precision;
int          to_len;		/* length of destination */
int          width;		/* minimum width to print */
int          chars_printed;	/* number of characters printed */
UCHAR        temp [] = "-1.234567890123456E-300";

to_len = DSC_string_length (to);
width = MIN(to_len, sizeof(temp)-1); 

if (dtype_double == from->dsc_dtype)
    {
    precision    = 16;		/* minimum significant digits in a double */
    d            = *(double *) from->dsc_address;
    }
else
    {
    assert(dtype_real == from->dsc_dtype);
    precision    = 8;		/* minimum significant digits in a float */
    d            = (double) *(float *) from->dsc_address;
    }

/* If this is a double with non-zero scale, then it is an old-style
   NUMERIC(15, -scale): print it in fixed format with -scale digits
   to the right of the ".". */

if ( (dtype_double == from->dsc_dtype) && (from->dsc_scale < 0) )
    chars_printed = sprintf (temp, "%- #*.*f", width, -from->dsc_scale, d);
else
    chars_printed = LONG_MAX_int; /* sure to be greater than to_len */

/* If it's not an old-style numeric, or the f-format was too long for the
   destination, try g-format with the maximum precision which makes sense
   for the input type: if it fits, we're done. */

if (chars_printed > width)
    {
    chars_printed = sprintf (temp, "%- #*.*g", width, precision, d);

    /* If the full-precision result is too wide for the destination,
       reduce the precision and try again. */
    
    if (chars_printed > width)
	{
	precision -= (chars_printed - width);

	/* If we cannot print at least two digits, one on each side of the
	   ".", report an overflow exception. */
	if (precision < 2)
	    (*err) (gds__arith_except, 0);

	chars_printed = sprintf (temp, "%- #*.*g", width, precision, d);

	/* It's possible that reducing the precision caused sprintf to switch
	   from f-format to e-format, and that the output is still too long
	   for the destination.  If so, reduce the precision once more.
	   This is certain to give a short-enough result. */

	if (chars_printed > width)
	    {
	    precision -= (chars_printed - width);
	    if (precision < 2)
		(*err) (gds__arith_except, 0);
	    chars_printed = sprintf (temp, "%- #*.*g", width, precision, d);
	    }
	}
    } 
assert(chars_printed <= width);

/* Now move the result to the destination array. */

MOVE_CLEAR (&intermediate, sizeof (intermediate));
intermediate.dsc_dtype   = dtype_text;
intermediate.dsc_ttype   = ttype_ascii;
intermediate.dsc_address = temp;
intermediate.dsc_length  = chars_printed;

CVT_move (&intermediate, to, err); 
}

static void integer_to_text (
    DSC         *from,
    DSC         *to,
    FPTR_VOID   err)
{
/**************************************
 *
 *      i n t e g e r _ t o _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Convert your basic binary number to 
 *      nice, formatted text.
 *
 **************************************/
DSC     intermediate;
UCHAR   temp [32], *p, *q;
SSHORT  l, scale, neg, pad, decimal, length;
SINT64  n;
UINT64  u;

#ifndef NATIVE_QUAD
/* For now, this routine does not handle quadwords unless this is
   supported by the platform as a native datatype. */

if (from->dsc_dtype == dtype_quad)
    (*err) (gds__badblk, 0);    /* internal error */
#endif

pad = decimal = neg = 0;

/* Save (or compute) scale of source.  Then convert source to ordinary
   longword or int64. */

scale = from->dsc_scale;

if (scale > 0)
    pad = scale;
else if (scale < 0)
    decimal = 1;

MOVE_CLEAR (&intermediate, sizeof (intermediate));
intermediate.dsc_dtype = dtype_int64;
intermediate.dsc_length = sizeof (n);
intermediate.dsc_scale = scale;
intermediate.dsc_address = (UCHAR*) &n;

CVT_move (from, &intermediate, err);

/* Check for negation, then convert the number to a string of digits */

if (n >= 0)
  u = n;
else
    {
    neg = 1;
    u = -n;
    }

p = temp;

do {
    *p++ = u % 10 + '0';
    u /= 10;
} while (u);

l = p - temp;

/* if scale < 0, we need at least abs(scale)+1 digits, so add
   any leading zeroes required. */
while (l + scale <= 0)
  {
    *p++ = '0';
    l++;
  }
/* postassertion: l+scale > 0 */

/* Compute the total length of the field formatted.  Make sure it
   fits.  Keep in mind that routine handles both string and varying
   string fields. */

length = l + neg + decimal + pad;

if ((to->dsc_dtype == dtype_text    && length > to->dsc_length) ||
    (to->dsc_dtype == dtype_cstring && length >= to->dsc_length) ||
    (to->dsc_dtype == dtype_varying && length > to->dsc_length - sizeof (USHORT)))
    conversion_error (to, err);

q = (to->dsc_dtype == dtype_varying) ? to->dsc_address + sizeof (USHORT) : to->dsc_address;

/* If negative, put in minus sign */

if (neg)
    *q++ = '-';

/* If a decimal point is required, do the formatting.  Otherwise just
   copy number */

if (scale >= 0)
    do *q++ = *--p; while (--l);
else
    {
    l += scale;		/* l > 0 (see postassertion: l+scale > 0 above) */
    do *q++ = *--p; while (--l);
    *q++ = '.';
    do *q++ = *--p; while (++scale);
    }

/* If padding is required, do it now. */

if (pad)
    do *q++ = '0'; while (--pad);

/* Finish up by padding (if fixed) or computing the actual length
   (varying string) */

if (to->dsc_dtype == dtype_text)
    {
    if ((l = to->dsc_length - length) > 0)
	do *q++ = ' '; while (--l);
    return;
    }

if (to->dsc_dtype == dtype_cstring)
    {
    *q = 0;
    return;
    }

*(SSHORT*) (to->dsc_address) = q - to->dsc_address - sizeof (SSHORT);
}

static void string_to_datetime (
   DSC          	*desc,
   GDS_TIMESTAMP        *date,
   EXPECT_DATETIME	expect_type,
   FPTR_VOID    	err)
{
/**************************************
 *
 *      s t r i n g _ t o _ d a t e t i m e
 *
 **************************************
 *
 * Functional description
 *      Convert an arbitrary string to a date and/or time.
 *
 *      String must be formed using ASCII characters only.
 *      Conversion routine can handle the following input formats
 *      "now"           current date & time
 *      "today"         Today's date       0:0:0.0 time
 *      "tomorrow"      Tomorrow's date    0:0:0.0 time
 *      "yesterday"     Yesterday's date   0:0:0.0 time
 *      YYYY-MM-DD [HH:[Min:[SS.[Thou]]]]] 
 *      MM:DD[:YY [HH:[Min:[SS.[Thou]]]]] 
 *      DD.MM[:YY [HH:[Min:[SS.[Thou]]]]] 
 *      Where:
 *        DD = 1  .. 31    (Day of month)
 *        YY = 00 .. 99    2-digit years are converted to the nearest year
 *		           in a 50 year range.  Eg: if this is 1996
 *                              96 ==> 1996
 *                              97 ==> 1997
 *                              ...
 *                              00 ==> 2000
 *                              01 ==> 2001
 *                              ...
 *                              44 ==> 2044
 *                              45 ==> 2045
 *                              46 ==> 1946
 *                              47 ==> 1947
 *                              ...
 *                              95 ==> 1995
 *                         If the current year is 1997, then 46 is converted
 *                         to 2046 (etc).
 *           = 100.. 5200  (Year) 
 *        MM = 1  .. 12    (Month of year)
 *           = "JANUARY"... (etc)
 *        HH = 0  .. 23    (Hour of day)
 *       Min = 0  .. 59    (Minute of hour)
 *        SS = 0  .. 59    (Second of minute - LEAP second not supported)
 *      Thou = 0  .. 9999  (Fraction of second)
 *      HH, Min, SS, Thou default to 0 if missing.
 *      YY defaults to current year if missing.
 *      Note: ANY punctuation can be used instead of : (eg: / - etc)
 *            Using . (period) in either of the first two separation
 *            points will cause the date to be parsed in European DMY
 *            format.
 *            Arbitrary whitespace (space or TAB) can occur between
 *            components.
 *
 **************************************/
TEXT            *string, c, *p, *end;
USHORT          length, n, i, components [7];
USHORT		start_component;
SSHORT		description [7];
/* Values inside of description
     > 0 is number of digits 
       0 means missing 
     ENGLISH_MONTH for the presence of an English month name
     SPECIAL       for a special date verb */
#define		ENGLISH_MONTH	-1
#define		SPECIAL		-2
USHORT		position_year = 0;
USHORT		position_month = 1;
USHORT		position_day = 2;
BOOLEAN		have_english_month = FALSE;
BOOLEAN		dot_separator_seen = FALSE;
SLONG           clock;
struct tm       times, times2;
TEXT            buffer [100];   /* arbitrarily large */

length = CVT_make_string (desc, ttype_ascii, &string, buffer, sizeof (buffer), err);
p = string;

end = p + length;

memset (components, 0, sizeof (components));
memset (description, 0, sizeof (description));

/* Parse components */
/* The 7 components are Year, Month, Day, Hours, Minutes, Seconds, Thou */
/* The first 3 can be in any order */

start_component = (expect_type == expect_sql_time) ? 3 : 0;
for (i = start_component; i < 7; i++)
    {

    /* Skip leading blanks.  If we run out of characters, we're done
       with parse.  */

    while (p < end && (*p == ' ' || *p == '\t'))
	p++;
    if (p == end)
	break;

    /* Handle digit or character strings */

    c = UPPER7 (*p);
    if (DIGIT (c))
	{
	USHORT precision = 0;
	n = 0;
	while (p < end && DIGIT (*p))
	    {
	    n = n * 10 + *p++ - '0';
	    precision++;
	    }
	description [i] = precision;
	}
    else if (LETTER7 (c) && !have_english_month)
	{
	TEXT	temp [sizeof(YESTERDAY)+1], *t;
	CONST   TEXT	* CONST *month_ptr; 
	CONST	TEXT	*m;

	t = temp;
	while ((p < end) && (t < &temp[sizeof(temp)-1])) 
	    {
	    c = UPPER7 (*p);
	    if (!LETTER7 (c))
		break;
	    *t++ = c;
	    p++;
	    }
	*t = 0;

	/* Insist on at least 3 characters for month names */
	if (t - temp < 3)
	    {
	    conversion_error (desc, err);
	    return;
	    }

	month_ptr = months;
	while (TRUE)
	    {
	    /* Month names are only allowed in first 2 positions */
	    if (*month_ptr && i < 2)
		{
		for (t = temp, m = *month_ptr++; *t && *t == *m; t++, m++)
		    ;
		if (!*t)
		    break;
		}
	    else
		{
		/* it's not a month name, so it's either a magic word or
		   a non-date string.  If there are more characters, it's bad */

		description [i] = SPECIAL;

		while (++p < end)
		    if (*p != ' ' && *p != '\t' && *p != 0)
			conversion_error (desc, err);

		/* fetch the current time */

		clock = time (NULL_PTR);
		times2 = *localtime (&clock);

		if (strcmp (temp, NOW) == 0)
		    {
		    isc_encode_timestamp (&times2, date);
		    return;
		    }
		if (expect_type == expect_sql_time)
		    {
		    conversion_error (desc, err);
		    return;
		    }
		times2.tm_hour = times2.tm_min = times2.tm_sec = 0;
		isc_encode_timestamp (&times2, date);
		if (strcmp (temp, TODAY) == 0)
		    return;
		if (strcmp (temp, TOMORROW) == 0)
		    {
		    date->timestamp_date++;
		    return;
		    }
		if (strcmp (temp, YESTERDAY) == 0)
		    {
		    date->timestamp_date--;
		    return;
		    }
		conversion_error (desc, err);
		return;
		}
	    }
	n = month_ptr - months;
	position_month = i;
	description [i] = ENGLISH_MONTH;
	have_english_month++;
	}
    else /* Not a digit and not a letter - must be punctuation */
	{
	conversion_error (desc, err);
	return;
	}

    components [i] = n;

    /* Grab whitespace following the number */
    while (p < end && (*p == ' ' || *p == '\t'))
	p++;

    if (p == end)
	break;

    /* Grab a separator character */
    if (*p == '/' || *p == '-' || *p == ',' || *p == ':')
	{
	p++;
	continue;
	}
    if (*p == '.')
	{
	if (i <= 1)
	    dot_separator_seen++;
	p++;
	continue;
	}
    }

/* User must provide at least 2 components */
if (i-start_component < 1)
    {
    conversion_error (desc, err);
    return;
    }

/* Dates cannot have a Time portion */
if (expect_type == expect_sql_date && i > 2)
    {
    conversion_error (desc, err);
    return;
    }

memset (&times, 0, sizeof (times));

if (expect_type != expect_sql_time)
    {
    /* Figure out what format the user typed the date in */

    /* A 4 digit number to start implies YYYY-MM-DD */
    if (description [0] >= 3)
	{
	position_year = 0;
	position_month = 1;
	position_day = 2;
	}

    /* An English month to start implies MM-DD-YYYY */
    else if (description [0] == ENGLISH_MONTH)
	{
	position_year = 2;
	position_month = 0;
	position_day = 1;
	}

    /* An English month in the middle implies DD-MM-YYYY */
    else if (description [1] == ENGLISH_MONTH)
	{
	position_year = 2;
	position_month = 1;
	position_day = 0;
	}

    /* A period as a separator implies DD.MM.YYYY */
    else if (dot_separator_seen)
	{
	position_year = 2;
	position_month = 1;
	position_day = 0;
	}

    /* Otherwise assume MM-DD-YYYY */
    else
	{
	position_year = 2;
	position_month = 0;
	position_day = 1;
	}

    /* Forbid years with more than 4 digits */
    /* Forbid months or days with more than 2 digits */
    /* Forbid months or days being missing */
    if (description [position_year]  > 4  ||
	description [position_month] > 2  || description [position_month] == 0 ||
	description [position_day]   > 2  || description [position_day] <= 0)
	{
	conversion_error (desc, err);
	return;
	}

    /* Slide things into day, month, year form */

    times.tm_year = components [position_year];
    times.tm_mon = components [position_month];
    times.tm_mday = components [position_day];

    /* Handle defaulting of year */

    if (description [position_year] == 0)
	{
	clock = time (NULL_PTR);
	times2 = *localtime (&clock);
	times.tm_year = times2.tm_year + 1900;
	}

    /* Handle conversion of 2-digit years */

    else if (description [position_year] <= 2)
	{
	clock = time (NULL_PTR);
	times2 = *localtime (&clock);
	if (times.tm_year < (times2.tm_year - 50) % 100)
	    times.tm_year += 2000;
	else
	    times.tm_year += 1900;
	}

    times.tm_year -= 1900;
    times.tm_mon  -= 1;
    }
else
   {
	/* The date portion isn't needed for time - but to
	   keep the conversion in/out of isc_time clean lets
	   intialize it properly anyway */
   times.tm_year = 0;
   times.tm_mon  = 0;
   times.tm_mday = 1;
   }

/* Handle time values out of range - note possibility of 60 for
 * seconds value due to LEAP second (Not supported in V4.0).
 */
if (((times.tm_hour = components [3]) > 23) ||
    ((times.tm_min  = components [4]) > 59) ||
    ((times.tm_sec  = components [5]) > 59) ||
     (description [6] > -ISC_TIME_SECONDS_PRECISION_SCALE))
	conversion_error (desc, err);

/* convert day/month/year to Julian and validate result
   This catches things like 29-Feb-1995 (not a leap year) */

isc_encode_timestamp (&times, date);
if (expect_type != expect_sql_time)
    {
    isc_decode_timestamp (date, &times2);

	if ((times.tm_year+1900) < MIN_YEAR || (times.tm_year)+1900 > MAX_YEAR)
		(*err) (isc_date_range_exceeded, 0);

    if (times.tm_year != times2.tm_year ||
	times.tm_mon  != times2.tm_mon  ||
	times.tm_mday != times2.tm_mday ||
	times.tm_hour != times2.tm_hour ||
	times.tm_min  != times2.tm_min  ||
	times.tm_sec  != times2.tm_sec)
	conversion_error (desc, err);
    };

/* Convert fraction of seconds */
while (description [6]++ < -ISC_TIME_SECONDS_PRECISION_SCALE)
    components[6] *= 10;

date->timestamp_time += components [6];
}

double PASCAL_ROUTINE power_of_ten ( int scale )
{
/*************************************
 *
 *      p o w e r _ o f _ t e n
 *
 *************************************
 *
 * Functional description
 *      return 10.0 raised to the scale power for 0 <= scale < 320.
 *
 *************************************/

  /* Note that we could speed things up slightly by making the auxiliary
   * arrays global to this source module and replacing this function with
   * a macro, but the old code did up to 308 multiplies to our 1, and
   * that seems enough of a speed-up for now.
   */
  
  static const double upper_part[]={ 1.e000, 1.e032, 1.e064, 1.e096, 1.e128,
				     1.e160, 1.e192, 1.e224, 1.e256, 1.e288 };

  static const double lower_part[]={ 1.e00, 1.e01, 1.e02, 1.e03, 1.e04, 1.e05,
				     1.e06, 1.e07, 1.e08, 1.e09, 1.e10, 1.e11,
				     1.e12, 1.e13, 1.e14, 1.e15, 1.e16, 1.e17,
				     1.e18, 1.e19, 1.e20, 1.e21, 1.e22, 1.e23,
				     1.e24, 1.e25, 1.e26, 1.e27, 1.e28, 1.e29,
				     1.e30, 1.e31 };
  
  /* The sole caller of this function checks for scale <= 308 before calling,
   * but we just assert the weakest precondition which lets the code work.
   * If the size of the exponent field, and thus the scaling, of doubles
   * gets bigger, increase the size of the upper_part array.
   */
  assert((scale >= 0) && (scale < 320));
  
  /* Note that "scale >> 5" is another way of writing "scale / 32",
   * while "scale & 0x1f" is another way of writing "scale % 32".
   * We split the scale into the lower 5 bits and everything else,
   * then use the "everything else" to index into the upper_part array,
   * whose contents increase in steps of 1e32.
   */
  return upper_part[scale >> 5] * lower_part[scale & 0x1f];
}
