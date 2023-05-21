/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		command.c
 *	DESCRIPTION:	Interprete commands
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
#include <stdlib.h>
#include <string.h>
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../qli/compile.h"
#include "../qli/exe.h"
#include "../jrd/license.h"
#include "../qli/all_proto.h"
#include "../qli/err_proto.h"
#include "../qli/exe_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/proc_proto.h"
#include "../jrd/gds_proto.h"
#ifdef VMS
#include <descrip.h>
#endif

static void	dump_procedure (DBB, IB_FILE *, TEXT *, USHORT, void *);
static void	extract_procedure (IB_FILE *, TEXT *, USHORT, DBB, SLONG *);

extern USHORT	QLI_lines, QLI_columns, QLI_form_mode, QLI_name_columns;

static SCHAR	db_items [] =
		{ gds__info_page_size, gds__info_allocation, gds__info_end }; 

int CMD_check_ready (void)
{
/**************************************
 *
 *	C M D _ c h e c k _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Make sure at least one database is ready.  If not, give a 
 *	message.
 *
 **************************************/

if (QLI_databases)
    return FALSE;

ERRQ_msg_put (95, NULL, NULL, NULL, NULL, NULL); /* Msg95 No databases are currently ready */

return TRUE;
}

void CMD_copy_procedure (
    SYN		node)
{
/**************************************
 *
 *	C M D _ c o p y  _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Copy one procedure to another, possibly 
 *	across databases
 *
 **************************************/
QPR		new_proc, old_proc;

old_proc = (QPR) node->syn_arg[0];
new_proc = (QPR) node->syn_arg[1];

PRO_copy_procedure (old_proc->qpr_database, 
		old_proc->qpr_name->nam_string, 
		new_proc->qpr_database, 
		new_proc->qpr_name->nam_string);
}

void CMD_define_procedure (
    SYN		node)
{
/**************************************
 *
 *	C M D _ d e f i n e  _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Define a procedure in the named database
 *	or in the most recently readied database.
 *
 **************************************/
QPR		proc;

proc = (QPR) node->syn_arg[0];

if (!(proc->qpr_database))
    proc->qpr_database = QLI_databases;

PRO_create (proc->qpr_database, proc->qpr_name->nam_string);
}

void CMD_delete_proc (
    SYN		node)
{
/**************************************
 *
 *	C M D _ d e l e t e _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Delete a procedure in the named database
 *	or in the most recently readied database.
 *
 **************************************/
QPR		proc;

proc = (QPR) node->syn_arg[0];

if (!proc->qpr_database)
    proc->qpr_database = QLI_databases;

if (PRO_delete_procedure (proc->qpr_database, proc->qpr_name->nam_string))
     return;

ERRQ_msg_put (88, proc->qpr_name->nam_string, /* Msg88 Procedure %s not found in database %s */
	proc->qpr_database->dbb_symbol->sym_string, NULL, NULL, NULL);
}

void CMD_edit_proc (
    SYN		node)
{
/**************************************
 *
 *	C M D _ e d i t _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Edit a procedure in the specified database.
 *
 **************************************/
QPR	proc;

proc = (QPR) node->syn_arg[0];
if (!proc->qpr_database)
    proc->qpr_database = QLI_databases;

PRO_edit_procedure (proc->qpr_database, proc->qpr_name->nam_string);
}

void CMD_extract (
    SYN		node)
{
/**************************************
 *
 *	C M D _ e x t r a c t
 *
 **************************************
 *
 * Functional description
 *	Extract a series of procedures.
 *
 **************************************/
SYN	list, *ptr, *end;
QPR	proc;
DBB	database;
NAM	name;
IB_FILE	*file;
int	*blob;

file = EXEC_open_output (node->syn_arg [1]);

if (list = node->syn_arg [0])
    for (ptr = list->syn_arg, end = ptr + list->syn_count; ptr < end; ptr++)
	{
	proc = (QPR) *ptr;
	if (!(database = proc->qpr_database))
	    database = QLI_databases;
	name = proc->qpr_name;
	if (!(blob = PRO_fetch_procedure (database, name->nam_string)))
	    {
	    ERRQ_msg_put (89, /* Msg89 Procedure %s not found in database %s */
		name->nam_string, database->dbb_symbol->sym_string,
		NULL, NULL, NULL);
	    continue;
	    }
	dump_procedure (database, file, name->nam_string, name->nam_length, blob);
	}
else
    {
    CMD_check_ready();
    for (database = QLI_databases; database; database = database->dbb_next)
	PRO_scan (database, extract_procedure, file);
    }

#ifdef WIN_NT
if (((NOD) node->syn_arg [1])->nod_arg [e_out_pipe])
    _pclose (file);
else
#endif
ib_fclose (file);
}

void CMD_finish (
    SYN		node)
{
/**************************************
 *
 *	C M D _ f i n i s h
 *
 **************************************
 *
 * Functional description
 *	Perform FINISH.  Either finish listed databases or everything.
 *
 **************************************/
USHORT	i;

if (node->syn_count == 0)
    {
    while (QLI_databases)
	MET_finish (QLI_databases);
    return;
    }

for (i = 0; i < node->syn_count; i++)
    MET_finish (node->syn_arg [i]);
}

void CMD_rename_proc (
    SYN	node)
{
/**************************************
 *
 *	C M D _ r e n a m e _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Rename a procedure in the named database,
 *	or the most recently readied database.
 *
 **************************************/
QPR	old_proc, new_proc;
DBB	database;
NAM	old_name, new_name;

old_proc = (QPR) node->syn_arg[0];
new_proc = (QPR) node->syn_arg[1];

if (!(database = old_proc->qpr_database))
    database = QLI_databases;

if (new_proc->qpr_database && (new_proc->qpr_database != database))
    IBERROR (84); /* Msg84 Procedures can not be renamed across databases. Try COPY */
old_name = old_proc->qpr_name;
new_name = new_proc->qpr_name;

if (PRO_rename_procedure (database, old_name->nam_string, new_name->nam_string))
	return;

ERRQ_error (85, /* Msg85 Procedure %s not found in database %s */
	old_name->nam_string, database->dbb_symbol->sym_string, NULL, NULL, NULL);
}

void CMD_set (
    SYN		node)
{
/**************************************
 *
 *	C M D _ s e t
 *
 **************************************
 *
 * Functional description
 *	Set various options.
 *
 **************************************/
SYN		*ptr, value;
ENUM set_t	sw;
USHORT		i, foo, length;
CON		string;
TEXT		*name;

ptr = node->syn_arg;

for (i = 0; i < node->syn_count; i++)
    {
    foo = (USHORT) *ptr++;
    sw = (ENUM set_t) foo;
    value = *ptr++;
    switch (sw)
	{
	case set_blr:
	    QLI_blr = (USHORT) value;
	    break;

	case set_statistics:
	    QLI_statistics = (USHORT) value;
	    break;
	
	case set_columns:
	    QLI_name_columns = QLI_columns = (USHORT) value;
	    break;

	case set_lines:
	    QLI_lines = (USHORT) value;
	    break;

	case set_semi:
	    QLI_semi = (USHORT) value;
	    break;

	case set_echo:
	    QLI_echo = (USHORT) value;
	    break;

	case set_form:
#ifdef NO_PYXIS
	    IBERROR (484); /* FORMs not supported */
#else
	    QLI_form_mode = (USHORT) value;
#endif
	    break;

	case set_password:
	    string = (CON) value;
	    length = MIN (string->con_desc.dsc_length, sizeof (QLI_default_password));
	    strncpy (QLI_default_password, string->con_data, length);
	    QLI_default_password [length] = 0;
	    break;

	case set_prompt:
	    string = (CON) value;
	    if (string->con_desc.dsc_length > sizeof (QLI_prompt_string))
		ERRQ_error (86, NULL, NULL, NULL, NULL, NULL); /* Msg86 substitute prompt string too long */
	    strncpy (QLI_prompt_string, string->con_data,
		string->con_desc.dsc_length);
	    QLI_prompt_string [string->con_desc.dsc_length] = 0;
	    break;

	case set_continuation:
	    string = (CON) value;
	    if (string->con_desc.dsc_length > sizeof (QLI_cont_string))
		ERRQ_error (87, NULL, NULL, NULL, NULL, NULL); /* Msg87 substitute prompt string too long */
	    strncpy (QLI_cont_string, string->con_data,
		string->con_desc.dsc_length);
	    QLI_cont_string [string->con_desc.dsc_length] = 0;
	    break;
 
	case set_matching_language:
	    if (QLI_matching_language)
		ALLQ_release (QLI_matching_language);		    
	    if (!(string = (CON) value))
		{
		QLI_matching_language = NULL;
		break;
		}
	    QLI_matching_language = (CON) ALLOCPV (type_con, string->con_desc.dsc_length);
	    strncpy (QLI_matching_language->con_data, string->con_data, 
		string->con_desc.dsc_length);
	    QLI_matching_language->con_desc.dsc_dtype = dtype_text;
	    QLI_matching_language->con_desc.dsc_address = QLI_matching_language->con_data;
	    QLI_matching_language->con_desc.dsc_length = string->con_desc.dsc_length;
	    break;

#if (defined JPN_SJIS || defined JPN_EUC)
        case set_euc_justify:
            QLI_euc_justify = (USHORT) value;
            break;
#endif 

	case set_user:
	    string = (CON) value;
	    length = MIN (string->con_desc.dsc_length, sizeof (QLI_default_user));
	    strncpy (QLI_default_user, string->con_data, length);
	    QLI_default_user [length] = 0;
	    break;

	    break;

	case set_count:
	    QLI_count = (USHORT) value;
	    break;

	case set_charset:
	    if (!value)
		{
		QLI_charset [0] = 0;
		break;
		}
	    name = ((NAM) value)->nam_string;
	    length = MIN (strlen (name), sizeof (QLI_charset));
	    strncpy (QLI_charset, name, length);
	    QLI_charset [length] = 0;
	    break;

#ifdef DEV_BUILD
	case set_explain:
	    QLI_explain = (USHORT) value;
	    break;

	case set_hex_output:
	    QLI_hex_output = (USHORT) value;
	    break;
#endif

	default:
	    BUGCHECK (6); /* Msg6 set option not implemented */
	}
    }
}

void CMD_shell (
    SYN		node)
{
/**************************************
 *
 *	C M D _ s h e l l
 *
 **************************************
 *
 * Functional description
 *	Invoke operating system shell.
 *
 **************************************/
TEXT		buffer [256], *p, *q;
USHORT		l;
CON		constant;
#ifdef VMS
int		status, return_status, mask;
struct dsc$descriptor	desc, *ptr;
#endif

/* Copy command, inserting extra blank at end. */

p = buffer;

if (constant = (CON) node->syn_arg [0])
    {
    q = (TEXT*) constant->con_data;
    if (l = constant->con_desc.dsc_length)
	do *p++ = *q++; while (--l);
    *p++ = ' ';
    *p = 0;
    }
else
#ifndef WIN_NT
    strcpy (buffer, "$SHELL");
#else
    strcpy (buffer, "%ComSpec%");
#endif

#ifdef VMS
    if (constant)
	{
	desc.dsc$b_dtype = DSC$K_DTYPE_T;
	desc.dsc$b_class = DSC$K_CLASS_S;
	desc.dsc$w_length = p - buffer;
	desc.dsc$a_pointer = buffer;
	ptr = &desc;
	}
    else
	ptr = NULL;
    return_status = 0;
    mask = 1;
    status = lib$spawn (
	ptr,			/* Command to be executed */
	NULL,			/* Command file */
	NULL,			/* Output file */
	&mask,			/* sub-process characteristics mask */
	NULL,			/* sub-process name */
	NULL,			/* returned process id */
	&return_status,		/* completion status */
	&15);			/* event flag for completion */
    if (status & 1)
	while (!return_status)
	    sys$waitfr (15);
#else
    system (buffer);
#endif
}

void CMD_transaction (
    SYN		node)
{
/**************************************
 *
 *	C M D _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Perform COMMIT, ROLLBACK or PREPARE
 *      on listed databases or everything.
 *
 **************************************/
SYN	*ptr, *end;
DBB	database;

/* If there aren't any open databases then obviously
   there isn't anything to commit. */

if (node->syn_count == 0 && !QLI_databases)
    return;

if (node->syn_type ==  nod_commit)
    if ((node->syn_count > 1) ||
        (node->syn_count == 0 && QLI_databases->dbb_next))
	{
	node->syn_type = nod_prepare;
	CMD_transaction (node);
	node->syn_type = nod_commit;
	} 
    else if (node->syn_count == 1) 
	{
    	database = (DBB) node->syn_arg[0];
	database->dbb_flags |= DBB_prepared;
	}
    else
	QLI_databases->dbb_flags |= DBB_prepared;


if (node->syn_count == 0)
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	{
	if ((node->syn_type == nod_commit) &&
	    !(database->dbb_flags & DBB_prepared))
		ERRQ_msg_put (465, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
	else if (node->syn_type == nod_prepare)
	    database->dbb_flags |= DBB_prepared;
	if (database->dbb_transaction)
	    MET_transaction (node->syn_type, database);
	if (database->dbb_meta_trans)
	    MET_meta_commit (database);
	if (database->dbb_proc_trans)
	    PRO_commit (database);
	}
    return;
    }

for (ptr = node->syn_arg, end = ptr + node->syn_count; ptr < end; ptr++)
    {
    database = (DBB) *ptr;
    if ((node->syn_type == nod_commit) &&    
	!(database->dbb_flags & DBB_prepared))
	    ERRQ_msg_put (465, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
    else if (node->syn_type == nod_prepare)
	database->dbb_flags |= DBB_prepared;
    if (database->dbb_transaction)
	MET_transaction (node->syn_type, database);
    }
}

static void dump_procedure (
    DBB		database,
    IB_FILE	*file,
    TEXT	*name,
    USHORT	length,
    void	*blob)
{
/**************************************
 *
 *	d u m p _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Extract a procedure from a database.
 *
 **************************************/
TEXT	buffer [256];

ib_fprintf (file, "DELETE PROCEDURE %.*s;\n", length, name);
ib_fprintf (file, "DEFINE PROCEDURE %.*s\n", length, name);

while (PRO_get_line (blob, buffer, sizeof (buffer)))
    ib_fputs (buffer, file);

PRO_close (database, blob);
ib_fprintf (file, "END_PROCEDURE\n\n");
}

static void extract_procedure (
    IB_FILE	*file,
    TEXT	*name,
    USHORT	length,
    DBB		database,
    SLONG	*blob_id)
{
/**************************************
 *
 *	e x t r a c t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Extract a procedure from a database.
 *
 **************************************/
void	*blob;

blob = PRO_open_blob (database, blob_id);
dump_procedure (database, file, name, length, blob);
}
