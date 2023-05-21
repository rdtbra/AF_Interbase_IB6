/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		proc.e
 *	DESCRIPTION:	Procedure maintenance routines
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
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../qli/compile.h"
#if (defined JPN_EUC || defined JPN_SJIS)
#include "../jrd/kanji.h"
#endif
#include "../qli/err_proto.h"
#include "../qli/lex_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/parse_proto.h"
#include "../qli/proc_proto.h"

DATABASE
    DB1 = EXTERN FILENAME "yachts.lnk";
DATABASE
    DB = EXTERN FILENAME "yachts.lnk";

static int	clear_out_qli_procedures (DBB);
static void	create_qli_procedures (DBB);
static void	probe (DBB, TEXT *);
static int	upcase_name (TEXT *, TEXT *);

static UCHAR	tpb [] = { gds__tpb_version1, gds__tpb_write,
			   gds__tpb_concurrency };

static UCHAR	dyn_gdl1 [] = {
#include "../qli/procddl1.h"
};
static UCHAR	dyn_gdl2 [] = {
#include "../qli/procddl2.h"
};
static UCHAR	dyn_gdl3 [] = {
#include "../qli/procddl3.h"
};
static UCHAR	dyn_gdl4 [] = {
#include "../qli/procddl4.h"
};

void PRO_close (
    DBB		database,
    void	*blob)
{
/**************************************
 *
 *	P R O _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Close out an open procedure blob.
 *
 **************************************/
STATUS	status_vector [20];

if (database && gds__close_blob (status_vector, 
	GDS_REF (blob)))
    ERRQ_database_error (database, status_vector);
}

void PRO_commit (
    DBB		database)
{
/**************************************
 *
 *	P R O _ c o m m i t
 *
 **************************************
 *
 * Functional description
 *	Commit the procedure transaction.
 *
 **************************************/
STATUS	status_vector [20];

if ((database->dbb_capabilities & DBB_cap_multi_trans) &&
    !(LEX_active_procedure()))
    if (gds__commit_transaction (status_vector, 
		GDS_REF (database->dbb_proc_trans)))
	{
	PRO_rollback (database);
	ERRQ_database_error (database, status_vector);
	}
}

void PRO_copy_procedure (
    DBB		old_database,
    TEXT	*old_name,
    DBB		new_database,
    TEXT	*new_name)
{
/**************************************
 *
 *	P R O _ c o p y _  p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Create a new copy of an existing
 *	procedure.  Search databases for the
 *	existing procedure.
 *
 **************************************/
void	*old_blob, *new_blob;
void	*store_request;
STATUS	status_vector[20];
TEXT	s [120], buffer [255];
SSHORT	length;
USHORT	bpb_length;
UCHAR	bpb [20], *p;

if (!new_database)
    new_database = QLI_databases;

if (!old_database)
    {    
    for (old_database = QLI_databases; old_database; old_database = old_database->dbb_next)
	if (old_blob = PRO_fetch_procedure (old_database, old_name))
	    break;
    }                     
else 
    old_blob = PRO_fetch_procedure (old_database, old_name);

if (!old_blob)
    ERRQ_print_error (70, old_name, NULL, NULL, NULL, NULL);
/* Msg 70 procedure \"%s\" is undefined */

if (new_database != old_database)
    PRO_setup (new_database);

new_blob = NULL;
store_request = NULL;
probe (new_database, new_name);

DB1 = new_database->dbb_handle;

/* create blob parameter block since procedure is a text blob */

p = bpb;

#if (defined JPN_EUC || defined JPN_SJIS)
*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_interp;
*p++ = 2;
*p++ = QLI_interp;
*p++ = QLI_interp  >> 8;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

bpb_length = p - bpb;

STORE (REQUEST_HANDLE store_request TRANSACTION_HANDLE new_database->dbb_proc_trans)
	  NEW IN DB1.QLI$PROCEDURES USING 
	strcpy (NEW.QLI$PROCEDURE_NAME, new_name);
	if (gds__create_blob2 (status_vector, 
		GDS_REF (new_database->dbb_handle),
		GDS_REF (new_database->dbb_proc_trans), 
		GDS_REF (new_blob), 
		GDS_REF (NEW.QLI$PROCEDURE),
		bpb_length,
		bpb))
	    ERRQ_database_error (new_database, status_vector);
	while (!(gds__get_segment (status_vector, 
			GDS_REF (old_blob), 
			GDS_REF (length),
			sizeof(buffer), 
			GDS_VAL (buffer))))
	    {
	    buffer [length] = 0;
	    if (gds__put_segment (status_vector, GDS_REF (new_blob), 
			length,	buffer))
		ERRQ_database_error (new_database, status_vector);
	    }
	PRO_close (old_database, old_blob);
	PRO_close (new_database, new_blob);
END_STORE;

/* Release the FOR and STORE requests */			     		 
gds__release_request (gds__status, GDS_REF (store_request));
PRO_commit (new_database);
}

void PRO_create (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	P R O _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Create a new procedure, assuming, of course, it doesn't already
 *	exist.
 *
 **************************************/
STATUS	status_vector [20];
SLONG	start, stop;
void	*blob;
TEXT	s [64];
USHORT	bpb_length;
UCHAR	bpb [20], *p;
#ifdef PC_PLATFORM
SLONG	lines = 0, last_lines = 0;
#endif

/* See if procedure is already in use */

probe (database, name);

blob = NULL;
start = QLI_token->tok_position;
#ifdef PC_PLATFORM
/* skip over the CR-LF pair for OS/2 */

start += 2;
#endif
stop = start;

p = bpb;

#if (defined JPN_EUC || defined JPN_SJIS)
/* create blob parameter block since procedure is a text blob */

*p++ = gds__bpb_version1;
*p++ = gds__bpb_source_interp;
*p++ = 2;
*p++ = QLI_interp;
*p++ = QLI_interp  >> 8;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

bpb_length = p - bpb;

PRO_setup (database);

/* Store record in QLI$PROCEDURES.  Eat tokens until we run into
   END_PROCEDURE, then have LEX store the procedure in blobs. */

STORE (REQUEST_HANDLE database->dbb_store_blob) X IN DB.QLI$PROCEDURES
    gds__vtof (name, X.QLI$PROCEDURE_NAME, sizeof (X.QLI$PROCEDURE_NAME));

    if (gds__create_blob2 (status_vector,
	GDS_REF (database->dbb_handle),
	GDS_REF (gds__trans), 
	GDS_REF (blob), 
	GDS_REF (X.QLI$PROCEDURE),
        bpb_length,
        bpb))
	ERRQ_database_error (database, status_vector);
    while (!MATCH (KW_END_PROCEDURE))
	{
	if (QLI_token->tok_type == tok_eof)
	    SYNTAX_ERROR (350);
	if (QLI_token->tok_type != tok_eol)
#ifdef PC_PLATFORM
	    {
	    stop = QLI_token->tok_position + QLI_token->tok_length;
	    last_lines = lines;
	    }
	else
	    lines++;
#else
	    stop = QLI_token->tok_position + QLI_token->tok_length;
#endif
	LEX_token();
	}

#ifdef PC_PLATFORM
    /* adjust the length downward to ignore line feeds when determining length
       of the procedure to be read in from the trace file, then add a couple
       more bytes for the last CR-LF pair */
    stop = stop - last_lines + 2;
#endif
    LEX_put_procedure (blob, start, stop);
    gds__close_blob (status_vector, GDS_REF (blob));
END_STORE;

/* Commit the procedure transaction, if there is one */

PRO_commit (database);
}

int PRO_delete_procedure (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	P R O _ d e l e t e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Delete a procedure.
 *
 **************************************/
USHORT	count;
void	*request;

request = NULL;
count = 0;

PRO_setup (database);

FOR (REQUEST_HANDLE request) X IN DB.QLI$PROCEDURES WITH 
	X.QLI$PROCEDURE_NAME EQ name
    ERASE X;
    count++;
END_FOR;

gds__release_request (gds__status, GDS_REF (request));

/* Commit the procedure transaction, if there is one */

PRO_commit (database);

return count;
}

void PRO_edit_procedure (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	P R O _ e d i t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Edit a procedure, using the token stream to get the name of
 *	the procedure.
 *
 **************************************/
TEXT	s [64];

PRO_setup (database);

FOR (REQUEST_HANDLE database->dbb_edit_blob) X IN DB.QLI$PROCEDURES WITH 
	X.QLI$PROCEDURE_NAME EQ name
    MODIFY X USING
#if (defined JPN_EUC  || defined JPN_SJIS)
	if (!BLOB_edit2 (&X.QLI$PROCEDURE, database->dbb_handle,
	    gds__trans, name, 1))
#else
	if (!BLOB_edit (&X.QLI$PROCEDURE, database->dbb_handle,
	    gds__trans, name))
#endif /* (defined JPN_EUC  || defined JPN_SJIS) */
	    return;
    END_MODIFY
    PRO_commit (database);
    return;
END_FOR

STORE (REQUEST_HANDLE database->dbb_edit_store) X IN DB.QLI$PROCEDURES 
    X.QLI$PROCEDURE = gds__blob_null;
#if (defined JPN_EUC  || defined JPN_SJIS)
    if (!BLOB_edit2 (&X.QLI$PROCEDURE, database->dbb_handle,
	   gds__trans, name, 1))
#else
    if (!BLOB_edit (&X.QLI$PROCEDURE, database->dbb_handle,
	   gds__trans, name))
#endif /* (defined JPN_EUC  || defined JPN_SJIS) */
	return;
    gds__vtof (name, X.QLI$PROCEDURE_NAME, sizeof (X.QLI$PROCEDURE_NAME));
END_STORE

/* Commit the procedure transaction, if there is one */

PRO_commit (database);
}

void *PRO_fetch_procedure (
    DBB		database,
    TEXT	*proc)
{
/**************************************
 *
 *	P R O _ f e t c h _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Fetch a procedure.  Look up a name and return an open blob.
 *	If the name doesn't exit, return NULL.
 *
 **************************************/
int	*blob;
TEXT	name [32];
USHORT	l;

upcase_name (proc, name);
PRO_setup (database);
blob = NULL;

FOR (REQUEST_HANDLE database->dbb_lookup_blob) X IN DB.QLI$PROCEDURES WITH 
	X.QLI$PROCEDURE_NAME EQ name
    blob = PRO_open_blob (database, &X.QLI$PROCEDURE);
END_FOR

return blob;
}

int PRO_get_line (
    void	*blob,
    TEXT	*buffer,
    USHORT	size)
{
/**************************************
 *
 *	P R O _ g e t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Get the next segment of procedure blob.  If there are
 *	no more segments, return false.
 *
 **************************************/
STATUS	status_vector [20];
TEXT	*p;
USHORT	length;

gds__get_segment (status_vector, 
    GDS_REF (blob), 
    GDS_REF (length),
    size, 
    GDS_VAL (buffer));

if (status_vector [1] && status_vector [1] != gds__segment)
    return FALSE;

p = buffer + length;

if (p [-1] != '\n' && !status_vector [1])
    *p++ = '\n';

*p = 0;

return TRUE;
}

void PRO_invoke (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	P R O _ i n v o k e
 *
 **************************************
 *
 * Functional description
 *	Invoke a procedure.  The qualified procedure
 *	block may include the database we want, or
 *	we may have to loop through databases.  Whatever...
 *
 **************************************/
void	*blob;
TEXT	s [64];

blob = NULL;
if (database)
    {
    if (!(blob = PRO_fetch_procedure (database, name)))
	ERRQ_print_error (71, name, database->dbb_symbol->sym_string, NULL, NULL, NULL);
/* Msg 71 procedure \"%s\" is undefined in database %s */
    }
else 
    for (database = QLI_databases; database; database = database->dbb_next)
	if (blob = PRO_fetch_procedure (database, name))
	    break;
	    
if (!blob)
    ERRQ_print_error (72, name, NULL, NULL, NULL, NULL);
/* Msg 72 procedure \"%s\" is undefined */

LEX_procedure (database, blob);
LEX_token();
}

void *PRO_open_blob (
    DBB		database,
    SLONG	*blob_id)
{
/**************************************
 *
 *	P R O _ o p e n _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Open a procedure blob.
 *
 **************************************/
void	*blob;
STATUS	status_vector [20];
USHORT	bpb_length;
UCHAR	bpb [20], *p;

blob = NULL;
p = bpb;

#if (defined JPN_EUC || defined JPN_SJIS)
/* create blob parameter block since procedure is a text blob */

*p++ = gds__bpb_version1;
*p++ = gds__bpb_target_interp;
*p++ = 2;
*p++ = QLI_interp;
*p++ = QLI_interp  >> 8;
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

bpb_length = p - bpb;

if (gds__open_blob2 (status_vector,
	GDS_REF (database->dbb_handle),
	GDS_REF (database->dbb_proc_trans), 
	GDS_REF (blob), 
        GDS_VAL (blob_id),
        bpb_length,
        bpb))
        ERRQ_database_error (database, status_vector);

return blob;
}

int PRO_rename_procedure (
    DBB		database,
    TEXT	*old_name,
    TEXT	*new_name)
{
/**************************************
 *
 *	P R O _ r e n a m e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Change the name of a procedure.
 *
 **************************************/
USHORT	count;
STATUS	status_vector [20];
TEXT	s [132];
void	*request;

request = NULL;
PRO_setup (database);

probe (database, new_name);

count = 0;
FOR (REQUEST_HANDLE request) X IN DB.QLI$PROCEDURES WITH 
	X.QLI$PROCEDURE_NAME EQ old_name
    count++;
    MODIFY X USING
	gds__vtof (new_name, X.QLI$PROCEDURE_NAME, sizeof (X.QLI$PROCEDURE_NAME));
    END_MODIFY
    ON ERROR
	gds__release_request (gds__status, GDS_REF (request));
	ERRQ_database_error (database, status_vector);
    END_ERROR;
END_FOR;

gds__release_request (gds__status, GDS_REF (request));

/* Commit the procedure transaction, if there is one */

PRO_commit (database);

return count;
}

void PRO_rollback (
    DBB		database)
{                                         
/**************************************
 *
 *	P R O _ r o l l b a c k
 *
 **************************************
 *
 * Functional description
 *	Rollback the procedure transaction,
 *	if there is one.
 *
 **************************************/
STATUS	status_vector [20];

if (database->dbb_capabilities & DBB_cap_multi_trans)
    {
    gds__rollback_transaction (status_vector,
	GDS_REF (database->dbb_proc_trans));

    gds__trans = NULL; 
    }
}

void PRO_scan (
    DBB		database,
    void	(*routine)(),
    void	*arg)
{
/**************************************
 *
 *	P R O _ s c a n
 *
 **************************************
 *
 * Functional description
 *	Loop thru procedures calling given routine.
 *
 **************************************/
TEXT	*p;

PRO_setup (database);

FOR (REQUEST_HANDLE database->dbb_scan_blobs) X IN DB.QLI$PROCEDURES
	SORTED BY X.QLI$PROCEDURE_NAME
    for (p = X.QLI$PROCEDURE_NAME; *p && *p != ' '; p++)
	;
    (*routine) (arg, 
		X.QLI$PROCEDURE_NAME, 
		strlen (X.QLI$PROCEDURE_NAME),
		database,
		&X.QLI$PROCEDURE);
END_FOR
}

void PRO_setup (
    DBB		dbb)
{
/**************************************
 *
 *	 P R O _ s e t u p
 *
 **************************************
 *
 * Functional description
 *	Prepare for a DML operation on a database.  Start a procedure
 *	transaction is one hasn't been started and the database
 *	system will start multiple transactions.  Otherwise use
 *	the default transaction.
 *
 **************************************/

if (!dbb)
    IBERROR (77);
/* Msg 77 database handle required */

/* If we don't have a QLI$PROCEDURES relation, and can't get one, punt */

if (dbb->dbb_type == gds__info_db_impl_rdb_vms &&
    !(dbb->dbb_flags & DBB_procedures))
    IBERROR (78);
/* Msg 78 QLI$PROCEDURES relation must be created with RDO in Rdb/VMS databases */

DB = dbb->dbb_handle;

if (dbb->dbb_flags & DBB_procedures)
    {
    gds__trans = PRO_transaction (dbb, FALSE);
    return;
    }

/* Sigh.  Relation doesn't exist.  So make it exist. */

create_qli_procedures (dbb);

gds__trans = PRO_transaction (dbb, FALSE);
}

void *PRO_transaction (
    DBB		database,
    int		update_flag)
{
/**************************************
 *
 *	P R O _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	Setup transaction for procedure or form operation.
 *
 *	In any event, we will set up the met_transaction because
 *	it's widely used and somebody is going to forget to test
 *	that the database is multi_transaction before using it.
 *	We have to be careful about committing or rolling back
 *	because we ould affect user data.
 *
 **************************************/
STATUS	status_vector [20];
void	*transaction;

if (!database)
    IBERROR (248); /* Msg248 no active database for operation */

transaction = (database->dbb_capabilities & DBB_cap_multi_trans) ? 
	 database->dbb_proc_trans : NULL;

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

/* If we already have a procedure transaction, there's more nothing to do */

gds__trans = transaction;

/* If we only support a single transaction, use the data transaction */

if (!gds__trans && (database->dbb_capabilities & DBB_cap_single_trans))
    {
    if (update_flag)
	IBERROR (249); /* Msg249 Interactive metadata updates are not available on Rdb */
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

database->dbb_proc_trans = gds__trans;

return gds__trans;
}

static int clear_out_qli_procedures (
    DBB	dbb)
{
/**************************************
 *
 *	c l e a r _ o u t  _ q l i _ p r o c e d u r e s
 *
 **************************************
 *
 * Functional description
 *	The procedures relation can't be found.  Poke
 *	around and delete any trash lying around.
 *	Before cleaning out the trash, see if somebody
 *	else has set up for us.
 *
 **************************************/
int	count;
void	*req;
STATUS	status_vector[20];

req  = NULL;
count = 0;

FOR (REQUEST_HANDLE req) R IN DB.RDB$RELATIONS CROSS 
	F IN DB.RDB$FIELDS CROSS RFR IN DB.RDB$RELATION_FIELDS WITH
	RFR.RDB$RELATION_NAME = R.RDB$RELATION_NAME AND 
	RFR.RDB$FIELD_NAME = F.RDB$FIELD_NAME AND
	R.RDB$RELATION_NAME = "QLI$PROCEDURES"
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (status_vector, GDS_REF (req));

if (count >= 2)
    { 
    dbb->dbb_flags |= DBB_procedures;
    return 0;
    }

count = 0;

FOR (REQUEST_HANDLE req) X IN DB.RDB$INDICES WITH X.RDB$INDEX_NAME = "QLI$PROCEDURES_IDX1"
    ERASE X;
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (status_vector, GDS_REF (req));

FOR (REQUEST_HANDLE req) X IN DB.RDB$INDEX_SEGMENTS WITH X.RDB$INDEX_NAME = "QLI$PROCEDURES_IDX1"
    ERASE X;
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (status_vector, GDS_REF (req));

FOR (REQUEST_HANDLE req) X IN DB.RDB$RELATION_FIELDS 
	WITH X.RDB$FIELD_NAME = "QLI$PROCEDURE_NAME" OR
	X.RDB$FIELD_NAME = "QLI$PROCEDURE"
    ERASE X;
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (gds__status, GDS_REF (req));

FOR (REQUEST_HANDLE req) X IN DB.RDB$FIELDS 
	WITH X.RDB$FIELD_NAME = "QLI$PROCEDURE_NAME" OR
	X.RDB$FIELD_NAME = "QLI$PROCEDURE"
    ERASE X;
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (gds__status, GDS_REF (req)); 

FOR (REQUEST_HANDLE req) X IN DB.RDB$RELATIONS 
	WITH X.RDB$RELATION_NAME = "QLI$PROCEDURES" 
    ERASE X;
    count++;
END_FOR
ON ERROR
    gds__release_request (status_vector, GDS_REF (req));
    ERRQ_database_error (dbb, gds__status);
END_ERROR;
gds__release_request (gds__status, GDS_REF (req)); 

return count;
}

static void create_qli_procedures (
    DBB		dbb)
{
/**************************************
 *
 *	c r e a t e _ q l i _ p r o c e d u r e s
 *
 **************************************
 *
 * Functional description
 *	The procedures relation can't be found.  Clean
 *      out residual trash and (re)create the relation. 
 *	Before cleaning out the trash, see if somebody
 *	else has set up for us.
 *
 **************************************/

gds__trans = PRO_transaction (dbb, TRUE);

if (clear_out_qli_procedures (dbb))
    {
    PRO_commit (dbb);
    gds__trans = PRO_transaction (dbb, TRUE);
    }

if (dbb->dbb_flags & DBB_procedures)
    return;

if (gds__ddl (gds__status, 
    GDS_REF (DB), 
    GDS_REF (gds__trans),
    sizeof (dyn_gdl1), 
    dyn_gdl1))
	{
        PRO_rollback (dbb);
	IBERROR (73);
	/* Msg 73 Could not create QLI$PROCEDURE_NAME field */
	} 

if (gds__ddl (gds__status, 
    GDS_REF (DB), 
    GDS_REF (gds__trans),
    sizeof (dyn_gdl2), 
    dyn_gdl2))
	{
        PRO_rollback (dbb);
	IBERROR (74);
	/* Msg 74 Could not create QLI$PROCEDURE field */
	} 

if (gds__ddl (gds__status, 
    GDS_REF (DB), 
    GDS_REF (gds__trans),
    sizeof (dyn_gdl3), 
    dyn_gdl3))
	{
        PRO_rollback (dbb);
	IBERROR (75);
	/* Msg 75 Could not create QLI$PROCEDURES relation */
	} 

if (gds__ddl (gds__status, 
    GDS_REF (DB), 
    GDS_REF (gds__trans),
    sizeof (dyn_gdl4), 
    dyn_gdl4))
	{
        PRO_rollback (dbb);
	IBERROR (409);
	/* msg 409  Could not create QLI$PROCEDURES index */
	} 

dbb->dbb_flags |= DBB_procedures;
PRO_commit (dbb);
}

static void probe (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	p r o b e
 *
 **************************************
 *
 * Functional description
 *	Check whether a procedure name is already in
 *	use in a particular database.  IBERROR and don't
 *	return if it is.
 *
 **************************************/
void	*blob;
STATUS	status_vector[20];

/* Probe to see if procedure is already in use */

if (blob = PRO_fetch_procedure (database, name))
    {
    gds__close_blob (status_vector, GDS_REF (blob));
    ERRQ_print_error (76, name, database->dbb_symbol->sym_string, NULL, NULL, NULL);
/* Msg 76 procedure name \"%s\" in use in database %s */
    }
}

static int upcase_name (
    TEXT	*name,
    TEXT	*buffer)
{
/**************************************
 *
 *	u p c a s e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Upcase a null terminated string and return its length.  If the
 *	length is greater than 31 bytes, barf.
 *
 **************************************/
USHORT	l;
TEXT	c;

l = 0;

while (TRUE)
    {
    c = *name++;
    *buffer++ = UPPER (c);
    if (!c)
	return l;
    if (++l > 31)
	IBERROR (79); /* Msg 79 procedure name over 31 characters */

#ifdef JPN_SJIS
    /* Do not upcase second byte of a sjis kanji character */

    if (KANJI1(c))
        {
        c = *name++;
        *buffer++ = c;
        if (!c)
            return l;
        if (++l > 31)
            IBERROR (79);
        }
#endif
    }
}
