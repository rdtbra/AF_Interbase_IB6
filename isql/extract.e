/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		extract.e
 *	DESCRIPTION:	Definition extract routines
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
#include <math.h>
#ifdef GUI_TOOLS
#include <windows.h>
#endif
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../dsql/dsql.h"
#include "../isql/isql.h" 
#include "../jrd/constants.h"
#include "../isql/extra_proto.h"
#include "../isql/isql_proto.h" 
#include "../isql/show_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/ods.h"

DATABASE DB = EXTERN COMPILETIME "yachts.lnk";

extern USHORT	major_ods;
extern USHORT	minor_ods;
extern SSHORT	V33;

static void	list_all_grants (void);
static void	list_all_procs (void);
static void	list_all_tables (SSHORT);
static void	list_all_triggers (void);
static void	list_check (void);
static void	list_create_db (void);
static void	list_domain_table (SCHAR *);
static void	list_domains (void);
static void	list_exception (void);
static void	list_filters (void);
static void	list_foreign (void);
static void	list_functions (void);
static void	list_generators (void);
static void	list_index (void);
static void	list_views (void);

static void     get_procedure_args (SCHAR *);

#define MAX_INTSUBTYPES	2
#define MAXSUBTYPES	8
#define BLANK		'\040'
#define DBL_QUOTE	'\042'
#define SINGLE_QUOTE	'\''

extern IB_FILE	*Out;

/* Command terminator and procedure terminator */

extern SQLTYPES	Column_types [];
extern SCHAR	*Integral_subtypes [];
extern SCHAR	*Sub_types [];
extern SCHAR	*Trigger_types [];

static SCHAR	*Procterm = "^";	/* TXNN: script use only */
static TEXT	Print_buffer[512];
static TEXT	SQL_identifier[BUFFER_LENGTH128];
static TEXT	SQL_identifier2[BUFFER_LENGTH128];


SSHORT EXTRACT_ddl (
    int		flag,
    SCHAR	*tabname)
{
/**************************************
 *
 *	E X T R A C T _ d d l
 *
 **************************************
 *
 * Functional description
 *	Extract all sql information
 *	0 flag means SQL only tables. 1 flag means all tables
 *
 **************************************/
SCHAR	errbuf [MSG_LENGTH];
SSHORT	ret_code = FINI_OK;
USHORT	did_attach = FALSE;
USHORT	did_start = FALSE;

if (!DB)
    {
    if (isc_attach_database (gds__status, 0, Db_name, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (gds__status);
	return (SSHORT)FINI_ERROR;
	}
    did_attach = TRUE;
    }

ISQL_get_version (FALSE);
if (SQL_dialect != db_SQL_dialect)
    {
    sprintf (Print_buffer, 
    "/*=========================================================*/%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, 
    "/*=                                                      ==*/%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, 
    "/*=     Command Line -sqldialect %d is over writted by    ==*/%s", 
						       SQL_dialect, NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, 
    "/*=     Database SQL Dialect %d.                          ==*/%s", 
						       db_SQL_dialect, NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, 
    "/*=                                                      ==*/%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    sprintf (Print_buffer, 
    "/*=========================================================*/%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }
sprintf (Print_buffer, " %s", NEWLINE);
ISQL_printf (Out, Print_buffer);

sprintf (Print_buffer, "SET SQL DIALECT %d; %s", db_SQL_dialect, NEWLINE);
ISQL_printf (Out, Print_buffer);

sprintf (Print_buffer, " %s", NEWLINE);
ISQL_printf (Out, Print_buffer);

if (!gds__trans)
    {
    if (isc_start_transaction (gds__status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
        {
	ISQL_errmsg (gds__status);
	return (SSHORT)FINI_ERROR;
	}
    did_start = TRUE;
    }

/* If a table name was passed, extract only that table and domains */
if (*tabname)
    {
    if (EXTRACT_list_table (tabname, NULL, 1))
	{
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, NOT_FOUND, sizeof (errbuf), 
	    errbuf, tabname, NULL, NULL, NULL, NULL);
	STDERROUT (errbuf, 1);
	ret_code = (SSHORT) FINI_ERROR;
	}
    }
else
    {
    list_create_db();
    list_filters();
    list_functions();
    list_domains();
    list_all_tables (flag);
    list_index ();
    list_foreign ();
    list_generators();
    list_views ();
    list_check();
    list_exception();
    list_all_procs ();
    list_all_triggers ();
    list_all_grants ();
    }

if (gds__trans && did_start)
    if (isc_commit_transaction (gds__status, &gds__trans))
	{
	ISQL_errmsg (gds__status);
	return (SSHORT)FINI_ERROR;
	}

if (DB && did_attach)
    {
    if (isc_detach_database (gds__status, &DB))
	{
	ISQL_errmsg (gds__status);
	return (SSHORT)FINI_ERROR;
	}
    DB = NULL;
    }

return (SSHORT) ret_code;
}

SSHORT EXTRACT_list_table (
    SCHAR	*relation_name,
    SCHAR	*new_name,
    SSHORT	domain_flag)
{
/**************************************
 *
 *	E X T R A C T _ l i s t _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Shows columns, types, info for a given table name
 *	and text of views.
 *	Use a GDML query to get the info and print it.
 *	If a new_name is passed, substitute it for relation_name
 *
 *	relation_name -- Name of table to investigate
 *	new_name -- Name of a new name for a replacement table
 *	domain_flag -- extract needed domains before the table
 *
 **************************************/
USHORT		first; 
SSHORT		collation, char_set_id;
SSHORT		i;
SCHAR		*collist, char_sets[86];
SSHORT		subtype;
SSHORT		intchar;

/* Query to obtain relation detail information */

first   = TRUE;
collist = NULL;
intchar = 0;

FOR REL IN RDB$RELATIONS CROSS
	RFR IN RDB$RELATION_FIELDS CROSS
	FLD IN RDB$FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ REL.RDB$RELATION_NAME AND
	REL.RDB$RELATION_NAME EQ relation_name
	SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME

    if (first)
	{
	first = FALSE;
	/* Do we need to print domains */
	if (domain_flag)
	    list_domain_table (relation_name);

	ISQL_blankterm (REL.RDB$OWNER_NAME);
	sprintf (Print_buffer, "%s/* Table: %s, Owner: %s */%s", 
		 NEWLINE,
		 relation_name,
		 REL.RDB$OWNER_NAME,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    if (new_name)
		ISQL_copy_SQL_id (new_name, &SQL_identifier, DBL_QUOTE);
	    else
		ISQL_copy_SQL_id (relation_name, &SQL_identifier, DBL_QUOTE);
	    sprintf (Print_buffer, "CREATE TABLE %s ", SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "CREATE TABLE %s ", 
		    new_name ? new_name : relation_name);
	ISQL_printf (Out, Print_buffer);
	if (!REL.RDB$EXTERNAL_FILE.NULL)
	    {
	    ISQL_copy_SQL_id (REL.RDB$EXTERNAL_FILE, &SQL_identifier2, 
			      SINGLE_QUOTE);
	    sprintf (Print_buffer, "EXTERNAL FILE %s ", SQL_identifier2);
	    ISQL_printf (Out, Print_buffer);
	}
	ISQL_printf (Out, "(");
	}
    else
	{
	sprintf (Print_buffer, ",%s%s", NEWLINE, TAB);
	ISQL_printf (Out, Print_buffer); 
	}

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (ISQL_blankterm (RFR.RDB$FIELD_NAME),
			  &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "%s ", SQL_identifier);
	}
    else
	sprintf (Print_buffer, "%s ", ISQL_blankterm (RFR.RDB$FIELD_NAME));
    ISQL_printf (Out, Print_buffer);

    /*
    ** Check first for computed fields, then domains.
    ** If this is a known domain, then just print the domain rather than type 
    ** Domains won't have length, array, or blob definitions, but they
    ** may have not null, default and check overriding their definitions
    */

    if (!FLD.RDB$COMPUTED_BLR.NULL)
	{
	ISQL_printf (Out, "COMPUTED BY ");
	if (!FLD.RDB$COMPUTED_SOURCE.NULL)
	    ISQL_print_validation (Out, &FLD.RDB$COMPUTED_SOURCE, 1, gds__trans);
	}
    else if (!((strncmp(FLD.RDB$FIELD_NAME, "RDB$", 4) == 0) && 
		isdigit (FLD.RDB$FIELD_NAME[4]) &&
		FLD.RDB$SYSTEM_FLAG != 1))
	{
	ISQL_blankterm (FLD.RDB$FIELD_NAME);
	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (FLD.RDB$FIELD_NAME, &SQL_identifier, DBL_QUOTE);
	    sprintf (Print_buffer, "%s", SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "%s", FLD.RDB$FIELD_NAME);
	ISQL_printf (Out, Print_buffer);
	/* International character sets */
	if ((FLD.RDB$FIELD_TYPE == FCHAR || FLD.RDB$FIELD_TYPE == VARCHAR)
	    && !RFR.RDB$COLLATION_ID.NULL && RFR.RDB$COLLATION_ID)
	    {
	    char_sets [0] = '\0';
	    collation = RFR.RDB$COLLATION_ID;
	    ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID, collation, TRUE, char_sets);
	    if (char_sets [0])
		ISQL_printf (Out, char_sets);
	    }
	}
    else
	{
	/* Look through types array */
	for (i = 0; Column_types [i].type; i++)
            if (FLD.RDB$FIELD_TYPE == Column_types [i].type)
                {
		BOOLEAN precision_known = FALSE;

		if (major_ods >= ODS_VERSION10)
		    {
		    /* Handle Integral subtypes NUMERIC and DECIMAL */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) || 
			(FLD.RDB$FIELD_TYPE == INTEGER)  ||
		        (FLD.RDB$FIELD_TYPE == blr_int64))
		      {
			FOR FLD1 IN RDB$FIELDS WITH
			    FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME
			
			  /* We are ODS >= 10 and could be any Dialect */
			  if (!FLD1.RDB$FIELD_PRECISION.NULL)
			    {
		      /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
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
			{
			sprintf (Print_buffer, "NUMERIC(4, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == INTEGER) && 
				(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(9, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
				(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(15, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else
			sprintf (Print_buffer, "%s", 
				    Column_types [i].type_name);
                    }
		ISQL_printf (Out, Print_buffer);
                break;
                }

	if ((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR))
	    if (FLD.RDB$CHARACTER_LENGTH.NULL)
		{
		sprintf (Print_buffer, "(%d)", FLD.RDB$FIELD_LENGTH);
		ISQL_printf (Out, Print_buffer);
		}
	    else
		{
		sprintf (Print_buffer, "(%d)", FLD.RDB$CHARACTER_LENGTH);
		ISQL_printf (Out, Print_buffer);
		}

        /* Catch arrays after printing the type  */

	if (!FLD.RDB$DIMENSIONS.NULL)
	    ISQL_array_dimensions (FLD.RDB$FIELD_NAME);

	if (FLD.RDB$FIELD_TYPE == BLOB)
	    {
	    subtype = FLD.RDB$FIELD_SUB_TYPE;
	    ISQL_printf (Out, " SUB_TYPE "); 
	    if ((subtype > 0) && (subtype <= MAXSUBTYPES))
	        ISQL_printf (Out, Sub_types [subtype]);
	    else
		{
	        sprintf (Print_buffer, "%d", subtype); 
		ISQL_printf (Out, Print_buffer);
		}
	    sprintf (Print_buffer, " SEGMENT SIZE %u", 
		    (USHORT) FLD.RDB$SEGMENT_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }

	/* International character sets */
	if ((FLD.RDB$FIELD_TYPE == FCHAR || FLD.RDB$FIELD_TYPE == VARCHAR ||
	     FLD.RDB$FIELD_TYPE == BLOB) &&
	    !FLD.RDB$CHARACTER_SET_ID.NULL && FLD.RDB$CHARACTER_SET_ID)
	    {
	    char_sets [0] = '\0';

	    /* Override rdb$fields id with relation_fields if present */

	    collation = 0; 
	    if (!RFR.RDB$COLLATION_ID.NULL)
		collation = RFR.RDB$COLLATION_ID;
	    else if (!FLD.RDB$COLLATION_ID.NULL)
		collation = FLD.RDB$COLLATION_ID;

	    char_set_id = 0;
	    if (!FLD.RDB$CHARACTER_SET_ID.NULL)
		char_set_id = FLD.RDB$CHARACTER_SET_ID;

	    ISQL_get_character_sets (char_set_id, 0, FALSE, char_sets);
	    if (char_sets [0])
		ISQL_printf (Out, char_sets);
	    intchar = 1;
	    }
	}


    /* Handle defaults for columns */

    if (!RFR.RDB$DEFAULT_SOURCE.NULL)
	{
	ISQL_printf (Out, " ");
	SHOW_print_metadata_text_blob (Out, &RFR.RDB$DEFAULT_SOURCE);
	}


    /* The null flag is either 1 or null (for nullable) .  if there is
    **  a constraint name, print that too.  Domains cannot have named 
    **  constraints.  The column name is in rdb$trigger_name in 
    **  rdb$check_constraints.  We hope we get at most one row back.
    */

    if (RFR.RDB$NULL_FLAG == 1)
	{
	FOR RCO IN RDB$RELATION_CONSTRAINTS CROSS
	    CON IN RDB$CHECK_CONSTRAINTS WITH
	    CON.RDB$TRIGGER_NAME = RFR.RDB$FIELD_NAME AND
	    CON.RDB$CONSTRAINT_NAME = RCO.RDB$CONSTRAINT_NAME AND
	    RCO.RDB$CONSTRAINT_TYPE EQ "NOT NULL" AND 
	    RCO.RDB$RELATION_NAME = RFR.RDB$RELATION_NAME

	    if (strncmp(CON.RDB$CONSTRAINT_NAME, "INTEG", 5))
		{
		ISQL_blankterm(CON.RDB$CONSTRAINT_NAME);
		if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		    {
		    ISQL_copy_SQL_id (CON.RDB$CONSTRAINT_NAME, &SQL_identifier,
				      DBL_QUOTE);
		    sprintf (Print_buffer, " CONSTRAINT %s", SQL_identifier);
		    }
	        else
		    sprintf (Print_buffer, " CONSTRAINT %s", 
			     CON.RDB$CONSTRAINT_NAME);
		ISQL_printf (Out, Print_buffer);
		}
	END_FOR
	    ON_ERROR
		ISQL_errmsg (gds__status);
		return (SSHORT)FINI_ERROR;
	    END_ERROR;


	ISQL_printf (Out, " NOT NULL");
	}

if ((FLD.RDB$FIELD_TYPE == FCHAR || FLD.RDB$FIELD_TYPE == VARCHAR ||
    FLD.RDB$FIELD_TYPE == BLOB) &&
    !FLD.RDB$CHARACTER_SET_ID.NULL && FLD.RDB$CHARACTER_SET_ID && intchar)
    {
    collation = 0; 
    if (!RFR.RDB$COLLATION_ID.NULL)
	collation = RFR.RDB$COLLATION_ID;
    else if (!FLD.RDB$COLLATION_ID.NULL)
	collation = FLD.RDB$COLLATION_ID;

    char_set_id = 0;
    if (!FLD.RDB$CHARACTER_SET_ID.NULL)
	char_set_id = FLD.RDB$CHARACTER_SET_ID;

    if (collation)
    	{
    	char_sets[0] = '\0';
    	ISQL_get_character_sets (char_set_id, collation, TRUE, char_sets);
    	if (char_sets [0])
		ISQL_printf (Out, char_sets);
    	}
    }

END_FOR
    ON_ERROR
	if (!V33)
	    {
	    ISQL_errmsg (gds__status);
	    return (SSHORT)FINI_ERROR;
	    }
    END_ERROR;

/* Do primary and unique keys only. references come later */

collist = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH512 * 2);
if (!collist) 
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    return ((SSHORT) FINI_ERROR);
    }

FOR RELC IN RDB$RELATION_CONSTRAINTS WITH
    (RELC.RDB$CONSTRAINT_TYPE EQ "PRIMARY KEY" OR
    RELC.RDB$CONSTRAINT_TYPE EQ "UNIQUE") AND
    RELC.RDB$RELATION_NAME EQ relation_name
    SORTED BY RELC.RDB$CONSTRAINT_NAME

    sprintf (Print_buffer, ",%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    /* If the name of the constraint is not INTEG..., print it */
    if (strncmp(RELC.RDB$CONSTRAINT_NAME, "INTEG", 5))
	{
	ISQL_blankterm(RELC.RDB$CONSTRAINT_NAME);
	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (RELC.RDB$CONSTRAINT_NAME, &SQL_identifier,
			      DBL_QUOTE);
	    sprintf (Print_buffer, "CONSTRAINT %s ", SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "CONSTRAINT %s ", RELC.RDB$CONSTRAINT_NAME);
	ISQL_printf (Out, Print_buffer);
	}

    if (!strncmp (RELC.RDB$CONSTRAINT_TYPE, "PRIMARY", 7))
	{
	ISQL_get_index_segments (collist, RELC.RDB$INDEX_NAME, TRUE);
	sprintf (Print_buffer, "PRIMARY KEY (%s)", collist);
	ISQL_printf (Out, Print_buffer);
	}

    else if (!strncmp (RELC.RDB$CONSTRAINT_TYPE, "UNIQUE", 6))
	{
	ISQL_get_index_segments (collist, RELC.RDB$INDEX_NAME, TRUE);
	sprintf (Print_buffer, "UNIQUE (%s)", collist);
	ISQL_printf (Out, Print_buffer);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	if (collist)
    	    ISQL_FREE (collist);
	return (SSHORT)FINI_ERROR;
    END_ERROR;

/* Check constaints are now deferred*/

sprintf (Print_buffer,")%s%s", Term, NEWLINE);
ISQL_printf (Out, Print_buffer);

if (collist)
    ISQL_FREE (collist);

return (SSHORT)FINI_OK;
}

#ifdef GUI_TOOLS
void EXTRACT_list_view (
	SCHAR	*viewname)
{
/**************************************
 *
 *	E X T R A C T _ l i s t _ v i e w 
 *
 **************************************
 *
 * Functional description
 *	Show text of the specified view.
 *	Use a SQL query to get the info and print it.
 *	Note: This should also contain check option
 *
 **************************************/
SSHORT 	first;

if (!DB)
    if (isc_attach_database (gds__status, 0, Db_name, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (gds__status);
	return (SSHORT)FINI_ERROR;
	}
if (!gds__trans)
    isc_start_transaction (gds__status, &gds__trans, 1, &DB, 0, (SCHAR*) 0);

/* If this is a view, use print_blob to print the view text */

FOR REL IN RDB$RELATIONS WITH
	(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	REL.RDB$VIEW_BLR NOT MISSING AND
	REL.RDB$RELATION_NAME = viewname AND
	REL.RDB$FLAGS = REL_sql
	SORTED BY REL.RDB$RELATION_ID
	
    first = TRUE;
    ISQL_blankterm (REL.RDB$RELATION_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	ISQL_copy_SQL_id (REL.RDB$RELATION_NAME, &SQL_identifier, DBL_QUOTE);
    else
	strcpy (SQL_identifier, REL.RDB$RELATION_NAME);

    ISQL_blankterm (REL.RDB$OWNER_NAME);

    sprintf (Print_buffer, "%s/* View: %s, Owner: %s */%s",
	     NEWLINE,
	     SQL_identifier, 
	     REL.RDB$OWNER_NAME,
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "CREATE VIEW %s (", SQL_identifier);
    ISQL_printf (Out, Print_buffer);

    /* Get column list */
    FOR RFR IN RDB$RELATION_FIELDS WITH
	RFR.RDB$RELATION_NAME = REL.RDB$RELATION_NAME
	SORTED BY RFR.RDB$FIELD_POSITION

	ISQL_blankterm (RFR.RDB$FIELD_NAME);

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    ISQL_blankterm (RFR.RDB$FIELD_NAME, &SQL_identifier);
	else
	    strcpy (SQL_identifier, RFR.RDB$FIELD_NAME);

        sprintf (Print_buffer, "%s%s", (first ? "" : ", "), SQL_identifier);

	ISQL_printf (Out, Print_buffer);
	first = FALSE;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (gds__status);
	    return;
	END_ERROR;

    sprintf (Print_buffer, ") AS%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    if (!REL.RDB$VIEW_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &REL.RDB$VIEW_SOURCE);
    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}
#endif

static void get_procedure_args (
    SCHAR	*proc_name)
{
/**************************************
 *
 *	g e t _ p r o c e d u r e _ a r g s
 *
 **************************************
 *
 * Functional description
 *	This function extract the procedure parameters and adds it to the
 *	extract file
 *
 **************************************/
SSHORT	first_time, i, header = TRUE;
TEXT	msg [MSG_LENGTH];
SCHAR   char_sets [86];

/* query to retrieve the parameters. */
    {
    first_time = TRUE;
    /*  Parameter types 0 = input first */
    FOR PRM IN RDB$PROCEDURE_PARAMETERS CROSS
	FLD IN RDB$FIELDS WITH
	PRM.RDB$PROCEDURE_NAME = proc_name AND
	PRM.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND 
	PRM.RDB$PARAMETER_TYPE = 0
	SORTED BY PRM.RDB$PARAMETER_NUMBER

	if (first_time)
	    {
	    first_time = FALSE;
	    ISQL_printf (Out, "(");
	    }
	else
	    {
	    sprintf (Print_buffer, ",%s", NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }

	ISQL_blankterm (PRM.RDB$PARAMETER_NAME);
	sprintf (Print_buffer, "%s ", PRM.RDB$PARAMETER_NAME);
	ISQL_printf (Out, Print_buffer);

	/* Get column type name to print */
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
			/* We are ODS >= 10 and could be any Dialect */

			FOR FLD1 IN RDB$FIELDS WITH
			    FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME

			  if (!FLD1.RDB$FIELD_PRECISION.NULL)
			    {
		      /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
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
			      return;
			  END_ERROR;
		      }
		    }
		if (precision_known == FALSE)
                    {
		    /* Take a stab at numerics and decimals */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) && 
			(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(4, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == INTEGER) && 
			    (FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(9, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
			(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(15, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else
			{
			sprintf (Print_buffer, "%s", 
				Column_types [i].type_name);
			}
                    }
		ISQL_printf (Out, Print_buffer);
                break;
                }
	if (((FLD.RDB$FIELD_TYPE == FCHAR) || 
	    (FLD.RDB$FIELD_TYPE == VARCHAR)) &&
           !FLD.RDB$CHARACTER_LENGTH.NULL)
	    {
	    sprintf (Print_buffer, "(%d)", FLD.RDB$FIELD_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }

	/* Show international character sets and collations */
	if (!FLD.RDB$COLLATION_ID.NULL || !FLD.RDB$CHARACTER_SET_ID.NULL)
            {
            char_sets [0] = 0;
            if (FLD.RDB$COLLATION_ID.NULL)
                FLD.RDB$COLLATION_ID = 0;

            if (FLD.RDB$CHARACTER_SET_ID.NULL)
                FLD.RDB$CHARACTER_SET_ID = 0;

            ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID,
                FLD.RDB$COLLATION_ID, FALSE, char_sets);
            if (char_sets [0])
                ISQL_printf (Out, char_sets);
            }	
    END_FOR
	ON_ERROR
	if (!V33)
	    {
	    ISQL_errmsg (gds__status);
	    return;
	    }
	END_ERROR;

    /* If there was at least one param, close parens */

    if (!first_time)
	{
	sprintf (Print_buffer, ")%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first_time = TRUE;

    /*  Parameter types 1 = return last */
    FOR PRM IN RDB$PROCEDURE_PARAMETERS CROSS
	FLD IN RDB$FIELDS WITH
	PRM.RDB$PROCEDURE_NAME  = proc_name AND
	PRM.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND 
	PRM.RDB$PARAMETER_TYPE = 1
	SORTED BY PRM.RDB$PARAMETER_NUMBER

	if (first_time)
	    {
	    first_time = FALSE;
	    ISQL_printf (Out, "RETURNS (");
	    }
	else
	    {
	    sprintf (Print_buffer, ",%s", NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }

	ISQL_blankterm (PRM.RDB$PARAMETER_NAME);
	sprintf (Print_buffer, "%s ", PRM.RDB$PARAMETER_NAME);
	ISQL_printf (Out, Print_buffer);

	/* Get column type name to print */
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
			/* We are ODS >= 10 and could be any Dialect */
			FOR FLD1 IN RDB$FIELDS WITH
			    FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME

			  if (!FLD1.RDB$FIELD_PRECISION.NULL)
			    {
		    /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
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
			      return;
			  END_ERROR;
		      }	/* if field_type */
		    } /* if major_ods */

		if (precision_known == FALSE)
                    {
		    /* Take a stab at numerics and decimals */
		    if ((FLD.RDB$FIELD_TYPE == SMALLINT) && 
			(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(4, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == INTEGER) && 
			    (FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(9, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else if ((FLD.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
			(FLD.RDB$FIELD_SCALE < 0))
			{
			sprintf (Print_buffer, "NUMERIC(15, %d)", 
				-FLD.RDB$FIELD_SCALE);
			}
		    else
			{
			sprintf (Print_buffer, "%s", 
				Column_types [i].type_name);
			}
                    }
		ISQL_printf (Out, Print_buffer);
                break;
                }

	if (((FLD.RDB$FIELD_TYPE == FCHAR) || 
	    (FLD.RDB$FIELD_TYPE == VARCHAR)) && !FLD.RDB$CHARACTER_LENGTH.NULL)
	    {
	    sprintf (Print_buffer, "(%d)", FLD.RDB$FIELD_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }

	/* Show international character sets and collations */
	if (!FLD.RDB$COLLATION_ID.NULL || !FLD.RDB$CHARACTER_SET_ID.NULL)
            {
            char_sets [0] = 0;
            if (FLD.RDB$COLLATION_ID.NULL)
                FLD.RDB$COLLATION_ID = 0;

            if (FLD.RDB$CHARACTER_SET_ID.NULL)
                FLD.RDB$CHARACTER_SET_ID = 0;

            ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID,
                FLD.RDB$COLLATION_ID, FALSE, char_sets);
            if (char_sets [0])
                ISQL_printf (Out, char_sets);
            }	
    END_FOR
	ON_ERROR
	    if (!V33)
		{
		ISQL_errmsg (gds__status);
		return;
		}
	END_ERROR;
    if (!first_time)
	{
	sprintf (Print_buffer, ")%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    sprintf (Print_buffer, "AS %s", NEWLINE);
    ISQL_printf (Out, Print_buffer);


    }
}


static void list_all_grants (void)
{
/**************************************
 *
 *	l i s t _ a l l _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Print the permissions on all user tables.
 *
 *	Get separate permissions on table/views and then procedures
 *
 **************************************/
SSHORT 	first = TRUE;
TEXT	prev_owner [44];

/******************************************************************
**
**	process GRANT roles
**
*******************************************************************/

if (major_ods >= ODS_VERSION9)
    {
    prev_owner [0] = '\0';

    FOR XX IN RDB$ROLES 
	SORTED BY XX.RDB$ROLE_NAME

	if (first)
	    {
	    sprintf (Print_buffer, "%s/* Grant role for this database */%s",
		     NEWLINE, 
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }

	first = FALSE;

	/* Null terminate name string */
	ISQL_blankterm (XX.RDB$ROLE_NAME);
	ISQL_blankterm (XX.RDB$OWNER_NAME);

	if (strcmp (prev_owner, XX.RDB$OWNER_NAME) != 0)
	    {
	    sprintf (Print_buffer, "%s/* Role: %s, Owner: %s */%s", 
		     NEWLINE,
		     XX.RDB$ROLE_NAME,
		     XX.RDB$OWNER_NAME,
		     NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    strcpy (prev_owner, XX.RDB$OWNER_NAME);
	    }

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (XX.RDB$ROLE_NAME, &SQL_identifier, DBL_QUOTE);
	    sprintf (Print_buffer, "CREATE ROLE %s;%s",
		     SQL_identifier, NEWLINE);
	    }
	else
	    sprintf (Print_buffer, "CREATE ROLE %s;%s",
		     XX.RDB$ROLE_NAME, NEWLINE);
	ISQL_printf (Out, Print_buffer);

    END_FOR
	ON_ERROR
	    ISQL_errmsg (gds__status);
	    return;
	END_ERROR;
    }

/* This version of cursor gets only sql tables identified by security class 
   and misses views, getting only null view_source */

first = TRUE;

FOR REL IN RDB$RELATIONS WITH
	(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	REL.RDB$SECURITY_CLASS STARTING "SQL$"
	SORTED BY REL.RDB$RELATION_NAME

    if (first)
	{
	sprintf (Print_buffer, "%s/* Grant permissions for this database */%s",
		 NEWLINE, 
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first = FALSE;

    /* Null terminate name string */

    ISQL_blankterm (REL.RDB$RELATION_NAME);

    SHOW_grants (REL.RDB$RELATION_NAME, Term);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

if (first)
    {
    sprintf (Print_buffer, "%s/* Grant permissions for this database */%s",
	     NEWLINE, 
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);
    first = FALSE;
    }

SHOW_grant_roles (Term); 

/* Again for stored procedures */
FOR PRC IN RDB$PROCEDURES 
	SORTED BY PRC.RDB$PROCEDURE_NAME

    if (first)
	{
	sprintf (Print_buffer, "%s/* Grant permissions for this database */%s",
		 NEWLINE, 
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first = FALSE;

    /* Null terminate name string */

    ISQL_blankterm (PRC.RDB$PROCEDURE_NAME);

    SHOW_grants (PRC.RDB$PROCEDURE_NAME, Term);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

}

static void list_all_procs ()
{
/**************************************
 *
 *	l i s t _ a l l _ p r o c s
 *
 **************************************
 *
 * Functional description
 *	Shows text of a stored procedure given a name.
 *	or lists procedures if no argument.
 *	Since procedures may reference each other, we will create all
 *	dummy procedures of the correct name, then alter these to their
 *	correct form. 
 *      Add the parameter names when these procedures are created.
 *
 *	procname -- Name of procedure to investigate
 *
 **************************************/
SSHORT	first_time, i, header = TRUE;
TEXT	msg [MSG_LENGTH];
SCHAR   char_sets [86];
static CONST SCHAR	*create_procedure_str1 = 
	"CREATE PROCEDURE %s ";
static CONST SCHAR	*create_procedure_str2 = 
	"BEGIN EXIT; END %s%s";


/*  First the dummy procedures */
/* create the procedures with their parameters */

FOR PRC IN RDB$PROCEDURES
    SORTED BY PRC.RDB$PROCEDURE_NAME
    if (header)
        {
        sprintf (Print_buffer, "COMMIT WORK;%s", NEWLINE);
        ISQL_printf (Out, Print_buffer);
        sprintf (Print_buffer, "SET AUTODDL OFF;%s", NEWLINE);
        ISQL_printf (Out, Print_buffer);
        sprintf (Print_buffer, "SET TERM %s %s%s", Procterm, Term, NEWLINE);
        ISQL_printf (Out, Print_buffer);
        sprintf (Print_buffer, "%s/* Stored procedures */%s", NEWLINE, NEWLINE);
        ISQL_printf (Out, Print_buffer);
        header = FALSE;
        }
    ISQL_blankterm (PRC.RDB$PROCEDURE_NAME);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (PRC.RDB$PROCEDURE_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, create_procedure_str1, 
		SQL_identifier);
        ISQL_printf (Out, Print_buffer);
        get_procedure_args (SQL_identifier);
	sprintf (Print_buffer, create_procedure_str2, Procterm, NEWLINE);
	}
    else
        {
	sprintf (Print_buffer, create_procedure_str1, 
		PRC.RDB$PROCEDURE_NAME);
        ISQL_printf (Out, Print_buffer);
        get_procedure_args (PRC.RDB$PROCEDURE_NAME);
	sprintf (Print_buffer, create_procedure_str2, Procterm, NEWLINE);
        }

    ISQL_printf (Out, Print_buffer);
END_FOR
    ON_ERROR
    if (!V33)
	{
	ISQL_errmsg (gds__status);
	return;
	}
    END_ERROR;

/* This query gets the procedure name and the source.  We then nest a query
   to retrieve the parameters. Alter is used, because the procedures are already there*/
FOR PRC IN RDB$PROCEDURES
    SORTED BY PRC.RDB$PROCEDURE_NAME

    ISQL_blankterm (PRC.RDB$PROCEDURE_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (PRC.RDB$PROCEDURE_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "%sALTER PROCEDURE %s ", NEWLINE, 
		 SQL_identifier);
	}
    else
	sprintf (Print_buffer, "%sALTER PROCEDURE %s ", NEWLINE, 
		 PRC.RDB$PROCEDURE_NAME);
    ISQL_printf (Out, Print_buffer);
    get_procedure_args (PRC.RDB$PROCEDURE_NAME);
    
    /* Print the procedure body */

    if (!PRC.RDB$PROCEDURE_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &PRC.RDB$PROCEDURE_SOURCE);

    sprintf (Print_buffer, " %s%s", Procterm, NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_msg_get (GEN_ERR, msg, (TEXT*) isc_sqlcode (gds__status), NULL, NULL, NULL, NULL);
	STDERROUT (msg, 1);	/* Statement failed, SQLCODE = %d\n\n */
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

/* Only reset the terminators is there were procs to print */
if (!header)
    {
    sprintf (Print_buffer, "SET TERM %s %s%s", Term, Procterm, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "COMMIT WORK %s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "SET AUTODDL ON;%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }
}

static void list_all_tables (
    SSHORT	flag)
{
/**************************************
 *
 *	l i s t _ a l l _ t a b l e s
 *
 **************************************
 *
 * Functional description
 *	Extract the names of all user tables from
 *	rdb$relations.  Filter SQL tables by
 *	security class after we fetch them
 *	Parameters:  flag -- 0, get all tables
 *
 **************************************/

/* This version of cursor gets only sql tables identified by security class 
   and misses views, getting only null view_source */

FOR REL IN RDB$RELATIONS WITH
	(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	REL.RDB$VIEW_BLR MISSING 
	SORTED BY REL.RDB$RELATION_NAME

    /* If this is not an SQL table and we aren't doing ALL objects */
    if ((REL.RDB$FLAGS != REL_sql) && (flag != ALL_objects))
	continue;
    /* Null terminate name string */

    ISQL_blankterm (REL.RDB$RELATION_NAME);

    if (flag || !strncmp (REL.RDB$SECURITY_CLASS, "SQL$", 4))
	EXTRACT_list_table (REL.RDB$RELATION_NAME, NULL, 0);
END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	ROLLBACK;
	return;
    END_ERROR;

}

static void list_all_triggers ()
{
/**************************************
 *
 *	l i s t _ a l l _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Lists triggers in general on non-system
 *	tables with sql source only.
 *
 **************************************/
SSHORT 		header = TRUE;


/* Query gets the trigger info for non-system triggers with 
   source that are not part of an SQL constraint */

FOR TRG IN RDB$TRIGGERS CROSS REL IN RDB$RELATIONS OVER RDB$RELATION_NAME
    WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	NOT (ANY CHK IN RDB$CHECK_CONSTRAINTS WITH
	TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME)
	SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_TYPE,
		TRG.RDB$TRIGGER_SEQUENCE, TRG.RDB$TRIGGER_NAME

    if (header)
    	{
	sprintf (Print_buffer, "SET TERM %s %s%s", Procterm, Term, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	sprintf (Print_buffer, 
		 "%s/* Triggers only will work for SQL triggers */%s", 
		 NEWLINE,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	header = FALSE;
	}
    ISQL_blankterm (TRG.RDB$TRIGGER_NAME);
    ISQL_blankterm (TRG.RDB$RELATION_NAME);

    if (TRG.RDB$TRIGGER_INACTIVE.NULL) 
	TRG.RDB$TRIGGER_INACTIVE = 0;

    /*  If trigger is not SQL put it in comments */
    if (TRG.RDB$FLAGS != TRG_sql)
	ISQL_printf (Out, "/* ");

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (TRG.RDB$TRIGGER_NAME,  &SQL_identifier,  DBL_QUOTE);
	ISQL_copy_SQL_id (TRG.RDB$RELATION_NAME, &SQL_identifier2, DBL_QUOTE);
	}
    else
	{
	strcpy (SQL_identifier,  TRG.RDB$TRIGGER_NAME);
	strcpy (SQL_identifier2, TRG.RDB$RELATION_NAME);
	}

    sprintf (Print_buffer, "CREATE TRIGGER %s FOR %s %s%s %s POSITION %d %s",
	SQL_identifier, SQL_identifier2, NEWLINE,
	(TRG.RDB$TRIGGER_INACTIVE ? "INACTIVE" : "ACTIVE"),
	Trigger_types [TRG.RDB$TRIGGER_TYPE], TRG.RDB$TRIGGER_SEQUENCE, 
	NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (!TRG.RDB$TRIGGER_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &TRG.RDB$TRIGGER_SOURCE);

    sprintf (Print_buffer, " %s%s", Procterm, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, " %s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (TRG.RDB$FLAGS != TRG_sql)
	{
	sprintf (Print_buffer, "*/%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

if (!header)
    {
    sprintf (Print_buffer, "COMMIT WORK %s%s", Procterm, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "SET TERM %s %s%s", Term, Procterm, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }
}

static void list_check (void)
{
/**************************************
 *
 *	l i s t _ c h e c k
 *
 **************************************
 *
 * Functional description
 *	List check constraints for all objects to allow forward references
 *
 **************************************/

/* Query gets the check clauses for triggers stored for check constraints */

FOR TRG IN RDB$TRIGGERS CROSS
	CHK IN RDB$CHECK_CONSTRAINTS WITH
	TRG.RDB$TRIGGER_TYPE EQ 1 AND
	TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME AND
	(ANY RELC IN RDB$RELATION_CONSTRAINTS WITH
	CHK.RDB$CONSTRAINT_NAME EQ RELC.RDB$CONSTRAINT_NAME)
	REDUCED TO CHK.RDB$CONSTRAINT_NAME
	SORTED BY CHK.RDB$CONSTRAINT_NAME


    ISQL_blankterm(TRG.RDB$RELATION_NAME);

    sprintf (Print_buffer, " %s ", NEWLINE); 
    ISQL_printf (Out, Print_buffer);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (TRG.RDB$RELATION_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "ALTER TABLE %s ADD %s%s", 
		 SQL_identifier, NEWLINE, TAB);
	}
    else
	sprintf (Print_buffer, "ALTER TABLE %s ADD %s%s", 
		 TRG.RDB$RELATION_NAME, NEWLINE, TAB);
    ISQL_printf (Out, Print_buffer);

    /* If the name of the constraint is not INTEG..., print it */
    if (strncmp(CHK.RDB$CONSTRAINT_NAME, "INTEG", 5))
	{
	ISQL_blankterm(CHK.RDB$CONSTRAINT_NAME);
	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (CHK.RDB$CONSTRAINT_NAME, &SQL_identifier, 
			      DBL_QUOTE);
	    sprintf (Print_buffer, "CONSTRAINT %s ", SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "CONSTRAINT %s ", CHK.RDB$CONSTRAINT_NAME);
	ISQL_printf (Out, Print_buffer);
	}

    if (!TRG.RDB$TRIGGER_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &TRG.RDB$TRIGGER_SOURCE);

    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}

static void print_set (
	SSHORT *set_used)
{
/**************************************
 *
 *	p r i n t _ s e t
 *
 **************************************
 *
 * Functional description
 *    print (using ISQL_printf) the word "SET"
 *    if the first line of the ALTER DATABASE
 *    settings options. Also, add trailing
 *    comma for end of prior line if needed.
 * 
 * uses Print_buffer, a global
 *
 **************************************/
if (!*set_used)
    {
    sprintf (Print_buffer, "  SET "); 
    ISQL_printf (Out, Print_buffer);
    *set_used = TRUE;
    }
else
    {
    sprintf (Print_buffer, ", %s      ", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    } 
}
static void list_create_db (void)
{
/**************************************
 *
 *	l i s t _ c r e a t e _ d b
 *
 **************************************
 *
 * Functional description
 *	Print the create database command if requested.  At least put 
 *	the page size in a comment with the extracted db name
 *
 **************************************/
SCHAR	info_buf[20];
static CONST SCHAR     page_items [] = {
	isc_info_page_size,
	isc_info_end
};
static CONST SCHAR wal_items [] = {
        isc_info_num_wal_buffers,
        isc_info_wal_buffer_size,
        isc_info_wal_grpc_wait_usecs,
        isc_info_wal_ckpt_length,
        isc_info_end
};

SSHORT	nodb = FALSE, first = TRUE, first_file = TRUE, has_wal = FALSE, set_used = FALSE;
USHORT   translate = FALSE;
SCHAR	*d, *buffer, item;
int	length;
SLONG	value_out;

buffer = NULL;
/* Comment out the create database if no db param was specified */
if (!*Target_db)
    {
    ISQL_printf (Out, "/* ");
    strcpy(Target_db,  Db_name);
    nodb = TRUE;
    }
sprintf (Print_buffer, "CREATE DATABASE '%s'", Target_db);
ISQL_printf (Out, Print_buffer);

/* Get the page size from db_info call */
SHOW_dbb_parameters (DB, info_buf, page_items, sizeof (page_items), translate);
sprintf (Print_buffer, " %s", info_buf);
ISQL_printf (Out, Print_buffer);

FOR DBP IN RDB$DATABASE 
    WITH DBP.RDB$CHARACTER_SET_NAME NOT MISSING
    AND DBP.RDB$CHARACTER_SET_NAME != " ";
    sprintf (Print_buffer, " DEFAULT CHARACTER SET %s", 
	ISQL_blankterm (DBP.RDB$CHARACTER_SET_NAME)); 
    ISQL_printf (Out, Print_buffer);
END_FOR
    ON_ERROR
	if (!V33)
	    {
	    ISQL_errmsg (gds__status);
	    return;
	    }
    END_ERROR;

if (nodb)
    {
    sprintf (Print_buffer, " */%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }
else
    {
    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }

/* List secondary files and shadows as alter db and create shadow in comment */


FOR FIL IN RDB$FILES 
    SORTED BY FIL.RDB$SHADOW_NUMBER, FIL.RDB$FILE_SEQUENCE

    if (first)
	{
	sprintf (Print_buffer, "%s/* Add secondary files in comments %s",
		 NEWLINE,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

    first = FALSE;
    /* reset nulls to zero */

    if (FIL.RDB$FILE_FLAGS.NULL)
	FIL.RDB$FILE_FLAGS = 0;
    if (FIL.RDB$FILE_LENGTH.NULL)
	FIL.RDB$FILE_LENGTH = 0;
    if (FIL.RDB$FILE_SEQUENCE.NULL)
	FIL.RDB$FILE_SEQUENCE = 0;
    if (FIL.RDB$FILE_START.NULL)
	FIL.RDB$FILE_START = 0;
    ISQL_blankterm (FIL.RDB$FILE_NAME);

    /* Pure secondary files */
    if (FIL.RDB$FILE_FLAGS == 0)
	{
	sprintf (Print_buffer, "%sALTER DATABASE ADD FILE '%s'", 
		 NEWLINE,
		 FIL.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	if (FIL.RDB$FILE_START)
	    {
	    sprintf (Print_buffer, " STARTING %ld", FIL.RDB$FILE_START);
	    ISQL_printf (Out, Print_buffer);
	    }
	if (FIL.RDB$FILE_LENGTH)
	    {
	    sprintf (Print_buffer, " LENGTH %ld", FIL.RDB$FILE_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }
	sprintf (Print_buffer, "%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	} 

    if (FIL.RDB$FILE_FLAGS & FILE_cache)
	{
	sprintf (Print_buffer, "%sALTER DATABASE ADD CACHE '%s' LENGTH %ld%s",
	    	 NEWLINE,
		 FIL.RDB$FILE_NAME, 
		 FIL.RDB$FILE_LENGTH,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
	
    if (FIL.RDB$FILE_FLAGS & FILE_shadow)
	{
	if (FIL.RDB$FILE_SEQUENCE)
	    {
	    sprintf (Print_buffer, "%sFILE '%s' ", TAB, FIL.RDB$FILE_NAME);
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, "%sCREATE SHADOW %d '%s' ", 
	             NEWLINE,
		     FIL.RDB$SHADOW_NUMBER, 
		     FIL.RDB$FILE_NAME);
	    ISQL_printf (Out, Print_buffer);
	    if (FIL.RDB$FILE_FLAGS & FILE_inactive)
	        ISQL_printf (Out, "INACTIVE ");
	    if (FIL.RDB$FILE_FLAGS & FILE_manual)
	        ISQL_printf (Out, "MANUAL ");
	    else 
		ISQL_printf (Out, "AUTO ");
	    if (FIL.RDB$FILE_FLAGS & FILE_conditional)
	        ISQL_printf (Out, "CONDITIONAL ");
	    }
	if (FIL.RDB$FILE_LENGTH)
	    {
	    sprintf (Print_buffer, "LENGTH %ld ", FIL.RDB$FILE_LENGTH);
	    ISQL_printf (Out, Print_buffer);
	    }
	if (FIL.RDB$FILE_START)
	    {
	    sprintf (Print_buffer, "STARTING %ld ", FIL.RDB$FILE_START);
	    ISQL_printf (Out, Print_buffer);
	    }
	ISQL_printf (Out, NEWLINE);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

FOR WAL IN RDB$LOG_FILES 
    SORTED BY WAL.RDB$FILE_FLAGS, WAL.RDB$FILE_SEQUENCE

    has_wal = TRUE;
    if (first)
    {
	if (nodb)
	{
	sprintf (Print_buffer, "/*%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
	sprintf (Print_buffer, "%sALTER DATABASE ADD ", NEWLINE);
	ISQL_printf (Out, Print_buffer);
    }
    first = FALSE;

    if (first_file)
	ISQL_printf (Out, "LOGFILE ");

    if (WAL.RDB$FILE_FLAGS & LOG_default)
	;

    /* Overflow files also have the serial bit set */
    else if (WAL.RDB$FILE_FLAGS & LOG_overflow)
	{
	sprintf (Print_buffer, ")%s   OVERFLOW '%s'", 
		 NEWLINE,
		 WAL.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}

    else if (WAL.RDB$FILE_FLAGS & LOG_serial)
	{
	sprintf (Print_buffer, "%s  BASE_NAME '%s'", 
		 NEWLINE,
		 WAL.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}

   /* Since we are fetching order by FILE_FLAGS, the LOG_0verflow will
   **  be last.  It will only appear if there were named round robin,
   **  so we must close the parens first
   */

    /* We have round robin and overflow file specifications */
    else
	{
	if (first_file)
	    ISQL_printf (Out, "(");
	else
	    {
	    sprintf (Print_buffer, ",%s  ", NEWLINE); 
	    ISQL_printf (Out, Print_buffer);
	    } 
        first_file = FALSE;	

	sprintf (Print_buffer, "'%s'", WAL.RDB$FILE_NAME);
	ISQL_printf (Out, Print_buffer);
	}
	
    /* Any file can have a length */
    if (!WAL.RDB$FILE_LENGTH.NULL && WAL.RDB$FILE_LENGTH)
	{
	sprintf (Print_buffer, " SIZE %ld ", WAL.RDB$FILE_LENGTH);
	ISQL_printf (Out, Print_buffer);
	}

END_FOR
    ON_ERROR
	if (!V33)
	    {
	    ISQL_errmsg (gds__status);
	    return;
	    }
    END_ERROR;

ISQL_printf (Out, NEWLINE);
/*************************************************************
** gds__info_num_wal_buffers         for NUM_LOG_BUFFERS
** gds__info_wal_buffer_size         for LOG_BUFFER_SIZE
** gds__info_wal_grpc_wait_usecs     for GROUP_COMMIT_WAIT_TIME
** gds__info_wal_ckpt_length         for CHECK_POINT_LENGTH
** 
**************************************************************/

if (has_wal)
    {
    buffer = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH128);
    if (!buffer)
	{
	isc_status [0] = isc_arg_gds;
	isc_status [1] = isc_virmemexh;
	isc_status [2] = isc_arg_end;
	ISQL_errmsg (isc_status);
	return;
	}

    if (!isc_database_info (gds__status, &DB, sizeof(wal_items), wal_items, BUFFER_LENGTH128,
	buffer))
	{
	for (d = buffer; *d != isc_info_end;)
	    {
	    item = *d++;
	    length = isc_vax_integer (d, 2);
	    d += 2;
	    value_out = isc_vax_integer (d, length);
	    switch (item)
		{
		case isc_info_end:
		    break;

		case isc_info_num_wal_buffers:
		    print_set (&set_used);
                    sprintf (Print_buffer, "NUM_LOG_BUFFERS = %ld", 
			     value_out);
		    ISQL_printf (Out, Print_buffer);
		    break;

		case isc_info_wal_buffer_size:
		    print_set (&set_used);
		    sprintf (Print_buffer, "LOG_BUFFER_SIZE = %ld", 
			     value_out);
		    ISQL_printf (Out, Print_buffer);
		    break;

		case isc_info_wal_grpc_wait_usecs:
		    print_set (&set_used);
		    sprintf (Print_buffer, "GROUP_COMMIT_WAIT_TIME = %ld", 
			     value_out);
		    ISQL_printf (Out, Print_buffer);
		    break;

		case isc_info_wal_ckpt_length:
		    print_set (&set_used);
		    sprintf (Print_buffer, "CHECK_POINT_LENGTH = %ld", 
			     value_out);
		    ISQL_printf (Out, Print_buffer);
		    break;

		default:
		    break;
		}
	    d += length;
	    }
	}
    if (buffer)
	ISQL_FREE (buffer);
    }	

    if (!first)
    {
	if (nodb)
	{
	    sprintf (Print_buffer, "%s */%s", NEWLINE, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	}
	else
	{
	    sprintf (Print_buffer, "%s%s%s", Term, NEWLINE, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	}
    }
}

static void list_domain_table (
    SCHAR	*table_name)
{
/**************************************
 *
 *	l i s t _ d o m a i n _ t a b l e 
 *
 **************************************
 *
 * Functional description
 *	List domains as identified by fields with any constraints on them
 *	for the named table
 *
 *	Parameters:  table_name == only extract domains for this table 
 *
 **************************************/
SSHORT 	first = TRUE;
SSHORT	i;
SCHAR	char_sets [86];
SSHORT		subtype;

FOR FLD IN RDB$FIELDS CROSS
    RFR IN RDB$RELATION_FIELDS WITH
    RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
    RFR.RDB$RELATION_NAME EQ table_name
    SORTED BY FLD.RDB$FIELD_NAME

    /* Skip over artifical domains */
    if (strncmp (FLD.RDB$FIELD_NAME, "RDB$", 4) == 0 &&
	isdigit (FLD.RDB$FIELD_NAME[4]) &&
	FLD.RDB$SYSTEM_FLAG != 1)
	continue;

    if (first)
	{
	sprintf (Print_buffer, "/* Domain definitions */%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	first = FALSE;
	}
    ISQL_blankterm(FLD.RDB$FIELD_NAME);
	
    sprintf (Print_buffer, "CREATE DOMAIN %s AS ", FLD.RDB$FIELD_NAME);
    ISQL_printf (Out, Print_buffer);

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
		    /* We are ODS >= 10 and could be any Dialect */

		    FOR FLD1 IN RDB$FIELDS WITH
		        FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME
		      if (!FLD1.RDB$FIELD_PRECISION.NULL)
			{
			  /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
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
			    return;
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
	

    if (FLD.RDB$FIELD_TYPE == BLOB)
	{
	subtype = FLD.RDB$FIELD_SUB_TYPE;
	ISQL_printf (Out, " SUB_TYPE ");
	if ((subtype > 0) && (subtype <= MAXSUBTYPES))
	    {
	    sprintf (Print_buffer, "%s", Sub_types [subtype]);
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, "%d", subtype); 
	    ISQL_printf (Out, Print_buffer);
	    }
	sprintf (Print_buffer, " SEGMENT SIZE %u", (USHORT) FLD.RDB$SEGMENT_LENGTH);
	ISQL_printf (Out, Print_buffer);
	}

    else if ((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR))
        {
        /* Length for chars */
        sprintf (Print_buffer, "(%d)", ISQL_get_field_length(FLD.RDB$FIELD_NAME));
	ISQL_printf (Out, Print_buffer);
        };

    /* Bug 8261: do not show the collation information just yet!  If you
       do, then the domain syntax when printed is not correct */

    /* since the character set is part of the field type, display that
       information now. */
    if (!FLD.RDB$CHARACTER_SET_ID.NULL)
	{
	char_sets[0] = 0;
	ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID, FALSE, FALSE, char_sets);
	if (char_sets[0])
	    ISQL_printf (Out, char_sets);
	}

    if (!FLD.RDB$DIMENSIONS.NULL)
	ISQL_array_dimensions (FLD.RDB$FIELD_NAME);

    if (!FLD.RDB$DEFAULT_SOURCE.NULL)
	{
	sprintf (Print_buffer, "%s%s ", NEWLINE, TAB);
	ISQL_printf (Out, Print_buffer);
	SHOW_print_metadata_text_blob (Out, &FLD.RDB$DEFAULT_SOURCE);
	}
    if (!FLD.RDB$VALIDATION_SOURCE.NULL)
	{
	sprintf (Print_buffer, "%s%s ", NEWLINE, TAB);
	ISQL_printf (Out, Print_buffer);
	ISQL_print_validation (Out, &FLD.RDB$VALIDATION_SOURCE, 0, gds__trans);
	}
    if (FLD.RDB$NULL_FLAG == 1)
	ISQL_printf (Out, " NOT NULL");

    /* Bug 8261:  Now show the collation order information */
    /* Show the collation order if one has been specified.  If the collation
       order is the default for the character set being used, then no collation
       order will be shown ( because it isn't needed ).

       If the collation id is 0, then the default for the character set is 
       being used so there is no need to retrieve the collation information.*/

    if (!FLD.RDB$COLLATION_ID.NULL && FLD.RDB$COLLATION_ID != 0)
        {
	char_sets[0] = 0;
	ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID, FLD.RDB$COLLATION_ID, TRUE, char_sets);
	if (char_sets[0])
	    ISQL_printf (Out, char_sets);
        }
	
    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);
END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}

static void list_domains (void)
{
/**************************************
 *
 *	l i s t _ d o m a i n s
 *
 **************************************
 *
 * Functional description
 *	List domains 
 *
 *	Parameters:  none
 *
 **************************************/
SSHORT 	first = TRUE;
SSHORT	i;
SCHAR	char_sets [86];
SSHORT	subtype;

FOR FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
    AND FLD.RDB$SYSTEM_FLAG NE 1
    SORTED BY FLD.RDB$FIELD_NAME

    if (first)
	{
	sprintf (Print_buffer, "/* Domain definitions */%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	first = FALSE;
	}
    ISQL_blankterm(FLD.RDB$FIELD_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id(FLD.RDB$FIELD_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "CREATE DOMAIN %s AS ", SQL_identifier);
	}
    else	
	sprintf (Print_buffer, "CREATE DOMAIN %s AS ", FLD.RDB$FIELD_NAME);

    ISQL_printf (Out, Print_buffer);

    /* Get domain type */
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
		    /* We are ODS >= 10 and could be any Dialect */
		    FOR FLD1 IN RDB$FIELDS WITH
		        FLD1.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME

		      if (!FLD1.RDB$FIELD_PRECISION.NULL)
			{
			  /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
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
			    return;
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

    if (FLD.RDB$FIELD_TYPE == BLOB)
	{
	subtype = FLD.RDB$FIELD_SUB_TYPE;
	ISQL_printf (Out, " SUB_TYPE ");

	if ((subtype > 0) && (subtype <= MAXSUBTYPES))
	    {
	    sprintf (Print_buffer, "%s", Sub_types [subtype]);
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (Print_buffer, "%d", subtype); 
	    ISQL_printf (Out, Print_buffer);
	    }
	sprintf (Print_buffer, " SEGMENT SIZE %u", (USHORT) FLD.RDB$SEGMENT_LENGTH);
	ISQL_printf (Out, Print_buffer);
	}

    else if ((FLD.RDB$FIELD_TYPE == FCHAR) || (FLD.RDB$FIELD_TYPE == VARCHAR))
        {
        /* Length for chars */
        sprintf (Print_buffer, "(%d)", ISQL_get_field_length(FLD.RDB$FIELD_NAME));
	ISQL_printf (Out, Print_buffer);
        };

    /* Bug 8261: do not show the collation information just yet!  If you
       do, then the domain syntax when printed is not correct */

    /* since the character set is part of the field type, display that
       information now. */
    if (!FLD.RDB$CHARACTER_SET_ID.NULL)
	{
	char_sets[0] = 0;
	ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID, FALSE, FALSE, char_sets);
	if (char_sets[0])
	    ISQL_printf (Out, char_sets);
	}

    if (!FLD.RDB$DIMENSIONS.NULL)
	ISQL_array_dimensions (FLD.RDB$FIELD_NAME);

    if (!FLD.RDB$DEFAULT_SOURCE.NULL)
	{
	sprintf (Print_buffer, "%s%s ", NEWLINE, TAB);
	ISQL_printf (Out, Print_buffer);
	SHOW_print_metadata_text_blob (Out, &FLD.RDB$DEFAULT_SOURCE);
	}
    if (!FLD.RDB$VALIDATION_SOURCE.NULL)
	{
	sprintf (Print_buffer, "%s%s ", NEWLINE, TAB);
	ISQL_printf (Out, Print_buffer);
	ISQL_print_validation (Out, &FLD.RDB$VALIDATION_SOURCE, 0, gds__trans);
	}
    if (FLD.RDB$NULL_FLAG == 1)
	ISQL_printf (Out, " NOT NULL");
	
    /* Bug 8261:  Now show the collation order information */
    /* Show the collation order if one has been specified.  If the collation
       order is the default for the character set being used, then no collation
       order will be shown ( because it isn't needed 

       If the collation id is 0, then the default for the character set is 
       being used so there is no need to retrieve the collation information.*/

    if (!FLD.RDB$COLLATION_ID.NULL && FLD.RDB$COLLATION_ID != 0)
        {
	char_sets[0] = 0;
	ISQL_get_character_sets (FLD.RDB$CHARACTER_SET_ID, FLD.RDB$COLLATION_ID, TRUE, char_sets);
	if (char_sets[0])
	    ISQL_printf (Out, char_sets);
        }

    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);
END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}

static void list_exception (void)
{
/**************************************
 *
 *	l i s t _ e x c e p t i o n
 *
 **************************************
 *
 * Functional description
 *	List all exceptions defined in the database
 *
 *	Parameters:  none
 *
 **************************************/
SSHORT 	first = TRUE;

FOR EXC IN RDB$EXCEPTIONS
    SORTED BY EXC.RDB$EXCEPTION_NAME

    if (first)
	{
	sprintf (Print_buffer, "%s/*  Exceptions */%s", NEWLINE, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first = FALSE;
    ISQL_blankterm (EXC.RDB$EXCEPTION_NAME);

    ISQL_copy_SQL_id (EXC.RDB$MESSAGE, &SQL_identifier2, SINGLE_QUOTE);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (EXC.RDB$EXCEPTION_NAME, &SQL_identifier, DBL_QUOTE);
	sprintf (Print_buffer, "CREATE EXCEPTION %s %s%s%s", 
		SQL_identifier, SQL_identifier2, Term, NEWLINE);
	}
    else
	sprintf (Print_buffer, "CREATE EXCEPTION %s %s%s%s", 
		EXC.RDB$EXCEPTION_NAME, SQL_identifier2, Term, NEWLINE);

    ISQL_printf (Out, Print_buffer); 


END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}

static void list_filters (void)
{
/**************************************
 *
 *	l i s t _ f i l t e r s
 *
 **************************************
 *
 * Functional description
 *	List all blob filters
 *
 *	Parameters:  none
 * Results in 
 * DECLARE FILTER <fname> INPUT_TYPE <blob_sub_type> OUTPUT_TYPE <blob_subtype>
 *		 ENTRY_POINT <string> MODULE_NAME <string>
 **************************************/
SSHORT 	first = TRUE;

FOR FIL IN RDB$FILTERS 
    SORTED BY FIL.RDB$FUNCTION_NAME

    ISQL_blankterm (FIL.RDB$FUNCTION_NAME);
    ISQL_blankterm (FIL.RDB$MODULE_NAME);
    ISQL_blankterm (FIL.RDB$ENTRYPOINT);

    if (first)
	{
	sprintf (Print_buffer, "%s/*  BLOB Filter declarations */%s",
		 NEWLINE,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first = FALSE;

    sprintf (Print_buffer, "DECLARE FILTER %s INPUT_TYPE %d OUTPUT_TYPE %d%s%sENTRY_POINT '%s' MODULE_NAME '%s'%s%s%s",
	FIL.RDB$FUNCTION_NAME, FIL.RDB$INPUT_SUB_TYPE, FIL.RDB$OUTPUT_SUB_TYPE, 
	NEWLINE, TAB, FIL.RDB$ENTRYPOINT, FIL.RDB$MODULE_NAME, Term, NEWLINE,
	NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

}

static void list_foreign ()
{
/**************************************
 *
 *	l i s t _ f o r e i g n
 *
 **************************************
 *
 * Functional description
 *	List all foreign key constraints and alter the tables
 *
 **************************************/
SCHAR	*collist;

collist = NULL;
collist = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH512 * 2);
if (!collist) 
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    return;
    }

/* Static queries for obtaining foreign constraints, where RELC1 is the
   foreign key constraints, RELC2 is the primary key lookup and REFC
   is the join table */

FOR RELC1 IN RDB$RELATION_CONSTRAINTS CROSS
	RELC2 IN RDB$RELATION_CONSTRAINTS CROSS
	REFC IN RDB$REF_CONSTRAINTS WITH 
	RELC1.RDB$CONSTRAINT_TYPE EQ "FOREIGN KEY" AND
	REFC.RDB$CONST_NAME_UQ EQ RELC2.RDB$CONSTRAINT_NAME AND
	REFC.RDB$CONSTRAINT_NAME EQ RELC1.RDB$CONSTRAINT_NAME AND 
	(RELC2.RDB$CONSTRAINT_TYPE EQ "UNIQUE" OR
	 RELC2.RDB$CONSTRAINT_TYPE EQ "PRIMARY KEY")
	SORTED BY RELC1.RDB$RELATION_NAME, RELC1.RDB$CONSTRAINT_NAME

        ISQL_blankterm (RELC1.RDB$RELATION_NAME);
        ISQL_blankterm (RELC2.RDB$RELATION_NAME);

        ISQL_get_index_segments (collist, RELC1.RDB$INDEX_NAME, TRUE);

	sprintf (Print_buffer, " %s", NEWLINE);
        ISQL_printf (Out, Print_buffer);

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (RELC1.RDB$RELATION_NAME, &SQL_identifier,
			      DBL_QUOTE);
	    sprintf (Print_buffer, "ALTER TABLE %s ADD ", SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "ALTER TABLE %s ADD ", 
		     RELC1.RDB$RELATION_NAME);
        ISQL_printf (Out, Print_buffer);

        /* If the name of the constraint is not INTEG..., print it.
	   INTEG... are internally generated names. */
        if (!RELC1.RDB$CONSTRAINT_NAME.NULL && 
	    strncmp(RELC1.RDB$CONSTRAINT_NAME, "INTEG", 5))
	    {
            ISQL_truncate_term (RELC1.RDB$CONSTRAINT_NAME,
				strlen(RELC1.RDB$CONSTRAINT_NAME));
	    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
		ISQL_copy_SQL_id (RELC1.RDB$CONSTRAINT_NAME, &SQL_identifier,
				  DBL_QUOTE);
		sprintf (Print_buffer, "CONSTRAINT %s ", SQL_identifier);
		}
	    else
		sprintf (Print_buffer, "CONSTRAINT %s ",
			 RELC1.RDB$CONSTRAINT_NAME);
	    ISQL_printf (Out, Print_buffer);
	    }

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    {
	    ISQL_copy_SQL_id (RELC2.RDB$RELATION_NAME, &SQL_identifier,
			      DBL_QUOTE);
	    sprintf (Print_buffer, "FOREIGN KEY (%s) REFERENCES %s ", 
		     collist, SQL_identifier);
	    }
	else
	    sprintf (Print_buffer, "FOREIGN KEY (%s) REFERENCES %s ", 
		     collist, 
		     RELC2.RDB$RELATION_NAME);
        ISQL_printf (Out, Print_buffer);

        /* Get the column list for the primary key */

        ISQL_get_index_segments (collist, RELC2.RDB$INDEX_NAME, TRUE);
    
        sprintf (Print_buffer, "(%s)", collist);
        ISQL_printf (Out, Print_buffer);

        /* Add the referential actions, if any */
	if (!REFC.RDB$UPDATE_RULE.NULL)
	    {
	    ISQL_truncate_term (REFC.RDB$UPDATE_RULE,
	        strlen(REFC.RDB$UPDATE_RULE));
	    ISQL_ri_action_print (REFC.RDB$UPDATE_RULE, " ON UPDATE", TRUE);
	    }

        if (!REFC.RDB$DELETE_RULE.NULL)
	    {
            ISQL_truncate_term (REFC.RDB$DELETE_RULE,
	        strlen(REFC.RDB$DELETE_RULE));
	    ISQL_ri_action_print (REFC.RDB$DELETE_RULE, " ON DELETE", TRUE);
	    }

        sprintf (Print_buffer, "%s%s", Term, NEWLINE);
        ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	ISQL_FREE (collist);
	return;
    END_ERROR;

ISQL_FREE (collist);

}

static void list_functions (void)
{
/**************************************
 *
 *	l i s t _ f u n c t i o n s
 *
 **************************************
 *
 * Functional description
 *	List all external functions
 *
 *	Parameters:  none
 * Results in 
 * DECLARE EXTERNAL FUNCTION function_name
 *               CHAR [256] , INTEGER, ....
 *               RETURNS INTEGER BY VALUE
 *               ENTRY_POINT entrypoint MODULE_NAME module;
 **************************************/
SSHORT 	first = TRUE, firstarg = TRUE;
SSHORT  i;
SCHAR	*return_buffer;
SCHAR	*type_buffer;
SCHAR	*buffer;

buffer         = NULL;
type_buffer    = NULL;
return_buffer  = NULL;

buffer = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH360);
if (!buffer)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    return;
    }

type_buffer = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!type_buffer)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    ISQL_FREE (buffer);
    return;
    }

return_buffer = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!return_buffer)
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    ISQL_FREE (type_buffer);
    ISQL_FREE (buffer);
    return;
    }

FOR FUN IN RDB$FUNCTIONS 
    SORTED BY FUN.RDB$FUNCTION_NAME

    ISQL_blankterm (FUN.RDB$FUNCTION_NAME);
    ISQL_blankterm (FUN.RDB$MODULE_NAME);
    ISQL_blankterm (FUN.RDB$ENTRYPOINT);
    if (first)
	{
	sprintf (Print_buffer, "%s/*  External Function declarations */%s",
		 NEWLINE,
	   	 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	first = FALSE;
	}

    /* Start new function declaration */
    sprintf (Print_buffer, "DECLARE EXTERNAL FUNCTION %s %s", 
	     FUN.RDB$FUNCTION_NAME,
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);

    firstarg = TRUE;

    FOR FNA IN RDB$FUNCTION_ARGUMENTS WITH
	FUN.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME
	SORTED BY FNA.RDB$ARGUMENT_POSITION

	/* Find parameter type */
	i = 0;
	while (FNA.RDB$FIELD_TYPE != Column_types [i].type)
	    i++;

	/* Print length where appropriate */
	if ((FNA.RDB$FIELD_TYPE == FCHAR)   || 
	    (FNA.RDB$FIELD_TYPE == VARCHAR) || 
	    (FNA.RDB$FIELD_TYPE == CSTRING))
	    {
	    USHORT	did_charset;
	    did_charset = 0;
    	    FOR CHARSET IN RDB$CHARACTER_SETS 
		WITH CHARSET.RDB$CHARACTER_SET_ID = FNA.RDB$CHARACTER_SET_ID

		    did_charset = 1;
		    ISQL_blankterm (CHARSET.RDB$CHARACTER_SET_NAME);
		    sprintf (type_buffer, "%s(%d) CHARACTER SET %s", 
				Column_types [i].type_name,
				(FNA.RDB$FIELD_LENGTH / MAX (1,CHARSET.RDB$BYTES_PER_CHARACTER)),
				CHARSET.RDB$CHARACTER_SET_NAME);

	    END_FOR
		ON_ERROR
		    ISQL_errmsg (gds__status);
		    return;
	 	END_ERROR;

            if (!did_charset)
                sprintf (type_buffer, "%s(%d)",  Column_types [i].type_name,
                        FNA.RDB$FIELD_LENGTH);
            }
        else
          {
            BOOLEAN precision_known = FALSE;
            
            if ( (major_ods >= ODS_VERSION10) &&
                 ((FNA.RDB$FIELD_TYPE == SMALLINT) ||
                  (FNA.RDB$FIELD_TYPE == INTEGER) ||
                  (FNA.RDB$FIELD_TYPE == blr_int64)))
                {
                FOR FNA1 IN RDB$FUNCTION_ARGUMENTS WITH
                    FNA1.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME AND
                    FNA1.RDB$ARGUMENT_POSITION EQ FNA.RDB$ARGUMENT_POSITION
                        
                    /* We are ODS >= 10 and could be any Dialect */
                    if (!FNA1.RDB$FIELD_PRECISION.NULL)
                        {
                        /* We are Dialect >=3 since FIELD_PRECISION is non-NULL */
                        if (FNA1.RDB$FIELD_SUB_TYPE > 0 &&
                            FNA1.RDB$FIELD_SUB_TYPE <= MAX_INTSUBTYPES)
                            {
                            sprintf (type_buffer, "%s(%d, %d)", 
                                     Integral_subtypes [FNA1.RDB$FIELD_SUB_TYPE],
                                     FNA1.RDB$FIELD_PRECISION,
                                    -FNA1.RDB$FIELD_SCALE);
                            precision_known = TRUE;
                            }
		        } /* if field_precision is not null */
                END_FOR
                    ON_ERROR
                        ISQL_errmsg (isc_status);
                        return;
                    END_ERROR;
                } /* if major_ods >= ods_version10 && */

                if (!precision_known)
                    {

                    /* Take a stab at numerics and decimals */
                    if ((FNA.RDB$FIELD_TYPE == SMALLINT) &&
                        (FNA.RDB$FIELD_SCALE < 0))
                        {
                        sprintf (type_buffer, "NUMERIC(4, %d)",
                                 -FNA.RDB$FIELD_SCALE);
                        }
                    else if ((FNA.RDB$FIELD_TYPE == INTEGER) &&
                             (FNA.RDB$FIELD_SCALE < 0))
                        {
                        sprintf (type_buffer, "NUMERIC(9, %d)",
                                 -FNA.RDB$FIELD_SCALE);
                        }
                    else if ((FNA.RDB$FIELD_TYPE == DOUBLE_PRECISION) &&
                        (FNA.RDB$FIELD_SCALE < 0))
                        {
                        sprintf (type_buffer, "NUMERIC(15, %d)",
                                 -FNA.RDB$FIELD_SCALE);
                        }
                    else
                        sprintf (type_buffer, "%s",
                                 Column_types [i].type_name);
                    } /* if !precision_known */
                } /* if FCHAR or VARCHAR or CSTRING ... else */

        /* If a return argument, save it for the end, otherwise print */

        if (FUN.RDB$RETURN_ARGUMENT == FNA.RDB$ARGUMENT_POSITION)
            sprintf (return_buffer, "RETURNS %s %s %s", type_buffer, 
                ((SSHORT) abs (FNA.RDB$MECHANISM) == FUN_reference ? "" : "BY VALUE"), 
                (FNA.RDB$MECHANISM < 0 ? "FREE_IT" : ""));
        else
            {
            /* First arg needs no comma */
            sprintf (Print_buffer, "%s%s", (firstarg ? "" : ", "), type_buffer);
            ISQL_printf (Out, Print_buffer);
            firstarg = FALSE;
            }

    END_FOR
	ON_ERROR
	    ISQL_errmsg (gds__status);
	    ISQL_FREE (type_buffer);
	    ISQL_FREE (return_buffer);
	    ISQL_FREE (buffer);
	    return;
	END_ERROR;

    /* Print the return type -- All functions return a type */
    sprintf (Print_buffer, "%s%s%s",
		NEWLINE,
		return_buffer,
		NEWLINE);
    ISQL_printf (Out, Print_buffer);

    /* Print out entrypoint information */
    sprintf (Print_buffer, "ENTRY_POINT '%s' MODULE_NAME '%s'%s%s%s",
		FUN.RDB$ENTRYPOINT, 
		FUN.RDB$MODULE_NAME,
		Term, 
		NEWLINE, 
		NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	ISQL_FREE (type_buffer);
	ISQL_FREE (return_buffer);
	ISQL_FREE (buffer);
	return;
    END_ERROR;

ISQL_FREE (type_buffer);
ISQL_FREE (return_buffer);
ISQL_FREE (buffer);
}


static void list_generators (void)
{
/**************************************
 *
 *	l i s t _ g e n e r a t o r s
 *
 **************************************
 *
 * Functional description
 *	Re create all non-system generators
 *
 **************************************/

ISQL_printf (Out, NEWLINE);

FOR GEN IN RDB$GENERATORS WITH
    GEN.RDB$GENERATOR_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *" AND
    GEN.RDB$GENERATOR_NAME NOT MATCHING "SQL$+" USING "+=[0-9][0-9]* *" AND
    (GEN.RDB$SYSTEM_FLAG MISSING OR GEN.RDB$SYSTEM_FLAG NE 1)
    SORTED BY GEN.RDB$GENERATOR_NAME

    ISQL_blankterm (GEN.RDB$GENERATOR_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	ISQL_copy_SQL_id (GEN.RDB$GENERATOR_NAME, &SQL_identifier, DBL_QUOTE);
    else
	strcpy (SQL_identifier, GEN.RDB$GENERATOR_NAME);

    sprintf (Print_buffer, "CREATE GENERATOR %s%s%s", 
	    SQL_identifier,
	    Term,
	    NEWLINE); 
    ISQL_printf (Out, Print_buffer);
END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;

ISQL_printf (Out, NEWLINE);
}

static void list_index ()
{
/**************************************
 *
 *	l i s t _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Define all non-constraint indices
 *	Use a static SQL query to get the info and print it.
 *
 *	Uses get_index_segment to provide a key list for each index
 *
 **************************************/
SCHAR	*collist;
SSHORT	first = TRUE;


collist = NULL;
collist = (SCHAR *) ISQL_ALLOC (BUFFER_LENGTH512 * 2);
if (!collist) 
    {
    isc_status [0] = isc_arg_gds;
    isc_status [1] = isc_virmemexh;
    isc_status [2] = isc_arg_end;
    ISQL_errmsg (isc_status);
    return;
    }

FOR IDX IN RDB$INDICES CROSS RELC IN RDB$RELATIONS
        OVER RDB$RELATION_NAME
        WITH (RELC.RDB$SYSTEM_FLAG NE 1 OR RELC.RDB$SYSTEM_FLAG MISSING)
        AND NOT (ANY RC IN RDB$RELATION_CONSTRAINTS
           WITH RC.RDB$INDEX_NAME EQ IDX.RDB$INDEX_NAME)
        SORTED BY IDX.RDB$RELATION_NAME, IDX.RDB$INDEX_NAME


    if (first)
	{

	sprintf (Print_buffer, 
		 "%s/*  Index definitions for all user tables */%s",
		 NEWLINE,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    first = FALSE;
    /* Strip trailing blanks */

    ISQL_blankterm (IDX.RDB$RELATION_NAME);
    ISQL_blankterm (IDX.RDB$INDEX_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
	ISQL_copy_SQL_id (IDX.RDB$INDEX_NAME,    &SQL_identifier,  DBL_QUOTE);
	ISQL_copy_SQL_id (IDX.RDB$RELATION_NAME, &SQL_identifier2, DBL_QUOTE);
	sprintf (Print_buffer, "CREATE%s%s INDEX %s ON %s(",
	    (IDX.RDB$UNIQUE_FLAG ? " UNIQUE" : ""),
	    (IDX.RDB$INDEX_TYPE ? " DESCENDING" : ""),
	    SQL_identifier,
	    SQL_identifier2);
	}
    else
	sprintf (Print_buffer, "CREATE%s%s INDEX %s ON %s(",
	    (IDX.RDB$UNIQUE_FLAG ? " UNIQUE" : ""),
	    (IDX.RDB$INDEX_TYPE ? " DESCENDING" : ""),
	    IDX.RDB$INDEX_NAME, 
	    IDX.RDB$RELATION_NAME);
    ISQL_printf (Out, Print_buffer);

    /* Get column names */

    if (ISQL_get_index_segments (collist, IDX.RDB$INDEX_NAME, TRUE))
	{
	sprintf (Print_buffer, "%s)%s%s", collist, Term, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	ISQL_FREE (collist);
	return;
    END_ERROR;

ISQL_FREE (collist);
}

static void list_views ()
{
/**************************************
 *
 *	l i s t _ v i e w s
 *
 **************************************
 *
 * Functional description
 *	Show text of views.
 *	Use a SQL query to get the info and print it.
 *	Note: This should also contain check option
 *
 **************************************/
SSHORT 	first;

/* If this is a view, use print_blob to print the view text */

FOR REL IN RDB$RELATIONS WITH
	(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
	REL.RDB$VIEW_BLR NOT MISSING AND
	REL.RDB$FLAGS = REL_sql
	SORTED BY REL.RDB$RELATION_ID
	
    first = TRUE;
    ISQL_blankterm (REL.RDB$RELATION_NAME);

    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	ISQL_copy_SQL_id (REL.RDB$RELATION_NAME, &SQL_identifier, DBL_QUOTE);
    else
	strcpy (SQL_identifier, REL.RDB$RELATION_NAME);

    ISQL_blankterm (REL.RDB$OWNER_NAME);

    sprintf (Print_buffer, "%s/* View: %s, Owner: %s */%s",
	     NEWLINE,
	     REL.RDB$RELATION_NAME, 
	     REL.RDB$OWNER_NAME,
	     NEWLINE);
    ISQL_printf (Out, Print_buffer);
    sprintf (Print_buffer, "CREATE VIEW %s (", SQL_identifier);
    ISQL_printf (Out, Print_buffer);

    /* Get column list */
    FOR RFR IN RDB$RELATION_FIELDS WITH
	RFR.RDB$RELATION_NAME = REL.RDB$RELATION_NAME
	SORTED BY RFR.RDB$FIELD_POSITION

	ISQL_blankterm (RFR.RDB$FIELD_NAME);

	if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	    ISQL_copy_SQL_id (RFR.RDB$FIELD_NAME, &SQL_identifier, DBL_QUOTE);
	else
	    strcpy (SQL_identifier, RFR.RDB$FIELD_NAME);

        sprintf (Print_buffer, "%s%s", (first ? "" : ", "), SQL_identifier);
	ISQL_printf (Out, Print_buffer);
	first = FALSE;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (gds__status);
	    return;
	END_ERROR;
    sprintf (Print_buffer, ") AS%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);

    if (!REL.RDB$VIEW_SOURCE.NULL)
	SHOW_print_metadata_text_blob (Out, &REL.RDB$VIEW_SOURCE);

    sprintf (Print_buffer, "%s%s", Term, NEWLINE);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (gds__status);
	return;
    END_ERROR;
}
