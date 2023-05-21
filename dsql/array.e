/*
 *	PROGRAM:	Interbase layered support library
 *	MODULE:		array.e
 *	DESCRIPTION:	Dynamic array support
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
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/gds.h"
#include "../dsql/array_proto.h"
#include "../jrd/gds_proto.h"

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

DATABASE
    DB = STATIC "yachts.lnk";

#define ARRAY_DESC_COLUMN_MAJOR		1		/* Set for FORTRAN */

typedef struct gen {
    SCHAR	*gen_sdl;
    SCHAR	**gen_sdl_ptr;
    SCHAR	*gen_end;
    STATUS	*gen_status;
    SSHORT	gen_internal;
} *GEN;

static void	adjust_length (ISC_ARRAY_DESC *);
static STATUS	copy_status (STATUS *, STATUS *);
static STATUS	error (STATUS *, SSHORT, ...);
static STATUS	gen_sdl (STATUS *, ISC_ARRAY_DESC *, SSHORT *, SCHAR **, SSHORT *, BOOLEAN);
static void	get_name (SCHAR *, SCHAR *);
static STATUS	lookup_desc (STATUS *, void **, void **, SCHAR *, SCHAR *, ISC_ARRAY_DESC *, SCHAR *);
static STATUS	stuff (GEN, SSHORT, ...);
static STATUS	stuff_literal (GEN, SLONG);
static STATUS	stuff_string (GEN, SCHAR, SCHAR *);

/* STUFF_SDL used in place of STUFF to avoid confusion with BLR STUFF
   macro defined in dsql.h */

#define STUFF_SDL(byte)	 if (stuff (gen, 1, byte)) return status [1]
#define STUFF_SDL_WORD(word) if (stuff (gen, 2, word, word >> 8)) return status [1]
#define STUFF_SDL_LONG(word) if (stuff (gen, 4, word, word >> 8, word >> 16, word >> 24)) return status [1]

STATUS API_ROUTINE isc_array_gen_sdl (
    STATUS		*status,
    ISC_ARRAY_DESC	*desc,
    SSHORT		*sdl_buffer_length,
    SCHAR		*sdl_buffer,
    SSHORT		*sdl_length)
{
/**************************************
 *
 *	i s c _ a r r a y _ g e n _ s d l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return gen_sdl (status, desc, sdl_buffer_length, &sdl_buffer, sdl_length, FALSE);
}

STATUS API_ROUTINE isc_array_get_slice (   
    STATUS		*status,
    void		**db_handle,
    void		**trans_handle,
    GDS_QUAD		*array_id,
    ISC_ARRAY_DESC	*desc,
    void		*array,
    SLONG		*slice_length)
{
/**************************************
 *
 *	i s c _ a r r a y _ g e t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*sdl, sdl_buffer [512];
SSHORT	sdl_length;

sdl_length = sizeof (sdl_buffer);
sdl = sdl_buffer;

if (gen_sdl (status, desc, &sdl_length, &sdl, &sdl_length, TRUE))
    return status [1];

isc_get_slice (status, db_handle, trans_handle, array_id, 
		sdl_length, sdl, 0, NULL_PTR, 
		*slice_length, array, slice_length);

if (sdl != sdl_buffer)
    gds__free ((SLONG*) sdl);

return status [1];
}

STATUS API_ROUTINE isc_array_lookup_bounds (
    STATUS		*status,
    void		**db_handle,
    void		**trans_handle,
    SCHAR		*relation_name,
    SCHAR		*field_name,
    ISC_ARRAY_DESC	*desc)
{
/**************************************
 *
 *	i s c _ a r r a y _ l o o k u p _ b o u n d s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
ISC_ARRAY_BOUND	*tail;
SCHAR		global [32];

if (lookup_desc (status, db_handle, trans_handle, 
		    field_name, relation_name, desc, global))
    return status [1];

tail = desc->array_desc_bounds;

FOR X IN RDB$FIELD_DIMENSIONS WITH X.RDB$FIELD_NAME EQ global SORTED BY X.RDB$DIMENSION
    tail->array_bound_lower = X.RDB$LOWER_BOUND;
    tail->array_bound_upper = X.RDB$UPPER_BOUND;
    ++tail;
END_FOR
    ON_ERROR
	return copy_status (gds__status, status);
    END_ERROR;

return status [1];
}

STATUS API_ROUTINE isc_array_lookup_desc (
    STATUS		*status,
    void		**db_handle,
    void		**trans_handle,
    SCHAR		*relation_name,
    SCHAR		*field_name,
    ISC_ARRAY_DESC	*desc)
{
/**************************************
 *
 *	i s c _ a r r a y _ l o o k u p _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return lookup_desc (status, db_handle, trans_handle, 
		    field_name, relation_name, desc, NULL_PTR);
}

STATUS API_ROUTINE isc_array_put_slice (
    STATUS		*status,
    void		**db_handle,
    void		**trans_handle,
    GDS_QUAD		*array_id,
    ISC_ARRAY_DESC	*desc,
    void		*array,
    SLONG		*slice_length)
{
/**************************************
 *
 *	i s c _ a r r a y _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*sdl, sdl_buffer [512];
SSHORT	sdl_length;

sdl_length = sizeof (sdl_buffer);
sdl = sdl_buffer;

if (gen_sdl (status, desc, &sdl_length, &sdl, &sdl_length, TRUE))
    return status [1];

isc_put_slice (status, db_handle, trans_handle, array_id, 
		sdl_length, sdl, 0, NULL_PTR, 
		*slice_length, array);

if (sdl != sdl_buffer)
    gds__free ((SLONG*) sdl);

return status [1];
}

STATUS API_ROUTINE isc_array_set_desc (
    STATUS		*status,
    SCHAR		*relation_name,
    SCHAR		*field_name,
    SSHORT		*sql_dtype,
    SSHORT		*sql_length,
    SSHORT		*dimensions,
    ISC_ARRAY_DESC	*desc)
{
/**************************************
 *
 *	i s c _ a r r a y _ s e t _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SSHORT	dtype;

get_name (field_name, desc->array_desc_field_name);
get_name (relation_name, desc->array_desc_relation_name);
desc->array_desc_flags = 0;
desc->array_desc_dimensions = *dimensions;
desc->array_desc_length = *sql_length;
desc->array_desc_scale = 0;

dtype = *sql_dtype & ~1;

if (dtype == SQL_VARYING)
    desc->array_desc_dtype = blr_varying;
else if (dtype == SQL_TEXT)
    desc->array_desc_dtype = blr_text;
else if (dtype == SQL_DOUBLE)
    desc->array_desc_dtype = blr_double;
else if (dtype == SQL_FLOAT)
    desc->array_desc_dtype = blr_float;
else if (dtype == SQL_D_FLOAT)
    desc->array_desc_dtype = blr_d_float;
else if (dtype == SQL_TIMESTAMP)
    desc->array_desc_dtype = blr_timestamp;
else if (dtype == SQL_TYPE_DATE)
    desc->array_desc_dtype = blr_sql_date;
else if (dtype == SQL_TYPE_TIME)
    desc->array_desc_dtype = blr_sql_time;
else if (dtype == SQL_LONG)
    desc->array_desc_dtype = blr_long;
else if (dtype == SQL_SHORT)
    desc->array_desc_dtype = blr_short;
else if (dtype == SQL_INT64)
    desc->array_desc_dtype = blr_int64;
else if (dtype == SQL_QUAD)
    desc->array_desc_dtype = blr_quad;
else
    return error (status, 7, (STATUS) gds__sqlerr,
		(STATUS) gds_arg_number, (STATUS) -804,
		(STATUS) gds_arg_gds, (STATUS) gds__random,
		(STATUS) gds_arg_string, (STATUS) "data type not understood");

return error (status, 1, (STATUS) SUCCESS);
}

static void adjust_length (
    ISC_ARRAY_DESC	*desc)
{
/**************************************
 *
 *	a d j u s t _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Make architectural adjustment to fixed datatypes.
 *
 **************************************/
}

static STATUS copy_status (
    STATUS	*from,
    STATUS	*to)
{
/**************************************
 *
 *	c o p y _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Copy a status vector.
 *
 **************************************/
STATUS	status, *end;

status = from [1];

for (end = from + ISC_STATUS_LENGTH; from < end;)
    *to++ = *from++;

return status;
}

static STATUS error (
    STATUS	*status,
    SSHORT	count,
    ...)          
{
/**************************************
 *
 *	e r r o r	
 *
 **************************************
 *
 * Functional description
 *	Stuff a status vector.
 *
 **************************************/
STATUS	*stat;
va_list	ptr;

VA_START (ptr, count);
stat = status;
*stat++ = gds_arg_gds;

for (; count; --count)
    *stat++ = (STATUS) va_arg (ptr, STATUS);

*stat = gds_arg_end;

return status [1];
}

static STATUS gen_sdl (
    STATUS		*status,
    ISC_ARRAY_DESC	*desc,
    SSHORT		*sdl_buffer_length,
    SCHAR		**sdl_buffer,
    SSHORT		*sdl_length,
    BOOLEAN		internal_flag)
{
/**************************************
 *
 *	g e n _ s d l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SSHORT		n, from, to, increment, dimensions;
ISC_ARRAY_BOUND	*tail;
struct gen	*gen, gen_block;

dimensions = desc->array_desc_dimensions;

if (dimensions > 16)
    return error (status, 5, (STATUS) gds__invalid_dimension,
	(STATUS) gds_arg_number, (STATUS) dimensions,
	(STATUS) gds_arg_number, (STATUS) 16);

gen  = &gen_block;
gen->gen_sdl = *sdl_buffer;
gen->gen_sdl_ptr = sdl_buffer;
gen->gen_end = *sdl_buffer + *sdl_buffer_length;
gen->gen_status = status;
gen->gen_internal = internal_flag ? 0 : -1;

if (stuff (gen, 4, gds__sdl_version1, gds__sdl_struct, 1, desc->array_desc_dtype))
    return status [1];

switch (desc->array_desc_dtype)
    {
    case blr_short:
    case blr_long:
    case blr_int64:
    case blr_quad:
	STUFF_SDL (desc->array_desc_scale);
	break;
    
    case blr_text:
    case blr_cstring:
    case blr_varying:
	STUFF_SDL_WORD (desc->array_desc_length);
	break;
    default:
        break;
    }

if (stuff_string (gen, gds__sdl_relation, desc->array_desc_relation_name))
    return status [1];

if (stuff_string (gen, gds__sdl_field, desc->array_desc_field_name))
    return status [1];

if (desc->array_desc_flags & ARRAY_DESC_COLUMN_MAJOR)
    {
    from = dimensions - 1;
    to = -1;
    increment = -1;
    }
else
    {
    from = 0;
    to = dimensions;
    increment = 1;
    }

for (n = from; n != to; n += increment)
    {
    tail = desc->array_desc_bounds + n;
    if (tail->array_bound_lower == 1)
	{
	if (stuff (gen, 2, gds__sdl_do1, n))
	    return status [1];
	}
    else
	{
	if (stuff (gen, 2, gds__sdl_do2, n))
	    return status [1];
	if (stuff_literal (gen, (SLONG) tail->array_bound_lower))
	    return status [1];
	}
    if (stuff_literal (gen, (SLONG) tail->array_bound_upper))
	return status [1];
    }

if (stuff (gen, 5, gds__sdl_element, 1, gds__sdl_scalar, 0, dimensions))
    return status [1];

for (n = 0; n < dimensions; n++)
    if (stuff (gen, 2, gds__sdl_variable, n))
	return status [1];

STUFF_SDL (gds__sdl_eoc);
*sdl_length = gen->gen_sdl - *gen->gen_sdl_ptr;

return error (status, 1, (STATUS) SUCCESS);
}

static void get_name (
    SCHAR	*from,
    SCHAR	*to)
{
/**************************************
 *
 *	g e t _ n a m e 
 *
 **************************************
 *
 * Functional description
 *	Copy null or blank terminated name.
 *
 **************************************/

while (*from && *from != ' ')
    {
    *to++ = UPPER7( *from);
    from++;
    }

*to = 0;
}

static STATUS lookup_desc (
    STATUS		*status,
    void		**db_handle,
    void		**trans_handle,
    SCHAR		*field_name,
    SCHAR		*relation_name,
    ISC_ARRAY_DESC	*desc,
    SCHAR		*global)
{
/**************************************
 *
 *	l o o k u p _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SSHORT	flag;

if (DB && DB != *db_handle)
    RELEASE_REQUESTS;

DB = *db_handle;
gds__trans = *trans_handle;
get_name (field_name, desc->array_desc_field_name);
get_name (relation_name, desc->array_desc_relation_name);
desc->array_desc_flags = 0;

flag = FALSE;

FOR X IN RDB$RELATION_FIELDS CROSS Y IN RDB$FIELDS 
	WITH X.RDB$FIELD_SOURCE EQ Y.RDB$FIELD_NAME AND
	     X.RDB$RELATION_NAME EQ desc->array_desc_relation_name AND
	     X.RDB$FIELD_NAME EQ desc->array_desc_field_name
    flag = TRUE;
    desc->array_desc_dtype = Y.RDB$FIELD_TYPE;
    desc->array_desc_scale = Y.RDB$FIELD_SCALE;
    desc->array_desc_length = Y.RDB$FIELD_LENGTH;
    adjust_length (desc);
    desc->array_desc_dimensions = Y.RDB$DIMENSIONS;
    if (global)
	get_name (Y.RDB$FIELD_NAME, global);
END_FOR
    ON_ERROR
	return copy_status (gds__status, status);
    END_ERROR;

if (!flag)
    return error (status, 5, (STATUS) gds__fldnotdef, 
		(STATUS) gds_arg_string, (STATUS) desc->array_desc_field_name,
		(STATUS) gds_arg_string, (STATUS) desc->array_desc_relation_name);

return error (status, 1, (STATUS) SUCCESS);
}

static STATUS stuff (
    GEN		gen,
    SSHORT	count,
    ...)       
{
/**************************************
 *
 *	s t u f f
 *
 **************************************
 *
 * Functional description
 *	Stuff a SDL byte.
 *
 **************************************/
SCHAR	c, *new_sdl;
va_list	ptr;
SSHORT	new_len, current_len;

if (gen->gen_sdl + count >= gen->gen_end)
    {
    if (gen->gen_internal < 0)
	return error (gen->gen_status, 3, (STATUS) gds__misc_interpreted,
	    (STATUS) gds_arg_string, (STATUS) "SDL buffer overflow");

    /* The sdl buffer is too small.  Allocate a larger one. */

    new_len = gen->gen_end - *gen->gen_sdl_ptr + 512 + count;
    new_sdl = (SCHAR*) gds__alloc ((SLONG) new_len);
    if (!new_sdl)
	return error (gen->gen_status, 5, (STATUS) gds__misc_interpreted,
		(STATUS) gds_arg_string, (STATUS) "SDL buffer overflow",
		(STATUS) gds_arg_gds, (STATUS) gds__virmemexh);

    current_len = gen->gen_sdl - *gen->gen_sdl_ptr;
    memcpy (new_sdl, *gen->gen_sdl_ptr, current_len);
    if (gen->gen_internal++)
	gds__free (*gen->gen_sdl_ptr);
    gen->gen_sdl = new_sdl + current_len;
    *gen->gen_sdl_ptr = new_sdl;
    gen->gen_end = new_sdl + new_len;
    }

VA_START (ptr, count);

for (; count; --count)
    {
    c = va_arg (ptr, int);
    *(gen->gen_sdl)++ = c;
    }

return 0;
}

static STATUS stuff_literal (
    GEN		gen,
    SLONG	literal)
{
/**************************************
 *
 *	s t u f f _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	Stuff an SDL literal.
 *
 **************************************/
STATUS	*status;

status = gen->gen_status;

if (literal >= -128 && literal <= 127)
    return stuff (gen, 2, gds__sdl_tiny_integer, literal);

if (literal >= -32768 && literal <= 32767)
    return stuff (gen, 3, gds__sdl_short_integer, literal, literal >> 8);

STUFF_SDL (gds__sdl_long_integer);
STUFF_SDL_LONG (literal);

return SUCCESS;
}

static STATUS stuff_string (
    GEN		gen,
    SCHAR	sdl,
    SCHAR	*string)
{
/**************************************
 *
 *	s t u f f _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Stuff a "thing" then a counted string.
 *
 **************************************/
STATUS	*status;

status = gen->gen_status;

STUFF_SDL (sdl);
STUFF_SDL (strlen (string));

while (*string)
    STUFF_SDL (*string++);

return SUCCESS;
}
