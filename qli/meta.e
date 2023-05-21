/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		meta.e
 *	DESCRIPTION:	Meta-data interface
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
#include "../qli/dtr.h"
#include "../qli/compile.h"
#include "../qli/exe.h"
#include "../jrd/license.h"
#include "../jrd/flags.h"
#include "../jrd/gds.h"
#include "../qli/reqs.h"
#if (defined JPN_SJIS || defined JPN_EUC)
#include "../jrd/kanji.h"
#endif
#include "../qli/all_proto.h"
#include "../qli/err_proto.h"
#include "../qli/form_proto.h"
#include "../qli/gener_proto.h"
#include "../qli/hsh_proto.h"
#include "../qli/meta_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/utl_proto.h"
#include "../wal/walf_proto.h"

DATABASE
    DB = FILENAME "yachts.lnk";
DATABASE
    DB1 = FILENAME "yachts.lnk";

extern USHORT	sw_buffers;

static void	add_field (REL, FLD, USHORT);
static void	add_sql_field (REL, FLD, USHORT, RLB);
static int	blob_copy (RLB, REL, SLONG *);
static void	change_field (REL, FLD);
static int	check_global_field (DBB, FLD, TEXT *);
static int	check_relation (REL);
static int	clone_fields (REL, REL);
static int	clone_global_fields (REL, REL);
static void	define_global_field (DBB, FLD, SYM);
static void	delete_fields (REL);
static STATUS	detach (STATUS *, DBB);
static int	execute_dynamic_ddl (DBB, RLB);
static int	field_length (USHORT, USHORT);
static void	get_database_type (DBB);
static void	get_log_names (DBB, SCHAR *, LLS *, SCHAR *, SLONG, SSHORT, SSHORT);
static void	get_log_names_serial (LLS *);
static TEXT	*get_query_header (DBB, SLONG [2]);
static void	install (DBB);
static SYN	make_node (NOD_T, USHORT);
static TEXT	*make_string (TEXT *, SSHORT);
static SYM	make_symbol (TEXT *, SSHORT);
static CON	missing_value (SLONG *, SYM);
static SYN	parse_blr (UCHAR **, SYM);
static SYN	parse_blr_blob (SLONG *, SYM);
static void	purge_relation (REL);
static void	put_dyn_string (RLB, TEXT *);
static void	rollback_update (DBB);
static void	set_capabilities (DBB);
static DBB	setup_update (DBB);
static void	sql_grant_revoke (SYN, USHORT);
static void	stuff_priv (RLB, USHORT, TEXT *, USHORT, TEXT *, TEXT *);
static int	truncate_string (TEXT *);
static void	wal_info (UCHAR *, SLONG *, SCHAR *, SLONG *);

static CONST UCHAR	tpb [] = 	{ gds__tpb_version1, gds__tpb_write };
static CONST UCHAR	db_info [] =	{ gds__info_implementation, gds__info_end };
static UCHAR	parm_buffer [256];

static CONST UCHAR     db_log_info [] = { gds__info_logfile, gds__info_cur_logfile_name, gds__info_cur_log_part_offset };

/*
static CONST UCHAR	dpb_trace [] =	{ gds__dpb_version1, gds__dpb_trace, 1, 1 };
static CONST UCHAR	dpb_num_buffers [ ] = { gds__dpb_version1, gds__dpb_num_buffers, 1, 1 };
*/

#ifdef mpexl
#define unlink		PIO_unlink
#endif

#define STUFF_STRING(str)	put_dyn_string (rlb, str)
#define BLR_BYTE	*blr++

static CONST USHORT	blr_dtypes [] = {
	0,
	blr_text,
	blr_text,
	blr_varying,
	0,
	0,
	0,
	0,
	blr_short,
	blr_long,
	blr_quad,
	blr_float,
	blr_double,
	blr_double,
	blr_sql_date,
	blr_sql_time,
	blr_timestamp,
	blr_blob
	};
		
/* 
   table used to determine capabilities, checking for specific 
   fields in system relations 
*/

typedef struct rfr_tab_t
    {
    CONST TEXT	*relation;
    CONST TEXT	*field; 
    int		bit_mask;
    } *RFR_TAB;
 
static CONST struct rfr_tab_t rfr_table [] =
	{
        "RDB$INDICES",		"RDB$INDEX_INACTIVE", 	DBB_cap_idx_inactive,
/* OBSOLETE - 1996-Aug-06 David Schnepper 
	"RDB$RELATIONS",	"RDB$STORE_TRIGGER",	DBB_cap_triggers,
*/
	"RDB$RELATIONS",	"RDB$EXTERNAL_FILE",	DBB_cap_extern_file,
	"RDB$SECURITY_CLASSES", "RDB$SECURITY_CLASS",   DBB_cap_security,
	"RDB$FILES",		"RDB$FILE_NAME",	DBB_cap_files,
	"RDB$FUNCTIONS",	"RDB$FUNCTION_NAME",	DBB_cap_functions,
	"RDB$TRIGGERS",		"RDB$TRIGGER_BLR",	DBB_cap_new_triggers,
	"RDB$CONSTRAINTS", 	"RDB$CONSTRAINT_NAME",  DBB_cap_single_trans,
	"RDB$FILES",	 	"RDB$SHADOW_NUMBER",	DBB_cap_shadowing,
	"RDB$TYPES",		"RDB$TYPE_NAME",	DBB_cap_types,
	"RDB$FIELDS",		"RDB$DIMENSIONS",	DBB_cap_dimensions,
	"RDB$FIELDS",		"RDB$EXTERNAL_TYPE",	DBB_cap_external_type,
	"RDB$RELATION_FIELDS",	"RDB$SYSTEM_FLAG",	DBB_cap_rfr_sys_flag,
	"RDB$FILTERS",		"RDB$FUNCTION_NAME",	DBB_cap_filters,
	"RDB$INDICES",		"RDB$INDEX_TYPE",	DBB_cap_index_type,
	0, 0, 0
	};

int MET_declare (
    DBB		database,
    FLD		variable,
    NAM		field_name)
{
/**************************************
 *
 *	M E T _ d e c l a r e
 *
 **************************************
 *
 * Functional description
 *	Find a global field referenced in a DECLARE.
 *
 **************************************/

if (!database)
    database = QLI_databases;

for (; database; database = database->dbb_next)
    {
    MET_meta_transaction (database, FALSE);
    if (check_global_field (database, variable, field_name->nam_string))
	return TRUE;
    }        

return FALSE;
}

void MET_define_field (
    DBB		database,
    FLD		field)
{
/**************************************
 *
 *	M E T _ d e f i n e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Define a new global field.
 *
 **************************************/
SYM	symbol;

database = setup_update (database); 

if (check_global_field (database, NULL_PTR, field->fld_name->sym_string))
    {
    rollback_update (database);
    ERRQ_print_error (233, field->fld_name->sym_string, NULL, NULL, NULL, NULL); /* Msg233 global field %s already exists */
    }

define_global_field (database, field, field->fld_name);

MET_meta_commit (database);
}

void MET_define_index (
    SYN		node)
{
/**************************************
 *
 *	M E T _ d e f i n e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Define a new index.
 *
 **************************************/
DBB	database;
REL	relation;
SYN	fields;
FLD	field;
NAM	*ptr;
SYM	symbol;
int	position, present;
STATUS	status_vector [20];

symbol = (SYM) node->syn_arg [s_dfi_name];
relation = (REL) node->syn_arg [s_dfi_relation];
fields = node->syn_arg [s_dfi_fields];
database = setup_update (relation->rel_database); 

if (relation->rel_flags & REL_view)
    IBERROR ( 234 ); /* Msg234 Can't define an index in a view */

FOR (REQUEST_HANDLE database->dbb_requests [REQ_def_index1])
	X IN DB.RDB$INDICES WITH X.RDB$INDEX_NAME EQ symbol->sym_string
    ERRQ_print_error (235, symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg235 Index %s already exists */
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

STORE (REQUEST_HANDLE database->dbb_requests [REQ_def_index2])
	X IN DB.RDB$INDICES
    strcpy (X.RDB$RELATION_NAME, relation->rel_symbol->sym_string);
    strcpy (X.RDB$INDEX_NAME, symbol->sym_string);
    X.RDB$UNIQUE_FLAG = (node->syn_flags & s_dfi_flag_unique);
    X.RDB$INDEX_INACTIVE = (node->syn_flags & s_dfi_flag_inactive);
    X.RDB$SEGMENT_COUNT = fields->syn_count;
    X.RDB$INDEX_TYPE = (node->syn_flags & s_dfi_flag_descending) ? TRUE : FALSE;
END_STORE
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;


/* at this point force garbage collection of any index segments left over
   from previous failures.  The system transaction (which might notice them
   later does NOT recognize rolled back stuff!  Should someone have actually
   left an orphanned segment around, kill that too.
*/

FOR (REQUEST_HANDLE database->dbb_requests [REQ_scan_index])
	X IN DB.RDB$INDEX_SEGMENTS WITH X.RDB$INDEX_NAME = symbol->sym_string
    ERASE X
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;


for (ptr = (NAM*) fields->syn_arg, position = 0; 
     position < fields->syn_count; ptr++, position++)
    {
    MET_fields (relation);

   for (present = FALSE, field = relation->rel_fields; field; field = field->fld_next)
	if (!(strcmp ((*ptr)->nam_string, field->fld_name->sym_string)))
	    {
	    present = TRUE;
	    break;
	    }

    if (!present)
	{
	rollback_update (database);
	ERRQ_print_error (236, (*ptr)->nam_string, relation->rel_symbol->sym_string, NULL, NULL, NULL); /* Msg236 Field %s does not occur in relation %s */
	}

    setup_update (database);
  	    	     
    STORE (REQUEST_HANDLE database->dbb_requests [REQ_def_index3])
	     X IN DB.RDB$INDEX_SEGMENTS
	strcpy (X.RDB$INDEX_NAME, symbol->sym_string);
	strcpy (X.RDB$FIELD_NAME, (*ptr)->nam_string);
	X.RDB$FIELD_POSITION = position;
    END_STORE
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
    }

MET_meta_commit (database);
}

void MET_define_relation (
    REL		relation,
    REL		source)
{
/**************************************
 *
 *	M E T _ d e f i n e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Define a new relation.   There may
 *	be field definitions here or we may
 *	copy the field definitions from somebody
 *	else.
 *
 **************************************/
DBB	database;
SYM	symbol;
FLD	field;
USHORT	position;
STATUS	status_vector [20];

symbol = relation->rel_symbol;

if (source)
    {
    source->rel_database = setup_update (source->rel_database);
    if (!(check_relation (source)))
        ERRQ_print_error (483, source->rel_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg 483 Relation %s does not exist */  
    }


relation->rel_database = database = setup_update (relation->rel_database); 

if (check_relation (relation))
    ERRQ_print_error (237, symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg237 Relation %s already exists */


STORE (REQUEST_HANDLE database->dbb_requests [REQ_store_relation])
	X IN DB.RDB$RELATIONS
    strcpy (X.RDB$RELATION_NAME, symbol->sym_string);
END_STORE
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (source) 
    clone_fields (relation, source);
else 
    for (field = relation->rel_fields, position = 1; field; field = field->fld_next, position++)
	add_field (relation, field, position); 

MET_meta_commit (database); 

relation = (REL) ALLOCP (type_rel);
relation->rel_next = database->dbb_relations;
database->dbb_relations = relation;
relation->rel_database = database;

/* Go back and pick up relation id */

setup_update (database);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_relation_id]) 
	X IN DB.RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    relation->rel_id = X.RDB$RELATION_ID;
    symbol = make_symbol (X.RDB$RELATION_NAME, sizeof (X.RDB$RELATION_NAME));
    symbol->sym_type = SYM_relation;
    symbol->sym_object = (BLK) relation;
    relation->rel_symbol = symbol;
    HSH_insert (symbol);
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

delete_fields (relation);
MET_fields (relation);
}

void MET_define_sql_relation (
    REL		relation)
{
/**************************************
 *
 *	M E T _ d e f i n e _ s q l _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Define a new SQL relation, using dynamic ddl.
 *
 **************************************/
DBB	database;
SYM	symbol;
FLD	field;
USHORT	position;
STATUS	status_vector [20];
RLB	rlb;

symbol = relation->rel_symbol;
relation->rel_database = database = setup_update (relation->rel_database); 

FOR (REQUEST_HANDLE database->dbb_requests [REQ_relation_def])
	X IN DB.RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    ERRQ_print_error (237, symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg237 Relation %s already exists */
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

rlb = NULL;
rlb = CHECK_RLB (rlb);

STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);
STUFF (gds__dyn_def_rel);
STUFF_STRING (symbol->sym_string);
STUFF (gds__dyn_rel_sql_protection);
STUFF_WORD (2);
STUFF_WORD (1);

for (field = relation->rel_fields, position = 1; field; field = field->fld_next, position++)
    add_sql_field (relation, field, position, rlb);

STUFF (gds__dyn_end);
STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

execute_dynamic_ddl (database, rlb);
MET_meta_commit (database);

relation = (REL) ALLOCP (type_rel);
relation->rel_next = database->dbb_relations;
database->dbb_relations = relation;
relation->rel_database = database;

/* Go back and pick up relation id */

setup_update (database);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_relation_id]) 
	X IN DB.RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    relation->rel_id = X.RDB$RELATION_ID;
    symbol = make_symbol (X.RDB$RELATION_NAME, sizeof (X.RDB$RELATION_NAME));
    symbol->sym_type = SYM_relation;
    symbol->sym_object = (BLK) relation;
    relation->rel_symbol = symbol;
    HSH_insert (symbol);
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

delete_fields (relation);
MET_fields (relation);
}

void MET_delete_database (
    DBB		dbb)
{
/**************************************
 *
 *	M E T _ d e l e t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Drop an existing database, and all its files,
 *	and finish any copies we have active.
 *
 **************************************/
STATUS	status_vector [20];
LLS	log_stack, stack;
STR	string;
TEXT	s [126];
DBB	database, next;
UCHAR	*dpb = NULL, *p;
TEXT	*user, *password;
USHORT	dpb_length = 0, user_length, password_length;
SCHAR    db_info_buffer [512], cur_log [512], db_name [512];
SLONG	part_offset, log;

/* generate a database parameter block to 
   include the user/password, if necessary */

if (dbb->dbb_user)
    {
    user = (TEXT*) dbb->dbb_user->con_data;
    user_length = dbb->dbb_user->con_desc.dsc_length;
    }
else 
    {
    user = QLI_default_user;
    user_length = strlen (QLI_default_user);
    }

if (dbb->dbb_password)
    {
    password = (TEXT*) dbb->dbb_password->con_data;
    password_length = dbb->dbb_user->con_desc.dsc_length;
    }
else 
    {
    password = QLI_default_password;
    password_length = strlen (QLI_default_password);
    }

dpb = p = parm_buffer;
*p++ = gds__dpb_version1;

if (user_length)
    {
    *p++ = gds__dpb_user_name;
    *p++ = user_length;
    while (user_length--)
	*p++ = *user++;
    }

if (password_length)
    {
    *p++ = gds__dpb_password;
    *p++ = password_length;
    while (password_length--)
	*p++ = *password++;
    }

dpb_length = p - parm_buffer;

if (dpb_length == 1)
    dpb_length = 0;

if (gds__attach_database (status_vector, 
	0, 
	GDS_VAL (dbb->dbb_filename), 
	GDS_REF (dbb->dbb_handle), 
	dpb_length, 
	GDS_VAL(dpb)))
    ERRQ_database_error (dbb, status_vector);

log_stack = stack = NULL;

gds__trans = MET_transaction (nod_start_trans, dbb);
DB = dbb->dbb_handle;

FOR F IN DB.RDB$FILES SORTED BY F.RDB$FILE_START
    string = (STR) ALLOCDV (type_str, strlen (F.RDB$FILE_NAME));
    LLS_PUSH (string, &stack);
    strcpy (string->str_data, F.RDB$FILE_NAME);
END_FOR
    ON_ERROR
	ERRQ_database_error (dbb, gds__status);
    END_ERROR;

/* Get write ahead log information */

if (gds__database_info (status_vector,
    GDS_REF (DB),
    sizeof (db_log_info),
    db_log_info,
    sizeof (db_info_buffer),
    db_info_buffer)
    ) ERRQ_database_error (dbb, status_vector);

/* extract info from buffer */

wal_info (db_info_buffer, &log, cur_log, &part_offset);

if (log)
    get_log_names_serial (&log_stack);

MET_transaction (nod_commit, dbb);

if (gds__detach_database (gds__status, GDS_REF (dbb->dbb_handle)))
    gds__print_status (gds__status);

for (database = QLI_databases; database; database = next) 
    {
    next = database->dbb_next;
    if (!strcmp (database->dbb_filename, dbb->dbb_filename))
	MET_finish (database);
    }

if (log)
    {
    ISC_expand_filename (dbb->dbb_filename, 0, db_name);
    get_log_names (dbb, db_name, &log_stack, cur_log, part_offset, 0, 1);
    }

while (stack)
    {
    string = (STR) LLS_POP (&stack);
    if (unlink (string->str_data))
	ERRQ_print_error (431, string->str_data, NULL, NULL, NULL, NULL); /* Msg431 Could not drop database file "%s" */
    }

while (log_stack)
    {
    string = (STR) LLS_POP (&log_stack);
    unlink (string->str_data);	/* do not check for error */
    }

if (unlink (dbb->dbb_filename))
    ERRQ_print_error (431, dbb->dbb_filename, NULL, NULL, NULL, NULL); /* Msg431 Could not drop database file "%s" */
}

void MET_delete_field (
    DBB		database,
    NAM		name)
{
/**************************************
 *
 *	M E T _ d e l e t e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Delete a global field.
 *
 **************************************/
USHORT	count;
RLB	rlb;

database = setup_update (database);
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_check_fld])
    RFR IN DB.RDB$RELATION_FIELDS WITH RFR.RDB$FIELD_SOURCE EQ name->nam_string
	REDUCED TO RFR.RDB$RELATION_NAME
    if (!count)
	ERRQ_msg_put (238, name->nam_string, NULL, NULL, NULL, NULL); /* Msg238 Field %s is in use in the following relations: */
    ib_printf ("\t%s\n", RFR.RDB$RELATION_NAME);
    count++;
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (count)
     ERRQ_print_error (239, name->nam_string, database->dbb_filename, NULL, NULL, NULL); /* Msg239 Field %s is in use in database %s */

FOR (REQUEST_HANDLE database->dbb_requests [REQ_erase_fld])
    FLD IN DB.RDB$FIELDS WITH FLD.RDB$FIELD_NAME EQ name->nam_string
    rlb = NULL;
    rlb = CHECK_RLB (rlb);
    STUFF (gds__dyn_version_1);
    STUFF (gds__dyn_delete_dimensions);
    STUFF_STRING (FLD.RDB$FIELD_NAME);
    STUFF (gds__dyn_end);
    STUFF (gds__dyn_eoc);
    execute_dynamic_ddl (database, rlb);    
    ERASE FLD
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
    count++;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (!count)
     ERRQ_print_error (240, name->nam_string, database->dbb_filename, NULL, NULL, NULL); /* Msg240 Field %s is not defined in database %s */

MET_meta_commit (database);
}

void MET_delete_index (
    DBB		database,
    NAM		name)
{
/**************************************
 *
 *	M E T _ d e l e t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Delete an index.
 *
 **************************************/
USHORT	count;

database = setup_update (database);
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_erase_index])
    IDX IN DB.RDB$INDICES WITH IDX.RDB$INDEX_NAME EQ name->nam_string
    count++;
    ERASE IDX
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (!count)
     ERRQ_print_error (241, name->nam_string, database->dbb_filename, NULL, NULL, NULL); /* Msg241 Index %s is not defined in database %s */

FOR (REQUEST_HANDLE database->dbb_requests [REQ_erase_segments])
    SEG IN DB.RDB$INDEX_SEGMENTS WITH SEG.RDB$INDEX_NAME EQ name->nam_string
    ERASE SEG
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

MET_meta_commit (database);
}

void MET_delete_relation (
    REL		relation)
{
/**************************************
 *
 *	M E T _ d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Delete a relation.
 *
 **************************************/
SYM	symbol;
RLB	rlb;

/* Pass the mess off to dynamic DDL and let it worry */

symbol = relation->rel_symbol;
rlb = NULL;
rlb = CHECK_RLB (rlb);
STUFF (gds__dyn_version_1);
STUFF (gds__dyn_delete_rel);
STUFF_STRING (symbol->sym_string);
STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);
setup_update (relation->rel_database); 

execute_dynamic_ddl (relation->rel_database, rlb);
MET_meta_commit (relation->rel_database);

/* Unlink and release various blocks */

purge_relation (relation);
}

int MET_dimensions (
    DBB		database,
    TEXT	*field_name)
{
/**************************************
 *
 *	M E T _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Determine if the field has any dimensions.
 *
 **************************************/
USHORT	dimensions;

if (!(database->dbb_capabilities & DBB_cap_dimensions))
    return 0;

dimensions = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_fld_dimensions])
	F IN DB.RDB$FIELDS WITH F.RDB$FIELD_NAME = field_name

    dimensions = F.RDB$DIMENSIONS;
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

return dimensions;
}

void MET_fields (
    REL		relation)
{
/**************************************
 *
 *	M E T _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Get all fields for a relation.
 *
 **************************************/
DBB	database;
FLD	field, *ptr;
SYM	symbol;
SLONG	*blob;

/* If we already have fetched the fields for the relation, 
   don't do it again. */

if (relation->rel_flags & REL_fields)      
    return; 

database = relation->rel_database;
MET_meta_transaction (database, FALSE);
relation->rel_flags |= REL_fields;
ptr = &relation->rel_fields;

FOR (REQUEST_HANDLE database->dbb_field_request)
	RFR IN DB.RDB$RELATION_FIELDS CROSS
	RFL IN DB.RDB$FIELDS WITH 
	RFR.RDB$FIELD_SOURCE EQ RFL.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
	SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME;
    if (RFR.RDB$FIELD_POSITION > relation->rel_max_field_pos)
	relation->rel_max_field_pos = RFR.RDB$FIELD_POSITION;
    *ptr = field = (FLD) ALLOCPV (type_fld, 0);
    ptr = &field->fld_next;
    field->fld_relation = relation;
    if (symbol = make_symbol (RFR.RDB$FIELD_NAME, sizeof (RFR.RDB$FIELD_NAME)))
	{
	symbol->sym_object = (BLK) field;
	symbol->sym_type = SYM_field;
	field->fld_name = symbol;
	}
    if ((symbol = make_symbol (RFR.RDB$QUERY_NAME, sizeof (RFR.RDB$QUERY_NAME))) ||
        (symbol = make_symbol (RFL.RDB$QUERY_NAME, sizeof (RFL.RDB$QUERY_NAME))))
	{
	symbol->sym_object = (BLK) field;
	symbol->sym_type = SYM_field;
	field->fld_query_name = symbol;
	}
    field->fld_scale = RFL.RDB$FIELD_SCALE;
    field->fld_id = RFR.RDB$FIELD_ID;

    if (RFL.RDB$SEGMENT_LENGTH.NULL)
	field->fld_segment_length = 80;
    else
	field->fld_segment_length = 
		((RFL.RDB$SEGMENT_LENGTH) < 256 && (RFL.RDB$SEGMENT_LENGTH > 0)) ? 
		RFL.RDB$SEGMENT_LENGTH  : 255 ;

    blob = (SLONG*) &RFR.RDB$QUERY_HEADER;
    if (!blob [0] && !blob [1])
	blob = (SLONG*) &RFL.RDB$QUERY_HEADER;
    if (blob [0] || blob [1])      
	field->fld_query_header = get_query_header (database, blob);

    blob = (SLONG*) &RFL.RDB$COMPUTED_BLR;
    if (blob [0] || blob [1])
	field->fld_flags |= FLD_computed;

    field->fld_dtype = MET_get_datatype (RFL.RDB$FIELD_TYPE);
    field->fld_length = field_length (field->fld_dtype, RFL.RDB$FIELD_LENGTH);
    if (field->fld_dtype == dtype_varying)
	field->fld_length += sizeof (SSHORT);
    field->fld_sub_type = RFL.RDB$FIELD_SUB_TYPE;
    field->fld_sub_type_missing = RFL.RDB$FIELD_SUB_TYPE.NULL;

    if (!RFL.RDB$MISSING_VALUE.NULL)
	field->fld_missing = 
		missing_value (&RFL.RDB$MISSING_VALUE, field->fld_name);

    if (!(field->fld_edit_string = 
		make_string (RFR.RDB$EDIT_STRING, sizeof (RFR.RDB$EDIT_STRING))))
	field->fld_edit_string = 
		make_string (RFL.RDB$EDIT_STRING, sizeof (RFL.RDB$EDIT_STRING));
    field->fld_validation = 
	parse_blr_blob (&RFL.RDB$VALIDATION_BLR, field->fld_name);
    if (MET_dimensions (database, RFL.RDB$FIELD_NAME) > 0)
	field->fld_flags |= FLD_array;
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;	
}

void MET_finish (
    DBB		dbb)
{
/**************************************
 *
 *	M E T _ f i n i s h
 *
 **************************************
 *
 * Functional description
 *	Finish a database and release all associated blocks.
 *
 **************************************/
STATUS	status_vector [20];
DBB	*ptr;
REL	relation;
FUN	function;
SCHAR	count;

/* Get rid of relation and field blocks. */

while (relation = dbb->dbb_relations)
    purge_relation (relation);

/* Get rid of any functions */

while (function = dbb->dbb_functions)
    {
    dbb->dbb_functions = function->fun_next;
    HSH_remove (function->fun_symbol);
    ALL_release (function->fun_symbol);
    ALL_release (function);
    }

/* Get rid of any forms */

if (dbb->dbb_forms)
    FORM_finish (dbb);

/* Finally, actually close down database connection. */

detach (status_vector, dbb);

if (dbb->dbb_symbol)
    {
    HSH_remove (dbb->dbb_symbol);
    ALL_release (dbb->dbb_symbol);
    }

for (count = 0, ptr = &QLI_databases; *ptr; ptr = &(*ptr)->dbb_next)
    if (*ptr == dbb)
	{
	*ptr = dbb->dbb_next;
	ALL_release (dbb);
	count++;
	if (!*ptr)
	    break;
	}

if (!count)
    BUGCHECK ( 231 ); /* Msg231 database block not found for removal */

if (status_vector[1])
    ERRQ_database_error (NULL_PTR, status_vector);
}

int MET_get_datatype (
    USHORT	blr_datatype)
{ 
/**************************************
 *
 *	M E T _ g e t _ d a t a t y p e
 *
 **************************************
 *
 * Functional description
 *	Convert a blr datatype to a QLI dtype.
 *
 **************************************/
USHORT	retvalue;

if (blr_datatype == blr_text)
	retvalue = dtype_text;
    else if (blr_datatype == blr_varying)
	retvalue = dtype_varying;
    else if (blr_datatype == blr_cstring)
	retvalue = dtype_cstring;
    else if (blr_datatype == blr_short)
	retvalue = dtype_short;
    else if (blr_datatype == blr_long)
	retvalue = dtype_long;
    else if (blr_datatype == blr_quad)
	retvalue = dtype_quad;
    else if (blr_datatype == blr_float)
	retvalue = dtype_real;
    else if (blr_datatype == blr_double)
	retvalue = dtype_double;
    else if (blr_datatype == blr_timestamp)
	retvalue = dtype_timestamp;
    else if (blr_datatype == blr_sql_date)
	retvalue = dtype_sql_date;
    else if (blr_datatype == blr_sql_time)
	retvalue = dtype_sql_time;
    else if (blr_datatype == blr_blob)
	retvalue = dtype_blob;
    else retvalue = dtype_null;

return retvalue;
}


#ifdef DEV_BUILD
void MET_index_info (
    SCHAR	*relation_name,
    SCHAR	*index_name,
    SCHAR	*buffer)
{ 
/**************************************
 *
 *	M E T _ i n d e x _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Get info about a particular index.
 *
 **************************************/
SLONG	*request_handle = NULL; 
SCHAR	*b, *p;
                  
b = buffer;      
FOR (REQUEST_HANDLE request_handle)
    IDX IN DB.RDB$INDICES CROSS
    SEG IN DB.RDB$INDEX_SEGMENTS OVER
    RDB$INDEX_NAME WITH
    IDX.RDB$INDEX_NAME EQ index_name AND
    IDX.RDB$RELATION_NAME EQ relation_name
    SORTED BY SEG.RDB$FIELD_POSITION

    if (b == buffer)
	{
        for (p = IDX.RDB$INDEX_NAME; *p && *p != ' ';)
	    *b++ = *p++;
	*b++ = ' ';
	*b++ = '(';
	}

    for (p = SEG.RDB$FIELD_NAME; *p && *p != ' ';)
        *b++ = *p++;

    *b++ = ' ';

END_FOR
    ON_ERROR
	ERRQ_database_error (NULL, gds__status);
    END_ERROR;

if (request_handle)
    if (gds__release_request (gds__status, GDS_REF (request_handle))) 
	ERRQ_database_error (NULL, gds__status);
    
/* back up over the last space and finish off */

b--;
*b++ = ')';
*b++ = 0;
}
#endif

void MET_meta_commit (
    DBB		database)
{
/**************************************
 *
 *	M E T _ m e t a _ c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit the meta data lookup & update
 *	transaction.
 *
 **************************************/
STATUS	status_vector [20];

if (database->dbb_capabilities & DBB_cap_multi_trans)
     if (gds__commit_transaction (status_vector, 
		GDS_REF (database->dbb_meta_trans)))
	{
	rollback_update (database);
	ERRQ_database_error (database, status_vector);
	}
}

void MET_meta_rollback (
    DBB		database)
{                                         
/**************************************
 *
 *	M E T _ m e t a _ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Rollback the metadata transaction,
 *	if there is one.
 *
 **************************************/
STATUS	status_vector [20];

if (database->dbb_capabilities & DBB_cap_multi_trans)
    rollback_update (database);
}

SLONG *MET_meta_transaction (
    DBB		database,
    int		update_flag)
{
/**************************************
 *
 *	M E T _ m e t a _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Setup transaction for meta-data operation.  Set up
 *	DB and gds__trans, and return meta-data transaction
 *	handle for yucks.  If we're doing metadata updates,
 *	and lookups we'll use the metadat date transaction, 
 *	unless this is an rdb database or gateway in which  
 *	case, we'll use the only transacti.
 *	
 *	In any event, we will set up the met_transaction because
 *	it's widely used and somebody is going to forget to test
 *	that the database is multi_transaction before using it.
 *	This means that we have to be very careful about committing
 *	or rolling back, because this could affect user data.
 *
 **************************************/
STATUS	status_vector [20];
SLONG	*transaction;

if (!database)
    IBERROR ( 243 ); /* Msg243 no active database for operation */

transaction = (database->dbb_capabilities & DBB_cap_multi_trans) ? 
	 database->dbb_meta_trans : NULL;

DB = database->dbb_handle;

/* If we don't know whether or not the database can handle
   multiple transactions, find out now */

if (!transaction &&
    ((database->dbb_capabilities & DBB_cap_multi_trans) ||
     !(database->dbb_capabilities & DBB_cap_single_trans)))
    if (gds__start_transaction (status_vector, 
	    GDS_REF (transaction), 
	    1, 
	    GDS_REF (database->dbb_handle),
	    sizeof (tpb), 
	    tpb))
        database->dbb_capabilities |= DBB_cap_single_trans;
    else
        database->dbb_capabilities |= DBB_cap_multi_trans;

/* If we already have a meta-data transaction, there's more nothing to do */

gds__trans = transaction;

/* If we only support a single transaction, use the data transaction */

if (!gds__trans && (database->dbb_capabilities & DBB_cap_single_trans))
    {
    if (update_flag)
	IBERROR ( 244 ); /* Msg244 Interactive metadata updates are not available on Rdb */
    if (!(gds__trans = database->dbb_transaction))
	gds__trans = MET_transaction (nod_start_trans, database);
    }

/* otherwise make one more effort to start the transaction */

else if (!gds__trans)
    {
    START_TRANSACTION
    ON_ERROR
	ERRQ_database_error (database, status_vector);
    END_ERROR;
    }

database->dbb_meta_trans = gds__trans;
return gds__trans;
}

void MET_modify_field (
    DBB		database,
    FLD		field)
{
/**************************************
 *
 *	M E T _ m o d i f y _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Modify an existing global field.
 *
 **************************************/
SYM	symbol, field_name;
REL	relation;
TEXT	*p;
SSHORT	flag;

database = setup_update (database);
field_name = field->fld_name;
flag = 0;	

FOR (REQUEST_HANDLE database->dbb_requests [REQ_modify_fld])
	X IN DB.RDB$FIELDS WITH X.RDB$FIELD_NAME EQ field_name->sym_string
    if (field->fld_dtype &&
	(X.RDB$FIELD_TYPE == blr_blob || blr_dtypes [field->fld_dtype] == blr_blob) &&
	X.RDB$FIELD_TYPE != blr_dtypes [field->fld_dtype])
	flag = -1;
    else
	{
	flag = 1;
	MODIFY X USING
	    if (field->fld_dtype)
		{
		X.RDB$FIELD_TYPE = blr_dtypes [field->fld_dtype];
		X.RDB$FIELD_SCALE = field->fld_scale;
		X.RDB$FIELD_LENGTH = (field->fld_dtype == dtype_varying) ?
		    field->fld_length - sizeof (SSHORT) : field->fld_length;
		}
	    if (!field->fld_sub_type_missing)
	        {
	        X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	        X.RDB$FIELD_SUB_TYPE = field->fld_sub_type;
	        }
	    if (symbol = field->fld_query_name)
		{
		X.RDB$QUERY_NAME.NULL = FALSE;
		strcpy (X.RDB$QUERY_NAME, symbol->sym_string);
		}
	    if (field->fld_edit_string)
		{
		X.RDB$EDIT_STRING.NULL = FALSE;
		strcpy (X.RDB$EDIT_STRING, field->fld_edit_string);
		}
	END_MODIFY
	    ON_ERROR
	        rollback_update (database);
		ERRQ_database_error (database, gds__status);
	    END_ERROR;
	}
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (flag <= 0)
    {
    rollback_update (database);
    if (flag)
	ERRQ_print_error (468, field_name->sym_string, NULL, NULL, NULL, NULL); /* Msg468 Datatype of field %s may not be changed to or from blob */
    else
	ERRQ_print_error (245, field_name->sym_string, NULL, NULL, NULL, NULL); /* Msg245 global field %s is not defined */
    }

MET_meta_commit (database);

/* Now go back and re-fetch fields for affected databases */

setup_update (database);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_update_fld])
	X IN DB.RDB$RELATION_FIELDS WITH X.RDB$FIELD_SOURCE EQ field_name->sym_string
	REDUCED TO X.RDB$RELATION_NAME
    for (p = X.RDB$RELATION_NAME; *p && *p != ' '; p++)
	;
    symbol = HSH_lookup (X.RDB$RELATION_NAME, p - X.RDB$RELATION_NAME);
    for (; symbol; symbol = symbol->sym_homonym)
	if (symbol->sym_type == SYM_relation &&
	    (relation = (REL) symbol->sym_object) &&
	    relation->rel_database == database)
	    {
	    delete_fields (relation);
	    MET_fields (relation);
	    }
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;
}

void MET_modify_index (
    SYN		node)
{
/**************************************
 *
 *	M E T _ m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Change the changeable options
 *	of an index.
 *
 **************************************/
DBB	database;
REL	relation;
SYN	fields;
NAM	*ptr, name;
USHORT	count;
STATUS	status_vector [20];

name = (NAM) node->syn_arg [s_mfi_name];
database = (DBB) node->syn_arg [s_mfi_database];
database = setup_update (database);

count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_mdf_index])
	X IN DB.RDB$INDICES WITH X.RDB$INDEX_NAME = name->nam_string
    MODIFY X USING 
        if (node->syn_flags & s_dfi_flag_selectivity)
            X.RDB$UNIQUE_FLAG = (node->syn_flags & s_dfi_flag_unique);
        if (node->syn_flags & s_dfi_flag_activity)
            X.RDB$INDEX_INACTIVE = (node->syn_flags & s_dfi_flag_inactive);
	if (node->syn_flags & s_dfi_flag_order)
	    X.RDB$INDEX_TYPE = (node->syn_flags & s_dfi_flag_descending) ? TRUE  :FALSE;
	if (node->syn_flags & s_dfi_flag_statistics)
	    X.RDB$STATISTICS = -1.0;
    END_MODIFY
	ON_ERROR
	    rollback_update (database);
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
    count++;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (!count)
    ERRQ_print_error (246, name->nam_string, database->dbb_filename, NULL, NULL, NULL); /* Msg246 Index %s does not exist */

MET_meta_commit (database);
}

void MET_modify_relation (
    REL		relation,
    FLD		fields)
{
/**************************************
 *
 *	M E T _ m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Modify an existing relation.
 *
 **************************************/
DBB	database;
SYM	relation_name, field_name;
FLD	field;
USHORT	count;

relation->rel_database = database = setup_update (relation->rel_database); 
relation_name = relation->rel_symbol;

for (field = fields; field; field = field->fld_next)
    if (field->fld_flags & FLD_drop)
	{
	count = 0;
	field_name = field->fld_name;
	FOR (REQUEST_HANDLE database->dbb_requests [REQ_modify_rel])
		X IN DB.RDB$RELATION_FIELDS WITH 
		X.RDB$FIELD_NAME EQ field_name->sym_string AND
		X.RDB$RELATION_NAME EQ relation_name->sym_string
	    count++;
	    ERASE X
		ON_ERROR
		    rollback_update (database);
		    ERRQ_database_error (database, gds__status);
		END_ERROR;
	END_FOR
	    ON_ERROR
		rollback_update (database);
		ERRQ_database_error (database, gds__status);
	    END_ERROR;
	if (!count)
	    {
	    rollback_update (database);
	    ERRQ_print_error (247, field_name->sym_string, NULL, NULL, NULL, NULL); /* Msg247 field %s doesn't exist */
	    }
	}
    else if (field->fld_flags & FLD_modify)
	change_field (relation, field);
    else
	add_field (relation, field, 0);

MET_meta_commit (database);
setup_update (database);
delete_fields (relation);
MET_fields (relation);	    
}

void MET_ready (
    SYN		node,
    USHORT	create_flag)
{
/**************************************
 *
 *	M E T _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Create or Ready one or more databases.  If any
 *	fail, all fail.  Fair enough?
 *
 **************************************/
DBB	temp, dbb;
SYN	*ptr, *ptr2, *end;
STATUS	status_vector [20];
SSHORT	i, dpb_length, length;
UCHAR	*dpb, *p;
TEXT	*q;
TEXT	*lc_ctype;
     
p = parm_buffer;
temp = (DBB) node->syn_arg[0];

/* Only a SQL CREATE DATABASE will have a pagesize parameter,
   so only looking at the first pointer is justified.	*/

#if (defined JPN_SJIS || defined JPN_EUC)

    /* Always generate a dpb  and populate it with the language info.*/

    *p++ = gds__dpb_version1;
    *p++ = gds__dpb_interp;
    *p++ = 2;
    *p++ = QLI_interp % 256;
    *p++ = QLI_interp / 256;


#else
*p++ = gds__dpb_version1;
#endif

if ((lc_ctype = QLI_charset) && (length = strlen (lc_ctype)))
    {
    *p++ = gds__dpb_lc_ctype;
    *p++ = length;
    while (*lc_ctype)
	*p++ = *lc_ctype++;
    }

if (QLI_trace)
    {
    *p++ = gds__dpb_trace;
    *p++ = 1;
    *p++ = 1;
    }

if (sw_buffers)
    {
    *p++ = gds__dpb_num_buffers;
    *p++ = 2;
    *p++ = sw_buffers % 256;
    *p++ = sw_buffers / 256;
    }

if (temp->dbb_pagesize)
    {
    *p++ = gds__dpb_page_size;
    *p++ = 2;
    *p++ = temp->dbb_pagesize % 256;
    *p++ = temp->dbb_pagesize / 256;
    }

/* get a username, if specified globally or on
   the command line */

if (temp->dbb_user)
    {
    q = (TEXT*) temp->dbb_user->con_data;
    length = temp->dbb_user->con_desc.dsc_length;
    }
else
    {
    q = QLI_default_user;
    length = strlen (QLI_default_user);
    }
if (length)
    {
    *p++ = gds__dpb_user_name;
    *p++ = length;
    while (length--)
	*p++ = *q++;
    }

/* get a password, if specified */

if (temp->dbb_password)
    {
    q = (TEXT*) temp->dbb_password->con_data;
    length = temp->dbb_password->con_desc.dsc_length;
    }
else
    {
    q = QLI_default_password;
    length = strlen (QLI_default_password);
    }
if (length)
    {
    *p++ = gds__dpb_password;
    *p++ = length;
    while (length--)
	*p++ = *q++;
    }

dpb_length = p - parm_buffer;
if (dpb_length == 1)
    {
    dpb = NULL;
    dpb_length = 0;
    }
else
    dpb = parm_buffer;

end = node->syn_arg + node->syn_count;

/* Start by attaching all databases */

for (ptr = node->syn_arg; ptr < end; ptr++)
    {
    dbb = (DBB) *ptr;
    if (create_flag)
	gds__create_database (status_vector, 
		0, 
		GDS_VAL (dbb->dbb_filename), 
		GDS_REF (dbb->dbb_handle), 
		dpb_length, 
		GDS_VAL (dpb),
		0);
    else
	gds__attach_database (status_vector, 
		0, 
		GDS_VAL (dbb->dbb_filename), 
		GDS_REF (dbb->dbb_handle), 
		dpb_length, 
		GDS_VAL (dpb));
    if (status_vector [1])
	break;
    }

/* If any attach failed, cleanup and give up */

if (ptr < end)
    {
    for (ptr2 = node->syn_arg; ptr2 < ptr; ptr2++)    
	detach (gds__status, *ptr2);
    ERRQ_database_error (dbb, status_vector);
    }


/* Databases are up and running.  Install each. */

for (ptr = node->syn_arg; ptr < end; ptr++)
    install (*ptr);
}

void MET_shutdown (void)
{
/**************************************
 *
 *	M E T _ s h u t d o w n
 *
 **************************************
 *
 * Functional description
 *	Shut down all attached databases.
 *
 **************************************/
STATUS	status_vector [20];
DBB	dbb;

for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
    if (detach (status_vector, dbb))
	gds__print_status (status_vector);
}

void MET_sql_alter_table (
    REL		relation,
    FLD		fields)
{
/**************************************
 *
 *	M E T _ s q l _ a l t e r _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Alter table data based on a SQL metadata request
 *
 **************************************/
DBB	database;
SYM	symbol;
FLD	field;
STATUS	status_vector [20];
RLB	rlb;
USHORT	count;

symbol = relation->rel_symbol;
relation->rel_database = database = setup_update (relation->rel_database); 
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_relation_def])
	X IN DB.RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ symbol->sym_string
    count++;
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;
if (!count)
    ERRQ_print_error (414, symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg237 Relation %s is lost */


rlb = NULL;
rlb = CHECK_RLB (rlb);

STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);
STUFF (gds__dyn_mod_rel);
STUFF_STRING (symbol->sym_string);

for (field = fields; field; field = field->fld_next)
    {
    if (field->fld_flags & FLD_add)
	add_sql_field (relation, field, 0, rlb);
    else if (field->fld_flags & FLD_drop)
	{
	STUFF (gds__dyn_delete_local_fld);
	STUFF_STRING (field->fld_name->sym_string);
	STUFF (gds__dyn_end);
	}
    }

STUFF (gds__dyn_end);
STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

execute_dynamic_ddl (database, rlb);
MET_meta_commit (database);

setup_update (database);
delete_fields (relation);
MET_fields (relation);
}

void MET_sql_cr_view (
    SYN		node)
{
/**************************************
 *
 *	M E T _ s q l _ c r _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Create a view from a SQL metadata request
 *
 **************************************/
STATUS	status_vector [20];
REL	relation;
DBB	database;
SYM	symbol;
RLB	rlb;

relation = (REL) node->syn_arg[s_crv_name];
relation->rel_database = database = setup_update (relation->rel_database); 
symbol = relation->rel_symbol;

rlb = NULL;
rlb = CHECK_RLB (rlb);
STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);

STUFF (gds__dyn_def_view);
STUFF_STRING (symbol->sym_string);

/* The meat of the blr generation will go here */

STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

execute_dynamic_ddl (database, rlb);
MET_meta_commit (database);
}

void MET_sql_grant (
    SYN		node)
{
/**************************************
 *
 *	M E T _ s q l _ g r a n t
 *
 **************************************
 *
 * Functional description
 *	Grant access privilege(s) on a SQL table
 *
 **************************************/

sql_grant_revoke (node, gds__dyn_grant);
}

void MET_sql_revoke (
    SYN		node)
{
/**************************************
 *
 *	M E T _ s q l _ r e v o k e
 *
 **************************************
 *
 * Functional description
 *	Revoke access privilege(s) on a SQL table
 *
 **************************************/

sql_grant_revoke (node, gds__dyn_revoke);
}

SLONG *MET_transaction (
    NOD_T	node_type,
    DBB		database)
{
/**************************************
 *
 *	M E T _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Finish off a transaction with a commit, prepare, or rollback.
 *	For commit and rollback, start a new transaction.
 *
 *	Multiple transaction coordination is handled
 *	by the caller.  This is a singleminded routine.
 *
 **************************************/
STATUS	status_vector [20];


switch (node_type)
    {
    case nod_commit:
	gds__commit_transaction (status_vector, 
	    GDS_REF (database->dbb_transaction));
	break;
	
    case nod_rollback:
	gds__rollback_transaction (status_vector, 
	    GDS_REF (database->dbb_transaction));
	break;
	
    case nod_prepare:
	gds__prepare_transaction (status_vector, 
	    GDS_REF (database->dbb_transaction));
	break;

    case nod_start_trans:
	gds__start_transaction (status_vector, 
	    GDS_REF (database->dbb_transaction), 
	    1, 
	    GDS_REF (database->dbb_handle),
	    sizeof (tpb), 
	    tpb);
	database->dbb_flags &= ~DBB_updates & ~DBB_prepared;
	break;
    }

if (status_vector [1])
    ERRQ_database_error (database, status_vector);

return (database->dbb_transaction);
}

static void add_field (
    REL		relation,
    FLD		field,
    USHORT	position)
{
/**************************************
 *
 *	a d d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Add a field to a relation.  Do all sorts of sanity checks.
 *
 **************************************/
DBB	database;
SYM	relation_name, field_name, symbol, global_field;
USHORT	global_flag;

database = relation->rel_database;
relation_name = relation->rel_symbol;
field_name = field->fld_name;
if (!(global_field = field->fld_based))
    global_field = field_name;

/* Check to see if it already exits in relation */

FOR (REQUEST_HANDLE database->dbb_requests [REQ_rfr_def]) 
	X IN DB.RDB$RELATION_FIELDS WITH 
	X.RDB$RELATION_NAME EQ relation_name->sym_string AND
	X.RDB$FIELD_NAME EQ field_name->sym_string
    rollback_update (relation->rel_database);
    ERRQ_print_error (251, field_name->sym_string, relation_name->sym_string, NULL, NULL, NULL); /* Msg251 Field %s already exists in relation %s */
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

/* Check global field.  Define it if it doesn't exist. */

global_flag = FALSE;
if (!check_global_field (database, field, global_field->sym_string))
    {
    global_flag = TRUE;
    define_global_field (database, field, global_field); 
    }

/* Finally, store into RFR.  If we defined a global field, assume that
   the query name and edit string will be inherited from there, otherwise
   include them here */

STORE (REQUEST_HANDLE database->dbb_requests [REQ_store_rfr])
	X IN DB.RDB$RELATION_FIELDS

    strcpy (X.RDB$RELATION_NAME, relation_name->sym_string);
    strcpy (X.RDB$FIELD_NAME, field_name->sym_string);
    strcpy (X.RDB$FIELD_SOURCE, global_field->sym_string);
    if (position)
	{
	X.RDB$FIELD_POSITION.NULL = FALSE;
	X.RDB$FIELD_POSITION = position;
	}
    else
	X.RDB$FIELD_POSITION.NULL = TRUE;
    if (!global_flag && (symbol = field->fld_query_name))
	{
	X.RDB$QUERY_NAME.NULL = FALSE;
	strcpy (X.RDB$QUERY_NAME, symbol->sym_string);
	}
    else
	X.RDB$QUERY_NAME.NULL = TRUE;
    if (!global_flag && (field->fld_edit_string))
	{
	X.RDB$EDIT_STRING.NULL = FALSE;
	strcpy (X.RDB$EDIT_STRING, field->fld_edit_string);
	}
    else
	X.RDB$EDIT_STRING.NULL = TRUE;
	
END_STORE
    ON_ERROR
	rollback_update (relation->rel_database);
	ERRQ_database_error (relation->rel_database, gds__status);
    END_ERROR;
}

static void add_sql_field (
    REL		relation,
    FLD		field,
    USHORT	position,
    RLB		rlb)
{
/**************************************
 *
 *	a d d _ s q l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Add a SQL field to a relation via dyn.
 *
 **************************************/
DBB	database;
SYM	relation_name, field_name, symbol, global_field;
USHORT	l;
STATUS	status_vector [20];

rlb = CHECK_RLB (rlb); 
database = relation->rel_database;
relation_name = relation->rel_symbol;
field_name = field->fld_name;

/* Check to see if it already exits in relation */

FOR (REQUEST_HANDLE database->dbb_requests [REQ_rfr_def]) 
	X IN DB.RDB$RELATION_FIELDS WITH 
	X.RDB$RELATION_NAME EQ relation_name->sym_string AND
	X.RDB$FIELD_NAME EQ field_name->sym_string
    rollback_update (relation->rel_database);
    ERRQ_print_error (251, field_name->sym_string, relation_name->sym_string, NULL, NULL, NULL); /* Msg251 Field %s already exists in relation %s */
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;


STUFF (gds__dyn_def_sql_fld);
STUFF_STRING (field->fld_name->sym_string);

STUFF (gds__dyn_fld_type);
STUFF_WORD (2);
STUFF_WORD (blr_dtypes [field->fld_dtype]);

STUFF (gds__dyn_fld_length);
STUFF_WORD (2);
l = (field->fld_dtype == dtype_varying) ? field->fld_length - sizeof (SSHORT) : field->fld_length;
STUFF_WORD (l);

STUFF (gds__dyn_fld_scale);
STUFF_WORD (2);
STUFF_WORD (field->fld_scale);

if (position)
    {
    STUFF (gds__dyn_fld_position);
    STUFF_WORD (2);
    STUFF_WORD (position);
    }

rlb = CHECK_RLB (rlb); 

if (field->fld_flags & FLD_not_null)
    {
    STUFF (gds__dyn_fld_validation_blr);
    STUFF_WORD (8);
    STUFF (blr_version4);
    STUFF (blr_not);
    STUFF (blr_missing);
    STUFF (blr_fid);
    STUFF (0);
    STUFF (0);
    STUFF (0);
    STUFF (blr_eoc);
    }

STUFF (gds__dyn_end);
}   

static int blob_copy (
    RLB		rlb,
    REL		source,
    SLONG	*source_blob_id)
{
/**************************************
 *
 *	b l o b _ c o p y
 *
 **************************************
 *
 * Functional description
 *	Copy information from one blob to a request
 *	language block to stick it into a new definition.
 *
 **************************************/
DBB	source_dbb;
SLONG	*source_trans;
SLONG	*source_blob, size, segment_count, max_segment;
STATUS	status_vector [20]; 
USHORT	length, buffer_length;
TEXT	*buffer, fixed_buffer[4096], *p;

source_dbb = (source->rel_database) ? source->rel_database : QLI_databases ;
DB = source_dbb->dbb_handle;
source_blob = NULL;

if (gds__open_blob (status_vector,
	GDS_REF (DB),
	GDS_REF (source_dbb->dbb_meta_trans),
	GDS_REF (source_blob),
	GDS_VAL (source_blob_id)))
    {
    rollback_update (DB);
    ERRQ_database_error (source_dbb, status_vector);
    }

gds__blob_size (&source_blob, &size, &segment_count, &max_segment);

#if JPN_EUC
max_segment *= 2;	/* prepare for SJIS->EUC expansion */
#endif /* JPN_EUC */

if (max_segment < sizeof (fixed_buffer))
    {
    buffer_length = sizeof (fixed_buffer);
    buffer = fixed_buffer;
    }
else
    {
    buffer_length = max_segment;
    buffer = (TEXT *) gds__alloc ((SLONG) buffer_length);
    }

STUFF_WORD ((USHORT)size);

while (!gds__get_segment (status_vector,
	    GDS_REF (source_blob),
	    GDS_REF (length),
	    buffer_length,
	    GDS_VAL (buffer)))
    {
    while (rlb->rlb_limit - rlb->rlb_data < length)
	rlb = GEN_rlb_extend (rlb);
    p = buffer; 
    while (length--)
	STUFF (*p++);    	
    }

if (status_vector[1] != gds__segstr_eof)
    {
    rollback_update (DB); 
    if (buffer != fixed_buffer)
	gds__free (buffer);
    ERRQ_database_error (source_dbb, status_vector);
    }

if (buffer != fixed_buffer)
    gds__free (buffer);

if (gds__close_blob (status_vector,
	GDS_REF (source_blob)))
    {
    rollback_update (DB);
    ERRQ_database_error (source_dbb, status_vector);
    } 

return TRUE;
}

static void change_field (
    REL		relation,
    FLD		field)
{
/**************************************
 *
 *	c h a n g e  _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Change optional attributes of a field.
 *
 **************************************/
DBB	database;
SYM	relation_name, field_name, symbol, global_field;
USHORT	count;

database = relation->rel_database;
relation_name = relation->rel_symbol;
field_name = field->fld_name;
global_field = field->fld_based;

if (field->fld_dtype) 
    {
    rollback_update (database);
    ERRQ_print_error (252, NULL, NULL, NULL, NULL, NULL); /* Msg252 datatype can not be changed locally */
    }

if (global_field && !(check_global_field (database, NULL_PTR, global_field->sym_string)))
    {
    rollback_update (database);
    ERRQ_print_error (253, global_field->sym_string, NULL, NULL, NULL, NULL);  /* Msg253 global field %s does not exist */
    }

/* Modify  RFR */
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_mdf_rfr])
	X IN DB.RDB$RELATION_FIELDS WITH 
	X.RDB$RELATION_NAME = relation_name->sym_string AND
	X.RDB$FIELD_NAME = field_name->sym_string
    count++;
    MODIFY X USING
	if (global_field)
	    strcpy (X.RDB$FIELD_SOURCE, global_field->sym_string);
	if (symbol = field->fld_query_name)
	    strcpy (X.RDB$QUERY_NAME, symbol->sym_string);
	if (field->fld_edit_string)
	    strcpy (X.RDB$EDIT_STRING, field->fld_edit_string);
    END_MODIFY
	ON_ERROR
	    rollback_update (relation->rel_database);
	    ERRQ_database_error (relation->rel_database, gds__status);
	END_ERROR;
END_FOR
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;

if (!count)
    {
    rollback_update (database);
    ERRQ_print_error (254, field_name->sym_string, relation_name->sym_string, NULL, NULL, NULL); /* Msg254 field %s not found in relation %s */
    }
}
 
static int check_global_field (
    DBB		database,
    FLD		field,
    TEXT	*name)
{
/**************************************
 *
 *	c h e c k _ g l o b a l _ f i e l d 
 *
 **************************************
 *
 * Functional description
 *	Given a name, check for the existence of a global field
 *	by that name.  If a FLD block is supplied, 
 *      check its characteristics against the global.
 *	If it is not fully defined, flesh it out from the global.  
 *
 **************************************/
BOOLEAN	previously_defined;        
SLONG	*blob;

previously_defined = FALSE;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_field_def]) 
    X IN DB.RDB$FIELDS WITH X.RDB$FIELD_NAME EQ name
    if (field)
	{
	if (field->fld_dtype &&
	    (X.RDB$FIELD_TYPE != blr_dtypes [field->fld_dtype] || 
	    X.RDB$FIELD_LENGTH != ((field->fld_dtype == dtype_varying) ? field->fld_length - sizeof (SSHORT) : field->fld_length) ||
#if (! (defined JPN_SJIS || defined JPN_EUC) )
	    
	    X.RDB$FIELD_SCALE != field->fld_scale))

#else
	    /* The scale of char type data columns is set by triggers,
	       so it is possible that the scale specifed by the user may not
	       match the actual scale in the database, so dont complain */

	    (field->fld_dtype > dtype_varying && X.RDB$FIELD_SCALE != field->fld_scale)))

#endif
	    {
	    rollback_update (database);
	    ERRQ_print_error (255, name, NULL, NULL, NULL, NULL); /* Msg255 Datatype conflict with existing global field %s */
	    }                        
	field->fld_dtype = MET_get_datatype (X.RDB$FIELD_TYPE);
	field->fld_length = field_length (field->fld_dtype, X.RDB$FIELD_LENGTH);
        if (field->fld_dtype == dtype_varying)
            field->fld_length += sizeof (SSHORT);
	field->fld_scale = X.RDB$FIELD_SCALE;  
	field->fld_sub_type = X.RDB$FIELD_SUB_TYPE;
	field->fld_sub_type_missing = X.RDB$FIELD_SUB_TYPE.NULL;
        if (!field->fld_edit_string)
	    field->fld_edit_string = make_string (X.RDB$EDIT_STRING, 
						  sizeof (X.RDB$EDIT_STRING));
        if (!field->fld_query_name)
	    field->fld_query_name = make_symbol (X.RDB$QUERY_NAME, 
						 sizeof (X.RDB$QUERY_NAME));
        if (!field->fld_query_header)
            {
	    blob = (SLONG*) &X.RDB$QUERY_HEADER;
	    if (blob [0] || blob [1])      
	        field->fld_query_header = get_query_header (database, blob);
	    }
        }
    previously_defined = TRUE;
END_FOR
    ON_ERROR
	ERRQ_database_error (database, gds__status);
    END_ERROR;

return previously_defined;
}

static int check_relation (
    REL		relation)
{
/**************************************
 *
 *	c h e c k _ r e l a t i o n  
 *
 **************************************
 *
 * Functional description
 *	Check the existence of the named relation.
 *
 **************************************/
BOOLEAN	previously_defined;        
SLONG	*spare;

previously_defined = FALSE;

spare = DB;
DB = relation->rel_database->dbb_handle; 

FOR (REQUEST_HANDLE relation->rel_database->dbb_requests [REQ_relation_def]
    TRANSACTION_HANDLE relation->rel_database->dbb_meta_trans)
	X IN DB.RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
    previously_defined = TRUE;
END_FOR
    ON_ERROR
	ERRQ_database_error (relation->rel_database, gds__status);
    END_ERROR;

DB = spare;
return previously_defined;
}

static int clone_fields (
    REL		target,
    REL		source)
{
/**************************************
 *
 *	c l o n e _ f i e l d s 
 *
 **************************************
 *
 * Functional description
 *	Copy the RFR records for one relation
 *	into another.  The target database is already
 *	setup for updates.  If there is a different source
 *	database, get it ready too.  Then make sure that
 *	the source relation actually exists.
 *
 *	If it's two different databases, create the global
 *	fields that are missing in the target (except those
 *	that support computed fields), then create the local 
 *	fields and during that pass create the computed
 *	fields and the SQL fields.
 *
 **************************************/
SLONG	*req1, *req2;
SLONG	*t_trans, *s_trans;
RLB	rlb;
SSHORT	count;

req1 = req2 = NULL;
s_trans = t_trans = gds__trans;
DB = DB1 = target->rel_database->dbb_handle;

if (target->rel_database != source->rel_database)
    {
    MET_meta_transaction (source->rel_database, FALSE);
    s_trans = gds__trans;
    }

if (target->rel_database != source->rel_database)
    clone_global_fields (target, source); 

rlb = NULL;
rlb = CHECK_RLB (rlb);

STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);

FOR (REQUEST_HANDLE req1 
     TRANSACTION_HANDLE s_trans) Y IN DB.RDB$RELATION_FIELDS 
	CROSS F IN DB.RDB$FIELDS
	WITH Y.RDB$RELATION_NAME = source->rel_symbol->sym_string 
	    AND Y.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME

    /* If SQL made this field, create a new global field for it */

    if (F.RDB$COMPUTED_BLR.NULL &&
	(strncmp ("RDB$", Y.RDB$FIELD_NAME, 4)) &&
	(!(strncmp ("RDB$", F.RDB$FIELD_NAME, 4)))) 
	{
	STUFF (gds__dyn_def_sql_fld);
	STUFF_STRING (Y.RDB$FIELD_NAME);
	STUFF (gds__dyn_fld_type);
	STUFF_WORD (2);
	STUFF_WORD (F.RDB$FIELD_TYPE);

	STUFF (gds__dyn_fld_length);
	STUFF_WORD (2);
	STUFF_WORD (F.RDB$FIELD_LENGTH);

	STUFF (gds__dyn_fld_scale);
	STUFF_WORD (2);
	STUFF_WORD (F.RDB$FIELD_SCALE);
 	
	if (!F.RDB$VALIDATION_BLR.NULL)
	    {
	    STUFF (gds__dyn_fld_validation_blr);
	    blob_copy (rlb, source, &F.RDB$VALIDATION_BLR);
	    }
	}
    else
	{
	STUFF (gds__dyn_def_local_fld);
	STUFF_STRING (Y.RDB$FIELD_NAME);
	STUFF (gds__dyn_fld_source); 
	STUFF_STRING (Y.RDB$FIELD_SOURCE); 
	}

    STUFF (gds__dyn_rel_name);
    STUFF_STRING (target->rel_symbol->sym_string);
	
    if (!Y.RDB$FIELD_POSITION.NULL)
	{
	STUFF (gds__dyn_fld_position); 
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$FIELD_POSITION);
	}

    if (!Y.RDB$QUERY_NAME.NULL)
	{
	STUFF (gds__dyn_fld_query_name); 
	STUFF_STRING (Y.RDB$QUERY_NAME);
	}
	
    if (!Y.RDB$EDIT_STRING.NULL)
	{
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_edit_string); 

#else

	STUFF (gds__dyn_fld_edit_string2); 
	STUFF_WORD(QLI_interp);

#endif
	STUFF_STRING (Y.RDB$EDIT_STRING); 
	}
    if (!Y.RDB$UPDATE_FLAG.NULL)
	{
	STUFF (gds__dyn_update_flag); 
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$UPDATE_FLAG); 
	}
	
    if (!Y.RDB$QUERY_HEADER.NULL)
	{
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_query_header);

#else

	STUFF (gds__dyn_fld_query_header2);
	STUFF_WORD(QLI_interp);

#endif
	blob_copy (rlb, source, &Y.RDB$QUERY_HEADER); 
	}

    if (!Y.RDB$DESCRIPTION.NULL) 
	{
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_description);

#else

	STUFF (gds__dyn_description2);
	STUFF_WORD(QLI_interp);

#endif

	blob_copy (rlb, source, &Y.RDB$DESCRIPTION);
	}

    if (!Y.RDB$DEFAULT_VALUE.NULL) 
	{
	STUFF (gds__dyn_fld_default_value);
	blob_copy (rlb, source, &Y.RDB$DEFAULT_VALUE);  
	}

    if ((source->rel_database->dbb_capabilities & DBB_cap_rfr_sys_flag) &&
	(target->rel_database->dbb_capabilities & DBB_cap_rfr_sys_flag)) 
	FOR (REQUEST_HANDLE req2
	    TRANSACTION_HANDLE s_trans)
	    S_RFR IN DB.RDB$RELATION_FIELDS WITH 
		S_RFR.RDB$FIELD_NAME = Y.RDB$FIELD_NAME AND
	    	S_RFR.RDB$RELATION_NAME = Y.RDB$RELATION_NAME
	if (!S_RFR.RDB$SYSTEM_FLAG.NULL)
	    {
	    STUFF (gds__dyn_system_flag);
	    STUFF_WORD (2);
	    STUFF_WORD (S_RFR.RDB$SYSTEM_FLAG);
	    }
	
	END_FOR
	    ON_ERROR
		rollback_update (target->rel_database);
		ERRQ_database_error (source->rel_database, gds__status);
	    END_ERROR;

    if (!F.RDB$COMPUTED_BLR.NULL)
	{
	STUFF (gds__dyn_fld_computed_blr);
	blob_copy (rlb, source, &F.RDB$COMPUTED_BLR);
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_computed_source);

#else
	STUFF (gds__dyn_fld_computed_source2);
	STUFF_WORD(QLI_interp);

#endif

	blob_copy (rlb, source, &F.RDB$COMPUTED_SOURCE);

	STUFF (gds__dyn_fld_type);
	STUFF_WORD (2);
	STUFF_WORD (F.RDB$FIELD_TYPE);

	STUFF (gds__dyn_fld_length);
	STUFF_WORD (2);
	STUFF_WORD (F.RDB$FIELD_LENGTH);

	if (!F.RDB$FIELD_SCALE.NULL)
	    {
	    STUFF (gds__dyn_fld_scale);
	    STUFF_WORD (2);
	    STUFF_WORD (F.RDB$FIELD_SCALE);
	    }

	if (!F.RDB$FIELD_SUB_TYPE.NULL)
	    {
    	    STUFF (gds__dyn_fld_sub_type);
	    STUFF_WORD (2);
	    STUFF_WORD (F.RDB$FIELD_SUB_TYPE);
	    }
	}				
    STUFF (gds__dyn_end);
END_FOR
    ON_ERROR
	rollback_update (source->rel_database);
	ERRQ_database_error (source->rel_database, gds__status);
    END_ERROR;

STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

execute_dynamic_ddl (target->rel_database, rlb);

if (req1)
    if (gds__release_request (gds__status, GDS_REF (req1))) 
	ERRQ_database_error (source->rel_database, gds__status);
if (req2)
    if (gds__release_request (gds__status, GDS_REF (req2))) 
	ERRQ_database_error (source->rel_database, gds__status);
return TRUE;
}

static clone_global_fields (
    REL		target,
    REL		source)
{
/**************************************
 *
 *	c l o n e _ g l o b a l _ f i e l d s 
 *
 **************************************
 *
 * Functional description
 *	Copy the global field definitions required by a relation
 *      from one database into another.  Both databases are
 *	setup correctly.  (Set up the handles again just because
 *	we're compulsive.) 
 *
 *	In this routine, as in clone fields, be careful to probe
 *	for our extensions rather than just using them so we
 *	can copy relations from V2 and from Rdb.
 *
 *	Don't clone fields that already exist and have the
 *	correct datatype and length.
 *
 *	Don't clone computed fields since they will be cloned
 *	later with the local field definitions.   Don't clone
 *	SQL defined fields for the same reason.
 *
 **************************************/
SLONG	*req1, *req2, *req3;
SLONG	*t_trans, *s_trans;
TEXT	*name;
SSHORT	first_field, first_dimension, predefined;
RLB	rlb;

req1 = req2 = req3 = NULL;
s_trans = source->rel_database->dbb_meta_trans;
t_trans = target->rel_database->dbb_meta_trans;
DB = source->rel_database->dbb_handle;
DB1 = target->rel_database->dbb_handle;

rlb = NULL;
first_field = TRUE;

FOR (REQUEST_HANDLE req1 
     TRANSACTION_HANDLE s_trans) RFR IN DB.RDB$RELATION_FIELDS CROSS
	Y IN DB.RDB$FIELDS 
	WITH RFR.RDB$RELATION_NAME = source->rel_symbol->sym_string AND
	Y.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE REDUCED TO Y.RDB$FIELD_NAME

    if (!Y.RDB$COMPUTED_BLR.NULL ||
	((strncmp ("RDB$", RFR.RDB$FIELD_NAME, 4)) &&
	(!(strncmp ("RDB$", Y.RDB$FIELD_NAME, 4))))) 
	continue;

    predefined = FALSE;

    FOR (REQUEST_HANDLE req2 TRANSACTION_HANDLE t_trans) 
	A IN DB1.RDB$FIELDS WITH A.RDB$FIELD_NAME = Y.RDB$FIELD_NAME

	if ((A.RDB$FIELD_TYPE != Y.RDB$FIELD_TYPE) ||
		(A.RDB$FIELD_LENGTH != Y.RDB$FIELD_LENGTH) ||
	        (A.RDB$FIELD_SCALE.NULL != Y.RDB$FIELD_SCALE.NULL) ||
		((!Y.RDB$FIELD_SCALE.NULL) && 
		    (A.RDB$FIELD_SCALE != Y.RDB$FIELD_SCALE)) ||
		(A.RDB$FIELD_SUB_TYPE.NULL != Y.RDB$FIELD_SUB_TYPE.NULL) ||
		((!Y.RDB$FIELD_SUB_TYPE.NULL) &&
		    (A.RDB$FIELD_SUB_TYPE != Y.RDB$FIELD_SUB_TYPE))) 
	    {
	    name = (TEXT *) ALLQ_malloc ((SLONG) sizeof (Y.RDB$FIELD_NAME));
	    strcpy (name, Y.RDB$FIELD_NAME); 
	    rollback_update (target->rel_database);
	    ERRQ_error (413, name, NULL, NULL, NULL, NULL);   /* conflicting previous definition */
	    }
        else
	    predefined = TRUE;
    END_FOR
	ON_ERROR
	    ERRQ_database_error (target->rel_database, gds__status);
	END_ERROR;

    if (predefined)
	continue;

    if (first_field)
	{
	rlb = CHECK_RLB (rlb); 
	STUFF (gds__dyn_version_1);
	STUFF (gds__dyn_begin);
	first_field = FALSE;
	}
    STUFF (gds__dyn_def_global_fld);
    STUFF_STRING (Y.RDB$FIELD_NAME);

    STUFF (gds__dyn_fld_type);
    STUFF_WORD (2);
    STUFF_WORD (Y.RDB$FIELD_TYPE);

    STUFF (gds__dyn_fld_length);
    STUFF_WORD (2);
    STUFF_WORD (Y.RDB$FIELD_LENGTH);

    if (!Y.RDB$FIELD_SCALE.NULL)
	{
	STUFF (gds__dyn_fld_scale);
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$FIELD_SCALE);
	}

    if (!Y.RDB$FIELD_SUB_TYPE.NULL)
	{
	STUFF (gds__dyn_fld_sub_type);
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$FIELD_SUB_TYPE);
	}

    if (!Y.RDB$SEGMENT_LENGTH.NULL)
	{
	STUFF (gds__dyn_fld_segment_length);
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$SEGMENT_LENGTH);
	}

    if (!Y.RDB$SYSTEM_FLAG.NULL)
	{
	STUFF (gds__dyn_system_flag);
	STUFF_WORD (2);
	STUFF_WORD (Y.RDB$SYSTEM_FLAG);
	}

    if (!Y.RDB$QUERY_NAME.NULL)
	{ 
	STUFF (gds__dyn_fld_query_name);
        STUFF_STRING (Y.RDB$QUERY_NAME);
	}

    if (!Y.RDB$EDIT_STRING.NULL)
	{ 
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_edit_string);

#else

	STUFF (gds__dyn_fld_edit_string2);
	STUFF_WORD(QLI_interp);

#endif

        STUFF_STRING (Y.RDB$EDIT_STRING);
	}
	
    if (!Y.RDB$MISSING_VALUE.NULL)
	{ 
	STUFF (gds__dyn_fld_missing_value);
	blob_copy (rlb, source, &Y.RDB$MISSING_VALUE);
	}

    if (!Y.RDB$DEFAULT_VALUE.NULL)
	{ 
	STUFF (gds__dyn_fld_default_value);
	blob_copy (rlb, source, &Y.RDB$DEFAULT_VALUE);
	}

    if (!Y.RDB$QUERY_HEADER.NULL)
	{ 
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_query_header);

#else

	STUFF (gds__dyn_fld_query_header2);
	STUFF_WORD(QLI_interp);

#endif
	blob_copy (rlb, source, &Y.RDB$QUERY_HEADER); 
	}

    if (!Y.RDB$DESCRIPTION.NULL)
	{ 
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_description);

#else

	STUFF (gds__dyn_description2);
	STUFF_WORD(QLI_interp);

#endif

	blob_copy (rlb, source, &Y.RDB$DESCRIPTION);
	}

    if (!Y.RDB$VALIDATION_BLR.NULL)
	{ 
	STUFF (gds__dyn_fld_validation_blr);
	blob_copy (rlb, source, &Y.RDB$VALIDATION_BLR);
	}

    if (!Y.RDB$VALIDATION_SOURCE.NULL)
	{ 
#if (! (defined JPN_SJIS || defined JPN_EUC) )

	STUFF (gds__dyn_fld_validation_source);

#else

	STUFF (gds__dyn_fld_validation_source2);
	STUFF_WORD(QLI_interp);

#endif

	blob_copy (rlb, source, &Y.RDB$VALIDATION_SOURCE);
	}

    if ((target->rel_database->dbb_capabilities & DBB_cap_dimensions) &&
        (source->rel_database->dbb_capabilities & DBB_cap_dimensions))
        {
	first_dimension = TRUE;
	FOR (REQUEST_HANDLE req3 TRANSACTION_HANDLE s_trans)
	    F IN DB.RDB$FIELDS CROSS 
	    D IN DB.RDB$FIELD_DIMENSIONS
	    WITH F.RDB$FIELD_NAME = D.RDB$FIELD_NAME AND
	    F.RDB$FIELD_NAME = Y.RDB$FIELD_NAME
	    if (first_dimension)
		{
		first_dimension = FALSE;
		STUFF (gds__dyn_fld_dimensions); 
		STUFF_WORD (2);
		STUFF_WORD (F.RDB$DIMENSIONS); 
		}
	    STUFF (gds__dyn_def_dimension);
	    STUFF_WORD (2);
	    STUFF_WORD (D.RDB$DIMENSION);
	    if (!D.RDB$LOWER_BOUND.NULL)
		{
		STUFF (gds__dyn_dim_lower);
		STUFF_WORD (2);
		STUFF_WORD (D.RDB$LOWER_BOUND);
		}
	    if (!D.RDB$UPPER_BOUND.NULL)
		{
		STUFF (gds__dyn_dim_upper);
		STUFF_WORD (2);
		STUFF_WORD (D.RDB$UPPER_BOUND);
		}
	    STUFF (gds__dyn_end);
	END_FOR
	    ON_ERROR
		if (target->rel_database->dbb_meta_trans)
		    rollback_update (target->rel_database);
		rollback_update (source->rel_database->dbb_meta_trans);
		ERRQ_database_error (source->rel_database, gds__status);
	    END_ERROR;
	}
    STUFF (gds__dyn_end);
END_FOR
    ON_ERROR
	rollback_update (source->rel_database);
	if (target->rel_database->dbb_meta_trans)
	    rollback_update (target->rel_database);
	ERRQ_database_error (source->rel_database, gds__status);
    END_ERROR;

if (rlb)
    {
    STUFF (gds__dyn_end);
    STUFF (gds__dyn_eoc);
    execute_dynamic_ddl (target->rel_database, rlb);
    }

if (req1)
    if (gds__release_request (gds__status, GDS_REF (req1))) 
	ERRQ_database_error (source->rel_database, gds__status);
if (req2)
    if (gds__release_request (gds__status, GDS_REF (req2))) 
	ERRQ_database_error (target->rel_database, gds__status);
if (req3)
    if (gds__release_request (gds__status, GDS_REF (req3))) 
	ERRQ_database_error (source->rel_database, gds__status);
return TRUE;
}

static void define_global_field (
    DBB		database,
    FLD		field,
    SYM		name)
{
/**************************************
 *
 *	d e f i n e _ g l o b a l _ f i e l d 
 *
 **************************************
 *
 * Functional description
 *
 *	Define a global field if we've got enough
 *	information.
 *
 **************************************/
SYM	symbol;


if (!field->fld_dtype)
    {
    rollback_update (database);
    ERRQ_print_error (256, name->sym_string, NULL, NULL, NULL, NULL); /* Msg256 No datatype specified for field %s */
    }
STORE (REQUEST_HANDLE database->dbb_requests [REQ_store_field])
	X IN DB.RDB$FIELDS
    strcpy (X.RDB$FIELD_NAME, name->sym_string);
    X.RDB$FIELD_TYPE = blr_dtypes [field->fld_dtype];
    X.RDB$FIELD_SCALE = field->fld_scale;
    X.RDB$FIELD_LENGTH = (field->fld_dtype == dtype_varying) ?
	field->fld_length - sizeof (SSHORT) : field->fld_length;
    if (!field->fld_sub_type_missing)
	{
	X.RDB$FIELD_SUB_TYPE.NULL = FALSE;
	X.RDB$FIELD_SUB_TYPE = field->fld_sub_type;
	}
    else
	X.RDB$FIELD_SUB_TYPE.NULL = TRUE;
    if (symbol = field->fld_query_name)
	{
	X.RDB$QUERY_NAME.NULL = FALSE;
	strcpy (X.RDB$QUERY_NAME, symbol->sym_string);
	}
    else
	X.RDB$QUERY_NAME.NULL = TRUE;
    if (field->fld_edit_string)
	{
	X.RDB$EDIT_STRING.NULL = FALSE;
	strcpy (X.RDB$EDIT_STRING, field->fld_edit_string);
	}
    else
	X.RDB$EDIT_STRING.NULL = TRUE;
END_STORE
    ON_ERROR
	rollback_update (database);
	ERRQ_database_error (database, gds__status);
    END_ERROR;
}

static void delete_fields (
    REL		relation)
{
/**************************************
 *
 *	d e l e t e _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Delete field definitions.
 *
 **************************************/
FLD	field;

while (field = relation->rel_fields)
    {
    relation->rel_fields = field->fld_next;
    ALL_release (field->fld_name);
    if (field->fld_query_name)
	ALL_release (field->fld_query_name);
    ALL_release (field);
    }

relation->rel_flags &= ~REL_fields;
}

static STATUS detach (
    STATUS	*status_vector,
    DBB		dbb)
{
/**************************************
 *
 *	d e t a c h
 *
 **************************************
 *
 * Functional description
 *	Shut down a database.  Return the status of the
 *	first failing call, if any.
 *
 **************************************/
STATUS	*status, alt_vector [20];

if (!dbb->dbb_handle)
    return SUCCESS;

status = status_vector;

if (dbb->dbb_transaction)
    if (gds__commit_transaction (
	GDS_VAL (status), 
	GDS_REF (dbb->dbb_transaction)))
	status = alt_vector;

if (dbb->dbb_proc_trans && (dbb->dbb_capabilities & DBB_cap_multi_trans))
    if (gds__commit_transaction (
	GDS_VAL (status), 
	GDS_REF (dbb->dbb_proc_trans)))
	status = alt_vector;

if (dbb->dbb_meta_trans && (dbb->dbb_capabilities & DBB_cap_multi_trans))
    if (gds__commit_transaction (
	GDS_VAL (status),
	GDS_REF (dbb->dbb_meta_trans)))
	status = alt_vector;

gds__detach_database (
	GDS_VAL (status), 
	GDS_REF (dbb->dbb_handle));

return status_vector [1];
}

static int execute_dynamic_ddl (
    DBB		database,
    RLB		rlb)
{
/**************************************
 *
 *	e x e c u t e _ d y n a m i c _ d d l
 *
 **************************************
 *
 * Functional description
 *	Execute a ddl command, for better or for worse.
 *
 **************************************/
USHORT	length;

length = (UCHAR*) rlb->rlb_data - (UCHAR*) rlb->rlb_base;

if (QLI_blr)
    PRETTY_print_dyn (rlb->rlb_base, (FPTR_INT) ib_printf, "%4d %s\n", 0);

if (gds__ddl (gds__status,
	GDS_REF (database->dbb_handle), 
	GDS_REF (database->dbb_meta_trans), 
	length,
	GDS_VAL (rlb->rlb_base)))
    {
    rollback_update (database);
    ERRQ_database_error (database, gds__status);
    }

RELEASE_RLB;

return SUCCESS;
}

static int field_length (
    USHORT	dtype,
    USHORT	length)
{
/**************************************
 *
 *	f i e l d _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Return implementation specific length for field.
 *
 **************************************/

switch (dtype)
    {
    case dtype_short	: return sizeof (SSHORT);
    case dtype_long	: return sizeof (SLONG);
    case dtype_real	: return sizeof (float);
    case dtype_double	: return sizeof (double);

    case dtype_sql_time:
    case dtype_sql_date:
	return sizeof (SLONG);

    case dtype_timestamp:
    case dtype_blob:
    case dtype_quad: 
	return sizeof (GDS__QUAD);	
    }

return length;
}
 
static void get_database_type (
    DBB	new_dbb)
{
/**************************************
 *
 *	g e t _ d a t a b a s e _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Ask what type of database this is. 
 *
 **************************************/
STATUS	status_vector [20];
int	count;
USHORT	l;
UCHAR	item, buffer [1024], *p, *q;

gds__database_info (status_vector, 
	GDS_REF (new_dbb->dbb_handle),
	sizeof (db_info),
	db_info,
	sizeof (buffer),
	buffer);

if (status_vector [1])
    ERRQ_database_error (new_dbb, gds__status);

p = buffer;

while (*p != gds__info_end && p < buffer + sizeof (buffer))
    {
    item = *p++;
    l = gds__vax_integer (p, 2);
    p += 2;
    switch (item)
	{
 	case gds__info_implementation:
            q = p;
	    count = *q++;
	    while (count--)
		{
		new_dbb->dbb_type = *q++;
		if (*q++ == gds__info_db_class_access)
     		    break;
		}
	    break;
	
	default:
	    ERRQ_error (257, NULL, NULL, NULL, NULL, NULL); /* Msg257 database info call failed */
	}
    p += l;
    }
}

static void get_log_names (
    DBB		dbb,
    SCHAR	*db_name,
    LLS		*log_stack,
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
SCHAR    next_log [512];
SCHAR    *cl, *nl;
int     log_count, ret_val;
SSHORT   not_archived;
SLONG    last_log_flag, next_offset;
SLONG    log_seqno, log_length;
SSHORT	loop;
STR	string;

loop = 0;

/* loop up to 10 times to allow the file to be archived */

while (TRUE)
    {
    loop++;
    if (WALF_get_linked_logs_info (gds__status, db_name, cur_log, part_offset,
		&log_count, next_log, &next_offset,
		&last_log_flag, &not_archived) != SUCCESS)
	ERRQ_database_error (dbb, gds__status);

    if ((!not_archived) || force)
	break;

    if (not_archived && skip_delete)
	{
	*log_stack = (LLS) 0;
	return;
	}

    if (loop >= 10)
	 ERRQ_print_error (431, dbb->dbb_filename, NULL, NULL, NULL, NULL);
    }

if (log_count)
    {
    string = (STR) ALLOCDV (type_str, strlen (next_log));
    LLS_PUSH (string, log_stack);
    strcpy (string->str_data, next_log);
    }
else
    {
    string = (STR) ALLOCDV (type_str, strlen (cur_log));
    LLS_PUSH (string, log_stack);
    strcpy (string->str_data, cur_log);
    }

cl = next_log;
nl = cur_log;
part_offset = next_offset;

while (log_count)
    {
    ret_val = WALF_get_next_log_info (gds__status,
		db_name,
		cl, part_offset,
		nl, &next_offset,
                &log_seqno, &log_length,
		&last_log_flag, 1);

    if (ret_val == FAILURE)
	ERRQ_database_error (dbb, gds__status);

    if (ret_val < 0)
	break;

    string = (STR) ALLOCDV (type_str, strlen (nl));
    LLS_PUSH (string, log_stack);
    strcpy (string->str_data, nl);

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
    LLS		*log_stack)
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
SCHAR    next_log [512];
STR	string;

/* 
 * for round robin log files, some of the log files may not have been used
 * when the database is dropped or log is dropped. Add them to the list
 */

FOR L IN DB.RDB$LOG_FILES
    if (L.RDB$FILE_FLAGS & LOG_serial)
	continue;
    ISC_expand_filename (L.RDB$FILE_NAME, 0, next_log);
    string = (STR) ALLOCDV (type_str, strlen (next_log));
    LLS_PUSH (string, log_stack);
    strcpy (string->str_data, next_log);
END_FOR
    ON_ERROR
    END_ERROR;
}

static TEXT *get_query_header (
    DBB		database,
    SLONG	blob_id [2])
{
/**************************************
 *
 *	g e t _ q u e r y _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Pick up a query header for a field.
 *
 **************************************/
int	*blob;
STATUS	status, status_vector [20];
USHORT	length;
TEXT	header [1024], buffer [1024], *p, *q;
#if (defined JPN_EUC || defined JPN_SJIS)
USHORT   bpb_length;
UCHAR    bpb2[20], *bpb, *p2;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

blob = NULL;

#if (defined JPN_EUC || defined JPN_SJIS)
p2 = bpb2;
*p2++ = gds__bpb_version1;
*p2++ = gds__bpb_target_interp;
*p2++ = 2;
*p2++ = QLI_interp;
*p2++ = QLI_interp >> 8;
bpb_length = p2 - bpb2;
bpb = bpb2;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

#if (defined JPN_EUC || defined JPN_SJIS)
if (gds__open_blob2 (
	status_vector,
	GDS_REF (database->dbb_handle),
	GDS_REF (gds__trans),
	GDS_REF (blob),
	GDS_VAL (blob_id),
	bpb_length,
	bpb))
#else
if (gds__open_blob (
	status_vector,
	GDS_REF (database->dbb_handle),
	GDS_REF (gds__trans),
	GDS_REF (blob),
	GDS_VAL (blob_id)))
#endif /* (defined JPN_EUC || defined JPN_SJIS) */
    ERRQ_database_error (database, status_vector);

p = header;

for (;;)
    {
    status = gds__get_segment (
	status_vector,
	GDS_REF (blob),
	GDS_REF (length),
	sizeof (buffer),
	buffer);
    if (status && status != gds__segment)
	break;
    if (length && buffer [length - 1] == '\n')
	--length;
    buffer [length] = 0;
    q = buffer;
    if (*q == '"')
	do *p++ = *q++; while (*q);
    else
	{
	*p++ = '"';
	while (*q)
	    *p++ = *q++;
	*p++ = '"';
	}
    }

if (gds__close_blob (
	status_vector,
	GDS_REF (blob)))
    ERRQ_database_error (database, gds__status);

*p = 0;
if (!strcmp (header, "\" \""))
    {
    header [0] = '-'; header [1] = 0;
    p = header + 1;
    }

if (length = p - header)
    return make_string (header, length);

return NULL;
}

static void install (
    DBB		old_dbb)
{
/**************************************
 *
 *	i n s t a l l
 *
 **************************************
 *
 * Functional description
 *	Install a database.  Scan relations, etc., make up relation
 *	blocks, etc, and copy the database block to someplace more
 *	permanent.
 *
 **************************************/
DBB	new_dbb;
REL	relation;
NAM	name;
SYM	symbol, symbol2;
SLONG	*request, *request2;
SSHORT	l;
FUN	function;
TEXT	*p, *q;

/* Copy the database block to one out of the permanent pool */

l = old_dbb->dbb_filename_length;
new_dbb = (DBB) ALLOCPV (type_dbb, l);
new_dbb->dbb_filename_length = l;
p = new_dbb->dbb_filename;
q = old_dbb->dbb_filename;
do *p++ = *q++; while (--l);
DB = new_dbb->dbb_handle = old_dbb->dbb_handle;
new_dbb->dbb_capabilities = old_dbb->dbb_capabilities;

new_dbb->dbb_next = QLI_databases;
QLI_databases = new_dbb;

/* If a name was given, make up a symbol for the database. */

if (name = (NAM) old_dbb->dbb_symbol)
    {
    new_dbb->dbb_symbol = symbol = 
	make_symbol (name->nam_string, name->nam_length);
    symbol->sym_type = SYM_database;
    symbol->sym_object = (BLK) new_dbb;
    HSH_insert (symbol);
    }

gds__trans = MET_transaction (nod_start_trans, new_dbb);

/* Find out whose database this is so later we don't ask Rdb/VMS
   or its pals to do any unnatural acts.  While we're at it get
   general capabilities */

get_database_type (new_dbb);
set_capabilities (new_dbb);

/* Get all relations in database.  For each relation make up a
   relation block, make up a symbol block, and insert the symbol 
   into the hash table. */

request = NULL;

FOR (REQUEST_HANDLE request) X IN DB.RDB$RELATIONS SORTED BY DESCENDING
	X.RDB$RELATION_NAME
    relation = (REL) ALLOCP (type_rel);
    relation->rel_next = new_dbb->dbb_relations;
    new_dbb->dbb_relations = relation;
    relation->rel_database = new_dbb;
    relation->rel_id = X.RDB$RELATION_ID;
    symbol = make_symbol (X.RDB$RELATION_NAME, sizeof (X.RDB$RELATION_NAME));
    symbol->sym_type = SYM_relation;
    symbol->sym_object = (BLK) relation;
    relation->rel_symbol = symbol;
    HSH_insert (symbol);
    if (strcmp (symbol->sym_string, "QLI$PROCEDURES") == 0)
	new_dbb->dbb_flags |= DBB_procedures;
    if (relation->rel_system_flag = X.RDB$SYSTEM_FLAG)
	relation->rel_flags |= REL_system;
    if (!X.RDB$VIEW_BLR.NULL)
	relation->rel_flags |= REL_view;
END_FOR
    ON_ERROR
	ERRQ_database_error (new_dbb, gds__status);
    END_ERROR;

if (request)
    if (gds__release_request (gds__status, GDS_REF (request))) 
	ERRQ_database_error (new_dbb, gds__status);

/* Pick up functions, if appropriate */

if (new_dbb->dbb_capabilities & DBB_cap_functions)
    {
    request2 = NULL;
    FOR (REQUEST_HANDLE request) X IN DB.RDB$FUNCTIONS
	if (!(symbol = make_symbol (X.RDB$FUNCTION_NAME, sizeof (X.RDB$FUNCTION_NAME))))
	    continue;
	function = (FUN) ALLOCPV (type_fun, 0);
	function->fun_next = new_dbb->dbb_functions;
	new_dbb->dbb_functions = function;
	function->fun_symbol = symbol;
	function->fun_database = new_dbb;
	FOR (REQUEST_HANDLE request2) Y IN DB.RDB$FUNCTION_ARGUMENTS WITH
		Y.RDB$FUNCTION_NAME EQ X.RDB$FUNCTION_NAME AND
		Y.RDB$ARGUMENT_POSITION EQ X.RDB$RETURN_ARGUMENT
	    function->fun_return.dsc_dtype = MET_get_datatype (Y.RDB$FIELD_TYPE);
	    function->fun_return.dsc_length = 
		field_length (function->fun_return.dsc_dtype, Y.RDB$FIELD_LENGTH);
	    function->fun_return.dsc_scale = Y.RDB$FIELD_SCALE;
	END_FOR
	    ON_ERROR
		ERRQ_database_error (new_dbb, gds__status);
	    END_ERROR;
	if (!function->fun_return.dsc_dtype)
	    {
	    function->fun_return.dsc_dtype = dtype_text;
	    function->fun_return.dsc_length = 20;
	    }
	symbol->sym_type = SYM_function;
	symbol->sym_object = (BLK) function;
	HSH_insert (symbol);

	/* Insert another symbol if there is a function query name */

	if (symbol2 = make_symbol (X.RDB$QUERY_NAME, sizeof (X.RDB$QUERY_NAME)))
	    {
	    function->fun_query_name = symbol2;
	    symbol2->sym_type = SYM_function;
	    symbol2->sym_object = (BLK) function;
	    HSH_insert (symbol2);
	    }
    END_FOR
	ON_ERROR
	    ERRQ_database_error (new_dbb, gds__status);
	END_ERROR;
    if (request)
	if (gds__release_request (gds__status, GDS_REF (request))) 
	    ERRQ_database_error (new_dbb, gds__status);
    if (request2)
	if (gds__release_request (gds__status, GDS_REF (request2))) 
	    ERRQ_database_error (new_dbb, gds__status);
    }
}

static SYN make_node (
    NOD_T	type,
    USHORT	count)
{
/**************************************
 *
 *	m a k e _ n o d e
 *
 **************************************
 *
 * Functional description
 *	Generate a syntax node.
 *
 **************************************/
SYN	node;

node = (SYN) ALLOCPV (type_syn, count);
node->syn_count = count;
node->syn_type = type;

return node;
}

static TEXT *make_string (
    TEXT	*string,
    SSHORT	length)
{
/**************************************
 *
 *	m a k e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Make up a permanent string from a large string.  Chop off
 *	trailing blanks.  If there's nothing left, return NULL.
 *
 **************************************/
STR	str;
TEXT	*p;

string [length] = 0;

if (!(length = truncate_string (string)))
    return NULL;

str = (STR) ALLOCPV (type_str, length + 1);
p = str->str_data;
do *p++ = *string++; while (--length);
*p = 0;

return str->str_data;
}

static SYM make_symbol (
    TEXT	*string,
    SSHORT	length)
{
/**************************************
 *
 *	m a k e _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Make up a symbol from a string.  If the string is all blanks, 
 *	don't make up a symbol.  Phooey.
 *
 **************************************/
SYM	symbol;
TEXT	*p, *end;;

if (!(length = truncate_string (string)))
    return NULL;

length;

symbol = (SYM) ALLOCPV (type_sym, length);
symbol->sym_type = SYM_relation;
symbol->sym_length = length;
symbol->sym_string = p = symbol->sym_name;
do *p++ = *string++; while (--length);

return symbol;
}

static CON missing_value (
    SLONG	*blob_id,
    SYM		symbol)
{
/**************************************
 *
 *	m i s s i n g _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Get a missing value into a constant
 *	block for later reference.  
 *
 **************************************/
SYN	element;

element = parse_blr_blob (blob_id, symbol);
if (element)
    return ((CON) element->syn_arg [0]);
else
    return NULL;
}

static SYN parse_blr (
    UCHAR	**ptr,
    SYM		symbol)
{
/**************************************
 *
 *	p a r s e _ b l r
 *
 **************************************
 *
 * Functional description
 *	Parse a BLR expression.
 *
 **************************************/
SYN	node, *arg;
CON	constant;
NOD_T	operator;
NAM	name;
UCHAR	*p, *blr;
SSHORT	args, dtype, scale, length, l, op;

#if (defined JPN_SJIS || defined JPN_EUC)
USHORT	length_specified_in_blr;
USHORT	scale_specified_in_blr;
#endif


blr = *ptr;
args = 2;
node = NULL;

switch (op = BLR_BYTE)
    {
    case blr_any:
    case blr_unique:
    case blr_eoc:
	return NULL;

    case blr_eql:		operator = nod_eql; break;
    case blr_neq:		operator = nod_neq; break;
    case blr_gtr:		operator = nod_gtr; break;
    case blr_geq:		operator = nod_geq; break;
    case blr_lss:		operator = nod_lss; break;
    case blr_leq:		operator = nod_leq; break;
    case blr_containing:	operator = nod_containing; break;
    case blr_starting:		operator = nod_starts; break;
    case blr_matching:		operator = nod_matches; break;
    case blr_matching2:		operator = nod_sleuth; break;
    case blr_like:		operator = nod_like; break;
    case blr_and:		operator = nod_and; break;
    case blr_or:		operator = nod_or; break;
    case blr_add:		operator = nod_add; break;
    case blr_subtract:		operator = nod_subtract; break;
    case blr_multiply:		operator = nod_multiply; break;
    case blr_divide:		operator = nod_divide; break;
    case blr_upcase:		operator = nod_upcase; break;
    case blr_null:		operator = nod_null; break;

    case blr_between:
	operator = nod_between; 
	args = 3;
	break;

    case blr_not:
	operator = nod_not; 
	args = 1;
	break;

    case blr_negate:
	operator = nod_negate; 
	args = 1;
	break;

    case blr_missing:
	operator = nod_missing;
	args = 1;
	break;

    case blr_fid:
	BLR_BYTE;
	blr += 2;
	node = make_node (nod_field, 1);
	name = (NAM) ALLOCPV (type_nam, symbol->sym_length);
	node->syn_arg [0] = (SYN) name;
	name->nam_length = symbol->sym_length;
	strcpy (name->nam_string, symbol->sym_string);
	break;

    case blr_field:
	BLR_BYTE;
	length = BLR_BYTE;
	node = make_node (nod_field, 1);
	name = (NAM) ALLOCPV (type_nam, length);
	node->syn_arg [0] = (SYN) name;
	name->nam_length = length;
	p = (UCHAR*) name->nam_string;
	do *p++ = BLR_BYTE; while (--length);
	break;

    case blr_function:
	length = BLR_BYTE;
	node = make_node (nod_function, s_fun_count);
	node->syn_arg [s_fun_function] = (SYN) HSH_lookup (blr, length);
	blr += length;
	args = BLR_BYTE;
	node->syn_arg [s_fun_args] = make_node (nod_list, args);
	arg = node->syn_arg [s_fun_args]->syn_arg;
	while (--args >= 0)
	    {
	    if (!(*arg++ = parse_blr (&blr, symbol)))
		return NULL;
	    }
	break;

    case blr_literal:
	scale = 0;
	switch (BLR_BYTE)
	    {
	    case blr_text:
		dtype = dtype_text;
		length = l = gds__vax_integer (blr, 2);
#if (defined JPN_SJIS || defined JPN_EUC)
                scale_specified_in_blr = gds__interp_eng_ascii;
                scale = (UCHAR) (QLI_interp);
#endif
		blr += 2;
		break;

            case blr_text2:
                dtype = dtype_text;
#if !(defined JPN_SJIS || defined JPN_EUC)
                scale = gds__vax_integer (blr, 2);
                blr += 2;
                length = l= gds__vax_integer (blr, 2);
                blr += 2;
#else
                scale = (UCHAR) QLI_interp;
                scale_specified_in_blr = gds__vax_integer (blr, 2);
                blr += 2;
                length_specified_in_blr = gds__vax_integer (blr, 2);
                blr += 2;

                if (QLI_interp == scale_specified_in_blr)
                    length = l = length_specified_in_blr;
                else if (QLI_interp == gds__interp_jpn_euc && 
		     scale_specified_in_blr == gds__interp_jpn_sjis)
                    { 
		    if (KANJI_euc_len (blr,length_specified_in_blr, &length))
			{
			ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); 
			/* Msg258 don't understand BLR operator %d */
			}
                    }
                else if (QLI_interp == gds__interp_jpn_sjis &&
		     scale_specified_in_blr == gds__interp_jpn_euc)
                     {
                     if (KANJI_sjis_len (blr,length_specified_in_blr, &length))
			 {
		         ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); 
			 /* Msg258 don't understand BLR operator %d */
			 }
                     }
		else
		    {
		    ERRQ_print_error(258, (TEXT*) op, NULL, NULL, NULL, NULL);
		    /* Msg258 don't understand BLR operator %d */
		    }
#endif
                break;

	    case blr_varying:
		dtype = dtype_varying;
		length = l = gds__vax_integer (blr, 2) + sizeof (USHORT);
		blr += 2;
		break;

	    case blr_varying2:
		dtype = dtype_varying;
                scale = gds__vax_integer (blr, 2);
                blr += 2;
		length = l = gds__vax_integer (blr, 2) + sizeof (USHORT);
		blr += 2;
		break;

	    case blr_short:
		dtype = dtype_short;
		l = 2;
		length = sizeof (SSHORT);
		scale = (int) BLR_BYTE;
		break;

	    case blr_long:
		dtype = dtype_long;
		length = sizeof (SLONG);
		l = 4;
		scale = (int) BLR_BYTE;
		break;

	    case blr_quad:
		dtype = dtype_quad;
		l = 8;
		length = sizeof (GDS__QUAD);
		scale = (int) BLR_BYTE;
		break;
	    
	    case blr_timestamp:
		dtype = dtype_timestamp;
		l = 8;
		length = sizeof (GDS__QUAD);
		break;
	    
	    case blr_sql_date:
		dtype = dtype_sql_date;
		l = 4;
		length = sizeof (SLONG);
		break;
	    
	    case blr_sql_time:
		dtype = dtype_sql_time;
		l = 4;
		length = sizeof (ULONG);
		break;
	    }
	constant = (CON) ALLOCPV (type_con, length);
	constant->con_desc.dsc_dtype = dtype;
	constant->con_desc.dsc_scale = scale;
	constant->con_desc.dsc_length = length;
	constant->con_desc.dsc_address = p = constant->con_data;
	if (dtype == dtype_text)
#if (!( defined JPN_SJIS || defined JPN_EUC) )
            while (l--) *p++ = BLR_BYTE;
#else
	    {
	    if ((scale_specified_in_blr == gds__interp_eng_ascii) ||
		(QLI_interp == scale_specified_in_blr))
	    	{ 
	    	while (l--) *p++ = BLR_BYTE;
	    	}

	    else if (QLI_interp == gds__interp_jpn_euc && 
		scale_specified_in_blr == gds__interp_jpn_sjis)
                { 
		if(KANJI_sjis2euc(blr,length_specified_in_blr,p,length,&length))
		    {
		    ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); 
		    /* Msg258 don't understand BLR operator %d */
		    }
	    	constant->con_desc.dsc_length = length;
            	blr += length_specified_in_blr;
                }

            else if (QLI_interp == gds__interp_jpn_sjis && 
		scale_specified_in_blr == gds__interp_jpn_euc)
                { 
		if(KANJI_euc2sjis(blr,length_specified_in_blr,p,length,&length))
		    {
		    ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); 
		    /* Msg258 don't understand BLR operator %d */
		    }
	    	constant->con_desc.dsc_length = length;
            	blr += length_specified_in_blr;
                }
	    else
		{
	    	ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); 
		/* Msg258 don't understand BLR operator %d */
		}
            }
#endif
	else if (dtype == dtype_short)
	    {
	    *(SSHORT*) p = gds__vax_integer (blr, l);
	    blr += l;
	    }
	else if (dtype == dtype_long)
	    {
	    *(SLONG*) p = gds__vax_integer (blr, l);
	    blr += l;
	    }
	node = make_node (nod_constant, 1);
	node->syn_count = 0;
	node->syn_arg [0] = (SYN) constant;
	break;

    default:
	ERRQ_print_error (258, (TEXT*) op, NULL, NULL, NULL, NULL); /* Msg258 don't understand BLR operator %d */
    }

if (!node)
    {
    node = make_node (operator, args);
    arg = node->syn_arg;
    while (--args >= 0)
	if (!(*arg++ = parse_blr (&blr, symbol)))
	    return NULL;
    }

*ptr = blr;

return node;
}

static SYN parse_blr_blob (
    SLONG	*blob_id,
    SYM		symbol)
{
/**************************************
 *
 *	p a r s e _ b l r _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Gobble up a BLR blob, if possible.  
 *
 **************************************/
int	*handle;
STATUS	status_vector [20];
SYN	node;
USHORT	length;
UCHAR	buffer [1024], *ptr;

if (!blob_id [0] && !blob_id [1])
    return NULL;

handle = NULL;

if (gds__open_blob (status_vector,
    GDS_REF (DB),
    GDS_REF (gds__trans),
    GDS_REF (handle),
    GDS_VAL (blob_id)))
    return NULL;

ptr = buffer;

for (;;)
    {
    if (!(length = buffer + sizeof (buffer) - ptr))
	break;
    if (gds__get_segment (status_vector,
	    GDS_REF (handle),
	    GDS_REF (length),
	    length,
	    GDS_VAL (ptr)))
	break;
    ptr += length;
    }

if (gds__close_blob (status_vector,
	GDS_REF (handle)))
    return NULL;

if (ptr == buffer)
    return NULL;

ptr = buffer;

if (*ptr++ != blr_version4)
    return NULL;

node = parse_blr (&ptr, symbol);

if (*ptr != blr_eoc)
    return NULL;

return node;
}

static void purge_relation (
    REL		relation)
{
/**************************************
 *
 *	p u r g e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Purge a relation out of internal data structures.
 *
 **************************************/
SYM	symbol;
REL	*ptr;
FLD	field;
DBB	database;

symbol = relation->rel_symbol;
database = relation->rel_database;
HSH_remove (symbol);

for (ptr = &database->dbb_relations; *ptr; ptr = &(*ptr)->rel_next)
    if (*ptr == relation)
	{
	*ptr = relation->rel_next;
	break;
	}

while (field = relation->rel_fields)
    {
    relation->rel_fields = field->fld_next;
    ALLQ_release (field->fld_name);
    if (symbol = field->fld_query_name)
	ALLQ_release (symbol);
    ALLQ_release (field);
    }

ALL_release (relation);
}

static void put_dyn_string (
    RLB    	rlb,
    TEXT	*string)
{
/**************************************
 *
 *	p u t _ d y n _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Move a string to a dynamic DDL command block.
 *
 **************************************/
SSHORT	l;

l = strlen (string);

while ((SSHORT)(rlb->rlb_limit - rlb->rlb_data) <  l)
    rlb = GEN_rlb_extend (rlb);

STUFF_WORD (l);

while (*string)
    {
    STUFF (*string++);
    }
CHECK_RLB (rlb);
}

static void rollback_update (
    DBB		database)
{
/**************************************
 *
 *	 r o l l b a c k _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Roll back  a meta-data update.
 *
 **************************************/
STATUS	alt_vector [20];

if (gds__trans == database->dbb_meta_trans && 
	(database->dbb_capabilities & DBB_cap_multi_trans))
    gds__rollback_transaction (alt_vector,
	GDS_REF (database->dbb_meta_trans));
    /* Note: No message given if rollback fails */

gds__trans = NULL;
}

static void set_capabilities (
    DBB		database)
{
/**************************************
 *
 *	s e t _ c a p a b i l i t i e s
 *
 **************************************
 *
 * Functional description
 *	Probe the database to determine
 *	what metadata capabilities it has.
 *	The table, rfr_table, in the static
 *	data declarations has the relation
 *	and field names, together with the
 *	capabilities bits.
 *
 **************************************/
ULONG	*req;
STATUS	status_vector [20];
RFR_TAB	rel_field_table;

req = NULL;

/* Look for desireable fields in system relations */

for (rel_field_table = rfr_table; rel_field_table->relation; rel_field_table++)
    {
    FOR (REQUEST_HANDLE req) x IN DB.RDB$RELATION_FIELDS 
	    WITH x.RDB$RELATION_NAME = rel_field_table->relation
	    AND x.RDB$FIELD_NAME = rel_field_table->field
	database->dbb_capabilities |= rel_field_table->bit_mask;
    END_FOR
	ON_ERROR
	    ERRQ_database_error (database, gds__status);
	END_ERROR;
    }

if (req)
    if (gds__release_request (gds__status, GDS_REF (req))) 
	ERRQ_database_error (database, gds__status);

}

static DBB setup_update (
    DBB		database)
{
/**************************************
 *
 *	s e t u p _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Get ready to do a meta data update.
 *	If no database was specified, use the
 *	most recently readied one.  
 *
 **************************************/

if (!database)
    database = QLI_databases;

MET_meta_transaction (database, TRUE);

return database;
}

static void sql_grant_revoke (
    SYN		node,
    USHORT	type)
{
/*****************************************************
 *
 *	s q l _ g r a n t _ r e v o k e
 *
 *****************************************************
 *
 *	Build a DYN string for a SQL GRANT or REVOKE statement
 *
 *****************************************************/
STATUS	status_vector [20];
DBB	database;
SYM	symbol;
REL	relation;
NAM	*name, *end, *field, *end_field;
SYN	names, fields;
USHORT	privileges;
TEXT	*relation_name;
USHORT	fld_count; 
RLB	rlb;


privileges = (USHORT) node->syn_arg [s_grant_privileges];
relation = (REL) node->syn_arg [s_grant_relation];
relation->rel_database = database = setup_update (relation->rel_database); 
relation_name = relation->rel_symbol->sym_string;

rlb = NULL;
rlb = CHECK_RLB (rlb);

STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);

/* For each user create a separate grant string */
/*      For each specified field create a separate grant string */

names = node->syn_arg [s_grant_users];
fields = node->syn_arg [s_grant_fields];

for (name = (NAM*) names->syn_arg, end = name + names->syn_count;
     name < end; name++)
    if (fields->syn_count)
	{
	for (field = (NAM*) fields->syn_arg, end_field = field + fields->syn_count;
	     field < end_field; field++)
	    stuff_priv (rlb, type, relation_name, privileges, (*name)->nam_string, (*field)->nam_string);
	}
    else
	stuff_priv (rlb, type, relation_name, privileges, (*name)->nam_string, NULL_PTR);

STUFF (gds__dyn_end);
STUFF (gds__dyn_eoc);

execute_dynamic_ddl (database, rlb);
MET_meta_commit (database);
}

static int truncate_string (
    TEXT	*string)
{
/**************************************
 *
 *	t r u n c a t e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Convert a blank filled string to
 *	a null terminated string without
 *	trailing blanks.  Because some
 *	strings contain embedded blanks
 *	(e.g. query headers & edit strings)
 *	truncate from the back forward.
 *	Return the number of characters found,
 *	not including the terminating null.
 *
 **************************************/
TEXT	*p;

for (p = string; *p; p++)
    ;

for (p--; (p >= string) && (*p == ' '); p--)
    ;

*++p = 0;

return (p - string);
}

static void stuff_priv (
    RLB		rlb,
    USHORT	type,
    TEXT	*relation,
    USHORT	privileges,
    TEXT	*user,
    TEXT	*field)
{
/**************************************
 *
 *	s t u f f _ p r i v
 *
 **************************************
 *
 * Functional description
 *	Stuff a privilege for grant/revoke.
 *
 **************************************/
USHORT	priv_count;

rlb = CHECK_RLB (rlb);
STUFF (type);

/* Stuff privileges first */

priv_count = 0;
if (privileges & PRV_select)
    priv_count++;
if (privileges & PRV_insert)
    priv_count++;
if (privileges & PRV_delete)
    priv_count++;
if (privileges & PRV_update)
    priv_count++;

STUFF_WORD (priv_count);

if (privileges & PRV_select)
    STUFF ('S');

if (privileges & PRV_insert)
    STUFF ('I');

if (privileges & PRV_delete)
    STUFF ('D');

if (privileges & PRV_update)
    STUFF ('U');

STUFF (gds__dyn_rel_name);
STUFF_STRING (relation);

STUFF (gds__dyn_grant_user);
STUFF_STRING (user);

if (field)
    {
    STUFF (gds__dyn_fld_name);
    STUFF_STRING (field);
    }

if (privileges & PRV_grant_option)
    {
    STUFF (gds__dyn_grant_options);
    STUFF_WORD (2);
    STUFF_WORD (TRUE);
    }

STUFF (gds__dyn_end);
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
 *	Extract wal name and log offset
 *
 **************************************/
UCHAR	item;
UCHAR	*d, *p;
SLONG	length;

p = db_info_buffer;
*log = 0;
*part_offset = 0;
cur_log [0] = 0;

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
