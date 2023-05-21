/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn_delete.e
 *	DESCRIPTION:	Dynamic data definition - DYN_delete_<x>
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

#include "../jrd/ibsetjmp.h"
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/scl.h"
#include "../jrd/drq.h"
#include "../jrd/flags.h"
#include "../jrd/gds.h"
#include "../jrd/lls.h"
#include "../jrd/all.h"
#include "../jrd/met.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/dyn.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/dyn_dl_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/vio_proto.h"

#ifdef SUPERSERVER
#define V4_THREADING
#endif
#include "../jrd/nlm_thd.h"

DATABASE
    DB = STATIC "yachts.gdb";

static	BOOLEAN	delete_constraint_records (GBL, TEXT *, TEXT *);
static	BOOLEAN	delete_dimension_records (GBL, TEXT *);
static	void	delete_f_key_constraint (TDBB, GBL, TEXT *, TEXT *,
                                                TEXT *, TEXT *);
static	void	delete_gfield_for_lfield (GBL, TEXT *);
static	BOOLEAN	delete_index_segment_records (GBL, TEXT *);
static	BOOLEAN	delete_security_class2 (GBL, TEXT *);

void DYN_delete_constraint (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ c o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *	Execute ddl to DROP an Integrity Constraint
 *
 **************************************/
TEXT	rel_name [32], constraint [32];

/* GET name of the constraint to be deleted  */

GET_STRING (ptr,constraint);

if (relation)
    strcpy (rel_name, relation);
else if (*(*ptr)++ != gds__dyn_rel_name)
    {
    DYN_error_punt (FALSE, 128, NULL, NULL, NULL, NULL, NULL);
    /* msg 128: "No relation specified in delete_constraint" */
    }
else
    GET_STRING (ptr, rel_name);

if (!delete_constraint_records (gbl, constraint, rel_name))
    DYN_error_punt (FALSE, 130, constraint, NULL, NULL, NULL, NULL);
    /* msg 130: "CONSTRAINT %s does not exist." */
}

void DYN_delete_dimensions (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Delete dimensions associated with
 *	a global field.  Used when modifying
 *	the datatype and from places where a
 *	field is deleted directly in the system
 *	relations.  The DYN version of delete
 *	global field deletes the dimensions for
 *	you.
 *
 **************************************/
TEXT	f [32];

GET_STRING (ptr, f);

delete_dimension_records (gbl, f);

while (*(*ptr)++ != gds__dyn_end)
    {
    --(*ptr);
    DYN_execute (gbl, ptr, NULL_PTR, f, NULL_PTR, NULL_PTR, NULL_PTR);
    }
}

void DYN_delete_exception (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes an exception.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
TEXT	t [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

GET_STRING (ptr, t);

request = (BLK) CMP_find_request (tdbb, drq_e_xcp, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 143, NULL, NULL, NULL, NULL, NULL);
    /* msg 143: "ERASE EXCEPTION failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$EXCEPTIONS
	WITH X.RDB$EXCEPTION_NAME EQ t
    if (!DYN_REQUEST (drq_e_xcp))
	DYN_REQUEST (drq_e_xcp) = request;

    found = TRUE;
    ERASE X;
END_FOR;
if (!DYN_REQUEST (drq_e_xcp))
    DYN_REQUEST (drq_e_xcp) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 144, NULL, NULL, NULL, NULL, NULL);
    /* msg 144: "Exception not found" */
}

void DYN_delete_filter (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a blob filter.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
TEXT	f [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_filters, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 36, NULL, NULL, NULL, NULL, NULL);
    /* msg 36: "ERASE BLOB FILTER failed" */
    }

GET_STRING (ptr, f);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FILTERS WITH X.RDB$FUNCTION_NAME = f
    if (!DYN_REQUEST (drq_e_filters))
	DYN_REQUEST (drq_e_filters) = request;

    ERASE X;
    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_e_filters))
    DYN_REQUEST (drq_e_filters) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 37, NULL, NULL, NULL, NULL, NULL);
    /* msg 37: "Blob Filter not found" */

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_function (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a user defined function.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
VOLATILE USHORT	id, found;
TEXT	f [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_func_args, DYN_REQUESTS);
id = drq_e_func_args;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_e_func_args)
	DYN_error_punt (TRUE, 39, NULL, NULL, NULL, NULL, NULL);
	/* msg 39: "ERASE RDB$FUNCTION_ARGUMENTS failed" */
    else
	DYN_error_punt (TRUE, 40, NULL, NULL, NULL, NULL, NULL);
	/* msg 40: "ERASE RDB$FUNCTIONS failed" */
    }

GET_STRING (ptr, f);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FA IN RDB$FUNCTION_ARGUMENTS WITH FA.RDB$FUNCTION_NAME EQ f
    if (!DYN_REQUEST (drq_e_func_args))
	DYN_REQUEST (drq_e_func_args) = request;

    ERASE FA;
END_FOR;
if (!DYN_REQUEST (drq_e_func_args))
    DYN_REQUEST (drq_e_func_args) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_funcs, DYN_REQUESTS);
id = drq_e_funcs;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FUNCTIONS WITH X.RDB$FUNCTION_NAME EQ f
    if (!DYN_REQUEST (drq_e_funcs))
	DYN_REQUEST (drq_e_funcs) = request;

    ERASE X;
    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_e_funcs))
    DYN_REQUEST (drq_e_funcs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 41, NULL, NULL, NULL, NULL, NULL);
    /* msg 41: "Function not found" */

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_global_field (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a global field.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
VOLATILE USHORT	id, found;
TEXT	f [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_l_fld_src, DYN_REQUESTS);
id = drq_l_fld_src;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_l_fld_src)
	DYN_error_punt (TRUE, 44, NULL, NULL, NULL, NULL, NULL);
	/* msg 44: "ERASE RDB$FIELDS failed" */
    else
	DYN_error_punt (TRUE, 45, NULL, NULL, NULL, NULL, NULL);
	/* msg 45: "ERASE RDB$FIELDS failed" */
    }

GET_STRING (ptr, f);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	Y IN RDB$RELATION_FIELDS WITH Y.RDB$FIELD_SOURCE EQ f
    if (!DYN_REQUEST (drq_l_fld_src))
	DYN_REQUEST (drq_l_fld_src) = request;

    DYN_terminate (Y.RDB$FIELD_SOURCE, sizeof (Y.RDB$FIELD_SOURCE));
    DYN_terminate (Y.RDB$RELATION_NAME, sizeof (Y.RDB$RELATION_NAME));
    DYN_terminate (Y.RDB$FIELD_NAME, sizeof (Y.RDB$FIELD_NAME));
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (FALSE, 43, Y.RDB$FIELD_SOURCE, Y.RDB$RELATION_NAME, Y.RDB$FIELD_NAME, NULL, NULL);
    /* msg 43: "field %s is used in relation %s (local name %s) and can not be dropped" */
END_FOR;
if (!DYN_REQUEST (drq_l_fld_src))
    DYN_REQUEST (drq_l_fld_src) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_gfields, DYN_REQUESTS);
id = drq_e_gfields;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FIELDS WITH X.RDB$FIELD_NAME EQ f
    if (!DYN_REQUEST (drq_e_gfields))
	DYN_REQUEST (drq_e_gfields) = request;

    delete_dimension_records (gbl, f);
    ERASE X;
    found = TRUE;
END_FOR
if (!DYN_REQUEST (drq_e_gfields))
    DYN_REQUEST (drq_e_gfields) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    {
    DYN_error_punt (FALSE, 46, NULL, NULL, NULL, NULL, NULL);
    /* msg 46: "Field not found" */
    }

while (*(*ptr)++ != gds__dyn_end)
    {
    --(*ptr);
    DYN_execute (gbl, ptr, NULL_PTR, f, NULL_PTR, NULL_PTR, NULL_PTR);
    }
}

void DYN_delete_index (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes an index.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
TEXT	i [32], r [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_indices, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 47, NULL, NULL, NULL, NULL, NULL);
    /* msg 47: "ERASE RDB$INDICES failed" */
    }

GET_STRING (ptr, i);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	IDX IN RDB$INDICES WITH IDX.RDB$INDEX_NAME EQ i
    if (!DYN_REQUEST (drq_e_indices))
	DYN_REQUEST (drq_e_indices) = request;

    strcpy (r, IDX.RDB$RELATION_NAME);
    found = TRUE;
    ERASE IDX;
END_FOR;
if (!DYN_REQUEST (drq_e_indices))
    DYN_REQUEST (drq_e_indices) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 48, NULL, NULL, NULL, NULL, NULL);
    /* msg 48: "Index not found" */

if (!delete_index_segment_records (gbl, i))
    DYN_error_punt (FALSE, 50, NULL, NULL, NULL, NULL, NULL);
    /* msg 50: "No segments found for index" */

while (*(*ptr)++ != gds__dyn_end)
    {
    --(*ptr);
    DYN_execute (gbl, ptr, r, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    }
}

void DYN_delete_local_field (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ l o c a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl 'delete local field'
 *	statement.
 *
 *  The rules for dropping a regular column:
 *  
 *    1. the column is not referenced in any views.
 *    2. the column is not part of any user defined indexes.
 *    3. the column is not used in any SQL statements inside of store
 *         procedures or triggers
 *    4. the column is not part of any check-constraints
 *  
 *  The rules for dropping a column that was created as primary key:
 *
 *    1. the column is not defined as any foreign keys
 *    2. the column is not defined as part of compound primary keys
 *
 *  The rules for dropping a column that was created as foreign key:
 *
 *    1. the column is not defined as a compound foreign key. A
 *         compound foreign key is a foreign key consisted of more
 *         than one columns.
 *
 *  The RI enforcement for dropping primary key column is done by system
 *  triggers and the RI enforcement for dropping foreign key column is
 *  done by code and system triggers. See the functional description of
 *  delete_f_key_constraint function for detail.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request, old_request;
VOLATILE USHORT	id, found;
TEXT	tbl_nm [32], col_nm [32], i [32], constraint [32], index_name [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

GET_STRING (ptr, col_nm);

if (relation_name)
    strcpy (tbl_nm, relation_name);
else if (*(*ptr)++ != gds__dyn_rel_name)
    {
    DYN_error_punt (FALSE, 51, NULL, NULL, NULL, NULL, NULL);
    /* msg 51: "No relation specified in ERASE RFR" */
    }
else
    GET_STRING (ptr, tbl_nm);

request = (BLK) CMP_find_request (tdbb, drq_l_dep_flds, DYN_REQUESTS);
id = drq_l_dep_flds;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_l_dep_flds)
	DYN_error_punt (TRUE, 53, NULL, NULL, NULL, NULL, NULL);
	/* msg 53: "ERASE RDB$RELATION_FIELDS failed" */
    else
	DYN_error_punt (TRUE, 54, NULL, NULL, NULL, NULL, NULL);
	/* msg 54: "ERASE RDB$RELATION_FIELDS failed" */
    }

/*
**  ================================================================
**  ==
**  ==  make sure that column is not referenced in any views
**  ==
**  ================================================================
*/
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$RELATION_FIELDS CROSS Y IN RDB$RELATION_FIELDS CROSS
	Z IN RDB$VIEW_RELATIONS WITH
	X.RDB$RELATION_NAME EQ tbl_nm AND
	X.RDB$FIELD_NAME EQ col_nm AND
	X.RDB$FIELD_NAME EQ Y.RDB$BASE_FIELD AND
	X.RDB$FIELD_SOURCE EQ Y.RDB$FIELD_SOURCE AND
	Y.RDB$RELATION_NAME EQ Z.RDB$VIEW_NAME AND
	X.RDB$RELATION_NAME EQ Z.RDB$RELATION_NAME AND
	Y.RDB$VIEW_CONTEXT EQ Z.RDB$VIEW_CONTEXT
    if (!DYN_REQUEST (drq_l_dep_flds))
	DYN_REQUEST (drq_l_dep_flds) = request;

    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (FALSE, 52, col_nm, tbl_nm, Y.RDB$RELATION_NAME, NULL, NULL);
    /* msg 52: "field %s from relation %s is referenced in view %s" */
END_FOR;
if (!DYN_REQUEST (drq_l_dep_flds))
    DYN_REQUEST (drq_l_dep_flds) = request;

/*
**  ===============================================================
**  ==
**  ==  If the column to be dropped is being used as a foreign key
**  ==  and the coulmn was not part of any compound foreign key,
**  ==  then we can drop the column. But we have to drop the foreign key
**  ==  constraint first.
**  ==
**  ===============================================================
*/

request = (BLK) CMP_find_request (tdbb, drq_g_rel_constr_nm, DYN_REQUESTS);
id = drq_g_rel_constr_nm;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
        IDX       IN RDB$INDICES        CROSS
        IDX_SEG   IN RDB$INDEX_SEGMENTS CROSS
        REL_CONST IN RDB$RELATION_CONSTRAINTS
                 WITH IDX.RDB$RELATION_NAME         EQ tbl_nm
                  AND REL_CONST.RDB$RELATION_NAME   EQ tbl_nm
                  AND IDX_SEG.RDB$FIELD_NAME        EQ col_nm
                  AND IDX.RDB$INDEX_NAME            EQ IDX_SEG.RDB$INDEX_NAME
                  AND IDX.RDB$INDEX_NAME            EQ REL_CONST.RDB$INDEX_NAME
                  AND REL_CONST.RDB$CONSTRAINT_TYPE EQ FOREIGN_KEY

    if (!DYN_REQUEST (drq_g_rel_constr_nm))
        DYN_REQUEST (drq_g_rel_constr_nm) = request;

    if (IDX.RDB$SEGMENT_COUNT == 1)
       {
       DYN_terminate (REL_CONST.RDB$CONSTRAINT_NAME,
                      sizeof (REL_CONST.RDB$CONSTRAINT_NAME));
       strcpy (constraint, REL_CONST.RDB$CONSTRAINT_NAME);

       DYN_terminate (IDX.RDB$INDEX_NAME, sizeof (IDX.RDB$INDEX_NAME));
       strcpy (index_name, IDX.RDB$INDEX_NAME);

       delete_f_key_constraint (tdbb, gbl, tbl_nm, col_nm,
                                constraint, index_name);
       }
    else
       {
       DYN_rundown_request (old_env, request, -1);
       DYN_error_punt (FALSE, 187, col_nm, tbl_nm,
                       IDX.RDB$INDEX_NAME, NULL, NULL);
       /* msg 187: "field %s from relation %s is referenced in index %s" */
       }

END_FOR;

if (!DYN_REQUEST (drq_g_rel_constr_nm))
    DYN_REQUEST (drq_g_rel_constr_nm) = request;

/*
**  ================================================================
**  ==
**  ==  make sure that column is not referenced in any indexes
**  ==
**  ==  NOTE: You still could see the system generated indices even though
**  ==        they were already been deleted when dropping column that was 
**  ==        used as foreign key before "commit".
**  ==
**  ================================================================
*/

request = (BLK) CMP_find_request (tdbb, drq_e_l_idx, DYN_REQUESTS);
id = drq_e_l_idx;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
        IDX IN RDB$INDICES
                 WITH IDX.RDB$RELATION_NAME EQ tbl_nm
    if (!DYN_REQUEST (drq_e_l_idx))
        DYN_REQUEST (drq_e_l_idx) = request;

    found = FALSE;
    if (strncmp ("RDB$", IDX.RDB$INDEX_NAME, 4) != 0)
        {
    old_request = request;

    id = drq_l_idx_seg;
    request = (BLK) CMP_find_request (tdbb, drq_l_idx_seg, DYN_REQUESTS);
    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
        FIRST 1 IDX_SEG IN RDB$INDEX_SEGMENTS
                WITH IDX_SEG.RDB$INDEX_NAME EQ IDX.RDB$INDEX_NAME AND
                     IDX_SEG.RDB$FIELD_NAME = col_nm
        if (!DYN_REQUEST (drq_l_idx_seg))
            DYN_REQUEST (drq_l_idx_seg) = request;
        found = TRUE;
    END_FOR;
    if (!DYN_REQUEST (drq_l_idx_seg))
        DYN_REQUEST (drq_l_idx_seg) = request;
    request = old_request;
    id = drq_e_l_idx;
        }
    if (found)
        {
	DYN_rundown_request (old_env, request, -1);
	DYN_error_punt (FALSE, 187, col_nm, tbl_nm, IDX.RDB$INDEX_NAME, 
                        NULL, NULL);
	/* msg 187: "field %s from relation %s is referenced in index %s" */
        }
END_FOR;
if (!DYN_REQUEST (drq_e_l_idx))
    DYN_REQUEST (drq_e_l_idx) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_lfield, DYN_REQUESTS);
id = drq_e_lfield;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RFR IN RDB$RELATION_FIELDS
	WITH RFR.RDB$FIELD_NAME    EQ col_nm
         AND RFR.RDB$RELATION_NAME EQ tbl_nm
    if (!DYN_REQUEST (drq_e_lfield))
	DYN_REQUEST (drq_e_lfield) = request;

    ERASE RFR;
    found = TRUE;
    delete_gfield_for_lfield (gbl, RFR.RDB$FIELD_SOURCE);
    while (*(*ptr)++ != gds__dyn_end)
	{
	--(*ptr);
	DYN_execute (gbl, ptr, RFR.RDB$RELATION_NAME, 
			RFR.RDB$FIELD_SOURCE, NULL_PTR, NULL_PTR, NULL_PTR);
	}
END_FOR;
if (!DYN_REQUEST (drq_e_lfield))
    DYN_REQUEST (drq_e_lfield) = request;

if (!found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 55, NULL, NULL, NULL, NULL, NULL);
    }
    /* msg 55: "Field not found for relation" */

tdbb->tdbb_setjmp = (UCHAR*) old_env;

} 

void DYN_delete_parameter (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*proc_name)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a stored procedure parameter.
 *
 **************************************/
TDBB    tdbb;
DBB	dbb;
BLK	old_request, request;
VOLATILE USHORT	old_id, id, found;
TEXT	name [32];
JMP_BUF	env, *old_env;

GET_STRING (ptr, name);
if (**ptr == gds__dyn_prc_name)
    DYN_get_string (++ptr, proc_name, PROC_NAME_SIZE, TRUE);

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_prm, DYN_REQUESTS);
id = drq_e_prms;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_e_prms)
	{
	DYN_error_punt (TRUE, 138, NULL, NULL, NULL, NULL, NULL);
	/* msg 138: "ERASE RDB$PROCEDURE_PARAMETERS failed" */
	}
    else 
	{
	DYN_error_punt (TRUE, 35, NULL, NULL, NULL, NULL, NULL);
	/* msg 35: "ERASE RDB$FIELDS failed" */
	}
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	PP IN RDB$PROCEDURE_PARAMETERS WITH PP.RDB$PROCEDURE_NAME EQ proc_name
	AND PP.RDB$PARAMETER_NAME EQ name
    if (!DYN_REQUEST (drq_e_prm))
	DYN_REQUEST (drq_e_prm) = request;
    found = TRUE;

    /* get rid of parameters in rdb$fields */

    if (!PP.RDB$FIELD_SOURCE.NULL)
	{
	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_d_gfields, DYN_REQUESTS);
	id = drq_d_gfields;

	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		FLD IN RDB$FIELDS 
		WITH FLD.RDB$FIELD_NAME EQ PP.RDB$FIELD_SOURCE

	    if (!DYN_REQUEST (drq_d_gfields))
		DYN_REQUEST (drq_d_gfields) = request;

	    ERASE FLD;
	END_FOR;

	if (!DYN_REQUEST (drq_d_gfields))
	    DYN_REQUEST (drq_d_gfields) = request;

	request = old_request;
	id = old_id;
	}
    ERASE PP;

END_FOR;
if (!DYN_REQUEST (drq_e_prm))
    DYN_REQUEST (drq_e_prm) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (!found)
    DYN_error_punt (FALSE, 146, name, proc_name, NULL, NULL, NULL);
    /* msg 146: "Parameter %s in procedure %s not found" */

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_procedure (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a stored procedure.
 *
 **************************************/
TDBB    tdbb;
DBB	dbb;
BLK	old_request, request;
VOLATILE USHORT	old_id, id, found;
TEXT	name [32];
JMP_BUF	env, *old_env;

GET_STRING (ptr, name);
tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

tdbb->tdbb_flags |= TDBB_prc_being_dropped;
if ( MET_lookup_procedure (tdbb, name) == NULL)
    {
    tdbb->tdbb_flags &= ~TDBB_prc_being_dropped;
    DYN_error_punt (FALSE, 140, name, NULL, NULL, NULL, NULL);
    /* msg 140: "Procedure %s not found" */
    }

tdbb->tdbb_flags &= ~TDBB_prc_being_dropped;

request = (BLK) CMP_find_request (tdbb, drq_e_prms, DYN_REQUESTS);
id = drq_e_prms;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_e_prms)
	DYN_error_punt (TRUE, 138, NULL, NULL, NULL, NULL, NULL);
	/* msg 138: "ERASE RDB$PROCEDURE_PARAMETERS failed" */
    else if (id == drq_e_prcs)
	DYN_error_punt (TRUE, 139, NULL, NULL, NULL, NULL, NULL);
	/* msg 139: "ERASE RDB$PROCEDURES failed" */
    else if (id == drq_d_gfields)
	DYN_error_punt (TRUE, 35, NULL, NULL, NULL, NULL, NULL);
	/* msg 35: "ERASE RDB$FIELDS failed" */
    else
	DYN_error_punt (TRUE, 62, NULL, NULL, NULL, NULL, NULL);
	/* msg 62: "ERASE RDB$USER_PRIVILEGES failed" */
    }


FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	PP IN RDB$PROCEDURE_PARAMETERS WITH PP.RDB$PROCEDURE_NAME EQ name

    if (!DYN_REQUEST (drq_e_prms))
	DYN_REQUEST (drq_e_prms) = request;

    /* get rid of parameters in rdb$fields */

    if (!PP.RDB$FIELD_SOURCE.NULL)
	{
	old_request = request;
	old_id = id;
	request = (BLK) CMP_find_request (tdbb, drq_d_gfields, DYN_REQUESTS);
	id = drq_d_gfields;

	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		FLD IN RDB$FIELDS 
		WITH FLD.RDB$FIELD_NAME EQ PP.RDB$FIELD_SOURCE

	    if (!DYN_REQUEST (drq_d_gfields))
		DYN_REQUEST (drq_d_gfields) = request;

	    ERASE FLD;
	END_FOR;

	if (!DYN_REQUEST (drq_d_gfields))
	    DYN_REQUEST (drq_d_gfields) = request;

	request = old_request;
	id = old_id;
	}

    ERASE PP;
END_FOR;
if (!DYN_REQUEST (drq_e_prms))
    DYN_REQUEST (drq_e_prms) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_prcs, DYN_REQUESTS);
id = drq_e_prcs;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_NAME EQ name

    if (!DYN_REQUEST (drq_e_prcs))
	DYN_REQUEST (drq_e_prcs) = request;
    if (!P.RDB$SECURITY_CLASS.NULL)
	delete_security_class2 (gbl, P.RDB$SECURITY_CLASS);
    ERASE P;
    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_e_prcs))
    DYN_REQUEST (drq_e_prcs) = request;

if (!found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 140, name, NULL, NULL, NULL, NULL);
    /* msg 140: "Procedure %s not found" */
    }

request = (BLK) CMP_find_request (tdbb, drq_e_prc_prvs, DYN_REQUESTS);
id = drq_e_prc_prvs;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	PRIV IN RDB$USER_PRIVILEGES WITH PRIV.RDB$RELATION_NAME EQ name
	AND PRIV.RDB$OBJECT_TYPE = obj_procedure
    if (!DYN_REQUEST (drq_e_prc_prvs))
	DYN_REQUEST (drq_e_prc_prvs) = request;

    ERASE PRIV;
END_FOR;
if (!DYN_REQUEST (drq_e_prc_prvs))
    DYN_REQUEST (drq_e_prc_prvs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_relation (
    GBL		gbl,
    UCHAR	**ptr, 
    TEXT	*relation)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a relation with all its indices, triggers
 *	and fields.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
VOLATILE USHORT	id, found;
TEXT	relation_name [32], security_class [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

if (relation)
    strcpy (relation_name, relation);
else
    GET_STRING (ptr, relation_name);

request = (BLK) CMP_find_request (tdbb, drq_e_rel_con2, DYN_REQUESTS);
id = drq_e_rel_con2;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_e_rel_con2)
	DYN_error_punt (TRUE, 129, NULL, NULL, NULL, NULL, NULL);
	/* msg 129: "ERASE RDB$RELATION_CONSTRAINTS failed" */
    else if (id == drq_e_rel_idxs)
	DYN_error_punt (TRUE, 57, NULL, NULL, NULL, NULL, NULL);
	/* msg 57: "ERASE RDB$INDICES failed" */
    else if (id == drq_e_trg_msgs2)
	DYN_error_punt (TRUE, 65, NULL, NULL, NULL, NULL, NULL);
	/* msg 65: "ERASE RDB$TRIGGER_MESSAGES failed" */
    else if (id == drq_e_trigger2)
	DYN_error_punt (TRUE, 66, NULL, NULL, NULL, NULL, NULL);
	/* msg 66: "ERASE RDB$TRIGGERS failed" */
    else if (id == drq_e_rel_flds)
	DYN_error_punt (TRUE, 58, NULL, NULL, NULL, NULL, NULL);
	/* msg 58: "ERASE RDB$RELATION_FIELDS failed" */
    else if (id == drq_e_view_rels)
	DYN_error_punt (TRUE, 59, NULL, NULL, NULL, NULL, NULL);
	/* msg 59: "ERASE RDB$VIEW_RELATIONS failed" */
    else if (id == drq_e_relation)
	DYN_error_punt (TRUE, 60, NULL, NULL, NULL, NULL, NULL);
	/* msg 60: "ERASE RDB$RELATIONS failed" */
    else if (id == drq_e_rel_con2)
	DYN_error_punt (TRUE, 129, NULL, NULL, NULL, NULL, NULL);
	/* msg 129: "ERASE RDB$RELATION_CONSTRAINTS failed" */
    else if (id == drq_e_sec_class)
	DYN_error_punt (TRUE, 74, NULL, NULL, NULL, NULL, NULL);
	/* msg 74: "ERASE RDB$SECURITY_CLASSES failed" */
    else
	DYN_error_punt (TRUE, 62, NULL, NULL, NULL, NULL, NULL);
	/* msg 62: "ERASE RDB$USER_PRIVILEGES failed" */
    }

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	CRT IN RDB$RELATION_CONSTRAINTS
	WITH CRT.RDB$RELATION_NAME EQ relation_name AND
	(CRT.RDB$CONSTRAINT_TYPE EQ PRIMARY_KEY OR
	 CRT.RDB$CONSTRAINT_TYPE EQ UNIQUE_CNSTRT OR
	 CRT.RDB$CONSTRAINT_TYPE EQ FOREIGN_KEY)
	SORTED BY ASCENDING CRT.RDB$CONSTRAINT_TYPE
    if (!DYN_REQUEST (drq_e_rel_con2))
	DYN_REQUEST (drq_e_rel_con2) = request;

   ERASE CRT;
END_FOR;
if (!DYN_REQUEST (drq_e_rel_con2))
    DYN_REQUEST (drq_e_rel_con2) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_rel_idxs, DYN_REQUESTS);
id = drq_e_rel_idxs;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	IDX IN RDB$INDICES WITH IDX.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_rel_idxs))
	DYN_REQUEST (drq_e_rel_idxs) = request;

    delete_index_segment_records (gbl, IDX.RDB$INDEX_NAME);
    ERASE IDX;
END_FOR;
if (!DYN_REQUEST (drq_e_rel_idxs))
    DYN_REQUEST (drq_e_rel_idxs) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_trg_msgs2, DYN_REQUESTS);
id = drq_e_trg_msgs2;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	TM IN RDB$TRIGGER_MESSAGES CROSS
	T IN RDB$TRIGGERS WITH T.RDB$RELATION_NAME EQ relation_name AND
	TM.RDB$TRIGGER_NAME EQ T.RDB$TRIGGER_NAME
    if (!DYN_REQUEST (drq_e_trg_msgs2))
	DYN_REQUEST (drq_e_trg_msgs2) = request;

    ERASE TM;
END_FOR;
if (!DYN_REQUEST (drq_e_trg_msgs2))
    DYN_REQUEST (drq_e_trg_msgs2) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_rel_flds, DYN_REQUESTS);
id = drq_e_rel_flds;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RFR IN RDB$RELATION_FIELDS WITH RFR.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_rel_flds))
	DYN_REQUEST (drq_e_rel_flds) = request;

    if (!RFR.RDB$SECURITY_CLASS.NULL && !strncmp (RFR.RDB$SECURITY_CLASS, "SQL$", 4))
	delete_security_class2 (gbl, RFR.RDB$SECURITY_CLASS);

    ERASE RFR;

    delete_gfield_for_lfield (gbl, RFR.RDB$FIELD_SOURCE);

END_FOR;
if (!DYN_REQUEST (drq_e_rel_flds))
    DYN_REQUEST (drq_e_rel_flds) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_view_rels, DYN_REQUESTS);
id = drq_e_view_rels;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	VR IN RDB$VIEW_RELATIONS WITH VR.RDB$VIEW_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_view_rels))
	DYN_REQUEST (drq_e_view_rels) = request;

    ERASE VR;
END_FOR;
if (!DYN_REQUEST (drq_e_view_rels))
    DYN_REQUEST (drq_e_view_rels) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_relation, DYN_REQUESTS);
id = drq_e_relation;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_relation))
	DYN_REQUEST (drq_e_relation) = request;

    if (!R.RDB$SECURITY_CLASS.NULL && !strncmp (R.RDB$SECURITY_CLASS, "SQL$", 4))
	delete_security_class2 (gbl, R.RDB$SECURITY_CLASS);

    ERASE R;
    found = TRUE;

END_FOR;
if (!DYN_REQUEST (drq_e_relation))
    DYN_REQUEST (drq_e_relation) = request;

if (!found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 61, NULL, NULL, NULL, NULL, NULL);
    /* msg 61: "Relation not found" */
    }

request = (BLK) CMP_find_request (tdbb, drq_e_rel_con3, DYN_REQUESTS);
id = drq_e_rel_con3;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	CRT IN RDB$RELATION_CONSTRAINTS WITH 
	CRT.RDB$RELATION_NAME EQ relation_name AND
	(CRT.RDB$CONSTRAINT_TYPE EQ CHECK_CNSTRT OR
	 CRT.RDB$CONSTRAINT_TYPE EQ NOT_NULL_CNSTRT)
    if (!DYN_REQUEST (drq_e_rel_con3))
	DYN_REQUEST (drq_e_rel_con3) = request;

    ERASE CRT;
END_FOR;
if (!DYN_REQUEST (drq_e_rel_con3))
    DYN_REQUEST (drq_e_rel_con3) = request;

/* Triggers must be deleted after check constraints   */

request = (BLK) CMP_find_request (tdbb, drq_e_trigger2, DYN_REQUESTS);
id = drq_e_trigger2;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGERS WITH X.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_trigger2))
	DYN_REQUEST (drq_e_trigger2) = request;

    ERASE X;
END_FOR;
if (!DYN_REQUEST (drq_e_trigger2))
    DYN_REQUEST (drq_e_trigger2) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_usr_prvs, DYN_REQUESTS);
id = drq_e_usr_prvs;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	PRIV IN RDB$USER_PRIVILEGES WITH PRIV.RDB$RELATION_NAME EQ relation_name
	AND PRIV.RDB$OBJECT_TYPE = obj_relation
    if (!DYN_REQUEST (drq_e_usr_prvs))
	DYN_REQUEST (drq_e_usr_prvs) = request;

    ERASE PRIV;
END_FOR;
if (!DYN_REQUEST (drq_e_usr_prvs))
    DYN_REQUEST (drq_e_usr_prvs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

while (*(*ptr)++ != gds__dyn_end)
    {
    --(*ptr);
    DYN_execute (gbl, ptr, relation_name, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    }
}

void DYN_delete_security_class (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a security class.
 *
 **************************************/
TEXT	security_class [32];

GET_STRING (ptr, security_class);

if (!delete_security_class2 (gbl, security_class))
    DYN_error_punt (FALSE, 75, NULL, NULL, NULL, NULL, NULL);
    /* msg 75: "Security class not found" */

while (*(*ptr)++ != gds__dyn_end)
    {
    --(*ptr);
    DYN_execute (gbl, ptr, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    }
}

void DYN_delete_shadow (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	Delete a shadow.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
int	shadow_number;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

/*****
the code commented out in this routine was meant to delete the
shadow files, however this cannot be done until the transaction
is commited, which we cannot do automatically as the user may
choose to roll it back.
LLS	files;
STR	file;

files = NULL;
*****/

request = (BLK) CMP_find_request (tdbb, drq_e_shadow, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 63, NULL, NULL, NULL, NULL, NULL);
    /* msg 63: "ERASE RDB$FILES failed" */
    }

shadow_number = DYN_get_number (ptr);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FIL IN RDB$FILES WITH FIL.RDB$SHADOW_NUMBER EQ shadow_number
    if (!DYN_REQUEST (drq_e_shadow))
	DYN_REQUEST (drq_e_shadow) = request;

    ERASE FIL;
/****
    file = (STR) ALLOCDV (type_str, sizeof (FIL.RDB$FILE_NAME) - 1);
    strcpy (file->str_data, FIL.RDB$FILE_NAME);
    LLS_PUSH (file, &files);
****/
END_FOR;
if (!DYN_REQUEST (drq_e_shadow))
    DYN_REQUEST (drq_e_shadow) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

/****
while (files)
    {
    file = LLS_POP (&files);
    unlink (file->str_data);
    ALL_release (file);
    }
****/

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_trigger (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes an trigger.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	id, found;
TEXT	r [32], t [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_trg_msgs, DYN_REQUESTS);
id = drq_e_trg_msgs;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_e_trg_msgs)
	DYN_error_punt (TRUE, 65, NULL, NULL, NULL, NULL, NULL);
	/* msg 65: "ERASE RDB$TRIGGER_MESSAGES failed" */
    else if (id == drq_e_trigger)
	DYN_error_punt (TRUE, 66, NULL, NULL, NULL, NULL, NULL);
	/* msg 66: "ERASE RDB$TRIGGERS failed" */
    else
	DYN_error_punt (TRUE, 68, NULL, NULL, NULL, NULL, NULL);
	/* msg 68: "MODIFY RDB$VIEW_RELATIONS failed" */
    }

GET_STRING (ptr, t);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	TM IN RDB$TRIGGER_MESSAGES WITH TM.RDB$TRIGGER_NAME EQ t
    if (!DYN_REQUEST (drq_e_trg_msgs))
	DYN_REQUEST (drq_e_trg_msgs) = request;

    ERASE TM;
END_FOR;
if (!DYN_REQUEST (drq_e_trg_msgs))
    DYN_REQUEST (drq_e_trg_msgs) = request;

request = (BLK) CMP_find_request (tdbb, drq_e_trigger, DYN_REQUESTS);
id = drq_e_trigger;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGERS WITH X.RDB$TRIGGER_NAME EQ t
    if (!DYN_REQUEST (drq_e_trigger))
	DYN_REQUEST (drq_e_trigger) = request;

    gds__vtov (X.RDB$RELATION_NAME, r, sizeof (r));
    ERASE X;
    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_e_trigger))
    DYN_REQUEST (drq_e_trigger) = request;

if (!found)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (FALSE, 67, NULL, NULL, NULL, NULL, NULL);
    /* msg 67: "Trigger not found" */
    }

/* clear the update flags on the fields if this is the last remaining
   trigger that changes a view */

request = (BLK) CMP_find_request (tdbb, drq_l_view_rel2, DYN_REQUESTS);
id = drq_l_view_rel2;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FIRST 1 V IN RDB$VIEW_RELATIONS  
	CROSS F IN RDB$RELATION_FIELDS CROSS T IN RDB$TRIGGERS
	WITH V.RDB$VIEW_NAME EQ r AND
	F.RDB$RELATION_NAME EQ V.RDB$VIEW_NAME AND
	F.RDB$RELATION_NAME EQ T.RDB$RELATION_NAME
    if (!DYN_REQUEST (drq_l_view_rel2))
	DYN_REQUEST (drq_l_view_rel2) = request;

    found = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_l_view_rel2))
    DYN_REQUEST (drq_l_view_rel2) = request;

if (!found)
    {
    request = (BLK) CMP_find_request (tdbb, drq_m_rel_flds, DYN_REQUESTS);
    id = drq_m_rel_flds;

    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    F IN RDB$RELATION_FIELDS WITH F.RDB$RELATION_NAME EQ r
	if (!DYN_REQUEST (drq_m_rel_flds))
	    DYN_REQUEST (drq_m_rel_flds) = request;

	MODIFY F USING
	    F.RDB$UPDATE_FLAG = FALSE;
	END_MODIFY;
    END_FOR;
    if (!DYN_REQUEST (drq_m_rel_flds))
	DYN_REQUEST (drq_m_rel_flds) = request;
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

void DYN_delete_trigger_msg (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*trigger_name)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes an trigger message.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
int	number;
TEXT	t [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

number = DYN_get_number (ptr);
if (trigger_name)
    strcpy (t, trigger_name);
else if (*(*ptr)++ == gds__dyn_trg_name)
    GET_STRING (ptr, t);
else
    DYN_error_punt (FALSE, 70, NULL, NULL, NULL, NULL, NULL);
    /* msg 70: "TRIGGER NAME expected" */

request = (BLK) CMP_find_request (tdbb, drq_e_trg_msg, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 71, NULL, NULL, NULL, NULL, NULL);
    /* msg 71: "ERASE TRIGGER MESSAGE failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGER_MESSAGES
	WITH X.RDB$TRIGGER_NAME EQ t AND X.RDB$MESSAGE_NUMBER EQ number
    if (!DYN_REQUEST (drq_e_trg_msg))
	DYN_REQUEST (drq_e_trg_msg) = request;

    found = TRUE;
    ERASE X;
END_FOR;
if (!DYN_REQUEST (drq_e_trg_msg))
    DYN_REQUEST (drq_e_trg_msg) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 72, NULL, NULL, NULL, NULL, NULL);
    /* msg 72: "Trigger Message not found" */

if (*(*ptr)++ != gds__dyn_end)
    DYN_unsupported_verb();
}

static	BOOLEAN delete_constraint_records (
    GBL		gbl,
    TEXT	*constraint_name,
    TEXT	*relation_name)
{
/**************************************
 *
 *	d e l e t e _ c o n s t r a i n t _ r e c o r d s
 *
 **************************************
 *
 * Functional description
 *	Delete a record from RDB$RELATION_CONSTRAINTS
 *	based on a constraint name.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
BOOLEAN	found;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_rel_con, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 129, NULL, NULL, NULL, NULL, NULL);
    /* msg 129: "ERASE RDB$RELATION_CONSTRAINTS failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RC IN RDB$RELATION_CONSTRAINTS
	WITH RC.RDB$CONSTRAINT_NAME EQ constraint_name AND
	     RC.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_e_rel_con))
	DYN_REQUEST (drq_e_rel_con) = request;

    found = TRUE;
    ERASE RC;
END_FOR;
if (!DYN_REQUEST (drq_e_rel_con))
    DYN_REQUEST (drq_e_rel_con) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

return found;
}

static	BOOLEAN delete_dimension_records (
    GBL		gbl,
    TEXT	*field_name)
{
/**************************************
 *
 *	d e l e t e _ d i m e n s i o n s _ r e c o r d s
 *
 **************************************
 *
 * Functional description
 *	Delete the records in RDB$FIELD_DIMENSIONS
 *	pertaining to a field.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
BOOLEAN	found;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_dims, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 35, NULL, NULL, NULL, NULL, NULL);
    /* msg 35: "ERASE RDB$FIELDS failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FIELD_DIMENSIONS WITH X.RDB$FIELD_NAME EQ field_name
    if (!DYN_REQUEST (drq_e_dims))
	DYN_REQUEST (drq_e_dims) = request;

    found = TRUE;
    ERASE X;
END_FOR;
if (!DYN_REQUEST (drq_e_dims))
    DYN_REQUEST (drq_e_dims) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

return found;
}

static	void delete_f_key_constraint (
    TDBB        tdbb,
    GBL		gbl,
    TEXT	*tbl_nm,
    TEXT        *col_nm,
    TEXT        *constraint_nm,
    TEXT        *index_name)
{
/**************************************
 *
 *	d e l e t e _ f _ k e y _ c o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a record from RDB$RELATION_CONSTRAINTS based on a constraint_nm
 *  
 *      On deleting from RDB$RELATION_CONSTRAINTS, 2 system triggers fire:
 *  
 *      (A) pre delete trigger: pre_delete_constraint, will:
 *  
 *        1. delete a record first from RDB$REF_CONSTRAINTS where
 *           RDB$REF_CONSTRAINTS.RDB$CONSTRAINT_NAME =  
 *                               RDB$RELATION_CONSTRAINTS.RDB$CONSTRAINT_NAME
 *  
 *      (B) post delete trigger: post_delete_constraint will:
 *  
 *        1. also delete a record from RDB$INDICES where
 *           RDB$INDICES.RDB$INDEX_NAME =
 *                               RDB$RELATION_CONSTRAINTS.RDB$INDEX_NAME
 *  
 *        2. also delete a record from RDB$INDEX_SEGMENTS where
 *           RDB$INDEX_SEGMENTS.RDB$INDEX_NAME =
 *                               RDB$RELATION_CONSTRAINTS.RDB$INDEX_NAME
 *  
 **************************************/
DBB	dbb;
BLK	request;
BOOLEAN found;
VOLATILE USHORT id;
JMP_BUF env, *old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_rel_const, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
     DYN_rundown_request (old_env, request, -1);
     DYN_error_punt (TRUE, 129, NULL, NULL, NULL, NULL, NULL);
        /* msg 49: "ERASE RDB$RELATION_CONSTRAINTS failed" */
    }
 
found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
        RC IN RDB$RELATION_CONSTRAINTS
        WITH RC.RDB$CONSTRAINT_NAME EQ constraint_nm
         AND RC.RDB$CONSTRAINT_TYPE EQ FOREIGN_KEY
         AND RC.RDB$RELATION_NAME   EQ tbl_nm
         AND RC.RDB$INDEX_NAME      EQ index_name
    if (!DYN_REQUEST (drq_e_rel_const))
        DYN_REQUEST (drq_e_rel_const) = request;
 
    found = TRUE;
    ERASE RC;
END_FOR;
if (!DYN_REQUEST (drq_e_rel_const))
    DYN_REQUEST (drq_e_rel_const) = request;

if (!found)
   DYN_error_punt (FALSE, 130, constraint_nm, NULL, NULL, NULL, NULL);
   /* msg 130: "CONSTRAINT %s does not exist." */

tdbb->tdbb_setjmp = (UCHAR*) old_env;

}

static	void delete_gfield_for_lfield (
    GBL		gbl,
    TEXT	*lfield_name)
{
/**************************************
 *
 *	d e l e t e _ g f i e l d _ f o r _ l f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement that
 *	deletes a global field for a given local field.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;
request = (BLK) CMP_find_request (tdbb, drq_e_l_gfld, DYN_REQUESTS);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	    FLD IN RDB$FIELDS 
	    WITH FLD.RDB$FIELD_NAME EQ lfield_name
	    AND FLD.RDB$VALIDATION_SOURCE MISSING AND
	    FLD.RDB$NULL_FLAG MISSING AND
	    FLD.RDB$DEFAULT_SOURCE MISSING AND
	    FLD.RDB$FIELD_NAME STARTING WITH "RDB$" AND
	    NOT ANY RFR IN RDB$RELATION_FIELDS WITH
		RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME
	if (!DYN_REQUEST (drq_e_l_gfld))
	    DYN_REQUEST (drq_e_l_gfld) = request;

   delete_dimension_records (gbl, FLD.RDB$FIELD_NAME);
   ERASE FLD;
END_FOR;

if (!DYN_REQUEST (drq_e_l_gfld))
    DYN_REQUEST (drq_e_l_gfld) = request;

}

static	BOOLEAN delete_index_segment_records (
    GBL		gbl,
    TEXT	*index_name)
{
/**************************************
 *
 *	d e l e t e _ i n d e x _ s e g m e n t _ r e c o r d s
 *
 **************************************
 *
 * Functional description
 *	Delete the records in RDB$INDEX_SEGMENTS
 *	pertaining to an index.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
BOOLEAN	found;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_idx_segs, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 49, NULL, NULL, NULL, NULL, NULL);
    /* msg 49: "ERASE RDB$INDEX_SEGMENTS failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	I_S IN RDB$INDEX_SEGMENTS WITH I_S.RDB$INDEX_NAME EQ index_name
    if (!DYN_REQUEST (drq_e_idx_segs))
	DYN_REQUEST (drq_e_idx_segs) = request;

    found = TRUE;
    ERASE I_S;
END_FOR;
if (!DYN_REQUEST (drq_e_idx_segs))
    DYN_REQUEST (drq_e_idx_segs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

return found;
}

static	BOOLEAN delete_security_class2 (
    GBL		gbl,
    TEXT	*security_class)
{
/**************************************
 *
 *	d e l e t e _ s e c u r i t y _ c l a s s 2
 *
 **************************************
 *
 * Functional description
 *	Utility routine for delete_security_class(),
 *	which takes a string as input.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
BOOLEAN	found;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_e_class, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 74, NULL, NULL, NULL, NULL, NULL);
    /* msg 74: "ERASE RDB$SECURITY_CLASSES failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	SC IN RDB$SECURITY_CLASSES WITH SC.RDB$SECURITY_CLASS EQ security_class

    if (!DYN_REQUEST (drq_e_class))
	DYN_REQUEST (drq_e_class) = request;

    found = TRUE;
    ERASE SC;

END_FOR;
if (!DYN_REQUEST (drq_e_class))
    DYN_REQUEST (drq_e_class) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

return found;
}
