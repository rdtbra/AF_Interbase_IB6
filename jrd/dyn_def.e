/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn_define.e
 *	DESCRIPTION:	Dynamic data definition DYN_define_<x>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../jrd/ibsetjmp.h"
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/constants.h" 
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/tra.h"
#include "../jrd/scl.h"
#include "../jrd/drq.h"
#include "../jrd/req.h"
#include "../jrd/flags.h"
#include "../jrd/gds.h"
#include "../jrd/lls.h"
#include "../jrd/all.h"
#include "../jrd/met.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/dyn.h"
#include "../jrd/gdsassert.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/dyn_df_proto.h"
#include "../jrd/dyn_ut_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/gdsassert.h"

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

#ifdef SUPERSERVER
#define V4_THREADING
#endif
#include "../jrd/nlm_thd.h"

#define FOR_KEY_UPD_CASCADE  0x01
#define FOR_KEY_UPD_NULL     0x02
#define FOR_KEY_UPD_DEFAULT  0x04
#define FOR_KEY_UPD_NONE     0x08
#define FOR_KEY_DEL_CASCADE  0x10
#define FOR_KEY_DEL_NULL     0x20
#define FOR_KEY_DEL_DEFAULT  0x40
#define FOR_KEY_DEL_NONE     0x80

DATABASE
    DB = STATIC "yachts.gdb";

static CONST SCHAR
      who_blr [] = {
        blr_version5,
        blr_begin, 
          blr_message, 0, 1,0, 
            blr_cstring, 32,0, 
          blr_begin, 
            blr_send, 0, 
              blr_begin, 
                blr_assignment, 
                  blr_user_name, 
                  blr_parameter, 0, 0,0, 
              blr_end, 
          blr_end, 
        blr_end, 
        blr_eoc
        };

static void	check_unique_name		(TDBB, GBL,
							TEXT *, BOOLEAN);
static BOOLEAN	find_field_source		(TDBB, GBL,
							TEXT *, USHORT,
							TEXT *, TEXT *);
static BOOLEAN	get_who				(TDBB, GBL,
							SCHAR *);
static BOOLEAN	is_it_user_name     (GBL, TEXT *, TDBB);

void DYN_define_cache (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Define a database cache file.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
USHORT		  found = FALSE;
VOLATILE SSHORT	  id;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;
request = NULL;
old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
id = -1;

if (SETJMP (env))
    {
    if (id == drq_s_cache)
	{
	DYN_rundown_request (old_env, request, drq_s_cache);
	DYN_error_punt (TRUE, 150, NULL, NULL, NULL, NULL, NULL);
	/* msg 150: STORE RDB$FILES failed */
	}
    else
	{
	DYN_rundown_request (old_env, request, drq_l_cache);
	DYN_error_punt (TRUE, 156, NULL, NULL, NULL, NULL, NULL);
	/* msg 156: Shared cache lookup failed */
	}
    }
id = drq_l_cache;
request = (BLK) CMP_find_request (tdbb, drq_l_cache, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FIL IN RDB$FILES WITH FIL.RDB$FILE_FLAGS EQ FILE_cache
    found = TRUE;
END_FOR;

if (!DYN_REQUEST (drq_l_cache))
    DYN_REQUEST (drq_l_cache) = request;

if (found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 148, NULL, NULL, NULL, NULL, NULL);
    /* msg 148: "Shared cache file already exists" */
    }

request = (BLK) CMP_find_request (tdbb, drq_s_cache, DYN_REQUESTS);
id = drq_s_cache;
STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FILES

    GET_STRING (ptr, X.RDB$FILE_NAME);
    X.RDB$FILE_FLAGS = FILE_cache;
    X.RDB$FILE_FLAGS.NULL = FALSE;
    X.RDB$FILE_START = 0;
    X.RDB$FILE_START.NULL = FALSE;
    X.RDB$FILE_LENGTH.NULL = TRUE;

    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_file_length:
		X.RDB$FILE_LENGTH = DYN_get_number (ptr);
		X.RDB$FILE_LENGTH.NULL = FALSE;
		break;

	    default:
		DYN_unsupported_verb();
	    }

END_STORE;

if (!DYN_REQUEST (drq_s_cache))
    DYN_REQUEST (drq_s_cache) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_constraint (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ c o n s t r a i n t 
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	creates an integrity constraint and if not a CHECK 
 *	constraint, also an index for the constraint.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
BLK		  old_request;
VOLATILE SSHORT   id, old_id;
UCHAR		  verb;
TEXT		  constraint_name [32], index_name [32], 
		  referred_index_name [32];
TEXT		  null_field_name [32], trigger_name [32];
int		  all_count, unique_count;
LLS		  field_list = NULL, list_ptr;
STR		  str;
USHORT		  found, foreign_flag = FALSE, not_null;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
UCHAR   	  ri_action = 0;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

if (!GET_STRING (ptr, constraint_name))
    DYN_UTIL_generate_constraint_name (tdbb, gbl, constraint_name);

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_rel_con)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 121, NULL, NULL, NULL, NULL, NULL);
	/* msg 121: "STORE RDB$RELATION_CONSTRAINTS failed" */
	}
    else if (id == drq_s_ref_con)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 127, NULL, NULL, NULL, NULL, NULL);
	/* msg 127: "STORE RDB$REF_CONSTRAINTS failed" */
	}

    DYN_rundown_request (old_env, request, -1);
    if (id == drq_c_unq_nam)
	DYN_error_punt (TRUE, 121, NULL, NULL, NULL, NULL, NULL);
	/* msg 121: "STORE RDB$RELATION_CONSTRAINTS failed" */
    else if (id == drq_n_idx_seg)
	DYN_error_punt (TRUE, 124, constraint_name, NULL, NULL, NULL, NULL);
	/* msg 124: "A column name is repeated in the definition of constraint: %s" */
    else if (id == drq_c_dup_con)
	DYN_error_punt (TRUE, 125, NULL, NULL, NULL, NULL, NULL);
	/* msg 125: "Integrity constraint lookup failed" */
    else
	DYN_error_punt (TRUE, 125, NULL, NULL, NULL, NULL, NULL);
	/* msg 125: "Integrity constraint lookup failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_s_rel_con, DYN_REQUESTS);
id = drq_s_rel_con;

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	CRT IN RDB$RELATION_CONSTRAINTS

    strcpy (CRT.RDB$CONSTRAINT_NAME, constraint_name);
    strcpy (CRT.RDB$RELATION_NAME, relation_name);

    switch (verb = *(*ptr)++)
	{
	case gds__dyn_def_primary_key:
	    strcpy (CRT.RDB$CONSTRAINT_TYPE, PRIMARY_KEY);
	    break;

	case gds__dyn_def_foreign_key:
	    foreign_flag = TRUE;
	    strcpy (CRT.RDB$CONSTRAINT_TYPE, FOREIGN_KEY);
	    DYN_terminate (CRT.RDB$CONSTRAINT_NAME, sizeof (CRT.RDB$CONSTRAINT_NAME));
	    break;
    
	case gds__dyn_def_unique:
	    strcpy (CRT.RDB$CONSTRAINT_TYPE, UNIQUE_CNSTRT);
	    break;

	case gds__dyn_def_trigger:
	    strcpy (CRT.RDB$CONSTRAINT_TYPE, CHECK_CNSTRT);
	    CRT.RDB$INDEX_NAME.NULL = TRUE;
	    break;

	case gds__dyn_fld_not_null:
	    strcpy (CRT.RDB$CONSTRAINT_TYPE, NOT_NULL_CNSTRT);
	    CRT.RDB$INDEX_NAME.NULL = TRUE;
	    break;

	default:
	    DYN_unsupported_verb();
	}

    if (verb != gds__dyn_def_trigger && verb != gds__dyn_fld_not_null)
	{
	referred_index_name [0] = 0;
	DYN_define_index (gbl, ptr, relation_name, verb, index_name, referred_index_name, constraint_name, &ri_action);
	strcpy (CRT.RDB$INDEX_NAME, index_name);
	CRT.RDB$INDEX_NAME.NULL = FALSE;

	/* check that we have references permissions on the table and
           fields that the index:referred_index_name is on. */
	   
	SCL_check_index (tdbb, referred_index_name, SCL_sql_references);
	}
END_STORE;

if (!DYN_REQUEST (drq_s_rel_con))
    DYN_REQUEST (drq_s_rel_con) = request;

if (verb == gds__dyn_def_trigger) 
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    do {
	DYN_define_trigger (gbl, ptr, relation_name, trigger_name, FALSE);
	DYN_UTIL_store_check_constraints (tdbb, gbl, constraint_name, trigger_name);
	} while ((verb = *(*ptr)++) == gds__dyn_def_trigger);

    if (verb != gds__dyn_end)
	DYN_unsupported_verb();
    return;
    }

if (verb == gds__dyn_fld_not_null)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_UTIL_store_check_constraints (tdbb, gbl, constraint_name, field_name);

    if (*(*ptr)++ != gds__dyn_end) 
	DYN_unsupported_verb();
    return;
    }

/* Make sure unique field names were specified for UNIQUE/PRIMARY/FOREIGN */
/* All fields must have the NOT NULL attribute specified for UNIQUE/PRIMARY. */

request = (BLK) CMP_find_request (tdbb, drq_c_unq_nam, DYN_REQUESTS);
id = drq_c_unq_nam;

not_null = TRUE;
all_count = unique_count = 0;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    IDS IN RDB$INDEX_SEGMENTS
    CROSS RFR IN RDB$RELATION_FIELDS
    CROSS FLX IN RDB$FIELDS WITH
    IDS.RDB$INDEX_NAME EQ index_name AND 
    RFR.RDB$RELATION_NAME EQ relation_name  AND 
    RFR.RDB$FIELD_NAME EQ IDS.RDB$FIELD_NAME  AND
    FLX.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE  
    REDUCED TO IDS.RDB$FIELD_NAME, IDS.RDB$INDEX_NAME, FLX.RDB$NULL_FLAG 
    SORTED BY ASCENDING IDS.RDB$FIELD_NAME

    if (!DYN_REQUEST (drq_c_unq_nam))
	DYN_REQUEST (drq_c_unq_nam) = request;

    if ((FLX.RDB$NULL_FLAG.NULL || !FLX.RDB$NULL_FLAG) && 
	(RFR.RDB$NULL_FLAG.NULL || !RFR.RDB$NULL_FLAG) &&
	(foreign_flag == FALSE))
	{
	not_null = FALSE;
	DYN_terminate (RFR.RDB$FIELD_NAME, sizeof (RFR.RDB$FIELD_NAME));
	strcpy (null_field_name, RFR.RDB$FIELD_NAME);
	EXE_unwind (tdbb, request);
	break;
	}

    unique_count++;
    str = (STR) ALLOCDV (type_str, sizeof (IDS.RDB$FIELD_NAME) - 1);
    strcpy (str->str_data, IDS.RDB$FIELD_NAME);
    LLS_PUSH (str, &field_list);
END_FOR;
if (!DYN_REQUEST (drq_c_unq_nam))
    DYN_REQUEST (drq_c_unq_nam) = request;

if (not_null == FALSE)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error (FALSE, 123, null_field_name, NULL, NULL, NULL, NULL);
    /* msg 123: "Field: %s not defined as NOT NULL - can't be used in PRIMARY KEY/UNIQUE constraint definition" */
    ERR_punt();
    }

request = (BLK) CMP_find_request (tdbb, drq_n_idx_seg, DYN_REQUESTS);
id = drq_n_idx_seg;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    IDS IN RDB$INDEX_SEGMENTS WITH IDS.RDB$INDEX_NAME EQ index_name
    if (!DYN_REQUEST (drq_n_idx_seg))
	DYN_REQUEST (drq_n_idx_seg) = request;

    all_count++;
END_FOR;
if (!DYN_REQUEST (drq_n_idx_seg))
    DYN_REQUEST (drq_n_idx_seg) = request;

if (unique_count != all_count)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error (FALSE, 124, constraint_name, NULL, NULL, NULL, NULL);
    /* msg 124: "A column name is repeated in the definition of constraint: %s" */
    ERR_punt();
    }

/* For PRIMARY KEY/UNIQUE constraints, make sure same set of columns 
   is not used in another constraint of either type */

if (foreign_flag == FALSE)
    {
    request = (BLK) CMP_find_request (tdbb, drq_c_dup_con, DYN_REQUESTS);
    id = drq_c_dup_con;

    index_name [0] = 0;
    list_ptr = NULL;
    found = FALSE;
    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	CRT IN RDB$RELATION_CONSTRAINTS CROSS
	IDS IN RDB$INDEX_SEGMENTS OVER RDB$INDEX_NAME 
	WITH CRT.RDB$RELATION_NAME EQ relation_name AND
	(CRT.RDB$CONSTRAINT_TYPE EQ PRIMARY_KEY OR
	 CRT.RDB$CONSTRAINT_TYPE EQ UNIQUE_CNSTRT) AND
	CRT.RDB$CONSTRAINT_NAME NE constraint_name
	SORTED BY CRT.RDB$INDEX_NAME, DESCENDING IDS.RDB$FIELD_NAME 

	if (!DYN_REQUEST (drq_c_dup_con))
	    DYN_REQUEST (drq_c_dup_con) = request;

	DYN_terminate (CRT.RDB$INDEX_NAME, sizeof (CRT.RDB$INDEX_NAME));
	if (strcmp (index_name, CRT.RDB$INDEX_NAME))
	    {
	    if (list_ptr)
		found = FALSE;
	    if (found)
		break;
	    list_ptr = field_list;
	    strcpy (index_name, CRT.RDB$INDEX_NAME);
	    found = TRUE;
	    } 

	if (list_ptr)
	    {
	    if (strcmp (((STR) list_ptr->lls_object)->str_data, IDS.RDB$FIELD_NAME))
		found = FALSE;
	    list_ptr = list_ptr->lls_next;
	    }
	else
	    found = FALSE;
    END_FOR;
    if (!DYN_REQUEST (drq_c_dup_con))
	DYN_REQUEST (drq_c_dup_con) = request;

    if (list_ptr)
	found = FALSE;

    if (found)
	{
	tdbb->tdbb_setjmp = (UCHAR*) old_env;
	DYN_error (FALSE, 126, NULL, NULL, NULL, NULL, NULL);
	/* msg 126: "Same set of columns cannot be used in more than one PRIMARY KEY and/or UNIQUE constraint definition" */
	ERR_punt();
	}
    }
else  /* Foreign key being defined   */
    {
    request = (BLK) CMP_find_request (tdbb, drq_s_ref_con, DYN_REQUESTS);
    id = drq_s_ref_con;

    STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    REF IN RDB$REF_CONSTRAINTS

	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_l_intg_con, DYN_REQUESTS);
	id = drq_l_intg_con;

	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    CRT IN  RDB$RELATION_CONSTRAINTS WITH
	    CRT.RDB$INDEX_NAME EQ referred_index_name AND
	    (CRT.RDB$CONSTRAINT_TYPE = PRIMARY_KEY OR
	     CRT.RDB$CONSTRAINT_TYPE = UNIQUE_CNSTRT)

	    if (!DYN_REQUEST (drq_l_intg_con))
		DYN_REQUEST (drq_l_intg_con) = request;

	    DYN_terminate (CRT.RDB$CONSTRAINT_NAME, sizeof (CRT.RDB$CONSTRAINT_NAME));
	    strcpy (REF.RDB$CONST_NAME_UQ, CRT.RDB$CONSTRAINT_NAME);
	    strcpy (REF.RDB$CONSTRAINT_NAME, constraint_name);

	    REF.RDB$UPDATE_RULE.NULL = FALSE;
	    if (ri_action & FOR_KEY_UPD_CASCADE)
	       strcpy (REF.RDB$UPDATE_RULE, RI_ACTION_CASCADE);
	    else if (ri_action & FOR_KEY_UPD_NULL)
	       strcpy (REF.RDB$UPDATE_RULE, RI_ACTION_NULL);
	    else if (ri_action & FOR_KEY_UPD_DEFAULT)
	       strcpy (REF.RDB$UPDATE_RULE, RI_ACTION_DEFAULT);
	    else if (ri_action & FOR_KEY_UPD_NONE)
	       strcpy (REF.RDB$UPDATE_RULE, RI_ACTION_NONE);
	    else
	       /* RESTRICT is the default value for this column */
	       strcpy (REF.RDB$UPDATE_RULE, RI_RESTRICT);
	    

	    REF.RDB$DELETE_RULE.NULL = FALSE;
	    if (ri_action & FOR_KEY_DEL_CASCADE)
	       strcpy (REF.RDB$DELETE_RULE, RI_ACTION_CASCADE);
	    else if (ri_action & FOR_KEY_DEL_NULL)
	       strcpy (REF.RDB$DELETE_RULE, RI_ACTION_NULL);
	    else if (ri_action & FOR_KEY_DEL_DEFAULT)
	       strcpy (REF.RDB$DELETE_RULE, RI_ACTION_DEFAULT);
	    else if (ri_action & FOR_KEY_DEL_NONE)
	       strcpy (REF.RDB$DELETE_RULE, RI_ACTION_NONE);
	    else 
	       /* RESTRICT is the default value for this column */
	       strcpy (REF.RDB$DELETE_RULE, RI_RESTRICT);

	END_FOR;
	if (!DYN_REQUEST (drq_l_intg_con))
	    DYN_REQUEST (drq_l_intg_con) = request;
	request = old_request;
	id = old_id;
    END_STORE;

    if (!DYN_REQUEST (drq_s_ref_con))
	DYN_REQUEST (drq_s_ref_con) = request;
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_dimension (
    GBL		gbl,
    UCHAR	**ptr, 
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ d i m e n s i o n 
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	defines a single dimension for a field.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_dims, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	DIM IN RDB$FIELD_DIMENSIONS

    DIM.RDB$UPPER_BOUND.NULL = TRUE;
    DIM.RDB$LOWER_BOUND.NULL = TRUE;
    DIM.RDB$DIMENSION = DYN_get_number (ptr);
    if (field_name)
	strcpy (DIM.RDB$FIELD_NAME, field_name);

    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_fld_name:
		GET_STRING (ptr, DIM.RDB$FIELD_NAME);
		break;

	    case gds__dyn_dim_upper:
		DIM.RDB$UPPER_BOUND = DYN_get_number (ptr);
		DIM.RDB$UPPER_BOUND.NULL = FALSE;
		break;

	    case gds__dyn_dim_lower:
		DIM.RDB$LOWER_BOUND = DYN_get_number (ptr);
		DIM.RDB$LOWER_BOUND.NULL = FALSE;
		break;

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, relation_name, field_name,
			NULL_PTR, NULL_PTR, NULL_PTR);
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_dims);
	DYN_error_punt (TRUE, 3, NULL, NULL, NULL, NULL, NULL);
	/* msg 3: "STORE RDB$FIELD_DIMENSIONS failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_dims))
    DYN_REQUEST (drq_s_dims) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_exception (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	Define an exception.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb; 
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_xcp, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$EXCEPTIONS
    GET_STRING (ptr, X.RDB$EXCEPTION_NAME);
    X.RDB$MESSAGE.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_xcp_msg:
		GET_STRING_2 (ptr, X.RDB$MESSAGE);
		X.RDB$MESSAGE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_xcp_msg2:
		DYN_get_string2 (ptr, X.RDB$MESSAGE, sizeof(X.RDB$MESSAGE));
		X.RDB$MESSAGE.NULL = FALSE;
		break;
#endif

	    default:
		DYN_unsupported_verb();
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_trg_msgs);
	DYN_error_punt (TRUE, 142, NULL, NULL, NULL, NULL, NULL);
	/* msg 142: "DEFINE EXCEPTION failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_xcp))
    DYN_REQUEST (drq_s_xcp) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_file (
    GBL		gbl,
    UCHAR	**ptr,
    SLONG	shadow_number,
    SLONG	*start,
    USHORT	msg)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Define a database or shadow file.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
SLONG		  temp;
USHORT		  man_auto;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
TEXT		  temp_f1 [MAXPATHLEN], temp_f [MAXPATHLEN];
VOLATILE SSHORT   id;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = NULL;
old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
id = -1;

if (SETJMP (env))
    {
    if (id == drq_l_files)
	{
	DYN_rundown_request (old_env, request, drq_l_files);
	DYN_error_punt (FALSE, 166, NULL, NULL, NULL, NULL, NULL);
	}
    else
	{
	DYN_rundown_request (old_env, request, drq_s_files);
	DYN_error_punt (TRUE, msg, NULL, NULL, NULL, NULL, NULL);
	}
    }

request = (BLK) CMP_find_request (tdbb, id = drq_l_files, DYN_REQUESTS);

GET_STRING (ptr, temp_f1);
ISC_expand_filename (temp_f1, 0, temp_f);
if (!strcmp (dbb->dbb_filename->str_data, temp_f))
    DYN_error_punt (FALSE, 166, NULL, NULL, NULL, NULL, NULL);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	 FIRST 1 X IN RDB$FILES WITH X.RDB$FILE_NAME EQ temp_f
    DYN_error_punt (FALSE, 166, NULL, NULL, NULL, NULL, NULL);
END_FOR;

request = (BLK) CMP_find_request (tdbb, id = drq_s_files, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FILES

    strcpy (X.RDB$FILE_NAME, temp_f);
    X.RDB$SHADOW_NUMBER = shadow_number;
    X.RDB$FILE_FLAGS = 0;
    X.RDB$FILE_FLAGS.NULL = FALSE;
    X.RDB$FILE_START.NULL = TRUE;
    X.RDB$FILE_LENGTH.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_file_start:
		temp = DYN_get_number (ptr);
		*start = MAX (*start, temp);
		X.RDB$FILE_START = *start;
		X.RDB$FILE_START.NULL = FALSE;
		break;
	    
	    case gds__dyn_file_length:
		X.RDB$FILE_LENGTH = DYN_get_number (ptr);
		X.RDB$FILE_LENGTH.NULL = FALSE;
		break;
	    
	    case gds__dyn_shadow_man_auto:
		man_auto = DYN_get_number (ptr);
		if (man_auto)
		    X.RDB$FILE_FLAGS |= FILE_manual;
		break;

 	    case gds__dyn_shadow_conditional:
 		if (DYN_get_number (ptr))
 		    X.RDB$FILE_FLAGS |= FILE_conditional;
 		break;
	    
	    default:
		DYN_unsupported_verb();
	    }
    *start += X.RDB$FILE_LENGTH;
END_STORE;

if (!DYN_REQUEST (drq_s_files))
    DYN_REQUEST (drq_s_files) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_filter (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Define a blob filter.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_filters, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FILTERS USING

    GET_STRING (ptr, X.RDB$FUNCTION_NAME);
    X.RDB$OUTPUT_SUB_TYPE.NULL = TRUE;
    X.RDB$INPUT_SUB_TYPE.NULL = TRUE;
    X.RDB$MODULE_NAME.NULL = TRUE;
    X.RDB$ENTRYPOINT.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_filter_in_subtype:
		X.RDB$INPUT_SUB_TYPE = DYN_get_number (ptr);
		X.RDB$INPUT_SUB_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_filter_out_subtype:
		X.RDB$OUTPUT_SUB_TYPE = DYN_get_number (ptr);
		X.RDB$OUTPUT_SUB_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_func_module_name:
		GET_STRING (ptr, X.RDB$MODULE_NAME);
		X.RDB$MODULE_NAME.NULL = FALSE;
		break;

	    case gds__dyn_func_entry_point:
		GET_STRING (ptr, X.RDB$ENTRYPOINT);
		X.RDB$ENTRYPOINT.NULL = FALSE;
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    default:
		DYN_unsupported_verb();
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_filters);
	DYN_error_punt (TRUE, 7, NULL, NULL, NULL, NULL, NULL);
	/* msg 7: "DEFINE BLOB FILTER failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_filters))
    DYN_REQUEST (drq_s_filters) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_function (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Define a user defined function.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
JMP_BUF		  env;
VOLATILE JMP_BUF *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_funcs, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FUNCTIONS USING
    GET_STRING (ptr, X.RDB$FUNCTION_NAME);
    X.RDB$RETURN_ARGUMENT.NULL = TRUE;
    X.RDB$QUERY_NAME.NULL = TRUE;
    X.RDB$MODULE_NAME.NULL = TRUE;
    X.RDB$ENTRYPOINT.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_func_return_argument:
		X.RDB$RETURN_ARGUMENT = DYN_get_number (ptr);
		X.RDB$RETURN_ARGUMENT.NULL = FALSE;
		if (X.RDB$RETURN_ARGUMENT > MAX_UDF_ARGUMENTS)
		    DYN_error_punt (TRUE, 10, NULL, NULL, NULL, NULL, NULL);
		    /* msg 10: "DEFINE FUNCTION failed" */
		break;

	    case gds__dyn_func_module_name:
		GET_STRING (ptr, X.RDB$MODULE_NAME);
		X.RDB$MODULE_NAME.NULL = FALSE;
		break;

	    case gds__dyn_fld_query_name:
		GET_STRING (ptr, X.RDB$QUERY_NAME);
		X.RDB$QUERY_NAME.NULL = FALSE;
		break;

	    case gds__dyn_func_entry_point:
		GET_STRING (ptr, X.RDB$ENTRYPOINT);
		X.RDB$ENTRYPOINT.NULL = FALSE;
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, NULL_PTR, NULL_PTR,
			NULL_PTR, X.RDB$FUNCTION_NAME, NULL_PTR);
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_funcs);
	DYN_error_punt (TRUE, 10, NULL, NULL, NULL, NULL, NULL);
	/* msg 10: "DEFINE FUNCTION failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_funcs))
    DYN_REQUEST (drq_s_funcs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_function_arg (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*function_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ f u n c t i o n _ a r g
 *
 **************************************
 *
 * Functional description
 *	Define a user defined function argument.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request = NULL;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (request)
	DYN_rundown_request (old_env, request, drq_s_func_args);
    DYN_error_punt (TRUE, 12, NULL, NULL, NULL, NULL, NULL);
    /* msg 12: "DEFINE FUNCTION ARGUMENT failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_s_func_args, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FUNCTION_ARGUMENTS
    X.RDB$ARGUMENT_POSITION = DYN_get_number (ptr);

    if (X.RDB$ARGUMENT_POSITION > MAX_UDF_ARGUMENTS)
	DYN_error_punt (TRUE, 12, NULL, NULL, NULL, NULL, NULL);
	/* msg 12: "DEFINE FUNCTION ARGUMENT failed" */

    if (function_name)
	{
	strcpy (X.RDB$FUNCTION_NAME, function_name);
	X.RDB$FUNCTION_NAME.NULL = FALSE;
	}
    else
	X.RDB$FUNCTION_NAME.NULL = TRUE;
    X.RDB$MECHANISM.NULL = TRUE;
    X.RDB$FIELD_TYPE.NULL = TRUE;
    X.RDB$FIELD_SCALE.NULL = TRUE;
    X.RDB$FIELD_LENGTH.NULL = TRUE;
    X.RDB$FIELD_SUB_TYPE.NULL = TRUE;
    X.RDB$CHARACTER_SET_ID.NULL = TRUE;
    X.RDB$FIELD_PRECISION.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_function_name:
		GET_STRING (ptr, X.RDB$FUNCTION_NAME);
		X.RDB$FUNCTION_NAME.NULL = FALSE;
		break;

	    case gds__dyn_func_mechanism:
		X.RDB$MECHANISM = DYN_get_number (ptr);
		X.RDB$MECHANISM.NULL = FALSE;
		break;

	    case gds__dyn_fld_type:
		X.RDB$FIELD_TYPE = DYN_get_number (ptr);
		X.RDB$FIELD_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_fld_sub_type:
		X.RDB$FIELD_SUB_TYPE = DYN_get_number (ptr);
		X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_fld_scale:
		X.RDB$FIELD_SCALE = DYN_get_number (ptr);
		X.RDB$FIELD_SCALE.NULL = FALSE;
		break;

	    case gds__dyn_fld_length:
		X.RDB$FIELD_LENGTH = DYN_get_number (ptr);
		X.RDB$FIELD_LENGTH.NULL = FALSE;
		break;

	    case gds__dyn_fld_character_set:
		X.RDB$CHARACTER_SET_ID = DYN_get_number (ptr);
		X.RDB$CHARACTER_SET_ID.NULL = FALSE;
		break;

	    case isc_dyn_fld_precision:
		X.RDB$FIELD_PRECISION = DYN_get_number (ptr);
		X.RDB$FIELD_PRECISION.NULL = FALSE;
		break;

	    /* Ignore the field character length as the system UDF parameter
	       table has no place to store the information */
	    case gds__dyn_fld_char_length:
		(void) DYN_get_number (ptr);
		break;

	    default:
		DYN_unsupported_verb();
	    }

END_STORE;

if (!DYN_REQUEST (drq_s_func_args))
    DYN_REQUEST (drq_s_func_args) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_generator (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Define a generator.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_gens, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$GENERATORS
    GET_STRING (ptr, X.RDB$GENERATOR_NAME);

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_gens);
	DYN_error_punt (TRUE, 8, NULL, NULL, NULL, NULL, NULL);
	/* msg 8: "DEFINE GENERATOR failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_gens))
    DYN_REQUEST (drq_s_gens) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (*(*ptr)++ != gds__dyn_end)
    DYN_error_punt (TRUE, 9, NULL, NULL, NULL, NULL, NULL);
    /* msg 9: "DEFINE GENERATOR unexpected dyn verb" */
}

void DYN_define_role (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ r o l e
 *
 **************************************
 *
 * Functional description
 *
 *     Define a SQL role.
 *     ROLES cannot be named the same as any existing user name 
 *
 **************************************/
TDBB          tdbb;
DBB           dbb;
VOLATILE BLK  request = NULL;
JMP_BUF		env;
JMP_BUF* VOLATILE old_env;
TEXT          dummy_name [32], owner_name [32], role_name [32], *p;
USHORT        major_version, minor_original;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

if (ENCODE_ODS(major_version, minor_original) < (unsigned) ODS_9_0)
    {
    DYN_error (FALSE, 196, NULL, NULL, NULL, NULL, NULL);
    ERR_punt();
    }

strcpy (owner_name, tdbb->tdbb_attachment->att_user->usr_user_name);

for (p = owner_name; *p; p++)
    *p = UPPER7 (*p);
*p = '\0';

GET_STRING (ptr, role_name);

if (strcmp (role_name, owner_name)==0)
    {
    /************************************************
    **
    ** user name could not be used for SQL role
    **
    *************************************************/
    DYN_error (FALSE, 193, owner_name, NULL, NULL, NULL, NULL);
    ERR_punt();
    }

if (strcmp (role_name, "NONE")==0)
    {
    /************************************************
    **
    ** keyword NONE could not be used as SQL role name
    **
    *************************************************/
    DYN_error (FALSE, 195, role_name, NULL, NULL, NULL, NULL);
    ERR_punt();
    }

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;  /* save environment */
tdbb->tdbb_setjmp = (UCHAR*) env;        /* get new environment */

if (SETJMP (env))
    {
    if (request)
	DYN_rundown_request (old_env, request, drq_role_gens);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
    DYN_error_punt (TRUE, 8, NULL, NULL, NULL, NULL, NULL);
    /* msg 8: "DEFINE ROLE failed" */
    }

if (is_it_user_name (gbl, role_name, tdbb))
    {
    /************************************************
    **
    ** user name could not be used for SQL role
    **
    *************************************************/
    DYN_error (FALSE, 193, role_name, NULL, NULL, NULL, NULL);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
    ERR_punt();
    }

if (DYN_is_it_sql_role (gbl, role_name, dummy_name, tdbb))
    {
    /************************************************
    **
    ** SQL role already exist
    **
    *************************************************/
    DYN_error (FALSE, 194, role_name, NULL, NULL, NULL, NULL);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
    ERR_punt();
    }

request = (BLK) CMP_find_request (tdbb, drq_role_gens, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$ROLES

    strcpy (X.RDB$ROLE_NAME, role_name);
    strcpy (X.RDB$OWNER_NAME, owner_name);

END_STORE;

if (!DYN_REQUEST (drq_role_gens))
    DYN_REQUEST (drq_role_gens) = request;

if (*(*ptr)++ != gds__dyn_end)
    DYN_error_punt (TRUE, 9, NULL, NULL, NULL, NULL, NULL);
    /* msg 9: "DEFINE ROLE unexpected dyn verb" */

tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
}

void DYN_define_global_field (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
USHORT		  dtype;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_gfields, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FLD IN RDB$FIELDS USING
    if (!GET_STRING (ptr, FLD.RDB$FIELD_NAME))
	DYN_UTIL_generate_field_name (tdbb, gbl, FLD.RDB$FIELD_NAME);
    
    FLD.RDB$SYSTEM_FLAG.NULL = TRUE;
    FLD.RDB$FIELD_SCALE.NULL = TRUE;
    FLD.RDB$FIELD_SUB_TYPE.NULL = TRUE;
    FLD.RDB$SEGMENT_LENGTH.NULL = TRUE;
    FLD.RDB$QUERY_NAME.NULL = TRUE;
    FLD.RDB$QUERY_HEADER.NULL = TRUE;
    FLD.RDB$EDIT_STRING.NULL = TRUE;
    FLD.RDB$MISSING_VALUE.NULL = TRUE;
    FLD.RDB$COMPUTED_BLR.NULL = TRUE;
    FLD.RDB$COMPUTED_SOURCE.NULL = TRUE;
    FLD.RDB$DEFAULT_VALUE.NULL = TRUE;
    FLD.RDB$DEFAULT_SOURCE.NULL = TRUE;
    FLD.RDB$VALIDATION_BLR.NULL = TRUE;
    FLD.RDB$VALIDATION_SOURCE.NULL = TRUE;
    FLD.RDB$DESCRIPTION.NULL = TRUE;
    FLD.RDB$DIMENSIONS.NULL = TRUE;
    FLD.RDB$CHARACTER_LENGTH.NULL = TRUE;
    FLD.RDB$NULL_FLAG.NULL = TRUE;
    FLD.RDB$CHARACTER_SET_ID.NULL = TRUE;
    FLD.RDB$COLLATION_ID.NULL = TRUE;
    FLD.RDB$FIELD_PRECISION.NULL = TRUE;

    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_system_flag:
		FLD.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		FLD.RDB$SYSTEM_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_fld_length:
		FLD.RDB$FIELD_LENGTH = DYN_get_number (ptr);
		FLD.RDB$FIELD_LENGTH.NULL = FALSE;
		break;

	    case gds__dyn_fld_type:
		FLD.RDB$FIELD_TYPE = dtype = DYN_get_number (ptr);
		switch (dtype)
		    {
		    case blr_short :	
			FLD.RDB$FIELD_LENGTH = 2; 
			FLD.RDB$FIELD_LENGTH.NULL = FALSE;
			break;

		    case blr_long :
		    case blr_float:
		    case blr_sql_time:
		    case blr_sql_date:
			FLD.RDB$FIELD_LENGTH = 4; 
			FLD.RDB$FIELD_LENGTH.NULL = FALSE;
			break;

		    case blr_int64:
		    case blr_quad:
		    case blr_timestamp:
		    case blr_double:
		    case blr_d_float:
		    case blr_blob:
			FLD.RDB$FIELD_LENGTH = 8; 
			FLD.RDB$FIELD_LENGTH.NULL = FALSE;
			break;
		    
		    default:
			break;
		    }
		break;

	    case gds__dyn_fld_scale:
		FLD.RDB$FIELD_SCALE = DYN_get_number (ptr);
		FLD.RDB$FIELD_SCALE.NULL = FALSE;
		break;

	    case isc_dyn_fld_precision:
		FLD.RDB$FIELD_PRECISION = DYN_get_number (ptr);
		FLD.RDB$FIELD_PRECISION.NULL = FALSE;
		break;

	    case gds__dyn_fld_sub_type:
		FLD.RDB$FIELD_SUB_TYPE = DYN_get_number (ptr);
		FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_fld_char_length:
		FLD.RDB$CHARACTER_LENGTH = DYN_get_number (ptr);
		FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
		break;

	    case gds__dyn_fld_character_set:
		FLD.RDB$CHARACTER_SET_ID = DYN_get_number (ptr);
		FLD.RDB$CHARACTER_SET_ID.NULL = FALSE;
		break;

	    case gds__dyn_fld_collation:
		FLD.RDB$COLLATION_ID = DYN_get_number (ptr);
		FLD.RDB$COLLATION_ID.NULL = FALSE;
		break;

	    case gds__dyn_fld_segment_length:
		FLD.RDB$SEGMENT_LENGTH = DYN_get_number (ptr);
		FLD.RDB$SEGMENT_LENGTH.NULL = FALSE;
		break;

	    case gds__dyn_fld_query_name:
		GET_STRING (ptr, FLD.RDB$QUERY_NAME);
		FLD.RDB$QUERY_NAME.NULL = FALSE;
		break;

	    case gds__dyn_fld_query_header:
		DYN_put_blr_blob (gbl, ptr, &FLD.RDB$QUERY_HEADER);
		FLD.RDB$QUERY_HEADER.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_query_header2:
		DYN_put_blr_blob2 (gbl, ptr, &FLD.RDB$QUERY_HEADER);
		FLD.RDB$QUERY_HEADER.NULL = FALSE;
		break;
#endif
	    
	    case gds__dyn_fld_not_null:
		FLD.RDB$NULL_FLAG.NULL = FALSE;
		FLD.RDB$NULL_FLAG = TRUE;
		break;

	    case gds__dyn_fld_missing_value:
		DYN_put_blr_blob (gbl, ptr, &FLD.RDB$MISSING_VALUE);
		FLD.RDB$MISSING_VALUE.NULL = FALSE;
		break;

	    case gds__dyn_fld_computed_blr:
		DYN_put_blr_blob (gbl, ptr, &FLD.RDB$COMPUTED_BLR);
		FLD.RDB$COMPUTED_BLR.NULL = FALSE;
		break;

	    case gds__dyn_fld_computed_source:
		DYN_put_text_blob (gbl, ptr, &FLD.RDB$COMPUTED_SOURCE);
		FLD.RDB$COMPUTED_SOURCE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_computed_source2:
		DYN_put_text_blob2 (gbl, ptr, &FLD.RDB$COMPUTED_SOURCE);
		FLD.RDB$COMPUTED_SOURCE.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_default_value:
		FLD.RDB$DEFAULT_VALUE.NULL = FALSE;
		DYN_put_blr_blob (gbl, ptr, &FLD.RDB$DEFAULT_VALUE);
		break;

	    case gds__dyn_fld_default_source:
		FLD.RDB$DEFAULT_SOURCE.NULL = FALSE;
		DYN_put_text_blob (gbl, ptr, &FLD.RDB$DEFAULT_SOURCE);
		break;

	    case gds__dyn_fld_validation_blr:
		DYN_put_blr_blob (gbl, ptr, &FLD.RDB$VALIDATION_BLR);
		FLD.RDB$VALIDATION_BLR.NULL = FALSE;
		break;

	    case gds__dyn_fld_validation_source:
		DYN_put_text_blob (gbl, ptr, &FLD.RDB$VALIDATION_SOURCE);
		FLD.RDB$VALIDATION_SOURCE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_validation_source2:
		DYN_put_text_blob2 (gbl, ptr, &FLD.RDB$VALIDATION_SOURCE);
		FLD.RDB$VALIDATION_SOURCE.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_edit_string:
		GET_STRING (ptr, FLD.RDB$EDIT_STRING);
		FLD.RDB$EDIT_STRING.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_edit_string2:
		DYN_get_string2(ptr,FLD.RDB$EDIT_STRING,sizeof(FLD.RDB$EDIT_STRING));
		FLD.RDB$EDIT_STRING.NULL = FALSE;
		break;
#endif

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &FLD.RDB$DESCRIPTION);
		FLD.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &FLD.RDB$DESCRIPTION);
		FLD.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_dimensions:
		FLD.RDB$DIMENSIONS = DYN_get_number (ptr);
		FLD.RDB$DIMENSIONS.NULL = FALSE;
		break;

		
	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, relation_name, (field_name) ? field_name :
			FLD.RDB$FIELD_NAME, NULL_PTR, NULL_PTR, NULL_PTR);
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_gfields);
	DYN_error_punt (TRUE, 13, NULL, NULL, NULL, NULL, NULL);
	/* msg 13: "STORE RDB$FIELDS failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_gfields))
    DYN_REQUEST (drq_s_gfields) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_index (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    UCHAR	index_type,
    TEXT	*new_index_name,
    TEXT	*referred_index_name,
    TEXT        *cnst_name,
    UCHAR       *ri_actionP
    )
{
/**************************************
 *
 *	D Y N _ d e f i n e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	creates an index.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
BLK	 	  old_request;
VOLATILE SSHORT	  id, old_id;
UCHAR		  index_name [32], field_name [32], referenced_relation [32];
UCHAR		  verb, seg_count, fld_count;
LLS		  field_list = NULL, seg_list = NULL, list_ptr;
STR		  str;
USHORT		  found, key_length, length, referred_cols = 0;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
TEXT    	  trigger_name [32];

if (ri_actionP != NULL)
    (*ri_actionP) = 0;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_indices)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 21, NULL, NULL, NULL, NULL, NULL);
	/* msg 21: "STORE RDB$INDICES failed" */
	}
    else if (id == drq_s_idx_segs)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 15, NULL, NULL, NULL, NULL, NULL);
	/* msg 15: "STORE RDB$INDICES failed" */
	}

    DYN_rundown_request (old_env, request, -1);
    if (id == drq_l_lfield)
	DYN_error_punt (TRUE, 15, NULL, NULL, NULL, NULL, NULL);
	/* msg 15: "STORE RDB$INDICES failed" */
    else if (id == drq_l_unq_idx)
	DYN_error_punt (TRUE, 17, NULL, NULL, NULL, NULL, NULL);
	/* msg 17: "Primary Key field lookup failed" */
    else if (id == drq_l_view_idx)
	DYN_error_punt (TRUE, 180, NULL, NULL, NULL, NULL, NULL);
	/* msg 180: "Table Name lookup failed" */
    else
	DYN_error_punt (TRUE, 19, NULL, NULL, NULL, NULL, NULL);
	/* msg 19: "Primary Key lookup failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_s_indices, DYN_REQUESTS);
id = drq_s_indices;

referenced_relation [0] = 0;
key_length = 0;
STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	IDX IN RDB$INDICES
    IDX.RDB$UNIQUE_FLAG.NULL = TRUE;
    IDX.RDB$INDEX_INACTIVE.NULL = TRUE;
    IDX.RDB$INDEX_TYPE.NULL = TRUE;
    IDX.RDB$DESCRIPTION.NULL = TRUE;
    IDX.RDB$FOREIGN_KEY.NULL = TRUE;
    IDX.RDB$EXPRESSION_SOURCE.NULL = TRUE; 
    IDX.RDB$EXPRESSION_BLR.NULL = TRUE; 
    fld_count = seg_count = 0;
    if (!GET_STRING (ptr, IDX.RDB$INDEX_NAME))
	DYN_UTIL_generate_index_name (tdbb, gbl, IDX.RDB$INDEX_NAME, index_type);
    if (new_index_name != NULL)
	strcpy (new_index_name, IDX.RDB$INDEX_NAME);
    if (relation_name)
	strcpy (IDX.RDB$RELATION_NAME, relation_name);
    else if (*(*ptr)++ == gds__dyn_rel_name)
 	GET_STRING (ptr, IDX.RDB$RELATION_NAME);
    else
	DYN_error_punt (FALSE, 14, NULL, NULL, NULL, NULL, NULL);
	/* msg 14: "No relation specified for index" */

    IDX.RDB$RELATION_NAME.NULL = FALSE;

    /* Check if the table is actually a view */

    old_request = request;
    old_id = id;
    request = (BLK) CMP_find_request (tdbb, drq_l_view_idx, DYN_REQUESTS);
    id = drq_l_view_idx;

    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	VREL IN RDB$RELATIONS WITH VREL.RDB$RELATION_NAME EQ 
	IDX.RDB$RELATION_NAME 
      
	if (!VREL.RDB$VIEW_BLR.NULL)
	    DYN_error_punt (FALSE, 181, NULL, NULL, NULL, NULL, NULL);
	    /* msg 181: "attempt to index a view" */

    END_FOR;
    if (!DYN_REQUEST (drq_l_view_idx))
	DYN_REQUEST (drq_l_view_idx) = request;

    request = old_request;
    id = old_id;

    /* The previous 2 lines and the next two lines can
     * deleted as long as no code is added in the middle.
     */

    old_request = request;
    old_id = id;
    request = (BLK) CMP_find_request (tdbb, drq_l_lfield, DYN_REQUESTS);
    id = drq_l_lfield;

    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_idx_unique:
		IDX.RDB$UNIQUE_FLAG = DYN_get_number (ptr);
		IDX.RDB$UNIQUE_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_idx_inactive:
		IDX.RDB$INDEX_INACTIVE = DYN_get_number (ptr);
		IDX.RDB$INDEX_INACTIVE.NULL = FALSE;
		break;

	    case gds__dyn_idx_type:
		IDX.RDB$INDEX_TYPE = DYN_get_number (ptr);
		IDX.RDB$INDEX_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_fld_name:
		str = (STR) ALLOCDV (type_str, sizeof (field_name) - 1);
 		DYN_get_string (ptr, str->str_data, sizeof (field_name), TRUE);
		LLS_PUSH (str, &seg_list);
		seg_count++;

		FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
			F IN RDB$RELATION_FIELDS CROSS GF IN RDB$FIELDS 
			WITH GF.RDB$FIELD_NAME EQ F.RDB$FIELD_SOURCE AND
			F.RDB$FIELD_NAME EQ str->str_data AND
			IDX.RDB$RELATION_NAME EQ F.RDB$RELATION_NAME
		    if (!DYN_REQUEST (drq_l_lfield))
			DYN_REQUEST (drq_l_lfield) = request;

		   fld_count++;
		   if (GF.RDB$FIELD_TYPE == blr_blob)
			{
			DYN_error_punt (FALSE, 116, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
			/* msg 116 "attempt to index blob field in index %s" */
			}
		   else if (!GF.RDB$DIMENSIONS.NULL)
			{
			DYN_error_punt (FALSE, 117, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
			/* msg 117 "attempt to index array field in index %s" */
			}
		   else if (!GF.RDB$COMPUTED_BLR.NULL)
			{
			DYN_error_punt (FALSE, 179, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
			/* msg 179 "attempt to index COMPUTED BY field in index %s" */
			}
		   else if (GF.RDB$FIELD_TYPE == blr_varying ||
			    GF.RDB$FIELD_TYPE == blr_text)
			{
			/* Compute the length of the key segment allowing
			   for international information.  Note that we
			   must convert a <character set, collation> type
			   to an index type in order to compute the length */
			if (!F.RDB$COLLATION_ID.NULL)
			    length = INTL_key_length (tdbb,  
					INTL_TEXT_TO_INDEX (
					    INTL_CS_COLL_TO_TTYPE (
						GF.RDB$CHARACTER_SET_ID, 
						F.RDB$COLLATION_ID)),
					GF.RDB$FIELD_LENGTH);
			else if (!GF.RDB$COLLATION_ID.NULL)
			    length = INTL_key_length (tdbb, 
					INTL_TEXT_TO_INDEX (
					    INTL_CS_COLL_TO_TTYPE (
						GF.RDB$CHARACTER_SET_ID, 
						GF.RDB$COLLATION_ID)),
					GF.RDB$FIELD_LENGTH);
			else
			    length = GF.RDB$FIELD_LENGTH;
			}
		    else
			length = sizeof (double);
		    if (key_length)
			key_length += ((length + STUFF_COUNT - 1) /
				(unsigned) STUFF_COUNT) * (STUFF_COUNT + 1);
		    else
			key_length = length;
		END_FOR;
		if (!DYN_REQUEST (drq_l_lfield))
		    DYN_REQUEST (drq_l_lfield) = request;
		break;

#ifdef EXPRESSION_INDICES
	    /* for expression indices, store the BLR and the source string */ 

	    case gds__dyn_fld_computed_blr:
		DYN_put_blr_blob (gbl, ptr, &IDX.RDB$EXPRESSION_BLR);
		IDX.RDB$EXPRESSION_BLR.NULL = FALSE;
		break;

	    case gds__dyn_fld_computed_source:
		DYN_put_text_blob (gbl, ptr, &IDX.RDB$EXPRESSION_SOURCE);
		IDX.RDB$EXPRESSION_SOURCE.NULL = FALSE;
		break;   
#endif         

	    /* for foreign keys, point to the corresponding relation */

	    case gds__dyn_idx_foreign_key:
 		GET_STRING (ptr, referenced_relation);
		break;

	    case gds__dyn_idx_ref_column:
		str = (STR) ALLOCDV (type_str, sizeof (field_name) - 1);
 		DYN_get_string (ptr, str->str_data, sizeof (field_name), TRUE);
		LLS_PUSH (str, &field_list);
		referred_cols++;
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &IDX.RDB$DESCRIPTION);
		IDX.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &IDX.RDB$DESCRIPTION);
		IDX.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    case gds__dyn_foreign_key_delete:
		assert (ri_actionP != NULL);
       		switch (verb = *(*ptr)++)
	    	    {
	    	    case gds__dyn_foreign_key_cascade:
			(*ri_actionP) |=  FOR_KEY_DEL_CASCADE;
			if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
			    {
		            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
			    DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
			    }
                        else
			    DYN_unsupported_verb(); 
			break;
	    	    case gds__dyn_foreign_key_null:
			(*ri_actionP) |= FOR_KEY_DEL_NULL;
			if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
			    {
		            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
			    DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
			    }
                        else
			    DYN_unsupported_verb(); 
			break;
	    	    case gds__dyn_foreign_key_default:
			(*ri_actionP) |= FOR_KEY_DEL_DEFAULT;
                        if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
                           {
                            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
                            DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
                            }
                        else
                            DYN_unsupported_verb();
                        break;
	    	    case gds__dyn_foreign_key_none:
			(*ri_actionP) |=  FOR_KEY_DEL_NONE;
			break;
	    	    default: 
			assert (0); /* should not come here */
			DYN_unsupported_verb();
	    	    }
		break;

            case gds__dyn_foreign_key_update:
		assert (ri_actionP != NULL);
       		switch (verb = *(*ptr)++)
	    	    {
	    	    case gds__dyn_foreign_key_cascade:
			(*ri_actionP) |= FOR_KEY_UPD_CASCADE;
			if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
			    {
		            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
			    DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
			    }
                        else
			    DYN_unsupported_verb();
			break;
	    	    case gds__dyn_foreign_key_null:
			(*ri_actionP) |= FOR_KEY_UPD_NULL;
			if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
			    {
		            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
			    DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
			    }
                        else
			    DYN_unsupported_verb();
			break;
	    	    case gds__dyn_foreign_key_default:
			(*ri_actionP) |= FOR_KEY_UPD_DEFAULT;
                        if ((verb = *(*ptr)++) == gds__dyn_def_trigger)
                           {
                            DYN_define_trigger (gbl, ptr, relation_name, trigger_name, TRUE);
                            DYN_UTIL_store_check_constraints (tdbb, gbl, cnst_name,
						     trigger_name);
                            }
                        else
			    DYN_unsupported_verb();
			break;
	    	    case gds__dyn_foreign_key_none:
			(*ri_actionP) |= FOR_KEY_UPD_NONE;
			break;
	    	    default: 
			assert(0); /* should not come here */
			DYN_unsupported_verb();
	    	    }
		break;
	    default:
		DYN_unsupported_verb();
	    }
    request = old_request;
    id = old_id;

    key_length = ROUNDUP (key_length, sizeof (SLONG));
    if (key_length >= MAX_KEY)
	DYN_error_punt (FALSE, 118, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
	/* msg 118 "key size too big for index %s" */

    if (seg_count)
	{
	if (seg_count != fld_count)
	    DYN_error_punt (FALSE, 120, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
	    /* msg 118 "Unknown fields in index %s" */

	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_s_idx_segs, DYN_REQUESTS);
	id = drq_s_idx_segs;
	while (seg_list)
	    {
	    str = (STR) LLS_POP (&seg_list);
	    STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		    X IN RDB$INDEX_SEGMENTS
		strcpy (X.RDB$INDEX_NAME, IDX.RDB$INDEX_NAME);
		strcpy (X.RDB$FIELD_NAME, str->str_data);
		X.RDB$FIELD_POSITION = --fld_count;
		ALL_release (str);
	    END_STORE;
	    }
	if (!DYN_REQUEST (drq_s_idx_segs))
	    DYN_REQUEST (drq_s_idx_segs) = request;
	request = old_request;
	id = old_id;
	}
    else
#ifdef EXPRESSION_INDICES
        if (IDX.RDB$EXPRESSION_BLR.NULL)
#endif
	DYN_error_punt (FALSE, 119, IDX.RDB$INDEX_NAME, NULL, NULL, NULL, NULL);
	/* msg 119 "no keys for index %s" */

    if (field_list)
	{
	/* If referring columns count <> referred columns return error */

	if (seg_count != referred_cols)
	    DYN_error_punt (TRUE, 133, NULL, NULL, NULL, NULL, NULL);
	    /* msg 133: "Number of referencing columns do not equal number of referenced columns */

	/* lookup a unique index in the referenced relation with the
	   referenced fields mentioned */

	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_l_unq_idx, DYN_REQUESTS);
	id = drq_l_unq_idx;

	index_name [0] = 0;
	list_ptr = NULL;
	found = FALSE;
	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    Y IN RDB$INDICES CROSS
	    Z IN RDB$INDEX_SEGMENTS OVER RDB$INDEX_NAME WITH
  	    Y.RDB$RELATION_NAME EQ referenced_relation AND
	    Y.RDB$UNIQUE_FLAG NOT MISSING
	    SORTED BY Y.RDB$INDEX_NAME, DESCENDING Z.RDB$FIELD_POSITION

	    if (!DYN_REQUEST (drq_l_unq_idx))
		DYN_REQUEST (drq_l_unq_idx) = request;

	    /* create a control break on index name, in which we set up
	       to handle a new index, assuming it is the right one */

	    DYN_terminate (Y.RDB$INDEX_NAME, sizeof (Y.RDB$INDEX_NAME));
	    if (strcmp (index_name, Y.RDB$INDEX_NAME))
		{
		if (list_ptr)
		    found = FALSE;
		if (found)
		    break;
		list_ptr = field_list;
		strcpy (index_name, Y.RDB$INDEX_NAME);
		found = TRUE;
		}

	    /* if there are no more fields or the field name doesn't
	       match, then this is not the correct index */

	    if (list_ptr)
		{
		DYN_terminate (Z.RDB$FIELD_NAME, sizeof (Z.RDB$FIELD_NAME));
		DYN_terminate (((STR) list_ptr->lls_object)->str_data,
			       (int) ((STR) list_ptr->lls_object)->str_length);
	    	if (strcmp (((STR) list_ptr->lls_object)->str_data, Z.RDB$FIELD_NAME))
		    found = FALSE;
		list_ptr = list_ptr->lls_next;
	    	}
	    else
		found = FALSE;

	END_FOR;
	if (!DYN_REQUEST (drq_l_unq_idx))
	    DYN_REQUEST (drq_l_unq_idx) = request;
	request = old_request;
	id = old_id;

	if (list_ptr)
  	    found = FALSE;

	if (found)
	    {
	    strcpy (IDX.RDB$FOREIGN_KEY, index_name);
	    IDX.RDB$FOREIGN_KEY.NULL = FALSE;
	    if (referred_index_name != NULL)
		strcpy (referred_index_name, index_name);
	    }
	else
	    DYN_error_punt (FALSE, 18, NULL, NULL, NULL, NULL, NULL);
	    /* msg 18: "could not find unique index with specified fields" */
	}
    else if (referenced_relation [0])
	{
	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_l_primary, DYN_REQUESTS);
	id = drq_l_primary;

	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    IND IN RDB$INDICES WITH
	    IND.RDB$RELATION_NAME EQ referenced_relation AND
	    IND.RDB$INDEX_NAME STARTING "RDB$PRIMARY"

	    if (!DYN_REQUEST (drq_l_primary))
		DYN_REQUEST (drq_l_primary) = request;

	    /* Number of columns in referred index should be same as number
	       of columns in referring index */

	    if (seg_count != IND.RDB$SEGMENT_COUNT)
		DYN_error_punt (TRUE, 133, NULL, NULL, NULL, NULL, NULL);
		/* msg 133: "Number of referencing columns do not equal number of referenced columns" */

	    DYN_terminate (IND.RDB$INDEX_NAME, sizeof (IND.RDB$INDEX_NAME));
	    strcpy (IDX.RDB$FOREIGN_KEY, IND.RDB$INDEX_NAME);
	    IDX.RDB$FOREIGN_KEY.NULL = FALSE;
	    if (referred_index_name != NULL)
		strcpy (referred_index_name, IND.RDB$INDEX_NAME);

	END_FOR;
	if (!DYN_REQUEST (drq_l_primary))
	    DYN_REQUEST (drq_l_primary) = request;
	request = old_request;
	id = old_id;

	if (IDX.RDB$FOREIGN_KEY.NULL)
	    DYN_error_punt (FALSE, 20, NULL, NULL, NULL, NULL, NULL);
	    /* msg 20: "could not find primary key index in specified relation" */
	}

    IDX.RDB$SEGMENT_COUNT = seg_count;
END_STORE;

if (!DYN_REQUEST (drq_s_indices))
    DYN_REQUEST (drq_s_indices) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_local_field (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ l o c a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
BLK		  old_request;
VOLATILE SSHORT	  id, old_id;
UCHAR		  verb, *blr;
USHORT		  dtype, length,
                  lflag, sflag, slflag, scflag, clflag, clength,
                  prflag, precision;
SSHORT		  stype, scale;
SSHORT		  charset_id;
USHORT		  charset_id_flag;
TEXT		 *source;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
SLONG		  fld_pos;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_lfields)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 23, NULL, NULL, NULL, NULL, NULL);
	/* msg 23: "STORE RDB$RELATION_FIELDS failed" */
	}
    else
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 22, NULL, NULL, NULL, NULL, NULL);
	/* msg 22: "STORE RDB$FIELDS failed" */
	}
    }

request = (BLK) CMP_find_request (tdbb, drq_s_lfields, DYN_REQUESTS);
id = drq_s_lfields;

scflag = lflag = sflag = slflag = clflag = prflag = FALSE;
charset_id_flag = FALSE;
blr = NULL;
source = NULL;
STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RFR IN RDB$RELATION_FIELDS

    GET_STRING (ptr, RFR.RDB$FIELD_NAME);
    strcpy (RFR.RDB$FIELD_SOURCE, RFR.RDB$FIELD_NAME);
    if (field_name != NULL)
	strcpy (field_name, RFR.RDB$FIELD_NAME);
    RFR.RDB$RELATION_NAME.NULL = TRUE;
    if (relation_name)
	{
	strcpy (RFR.RDB$RELATION_NAME, relation_name);
	RFR.RDB$RELATION_NAME.NULL = FALSE;
	}
    RFR.RDB$SYSTEM_FLAG = 0;
    RFR.RDB$SYSTEM_FLAG.NULL = FALSE;
    RFR.RDB$NULL_FLAG.NULL = TRUE;
    RFR.RDB$BASE_FIELD.NULL = TRUE;
    RFR.RDB$UPDATE_FLAG.NULL = TRUE;
    RFR.RDB$FIELD_POSITION.NULL = TRUE;
    RFR.RDB$VIEW_CONTEXT.NULL = TRUE;
    RFR.RDB$QUERY_NAME.NULL = TRUE;
    RFR.RDB$QUERY_HEADER.NULL = TRUE;
    RFR.RDB$SECURITY_CLASS.NULL = TRUE;
    RFR.RDB$DESCRIPTION.NULL = TRUE;
    RFR.RDB$DEFAULT_VALUE.NULL = TRUE;
    RFR.RDB$DEFAULT_SOURCE.NULL = TRUE;
    RFR.RDB$EDIT_STRING.NULL = TRUE;
    RFR.RDB$COLLATION_ID.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_rel_name:
		GET_STRING (ptr, RFR.RDB$RELATION_NAME);
		relation_name = RFR.RDB$RELATION_NAME;
		RFR.RDB$RELATION_NAME.NULL = FALSE;
		break;
	    
	    case gds__dyn_fld_source:
		GET_STRING (ptr, RFR.RDB$FIELD_SOURCE);
		break;

	    case gds__dyn_fld_base_fld:
		GET_STRING (ptr, RFR.RDB$BASE_FIELD);
		RFR.RDB$BASE_FIELD.NULL = FALSE;
		break;

	    case gds__dyn_fld_query_name:
		GET_STRING (ptr, RFR.RDB$QUERY_NAME);
		RFR.RDB$QUERY_NAME.NULL = FALSE;
		break;

	    case gds__dyn_fld_query_header:
		DYN_put_blr_blob (gbl, ptr, &RFR.RDB$QUERY_HEADER);
		RFR.RDB$QUERY_HEADER.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_query_header2:
		DYN_put_blr_blob2 (gbl, ptr, &RFR.RDB$QUERY_HEADER);
		RFR.RDB$QUERY_HEADER.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_edit_string:
		GET_STRING (ptr, RFR.RDB$EDIT_STRING);
		RFR.RDB$EDIT_STRING.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_edit_string2:
		DYN_get_string2 (ptr, RFR.RDB$EDIT_STRING, sizeof(RFR.RDB$EDIT_STRING));
		RFR.RDB$EDIT_STRING.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_position:
		RFR.RDB$FIELD_POSITION = DYN_get_number (ptr);
		RFR.RDB$FIELD_POSITION.NULL = FALSE;
		break;

	    case gds__dyn_system_flag:
		RFR.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		RFR.RDB$SYSTEM_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_fld_update_flag:
	    case gds__dyn_update_flag:
		RFR.RDB$UPDATE_FLAG = DYN_get_number (ptr);
		RFR.RDB$UPDATE_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_view_context:
		RFR.RDB$VIEW_CONTEXT = DYN_get_number (ptr);
		RFR.RDB$VIEW_CONTEXT.NULL = FALSE;
		break;

	    case gds__dyn_security_class:
		GET_STRING (ptr, RFR.RDB$SECURITY_CLASS);
		RFR.RDB$SECURITY_CLASS.NULL = FALSE;
		break;
	    
	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &RFR.RDB$DESCRIPTION);
		RFR.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &RFR.RDB$DESCRIPTION);
		RFR.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    case gds__dyn_fld_computed_blr:
		DYN_UTIL_generate_field_name (tdbb, gbl, RFR.RDB$FIELD_SOURCE);
		blr = *ptr;
		DYN_skip_attribute (ptr);
		break;

	    case gds__dyn_fld_computed_source:
		source = (TEXT*) *ptr;
		DYN_skip_attribute (ptr);
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_fld_computed_source2:
		source = (TEXT*) *ptr;
		DYN_skip_attribute2 (ptr);
		break;
#endif
	    case gds__dyn_fld_default_value:
		RFR.RDB$DEFAULT_VALUE.NULL = FALSE;
		DYN_put_blr_blob (gbl, ptr, &RFR.RDB$DEFAULT_VALUE);
		break;


	    case gds__dyn_fld_default_source:
		RFR.RDB$DEFAULT_SOURCE.NULL = FALSE;
		DYN_put_text_blob (gbl, ptr, &RFR.RDB$DEFAULT_SOURCE);
		break;

	    case gds__dyn_fld_not_null:
		RFR.RDB$NULL_FLAG.NULL = FALSE;
		RFR.RDB$NULL_FLAG = TRUE;
		break;

	    case gds__dyn_fld_type:
		dtype = DYN_get_number (ptr);
		break;

	    case gds__dyn_fld_length:
		length = DYN_get_number (ptr);
		lflag = TRUE;
		break;

	    case gds__dyn_fld_sub_type:
		stype = DYN_get_number (ptr);
		sflag = TRUE;
		break;

	    case gds__dyn_fld_char_length:
		clength = DYN_get_number (ptr);
		clflag = TRUE;
		break;

	    case gds__dyn_fld_segment_length:
		stype = DYN_get_number (ptr);
		slflag = TRUE;
		break;

	    case gds__dyn_fld_scale:
		scale = DYN_get_number (ptr);
		scflag = TRUE;
		break;

	    case isc_dyn_fld_precision:
		precision = DYN_get_number (ptr);
		prflag = TRUE;
		break;

	    case gds__dyn_fld_character_set:
		charset_id = DYN_get_number (ptr);
		charset_id_flag = TRUE;
		break;

	    case gds__dyn_fld_collation:
		RFR.RDB$COLLATION_ID.NULL = FALSE;
		RFR.RDB$COLLATION_ID = DYN_get_number (ptr);
		break;

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, relation_name, RFR.RDB$FIELD_SOURCE,
			NULL_PTR, NULL_PTR, NULL_PTR);
	    }

    if (RFR.RDB$FIELD_POSITION.NULL == TRUE)
	{
	fld_pos = -1;
	DYN_UTIL_generate_field_position (tdbb, gbl, relation_name, &fld_pos);

	if (fld_pos >= 0)
	    {
	    RFR.RDB$FIELD_POSITION = ++fld_pos;
	    RFR.RDB$FIELD_POSITION.NULL = FALSE;
	    }
	}

    if (blr)
	{
	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_s_gfields2, DYN_REQUESTS);

	STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		FLD IN RDB$FIELDS
	    strcpy (FLD.RDB$FIELD_NAME, RFR.RDB$FIELD_SOURCE);
	    DYN_put_blr_blob (gbl, &blr, &FLD.RDB$COMPUTED_BLR);
	    if (source)
		{
#if (defined JPN_EUC || defined JPN_SJIS)
		DYN_put_text_blob2 (gbl, &source, &FLD.RDB$COMPUTED_SOURCE);
#else
		DYN_put_text_blob (gbl, &source, &FLD.RDB$COMPUTED_SOURCE);
#endif /* (defined JPN_EUC || defined JPN_SJIS) */
		}
	    FLD.RDB$FIELD_TYPE = dtype;
	    if (lflag)
		{
		FLD.RDB$FIELD_LENGTH = length;
		FLD.RDB$FIELD_LENGTH.NULL = FALSE;
		}
	    else
		FLD.RDB$FIELD_LENGTH.NULL = TRUE;
	    if (sflag)
		{
		FLD.RDB$FIELD_SUB_TYPE = stype;
		FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		}
	    else
		FLD.RDB$FIELD_SUB_TYPE.NULL = TRUE;
	    if (clflag)
		{
		FLD.RDB$CHARACTER_LENGTH = clength;
		FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
		}
	    else
		FLD.RDB$CHARACTER_LENGTH.NULL = TRUE;
	    if (slflag)
		{
		FLD.RDB$SEGMENT_LENGTH = stype;
		FLD.RDB$SEGMENT_LENGTH.NULL = FALSE;
		}
	    else
		FLD.RDB$SEGMENT_LENGTH.NULL = TRUE;
	    if (scflag)
		{
		FLD.RDB$FIELD_SCALE = scale;
		FLD.RDB$FIELD_SCALE.NULL = FALSE;
		}
	    else
		FLD.RDB$FIELD_SCALE.NULL = TRUE;
	    if (prflag)
		{
		FLD.RDB$FIELD_PRECISION = precision;
		FLD.RDB$FIELD_PRECISION.NULL = FALSE;
		}
	    else
		FLD.RDB$FIELD_PRECISION.NULL = TRUE;
	    if (charset_id_flag)
		{
		FLD.RDB$CHARACTER_SET_ID = charset_id;
		FLD.RDB$CHARACTER_SET_ID.NULL = FALSE;
		}
	    else
		FLD.RDB$CHARACTER_SET_ID.NULL = TRUE;

	    END_STORE;

	    if (!DYN_REQUEST (drq_s_gfields2))
		DYN_REQUEST (drq_s_gfields2) = request;
	    request = old_request;
	    id = old_id;
	}

    if (!RFR.RDB$VIEW_CONTEXT.NULL)
	find_field_source (tdbb, gbl, relation_name, RFR.RDB$VIEW_CONTEXT,
	    RFR.RDB$BASE_FIELD, RFR.RDB$FIELD_SOURCE);

END_STORE;

if (!DYN_REQUEST (drq_s_lfields))
    DYN_REQUEST (drq_s_lfields) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_log_file (
    GBL		gbl,
    UCHAR	**ptr,
    USHORT	first_log_file,
    USHORT	default_log)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ l o g _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Define a database or shadow file.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
USHORT		  found = FALSE;
VOLATILE SSHORT	  id;
STR		  db_filename;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_log_files)
	{
	DYN_rundown_request (old_env, request, drq_s_log_files);
	DYN_error_punt (TRUE, 154, NULL, NULL, NULL, NULL, NULL);
	/* msg 154: STORE RDB$LOG_FILES failed */
	}
    else 
	{
	DYN_rundown_request (old_env, request, drq_l_log_files);
	DYN_error_punt (TRUE, 155, NULL, NULL, NULL, NULL, NULL);
	/* msg 155: Write ahead log lookup failed */
	}
    }

if (first_log_file)
    {
    id = drq_l_log_files;
    request = (BLK) CMP_find_request (tdbb, drq_l_log_files, DYN_REQUESTS);
    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	 FIL IN RDB$LOG_FILES
    	found = TRUE;
    END_FOR

    if (!DYN_REQUEST (drq_l_log_files))
	DYN_REQUEST (drq_l_log_files) = request;

    if (found)
	{
    	tdbb->tdbb_setjmp = (UCHAR*) old_env;
	DYN_error_punt (FALSE, 151, NULL, NULL, NULL, NULL, NULL);
	}
	/* msg 151: "Write ahead log already exists" */
    }

request = (BLK) CMP_find_request (tdbb, drq_s_log_files, DYN_REQUESTS);
id = drq_s_log_files;
STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$LOG_FILES

    X.RDB$FILE_LENGTH.NULL = TRUE;
    X.RDB$FILE_SEQUENCE.NULL = TRUE;
    X.RDB$FILE_PARTITIONS.NULL = TRUE;
    X.RDB$FILE_P_OFFSET.NULL = TRUE;

    if (default_log)
	{
	    db_filename = dbb->dbb_filename;
	    if (db_filename->str_length	>= sizeof (X.RDB$FILE_NAME))
		DYN_error_punt (FALSE, 159, NULL, NULL, NULL, NULL, NULL);
	    memcpy (X.RDB$FILE_NAME, db_filename->str_data, 
				(int) db_filename->str_length);
	    X.RDB$FILE_NAME [db_filename->str_length ] = '\0';
#ifdef NETWARE_386
 	    ISC_strip_extension (X.RDB$FILE_NAME);
#endif
    	    X.RDB$FILE_FLAGS.NULL = FALSE;
	    X.RDB$FILE_FLAGS = LOG_default | LOG_serial;
	}
    else
	{
	X.RDB$FILE_FLAGS.NULL = FALSE;
	X.RDB$FILE_FLAGS = 0;
    	GET_STRING (ptr, X.RDB$FILE_NAME);
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_file_length:
		    X.RDB$FILE_LENGTH = DYN_get_number (ptr);
		    X.RDB$FILE_LENGTH.NULL = FALSE;
		    break;

		case gds__dyn_log_file_sequence:
		    X.RDB$FILE_SEQUENCE.NULL = FALSE;
	    	    X.RDB$FILE_SEQUENCE = DYN_get_number (ptr);
	    	    break;

		case gds__dyn_log_file_partitions:
	    	    X.RDB$FILE_PARTITIONS.NULL = FALSE;
	    	    X.RDB$FILE_PARTITIONS = DYN_get_number (ptr);
	    	    break;

		case gds__dyn_log_file_raw:
	    	    X.RDB$FILE_FLAGS |= LOG_raw;
	    	    break;

		case gds__dyn_log_file_serial:
	    	    X.RDB$FILE_FLAGS |= LOG_serial;
#ifdef NETWARE_386
		    ISC_strip_extension (X.RDB$FILE_NAME);
#endif
	    	    break;

		case gds__dyn_log_file_overflow:
	    	    X.RDB$FILE_FLAGS |= LOG_overflow;
#ifdef NETWARE_386
		    ISC_strip_extension (X.RDB$FILE_NAME);
#endif
	    	    break;

		default:
	    	    DYN_unsupported_verb();
		}
	}

END_STORE;

if (!DYN_REQUEST (drq_s_log_files))
    DYN_REQUEST (drq_s_log_files) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_parameter (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*procedure_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB    	  tdbb;
DBB		  dbb;
UCHAR		  verb;
VOLATILE BLK	  request;
BLK		  request2;
USHORT		  f_length, f_type, f_charlength, f_seg_length,
		  f_scale_null, f_subtype_null, f_seg_length_null,
		  f_charlength_null, f_precision_null,
		  f_charset_null, f_collation_null;
VOLATILE SSHORT   id;
SSHORT		  f_subtype, f_scale, f_precision;
SSHORT		  f_charset, f_collation;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_prms, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
id = -1;
if (SETJMP (env))
    {
    if (id == drq_s_prms)
	{
	DYN_rundown_request (old_env, request, drq_s_prms);
	DYN_error_punt (TRUE, 136, NULL, NULL, NULL, NULL, NULL);
	/* msg 163: "STORE RDB$PROCEDURE_PARAMETERS failed" */
	}
    else if (id == drq_s_prm_src)
	{
	DYN_rundown_request (old_env, request, drq_s_prm_src);
	DYN_error_punt (TRUE, 136, NULL, NULL, NULL, NULL, NULL);
	/* msg 136: "STORE RDB$PROCEDURE_PARAMETERS failed" */
	}
    DYN_rundown_request (old_env, request, -1);

    /* Control should never reach this point,
       because id should always have one of the values tested above.  */
    assert(0);
    DYN_error_punt (TRUE, 0, NULL, NULL, NULL, NULL, NULL);
    }

f_length = f_type = f_subtype = f_charlength = f_scale = f_seg_length = 0;
f_charset = f_collation = f_precision = 0;
f_scale_null = f_subtype_null = f_charlength_null  = f_seg_length_null = TRUE;
f_precision_null = f_charset_null = f_collation_null = TRUE;
id = drq_s_prms;
STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	P IN RDB$PROCEDURE_PARAMETERS USING
    GET_STRING (ptr, P.RDB$PARAMETER_NAME);
    if (procedure_name)
	{
	P.RDB$PROCEDURE_NAME.NULL = FALSE;
	strcpy (P.RDB$PROCEDURE_NAME, procedure_name);
	}
    else
	P.RDB$PROCEDURE_NAME.NULL = TRUE;
    P.RDB$PARAMETER_NUMBER.NULL = TRUE;
    P.RDB$PARAMETER_TYPE.NULL = TRUE;
    P.RDB$FIELD_SOURCE.NULL = TRUE;
    P.RDB$SYSTEM_FLAG.NULL = TRUE;
    P.RDB$DESCRIPTION.NULL = TRUE;

    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_system_flag:
		P.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		P.RDB$SYSTEM_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_prm_number:
		P.RDB$PARAMETER_NUMBER = DYN_get_number (ptr);
		P.RDB$PARAMETER_NUMBER.NULL = FALSE;
		break;

	    case gds__dyn_prm_type:
		P.RDB$PARAMETER_TYPE = DYN_get_number (ptr);
		P.RDB$PARAMETER_TYPE.NULL = FALSE;
		break;

	    case gds__dyn_prc_name:
		GET_STRING (ptr, P.RDB$PROCEDURE_NAME);
		P.RDB$PROCEDURE_NAME.NULL = FALSE;
		break;

	    case gds__dyn_fld_source:
		GET_STRING (ptr, P.RDB$FIELD_SOURCE);
		P.RDB$FIELD_SOURCE.NULL = FALSE;
		break;

	    case gds__dyn_fld_length:
		f_length = DYN_get_number (ptr);
		break;

	    case gds__dyn_fld_type:
		f_type = DYN_get_number (ptr);
		switch (f_type)
		    {
		    case blr_short :	
			f_length = 2; 
			break;

		    case blr_long :
		    case blr_float:
		    case blr_sql_time:
		    case blr_sql_date:
			f_length = 4; 
			break;

		    case blr_int64:
		    case blr_quad:
		    case blr_timestamp:
		    case blr_double:
		    case blr_d_float:
			f_length = 8; 
			break;
		    
		    default:
			if (f_type == blr_blob)
			    f_length = 8; 
			break;
		    }
		break;

	    case gds__dyn_fld_scale:
		f_scale = DYN_get_number (ptr);
		f_scale_null = FALSE;
		break;

	    case isc_dyn_fld_precision:
		f_precision = DYN_get_number (ptr);
		f_precision_null = FALSE;
		break;

	    case gds__dyn_fld_sub_type:
		f_subtype = DYN_get_number (ptr);
		f_subtype_null = FALSE;
		break;

	    case gds__dyn_fld_char_length:
		f_charlength = DYN_get_number (ptr);
		f_charlength_null = FALSE;
		break;

	    case gds__dyn_fld_character_set:
		f_charset = DYN_get_number (ptr);
		f_charset_null = FALSE;
		break;

	    case gds__dyn_fld_collation:
		f_collation = DYN_get_number (ptr);
		f_collation_null = FALSE;
		break;

	    case gds__dyn_fld_segment_length:
		f_seg_length = DYN_get_number (ptr);
		f_seg_length_null = FALSE;
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &P.RDB$DESCRIPTION);
		P.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &P.RDB$DESCRIPTION);
		P.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, NULL_PTR, 
			NULL_PTR, NULL_PTR, NULL_PTR, procedure_name);
	    }
    if (P.RDB$FIELD_SOURCE.NULL)
	{
	/* Need to store dummy global field */
	id = drq_s_prm_src;
	request2 = (BLK) CMP_find_request (tdbb, drq_s_prm_src, DYN_REQUESTS);
	STORE (REQUEST_HANDLE request2 TRANSACTION_HANDLE gbl->gbl_transaction)
		PS IN RDB$FIELDS USING
	    DYN_UTIL_generate_field_name (tdbb, gbl, PS.RDB$FIELD_NAME);
	    strcpy (P.RDB$FIELD_SOURCE, PS.RDB$FIELD_NAME);
	    P.RDB$FIELD_SOURCE.NULL = FALSE;
	    PS.RDB$FIELD_LENGTH = f_length;
	    PS.RDB$FIELD_TYPE = f_type;
	    PS.RDB$FIELD_SUB_TYPE = f_subtype;
	    PS.RDB$FIELD_SUB_TYPE.NULL = f_subtype_null;
	    PS.RDB$FIELD_SCALE = f_scale;
	    PS.RDB$FIELD_SCALE.NULL = f_scale_null;
	    PS.RDB$FIELD_PRECISION = f_precision;
	    PS.RDB$FIELD_PRECISION.NULL = f_precision_null;
	    PS.RDB$SEGMENT_LENGTH = f_seg_length;
	    PS.RDB$SEGMENT_LENGTH.NULL = f_seg_length_null;
	    PS.RDB$CHARACTER_LENGTH = f_charlength;
	    PS.RDB$CHARACTER_LENGTH.NULL = f_charlength_null;
	    PS.RDB$CHARACTER_SET_ID = f_charset;
	    PS.RDB$CHARACTER_SET_ID.NULL = f_charset_null;
	    PS.RDB$COLLATION_ID = f_collation;
	    PS.RDB$COLLATION_ID.NULL = f_collation_null;
	END_STORE;
	if (!DYN_REQUEST (drq_s_prm_src))
	    DYN_REQUEST (drq_s_prm_src) = request2;
	id = drq_s_prms;
	}
END_STORE;

if (!DYN_REQUEST (drq_s_prms))
    DYN_REQUEST (drq_s_prms) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_procedure (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB    	  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb;
USHORT		  found, sql_prot;
VOLATILE SSHORT	  id;
TEXT		  procedure_name [PROC_NAME_SIZE], owner_name [32], *p;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

GET_STRING (ptr, procedure_name);

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

sql_prot = FALSE;
request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_prcs)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 134, NULL, NULL, NULL, NULL, NULL);
	/* msg 134: "STORE RDB$PROCEDURES failed" */
	}
    else if (id == drq_s_prc_usr_prvs)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 25, NULL, NULL, NULL, NULL, NULL);
	/* msg 25: "STORE RDB$USER_PRIVILEGES failed defining a relation" */
	}

    DYN_rundown_request (old_env, request, -1);
    if (id == drq_l_prc_name)
	DYN_error_punt (TRUE, 134, NULL, NULL, NULL, NULL, NULL);
	/* msg 134: "STORE RDB$PROCEDURES failed" */
	
    /* Control should never reach this point, because id should have
       one of the values tested-for above. */
    assert(0);
    DYN_error_punt (TRUE, 0, NULL, NULL, NULL, NULL, NULL);
    }

id = drq_l_prc_name;
check_unique_name (tdbb, gbl, procedure_name, TRUE);

request = (BLK) CMP_find_request (tdbb, drq_s_prcs, DYN_REQUESTS);
id = drq_s_prcs;
 

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	P IN RDB$PROCEDURES
    strcpy (P.RDB$PROCEDURE_NAME, procedure_name);
    P.RDB$SYSTEM_FLAG.NULL = TRUE;
    P.RDB$PROCEDURE_BLR.NULL = TRUE;
    P.RDB$PROCEDURE_SOURCE.NULL = TRUE;
    P.RDB$SECURITY_CLASS.NULL = TRUE;
    P.RDB$DESCRIPTION.NULL = TRUE;
    P.RDB$PROCEDURE_INPUTS.NULL = TRUE;
    P.RDB$PROCEDURE_OUTPUTS.NULL = TRUE;
	    
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_system_flag:
		P.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		P.RDB$SYSTEM_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_prc_blr:
		P.RDB$PROCEDURE_BLR.NULL = FALSE;
		DYN_put_blr_blob (gbl, ptr, &P.RDB$PROCEDURE_BLR);
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &P.RDB$DESCRIPTION);
		P.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &P.RDB$DESCRIPTION);
		P.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    case gds__dyn_prc_source:
		DYN_put_text_blob (gbl, ptr, &P.RDB$PROCEDURE_SOURCE);
		P.RDB$PROCEDURE_SOURCE.NULL = FALSE;
		break;

	    case gds__dyn_prc_inputs:
		P.RDB$PROCEDURE_INPUTS = DYN_get_number (ptr);
		P.RDB$PROCEDURE_INPUTS.NULL = FALSE;
		break;

	    case gds__dyn_prc_outputs:
		P.RDB$PROCEDURE_OUTPUTS = DYN_get_number (ptr);
		P.RDB$PROCEDURE_OUTPUTS.NULL = FALSE;
		break;

	    case gds__dyn_security_class:
		GET_STRING (ptr, P.RDB$SECURITY_CLASS);
		P.RDB$SECURITY_CLASS.NULL = FALSE;
		break;
	    
	    case gds__dyn_rel_sql_protection:
		sql_prot = DYN_get_number (ptr);
		break;

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr,
			  NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, procedure_name);
	    }

END_STORE;

if (!DYN_REQUEST (drq_s_prcs))
    DYN_REQUEST (drq_s_prcs) = request;

if (sql_prot)
    {
    if (get_who (tdbb, gbl, owner_name))
	DYN_error_punt (TRUE, 134, NULL, NULL, NULL, NULL, NULL);
	/* msg 134: "STORE RDB$PROCEDURES failed" */

    for (p = ALL_PROC_PRIVILEGES; *p; p++)
	{
	request = (BLK) CMP_find_request (tdbb, drq_s_prc_usr_prvs, DYN_REQUESTS);
	id = drq_s_prc_usr_prvs;

	STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		X IN RDB$USER_PRIVILEGES
	    strcpy (X.RDB$RELATION_NAME, procedure_name);
	    strcpy (X.RDB$USER, owner_name);
	    X.RDB$USER_TYPE = obj_user;
	    X.RDB$OBJECT_TYPE = obj_procedure;
	    X.RDB$PRIVILEGE [0] = *p;
	    X.RDB$PRIVILEGE [1] = 0;
	END_STORE;

	if (!DYN_REQUEST (drq_s_prc_usr_prvs))
	    DYN_REQUEST (drq_s_prc_usr_prvs) = request;
	}
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_relation (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
BLK		  old_request;
UCHAR		  verb;
USHORT		  found, sql_prot, is_a_view, priv;
VOLATILE SSHORT	  id, old_id;
STATUS		 *s;
TEXT		  relation_name [32], owner_name [32], field_name [32], *p;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

sql_prot = is_a_view = FALSE;
GET_STRING (ptr, relation_name);

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_rels)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 24, NULL, NULL, NULL, NULL, NULL);
	/* msg 24: "STORE RDB$RELATIONS failed" */
	}
    else if (id == drq_s_usr_prvs)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 25, NULL, NULL, NULL, NULL, NULL);
	/* msg 25: "STORE RDB$USER_PRIVILEGES failed defining a relation" */
	}

    DYN_rundown_request (old_env, request, -1);
    if (id == drq_l_rel_name)
	DYN_error_punt (TRUE, 24, NULL, NULL, NULL, NULL, NULL);
	/* msg 24: "STORE RDB$RELATIONS failed" */
    else if (id == drq_l_view_rels)
	DYN_error_punt (TRUE, 115, NULL, NULL, NULL, NULL, NULL);
	/* msg 115: "CREATE VIEW failed" */

    /* Control should never reach this point, because id should
       always have one of the values test-for above. */
    assert(0);
    DYN_error_punt (TRUE, 0, NULL, NULL, NULL, NULL, NULL);
    }

id = drq_l_rel_name;
check_unique_name (tdbb, gbl, relation_name, FALSE);
request = (BLK) CMP_find_request (tdbb, drq_s_rels, DYN_REQUESTS);
id = drq_s_rels;

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	REL IN RDB$RELATIONS
    strcpy (REL.RDB$RELATION_NAME, relation_name);
    REL.RDB$SYSTEM_FLAG.NULL = TRUE;
    REL.RDB$VIEW_BLR.NULL = TRUE;
    REL.RDB$VIEW_SOURCE.NULL = TRUE;
    REL.RDB$SECURITY_CLASS.NULL = TRUE;
    REL.RDB$DESCRIPTION.NULL = TRUE;
    REL.RDB$EXTERNAL_FILE.NULL = TRUE;
    REL.RDB$FLAGS = 0;
    REL.RDB$FLAGS.NULL = FALSE;
	    
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_system_flag:
		REL.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		REL.RDB$SYSTEM_FLAG.NULL = FALSE;
		break;

	    case gds__dyn_sql_object:
		REL.RDB$FLAGS |= REL_sql;
		break;

	    case gds__dyn_view_blr:
		REL.RDB$VIEW_BLR.NULL = FALSE;
		is_a_view = TRUE;
		DYN_put_blr_blob (gbl, ptr, &REL.RDB$VIEW_BLR);
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &REL.RDB$DESCRIPTION);
		REL.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &REL.RDB$DESCRIPTION);
		REL.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    case gds__dyn_view_source:
		DYN_put_text_blob (gbl, ptr, &REL.RDB$VIEW_SOURCE);
		REL.RDB$VIEW_SOURCE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_view_source2:
		DYN_put_text_blob2 (gbl, ptr, &REL.RDB$VIEW_SOURCE);
		REL.RDB$VIEW_SOURCE.NULL = FALSE;
		break;
#endif

	    case gds__dyn_security_class:
		GET_STRING (ptr, REL.RDB$SECURITY_CLASS);
		REL.RDB$SECURITY_CLASS.NULL = FALSE;
		break;
	    
	    case gds__dyn_rel_ext_file:
		GET_STRING (ptr, REL.RDB$EXTERNAL_FILE);
		if (ISC_check_if_remote (REL.RDB$EXTERNAL_FILE, FALSE))
		    DYN_error_punt (TRUE, 163, NULL, NULL, NULL, NULL, NULL);
	        ISC_expand_filename (REL.RDB$EXTERNAL_FILE, 
			strlen(REL.RDB$EXTERNAL_FILE), REL.RDB$EXTERNAL_FILE);
		REL.RDB$EXTERNAL_FILE.NULL = FALSE;
		break;

	    case gds__dyn_rel_sql_protection:
		REL.RDB$FLAGS |= REL_sql;
		sql_prot = DYN_get_number (ptr);
		break;

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, REL.RDB$RELATION_NAME, field_name,
			NULL_PTR, NULL_PTR, NULL_PTR);
	    }

    if (sql_prot)
	{
	if (get_who (tdbb, gbl, owner_name))
	    DYN_error_punt (TRUE, 115, NULL, NULL, NULL, NULL, NULL);
	    /* msg 115: "CREATE VIEW failed" */

	if (is_a_view)
	    {
	    old_request = request;
	    old_id = id;
	    request = (BLK) CMP_find_request (tdbb, drq_l_view_rels, DYN_REQUESTS);
	    id = drq_l_view_rels;

    	    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		VRL IN RDB$VIEW_RELATIONS CROSS
		PREL IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH
		VRL.RDB$VIEW_NAME EQ relation_name

		if (!DYN_REQUEST (drq_l_view_rels))
		    DYN_REQUEST (drq_l_view_rels) = request;

		if (strcmp (PREL.RDB$OWNER_NAME, owner_name))
		    {
		    if (DYN_UTIL_get_prot (tdbb, gbl, PREL.RDB$RELATION_NAME,
				  "", &priv))
			DYN_error_punt (TRUE, 115, NULL, NULL, NULL, NULL, NULL);
			/* msg 115: "CREATE VIEW failed" */

		    if (!(priv & SCL_read))
			{
			s = tdbb->tdbb_status_vector;
			*s++ = gds_arg_gds;
			*s++ = gds__no_priv;
			*s++ = gds_arg_string;
			*s++ = (STATUS) "SELECT";	/* Non-Translatable */
			*s++ = gds_arg_string;
			*s++ = (STATUS) "TABLE";	/* Non-Translatable */
			*s++ = gds_arg_string;
			*s++ = (STATUS) ERR_cstring (REL.RDB$RELATION_NAME);
			*s = 0;
			/* msg 32: no permission for %s access to %s %s */
			DYN_error_punt (TRUE, 115, NULL, NULL, NULL, NULL, NULL);
			/* msg 115: "CREATE VIEW failed" */
			}
		    }
	    END_FOR;
	    if (!DYN_REQUEST (drq_l_view_rels))
		DYN_REQUEST (drq_l_view_rels) = request;
	    request = old_request;
	    id = old_id;
	    }
	}
END_STORE;

if (!DYN_REQUEST (drq_s_rels))
    DYN_REQUEST (drq_s_rels) = request;

if (sql_prot)
    for (p = ALL_PRIVILEGES; *p; p++)
	{
	request = (BLK) CMP_find_request (tdbb, drq_s_usr_prvs, DYN_REQUESTS);
	id = drq_s_usr_prvs;

	STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		X IN RDB$USER_PRIVILEGES
	    strcpy (X.RDB$RELATION_NAME, relation_name);
	    strcpy (X.RDB$USER, owner_name);
	    X.RDB$USER_TYPE = obj_user;
	    X.RDB$OBJECT_TYPE = obj_relation;
	    X.RDB$PRIVILEGE [0] = *p;
	    X.RDB$PRIVILEGE [1] = 0;
	    X.RDB$GRANT_OPTION = 1;
	END_STORE;

	if (!DYN_REQUEST (drq_s_usr_prvs))
	    DYN_REQUEST (drq_s_usr_prvs) = request;
	}

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_security_class (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_classes, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	SC IN RDB$SECURITY_CLASSES
    GET_STRING (ptr, SC.RDB$SECURITY_CLASS);
    SC.RDB$ACL.NULL = TRUE;
    SC.RDB$DESCRIPTION.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_scl_acl:
		DYN_put_blr_blob (gbl, ptr, &SC.RDB$ACL);
		SC.RDB$ACL.NULL = FALSE;
		break;

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &SC.RDB$DESCRIPTION);
		SC.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &SC.RDB$DESCRIPTION);
		SC.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    default:
		DYN_unsupported_verb();
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_classes);
	DYN_error_punt (TRUE, 27, NULL, NULL, NULL, NULL, NULL);
	/* msg 27: "STORE RDB$RELATIONS failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_classes))
    DYN_REQUEST (drq_s_classes) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_sql_field (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ s q l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Define a local, SQL field.  This will require generation of
 *	an global field name.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
BLK		  old_request;
VOLATILE SSHORT	  id, old_id;
UCHAR		  verb;
USHORT		  dtype;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
SLONG		  fld_pos;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (id == drq_s_sql_lfld)
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 29, NULL, NULL, NULL, NULL, NULL);
	/* msg 29: "STORE RDB$RELATION_FIELDS failed" */
	}
    else
	{
	DYN_rundown_request (old_env, request, id);
	DYN_error_punt (TRUE, 28, NULL, NULL, NULL, NULL, NULL);
	/* msg 28: "STORE RDB$FIELDS failed" */
	}
    }

request = (BLK) CMP_find_request (tdbb, drq_s_sql_lfld, DYN_REQUESTS);
id = drq_s_sql_lfld;

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RFR IN RDB$RELATION_FIELDS
    GET_STRING (ptr, RFR.RDB$FIELD_NAME);
    if (field_name != NULL)
	strcpy (field_name, RFR.RDB$FIELD_NAME);
    if (relation_name)
	strcpy (RFR.RDB$RELATION_NAME, relation_name);
    RFR.RDB$SYSTEM_FLAG = 0;
    RFR.RDB$SYSTEM_FLAG.NULL = FALSE;
    RFR.RDB$QUERY_NAME.NULL = TRUE;
    RFR.RDB$QUERY_HEADER.NULL = TRUE;
    RFR.RDB$EDIT_STRING.NULL = TRUE;
    RFR.RDB$FIELD_POSITION.NULL = TRUE;
    RFR.RDB$VIEW_CONTEXT.NULL = TRUE;
    RFR.RDB$BASE_FIELD.NULL = TRUE;
    RFR.RDB$UPDATE_FLAG.NULL = TRUE;
    RFR.RDB$NULL_FLAG.NULL = TRUE;
    RFR.RDB$DEFAULT_SOURCE.NULL = TRUE;
    RFR.RDB$DEFAULT_VALUE.NULL = TRUE;
    RFR.RDB$COLLATION_ID.NULL = TRUE;

    old_request = request;
    old_id = id;
    request = (BLK) CMP_find_request (tdbb, drq_s_sql_gfld, DYN_REQUESTS);
    id = drq_s_sql_gfld;

    STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    FLD IN RDB$FIELDS
	FLD.RDB$SYSTEM_FLAG.NULL = TRUE;
	FLD.RDB$FIELD_SCALE.NULL = TRUE;
	FLD.RDB$FIELD_PRECISION.NULL = TRUE;
	FLD.RDB$FIELD_SUB_TYPE.NULL = TRUE;
	FLD.RDB$SEGMENT_LENGTH.NULL = TRUE;
	FLD.RDB$COMPUTED_BLR.NULL = TRUE;
	FLD.RDB$COMPUTED_SOURCE.NULL = TRUE;
	FLD.RDB$DEFAULT_VALUE.NULL = TRUE;
	FLD.RDB$DEFAULT_SOURCE.NULL = TRUE;
	FLD.RDB$VALIDATION_BLR.NULL = TRUE;
	FLD.RDB$VALIDATION_SOURCE.NULL = TRUE;
	FLD.RDB$NULL_FLAG.NULL = TRUE;
	FLD.RDB$EDIT_STRING.NULL = TRUE;
	FLD.RDB$DIMENSIONS.NULL = TRUE;
	FLD.RDB$CHARACTER_LENGTH.NULL = TRUE;
	FLD.RDB$CHARACTER_SET_ID.NULL = TRUE;
	FLD.RDB$COLLATION_ID.NULL = TRUE;

	DYN_UTIL_generate_field_name (tdbb, gbl, RFR.RDB$FIELD_SOURCE);
	strcpy (FLD.RDB$FIELD_NAME, RFR.RDB$FIELD_SOURCE);
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_rel_name:
		    GET_STRING (ptr, RFR.RDB$RELATION_NAME);
		    break;

		case gds__dyn_fld_query_name:
		    GET_STRING (ptr, RFR.RDB$QUERY_NAME);
		    RFR.RDB$QUERY_NAME.NULL = FALSE;
		    break;

		case gds__dyn_fld_edit_string:
		    GET_STRING (ptr, RFR.RDB$EDIT_STRING);
		    RFR.RDB$EDIT_STRING.NULL = FALSE;
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_fld_edit_string2:
		    DYN_get_string2 (ptr, RFR.RDB$EDIT_STRING, sizeof(RFR.RDB$EDIT_STRING));
		    RFR.RDB$EDIT_STRING.NULL = FALSE;
		    break;
#endif

		case gds__dyn_fld_position:
		    RFR.RDB$FIELD_POSITION = DYN_get_number (ptr);
		    RFR.RDB$FIELD_POSITION.NULL = FALSE;
		    break;

		case gds__dyn_view_context:
		    RFR.RDB$VIEW_CONTEXT = DYN_get_number (ptr);
		    RFR.RDB$VIEW_CONTEXT.NULL = FALSE;
		    break;

		case gds__dyn_system_flag:
		    RFR.RDB$SYSTEM_FLAG = FLD.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		    RFR.RDB$SYSTEM_FLAG.NULL = FLD.RDB$SYSTEM_FLAG.NULL = FALSE;
		    break;

		case gds__dyn_update_flag:
		    RFR.RDB$UPDATE_FLAG = DYN_get_number (ptr);
		    RFR.RDB$UPDATE_FLAG.NULL = FALSE;
		    break;

		case gds__dyn_fld_length:
		    FLD.RDB$FIELD_LENGTH = DYN_get_number (ptr);
		    break;

		case gds__dyn_fld_computed_blr:
		    FLD.RDB$COMPUTED_BLR.NULL = FALSE;
		    DYN_put_blr_blob (gbl, ptr, &FLD.RDB$COMPUTED_BLR);
		    break;

		case gds__dyn_fld_computed_source:
		    FLD.RDB$COMPUTED_SOURCE.NULL = FALSE;
		    DYN_put_text_blob (gbl, ptr, &FLD.RDB$COMPUTED_SOURCE);
		    break;

		case gds__dyn_fld_default_value:
		    RFR.RDB$DEFAULT_VALUE.NULL = FALSE;
		    DYN_put_blr_blob (gbl, ptr, &RFR.RDB$DEFAULT_VALUE);
		    break;

		case gds__dyn_fld_default_source:
		    RFR.RDB$DEFAULT_SOURCE.NULL = FALSE;
		    DYN_put_text_blob (gbl, ptr, &RFR.RDB$DEFAULT_SOURCE);
		    break;

		case gds__dyn_fld_validation_blr:
		    FLD.RDB$VALIDATION_BLR.NULL = FALSE;
		    DYN_put_blr_blob (gbl, ptr, &FLD.RDB$VALIDATION_BLR);
		    break;

		case gds__dyn_fld_not_null:
		    RFR.RDB$NULL_FLAG.NULL = FALSE;
		    RFR.RDB$NULL_FLAG = TRUE;
		    break;

		case gds__dyn_fld_query_header:
		    DYN_put_blr_blob (gbl, ptr, &RFR.RDB$QUERY_HEADER);
		    RFR.RDB$QUERY_HEADER.NULL = FALSE;
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_fld_query_header2:
		    DYN_put_blr_blob2 (gbl, ptr, &RFR.RDB$QUERY_HEADER);
		    RFR.RDB$QUERY_HEADER.NULL = FALSE;
		    break;
#endif

		case gds__dyn_fld_type:
		    FLD.RDB$FIELD_TYPE = dtype = DYN_get_number (ptr);
		    switch (dtype)
			{
			case blr_short :	
			    FLD.RDB$FIELD_LENGTH = 2; 
			    break;

			case blr_long :
			case blr_float:
			case blr_sql_date:
			case blr_sql_time:
			    FLD.RDB$FIELD_LENGTH = 4; 
			    break;

			case blr_int64:
			case blr_quad:
			case blr_timestamp:
			case blr_double:
			case blr_d_float:
			    FLD.RDB$FIELD_LENGTH = 8; 
			    break;
			
			default:
			    if (dtype == blr_blob)
				FLD.RDB$FIELD_LENGTH = 8; 
			    break;
			}
		    break;

		case gds__dyn_fld_scale:
		    FLD.RDB$FIELD_SCALE = DYN_get_number (ptr);
		    FLD.RDB$FIELD_SCALE.NULL = FALSE;
		    break;

		case isc_dyn_fld_precision:
		    FLD.RDB$FIELD_PRECISION = DYN_get_number (ptr);
		    FLD.RDB$FIELD_PRECISION.NULL = FALSE;
		    break;

		case gds__dyn_fld_sub_type:
		    FLD.RDB$FIELD_SUB_TYPE = DYN_get_number (ptr);
		    FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		    break;

	    	case gds__dyn_fld_char_length:
		    FLD.RDB$CHARACTER_LENGTH = DYN_get_number (ptr);
		    FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
		    break;

	    	case gds__dyn_fld_character_set:
		    FLD.RDB$CHARACTER_SET_ID = DYN_get_number (ptr);
		    FLD.RDB$CHARACTER_SET_ID.NULL = FALSE;
		    break;

	    	case gds__dyn_fld_collation:
		    /* Note: the global field's collation is not set, just 
		     *       the local field.  There is no full "domain" 
		     *       created for the local field.
		     *       This is the same decision for items like NULL_FLAG
		     */
		    RFR.RDB$COLLATION_ID = DYN_get_number (ptr);
		    RFR.RDB$COLLATION_ID.NULL = FALSE;
		    break;

		case gds__dyn_fld_dimensions:
		    FLD.RDB$DIMENSIONS = DYN_get_number (ptr);
		    FLD.RDB$DIMENSIONS.NULL = FALSE;
		    break;

		case gds__dyn_fld_segment_length:
		    FLD.RDB$SEGMENT_LENGTH = DYN_get_number (ptr);
		    FLD.RDB$SEGMENT_LENGTH.NULL = FALSE;
		    break;

		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, relation_name, RFR.RDB$FIELD_SOURCE,
				 NULL_PTR, NULL_PTR, NULL_PTR);
		}

    if (RFR.RDB$FIELD_POSITION.NULL == TRUE)
	{
	fld_pos = -1;
	DYN_UTIL_generate_field_position (tdbb, gbl, relation_name, &fld_pos);

	if (fld_pos >= 0)
	    {
	    RFR.RDB$FIELD_POSITION = ++fld_pos;
	    RFR.RDB$FIELD_POSITION.NULL = FALSE;
	    }
	}

    END_STORE;

    if (!DYN_REQUEST (drq_s_sql_gfld))
	DYN_REQUEST (drq_s_sql_gfld) = request;
    request = old_request;
    id = old_id;
END_STORE;

if (!DYN_REQUEST (drq_s_sql_lfld))
    DYN_REQUEST (drq_s_sql_lfld) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_shadow (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	Define a shadow.
 *
 **************************************/
SLONG		  shadow_number, start;
UCHAR		  verb;
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
BOOLEAN 	  found = FALSE;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

shadow_number = DYN_get_number (ptr);

/* If a shadow set identified by the
   shadow number already exists return error.  */ 

request = (BLK) CMP_find_request (tdbb, drq_l_shadow, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
	DYN_rundown_request (old_env, request, drq_l_shadow);
	DYN_error_punt (TRUE, 164, NULL, NULL, NULL, NULL, NULL);
	/* msg 164: "Shadow lookup failed" */
    }

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FIRST 1 X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER EQ shadow_number
    found = TRUE;
END_FOR;

if (!DYN_REQUEST (drq_l_shadow))
    DYN_REQUEST (drq_l_shadow) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (found)
    DYN_error_punt (FALSE, 165, (TEXT*) shadow_number, NULL, NULL, NULL, NULL);
    /* msg 165: "Shadow %ld already exists" */

start = 0;
while ((verb = *(*ptr)++) != gds__dyn_end)
    switch (verb)
	{
	case gds__dyn_def_file:
	    DYN_define_file (gbl, ptr, shadow_number, &start, 157);
	    break;

	default:
	    DYN_unsupported_verb();
	    }
}

void DYN_define_trigger (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*trigger_name,
    BOOLEAN     ignore_perm)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Define a trigger for a relation.
 *
 *
 * if the ignore_perm flag is TRUE, then this trigger must be defined
 * now (and fired at run time) without making SQL permissions checks.
 * In particular, one should not need control permissions on the table
 * to define this trigger. Currently used to define triggers for
 * cascading referential interity.
 * 
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb, *blr; 
TEXT		  t [32], *source;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

if (!GET_STRING (ptr, t))
    DYN_UTIL_generate_trigger_name (tdbb, gbl, t);

if (trigger_name != NULL)
    strcpy (trigger_name,t);

source = NULL;
blr = NULL;

request = (BLK) CMP_find_request (tdbb, drq_s_triggers, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGERS
    X.RDB$TRIGGER_TYPE.NULL = TRUE;
    X.RDB$TRIGGER_SEQUENCE = 0;
    X.RDB$TRIGGER_SEQUENCE.NULL = FALSE;
    X.RDB$TRIGGER_INACTIVE = 0;
    X.RDB$TRIGGER_INACTIVE.NULL = FALSE;

    /* currently, we make no difference between ignoring permissions in
       order to define this trigger and ignoring permissions checks when the
       trigger fires. The RDB$FLAGS is used to indicate permissions checks 
       when the trigger fires. Later, if we need to make a difference 
       between these, then the caller should pass the required value 
       of RDB$FLAGS as an extra argument to this func.  */

    X.RDB$FLAGS = ignore_perm ? TRG_ignore_perm : 0;
    X.RDB$FLAGS.NULL = FALSE;
    if (relation_name)
	{
	strcpy (X.RDB$RELATION_NAME, relation_name);
	X.RDB$RELATION_NAME.NULL = FALSE;
	}
    else
	X.RDB$RELATION_NAME.NULL = TRUE;
    X.RDB$TRIGGER_BLR.NULL = TRUE;
    X.RDB$TRIGGER_SOURCE.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;
    strcpy (X.RDB$TRIGGER_NAME, t);
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_trg_type:
		X.RDB$TRIGGER_TYPE = DYN_get_number (ptr);
		X.RDB$TRIGGER_TYPE.NULL = FALSE;
		break;
	    
	    case gds__dyn_sql_object:
		X.RDB$FLAGS |= TRG_sql;
		break;

	    case gds__dyn_trg_sequence:
		X.RDB$TRIGGER_SEQUENCE = DYN_get_number (ptr);
		break;
	    
	    case gds__dyn_trg_inactive:
		X.RDB$TRIGGER_INACTIVE = DYN_get_number (ptr);
		break;
	    
	    case gds__dyn_rel_name:
		GET_STRING (ptr, X.RDB$RELATION_NAME);
		X.RDB$RELATION_NAME.NULL = FALSE;
		break;

	    case gds__dyn_trg_blr:
		blr = *ptr;
		DYN_skip_attribute (ptr);
		DYN_put_blr_blob (gbl, &blr, &X.RDB$TRIGGER_BLR);
		X.RDB$TRIGGER_BLR.NULL = FALSE;
		break;

	    case gds__dyn_trg_source:
		source = (TEXT*) *ptr;
		DYN_skip_attribute (ptr);
		DYN_put_text_blob (gbl, &source, &X.RDB$TRIGGER_SOURCE);
		X.RDB$TRIGGER_SOURCE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_trg_source2:
		source = (TEXT*) *ptr;
		DYN_skip_attribute2 (ptr);
		DYN_put_text_blob2 (gbl, &source, &X.RDB$TRIGGER_SOURCE);
		X.RDB$TRIGGER_SOURCE.NULL = FALSE;
		break;
#endif

	    case gds__dyn_description:
		DYN_put_text_blob (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_description2:
		DYN_put_text_blob2 (gbl, ptr, &X.RDB$DESCRIPTION);
		X.RDB$DESCRIPTION.NULL = FALSE;
		break;
#endif

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, X.RDB$RELATION_NAME, NULL_PTR,
			t, NULL_PTR, NULL_PTR);
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_triggers);
	DYN_error_punt (TRUE, 31, NULL, NULL, NULL, NULL, NULL);
	/* msg 31: "DEFINE TRIGGER failed" */
	}

/* the sed script dyn_def.sed adds the foll. lines of code to the 
   dyn_def.c file.
   if (ignore_perm) 
       ((REQ)request)->req_flags |= req_ignore_perm; 
   after the request is compiled and before the request is sent.
   It makes the current request (to define the trigger) go through
   without checking any permissions lower in the engine */

MARKER_FOR_SED_SCRIPT
END_STORE;

if (ignore_perm)
	((REQ)request)->req_flags &= ~req_ignore_perm; 

if (!DYN_REQUEST (drq_s_triggers))
    DYN_REQUEST (drq_s_triggers) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_trigger_msg (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*trigger_name)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Define a trigger message.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb; 
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_trg_msgs, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGER_MESSAGES
    X.RDB$MESSAGE_NUMBER = DYN_get_number (ptr);
    X.RDB$MESSAGE.NULL = TRUE;
    if (trigger_name)
	{
	strcpy (X.RDB$TRIGGER_NAME, trigger_name);
	X.RDB$TRIGGER_NAME.NULL = FALSE;
	}
    else
	X.RDB$TRIGGER_NAME.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_trg_name:
		GET_STRING (ptr, X.RDB$TRIGGER_NAME);
		X.RDB$TRIGGER_NAME.NULL = FALSE;
		break;

	    case gds__dyn_trg_msg:
		GET_STRING (ptr, X.RDB$MESSAGE);
		X.RDB$MESSAGE.NULL = FALSE;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	    case gds__dyn_trg_msg2:
		DYN_get_string2 (ptr, X.RDB$MESSAGE, sizeof(X.RDB$MESSAGE));
		X.RDB$MESSAGE.NULL = FALSE;
		break;
#endif

	    default:
		DYN_unsupported_verb();
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_trg_msgs);
	DYN_error_punt (TRUE, 33, NULL, NULL, NULL, NULL, NULL);
	/* msg 33: "DEFINE TRIGGER MESSAGE failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_trg_msgs))
    DYN_REQUEST (drq_s_trg_msgs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_define_view_relation (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*view)
{
/**************************************
 *
 *	D Y N _ d e f i n e _ v i e w _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Store a RDB$VIEW_RELATION record.
 *
 **************************************/
TDBB		  tdbb;
DBB		  dbb;
VOLATILE BLK	  request;
UCHAR		  verb;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_view_rels, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	VRL IN RDB$VIEW_RELATIONS
    strcpy (VRL.RDB$VIEW_NAME, view);
    GET_STRING (ptr, VRL.RDB$RELATION_NAME);
    VRL.RDB$CONTEXT_NAME.NULL = TRUE;
    VRL.RDB$VIEW_CONTEXT.NULL = TRUE;
    while ((verb = *(*ptr)++) != gds__dyn_end)
	switch (verb)
	    {
	    case gds__dyn_view_context:
		VRL.RDB$VIEW_CONTEXT = DYN_get_number (ptr);
		VRL.RDB$VIEW_CONTEXT.NULL = FALSE;
		break;

	    case gds__dyn_view_context_name:
		GET_STRING (ptr, VRL.RDB$CONTEXT_NAME);
		VRL.RDB$CONTEXT_NAME.NULL = FALSE;
		break;

	    default:
		--(*ptr);
		DYN_execute (gbl, ptr, VRL.RDB$RELATION_NAME, NULL_PTR,
			NULL_PTR, NULL_PTR, NULL_PTR);
	    }

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_view_rels);
	DYN_error_punt (TRUE, 34, NULL, NULL, NULL, NULL, NULL);
	/* msg 34: "STORE RDB$VIEW_RELATIONS failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_view_rels))
    DYN_REQUEST (drq_s_view_rels) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

static void check_unique_name (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*object_name,
    BOOLEAN	proc_flag)
{
/**************************************
 *
 *	c h e c k _ u n i q u e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Check if a procedure, view or relation by
 *	name of object_name already exists.
 *	If yes then return error.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
BOOLEAN 	  found;
STATUS		  *s;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

request = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (!proc_flag)
	DYN_error_punt (TRUE, 24, NULL, NULL, NULL, NULL, NULL);
	/* msg 24: "STORE RDB$RELATIONS failed" */
    DYN_error_punt (TRUE, 134, NULL, NULL, NULL, NULL, NULL);
    }

request = (BLK) CMP_find_request (tdbb, drq_l_rel_name, DYN_REQUESTS);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	EREL IN RDB$RELATIONS WITH EREL.RDB$RELATION_NAME EQ object_name
    if (!DYN_REQUEST (drq_l_rel_name))
	DYN_REQUEST (drq_l_rel_name) = request;

    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_l_rel_name))
    DYN_REQUEST (drq_l_rel_name) = request;

if (found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 132, object_name, NULL, NULL, NULL, NULL);
    }

request = (BLK) CMP_find_request (tdbb, drq_l_prc_name, DYN_REQUESTS);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	EPRC IN RDB$PROCEDURES WITH EPRC.RDB$PROCEDURE_NAME EQ object_name
    if (!DYN_REQUEST (drq_l_prc_name))
	DYN_REQUEST (drq_l_prc_name) = request;

    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_l_prc_name))
    DYN_REQUEST (drq_l_prc_name) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (found)
    DYN_error_punt (FALSE, 135, object_name, NULL, NULL, NULL, NULL);
}

static BOOLEAN find_field_source (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*view_name,
    USHORT	context,
    TEXT	*local_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	f i n d _ f i e l d _ s o u r c e
 *
 **************************************
 *
 * Functional description
 *	Find the original source for a view field.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
BOOLEAN		  found;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_l_fld_src2, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 80, NULL, NULL, NULL, NULL, NULL);
    /* msg 80: "Specified domain or source field does not exist" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	VRL IN RDB$VIEW_RELATIONS CROSS
	RFR IN RDB$RELATION_FIELDS OVER RDB$RELATION_NAME
	WITH VRL.RDB$VIEW_NAME EQ view_name AND
	    VRL.RDB$VIEW_CONTEXT EQ context AND
	    RFR.RDB$FIELD_NAME EQ local_name 
    if (!DYN_REQUEST (drq_l_fld_src2))
	DYN_REQUEST (drq_l_fld_src2) = request;

    found = TRUE;
    DYN_terminate (RFR.RDB$FIELD_SOURCE, sizeof (RFR.RDB$FIELD_SOURCE));
    strcpy (field_name, RFR.RDB$FIELD_SOURCE);
END_FOR;
if (!DYN_REQUEST (drq_l_fld_src2))
    DYN_REQUEST (drq_l_fld_src2) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

return found;
}

static BOOLEAN get_who (
    TDBB	tdbb,
    GBL		gbl,
    SCHAR	*name)
{
/**************************************
 *
 *	g e t _ w h o
 *
 **************************************
 *
 * Functional description
 *	Get user name
 *
 **************************************/
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);

request = (BLK) CMP_find_request (tdbb, drq_l_user_name, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, drq_l_user_name);
    return FAILURE;
    }

if (!request)
    request = (BLK) CMP_compile2 (tdbb, who_blr, TRUE);
EXE_start (tdbb, request, gbl->gbl_transaction);
EXE_receive (tdbb, request, 0, 32, name);

DYN_rundown_request (old_env, request, drq_l_user_name);

return SUCCESS;
}

BOOLEAN is_it_user_name (
    GBL		gbl,
    TEXT 	*role_name,
    TDBB 	tdbb)
{
/**************************************
 *
 *	i s _ i t _ u s e r _ n a m e
 *
 **************************************
 *
 * Functional description
 *
 *     if role_name is user name returns TRUE. Otherwise returns FALSE
 *
 **************************************/
DBB           dbb;
VOLATILE BLK  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
BOOLEAN       found = FALSE;
VOLATILE USHORT	request_id;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;  /* save environment */
tdbb->tdbb_setjmp = (UCHAR*) env;        /* get new environment */
if (SETJMP (env))
    {
    if (request)
	DYN_rundown_request (old_env, request, request_id);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;	/* restore old env */
    ERR_punt();
    }

/* If there is a user with privilege or a grantor on a relation we
   can infer there is a user with this name */

request_id = drq_get_user_priv;
request = (BLK) CMP_find_request (tdbb, request_id, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    PRIV IN RDB$USER_PRIVILEGES WITH
        ( PRIV.RDB$USER        EQ role_name AND
          PRIV.RDB$USER_TYPE   =  obj_user ) OR
        ( PRIV.RDB$GRANTOR     EQ role_name AND
          PRIV.RDB$OBJECT_TYPE =  obj_relation )

    found = TRUE;

END_FOR;

if (!DYN_REQUEST (drq_get_user_priv))
    DYN_REQUEST (drq_get_user_priv) = request;

if (found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
    return found;
    }

/* We can infer that 'role_name' is a user name if it owns any relations
   Note we can only get here if a user creates a table and revokes all
   his privileges on the table */

request_id = drq_get_rel_owner;
request = (BLK) CMP_find_request (tdbb, request_id, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    REL IN RDB$RELATIONS WITH
        REL.RDB$OWNER_NAME  EQ role_name

    found = TRUE;

END_FOR;

if (!DYN_REQUEST (drq_get_rel_owner))
    DYN_REQUEST (drq_get_rel_owner) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;  /* restore environment */
return found;
}

