/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		picstr.c
 *	DESCRIPTION:	Picture String Handler
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
#include "../qli/format.h"
#include "../qli/all_proto.h"
#include "../qli/err_proto.h"
#include "../qli/picst_proto.h"
#include "../qli/mov_proto.h"
#else
#include "../pyxis/pyxis.h"
#endif
#include "../jrd/time.h"
#if (defined JPN_SJIS || defined JPN_EUC)
#include "../jrd/kanji.h"
#endif
#include "../jrd/gds_proto.h"

#define PRECISION	10000

static TEXT	*cvt_to_ascii (SLONG, TEXT *, int);
static TEXT	*default_edit_string (DSC *, TEXT *);
static void	edit_alpha (DSC *, PIC, TEXT **, USHORT);
static void	edit_date (DSC *, PIC, TEXT **);
static void	edit_float (DSC *, PIC, TEXT **);
static void	edit_numeric (DSC *, PIC, TEXT **);
static int	generate (PIC);
static void	literal (PIC, TEXT, TEXT **);

#ifdef PYXIS
#define PIC_analyze		PICSTR_analyze
#define PIC_edit		PICSTR_edit
#define default_edit_string	PICSTR_default_edit_string

#define MOVQ_get_double		MOVP_get_double
#define MOVQ_get_string		MOVP_get_string
#define MOVQ_move		MOVP_move

extern double PASCAL_ROUTINE	MOVQ_get_double();
#endif

#ifdef PYXIS
TEXT		*PICSTR_default_edit_string();
#endif

static TEXT	*alpha_weekdays [] =
		    {
		    "Sunday",
		    "Monday",
		    "Tuesday",
		    "Wednesday",
		    "Thursday",
		    "Friday",
		    "Saturday"
		    },
		*alpha_months [] =
		    {
		    "January",
		    "February",
		    "March",
		    "April",
		    "May",
		    "June",
		    "July",
		    "August",
		    "September",
		    "October",
		    "November",
		    "December"
		    };

PIC PIC_analyze (
    TEXT	*string,
    DSC		*desc)
{
/**************************************
 *
 *	P I C _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *	Analyze a picture in preparation for formatting.
 *
 **************************************/
PIC	picture;
TEXT	c, d, *p, debit;
USHORT	length;

if (!string)
    if (!desc)
	return NULL;
    else
	string = default_edit_string (desc, NULL);

debit = 0;

picture = (PIC) ALLOCD (type_pic);
picture->pic_string = picture->pic_pointer = string;

/* Make a first pass just counting characters */

while ((c = generate (picture)) && c != '?')
    {
#ifdef JPN_SJIS
    
    /* Be sure to lex double byte sjis characters correctly */

    if SJIS1(c)
	{
	++picture->pic_literals;
	picture->pic_flags |= PIC_literal;
	if (generate (picture))
	    {
	    ++picture->pic_literals;
	    picture->pic_flags &= ~PIC_literal;
	    continue;
	    }
	else
	    {
	    picture->pic_flags &= ~PIC_literal;
	    break;
	    }
	}
#endif

    c = UPPER (c);
    switch (c)
	{
	case 'X':
	case 'A':
	    ++picture->pic_chars;
	    break;
	
	case '9':
	case 'Z':
	case '*':
            /* Count all digits; 
               count them as fractions only after a decimal pt and 
               before an E */
	    ++picture->pic_digits;                              
	    if (picture->pic_decimals && !picture->pic_exponents)
		++picture->pic_fractions;
	    break;
	
	case 'H':
	    ++picture->pic_hex_digits;
	    break;

	case '.':
	    ++picture->pic_decimals;
	    break;
	
	case '-':
	case '+':
	    picture->pic_flags |= PIC_signed;
	case '$':
	    if (picture->pic_chars || picture->pic_exponents)
		++picture->pic_literals;
	    else if (picture->pic_floats)
		++picture->pic_digits;
	    else
		++picture->pic_floats;
	    break;

	case 'M':
	    ++picture->pic_months;
	    break;

	case 'D':
	    /* DB is ambiguous, could be Day Blank or DeBit... */
            d = UPPER (*picture->pic_pointer);
	    if (d == 'B')
		{
		++picture->pic_pointer;
		++picture->pic_literals;
		++debit;
		}
	    ++picture->pic_days;
	    break;

	case 'Y':
	    ++picture->pic_years;
	    break;

	case 'J':
	    ++picture->pic_julians;
	    break;

	case 'W':
	    ++picture->pic_weekdays;
	    break;

	case 'N':
	    ++picture->pic_nmonths;
	    break;

	case 'E':
	    ++picture->pic_exponents;
	    break;

	case 'G':
	    ++picture->pic_float_digits;
	    break;

	case '(':
	case ')':
	    picture->pic_flags |= PIC_signed;
	    ++picture->pic_brackets;
	    break;

	case '\'':
	case '"':
	    picture->pic_flags |= PIC_literal;
	    while ((d = generate (picture)) && d != c)
		++picture->pic_literals;
	    picture->pic_flags &= ~PIC_literal;
	    break;

	case '\\':
	    ++picture->pic_literals;
	    picture->pic_flags |= PIC_literal;
	    generate (picture);
	    picture->pic_flags &= ~PIC_literal;
	    break;

	case 'C':
	case 'R':
	    picture->pic_flags |= PIC_signed;
	    ++picture->pic_brackets;
	    if (d = generate (picture))
		++picture->pic_brackets;
	    else
		++picture->pic_count;
	    break;

        case 'P':
	    ++picture->pic_meridian;
	    break;

        case 'T':
	    if (picture->pic_hours < 2)
		++picture->pic_hours;
	    else if (picture->pic_minutes < 2)
		++picture->pic_minutes;
	    else
		++picture->pic_seconds;
	    break;

	case 'B':
	case '%':
	case ',':
	case '/':
	default:
	    ++picture->pic_literals;
	    break;
	}
    }

if (c == '?')
    picture->pic_missing = PIC_analyze (picture->pic_pointer, NULL_PTR);

/* if a DB showed up, and the string is numeric, treat the DB as DeBit */

if (debit && (picture->pic_digits || picture->pic_hex_digits))
	{
	--picture->pic_days;
	--picture->pic_literals;
	picture->pic_flags |= PIC_signed;
	++picture->pic_brackets;
	++picture->pic_brackets;
	}

picture->pic_print_length = 
	picture->pic_digits +
	picture->pic_hex_digits +
	picture->pic_chars +
	picture->pic_floats +
	picture->pic_literals +
	picture->pic_decimals +
	picture->pic_months +
	picture->pic_days +
	picture->pic_weekdays +
	picture->pic_years +
	picture->pic_nmonths +
	picture->pic_julians +
	picture->pic_brackets +
	picture->pic_exponents +
	picture->pic_float_digits +
	picture->pic_hours +
	picture->pic_minutes +
	picture->pic_seconds +
	picture->pic_meridian;

 
if (picture->pic_missing)
    {
    picture->pic_length = 
	MAX (picture->pic_print_length, picture->pic_missing->pic_print_length);
    picture->pic_missing->pic_length = picture->pic_length;
    }
else
    picture->pic_length = picture->pic_print_length;

if (picture->pic_days ||
    picture->pic_weekdays ||
    picture->pic_months ||
    picture->pic_nmonths ||
    picture->pic_years ||
    picture->pic_hours ||
    picture->pic_julians)
	picture->pic_type = pic_date;
else if (picture->pic_exponents || picture->pic_float_digits)
    picture->pic_type = pic_float;
else if (picture->pic_digits || picture->pic_hex_digits)
    picture->pic_type = pic_numeric;
else
    picture->pic_type = pic_alpha;
              
return picture;
}

int PIC_edit (
    DSC		*desc,
    PIC		picture,
    TEXT	**output,
    USHORT	max_length)
{
/**************************************
 *
 *	P I C _ e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit data from a descriptor through an edit string to a running
 *	output pointer.  For text strings, check that we don't overflow
 *	the output buffer.
 *
 **************************************/

switch (picture->pic_type)
    {
    case pic_alpha:	edit_alpha (desc, picture, output, max_length); return;
    case pic_numeric:	edit_numeric (desc, picture, output); return;
    case pic_date:	edit_date (desc, picture, output); return;
    case pic_float:	edit_float (desc, picture, output); return;
    default:
#ifndef PYXIS
	BUGCHECK ( 68 ); /* Msg 68 PIC_edit: class not yet implemented */
#else
	BUGCHECK ("PIC_edit: class not yet implemented");
#endif
    }
}

#ifndef PYXIS
void PIC_missing (
    CON		constant,
    PIC		picture)
{
/**************************************
 *
 *	P I C _ m i s s i n g
 *
 **************************************
 *
 * Functional description
 *	Create a literal picture string from
 *	a descriptor for a missing value so
 *	we can print the missing value  
 *
 **************************************/
STR	scratch;
int	l;
TEXT	*p;
PIC	missing_picture;
DSC	*desc;

desc = &constant->con_desc;

l = MAX (desc->dsc_length, picture->pic_length);

scratch = (STR) ALLOCDV (type_str, l + 3);
p = scratch->str_data;
*p++ = '\"';

PIC_edit (desc, picture, &p, l);
*p++ = '\"';
*p = 0;

picture->pic_missing = missing_picture = PIC_analyze (scratch->str_data, desc);
picture->pic_length = 
    MAX (picture->pic_print_length, missing_picture->pic_print_length);
missing_picture->pic_length = picture->pic_length;
}
#endif

static TEXT *cvt_to_ascii (
    SLONG	number,
    TEXT	*pointer,
    int		length)
{
/**************************************
 *
 *	c v t _ t o _ a s c i i
 *
 **************************************
 *
 * Functional description
 *	Convert a number to a number of ascii digits (plus terminating
 *	null), updating pointer.
 *
 **************************************/
TEXT	*p;

pointer += length + 1;
p = pointer;
*--p = 0;

while (--length >= 0)
    {
    *--p = (number % 10) + '0';
    number /= 10;
    }

return pointer;
}

#ifndef PYXIS
static TEXT *default_edit_string (
#else
TEXT *PICSTR_default_edit_string (
#endif
    DSC		*desc,
    TEXT	*buff)
{
/**************************************
 *
 *	d e f a u l t _ e d i t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Given a skeletal descriptor, generate a default edit string.
 *
 **************************************/
TEXT	buffer [32];
STR	string;
SSHORT	scale;

if (!buff)
    buff = buffer;

scale = desc->dsc_scale;

switch (desc->dsc_dtype)
    {
    case dtype_text:
	sprintf (buff, "X(%d)", desc->dsc_length);
	break;

    case dtype_cstring:
	sprintf (buff, "X(%d)", desc->dsc_length - 1);
	break;
    
    case dtype_varying:
	sprintf (buff, "X(%d)", desc->dsc_length - 2);
	break;
    
    case dtype_short:
	if (!scale)
	    return "-(5)9";
	if (scale < 0 && scale > -5)
	    sprintf (buff, "-(%d).9(%d)", 6 + scale, -scale);
	else if (scale < 0)
	    sprintf (buff, "-.9(%d)", -scale);
	else
	    sprintf (buff, "-(%d)9", 5 + scale);
	break;

    case dtype_long:
	if (!scale)
	    return "-(10)9";
	if (scale < 0 && scale > -10)
	    sprintf (buff, "-(%d).9(%d)", 10 + scale, -scale);
	else if (scale < 0)
	    sprintf (buff, "-.9(%d)", -scale);
	else
	    sprintf (buff, "-(%d)9", 11 + scale);
	break;
    
#if ( (defined JPN_SJIS || defined JPN_EUC) && defined JPN_DATE) 

    /* For Japanese the default date format is "YYYY.NN.DD" */

    case dtype_sql_date:
    case dtype_timestamp:
	return "YYYY.NN.DD";

#else

    case dtype_sql_date:
    case dtype_timestamp:
	return "DD-MMM-YYYY";

#endif
    case dtype_sql_time:
	return "TT:TT:TT.TTTT";
    
    case dtype_real:
	return "G(8)";

    case dtype_double:
	return "G(16)";

    default:
	return "X(11)";
    }

if (buff == buffer)
    {
    string = (STR) ALLOCDV (type_str, strlen (buff));
    strcpy (string->str_data, buff);
    buff = string->str_data;
    }

return buff;
}

static void edit_alpha (
    DSC		*desc,
    PIC		picture,
    TEXT	**output,
    USHORT	max_length)
{
/**************************************
 *
 *	e d i t _ a l p h a
 *
 **************************************
 *
 * Functional description
 *	Edit data from a descriptor through an edit string to a running
 *	output pointer.
 *
 **************************************/
TEXT	c, d, *p, *out, *end, temp [512], *q, date [32];
USHORT	l, i;

l = MOVQ_get_string (desc, &p, temp, sizeof (temp));
end = p + l;
picture->pic_pointer = picture->pic_string;
picture->pic_count = 0;
out = *output;

while (p < end)
    {
    if ((out - *output) >= max_length)
	break;
    c = generate (picture);
    if (!c || c == '?')
	break;

#if (defined JPN_SJIS || defined JPN_EUC)
    
    /* Be sure to group double byte characters correctly */

    if KANJI1(c)
	{
	*out++ = c;
	picture->pic_flags |= PIC_literal;
	c = generate(picture);
	    
	/* Make sure it is not a truncated kanji else punt */
	
	if (!c)
	    IBERROR (69);

	/* Try to put the second byte in the output buffer 
	   If both bytes dont fit in, put a single space and break*/
	
	if ((out - *output) >= max_length)
	    {
	    *(out -1) = ' ';
	    break;
	    }
	else
	    {
	    *out++ = c;
	    picture->pic_flags &= ~PIC_literal;
	    continue;
	    }
	}
#endif

    c = UPPER (c);
    switch (c)
	{
	case 'X':
#if (defined JPN_SJIS || defined JPN_EUC)

	    *out++ = *p++;

	    /* Make sure double bytes are handled correctly */

	    if (KANJI1(*(out-1)))
		{
		c = generate(picture);
		if (!c || (out - *output) >= max_length)
		    {
            	    *(out -1) = ' ';
		    *output = out;
		    return;
		    }
		if (c != 'X' || p == end)
		    {
		    IBERROR (69);
		    }
		*out++ = *p++;
		}
	    break;
#endif 

#ifdef PYXIS
	case 'A':
#endif
	    *out++ = *p++;
	    break;

#ifndef PYXIS
	case 'A':
	    if ((*p >= 'a' && *p <='z') ||
		(*p >= 'A' && *p <='Z'))
		*out++ = *p++;
	    else 
		IBERROR ( 69 ); /* Msg 69 conversion error */
	    break;
#endif
  
	case 'B':
	    *out++ = ' ';
	    break;

	case '"':
	case '\'':
	case '\\':
	    literal (picture, c, &out);
	    break;

	default:
	    *out++ = c;
	    break;
	}
     }

*output = out;
}

static void edit_date (
    DSC		*desc,
    PIC		picture,
    TEXT	**output)
{
/**************************************
 *
 *	e d i t _ d a t e
 *
 **************************************
 *
 * Functional description
 *	Edit data from a descriptor through an edit string to a running
 *	output pointer.
 *
 **************************************/
SLONG		date [2], rel_day, seconds;
DSC		temp_desc;
TEXT		c, d, *p, *out, *month, *weekday, *year, *nmonth, *day,
		*hours, temp [256], *meridian, *julians;
USHORT		l, sig_day, blank;
struct tm	times;

temp_desc.dsc_dtype = dtype_timestamp;
temp_desc.dsc_scale = 0;
temp_desc.dsc_sub_type = 0;
temp_desc.dsc_length = sizeof (date);
temp_desc.dsc_address = (UCHAR*) date;
MOVQ_move (desc, &temp_desc);

#ifdef PYXIS
if (!date [0] && !date [1])
    return;
#endif

isc_decode_date (date, &times);
p = temp;

nmonth = p;
p = cvt_to_ascii ((SLONG) times.tm_mon + 1, p, picture->pic_nmonths);

day = p;
p = cvt_to_ascii ((SLONG) times.tm_mday, p, picture->pic_days);

year = p;
p = cvt_to_ascii ((SLONG) times.tm_year + 1900, p, picture->pic_years);

julians = p;
p = cvt_to_ascii ((SLONG) times.tm_yday + 1, p, picture->pic_julians);

if (picture->pic_meridian)
    {
    if (times.tm_hour >= 12)
	{
	meridian = "PM";
	if (times.tm_hour > 12)
	    times.tm_hour -= 12;
	}
    else
	meridian = "AM";
    }
else
    meridian = "";

seconds = date[1] % (60 * PRECISION);

hours = p;
p = cvt_to_ascii ((SLONG) times.tm_hour, p, picture->pic_hours);
p = cvt_to_ascii ((SLONG) times.tm_min, --p, picture->pic_minutes);
p = cvt_to_ascii ((SLONG) seconds, --p, 6);

if (*hours == '0')
    *hours = ' ';

if ((rel_day = (date [0] + 3) % 7) < 0)
    rel_day += 7;
weekday = alpha_weekdays [rel_day];
month = alpha_months [times.tm_mon];

picture->pic_pointer = picture->pic_string;
picture->pic_count = 0;
out = *output;
sig_day = FALSE;
blank = TRUE;

for (;;)
    {
    c = generate (picture);
    if (!c || c == '?')
	break;
#if (defined JPN_SJIS || defined JPN_EUC)
    
    if KANJI1(c)
	{
        
	/* group double byte characters correctly */

	*out++ = c;
	picture->pic_flags |= PIC_literal;
	c = generate(picture);
	    
	/* Make sure it is not a truncated kanji else punt */
	
	if (!c)
	    IBERROR (69);

	*out++ = c;
	picture->pic_flags &= ~PIC_literal;
	continue;
	}
#endif

    c = UPPER (c);

    switch (c)
	{
	case 'Y':
	    *out++ = *year++;
	    break;

	case 'M':
	    if (*month)
		*out++ = *month++;
	    break;

	case 'N':
	    *out++ = *nmonth++;
	    break;

	case 'D':
	    d = *day++;
	    if (!sig_day && d == '0' && blank)
		*out++ = ' ';
	    else
		{
		sig_day = TRUE;
		*out++ = d;
		}
	    break;

	case 'J':
	    if (*julians)
		*out++ = *julians++;
	    break;

	case 'W':
	    if (*weekday)
		*out++ = *weekday++;
	    break;

	case 'B':
	    *out++ = ' ';
	    break;

 	case 'P':
	    if (*meridian)
		*out++ = *meridian++;
	    break;

        case 'T':
	    if (*hours)
	        *out++ = *hours++;
	    break;

	case '"':
	case '\'':
	case '\\':
 	    literal (picture, c, &out);                       
	    break;

	default:
	    *out++ = c;
	    break;
	}
     if (c != 'B')
	blank = FALSE;     
     }

*output = out;
}

static void edit_float (
    DSC		*desc,
    PIC		picture,
    TEXT	**output)
{
/**************************************
 *
 *	e d i t _ f l o a t
 *
 **************************************
 *
 * Functional description
 *	Edit data from a descriptor through an edit string to a running
 *	output pointer.
 *
 **************************************/
TEXT	c, d, e, *p, *out, temp [512];
BOOLEAN negative, is_signed;
USHORT	l,  width, decimal_digits, w_digits, f_digits;
double	number;

#ifdef VMS
BOOLEAN	hack_for_vms_flag;
#endif
#ifdef WIN_NT
BOOLEAN	hack_for_nt_flag;
#endif

negative = is_signed = FALSE;
#ifdef VMS
hack_for_vms_flag = FALSE;
#endif
#ifdef WIN_NT
hack_for_nt_flag = FALSE;
#endif
number = MOVQ_get_double (desc);
if (number < 0) 
    {
    negative = TRUE;      
    number = -number;
    }                                                    

/* If exponents are explicitly requested (E-format edit_string), generate them.
   Otherwise, the rules are: if the number in f-format will fit into the allotted
   space, print it in f-format; otherwise print it in e-format.  
   (G-format is untrustworthy.) */

if (picture->pic_exponents)                 
    {      
    width = picture->pic_print_length - picture->pic_floats - picture->pic_literals;
    decimal_digits = picture->pic_fractions;        
    sprintf (temp, "%*.*e", width, decimal_digits, number);
#ifdef VMS
    if (!decimal_digits)
	hack_for_vms_flag = TRUE;
#endif
#ifdef WIN_NT
    hack_for_nt_flag = TRUE;
#endif
    }
else if (number == 0)
    sprintf (temp, "%.0f", number);
else     
    {
    width = picture->pic_float_digits - 1 + picture->pic_floats;
    f_digits  = (width > 2) ? width - 2 : 0;
    sprintf (temp, "%.*f", f_digits, number);
    w_digits = strlen (temp);
    if (f_digits)
        {
        p = temp + w_digits;   /* find the end */
        w_digits = w_digits - (f_digits + 1);  
        while (*--p == '0')
            --f_digits;                     
        if (*p != '.')
            ++p;
        *p = 0;            /* move the end */
        }
    if ((w_digits > width) || (!f_digits && w_digits == 1 && temp[0] == '0')) 
        {
	/* if the number doesn't fit in the default window, revert
	   to exponential notation; displaying the maximum number of
	   mantissa digits. */

	if (number < 1e100)
            decimal_digits = (width > 6) ? width - 6 : 0;
	else
	    decimal_digits = (width > 7) ? width - 7 : 0;
        sprintf (temp, "%.*e", decimal_digits, number);
#ifdef VMS
        if (!decimal_digits)
	    hack_for_vms_flag = TRUE;
#endif
#ifdef WIN_NT
	hack_for_nt_flag = TRUE;
#endif
        }
    }

#ifdef VMS
/* If precision of 0 is specified to e- or f-format in VMS, the dec point
   is printed regardless.  On no other platform is this the case; so take
   it out here.  When/if this is ever fixed, ALL LINES IN THIS ROUTINE
   THAT RELATED TO 'hack_for_vms_flag' MAY BE DELETED. */

if (hack_for_vms_flag)
    {
    p = temp;
    while (*p != '.') ++p;
    while (*p = *(p + 1)) ++p;
    }
#endif
#ifdef WIN_NT
/* On Windows NT exponents have three digits regardless of the magnitude
   of the number being formatted.  To maintain compatiblity with other
   platforms, if the first digit of the exponent is '0', shift the other
   digits one to the left. */

if (hack_for_nt_flag)
    {
    p = temp;
    while (*p != 'e' && *p != 'E') ++p;
    p += 2;
    if (*p == '0')
	while (*p = *(p + 1)) ++p;
    }
#endif

p = temp;
picture->pic_pointer = picture->pic_string;
picture->pic_count = 0;
out = *output;

for (l = picture->pic_length - picture->pic_print_length; l > 0; --l)
    *out++ = ' ';

for (;;)
    {
    c = e = generate (picture);
    if (!c || c == '?')
	break;
#if (defined JPN_SJIS || defined JPN_EUC)
    
    if KANJI1(c)
	{
	/* group double byte characters correctly */

	*out++ = c;
	picture->pic_flags |= PIC_literal;
	c = generate (picture);
	    
	/* Make sure it is not a truncated kanji else punt */
	
	if (!c)
	    IBERROR (69);

	*out++ = c;
	picture->pic_flags &= ~PIC_literal;
	continue;
	}
#endif
    
    c = UPPER (c);
    switch (c)
	{
	case 'G':
            if (!is_signed)
               {
               if (negative)
                   *out++ = '-';
               else
                   *out++ = ' ';
               is_signed = TRUE;
               }
	    else if (*p)
		*out++ = *p++;
	    break;

	case 'B':
	    *out++ = ' ';
	    break;

	case '"':
	case '\'':
	case '\\':
	    literal (picture, c, &out);
	    break;

        case '9':
        case 'Z': 
            if (!(*p) || *p > '9' || *p < '0')
                break;
            d = *p++; 
            if (c == '9' && d == ' ')
                d = '0';
            else if (c == 'Z' && d == '0')
                d = ' ';
            *out++ = d;
            break;

        case '.':
            *out++ = (*p == c) ? *p++ : c;
            break;

        case 'E':     
            if (!(*p))
                break;
            *out++ = e;
            if (UPPER (*p) == c)
                ++p;
            break;

	case '+':
	case '-':     
            if (!(*p))
                break;      
            if (*p != '+' && *p != '-')
                {
                if (is_signed) 
                    *out++ = c;
                else if (c == '-' && !negative)
		    *out++ = ' ';
                else if (c == '+' && negative)
		    *out++ = '-';
                else 
                    *out++ = c;
                is_signed = TRUE;
                }
            else if (*p == '-' || c == '+')
                *out++ = *p++;                             
            else
                {
                *out++ = ' ';
                p++;
                }
	    break;                  

	default:
	    *out++ = c;
	    break;
	}
     }

*output = out;
}

static void edit_numeric (
    DSC		*desc,
    PIC		picture,
    TEXT	**output)
{
/**************************************
 *
 *	e d i t _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *	Edit data from a descriptor through an edit string to a running
 *	output pointer.
 *
 **************************************/
TEXT	c, d, float_char, temp [512], *p, *float_ptr, *out, *hex, *digits;
USHORT	power, negative, signif, hex_overflow, overflow, l;
SSHORT	scale;
SLONG	n;
double	check, number;

out = *output;
float_ptr = NULL;
negative = signif = FALSE;
hex_overflow = overflow = FALSE;

number = MOVQ_get_double (desc);
if (number < 0) 
    {
    number = -number;
    negative = TRUE;
    if (!(picture->pic_flags & PIC_signed))
	overflow = TRUE;
    }

if (scale = picture->pic_fractions)
    if (scale < 0)
	do number /= 10.; while (++scale);
    else if (scale > 0)
	do number *= 10.; while (--scale);

digits = p = temp;

if (picture->pic_digits && !overflow)
    {
    for (check = number, power = picture->pic_digits; power; --power)
	check /= 10.;
    if (check >= 1)
	overflow = TRUE;
    else
	{
	sprintf (digits, "%0*.0f", picture->pic_digits, number);
	p = digits + strlen (digits);
	}
    }

picture->pic_pointer = picture->pic_string;
if (picture->pic_hex_digits)
    {
    hex = p;
    p += picture->pic_hex_digits;
    for (check = number, power = picture->pic_hex_digits; power; --power)
	check /= 16.;
    if (check >= 1)
	hex_overflow = TRUE;
    else
	{
	n = number;
	while (p-- > hex)
	    {
	    *p = "0123456789abcdef" [n & 15];
	    n >>= 4;
	    }
	}
    }

for (l = picture->pic_length - picture->pic_print_length; l-- > 0;)
    *out++ = ' ';

n = (number + 0.5 < 1) ? 0 : 1;

for (;;)
    {
    c = generate (picture);
    if (!c || c == '?')
	break;
#if (defined JPN_SJIS || defined JPN_EUC)
    
    if KANJI1(c)
	{
	/* group double byte characters correctly */

	*out++ = c;
	picture->pic_flags |= PIC_literal;
	c = generate (picture);
	    
	/* Make sure it is not a truncated kanji else punt */
	
	if (!c)
	    IBERROR (69);

	*out++ = c;
	picture->pic_flags &= ~PIC_literal;
	continue;
	}
#endif

    c = UPPER (c);
    if (overflow && c != 'H')
        {
	*out++ = '*';
	continue;
	}
    switch (c)
	{
	case '9':
	    signif = TRUE;
	    *out++ = *digits++;
	    break;

	case 'H':
	    if (hex_overflow)
	        {
		*out++ = '*';
		continue;
		}
	case '*':
	case 'Z':
	    d = (c == 'H') ? *hex++ : *digits++;
	    if (signif || d != '0')
		{
		*out++ = d;
		signif = TRUE;
		}
	    else
		*out++ = (c == '*') ? '*' : ' ';
	    break;

	case '+':
	case '-':
	case '$':
	    if (c == '+' && negative)
		c = '-';
	    else if (c == '-' && !negative)
		c = ' ';
	    float_char = c;
	    if (!float_ptr)
		{
		float_ptr = out;
		*out++ = (n) ? c : ' ';
		break;
		}
	    d = *digits++;
	    if (signif || d != '0')
		{
		*out++ = d;
		signif = TRUE;
		break;
		}
	    *float_ptr = ' ';
	    float_ptr = out;
	    *out++ = (n) ? c : ' ';
	    break;

	case '(':
	case ')':
	    *out++ = (negative) ? c : ' ';
	    break;

	case 'C':
	case 'D':
	    d = generate (picture);
	    if (negative)
		{
		*out++ = c;
		*out++ = UPPER (d);
		}
	    else if (d)
		{
		*out++ = ' ';
		*out++ = ' ';
		}
	    break;

	case '.':
	    signif = TRUE;
	    *out++ = c;
	    break;

	case 'B':
	    *out++ = ' ';
	    break;

	case '"':
	case '\'':
	case '\\':
	    literal (picture, c, &out);
	    break;

	case ',':
	    if (signif)
		*out++ = c;
	    else if (float_ptr)
		{
		*float_ptr = ' ';
		float_ptr = out;
		*out++ = float_char;
		}
	    else
		*out++ = ' ';
	    break;

	default:
	    *out++ = c;
	    break;
	}
     if (picture->pic_flags & PIC_suppress_blanks &&
	 out [-1] == ' ')
	--out;
     }

*output = out;
}

static int generate (
    PIC		picture)
{
/**************************************
 *
 *	g e n e r a t e
 *
 **************************************
 *
 * Functional description
 *	Generate the next character from a picture string.
 *
 **************************************/
TEXT	c, *p;

for (;;)
    {
    /* If we're in a repeat, pass it back */

    if (picture->pic_count > 0)
	{
	--picture->pic_count;
	return picture->pic_character;
	}

    /* Get the next character.  If null, we're done */

    c = *picture->pic_pointer++;

    /* If we're in literal mode, just return the character */

    if (picture->pic_flags & PIC_literal)
	break;

    /* If the next character is also a paren, it is a debit indicating
       bracket.  If so, swallow the second. */

    if ((c == ')' || c == '(') && *picture->pic_pointer == c)
	{
	picture->pic_pointer++;
	return (picture->pic_character = c);
	}

    /* If the character is null and not a repeat count, we're done */

    if (!c || c != '(')
	break;

    /* We're got a potential repeat count.  If real, extract it. */

    p = picture->pic_pointer;
    while (*p >= '0' && *p <= '9')
	picture->pic_count = picture->pic_count * 10 + *p++ - '0';
    
    if (p == picture->pic_pointer)
	{
	c = '(';
	break;
	}
    
    if (*p == ')')
	++p;

    picture->pic_pointer = p;           

    /* Someone may have done something very stupid, like specify a repeat
       count of zero.  It's too late, as we've already gen'd one instance
       of the edit character -- but let's not access violate, shall we? */

    if (picture->pic_count)
        --picture->pic_count;
    }

return (picture->pic_character = c);
}

static void literal (
    PIC		picture,
    TEXT	c,
    TEXT	**ptr)
{
/**************************************
 *
 *	l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Handle a literal string in a picture string.
 *
 **************************************/
TEXT	*p, d;

p = *ptr;
picture->pic_flags |= PIC_literal;

if (c == '\\')
    *p++ = generate (picture);
else
    while ((d = generate (picture)) && d != c)
	*p++ = d;

*ptr = p;
picture->pic_flags &= ~PIC_literal;
}
