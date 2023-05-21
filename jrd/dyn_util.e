/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		dyn_util.e
 *	DESCRIPTION:	Dynamic data definition - utility functions
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

static CONST SCHAR
      gen_id_blr1 [] = {
        blr_version5,
        blr_begin, 
          blr_message, 0, 1,0, 
#ifndef EXACT_NUMERICS
    	    blr_long, 0, 
#else
    	    blr_int64, 0, 
#endif
          blr_begin, 
            blr_send, 0, 
              blr_begin, 
                blr_assignment, 
                  blr_gen_id
        },
      gen_id_blr2 [] = {
                     blr_literal, blr_long, 0, 1,0,0,0,
                  blr_parameter, 0, 0,0, 
              blr_end, 
          blr_end, 
        blr_end, 
        blr_eoc
        };

static CONST SCHAR
      prot_blr [] = {
        blr_version5,
        blr_begin, 
          blr_message, 1, 1,0, 
    	    blr_short, 0, 
          blr_message, 0, 2,0, 
            blr_cstring, 32,0, 
            blr_cstring, 32,0, 
          blr_receive, 0, 
            blr_begin, 
              blr_send, 1, 
                blr_begin, 
                  blr_assignment, 
                    blr_prot_mask,
                      blr_parameter, 0, 0,0, 
                      blr_parameter, 0, 1,0, 
                    blr_parameter, 1, 0,0, 
                blr_end, 
            blr_end, 
        blr_end, 
        blr_eoc
        };



SINT64 DYN_UTIL_gen_unique_id (
    TDBB	tdbb,
    GBL		gbl,
    SSHORT	id,
    SCHAR	*generator_name,
    BLK		*request)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n _ u n i q u e _ i d
 *
 **************************************
 *
 * Functional description
 *	Generate a unique id using a generator.
 *
 **************************************/
DBB	dbb;
BLK	handle;
#ifndef EXACT_NUMERICS
SLONG	value;
#else
SINT64	value;
#endif

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

if (!(handle = (BLK) CMP_find_request (tdbb, id, DYN_REQUESTS)))
    {
    /* 32 bytes allocated for size of of a metadata name */
    SCHAR	blr [sizeof(gen_id_blr1)+sizeof(gen_id_blr2)+1+32], *p;
    assert (strlen (generator_name) < 32);

    p = blr;
    memcpy (p, gen_id_blr1, sizeof (gen_id_blr1));
    p += sizeof (gen_id_blr1);
    *p++ = strlen (generator_name);
    strcpy (p, generator_name);
    p += p [-1];
    memcpy (p, gen_id_blr2, sizeof (gen_id_blr2));
    p += sizeof (gen_id_blr2);
    handle = (BLK) CMP_compile2 (tdbb, blr, TRUE);
    }
*request = handle;
EXE_start (tdbb, handle, dbb->dbb_sys_trans);
EXE_receive (tdbb, handle, 0, sizeof (value), &value);
EXE_unwind (tdbb, handle);
*request = NULL;

if (!DYN_REQUEST (id))
    DYN_REQUEST (id) = handle;

return value;
}

void DYN_UTIL_generate_constraint_name (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n e r a t e _ c o n s t r a i n t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$RELATION_CONSTRAINTS.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
USHORT		  found;
VOLATILE SSHORT	  id;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, id);
    DYN_error_punt (TRUE, 131, NULL, NULL, NULL, NULL, NULL);
    /* msg 131: "Generation of constraint name failed" */
    }

do {
    id = drq_g_nxt_con;
    sprintf (buffer, "INTEG_%" QUADFORMAT "d",
	(SINT64) DYN_UTIL_gen_unique_id (tdbb, gbl, drq_g_nxt_con,
			"RDB$CONSTRAINT_NAME", &request));

    request = (BLK) CMP_find_request (tdbb, drq_f_nxt_con, DYN_REQUESTS);
    id = drq_f_nxt_con;

    found = FALSE;
    FOR (REQUEST_HANDLE request)
	    FIRST 1 X IN RDB$RELATION_CONSTRAINTS
	    WITH X.RDB$CONSTRAINT_NAME EQ buffer
	if (!DYN_REQUEST (drq_f_nxt_con))
	    DYN_REQUEST (drq_f_nxt_con) = request;
	found = TRUE;
    END_FOR;

    if (!DYN_REQUEST (drq_f_nxt_con))
	DYN_REQUEST (drq_f_nxt_con) = request;
    request = NULL;
    } while (found);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_UTIL_generate_field_name (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n e r a t e _ f i e l d _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$FIELDS.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
USHORT		  found;
VOLATILE SSHORT	  id;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, id);
    DYN_error_punt (TRUE, 81, NULL, NULL, NULL, NULL, NULL);
    /* msg 81: "Generation of field name failed" */
    }

do {
    id = drq_g_nxt_fld;
    sprintf (buffer, "RDB$%" QUADFORMAT "d",
	(SINT64) DYN_UTIL_gen_unique_id (tdbb, gbl, drq_g_nxt_fld,
			"RDB$FIELD_NAME", &request));

    request = (BLK) CMP_find_request (tdbb, drq_f_nxt_fld, DYN_REQUESTS);
    id = drq_f_nxt_fld;

    found = FALSE;
    FOR (REQUEST_HANDLE request)
	    FIRST 1 X IN RDB$FIELDS WITH X.RDB$FIELD_NAME EQ buffer
	if (!DYN_REQUEST (drq_f_nxt_fld))
	    DYN_REQUEST (drq_f_nxt_fld) = request;
	found = TRUE;
    END_FOR;

    if (!DYN_REQUEST (drq_f_nxt_fld))
	DYN_REQUEST (drq_f_nxt_fld) = request;
    request = NULL;
    } while (found);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_UTIL_generate_field_position (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*relation_name,
    SLONG	*field_pos)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n e r a t e _ f i e l d _ p o s i t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a field position if not specified
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;
SLONG		  field_position = -1;

if (!relation_name)
    return;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = NULL;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, -1);
    DYN_error_punt (TRUE, 162, NULL, NULL, NULL, NULL, NULL);
    /* msg 162: "Looking up field position failed" */
    }

request = (BLK) CMP_find_request (tdbb, drq_l_fld_pos, DYN_REQUESTS);

FOR (REQUEST_HANDLE request)
	X IN RDB$RELATION_FIELDS
	WITH X.RDB$RELATION_NAME EQ relation_name
    if (!DYN_REQUEST (drq_l_fld_pos))
	DYN_REQUEST (drq_l_fld_pos) = request;

    if (X.RDB$FIELD_POSITION.NULL)
	continue;

    field_position = MAX (X.RDB$FIELD_POSITION, field_position);
END_FOR;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
*field_pos = field_position;
}

void DYN_UTIL_generate_index_name (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*buffer,
    UCHAR	verb)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n e r a t e _ i n d e x _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$INDICES.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
USHORT		  found;
VOLATILE SSHORT	  id;
SCHAR		 *format;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, id);
    DYN_error_punt (TRUE, 82, NULL, NULL, NULL, NULL, NULL);
    /* msg 82: "Generation of index name failed" */
    }

do {
    if (verb == gds__dyn_def_primary_key)
	format = "RDB$PRIMARY%" QUADFORMAT "d";
    else if (verb == gds__dyn_def_foreign_key)
	format = "RDB$FOREIGN%" QUADFORMAT "d";
    else
	format = "RDB$%" QUADFORMAT "d";
    id = drq_g_nxt_idx;
    sprintf (buffer, format,
	(SINT64) DYN_UTIL_gen_unique_id (tdbb, gbl, drq_g_nxt_idx,
			"RDB$INDEX_NAME", &request));

    request = (BLK) CMP_find_request (tdbb, drq_f_nxt_idx, DYN_REQUESTS);
    id = drq_f_nxt_idx;

    found = FALSE;
    FOR (REQUEST_HANDLE request)
	    FIRST 1 X IN RDB$INDICES WITH X.RDB$INDEX_NAME EQ buffer
	if (!DYN_REQUEST (drq_f_nxt_idx))
	    DYN_REQUEST (drq_f_nxt_idx) = request;
	found = TRUE;
    END_FOR;

    if (!DYN_REQUEST (drq_f_nxt_idx))
	DYN_REQUEST (drq_f_nxt_idx) = request;
    request = NULL;
    } while (found);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void DYN_UTIL_generate_trigger_name (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*buffer)
{
/**************************************
 *
 *	D Y N _ U T I L _g e n e r a t e _ t r i g g e r _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name unique to RDB$TRIGGERS.
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
USHORT		  found;
VOLATILE SSHORT	  id;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = NULL;
id = -1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, id);
    DYN_error_punt (TRUE, 83, NULL, NULL, NULL, NULL, NULL);
    /* msg 83: "Generation of trigger name failed" */
    }

do {
    id = drq_g_nxt_trg;
    sprintf (buffer, "CHECK_%" QUADFORMAT "d",
	(SINT64) DYN_UTIL_gen_unique_id (tdbb, gbl, drq_g_nxt_trg,
			"RDB$TRIGGER_NAME", &request));

    request = (BLK) CMP_find_request (tdbb, drq_f_nxt_trg, DYN_REQUESTS);
    id = drq_f_nxt_trg;

    found = FALSE;
    FOR (REQUEST_HANDLE request)
	    FIRST 1 X IN RDB$TRIGGERS WITH X.RDB$TRIGGER_NAME EQ buffer
	if (!DYN_REQUEST (drq_f_nxt_trg))
	    DYN_REQUEST (drq_f_nxt_trg) = request;
	found = TRUE;
    END_FOR;

    if (!DYN_REQUEST (drq_f_nxt_trg))
	DYN_REQUEST (drq_f_nxt_trg) = request;
    request = NULL;
    } while (found);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

BOOLEAN DYN_UTIL_get_prot (
    TDBB	tdbb,
    GBL		gbl,
    SCHAR	*rname,
    SCHAR	*fname,
    USHORT	*prot_mask)
{
/**************************************
 *
 *	D Y N _ U T I L _g e t _ p r o t
 *
 **************************************
 *
 * Functional description
 *	Get protection mask for relation or relation_field
 *
 **************************************/
VOLATILE BLK	  request;
struct {
    SCHAR	  relation_name [32];
    SCHAR	  field_name [32];
}		  in_msg;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);

request = (BLK) CMP_find_request (tdbb, drq_l_prot_mask, DYN_REQUESTS);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    DYN_rundown_request (old_env, request, drq_l_prot_mask);
    return FAILURE;
    }

if (!request)
    request = (BLK) CMP_compile2 (tdbb, prot_blr, TRUE);
gds__vtov (rname, in_msg.relation_name, sizeof (in_msg.relation_name));
gds__vtov (fname, in_msg.field_name, sizeof (in_msg.field_name));
EXE_start (tdbb, request, gbl->gbl_transaction);
EXE_send (tdbb, request, 0, sizeof (in_msg), &in_msg);
EXE_receive (tdbb, request, 1, sizeof (USHORT), prot_mask);

DYN_rundown_request (old_env, request, drq_l_prot_mask);

return SUCCESS;
}

void DYN_UTIL_store_check_constraints (
    TDBB	tdbb,
    GBL		gbl,
    TEXT	*constraint_name,
    TEXT	*trigger_name)
{
/**************************************
 *
 *	D Y N _ U T I L _s t o r e _ c h e c k _ c o n s t r a i n t s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
DBB		  dbb;
VOLATILE BLK	  request;
JMP_BUF		  env;
JMP_BUF* VOLATILE old_env;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, drq_s_chk_con, DYN_REQUESTS);

STORE (REQUEST_HANDLE request TRANSACTION_HANDLE gbl->gbl_transaction)
	CHK IN RDB$CHECK_CONSTRAINTS  

    strcpy (CHK.RDB$CONSTRAINT_NAME, constraint_name);
    strcpy (CHK.RDB$TRIGGER_NAME, trigger_name);

    old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
    tdbb->tdbb_setjmp = (UCHAR*) env;
    if (SETJMP (env))
	{
	DYN_rundown_request (old_env, request, drq_s_chk_con);
	DYN_error_punt (TRUE, 122, NULL, NULL, NULL, NULL, NULL);
	/* msg 122: "STORE RDB$CHECK_CONSTRAINTS failed" */
	}
END_STORE;

if (!DYN_REQUEST (drq_s_chk_con))
    DYN_REQUEST (drq_s_chk_con) = request;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}
