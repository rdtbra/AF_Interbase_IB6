/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		cmd.c
 *	DESCRIPTION:	Data definition compiler
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
#include "../gpre/gpre.h"
#include "../jrd/gds.h"
#include "../jrd/flags.h"
#include "../jrd/constants.h"
#include "../gpre/cmd_proto.h"
#include "../gpre/cme_proto.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/msc_proto.h"

static void	add_cache (REQ, ACT, DBB);
static void	add_log_files (REQ, ACT, DBB);
static void	alter_database (REQ, ACT);
static void	alter_domain (REQ, ACT);
static void	alter_index (REQ, ACT);
static void	alter_table (REQ, ACT);
static void	create_check_constraint (REQ, ACT, CNSTRT);
static void	create_constraint (REQ, ACT, CNSTRT);
static void	create_database (REQ, ACT);
static void	create_database_modify_dyn (REQ, ACT);
static void	create_default_blr (REQ, TEXT*, USHORT);
static void	create_del_cascade_trg (REQ, ACT, CNSTRT);
static void	create_domain (REQ, ACT);
static void	create_domain_constraint (REQ, ACT, CNSTRT);
static void	create_generator (REQ, ACT);
static void	create_index (REQ, IND);
static void	create_matching_blr (REQ, CNSTRT);
static void	create_set_default_trg (REQ, ACT, CNSTRT, BOOLEAN);
static void	create_set_null_trg (REQ, ACT, CNSTRT, BOOLEAN);
static void	create_shadow (REQ, ACT);
static void	create_table (REQ, ACT);
static void	create_trg_firing_cond (REQ, CNSTRT);
static void	create_trigger (REQ, ACT, TRG, FPTR_VOID);
static void	create_upd_cascade_trg (REQ, ACT, CNSTRT);
static BOOLEAN	create_view (REQ, ACT);
static void	create_view_trigger (REQ, ACT, TRG, NOD, CTX *, NOD);
static void	declare_filter (REQ, ACT);
static void	declare_udf (REQ, ACT);
static void	get_referred_fields (ACT, CNSTRT);
static void	grant_revoke_privileges (REQ, ACT);
static void	init_field_struct (FLD);
static void	put_array_info (REQ, FLD);
static void	put_blr (REQ, USHORT, NOD, FPTR_VOID);
static void	put_computed_blr (REQ, FLD);
static void	put_computed_source (REQ, FLD);
static void	put_cstring (REQ, USHORT, TEXT *);
static void	put_dtype (REQ, FLD);
static void	put_field_attributes (REQ, FLD);
static void	put_numeric (REQ, USHORT, SSHORT);
static void	put_short_cstring (REQ, USHORT, TEXT *);
static void	put_string (REQ, USHORT, TEXT *, USHORT);
static void	put_symbol (REQ, int, SYM);
static void	put_trigger_blr (REQ, USHORT, NOD, FPTR_VOID);
static void	put_view_trigger_blr (REQ, REL, USHORT, TRG, NOD, CTX *, NOD);
static void	replace_field_names (NOD, NOD, FLD, SSHORT, CTX *);
static void	set_statistics (REQ, ACT);

#define STUFF(blr)	*request->req_blr++ = (blr) 
#define STUFF_END	STUFF (gds__dyn_end);
#define STUFF_WORD(blr) STUFF (blr); STUFF ((blr) >> 8)
#define STUFF_INT(blr)	STUFF (blr); STUFF ((blr) >> 8); STUFF ((blr) >> 16); STUFF ((blr) >> 24)
#define STUFF_CHECK(n)	if (request->req_base - request->req_blr + request->req_length <= (n) + 50)\
			CMP_check (request,(n))
#define MAX_TPB		4000
#define STANDARD_TPB	4
#define BLOB_BUFFER_SIZE   4096   /* to read in blr blob for default values */

CMD_compile_ddl (
    register REQ	request)
{
/**************************************
 *
 *	C M D _ c o m p i l e _ d d l
 *
 **************************************
 *
 * Functional description
 *	Compile a single request, but do not generate any text.
 *	Generate port blocks, assign parameter numbers, message
 *	numbers, and internal idents.  Compute length of request.
 *
 **************************************/
ACT	action;
IND	index;
REL	relation;

/* Initialize the blr string */

action = request->req_actions;
request->req_blr = request->req_base = ALLOC (250);
request->req_length = 250;
request->req_flags |= REQ_exp_hand;
STUFF (gds__dyn_version_1);
STUFF (gds__dyn_begin);

switch (action->act_type)
    {
    case ACT_alter_table:
	alter_table (request, action);
	break;

    case ACT_alter_database:
	alter_database (request, action);
	break;

    case ACT_alter_domain:
	alter_domain (request, action);
	break;

    case ACT_alter_index:
	alter_index (request, action);
	break;

    case ACT_create_database :
	if (!(request->req_flags & REQ_sql_database_dyn))
	    {
	    request->req_blr = request->req_base;
	    create_database (request, action);
	    return 0;
	    }
	else 
	    create_database_modify_dyn (request, action);
	break;


    case ACT_create_domain:
	create_domain (request, action);
	break;

    case ACT_create_generator:
	create_generator (request, action);
	break;

    case ACT_create_index :
	index = (IND) action->act_object;
	create_index (request, index);
	break;

    case ACT_create_shadow:
	create_shadow (request, action);
	break;

    case ACT_create_table :
	create_table (request, action);
	break;

    case ACT_create_view :
	if (!create_view (request, action))
	    return -1;
	break;

    case ACT_drop_database :
	break;

    case ACT_drop_domain:
	put_cstring (request, gds__dyn_delete_global_fld, action->act_object);
	STUFF_END;
	break;

    case ACT_drop_filter:
	put_cstring (request, gds__dyn_delete_filter, action->act_object);
	STUFF_END;
	break;

    case ACT_drop_index :
	index = (IND) action->act_object;
	put_symbol (request, gds__dyn_delete_idx, index->ind_symbol);
	STUFF_END;
 	break;

    case ACT_drop_shadow:
	put_numeric (request, gds__dyn_delete_shadow,(SSHORT) action->act_object);
	STUFF_END;
	break;

    case ACT_drop_table :
    case ACT_drop_view :
	relation = (REL) action->act_object;
	put_symbol (request, gds__dyn_delete_rel, relation->rel_symbol);
	STUFF_END;
	break;

    case ACT_drop_udf:
	put_cstring (request, gds__dyn_delete_function, action->act_object);
	STUFF_END;
	break;

    case ACT_dyn_grant:
	grant_revoke_privileges (request, action);
	break;

    case ACT_dyn_revoke:
	grant_revoke_privileges (request, action);
	break;

    case ACT_declare_filter:
	declare_filter (request, action);
	break;

    case ACT_declare_udf:
	declare_udf (request, action);
	break;

    case ACT_statistics:
	set_statistics (request, action);
	break;

    default :
	CPR_error ("*** unknown request type node ***");
	return -1;
    }

STUFF_END;
STUFF (gds__dyn_eoc);
request->req_length = request->req_blr - request->req_base; 
request->req_blr = request->req_base;

return 0;
}

static void add_cache (
    REQ		request,
    ACT		action,
    DBB		dbb)
{
/**************************************
 *
 *	a d d _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Add cache file to a database.
 *
 **************************************/
FIL	file;
TEXT	file_name [254];
SSHORT	l;

file = dbb->dbb_cache_file;
l = MIN ((strlen (file->fil_name)), (sizeof (file_name) - 1));

strncpy (file_name, file->fil_name, l);
file_name [l] = '\0';
put_cstring (request, gds__dyn_def_cache_file, file_name);
STUFF (gds__dyn_file_length);
STUFF_WORD (4);
STUFF_INT (file->fil_length);
STUFF_END;
}

static void add_log_files (
    REQ		request,
    ACT		action,
    DBB		db)
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

for (file = db->dbb_logfiles, db->dbb_logfiles = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = db->dbb_logfiles;
    db->dbb_logfiles = file;
    }

number = 0;

/* generate values for default log */

if (db->dbb_flags & DBB_log_default)
    STUFF (gds__dyn_def_default_log);
	
for (file = db->dbb_logfiles; file; file = file->fil_next)
    {
    put_cstring (request, gds__dyn_def_log_file, file->fil_name);
    STUFF (gds__dyn_file_length);
    STUFF_WORD (4);
    STUFF_INT (file->fil_length);
    STUFF (gds__dyn_log_file_sequence);
    STUFF_WORD (2);
    STUFF_WORD (number);
    number++;
    STUFF (gds__dyn_log_file_partitions);
    STUFF_WORD (2);
    STUFF_WORD (file->fil_partitions);
    if (file->fil_flags & FIL_raw)
	STUFF (gds__dyn_log_file_raw);
    if (db->dbb_flags & DBB_log_serial)
	STUFF (gds__dyn_log_file_serial);
    STUFF_END;
    }

if (file = db->dbb_overflow)
    {
    put_cstring (request, gds__dyn_def_log_file, file->fil_name);
    STUFF (gds__dyn_file_length);
    STUFF_WORD (4);
    STUFF_INT (file->fil_length);
    STUFF (gds__dyn_log_file_sequence);
    STUFF_WORD (2);
    STUFF_WORD (number);
    number++;
    STUFF (gds__dyn_log_file_partitions);
    STUFF_WORD (2);
    STUFF_WORD (file->fil_partitions);
    STUFF (gds__dyn_log_file_serial);
    STUFF (gds__dyn_log_file_overflow);
    STUFF_END;	
    }
}

static void alter_database (
    REQ	request,
    ACT action)
{
/********************************************
 *
 *	a l t e r _ d a t a b a s e
 *
 ********************************************
 *
 * Functional description
 *	Generate dynamic DDL for modifying database.
 *
 ********************************************/
DBB	db;
FIL	files, file, next;
SLONG	start;

db = (DBB) action->act_object;

STUFF (gds__dyn_mod_database);

/* Reverse the order of files (parser left them backwards) */

files = db->dbb_files;
for (file = files, files = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = files;
    files = file;
    }

for (file = files; file != NULL; file=file->fil_next)
    {
    put_cstring (request, gds__dyn_def_file, file->fil_name);
    STUFF (gds__dyn_file_start);
    STUFF_WORD (4);
    start = file->fil_start;
    STUFF_INT (start);
    STUFF (gds__dyn_file_length);
    STUFF_WORD (4);
    STUFF_INT (file->fil_length);
    STUFF_END;
    }

/* Drop log and cache */

if (db->dbb_flags & DBB_drop_log)
    STUFF (gds__dyn_drop_log);
if (db->dbb_flags & DBB_drop_cache)
    STUFF (gds__dyn_drop_cache);

/**** Add log files *****/
	
if ((db->dbb_flags & DBB_log_default) || (db->dbb_logfiles))
    add_log_files (request, action, db);

if (db->dbb_cache_file)
    add_cache (request, action, db);

if (db->dbb_chkptlen)
    {
    STUFF (gds__dyn_log_check_point_length);
    STUFF_WORD (4);
    STUFF_INT (db->dbb_chkptlen);
    }
if (db->dbb_numbufs)
    {
    STUFF (gds__dyn_log_num_of_buffers);
    STUFF_WORD (2);
    STUFF_WORD (db->dbb_numbufs);
    }

if (db->dbb_bufsize)
    {
    STUFF (gds__dyn_log_buffer_size);
    STUFF_WORD (2);
    STUFF_WORD (db->dbb_bufsize);
    }


if (db->dbb_grp_cmt_wait != -1)
    {
    STUFF (gds__dyn_log_group_commit_wait);
    STUFF_WORD (4);
    STUFF_INT (db->dbb_grp_cmt_wait);
    }

if (db->dbb_def_charset)
   put_cstring (request, gds__dyn_fld_character_set_name, db->dbb_def_charset);

STUFF_END;
}

static void alter_domain (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	a l t e r _ d o m a i n 
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for CREATE DOMAIN action.
 *
 **************************************/
FLD	field;
TEXT    *default_source;
NOD     default_node;
CNSTRT  cnstrt;

field = (FLD) action->act_object;

/* modify field info */

put_symbol (request, gds__dyn_mod_global_fld, field->fld_symbol);

if (default_node = field->fld_default_value)
    {
    if (default_node->nod_type == nod_erase)
	{
	STUFF (gds__dyn_del_default);
	}
    else
	{
	put_blr (request, gds__dyn_fld_default_value, field->fld_default_value, CME_expr);
	default_source = (TEXT*) ALLOC (field->fld_default_source->txt_length + 1);
	CPR_get_text (default_source, field->fld_default_source);
	put_cstring (request, gds__dyn_fld_default_source, default_source); 
	}
    } 

if (field->fld_constraints)
    {
    STUFF (gds__dyn_single_validation);
    for (cnstrt = field->fld_constraints; cnstrt; cnstrt = cnstrt->cnstrt_next)
	if (cnstrt->cnstrt_flags & CNSTRT_delete)
	   STUFF (gds__dyn_del_validation); 
    create_domain_constraint (request, action, field->fld_constraints);
    }

STUFF_END;
}

static void alter_index (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	a l t e r _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for ALTER INDEX statement
 *
 **************************************/
IND	index;
SSHORT	value;

index = (IND) action->act_object;
put_symbol (request, gds__dyn_mod_idx, index->ind_symbol);

value = (index->ind_flags & IND_active) ? 0 : 1;
put_numeric (request, gds__dyn_idx_inactive, value);

STUFF_END;
}

static void alter_table (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	a l t e r _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for ALTER TABLE action.
 *
 **************************************/
FLD	field;
REL	relation;
CNSTRT  cnstrt;
TEXT    *default_source;

/* add relation name */

relation = (REL) action->act_object;
put_symbol (request, gds__dyn_mod_rel, relation->rel_symbol);

/* add field info */

for (field = relation->rel_fields; field; field = field->fld_next)
    {
    if (field->fld_flags & FLD_delete)
	{
	put_symbol (request, gds__dyn_delete_local_fld, field->fld_symbol);
	STUFF_END;
	}
    else if (field->fld_flags & FLD_meta)
	{
	if (field->fld_global)
	    {
	    put_symbol (request, gds__dyn_def_local_fld, field->fld_symbol);
	    put_symbol (request, gds__dyn_fld_source, field->fld_global);
	    }
	else
	    put_symbol (request, gds__dyn_def_sql_fld, field->fld_symbol);

	put_field_attributes (request, field);

	if (field->fld_default_value)
	    {
	    put_blr (request, gds__dyn_fld_default_value, 
			field->fld_default_value, CME_expr);
	    default_source = (TEXT *) 
			ALLOC (field->fld_default_source->txt_length + 1);
	    CPR_get_text (default_source, field->fld_default_source);
	    put_cstring (request, gds__dyn_fld_default_source, default_source);
	    } 

	STUFF_END;

	/* Any constraints defined for field being added  ?  */

	if (field->fld_constraints)
	    create_constraint (request, action, field->fld_constraints);
	}
    }

/* Check for any relation level ADD/DROP of constraints */

if (relation->rel_constraints)
    {
    for (cnstrt = relation->rel_constraints; cnstrt; cnstrt = cnstrt->cnstrt_next)
	if (cnstrt->cnstrt_flags & CNSTRT_delete)
	    put_cstring (request,gds__dyn_delete_rel_constraint,cnstrt->cnstrt_name);
    create_constraint (request, action, relation->rel_constraints);
    }

STUFF_END;
}

static void create_check_constraint (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt)
{
/**************************************
 *
 *	c r e a t e _ c h e c k _ c o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for creating a CHECK constraint.
 *
 **************************************/
TRG	trigger;

trigger = (TRG) ALLOC (TRG_LEN);

/* create the INSERT trigger */

trigger->trg_type = PRE_STORE_TRIGGER;

/* "insert violates CHECK constraint on table" */

trigger->trg_message = (STR) NULL;
trigger->trg_boolean = cnstrt->cnstrt_boolean;
trigger->trg_source = (STR) ALLOC (cnstrt->cnstrt_text->txt_length+1);
CPR_get_text (trigger->trg_source, cnstrt->cnstrt_text);
create_trigger (request, action, trigger, CME_expr);

/* create the UPDATE trigger   */

trigger->trg_type = PRE_MODIFY_TRIGGER;

/* "update violates CHECK constraint on table" */

create_trigger (request, action, trigger, CME_expr);
}

static void create_trg_firing_cond (
    REQ		request,
    CNSTRT	cnstrt
    )
{
/*****************************************************
 *
 *	c r e a t e _ t r g _ f i r i n g _ c o n d
 *
 *****************************************************
 *
 * Function
 *	Generate blr to express: if (old.primary_key != new.primary_key).
 *	do a column by column comparison.
 *
 **************************************/
LLS	prim_key_fld, field;
USHORT	num_flds = 0, prim_key_num_flds = 0;
STR	prim_key_fld_name;

/* count primary key columns */
field = prim_key_fld = cnstrt->cnstrt_referred_fields;

assert(field != NULL);

while (field)
    {
    prim_key_num_flds++;
    field = field->lls_next;
    }

assert(prim_key_num_flds > 0)

/* generate blr */
STUFF (blr_if);
if (prim_key_num_flds > 1)
   STUFF (blr_or);

for (; prim_key_fld; prim_key_fld = prim_key_fld->lls_next)
    {
    STUFF (blr_neq);

    prim_key_fld_name = (STR) prim_key_fld->lls_object; 

    STUFF (blr_field); STUFF ((SSHORT)0);
    put_cstring (request, 0, prim_key_fld_name);
    STUFF (blr_field); STUFF ((SSHORT)1);
    put_cstring (request, 0, prim_key_fld_name);

    num_flds++;

    if (prim_key_num_flds - num_flds >= 2)
	STUFF (blr_or);
    }
}

static void create_matching_blr (
    REQ		request,
    CNSTRT	cnstrt
    )
{
/********************************************
 *
 *	c r e a t e _ m a t c h i n g _ b l r
 *
 ********************************************
 *
 * Function
 *	Generate blr to express: foreign_key == primary_key
 *	ie.,  for_key.column_1 = prim_key.column_1 and
 *	for_key.column_2 = prim_key.column_2 and ....  so on..
 *
 **************************************/
LLS	field, for_key_fld, prim_key_fld;
USHORT	for_key_num_flds = 0, prim_key_num_flds = 0, num_flds = 0;
STR	for_key_fld_name, prim_key_fld_name;

/* count primary key columns */
field = prim_key_fld = cnstrt->cnstrt_referred_fields;

assert(field != NULL);

while (field)
    {
    prim_key_num_flds++;
    field = field->lls_next;
    }

assert(prim_key_num_flds > 0)

/* count of foreign key columns */
field = for_key_fld = cnstrt->cnstrt_fields;

assert(field != NULL);

while (field)
    {
    for_key_num_flds++;
    field = field->lls_next;
    }

assert(for_key_num_flds > 0)
assert (prim_key_num_flds == for_key_num_flds);

STUFF (blr_boolean);
if (prim_key_num_flds > 1)
    STUFF (blr_and);

num_flds = 0;

do  {
    STUFF (blr_eql);

    for_key_fld_name = (STR) for_key_fld->lls_object;
    prim_key_fld_name = (STR) prim_key_fld->lls_object;

    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name);
    STUFF (blr_field); STUFF ((SSHORT)0);
    put_cstring (request, 0, prim_key_fld_name);

    num_flds++;

    if (prim_key_num_flds - num_flds >= 2)
        STUFF (blr_and);

    for_key_fld = for_key_fld->lls_next;
    prim_key_fld = prim_key_fld->lls_next;
    }
while (num_flds < for_key_num_flds);

STUFF (blr_end);
}

static void create_default_blr (
    REQ         request,
    TEXT        *default_buff,
    USHORT      buff_size
    )
{
/********************************************
 *
 *	c r e a t e _ d e f a u l t _ b l r
 *
 ********************************************
 * Function:
 *	The default_blr is passed in default_buffer. It is of the form:
 *	blr_version4 blr_literal ..... blr_eoc.
 *	strip the blr_version4/blr_version5 and blr_eoc verbs and stuff the remaining
 *	blr in the blr stream in the request.
 *
 *********************************************/
int	i;

assert (*default_buff == blr_version4 || *default_buff == blr_version5);
  
for (i = 1;
    ( (i< buff_size) && (default_buff[i] != blr_eoc) );
    i++)
   STUFF (default_buff[i]);

assert (default_buff[i] == blr_eoc);

}

static void create_upd_cascade_trg (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt)
{
/**************************************
 *
 *	c r e a t e _ u p d _ c a s c a d e _ t r g
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for "on update cascade" trigger (for referential
 *	integrity) along with the trigger blr.
 *
 **************************************/

LLS	for_key_fld, prim_key_fld;
STR	for_key_fld_name,  prim_key_fld_name;
USHORT	offset, length;
REL	relation;

for_key_fld = cnstrt->cnstrt_fields;
prim_key_fld = cnstrt->cnstrt_referred_fields;
relation = (REL) action->act_object;

/* no trigger name is generated here. Let the engine make one up */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_numeric (request, gds__dyn_trg_type, (SSHORT) POST_MODIFY_TRIGGER);

STUFF (gds__dyn_sql_object);
put_numeric (request, gds__dyn_trg_sequence, (SSHORT)1);
put_numeric (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, cnstrt->cnstrt_referred_rel);

/* the trigger blr */

STUFF (gds__dyn_trg_blr);
offset = request->req_blr - request->req_base;

STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);

create_trg_firing_cond (request, cnstrt);

STUFF (blr_begin);
STUFF (blr_begin);

STUFF (blr_for);
STUFF (blr_rse);

/* the new context for the prim. key relation */
STUFF (1);

STUFF (blr_relation);
put_cstring (request, 0, relation->rel_symbol->sym_string);
/* the context for the foreign key relation */
STUFF (2);

/* generate the blr for: foreign_key == primary_key */
create_matching_blr (request, cnstrt);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

for (; for_key_fld && prim_key_fld; 
       for_key_fld =for_key_fld->lls_next, prim_key_fld =prim_key_fld->lls_next)
    {
    for_key_fld_name = (STR) for_key_fld->lls_object;
    prim_key_fld_name = (STR) prim_key_fld->lls_object;

    STUFF (blr_assignment);
    STUFF (blr_field); STUFF ((SSHORT)1);
    put_cstring (request, 0, prim_key_fld_name);
    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name);
    }

STUFF (blr_end);
STUFF (blr_end); STUFF (blr_end); STUFF (blr_end);

STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);
}

static void create_del_cascade_trg (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt)
{
/**************************************
 *
 *	c r e a t e _ d e l _ c a s c a d e _ t r g
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for "on delete cascade" trigger (for referential
 *	integrity) along with the trigger blr.
 *
 **************************************/
REL	relation;
USHORT	offset, length;

relation = (REL) action->act_object;

/* stuff a trigger_name of size 0. So the dyn-parser will make one up.  */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_numeric (request, gds__dyn_trg_type, (SSHORT) POST_ERASE_TRIGGER);

STUFF (gds__dyn_sql_object);
put_numeric (request, gds__dyn_trg_sequence, (SSHORT)1);
put_numeric (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, cnstrt->cnstrt_referred_rel);

/* the trigger blr */
STUFF (gds__dyn_trg_blr);
offset = request->req_blr - request->req_base;

STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);

STUFF (blr_for);
STUFF (blr_rse);

/* the context for the prim. key relation */
STUFF (1);

STUFF(blr_relation);
put_cstring (request, 0, relation->rel_symbol->sym_string);
/* the context for the foreign key relation */
STUFF (2);

create_matching_blr(request, cnstrt);

STUFF (blr_erase); STUFF ((SSHORT)2);
STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);

}

static void create_set_default_trg (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt,
    BOOLEAN	on_upd_trg)
{
/**************************************
 *
 *	c r e a t e _ s e t _ d e f a u l t _ t r g
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for "on delete|update set default" trigger (for 
 *	referential integrity) along with the trigger blr.
 *	The non_upd_trg parameter == TRUE is an update trigger.
 *
 **************************************/

BOOLEAN	search_for_default, search_for_column;
TEXT	*search_for_domain;
FLD	field, domain, fld;
LLS	for_key_fld;
STR	for_key_fld_name, domain_name;
USHORT	offset, length;
REL	relation, rel;
REQ	req, tmp_req;
ACT	act;
TEXT	s[512];
UCHAR	default_val[BLOB_BUFFER_SIZE];

assert (request->req_actions->act_type == ACT_create_table ||
	request->req_actions->act_type == ACT_alter_table);

for_key_fld = cnstrt->cnstrt_fields;
relation = (REL) action->act_object;

/* no trigger name. It is generated by the engine */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_numeric (request, gds__dyn_trg_type,
     on_upd_trg ? (SSHORT)POST_MODIFY_TRIGGER : (SSHORT)POST_ERASE_TRIGGER);

STUFF (gds__dyn_sql_object);
put_numeric (request, gds__dyn_trg_sequence, (SSHORT)1);
put_numeric (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, cnstrt->cnstrt_referred_rel);

/* the trigger blr */

STUFF (gds__dyn_trg_blr);
offset = request->req_blr - request->req_base;

STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);

/* for ON UPDATE TRIGGER only: generate the trigger firing condition:
   if prim_key.old_value != prim_key.new value.
   Note that the key could consist of multiple columns */

if (on_upd_trg)
    {
    create_trg_firing_cond (request, cnstrt);
    STUFF (blr_begin);
    STUFF (blr_begin);
    }

STUFF (blr_for);
STUFF (blr_rse);

/* the context for the prim. key relation */
STUFF (1);

STUFF (blr_relation);
put_cstring (request, 0, relation->rel_symbol->sym_string);
/* the context for the foreign key relation */
STUFF (2);

create_matching_blr (request, cnstrt);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

for (; for_key_fld;
       for_key_fld =for_key_fld->lls_next)
    {
    /* for every column in the foreign key .... */
    for_key_fld_name = (STR) for_key_fld->lls_object;

    STUFF (blr_assignment);

    /* here stuff the default value as blr_literal .... or blr_null
       if this column does not have an applicable default */

    /* the default is determined in many cases:
	(1) The default-info for the column is in memory. (This is because
	    the column is being created in this application)

	    (1-a) the table has a column level default. We get this by
		  searching both current ddl statement and requests
		  linked list.
	    (1-b) the table does not have a column level default, but
		  has a domain default. We get this by searching
		  requests linked list.
	(2) The default-info for the column is not in memory.
            We get the column and/or domain default value from the system
	    tables by calling MET_get_column_default/MET_get_domain_defult */

    search_for_default = TRUE;
    search_for_column = FALSE;
    search_for_domain = NULL;

    /* Is the column being created in this ddl statement ? */
    for (field = relation->rel_fields; field; field = field->fld_next)
	{
	if (strcmp (field->fld_symbol->sym_string, for_key_fld_name) == 0)
	    break;
	}

    if (field)
	{
	/* Yes. The column is being created in this ddl statement */
	if (field->fld_default_value)
	    {
	    /* (1-a) */
	    CME_expr (field->fld_default_value, request);
	    search_for_default = FALSE;
	    }
	else
	    {
	    /* check for domain default */
	    if (field->fld_global)
		{
		/* could be either (1-b) or (2) */
		search_for_domain = field->fld_global->sym_string;
		}
	    else
		{
		STUFF (blr_null);
		search_for_default = FALSE;
		}
	    }
	}
    else
	{
	/* Nop. The column is not being created in this ddl statement */
	if (request->req_actions->act_type == ACT_create_table)
	    {
	    sprintf (s, "field \"%s\" does not exist in relation \"%s\"",
			for_key_fld_name, relation->rel_symbol->sym_string);
	    CPR_error (s);
	    }

	/* Thus we have an ALTER TABLE statement. */

	/* If somebody is 'clever' enough to create table and then to alter it
	   within the same application ... */
	for (req=requests; req; req=req->req_next)
	    {
	    if ( (req->req_type == REQ_ddl) &&
		 (act = req->req_actions) &&
		 (act->act_type == ACT_create_table ||
		    act->act_type == ACT_alter_table) &&
		 (rel = (REL) act->act_object) &&
		 (strcmp (rel->rel_symbol->sym_string,
			  relation->rel_symbol->sym_string) == 0))
		{
		/* ... then try to check for the default in memory */
		for ( fld = (FLD) rel->rel_fields;
		      fld; 
		      fld = fld->fld_next )
		    {
		    if (strcmp (fld->fld_symbol->sym_string, 
				for_key_fld_name) != 0)
			continue;
		    if (fld->fld_default_value)
			{
			/* case (1-a): */
			CME_expr (fld->fld_default_value, request);
			search_for_default = FALSE;
			}
		    else
			{
			if (fld->fld_global)
			    {
			    search_for_domain = fld->fld_global->sym_string;
			    }
			else
			    {
			    /* default not found */
			    STUFF (blr_null);
			    search_for_default = FALSE;
			    }
			}
		    break;
		    }
		if (fld)
		    break;
		}
	    }
	if (!req)
	    search_for_column = TRUE;
	}

    if (search_for_default && search_for_domain != NULL)
	{
	/* search for domain level default */
	assert (search_for_column == FALSE);
	/* search for domain in memory */
	for (req=requests; req; req=req->req_next)
	    {
	    if ( (req->req_type == REQ_ddl) &&
		 (act = req->req_actions) &&
		 ( (act->act_type == ACT_create_domain) ||
		   (act->act_type == ACT_alter_domain) ) &&
		 (domain = (FLD) act->act_object) &&
		 (strcmp (search_for_domain,
			  domain->fld_symbol->sym_string) == 0) &&
		 (domain->fld_default_value->nod_type != nod_erase) )
		{
		/* domain found in memory */
		if (domain->fld_default_value)
		    {
		    /* case (1-b) */
		    CME_expr (domain->fld_default_value, request);
		    }
		else
		    {
		    /* default not found */
		    STUFF (blr_null);
		    }
		search_for_default = FALSE;
		break;
		}
	    }

	if (search_for_default)
	    {
	    /* search for domain in db system tables */
	    if (MET_get_domain_default (relation->rel_database,
					search_for_domain,
				        default_val, sizeof(default_val)))
		{
		create_default_blr (request, default_val, sizeof(default_val));
		}
	    else
		{
		STUFF (blr_null);
		}
	    search_for_default = FALSE;
	    }
	} /* end of search for domain level default */

    if (search_for_default && search_for_column)
	{
	/* nothing is found in memory, try to check db system tables */
	assert (search_for_domain == NULL);
	if (MET_get_column_default (relation, for_key_fld_name, 
		default_val, sizeof(default_val)) != NULL)
	    {
	    create_default_blr (request, default_val, sizeof(default_val));
	    }
	else
	    {
	    STUFF (blr_null);
	    }
	}

    /* the context for the foreign key relation */
    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name);

    }
STUFF (blr_end);

if (on_upd_trg)
    {
    STUFF (blr_end);
    STUFF (blr_end);
    STUFF (blr_end);
    }

STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);

}

static void create_set_null_trg (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt,
    BOOLEAN	on_upd_trg)
{
/**************************************
 *
 *	c r e a t e _ s e t _ n u l l _ t r g
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for "on delete|update set null" trigger (for 
 *	referential integrity). The trigger blr is the same for
 *	both the delete and update cases. Only differences is its
 *	TRIGGER_TYPE (ON DELETE or ON UPDATE).
 *	The non_upd_trg parameter == TRUE is an update trigger.
 *
 **************************************/

LLS	for_key_fld;
STR	for_key_fld_name;
USHORT	offset, length;
REL	relation;

for_key_fld = cnstrt->cnstrt_fields;
relation = (REL) action->act_object;

/* no trigger name. It is generated by the engine */
put_string (request, gds__dyn_def_trigger, "", (USHORT)0);

put_numeric (request, gds__dyn_trg_type,
     on_upd_trg ? (SSHORT)POST_MODIFY_TRIGGER : (SSHORT)POST_ERASE_TRIGGER);

STUFF (gds__dyn_sql_object);
put_numeric (request, gds__dyn_trg_sequence, (SSHORT)1);
put_numeric (request, gds__dyn_trg_inactive, (SSHORT)0);
put_cstring (request, gds__dyn_rel_name, cnstrt->cnstrt_referred_rel);

/* the trigger blr */

STUFF (gds__dyn_trg_blr);
offset = request->req_blr - request->req_base;

STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);

/* for ON UPDATE TRIGGER only: generate the trigger firing condition:
   if prim_key.old_value != prim_key.new value.
   Note that the key could consist of multiple columns */

if (on_upd_trg)
    {
    create_trg_firing_cond (request, cnstrt);
    STUFF (blr_begin);
    STUFF (blr_begin);
    }

STUFF (blr_for);
STUFF (blr_rse);

/* the context for the prim. key relation */
STUFF (1);

STUFF (blr_relation);
put_cstring (request, 0, relation->rel_symbol->sym_string);
/* the context for the foreign key relation */
STUFF (2);

create_matching_blr (request, cnstrt);

STUFF (blr_modify); STUFF ((SSHORT)2); STUFF ((SSHORT)2);
STUFF (blr_begin);

for (; for_key_fld;
       for_key_fld =for_key_fld->lls_next)
    {
    for_key_fld_name = (STR) for_key_fld->lls_object;

    STUFF (blr_assignment);
    STUFF (blr_null);
    STUFF (blr_field); STUFF ((SSHORT)2);
    put_cstring (request, 0, for_key_fld_name);
    }
STUFF (blr_end);

if (on_upd_trg)
    {
    STUFF (blr_end);
    STUFF (blr_end);
    STUFF (blr_end);
    }

STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
/* end of the blr */

/* no trg_source and no trg_description */
STUFF (gds__dyn_end);
}

static void get_referred_fields (
    ACT		action,
    CNSTRT	cnstrt)
{
/************************************************
 *
 *	g e t _ r e f e r r e d _ f i e l d s
 *
 ************************************************
 *
 * Functional description
 *	Get referred fields from memory/system tables
 *
 ************************************************/
REQ	req;
REL	rel, relation;
ACT	act;
CNSTRT	cns;
TEXT	s[512];
LLS	field;
USHORT	prim_key_num_flds = 0, for_key_num_flds = 0;

relation = (REL) action->act_object;

for (req=requests; req; req=req->req_next)
    {
    if ( (req->req_type == REQ_ddl) &&
	 (act = req->req_actions) &&
	 (act->act_type == ACT_create_table ||
	    act->act_type == ACT_alter_table) &&
	 (rel = (REL) act->act_object) &&
	 (strcmp (rel->rel_symbol->sym_string,
		  cnstrt->cnstrt_referred_rel) == 0) )
	{
	for (cns = rel->rel_constraints; cns; cns = cns->cnstrt_next)
	    if (cns->cnstrt_type == CNSTRT_PRIMARY_KEY)
		{
		cnstrt->cnstrt_referred_fields = cns->cnstrt_fields;
		break;
		}
	if (!cnstrt->cnstrt_referred_fields && rel->rel_fields)
	    for (cns = rel->rel_fields->fld_constraints; cns; cns = cns->cnstrt_next)
		if (cns->cnstrt_type == CNSTRT_PRIMARY_KEY)
		    {
		    cnstrt->cnstrt_referred_fields = cns->cnstrt_fields;
		    break;
		    }
	if (cnstrt->cnstrt_referred_fields)
	    break;
	}
    }

if (cnstrt->cnstrt_referred_fields == NULL)
    /* Nothing is in memory. Try to find in system tables */
    cnstrt->cnstrt_referred_fields = MET_get_primary_key (
						relation->rel_database,
						cnstrt->cnstrt_referred_rel);

if (cnstrt->cnstrt_referred_fields == NULL)
    {
    /* Nothing is in system tables. */
    sprintf(s, "\"REFERENCES %s\" without \"(column list)\" requires PRIMARY KEY on referenced table",
		cnstrt->cnstrt_referred_rel);
    CPR_error (s);
    }
else
    {
    /* count both primary key and foreign key columns */
    field = cnstrt->cnstrt_referred_fields;
    while (field)
	{
	prim_key_num_flds++;
	field = field->lls_next;
	}
    field = cnstrt->cnstrt_fields;
    while (field)
	{
	for_key_num_flds++;
	field = field->lls_next;
	}
    if (prim_key_num_flds != for_key_num_flds)
	{
	sprintf(s, "PRIMARY KEY column count in relation \"%s\" does not match FOREIGN KEY in relation \"%s\"",
		   cnstrt->cnstrt_referred_rel,
		   relation->rel_symbol->sym_string);
	CPR_error (s);
	}
    }
}

static void create_constraint (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt)
{
/**************************************
 *
 *	c r e a t e _ c o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for creating a constraint.
 *
 **************************************/
LLS	field;
STR	string;

for (; cnstrt; cnstrt = cnstrt->cnstrt_next)
    {
    if (cnstrt->cnstrt_flags & CNSTRT_delete)
	continue;
    put_cstring (request,gds__dyn_rel_constraint,cnstrt->cnstrt_name);

    if (cnstrt->cnstrt_type ==  CNSTRT_PRIMARY_KEY)
	STUFF (gds__dyn_def_primary_key);
    else if (cnstrt->cnstrt_type == CNSTRT_UNIQUE)
	STUFF (gds__dyn_def_unique);
    else if (cnstrt->cnstrt_type == CNSTRT_FOREIGN_KEY)
	STUFF (gds__dyn_def_foreign_key); 
    else if (cnstrt->cnstrt_type == CNSTRT_NOT_NULL)
	{
	STUFF (gds__dyn_fld_not_null);
	STUFF_END;
	continue;
	}
    else if (cnstrt->cnstrt_type == CNSTRT_CHECK)
	{
	create_check_constraint (request, action, cnstrt);
	STUFF_END;
	continue;
	}

    /* stuff a zero-length name, indicating that an index
       name should be generated */

    STUFF_WORD (0);   

    if (cnstrt->cnstrt_type == CNSTRT_FOREIGN_KEY)
	{
	/* If <referenced column list> is not specified try to catch
	   them right here */
	if (cnstrt->cnstrt_referred_fields == NULL)
	    get_referred_fields (action, cnstrt);

	if (cnstrt->cnstrt_fkey_def_type & REF_UPDATE_ACTION)
	    {
	    STUFF (gds__dyn_foreign_key_update);
	    switch (cnstrt->cnstrt_fkey_def_type & REF_UPDATE_MASK)
		{
		case REF_UPD_NONE:
		    STUFF (gds__dyn_foreign_key_none);
		    break;
		case REF_UPD_CASCADE:
		    STUFF (gds__dyn_foreign_key_cascade);
		    create_upd_cascade_trg(request, action, cnstrt);
		    break;
		case REF_UPD_SET_DEFAULT:
		    STUFF (gds__dyn_foreign_key_default);
		    create_set_default_trg(request, action, cnstrt, TRUE);
		    break;
		case REF_UPD_SET_NULL:
		    STUFF (gds__dyn_foreign_key_null);
		    create_set_null_trg(request, action, cnstrt, TRUE);
		    break;
		default:
		    /* just in case */
		    assert(0);
		    STUFF (gds__dyn_foreign_key_none);
		    break;
		}
	    }
	if (cnstrt->cnstrt_fkey_def_type & REF_DELETE_ACTION)
	    {
	    STUFF (gds__dyn_foreign_key_delete);
	    switch (cnstrt->cnstrt_fkey_def_type & REF_DELETE_MASK)
		{
		case REF_DEL_NONE:
		    STUFF (gds__dyn_foreign_key_none);
		    break;
		case REF_DEL_CASCADE:
		    STUFF (gds__dyn_foreign_key_cascade);
		    create_del_cascade_trg(request, action, cnstrt);
		    break;
		case REF_DEL_SET_DEFAULT:
		    STUFF (gds__dyn_foreign_key_default);
		    create_set_default_trg(request, action, cnstrt, FALSE);
		    break;
		case REF_DEL_SET_NULL:
		    STUFF (gds__dyn_foreign_key_null);
		    create_set_null_trg(request, action, cnstrt, FALSE);
		    break;
		default:
		    /* just in case */
		    assert(0);
		    STUFF (gds__dyn_foreign_key_none);
		    break;
		}
	    }
	}
    else
	put_numeric (request, gds__dyn_idx_unique, TRUE);

    for (field = cnstrt->cnstrt_fields; field; field = field->lls_next)
	{
	string = (STR) field->lls_object;
	put_cstring (request, gds__dyn_fld_name, string);
	}
    if (cnstrt->cnstrt_type == CNSTRT_FOREIGN_KEY)
	{
	put_cstring (request, gds__dyn_idx_foreign_key,cnstrt->cnstrt_referred_rel);
	for (field = cnstrt->cnstrt_referred_fields; field; field = field->lls_next)
	    {
	    string = (STR) field->lls_object;
	    put_cstring (request, gds__dyn_idx_ref_column, string);
	    }
	}
    STUFF_END;
    }
}

static void create_database (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate parameter buffer for CREATE DATABASE action.
 *
 **************************************/
DBB	db;
SSHORT	l;
SCHAR	*ch;
int	def_db_dial = 3;

db = ((MDBB) action->act_object)->mdbb_database;
STUFF (gds__dpb_version1);
STUFF (isc_dpb_overwrite);
STUFF (1);
STUFF (0);

STUFF (isc_dpb_sql_dialect);
STUFF (sizeof(int));
if ((dialect_specified) && ( sw_sql_dialect == 1 || sw_sql_dialect == 3))
    def_db_dial = sw_sql_dialect;
STUFF_INT (def_db_dial);

if (db->dbb_allocation)
    {
    STUFF (gds__dpb_allocation);
    STUFF (4);
    STUFF_INT (db->dbb_allocation);
    }

if (db->dbb_pagesize)
    {
    STUFF (gds__dpb_page_size);
    STUFF (4);
    STUFF_INT (db->dbb_pagesize);
    }

if (db->dbb_buffercount)
    {
    STUFF (gds__dpb_num_buffers);
    STUFF (4);
    STUFF_INT (db->dbb_buffercount);
    }

if (db->dbb_buffersize)
    {
    STUFF (gds__dpb_buffer_length);
    STUFF (4);
    STUFF_INT (db->dbb_buffersize);
    }

if (db->dbb_users)
    {
    STUFF (gds__dpb_number_of_users);
    STUFF (4);
    STUFF_INT (db->dbb_users);
    }

if (db->dbb_c_user && !db->dbb_r_user)
    {
    STUFF (gds__dpb_user_name);
    l = strlen (db->dbb_c_user);
    STUFF (l); 
    ch = db->dbb_c_user;
    while (*ch) 
	STUFF (*ch++);
    }

if (db->dbb_c_password && !db->dbb_r_password)
    {
    STUFF (gds__dpb_password);
    l = strlen (db->dbb_c_password);
    STUFF (l); 
    ch = db->dbb_c_password;
    while (*ch) 
	STUFF (*ch++);
    }

if (db->dbb_chkptlen)
    {
    STUFF (gds__dpb_wal_chkptlen);
    STUFF (4);
    STUFF_INT (db->dbb_chkptlen);
    }

if (db->dbb_numbufs)
    {
    STUFF (gds__dpb_wal_numbufs);
    STUFF (2);
    STUFF_WORD (db->dbb_numbufs);
    }

if (db->dbb_bufsize)
    {
    STUFF (gds__dpb_wal_bufsize);
    STUFF (2);
    STUFF_WORD (db->dbb_bufsize);
    }


if (db->dbb_grp_cmt_wait != -1)
    {
    STUFF (gds__dpb_wal_grp_cmt_wait);
    STUFF (4);
    STUFF_INT (db->dbb_grp_cmt_wait);
    }

*request->req_blr = 0;
request->req_length = request->req_blr - request->req_base; 
if (request->req_length == 1)
    request->req_length = 0;
request->req_blr = request->req_base;
}

static void create_database_modify_dyn (
    REQ	request,
    ACT action)
{
/**********************************************************
 *
 *	c r e a t e _ d a t a b a s e _ m o d i f y _ d y n
 *
 **********************************************************
 *
 * Functional description
 *	Generate dynamic DDL for second stage of create database
 *
 **********************************************************/
DBB	db;
FIL	files, file, next;
SLONG	start;

db = ((MDBB) action->act_object)->mdbb_database;

STUFF (gds__dyn_mod_database);

/* Reverse the order of files (parser left them backwards) */

files = db->dbb_files;
for (file = files, files = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = files;
    files = file;
    }

start = db->dbb_length;

for (file = files; file != NULL; file=file->fil_next)
    {
    put_cstring (request, gds__dyn_def_file, file->fil_name);
    STUFF (gds__dyn_file_start);
    STUFF_WORD (4);
    start = MAX (start, file->fil_start);
    STUFF_INT (start);
    STUFF (gds__dyn_file_length);
    STUFF_WORD (4);
    STUFF_INT (file->fil_length);
    STUFF_END;
    start += file->fil_length;
    }

/* Add log files */
	
if ((db->dbb_flags & DBB_log_default) || (db->dbb_logfiles))
    add_log_files (request, action, db);

if (db->dbb_cache_file)
    add_cache (request, action, db);

if (db->dbb_def_charset)
   put_cstring (request, gds__dyn_fld_character_set_name, db->dbb_def_charset);

STUFF_END;
}

static void create_domain (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	c r e a t e _ d o m a i n 
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for CREATE DOMAIN action.
 *
 **************************************/
FLD	field;
TEXT    *default_source;

field = (FLD) action->act_object;

/* add field info */

put_symbol (request, gds__dyn_def_global_fld, field->fld_symbol);
put_field_attributes (request, field);

if (field->fld_default_value)
    {
    put_blr (request, gds__dyn_fld_default_value, field->fld_default_value,
		 CME_expr);
    default_source = (TEXT *) 
			ALLOC (field->fld_default_source->txt_length + 1);
    CPR_get_text (default_source, field->fld_default_source);
    put_cstring (request, gds__dyn_fld_default_source, default_source);    
    } 

if (field->fld_constraints)
    create_domain_constraint (request, action, field->fld_constraints);

STUFF_END;
}

static void create_domain_constraint (
    REQ		request,
    ACT		action,
    CNSTRT	cnstrt)
{
/**************************************
 *
 *	c r e a t e _ d o m a i n _ c o n s t r a i n t
 *
 **************************************
 *
 * Functional description
 *	Generate dyn for creating a constraints for domains.
 *
 **************************************/
TEXT	*source;

for (; cnstrt; cnstrt = cnstrt->cnstrt_next)
    {
    if (cnstrt->cnstrt_flags & CNSTRT_delete)
	continue;

/****    this will be used later 
    put_cstring (request, gds__dyn_rel_constraint, cnstrt->cnstrt_name);
****/

    if (cnstrt->cnstrt_type == CNSTRT_CHECK)
	{
	source = (TEXT*) ALLOC (cnstrt->cnstrt_text->txt_length + 1);
	CPR_get_text (source, cnstrt->cnstrt_text);
	if (source != NULL)
	    put_cstring (request, gds__dyn_fld_validation_source, source); 
	put_blr (request, gds__dyn_fld_validation_blr,
		cnstrt->cnstrt_boolean, CME_expr);
	}
    }
}

static void create_generator (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	c r e a t e _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for creating a generator.
 *
 **************************************/
TEXT	*generator_name;

generator_name = (TEXT *) action->act_object;
put_cstring (request, gds__dyn_def_generator, generator_name);
STUFF_END;
}

static void create_index (
    REQ	request,
    IND index)
{
/**************************************
 *
 *	c r e a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for CREATE INDEX action.
 *
 **************************************/
FLD	field;

if (index->ind_symbol)
    put_symbol (request, gds__dyn_def_idx, index->ind_symbol);
else
    /* An index created because of the UNIQUE constraint on this field.  */
    put_cstring (request, gds__dyn_def_idx, "");

put_symbol (request, gds__dyn_rel_name, index->ind_relation->rel_symbol);

if (index->ind_flags & IND_dup_flag)
    put_numeric (request, gds__dyn_idx_unique, TRUE); 

if (index->ind_flags & IND_descend)
    put_numeric (request, gds__dyn_idx_type, TRUE);

if (index->ind_symbol)
    for (field = index->ind_fields; field; field = field->fld_next)
	put_symbol (request, gds__dyn_fld_name, field->fld_symbol);
else
    /* An index created on this one field because of the
       UNIQUE constraint on this field.  */

    put_symbol (request, gds__dyn_fld_name, index->ind_fields->fld_symbol);

STUFF_END;
}

static void create_shadow (
    REQ	request,
    ACT action)
{
/********************************************
 *
 *	c r e a t e _ s h a d o w
 *
 ********************************************
 *
 * Functional description
 *	Generate dynamic DDL for creating a shadow
 *
 ********************************************/
FIL	files, file, next;
SLONG	start;

files = (FIL) action->act_object;

/* Reverse the order of files (parser left them backwards) */
for (file = files, files = NULL; file; file = next)
    {
    next = file->fil_next;
    file->fil_next = files;
    files = file;
    }
put_numeric (request, gds__dyn_def_shadow, (SSHORT) files->fil_shadow_number);

for (file = files; file != NULL; file=file->fil_next)
    {
    put_cstring (request, gds__dyn_def_file, file->fil_name);
    STUFF (gds__dyn_file_start);
    STUFF_WORD (4);
    start = file->fil_start;
    STUFF_INT (start);
    STUFF (gds__dyn_file_length);
    STUFF_WORD (4);
    STUFF_INT (file->fil_length);
    if (file->fil_flags & FIL_manual)
	put_numeric (request, gds__dyn_shadow_man_auto, 1);
    if (file->fil_flags & FIL_conditional)
	put_numeric (request, gds__dyn_shadow_conditional, 1);
    STUFF_END;
    }
STUFF_END;
}

static void create_table (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	c r e a t e _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for CREATE TABLE action.
 *
 **************************************/
FLD	field;
REL	relation;
USHORT	position;
TEXT    *default_source;

/* add relation name */

relation = (REL) action->act_object;
put_symbol (request, gds__dyn_def_rel, relation->rel_symbol);

/* If the relation is defined as an external file, add dyn for that */

if (relation->rel_ext_file)
    put_cstring (request, gds__dyn_rel_ext_file, relation->rel_ext_file);

put_numeric (request, gds__dyn_rel_sql_protection, 1);
position = 0;

/* add field info */

for (field = relation->rel_fields; field; field = field->fld_next)
    {
    if (field->fld_global)
	{
	put_symbol (request, gds__dyn_def_local_fld, field->fld_symbol);
	put_symbol (request, gds__dyn_fld_source, field->fld_global);
	}
    else
	put_symbol (request, gds__dyn_def_sql_fld, field->fld_symbol);

    put_field_attributes (request, field);
    put_numeric (request, gds__dyn_fld_position, position++);

    if (field->fld_default_value)
	{
	put_blr (request, gds__dyn_fld_default_value, field->fld_default_value,
			CME_expr);
	default_source = (TEXT*)
			ALLOC (field->fld_default_source->txt_length + 1);
	CPR_get_text (default_source, field->fld_default_source);
	put_cstring (request, gds__dyn_fld_default_source, default_source);    
	} 

    STUFF_END;
    if (field->fld_constraints)
	create_constraint (request, action, field->fld_constraints);
    }


if (relation->rel_constraints) 
    create_constraint (request, action, relation->rel_constraints);

STUFF_END;

/* Need to create an index for any fields (columns) declared with
   the UNIQUE constraint. */

for (field = relation->rel_fields; field; field = field->fld_next)
    if (field->fld_index)
	create_index (request, field->fld_index);
}

static void create_trigger (
    REQ		request,
    ACT		action,
    TRG		trigger,
    void	(*routine)())
{
/**************************************
 *
 *	c r e a t e _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for a trigger.
 *
 **************************************/
REL	relation;

relation = (REL) action->act_object;

/* Name of trigger to be generated    */

put_cstring (request, gds__dyn_def_trigger, "");

put_symbol (request, gds__dyn_rel_name, relation->rel_symbol);
put_numeric (request, gds__dyn_trg_type, trigger->trg_type);
put_numeric (request, gds__dyn_trg_sequence, 0);
put_numeric (request, gds__dyn_trg_inactive, 0);
STUFF (gds__dyn_sql_object);

if (trigger->trg_source != NULL)
   put_cstring (request, gds__dyn_trg_source, trigger->trg_source);

if (trigger->trg_message != NULL)
    {
    put_numeric (request, gds__dyn_def_trigger_msg, 1);
    put_cstring (request, gds__dyn_trg_msg, trigger->trg_message);
    STUFF (gds__dyn_end);
    }

/* Generate the BLR for firing the trigger */

put_trigger_blr (request, gds__dyn_trg_blr, trigger->trg_boolean, routine);

STUFF_END;
}

static BOOLEAN create_view (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	c r e a t e _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for CREATE VIEW action.
 *
 **************************************/
FLD		field, fld;
NOD		*ptr, *end, fields, value, view_field, and_nod, eq_nod;
NOD		view_boolean, new_view_field, set_item, set_list;
NOD		iand_node, or_node, anull_node, bnull_node;
REL		relation, sub_relation;
REF		new_view_ref, view_ref, reference;
CTX		context;
SYM		symbol;
SSHORT		position, non_updateable = 0;
TEXT		*view_source;
RSE		select;
TRG		trigger;
CTX		contexts [3];
SSHORT		count;
LLS		stack;
SLC		slice;
REQ		slice_req;
struct fld	tmp_field;

/* add relation name */

relation = (REL) action->act_object;
put_symbol (request, gds__dyn_def_view, relation->rel_symbol);
put_numeric (request, gds__dyn_rel_sql_protection, 1);

/* write out blr */

put_blr (request, gds__dyn_view_blr, relation->rel_view_rse, CME_rse);

/* write out view source   */

view_source = (TEXT *) ALLOC (relation->rel_view_text->txt_length+1);
CPR_get_text (view_source, relation->rel_view_text);
put_cstring (request, gds__dyn_view_source, view_source);

/* Write out view context info */

for (context = request->req_contexts; context; context = context->ctx_next)
    {
    if (!(sub_relation = context->ctx_relation))
	continue;
    put_symbol (request, gds__dyn_view_relation, sub_relation->rel_symbol);
    put_numeric (request, gds__dyn_view_context, context->ctx_internal);
    if (context->ctx_symbol)
	put_symbol (request, gds__dyn_view_context_name, context->ctx_symbol);
    STUFF_END;
    }

/* add the mapping from the rse to the view fields */

field = relation->rel_fields;
fields = relation->rel_view_rse->rse_fields;
position = 0;

for (ptr = fields->nod_arg, end = ptr + fields->nod_count; ptr < end; 
     ptr++, field = (field) ? field->fld_next : NULL)
    {
    fld = NULL;
    value = *ptr;
    if (value->nod_type == nod_field)
	{
	reference = (REF) value->nod_arg [0];
	fld = reference->ref_field;
	if ((value->nod_count >= 2) && (slice_req = value->nod_arg[2]) &&
	    (slice = slice_req->req_slice))
	    IBERROR ("Array slices not supported in views"); 
	context = reference->ref_context;
	}
    if (field)
	symbol = field->fld_symbol;
    else if (fld)
	symbol = fld->fld_symbol;
    else
	{
	request->req_length = 0;
	IBERROR ("view expression requires field name");
	return FALSE;
	}
    if (fld)
	{
	put_symbol (request, gds__dyn_def_local_fld, symbol);
	put_symbol (request, gds__dyn_fld_base_fld, fld->fld_symbol);
	put_numeric (request, gds__dyn_view_context, context->ctx_internal);
	}
    else
	{
	non_updateable = 1;
	put_symbol (request, gds__dyn_def_sql_fld, symbol);
	put_blr (request, gds__dyn_fld_computed_blr, value, CME_expr);
	init_field_struct (&tmp_field);
	CME_get_dtype (value, &tmp_field);
	put_dtype (request, &tmp_field);
	}
    put_numeric (request, gds__dyn_fld_position, position++);
    STUFF_END;
    }

if (relation->rel_flags & REL_view_check) 
    {  /* VIEW WITH CHECK OPTION  */
    /* Make sure VIEW is updateable   */
    select = relation->rel_view_rse;
    if ((select->rse_aggregate) ||
	(non_updateable) ||
	(select->rse_count != 1))
	{
	IBERROR ("Invalid view WITH CHECK OPTION - non-updateable view");
	return FALSE;
	}

    if (!(select->rse_boolean))
	{
	IBERROR ("Invalid view WITH CHECK OPTION - no WHERE clause");
	return FALSE;
	}

    trigger = (TRG) ALLOC (TRG_LEN);

    /* For the triggers, the OLD, NEW contexts are reserved  */

    request->req_internal = 0;
    request->req_contexts = NULL_PTR;

    /* Make the OLD context for the trigger    */

    contexts [0] = request->req_contexts = context = MAKE_CONTEXT (request);
    context->ctx_relation = relation;

    /* Make the NEW context for the trigger    */

    contexts [1] = request->req_contexts = context = MAKE_CONTEXT (request);
    context->ctx_relation = relation;

    /* Make the context for the  base relation  */

    contexts [2] = select->rse_context[0] = request->req_contexts = context = MAKE_CONTEXT (request);
    context->ctx_relation = sub_relation;

    /* Make lists to assign from NEW fields to fields in the base relation.  */
    /* Also make sure rows in base relation correspond to rows in VIEW by
       making sure values in fields inherited by the VIEW are same as
       values in base relation.  */

    field = relation->rel_fields;
    fields = relation->rel_view_rse->rse_fields;
    and_nod = eq_nod = NULL_PTR;
    count = 0;
    stack = NULL;
    for (ptr = fields->nod_arg, end = ptr + fields->nod_count; ptr < end;
	 ptr++, field = (field) ? field->fld_next : NULL)
	{
	fld = NULL;
	value = *ptr;
	if (value->nod_type == nod_field)
	    {
	    reference = (REF) value->nod_arg [0];
	    reference->ref_context = contexts [2];
	    fld = reference->ref_field;
	    }
	view_ref = MAKE_REFERENCE (NULL_PTR);
	view_ref->ref_context = contexts [0];
	new_view_ref = MAKE_REFERENCE (NULL_PTR);
	new_view_ref->ref_context = contexts [1];
	new_view_ref->ref_field = view_ref->ref_field = (field) ? field : fld;
	view_field = MSC_unary (nod_field, view_ref);
	new_view_field = MSC_unary (nod_field, new_view_ref);

	anull_node = MSC_unary (nod_missing, view_field);
	bnull_node = MSC_unary (nod_missing, value);
	iand_node = MSC_binary (nod_and, anull_node, bnull_node);

	eq_nod = MSC_binary (nod_eq, view_field, value);

	or_node = MSC_binary (nod_or, eq_nod, iand_node);

	set_item = MAKE_NODE (nod_assignment, 2);
	set_item->nod_arg [1] = value;
	set_item->nod_arg [0] = new_view_field;
	PUSH (set_item, &stack);
	count++;

	if (!or_node)
	    or_node = MSC_binary (nod_or, eq_nod, iand_node); 
	else if (!and_nod)
	    and_nod = MSC_binary (nod_and, or_node, 
			MSC_binary (nod_or, eq_nod, iand_node));
	else 
	    and_nod = MSC_binary (nod_and, and_nod,  
			MSC_binary (nod_or, eq_nod, iand_node)); 
	}

    set_list = MAKE_NODE (nod_list, count);
    ptr = set_list->nod_arg + count;
    while (stack)
	*--ptr = (NOD) POP (&stack);

    /* Modify the context of fields in boolean to be that of the
       sub-relation. */

     replace_field_names (select->rse_boolean, NULL_PTR, NULL_PTR, 0, /* Don't null */ contexts);

    view_boolean = select->rse_boolean;
    select->rse_boolean = MSC_binary (nod_and, (and_nod) ? and_nod : or_node, select->rse_boolean);

    /* create the UPDATE trigger   */

    trigger->trg_type = PRE_MODIFY_TRIGGER;

    /* "update violates CHECK constraint on view" */

    trigger->trg_message = (STR) NULL;
    trigger->trg_boolean = (NOD) select;
    create_view_trigger (request, action, trigger, view_boolean, contexts, set_list);

    /* create the Pre-store trigger   */

    trigger->trg_type = PRE_STORE_TRIGGER;

    /* "insert violates CHECK constraint on view" */

    create_view_trigger (request, action, trigger, view_boolean, contexts, set_list);
    }

STUFF_END;

return TRUE;
}

static void create_view_trigger (
    REQ		request,
    ACT		action,
    TRG		trigger,
    NOD		view_boolean,
    CTX		*contexts,
    NOD		set_list)
{
/**************************************
 *
 *	c r e a t e _ v i e w _ t r i g g e r 
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for a trigger.
 *
 **************************************/
REL	relation;

relation = (REL) action->act_object;

/* Name of trigger to be generated    */

put_cstring (request, gds__dyn_def_trigger,""); 

put_symbol (request, gds__dyn_rel_name, relation->rel_symbol);
put_numeric (request, gds__dyn_trg_type, trigger->trg_type);
put_numeric (request, gds__dyn_trg_sequence, 0);
STUFF (gds__dyn_sql_object);    

if (trigger->trg_source != NULL)
   put_cstring (request, gds__dyn_trg_source, trigger->trg_source);

if (trigger->trg_message != NULL)
    {
    put_numeric (request, gds__dyn_def_trigger_msg, 1);
    put_cstring (request, gds__dyn_trg_msg, trigger->trg_message);
    STUFF (gds__dyn_end);
    }

/* Generate the BLR for firing the trigger */

put_view_trigger_blr (request, relation, gds__dyn_trg_blr, trigger, view_boolean, contexts, set_list);

STUFF_END;
}

static void declare_filter (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	d e c l a r e _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for DECLARE FILTER action.
 *
 **************************************/
FLTR	filter;

filter = (FLTR) action->act_object;
put_cstring (request, gds__dyn_def_filter, filter->fltr_name);
put_numeric ( request, gds__dyn_filter_in_subtype, filter->fltr_input_type);
put_numeric (request, gds__dyn_filter_out_subtype, filter->fltr_output_type);
put_cstring (request, gds__dyn_func_entry_point, filter->fltr_entry_point);

put_cstring (request, gds__dyn_func_module_name, filter->fltr_module_name);
STUFF_END;
}

static void declare_udf (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	d e c l a r e _ u d f
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for DECLARE EXTERNAL
 *
 **************************************/
DECL_UDF	udf;
FLD		field, next;
TEXT		*udf_name;
SSHORT		position, blob_position;

udf = (DECL_UDF) action->act_object;
udf_name = udf->decl_udf_name;
put_cstring (request, gds__dyn_def_function, udf_name);
put_cstring (request, gds__dyn_func_entry_point, udf->decl_udf_entry_point);
put_cstring (request, gds__dyn_func_module_name, udf->decl_udf_module_name);

/* Reverse the order of arguments which parse left backwords. */

/*
for (field = udf->decl_udf_arg_list, udf->decl_udf_arg_list = NULL; field; field = next)
    {
    next = field->fld_next;
    field->fld_next = udf->decl_udf_arg_list; 
    udf->decl_udf_arg_list = field;
    }
*/

if (field = udf->decl_udf_return_type)
    {
    /* Function returns a value */

    /* Some data types can not be returned as value */
    if ((udf->decl_udf_return_mode == FUN_value) &&
	   (field->fld_dtype == dtype_text ||
	    field->fld_dtype == dtype_varying ||
	    field->fld_dtype == dtype_cstring ||
	    field->fld_dtype == dtype_blob ||
	    field->fld_dtype == dtype_timestamp))
	CPR_error ("return mode by value not allowed for this data type");

    /* For functions returning a blob, coerce return argument position to
       be the last parameter. */
    
    if (field->fld_dtype == dtype_blob)
    	{
	blob_position = 1;
	for (next = udf->decl_udf_arg_list; next; next = next->fld_next)
	    ++blob_position;
    	put_numeric (request, gds__dyn_func_return_argument, blob_position);
	}
    else 
    	put_numeric (request, gds__dyn_func_return_argument, 0);
    
    position = 0;     
    }
else
    {
    position = 1;

    /* Function modifies an argument whose value is the function return value */

    put_numeric (request, gds__dyn_func_return_argument,
		udf->decl_udf_return_parameter);
    }

/* Now define all the arguments */

if (!position)
    {
    if (field->fld_dtype == dtype_blob)
    	{
    	put_numeric (request, gds__dyn_def_function_arg, blob_position);
    	put_numeric (request, gds__dyn_func_mechanism, FUN_blob_struct);
	}
    else
    	{
    	put_numeric (request, gds__dyn_def_function_arg, 0);
    	put_numeric (request, gds__dyn_func_mechanism, 
				udf->decl_udf_return_mode);
	}
    
    put_cstring (request, gds__dyn_function_name, udf_name);
    put_dtype (request, field);
    STUFF (gds__dyn_end);
    position = 1;
    }

for (field = udf->decl_udf_arg_list; field; field = field->fld_next)
    {
    if (position > 10)
	CPR_error ("External functions can not have more than 10 parameters");
    put_numeric (request, gds__dyn_def_function_arg, position++);

    if (field->fld_dtype == dtype_blob)
	put_numeric (request, gds__dyn_func_mechanism, FUN_blob_struct);
    else
	put_numeric (request, gds__dyn_func_mechanism, FUN_reference);

    put_cstring (request, gds__dyn_function_name, udf_name);
    put_dtype (request, field);
    STUFF (gds__dyn_end);
    }

STUFF_END;
}

static void grant_revoke_privileges (
    REQ	request,
    ACT action)
{
/**************************************
 *
 *	g r a n t _ r e v o k e _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for GRANT or
 *	REVOKE privileges action.
 *
 **************************************/
UCHAR	privileges [5], *p;
LLS	field;
STR	string;
PRV	priv_block;

priv_block = (PRV) action->act_object;
p = privileges;

if (priv_block->prv_privileges & PRV_select)
    *p++ = 'S';

if (priv_block->prv_privileges & PRV_insert)
    *p++ = 'I';

if (priv_block->prv_privileges & PRV_delete)
    *p++ = 'D';

if (priv_block->prv_privileges & PRV_execute)
    *p++ = 'X';

if (priv_block->prv_privileges & PRV_all)
    *p++ = 'A';

if (priv_block->prv_privileges & PRV_references)
    *p++ = 'R';

*p = 0;

/* If there are any select, insert, or delete privileges to be granted
   or revoked output the necessary DYN strings  */

if (p != privileges)
    for (priv_block = (PRV) action->act_object; priv_block;
	    priv_block = priv_block->prv_next)
	{
	if (action->act_type == ACT_dyn_grant)
	    put_cstring (request, gds__dyn_grant, privileges);
	else  /*  action = ACT_dyn_revoke  */
	    put_cstring (request, gds__dyn_revoke, privileges);

	put_cstring (request, priv_block->prv_object_dyn, priv_block->prv_relation);
	put_cstring (request, priv_block->prv_user_dyn, priv_block->prv_username);

	if ((priv_block->prv_privileges & PRV_grant_option) &&
	    ((action->act_type == ACT_dyn_grant) || 
	     (!(request->req_database->dbb_flags & DBB_v3))))
	    put_numeric (request, gds__dyn_grant_options, 1);

	STUFF_END;
	}

/* If there are no UPDATE privileges to be granted or revoked, we've finished */

if (!(((PRV) action->act_object)->prv_privileges & PRV_update))
    return;

p = privileges;
*p++ = 'U';
*p = 0;

for (priv_block = (PRV) action->act_object; priv_block;
	priv_block = priv_block->prv_next)
    {
    if (priv_block->prv_fields)
	{
	for (field = priv_block->prv_fields; field; field = field->lls_next)
	    {
	    if (action->act_type == ACT_dyn_grant)
		put_cstring (request, gds__dyn_grant, privileges);
	    else  /*  action = ACT_dyn_revoke  */
		put_cstring (request, gds__dyn_revoke, privileges);

	    put_cstring (request, priv_block->prv_object_dyn, priv_block->prv_relation);
	    string = (STR) field->lls_object;
	    put_cstring (request, gds__dyn_fld_name, string);
	    put_cstring (request, priv_block->prv_user_dyn, priv_block->prv_username);

	    if ((priv_block->prv_privileges & PRV_grant_option) &&
		((action->act_type == ACT_dyn_grant) || 
		 (!(request->req_database->dbb_flags & DBB_v3))))
		put_numeric (request, gds__dyn_grant_options, 1);

	    STUFF_END;
	    }
	}
    else  /*  No specific fields mentioned;
	      UPDATE privilege granted or revoked on all fields  */
	{
	if (action->act_type == ACT_dyn_grant)
	    put_cstring (request, gds__dyn_grant, privileges);
	else  /*  action = ACT_dyn_revoke  */
	    put_cstring (request, gds__dyn_revoke, privileges);

	put_cstring (request, priv_block->prv_object_dyn, priv_block->prv_relation);
	put_cstring (request, priv_block->prv_user_dyn, priv_block->prv_username);

	if ((priv_block->prv_privileges & PRV_grant_option) &&
	    ((action->act_type == ACT_dyn_grant) || 
	     (!(request->req_database->dbb_flags & DBB_v3))))
	    put_numeric (request, gds__dyn_grant_options, 1);

	STUFF_END;
	}
    }
}

static void init_field_struct (
    FLD		field)
{
/******************************************
 *
 *	i n i t _ f i e l d _ s t r u c t
 *
 ******************************************
 *
 *
 *
 ******************************************/

field->fld_dtype  = 0;
field->fld_length = 0;
field->fld_scale = 0;
field->fld_id = 0;
field->fld_flags = 0;	
field->fld_seg_length = 0;
field->fld_position = 0;
field->fld_sub_type = 0;
field->fld_next = NULL_PTR;
field->fld_array = NULL_PTR;
field->fld_relation = NULL_PTR;
field->fld_procedure = NULL_PTR;
field->fld_symbol = NULL_PTR;
field->fld_global = NULL_PTR;
field->fld_handle = NULL_PTR;
field->fld_prototype = NULL_PTR;
field->fld_array_info = NULL_PTR;
field->fld_default_value = NULL_PTR;
field->fld_default_source = NULL_PTR;
field->fld_index = NULL_PTR;
field->fld_constraints = NULL_PTR;
field->fld_character_set = NULL_PTR;
field->fld_collate = NULL_PTR;
field->fld_computed = NULL_PTR;
field->fld_char_length = 0;
field->fld_charset_id = 0;
field->fld_collate_id = 0;
}

static void put_array_info (
    REQ		request,
    FLD		field)
{
/******************************************
 *
 *	p u t _ a r r a y _ i n f o
 *
 ******************************************
 *
 * Functional description
 *	Put dimensions for the array field.
 *
 ******************************************/
ARY	array_info;
SSHORT	dims;
SSHORT	i;
SLONG	lrange, urange;

array_info = field->fld_array_info;
dims = (SSHORT) array_info->ary_dimension_count;
put_numeric (request, gds__dyn_fld_dimensions, dims);
for (i = 0; i < dims ; i++)
    {
    put_numeric (request, gds__dyn_def_dimension, i);
    STUFF (gds__dyn_dim_lower);
    lrange = (SLONG) (array_info->ary_rpt [i].ary_lower);
    STUFF_WORD (4);
    STUFF_INT (lrange);
    STUFF (gds__dyn_dim_upper);
    urange = (SLONG) (array_info->ary_rpt [i].ary_upper);
    STUFF_WORD (4);
    STUFF_INT (urange);
    STUFF (gds__dyn_end);	
    }
}

static void put_blr (
    REQ		request,
    USHORT	operator,
    NOD		node,
    void	(*routine)())
{
/**************************************
 *
 *	p u t _ b l r
 *
 **************************************
 *
 * Functional description
 *	Put an expression expressed in BLR.
 *
 **************************************/
USHORT	length, offset;

STUFF (operator);
offset = request->req_blr - request->req_base;
STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
(*routine) (node, request, NULL_PTR);
STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
}

static void put_computed_blr (
    REQ		request,
    FLD 	field)
{
/**************************************
 *
 *	p u t _ c o m p u t e d _ b l r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for a computed field.
 *
 **************************************/
REL	relation;
ACT	action;
USHORT	length, offset;
CTX	context;
USHORT	ctx_int;

action = request->req_actions;
relation = (REL) action->act_object;

/* Computed field context has to be 0 - so force it */

context = request->req_contexts;
ctx_int = context->ctx_internal;
context->ctx_internal = 0;
STUFF (gds__dyn_fld_computed_blr);

offset = request->req_blr - request->req_base;
STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
CME_expr (field->fld_computed->cmpf_boolean, request);
STUFF (blr_eoc);

context->ctx_internal = ctx_int;

length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
}

static void put_computed_source (
    REQ		request,
    FLD 	field)
{
/**************************************
 *
 *	p u t _ c o m p u t e d _ s o u r c e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for a computed field.
 *
 **************************************/
REL	relation;
ACT	action;
TEXT	*computed_source; 

action = request->req_actions;
relation = (REL) action->act_object;

if (field->fld_computed->cmpf_text != NULL)
    {
    computed_source = (TEXT*) 
		ALLOC (field->fld_computed->cmpf_text->txt_length + 1);
    CPR_get_text (computed_source, field->fld_computed->cmpf_text);
    put_cstring (request, gds__dyn_fld_computed_source, computed_source);
    }
}

static void put_cstring (
    REQ		request,
    USHORT	operator,
    TEXT	*string)
{
/**************************************
 *
 *	p u t _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Put a null-terminated string valued attributed to the output string.
 *
 **************************************/
USHORT	length;

if (string != NULL) 
   length = strlen (string);
else
   length = 0;
put_string (request, operator, string, length);
}

static void put_dtype (
    REQ		request,
    FLD		field)
{
/**************************************
 *
 *	p u t _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a field for CREATE
 *	or ALTER TABLE action.
 *
 **************************************/
USHORT	dtype;
USHORT	length;
USHORT	precision;
SSHORT	sub_type;

length = field->fld_length;
precision = field->fld_precision;
sub_type = field->fld_sub_type;
switch (field->fld_dtype)
    {
    case dtype_cstring :

	/* If the user is defining a field as cstring then generate
	   blr_cstring. Currently being used only for defining udf's */

	if (field->fld_flags & FLD_meta_cstring)
	    dtype = blr_cstring;
	else
	    {

	    /* Correct the length of C string for meta data operations */

	    if (sw_cstring && !SUBTYPE_ALLOWS_NULLS (field->fld_sub_type))
	        length--;
	    dtype = blr_text;
	    }

    case dtype_text :
	if (field->fld_dtype == dtype_text) 
	    dtype = blr_text;
	/* Fall into */

    case dtype_varying :
	assert (length);
	if (field->fld_dtype == dtype_varying)
	    dtype = blr_varying;

	put_numeric (request, gds__dyn_fld_type, dtype);
	put_numeric (request, gds__dyn_fld_length, length);
	put_numeric (request, gds__dyn_fld_scale, 0);

	if (field->fld_sub_type)
	    put_numeric (request, gds__dyn_fld_sub_type, field->fld_sub_type);

	if (field->fld_char_length)
	    put_numeric (request, gds__dyn_fld_char_length, field->fld_char_length);
	if (field->fld_collate_id)
	    put_numeric (request, gds__dyn_fld_collation, field->fld_collate_id);

	if (field->fld_charset_id)
	    put_numeric (request, gds__dyn_fld_character_set, field->fld_charset_id);
	return;

    case dtype_short :
	dtype = blr_short;
	length = sizeof (SSHORT);
	if (sw_server_version >= 6)
	    {
            put_numeric (request, gds__dyn_fld_type, dtype);
            put_numeric (request, gds__dyn_fld_length, length);
            put_numeric (request, gds__dyn_fld_scale, field->fld_scale);
            put_numeric (request, isc_dyn_fld_precision, field->fld_precision);
            put_numeric (request, gds__dyn_fld_sub_type, field->fld_sub_type);
	    return;
	    }
	break;

    case dtype_long :
	dtype = blr_long;
	length = sizeof (SLONG);
	if (sw_server_version >= 6)
	    {
            put_numeric (request, gds__dyn_fld_type, dtype);
            put_numeric (request, gds__dyn_fld_length, length);
            put_numeric (request, gds__dyn_fld_scale, field->fld_scale);
            put_numeric (request, isc_dyn_fld_precision, field->fld_precision);
            put_numeric (request, gds__dyn_fld_sub_type, field->fld_sub_type);
	    return;
	    }
	break;

    case dtype_blob:
	dtype = blr_blob;
	length = 8;
	put_numeric (request, gds__dyn_fld_type, dtype);
	put_numeric (request, gds__dyn_fld_length, length);
	put_numeric (request, gds__dyn_fld_scale, 0);
	put_numeric (request, gds__dyn_fld_sub_type, field->fld_sub_type);
	put_numeric (request, gds__dyn_fld_segment_length, 
						field->fld_seg_length);
	if (field->fld_sub_type == BLOB_text && field->fld_charset_id)
	    put_numeric (request, gds__dyn_fld_character_set, 
				field->fld_charset_id);
	return;

    case dtype_quad:
	dtype = blr_quad;
	length = 8;
	break;

    case dtype_float :
	dtype = blr_float;
	length = sizeof (float);
	break;

    case dtype_double :
	dtype = blr_double;
	length = sizeof (double);
	break;

/** Begin sql date/time/timestamp **/
    case dtype_sql_date :
	dtype = blr_sql_date;
	length = sizeof(ISC_DATE);
	break;

    case dtype_sql_time :
	dtype = blr_sql_time;
	length = sizeof(ISC_TIME);
	break;

    case dtype_timestamp :
	dtype = blr_timestamp;
	length = sizeof(ISC_TIMESTAMP);
	break;
/** End sql date/time/timestamp **/

    case dtype_int64 :
	dtype = blr_int64;
	length = sizeof(ISC_INT64);
        put_numeric (request, gds__dyn_fld_type, dtype);
        put_numeric (request, gds__dyn_fld_length, length);
        put_numeric (request, gds__dyn_fld_scale, field->fld_scale);
        put_numeric (request, isc_dyn_fld_precision, field->fld_precision);
        put_numeric (request, gds__dyn_fld_sub_type, field->fld_sub_type);
	return;

    default:
	CPR_bugcheck (" *** Unknown datatype in put_dtype *** ");
	break;
    }

put_numeric (request, gds__dyn_fld_type, dtype);
put_numeric (request, gds__dyn_fld_length, length);
put_numeric (request, gds__dyn_fld_scale, field->fld_scale);

}  

static void put_field_attributes (
    REQ		request,
    FLD 	field)
{
/**************************************
 *
 *	p u t _ f i e l d _ a t t r i b u t e s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a field for CREATE
 *	or ALTER TABLE action.
 *
 *	Emit the DDL that is appopriate for a local instance
 *	of a field.  eg: that can vary from the local fields
 *	global field (DOMAIN in SQL).
 *
 **************************************/

if (field->fld_flags & FLD_computed)
    put_computed_blr (request, field);

if (!field->fld_global)
    put_dtype (request, field);

/* Put the DDL for local field instances */

if (field->fld_array_info)
    put_array_info (request, field);

if (field->fld_collate && field->fld_global)
    {
    put_numeric (request, gds__dyn_fld_collation, field->fld_collate->intlsym_collate_id);
    }

if (field->fld_flags & FLD_not_null)
    {
    STUFF (gds__dyn_fld_not_null); 
    }

if (field->fld_flags & FLD_computed)
    put_computed_source (request, field);
}  

static void put_numeric (
    REQ		request,
    USHORT	operator,
    SSHORT	number)
{
/**************************************
 *
 *	p u t _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *	Put a numeric valued attributed to the output string.
 *
 **************************************/

STUFF (operator);
STUFF_WORD (2);
STUFF_WORD (number);
}

static void put_short_cstring (
    REQ		request,
    USHORT	operator,
    TEXT	*string)
{
/**************************************
 *
 *	p u t _ s h o r t _ c s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Put a counted string valued attributed to the output string.
 *	Count value is BYTE instead of WORD like put_cstring & put_string
 *
 **************************************/
SSHORT	length;

if (string == NULL)
    length = 0;
else
    length = strlen (string);

STUFF_CHECK (length);

STUFF (operator);
STUFF (length);

if (string != NULL)
   {
   while (length--)
	STUFF (*string++);
   }
}

static void put_string (
    REQ		request,
    USHORT	operator,
    TEXT	*string,
    USHORT	length)
{
/**************************************
 *
 *	p u t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Put a counted string valued attributed to the output string.
 *
 **************************************/

STUFF_CHECK (length);

if (operator)
    {
    STUFF (operator);
    STUFF_WORD (length);
    }
else
    STUFF (length);


if (string != NULL)
   {
   while (length--)
	STUFF (*string++);
   }
}

static void put_symbol (
    REQ 	request,
    int		operator,
    SYM		symbol)
{
/**************************************
 *
 *	p u t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Put a symbol valued attribute.
 *
 **************************************/

put_cstring (request, operator, symbol->sym_string);
}

static void put_trigger_blr (
    REQ		request,
    USHORT	operator,
    NOD		node,
    void	(*routine)())
{
/**************************************
 *
 *	p u t _ t r i g g e r _ b l r
 *
 **************************************
 *
 * Functional description
 *	Generate BLR for a trigger whose action is to abort.
 *	Abort with a gds_error code error.
 *
 **************************************/
USHORT   length, offset;

STUFF (operator);
offset = request->req_blr - request->req_base;
STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
STUFF (blr_begin);
STUFF (blr_if);
(*routine) (node, request, NULL_PTR);
STUFF (blr_begin);
STUFF (blr_end);  

/* Generate the action for the trigger to be abort   */

STUFF (blr_abort);
put_short_cstring (request, blr_gds_code, "check_constraint");
STUFF (blr_end);  /* for if  */
STUFF (blr_eoc);
length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
}

static void put_view_trigger_blr (
    REQ		request,
    REL		relation,
    USHORT	operator,
    TRG		trigger,
    NOD		view_boolean,
    CTX		*contexts,
    NOD		set_list)
{
/**************************************
 *
 *	p u t _ v i e w _ t r i g g e r _ b l r
 *
 **************************************
 *
 * Functional description
 *	Generate BLR for a trigger for a VIEW WITH CHECK OPTION.
 *	This is messy, the RSE passed in is mutilated by the end.
 *	Fields in the where clause and in the VIEW definition, are replaced
 *	by the VIEW fields.
 *	For fields in the where clause but not in the VIEW definition,
 *	the fields are set to NULL node for the PRE STORE trigger.
 *
 *
 **************************************/
USHORT	length, offset;
RSE	node;

node = (RSE) trigger->trg_boolean;
STUFF (operator);
offset = request->req_blr - request->req_base;
STUFF_WORD (0);
if (request->req_flags & REQ_blr_version4)
    STUFF (blr_version4);
else
    STUFF (blr_version5);
STUFF (blr_begin);

if (trigger->trg_type == PRE_MODIFY_TRIGGER)
    {
    STUFF (blr_for);
    CME_rse (node, request);

    /* For the boolean, replace all fields in the rse and the view, with the
       equivalent view field_name, for remaining fields in rse, leave them
       alone. */

    replace_field_names (view_boolean, node->rse_fields, 
		relation->rel_fields, 0, /* Don't null */ contexts);

    STUFF (blr_begin);
    STUFF (blr_if);
    CME_expr (view_boolean, request);

    STUFF (blr_begin);
    STUFF (blr_end);

    /* Generate the action for the trigger to be abort   */

    STUFF (blr_abort);
    put_short_cstring (request, blr_gds_code, "check_constraint");
    STUFF (blr_end);
    STUFF (blr_end);
    STUFF (blr_eoc);
    }  /* end of PRE_MODIFY_TRIGGER trigger   */

if (trigger->trg_type == PRE_STORE_TRIGGER)
    {
    replace_field_names (view_boolean, node->rse_fields, relation->rel_fields, 1, contexts);

    STUFF (blr_if);
    CME_expr (view_boolean, request);

    STUFF (blr_begin);
    STUFF (blr_end);

    /* Generate the action for the trigger to be abort   */

    STUFF (blr_abort);
    put_short_cstring (request, blr_gds_code, "check_constraint");
    STUFF (blr_end);
    STUFF (blr_eoc);

    } /* end of PRE_STORE_TRIGGER trigger   */

length = request->req_blr - request->req_base - offset - 2;
request->req_base [offset] = length;
request->req_base [offset + 1] = length >> 8;
}

static void replace_field_names (
    NOD		input,
    NOD		search_list,
    FLD		replace_with,
    SSHORT	null_them,
    CTX		*contexts)
{
/**************************************
 *
 *	r e p l a c e _ f i e l d _ n a m e s 
 *
 **************************************
 *
 * Functional description
 *	Replace fields in given rse by fields referenced in VIEW.
 *	if fields in RSE are not part of VIEW definition, then they 
 *	are not changed.
 *	If search list is not specified, then only the context of the fields
 *	in the rse is chaged to contexts[2].
 *	If null_them is TRUE, then fields in rse but not in VIEW definition
 *	are converted to null nodes, this is used for PRE STORE trigger
 *	verification.
 *
 **************************************/
NOD	*ptr, *end;
NOD	*ptrs, *ends;
REF	reference,references;
FLD	rse_field, select_field, view_field;

if (!input)
    return;

if (((input->nod_type == nod_via) || (input->nod_type == nod_any) ||
      (input->nod_type == nod_unique) ) && (search_list == NULL_PTR) &&
      (replace_with  == NULL_PTR) && (input->nod_count == 0))
    {
    IBERROR ("Invalid view WITH CHECK OPTION - no subqueries permitted");
    return;
    }

for (ptr = input->nod_arg, end = ptr + input->nod_count;ptr < end; ptr++)
    {
    if ((*ptr)->nod_type == nod_field)
	{
	reference = (REF) (*ptr)->nod_arg[0];
	rse_field = reference->ref_field;
	if (null_them) 
	    {
	    if (reference->ref_context == contexts [2])
		{
		*ptr = MAKE_NODE (nod_null,0);
		}
	    continue;
	    }
	reference->ref_context = contexts [2];
	if (!search_list) 
	    continue;
	view_field = replace_with;
	for (ptrs = search_list->nod_arg, ends = ptrs + search_list->nod_count; ptrs < ends;
	     ptrs++, view_field = (view_field) ? view_field->fld_next : NULL)
	    {
	    references = (REF) (*ptrs)->nod_arg[0];
	    select_field = references->ref_field;
	    if (rse_field == select_field) 
		{
		reference->ref_field = (view_field) ? view_field : select_field; 
		reference->ref_context = contexts [1];
		break;
		}
	    }
	}
	else 
	    replace_field_names (*ptr, search_list, replace_with, null_them, contexts);
    }
}

static void set_statistics (
    REQ		request,
    ACT		action)
{
/**************************************
 *
 *	s e t _ s t a t i s t i c s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL for a set statistics
 *
 **************************************/
STS	stats;

if (stats = (STS) action->act_object)
    if (stats->sts_flags & STS_index)
	{
	put_cstring (request, gds__dyn_mod_idx, stats->sts_name);
	STUFF (gds__dyn_idx_statistic);
	}

STUFF_END;
}
