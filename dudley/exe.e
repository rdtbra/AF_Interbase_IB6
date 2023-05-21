/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		exe.e
 *	DESCRIPTION:	Meta-data update execution module.
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

#include <string.h>
#include "../dudley/ddl.h"
#include "../jrd/license.h"
#include "../jrd/gds.h"
#include "../jrd/flags.h"
#include "../jrd/acl.h"
#include "../jrd/intl.h"
#include "../jrd/obj.h"
#include "../dudley/ddl_proto.h"
#include "../dudley/exe_proto.h"
#include "../dudley/gener_proto.h"
#include "../dudley/hsh_proto.h"
#include "../dudley/lex_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../wal/walf_proto.h"

#ifdef WIN_NT
#include <io.h>
#endif

#undef IS_DTYPE_ANY_TEXT
#undef NUMERIC_SCALE

#define	IS_DTYPE_ANY_TEXT(x)	(make_dtype(x) <= dtype_any_text)
#define NUMERIC_SCALE(desc)	((IS_DTYPE_ANY_TEXT((desc).dsc_dtype)) ? 0 : (desc).dsc_scale)

DATABASE
    DB = FILENAME "yachts.lnk";

static void	add_cache (DBB);
static void	add_field (FLD);
static void	add_files (DBB, FIL);
static void	add_filter (FILTER);
static void	add_function (FUNC);
static void	add_function_arg (FUNCARG);
static void	add_generator (SYM);
static void	add_global_field (FLD);
static void	add_index (DUDLEY_IDX);
static void	add_log_files (DBB);
static void	add_relation (REL);
static void	add_security_class (SCL);
static void	add_trigger (TRG);
static void	add_trigger_msg (TRGMSG);
static void	add_type (TYP);
static void	add_user_privilege (USERPRIV);
static void	add_view (REL);
static void	alloc_file_name (FIL *, UCHAR *);
static FLD	check_field (SYM, SYM);
static BOOLEAN	check_function (SYM);
static BOOLEAN	check_range (FLD);
static BOOLEAN	check_relation (SYM);
static void	close_blob (void *);
static void	*create_blob (SLONG *, USHORT, UCHAR *);
static void	drop_cache (DBB);
static void	drop_field (FLD);
static void	drop_filter (FILTER);
static void	drop_function (FUNC);
static void	drop_global_field (FLD);
static void	drop_index (DUDLEY_IDX);
static void	drop_relation (REL);
static void	drop_security_class (SCL);
static void	drop_shadow (SLONG);
static void	drop_trigger (TRG);
static void	drop_trigger_msg (TRGMSG);
static void	drop_type (TYP);
static void	drop_user_privilege (USERPRIV);
static void	erase_userpriv (USERPRIV, TEXT *, USRE, UPFE);
static void	get_field_desc (FLD);
static void	get_global_fields (void);
static void	get_log_names (SCHAR *, FIL *, SCHAR *, SLONG, SSHORT, SSHORT);
static void	get_log_names_serial (FIL *);
static void	get_relations (DBB);
static USHORT	get_prot_mask (TEXT *, TEXT *);
static SYM	get_symbol (enum sym_t, TEXT *, CTX);
static void	get_triggers (DBB);
static void	get_udfs (DBB);
static void	make_desc (NOD, DSC *);
static int	make_dtype (USHORT);
static void	modify_field (FLD);
static void	modify_global_field (FLD);
static void	modify_index (DUDLEY_IDX);
static void	modify_relation (REL);
static void	modify_trigger (TRG);
static void	modify_trigger_msg (TRGMSG);
static void	modify_type (TYP);
static void	move_symbol (SYM, register SCHAR *, SSHORT);
static void	set_generator (NOD);
static void	store_acl (SCL, SLONG *);
static void	store_blr (NOD, SLONG *, REL);
static void	store_query_header (NOD, SLONG *);
static void	store_range (FLD);
static void	store_text (TXT, SLONG *);
static void	store_userpriv (USERPRIV, TEXT *, TEXT *, USRE, UPFE);
static int	string_length (SCHAR);
static void	wal_info (UCHAR *, SLONG *, SCHAR *, SLONG *);

static GDS__QUAD	null_blob;
static LLS		files_to_delete = (LLS) NULL;
static TEXT		alloc_info [] = { gds__info_allocation, gds__info_end };

#define CMP_SYMBOL(sym1, sym2)		strcmp (sym1->sym_string, sym2->sym_string)
#define MOVE_SYMBOL(symbol, field)	move_symbol (symbol, field, sizeof (field) - 1)

#ifdef mpexl
#define unlink		PIO_unlink
#endif

static SCHAR	db_info [] = { gds__info_logfile, gds__info_cur_logfile_name, gds__info_cur_log_part_offset };
/*
#define DEBUG	1
*/
 
SLONG EXE_check_db_version (
    DBB	dbb)
{
/**************************************
 *
 *	E X E _ c h e c k _ d b _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Find the version number of the database.
 *
 **************************************/
SLONG	db_version = DB_VERSION_DDL4;

FOR X IN RDB$RELATIONS
    WITH X.RDB$RELATION_NAME = "RDB$TRIGGERS"
    db_version = DB_VERSION_DDL6;
END_FOR;
FOR X IN RDB$RELATIONS
    WITH X.RDB$RELATION_NAME = "RDB$LOG_FILES"
    db_version = DB_VERSION_DDL8;
END_FOR;

return db_version;
}

void EXE_create_database (
    DBB	dbb)
{
/**************************************
 *
 *	E X E _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Create a new database.
 *
 **************************************/
TEXT	*file_name, dpb [128], *d, *p;
USHORT	page_size, dpb_length;
USHORT	len;
ULONG	llen;
SLONG	log, part_offset;
SCHAR	db_info_buffer [512], cur_log [512];
SLONG	ods_version;
SLONG	result;

/* Generate a dpb for attempting to attach to, and
   later to create, the specified database. */

dpb_length = 0;
d = dpb;
*d++ = gds__dpb_version1;

if (p = DDL_default_user)
    {   
    *d++ = gds__dpb_user_name;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

if (p = DDL_default_password)
    {   
    *d++ = gds__dpb_password;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

dpb_length = d - dpb;
if (dpb_length == 1)
    dpb_length = 0;

file_name = dbb->dbb_name->sym_string;

result = gds__attach_database  (gds__status, 0, GDS_VAL (file_name), 
	    GDS_REF (DB), dpb_length, dpb);
if (!DDL_replace)
    {
    if (!result)
	{
	gds__detach_database (gds__status,
	    GDS_REF (DB));
	DDL_msg_put (18, file_name, NULL, NULL, NULL, NULL); /* msg 18: Database \"%s\" already exists */
	if (DDL_interactive)
	    { 
	    DDL_msg_partial (19, NULL, NULL, NULL, NULL, NULL); /* msg 19: Do you want to replace it? */
	    if (!DDL_yes_no (286))
		DDL_exit (FINI_ERROR);
	    }
	else
	    DDL_exit (FINI_ERROR);
	}
    else if (gds__status [1] != gds__io_error)
	DDL_error_abort (gds__status, 20, file_name, NULL, NULL, NULL, NULL); /* msg 20: Database \"%s\" exists but can't be opened */
    } 
    else
        {
    	/* replacing the database so try to drop it first. */
	if (!result)
	    {
	    isc_drop_database (gds__status, &DB);
	    if (DB)
	        isc_detach_database (gds__status, &DB);
	    }
    	}
    

/* add the specified page size to the already defined dpb */

if (dbb->dbb_page_size)
    {
    page_size = MAX (dbb->dbb_page_size, 1024);
    *d++ = gds__dpb_page_size;
    *d++ = 2;
    *d++ = page_size;
    *d++ = page_size >> 8;
    dpb_length = d - dpb;
    }

if (llen = dbb->dbb_chkptlen)
    {
    *d++ = gds__dpb_wal_chkptlen;
    *d++ = 4;
    for (len = 0; len < 4; len++, llen = llen >> 8)
	*d++ = llen;
    }

if (len = dbb->dbb_numbufs)
    {
    *d++ = gds__dpb_wal_numbufs;
    *d++ = 2;
    *d++ = len;
    *d++ = len >> 8;
    }

if (len = dbb->dbb_bufsize)
    {
    *d++ = gds__dpb_wal_bufsize;
    *d++ = 2;
    *d++ = len;
    *d++ = len >> 8;
    }

if ((llen = dbb->dbb_grp_cmt_wait) >= 0)
    {
    *d++ = gds__dpb_wal_grp_cmt_wait;
    *d++ = 4;
    for (len = 0; len < 4; len++, llen = llen >> 8)
	*d++ = llen;
    }

dpb_length = d - dpb;
if (dpb_length == 1)
    dpb_length = 0;

if (gds__create_database (gds__status, 
	    0, 
	    GDS_VAL (file_name), 
	    GDS_REF (DB),
	    dpb_length, 
	    dpb,
	    0))
    DDL_error_abort (gds__status, 21, file_name, NULL, NULL, NULL, NULL); /* msg 21: Couldn't create database \"%s\" */

if (DDL_version)
    {
    DDL_msg_put (23, file_name, NULL, NULL, NULL, NULL); /* msg 23: Version(s) for database \"%s\" */
    gds__version (&DB, NULL, NULL); 
    }

START_TRANSACTION;

ods_version = EXE_check_db_version (dbb);

if (ods_version < DB_VERSION_DDL6)
    DDL_error_abort (NULL_PTR, 32, NULL, NULL, NULL, NULL, NULL); /* msg 32: database version is too old to modify:
use GBAK first */

dbb->dbb_handle = DB;
dbb->dbb_transaction = gds__trans;

/* Don't allow cache and WAL configurations for pre-ODS8 databases */

if (ods_version < DB_VERSION_DDL8) 
    {
    if ((dbb->dbb_flags & DBB_log_default) || 
	(dbb->dbb_logfiles) ||
	(dbb->dbb_cache_file))
	DDL_error_abort (NULL_PTR, 32, NULL, NULL, NULL, NULL, NULL); 
	     /* msg 32: database version is too old to modify: use GBAK first */
    }

if (dbb->dbb_files)
    add_files (dbb, dbb->dbb_files);

if (dbb->dbb_cache_file)
    add_cache (dbb);

if ((dbb->dbb_flags & DBB_log_default) || (dbb->dbb_logfiles))
    add_log_files (dbb);

/* If there is a description of the database, store it now */

FOR D IN RDB$DATABASE
    MODIFY D USING
	if (dbb->dbb_description)
	    {
	    store_text (dbb->dbb_description, &D.RDB$DESCRIPTION);
	    D.RDB$DESCRIPTION.NULL = FALSE;
	    }
	if (dbb->dbb_security_class)
	    {
	    MOVE_SYMBOL (dbb->dbb_security_class, D.RDB$SECURITY_CLASS);
	    D.RDB$SECURITY_CLASS.NULL = FALSE;
	    }	
    END_MODIFY;
END_FOR;

get_global_fields();
get_relations (dbb);
get_udfs (dbb); 
get_triggers (dbb);

if ((dbb->dbb_flags & DBB_log_default) || (dbb->dbb_logfiles))
    {
    /* setup enough information to drop log files created */

    if (gds__database_info (gds__status,
			    GDS_REF (DB),
			    sizeof (db_info),
			    db_info,
			    sizeof (db_info_buffer),
			    db_info_buffer))
	DDL_error_abort (gds__status, 327, NULL, NULL, NULL, NULL, NULL);	/* msg 327: error in getting write ahead log information */

    log = 0;

    wal_info (db_info_buffer, &log, cur_log, &part_offset);

    if (log)
	get_log_names_serial (&dbb->dbb_files);

    if (log)
	{
	get_log_names (dbb->dbb_name->sym_string, &dbb->dbb_files, cur_log,
	    part_offset, dbb->dbb_flags & DBB_cascade, 0);
	}
    }
}

void EXE_drop_database (
    DBB	dbb)
{
/**************************************
 *
 *	E X E _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Drop an existing database, and all
 *	its files.
 *
 **************************************/
STATUS	status_vector [20];
FIL	file;
SYM	string;
TEXT	dpb [128], *d, *p;
USHORT	dpb_length;
SLONG	log, part_offset;
SCHAR	db_info_buffer [512], cur_log [512];

dpb_length = 0;
d = dpb;
*d++ = gds__dpb_version1;

if (p = DDL_default_user)
    {   
    *d++ = gds__dpb_user_name;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

if (p = DDL_default_password)
    {   
    *d++ = gds__dpb_password;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

dpb_length = d - dpb;
if (dpb_length == 1)
    dpb_length = 0;

if (gds__attach_database (gds__status, 
	    0, 
	    GDS_VAL (dbb->dbb_name->sym_string), 
	    GDS_REF (DB), 
	    dpb_length, 
	    dpb))
    DDL_error_abort (gds__status, 25, NULL, NULL, NULL, NULL, NULL); /* msg 25: Couldn't locate database */

START_TRANSACTION;

FOR F IN RDB$FILES SORTED BY F.RDB$FILE_START
    alloc_file_name (&dbb->dbb_files, F.RDB$FILE_NAME);
END_FOR;

COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 26, NULL, NULL, NULL, NULL, NULL); /* msg 26: error commiting metadata changes */
	ROLLBACK;
    END_ERROR;

if (gds__database_info (gds__status,
			GDS_REF (DB),
			sizeof (db_info),
			db_info,
			sizeof (db_info_buffer),
			db_info_buffer))
    DDL_error_abort (gds__status, 327, NULL, NULL, NULL, NULL, NULL);	/* msg 327: error in getting write ahead log information */

wal_info (db_info_buffer, &log, cur_log, &part_offset);

if (log)
    get_log_names_serial (&dbb->dbb_files);

if (gds__detach_database (gds__status, GDS_REF (DB)))
    DDL_error_abort (gds__status, 27, NULL, NULL, NULL, NULL, NULL); /* msg 27: Couldn't release database */

if (log)
    {
    get_log_names (dbb->dbb_name->sym_string, &dbb->dbb_files, cur_log,
	part_offset, dbb->dbb_flags & DBB_cascade, 0);
    }

for (file = dbb->dbb_files; file; file = file->fil_next)
    if (unlink (file->fil_name->sym_name))
	DDL_error_abort (NULL_PTR, 28, file->fil_name->sym_name, NULL, NULL, NULL, NULL); /* msg 28: Could not delete file %s */

if (unlink (dbb->dbb_name->sym_string))
    DDL_error_abort (NULL_PTR, 28, dbb->dbb_name->sym_string, NULL, NULL, NULL, NULL); /* msg 28: Could not delete file %s */
}

void EXE_execute (void) 
{
/**************************************
 *
 *	E X E _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Execute the output of the parser.
 *	By this time we should have openned
 *	a database - by creating it or by
 *	preparing to modify it.  If not,
 *	give up quietly.
 *
 **************************************/
ACT	action;

if (!DDL_actions)
    return;

if (!DB)
    DDL_error_abort (NULL_PTR, 33, NULL, NULL, NULL, NULL, NULL); /* msg 33: no database specified */

for (action = DDL_actions; action; action = action->act_next)
    if (!(action->act_flags & ACT_ignore))
	{
	DDL_line = action->act_line;
	switch (action->act_type)
	    {
	    case act_c_database:
	    case act_m_database:
		break;

	    case act_a_field:
		add_field (action->act_object);
		break;

	    case act_m_field:
		modify_field (action->act_object);
		break;

 	    case act_d_field:
		drop_field (action->act_object);
		break;

	    case act_a_filter:
		add_filter (action->act_object);
		break;

	    case act_d_filter:
  		drop_filter (action->act_object);
		break;

	    case act_a_function:
		add_function (action->act_object);
		break;

	    case act_a_function_arg:
		add_function_arg (action->act_object);
		break;

	    case act_d_function:
		drop_function (action->act_object);
		break;

	    case act_a_gfield:
		add_global_field (action->act_object);
		break;

	    case act_m_gfield:
		modify_global_field (action->act_object);
		break;

	    case act_d_gfield:
		drop_global_field (action->act_object);
		break;

	    case act_a_index:
		add_index (action->act_object);
		break;

	    case act_m_index:
		modify_index (action->act_object);
		break;

	    case act_d_index:
		drop_index (action->act_object);
		break;

	    case act_a_relation:
		add_relation (action->act_object);
		break;

	    case act_m_relation:
		modify_relation (action->act_object);
		break;

	    case act_d_relation:
		drop_relation (action->act_object);
		break;

	    case act_a_security:
		add_security_class (action->act_object);
		break;
	
	    case act_d_security:
		drop_security_class (action->act_object);
		break;

	    case act_a_trigger: 
		add_trigger (action->act_object);
		break;

	    case act_m_trigger:
		modify_trigger (action->act_object);
		break;

	    case act_d_trigger:
		drop_trigger (action->act_object);
		break;

	    case act_a_trigger_msg: 
		add_trigger_msg (action->act_object);
		break;

	    case act_m_trigger_msg:
		modify_trigger_msg (action->act_object);
		break;

	    case act_d_trigger_msg:
		drop_trigger_msg (action->act_object);
		break;

	    case act_a_type: 
		add_type (action->act_object);
		break;

	    case act_m_type:
		modify_type (action->act_object);
		break;

	    case act_d_type:
		drop_type (action->act_object);
		break;

	    case act_grant:
		add_user_privilege (action->act_object);
		break;

	    case act_revoke:
		drop_user_privilege (action->act_object);
		break;

	    case act_a_shadow:
		add_files (NULL_PTR, action->act_object);
		break;

	    case act_d_shadow:
		drop_shadow ((SLONG) action->act_object);
		break;

	    case act_a_generator:
		add_generator (action->act_object);
		break;

	    case act_s_generator:
		set_generator (action->act_object);
		break;

	    default:
		DDL_err (34, NULL, NULL, NULL, NULL, NULL); /* msg 34: action not implemented yet */
	    } 
	}
}

void EXE_fini (
    DBB	dbb)
{
/**************************************
 *
 *	E X E _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Sign off database -- commit or rollback depending on whether or
 *	not errors have occurred.
 *
 **************************************/
SYM	string;

if (DB)
    {
    if (DDL_errors)
	{
	if (dbb && (dbb->dbb_flags & DBB_create_database))
	    {
	    FOR F IN RDB$FILES WITH F.RDB$SHADOW_NUMBER NE 0
		alloc_file_name (&dbb->dbb_files, F.RDB$FILE_NAME);
	    END_FOR;
	    }

	ROLLBACK
	ON_ERROR
	    DDL_db_error (gds__status, 35, NULL, NULL, NULL, NULL, NULL); /* msg 35: error rolling back metadata changes */ 
	END_ERROR
	}
    else 
	{
	COMMIT
	ON_ERROR
	    DDL_db_error (gds__status, 36, NULL, NULL, NULL, NULL, NULL); /* msg 36: error commiting metadata changes */
	    ROLLBACK;
	END_ERROR
	}
    FINISH;
    }

/* We have previously committed the deletion of some files.
   Do the actual deletion here. */

while (files_to_delete)
    {
    string = (SYM) DDL_pop (&files_to_delete);
    unlink (string->sym_name);
    gds__free (string);
    }
}

void EXE_modify_database (
    DBB	dbb)
{
/**************************************
 *
 *	E X E _ m o d i f y _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Modify an existing database.
 *
 **************************************/
TEXT	dpb [128], *d, *p;
USHORT	dpb_length, len;
ULONG	llen;
FIL	file, log_files;
SLONG	log, part_offset;
SCHAR	db_info_buffer [512], cur_log [512];
SLONG    ods_version;

dpb_length = 0;
d = dpb;
*d++ = gds__dpb_version1;

if (p = DDL_default_user)
    {
    *d++ = gds__dpb_user_name;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

if (p = DDL_default_password)
    {   
    *d++ = gds__dpb_password;
    *d++ = strlen (p);
    while (*p)
	*d++ = *p++;
    }

if (dbb->dbb_flags & DBB_drop_log)
    {
    *d++ = gds__dpb_drop_walfile;
    *d++ = 1;
    *d++ = 1;
    }

if (llen = dbb->dbb_chkptlen)
    {
    *d++ = gds__dpb_wal_chkptlen;
    *d++ = 4;
    for (len = 0; len < 4; len++, llen = llen >> 8)
	*d++ = llen;
    }

if (len = dbb->dbb_numbufs)
    {
    *d++ = gds__dpb_wal_numbufs;
    *d++ = 2;
    *d++ = len;
    *d++ = len >> 8;
    }

if (len = dbb->dbb_bufsize)
    {
    *d++ = gds__dpb_wal_bufsize;
    *d++ = 2;
    *d++ = len;
    *d++ = len >> 8;
    }

if ((llen = dbb->dbb_grp_cmt_wait) >= 0)
    {
    *d++ = gds__dpb_wal_grp_cmt_wait;
    *d++ = 4;
    for (len = 0; len < 4; len++, llen = llen >> 8)
	*d++ = llen;
    }

dpb_length = d - dpb;
if (dpb_length == 1)
    dpb_length = 0;

if (gds__attach_database (gds__status, 0, 
	    GDS_VAL (dbb->dbb_name->sym_string), 
	    GDS_REF (DB), 
	    dpb_length, 
	    dpb))
    DDL_error_abort (gds__status, 29, dbb->dbb_name->sym_string, NULL, NULL, NULL, NULL); /* msg 29: Couldn't attach database */

if (DDL_version)
    {
    DDL_msg_put (30, dbb->dbb_name->sym_string, NULL, NULL, NULL, NULL); /* msg 30:    Version(s) for database \"%s\" */
    gds__version (&DB, NULL, NULL); 
    }

START_TRANSACTION;

ods_version = EXE_check_db_version (dbb);

if (ods_version < DB_VERSION_DDL6)
    DDL_error_abort (NULL_PTR, 32, NULL, NULL, NULL, NULL, NULL); /* msg 32: database version is too old to modify:
use GBAK first */

/* Don't allow cache and WAL configurations for pre-ODS8 databases */

if (ods_version < DB_VERSION_DDL8) 
    {
    if ((dbb->dbb_flags & DBB_log_default) || 
	(dbb->dbb_logfiles) ||
	(dbb->dbb_flags && DBB_drop_log) ||
	(dbb->dbb_cache_file) ||
	(dbb->dbb_flags & DBB_drop_cache))
	DDL_error_abort (NULL_PTR, 32, NULL, NULL, NULL, NULL, NULL); 
	     /* msg 32: database version is too old to modify: use GBAK first */
    }

dbb->dbb_handle = DB;
dbb->dbb_transaction = gds__trans;

/* erase log files and commit transaction */

if (dbb->dbb_flags & DBB_drop_log)
    {
    if (gds__database_info (gds__status,
			    GDS_REF (DB),
			    sizeof (db_info),
			    db_info,
			    sizeof (db_info_buffer),
			    db_info_buffer))
	DDL_error_abort (gds__status, 327, NULL, NULL, NULL, NULL, NULL);	/* msg 327: error in getting write ahead log information */

    log_files = (FIL) NULL;
    log = 0;

    wal_info (db_info_buffer, &log, cur_log, &part_offset);

    if (log)
	get_log_names_serial (&log_files);

    FOR X IN RDB$LOG_FILES
	ERASE X;
    END_FOR;
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 321, NULL, NULL, NULL, NULL, NULL); /* msg 321: error commiting new write ahead log declarations */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;

    if (log)
	{
	get_log_names (dbb->dbb_name->sym_string, &log_files, cur_log,
	    part_offset, dbb->dbb_flags & DBB_cascade, 1);
	}

    for (file = log_files; file; file = file->fil_next)
	if (unlink (file->fil_name->sym_name))
	    DDL_error_abort (NULL_PTR, 28, file->fil_name->sym_name, NULL, NULL, NULL, NULL); /* msg 28: Could not delete file %s */
    }

if (dbb->dbb_flags & DBB_drop_cache)
    drop_cache (dbb);

/* adding new files */

if (dbb->dbb_files)
    add_files (dbb, dbb->dbb_files);

if (dbb->dbb_cache_file)
    add_cache (dbb);

if ((dbb->dbb_flags & DBB_log_default) || (dbb->dbb_logfiles))
    {
    /* check if log files exist */

    FOR X IN RDB$LOG_FILES
	DDL_error_abort (NULL_PTR, 333, NULL, NULL, NULL, NULL, NULL); /* msg 333: Cannot modify log file specification.  Drop and redefine log files */
    END_FOR;

    /* add the new log files. */ 

    add_log_files (dbb);

    /* commit log file changes */

    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 321, NULL, NULL, NULL, NULL, NULL); /* msg 321: error commiting new write ahead log declarations */
	ROLLBACK;
	END_ERROR;
    START_TRANSACTION;
    }

/* If there is a description of the database, modify it now */

FOR D IN RDB$DATABASE
    MODIFY D USING
	if (dbb->dbb_flags & DBB_null_security_class)
	    D.RDB$SECURITY_CLASS.NULL = TRUE;
	if (dbb->dbb_flags & DBB_null_description)
	    D.RDB$DESCRIPTION.NULL = TRUE;
	if (dbb->dbb_description)
	    {
	    store_text (dbb->dbb_description, &D.RDB$DESCRIPTION);
	    D.RDB$DESCRIPTION.NULL = FALSE;
	    }
	if (dbb->dbb_security_class)
 	    {
	    MOVE_SYMBOL (dbb->dbb_security_class, D.RDB$SECURITY_CLASS);
	    D.RDB$SECURITY_CLASS.NULL = FALSE;
	    }	

    END_MODIFY;
END_FOR;

get_global_fields();
get_relations (dbb);
get_udfs (dbb); 
get_triggers (dbb);
}

int EXE_relation (
    REL		relation)
{
/**************************************
 *
 *	E X E _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Lookup a relation. On the way by,
 *	fix up the field position in the
 *	relation block;
 *
 **************************************/
USHORT	result;
SYM	symbol;

symbol = relation->rel_name;
result = FALSE;

FOR X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    result = TRUE;
END_FOR;

FOR X IN RDB$RELATION_FIELDS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    if (X.RDB$FIELD_POSITION > relation->rel_field_position)
	 relation->rel_field_position = X.RDB$FIELD_POSITION;
END_FOR;

return result;
}

static void add_cache (
    DBB		dbb)
{
/**************************************
 *
 *	a d d _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Add a shared cache file to database.  
 *
 **************************************/
FIL	file;
USHORT	result;

result = FALSE;

FOR FIL IN RDB$FILES WITH FIL.RDB$FILE_FLAGS EQ FILE_cache
    result = TRUE;
    DDL_err (323, FIL.RDB$FILE_NAME, NULL, NULL, NULL, NULL); /* msg 323: a shared cache file %s already exists */
END_FOR;

if (result)
    return;

file = dbb->dbb_cache_file;

STORE FIL IN RDB$FILES
    MOVE_SYMBOL (file->fil_name, FIL.RDB$FILE_NAME);
    FIL.RDB$FILE_START = 0;
    FIL.RDB$FILE_LENGTH = file->fil_length;
    FIL.RDB$FILE_FLAGS = FILE_cache;
END_STORE;

/* Unless there are errors, commit the new shared cache immediately! */

if (!DDL_errors)
    {
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 324, NULL, NULL, NULL, NULL, NULL); /* msg 324: error commiting new shared cache declaration */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;
    }
}

static void add_field (
    FLD	field)
{
/**************************************
 *
 *	a d d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Add a field to a relation.
 *
 **************************************/
REL	relation;
SYM	source;
STATUS	status_vector [20];
SLONG	*blob;

relation = field->fld_relation;
if (!(source = field->fld_source))
    source = field->fld_name; 

if (check_field (relation->rel_name, field->fld_name))
    {
    DDL_err (37, field->fld_name->sym_string, relation->rel_name->sym_string, NULL, NULL, NULL);
	/* msg 37: field %s already exists in relation %s */
    return;
    }

STORE X IN RDB$RELATION_FIELDS
    MOVE_SYMBOL (relation->rel_name, X.RDB$RELATION_NAME);
    MOVE_SYMBOL (field->fld_name, X.RDB$FIELD_NAME);
    MOVE_SYMBOL (source, X.RDB$FIELD_SOURCE);
    X.RDB$SECURITY_CLASS.NULL = TRUE;
    X.RDB$FIELD_POSITION.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;
    X.RDB$QUERY_NAME.NULL = TRUE;
    X.RDB$QUERY_HEADER.NULL = TRUE;
    X.RDB$EDIT_STRING.NULL = TRUE;

    if (field->fld_security_class)
	{
	X.RDB$SECURITY_CLASS.NULL = FALSE;
	MOVE_SYMBOL (field->fld_security_class, X.RDB$SECURITY_CLASS);
	}
    if (field->fld_flags & fld_explicit_position)
	{
	X.RDB$FIELD_POSITION.NULL = FALSE;
	X.RDB$FIELD_POSITION = field->fld_position;
	}
    if (field->fld_query_name)
	{
	MOVE_SYMBOL (field->fld_query_name, X.RDB$QUERY_NAME);
	X.RDB$QUERY_NAME.NULL = FALSE;
	}
    if (field->fld_description)
	{
	store_text (field->fld_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
    if (field->fld_edit_string)
	{
	X.RDB$EDIT_STRING.NULL = FALSE;
	MOVE_SYMBOL (field->fld_edit_string, X.RDB$EDIT_STRING);
	}
    if (field->fld_query_header)
	{
	X.RDB$QUERY_HEADER.NULL = FALSE;
	store_query_header (field->fld_query_header, &X.RDB$QUERY_HEADER);
	}
    if (field->fld_system)
	X.RDB$SYSTEM_FLAG = field->fld_system;
    else
	X.RDB$SYSTEM_FLAG = relation->rel_system;

END_STORE;
}

static void add_files (
    DBB		dbb,
    FIL		files)
{
/**************************************
 *
 *	a d d _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Add a file to an existing database.  
 *
 **************************************/
SLONG	length, start;
FIL	file, next;
TEXT	s [128];
SSHORT	shadow_number;

/* Reverse the order of files (parser left them backwards) */

for (file = files, files = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = files;
    files = file;
    }

/* To assign page ranges, we need a place to start.  Get
   current allocation of database, then check the allocation
   against the user given maximum length (if given). */
 
if (dbb)
    {
    if (gds__database_info (gds__status,
	    GDS_REF (DB),
	    sizeof (alloc_info), 
	    alloc_info,
	    sizeof (s),
	    s) ||
	  s [0] != gds__info_allocation)
	DDL_err (38, NULL, NULL, NULL, NULL, NULL); /* msg 38: gds__database_info failed */

    length = gds__vax_integer (s + 1, 2);
    start = gds__vax_integer (s + 3, length); 

    length = (dbb->dbb_length) ? dbb->dbb_length + 1 : 0;
    start = MAX (start, length);
    }

shadow_number = 0;
for (file = files; file; file = file->fil_next)
    {
    if (file->fil_shadow_number != shadow_number)
	{
	start = 0;
	shadow_number = file->fil_shadow_number;
	}
    else if (!length && !file->fil_start)
	DDL_err (39, file->fil_name->sym_string, NULL, NULL, NULL, NULL); 
	    /* msg 39: Preceding file did not specify length, so %s must include starting page number */

    start = MAX (start, file->fil_start);
    STORE X IN RDB$FILES
	MOVE_SYMBOL (file->fil_name, X.RDB$FILE_NAME);
	X.RDB$FILE_START = start;
	X.RDB$FILE_LENGTH = file->fil_length;
	X.RDB$SHADOW_NUMBER = file->fil_shadow_number;
	X.RDB$FILE_FLAGS = 0;
	if (file->fil_manual)
	    X.RDB$FILE_FLAGS = FILE_manual;
	if (file->fil_conditional)
	    X.RDB$FILE_FLAGS |= FILE_conditional;
    END_STORE;
    length = file->fil_length;
    start += length;
    }

/* Unless there are errors floating around, commit the new file immediately! */

if (!DDL_errors)
    {
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 40, NULL, NULL, NULL, NULL, NULL); /* msg 40: error commiting new file declarations */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;
    }
}

static void add_filter (
    FILTER	filter)
{
/**************************************
 *
 *	a d d _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Add a new filter.
 *
 **************************************/

STORE X IN RDB$FILTERS USING
    X.RDB$MODULE_NAME.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;

    MOVE_SYMBOL (filter->filter_name, X.RDB$FUNCTION_NAME);
    X.RDB$INPUT_SUB_TYPE = filter->filter_input_sub_type;
    X.RDB$OUTPUT_SUB_TYPE = filter->filter_output_sub_type;
    if (filter->filter_module_name)
	{
	X.RDB$MODULE_NAME.NULL = FALSE;
	MOVE_SYMBOL (filter->filter_module_name, X.RDB$MODULE_NAME); 
	}
    MOVE_SYMBOL (filter->filter_entry_point, X.RDB$ENTRYPOINT);
    if (filter->filter_description)
	{
	store_text (filter->filter_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
END_STORE;
}

static void add_function (
    FUNC	function)
{
/**************************************
 *
 *	a d d _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Add a new function.
 *
 **************************************/

if (check_function (function->func_name))
    {
    DDL_err (41, function->func_name->sym_string, NULL, NULL, NULL, NULL); /* msg 41: function %s already exists */
    return;
    }

STORE X IN RDB$FUNCTIONS USING
    X.RDB$QUERY_NAME.NULL = TRUE;
    X.RDB$MODULE_NAME.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;

    MOVE_SYMBOL (function->func_name, X.RDB$FUNCTION_NAME);
    if (function->func_query_name)
	{
	X.RDB$QUERY_NAME.NULL = FALSE;
	MOVE_SYMBOL (function->func_query_name, X.RDB$QUERY_NAME); 
	}
    if (function->func_module_name)
	{
	X.RDB$MODULE_NAME.NULL = FALSE;
	MOVE_SYMBOL (function->func_module_name, X.RDB$MODULE_NAME); 
	}
    MOVE_SYMBOL (function->func_entry_point, X.RDB$ENTRYPOINT);
    if (function->func_description)
	{
	store_text (function->func_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
    X.RDB$RETURN_ARGUMENT = function->func_return_arg;

END_STORE;
}

static void add_function_arg (
    FUNCARG	func_arg)
{
/**************************************
 *
 *	a d d _ f u n c t i o n _ a r g
 *
 **************************************
 *
 * Functional description
 *	Add a new function argument.
 *
 **************************************/

STORE X IN RDB$FUNCTION_ARGUMENTS

    MOVE_SYMBOL (func_arg->funcarg_funcname, X.RDB$FUNCTION_NAME);
    X.RDB$ARGUMENT_POSITION = func_arg->funcarg_position;
    X.RDB$MECHANISM = func_arg->funcarg_mechanism;
    X.RDB$FIELD_TYPE =  func_arg->funcarg_dtype;
    if (func_arg->funcarg_has_sub_type)
	{
	X.RDB$FIELD_SUB_TYPE = func_arg->funcarg_sub_type;
	X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	}
    else
	X.RDB$FIELD_SUB_TYPE.NULL = TRUE;
    X.RDB$FIELD_SCALE = func_arg->funcarg_scale;
    X.RDB$FIELD_LENGTH = func_arg->funcarg_length;

END_STORE;
}

static void add_generator (
    SYM		symbol)
{
/**************************************
 *
 *	a d d _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Add a new generator.
 *
 **************************************/

STORE X IN RDB$GENERATORS
    MOVE_SYMBOL (symbol, X.RDB$GENERATOR_NAME);
END_STORE;
}

static void add_global_field (
    FLD	field)
{
/**************************************
 *
 *	a d d _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Add a field to a relation.
 *
 **************************************/
SYM	name;
DSC	desc;
STATUS	status_vector [20];
SLONG	*blob;

name = field->fld_name;

STORE X IN RDB$FIELDS 

    X.RDB$SEGMENT_LENGTH.NULL = TRUE;
    X.RDB$COMPUTED_BLR.NULL = TRUE;
    X.RDB$COMPUTED_SOURCE.NULL = TRUE;
    X.RDB$VALIDATION_BLR.NULL = TRUE;
    X.RDB$VALIDATION_SOURCE.NULL = TRUE;
    X.RDB$MISSING_VALUE.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;
    X.RDB$QUERY_HEADER.NULL = TRUE;
    X.RDB$QUERY_NAME.NULL = TRUE;
    X.RDB$EDIT_STRING.NULL = TRUE;
    X.RDB$DIMENSIONS.NULL = TRUE;
    X.RDB$DEFAULT_VALUE.NULL = TRUE;
 
    if (field->fld_computed)
	{
	store_blr (field->fld_computed, &X.RDB$COMPUTED_BLR, NULL);
	store_text (field->fld_compute_src, &X.RDB$COMPUTED_SOURCE);
	X.RDB$COMPUTED_BLR.NULL = FALSE;
	X.RDB$COMPUTED_SOURCE.NULL = FALSE;
	if (!field->fld_dtype)
	    {
	    make_desc (field->fld_computed, &desc);
	    field->fld_dtype = desc.dsc_dtype;
	    field->fld_length = desc.dsc_length;
	    if (IS_DTYPE_ANY_TEXT (desc.dsc_dtype))
		{
		field->fld_scale = 0;
	    	field->fld_sub_type = desc.dsc_ttype;
		field->fld_has_sub_type = TRUE;
		}
	    else
		{
		field->fld_sub_type = 0;
		field->fld_scale = desc.dsc_scale;
		}
	    }
	}
 
    if (field->fld_default)
	{
	store_blr (field->fld_default, &X.RDB$DEFAULT_VALUE, NULL);
	X.RDB$DEFAULT_VALUE.NULL = FALSE;
	}
 
    MOVE_SYMBOL (name, X.RDB$FIELD_NAME);
    X.RDB$FIELD_TYPE = (int) field->fld_dtype;
    X.RDB$FIELD_LENGTH = field->fld_length;
    X.RDB$FIELD_SCALE = field->fld_scale;
    X.RDB$SYSTEM_FLAG = field->fld_system;
    X.RDB$FIELD_SUB_TYPE = field->fld_sub_type;

    if (field->fld_segment_length && (X.RDB$FIELD_TYPE == blr_blob))
	{
	X.RDB$SEGMENT_LENGTH = field->fld_segment_length; 
	X.RDB$SEGMENT_LENGTH.NULL = FALSE;
	}
    if (field->fld_missing)
	{
	store_blr (field->fld_missing, &X.RDB$MISSING_VALUE, NULL);
	X.RDB$MISSING_VALUE.NULL = FALSE;
	}
    if (field->fld_validation)
	{
	store_blr (field->fld_validation, &X.RDB$VALIDATION_BLR, NULL);
	store_text (field->fld_valid_src, &X.RDB$VALIDATION_SOURCE);
	X.RDB$VALIDATION_BLR.NULL = FALSE;
	X.RDB$VALIDATION_SOURCE.NULL = FALSE;
	}
    if (field->fld_description)
	{
	store_text (field->fld_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
    if (field->fld_query_name)
	{
	MOVE_SYMBOL (field->fld_query_name, X.RDB$QUERY_NAME);
	X.RDB$QUERY_NAME.NULL = FALSE;
	}
   if (field->fld_edit_string)
	{
	X.RDB$EDIT_STRING.NULL = FALSE;
	MOVE_SYMBOL (field->fld_edit_string, X.RDB$EDIT_STRING);
	}
    if (field->fld_query_header)
	{
	X.RDB$QUERY_HEADER.NULL = FALSE;
	store_query_header (field->fld_query_header, &X.RDB$QUERY_HEADER);
	}

    if (field->fld_dimension)
	{
	X.RDB$DIMENSIONS.NULL = FALSE; 
	X.RDB$DIMENSIONS = field->fld_dimension;
	store_range (field);
	}

END_STORE;
}

static void add_index (
    DUDLEY_IDX	index)
{
/**************************************
 *
 *	a d d _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Add an index to a database.
 *
 **************************************/
SYM	*symbol;
TEXT	s [128];
USHORT	i, size, error, if_any;
STATUS	status_vector [20];
SLONG	*blob;
FLD	field;

error = if_any = FALSE;

FOR X IN RDB$RELATIONS WITH 
	X.RDB$RELATION_NAME = index->idx_relation->sym_string
    if (!X.RDB$VIEW_BLR.NULL)
	{
	DDL_err (42, index->idx_relation->sym_string, NULL, NULL, NULL, NULL); /* msg 42: %s is a view and can not be indexed */
	error = TRUE;
	}

    if_any = TRUE;
END_FOR;
 
if (error)
    return;

if (!if_any)
    {
    DDL_err (43, index->idx_relation->sym_string, NULL, NULL, NULL, NULL); /* msg 43: relation %s doesn't exist  */
    return;
    }

size = 0;
for (i = 0, symbol = index->idx_field; i < index->idx_count; i++, symbol++)
    {
    if (!(field = check_field (index->idx_relation, *symbol)))
	{
	DDL_err (44, index->idx_name->sym_string,
	    (*symbol)->sym_string, index->idx_relation->sym_string, NULL, NULL);
		/* msg 44: index %s: field %s doesn't exist in relation %s */
	return;
	}

    if (field->fld_computed)
	{
	DDL_err (45, index->idx_name->sym_string, 
	    (*symbol)->sym_string, index->idx_relation->sym_string, NULL, NULL);
		/* msg 45: index %s: field %s in %s is computed and can not be a key */
	return;
	}
    size += field->fld_length;
    }

if ((index->idx_count == 1) && size > 254)
    {
    DDL_err (46, (TEXT*) size, index->idx_name->sym_string, NULL, NULL, NULL);
	/* msg 46: combined key length (%d) for index %s is > 254 bytes */ 
    return;
    }
else if ((index->idx_count > 1) && size > 202)
    {
    DDL_err (312, (TEXT*) size, index->idx_name->sym_string, NULL, NULL, NULL);
	/* msg 312:  key length (%d) for compound index %s exceeds 202  */
    return;
    }

STORE X IN RDB$INDICES
    MOVE_SYMBOL (index->idx_name, X.RDB$INDEX_NAME);
    MOVE_SYMBOL (index->idx_relation, X.RDB$RELATION_NAME);
    X.RDB$SEGMENT_COUNT = index->idx_count;
    X.RDB$UNIQUE_FLAG = index->idx_unique;
    X.RDB$INDEX_INACTIVE = (index->idx_inactive) ? TRUE : FALSE;
    if (index->idx_type)
	{
	X.RDB$INDEX_TYPE = index->idx_type;
	X.RDB$INDEX_TYPE.NULL = FALSE;
	}
    else
	X.RDB$INDEX_TYPE.NULL = TRUE;
    if (index->idx_description)
	{
	store_text (index->idx_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
    else
	X.RDB$DESCRIPTION.NULL = TRUE;
END_STORE
    ON_ERROR
	if (gds__status [1] == gds__no_dup)
	    {
	    DDL_err (47, index->idx_name->sym_string, NULL, NULL, NULL, NULL); /* msg 47: index %s already exists */
	    return;
	    }

	{
	DDL_db_error (gds__status, 48, index->idx_name->sym_string, NULL, NULL, NULL, NULL);
	    /* msg 48: error creating index %s */
	return;
	}
    END_ERROR;

/* if there wasn't an index record, clean up any orphan index segments */

FOR X IN RDB$INDEX_SEGMENTS 
	WITH X.RDB$INDEX_NAME = index->idx_name->sym_string
    ERASE X;
END_FOR;


for (i = 0; i < index->idx_count; i++)
    STORE X IN RDB$INDEX_SEGMENTS
	MOVE_SYMBOL (index->idx_name, X.RDB$INDEX_NAME);
	MOVE_SYMBOL (index->idx_field [i], X.RDB$FIELD_NAME);
	X.RDB$FIELD_POSITION = i;
    END_STORE;
}

static void add_log_files (
    DBB		dbb)
{
/**************************************
 *
 *	a d d _ l o g _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Add log files to a database.  
 *
 **************************************/
FIL	file, next;
SSHORT	number;

/* Reverse the order of files (parser left them backwards) */

for (file = dbb->dbb_logfiles, dbb->dbb_logfiles = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = dbb->dbb_logfiles;
    dbb->dbb_logfiles = file;
    }

number = 0;

/* Add default values for file size */

if (dbb->dbb_flags & DBB_log_default)
    {
    STORE X IN RDB$LOG_FILES
	MOVE_SYMBOL (dbb->dbb_name, X.RDB$FILE_NAME);
	X.RDB$FILE_LENGTH 	= 0L;
	X.RDB$FILE_SEQUENCE 	= number++;
	X.RDB$FILE_PARTITIONS 	= 0;
	X.RDB$FILE_FLAGS 	= LOG_serial | LOG_default;
    END_STORE;
    }

for (file = dbb->dbb_logfiles; file; file = file->fil_next)
    {
    STORE X IN RDB$LOG_FILES
	MOVE_SYMBOL (file->fil_name, X.RDB$FILE_NAME);
	X.RDB$FILE_LENGTH 	= file->fil_length;
	X.RDB$FILE_SEQUENCE 	= number++;
	X.RDB$FILE_PARTITIONS 	= file->fil_partitions;
	X.RDB$FILE_FLAGS 	= file->fil_raw; 

	if (dbb->dbb_flags & DBB_log_serial)
	    X.RDB$FILE_FLAGS |= LOG_serial; 
    END_STORE;
    }

if (file = dbb->dbb_overflow)
    {
    STORE X IN RDB$LOG_FILES
	MOVE_SYMBOL (file->fil_name, X.RDB$FILE_NAME);
	X.RDB$FILE_LENGTH 	= file->fil_length;
	X.RDB$FILE_SEQUENCE 	= number++;
	X.RDB$FILE_PARTITIONS 	= file->fil_partitions;
	X.RDB$FILE_FLAGS 	= LOG_serial | LOG_overflow;
    END_STORE;
    }

/* Unless there were errors , commit immediately! */

if (!DDL_errors)
    {
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 321, NULL, NULL, NULL, NULL, NULL); /* msg 321: error commiting new write ahead log declarations */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;
    }
}

static void add_relation (
    REL	relation)
{
/**************************************
 *
 *	a d d _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Add a new relation.
 *
 **************************************/

if (relation->rel_rse)
    {
    add_view (relation);
    return;
    }

if (check_relation (relation->rel_name))
    {
    DDL_err (49, relation->rel_name->sym_string, NULL, NULL, NULL, NULL); /* msg 49: relation %s already exists */
    return;
    }

STORE X IN RDB$RELATIONS

    X.RDB$SECURITY_CLASS.NULL = TRUE;
    X.RDB$EXTERNAL_FILE.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;

    if (relation->rel_security_class)
	{
	X.RDB$SECURITY_CLASS.NULL = FALSE;
	MOVE_SYMBOL (relation->rel_security_class, X.RDB$SECURITY_CLASS);
	}

    if (relation->rel_filename)
	{
	X.RDB$EXTERNAL_FILE.NULL = FALSE;
	MOVE_SYMBOL (relation->rel_filename, X.RDB$EXTERNAL_FILE);
	}

    MOVE_SYMBOL (relation->rel_name, X.RDB$RELATION_NAME);
    if (relation->rel_description)
	{
	store_text (relation->rel_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    

    X.RDB$SYSTEM_FLAG = relation->rel_system;

END_STORE;
}

static void add_security_class (
    SCL		class)
{
/**************************************
 *
 *	a d d _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Add a new security class.
 *
 **************************************/
SYM	name;
USHORT	if_any;

name = class->scl_name;
if_any = FALSE;

FOR X IN RDB$SECURITY_CLASSES WITH X.RDB$SECURITY_CLASS EQ name->sym_string
    if_any = TRUE;
    DDL_err (50, name->sym_string, NULL, NULL, NULL, NULL); /* msg 50: security class %s already exists */
END_FOR;

if (if_any)
    return;

STORE X IN RDB$SECURITY_CLASSES
    MOVE_SYMBOL (name, X.RDB$SECURITY_CLASS);
    if (class->scl_description)
	{
	store_text (class->scl_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}
    else
	X.RDB$DESCRIPTION.NULL = TRUE;
    store_acl (class, &X.RDB$ACL);
END_STORE;
}

static void add_trigger (
    TRG		trigger)
{
/**************************************
 *
 *	a d d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	add a trigger for a relation in rdb$triggers.
 *
 **************************************/
REL		relation;

relation = trigger->trg_relation;

FOR F IN RDB$RELATION_FIELDS CROSS V IN RDB$VIEW_RELATIONS WITH
    F.RDB$RELATION_NAME = V.RDB$VIEW_NAME AND
    F.RDB$RELATION_NAME = relation->rel_name->sym_string
    MODIFY F USING
	F.RDB$UPDATE_FLAG = TRUE;
    END_MODIFY;
END_FOR;

STORE X IN RDB$TRIGGERS
    
    X.RDB$TRIGGER_SOURCE.NULL = TRUE;
    X.RDB$TRIGGER_BLR.NULL = TRUE;
    X.RDB$DESCRIPTION.NULL = TRUE;

    MOVE_SYMBOL (trigger->trg_name, X.RDB$TRIGGER_NAME);
    MOVE_SYMBOL (relation->rel_name, X.RDB$RELATION_NAME);
    X.RDB$TRIGGER_SEQUENCE = trigger->trg_sequence;
    X.RDB$TRIGGER_TYPE = (SSHORT) trigger->trg_type;
    X.RDB$TRIGGER_INACTIVE = trigger->trg_inactive;

    if (trigger->trg_source)
	{
	X.RDB$TRIGGER_SOURCE.NULL = FALSE;
	store_text (trigger->trg_source, &X.RDB$TRIGGER_SOURCE);
	}

    if (trigger->trg_statement)
	{
	X.RDB$TRIGGER_BLR.NULL = FALSE;
	store_blr (trigger->trg_statement, &X.RDB$TRIGGER_BLR, NULL);
	}

    if (trigger->trg_description)
	{
	X.RDB$DESCRIPTION.NULL = FALSE;
	store_text (trigger->trg_description, &X.RDB$DESCRIPTION);
	}

END_STORE;

}

static void add_trigger_msg (
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	a d d _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Add a new trigger message.
 *
 **************************************/

STORE X IN RDB$TRIGGER_MESSAGES

    MOVE_SYMBOL (trigmsg->trgmsg_trg_name, X.RDB$TRIGGER_NAME);
    X.RDB$MESSAGE_NUMBER = trigmsg->trgmsg_number;
    MOVE_SYMBOL (trigmsg->trgmsg_text, X.RDB$MESSAGE);

END_STORE;
}

static void add_type (
    TYP		fldtype)
{
/**************************************
 *
 *	a d d _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Add all new type values for a given field.
 *
 **************************************/
    
STORE X IN RDB$TYPES
    X.RDB$DESCRIPTION.NULL = TRUE;    

    MOVE_SYMBOL (fldtype->typ_field_name, X.RDB$FIELD_NAME);
    MOVE_SYMBOL (fldtype->typ_name, X.RDB$TYPE_NAME);
    X.RDB$TYPE = fldtype->typ_type;
    if (fldtype->typ_description)
	{
	X.RDB$DESCRIPTION.NULL = FALSE;
	store_text (fldtype->typ_description, &X.RDB$DESCRIPTION);
	}
END_STORE;
}

static void add_user_privilege (
    USERPRIV	upriv)
{
/**************************************
 *
 *	a d d _ u s e r _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *	Add as many rdb$user_privilege entries as needed
 *	for the GRANT statement.
 *
 **************************************/
TEXT	grantor [32], priv [7];    
USHORT	if_any, privflag;
USRE	usr;   /* user entry */
UPFE	upf;   /* user privilege field entry */

if_any = FALSE;

/* rdb$owner_name doesn't get filled in until we store the relation */

FOR X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ upriv->userpriv_relation->sym_string
    strcpy (grantor, X.RDB$OWNER_NAME);
    if_any = TRUE;
END_FOR;

if (!if_any)
    {
    DDL_err (54, upriv->userpriv_relation->sym_string, NULL, NULL, NULL, NULL);
	/* msg 54: relation %s doesn't exist */
    return;
    }

/* we need one rdb$user_privileges entry per each user/privilege/field
   combination so start by looping through each user in user list, then
   loop for each privilege, and if update privilege, loop for each field */

for (usr = upriv->userpriv_userlist; usr; usr = usr->usre_next)
    {
    /* start with a fresh copy of privflag and erase each 
       privilege as we take care of it */

    privflag = upriv->userpriv_flags & ~USERPRIV_grant;
    while (privflag)
	{
	upf = NULL;
	if (privflag & USERPRIV_select)
	    {
	    strcpy (priv, UPRIV_SELECT);
	    privflag ^= USERPRIV_select;
	    } 
	else if (privflag & USERPRIV_delete)
	    {
	    strcpy (priv, UPRIV_DELETE);
	    privflag ^= USERPRIV_delete;
	    } 
	else if (privflag & USERPRIV_insert)
	    {
	    strcpy (priv, UPRIV_INSERT);
	    privflag ^= USERPRIV_insert;
	    } 
	else if (privflag & USERPRIV_update)
	    {
	    strcpy (priv, UPRIV_UPDATE);
	    privflag ^= USERPRIV_update;

	    /* no field list implies all fields */

	    if (!upriv->userpriv_upflist)
		store_userpriv (upriv, grantor, priv, usr, upf);
	    else
		/* update is the only privilege with a possible field list */
		for (upf = upriv->userpriv_upflist; upf; upf = upf->upfe_next)
		    store_userpriv (upriv, grantor, priv, usr, upf);

	    continue;
	    } 

	store_userpriv (upriv, grantor, priv, usr, upf);
	}  /* while (privflag) */
    } /* for */
}

static void add_view (
    REL	relation)
{
/**************************************
 *
 *	a d d _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Add a new view relation.
 *
 **************************************/
FLD	field;
REL	target;
NOD	rse, *arg, contexts;
CTX	context;
USHORT	i;

if (check_relation (relation->rel_name))
    {
    DDL_err (55, relation->rel_name->sym_string, NULL, NULL, NULL, NULL); /* msg 55: relation %s already exists */
    return;
    }

rse = relation->rel_rse;

/* Store the relation record proper */

STORE X IN RDB$RELATIONS
    MOVE_SYMBOL (relation->rel_name, X.RDB$RELATION_NAME);
    store_blr (rse, &X.RDB$VIEW_BLR, relation);
    store_text (relation->rel_view_source, &X.RDB$VIEW_SOURCE);
    if (relation->rel_security_class)
	{
	X.RDB$SECURITY_CLASS.NULL = FALSE;
	MOVE_SYMBOL (relation->rel_security_class, X.RDB$SECURITY_CLASS);
	}
    else
	X.RDB$SECURITY_CLASS.NULL = TRUE;

    if (relation->rel_description)
	{
	store_text (relation->rel_description, &X.RDB$DESCRIPTION);
	X.RDB$DESCRIPTION.NULL = FALSE;
	}	    
    else
	X.RDB$DESCRIPTION.NULL = TRUE;

    X.RDB$SYSTEM_FLAG = relation->rel_system;

END_STORE

/* Store VIEW_RELATIONS for each relation in view */

contexts = rse->nod_arg [s_rse_contexts];
for (i = 0, arg = contexts->nod_arg; i < contexts->nod_count; i++, arg++)
    STORE X IN RDB$VIEW_RELATIONS
	context = (CTX) (*arg)->nod_arg [0];
	target = context->ctx_relation;
	MOVE_SYMBOL (relation->rel_name, X.RDB$VIEW_NAME);
	MOVE_SYMBOL (target->rel_name, X.RDB$RELATION_NAME);
	MOVE_SYMBOL (context->ctx_name, X.RDB$CONTEXT_NAME);
	X.RDB$VIEW_CONTEXT = context->ctx_context_id;
    END_STORE;

/* Store the various fields */

for (field = relation->rel_fields; field; field = field->fld_next)
    {
    if (check_field (relation->rel_name, field->fld_name))
	DDL_err (56, field->fld_name->sym_string, relation->rel_name->sym_string, NULL, NULL, NULL);
	    /* msg 56: field %s already exists in relation %s */

    STORE X IN RDB$RELATION_FIELDS
	X.RDB$SECURITY_CLASS.NULL = TRUE;
	X.RDB$FIELD_POSITION.NULL = TRUE;
	X.RDB$DESCRIPTION.NULL = TRUE;
	X.RDB$QUERY_NAME.NULL = TRUE;
	X.RDB$QUERY_HEADER.NULL = TRUE;
	X.RDB$EDIT_STRING.NULL = TRUE;

	MOVE_SYMBOL (relation->rel_name, X.RDB$RELATION_NAME);
	MOVE_SYMBOL (field->fld_name, X.RDB$FIELD_NAME);

	if (field->fld_computed)
	    {
	    MOVE_SYMBOL (field->fld_source, X.RDB$FIELD_SOURCE);
	    X.RDB$BASE_FIELD.NULL = TRUE;
	    X.RDB$VIEW_CONTEXT = 0;
	    }
	else
	    {
	    X.RDB$BASE_FIELD.NULL = FALSE;
	    MOVE_SYMBOL (field->fld_base, X.RDB$BASE_FIELD);
	    context = field->fld_context;
	    target = context->ctx_relation;
	    if (!check_field (target->rel_name, field->fld_base))
		DDL_err (57,
		    field->fld_base->sym_string, 
		    target->rel_name->sym_string, 
		    field->fld_name->sym_string, NULL, NULL);
		    /* msg 57: field %s doesn't exist in relation %s as referenced in view field %s */

	    X.RDB$VIEW_CONTEXT = context->ctx_context_id;
	    FOR RFR IN RDB$RELATION_FIELDS WITH
		    RFR.RDB$RELATION_NAME EQ target->rel_name->sym_string AND
		    RFR.RDB$FIELD_NAME EQ field->fld_base->sym_string
		gds__vtov (RFR.RDB$FIELD_SOURCE,
			   X.RDB$FIELD_SOURCE, sizeof (X.RDB$FIELD_SOURCE));
	    END_FOR;
	    }

	if (field->fld_security_class)
	    {
	    X.RDB$SECURITY_CLASS.NULL = FALSE;
	    MOVE_SYMBOL (field->fld_security_class, X.RDB$SECURITY_CLASS);
	    }
	if (field->fld_flags & fld_explicit_position)
	    {
	    X.RDB$FIELD_POSITION.NULL = FALSE;
	    X.RDB$FIELD_POSITION = field->fld_position;
	    }
	if (field->fld_query_name)
	    {
	    MOVE_SYMBOL (field->fld_query_name, X.RDB$QUERY_NAME);
	    X.RDB$QUERY_NAME.NULL = FALSE;
	    }
	if (field->fld_description)
	    {
	    store_text (field->fld_description, &X.RDB$DESCRIPTION);
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    }	    
	if (field->fld_edit_string)
	    {
	    X.RDB$EDIT_STRING.NULL = FALSE;
	    MOVE_SYMBOL (field->fld_edit_string, X.RDB$EDIT_STRING);
	    }
	if (field->fld_query_header)
	    {
	    X.RDB$QUERY_HEADER.NULL = FALSE;
	    store_query_header (field->fld_query_header, &X.RDB$QUERY_HEADER);
	    }
	if (field->fld_system)
	    X.RDB$SYSTEM_FLAG = field->fld_system;
	else
	    X.RDB$SYSTEM_FLAG = relation->rel_system;
    END_STORE;
    }
}

static void alloc_file_name (
    FIL		*start_file,
    UCHAR	*file_name)
{
/**************************************
 *
 *	a l l o c  _ f i l e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Add a file name only once. No duplicates.
 *
 **************************************/
SYM	string;
FIL	file;
FIL	temp;

for (temp = *start_file; temp; temp = temp->fil_next)
    if (!strcmp (temp->fil_name->sym_name, file_name))
	return;

file = (FIL) DDL_alloc (FIL_LEN);
file->fil_next = *start_file;
*start_file = file;
string = (SYM) DDL_alloc (SYM_LEN + strlen (file_name));
strcpy (string->sym_name, file_name);
file->fil_name = string;
}

static FLD check_field (
    SYM	relation,
    SYM	field)
{
/**************************************
 *
 *	c h e c k _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Check to see if a field already exists in RFR.
 *
 **************************************/
SYM	symbol;

FOR X IN RDB$RELATION_FIELDS WITH 
	X.RDB$RELATION_NAME EQ relation->sym_string AND
	X.RDB$FIELD_NAME EQ field->sym_string
    {
    if (symbol = HSH_typed_lookup (X.RDB$FIELD_SOURCE, (USHORT) 0, SYM_global))
	return (FLD) symbol->sym_object;
    }
END_FOR;

return NULL;
}

static BOOLEAN check_function (
    SYM		name)
{
/**************************************
 *
 *	c h e c k _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Check function for existence.  
 *
 **************************************/
BOOLEAN	if_any;

if_any = FALSE;

FOR X IN RDB$FUNCTIONS WITH X.RDB$FUNCTION_NAME EQ name->sym_string
    if_any = TRUE;
END_FOR;

return if_any;
}

static BOOLEAN check_range (
    FLD		field)
{
/**************************************
 *
 *	c h e c k _ r a n g e 
 *
 **************************************
 *
 * Functional description
 *	Don't let the poor user modify the
 *	datatype of an array unless he has
 *	specifed the indices correctly.  Ahy
 *	deviation in the ranges is assumed
 *	to be an illconsidered modification.
 *
 **************************************/
SLONG	*range;
USHORT	dims;
BASED ON RDB$FIELD_DIMENSIONS.RDB$FIELD_NAME name;
BOOLEAN   if_any, match;

MOVE_SYMBOL (field->fld_name, name);
if_any = match = FALSE;
dims = field->fld_dimension;

/* existance: if neither is an array, life is good
    else if there are dimensions they'd better match */

FOR X IN RDB$FIELDS WITH X.RDB$FIELD_NAME EQ name
    if (!dims)
	{
	if (!X.RDB$DIMENSIONS.NULL)
	    DDL_err (302, name, NULL, NULL, NULL, NULL);
		/* msg 302: Include complete field specification to change datatype of array %s */
	else
	    match = TRUE;
	}
    else if (dims == X.RDB$DIMENSIONS)
	if_any = TRUE;
END_FOR;

if (match)
    return match;

if (!dims)
    return FALSE;

if (if_any)
    {
    range = field->fld_ranges;
    FOR X IN RDB$FIELD_DIMENSIONS WITH X.RDB$FIELD_NAME EQ name
	SORTED BY ASCENDING X.RDB$DIMENSION
	if ((*range++ != X.RDB$LOWER_BOUND) || (*range++ != X.RDB$UPPER_BOUND))
	    if_any = FALSE;
    END_FOR;
    }

if (!if_any)
    DDL_err (301, NULL, NULL, NULL, NULL, NULL);
return if_any;
} 

static BOOLEAN check_relation (
    SYM		name)
{
/**************************************
 *
 *	c h e c k _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Check relation for existence.  
 *
 **************************************/
BOOLEAN	if_any;

if_any = FALSE;

FOR X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ name->sym_string
    if_any = TRUE;
END_FOR;

return if_any;
}

static void close_blob (
    void	*blob)
{
/**************************************
 *
 *	c l o s e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Close a blob.
 *
 **************************************/
STATUS	status_vector [20];

if (gds__close_blob (status_vector,
	GDS_REF (blob)))
    DDL_db_error (status_vector, 58, NULL, NULL, NULL, NULL, NULL); /* msg 58: gds__close_blob failed */
} 

static void *create_blob (
    SLONG	*blob_id,
    USHORT	bpb_length,
    UCHAR	*bpb)
{
/**************************************
 *
 *	c r e a t e _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Create a new blob.
 *
 **************************************/
STATUS	status_vector [20];
void	*blob;

blob = NULL;

if (gds__create_blob2 (status_vector,
	GDS_REF (DB),
	GDS_REF (gds__trans),
	GDS_REF (blob),
	GDS_VAL (blob_id),
	bpb_length,
	bpb))
    {
    DDL_db_error (status_vector, 59, NULL, NULL, NULL, NULL, NULL); /* msg 59: gds__create_blob failed*/
    return NULL;
    }

return blob;
} 

static void drop_cache (
    DBB	dbb)
{
/**************************************
 *
 *	d r o p _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Get rid of shared cache file in the database.
 *
 **************************************/
USHORT	found;

found = FALSE;

FOR FIL IN RDB$FILES WITH FIL.RDB$FILE_FLAGS EQ FILE_cache
    ERASE FIL;
    found = TRUE;
END_FOR;   

if (!found)
    DDL_err (325, NULL, NULL, NULL, NULL, NULL); /* msg 325: no shared cache file exists to drop */

if (!DDL_errors)
    {
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 326, NULL, NULL, NULL, NULL, NULL); /* msg 326: error commiting deletion of shared cache file */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;
    }
}

static void drop_field (
    FLD	field)
{
/**************************************
 *
 *	d r o p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Drop a field from a relation.  First
 *	check that it's not referenced in a
 *	view (other that the current one, if the
 *	current relation happens to be a view).  
 *	If the field is computed, and
 *	the current reference is not a view, 
 *	erase the artificially named source field.
 *
 **************************************/
REL	relation;
USHORT	error, if_any;

relation = field->fld_relation;
error = if_any = FALSE;

FOR X IN RDB$RELATION_FIELDS CROSS Y IN RDB$RELATION_FIELDS CROSS
    Z IN RDB$VIEW_RELATIONS WITH
	X.RDB$RELATION_NAME EQ relation->rel_name->sym_string AND
	X.RDB$FIELD_NAME EQ field->fld_name->sym_string AND
	X.RDB$FIELD_NAME = Y.RDB$BASE_FIELD AND
	X.RDB$FIELD_SOURCE = Y.RDB$FIELD_SOURCE AND
	Y.RDB$RELATION_NAME EQ Z.RDB$VIEW_NAME AND
	X.RDB$RELATION_NAME EQ Z.RDB$RELATION_NAME AND
	Y.RDB$VIEW_CONTEXT EQ Z.RDB$VIEW_CONTEXT
    DDL_err (60, 
	    field->fld_name->sym_string, 
	    relation->rel_name->sym_string,
	    Y.RDB$RELATION_NAME, NULL, NULL);
	    /* msg 60: field %s from relation %s is referenced in view %s */
    error = TRUE;
END_FOR;

if (error)
    return;

FOR FIRST 1 X IN RDB$RELATION_FIELDS WITH 
	X.RDB$RELATION_NAME EQ relation->rel_name->sym_string AND
	X.RDB$FIELD_NAME EQ field->fld_name->sym_string
    FOR Y IN RDB$RELATION_FIELDS WITH 
	    Y.RDB$FIELD_NAME = X.RDB$BASE_FIELD
	if_any = TRUE;
    END_FOR
    if (!if_any)
	FOR Z IN RDB$FIELDS WITH 
		Z.RDB$FIELD_NAME = X.RDB$FIELD_SOURCE 
		AND NOT (Z.RDB$COMPUTED_BLR MISSING)
	    ERASE Z;
	END_FOR;
    ERASE X;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (61, 
	field->fld_name->sym_string, 
	relation->rel_name->sym_string, NULL, NULL, NULL);
	/* msg 61: field %s doesn't exists in relation %s */
}

static void drop_filter (
    FILTER	filter)
{
/**************************************
 *
 *	d r o p _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Get rid of a filter.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;

FOR F IN RDB$FILTERS WITH F.RDB$FUNCTION_NAME EQ filter->filter_name->sym_string
    ERASE F;
    if_any = TRUE;
END_FOR;   

if (!if_any)
    DDL_err (62, filter->filter_name->sym_string, NULL, NULL, NULL, NULL); /* msg 62: filter %s doesn't exist */
}

static void drop_function (
    FUNC	function)
{
/**************************************
 *
 *	d r o p _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Get rid of a function and its arguments.
 *
 **************************************/
TEXT	*name;
USHORT	if_any;

name = function->func_name->sym_string;
if_any = FALSE;

/* Clean out function argument first.  */

FOR FUNCARG IN RDB$FUNCTION_ARGUMENTS WITH FUNCARG.RDB$FUNCTION_NAME EQ name
    ERASE FUNCARG;
END_FOR;

FOR FUNC IN RDB$FUNCTIONS WITH FUNC.RDB$FUNCTION_NAME EQ name
    ERASE FUNC;
    if_any = TRUE;
END_FOR;   

if (!if_any)
    DDL_err (63, name, NULL, NULL, NULL, NULL); /* msg 63: function %s doesn't exist */
}

static void drop_global_field (
    FLD	field)
{
/**************************************
 *
 *	d r o p _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Drop a field from a relation.
 *
 **************************************/
SCHAR	*p;
USHORT	if_any, error;

if_any = FALSE;
error = FALSE;

FOR Y IN RDB$RELATION_FIELDS WITH
	Y.RDB$FIELD_SOURCE EQ field->fld_name->sym_string

    for (p = Y.RDB$FIELD_SOURCE; *p && *p != ' '; p++)
	;
    *p = 0;

    for (p = Y.RDB$RELATION_NAME; *p && *p != ' '; p++)
	;
    *p = 0;

    for (p = Y.RDB$FIELD_NAME; *p && *p != ' '; p++)
	;
    *p = 0;

    DDL_err (64,
	Y.RDB$FIELD_SOURCE,
	Y.RDB$RELATION_NAME,
	Y.RDB$FIELD_NAME, NULL, NULL);
	/* msg 64: field %s is used in relation %s (local name %s) and can not be dropped */
    error = TRUE;
END_FOR;

if (error)
    return;

FOR X IN RDB$TYPES WITH
	X.RDB$FIELD_NAME EQ field->fld_name->sym_string
    ERASE X;
END_FOR;

FOR X IN RDB$FIELD_DIMENSIONS WITH 
	X.RDB$FIELD_NAME EQ field->fld_name->sym_string
    ERASE X;
END_FOR;

FOR X IN RDB$FIELDS WITH 
	X.RDB$FIELD_NAME EQ field->fld_name->sym_string
    ERASE X;
    if_any = TRUE;
END_FOR

if (!if_any)
    DDL_err (65, field->fld_name->sym_string, NULL, NULL, NULL, NULL); /* msg 65: field %s doesn't exist */
}

static void drop_index (
    DUDLEY_IDX		index)
{
/**************************************
 *
 *	d r o p _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Get rid of an index.
 *
 **************************************/
TEXT	*name;
USHORT	if_any;

name = index->idx_name->sym_string;
if_any = FALSE;

/* Clean out index segments first.  */

FOR SEG IN RDB$INDEX_SEGMENTS WITH SEG.RDB$INDEX_NAME EQ name
    ERASE SEG;
END_FOR;

FOR IDX IN RDB$INDICES WITH IDX.RDB$INDEX_NAME EQ name
    ERASE IDX;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (66, name, NULL, NULL, NULL, NULL); /* msg 66: index %s doesn't exist  */
}

static void drop_relation (
    REL		relation)
{
/**************************************
 *
 *	d r o p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Drop a relation, friends, and relatives.
 *
 **************************************/
TEXT	*name;
USHORT	if_any;

name = relation->rel_name->sym_string;
if_any = FALSE;

FOR X IN RDB$VIEW_RELATIONS WITH X.RDB$RELATION_NAME EQ name
    DDL_err (67, name, X.RDB$VIEW_NAME, NULL, NULL, NULL); /* msg 67: %s referenced by view %s */
    return;
END_FOR;

/* check that the relation exists and can be deleted */

FOR X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ name
    if (X.RDB$SYSTEM_FLAG == 1) 
	{
	DDL_err (68, name, NULL, NULL, NULL, NULL); /* msg 68: can't drop system relation %s */
	return;
	}
    if_any = TRUE;
END_FOR;

if (!if_any)
    {
    DDL_err (69, name, NULL, NULL, NULL, NULL); /* msg 69: relation %s doesn't exist */
    return;
    }

/* First, get rid of the relation itself.  This will speed things later on. */

FOR X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ name
    ERASE X;
END_FOR;

/* Clean out RDB$INDICES, RDB$VIEW_RELATIONS and RDB$RELATION_FIELDS */

FOR IDX IN RDB$INDICES WITH IDX.RDB$RELATION_NAME EQ name
    FOR SEG IN RDB$INDEX_SEGMENTS WITH SEG.RDB$INDEX_NAME EQ IDX.RDB$INDEX_NAME
	ERASE SEG;
    END_FOR;
    ERASE IDX;
END_FOR;

FOR X IN RDB$VIEW_RELATIONS WITH X.RDB$VIEW_NAME EQ name
    ERASE X;
END_FOR;

FOR X IN RDB$RELATION_FIELDS WITH X.RDB$RELATION_NAME EQ name
    ERASE X;
END_FOR;

/* Finally, get rid of any user privileges */

FOR PRIV IN RDB$USER_PRIVILEGES WITH PRIV.RDB$RELATION_NAME EQ name AND
    PRIV.RDB$OBJECT_TYPE = obj_relation
    ERASE PRIV;
END_FOR
    ON_ERROR
    END_ERROR;
}

static void drop_security_class (
    SCL		class)
{
/**************************************
 *
 *	d r o p _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Drop an existing security class name.
 *
 **************************************/
SYM	name;
USHORT	if_any;

name = class->scl_name;
if_any = FALSE;

FOR X IN RDB$SECURITY_CLASSES WITH X.RDB$SECURITY_CLASS EQ name->sym_string
    ERASE X;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (70, name->sym_string, NULL, NULL, NULL, NULL); /* msg 70: security class %s doesn't exist */
}

static void drop_shadow (
    SLONG	shadow_number)
{
/**************************************
 *
 *	d r o p _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	Get rid of all files in the database
 *	with the given shadow number.
 *
 **************************************/
SYM	string;
LLS	files, first_file;

files = first_file = NULL;

FOR FIL IN RDB$FILES WITH 
	FIL.RDB$SHADOW_NUMBER EQ shadow_number
    ERASE FIL;
    string = (SYM) DDL_alloc (SYM_LEN + strlen (FIL.RDB$FILE_NAME));
    strcpy (string->sym_name, FIL.RDB$FILE_NAME);
    DDL_push (string, &files);
    if (!first_file)
	first_file = files;
END_FOR;   

if (!files)
    DDL_err (71, (TEXT*) shadow_number, NULL, NULL, NULL, NULL); /* msg 71: shadow %ld doesn't exist */

if (!DDL_errors)
    {
    COMMIT
    ON_ERROR
	DDL_db_error (gds__status, 72, NULL, NULL, NULL, NULL, NULL); /* msg 72: error commiting deletion of shadow */
	ROLLBACK;
    END_ERROR;
    START_TRANSACTION;

    first_file->lls_next = files_to_delete;
    files_to_delete = files;
    }
}

static void drop_trigger (
    TRG		trigger)
{
/**************************************
 *
 *	 d r o p _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	drop a trigger in rdb$triggers.
 *
 **************************************/
TEXT	*name;
USHORT	if_any, others;
BASED_ON RDB$TRIGGERS.RDB$RELATION_NAME relation_name;

name = trigger->trg_name->sym_string;
if_any  = others = FALSE;

/* Clean out possible trigger messages first. */

FOR TM IN RDB$TRIGGER_MESSAGES WITH TM.RDB$TRIGGER_NAME EQ name
    ERASE TM;
END_FOR;

FOR X IN RDB$TRIGGERS WITH X.RDB$TRIGGER_NAME EQ name
    gds__vtov (X.RDB$RELATION_NAME, relation_name, sizeof (relation_name));
    ERASE X;
    if_any = TRUE;
END_FOR;

if (!if_any) 
    DDL_err (73, name, NULL, NULL, NULL, NULL); /* msg 73: Trigger %s doesn't exist */ 

/* clear the update flags on the fields if this is the last remaining   trigger that changes a view */

FOR V IN RDB$VIEW_RELATIONS  
    WITH V.RDB$VIEW_NAME =  relation_name

    FOR F IN RDB$RELATION_FIELDS CROSS T IN RDB$TRIGGERS OVER
	RDB$RELATION_NAME WITH 
	F.RDB$RELATION_NAME = V.RDB$VIEW_NAME
	others = TRUE;
    END_FOR;

    if (!others)
	FOR  F IN RDB$RELATION_FIELDS 
	    WITH F.RDB$RELATION_NAME = V.RDB$VIEW_NAME 
	    MODIFY F USING
		F.RDB$UPDATE_FLAG = FALSE;
	    END_MODIFY;
	END_FOR;

END_FOR;
}
 
static void drop_trigger_msg (
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	d r o p _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Drop a trigger message.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;

FOR FIRST 1 X IN RDB$TRIGGER_MESSAGES WITH
	X.RDB$TRIGGER_NAME EQ trigmsg->trgmsg_trg_name->sym_string AND
	X.RDB$MESSAGE_NUMBER = trigmsg->trgmsg_number; 
    ERASE X;
    if_any = TRUE;
END_FOR;

if (!if_any) 
    DDL_err (74, 
	(TEXT*) trigmsg->trgmsg_number, 
	trigmsg->trgmsg_trg_name->sym_string, NULL, NULL, NULL);
	/* msg 74: Trigger message number %d for trigger %s doesn't exist */
}

static void drop_type (
    TYP		fldtype)
{
/**************************************
 *
 *	d r o p _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Drop a list of types or all types of an existing field.
 *
 **************************************/
USHORT	if_any;

if (fldtype->typ_name->sym_length == 3 &&
    !strncmp (fldtype->typ_name->sym_string, "ALL", 3))
    {
    FOR X IN RDB$TYPES WITH
	    X.RDB$FIELD_NAME EQ fldtype->typ_field_name->sym_string
	ERASE X;
    END_FOR;

    return;
    }

if_any = FALSE;

FOR X IN RDB$TYPES WITH
	X.RDB$FIELD_NAME EQ fldtype->typ_field_name->sym_string AND
	X.RDB$TYPE_NAME EQ fldtype->typ_name->sym_string
    ERASE X;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (75,
	fldtype->typ_name->sym_string, 
	fldtype->typ_field_name->sym_string, NULL, NULL, NULL);
	    /* msg 75: Type %s for field %s doesn't exist */
}

static void drop_user_privilege (
    USERPRIV	upriv)
{
/**************************************
 *
 *	d r o p _ u s e r _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *	Find and delete as many rdb$user_privilege entries 
 *	as needed for the REVOKE statement.
 *
 **************************************/
USHORT	privflag;
TEXT	priv [7];
UPFE	upf;   /* user privilege field entry */
USRE	usr;   /* user entry */

/* we have one rdb$user_privileges entry per each user/privilege/field
   combination so start by looping through each user in user list, then
   loop for each privilege - erase_userpriv takes care of the multiple
   field entries */

for (usr = upriv->userpriv_userlist; usr; usr = usr->usre_next)
    {
    /* start with a fresh copy of privflag and erase each 
       privilege as we take care of it */

    privflag = upriv->userpriv_flags & ~USERPRIV_grant;
    while (privflag)
	{
	upf = NULL;
	if (privflag & USERPRIV_select)
	    {
	    strcpy (priv, UPRIV_SELECT);
	    privflag ^= USERPRIV_select;
	    } 
	else if (privflag & USERPRIV_delete)
	    {
	    strcpy (priv, UPRIV_DELETE);
	    privflag ^= USERPRIV_delete;
	    } 
	else if (privflag & USERPRIV_insert)
	    {
	    strcpy (priv, UPRIV_INSERT);
	    privflag ^= USERPRIV_insert;
	    } 
	else if (privflag & USERPRIV_update)
	    {
	    strcpy (priv, UPRIV_UPDATE);
	    privflag ^= USERPRIV_update;

	    /* no field list implies all fields */

	    if (!upriv->userpriv_upflist)
		erase_userpriv (upriv, priv, usr, upf);
	    else
		/* update is the only privilege with a possible field list */
		for (upf = upriv->userpriv_upflist; upf; upf = upf->upfe_next)
		    erase_userpriv (upriv, priv, usr, upf);
	    } 

	/* don't call erase_userpriv if we just finished update */

	if (strcmp (priv, UPRIV_UPDATE))
	    erase_userpriv (upriv, priv, usr, upf);

	}  /* while (privflag) */
    } /* for */
}

static void erase_userpriv (
    USERPRIV	upriv,
    TEXT	*priv,
    USRE	usr,
    UPFE	upf)
{
/**************************************
 *
 *	e r a s e _ u s e r p r i v
 *
 **************************************
 *
 * Functional description
 *	Erase a user/privilege/field combination for a REVOKE statement.
 *
 **************************************/
TEXT	*fldname, *relname, *usrname;  
USHORT	if_any;

usrname = usr->usre_name->sym_string;
relname = upriv->userpriv_relation->sym_string;
if_any = FALSE;

/* for UPDATE privilege the field is specified */

if (upf)
    {
    fldname = upf->upfe_fldname->sym_string;
    FOR X IN RDB$USER_PRIVILEGES WITH
	    X.RDB$USER EQ usrname AND
	    X.RDB$USER_TYPE EQ obj_user AND
	    X.RDB$PRIVILEGE EQ priv AND
	    X.RDB$FIELD_NAME EQ fldname AND
	    X.RDB$RELATION_NAME EQ relname AND
	    X.RDB$OBJECT_TYPE EQ obj_relation
	ERASE X;
	if_any = TRUE;
    END_FOR;
    if (!if_any)
	DDL_err (76, priv, fldname, relname, usrname, NULL);
	    /* msg 76: User privilege %s on field %s in relation %s for user %s doesn't exist */
    }
else
    {
    FOR X IN RDB$USER_PRIVILEGES WITH
	    X.RDB$USER EQ usrname AND
	    X.RDB$USER_TYPE EQ obj_user AND
	    X.RDB$PRIVILEGE EQ priv AND
	    X.RDB$RELATION_NAME EQ relname AND
	    X.RDB$OBJECT_TYPE EQ obj_relation
	ERASE X;
	if_any = TRUE;
    END_FOR;
    if (!if_any)
	DDL_err (77, priv, relname, usrname, NULL, NULL); 
	    /* msg 77: User privilege %s on relation %s for user %s doesn't exist*/
    }
}

static void get_field_desc (
    FLD		field)
{
/**************************************
 *
 *	g e t _ f i e l d _ d e s c
 *
 **************************************
 *
 * Functional description
 *	Get description for existing field.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;

FOR FLD IN RDB$RELATION_FIELDS CROSS SRC IN RDB$FIELDS
    WITH FLD.RDB$FIELD_NAME = field->fld_name->sym_string
    AND FLD.RDB$RELATION_NAME = field->fld_relation->rel_name->sym_string
    AND FLD.RDB$FIELD_SOURCE = SRC.RDB$FIELD_NAME

    field->fld_dtype = SRC.RDB$FIELD_TYPE;
    field->fld_length = SRC.RDB$FIELD_LENGTH;
    field->fld_scale = SRC.RDB$FIELD_SCALE;
    field->fld_segment_length = SRC.RDB$SEGMENT_LENGTH;
    field->fld_sub_type = SRC.RDB$FIELD_SUB_TYPE;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (78,
	field->fld_name->sym_string,  
	field->fld_relation->rel_name->sym_string, NULL, NULL, NULL);
	    /* msg 78: field %s is unknown in relation %s */
}

static void get_global_fields (void)
{
/**************************************
 *
 *	g e t _ g l o b a l _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Get global fields for database.
 *
 **************************************/
FLD	field;
SYM	symbol;
USHORT	l;
TEXT	*p, *q;

FOR X IN RDB$FIELDS
    field = (FLD) DDL_alloc (FLD_LEN);
    field->fld_name = symbol = get_symbol (SYM_global, X.RDB$FIELD_NAME, field);
    HSH_insert (symbol);
    field->fld_dtype = X.RDB$FIELD_TYPE;
    field->fld_length = X.RDB$FIELD_LENGTH;
    field->fld_scale = X.RDB$FIELD_SCALE;
    field->fld_segment_length = X.RDB$SEGMENT_LENGTH;
    field->fld_sub_type = X.RDB$FIELD_SUB_TYPE;
END_FOR;
}

static void get_log_names (
    SCHAR	*db_name,
    FIL		*start_file,
    SCHAR	*cur_log,
    SLONG	part_offset,
    SSHORT	force,
    SSHORT	skip_delete)
{
/**************************************
 *
 *	g e t _ l o g _ n a m e s
 *
 **************************************
 *
 * Functional description
 *	Walk through list of log files and add to link list.
 *
 **************************************/
SCHAR    next_log [512], expanded_name [512];
SCHAR    *cl, *nl;
int	log_count, ret_val;
SSHORT   not_archived;
SLONG    last_log_flag, next_offset;
SLONG    log_seqno, log_length;
SSHORT	loop;

ISC_expand_filename (db_name, 0, expanded_name);

loop = 0;

/* loop up to 10 times to allow the file to be archived */

while (TRUE)
    {
    loop++;
    if (WALF_get_linked_logs_info (gds__status, expanded_name, cur_log,
	    part_offset, &log_count, next_log, &next_offset,
	    &last_log_flag, &not_archived) != SUCCESS)
	DDL_error_abort (gds__status, 328, NULL, NULL, NULL, NULL, NULL);
		      /* msg 328: error in reading list of log files */

    if ((!not_archived) || force)
	break;

    if (not_archived && skip_delete)
	{
	*start_file = (FIL) NULL;
	return;
	}

    if (loop >= 10)
	DDL_error_abort (NULL_PTR, 329, NULL, NULL, NULL, NULL, NULL); /* msg 329: use CASCADE option to remove log files before archive is done */
    }

if (log_count)
    alloc_file_name (start_file, next_log);
else
    alloc_file_name (start_file, cur_log);

cl = next_log;
nl = cur_log;
part_offset = next_offset;

while (log_count)
    {
    ret_val = WALF_get_next_log_info (gds__status,
	expanded_name,
	cl, part_offset,
	nl, &next_offset,
	&log_seqno, &log_length,
	&last_log_flag, 1);

    if (ret_val == FAILURE)
	DDL_error_abort (gds__status, 328, NULL, NULL, NULL, NULL, NULL); /* msg 328: error in reading list of log files */
    if (ret_val < 0)
	break;

    alloc_file_name (start_file, nl);

    /* switch files */

    if (cl == next_log)
	{
	cl = cur_log;
	nl = next_log;
	}
    else
	{
	cl = next_log;
	nl = cur_log;
	}
    part_offset = next_offset;
    }
}

static void get_log_names_serial (
    FIL		*start_file)
{
/**************************************
 *
 *	g e t _ l o g _ n a m e s _ s e r i a l
 *
 **************************************
 *
 * Functional description
 *	Walk through list of serial log files and add to link list.
 *
 **************************************/
SLONG	*tr1 = 0;

/* 
 * for round robin log files, some of the log files may not have been used
 * when the database is dropped or log is dropped. Add them to the list
 */

START_TRANSACTION tr1;

FOR (TRANSACTION_HANDLE tr1) L IN RDB$LOG_FILES
    if (L.RDB$FILE_FLAGS & LOG_serial)
	continue;
    alloc_file_name (start_file, L.RDB$FILE_NAME);
END_FOR;

COMMIT tr1
    ON_ERROR
    END_ERROR;
}

static void get_relations (
    DBB		database)
{
/**************************************
 *
 *	g e t _ r e l a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Get information on existing relations
 *	and their fields.
 *
 **************************************/
REL	relation;
FLD	field; 
SYM	symbol;
USHORT	l;
TEXT	*p, *q;

FOR R IN RDB$RELATIONS
    relation = (REL) DDL_alloc (REL_LEN);
    relation->rel_name = symbol = get_symbol (SYM_relation, R.RDB$RELATION_NAME, relation);
    HSH_insert (symbol);
    relation->rel_database = database;
    relation->rel_next = database->dbb_relations;
    database->dbb_relations = relation; 
    relation->rel_system = R.RDB$SYSTEM_FLAG;
    FOR RFR IN RDB$RELATION_FIELDS WITH RFR.RDB$RELATION_NAME = R.RDB$RELATION_NAME
	field = (FLD) DDL_alloc (FLD_LEN);
	field->fld_name =  get_symbol (SYM_field, RFR.RDB$FIELD_NAME, field);
	HSH_insert (field->fld_name);
	field->fld_relation = relation;
	field->fld_next = relation->rel_fields;
	relation->rel_fields = field;
	field->fld_position = RFR.RDB$FIELD_POSITION; 
	relation->rel_field_position = MAX (RFR.RDB$FIELD_POSITION, relation->rel_field_position);
	field->fld_source = HSH_typed_lookup (RFR.RDB$FIELD_SOURCE, (USHORT) 0, SYM_global);
    END_FOR;	
END_FOR;
}

static USHORT get_prot_mask (
    TEXT	*relation,
    TEXT	*field)
{
/**************************************
 *
 *	g e t _ p r o t _ m a s k
 *
 **************************************
 *
 * Functional description
 *	returns the protection mask for a relation
 *	or field (if the field is an empty string it
 *	gets the mask for the relation).
 *
 **************************************/
static SCHAR blr [] = {
    blr_version4,
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
static int req_handle;
struct {
    SCHAR  relation_name [32];
    SCHAR  field_name [32];
    } msg;
struct {
    SSHORT prot_mask;
    } output;

strcpy (msg.relation_name, relation);
if (field)
    strcpy (msg.field_name, field);
else
    strcpy (msg.field_name, "");

if (!req_handle)
    isc_compile_request2 ((SLONG*) 0, &DB, &req_handle, sizeof (blr), blr);
isc_start_request ((SLONG*) 0, &req_handle, &gds__trans, 0);
isc_start_and_send ((SLONG*) 0, &req_handle, &gds__trans, 0, sizeof (msg), &msg, 0);
isc_receive ((SLONG*) 0, &req_handle, 1, sizeof (output), &output, 0);

return output.prot_mask;
}

static SYM get_symbol (
    enum sym_t	type,
    TEXT	*string,
    CTX		object)
{
/**************************************
 *
 *	g e t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Allocate and format symbol block for null or blank terminated string.
 *
 **************************************/
TEXT	*p;
USHORT	l;
SYM	symbol;

for (p = string; *p && *p != ' '; p++)
    ;

l = p - string;
symbol = (SYM) DDL_alloc (SYM_LEN + l);
symbol->sym_object = object;
symbol->sym_type = type;
symbol->sym_string = p = symbol->sym_name;

if (symbol->sym_length = l)
    do *p++ = *string++; while (--l);

return symbol;
}

static void get_triggers (
    DBB		database)
{
/**************************************
 *
 *	g e t _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Get information on existing relations
 *	and their fields.
 *
 **************************************/
TRG	trigger;
SYM	symbol, rel_name;
USHORT	l;
TEXT	*p, *q;

FOR T IN RDB$TRIGGERS WITH T.RDB$SYSTEM_FLAG MISSING OR T.RDB$SYSTEM_FLAG = 0 
    trigger = (TRG) DDL_alloc (TRG_LEN);
    trigger->trg_name = symbol = get_symbol (SYM_trigger, T.RDB$TRIGGER_NAME, trigger);
    HSH_insert (symbol);
    rel_name = HSH_typed_lookup (T.RDB$RELATION_NAME, (USHORT) 0, SYM_relation);
    if (rel_name && rel_name->sym_object)
	trigger->trg_relation = (REL) rel_name->sym_object;    
    else
	trigger->trg_relation = (REL) get_symbol (SYM_relation, T.RDB$RELATION_NAME, NULL);
    trigger->trg_type =	(TRG_T) T.RDB$TRIGGER_TYPE; 
    trigger->trg_inactive = T.RDB$TRIGGER_INACTIVE; 
    trigger->trg_sequence = T.RDB$TRIGGER_SEQUENCE; 
END_FOR;
}

static void get_udfs (
    DBB		dbb)
{
/**************************************
 *
 *	g e t _ u d f s
 *
 **************************************
 *
 * Functional description
 *	Pickup any user defined functions.
 *
 **************************************/
FUNC	function;
FUNCARG	arg;
SYM	symbol;

FOR FUN IN RDB$FUNCTIONS CROSS ARG IN RDB$FUNCTION_ARGUMENTS WITH
	FUN.RDB$FUNCTION_NAME EQ ARG.RDB$FUNCTION_NAME AND
	FUN.RDB$RETURN_ARGUMENT EQ ARG.RDB$ARGUMENT_POSITION
    function = (FUNC) DDL_alloc (FUNC_LEN);
    function->func_database = dbb;
    function->func_name = symbol = get_symbol (SYM_function, FUN.RDB$FUNCTION_NAME, function);
    HSH_insert (symbol);
    function->func_return = arg = (FUNCARG) DDL_alloc (FUNCARG_LEN);
    arg->funcarg_dtype = ARG.RDB$FIELD_TYPE;
    arg->funcarg_scale = ARG.RDB$FIELD_SCALE;
    arg->funcarg_length = ARG.RDB$FIELD_LENGTH;
    arg->funcarg_sub_type = ARG.RDB$FIELD_SUB_TYPE;
    arg->funcarg_has_sub_type = !(ARG.RDB$FIELD_SUB_TYPE.NULL);
END_FOR
    ON_ERROR
    END_ERROR;
}

static void make_desc (
    NOD		node,
    DSC		*desc)
{
/**************************************
 *
 *	m a k e _ d e s c
 *
 **************************************
 *
 * Functional description
 *	Invent a descriptor based on expression tree.
 *
 **************************************/
FLD	field;
REL	relation;
DSC	desc1, desc2;
CTX	context;
CON	constant;
FUNC	function;
USHORT	dtype;
FUNCARG	arg;

switch (node->nod_type)
    {
    case nod_max:
    case nod_min:
    case nod_from:
	make_desc (node->nod_arg [s_stt_value], desc);
	return;
    
    case nod_total:
	make_desc (node->nod_arg [s_stt_value], &desc1);
	dtype = make_dtype (desc1.dsc_dtype);
	switch (dtype)
	    {
	    case dtype_short:
	    case dtype_long:
		desc->dsc_dtype = blr_long;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = desc1.dsc_scale;
		desc->dsc_sub_type = 0;
		return;

	    case dtype_timestamp:
	    case dtype_sql_date:
	    case dtype_sql_time:
		DDL_msg_put (80, NULL, NULL, NULL, NULL, NULL); /* msg 80: TOTAL of date not supported */
	  	return;

	    default: 
		desc->dsc_dtype = blr_double;
		desc->dsc_length = sizeof (double);
		desc->dsc_scale = 0;
		desc->dsc_sub_type = 0;
		return;
	    }
	break;
 
    case nod_average:
    case nod_divide:
	desc->dsc_dtype = blr_double;
	desc->dsc_length = sizeof (double);
	desc->dsc_scale = 0;
	desc->dsc_sub_type = 0;
	return;
    
    case nod_count:
	desc->dsc_dtype = blr_long;
	desc->dsc_length = sizeof (SLONG);
	desc->dsc_scale = 0;
	desc->dsc_sub_type = 0;
	return;
    
    case nod_add:
    case nod_subtract:
	make_desc (node->nod_arg [0], &desc1);
	make_desc (node->nod_arg [1], &desc2);
	dtype = MAX (make_dtype (desc1.dsc_dtype), make_dtype (desc2.dsc_dtype));
	switch (dtype)
	    {
	    case dtype_short:
	    case dtype_long:
		desc->dsc_dtype = blr_long;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale =
		    MIN (NUMERIC_SCALE (desc1), NUMERIC_SCALE (desc2));
		desc->dsc_sub_type = 0;
		return;

	    case dtype_timestamp:
		if (node->nod_type == nod_add)
		    {
		    desc->dsc_dtype = blr_timestamp;
		    desc->dsc_length = 8;
		    desc->dsc_scale = 0;
		    desc->dsc_sub_type = 0;
		    return;
		    }
		break;

	    case dtype_sql_date:
		if (node->nod_type == nod_add)
		    {
		    desc->dsc_dtype = blr_sql_date;
		    desc->dsc_length = 4;
		    desc->dsc_scale = 0;
		    desc->dsc_sub_type = 0;
		    return;
		    }
		break;

	    case dtype_sql_time:
		if (node->nod_type == nod_add)
		    {
		    desc->dsc_dtype = blr_sql_time;
		    desc->dsc_length = 4;
		    desc->dsc_scale = 0;
		    desc->dsc_sub_type = 0;
		    return;
		    }
		break;
	    }
	desc->dsc_dtype = blr_double;
	desc->dsc_length = sizeof (double);
	desc->dsc_scale = 0;
	desc->dsc_sub_type = 0;
	return;

    case nod_multiply:
	make_desc (node->nod_arg [0], &desc1);
	make_desc (node->nod_arg [1], &desc2);
	dtype = MAX (make_dtype (desc1.dsc_dtype), make_dtype (desc2.dsc_dtype));
	if (dtype == dtype_short || dtype == dtype_long)
	    {
	    desc->dsc_dtype = blr_long;
	    desc->dsc_length = sizeof (SLONG);
	    desc->dsc_sub_type = 0;
	    desc->dsc_scale = NUMERIC_SCALE (desc1) + NUMERIC_SCALE (desc2);
	    return;
	    }
	desc->dsc_dtype = blr_double;
	desc->dsc_length = sizeof (double);
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	return;

    case nod_concatenate:
	make_desc (node->nod_arg [0], &desc1);
	make_desc (node->nod_arg [1], &desc2);
	desc->dsc_dtype = blr_varying;
	desc->dsc_sub_type = 0;
	desc->dsc_scale = 0;
	dtype = make_dtype (desc1.dsc_dtype);
	if (dtype > dtype_any_text)
	    {
	    desc1.dsc_length = string_length (dtype);
	    desc->dsc_ttype = ttype_ascii;
	    }
	else
	    desc->dsc_ttype = desc1.dsc_ttype;
	dtype = make_dtype (desc2.dsc_dtype);
	if (dtype > dtype_any_text)
	    desc2.dsc_length = string_length (dtype);
	desc->dsc_length = desc1.dsc_length + desc2.dsc_length;
	return;

    case nod_field:
	field = (FLD) node->nod_arg [s_fld_field];
	if (!field->fld_dtype)
	    get_field_desc (field);
	desc->dsc_dtype = field->fld_dtype;
	desc->dsc_length = field->fld_length;
	desc->dsc_scale = field->fld_scale;
	desc->dsc_sub_type = 0;
	if (IS_DTYPE_ANY_TEXT (desc->dsc_dtype))
	    desc->dsc_ttype = field->fld_sub_type;
	return;

    case nod_literal:
	constant = (CON) node->nod_arg [0];
	desc->dsc_length = constant->con_desc.dsc_length;
	desc->dsc_scale = constant->con_desc.dsc_scale;
	desc->dsc_sub_type = 0; 
	/* Note: GDEF doesn't have any knowledge of alternate character
	   sets (GDML is frozen) - so literal constants appearing
	   in GDEF must be ttype_ascii.  Note they are not ttype_dynamic
	   as we are defining data objects, which cannot be "typed"
	   at runtime
	 */
	dtype = constant->con_desc.dsc_dtype;
	switch (dtype)
	    {
	    case dtype_short:	desc->dsc_dtype = blr_short;	break; 
	    case dtype_long:	desc->dsc_dtype = blr_long;	break;
	    case dtype_quad:	desc->dsc_dtype = blr_quad;	break;
	    case dtype_real:	desc->dsc_dtype = blr_float;	break;
	    case dtype_double:	desc->dsc_dtype = blr_double;	break;
	    case dtype_text:	desc->dsc_dtype = blr_text;	
				desc->dsc_sub_type = ttype_ascii;
				break;
	    case dtype_varying:	desc->dsc_dtype = blr_varying;	
				desc->dsc_sub_type = ttype_ascii;
				break;
	    case dtype_cstring:	desc->dsc_dtype = blr_cstring;	    
				desc->dsc_sub_type = ttype_ascii;
				break;
	    case dtype_timestamp:desc->dsc_dtype = blr_timestamp;break;
	    case dtype_sql_date:desc->dsc_dtype = blr_sql_date;	break;
	    case dtype_sql_time:desc->dsc_dtype = blr_sql_time;	break;
	    case dtype_int64:   desc->dsc_dtype = blr_int64;	break;
	    case dtype_blob:	desc->dsc_dtype = blr_blob;	break;	
	    default:		desc->dsc_dtype = blr_double;	break;
	    }
	return;

    case nod_function:
	function = (FUNC) node->nod_arg [2];
	arg = function->func_return;
	desc->dsc_dtype = arg->funcarg_dtype;
	desc->dsc_length = arg->funcarg_length;
	desc->dsc_scale = arg->funcarg_scale;
	desc->dsc_sub_type = 0;
	if (IS_DTYPE_ANY_TEXT (desc->dsc_dtype))
	    desc->dsc_ttype = arg->funcarg_sub_type;
	return;

    case nod_fid:
	DDL_err (287, NULL, NULL, NULL, NULL, NULL); /* msg 287: Inappropriate self reference */
	return;
    default:
	DDL_err (81, NULL, NULL, NULL, NULL, NULL); /* msg 81: (EXE) make_desc: don't understand node type */
	return;
    }
}

static int make_dtype (
    USHORT	blr_type)
{
/**************************************
 *
 *	m a k e _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Translate from blr to internal data types.
 *
 **************************************/

switch (blr_type)
    {
    case blr_short :	return dtype_short;
    case blr_long :	return dtype_long;
    case blr_quad :	return dtype_quad;
    case blr_float :	return dtype_real;
    case blr_double :	return dtype_double;
    case blr_text :	return dtype_text;
    case blr_cstring :	return dtype_cstring; 
    case blr_varying :	return dtype_varying;
    case blr_timestamp :  return dtype_timestamp;
    case blr_sql_date :  return dtype_sql_date;
    case blr_sql_time :  return dtype_sql_time;
    case blr_int64 :	return dtype_int64;
    }

if (blr_type == blr_blob)
    return dtype_blob;

return dtype_double;
}

static void modify_field (
    FLD	field)
{
/**************************************
 *
 *	m o d i f y _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Change attributes of a local field.
 *	The first group of attributes are ones
 *	that should never be false.  The second
 *	set also manipulate the missing flag so
 *	if they came in null and weren't changed
 *	they'll still be null
 *
 **************************************/
SYM	name;
TEXT	*p;
USHORT	if_any;
REL	relation;

relation = field->fld_relation;
name = field->fld_name;
if_any = FALSE;

FOR FIRST 1 X IN RDB$RELATION_FIELDS WITH X.RDB$FIELD_NAME EQ name->sym_string AND
	X.RDB$RELATION_NAME EQ relation->rel_name->sym_string
    MODIFY X USING
	if (field->fld_flags & fld_null_description)
	    X.RDB$DESCRIPTION.NULL = TRUE;
	if (field->fld_flags & fld_null_security_class)
	    X.RDB$SECURITY_CLASS.NULL = TRUE;
	if (field->fld_flags & fld_null_edit_string)
	    X.RDB$EDIT_STRING.NULL = TRUE;
	if (field->fld_flags & fld_null_query_name)
	    X.RDB$QUERY_NAME.NULL = TRUE;
	if (field->fld_flags & fld_null_query_header)
	    X.RDB$QUERY_HEADER.NULL = TRUE;

	if (field->fld_flags & fld_explicit_system)
	    X.RDB$SYSTEM_FLAG = field->fld_system;

	if (field->fld_source)
	    MOVE_SYMBOL (field->fld_source, X.RDB$FIELD_SOURCE);

	if (field->fld_security_class)
	    {
	    MOVE_SYMBOL (field->fld_security_class, X.RDB$SECURITY_CLASS);
	    X.RDB$SECURITY_CLASS.NULL = FALSE;
	    }
	if ((field->fld_flags & fld_explicit_position) || field->fld_position)
	    {
	    X.RDB$FIELD_POSITION = field->fld_position;
	    X.RDB$FIELD_POSITION.NULL = FALSE;
	    }
	if (field->fld_description)
	    {
	    store_text (field->fld_description, &X.RDB$DESCRIPTION);
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    }
	if (field->fld_query_name)
	    {
	    MOVE_SYMBOL (field->fld_query_name, X.RDB$QUERY_NAME); 
	    X.RDB$QUERY_NAME.NULL = FALSE;
	    }
	if (field->fld_edit_string)
	    {
	    MOVE_SYMBOL (field->fld_edit_string, X.RDB$EDIT_STRING);
	    X.RDB$EDIT_STRING.NULL = FALSE;
	    }
	if (field->fld_query_header)
	    {
	    store_query_header (field->fld_query_header, &X.RDB$QUERY_HEADER);
	    X.RDB$QUERY_HEADER.NULL = FALSE;
	    }

    END_MODIFY
    if_any = TRUE;
END_FOR

if (!if_any)
    DDL_err (82, name->sym_string, NULL, NULL, NULL, NULL); /* msg 82: field %s doesn't exist */
}

static void modify_global_field (
    FLD	field)
{
/**************************************
 *
 *	m o d i f y _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Change attributes of a global field.
 *
 **************************************/
SYM	name;
TEXT	*p;
USHORT	if_any;

name = field->fld_name;
if_any = FALSE;

FOR FIRST 1 X IN RDB$FIELDS WITH X.RDB$FIELD_NAME EQ name->sym_string
    MODIFY X USING
	if (field->fld_dtype)
	    {
	    if ((X.RDB$FIELD_TYPE != blr_blob) && (field->fld_dtype != blr_blob))
		{
		if ((check_range (field)))
		    {
		    X.RDB$FIELD_TYPE = (int) field->fld_dtype;
		    X.RDB$FIELD_LENGTH = field->fld_length;
		    X.RDB$FIELD_SCALE = field->fld_scale;
		    }
		}
	    else if (field->fld_dtype != X.RDB$FIELD_TYPE)
		DDL_err (83, name->sym_name, NULL, NULL, NULL, NULL);
		    /* msg 83: Unauthorized attempt to change field %s to or from blob */
	    }
	if (field->fld_flags & fld_explicit_system)
	    X.RDB$SYSTEM_FLAG = field->fld_system;

	if (field->fld_flags & fld_null_description)
	    X.RDB$DESCRIPTION.NULL = TRUE; 

	if (field->fld_flags & fld_null_missing_value)
	    X.RDB$MISSING_VALUE.NULL = TRUE; 

	if (field->fld_flags & fld_null_query_name)
	    X.RDB$QUERY_NAME.NULL = TRUE; 

	if (field->fld_flags & fld_null_query_header)
	    X.RDB$QUERY_HEADER.NULL = TRUE; 

	if (field->fld_flags & fld_null_edit_string)
	    X.RDB$EDIT_STRING.NULL = TRUE; 

	if (field->fld_flags & fld_null_validation)
	    {
	    X.RDB$VALIDATION_BLR.NULL = TRUE;
	    X.RDB$VALIDATION_SOURCE.NULL = TRUE;
	    }

	if (field->fld_segment_length)
	    {
	    X.RDB$SEGMENT_LENGTH = field->fld_segment_length;
	    X.RDB$SEGMENT_LENGTH.NULL = FALSE;
	    }

	if (field->fld_has_sub_type)
	    {
	    X.RDB$FIELD_SUB_TYPE = field->fld_sub_type;
	    X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	    }

    	if (field->fld_missing)
	    {
	    store_blr (field->fld_missing, &X.RDB$MISSING_VALUE, NULL);
	    X.RDB$MISSING_VALUE.NULL = FALSE;
	    }
	if (field->fld_validation)
	    {
	    store_blr (field->fld_validation, &X.RDB$VALIDATION_BLR, NULL);
	    store_text (field->fld_valid_src, &X.RDB$VALIDATION_SOURCE);
	    X.RDB$VALIDATION_BLR.NULL = FALSE;
	    X.RDB$VALIDATION_SOURCE.NULL = FALSE;
	    }
	if (field->fld_description)
	    {
	    store_text (field->fld_description, &X.RDB$DESCRIPTION);
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    }	    
	if (field->fld_query_name)
	    {
	    MOVE_SYMBOL (field->fld_query_name, X.RDB$QUERY_NAME);
	    X.RDB$QUERY_NAME.NULL = FALSE;
	    }
	if (field->fld_edit_string)
	    {
	    MOVE_SYMBOL (field->fld_edit_string, X.RDB$EDIT_STRING);
	    X.RDB$EDIT_STRING.NULL = FALSE;
	    }
	if (field->fld_query_header)
	    {
	    store_query_header (field->fld_query_header, &X.RDB$QUERY_HEADER);
	    X.RDB$QUERY_HEADER.NULL = FALSE;
	    }
    END_MODIFY
    if_any = TRUE;
END_FOR

if (!if_any)
    DDL_err (84, name->sym_string, NULL, NULL, NULL, NULL); /* msg 84: field %s doesn't exist */

} 

static void modify_index (
    DUDLEY_IDX	index)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Modify an index.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;
		
FOR X IN RDB$INDICES WITH X.RDB$INDEX_NAME = index->idx_name->sym_string
    MODIFY X USING
	if (index->idx_flags & IDX_active_flag)
	    X.RDB$INDEX_INACTIVE = (index->idx_inactive) ? TRUE : FALSE;
	if (index->idx_flags & IDX_unique_flag)
	    X.RDB$UNIQUE_FLAG = index->idx_unique;
	if (index->idx_flags & IDX_type_flag)
	    {
	    X.RDB$INDEX_TYPE = index->idx_type;
	    X.RDB$INDEX_TYPE.NULL = FALSE;
	    }
	if (index->idx_flags & IDX_null_description)
	    X.RDB$DESCRIPTION.NULL = TRUE;
	if (index->idx_description)
	    {
	    store_text (index->idx_description, &X.RDB$DESCRIPTION);
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    }
	if (index->idx_flags & IDX_statistics_flag)
	    X.RDB$STATISTICS = -1.0;
    END_MODIFY;
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (85, index->idx_name->sym_string, NULL, NULL, NULL, NULL); /* msg 85: index %s does not exist */
}

static void modify_relation (
    REL relation)
{
/**************************************
 *
 *	m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Change the volitile attributes of a relation -
 *	to wit, the description, security class, and
 *	system flag.
 *
 **************************************/
SYM	name;
TEXT	*p;
USHORT	if_any;

if (!relation->rel_flags & rel_marked_for_modify)
    return;
name = relation->rel_name;
if_any = FALSE;

FOR FIRST 1 X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ name->sym_string
    MODIFY X USING
	if (relation->rel_flags & rel_null_security_class)
	    X.RDB$SECURITY_CLASS.NULL = TRUE;
	if (relation->rel_security_class)
	    {
	    MOVE_SYMBOL (relation->rel_security_class, X.RDB$SECURITY_CLASS);
	    X.RDB$SECURITY_CLASS.NULL = FALSE;
	    }
	if (relation->rel_flags & rel_null_description)
	    X.RDB$DESCRIPTION.NULL = TRUE;
	if (relation->rel_description)
	    {
	    store_text (relation->rel_description, &X.RDB$DESCRIPTION);
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    }
	if (relation->rel_flags & rel_explicit_system)
 	    X.RDB$SYSTEM_FLAG = relation->rel_system;
	if (relation->rel_filename)
	    {
	    if (X.RDB$EXTERNAL_FILE.NULL)
		DDL_err (86, name->sym_string, NULL, NULL, NULL, NULL); /* msg 86: relation %s is not external */
	    MOVE_SYMBOL (relation->rel_filename, X.RDB$EXTERNAL_FILE);
	    }
    END_MODIFY
    if_any = TRUE;
END_FOR

if (!if_any)
    DDL_err (87, name->sym_string, NULL, NULL, NULL, NULL); /* msg 87: relation %s doesn't exist */
}

static void modify_trigger (
    TRG		trigger)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	modify a trigger in rdb$triggers.
 *
 **************************************/
USHORT	if_any;
REL	relation;
TEXT	buffer[32], *p, *q ;

if_any = FALSE;

FOR FIRST 1 X IN RDB$TRIGGERS WITH
	X.RDB$TRIGGER_NAME EQ trigger->trg_name->sym_string
    MODIFY X USING

    if (relation = trigger->trg_relation)
	{
	for (p = buffer, q = X.RDB$RELATION_NAME; *q && *q != ' ';)
		*p++ = *q++;
	*p = 0;
	if (strcmp (relation->rel_name->sym_string, buffer))
	    DDL_err (88, trigger->trg_name->sym_string, NULL, NULL, NULL, NULL); 
		/* msg 88: Invalid attempt to assign trigger %s to a new relation */
	}

    if (trigger->trg_mflag & trg_mflag_onoff)
	X.RDB$TRIGGER_INACTIVE = trigger->trg_inactive;
    if (trigger->trg_mflag & trg_mflag_type) 
	X.RDB$TRIGGER_TYPE = (SSHORT) trigger->trg_type;
    if (trigger->trg_mflag & trg_mflag_seqnum)
	X.RDB$TRIGGER_SEQUENCE = trigger->trg_sequence;

    if (trigger->trg_source)
	{
	X.RDB$TRIGGER_SOURCE.NULL = FALSE;
	store_text (trigger->trg_source, &X.RDB$TRIGGER_SOURCE);
	}

    if (trigger->trg_statement)
	{
	X.RDB$TRIGGER_BLR.NULL = FALSE;
	store_blr (trigger->trg_statement, &X.RDB$TRIGGER_BLR, NULL);
	}

    if (trigger->trg_description)
	{
	X.RDB$DESCRIPTION.NULL = FALSE;
	store_text (trigger->trg_description, &X.RDB$DESCRIPTION);
	}
    END_MODIFY;   
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (89, trigger->trg_name->sym_string, NULL, NULL, NULL, NULL); /* msg 89: Trigger %s doesn't exist */
}

static void modify_trigger_msg (
    TRGMSG	trigmsg)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Modify a trigger message.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;

FOR FIRST 1 X IN RDB$TRIGGER_MESSAGES WITH
	X.RDB$TRIGGER_NAME EQ trigmsg->trgmsg_trg_name->sym_string AND
	X.RDB$MESSAGE_NUMBER = trigmsg->trgmsg_number; 
    MODIFY X USING
	MOVE_SYMBOL (trigmsg->trgmsg_text, X.RDB$MESSAGE);
    END_MODIFY;   
    if_any = TRUE;
END_FOR;

if (!if_any)
    add_trigger_msg (trigmsg);
}

static void modify_type (
    TYP		fldtype)
{
/**************************************
 *
 *	m o d i f y _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Modify type value or description of a type for a field.
 *
 **************************************/
USHORT	if_any;

if_any = FALSE;
    
FOR FIRST 1 X IN RDB$TYPES WITH
	X.RDB$FIELD_NAME EQ fldtype->typ_field_name->sym_string AND
	X.RDB$TYPE_NAME EQ fldtype->typ_name->sym_string;
    MODIFY X USING
	X.RDB$TYPE = fldtype->typ_type;
	if (fldtype->typ_description)
	    {
	    X.RDB$DESCRIPTION.NULL = FALSE;
	    store_text (fldtype->typ_description, &X.RDB$DESCRIPTION);
	    }
    END_MODIFY;   
    if_any = TRUE;
END_FOR;

if (!if_any)
    DDL_err (90, fldtype->typ_name->sym_string, fldtype->typ_field_name->sym_string, NULL, NULL, NULL);
	/* msg 90: Type %s for field %s doesn't exist */
}

static void move_symbol (
    SYM			symbol,
    register SCHAR	*field,
    SSHORT		length)
{
/**************************************
 *
 *	m o v e _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Move a string from a symbol to a field.
 *
 **************************************/
register SCHAR	*p;
register SSHORT	l;

if (symbol)
    {
    if ((l = symbol->sym_length) > length)
	{
	DDL_err (296, symbol->sym_string, (TEXT*) length, NULL, NULL, NULL);
	    /*msg 296: symbol %s is too long, truncating it to %ld characters */
	DDL_errors--;
	l = length;
	}
    p = symbol->sym_string;
    while (l--) *field++ = *p++;
    }

*field = 0;
}

static void set_generator (
    NOD		node)
{
/**************************************
 *
 *	s e t _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	if the user has write privs for
 *	rdb$generators, get current value,
 *	figure out the difference, and use
 *	that as the increment for set.  give
 *	it half a dozen tries in case someone
 *	else is interfering.
 *
 **************************************/
int		*req_handle1, *req_handle2;
USHORT		prot_mask, i;
SLONG		*number;
SLONG		new;
SSHORT		l;
struct str	current_blr, new_blr;
SCHAR		*blr;
SLONG		gen_value;
CON		constant;

prot_mask = get_prot_mask ("RDB$GENERATORS", NULL);
if (!(prot_mask & SCL_write))
    {
    DDL_err (311, NULL, NULL, NULL, NULL, NULL); /* msg 311: Set generator requires write privilege for RDB$GENERATORS. */
    return;
    }

/* set up node to get the current value of the generator */

constant = (CON) node->nod_arg[0]->nod_arg[0];
number = (SLONG *) constant->con_data;
new = *number;
*number = 0;

current_blr.str_current = current_blr.str_start = (SCHAR*) gds__alloc ((SLONG) 1028);
current_blr.str_length = 1028;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
gds_alloc_flag_unfreed (current_blr.str_start);
#endif

GENERATE_blr (&current_blr, node);
l = current_blr.str_current - current_blr.str_start;

req_handle1 = (int *) NULL_PTR;
blr = (SCHAR*) current_blr.str_start;
isc_compile_request2 ((SLONG*) 0, &DB, &req_handle1, l, blr);
isc_start_request ((SLONG*) 0, &req_handle1, &gds__trans, 0);
isc_receive ((SLONG*) 0, &req_handle1, 0, sizeof (gen_value), &gen_value, 0);
isc_release_request ((SLONG*) 0, &req_handle1);

/* the difference between output.current and new is the amount we
   want to change the generator by */

*number = new - gen_value;
new_blr.str_current = new_blr.str_start = (SCHAR*) gds__alloc ((SLONG) 1028);
new_blr.str_length = 1028;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
gds_alloc_flag_unfreed (new_blr.str_start);
#endif

GENERATE_blr (&new_blr, node);
l = new_blr.str_current - new_blr.str_start;

req_handle2 = (int *) NULL_PTR;
blr = (SCHAR*) new_blr.str_start;
isc_compile_request2 ((SLONG*) 0, &DB, &req_handle2, l, blr);
isc_start_request ((SLONG*) 0, &req_handle2, &gds__trans, 0);
isc_receive ((SLONG *) 0, &req_handle2, 0, sizeof (gen_value), &gen_value, 0);
isc_release_request ((SLONG*) 0, &req_handle2);

for (i = 5; (gen_value != new) && --i; )
    {
    *number = new - gen_value;
    new_blr.str_current = new_blr.str_start;
    new_blr.str_length = 1028;
    GENERATE_blr (&new_blr, node);
    l = new_blr.str_current - new_blr.str_start;

    req_handle2 = (int *) NULL_PTR;
    blr = (SCHAR*) new_blr.str_start;
    isc_compile_request2 ((SLONG*) 0, &DB, &req_handle2, l, blr);
    isc_start_request ((SLONG*) 0, &req_handle2, &gds__trans, 0);
    isc_receive ((SLONG*) 0, &req_handle2, 0, sizeof (gen_value), &gen_value, 0);
    isc_release_request ((SLONG*) 0, &req_handle2);
    }

gds__free (current_blr.str_start);
gds__free (new_blr.str_start);
}

static void store_acl (
    SCL		class,
    SLONG	*blob_id)
{
/**************************************
 *
 *	s t o r e _ a c l
 *
 **************************************
 *
 * Functional description
 *	Generate and store an access control list.
 *
 **************************************/
TEXT	buffer [4096], *p;
USHORT	length;
STATUS	status_vector [20];
void	*blob;

/* Generate access control list */

length = GENERATE_acl (class, buffer);

blob = create_blob (blob_id, 0, NULL_PTR);

if (gds__put_segment (status_vector,
	GDS_REF (blob),
	length,
	buffer))
    {
    DDL_db_error (status_vector, 92, NULL, NULL, NULL, NULL, NULL); /* msg 92: gds__put_segment failed */
    return;
    }

close_blob (blob);
} 

static void store_blr (
    NOD		node,
    SLONG	*blob_id,
    REL		relation)
{
/**************************************
 *
 *	s t o r e _ b l r
 *
 **************************************
 *
 * Functional description
 *	Generate blr for an expression and store result in
 *	a blob.
 *
 **************************************/
struct str blr;
USHORT	length, base;
STATUS	status_vector [20];
void	*handle;

blr.str_current = blr.str_start = (SCHAR*) gds__alloc ((SLONG) 4096);
blr.str_length = 4096;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
gds_alloc_flag_unfreed (blr.str_start);
#endif

GENERATE_blr (&blr, node);
#ifdef DEBUG
gds__print_blr (blr.str_start, 0, 0, 0);
#endif
handle = create_blob (blob_id, 0, NULL_PTR);
length = blr.str_current - blr.str_start;

if (gds__put_segment (status_vector,
	GDS_REF (handle),
	length,
	GDS_VAL (blr.str_start)))
    {
    DDL_db_error (status_vector, 93, NULL, NULL, NULL, NULL, NULL); /* msg 93: gds__put_segment failed */
    return;
    }

close_blob (handle);
} 

static void store_query_header (
    NOD		node,
    SLONG	*blob_id)
{
/**************************************
 *
 *	s t o r e _ q u e r y _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Store a query header.
 *
 **************************************/
NOD	*ptr, *end;
SYM	symbol;
STATUS	status_vector [20];
void	*blob;
USHORT	bpb_length;
UCHAR	bpb [20], *p;

#if (defined JPN_EUC || defined JPN_SJIS)
/* always create bpb since query header is a text blob */

p = bpb;
*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_interp;
*p++ = 2;
*p++ = DDL_interp;
*p++ = DDL_interp >> 8;
bpb_length = p - bpb;
#else
bpb_length = 0;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

blob = create_blob (blob_id, bpb_length, bpb);

for (ptr = node->nod_arg, end = ptr + node->nod_count; ptr < end; ptr++)
    {
    symbol = (SYM) *ptr;
    if (gds__put_segment (status_vector,
	    GDS_REF (blob),
	    symbol->sym_length,
	    GDS_VAL (symbol->sym_string)))
	{
	DDL_db_error (status_vector, 93, NULL, NULL, NULL, NULL, NULL); /* msg 93: gds__put_segment failed */
	return;
	}
    }

close_blob (blob);
}

static void store_range (
    FLD		field)
{
/**************************************
 *
 *	s t o r e _ r a n g e 
 *
 **************************************
 *
 * Functional description
 *	Store the range in a blob.  Range was
 *	already generated into a storable format
 *	during parse.
 *
 **************************************/
SLONG	*range;
USHORT	n;
BASED ON RDB$FIELD_DIMENSIONS.RDB$FIELD_NAME name;

MOVE_SYMBOL (field->fld_name, name);

FOR X IN RDB$FIELD_DIMENSIONS WITH X.RDB$FIELD_NAME EQ name
    ERASE X;
END_FOR;

for (range = field->fld_ranges, n = 0; n < field->fld_dimension; range += 2, ++n)
    STORE X IN RDB$FIELD_DIMENSIONS
	strcpy (X.RDB$FIELD_NAME, name);
	X.RDB$DIMENSION = n;
	X.RDB$LOWER_BOUND = range [0];
	X.RDB$UPPER_BOUND = range [1];
    END_STORE;
} 

static void store_text (
    TXT		text,
    SLONG	*blob_id)
{
/**************************************
 *
 *	s t o r e _ t e x t
 *
 **************************************
 *
 * Functional description
 *	store the text of a description or
 *	BLR expression in a blob.
 *
 **************************************/
STATUS	status_vector [20];
void	*blob;
USHORT	bpb_length;
UCHAR	bpb [20], *p;

#if (defined JPN_EUC || defined JPN_SJIS)
/* create bpb for text blob */

p = bpb;
*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_interp;
*p++ = 2;
*p++ = DDL_interp;
*p++ = DDL_interp >> 8;
bpb_length = p - bpb;
#else
bpb_length = 0;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

blob = create_blob (blob_id, bpb_length, bpb);
LEX_put_text (blob, text);
close_blob (blob);
}

static void store_userpriv (
    USERPRIV	upriv,
    TEXT	*grantor,
    TEXT	*priv,
    USRE	usr,
    UPFE	upf)
{
/**************************************
 *
 *	s t o r e _ u s e r p r i v
 *
 **************************************
 *
 * Functional description
 *	store one of possibly many entries for
 *	a single GRANT statement.
 *
 **************************************/

STORE X IN RDB$USER_PRIVILEGES USING
    X.RDB$FIELD_NAME.NULL = TRUE;

    MOVE_SYMBOL (usr->usre_name, X.RDB$USER);
    strcpy (X.RDB$GRANTOR, grantor);
    strcpy (X.RDB$PRIVILEGE, priv);
    X.RDB$GRANT_OPTION = (upriv->userpriv_flags & USERPRIV_grant) ? TRUE : FALSE;
    MOVE_SYMBOL (upriv->userpriv_relation, X.RDB$RELATION_NAME);
    X.RDB$USER_TYPE = obj_user;
    X.RDB$OBJECT_TYPE = obj_relation;
    if (upf)
	{
	MOVE_SYMBOL (upf->upfe_fldname, X.RDB$FIELD_NAME);
	X.RDB$FIELD_NAME.NULL = FALSE;
	}
END_STORE;
 
}

static int string_length (
    SCHAR	dtype)
{
/**************************************
 *
 *	s t r i n g _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Take a guess at how long a number
 *	or date will be as a string
 *
 **************************************/

switch (dtype)
    {
    case dtype_short	: return 6;
    case dtype_long	: return 10;
    case dtype_quad	: return 19;
    case dtype_real	: return 10;
    case dtype_double	: return 19;
    case dtype_timestamp: return 10;	/* really 24, kept at 10 for old apps */
    case dtype_sql_date	: 
    case dtype_sql_time :
    case dtype_int64    :	
    case dtype_blob	: 
    default:
	DDL_msg_put (94, NULL, NULL, NULL, NULL, NULL); /* msg 94: (EXE) string_length: No defined length for blobs */
        return 0;
    }
}

static void wal_info (
    UCHAR	*db_info_buffer,
    SLONG	*log,
    SCHAR	*cur_log,
    SLONG	*part_offset)
{
/**************************************
 *
 *	w a l _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Extract wal name and offset from wal_info string.
 *
 **************************************/
UCHAR	item;
UCHAR	*d, *p;
SLONG	length;

p = db_info_buffer;

while ((item = *p++) != gds__info_end)
    {
    length = gds__vax_integer (p, 2);
    p += 2;
    switch (item)
	{
	case gds__info_logfile:
	    *log = gds__vax_integer (p, length);
	    p += length;
	    break;

	case gds__info_cur_logfile_name:
	    d = p;
	    p += length;
	    length = *d++;
	    memcpy (cur_log, d, length);
	    cur_log [length]  = 0;
	    break;
	    
	case gds__info_cur_log_part_offset:
	    *part_offset = gds__vax_integer (p, length);
	    p += length;
	    break;

	default:
	    ;
	}
    }
}
