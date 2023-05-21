/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		make.c
 *	DESCRIPTION:	Routines to make various blocks.
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

#include <ctype.h>
#include <string.h>
#include "../dsql/dsql.h"
#include "../dsql/node.h"
#include "../dsql/sym.h"
#include "../jrd/gds.h"
#include "../jrd/intl.h"
#include "../jrd/constants.h"
#include "../jrd/align.h"
#include "../dsql/alld_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/hsh_proto.h"
#include "../dsql/make_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/dsc_proto.h"

ASSERT_FILENAME				/* declare things assert() needs */

/* InterBase provides transparent conversion from string to date in
 * contexts where it makes sense.  This macro checks a descriptor to
 * see if it is something that *could* represent a date value
 */
#define COULD_BE_DATE(d)	(DTYPE_IS_DATE((d).dsc_dtype) || ((d).dsc_dtype <= dtype_any_text))

/* One of d1,d2 is time, the other is date */
#define IS_DATE_AND_TIME(d1,d2)	\
  ((((d1).dsc_dtype==dtype_sql_time)&&((d2).dsc_dtype==dtype_sql_date)) || \
   (((d2).dsc_dtype==dtype_sql_time)&&((d1).dsc_dtype==dtype_sql_date)))


NOD MAKE_constant (
    STR		constant,
    int		numeric_flag)
{
/**************************************
 *
 *	M A K E _ c o n s t a n t
 *
 **************************************
 *
 * Functional description
 *	Make a constant node.
 *
 **************************************/
NOD	node;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

node = (NOD) ALLOCDV (type_nod,
		      (numeric_flag ==  CONSTANT_TIMESTAMP ||
		       numeric_flag ==  CONSTANT_SINT64) ? 2 : 1);
node->nod_type = nod_constant;

if (numeric_flag == CONSTANT_SLONG)
    {
    node->nod_desc.dsc_dtype = dtype_long;
    node->nod_desc.dsc_length = sizeof (SLONG);
    node->nod_desc.dsc_scale = 0;
    node->nod_desc.dsc_sub_type = 0;
    node->nod_desc.dsc_address = (UCHAR*) node->nod_arg;
    node->nod_arg [0] = (NOD) constant;
    }
else if (numeric_flag == CONSTANT_DOUBLE)
    {
    DEV_BLKCHK (constant, type_str);

    /* This is a numeric value which is transported to the engine as
     * a string.  The engine will convert it. Use dtype_double so that
       the engine can distinguish it from an actual string.
       Note: Due to the size of dsc_scale we are limited to numeric
	     constants of less than 256 bytes.  
     */
    node->nod_desc.dsc_dtype = dtype_double;
    node->nod_desc.dsc_scale = constant->str_length;  /* Scale has no use for double */
    node->nod_desc.dsc_sub_type = 0;
    node->nod_desc.dsc_length = sizeof (double);
    node->nod_desc.dsc_address = constant->str_data;
    node->nod_desc.dsc_ttype = ttype_ascii;
    node->nod_arg [0] = (NOD) constant;
    }
else if (numeric_flag == CONSTANT_SINT64)
    {
      /* We convert the string to an int64.  We treat the two adjacent
	 32-bit words node->nod_arg[0] and node->nod_arg[1] as a
	 64-bit integer: if we ever port to a platform which requires
	 8-byte alignment of int64 data, we will have to force 8-byte
	 alignment of node->nod_arg, which is now only guaranteed
	 4-byte alignment.    -- ChrisJ 1999-02-20 */

    UINT64   value = 0;
    BOOLEAN  point_seen = 0;
    UCHAR   *p = constant->str_data;

    node->nod_desc.dsc_dtype = dtype_int64;
    node->nod_desc.dsc_length = sizeof (SINT64);
    node->nod_desc.dsc_scale = 0;
    node->nod_desc.dsc_sub_type = 0;
    node->nod_desc.dsc_address = (UCHAR*) node->nod_arg;

    /* Now convert the string to an int64.  We can omit testing for
       overflow, because we would never have gotten here if yylex
       hadn't recognized the string as a valid 64-bit integer value.
       We *might* have "9223372936854775808", which works an an int64
       only if preceded by a '-', but that issue is handled in GEN_expr,
       and need not be addressed here. */

    while (isdigit(*p))
	value = 10 * value + (*(p++) - '0');
    if (*p++ = '.')
        {
	while (isdigit(*p))
	    {
	    value = 10 * value + (*p++ - '0');
	    node->nod_desc.dsc_scale--;
	    }
        }
    
    *(UINT64 *) (node->nod_desc.dsc_address) = value;
    
    }
else if (numeric_flag == CONSTANT_DATE ||
	 numeric_flag == CONSTANT_TIME ||
	 numeric_flag == CONSTANT_TIMESTAMP)
    {
    DSC	tmp;

	/* Setup the constant's descriptor */

    switch (numeric_flag)
	{
	case CONSTANT_DATE:
	    node->nod_desc.dsc_dtype = dtype_sql_date;
	    break;
	case CONSTANT_TIME:
	    node->nod_desc.dsc_dtype = dtype_sql_time;
	    break;
	case CONSTANT_TIMESTAMP:
	    node->nod_desc.dsc_dtype = dtype_timestamp;
	    break;
	};
    node->nod_desc.dsc_sub_type = 0;
    node->nod_desc.dsc_scale = 0;
    node->nod_desc.dsc_length = type_lengths [node->nod_desc.dsc_dtype];
    node->nod_desc.dsc_address = (UCHAR*) node->nod_arg;

	/* Set up a descriptor to point to the string */

    tmp.dsc_dtype = dtype_text;
    tmp.dsc_scale = 0;
    tmp.dsc_flags = 0;
    tmp.dsc_ttype = ttype_ascii;
    tmp.dsc_length = constant->str_length;
    tmp.dsc_address = constant->str_data;

	/* Now invoke the string_to_date/time/timestamp routines */

    CVT_move (&tmp, &node->nod_desc, ERRD_post);
    }
else
    {
    assert (numeric_flag == CONSTANT_STRING);
    DEV_BLKCHK (constant, type_str);

    node->nod_desc.dsc_dtype = dtype_text;
    node->nod_desc.dsc_sub_type = 0;
    node->nod_desc.dsc_scale = 0;
    node->nod_desc.dsc_length = constant->str_length;
    node->nod_desc.dsc_address = constant->str_data;
    node->nod_desc.dsc_ttype = ttype_dynamic;
    /* carry a pointer to the constant to resolve character set in pass1 */
    node->nod_arg [0] = (NOD) constant;
    }

return node;
}

STR MAKE_cstring (
    CONST SCHAR	*str)
{
/**************************************
 *
 *	M A K E _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Make a string node for a string whose
 *	length is not known, but is null-terminated.
 *
 **************************************/

return MAKE_string ((UCHAR *)str, strlen(str));
}

void MAKE_desc (
    DSC		*desc,
    NOD		node)
{
/**************************************
 *
 *	M A K E _ d e s c
 *
 **************************************
 *
 * Functional description
 *	Make a descriptor from input node.
 *
 **************************************/
DSC	desc1, desc2;
USHORT	dtype, dtype1, dtype2;
MAP	map;
CTX	context;
DSQL_REL	relation;
UDF	udf;
FLD	field;

DEV_BLKCHK (node, type_nod);

/* If we already know the datatype, don't worry about anything */

if (node->nod_desc.dsc_dtype)
    {
    *desc = node->nod_desc;
    return;
    }

switch (node->nod_type)
    {
    case nod_agg_count:
/* count2
    case nod_agg_distinct:
*/
	desc->dsc_dtype = dtype_long;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
        desc->dsc_flags = 0;
	return;

    case nod_map:
	map = (MAP) node->nod_arg [e_map_map];
	MAKE_desc (desc, map->map_node);
	return;

    case nod_agg_min:
    case nod_agg_max:
	MAKE_desc (desc, node->nod_arg [0]);
        desc->dsc_flags = DSC_nullable;
	return;

    case nod_agg_average:
	MAKE_desc (desc, node->nod_arg [0]);
        desc->dsc_flags = DSC_nullable;
	dtype = desc->dsc_dtype;
        if (!DTYPE_CAN_AVERAGE (dtype))
	    ERRD_post (gds__expression_eval_err, 0);
	return;

    case nod_agg_average2:
	MAKE_desc (desc, node->nod_arg [0]);
        desc->dsc_flags = DSC_nullable;
        dtype = desc->dsc_dtype;
	if (!DTYPE_CAN_AVERAGE (dtype))
	    ERRD_post (gds__expression_eval_err, 0);
	if (DTYPE_IS_EXACT(dtype))
	    {
	    desc->dsc_dtype  = dtype_int64;
	    desc->dsc_length = sizeof (SINT64);
	    node->nod_flags |= NOD_COMP_DIALECT;
	    }
	else
	    {
	    desc->dsc_dtype  = dtype_double;
	    desc->dsc_length = sizeof (double);
	    }
	return;

    case nod_agg_total:
	MAKE_desc (desc, node->nod_arg [0]);
	if (desc->dsc_dtype == dtype_short)
	    {
	    desc->dsc_dtype = dtype_long;
	    desc->dsc_length = sizeof (SLONG);
	    }
        else if (desc->dsc_dtype == dtype_int64)
	    {
	    desc->dsc_dtype = dtype_double;
	    desc->dsc_length = sizeof (double);
	    }
        desc->dsc_flags = DSC_nullable;
	return;

    case nod_agg_total2:
	MAKE_desc (desc, node->nod_arg [0]);
	dtype = desc->dsc_dtype;
	if (DTYPE_IS_EXACT(dtype))
	    {
	    desc->dsc_dtype  = dtype_int64;
	    desc->dsc_length = sizeof (SINT64);
	    node->nod_flags |= NOD_COMP_DIALECT;
	    }
	else
	    {
	    desc->dsc_dtype  = dtype_double;
	    desc->dsc_length = sizeof (double);
	    }
        desc->dsc_flags = DSC_nullable;
	return;

    case nod_concatenate:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	desc->dsc_scale = 0;
	desc->dsc_dtype = dtype_varying;
	if (desc1.dsc_dtype <= dtype_any_text)
	    desc->dsc_ttype = desc1.dsc_ttype;
	else
	    desc->dsc_ttype = ttype_ascii;
	desc->dsc_length = sizeof (USHORT) +
		DSC_string_length (&desc1) + DSC_string_length (&desc2);
        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	return;	

    case nod_upcase:
	MAKE_desc (&desc1, node->nod_arg [0]);
	if (desc1.dsc_dtype <= dtype_any_text)
	    {
	    *desc = desc1;
	    return;
	    }
	desc->dsc_dtype = dtype_varying;
	desc->dsc_scale = 0;
	desc->dsc_ttype = ttype_ascii;
	desc->dsc_length = sizeof (USHORT) + DSC_string_length (&desc1);
        desc->dsc_flags = desc1.dsc_flags & DSC_nullable;
	return;

    case nod_cast:
	field = (FLD) node->nod_arg [e_cast_target];
	MAKE_desc_from_field (desc, field);
	MAKE_desc (&desc1, node->nod_arg [e_cast_source]);
	desc->dsc_flags = desc1.dsc_flags & DSC_nullable;
	return;

#ifdef DEV_BUILD
    case nod_collate:
	ERRD_bugcheck ("Not expecting nod_collate in dsql/MAKE_desc");
	return;
#endif

    case nod_add:
    case nod_subtract:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	dtype1 = desc1.dsc_dtype;
	dtype2 = desc2.dsc_dtype;

	if (dtype_int64 == dtype1)
	    dtype1 = dtype_double;
	if (dtype_int64 == dtype2)
	    dtype2 = dtype_double;
	
	dtype = MAX (dtype1, dtype2);

        if (DTYPE_IS_BLOB (dtype))
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, gds__dsql_no_blob_array,
                       0);

        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	switch (dtype)
	    {
	    case dtype_sql_time:
	    case dtype_sql_date:
		/* Forbid <date/time> +- <string> */
		if (IS_DTYPE_ANY_TEXT (desc1.dsc_dtype) || 
		    IS_DTYPE_ANY_TEXT (desc2.dsc_dtype))
		    ERRD_post (gds__expression_eval_err, 0);

	    case dtype_timestamp:

		/* Allow <timestamp> +- <string> (historical) */
		if (COULD_BE_DATE (desc1) &&
		    COULD_BE_DATE (desc2))
		    {
		    if (node->nod_type == nod_subtract)
		        {
					/* <any date> - <any date> */

			/* Legal permutations are:
				<timestamp> - <timestamp>
				<timestamp> - <date>
				<date> - <date>
				<date> - <timestamp>
				<time> - <time>
				<timestamp> - <string>
				<string> - <timestamp> 
				<string> - <string>	 */

			if (IS_DTYPE_ANY_TEXT (dtype1))
			    dtype == dtype_timestamp;
			else if (IS_DTYPE_ANY_TEXT (dtype2))
			    dtype == dtype_timestamp;
			else if (dtype1 == dtype2)
			    dtype = dtype1;
		 	else if ((dtype1 == dtype_timestamp) && 
				(dtype2 == dtype_sql_date))
			    dtype = dtype_timestamp;
		 	else if ((dtype2 == dtype_timestamp) && 
				(dtype1 == dtype_sql_date))
			    dtype = dtype_timestamp;
			else
			    ERRD_post (gds__expression_eval_err, 0);

			if (dtype == dtype_sql_date)
			    {
			    desc->dsc_dtype = dtype_long;
			    desc->dsc_length = sizeof (SLONG);
			    desc->dsc_scale = 0;
			    }
			else if (dtype == dtype_sql_time)
			    {
			    desc->dsc_dtype = dtype_long;
			    desc->dsc_length = sizeof (SLONG);
			    desc->dsc_scale = ISC_TIME_SECONDS_PRECISION_SCALE;
			    }
			else
			    {
			    assert (dtype == dtype_timestamp);
			    desc->dsc_dtype = dtype_double;
			    desc->dsc_length = sizeof (double);
			    desc->dsc_scale = 0;
			    }
		        }
		    else if (IS_DATE_AND_TIME (desc1, desc2))
			{
					/* <date> + <time> */
					/* <time> + <date> */
		        desc->dsc_dtype = dtype_timestamp;
		        desc->dsc_length = type_lengths [dtype_timestamp];
		        desc->dsc_scale = 0;
			}
		    else
					/* <date> + <date> */
			ERRD_post (gds__expression_eval_err, 0);
		    }
		else if (DTYPE_IS_DATE (desc1.dsc_dtype) ||
					/* <date> +/- <non-date> */
		         (node->nod_type == nod_add))
		    			/* <non-date> + <date> */
		    {
		    desc->dsc_dtype = desc1.dsc_dtype;
		    if (!DTYPE_IS_DATE (desc->dsc_dtype))
			desc->dsc_dtype = desc2.dsc_dtype;
		    assert (DTYPE_IS_DATE (desc->dsc_dtype));
		    desc->dsc_length = type_lengths [desc->dsc_dtype];
		    desc->dsc_scale = 0;
		    }
		else
		    {
		    /* <non-date> - <date> */
		    assert (node->nod_type == nod_subtract);
		    ERRD_post (gds__expression_eval_err, 0);
		    }
		return;

	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
	    case dtype_double:
	    case dtype_real:
		desc->dsc_dtype = dtype_double;
		desc->dsc_sub_type = 0;
		desc->dsc_scale = 0;
		desc->dsc_length = sizeof (double);
		return;

	    default:
		desc->dsc_dtype = dtype_long;
		desc->dsc_sub_type = 0;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = MIN ( NUMERIC_SCALE (desc1), 
					NUMERIC_SCALE (desc2) );
		break;
	    }
	return;

    case nod_add2:
    case nod_subtract2:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	dtype1 = desc1.dsc_dtype;
	dtype2 = desc2.dsc_dtype;

	/* Arrays and blobs can never partipate in addition/subtraction*/
        if (DTYPE_IS_BLOB (desc1.dsc_dtype) ||
            DTYPE_IS_BLOB (desc2.dsc_dtype))
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, gds__dsql_no_blob_array,
                       0);

	/* In Dialect 2 or 3, strings can never partipate in addition / sub 
	   (Use a specific cast instead) */
	if (IS_DTYPE_ANY_TEXT (desc1.dsc_dtype) || 
	    IS_DTYPE_ANY_TEXT (desc2.dsc_dtype))
		ERRD_post (gds__expression_eval_err, 0);

	/* Determine the TYPE of arithmetic to perform, store it
	   in dtype.  Note:  this is different from the result of
	   the operation, as <timestamp>-<timestamp> uses
	   <timestamp> arithmetic, but returns a <double> */
	if (DTYPE_IS_EXACT(desc1.dsc_dtype) && DTYPE_IS_EXACT(desc2.dsc_dtype))
	    dtype = dtype_int64;
	else if (DTYPE_IS_NUMERIC(desc1.dsc_dtype)
		&& DTYPE_IS_NUMERIC(desc2.dsc_dtype))
	    {
	    assert (DTYPE_IS_APPROX (desc1.dsc_dtype) ||
		    DTYPE_IS_APPROX (desc2.dsc_dtype));
	    dtype = dtype_double;
	    }
	else
	    {
	    /* mixed numeric and non-numeric: */

	    assert (DTYPE_IS_DATE(dtype1) || DTYPE_IS_DATE(dtype2));

		/* The MAX(dtype) rule doesn't apply with dtype_int64 */

	    if (dtype_int64 == dtype1)
	        dtype1 = dtype_double;
	    if (dtype_int64 == dtype2)
	        dtype2 = dtype_double;
	
	    dtype = MAX (dtype1, dtype2);
	    }

        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	switch (dtype)
	    {
	    case dtype_sql_time:
	    case dtype_sql_date:
	    case dtype_timestamp:

		if ((DTYPE_IS_DATE (dtype1) || (dtype1 == dtype_null)) &&
		    (DTYPE_IS_DATE (dtype2) || (dtype2 == dtype_null)))
		    {
		    if (node->nod_type == nod_subtract2)
		        {
					/* <any date> - <any date> */
			/* Legal permutations are:
				<timestamp> - <timestamp>
				<timestamp> - <date>
				<date> - <date>
				<date> - <timestamp>
				<time> - <time> */

			if (dtype1 == dtype2)
			    dtype = dtype1;
		 	else if ((dtype1 == dtype_timestamp) && 
				(dtype2 == dtype_sql_date))
			    dtype = dtype_timestamp;
		 	else if ((dtype2 == dtype_timestamp) && 
				(dtype1 == dtype_sql_date))
			    dtype = dtype_timestamp;
			else
			    ERRD_post (gds__expression_eval_err, 0);

			if (dtype == dtype_sql_date)
			    {
			    desc->dsc_dtype = dtype_long;
			    desc->dsc_length = sizeof (SLONG);
			    desc->dsc_scale = 0;
			    }
			else if (dtype == dtype_sql_time)
			    {
			    desc->dsc_dtype = dtype_long;
			    desc->dsc_length = sizeof (SLONG);
			    desc->dsc_scale = ISC_TIME_SECONDS_PRECISION_SCALE;
			    }
			else
			    {
			    assert (dtype == dtype_timestamp);
			    desc->dsc_dtype = dtype_int64;
                            desc->dsc_length = sizeof (SINT64);
			    desc->dsc_scale = -9;
			    }
		        }
		    else if (IS_DATE_AND_TIME (desc1, desc2))
			{
					/* <date> + <time> */
					/* <time> + <date> */
		        desc->dsc_dtype = dtype_timestamp;
		        desc->dsc_length = type_lengths [dtype_timestamp];
		        desc->dsc_scale = 0;
			}
		    else
					/* <date> + <date> */
			ERRD_post (gds__expression_eval_err, 0);
		    }
		else if (DTYPE_IS_DATE (desc1.dsc_dtype) ||
					/* <date> +/- <non-date> */
		         (node->nod_type == nod_add2))
		    			/* <non-date> + <date> */
		    {
		    desc->dsc_dtype = desc1.dsc_dtype;
		    if (!DTYPE_IS_DATE (desc->dsc_dtype))
			desc->dsc_dtype = desc2.dsc_dtype;
		    assert (DTYPE_IS_DATE (desc->dsc_dtype));
		    desc->dsc_length = type_lengths [desc->dsc_dtype];
		    desc->dsc_scale = 0;
		    }
		else
		    {
		    /* <non-date> - <date> */
		    assert (node->nod_type == nod_subtract2);
		    ERRD_post (gds__expression_eval_err, 0);
		    }
		return;

	    case dtype_varying:
	    case dtype_cstring:
	    case dtype_text:
	    case dtype_double:
	    case dtype_real:
		desc->dsc_dtype = dtype_double;
		desc->dsc_sub_type = 0;
		desc->dsc_scale = 0;
		desc->dsc_length = sizeof (double);
		return;

	    case dtype_short:
	    case dtype_long:
	    case dtype_int64:
		desc->dsc_dtype = dtype_int64;
		desc->dsc_sub_type = 0;
		desc->dsc_length = sizeof (SINT64);
		
		/* The result type is int64 because both operands are
		   exact numeric: hence we don't need the NUMERIC_SCALE
		   macro here. */
		assert (DTYPE_IS_EXACT (desc1.dsc_dtype));
		assert (DTYPE_IS_EXACT (desc2.dsc_dtype));
		
		desc->dsc_scale = MIN ( desc1.dsc_scale, desc2.dsc_scale );
		node->nod_flags |= NOD_COMP_DIALECT;
		break;
	    
	    default:
	        /* a type which cannot participate in an add or subtract */ 
		ERRD_post (gds__expression_eval_err, 0);
	    }
	return;

    case nod_multiply:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	dtype = DSC_multiply_blr4_result [desc1.dsc_dtype] [desc2.dsc_dtype];

        if (dtype_null == dtype)
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, gds__dsql_no_blob_array,
                       0);

        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	switch (dtype)
	    {
	    case dtype_double:
		desc->dsc_dtype = dtype_double;
		desc->dsc_sub_type = 0;
		desc->dsc_scale = 0;
		desc->dsc_length = sizeof (double);
		break;

	    case dtype_long:
		desc->dsc_dtype = dtype_long;
		desc->dsc_sub_type = 0;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = NUMERIC_SCALE (desc1) + NUMERIC_SCALE (desc2);
		break;

	    default:
	        ERRD_post (gds__expression_eval_err, 0);
	    }
	return;

    case nod_multiply2:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	dtype = DSC_multiply_result [desc1.dsc_dtype] [desc2.dsc_dtype];

        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	switch (dtype)
	    {
	    case dtype_double:
		desc->dsc_dtype = dtype_double;
		desc->dsc_sub_type = 0;
		desc->dsc_scale = 0;
		desc->dsc_length = sizeof (double);
		break;
		
	    case dtype_int64:
		desc->dsc_dtype = dtype_int64;
		desc->dsc_sub_type = 0;
		desc->dsc_length = sizeof (SINT64);
		desc->dsc_scale = NUMERIC_SCALE (desc1) + NUMERIC_SCALE (desc2);
		node->nod_flags |= NOD_COMP_DIALECT;
		break;
		
	    default:
	        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
			   gds_arg_gds, gds__dsql_no_blob_array,
			   0);
		break;
	    }
	return;

    case nod_count:
	desc->dsc_dtype = dtype_long;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_flags = 0;
	return;

    case nod_divide:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);

	dtype1 = desc1.dsc_dtype;
	if (dtype_int64 == dtype1)
	    dtype1 = dtype_double;
	
	dtype2 = desc2.dsc_dtype;
	if (dtype_int64 == dtype2)
	    dtype2 = dtype_double;
	
	dtype = MAX (dtype1, dtype2);

        if (!DTYPE_CAN_DIVIDE (dtype))
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, gds__dsql_no_blob_array,
                       0);
	desc->dsc_dtype = dtype_double;
	desc->dsc_length = sizeof (double);
	desc->dsc_scale = 0;
        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	return;

    case nod_divide2:
	MAKE_desc (&desc1, node->nod_arg [0]);
	MAKE_desc (&desc2, node->nod_arg [1]);
	desc->dsc_dtype = dtype =
	  DSC_multiply_result [desc1.dsc_dtype] [desc2.dsc_dtype];

        desc->dsc_flags = (desc1.dsc_flags | desc2.dsc_flags) & DSC_nullable;
	switch (dtype)
	    {
	    case dtype_int64:
	        desc->dsc_length = sizeof (SINT64);
		desc->dsc_scale = NUMERIC_SCALE (desc1) + NUMERIC_SCALE (desc2);
		node->nod_flags |= NOD_COMP_DIALECT;
		break;
		
	    case dtype_double:
	        desc->dsc_length = sizeof (double);
		desc->dsc_scale = 0;
		break;

	    default:
	        ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
			   gds_arg_gds, gds__dsql_no_blob_array,
			   0);
	        break;
	    }

	return;

    case nod_negate:
	MAKE_desc (desc, node->nod_arg [0]);
        if (!DTYPE_CAN_NEGATE (desc->dsc_dtype))
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, gds__dsql_no_blob_array,
                       0);
	return;

    case nod_alias:
	MAKE_desc (desc, node->nod_arg [e_alias_value]);
	return;

    case nod_dbkey:
	/* Fix for bug 10072 check that the target is a relation */
        context = (CTX) node->nod_arg [0]->nod_arg [0];
        if (relation = context->ctx_relation)
	    {
            desc->dsc_dtype = dtype_text;
            desc->dsc_length = relation->rel_dbkey_length;
	    desc->dsc_flags = 0;
	    desc->dsc_ttype = ttype_binary;
	    }
	else 
	    {
            ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -607,
                       gds_arg_gds, isc_dsql_dbkey_from_non_table,
                       0);
	    }
        return;

    case nod_udf:
	udf = (UDF) node->nod_arg [0];
        desc->dsc_dtype = udf->udf_dtype;
        desc->dsc_length = udf->udf_length;
	desc->dsc_scale = udf->udf_scale;
	desc->dsc_flags = 0;
	desc->dsc_ttype = udf->udf_sub_type;
	return;

    case nod_gen_id:
	desc->dsc_dtype = dtype_long;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_flags = node->nod_arg [e_gen_id_value]->nod_desc.dsc_flags;
	return;

    case nod_gen_id2:
	desc->dsc_dtype = dtype_int64;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_length = sizeof (SINT64);
	desc->dsc_flags = node->nod_arg [e_gen_id_value]->nod_desc.dsc_flags;
	node->nod_flags |= NOD_COMP_DIALECT;
	return;

    case nod_field:
     	ERRD_post (gds__sqlerr, gds_arg_number, (SLONG) -203,
           	gds_arg_gds, gds__dsql_field_ref, 0);
	return;

    case nod_user_name:
        desc->dsc_dtype = dtype_varying;
	desc->dsc_scale = 0;
	desc->dsc_flags = 0;
	desc->dsc_ttype = ttype_dynamic;
        desc->dsc_length = USERNAME_LENGTH + sizeof (USHORT);
        return;

    case nod_current_time:
        desc->dsc_dtype = dtype_sql_time;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_flags = 0;
        desc->dsc_length = type_lengths [desc->dsc_dtype];
        return;

    case nod_current_date:
        desc->dsc_dtype = dtype_sql_date;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_flags = 0;
        desc->dsc_length = type_lengths [desc->dsc_dtype];
        return;

    case nod_current_timestamp:
        desc->dsc_dtype = dtype_timestamp;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_flags = 0;
        desc->dsc_length = type_lengths [desc->dsc_dtype];
        return;

    case nod_extract:
	MAKE_desc (&desc1, node->nod_arg [e_extract_value]);
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	desc->dsc_flags = (desc1.dsc_flags & DSC_nullable);
	if (*(ULONG *)node->nod_arg [e_extract_part]->nod_desc.dsc_address 
		== blr_extract_second)
	    {
	    /* QUADDATE - maybe this should be DECIMAL(6,4) */
	    desc->dsc_dtype = dtype_long;
	    desc->dsc_scale = ISC_TIME_SECONDS_PRECISION_SCALE;
	    desc->dsc_length = sizeof (ULONG);
	    return;
	    }
        desc->dsc_dtype = dtype_short;
	desc->dsc_length = sizeof (SSHORT);
	return;

    case nod_parameter:
	/* We don't actually know the datatype of a parameter -
	   we have to guess it based on the context that the 
	   parameter appears in. (This is done is pass1.c::set_parameter_type())
	   However, a parameter can appear as part of an expression.
	   As MAKE_desc is used for both determination of parameter
	   types and for expression type checking, we just continue. */
	return;

    case nod_null:
	/* This occurs when SQL statement specifies a literal NULL, eg:
	 *  SELECT NULL FROM TABLE1;
	 * As we don't have a <dtype_null, SQL_NULL> datatype pairing,
	 * we don't know how to map this NULL to a host-language
	 * datatype.  Therefore we now describe it as a 
	 * CHAR(1) CHARACTER SET NONE type.
	 * No value will ever be sent back, as the value of the select
	 * will be NULL - this is only for purposes of DESCRIBING
	 * the statement.  Note that this mapping could be done in dsql.c
	 * as part of the DESCRIBE statement - but I suspect other areas
	 * of the code would break if this is declared dtype_null.
	 */
        desc->dsc_dtype = dtype_text;
        desc->dsc_length = 1;
	desc->dsc_scale = 0;
	desc->dsc_ttype = 0;
	desc->dsc_flags = DSC_nullable;
	return;

    case nod_via:
        MAKE_desc (desc, node->nod_arg [e_via_value_1]);
	/**
	    Set the descriptor flag as nullable. The
	    select expression may or may not return 
	    this row based on the WHERE clause. Setting this
	    flag warns the client to expect null values.
	    (bug 10379)
	**/
	desc->dsc_flags |= DSC_nullable;
        return;

    default:
	ASSERT_FAIL;				    /* unexpected nod type */

    case nod_dom_value:				/* computed value not used */
        /* By the time we get here, any nod_dom_value node should have had
	 * its descriptor set to the type of the domain being created, or
	 * to the type of the existing domain to which a CHECK constraint
	 * is being added.
	 */
	assert (node->nod_desc.dsc_dtype != dtype_null);
	if (desc != &node->nod_desc)
	    *desc = node->nod_desc;
        return;
    }
}

void MAKE_desc_from_field (
    DSC		*desc,
    FLD		field)
{
/**************************************
 *
 *	M A K E _ d e s c _ f r o m _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Compute a DSC from a field's description information.
 *
 **************************************/

DEV_BLKCHK (field, type_fld);

desc->dsc_dtype = field->fld_dtype;
desc->dsc_scale = field->fld_scale;
desc->dsc_sub_type = field->fld_sub_type;
desc->dsc_length = field->fld_length;
desc->dsc_flags = (field->fld_flags & FLD_nullable) ? DSC_nullable : 0;
if (desc->dsc_dtype <= dtype_any_text)
    {
    INTL_ASSIGN_DSC (desc, field->fld_character_set_id, field->fld_collation_id);
    }
else if (desc->dsc_dtype == dtype_blob)
    desc->dsc_scale = field->fld_character_set_id;
}

NOD MAKE_field (
    CTX		context,
    FLD		field,
    NOD		indices)
{
/**************************************
 *
 *	M A K E _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make up a field node.
 *
 **************************************/
NOD	node;

DEV_BLKCHK (context, type_ctx);
DEV_BLKCHK (field, type_fld);
DEV_BLKCHK (indices, type_nod);

node = MAKE_node (nod_field, e_fld_count);
node->nod_arg [e_fld_context] = (NOD) context;
node->nod_arg [e_fld_field] = (NOD) field;
if (field->fld_dimensions)
    {
    if (indices)
	{
	node->nod_arg [e_fld_indices] = indices;
	MAKE_desc_from_field (&node->nod_desc, field);
	node->nod_desc.dsc_dtype = field->fld_element_dtype;
	node->nod_desc.dsc_length = field->fld_element_length;
	/*
        node->nod_desc.dsc_scale = field->fld_scale;
        node->nod_desc.dsc_sub_type = field->fld_sub_type;
	*/
	}
    else
	{
	node->nod_desc.dsc_dtype = dtype_array;
	node->nod_desc.dsc_length = sizeof (GDS__QUAD);
	node->nod_desc.dsc_scale = field->fld_scale;
	node->nod_desc.dsc_sub_type = field->fld_sub_type;
	}
    }
else
    {
    assert (!indices);
    MAKE_desc_from_field (&node->nod_desc, field);
    }

if ((field->fld_flags & FLD_nullable) ||
    (context->ctx_flags & CTX_outer_join))
    node->nod_desc.dsc_flags = DSC_nullable;

return node;
}

NOD MAKE_list (
    LLS		stack)
{
/**************************************
 *
 *	M A K E _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Make a list node from a linked list stack of things.
 *
 **************************************/
LLS	temp;
USHORT	count;
NOD	node, *ptr;

DEV_BLKCHK (stack, type_lls);

for (temp = stack, count = 0; temp; temp = temp->lls_next)
    ++count;

node = MAKE_node (nod_list, count);
ptr = node->nod_arg + count;

while (stack)
    *--ptr = (NOD) LLS_POP (&stack);

return node;
}

NOD MAKE_node (
    NOD_TYPE	type,
    int		count)
{
/**************************************
 *
 *	M A K E _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Make a node of given type.
 *
 **************************************/
NOD	node;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

node = (NOD) ALLOCDV (type_nod, count);
node->nod_type = type;
node->nod_count = count;

return node;
}

STR MAKE_numeric (
    CONST UCHAR	*str,
    int		length)
{
/**************************************
 *
 *	M A K E _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *	routine for to make a numeric block
 *	(really just a string block with all
 *	digits in the string)
 *
 **************************************/
UCHAR	*p;
STR	string;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

string = (STR) ALLOCDV (type_str, length);
string->str_length = length;
MOVE_FAST (str, p, length);

return string;
}

PAR MAKE_parameter (
    MSG		message,
    USHORT	sqlda_flag,
    USHORT	null_flag)
{
/**************************************
 *
 *	M A K E _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Generate a parameter block for a message.  If requested,
 *	set up for a null flag as well.
 *
 **************************************/
PAR	parameter, null;
TSQL    tdsql;

DEV_BLKCHK (message, type_msg);

tdsql = GET_THREAD_DATA;

parameter = (PAR) ALLOCD (type_par);
parameter->par_message = message;
if (parameter->par_next = message->msg_parameters)
    parameter->par_next->par_ordered = parameter;
else
    message->msg_par_ordered = parameter;
message->msg_parameters = parameter;
parameter->par_parameter = message->msg_parameter++;
parameter->par_rel_name = NULL;
parameter->par_owner_name = NULL;

/* If the parameter is used declared, set SQLDA index */

if (sqlda_flag)
    parameter->par_index = ++message->msg_index;

/* If a null handing has been requested, set up a null flag */

if (null_flag)
    {
    parameter->par_null = null = MAKE_parameter (message, FALSE, FALSE);
    null->par_desc.dsc_dtype = dtype_short;
    null->par_desc.dsc_scale = 0;
    null->par_desc.dsc_length = sizeof (SSHORT);
    }

return parameter;
}
STR MAKE_string (
    CONST UCHAR	*str,
    int		length)
{
/**************************************
 *
 *	M A K E _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Generalized routine for making a string block.
 *
 **************************************/

return MAKE_tagged_string (str, length, NULL);
}

SYM MAKE_symbol (
    DBB		database,
    CONST TEXT	*name,
    USHORT	length,
    SYM_TYPE	type,
    REQ		object)
{
/**************************************
 *
 *	M A K E _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Make a symbol for an object and insert symbol into hash table.
 *
 **************************************/
SYM	symbol;
TEXT	*p;
TSQL    tdsql;

DEV_BLKCHK (database, type_dbb);
DEV_BLKCHK (object, type_req);
assert (name);
assert (length > 0);

tdsql = GET_THREAD_DATA;

symbol = (SYM) ALLOCDV (type_sym, length);
symbol->sym_type = type;
symbol->sym_object = (BLK) object;
symbol->sym_dbb = database;
symbol->sym_length = length;
symbol->sym_string = p = symbol->sym_name;

if (length)
    MOVE_FAST (name, p, length);

HSHD_insert (symbol);

return symbol;
}

STR MAKE_tagged_string (
    CONST UCHAR	*str,
    int		length,
    CONST TEXT	*charset)
{
/**************************************
 *
 *	M A K E _ t a g g e d _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Generalized routine for making a string block.
 *	Which is tagged with a character set descriptor.
 *
 **************************************/
UCHAR	*p;
STR	string;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

string = (STR) ALLOCDV (type_str, length);
string->str_charset = charset;
string->str_length = length;
for (p = string->str_data; length; --length)
    *p++ = *str++;

return string;
}

NOD MAKE_variable (
    FLD		field,
    CONST TEXT	*name,
    USHORT	type,
    USHORT	msg_number,
    USHORT	item_number,
    USHORT	local_number)
{
/**************************************
 *
 *	M A K E _ v a r i a b l e
 *
 **************************************
 *
 * Functional description
 *	Make up a field node.
 *
 **************************************/
NOD	node;
VAR	var;
TSQL    tdsql;

DEV_BLKCHK (field, type_fld);

tdsql = GET_THREAD_DATA;

var = (VAR) ALLOCDV (type_var, strlen (name));
node = MAKE_node (nod_variable, e_var_count);
node->nod_arg [e_var_variable] = (NOD) var;
var->var_msg_number = msg_number;
var->var_msg_item = item_number;
var->var_variable_number = local_number;
var->var_field = field;
strcpy (var->var_name, name);
var->var_flags = type;
MAKE_desc_from_field (&node->nod_desc, field);

return node;
}
