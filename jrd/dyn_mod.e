/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn_modify.e
 *	DESCRIPTION:	Dynamic data definition - DYN_modify_<x>
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
#include "../jrd/dyn_md_proto.h"
#include "../jrd/dyn_ut_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/inf_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/vio_proto.h"
#ifndef WINDOWS_ONLY
#include "../jrd/ail_proto.h"
#endif

#ifdef SUPERSERVER
#define V4_THREADING
#endif
#include "../jrd/nlm_thd.h"

DATABASE
    DB = STATIC "yachts.gdb";

#define MAX_CHARS_SHORT		6	/* 2**16  = 5 chars + sign */
#define MAX_CHARS_LONG		11	/* 2**32  = 10 chars + sign */
#define MAX_CHARS_INT64		20	/* 2**64  = 19 chars + sign */
#define MAX_CHARS_DOUBLE	22	/* 15 digits + 2 signs + E + decimal + 3 digit exp */
#define MAX_CHARS_FLOAT		13	/* 7 digits + 2 signs + E + decimal + 2 digit exp */

static UCHAR	alloc_info [] = { gds__info_allocation, gds__info_end };
static void	drop_cache (GBL);
static void	drop_log (GBL);
static void 	modify_lfield_type (GBL, UCHAR**, TEXT*, TEXT*);
static void	modify_lfield_position (TDBB, DBB, GBL, TEXT*, TEXT*, USHORT, USHORT); 
static BOOLEAN	check_view_dependency (TDBB, DBB, GBL, TEXT*, TEXT*);
static BOOLEAN	check_sptrig_dependency (TDBB, DBB, GBL, TEXT*, TEXT*);
static void 	modify_lfield_index (TDBB, DBB, GBL, TEXT*, TEXT*, TEXT*);
static BOOLEAN  field_exists (TDBB, DBB, GBL, TEXT*, TEXT*);
static BOOLEAN  domain_exists (TDBB, DBB, GBL, TEXT*);
static void	get_domain_type (TDBB, DBB, GBL, DYN_FLD);
static ULONG	check_update_fld_type (DYN_FLD, DYN_FLD);
static void	modify_err_punt (TDBB, ULONG, DYN_FLD, DYN_FLD);

void DYN_modify_database (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Modify a database.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
SSHORT	length;
SLONG	start;
UCHAR	verb, s [128];
JMP_BUF	env, *old_env;
#ifndef WINDOWS_ONLY
SSHORT	num_log_buffers = 0;
USHORT	log_buffer_size = 0;
SLONG	check_point_len = 0, group_commit_wait = -1;
SSHORT	log_params_defined = FALSE;
#endif
#ifdef SUPERSERVER
USHORT	first_log_file = TRUE;
#endif

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 84, NULL, NULL, NULL, NULL, NULL);
    /* msg 84: "MODIFY DATABASE failed" */
    }

INF_database_info (alloc_info, sizeof (alloc_info), s, sizeof (s));

if (s [0] != gds__info_allocation)
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 84, NULL, NULL, NULL, NULL, NULL);
    /* msg 84: "MODIFY DATABASE failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_m_database, DYN_REQUESTS);

length = gds__vax_integer (s + 1, 2);
start = gds__vax_integer (s + 3, length); 

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	DBB IN RDB$DATABASE
    if (!DYN_REQUEST (drq_m_database))
	DYN_REQUEST (drq_m_database) = request;

    MODIFY DBB USING
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_security_class:
		    if (GET_STRING (ptr, DBB.RDB$SECURITY_CLASS))
			DBB.RDB$SECURITY_CLASS.NULL = FALSE;
		    else
			DBB.RDB$SECURITY_CLASS.NULL = TRUE;
		    break;
	    
		case gds__dyn_description:
		    if (DYN_put_text_blob (gbl, ptr, &DBB.RDB$DESCRIPTION))
			DBB.RDB$DESCRIPTION.NULL = FALSE;
		    else
			DBB.RDB$DESCRIPTION.NULL = TRUE;
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_description2:
		    if (DYN_put_text_blob2 (gbl, ptr, &DBB.RDB$DESCRIPTION))
			DBB.RDB$DESCRIPTION.NULL = FALSE;
		    else
			DBB.RDB$DESCRIPTION.NULL = TRUE;
		    break;
#endif

		case gds__dyn_def_file:
		    DYN_define_file (gbl, ptr, (SLONG) 0, &start, 158);
		    break;

#ifdef SUPERSERVER
		case gds__dyn_def_default_log:
		    DYN_define_log_file (gbl, ptr, first_log_file, TRUE);
		    break;
 
		case gds__dyn_def_log_file:
		    DYN_define_log_file (gbl, ptr, first_log_file, FALSE);
		    first_log_file = FALSE;
		    break;
#endif

		case gds__dyn_def_cache_file:
		    DYN_define_cache(gbl, ptr);
		    break;

		case gds__dyn_log_group_commit_wait:
#ifndef WINDOWS_ONLY
		    group_commit_wait = DYN_get_number (ptr);
		    log_params_defined = TRUE;
#endif
		    break;

		case gds__dyn_log_buffer_size:
#ifndef WINDOWS_ONLY
		    log_buffer_size = DYN_get_number (ptr);
		    log_params_defined = TRUE;
#endif
		    break;

		case gds__dyn_log_check_point_length:
#ifndef WINDOWS_ONLY
		    check_point_len = DYN_get_number (ptr);
		    log_params_defined = TRUE;
#endif
		    break;

		case gds__dyn_log_num_of_buffers:
#ifndef WINDOWS_ONLY
		    num_log_buffers = DYN_get_number (ptr);
		    log_params_defined = TRUE;
#endif
		    break;

		case gds__dyn_drop_cache:
		    drop_cache (gbl);
		    break;	

		case gds__dyn_drop_log:
		    drop_log (gbl);
		    break;

		case gds__dyn_fld_character_set_name:
		    if (GET_STRING (ptr, DBB.RDB$CHARACTER_SET_NAME))
			DBB.RDB$CHARACTER_SET_NAME.NULL = FALSE;
		    else
			DBB.RDB$CHARACTER_SET_NAME.NULL = TRUE;
		    break;

		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, NULL_PTR, NULL_PTR,
				 NULL_PTR, NULL_PTR, NULL_PTR);
		}
    END_MODIFY;
END_FOR;
#ifndef WINDOWS_ONLY
if (log_params_defined)
	AIL_set_log_options (check_point_len, num_log_buffers, log_buffer_size,
		group_commit_wait);
#endif

if (!DYN_REQUEST (drq_m_database))
    DYN_REQUEST (drq_m_database) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_modify_exception (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	Modify an exception.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
UCHAR	verb;
TEXT	t [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_m_xcp, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 145, NULL, NULL, NULL, NULL, NULL);
    /* msg 145: "MODIFY EXCEPTION failed" */
    }

GET_STRING (ptr, t);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$EXCEPTIONS
	WITH X.RDB$EXCEPTION_NAME EQ t
    if (!DYN_REQUEST (drq_m_xcp))
	DYN_REQUEST (drq_m_xcp) = request;

    found = TRUE;
    MODIFY X
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_xcp_msg:
		    GET_STRING (ptr, X.RDB$MESSAGE);
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
    END_MODIFY;
END_FOR;
if (!DYN_REQUEST (drq_m_xcp))
    DYN_REQUEST (drq_m_xcp) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 144, NULL, NULL, NULL, NULL, NULL);
    /* msg 144: "Exception not found" */
}

void DYN_modify_global_field (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request, old_request;
USHORT	found, single_validate = FALSE;
UCHAR	verb;
JMP_BUF	env, *old_env;
DYN_FLD orig_dom,
	new_dom;
BOOLEAN dtype, scale, prec, subtype, charlen, collation, fldlen, nullflg, charset;
TEXT	*qryname, *qryhdr, *qryhdr2, *edtstr, *edtstr2, *missingval, *singvald, 
	*fldvald, *fldvaldsrc, *fldvaldsrc2, *desc, *desc2, *delvald, *deldflt, *flddftval, *flddfltsrc;

BOOLEAN bqryname, bqryhdr, bqryhdr2, bedtstr, bedtstr2, bmissingval, bsingvald,
	bfldvald, bfldvaldsrc, bfldvaldsrc2, bdesc, bdesc2,bdelvald, bdeldflt, bflddftval, bflddfltsrc;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

dtype = scale = prec = subtype = charlen = collation = fldlen = nullflg = charset = FALSE;
bqryname = bqryhdr = bqryhdr2 = bedtstr = bedtstr2 = bmissingval = bsingvald = FALSE;
bfldvald = bfldvaldsrc = bfldvaldsrc2 = bdesc = bdesc2 = bdelvald = bdeldflt = bflddftval = bflddfltsrc = FALSE;


request = (BLK) CMP_find_request (tdbb, drq_m_gfield, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    if (orig_dom)
	gds__free ((SLONG*)orig_dom);

    if (new_dom)
	gds__free ((SLONG*) new_dom);

    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 87, NULL, NULL, NULL, NULL, NULL);
    /* msg 87: "MODIFY RDB$FIELDS failed" */
    }

/* Allocate the field structures */
orig_dom = (DYN_FLD) gds__alloc(sizeof(struct dyn_fld));
if (!orig_dom)
    DYN_error_punt (TRUE, 211, NULL, NULL, NULL, NULL, NULL);

MOVE_CLEAR (orig_dom, sizeof(struct dyn_fld));

new_dom = (DYN_FLD) gds__alloc(sizeof(struct dyn_fld));
if (!new_dom)
    DYN_error_punt (TRUE, 211, NULL, NULL, NULL, NULL, NULL);

MOVE_CLEAR (new_dom, sizeof(struct dyn_fld));

GET_STRING (ptr, orig_dom->dyn_fld_name);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FLD IN RDB$FIELDS WITH FLD.RDB$FIELD_NAME EQ orig_dom->dyn_fld_name

    if (!DYN_REQUEST (drq_m_gfield))
	DYN_REQUEST (drq_m_gfield) = request;

    found = TRUE;

    DSC_make_descriptor (&orig_dom->dyn_dsc,
			 FLD.RDB$FIELD_TYPE,
			 FLD.RDB$FIELD_SCALE,
			 FLD.RDB$FIELD_LENGTH,
			 FLD.RDB$FIELD_SUB_TYPE,
			 FLD.RDB$CHARACTER_SET_ID,
			 FLD.RDB$COLLATION_ID);

    orig_dom->dyn_dtype = FLD.RDB$FIELD_TYPE;
    orig_dom->dyn_precision = FLD.RDB$FIELD_PRECISION;
    orig_dom->dyn_charlen = FLD.RDB$CHARACTER_LENGTH;
    orig_dom->dyn_collation = FLD.RDB$COLLATION_ID;
    orig_dom->dyn_null_flag = FLD.RDB$NULL_FLAG;

    /* If the original field type is an array, force its blr type to blr_blob */
    if (FLD.RDB$DIMENSIONS != 0)
	orig_dom->dyn_dtype = blr_blob;


    while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_fld_name:
		    {
		    char newfld[32];

		    if (GET_STRING (ptr, newfld))
                        {
			if (!domain_exists (tdbb, dbb, gbl, newfld))
			    {
		            MODIFY FLD USING
			        strcpy (FLD.RDB$FIELD_NAME, newfld);
			        FLD.RDB$FIELD_NAME.NULL = FALSE;
				old_request = request;
				request = NULL;
    			        FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
				    DOM IN RDB$RELATION_FIELDS WITH DOM.RDB$FIELD_SOURCE EQ orig_dom->dyn_fld_name
			                MODIFY DOM USING
			                    strcpy (DOM.RDB$FIELD_SOURCE, newfld);
			                    DOM.RDB$FIELD_SOURCE.NULL = FALSE;
			                END_MODIFY;
				    modify_lfield_index (tdbb, dbb, gbl, DOM.RDB$RELATION_NAME,
						DOM.RDB$FIELD_NAME, DOM.RDB$FIELD_NAME);	
			        END_FOR;
				CMP_release (tdbb, request);
				request = old_request;
			    END_MODIFY;
			    }
			else
			    {
                            DYN_error_punt (FALSE, 204, orig_dom->dyn_fld_name, newfld, NULL, NULL, NULL);
                            /* msg 204: Cannot rename domain %s to %s.  A domain with that name already exists. */
			    }
 			}
		    break;
		    }

		case gds__dyn_rel_name:
		    GET_STRING (ptr, new_dom->dyn_rel_name);
		    break;

		case gds__dyn_fld_length:
		    fldlen = TRUE;
		    new_dom->dyn_dsc.dsc_length = DYN_get_number (ptr);
		    break;

		case gds__dyn_fld_type:
		    dtype = TRUE;
                    new_dom->dyn_dtype = DYN_get_number (ptr);

		    switch (new_dom->dyn_dtype)
			{
			case blr_short :	
			    new_dom->dyn_dsc.dsc_length = 2; 
			    break;

			case blr_long :
			case blr_float:
			    new_dom->dyn_dsc.dsc_length = 4; 
			    break;

			case blr_int64:
			case blr_sql_time:
			case blr_sql_date:
			case blr_timestamp:
			case blr_double:
			case blr_d_float:
			    new_dom->dyn_dsc.dsc_length = 8;
			    break;
			
			default:
			    break;
			}
		    break;

		case gds__dyn_fld_scale:
		    scale = TRUE;
                    new_dom->dyn_dsc.dsc_scale = DYN_get_number (ptr);
		    break;

                case isc_dyn_fld_precision:
                    prec = TRUE;
                    new_dom->dyn_precision = DYN_get_number (ptr);
                    break;

		case gds__dyn_fld_sub_type:
                    subtype = TRUE;
		    new_dom->dyn_dsc.dsc_sub_type = DYN_get_number (ptr);
		    break;

	    	case gds__dyn_fld_char_length:
                    charlen = TRUE; 
                    new_dom->dyn_charlen = DYN_get_number (ptr);
		    break;

	    	case gds__dyn_fld_collation:
                    collation = TRUE;
		    new_dom->dyn_collation = DYN_get_number (ptr);
		    break;

                case gds__dyn_fld_character_set:
                    charset = TRUE;
                    new_dom->dyn_charset = DYN_get_number (ptr);
		    break;

		case gds__dyn_fld_not_null:
		    nullflg = TRUE;
		    new_dom->dyn_null_flag = TRUE;
		    break;

		case gds__dyn_fld_query_name:
	 	    qryname = (TEXT*) *ptr;
		    bqryname = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

		case gds__dyn_fld_query_header:
	 	    qryhdr = (TEXT*) *ptr;
		    bqryhdr = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_fld_query_header2:
	 	    qryhdr2 = (TEXT*) *ptr;
		    bqryhdr2 = TRUE;
		    DYN_skip_attribute2 (ptr);
		    break;
#endif

		case gds__dyn_fld_edit_string:
	 	    edtstr = (TEXT*) *ptr;
		    bedtstr = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_fld_edit_string2:
	 	    edtstr2 = (TEXT*) *ptr;
		    bedtstr2 = TRUE;
		    DYN_skip_attribute2 (ptr);
		    break;
#endif
		case gds__dyn_fld_missing_value:
	 	    missingval = (TEXT*) *ptr;
		    bmissingval = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

		case gds__dyn_single_validation:
		    if (single_validate)
			{
			EXE_unwind (tdbb, request);
			DYN_error_punt (FALSE, 160, NULL, NULL, NULL, NULL, NULL);
			/* msg 160: "Only one constraint allowed for a domain" */
			break;
			}
		    else	
			single_validate = TRUE;
		    break;

		case gds__dyn_fld_validation_blr:
		    if (single_validate && (!FLD.RDB$VALIDATION_BLR.NULL))
			{
			EXE_unwind (tdbb, request);
			DYN_error_punt (FALSE, 160, NULL, NULL, NULL, NULL, NULL);
			/* msg 160: "Only one constraint allowed for a domain" */
			break;
			}
		    else 
			single_validate = TRUE;

		    fldvald = (TEXT*) *ptr;
	    	    bfldvald = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

		case gds__dyn_fld_validation_source:
		    fldvaldsrc = (TEXT*) *ptr;
		    bfldvaldsrc = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_fld_validation_source2:
		    fldvaldsrc2 = (TEXT*) *ptr;
		    bfldvaldsrc2 = TRUE;
		    DYN_skip_attribute2 (ptr);
		    break;
#endif

		case gds__dyn_description:
		    desc = (TEXT*) *ptr;
		    bdesc = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_description2:
		    desc2 = (TEXT*) *ptr;
		    bdesc2 = TRUE;
		    DYN_skip_attribute2 (ptr);
		    break;
#endif
		case gds__dyn_del_validation:
		    bdelvald = TRUE;
		    break;

		case gds__dyn_del_default:
		    bdeldflt = TRUE;
		    break;

		case gds__dyn_fld_default_value:
		    flddftval = (TEXT*) *ptr;
		    bflddftval = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

		case gds__dyn_fld_default_source:
		    flddfltsrc = (TEXT*) *ptr;
		    bflddfltsrc = TRUE;
		    DYN_skip_attribute (ptr);
		    break;

		case gds__dyn_fld_dimensions:
		    new_dom->dyn_dtype = blr_blob;
		    break;

                /* These should only be defined for BLOB types and should not come through with
                 * any other types.  Do nothing with them.
                 */

		case gds__dyn_fld_segment_length:
                    DYN_get_number (ptr);
		    break;

		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, relation_name, field_name,
				 NULL_PTR, NULL_PTR, NULL_PTR);
		}
    
			 
    /* Now that we have all of the information needed, let's check to see if the field type can be modifed.  Only
     * do this, however, if we are actually modifying the datatype of the domain.
     */

    if (dtype)
        {
	ULONG retval;

        DSC_make_descriptor (&new_dom->dyn_dsc,
    			 new_dom->dyn_dtype,
			 new_dom->dyn_dsc.dsc_scale,
			 new_dom->dyn_dsc.dsc_length,
			 new_dom->dyn_dsc.dsc_sub_type,
			 new_dom->dyn_charset,
			 new_dom->dyn_collation);
        if ((retval = check_update_fld_type (orig_dom, new_dom)) != SUCCESS)
	    modify_err_punt (tdbb, retval, orig_dom, new_dom);
	}

    MODIFY FLD USING
	if (dtype) 
	    {
	    FLD.RDB$FIELD_TYPE = new_dom->dyn_dtype;
	    FLD.RDB$FIELD_TYPE.NULL = FALSE;

	    /* If the datatype was changed, update any indexes that involved the domain */

	    old_request = request;
	    request = NULL;
	    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		DOM IN RDB$RELATION_FIELDS WITH DOM.RDB$FIELD_SOURCE EQ orig_dom->dyn_fld_name
			modify_lfield_index (tdbb, dbb, gbl, DOM.RDB$RELATION_NAME,
					DOM.RDB$FIELD_NAME, DOM.RDB$FIELD_NAME);	
	    END_FOR;
	    CMP_release (tdbb, request);
	    request = old_request;
	    }

	if (scale)
	    {
	    FLD.RDB$FIELD_SCALE = new_dom->dyn_dsc.dsc_scale;
	    FLD.RDB$FIELD_SCALE.NULL = FALSE;
	    }

	if (prec)
	    {
	    FLD.RDB$FIELD_PRECISION = new_dom->dyn_precision;
	    FLD.RDB$FIELD_PRECISION.NULL = FALSE;
	    }

	if (subtype)
	    {
	    FLD.RDB$FIELD_SUB_TYPE = new_dom->dyn_dsc.dsc_sub_type;
	    FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	    }

	if (charlen)
	    {
	    FLD.RDB$CHARACTER_LENGTH = new_dom->dyn_charlen;
	    FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
	    }

	if (fldlen)
	    {
	    FLD.RDB$FIELD_LENGTH = new_dom->dyn_dsc.dsc_length;
	    FLD.RDB$FIELD_LENGTH.NULL = FALSE;
	    }

	if (bqryname)
	    {
	    if (GET_STRING (&qryname, FLD.RDB$QUERY_NAME))
		FLD.RDB$QUERY_NAME.NULL = FALSE;
	    else
		FLD.RDB$QUERY_NAME.NULL = TRUE;
	    }

	if (bqryhdr)
	    {
	    if (DYN_put_blr_blob (gbl, &qryhdr, &FLD.RDB$QUERY_HEADER))
		FLD.RDB$QUERY_HEADER.NULL = FALSE;
	    else
		FLD.RDB$QUERY_HEADER.NULL = TRUE;
	    }

#if (defined JPN_SJIS || defined JPN_EUC)
	if (bqryhdr2)
	    {
	    if (DYN_put_blr_blob2 (gbl, &qryhdr2, &FLD.RDB$QUERY_HEADER))
		FLD.RDB$QUERY_HEADER.NULL = FALSE;
	    else
		FLD.RDB$QUERY_HEADER.NULL = TRUE;
	    }
#endif

	if (bedtstr)
	    {
	    if (GET_STRING (&edtstr, FLD.RDB$EDIT_STRING))
		FLD.RDB$EDIT_STRING.NULL = FALSE;
	    else
		FLD.RDB$EDIT_STRING.NULL = TRUE;
	    }

#if (defined JPN_SJIS || defined JPN_EUC)
	if (bedtstr2)
	    { 
	    if (DYN_get_string2 (&edtstr2, FLD.RDB$EDIT_STRING, sizeof( FLD.RDB$EDIT_STRING)))
		FLD.RDB$EDIT_STRING.NULL = FALSE;
	    else
		FLD.RDB$EDIT_STRING.NULL = TRUE;
	    }
#endif

	if (bmissingval)
	    {
	    if (DYN_put_blr_blob (gbl, &missingval, &FLD.RDB$MISSING_VALUE))
		FLD.RDB$MISSING_VALUE.NULL = FALSE;
	    else
		FLD.RDB$MISSING_VALUE.NULL = TRUE;
	    }

	if (bfldvald)
	    {
	    if (DYN_put_blr_blob (gbl, &fldvald, &FLD.RDB$VALIDATION_BLR))
		FLD.RDB$VALIDATION_BLR.NULL = FALSE;
	    else    
		FLD.RDB$VALIDATION_BLR.NULL = TRUE;
	    }

	if (bfldvaldsrc)
	    {
	    if (DYN_put_text_blob (gbl, &fldvaldsrc, &FLD.RDB$VALIDATION_SOURCE))
		FLD.RDB$VALIDATION_SOURCE.NULL = FALSE;
	    else
		    FLD.RDB$VALIDATION_SOURCE.NULL = TRUE;
	    }

#if (defined JPN_SJIS || defined JPN_EUC)
	if (bfldvaldsrc2)
	    {
	    if (DYN_put_text_blob2 (gbl, &fldvaldsrc2, &FLD.RDB$VALIDATION_SOURCE))
		FLD.RDB$VALIDATION_SOURCE.NULL = FALSE;
	    else
		FLD.RDB$VALIDATION_SOURCE.NULL = TRUE;
	    }
#endif

	if (bdesc)
	    {
	    if (DYN_put_text_blob (gbl, &desc, &FLD.RDB$DESCRIPTION))
		FLD.RDB$DESCRIPTION.NULL = FALSE;
	    else
		FLD.RDB$DESCRIPTION.NULL = TRUE;
	    }

#if (defined JPN_SJIS || defined JPN_EUC)
	if (bdesc2)
	    {
	    if (DYN_put_text_blob2 (gbl, &desc2, &FLD.RDB$DESCRIPTION))
		FLD.RDB$DESCRIPTION.NULL = FALSE;
	    else
		FLD.RDB$DESCRIPTION.NULL = TRUE;
	    }
#endif
	if (bdelvald)
	    {
	    FLD.RDB$VALIDATION_BLR.NULL = TRUE;
	    FLD.RDB$VALIDATION_SOURCE.NULL = TRUE;
	    }

	if (bdeldflt)
	    {
	    FLD.RDB$DEFAULT_VALUE.NULL = TRUE;
	    FLD.RDB$DEFAULT_SOURCE.NULL = TRUE;
	    }

	if (bflddftval)
	    {
	    FLD.RDB$DEFAULT_VALUE.NULL = FALSE;
	    DYN_put_blr_blob (gbl, &flddftval, &FLD.RDB$DEFAULT_VALUE);
	    }

	if (bflddfltsrc)
	    {
	    FLD.RDB$DEFAULT_SOURCE.NULL = FALSE;
	    DYN_put_text_blob (gbl, &flddfltsrc, &FLD.RDB$DEFAULT_SOURCE);
	    }

    END_MODIFY;
END_FOR;

if (!DYN_REQUEST (drq_m_gfield))
    DYN_REQUEST (drq_m_gfield) = request;

if (orig_dom)
    gds__free ((SLONG*)orig_dom);

if (new_dom)
    gds__free ((SLONG*) new_dom);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 89, NULL, NULL, NULL, NULL, NULL);
    /* msg 89: "Global field not found" */
}

void DYN_modify_index (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Modify an existing index
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
UCHAR	verb;
TEXT	name [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_m_index, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 91, NULL, NULL, NULL, NULL, NULL);
    /* msg 91: "MODIFY RDB$INDICES failed" */
    }

GET_STRING (ptr, name);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	IDX IN RDB$INDICES WITH IDX.RDB$INDEX_NAME EQ name
    if (!DYN_REQUEST (drq_m_index))
	DYN_REQUEST (drq_m_index) = request;

    found = TRUE;
    MODIFY IDX USING
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

		case gds__dyn_description:
		    if (DYN_put_text_blob (gbl, ptr, &IDX.RDB$DESCRIPTION))
			IDX.RDB$DESCRIPTION.NULL = FALSE;
		    else
			IDX.RDB$DESCRIPTION.NULL = TRUE;
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_description2:
		    if (DYN_put_text_blob2 (gbl, ptr, &IDX.RDB$DESCRIPTION))
			IDX.RDB$DESCRIPTION.NULL = FALSE;
		    else
			IDX.RDB$DESCRIPTION.NULL = TRUE;
		    break;
#endif

		/* For V4 index selectivity can be set only to -1 */

		case gds__dyn_idx_statistic:
		    IDX.RDB$STATISTICS = -1.0;
		    IDX.RDB$STATISTICS.NULL = FALSE;
		    break;

		default:
		    DYN_unsupported_verb();
		}
    END_MODIFY;
END_FOR;
if (!DYN_REQUEST (drq_m_index))
    DYN_REQUEST (drq_m_index) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 93, NULL, NULL, NULL, NULL, NULL);
    /* msg 93: "Index field not found" */
}

void DYN_modify_local_field (
    GBL		gbl,
    UCHAR	**ptr, 
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ l o c a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found, position, existing_position;
USHORT	sfflag, qnflag, qhflag, esflag, dflag, system_flag, scflag, nnflag, ntflag, npflag;
UCHAR	verb; 
TEXT	f [32], r [32], *query_name, *query_header, *edit_string, *description,
	*security_class, *new_name;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

GET_STRING (ptr, f);
r[0] = 0;
sfflag = qnflag = qhflag = esflag = dflag = scflag = npflag = nnflag = ntflag = FALSE;
while ((verb = *(*ptr)++) != gds__dyn_end)
    switch (verb)
	{
	case gds__dyn_rel_name:
	    GET_STRING (ptr, r);
	    break;

	case gds__dyn_system_flag:
	    system_flag = DYN_get_number (ptr);
	    sfflag = TRUE;
	    break;

	case gds__dyn_fld_position:
	    position = DYN_get_number (ptr);
	    npflag = TRUE;
	    break;

	case isc_dyn_new_fld_name:
	    new_name = (TEXT*) *ptr;
	    nnflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

	case gds__dyn_fld_query_name:
	    query_name = (TEXT*) *ptr;
	    qnflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

	case gds__dyn_fld_query_header:
	    query_header = (TEXT*) *ptr;
	    qhflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

#if (defined JPN_SJIS || defined JPN_EUC)
	case gds__dyn_fld_query_header2:
	    query_header = (TEXT*) *ptr;
	    qhflag = TRUE;
	    DYN_skip_attribute2 (ptr);
	    break;
#endif

	case gds__dyn_fld_edit_string:
	    edit_string = (TEXT*) *ptr;
	    esflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

#if (defined JPN_SJIS || defined JPN_EUC)
	case gds__dyn_fld_edit_string2:
	    edit_string = (TEXT*) *ptr;
	    esflag = TRUE;
	    DYN_skip_attribute2 (ptr);
	    break;
#endif

	case gds__dyn_security_class:
	    security_class = (TEXT*) *ptr;
	    scflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

	case gds__dyn_description:
	    description = (TEXT*) *ptr;
	    dflag = TRUE;
	    DYN_skip_attribute (ptr);
	    break;

#if (defined JPN_SJIS || defined JPN_EUC)
	case gds__dyn_description2:
	    description = (TEXT*) *ptr;
	    dflag = TRUE;
	    DYN_skip_attribute2 (ptr);
	    break;
#endif

	default:
	    --(*ptr);
	    DYN_execute (gbl, ptr, relation_name, field_name,
			 NULL_PTR, NULL_PTR, NULL_PTR);
	}

request = (BLK) CMP_find_request (tdbb, drq_m_lfield, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 95, NULL, NULL, NULL, NULL, NULL);
    /* msg 95: "MODIFY RDB$RELATION_FIELDS failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	FLD IN RDB$RELATION_FIELDS
	WITH FLD.RDB$FIELD_NAME EQ f AND FLD.RDB$RELATION_NAME EQ r
    if (!DYN_REQUEST (drq_m_lfield))
	DYN_REQUEST (drq_m_lfield) = request;

    found = TRUE;
	
    MODIFY FLD USING
	if (npflag)
	    existing_position = FLD.RDB$FIELD_POSITION;

	if (sfflag)
	    {
	    FLD.RDB$SYSTEM_FLAG = system_flag;
	    FLD.RDB$SYSTEM_FLAG.NULL = FALSE;
	    }
	if (qnflag)
	    {
	    if (GET_STRING (&query_name, FLD.RDB$QUERY_NAME))
		FLD.RDB$QUERY_NAME.NULL = FALSE;
	    else
		FLD.RDB$QUERY_NAME.NULL = TRUE;
	    }

	if (nnflag)
	    {
	    TEXT  new_fld[FLD_NAME_LEN];
            GET_STRING (&new_name, new_fld);

	    check_view_dependency (tdbb, dbb, gbl, r, f);
	    check_sptrig_dependency (tdbb, dbb, gbl, r, f);
	    if (!field_exists (tdbb, dbb, gbl, r, new_fld))
	        {
	        strcpy (FLD.RDB$FIELD_NAME, new_fld);
	        modify_lfield_index (tdbb, dbb, gbl, r, f, FLD.RDB$FIELD_NAME);
	        }
	    else
	        {
    	        DYN_error_punt (FALSE, 205, f, new_fld, r, NULL, NULL); 
              /* msg 205: Cannot rename field %s to %s.  A field with that name already exists in table %s. */
	        }
	    }

	if (qhflag)
	    {
#if ( !(defined JPN_SJIS || defined JPN_EUC))
	    if (DYN_put_blr_blob (gbl, &query_header, &FLD.RDB$QUERY_HEADER))
#else
	    if (DYN_put_blr_blob2 (gbl, &query_header, &FLD.RDB$QUERY_HEADER))
#endif
		FLD.RDB$QUERY_HEADER.NULL = FALSE;
	    else
		FLD.RDB$QUERY_HEADER.NULL = TRUE;
	    }
	if (esflag)
	    {
#if (! (defined JPN_SJIS || defined JPN_EUC) )
	    if (GET_STRING (&edit_string, FLD.RDB$EDIT_STRING))

#else
	    if (DYN_get_string2 (&edit_string, FLD.RDB$EDIT_STRING, sizeof(FLD.RDB$EDIT_STRING)))

#endif
		FLD.RDB$EDIT_STRING.NULL = FALSE;
	    else
		FLD.RDB$EDIT_STRING.NULL = TRUE;
	    }
	if (scflag)
	    {
	    if (GET_STRING (&security_class, FLD.RDB$SECURITY_CLASS))
		FLD.RDB$SECURITY_CLASS.NULL = FALSE;
	    else
		FLD.RDB$SECURITY_CLASS.NULL = TRUE;
	    }
	if (dflag)
	    {
#if (defined JPN_EUC || defined JPN_SJIS)
	    if (DYN_put_text_blob2 (gbl, &description, &FLD.RDB$DESCRIPTION))
#else
	    if (DYN_put_text_blob (gbl, &description, &FLD.RDB$DESCRIPTION))
#endif /* (defined JPN_EUC || defined JPN_SJIS) */
		FLD.RDB$DESCRIPTION.NULL = FALSE;
	    else
		FLD.RDB$DESCRIPTION.NULL = TRUE;
	    }
    END_MODIFY;
END_FOR;

if (!DYN_REQUEST (drq_m_lfield))
    DYN_REQUEST (drq_m_lfield) = request;

if (npflag && found)
    modify_lfield_position (tdbb, dbb, gbl, r, f, position, existing_position);


tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 96, NULL, NULL, NULL, NULL, NULL);
    /* msg 96: "Local column not found" */
}

void DYN_modify_procedure (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ p r o c e d u r e
 *
/**************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement.
 *
 **************************************/
TDBB    tdbb;
DBB	dbb;
BLK	request;
UCHAR	verb;
USHORT	found;
TEXT	procedure_name [PROC_NAME_SIZE];
JMP_BUF	env, *old_env;

GET_STRING (ptr, procedure_name);

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

request = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 141, NULL, NULL, NULL, NULL, NULL);
    /* msg 141: "MODIFY RDB$PROCEDURES failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_m_prcs, DYN_REQUESTS);

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_NAME = procedure_name
    if (!DYN_REQUEST (drq_m_prcs))
	DYN_REQUEST (drq_m_prcs) = request;
    found = TRUE;

    /* Set NULL flags to TRUE only for fields which must be specified in the DYN string.
       Retain existing values on other fields (RDB$DESCRIPTION, RDB$SECURITY_CLASS),
       unless explicitly changed in the DYN string */
    MODIFY P
	P.RDB$SYSTEM_FLAG.NULL = TRUE;
	P.RDB$PROCEDURE_BLR.NULL = TRUE;
	P.RDB$PROCEDURE_SOURCE.NULL = TRUE;
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
	    
		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, NULL_PTR, NULL_PTR,
				NULL_PTR, NULL_PTR, procedure_name);
		}

    END_MODIFY;
END_FOR;

if (!DYN_REQUEST (drq_m_prcs))
    DYN_REQUEST (drq_m_prcs) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (!found)
    DYN_error_punt (FALSE, 140, procedure_name, NULL, NULL, NULL, NULL);
    /* msg 140: "Procedure %s not found" */
}

void DYN_modify_relation (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Modify an existing relation
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
UCHAR	verb;
TEXT	name [32], field_name [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

field_name[0] = '\0';
GET_STRING (ptr, name);

request = (BLK) CMP_find_request (tdbb, drq_m_relation, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 99, NULL, NULL, NULL, NULL, NULL);
    /* msg 99: "MODIFY RDB$RELATIONS failed" */
    }

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	REL IN RDB$RELATIONS WITH REL.RDB$RELATION_NAME EQ name
    if (!DYN_REQUEST (drq_m_relation))
	DYN_REQUEST (drq_m_relation) = request;

    if (!REL.RDB$VIEW_BLR.NULL)
        DYN_error_punt (FALSE, 177, NULL, NULL, NULL, NULL, NULL);
    found = TRUE;
    MODIFY REL USING
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_system_flag:
		    REL.RDB$SYSTEM_FLAG = DYN_get_number (ptr);
		    REL.RDB$SYSTEM_FLAG.NULL = FALSE;
		    break;

		case gds__dyn_security_class:
		    if (GET_STRING (ptr, REL.RDB$SECURITY_CLASS))
			REL.RDB$SECURITY_CLASS.NULL = FALSE;
		    else
			REL.RDB$SECURITY_CLASS.NULL = TRUE;
		    break;
	    
		case gds__dyn_rel_ext_file:
		    if (REL.RDB$EXTERNAL_FILE.NULL)
			{
			DYN_rundown_request (old_env, request, -1);
			DYN_error_punt (FALSE, 97, NULL, NULL, NULL, NULL, NULL);
			/* msg 97: "add EXTERNAL FILE not allowed" */
			}
		    if (!GET_STRING (ptr, REL.RDB$EXTERNAL_FILE))
			{
			DYN_rundown_request (old_env, request, -1);
			DYN_error_punt (FALSE, 98, NULL, NULL, NULL, NULL, NULL);
			/* msg 98: "drop EXTERNAL FILE not allowed" */
			}
		    break;
	    
		case gds__dyn_description:
		    if (DYN_put_text_blob (gbl, ptr, &REL.RDB$DESCRIPTION))
			REL.RDB$DESCRIPTION.NULL = FALSE;
		    else
			REL.RDB$DESCRIPTION.NULL = TRUE;
		    break;

#if (defined JPN_SJIS || defined JPN_EUC)
		case gds__dyn_description2:
		    if (DYN_put_text_blob2 (gbl, ptr, &REL.RDB$DESCRIPTION))
			REL.RDB$DESCRIPTION.NULL = FALSE;
		    else
			REL.RDB$DESCRIPTION.NULL = TRUE;
		    break;
#endif
		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, name, field_name,
				 NULL_PTR, NULL_PTR, NULL_PTR);
		}
    END_MODIFY;
END_FOR;
if (!DYN_REQUEST (drq_m_relation))
    DYN_REQUEST (drq_m_relation) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (!found)
    DYN_error_punt (FALSE, 101, NULL, NULL, NULL, NULL, NULL);
    /* msg 101: "Relation field not found" */
}

void DYN_modify_trigger (
    GBL		gbl,
    UCHAR	**ptr)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Modify a trigger for a relation.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
USHORT	found;
UCHAR	verb, *blr;
TEXT	trigger_name [32], *source;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_m_trigger, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 102, NULL, NULL, NULL, NULL, NULL);
    /* msg 102: "MODIFY TRIGGER failed" */
    }

GET_STRING (ptr, trigger_name);
source = NULL;
blr = NULL;

found = FALSE;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGERS WITH X.RDB$TRIGGER_NAME EQ trigger_name
    if (!DYN_REQUEST (drq_m_trigger))
	DYN_REQUEST (drq_m_trigger) = request;

    found = TRUE;
    MODIFY X
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_trg_name:
		    GET_STRING (ptr, X.RDB$TRIGGER_NAME);
		    break;

		case gds__dyn_trg_type:
		    X.RDB$TRIGGER_TYPE = DYN_get_number (ptr);
		    X.RDB$TRIGGER_TYPE.NULL = FALSE;
		    break;
		
		case gds__dyn_trg_sequence:
		    X.RDB$TRIGGER_SEQUENCE = DYN_get_number (ptr);
		    X.RDB$TRIGGER_SEQUENCE.NULL = FALSE;
		    break;
		
		case gds__dyn_trg_inactive:
		    X.RDB$TRIGGER_INACTIVE = DYN_get_number (ptr);
		    X.RDB$TRIGGER_INACTIVE.NULL = FALSE;
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
		    DYN_execute (gbl, ptr, NULL_PTR, NULL_PTR,
				 trigger_name, NULL_PTR, NULL_PTR);
		}
    END_MODIFY;
END_FOR;
if (!DYN_REQUEST (drq_m_trigger))
    DYN_REQUEST (drq_m_trigger) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (!found)
    DYN_error_punt (FALSE, 147, trigger_name, NULL, NULL, NULL, NULL);
    /* msg 147: "Trigger %s not found" */
}

void DYN_modify_trigger_msg (
    GBL		gbl,
    UCHAR	**ptr,
    TEXT	*trigger_name)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Modify a trigger message.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
int	number;
UCHAR	verb;
TEXT	t [32];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_m_trg_msg, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 105, NULL, NULL, NULL, NULL, NULL);
    /* msg 105: "MODIFY TRIGGER MESSAGE failed" */
    }

number = DYN_get_number (ptr);
if (trigger_name)
    strcpy (t, trigger_name);
else if (*(*ptr)++ == gds__dyn_trg_name)
    GET_STRING (ptr, t);
else
    DYN_error_punt (FALSE, 103, NULL, NULL, NULL, NULL, NULL);
    /* msg 103: "TRIGGER NAME expected" */

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$TRIGGER_MESSAGES
	WITH X.RDB$MESSAGE_NUMBER EQ number AND X.RDB$TRIGGER_NAME EQ t
    if (!DYN_REQUEST (drq_m_trg_msg))
	DYN_REQUEST (drq_m_trg_msg) = request;

    MODIFY X
	while ((verb = *(*ptr)++) != gds__dyn_end)
	    switch (verb)
		{
		case gds__dyn_trg_msg_number:
		    X.RDB$MESSAGE_NUMBER = DYN_get_number (ptr);
		    X.RDB$MESSAGE_NUMBER.NULL = FALSE;
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
    END_MODIFY;
END_FOR;
if (!DYN_REQUEST (drq_m_trg_msg))
    DYN_REQUEST (drq_m_trg_msg) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

static void drop_cache (
    GBL		gbl)
{
/**************************************
 *
 *	d r o p _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Drop the database cache
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
JMP_BUF	env, *old_env;
USHORT	found = FALSE;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_d_cache, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
{
	DYN_rundown_request (old_env, request, drq_s_cache);
	DYN_error_punt (TRUE, 63, NULL, NULL, NULL, NULL, NULL);
	/* msg 63: ERASE RDB$FILE failed */
}

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$FILES WITH X.RDB$FILE_FLAGS EQ FILE_cache

    ERASE X;
    found = TRUE;

END_FOR;

if (!DYN_REQUEST (drq_d_cache))
    DYN_REQUEST (drq_d_cache) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (!found)
    DYN_error_punt (FALSE, 149, NULL, NULL, NULL, NULL, NULL);
    /* msg 149: "Shared cache file not found" */
}

static void drop_log (
    GBL		gbl)
{
/**************************************
 *
 *	d r o p _ l o g
 *
 **************************************
 *
 * Functional description
 *	Delete all log files
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
JMP_BUF	env, *old_env;
USHORT	found = FALSE;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_d_log, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
{
	DYN_rundown_request (old_env, request, drq_d_log);
	DYN_error_punt (TRUE, 153, NULL, NULL, NULL, NULL, NULL);
	/* msg 153: ERASE RDB$LOG_FILE failed */
}

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	X IN RDB$LOG_FILES 

    ERASE X;
    found = TRUE;

END_FOR;

if (!DYN_REQUEST (drq_d_log))
    DYN_REQUEST (drq_d_log) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (!found)
    DYN_error_punt (FALSE, 152, NULL, NULL, NULL, NULL, NULL);
    /* msg 152: "Write ahead log not found" */	
}

static void modify_lfield_position (TDBB tdbb,
				DBB dbb,
				GBL gbl,
                                TEXT *relation_name,
				TEXT *field_name,
				USHORT new_position,
				USHORT existing_position)
{
/***********************************************************
 *
 *  m o d i f y _ l f i e l d _ p o s i t i o n
 ***********************************************************
 *
 *  Functional Description:
 *     Alters the position of a field with respect to the 
 *     other fields in the relation.  This will only affect
 *     the order in which the fields will be returned when either
 *     viewing the relation or performing select * from the relation.
 *
 *     The rules of engagement are as follows:
 *          if new_position > original position
 *             increase RDB$FIELD_POSITION for all fields with RDB$FIELD_POSITION 
 *             between the new_position and existing position of the field
 *	       then update the position of the field being altered.
 *              just update the position
 *
 *          if new_position < original position
 *             decrease RDB$FIELD_POSITION for all fields with RDB$FIELD_POSITION 
 *             between the new_position and existing position of the field
 *	       then update the position of the field being altered.
 *
 *	    if new_position == original_position -- no_op
 *
 ***********************************************************/
VOLATILE BLK	request = NULL;
USHORT		new_pos = 0;
SLONG		max_position = -1;
JMP_BUF		env, *old_env;
BOOLEAN		move_down = FALSE;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_error_punt (TRUE, 95, NULL, NULL, NULL, NULL, NULL);
    /* msg 95: "MODIFY RDB$RELATION_FIELDS failed" */ 
    }

/* Find the position of the last field in the relation */
DYN_UTIL_generate_field_position (tdbb, gbl, relation_name, &max_position);

/* if the existing position of the field is less than the new position of
 * the field, subtract 1 to move the fields to their new positions otherwise,
 * increase the value in RDB$FIELD_POSITION by one
 */

if (existing_position < new_position)
    move_down = TRUE;

/* Retrieve the records for the fields which have a position between the
 * existing field position and the new field position
 */

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FLD IN RDB$RELATION_FIELDS WITH FLD.RDB$RELATION_NAME EQ relation_name AND
    FLD.RDB$FIELD_POSITION >= MIN (new_position, existing_position) AND
    FLD.RDB$FIELD_POSITION <= MAX (new_position, existing_position)
    MODIFY FLD USING
        /* If the field is the one we want, change the position, otherwise
         * increase the value of RDB$FIELD_POSITION
         */
	MET_exact_name (FLD.RDB$FIELD_NAME);
        if (strcmp (FLD.RDB$FIELD_NAME, field_name) == 0)
           {
           if (new_position > max_position)
               /* This prevents gaps in the position sequence of the
                * fields */
               FLD.RDB$FIELD_POSITION = max_position;
           else
               FLD.RDB$FIELD_POSITION = new_position;
	   }
        else 
            {
            if (move_down)
                FLD.RDB$FIELD_POSITION = FLD.RDB$FIELD_POSITION-1;
            else
                FLD.RDB$FIELD_POSITION = FLD.RDB$FIELD_POSITION+1;
            }
	    
        FLD.RDB$FIELD_POSITION.NULL = FALSE;
    END_MODIFY;
END_FOR;
CMP_release (tdbb, request); 
request = NULL;

/* Once the field position has been changed, make sure that there are no
 * duplicate field positions and no gaps in the position sequence (this can
 * not be guaranteed by the query above */

new_pos = 0;
FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FLD IN RDB$RELATION_FIELDS WITH FLD.RDB$RELATION_NAME EQ relation_name
    SORTED BY ASCENDING FLD.RDB$FIELD_POSITION
    if (FLD.RDB$FIELD_POSITION != new_pos)
	{
    	MODIFY FLD USING
            FLD.RDB$FIELD_POSITION = new_pos;
	END_MODIFY;
	}
    new_pos += 1;
END_FOR;
CMP_release (tdbb, request); 
tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

static BOOLEAN check_view_dependency (TDBB tdbb,
				      DBB  dbb,
				      GBL  gbl,
				      TEXT *relation_name,
				      TEXT *field_name)
{				       
/***********************************************************
 *
 *  c h e c k _ v i e w _ d e p e n d e n c y
 ***********************************************************
 *
 *  Functional Description:
 *	Checks to see if the given field is referenced in a view. If the field
 *	is referenced in a view, return TRUE, else return FALSE
 *
 ***********************************************************/
BLK	request = NULL;
BOOLEAN retval = FALSE;
TEXT	*view_name;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FIRST 1 
    X IN RDB$RELATION_FIELDS CROSS Y IN RDB$RELATION_FIELDS CROSS
    Z IN RDB$VIEW_RELATIONS WITH
    X.RDB$RELATION_NAME EQ relation_name AND
    X.RDB$FIELD_NAME EQ field_name AND
    X.RDB$FIELD_NAME EQ Y.RDB$BASE_FIELD AND
    X.RDB$FIELD_SOURCE EQ Y.RDB$FIELD_SOURCE AND
    Y.RDB$RELATION_NAME EQ Z.RDB$VIEW_NAME AND
    X.RDB$RELATION_NAME EQ Z.RDB$RELATION_NAME AND
    Y.RDB$VIEW_CONTEXT EQ Z.RDB$VIEW_CONTEXT

    retval = TRUE;
    view_name = Z.RDB$VIEW_NAME;
END_FOR;

CMP_release (tdbb, request);

if (retval)
    DYN_error_punt (FALSE, 206, field_name, relation_name, view_name, NULL, NULL);
    /* msg 206: Column %s from table %s is referenced in  %s. */

return retval;

}

static BOOLEAN check_sptrig_dependency (TDBB tdbb,
				        DBB  dbb,
					GBL  gbl,
				        TEXT *relation_name,
				        TEXT *field_name)
{
/***********************************************************
 *
 *  c h e c k _ s p t r i g _ d e p e n d e n c y
 ***********************************************************
 *
 *  Functional Description:
 *	Checks to see if the given field is referenced in a stored procedure
 *	or trigger.  If the field is refereneced, return TRUE, else return
 *	FALSE 
 ***********************************************************/
BLK	request = NULL;
BOOLEAN	retval = FALSE;
TEXT	*dep_name;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FIRST 1 
    DEP IN RDB$DEPENDENCIES WITH
    DEP.RDB$DEPENDED_ON_NAME EQ relation_name AND
    DEP.RDB$FIELD_NAME EQ field_name

    retval = TRUE;
    dep_name = DEP.RDB$DEPENDENT_NAME;
END_FOR;

CMP_release (tdbb, request);

if (retval)
    DYN_error_punt (FALSE, 206, field_name, relation_name, dep_name, NULL, NULL);
    /* msg 206: Column %s from table %s is referenced in %s. */

return retval;
}

static void modify_lfield_index (TDBB tdbb,
				 DBB  dbb,
				 GBL  gbl,
				 TEXT *relation_name,
				 TEXT *field_name,
				 TEXT *new_fld_name)
{
/***********************************************************
 *
 *  m o d i f y _ l f i e l d _ i n d e x
 ***********************************************************
 *
 *  Functional Description:
 *	Updates the field names in an index and forces the index to be rebuilt
 *	with the new field names
 *
 ***********************************************************/
BLK	request = NULL;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    IDX IN RDB$INDICES CROSS IDXS IN RDB$INDEX_SEGMENTS WITH
    IDX.RDB$INDEX_NAME EQ IDXS.RDB$INDEX_NAME AND
    IDX.RDB$RELATION_NAME EQ relation_name AND
    IDXS.RDB$FIELD_NAME EQ field_name

    /* Change the name of the field in the index */
    MODIFY IDXS USING
        MOVE_FASTER (new_fld_name, IDXS.RDB$FIELD_NAME, sizeof(IDXS.RDB$FIELD_NAME));
    END_MODIFY;

    /* Set the index name to itself to tell the index to rebuild */
    MODIFY IDX USING
	MOVE_FASTER (IDX.RDB$INDEX_NAME, IDX.RDB$INDEX_NAME, sizeof(IDX.RDB$INDEX_NAME));
    END_MODIFY;
END_FOR;

CMP_release (tdbb, request);
}

static BOOLEAN  field_exists (TDBB tdbb,
			      DBB  dbb,
			      GBL  gbl,
		              TEXT *relation_name,
		              TEXT *field_name)
{
/***********************************************************
 *
 *  f i e l d _ e x i s t s
 ***********************************************************
 *
 *  Functional Description:
 *	Checks to see if the given field already exists in a relation
 ***********************************************************/
BLK	request = NULL;
BOOLEAN	retval = FALSE;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FLD IN RDB$RELATION_FIELDS WITH
    FLD.RDB$RELATION_NAME EQ relation_name AND
    FLD.RDB$FIELD_NAME EQ field_name
    retval = TRUE;
END_FOR;

CMP_release (tdbb, request);
return retval;
}

static BOOLEAN  domain_exists (TDBB tdbb,
			      DBB  dbb,
			      GBL  gbl,
		              TEXT *field_name)
{
/***********************************************************
 *
 *  d o m a i n _ e x i s t s
 ***********************************************************
 *
 *  Functional Description:
 *	Checks to see if the given field already exists in a relation
 ***********************************************************/
BLK	request = NULL;
BOOLEAN	retval = FALSE;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME EQ field_name
    retval = TRUE;
END_FOR;

CMP_release (tdbb, request);
return retval;
}

void DYN_modify_sql_field (
    GBL		gbl,
    UCHAR	**ptr, 
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	D Y N _ m o d i f y _ sql _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Execute a dynamic ddl statement 
 *      to modify the datatype of a field.
 *
 *  If there are dependencies on the field, abort the operation
 *  unless the dependency is an index.  In this case, rebuild the 
 *  index once the operation has completed.
 *
 *  If the original datatype of the field was a domain:
 *     if the new type is a domain, just make the change to the new domain
 *     if it exists
 *
 *     if the new type is a base type, just make the change
 *
 *  If the original datatype of the field was a base type:
 *     if the new type is a base type, just make the change
 *
 *     if the new type is a domain, make the change to the field
 *     definition and remove the entry for RDB$FIELD_SOURCE from the original
 *     field.  In other words ... clean up after ourselves
 *
 *  The following conversions are not allowed:
 *        Blob to anything
 *        Array to anything
 *        Date to anything
 *        Char to any numeric
 *        Varchar to any numeric
 *        Anything to Blob
 *        Anything to Array
 *
 *  The following operations return a warning
 *        Decreasing the length of a char (varchar) field
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request = NULL, first_request;
UCHAR	verb; 
JMP_BUF	env, *old_env;
BOOLEAN	found = FALSE, update_domain = FALSE;
BOOLEAN dtype, scale, prec, subtype, charlen, collation, fldlen, nullflg, charset;
DYN_FLD orig_fld,
	new_fld,
	dom_fld;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

dtype = scale = prec = subtype = charlen = collation = fldlen = nullflg = charset = FALSE;

if (SETJMP (env))
    {
    if (new_fld)
	gds__free ((SLONG*) new_fld);

    if (dom_fld)
	gds__free ((SLONG*) dom_fld);

    if (orig_fld)
	gds__free ((SLONG*) orig_fld);

    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    DYN_error_punt (TRUE, 95, NULL, NULL, NULL, NULL, NULL);
    /* msg 95: "MODIFY RDB$RELATION_FIELDS failed" */ 
    }

/* Allocate the field structures */
orig_fld = (DYN_FLD) gds__alloc(sizeof(struct dyn_fld));
if (!orig_fld)
    DYN_error_punt (FALSE, 211, NULL, NULL, NULL, NULL, NULL);

MOVE_CLEAR(orig_fld, sizeof(struct dyn_fld));

new_fld = (DYN_FLD) gds__alloc(sizeof(struct dyn_fld));
if (!new_fld)
    DYN_error_punt (FALSE, 211, NULL, NULL, NULL, NULL, NULL);
MOVE_CLEAR(new_fld, sizeof(struct dyn_fld));

dom_fld = (DYN_FLD) gds__alloc(sizeof(struct dyn_fld));
if (!dom_fld)
    DYN_error_punt (FALSE, 211, NULL, NULL, NULL, NULL, NULL);
MOVE_CLEAR(dom_fld, sizeof(struct dyn_fld));

GET_STRING (ptr, orig_fld->dyn_fld_name);

/* check to see if the field being altered is involved in any type of dependency.
 * If there is a dependency, call DYN_error_punt (inside the function).
 */
check_sptrig_dependency (tdbb, dbb, gbl, relation_name, orig_fld->dyn_fld_name);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	RFR IN RDB$RELATION_FIELDS WITH RFR.RDB$RELATION_NAME = relation_name
	AND RFR.RDB$FIELD_NAME = orig_fld->dyn_fld_name 

    first_request = request;
    request = NULL;

    found = TRUE;

    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	   FLD IN RDB$FIELDS WITH FLD.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE

        /* Get information about the original field type.  If a conversion 
         * can not be at any time made between
         * the two datatypes, error.
         */
        
	DSC_make_descriptor (&orig_fld->dyn_dsc,
			     FLD.RDB$FIELD_TYPE,
			     FLD.RDB$FIELD_SCALE,
			     FLD.RDB$FIELD_LENGTH,
			     FLD.RDB$FIELD_SUB_TYPE,
			     FLD.RDB$CHARACTER_SET_ID,
			     FLD.RDB$COLLATION_ID);

        orig_fld->dyn_dtype = FLD.RDB$FIELD_TYPE;
	orig_fld->dyn_precision = FLD.RDB$FIELD_PRECISION;
	orig_fld->dyn_charlen = FLD.RDB$CHARACTER_LENGTH;
	orig_fld->dyn_collation = FLD.RDB$COLLATION_ID;
	orig_fld->dyn_null_flag = FLD.RDB$NULL_FLAG;

        strcpy (orig_fld->dyn_fld_source, RFR.RDB$FIELD_SOURCE);

        /* If the original field type is an array, force its blr type to blr_blob */
        if (FLD.RDB$DIMENSIONS != 0)
   	    orig_fld->dyn_dtype = blr_blob;

	while ((verb = *(*ptr)++) != gds__dyn_end)
	    {
	    switch (verb)
		{
		case gds__dyn_fld_source:
		    GET_STRING (ptr, dom_fld->dyn_fld_source);
		    get_domain_type (tdbb, dbb, gbl, dom_fld);
		    update_domain = TRUE;
		    break;

		case gds__dyn_rel_name:
		    GET_STRING (ptr, new_fld->dyn_rel_name);
		    break;

		case gds__dyn_fld_length:
		    fldlen = TRUE;
		    new_fld->dyn_dsc.dsc_length = DYN_get_number (ptr);
		    break;

		case gds__dyn_fld_type:
		    dtype = TRUE;
                    new_fld->dyn_dtype = DYN_get_number (ptr);

		    switch (new_fld->dyn_dtype)
			{
			case blr_short :	
			    new_fld->dyn_dsc.dsc_length = 2; 
			    break;

			case blr_long :
			case blr_float:
			    new_fld->dyn_dsc.dsc_length = 4; 
			    break;

			case blr_int64:
			case blr_sql_time:
			case blr_sql_date:
			case blr_timestamp:
			case blr_double:
			case blr_d_float:
			    new_fld->dyn_dsc.dsc_length = 8;
			    break;
			
			default:
			    break;
			}
		    break;

		case gds__dyn_fld_scale:
		    scale = TRUE;
                    new_fld->dyn_dsc.dsc_scale = DYN_get_number (ptr);
		    break;

                case isc_dyn_fld_precision:
		    prec = TRUE;
                    new_fld->dyn_precision = DYN_get_number (ptr);
                    break;

		case gds__dyn_fld_sub_type:
		    subtype = TRUE;
		    new_fld->dyn_dsc.dsc_sub_type = DYN_get_number (ptr);
		    break;

	    	case gds__dyn_fld_char_length:
		    charlen = TRUE;
                    new_fld->dyn_charlen = DYN_get_number (ptr);
		    break;

	    	case gds__dyn_fld_collation:
		    collation = TRUE;
		    new_fld->dyn_collation = DYN_get_number (ptr);
		    break;

                case gds__dyn_fld_character_set:
		    charset = TRUE;
                    new_fld->dyn_charset = DYN_get_number (ptr);
		    break;

		case gds__dyn_fld_not_null:
		    nullflg = TRUE;
		    new_fld->dyn_null_flag = TRUE;
		    break;

		case gds__dyn_fld_dimensions:
		    new_fld->dyn_dtype = blr_blob;
		    break;

                /* These should only be defined for BLOB types and should not come through with
                 * any other types.  BLOB types are detected above
                 */

		case gds__dyn_fld_segment_length:
                    DYN_get_number (ptr);
		    break;

		default:
		    --(*ptr);
		    DYN_execute (gbl, ptr, relation_name, RFR.RDB$FIELD_SOURCE,
				 NULL_PTR, NULL_PTR, NULL_PTR);
		}
	    }
    END_FOR;   /* FLD in RDB$FIELDS */
    CMP_release (tdbb, request);
    request = NULL;

    /* Now that we have all of the information needed, let's check to see if the field type can be modifed */


    if (update_domain)
	{
	ULONG retval;

	DSC_make_descriptor (&dom_fld->dyn_dsc,
			 dom_fld->dyn_dtype,
			 dom_fld->dyn_dsc.dsc_scale,
			 dom_fld->dyn_dsc.dsc_length,
			 dom_fld->dyn_dsc.dsc_sub_type,
			 dom_fld->dyn_charset,
			 dom_fld->dyn_collation);

        if ((retval = check_update_fld_type (orig_fld, dom_fld)) != SUCCESS)
	    modify_err_punt (tdbb, retval, orig_fld, dom_fld);

	/* if the original definition was a base field type, remove the entries from RDB$FIELDS */
	if (!strncmp(orig_fld->dyn_fld_source, "RDB$", 4))
	    {
	    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		FLD IN RDB$FIELDS WITH FLD.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE
		ERASE FLD;
	    END_FOR;

	    CMP_release (tdbb, request);
	    request = NULL;
	    }
	request = first_request;
	MODIFY RFR USING
	    strcpy (RFR.RDB$FIELD_SOURCE, dom_fld->dyn_fld_source);
	    RFR.RDB$FIELD_SOURCE.NULL = FALSE;
	END_MODIFY;
	first_request = request;
	}
    else
	{
	ULONG retval;

	DSC_make_descriptor (&new_fld->dyn_dsc,
			 new_fld->dyn_dtype,
			 new_fld->dyn_dsc.dsc_scale,
			 new_fld->dyn_dsc.dsc_length,
			 new_fld->dyn_dsc.dsc_sub_type,
			 new_fld->dyn_charset,
			 new_fld->dyn_collation);

        if ((retval = check_update_fld_type (orig_fld, new_fld)) != SUCCESS)
	    modify_err_punt (tdbb, retval, orig_fld, new_fld);

	    /* check to see if the original data type for the field was based on a domain.  If it
	     * was (and now it isn't), remove the domain information and replace it with a generated
	     * field name for RDB$FIELDS
	     */

	if (strncmp(orig_fld->dyn_fld_source, "RDB$", 4))
	    {
	    request = first_request;
	    MODIFY RFR USING
		DYN_UTIL_generate_field_name (tdbb, gbl, RFR.RDB$FIELD_SOURCE);
		strcpy (new_fld->dyn_fld_name, RFR.RDB$FIELD_SOURCE);
	    END_MODIFY;
	    first_request = request;
	    request = NULL;

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

		if (dtype) 
		    {
		    FLD.RDB$FIELD_TYPE = new_fld->dyn_dtype;
		    FLD.RDB$FIELD_TYPE.NULL = FALSE;
		    }

		if (scale)
		    {
		    FLD.RDB$FIELD_SCALE = new_fld->dyn_dsc.dsc_scale;
		    FLD.RDB$FIELD_SCALE.NULL = FALSE;
		    }

		if (prec)
		    {
		    FLD.RDB$FIELD_PRECISION = new_fld->dyn_precision;
		    FLD.RDB$FIELD_PRECISION.NULL = FALSE;
		    }

		if (subtype)
		    {
		    FLD.RDB$FIELD_SUB_TYPE = new_fld->dyn_dsc.dsc_sub_type;
		    FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		    }

		if (charlen)
		    {
		    FLD.RDB$CHARACTER_LENGTH = new_fld->dyn_charlen;
		    FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
		    }

		if (fldlen)
		    {
		    FLD.RDB$FIELD_LENGTH = new_fld->dyn_dsc.dsc_length;
		    FLD.RDB$FIELD_LENGTH.NULL = FALSE;
		    }

		/* Copy the field name into RDB$FIELDS */
		strcpy (FLD.RDB$FIELD_NAME, new_fld->dyn_fld_name);
		END_STORE;

	    CMP_release (tdbb, request);
	    request = NULL;
	    }
	else  /* the original and new definitions are both base types */
	    {
	    FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
		FLD IN RDB$FIELDS WITH FLD.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE
		MODIFY FLD USING

		if (dtype) 
		    {
		    FLD.RDB$FIELD_TYPE = new_fld->dyn_dtype;
		    FLD.RDB$FIELD_TYPE.NULL = FALSE;
		    }

		if (scale)
		    {
		    FLD.RDB$FIELD_SCALE = new_fld->dyn_dsc.dsc_scale;
		    FLD.RDB$FIELD_SCALE.NULL = FALSE;
		    }

		if (prec)
		    {
		    FLD.RDB$FIELD_PRECISION = new_fld->dyn_precision;
		    FLD.RDB$FIELD_PRECISION.NULL = FALSE;
		    }

		if (subtype)
		    {
		    FLD.RDB$FIELD_SUB_TYPE = new_fld->dyn_dsc.dsc_sub_type;
		    FLD.RDB$FIELD_SUB_TYPE.NULL = FALSE;
		    }

		if (charlen)
		    {
		    FLD.RDB$CHARACTER_LENGTH = new_fld->dyn_charlen;
		    FLD.RDB$CHARACTER_LENGTH.NULL = FALSE;
		    }

		if (fldlen)
		    {
		    FLD.RDB$FIELD_LENGTH = new_fld->dyn_dsc.dsc_length;
		    FLD.RDB$FIELD_LENGTH.NULL = FALSE;
		    }

		END_MODIFY;
	    END_FOR;    /* FLD in RDB$FIELDS */
	    CMP_release (tdbb, request);
	    request = NULL;
	    }
    } /* else not a domain */
    request = first_request;
END_FOR;  /* RFR IN RDB$RELATION_FIELDS */
CMP_release (tdbb, request);
request = NULL;

if (!found)
    DYN_error_punt (FALSE, 96, NULL, NULL, NULL, NULL, NULL);
    /* msg 96: "Local column not found" */

/* Update any indicies that exist */
modify_lfield_index (tdbb, dbb, gbl, relation_name, orig_fld->dyn_fld_name, orig_fld->dyn_fld_name);

if (new_fld)
    gds__free ((SLONG*) new_fld);

if (dom_fld)
    gds__free ((SLONG*) dom_fld);

if (orig_fld)
    gds__free ((SLONG*) orig_fld);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void get_domain_type (TDBB	tdbb,
                      DBB	dbb,
                      GBL	gbl,
                      DYN_FLD dom_fld)
{
/**************************************
 *
 *	g e t _ d o m a i n _ t y p e	
 *
 **************************************
 *
 * Functional description
 *	Retrives the type information for a domain so
 *      that it can be compared to a local field before
 *      modifying the datatype of a field.
 *
 **************************************/
BLK	request = NULL;

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
    FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME EQ dom_fld->dyn_fld_source;

    DSC_make_descriptor (&dom_fld->dyn_dsc,
			 FLD.RDB$FIELD_TYPE,
			 FLD.RDB$FIELD_SCALE,
			 FLD.RDB$FIELD_LENGTH,
			 FLD.RDB$FIELD_SUB_TYPE,
			 FLD.RDB$CHARACTER_SET_ID,
			 FLD.RDB$COLLATION_ID);

    dom_fld->dyn_dtype = FLD.RDB$FIELD_TYPE;
    dom_fld->dyn_precision = FLD.RDB$FIELD_PRECISION;
    dom_fld->dyn_charlen = FLD.RDB$CHARACTER_LENGTH;
    dom_fld->dyn_collation = FLD.RDB$COLLATION_ID;
    dom_fld->dyn_null_flag = FLD.RDB$NULL_FLAG;

END_FOR;
CMP_release (tdbb, request);
}

static ULONG check_update_fld_type (DYN_FLD orig_fld,
				     DYN_FLD new_fld)
{
/**************************************
 *
 *	c h e c k _ u p d a t e _ f l d _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Compare the original field type with the new field type to
 *      determine if the original type can be changed to the new type
 *
 *  The following conversions are not allowed:
 *        Blob to anything
 *        Array to anything
 *        Date to anything
 *        Char to any numeric
 *        Varchar to any numeric
 *        Anything to Blob
 *        Anything to Array
 *
 *  This function returns an error code if the conversion can not be
 *  made.  If the conversion can be made, SUCCESS is returned
 **************************************/


/* Check to make sure that the old and new types are compatible */
switch (orig_fld->dyn_dtype)
    {
/* CHARACTER types */
    case blr_text:
    case blr_varying:
    case blr_cstring:
        switch (new_fld->dyn_dtype)
            {
            case blr_blob:
            case blr_blob_id:
        	return isc_dyn_dtype_invalid; 
                /* Cannot change datatype for column %s.
                   The operation cannot be performed on BLOB, or ARRAY columns. */
                break;

            case blr_sql_date:
            case blr_sql_time:
            case blr_timestamp:
            case blr_int64:
            case blr_long:
            case blr_short:
            case blr_d_float:
            case blr_double:
            case blr_float:
                return isc_dyn_dtype_conv_invalid;
                /* Cannot convert column %s from character to non-character data. */
                break;

            /* If the original field is a character field and the new field is a character field, 
             * is there enough space in the new field? */
            case blr_text:
            case blr_varying:
            case blr_cstring:
                {
                USHORT maxflen = 0;

		maxflen = DSC_string_length (&orig_fld->dyn_dsc);

		if (new_fld->dyn_dsc.dsc_length < maxflen)
                    return isc_dyn_char_fld_too_small;
	            /* New size specified for column %s must be greater than %d characters. */
		}
                break;

            default:
               assert (FALSE);
               return 87;    /* MODIFY RDB$FIELDS FAILED */
            }
        break;

/* BLOB and ARRAY types */
    case blr_blob:
    case blr_blob_id:
        return isc_dyn_dtype_invalid; 
        /* Cannot change datatype for column %s.
           The operation cannot be performed on BLOB, or ARRAY columns. */
        break;

/* DATE types */
    case blr_sql_date:
    case blr_sql_time:
    case blr_timestamp:
	switch (new_fld->dyn_dtype)
	    {
	    case blr_sql_date:
		if (orig_fld->dyn_dtype == blr_sql_time)
                    return isc_dyn_invalid_dtype_conversion;
                    /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		break;

	    case blr_sql_time:
		if (orig_fld->dyn_dtype == blr_sql_date)
                    return isc_dyn_invalid_dtype_conversion;
                    /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		break;

	    case blr_timestamp:
		if (orig_fld->dyn_dtype == blr_sql_time)
                    return isc_dyn_invalid_dtype_conversion;
                    /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		break;

	    /* If the original field is a date field and the new field is a character field, 
             * is there enough space in the new field? */
            case blr_text:
            case blr_text2:
            case blr_varying:
            case blr_varying2:
            case blr_cstring:
            case blr_cstring2:
                {
                USHORT maxflen = 0;

		maxflen = DSC_string_length (&orig_fld->dyn_dsc);

		if (new_fld->dyn_dsc.dsc_length < maxflen)
                    return isc_dyn_char_fld_too_small;
	            /* New size specified for column %s must be greater than %d characters. */
                }

		break;

	    default:
                return isc_dyn_invalid_dtype_conversion;
                /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
	    }
	break;

/* NUMERIC types */
    case blr_int64:
    case blr_long:
    case blr_short:
    case blr_d_float:
    case blr_double:
    case blr_float:
        switch (new_fld->dyn_dtype)
            {
            case blr_blob:
            case blr_blob_id:
                return isc_dyn_dtype_invalid; 
                /* Cannot change datatype for column %s.
                   The operation cannot be performed on BLOB, or ARRAY columns. */
                break;

            case blr_sql_date:
            case blr_sql_time:
            case blr_timestamp:
                return isc_dyn_invalid_dtype_conversion;
                /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */

            /* If the original field is a numeric field and the new field is a numeric field,
             * is there enough space in the new field (do not allow the base type to decrease) */

            case blr_short:
		switch (orig_fld->dyn_dtype)
		    {
		    case blr_short:
		        break;
		    default:
                	return isc_dyn_invalid_dtype_conversion;
	                /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		    }
                break;

            case blr_long:
		switch (orig_fld->dyn_dtype)
		    {
		    case blr_long:
		    case blr_short:
			break;
		    default:
                	return isc_dyn_invalid_dtype_conversion;
                	/* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		    }
		break;

            case blr_float:
                switch (orig_fld->dyn_dtype)
                    {
                    case blr_float:
		    case blr_short:
			break;

                    default:
	                return isc_dyn_invalid_dtype_conversion;
        	        /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
                    }
		break;
 
            case blr_int64:
		switch (orig_fld->dyn_dtype)
		    {
		    case blr_int64:
		    case blr_long:
	 	    case blr_short:
			break;

		    default:
	                return isc_dyn_invalid_dtype_conversion;
        	        /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		    }
		break;

            case blr_d_float:
            case blr_double:
		switch (orig_fld->dyn_dtype)
		    {
		    case blr_double:
		    case blr_d_float:
		    case blr_float:
	 	    case blr_short:
		    case blr_long:
			break;

		    default:
	                return isc_dyn_invalid_dtype_conversion;
        	        /* Cannot change datatype for column %s.  Conversion from base type %s to base type %s is not supported. */
		    }
                break;

            /* If the original field is a numeric field and the new field is a character field, 
             * is there enough space in the new field? */
            case blr_text:
            case blr_varying:
            case blr_cstring:
                {
                USHORT maxflen = 0;

		maxflen = DSC_string_length (&orig_fld->dyn_dsc);

		if (new_fld->dyn_dsc.dsc_length < maxflen)
                    return isc_dyn_char_fld_too_small;
	            /* New size specified for column %s must be greater than %d characters. */
                }
		break;

            default:
               assert (FALSE);
               return 87;    /* MODIFY RDB$FIELDS FAILED */
            }
        break;

    default:
       assert (FALSE);
       return 87;    /* MODIFY RDB$FIELDS FAILED */
    }
return SUCCESS;
}

static void modify_err_punt (TDBB tdbb,
				ULONG	errorcode,
				DYN_FLD orig_fld_def,
				DYN_FLD new_fld_def)
{
/**************************************
 *
 *	m o d i f y _ e r r _ p u n t
 *
 **************************************
 *
 * Functional description
 *	A generic error routine that calls DYN_error_punt
 *      based on the error code returned by check_update_field_type.
 *     
 *      This function is called by DYN_modify_global_field and by   
 *	DYN_modify_sql_field
 **************************************/


switch (errorcode)
    {
    case isc_dyn_dtype_invalid:
	DYN_error_punt (FALSE, 207, orig_fld_def->dyn_fld_name, NULL, NULL, NULL, NULL);
	/* Cannot change datatype for column %s.The operation cannot be performed on DATE, BLOB, or ARRAY columns. */
	break;

    case isc_dyn_dtype_conv_invalid:
	DYN_error_punt (FALSE, 210, orig_fld_def->dyn_fld_name, NULL, NULL, NULL, NULL);
	/* Cannot convert column %s from character to non-character data. */
	break;

    case isc_dyn_char_fld_too_small:
	DYN_error_punt (FALSE, 208, orig_fld_def->dyn_fld_name, DSC_string_length(&orig_fld_def->dyn_dsc), NULL, NULL, NULL);
	/* msg 208: New size specified for column %s must be greater than %d characters. */	    
	break;

    case isc_dyn_invalid_dtype_conversion:
	{
	TEXT    orig_type[25], new_type[25];

	DSC_get_dtype_name (&orig_fld_def->dyn_dsc, orig_type, sizeof (orig_type));
	DSC_get_dtype_name (&new_fld_def->dyn_dsc, new_type, sizeof (new_type));
	
	DYN_error_punt (FALSE, 209, orig_fld_def->dyn_fld_name, orig_type, new_type, NULL, NULL);
	/* Cannot convert base type %s to base type %s. */
	break;
	}

    default:
	DYN_error_punt (TRUE, 95, NULL, NULL, NULL, NULL, NULL);
	/* msg 95: "MODIFY RDB$RELATION_FIELDS failed" */ 
    }
}
