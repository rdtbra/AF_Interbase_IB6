/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		utld.c
 *	DESCRIPTION:	Utility routines for DSQL
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
#include "../dsql/dsql.h"
#include "../dsql/sqlda.h"
#include "../jrd/blr.h"
#include "../jrd/codes.h"
#include "../jrd/inf.h"
#include "../jrd/align.h"
#include "../dsql/utld_proto.h"
#include "../jrd/gds_proto.h"

static void	cleanup (void *);
static STATUS	error_dsql_804 (STATUS *, STATUS);
static SLONG	get_numeric_info (SCHAR **);
static SLONG	get_string_info (SCHAR **, SCHAR *, int);
#ifdef DEV_BUILD
static void	print_xsqlda (XSQLDA *);
#endif
static void	sqlvar_to_xsqlvar (SQLVAR *, XSQLVAR *);
static void	xsqlvar_to_sqlvar (XSQLVAR *, SQLVAR *);

#ifdef NETWARE_386
#define	PRINTF	ConsolePrintf
#endif

#ifndef PRINTF
#define PRINTF	ib_printf
#endif

#define CH_STUFF(p,value)	{if ((SCHAR) *(p) == (SCHAR) (value)) (p)++; else\
				{*(p)++ = (value); same_flag = FALSE;}}
#define CH_STUFF_WORD(p,value)	{CH_STUFF (p, (value) & 255);\
				CH_STUFF (p, (value) >> 8);}

/* these statics define a round-robin data area for storing
   textual error messages returned to the user */

static TEXT	*DSQL_failures, *DSQL_failures_ptr;

#define DSQL_FAILURE_SPACE	2048

STATUS DLL_EXPORT UTLD_parse_sql_info (
    STATUS	*status,
    USHORT	dialect,
    SCHAR	*info,
    XSQLDA	*xsqlda,
    USHORT	*return_index)
{
/**************************************
 *
 *	U T L D _ p a r s e _ s q l _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Fill in an SQLDA using data returned
 *	by a call to isc_dsql_sql_info.
 *
 **************************************/
SCHAR	item;
SSHORT	n;
XSQLVAR	*xvar, xsqlvar;
SQLDA	*sqlda;
SQLVAR	*var;
USHORT	index, last_index;

if (return_index)
    *return_index = last_index = 0;

if (!xsqlda)
    return 0;

/* The first byte of the returned buffer is assumed to be either a
   gds__info_sql_select or gds__info_sql_bind item.  The second byte
   is assumed to be gds__info_sql_describe_vars. */

info += 2;

n = get_numeric_info (&info);
if (dialect >= DIALECT_xsqlda)
    {
    if (xsqlda->version != SQLDA_VERSION1)
        return error_dsql_804 (status, gds__dsql_sqlda_err);
    xsqlda->sqld = n;

    /* If necessary, inform the application that more sqlda items are needed */
    if (xsqlda->sqld > xsqlda->sqln)
	return 0;
    }
else
    {
    sqlda = (SQLDA*) xsqlda;
    sqlda->sqld = n;

    /* If necessary, inform the application that more sqlda items are needed */
    if (sqlda->sqld > sqlda->sqln)
	return 0;

    xsqlda = NULL;
    xvar = &xsqlvar;
    }

/* Loop over the variables being described. */

while (*info != gds__info_end)
    {
    while ((item = *info++) != gds__info_sql_describe_end)
	switch (item)
	    {
	    case gds__info_sql_sqlda_seq:
		index = get_numeric_info (&info);
		if (xsqlda)
		    xvar = xsqlda->sqlvar + index - 1;
		else
		    {
		    var = sqlda->sqlvar + index - 1;
		    memset (xvar, 0, sizeof (XSQLVAR));
		    }
		break;

	    case gds__info_sql_type:
		xvar->sqltype = get_numeric_info (&info);
		break;

	    case gds__info_sql_sub_type:
		xvar->sqlsubtype = get_numeric_info (&info);
		break;

	    case gds__info_sql_scale:
		xvar->sqlscale = get_numeric_info (&info);
		break;

	    case gds__info_sql_length:
		xvar->sqllen = get_numeric_info (&info);
		break;

	    case gds__info_sql_field:
		xvar->sqlname_length =
		    get_string_info (&info, xvar->sqlname, sizeof (xvar->sqlname));
		break;

	    case gds__info_sql_relation:
		xvar->relname_length =
		    get_string_info (&info, xvar->relname, sizeof (xvar->relname));
		break;

	    case gds__info_sql_owner:
		xvar->ownname_length =
		    get_string_info (&info, xvar->ownname, sizeof (xvar->ownname));
		break;

	    case gds__info_sql_alias:
		xvar->aliasname_length =
		    get_string_info (&info, xvar->aliasname, sizeof (xvar->aliasname));
		break;

	    case gds__info_truncated:
		if (return_index)
		    *return_index = last_index;

	    default:
		return error_dsql_804 (status, gds__dsql_sqlda_err);
	    }

    if (!xsqlda)
	xsqlvar_to_sqlvar (xvar, var);

    if (index > last_index)
	last_index = index;
    }

if (*info != gds__info_end)
    return error_dsql_804 (status, gds__dsql_sqlda_err);

return SUCCESS;
}

STATUS DLL_EXPORT UTLD_parse_sqlda (
    STATUS	*status,
    DASUP	dasup,
    USHORT	*blr_length,
    USHORT	*msg_type,
    USHORT	*msg_length,
    USHORT	dialect,
    XSQLDA	*xsqlda,
    USHORT	clause)
{
/*****************************************
 *
 *	U T L D _ p a r s e _ s q l d a
 *
 *****************************************
 *
 * Functional Description
 *	This routine creates a blr message that describes
 *	a SQLDA as well as moving data from the SQLDA
 *	into a message buffer or from the message buffer
 *	into the SQLDA.
 *
 *****************************************/
USHORT	i, n, blr_len, par_count, dtype, same_flag, msg_len, len, align, offset,
	null_offset;
SSHORT	*null_ind;
XSQLVAR	*xvar, xsqlvar;
SQLDA	*sqlda;
SQLVAR	*var;
BLOB_PTR *p;		/* one huge pointer per line for LIBS */
BLOB_PTR *msg_buf;	/* one huge pointer per line for LIBS */

if (!xsqlda)
    n = 0;
else
    if (dialect >= DIALECT_xsqlda)
        {
        if (xsqlda->version != SQLDA_VERSION1)
            return error_dsql_804 (status, gds__dsql_sqlda_err);
	n = xsqlda->sqld;
        }
    else
	{
	sqlda = (SQLDA*) xsqlda;
	n = sqlda->sqld;
	xsqlda = NULL;
	xvar = &xsqlvar;
	}

if (!n)
    {
    /* If there isn't an SQLDA, don't bother with anything else. */

    if (blr_length)
	*blr_length = dasup->dasup_clauses [clause].dasup_blr_length = 0;
    if (msg_length)
	*msg_length = 0;
    if (msg_type)
	*msg_type = 0;
    return 0;
    }

if (msg_length)
    {
    /* This is a call from execute or open, or the first call from fetch.
       Determine the size of the blr, allocate a blr buffer, create the
       blr, and finally allocate a message buffer. */

    /* The message buffer we are describing could be for a message to
       either IB 4.0 or IB 3.3 - thus we need to describe the buffer
       with the right set of blr.
       As the BLR is only used to describe space allocation and alignment,
       we can safely use blr_text for both 4.0 and 3.3. */

    /* Make a quick pass through the SQLDA to determine the amount of
       blr that will be generated. */

    blr_len = 8;
    par_count = 0;
    if (xsqlda)
	xvar = xsqlda->sqlvar - 1;
    else
	var = sqlda->sqlvar - 1;
    for (i = 0; i < n; i++)
	{
	if (xsqlda)
	    xvar++;
	else
	    {
	    var++;
	    sqlvar_to_xsqlvar (var, xvar);
	    }
	dtype = xvar->sqltype & ~1;
	if (dtype == SQL_VARYING || dtype == SQL_TEXT)
	    blr_len += 3;
	else if (dtype == SQL_SHORT || dtype == SQL_LONG ||
		 dtype == SQL_INT64 ||
	    dtype == SQL_QUAD || dtype == SQL_BLOB || dtype == SQL_ARRAY)
	    blr_len += 2;
	else
	    blr_len++;
	blr_len += 2;
	par_count += 2;
	}

    /* Make sure the blr buffer is large enough.  If it isn't, allocate
       a new one. */

    if (blr_len > dasup->dasup_clauses [clause].dasup_blr_buf_len)
	{
	if (dasup->dasup_clauses [clause].dasup_blr)
	    gds__free (dasup->dasup_clauses [clause].dasup_blr);
	dasup->dasup_clauses [clause].dasup_blr = gds__alloc ((SLONG) blr_len);
	/* FREE: unknown */
	if (!dasup->dasup_clauses [clause].dasup_blr)	/* NOMEM: */
	    return error_dsql_804 (status, gds__virmemexh);
	memset (dasup->dasup_clauses [clause].dasup_blr, 0, blr_len);
	dasup->dasup_clauses [clause].dasup_blr_buf_len = blr_len;
	dasup->dasup_clauses [clause].dasup_blr_length = 0;
	}

    same_flag = (blr_len == dasup->dasup_clauses [clause].dasup_blr_length);
    dasup->dasup_clauses [clause].dasup_blr_length = blr_len;

    /* Generate the blr for the message and at the same time, determine
       the size of the message buffer.  Allow for a null indicator with
       each variable in the SQLDA. */

    p = dasup->dasup_clauses [clause].dasup_blr;
    /** The define SQL_DIALECT_V5 is not available here, Hence using
	constant 1.
    **/
    if (dialect > 1)
        CH_STUFF (p, blr_version5)
    else
        CH_STUFF (p, blr_version4)
    CH_STUFF (p, blr_begin);
    CH_STUFF (p, blr_message);
    CH_STUFF (p, 0);
    CH_STUFF_WORD (p, par_count);
    msg_len = 0;
    if (xsqlda)
	xvar = xsqlda->sqlvar - 1;
    else
	var = sqlda->sqlvar - 1;
    for (i = 0; i < n; i++)
	{
	if (xsqlda)
	    xvar++;
	else
	    {
	    var++;
	    sqlvar_to_xsqlvar (var, xvar);
	    }
	dtype = xvar->sqltype & ~1;
	len = xvar->sqllen;
	if (dtype == SQL_VARYING)
	    {
	    CH_STUFF (p, blr_varying);
	    CH_STUFF_WORD (p, len);
	    dtype = dtype_varying;
	    len += sizeof (USHORT);
	    }
	else if (dtype == SQL_TEXT)
	    {
	    CH_STUFF (p, blr_text);
	    CH_STUFF_WORD (p, len);
	    dtype = dtype_text;
	    }
	else if (dtype == SQL_DOUBLE)
	    {
	    CH_STUFF (p, blr_double);
	    dtype = dtype_double;
	    }
	else if (dtype == SQL_FLOAT)
	    {
	    CH_STUFF (p, blr_float);
	    dtype = dtype_real;
	    }
	else if (dtype == SQL_D_FLOAT)
	    {
	    CH_STUFF (p, blr_d_float);
	    dtype = dtype_d_float;
	    }
	else if (dtype == SQL_TYPE_DATE)
	    {
	    CH_STUFF (p, blr_sql_date);
	    dtype = dtype_sql_date;
	    }
	else if (dtype == SQL_TYPE_TIME)
	    {
	    CH_STUFF (p, blr_sql_time);
	    dtype = dtype_sql_time;
	    }
	else if (dtype == SQL_TIMESTAMP)
	    {
	    CH_STUFF (p, blr_timestamp);
	    dtype = dtype_timestamp;
	    }
	else if (dtype == SQL_BLOB)
	    {
	    CH_STUFF (p, blr_quad);
	    CH_STUFF (p, 0);
	    dtype = dtype_blob;
	    }
	else if (dtype == SQL_ARRAY)
	    {
	    CH_STUFF (p, blr_quad);
	    CH_STUFF (p, 0);
	    dtype = dtype_array;
	    }
	else if (dtype == SQL_LONG)
	    {
	    CH_STUFF (p, blr_long);
	    CH_STUFF (p, xvar->sqlscale);
	    dtype = dtype_long;
	    }
	else if (dtype == SQL_SHORT)
	    {
	    CH_STUFF (p, blr_short);
	    CH_STUFF (p, xvar->sqlscale);
	    dtype = dtype_short;
	    }
	else if (dtype == SQL_INT64)
	    {
	    CH_STUFF (p, blr_int64);
	    CH_STUFF (p, xvar->sqlscale);
	    dtype = dtype_int64;
	    }
	else if (dtype == SQL_QUAD)
	    {
	    CH_STUFF (p, blr_quad);
	    CH_STUFF (p, xvar->sqlscale);
	    dtype = dtype_quad;
	    }
	else
	    return error_dsql_804 (status, gds__dsql_sqlda_value_err);

	CH_STUFF (p, blr_short);
	CH_STUFF (p, 0);

	align = type_alignments [dtype];
	if (align)
	    msg_len = ALIGN (msg_len, align);
	msg_len += len;
	align = type_alignments [dtype_short];
	if (align)
	    msg_len = ALIGN (msg_len, align);
	msg_len += sizeof (SSHORT);
	}

    CH_STUFF (p, blr_end);
    CH_STUFF (p, blr_eoc);

    /* Make sure the message buffer is large enough.  If it isn't, allocate
       a new one. */

    if (msg_len > dasup->dasup_clauses [clause].dasup_msg_buf_len)
	{
	if (dasup->dasup_clauses [clause].dasup_msg)
	    gds__free (dasup->dasup_clauses [clause].dasup_msg);
	dasup->dasup_clauses [clause].dasup_msg = gds__alloc ((SLONG) msg_len);
	/* FREE: unknown */
	if (!dasup->dasup_clauses [clause].dasup_msg) /* NOMEM: */
	    return error_dsql_804 (status, gds__virmemexh);
	memset (dasup->dasup_clauses [clause].dasup_msg, 0, msg_len);
	dasup->dasup_clauses [clause].dasup_msg_buf_len = msg_len;
	}

    /* Fill in the return values to the caller. */

    *blr_length = (same_flag) ? 0 : blr_len;
    *msg_length = msg_len;
    *msg_type = 0;

    /* If this is the first call from fetch, we're done. */

    if (clause == DASUP_CLAUSE_select)
	return 0;
    }

/* Move the data between the SQLDA and the message buffer. */

offset = 0;
msg_buf = dasup->dasup_clauses [clause].dasup_msg;
if (xsqlda)
    xvar = xsqlda->sqlvar - 1;
else
    var = sqlda->sqlvar - 1;
for (i = 0; i < n; i++)
    {
    if (xsqlda)
	xvar++;
    else
	{
	var++;
	sqlvar_to_xsqlvar (var, xvar);
	}
    dtype = xvar->sqltype & ~1;
    len = xvar->sqllen;
    if (dtype == SQL_VARYING)
	{
	dtype = dtype_varying;
	len += sizeof (USHORT);
	}
    else if (dtype == SQL_TEXT)
	dtype = dtype_text;
    else if (dtype == SQL_DOUBLE)
	dtype = dtype_double;
    else if (dtype == SQL_FLOAT)
	dtype = dtype_real;
    else if (dtype == SQL_D_FLOAT)
	dtype = dtype_d_float;
    else if (dtype == SQL_TYPE_DATE)
	dtype = dtype_sql_date;
    else if (dtype == SQL_TYPE_TIME)
	dtype = dtype_sql_time;
    else if (dtype == SQL_TIMESTAMP)
	dtype = dtype_timestamp;
    else if (dtype == SQL_BLOB)
	dtype = dtype_blob;
    else if (dtype == SQL_ARRAY)
	dtype = dtype_array;
    else if (dtype == SQL_LONG)
	dtype = dtype_long;
    else if (dtype == SQL_SHORT)
	dtype = dtype_short;
    else if (dtype == SQL_INT64)
	dtype = dtype_int64;
    else if (dtype == SQL_QUAD)
	dtype = dtype_quad;

    align = type_alignments [dtype];
    if (align)
	offset = ALIGN (offset, align);
    null_offset = offset + len;

    align = type_alignments [dtype_short];
    if (align)
	null_offset = ALIGN (null_offset, align);

    null_ind = (SSHORT*) (msg_buf + null_offset);
    if (clause == DASUP_CLAUSE_select)
	{
	/* Move data from the message into the SQLDA. */

	/* Make sure user has specified a data location */
	if (!xvar->sqldata)
	    return error_dsql_804 (status, gds__dsql_sqlda_value_err);

	memcpy (xvar->sqldata, msg_buf + offset, len);
	if (xvar->sqltype & 1)
	    {
	    /* Make sure user has specified a location for null indicator */
	    if (!xvar->sqlind)
		return error_dsql_804 (status, gds__dsql_sqlda_value_err);
	    *xvar->sqlind = *null_ind;
	    }
	}
    else
	{
	/* Move data from the SQLDA into the message.  If the
	   indicator variable identifies a null value, permit
	   the data value to be missing. */

	if (xvar->sqltype & 1)
	    {
	    /* Make sure user has specified a location for null indicator */
	    if (!xvar->sqlind)
		return error_dsql_804 (status, gds__dsql_sqlda_value_err);
	    *null_ind = *xvar->sqlind;
	    }
	else
	    *null_ind = 0;

	/* Make sure user has specified a data location (unless NULL) */
	if (!xvar->sqldata && !*null_ind)
	    return error_dsql_804 (status, gds__dsql_sqlda_value_err);

	/* Copy data - unless known to be NULL */
	if (!*null_ind)
	    memcpy (msg_buf + offset, xvar->sqldata, len);
	}

    offset = null_offset + sizeof (SSHORT);
    }

return 0;
}

void DLL_EXPORT UTLD_save_status_strings (
    STATUS	*vector)
{
/**************************************
 *
 *	U T L D _ s a v e _ s t a t u s _ s t r i n g s
 *
 **************************************
 *
 * Functional description
 *	Strings in status vectors may be stored in stack variables
 *	or memory pools that are transient.  To perserve the information,
 *	copy any included strings to a special buffer.
 *
 **************************************/
TEXT	*p;
STATUS	status;
USHORT	l;

/* allocate space for failure strings if it hasn't already been allocated */

if (!DSQL_failures)
    {
    DSQL_failures = (TEXT*) ALLOC_LIB_MEMORY ((SLONG) DSQL_FAILURE_SPACE);
    /* FREE: by exit handler cleanup() */
    if (!DSQL_failures)		/* NOMEM: don't try to copy the strings */
	return;
    DSQL_failures_ptr = DSQL_failures;
    gds__register_cleanup (cleanup, NULL_PTR);
#ifndef WINDOWS_ONLY
#ifdef DEBUG_GDS_ALLOC
    gds_alloc_flag_unfreed ((void *) DSQL_failures);
#endif
#endif
    }

while (*vector)
    {
    status = *vector;
    vector++;
    switch (status)
	{
	case gds_arg_cstring:
	    l = *vector++;

	case gds_arg_interpreted:
	case gds_arg_string:
	    p = (TEXT*) *vector;
	    if (status != gds_arg_cstring)
		l = strlen (p) + 1;

	    /* If there isn't any more room in the buffer,
	       start at the beginning again */

	    if (DSQL_failures_ptr + l > DSQL_failures + DSQL_FAILURE_SPACE)
		DSQL_failures_ptr = DSQL_failures;
 	    *vector++ = (STATUS) DSQL_failures_ptr;
	    if (l)
		do *DSQL_failures_ptr++ = *p++;
	        while (--l && (DSQL_failures_ptr < DSQL_failures + DSQL_FAILURE_SPACE));
	    if (l)
		*(DSQL_failures_ptr - 1) = '\0';
	    break;

	default:
	    ++vector;
	    break;
	}
    }
}

#ifdef  WINDOWS_ONLY
void    UTLD_wep( void)
{
/**************************************
 *
 *      U T L D _ w e p
 *
 **************************************
 *
 * Functional description
 *      Call cleanup for WEP.
 *
 **************************************/

cleanup( NULL);
}
#endif

static void cleanup (
    void	*arg)
{
/**************************************
 *
 *	c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Exit handler to cleanup dynamically allocated memory.
 *
 **************************************/

if (DSQL_failures)
    FREE_LIB_MEMORY (DSQL_failures);

gds__unregister_cleanup (cleanup, NULL_PTR);
DSQL_failures = NULL;
}

static STATUS error_dsql_804 (
    STATUS	*status,
    STATUS	err)
{
/**************************************
 *
 *	e r r o r _ d s q l _ 8 0 4
 *
 **************************************
 *
 * Functional description
 *	Move a DSQL -804 error message into a status vector.
 *
 **************************************/
STATUS	*p;

p = status;
*p++ = gds_arg_gds;
*p++ = gds__dsql_error;
*p++ = gds_arg_gds;
*p++ = gds__sqlerr;
*p++ = gds_arg_number;
*p++ = -804;
*p++ = gds_arg_gds;
*p++ = (err);
*p = gds_arg_end;

return status [1];
}

static SLONG get_numeric_info (
    SCHAR	**ptr)
{
/**************************************
 *
 *	g e t _ n u m e r i c _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Pick up a VAX format numeric info item
 *	with a 2 byte length.
 *
 **************************************/
int	item;
SSHORT	l;

l = gds__vax_integer (*ptr, 2);
*ptr += 2;
item = gds__vax_integer (*ptr, l);
*ptr += l;

return item;
}

static SLONG get_string_info (
    SCHAR	**ptr,
    SCHAR	*buffer,
    int		buffer_len)
{
/**************************************
 *
 *	g e t _ s t r i n g _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Pick up a string valued info item and return
 *	its length.  The buffer_len argument is assumed
 *	to include space for the terminating null.
 *
 **************************************/
SSHORT	l, len;
SCHAR	*p;

p = *ptr;
l = gds__vax_integer (p, 2);
*ptr += l + 2;
p += 2;

if (l >= buffer_len)
    l = buffer_len - 1;

if (len = l)
    do *buffer++ = *p++; while (--l);
*buffer = 0;

return len;
}

#ifdef DEV_BUILD
static void print_xsqlda (
    XSQLDA	*xsqlda)
{
/*****************************************
 *
 *	p r i n t _ x s q l d a 
 *
 *****************************************
 *
 * print an sqlda
 *
 *****************************************/
XSQLVAR	*xvar, *end_var;

if (!xsqlda)
    return;

PRINTF ("SQLDA Version %d\n", xsqlda->version);
PRINTF ("      sqldaid %.8s\n", xsqlda->sqldaid);
PRINTF ("      sqldabc %d\n", xsqlda->sqldabc);
PRINTF ("      sqln    %d\n", xsqlda->sqln);
PRINTF ("      sqld    %d\n", xsqlda->sqld);

xvar = xsqlda->sqlvar;
for (end_var = xvar + xsqlda->sqld; xvar < end_var; xvar++)
    PRINTF ("         %.31s %.31s type: %d, scale %d, len %d subtype %d\n", 
		xvar->sqlname, xvar->relname, xvar->sqltype, 
		xvar->sqlscale, xvar->sqllen, xvar->sqlsubtype);
}
#endif

static void sqlvar_to_xsqlvar (
    SQLVAR	*sqlvar,
    XSQLVAR	*xsqlvar)
{
/*****************************************
 *
 *	s q l v a r _ t o _ x s q l v a r
 *
 *****************************************
 *
 * Functional Description
 *	Move an SQLVAR to an XSQLVAR.
 *	THIS DOES NOT MOVE NAME RELATED DATA.
 *
 *****************************************/

xsqlvar->sqltype = sqlvar->sqltype;
xsqlvar->sqldata = sqlvar->sqldata;
xsqlvar->sqlind = sqlvar->sqlind;

xsqlvar->sqlsubtype = 0;
xsqlvar->sqlscale = 0;
xsqlvar->sqllen = sqlvar->sqllen;
if ((xsqlvar->sqltype & ~1) == SQL_LONG)
    {
    xsqlvar->sqlscale = xsqlvar->sqllen >> 8;
    xsqlvar->sqllen   = sizeof (SLONG);
    }
else if ((xsqlvar->sqltype & ~1) == SQL_SHORT)
    {
    xsqlvar->sqlscale = xsqlvar->sqllen >> 8;
    xsqlvar->sqllen   = sizeof (SSHORT);
    }
else if ((xsqlvar->sqltype & ~1) == SQL_INT64)
    {
    xsqlvar->sqlscale = xsqlvar->sqllen >> 8;
    xsqlvar->sqllen   = sizeof (SINT64);
    }
else if ((xsqlvar->sqltype & ~1) == SQL_QUAD)
    {
    xsqlvar->sqlscale = xsqlvar->sqllen >> 8;
    xsqlvar->sqllen   = sizeof (GDS__QUAD);
    }
}

static void xsqlvar_to_sqlvar (
    XSQLVAR	*xsqlvar,
    SQLVAR	*sqlvar)
{
/*****************************************
 *
 *	x s q l v a r _ t o _ s q l v a r
 *
 *****************************************
 *
 * Functional Description
 *	Move an XSQLVAR to an SQLVAR.
 *
 *****************************************/

sqlvar->sqltype = xsqlvar->sqltype;
sqlvar->sqlname_length = xsqlvar->aliasname_length;

/* N.B., this may not NULL-terminate the name... */

memcpy (sqlvar->sqlname, xsqlvar->aliasname, sizeof (sqlvar->sqlname));

sqlvar->sqllen = xsqlvar->sqllen;
if ((sqlvar->sqltype & ~1) == SQL_LONG)
    sqlvar->sqllen = sizeof (SLONG) | (xsqlvar->sqlscale << 8);
else if ((sqlvar->sqltype & ~1) == SQL_SHORT)
    sqlvar->sqllen = sizeof (SSHORT) | (xsqlvar->sqlscale << 8);
else if ((sqlvar->sqltype & ~1) == SQL_INT64)
    sqlvar->sqllen = sizeof (SINT64) | (xsqlvar->sqlscale << 8);
else if ((sqlvar->sqltype & ~1) == SQL_QUAD)
    sqlvar->sqllen = sizeof (GDS__QUAD) | (xsqlvar->sqlscale << 8);
}  

