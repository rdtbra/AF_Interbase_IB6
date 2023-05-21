/*
 *	PROGRAM:	InterBase International support
 *	MODULE:		cv_narrow.c
 *	DESCRIPTION:	Codeset conversion for narrow character sets.
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


#include "../intl/ldcommon.h"


void	CV_convert_init (csptr, to_cs, from_cs, cvt_fn, datatable, datatable2)
CSCONVERT	csptr;
SSHORT		to_cs;
SSHORT		from_cs;
FPTR_SHORT	cvt_fn;
BYTE		*datatable;
BYTE		*datatable2;
{
csptr->csconvert_version = 40;
csptr->csconvert_name = (ASCII *) "DIRECT";
csptr->csconvert_from = from_cs;
csptr->csconvert_to   = to_cs;
csptr->csconvert_convert = cvt_fn;
csptr->csconvert_datatable = datatable;
csptr->csconvert_misc = datatable2;
}


USHORT	CV_unicode_to_nc (obj, dest_ptr, dest_len, src_ptr, src_len, err_code, err_position)
CSCONVERT	obj;
BYTE		*dest_ptr;
USHORT		dest_len;
BYTE		*src_ptr;
USHORT		src_len;
SSHORT		*err_code;
USHORT		*err_position;
{
BYTE	*start;
USHORT	src_start = src_len;
UCHAR	ch;
UNICODE	uni;

assert(	src_ptr  != NULL || dest_ptr == NULL );
assert( err_code != NULL );
assert( err_position != NULL );
assert( obj != NULL );
assert( obj->csconvert_convert == CV_unicode_to_nc );
assert( obj->csconvert_datatable != NULL );
assert( obj->csconvert_misc != NULL );

*err_code = 0;

/* See if we're only after a length estimate */
if (dest_ptr == NULL)
	return( (USHORT) (src_len + 1) / 2 );

start = dest_ptr;
while ((src_len > 1) && dest_len) 
	{
	uni = *((UNICODE *) src_ptr);
	ch = obj->csconvert_datatable[ 
		((USHORT *)obj->csconvert_misc)[ (USHORT) uni / 256 ] 
				+ (uni % 256) ];
	if ((ch == CS_CANT_MAP) && !(uni == CS_CANT_MAP))
		{
		*err_code = CS_CONVERT_ERROR;
		break;
		}
	*dest_ptr++ = ch;
	src_ptr += 2;
	src_len -= 2;
	dest_len -= 1;
	};
if (src_len && !*err_code) 
	{
	if (src_len == 1)
		*err_code = CS_BAD_INPUT;
	else
		*err_code = CS_TRUNCATION_ERROR;
	};
*err_position = src_start - src_len;
return( dest_ptr - start ); 
}

USHORT	CV_wc_to_wc( obj, dest_ptr, dest_len, src_ptr, src_len, err_code, err_position )
CSCONVERT	obj;
USHORT		*dest_ptr;
USHORT		dest_len;
USHORT		*src_ptr;
USHORT		src_len;
SSHORT		*err_code;
USHORT		*err_position;
{
USHORT	*start;
USHORT	ch;
UNICODE	uni;
USHORT	src_start = src_len;

assert(	src_ptr  != NULL || dest_ptr == NULL );
assert( err_code != NULL );
assert( err_position != NULL );
assert( obj != NULL );
assert( obj->csconvert_convert == CV_wc_to_wc );
assert( obj->csconvert_datatable != NULL );
assert( obj->csconvert_misc != NULL );

*err_code = 0;

/* See if we're only after a length estimate */
if (dest_ptr == NULL)
	return( src_len );

start = dest_ptr;
while ((src_len > 1) && (dest_len > 1)) 
	{
	uni = *((UNICODE *) src_ptr);
	ch = ((USHORT *)obj->csconvert_datatable)[ 
		((USHORT *)obj->csconvert_misc)[ (USHORT) uni / 256 ] 
				+ (uni % 256) ];
	if ((ch == CS_CANT_MAP) && !(uni == CS_CANT_MAP))
		{
		*err_code = CS_CONVERT_ERROR;
		break;
		}
	*dest_ptr++ = ch;
	src_ptr++;
	src_len -= 2;
	dest_len -= 2;
	};
if (src_len && !*err_code) 
	{
	if (src_len == 1)
		*err_code = CS_BAD_INPUT;
	else
		*err_code = CS_TRUNCATION_ERROR;
	};
*err_position = src_start - src_len;
return( (dest_ptr - start)*sizeof( *dest_ptr ) ); 
}

USHORT	CV_nc_to_unicode( obj, dest_ptr, dest_len, src_ptr, src_len, err_code, err_position )
CSCONVERT	obj;
BYTE		*dest_ptr;
USHORT		dest_len;
BYTE		*src_ptr;
USHORT		src_len;
SSHORT		*err_code;
USHORT		*err_position;
{
BYTE	*start;
UNICODE	ch;
USHORT	src_start = src_len;

assert(	src_ptr  != NULL || dest_ptr == NULL );
assert( err_code != NULL );
assert( err_position != NULL );
assert( obj != NULL );
assert( obj->csconvert_convert == CV_nc_to_unicode );
assert( obj->csconvert_datatable != NULL );
assert( sizeof( UNICODE ) == 2 );

*err_code = 0;

/* See if we're only after a length estimate */
if (dest_ptr == NULL)
	return( src_len * 2 );

start = dest_ptr;
while (src_len && (dest_len > 1)) 
	{
	ch = ((UNICODE *)(obj->csconvert_datatable))[ *src_ptr ];
	/* No need to check for CS_CONVERT_ERROR, all charsets
	 * must convert to unicode.
	 */

	*((UNICODE *)dest_ptr) = ch;
	src_ptr++;
	src_len--;
	dest_len -= sizeof( UNICODE );
	dest_ptr += sizeof( UNICODE );
	};
if (src_len && !*err_code) 
	{
	*err_code = CS_TRUNCATION_ERROR;
	};
*err_position = src_start - src_len;
return( dest_ptr - start ); 
}

USHORT	CV_wc_copy( obj, dest_ptr, dest_len, src_ptr, src_len, err_code, err_position )
CSCONVERT	obj;
BYTE		*dest_ptr;
USHORT		dest_len;	/* byte count */
BYTE		*src_ptr;
USHORT		src_len;	/* byte count */
SSHORT		*err_code;
USHORT		*err_position;
{
BYTE	*start;
UNICODE	ch;
USHORT	src_start = src_len;

assert(	src_ptr  != NULL || dest_ptr == NULL );
assert( err_code != NULL );
assert( err_position != NULL );
assert( obj != NULL );
assert( obj->csconvert_convert == CV_wc_copy );

*err_code = 0;

/* See if we're only after a length estimate */
if (dest_ptr == NULL)
	return( src_len );

start = dest_ptr;
while ((src_len > 1) && (dest_len > 1))
	{
	*dest_ptr++ = *src_ptr++;	/* first byte of unicode */
	*dest_ptr++ = *src_ptr++;	/* 2nd   byte of unicode */
	src_len -= 2;
	dest_len -= 2;
	};
if (src_len && !*err_code)
	{
	if (src_len == 1)
		*err_code = CS_BAD_INPUT;
	else
		*err_code = CS_TRUNCATION_ERROR;
	};
*err_position = src_start - src_len;
return( dest_ptr - start ); 
}

static USHORT	eight_bit_convert( obj, dest_ptr, dest_len, src_ptr, src_len, err_code, err_position )
CSCONVERT	obj;
BYTE		*dest_ptr;
USHORT		dest_len;
BYTE		*src_ptr;
USHORT		src_len;
SSHORT		*err_code;
USHORT		*err_position;
{
BYTE	*start;
UCHAR	ch;
USHORT	src_start = src_len;

assert(	src_ptr  != NULL || dest_ptr == NULL );
assert( err_code != NULL );
assert( err_position != NULL );
assert( obj != NULL );
assert( obj->csconvert_convert == eight_bit_convert );
assert( obj->csconvert_datatable != NULL );

*err_code = 0;

/* See if we're only after a length estimate */
if (dest_ptr == NULL)
	return( src_len );

start = dest_ptr;
while (src_len && dest_len)
	{
	ch = obj->csconvert_datatable[ *src_ptr ];
	if ((ch == CS_CANT_MAP) && (*src_ptr != CS_CANT_MAP))
		{
		*err_code = CS_CONVERT_ERROR;
		break;
		};
	*dest_ptr++ = ch;
	src_ptr++;
	src_len--;
	dest_len--;
	};
if (src_len && !*err_code)
	{
	*err_code = CS_TRUNCATION_ERROR;
	};
*err_position = src_start - src_len;
return( dest_ptr - start ); 
}


CONVERT_ENTRY( CS_LATIN1, CS_DOS_865, CV_dos_865_x_latin1 )
{
#include "../intl/tx865_lat1.h"
if (dest_cs == CS_LATIN1)
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_865_to_Latin1, NULL );
else
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_Latin1_to_865, NULL );
CONVERT_RETURN;
}


CONVERT_ENTRY( CS_LATIN1, CS_DOS_437, CV_dos_437_x_dos_865 )
{
#include "../intl/tx437_865.h"
if (dest_cs == CS_DOS_865)
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_437_to_865, NULL );
else
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_865_to_437, NULL );

CONVERT_RETURN;
}


CONVERT_ENTRY( CS_LATIN1, CS_DOS_437, CV_dos_437_x_latin1 )
{
#include "../intl/tx437_lat1.h"
if (dest_cs == CS_LATIN1)
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_437_to_Latin1, NULL );
else
	CV_convert_init( csptr, dest_cs, source_cs, eight_bit_convert, 
				cvt_Latin1_to_437, NULL );

CONVERT_RETURN;
}
