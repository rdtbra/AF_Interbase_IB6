/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		fun.e
 *	DESCRIPTION:	External Function handling code.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../jrd/gds.h"
#include "../jrd/req.h"
#include "../jrd/lls.h"
#include "../jrd/blb.h"
#include "../jrd/flu.h"
#include "../jrd/common.h"
#include "../jrd/constants.h"
#include "../jrd/ibsetjmp.h"
#ifndef GATEWAY
#include "../jrd/irq.h"
#else
#include "../gway/irq.h"
#endif
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/fun_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/sym_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/isc_s_proto.h"

#ifdef NETWARE_386
#define V4_THREADING
#endif
#include "../jrd/nlm_thd.h"

typedef SSHORT	(*FPTR_SSHORT)();
typedef SLONG	(*FPTR_SLONG)();
typedef SINT64	(*FPTR_SINT64)();
typedef float	(*FPTR_REAL)();
typedef double	(*FPTR_DOUBLE)();
typedef UCHAR    *(*FPTR_UCHAR)();

#ifdef VMS
#define CALL_UDF(ptr)		(*ptr) (args [0], args[1], args[2], args[3], args [4], args [5], args [6], args [7], args [8], args [9])
#else
#define CALL_UDF(ptr)		(ptr) (args [0], args[1], args[2], args[3], args [4], args [5], args [6], args [7], args [8], args [9])
#endif

DATABASE
    DB = FILENAME "ODS.RDB";

#ifdef VMS
extern double	MTH$CVT_D_G(), MTH$CVT_G_D();
#endif

#define EXCEPTION_MESSAGE "The user defined function: \t%s\n\t   referencing entrypoint: \t%s\n\t                in module: \t%s\n\tcaused the fatal exception:"

static SSHORT	blob_get_segment (BLB, UCHAR *, USHORT, USHORT *);
static void	blob_put_segment (BLB, UCHAR *, USHORT);
static SLONG	blob_lseek (BLB, USHORT, SLONG);
static SLONG	get_scalar_array (struct fun_repeat *, DSC *, SAD, LLS *);

void DLL_EXPORT FUN_evaluate (
    FUN		function,
    NOD		node,
    VLU		value)
{
/**************************************
 *
 *	F U N _ e v a l u a t e
 *
 **************************************
 *
 * Functional description
 *	Evaluate a function.
 *
 **************************************/
TDBB		tdbb;
NOD		*ptr;
DSC		*input, temp_desc;
SLONG		**ap, *arg_ptr, args [MAX_UDF_ARGUMENTS+1], *ip;
SSHORT		s, *sp;
SLONG		l, *lp, null_quad [2], *blob_id;
SINT64		i64, *pi64;
USHORT		length, null_flag;
float		f, *fp;
double		*dp, d;
UCHAR		temp [800], *temp_ptr, *p;
STR		string, temp_string;
LLS		blob_stack, array_stack;
REQ		request;
BLB		blob;
BLOB		blob_desc;
struct fun_repeat	*tail, *end, *return_ptr;
JMP_BUF			*old_env, env;
SSHORT          unsup_datatype;

tdbb = GET_THREAD_DATA;

/* Start by constructing argument list */

if (function->fun_temp_length < sizeof (temp)) 
    {
    temp_string = NULL;
    temp_ptr = temp;
    MOVE_CLEAR (temp, sizeof(temp));
    }
else
    {
    /* Need to align the starting address because it consists of
       a number of data blocks with aligned length */

    temp_string = (STR) ALLOCDV (type_str, function->fun_temp_length + DOUBLE_ALIGN - 1);
    MOVE_CLEAR (temp_string->str_data, temp_string->str_length);
    temp_ptr = (UCHAR*) ALIGN ((U_IPTR) temp_string->str_data, DOUBLE_ALIGN);
    }

MOVE_CLEAR (args, sizeof (args));
arg_ptr = args;
blob_stack = array_stack = NULL;
request = tdbb->tdbb_request;
null_flag = request->req_flags & req_null;
return_ptr = function->fun_rpt + function->fun_return_arg;
value->vlu_desc = return_ptr->fun_desc;
value->vlu_desc.dsc_address = (UCHAR*) &value->vlu_misc;
ptr = node->nod_arg;

/* Trap any potential errors */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (temp_string)
	ALL_release (temp_string);
    while (array_stack)
	ALL_free (LLS_POP (&array_stack));
    ERR_punt();
    }

START_CHECK_FOR_EXCEPTIONS(function->fun_exception_message);

/* If the return data type is any of the string types, allocate space to
   hold value */

if (value->vlu_desc.dsc_dtype <= dtype_varying)
    {
    length = value->vlu_desc.dsc_length;
    if ((string = value->vlu_string) && string->str_length < length)
	{
	ALL_release (string);
	string = NULL;
	}
    if (!string)
	{
	string = (STR) ALLOCDV (type_str, length);
	string->str_length = length;
	value->vlu_string = string;
	}
    value->vlu_desc.dsc_address = string->str_data;
    }

/* Process arguments */

for (tail = function->fun_rpt + 1, end = tail + function->fun_count; tail < end; tail++)
    {
    if (tail == return_ptr)
	input = (DSC*) value;
    else
	input = EVL_expr (tdbb, *ptr++);
    ap = (SLONG**) arg_ptr;

    /* If we're passing data type ISC descriptor, there's nothing left to
       be done */

    if (tail->fun_mechanism == FUN_descriptor)
	{
	*ap++ = (SLONG*) input;
	arg_ptr = (SLONG*) ap;
	continue;
	}

    temp_desc = tail->fun_desc;
    temp_desc.dsc_address = temp_ptr;
    length = ALIGN (temp_desc.dsc_length, DOUBLE_ALIGN);
                                                 
    /* If we've got a null argument, just pass zeros (got any better ideas?) */

    if (!input || (request->req_flags & req_null)) 
	{
	if (tail->fun_mechanism == FUN_value)
	    {
	    p = (UCHAR*) arg_ptr;
	    MOVE_CLEAR (p, (SLONG) length);
	    p += length;
	    arg_ptr = (SLONG*) p;
	    continue;	
	    }
	else
	    MOVE_CLEAR (temp_ptr, (SLONG) length);
	}
#ifndef GATEWAY
    else if ((SSHORT) abs(tail->fun_mechanism) == FUN_scalar_array)
	length = get_scalar_array (tail, input, temp_ptr, &array_stack);
#endif
    else switch (tail->fun_desc.dsc_dtype)
	{
	case dtype_short:
	    s = MOV_get_long (input, (SSHORT) tail->fun_desc.dsc_scale);
	    if (tail->fun_mechanism == FUN_value)
		{
		/* For (apparent) portability reasons, SHORT by value
		 * is always passed as a LONG.  See v3.2 release notes
		 * Passing by value is not supported in SQL due to
		 * these problems, but can still occur in GDML.
		 * 1994-September-28 David Schnepper 
		 */
		*arg_ptr++ = (SLONG) s;
		continue;
		}
	    sp = (SSHORT*) temp_ptr;
	    *sp = s;
	    break;

	case dtype_sql_time:
	case dtype_sql_date:
	case dtype_long:
	    /* this check added for bug 74193 */
	    if (tail->fun_desc.dsc_dtype == dtype_long)
	        l = MOV_get_long (input, (SSHORT) tail->fun_desc.dsc_scale);
	    else
		{
		/* for sql_date and sql_time just move the value to a long, 
		   scale does not mean anything for it */
	        l = *((SLONG *) (input->dsc_address)); 
		}

	    if (tail->fun_mechanism == FUN_value)
		{
		*arg_ptr++ = l;
		continue;
		}
	    lp = (SLONG*) temp_ptr;
	    *lp = l;
	    break;

	case dtype_int64:
	    i64 = MOV_get_int64 (input, (SSHORT) tail->fun_desc.dsc_scale);
	    if (tail->fun_mechanism == FUN_value)
		{
	        pi64 = (SINT64*) arg_ptr;
		*pi64++ = i64;
		arg_ptr = (SLONG*) pi64;
		continue;
		}
	    pi64 = (SINT64*) temp_ptr;
	    *pi64 = i64;
	    break;

	case dtype_real:
	    f = (float) MOV_get_double (input);
	    if (tail->fun_mechanism == FUN_value)
		{
		/* For (apparent) portability reasons, FLOAT by value
		 * is always passed as a DOUBLE.  See v3.2 release notes
		 * Passing by value is not supported in SQL due to
		 * these problems, but can still occur in GDML.
		 * 1994-September-28 David Schnepper 
		 */
		dp = (double*) arg_ptr;
		*dp++ = (double) f;
		arg_ptr = (SLONG*) dp;
		continue;
		}
	    fp = (float*) temp_ptr;
	    *fp = f;
	    break;

	case dtype_double:
#ifdef VMS
	case dtype_d_float:
	    d = MOV_get_double (input);
	    if (tail->fun_desc.dsc_dtype == SPECIAL_DOUBLE)
		d = CNVT_FROM_DFLT (&d);
#else
	    d = MOV_get_double (input);
#endif
	    if (tail->fun_mechanism == FUN_value)
		{
		dp = (double*) arg_ptr;
		*dp++ = d;
		arg_ptr = (SLONG*) dp;
		continue;
		}
	    dp = (double*) temp_ptr;
	    *dp = d;
	    break;

	case dtype_text:
	case dtype_cstring:
	case dtype_varying:
	    if (tail == return_ptr)
		{
		temp_ptr = value->vlu_desc.dsc_address;
		length = 0;
		}
	    else
	        MOV_move (input, &temp_desc);
	    break;

	case dtype_timestamp:
	case dtype_quad:
	case dtype_array:
	    MOV_move (input, &temp_desc);
	    break;

	case dtype_blob:
	    blob_desc = (BLOB) temp_ptr;
	    length = sizeof (struct blob);
	    if (tail == return_ptr)
		blob = BLB_create (tdbb, tdbb->tdbb_request->req_transaction, (BID) &value->vlu_misc);
	    else
		{
		if (request->req_flags & req_null)
		    {
		    null_quad [0] = null_quad [1] = 0;
		    blob_id = null_quad;
		    }
		else
		    {
		    if (input->dsc_dtype != dtype_quad &&
			input->dsc_dtype != dtype_blob)
			ERR_post (gds__wish_list, 
			    gds_arg_gds, gds__blobnotsup, 
			    gds_arg_string, "conversion", 0);
		    blob_id = (SLONG*) input->dsc_address;
		    }
		blob = BLB_open (tdbb, tdbb->tdbb_request->req_transaction, blob_id);
		}
	    LLS_PUSH (blob, &blob_stack);
	    blob_desc->blob_get_segment = blob_get_segment;
	    blob_desc->blob_put_segment = blob_put_segment;
	    blob_desc->blob_seek = blob_lseek;
	    blob_desc->blob_handle = (int*) blob;
	    blob_desc->blob_number_segments = blob->blb_count;
	    blob_desc->blob_max_segment = blob->blb_max_segment;
	    blob_desc->blob_total_length = blob->blb_length;
	    break;

	default:
	    assert (FALSE);
	    MOV_move (input, &temp_desc);
	    break;
	
	}
    *ap++ = (SLONG*) temp_ptr;
    arg_ptr = (SLONG*) ap;
    temp_ptr += length;
    }

unsup_datatype = 0;

if (function->fun_return_arg)
    {
    THREAD_EXIT;
        CALL_UDF (function->fun_entrypoint);
    THREAD_ENTER;
    }
else if ((SLONG) return_ptr->fun_mechanism == FUN_value)
    {
    switch (value->vlu_desc.dsc_dtype)
	{
	case dtype_sql_time:
	case dtype_sql_date:
	case dtype_long:
	    THREAD_EXIT;
		value->vlu_misc.vlu_long =
		    CALL_UDF ((FPTR_SLONG) function->fun_entrypoint);
	    THREAD_ENTER;
	    break;

	case dtype_short:
		/* For (apparent) portability reasons, SHORT by value
		 * must always be returned as a LONG.  See v3.2 release notes
		 * 1994-September-28 David Schnepper 
		 */
	    THREAD_EXIT;
	        value->vlu_misc.vlu_short = (SSHORT)
		    CALL_UDF ((FPTR_SLONG) function->fun_entrypoint);
	    THREAD_ENTER;
	    break;
	
	case dtype_real:
		/* For (apparent) portability reasons, FLOAT by value
		 * must always be returned as a DOUBLE.  See v3.2 release notes
		 * 1994-September-28 David Schnepper 
		 */
	    THREAD_EXIT;
		value->vlu_misc.vlu_float = (float)
		    CALL_UDF ((FPTR_DOUBLE) function->fun_entrypoint);
	    THREAD_ENTER;
	    break;
	
	case dtype_int64:
	    THREAD_EXIT;
		value->vlu_misc.vlu_int64 =
		    CALL_UDF ((FPTR_SINT64) function->fun_entrypoint);
	    THREAD_ENTER;
	    break;

	case dtype_double:
#ifdef VMS
	case dtype_d_float:
#endif
	    THREAD_EXIT;
		value->vlu_misc.vlu_double =
		    CALL_UDF ((FPTR_DOUBLE) function->fun_entrypoint);
	    THREAD_ENTER;
	    break;
	
	case dtype_timestamp:
	default:
            unsup_datatype = 1;
	    break;
        }
    }
else
    {
    THREAD_EXIT;
        temp_ptr = CALL_UDF (*(FPTR_UCHAR) function->fun_entrypoint);
    THREAD_ENTER;

    if (temp_ptr != NULL)
	{
	switch (value->vlu_desc.dsc_dtype)
	    {
	    case dtype_sql_date:
	    case dtype_sql_time:
	    case dtype_long:
		value->vlu_misc.vlu_long = *(SLONG*) temp_ptr;
		break;

	    case dtype_short:
		value->vlu_misc.vlu_short = *(SSHORT*) temp_ptr;
		break;
	
	    case dtype_real:
		value->vlu_misc.vlu_float = *(float*) temp_ptr;
		break;
	
	    case dtype_int64:
		value->vlu_misc.vlu_int64 = *(SINT64*) temp_ptr;
		break;

	    case dtype_double:
#ifdef VMS
	    case dtype_d_float:
#endif
		value->vlu_misc.vlu_double = *(double*) temp_ptr;
		break;
	
	    case dtype_text:
	    case dtype_cstring:
	    case dtype_varying:
		temp_desc = value->vlu_desc;
		temp_desc.dsc_address = temp_ptr;
		temp_desc.dsc_length = strlen(temp_ptr) + 1;
		MOV_move (&temp_desc, &value->vlu_desc);
		break;

	    case dtype_timestamp:
		ip = (SLONG*) temp_ptr;
		value->vlu_misc.vlu_dbkey [0] = *ip++;
		value->vlu_misc.vlu_dbkey [1] = *ip;
		break;

	    default:
		unsup_datatype = 1;
		break;
	    }
            /* check if this is function has the FREE_IT set, if set and
               return_value is not null, then free the return value */
            if (((SLONG) return_ptr->fun_mechanism < 0) && temp_ptr)
                free (temp_ptr);
	}
    }

END_CHECK_FOR_EXCEPTIONS(function->fun_exception_message);

if (unsup_datatype)
    IBERROR (169); /* msg 169 return data type not supported */

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (temp_string)
    ALL_release (temp_string);

while (blob_stack)
    BLB_close (tdbb, LLS_POP (&blob_stack));

while (array_stack)
    ALL_free (LLS_POP (&array_stack));

if (temp_ptr == NULL)
    request->req_flags |= req_null;
else
    request->req_flags &= ~req_null;
request->req_flags |= null_flag;
}

void DLL_EXPORT FUN_fini (
    TDBB	tdbb)
{
/**************************************
 *
 *	F U N _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Unregister interest in external function modules.
 *
 **************************************/
DBB	dbb;

dbb = tdbb->tdbb_database;

while (dbb->dbb_modules)
    FLU_unregister_module ((MOD) LLS_POP (&dbb->dbb_modules));
}

void DLL_EXPORT FUN_init (void)
{
/**************************************
 *
 *	F U N _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize external function mechanism.
 *
 **************************************/
}

FUN DLL_EXPORT FUN_lookup_function (
    TEXT	*name)
{
/**************************************
 *
 *	F U N _ l o o k u p _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Lookup function by name.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request_fun, request_arg;
FUN	function, prior;
SYM	symbol;
STR	string;
STR	exception_msg;
TEXT	*p;
MOD	module;
LLS	stack;
USHORT	count, args, l;
ULONG   length;
struct fun_repeat	*tail, temp [MAX_UDF_ARGUMENTS+1];
#ifdef GATEWAY
JMP_BUF			*old_env, env;
#endif

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

/* Start by looking for already defined symbol */

V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_udf);

for (symbol = SYM_lookup (name); symbol; symbol = symbol->sym_homonym)
    if ((int) (symbol->sym_type = SYM_fun))
	{
	V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_udf);
	return (FUN) symbol->sym_object;
	}
    
#ifdef GATEWAY
/* Trap any potential errors */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    return prior;
    }
#endif

prior = NULL;

request_fun = (BLK) CMP_find_request (tdbb, irq_l_functions, IRQ_REQUESTS);
request_arg = (BLK) CMP_find_request (tdbb, irq_l_args, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request_fun) X IN RDB$FUNCTIONS 
	WITH X.RDB$FUNCTION_NAME EQ name
    if (!REQUEST (irq_l_functions))
	REQUEST (irq_l_functions) = request_fun;
    count = args = 0;
    MOVE_CLEAR (temp, (SLONG) sizeof (temp));
    length = 0;
    FOR (REQUEST_HANDLE request_arg) Y IN RDB$FUNCTION_ARGUMENTS 
	    WITH Y.RDB$FUNCTION_NAME EQ X.RDB$FUNCTION_NAME
	    SORTED BY Y.RDB$ARGUMENT_POSITION
	if (!REQUEST (irq_l_args))
	    REQUEST (irq_l_args) = request_arg;
	tail = temp + Y.RDB$ARGUMENT_POSITION;
	tail->fun_mechanism = (FUN_T) Y.RDB$MECHANISM;
	count = MAX (count, Y.RDB$ARGUMENT_POSITION);
	DSC_make_descriptor (&tail->fun_desc, Y.RDB$FIELD_TYPE, 
			Y.RDB$FIELD_SCALE, Y.RDB$FIELD_LENGTH, 
			Y.RDB$FIELD_SUB_TYPE, Y.RDB$CHARACTER_SET_ID, 0);

	if ( tail->fun_desc.dsc_dtype == dtype_cstring)
	    tail->fun_desc.dsc_length++;

	if (Y.RDB$ARGUMENT_POSITION != X.RDB$RETURN_ARGUMENT)
	    ++args;
	l = ALIGN (tail->fun_desc.dsc_length, DOUBLE_ALIGN);
	if (tail->fun_desc.dsc_dtype == dtype_blob)
	    l = sizeof (struct blob);
	length += l;
    END_FOR;
    function = (FUN) ALLOCPV (type_fun, count + 1);
    function->fun_count = count;
    function->fun_args = args;
    function->fun_return_arg = X.RDB$RETURN_ARGUMENT;
    function->fun_type = X.RDB$FUNCTION_TYPE;
    function->fun_temp_length = length;
    MOVE_FAST (temp, function->fun_rpt, (count + 1) * sizeof (struct fun_repeat));
#ifndef	WIN_NT		/* NT allows blanks in file paths */
    for (p = X.RDB$MODULE_NAME; *p && *p != ' '; p++)
	;
    *p = 0;
#endif
    for (p = X.RDB$ENTRYPOINT; *p && *p != ' '; p++)
	;
    *p = 0;
   
    /* Prepare the exception message to be used in case this function ever
       causes an exception.  This is done at this time to save us from preparing
       (thus allocating) this message every time the function is called. */ 
    exception_msg = (STR) ALLOCPV (type_str, strlen (EXCEPTION_MESSAGE) + strlen (name) +
				strlen (X.RDB$ENTRYPOINT) + strlen (X.RDB$MODULE_NAME) + 1 );
    sprintf (exception_msg->str_data, EXCEPTION_MESSAGE, name, X.RDB$ENTRYPOINT, X.RDB$MODULE_NAME);
    function->fun_exception_message = exception_msg;

    function->fun_entrypoint =
	ISC_lookup_entrypoint (X.RDB$MODULE_NAME, X.RDB$ENTRYPOINT, ISC_EXT_LIB_PATH_ENV);

    if (module = FLU_lookup_module (X.RDB$MODULE_NAME))
	{
	/* Register interest in the module by database. */

	for (stack = dbb->dbb_modules; stack; stack = stack->lls_next)
	    if (module == (MOD) stack->lls_object)
		break;

	/* If the module was already registered with this database
	   then decrement the use count that was incremented in
	   ISC_lookup_entrypoint() above. Otherwise push it onto
           the stack of registered modules. */

	if (stack)
	    FLU_unregister_module (module);
	else
	    {
	    PLB		old_pool;

	    old_pool = tdbb->tdbb_default;
	    tdbb->tdbb_default = dbb->dbb_permanent;
	    LLS_PUSH (module, &dbb->dbb_modules);
	    tdbb->tdbb_default = old_pool;
	    }
	}

    /* Could not find a function with given MODULE, ENTRYPOINT,
     * Try the list of internally implemented functions.
     */
    if (!function->fun_entrypoint)
        function->fun_entrypoint =
	    BUILTIN_entrypoint (X.RDB$MODULE_NAME, X.RDB$ENTRYPOINT);

    if (prior)
	{
	function->fun_homonym = prior->fun_homonym;
	prior->fun_homonym = function;
	}
    else
	{
	prior = function;
	function->fun_symbol = symbol = (SYM) ALLOCP (type_sym);
	symbol->sym_object = (BLK) function;
	string = (STR) ALLOCPV (type_str, strlen (name));
	strcpy (string->str_data, name);
	symbol->sym_string = (TEXT*) string->str_data;
	symbol->sym_type = SYM_fun;
	SYM_insert (symbol);
	}
END_FOR;

if (!REQUEST (irq_l_functions))
    REQUEST (irq_l_functions) = request_fun;
if (!REQUEST (irq_l_args))
    REQUEST (irq_l_args) = request_arg;

V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_udf);

#ifdef GATEWAY
tdbb->tdbb_setjmp = (UCHAR*) old_env;
#endif

return prior;
}

FUN DLL_EXPORT FUN_resolve (
    CSB		csb,
    FUN		function,
    NOD		args)
{
/**************************************
 *
 *	F U N _ r e s o l v e
 *
 **************************************
 *
 * Functional description
 *	Resolve instance of potentially overloaded function.
 *
 **************************************/
FUN		best;
NOD		*ptr, *end;
DSC		arg;
int		best_score, score;
struct fun_repeat	*tail;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;

best = NULL;
best_score = 0;
end = args->nod_arg + args->nod_count;

for (; function; function = function->fun_homonym)
    if (function->fun_entrypoint &&
	function->fun_args == args->nod_count)
	{
	score = 0;
	for (ptr = args->nod_arg, tail = function->fun_rpt + 1; ptr < end; ptr++, tail++)
	    {
	    CMP_get_desc (tdbb, csb, *ptr, &arg);
	    if ((SSHORT) abs (tail->fun_mechanism) == FUN_descriptor)
		score += 10;
	    else if (tail->fun_desc.dsc_dtype == dtype_blob ||
		     arg.dsc_dtype == dtype_blob)
		{
		score = 0;
		break;
		}
	    else if (tail->fun_desc.dsc_dtype >= arg.dsc_dtype)
		score += 10 - (arg.dsc_dtype - tail->fun_desc.dsc_dtype);
	    else
		score += 1;
	    }
	if (!best || score > best_score)
	    {
	    best_score = score;
	    best = function;
	    }
	}

return best;
}

static SLONG blob_lseek (
    BLB		blob,
    USHORT	mode,
    SLONG	offset)
{
/**************************************
 *
 *	b l o b _ l s e e k
 *
 **************************************
 *
 * Functional description
 *	lseek a  a blob segement.  Return the offset 
 *
 **************************************/

SLONG return_offset;

/* add thread enter and thread_exit wrappers */
THREAD_ENTER;
return_offset = BLB_lseek (blob, mode, offset);
THREAD_EXIT;
return (return_offset);
}

static void blob_put_segment (
    BLB		blob,
    UCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	b l o b _ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Put  segment into a blob.  Return nothing 
 *
 **************************************/
TDBB	tdbb;

/* As this is a call-back from a UDF, must reacquire the
   engine mutex */

THREAD_ENTER;
tdbb = GET_THREAD_DATA;
BLB_put_segment (tdbb, blob, buffer, length);
THREAD_EXIT;
}

static SSHORT blob_get_segment (
    BLB		blob,
    UCHAR	*buffer,
    USHORT	length,
    USHORT	*return_length)
{
/**************************************
 *
 *	b l o b _ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Get next segment of a blob.  Return the following:
 *
 *		1	-- Complete segment has been returned.
 *		0	-- End of blob (no data returned).
 *		-1	-- Current segment is incomplete.
 *
 **************************************/
TDBB	tdbb;

/* add thread enter and thread_exit wrappers */
THREAD_ENTER;
tdbb = GET_THREAD_DATA;
*return_length = BLB_get_segment (tdbb, blob, buffer, length);
THREAD_EXIT;

if (blob->blb_flags & BLB_eof)
    return 0;

if (blob->blb_fragment_size)
    return -1;

return 1;
}

#ifndef GATEWAY
static SLONG get_scalar_array (
    struct fun_repeat	*arg,
    DSC		*value,
    SAD		scalar_desc,
    LLS		*stack)
{
/**************************************
 *
 *	g e t _ s c a l a r _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	Get and format a scalar array descriptor, then allocate space
 *	and fetch the array.  If conversion is required, convert.
 *	Return length of array desc.
 *
 **************************************/
TDBB	tdbb;
BLB	blob;
ADS	array_desc;
SLONG	stuff [ADS_LEN (16) / 4], n;
UCHAR	*data, *temp;
DSC	from, to;
USHORT	dimensions;
struct ads_repeat	*tail1;
struct sad_repeat	*tail2, *end;

tdbb = GET_THREAD_DATA;

/* Get first the array descriptor, then the array */

array_desc = (ADS) stuff;
blob = BLB_get_array (tdbb, tdbb->tdbb_request->req_transaction, value->dsc_address, array_desc);
data = (UCHAR *) ALL_malloc (array_desc->ads_total_length, ERR_jmp);
BLB_get_data (tdbb, blob, data, array_desc->ads_total_length);
dimensions = array_desc->ads_dimensions;

/* Convert array, if necessary */

to = arg->fun_desc;
from = array_desc->ads_rpt[0].ads_desc;

if (to.dsc_dtype != from.dsc_dtype ||
    to.dsc_scale != from.dsc_scale ||
    to.dsc_length != from.dsc_length)
    {
    n = array_desc->ads_count;
    to.dsc_address = temp = (UCHAR *) ALL_malloc ((SLONG) to.dsc_length * n, ERR_jmp);
    from.dsc_address = data;
    for (; n; --n, to.dsc_address += to.dsc_length, 
                   from.dsc_address += array_desc->ads_element_length)
	MOV_move (&from, &to);
    ALL_free (data);
    data = temp;
    }

/* Fill out the scalar array descriptor */

LLS_PUSH (data, stack);
scalar_desc->sad_desc = arg->fun_desc;
scalar_desc->sad_desc.dsc_address = data;
scalar_desc->sad_dimensions = dimensions;

for (tail2 = scalar_desc->sad_rpt, end = tail2 + dimensions, tail1 = array_desc->ads_rpt;
     tail2 < end; ++tail1, ++tail2)
    {
    tail2->sad_upper = tail1->ads_upper;
    tail2->sad_lower = tail1->ads_lower;
    }

return sizeof (struct sad) + (dimensions - 1) * sizeof (struct sad_repeat);
}
#endif
