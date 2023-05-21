/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		show.e
 *	DESCRIPTION:	Display routines
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

#ifdef GUI_TOOLS
#include <windows.h>
#endif
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../dsql/dsql.h"
#include "../isql/isql.h"
#include "../jrd/license.h"
#include "../jrd/constants.h"
#include "../jrd/intl.h"
#include "../isql/isql_proto.h"
#include "../isql/show_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/utl_proto.h"
#include "../jrd/obj.h"
#include "../jrd/ods.h"

ASSERT_FILENAME

DATABASE DB = EXTERN COMPILETIME "yachts.lnk";

extern USHORT	major_ods;
extern USHORT	minor_ods;
extern SSHORT	V4;
extern SSHORT	V33;

static void	*local_fprintf (UCHAR *, UCHAR *);
static SCHAR	*remove_delimited_double_quotes (TEXT *);
static void	make_priv_string (USHORT, UCHAR *);
static int	show_all_tables (SSHORT);
static void	show_charsets (SCHAR *, SCHAR *);
static int	show_check (SCHAR *);
static void	show_db (void);
static int	show_dialect  (void);
static int	show_domains (SCHAR *);
static int	show_exceptions (SCHAR *);
static int	show_filters (SCHAR *);
static int	show_functions (SCHAR *);
static int	show_generators (SCHAR *);
static void	show_index (SCHAR *, SCHAR *, SSHORT, SSHORT, SSHORT);
static int	show_indices (SCHAR **);
static int	show_proc (SCHAR *);
static int	show_role  (SCHAR *);
static int	show_table (SCHAR *);
static int	show_trigger (SCHAR *, SSHORT);

static TEXT	Print_buffer [512];
static TEXT	SQL_identifier [BUFFER_LENGTH128];
static TEXT	SQL_id_for_grant [BUFFER_LENGTH128];

#define	BLANK		'\040'
#define	DBL_QUOTE	'\042'
#define	MAX_NAME_LENGTH	31

/* Initialize types */

static CONST SCHAR	*Object_types[] = {
	"Table",
	"View",	
	"Trigger", 
	"Computed column",
	"Validation",
	"Procedure",
	"Expression index",
	"Exception",
	"User",
	"Domain",
	"Index"
};

CONST SQLTYPES	Column_types[] = {
	{SMALLINT,	"SMALLINT"},		/* NTX: keyword */
	{INTEGER, 	"INTEGER"},		/* NTX: keyword */
	{QUAD, 		"QUAD"},		/* NTX: keyword */
	{FLOAT, 	"FLOAT"},		/* NTX: keyword */
	{FCHAR, 	"CHAR"},		/* NTX: keyword */
	{DOUBLE_PRECISION, "DOUBLE PRECISION"},	/* NTX: keyword */
	{VARCHAR, 	"VARCHAR"},		/* NTX: keyword */
	{CSTRING, 	"CSTRING"},		/* NTX: keyword */
	{BLOB_ID, 	"BLOB_ID"},		/* NTX: keyword */
	{BLOB, 		"BLOB"},		/* NTX: keyword */
	{blr_sql_time,  "TIME"},		/* NTX: keyword */
	{blr_sql_date,  "DATE"},		/* NTX: keyword */
	{blr_timestamp,	"TIMESTAMP"},		/* NTX: keyword */
	{blr_int64, 	"INT64"},		/* NTX: keyword */
	{0, ""}
};

/* Integral subtypes */

#define MAX_INTSUBTYPES	2

CONST SCHAR 	*Integral_subtypes[] = {
	"UNKNOWN",			/* Defined type, NTX: keyword */
	"NUMERIC",			/* NUMERIC, NTX: keyword */
	"DECIMAL" 			/* DECIMAL, NTX: keyword */
};

/* Blob subtypes */

#define MAXSUBTYPES 8

CONST SCHAR 	*Sub_types[] = {
	"UNKNOWN",			/* NTX: keyword */
	"TEXT",				/* NTX: keyword */
	"BLR",				/* NTX: keyword */
	"ACL",				/* NTX: keyword */
	"RANGES",			/* NTX: keyword */
	"SUMMARY",			/* NTX: keyword */
	"FORMAT",			/* NTX: keyword */
	"TRANSACTION_DESCRIPTION",	/* NTX: keyword */
	"EXTERNAL_FILE_DESCRIPTION"	/* NTX: keyword */
};

CONST SCHAR	*Trigger_types[] = {
	"", 
	"BEFORE INSERT",			/* NTX: keyword */
	"AFTER INSERT",				/* NTX: keyword */
	"BEFORE UPDATE",			/* NTX: keyword */
	"AFTER UPDATE",				/* NTX: keyword */
	"BEFORE DELETE",			/* NTX: keyword */
	"AFTER DELETE"				/* NTX: keyword */
};

#define priv_UNKNOWN	1
#define priv_SELECT	2
#define priv_INSERT	4
#define priv_UPDATE	8
#define priv_DELETE	16
#define priv_EXECUTE	32
#define priv_REFERENCES	64


static CONST struct {
   USHORT	priv_flag;
   CONST SCHAR	 *priv_string;
} privs [] = {
    { priv_DELETE,	"DELETE" },	/* NTX: keyword */
    { priv_EXECUTE,	"EXECUTE" },	/* NTX: keyword */
    { priv_INSERT,	"INSERT" },	/* NTX: keyword */
    { priv_SELECT,	"SELECT" },	/* NTX: keyword */
    { priv_UPDATE,	"UPDATE" },	/* NTX: keyword */
    { priv_REFERENCES,  "REFERENCES"},  /* NTX: keyword */
    { 0,		NULL }
};

/* strlen of each element above, + strlen(", ") for separators */

#define MAX_PRIV_LIST	(6+2+7+2+6+2+6+2+6+2+10+1)

static CONST SCHAR	db_dialect_info [] = { 
	isc_info_db_sql_dialect, 
	isc_info_end
};

static CONST SCHAR	db_items [] = {
	isc_info_page_size,
	isc_info_db_size_in_pages,
	isc_info_sweep_interval,
	isc_info_limbo,
	isc_info_end
};

static CONST SCHAR	wal_items [] = {
	isc_info_num_wal_buffers,
	isc_info_wal_buffer_size,
	isc_info_wal_ckpt_length,
	isc_info_wal_grpc_wait_usecs,
	isc_info_end
};

/* BPB to force transliteration of any shown system blobs from
 * Database character set (CS_METADATA) to process character set
 * (CS_dynamic).
 * This same BPB is safe to use for both V3 & V4 db's - as
 * a V3 db will ignore the source_ & target_interp values.
 */
static CONST UCHAR	metadata_text_bpb [] = { 
	isc_bpb_version1, 
	isc_bpb_source_type, 1, BLOB_text,
	isc_bpb_target_type, 1, BLOB_text,
	isc_bpb_source_interp, 1, CS_METADATA,
	isc_bpb_target_interp, 1, CS_dynamic
};

 
void SHOW_dbb_parameters (
    SLONG	*db_handle,
    SCHAR 	*info_buf,
    SCHAR	*db_items,
    USHORT	item_length,
    USHORT	translate)
{
/**************************************
 *
 *	S H O W _ d b b _ p a r a m e t e r s 
 *
 **************************************
 *
 * Functional description
 *	Show db_info on this database
 *
 *	Arguments:  
 *	    db_handle -- database handle
 *	    info_buf -- info_bufput file pointer
 *	    db_items -- list of db_info items to process
 *
 **************************************/
SCHAR	*d, *buffer, item, *info;
int	length;
SLONG	value_out;
STATUS	status_vector [20];
TEXT    *msg;

buffer 	= (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!buffer)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;	
    ISQL_errmsg (isc_status);
    return;
    }
msg	= (SCHAR *) ISQL_ALLOC (MSG_LENGTH);
if (!msg)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;	
    ISQL_errmsg (isc_status);
    ISQL_FREE (buffer);
    return;
    }

if (isc_database_info (status_vector,
	&db_handle,
	item_length,
	db_items,
	BUFFER_LENGTH128,
	buffer)
    )
    ISQL_errmsg (status_vector);

*info_buf = '\0';
for (d = buffer, info = info_buf; *d != isc_info_end;)
    {
    value_out = 0;
    item = *d++;
    length = isc_vax_integer (d, 2);
    d += 2;
/*
 * This is not the best solution but it fixes the lack of <LF> characters
 * in Windows ISQL.  This will need to remain until we modify the messages
 * to remove the '\n' (on the PC its '\n\r').
 */
#ifdef GUI_TOOLS
    translate = FALSE;
#endif
    switch (item)
	{
	case isc_info_end:
	    break;

	case isc_info_page_size:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "PAGE_SIZE %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_db_size_in_pages:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (NUMBER_PAGES, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    	sprintf (info, "Number of DB pages allocated = %ld %s", 
			 value_out,
			 NEWLINE);
	    break;
 
	case isc_info_sweep_interval:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (SWEEP_INTERV, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    	sprintf (info, "Sweep interval = %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_num_wal_buffers:       
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (NUM_WAL_BUFF, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    sprintf (info, "Number of wal buffers = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_buffer_size:  
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (WAL_BUFF_SIZE, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    sprintf (info, "Wal buffer size = %ld %s", value_out, NEWLINE);
	    break;

	case isc_info_wal_ckpt_length:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (CKPT_LENGTH, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    sprintf (info, "Check point length = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_cur_ckpt_interval:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (CKPT_INTERV, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    sprintf (info, "Check point interval = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_grpc_wait_usecs:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (WAL_GRPC_WAIT, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    sprintf (info, "Wal group commit wait = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_base_level:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (BASE_LEVEL, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    	sprintf (info, "Base level = %ld %s", 
			 value_out,
			 NEWLINE);
	    break;

	case isc_info_limbo:
	    value_out = isc_vax_integer (d, length);
            if (translate)
                {
		ISQL_msg_get (LIMBO, msg, (TEXT*) value_out, NULL, NULL, NULL, NULL);
		sprintf (info, msg);
		}
	    else
	    	sprintf (info, "Transaction in limbo = %ld %s", 
			 value_out,
			 NEWLINE);
	    break;

#ifdef UNUSED_FOR_ISQL

	case isc_info_reads:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of reads = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_writes:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of writes = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_fetches:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of fetches = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_marks:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of marks = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
  
	case isc_info_num_buffers:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of buffers = %ld%s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_wal_num_io:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Wal number of io = %ld %s",
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_avg_io_size:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Wal average io size = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_num_commits:   
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Wal number of commits = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_wal_avg_grpc_size: 
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Wal average group commit size = %ld %s", 
		     value_out
		     NEWLINE);
	    break;

	case isc_info_current_memory:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Current memory size = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_max_memory:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Max memory size = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_attachment_id:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "DB attachment id = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_ods_version:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "ODS version = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_ods_minor_version:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Minor ODS version = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_read_seq_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Reads sequential count = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_read_idx_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Reads via index count = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_update_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of updates = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_insert_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of inserts = %ld %s", 
		     value_out
		     NEWLINE);
	    break;
 
	case isc_info_delete_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Number of deletes = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;
 
	case isc_info_backout_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Backout count = %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_purge_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Purge count = %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_expunge_count:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Expunge count = %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_implementation:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Implementation = %ld %s", value_out, NEWLINE);
	    break;
 
	case isc_info_version:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "Info version number = %ld %s", 
		     value_out,
		     NEWLINE);
	    break;

	case isc_info_no_reserve:
	    value_out = isc_vax_integer (d, length);
	    sprintf (info, "No reserve = %s %s", value_out, NEWLINE  );
	    break;
#endif	/* UNUSED_FOR_ISQL */
	}

    d += length;
    info += strlen (info);
    }
ISQL_FREE (buffer);
ISQL_FREE (msg);
}

int SHOW_grants (
    SCHAR	*object,
    SCHAR	*terminator)
{
/**************************************
 *
 *	S H O W _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Show grants for given object name
 *	This function is also called by extract for privileges.
 *  	It must extract granted privileges on tables/views to users,
 *		- these may be compound, so put them on the same line.
 *	Grant execute privilege on procedures to users
 *	Grant various privilegs to procedures.
 *	All privileges may have the with_grant option set.
 *
 **************************************/
BASED_ON RDB$USER_PRIVILEGES.RDB$USER		prev_user;
BASED_ON RDB$USER_PRIVILEGES.RDB$GRANT_OPTION	prev_option;
BASED_ON RDB$USER_PRIVILEGES.RDB$FIELD_NAME	prev_field;
SCHAR	c, with_option [18]; 
SCHAR	priv_string [MAX_PRIV_LIST];
SCHAR	col_string[BUFFER_LENGTH128];
SCHAR	user_string [44];
USHORT	priv_flags;
SSHORT	prev_field_null;
SSHORT	first = TRUE;
SCHAR	role_name [BUFFER_LENGTH128];

if (!*object)
    return ERR;

/* Query against user_privileges instead of looking at rdb$security_classes*/

prev_option = -1;
prev_user [0] = '\0';
priv_string [0] = '\0';
col_string[0] = '\0';
with_option [0] = '\0';
priv_flags = 0;
prev_field_null = -1;
prev_field [0] = '\0';

/* Find the user specified relation and show its privileges */

FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ object;

    /* This query only finds tables, eliminating owner privileges */

    FOR PRV IN RDB$USER_PRIVILEGES CROSS
	    REL IN RDB$RELATIONS WITH
	    PRV.RDB$RELATION_NAME EQ object AND
	    REL.RDB$RELATION_NAME EQ object AND
	    PRV.RDB$PRIVILEGE     NE 'M'    AND
	    REL.RDB$OWNER_NAME    NE PRV.RDB$USER
	    SORTED BY  PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION;

        ISQL_blankterm (PRV.RDB$USER);

        /* Sometimes grant options are null, sometimes 0.  Both same */

        if (PRV.RDB$GRANT_OPTION.NULL)
	    PRV.RDB$GRANT_OPTION = 0;

        if (PRV.RDB$FIELD_NAME.NULL)
	    PRV.RDB$FIELD_NAME [0] = '\0';

        /* Print a new grant statement for each new user or change of option */

        if ((prev_user [0] && strcmp (prev_user, PRV.RDB$USER)) || 
	    (prev_field_null != -1 &&
	     prev_field_null != PRV.RDB$FIELD_NAME.NULL) ||
	    (!prev_field_null && strcmp (prev_field, PRV.RDB$FIELD_NAME)) ||
	    (prev_option != -1 && prev_option != PRV.RDB$GRANT_OPTION))
	    {
	    make_priv_string (priv_flags, priv_string);

    	    first = FALSE;

	    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		ISQL_copy_SQL_id (object, &SQL_identifier, DBL_QUOTE);
	    else
		strcpy (SQL_identifier, object);

	    sprintf (Print_buffer, "GRANT %s%s ON %s TO %s%s%s%s", priv_string, 
		    col_string, SQL_identifier, user_string, 
		    with_option, terminator, NEWLINE);
	    ISQL_printf (Out, Print_buffer);

	    /* re-initialize strings */

	    priv_string [0] = '\0';
	    with_option [0] = '\0';
	    col_string[0] = '\0';
	        priv_flags = 0;
	    }

        /* At each row, store this value for the next compare of contol break */

        strcpy (prev_user, PRV.RDB$USER);
        prev_option = PRV.RDB$GRANT_OPTION;
        prev_field_null = PRV.RDB$FIELD_NAME.NULL;
        strcpy (prev_field, PRV.RDB$FIELD_NAME);

        switch (PRV.RDB$USER_TYPE)
            {
	    case obj_relation:
	    case obj_view:
	    case obj_trigger:
	    case obj_procedure:
	    case obj_sql_role:
		if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		    {
		    ISQL_copy_SQL_id (PRV.RDB$USER, &SQL_identifier, DBL_QUOTE);
		    }
		else
		    strcpy (SQL_identifier, PRV.RDB$USER);
	        break;
	    default:
		strcpy (SQL_identifier, PRV.RDB$USER);
	        break;
            }

        switch (PRV.RDB$USER_TYPE)
            {
	    case obj_view:
	        sprintf (user_string, "VIEW %s", SQL_identifier);
	        break;
	    case obj_trigger:
	        sprintf (user_string, "TRIGGER %s", SQL_identifier);
	        break;
	    case obj_procedure:
	        sprintf (user_string, "PROCEDURE %s", SQL_identifier);
	        break;
    	    default:
	        strcpy (user_string, SQL_identifier);
	        break;
            }

        /* Only the first character is used for permissions */

        c = PRV.RDB$PRIVILEGE [0];
    
        switch (c)
	    {
	    case 'S':
	        priv_flags |= priv_SELECT;
	        break;
	    case 'I':
	        priv_flags |= priv_INSERT;
	        break;
	    case 'U':
	        priv_flags |= priv_UPDATE;
	        break;
	    case 'D':
	        priv_flags |= priv_DELETE;
	        break;
	    case 'R':
		priv_flags |= priv_REFERENCES;
	        break;
	    case 'X':
	    	/* Execute should not be here -- special handling below */
	        break;
    	    default:
	        priv_flags |= priv_UNKNOWN;
	    }

        /* Column level privileges for update only */
        if (PRV.RDB$FIELD_NAME.NULL)
	    *col_string = '\0';
        else
	    {
	    ISQL_blankterm (PRV.RDB$FIELD_NAME);
	    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
		ISQL_copy_SQL_id (PRV.RDB$FIELD_NAME, &SQL_identifier, 
				  DBL_QUOTE);
		sprintf(col_string, " (%s)", SQL_identifier);
		}
	    else
		sprintf(col_string, " (%s)", PRV.RDB$FIELD_NAME);
	    }

        if (PRV.RDB$GRANT_OPTION)
	    strcpy (with_option, " WITH GRANT OPTION");

    END_FOR
        ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
        END_ERROR;

    /* Print last case if there was anything to print */

    if (prev_option != -1)
        {
        make_priv_string (priv_flags, priv_string);
    	first = FALSE;

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    ISQL_copy_SQL_id (object, &SQL_identifier, DBL_QUOTE);
	else
	    strcpy (SQL_identifier, object);

        sprintf (Print_buffer, "GRANT %s%s ON %s TO %s%s%s%s", 
	         priv_string, col_string,
	         SQL_identifier, user_string, with_option, terminator, NEWLINE);
        ISQL_printf (Out, Print_buffer);
        }

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;

if (!first)
    return (SKIP);

/* No relation called "object" was found, try procedure "object" */

FOR FIRST 1 P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_NAME EQ object;

    /* Part two is for stored procedures only */

    FOR PRV IN RDB$USER_PRIVILEGES CROSS
	    PRC IN RDB$PROCEDURES WITH
	    PRV.RDB$OBJECT_TYPE = obj_procedure AND
	    PRV.RDB$RELATION_NAME EQ object AND
	    PRC.RDB$PROCEDURE_NAME EQ object AND
	    PRV.RDB$PRIVILEGE EQ 'X' AND
	    PRC.RDB$OWNER_NAME NE PRV.RDB$USER
	    SORTED BY  PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION;

    	first = FALSE;
        ISQL_blankterm (PRV.RDB$USER);

        switch (PRV.RDB$USER_TYPE)
            {
	    case obj_relation:
	    case obj_view:
	    case obj_trigger:
	    case obj_procedure:
	    case obj_sql_role:
		if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		    {
		    ISQL_copy_SQL_id (PRV.RDB$USER, &SQL_identifier, DBL_QUOTE);
		    }
		else
		    strcpy (SQL_identifier, PRV.RDB$USER);
		break;
	    default:
		strcpy (SQL_identifier, PRV.RDB$USER);
		break;
            }

        switch (PRV.RDB$USER_TYPE)
            {
	    case obj_view:
	        sprintf (user_string, "VIEW %s", SQL_identifier);
	        break;
	    case obj_trigger:
	        sprintf (user_string, "TRIGGER %s", SQL_identifier);
	        break;
	    case obj_procedure:
	        sprintf (user_string, "PROCEDURE %s", SQL_identifier);
	        break;
    	    default:
	        strcpy (user_string, SQL_identifier);
	        break;
            }

        if (PRV.RDB$GRANT_OPTION)
	    strcpy (with_option, " WITH GRANT OPTION");
        else 
	    with_option[0] = '\0';

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (object, &SQL_identifier, DBL_QUOTE);
	    }
	else
	    strcpy (SQL_identifier, object);

	sprintf (Print_buffer, "GRANT EXECUTE ON PROCEDURE %s TO %s%s%s%s", 
		SQL_identifier, user_string, with_option, terminator, NEWLINE);
	first = FALSE;
	ISQL_printf (Out, Print_buffer);

    END_FOR
        ON_ERROR
	    ISQL_errmsg (isc_status);
	    return (ERR);
        END_ERROR
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return (ERR);
    END_ERROR;

if (!first)
	return (SKIP);

/* No procedure called "object" was found, try role "object" */

FOR FIRST 1 R IN RDB$ROLES WITH R.RDB$ROLE_NAME EQ object;

FOR PRV IN RDB$USER_PRIVILEGES WITH
    PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
    PRV.RDB$USER_TYPE     EQ obj_user     AND
    PRV.RDB$RELATION_NAME EQ object       AND
    PRV.RDB$PRIVILEGE     EQ 'M'
    SORTED BY  PRV.RDB$USER

    ISQL_blankterm (PRV.RDB$RELATION_NAME);
    strcpy (role_name, PRV.RDB$RELATION_NAME);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	ISQL_copy_SQL_id (role_name, &SQL_identifier, DBL_QUOTE);
    else
	strcpy (SQL_identifier, role_name);

    ISQL_blankterm (PRV.RDB$USER);
    strcpy (user_string, PRV.RDB$USER);

    if (PRV.RDB$GRANT_OPTION)
        strcpy (with_option, " WITH ADMIN OPTION");
    else
        with_option [0] = '\0';

    sprintf (Print_buffer, "GRANT %s TO %s%s%s%s", SQL_identifier,
             user_string, with_option, terminator, NEWLINE);

    first = FALSE;
    ISQL_printf (Out, Print_buffer);

    END_FOR
        ON_ERROR
        ISQL_errmsg (isc_status);
        return (ERR);
        END_ERROR;

END_FOR
    ON_ERROR
    ISQL_errmsg (isc_status);
    return (ERR);
    END_ERROR;

if (first)
    return (NOT_FOUND);

return (SKIP);
}

void SHOW_grant_roles (
    SCHAR	*terminator)
{
/**************************************
 *
 *	S H O W _ g r a n t _ r o l e s
 *
 **************************************
 *
 * Functional description
 *	Show grants for given role name
 *	This function is also called by extract for privileges.
 *	All membership privilege may have the with_admin option set.
 *
 **************************************/
SCHAR	with_option [18]; 
SCHAR	user_string [44];
SCHAR	role_name [44];

/* process role "object" */

FOR PRV IN RDB$USER_PRIVILEGES WITH
    PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
    PRV.RDB$USER_TYPE     EQ obj_user     AND
    PRV.RDB$PRIVILEGE     EQ 'M'
    SORTED BY  PRV.RDB$RELATION_NAME, PRV.RDB$USER

    ISQL_blankterm (PRV.RDB$USER);
    strcpy (user_string, PRV.RDB$USER);

    if (PRV.RDB$GRANT_OPTION)
        strcpy (with_option, " WITH ADMIN OPTION");
    else
        with_option [0] = '\0';

    ISQL_blankterm (PRV.RDB$RELATION_NAME);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (PRV.RDB$RELATION_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "GRANT %s TO %s%s%s%s", SQL_identifier,
		 user_string, with_option, terminator, NEWLINE);
	}
    else
	sprintf (Print_buffer, "GRANT %s TO %s%s%s%s", PRV.RDB$RELATION_NAME,
		 user_string, with_option, terminator, NEWLINE);

    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
    END_ERROR;

}

void SHOW_print_metadata_text_blob (
    IB_FILE	*fp,
    ISC_QUAD	*blobid
    )
{
/**************************************
 *
 *	S H O W _ p r i n t _ m e t a d a t a _ t e x t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Print a Blob that is known to be metadata text.
 *
 **************************************/
isc_blob_handle		blob_handle;
USHORT			length;
UCHAR			*buffer, *b, *start;

/* Don't bother with null blobs */

if (blobid->isc_quad_high == 0 && blobid->gds_quad_low == 0)
    return;

blob_handle = NULL;
if (isc_open_blob2 (isc_status, &DB, &gds__trans, &blob_handle, blobid, 
			sizeof (metadata_text_bpb), metadata_text_bpb))
    {
    ISQL_errmsg (isc_status);
    return;
    }

buffer = (UCHAR *) ISQL_ALLOC (BUFFER_LENGTH512);
if (!buffer)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;	
    ISQL_errmsg (isc_status);
    return;
    }

while (!isc_get_segment (isc_status, &blob_handle, &length,
			(USHORT) (BUFFER_LENGTH512 - 1), buffer) || 
	isc_status [1] == isc_segment)
    {
/*
 * Need to step through the buffer until we hit a '\n' and then print the
 * line.  This is because '\n' are included in blob text.
*/	
#ifdef GUI_TOOLS
    buffer [length--] = 0;
    for (b = buffer, start = buffer; b <= buffer + length; b++)
        {
        if (*b == '\n')
    	     {
	     *b = 0;
	     sprintf (Print_buffer, "%s%s", start, NEWLINE);
    	     ISQL_printf (fp, Print_buffer);
	     start = b + 1;
    	     }
        }
    	 sprintf (Print_buffer, "%s", start);
    	 ISQL_printf (fp, Print_buffer);
#else
    buffer [length] = 0;
    ISQL_printf (fp, buffer);	      
#endif	
    }
#ifdef GUI_TOOLS
ISQL_printf (fp, NEWLINE);
#endif

if (isc_status [1] && isc_status [1] != isc_segstr_eof)
    ISQL_errmsg (isc_status);

isc_close_blob (isc_status, &blob_handle);
ISQL_FREE (buffer);
}

int SHOW_metadata (
    SCHAR	**cmd,
    SCHAR	**lcmd)
{
/**************************************
 *
 *	S H O W _ m e t a d a t a
 *
 **************************************
 *
 * Functional description
 *	If somebody presses the show ..., come here to
 *	interpret the desired command.
 *	Paramters:
 *	cmd -- Array of words for the command
 *
 **************************************/
int	ret = SKIP;
TEXT	msg_string[MSG_LENGTH];
TEXT	key_string[MSG_LENGTH];
int	key = 0;

/* Can't show nothing, return an error */

*key_string = '\0';

if (!cmd [1])
    return (ERR);

/* Only show version works if there is no db attached */

if ((!strcmp (cmd [1], "VERSION")) ||
         (!strcmp (cmd [1], "VER")))
    {
    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, VERSION, sizeof (msg_string),
	     msg_string, GDS_VERSION, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    sprintf(Print_buffer, "%s%s", msg_string, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    isc_version (&DB, (FPTR_VOID)local_fprintf, NULL);
    }
else if (!strcmp (cmd [1], "SQL"))
    {
    if (!strcmp (cmd [2], "DIALECT"))
	ret = show_dialect ();
    else
	ret = ERR;
    }
else if (!ISQL_dbcheck())
    /* Do nothing */;
else if ((!strcmp (cmd [1], "ROLE")) || 
    (!strcmp (cmd [1], "ROLES")))
    {
    if (major_ods >= ODS_VERSION9)
       {
        if (*cmd [2])
        {
        if (*cmd [2] == '"')
            {
            remove_delimited_double_quotes (lcmd [2]);
            ret = show_role (lcmd [2]);
            }
        else
            ret = show_role (cmd [2]);

        if (ret == NOT_FOUND)
            key = NO_ROLE;
        }
        else
        {
        ret = show_role (NULL);
        if (ret == NOT_FOUND)
            key = NO_ROLES;
    	}
       }
    }
else if ((!strcmp (cmd [1], "TABLE")) || 
    (!strcmp (cmd [1], "TABLES")))
    {
    if (*cmd [2])
	{
	if (*cmd [2] == '"')
	    {
	    remove_delimited_double_quotes (lcmd [2]);
	    ret = show_table (lcmd [2]);
	    }
	else
	    ret = show_table (cmd [2]);

	if (ret == NOT_FOUND)
	    key = NO_TABLE;
	}
    else
	{
	ret = show_all_tables (0);
	if (ret == NOT_FOUND)
	    key = NO_TABLES;
	}
    }

else if (!strcmp (cmd [1], "VIEW") ||
         !strcmp (cmd [1], "VIEWS"))
	{
    if (*cmd [2])
    {
    if (*cmd[2] == '"')
        {
        remove_delimited_double_quotes (lcmd [2]);
        ret = show_table (lcmd [2]);
        }
    else
        ret = show_table (cmd [2]);

    if (ret == NOT_FOUND)
        key = NO_VIEW;
    }
    else
    {
    ret = show_all_tables (-1);
    if (ret == NOT_FOUND)
        key = NO_VIEWS;
    }
    }
        
else if ((!strcmp (cmd [1], "SYSTEM")) ||
         (!strcmp (cmd [1], "SYS")))
    show_all_tables (1);
else if ((!strcmp (cmd [1], "INDEX")) ||
         (!strcmp (cmd [1], "IND")) || 
         (!strcmp (cmd [1], "INDICES")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_indices (lcmd);
	}
    else
	ret = show_indices (cmd);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        {
        FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ cmd [2];
	    key = NO_INDICES_ON_REL;
        END_FOR
	    ON_ERROR
	    /* Ignore any error */
	    END_ERROR;
        if (!key)
            key = NO_REL_OR_INDEX;
        }
      else
        key = NO_INDICES;
      }
    }
else if ((!strcmp (cmd [1], "DOMAIN")) ||
         (!strcmp (cmd [1], "DOMAINS")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_domains (lcmd [2]);
	}
    else
	ret = show_domains (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_DOMAIN;
      else
        key = NO_DOMAINS;
      }
    }
else if ((!strcmp (cmd [1], "EXCEPTION")) ||
         (!strcmp (cmd [1], "EXCEPTIONS")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_exceptions (lcmd [2]);
	}
    else
	ret = show_exceptions (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_EXCEPTION;
      else
	key = NO_EXCEPTIONS;
      }
    }
else if ((!strcmp (cmd [1], "FILTER")) ||
         (!strcmp (cmd [1], "FILTERS")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_filters (lcmd [2]);
	}
    else
	ret = show_filters (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_FILTER;
      else
        key = NO_FILTERS;
      }
    }
else if ((!strcmp (cmd [1], "FUNCTION")) ||
         (!strcmp (cmd [1], "FUNCTIONS")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_functions (lcmd [2]);
	}
    else
	ret = show_functions (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_FUNCTION;
      else
        key = NO_FUNCTIONS;
      }
    }
else if ((!strcmp (cmd [1], "GENERATOR")) ||
         (!strcmp (cmd [1], "GENERATORS")) ||
         (!strcmp (cmd [1], "GEN")))
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_generators (lcmd [2]);
	}
    else
	ret = show_generators (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_GEN;
      else
        key = NO_GENS;
      }
    }
else if ((!strcmp (cmd [1], "GRANT")) || 
         (!strcmp (cmd [1], "GRANTS")))
    {
    if (*cmd [2])
	{
	if (*cmd [2] == '"')
	    {
	    remove_delimited_double_quotes (lcmd [2]);
	    strcpy (SQL_id_for_grant, lcmd [2]);
	    }
	else
	    strcpy (SQL_id_for_grant, cmd [2]);
	}
    else
	strcpy (SQL_id_for_grant, cmd [2]);

    ret = SHOW_grants (SQL_id_for_grant, "");
    
    if (ret == NOT_FOUND)
      {
      FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ 
					  SQL_id_for_grant;
	  key = NO_GRANT_ON_REL;
      END_FOR
	  ON_ERROR
	  /* Ignore any error */
	  END_ERROR;
      if (!key)
        {
        FOR FIRST 1 P IN RDB$PROCEDURES WITH P.RDB$PROCEDURE_NAME EQ 
					     SQL_id_for_grant;
	    key = NO_GRANT_ON_PROC;
        END_FOR
	    ON_ERROR
	    /* Ignore any error */
	    END_ERROR;
        }
      if (!key)
        {
        FOR FIRST 1 R IN RDB$ROLES WITH R.RDB$ROLE_NAME EQ SQL_id_for_grant;
	    key = NO_GRANT_ON_ROL;
        END_FOR
	    ON_ERROR
	    /* Ignore any error */
	    END_ERROR;
        }
      if (!key)
            key = NO_REL_OR_PROC_OR_ROLE;
      }
    }
else if ((!strcmp (cmd [1], "PROCEDURE")) ||
         (!strcmp (cmd [1], "PROC")) || 
         (!strcmp (cmd [1], "PROCEDURES")))  
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_proc (lcmd [2]);
	}
    else
	ret = show_proc (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (*cmd [2])
        key = NO_PROC;
      else
        key = NO_PROCS;
      }
    }
else if ((!strcmp (cmd [1], "TRIGGER")) || 
         (!strcmp (cmd [1], "TRIG")) || 
         (!strcmp (cmd [1], "TRIGGERS")))  
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_trigger (lcmd [2], SHOW_SOURCE);
	}
    else
	ret = show_trigger (cmd [2], SHOW_SOURCE);

    if (ret == NOT_FOUND)
      if (*cmd [2])
        {
        FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ cmd [2];
	    key = NO_TRIGGERS_ON_REL;
        END_FOR
	    ON_ERROR
	    /* Ignore any error */
	    END_ERROR;
        if (!key)
	    key = NO_REL_OR_TRIGGER;
        }
      else
        key = NO_TRIGGERS;
    }
else if (!strcmp (cmd [1], "CHECK")) 
    {
    if (*cmd [2] == '"')
	{
	remove_delimited_double_quotes (lcmd [2]);
	ret = show_check (lcmd [2]);
	}
    else
	ret = show_check (cmd [2]);

    if (ret == NOT_FOUND)
      {
      if (* cmd [2])
        {
        FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ cmd [2];
	    key = NO_CHECKS_ON_REL;
        END_FOR
	    ON_ERROR
	    /* Ignore any error */
	    END_ERROR;
        }
      if (!key)
          key = NO_TABLE;
      }
    }
else if ((!strcmp (cmd [1], "DATABASE")) || 
         (!strcmp (cmd [1], "DB")))
    show_db();
else
    return (ERR);

if (ret == NOT_FOUND)
{
    if (*cmd [2] == '"')
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, key, sizeof (msg_string), 
			 msg_string, lcmd [2], NULL_PTR, NULL_PTR, 
			 NULL_PTR, NULL_PTR);
    else
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, key, sizeof (msg_string), 
			 msg_string, cmd [2], NULL_PTR, NULL_PTR, NULL_PTR, 
			 NULL_PTR);
    STDERROUT (msg_string, 1);
}

return (ret);
}

static void *local_fprintf (
    UCHAR	*format,
    UCHAR	*string)
{
/**************************************
 *
 *	l o c a l _ p r i n t f
 *
 **************************************
 *
 * Functional description
 *	Used to make sure that local calls to print stuff go to Out
 *	and not to ib_stdout if gds__version gets called.
 *
 **************************************/

ib_fprintf (Out, "%s", string);
ib_fprintf (Out, "%s", NEWLINE);

#ifdef GUI_TOOLS
PaintOutputWindow (string);
PaintOutputWindow (NEWLINE);
#endif 
}

static SCHAR *remove_delimited_double_quotes (
    TEXT	*string)
{
/**************************************
 *
 *	r e m o v e _ d e l i m i t e d _ d o u b l e _ q u o t e s
 *
 **************************************
 *
 * Functional description
 *	remove the delimited double quotes. Blanks could be part of
 *	delimited SQL identifier.
 *
 **************************************/
UCHAR	*p, *q, *next_char, *end_of_str;
USHORT	cmd_len, cnt;

p = string;
q = string;
cmd_len = strlen (string);
end_of_str = string + cmd_len;

for (cnt = 1; cnt < cmd_len && p < end_of_str; cnt++)
    {
    p++;
    if (cnt < cmd_len -1)
	{
	*q = *p;
	if (p + 1 < end_of_str)
	    {
	    next_char = p + 1;
	    if (*next_char == DBL_QUOTE) /* skip the escape double quote */
		p++;
	    }
	else 
	    {
	    p++;
	    *q = '\0';
	    }
	}
    else
	*q = '\0';
    q++;
    }
*q = '\0';
}

static void make_priv_string (
    USHORT	flags,
    UCHAR	*string)
{
/**************************************
 *
 *	m a k e _ p r i v _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Given a bit-vector of privileges, turn it into a 
 *	string list.
 *
 **************************************/
USHORT	i;

for (i = 0; privs [i].priv_string; i++)
    {
    if (flags & privs [i].priv_flag)
	{
	if (*string)
	    strcat (string, ", ");
	strcat (string, privs [i].priv_string);
	}
    }
}

static int show_all_tables (
    SSHORT	flag)
{
/**************************************
 *
 *	s h o w _ a l l _ t a b l e s
 *
 **************************************
 *
 *	Print the names of all user tables from
 *	rdb$relations.  We use a dynamic query 
 *
 *	Parameters:  flag -- 0, show user tables
 *	1, show system tables only
 *
 **************************************/
SSHORT	odd = 1;
SSHORT	first = TRUE;

if (flag == -1)
    {
	/* Views */
    FOR REL IN RDB$RELATIONS WITH
	REL.RDB$VIEW_BLR NOT MISSING
	SORTED BY REL.RDB$RELATION_NAME
	
	first = FALSE;
        sprintf (Print_buffer, "%38s%s", REL.RDB$RELATION_NAME, (odd ? " " : NEWLINE));
	ISQL_printf (Out, Print_buffer);
        odd = 1 - odd;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
        END_ERROR;
    }
else
    {
   FOR REL IN RDB$RELATIONS WITH
	REL.RDB$SYSTEM_FLAG EQ flag
	SORTED BY REL.RDB$RELATION_NAME

	first = FALSE;
        sprintf (Print_buffer, "%38s%s", REL.RDB$RELATION_NAME, (odd ? " " : NEWLINE));
	ISQL_printf (Out, Print_buffer);
        odd = 1 - odd;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
        END_ERROR;
    }

if (!first)
    {
    ISQL_printf (Out, NEWLINE);
    return SKIP;
    }
else
    return NOT_FOUND;
}

#ifdef GUI_TOOLS
void   SHOW_query_database (
    int	*tables,
    int	*views,
    int	*indices)
{
/**************************************
 *
 *	 S H O W _ q u e r y _ d a t a b a s e 
 *
 **************************************
 *
 *	Query RDB$RELATIONS to determine if
 *	the database has tables, views and indices
 *	defined.
 *
 **************************************/

*tables   = 0;
*views    = 0;
*indices  = 0;

/* views */ 
FOR FIRST 1 REL IN RDB$RELATIONS WITH
    REL.RDB$VIEW_BLR NOT MISSING AND
    REL.RDB$FLAGS = REL_sql
    {	
    	*views = 1;
    }
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;

FOR FIRST 1 REL IN RDB$RELATIONS WITH
    REL.RDB$SYSTEM_FLAG EQ 0 AND
    REL.RDB$VIEW_BLR MISSING AND
    REL.RDB$FLAGS = REL_sql
    {
    	*tables = 1;
    }
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;
return;
}
#endif

#ifdef GUI_TOOLS
void   SHOW_build_table_namelist (
    TBLLIST	**tbl_list)
{
/**************************************
 *
 *	 b u i l d _ t a b l e _ n a m e l i s t 
 *
 **************************************
 *
 *	Build a list of all user tables from
 *	rdb$relations.  We use a dynamic query 
 *
 **************************************/
SSHORT		 length = 0, save_length = 0;
TBLLIST		*templist;
TBLLIST		*tbl_node;

templist = *tbl_list;
/* Tables */
FOR REL IN RDB$RELATIONS WITH
	REL.RDB$SYSTEM_FLAG EQ 0 AND
        REL.RDB$VIEW_BLR MISSING AND
        REL.RDB$FLAGS = REL_sql
	SORTED BY REL.RDB$RELATION_NAME
	
    sprintf (Print_buffer, "%s", REL.RDB$RELATION_NAME);

    tbl_node = (TBLLIST *) ISQL_ALLOC ((SLONG) sizeof(TBLLIST));
    if (tbl_node)
	{
 	/* remove whitespace at end of table name */
	length = strlen (Print_buffer) - 1;
	save_length = length + 1;
        while (Print_buffer[length] == ' ' && length >= 0)
	    length--;
	Print_buffer[length + 1] = NULL;
	if (save_length > TBLNAME_LENGTH)
	    {	
	    strncpy (tbl_node->szName, Print_buffer, TBLNAME_LENGTH);
	    tbl_node->szName[TBLNAME_LENGTH] = NULL;	
	    }	
	else
	    strcpy (tbl_node->szName, Print_buffer);
	tbl_node->pNext = NULL;
 	if (templist == NULL)
	    *tbl_list	  = tbl_node;
	else
	    templist->pNext = tbl_node;
	templist            = tbl_node;	
	}
    else
	{
    	isc_status [0] = isc_arg_gds;
    	isc_status [1] = isc_virmemexh;
    	isc_status [2] = isc_arg_end;	
    	ISQL_errmsg (isc_status);
    	return;
    	}
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;
}
#endif

#ifdef GUI_TOOLS
void   SHOW_build_view_namelist (
    VIEWLIST	**view_list)
{
/**************************************
 *
 *	 b u i l d _ v i e w _ n a m e l i s t 
 *
 **************************************
 *
 *	Build a list of all user views from
 *	rdb$relations.  We use a dynamic query 
 *
 **************************************/
SSHORT		 length = 0, save_length = 0;
VIEWLIST	*templist;
VIEWLIST	*view_node;

templist = *view_list;
/* Views */
FOR REL IN RDB$RELATIONS WITH
    REL.RDB$VIEW_BLR NOT MISSING AND
    REL.RDB$FLAGS = REL_sql
    SORTED BY REL.RDB$RELATION_NAME
	
    sprintf (Print_buffer, "%s", REL.RDB$RELATION_NAME);

    view_node = (VIEWLIST *) ISQL_ALLOC ((SLONG) sizeof(VIEWLIST));
    if (view_node)
	{
 	/* remove whitespace at end of view name */
	length = strlen (Print_buffer) - 1;
	save_length = length + 1;
        while (Print_buffer[length] == ' ' && length >= 0)
	    length--;
	Print_buffer[length + 1] = NULL;
	if (save_length > TBLNAME_LENGTH)
	    {	
	    strncpy (view_node->szName, Print_buffer, TBLNAME_LENGTH);
	    view_node->szName[TBLNAME_LENGTH] = NULL;	
	    }	
	else
	    strcpy (view_node->szName, Print_buffer);
	view_node->pNext = NULL;
 	if (templist == NULL)
	    *view_list	  = view_node;
	else
	    templist->pNext = view_node;
	templist            = view_node;	
	}
    else
	{
    	isc_status [0] = isc_arg_gds;
    	isc_status [1] = isc_virmemexh;
    	isc_status [2] = isc_arg_end;	
    	ISQL_errmsg (isc_status);
    	return;
    	}
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;
}
#endif

static void show_charsets (
    SCHAR	*relation_name,
    SCHAR	*field_name)
{
/************************************* 
*
*	s h o w _ c h a r s e t s
*
**************************************
*
* Functional description
*	Show character set and collations for V4 dbs only.
*
**************************************/
SSHORT	collation, char_set_id;
TEXT	char_sets [86];	/* CHARACTER SET <name31> COLLATE <name31> */	

/* If there is a relation_name, this is a real column, look up collation */
/* in rdb$relation_fields */

if (!V33 && relation_name)
    {
    FOR RRF IN RDB$RELATION_FIELDS CROSS
	FLD IN RDB$FIELDS
    WITH RRF.RDB$FIELD_NAME EQ field_name AND
	 RRF.RDB$RELATION_NAME EQ relation_name AND
	 RRF.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME

	char_set_id = 0;
	if (!FLD.RDB$CHARACTER_SET_ID.NULL)
	    char_set_id = FLD.RDB$CHARACTER_SET_ID;
	collation = 0;
        if (!RRF.RDB$COLLATION_ID.NULL)
	    collation = RRF.RDB$COLLATION_ID;
        else if (!FLD.RDB$COLLATION_ID.NULL)
	    collation = FLD.RDB$COLLATION_ID;
    END_FOR
	ON_ERROR
#ifdef DEV_BUILD
	ib_fprintf (ib_stderr, "show_charsets(%s %s) failed\n", 
		 relation_name, 
		 field_name);
#endif
	END_ERROR;
    }
else if (!V33)
    {
    FOR FLD IN RDB$FIELDS WITH
        FLD.RDB$FIELD_NAME EQ field_name

        char_set_id = 0;
        collation = 0;
        if (!FLD.RDB$CHARACTER_SET_ID.NULL)
	    char_set_id = FLD.RDB$CHARACTER_SET_ID;
        if (!FLD.RDB$COLLATION_ID.NULL)
	    collation = FLD.RDB$COLLATION_ID;
    END_FOR
        ON_ERROR
#ifdef DEV_BUILD
	ib_fprintf (ib_stderr, "show_charsets(NULL %s) failed\n", field_name);
#endif
        END_ERROR;
    }

char_sets [0] = 0;
ISQL_get_character_sets (char_set_id, collation, FALSE, char_sets);
if (char_sets [0])
    ISQL_printf(Out, char_sets);
}

static show_check (
    SCHAR	*object)
{
/**************************************
 *
 *	s h o w _ c h e c k
 *
 **************************************
 *
 * Functional description
 *	Show check constraints for the named object
 *
 **************************************/
int	first = TRUE;
/* Only V4 databases have this information */

if (!V4)
    return (SKIP);

if (!*object)
    return (ERR);
/* Query gets the check clauses for triggers stored for check constraints */

FOR TRG IN RDB$TRIGGERS CROSS 
	CHK IN RDB$CHECK_CONSTRAINTS WITH
	TRG.RDB$TRIGGER_TYPE EQ 1 AND
	TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME AND
	CHK.RDB$TRIGGER_NAME STARTING WITH "CHECK" AND
	TRG.RDB$RELATION_NAME EQ object
	SORTED BY CHK.RDB$CONSTRAINT_NAME

    /* Use print_blob to print the blob */
    first = FALSE;

    sprintf (Print_buffer, "CONSTRAINT %s:%s  ", ISQL_blankterm(CHK.RDB$CONSTRAINT_NAME), NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (!TRG.RDB$TRIGGER_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &TRG.RDB$TRIGGER_SOURCE);
    ISQL_printf (Out, NEWLINE);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return (ERR);
    END_ERROR;
if (first)
    return (NOT_FOUND);
return (SKIP);
}

static void show_db (void)
{
/**************************************
 *
 *	s h o w _ d b
 *
 **************************************
 *
 * Functional description
 *	Show info on this database.  cache, logfiles, etc
 *
 **************************************/
SCHAR	*info_buf;
SSHORT	is_wal = FALSE;
USHORT   translate = TRUE;

/* First print the name of the database */

sprintf (Print_buffer, "Database: %s%s", Db_name, NEWLINE);
ISQL_printf (Out, Print_buffer);
/* Get the owner name */
FOR REL IN RDB$RELATIONS WITH
    REL.RDB$RELATION_NAME = "RDB$DATABASE"

    if (!REL.RDB$OWNER_NAME.NULL);
	{
	sprintf (Print_buffer, "%sOwner: %s%s", TAB, REL.RDB$OWNER_NAME, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;

/* Query for files */

FOR FIL IN RDB$FILES SORTED BY FIL.RDB$SHADOW_NUMBER, FIL.RDB$FILE_SEQUENCE,
	FIL.RDB$FILE_SEQUENCE

    /* reset nulls to zero */

    if (FIL.RDB$FILE_FLAGS.NULL)
	FIL.RDB$FILE_FLAGS = 0;
    if (FIL.RDB$FILE_LENGTH.NULL)
	FIL.RDB$FILE_LENGTH = 0;
    if (FIL.RDB$FILE_SEQUENCE.NULL)
	FIL.RDB$FILE_SEQUENCE = 0;
    if (FIL.RDB$FILE_START.NULL)
	FIL.RDB$FILE_START = 0;
/*
This line used to terminate the file name at the first space.  This does
not seem necessary, and creates problems since Windows 95 allows embeded
spaces in file names.

    ISQL_blankterm (FIL.RDB$FILE_NAME);
*/

    if (FIL.RDB$FILE_FLAGS == 0)
	{
	sprintf (Print_buffer, " File %d: \"%s\", length %ld, start %ld%s", 
	    FIL.RDB$FILE_SEQUENCE, FIL.RDB$FILE_NAME,
	    FIL.RDB$FILE_LENGTH, FIL.RDB$FILE_START, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

    if (FIL.RDB$FILE_FLAGS & FILE_cache)
	{
	sprintf (Print_buffer, " Cache: %s, length %ld%s",
	    FIL.RDB$FILE_NAME, FIL.RDB$FILE_LENGTH, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
	
    if (FIL.RDB$FILE_FLAGS & FILE_shadow)
	{
	if (FIL.RDB$FILE_SEQUENCE)
	    {
	    sprintf (Print_buffer, "%sfile %s ", TAB, FIL.RDB$FILE_NAME);
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, " Shadow %d: \"%s\" ", 
	        FIL.RDB$SHADOW_NUMBER, FIL.RDB$FILE_NAME);
	    ISQL_printf (Out, Print_buffer);
	    if (FIL.RDB$FILE_FLAGS & FILE_inactive)
		{
	        sprintf (Print_buffer, "inactive ");
		ISQL_printf (Out, Print_buffer);
		}
	    if (FIL.RDB$FILE_FLAGS & FILE_manual)
		{
	        sprintf (Print_buffer, "manual ");
		ISQL_printf (Out, Print_buffer);
		}
	    else 
		{
		sprintf (Print_buffer, "auto ");
		ISQL_printf (Out, Print_buffer);
		}
	    if (FIL.RDB$FILE_FLAGS & FILE_conditional)
		{
	        sprintf (Print_buffer, "conditional ");
		ISQL_printf (Out, Print_buffer);
		}
	    }
	if (FIL.RDB$FILE_LENGTH)
	    {
	    sprintf (Print_buffer, "length %ld ", FIL.RDB$FILE_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }
	if (FIL.RDB$FILE_START)
	    {
	    sprintf (Print_buffer, "starting %ld", FIL.RDB$FILE_START);
	    ISQL_printf (Out, Print_buffer);
	    }
	ISQL_printf (Out, NEWLINE);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;

info_buf = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH400);
if (!info_buf)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;	
    ISQL_errmsg (isc_status);
    return;
    }

/* First general database parameters */

SHOW_dbb_parameters (DB, info_buf, db_items, sizeof (db_items), translate);
ISQL_printf (Out, info_buf);

/* Only v4 databases have log_files */

if (!V4)	
    {
    ISQL_FREE (info_buf);
    return;
    }

FOR LOG IN RDB$LOG_FILES SORTED BY LOG.RDB$FILE_FLAGS, LOG.RDB$FILE_SEQUENCE
    SORTED BY LOG.RDB$FILE_FLAGS, LOG.RDB$FILE_SEQUENCE

    /* If there are log files, print Wal statistics */

    is_wal = TRUE;

    /* reset nulls to zero */

    if (LOG.RDB$FILE_FLAGS.NULL)
	LOG.RDB$FILE_FLAGS = 0;
    if (LOG.RDB$FILE_LENGTH.NULL)
	LOG.RDB$FILE_LENGTH = 0;
    if (LOG.RDB$FILE_SEQUENCE.NULL)
	LOG.RDB$FILE_SEQUENCE = 0;
    ISQL_blankterm (LOG.RDB$FILE_NAME);

    if (LOG.RDB$FILE_FLAGS & LOG_overflow)
	{
	sprintf (Print_buffer, "overflow: %s, ", LOG.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}
    else if (LOG.RDB$FILE_FLAGS & LOG_serial)
	{
	sprintf (Print_buffer, "Logfile serial: %s, ", LOG.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}
    else if (LOG.RDB$FILE_FLAGS & LOG_default)
	{
	sprintf (Print_buffer, "Logfile default, ");
	ISQL_printf (Out, Print_buffer);
	}
    else if (LOG.RDB$FILE_FLAGS & LOG_raw)
	{
	sprintf (Print_buffer, "Logfile raw: %s, %d partitions, ", LOG.RDB$FILE_NAME, LOG.RDB$FILE_PARTITIONS);
	ISQL_printf (Out, Print_buffer);
	}
    else
	{
	sprintf (Print_buffer, "Logfile #%d: %s, ", LOG.RDB$FILE_SEQUENCE, LOG.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}
    sprintf (Print_buffer, "length %ld%s", LOG.RDB$FILE_LENGTH, NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	if (info_buf)
    	    ISQL_FREE (info_buf);
	return;
    END_ERROR;

/* And now for Rich's show db parameters */

if (is_wal)
   {
   SHOW_dbb_parameters (DB, info_buf, wal_items, sizeof (wal_items), translate);
   ISQL_printf (Out, info_buf);
   }

if (V4)
    {
    FOR DBB IN RDB$DATABASE 
	WITH DBB.RDB$CHARACTER_SET_NAME NOT MISSING
	 AND DBB.RDB$CHARACTER_SET_NAME NE " ";
	 sprintf (Print_buffer, "Default Character set: %s%s", 
		  DBB.RDB$CHARACTER_SET_NAME,
		  NEWLINE);
	 ISQL_printf (Out, Print_buffer);
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    if (info_buf)
		ISQL_FREE (info_buf);
	    return;
	END_ERROR;
    }

ISQL_FREE (info_buf);
}

static int show_dialect (void)
{
/**************************************
 *
 *	s h o w _ d i a l e c t
 *
 **************************************
 *
 *	Print out the SQL dialect information 
 *
 **************************************/

    if (db_SQL_dialect > 0)
	sprintf (Print_buffer, "%38s%d%s%d", 
		"Client SQL dialect is set to: ", SQL_dialect, 
		" and database SQL dialect is: ", db_SQL_dialect);
    else
	if (SQL_dialect == 0)
	    sprintf (Print_buffer, "%38s%s", 
		     "Client SQL dialect has not been set",
		     " and no database has been connected yet.");
	else 
	    sprintf (Print_buffer, "%38s%d%s", 
		     "Client SQL dialect is set to: ", SQL_dialect, 
		     ". No database has been connected.");

    ISQL_printf (Out, Print_buffer);
    ISQL_printf (Out, NEWLINE);
    return SKIP;
}

static int show_domains (
    SCHAR 	*domain_name)
{
/************************************* 
*
*	s h o w _ d o m a i n s
*
**************************************
*
* Functional description
*	Show all domains or the named domain
************************************/
SSHORT 		odd = 1, i;
ISC_QUAD	default_source;
SSHORT		subtype, first = TRUE;;

if (!*domain_name)
    {
    /*  List all domain names in columns */
    if (V4)
	{
	FOR FLD IN RDB$FIELDS WITH
	    FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
	    AND FLD.RDB$SYSTEM_FLAG NE 1
	    SORTED BY FLD.RDB$FIELD_NAME

	    first = FALSE;
	    sprintf (Print_buffer, "%38s%s", FLD.RDB$FIELD_NAME, (odd ? " " : NEWLINE));
	    ISQL_printf (Out, Print_buffer);
	    odd = 1 - odd;
	END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;
	}
    else
	{
	/* V3 databases will list all gdml fields as well */
	FOR FLD IN RDB$FIELDS WITH
	    FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
	    AND FLD.RDB$SYSTEM_FLAG NE 1
	    SORTED BY FLD.RDB$FIELD_NAME

	    if (strncmp (FLD.RDB$FIELD_NAME, "RDB$", 4) == 0 &&
		isdigit (FLD.RDB$FIELD_NAME[4]))
		continue;

	    first = FALSE;
	    sprintf (Print_buffer, "%38s%s", FLD.RDB$FIELD_NAME, (odd ? " " : NEWLINE));
	    ISQL_printf (Out, Print_buffer);
	    odd = 1 - odd;
	END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;
	}
	if (!first)
	    ISQL_printf (Out, NEWLINE);
    }
else  /* List named domain */
    {
    FOR FLD IN RDB$FIELDS WITH
	FLD.RDB$FIELD_NAME EQ domain_name;

	first = FALSE;
	/* Print the name of the domain */
	ISQL_blankterm (FLD.RDB$FIELD_NAME);
	sprintf (Print_buffer, "%-32s", FLD.RDB$FIELD_NAME);
	ISQL_printf (Out, Print_buffer);

	/* Array dimensions */
	if (!FLD.RDB$DIMENSIONS.NULL)
	    {
	    sprintf (Print_buffer, "ARRAY OF ");
	    ISQL_printf (Out, Print_buffer);
	    ISQL_array_dimensions (FLD.RDB$FIELD_NAME);
	    sprintf (Print_buffer, "%s                                ",
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }

    /* Look through types array */

	for (i = 0; Column_types [i].type; i++)
            if (FLD.RDB$FIELD_TYPE == Column_types [i].type)
                {
		BOOLEAN precision_known = FALSE;

		if (major_ods >= ODS_VERSION10)
		    {
		    /* Handle Integral subtypes NUMERIC and DECIMAL */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) ||
			(FLD.RDB$FIELD_TYPE == INTEGER) ||
		        (FLD.RDB$FIELD_TYPE == blr_int64))
		      {
			FOR FLD1 IN RDB$FIELDS WITH
			    FLD1.RDB$FIELD_NAME EQ domain_name;
			  /* We are ODS >= 10 and could be any Dialect */
			  if (!FLD1.RDB$FIELD_PRECISION.NULL)
			      {
			      /* We are Dialect >=3 since FIELD_PRECISION
			         is non-NULL */
				if (FLD1.RDB$FIELD_SUB_TYPE > 0 &&
				    FLD1.RDB$FIELD_SUB_TYPE <= MAX_INTSUBTYPES)
				  {
				  sprintf (Print_buffer, "%s(%d, %d)", 
				    Integral_subtypes [FLD1.RDB$FIELD_SUB_TYPE],
				    FLD1.RDB$FIELD_PRECISION,
				    -FLD1.RDB$FIELD_SCALE);
				  precision_known = TRUE;
				  }
			      }
			END_FOR
			    ON_ERROR
				ISQL_errmsg (isc_status);
				return ERR;
			    END_ERROR;
		      }
		    }

		if (precision_known == FALSE)
                    {
		    /* Take a stab at numerics and decimals */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) && 
			(FLD.RDB$FIELD_SCALE < 0))
			sprintf (Print_buffer, 
				 "NUMERIC(4, %d)", 
				 -FLD.RDB$FIELD_SCALE);

		    else if ((FLD.RDB$FIELD_TYPE == INTEGER) && 
			     (FLD.RDB$FIELD_SCALE < 0))
			sprintf (Print_buffer, 
				 "NUMERIC(9, %d)", 
				 -FLD.RDB$FIELD_SCALE);

		    else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
			     (FLD.RDB$FIELD_SCALE < 0))
			sprintf (Print_buffer, 
				 "NUMERIC(15, %d)", 
				 -FLD.RDB$FIELD_SCALE);
		    else
			sprintf (Print_buffer, "%s", Column_types [i].type_name);
                    }

		ISQL_printf (Out, Print_buffer);

                break;
                }

	/* Length for CHARs */
	if ((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR))
            {
            /* Don't test character_length in a V3 database */
            if (!V4)
		{
                sprintf (Print_buffer, "(%d)", FLD.RDB$FIELD_LENGTH);
		ISQL_printf (Out, Print_buffer);
		}
            else
		{
                sprintf (Print_buffer, "(%d)", ISQL_get_field_length(FLD.RDB$FIELD_NAME));
		ISQL_printf (Out, Print_buffer);
		}
	    }

	/* Blob domains */
	if (FLD.RDB$FIELD_TYPE == BLOB)
	    {
            sprintf (Print_buffer, " segment %u, subtype ", (USHORT) FLD.RDB$SEGMENT_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    subtype  = FLD.RDB$FIELD_SUB_TYPE;
            if (subtype >= 0 && subtype <= MAXSUBTYPES) 
		{
		sprintf (Print_buffer, "%s", Sub_types [subtype]);
		ISQL_printf (Out, Print_buffer);
		}
	    else 
		{
		sprintf (Print_buffer, "%d", subtype);
		ISQL_printf (Out, Print_buffer);
		}
	    }	

	/* Show international character sets and collations */
	if (V4 && (FLD.RDB$FIELD_TYPE == FCHAR || 
		FLD.RDB$FIELD_TYPE == VARCHAR  ||
		FLD.RDB$FIELD_TYPE == BLOB))
	    show_charsets(NULL, FLD.RDB$FIELD_NAME);

	
	if (V4)
	    {
	    if (FLD.RDB$NULL_FLAG != 1)
		{
	        sprintf (Print_buffer, " Nullable");
		ISQL_printf (Out, Print_buffer);
		}
	    else
		{
	        sprintf (Print_buffer, " Not Null");
		ISQL_printf (Out, Print_buffer);
		}
	    }
	ISQL_printf (Out, NEWLINE);
	
        if (V4)
	    {
	    ISQL_get_default_source (NULL, FLD.RDB$FIELD_NAME, 
		&default_source);
	    if (default_source.gds_quad_high)
		{
		sprintf (Print_buffer, "                                ");
		ISQL_printf (Out, Print_buffer);
	        SHOW_print_metadata_text_blob (Out, &default_source);
		ISQL_printf (Out, NEWLINE);
		}
	    }

	if (!FLD.RDB$VALIDATION_SOURCE.NULL)
	    {
	    sprintf (Print_buffer, "                                ");
	    ISQL_printf (Out, Print_buffer);
            SHOW_print_metadata_text_blob (Out, &FLD.RDB$VALIDATION_SOURCE);
	    ISQL_printf (Out, NEWLINE);
	    }

    END_FOR
	ON_ERROR
	    ISQL_errmsg(isc_status);
	    return (ERR);
	END_ERROR;
    }
if (first)
   return(NOT_FOUND);
return(SKIP);
}

static int show_exceptions (
    SCHAR	*object)
{
/**************************************
 *
 *	s h o w _ e x c e p t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show exceptions and their dependencies
 *	This version fetches all the exceptions, and only prints the
 *	one you asked for if you ask for one.  It could be optimized
 *	like other such functions.
 *
 **************************************/
SSHORT	first = TRUE, first_dep = TRUE;
SCHAR	type[20];

FOR EXC IN RDB$EXCEPTIONS 
    SORTED BY EXC.RDB$EXCEPTION_NAME

    ISQL_blankterm (EXC.RDB$EXCEPTION_NAME);
    ISQL_blankterm (object);
    /* List all objects if none specified, or just the named exception */

    if (!*object || !strcmp (EXC.RDB$EXCEPTION_NAME, object))
	{
	if (first)
	    {
	    sprintf (Print_buffer, "Exception Name                  Used by, Type%s=============================== =============================================%s", NEWLINE, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	first = FALSE;

	sprintf (Print_buffer, "%-31s ", EXC.RDB$EXCEPTION_NAME);
	ISQL_printf (Out, Print_buffer);

	/* Look up dependent objects --procedures and triggers */
	first_dep = TRUE;
	FOR DEP IN RDB$DEPENDENCIES WITH
	    DEP.RDB$DEPENDED_ON_TYPE = 7 AND
	    DEP.RDB$DEPENDED_ON_NAME EQ EXC.RDB$EXCEPTION_NAME
	    SORTED BY DEP.RDB$DEPENDENT_TYPE, DEP.RDB$DEPENDENT_NAME

	    if (!first_dep)
		{
		sprintf (Print_buffer, "                                ");
		ISQL_printf (Out, Print_buffer);
		}
	    first_dep = FALSE;

	    ISQL_blankterm (DEP.RDB$DEPENDENT_NAME);

	    strcpy (type, "Unknown");
	    if (DEP.RDB$DEPENDENT_TYPE == 2)
	        strcpy (type, "Trigger");
	    if (DEP.RDB$DEPENDENT_TYPE == 5)
	        strcpy (type, "Stored procedure");

	    sprintf (Print_buffer, "%s, %s%s", 
		     DEP.RDB$DEPENDENT_NAME, 
		     type,
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;

	sprintf (Print_buffer, "%s  %s%s", (first_dep ? NEWLINE : ""), 
		 EXC.RDB$MESSAGE,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    if (!first)
        ISQL_printf (Out, NEWLINE);
END_FOR
    ON_ERROR
	if (!V33)
	    {
	    ISQL_errmsg (isc_status);
	    return ERR;
	    }
    END_ERROR;
if (first)
    return (NOT_FOUND);
return (SKIP);
}

static int show_filters (
    SCHAR	*object)
{
/**************************************
 *
 *	s h o w _ f i l t e r s
 *
 **************************************
 *
 * Functional description
 *	Show blob filters in general or  for the named filters
 *
 **************************************/
SSHORT	odd = 1, first = TRUE;


/* Show all functions */
if (!*object) 
    {
    FOR FIL IN RDB$FILTERS
	SORTED BY FIL.RDB$FUNCTION_NAME

	first = FALSE;
	ISQL_blankterm (FIL.RDB$FUNCTION_NAME);
	sprintf (Print_buffer, "%-38s%s", FIL.RDB$FUNCTION_NAME, (odd ? " " : NEWLINE));
	ISQL_printf (Out, Print_buffer);
	odd = 1 - odd;
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;
    if (!first)
	{
        ISQL_printf (Out, NEWLINE);
        return (SKIP);
	}
    else
	return NOT_FOUND;
    }

/* We have a filter name, so expand on it */
FOR FIL IN RDB$FILTERS WITH
	FIL.RDB$FUNCTION_NAME EQ object

	first = FALSE;

	ISQL_blankterm (FIL.RDB$FUNCTION_NAME);
	ISQL_blankterm (FIL.RDB$MODULE_NAME);
	ISQL_blankterm (FIL.RDB$ENTRYPOINT);

    sprintf (Print_buffer, "BLOB Filter: %s %s%sInput subtype: %d Output subtype: %d%s",
        FIL.RDB$FUNCTION_NAME, NEWLINE, 
	TAB, FIL.RDB$INPUT_SUB_TYPE, FIL.RDB$OUTPUT_SUB_TYPE, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "%sFilter library is %s%s%sEntry point is %s%s%s", 
	TAB, FIL.RDB$MODULE_NAME, NEWLINE,
        TAB, FIL.RDB$ENTRYPOINT, NEWLINE, 
	NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;

if (first)
    return (NOT_FOUND);
return (SKIP);
}

static int show_functions (
    SCHAR	*object)
{
/**************************************
 *
 *	s h o w _ f u n c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show external functions in general or  for the named function
 *
 **************************************/
SSHORT	first = TRUE;
SSHORT	odd = 1;
SSHORT	i;

/* Show all functions */
if (!*object) 
    {
    FOR FUN IN RDB$FUNCTIONS 
	SORTED BY FUN.RDB$FUNCTION_NAME

	first = FALSE;
	ISQL_blankterm (FUN.RDB$FUNCTION_NAME);
	sprintf (Print_buffer, "%-38s%s", FUN.RDB$FUNCTION_NAME, (odd ? " " : NEWLINE));
	ISQL_printf (Out, Print_buffer);
	odd = 1 - odd;
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;
    if (!first)
	{
        ISQL_printf (Out, NEWLINE);
        return (SKIP);
	}
    else
	return NOT_FOUND;
    }
FOR FUN IN RDB$FUNCTIONS CROSS
	FNA IN RDB$FUNCTION_ARGUMENTS WITH
	FUN.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME AND
	FUN.RDB$FUNCTION_NAME EQ object
	SORTED BY FNA.RDB$ARGUMENT_POSITION

	ISQL_blankterm (FUN.RDB$FUNCTION_NAME);
	ISQL_blankterm (FUN.RDB$MODULE_NAME);
	ISQL_blankterm (FUN.RDB$ENTRYPOINT);
	if (first)
	    {
	    sprintf (Print_buffer, "%sFunction %s:%s", 
		     NEWLINE,
		     FUN.RDB$FUNCTION_NAME,
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    sprintf (Print_buffer, "Function library is %s%s", 
		     FUN.RDB$MODULE_NAME,
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    sprintf (Print_buffer, "Entry point is %s%s", 
		     FUN.RDB$ENTRYPOINT,
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	first = FALSE;
	if (FUN.RDB$RETURN_ARGUMENT == FNA.RDB$ARGUMENT_POSITION)
	    {
	    sprintf (Print_buffer, "Returns %s %s ", 
                 ((SSHORT) abs(FNA.RDB$MECHANISM) == FUN_reference ? 
                      "BY REFERENCE " : "BY VALUE "), 
                 (FNA.RDB$MECHANISM < 0 ? "FREE_IT " : ""));
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, "Argument %d: ", FNA.RDB$ARGUMENT_POSITION);
	    ISQL_printf (Out, Print_buffer);
	    }
	for (i = 0; Column_types [i].type; i++)
	    {
            if (FNA.RDB$FIELD_TYPE == Column_types [i].type)
                {
		BOOLEAN precision_known = FALSE;
		
		/* Handle Integral subtypes NUMERIC and DECIMAL */
		if ( (major_ods >= ODS_VERSION10) &&
		     ((FNA.RDB$FIELD_TYPE == SMALLINT) ||
		      (FNA.RDB$FIELD_TYPE == INTEGER) ||
		      (FNA.RDB$FIELD_TYPE == blr_int64)) )
		    {
		    FOR FNA1 IN RDB$FUNCTION_ARGUMENTS WITH
		        FNA1.RDB$FUNCTION_NAME = FNA.RDB$FUNCTION_NAME AND
		        FNA1.RDB$ARGUMENT_POSITION = FNA.RDB$ARGUMENT_POSITION
		      
		      /* We are ODS >= 10 */
		      if (!FNA1.RDB$FIELD_PRECISION.NULL)
			  {
			  /* We are Dialect >=3 since FIELD_PRECISION is
			     non-NULL */
			  if (FNA1.RDB$FIELD_SUB_TYPE > 0 &&
			      FNA1.RDB$FIELD_SUB_TYPE <= MAX_INTSUBTYPES)
			      {
			      sprintf (Print_buffer, "%s(%d, %d)", 
				       Integral_subtypes [FNA1.RDB$FIELD_SUB_TYPE],
				       FNA1.RDB$FIELD_PRECISION,
				       -FNA1.RDB$FIELD_SCALE);
			      precision_known = TRUE;
			      }
			  }
		    END_FOR
		        ON_ERROR
		            ISQL_errmsg (isc_status);
		            return ERR;
		        END_ERROR;
		    }

		if (precision_known == FALSE)
		    {
		    /* Take a stab at numerics and decimals */
		    if ((FNA.RDB$FIELD_TYPE == SMALLINT) && (FNA.RDB$FIELD_SCALE < 0))
		        {
			sprintf (Print_buffer, "NUMERIC(4, %d)", -FNA.RDB$FIELD_SCALE);
		        }
		    else if ((FNA.RDB$FIELD_TYPE == INTEGER) && (FNA.RDB$FIELD_SCALE < 0))
		        {
			sprintf (Print_buffer, "NUMERIC(9, %d)", -FNA.RDB$FIELD_SCALE);
		        }
		    else if ((FNA.RDB$FIELD_TYPE == DOUBLE_PRECISION) && 
			     (FNA.RDB$FIELD_SCALE < 0))
		        {
			sprintf (Print_buffer, "NUMERIC(15, %d)", -FNA.RDB$FIELD_SCALE);
		        }
		    else
		        {
			sprintf (Print_buffer, "%s", Column_types [i].type_name);
		        }
		    }
		ISQL_printf (Out, Print_buffer);
		break;
		}

            }
	/* Print length where appropriate */
	if ((FNA.RDB$FIELD_TYPE == FCHAR) || (FNA.RDB$FIELD_TYPE == VARCHAR)
	    || (FNA.RDB$FIELD_TYPE == CSTRING))
	    {
	    if (V4)
		{
    		FOR V4FNA IN RDB$FUNCTION_ARGUMENTS CROSS
		    CHARSET IN RDB$CHARACTER_SETS OVER RDB$CHARACTER_SET_ID
		WITH V4FNA.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME AND
		     V4FNA.RDB$ARGUMENT_POSITION EQ FNA.RDB$ARGUMENT_POSITION

			ISQL_blankterm (CHARSET.RDB$CHARACTER_SET_NAME);
			sprintf (Print_buffer, "(%d) CHARACTER SET %s", 
				(FNA.RDB$FIELD_LENGTH / MAX (1,CHARSET.RDB$BYTES_PER_CHARACTER)),
				CHARSET.RDB$CHARACTER_SET_NAME);
			ISQL_printf (Out, Print_buffer);

		END_FOR
		    ON_ERROR
			ISQL_errmsg (isc_status);
			return ERR;
		    END_ERROR;
		}
	    else /* V3 - Note: This is the BYTE length, not the CHAR length */
		{
	    	sprintf (Print_buffer, "(%d)", FNA.RDB$FIELD_LENGTH);
		ISQL_printf (Out, Print_buffer);
		}
	    }

	ISQL_printf (Out, NEWLINE);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;
if (first)
    return (NOT_FOUND);
return (SKIP);
}

static int show_generators (
    SCHAR	*object)
{
/**************************************
 *
 *	s h o w _ g e n e r a t o r s
 *
 **************************************
 *
 * Functional description
 *	Show generators including the next number they return
 *      We do this by selecting the GEN_ID of each one,
 *         incrementing by 0 to not change the current value.
 *
 **************************************/
SSHORT			indicator, first = TRUE;
XSQLDA			sqlda;
SINT64			genid64 = 0;
SLONG			genid = 0;
isc_stmt_handle 	stmt = 0;
TEXT			query [100], gen_name [MAX_NAME_LENGTH * 2],
			*p1, *q1;

/* Show all generators  or named generator */
FOR GEN IN RDB$GENERATORS 
    SORTED BY GEN.RDB$GENERATOR_NAME

    ISQL_blankterm (GEN.RDB$GENERATOR_NAME);

    if ((!*object && GEN.RDB$SYSTEM_FLAG.NULL) || 
	(!strcmp (GEN.RDB$GENERATOR_NAME, object))) 
	{

	/* Get the current id for each generator */

	if (strchr (GEN.RDB$GENERATOR_NAME, DBL_QUOTE))
	    {
	    q1 = gen_name;
	    *q1++ = DBL_QUOTE;
	    for (p1 = GEN.RDB$GENERATOR_NAME; *p1; p1++)
		{
		*q1 = *p1;
		q1++;
		if (*p1 == DBL_QUOTE)
		    {
		    *q1 = DBL_QUOTE;
		    q1++;
		    }
		}
	    *q1++ = DBL_QUOTE;
	    *q1 = '\0';
	    }
	else
	    {
	    if (SQL_dialect == SQL_DIALECT_V6)
		{
		q1 = gen_name;
		*q1++ = DBL_QUOTE;
		for (p1 = GEN.RDB$GENERATOR_NAME; *p1; p1++)
		    {
		    *q1 = *p1;
		    q1++;
		    }
		*q1++ = DBL_QUOTE;
		*q1 = '\0';
		}
	    else
		strcpy(gen_name, GEN.RDB$GENERATOR_NAME);
	    }

	sprintf (query, "SELECT GEN_ID(%s, 0) FROM RDB$DATABASE", gen_name);

	isc_dsql_allocate_statement (isc_status, &DB, &stmt);
	sqlda.sqln = 1;
	sqlda.version = 1;
	
	/* If the user has set his client dialect to 1, we take that to
	   mean that he wants to see just the lower 32 bits of the
	   generator, as in V5.  Otherwise, we show him the whole 64-bit
	   value.
	 */
	if (isc_dsql_prepare (isc_status, &gds__trans, &stmt, 0, query, 
		    SQL_dialect, &sqlda))
	    {
	    ISQL_errmsg (isc_status);
	    continue;
	    };
	if (SQL_dialect >= SQL_DIALECT_V6_TRANSITION)
	    sqlda.sqlvar[0].sqldata = (SCHAR*) &genid64;
	else
	    sqlda.sqlvar[0].sqldata = (SCHAR*) &genid;
	sqlda.sqlvar[0].sqlind = &indicator;

	/* Singleton select needs no fetch */
	if (isc_dsql_execute2 (isc_status, &gds__trans, &stmt, 
				SQL_dialect, NULL, &sqlda))
	    {
	    ISQL_errmsg (isc_status);
	    }
	else
	    {
	    first = FALSE;
	    if( SQL_dialect >= SQL_DIALECT_V6_TRANSITION)
	        sprintf (Print_buffer,
			 "Generator %s, current value is %" QUADFORMAT "d%s", 
			 GEN.RDB$GENERATOR_NAME, 
			 genid64,
			 NEWLINE);
	    else
	        sprintf (Print_buffer, "Generator %s, current value is %ld%s", 
			 GEN.RDB$GENERATOR_NAME, 
			 genid,
			 NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	if (isc_dsql_free_statement (isc_status, &stmt, DSQL_drop))
	    ISQL_errmsg (isc_status);
	}
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;
if (first)
    return (NOT_FOUND);
return SKIP;
}

static void show_index (
    SCHAR	*relation_name,
    SCHAR	*index_name,
    SSHORT	unique_flag,
    SSHORT	index_type,
    SSHORT	inactive)
{
/**************************************
 *
 *	s h o w _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Show an index.
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/
SCHAR	*collist;

/* Strip trailing blanks */

ISQL_blankterm (relation_name);
ISQL_blankterm (index_name);

sprintf (Print_buffer, "%s%s%s INDEX ON %s", index_name,
    (unique_flag ? " UNIQUE" : ""), 
    (index_type == 1 ? " DESCENDING" : ""), 
    relation_name);
ISQL_printf (Out, Print_buffer);

/* Get column names */

collist = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH512);
if (!collist)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;	
    ISQL_errmsg (isc_status);
    return;
    }

if (ISQL_get_index_segments (collist, index_name, FALSE))
    {
    sprintf (Print_buffer, "(%s) %s%s", collist, (inactive ? "(inactive)" : ""),
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }

ISQL_FREE (collist);
}

static int show_indices (
    SCHAR	**cmd)
{
/**************************************
 *
 *	s h o w _ i n d i c e s
 *
 **************************************
 *
 * Functional description
 *	shows indices for a given table name or index name or all tables
 *
 *	Use a static SQL query to get the info and print it.
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/
SCHAR	*name;
SSHORT	first = TRUE;

/* The names stored in the database are all upper case */

name = cmd [2];

if (*name)
    {
    FOR IDX1 IN RDB$INDICES WITH
	IDX1.RDB$RELATION_NAME EQ name OR 
	IDX1.RDB$INDEX_NAME EQ name
	SORTED BY IDX1.RDB$INDEX_NAME

	if (IDX1.RDB$INDEX_INACTIVE.NULL)
	    IDX1.RDB$INDEX_INACTIVE = 0;
	
	show_index (IDX1.RDB$RELATION_NAME, IDX1.RDB$INDEX_NAME,
	    IDX1.RDB$UNIQUE_FLAG, IDX1.RDB$INDEX_TYPE, IDX1.RDB$INDEX_INACTIVE);

#ifdef EXPRESSION_INDICES
	if (!IDX1.RDB$EXPRESSION_BLR.NULL)
            {
	    sprintf (Print_buffer, " COMPUTED BY ");
	    ISQL_printf (Out, Print_buffer);
	    if (!IDX1.RDB$EXPRESSION_SOURCE.NULL)
		SHOW_print_metadata_text_blob (Out, &IDX1.RDB$EXPRESSION_SOURCE);
	    ISQL_printf (Out, NEWLINE);
	    }
#endif

	first = FALSE;
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;
    if (first)
	return (NOT_FOUND);
    return (SKIP);

    }
else
    {
    FOR IDX2 IN RDB$INDICES CROSS
	REL IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH 
	REL.RDB$SYSTEM_FLAG NE 1 OR 
	REL.RDB$SYSTEM_FLAG MISSING 
	SORTED BY IDX2.RDB$RELATION_NAME, IDX2.RDB$INDEX_NAME

	first = FALSE;

	show_index (IDX2.RDB$RELATION_NAME, IDX2.RDB$INDEX_NAME,
	    IDX2.RDB$UNIQUE_FLAG, IDX2.RDB$INDEX_TYPE, IDX2.RDB$INDEX_INACTIVE);

#ifdef EXPRESSION_INDICES
	if (!IDX2.RDB$EXPRESSION_BLR.NULL)
            {
	    sprintf (Print_buffer, " COMPUTED BY ");
	    ISQL_printf (Out, Print_buffer);
	    if (!IDX2.RDB$EXPRESSION_SOURCE.NULL)
		SHOW_print_metadata_text_blob (Out, &IDX2.RDB$EXPRESSION_SOURCE);
	    ISQL_printf (Out, NEWLINE);
	    }
#endif

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return (ERR);
	END_ERROR;
    if (first)
	return (NOT_FOUND);
    return (SKIP);
    }

}

static int show_proc (
    SCHAR	*procname)
{
/**************************************
 *
 *	s h o w _ p r o c
 *
 **************************************
 *
 * Functional description
 *	shows text of a stored procedure given a name.
 *	or lists procedures if no argument.
 *
 *	procname -- Name of procedure to investigate
 *
 **************************************/
SCHAR	type_name[33], lenstring[33];
SSHORT 	i;
SSHORT 	first_proc = TRUE, first = TRUE;

/* If this is not a V4 database, just return */

if (!V4)
    return (SKIP);

/* If no procedure name was given, just list the procedures */

if (!strlen (procname))
    {
    /* This query gets the procedure name  the next query
    **   gets all the dependencies if any 
    */

    FOR PRC IN RDB$PROCEDURES
	SORTED BY PRC.RDB$PROCEDURE_NAME

	if (first_proc)
	{
    	sprintf (Print_buffer, 
    	"Procedure Name                    Dependency, Type%s", NEWLINE);
    	ISQL_printf (Out, Print_buffer);
    	sprintf (Print_buffer, 
	"================================= ======================================%s", NEWLINE);
    	ISQL_printf (Out, Print_buffer);
	first_proc = FALSE;
	}

	/* Strip trailing blanks */

	ISQL_blankterm (PRC.RDB$PROCEDURE_NAME);
	sprintf (Print_buffer, "%-34s", PRC.RDB$PROCEDURE_NAME);
	ISQL_printf (Out, Print_buffer);

	first = TRUE;
	FOR DEP IN RDB$DEPENDENCIES WITH
	    PRC.RDB$PROCEDURE_NAME EQ DEP.RDB$DEPENDENT_NAME
	    REDUCED TO DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME
	    SORTED BY DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME

	    ISQL_blankterm (DEP.RDB$DEPENDED_ON_NAME);
	    /* Get column type name to print */
	    if (!first)
		{
		sprintf (Print_buffer, "%s%34s", NEWLINE, "");
		ISQL_printf (Out, Print_buffer);
		}
	    first = FALSE;
	    sprintf (Print_buffer, "%s, %s", DEP.RDB$DEPENDED_ON_NAME, 
			Object_types[DEP.RDB$DEPENDED_ON_TYPE]);
	    ISQL_printf (Out, Print_buffer);
	END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return (ERR);
	    END_ERROR;
	ISQL_printf (Out, NEWLINE);

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return (ERR);
	END_ERROR;
    if (first_proc)
	return NOT_FOUND;
    return (SKIP);
    }

/* A procedure was named, so print all the info on that procedure */


FOR PRC IN RDB$PROCEDURES WITH
	PRC.RDB$PROCEDURE_NAME EQ procname
	first = FALSE;

    sprintf (Print_buffer, "Procedure text:%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "=============================================================================%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (!PRC.RDB$PROCEDURE_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &PRC.RDB$PROCEDURE_SOURCE);
    sprintf (Print_buffer, "%s=============================================================================%s", NEWLINE, NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, "Parameters:%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
	
    FOR PRM IN RDB$PROCEDURE_PARAMETERS CROSS
	FLD IN RDB$FIELDS WITH
	PRC.RDB$PROCEDURE_NAME EQ PRM.RDB$PROCEDURE_NAME AND
	PRM.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME
	SORTED BY PRM.RDB$PARAMETER_TYPE, PRM.RDB$PARAMETER_NUMBER

	ISQL_blankterm (PRM.RDB$PARAMETER_NAME);
	/* Get column type name to print */
    /* Look through types array */

	for (i = 0; Column_types [i].type; i++)
            if (FLD.RDB$FIELD_TYPE == Column_types [i].type)
                {
		BOOLEAN precision_known = FALSE;

		if (major_ods >= ODS_VERSION10)
		    {
		    /* Handle Integral subtypes NUMERIC and DECIMAL */

		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) ||
			(FLD.RDB$FIELD_TYPE == INTEGER) ||
		        (FLD.RDB$FIELD_TYPE == blr_int64))
		      {
			FOR FLD1 IN RDB$FIELDS
			    WITH FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME
			  /* We are ODS >= 10 and could be any Dialect */
			  if (!FLD1.RDB$FIELD_PRECISION.NULL)
			    {
			      /* We are Dialect >=3 since FIELD_PRECISION is
				 non-NULL */
			      if (FLD1.RDB$FIELD_SUB_TYPE > 0 &&
				  FLD1.RDB$FIELD_SUB_TYPE <= MAX_INTSUBTYPES)
				{
				sprintf (type_name, "%s(%d, %d)", 
				   Integral_subtypes [FLD1.RDB$FIELD_SUB_TYPE],
				   FLD1.RDB$FIELD_PRECISION,
				   -FLD1.RDB$FIELD_SCALE);
				precision_known = TRUE;
				}
			    }
			END_FOR
			    ON_ERROR
				ISQL_errmsg (isc_status);
				return ERR;
			    END_ERROR;
		      } /* if field_type ... */
		    } /* if major_ods ... */

		if (precision_known == FALSE)
                    {
		    /* Take a stab at numerics and decimals */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) && (FLD.RDB$FIELD_SCALE < 0))
			sprintf (type_name, "NUMERIC(4, %d)", -FLD.RDB$FIELD_SCALE);
		    else if ((FLD.RDB$FIELD_TYPE == INTEGER) && (FLD.RDB$FIELD_SCALE < 0))
			sprintf (type_name, "NUMERIC(9, %d)", -FLD.RDB$FIELD_SCALE);
		    else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
			(FLD.RDB$FIELD_SCALE < 0))
			sprintf (type_name, "NUMERIC(15, %d)", -FLD.RDB$FIELD_SCALE);
		    else
			strcpy(type_name, Column_types [i].type_name);
		    }
                break;
                }

	if (((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR)) &&
	    !FLD.RDB$CHARACTER_LENGTH.NULL)
	    sprintf (lenstring, "(%d)", FLD.RDB$FIELD_LENGTH);
	else
	    sprintf (lenstring, "");

	sprintf (Print_buffer, "%-33s %s %s%s ", PRM.RDB$PARAMETER_NAME,
	    (PRM.RDB$PARAMETER_TYPE ? "OUTPUT" : "INPUT"),
	    type_name, lenstring);
	ISQL_printf (Out, Print_buffer);

	/* Show international character sets and collations */

        if (V4 && (FLD.RDB$FIELD_TYPE == FCHAR ||
                FLD.RDB$FIELD_TYPE == VARCHAR  ||
                FLD.RDB$FIELD_TYPE == BLOB))
            show_charsets(NULL, FLD.RDB$FIELD_NAME);
	ISQL_printf (Out, NEWLINE);

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;
if (first)
    return (NOT_FOUND);
return (SKIP);
}

static int show_role (
    SCHAR	*object)
{
SCHAR	role_name [BUFFER_LENGTH128];
SCHAR	user_string [44];
SSHORT	first = TRUE;
SSHORT  skip = FALSE;
SSHORT	odd = 1;

if (object == NULL) /* show role with no parameters, show all roles */
{
/**************************************
 *	Print the names of all roles from
 *	RDB$ROLES.  We use a dynamic query 
 *	If there is any roles, then returns SKIP.
 *	Otherwise returns NOT_FOUND.
 **************************************/

FOR X IN RDB$ROLES WITH
    X.RDB$ROLE_NAME NOT MISSING
    SORTED BY X.RDB$ROLE_NAME
	
    first = FALSE;
    sprintf (Print_buffer, "%38s%s", X.RDB$ROLE_NAME, (odd ? " " : NEWLINE));
    ISQL_printf (Out, Print_buffer);
    odd = 1 - odd;

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;

if (!first)
    {
    ISQL_printf (Out, NEWLINE);
    return SKIP;
    }
else
    return NOT_FOUND;
}
else  /* show role with role supplied, display users granted this role */
{

FOR FIRST 1 R IN RDB$ROLES WITH R.RDB$ROLE_NAME EQ object;
first = FALSE;

FOR PRV IN RDB$USER_PRIVILEGES WITH
    PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
    PRV.RDB$USER_TYPE     EQ obj_user     AND
    PRV.RDB$RELATION_NAME EQ object       AND
    PRV.RDB$PRIVILEGE     EQ 'M'
    SORTED BY  PRV.RDB$USER

    ISQL_blankterm (PRV.RDB$RELATION_NAME);
    strcpy (role_name, PRV.RDB$RELATION_NAME);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	ISQL_copy_SQL_id (role_name, &SQL_identifier, DBL_QUOTE);
    else
	strcpy (SQL_identifier, role_name);

    ISQL_blankterm (PRV.RDB$USER);
    strcpy (user_string, PRV.RDB$USER);
	sprintf(Print_buffer, "%s\n", user_string);

    first = FALSE;
    ISQL_printf (Out, Print_buffer);

    END_FOR
        ON_ERROR
        ISQL_errmsg (isc_status);
        return (ERR);
        END_ERROR;

END_FOR
    ON_ERROR
    ISQL_errmsg (isc_status);
    return (ERR);
    END_ERROR;

if (first)
    return (NOT_FOUND);

return(SKIP);
}
}

static int show_table (
    SCHAR	*relation_name)
{
/**************************************
 *
 *	s h o w _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	shows columns, types, info for a given table name
 *	and text of views.
 *	Use a SQL query to get the info and print it.
 *	This also shows integrity constraints and triggers 
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/
SSHORT		i, subtype;
ISC_QUAD	default_source;
SCHAR		*collist;
SSHORT		first = TRUE;

collist = NULL;

/* Query to obtain relation information */

FOR REL IN RDB$RELATIONS
    WITH REL.RDB$RELATION_NAME EQ relation_name
    if (first)
	{
	if (!REL.RDB$EXTERNAL_FILE.NULL)
	    {
	    sprintf (Print_buffer, "External file: %s\n", REL.RDB$EXTERNAL_FILE);
	    ISQL_printf (Out, Print_buffer);
	    }
	}
    first = FALSE;
END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;

if (first)
    return (NOT_FOUND);

/* 
FOR RFR IN RDB$RELATION_FIELDS CROSS
	REL IN RDB$RELATIONS CROSS
	FLD IN RDB$FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ relation_name AND
	REL.RDB$RELATION_NAME EQ RFR.RDB$RELATION_NAME
	SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME
*/

FOR RFR IN RDB$RELATION_FIELDS CROSS
	FLD IN RDB$FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ relation_name 
	SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME

    /* Get length of colname to align columns for printing */

    ISQL_blankterm (RFR.RDB$FIELD_NAME);

    /* Print the column name in first column */

    sprintf (Print_buffer, "%-32s", RFR.RDB$FIELD_NAME);  
    ISQL_printf (Out, Print_buffer);

    /* Decide if this is a user-created domain */
    if (!((strncmp (FLD.RDB$FIELD_NAME, "RDB$", 4) == 0) && 
	   isdigit (FLD.RDB$FIELD_NAME[4]) &&
	   FLD.RDB$SYSTEM_FLAG != 1))
	{
	ISQL_blankterm (FLD.RDB$FIELD_NAME);
	sprintf (Print_buffer, "(%s) ", FLD.RDB$FIELD_NAME);
	ISQL_printf (Out, Print_buffer);
	}

    /* Detect the existence of arrays */

    if (!FLD.RDB$DIMENSIONS.NULL)
	{
	sprintf (Print_buffer, "ARRAY OF ");
	ISQL_printf (Out, Print_buffer);
        ISQL_array_dimensions (FLD.RDB$FIELD_NAME);
	sprintf (Print_buffer, "%s                                ", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

    /* If a computed field, show the source and exit */
    /* Note that view columns which are computed are dealt with later.*/
    if (!FLD.RDB$COMPUTED_BLR.NULL)
        {
	sprintf (Print_buffer, "Computed by: ");
	ISQL_printf (Out, Print_buffer);
	if (!FLD.RDB$COMPUTED_SOURCE.NULL)
	    SHOW_print_metadata_text_blob (Out, &FLD.RDB$COMPUTED_SOURCE);
	ISQL_printf (Out, NEWLINE);
	continue;
	}


    /* Look through types array */

    for (i = 0; Column_types [i].type; i++)
	if (FLD.RDB$FIELD_TYPE == Column_types [i].type)
	    {
	    BOOLEAN precision_known = FALSE;

	    if (major_ods >= ODS_VERSION10)
		{
		/* Handle Integral subtypes NUMERIC and DECIMAL */
		if ((FLD.RDB$FIELD_TYPE == SMALLINT) ||
		    (FLD.RDB$FIELD_TYPE == INTEGER) ||
		    (FLD.RDB$FIELD_TYPE == blr_int64))
		  {
		    FOR FLD1 IN RDB$FIELDS WITH
		        FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME
		    
		      /* We are ODS >= 10 and could be any Dialect */
		      if (!FLD1.RDB$FIELD_PRECISION.NULL)
			{
			  /* We are Dialect >=3 since FIELD_PRECISION is
			     non-NULL */
			  if (FLD1.RDB$FIELD_SUB_TYPE > 0 &&
			      FLD1.RDB$FIELD_SUB_TYPE <= MAX_INTSUBTYPES)
			    {
			    sprintf (Print_buffer, "%s(%d, %d)", 
			        Integral_subtypes [FLD1.RDB$FIELD_SUB_TYPE],
				FLD1.RDB$FIELD_PRECISION,
			        -FLD1.RDB$FIELD_SCALE);
			    precision_known = TRUE;
			    }
			}
		    END_FOR
		      ON_ERROR
		          ISQL_errmsg (isc_status);
		          return ERR;
		      END_ERROR;
		  }
		}

	    if (precision_known == FALSE)
		{
		/* Take a stab at numerics and decimals */
		if ((FLD.RDB$FIELD_TYPE == SMALLINT) && (FLD.RDB$FIELD_SCALE < 0))
		    {
		    sprintf (Print_buffer, "NUMERIC(4, %d)", -FLD.RDB$FIELD_SCALE);
		    }
		else if ((FLD.RDB$FIELD_TYPE == INTEGER) && (FLD.RDB$FIELD_SCALE < 0))
		    {
		    sprintf (Print_buffer, "NUMERIC(9, %d)", -FLD.RDB$FIELD_SCALE);
		    }
		else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) && 
		    (FLD.RDB$FIELD_SCALE < 0))
		    {
		    sprintf (Print_buffer, "NUMERIC(15, %d)", -FLD.RDB$FIELD_SCALE);
		    }
		else
		    {
		    sprintf (Print_buffer, "%s", Column_types [i].type_name);
		    }
		}
	    ISQL_printf (Out, Print_buffer);
	    break;
	    }

    if ((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR))
	{
	/* Don't test character_length in a V3 database */

	if (!V4) 
	    {
	    sprintf (Print_buffer, "(%d)", FLD.RDB$FIELD_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }
	else 
	    {
	    sprintf (Print_buffer, "(%d)", ISQL_get_field_length(FLD.RDB$FIELD_NAME));
	    ISQL_printf (Out, Print_buffer);
	    }

        /* Show international character sets and collations */

	if (V4)
	    show_charsets  (relation_name, RFR.RDB$FIELD_NAME);
	}

    if (FLD.RDB$FIELD_TYPE == BLOB)
	{
	sprintf (Print_buffer, " segment %u, subtype ", (USHORT) FLD.RDB$SEGMENT_LENGTH);
	ISQL_printf (Out, Print_buffer);
	subtype  = FLD.RDB$FIELD_SUB_TYPE;
	if (subtype >= 0 && subtype <= MAXSUBTYPES) 
	    {
	    sprintf (Print_buffer, "%s", Sub_types [subtype]);
	    ISQL_printf (Out, Print_buffer);
	    }
	else 
	    {
	    sprintf (Print_buffer, "%d", subtype);
	    ISQL_printf (Out, Print_buffer);
	    }

        /* Show international character sets and collations */

	if (V4)
	    show_charsets  (relation_name, RFR.RDB$FIELD_NAME);
	}

    if (!FLD.RDB$COMPUTED_BLR.NULL)
	{
	/* A view expression. Other computed fields will not reach this point.*/
	sprintf (Print_buffer, " Expression%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	continue;
	}

    /* The null flag is either 1 or null (for nullable) */

    if (V4)
	{
	if (RFR.RDB$NULL_FLAG == 1 || FLD.RDB$NULL_FLAG == 1 ||
	     (!RFR.RDB$BASE_FIELD.NULL && 
		!ISQL_get_null_flag (relation_name, RFR.RDB$FIELD_NAME)))
	    {
	    sprintf (Print_buffer, " Not Null ");
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, " Nullable ");
	    ISQL_printf (Out, Print_buffer);
	    }
	}

    /* Handle defaults for columns */

    if (V4)
	{
	if (!RFR.RDB$DEFAULT_SOURCE.NULL)
	    SHOW_print_metadata_text_blob (Out, &RFR.RDB$DEFAULT_SOURCE);
	else if (!FLD.RDB$DEFAULT_SOURCE.NULL)
	    SHOW_print_metadata_text_blob (Out, &FLD.RDB$DEFAULT_SOURCE);
	}
    ISQL_printf (Out, NEWLINE);

    /* Validation clause for domains */
    if (!FLD.RDB$VALIDATION_SOURCE.NULL)
	{
	sprintf (Print_buffer, "                                ");
	ISQL_printf (Out, Print_buffer);
	SHOW_print_metadata_text_blob (Out, &FLD.RDB$VALIDATION_SOURCE);
	ISQL_printf (Out, NEWLINE);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;

/* If this is a view and there were columns, print the view text */

if (!first)
    {
    FOR REL IN RDB$RELATIONS WITH
	REL.RDB$RELATION_NAME EQ relation_name AND
	REL.RDB$VIEW_BLR NOT MISSING

	sprintf (Print_buffer, "View Source:%s==== ======%s", NEWLINE, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	if (!REL.RDB$VIEW_SOURCE.NULL)
	    SHOW_print_metadata_text_blob (Out, &REL.RDB$VIEW_SOURCE);
	ISQL_printf (Out, NEWLINE);
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;
    } 

/* Handle any referential or primary constraint on this table */

if (V4)
    {
    collist = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH512);
    if (!collist)
	{
	isc_status [0] = isc_arg_gds;
	isc_status [1] = isc_virmemexh;
	isc_status [2] = isc_arg_end;	
	ISQL_errmsg (isc_status);
	return ERR;
	}

    /* Static queries for obtaining referential constraints */

    FOR RELC1 IN RDB$RELATION_CONSTRAINTS WITH
	RELC1.RDB$RELATION_NAME EQ relation_name
	SORTED BY RELC1.RDB$CONSTRAINT_TYPE, RELC1.RDB$CONSTRAINT_NAME

	ISQL_get_index_segments (collist, RELC1.RDB$INDEX_NAME, FALSE);
	if (!strncmp (RELC1.RDB$CONSTRAINT_TYPE, "PRIMARY", 7))
	    {
            ISQL_blankterm (RELC1.RDB$CONSTRAINT_NAME);
            sprintf (Print_buffer, "CONSTRAINT %s:%s", RELC1.RDB$CONSTRAINT_NAME, NEWLINE);
            ISQL_printf (Out, Print_buffer);
	    sprintf (Print_buffer, "  Primary key (%s)%s", collist, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	else if (!strncmp (RELC1.RDB$CONSTRAINT_TYPE, "UNIQUE", 6))
	    {
            ISQL_blankterm (RELC1.RDB$CONSTRAINT_NAME);
            sprintf (Print_buffer, "CONSTRAINT %s:%s", RELC1.RDB$CONSTRAINT_NAME, NEWLINE);
            ISQL_printf (Out, Print_buffer);
	    sprintf (Print_buffer, "  Unique key (%s)%s", collist, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	else if (!strncmp (RELC1.RDB$CONSTRAINT_TYPE, "FOREIGN", 7))
	    {
            ISQL_blankterm (RELC1.RDB$CONSTRAINT_NAME);
            sprintf (Print_buffer, "CONSTRAINT %s:%s", RELC1.RDB$CONSTRAINT_NAME, NEWLINE);
            ISQL_printf (Out, Print_buffer);
	    ISQL_get_index_segments (collist, RELC1.RDB$INDEX_NAME, FALSE);
	    sprintf (Print_buffer, "  Foreign key (%s)", collist);
	    ISQL_printf (Out, Print_buffer);

	    FOR RELC2 IN RDB$RELATION_CONSTRAINTS CROSS
	        REFC IN RDB$REF_CONSTRAINTS WITH
		RELC2.RDB$CONSTRAINT_NAME EQ REFC.RDB$CONST_NAME_UQ AND
		REFC.RDB$CONSTRAINT_NAME EQ RELC1.RDB$CONSTRAINT_NAME
    
		ISQL_get_index_segments (collist, RELC2.RDB$INDEX_NAME, FALSE);
		ISQL_blankterm (RELC2.RDB$RELATION_NAME);

		sprintf (Print_buffer, "    References %s (%s)", 
		    RELC2.RDB$RELATION_NAME, collist);
		ISQL_printf (Out, Print_buffer);
    
		if (!REFC.RDB$UPDATE_RULE.NULL)
		    {
		    ISQL_truncate_term (REFC.RDB$UPDATE_RULE,
		        sizeof(REFC.RDB$UPDATE_RULE));
                    ISQL_ri_action_print (REFC.RDB$UPDATE_RULE, " On Update",
			FALSE);
		    }
		      
		if (!REFC.RDB$DELETE_RULE.NULL)
		    {
		    ISQL_truncate_term (REFC.RDB$DELETE_RULE,
		        sizeof(REFC.RDB$DELETE_RULE));
                    ISQL_ri_action_print (REFC.RDB$DELETE_RULE, " On Delete", 
			FALSE);
		    }

                sprintf (Print_buffer, "%s", NEWLINE);
		ISQL_printf (Out, Print_buffer);
    
	    END_FOR
		ON_ERROR
		    ISQL_errmsg (isc_status);
		    return ERR;
		END_ERROR;
	    }

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return ERR;
	END_ERROR;

    FOR R_C IN RDB$RELATION_CONSTRAINTS CROSS
        C_C IN RDB$CHECK_CONSTRAINTS
        WITH R_C.RDB$RELATION_NAME   EQ relation_name
         AND R_C.RDB$CONSTRAINT_TYPE EQ 'NOT NULL'
         AND R_C.RDB$CONSTRAINT_NAME EQ C_C.RDB$CONSTRAINT_NAME

        if (strncmp (R_C.RDB$CONSTRAINT_NAME, "INTEG_", 6))
            {
            ISQL_blankterm (C_C.RDB$TRIGGER_NAME);
            ISQL_blankterm (R_C.RDB$CONSTRAINT_NAME);
            sprintf (Print_buffer, "CONSTRAINT %s:%s", R_C.RDB$CONSTRAINT_NAME,
                     NEWLINE);
            ISQL_printf (Out, Print_buffer);
            sprintf (Print_buffer, "  Not Null Column (%s)%s",
                     C_C.RDB$TRIGGER_NAME, NEWLINE);
            ISQL_printf (Out, Print_buffer);
            }
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    ISQL_FREE (collist);
	    return ERR;
	END_ERROR;
    }

if (collist)
    ISQL_FREE (collist);

/* Do check constraints */

(void) show_check (relation_name);

/* Do triggers */

(void) show_trigger (relation_name, NO_SOURCE);

if (first)
    return (NOT_FOUND);
return SKIP;
}

static int show_trigger (
    SCHAR	*object,
    SSHORT	flag)
{
/**************************************
 *
 *	s h o w _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Show triggers in general or  for the named object or trigger
 *
 **************************************/
SSHORT	first = TRUE;
SSHORT skip = FALSE;

/* Show all triggers */
if (!*object) 
    {
    if (V4)
	{
	FOR TRG IN RDB$TRIGGERS CROSS REL IN RDB$RELATIONS
	WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	    TRG.RDB$RELATION_NAME = REL.RDB$RELATION_NAME AND
	    NOT (ANY CHK IN RDB$CHECK_CONSTRAINTS WITH
	    TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME)
	    SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_NAME

	    if (first)
		{
		sprintf (Print_buffer, 
		    "Table name                       Trigger name%s", NEWLINE);
		ISQL_printf (Out, Print_buffer);
		sprintf (Print_buffer, 
		    "===========                      ============%s", NEWLINE);
		ISQL_printf (Out, Print_buffer);
		first = FALSE;
		}

	    ISQL_blankterm (TRG.RDB$TRIGGER_NAME);
	    ISQL_blankterm (TRG.RDB$RELATION_NAME);
	    sprintf (Print_buffer, "%-32s %s%s%s", 
		TRG.RDB$RELATION_NAME, 
		TRG.RDB$TRIGGER_NAME, 
		(TRG.RDB$SYSTEM_FLAG == 1 ? "(system)" : ""),
		NEWLINE);
	    ISQL_printf (Out, Print_buffer);
        END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;
	}
    else /* V3 databases */
	{
	FOR TRG IN RDB$TRIGGERS CROSS REL IN RDB$RELATIONS
	WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	     TRG.RDB$RELATION_NAME = REL.RDB$RELATION_NAME
	    SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_NAME

	    if (first)
		{
		sprintf (Print_buffer, 
		    "Table name                       Trigger name%s", NEWLINE);
		ISQL_printf (Out, Print_buffer);
		sprintf (Print_buffer, 
		    "===========                      ============%s", NEWLINE);
		ISQL_printf (Out, Print_buffer);
		first = FALSE;
		}

	    ISQL_blankterm (TRG.RDB$TRIGGER_NAME);
	    ISQL_blankterm (TRG.RDB$RELATION_NAME);
	    sprintf (Print_buffer, "%-32s %s%s%s", TRG.RDB$RELATION_NAME, 
		TRG.RDB$TRIGGER_NAME, 
		(TRG.RDB$SYSTEM_FLAG == 1 ? "(system)" : ""),
		NEWLINE);
	    ISQL_printf (Out, Print_buffer);
        END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;
	}
	if (first)
	    return NOT_FOUND;
    	else
    	    return (SKIP);
    }

/* Show triggers for the named object */
/* In V4 databases we also want to avoid check constraints */

FOR TRG IN RDB$TRIGGERS WITH
	(TRG.RDB$RELATION_NAME EQ object OR
	TRG.RDB$TRIGGER_NAME EQ object)
	SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_TYPE, 
		  TRG.RDB$TRIGGER_SEQUENCE, TRG.RDB$TRIGGER_NAME;

    skip = FALSE;
    if (V4)
	{
	/* Skip triggers for check constraints */
	FOR FIRST 1 CHK IN RDB$CHECK_CONSTRAINTS WITH
	    TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME 
	    skip = TRUE;
	END_FOR
	    ON_ERROR
		ISQL_errmsg (isc_status);
		return ERR;
	    END_ERROR;
	}
    if (skip)
	continue;
    ISQL_blankterm (TRG.RDB$TRIGGER_NAME);
    ISQL_blankterm (TRG.RDB$RELATION_NAME);

    if (first)
	{
	sprintf (Print_buffer, "%sTriggers on Table %s:%s", 
		 NEWLINE, 
		 TRG.RDB$RELATION_NAME,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
        first = FALSE;
	}

    sprintf (Print_buffer, "%s, Sequence: %d, Type: %s, %s%s",
             TRG.RDB$TRIGGER_NAME,
	     TRG.RDB$TRIGGER_SEQUENCE,
	     Trigger_types [TRG.RDB$TRIGGER_TYPE],
	     (TRG.RDB$TRIGGER_INACTIVE ? "Inactive" : "Active"), 
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (flag == SHOW_SOURCE)
	{
	/* Use print_blob to print the blob */

	if (!TRG.RDB$TRIGGER_SOURCE.NULL)
	    SHOW_print_metadata_text_blob (Out, &TRG.RDB$TRIGGER_SOURCE);
	sprintf (Print_buffer, "%s+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++%s", NEWLINE, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return ERR;
    END_ERROR;
if (first)
    return NOT_FOUND;
return SKIP;
}
