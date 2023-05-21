/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ini.e
 *	DESCRIPTION:	Metadata initialization / population
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
#include "../jrd/flags.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/gds.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/ids.h"
#include "../jrd/tra.h"
#include "../jrd/trig.h"
#include "../jrd/intl.h"
#include "../jrd/dflt.h"
#include "../jrd/constants.h"
#include "../jrd/ini.h" 
#include "../jrd/idx.h"
#include "../jrd/gdsassert.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/ini_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/obj.h"
#include "../jrd/acl.h"
#include "../jrd/irq.h"

DATABASE
    DB = FILENAME "ODS.RDB";

#define PAD(string, field) jrd_vtof (string, field, sizeof (field))
#define MAX_ACL_SIZE    4096
#define DEFAULT_CLASS   "SQL$DEFAULT"

static void	add_generator (TEXT *, BLK *);
static void	add_global_fields (USHORT);
static void	add_index_set (DBB, USHORT, USHORT, USHORT);
static void	add_new_triggers (USHORT, USHORT);
static void	add_relation_fields (USHORT);
static void	add_security_to_sys_rel (TDBB,   TEXT *, TEXT *,
                                                TEXT *, SSHORT);
static void	add_trigger (TEXT *, BLK *, BLK *);
static void	add_user_priv           (TDBB,   TEXT *, TEXT *,
                                                TEXT *, TEXT);
static void	modify_relation_field	(TDBB, UCHAR *,
						UCHAR *, BLK *);
static void	store_generator		(TDBB, GEN *, BLK *);
static void	store_global_field	(TDBB, GFLD *, BLK *);
static void	store_intlnames		(TDBB, DBB);
static void	store_message		(TDBB, TRIGMSG *, BLK *);
static void	store_relation_field	(TDBB, UCHAR *, UCHAR *,
						int, BLK *, int);
static void	store_trigger		(TDBB, TRG *, BLK *);

/* This is the table used in defining triggers; note that
   RDB$TRIGGER_0 was first changed to RDB$TRIGGER_7 to make it easier to
   upgrade a database to support field-level grant.  It has since been
   changed to RDB$TRIGGER_9 to handle SQL security on relations whose
   name is > 27 characters */

static CONST TRG FAR_VARIABLE	triggers [] = {
    "RDB$TRIGGER_1", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger3), trigger3, 0, ODS_8_0,
    "RDB$TRIGGER_8", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger2), trigger2, 0, ODS_8_0,
    "RDB$TRIGGER_9", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger1), trigger1, 0, ODS_8_0,
    "RDB$TRIGGER_2", (UCHAR) nam_trgs, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger4), trigger4, 0, ODS_8_0,
    "RDB$TRIGGER_3", (UCHAR) nam_trgs, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger4), trigger4, 0, ODS_8_0,
    "RDB$TRIGGER_4", (UCHAR) nam_relations, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger5), trigger5, 0, ODS_8_0,
    "RDB$TRIGGER_5", (UCHAR) nam_relations, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger6), trigger6, 0, ODS_8_0,
    "RDB$TRIGGER_6", (UCHAR) nam_gens, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger7), trigger7, 0, ODS_8_0,
    "RDB$TRIGGER_26", (UCHAR) nam_rel_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger26), trigger26, 0, ODS_8_0,
    "RDB$TRIGGER_25", (UCHAR) nam_rel_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger25), trigger25, 0, ODS_8_0,
    "RDB$TRIGGER_10", (UCHAR) nam_rel_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger10), trigger10, 0, ODS_8_0,
    "RDB$TRIGGER_11", (UCHAR) nam_rel_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.POST_ERASE, sizeof (trigger11), trigger11, 0, ODS_8_0,
    "RDB$TRIGGER_12", (UCHAR) nam_ref_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger12), trigger12, 0, ODS_8_0,
    "RDB$TRIGGER_13", (UCHAR) nam_ref_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger13), trigger13, 0, ODS_8_0,
    "RDB$TRIGGER_14", (UCHAR) nam_chk_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger14), trigger14, 0, ODS_8_0,
    "RDB$TRIGGER_15", (UCHAR) nam_chk_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger15), trigger15, 0, ODS_8_0,
    "RDB$TRIGGER_16", (UCHAR) nam_chk_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.POST_ERASE, sizeof (trigger16), trigger16, 0, ODS_8_0,
    "RDB$TRIGGER_17", (UCHAR) nam_i_segments, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger17), trigger17, 0, ODS_8_0,
    "RDB$TRIGGER_18", (UCHAR) nam_i_segments, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger18), trigger18, 0, ODS_8_0,
    "RDB$TRIGGER_19", (UCHAR) nam_indices,    RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger19), trigger19, 0, ODS_8_0,
    "RDB$TRIGGER_20", (UCHAR) nam_indices,    RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger20), trigger20, 0, ODS_8_0,
    "RDB$TRIGGER_21", (UCHAR) nam_trgs,       RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger21), trigger21, 0, ODS_8_0,
    "RDB$TRIGGER_22", (UCHAR) nam_trgs,       RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger22), trigger22, 0, ODS_8_0,
    "RDB$TRIGGER_23", (UCHAR) nam_r_fields,   RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger23), trigger23, 0, ODS_8_0,
    "RDB$TRIGGER_24", (UCHAR) nam_r_fields,   RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger24), trigger24, 0, ODS_8_0,
    "RDB$TRIGGER_27", (UCHAR) nam_r_fields,   RDB$TRIGGERS.RDB$TRIGGER_TYPE.POST_ERASE, sizeof (trigger27), trigger27, 0, ODS_8_0,
    "RDB$TRIGGER_28", (UCHAR) nam_procedures, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger28), trigger28, 0, ODS_8_0,
    "RDB$TRIGGER_29", (UCHAR) nam_procedures, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger29), trigger29, 0, ODS_8_0,
    "RDB$TRIGGER_30", (UCHAR) nam_exceptions, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger30), trigger30, 0, ODS_8_0,
    "RDB$TRIGGER_31", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_MODIFY, sizeof (trigger31), trigger31, 0, ODS_8_1,
    "RDB$TRIGGER_32", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_ERASE, sizeof (trigger31), trigger31, 0, ODS_8_1,
    "RDB$TRIGGER_33", (UCHAR) nam_user_privileges, RDB$TRIGGERS.RDB$TRIGGER_TYPE.PRE_STORE, sizeof (trigger31), trigger31, 0, ODS_8_1,
    "RDB$TRIGGER_34", (UCHAR) nam_rel_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.POST_ERASE, sizeof (trigger34), trigger34, TRG_ignore_perm, ODS_8_1,
    "RDB$TRIGGER_35", (UCHAR) nam_chk_constr, RDB$TRIGGERS.RDB$TRIGGER_TYPE.POST_ERASE, sizeof (trigger35), trigger35, TRG_ignore_perm, ODS_8_1,
    0, 0, 0, 0, 0, 0
    };


/* this table is used in defining messages for system triggers */

static CONST TRIGMSG FAR_VARIABLE	trigger_messages [] = {
    "RDB$TRIGGER_9",	0,  "grant_obj_notfound",	ODS_8_0,
    "RDB$TRIGGER_9",	1,  "grant_fld_notfound",	ODS_8_0,
    "RDB$TRIGGER_9",	2,  "grant_nopriv",		ODS_8_0,
    "RDB$TRIGGER_9",	3,  "nonsql_security_rel",	ODS_8_0,
    "RDB$TRIGGER_9",	4,  "nonsql_security_fld",	ODS_8_0,
    "RDB$TRIGGER_9",	5,  "grant_nopriv_on_base",	ODS_8_0,
    "RDB$TRIGGER_1",	0,  "existing_priv_mod",	ODS_8_0,
    "RDB$TRIGGER_2",	0,  "systrig_update",		ODS_8_0,
    "RDB$TRIGGER_3",	0,  "systrig_update",		ODS_8_0,
    "RDB$TRIGGER_5",	0,  "not_rel_owner",		ODS_8_0,
    "RDB$TRIGGER_24",	1,  "cnstrnt_fld_rename",	ODS_8_0,
    "RDB$TRIGGER_23",	1,  "cnstrnt_fld_del",		ODS_8_0,
    "RDB$TRIGGER_22",	1,  "check_trig_update",	ODS_8_0,
    "RDB$TRIGGER_21",	1,  "check_trig_del",		ODS_8_0,
    "RDB$TRIGGER_20",	1,  "integ_index_mod",		ODS_8_0,
    "RDB$TRIGGER_20",	2,  "integ_index_deactivate",	ODS_8_0,
    "RDB$TRIGGER_20",	3,  "integ_deactivate_primary",	ODS_8_0,
    "RDB$TRIGGER_19",	1,  "integ_index_del",		ODS_8_0,
    "RDB$TRIGGER_18",	1,  "integ_index_seg_mod",	ODS_8_0,
    "RDB$TRIGGER_17",	1,  "integ_index_seg_del",	ODS_8_0,
    "RDB$TRIGGER_15",	1,  "check_cnstrnt_del",	ODS_8_0,
    "RDB$TRIGGER_14",	1,  "check_cnstrnt_update",	ODS_8_0,
    "RDB$TRIGGER_13",	1,  "ref_cnstrnt_update",	ODS_8_0,
    "RDB$TRIGGER_12",	1,  "ref_cnstrnt_notfound",	ODS_8_0,
    "RDB$TRIGGER_12",	2,  "foreign_key_notfound",	ODS_8_0,
    "RDB$TRIGGER_10",	1,  "primary_key_ref",		ODS_8_0,
    "RDB$TRIGGER_10",	2,  "primary_key_notnull",	ODS_8_0,
    "RDB$TRIGGER_25",	1,  "rel_cnstrnt_update",	ODS_8_0,
    "RDB$TRIGGER_26",	1,  "constaint_on_view",	ODS_8_0,
    "RDB$TRIGGER_26",	2,  "invld_cnstrnt_type",	ODS_8_0,
    "RDB$TRIGGER_26",	3,  "primary_key_exists",	ODS_8_0,
    "RDB$TRIGGER_31",   0,  "no_write_user_priv", 	ODS_8_1,
    "RDB$TRIGGER_32",   0,  "no_write_user_priv", 	ODS_8_1,
    "RDB$TRIGGER_33",   0,  "no_write_user_priv", 	ODS_8_1,
    0, 0, 0, 0
    };


void INI_format (
    TEXT	*owner)
{
/**************************************
 *
 *	I N I _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *	Initialize system relations in the database.
 *	The full complement of metadata should be
 *	stored here.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
int	n;
RTYP	*type;
UCHAR	*relfld, *fld;
GFLD	*gfield;
TRG	*trigger;
TRIGMSG	*message;
GEN 	*generator;
TEXT	string [32], *p;
BLK	handle1, handle2;

UCHAR	        *acl, buffer [MAX_ACL_SIZE];
TEXT	        *p_1;
USHORT	        length;

tdbb = GET_THREAD_DATA;
dbb  = tdbb->tdbb_database;

/* Uppercase owner name */

*string = 0;
if (owner && *owner)
    for (p = string; *p++ = UPPER7 (*owner); owner++)
    	;

/* Make sure relations exist already */

for (n = 0; n < (int) rel_MAX; n++)
    DPM_create_relation (tdbb, MET_relation (tdbb, n));

/* Store RELATIONS and RELATION_FIELDS */

handle1 = handle2 = NULL;
for (relfld = relfields; relfld [RFLD_R_NAME]; relfld = fld + 1)
    {
    for (n = 0, fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH)
	if (!fld [RFLD_F_MINOR])
	    {
	    store_relation_field (tdbb, fld, relfld, n, &handle2, TRUE);
	    n++;
	    }

    STORE (REQUEST_HANDLE handle1) X IN RDB$RELATIONS
	X.RDB$RELATION_ID = relfld [RFLD_R_ID];
	PAD (names [relfld [RFLD_R_NAME]], X.RDB$RELATION_NAME);
	X.RDB$FIELD_ID = n;
	X.RDB$FORMAT = 0;
	X.RDB$SYSTEM_FLAG = RDB_system;
	X.RDB$DBKEY_LENGTH = 8;
	X.RDB$OWNER_NAME.NULL = TRUE;
	if (*string)
	    {
	    PAD (string, X.RDB$OWNER_NAME);
	    X.RDB$OWNER_NAME.NULL = FALSE;
	    }
    END_STORE;
    }

CMP_release (tdbb, handle1);
CMP_release (tdbb, handle2);
handle1 = handle2 = NULL;

/* Store global FIELDS */

for (gfield = (GFLD*) gfields; gfield->gfld_name; gfield++)
    store_global_field (tdbb, gfield, &handle1);

CMP_release (tdbb, handle1);
handle1 = NULL;

STORE (REQUEST_HANDLE handle1) X IN RDB$DATABASE
    X.RDB$RELATION_ID = (int) USER_DEF_REL_INIT_ID;
END_STORE

CMP_release (tdbb, handle1);
handle1 = NULL;

/* Create indices for system relations */
add_index_set (dbb, FALSE, 0, 0);

/* Create parameter types */

handle1 = NULL;

for (type = types; type->rtyp_name; type++)
    STORE (REQUEST_HANDLE handle1) X IN RDB$TYPES
	PAD (names [type->rtyp_field], X.RDB$FIELD_NAME);
	PAD (type->rtyp_name, X.RDB$TYPE_NAME);
	X.RDB$TYPE = type->rtyp_value;
	X.RDB$SYSTEM_FLAG = RDB_system;
    END_STORE;

CMP_release (tdbb, handle1);

/* Store symbols for international character sets & collations */

store_intlnames (tdbb, dbb);

/* Create generators to be used by system triggers */

handle1 = NULL;
for (generator = generators; generator->gen_name; generator++)
    store_generator (tdbb, generator, &handle1);
CMP_release (tdbb, handle1);

/* store system-defined triggers */

handle1 = NULL;
for (trigger = triggers; trigger->trg_relation; ++trigger)
    store_trigger (tdbb, trigger, &handle1);
CMP_release (tdbb, handle1);

/* store trigger messages to go with triggers */

handle1 = NULL;
for (message = trigger_messages; message->trigmsg_name; ++message)
    store_message (tdbb, message, &handle1);
CMP_release (tdbb, handle1);

DFW_perform_system_work();

add_relation_fields (0);

/*
====================================================================
==
== Add security on RDB$ROLES system table
==
======================================================================
*/

acl = buffer;
*acl++ = ACL_version;
*acl++ = ACL_id_list;
*acl++ = id_person;
p_1    = string;
if (*acl++ = length = strlen (string))
   do *acl++ = *p_1++; while (--length);
*acl++ = ACL_end;
*acl++ = ACL_priv_list;
*acl++ = priv_protect;
*acl++ = priv_control;
*acl++ = priv_delete;
*acl++ = priv_write;
*acl++ = priv_read;
*acl++ = ACL_end;
*acl++ = ACL_id_list;
*acl++ = ACL_end;
*acl++ = ACL_priv_list;
*acl++ = priv_read;
*acl++ = ACL_end;
*acl++ = ACL_end;
length = acl - buffer;

add_security_to_sys_rel (tdbb, string, "RDB$ROLES", buffer, length);

}

USHORT INI_get_trig_flags (
    TEXT        *trig_name)
{
/*********************************************
 *
 *      I N I _ g e t _ t r i g _ f l a g s
 *
 *********************************************
 *
 * Functional description
 *      Return the trigger flags for a system trigger.
 *
 **************************************/
TRG *trig;

for (trig = triggers; trig->trg_length > 0; trig++)
    if (!strcmp(trig->trg_name, trig_name))
        return (trig->trg_flags);

return(0);
}

void INI_init (void)
{
/**************************************
 *
 *	I N I _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize in memory meta data.  Assume that all meta data
 *	fields exist in the database even if this is not the case.
 *	Do not fill in the format length or the address in each
 *	format descriptor.
 *
 **************************************/
DBB	dbb;
REL	relation;
FMT	format;
FLD	*field;
VEC	formats, fields;
int	n;
UCHAR 	*relfld, *fld;
DSC	*desc;
GFLD	*gfield;
TRG	*trigger;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

for (relfld = relfields; relfld [RFLD_R_NAME]; relfld = fld + 1)
    {
    relation = MET_relation (tdbb, relfld [RFLD_R_ID]);
    relation->rel_flags |= REL_system;
    relation->rel_name = names [relfld [RFLD_R_NAME]];
    relation->rel_length = strlen (relation->rel_name);
    for (n = 0, fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH)
	n++;

    /* Set a flag if their is a trigger on the relation.  Later we may
       need to compile it. */

    for (trigger = triggers; trigger->trg_relation; trigger++)
	if (relation->rel_name == names [trigger->trg_relation])
	    {
	    relation->rel_flags |= REL_sys_triggers;
	    break;
	    }

    relation->rel_fields = fields = (VEC) ALLOCPV (type_vec, n);
    fields->vec_count = n;
    field = (FLD*) fields->vec_object;
    relation->rel_current_format = format = (FMT) ALLOCPV (type_fmt, n);
    relation->rel_formats = formats = (VEC) ALLOCPV (type_vec, 1);
    formats->vec_count = 1;
    formats->vec_object [0] = (BLK) format;
    format->fmt_count = n;
    desc = format->fmt_desc;

    for (fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH, desc++, field++)
	{
	gfield = &gfields [fld [RFLD_F_ID]];
	desc->dsc_length = gfield->gfld_length;
	desc->dsc_dtype = gfield->gfld_dtype;
	*field = (FLD) ALLOCPV (type_fld, 0);
	(*field)->fld_name = names [fld [RFLD_F_NAME]];
	(*field)->fld_length = strlen ((*field)->fld_name);
	}
    }
}

void INI_init2 (void)
{
/**************************************
 *
 *	I N I _ i n i t 2
 *
 **************************************
 *
 * Functional description
 *	Re-initialize in memory meta data.  Fill in
 *	format 0 based on the minor ODS version of
 *	the database when it was created.
 *
 **************************************/
REL	relation;
FMT	format;
int	n;
UCHAR 	*relfld, *fld;
DSC	*desc;
TDBB	tdbb;
USHORT  major_version, minor_original, id;
register VEC    vector;
DBB     dbb;

tdbb = GET_THREAD_DATA;
dbb  = tdbb->tdbb_database;
 
major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;
vector         = dbb->dbb_relations;


for (relfld = relfields; relfld [RFLD_R_NAME]; relfld = fld + 1)
    {
    if (relfld [RFLD_R_MINOR] > ENCODE_ODS(major_version, minor_original))
        {
        /*****************************************************
        **
        ** free the space allocated for RDB$ROLES
        **
        ******************************************************/
        id = relfld [RFLD_R_ID];
        relation = vector->vec_object [id];
        ALL_release (relation->rel_current_format);
        ALL_release (relation->rel_formats);
        ALL_release (relation->rel_fields);
        vector->vec_object [id]= NULL;
        for (fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH);
        }
    else
        {
    relation = MET_relation (tdbb, relfld [RFLD_R_ID]);
    format = relation->rel_current_format;

    for (n = 0, fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH)
	{
	/* If the ODS is less than 10, then remove the field
	 * RDB$FIELD_PRECISION, as it is not present in < 10 ODS
	 */
	if (fld [RFLD_F_NAME] == nam_f_precision)
	    {
	    if (major_version >= ODS_VERSION10)
		if (!fld [RFLD_F_MINOR])
		    {
		    n++;
		    if (fld [RFLD_F_UPD_MINOR])
			relation->rel_flags |= REL_force_scan;
		    }
		else
		    relation->rel_flags |= REL_force_scan;
	    }
	else
	    {
	    if (!fld [RFLD_F_MINOR])
		{
		n++;
		if (fld [RFLD_F_UPD_MINOR])
		    relation->rel_flags |= REL_force_scan;
		}
	    else
		relation->rel_flags |= REL_force_scan;
	    }
	}

    relation->rel_fields->vec_count = n;
    format->fmt_count = n;
    format->fmt_length = FLAG_BYTES (n);
    desc = format->fmt_desc;
    for (fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; fld += RFLD_F_LENGTH, desc++)
	if (n-- > 0)
	    {
	    format->fmt_length = MET_align (desc, format->fmt_length);
	    desc->dsc_address = (UCHAR*) (SLONG) format->fmt_length;
	    format->fmt_length += desc->dsc_length;
	    }
        }
    }
}

TRG *INI_lookup_sys_trigger (
    REL		relation,
    TRG		*trigger,
    UCHAR	**blr,
    UCHAR	*trigger_type,
    SCHAR	**trigger_name,
    USHORT      *trig_flags)
{
/**************************************
 *
 *	I N I _ l o o k u p _ s y s _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Lookup the next trigger for a system relation.
 *
 **************************************/

trigger = (trigger) ? trigger + 1 : triggers;

for (; trigger->trg_relation; trigger++)
    if (!strcmp (relation->rel_name, names [trigger->trg_relation]))
	{
	*blr = trigger->trg_blr;
	*trigger_type = trigger->trg_type;
	*trigger_name = trigger->trg_name;
        *trig_flags = trigger->trg_flags;
	return trigger;
	}

return NULL;
}

void INI_update_database (void)
{
/**************************************
 *
 *	I N I _ u p d a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Perform changes to ODS that were required 
 *	since ODS 8 and are dynamically updatable.
 *
 * %% Note %% Update the switch() statement to reflect new major ODS 
 *            addition
 **************************************/
DBB	dbb;
HDR	header;
WIN	window;
USHORT	major_version, minor_version;
TDBB	tdbb;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

#ifdef READONLY_DATABASE
/* If database is ReadOnly, return without upgrading ODS */
if (dbb->dbb_flags & DBB_read_only)
    return;
#endif

/* check out the update version to see if we have work to do */

major_version = (SSHORT) dbb->dbb_ods_version;
minor_version = (SSHORT) dbb->dbb_minor_version;

/*******************************************************************
** when old engine is attaching a newer ODS database, do nothing
********************************************************************/

/* if database ODS is less than the server's, then upgrade */
if (ENCODE_ODS (major_version, minor_version) >= ODS_CURRENT_VERSION)
    return;

/*******************************************************************
** when new engine is attaching an older ODS database 
**   perform the necessary modifications
********************************************************************/

if (major_version == ODS_VERSION8)
    {
    /*** NOTE: The following two functions/structures need to understand
               the difference between major ODS versions. The structure
               which holds their information needs to have an additional field
               to define which major ODS version they belong to.
               PENDING WORK.
                 add_global_fields (major_version, minor_version);
                 add_relation_fields (major_version, minor_version);
               Look at add_new_triggers() for reference.
     ***/

    add_global_fields (minor_version);
    add_relation_fields (minor_version);
    }

add_index_set (dbb, TRUE, major_version, minor_version);
add_new_triggers (major_version, minor_version);

    /* if the database was updated; mark it with the current ODS minor
       version so other process will attempt to do so. We mark this after
       doing the update so that no other process can see the new minor
       version but not the update. */

window.win_page = HEADER_PAGE;
window.win_flags = 0;
header= (HDR) CCH_FETCH (tdbb, &window, LCK_write, pag_header);
CCH_MARK (tdbb, &window);

/* Only minor upgrades can occur within a major ODS, define which one   
   occured here. */
switch (major_version) 
    {
    case ODS_VERSION8: 
        header->hdr_ods_minor = ODS_CURRENT8; 
        break;
    case ODS_VERSION9: 
        header->hdr_ods_minor = ODS_CURRENT9; 
        break;
    case ODS_VERSION10: 
        header->hdr_ods_minor = ODS_CURRENT10; 
        break;
    default: 
        /* Make sure we add a new case per new major ODS. Look at code above */
        assert (FALSE);
        header->hdr_ods_minor = minor_version; 
        break;
    }

dbb->dbb_minor_version = header->hdr_ods_minor;
CCH_RELEASE (tdbb, &window);

DFW_perform_system_work();
}

static void add_generator (
    TEXT	*generator_name,
    BLK		*handle)
{
/**************************************
 *
 *	a d d _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Store a generator of the given name.
 *	This routine is used to upgrade ODS versions.
 *	DO NOT DELETE, even though it is not used
 *	now, since it may be used when we go to 8.1.
 *
 **************************************/
TDBB	tdbb;
GEN	*generator;

tdbb = GET_THREAD_DATA;

/* find the new generator to be stored; assume it exists in the table */

for (generator = generators; strcmp (generator->gen_name, generator_name); generator++)
    ;
 
store_generator (tdbb, generator, handle);
}

static void add_global_fields (
    USHORT	minor_version)
{
/**************************************
 *
 *	a d d _ g l o b a l _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Add any global fields which have a non-zero
 *	update number in the fields table.  That
 *	is, those that have been added since the last
 *	ODS change.
 *
 **************************************/
TDBB	tdbb;
BLK	handle;
GFLD	*gfield;

tdbb = GET_THREAD_DATA;

/* add desired global fields to system relations  */

handle = NULL;
for (gfield = (GFLD*) gfields; gfield->gfld_name; gfield++)
    if (minor_version < gfield->gfld_minor)
	store_global_field (tdbb, gfield, &handle);

if (handle)
    CMP_release (tdbb, handle);

DFW_perform_system_work();
}

static void add_index_set (
    DBB		dbb,
    USHORT	update_ods,
    USHORT	major_version,
    USHORT	minor_version
)
{
/**************************************
 *
 *	a d d _ i n d e x _ s e t
 *
 **************************************
 *
 * Functional description
 *	Add system indices.  If update_ods is TRUE we are performing
 *	an ODS update, and only add the indices marked as newer than
 *	ODS (major_version,minor_version).
 *
 **************************************/
TDBB	tdbb;
REL	relation;
TEXT	string [32];
struct ini_idx_t		*index;
struct ini_idx_segment_t 	*segment;
IDX	idx;
struct idx_repeat	*tail;
USHORT	position;
FLD	field;
float	selectivity;
BLK	handle1, handle2;
USHORT	n;

tdbb = GET_THREAD_DATA;
handle1 = handle2 = NULL;

for (n = 0; n < SYSTEM_INDEX_COUNT; n++)
    {

    index = &indices [n];

    /* For minor ODS updates, only add indices newer than on-disk ODS */
    if (update_ods && 
	  ((index->ini_idx_version_flag 
		<= ENCODE_ODS (major_version, minor_version)) ||
	   (index->ini_idx_version_flag > ODS_CURRENT_VERSION) ||
	   (((USHORT) DECODE_ODS_MAJOR(index->ini_idx_version_flag)) != major_version)))
	/* The DECODE_ODS_MAJOR() is used (in this case) to instruct the server 
	   to perform updates for minor ODS versions only within a major ODS */
	continue;

    STORE (REQUEST_HANDLE handle1) X IN RDB$INDICES
	relation = MET_relation (tdbb, index->ini_idx_relid);
	PAD (relation->rel_name, X.RDB$RELATION_NAME);
	sprintf (string, "RDB$INDEX_%d", index->ini_idx_index_id);
	PAD (string, X.RDB$INDEX_NAME);
	X.RDB$UNIQUE_FLAG = index->ini_idx_unique_flag;
	X.RDB$SEGMENT_COUNT = index->ini_idx_segment_count;
	X.RDB$SYSTEM_FLAG = 1;
	X.RDB$INDEX_INACTIVE = 0;

	 /* Store each segment for the index */

	for (position = 0, tail = idx.idx_rpt;
	     position < index->ini_idx_segment_count; 
	     position++, tail++)
	    {
	    segment = &index->ini_idx_segment [position];
	    STORE (REQUEST_HANDLE handle2) Y IN RDB$INDEX_SEGMENTS
		field = (FLD) relation->rel_fields->vec_object [segment->ini_idx_rfld_id];
		Y.RDB$FIELD_POSITION = position;
		PAD (X.RDB$INDEX_NAME, Y.RDB$INDEX_NAME);
		PAD (field->fld_name, Y.RDB$FIELD_NAME);
		tail->idx_field = segment->ini_idx_rfld_id;
		tail->idx_itype = segment->ini_idx_type;
	    END_STORE;
	    }
	idx.idx_count = index->ini_idx_segment_count;
	idx.idx_flags = index->ini_idx_unique_flag;
	IDX_create_index (tdbb, relation, &idx, string, NULL_PTR, NULL_PTR, &selectivity);
	X.RDB$INDEX_ID = idx.idx_id + 1;
    END_STORE;

    }
if (handle1)
    CMP_release (tdbb, handle1);
if (handle2)
    CMP_release (tdbb, handle2);
}

static void add_new_triggers (
    USHORT major_version,
    USHORT minor_version)
{
/**************************************
 *
 *	a d d _ n e w _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Store all new ODS 8.x (x > 0) triggers. 
 *      The major and minor_version passed in are the ODS versions
 *      before the ODS is upgraded. 
 *	This routine is used to upgrade ODS versions.
 *
 **************************************/
TDBB	tdbb;
TRG	*trig;
TRIGMSG	*message;
BLK     handle1, handle2;

tdbb = GET_THREAD_DATA;

handle1 = handle2 = NULL;

/* add all new triggers, that were added since the database was created */
for (trig = triggers; trig->trg_length > 0; trig++)
    if ((trig->trg_ods_version > ENCODE_ODS(major_version, minor_version)) &&
        (DECODE_ODS_MAJOR(trig->trg_ods_version) == major_version))
        store_trigger (tdbb, trig, &handle1);

for (message = trigger_messages; message->trigmsg_name; message++)
    if ((message->trg_ods_version > ENCODE_ODS (major_version, minor_version)) &&
        (DECODE_ODS_MAJOR(message->trg_ods_version) == major_version))
         store_message (tdbb, message, &handle2);

if (handle1)
    CMP_release (tdbb, handle1);
if (handle2)
    CMP_release (tdbb, handle2);
}

static void add_relation_fields (
    USHORT	minor_version)
{
/**************************************
 *
 *	a d d _ r e l a t i o n _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Add any local fields which have a non-zero
 *	update number in the relfields table.  That
 *	is, those that have been added since the last
 *	ODS change.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
DSC	desc;
BLK	s_handle, m_handle;
UCHAR	*fld, *relfld;
int	n;

tdbb = GET_THREAD_DATA;
dbb  = tdbb->tdbb_database;

/* add desired fields to system relations, forcing a new format version */

s_handle = m_handle = NULL;
for (relfld = relfields; relfld [RFLD_R_NAME]; relfld = fld + 1)
    for (n = 0, fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; n++, fld += RFLD_F_LENGTH)
	if (minor_version < fld [RFLD_F_MINOR] ||
	    minor_version < fld [RFLD_F_UPD_MINOR])
	    {
	    if (minor_version < fld [RFLD_F_MINOR])
		store_relation_field (tdbb, fld, relfld, n,
					&s_handle, FALSE);
	    else
		modify_relation_field (tdbb, fld, relfld, &m_handle);
	    desc.dsc_dtype = dtype_text;
	    INTL_ASSIGN_DSC (&desc, CS_METADATA, COLLATE_NONE);
	    desc.dsc_address = (UCHAR*) names [relfld [RFLD_R_NAME]];
	    desc.dsc_length = strlen (desc.dsc_address);
	    DFW_post_work (dbb->dbb_sys_trans, dfw_update_format, &desc, 0);
	    }

if (s_handle)
    CMP_release (tdbb, s_handle);
if (m_handle)
    CMP_release (tdbb, m_handle);

DFW_perform_system_work();
}

static void add_security_to_sys_rel (
    TDBB        tdbb,
    TEXT        *user_name,
    TEXT        *rel_name,
    TEXT        *acl,
    SSHORT      acl_length)
{
/**************************************
 *
 *      a d d _ s e c u r i t y _ t o _ s y s _ r e l
 *
 **************************************
 *
 * Functional description
 *
 *      Add security to system relations. Only the owner of the 
 *      database has SELECT/INSERT/UPDATE/DELETE privileges on
 *      any system relations. Any other users only has SELECT
 *      privilege.
 *
 **************************************/

DBB             dbb;
GDS__QUAD       blob_id_1, blob_id_2;
BLB             blob;
BLK             request;
BLK             handle1;
TEXT            sec_class_name [100];
TEXT	        default_class [32];
SSHORT          cnt;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

strcpy (sec_class_name, "SQL$");
strcat (sec_class_name, rel_name);

blob = BLB_create (tdbb, dbb->dbb_sys_trans, &blob_id_1);
BLB_put_segment (tdbb, blob, acl, acl_length);
BLB_close (tdbb, blob);

blob = BLB_create (tdbb, dbb->dbb_sys_trans, &blob_id_2);
BLB_put_segment (tdbb, blob, acl, acl_length);
BLB_close (tdbb, blob);

sprintf (default_class, "%s%" QUADFORMAT "d\0", DEFAULT_CLASS,
         DPM_gen_id (tdbb, MET_lookup_generator (tdbb, DEFAULT_CLASS),
         0, (SINT64) 1));

for (cnt = 0; cnt < 6; cnt++)
    {
    handle1 = NULL;

    STORE (REQUEST_HANDLE handle1) PRIV IN RDB$USER_PRIVILEGES
        switch (cnt)
            {
            case 0:
                strcpy (PRIV.RDB$USER, user_name);
                PRIV.RDB$PRIVILEGE [0] = 'S';
                PRIV.RDB$GRANT_OPTION  = 1;
                break;
            case 1:
                strcpy (PRIV.RDB$USER, user_name);
                PRIV.RDB$PRIVILEGE [0] = 'I';
                PRIV.RDB$GRANT_OPTION  = 1;
                break;
            case 2:
                strcpy (PRIV.RDB$USER, user_name);
                PRIV.RDB$PRIVILEGE [0] = 'U';
                PRIV.RDB$GRANT_OPTION  = 1;
                break;
            case 3:
                strcpy (PRIV.RDB$USER, user_name);
                PRIV.RDB$PRIVILEGE [0] = 'D';
                PRIV.RDB$GRANT_OPTION  = 1;
                break;
            case 4:
                strcpy (PRIV.RDB$USER, user_name);
                PRIV.RDB$PRIVILEGE [0] = 'R';
                PRIV.RDB$GRANT_OPTION  = 1;
                break;
            default:
                strcpy (PRIV.RDB$USER, "PUBLIC");
                PRIV.RDB$PRIVILEGE [0] = 'S';
                PRIV.RDB$GRANT_OPTION  = 0;
                break;
            }
        strcpy (PRIV.RDB$GRANTOR, user_name);
        PRIV.RDB$PRIVILEGE [1] = 0;
        strcpy (PRIV.RDB$RELATION_NAME, rel_name);
        PRIV.RDB$FIELD_NAME.NULL = TRUE;
        PRIV.RDB$USER_TYPE   = obj_user;
        PRIV.RDB$OBJECT_TYPE = obj_relation;
    END_STORE;

    CMP_release (tdbb, handle1);
    }

handle1 = NULL;

STORE (REQUEST_HANDLE handle1)
        CLS IN RDB$SECURITY_CLASSES
    jrd_vtof (sec_class_name, CLS.RDB$SECURITY_CLASS,
              sizeof (CLS.RDB$SECURITY_CLASS));
    CLS.RDB$ACL = blob_id_1;
END_STORE;

CMP_release (tdbb, handle1);

handle1 = NULL;

STORE (REQUEST_HANDLE handle1)
        CLS IN RDB$SECURITY_CLASSES
    jrd_vtof (default_class, CLS.RDB$SECURITY_CLASS,
              sizeof (CLS.RDB$SECURITY_CLASS));
    CLS.RDB$ACL = blob_id_2;
END_STORE;

CMP_release (tdbb, handle1);

handle1 = NULL;

FOR (REQUEST_HANDLE handle1) REL IN RDB$RELATIONS
    WITH REL.RDB$RELATION_NAME EQ rel_name

    MODIFY REL USING
        REL.RDB$DEFAULT_CLASS.NULL = FALSE;
        jrd_vtof (default_class, REL.RDB$DEFAULT_CLASS,
                  sizeof (REL.RDB$DEFAULT_CLASS));
    END_MODIFY;

END_FOR;

CMP_release (tdbb, handle1);

}

static void add_trigger (
    TEXT	*trigger_name,
    BLK		*handle1,
    BLK		*handle2)
{
/**************************************
 *
 *	a d d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Store a trigger of the given name.
 *	This routine is used to upgrade ODS versions.
 *	DO NOT DELETE, even though it is not used
 *	now, since it will be used when we go to 8.1.
 *
 **************************************/
TDBB	tdbb;
TRG	*trigger;
TRIGMSG	*message;

tdbb = GET_THREAD_DATA;

/* Find the new trigger to be stored; assume it exists in the table */

for (trigger = triggers; strcmp (trigger->trg_name, trigger_name); trigger++)
    ;
 
store_trigger (tdbb, trigger, handle1);

/* Look for any related trigger messages */

for (message = trigger_messages; message->trigmsg_name; ++message)
    if (!strcmp (message->trigmsg_name, trigger->trg_name))
	store_message (tdbb, message, handle2);
}

static void modify_relation_field (
    TDBB	tdbb,
    UCHAR	*fld,
    UCHAR	*relfld,
    BLK		*handle)
{
/**************************************
 *
 *	m o d i f y _ r e l a t i o n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Modify a local field according to the 
 *	passed information.  Note that the field id and
 *	field position do not change.
 *
 **************************************/
DBB	dbb;
GFLD	*gfield;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

FOR (REQUEST_HANDLE *handle) X IN RDB$RELATION_FIELDS WITH
    X.RDB$RELATION_NAME EQ names [relfld [RFLD_R_NAME]] AND
    X.RDB$FIELD_NAME EQ names [fld [RFLD_F_NAME]]
    MODIFY X USING
	gfield = &gfields [fld [RFLD_F_UPD_ID]];
	PAD (names [gfield->gfld_name], X.RDB$FIELD_SOURCE);
	X.RDB$UPDATE_FLAG = fld [RFLD_F_UPDATE];
    END_MODIFY;
END_FOR;
}

static void store_generator (
    TDBB	tdbb,
    GEN		*generator,
    BLK		*handle)
{
/**************************************
 *
 *	s t o r e _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Store the passed generator according to 
 *	the information in the generator block.
 *
 **************************************/
DBB	dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

STORE (REQUEST_HANDLE *handle) X IN RDB$GENERATORS
    PAD (generator->gen_name, X.RDB$GENERATOR_NAME);
    X.RDB$GENERATOR_ID = generator->gen_id;
    X.RDB$SYSTEM_FLAG = RDB_system;
END_STORE;
}

static void store_global_field (
    TDBB	tdbb,
    GFLD	*gfield,
    BLK		*handle)
{
/**************************************
 *
 *	s t o r e _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Store a global field according to the 
 *	passed information.
 *
 **************************************/
DBB		dbb;
TRA		trans;
struct blb	*blob;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

trans = dbb->dbb_sys_trans;

STORE (REQUEST_HANDLE *handle) X IN RDB$FIELDS
    PAD (names [gfield->gfld_name], X.RDB$FIELD_NAME);
    X.RDB$FIELD_LENGTH = gfield->gfld_length;
    X.RDB$FIELD_SCALE = 0;
    X.RDB$SYSTEM_FLAG = RDB_system;
    X.RDB$FIELD_SUB_TYPE.NULL = TRUE;
    X.RDB$CHARACTER_SET_ID.NULL = TRUE;
    X.RDB$COLLATION_ID.NULL = TRUE;
    X.RDB$SEGMENT_LENGTH.NULL = TRUE;
    if (gfield->gfld_dflt_blr)
	{
	blob = BLB_create (tdbb, trans, &X.RDB$DEFAULT_VALUE);
	BLB_put_segment (tdbb, blob, gfield->gfld_dflt_blr, gfield->gfld_dflt_len);
	BLB_close (tdbb, blob);
	X.RDB$DEFAULT_VALUE.NULL = FALSE;
	}
    else
	X.RDB$DEFAULT_VALUE.NULL = TRUE;
    switch (gfield->gfld_dtype)
	{
	case dtype_timestamp:
	    X.RDB$FIELD_TYPE = (int) blr_timestamp;
	    break;

	case dtype_sql_time:
	    X.RDB$FIELD_TYPE = (int) blr_sql_time;
	    break;

	case dtype_sql_date:
	    X.RDB$FIELD_TYPE = (int) blr_sql_date;
	    break;

	case dtype_short:
	case dtype_long:
	case dtype_int64:
	    if (gfield->gfld_dtype == dtype_short)
		X.RDB$FIELD_TYPE = (int) blr_short;
	    else if (gfield->gfld_dtype == dtype_long)
		X.RDB$FIELD_TYPE = (int) blr_long;
	    else 
		X.RDB$FIELD_TYPE = (int) blr_int64;
	    if ((gfield->gfld_sub_type == dsc_num_type_numeric) || 
		(gfield->gfld_sub_type == dsc_num_type_decimal))
		{
	        X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	        X.RDB$FIELD_SUB_TYPE = gfield->gfld_sub_type;
		}
	    break;

	case dtype_double:
	    X.RDB$FIELD_TYPE = (int) blr_double;
	    break;

	case dtype_text:
	case dtype_varying:
	    if (gfield->gfld_dtype == dtype_text)
	        X.RDB$FIELD_TYPE = (int) blr_text;
	    else
		{
	        X.RDB$FIELD_TYPE = (int) blr_varying;
	        X.RDB$FIELD_LENGTH -= sizeof (USHORT);
		}
	    if (gfield->gfld_sub_type == dsc_text_type_metadata)
		{
    	        X.RDB$CHARACTER_SET_ID.NULL = FALSE;
    	        X.RDB$CHARACTER_SET_ID = CS_METADATA;
    	        X.RDB$COLLATION_ID.NULL = FALSE;
    	        X.RDB$COLLATION_ID = COLLATE_NONE;
	        X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	        X.RDB$FIELD_SUB_TYPE = gfield->gfld_sub_type;
		}
	    else
		{
    	        X.RDB$CHARACTER_SET_ID.NULL = FALSE;
    	        X.RDB$CHARACTER_SET_ID = CS_NONE;
    	        X.RDB$COLLATION_ID.NULL = FALSE;
    	        X.RDB$COLLATION_ID = COLLATE_NONE;
		}
	    break;

	case dtype_blob:
	    X.RDB$FIELD_TYPE = (int) blr_blob;
	    X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	    X.RDB$SEGMENT_LENGTH.NULL = FALSE;
	    X.RDB$FIELD_SUB_TYPE = gfield->gfld_sub_type;
	    X.RDB$SEGMENT_LENGTH = 80;
	    if (gfield->gfld_sub_type == BLOB_text)
		{
    	        X.RDB$CHARACTER_SET_ID.NULL = FALSE;
    	        X.RDB$CHARACTER_SET_ID = CS_METADATA;
		}
	    break;

	default:
	    assert (FALSE);
	    break;
	}

END_STORE;
}

static void store_intlnames (
    TDBB	tdbb,
    DBB		dbb)
{
/**************************************
 *
 *	s t o r e _ i n t l n a m e s 
 *
 **************************************
 *
 * Functional description
 *	Store symbolic names & information for international
 *	character sets & collations.
 *
 **************************************/
CS_TYPE		*csptr;
COLL_TYPE	*collptr;
BLK		handle;

SET_TDBB (tdbb);

handle = NULL;
for (csptr = cs_types; csptr->init_charset_name; csptr++)
    {
    STORE (REQUEST_HANDLE handle) X IN RDB$CHARACTER_SETS USING
	PAD (csptr->init_charset_name, X.RDB$CHARACTER_SET_NAME);
	PAD (csptr->init_charset_name, X.RDB$DEFAULT_COLLATE_NAME);
	X.RDB$CHARACTER_SET_ID     = csptr->init_charset_id;
	X.RDB$BYTES_PER_CHARACTER  = csptr->init_charset_bytes_per_char;
	X.RDB$SYSTEM_FLAG = RDB_system;
    END_STORE;
    }

CMP_release (tdbb, handle);
handle = NULL;

for (collptr = coll_types; collptr->init_collation_name; collptr++)
    {
    STORE (REQUEST_HANDLE handle) X IN RDB$COLLATIONS USING
	PAD (collptr->init_collation_name, X.RDB$COLLATION_NAME); 
	X.RDB$CHARACTER_SET_ID = collptr->init_collation_charset;
	X.RDB$COLLATION_ID     = collptr->init_collation_id;
	X.RDB$SYSTEM_FLAG = RDB_system;
    END_STORE;
    }

CMP_release (tdbb, handle);
handle = NULL;
}

static void store_message (
    TDBB	tdbb,
    TRIGMSG	*message,
    BLK		*handle)
{
/**************************************
 *
 *	s t o r e _ m e s s a g e
 *
 **************************************
 *
 * Functional description
 *	Store system trigger messages.
 *
 **************************************/
DBB	dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

/* store the trigger */

STORE (REQUEST_HANDLE *handle) X IN RDB$TRIGGER_MESSAGES
   PAD (message->trigmsg_name, X.RDB$TRIGGER_NAME);
   X.RDB$MESSAGE_NUMBER = message->trigmsg_number;
   PAD (message->trigmsg_text, X.RDB$MESSAGE);   
END_STORE;
}

static void store_relation_field (
    TDBB	tdbb,
    UCHAR	*fld,
    UCHAR	*relfld,
    int		field_id,
    BLK		*handle,
    int		fmt0_flag)
{
/**************************************
 *
 *	s t o r e _ r e l a t i o n _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Store a local field according to the 
 *	passed information.
 *
 **************************************/
DBB	dbb;
GFLD	*gfield;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

STORE (REQUEST_HANDLE *handle) X IN RDB$RELATION_FIELDS
    gfield = (fld [RFLD_F_UPD_MINOR] && !fmt0_flag) ?
	&gfields [fld [RFLD_F_UPD_ID]] : &gfields [fld [RFLD_F_ID]];
    PAD (names [relfld [RFLD_R_NAME]], X.RDB$RELATION_NAME);
    PAD (names [fld [RFLD_F_NAME]], X.RDB$FIELD_NAME);
    PAD (names [gfield->gfld_name], X.RDB$FIELD_SOURCE);
    X.RDB$FIELD_POSITION = field_id;
    X.RDB$FIELD_ID = field_id;
    X.RDB$SYSTEM_FLAG = RDB_system;
    X.RDB$UPDATE_FLAG = fld [RFLD_F_UPDATE];
END_STORE;
}

static void store_trigger (
    TDBB	tdbb,
    TRG		*trigger,
    BLK		*handle)
{
/**************************************
 *
 *	s t o r e _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Store the trigger according to the 
 *	information in the trigger block.
 *
 **************************************/
DBB		dbb;
TRA		trans;
DSC		desc;
struct blb	*blob;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

trans = dbb->dbb_sys_trans;

/* indicate that the relation format needs revising */

desc.dsc_dtype = dtype_text;
INTL_ASSIGN_DSC (&desc, CS_METADATA, COLLATE_NONE);
desc.dsc_address = (UCHAR*) names [trigger->trg_relation];
desc.dsc_length = strlen (desc.dsc_address);
DFW_post_work (trans, dfw_update_format, &desc, 0);

/* store the trigger */

STORE (REQUEST_HANDLE *handle) X IN RDB$TRIGGERS
   PAD (trigger->trg_name, X.RDB$TRIGGER_NAME);
   PAD (names [trigger->trg_relation], X.RDB$RELATION_NAME);
   X.RDB$TRIGGER_SEQUENCE = 0;
   X.RDB$SYSTEM_FLAG = RDB_system;
   X.RDB$TRIGGER_TYPE = trigger->trg_type;
   X.RDB$FLAGS = trigger->trg_flags;
   blob = BLB_create (tdbb, trans, &X.RDB$TRIGGER_BLR);
   BLB_put_segment (tdbb, blob, trigger->trg_blr, trigger->trg_length);
   BLB_close (tdbb, blob);
END_STORE;
}
