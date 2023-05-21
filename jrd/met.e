/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met.e
 *	DESCRIPTION:	Meta data handler
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

#ifdef SHLIB_DEFS
#define LOCAL_SHLIB_DEFS
#endif

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../jrd/ibsetjmp.h"
#include "../jrd/common.h"
#include <stdarg.h>

#include "../jrd/constants.h"
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/scl.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/pio.h"
#include "../jrd/sdw.h"
#include "../jrd/flags.h"
#include "../jrd/all.h"
#include "../jrd/lls.h"
#include "../jrd/intl.h"
#include "../jrd/align.h"
#include "../jrd/jrn.h"
#include "../jrd/flu.h"
#include "../jrd/blf.h"
#include "../intl/charsets.h"
#include "../jrd/gdsassert.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/ini_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/pcmet_proto.h"
#include "../jrd/pio_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/iberr.h"

#ifdef  WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

/* Pick up relation ids */

#define RELATION(name,id,ods)       id,
#define FIELD(symbol,name,id,update,ods,new_id,new_ods)
#define END_RELATION

typedef ENUM rids {
#include "../jrd/relations.h"
    rel_MAX
} RIDS;

#undef RELATION
#undef FIELD
#undef END_RELATION

#define BLR_BYTE        *((*csb)->csb_running)++
#define NULL_BLOB(id)   (!id.gds_quad_high && !id.gds_quad_low)

#define	BLANK			'\040'

DATABASE
    DB = FILENAME "ODS.RDB"; 

static void     blocking_ast_procedure (PRC);
static void     blocking_ast_relation (REL);
static void     get_trigger (TDBB, REL, SLONG [2], VEC *, TEXT *, BOOLEAN, 
				    USHORT);
static BOOLEAN  get_type		(TDBB, SSHORT *, UCHAR *,
						UCHAR *);
static void     lookup_view_contexts (TDBB, REL);
static void     name_copy (TEXT *, TEXT *);
static USHORT   name_length (TEXT *);
static NOD      parse_procedure_blr (TDBB, PRC, SLONG [2], CSB *);
static BOOLEAN  par_messages (TDBB, UCHAR *, USHORT, PRC, CSB *);
static BOOLEAN  resolve_charset_and_collation (TDBB, SSHORT *, UCHAR *, UCHAR *);
static STR      save_name (TDBB, TEXT*);
static void     save_trigger_request (DBB, VEC *, REQ);
static void     store_dependencies	(TDBB, CSB, TEXT *,
						USHORT);
static void     terminate (TEXT *, USHORT);
static BOOLEAN  verify_TRG_ignore_perm (TDBB, TEXT *);

#ifdef SHLIB_DEFS
#define strlen          (*_libgds_strlen)
#define strcmp          (*_libgds_strcmp)
#define strcpy          (*_libgds_strcpy)
#define vsprintf        (*_libgds_vsprintf)
#define SETJMP          (*_libgds_setjmp)
#define memcmp          (*_libgds_memcmp)
#define strncmp         (*_libgds_strncmp)
#define memset          (*_libgds_memset)
#define _iob            (*_libgds__iob)
#define ib_fprintf         (*_libgds_fprintf)

extern int              strlen();
extern int              strcmp();
extern char             *strcpy();
extern int              vsprintf();
extern int              SETJMP();
extern int              memcmp();
extern int              strncmp();
extern void             *memset();
extern IB_FILE             _iob [];
extern int              ib_fprintf();
#endif

void MET_activate_shadow (
    TDBB	tdbb)
{
/**************************************
 *
 *      M E T _ a c t i v a t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *      Activate the current database, which presumably
 *      was formerly a shadow, by deleting all records
 *      corresponding to the shadow that this database
 *      represents.
 *      Get rid of write ahead log for the activated shadow.
 *
 **************************************/
DBB     dbb;
BLK     handle, handle2;
SCHAR   expanded_name [MAX_PATH_LENGTH], *dbb_file_name;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

/* Erase any secondary files of the primary database of the
   shadow being activated. */
   
handle = NULL;
FOR (REQUEST_HANDLE handle) X IN RDB$FILES
     WITH X.RDB$SHADOW_NUMBER NOT MISSING
     AND X.RDB$SHADOW_NUMBER EQ 0
     ERASE X;
END_FOR;

CMP_release (tdbb, handle);

dbb_file_name = dbb->dbb_file->fil_string;

/* go through files looking for any that expand to the current database name */

handle = handle2 = NULL;
FOR (REQUEST_HANDLE handle) X IN RDB$FILES 
      WITH X.RDB$SHADOW_NUMBER NOT MISSING
      AND X.RDB$SHADOW_NUMBER NE 0
    PIO_expand (X.RDB$FILE_NAME, strlen (X.RDB$FILE_NAME), expanded_name);

    if (!strcmp (expanded_name, dbb_file_name))
	{
	FOR (REQUEST_HANDLE handle2) Y IN RDB$FILES
	      WITH X.RDB$SHADOW_NUMBER EQ Y.RDB$SHADOW_NUMBER
	    MODIFY Y
		Y.RDB$SHADOW_NUMBER = 0;
	    END_MODIFY;
	END_FOR;
	ERASE X;
	}
END_FOR;
    
if (handle2)
    CMP_release (tdbb, handle2);
CMP_release (tdbb, handle);

/* Get rid of WAL after activation.  For V4.0, we are not allowing
   WAL and Shadowing to be configured together.  So the following
   (commented out) code should be re-visited when we do allow 
   such configuration. */

/***********************
handle = 0;
FOR (REQUEST_HANDLE handle) X IN RDB$LOG_FILES 
    ERASE X;
END_FOR;
CMP_release (tdbb, handle);
#ifndef WINDOWS_ONLY
AIL_modify_log ();
#endif
************************/
}

ULONG MET_align (
    DSC         *desc,
    USHORT      value)
{
/**************************************
 *
 *      M E T _ a l i g n
 *
 **************************************
 *
 * Functional description
 *      Align value (presumed offset) on appropriate border
 *      and return.
 *
 **************************************/
USHORT  alignment;

alignment = desc->dsc_length;
switch (desc->dsc_dtype)
    {
    case dtype_text:
    case dtype_cstring:
	return value;
    
    case dtype_varying:
	alignment = sizeof (USHORT);
	break;
    }

alignment = MIN (alignment, ALIGNMENT);

return ALIGN (value, alignment);
}

void MET_change_fields (
    TDBB	tdbb,
    TRA transaction,
    DSC *field_source)
{
/**************************************
 *
 *      M E T _ c h a n g e _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *      Somebody is modifying RDB$FIELDS.  Find all relations affected
 *      and schedule a format update.
 *
 **************************************/
DBB     dbb;
BLK     request;
DSC     relation_name;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_m_fields, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$RELATION_FIELDS WITH 
	X.RDB$FIELD_SOURCE EQ field_source->dsc_address
    if (!REQUEST (irq_m_fields))
	REQUEST (irq_m_fields) = request;
    relation_name.dsc_dtype = dtype_text;
    INTL_ASSIGN_DSC (&relation_name, CS_METADATA, COLLATE_NONE);
    relation_name.dsc_length = sizeof (X.RDB$RELATION_NAME);
    relation_name.dsc_address = (UCHAR*) X.RDB$RELATION_NAME;
    SCL_check_relation (&relation_name, SCL_control);
    DFW_post_work (transaction, dfw_update_format, &relation_name, 0);
END_FOR;

if (!REQUEST (irq_m_fields))
    REQUEST (irq_m_fields) = request;
}

FMT MET_current (
    TDBB	tdbb,
    REL 	relation)
{
/**************************************
 *
 *      M E T _ c u r r e n t
 *
 **************************************
 *
 * Functional description
 *      Get the current format for a relation.  The current format is the
 *      format in which new records are to be stored.
 *
 **************************************/

if (relation->rel_current_format)
    return relation->rel_current_format;

SET_TDBB (tdbb);

return relation->rel_current_format = MET_format (tdbb, relation, relation->rel_current_fmt);
}

void MET_delete_dependencies (
    TDBB	tdbb,
    TEXT        *object_name,
    USHORT      dependency_type)
{
/**************************************
 *
 *      M E T _ d e l e t e _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *      Delete all dependencies for the specified 
 *      object of given type.
 *
 **************************************/
DBB     dbb;
BLK     request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

if (!object_name)
    return;

request = (BLK) CMP_find_request (tdbb, irq_d_deps, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
	DEP IN RDB$DEPENDENCIES
	WITH DEP.RDB$DEPENDENT_NAME = object_name
	AND DEP.RDB$DEPENDENT_TYPE = dependency_type
    if (!REQUEST (irq_d_deps))
	REQUEST (irq_d_deps) = request;
    ERASE DEP;
END_FOR;

if (!REQUEST (irq_d_deps))
    REQUEST (irq_d_deps) = request;
}

void MET_delete_shadow (
    TDBB	tdbb,
    USHORT      shadow_number)
{
/**************************************
 *
 *      M E T _ d e l e t e _ s h a d o w
 *
 **************************************
 *
 * Functional description 
 *      When any of the shadows in RDB$FILES for a particular
 *      shadow are deleted, stop shadowing to that file and
 *      remove all other files from the same shadow.
 *
 **************************************/
DBB     dbb;
BLK     handle;
SDW     shadow;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

handle = NULL;
FOR (REQUEST_HANDLE handle) 
      X IN RDB$FILES
      WITH X.RDB$SHADOW_NUMBER EQ shadow_number
    ERASE X;    
END_FOR;
CMP_release (tdbb, handle);
       
for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
    if (shadow->sdw_number == shadow_number)
	shadow->sdw_flags |= SDW_shutdown;

/* notify other processes to check for shadow deletion */
if ( SDW_lck_update( (SLONG)0) )
	SDW_notify();
}

void MET_error (
    TEXT        *string,
    ...)
{
/**************************************
 *
 *      M E T _ e r r o r
 *
 **************************************
 *
 * Functional description
 *      Post an error in a metadata update
 *      Oh, wow.
 *
 **************************************/
TEXT    s [128];
va_list ptr;

VA_START (ptr, string);
vsprintf (s, string, ptr);

ERR_post (gds__no_meta_update, gds_arg_gds, gds__random, gds_arg_string,
	ERR_cstring (s), 0);
}

SCHAR *MET_exact_name (
    TEXT	*str)
{
/**************************************
 *
 *	M E T _ e x a c t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Trim off trailing spaces from a metadata name.
 *	eg: insert a null after the last non-blank character.
 *
 *	SQL delimited identifier may have blank as part of the name
 *
 *	Parameters:  str - the string to terminate
 *	Returns:     str
 *
 **************************************/
TEXT    *p, *q;

q = str - 1;
for (p = str; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

*(q+1) = '\0';
return str;
}

FMT MET_format (
    TDBB		tdbb,
    register REL        relation,
    USHORT              number)
{
/**************************************
 *
 *      M E T _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *      Lookup a format for given relation.
 *
 **************************************/
DBB     dbb;
VEC     formats;
FMT     format;
BLB     blob;
DSC     *desc;
BLK     request;
USHORT  count;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

if ((formats = relation->rel_formats) &&
    (number < formats->vec_count) &&
    (format = (FMT) formats->vec_object [number]))
    return format;

format = NULL;
request = (BLK) CMP_find_request (tdbb, irq_r_format, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    X IN RDB$FORMATS WITH X.RDB$RELATION_ID EQ relation->rel_id AND
			  X.RDB$FORMAT EQ number
    if (!REQUEST (irq_r_format))
	REQUEST (irq_r_format) = request;
    blob = BLB_open (tdbb, dbb->dbb_sys_trans, &X.RDB$DESCRIPTOR);
    count = blob->blb_length / sizeof (struct dsc);
    format = (FMT) ALLOCPV (type_fmt, count);
    format->fmt_count = count;
    BLB_get_data (tdbb, blob, format->fmt_desc, blob->blb_length);
    for (desc = format->fmt_desc + count - 1; desc >= format->fmt_desc; --desc)
	if (desc->dsc_address)
	    {
	    format->fmt_length = (ULONG) desc->dsc_address + desc->dsc_length;
	    break;
	    }
END_FOR;

if (!REQUEST (irq_r_format))
    REQUEST (irq_r_format) = request;

if (!format)
    format = (FMT) ALLOCPV (type_fmt, 0);

format->fmt_version = number;

/* Link the format block into the world */

formats = ALL_vector (dbb->dbb_permanent, &relation->rel_formats, number);
formats->vec_object [number] = (BLK) format;

return format;
}

BOOLEAN MET_get_char_subtype (
    TDBB	tdbb,
    SSHORT      *id,
    UCHAR       *name,
    USHORT      length)
{
/**************************************
 *
 *      M E T _ g e t _ c h a r _ s u b t y p e 
 *
 **************************************
 *
 * Functional description
 *      Character types can be specified as either:
 *         a) A POSIX style locale name "<collation>.<characterset>"
 *         or
 *         b) A simple <characterset> name (using default collation)
 *         c) A simple <collation> name    (use charset for collation)
 *
 *      Given an ASCII7 string which could be any of the above, try to
 *      resolve the name in the order a, b, c
 *      a) is only tried iff the name contains a period.
 *      (in which case b) and c) are not tried).
 *
 * Return:
 *      1 if no errors (and *id is set).
 *      0 if the name could not be resolved.
 *
 **************************************/
UCHAR   buffer [32];            /* BASED ON RDB$COLLATION_NAME */
UCHAR   *p;
UCHAR   *period = NULL;
UCHAR   *end_name;
BOOLEAN res;

SET_TDBB (tdbb);

assert (id != NULL);
assert (name != NULL);

end_name = name + length;

/* Force key to uppercase, following C locale rules for uppercasing */
/* At the same time, search for the first period in the string (if any) */

for (p = buffer;
      name < end_name && p < buffer + sizeof (buffer) - 1; 
      p++, name++)
    {
    *p = UPPER7 (*name);
    if ((*p == '.') && !period)
	period = p;
    }
*p = 0;

/* Is there a period, separating collation name from character set? */
if (period)
    {
    *period = 0;
    return resolve_charset_and_collation (tdbb, id, period+1, buffer);
    }

else
    {
    /* Is it a character set name (implying charset default collation sequence) */

    res = resolve_charset_and_collation (tdbb, id, buffer, NULL);
    if (!res)
	{
	/* Is it a collation name (implying implementation-default character set) */
	res = resolve_charset_and_collation (tdbb, id, NULL, buffer);
	}
    return res;
    }
}

NOD MET_get_dependencies (
    TDBB	tdbb,
    REL         relation,
    TEXT        *blob,
    CSB         view_csb,
    SLONG       blob_id [2],
    REQ         *request,
    CSB         *csb_ptr,
    TEXT        *object_name,
    USHORT      type)
{
/**************************************
 *
 *      M E T _ g e t _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *      Get dependencies for an object by parsing 
 *      the blr used in its definition.
 *
 **************************************/
DBB     dbb;
CSB     csb;
NOD     node;
BLK     handle;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

csb = (CSB) ALLOCDV (type_csb, 5);
csb->csb_count = 5;
csb->csb_g_flags |= csb_get_dependencies;

if (blob)
    node = PAR_blr (tdbb, relation, blob, view_csb, &csb, request,
		(type == obj_trigger) ? TRUE : FALSE, 0);
else
    node = MET_parse_blob (tdbb, relation, blob_id, &csb, request,
		(type == obj_trigger) ? TRUE : FALSE, FALSE);

if (type == (USHORT) obj_computed)
    {
    handle = NULL;
    FOR (REQUEST_HANDLE handle)
	    RLF IN RDB$RELATION_FIELDS CROSS
	    FLD IN RDB$FIELDS WITH
	    RLF.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	    RLF.RDB$RELATION_NAME EQ relation->rel_name AND
	    RLF.RDB$FIELD_NAME EQ object_name
	object_name = FLD.RDB$FIELD_NAME;
    END_FOR;
    CMP_release (tdbb, handle);
    }   

store_dependencies (tdbb, csb, object_name, type);

if (csb_ptr)
    *csb_ptr = csb;
else
    ALL_release (csb);

return node;
}

FLD MET_get_field (
    REL         relation,
    USHORT      id)
{
/**************************************
 *
 *      M E T _ g e t _ f i e l d
 *
 **************************************
 *
 * Functional description
 *      Get the field block for a field if possible.  If not,
 *      return NULL;
 *
 **************************************/
VEC     vector;

if (!relation ||
    !(vector = relation->rel_fields) ||
    id >= vector->vec_count)
    return NULL;

return (FLD) vector->vec_object [id];
}

void MET_get_shadow_files (
    TDBB	tdbb,
    USHORT      delete)
{
/**************************************
 *
 *      M E T _ g e t _ s h a d o w _ f i l e s
 *
 **************************************
 *
 * Functional description
 *      Check the shadows found in the database against
 *      our in-memory list: if any new shadow files have 
 *      been defined since the last time we looked, start
 *      shadowing to them; if any have been deleted, stop
 *      shadowing to them.
 *
 **************************************/
DBB     dbb;
BLK     handle;
USHORT  file_flags;
SDW     shadow;
 
SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

handle = NULL;
FOR (REQUEST_HANDLE handle) X IN RDB$FILES 
      WITH X.RDB$SHADOW_NUMBER NOT MISSING
      AND X.RDB$SHADOW_NUMBER NE 0
      AND X.RDB$FILE_SEQUENCE EQ 0
    if ((X.RDB$FILE_FLAGS & FILE_shadow) &&
       !(X.RDB$FILE_FLAGS & FILE_inactive))
	{
	file_flags = X.RDB$FILE_FLAGS;
	SDW_start (X.RDB$FILE_NAME, X.RDB$SHADOW_NUMBER, file_flags, delete);

	/* if the shadow exists, mark the appropriate shadow 
	   block as found for the purposes of this routine;
	   if the shadow was conditional and is no longer, note it */

	for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
	    if ((shadow->sdw_number == X.RDB$SHADOW_NUMBER) &&
	       !(shadow->sdw_flags & SDW_IGNORE))
		{
		shadow->sdw_flags |= SDW_found;
		if (!(file_flags & FILE_conditional))
		    shadow->sdw_flags &= ~SDW_conditional;
		break;
		}
	}
END_FOR;
CMP_release (tdbb, handle);

/* if any current shadows were not defined in database, mark 
   them to be shutdown since they don't exist anymore */

for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
    if (!(shadow->sdw_flags & SDW_found))
	shadow->sdw_flags |= SDW_shutdown;
    else
	shadow->sdw_flags &= ~SDW_found;
     
SDW_check ();
}

int MET_get_walinfo (
    TDBB	tdbb,
    LGFILE      **logfiles,
    ULONG       *number,
    LGFILE      **over_flow)
{
/**************************************
 *
 *      M E T _ g e t _ w a l i n f o
 *
 **************************************
 *
 * Functional description
 *      Get WAL information from RDB$LOG_FILES
 *      Initialize over_flow pointer if over flow file is specified.
 *
 **************************************/
DBB     dbb;
BLK     handle;
SSHORT  num = 0;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

handle = NULL;

FOR (REQUEST_HANDLE handle)
	LOG IN RDB$LOG_FILES SORTED BY LOG.RDB$FILE_SEQUENCE
    logfiles [num] = (LGFILE*) ALLOCPV (type_ail,
				 LGFILE_SIZE+MAX_PATH_LENGTH);
    strcpy (logfiles [num]->lg_name, LOG.RDB$FILE_NAME);
    logfiles [num]->lg_size = LOG.RDB$FILE_LENGTH;
    logfiles [num]->lg_partitions = LOG.RDB$FILE_PARTITIONS;
    logfiles [num]->lg_flags = LOG.RDB$FILE_FLAGS;
    logfiles [num]->lg_sequence = LOG.RDB$FILE_SEQUENCE;

    num++;
END_FOR;

if (handle)
    CMP_release (tdbb, handle);

if (num)
    {
    *over_flow = logfiles [num-1];

    /* The overflow file will be passed separately */

    if (over_flow [0]->lg_flags & LOG_overflow)
	num--;
    else
	*over_flow = (LGFILE*) 0;
    }

*number = num;

return num;
}

void MET_load_trigger (
    TDBB	tdbb,
    REL         relation,
    TEXT        *trigger_name,
    VEC         *triggers)
{
/**************************************
 *
 *      M E T _ l o a d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *      Load triggers from RDB$TRIGGERS.  If a requested,
 *      also load triggers from RDB$RELATIONS.
 *
 **************************************/
DBB     dbb;
BLK     trigger_request;
VEC     *ptr;
USHORT  trig_flags;
TEXT    errmsg [MAX_ERRMSG_LEN + 1];

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

if (relation->rel_flags & REL_sys_trigs_being_loaded)
    return;

#ifdef READONLY_DATABASE
/* No need to load triggers for ReadOnly databases,
   since INSERT/DELETE/UPDATE statements are not going to be allowed */
if (dbb->dbb_flags & DBB_read_only)
    return;
#endif  /* READONLY_DATABASE */

/* Scan RDB$TRIGGERS next */

trigger_request = (BLK) CMP_find_request (tdbb, irq_s_triggers, IRQ_REQUESTS);

FOR (REQUEST_HANDLE trigger_request)
	TRG IN RDB$TRIGGERS WITH TRG.RDB$RELATION_NAME = relation->rel_name AND
	    TRG.RDB$TRIGGER_NAME EQ trigger_name
    if (!REQUEST (irq_s_triggers))
	REQUEST (irq_s_triggers) = trigger_request;
    if (TRG.RDB$TRIGGER_TYPE > 0 && TRG.RDB$TRIGGER_TYPE < TRIGGER_MAX)
	{
	ptr = triggers + TRG.RDB$TRIGGER_TYPE;

	/* check if the trigger is to be fired without any permissions
	   checks. Verify such a claim */
	trig_flags = (USHORT)TRG.RDB$FLAGS;

        /* if there is an ignore permission flag, see if it is legit */
	if ((TRG.RDB$FLAGS & TRG_ignore_perm) && 
            !verify_TRG_ignore_perm (tdbb, trigger_name))
	    {
	    gds__msg_format (NULL_PTR, JRD_BUGCHK, 304, sizeof (errmsg),
	        errmsg, trigger_name, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    ERR_log (JRD_BUGCHK, 304, errmsg);

	    trig_flags &=  ~TRG_ignore_perm;
            }

	get_trigger (tdbb, relation, &TRG.RDB$TRIGGER_BLR, ptr, TRG.RDB$TRIGGER_NAME, TRG.RDB$SYSTEM_FLAG, trig_flags);
	}
END_FOR;

if (!REQUEST (irq_s_triggers))
    REQUEST (irq_s_triggers) = trigger_request;
}


void DLL_EXPORT MET_lookup_cnstrt_for_index (
    TDBB	tdbb,
    TEXT        *constraint_name,
    TEXT        *index_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ c n s t r t _ f o r _ i n d e x
 *
 **************************************
 *
 * Functional description
 *      Lookup  constraint name from index name, if one exists.
 *      Calling routine must pass a buffer of at least 32 bytes.
 *
 **************************************/
DBB     dbb;
BLK     request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

constraint_name [0] = 0;
request = (BLK) CMP_find_request (tdbb, irq_l_cnstrt, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    X IN RDB$RELATION_CONSTRAINTS WITH X.RDB$INDEX_NAME EQ index_name

    if (!REQUEST (irq_l_cnstrt))
	REQUEST (irq_l_cnstrt) = request;

    X.RDB$CONSTRAINT_NAME [name_length (X.RDB$CONSTRAINT_NAME)] = 0;
    strcpy (constraint_name, X.RDB$CONSTRAINT_NAME);

END_FOR;

if (!REQUEST (irq_l_cnstrt))
    REQUEST (irq_l_cnstrt) = request;
}

void MET_lookup_cnstrt_for_trigger (
    TDBB	tdbb,
    TEXT        *constraint_name,
    TEXT        *relation_name,
    TEXT        *trigger_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ c n s t r t _ f o r _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *      Lookup  constraint name from trigger name, if one exists.
 *      Calling routine must pass a buffer of at least 32 bytes.
 *
 **************************************/
DBB     dbb;
BLK     request, request2;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

constraint_name [0] = 0;
relation_name [0] = 0;
request = (BLK) CMP_find_request (tdbb, irq_l_check, IRQ_REQUESTS);
request2 = (BLK) CMP_find_request (tdbb, irq_l_check2, IRQ_REQUESTS);

/* utilize two requests rather than one so that we 
   guarantee we always return the name of the relation
   that the trigger is defined on, even if we don't 
   have a check constraint defined for that trigger */

FOR (REQUEST_HANDLE request)
    Y IN RDB$TRIGGERS WITH
    Y.RDB$TRIGGER_NAME EQ trigger_name

    if (!REQUEST (irq_l_check))
	REQUEST (irq_l_check) = request;
      
    FOR (REQUEST_HANDLE request2) 
	X IN RDB$CHECK_CONSTRAINTS WITH
	X.RDB$TRIGGER_NAME EQ Y.RDB$TRIGGER_NAME

	if (!REQUEST (irq_l_check2))
	    REQUEST (irq_l_check2) = request2;

	X.RDB$CONSTRAINT_NAME [name_length (X.RDB$CONSTRAINT_NAME)] = 0;
	strcpy (constraint_name, X.RDB$CONSTRAINT_NAME);

    END_FOR;

    if (!REQUEST (irq_l_check2))
	REQUEST (irq_l_check2) = request2;

    Y.RDB$RELATION_NAME [name_length (Y.RDB$RELATION_NAME)] = 0;
    strcpy (relation_name, Y.RDB$RELATION_NAME);

END_FOR;

if (!REQUEST (irq_l_check))
    REQUEST (irq_l_check) = request;
}

void MET_lookup_exception (
    TDBB	tdbb,
    SLONG       number,
    TEXT        *name,
    TEXT        *message)
{
/**************************************
 *
 *      M E T _ l o o k u p _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *      Lookup exception by number and return its name and message.
 *
 **************************************/
DBB     dbb;
BLK     request;
SCHAR   *p;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
 
/* We need to look up exception in RDB$EXCEPTIONS */

request = (BLK) CMP_find_request (tdbb, irq_l_exception, IRQ_REQUESTS);

*name = 0;
if (message)
    *message = 0;

FOR (REQUEST_HANDLE request) 
    X IN RDB$EXCEPTIONS WITH X.RDB$EXCEPTION_NUMBER = number

    if (!REQUEST (irq_l_exception))
	REQUEST (irq_l_exception) = request;
    if (!X.RDB$EXCEPTION_NAME.NULL)
	name_copy (name, X.RDB$EXCEPTION_NAME);
    if (!X.RDB$MESSAGE.NULL && message)
	name_copy (message, X.RDB$MESSAGE);
END_FOR;

if (!REQUEST (irq_l_exception))
    REQUEST (irq_l_exception) = request;
}

SLONG MET_lookup_exception_number (
    TDBB	tdbb,
    TEXT        *name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ e x c e p t i o n _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *      Lookup exception by name and return its number.
 *
 **************************************/
DBB     dbb;
BLK     request;
SLONG   number;
 
SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
 
/* We need to look up exception in RDB$EXCEPTIONS */

request = (BLK) CMP_find_request (tdbb, irq_l_except_no, IRQ_REQUESTS);

number = 0;

FOR (REQUEST_HANDLE request) 
    X IN RDB$EXCEPTIONS WITH X.RDB$EXCEPTION_NAME = name

    if (!REQUEST (irq_l_except_no))
	REQUEST (irq_l_except_no) = request;
    number = X.RDB$EXCEPTION_NUMBER;

END_FOR;

if (!REQUEST (irq_l_except_no))
    REQUEST (irq_l_except_no) = request;

return number;
}

int MET_lookup_field (
    TDBB	tdbb,
    REL         relation,
    TEXT        *name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *      Look up a field name.
 *
 **************************************/
DBB     dbb;
BLK     request;
VEC     vector;
FLD     *field, *end;
TEXT    *p, *q;
USHORT  id;
UCHAR	length;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

/* Start by checking field names that we already know */

if (vector = relation->rel_fields)
    {
    length = strlen (name);
    for (field = (FLD*) vector->vec_object, id = 0, end = field + vector->vec_count;
	 field < end; field++, id++)
	if (*field && (*field)->fld_length == length && (p = (*field)->fld_name)) 
	    {
	    q = name;
	    while (*p++ == *q)
		if (!*q++)
		    return id;
	    }
    }

/* Not found.  Next, try system relations directly */

id = -1;
 
if (!relation->rel_name)
    return id;

request = (BLK) CMP_find_request (tdbb, irq_l_field, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    X IN RDB$RELATION_FIELDS WITH
    X.RDB$RELATION_NAME EQ relation->rel_name AND
    X.RDB$FIELD_NAME EQ name
    if (!REQUEST (irq_l_field))
	REQUEST (irq_l_field) = request;
    id = X.RDB$FIELD_ID;
END_FOR;

if (!REQUEST (irq_l_field))
    REQUEST (irq_l_field) = request;

return id;
}

BLF MET_lookup_filter (
    TDBB	tdbb,
    SSHORT      from,
    SSHORT      to)
{
/**************************************
 *
 *      M E T _ l o o k u p _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
DBB     dbb;
BLK     request;
PTR     filter;
TEXT    *p;
MOD	module;
LLS	stack;
BLF	blf;
STR	exception_msg;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

filter = NULL;
blf = NULL;

request = (BLK) CMP_find_request (tdbb, irq_r_filters, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$FILTERS WITH X.RDB$INPUT_SUB_TYPE EQ from AND X.RDB$OUTPUT_SUB_TYPE EQ to
    if (!REQUEST (irq_r_filters))
	REQUEST (irq_r_filters) = request;
    MET_exact_name (X.RDB$MODULE_NAME);
    MET_exact_name (X.RDB$ENTRYPOINT);
    filter = ISC_lookup_entrypoint (X.RDB$MODULE_NAME, X.RDB$ENTRYPOINT, ISC_EXT_LIB_PATH_ENV);
    if (filter)
	{
	blf = (BLF) ALLOCP (type_blf);
	blf->blf_next = NULL;
	blf->blf_from = from;
	blf->blf_to = to;
	blf->blf_filter = filter;
	exception_msg = (STR) ALLOCPV (type_str, strlen (EXCEPTION_MESSAGE) + 
						 strlen (X.RDB$FUNCTION_NAME) + 
						 strlen (X.RDB$ENTRYPOINT) +
						 strlen (X.RDB$MODULE_NAME) + 1 );
	sprintf (exception_msg->str_data, EXCEPTION_MESSAGE, X.RDB$FUNCTION_NAME, X.RDB$ENTRYPOINT, X.RDB$MODULE_NAME);
	blf->blf_exception_message = exception_msg;
	}
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
END_FOR;

if (!REQUEST (irq_r_filters))
    REQUEST (irq_r_filters) = request;

return blf;
}

SLONG MET_lookup_generator (
    TDBB	tdbb,
    TEXT        *name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *      Lookup generator (aka gen_id).  If we can't find it, make a new one.
 *
 **************************************/
DBB     dbb;
BLK     request;
SLONG   gen_id;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

if (!strcmp (name, "RDB$GENERATORS"))
    return 0;

gen_id = -1;

request = (BLK) CMP_find_request (tdbb, irq_r_gen_id, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$GENERATORS WITH X.RDB$GENERATOR_NAME EQ name
    if (!REQUEST (irq_r_gen_id))
	REQUEST (irq_r_gen_id) = request;
    gen_id = X.RDB$GENERATOR_ID;
END_FOR;

if (!REQUEST (irq_r_gen_id))
    REQUEST (irq_r_gen_id) = request;

return gen_id;
}

void DLL_EXPORT MET_lookup_index (
    TDBB	tdbb,
    TEXT        *index_name,
    TEXT        *relation_name,
    USHORT      number)
{
/**************************************
 *
 *      M E T _ l o o k u p _ i n d e x
 *
 **************************************
 *
 * Functional description
 *      Lookup index name from relation and index number.
 *      Calling routine must pass a buffer of at least 32 bytes.
 *
 **************************************/
DBB     dbb;
BLK     request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

index_name [0] = 0;

request = (BLK) CMP_find_request (tdbb, irq_l_index, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$INDICES WITH X.RDB$RELATION_NAME EQ relation_name
    AND X.RDB$INDEX_ID EQ number

    if (!REQUEST (irq_l_index))
	REQUEST (irq_l_index) = request;

    X.RDB$INDEX_NAME [name_length (X.RDB$INDEX_NAME)] = 0;
    strcpy (index_name, X.RDB$INDEX_NAME);

END_FOR;

if (!REQUEST (irq_l_index))
    REQUEST (irq_l_index) = request;
}

SLONG MET_lookup_index_name (
    TDBB	tdbb,
    TEXT        *index_name,
    SLONG       *relation_id,
    SSHORT	*status)
{
/**************************************
 *
 *      M E T _ l o o k u p _ i n d e x _ n a m e
 *
 **************************************
 *
 * Functional description
 *      Lookup index id from index name.
 *
 **************************************/
DBB     dbb;
BLK     request;
SLONG   id = -1;
REL     relation;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_l_index_name, IRQ_REQUESTS);

*status = MET_object_unknown;

FOR (REQUEST_HANDLE request) 
    X IN RDB$INDICES WITH 
	X.RDB$INDEX_NAME EQ index_name

    if (!REQUEST (irq_l_index_name))
	REQUEST (irq_l_index_name) = request;

    if (X.RDB$INDEX_INACTIVE == 0)
	*status = MET_object_active;
    else
	*status = MET_object_inactive;

    id = X.RDB$INDEX_ID - 1;
    relation = MET_lookup_relation (tdbb, X.RDB$RELATION_NAME);
    *relation_id = relation->rel_id;

END_FOR;

if (!REQUEST (irq_l_index_name))
    REQUEST (irq_l_index_name) = request;

return id;
}

int MET_lookup_partner (
    TDBB	tdbb,
    REL         relation,
    IDX         *idx,
    UCHAR        *index_name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ p a r t n e r
 *
 **************************************
 *
 * Functional description
 *      Find partner index participating in a
 *      foreign key relationship.
 *
 **************************************/
DBB     dbb;
BLK     request;
REL     partner_relation;
int     index_number, found;
PRIM    dependencies;
FRGN    references;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

if (relation->rel_flags & REL_check_partners)
    {
    /* Prepare for rescan of foreign references on other relations'
       primary keys and release stale vectors. */
       
    request = (BLK) CMP_find_request (tdbb, irq_foreign1, IRQ_REQUESTS);
    references = &relation->rel_foreign_refs;
    index_number = 0;

    if (references->frgn_reference_ids)
	{
	ALL_release (references->frgn_reference_ids);
	references->frgn_reference_ids = (VEC) NULL_PTR;
	}
    if (references->frgn_relations)
	{
	ALL_release (references->frgn_relations);
	references->frgn_relations = (VEC) NULL_PTR;
	}
    if (references->frgn_indexes)
	{
	ALL_release (references->frgn_indexes);
	references->frgn_indexes = (VEC) NULL_PTR;
	}
	
    FOR (REQUEST_HANDLE request) 
	IDX IN RDB$INDICES CROSS
	IND IN RDB$INDICES WITH
	IDX.RDB$INDEX_NAME STARTING WITH "RDB$FOREIGN" AND
	IDX.RDB$RELATION_NAME EQ relation->rel_name AND
	IND.RDB$INDEX_NAME EQ IDX.RDB$FOREIGN_KEY AND
	IND.RDB$UNIQUE_FLAG NOT MISSING
				    
	if (!REQUEST (irq_foreign1))
	    REQUEST (irq_foreign1) = request;

	if (partner_relation = MET_lookup_relation (tdbb, IND.RDB$RELATION_NAME))
	    {
	    ALL_vector (dbb->dbb_permanent, &references->frgn_reference_ids, index_number);
	    references->frgn_reference_ids->vec_object [index_number] = (BLK) (IDX.RDB$INDEX_ID - 1);
	    ALL_vector (dbb->dbb_permanent, &references->frgn_relations, index_number);
	    references->frgn_relations->vec_object [index_number] = (BLK) partner_relation->rel_id;
	    ALL_vector (dbb->dbb_permanent, &references->frgn_indexes, index_number);
	    references->frgn_indexes->vec_object [index_number] = (BLK) (IND.RDB$INDEX_ID - 1);
		
	    index_number++;
	    }
    END_FOR;

    if (!REQUEST (irq_foreign1))
	REQUEST (irq_foreign1) = request;

    /* Prepare for rescan of primary dependencies on relation's primary
       key and stale vectors. */
       
    request = (BLK) CMP_find_request (tdbb, irq_foreign2, IRQ_REQUESTS);
    dependencies = &relation->rel_primary_dpnds;
    index_number = 0;
    
    if (dependencies->prim_reference_ids)
	{
	ALL_release (dependencies->prim_reference_ids);
	dependencies->prim_reference_ids = (VEC) NULL_PTR;
	}
    if (dependencies->prim_relations)
	{
	ALL_release (dependencies->prim_relations);
	dependencies->prim_relations = (VEC) NULL_PTR;
	}
    if (dependencies->prim_indexes)
	{
	ALL_release (dependencies->prim_indexes);
	dependencies->prim_indexes = (VEC) NULL_PTR;
	}
    
/*
** ============================================================
** ==
** == since UNIQUE constraint also could be used as primary key
** == therefore we change:
** ==
** ==    IDX.RDB$INDEX_NAME STARTING WITH "RDB$PRIMARY"
** ==
** == to
** ==
** ==    IDX.RDB$UNIQUE_FLAG = 1
** ==
** ============================================================
*/

    FOR (REQUEST_HANDLE request)              
	IDX IN RDB$INDICES CROSS
	IND IN RDB$INDICES WITH
	IDX.RDB$UNIQUE_FLAG = 1 AND
	IDX.RDB$RELATION_NAME EQ relation->rel_name AND
	IND.RDB$FOREIGN_KEY EQ IDX.RDB$INDEX_NAME
				    
	if (!REQUEST (irq_foreign2))
	    REQUEST (irq_foreign2) = request;

	if (partner_relation = MET_lookup_relation (tdbb, IND.RDB$RELATION_NAME))
	    {
	    ALL_vector (dbb->dbb_permanent, &dependencies->prim_reference_ids, index_number);
	    dependencies->prim_reference_ids->vec_object [index_number] = (BLK) (IDX.RDB$INDEX_ID - 1);
	    ALL_vector (dbb->dbb_permanent, &dependencies->prim_relations, index_number);
	    dependencies->prim_relations->vec_object [index_number] = (BLK) partner_relation->rel_id;
	    ALL_vector (dbb->dbb_permanent, &dependencies->prim_indexes, index_number);
	    dependencies->prim_indexes->vec_object [index_number] = (BLK) (IND.RDB$INDEX_ID - 1);

	    index_number++;
	    }
    END_FOR;

    if (!REQUEST (irq_foreign2))
	REQUEST (irq_foreign2) = request;
	
    relation->rel_flags &= ~REL_check_partners;
    }

if (idx->idx_flags & idx_foreign)
    {
    if (*index_name)
	{
	/* Since primary key index names aren't being cached, do a long
	   hard lookup. This is only called during index create for foreign
	   keys. */
	   
	found = FALSE;
	request = NULL_PTR;
	   
	FOR (REQUEST_HANDLE request) 
	    IDX IN RDB$INDICES CROSS
	    IND IN RDB$INDICES WITH
	    IDX.RDB$RELATION_NAME EQ relation->rel_name AND
	    (IDX.RDB$INDEX_ID EQ idx->idx_id + 1 OR
	      IDX.RDB$INDEX_NAME EQ index_name)       AND
	    IND.RDB$INDEX_NAME EQ IDX.RDB$FOREIGN_KEY AND
	    IND.RDB$UNIQUE_FLAG NOT MISSING
				    
	if (partner_relation = MET_lookup_relation (tdbb, IND.RDB$RELATION_NAME))
	    {
	    idx->idx_primary_relation = partner_relation->rel_id;
	    idx->idx_primary_index = IND.RDB$INDEX_ID - 1;
	    found = TRUE;
	    }
	END_FOR;
    
	CMP_release (tdbb, request);
	return found;
	}
	
    references = &relation->rel_foreign_refs;
    if (references->frgn_reference_ids)
	for (index_number = 0; index_number < references->frgn_reference_ids->vec_count; index_number++)
	    if (idx->idx_id == (UCHAR) references->frgn_reference_ids->vec_object [index_number])
		{
		idx->idx_primary_relation = (USHORT) references->frgn_relations->vec_object [index_number];
		idx->idx_primary_index = (UCHAR) references->frgn_indexes->vec_object [index_number];
		return TRUE;
		}
    return FALSE;
    }
else if (idx->idx_flags & (idx_primary | idx_unique))
    {
    dependencies = &relation->rel_primary_dpnds;
    if (dependencies->prim_reference_ids)
	for (index_number = 0; index_number < dependencies->prim_reference_ids->vec_count; index_number++)
	    if (idx->idx_id == (UCHAR) dependencies->prim_reference_ids->vec_object [index_number])
		{
		idx->idx_foreign_primaries = relation->rel_primary_dpnds.prim_reference_ids;
		idx->idx_foreign_relations = relation->rel_primary_dpnds.prim_relations;
		idx->idx_foreign_indexes = relation->rel_primary_dpnds.prim_indexes;
		return TRUE;
		}
    return FALSE;
    }

return FALSE;
}

PRC MET_lookup_procedure (
    TDBB	tdbb,
    SCHAR       *name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *      Lookup procedure by name.  Name passed in is
 *      ASCIZ name.
 *
 **************************************/
DBB     dbb;
BLK     request;
PRC     procedure, *ptr, *end;
VEC     procedures;
SCHAR   *p, *q;
 
SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;
 
/* See if we already know the relation by name */
 
if (procedures = dbb->dbb_procedures)
    for (ptr = (PRC*) procedures->vec_object, end = ptr + procedures->vec_count; ptr < end; ptr++)
	{
	if ((procedure = *ptr) &&
	    !(procedure->prc_flags & PRC_obsolete) &&
	    (procedure->prc_flags & PRC_scanned) &&
	    !(procedure->prc_flags & PRC_being_scanned) &&
	    !(procedure->prc_flags & PRC_being_altered) &&
	    (p = procedure->prc_name->str_data))
	    for (q = name; *p == *q; p++, q++)
		if (*p == 0)
		    return procedure;
	}

/* We need to look up the procedure name in RDB$PROCEDURES */

procedure = NULL;

request = (BLK) CMP_find_request (tdbb, irq_l_procedure, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_NAME EQ name

    if (!REQUEST (irq_l_procedure))
	REQUEST (irq_l_procedure) = request;
    procedure = MET_procedure (tdbb, P.RDB$PROCEDURE_ID, 0);

END_FOR;

if (!REQUEST (irq_l_procedure))
    REQUEST (irq_l_procedure) = request;

return procedure;
}

PRC MET_lookup_procedure_id (
    TDBB	tdbb,
    SSHORT      id,
    BOOLEAN     return_deleted,
    USHORT	flags)
{
/**************************************
 *
 *      M E T _ l o o k u p _ p r o c e d u r e _ i d
 *
 **************************************
 *
 * Functional description
 *      Lookup procedure by id.
 *
 **************************************/
DBB     dbb;
BLK     request;
PRC     procedure, *ptr, *end;
VEC     procedures;
 
SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;
 
/* See if we already know the relation by name */
 
if (procedures = dbb->dbb_procedures)
    for (ptr = (PRC*) procedures->vec_object, end = ptr + procedures->vec_count; ptr < end; ptr++)
	{
	if ((procedure = *ptr) &&
	    procedure->prc_id == id &&
	    !(procedure->prc_flags & PRC_being_scanned) &&
	    (procedure->prc_flags & PRC_scanned) &&
	    !(procedure->prc_flags & PRC_being_altered) &&
	    (!(procedure->prc_flags & PRC_obsolete) || return_deleted))
	    return procedure;
	}

/* We need to look up the procedure name in RDB$PROCEDURES */

procedure = NULL;

request = (BLK) CMP_find_request (tdbb, irq_l_proc_id, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_ID EQ id

    if (!REQUEST (irq_l_proc_id))
	REQUEST (irq_l_proc_id) = request;
    procedure = MET_procedure (tdbb, P.RDB$PROCEDURE_ID, flags);

END_FOR;

if (!REQUEST (irq_l_proc_id))
    REQUEST (irq_l_proc_id) = request;

return procedure;
}

REL MET_lookup_relation (
    TDBB	tdbb,
    SCHAR       *name)
{
/**************************************
 *
 *      M E T _ l o o k u p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Lookup relation by name.  Name passed in is
 *      ASCIZ name.
 *
 **************************************/
DBB     dbb;
BLK     request;
VEC     relations;
REL     relation, check_relation, *ptr, *end;
SCHAR   *p, *q;
UCHAR	length;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

/* See if we already know the relation by name */

relations = dbb->dbb_relations;
check_relation = NULL;
length = strlen (name);

for (ptr = (REL*) relations->vec_object, end = ptr + relations->vec_count; ptr < end; ptr++)
    {
    if ((relation = *ptr) &&
	(relation->rel_length == length) &&
	(!(relation->rel_flags & REL_deleted)) &&
	(p = relation->rel_name))
	for (q = name; *p == *q; p++, q++)
	    if (*p == 0)
		if (relation->rel_flags & REL_check_existence)
		    {
		    check_relation = relation;
		    LCK_lock (tdbb, check_relation->rel_existence_lock, LCK_SR, TRUE);
		    break;
		    }
		else
		    return relation;
    if (check_relation)
	break;
    }

/* We need to look up the relation name in RDB$RELATIONS */

relation = NULL;

request = (BLK) CMP_find_request (tdbb, irq_l_relation, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ name
    if (!REQUEST (irq_l_relation))
	REQUEST (irq_l_relation) = request;
    relation = MET_relation (tdbb, X.RDB$RELATION_ID);
    if (!relation->rel_name)
	{
	relation->rel_name = MET_save_name (tdbb, name);
	relation->rel_length = strlen (relation->rel_name);
	}
END_FOR;

if (!REQUEST (irq_l_relation))
    REQUEST (irq_l_relation) = request;

if (check_relation)
    {
    check_relation->rel_flags &= ~REL_check_existence;
    if (check_relation != relation)
	{
	LCK_release (tdbb, check_relation->rel_existence_lock);
	check_relation->rel_flags |= REL_deleted;
	}
    }

return relation;
}

REL MET_lookup_relation_id (
    TDBB	tdbb,
    SLONG       id,
    BOOLEAN     return_deleted)
{
/**************************************
 *
 *      M E T _ l o o k u p _ r e l a t i o n _ i d
 *
 **************************************
 *
 * Functional description
 *      Lookup relation by id. Make sure it really exists.
 *
 **************************************/
DBB             dbb;
register REL    relation, check_relation;
register VEC    vector;
BLK             request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

/* System relations are above suspicion */

if (id < (int) rel_MAX)
    return MET_relation (tdbb, (int)id);

check_relation = NULL;

if ((vector = dbb->dbb_relations) &&
    (id < vector->vec_count) &&
    (relation = (REL) vector->vec_object[id]))
    {
    if (relation->rel_flags & REL_deleted)
	{
	if (return_deleted)
	    return relation;
	return NULL;
	}
    else if (relation->rel_flags & REL_check_existence)
	{
	check_relation = relation;
	LCK_lock (tdbb, check_relation->rel_existence_lock, LCK_SR, TRUE);
	}
    else
	return relation;
     }

/* We need to look up the relation id in RDB$RELATIONS */

relation = NULL;

request = (BLK) CMP_find_request (tdbb, irq_l_rel_id, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    X IN RDB$RELATIONS WITH X.RDB$RELATION_ID EQ id
    if (!REQUEST (irq_l_rel_id))
	REQUEST (irq_l_rel_id) = request;
    relation = MET_relation (tdbb, X.RDB$RELATION_ID);
    if (!relation->rel_name)
	{
	relation->rel_name = MET_save_name (tdbb, X.RDB$RELATION_NAME);
	relation->rel_length = strlen (relation->rel_name);
	}
END_FOR;

if (!REQUEST (irq_l_rel_id))
    REQUEST (irq_l_rel_id) = request;

if (check_relation)
    {
    check_relation->rel_flags &= ~REL_check_existence;
    if (check_relation != relation)
	{
	LCK_release (tdbb, check_relation->rel_existence_lock);
	check_relation->rel_flags |= REL_deleted;
	}
    }

return relation;
}

NOD MET_parse_blob (
    TDBB	tdbb,
    REL         relation,
    SLONG       blob_id [2],
    CSB         *csb_ptr,
    REQ         *request_ptr,
    BOOLEAN     trigger,
    BOOLEAN	ignore_perm)
{
/**************************************
 *
 *      M E T _ p a r s e _ b l o b
 *
 **************************************
 *
 * Functional description
 *      Parse blr, returning a compiler scratch block with the results.
 *
 * if ignore_perm is true then, the request generated must be set to
 *   ignore all permissions checks. In this case, we call PAR_blr
 *   passing it the csb_ignore_perm flag to generate a request
 *   which must go through without checking any permissions.
 *
 **************************************/
DBB     dbb;
BLB     blob;
NOD     node;
STR     temp;
SLONG   length;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

blob = BLB_open (tdbb, dbb->dbb_sys_trans, blob_id);
length = blob->blb_length + 10;
temp = (STR) ALLOCDV (type_str, length);
BLB_get_data (tdbb, blob, temp->str_data, length);
node = PAR_blr (tdbb, relation, temp->str_data, NULL_PTR, csb_ptr, request_ptr,
		trigger, ignore_perm ? csb_ignore_perm: 0);
ALL_release (temp);

return node;
}

NOD MET_parse_sys_trigger (
    TDBB	tdbb,
    REL         relation)
{
/**************************************
 *
 *      M E T _ p a r s e _ s y s _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *      Parse the blr for a system relation's triggers.
 *
 **************************************/
DBB	dbb;
BLK     *trigger;
UCHAR   *blr, type;
TEXT    *name;
VEC     *ptr;
REQ     request;
PLB     old_pool;
USHORT  trig_flags;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

relation->rel_flags &= ~REL_sys_triggers;

/* release any triggers in case of a rescan */

if (relation->rel_pre_store)
    MET_release_triggers (tdbb, &relation->rel_pre_store);
if (relation->rel_post_store)
    MET_release_triggers (tdbb, &relation->rel_post_store);
if (relation->rel_pre_erase)
    MET_release_triggers (tdbb, &relation->rel_pre_erase);
if (relation->rel_post_erase)
    MET_release_triggers (tdbb, &relation->rel_post_erase);
if (relation->rel_pre_modify)
    MET_release_triggers (tdbb, &relation->rel_pre_modify);
if (relation->rel_post_modify)
    MET_release_triggers (tdbb, &relation->rel_post_modify);

#ifdef READONLY_DATABASE
/* No need to load triggers for ReadOnly databases, since
   INSERT/DELETE/UPDATE statements are not going to be allowed */

if (dbb->dbb_flags & DBB_read_only)
    return;
#endif  /* READONLY_DATABASE */

relation->rel_flags |= REL_sys_trigs_being_loaded;

trigger = NULL;
while (trigger = (BLK*) INI_lookup_sys_trigger (relation, trigger, &blr, &type, &name, &trig_flags))
    {
    switch (type)
	{
	case 1: ptr = &relation->rel_pre_store; break;
	case 2: ptr = &relation->rel_post_store; break;
	case 3: ptr = &relation->rel_pre_modify; break;
	case 4: ptr = &relation->rel_post_modify; break;
	case 5: ptr = &relation->rel_pre_erase; break;
	case 6: ptr = &relation->rel_post_erase; break;
	default:        ptr = NULL; break;
	}

    if (ptr)
	{
	old_pool = tdbb->tdbb_default;
	tdbb->tdbb_default = ALL_pool();
	PAR_blr (tdbb, relation, blr, NULL_PTR, NULL_PTR, &request, TRUE,
            (trig_flags & TRG_ignore_perm) ? csb_ignore_perm: 0);
	tdbb->tdbb_default = old_pool;

	request->req_trg_name = name;

	request->req_flags |= req_sys_trigger;
	if (trig_flags & TRG_ignore_perm)
	    request->req_flags |= req_ignore_perm;

	save_trigger_request (dbb, ptr, request);
	}
    }

relation->rel_flags &= ~REL_sys_trigs_being_loaded;
}

int MET_post_existence (
    TDBB	tdbb,
    REL         relation)
{
/**************************************
 *
 *      M E T _ p o s t _ e x i s t e n c e
 *
 **************************************
 *
 * Functional description
 *      Post an interest in the existence of a relation.
 *
 **************************************/

SET_TDBB (tdbb);

if (++relation->rel_use_count == 1 &&
    !MET_lookup_relation_id (tdbb, relation->rel_id, FALSE))
	return FALSE;

return TRUE;
}

void MET_prepare (
    TDBB	tdbb,
    TRA         transaction,
    USHORT      length,
    UCHAR       *msg)
{
/**************************************
 *
 *      M E T _ p r e p a r e 
 *
 **************************************
 *
 * Functional description
 *      Post a transaction description to RDB$TRANSACTIONS.
 *
 **************************************/
DBB     dbb;
BLK     request;
BLB     blob;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_s_trans, IRQ_REQUESTS);

STORE (REQUEST_HANDLE request) X IN RDB$TRANSACTIONS
    X.RDB$TRANSACTION_ID = transaction->tra_number;
    X.RDB$TRANSACTION_STATE = RDB$TRANSACTIONS.RDB$TRANSACTION_STATE.LIMBO;
    blob = BLB_create (tdbb, dbb->dbb_sys_trans, &X.RDB$TRANSACTION_DESCRIPTION);
    BLB_put_segment (tdbb, blob, msg, length);
    BLB_close (tdbb, blob);
END_STORE;

if (!REQUEST (irq_s_trans))
    REQUEST (irq_s_trans) = request;
}

PRC MET_procedure (
    TDBB	tdbb,
    int         id,
    USHORT      flags)
{
/**************************************
 *
 *      M E T _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *      Find or create a procedure block for a given procedure id.
 *
 **************************************/
DBB     dbb;
PRC     procedure;
VEC     vector;
LCK     lock;
BLK     request, request2, *ptr, *end;
CSB     csb;
PRM     parameter;
PLB     old_pool;
NOD     node;
FMT     format;
DSC     *desc;
SSHORT  i;
USHORT  length;
JMP_BUF env, *old_env;


SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

if (!(vector = dbb->dbb_procedures))
    {
    vector = dbb->dbb_procedures = (VEC) ALLOCPV (type_vec, id + 10);
    vector->vec_count = id + 10;
    }
else if (id >= vector->vec_count)
    vector = (VEC) ALL_extend (&dbb->dbb_procedures, id + 10);

#ifdef SUPERSERVER
if (!(dbb->dbb_flags & DBB_sp_rec_mutex_init))
    {
    THD_rec_mutex_init (&dbb->dbb_sp_rec_mutex);
    dbb->dbb_flags |= DBB_sp_rec_mutex_init;
    }
THREAD_EXIT;
if (THD_rec_mutex_lock (&dbb->dbb_sp_rec_mutex))
    {
    THREAD_ENTER;
    return NULL;
    }
THREAD_ENTER;
#endif /* SUPERSERVER */

if (procedure = (PRC) vector->vec_object [id])
    {
    /* Make sure PRC_being_scanned and PRC_scanned
       are not set at the same time
     */
    assert (!(procedure->prc_flags & PRC_being_scanned) ||
            !(procedure->prc_flags & PRC_scanned));

    /* To avoid scanning recursive procedure's blr recursively let's
       make use of PRC_being_scanned bit. Because this bit is set
       later in the code, it is not set when we are here first time.
       If (in case of rec. procedure) we get here second time it is 
       already set and we return half baked procedure.
         In case of superserver this code is under the rec. mutex
       protection, thus the only guy (thread) who can get here and see
       PRC_being_scanned bit set is the guy which started procedure scan
       and currently holds the mutex.
         In case of classic, there is always only one guy and if it 
       sees PRC_being_scanned bit set it is safe to assume it is here
       second time.

       If procedure has already been scanned - return. This condition
       is for those threads that did not find procedure in cach and 
       came here to get it from disk. But only one was able to lock
       the mutex and do the scanning, others were waiting. As soon as
       the first thread releases the mutex another thread gets in and
       it would be just unfair to make it do the work again
    */
    if ((procedure->prc_flags & PRC_being_scanned) ||
	(procedure->prc_flags & PRC_scanned))
        {
#ifdef SUPERSERVER
        THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
        return procedure;
        }
    }

if (!procedure)
    procedure = (PRC) ALLOCP (type_prc);

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    procedure->prc_flags &= ~(PRC_being_scanned | PRC_scanned);
#ifdef SUPERSERVER
    THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
    if (procedure->prc_existence_lock)
        LCK_release (tdbb, procedure->prc_existence_lock);
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    ERR_punt ();
    }

procedure->prc_flags |= (PRC_being_scanned|flags);
procedure->prc_id = id;
vector->vec_object [id] = (BLK) procedure;

procedure->prc_existence_lock = lock = (LCK) ALLOCPV (type_lck, 0);
lock->lck_parent = dbb->dbb_lock;
lock->lck_dbb = dbb;
lock->lck_key.lck_long = procedure->prc_id;
lock->lck_length = sizeof (lock->lck_key.lck_long);
lock->lck_type = LCK_prc_exist;
lock->lck_owner_handle = LCK_get_owner_handle (tdbb, lock->lck_type);
lock->lck_object = (BLK) procedure;
lock->lck_ast = blocking_ast_procedure;

LCK_lock (tdbb, procedure->prc_existence_lock, LCK_SR, TRUE);

request = (BLK) CMP_find_request (tdbb, irq_r_procedure, IRQ_REQUESTS);
request2 = (BLK) CMP_find_request (tdbb, irq_r_params, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_ID EQ procedure->prc_id

    if (!REQUEST (irq_r_procedure))
	REQUEST (irq_r_procedure) = request;

    if (!procedure->prc_name)
	procedure->prc_name = save_name (tdbb, P.RDB$PROCEDURE_NAME);
    procedure->prc_id = P.RDB$PROCEDURE_ID;
    procedure->prc_use_count = 0;
    if (!P.RDB$SECURITY_CLASS.NULL)
	procedure->prc_security_name = save_name (tdbb, P.RDB$SECURITY_CLASS);
    if (P.RDB$SYSTEM_FLAG.NULL || !P.RDB$SYSTEM_FLAG)
	procedure->prc_flags &= ~PRC_system;
    else
	procedure->prc_flags |= PRC_system;
    if (procedure->prc_inputs = P.RDB$PROCEDURE_INPUTS)
	procedure->prc_input_fields = ALL_vector (dbb->dbb_permanent,
				    &procedure->prc_input_fields,
				    P.RDB$PROCEDURE_INPUTS);
    if (procedure->prc_outputs = P.RDB$PROCEDURE_OUTPUTS)
	procedure->prc_output_fields = ALL_vector (dbb->dbb_permanent,
				    &procedure->prc_output_fields,
				    P.RDB$PROCEDURE_OUTPUTS);

    FOR (REQUEST_HANDLE request2) PA IN RDB$PROCEDURE_PARAMETERS CROSS
      F IN RDB$FIELDS WITH F.RDB$FIELD_NAME = PA.RDB$FIELD_SOURCE
      AND PA.RDB$PROCEDURE_NAME = P.RDB$PROCEDURE_NAME
	if (!REQUEST (irq_r_params))
	    REQUEST (irq_r_params) = request2;
	if (PA.RDB$PARAMETER_TYPE)
	    vector = procedure->prc_output_fields;
	else
	    vector = procedure->prc_input_fields;

	/* should be error if field already exists */
	parameter = (PRM) ALLOCPV (type_prm, name_length (PA.RDB$PARAMETER_NAME));
	parameter->prm_number = PA.RDB$PARAMETER_NUMBER;
	vector->vec_object [parameter->prm_number] = (BLK) parameter;
	name_copy (parameter->prm_string, PA.RDB$PARAMETER_NAME);
	parameter->prm_name = parameter->prm_string;
	DSC_make_descriptor (&parameter->prm_desc, F.RDB$FIELD_TYPE,
		F.RDB$FIELD_SCALE, F.RDB$FIELD_LENGTH, F.RDB$FIELD_SUB_TYPE,
		F.RDB$CHARACTER_SET_ID, F.RDB$COLLATION_ID);
    END_FOR;

    if (!REQUEST (irq_r_params))
	REQUEST (irq_r_params) = request2;

    if ((vector = procedure->prc_output_fields) && vector->vec_object [0])
	{
	format = procedure->prc_format =
			(FMT) ALLOCPV (type_fmt, procedure->prc_outputs);
	format->fmt_count = procedure->prc_outputs;
	length = FLAG_BYTES (format->fmt_count);
	desc = format->fmt_desc;
	for (ptr = vector->vec_object, end = ptr + procedure->prc_outputs;
	     ptr < end; ptr++, desc++)
	    {
	    parameter = (PRM) *ptr;
	    /* check for parameter to be null, this can only happen if the
	     * parameter numbers get out of sync. This was added to fix bug
	     * 10534. -Shaunak Mistry 12-May-99
	     */
	    if (parameter)
	        {
	        *desc = parameter->prm_desc;
	        length = MET_align (desc, length);
	        desc->dsc_address = (UCHAR*) length;
	        length += desc->dsc_length;
		}
	    }
	format->fmt_length = length;
	}
    
    old_pool = tdbb->tdbb_default;
    tdbb->tdbb_default = ALL_pool();
    csb = (CSB) ALLOCDV (type_csb, 5);
    csb->csb_count = 5;
    parse_procedure_blr (tdbb, procedure, &P.RDB$PROCEDURE_BLR, &csb);
    procedure->prc_request->req_procedure = procedure;
    for (i = 0; i < csb->csb_count; i++)
	if (node = csb->csb_rpt[i].csb_message)
	    {
	    if ((int) node->nod_arg [e_msg_number] == 0)
		procedure->prc_input_msg = node;
	    else if ((int) node->nod_arg [e_msg_number] == 1)
		procedure->prc_output_msg = node;
	    }
    ALL_release (csb);
    tdbb->tdbb_default = old_pool;

END_FOR;

if (!REQUEST (irq_r_procedure))
    REQUEST (irq_r_procedure) = request;

/* Make sure that it is really being Scanned ! */
assert (procedure->prc_flags & PRC_being_scanned);

procedure->prc_flags |= PRC_scanned;
procedure->prc_flags &= ~PRC_being_scanned;

#ifdef SUPERSERVER
THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
tdbb->tdbb_setjmp = (UCHAR*) old_env;
return procedure;
}

REL MET_relation (
    TDBB	tdbb,
    USHORT	id)
{
/**************************************
 *
 *      M E T _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Find or create a relation block for a given relation id.
 *
 **************************************/
DBB             dbb;
register REL    relation;
register VEC    vector;
LCK             lock;
USHORT          major_version, minor_original, max_sys_rel;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
CHECK_DBB (dbb);

if (!(vector = dbb->dbb_relations))
    {
    vector = dbb->dbb_relations = (VEC) ALLOCPV (type_vec, id + 10);
    vector->vec_count = id + 10;
    }
else if (id >= vector->vec_count)
    vector = (VEC) ALL_extend (&dbb->dbb_relations, id + 10);

if (relation = (REL) vector->vec_object[id])
    return relation;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

/* From ODS 9 onwards, the first 128 relation IDS have been 
   reserved for system relations */
if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
    max_sys_rel = (USHORT) USER_REL_INIT_ID_ODS8 - 1;
else
    max_sys_rel = (USHORT) USER_DEF_REL_INIT_ID - 1;

relation = (REL) ALLOCP (type_rel);
vector->vec_object [id] = (BLK) relation;
relation->rel_id = id;

if (relation->rel_id <= max_sys_rel)
    return relation;

relation->rel_existence_lock = lock = (LCK) ALLOCPV (type_lck, 0);
lock->lck_parent = dbb->dbb_lock;
lock->lck_dbb = dbb;
lock->lck_key.lck_long = relation->rel_id;
lock->lck_length = sizeof (lock->lck_key.lck_long);
lock->lck_type = LCK_rel_exist;
lock->lck_owner_handle = LCK_get_owner_handle (tdbb, lock->lck_type);
lock->lck_object = (BLK) relation;
lock->lck_ast = blocking_ast_relation;

relation->rel_flags |= (REL_check_existence | REL_check_partners);
return relation;
}

void MET_release_existence (
    REL         relation)
{
/**************************************
 *
 *      M E T _ r e l e a s e _ e x i s t e n c e
 *
 **************************************
 *
 * Functional description
 *      Release interest in relation. If no remaining interest
 *      and we're blocking the drop of the relation then release
 *      existence lock and mark deleted.
 *
 **************************************/

if (!relation->rel_use_count || !--relation->rel_use_count)
    if (relation->rel_flags & REL_blocking)
	LCK_re_post (relation->rel_existence_lock);
}

void MET_remove_procedure (
    TDBB	tdbb,
    int         id,
    PRC         procedure)
{
/**************************************
 *
 *      M E T _ r e m o v e  _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *      Remove a procedure from cache
 *
 **************************************/
DBB     dbb;
VEC     vector;
SSHORT  i;
USHORT  save_proc_flags;
BOOLEAN	free_procedure_block = TRUE;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

if (!procedure)
    {
    free_procedure_block = FALSE;
    /** If we are in here then dfw.e/modify_procedure() called us **/
    if (!(vector = dbb->dbb_procedures))
        return;

    if (!(procedure = (PRC) vector->vec_object [id]))
        return;
    }

/* Procedure that is being altered may have references
   to it by other procedures via pointer to current meta
   data structure, so don't loose the structure or the pointer. 
if (!(procedure->prc_flags & PRC_being_altered))
    vector->vec_object [id] = (BLK) NULL_PTR;
*/

/* deallocate all structure which were allocated.  The procedure
 * blr is originally read into a new pool from which all request
 * are allocated.  That will not be freed up.
 */

if (procedure->prc_existence_lock)
    ALL_release (procedure->prc_existence_lock);

if (procedure->prc_name)
    ALL_release (procedure->prc_name);
if (procedure->prc_security_name)
    ALL_release (procedure->prc_security_name);

/* deallocate input param structures */

if ((procedure->prc_inputs) && (vector = procedure->prc_input_fields))
    {
    for (i = 0; i < procedure->prc_inputs; i++)
	if (vector->vec_object [i])
	    ALL_release (vector->vec_object [i]);
    ALL_release (vector);
    }

/* deallocate output param structures */

if ((procedure->prc_outputs) && (vector = procedure->prc_output_fields))
    {
    for (i = 0; i < procedure->prc_outputs; i++)
	if (vector->vec_object [i])
	    ALL_release (vector->vec_object [i]);
    ALL_release (vector);
    }

if (procedure->prc_format)
    ALL_release (procedure->prc_format);
    
if (!(procedure->prc_flags & PRC_being_altered))
    ALL_release (procedure);
else
    {
    save_proc_flags = procedure->prc_flags;
    memset (((SCHAR *) procedure) + sizeof (struct blk), 0, 
				sizeof (struct prc) - sizeof (struct blk));
    procedure->prc_flags = save_proc_flags;
    }
}

void MET_revoke (
    TDBB	tdbb,
    TRA         transaction,
    TEXT        *relation,
    TEXT        *revokee,
    TEXT        *privilege)
{
/**************************************
 *
 *      M E T _ r e v o k e
 *
 **************************************
 *
 * Functional description
 *      Execute a recursive revoke.  This is called only when
 *      a revoked privilege had the grant option.
 *
 **************************************/
DBB     dbb;
USHORT  count;
BLK     request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

/* See if the revokee still has the privilege.  If so, there's
   nothing to do */

count = 0;

request = (BLK) CMP_find_request (tdbb, irq_revoke1, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE transaction) 
    FIRST 1 P IN RDB$USER_PRIVILEGES WITH
	P.RDB$RELATION_NAME EQ relation AND
	P.RDB$PRIVILEGE EQ privilege AND
	P.RDB$USER EQ revokee
    if (!REQUEST (irq_revoke1))
	REQUEST (irq_revoke1) = request;
    ++count;
END_FOR;

if (!REQUEST (irq_revoke1))
    REQUEST (irq_revoke1) = request;

if (count)
    return;

request = (BLK) CMP_find_request (tdbb, irq_revoke2, IRQ_REQUESTS);

/* User lost privilege.  Take it away from anybody he/she gave
   it to. */

FOR (REQUEST_HANDLE request TRANSACTION_HANDLE transaction)
    P IN RDB$USER_PRIVILEGES WITH
	P.RDB$RELATION_NAME EQ relation AND
	P.RDB$PRIVILEGE EQ privilege AND
	P.RDB$GRANTOR EQ revokee
    if (!REQUEST (irq_revoke2))
	REQUEST (irq_revoke2) = request;
    ERASE P;
END_FOR;

if (!REQUEST (irq_revoke2))
    REQUEST (irq_revoke2) = request;
}

TEXT *MET_save_name (
    TDBB	tdbb,
    TEXT        *name)
{
/**************************************
 *
 *      M E T _ s a v e _ n a m e
 *
 **************************************
 *
 * Functional description
 *      Immortalize a field or relation name in a permant string block.
 *      Oh, wow.
 *
 **************************************/
STR     string;

SET_TDBB (tdbb);

string = save_name (tdbb, name);
return (TEXT*) string->str_data;
}

void MET_scan_relation (
    TDBB	tdbb,
    REL         relation)
{
/**************************************
 *
 *      M E T _ s c a n _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Scan a relation for view rse, computed by expressions, missing
 *      expressions, and validation expressions.
 *
 **************************************/
DBB     dbb;
USHORT  n, length, view_context, field_id;
ARR     array;
VEC     vector, triggers [TRIGGER_MAX], tmp_vector;
FLD     field;
BLK     request;
CSB     csb;
BLB     blob;
UCHAR   *p, *q, *buffer, temp [256];
STR     name, string;
PLB     old_pool;
VOLATILE BOOLEAN dependencies, sys_triggers;
JMP_BUF env, *old_env;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

#ifdef SUPERSERVER
if (!(dbb->dbb_flags & DBB_sp_rec_mutex_init))
    {
    THD_rec_mutex_init (&dbb->dbb_sp_rec_mutex);
    dbb->dbb_flags |= DBB_sp_rec_mutex_init;
    }
THREAD_EXIT;
if (THD_rec_mutex_lock (&dbb->dbb_sp_rec_mutex))
    {
    THREAD_ENTER;
    return;
    }
THREAD_ENTER;
#endif /* SUPERSERVER */

if (relation->rel_flags & REL_scanned || relation->rel_flags & REL_deleted)
    {
#ifdef SUPERSERVER
    THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
    return;
    }

relation->rel_flags |= REL_being_scanned;
#if defined(NETWARE_386) || defined(WINDOWS_ONLY)
if ((unsigned) stackavail () < (unsigned) STACK_SAFE_LIMIT)
    ERR_post (isc_err_stack_limit, 0);
#endif

dependencies = (relation->rel_flags & REL_get_dependencies) ? TRUE : FALSE;
sys_triggers = (relation->rel_flags & REL_sys_triggers) ? TRUE : FALSE;
relation->rel_flags &= ~(REL_get_dependencies | REL_sys_triggers);
for (n = 0; n < TRIGGER_MAX; n++)
    triggers [n] = NULL_PTR;
old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = dbb->dbb_permanent;

/* If anything errors, catch it to reset the scan flag.  This will
   make sure that the error will be caught if the operation is tried
   again. */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    relation->rel_flags &= ~(REL_being_scanned | REL_scanned);
    if (dependencies)
	relation->rel_flags |= REL_get_dependencies;
    if (sys_triggers)
	relation->rel_flags |= REL_sys_triggers;
#ifdef SUPERSERVER
    THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    tdbb->tdbb_default = old_pool;
    ERR_punt ();
    }

/* Since this can be called recursively, find an inactive clone of the request */

request = (BLK) CMP_find_request (tdbb, irq_r_fields, IRQ_REQUESTS);
csb = NULL;

FOR (REQUEST_HANDLE request) 
	REL IN RDB$RELATIONS
	WITH REL.RDB$RELATION_ID EQ relation->rel_id 

    /* Pick up relation level stuff */

    if (!REQUEST (irq_r_fields))
	REQUEST (irq_r_fields) = request;
    relation->rel_current_fmt = REL.RDB$FORMAT;
    vector = ALL_vector (dbb->dbb_permanent, &relation->rel_fields, REL.RDB$FIELD_ID);  
    if (!REL.RDB$SECURITY_CLASS.NULL)
	relation->rel_security_name = MET_save_name (tdbb, REL.RDB$SECURITY_CLASS);
    if (!relation->rel_name)
	{
	relation->rel_name = MET_save_name (tdbb, REL.RDB$RELATION_NAME);
	relation->rel_length = strlen (relation->rel_name);
	}
    if (!relation->rel_owner_name)
	relation->rel_owner_name = MET_save_name (tdbb, REL.RDB$OWNER_NAME);
    if (REL.RDB$FLAGS & REL_sql)
	relation->rel_flags |= REL_sql_relation;
    if (!NULL_BLOB (REL.RDB$VIEW_BLR))
	{
	/* parse the view blr, getting dependencies on relations, etc. at the same time */

	relation->rel_view_rse = (dependencies) ?
	    (RSE) MET_get_dependencies (tdbb, relation, 
			NULL_PTR, 
			NULL_PTR, 
			&REL.RDB$VIEW_BLR, 
			NULL_PTR, 
			&csb, 
			REL.RDB$RELATION_NAME, 
			obj_view) :
	    (RSE) MET_parse_blob (tdbb, relation, &REL.RDB$VIEW_BLR, &csb, NULL_PTR,
			FALSE, FALSE);


	/* retrieve the view context names */
 
	lookup_view_contexts (tdbb, relation);
	}

    relation->rel_flags |= REL_scanned;
    if (REL.RDB$EXTERNAL_FILE [0])
	EXT_file (relation, REL.RDB$EXTERNAL_FILE, &REL.RDB$EXTERNAL_DESCRIPTION);

    /* Pick up field specific stuff */

    blob = BLB_open (tdbb, dbb->dbb_sys_trans, &REL.RDB$RUNTIME);
    if (blob->blb_max_segment < sizeof (temp))
	buffer = temp;
    else
	{
	string = (STR) ALLOCDV (type_str, blob->blb_max_segment);
	buffer = string->str_data;
	}

    field = NULL;
    for (;;)
	{
	length = BLB_get_segment (tdbb, blob, buffer, blob->blb_max_segment);
	if (blob->blb_flags & BLB_eof)
	    break;
	buffer [length] = 0;
	for (p = (UCHAR*) &n, q = buffer + 1; q < buffer + 1 + sizeof (SSHORT);)
	    *p++ = *q++;
	p = buffer + 1;
	--length;
	switch ((RSR_T) buffer [0])
	    {
	    case RSR_field_id:
		if (field && !field->fld_security_name && !REL.RDB$DEFAULT_CLASS.NULL)
		    field->fld_security_name = MET_save_name (tdbb, REL.RDB$DEFAULT_CLASS);
		field_id = n;
		field = (FLD) vector->vec_object [field_id];
		array = NULL;
		break;
	    
	    case RSR_field_name:
		if (field)
		    {
		    /* The field exists.  If its name hasn't changed, then
		       there's no need to copy anything. */

		    if (!strcmp (p, field->fld_name))
			break;

		    name = (STR) ALLOCPV (type_str, length);
		    field->fld_name = (TEXT*) name->str_data;
		    }
		else
		    {
		    field = (FLD) ALLOCPV (type_fld, length);
		    vector->vec_object [field_id] = (BLK) field;
		    field->fld_name = (TEXT*) field->fld_string;
		    }
		strcpy (field->fld_name, p);
		field->fld_length = strlen (field->fld_name);
		break;
	    
	    case RSR_view_context:
		view_context = n;
		break;
	    
	    case RSR_base_field:
		field->fld_source = PAR_make_field (tdbb, csb, view_context, p);
		break;
	    
	    case RSR_computed_blr:
		field->fld_computation = (dependencies) ?
		    MET_get_dependencies (tdbb, relation, 
			p, 
			csb, 
			NULL_PTR, 
			NULL_PTR, 
			NULL_PTR,
			field->fld_name,
			obj_computed) :
		    PAR_blr (tdbb, relation, p, csb, NULL_PTR, NULL_PTR, FALSE, 0);
		break;

	    case RSR_missing_value:
		field->fld_missing_value = PAR_blr (tdbb, relation, p, csb, NULL_PTR, NULL_PTR, FALSE, 0);
		break;

	    case RSR_default_value:
		field->fld_default_value = PAR_blr (tdbb, relation, p, csb, NULL_PTR, NULL_PTR, FALSE, 0);
		break;

	    case RSR_validation_blr:
		field->fld_validation = PAR_blr (tdbb, relation, p, csb, NULL_PTR, NULL_PTR, FALSE, csb_validation);
		break;
	   
	   case RSR_field_not_null:
		field->fld_not_null = PAR_blr (tdbb, relation, p, csb, NULL_PTR,
			   NULL_PTR, FALSE, csb_validation);
		break;

	    case RSR_security_class:
		field->fld_security_name = MET_save_name (tdbb, p);
		break;

	    case RSR_trigger_name:
		MET_load_trigger (tdbb, relation, p, triggers);
		break;

	    case RSR_dimensions:
		field->fld_array = array = (ARR) ALLOCPV (type_arr, n);
		array->arr_desc.ads_dimensions = n;
		break;
		
	    case RSR_array_desc:
		if (array)
		    MOVE_FAST (p, &array->arr_desc, length);
		break;
	    }
	}
    if (field && !field->fld_security_name && !REL.RDB$DEFAULT_CLASS.NULL)
	field->fld_security_name = MET_save_name (tdbb, REL.RDB$DEFAULT_CLASS);
    if (buffer != temp)
	ALL_release (string);
END_FOR;

if (csb)
    ALL_release (csb);

/* release any triggers in case of a rescan, but not if the rescan 
   hapenned while system triggers were being loaded. */

if (!(relation->rel_flags & REL_sys_trigs_being_loaded))
    {
    /* if we are scanning a system relation during loading the system 
       triggers, (during parsing its blr actually), we must not release the
       existing system triggers; because we have already set the
       relation->rel_flag to not have REL_sys_trig, so these
       system triggers will not get loaded again. This fixes bug 8149. */

    /* We have just loaded the triggers onto the local vector triggers.
       Its now time to place them at their rightful place ie the relation
       block. 
    */
    tmp_vector = relation->rel_pre_store;
    relation->rel_pre_store = triggers [TRIGGER_PRE_STORE];
    MET_release_triggers (tdbb, &tmp_vector);

    tmp_vector = relation->rel_post_store;
    relation->rel_post_store = triggers [TRIGGER_POST_STORE];
    MET_release_triggers (tdbb, &tmp_vector);

    tmp_vector = relation->rel_pre_erase;
    relation->rel_pre_erase = triggers [TRIGGER_PRE_ERASE];
    MET_release_triggers (tdbb, &tmp_vector);

    tmp_vector = relation->rel_post_erase;
    relation->rel_post_erase = triggers [TRIGGER_POST_ERASE];
    MET_release_triggers (tdbb, &tmp_vector);

    tmp_vector = relation->rel_pre_modify;
    relation->rel_pre_modify = triggers [TRIGGER_PRE_MODIFY];
    MET_release_triggers (tdbb, &tmp_vector);

    tmp_vector = relation->rel_post_modify;
    relation->rel_post_modify = triggers [TRIGGER_POST_MODIFY];
    MET_release_triggers (tdbb, &tmp_vector);
    }
		     
relation->rel_flags &= ~REL_being_scanned;
#ifdef SUPERSERVER
THD_rec_mutex_unlock (&dbb->dbb_sp_rec_mutex);
#endif
relation->rel_current_format = NULL;
tdbb->tdbb_default = old_pool;
tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

TEXT *MET_trigger_msg (
    TDBB	tdbb,
    TEXT        *name,
    USHORT      number)
{
/**************************************
 *
 *      M E T _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *      Look up trigger message using trigger and abort code.
 *
 **************************************/
DBB     dbb;
BLK     request;
TEXT    *msg;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

msg = NULL;
request = (BLK) CMP_find_request (tdbb, irq_s_msgs, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
	MSG IN RDB$TRIGGER_MESSAGES WITH 
	MSG.RDB$TRIGGER_NAME EQ name AND 
	MSG.RDB$MESSAGE_NUMBER EQ number
    if (!REQUEST (irq_s_msgs))
	REQUEST (irq_s_msgs) = request;
    msg = ERR_cstring (MSG.RDB$MESSAGE);
END_FOR;

if (!REQUEST (irq_s_msgs))
    REQUEST (irq_s_msgs) = request;

return msg;
}

void MET_update_shadow (
    TDBB	tdbb,
    SDW         shadow,
    USHORT      file_flags)
{
/**************************************
 *
 *      M E T _ u p d a t e _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *      Update the stored file flags for the specified shadow.
 *
 **************************************/
DBB     dbb;
BLK     handle;
 
SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

handle = NULL;
FOR (REQUEST_HANDLE handle)
    FIL IN RDB$FILES WITH FIL.RDB$SHADOW_NUMBER EQ shadow->sdw_number
    MODIFY FIL USING
	FIL.RDB$FILE_FLAGS = file_flags;
    END_MODIFY;
END_FOR;
CMP_release (tdbb, handle);
}

void MET_update_transaction (
    TDBB	tdbb,
    TRA         transaction,
    USHORT      flag)
{
/**************************************
 *
 *      M E T _ u p d a t e _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *      Update a record in RDB$TRANSACTIONS.  If flag is TRUE, this is a
 *      commit; otherwise it is a ROLLBACK.
 *
 **************************************/
DBB     dbb;
BLK     request;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_m_trans, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
	X IN RDB$TRANSACTIONS WITH X.RDB$TRANSACTION_ID EQ transaction->tra_number
    if (!REQUEST (irq_m_trans))
	REQUEST (irq_m_trans) = request;
    if (flag && (transaction->tra_flags & TRA_prepare2))
	{
	ERASE X
	}
    else
	{
	MODIFY X
	    X.RDB$TRANSACTION_STATE = (flag) ?
		RDB$TRANSACTIONS.RDB$TRANSACTION_STATE.COMMITTED :
		RDB$TRANSACTIONS.RDB$TRANSACTION_STATE.ROLLED_BACK;
	END_MODIFY;
	}
END_FOR;

if (!REQUEST (irq_m_trans))
    REQUEST (irq_m_trans) = request;
}

static void blocking_ast_procedure (
    PRC procedure)
{
/**************************************
 *
 *      b l o c k i n g _ a s t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *      Someone is trying to drop a proceedure. If there
 *      are outstanding interests in the existence of
 *      the relation then just mark as blocking and return.
 *      Otherwise, mark the procedure block as questionable
 *      and release the procedure existence lock.
 *
 **************************************/
struct tdbb     thd_context, *tdbb;

/* Since this routine will be called asynchronously, we must establish
   a thread context. */

SET_THREAD_DATA;

tdbb->tdbb_database = procedure->prc_existence_lock->lck_dbb;
tdbb->tdbb_attachment = procedure->prc_existence_lock->lck_attachment;
tdbb->tdbb_quantum = QUANTUM;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_default = NULL;

if (procedure->prc_existence_lock)
    LCK_release (tdbb, procedure->prc_existence_lock);
procedure->prc_flags |= PRC_obsolete;

/* Restore the prior thread context */

RESTORE_THREAD_DATA;
}

static void blocking_ast_relation (
    REL         relation)
{
/**************************************
 *
 *      b l o c k i n g _ a s t _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Someone is trying to drop a relation. If there
 *      are outstanding interests in the existence of
 *      the relation then just mark as blocking and return.
 *      Otherwise, mark the relation block as questionable
 *      and release the relation existence lock.
 *
 **************************************/
struct tdbb     thd_context, *tdbb;

/* Since this routine will be called asynchronously, we must establish
   a thread context. */

SET_THREAD_DATA;

tdbb->tdbb_database = relation->rel_existence_lock->lck_dbb;
tdbb->tdbb_attachment = relation->rel_existence_lock->lck_attachment;
tdbb->tdbb_quantum = QUANTUM;
tdbb->tdbb_request = NULL;
tdbb->tdbb_transaction = NULL;
tdbb->tdbb_default = NULL;

if (relation->rel_use_count)
    relation->rel_flags |= REL_blocking;
else
    {
    relation->rel_flags &= ~REL_blocking;
    relation->rel_flags |= (REL_check_existence | REL_check_partners);
    if (relation->rel_existence_lock)
	LCK_release (tdbb, relation->rel_existence_lock);
    }

/* Restore the prior thread context */

RESTORE_THREAD_DATA;
}

static void get_trigger (
    TDBB	tdbb,
    REL         relation,
    SLONG       blob_id [2],
    VEC         *ptr,
    TEXT        *name,
    BOOLEAN     sys_trigger,
    USHORT      flags)
{
/**************************************
 *
 *      g e t _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *      Get trigger.
 *
 **************************************/
REQ     request;
PLB     old_pool;

SET_TDBB (tdbb);

if (!blob_id [0] && !blob_id [1])
    return;

old_pool = tdbb->tdbb_default;
tdbb->tdbb_default = ALL_pool();
MET_parse_blob (tdbb, relation, blob_id, NULL_PTR, &request, TRUE,
     (flags & TRG_ignore_perm) ? TRUE : FALSE);
tdbb->tdbb_default = old_pool;

if (name)
    request->req_trg_name = MET_save_name (tdbb, name);
if (sys_trigger)
    request->req_flags |= req_sys_trigger;

if (flags & TRG_ignore_perm)
    request->req_flags |= req_ignore_perm;

save_trigger_request (tdbb->tdbb_database, ptr, request);
}

static BOOLEAN get_type (
    TDBB	tdbb,
    SSHORT      *id,
    UCHAR       *name,
    UCHAR       *field)
{
/**************************************
 *
 *      g e t _ t y p e
 *
 **************************************
 *
 * Functional description
 *      Resoved a symbolic name in RDB$TYPES.  Returned the value
 *      defined for the name in (*id).  Don't touch (*id) if you
 *      don't find the name.
 *
 *      Return (1) if found, (0) otherwise.
 *
 **************************************/
UCHAR   buffer [32];    /* BASED ON RDB$TYPE_NAME */
BOOLEAN found;
UCHAR   *p;
BLK     handle;
DBB     dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

assert (id != NULL);
assert (name != NULL);
assert (field != NULL);

/* Force key to uppercase, following C locale rules for uppercase */

for (p = buffer; *name && p < buffer + sizeof (buffer) - 1; p++, name++)
    *p = UPPER7 (*name);
*p = 0;

/* Try for exact name match */

found = FALSE;

handle = NULL;
FOR (REQUEST_HANDLE handle) 
	FIRST 1 T IN RDB$TYPES WITH
	T.RDB$FIELD_NAME EQ field AND
	T.RDB$TYPE_NAME EQ buffer
    found = TRUE;
    *id = T.RDB$TYPE;
END_FOR;
CMP_release (tdbb, handle);

return found;
}

static void lookup_view_contexts (
    TDBB	tdbb,
    REL         view)
{
/**************************************
 *
 *      l o o k u p _ v i e w _ c o n t e x t s
 *
 **************************************
 *
 * Functional description
 *      Lookup view contexts and store in a linked 
 *      list on the relation block.
 *                                       
 **************************************/
DBB     dbb;
BLK     request;
SSHORT  length;
STR     alias;
VCX     view_context, *vcx_ptr;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;
request = (BLK) CMP_find_request (tdbb, irq_view_context, IRQ_REQUESTS);

vcx_ptr = &view->rel_view_contexts;

FOR (REQUEST_HANDLE request) 
    V IN RDB$VIEW_RELATIONS WITH 
    V.RDB$VIEW_NAME EQ view->rel_name
    SORTED BY V.RDB$VIEW_CONTEXT

    if (!REQUEST (irq_view_context))
	REQUEST (irq_view_context) = request;

    /* allocate a view context block and link it in
       to the relation block's linked list */

    view_context = (VCX) ALLOCD (type_vcx);
    *vcx_ptr = view_context;
    vcx_ptr = &view_context->vcx_next;

    view_context->vcx_context = V.RDB$VIEW_CONTEXT;

    /* allocate a string block for the context name */
	       
    length = name_length (V.RDB$CONTEXT_NAME);
    alias = (STR) ALLOCDV (type_str, length + 1);
    V.RDB$CONTEXT_NAME [length] = 0;
    strcpy (alias->str_data, V.RDB$CONTEXT_NAME);
    alias->str_length = length;

    view_context->vcx_context_name = alias;

    /* allocate a string block for the relation name */
	       
    length = name_length (V.RDB$RELATION_NAME);
    alias = (STR) ALLOCDV (type_str, length + 1);
    V.RDB$RELATION_NAME [length] = 0;
    strcpy (alias->str_data, V.RDB$RELATION_NAME);
    alias->str_length = length;

    view_context->vcx_relation_name = alias;

END_FOR;

if (!REQUEST (irq_view_context))
    REQUEST (irq_view_context) = request;
}

static void name_copy (
    TEXT        *to,
    TEXT        *from)
{
/**************************************
 *
 *      n a m e _ c o p y
 *
 **************************************
 *
 * Functional description
 *      Copy a system name, stripping trailing blanks.
 *
 **************************************/
USHORT	length;

length = name_length (from);
memcpy (to, from, length);
to [length] = '\0';
}

static USHORT name_length (
    TEXT        *name)
{
/**************************************
 *
 *      n a m e _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *      Compute effective length of system relation name.
 *      SQL delimited identifier may contain blanks.
 *
 **************************************/
TEXT    *p, *q;

q = name - 1;
for (p = name; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

return (q+1) - name;
}

static NOD parse_procedure_blr (
    TDBB	tdbb,
    PRC         procedure,
    SLONG       blob_id [2],
    CSB         *csb_ptr)
{
/**************************************
 *
 *      p a r s e _ p r o c e d u r e _ b l r
 *
 **************************************
 *
 * Functional description
 *      Parse blr, returning a compiler scratch block with the results.
 *
 **************************************/
DBB     dbb;
BLB     blob;
NOD     node;
STR     temp;
SLONG   length;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

blob = BLB_open (tdbb, dbb->dbb_sys_trans, blob_id);
length = blob->blb_length + 10;
temp = (STR) ALLOCDV (type_str, length);
BLB_get_data (tdbb, blob, temp->str_data, length);
(*csb_ptr)->csb_blr = temp->str_data;
par_messages (tdbb, temp->str_data, (USHORT)blob->blb_length, procedure, csb_ptr);
node = PAR_blr (tdbb, NULL_PTR, temp->str_data, NULL_PTR, csb_ptr, &procedure->prc_request,
		FALSE, 0);
ALL_release (temp);

return node;
}

static BOOLEAN par_messages (
    TDBB	tdbb,
    UCHAR       *blr,
    USHORT      blr_length,
    PRC         procedure,
    CSB         *csb)
{
/**************************************
 *
 *      p a r _ m e s s a g e s
 *
 **************************************
 *
 * Functional description
 *      Parse the messages of a blr request.  For specified message, allocate
 *      a format (fmt) block.
 *
 **************************************/
UCHAR   *end;
FMT     format;
DSC     *desc;
USHORT  count, offset, align, msg_number;
SSHORT  version;

(*csb)->csb_running = blr;
version = BLR_BYTE; 
if (version != blr_version4 && version != blr_version5)
    return FALSE;

if (BLR_BYTE != blr_begin)
    return FALSE;

SET_TDBB (tdbb);

while (BLR_BYTE == blr_message)
    {
    msg_number = BLR_BYTE;
    count = BLR_BYTE;
    count += (BLR_BYTE) << 8;
    format = (FMT) ALLOCDV (type_fmt, count);
    format->fmt_count = count;
    offset = 0;
    for (desc = format->fmt_desc; count; --count, ++desc)
	{
	align = PAR_desc (csb, desc);
	if (align)
	    offset = ALIGN (offset, align);
	desc->dsc_address = (UCHAR*) offset;
	offset += desc->dsc_length;
	}
    format->fmt_length = offset;
    if (msg_number == 0)
	procedure->prc_input_fmt = format;
    else if (msg_number == 1)
	procedure->prc_output_fmt = format;
    else
	ALL_release (format);
    }

return TRUE;
}

void MET_release_triggers (
    TDBB	tdbb,
    VEC         *vector_ptr)
{
/***********************************************
 *
 *      M E T _ r e l e a s e _ t r i g g e r s
 *
 ***********************************************
 *
 * Functional description
 *      Release a possibly null vector of triggers.
 *      If triggers are still active let someone
 *      else do the work.
 *
 **************************************/
VEC     vector;
REQ     *ptr, *end;


if (!(vector = *vector_ptr))
    return;

SET_TDBB (tdbb);

*vector_ptr = NULL;

for (ptr = (REQ*) vector->vec_object, end = ptr + vector->vec_count;
     ptr < end; ptr++)
    if (*ptr && CMP_clone_active (*ptr))
	return;

for (ptr = (REQ*) vector->vec_object, end = ptr + vector->vec_count;
     ptr < end; ptr++)
    if (*ptr)
	CMP_release (tdbb, *ptr);

ALL_release (vector);
}

static BOOLEAN resolve_charset_and_collation (
    TDBB	tdbb,
    SSHORT      *id,
    UCHAR       *charset,
    UCHAR       *collation)
{
/**************************************
 *
 *      r e s o l v e _ c h a r s e t _ a n d _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *      Given ASCII7 name of charset & collation
 *      resolve the specification to a Character set id.
 *      This character set id is also the id of the text_object
 *      that implements the C locale for the Character set.
 *
 * Inputs:
 *      (charset)
 *              ASCII7z name of character set.
 *              NULL (implying unspecified) means use the character set
 *              for defined for (collation).
 *
 *      (collation)
 *              ASCII7z name of collation.
 *              NULL means use the default collation for (charset).
 *
 * Outputs:
 *      (*id)   
 *              Set to character set specified by this name.
 *
 * Return:
 *      1 if no errors (and *id is set).
 *      0 if either name not found.
 *        or if names found, but the collation isn't for the specified
 *        character set.
 *
 **************************************/
BOOLEAN found = FALSE;
BLK     handle;
DBB     dbb;
SSHORT  charset_id;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

assert (id != NULL);

handle = NULL;

if (collation == NULL)
    {
    if (charset == NULL)
	charset = (UCHAR*) DEFAULT_CHARACTER_SET_NAME;

    charset_id = 0;
    if (get_type (tdbb, &charset_id, charset, "RDB$CHARACTER_SET_NAME"))
	{
	*id = charset_id;
	return TRUE;
	}

    /* Charset name not found in the alias table - before giving up
       try the character_set table */

    FOR (REQUEST_HANDLE handle) 
	FIRST 1 CS IN RDB$CHARACTER_SETS  
	WITH CS.RDB$CHARACTER_SET_NAME EQ charset

	found = TRUE;
	*id = CS.RDB$CHARACTER_SET_ID;

    END_FOR;
    CMP_release (tdbb, handle);

    return found;
    }
else if (charset == NULL)
    {
    FOR (REQUEST_HANDLE handle) 
	FIRST 1 COL IN RDB$COLLATIONS 
	WITH COL.RDB$COLLATION_NAME EQ collation

	found = TRUE;
	*id = COL.RDB$CHARACTER_SET_ID;

    END_FOR;
    CMP_release (tdbb, handle);

    return found;
    }

FOR (REQUEST_HANDLE handle) 
    FIRST 1 CS  IN RDB$CHARACTER_SETS CROSS 
	    COL IN RDB$COLLATIONS     OVER   RDB$CHARACTER_SET_ID CROSS
	    AL1 IN RDB$TYPES
	WITH AL1.RDB$FIELD_NAME EQ "RDB$CHARACTER_SET_NAME"
	 AND AL1.RDB$TYPE_NAME  EQ charset
	 AND COL.RDB$COLLATION_NAME EQ collation
	 AND AL1.RDB$TYPE EQ CS.RDB$CHARACTER_SET_ID

    found = TRUE;
    *id = CS.RDB$CHARACTER_SET_ID;

END_FOR;
CMP_release (tdbb, handle);

return found;
}

static STR save_name (
    TDBB	tdbb,
    TEXT        *name)
{
/**************************************
 *
 *      s a v e _ n a m e
 *
 **************************************
 *
 * Functional description
 *      save a name in a string.
 *
 **************************************/
DBB     dbb;
STR     string;
TEXT    *p;
USHORT  l;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

l = name_length (name);
string = (STR) ALLOCPV (type_str, l);
string->str_length = l;
p = (TEXT*) string->str_data;

if (l)
    do *p++ = *name++; while (--l);

return string;
}

static void save_trigger_request (
    DBB		dbb,
    VEC         *ptr,
    REQ         request)
{
/**************************************
 *
 *      s a v e _ t r i g g e r _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *      Get trigger.
 *
 **************************************/
VEC     vector;
USHORT  n;

n = (*ptr) ? (*ptr)->vec_count : 0;
vector = ALL_vector (dbb->dbb_permanent, ptr, n);

vector->vec_object [n] = (BLK) request;
}

static void store_dependencies (
    TDBB	tdbb,
    CSB         csb,
    TEXT        *object_name,
    USHORT      dependency_type)
{
/**************************************
 *
 *      s t o r e _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *      Store records in RDB$DEPENDENCIES
 *      corresponding to the objects found during 
 *      compilation of blr for a trigger, view, etc.
 *
 **************************************/
DBB     dbb;
BLK     request;
REL     relation;
PRC     procedure;
FLD     field;
NOD     node, field_node;  
SSHORT  fld_id, dpdo_type;
SLONG   number;
BOOLEAN found;
TEXT    *field_name, *dpdo_name, name[32];

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

while (csb->csb_dependencies)
    {
    node = (NOD) LLS_POP (&csb->csb_dependencies);
    if (!node->nod_arg [e_dep_object])
	continue;
    dpdo_type = (SSHORT) node->nod_arg [e_dep_object_type];
    if (dpdo_type == obj_relation)
	{
	relation = (REL) node->nod_arg [e_dep_object];
	dpdo_name = relation->rel_name;
	}
    else
	relation = NULL;
    if (dpdo_type == obj_procedure)
	{
	procedure = (PRC) node->nod_arg [e_dep_object];
	dpdo_name = procedure->prc_name->str_data;
	}
    else
	procedure = NULL;
    if (dpdo_type == obj_exception)
	{
	number = (SLONG) node->nod_arg [e_dep_object];
	MET_lookup_exception (tdbb, number, name, NULL);
	dpdo_name = name;
	}
    field_node = node->nod_arg [e_dep_field];

    field_name = NULL;
    if (field_node)
	{
	if (field_node->nod_type == nod_field)
	    {
	    fld_id = (SSHORT) field_node->nod_arg [0];
	    if (relation)
		if (field = MET_get_field (relation, fld_id))
		    field_name = field->fld_name;
	    else if (procedure)
		field = (FLD) procedure->prc_output_fields->vec_object[fld_id];
	    }
	else
	    field_name = (TEXT*) field_node->nod_arg[0];
	}

    if (field_name)
	{
	request = (BLK) CMP_find_request (tdbb, irq_c_deps_f, IRQ_REQUESTS);
	found = FALSE;
	FOR (REQUEST_HANDLE request) X IN RDB$DEPENDENCIES WITH
	  X.RDB$DEPENDENT_NAME = object_name AND
	  X.RDB$DEPENDED_ON_NAME = dpdo_name AND
	  X.RDB$DEPENDED_ON_TYPE = dpdo_type AND
	  X.RDB$FIELD_NAME = field_name AND
	  X.RDB$DEPENDENT_TYPE = dependency_type
	    if (!REQUEST (irq_c_deps_f))
		REQUEST (irq_c_deps_f) = request;
	    found = TRUE;
	END_FOR;
	if (found)
	    continue;
	if (!REQUEST (irq_c_deps_f))
	    REQUEST (irq_c_deps_f) = request;
	}
    else
	{
	request = (BLK) CMP_find_request (tdbb, irq_c_deps, IRQ_REQUESTS);
	found = FALSE;
	FOR (REQUEST_HANDLE request) X IN RDB$DEPENDENCIES WITH
	  X.RDB$DEPENDENT_NAME = object_name AND
	  X.RDB$DEPENDED_ON_NAME = dpdo_name AND
	  X.RDB$DEPENDED_ON_TYPE = dpdo_type AND
	  X.RDB$FIELD_NAME MISSING AND
	  X.RDB$DEPENDENT_TYPE = dependency_type
	    if (!REQUEST (irq_c_deps))
		REQUEST (irq_c_deps) = request;
	    found = TRUE;
	END_FOR;
	if (found)
	    continue;
	if (!REQUEST (irq_c_deps))
	    REQUEST (irq_c_deps) = request;
	}

    request = (BLK) CMP_find_request (tdbb, irq_s_deps, IRQ_REQUESTS);

    STORE (REQUEST_HANDLE request) DEP IN RDB$DEPENDENCIES
	strcpy (DEP.RDB$DEPENDENT_NAME, object_name);
	DEP.RDB$DEPENDED_ON_TYPE = dpdo_type;
	strcpy (DEP.RDB$DEPENDED_ON_NAME, dpdo_name);
	if (field_name)
	    {
	    DEP.RDB$FIELD_NAME.NULL = FALSE;
	    strcpy (DEP.RDB$FIELD_NAME, field_name);
	    }
	else
	    DEP.RDB$FIELD_NAME.NULL = TRUE;
	DEP.RDB$DEPENDENT_TYPE = dependency_type;
    END_STORE;

    if (!REQUEST (irq_s_deps))
	REQUEST (irq_s_deps) = request;
    }
}

static void terminate (TEXT *str, USHORT len)
{
/**************************************
 *
 * t e r m i n a t e
 *
 ****************************************
 *
 *  Functional description
 *      Truncates the rightmost contiguous spaces on a string.
 *
 **************************************************************/
SSHORT i;
assert (len >0);

for (i=len-1; i>=0 && ( (isspace(str[i])) || (str[i] == 0)); i--) 
    /* do nothing */ ;
str[i+1] = 0;
}

static BOOLEAN verify_TRG_ignore_perm (
    TDBB	tdbb,
    TEXT        *trig_name)
{
/*****************************************************
 *
 *      v e r i f y _ T R G _ i g n o r e  _ p e r m
 *
 *****************************************************
 *
 * Functional description
 *      Return TRUE if this trigger can go through without any permission
 *      checks. Currently, the only class of triggers that can go
 *      through without permission checks are 
 *      (a) two system triggers (RDB$TRIGGERS_34 and RDB$TRIGGERS_35)
 *      (b) those defined for referential integrity actions such as, 
 *      set null, set default, and cascade.
 *
 **************************************/
DBB     dbb;
BLK     request;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

/* see if this is a system trigger, with the flag set as TRG_ignore_perm */
if (INI_get_trig_flags(trig_name) & (USHORT)TRG_ignore_perm)
    return(TRUE);

request = (BLK) CMP_find_request (tdbb, irq_c_trg_perm, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    CHK IN RDB$CHECK_CONSTRAINTS CROSS
    REF IN RDB$REF_CONSTRAINTS WITH
    CHK.RDB$TRIGGER_NAME EQ trig_name AND
    REF.RDB$CONSTRAINT_NAME = CHK.RDB$CONSTRAINT_NAME

    if (!REQUEST (irq_c_trg_perm))
	REQUEST (irq_c_trg_perm) = request;

    EXE_unwind (tdbb, request);
    terminate (REF.RDB$UPDATE_RULE, sizeof(REF.RDB$UPDATE_RULE));
    terminate (REF.RDB$DELETE_RULE, sizeof(REF.RDB$DELETE_RULE));

    if (!strcmp (REF.RDB$UPDATE_RULE, RI_ACTION_CASCADE) ||
        !strcmp (REF.RDB$UPDATE_RULE, RI_ACTION_NULL)    ||
        !strcmp (REF.RDB$UPDATE_RULE, RI_ACTION_DEFAULT) ||
        !strcmp (REF.RDB$DELETE_RULE, RI_ACTION_CASCADE) ||
        !strcmp (REF.RDB$DELETE_RULE, RI_ACTION_NULL)    ||
        !strcmp (REF.RDB$DELETE_RULE, RI_ACTION_DEFAULT) )
        return (TRUE);
    else
        return (FALSE);
END_FOR;

if (!REQUEST (irq_c_trg_perm))
    REQUEST (irq_c_trg_perm) = request;

return (FALSE);
}

