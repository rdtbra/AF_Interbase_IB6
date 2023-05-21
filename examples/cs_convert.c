/*==============================================================
  This module contains examples of user defined functions.  

  These functions perform character set translation between
  different text types.
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
================================================================ */


/* variable to return values in.  Needs to be global so
   it doesn't go away as soon as the function invocation 
   is finished */

static unsigned char buffer[256];

/*
 *	Policy for character glyphs which don't map between character sets
 *	is to map the glyph to a space character
 */

#define CANT	' '

#include "437_to_lat1.h" 
#include "865_to_lat1.h"
#include "437_to_865.h"


/*===============================================================
 n_convert_*_to_*() - 
	Convert's it's argument - for use from non-C languages

	define function n_convert_latin1_to_437
	  module_name 'CONVERT'
	  entry_point 'n_convert_latin1_to_437'
	  char[256] sub_type 153 by reference,
	  short	by reference;
	  char[256] sub_type 101 by reference return_value;

================================================================= */
static unsigned char	*nconverter( table, string, length )
unsigned char	*table;
unsigned char	*string;
short		*length;
{
unsigned char	*buf, *end;
short	len;

buf = buffer;
end = buffer + sizeof (buffer);
len = *length;

while (*string && buf < end && len--)
    *buf++ = table[ *string++ ];

while (buf < end)
    *buf++ = ' ';

return buffer;
}


unsigned char *n_convert_865_to_latin1 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_865_to_Latin1, s, n ) );
}

unsigned char *n_convert_latin1_to_865 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_Latin1_to_865, s, n ) );
}

unsigned char *n_convert_865_to_437 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_865_to_437, s, n ) );
}

unsigned char *n_convert_437_to_865 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_437_to_865, s, n ) );
}

unsigned char *n_convert_latin1_to_437 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_Latin1_to_437, s, n ) );
}

unsigned char *n_convert_437_to_latin1 (s, n)
    unsigned char	*s;
    short		*n; 
{
	return( nconverter( cvt_437_to_Latin1, s, n ) );
}


/*===============================================================
 fn_convert_437_to_latin1() -
	Convert's it's C-String format argument.

	define function convert_437_to_latin1
	  module_name 'CONVERT'
	  entry_point 'c_convert_437_to_latin1'
	  cstring [256] sub_type 101 by reference,
	  cstring [256] sub_type 153 by reference return_value;

================================================================= */


static unsigned char	*converter( table, string )
unsigned char	*table;
unsigned char	*string;
{
unsigned char	*buf;
int		len;

len = sizeof( buffer )-1;

for (buf = buffer; *string && len; string++, buf++, len--) {
	*buf = table[ *string ];
}
*buf = 0;
return( buffer );
}


unsigned char *c_convert_865_to_latin1 (s)
    unsigned char	*s; 
{
	return( converter( cvt_865_to_Latin1, s ) );
}

unsigned char *c_convert_latin1_to_865 (s)
    unsigned char	*s; 
{
	return( converter( cvt_Latin1_to_865, s ) );
}

unsigned char *c_convert_865_to_437 (s)
    unsigned char	*s; 
{
	return( converter( cvt_865_to_437, s ) );
}

unsigned char *c_convert_437_to_865 (s)
    unsigned char	*s; 
{
	return( converter( cvt_437_to_865, s ) );
}

unsigned char *c_convert_latin1_to_437 (s)
    unsigned char	*s; 
{
	return( converter( cvt_Latin1_to_437, s ) );
}

unsigned char *c_convert_437_to_latin1 (s)
    unsigned char	*s; 
{
	return( converter( cvt_437_to_Latin1, s ) );
}
