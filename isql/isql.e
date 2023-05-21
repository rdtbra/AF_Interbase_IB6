/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		isql.e
 *	DESCRIPTION:	Main line routine
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
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#define TIME_ERR	101
#define	BLANK			'\040'
#define	DBL_QUOTE		'\042'
#define	SINGLE_QUOTE		'\''
#define	KW_FILE			"IB_FILE"
#define	INIT_STR_FLAG		0
#define	SINGLE_QUOTED_STRING	1
#define	DOUBLE_QUOTED_STRING	2
#define	NO_MORE_STRING		3
#define INCOMPLETE_STRING	4
#define	KW_ROLE			"ROLE"

#include "../jrd/time.h"

#define	ISQL_MAIN
#include "../jrd/common.h"
#if defined(WIN_NT) || defined(GUI_TOOLS)
#include <windows.h>
#endif
#include "../jrd/codes.h"
#include "../jrd/gds.h"
#include "../isql/isql.h"
#include "../jrd/perf.h"
#include "../jrd/license.h"
#include "../jrd/constants.h"
#include "../jrd/ods.h"
#include "../isql/extra_proto.h"
#include "../isql/isql_proto.h"
#include "../isql/show_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/perf_proto.h"
#include "../jrd/utl_proto.h"
#ifdef GUI_TOOLS
#include "../wisql/isqlcomm.h"
#endif
#ifdef WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif
#include "../jrd/gdsassert.h"

DATABASE DB = COMPILETIME "yachts.lnk";

#define STUFF(x)      *dpb++ = (x)
#define STUFF_INT(x)  {STUFF (x); STUFF ((x) >> 8); STUFF ((x) >> 16); STUFF ((x) >> 24);}

#define DIGIT(c)        ((c) >= '0' && (c) <= '9')
#define INT64_LIMIT     ((((SINT64) 1) << 62) / 5)  /* same as in cvt.c */

/*  Print lengths of numeric values */

#define MAXCHARSET_LENGTH  32   /* CHARSET names */
#define SHORT_LEN	7       /* NUMERIC (4,2) = -327.68 */
#define LONG_LEN	12	/* NUMERIC (9,2) = -21474836.48 */
#define INT64_LEN	21	/* NUMERIC(18,2) = -92233720368547758.08 */
#define QUAD_LEN	19
#define FLOAT_LEN	14	/* -1.2345678E+38 */
#define DOUBLE_LEN	23	/* -1.234567890123456E+300 */
#define DATE_LEN	11	/* 11 for date only */
#define DATETIME_LEN	25	/* 25 for date-time */
#define TIME_ONLY_LEN   13      /* 13 for time only */
#define DATE_ONLY_LEN   11
#define UNKNOWN_LEN     20      /* Unknown type: %d */

#define MAX_TERMS	10   /* max # of terms in an interactive cmd */

#define COMMIT_TRANS(x)	{if (isc_commit_transaction (isc_status, x)) { ISQL_errmsg (isc_status); isc_rollback_transaction (isc_status, x); }}

typedef struct indev {
    struct indev	*indev_next;
    IB_FILE		*indev_fpointer;
} *INDEV;

typedef struct collist {
    struct collist 	*collist_next;
    TEXT		col_name [WORDLENGTH];
    SSHORT		col_len;
} COLLIST;

typedef struct   ri_actions {
    SCHAR   *ri_action_name;
    SCHAR   *ri_action_print_caps;
    SCHAR   *ri_action_print_mixed;
} RI_ACTIONS;

static SSHORT	add_row (TEXT *);
static SSHORT	blobedit (TEXT *, TEXT **);
static void	col_check (TEXT *, SSHORT *);
static void	copy_str (TEXT **, TEXT **, BOOLEAN *, 
				 TEXT *, TEXT *, SSHORT);
static SSHORT	copy_table (TEXT *, TEXT *, TEXT *);
static SSHORT	create_db (TEXT *, TEXT *);
static void	do_isql (void);
static SSHORT	drop_db (void);
static SSHORT	edit (TEXT **);
static int	end_trans (void);
static SSHORT	escape (TEXT *);
static SSHORT	frontend (TEXT *);
static SSHORT	get_statement (TEXT **, USHORT *, TEXT*);
static BOOLEAN 	get_numeric (UCHAR *, USHORT, SSHORT *, SINT64 *);
static void	get_str (TEXT *, TEXT **, TEXT **, SSHORT *);
static SSHORT	print_sets (void);
static SSHORT	help (TEXT *);
static SSHORT	input (TEXT *);
static BOOLEAN	isyesno (TEXT *);
static SSHORT	newdb (TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
static SSHORT	newoutput (TEXT *);
static SSHORT	newsize (TEXT *, TEXT *);
static SSHORT	newtrans (TEXT *);
static SSHORT	parse_arg (int, SCHAR **, SCHAR *, IB_FILE **);
static SSHORT	print_item (TEXT **, XSQLVAR *, SLONG);
static int 	print_item_blob (IB_FILE *, XSQLVAR *, void *);
static void 	print_item_numeric (SINT64, SSHORT, SSHORT, TEXT *);
static int	print_line (XSQLDA *, SLONG *, TEXT *);
static int	process_statement (TEXT *, XSQLDA **);
static void 	CLIB_ROUTINE query_abort (void);
static void	strip_quotes (TEXT *, TEXT *);
static SSHORT	do_set_command (TEXT *, SSHORT *);
#ifdef DEV_BUILD
static UCHAR	*sqltype_to_string (USHORT);
#endif

XSQLDA		*sqlda;
XSQLDA   	**sqldap;
USHORT		dialect_spoken = 0;
USHORT		requested_SQL_dialect = SQL_DIALECT_V6;
BOOLEAN		connecting_to_pre_v6_server = FALSE;
SSHORT		V45 = FALSE;
SSHORT		V4  = FALSE;
SSHORT		V33 = FALSE;
USHORT		major_ods = 0;
USHORT		minor_ods = 0;
SSHORT		Quiet;
#if defined (WIN95) && !defined (GUI_TOOLS)
BOOL		fAnsiCP;
#endif

static COLLIST	*Cols = NULL;
static SCHAR	statistics [256];
static INDEV	Filelist;
static TEXT	Tmpfile [128];
static int	Abort_flag = 0;
static SSHORT	Echo = FALSE;
static SSHORT	Time_display = FALSE;
static SSHORT   Sqlda_display = FALSE;
static SSHORT	Exit_value;
static BOOLEAN	Continuation = FALSE;
static BOOLEAN	Interactive = TRUE;
static BOOLEAN	Input_file = FALSE;
static SSHORT	Pagelength = 20;
static SSHORT	Stats = FALSE;
static SSHORT	Autocommit = TRUE;  /* Commit ddl */
static SSHORT	Warnings = TRUE;    /* Print warnings */
static SSHORT	Autofetch = TRUE;   /* automatically fetch all records */
static USHORT	fetch_direction = 0;
static SLONG	fetch_offset = 1;	
static SSHORT	Doblob = 1;	/* Default to printing only text types */
static SSHORT	List = FALSE;	
static SSHORT 	Docount = FALSE;
static SSHORT	Plan = FALSE;
static int	Termlen = 0;
static SCHAR	ISQL_charset[MAXCHARSET_LENGTH] = {0};
static IB_FILE	*Diag;
static IB_FILE	*Help;
static TEXT	*sql_prompt = "SQL> ";
static TEXT	*fetch_prompt = "FETCH> ";

#ifdef GUI_TOOLS
static IB_FILE	*Session; 		/* WISQL session file */
static TEXT	*SaveOutfileName;	/* WISQL output file name */
#endif 

static BOOLEAN	global_psw = FALSE;
static BOOLEAN	global_usr = FALSE;
static BOOLEAN	global_role = FALSE;
static BOOLEAN	global_numbufs = FALSE;
static BOOLEAN	have_trans = FALSE;	/* translation of word "Yes" */
static TEXT	yesword [MSG_LENGTH];   
static TEXT	Print_buffer [PRINT_BUFFER_LENGTH];

static UCHAR	db_version_info [] = { isc_info_ods_version,
					isc_info_ods_minor_version,
					isc_info_db_sql_dialect };
static UCHAR	blr_bpb [] = { isc_bpb_version1, 
				isc_bpb_source_type, 1, BLOB_blr,
	 			isc_bpb_target_type, 1, BLOB_text },
		text_bpb [] = { isc_bpb_version1, 
				isc_bpb_source_type, 1, BLOB_text,
				isc_bpb_target_type, 1, BLOB_text },
		acl_bpb [] = { isc_bpb_version1, 
				isc_bpb_source_type, 1, BLOB_acl,
				isc_bpb_target_type, 1, BLOB_text };

/* Note that these transaction options aren't understood in Version 3.3 */
static UCHAR	default_tpb [] = {isc_tpb_version1, isc_tpb_write,
				isc_tpb_read_committed, isc_tpb_wait,
				isc_tpb_no_rec_version};

/* InterBase Performance Statistic format Variables */

static struct perf	perf_before, perf_after;
static BOOLEAN	have_report = FALSE;	/* translation of report strings */
static TEXT	report_1 [MSG_LENGTH+MSG_LENGTH];
static TEXT	report_2 [MSG_LENGTH];
static TEXT	rec_count [MSG_LENGTH];

/* if the action is rrstrict, do not print anything at all */
static RI_ACTIONS ri_actions_all [] = { 
   RI_ACTION_CASCADE, RI_ACTION_CASCADE,  "Cascade",
   RI_ACTION_NULL,    RI_ACTION_NULL,     "Set Null",
   RI_ACTION_DEFAULT, RI_ACTION_DEFAULT,  "Set Default",
   RI_ACTION_NONE,    RI_ACTION_NONE,     "No Action",
   RI_RESTRICT,       "",                 "",
   "",		      "", 	          "",
   0, 0, 0
   };
    

#ifndef GUI_TOOLS
int CLIB_ROUTINE main (
    int		argc,
    char	*argv[])
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	This calls ISQL_main, and exists to
 *	isolate main which does not exist under
 *	MS Windows.
 *
 **************************************/

#if defined (WIN95) && !defined (GUI_TOOLS)
{
ULONG	ulConsoleCP;

ulConsoleCP = GetConsoleCP();
if (ulConsoleCP == GetACP())
    fAnsiCP = TRUE;
else
    {
    fAnsiCP = FALSE;
    if (ulConsoleCP != GetOEMCP() && Warnings)
	{
	ISQL_printf(Out,
		"WARNING: The current codepage is not supported.  Any use of any\n"
		"         extended characters may result in incorrect file names.\n");
	}
    }
}
#endif

Exit_value = ISQL_main (argc, argv);

exit (Exit_value);
}
#endif

SSHORT ISQL_main (
    int		argc,
    char	*argv[])
{
/**************************************
 *
 *	I S Q L _ m a i n
 *
 **************************************
 *
 * Functional description
 *	Choose between reading and executing or generating SQL
 *	ISQL_main isolates this from main for PC Clients.
 *
 **************************************/
SSHORT	ret;
TEXT	helpstring[155]; 
TEXT	tabname [WORDLENGTH];

#ifdef MU_ISQL
/*
** This is code for QA Test bed Multiuser environment.
**
** Setup multi-user test environment.
*/
if (qa_mu_environment())
    {
    /*
    ** Initialize multi-user test manager only if in that
    ** environment.
    */
    if (!qa_mu_init(argc, argv, 1))
	{
	/*
	** Some problem in initializing multi-user test
	** environment.
	*/
	qa_mu_cleanup();
	exit(1);
        }
    else
	{
	/*
	** When invoked by MU, additional command-line argument
	** was added at tail. ISQL is not interested in it so
	** remove it.
	*/
	argv[argc - 1] = NULL;
	argc--;
        }
    }
#endif MU_ISQL

tabname [0] = '\0';
db_SQL_dialect = 0;

#ifdef GUI_TOOLS
/* 
 * Since there is no concept of stdout & stderr, we set Out and Errfp to 
 * NULL initially.
*/
Out 		= NULL;
Errfp		= NULL;

ret = parse_arg (argc, argv, tabname, &Session);

/* Init the diagnostics and help files */
Echo            = TRUE;
Diag            = Out;
Help            = Out;
#else
/* Output goes to ib_stdout by default */
Out   = ib_stdout;
Errfp = ib_stderr;

ret = parse_arg (argc, argv, tabname, NULL);

/* Init the diagnostics and help files */
Diag            = ib_stdout;
Help            = ib_stdout;
#endif

if (Merge_stderr)
    Errfp = Out;

ISQL_make_upper (tabname);
switch (ret)
    {
    case EXTRACT:
	/* This is a call to do extractions */

	if (*Db_name)
	    Exit_value = EXTRACT_ddl (SQL_objects, tabname);
	break;

    case EXTRACTALL:
	if (*Db_name)
	    Exit_value = EXTRACT_ddl (ALL_objects, tabname);
	break;

    case ERR:
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, USAGE1, sizeof (helpstring),
	    		     helpstring, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (helpstring, 1);
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, USAGE2, sizeof (helpstring),
	    		     helpstring, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (helpstring, 1);

	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, USAGE3, sizeof (helpstring),
	    		     helpstring, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (helpstring, 1);
	Exit_value = FINI_ERROR;
	break;

    default:
	do_isql();
	Exit_value = FINI_OK;
	break;
    }
#ifdef MU_ISQL
/*
** This is code for QA Test bed Multiuser environment.
**
** Do multi-user cleanup if running in multi-user domain.
** qa_mu_cleanup() is NOP in non-multi-user domain.
*/
qa_mu_cleanup();
#endif MU_ISQL

#ifdef DEBUG_GDS_ALLOC
/* As ISQL can run under windows, all memory should be freed before
 * returning.  In debug mode this call will report unfreed blocks.
 */
gds_alloc_report (0, __FILE__, __LINE__);
#endif

#ifdef GUI_TOOLS
/* Free the original WISQL output file name */
if (SaveOutfileName)
    {
    ISQL_FREE (SaveOutfileName);
    SaveOutfileName = NULL;
    }
#endif
return Exit_value;
}

void ISQL_array_dimensions (
    TEXT	*fieldname)
{
/**************************************
 *
 *	I S Q L _ a r r a y _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Retrieves the dimensions of arrays and prints them.
 *
 *	Parameters:  fieldname -- the actual name of the array field
 *
 **************************************/

ISQL_printf (Out, "[");

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return;
	};

FOR FDIM IN RDB$FIELD_DIMENSIONS WITH
	FDIM.RDB$FIELD_NAME EQ fieldname
	SORTED BY FDIM.RDB$DIMENSION

    /*  Format is [lower:upper, lower:upper,..]  */

    if (FDIM.RDB$DIMENSION > 0)
	{
	ISQL_printf (Out, ", ");
	}
    sprintf (Print_buffer, "%ld:%ld", FDIM.RDB$LOWER_BOUND, FDIM.RDB$UPPER_BOUND);
    ISQL_printf (Out, Print_buffer);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return;
    END_ERROR;

ISQL_printf (Out, "] ");
}

SCHAR *ISQL_blankterm (
    TEXT	*str)
{
/**************************************
 *
 *	I S Q L _ b l a n k t e r m
 *
 **************************************
 *
 * Functional description
 *	Trim any trailing spaces off a string;
 *	eg: find the last non blank in the string and insert 
 *	a null byte after that.
 *
 *	SQL delimited identifier may have blank as part of the name
 *
 *	Parameters:  str - the string to terminate
 *	Returns:     str
 *
 **************************************/
TEXT    *p, *q;

/* Scan to the end-of-string, record position of last non-blank seen */
q = str-1;
for (p = str; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

*(q+1) = '\0';
return str;
}

BOOLEAN ISQL_dbcheck (void)
{
/**************************************
 *
 *	I S Q L _ d b c h e c k
 *
 **************************************
 *
 * Functional description
 *	Check to see if we are connected to a database.
 *  	Return TRUE if connected, FALSE otherwise
 *
 **************************************/
TEXT	errbuf [MSG_LENGTH];

if (!Db_name[0])
    {
    if (!Quiet)
	{
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, NO_DB, sizeof (errbuf), 
		errbuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (errbuf, 1);
#ifdef GUI_TOOLS
        /*
         * This will force an extra carriage return line feed for better
         * formatting of errors when executing a script through windows ISQL.
         * The Merge_stderr flag is only set when scripts are running.
        */
        if (Merge_stderr)
            ISQL_printf(Out, NEWLINE);
#endif
	}
    else
	Exit_value = FINI_ERROR;
    return FALSE;
    }
else
    return TRUE;
}

void ISQL_prompt (
    TEXT	*string)
{
/**************************************
 *
 *	I S Q L _ p r o m p t 
 *
 **************************************
 *
 * Functional description
 *	Print a prompt string for interactive user
 *	Not for Windows, otherwise flush the string
 **************************************/
#ifndef GUI_TOOLS
ib_fprintf (ib_stdout, string);
ib_fflush (ib_stdout);
#endif
}

#ifdef GUI_TOOLS
int ISQL_commit_work (
    int	response,
    IB_FILE	*ipf,
    IB_FILE	*opf,
    IB_FILE	*sf)
{
/**************************************
 *
 *	I S Q L _ c o m m i t _ w o r k 
 *
 **************************************
 *
 * Functional description
 *	If response = WISQL_RESPONSE_YES then commit work.
 * 	If response = WISQL_RESPONSE_NO rollback.
 *
 **************************************/
ISC_STATUS		ret_code;


Out = opf;
Ifp = ipf;
Diag = Out;
Help = Out;

ret_code = 0;
/*
 * Commit transaction that was started on behalf of the request 
 * Don't worry about the error if one occurs it will be network 
 * related and caught later.
 */
if (gds__trans)
    isc_rollback_transaction (isc_status, &gds__trans);

if (response == WISQL_RESPONSE_NO)
    {
    /* add command to command history file */
    ib_fprintf (sf, "%s%s%s", "Rollback", Term, NEWLINE);
    ib_fflush (sf);
    if (D__trans)
        if (ret_code = isc_rollback_transaction (isc_status, &D__trans))
            ISQL_errmsg (isc_status);

    if (M__trans && !ret_code)
        if (ret_code = isc_rollback_transaction (isc_status, &M__trans))
            ISQL_errmsg (isc_status);

    if (!D__trans && !M__trans && !ret_code) 	
	{
	ISQL_printf (Out, "Work Rolled Back");
	ISQL_printf (Out, NEWLINE);
	}
    }
else
    {	
    /* add command to command history file */
    ib_fprintf (sf, "%s%s%s", "Commit", Term, NEWLINE);
    ib_fflush (sf);
    if (D__trans)
        if (ret_code = isc_commit_transaction (isc_status, &D__trans))
            ISQL_errmsg (isc_status);

    if (M__trans && !ret_code)
        if (ret_code = isc_commit_transaction (isc_status, &M__trans))
            ISQL_errmsg (isc_status);

    if (!D__trans && !M__trans && !ret_code) 	
	{
	ISQL_printf (Out, "Work Committed");
    	ISQL_printf (Out, NEWLINE);
	}
    }

return ((int) ret_code);
}
#endif

void ISQL_copy_SQL_id (
    TEXT	*in_str,
    TEXT	**output_str,
    TEXT	escape_char)
{
/**************************************
 *
 *	I S Q L _ c o p y _ S Q L _ i d
 *
 **************************************
 *
 * Functional description
 *
 *	Copy/rebuild the SQL identifier by adding escape double quote if
 *	double quote is part of the SQL identifier and raps around the 
 *	SQL identifier with delimited double quotes 
 *
 **************************************/
TEXT	*p1, *q1;

q1 = output_str;
*q1 = escape_char;
q1++;
for (p1 = in_str; *p1; p1++)
    {
    *q1 = *p1;
    q1++;
    if (*p1 == escape_char)
	{
	*q1 = escape_char;
	q1++;
	}
    }
*q1 = escape_char;
q1++;
*q1 = '\0';
}

void ISQL_errmsg (
    STATUS	*status)
{
/**************************************
 *
 *	I S Q L _ e r r m s g
 *
 **************************************
 *
 * Functional description
 *	Report error conditions
 *	Simulate isc_print_status exactly, to control ib_stderr 
 **************************************/
TEXT	errbuf [MSG_LENGTH];
STATUS	*vec;
#if defined (WIN95) && !defined (GUI_TOOLS)
#define TRANSLATE_CP if (!fAnsiCP) AnsiToOem(errbuf, errbuf)
#else
#define TRANSLATE_CP
#endif

vec = status;
if (Quiet)
    Exit_value = FINI_ERROR;
else
    {
    if (vec[0] != gds_arg_gds ||
	(vec[0] == gds_arg_gds && vec[1] == 0 && vec[2] != gds_arg_warning) ||
	(vec[0] == gds_arg_gds && vec[1] == 0 && vec[2] == gds_arg_warning &&
	 !Warnings))
	return;
    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, 0, sizeof (errbuf), errbuf,
	(TEXT*) isc_sqlcode (status), NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
#ifndef GUI_TOOLS
    TRANSLATE_CP;
    STDERROUT (errbuf, 1);
#else
    buffer_error_msg (errbuf, WISQL_SHORTMSG);
#endif

    isc_interprete (errbuf, &vec);
#ifndef GUI_TOOLS
    TRANSLATE_CP;
    STDERROUT (errbuf, 1);
#else
    buffer_error_msg (errbuf, WISQL_DETAILEDMSG);
    buffer_error_msg (NEWLINE, WISQL_DETAILEDMSG);
#endif

    /* Continuation of error */
    errbuf [0] = '-';
    while (isc_interprete (errbuf + 1, &vec))
	{
#ifndef GUI_TOOLS
	TRANSLATE_CP;
	STDERROUT (errbuf, 1);
#else
    	buffer_error_msg (errbuf, WISQL_DETAILEDMSG);
    	buffer_error_msg (NEWLINE, WISQL_DETAILEDMSG);
#endif
	}
#ifdef GUI_TOOLS
    /*
     * This will force an extra carriage return line feed for better 
     * formatting of errors when executing a script through windows 
     * ISQL.  The Merge_stderr flag is only set when scripts are 
     * running.
    */
    if (Merge_stderr)
    	ISQL_printf(Out, NEWLINE);

    /* Show the error dialog */
    wisqlerr ();
#endif
    }
}

void ISQL_warning (
    STATUS	*status)
{
/**************************************
 *
 *	I S Q L _ w a r n i n g
 *
 **************************************
 *
 * Functional desription
 *	Report warning
 *	Simulate isc_print_status exactly, to control ib_stderr
 **************************************/
STATUS	*vec;
TEXT	buf [MSG_LENGTH];
#if defined (WIN95) && !defined (GUI_TOOLS)
#define TRANSLATE_CP if (!fAnsiCP) AnsiToOem(buf, buf)
#else
#define TRANSLATE_CP
#endif


vec = status;
assert (vec[1] == 0);	/* shouldn't be any errors */

if (!Quiet)
    {
    if (vec[0] != gds_arg_gds ||
	vec[2] != gds_arg_warning ||
	(vec[2] == gds_arg_warning && !Warnings))
	return;
    isc_interprete (buf, &vec);
#ifndef GUI_TOOLS
    TRANSLATE_CP;
    STDERROUT (buf, 1);
#else
    buffer_error_msg (buf, WISQL_DETAILEDMSG);
    buffer_error_msg (NEWLINE, WISQL_DETAILEDMSG);
#endif

    /* Continuation of warning */
    buf [0] = '-';
    while (isc_interprete (buf + 1, &vec))
	{
	#ifndef GUI_TOOLS
	TRANSLATE_CP;
	STDERROUT (buf, 1);
#else
	buffer_error_msg (buf, WISQL_DETAILEDMSG);
	buffer_error_msg (NEWLINE, WISQL_DETAILEDMSG);
#endif
	}
#ifdef GUI_TOOLS
    /*
     * This will force an extra carriage return line feed for better
     * formatting of errors when executing a script through windows
     * ISQL.  The Merge_stderr flag is only set when scripts are
     * running.
    */
    if (Merge_stderr)
	ISQL_printf(Out, NEWLINE);

    /* Show the error dialog */
    wisqlerr ();
#endif
    }

status[2] == gds_arg_end;

}

#ifdef GUI_TOOLS
void ISQL_exit_db (void)
{
/**************************************
 *
 *	I S Q L _ e x i t _ d b
 *
 **************************************
 *
 * Functional description
 *	Detach from the database.
 *
 **************************************/

if (D__trans)
    isc_rollback_transaction (isc_status, &D__trans);
if (M__trans)
    isc_rollback_transaction (isc_status, &M__trans);
if (gds__trans)
    isc_rollback_transaction (isc_status, &gds__trans);

if (Stmt)
    isc_dsql_free_statement (isc_status, &Stmt, 2);

if (DB)
    isc_detach_database (isc_status, &DB);

DB = NULL;
Db_name[0] = '\0';
D__trans = NULL;
M__trans = NULL;
gds__trans = NULL;
Stmt = NULL;
if (sqlda)
    ISQL_FREE (sqlda);

}
#endif

#ifdef GUI_TOOLS
int ISQL_frontend_command (
    TEXT	*string,
    IB_FILE	*ipf,
    IB_FILE	*opf,
    IB_FILE	*sf)
{
/**************************************
 *
 *	I S Q L _ f r o n t e n d _ c o m m a n d
 *
 **************************************
 *
 * Functional description
 *	Process a frontend command for Windows ISQL.   This sets up
 *	the input and output files, and executes the statement.
 *
 **************************************/
SSHORT		 ret;
SCHAR		*endpassword, savechar;
int		 length;
SSHORT		 connect_stmt;

Out = opf;
Ifp = ipf;
Diag = Out;
Help = Out;

connect_stmt = FALSE;

if (strncmp (string, WISQL_CONNECT_LITERAL, strlen(WISQL_CONNECT_LITERAL)))
    {
    /* add command to command history file */
    ib_fprintf (sf, "%s%s%s", string, Term, NEWLINE);
    ib_fflush (sf);
	
    if (strcmp (string, WISQL_DROP_DATABASE_LITERAL))
  	{
    	/* Echo string to Output */
    	ISQL_printf (Out, string);
    	ISQL_printf (Out, NEWLINE);
	}
    }
else
    { 
    /* 
     *	Need to remove the users password from connect string.  Step through
     *	the string until you see the PASSWORD literal.  Save of the char,
     *	NULL terminate the string and print.  Restore the char and move
     *	on. 
    */			
    connect_stmt = TRUE;
    endpassword       = strstr (string, WISQL_PASSWORD_LITERAL);
    if (endpassword)
	 {	
    	 length	      = strlen (WISQL_PASSWORD_LITERAL);
    	 endpassword   = endpassword + length;
    	 savechar      = *endpassword;
    	 *endpassword  = NULL;	
    	
    	 /* add command to command history file */
    	 ib_fprintf (sf, "%s %s %s%s", 
		 string, 
		 WISQL_PASSWORD_COMMENT_LITERAL, 
		 Term, 
		 NEWLINE);
    	 ib_fflush (sf);
    	 *endpassword   = savechar;
	 }
    }		

ret     = (int) frontend (string);

if (ret == SKIP)
  ret  = 0;

if (!connect_stmt)  
    ISQL_printf (Out, NEWLINE);

return (ret);
}
#endif

#ifdef GUI_TOOLS
int ISQL_extract (
    SCHAR	*string,
    int  	type,
    IB_FILE	*ipf,
    IB_FILE	*opf,
    IB_FILE	*sf)
{
/**************************************
 *
 *	I S Q L _ e x t r a c t
 *
 **************************************
 *
 * Functional description
 *	Process a extract command from Windows ISQL.   This sets up
 *	the input and output files, and executes the statement.
 *
 **************************************/
int	ret;
isc_tr_handle 	save_trans_handle;

Out  	  = opf;
Ifp  	  = ipf;
Diag 	  = Out;
Help 	  = Out;
ret  	  = 0;
strcpy (Target_db, Db_name);

/* 
 *  Add command to command history file.  There is no way to perform an
 *  extract from an input script.  Will defer for now
 *
 *  ib_fprintf (sf, "%s%s%s \n\r", string, Term, NEWLINE);
 *  ib_fflush (sf);
 */

/* Save off the transaction handle so we can set it to NULL */
save_trans_handle  	= gds__trans;
gds__trans 	   	= NULL;

if (type == EXTRACT_VIEW)
    {
    ib_fprintf (sf, "/* Extract View %s%s */%s", string, Term, NEWLINE);
    ib_fflush (sf);
    ISQL_printf (Out, "/* Extract View ");
    ISQL_printf (Out, string);
    ISQL_printf (Out, " */");
    ISQL_printf (Out, NEWLINE);
    EXTRACT_list_view (string);
    }
else
    {
    if (type == EXTRACT_TABLE)
	{
    	ib_fprintf (sf, "/* Extract Table %s%s */%s", string, Term, NEWLINE);
    	ib_fflush (sf);
    	ISQL_printf (Out, "/* Extract Table ");
    	ISQL_printf (Out, string);
        ISQL_printf (Out, " */");
    	ISQL_printf (Out, NEWLINE);
	}
    else
        {
    	ib_fprintf (sf, "/* Extract Database %s%s */%s", Db_name, Term, NEWLINE);
    	ib_fflush (sf);
    	ISQL_printf (Out, "/* Extract Database ");
    	ISQL_printf (Out, Db_name);
        ISQL_printf (Out, " */");
    	ISQL_printf (Out, NEWLINE);
   	}
    ret     	= EXTRACT_ddl (type, string);
    }

if (ret == FINI_OK)
    ret = 0;

/*
 * Commit transaction that was started on behalf of the request 
 * Don't worry about the error if one occurs it will be network related
 * and caught later.
*/
if (gds__trans)
    isc_rollback_transaction (isc_status, &gds__trans);

/* Restore old tranasaction handle */
gds__trans 	= save_trans_handle;

ISQL_printf (Out, NEWLINE);

return (ret);
}
#endif

#ifdef GUI_TOOLS
void ISQL_build_table_list (
    void        **tbl_list,
    IB_FILE         *ipf,
    IB_FILE         *opf,
    IB_FILE         *sf)
{
/**************************************
 *
 *      I S Q L _ b u i l d _ t a b l e _ l i s t
 *
 **************************************
 *
 * Functional description
 *      Build a list of table names within the connected
 *      database.
 *
 **************************************/
isc_tr_handle 	save_trans_handle;

Out 		   = opf;
Ifp 		   = ipf;
Diag 		   = Out;
Help 		   = Out;

/* Transaction for all frontend commands */
if (DB)
    {
    save_trans_handle  = gds__trans;
    gds__trans 	   = NULL;
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
        ISQL_errmsg (isc_status);
    else
	{
        SHOW_build_table_namelist (tbl_list);
	/*
 	 * Commit transaction that was started on behalf of the request 
 	 * Don't worry about the error if one occurs it will be network 
	 * related and caught later.
	 */
    	isc_rollback_transaction (isc_status, &gds__trans);
    	}
    gds__trans = save_trans_handle;
    }
return;
}
#endif
#ifdef GUI_TOOLS
void ISQL_query_database (
    SSHORT	*db_objects,
    IB_FILE        *ipf,
    IB_FILE        *opf,
    IB_FILE        *sf)
{
/**************************************
 *
 *      I S Q L _ q u e r y _ d a t a b a s e 
 *
 **************************************
 *
 * Functional description
 *      Query the database to see if it has any 
 *	 objects defined.
 *
 **************************************/

int		tables, views, indices;
isc_tr_handle 	save_trans_handle;
	
Out 		   = opf;
Ifp 		   = ipf;
Diag 		   = Out;
Help 		   = Out;
tables             = 0;
views              = 0;
indices            = 0;

ISQL_get_version (FALSE);
/*
 * This should only be executed if we are connected to a V4
 * database.  This query will return a list of SQL tables and
 * views that we use to instantiate our extract table/view list
 * box.
*/

if (V4)
    {
    /* Transaction for all frontend commands */
    if (DB)
        {
    	save_trans_handle  = gds__trans;
    	gds__trans         = NULL;
        if (isc_start_transaction (isc_status,
                                   &gds__trans,
                                   1,
                                   &DB,
                                   0,
                                   (SCHAR*) 0))
            ISQL_errmsg (isc_status);
	else
	    {
            /* Call SHOW_query_database to determine if the DB is empty */
    	    SHOW_query_database (&tables, &views, &indices);
	    /*
 	     * Commit transaction that was started on behalf of the request 
 	     * Don't worry about the error if one occurs it will be network 
	     * related and caught later.
	     */
    	    isc_rollback_transaction (isc_status, &gds__trans);
    	    gds__trans = save_trans_handle;
	    }
        }
    }

/* Set up the flags for Windows ISQL */
if (tables)
    *db_objects |= TABLE_OBJECTS;
else
    if (*db_objects & TABLE_OBJECTS)
        *db_objects ^= TABLE_OBJECTS;

if (views)
    *db_objects |= VIEW_OBJECTS;
else
    if (*db_objects & VIEW_OBJECTS)
        *db_objects ^= VIEW_OBJECTS;
return;
}
#endif

#ifdef GUI_TOOLS
void ISQL_build_view_list (
    void        **view_list,
    IB_FILE         *ipf,
    IB_FILE         *opf,
    IB_FILE         *sf)
{
/**************************************
 *
 *      I S Q L _ b u i l d _ v i e w _ l i s t
 *
 **************************************
 *
 * Functional description
 *      Build a list of view names within the connected
 *      database.
 *
 **************************************/
isc_tr_handle 		save_trans_handle;

Out 		   = opf;
Ifp 		   = ipf;
Diag 		   = Out;
Help 		   = Out;

/* Transaction for all frontend commands */
if (DB)
    {
    save_trans_handle  	= gds__trans;
    gds__trans 	   	= NULL;
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
        ISQL_errmsg (isc_status);
    else
	{
    	SHOW_build_view_namelist (view_list);
	/*
 	 * Commit transaction that was started on behalf of the request 
 	 * Don't worry about the error if one occurs it will be network 
	 * related and caught later.
	 */
    	isc_rollback_transaction (isc_status, &gds__trans);
	}
    gds__trans 		= save_trans_handle;
    }
return;
}
#endif
#ifdef GUI_TOOLS
int ISQL_create_database (
    SCHAR        *create_stmt,
    SCHAR	**database,
    SCHAR	 *username,
    SCHAR	 *password,	
    IB_FILE         *ipf,
    IB_FILE         *opf,
    IB_FILE         *sf)
{
/**************************************
 *
 *      I S Q L _ c r e a t e _ d a t a b a s e 
 *
 **************************************
 *
 * Functional description
 *      Set up the global user & password 
 *	 fields.  Call frontend to create a database
 *
 **************************************/

int		ret;

Out 	= opf;
Ifp 	= ipf;
Diag 	= Out;
Help 	= Out;

/* add command to command history file */
ib_fprintf (sf, "%s%s", create_stmt, NEWLINE);
ib_fflush (sf);

/* 
 * Set up the username and password for the frontend create
 * database statement.  Uncomment the following lines if you wish to surface
 * to the user, a create db statement in which the username and password are
 * not required.  They are remembered from the connect statement.
 *
 * strcpy (Password, password);
 * global_psw  = TRUE;
 * strcpy (User, username);
 * global_usr  = TRUE;
 */

ret         = (int) frontend (create_stmt);

/*
 * Copy the database name to the ISQL parameter block field pszDatabase.
 * Check for skip because that signifies that create_db processed the
 * create database statement.
*/
if (ret == SKIP)
    {	
    ret  = 0;
    strcpy (*database, Db_name);
    }		
else
    *database[0] = NULL;

/* 
 * Reset user name and password so that the user has to re-enter valid ones.
 * Uncomment the following lines if you wish to allow the user to issue the
 * create db statement without specifing a username and password.
 * Password[0] = NULL;
 * global_psw  = FALSE;
 * User[0]     = NULL;
 * global_usr  = FALSE;
 */

return (ret);
}
#endif

SSHORT ISQL_get_field_length (
    TEXT	*field_name)
{
/**************************************
 *
 *	I S Q L _ g e t _ f i e l d _ l e n g t h
 *
 **************************************
 *
 *	Retrieve character or field length of V4 character types.
 *
 **************************************/
SSHORT l;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return 0;
	};

if (V33)
    {
    FOR FLD IN RDB$FIELDS WITH
	FLD.RDB$FIELD_NAME EQ field_name

	l = FLD.RDB$FIELD_LENGTH;
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return 0;
	END_ERROR;
    }
else
    {
    FOR FLD IN RDB$FIELDS WITH
	FLD.RDB$FIELD_NAME EQ field_name

	if (FLD.RDB$CHARACTER_LENGTH.NULL)
	    l = FLD.RDB$FIELD_LENGTH;
	else 
	    l = FLD.RDB$CHARACTER_LENGTH;
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return 0;
	END_ERROR;
    }

return l;
}

void ISQL_get_character_sets (
    SSHORT	char_set_id,
    SSHORT	collation,
    USHORT	collate_only,
    TEXT	*string)
{
/**************************************
 *
 *	I S Q L _ g e t _ c h a r a c t e r _ s e t s
 *
 **************************************
 *
 *	Retrieve character set and collation order and format it
 *
 **************************************/
#ifdef	DEV_BUILD
BOOLEAN	found = FALSE;
#endif

string [0] = 0;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return;
	};

if (!V33 && collation)
    {
    FOR FIRST 1 COL IN RDB$COLLATIONS CROSS 
	CST IN RDB$CHARACTER_SETS WITH
	COL.RDB$CHARACTER_SET_ID EQ CST.RDB$CHARACTER_SET_ID AND
	COL.RDB$COLLATION_ID EQ collation AND
	CST.RDB$CHARACTER_SET_ID EQ char_set_id
	SORTED BY COL.RDB$COLLATION_NAME, CST.RDB$CHARACTER_SET_NAME

#ifdef	DEV_BUILD
	found = TRUE;
#endif
	ISQL_blankterm (CST.RDB$CHARACTER_SET_NAME);
	ISQL_blankterm (COL.RDB$COLLATION_NAME);
	ISQL_blankterm (CST.RDB$DEFAULT_COLLATE_NAME);

	/* Is specified collation the default collation for character set? */
	if (strcmp (CST.RDB$DEFAULT_COLLATE_NAME, COL.RDB$COLLATION_NAME) == 0)
	    {
	    if (!collate_only)
		sprintf (string, " CHARACTER SET %s", CST.RDB$CHARACTER_SET_NAME);
	    }
	else if (collate_only)
	    sprintf (string, " COLLATE %s", COL.RDB$COLLATION_NAME);
	else 
	    sprintf (string, " CHARACTER SET %s COLLATE %s",
		CST.RDB$CHARACTER_SET_NAME, COL.RDB$COLLATION_NAME);
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return;
	END_ERROR;
#ifdef DEV_BUILD
    if (!found)
	{
	sprintf (Print_buffer, "ISQL_get_character_set: charset %d collation %d not found.\n", char_set_id, collation);
	STDERROUT (Print_buffer, 1);
	}
#endif
    }
else if (!V33 && char_set_id)
    {
    FOR FIRST 1 CST IN RDB$CHARACTER_SETS WITH
	CST.RDB$CHARACTER_SET_ID EQ char_set_id
	SORTED BY CST.RDB$CHARACTER_SET_NAME

#ifdef DEV_BUILD
	found = TRUE;
#endif
	ISQL_blankterm (CST.RDB$CHARACTER_SET_NAME);

	sprintf (string, " CHARACTER SET %s", CST.RDB$CHARACTER_SET_NAME);
    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return;
	END_ERROR;
#ifdef DEV_BUILD
    if (!found)
	{
	sprintf (Print_buffer, "ISQL_get_character_set: charset %d not found.\n", char_set_id);
	STDERROUT (Print_buffer, 1);
	}
#endif
    }
}

void ISQL_get_default_source (
    TEXT	*rel_name,
    TEXT	*field_name,
    ISC_QUAD	*blob_id)
{
/**************************************
 *
 *	I S Q L _ g e t _ d e f a u l t _ s o u r c e
 *
 **************************************
 *
 *	Retrieve the default source of a field.
 *	Relation_fields takes precedence over fields if both
 *	are present
 *
 *	For a domain, a NULL is passed for rel_name
 **************************************/

ISQL_blankterm (field_name);

*blob_id = isc_blob_null;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return;
	};

if (rel_name)
    {
    /* This is default for a column of a table */
    FOR FLD IN RDB$FIELDS CROSS
	RFR IN RDB$RELATION_FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ rel_name AND
	FLD.RDB$FIELD_NAME EQ field_name 

	if (!RFR.RDB$DEFAULT_SOURCE.NULL)
	    *blob_id = RFR.RDB$DEFAULT_SOURCE;
	else if (!FLD.RDB$DEFAULT_SOURCE.NULL)
	    *blob_id = FLD.RDB$DEFAULT_SOURCE;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return;
	END_ERROR;
    }
else /* default for a domain */
    {
    FOR FLD IN RDB$FIELDS WITH 
	FLD.RDB$FIELD_NAME EQ field_name 

	if (!FLD.RDB$DEFAULT_SOURCE.NULL)
	    *blob_id = FLD.RDB$DEFAULT_SOURCE;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return;
	END_ERROR;
    }
}

SSHORT ISQL_get_index_segments (
    TEXT	*segs,
    TEXT	*indexname,
    BOOLEAN	delimited_yes)
{
/**************************************
 *
 *	I S Q L _ g e t _ i n d e x _ s e g m e n t s
 *
 **************************************
 *
 * Functional description
 *	returns the list of columns in an index.
 *
 **************************************/
SSHORT	n = 0;
TEXT	SQL_identifier[BUFFER_LENGTH128];

*segs = '\0';

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return 0;
	};

/* Query to get column names */

FOR SEG IN RDB$INDEX_SEGMENTS WITH
	SEG.RDB$INDEX_NAME EQ indexname
	SORTED BY SEG.RDB$FIELD_POSITION

    n++;

    /* Place a comma and a blank between each segment column name */

    ISQL_blankterm (SEG.RDB$FIELD_NAME);
    if (db_SQL_dialect > SQL_DIALECT_V6_TRANSITION && delimited_yes)
	{
	ISQL_copy_SQL_id (SEG.RDB$FIELD_NAME, &SQL_identifier, DBL_QUOTE);
	}
    else
	strcpy (SQL_identifier, SEG.RDB$FIELD_NAME);


    if (n == 1)
	sprintf (segs + strlen (segs), "%s", SQL_identifier);
    else
	sprintf (segs + strlen (segs),", %s", SQL_identifier);

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	ROLLBACK;
	return 0;
    END_ERROR;

return (n);
}

BOOLEAN ISQL_get_base_column_null_flag (
    TEXT	*view_name,
    SSHORT	view_context,
    TEXT	*base_field)
{
/**************************************
 *
 *	I S Q L _ g e t _ b a s e _ c o l u m n _ n u l l _ f l a g
 *
 **************************************
 *
 *	Determine if a field on which view column is based
 *	is nullable. We are passed the view_name
 *	view_context and the base_field of the view column.
 *
 **************************************/
BOOLEAN	null_flag;
BOOLEAN done = FALSE;
BOOLEAN error = FALSE;
SSHORT	save_view_context;
BASED_ON RDB$RELATION_FIELDS.RDB$RELATION_NAME save_view_name, save_base_field;

strcpy (save_view_name, view_name);
strcpy (save_base_field, base_field);
save_view_context = view_context;

null_flag = TRUE;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return FALSE;
	};

/* 
   Using view_name and view_context get the relation name from 
   RDB$VIEW_RELATIONS which contains the base_field for this view column. 
   Get row corresponding to this base field and relation from 
   rdb$field_relations. This will contain info on field's nullability unless
   it is a view column itself, in which case repeat this procedure till
   we get to a "real" column. 
*/ 

while (!done && !error)
    {
    ISQL_blankterm (save_view_name);
    ISQL_blankterm (save_base_field);
    FOR FIRST 1 
	  VR IN RDB$VIEW_RELATIONS
    	  CROSS
    	  NEWRFR IN RDB$RELATION_FIELDS
 		WITH
		VR.RDB$VIEW_NAME EQ save_view_name AND
		VR.RDB$VIEW_CONTEXT EQ save_view_context AND
		NEWRFR.RDB$RELATION_NAME = VR.RDB$RELATION_NAME AND
		NEWRFR.RDB$FIELD_NAME = save_base_field

	if (NEWRFR.RDB$BASE_FIELD.NULL)
	    {
	    if (NEWRFR.RDB$NULL_FLAG == 1)
		null_flag = FALSE;
	    done = TRUE;
	    }
    	else
	    {
	    strcpy (save_view_name, NEWRFR.RDB$RELATION_NAME);
	    save_view_context = NEWRFR.RDB$VIEW_CONTEXT;
	    strcpy (save_base_field, NEWRFR.RDB$BASE_FIELD); 
	    }	
    END_FOR
	ON_ERROR
	    error = TRUE;
	END_ERROR;
    }

return null_flag;
}
BOOLEAN ISQL_get_null_flag (
    TEXT	*rel_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	I S Q L _ g e t _ n u l l _ f l a g
 *
 **************************************
 *
 *	Determine if a field has the null flag set.
 *	Look for either rdb$relation_fields or rdb$fields to be 
 *	Set to 1 (NOT NULL), then this field cannot be null
 *	We are passed the relation name and the relation_field name
 *  	For domains, the relation name is null.
 **************************************/
BOOLEAN	null_flag;

ISQL_blankterm (field_name);

null_flag = TRUE;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return FALSE;
	};

if (rel_name)
    {
    FOR FLD IN RDB$FIELDS CROSS
	RFR IN RDB$RELATION_FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ rel_name AND
	RFR.RDB$FIELD_NAME EQ field_name 

	if (FLD.RDB$NULL_FLAG == 1) 
	    null_flag = FALSE;
	else	
	    {

	    /* If RDB$BASE_FIELD is not null then it is a view column */

	    if (RFR.RDB$BASE_FIELD.NULL)
		{

		/* Simple column. Did user define it not null?  */

		if (RFR.RDB$NULL_FLAG == 1)
		    null_flag = FALSE;
		}
	    else
		null_flag = ISQL_get_base_column_null_flag (rel_name, 
				RFR.RDB$VIEW_CONTEXT, RFR.RDB$BASE_FIELD);
	    }

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return null_flag;
	END_ERROR;
    }
else
    {
    /* Domains have only field entries to worry about */
    FOR FLD IN RDB$FIELDS WITH
	FLD.RDB$FIELD_NAME EQ field_name

	if (FLD.RDB$NULL_FLAG == 1)
	    null_flag = FALSE;

    END_FOR
	ON_ERROR
	    ISQL_errmsg (isc_status);
	    return null_flag;
	END_ERROR;
    }

return null_flag;
}

#ifdef GUI_TOOLS
SSHORT ISQL_init (
    IB_FILE	*inputfile,
    IB_FILE	*outputfile)
{
/**************************************
 *
 *	I S Q L _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Open database and initialize input and output files for later
 *	access from Windows ISQL.
 *
 **************************************/
SSHORT	ret;
int	lgth;

/* Initialize all pertinent stuff, start db and start transaction, and
   set up the input and output files */

Out 		= outputfile;
Ifp 		= inputfile;
Diag 		= Out;
Help 		= Out;

Merge_stderr  	= FALSE;
Quiet 		= FALSE;
List 		= FALSE;
Docount 	= FALSE;
Plan 		= FALSE;
Doblob		= 1;
Time_display	= FALSE;
Sqlda_display	= FALSE;
Abort_flag 	= 0;
Echo 		= FALSE;
Interactive 	= FALSE;
Input_file	= FALSE;
Pagelength 	= 20;
Stats 		= FALSE;
Autocommit 	= TRUE;
Autofetch	= TRUE;

/* Reset global user and password so that the user has o resupply them */
Password[0]     = NULL;
global_psw      = FALSE;
User[0]         = NULL;
global_usr      = FALSE;
Role[0]         = NULL;
global_role     = FALSE;
Numbufs[0] = 0;
global_numbufs=FALSE;

lgth = strlen (DEFCHARSET);
if (lgth <= MAXCHARSET_LENGTH) 
    strcpy (ISQL_charset, DEFCHARSET);
else
    strncpy (ISQL_charset, DEFCHARSET, MAXCHARSET_LENGTH);

Termlen 	= strlen (DEFTERM);
if (Termlen <= MAXTERM_LENGTH)
    strcpy (Term, DEFTERM);
else
   {
   Termlen = MAXTERM_LENGTH;
   strncpy (Term, DEFTERM, MAXTERM_LENGTH);
   }

D__trans 	= NULL;
M__trans 	= NULL;
gds__trans 	= NULL;
DB 		= NULL;
Stmt 		= NULL;
signal (SIGINT, query_abort);

sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (20)));
sqlda->version 	= SQLDA_VERSION1;
sqlda->sqln 	= 20;
sqldap 	= &sqlda;
return 0;
}
#endif

#ifdef GUI_TOOLS
void ISQL_reset_settings ()
{
/**************************************
 *
 *	I S Q L _ r e s e t _ s e t t i n g s
 *
 **************************************
 *
 * Functional description
 *	Called after a script is executed in 
 *	Windows ISQL to reset the ISQL session 
 * 	settings.
 *
 **************************************/
int	lgth;

Merge_stderr  	= FALSE;
Quiet 		= FALSE;
List 		= FALSE;
Stats 		= FALSE;
Autocommit     	= TRUE;
Autofetch	= TRUE;
Docount 	= FALSE;
Plan 		= FALSE;
Doblob		= 1;
Time_display	= FALSE;
Sqlda_display	= FALSE;
Echo 		= FALSE;
Pagelength 	= 20;
Abort_flag 	= 0;
Interactive 	= FALSE;
Input_file	= FALSE;

lgth = strlen (DEFCHARSET);
if (lgth <= MAXCHARSET_LENGTH) 
    strcpy (ISQL_charset, DEFCHARSET);
else
    strncpy (ISQL_charset, DEFCHARSET, MAXCHARSET_LENGTH);

Termlen 	= strlen (DEFTERM);
if (Termlen <= MAXTERM_LENGTH)
    strcpy (Term, DEFTERM);
else
   {
   Termlen = MAXTERM_LENGTH;
   strncpy (Term, DEFTERM, MAXTERM_LENGTH);
   }

/* Reset global user and password so that the user has o resupply them */
Password[0]     = NULL;
global_psw      = FALSE;
User[0]         = NULL;
global_usr      = FALSE;
Role[0]         = NULL;
global_role     = FALSE;
Numbufs[0] = 0;
global_numbufs=FALSE;

return;
}
#endif

void  ISQL_disconnect_database (
     int nQuietMode)
	
{
/**************************************
 *
 *	I S Q L _ d i s c o n n e c t _ d a t a b a s e 
 *
 **************************************
 *
 * Functional description
 *	Disconnect from the current database.  First commit work and then 
 *	call isc_detach_database to detach from the database and the zero
 *	out the DB handle.
 *
 **************************************/
extern	SSHORT		Quiet;

/* Ignore error msgs during disconnect */
Quiet = nQuietMode;

/* If we were in a database, commit before proceding */
if (DB && (M__trans || D__trans))
    end_trans();
  
/*
 * Commit transaction that was started on behalf of the request 
 * Don't worry about the error if one occurs it will be network 
 * related and caught later.
 */
if (DB && gds__trans)
    isc_rollback_transaction (isc_status, &gds__trans);

/* If there is  current user statement, free it
   I think option 2 is the right one (DSQL_drop), but who knows */
if (Stmt)
    isc_dsql_free_statement (isc_status, &Stmt, 2);

/* Detach from old database */
isc_detach_database (isc_status, &DB);

/* Enable error msgs */
Quiet = FALSE;

/* Zero database handle and transaction handles */
Stmt	    = NULL;
DB 	    = NULL;
Db_name[0]  = '\0';
D__trans    = NULL;
M__trans    = NULL;
gds__trans  = NULL;

return;
}

BOOLEAN ISQL_is_domain (
    TEXT	*field_name)
{
/**************************************
 *
 *	I S Q L _ i s _ d o m a i n
 *
 **************************************
 *
 *	Determine if a field in rdb$fields is a domain,
 *
 **************************************/
BOOLEAN	is_domain = FALSE;

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return FALSE;
	};

FOR FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME EQ field_name

    if (!(strncmp (FLD.RDB$FIELD_NAME, "RDB$", 4) == 0 &&
	  isdigit (FLD.RDB$FIELD_NAME[4]) &&
	  FLD.RDB$SYSTEM_FLAG != 1))
	is_domain = TRUE;

END_FOR
    ON_ERROR
	ISQL_errmsg (isc_status);
	return is_domain;
    END_ERROR;

return is_domain;
}

void ISQL_make_upper (
    UCHAR	*str)
{
/**************************************
 *
 *	I S Q L _ m a k e _ u p p e r
 *
 **************************************
 *
 *	Force the name of a metadata object to
 *	uppercase.
 *
 **************************************/
UCHAR	*p;

if (!str)
    return;

for (p = str; *p; p++)
    *p = UPPER7 (*p);
}

void ISQL_msg_get (
    USHORT	number,
    TEXT	*msg,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3,
    TEXT	*arg4,
    TEXT	*arg5)
{
/**************************************
 *
 *	I S Q L _ m s g _ g e t
 *
 **************************************
 *
 * Functional description
 *	Retrieve a message from the error file
 *
 **************************************/

gds__msg_format (NULL_PTR, ISQL_MSG_FAC, number, MSG_LENGTH, msg,
		 arg1, arg2, arg3, arg4, arg5);
}

void ISQL_print_validation (
    IB_FILE	*fp,
    ISC_QUAD	*blobid,
    SSHORT	flag,
    SLONG	*trans)
{
/**************************************
 *
 *	I S Q L _ p r i n t _ v a l i d a t i o n
 *
 **************************************
 *
 * Functional description
 *	This does some minor syntax adjustmet for extracting 
 *	validation blobs and computed fields.
 *	if it does not start with the word CHECK
 *	if this is a computed field blob,look for () or insert them.
 *	if flag == 0, this is a validation clause,
 *	if flag == 1, this is a computed field
 *
 **************************************/
SLONG	*blob;
USHORT	length;
TEXT	*buffer, *p;
SSHORT	issql = FALSE, firsttime = TRUE;

/* Don't bother with null blobs */

if (blobid->isc_quad_high == 0)
    return;

buffer = (TEXT *) ISQL_ALLOC (BUFFER_LENGTH512);

blob = NULL;
if (isc_open_blob (isc_status, &DB, &trans, &blob, blobid))
    {
    ISQL_errmsg (isc_status);
    if (buffer)
	ISQL_FREE (buffer);
    return;
    }

while (!isc_get_segment (isc_status, &blob, &length,
	(USHORT) (BUFFER_LENGTH512 - 1),
	buffer) || isc_status [1] == isc_segment)
    {
    buffer [length] = 0;
    p = buffer;
    if (flag)
	{
	/* computed field SQL syntax correction */

	while (isspace (*p))
		p++;
	if (*p == '(')
	    issql = TRUE;
	}
    else  
	{
	/* Validation SQL syntax correction */

	while (isspace (*p))
		p++;
	if (!strncmp (p, "check", 5) || !strncmp (p, "CHECK", 5))
	    issql = TRUE;
	}
    if (firsttime)
	{
	firsttime = FALSE;
	if (!issql)
	    {
	    sprintf (Print_buffer, "%s ", (flag ? "/* " : "("));
	    ISQL_printf (fp, Print_buffer);
	    }
	}
	
    ISQL_printf (fp, buffer);
    }
    if (!issql)
	{
	sprintf (Print_buffer, "%s", (flag ? " */" : ")"));
	ISQL_printf (fp, Print_buffer);
	}

isc_close_blob (isc_status, &blob);

if (isc_status [1])
    ISQL_errmsg (isc_status);

if (buffer)
    ISQL_FREE (buffer);
}

void ISQL_printf(
	IB_FILE	*fp, 
	TEXT	*buffer)
{
/**************************************
 *
 *	I S Q L _ p r i n t f
 *
 **************************************
 *
 *	Centralized printing facility
 *
 **************************************/
#ifndef GUI_TOOLS

ib_fprintf (fp, "%s", buffer);

#else

SCHAR           *pszStartPos, *psChar, cSavChar;

/* Check buffer to make sure we have data */
if (!*buffer)
    return;

/* Windows ISQL output routine */
psChar          = buffer;
pszStartPos     = buffer;

/* If first char is newline then print PC newline */
if (*psChar == '\n')
    {
    PaintOutputWindow (NEWLINE);
    ib_fprintf (fp, "%s", "\n");
    pszStartPos = buffer + 1;
    }
psChar++;

/* Step through output buffer displaying output when a newline is reached */
for (; *psChar != NULL; psChar++)
    {
   if (*psChar == '\n' && *(psChar - 1) != '\r')
       {
       cSavChar = *psChar;
       *psChar  = NULL;

       /* Don't print the Null */
       if (*pszStartPos)
           {
           PaintOutputWindow (pszStartPos);
           ib_fprintf (fp, "%s", pszStartPos);
    	   }
       PaintOutputWindow (NEWLINE);
       ib_fprintf (fp, "%s", "\n");
       *psChar  = cSavChar;
       pszStartPos  = psChar + 1;
       }
    }

/*
 * Display the remainder if newlines whre encountered, otherwise display the
 * whole buffer
*/
if (*pszStartPos)
    {
    PaintOutputWindow (pszStartPos);
    if (!strcmp(pszStartPos, NEWLINE))
        ib_fprintf (fp, "%s", "\n");
    else
        ib_fprintf (fp, "%s", pszStartPos);
    }
#endif
}

#ifdef GUI_TOOLS
int ISQL_sql_statement (
    TEXT	*string,
    IB_FILE	*ipf,
    IB_FILE	*opf,
    IB_FILE	*sf)
{
/**************************************
 *
 *	I S Q L _ s q l _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Process a SQL statement for Windows ISQL.   This sets up
 *	the input and output files, and executes the statement.
 *
 **************************************/
int	ret;

Out = opf;
Ifp = ipf;
Diag = Out;
Help = Out;

/* add command to command history file */
ib_fprintf (sf, "%s%s", string, NEWLINE);
ib_fflush (sf);

ISQL_printf (Out, string);
ISQL_printf (Out, NEWLINE);

ret  = process_statement (string, sqldap);
if (ret == SKIP)
    ret = 0;

ISQL_printf (Out, NEWLINE);

return (ret);
}
#endif

#ifdef	DEV_BUILD
static SSHORT add_row (
    TEXT	*tabname)
{
/**************************************
 *
 *	a d d _ r o w
 *
 **************************************
 *
 * Functional description
 *	Allows interactive insert of row, prompting for every column
 *
 *	The technique is to describe a select query of select * from the table.
 *	Then generate an insert with ? in every position, using the same sqlda.
 *	It will allow edits of blobs, skip arrays and computed fields
 *
 *	Parameters:
 *	tabname -- Name of table to insert into
 *
 **************************************/
SSHORT		n_cols, length, type, nullable, i, j, i_cols, done;
SCHAR		string [BUFFER_LENGTH120], name [WORDLENGTH], buffer [BUFFER_LENGTH512], infobuf [BUFFER_LENGTH180];
SCHAR		*insertstring = NULL, *stringvalue = NULL, cmd[5];
SSHORT		*nullind = NULL, *nullp; 
/* Data types */
SSHORT		*smallint, dscale;
SSHORT		*colnumber;
SLONG		*integer;
SINT64		*pi64, n;
float		*fvalue;
double		*dvalue, numeric;
SCHAR		bfile[120];
struct tm	times;
XSQLDA		*sqlda = NULL, *isqlda;
XSQLVAR		*var, *ivar;
SCHAR		*p;
ISC_QUAD	*blobid;
VARY		*vary;
TEXT		msg [MSG_LENGTH];
SLONG		res;

if (!*tabname)
    return (ERR);

if (!Interactive)
    return (ERR);

/* Initialize the time structure */
for (p = (SCHAR *) &times, i = sizeof (times); i; p++, i--)
    *p = 0;

/* There may have been a commit transaction before we got here */	

if (!M__trans)
    if (isc_start_transaction (isc_status, &M__trans, 1, &DB, 0, (SCHAR*) 0))
	{
        ISQL_errmsg (isc_status);
	return FAIL;
	};

/* Start default transaction for prepare */
if (!D__trans)
    if (isc_start_transaction (isc_status, &D__trans, 1, &DB, 0, (SCHAR*) 0))
	{
        ISQL_errmsg (isc_status);
	return FAIL;
	};

/* Query to obtain relation information */

sprintf (string, "SELECT * FROM %s", tabname);

sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (20)));
sqlda->version = SQLDA_VERSION1;
sqlda->sqln = 20;
if (isc_dsql_prepare (isc_status, &D__trans, &Stmt, 0, string, 
		      SQL_dialect, sqlda))

    {
    ISQL_errmsg (isc_status);
    return (SKIP);
    }

n_cols = sqlda->sqld;

/* Reallocate to the proper size if necessary */

if (sqlda->sqld > sqlda->sqln)
    {
    ISQL_FREE (sqlda);
    sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (n_cols)));
    sqlda->sqld = sqlda->sqln = n_cols;

    /*  We must re-describe the sqlda, no need to re-prepare */

    if (isc_dsql_describe (isc_status, &Stmt, SQL_dialect, sqlda))
	{
        ISQL_errmsg (isc_status);
	if (sqlda)
	    ISQL_FREE (sqlda);
        return (FAIL);
	};
    }

/* Array for storing select/insert column mapping , colcheck sets it up*/
colnumber = (SSHORT*) ISQL_ALLOC ((SLONG) (n_cols * sizeof (SSHORT)));
col_check (tabname, colnumber);

/* Create the new INSERT statement from the sqlda info */

insertstring = (SCHAR*) ISQL_ALLOC ((SLONG) (40 + 36 * n_cols));

/* There is a question marker for each column known updatable column */

sprintf (insertstring, "INSERT INTO %s (", tabname);
    
for (i = 0, i_cols = 0, var = sqlda->sqlvar; i < n_cols; var++, i++)
    {
    /* Skip columns that are computed */
    if (colnumber [i] != -1)
	{
	sprintf (insertstring + strlen (insertstring), "%s,", var->sqlname);
	i_cols++;
	}
    }
/* Set i_cols to the number of insert columns  */
insertstring [strlen (insertstring) - 1] = ')';
strcat(insertstring, " VALUES (");

for (i = 0; i < n_cols; i++)
    {
    if (colnumber [i] != -1)
	strcat (insertstring, " ?,");
    }
    /* Patch the last character in the string */
insertstring [strlen (insertstring) - 1] = ')';
	

/* Allocate INSERT sqlda and  null indicators */

isqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (i_cols)));

/* ISQL_ALLOC doesn't initialize the structure, so init everything
 * to avoid lots of problems.
 */
for (p = (SCHAR *) isqlda, i = XSQLDA_LENGTH (i_cols); i; p++, i--)
    *p = 0;
isqlda->version = SQLDA_VERSION1;
isqlda->sqln = isqlda->sqld = i_cols;

nullind = (SSHORT*) ISQL_ALLOC ((SLONG) (i_cols * sizeof (SSHORT)));

/*
**  For each column, get the type, and length then prompt for a value
**  and ib_scanf the resulting string into a buffer of the right type.
*/

gds__msg_format (NULL_PTR, ISQL_MSG_FAC, ADD_PROMPT, sizeof (infobuf), infobuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
STDERROUT (infobuf, 1);

done = FALSE;
while (!done)
    {
    nullp = nullind;
    for (i = 0, var = sqlda->sqlvar; i < n_cols && !done; var++, i++)
	{
	if (colnumber [i] == -1)
	    continue;

	/* SQLDA for INSERT statement might have fewer columns */
	j = colnumber[i];
	ivar = &isqlda->sqlvar[j];
	*ivar = *var;
	length = var->sqllen;
	*nullp = 0;
	ivar->sqlind = nullp++;
	ivar->sqldata = NULL;
	strcpy (name, var->sqlname);
	name[var->sqlname_length] = '\0';
	type = var->sqltype & ~1;
	nullable = var->sqltype & 1;

	/* Prompt with the name and read the line */ 

	if (type == SQL_BLOB)
	    {
	    ISQL_msg_get (BLOB_PROMPT, msg, name, NULL, NULL, NULL, NULL);
	    ISQL_prompt (msg);   /* Blob: %s, type 'edit' or filename to load> */
	    }
	else if (type == SQL_TIMESTAMP || type == SQL_TYPE_DATE)
	    {
	    ISQL_msg_get (DATE_PROMPT, msg, name, NULL, NULL, NULL, NULL);
	    ISQL_prompt (msg);   	/* Enter %s as M/D/Y> */
	    }
	else
	    {
	    ISQL_msg_get (NAME_PROMPT, msg, name, NULL, NULL, NULL, NULL);
	    ISQL_prompt (msg);    	/* Enter %s> */
	    }

	/* On blank line or EOF, break the loop without doing an insert */

	if (!ib_fgets (buffer,BUFFER_LENGTH512,ib_stdin) || !strlen (buffer))
	    {
	    done = TRUE;
	    break;
	    }

	length = strlen (buffer);
	STDERROUT ("", 1);


	/* Convert first  4 chars to upper case for comparison */
	strncpy (cmd, buffer, 4);
	cmd[4] = '\0';
	ISQL_make_upper (cmd);

	/* If the user writes NULL, put a null in the column */

	if (!strcmp (cmd, "NULL"))
	    *ivar->sqlind = -1;
	else
	    switch (type)
		{
		case SQL_BLOB:

		    blobid = (ISC_QUAD*) ISQL_ALLOC ((SLONG) sizeof (ISC_QUAD));

		    if (!strcmp (cmd, "EDIT"))
		        /* If edit, we edit a temp file */
			{
			tmpnam (bfile);
			gds__edit (bfile, 0);
		        res = BLOB_text_load (blobid, DB, M__trans, bfile);
			unlink (bfile);
			}
		    else
			/* Otherwise just load the named file */
			/* We can't be sure if it's a TEXT or BINARY file
			 * being loaded.  As we aren't sure, we'll
			 * do binary operation - this is NEW data in
			 * the database, not updating existing info
			 */
			{
			strcpy (bfile, buffer);
		        res = BLOB_load (blobid, DB, M__trans, bfile);
			}

		    if (!res)
			{
			STDERROUT ("Unable to load file", 1);
			done = TRUE;
			}
			
		    ivar->sqldata = (SCHAR*) blobid;	
		    break;

		case SQL_FLOAT:
		    fvalue = (float*) ISQL_ALLOC (sizeof (float));
		    if (sscanf (buffer, "%g", fvalue) != 1)
			{
			STDERROUT ("Input parsing problem", 1);
			done = TRUE;
			}
		    ivar->sqldata = (SCHAR*) fvalue;
		    break;

		case SQL_DOUBLE:
		    dvalue = (double*) ISQL_ALLOC (sizeof (double));
		    if (sscanf (buffer, "%lg", dvalue) != 1)
			{
			STDERROUT ("Input parsing problem", 1);
			done = TRUE;
			}
		    ivar->sqldata = (SCHAR*) dvalue;
		    break;

		case SQL_TYPE_TIME:
		    if (3 != sscanf (buffer, "%d:%d:%d", &times.tm_hour, 
			&times.tm_min, &times.tm_sec))
			{
			ISQL_msg_get (TIME_ERR, msg, buffer, NULL, NULL, NULL, NULL);
			STDERROUT (msg, 1);	/* Bad date %s\n */
			done = TRUE;
			}
		    else
			{
			ULONG	date [2];
			stringvalue = (SCHAR*) ISQL_ALLOC ((SLONG) (ivar->sqllen + 1));
			isc_encode_date (&times, date);
			ivar->sqldata = stringvalue;
			*(ULONG *)stringvalue = date [1];
			};
		    break;

		case SQL_TIMESTAMP:
		case SQL_TYPE_DATE:
		    if (3 != sscanf (buffer, "%d/%d/%d", &times.tm_mon, 
			&times.tm_mday, &times.tm_year))
			{
			ISQL_msg_get (DATE_ERR, msg, buffer, NULL, NULL, NULL, NULL);
			STDERROUT (msg, 1);	/* Bad date %s\n */
			done = TRUE;
			}
		    else
			{
			ULONG	date [2];
			times.tm_mon--;
			stringvalue = (SCHAR*) ISQL_ALLOC ((SLONG) (ivar->sqllen + 1));
			isc_encode_date (&times, date);
			ivar->sqldata = stringvalue;
			((ULONG *)stringvalue) [0] = date [0];
			if (type == SQL_TIMESTAMP)
			    ((ULONG *)stringvalue) [1] = date [1];
			}
		    break;

		case SQL_TEXT:
		   /* Force all text to varying */
		    ivar->sqltype = SQL_VARYING + nullable;

		case SQL_VARYING:

		    vary = (VARY*) ISQL_ALLOC ((SLONG) (length + sizeof (USHORT)));
		    vary->vary_length = length;
		    strncpy (vary->vary_string, buffer, length);
		    ivar->sqldata = (SCHAR*) vary;
		    ivar->sqllen = length + sizeof (USHORT);
		    break;

		case SQL_SHORT:
		case SQL_LONG:
		    if (type == SQL_SHORT)
			{
		        smallint = (SSHORT*) ISQL_ALLOC (sizeof (SSHORT));
		        ivar->sqldata = (SCHAR*) smallint;
			}
		    else if (type == SQL_LONG)
			{
		        integer = (SLONG*) ISQL_ALLOC (sizeof (SLONG));
		        ivar->sqldata = (SCHAR*) integer;
			}

		/* Fall through from SQL_SHORT and SQL_LONG ... */
		case SQL_INT64:
		    if (type == SQL_INT64)
			{
			pi64 = (SINT64*) ISQL_ALLOC (sizeof (SINT64));
			ivar->sqldata = (SCHAR*) pi64;
			}
		    if ((dscale = var->sqlscale) < 0)
			{
		        SSHORT lscale=0;
			if (get_numeric (buffer, length, &lscale, &n) != TRUE)
			    {
			    STDERROUT ("Input parsing problem", 1);
			    done = TRUE;
			    }
			else
			    {
			    dscale = var->sqlscale - lscale;
			    if (dscale > 0)
				{
				TEXT *err_buf[256];
				sprintf (err_buf, 
                                         "input error: input scale %d exceeds the field's scale %d",
                                         -lscale, -var->sqlscale);
				STDERROUT (err_buf, 1);
				done = TRUE;
				}
			    while (dscale++ < 0)
				n *= 10;
			    }
			}
			/* sscanf assumes an 64-bit integer target */
		    else if (sscanf(buffer, "%" QUADFORMAT "d", &n) != 1)
			{
			STDERROUT ("Input parsing problem", 1);
			done = TRUE;
			}
		    if (type == SQL_INT64)
			*pi64 = n;
		    else if (type == SQL_SHORT)
		        *smallint = n;
		    else if (type == SQL_LONG)
			*integer = n;
		    break;

		default:
		    done = TRUE;
		    STDERROUT ("Unexpected SQLTYPE in add_row()", 1);
		    break;
		}
	}
    if (!done)
	{
	/* having completed all columns, try the insert statement with the sqlda */

	if (isc_dsql_exec_immed2 (isc_status, &DB, &M__trans, 0, insertstring,
				  SQL_dialect, isqlda, (XSQLDA*) 0))
	    {
	    ISQL_errmsg (isc_status);
	    break;
	    }

	/* Release each of the variables for next time */

	for (ivar = isqlda->sqlvar, i = 0; i < i_cols; i++, ivar++)
	    if (ivar->sqldata)
		ISQL_FREE (ivar->sqldata);
	}
   }

ISQL_FREE (insertstring);
for (ivar = isqlda->sqlvar, i = 0; i < i_cols; i++, ivar++)
    if (ivar->sqldata)
	ISQL_FREE (ivar->sqldata);
if (sqlda)
    ISQL_FREE (sqlda);
if (isqlda)
    ISQL_FREE (isqlda);
if (colnumber)
    ISQL_FREE (colnumber);
if (nullind)
    ISQL_FREE (nullind);

return (SKIP);
}
#endif

static SSHORT blobedit (
    TEXT	*action,
    TEXT	**cmd)
{
/**************************************
 *
 *	b l o b e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit the text blob indicated by blobid
 *
 *	Parameters:  cmd -- Array of words interpreted as file name
 *
 **************************************/
ISC_QUAD	blobid;
TEXT		*p;

if (!ISQL_dbcheck())
    return FAIL;

if (*cmd [1] == 0)
    return ERR;

p = cmd [1];

/* Find the high and low values of the blob id */

sscanf (p, "%x:%x", &blobid.isc_quad_high, &blobid.isc_quad_low);

/* If it isn't an explicit blobedit, then do a dump. Since this is a 
   user operation, put it on the M__trans handle */

if (!strcmp (action, "BLOBVIEW"))
    (void) BLOB_edit (&blobid, DB, M__trans, "blob");
else if ((!strcmp (action, "BLOBDUMP")) && (*cmd[2]))
    {
    /* If this is a blobdump, make sure there is a file name */
    /* We can't be sure if the BLOB is TEXT or BINARY data,
     * as we're not sure, we'll dump it in BINARY mode
     */
    (void) BLOB_dump (&blobid, DB, M__trans, cmd[2]);
    }
else 
    return ERR;

return (SKIP);
}   

static void col_check (
    TEXT	*tabname,
    SSHORT	*colnumber)
{
/**************************************
 *
 *	c o l _ c h e c k
 *
 **************************************
 *
 *	Check for peculiarities not currently revealed by the SQLDA
 *	colnumber array records the mapping of select columns to insert
 *	columns which do not have an equivalent for array or computed cols.
 **************************************/
SSHORT	i = 0, j = 0;

/* Query to get array info  and computed source not available in the sqlda */

ISQL_make_upper(tabname);

/* Transaction for all frontend commands */
if (DB && !gds__trans)
    if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	{
	ISQL_errmsg (isc_status);
	return;
	};

FOR F IN RDB$FIELDS CROSS 
    R IN RDB$RELATION_FIELDS WITH
    F.RDB$FIELD_NAME = R.RDB$FIELD_SOURCE AND
    R.RDB$RELATION_NAME EQ tabname
    SORTED BY R.RDB$FIELD_POSITION, R.RDB$FIELD_NAME

    if ((!F.RDB$DIMENSIONS.NULL && F.RDB$DIMENSIONS) || (!F.RDB$COMPUTED_BLR.NULL))
	colnumber [i] = -1;
    else
	colnumber [i] = j++;
    i++;
END_FOR
ON_ERROR
    ISQL_errmsg (isc_status);
END_ERROR;
}

static void copy_str (
    TEXT	**output_str,
    TEXT	**input_str,
    BOOLEAN	*done,
    TEXT	*str_begin,
    TEXT	*str_end,
    SSHORT	str_flag)
{
/**************************************
 *
 *	c o p y _ s t r
 *
 **************************************
 *
 * Functional description
 *
 *	Copy and/or modify the data in the input buffer and stores it
 *	into output buffer.
 *
 *	For SINGLE_QUOTED_STRING, copy everything from the begining of the
 *		the input buffer to the end of the string constant
 *
 *	For DOUBLE_QUOTED_STRING, copy everything from the begining of the
 *		the input buffer to the begining of the string constant
 *
 *		a. replace double quote string delimted charcter with single
 *			quote
 *		b. if '""' is found with in the string constant, removes one 
 *			extra double quote
 *		c. if a single quote is found with in the string constant, 
 *			adds one more single quote
 *		d. replace ending double quote string delimted charcter with 
 *			single quote
 *
 *	For NO_MORE_STRING, copy everything from the begining of the
 *		the input buffer to the end of input buffer
 *
 *	Input Arguments:
 *
 *		1. address of the output buffer
 *		2. address of the input buffer
 *		3. address of the end of processing flag
 *
 *	Output Arguments:
 *
 *		1. pointer to begining of the string constant
 *		2. pointer to ending   of the string constant
 *		3. string type
 *
 **************************************/
SSHORT	slen;
TEXT	*temp_str, *temp_local_stmt_str, *b1;

temp_str = *output_str;
temp_local_stmt_str = *input_str;

switch (str_flag)
    {
    case SINGLE_QUOTED_STRING:
	slen = str_end - temp_local_stmt_str;
	strncpy (temp_str, temp_local_stmt_str, slen);
	*output_str = temp_str + slen;
	*input_str = str_end;
    break;
    case DOUBLE_QUOTED_STRING:
	slen = str_begin - temp_local_stmt_str;
	strncpy (temp_str, temp_local_stmt_str, slen);
	temp_str += slen;
	*temp_str = SINGLE_QUOTE;
	temp_str++;
	b1 = str_begin + 1;
	while (b1 <= str_end)
	    {
	    *temp_str = *b1;
	    temp_str++;
	    switch (*b1)
		{
		case SINGLE_QUOTE:
		    *temp_str = SINGLE_QUOTE;
		    temp_str++;
		break;
		case DBL_QUOTE:
		    b1++;
		    if (*b1 != DBL_QUOTE)
			{
			temp_str--;
			*temp_str = SINGLE_QUOTE;
			temp_str++;
			b1--;
			}
		break;
		default:
		break;
		}
	    b1++;
	    }
	*output_str = temp_str;
	*input_str = b1;
    break;
    case NO_MORE_STRING:
	slen = strlen (temp_local_stmt_str);
	strncpy (temp_str, temp_local_stmt_str, slen);
	temp_str += slen;
	*temp_str = '\0';
	*done = TRUE;
	*output_str = temp_str;
	*input_str = temp_local_stmt_str + slen;
    break;
    default:
    break;
    }
}

#ifndef GUI_TOOLS
#ifdef	DEV_BUILD
static SSHORT copy_table (
    TEXT	*source,
    TEXT	*destination,
    TEXT	*otherdb)
{
/**************************************
 *
 *	c o p y _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Create a new table based on an existing one.
 *  
 *	Parameters:  source -- name of source table
 *		     destination == name of newly created table
 *  
 **************************************/
IB_FILE 	*holdout;
TEXT	*p, ftmp [128];
TEXT	errbuf [MSG_LENGTH];
TEXT	cmd [512];
TEXT	*altdb;
SSHORT	domain_flag = FALSE;

/* Call list_table with a temporary file, then hand that file to a
   new version of isql */

holdout = Out;

/* If there is an alternate database, extract the domains */

if (*otherdb)
    domain_flag = TRUE;

#ifndef __BORLANDC__
Out = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, ftmp);
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the ISQL
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (ftmp, p);
free (p);
Out = ib_fopen (ftmp, "w+");
if (!Out)
    Out = (IB_FILE*) -1;
#endif
if (Out == (IB_FILE*) -1)
    {
    /* If we can't open a temp file then bail */

    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, FILE_OPEN_ERR, sizeof (errbuf), 
	errbuf, ftmp, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    Exit_value = FINI_ERROR;
    return END;
    }

ISQL_make_upper (source);
if (EXTRACT_list_table (source, destination, domain_flag))
    {
    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, NOT_FOUND, sizeof (errbuf), 
	errbuf, source, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    (void) ib_fclose (Out);
    }
else
    {
    (void) ib_fclose (Out);

    /* easy to make a copy in another database */

    if (*otherdb)
	altdb = otherdb;
    else
	altdb = Db_name;
    sprintf (cmd, "isql -q %s -i %s", altdb, ftmp);
    if (system (cmd))
	{
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, COPY_ERR, sizeof (errbuf), 
	    errbuf, destination, altdb, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (errbuf, 1);
	}
    }

(void) unlink (ftmp);
Out = holdout;

return (SKIP);
}
#endif	/* DEV_BUILD */
#endif   /* GUI_TOOLS */

static SSHORT create_db (
    TEXT 	*statement,
    TEXT	*d_name)
{
/**************************************
 *
 *	c r e a t e _ d b
 *
 **************************************
 *
 * Functional description
 *	Intercept create database commands to
 *	adjust the DB and transaction handles
 *  
 *	Parameters:  statement == the entire statement for processing.
 *
 * Note: SQL ROLE settings are ignored - the newly created database
 *	will not have any roles defined in it.
 *  
 **************************************/
TEXT	*local_statement, *new_local_statement, *temp_str, *k, *l, *q, *p;
TEXT	*b1 = NULL, *b2 = NULL, *temp_local_stmt_str = NULL;
TEXT	*str_begin = NULL, *str_end = NULL;
SSHORT	arglength;
SSHORT	slen, ret, cnt = 0;
TEXT	*usr, *psw, quote = DBL_QUOTE;
BOOLEAN	fl_nm_del_by_dq = FALSE;     /* file name delimited by double quote */
BOOLEAN	done = FALSE, done1 = FALSE; 
SSHORT	str_flag;
TEXT	errbuf [MSG_LENGTH];

new_local_statement = k = l = q = p = NULL;
ret = SKIP;

/* Disconnect from the database and cleanup */
ISQL_disconnect_database (FALSE);

arglength = strlen (statement) + strlen (User) + strlen (Password) + 24;
if (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET))
    arglength += strlen (ISQL_charset) + strlen (" SET NAMES \'\' ");
local_statement = (TEXT*) ISQL_ALLOC ((SLONG) (arglength + 1));
if (!local_statement)
    return (FAIL);
usr 		= (TEXT*) ISQL_ALLOC (USER_LENGTH);
if (!usr)
    {
    ISQL_FREE (local_statement);
    return (FAIL);
    }
psw		= (TEXT*) ISQL_ALLOC (PASSWORD_LENGTH);
if (!psw)
    {
    ISQL_FREE (local_statement);
    ISQL_FREE (usr);
    return (FAIL);
    }

strcpy (local_statement, statement);

/* If there is a user parameter, we will set it into the create stmt. */
#if defined (WIN95) && !defined(GUI_TOOLS)
if (!fAnsiCP || global_usr || global_psw || 
#else
if (global_usr || global_psw || 
#endif
    (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET)))
    {
    strip_quotes (User, usr);
    strip_quotes (Password, psw);
    /*
     *  Look for the quotes on the database name and find the close quotes.
     * Use '"' first, if not successful try '''.
     */
    q 		 = strchr (statement, quote);
    if (!q)
         {
         quote   = SINGLE_QUOTE;
         q       = strchr (statement, quote);
         }

    if (q)
        {
        /* Set quote to match open quote */
        quote    = *q;
        q++;
        p        = strchr (q, quote);
	if (p)
	    {
#if defined (WIN95) && !defined (GUI_TOOLS)
	    /* If we are not using the ANSI codepage then we need to
	     * translate the database name to ANSI from OEM.
	     * We will translate (p - q) characters from (q) into
	     * the proper offset in local_statement.
	     */
	    if (!fAnsiCP)
		OemToAnsiBuff(q, local_statement + (q - statement), p - q);
#endif
	    p++;
	    slen = (SSHORT) (p - statement);
	    local_statement[slen] = '\0';
	    if (SQL_dialect == 1)
		{
		if (global_usr)
		    sprintf (local_statement + strlen (local_statement), 
			" USER \'%s\' ", usr);
		if (global_psw)
		    sprintf (local_statement + strlen (local_statement),
			" PASSWORD \'%s\' ",  psw);
		if (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET))
		    sprintf (local_statement + strlen (local_statement),
			" SET NAMES \'%s\' ",  ISQL_charset);
		sprintf (local_statement + strlen (local_statement), "%s", p);
		}
	    }
	}
    }

if ((SQL_dialect == 0) || (SQL_dialect > 1))
    {
    q = strchr (statement, SINGLE_QUOTE);
    while (q)
	{
	cnt++;
	l = q + 1;
	q = strchr (l, SINGLE_QUOTE);
	}

    if (cnt > 0)
	{
	arglength = strlen (statement) + 
		    strlen (User) + 
		    strlen (Password) + 
		    24 + 
		    2*cnt;

	if (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET))
	    arglength += strlen (ISQL_charset) + strlen (" SET NAMES \'\' ");
	new_local_statement = (TEXT*) ISQL_ALLOC ((SLONG) (arglength + 1));

	if (!new_local_statement)
	    return (FAIL);

	}

    if (new_local_statement)
	temp_str = new_local_statement;
    else
	temp_str = local_statement;

    temp_local_stmt_str = local_statement;
    while (!done)
	{
	str_begin = str_end = NULL;
	str_flag = INIT_STR_FLAG;
	get_str (temp_local_stmt_str, &str_begin, &str_end, &str_flag);
	if (str_flag == INCOMPLETE_STRING)
	{
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, INCOMPLETE_STR, 
			    sizeof (errbuf), errbuf, 
			    "create database statement", 
			    NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    STDERROUT (errbuf, 1);
	    return(FAIL);
	}
	copy_str (&temp_str, &temp_local_stmt_str, &done,
		  str_begin, str_end, str_flag);
	}

    if (new_local_statement)
	temp_str = new_local_statement;
    else
	temp_str = local_statement;

    if (global_usr)
	sprintf (temp_str + strlen (temp_str), 
	    " USER \'%s\' ", usr);

    if (global_psw)
	sprintf (temp_str + strlen (temp_str),
	    " PASSWORD \'%s\' ",  psw);

    if (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET))
	sprintf (temp_str + strlen (temp_str),
	    " SET NAMES \'%s\' ",  ISQL_charset);

    if (new_local_statement)
	temp_str = new_local_statement + strlen (new_local_statement);
    else
	temp_str = local_statement + strlen (local_statement);

    if (p && strlen (p) > 0)
	{
	temp_local_stmt_str = p;
	done = FALSE;
	while (!done)
	    {
	    str_begin = str_end = NULL;
	    str_flag = INIT_STR_FLAG;
	    get_str (temp_local_stmt_str, &str_begin, &str_end, &str_flag);
		if (str_flag == INCOMPLETE_STRING)
		{
		    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, INCOMPLETE_STR, 
				    sizeof (errbuf), errbuf, 
				    "create database statement", 
				    NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
		    STDERROUT (errbuf, 1);
		    return(FAIL);
		}
	    copy_str (&temp_str, &temp_local_stmt_str, &done,
		      str_begin, str_end, str_flag);
	    }
	}

    if (new_local_statement)
	local_statement = new_local_statement;
    }

    /* execute the create statement 
     * If the SQL_dialect is not set or set to 2, create the database
     * as a dialect 3 database.
     */
    if (SQL_dialect == 0 || SQL_dialect == SQL_DIALECT_V6_TRANSITION)
    {
    if (isc_dsql_execute_immediate (isc_status, &DB, &M__trans, 0, 
				    local_statement, requested_SQL_dialect, 
				    NULL))
	{
	ISQL_errmsg (isc_status);
	if (!DB)
	    ret = FAIL;
	}
    }
else
    {
    if (isc_dsql_execute_immediate (isc_status, &DB, &M__trans, 0, 
				    local_statement, SQL_dialect, NULL))
	{
	ISQL_errmsg (isc_status);
	if (!DB)
	    ret = FAIL;
	}
    }

if (DB)
    {
    /* Load Db_name with some value to show a successful attach */

    strip_quotes (d_name, Db_name);

    ISQL_get_version (TRUE);

    /* Start the user transaction */

    if (!M__trans)
    	{
    	if (isc_start_transaction (isc_status, &M__trans, 1, &DB, 0, (SCHAR*) 0))
	    ISQL_errmsg (isc_status);
    	if (D__trans)
	    COMMIT_TRANS (&D__trans);
        if (isc_start_transaction (isc_status, &D__trans, 1, &DB, 
		(V4) ? 5           : 0, 
		(V4) ? default_tpb : NULL))
	    ISQL_errmsg (isc_status);
        }

    /* Allocate a new user statement for ths database */

    if (isc_dsql_allocate_statement(isc_status, &DB, &Stmt))
    	{
    	ISQL_errmsg (isc_status);
    	ret = ERR;
    	}
    }

if (local_statement)
    ISQL_FREE (local_statement);
if (psw)
    ISQL_FREE (psw);
if (usr)
    ISQL_FREE (usr);
return(ret);
}

static void do_isql (void)
{
/**************************************
 *
 *	d o _ i s q l 
 *
 **************************************
 *
 * Functional description
 *	Process incoming SQL statements
 *
 **************************************/
TEXT	*p;
TEXT	*statement = NULL;
TEXT	*errbuf;
USHORT	bufsize = 0;
SSHORT	ret;
BOOLEAN	done = FALSE;

errbuf = (TEXT*) ISQL_ALLOC ((SLONG) MSG_LENGTH);
if (!errbuf)
    return;

/* Initialized user transactions */

M__trans = NULL;

/* File used to edit sessions */

#ifndef __BORLANDC__
Ofp = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, Tmpfile);
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the ISQL
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (Tmpfile, p);
free (p);
Ofp = ib_fopen (Tmpfile, "w+");
if (!Ofp)
    Ofp = (IB_FILE*) -1;
#endif
if (Ofp == (IB_FILE*) -1)
    {
    /* If we can't open a temp file then bail */

    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, FILE_OPEN_ERR, MSG_LENGTH,
	errbuf, Tmpfile, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    Exit_value = FINI_ERROR;
    exit (Exit_value);
    }

/* Open database and start tansaction */

signal (SIGINT, query_abort);

/* 
 * We will not excute this for now on WINDOWS. We are not prompting for 
 * a database name, username password.  A connect statement has to be in
 * the file containing the script.
 */

#ifndef GUI_TOOLS
(void) newdb (Db_name, User, Password, Numbufs, Role);

/* If that failed or no Dbname was specified */

(void) ISQL_dbcheck();
#endif
	
/* Set up SQLDA for SELECTs, allow up to 20 fields to be selected.
**  If we subsequently encounter a query with more columns, we
**  will realloc the sqlda as needed
*/

sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (20)));
sqlda->version = SQLDA_VERSION1;
sqlda->sqln = 20;

/* The sqldap is the handle containing the current sqlda pointer */

sqldap = &sqlda;

/*
**  read statements and process them from Ifp until the ret
**  code tells us we are done
*/

while (!done)
    {

    if (Abort_flag)
	{
        if (D__trans)
            isc_rollback_transaction (isc_status, &D__trans);
        if (M__trans)
            isc_rollback_transaction (isc_status, &M__trans);
        if (gds__trans)
            isc_rollback_transaction (isc_status, &gds__trans);
        /*
         * If there is  current user statement, free it
         * option 2 does the drop
        */
        if (Stmt)
            isc_dsql_free_statement (isc_status, &Stmt, 2);
        if (DB)
            isc_detach_database(isc_status, &DB);
        break;
	}

    ret = get_statement (&statement, &bufsize, sql_prompt);

    /* If there is no database yet, remind us of the need to connect */

    /* But don't execute the statement */

    if (!Db_name[0] && (ret == CONT))
	{
	if (!Quiet)
	    {
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, NO_DB, MSG_LENGTH, 
		errbuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    STDERROUT (errbuf, 1);
#ifdef GUI_TOOLS
            /*
             * This will force an extra carriage return line feed for better
             * formatting of errors when executing a script through windows
             * ISQL.  The Merge_stderr flag is only set when scripts are
             * running.
            */
            if (Merge_stderr)
                ISQL_printf(Out, NEWLINE);
#endif
	    }
	ret = SKIP;
	}

    switch (ret)
	{   
	case CONT:
	    (void) process_statement (statement, sqldap);
	    Continuation = FALSE;
	    break;

	case END:
	case EOF:
	case EXIT:
	    /* Quit upon EOF from top level file pointer */
#ifdef GUI_TOOLS
	    Quiet = TRUE;
#endif
	    if ((ret == EOF) && Continuation)
		{
		gds__msg_format (NULL_PTR, ISQL_MSG_FAC, UNEXPECTED_EOF,
				 MSG_LENGTH, errbuf, NULL_PTR, NULL_PTR,
				 NULL_PTR, NULL_PTR, NULL_PTR);
#ifndef GUI_TOOLS
		STDERROUT (errbuf, 1);
#else
		buffer_error_msg (errbuf, WISQL_SHORTMSG);
#endif
		Exit_value = FINI_ERROR;
		}

            if (Abort_flag)
                {
                if (D__trans)
                    isc_rollback_transaction (isc_status, &D__trans);
                if (M__trans)
                    isc_rollback_transaction (isc_status, &M__trans);
                if (gds__trans)
                    isc_rollback_transaction (isc_status, &gds__trans);
		}
	    else
		{	
	    	if (D__trans)
		    COMMIT_TRANS (&D__trans);
	    	if (M__trans)
		    COMMIT_TRANS (&M__trans);
	    	if (gds__trans)
		    COMMIT_TRANS (&gds__trans);
		}
	    /* 
	     * If there is  current user statement, free it
   	     * I think option 2 is the right one (DSQL_drop), but who knows
	     */
	    if (Stmt)
    	        isc_dsql_free_statement (isc_status, &Stmt, 2);     
	    isc_detach_database (isc_status, &DB);
#ifdef GUI_TOOLS
	    ib_fclose (Out);
	    ib_fclose (Ifp);
#endif
	    done = TRUE;
	    break;

	case BACKOUT:
#ifdef GUI_TOOLS
	    Quiet = TRUE;
#endif
	    if (D__trans)
		isc_rollback_transaction (isc_status, &D__trans);
	    if (M__trans)
		isc_rollback_transaction (isc_status, &M__trans);
	    if (gds__trans)
		isc_rollback_transaction (isc_status, &gds__trans);
	    /* 
	     * If there is  current user statement, free it
   	     * I think option 2 is the right one (DSQL_drop), but who knows
	     */
	    if (Stmt)
    	        isc_dsql_free_statement (isc_status, &Stmt, 2);     
	    isc_detach_database(isc_status, &DB);
#ifdef GUI_TOOLS
	    ib_fclose (Out);
	    ib_fclose (Ifp);
#endif
	    done = TRUE;
	    break;

	case EXTRACT:
	case EXTRACTALL:
	default:
	    /* assert (FALSE);	-- removed as finds too many problems */
	case ERR:
	case FAIL:
	case SKIP:
	    break;
	}
    }

Stmt	    = NULL;
DB 	    = NULL;
Db_name[0]  = '\0';
D__trans    = NULL;
M__trans    = NULL;
gds__trans  = NULL;

/* Should have a valid Temp file pointer */
assert (Ofp != (IB_FILE *) -1)
ib_fclose (Ofp);
unlink (Tmpfile);

if (errbuf)
    ISQL_FREE (errbuf);
if (sqlda)
    ISQL_FREE (sqlda);
if (statement)
    ISQL_FREE (statement);
if (Filelist)
    ISQL_FREE (Filelist);
while (Cols)
    {
    COLLIST	*p;
    p = Cols;
    Cols = Cols->collist_next;
    ISQL_FREE (p);
    }
if (Exit_value == FINI_ERROR)
    exit (Exit_value);
}

static SSHORT drop_db (void)
{
/**************************************
 *
 *	d r o p _ d b
 *
 **************************************
 *
 * Functional description
 *	Drop the current database
 *  
 **************************************/
TEXT	*errbuf;

if (Db_name [0])
    {
    if (!V4)
    	{
	errbuf = (TEXT *) ISQL_ALLOC ((SLONG) MSG_LENGTH);
	if (errbuf)
	    {
    	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, SERVER_TOO_OLD, 
			     MSG_LENGTH, errbuf, NULL_PTR, NULL_PTR, 
			     NULL_PTR, NULL_PTR, NULL_PTR);
    	    STDERROUT (errbuf, 1);
	    ISQL_FREE (errbuf);
	    }
    	return (FAIL);
    	}
    if (isc_drop_database (isc_status, &DB))
	{
	ISQL_errmsg (isc_status);
	return (FAIL);
	}

    /* If error occured and drop database got aborted */
    if (DB)
	return(FAIL);
    }
else
    return (FAIL);
	
/* The database got dropped with or without errors */

M__trans = NULL;
gds__trans = NULL;
Stmt = NULL;
D__trans = NULL;

/* Zero database name */

Db_name[0] = '\0';
DB = NULL;

return (SKIP);
}

static SSHORT edit (
    TEXT	**cmd)
{
/**************************************
 *
 *	e d i t
 *
 **************************************
 *
 * Functional description
 *	Edit the current file or named file
 *
 *	Parameters:  cmd -- Array of words interpreted as file name
 *	The result of calling this is to point the global input file 
 *	pointer, Ifp, to the named file after editing or the tmp file.
 *
 **************************************/
TEXT	*errbuf;
TEXT	*file;
IB_FILE	*fp;
INDEV	current, flist;
TEXT	path[MAXPATHLEN];

errbuf = (TEXT *) ISQL_ALLOC ((SLONG) MSG_LENGTH);
if (!errbuf)
    return (ERR);

/* Local pointer to global list of input files */

flist = Filelist;

/* Set up editing command for shell */

file = cmd [1];

/* If there is a file name specified, try to open it */

if (*file)
    {
    strip_quotes( file, path );
    if (fp = ib_fopen (path, "r"))
	{
	/* Push the current ifp on the indev */

	current = (INDEV) ISQL_ALLOC ((SLONG) sizeof (struct indev));
	current->indev_fpointer = Ifp;
	current->indev_next = NULL;
	if (!Filelist)
	    Filelist = current;
	else
	    {
	    while (flist->indev_next)
		flist = flist->indev_next;
	    flist->indev_next = current;
	    }
	Ifp = fp;
	gds__edit (file, 0);
	}
    else 
	{
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, FILE_OPEN_ERR, MSG_LENGTH, 
	    errbuf, file, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (errbuf, 1);
	}
    }
else
    {
    /* No file given, edit the temp file */

    current = (INDEV) ISQL_ALLOC ((SLONG) sizeof (struct indev));
    current->indev_fpointer = Ifp;
    current->indev_next = NULL;
    if (!Filelist)
	Filelist = current;
    else
	{
	while (flist->indev_next)
	    flist = flist->indev_next;
	flist->indev_next = current;
	}
    /* Close the file, edit it, then reopen and read from the top */
    ib_fclose (Ofp);
    gds__edit (Tmpfile, 0);
    Ofp = ib_fopen (Tmpfile, "r+");
    Ifp = Ofp;
    }

if (errbuf)
    ISQL_FREE (errbuf);

return (SKIP);
}

static int end_trans (void)
{
/**************************************
 *
 *	e n d _ t r a n s
 *
 **************************************
 *
 * Functional description
 *	Prompt the interactive user if there is an extant transaction and
 *	either commit or rollback 
 *
 *	Called by newtrans, createdb, newdb;
 *	Returns success or failure.
 *
 **************************************/
TEXT	buffer [BUFFER_LENGTH80], infobuf [BUFFER_LENGTH60];
int	ret;

ret = CONT;
/* Give option of committing or rolling back before proceding unless
**  the last command was a commit or rollback 
*/

if (M__trans)
    {
    if (Interactive)
	{
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, COMMIT_PROMPT, sizeof (infobuf), 
	    infobuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	ISQL_prompt (infobuf);
	 (void) ib_fgets (buffer,BUFFER_LENGTH80,ib_stdin);
	if (isyesno (buffer))		/* check for Yes answer */
	    {
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, COMMIT_MSG, sizeof (infobuf), 
		infobuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    STDERROUT (infobuf, 1);
	    if (DB && M__trans)
		{
		if (isc_commit_transaction (isc_status, &M__trans))
		    {
		    /* Commit failed, so roll back anyway */
		    ISQL_errmsg (isc_status);
		    ret = FAIL;
		    }
		}
	    }
	else 
	    {
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, ROLLBACK_MSG, sizeof (infobuf),
		infobuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    STDERROUT (infobuf, 1);
	    if (DB && M__trans)
		{
		if (isc_rollback_transaction (isc_status, &M__trans))
		    {
		    ISQL_errmsg (isc_status);
		    ret = FAIL;
		    }
		}
	    }
	}
    else  /* No answer, just roll back by default */
	{
	if (DB && M__trans)
	    {
#ifndef GUI_TOOLS
/* For WISQL, we keep track of whether a commit is needed by setting a flag in the ISQLPB
 * structure.  This flag is set whenever a sql command is entered in the SQL input area or
 * if the user uses the create database dialog.  Because of this, this should only show up if 
 * the user connects and then disconnects or if the user enters a SET TRANSACTION stat without 
 * ever doing anything that would cause changes to the dictionary.
*/
            gds__msg_format (NULL_PTR,
                             ISQL_MSG_FAC,
                             ROLLBACK_MSG, sizeof (infobuf),
                             infobuf,
                             NULL_PTR,
                             NULL_PTR,
                             NULL_PTR,
                             NULL_PTR,
                             NULL_PTR);
	    STDERROUT (infobuf, 1);
#endif
	    if (isc_rollback_transaction (isc_status, &M__trans))
		{
		ISQL_errmsg (isc_status);
		ret = FAIL;
		}
	    }
	}
    }

/* Commit background transaction */


if (DB && D__trans)
    {
    if (isc_commit_transaction (isc_status, &D__trans))
	{
	ISQL_errmsg (isc_status);
	ret = FAIL;
	}
    }
return (ret);
}

static SSHORT escape (
    TEXT	*cmd)
{
/**************************************
 *
 *	e s c a p e
 *
 **************************************
 *
 * Functional description
 *	Permit a shell escape to system call
 *
 *	Parameters:  cmd -- The command string with SHELL 
 *
 **************************************/
TEXT	*shellcmd;

#ifdef GUI_TOOLS
return (SKIP);
#else

/* Advance past the shell */

shellcmd = cmd;

/* Search past the 'shell' keyword */
shellcmd += strlen ("shell");

/* eat whitespace at beginning of command */
while (*shellcmd && isspace (*shellcmd))
    shellcmd++;

/* If no command given just spawn a shell */

if (!*shellcmd)
    {
#ifdef WIN_NT
    if (system("%ComSpec%"))
	return FAIL;
#else
    if (system("$SHELL"))
	return FAIL;
#endif
    }
else
    {    
    if (system (shellcmd))
	return FAIL;
    }

return (SKIP);
#endif
}


static SSHORT frontend (
    TEXT	*statement)
{
/**************************************
 *
 *	f r o n t e n d 
 *
 **************************************
 *
 * Functional description
 *	Handle any frontend commands that start with
 *	show or set converting the string into an
 *	array of words parms, with MAX_TERMS words only.
 *
 *	Parameters: statement is the string typed by the user 
 *
 **************************************/
TEXT	*p, *a, *cmd, *q;
TEXT	*parms [MAX_TERMS], *lparms [MAX_TERMS], *buffer, *errbuf; 
SSHORT	lgth, i, j, length;
SSHORT	ret = SKIP, slen;
TEXT	*psw, *usr, *numbufs, *sql_role_nm;
TEXT	end_quote;
SCHAR	*dialect_str;
USHORT	old_SQL_dialect;
BOOLEAN	bad_dialect = FALSE, print_warning = FALSE, delimited_done = FALSE,
	role_found = FALSE;
TEXT	bad_dialect_buf [512];

buffer = (TEXT*) ISQL_ALLOC ((SLONG) (BUFFER_LENGTH256));
if (!buffer)
    return (ERR);
errbuf = (TEXT *) ISQL_ALLOC ((SLONG) MSG_LENGTH);
if (!errbuf)
    {
    ISQL_FREE (buffer);
    return (ERR);
    }

/* Store the first NUM_TERMS words as they appear in parms, using blanks
   to delimit.   Each word beyond a real word gets a null char 
   Shift parms to upper case, leaving original case in lparms */

for (i = 0; i < sizeof (lparms)/sizeof (lparms[0]); i++)
    lparms [i] = NULL;
for (i = 0; i < sizeof (parms)/sizeof (parms[0]); i++)
    parms [i] = NULL;

p = statement;

/* Eat any whitespace at the beginning */

while (*p && isspace (*p))
    p++;

/* Is this a comment line -- chew until the last comment */

while (*p == '/' && *(p+1) == '*')
    {
    p++;
    p++;
    while (*p && !(*p == '*' && *(p+1) == '/'))
	p++;
    if (*p)
	p += 2;
    else
	{
	/* If we hit the end of string before the close comment,
	** raise an error 
	*/
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, CMD_ERR, MSG_LENGTH, 
	    errbuf, statement, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	STDERROUT (errbuf, 1);
	ISQL_FREE (errbuf);
	ISQL_FREE (buffer);
	return FAIL;
	}
    while (*p && isspace (*p))
	p++;
    }

/* Set beginning of statement past comment */
cmd = p;

for (i = 0; i < MAX_TERMS; i++)
    {
    a = buffer;
    j = 0;
    if (*p == DBL_QUOTE || *p == SINGLE_QUOTE)
	{
	if (i > 0 && (!strcmp (parms [i - 1], KW_ROLE)))
	    role_found = TRUE;
	delimited_done = FALSE;
	end_quote = *p;
	j++;
	*a++ = *p++;
	/* Allow a quoted string to have embedded spaces */
	/*  Prevent overflow */
	while (*p && !delimited_done && j < BUFFER_LENGTH256-1)
	    {
	    if (*p == end_quote)
		{
		j++;
		*a++ = *p++;
		if (*p && *p == end_quote && j < BUFFER_LENGTH256-1)
		    {
		    j++;	/* do not skip the escape quote here */
		    *a++ = *p++;
		    }
		else
		    delimited_done = TRUE;
		}
	    else
		{
		j++;
		*a++ = *p++;
		}
	    }
	}
    else
	{
        /*  Prevent overflow */
        while (*p && !isspace (*p) && j < BUFFER_LENGTH256-1)
	    {
	    j++;
	    *a++ = *p++;
	    }
	}
    *a = '\0';
    length = strlen (buffer);
    parms [i] = (TEXT*) ISQL_ALLOC ((SLONG) (length + 1));
    lparms [i] = (TEXT*) ISQL_ALLOC ((SLONG) (length + 1));
    strncpy (parms [i], buffer, length);
    parms [i][length] = '\0';
    while (*p && isspace (*p))
	p++;
    strcpy (lparms [i], parms [i]);
    if (!role_found)
	ISQL_make_upper (parms [i]);
    role_found = FALSE;
    }

/*
** Look to see if the words (parms) match any known verbs.  If nothing
**  matches then just hand the statement to process_statement
*/


if (!strcmp (parms [0], "SHOW"))
    {
    /* Transaction for all frontend commands */
    if (DB && !gds__trans)
	if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	    {
	    ISQL_errmsg (isc_status);
	    /* Free the frontend command */
	    for (j = 0; j < MAX_TERMS; j++)
		if (parms [j])
		    {
		    ISQL_FREE (parms [j]);
		    ISQL_FREE (lparms [j]);
		    }
	    if (buffer)
	    	ISQL_FREE (buffer);
	    if (errbuf)
	    	ISQL_FREE (errbuf);
	    return FAIL;
	    }
    ret = SHOW_metadata (parms, lparms);
    if (gds__trans)
        COMMIT_TRANS (&gds__trans);
    }
#ifdef DEV_BUILD
else if (!strcmp (parms [0], "ADD"))
    {
    if (DB && !gds__trans)
	if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	    {
	    ISQL_errmsg (isc_status);
	    /* Free the frontend command */
	    for (j = 0; j < MAX_TERMS; j++)
		if (parms [j])
		    {
		    ISQL_FREE (parms [j]);
		    ISQL_FREE (lparms [j]);
		    }
	    if (buffer)
	     	ISQL_FREE (buffer);
	    if (errbuf)
	    	ISQL_FREE (errbuf);
	    return FAIL;
	    }
    ret = add_row (parms [1]);
    if (gds__trans)
        COMMIT_TRANS (&gds__trans);
    }
else if (!strcmp (parms [0], "COPY"))
    {
    if (DB && !gds__trans)
	if (isc_start_transaction (isc_status, &gds__trans, 1, &DB, 0, (SCHAR*) 0))
	    {
	    ISQL_errmsg (isc_status);
	    /* Free the frontend command */
	    for (j = 0; j < MAX_TERMS; j++)
		if (parms [j])
		    {
		    ISQL_FREE (parms [j]);
		    ISQL_FREE (lparms [j]);
		    }
	    if (buffer)
	     	ISQL_FREE (buffer);
	    if (errbuf)
	    	ISQL_FREE (errbuf);
	    return FAIL;
	    }
    ret = copy_table (parms [1], parms [2], lparms [3]);
    if (gds__trans)
        COMMIT_TRANS (&gds__trans);
    }
#endif
#ifdef MU_ISQL
/*
** This is code for QA Test bed Multiuser environment.
 */
else if (!strcmp (parms [0], "PAUSE"))
    {
    if (qa_mu_environment())
	{
	qa_mu_pause();
	ret = SKIP;
        }
    else
	ret = CONT;
    }
#endif MU_ISQL
else if ((!strcmp (parms [0], "BLOBVIEW")) ||
	(!strcmp (parms [0], "BLOBDUMP")))
    ret = blobedit (parms[0], lparms);
else if ((!strcmp (parms [0], "OUTPUT")) ||
	(!strcmp (parms [0], "OUT")))
    ret = newoutput (lparms [1]);
else if (!strcmp (parms [0], "SHELL"))
    ret = escape (cmd);
else if (!strcmp (parms [0], "SET"))
    {
    /* Display current set options */
    if (!*parms [1])
	ret = print_sets();
    else if ((!strcmp (parms [1], "STATS")) ||
	    (!strcmp (parms [1], "STAT")))
	ret = do_set_command (parms[2], &Stats);
    else if (!strcmp (parms [1], "COUNT"))
	ret = do_set_command (parms[2], &Docount);
    else if (!strcmp (parms [1], "LIST"))
	ret = do_set_command (parms[2], &List);
    else if (!strcmp (parms [1], "PLAN"))
	ret = do_set_command (parms[2], &Plan);
    else if ((!strcmp (parms [1], "BLOBDISPLAY")) ||
	    (!strcmp (parms [1], "BLOB")))
	{
	/* No arg means turn off blob display */

	if (!*parms [2] || !strcmp (parms [2], "OFF"))
	    Doblob = NO_BLOBS;
	else if (!strcmp (parms [2], "ALL"))
	    Doblob = ALL_BLOBS;
	else 
	    Doblob = atoi(parms [2]);
	}
    else if (!strcmp (parms [1], "ECHO"))
	ret = do_set_command (parms[2], &Echo);
    else if ((!strcmp (parms [1], "AUTODDL")) ||
	    (!strcmp (parms [1], "AUTO")))
	ret = do_set_command (parms[2], &Autocommit);
    else if (!strcmp (parms [1], "AUTOFETCH")) 
	ret = do_set_command (parms[2], &Autofetch);
    else if (!strcmp (parms [1], "WIDTH"))
	ret = newsize (parms [2], parms[3]);
    else if ((!strcmp (parms [1], "TRANSACTION")) ||
	     (!strcmp (parms [1], "TRANS"))) 
	ret = newtrans(cmd);
    else if ((!strcmp (parms [1], "TERM")) ||
	     (!strcmp (parms [1], "TERMINATOR"))) 
	{
	a = (*lparms [2]) ? lparms [2] : DEFTERM;
	Termlen = strlen (a);
	if (Termlen <= MAXTERM_LENGTH)
	    strcpy (Term, a);
	else
	    {
	    Termlen = MAXTERM_LENGTH;
    	    strncpy (Term, a, MAXTERM_LENGTH);
	    }
	}
    else if (!strcmp (parms [1], "NAMES"))
	{
	if (!*parms [2])
	    {
	    lgth = strlen (DEFCHARSET);
	    if (lgth <= MAXCHARSET_LENGTH) 
    		strcpy (ISQL_charset, DEFCHARSET);
	    else
    		strncpy (ISQL_charset, DEFCHARSET, MAXCHARSET_LENGTH);
	    }
	else
	    {
	    lgth = strlen (parms[2]);
	    if (lgth <= MAXCHARSET_LENGTH) 
	        strcpy (ISQL_charset, parms [2]);
	    else
	        strncpy (ISQL_charset, parms [2], MAXCHARSET_LENGTH);
	    }	
	}
    else if (!strcmp (parms [1], "TIME"))
	{
	ret = do_set_command (parms[2], &Time_display);
	}
#ifdef DEV_BUILD
    else if (!strcmp (parms [1], "SQLDA_DISPLAY"))
	{
	ret = do_set_command (parms[2], &Sqlda_display);
	}
#endif /* DEV_BUILD */
    else if (!strcmp (parms [1], "SQL"))
	{
	if (!strcmp (parms [2], "DIALECT"))
	    {
	    old_SQL_dialect = SQL_dialect; /* save the old SQL dialect */
	    if ((dialect_str = parms [3]) && 
		(SQL_dialect = atoi (dialect_str)))
		{
		if (SQL_dialect < SQL_DIALECT_V5 || 
		    SQL_dialect > SQL_DIALECT_V6)
		    {
		    bad_dialect = TRUE;
		    sprintf (bad_dialect_buf, "%s%s", "invalid SQL dialect ", 
			     dialect_str);
		    SQL_dialect = old_SQL_dialect; /* restore SQL dialect */
		    ret = ERR;
		    }
		else
		    {
		    if (major_ods)
			{
			if (major_ods < ODS_VERSION10)
			    {
			    if (SQL_dialect > SQL_DIALECT_V5)
				{
				if (dialect_spoken)
				    sprintf (bad_dialect_buf, "%s%d%s%s%s%d%s",
					"ERROR: Database SQL dialect ",
					dialect_spoken,
					" database does not accept Client SQL dialect ",
					dialect_str,
					" setting. Client SQL dialect still remains ",
					old_SQL_dialect, NEWLINE);
				else
				    sprintf (bad_dialect_buf, "%s%s%s%s%s%s",
					"ERROR: Pre IB V6 database only speaks ",
					"Database SQL dialect 1 and ",
					"does not accept Client SQL dialect ",
					dialect_str,
					" setting. Client SQL dialect still remains 1.",
					NEWLINE);
				SQL_dialect = old_SQL_dialect;/* restore SQL dialect */
				ISQL_printf(Out, bad_dialect_buf);
				}
			    }
			else /* ODS 10 databases */
			    {
			    switch (dialect_spoken)
				{
				case SQL_DIALECT_V5:
				    if (SQL_dialect > SQL_DIALECT_V5)
					{
					if (SQL_DIALECT_V6_TRANSITION)
					  Merge_stderr = TRUE;
					print_warning = TRUE;
					}
				    break;
				case SQL_DIALECT_V6:
				    if (SQL_dialect == SQL_DIALECT_V5 ||
					SQL_dialect == SQL_DIALECT_V6_TRANSITION)
					{
					if (SQL_DIALECT_V6_TRANSITION)
					  Merge_stderr = TRUE;
					print_warning = TRUE;
					}
				    break;
				default:
				    break;
				}
			    if (print_warning && Warnings)
				{
				print_warning = FALSE;
				sprintf (bad_dialect_buf, "%s%d%s%d%s%s",
					"WARNING: Client SQL dialect has been set to ",
					SQL_dialect,
					" when connecting to Database SQL dialect ",
					dialect_spoken,
					" database. ",
					NEWLINE);
				ISQL_printf(Out, bad_dialect_buf);
				}
			    }
			}
		    }
		}
	    else /* handle non numeric invalid "set sql dialect" case */
		{
		SQL_dialect = old_SQL_dialect; /* restore SQL dialect */
		bad_dialect = TRUE;
		sprintf (bad_dialect_buf, "%s%s", "invalid SQL dialect ", 
			 dialect_str);
		ret = ERR;
		}
	    }
	else
	    ret = ERR;
	}
    else if (!strcmp (parms [1], "GENERATOR"))
	ret = CONT;
    else if (!strcmp (parms [1], "STATISTICS"))
	ret = CONT;
    else
	ret = ERR;
    }
else if (!strcmp (parms [0], "CREATE"))
    {
    if (!strcmp (parms [1], "DATABASE") || !strcmp (parms [1], "SCHEMA"))
	ret = create_db (cmd, lparms [2]);
    else
	ret = CONT;
    }
else if (!strcmp (parms [0], "DROP"))
    {
    if (!strcmp (parms [1], "DATABASE") || !strcmp (parms [1], "SCHEMA"))
	if (*parms [2])
	    ret = ERR;
	else
	    ret = drop_db ();
    else
	ret = CONT;
    }
else if (!strcmp (parms [0], "CONNECT")) 
    {
    psw = (TEXT*) NULL;
    usr = (TEXT*) NULL;
    sql_role_nm = (TEXT*) NULL;
    numbufs = (TEXT *) NULL;

    /* if a parameter is given in the command more than once, the
       last one will be used. The parameters can appear each any
       order, but each must provide a value. */

     ret = SKIP;
     for (i = 2; i < (MAX_TERMS-1);)
        {
	if (!strcmp (parms [i], "CACHE") && *lparms [i+1])
	    {
	    numbufs = lparms [i+1];
	    i += 2;
	    }
	else if (!strcmp (parms [i], "USER") && *lparms [i+1])
	    {
	    usr = lparms [i+1];
	    i += 2;
	    }
	else if (!strcmp (parms [i], "PASSWORD") && *lparms [i+1])
	    {
	    psw = lparms [i+1];
	    i += 2;
	    }
	else if (!strcmp (parms [i], "ROLE") && *lparms [i+1])
	    {
	    sql_role_nm = lparms [i+1];
	    i += 2;
	    }
	else if (*parms [i])
	    {
	    /* Unrecognized option to CONNECT */
	    ret = ERR;
	    break;
	    }
	else
	    i++;
        }
    if (ret != ERR)
	ret = newdb (lparms [1], usr, psw, numbufs, sql_role_nm);
    }

#ifdef SCROLLABLE_CURSORS
/* perform scrolling within the currently active cursor */

else if (!strcmp (parms [0], "NEXT"))
    {
    fetch_offset = 1;
    fetch_direction = 0;
    ret = FETCH;
    }
else if (!strcmp (parms [0], "PRIOR"))
    {
    fetch_offset = 1;
    fetch_direction = 1;
    ret = FETCH;
    }
else if (!strcmp (parms [0], "FIRST"))
    {
    fetch_offset = 1;
    fetch_direction = 2;
    ret = FETCH;
    }
else if (!strcmp (parms [0], "LAST"))
    {
    fetch_offset = 1;
    fetch_direction = 3;
    ret = FETCH;
    }
else if (!strcmp (parms [0], "ABSOLUTE"))
    {
    fetch_direction = 4;
    fetch_offset = atoi (parms [1]);
    ret = FETCH;
    }
else if (!strcmp (parms [0], "RELATIVE"))
    {
    fetch_direction = 5;
    fetch_offset = atoi (parms [1]); 
    ret = FETCH;
    }
#endif
else if (!strcmp (parms [0], "EDIT"))
    ret = edit(lparms);
else if ((!strcmp (parms [0], "INPUT")) ||
	(!strcmp (parms [0], "IN")))
	{
        Input_file = TRUE;
        ret = input(lparms [1]);
	}
else if (!strcmp (parms [0], "QUIT"))
    ret = BACKOUT;
else if (!strcmp (parms [0], "EXIT"))
    ret = EXIT;
else if (!strcmp (parms [0], "?") || (!strcmp (parms [0], "HELP")))
    ret = help (parms [1]);
else /* Didn't match, it must be SQL */
    ret = CONT;

/* In case any default transaction was started - commit it here */
if (gds__trans)
    COMMIT_TRANS (&gds__trans);

/* Free the frontend command */

for (j = 0; j < MAX_TERMS; j++)
    if (parms [j])
	{
	ISQL_FREE (parms [j]);
	ISQL_FREE (lparms [j]);
	}

#ifndef GUI_TOOLS
if (ret == ERR)
    {
    if (bad_dialect == TRUE)
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, CMD_ERR, MSG_LENGTH,
	    errbuf, bad_dialect_buf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    else
	gds__msg_format (NULL_PTR, ISQL_MSG_FAC, CMD_ERR, MSG_LENGTH, 
	    errbuf, cmd, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    }
#endif

if (buffer)
    ISQL_FREE (buffer);
if (errbuf)
    ISQL_FREE (errbuf);

return (ret);
}
static SSHORT	do_set_command (
    TEXT 	*parm,
    SSHORT	*global_flag)
{
/**************************************
 *
 *	d o _ s e t _ c o m m a n d
 *
 **************************************
 *
 * Functional description
 *    set the flag pointed to by global_flag
 *        to TRUE or FALSE.
 *    if parm is missing, toggle it
 *    if parm is "ON", set it to TRUE
 *    if parm is "OFF", set it to FALSE
 *
 **************************************/
SSHORT ret = SKIP;

if (!*parm)
    *global_flag = 1 - *global_flag;
else if (!strcmp (parm, "ON"))
    *global_flag = 1;
else if (!strcmp (parm, "OFF"))
    *global_flag = 0;
else
    ret = ERR;
return (ret);	
}

static SSHORT get_statement (
    TEXT	**statement_ref,
    USHORT	*statement_buffer,
    TEXT	*statement_prompt)
{
/**************************************
 *
 *	g e t _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Get an SQL statement, or QUIT/EXIT command to process
 *
 *	Arguments:  POinter to statement, and size of statement_buffer
 *
 **************************************/
TEXT	*p, *q;
TEXT	*statement, *statement2;
SSHORT	c, blank, done, start_quoted_string;
USHORT	count, bufsize, bufsize2;
SSHORT	ret = CONT;
INDEV	flist, oflist;
TEXT	con_prompt [MSG_LENGTH];
SSHORT	incomment = FALSE;
SSHORT  in_single_quoted_string = FALSE;  /* set to true once we are
                                          inside a single quoted string*/
SSHORT  in_double_quoted_string = FALSE;
SSHORT  *errbuf;
BOOLEAN ignore_this_stmt = FALSE;

/* Lookup the continuation prompt once */
ISQL_msg_get (CON_PROMPT, con_prompt, NULL, NULL, NULL, NULL, NULL);

bufsize = *statement_buffer;

/* Statement has no space so allocate some. The statement buffer just
** grows throughout the program to the maximum command size
*/
if (!bufsize)
    {
    bufsize = 512;
    statement = (TEXT*) ISQL_ALLOC ((SLONG) bufsize);
#ifdef DEBUG_GDS_ALLOC
    gds_alloc_flag_unfreed ((void *) statement);
#endif
    *statement_ref = statement;
    *statement_buffer = bufsize;
    }
else
    /* Otherwise just recall the address from last time */
    statement = *statement_ref;

#ifndef GUI_TOOLS
if (Ifp == ib_stdin)
    {
    ib_fprintf (ib_stdout, statement_prompt);
    ib_fflush (ib_stdout);
    }
#endif

/* Clear out statement buffer */

p = statement;
*p = '\0';

/* Set count of characters to zero */

count = 0;

blank = TRUE;
done = FALSE;

while (!done)
    {
    switch (c = ib_getc (Ifp))
	{
	case EOF:
	    /* Go back to ib_getc if we get interrupted by a signal. */

	    if (SYSCALL_INTERRUPTED(errno))
	    {
		errno = 0;
		break;
	    }

	    /* If we hit EOF at the top of the flist, exit time */

	    flist = Filelist;
	    if (!flist->indev_next)
		return EOF;

	    /* If this is not ib_tmpfile, close it */

	    if (Ifp != Ofp)
		ib_fclose (Ifp);

	    /* Save the last flist so it can be deleted */

	    while (flist->indev_next)
		{
		oflist = flist;
		flist = flist->indev_next;
		}
				
	    /* Zero out last pointer */

	    oflist->indev_next = NULL;

	    /* Reset to previous after other input */

	    Ifp = flist->indev_fpointer;
	    ISQL_FREE (flist);

	    if (Interactive && (Ifp == ib_stdin))
		ISQL_prompt (statement_prompt);
	    break;

	case '\n':
	    /* the context of string literals should not extend past eoln */
	    in_single_quoted_string = in_double_quoted_string = FALSE;

	    /* Ignore a series of carriage returns at the beginning */
	    if (blank)
		{
		if (Interactive && (Ifp == ib_stdin))
		    ISQL_prompt (statement_prompt);
		break;
		}

	    /* Catch the help ? without a terminator */
	    if (*statement == '?')
		{
		done = TRUE;
		Continuation = FALSE;
		break;
		}

	    /* Put the newline on the line for printing */
	    *p++ = c;
	    count++;

	    /* If in a comment, keep reading */
	    if (incomment && Interactive && (Ifp == ib_stdin))
		{
		/*  Comment prompt */
		ISQL_prompt ("--> ");
		break;
		}

	    /* Eat any white space at the end of a line */

	    q = p;
	    while (q > statement && isspace(*(q - 1)))
		q--;

	    /* Search back past a final comment for a terminator 
	    **  If a comment ends a line with a terminator,
	    **  null terminate the line before the comment
	    **  Otherwise, just read on
	    */
	    if ((*(q - 1) == '/') && (*(q - 2) == '*'))
		{
		q -= 2;

		/* Don't search past a newline or beginning of statement  */

		while ( q != statement && (*q != '\n') && 
			(*(q - 1) != '*') && (*(q - 2) != '/'))
		    q--;
		/* If the search took us to begin line, keep reading */
		if ((q == statement) || (*q == '\n'))
		    {
		    if (Interactive && (Ifp == ib_stdin))
		        ISQL_prompt (con_prompt);
		    break;
		    }

		q -= 2;
		}

	    while ((q > statement) && isspace(*(q - 1)))
		q--;

	    /* 
	    **  Look for terminator as last chars then
	    **  null terminate string and set done
	    *   do not bother with terminators inside
	    *   quoted strings and comments.
	    */

	    if (!incomment && 
		/* Making sure that q does'nt go out of bounds */
		((q-statement) >= Termlen ) &&
		!strncmp (q - Termlen, Term, Termlen))
		{
		if (Echo)
		    {
		    /* Terminate to print entire statement */
		    *p = '\0';
		    ISQL_printf (Out, statement);
		    }
		
#ifdef GUI_TOOLS 	
    		/* add command to command history file */
    		ib_fprintf (Session, "%s", statement);
    		ib_fflush (Session);
#endif

		/* Lop off terminal comment */
		*(q - Termlen) = '\0';
		done = TRUE;
		Continuation = FALSE;
		}
	    else 
		{

		/* Continuation prompt */
		if (Interactive && (Ifp == ib_stdin))
		    ISQL_prompt (con_prompt);
		}
	    break;

	case '\t':
	case BLANK:
	   /* Skip leading blanks and convert tabs */

	    if (!blank)
		{
		*p++ = BLANK;
		count++;
		}
	   break;
		
	case '*':
	    /* Could this the be start of a comment.  We can only look back,
	    **   not forward
	    * ignore possibilities of a comment beginning inside
      	    * quoted strings.
	    */
	    if (count && (*(p-1) == '/') &&
		!in_single_quoted_string && !in_double_quoted_string)
		incomment = TRUE;
	    *p++ = c;
	    blank = FALSE;
	    count++;
	    break;

	case '/':
	    /* Perhaps this is the end of a comment 
	    * ignore possibilities of a comment ending inside
	    * quoted strings.
	    */
	    if (incomment && (*(p-1) == '*') &&
		!in_single_quoted_string && !in_double_quoted_string)
		incomment = FALSE;
	    *p++ = c;
	    blank = FALSE;
	    count++;
	    break;

       case SINGLE_QUOTE:
	    /* check if this quote ends an open quote */
            if (!incomment && !in_double_quoted_string)
                if (in_single_quoted_string)
		    {
                    if (count && (*(p-1) != SINGLE_QUOTE) ||
		       (count == start_quoted_string+1))
		       /* for strings such as '' which are empty strings, 
		       we need to reset the in_single_quoted_string flag */
                        in_single_quoted_string = FALSE;
		    }
                else
		    {
		    start_quoted_string = count;
                    in_single_quoted_string = TRUE;
		    }
            *p++ = c;
            blank = FALSE;
            count++;
            break;

        case DBL_QUOTE:
	    /* check if this double quote ends an open double quote */
            if (!incomment && !in_single_quoted_string)
                if (in_double_quoted_string)
		    {
                    if (count && (*(p-1) != '\"') ||
		       (count == start_quoted_string+1))
		       /* for strings such as "" which are empty strings, 
			we need to reset the in_single_quoted_string flag */
                        in_double_quoted_string = FALSE;
		    }
                else
		    {
		    start_quoted_string = count;
                    in_double_quoted_string = TRUE;
		    }
            *p++ = c;
            blank = FALSE;
            count++;
            break;


	default:
	    /* Any other character is significant */

	    *p++ = c;
	    count++;
	    blank = FALSE;
	    if (!incomment) Continuation = TRUE;
	    break;
	}


	/* If in danger of running out of room, reallocate to a bigger 
	**  Statement buffer, keeping track of where p is pointing
	*/

	if (count == bufsize)
	    {
	    if ((USHORT)(bufsize + 512) < bufsize)
	      {
	      /* If we allocate any more, we would be in danger of 
		 overflowing the variable bufsize */
	      errbuf = (TEXT*) ISQL_ALLOC ((SLONG) MSG_LENGTH);
	      if (errbuf)
		{
                gds__msg_format (NULL_PTR, ISQL_MSG_FAC, BUFFER_OVERFLOW, MSG_LENGTH,
	          errbuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	        STDERROUT (errbuf, 1);
	        ISQL_FREE (errbuf);
		}
	      ret = SKIP;
	      ISQL_FREE (statement);

              ignore_this_stmt = TRUE;
	      /* now reallocate the buffer to hold the reminder of the statement */
   	      bufsize = 512;
	      statement = (TEXT*) ISQL_ALLOC ((SLONG) bufsize);
#ifdef DEBUG_GDS_ALLOC
	      gds_alloc_flag_unfreed ((void *) statement);
#endif
	      *statement_ref = statement;
	      *statement_buffer = 512;
	      in_single_quoted_string = in_double_quoted_string = FALSE;
	      count = 0;
	      p = statement;
	      *p = '\0';
	      }
	    else
	      {
	      bufsize2 = bufsize;
	      bufsize += 512;
	      statement2 = (TEXT*) ISQL_ALLOC ((SLONG) bufsize);
#ifdef DEBUG_GDS_ALLOC
	      gds_alloc_flag_unfreed (statement2);
#endif
	      memcpy (statement2, statement, bufsize2);
	      ISQL_FREE (statement);
	      statement = statement2;
	      *statement_ref = statement;
	      *statement_buffer = bufsize;
	      p = statement + count;
	      }
	    }

    /* Pass statement to frontend for processing */

    if (done && !ignore_this_stmt)
	ret = frontend(statement);	
    }

    /* If this was a null statement, skip it */
    if (!*statement)
	ret = SKIP;


if (ret == CONT)
    {

    /* Place each non frontend statement in the temp file if we are reading
    ** from ib_stdin.  Add newline to make the file more readable 
    */

    if (Ifp == ib_stdin) 
	{
	ib_fputs (statement, Ofp);
	ib_fputs (Term, Ofp);
	ib_fputc ('\n', Ofp);
	}
    }

return ret;
}

static void get_str (
    TEXT	*input_str,
    TEXT	**str_begin,
    TEXT	**str_end,
    SSHORT	*str_flag)
{
/**************************************
 *
 *	g e t _ s t r
 *
 **************************************
 *
 * Functional description
 *
 *	Scanning a string constant from the input buffer. If it found, then
 *	passes back the starting address and ending address of the string 
 *	constant. It also marks the type of string constant and passes back.
 *	
 *	string type:
 *
 *		1. SINGLE_QUOTED_STRING --- string constant delimited by
 *						single quotes
 *		2. DOUBLE_QUOTED_STRING --- string constant delimited by
 *						double quotes
 *		3. NO_MORE_STRING       --- no string constant was found
 *
 *	Input Arguments:
 *
 *		1. input buffer
 *
 *	Output Arguments:
 *
 *		1. pointer to the input buffer
 *		2. address to the begining of a string constant
 *		3. address to the ending   of a string constant
 *		4. address to the string flag
 *
 **************************************/
TEXT	*b1 = NULL, *b2 = NULL, delimited_char;
BOOLEAN	done = FALSE;

b1 = strchr (input_str, SINGLE_QUOTE);
b2 = strchr (input_str, DBL_QUOTE);
if (!b1 && !b2)
    *str_flag = NO_MORE_STRING;
else
    {
    if (b1 && !b2)
	*str_flag = SINGLE_QUOTED_STRING;
    else
	if (!b1 && b2)
	    {
	    *str_flag = DOUBLE_QUOTED_STRING;
	    b1 = b2;
	    }
	else
	    if (b1 > b2)
		{
		*str_flag = DOUBLE_QUOTED_STRING;
		b1 = b2;
		}
	    else
		*str_flag = SINGLE_QUOTED_STRING;

    *str_begin = b1;
    delimited_char = *b1;
    b1++;
    while (!done)
	{
	if (*b1 == delimited_char)
	    {
	    b1++;
	    if (*b1 == delimited_char)
		b1++;
	    else
		{
		done = TRUE;
		*str_end = b1;
		}
	    }
	else
	    b1 = strchr (b1, delimited_char);

	/* In case there is no matching ending quote (single or double) 
       either because of mismatched quotes  or simply because user 
       didn't complete the quotes, we cannot find a valid string
       at all. So we return a special value INCOMPLETE_STRING.
       In this case str_end is not populated and hence copy_str
       should not be called after get_str, but instead the caller
       must process this error & return to it's calling routine
       - Shailesh */
	if (b1 == NULL)
	{
		*str_flag = INCOMPLETE_STRING;
		done = TRUE;
	}
	} /* end of while (!done) */
    }
}

void ISQL_get_version (
    BOOLEAN	call_by_create_db)
{
/**************************************
 *
 *	I S Q L _ g e t _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	finds out if the database we just attached to is
 *	V4, V3.3 or earlier, as well as other info.
 *
 **************************************/
static CONST UCHAR
	db_version_info [] = { isc_info_ods_version,
			       isc_info_ods_minor_version,
			       isc_info_db_sql_dialect,
			       isc_info_end };
    /* 
    ** Each info item requested will return 
    **
    **	  1 byte for the info item tag
    **	  2 bytes for the length of the information that follows
    **	  1 to 4 bytes of integer information
    **
    ** isc_info_end will not have a 2-byte length - which gives us
    ** some padding in the buffer.
    */

UCHAR	buffer [sizeof(db_version_info)*(1+2+4)];
UCHAR	*p;
TEXT	bad_dialect_buf [512];
BOOLEAN	print_warning = FALSE;

V4 = FALSE;
V33 = FALSE;
dialect_spoken = 0;

if (isc_database_info (isc_status, &DB, sizeof (db_version_info),
			 db_version_info, sizeof (buffer), buffer))
    {
    ISQL_errmsg (isc_status);
    return;
    }

p = buffer;
while (*p != isc_info_end)
    {
    UCHAR	item;
    USHORT	length;

    item = (UCHAR) *p++;
    length = isc_vax_integer (p, sizeof (USHORT));
    p += sizeof (USHORT);
    switch (item)
	{
	case isc_info_ods_version:
	    major_ods = isc_vax_integer (p, length);
	    break;
	case isc_info_ods_minor_version:
	    minor_ods = isc_vax_integer (p, length);
	    break;
	case isc_info_db_sql_dialect:
	    dialect_spoken = isc_vax_integer (p, length);
	    if (major_ods < ODS_VERSION10)
		{
		if (SQL_dialect > SQL_DIALECT_V5 && Warnings)
		    {
		    ISQL_printf(Out, NEWLINE);
		    sprintf (bad_dialect_buf, "%s%s%s%d%s%s",
			"WARNING: Pre IB V6 database only speaks",
			" SQL dialect 1 and ",
			"does not accept Client SQL dialect ",
			SQL_dialect,
			" . Client SQL dialect is reset to 1.",
			NEWLINE);
		    ISQL_printf(Out, bad_dialect_buf);
		    }
		}
	    else /* ODS 10 databases */
		{
		switch (dialect_spoken)
		    {
		    case SQL_DIALECT_V5:
			if (SQL_dialect > SQL_DIALECT_V5)
			    print_warning = TRUE;
			break;

		    case SQL_DIALECT_V6:
			if (SQL_dialect != 0 && SQL_dialect < SQL_DIALECT_V6)
			    print_warning = TRUE;
			break;
		    default:
			break;
		    }
		if (print_warning && Warnings)
		    {
		    print_warning = FALSE;
		    ISQL_printf(Out, NEWLINE);
		    sprintf (bad_dialect_buf, "%s%d%s%d%s%s",
			"WARNING: This database speaks SQL dialect ",
			dialect_spoken,
			" but Client SQL dialect was set to ",
			SQL_dialect,
			" .",
			NEWLINE);
		    ISQL_printf(Out, bad_dialect_buf);
		    }
		}
	    break;
	case isc_info_error:
	    /* 
	    ** Error indicates an option was not understood by the
	    ** remote server.
	    */
	    if (SQL_dialect && SQL_dialect != SQL_DIALECT_V5 && Warnings)
		{
		ISQL_printf(Out, NEWLINE);
		if (call_by_create_db)
		    sprintf (bad_dialect_buf, "%s%s%d%s%s",
			"WARNING: Pre IB V6 server only speaks SQL dialect 1",
			" and does not accept Client SQL dialect ",
			SQL_dialect,
			" . Client SQL dialect is reset to 1.",
			NEWLINE);
		else
		    {
		    connecting_to_pre_v6_server = TRUE;
		    sprintf (bad_dialect_buf, "%s%s%d%s%s",
			"ERROR: Pre IB V6 server only speaks SQL dialect 1",
			" and does not accept Client SQL dialect ",
			SQL_dialect,
			" . Client SQL dialect is reset to 1.",
			NEWLINE);
		    }
		ISQL_printf(Out, bad_dialect_buf);
		}
	    else
		{
		if (SQL_dialect == 0)
		    {
		    connecting_to_pre_v6_server = TRUE;
		    sprintf (bad_dialect_buf, "%s%s%d%s%s",
			"ERROR: Pre IB V6 server only speaks SQL dialect 1",
			" and does not accept Client SQL dialect ",
			SQL_dialect,
			" . Client SQL dialect is reset to 1.",
			NEWLINE);
		    ISQL_printf(Out, bad_dialect_buf);
		    }
		}
	    break;
	default:
	    {
	    TEXT	message [100];
	    sprintf (message, "Internal error: Unexpected isc_info_value %d\n",
			item);
	    ISQL_printf (Out, message);
	    }
	    break;
	}
    p += length;
    }

if (major_ods >= ODS_VERSION8)
   {
    V4 = TRUE;
    if (major_ods >= ODS_VERSION9)
	V45 = TRUE;
   }
else if (major_ods == ODS_VERSION7 && 
	 minor_ods == ODS_CURRENT7)
    V33 = TRUE;

/* If the remote server did not respond to our request for
   "dialects spoken", then we can assume it can only speak
   the V5 dialect.  We automatically change the connection
   dialect to that spoken by the server.  Otherwise the 
   current dialect is set to whatever the user requested. */

if (dialect_spoken == 0)
    SQL_dialect = SQL_DIALECT_V5;
else
    if (major_ods < ODS_VERSION10)
	SQL_dialect = dialect_spoken;
    else
	if (SQL_dialect == 0) /* client SQL dialect has not been set */
	    SQL_dialect = dialect_spoken;

if (dialect_spoken > 0)
    db_SQL_dialect = dialect_spoken;
else
    db_SQL_dialect = SQL_DIALECT_V5;
}

void ISQL_truncate_term (
    TEXT        *str,
    USHORT      len)
{
/**************************************
 *
 *	I S Q L _ t r u n c a t e _ t e r m
 *
 **************************************
 *
 * Functional description
 *	Truncates the rightmost contiguous spaces on a string.
 *
 **************************************/
int i;
for (i=len-1; i>=0 && ( (isspace(str[i])) || (str[i] == 0)); i--) ;
str[i+1] = 0;
}

void ISQL_ri_action_print (
    TEXT        *ri_action_str,
    TEXT        *ri_action_prefix_str,
    BOOLEAN     all_caps) 
{
/**************************************
 *
 *	I S Q L _ r i _ a c t i o n _ p r i n t 
 *
 **************************************
 *
 * Functional description
 *	prints the description of ref. integrity actions.
 *      The actions must be one of the cascading actions or RESTRICT.
 *      RESTRICT is used to indicate that the user did not specify any
 *      actions, so do not print it out.
 *
 **************************************/
RI_ACTIONS *ref_int;

for (ref_int = ri_actions_all; ref_int->ri_action_name; ref_int++)
    {
    if (!strcmp (ref_int->ri_action_name, ri_action_str))
        {
	if (*ref_int->ri_action_print_caps)
	/* we have something to print  */
	    {
            if (all_caps)
                sprintf (Print_buffer, "%s %s", ri_action_prefix_str, 
		    ref_int->ri_action_print_caps);
            else
	        if (*ref_int->ri_action_print_mixed)
                    sprintf (Print_buffer, "%s %s", ri_action_prefix_str,
                    ref_int->ri_action_print_mixed);
            ISQL_printf (Out, Print_buffer);
	    }
	return;
	}
   }

assert (FALSE);
}

static BOOLEAN get_numeric (
    UCHAR       *string,
    USHORT      length,
    SSHORT      *scale,
    SINT64      *ptr)
{
/**************************************
 *
 *      g e t _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *      Convert a numeric literal (string) to its binary value.
 *
 *      The binary value (int64) is stored at the
 *      address given by ptr.
 *
 **************************************/
UCHAR   *p, *end;
SSHORT  local_scale, fraction, sign, digit_seen;
SINT64   value;

digit_seen = local_scale = fraction = sign = value = 0;

for (p = string, end = p + length; p < end; p++)
    {
    if (DIGIT (*p))
	{
	digit_seen = 1;

	/* Before computing the next value, make sure there will be
	   no overflow. Trying to detect overflow after the fact is
	   tricky: the value doesn't always become negative after an
	   overflow!  */

	if (value >= INT64_LIMIT)  
	    {
	    /* possibility of an overflow */
	    if (value > INT64_LIMIT)
		break;
	    else if (((*p > '8') && (sign == -1)) || ((*p > '7') && (sign != -1)))
		break;
	    }

 	/* Force the subtraction to be performed before the addition,
	   thus preventing a possible signed arithmetic overflow.    */
	value = value * 10 + (*p - '0');
	if (fraction)
	    --local_scale;
	}
    else if (*p == '.')
	{
	if (fraction)
	    return FALSE;
	else
	    fraction = TRUE;
	}
    else if (*p == '-' && !digit_seen && !sign && !fraction)
	sign = -1;
    else if (*p == '+' && !digit_seen && !sign && !fraction)
	sign = 1;
    else if (*p != BLANK)
	return FALSE;
    }

if (!digit_seen)
    return FALSE;

*scale = local_scale;
*(SINT64*) ptr = ((sign == -1) ? -value : value );
return TRUE;
}

static SSHORT newsize (
    TEXT 	*colname,
    TEXT	*sizestr)
{
/**************************************
 *
 *	n e w s i z e
 *
 **************************************
 *
 * Functional description
 *	Add a column name and print width to collist
 *
 **************************************/
COLLIST	*c, *p, *pold;
SSHORT	size;

if (!*colname || (strlen (colname) >= WORDLENGTH))
    return ERR;

p = Cols;
pold = NULL;

/* If no size is given, remove the entry */
 if (!*sizestr)
     {
	/* Scan until name matches or end of list */
     while (p && strcmp (p->col_name, colname))
 	{
	pold = p;
 	p = p->collist_next;
 	}
     
     /* If there is a match on name, delete the entry */
     if (p)
 	{
 	if (pold)
 	    pold->collist_next = p->collist_next;
 	else
 	    Cols = NULL;
 	ISQL_FREE (p);
 	}
     return SKIP;
     }

size = atoi (sizestr);
if (size <= 0)
    return ERR;

c = (COLLIST*) ISQL_ALLOC ((SLONG) sizeof (struct collist));
strcpy (c->col_name, colname);
c->col_len = size;
c->collist_next = NULL;

/* walk the linked list */
if (!p)
    Cols = c;
else
    {
    while (p->collist_next && strcmp (p->col_name, colname))
	p = p->collist_next;

    /* If there is a match on name, replace the length */
    if (!strcmp (p->col_name, colname))
	{
	p->col_len = size;
	ISQL_FREE (c);
	}
    else
	p->collist_next = c;
    }
return SKIP;
}

static SSHORT print_sets (void)
{
/**************************************
 *
 *	p r i n t _ s e t s
 *
 **************************************
 *
 * Functional description
 *	Print the current set values
 *
 **************************************/
COLLIST	*p;

ISQL_printf (Out, "Print statistics:        ");
ISQL_printf (Out, (Stats ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
#ifndef GUI_TOOLS
ISQL_printf (Out, "Echo commands:           ");
ISQL_printf (Out, (Echo ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
#endif
ISQL_printf (Out, "List format:             ");
ISQL_printf (Out, (List ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
ISQL_printf (Out, "Row Count:               ");
ISQL_printf (Out, (Docount ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
ISQL_printf (Out, "Autocommit DDL:          ");
ISQL_printf (Out, (Autocommit ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
#ifdef SCROLLABLE_CURSORS
ISQL_printf (Out, "Autofetch records:       ");
ISQL_printf (Out, (Autofetch ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
#endif
ISQL_printf (Out, "Access Plan:             ");
ISQL_printf (Out, (Plan ? "ON" : "OFF"));
ISQL_printf (Out, NEWLINE);
ISQL_printf (Out, "Display BLOB type:       ");
if (Doblob == ALL_BLOBS)
    ISQL_printf (Out, "ALL");
else if (Doblob == NO_BLOBS)
    ISQL_printf (Out, "NONE");
else 
    {
    sprintf (Print_buffer, "%d", Doblob);
    ISQL_printf (Out, Print_buffer);
    }
ISQL_printf (Out, NEWLINE);
if (*ISQL_charset && strcmp(ISQL_charset, DEFCHARSET))
    {
    sprintf (Print_buffer, "Set names:               %s%s", 
	     ISQL_charset, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }
if (Cols)
    {
    p = Cols;
    sprintf (Print_buffer, "Column print widths:%s", NEWLINE);
    ISQL_printf (Out, Print_buffer);
    while (p)
	{
	sprintf (Print_buffer, "%s%s width: %d%s", 
		 TAB,
		 p->col_name, 
		 p->col_len,
		 NEWLINE);
	ISQL_printf (Out, Print_buffer);
	p = p->collist_next;
	}
    }
ISQL_printf (Out, "Terminator:              ");
ISQL_printf (Out, Term);
ISQL_printf (Out, NEWLINE);
sprintf (Print_buffer, "Time:                    %s%s", 
		(Time_display) ? "ON" : "OFF", NEWLINE);
ISQL_printf (Out, Print_buffer);
sprintf (Print_buffer, "Warnings:                %s%s",
	 (Warnings) ? "ON" :  "OFF", NEWLINE);
ISQL_printf (Out, Print_buffer);
return (SKIP);
}

static SSHORT help (TEXT *what)
{
/**************************************
 *
 *	h e l p
 *
 **************************************
 *
 * Functional description
 *	List the known commands.
 *
 **************************************/
#ifndef GUI_TOOLS
TEXT	msg [MSG_LENGTH];
SSHORT	*msgid;

/* Ordered list of help messages to display.  Use -1 to terminate list,
 * and 0 for an empty blank line
 */
static SSHORT	help_ids [] = {
  HLP_FRONTEND,	/*Frontend commands:*/
  HLP_BLOBDMP,	/*BLOBDUMP <blobid> <file>   -- dump BLOB to a file*/
  HLP_BLOBVIEW,	/*BLOBVIEW <blobid>          -- view BLOB in text editor*/
  HLP_EDIT,	/*EDIT     [<filename>]      -- edit SQL script file and execute*/
  HLP_EDIT2,	/*EDIT                       -- edit current command buffer and execute*/
  HLP_HELP,	/*HELP                       -- display this menu*/
  HLP_INPUT,	/*INput    <filename>        -- take input from the named SQL file*/
  HLP_OUTPUT,	/*OUTput   [<filename>]      -- write output to named file*/
  HLP_OUTPUT2,	/*OUTput                     -- return output to ib_stdout*/
  HLP_SET_ROOT, /*SET      <option>          -- (use HELP SET for list)*/
  HLP_SHELL,	/*SHELL    <command>         -- execute Operating System command in sub-shell*/
  HLP_SHOW,	/*SHOW     <object> [<name>] -- display system information*/
  HLP_OBJTYPE,	/*    <object> = CHECK, DATABASE, DOMAIN, EXCEPTION, FILTER, FUNCTION, GENERATOR,*/
  HLP_OBJTYPE2,	/*               GRANT, INDEX, PROCEDURE, ROLE, SQL DIALECT, SYSTEM, TABLE,*/
  HLP_OBJTYPE3, /*               TRIGGER, VERSION, VIEW*/
  HLP_EXIT,	/*EXIT                       -- exit and commit changes*/
  HLP_QUIT,	/*QUIT                       -- exit and roll back changes*/
  0,
  HLP_ALL,	/*All commands may be abbreviated to letters in CAPitals*/
 -1		/* end of list */
  };

static SSHORT help_set_ids [] = {
  HLP_SETCOM,	/*Set commands:*/
  HLP_SET,	/*    SET                    -- display current SET options*/
  HLP_SETAUTO,	/*    SET AUTOddl            -- toggle autocommit of DDL statements*/
#ifdef SCROLLABLE_CURSORS
  HLP_SETFETCH,	/*    SET AUTOfetch          -- toggle autofetch of records*/
#endif
  HLP_SETBLOB,	/*    SET BLOB [ALL|<n>]     -- display BLOBS of subtype <n> or ALL*/
  HLP_SETBLOB2,	/*    SET BLOB               -- turn off BLOB display*/
  HLP_SETCOUNT,	/*    SET COUNT              -- toggle count of selected rows on/off*/
  HLP_SETECHO,	/*    SET ECHO               -- toggle command echo on/off*/
  HLP_SETLIST,	/*    SET LIST               -- toggle column or table display format*/
  HLP_SETNAMES,	/*    SET NAMES <csname>     -- set name of runtime character set*/
  HLP_SETPLAN,	/*    SET PLAN               -- toggle display of query access plan*/
  HLP_SETSQLDIALECT,	/* SET SQL DIALECT <n> -- set sql dialect to <n> */
  HLP_SETSTAT,	/*    SET STATs              -- toggle display of performance statistics*/
  HLP_SETTIME,	/*    SET TIME               -- toggle display of timestamp with DATE values*/
  HLP_SETTERM,	/*    SET TERM <string>      -- change statement terminator string*/
  HLP_SETWIDTH,	/*    SET WIDTH <col> [<n>]  -- set/unset print width to <n> for column <col>*/
  0,
  HLP_ALL,	/*All commands may be abbreviated to letters in CAPitals*/
 -1			/* end of list */
  };

if (!strcmp (what, "SET"))
    msgid = help_set_ids;
else
    msgid = help_ids;
for (; *msgid != -1; msgid++)
    {
    if (*msgid != 0)
	{
        ISQL_msg_get (*msgid, msg, NULL, NULL, NULL, NULL, NULL);
        ISQL_printf (Help, msg);
	}
    ISQL_printf (Help, NEWLINE);
    }
#endif
return (SKIP);
}

static SSHORT input (
    TEXT	*infile)
{
/**************************************
 *
 *	i n p u t
 *
 **************************************
 *
 * Functional description
 *	Read commands from the named input file
 *  
 *	Parameters:  infile -- Second word of the command line
 *		filelist is a stack of file pointers to
 *			return from nested inputs
 *
 *	The result of calling this is to point the
 *	global input file pointer, Ifp, to the named file. 
 *
 **************************************/
TEXT	*file, *errbuf;
IB_FILE	*fp; 
INDEV	current, flist;
TEXT    path[MAXPATHLEN];

errbuf = (TEXT *) ISQL_ALLOC (MSG_LENGTH);
if (!errbuf)
    return (ERR);

/* Local pointer to global list of input files */

flist = Filelist;

/* Set up editing command for shell */

file = infile;


/* If there is no file name specified, return error */

if (!file || !*file)
    {
    if (errbuf)
        ISQL_FREE (errbuf);
    return (ERR);
    }

file = path;
strip_quotes( infile, file );

/* filelist is a linked list of file pointers.  We must add a node to
** the linked list before discarding the current Ifp.  
**  Filelist is a global pointing to base of list.
*/

if (fp = ib_fopen (file, "r"))
    {
    current = (INDEV) ISQL_ALLOC ((SLONG) sizeof (struct indev));
    current->indev_fpointer = Ifp;
    current->indev_next = NULL;
#ifdef DEBUG_GDS_ALLOC
    gds_alloc_flag_unfreed ((void *) current);
#endif
    if (!Filelist)
	Filelist = current;
    else
	{
	while (flist->indev_next)
	    flist = flist->indev_next;
	flist->indev_next = current;
	}
    Ifp = fp;
    }
else 
    {
    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, FILE_OPEN_ERR, MSG_LENGTH, 
	errbuf, file, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    ISQL_FREE (errbuf);
    return FAIL;
    }

if (errbuf)
    ISQL_FREE (errbuf);
 
Input_file = TRUE;
return (SKIP);
}

static BOOLEAN isyesno (
    TEXT 	*buffer)
{
/**********************************************
 *
 *	i s y e s n o
 *
 **********************************************
 *
 * Functional description
 *	check if the first letter of the user's response
 *	corresponds to the first letter of Yes
 *	(in whatever language they are using)
 *
 *	returns TRUE for Yes, otherwise FALSE
 *
 **********************************************/

if (!have_trans)    /* get the translation if we don't have it already */
    {
    ISQL_msg_get (YES_ANS, yesword, NULL, NULL, NULL, NULL, NULL);
    have_trans = TRUE;
    }

/* Just check first byte of yes response -- could be multibyte problem */

if (UPPER7 (buffer[0]) == UPPER7 (yesword [0]))
    return TRUE; 
else
    return FALSE;
}

static SSHORT newdb (
    TEXT	*dbname,
    TEXT	*usr,
    TEXT	*psw,
    TEXT	*numbufs,
    TEXT	*sql_role_nm)
{
/**************************************
 *
 *	n e w d b
 *
 **************************************
 *
 * Functional description
 *	Change the current database from what it was.
 *	This procedure is called when we first enter this program.
 *  
 *	Parameters:	dbname      -- Name of database to open
 *			usr         -- user name, if given
 *			psw         -- password, if given
 *			numbufs     -- # of connection cache buffers, if given
 *			sql_role_nm -- sql role name
 *  
 **************************************/
UCHAR	*dpb, *dpb_buffer;
SCHAR   *save_database;
int	dpb_len;
SSHORT	l,i, cnt;
UCHAR	*p, *p1, *p2;
TEXT	end_quote;
BOOLEAN	delimited_done = FALSE;
TEXT	*local_psw, *local_usr, *local_name, *local_sql_role;
TEXT	local_numbufs [BUFFER_LENGTH128];
TEXT	*errbuf;

/* No name of a database, just return an error */

if (!dbname || !*dbname)
    return ERR; 

/* 
 * Since the dbname is set already, in the case where a database is specified
 * on the command line, we need to save it so ISQL_disconnect does not NULL it
 * out.  We will restore it after the disconnect.  The save_database buffer
 * will also be used to translate dbname to the proper character set.
*/
save_database = (SCHAR *) ISQL_ALLOC (strlen (dbname) + 1);
if (!save_database)
    return (ERR);

strcpy (save_database, dbname);
ISQL_disconnect_database (FALSE);
strcpy (dbname, save_database);

errbuf 		= (TEXT *) ISQL_ALLOC (MSG_LENGTH);
if (!errbuf)
    return ERR;
local_psw 	= (TEXT *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!local_psw)
    {
    ISQL_FREE (save_database);
    ISQL_FREE (errbuf);
    return ERR;
    }
local_usr 	= (TEXT *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!local_usr)
    {
    ISQL_FREE (save_database);
    ISQL_FREE (errbuf);
    ISQL_FREE (local_psw);
    return ERR;
    }
local_sql_role 	= (TEXT *) ISQL_ALLOC (BUFFER_LENGTH128);
if (!local_sql_role)
    {
    ISQL_FREE (save_database);
    ISQL_FREE (errbuf);
    ISQL_FREE (local_psw);
    ISQL_FREE (local_usr);
    return ERR;
    }
dpb_buffer	= (UCHAR *) ISQL_ALLOC (BUFFER_LENGTH256);
if (!dpb_buffer)
    {
    ISQL_FREE (save_database);
    ISQL_FREE (errbuf);
    ISQL_FREE (local_psw);
    ISQL_FREE (local_usr);
    ISQL_FREE (local_sql_role);
    return ERR;
    }

#ifdef DEBUG_GDS_ALLOC
gds_alloc_flag_unfreed ((void *) errbuf);
gds_alloc_flag_unfreed ((void *) local_psw);
gds_alloc_flag_unfreed ((void *) local_usr);
gds_alloc_flag_unfreed ((void *) local_sql_role);
gds_alloc_flag_unfreed ((void *) dpb_buffer);
#endif

/* global user and passwords are set only if they are not originally set */

local_psw [0] = 0;
local_usr [0] = 0;
local_sql_role [0] = 0;
local_numbufs [0] = '\0';

/* Strip quotes if well-intentioned */

strip_quotes (dbname, Db_name);
strip_quotes (usr, local_usr);
strip_quotes (psw, local_psw);
strip_quotes (numbufs, local_numbufs);

/* if local user is not specified, see if global options are
   specified - don't let a global role setting carry forward if a
   specific local user was specified  */

/* Special handling for SQL Roles:
 *     V5  client -- strip the quotes
 *     V6  client -- pass the role name as it is
 */

if (sql_role_nm)
    {
    if (SQL_dialect == SQL_DIALECT_V5)
	strip_quotes (sql_role_nm, local_sql_role);
    else
	strcpy (local_sql_role, sql_role_nm);
    }

if (!(strlen (local_sql_role)) && global_role)
    {
    if (SQL_dialect == SQL_DIALECT_V5)
        strip_quotes (Role, local_sql_role);
    else
        strcpy (local_sql_role, Role);
    }

if (!(strlen (local_usr)) && global_usr)
    strcpy (local_usr, User);

if (!(strlen (local_psw)) && global_psw)
    strcpy (local_psw, Password);

if (!(strlen (local_numbufs)) && global_numbufs)
    strcpy (local_numbufs, Numbufs);
    
/* Build up a dpb */

dpb = dpb_buffer;
*dpb++ = gds__dpb_version1;

if (*ISQL_charset && strcmp (ISQL_charset, DEFCHARSET))
    {
    *dpb++ = gds__dpb_lc_ctype;
    p = (UCHAR*) ISQL_charset;
    *dpb++ = strlen (p);
    while (*p)
	*dpb++ = *p++;
    }

if (l = strlen (local_usr))
    {
    *dpb++ = gds__dpb_user_name;
    *dpb++ = l;
    p = (UCHAR*) local_usr;
    while (*p)
	*dpb++ = *p++;
    }

if (l = strlen (local_psw))
    {
    *dpb++ = gds__dpb_password;
    *dpb++ = l;
    p = (UCHAR*) local_psw;
    while (*p)
	*dpb++ = *p++;
    }

if (l = strlen (local_sql_role))
    {
    STUFF (isc_dpb_sql_dialect);
    STUFF (4);
    STUFF_INT (SQL_dialect);
    *dpb++ = isc_dpb_sql_role_name;
    *dpb++ = l;
    p = (UCHAR*) local_sql_role;
    while (*p)
	*dpb++ = *p++;
    }

if (l = strlen (local_numbufs))
    {
    i = atoi (local_numbufs);
    STUFF (isc_dpb_num_buffers);
    STUFF (4);
    STUFF_INT (i);
    }

dpb_len = (dpb - dpb_buffer);
dpb = dpb_buffer;
if (dpb_len < 2)
    {
    dpb = NULL;
    dpb_len = 0;
    }

local_name = Db_name;

#if defined (WIN95) && !defined (GUI_TOOLS)
if (!fAnsiCP)
    {
    local_name = save_database;
    OemToAnsi(Db_name, local_name);
    }
#endif
if (isc_attach_database (isc_status, 0, local_name, &DB, dpb_len, (SCHAR*) dpb))
    {
    ISQL_errmsg (isc_status);
    Db_name[0] = '\0';
    if (save_database)
	ISQL_FREE (save_database);
    if (local_psw)
        ISQL_FREE (local_psw);
    if (local_usr)
        ISQL_FREE (local_usr);
    if (local_sql_role)
	ISQL_FREE (local_sql_role);
    if (dpb_buffer)
        ISQL_FREE (dpb_buffer);
    if (errbuf)
        ISQL_FREE (errbuf);
    return FAIL;
    }

ISQL_get_version (FALSE);

if (*local_sql_role)
    {
    switch (SQL_dialect)
	{
	case SQL_DIALECT_V5:
	    /* Uppercase the Sql Role name */
	    for (p1 = local_sql_role, p2 = local_sql_role;
		 *p1++ = UPPER7(*p2); p2++);
	    break;
	case SQL_DIALECT_V6_TRANSITION:
	case SQL_DIALECT_V6:
	    if (*local_sql_role == DBL_QUOTE || *local_sql_role == SINGLE_QUOTE)
		{
		p1 = p2 = local_sql_role;
		cnt = 1;
		delimited_done = FALSE;
		end_quote = *p1++;
		/*
		** remove the delimited quotes and escape quote from ROLE name
		*/
		while (*p1 && !delimited_done && cnt < BUFFER_LENGTH128-1)
		    {
		    if (*p1 == end_quote)
			{
			cnt++;
			*p2++ = *p1++;
			if (*p1 && *p1 == end_quote && cnt < BUFFER_LENGTH128-1)
			    *p1++; /* skip the escape quote here */
			else
			    {
			    delimited_done = TRUE;
			    p2--;
			    }
			}
		    else
			{
			cnt++;
			*p2++ = *p1++;
			}
		    }
		*p2 = '\0';
		}
	    else
		{
		for (p1 = local_sql_role, p2 = local_sql_role;
		     *p1++ = UPPER7(*p2); p2++);
		}
	    break;
	default:
	    break;
	}
    }

#ifndef GUI_TOOLS
if (local_usr [0] != '\0')
    {
    if (local_sql_role [0] != '\0')
        {
        sprintf (Print_buffer, "Database:  %s, User: %s, Role: %s%s", 
            dbname, 
            local_usr,
            local_sql_role,
            NEWLINE);
        }
    else
        {
        sprintf (Print_buffer, "Database:  %s, User: %s%s", 
            dbname, 
            local_usr,
            NEWLINE);
        }
    ISQL_printf (Out, Print_buffer);
    }
else
    {
    if (local_sql_role [0] != '\0')
        {
        sprintf (Print_buffer, "Database:  %s, Role:  %s%s", 
                 dbname, local_sql_role, NEWLINE);
        }
    else
        {
        sprintf (Print_buffer, "Database:  %s%s", dbname, NEWLINE);
        }
    ISQL_printf (Out, Print_buffer);
    }
#endif

if (!V4 && !V33)
    {
    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, SERVER_TOO_OLD, MSG_LENGTH,
		     errbuf, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    STDERROUT (errbuf, 1);
    isc_detach_database (isc_status, &DB);
    DB = 0;
    Db_name[0] = '\0';
    Stmt = 0;
    if (save_database)
	ISQL_FREE (save_database);
    if (local_psw)
        ISQL_FREE (local_psw);
    if (local_usr)
        ISQL_FREE (local_usr);
    if (dpb_buffer)
        ISQL_FREE (dpb_buffer);
    if (errbuf)
        ISQL_FREE (errbuf);
    return FAIL;
    }

/* Start the user transaction  and default transaction */

if (!M__trans)
    {
    if (isc_start_transaction (isc_status, &M__trans, 1, &DB, 0, (SCHAR*) 0))
	ISQL_errmsg (isc_status);
    if (D__trans)
	COMMIT_TRANS (&D__trans);
    if (isc_start_transaction (isc_status, &D__trans, 1, &DB,
		(V4) ? 5           : 0, 
		(V4) ? default_tpb : NULL))
	ISQL_errmsg (isc_status);
    }


/* Allocate a new user statement for ths database */

Stmt = 0;
if (isc_dsql_allocate_statement (isc_status, &DB, &Stmt))
    {
    ISQL_errmsg (isc_status);
    if (M__trans)
	end_trans();
    if (D__trans)
	COMMIT_TRANS (&D__trans);
    isc_detach_database (isc_status, &DB);
    DB = 0;
    Db_name[0] = '\0';
    M__trans = NULL;
    D__trans = NULL;
    if (save_database)
	ISQL_FREE (save_database);
    if (local_psw)
        ISQL_FREE (local_psw);
    if (local_usr)
        ISQL_FREE (local_usr);
    if (dpb_buffer)
        ISQL_FREE (dpb_buffer);
    if (errbuf)
        ISQL_FREE (errbuf);
    return FAIL;
    }

if (save_database)
    ISQL_FREE (save_database);
if (local_psw)
    ISQL_FREE (local_psw);
if (local_usr)
    ISQL_FREE (local_usr);
if (local_sql_role)
    ISQL_FREE (local_sql_role);
if (dpb_buffer)
    ISQL_FREE (dpb_buffer);
if (errbuf)
    ISQL_FREE (errbuf);
return (SKIP);
}

static SSHORT newoutput (
    TEXT	*outfile)
{
/**************************************
 *
 *	n e w o u t p u t
 *
 **************************************
 *
 * Functional description
 *	Change the current output file 
 *
 *	Parameters:  outfile : Name of file to receive query output
 *
 **************************************/
IB_FILE	*fp;
TEXT	*errbuf;
int	ret;
TEXT    path[MAXPATHLEN];

ret = SKIP;
/* If there is a file name, attempt to open it for append */

if (*outfile)
    {
       strip_quotes( outfile, path );
       outfile = path;
#ifdef GUI_TOOLS
    if (fp = ib_fopen (outfile, "w"))
#else		
    if (fp = ib_fopen (outfile, "a"))
#endif
	{
	if (Out && Out != ib_stdout)
	    ib_fclose (Out);
	Out = fp;
#ifndef GUI_TOOLS
	if (Merge_stderr)
	    Errfp = Out;
#else
	if (Merge_stderr)
	    {
	    Errfp = Out;
	    Diag  = Out;
	    Help  = Out;
	    }
#endif
	}
    else
	{
	errbuf = (TEXT *) ISQL_ALLOC (MSG_LENGTH);
	if (errbuf)
	    {
	    gds__msg_format (NULL_PTR, 
			     ISQL_MSG_FAC, 
			     FILE_OPEN_ERR, 
			     MSG_LENGTH, 
			     errbuf, 
			     outfile, 
			     NULL_PTR, 
			     NULL_PTR, 
			     NULL_PTR, 
			     NULL_PTR);
	    STDERROUT (errbuf, 1);
	    ISQL_FREE (errbuf);
	    }
	ret = FAIL;
	}
    }
else
    {
#ifndef GUI_TOOLS
    /* Revert to ib_stdout */
    if (Out != ib_stdout)
         {
         ib_fclose (Out);
         Out = ib_stdout;
         if (Merge_stderr)
             Errfp = Out;
         }
#else
    if (SaveOutfileName)
         {
         if (fp = ib_fopen (SaveOutfileName, "a"))
             {
             ib_fclose (Out);   
             Out = fp;
             if (Merge_stderr)
                {
                Errfp = Out;
                Diag  = Out;
		Help  = Out;
                }
             }
         else
             {
             errbuf = (TEXT *) ISQL_ALLOC (MSG_LENGTH);
             if (errbuf)
                {
                gds__msg_format (NULL_PTR,
                                ISQL_MSG_FAC,
                                FILE_OPEN_ERR,
                                MSG_LENGTH,
                                errbuf,
                                SaveOutfileName,
                                NULL_PTR,
                                NULL_PTR,
                                NULL_PTR,
                                NULL_PTR);
                STDERROUT (errbuf, 1);
                ISQL_FREE (errbuf);
                }
             ret = FAIL;
             }
         }
#endif
     }
return (ret); 
}

static SSHORT newtrans (
    TEXT	*statement)
{
/**************************************
 *
 *	n e w t r a n s
 *
 **************************************
 *
 * Functional description
 *	Intercept and handle a set transaction statement by zeroing M__trans
 *  
 *	Leave the default transaction, gds__trans, alone
 *	Parameters:  The statement in its entirety
 *
 **************************************/

if (!ISQL_dbcheck())
    return FAIL;

if (end_trans())
    return FAIL;

M__trans = NULL;

/* M__trans = 0 after the commit or rollback. ready for a new transaction */

if (isc_dsql_execute_immediate (isc_status, &DB, &M__trans, 0, statement, 
				SQL_dialect, (XSQLDA*) 0))
    {
    ISQL_errmsg (isc_status);
    return FAIL;
    }

return SKIP;
}

static SSHORT parse_arg (
    int		argc,
    SCHAR	**argv,
    SCHAR	*tabname,
    IB_FILE	**sess)
{
/**************************************
 *
 *	p a r s e _ a r g
 *
 **************************************
 *
 * Functional description
 *	Parse command line flags 
 *	All flags except database are -X.  Look for
 *	the - and make it so.
 *
 **************************************/
int 	c, lgth, ret = SKIP;
SCHAR 	*s, switchchar;
TEXT 	*errbuf;
SSHORT 	i;
#ifdef	DEV_BUILD
BOOLEAN	istable = FALSE;
#endif

errbuf = (TEXT *) ISQL_ALLOC (MSG_LENGTH);
if (!errbuf)
    return (ERR);

switchchar = '-';

/* Initialize database name */

*Db_name 	= '\0';
*Target_db 	= '\0';
Password [0] 	= '\0';
User [0] 	= '\0';
Role [0] 	= '\0';
Numbufs[0]= 0;
Quiet 		= FALSE;
Exit_value 	= FINI_OK;

    /* 
    ** default behavior in V6.0 is SQL_DIALECT_V6 
    */

requested_SQL_dialect 	= SQL_DIALECT_V6;

#ifdef GUI_TOOLS
Merge_stderr 	= TRUE;
#else
Merge_stderr 	= FALSE;
#endif

#if defined (WIN95) && !defined (GUI_TOOLS)
#define STRARGCPY(dest, src) fAnsiCP ? strcpy(dest, src) : AnsiToOem(src, dest)
#define STRARGNCPY(dest, src, len) fAnsiCP ? strncpy(dest, src, len) : AnsiToOemBuff(src, dest, len)
#else
#define STRARGCPY	strcpy
#define STRARGNCPY	strncpy
#endif

lgth = strlen (DEFCHARSET);
if (lgth <= MAXCHARSET_LENGTH) 
    strcpy (ISQL_charset, DEFCHARSET);
else
    strncpy (ISQL_charset, DEFCHARSET, MAXCHARSET_LENGTH);

/* redirected ib_stdin means act like -i was set */

Ifp = ib_stdin;

/* Terminators  are initialized */

Termlen 	= strlen (DEFTERM);
if (Termlen <= MAXTERM_LENGTH)
    strcpy (Term, DEFTERM);
else
   {
   Termlen = MAXTERM_LENGTH;
   strncpy (Term, DEFTERM, MAXTERM_LENGTH);
   }

/* Initialize list of input file pointers */

Filelist = 0;

/* Interpret each command line argument */

i = 1;
s = argv [i];
while (i < argc)
    {
    /* Look at flags to find unique match.  If the flag has an arg,
    ** advance the pointer (and i).  Only change the return value
    ** for extract switch or error.
    */
    if (*s == switchchar)
	{
	c = UPPER (s [1]);
	if (c == 'X' || (c == 'E' && UPPER (s [2]) == 'X'))
	    {
#ifdef DEV_BUILD
	    if (UPPER (s [2]) == 'T')
		istable = TRUE; 
#endif
	    ret = EXTRACT;
	    }
	else if (c == 'A')
	    ret = EXTRACTALL;
	else if (c == 'E')
	    Echo = TRUE;
	else if (c == 'M')
	    Merge_stderr = TRUE;
	else if (c == 'N' && UPPER (s [2]) == 'O' && UPPER (s [3]) == 'W')
	    Warnings = FALSE;
	else if (c == 'N' &&
		 (!s[2] || (UPPER (s[2]) == 'O' && !s[3]) ||
		  (UPPER (s[2]) == 'O' && UPPER (s[3]) == 'A')) )
	    Autocommit = FALSE;
	else if (c == 'O')
	    {	
	    /* Alternate output file */

	    if ((s = argv [++i]) && (newoutput (s) != FAIL))
#ifdef GUI_TOOLS
                {
                /* Save original output file for WINDOWS ISQL */
                SaveOutfileName = NULL;
                SaveOutfileName = (TEXT *) ISQL_ALLOC (strlen(s) + 1);
                STRARGCPY (SaveOutfileName, s);
                }
#else
                ;
#endif
	    else 
		ret = ERR;
	    }
	else if (c == 'I')
	    {
	    /* Alternate input file */

	    if ((s = argv [++i]) && (input (s) != FAIL))
		;
	    else
		ret = ERR;
	    Interactive = FALSE;
	    Input_file = TRUE;
	    }
	else if (c == 'T')
	    {
	    /* The next string is the proposed terminator */

	    if ((s = argv [++i]) && *s)
		{
		Termlen = strlen (s);
		if (Termlen <= MAXTERM_LENGTH)
    		    STRARGCPY (Term, s);
		else
   		    {
   		    Termlen = MAXTERM_LENGTH;
   		    STRARGNCPY (Term, s, MAXTERM_LENGTH);
   		    }
		}
	    else
		ret = ERR;
	    }
	else if (c == 'D')
	    {
	    /* advance to the next word for target db name */

	    if ((s = argv [++i]) && *s)
		STRARGCPY (Target_db, s);
	    else 
		ret = ERR;
	    }
	else if (c == 'P' &&
	    UPPER (s [2]) == 'A' &&
	    UPPER (s [3]) == 'G' )
	    {
	    /* Change the page length */

	    if ((s = argv [++i]) && (Pagelength = atoi (s)))
		;
	    else
		ret = ERR;
	    }
	/* This has to be after "PAGE" */
	else if (c == 'P')
	    {
	    if ((s = argv [++i]) && *s)
		{
		STRARGCPY (Password, s);
		global_psw = TRUE;
		}
	    else
		ret = ERR;
	    }
	else if (c == 'U')
	    {
	    if ((s = argv [++i]) && *s)
		{
		STRARGCPY (User, s);
		global_usr = TRUE;
		}
	    else
		ret = ERR;
	    }
	else if (c == 'R')
	    {
	    if ((s = argv [++i]) && *s)
		{
		STRARGCPY (Role, s);
		global_role = TRUE;
		}
	    else
		ret = ERR;
	    }
	else if (c == 'C')  /* number of cache buffers */
	{
  		if ((s = argv [++i]) && *s)
                {
			STRARGCPY(Numbufs,s);
			global_numbufs = TRUE;
		}
		else
			ret = ERR;
	}

#ifdef GUI_TOOLS
	/* Get the WISQL specific session file */
	else if (c == 'S')
	    {
	command line option '-s' is reserved for '-s[qldialect] n'  Bie Tie
	    /* Set up WISQL session file */
	    if (s = argv [++i])
		*sess = (IB_FILE *) s;
	    else
		ret = ERR;
	    }
#endif
	else if (c == 'Q')
	    /* Quiet flag */
	    Quiet = TRUE;
	else if (c == 'Z')
	    {
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, VERSION, MSG_LENGTH, 
		errbuf,	GDS_VERSION, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);

	    sprintf (Print_buffer, "%s%s", errbuf, NEWLINE);
	    ISQL_printf (Out, Print_buffer);
	    }
	else if (c == 'S')
	    {
	    /* SQL_dialect value */
	    if ((s = argv [++i]) && (requested_SQL_dialect = atoi (s)))
		{
		if (requested_SQL_dialect < SQL_DIALECT_V5 || 
		    requested_SQL_dialect > SQL_DIALECT_CURRENT)
		    ret = ERR;
		else
		    {
		    /* requested_SQL_dialect is used to specify the database dialect
		     * if SQL dialect was not specified.  Since it is possible to
		     * have a client dialect of 2, force the database dialect to 3, but
		     * leave the client dialect as 2
		     */
		    SQL_dialect = requested_SQL_dialect;
		    if (requested_SQL_dialect == SQL_DIALECT_V6_TRANSITION)
		        {
	    	        Merge_stderr = TRUE;
		        requested_SQL_dialect = SQL_DIALECT_V6;
		        }
		    }
		}
	    else
		ret = ERR;
	    }
	else	/* This is an unknown flag */
	    {
	    gds__msg_format (NULL_PTR, ISQL_MSG_FAC, SWITCH, MSG_LENGTH, 
		errbuf, s + 1, NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
	    STDERROUT (errbuf, 1);
	    ret =  ERR;
	    } 
	}
	else 
	    /* This is not a switch, it is a db_name */
	    {
	    STRARGCPY (Db_name, s);
#ifdef DEV_BUILD
	    /* If there is a table name, it follows */
	    if (istable && (s = argv [++i]) && *s)
		STRARGCPY (tabname, s);
#endif
	    }
	

    /* Quit the loop of interpreting if we got an error */

    if (ret == ERR)
	break;
    s = argv[++i];
    }

/* If not input, then set up first filelist */

if (Ifp == ib_stdin)
    {
    Filelist = (INDEV) ISQL_ALLOC ((SLONG) sizeof (struct indev));
    Filelist->indev_fpointer = Ifp;
    Filelist->indev_next = NULL;
#ifdef DEBUG_GDS_ALLOC
    gds_alloc_flag_unfreed ((void *) Filelist);
#endif
    }
if (errbuf)
    ISQL_FREE (errbuf);

return (ret);
#undef STRARGCPY
#undef STRARGNCPY
}

static SSHORT print_item (
    TEXT	**s,
    XSQLVAR	*var,
    SLONG	printlength)
{
/**************************************
 *
 *	p r i n t _ i t e m
 *
 **************************************
 *
 * Functional description
 *	Print an SQL data item as described.
 *
 **************************************/
TEXT		*p, *string, *t;
TEXT		d[32];
VARY		*vary;
SSHORT		dtype;
SSHORT		dscale;
struct tm	times;
SLONG		length; 
double		numeric, exponent;
ISC_QUAD	blobid;
TEXT		blobbuf [30];

p = *s;
*p = '\0';

dtype = var->sqltype & ~1;
dscale = var->sqlscale;

/* The printlength should have in it the right length */

length = printlength;

if (List)
    {
    sprintf (Print_buffer, "%-31s ", ISQL_blankterm (var->aliasname));
    ISQL_printf (Out, Print_buffer);
    }

if ((var->sqltype & 1) && (*var->sqlind < 0))
    {
    /* If field was missing print <null> */

    if (List)
	{ 
	sprintf (Print_buffer, "<null>%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}

    if ((dtype == SQL_TEXT) || (dtype == SQL_VARYING))
	sprintf (p, "%-*s ", (int) length, "<null>");
    else
	sprintf (p, "%*s ", (int) length, "<null>");
    }
else if (!strncmp (var->sqlname, "DB_KEY", 6))
    {
    /* Special handling for db_keys  printed in hex */

    /* Keep a temp buf, d for building the binary string */

    for (t = var->sqldata; t < var->sqldata + var->sqllen; t += 4)
	{
	if (List)
	    {
	    sprintf (Print_buffer,"%.8X", *(SLONG*) t);
	    ISQL_printf (Out, Print_buffer);
	    }
	else
	    {
	    sprintf (d, "%.8X", *(SLONG*) t);
	    strcat (p, d);
	    }
	}
    if (List)
	ISQL_printf (Out, NEWLINE);
    else
	strcat (p, " ");
    }
else
    {
    switch (dtype)
	{
	case SQL_ARRAY :

	    /* Print blob-ids only here */

	    blobid = *(ISC_QUAD*) var->sqldata;
	    sprintf (blobbuf, "%x:%x", blobid.gds_quad_high, blobid.gds_quad_low);
	    sprintf (p, "%17s ", blobbuf);
	    break;
	case SQL_BLOB :

	    /* Print blob-ids only here */

	    blobid = *(ISC_QUAD*) var->sqldata;
	    sprintf (blobbuf, "%x:%x", blobid.gds_quad_high, blobid.gds_quad_low);
	    sprintf (p, "%17s ", blobbuf);
	    if (List)
		{
		ISQL_printf (Out, blobbuf);
		ISQL_printf (Out, NEWLINE);
		dtype = print_item_blob (Out, var, M__trans);
		ISQL_printf (Out, NEWLINE);
		}
	    break;

	case SQL_SHORT :
	case SQL_LONG:
	case SQL_INT64:
            {
	    TEXT  str_buf[30];
	    SINT64  value;

	    if (dtype == SQL_SHORT)
	        value = (SINT64) (*(SSHORT*) (var->sqldata));
	    else if (dtype == SQL_LONG)
	        value = (SINT64) (*(SLONG*) (var->sqldata));
	    else if (dtype == SQL_INT64)
	        value = *(SINT64*) (var->sqldata);

	    print_item_numeric (value, length, dscale, str_buf);
	    sprintf (p, "%s ", str_buf);
	    if (List)
	      {
		/* Added 1999-03-23 to left-justify in LIST ON mode */
		{
		  TEXT *src = str_buf, *dest = str_buf;
		  while (' ' == *src)
		      src++;
		  /* This is pretty much the same thing as a strcpy,
		     but strcpy is not guaranteed to do the right thing
		     when the source and destination overlap. :-( */
		  if (src != dest)
		      while (0 != (*dest++ = *src++))
			  ;	/* empty */
		}
		sprintf (Print_buffer,"%s%s", str_buf, NEWLINE);
		ISQL_printf (Out, Print_buffer);
	      }
	    break;
            }

#ifdef NATIVE_QUAD
	case SQL_QUAD:
	    if (dscale)
		{
		/* Handle floating scale and precision */

		numeric = *(SQUAD*) (var->sqldata);
		exponent = -dscale;
		numeric = numeric / pow (10.0, exponent);
		sprintf (p, "%*.*f ", (int) length, (int) -dscale,  numeric);
		if (List)
		    {
		    sprintf (Print_buffer,"%.*f%s", 
			     (int) -dscale, 
			     numeric,
			     NEWLINE);
		    ISQL_printf (Out, Print_buffer);
		    }
		}
	    else
		{
		sprintf (p, "%*ld ", (int) length, *(SQUAD*) (var->sqldata));
		if (List)
		    {
		    sprintf (Print_buffer, "%ld%s", 
			     *(SQUAD*) var->sqldata,
			     NEWLINE);
		    ISQL_printf (Out, Print_buffer);
		    }
		}
	    break;
#endif

	case SQL_FLOAT:
	    sprintf (p, "% #*.*g ",
		     (int) length,
		     (int) MIN(8, (length - 6)),
		     (double) *(float*) (var->sqldata));
	    if (List)
		{
		sprintf (Print_buffer, "%.*g%s", 
			 FLOAT_LEN - 6,
			 *(float*) (var->sqldata),
			 NEWLINE);
		ISQL_printf (Out, Print_buffer);
		}
	    break;

	case SQL_DOUBLE:
	    /* Don't let numeric/decimal doubles overflow print length */
	    /* Special handling for 0 -- don't test log for length */
	    if ((!*var->sqldata && dscale) || (dscale && log10(fabs(*(double*) (var->sqldata))) < length + dscale))
		{
		sprintf (p, "%*.*f ", (int) length, (int) -dscale, *(double*) (var->sqldata));
		if (List)
		    {
		    sprintf (Print_buffer, "%.*f%s", 
			     (int) -dscale, 
			     *(double*) (var->sqldata),
			     NEWLINE);
		    ISQL_printf (Out, Print_buffer);
		    }
		}
	    else
		{
		sprintf (p, "% #*.*g ", (int) length,
			 (int) MIN(16, (int) (length - 7)),
			 *(double*) (var->sqldata));
		if (List)
		    {
		    sprintf (Print_buffer, "%#.*g%s", 
			     DOUBLE_LEN - 7,
			     *(double*) (var->sqldata),
			     NEWLINE);
		    ISQL_printf (Out, Print_buffer);
		    }
		}
	    break;

	case SQL_TEXT:
	    string = var->sqldata;
	    string [var->sqllen] = '\0';
	    /* See if it is character set OCTETS */
	    if (var->sqlsubtype == 1)
		{
		USHORT	i;
		TEXT	*buff2 = ISQL_ALLOC (2*var->sqllen+1);
		/* Convert the string to hex digits */
		for (i = 0; i < var->sqllen; i++)
		    {
		    sprintf (&buff2[i*2], "%02X", string [i]);
		    }
		buff2 [(2*var->sqllen+1)-1] = 0;
		if (List)
		    {
		    ISQL_printf (Out, buff2);
		    ISQL_printf (Out, NEWLINE);
		    }
		else
		    sprintf (p, "%s ", buff2);
		ISQL_FREE (buff2);
		}
	    else if (List)
		{
		ISQL_printf (Out, string);
		ISQL_printf (Out, NEWLINE);
		}
	    else
		{
		/* Truncate if necessary */
		if (length < var->sqllen)
		    string [length] = '\0';
		sprintf (p, "%-*s ", (int) length, var->sqldata);
		}
	    break;

	case SQL_TIMESTAMP:
	    isc_decode_date (var->sqldata, &times);
	   
	    if (SQL_dialect > SQL_DIALECT_V5)
	        {
	 	sprintf (d, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d.%4.4d", times.tm_year+1900,
		         (times.tm_mon+1), times.tm_mday,
		         times.tm_hour, times.tm_min, times.tm_sec,
			 ((ULONG *) var->sqldata) [1] % ISC_TIME_SECONDS_PRECISION);
	        }
	    else
	        {
	        if (Time_display)
	 	    sprintf (d, "%2d-%s-%4d %2.2d:%2.2d:%2.2d.%4.4d", times.tm_mday,
		  	    alpha_months[times.tm_mon], times.tm_year + 1900,
			    times.tm_hour, times.tm_min, times.tm_sec,
			    ((ULONG *) var->sqldata) [1] % ISC_TIME_SECONDS_PRECISION);
	        else
	            sprintf (d, "%2d-%s-%4d", times.tm_mday,
			alpha_months[times.tm_mon], times.tm_year + 1900);
		}

	    sprintf (p, "%-*s ", (int) length, d);
	    if (List)
		{
		sprintf (Print_buffer, "%s%s", d, NEWLINE);
		ISQL_printf (Out, Print_buffer);
		}
	    break;

	case SQL_TYPE_TIME:
	    {
	    /* Temporarily convert the Time to a Time & Date in order
	       to use isc_decode_date () */

	    ULONG	time_and_date [2];
	    time_and_date [0] = 0;
	    time_and_date [1] = *(ULONG *) var->sqldata;
	    isc_decode_date (time_and_date, &times);
	    sprintf (d, "%2.2d:%2.2d:%2.2d.%4.4d", 
			times.tm_hour, times.tm_min, times.tm_sec,
			(*(ULONG *) var->sqldata) % ISC_TIME_SECONDS_PRECISION);
	    sprintf (p, "%-*s ", (int) length, d);
	    if (List)
		{
		sprintf (Print_buffer, "%s%s", d, NEWLINE);
		ISQL_printf (Out, Print_buffer);
		}
	    }
	    break;

	case SQL_TYPE_DATE:
	    {
	    /* Temporarily convert the Time to a Time & Date in order
	       to use isc_decode_date () */

	    ULONG	time_and_date [2];
	    time_and_date [0] = *(ULONG *) var->sqldata;
	    time_and_date [1] = 0;
	    isc_decode_date (time_and_date, &times);

	    sprintf (d, "%4.4d-%2.2d-%2.2d", times.tm_year+1900,
		    (times.tm_mon+1), times.tm_mday);
			
	    sprintf (p, "%-*s ", (int) length, d);
	    if (List)
		{
		sprintf (Print_buffer, "%s%s", d, NEWLINE);
		ISQL_printf (Out, Print_buffer);
		}
	    }
	    break;

	case SQL_VARYING:
	    vary = (VARY*) var->sqldata;

	    /* Note: we allocated the dataspace 1 byte larger already */
	    vary->vary_string [vary->vary_length] = 0;

	    if (List)
		{
		ISQL_printf (Out, vary->vary_string);
		ISQL_printf (Out, NEWLINE);
		}
	    else
		{
		/* Truncate if necessary */
		if (length < vary->vary_length)
		    vary->vary_length = length;
		sprintf (p, "%-*.*s ", (int) length, (int) vary->vary_length, 
			vary->vary_string);
		}
	    break;

	default:
	    sprintf (d, "Unknown type: %d", dtype);
	    sprintf (p, "%-*s ", (int) length, d);
	    if (List)
		{
		sprintf (Print_buffer, "%s%s", d, NEWLINE);
		ISQL_printf (Out, Print_buffer);
		}
	    break;
	}
    }

while (*p)
    ++p;

*s = p;

return (dtype);
}

static int print_item_blob (
    IB_FILE	*fp,
    XSQLVAR	*var,
    void	*trans)
{
/**************************************
 *
 *	p r i n t _ i t e m _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Print a User Selected BLOB field (as oppposed to
 *	any BLOB selected in a show or extract command)
 *
 **************************************/
void		*blob;
USHORT		length, bpb_length;
UCHAR		*buffer, *b;
UCHAR		bpb_buffer [64];
TEXT		*msg;
UCHAR 		*bpb;
ISC_BLOB_DESC	from_desc, to_desc;
ISC_QUAD	*blobid;

buffer 	= (UCHAR *) ISQL_ALLOC ((SLONG) BUFFER_LENGTH512);
if (!buffer)
    return ERR;
msg 	= (TEXT *) ISQL_ALLOC ((SLONG) MSG_LENGTH);
if (!msg)
    {
    ISQL_FREE (buffer);
    return ERR;
    }

bpb_length 	= 0;
bpb 	= NULL;
blobid 	= (ISC_QUAD*) var->sqldata;

/* Don't bother with null blobs */

if (blobid->gds_quad_high == 0 && blobid->gds_quad_low == 0)
    return CONT;

if ((var->sqlsubtype != Doblob) && (Doblob != ALL_BLOBS))
    {
    ISQL_msg_get (BLOB_SUBTYPE, msg, (TEXT*) Doblob, (TEXT*) var->sqlsubtype, NULL, NULL, NULL);
	/* Blob display set to subtype %d. This blob: subtype = %d\n */
    ISQL_printf (fp, msg);
    if (buffer)
        ISQL_FREE (buffer);
    if (msg)
        ISQL_FREE (msg);
    return CONT;
    }

switch (var->sqlsubtype)
    {
    case BLOB_text:
	/* Lookup the remaining descriptor information for the BLOB field,
	 * most specifically we're interested in the Character Set so
	 * we can set up a BPB that requests character set transliteration
	 */
	if (!isc_blob_lookup_desc (isc_status, &DB, &trans, var->relname, var->sqlname, &from_desc, NULL))
	    {
	    isc_blob_default_desc (&to_desc, var->relname, var->sqlname);
	    if (!isc_blob_gen_bpb (isc_status, &to_desc, &from_desc, sizeof (bpb_buffer), 
			           bpb_buffer, &bpb_length))
		bpb = bpb_buffer;
	    }
	break;
    case BLOB_blr:
	bpb = blr_bpb;
	bpb_length = sizeof (blr_bpb);
	break;
    case BLOB_acl:
	bpb = acl_bpb;
	bpb_length = sizeof (acl_bpb);
	break;
    default:
	break;
    }

blob = NULL;
if (isc_open_blob2 (isc_status, &DB, &trans, &blob, blobid, bpb_length,
	bpb))
    {
    ISQL_errmsg (isc_status);
    if (buffer)
        ISQL_FREE (buffer);
    if (msg)
        ISQL_FREE (msg);
    return ERR;
    }

while (!isc_get_segment (isc_status, &blob, &length,
	(USHORT) (BUFFER_LENGTH512 - 1),
	buffer) || isc_status [1] == isc_segment)
    {
    /* Special displays for blr or acl subtypes */

    if (var->sqlsubtype == BLOB_blr || var->sqlsubtype == BLOB_acl)
	{
	buffer [length--] = 0;
	for (b = buffer + length; b >= buffer;)
	    {
	    if (*b == '\n' || *b == '\t' || *b == BLANK)
		*b-- = 0;
	    else
		break;
	    }
	sprintf (Print_buffer, "%s    %s%s", TAB, buffer, NEWLINE);
	ISQL_printf (fp, Print_buffer);
	}
    else
	{
	buffer [length] = 0;
	ISQL_printf (fp, buffer);
	}
    }

if (isc_status [1] == isc_segstr_eof)
    isc_close_blob (isc_status, &blob);
else if (isc_status [1])
    {
    ISQL_errmsg (isc_status);
    if (buffer)
        ISQL_FREE (buffer);
    if (msg)
        ISQL_FREE (msg);
    return ERR;
    }
if (buffer)
    ISQL_FREE (buffer);
if (msg)
    ISQL_FREE (msg);
return CONT;
}

static void print_item_numeric (
    SINT64	value,
    SSHORT	length,
    SSHORT	scale,
    TEXT	*buf)
{
/**************************************
 *
 *	p r i n t _ i t e m _ n u m e r i c
 *
 **************************************
 *
 * Functional description
 *	Print a INT64 value into a buffer by accomodating
 *	decimal point '.' for scale notation
 *
 **************************************/
SSHORT	from, to;
BOOLEAN	all_digits_used=FALSE, neg = (value < 0) ? TRUE : FALSE;
        
/* Handle special case of no negative scale, no '.' required! */
if (scale>=0)
    {
    if (scale > 0)
	value *= (SINT64) pow (10.0, (double) scale);
    sprintf (buf, "%*" QUADFORMAT "d", length, value);
    return;
    }

/* Use one less space than allowed, to leave room for '.' */
length--;
sprintf (buf, "%*" QUADFORMAT "d", length, value);

/* start from LSByte towards MSByte */
from = length-1;
to = length;

/* make space for decimal '.' point in buffer */
buf[to+1] = '\0';
for (; from>=0 && DIGIT(buf[from]) && scale; from--, to--, ++scale)
    buf[to] = buf[from];

/* Check whether we need a '0' (zero) in front of the '.' point */
if (!DIGIT(buf[from]))
    all_digits_used=TRUE;

/* Insert new '0's to satisfy larger scale than input digits 
 * For e.g: 12345 with scale -7 would be .0012345
 */
if (from > 0 && scale) 
	do {
		buf[to--] = '0';
	} while (++scale != 0);

/* Insert '.' decimal point, and if required, '-' and '0' */
buf[to--] = '.';
if (all_digits_used==TRUE) 
    {
    buf[to--] = '0';
    if (neg==TRUE) 
	buf[to--] = '-';
    }
}

static int print_line (
    XSQLDA	*sqlda,
    SLONG	*pad,
    TEXT	*line)
{
/**************************************
 *
 *	p r i n t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Print a line of SQL variables.
 *
 *	Args:  sqlda, an XSQLDA
 *	    pad = an array of the print lengths of all the columns
 *	    line = pointer to the line buffer.
 *
 **************************************/
TEXT	*p; 
XSQLVAR	*var, *end;
XSQLVAR	*varlist [20];
SSHORT	varnum, i, ret = CONT;

p = line;

varnum = -1;
for (i = 0, var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++, i++)
    {
    if (!Abort_flag)
	/* Save all the blob vars and print them at the end */
	if ((ret = print_item (&p, var, pad[i])) == SQL_BLOB)
	    {   
	    varnum++;
	    varlist[varnum] = var;
	    }
    }

*p = 0;

if (List)
    {
    ISQL_printf (Out, NEWLINE);
    return (CONT);
    }

ISQL_printf (Out, line);
ISQL_printf (Out, NEWLINE);

/* If blobdisplay is not wanted, set varnum back to -1 */

if (Doblob == NO_BLOBS)
    varnum = -1;

/* If there were Blobs to print, print them passing the blobid */

for (i = 0; i <= varnum; i++)
    {
    var = varlist [i];
    if (*var->sqlind == 0)
	{
	/* Print blob title */

	sprintf (Print_buffer, "==============================================================================%s", NEWLINE);
	ISQL_printf (Out, Print_buffer);
	sprintf (Print_buffer, "%s:  %s", var->aliasname, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	if (print_item_blob (Out, var, M__trans))
	    return (ERR);
	sprintf (Print_buffer, "%s==============================================================================%s", NEWLINE, NEWLINE);
	ISQL_printf (Out, Print_buffer);
	}
    }
return (CONT);
}

static int process_statement (
    TEXT	*string,
    XSQLDA	**sqldap)
{
/**************************************
 *
 *	p r o c e s s _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Prepare and execute a dynamic SQL statement.
 *	This function uses the isc_dsql user functions rather
 *	than embedded dynamic statements.  The user request
 *	is placed on transaction M__trans, while all
 *	background work is on the default gds__trans.
 *	THis function now returns CONT (success) or ERR
 **************************************/
SLONG	*buffer = NULL;
USHORT	length, data_length; 
SSHORT	alignment, type, namelength, i, j, n_cols; 
SSHORT	statement_type = 0, l;
SSHORT	*nullind = NULL, *nullp;
SLONG	lines, offset;
SLONG	*pad = NULL;
TEXT	*line = NULL, *header, *header2, *p, *p2;
COLLIST	*pc;
SLONG	bufsize = 0;
SLONG	linelength = 0;
SCHAR	*plan_buffer;
SCHAR	info_buffer[16];
SCHAR	count_buffer[33];
UCHAR	count_is, count_type;
SLONG	count;
XSQLDA	*sqlda;
XSQLVAR	*var, *end;
void	*prepare_trans;
int	ret = CONT;
static SCHAR	sqlda_info [] = { isc_info_sql_stmt_type };
static SCHAR	plan_info [] = { isc_info_sql_get_plan };
static SCHAR	count_info [] = { isc_info_sql_records };

/* We received the address of the sqlda in case we realloc it */
sqlda = *sqldap;

/* If somebody did a commit or rollback, we are out of a transaction */

if (!M__trans)
    if (isc_start_transaction (isc_status, &M__trans, 1, &DB, 0, (SCHAR*) 0))
	ISQL_errmsg (isc_status);

/* No need to start a default transaction unless there is no current one */

if (!D__trans)
    if (isc_start_transaction (isc_status, &D__trans, 1, &DB,
		(V4) ? 5           : 0, 
		(V4) ? default_tpb : NULL))
	ISQL_errmsg (isc_status);

/* If statistics are requested, then reserve them here */

if (Stats)
    perf_get_info (&DB, &perf_before);

/* Prepare the dynamic query stored in string. 
    But put this on the DDL transaction to get maximum visibility of
    metadata. */

if (Autocommit)
    prepare_trans = D__trans;
else 
    prepare_trans = M__trans;

if (isc_dsql_prepare (isc_status, &prepare_trans, &Stmt, 0, string, 
		      SQL_dialect, sqlda))
    {
    if (SQL_dialect == SQL_DIALECT_V6_TRANSITION && Input_file)
        {
	ISQL_printf (Out, NEWLINE);
	ISQL_printf (Out, "**** Error preparing statement:");
	ISQL_printf (Out, NEWLINE);
	ISQL_printf (Out, NEWLINE);
	ISQL_printf (Out, string);
	ISQL_printf (Out, NEWLINE);
	}
    ISQL_errmsg (isc_status);
    return ERR;
    }

/* check for warnings */
if (isc_status[2] == isc_arg_warning)
    ISQL_warning (isc_status);


#ifdef DEV_BUILD
if (Sqlda_display)
    {
    XSQLDA	*input_sqlda;

    /* Describe the input */
    input_sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (10)));
    input_sqlda->version = SQLDA_VERSION1;
    input_sqlda->sqld = input_sqlda->sqln = 10;

    if (isc_dsql_describe_bind (isc_status, &Stmt, SQL_dialect, input_sqlda))
	{
	ISQL_errmsg (isc_status);
	return ERR;
	}
    else if (isc_status[2] == isc_arg_warning)
	ISQL_warning (isc_status);

    /* Reallocate if there's LOTS of input */
    if (input_sqlda->sqld > input_sqlda->sqln)
	{
	USHORT	n_columns = input_sqlda->sqld;
	ISQL_FREE (input_sqlda);
	input_sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (n_columns)));
	input_sqlda->version = SQLDA_VERSION1;
	input_sqlda->sqld = input_sqlda->sqln = n_columns;
	if (isc_dsql_describe_bind (isc_status, &Stmt, SQL_dialect, input_sqlda))
	    {
	    ISQL_errmsg (isc_status);
	    return ERR;
	    }
	else if (isc_status[2] == isc_arg_warning)
	    ISQL_warning (isc_status);
	}

    if (input_sqlda->sqld > 0)
	{
	UCHAR	buffer [100];
	USHORT	i;
	sprintf (buffer, "\nINPUT  SQLDA version: %d sqldaid: %s sqldabc: %ld sqln: %d sqld: %d\n",
		input_sqlda->version, input_sqlda->sqldaid, input_sqlda->sqldabc,
		input_sqlda->sqln, input_sqlda->sqld);
	ISQL_printf (Out, buffer);
	for (i = 0, var = input_sqlda->sqlvar, end = var + input_sqlda->sqld; var < end; var++, i++)
	    {
	    sprintf (buffer, "%02d: sqltype: %d %s %s sqlscale: %d sqlsubtype: %d sqllen: %d\n",
		i+1, var->sqltype, sqltype_to_string(var->sqltype), 
		     (var->sqltype & 1) ? "Nullable" : "        ",
		     var->sqlscale, var->sqlsubtype, var->sqllen);
	    ISQL_printf (Out, buffer);
	    sprintf (buffer, "  :  name: (%d)%*s  alias: (%d)%*s\n",
		     var->sqlname_length, var->sqlname_length, var->sqlname,
		     var->aliasname_length, var->aliasname_length, var->aliasname);
	    ISQL_printf (Out, buffer);
	    sprintf (buffer, "  : table: (%d)%*s  owner: (%d)%*s\n",
		     var->relname_length, var->relname_length, var->relname,
		     var->ownname_length, var->ownname_length, var->ownname);
	    ISQL_printf (Out, buffer);
	    }
	}

    ISQL_FREE (input_sqlda);
    }
#endif

/* Find out what kind of statement this is */

if (isc_dsql_sql_info (isc_status, &Stmt, sizeof (sqlda_info), sqlda_info,
	sizeof (info_buffer), info_buffer))
    ISQL_errmsg (isc_status);
else
    if (info_buffer [0] == isc_info_sql_stmt_type)
	{
	l = gds__vax_integer (info_buffer + 1, 2);
	statement_type = gds__vax_integer (info_buffer + 3, l);
	}

/* if PLAN is set, print out the plan now */

/* check for warnings */
if (isc_status[2] == isc_arg_warning)
    ISQL_warning (isc_status);

if (Plan)
    {
    /* Bug 7565: A plan larger than plan_buffer will not be displayed */
    /* Bug 7565: Note also that the plan must fit into Print_Buffer */
    
    plan_buffer = (SCHAR *) ISQL_ALLOC (PRINT_BUFFER_LENGTH);
    memset (plan_buffer, 0, PRINT_BUFFER_LENGTH);
    if (isc_dsql_sql_info (isc_status, &Stmt, sizeof (plan_info), plan_info,
	PRINT_BUFFER_LENGTH, plan_buffer))
	{
	ISQL_errmsg (isc_status);
	}
    else if (plan_buffer [0] == isc_info_sql_get_plan)
	{
 	l = gds__vax_integer (plan_buffer + 1, 2);
	sprintf (Print_buffer, "%.*s%s", l, plan_buffer + 3, NEWLINE);
	ISQL_printf (Diag, Print_buffer);
	}
    ISQL_FREE (plan_buffer);
    }
	
/* If the statement isn't a select, execute it and be done */

if (!sqlda->sqld)
    {
    /* If this is an autocommit, put the DDL stmt on a special trans */

    if (Autocommit && (statement_type == isc_info_sql_stmt_ddl))
	{
	if (isc_dsql_execute_immediate (isc_status, &DB, &D__trans, 0, string, 
					SQL_dialect, NULL))
	    {
	    ISQL_errmsg (isc_status);
	    ret = ERR;
	    }
	COMMIT_TRANS (&D__trans);

	/* check for warnings */
	if (isc_status[2] == isc_arg_warning)
	    ISQL_warning (isc_status);

	return ret;
	}

    /* Check to see if this is a SET TRANSACTION statement */

    if (statement_type == isc_info_sql_stmt_start_trans)
	{
	newtrans(string);
	return ret;
	}

    /*  This is a non-select DML statement. or trans */

    if (isc_dsql_execute (isc_status, &M__trans, &Stmt, SQL_dialect, NULL))
	ISQL_errmsg (isc_status);

    /* check for warnings */
    if (isc_status[2] == isc_arg_warning)
	ISQL_warning (isc_status);

    /* We are executing a commit or rollback, commit default trans */

    if ((statement_type == isc_info_sql_stmt_commit) ||
	    (statement_type == isc_info_sql_stmt_rollback))
	COMMIT_TRANS (&D__trans);

    /* Print statistics report */

    if (Docount)
	{
	count_type = 0;
	count = 0;

	if (statement_type ==  isc_info_sql_stmt_update) 
	    count_type = isc_info_req_update_count;
	    
	if (statement_type == isc_info_sql_stmt_delete)
	    count_type = isc_info_req_delete_count;

	/* Skip selects, better to count records incoming later */

	if (statement_type == isc_info_sql_stmt_insert) 
		count_type = isc_info_req_insert_count;

	if (count_type)
	    {
	    isc_dsql_sql_info (isc_status, &Stmt, sizeof (count_info), 
		count_info, sizeof (count_buffer), count_buffer);

	    for (p = count_buffer + 3; *p != isc_info_end; )
		{
		count_is = *p++;
		l = gds__vax_integer (p, 2);
		p += 2;
		count = gds__vax_integer (p, l);
		p += l;
		if (count_is == count_type)
		    break;
		}
	    ISQL_msg_get (REC_COUNT, rec_count, (TEXT*) count, NULL, NULL, NULL, NULL);
	    /* Records affected: %ld */
	    sprintf (Print_buffer, "%s%s", rec_count, NEWLINE);	
	    ISQL_printf (Out, Print_buffer);
	    }	
	}

    if (Stats)
	{
	perf_get_info (&DB, &perf_after);
	if (!have_report)
	    {
	    ISQL_msg_get (REPORT1, report_1, NULL, NULL, NULL, NULL, NULL);
	    /* Current memory = !c\nDelta memory = !d\nMax memory = !x\nElapsed time= !e sec\n */
	    ISQL_msg_get (REPORT2, &report_1 [strlen (report_1)], NULL, NULL, NULL, NULL, NULL);
            /* For GUI_TOOLS:  Buffers = !b\nReads = !r\nWrites = !w\nFetches
 = !f\n */
            /* Else:  Cpu = !u sec\nBuffers = !b\nReads = !r\nWrites = !w\nFetch
es = !f\n */
    	    have_report = TRUE;
	    }
	perf_format (&perf_before, &perf_after, report_1, statistics, NULL_PTR);
	sprintf (Print_buffer, "%s%s", statistics, NEWLINE);
	ISQL_printf (Diag, Print_buffer);
	}
    return (ret);
    }

n_cols = sqlda->sqld;

/* The query is bigger than the curent sqlda, so make more room */

if (n_cols > sqlda->sqln)
    {
    ISQL_FREE (sqlda);
    sqlda = (XSQLDA*) ISQL_ALLOC ((SLONG) (XSQLDA_LENGTH (n_cols)));
    *sqldap = sqlda;
    sqlda->version = SQLDA_VERSION1;
    sqlda->sqld = sqlda->sqln = n_cols;

    /*  We must re-describe the sqlda, no need to re-prepare */

    isc_dsql_describe (isc_status, &Stmt, SQL_dialect, sqlda);
    }

if (statement_type != isc_info_sql_stmt_exec_procedure)
    {
    /* Otherwise, open the cursor to start things up */

    /* Declare the C cursor for what it is worth */

    if (isc_dsql_set_cursor_name (isc_status, &Stmt, "C ", 0))
	{
	ISQL_errmsg (isc_status);
	return (ERR);
	}

    /* check for warnings */
    if (isc_status[2] == isc_arg_warning)
	ISQL_warning (isc_status);

    if (isc_dsql_execute (isc_status, &M__trans, &Stmt, SQL_dialect, NULL)) 
	{
	ISQL_errmsg (isc_status);
	return (ERR);
	}

    /* check for warnings */
    if (isc_status[2] == isc_arg_warning)
        ISQL_warning (isc_status);

    }

#ifdef DEV_BUILD

/* To facilitate debugging, the option 
   SET SQLDA_DISPLAY ON
   will activate code to display the SQLDA after each statement. */

if (Sqlda_display)
    {
    UCHAR	buffer [100];
    USHORT	i;
    sprintf (buffer, "\nOUTPUT SQLDA version: %d sqldaid: %s sqldabc: %ld sqln: %d sqld: %d\n",
	    sqlda->version, sqlda->sqldaid, sqlda->sqldabc,
	    sqlda->sqln, sqlda->sqld);
    ISQL_printf (Out, buffer);
    for (i = 0, var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++, i++)
	{
	sprintf (buffer, "%02d: sqltype: %d %s %s sqlscale: %d sqlsubtype: %d sqllen: %d\n",
	    i+1, var->sqltype, sqltype_to_string(var->sqltype), 
		 (var->sqltype & 1) ? "Nullable" : "        ",
	         var->sqlscale, var->sqlsubtype, var->sqllen);
	ISQL_printf (Out, buffer);
	sprintf (buffer, "  :  name: (%d)%*s  alias: (%d)%*s\n",
	         var->sqlname_length, var->sqlname_length, var->sqlname,
	         var->aliasname_length, var->aliasname_length, var->aliasname);
	ISQL_printf (Out, buffer);
	sprintf (buffer, "  : table: (%d)%*s  owner: (%d)%*s\n",
	         var->relname_length, var->relname_length, var->relname,
	         var->ownname_length, var->ownname_length, var->ownname);
	ISQL_printf (Out, buffer);
	}
    }
#endif


nullind = (SSHORT*) ISQL_ALLOC ((SLONG) (n_cols * sizeof(SSHORT)));
#ifdef DEBUG_GDS_ALLOC
gds_alloc_flag_unfreed ((void *) nullind);
#endif
for (nullp = nullind, var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++)
    {
    /* Allocate enough space for all the sqldata and null pointers */

    type = var->sqltype & ~1;
    alignment = data_length = var->sqllen;

    /* special case db_key alignment which arrives as an SQL_TEXT */
    /* Bug 8562:  since DB_KEY arrives as SQL_TEXT, we need to account
       for the null character at the end of the string */
    if (!strncmp (var->sqlname, "DB_KEY", 6))
    {
	alignment = 8;
        /* Room for null string terminator */
        data_length++;
    }
    else
    /* Special alignment cases */
    if (type == SQL_TEXT)
	{
	alignment = 1;
	/* Room for null string terminator */
	data_length++;
	}
    else
    if (type == SQL_VARYING)
	{
	alignment = sizeof(SSHORT);
	/* Room for a null string terminator for display */
	data_length += sizeof (SSHORT) + 1;
	}

    bufsize = ALIGN (bufsize, alignment) + data_length;
    var->sqlind = nullp++;
    }

/* Buffer for reading data from the fetch */

buffer = (SLONG*) ISQL_ALLOC ((SLONG) bufsize);
memset (buffer, NULL, (unsigned int) bufsize);

/* Pad is an array of lengths to be passed to the print_item */

pad = (SLONG*) ISQL_ALLOC ((SLONG) (sqlda->sqld * sizeof (SLONG)));

offset = 0;

for (i = 0, var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++, i++)
    {
    /* record the length of name and var, then convert to print
    **   length , later to be stored in pad array
    */

    data_length = length = alignment = var->sqllen;
    namelength = var->aliasname_length;

    /* minimum display length should not be less than that needed 
    ** for displaying null 
    */

    if (namelength < NULL_DISP_LEN)
	namelength = NULL_DISP_LEN;

    type = var->sqltype & ~1;

    switch (type)
	{
	case SQL_BLOB:
	case SQL_ARRAY:
	    /* enough room for the blob id to print */

	    length = 17;
	    break;
	case SQL_TIMESTAMP:
	    if (Time_display || SQL_dialect > SQL_DIALECT_V5)
		length = DATETIME_LEN;
	    else
	        length = DATE_ONLY_LEN;
	    break;
	case SQL_TYPE_TIME:
	    length = TIME_ONLY_LEN;
	    break;
	case SQL_TYPE_DATE:
	    length = DATE_ONLY_LEN;
	    break;
	case SQL_FLOAT:
	    length = FLOAT_LEN;
	    break;
	case SQL_DOUBLE:
	    length = DOUBLE_LEN;
	    break;
	case SQL_TEXT:
	    alignment = 1;
	    data_length++;
	    /* OCTETS data is displayed in hex */
	    if (var->sqlsubtype == 1)
		length = 2*var->sqllen;
	    break;
	case SQL_VARYING:
	    data_length += sizeof (USHORT) + 1;
	    alignment = sizeof (USHORT);
	    /* OCTETS data is displayed in hex */
	    if (var->sqlsubtype == 1)
		length = 2*var->sqllen;
	    break;
	case SQL_SHORT:
	    length = SHORT_LEN;
	    break;
	case SQL_LONG:
	    length = LONG_LEN;
	    break;
	case SQL_INT64:
	    length = INT64_LEN;
	    break;
#ifdef NATIVE_QUAD
	case SQL_QUAD:
	    length = QUAD_LEN;
	    break;
#endif
	default:
	    length = UNKNOWN_LEN;
	    break;
	}

    /* special case db_key alignment which arrives as an SQL_TEXT */

    if (!strncmp (var->sqlname, "DB_KEY", 6))
	{
	alignment = 8;
	length = 2*var->sqllen;
	}

    /* This is the print width of each column */

    pad [i] = (length > namelength ? length : namelength);

    /* Is there a collist entry, then use that width, but only for text */
    if (type == SQL_TEXT || type == SQL_VARYING)
	{
	pc = Cols;
	while (pc)
	    {
	    if (!strcmp (var->aliasname, pc->col_name))
		{
		pad [i] = pc->col_len;
		break;
		}
	    pc = pc->collist_next;
	    }
	}
    /* The total line length */

    linelength += pad [i] + 1;

    /* Allocate space in buffer for data */

    offset = ALIGN (offset, alignment);
    var->sqldata = (SCHAR*) buffer + offset;
    offset += data_length;
    }

/* Add a few for line termination, et al */

linelength += 10;

/* Allocate the print line  and the header line  and the separator */

line = (TEXT*) ISQL_ALLOC ((SLONG) linelength);
header = (TEXT*) ISQL_ALLOC ((SLONG) linelength);
header2 = (TEXT*) ISQL_ALLOC ((SLONG) linelength);
*header = '\0';
*header2 = '\0';

/* Create the column header :  Left justify strings */

p = header;
p2 = header2;
for (i = 0, var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++, i++)
    {
    type = var->sqltype & ~1;
    if (type == SQL_TEXT || type == SQL_VARYING)    
	sprintf (p, "%-*.*s ", (int) (pad [i]), (int) (pad [i]), var->aliasname);
    else
	sprintf (p, "%*s ", (int) (pad [i]), var->aliasname);
    /* Separators need not go on forever no more than a line */
    for (j = 1; j < strlen (p) && j < 80; j++)
	*p2++ = '=';
    *p2++ = BLANK;
    p += strlen (p);
    
    }
*p2 = '\0';

/* If this is an exec procedure, execute into the sqlda with one fetch only */

if (statement_type == isc_info_sql_stmt_exec_procedure)
    {
    if (isc_dsql_execute2 (isc_status, &M__trans, &Stmt, 
			    SQL_dialect, NULL, sqlda)) 
	{
	ISQL_errmsg (isc_status);
	ret = ERR;
	}
    else
	{
	ISQL_printf (Out, NEWLINE);
	if (!List)
	    {
	    ISQL_printf (Out, header);
	    ISQL_printf (Out, NEWLINE);
	    ISQL_printf (Out, header2);
	    ISQL_printf (Out, NEWLINE);
	    }
	print_line (sqlda, pad, line);
	ISQL_printf (Out, NEWLINE);
	}
    }

/* Otherwise fetch and print records until EOF */
else
    {

#ifdef GUI_TOOLS
    ULONG       dwRows;
    dwRows      = 0;
#endif

    for (lines = 0; !Abort_flag; ++lines)
	{

	/* Fetch the current cursor */

#ifdef SCROLLABLE_CURSORS
	if (Autofetch)    
	    {
#endif
	    if (isc_dsql_fetch (isc_status, &Stmt, SQL_dialect, sqlda) == 100)
	        break;
#ifdef SCROLLABLE_CURSORS
	    }
	else
	    {
	    SSHORT	fetch_ret;
	    TEXT	*fetch_statement = NULL;
	    USHORT	fetch_bufsize = 0;

	    /* if not autofetching, sit and wait for user to input 
               NEXT, PRIOR, FIRST, LAST, ABSOLUTE <n>, or RELATIVE <n> */

	    fetch_ret = get_statement (&fetch_statement, &fetch_bufsize, fetch_prompt);
	    if (fetch_ret == FETCH)
		{
		if (isc_dsql_fetch2 (isc_status, &Stmt, SQL_dialect, sqlda, 
				     fetch_direction, fetch_offset) == 100)
		    {
	            if (fetch_statement)
    		        ISQL_FREE (fetch_statement);
		    break;
		    }
		}
	    else if (fetch_ret == BACKOUT || fetch_ret == EXIT)
		{
		lines = 0;
		break;
		}
	    else
		{
		TEXT	errbuf [MSG_LENGTH];
    		gds__msg_format (NULL_PTR, ISQL_MSG_FAC, CMD_ERR, MSG_LENGTH, 
			errbuf, "Expected NEXT, PRIOR, FIRST, LAST, ABSOLUTE <n>, RELATIVE <n>, or QUIT", NULL_PTR, NULL_PTR, NULL_PTR, NULL_PTR);
    		STDERROUT (errbuf, 1);
		}

	    if (fetch_statement)
    		ISQL_FREE (fetch_statement);
	    }
#endif

#ifdef GUI_TOOLS
        /* Update Row count for Windows ISQL */

        UpdateRowCount (dwRows++);
#endif

	/* Print the header every Pagelength number of lines for
	   command-line ISQL only.  For WISQL, print the column
           headings only once. */

#ifdef GUI_TOOLS
	if (!List && !lines)
	    {
	    ISQL_printf (Out, NEWLINE);
	    ISQL_printf (Out, header);
	    ISQL_printf (Out, NEWLINE);
	    ISQL_printf (Out, header2);
	    ISQL_printf (Out, NEWLINE);
	    }

#else
	if (!List && (lines % Pagelength == 0))
	    {
	    ISQL_printf (Out, NEWLINE);
	    ISQL_printf (Out, header);
	    ISQL_printf (Out, NEWLINE);
	    ISQL_printf (Out, header2);
	    ISQL_printf (Out, NEWLINE);
	    }
#endif

	if (isc_status [1])
	    {
	    ISQL_errmsg (isc_status);
	    isc_dsql_free_statement (isc_status, &Stmt, 1);
	    if (nullind)
		ISQL_FREE (nullind);
	    if (pad)
		ISQL_FREE (pad);
	    if (buffer)
		ISQL_FREE (buffer);
	    if (header)
		ISQL_FREE (header);
	    if (header2)
		ISQL_FREE (header2);
	    if (line)
		ISQL_FREE (line);
	    return (ERR);
	    }

	if (!lines)
	    ISQL_printf (Out, NEWLINE);

	ret = print_line (sqlda, pad, line);
	}

    if (lines)
	ISQL_printf (Out, NEWLINE);

    /* Record count printed here upon request */

    if (Docount)
	{
	ISQL_msg_get (REC_COUNT, rec_count, (TEXT*) lines, NULL, NULL, NULL, NULL);
	/* Total returned: %ld */
	sprintf (Print_buffer, "%s%s", rec_count, NEWLINE);	

	ISQL_printf (Out, Print_buffer);
	}
    }

isc_dsql_free_statement (isc_status, &Stmt, 1);

/* Statistics printed here upon request */

if (Stats)
    {
    perf_get_info (&DB, &perf_after);
    if (!have_report)
	{
	ISQL_msg_get (REPORT1, report_1, NULL, NULL, NULL, NULL, NULL);
	/* Current memory = !c\nDelta memory = !d\nMax memory = !x\nElapsed time= !e sec\n */
	ISQL_msg_get (REPORT2, &report_1 [strlen (report_1)], NULL, NULL, NULL, NULL, NULL);
            /* For GUI_TOOLS:  Buffers = !b\nReads = !r\nWrites = !w\nFetches
 = !f\n */
            /* Else:  Cpu = !u sec\nBuffers = !b\nReads = !r\nWrites = !w\nFetch
es = !f\n */
    	have_report = TRUE;
	}
    perf_format (&perf_before, &perf_after, report_1, statistics, NULL_PTR);
    sprintf (Print_buffer, "%s%s", statistics, NEWLINE);
    ISQL_printf (Out, Print_buffer);
    }

if (pad)
    ISQL_FREE (pad);
if (buffer)
    ISQL_FREE (buffer);
if (line)
    ISQL_FREE (line);
if (header)
    ISQL_FREE (header);
if (header2)
    ISQL_FREE (header2);
if (nullind)
    ISQL_FREE (nullind);

return (ret);
}

static void CLIB_ROUTINE query_abort (void)
{
/**************************************
 *
 *	q u e r y _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	Signal handler for interrupting output of a query.
 *
 **************************************/

Abort_flag = 1;
}

static void strip_quotes (
    TEXT	*in,
    TEXT	*out)
{
/**************************************
 *
 *	s t r i p _ q u o t e s
 *
 **************************************
 *
 * Functional description
 *	Get rid of quotes around strings
 *
 **************************************/
TEXT	*p;
TEXT	quote;

if (!in || !*in)
    {
    *out = 0;
    return;
    }

quote = 0;
/* Skip any initial quote */
if ((*in == DBL_QUOTE) || (*in == SINGLE_QUOTE))
    quote = *in++;
p = in;

/* Now copy characters until we see the same quote or EOS */
while (*p && (*p != quote))
    {
    *out++ = *p++;
    }
*out = 0;
}


#ifdef DEV_BUILD
static UCHAR	*sqltype_to_string (
    USHORT      sqltype)
{
/**************************************
 *
 *	s q l t y p e _ t o _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Return a more readable version of SQLDA.sqltype
 *
 **************************************/
switch (sqltype & ~1)
    {
    case SQL_TEXT:	return ("TEXT     ");
    case SQL_VARYING:   return ("VARYING  ");
    case SQL_SHORT:     return ("SHORT    ");
    case SQL_LONG:      return ("LONG     ");
    case SQL_INT64:     return ("INT64    ");
    case SQL_FLOAT:     return ("FLOAT    ");
    case SQL_DOUBLE:    return ("DOUBLE   ");
    case SQL_D_FLOAT:   return ("D_FLOAT  ");
    case SQL_TIMESTAMP: return ("TIMESTAMP");
    case SQL_TYPE_DATE: return ("SQL DATE ");
    case SQL_TYPE_TIME: return ("TIME     ");
    case SQL_BLOB:      return ("BLOB     ");
    case SQL_ARRAY:     return ("ARRAY    ");
    case SQL_QUAD:      return ("QUAD     ");
    default:		return ("unknown  ");
    }
}
#endif

