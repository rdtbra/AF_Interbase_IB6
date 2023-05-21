/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn.e
 *	DESCRIPTION:	Dynamic data definition
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
#include "../jrd/ods.h"
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
#include "../jrd/license.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/dyn_df_proto.h"
#include "../jrd/dyn_dl_proto.h"
#include "../jrd/dyn_md_proto.h"
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

#define BLANK	'\040'

DATABASE
    DB = STATIC "yachts.gdb";


static void	grant (GBL, UCHAR **);
static BOOLEAN  grantor_can_grant_role (TDBB, GBL, TEXT*, TEXT*);
static BOOLEAN  grantor_can_grant (GBL, TEXT*, TEXT*, TEXT*, TEXT*, BOOLEAN);
static void	revoke (GBL, UCHAR **);
static void 	store_privilege (GBL, TEXT *, TEXT *,TEXT *, TEXT *, SSHORT, SSHORT, int);

void DYN_ddl (
    ATT		attachment,
    TRA		transaction,
    USHORT	length,
    UCHAR	*ddl)
{
/**************************************
 *
 *	D Y N _ d d l
 *
 **************************************
 *
 * Functional description
 *	Do meta-data.
 *
 **************************************/
TDBB		tdbb;
#ifdef V4_THREADING
DBB		dbb;
#endif
STATUS		*status;
UCHAR		*ptr;
PLB		old_pool;
struct gbl	gbl;
JMP_BUF		env, *old_env;
#ifdef _PPC_
JMP_BUF		env1;
#endif

tdbb = GET_THREAD_DATA;
#ifdef V4_THREADING
dbb = tdbb->tdbb_database;
#endif

ptr = ddl;

if (*ptr++ != gds__dyn_version_1)
    ERR_post (gds__wrodynver, 0);

status = tdbb->tdbb_status_vector;
*status++ = gds_arg_gds;
*status++ = 0;
*status = gds_arg_end;

gbl.gbl_transaction = transaction;

/* Create a pool for DYN to operate in.  It will be released when
   the routine exits. */

old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = ALL_pool();

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    if (transaction->tra_save_point &&
	transaction->tra_save_point->sav_verb_count)
	{
	/* An error during undo is very bad and has to be dealt with
	   by aborting the transaction.  The easiest way is to kill the
	   application by calling bugcheck. */
#ifdef _PPC_
        tdbb->tdbb_setjmp = (UCHAR*) env1;
	if (!SETJMP (env1))
#else
	if (!SETJMP (env))
#endif
	    VIO_verb_cleanup (tdbb, transaction);
	else
	    {
	    tdbb->tdbb_setjmp = (UCHAR*) old_env; /* prepare for BUGCHECK's longjmp */
	    BUGCHECK (290); /* msg 290 error during savepoint backout */
	    }
	}

#if     ( defined( SUPERSERVER) && defined( WIN_NT))
    THD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
#else
    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
#endif

    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    ALL_rlpool (tdbb->tdbb_default);
    tdbb->tdbb_default = old_pool;
    ERR_punt();
    }

#if     ( defined( SUPERSERVER) && defined( WIN_NT))
THREAD_EXIT;
THD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
THREAD_ENTER;
#else
V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
#endif

VIO_start_save_point (tdbb, transaction);
transaction->tra_save_point->sav_verb_count++;

DYN_execute (&gbl, &ptr, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
transaction->tra_save_point->sav_verb_count--;
VIO_verb_cleanup (tdbb, transaction);

#if     ( defined( SUPERSERVER) && defined( WIN_NT))
THD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
#else
V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_dyn);
#endif

tdbb->tdbb_setjmp = (UCHAR*) old_env;
ALL_rlpool (tdbb->tdbb_default);
tdbb->tdbb_default = old_pool;
}

void DYN_delete_role (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ d e l e t e _ r o l e
 *
 **************************************
 *
 * Functional description
 *
 *	Execute a dynamic ddl statement that deletes a role with all its 
 *      members of the role.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
VOLATILE USHORT	id, err_num;
TEXT	role_name [32], security_class [32], role_owner [32], user [32];
TEXT    *ptr1, *ptr2;
JMP_BUF	env, *old_env;
BOOLEAN del_role_ok = TRUE;
USHORT  major_version, minor_original;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
    {
    DYN_error (FALSE, 196, NULL, NULL, NULL, NULL, NULL);
    ERR_punt();
    }
else
    {

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    if (id == drq_drop_role)
	DYN_error_punt (TRUE, 191, NULL, NULL, NULL, NULL, NULL);
	/* msg 191: "ERASE RDB$ROLES failed" */
    else
	DYN_error_punt (TRUE, 62, NULL, NULL, NULL, NULL, NULL);
	/* msg 62: "ERASE RDB$USER_PRIVILEGES failed" */
    }

for (ptr1 = tdbb->tdbb_attachment->att_user->usr_user_name, ptr2 = user; 
     *ptr1; ptr1++, ptr2++)
    *ptr2 = UPPER7 (*ptr1);

*ptr2 = '\0';

GET_STRING (ptr, role_name);

request = (BLK) CMP_find_request (tdbb, drq_drop_role, DYN_REQUESTS);
id = drq_drop_role;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	XX IN RDB$ROLES WITH 
    XX.RDB$ROLE_NAME EQ role_name

    if (!DYN_REQUEST (drq_drop_role))
	DYN_REQUEST (drq_drop_role) = request;

    DYN_terminate (XX.RDB$OWNER_NAME, sizeof (XX.RDB$OWNER_NAME));
    strcpy (role_owner, XX.RDB$OWNER_NAME);

    if ((tdbb->tdbb_attachment->att_user->usr_flags & USR_locksmith) ||
        (strcmp (role_owner, user) == 0))
        {
        ERASE XX;
        }
    else
        {
        del_role_ok = FALSE;
        }

END_FOR;

if (!DYN_REQUEST (drq_drop_role))
    DYN_REQUEST (drq_drop_role) = request;

if (del_role_ok)
    {
    request = (BLK) CMP_find_request (tdbb, drq_del_role_1, DYN_REQUESTS);
    id = drq_del_role_1;


    /* The first OR clause Finds all members of the role
       The 2nd OR clause  Finds all privileges granted to the role */ 
    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
        PRIV IN RDB$USER_PRIVILEGES WITH 
            ( PRIV.RDB$RELATION_NAME EQ role_name AND 
              PRIV.RDB$OBJECT_TYPE = obj_sql_role )
	 OR ( PRIV.RDB$USER EQ role_name AND
              PRIV.RDB$USER_TYPE = obj_sql_role )

        if (!DYN_REQUEST (drq_del_role_1))
            DYN_REQUEST (drq_del_role_1) = request;

        ERASE PRIV;

    END_FOR;

    if (!DYN_REQUEST (drq_del_role_1))
        DYN_REQUEST (drq_del_role_1) = request;

    }
else
    {
    /****************************************************
    **
    **  only owner of SQL role or USR_locksmith could drop SQL role
    **
    *****************************************************/
    DYN_error (FALSE, 191, user, role_name, NULL, NULL, NULL);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    ERR_punt();
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
    }
}

void DYN_error (
    USHORT	status_flag,
    USHORT	number,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3,
    TEXT	*arg4,
    TEXT	*arg5)
{
/**************************************
 *
 *	D Y N _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	DDL failed.
 *
 **************************************/
TDBB	tdbb;
STATUS	local_status [20], *v1, *v2, *end, arg;
TEXT	*error_buffer;


tdbb = GET_THREAD_DATA;

if (tdbb->tdbb_status_vector [1] == gds__no_meta_update)
    return;

error_buffer = (TEXT *)gds__alloc ((SLONG)(sizeof (TEXT) * BUFFER_MEDIUM));

if (number)
    gds__msg_format (NULL_PTR, DYN_MSG_FAC, number, sizeof (TEXT) * BUFFER_MEDIUM,
		error_buffer, arg1, arg2, arg3, arg4, arg5);

v1 = local_status;

*v1++ = gds_arg_gds;
*v1++ = gds__no_meta_update;
if (number)
    {
    *v1++ = gds_arg_gds;
    *v1++ = gds__random;
    *v1++ = gds_arg_string;
    *v1++ = (STATUS) ERR_cstring (error_buffer);
    }
if (status_flag) 
    {
    v2 = tdbb->tdbb_status_vector;

    /* check every other argument for end of vector */

    end = local_status + sizeof (local_status) - 1;
    while (v1 < end &&
	((arg = *v2++) != gds_arg_cstring || v1+1 < end) &&
	(*v1++ = arg))
	{
	*v1++ = *v2++;
	if (arg == gds_arg_cstring)
	    *v1++ = *v2++;
	}
    }
*v1 = gds_arg_end;
end = v1 + 1;

for (v1 = local_status, v2 = tdbb->tdbb_status_vector; v1 < end;)
    *v2++ = *v1++;

gds__free ((SLONG *)error_buffer);
}

void DYN_error_punt (
    USHORT	status_flag,
    USHORT	number,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3,
    TEXT	*arg4,
    TEXT	*arg5)
{
/**************************************
 *
 *	D Y N _ e r r o r _ p u n t
 *
 **************************************
 *
 * Functional description
 *	DDL failed.
 *
 **************************************/

DYN_error (status_flag, number, arg1, arg2, arg3, arg4, arg5);
ERR_punt();
}

BOOLEAN DYN_is_it_sql_role (
    GBL		gbl,
    TEXT	*input_name,
    TEXT	*output_name,
    TDBB	tdbb)
{
/**************************************
 *
 *	D Y N _ i s _ i t _ s q l _ r o l e
 *
 **************************************
 *
 * Functional description
 *
 *	If input_name is found in RDB$ROLES, then returns TRUE. Otherwise
 *    returns FALSE.
 *
 **************************************/
DBB         dbb;
BLK         request;
BOOLEAN     found;
USHORT  major_version, minor_original;

SET_TDBB (tdbb);
dbb     = tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;
found          = FALSE;

if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
    return found;

request = (BLK) CMP_find_request (tdbb, drq_get_role_nm, DYN_REQUESTS);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    X IN RDB$ROLES WITH
    X.RDB$ROLE_NAME EQ input_name

    if (!DYN_REQUEST (drq_get_role_nm))
	DYN_REQUEST (drq_get_role_nm) = request;

    found = TRUE;
    DYN_terminate (X.RDB$OWNER_NAME, sizeof (X.RDB$OWNER_NAME));
    strcpy (output_name, X.RDB$OWNER_NAME);

END_FOR;

if (!DYN_REQUEST (drq_get_role_nm))
    DYN_REQUEST (drq_get_role_nm) = request;

return found;
}

void DYN_unsupported_verb (void)
{
/**************************************
 *
 *	D Y N _ u n s u p p o r t e d _ v e r b
 *
 **************************************
 *
 * Functional description
 *	We encountered an unsupported dyn verb.
 *
 **************************************/

DYN_error_punt (FALSE, 2, NULL, NULL, NULL, NULL, NULL); /* msg 2: "unsupported DYN verb" */
}

void DYN_execute (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name,
    TEXT	*trigger_name,
    TEXT	*function_name,
    TEXT	*procedure_name)
{
/**************************************
 *
 *	D Y N _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
UCHAR	verb;

switch (verb = *(*ptr)++)
    {
    case gds__dyn_begin:
	while (**ptr != gds__dyn_end)
	    DYN_execute (gbl, ptr, relation_name, field_name,
			 trigger_name, function_name, procedure_name);
	++(*ptr);
	break;

    /* Runtime security-related dynamic DDL should not require licensing.
       A placeholder case statement for SQL 3 Roles is reserved below. */
	
    case gds__dyn_grant:
	grant (gbl, ptr);
	break;

    case gds__dyn_revoke:
	revoke (gbl, ptr);
	break;

/***
    case gds__dyn_def_role:
        create_role (gbl, ptr);
	break;
***/	
    default:
	/* make sure that the license allows metadata operations */
	
	switch (verb)
	    {
    	    case gds__dyn_mod_database:
		DYN_modify_database (gbl, ptr);
		break;

    	    case gds__dyn_def_rel:
    	    case gds__dyn_def_view:
		DYN_define_relation (gbl, ptr);
		break;

    	    case gds__dyn_mod_rel:
		DYN_modify_relation (gbl, ptr);
		break;
    
    	    case gds__dyn_delete_rel:
		DYN_delete_relation (gbl, ptr, relation_name);
		break;

    	    case gds__dyn_def_security_class:
		DYN_define_security_class (gbl, ptr);
		break;
    
    	    case gds__dyn_delete_security_class:
		DYN_delete_security_class (gbl, ptr);
		break;
    
    	    case gds__dyn_def_exception:
		DYN_define_exception (gbl, ptr);
		break;

    	    case gds__dyn_mod_exception:
		DYN_modify_exception (gbl, ptr);
		break;

            case gds__dyn_del_exception:
		DYN_delete_exception (gbl, ptr);
		break;

    	    case gds__dyn_def_filter:
		DYN_define_filter (gbl, ptr);
		break;

    	    case gds__dyn_delete_filter:
		DYN_delete_filter (gbl, ptr);
		break;

    	    case gds__dyn_def_function:
		DYN_define_function (gbl, ptr);
		break;

    	    case gds__dyn_def_function_arg:
		DYN_define_function_arg (gbl, ptr, function_name);
		break;

    	    case gds__dyn_delete_function:
		DYN_delete_function (gbl, ptr);
		break;

    	    case gds__dyn_def_generator:
		DYN_define_generator (gbl, ptr);
		break;

    	    case isc_dyn_def_sql_role:
                DYN_define_role (gbl, ptr);
                break;

    	    case isc_dyn_del_sql_role:
                DYN_delete_role (gbl, ptr);
                break;

    	    case gds__dyn_def_procedure:
		DYN_define_procedure (gbl, ptr);
		break;

    	    case gds__dyn_mod_procedure:
		DYN_modify_procedure (gbl, ptr);
		break;

    	    case gds__dyn_delete_procedure:
		DYN_delete_procedure (gbl, ptr);
		break;

    	    case gds__dyn_def_parameter:
		DYN_define_parameter (gbl, ptr, procedure_name);
		break;

      	    case gds__dyn_delete_parameter:
		DYN_delete_parameter (gbl, ptr, procedure_name);
		break;

    	    case gds__dyn_def_shadow:
		DYN_define_shadow (gbl, ptr);
		break;

    	    case gds__dyn_delete_shadow:
		DYN_delete_shadow (gbl, ptr);
		break;

    	    case gds__dyn_def_trigger:
		DYN_define_trigger (gbl, ptr, relation_name, NULL, FALSE);
		break;

    	    case gds__dyn_mod_trigger:
		DYN_modify_trigger (gbl, ptr);
		break;

      	    case gds__dyn_delete_trigger:
		DYN_delete_trigger (gbl, ptr);
		break;

    	    case gds__dyn_def_trigger_msg:
		DYN_define_trigger_msg (gbl, ptr, trigger_name);
		break;

    	    case gds__dyn_mod_trigger_msg:
		DYN_modify_trigger_msg (gbl, ptr, trigger_name);
		break;

    	    case gds__dyn_delete_trigger_msg:
		DYN_delete_trigger_msg (gbl, ptr, trigger_name);
		break;

    	    case gds__dyn_def_global_fld:
		DYN_define_global_field (gbl, ptr, relation_name, field_name);
		break;
    
    	    case gds__dyn_mod_global_fld:
		DYN_modify_global_field (gbl, ptr, relation_name, field_name);
		break;

	    case gds__dyn_delete_global_fld:
		DYN_delete_global_field (gbl, ptr);
		break;

    	    case gds__dyn_def_local_fld:
		DYN_define_local_field (gbl, ptr, relation_name, field_name);
		break;
    
    	    case gds__dyn_mod_local_fld:
		DYN_modify_local_field (gbl, ptr, relation_name, NULL);
		break;
    
    	    case gds__dyn_delete_local_fld:
		DYN_delete_local_field (gbl, ptr, relation_name, field_name);
		break;

	    case isc_dyn_mod_sql_fld:
		DYN_modify_sql_field (gbl, ptr, relation_name, NULL);
		break;
 
    	    case gds__dyn_def_sql_fld:
		DYN_define_sql_field (gbl, ptr, relation_name, field_name);
		break;
    
    	    case gds__dyn_def_idx:
		DYN_define_index (gbl, ptr, relation_name, verb, NULL, NULL, NULL, NULL);
		break;

    	    case gds__dyn_rel_constraint:
		DYN_define_constraint (gbl, ptr, relation_name, field_name);
		break;

       	    case gds__dyn_delete_rel_constraint:
		DYN_delete_constraint (gbl, ptr, relation_name); 
		break;

      	    case gds__dyn_mod_idx:
		DYN_modify_index (gbl, ptr);
		break;

    	    case gds__dyn_delete_idx:
		DYN_delete_index (gbl, ptr);
		break;

    	    case gds__dyn_view_relation:
		DYN_define_view_relation (gbl, ptr, relation_name); 
		break;

    	    case gds__dyn_def_dimension:
		DYN_define_dimension (gbl, ptr, relation_name, field_name);
		break;

    	    case gds__dyn_delete_dimensions:
		DYN_delete_dimensions (gbl, ptr, relation_name, field_name);
		break;

    	    default:
		DYN_unsupported_verb();
		break;
	    }
    }
}

SLONG DYN_get_number (
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ g e t _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Pick up a number and possible clear a null flag.
 *
 **************************************/
UCHAR	*p;
USHORT	length;

p = *ptr;
length = *p++;
length |= (*p++) << 8;
*ptr = p + length;

return gds__vax_integer (p, length);
}

USHORT DYN_get_string (
    TEXT	**ptr,
    TEXT	*field,
    USHORT	size,
    USHORT	err_flag)
{
/**************************************
 *
 *	D Y N _ g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Pick up a string, move to a target, and, if requested,
 *	set a null flag.  Return length of string.
 *	If destination field size is too small, punt.
 *	Strings need enough space for null pad.
 *
 **************************************/
TEXT	*p;
USHORT  l, e, length;

e = 0;
p = *ptr;
length = (UCHAR) *p++;
length |= ((USHORT) ((UCHAR) (*p++))) << 8;
if (l = length)
    {
    if (length >= size)
	{
	if ( err_flag)
	    DYN_error_punt (FALSE, 159, NULL, NULL, NULL, NULL, NULL);
	    /* msg 159: Name longer than database field size */
	else
            {
	    l = size - 1;
            e = length - l;
	    }
	}
    do *field++ = *p++; while (--l);
    for ( ; e; e--)
        p++;
    }

*field = 0;
*ptr = p;

return length;
}

#if (defined JPN_SJIS || defined JPN_EUC)
void DYN_get_string2 (
    TEXT	**ptr,
    TEXT	*field,
    USHORT	size)
{
/**************************************
 *
 *	D Y N _ g e t _ s t r i n g 2
 *
 **************************************
 *
 * Functional description
 *	Pick up a string, convert to the interp of
 *	the engine.
 *	If the string expands beyond the field size, punt.
 *
 **************************************/
TEXT	*p;
USHORT	length;
USHORT	from_interp;
USHORT	to_interp;
DSC 	from, to;

to_interp = HOST_INTERP;

p = *ptr;

from_interp = *p++;
from_interp |= (*p++) << 8;

length = *p++;
length |= (*p++) << 8;

from.dsc_dtype = dtype_text;
from.dsc_scale = from_interp;
from.dsc_length = length;
from.dsc_address = (UCHAR*) p;

to.dsc_dtype = dtype_cstring;
to.dsc_scale = to_interp;
to.dsc_length = size;
to.dsc_address = (UCHAR*) field;

MOV_move (&from,&to);

p +=length;
*ptr = p;
}
#endif

USHORT DYN_put_blr_blob (
    GBL		gbl,
    UCHAR	**ptr,
    GDS__QUAD	*blob_id)
{
/**************************************
 *
 *	D Y N _ p u t _ b l r _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Write out a blr blob.
 *
 **************************************/
TDBB	tdbb;
BLK	blob;
USHORT	length;
UCHAR	*p;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

p = *ptr;
length = *p++;
length |= (*p++) << 8;

if (!length)
    {
    *ptr = p;
    return length;
    }

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 106, NULL, NULL, NULL, NULL, NULL);
    /* msg 106: "Create metadata blob failed" */
    }

blob = (BLK) BLB_create (tdbb, gbl->gbl_transaction, blob_id);
BLB_put_segment (tdbb, blob, p, length);
BLB_close (tdbb, blob);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

*ptr = p + length;

return length;
}

#if (defined JPN_SJIS || defined JPN_EUC)
USHORT DYN_put_blr_blob2 (
    GBL		gbl,
    UCHAR	**ptr,
    GDS__QUAD	*blob_id)
{
/**************************************
 *
 *	D Y N _ p u t _ b l r _ b l o b 2
 *
 **************************************
 *
 * Functional description
 *	Write out a blob.
 *	Convert the string to local encoding.
 * 	Not sure why we call this blr blob	
 *	This routine is used to put query_header
 *	which is a text blob.
 *
 **************************************/
TDBB	tdbb;
BLK	blob;
USHORT	length;
UCHAR	*p;
USHORT	bpb_length, from_interp;
UCHAR	bpb [20], *p2;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

p = *ptr;

from_interp = *p++;
from_interp |= (*p++) << 8;

length = *p++;
length |= (*p++) << 8;

if (!length)
    {
    *ptr = p;
    return length;
    }

blob = NULL;

p2 = bpb;
*p2++ = gds__bpb_version1;
*p2++ = gds__bpb_source_interp;
*p2++ = 2;
*p2++ = from_interp;
*p2++ = from_interp >> 8;
bpb_length = p2 - bpb;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 106, NULL, NULL, NULL, NULL, NULL);
    /* msg 106: "Create metadata blob failed" */
    }

blob = (BLK) BLB_create2 (tdbb, gbl->gbl_transaction, blob_id, bpb_length, bpb);
BLB_put_segment (tdbb, blob, p, length);
BLB_close (tdbb, blob);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

*ptr = p + length;

return length;
}
#endif

USHORT DYN_put_text_blob (
    GBL		gbl,
    UCHAR	**ptr,
    GDS__QUAD	*blob_id)
{
/**************************************
 *
 *	D Y N _ p u t _ t e x t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Write out a text blob.
 *
 **************************************/
TDBB	tdbb;
USHORT	length;
UCHAR	*p, *end;
BLK	blob;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

p = *ptr;
length = *p++;
length |= (*p++) << 8;

if (!length)
    {
    *ptr = p;
    return length;
    }

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 106, NULL, NULL, NULL, NULL, NULL);
    /* msg 106: "Create metadata blob failed" */
    }

blob = (BLK) BLB_create (tdbb, gbl->gbl_transaction, blob_id);

for (end = p + length; p < end; p += TEXT_BLOB_LENGTH)
    {
    length = (p + TEXT_BLOB_LENGTH <= end) ? TEXT_BLOB_LENGTH : end - p;
    BLB_put_segment (tdbb, blob, p, length);
    }
BLB_close (tdbb, blob);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

*ptr = end;

return length;
}

#if (defined JPN_SJIS || defined JPN_EUC)
USHORT DYN_put_text_blob2 (
    GBL		gbl,
    UCHAR	**ptr,
    GDS__QUAD	*blob_id)
{
/**************************************
 *
 *	D Y N _ p u t _ t e x t _ b l o b 2
 *
 **************************************
 *
 * Functional description
 *	Write out a text blob.
 *	This one is similar to put_text_blob(),
 *	except that this one is for tagged description blobs.
 *
 **************************************/
TDBB	tdbb;
USHORT	length;
UCHAR	*p, *end;
BLK	blob;
USHORT	bpb_length, from_interp;
UCHAR	bpb [20], *p2;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

p = *ptr;

from_interp = *p++;
from_interp |= (*p++) << 8;

length = *p++;
length |= (*p++) << 8;

if (!length)
    {
    *ptr = p;
    return length;
    }

p2 = bpb;
*p2++ = gds__bpb_version1;
*p2++ = gds__bpb_source_interp;
*p2++ = 2;
*p2++ = from_interp;
*p2++ = from_interp >> 8;
bpb_length = p2 - bpb;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 106, NULL, NULL, NULL, NULL, NULL);
    /* msg 106: "Create metadata blob failed" */
    }

blob = (BLK) BLB_create2 (tdbb, gbl->gbl_transaction, blob_id, bpb_length, bpb);

for (end = p + length; p < end; p += TEXT_BLOB_LENGTH)
    {
    length = (p + TEXT_BLOB_LENGTH <= end) ? TEXT_BLOB_LENGTH : end - p;
    BLB_put_segment (tdbb, blob, p, length);
    }
BLB_close (tdbb, blob);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

*ptr = end;

return length;
}
#endif /* (defined JPN_SJIS || defined JPN_EUC) */

void DYN_rundown_request (
    JMP_BUF	*old_env,
    BLK		handle,
    SSHORT	id)
{
/**************************************
 *
 *	D Y N _ r u n d o w n _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Unwind a request and save it
 *	in the dyn internal request list.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

if (old_env)
    tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!handle)
    return;

EXE_unwind (tdbb, handle);
if (id >= 0 && !DYN_REQUEST (id))
    DYN_REQUEST (id) = handle;
}

USHORT DYN_skip_attribute (
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ s k i p _ a t t r i b u t e
 *
 **************************************
 *
 * Functional description
 *	Skip over attribute returning length (excluding
 *	size of count bytes).
 *
 **************************************/
UCHAR	*p;
USHORT	length;

p = *ptr;
length = *p++;
length |= (*p++) << 8;
*ptr = p + length;

return length;
}

#if (defined JPN_SJIS || defined JPN_EUC)
USHORT DYN_skip_attribute2 (
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ s k i p _ a t t r i b u t e 2
 *
 **************************************
 *
 * Functional description
 *	Skip over a tagged attribute returning length 
 *	(excluding size of count bytes and tag).
 *
 **************************************/
UCHAR	*p;
USHORT	length;
USHORT	from_interp;

p = *ptr;

from_interp = *p++;
from_interp |= (*p++) << 8;

length = *p++;
length |= (*p++) << 8;
*ptr = p + length;

return length;
}
#endif

void DYN_terminate (
    TEXT	*string,
    int		length)
{
/**************************************
 *
 *	D Y N _ t e r m i n a t e
 *
 **************************************
 *
 * Functional description
 *	Remove trailing blanks from a string.
 *	Ensure that the resulting string (of size length) is
 *	terminated with a NULL.
 *
 **************************************/
TEXT	*p, *q;

q = string - 1;
for (p = string; *p && --length; p++)
    {
    if (*p != BLANK)
	q = p;
    }

*(q+1) = '\0';
}

static void grant (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	g r a n t
 *
 **************************************
 *
 * Functional description
 *	Execute SQL grant operation.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
SSHORT	user_type, obj_type;
VOLATILE SSHORT	id, err_num;
int	options, duplicate;
UCHAR	verb;
TEXT	privileges[16], object[32], field[32], user[32], *p, priv[2];
JMP_BUF	env, *old_env;
BOOLEAN grant_role_stmt = FALSE;
TEXT    dummy_name[32], grantor[32], *ptr1, *ptr2;
USHORT  major_version, minor_original;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

GET_STRING (ptr, privileges);
if (!strcmp (privileges, "A"))
    strcpy (privileges, ALL_PRIVILEGES);

object [0] = field [0] = user [0] = 0;
options = 0;
obj_type = user_type = -1;

while ((verb = *(*ptr)++) != gds__dyn_end)
    switch (verb)
	{
	case gds__dyn_rel_name:
	    obj_type = obj_relation;
	    GET_STRING (ptr, object);
	    break;

	case gds__dyn_prc_name:
	    obj_type = obj_procedure;
	    GET_STRING (ptr, object);
	    break;

	case gds__dyn_fld_name:
	    GET_STRING (ptr, field);
	    break;

	case isc_dyn_grant_user_group:
	    user_type = obj_user_group;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_user:
	    GET_STRING (ptr, user);
	    if (DYN_is_it_sql_role (gbl, user, dummy_name, tdbb))
	        user_type = obj_sql_role;
	    else
	        user_type = obj_user;
	    break;

	case isc_dyn_sql_role_name:      /* role name in role_name_list */
            if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
                {
                DYN_error (FALSE, 196, NULL, NULL, NULL, NULL, NULL);
                ERR_punt();
                }
            else
                {
	        obj_type = obj_sql_role;
	        GET_STRING (ptr, object);
	        grant_role_stmt = TRUE;
                }
	    break;

	case gds__dyn_grant_proc:
	    user_type = obj_procedure;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_trig:
	    user_type = obj_trigger;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_view:
	    user_type = obj_view;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_options:
	case isc_dyn_grant_admin_options:
	    options = DYN_get_number (ptr);
	    break;

	default:
	    DYN_unsupported_verb();
	}


old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    /* we need to rundown as we have to set the env.
       But in case the error is from store_priveledge we have already
       unwound the request so passing that as null */
    DYN_rundown_request (old_env, 
        ((id == drq_s_grant || id == drq_gcg1) ? NULL : request), -1);
    if (id == drq_l_grant1)
	DYN_error_punt (TRUE, 77, NULL, NULL, NULL, NULL, NULL);
	/* msg 77: "SELECT RDB$USER_PRIVILEGES failed in grant" */
    else
    if (id == drq_l_grant2)
	DYN_error_punt (TRUE, 78, NULL, NULL, NULL, NULL, NULL);
	/* msg 78: "SELECT RDB$USER_PRIVILEGES failed in grant" */
    else
    if (id == drq_s_grant || id == drq_gcg1)
        ERR_punt();
        /* store_priviledge || grantor_can_grant error already handled, 
           just bail out */
    else
    if (id == drq_get_role_nm)
        ERR_punt();
    else
	{
        assert (id == drq_gcg1);
	DYN_error_punt (TRUE, 78, NULL, NULL, NULL, NULL, NULL);
	/* msg 78: "SELECT RDB$USER_PRIVILEGES failed in grant" */
	}
    }

request = (BLK) CMP_find_request (tdbb, field[0] ? drq_l_grant1 : drq_l_grant2, DYN_REQUESTS);
for (p = privileges; *p; p++)
    {
    duplicate = FALSE;
    priv [0] = *p;
    priv [1] = 0;
    if (field [0])
	{
	id = drq_l_grant1;
	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		PRIV IN RDB$USER_PRIVILEGES WITH
  		PRIV.RDB$RELATION_NAME EQ object AND
		PRIV.RDB$OBJECT_TYPE = obj_type AND
		PRIV.RDB$PRIVILEGE EQ priv AND
		PRIV.RDB$USER = user AND
		PRIV.RDB$USER_TYPE = user_type AND
		PRIV.RDB$GRANTOR EQ UPPERCASE (RDB$USER_NAME) AND
		PRIV.RDB$FIELD_NAME EQ field
	    if (!DYN_REQUEST (drq_l_grant1))
		DYN_REQUEST (drq_l_grant1) = request;

	    if ((PRIV.RDB$GRANT_OPTION.NULL) || 
		(PRIV.RDB$GRANT_OPTION) ||
		(PRIV.RDB$GRANT_OPTION == options))
		duplicate = TRUE;
	    else
		ERASE PRIV;	/* has to be 0 and options == 1 */
	END_FOR;
	if (!DYN_REQUEST (drq_l_grant1))
	    DYN_REQUEST (drq_l_grant1) = request;
	}
    else
	{
	id = drq_l_grant2;
	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		PRIV IN RDB$USER_PRIVILEGES WITH
  		PRIV.RDB$RELATION_NAME EQ object AND
		PRIV.RDB$OBJECT_TYPE = obj_type AND
		PRIV.RDB$PRIVILEGE EQ priv AND
		PRIV.RDB$USER = user AND
		PRIV.RDB$USER_TYPE = user_type AND
		PRIV.RDB$GRANTOR EQ UPPERCASE (RDB$USER_NAME) AND
		PRIV.RDB$FIELD_NAME MISSING
	    if (!DYN_REQUEST (drq_l_grant2))
		DYN_REQUEST (drq_l_grant2) = request;

	    if ((PRIV.RDB$GRANT_OPTION.NULL) ||
		(PRIV.RDB$GRANT_OPTION) ||
		(PRIV.RDB$GRANT_OPTION == options))
		duplicate = TRUE;
	    else
		ERASE PRIV;	/* has to be 0 and options == 1 */
	END_FOR;
	if (!DYN_REQUEST (drq_l_grant2))
	    DYN_REQUEST (drq_l_grant2) = request;
	}

    if (duplicate)
	continue;

    if (grant_role_stmt)
        {
        if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
            {
            DYN_error (FALSE, 196, NULL, NULL, NULL, NULL, NULL);
            ERR_punt();
            }
        id = drq_get_role_nm;
 
        for (ptr1 = tdbb->tdbb_attachment->att_user->usr_user_name,
             ptr2 = grantor; *ptr1; ptr1++, ptr2++)
            *ptr2 = UPPER7 (*ptr1);
        *ptr2 = '\0';

        if (!grantor_can_grant_role (tdbb, gbl, grantor, object))
            ERR_punt();

        if (user_type == obj_sql_role)
            {
            /****************************************************
            **
            ** temporary restriction. This should be removed once
            ** GRANT role1 TO rolex is supported and this message
            ** could be reused for blocking cycles of role grants
            **
            *****************************************************/
            DYN_error (FALSE, 192, user, NULL, NULL, NULL, NULL);
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
            ERR_punt();
            }
        }
    else
        {

    /* In the case where the object is a view, then the grantor must have
       some kind of grant privileges on the base table(s)/view(s).  If the
       grantor is the owner of the view, then we have to explicitely check
       this because the owner of a view by default has grant privileges on
       his own view.  If the grantor is not the owner of the view, then the
       base table/view grant privilege checks were made when the grantor
       got its grant privilege on the view and no further checks are
       necessary. */

    if (!obj_type)   /* relation or view because we cannot distinguish at this point. */
	{
	id = drq_gcg1;
	if (!grantor_can_grant (gbl, tdbb->tdbb_attachment->att_user->usr_user_name, 
				priv, object, field, TRUE))
	    {
	    /* grantor_can_grant has moved the error string already. 
               just punt back to the setjump */
	    ERR_punt();
	    }
	}
        }

    id = drq_s_grant;
    store_privilege (gbl, object, user, field, p, user_type, obj_type, options);
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

static BOOLEAN grantor_can_grant (
    GBL		gbl,
    TEXT	*grantor,
    TEXT	*privilege,
    TEXT	*relation_name,
    TEXT	*field_name,
    BOOLEAN	top_level)
{
/**************************************
 *
 *	g r a n t o r _ c a n _ g r a n t
 *
 **************************************
 *
 * Functional description
 *	return: TRUE is the grantor has grant privilege on the relation/field.
 *		FALSE otherwise.
 *
 **************************************/
JMP_BUF env, *old_env;
TDBB	tdbb;
DBB	dbb;
/* Remember the grant option for non field-specific user-privileges, and the
   grant option for the user-privileges for the input field.
   -1 = no privilege found (yet)
   0 = privilege without grant option found
   1 = privilege with grant option found */
SSHORT	go_rel = -1;
SSHORT	go_fld = -1; 
BLK	request;
BOOLEAN	sql_relation = FALSE;
BOOLEAN	relation_exists = FALSE;
BOOLEAN	field_exists = FALSE;
BOOLEAN	grantor_is_owner = FALSE;
VOLATILE USHORT err_num;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

/* Verify that the input relation exists. */

request = (BLK) CMP_find_request (tdbb, drq_gcg4, DYN_REQUESTS);


old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, err_num, NULL, NULL, NULL, NULL, NULL);
    /* msg 77: "SELECT RDB$USER_PRIVILEGES failed in grant" */

    return FALSE; 
    }


err_num = 182; /* for the longjump */
/* SELECT RDB$RELATIONS failed in grant */
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    REL IN RDB$RELATIONS WITH
	REL.RDB$RELATION_NAME = relation_name
    relation_exists = TRUE;
    if ((!REL.RDB$FLAGS.NULL) && (REL.RDB$FLAGS & REL_sql))
	sql_relation = TRUE;
    if (!DYN_REQUEST (drq_gcg4))
    	DYN_REQUEST (drq_gcg4) = request;
END_FOR;
if (!DYN_REQUEST (drq_gcg4))
    DYN_REQUEST (drq_gcg4) = request;
if (!relation_exists)
    {
    DYN_error (FALSE, 175, relation_name, NULL, NULL, NULL, NULL);
    /* table/view .. does not exist */
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    return FALSE; 
    }

/* Verify the the input field exists. */

if (field_name[0])
    {
    err_num = 183;
    /* SELECT RDB$RELATION_FIELDS failed in grant */
    request = (BLK) CMP_find_request (tdbb, drq_gcg5, DYN_REQUESTS);
    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    	G_FLD IN RDB$RELATION_FIELDS WITH
	    G_FLD.RDB$RELATION_NAME = relation_name AND
	    G_FLD.RDB$FIELD_NAME = field_name
    	if (!DYN_REQUEST (drq_gcg5))
    	    DYN_REQUEST (drq_gcg5) = request;
    	field_exists = TRUE;
    END_FOR;
    if (!DYN_REQUEST (drq_gcg5))
    	DYN_REQUEST (drq_gcg5) = request;
    if (!field_exists)
	{
	DYN_error (FALSE, 176, field_name, relation_name, NULL, NULL, NULL);
	/* column .. does not exist in table/view .. */
        tdbb->tdbb_setjmp = (UCHAR*) old_env;
	return FALSE;
	}
    }

/* If the current user is locksmith - allow all grants to occur */

if (tdbb->tdbb_attachment->att_user->usr_flags & USR_locksmith)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    return TRUE;
    }

/* If this is a non-sql table, then the owner will probably not have any
   entries in the rdb$user_privileges table.  Give the owner of a GDML
   table all privileges. */
err_num = 184;
/* SELECT RDB$RELATIONS/RDB$OWNER_NAME failed in grant */

request = (BLK) CMP_find_request (tdbb, drq_gcg2, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    REL IN RDB$RELATIONS WITH
	REL.RDB$RELATION_NAME = relation_name AND
	REL.RDB$OWNER_NAME = UPPERCASE (grantor)
    if (!DYN_REQUEST (drq_gcg2))
    	DYN_REQUEST (drq_gcg2) = request;
    grantor_is_owner = TRUE;
END_FOR;
if (!DYN_REQUEST (drq_gcg2))
    DYN_REQUEST (drq_gcg2) = request;
if (!sql_relation && grantor_is_owner)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    return TRUE;
    }

/* Verify that the grantor has the grant option for this relation/field
   in the rdb$user_privileges.  If not, then we don't need to look further. */

err_num = 185;
/* SELECT RDB$USER_PRIVILEGES failed in grant */

request = (BLK) CMP_find_request (tdbb, drq_gcg1, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    PRV IN RDB$USER_PRIVILEGES WITH
	PRV.RDB$USER = UPPERCASE(grantor) AND
	PRV.RDB$USER_TYPE = obj_user AND
	PRV.RDB$RELATION_NAME = relation_name AND
	PRV.RDB$OBJECT_TYPE = obj_relation AND
	PRV.RDB$PRIVILEGE = privilege
    if (!DYN_REQUEST (drq_gcg1))
	DYN_REQUEST (drq_gcg1) = request;

    if (PRV.RDB$FIELD_NAME.NULL)
	{
	if (PRV.RDB$GRANT_OPTION.NULL ||
	    !PRV.RDB$GRANT_OPTION)
	    go_rel = 0;
	else
	if (go_rel)
	    go_rel = 1;
	}
    else
	{
	if (PRV.RDB$GRANT_OPTION.NULL ||
	    !PRV.RDB$GRANT_OPTION) 
	    {
	    DYN_terminate (PRV.RDB$FIELD_NAME, sizeof (PRV.RDB$FIELD_NAME));
	    if (field_name[0] &&
		!strcmp (field_name, PRV.RDB$FIELD_NAME))
		go_fld = 0;
	    }
	else
	    {
	    DYN_terminate (PRV.RDB$FIELD_NAME, sizeof (PRV.RDB$FIELD_NAME));
	    if (field_name[0] &&
		!strcmp (field_name, PRV.RDB$FIELD_NAME)) 
		go_fld = 1;
	    }
	}
END_FOR;
if (!DYN_REQUEST (drq_gcg1))
    DYN_REQUEST (drq_gcg1) = request;

if (field_name[0])
    {
    if (go_fld == 0)
	{
	DYN_error (FALSE, (top_level ? 167 : 168), privilege, field_name, relation_name, NULL, NULL);
	/* no grant option for privilege .. on column .. of [base] table/view .. */
        tdbb->tdbb_setjmp = (UCHAR*) old_env;
	return FALSE; 
	}
    else
    if (go_fld == -1)
	{
	if (go_rel == 0)
	    {
	    DYN_error (FALSE, (top_level ? 169 : 170), privilege, relation_name, field_name, NULL, NULL);
	    /* no grant option for privilege .. on [base] table/view .. (for column ..) */
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
	    return FALSE; 
	    }
	else
	if (go_rel == -1)
	    {
	    DYN_error (FALSE, (top_level ? 171 : 172 ), privilege, relation_name, field_name, NULL, NULL); 
	    /* no .. privilege with grant option on [base] table/view .. (for column ..) */
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
	    return FALSE;
	    }
	}
    }
else
    {
    if (go_rel == 0)
	{
	DYN_error (FALSE, 173, privilege, relation_name, NULL, NULL, NULL);
	/* no grant option for privilege .. on table/view ..  */
        tdbb->tdbb_setjmp = (UCHAR*) old_env;
	return FALSE;
	}
    else
    if (go_rel == -1)
	{
	DYN_error (FALSE, 174, privilege, relation_name, NULL, NULL, NULL);
	/* no .. privilege with grant option on table/view .. */
        tdbb->tdbb_setjmp = (UCHAR*) old_env;
	return FALSE;
	}
    }

/* If the grantor is not the owner of the relation, then we don't need to
   check the base table(s)/view(s) because that check was performed when 
   the grantor was given its privileges. */

if (!grantor_is_owner)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    return TRUE;
    }

/* Find all the base fields/relations and check for the correct
   grant privileges on them. */

err_num = 186;
/* SELECT RDB$VIEW_RELATIONS/RDB$RELATION_FIELDS/... failed in grant */

request = (BLK) CMP_find_request (tdbb, drq_gcg3, DYN_REQUESTS);
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    G_FLD IN RDB$RELATION_FIELDS CROSS
    G_VIEW IN RDB$VIEW_RELATIONS WITH
	G_FLD.RDB$RELATION_NAME = relation_name AND
	G_FLD.RDB$BASE_FIELD NOT MISSING AND
	G_VIEW.RDB$VIEW_NAME EQ G_FLD.RDB$RELATION_NAME AND
	G_VIEW.RDB$VIEW_CONTEXT EQ G_FLD.RDB$VIEW_CONTEXT
    if (!DYN_REQUEST (drq_gcg3))
    	DYN_REQUEST (drq_gcg3) = request;
    DYN_terminate (G_FLD.RDB$BASE_FIELD, sizeof (G_FLD.RDB$BASE_FIELD));
    DYN_terminate (G_FLD.RDB$FIELD_NAME, sizeof (G_FLD.RDB$FIELD_NAME));
    DYN_terminate (G_VIEW.RDB$RELATION_NAME, sizeof (G_VIEW.RDB$RELATION_NAME));
    if (field_name[0])
	{
	if (!strcmp (field_name, G_FLD.RDB$FIELD_NAME))
	    if (!grantor_can_grant (gbl, grantor, privilege, G_VIEW.RDB$RELATION_NAME, 
				    G_FLD.RDB$BASE_FIELD, FALSE))
                {
                tdbb->tdbb_setjmp = (UCHAR*) old_env;
		return FALSE;
                }
	}
    else
	{
	if (!grantor_can_grant (gbl, grantor, privilege, G_VIEW.RDB$RELATION_NAME, 
				G_FLD.RDB$BASE_FIELD, FALSE))
            {
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
            return FALSE;
            }
	}
END_FOR;
if (!DYN_REQUEST (drq_gcg3))
    DYN_REQUEST (drq_gcg3) = request;

/* all done. */
tdbb->tdbb_setjmp = (UCHAR*) old_env;
return TRUE;
}

static BOOLEAN grantor_can_grant_role (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*grantor,
    TEXT	*role_name)
{
/**************************************
 *
 *	g r a n t o r _ c a n _ g r a n t _ r o l e
 *
 **************************************
 *
 * Functional description
 *
 *	return: TRUE if the grantor has admin privilege on the role.
 *		FALSE otherwise.
 *
 **************************************/
DBB     dbb;
BLK     request;
BOOLEAN	grantable = FALSE;
TEXT    owner [32];

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

if (tdbb->tdbb_attachment->att_user->usr_flags & USR_locksmith)
    return TRUE;

/* Fetch the name of the owner of the ROLE */
if (DYN_is_it_sql_role (gbl, role_name, owner, tdbb))
    {
    /* The owner of the ROLE can always grant membership */
    if (strcmp (owner, grantor) == 0)
        return TRUE;
    }
else
    {
    /****************************************************
    **
    **  role name not exist.
    **
    *****************************************************/
    DYN_error (FALSE, 188, role_name, NULL, NULL, NULL, NULL);
    return FALSE;
    }

request = (BLK) CMP_find_request (tdbb, drq_get_role_au, DYN_REQUESTS);

/* The 'grantor' is not the owner of the ROLE, see if they have
   admin privilege on the role */
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    PRV IN RDB$USER_PRIVILEGES WITH
	PRV.RDB$USER = UPPERCASE(grantor)  AND
	PRV.RDB$USER_TYPE = obj_user       AND
	PRV.RDB$RELATION_NAME EQ role_name AND
	PRV.RDB$OBJECT_TYPE = obj_sql_role AND
	PRV.RDB$PRIVILEGE EQ "M"

    if (!DYN_REQUEST (drq_get_role_au))
        DYN_REQUEST (drq_get_role_au) = request;

    if (PRV.RDB$GRANT_OPTION == 2)
        grantable = TRUE;
    else
        {
	/****************************************************
	**
	**  user have no admin option.
	**
	*****************************************************/
        DYN_error (FALSE, 189, grantor, role_name, NULL, NULL, NULL);
        return FALSE;
        }

END_FOR;

if (!DYN_REQUEST (drq_get_role_au))
    DYN_REQUEST (drq_get_role_au) = request;

if (!grantable)
    {
    /****************************************************
    **
    **  user is not a member of the role.
    **
    *****************************************************/
    DYN_error (FALSE, 190, grantor, role_name, NULL, NULL, NULL);
    return FALSE;
    }

return grantable;
}

static void revoke (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	r e v o k e
 *
 **************************************
 *
 * Functional description
 *	Revoke some privileges.
 *
 *	Note: According to SQL II, section 11.37, pg 280, 
 *	general rules 8 & 9,
 *	if the specified priviledge for the specified user
 *	does not exist, it is not an exception error condition
 *	but a completion condition.  Since V4.0 does not support
 *	completion conditions (warnings) we do not return
 *	any indication that the revoke operation didn't do
 *	anything.
 *	1994-August-2 Rich Damon & David Schnepper
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	old_request, request;
VOLATILE USHORT	old_id, id;
UCHAR	verb;
SSHORT	user_type, obj_type;
TEXT	privileges[16], object[32], field[32], user[32], 
	*p, *q, temp [2];
USHORT	all_privs = FALSE;
int	options;
USHORT	grant_erased;
JMP_BUF	env, *old_env;
USR	revoking_user;
TEXT	revoking_user_name [32], dummy_name[32];
TEXT	grantor[32], *ptr1, *ptr2;
USHORT  major_version, minor_original;


tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

/* Stash away a copy of the revoker's name, in uppercase form */

revoking_user = tdbb->tdbb_attachment->att_user;
for (p = revoking_user->usr_user_name, q = revoking_user_name; *p; p++, q++)
    *q = UPPER7 (*p);
*q = 0;

GET_STRING (ptr, privileges);
if (!strcmp (privileges, "A"))
    {
    strcpy (privileges, ALL_PRIVILEGES);
    all_privs = TRUE;
    }

object [0] = field [0] = user [0] = 0;
obj_type = user_type = -1;
options = 0;

while ((verb = *(*ptr)++) != gds__dyn_end)
    switch (verb)
	{
	case gds__dyn_rel_name:
	    obj_type = obj_relation;
	    GET_STRING (ptr, object);
	    break;

	case gds__dyn_prc_name:
	    obj_type = obj_procedure;
	    GET_STRING (ptr, object);
	    break;

	case gds__dyn_fld_name:
	    GET_STRING (ptr, field);
	    break;

	case isc_dyn_grant_user_group:
	    user_type = obj_user_group;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_user:
	    GET_STRING (ptr, user);
	    if (DYN_is_it_sql_role (gbl, user, dummy_name, tdbb))
	        user_type = obj_sql_role;
	    else
	        user_type = obj_user;
	    break;

	case isc_dyn_sql_role_name:       /* role name in role_name_list */
            if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
                {
                DYN_error (FALSE, 196, NULL, NULL, NULL, NULL, NULL);
                ERR_punt();
                }
            else
                {
	        obj_type = obj_sql_role;
	        GET_STRING (ptr, object);
                }
	    break;

	case gds__dyn_grant_proc:
	    user_type = obj_procedure;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_trig:
	    user_type = obj_trigger;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_view:
	    user_type = obj_view;
	    GET_STRING (ptr, user);
	    break;

	case gds__dyn_grant_options:
	    options = DYN_get_number (ptr);
	    break;

	default:
	    DYN_unsupported_verb();
	}

request = (BLK) CMP_find_request (tdbb, field [0] ? drq_e_grant1 : drq_e_grant2, DYN_REQUESTS);
id = field [0] ? drq_e_grant1 : drq_e_grant2;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    /* we need to rundown as we have to set the env.
       But in case the error is from store_priveledge we have already 
       unwound the request so passing that as null */
    DYN_rundown_request (old_env, ((id == drq_s_grant) ? NULL : request), -1);
    if (id == drq_e_grant1)
	DYN_error_punt (TRUE, 111, NULL, NULL, NULL, NULL, NULL);
	/* msg 111: "ERASE RDB$USER_PRIVILEGES failed in revoke(1)" */
    else if (id == drq_e_grant2)
	DYN_error_punt (TRUE, 113, NULL, NULL, NULL, NULL, NULL);
	/* msg 113: "ERASE RDB$USER_PRIVILEGES failed in revoke (3)" */
    else
        ERR_punt();
        /* store_priviledge error already handled, just bail out */
    }

temp [1] = 0;
for (p = privileges; temp [0] = *p; p++)
    {
    grant_erased = FALSE;

    if (field [0])
	{
	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		PRIV IN RDB$USER_PRIVILEGES WITH
		PRIV.RDB$PRIVILEGE EQ temp AND
		PRIV.RDB$RELATION_NAME EQ object AND
		PRIV.RDB$OBJECT_TYPE = obj_type AND
		PRIV.RDB$USER = user AND
		PRIV.RDB$USER_TYPE = user_type AND
		PRIV.RDB$FIELD_NAME EQ field
	    if (!DYN_REQUEST (drq_e_grant1))
		DYN_REQUEST (drq_e_grant1) = request;

	    DYN_terminate (PRIV.RDB$GRANTOR, sizeof (PRIV.RDB$GRANTOR));
	    if ((revoking_user->usr_flags & USR_locksmith) ||
		(!strcmp (PRIV.RDB$GRANTOR, revoking_user_name)))
		{
		ERASE PRIV;
		grant_erased = TRUE;
		};

	END_FOR;
	if (!DYN_REQUEST (drq_e_grant1))
	    DYN_REQUEST (drq_e_grant1) = request;
	}
    else
	{
	FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		PRIV IN RDB$USER_PRIVILEGES WITH
		PRIV.RDB$PRIVILEGE EQ temp AND
		PRIV.RDB$RELATION_NAME EQ object AND
		PRIV.RDB$OBJECT_TYPE = obj_type AND
		PRIV.RDB$USER EQ user AND
		PRIV.RDB$USER_TYPE = user_type
	    if (!DYN_REQUEST (drq_e_grant2))
		DYN_REQUEST (drq_e_grant2) = request;

	    DYN_terminate (PRIV.RDB$GRANTOR, sizeof (PRIV.RDB$GRANTOR));

	    /* revoking a permission at the table level implies
	       revoking the perm. on all columns. So for all fields
	       in this table which have been granted the privilege, we
	       erase the entries from RDB$USER_PRIVILEGES. */

	    if ((revoking_user->usr_flags & USR_locksmith) ||
	        (!strcmp (PRIV.RDB$GRANTOR, revoking_user_name)))
	        {
		ERASE PRIV;
		grant_erased = TRUE;
		}
	END_FOR;
	if (!DYN_REQUEST (drq_e_grant2))
	    DYN_REQUEST (drq_e_grant2) = request;
	}

    if (options && grant_erased)
	{
	/* Add the privilege without the grant option 
	 * There is a modify trigger on the rdb$user_privileges
	 * which disallows the table from being updated.  It would
	 * have to be changed such that only the grant_option
	 * field can be updated.
	 */

	old_id      = id;
	id          = drq_s_grant;

	store_privilege (gbl, object, user, field, p, user_type, obj_type, 0);

	id = old_id;
	}
    }

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

static void store_privilege (
    GBL		gbl,
    TEXT	*object,
    TEXT	*user,
    TEXT	*field,
    TEXT	*privilege,
    SSHORT	user_type,
    SSHORT	obj_type,
    int		option)
{
/**************************************
 *
 *	s t o r e _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *	Does its own cleanup in case of error, so calling
 *	routine should not.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;

JMP_BUF env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_grant, DYN_REQUESTS);

/* need to unwind our own request here!! SM 27-Sep-96 */
old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 79, NULL, NULL, NULL, NULL, NULL);
    /* msg 79: "STORE RDB$USER_PRIVILEGES failed in grant" */
    }


STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
					    PRIV IN RDB$USER_PRIVILEGES

    PRIV.RDB$FIELD_NAME.NULL = TRUE;
    strcpy (PRIV.RDB$RELATION_NAME, object);
    strcpy (PRIV.RDB$USER, user);
    PRIV.RDB$USER_TYPE   = user_type;
    PRIV.RDB$OBJECT_TYPE = obj_type;
    if (field && field [0])
	{
	strcpy (PRIV.RDB$FIELD_NAME, field);
	PRIV.RDB$FIELD_NAME.NULL = FALSE;
	}
    PRIV.RDB$PRIVILEGE [0] = privilege [0];
    PRIV.RDB$PRIVILEGE [1] = 0;
    PRIV.RDB$GRANT_OPTION  = option;

END_STORE;

if (!DYN_REQUEST (drq_s_grant))
    DYN_REQUEST (drq_s_grant) = request;
tdbb->tdbb_setjmp = (UCHAR*) old_env;
}
