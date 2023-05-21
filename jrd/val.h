/*
 *	PROGRAM:	JRD access method
 *	MODULE:		val.h
 *	DESCRIPTION:	Definitions associated with value handling
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

#ifndef _JRD_VAL_H_
#define _JRD_VAL_H_

#include "../jrd/dsc.h"

#ifdef GATEWAY
#define FLAG_BYTES(n)	(n * sizeof (SSHORT) * 2)
#else
#define FLAG_BYTES(n)	(((n + BITS_PER_LONG) & ~((ULONG)BITS_PER_LONG - 1)) >> 3)
#endif

#ifndef VMS
#define DEFAULT_DOUBLE	dtype_double
#else

#ifndef GATEWAY
#define DEFAULT_DOUBLE	dtype_double
#define SPECIAL_DOUBLE	dtype_d_float
#define CNVT_TO_DFLT(x)	MTH$CVT_D_G (x)
#define CNVT_FROM_DFLT(x)	MTH$CVT_G_D (x)
#else
#define DEFAULT_DOUBLE	dtype_d_float
#define SPECIAL_DOUBLE	dtype_double
#define CNVT_TO_DFLT(x)	MTH$CVT_G_D (x)
#define CNVT_FROM_DFLT(x)	MTH$CVT_D_G (x)
#endif

#endif

#ifndef REQUESTER
typedef struct fmt {
    struct blk	fmt_header;
    USHORT	fmt_length;
    USHORT	fmt_count;
    USHORT	fmt_version;
#if (defined PC_PLATFORM && !defined NETWARE_386)
    SCHAR	fmt_fill [2];	/* IMPORTANT!  THE SOLE PURPOSE OF THIS FIELD
			IS TO INSURE THAT THE SUCCEEDING FIELD IS ALIGNED ON A
			4 BYTE BOUNDARY.  IT IS NECESSARY BECAUSE THE 16-BIT
			COMPILER CANNOT DO THIS ALIGNMENT FOR US.  HAVING
			THIS FIELD INSURES THAT A PC_PLATFORM DATABASE IS
			IDENTICAL TO ONE ON Windows/NT. */
#endif
    struct dsc	fmt_desc [1];
#ifdef GATEWAY
    struct xdsc	fmt_ext_desc [1];
#endif
} *FMT;
#endif  /* REQUESTER */

#define MAX_FORMAT_SIZE		65535

typedef struct vary {
    USHORT	vary_length;
    UCHAR	vary_string [1];
} VARY;

/* A macro to define a local vary stack variable of a given length
   Usage:  VARY_STRING(5)	my_var;        */

#define VARY_STR(x)	struct { USHORT vary_length; UCHAR vary_string [x]; }

#ifndef REQUESTER
/* Function definition block */

typedef ENUM {
    FUN_value,
    FUN_reference,
    FUN_descriptor,
    FUN_blob_struct,
    FUN_scalar_array
} FUN_T;

typedef struct fun {
    struct blk	fun_header;
    STR		fun_exception_message;		/* message containing the exception error message */
    struct fun	*fun_homonym;			/* Homonym functions */
    struct sym	*fun_symbol;			/* Symbol block */
    int		(*fun_entrypoint)();		/* Function entrypoint */
    USHORT	fun_count;			/* Number of arguments (including return) */
    USHORT	fun_args;			/* Number of input arguments */
    USHORT	fun_return_arg;			/* Return argument */
    USHORT	fun_type;			/* Type of function */
    ULONG	fun_temp_length;		/* Temporary space required */
    struct	fun_repeat
    {
    DSC		fun_desc;			/* Datatype info */
    FUN_T	fun_mechanism;			/* Passing mechanism */
    }		fun_rpt [1];
} *FUN;

#define FUN_value	0
#define FUN_boolean	1

/* Blob passing structure */

typedef struct blob {
    SSHORT	(*blob_get_segment)();
    int		*blob_handle;
    SLONG	blob_number_segments;
    SLONG	blob_max_segment;
    SLONG	blob_total_length;
    void	(*blob_put_segment)();
    SLONG	(*blob_seek)();
} *BLOB;

/* Scalar array descriptor */

typedef struct sad {
    DSC		sad_desc;
    SLONG	sad_dimensions;
    struct	sad_repeat
    {
    SLONG	sad_lower;
    SLONG	sad_upper;
    }		sad_rpt [1];
} *SAD;
#endif /* REQUESTER */

/* Array description */

typedef struct ads {
    UCHAR	ads_version;			/* Array descriptor version number */
    UCHAR	ads_dimensions;			/* Dimensions of array */
    USHORT	ads_struct_count;		/* Number of struct elements */
    USHORT	ads_element_length;		/* Length of array element */
    USHORT	ads_length;			/* Length of array descriptor */
    SLONG	ads_count;			/* Total number of elements */
    SLONG	ads_total_length;		/* Total length of array */
    struct	ads_repeat
    {
    DSC		ads_desc;			/* Element descriptor */
    SLONG	ads_length;			/* Length of "vector" element */
    SLONG	ads_lower;			/* Lower bound */
    SLONG	ads_upper;			/* Upper bound */
    }		ads_rpt [1];
} *ADS;

#define ADS_VERSION_1	1
#define ADS_LEN(count)	(sizeof (struct ads) + (count - 1) * sizeof (struct ads_repeat))

#ifndef REQUESTER
typedef struct arr {
    struct blk	arr_header;
    UCHAR	*arr_data;			/* Data block, if allocated */
    struct blb	*arr_blob;			/* Blob for data access */
    struct tra	*arr_transaction;		/* Parent transaction block */
    struct arr	*arr_next;			/* Next array in transaction */
    struct req	*arr_request;			/* request */
    SLONG	arr_effective_length;		/* Length of array instance */
    USHORT	arr_desc_length;		/* Length of array descriptor */
    struct ads	arr_desc;			/* Array descriptor */
} *ARR;

#endif /* REQUESTER */

#endif /* _JRD_VAL_H_ */
