/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dfw.e
 *	DESCRIPTION:	Deferred Work handler
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
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/pio.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/scl.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/lck.h"
#include "../jrd/sdw.h"
#include "../jrd/flags.h"
#include "../jrd/all.h"
#include "../jrd/intl.h"
#include "../intl/charsets.h"
#include "../jrd/align.h"
#include "../jrd/gdsassert.h"
#include "../jrd/constants.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/isc_f_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/pcmet_proto.h"
#include "../jrd/pio_proto.h"
#include "../jrd/sch_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/thd_proto.h"
#ifndef WINDOWS_ONLY
#include "../jrd/event_proto.h"
#endif

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

/* Pick up system relation ids */

#define RELATION(name,id,ods)      id,
#define FIELD(symbol,name,id,update,ods,new_id,new_ods)
#define END_RELATION

typedef ENUM rids {
#include "../jrd/relations.h"
    rel_MAX
} RIDS;

#undef RELATION
#undef FIELD
#undef END_RELATION

/* Define range of user relation ids */

#define MIN_RELATION_ID		rel_MAX
#define MAX_RELATION_ID		32767

#define COMPUTED_FLAG	128
#define NULL_BLOB(id)	(!id.gds_quad_high && !id.gds_quad_low)
#define WAIT_PERIOD	-1

DATABASE
    DB = FILENAME "ODS.RDB"; 

/*==================================================================
**
** NOTE:
**
**	The following functions required the same number of
**	parameters to be passed.
**
**==================================================================
*/
static BOOLEAN	add_file		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	add_shadow		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_shadow		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	compute_security	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	create_index		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_index		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	create_log		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_log		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	create_procedure	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_procedure	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	modify_procedure	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	create_relation		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_relation		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	scan_relation		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	create_trigger		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_trigger		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	modify_trigger		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_exception	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_field		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_global		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_parameter	(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	delete_rfr		(TDBB, SSHORT, DFW, TRA);
static BOOLEAN	make_version		(TDBB, SSHORT, DFW, TRA);

/* ---------------------------------------------------------------- */

static void	check_dependencies	(TDBB, TEXT *, TEXT *,
						USHORT, TRA);
static void	check_filename (TEXT *, USHORT);
static BOOLEAN	find_depend_in_dfw	(TDBB, TEXT *, USHORT,
						USHORT, TRA);
static void	get_array_desc		(TDBB, TEXT *, ADS);
static void	get_procedure_dependencies (DFW);
static void	get_trigger_dependencies (DFW);
static FMT	make_format		(TDBB, REL, USHORT, TFB);
static USHORT	name_length (TEXT *);
static void	put_summary_blob (BLB, enum rsr_t, SLONG [2]);
static void	put_summary_record (BLB, enum rsr_t, UCHAR *, USHORT);
static BOOLEAN	shadow_defined		(TDBB);
static BOOLEAN	wal_defined		(TDBB);

static CONST UCHAR nonnull_validation_blr [] =
   {
   blr_version5,
   blr_not,
      blr_missing,
         blr_fid, 0, 0,0,
   blr_eoc
   };

typedef struct task {
    ENUM dfw_t	task_type;
    BOOLEAN	(*task_routine) (TDBB, SSHORT, DFW, TRA);
} TASK;

static CONST TASK task_table [] =
    {
    dfw_add_file, add_file,
    dfw_add_shadow, add_shadow,
    dfw_delete_index, delete_index,
    dfw_delete_rfr, delete_rfr,
    dfw_delete_relation, delete_relation,
    dfw_delete_shadow, delete_shadow,
    dfw_delete_field, delete_field,
    dfw_delete_global, delete_global,
    dfw_create_relation, create_relation,
    dfw_update_format, make_version,
    dfw_scan_relation, scan_relation,
    dfw_compute_security, compute_security,
    dfw_create_index, create_index,
#ifdef EXPRESSION_INDICES
    dfw_create_expression_index, PCMET_expression_index,
#endif
    dfw_delete_expression_index, delete_index,
    dfw_grant, GRANT_privileges,
    dfw_create_trigger, create_trigger,
    dfw_delete_trigger, delete_trigger,
    dfw_modify_trigger, modify_trigger,
    dfw_create_log, create_log,
    dfw_delete_log, delete_log,
    dfw_create_procedure, create_procedure,
    dfw_delete_procedure, delete_procedure,
    dfw_modify_procedure, modify_procedure,
    dfw_delete_prm, delete_parameter,
    dfw_delete_exception, delete_exception,
    dfw_null, NULL
    };

#ifdef SHLIB_DEFS
#define strcpy		(*_libgds_strcpy)
#define strlen		(*_libgds_strlen)
#define strcmp		(*_libgds_strcmp)
#define SETJMP		(*_libgds_setjmp)
#define memcmp		(*_libgds_memcmp)
#define strncmp		(*_libgds_strncmp)
#define unlink		(*_libgds_unlink)
#define _iob		(*_libgds__iob)
#define ib_fprintf		(*_libgds_fprintf)

extern int		strlen();
extern int		strcmp();
extern int		SETJMP();
extern int		memcmp();
extern int		strncmp();
extern SCHAR		*strcpy();
extern int		unlink();
extern IB_FILE		_iob [];
extern int		ib_fprintf();
#endif

USHORT DFW_assign_index_type (
    DFW		work,
    SSHORT	field_type,
    SSHORT	ttype)
{
/**************************************
 *
 *	D F W _ a s s i g n _ i n d e x _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Define the index segment type based
 * 	on the field's type and subtype.
 *
 **************************************/
TDBB	tdbb;

tdbb = GET_THREAD_DATA;

if (field_type == dtype_varying || field_type == dtype_text)
    {
    STATUS	status [20];

    if (ttype == ttype_none)
	return idx_string;

    if (ttype == ttype_binary)
	return idx_byte_array;

    if (ttype == ttype_metadata)
	return idx_metadata;

    if (ttype == ttype_ascii)
	return idx_string;

    /* Dynamic text cannot occur here as this is for an on-disk
       index, which must be bound to a text type. */

    assert (ttype != ttype_dynamic);

    if (INTL_defined_type (tdbb, status, ttype))
	return INTL_TEXT_TO_INDEX (ttype);

    ERR_post (gds__no_meta_update, 
	gds_arg_gds, gds__random, gds_arg_string, ERR_cstring (work->dfw_name), 
	status [0], status [1], status [2], status [3],
	0);
    }  

if (field_type == dtype_timestamp)
    return idx_timestamp2;
else if (field_type == dtype_sql_date)
    return idx_sql_date;
else if (field_type == dtype_sql_time)
    return idx_sql_time;

#ifdef EXACT_NUMERICS
/* idx_numeric2 used for 64-bit Integer support */
if (field_type == dtype_int64)
    return idx_numeric2;
#endif

return idx_numeric;
}

void DFW_delete_deferred (
    TRA		transaction,
    SLONG	sav_number)
{
/**************************************
 *
 *	D F W _ d e l e t e _ d e f e r r e d
 *
 **************************************
 *
 * Functional description
 *	Get rid of work deferred that was to be done at 
 *	COMMIT time as the statement has been rolled back.
 *
 *	if (sav_number == -1), then  remove all entries.
 *
 **************************************/
DFW	work, *ptr;
USHORT	deferred_meta;

/* If there is no deferred work, just return */

if (!transaction->tra_deferred_work)
    return;

/* Remove deferred work and events which are to be rolled back */

deferred_meta = 0;

for (ptr = &transaction->tra_deferred_work; work = *ptr;)
    if ((work->dfw_sav_number == sav_number) || (sav_number == -1))
	{
	*ptr = work->dfw_next;
	ALL_release (work);
	}
    else
	{
	ptr = &(*ptr)->dfw_next;
        if (work->dfw_type != dfw_post_event)
	    deferred_meta = 1;
	}

if (!deferred_meta)
    transaction->tra_flags &= ~TRA_deferred_meta;
}

void DFW_merge_work (
    TRA		transaction,
    SLONG	old_sav_number,
    SLONG	new_sav_number)
{
/**************************************
 *
 *	D F W _ m e r g e _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Merge the deferred work with the previous level.  This will
 *	be called only of there is a previous level.
 *
 **************************************/
DFW	work, *ptr;
DFW	work_m, *ptr_m;

/* If there is no deferred work, just return */

if (!transaction->tra_deferred_work)
    return;

/* Decrement the save point number in the deferred block
 * i.e. merge with the previous level.
 */

for (ptr = &transaction->tra_deferred_work; work = *ptr;)
    {
    if (work->dfw_sav_number == old_sav_number)
	{
	work->dfw_sav_number = new_sav_number;

	/* merge this entry with other identical entries at
	 * same save point level. Start from the beginning and
	 * stop with the that is being merged.
	 */
	
	for (ptr_m = &transaction->tra_deferred_work; ((work_m = *ptr_m) && 
	    (work_m != work)); ptr_m = &(*ptr_m)->dfw_next)
	    {
	    if (work_m->dfw_type 	== work->dfw_type &&
		work_m->dfw_id 		== work->dfw_id &&
		work_m->dfw_name_length == work->dfw_name_length &&
		work_m->dfw_sav_number 	== work->dfw_sav_number)
		{
		if ((work->dfw_name_length) && 
		    (memcmp (work->dfw_name, work_m->dfw_name, work->dfw_name_length)))
		    continue;

		/* Yes!  There is a duplicate entry.  Take out the
		 * entry for which the save point was decremented
		 */

		*ptr = work->dfw_next;

		if (work_m->dfw_name_length)
		    work_m->dfw_count += work->dfw_count;

		ALL_release (work);
		work = (DFW) 0;
		break;
		}
	    }
	}

    if (work)
	ptr = &(*ptr)->dfw_next;
    }
}

void DFW_perform_system_work (void)
{
/**************************************
 *
 *	D F W _ p e r f o r m _ s y s t e m _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Flush out the work left to be done in the
 *	system transaction.
 *
 **************************************/
DBB	dbb;

dbb = GET_DBB;

DFW_perform_work (dbb->dbb_sys_trans);
}

void DFW_perform_work (
    TRA	transaction)
{
/**************************************
 *
 *	D F W _ p e r f o r m _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Do work deferred to COMMIT time 'cause that time has
 *	come.
 *
 **************************************/
TDBB	tdbb;
TASK	*task;
DFW	work, *ptr;
BOOLEAN	dump_shadow, more;
SSHORT	phase;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

/* If no deferred work or it's all deferred event posting
   don't bother */

if (!transaction->tra_deferred_work ||
    !(transaction->tra_flags & TRA_deferred_meta))
    return;

tdbb->tdbb_default = transaction->tra_pool;
dump_shadow = FALSE;

/* Loop for as long as any of the deferred work routines says that it has
   more to do.  A deferred work routine should be able to deal with any
   value of phase, either to say that it wants to be called again in the
   next phase (by returning TRUE) or that it has nothing more to do in this
   or later phases (by returning FALSE). By convention, phase 0 has been
   designated as the cleanup phase. If any non-zero phase punts, then phase 0
   is executed for all deferred work blocks to cleanup work-in-progress. */

phase = 1;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    /* Do any necessary cleanup */

    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    phase = 0;
    }

do {
    more = FALSE;
    for (task = task_table; task->task_type != dfw_null; task++)
	for (work = transaction->tra_deferred_work; work; work = work->dfw_next)
	    if (work->dfw_type == task->task_type)
		{
		if (work->dfw_type == dfw_add_shadow)
	 	    dump_shadow = TRUE;
		if ((*task->task_routine) (tdbb, phase, work, transaction))
		    more = TRUE;
		}
    if (!phase)
	ERR_punt();
    phase++;
} while (more);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

/* Remove deferred work blocks so that system transaction and
   commit retaining transactions don't re-execute them. Leave
   events to be posted after commit */

for (ptr = &transaction->tra_deferred_work; work = *ptr;)
    if ((work->dfw_type == dfw_post_event) ||
	(work->dfw_type == dfw_delete_shadow))
	ptr = &(*ptr)->dfw_next;
    else
	{
	*ptr = work->dfw_next;
	ALL_release (work);
	}

transaction->tra_flags &= ~TRA_deferred_meta;

if (dump_shadow)
    SDW_dump_pages();
}

void DFW_perform_post_commit_work (
    TRA		transaction)
{
/**************************************
 *
 *	D F W _ p e r f o r m _ p o s t _ c o m m i t _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Perform any post commit work
 *	1. Post any pending events.
 *	2. Unlink shadow files for dropped shadows
 *
 *	Then, delete it from chain of pending work.
 *
 **************************************/
#ifndef WINDOWS_ONLY
DBB	dbb;
LCK	lock;
#endif
STATUS	status [20];
DFW	work, *ptr;

if (!transaction->tra_deferred_work)
    return;

#ifndef WINDOWS_ONLY
dbb = GET_DBB;
lock = dbb->dbb_lock;
#endif

for (ptr = &transaction->tra_deferred_work; work = *ptr;)
    if (work->dfw_type == dfw_post_event)
	{
#ifndef WINDOWS_ONLY /* Events are not supported under LIBS */
	EVENT_post (status, 
		lock->lck_length,
		(TEXT*) &lock->lck_key,
		work->dfw_name_length,
		work->dfw_name,
		work->dfw_count);
#endif
	*ptr = work->dfw_next;
	ALL_release (work);
	}
    else if (work->dfw_type == dfw_delete_shadow)
	{
	unlink (work->dfw_name);
	*ptr = work->dfw_next;
	ALL_release (work);
	}
    else
	ptr = &(*ptr)->dfw_next;
}

void DFW_post_work (
    TRA		transaction,
    ENUM dfw_t	type,
    DSC		*desc,
    USHORT	id)
{
/**************************************
 *
 *	D F W _ p o s t _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Post a piece of work to be deferred to commit time.
 *	If we already know about it, so much the better.
 *
 **************************************/
DFW		work, *ptr;
UCHAR		*p, *q, *string;
USHORT		l, length;
TEXT		temp [256];	/* Must hold largest metadata field */
SLONG		sav_number;

if (!desc)
    {
    string = NULL;
    length = 0;
    }
else
    {
    /* Find the actual length of the string, searching until the claimed
       end of the string, or the terminating \0, whichever comes first. */

    length = MOV_make_string (desc, ttype_metadata, &string, temp, sizeof (temp));
    for (p = string, q = string + length; p < q && *p; p++)
	;

    /* Trim trailing blanks (bug 3355) */

    while (--p >= string && *p == ' ')
	;
    length = (p + 1) - string;
    }

/* get the current save point number */

sav_number = transaction->tra_save_point ? 
	transaction->tra_save_point->sav_number : 0;

/* Check to see if work is already posted */

for (ptr = &transaction->tra_deferred_work; work = *ptr;
     ptr = &(*ptr)->dfw_next)
    if (work->dfw_type == type &&
	work->dfw_id == id &&
	work->dfw_name_length == length &&
	work->dfw_sav_number == sav_number)
	{
	if (!length)
	    return;
	if (!memcmp (string, work->dfw_name, length))
	    {
	    ++work->dfw_count;
	    return;
	    }
	}

/* Not already posted, so do so now. */

*ptr = work = (DFW) ALLOCTV (type_dfw, length);
work->dfw_type = type;
work->dfw_id = id;
work->dfw_count = 1;
work->dfw_name_length = length;
work->dfw_sav_number = sav_number;
if (length)
    MOVE_FAST (string, work->dfw_name, length);

if (type != dfw_post_event)
    transaction->tra_flags |= TRA_deferred_meta;
else if (transaction->tra_save_point)
    transaction->tra_save_point->sav_flags |= SAV_event_post;
}

void DFW_update_index (
    DFW		work,
    USHORT	id,
    float	selectivity)
{
/**************************************
 *
 *	D F W _ u p d a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Update information in the index relation after creation
 *	of the index.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;

tdbb = GET_THREAD_DATA;
dbb  = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_m_index, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    IDX IN RDB$INDICES WITH IDX.RDB$INDEX_NAME EQ work->dfw_name
    if (!REQUEST (irq_m_index))
	REQUEST (irq_m_index) = request;
    MODIFY IDX USING
	IDX.RDB$INDEX_ID = id + 1;
	IDX.RDB$STATISTICS = selectivity;
    END_MODIFY;
END_FOR;

if (!REQUEST (irq_m_index))
    REQUEST (irq_m_index) = request;
}

static BOOLEAN add_file (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	a d d _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Add a file to a database.
 *	This file could be a regular database
 *	file or a shadow file.  Either way we
 *	require exclusive access to the database.
 *
 **************************************/
DBB	dbb;
USHORT	section, shadow_number;
SLONG	start, max;
BLK	handle, handle2;
TEXT	temp [MAX_PATH_LENGTH];

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

switch (phase)
    {
    case 0:
	CCH_release_exclusive (tdbb);
	return FALSE;

    case 1:
    case 2:
	return TRUE;

    case 3:
        if (CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
            return TRUE;
        else
            {
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__lock_timeout,
                gds_arg_gds, gds__obj_in_use,
                gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string),
                0);
            return FALSE;
            }
    case 4:
	CCH_flush (tdbb, (USHORT) FLUSH_FINI, 0L);
	max = PIO_max_alloc (dbb) + 1;
	handle = handle2 = NULL;

	/* Check the file name for node name.  This has already been done for 
	** shadows in add_shadow () */

	if (work->dfw_type != dfw_add_shadow)
	    check_filename (work->dfw_name, work->dfw_name_length);

	/* get any files to extend into */

	FOR (REQUEST_HANDLE handle) X IN RDB$FILES 
	      WITH X.RDB$FILE_NAME EQ work->dfw_name

	    /* First expand the file name This has already been done 
	    ** for shadows in add_shadow ()) */

	    if (work->dfw_type != dfw_add_shadow)
		{
	    	ISC_expand_filename (X.RDB$FILE_NAME, 0, temp); 
	    	MODIFY X USING
                    strcpy(X.RDB$FILE_NAME, temp);
            	END_MODIFY;
		}

	    /* If there is no starting position specified, or if it is
	       too low a value, make a stab at assigning one based on 
	       the indicated preference for the previous file length. */

	    if ((start = X.RDB$FILE_START) < max)
		{
		FOR (REQUEST_HANDLE handle2) 
		    FIRST 1 Y IN RDB$FILES
		    WITH Y.RDB$SHADOW_NUMBER EQ X.RDB$SHADOW_NUMBER
		    AND Y.RDB$FILE_SEQUENCE NOT MISSING
		    SORTED BY DESCENDING Y.RDB$FILE_SEQUENCE
		    start = Y.RDB$FILE_START + Y.RDB$FILE_LENGTH;
		END_FOR;
		}

	    start = MAX (max, start);
	    shadow_number = X.RDB$SHADOW_NUMBER;
	    if ((shadow_number && (section = SDW_add_file (X.RDB$FILE_NAME, start, shadow_number))) ||
		(section = PAG_add_file (X.RDB$FILE_NAME, start)))
		MODIFY X USING
		    X.RDB$FILE_SEQUENCE = section;
		    X.RDB$FILE_START = start;
		END_MODIFY;
	END_FOR;

	CMP_release (tdbb, handle);
	if (handle2)
	    CMP_release (tdbb, handle2);

	if (section)
	    {
	    handle = NULL;
	    section--;
	    FOR (REQUEST_HANDLE handle) X IN RDB$FILES
		WITH X.RDB$FILE_SEQUENCE EQ section
		AND X.RDB$SHADOW_NUMBER EQ shadow_number
		    MODIFY X USING
			X.RDB$FILE_LENGTH = start - X.RDB$FILE_START;
		    END_MODIFY;
	    END_FOR;
	    CMP_release (tdbb, handle);
	    }

	CCH_release_exclusive (tdbb);
	break;
    }

return FALSE;
}


static BOOLEAN add_shadow (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	a d d _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	A file or files have been added for shadowing.
 *	Get all files for this particular shadow first
 *	in order of starting page, if specified, then
 *	in sequence order.
 *
 **************************************/
DBB	dbb;
BLK	handle;
SDW	shadow;
USHORT	sequence, add_sequence;
BOOLEAN	finished;
ULONG	min_page;
TEXT	expanded_fname [1024];

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 0:
	CCH_release_exclusive (tdbb);
	return FALSE;

    case 1:
    case 2:
    case 3:
	return TRUE;

    case 4:
	if (wal_defined (tdbb))
            ERR_post (gds__no_meta_update, 
		gds_arg_gds, gds__wal_shadow_err, 0);
	/* Msg309: Write-Ahead Log with Shadowing configuration not allowed */

	check_filename (work->dfw_name, work->dfw_name_length);

	/* could have two cases: 
	   1) this shadow has already been written to, so add this file using
	      the standard routine to extend a database
	   2) this file is part of a newly added shadow which has already been
	      fetched in totem and prepared for writing to, so just ignore it
	*/

	finished = FALSE;
	handle = NULL;
	FOR (REQUEST_HANDLE handle)
	    F IN RDB$FILES
	    WITH F.RDB$FILE_NAME EQ work->dfw_name

	    ISC_expand_filename (F.RDB$FILE_NAME, 0, expanded_fname); 
	    MODIFY F USING
		strcpy (F.RDB$FILE_NAME, expanded_fname);
            END_MODIFY;
	     
	    for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
		if ((F.RDB$SHADOW_NUMBER == shadow->sdw_number) &&
	           !(shadow->sdw_flags & SDW_IGNORE))
		    {
		    if (F.RDB$FILE_FLAGS & FILE_shadow)
			/* This is the case of a bogus duplicate posted
			 * work when we added a multi-file shadow 
			 */
			finished = TRUE;
		    else if (shadow->sdw_flags & (SDW_dumped))
			{
			/* Case of adding a file to a currently active
			 * shadow set.
			 * Note: as of 1995-January-31 there is
			 * no SQL syntax that supports this, but there
			 * may be GDML
			 */
			if (!CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
			    ERR_post (gds__no_meta_update,
				gds_arg_gds, gds__lock_timeout,
			        gds_arg_gds, gds__obj_in_use,
			        gds_arg_string, 
				ERR_cstring (dbb->dbb_file->fil_string), 
			        0);
			add_file (tdbb, phase, work, (TRA)0);
			finished = TRUE;
			}
		    else
			{
			/* We cannot add a file to a shadow that is still
			 * in the process of being created.
			 */
			ERR_post (gds__no_meta_update,
			        gds_arg_gds, gds__obj_in_use,
			        gds_arg_string, 
				ERR_cstring (dbb->dbb_file->fil_string), 
			        0);
			}
		    break;
		    }

	END_FOR;
	CMP_release (tdbb, handle);

	if (finished)
	    return FALSE;

	/* this file is part of a new shadow, so get all files for the shadow
	   in order of the starting page for the file */

	/* Note that for a multi-file shadow, we have several pieces of
	 * work posted (one dfw_add_shadow for each file).  Rather than
	 * trying to cancel the other pieces of work we ignore them
	 * when they arrive in this routine.
	 */

	sequence = 0;
	min_page = 0;
	shadow = NULL;
	handle = NULL;
	FOR (REQUEST_HANDLE handle) 
	      X IN RDB$FILES CROSS
	      Y IN RDB$FILES
	      OVER RDB$SHADOW_NUMBER
	      WITH X.RDB$FILE_NAME EQ expanded_fname
	      SORTED BY Y.RDB$FILE_START

	    /* for the first file, create a brand new shadow; for secondary
	       files that have a starting page specified, add a file */

	    if (!sequence)
		SDW_add (Y.RDB$FILE_NAME, Y.RDB$SHADOW_NUMBER, Y.RDB$FILE_FLAGS);
	    else if (Y.RDB$FILE_START)
		{
		if (!shadow)
		    for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
			if ((Y.RDB$SHADOW_NUMBER == shadow->sdw_number) &&
	       		    !(shadow->sdw_flags & SDW_IGNORE))
			    break;

		if (!shadow)
		    BUGCHECK (203); /* msg 203 shadow block not found for extend file */

		min_page = MAX ((min_page + 1), Y.RDB$FILE_START);
		add_sequence = SDW_add_file (Y.RDB$FILE_NAME, min_page, Y.RDB$SHADOW_NUMBER);
  		}

 	    /* update the sequence number and bless the file entry as being
	       good */

	    if (!sequence || (Y.RDB$FILE_START && add_sequence))
		{
		MODIFY Y
		    Y.RDB$FILE_FLAGS |= FILE_shadow;
		    Y.RDB$FILE_SEQUENCE = sequence;
		    Y.RDB$FILE_START = min_page;
		END_MODIFY;
		sequence++;
		}
	    
	END_FOR;
	CMP_release (tdbb, handle);
	break;
    }

return FALSE;
}

static void check_dependencies (
    TDBB	tdbb,
    TEXT	*dpdo_name,
    TEXT	*field_name,
    USHORT	dpdo_type,
    TRA		transaction)
{
/**************************************
 *
 *	c h e c k _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *	Check the dependency list for relation or relation.field
 *	before deleting such.
 *
 **************************************/
DBB	dbb;
BLK	request;
USHORT	dep_counts [obj_count], i;
STATUS	obj_type;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

for (i = 0; i < (USHORT) obj_count; i++)
    dep_counts [i] = 0;

if (field_name)
    {
    request = (BLK) CMP_find_request (tdbb, irq_ch_f_dpd, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request) 
	    DEP IN RDB$DEPENDENCIES
	    WITH DEP.RDB$DEPENDED_ON_NAME EQ dpdo_name
	    AND DEP.RDB$DEPENDED_ON_TYPE = dpdo_type
	    AND DEP.RDB$FIELD_NAME EQ field_name
	    REDUCED TO DEP.RDB$DEPENDENT_NAME

	if (!REQUEST (irq_ch_f_dpd))
	    REQUEST (irq_ch_f_dpd) = request;

	/* If the found object is also being deleted, there's no dependency */

	if (!find_depend_in_dfw (tdbb, DEP.RDB$DEPENDENT_NAME,
				 DEP.RDB$DEPENDENT_TYPE, 0, transaction))
	    dep_counts [DEP.RDB$DEPENDENT_TYPE]++;
    END_FOR;

    if (!REQUEST (irq_ch_f_dpd))
	REQUEST (irq_ch_f_dpd) = request;
    }
else
    {
    request = (BLK) CMP_find_request (tdbb, irq_ch_dpd, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request) 
	    DEP IN RDB$DEPENDENCIES
	    WITH DEP.RDB$DEPENDED_ON_NAME EQ dpdo_name
	    AND DEP.RDB$DEPENDED_ON_TYPE = dpdo_type
	    REDUCED TO DEP.RDB$DEPENDENT_NAME

	if (!REQUEST (irq_ch_dpd))
	    REQUEST (irq_ch_dpd) = request;

	/* If the found object is also being deleted, there's no dependency */

	if (!find_depend_in_dfw (tdbb, DEP.RDB$DEPENDENT_NAME,
				 DEP.RDB$DEPENDENT_TYPE,
				 0, transaction))
	    dep_counts [DEP.RDB$DEPENDENT_TYPE]++;
    END_FOR;

    if (!REQUEST (irq_ch_dpd))
	REQUEST (irq_ch_dpd) = request;
    }

for (i = 0; i < obj_count; i++)
    if (dep_counts [i])
	{
	switch (dpdo_type)
	    {
	    case obj_relation:
	    	obj_type = gds__table_name;
		break;
	    case obj_procedure:
		obj_type = gds__proc_name;
		break;
	    case obj_exception:
		obj_type = gds__exception_name;
		break;
	    default:
		assert (FALSE);
		break;
	    }
	if (field_name)
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__no_delete,    /* Msg353: can not delete */
		gds_arg_gds, gds__field_name, gds_arg_string, ERR_cstring (field_name),
		gds_arg_gds, gds__dependency, gds_arg_number, (SLONG) dep_counts [i],
		0);       /* Msg310: there are %ld dependencies */
	else
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__no_delete,            /* can not delete */
		gds_arg_gds, obj_type, gds_arg_string, ERR_cstring (dpdo_name),
		gds_arg_gds, gds__dependency, gds_arg_number, (SLONG) dep_counts [i],
		0);                           /* there are %ld dependencies */
	}
}

static void check_filename (
    TEXT	*name,
    USHORT	l)
{
/**************************************
 *
 *	c h e c k _ f i l e n a m e
 *
 **************************************
 *
 * Functional description
 *	Make sure that a file path doesn't contain an
 *	inet node name.
 *
 **************************************/
TEXT	file_name [MAX_PATH_LENGTH], *p, *q;
BOOLEAN	valid = TRUE;

l = MIN (l, sizeof (file_name) - 1);
for (p = file_name, q = name; l--; *p++ = *q++)
    ;
*p = 0;
for (p = file_name; *p; p++)
    if (p [0] == ':' && p [1] == ':')
	valid = FALSE;

if (!valid || ISC_check_if_remote (file_name, FALSE))
    ERR_post (gds__no_meta_update,
	gds_arg_gds, gds__node_name_err, 0);
    /* Msg305: A node name is not permitted in a secondary, shadow, or log file name */
}

static BOOLEAN compute_security (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c o m p u t e _ s e c u r i t y
 *
 **************************************
 *
 * Functional description
 *	There was a change in a security class.  Recompute everything
 *	it touches.
 *
 **************************************/
DBB	dbb;
BLK	handle;
SCL	class;
REL	relation;
FLD	field;
USHORT	id;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	/* Get security class.  This may return NULL if it doesn't exist */

	class = SCL_recompute_class (tdbb, work->dfw_name);

	handle = NULL;
	FOR (REQUEST_HANDLE handle) X IN RDB$DATABASE
		WITH X.RDB$SECURITY_CLASS EQ work->dfw_name
	    tdbb->tdbb_attachment->att_security_class = class;
	END_FOR;
	CMP_release (tdbb, handle);
	break;
    }

return FALSE;

/**** OBSOLETE!!! Only security class name strings in relations and fields.
      Note: fld_security_class  --> fld_security_name
            rel_security_class  --> rel_security_name

handle = NULL;
for (request_handle handle) x in rdb$relation_fields 
	with x.rdb$security_class eq work->dfw_name
    if (relation = MET_lookup_relation (tdbb, x.rdb$relation_name))
	{
    	id = MET_lookup_field (tdbb, relation, x.rdb$field_name);
    	if (field = MET_get_field (relation, id))
	    field->fld_security_class = class;
	}
end_for;
CMP_release (tdbb, handle);

handle = NULL;
for (request_handle handle) x in rdb$relations 
	with x.rdb$security_class eq work->dfw_name
    if (relation = MET_lookup_relation (tdbb, x.rdb$relation_name))
        relation->rel_security_class = class;
end_for;
CMP_release (tdbb, handle);

****/
}

static BOOLEAN create_index (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c r e a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Create a new index or change the state of an index between active/inactive.
 *
 **************************************/
DBB	dbb;
BLK	request;
REL	relation, partner_relation;
IDX	idx;
int	key_count;
float	selectivity;
SSHORT	text_type;
SSHORT	collate;
BLK	handle;
WIN	window;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 0:
	handle = NULL;

	/* Drop those indices at clean up time. */
	FOR (REQUEST_HANDLE handle) IDXN IN RDB$INDICES CROSS
		IREL IN RDB$RELATIONS OVER RDB$RELATION_NAME
		WITH IDXN.RDB$INDEX_NAME EQ work->dfw_name

	  /* Views do not have indices */
	  if (IREL.RDB$VIEW_BLR.NULL)
	    {
	    relation = MET_lookup_relation (tdbb, IDXN.RDB$RELATION_NAME);

	    /* Fetch the root index page and mark MUST_WRITE, and then
	       delete the index. It will also clean the index slot. */
	    if (relation->rel_index_root)
	        {
		if (work->dfw_id != MAX_IDX)
		    {
	            window.win_page = relation->rel_index_root;
		    window.win_flags = 0;
	            CCH_FETCH (tdbb, &window, LCK_write, pag_root);
	            CCH_MARK_MUST_WRITE (tdbb, &window);
	            BTR_delete_index (tdbb, &window, work->dfw_id);
		    work->dfw_id = MAX_IDX;
		    }
		if (!IDXN.RDB$INDEX_ID.NULL)
		    {
		    MODIFY IDXN USING
	        	IDXN.RDB$INDEX_ID.NULL = TRUE;
		    END_MODIFY;
		    }
	        }
	    }
	END_FOR;

	CMP_release (tdbb, handle);
    	CCH_release_exclusive (tdbb);
	return FALSE;
	    
    case 1:
    case 2:
	return TRUE;

    case 3:
	key_count = 0;
	relation = NULL;
	idx.idx_flags = 0;

	/* Fetch the information necessary to create the index.  On the first
	   time thru, check to see if the index already exists.  If so, delete
	   it.  If the index inactive flag is set, don't create the index */

	request = (BLK) CMP_find_request (tdbb, irq_c_index, IRQ_REQUESTS);

	FOR (REQUEST_HANDLE request) 
	    IDX IN RDB$INDICES CROSS
	    SEG IN RDB$INDEX_SEGMENTS OVER RDB$INDEX_NAME CROSS
	    RFR IN RDB$RELATION_FIELDS OVER RDB$FIELD_NAME, RDB$RELATION_NAME CROSS
	    FLD IN RDB$FIELDS CROSS
	    REL IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH
	        FLD.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE AND
	        IDX.RDB$INDEX_NAME EQ work->dfw_name 

	    if (!REQUEST (irq_c_index))
		REQUEST (irq_c_index) = request;

	    if (!relation)
	        {
	        if (!(relation = MET_lookup_relation_id (tdbb, REL.RDB$RELATION_ID, FALSE)))
		    {
		    EXE_unwind (tdbb, request);
                    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__idx_create_err,
			gds_arg_string, ERR_cstring (work->dfw_name),
			0);
			/* Msg308: can't create index %s */
		    }
	        if (IDX.RDB$INDEX_ID && IDX.RDB$STATISTICS < 0.0)
	            {
	            MODIFY IDX
	   	        IDX.RDB$STATISTICS = IDX_statistics (tdbb, relation, IDX.RDB$INDEX_ID - 1);
		    END_MODIFY;
		    EXE_unwind (tdbb, request);
		    return FALSE;
		    }
		if (IDX.RDB$INDEX_ID)
		    {
		    IDX_delete_index (tdbb, relation, IDX.RDB$INDEX_ID - 1);
		    MODIFY IDX
		        IDX.RDB$INDEX_ID.NULL = TRUE;
		    END_MODIFY;
		    }
		if (IDX.RDB$INDEX_INACTIVE)
		    {
		    EXE_unwind (tdbb, request);
		    return FALSE;
		    }
		idx.idx_count = IDX.RDB$SEGMENT_COUNT;
		if (!idx.idx_count || idx.idx_count > 16)
		    {
		    EXE_unwind (tdbb, request);
                    if (!idx.idx_count)
			ERR_post (gds__no_meta_update,
			    gds_arg_gds, gds__idx_seg_err,
			    gds_arg_string, ERR_cstring (work->dfw_name),
			    0);
			/* Msg304: segment count of 0 defined for index %s */
		    else
			ERR_post (gds__no_meta_update,
			    gds_arg_gds, gds__idx_key_err,
			    gds_arg_string, ERR_cstring (work->dfw_name),
			    0);
			/* Msg311: too many keys defined for index %s */
		    }
		if (IDX.RDB$UNIQUE_FLAG)
		    idx.idx_flags |= idx_unique;
		if (IDX.RDB$INDEX_TYPE == 1)
		    idx.idx_flags |= idx_descending;
		if (!IDX.RDB$FOREIGN_KEY.NULL)
		    idx.idx_flags |= idx_foreign;
		if (!strncmp (IDX.RDB$INDEX_NAME, "RDB$PRIMARY", strlen ("RDB$PRIMARY")))
		    idx.idx_flags |= idx_primary;
		}

	    if (++key_count > idx.idx_count ||
		SEG.RDB$FIELD_POSITION > idx.idx_count ||
		FLD.RDB$FIELD_TYPE == blr_blob ||
		!FLD.RDB$DIMENSIONS.NULL)
		{
		EXE_unwind (tdbb, request);
                if (key_count > idx.idx_count)
		    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__idx_key_err,
			gds_arg_string, ERR_cstring (work->dfw_name),
			0);
			/* Msg311: too many keys defined for index %s */
		else if (SEG.RDB$FIELD_POSITION > idx.idx_count)
                    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__inval_key_posn,    
			/* Msg358: invalid key position */
			gds_arg_gds, gds__field_name,
			gds_arg_string, ERR_cstring (RFR.RDB$FIELD_NAME),
			gds_arg_gds, gds__index_name,
			gds_arg_string, ERR_cstring (work->dfw_name),
			0);
		else if (FLD.RDB$FIELD_TYPE == blr_blob)
                    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__blob_idx_err,
			gds_arg_string, ERR_cstring (work->dfw_name),
			0);
		    /* Msg350: attempt to index blob column in index %s */
		else
                    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__array_idx_err,
			gds_arg_string, ERR_cstring (work->dfw_name),
			0);
		    /* Msg351: attempt to index array column in index %s */
		}
                                                                 
	    idx.idx_rpt [SEG.RDB$FIELD_POSITION].idx_field = RFR.RDB$FIELD_ID;

	    if (FLD.RDB$CHARACTER_SET_ID.NULL)
	        FLD.RDB$CHARACTER_SET_ID = CS_NONE;

	    if (!RFR.RDB$COLLATION_ID.NULL)
		collate = RFR.RDB$COLLATION_ID;
	    else if (!FLD.RDB$COLLATION_ID.NULL)
		collate = FLD.RDB$COLLATION_ID;
	    else 
		collate = COLLATE_NONE;

	    text_type = INTL_CS_COLL_TO_TTYPE (FLD.RDB$CHARACTER_SET_ID, collate);
            idx.idx_rpt [SEG.RDB$FIELD_POSITION].idx_itype = DFW_assign_index_type (work, gds_cvt_blr_dtype [FLD.RDB$FIELD_TYPE], text_type);
	END_FOR;

	if (!REQUEST (irq_c_index))
	    REQUEST (irq_c_index) = request;

	if (key_count != idx.idx_count)
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__key_field_err,
		gds_arg_string, ERR_cstring (work->dfw_name),
		0);
	    /* Msg352: too few key columns found for index %s (incorrect column name?) */
	if (!relation)
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__idx_create_err,
		gds_arg_string, ERR_cstring (work->dfw_name),
		0);
		/* Msg308: can't create index %s */

   	/* Make sure the relation info is all current */

	MET_scan_relation (tdbb, relation);

	if (relation->rel_view_rse)
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__idx_create_err,
		gds_arg_string, ERR_cstring (work->dfw_name),
		0);
		/* Msg308: can't create index %s */

	/* Actually create the index */

	partner_relation = (REL) NULL_PTR;
	if (idx.idx_flags & idx_foreign)
	    {
	    /* Get an exclusive lock on the database if the index being
	       defined enforces a foreign key constraint. This will prevent
	       the constraint from being violated during index construction. */
	       
	    if (MET_lookup_partner (tdbb, relation, &idx, work->dfw_name) &&
	    	(partner_relation = MET_lookup_relation_id (tdbb, idx.idx_primary_relation, TRUE)) &&
		!CCH_exclusive (tdbb, LCK_EX, LCK_NO_WAIT))
	    	    ERR_post (gds__no_meta_update,
			gds_arg_gds, gds__obj_in_use,
			gds_arg_string, partner_relation->rel_name, 
			0);
	    }
		    
	assert (work->dfw_id == MAX_IDX);
	IDX_create_index (tdbb, relation, &idx,
			  work->dfw_name, &work->dfw_id, transaction, &selectivity);
	assert (work->dfw_id == idx.idx_id);
	DFW_update_index (work, idx.idx_id, selectivity);

	if (partner_relation)
	    {
	    relation->rel_flags |= REL_check_partners;
	    partner_relation->rel_flags |= REL_check_partners;
	    CCH_release_exclusive (tdbb);
	    }
	
	break;
    }

return FALSE;
}

static BOOLEAN create_log (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c r e a t e _ l o g
 *
 **************************************
 *
 * Functional description
 *	Do work associated with the addition of records to RDB$LOG_FILES
 *
 **************************************/
BLK     handle;
TEXT	temp [MAX_PATH_LENGTH];
DBB     dbb;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 0:
	CCH_release_exclusive (tdbb);
	return 0;

    case 1:
    case 2:
	return TRUE;

    case 3:
        if (CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
            return TRUE;
        else
            {
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__lock_timeout,
                gds_arg_gds, gds__obj_in_use,
                gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string),
                0);
            return FALSE;
            }
    case 4:

	handle = NULL;
	if (shadow_defined (tdbb))
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__wal_shadow_err,
		0);
	    /* Msg309: Write-Ahead Log with Shadowing configuration not allowed */
        check_filename (work->dfw_name, work->dfw_name_length);

	FOR (REQUEST_HANDLE handle) X IN RDB$LOG_FILES

	    ISC_expand_filename (X.RDB$FILE_NAME, 0, temp);

	    MODIFY X USING
		strcpy (X.RDB$FILE_NAME, temp);
	    END_MODIFY;
	END_FOR;

	CMP_release (tdbb, handle);

	transaction->tra_flags |= TRA_add_log;
	PAG_modify_log (transaction->tra_number, TRA_add_log);
	return TRUE;

    case 5:
	CCH_release_exclusive (tdbb);
	break;
    }

return FALSE;
}

static BOOLEAN create_procedure (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c r e a t e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Create a new procedure.
 *
 **************************************/
BLK		request;
PRC		procedure;
USHORT		prc_id;
BLK		handle;

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	get_procedure_dependencies (work);
	if (!(procedure = MET_lookup_procedure (tdbb, work->dfw_name)))
	     return FALSE;
	procedure->prc_flags |= PRC_create;
	break;
    }

return FALSE;
}   

static BOOLEAN create_relation (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c r e a t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Create a new relation.
 *
 **************************************/
DBB		dbb;
BLK		request;
REL		relation;
USHORT		rel_id, external_flag;
GDS__QUAD	blob_id;
BLK		handle;
CSB		csb;
PLB		old_pool;
LCK		lock;
USHORT          major_version, minor_original, local_min_relation_id;


SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

if (ENCODE_ODS(major_version, minor_original) < ODS_9_0)
    local_min_relation_id = MIN_RELATION_ID;
else
    local_min_relation_id = USER_DEF_REL_INIT_ID;

switch (phase)
    {
    case 0:
	if (work->dfw_lock)
	    {
	    LCK_release (tdbb, work->dfw_lock);
	    ALL_release (work->dfw_lock);
	    work->dfw_lock = NULL;
	    }
	break;

    case 1:
    case 2:
	return TRUE;

    case 3:
	/* Take a relation lock on rel id -1 before actually
	   generating a relation id. */

	work->dfw_lock = lock = (LCK) ALLOCDV (type_lck, sizeof (SLONG));
	lock->lck_dbb = dbb;
	lock->lck_attachment = tdbb->tdbb_attachment;
	lock->lck_length = sizeof (SLONG);
	lock->lck_key.lck_long = -1;
	lock->lck_type = LCK_relation;
	lock->lck_owner_handle = LCK_get_owner_handle (tdbb, lock->lck_type);
	lock->lck_parent = dbb->dbb_lock;
	lock->lck_owner = (BLK) tdbb->tdbb_attachment;

	LCK_lock_non_blocking (tdbb, lock, LCK_EX, LCK_WAIT);

	/* Assign a relation ID and dbkey length to the new relation.
	   Probe the candidate relation ID returned from the system
	   relation RDB$DATABASE to make sure it isn't already assigned.
	   This can happen from nefarious manipulation of RDB$DATABASE
	   or wraparound of the next relation ID. Keep looking for a
	   usable relation ID until the search space is exhausted. */

	rel_id = 0;
	request = (BLK) CMP_find_request (tdbb, irq_c_relation, IRQ_REQUESTS);

	FOR (REQUEST_HANDLE request) 
		X IN RDB$DATABASE CROSS Y IN RDB$RELATIONS WITH
		Y.RDB$RELATION_NAME EQ work->dfw_name
	    if (!REQUEST (irq_c_relation))
		REQUEST (irq_c_relation) = request;

	    blob_id = Y.RDB$VIEW_BLR;
	    external_flag = Y.RDB$EXTERNAL_FILE [0];

	    MODIFY X USING
		rel_id = X.RDB$RELATION_ID;

		if (rel_id < local_min_relation_id || rel_id > MAX_RELATION_ID)
		    rel_id = X.RDB$RELATION_ID = local_min_relation_id;

		while (relation = MET_lookup_relation_id (tdbb, rel_id++, FALSE))
		    {
		    if (rel_id < local_min_relation_id || 
                        rel_id > MAX_RELATION_ID)
			rel_id = local_min_relation_id;
		    if (rel_id == X.RDB$RELATION_ID)
		    	{
			EXE_unwind (tdbb, request);
                    	ERR_post (gds__no_meta_update,
		    	    gds_arg_gds, gds__table_name,
		    	    gds_arg_string, ERR_cstring (work->dfw_name),
		    	    gds_arg_gds, gds__imp_exc,
		    	    0);
			}
		    }
		X.RDB$RELATION_ID = (rel_id > MAX_RELATION_ID) ? 
                                              local_min_relation_id : rel_id;
		MODIFY Y USING
		    Y.RDB$RELATION_ID = --rel_id;
		    if (NULL_BLOB (blob_id))
			Y.RDB$DBKEY_LENGTH = 8;
		    else
			{ 
			/* update the dbkey length to include each of the base
			   relations */

			handle = NULL;
			Y.RDB$DBKEY_LENGTH = 0;
			FOR (REQUEST_HANDLE handle) 
				Z IN RDB$VIEW_RELATIONS CROSS
				R IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH 
				Z.RDB$VIEW_NAME = work->dfw_name
			    Y.RDB$DBKEY_LENGTH += R.RDB$DBKEY_LENGTH;
			END_FOR;
			CMP_release (tdbb, handle);
			}
		END_MODIFY
	    END_MODIFY
	END_FOR;

	LCK_release (tdbb, lock);
	ALL_release (lock);
	work->dfw_lock = NULL;

	if (!REQUEST (irq_c_relation))
	    REQUEST (irq_c_relation) = request;

	/* If relation wasn't found, don't do anymore. This can happen
	   when the relation is created and deleted in the same transaction. */

	if (!rel_id)
	    break;

	/* get the relation and flag it to check for dependencies
	   in the view blr (if it exists) and any computed fields */

	relation = MET_relation (tdbb, rel_id);
	relation->rel_flags |= REL_get_dependencies;

	/* if this is not a view, create the relation */

	if (NULL_BLOB (blob_id))
	    {
	    if (!external_flag)
		DPM_create_relation (tdbb, relation);
	    }
	break;
    }

return FALSE;
}   

static BOOLEAN create_trigger (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	c r e a t e _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *	Perform required actions on creation of trigger.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	get_trigger_dependencies (work);
	break;
    }

return FALSE;
}

static BOOLEAN delete_exception (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	Check if it is allowable to delete
 *	an exception, and if so, clean up after it.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 0:
	return FALSE;

    case 1:
	check_dependencies (tdbb, work->dfw_name, NULL_PTR,
			obj_exception, transaction); 
	return TRUE;

    case 2:
	return TRUE;

    case 3:
	return TRUE;

    case 4:
	break;
    }

return FALSE;
}

static BOOLEAN delete_field (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ f i e l d
 *
 **************************************
 *
 * Functional description 
 *	This whole routine exists just to
 *	return an error if someone attempts to
 *	delete a global field that is in use
 *
 **************************************/
DBB	dbb;
int	field_count;
BLK	handle;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 1:
	/* Look up the field in RFR.  If we can't find the field,
	   go ahead with the delete. */

	handle = NULL;
	field_count = 0;

	FOR (REQUEST_HANDLE handle)
		RFR IN RDB$RELATION_FIELDS CROSS
		REL IN RDB$RELATIONS
		OVER RDB$RELATION_NAME
		WITH RFR.RDB$FIELD_SOURCE EQ work->dfw_name

	    /* If the rfr field is also being deleted, there's no dependency */

	    if (!find_depend_in_dfw (tdbb, RFR.RDB$FIELD_NAME,
					obj_computed,
					REL.RDB$RELATION_ID, transaction))
		field_count++;
	END_FOR;
	CMP_release (tdbb, handle);

	if (field_count) 
           ERR_post (gds__no_meta_update,
	  	gds_arg_gds, gds__no_delete,    /* Msg353: can not delete */
		gds_arg_gds, gds__field_name,
		gds_arg_string, ERR_cstring (work->dfw_name),
		gds_arg_gds, gds__dependency,
		gds_arg_number, (SLONG) field_count,
		0);       /* Msg310: there are %ld dependencies */

    case 2:
	return TRUE;

    case 3:
	MET_delete_dependencies (tdbb, work->dfw_name, obj_computed);
	break;
    }

return FALSE;
}

static BOOLEAN delete_global (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ g l o b a l
 *
 **************************************
 *
 * Functional description
 *	If a local field has been deleted,
 *	check to see if its global field
 *	is computed. If so, delete all its
 *	dependencies under the assumption
 *	that a global computed field has only
 *	one local field.
 *
 **************************************/
DBB	dbb;
BLK	handle;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	handle = NULL;
	FOR (REQUEST_HANDLE handle) 
	      FLD IN RDB$FIELDS WITH 
	      FLD.RDB$FIELD_NAME EQ work->dfw_name AND
	      FLD.RDB$COMPUTED_BLR NOT MISSING
	    MET_delete_dependencies (tdbb, work->dfw_name, obj_computed);
	END_FOR;
	CMP_release (tdbb, handle);
	break;
    }

return FALSE;
}

static BOOLEAN delete_index (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
IDL	index, *ptr;
IDB	index_block, *iptr;
REL	relation;
USHORT	id, wait;

SET_TDBB (tdbb);

switch (phase)
    {
    case 0:
	if (!(relation = MET_lookup_relation (tdbb, work->dfw_name)))
	    return FALSE;
	id = work->dfw_id - 1;
	if (index = CMP_get_index_lock (tdbb, relation, id))
	    if (!index->idl_count)
		LCK_release (tdbb, index->idl_lock);
	return FALSE;

    case 1:
    case 2:
	return TRUE;

    case 3:
	/* Look up the relation.  If we can't find the relation,
	   don't worry about the index. */

	if (!(relation = MET_lookup_relation (tdbb, work->dfw_name)))
	    return FALSE;

	/* Make sure nobody is currently using the index */

	id = work->dfw_id - 1;
	if (index = CMP_get_index_lock (tdbb, relation, id))
	    {
	    wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	    if (index->idl_count || !LCK_lock_non_blocking (tdbb, index->idl_lock, LCK_EX, wait))
	    	ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__obj_in_use, gds_arg_string, "INDEX", 
		    0);
	    index->idl_count++;
	    }
    
	return TRUE;

    case 4:
	if (!(relation = MET_lookup_relation (tdbb, work->dfw_name)))
	    return FALSE;

	id = work->dfw_id - 1;
	index = CMP_get_index_lock (tdbb, relation, id);
	IDX_delete_index (tdbb, relation, id);
	if (work->dfw_type == dfw_delete_expression_index)
            MET_delete_dependencies (tdbb, work->dfw_name, obj_expression_index);

	if (index)
	    {
	    /* in order for us to have gotten the lock in phase 3
             * idl_count HAD to be 0, therefore after having incremented
             * it for the exclusive lock it would have to be 1.
	     * IF now it is NOT 1 then someone else got a lock to
	     * the index and something is seriously wrong */ 
	    assert (index->idl_count == 1);
	    if (!--index->idl_count)
		{
		/* Release index existence lock and memory. */

	    	for (ptr = &relation->rel_index_locks; *ptr; ptr = &(*ptr)->idl_next)
	    	    if (*ptr == index)
		    	{
		    	*ptr = index->idl_next;
		    	break;
		    	}
	    	if (index->idl_lock)
		    {
		    LCK_release (tdbb, index->idl_lock);
		    ALL_release (index->idl_lock);
		    }
	    	ALL_release (index);

		/* Release index refresh lock and memory. */

	    	for (iptr = &relation->rel_index_blocks; *iptr; iptr = &(*iptr)->idb_next)
	    	    if ((*iptr)->idb_id == id)
		    	{
			index_block = *iptr;
		    	*iptr = index_block->idb_next;

			/* Lock was released in IDX_delete_index(). */

	    		if (index_block->idb_lock)
		    	    ALL_release (index_block->idb_lock);
	    		ALL_release (index_block);
		    	break;
		    	}
		}
	    }
	break;
    }

return FALSE;
}

static BOOLEAN delete_log (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ l o g
 *
 **************************************
 *
 * Functional description
 *	Do work associated with the deletion of records in RDB$LOG_FILES
 *
 **************************************/
DBB	dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
        if (CCH_exclusive (tdbb, LCK_EX, WAIT_PERIOD))
            return TRUE;
        else
            {
            ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__lock_timeout,
                gds_arg_gds, gds__obj_in_use,
                gds_arg_string, ERR_cstring (dbb->dbb_file->fil_string),
                0);
            return FALSE;
            }
    case 4:
	transaction->tra_flags |= TRA_delete_log;
	PAG_modify_log (transaction->tra_number, TRA_delete_log);
	return TRUE;

    case 5:
	CCH_release_exclusive (tdbb);
	break;
    }

return FALSE;
}

static BOOLEAN delete_parameter (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description 
 *	Return an error if someone attempts to
 *	delete a field from a procedure and it is
 *	used by a view or procedure.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
	if (MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0))
	    check_dependencies (tdbb, work->dfw_name, work->dfw_name,
			obj_procedure, transaction); 

    case 2:
	return TRUE;

    case 3:
	break;
    }

return FALSE;
}

static BOOLEAN delete_procedure (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Check if it is allowable to delete
 *	a procedure , and if so, clean up after it.
 *
 **************************************/
PRC	procedure;
USHORT	wait;
USHORT  old_flags;

SET_TDBB (tdbb);

switch (phase)
    {
    case 0:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0)))
	    return FALSE;

	wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	LCK_convert_non_blocking (tdbb, procedure->prc_existence_lock, LCK_SR, wait);
	return FALSE;

    case 1:
	check_dependencies (tdbb, work->dfw_name, NULL_PTR,
			obj_procedure, transaction); 
	return TRUE;

    case 2:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0)))
	    return FALSE;

	wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	if (!LCK_convert_non_blocking (tdbb, procedure->prc_existence_lock,
			LCK_EX, wait))
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__obj_in_use, 
		gds_arg_string, ERR_cstring (work->dfw_name), 
		0);

	/* If we are in a multi-client server, someone else may have marked
	   procedure obsolete. Unmark and we will remark it later. */

	procedure->prc_flags &= ~PRC_obsolete;
	return TRUE;

    case 3:
	return TRUE;

    case 4:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, TRUE, 0)))
	    return FALSE;

        if (procedure->prc_use_count)
	    {
	    MET_delete_dependencies (tdbb, work->dfw_name, obj_procedure);

	    LCK_release (tdbb, procedure->prc_existence_lock);
	    tdbb->tdbb_database->dbb_procedures->vec_object[procedure->prc_id] = NULL_PTR;
	    return FALSE;
	    }

	old_flags = procedure->prc_flags;
	procedure->prc_flags |= PRC_obsolete;
	if (procedure->prc_request)
	    {
	    if (CMP_clone_active (procedure->prc_request))
		{
	        procedure->prc_flags = old_flags;
		ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__obj_in_use,
		    gds_arg_string, ERR_cstring (work->dfw_name),
		    0);
		}

	    CMP_release (tdbb, procedure->prc_request);
	    procedure->prc_request = 0;
	    }

	/* delete dependency lists */

	MET_delete_dependencies (tdbb, work->dfw_name, obj_procedure);

	LCK_release (tdbb, procedure->prc_existence_lock);
	break;
    }

return FALSE;
}

static BOOLEAN delete_relation (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Check if it is allowable to delete
 *	a relation, and if so, clean up after it.
 *
 **************************************/
DBB	dbb;
BLK	request;
REL	relation;
RSC	rsc;
USHORT	wait, view_count, adjusted;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 0:
	if (!(relation = MET_lookup_relation_id (tdbb, work->dfw_id, FALSE)))
	    return FALSE;

	if (relation->rel_existence_lock)
	    {
 	    wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	    LCK_convert_non_blocking (tdbb, relation->rel_existence_lock, LCK_SR, wait);
	    }

	relation->rel_flags &= ~REL_deleting;
	return FALSE;

    case 1:
	/* check if any views use this as a base relation */

	request = NULL;
	view_count = 0;
	FOR (REQUEST_HANDLE request) 
	    X IN RDB$VIEW_RELATIONS WITH
	    X.RDB$RELATION_NAME EQ work->dfw_name

	    /* If the view is also being deleted, there's no dependency */

	    if (!find_depend_in_dfw (tdbb, X.RDB$VIEW_NAME, obj_view, 0,
					transaction))
		view_count++;

	END_FOR;
	CMP_release (tdbb, request);

        if (view_count)
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__no_delete,    /* Msg353: can not delete */
		gds_arg_gds, gds__table_name, 
		gds_arg_string, ERR_cstring (work->dfw_name),
		gds_arg_gds, gds__dependency,
		gds_arg_number, (SLONG) view_count,
		0);	    /* Msg310: there are %ld dependencies */
	check_dependencies (tdbb, work->dfw_name, NULL_PTR,
			obj_relation, transaction); 
	return TRUE;

    case 2:
	if (!(relation = MET_lookup_relation_id (tdbb, work->dfw_id, FALSE)))
	    return FALSE;

	/* Let relation be deleted if only this transaction is using it */

	adjusted = FALSE;
	if (relation->rel_use_count == 1)
	    for (rsc = transaction->tra_resources; rsc; rsc = rsc->rsc_next)
		if (rsc->rsc_rel == relation)
		    {
		    --relation->rel_use_count;
		    adjusted = TRUE;
		    break;
		    }

	wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	if (relation->rel_use_count ||
	    (relation->rel_existence_lock &&
	     !LCK_convert_non_blocking (tdbb, relation->rel_existence_lock, LCK_EX, wait)))
	    {
	    if (adjusted)
		++relation->rel_use_count;
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__obj_in_use,
		gds_arg_string, ERR_cstring (work->dfw_name), 
		0);
	    }
	return TRUE;

    case 3:
	return TRUE;

    case 4:
	if (!(relation = MET_lookup_relation_id (tdbb, work->dfw_id, TRUE)))
	    return FALSE;

	/* Flag relation delete in progress so that active sweep or
	   garbage collector threads working on relation can skip over it. */

	relation->rel_flags |= REL_deleting;

	/* The sweep and garbage collector threads have no more than
	   a single record latency in responding to the flagged relation
	   deletion. Nevertheless, as a defensive programming measure,
	   don't wait forever if something has gone awry and the sweep
	   count doesn't run down. */

	for (wait = 0; wait < 60; wait++)
	    if (!relation->rel_sweep_count)
		break;
	    else
		{
		THREAD_EXIT;
		THREAD_SLEEP(1*1000);
		THREAD_ENTER;
		}

	if (relation->rel_sweep_count)
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__obj_in_use,
		gds_arg_string, ERR_cstring (work->dfw_name), 
		0);

#ifdef GARBAGE_THREAD
	/* Free any memory associated with the relation's
	   garbage collection bitmap. */

	if (relation->rel_gc_bitmap)
	    {
	    SBM_release (relation->rel_gc_bitmap);
	    relation->rel_gc_bitmap = NULL_PTR;
	    }
#endif
	if (relation->rel_index_root)
	    IDX_delete_indices (tdbb, relation);

	if (relation->rel_pages)
	    DPM_delete_relation (tdbb, relation);

	/* if this is a view (or even if we don't know),
	   delete dependency lists */

	if (relation->rel_view_rse || !(relation->rel_flags & REL_scanned))
	    MET_delete_dependencies (tdbb, work->dfw_name, obj_view);

	/* Now that the data, pointer, and index pages are gone,
	   get rid of formats as well */

	if (relation->rel_existence_lock)
	    LCK_release (tdbb, relation->rel_existence_lock);
	request = NULL;

	FOR (REQUEST_HANDLE request) X IN RDB$FORMATS WITH 
		X.RDB$RELATION_ID EQ relation->rel_id
	    ERASE X;
	END_FOR;

	relation->rel_name = NULL;
	relation->rel_flags |= REL_deleted;
	relation->rel_flags &= ~REL_deleting;
	CMP_release (tdbb, request);
	break;
    }

return FALSE;
}

static BOOLEAN delete_rfr (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ r f r
 *
 **************************************
 *
 * Functional description 
 *	This whole routine exists just to
 *	return an error if someone attempts to
 *	1. delete a field from a relation if the relation
 *		is used in a view and the field is referenced in
 *		the view.  
 *	2. drop the last column of a table
 *
 **************************************/
DBB	dbb;
int	rel_exists, field_count, id;
BLK	handle;
TEXT	f [32], *p, *q;
REL	relation;
VEC	vector;
USHORT	dep_counts [obj_count], i;

SET_TDBB (tdbb);
dbb  = tdbb->tdbb_database;

switch (phase)
    {
    case 1:
	/* first check if there are any fields used explicitly by the view */

	handle = NULL;
	field_count = 0;
	FOR (REQUEST_HANDLE handle)
		REL IN RDB$RELATIONS CROSS
		VR IN RDB$VIEW_RELATIONS OVER RDB$RELATION_NAME CROSS
		VFLD IN RDB$RELATION_FIELDS WITH  
		REL.RDB$RELATION_ID EQ work->dfw_id AND
		VFLD.RDB$VIEW_CONTEXT EQ VR.RDB$VIEW_CONTEXT AND
                VFLD.RDB$RELATION_NAME EQ VR.RDB$VIEW_NAME AND
		VFLD.RDB$BASE_FIELD EQ work->dfw_name 

	    /* If the view is also being deleted, there's no dependency */

	    if (!find_depend_in_dfw (tdbb, VR.RDB$VIEW_NAME, obj_view,
					0, transaction))
		{
		for (p = f, q = VFLD.RDB$BASE_FIELD; *q && *q != ' '; *p++ = *q++)
		    ;
		*p = 0;
		field_count++;
		}
	END_FOR;
	CMP_release (tdbb, handle);

        if (field_count)
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__no_delete,    /* Msg353: can not delete */
		gds_arg_gds, gds__field_name, gds_arg_string, ERR_cstring (f),
		gds_arg_gds, gds__dependency, 
		gds_arg_number, (SLONG) field_count,
		0);       /* Msg310: there are %ld dependencies */

	/* now check if there are any dependencies generated through the blr
	   that defines the relation */
	     
	if (relation = MET_lookup_relation_id (tdbb, work->dfw_id, FALSE))
	    check_dependencies (tdbb, relation->rel_name, work->dfw_name,
				obj_relation, transaction);

	/* see if the relation itself is being dropped */

	handle = NULL;
	rel_exists = 0;
	FOR (REQUEST_HANDLE handle)
		REL IN RDB$RELATIONS WITH REL.RDB$RELATION_ID EQ work->dfw_id
	    rel_exists++;
	END_FOR;
	if (handle)
	    CMP_release (tdbb, handle);

	/* if table exists, check if this is the last column in the table */

	if (rel_exists)
	    {
	    field_count = 0;
	    handle = NULL;

	    FOR (REQUEST_HANDLE handle)
		REL IN RDB$RELATIONS CROSS
		RFLD IN RDB$RELATION_FIELDS OVER RDB$RELATION_NAME WITH
		REL.RDB$RELATION_ID EQ work->dfw_id

		field_count++;
	    END_FOR;
	    if (handle)
		CMP_release (tdbb, handle);

	    if (!field_count)
                ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__del_last_field,
		    0);
		    /* Msg354: last column in a relation cannot be deleted */
	    }

    case 2:
	return TRUE;

    case 3:
	/* Unlink field from data structures.  Don't try to actually release field and
	   friends -- somebody may be pointing to them */

	if ((relation = MET_lookup_relation_id (tdbb, work->dfw_id, FALSE)) &&
	    (id = MET_lookup_field (tdbb, relation, work->dfw_name)) >= 0 &&
	    (vector = relation->rel_fields) &&
	    id < vector->vec_count &&
	    vector->vec_object [id])
	    {
	    vector->vec_object [id] = NULL;
	    }
	break;
    }

return FALSE;
}

static BOOLEAN delete_shadow (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ s h a d o w
 *
 **************************************
 *
 * Functional description 
 *	Provide deferred work interface to 
 *	MET_delete_shadow.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	MET_delete_shadow (tdbb, work->dfw_id);
	break;
    }

return FALSE;
}

static BOOLEAN delete_trigger (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	d e l e t e _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *	Cleanup after a deleted trigger.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	/* get rid of dependencies */

	MET_delete_dependencies (tdbb, work->dfw_name, obj_trigger);
	break;
    }

return FALSE;
}

static BOOLEAN find_depend_in_dfw (
    TDBB	tdbb,
    TEXT	*object_name,
    USHORT	dep_type,
    USHORT	rel_id,
    TRA		transaction)
{
/**************************************
 *
 *	f i n d _ d e p e n d _ i n _ d f w
 *
 **************************************
 *
 * Functional description
 *	Check the object to see if it is being
 *	deleted as part of the deferred work.
 *	Return FALSE if it is, TRUE otherwise.
 *
 **************************************/
DBB		dbb;
BLK		request;
TEXT		*p;
ENUM dfw_t	dfw_type;
DFW		work;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

for (p = object_name; *p && *p != ' '; p++)
    ;
*p = 0;

switch (dep_type)
    {
    case obj_view:
	dfw_type = dfw_delete_relation;
	break;
    case obj_trigger:
	dfw_type = dfw_delete_trigger;
	break;
    case obj_computed:
	dfw_type = (rel_id) ? dfw_delete_rfr : dfw_delete_global;
	break;
    case obj_procedure:
	dfw_type = dfw_delete_procedure;
	break;
    case obj_expression_index:
	dfw_type = dfw_delete_index;
	break;  
    default:
	assert (FALSE);
	break;
    }

/* Look to see if an object of the desired type is being deleted. */

for (work = transaction->tra_deferred_work; work; work = work->dfw_next)
    if (work->dfw_type == dfw_type &&
	!strcmp (object_name, work->dfw_name) &&
	(!rel_id || rel_id == work->dfw_id))
	return TRUE;

if (dfw_type == dfw_delete_global)
    {
    /* Computed fields are more complicated.  If the global field isn't being
       deleted, see if all of the fields it is the source for, are. */

    request = (BLK) CMP_find_request (tdbb, irq_ch_cmp_dpd, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request)
	    FLD IN RDB$FIELDS CROSS
	    RFR IN RDB$RELATION_FIELDS CROSS
	    REL IN RDB$RELATIONS
	    WITH FLD.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE
	    AND FLD.RDB$FIELD_NAME EQ object_name
	    AND REL.RDB$RELATION_NAME EQ RFR.RDB$RELATION_NAME
	if (!REQUEST (irq_ch_cmp_dpd))
	    REQUEST (irq_ch_cmp_dpd) = request;
	if (!find_depend_in_dfw (tdbb, RFR.RDB$FIELD_NAME, obj_computed,
				 REL.RDB$RELATION_ID, transaction))
	    {
	    EXE_unwind (tdbb, request);
	    return FALSE;
	    }
    END_FOR;

    if (!REQUEST (irq_ch_cmp_dpd))
	REQUEST (irq_ch_cmp_dpd) = request;

    return TRUE;
    }

return FALSE;
}

static void get_array_desc (
    TDBB	tdbb,
    TEXT	*field_name,
    ADS		desc)
{
/**************************************
 *
 *	g e t _ a r r a y _ d e s c
 *
 **************************************
 *
 * Functional description
 *	Get array descriptor for an array.
 *
 **************************************/
DBB			dbb;
BLK			request;
struct ads_repeat	*ranges;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_r_fld_dim, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
    D IN RDB$FIELD_DIMENSIONS WITH D.RDB$FIELD_NAME EQ field_name
    if (!REQUEST (irq_r_fld_dim))
	REQUEST (irq_r_fld_dim) = request;
    if (D.RDB$DIMENSION >= 0 && D.RDB$DIMENSION < desc->ads_dimensions)
	{
	ranges = desc->ads_rpt + D.RDB$DIMENSION;
	ranges->ads_lower = D.RDB$LOWER_BOUND;
	ranges->ads_upper = D.RDB$UPPER_BOUND;
	}
END_FOR;

if (!REQUEST (irq_r_fld_dim))
    REQUEST (irq_r_fld_dim) = request;

desc->ads_count = 1;

for (ranges = desc->ads_rpt + desc->ads_dimensions; --ranges >= desc->ads_rpt;)
    {
    ranges->ads_length = desc->ads_count;
    desc->ads_count *= ranges->ads_upper - ranges->ads_lower + 1;
    }

desc->ads_version = ADS_VERSION_1;
desc->ads_length = ADS_LEN (MAX (desc->ads_struct_count, desc->ads_dimensions));
desc->ads_element_length = desc->ads_rpt [0].ads_desc.dsc_length;
desc->ads_total_length = desc->ads_element_length * desc->ads_count;
}

static void get_procedure_dependencies (
    DFW	work)
{
/**************************************
 *
 *	g e t _ p r o c e d u r e _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *	Get relations and fields on which this
 *	procedure depends, either when it's being
 *	created or when it's modified.
 *
 **************************************/
TDBB		tdbb;
DBB		dbb;
PRC		procedure;
GDS__QUAD	blob_id;
REQ		request;
BLK		handle;      
PLB		old_pool;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

procedure = NULL;

handle = (BLK) CMP_find_request (tdbb, irq_c_prc_dpd, IRQ_REQUESTS);

FOR (REQUEST_HANDLE handle) 
	X IN RDB$PROCEDURES WITH
	X.RDB$PROCEDURE_NAME EQ work->dfw_name

    if (!REQUEST (irq_c_prc_dpd))
	REQUEST (irq_c_prc_dpd) = handle;
    blob_id = X.RDB$PROCEDURE_BLR;
    procedure = MET_lookup_procedure (tdbb, work->dfw_name);

END_FOR;

if (!REQUEST (irq_c_prc_dpd))
    REQUEST (irq_c_prc_dpd) = handle;

/* get any dependencies now by parsing the blr */

if (procedure && !NULL_BLOB (blob_id))
    {
    old_pool = tdbb->tdbb_default;
    tdbb->tdbb_default = ALL_pool();
    MET_get_dependencies (tdbb, NULL_PTR, 
		      NULL_PTR, 
		      NULL_PTR, 
		      &blob_id,
		      &request, 
		      NULL_PTR, 
		      work->dfw_name, 
		      obj_procedure);
    if (request)
	CMP_release (tdbb, request);
    else
	ALL_rlpool (tdbb->tdbb_default);

    tdbb->tdbb_default = old_pool;
    }
}

static void get_trigger_dependencies (
    DFW	work)
{
/**************************************
 *
 *	g e t _ t r i g g e r _ d e p e n d e n c i e s
 *
 **************************************
 *
 * Functional description
 *	Get relations and fields on which this
 *	trigger depends, either when it's being
 *	created or when it's modified.
 *
 **************************************/
TDBB		tdbb;
DBB		dbb;
REL		relation;
GDS__QUAD	blob_id;
REQ		request;
BLK		handle;      
PLB		old_pool;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

relation = NULL;

handle = (BLK) CMP_find_request (tdbb, irq_c_trigger, IRQ_REQUESTS);

FOR (REQUEST_HANDLE handle) 
	X IN RDB$TRIGGERS WITH
	X.RDB$TRIGGER_NAME EQ work->dfw_name

    if (!REQUEST (irq_c_trigger))
	REQUEST (irq_c_trigger) = handle;
    blob_id = X.RDB$TRIGGER_BLR;
    relation = MET_lookup_relation (tdbb, X.RDB$RELATION_NAME);

END_FOR;

if (!REQUEST (irq_c_trigger))
    REQUEST (irq_c_trigger) = handle;

/* get any dependencies now by parsing the blr */

if (relation && !NULL_BLOB (blob_id))
    {
    old_pool = tdbb->tdbb_default;
    tdbb->tdbb_default = ALL_pool();
    MET_get_dependencies (tdbb, relation, 
		      NULL_PTR, 
		      NULL_PTR, 
		      &blob_id,
		      &request, 
		      NULL_PTR, 
		      work->dfw_name, 
		      obj_trigger);
    ALL_rlpool (tdbb->tdbb_default);
    tdbb->tdbb_default = old_pool;
    }
}

static FMT make_format (
    TDBB	tdbb,
    REL		relation,
    USHORT	version,
    TFB		stack)
{
/**************************************
 *
 *	m a k e _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *	Make a format block for a relation.
 *
 **************************************/
DBB	dbb;
BLK	request;
FMT	format;
VEC	vector;
TFB	tfb;
BLB	blob;
USHORT	count, dtype;
ULONG	offset, old_offset;
DSC	*desc;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

/* Figure out the highest field id and allocate a format block */

count = 0;
for (tfb = stack; tfb; tfb = tfb->tfb_next)
    count = MAX (count, tfb->tfb_id);

format = (FMT) ALLOCPV (type_fmt, count + 1);
format->fmt_count = count + 1;
format->fmt_version = version;

/* Fill in the format block from the temporary field blocks */

for (tfb = stack; tfb; tfb = tfb->tfb_next)
    {
    desc = format->fmt_desc + tfb->tfb_id;
    if (tfb->tfb_flags & TFB_array)
	{
	desc->dsc_dtype = dtype_array;
	desc->dsc_length = sizeof (GDS__QUAD);
	}
    else
	*desc = tfb->tfb_desc;
    if (tfb->tfb_flags & TFB_computed)
	desc->dsc_dtype |= COMPUTED_FLAG;
    }

/* Compute the offsets of the various fields */

offset = FLAG_BYTES (count);

for (count = 0, desc = format->fmt_desc; count < format->fmt_count; count++, desc++)
    {
    if (desc->dsc_dtype & COMPUTED_FLAG)
	{
	desc->dsc_dtype &= ~COMPUTED_FLAG;
	continue;
	}
    if (desc->dsc_dtype)
	{
	old_offset = offset;
	offset = MET_align (desc, (USHORT)offset);

	/* see of record too big */

	if ( offset < old_offset)
	    {
	    offset += 65536L;
	    break;
	    }
	desc->dsc_address = (UCHAR*) (SLONG) offset;
	offset += desc->dsc_length;
	}
    }

format->fmt_length = offset;

/* Release the temporary field blocks */

while (tfb = stack)
    {
    stack = tfb->tfb_next;
    ALL_release (tfb);
    }

if (offset > MAX_FORMAT_SIZE)
    {
    ALL_release (format);
    ERR_post (gds__no_meta_update,
	gds_arg_gds, gds__rec_size_err, gds_arg_number, (SLONG) offset,
	gds_arg_gds, gds__table_name, gds_arg_string, ERR_cstring (relation->rel_name),
	0);
	/* Msg361: new record size of %ld bytes is too big */
    }

/* Link the format block into the world */

vector = ALL_vector (dbb->dbb_permanent, &relation->rel_formats, version);
vector->vec_object [version] = (BLK) format;

/* Store format in system relation */

request = (BLK) CMP_find_request (tdbb, irq_format3, IRQ_REQUESTS);

STORE (REQUEST_HANDLE request)
	FMT IN RDB$FORMATS
    if (!REQUEST (irq_format3))
	REQUEST (irq_format3) = request;
    FMT.RDB$FORMAT = version;
    FMT.RDB$RELATION_ID = relation->rel_id;
    blob = BLB_create (tdbb, dbb->dbb_sys_trans, &FMT.RDB$DESCRIPTOR);
    BLB_put_segment (tdbb, blob, format->fmt_desc, format->fmt_count * sizeof (struct dsc));
    BLB_close (tdbb, blob);
END_STORE;

if (!REQUEST (irq_format3))
    REQUEST (irq_format3) = request;

return format;
}

static BOOLEAN make_version (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	m a k e _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Make a new format version for a relation.  While we're at it, make
 *	sure all fields have id's.  If the relation is a view, make a
 *	a format anyway -- used for view updates.
 *
 *	While we're in the vicinity, also check the updatability of fields.
 *
 **************************************/
DBB		dbb;
BLK		request_fmt1, request_fmtx;
TFB		stack, external, tfb;
REL		relation;
BLB		blob;
GDS__QUAD blob_id;
BLK		temp;
UCHAR		*p, *q;
USHORT		version, length, n, external_flag;
BOOLEAN	 null_view, computed_field;
SLONG		stuff [64];
ADS		array;
STATUS		status [20];
SSHORT		collation;
VEC		triggers [TRIGGER_MAX], tmp_vector;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	relation = NULL;
	stack = external = NULL;
	computed_field = FALSE;

	for (n = 0; n < TRIGGER_MAX; n++)
	    triggers [n] = NULL_PTR;

	request_fmt1 = (BLK) CMP_find_request (tdbb, irq_format1, IRQ_REQUESTS);

	FOR (REQUEST_HANDLE request_fmt1) 
		REL IN RDB$RELATIONS WITH REL.RDB$RELATION_NAME EQ work->dfw_name
	    if (!REQUEST (irq_format1))
		REQUEST (irq_format1) = request_fmt1;
	    if (!(relation = MET_lookup_relation_id (tdbb, REL.RDB$RELATION_ID, FALSE)))
		{
		EXE_unwind (tdbb, request_fmt1);
		return FALSE;
		}
	    blob_id = REL.RDB$VIEW_BLR;
	    null_view = NULL_BLOB (blob_id);
	    external_flag = REL.RDB$EXTERNAL_FILE [0];
            if (REL.RDB$FORMAT == 255)
		{
		EXE_unwind (tdbb, request_fmt1);
                ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__table_name,
		    gds_arg_string, ERR_cstring (work->dfw_name),
		    gds_arg_gds, gds__version_err,
		    0);
		    /* Msg357: too many versions */
		}
	    MODIFY REL USING
		version = ++REL.RDB$FORMAT;
		blob = BLB_create (tdbb, dbb->dbb_sys_trans, &REL.RDB$RUNTIME);
		request_fmtx = (BLK) CMP_find_request (tdbb, irq_format2, IRQ_REQUESTS);
		FOR (REQUEST_HANDLE request_fmtx)
		    RFR IN RDB$RELATION_FIELDS CROSS 
		    FLD IN RDB$FIELDS WITH
			RFR.RDB$RELATION_NAME EQ work->dfw_name AND
			RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME

		    if (!REQUEST (irq_format2))
			REQUEST (irq_format2) = request_fmtx;

		    /* Update RFR to reflect new fields id */

		    if (!RFR.RDB$FIELD_ID.NULL &&
			RFR.RDB$FIELD_ID >= REL.RDB$FIELD_ID)
			REL.RDB$FIELD_ID = RFR.RDB$FIELD_ID + 1;

		    if (RFR.RDB$FIELD_ID.NULL || RFR.RDB$UPDATE_FLAG.NULL)
			MODIFY RFR USING
			    if (RFR.RDB$FIELD_ID.NULL)
				{
				if (external_flag)
				    {
				    RFR.RDB$FIELD_ID = RFR.RDB$FIELD_POSITION;
				    /* RFR.RDB$FIELD_POSITION.NULL is
				    needed to be referenced in the
				    code somewhere for GPRE to include
				    this field in the structures that
				    it generates at the top of this func. */
				    RFR.RDB$FIELD_ID.NULL = RFR.RDB$FIELD_POSITION.NULL;
				    }
				else
				    {
				    RFR.RDB$FIELD_ID = REL.RDB$FIELD_ID;
				    RFR.RDB$FIELD_ID.NULL = FALSE;
				    }
				REL.RDB$FIELD_ID++;
				}
			    if (RFR.RDB$UPDATE_FLAG.NULL)
				{
				RFR.RDB$UPDATE_FLAG.NULL = FALSE;
				RFR.RDB$UPDATE_FLAG = 1;
				if (!NULL_BLOB (FLD.RDB$COMPUTED_BLR))
				    {
				    RFR.RDB$UPDATE_FLAG = 0;
				    computed_field = TRUE;
				    }
				if (!null_view && REL.RDB$DBKEY_LENGTH > 8)
				    {
				    temp = NULL;
				    RFR.RDB$UPDATE_FLAG = 0;
				    FOR (REQUEST_HANDLE temp) X IN RDB$TRIGGERS WITH 
					    X.RDB$RELATION_NAME EQ work->dfw_name AND
					    X.RDB$TRIGGER_TYPE EQ 1
					RFR.RDB$UPDATE_FLAG = 1;
				    END_FOR;
				    CMP_release (tdbb, temp);
				    }
				}
			END_MODIFY;

		    /* Store stuff in field summary */

		    n = RFR.RDB$FIELD_ID;
		    put_summary_record (blob, RSR_field_id, &n, sizeof (n));
		    put_summary_record (blob, RSR_field_name, 
			RFR.RDB$FIELD_NAME, name_length (RFR.RDB$FIELD_NAME));
		    if (!NULL_BLOB (FLD.RDB$COMPUTED_BLR) && !RFR.RDB$VIEW_CONTEXT)
			put_summary_blob (blob, RSR_computed_blr, &FLD.RDB$COMPUTED_BLR);
		    else if (!null_view)
			{
			n = RFR.RDB$VIEW_CONTEXT;
			put_summary_record (blob, RSR_view_context, &n, sizeof (n));
			put_summary_record (blob, RSR_base_field,
				RFR.RDB$BASE_FIELD, name_length (RFR.RDB$BASE_FIELD));
			}
		    put_summary_blob (blob, RSR_missing_value, &FLD.RDB$MISSING_VALUE);
		    put_summary_blob (blob, RSR_default_value, 
			(NULL_BLOB (RFR.RDB$DEFAULT_VALUE)) ? &FLD.RDB$DEFAULT_VALUE : &RFR.RDB$DEFAULT_VALUE);
		    put_summary_blob (blob, RSR_validation_blr, &FLD.RDB$VALIDATION_BLR);
                    if (FLD.RDB$NULL_FLAG || RFR.RDB$NULL_FLAG)
                          put_summary_record (blob, RSR_field_not_null,
                                              nonnull_validation_blr,
                                              sizeof (nonnull_validation_blr));

		    n = name_length (RFR.RDB$SECURITY_CLASS);
		    if (!RFR.RDB$SECURITY_CLASS.NULL && n)
			put_summary_record (blob, RSR_security_class, RFR.RDB$SECURITY_CLASS, n);

		    /* Make a temporary field block */

		    tfb = (TFB) ALLOCD (type_tfb);
		    tfb->tfb_next = stack;
		    stack = tfb;

		    /* for text data types, grab the CHARACTER_SET and
		       COLLATION to give the type of international text */

		    if (FLD.RDB$CHARACTER_SET_ID.NULL)
		        FLD.RDB$CHARACTER_SET_ID = CS_NONE;

		    collation = COLLATE_NONE;	/* codepoint collation */
		    if (!FLD.RDB$COLLATION_ID.NULL)
			collation = FLD.RDB$COLLATION_ID;
		    if (!RFR.RDB$COLLATION_ID.NULL)
			collation = RFR.RDB$COLLATION_ID;

		    DSC_make_descriptor (&tfb->tfb_desc, FLD.RDB$FIELD_TYPE,
			FLD.RDB$FIELD_SCALE, FLD.RDB$FIELD_LENGTH, 
			FLD.RDB$FIELD_SUB_TYPE, FLD.RDB$CHARACTER_SET_ID, 
			collation);

		    /* Make sure the text type specified is implemented */
		    if ((IS_DTYPE_ANY_TEXT (tfb->tfb_desc.dsc_dtype) &&
			!INTL_defined_type (tdbb, status, tfb->tfb_desc.dsc_ttype))
			||
			(tfb->tfb_desc.dsc_dtype == dtype_blob && 
			 tfb->tfb_desc.dsc_sub_type == BLOB_text && 
			 !INTL_defined_type (tdbb, status, tfb->tfb_desc.dsc_scale)))
			{
			    EXE_unwind (tdbb, request_fmt1);
			    EXE_unwind (tdbb, request_fmtx);
    			    ERR_post (gds__no_meta_update, 
			        gds_arg_gds, gds__random, 
				gds_arg_string, ERR_cstring (work->dfw_name), 
			        status [0], status [1], status [2], status [3],
			        0);
		    	}

		    if (!NULL_BLOB (FLD.RDB$COMPUTED_BLR))
			tfb->tfb_flags |= TFB_computed;
		    tfb->tfb_id = RFR.RDB$FIELD_ID;

		    if (n = FLD.RDB$DIMENSIONS)
			{
			put_summary_record (blob, RSR_dimensions, &n, sizeof (n));
			tfb->tfb_flags |= TFB_array;
			array = (ADS) stuff;
			MOVE_CLEAR (array, (SLONG) sizeof (struct ads));
			array->ads_dimensions = n;
			array->ads_struct_count = 1;
			array->ads_rpt[0].ads_desc = tfb->tfb_desc;
			get_array_desc (tdbb, FLD.RDB$FIELD_NAME, array);
			put_summary_record (blob, RSR_array_desc, array, array->ads_length);
			}

		    if (external_flag)
			{
			tfb = (TFB) ALLOCD (type_tfb);
			tfb->tfb_next = external;
			external = tfb;
			tfb->tfb_desc.dsc_dtype = FLD.RDB$EXTERNAL_TYPE;
			tfb->tfb_desc.dsc_scale = FLD.RDB$EXTERNAL_SCALE;
			tfb->tfb_desc.dsc_length = FLD.RDB$EXTERNAL_LENGTH;
			tfb->tfb_id = RFR.RDB$FIELD_ID;
			}
		END_FOR;

		if (!REQUEST (irq_format2))
		    REQUEST (irq_format2) = request_fmtx;

		request_fmtx = (BLK) CMP_find_request (tdbb, irq_format4, IRQ_REQUESTS);
		FOR (REQUEST_HANDLE request_fmtx) 
			TRG IN RDB$TRIGGERS 
			WITH TRG.RDB$RELATION_NAME = work->dfw_name AND
			TRG.RDB$SYSTEM_FLAG != 0 AND TRG.RDB$SYSTEM_FLAG NOT MISSING
			SORTED BY TRG.RDB$TRIGGER_SEQUENCE

		    if (!REQUEST (irq_format4))
			REQUEST (irq_format4) = request_fmtx;

		    if (!TRG.RDB$TRIGGER_INACTIVE)
			{
			put_summary_record (blob, RSR_trigger_name, 
			    TRG.RDB$TRIGGER_NAME, name_length (TRG.RDB$TRIGGER_NAME));

			/* for a view, load the trigger temporarily -- 
			   this is inefficient since it will just be reloaded
			   in MET_scan_relation () but it needs to be done
			   in case the view would otherwise be non-updatable */

			if (!null_view)
			    {
			    if (!relation->rel_name)
				relation->rel_name = MET_save_name (tdbb, TRG.RDB$RELATION_NAME);
			    MET_load_trigger (tdbb, relation,
						TRG.RDB$TRIGGER_NAME, triggers);
			    }
			}	
		END_FOR;

		if (!REQUEST (irq_format4))
		    REQUEST (irq_format4) = request_fmtx;

		request_fmtx = (BLK) CMP_find_request (tdbb, irq_format5, IRQ_REQUESTS);
		/* BUG #8458: Check constraint triggers have to be loaded
		              (and hence executed) after the user-defined 
			      triggers because user-defined triggers can modify
			      the values being inserted or updated so that
			      the end values stored in the database don't
			      fulfill the check constraint */

		FOR (REQUEST_HANDLE request_fmtx) 
			TRG IN RDB$TRIGGERS 
			WITH TRG.RDB$RELATION_NAME = work->dfw_name AND
			(TRG.RDB$SYSTEM_FLAG = 0 OR TRG.RDB$SYSTEM_FLAG MISSING)
			AND TRG.RDB$TRIGGER_NAME NOT STARTING WITH 'CHECK_'
			SORTED BY TRG.RDB$TRIGGER_SEQUENCE

		    if (!REQUEST (irq_format5))
			REQUEST (irq_format5) = request_fmtx;

		    if (!TRG.RDB$TRIGGER_INACTIVE)
			{
			put_summary_record (blob, RSR_trigger_name, 
			    TRG.RDB$TRIGGER_NAME, name_length (TRG.RDB$TRIGGER_NAME));

			/* for a view, load the trigger temporarily -- 
			   this is inefficient since it will just be reloaded
			   in MET_scan_relation () but it needs to be done
			   in case the view would otherwise be non-updatable */

			if (!null_view)
			    {
			    if (!relation->rel_name)
				relation->rel_name = MET_save_name (tdbb, TRG.RDB$RELATION_NAME);
			    MET_load_trigger (tdbb, relation,
						TRG.RDB$TRIGGER_NAME, triggers);
			    }
			}	
		END_FOR;

		if (!REQUEST (irq_format5))
		    REQUEST (irq_format5) = request_fmtx;

		request_fmtx = (BLK) CMP_find_request (tdbb, irq_format6, IRQ_REQUESTS);
		FOR (REQUEST_HANDLE request_fmtx)
			TRG IN RDB$TRIGGERS
			WITH TRG.RDB$RELATION_NAME = work->dfw_name AND
			(TRG.RDB$SYSTEM_FLAG = 0 OR TRG.RDB$SYSTEM_FLAG MISSING)
			AND (TRG.RDB$TRIGGER_NAME STARTING WITH 'CHECK_' OR
			     TRG.RDB$TRIGGER_SOURCE MISSING)
			SORTED BY TRG.RDB$TRIGGER_SEQUENCE

		if (!REQUEST (irq_format6))
		    REQUEST (irq_format6) = request_fmtx;

		if (!TRG.RDB$TRIGGER_INACTIVE)
		    {
		    put_summary_record (blob, RSR_trigger_name,
			TRG.RDB$TRIGGER_NAME, name_length (TRG.RDB$TRIGGER_NAME));

		    /* for a view, load the trigger temporarily --
		       this is inefficient since it will just be reloaded
		       in MET_scan_relation () but it needs to be done
		       in case the view would otherwise be non-updatable */

		    if (!null_view)
			{
			if (!relation->rel_name)
			    relation->rel_name = MET_save_name (tdbb, TRG.RDB$RELATION_NAME);
			MET_load_trigger (tdbb, relation,
					    TRG.RDB$TRIGGER_NAME, triggers);
			}
		    }
		END_FOR;

		if (!REQUEST (irq_format6))
		    REQUEST (irq_format6) = request_fmtx;

		BLB_close (tdbb, blob);
	    END_MODIFY;
	END_FOR;

	if (!REQUEST (irq_format1))
	    REQUEST (irq_format1) = request_fmt1;

	/* If we didn't find the relation, it is probably being dropped */

	if (!relation)
	    return FALSE;

	if (!(relation->rel_flags & REL_sys_trigs_being_loaded))
	    {
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
	relation->rel_current_format = make_format (tdbb, relation,
						    version, stack);

	/* in case somebody changed the view definition or a computed
	   field, reset the dependencies by deleting the current ones
	   and setting a flag for MET_scan_relation to find the new ones */

	if (!null_view)
	    MET_delete_dependencies (tdbb, work->dfw_name, obj_view);

	if (!null_view || computed_field)
	    relation->rel_flags |= REL_get_dependencies;

	if (external_flag)
	    {
	    temp = NULL;
	    FOR (REQUEST_HANDLE temp) FMT IN RDB$FORMATS WITH 
		    FMT.RDB$RELATION_ID EQ relation->rel_id AND
		    FMT.RDB$FORMAT EQ 0
		ERASE FMT;
	    END_FOR;
	    CMP_release (tdbb, temp);
	    make_format (tdbb, relation, 0, external);
	    }

	relation->rel_flags &= ~REL_scanned;
	DFW_post_work (transaction, dfw_scan_relation, NULL_PTR, relation->rel_id);
	break;
    }

return FALSE;
}

static BOOLEAN modify_procedure (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	m o d i f y _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Perform required actions when modifying procedure.
 *
 **************************************/
PRC	procedure;
USHORT	wait;
JMP_BUF env, *old_env;

SET_TDBB (tdbb);

switch (phase)
    {
    case 0:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0)))
	    return FALSE;

	wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;
	LCK_convert_non_blocking (tdbb, procedure->prc_existence_lock, LCK_SR, wait);
	return FALSE;

    case 1:
	return TRUE;

    case 2:
	return TRUE;

    case 3:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0)))
	    return FALSE;
	wait = (transaction->tra_flags & TRA_nowait) ? FALSE : TRUE;

	/* Let relation be deleted if only this transaction is using it */

	if (!LCK_convert_non_blocking (tdbb, procedure->prc_existence_lock,
				LCK_EX, wait))
	    ERR_post (gds__no_meta_update,
		gds_arg_gds, gds__obj_in_use,
		gds_arg_string, ERR_cstring (work->dfw_name), 
		0);

	/* If we are in a multi-client server, someone else may have marked
	   procedure obsolete. Unmark and we will remark it later. */

	procedure->prc_flags &= ~PRC_obsolete;
	return TRUE;

    case 4:
	if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, 0)))
	    return FALSE;

        old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
        tdbb->tdbb_setjmp = (UCHAR*) env;
        if (SETJMP (env))
            {
#ifdef SUPERSERVER
            THD_rec_mutex_unlock (&tdbb->tdbb_database->dbb_sp_rec_mutex);
#endif
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
            ERR_punt ();
            }

#ifdef SUPERSERVER
        if (!(tdbb->tdbb_database->dbb_flags & DBB_sp_rec_mutex_init))
            {
            THD_rec_mutex_init (&tdbb->tdbb_database->dbb_sp_rec_mutex);
            tdbb->tdbb_database->dbb_flags |= DBB_sp_rec_mutex_init;
            }
        THREAD_EXIT;
        if (THD_rec_mutex_lock (&tdbb->tdbb_database->dbb_sp_rec_mutex))
            {
            THREAD_ENTER;
            return FALSE;
            }
        THREAD_ENTER;
#endif /* SUPERSERVER */
        if (procedure->prc_use_count)
	    {
	    USHORT prc_alter_count = procedure->prc_alter_count;
	    if (prc_alter_count > MAX_PROC_ALTER )
		{
                ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__proc_name,
		    gds_arg_string, ERR_cstring (work->dfw_name),
		    gds_arg_gds, gds__version_err,
		    0);
		    /* Msg357: too many versions */
		}
	    LCK_release (tdbb, procedure->prc_existence_lock);
	    tdbb->tdbb_database->dbb_procedures->vec_object[procedure->prc_id] = NULL_PTR;
	    if (!(procedure = MET_lookup_procedure_id (tdbb, work->dfw_id, FALSE, PRC_being_altered )))
		{
#ifdef SUPERSERVER
                THD_rec_mutex_unlock (&tdbb->tdbb_database->dbb_sp_rec_mutex);
#endif
                tdbb->tdbb_setjmp = (UCHAR*) old_env;
		return FALSE;
		}
	    procedure->prc_alter_count = ++prc_alter_count;
	    }

	procedure->prc_flags |= PRC_being_altered;
        if (procedure->prc_request)
	    {
	    if (CMP_clone_active (procedure->prc_request))
		ERR_post (gds__no_meta_update,
		    gds_arg_gds, gds__obj_in_use,
		    gds_arg_string, ERR_cstring (work->dfw_name),
		    0);

	    /* release the request */

	    CMP_release (tdbb, procedure->prc_request);
	    procedure->prc_request = 0;
	    }

	/* delete dependency lists */

	MET_delete_dependencies (tdbb, work->dfw_name, obj_procedure);

	/* the procedure has just been scanned by MET_lookup_procedure_id
	   and its PRC_scanned flag is set. We are going to reread it
	   from file (create all new dependencies) and do not want this
	   flag to be set. That is why we do not add PRC_obsolete and
	   PRC_being_altered flags, we set only these two flags
	*/
	procedure->prc_flags = (PRC_obsolete | PRC_being_altered);

	if (procedure->prc_existence_lock)
	    LCK_release (tdbb, procedure->prc_existence_lock);

	/* remove procedure from cache */

	MET_remove_procedure (tdbb, work->dfw_id, NULL);

	/* Now handle the new definition */

	get_procedure_dependencies (work);

	procedure->prc_flags &= ~(PRC_obsolete | PRC_being_altered);

        tdbb->tdbb_setjmp = (UCHAR*) old_env;
#ifdef SUPERSERVER
        THD_rec_mutex_unlock (&tdbb->tdbb_database->dbb_sp_rec_mutex);
#endif

	break;
    }

return FALSE;
}

static BOOLEAN modify_trigger (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *	Perform required actions when modifying trigger.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	/* get rid of old dependencies, bring in the new */

	MET_delete_dependencies (tdbb, work->dfw_name, obj_trigger);
	get_trigger_dependencies (work);
	break;
    }

return FALSE;
}

static USHORT name_length (
    TEXT	*name)
{
/**************************************
 *
 *	n a m e _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Compute effective length of system relation name.
 *	Trailing blanks are ignored.
 *
 **************************************/
TEXT	*p, *q;
#define	BLANK	'\040'

q = name - 1;
for (p = name; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

return ((q+1) - name);
}

static void put_summary_blob (
    BLB		blob,
    RSR_T	type,
    SLONG	blob_id [2])
{
/**************************************
 *
 *	p u t _ s u m m a r y _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Put an attribute record to the relation summary blob.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLB	blr;
UCHAR	*buffer, temp [128];
USHORT	length;
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

/* If blob is null, don't both */

if (!blob_id [0] && !blob_id [1])
    return;

/* Go ahead and open blob */

blr = BLB_open (tdbb, dbb->dbb_sys_trans, blob_id);
length = blr->blb_length;
buffer = (length > sizeof (temp)) ?
    (UCHAR*) ALL_malloc ((SLONG) blr->blb_length, ERR_jmp) : temp;

/* Setup to cleanup after any error that may occur */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (buffer != temp)
	ALL_free (buffer);
    ERR_punt();
    }

length = BLB_get_data (tdbb, blr, buffer, (SLONG) length);
put_summary_record (blob, type, buffer, length);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (buffer != temp)
    ALL_free (buffer);
}

static void put_summary_record (
    BLB		blob,
    RSR_T	type,
    UCHAR	*data,
    USHORT	length)
{
/**************************************
 *
 *	p u t _ s u m m a r y _ r e c o r d
 *
 **************************************
 *
 * Functional description
 *	Put an attribute record to the relation summary blob.
 *
 **************************************/
TDBB	tdbb;
UCHAR	*p, *buffer, temp [129];
JMP_BUF	env, *old_env;

tdbb = GET_THREAD_DATA;

p = buffer = (length + 1 > sizeof (temp)) ?
    (UCHAR*) ALL_malloc ((SLONG) (length + 1), ERR_jmp) : temp;

*p++ = (UCHAR) type;

if (length)
    do *p++ = *data++; while (--length);

/* Setup to cleanup after any error that may occur */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;

if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (buffer != temp)
	ALL_free (buffer);
    ERR_punt();
    }

BLB_put_segment (tdbb, blob, buffer, p - buffer);

tdbb->tdbb_setjmp = (UCHAR*) old_env;

if (buffer != temp)
    ALL_free (buffer);
}

static BOOLEAN scan_relation (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work,
    TRA		transaction)
{
/**************************************
 *
 *	s c a n _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Call MET_scan_relation with the appropriate 
 *	relation.
 *
 **************************************/

SET_TDBB (tdbb);

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
	MET_scan_relation (tdbb, MET_relation (tdbb, work->dfw_id)); 
	break;
    }

return FALSE;
}

static BOOLEAN shadow_defined (TDBB	tdbb)
{
/**************************************
 *
 *	s h a d o w _ d e f i n e d
 *
 **************************************
 *
 * Functional description
 *	Return TRUE if any shadows have been has been defined 
 *      for the database else return FALSE.
 *
 **************************************/
DBB	dbb;
BOOLEAN	result;
BLK	handle;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

result = FALSE;
handle = NULL;

FOR (REQUEST_HANDLE handle) FIRST 1 X IN RDB$FILES WITH X.RDB$SHADOW_NUMBER > 0
    result = TRUE;
END_FOR

CMP_release (tdbb, handle);

return result;
}

static BOOLEAN wal_defined (TDBB	tdbb)
{
/**************************************
 *
 *	w a l _ d e f i n e d
 *
 **************************************
 *
 * Functional description
 *	Return TRUE if Writ_ahead Log (WAL) has been defined 
 *      for the database else return FALSE.
 *
 **************************************/
DBB	dbb;
BOOLEAN	result;
BLK	handle;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

result = FALSE;
handle = NULL;

FOR (REQUEST_HANDLE handle) FIRST 1 X IN RDB$LOG_FILES
    result = TRUE;
END_FOR

CMP_release (tdbb, handle);

return result;
}
