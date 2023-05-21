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
#include <time.h>

/*==============================================================
  This module contains example user defined functions.  The
  file udf.gdl contains the gdef input necessary to add these
  functions to the atlas database.
================================================================ */


/* variable to return values in.  Needs to be global so
   it doesn't go away as soon as the function invocation 
   is finished */

static char buffer[256];

#ifdef SHLIB_DEFS
#define time		(*_libfun_time)
#define ctime		(*_libfun_ctime)
#define strcpy		(*_libfun_strcpy)
#define strlen		(*_libfun_strlen)

extern time_t	time();
extern char	*ctime();
extern char	*strcpy();
extern int	strlen();
#endif

/*===============================================================
 fn_abs() - returns the absolute value of its argument.

	define function abs
	  module_name 'FUNCLIB'
	  entry_point 'FN_ABS'
	  double by reference,
	  double by reference return_value;

================================================================= */
double fn_abs(x)
    double	*x;
{
double	y;
char	*p, *q, *end;

if ((y = *x) < 0.)
    y = -y;

return y;
}

/*===============================================================
 fn_upper_non_c() - Puts its argument into upper case for non-C

	define function upper_non_c
	  module_name 'FUNCLIB'
	  entry_point 'FN_UPPER_NON_C'
	  char[256] by reference,
	  short	by reference;
	  char[256] by reference return_value;

================================================================= */
char *fn_upper_non_c (s, length)
    char	*s;
    short	*length;
{
char	*buf, *end;
short	len;

buf = buffer;
end = buffer + sizeof (buffer);
len = *length;

while (*s && buf < end && len--)
    if (*s >= 'a' && *s <= 'z')
        *buf++ = *s++ - 'a' + 'A'; 
    else
	*buf++ = *s++;

while (buf < end)
    *buf++ = ' ';

return buffer;
}

/*===============================================================
 fn_upper_c() - Puts its argument into upper case, for C programs

	define function upper_c
	  module_name 'FUNCLIB'
	  entry_point 'FN_UPPER_C'
	  cstring [256] by reference,
	  cstring [256] by reference return_value;

================================================================= */
char *fn_upper_c (s)
    char	*s; 
{
char	*buf;

for (buf = buffer; *s;)
    if (*s >= 'a' && *s <= 'z')
        *buf++ = *s++ - 'a' + 'A'; 
    else
	*buf++ = *s++;

*buf = '\0';

return buffer;
}

/*===============================================================
 fn_max() - Returns the greater of its two arguments

	define function maxnum
	  module_name 'FUNCLIB'
	  entry_point 'FN_MAX'
	  double by reference,
	  double by reference,
	  double by reference return_value;
================================================================ */
double fn_max(a,b)
    double	*a, *b;
{      
return  (*a > *b) ? *a : *b;
}

/*===============================================================
 fn_time() - Returns the current time 

	define function time
	  module_name 'FUNCLIB'
	  entry_point 'FN_TIME'
	  char[35] by reference return_value;

================================================================= */
char *fn_time()
{
int	i; 
time_t	time_int;
char	*buf, *end, *time_str;

strcpy (buffer, "The time is now ");
buf = buffer + strlen(buffer);

time (&time_int);

time_str = ctime (&time_int) + 11;

for (i = 0; i < 8; i++)
   *buf++ = *time_str++;

for (end = buffer + sizeof (buffer); buf < end;) 
    *buf++ = ' ';

return buffer;
}
