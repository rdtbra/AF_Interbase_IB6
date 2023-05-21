/*
 *	PROGRAM:	Preprocessor
 *	MODULE:		gpre.c
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

#define GPRE_MAIN
#define PARSER_MAIN
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "../gpre/gpre.h"
#include "../jrd/license.h"
#include "../gpre/parse.h"
#include "../jrd/intl.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/hsh_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/lang_proto.h"
#include "../gpre/met_proto.h"
#include "../gpre/msc_proto.h"
#include "../gpre/par_proto.h"
#include "../jrd/gds_proto.h"
#include "../gpre/gpreswi.h"

#ifdef VMS
#include <descrip.h>

extern int lib$get_foreign();
#endif

#ifdef APOLLO
#include "/sys/ins/base.ins.c"
#include "/sys/ins/pgm.ins.c"
#endif 

#ifdef mpexl
#include "../jrd/mpexl.h"

#define FOPEN_READ_TYPE		"r Tm"
#define FOPEN_WRITE_TYPE	"w V Ds2 E32 S256000"
#define SCRATCH			"gd"
#endif

#ifdef PC_PLATFORM
#define SCRATCH			"I"
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#define SCRATCH			"ib"
#endif

#ifndef SCRATCH
#ifdef SMALL_FILE_NAMES
#define SCRATCH			"gds_q"
#else
#define SCRATCH			"gds_query_"
#endif
#endif

#ifndef FOPEN_READ_TYPE
#define FOPEN_READ_TYPE		"r"
#define FOPEN_WRITE_TYPE	"w"
#endif

static BOOLEAN	all_digits (UCHAR *);
static int	arg_is_string (SLONG, TEXT **, TEXT *);
static SSHORT	compare_ASCII7z (UCHAR *, UCHAR *);
static SLONG	compile_module (SLONG);
static BOOLEAN	file_rename (TEXT *, TEXT *, TEXT *);
static void	finish_based (ACT);
static int	get_char (IB_FILE *);
static BOOLEAN	get_switches (int, TEXT **, IN_SW_TAB, SW_TAB, TEXT **);
static TOK	get_token (void);
static int	nextchar (void);
static SLONG	pass1 (void);
static void	pass2 (SLONG);
static void	print_switches (void);
static void	remember_label (TEXT *);
static IB_FILE	*reposition_file (IB_FILE *, SLONG);
static void	return_char (SSHORT);
static SSHORT	skip_white (void);

/* Program wide globals */
    
IB_FILE		*input_file, *trace_file;
TEXT		*file_name, *out_file_name;
SLONG		position, last_position, line_position, first_position,
		prior_line_position;
ACT		last_action, first_action;
UCHAR		classes [256], fortran_labels [1024];
TEXT		*ident_pattern, *utility_name, *count_name, *slack_name,
		*transaction_name, *database_name;

static TEXT	input_buffer [512], *input_char;

static DBB	sw_databases;
static USHORT	sw_first;
static jmp_buf	fatal_env;
struct tok	prior_token;
static TEXT	*comment_start, *comment_stop;
static void	(*gen_routine)();
static TEXT	trace_file_name [128];
static SLONG	traced_position = 0;


/*
 * Type and table definition for the extension tables.  Tells GPRE
 * the default extensions for DML and host languages.
 */

typedef struct ext_table_t {
    enum lan_t	ext_language;
    int		ext_in_sw;
    TEXT	*in;
    TEXT	*out;
} *EXT_TAB;

static struct ext_table_t dml_ext_table [] = {
	lan_c, IN_SW_GPRE_C, ".e", ".c",

#ifndef VMS
#ifndef mpexl
#ifndef WIN_NT
	lan_scxx, IN_SW_GPRE_SCXX, ".E", ".C",
#endif
#endif
#endif

	lan_cxx, IN_SW_GPRE_CXX, ".exx", ".cxx",
	lan_cpp, IN_SW_GPRE_CXX, ".epp", ".cpp",
	lan_internal, IN_SW_GPRE_G, ".e", ".c",
	lan_pascal, IN_SW_GPRE_P, ".epas", ".pas",

#ifdef FORTRAN
#ifdef VMS
#define FORTRAN_EXTENSIONS
	lan_fortran, IN_SW_GPRE_F, ".efor", ".for",
#endif
#ifdef APOLLO
#define FORTRAN_EXTENSIONS
	lan_fortran, IN_SW_GPRE_F, ".eftn", ".ftn",
#endif
#ifndef FORTRAN_EXTENSIONS
	lan_fortran, IN_SW_GPRE_F, ".ef", ".f",
#endif
#endif

#ifdef COBOL
#ifdef VMS
#define COBOL_EXTENSIONS
	lan_cobol, IN_SW_GPRE_COB, ".ecob", ".cob",
#endif
#ifdef mpexl
#define COBOL_EXTENSIONS
	lan_cobol, IN_SW_GPRE_COB, ".ecob", ".cob",
#endif
#ifndef COBOL_EXTENSIONS
#define COBOL_EXTENSIONS
	lan_cobol, IN_SW_GPRE_COB, ".ecbl", ".cbl",
#endif
#endif

#ifdef BASIC
	lan_basic, IN_SW_GPRE_BAS, ".ebas", ".bas",
#endif

#ifdef PLI
	lan_pli, IN_SW_GPRE_PLI, ".epli", ".pli",
#endif

#ifdef VMS
#define ADA_EXTENSIONS
	lan_ada, IN_SW_GPRE_ADA, ".eada", ".ada",
#endif
#ifdef hpux
#define ADA_EXTENSIONS
	lan_ada, IN_SW_GPRE_ADA, ".eada", ".ada",
#endif
#ifndef ADA_EXTENSIONS
	lan_ada, IN_SW_GPRE_ADA, ".ea", ".a",
#endif
#ifdef ALSYS_ADA
	lan_ada, IN_SW_GPRE_ALSYS, ".eada", ".ada",
#endif
#if ( defined( PC_PLATFORM) || defined( WIN_NT))
	lan_cplusplus, IN_SW_GPRE_CPLUSPLUS, ".epp", ".cpp",
#else
	lan_cplusplus, IN_SW_GPRE_CPLUSPLUS, ".exx", ".cxx",
#endif
	lan_undef, IN_SW_GPRE_0, NULL, NULL
};

#define CHR_LETTER	1
#define CHR_DIGIT	2
#define CHR_IDENT	4
#define CHR_QUOTE	8
#define CHR_WHITE	16
#define CHR_INTRODUCER	32
#define CHR_DBLQUOTE	64

/* macro compares chars; case sensitive for some platforms */

#ifdef EITHER_CASE
#define SAME(p,q)	UPPER7 (*p) == UPPER7 (*q)
#else
#define SAME(p,q)	*p == *q
#endif

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
 *	Main line routine for C preprocessor.  Initializes
 *	system, performs pass 1 and pass 2.  Interprets
 *	command line.
 *
 **************************************/
DBB		db;
SYM		symbol;
SLONG		end_position;
int		i;
TEXT		*p, spare_file_name [256], spare_out_file_name [256], 
		*filename_array [3], *db_filename, temp_name [256];
BOOLEAN		renamed, explicit;
EXT_TAB		ext_tab, src_ext_tab;
SW_TAB		sw_tab;
struct sw_tab_t	sw_table [IN_SW_GPRE_COUNT];
IB_FILE		*temp;
SSHORT		c;
	
strcpy (ada_package, "");
ada_flags = 0;
input_char = input_buffer;

#ifdef VMS
argc = VMS_parse (&argv, argc);
#endif

/* Initialize character class table */

for (i=0;   i <= 127; i++)
    classes [i] = 0;
for (i=128; i <= 255; i++)
    classes [i] = CHR_LETTER | CHR_IDENT;
for (i='a'; i<='z'; i++)
    classes [i] = CHR_LETTER | CHR_IDENT;
for (i='A'; i<='Z'; i++)
    classes [i] = CHR_LETTER | CHR_IDENT;
for (i='0'; i<= '9'; i++)
    classes [i] = CHR_DIGIT | CHR_IDENT;

classes ['_'] = CHR_LETTER | CHR_IDENT | CHR_INTRODUCER;
classes ['$'] = CHR_IDENT;
classes [' '] = CHR_WHITE;
classes ['\t'] = CHR_WHITE;
classes ['\n'] = CHR_WHITE;
classes ['\r'] = CHR_WHITE;
classes ['\''] = CHR_QUOTE;
classes ['\"'] = CHR_DBLQUOTE;
classes ['#'] = CHR_IDENT;

/* zorch 0 through 7 in the fortran label vector */

fortran_labels[0] = 255;

/* set switches and so on to default (C) values */ 

db = NULL;
sw_language = lan_undef;
sw_lines = sw_auto = sw_cstring = TRUE;
sw_alsys = sw_external = sw_gen_sql = sw_standard_out = sw_ansi = sw_dsql = FALSE;
sw_d_float = sw_version = FALSE;
sw_sql_dialect = SQL_DIALECT_V5;
dialect_specified = 0;
sw_window_scope = DBB_GLOBAL;
gen_routine = C_CXX_action;
comment_start = "/*";
comment_stop = "*/";
ident_pattern = "gds__%d";
transaction_name = "gds__trans";
database_name = "gds__database";
utility_name = "gds__utility";
count_name = "gds__count";
slack_name = "gds__slack";
global_db_count = 0;
default_user = NULL;
default_password = NULL;
default_lc_ctype = NULL;
default_lc_messages = NULL;
text_subtypes = NULL;
override_case = 0;

sw_know_interp = FALSE;
sw_interp = 0;

sw_sql_dialect = compiletime_db_dialect = SQL_DIALECT_V5;

/* 
 * Call a subroutine to process the input line 
 */

filename_array[0] = filename_array[1] = filename_array[2] = NULL;
if (!get_switches (argc, argv, gpre_in_sw_table, sw_table, filename_array))
   CPR_exit (FINI_ERROR);

file_name = filename_array[0];
out_file_name = filename_array[1];
db_filename = filename_array[2];

if (!file_name)
    {
    ib_fprintf (ib_stderr, "gpre:  no source file named.\n");
    CPR_exit (FINI_ERROR);
    }

/*
 * Try to open the input file.  
 * If the language wasn't supplied, maybe the kind user included a language
 * specific extension, and the file name fixer will find it.  The file name
 * fixer returns FALSE if it can't add an extension, which means there's already
 * one of the right type there.
 */

if (sw_language == lan_undef)
    for (ext_tab = dml_ext_table; sw_language = ext_tab->ext_language; ext_tab++)
	{
	strcpy (spare_file_name, file_name);
	if (!(file_rename (spare_file_name, ext_tab->in, NULL))) 
	    break;
	}

/*
 *  Sigh.  No such luck. Maybe there's a file lying around with a plausible
 *  extension and we can use that.
 */

if (sw_language == lan_undef)
    for (ext_tab = dml_ext_table; sw_language = ext_tab->ext_language; ext_tab++) 
	{ 
	strcpy (spare_file_name, file_name);
	if (file_rename (spare_file_name, ext_tab->in, NULL) &&
	    (input_file = ib_fopen (spare_file_name, FOPEN_READ_TYPE)))
	    {
	    file_name = spare_file_name;
	    break;
	    }
 	}

/*
 *  Well, if he won't tell me what language it is, or even give me a hint, I'm
 *  not going to spend all day figuring out what he wants done.
 */

if (sw_language == lan_undef)
    {
    ib_fprintf (ib_stderr, "gpre: can't find %s with any known extension.  Giving up.\n", file_name);
    CPR_exit (FINI_ERROR);
    }

/*
 * Having got here, we know the language, and might even have the file open.
 * Better check before reopening it on ourselves.  Try the file with the
 * extension first (even if we have to add the extension).   If we add an
 * extension, and find a file with that extension, we make the file name
 * point to the expanded file name string in a private buffer.
 */ 

if (!input_file) 
    {
    strcpy (spare_file_name, file_name);
    for (ext_tab = dml_ext_table; ext_tab->ext_language != sw_language; ext_tab++)
	;
    renamed = file_rename (spare_file_name, ext_tab->in, NULL);
    if (renamed && (input_file = ib_fopen (spare_file_name, FOPEN_READ_TYPE)))
	file_name = spare_file_name;
    else	
	if (!(input_file = ib_fopen (file_name, FOPEN_READ_TYPE)))
	    {
	    if (renamed)
		ib_fprintf (ib_stderr, "gpre: can't open %s or %s\n", file_name, spare_file_name);
	    else
		ib_fprintf (ib_stderr, "gpre: can't open %s\n", file_name);
	    CPR_exit (FINI_ERROR);
	    }
    }

/* 
 * Now, apply the switches and defaults we've so painfully acquired;
 * adding in the language switch in case we inferred it rather than parsing it.
 */

src_ext_tab = dml_ext_table; 
while (src_ext_tab->ext_language != sw_language)
    src_ext_tab++;
sw_table [0].sw_in_sw = src_ext_tab->ext_in_sw;

for (sw_tab = sw_table; sw_tab->sw_in_sw; sw_tab++)
    switch (sw_tab->sw_in_sw)
	{
	case IN_SW_GPRE_C :
	    sw_language = lan_c;
	    ident_pattern = "isc_%d";
	    utility_name = "isc_utility";
	    count_name = "isc_count";
	    slack_name = "isc_slack";
	    break;

	case IN_SW_GPRE_SCXX :
	case IN_SW_GPRE_CXX :
	case IN_SW_GPRE_CPLUSPLUS :
	    sw_language = lan_cxx;
	    ident_pattern = "isc_%d";
	    utility_name = "isc_utility";
	    count_name = "isc_count";
	    slack_name = "isc_slack";
	    transaction_name = "gds_trans";
	    database_name = "gds_database";
	    break;

 	case IN_SW_GPRE_D :
	    /* allocate database block and link to db chain */

	    db = (DBB) MSC_alloc_permanent (DBB_LEN);
	    db->dbb_next = isc_databases;

	    /* put this one in line to be next */

	    isc_databases = db;

	    /* allocate symbol block */

	    symbol = (SYM) MSC_alloc_permanent (SYM_LEN);

	    /* make it a database, specifically this one */

	    symbol->sym_type = SYM_database;
	    symbol->sym_object = (CTX) db;
	    symbol->sym_string = database_name;

	    /* database block points to the symbol block */

	    db->dbb_name = symbol;

	    /* give it the file name and try to open it */

	    db->dbb_filename = db_filename;
	    if (!MET_database (db, TRUE))
		CPR_exit (FINI_ERROR);
	    if (sw_external)
		db->dbb_scope = DBB_EXTERN;
#ifdef FTN_BLK_DATA
	    else
		{
		global_db_count = 1;
		strcpy (global_db_list[0].dbb_name, db->dbb_name->sym_string);
		}
#endif
	    break;

	case IN_SW_GPRE_E :
	    sw_case = TRUE;
	    break;

#ifdef ADA
	case IN_SW_GPRE_ADA:
#ifdef APOLLO
	    ada_null_address = "system.no_addr";
#else
#ifdef VMS
	    ada_null_address = "system.address_zero";
#else
	    ada_null_address = "0";
#endif
#endif
	    sw_case = TRUE;
	    sw_language = lan_ada;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = ADA_action;
	    utility_name = "isc_utility";
	    count_name = "isc_count";
	    slack_name = "isc_slack";
	    transaction_name = "gds_trans";
	    database_name = "isc_database";
	    ident_pattern = "isc_%d";
	    comment_start = "--";
	    if (db)
		db->dbb_name->sym_string = "isc_database";
	    comment_stop = "--";
	    break;

	case IN_SW_GPRE_ALSYS:
	    sw_alsys = TRUE;
	    sw_case = TRUE;
	    sw_language = lan_ada;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = ADA_action;
	    utility_name = "isc_utility";
	    count_name = "isc_count";
	    slack_name = "isc_slack";
	    transaction_name = "gds_trans";
	    database_name = "isc_database";
	    ident_pattern = "isc_%d";
	    comment_start = "--";
	    if (db)
		db->dbb_name->sym_string = "isc_database";
	    comment_stop = "--";
	    break;
#endif	    


#ifdef FORTRAN
	case IN_SW_GPRE_F :
	    sw_case = TRUE;
	    sw_language = lan_fortran;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = FTN_action;
#ifdef sun
	    comment_start = "*      ";
#else
	    comment_start = "C      ";
#endif
	    comment_stop = " ";

	    /* Change the patterns for v4.0 */

	    ident_pattern = "isc_%d";
	    utility_name = "isc_utility";
	    count_name = "isc_count";
	    slack_name = "isc_slack";
	    break;
#endif
 
#ifdef COBOL
	case IN_SW_GPRE_ANSI :
	    sw_ansi = TRUE;
	    break;

	case IN_SW_GPRE_COB :
	    sw_case = TRUE;
	    sw_language = lan_cobol;
	    comment_stop = " ";
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = COB_action;
	    break;
#endif

#ifdef BASIC
	case IN_SW_GPRE_BAS :
	    sw_case = TRUE;
	    sw_language = lan_basic;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = BAS_action;
	    comment_start = "\t! ";
	    comment_stop = " ";
	    break;
#endif

#ifdef PLI
	case IN_SW_GPRE_PLI :
	    sw_case = TRUE;
	    sw_language = lan_pli;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = PLI_action;
	    comment_start = "/*";
	    comment_stop = "*/";
	    break;
#endif

#ifdef PASCAL
	case IN_SW_GPRE_P :
	    sw_case = TRUE;
	    sw_language = lan_pascal;
	    sw_lines = FALSE;
	    sw_cstring = FALSE;
	    gen_routine = PAS_action;
	    comment_start = "(*";
	    comment_stop = "*)";
	    break;
#endif
	case IN_SW_GPRE_D_FLOAT:
	    sw_d_float = TRUE;
	    break;

	case IN_SW_GPRE_G :
	    sw_language = lan_internal;
	    gen_routine = INT_action;
	    sw_cstring = FALSE;
	    transaction_name = "dbb->dbb_sys_trans";
	    sw_know_interp = TRUE;
	    sw_interp = ttype_metadata;
	    break;

	case IN_SW_GPRE_I :
	    sw_ids = TRUE;
	    break;

	case IN_SW_GPRE_M :
	    sw_auto = FALSE;
	    break;

	case IN_SW_GPRE_N :
	    sw_lines = FALSE;
	    break;
 	
	case IN_SW_GPRE_O :
	    sw_standard_out = TRUE;
	    out_file = ib_stdout;
	    break;

	case IN_SW_GPRE_R :
	    sw_raw = TRUE;
	    break;

	case IN_SW_GPRE_S :
	    sw_cstring = FALSE;
	    break;

	case IN_SW_GPRE_T :
	    sw_trace = TRUE;
	    break;

	default:
	    break;
	}  

if ((sw_auto) && (default_user || default_password || default_lc_ctype))
    {
    CPR_warn ("gpre: -user, -password and -charset switches require -manual");
    }
/*
 * If one of the C++ variants was used/discovered, convert to C++ for
 * further internal use.
 */

if ( sw_language == lan_cpp || sw_language == lan_cplusplus)
    sw_language = lan_cxx;

#ifdef ALSYS_ADA
if (sw_alsys)
    {
    for (src_ext_tab = dml_ext_table; ;src_ext_tab++)
	if (src_ext_tab->ext_in_sw == IN_SW_GPRE_ALSYS)
	    break;
    }
#endif 
	    
#ifdef VMS
/*
 * If we're on VMS, we may have an RMS sequential file rather than
 * a stream file, and keeping track of our place will be harder.
 * So... for VMS, copy the input to a stream file.
 *
 * If this is Alpha OpenVMS, then we also have to do some more
 * work, since RMS is a little different...
 */

#ifndef __ALPHA
temp = (IB_FILE*) gds__temp_file (TRUE, "temp", NULL_PTR);
strcpy (temp_name, "temporary file");
#else
temp = (IB_FILE*) gds__temp_file (TRUE, "temp", temp_name);
#endif
if (temp != (IB_FILE*) -1)
    {
    while ((c = get_char (input_file)) != EOF)
	ib_putc (c, temp);
    ib_fclose (input_file);
#ifdef __ALPHA
    ib_fclose (temp);
    temp = ib_fopen (temp_name, FOPEN_READ_TYPE);
#endif
    }
else
    {
    ib_fprintf (ib_stderr, "gpre: can't open %s\n", temp_name);
    CPR_exit (FINI_ERROR);
    }

input_file = temp;
#endif

#ifdef COBOL
/* if cobol is defined we need both sw_cobol and sw_ansi to
 * determine how the string substitution table is set up
 */

if (sw_language == lan_cobol)
    if (sw_ansi)
	{
	if (db)
	    db->dbb_name->sym_string = "isc-database";
	comment_start = "      *  ";
	ident_pattern = "isc-%d";
	transaction_name = "isc-trans";
	database_name = "isc-database";
	utility_name = "isc-utility";
	count_name = "isc-count";
	slack_name = "isc-slack";
	}
    else
	comment_start = "*      ";

COB_name_init (sw_ansi);
#endif

/* 
 * See if user has specified an interpretation on the command line,
 * as might be used for SQL access.
 */

if (default_lc_ctype)
    {
    if (all_digits (default_lc_ctype))
	{
	/* Numeric name? if so assume user has hard-coded a subtype number */

	sw_interp = atoi (default_lc_ctype);
	sw_know_interp = TRUE;
	}
    else if (compare_ASCII7z (default_lc_ctype, "DYNAMIC") == 0)
	{
	/* Dynamic means use the interpretation declared at runtime */

	sw_interp = ttype_dynamic;
	sw_know_interp = TRUE;
	}
    else if (isc_databases)
	{
	/* Name resolution done by MET_load_hash_table */

	isc_databases->dbb_c_lc_ctype = default_lc_ctype;
	}
    }

/* 
 * Finally, open the output file, if we're not using standard out.
 * If only one file name was given, make sure it has the preprocessor
 * extension, then back up to that extension, zorch it, and add
 * the language extension. Then you've got an output name.  Take it
 * and add the correct extension.  If got an explicit output file
 * name, use it as is unless it doesn't have an extension in which
 * case use the default language extension.  Finally, open the file.
 * What could be easier?
 */

if (!sw_standard_out)
    {
    renamed = explicit = TRUE;
    if (!out_file_name)
	{
	out_file_name = spare_out_file_name;
	strcpy (spare_out_file_name, file_name);
	if (renamed = file_rename (spare_out_file_name, src_ext_tab->in, src_ext_tab->out))
	    explicit = FALSE;
	}

    if (renamed)
	{
	for (p = out_file_name; *p; p++)
	    ;
#ifdef VMS
	while (p != out_file_name && *p != '.' && *p != ']')
#else
	while (p != out_file_name && *p != '.' && *p != '/')
#endif
	    p--;
	if (!explicit)
	    *p = 0;

	if (*p != '.')
	    {
	    strcpy (spare_out_file_name, out_file_name);
	    out_file_name = spare_out_file_name;
	    file_rename (out_file_name, src_ext_tab->out, NULL);
	    }
	}

    if (!(strcmp (out_file_name, file_name)))
	{
	ib_fprintf (ib_stderr, "gpre: output file %s would duplicate input\n",
		out_file_name);
	CPR_exit (FINI_ERROR);
	}
    if ((out_file = ib_fopen (out_file_name, FOPEN_WRITE_TYPE)) == NULL)
	{
	ib_fprintf (ib_stderr, "gpre: can't open output file %s\n", out_file_name);
	CPR_exit (FINI_ERROR);
	}
    }

/* Compile modules until end of file */

sw_databases = isc_databases;

if (!setjmp (fatal_env))
    for (end_position = 0; end_position = compile_module (end_position);)
	;

#ifdef FTN_BLK_DATA
if (sw_language == lan_fortran)
    FTN_fini();
#endif

MET_fini (NULL_PTR);
ib_fclose (input_file);
#ifdef VMS
#ifdef __ALPHA
delete (temp_name);
#endif
#endif

if (!sw_standard_out)
    {
    ib_fclose (out_file);
    if (errors)
	unlink (out_file_name);
    }

if (errors || warnings)
    { 
    if (!errors)
	ib_fprintf (ib_stderr, "No errors, ");
    else if (errors == 1)
	ib_fprintf (ib_stderr, "1 error, ");
    else
	ib_fprintf (ib_stderr, "%3d errors, ", errors);
    if (!warnings)
	ib_fprintf (ib_stderr, "no warnings\n");
    else if (warnings == 1)
	ib_fprintf (ib_stderr, "1 warning\n");
    else
	ib_fprintf (ib_stderr, "%3d warnings\n", warnings); 
    }

CPR_exit ((errors) ? FINI_ERROR : FINI_OK);
}

void CPR_abort (void)
{
/**************************************
 *
 *	C P R _ a b o r t
 *
 **************************************
 *
 * Functional description
 *	Abort this silly program.
 *
 **************************************/

++fatals;
longjmp (fatal_env, 1);
}

#ifdef DEV_BUILD
void CPR_assert (
    TEXT	*file,
    int		line)
{
/**************************************
 *
 *	C P R _ a s s e r t
 *
 **************************************
 *
 * Functional description
 *	Report an assertion failure and abort this silly program.
 *
 **************************************/
TEXT	buffer [200];

sprintf (buffer, "GPRE assertion failure file '%s' line '%d'", file, line);
CPR_bugcheck (buffer);
}
#endif

void CPR_bugcheck (
    TEXT	*string)
{
/**************************************
 *
 *	C P R _ b u g c h e c k
 *
 **************************************
 *
 * Functional description
 *	Issue an error message.
 *
 **************************************/

ib_fprintf (ib_stderr, "*** INTERNAL BUGCHECK: %s ***\n", string);
MET_fini (NULL_PTR);
CPR_abort();
}

void CPR_end_text (
    TXT		text)
{
/**************************************
 *
 *      C P R _ e n d _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Mark end of a text description.
 *
 **************************************/

text->txt_length = token.tok_position - text->txt_position - 1;
}

CPR_error (
    TEXT	*string)
{
/**************************************
 *
 *	C P R _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Issue an error message.
 *
 **************************************/

ib_fprintf (ib_stderr, "(E) %s:%d: %s\n", file_name, line+1, string);
errors++;

return NULL;
}

void CPR_exit (
    int		stat)
{
/**************************************
 *
 *	C P R _ e x i t
 *
 **************************************
 *
 * Functional description
 *	Exit with status.
 *
 **************************************/
#ifdef LINUX
if (trace_file_name [0])
    {
    if (trace_file)
        ib_fclose (trace_file);
    unlink (trace_file_name);
    }
#else
if (trace_file)
    ib_fclose (trace_file);
if (trace_file_name [0])
    unlink (trace_file_name);
#endif

#ifdef APOLLO
switch (stat)
    {
    case FINI_ERROR	:
	exit (pgm_$output_invalid);
	break;
    case FINI_OK	:
	exit (pgm_$ok);
	break;
    default		:
	exit (pgm_$error);
	break;
    }
#else
exit (stat);
#endif
}

void CPR_warn (
    TEXT	*string)
{
/**************************************
 *
 *	C P R _ w a r n
 *
 **************************************
 *
 * Functional description
 *	Issue an warning message.
 *
 **************************************/

ib_fprintf (ib_stderr, "(W) %s:%d: %s\n", file_name, line+1, string);
warnings++;
}

TOK CPR_eol_token (void)
{
/**************************************
 *
 *	C P R _ e o l _ t o k e n
 *
 **************************************
 *
 * Functional description
 *	Fortran, being a line oriented language, sometimes needs
 *	to know when it is at end of line to avoid parsing into the
 *	next statement.  CPR_eol_token normally gets the next token,
 *	but if the language is FORTRAN and there isn't anything else
 *	on the line, it fakes a dummy token to indicate end of line.
 *
 **************************************/
SSHORT	c, peek;
SSHORT	num_chars;
TEXT	*p;

if (sw_language != lan_fortran && sw_language != lan_basic)
     return CPR_token();

/* Save the information from the previous token */

prior_token = token;
prior_token.tok_position = last_position;

last_position = token.tok_position + token.tok_length + token.tok_white_space - 1;
p = token.tok_string;
num_chars = 0;

/* skip spaces */

for (c = nextchar(); c == ' '; c = nextchar())
    {
    num_chars++;
    *p++ = c;
    }

/* in-line comments are equivalent to end of line */

if (c == '!')
   while (c != '\n' && c != EOF)
	{
	c = nextchar();
	num_chars++;
	}
#ifdef APOLLO
/* in-line comments are equivalent to end of line */

if (c == '{')
   while (c != '\n' && c != EOF)
	{
	c = nextchar();
	num_chars++;
	}
#endif

/* in-line SQL comments are equivalent to end of line */

if (sw_sql && (c == '-'))
    {
    peek = nextchar();
    if (peek != '-')
	return_char (peek);
    else
	{
	while (c != '\n' && c != EOF)
	    {
	    c = nextchar();
	    num_chars++;
	    }
	last_position = position - 1;
	}
    }

if (c == EOF)
    {
    token.tok_symbol = NULL;
    token.tok_keyword = KW_none;
    return NULL;
    }

/* Not EOL so back up to the begining and try again */

if (c != '\n')
    {
    return_char (c);
    while (--num_chars > 0)
	return_char (*--p);
    return CPR_token();
    }

/* if we've got EOL, treat it like a semi-colon */
/* NOTE: the fact that the length of this token is set to 0, is used as an
   indicator elsewhere that it was a faked token */

token.tok_string [0] = ';';
token.tok_string [1] = 0;
token.tok_type = tok_punct;
token.tok_length = 0;
token.tok_white_space = 0;
token.tok_position = position;
token.tok_symbol = HSH_lookup (token.tok_string);
token.tok_keyword = (KWWORDS) token.tok_symbol->sym_keyword;

if (sw_trace)
    ib_puts (token.tok_string);

return &token;
}

void CPR_get_text (
    TEXT	*buffer,
    TXT		text)
{
/**************************************
 *
 *      C P R _ g e t _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Write text from the scratch trace file into a buffer.
 *
 **************************************/
SLONG	start;
int	length;
TEXT	*p;

start = text->txt_position;
length = text->txt_length;

#ifndef mpexl
/* On PC-like platforms, '\n' will be 2 bytes.  The txt_position
 * will be incorrect for ib_fseek.  The position is not adjusted
 * just for PC-like platforms because, we use ib_fseek () and
 * ib_getc to position ourselves at the token position.
 * We should keep both character position and byte position
 * and use them appropriately. for now use ib_getc ()
 */

#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
if (ib_fseek (trace_file, 0L, 0))
#else
if (ib_fseek (trace_file, start, 0))
#endif
    {
    ib_fseek (trace_file, 0L, 2);
    CPR_error ("ib_fseek failed for trace file");
    }
#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
/* move forward to actual position */

while (start--)
    ib_getc (trace_file);
#endif

#else
if (ib_fclose (trace_file) ||
    !(trace_file = ib_fopen (trace_file_name, "r")) ||
    ib_fseek (trace_file, start, 0))
    {
    trace_file = ib_fopen (trace_file_name, "a");
    CPR_error ("ib_fseek failed for trace file");
    }
#endif

p = buffer;
while (length--)
    *p++ = ib_getc (trace_file);
#ifndef mpexl
ib_fseek (trace_file, (SLONG) 0, 2);
#else
ib_fclose (trace_file);
trace_file = ib_fopen (trace_file_name, "a");
#endif
}

void CPR_raw_read (void)
{
/**************************************
 *
 *	C P R _ r a w _ r e a d
 *
 **************************************
 *
 * Functional description
 *      A BASIC-specific function which resides here since it reads from
 *      the input file.  Look for a '\n' with no continuation character (&).
 *      Eat tokens until previous condition is satisfied.
 *      This function is used to "eat" an BASIC external function definition.
 *
 **************************************/
SSHORT	c;
SCHAR	token_string [MAXSYMLEN];
SCHAR	*p;
BOOLEAN	continue_char;

p = token_string;
continue_char = FALSE;

while (c = get_char (input_file))
    {
    position++;
    if ((classes [c] == CHR_WHITE) && sw_trace && token_string)
	{
	*p = 0;
	ib_puts (token_string);
	token_string [0] = 0;
	p = token_string;
	}
    else
	*p++ = c;

    if (c = '\n')
	{
	line++;
	line_position = 0;
	if (!continue_char)
	    return;
	continue_char = FALSE;
	}
    else
	{
	line_position++;
	if (classes [c] != CHR_WHITE)
	    continue_char = (KEYWORD (KW_AMPERSAND)) ? TRUE : FALSE;
	}
    }
}

void CPR_s_error (
    TEXT	*string)
{
/**************************************
 *
 *	C P R _ s _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Generate a syntax error.
 *
 **************************************/
TEXT	s [512];

sprintf (s, "expected %s, encountered \"%s\"", string, token.tok_string);
CPR_error (s);
PAR_unwind();
}

TXT CPR_start_text (void)
{
/**************************************
 *
 *      C P R _ s t a r t _ t e x t
 *
 **************************************
 *
 * Functional description
 *      Make the current position to save description text.
 *
 **************************************/
TXT     text;

text = (TXT) ALLOC (TXT_LEN);
text->txt_position = token.tok_position  - 1 ;

return text;
}

TOK CPR_token (void)
{
/**************************************
 *
 *	C P R _ t o k e n
 *
 **************************************
 *
 * Functional description
 *	Parse and return the next token.
 *	If the token is a charset introducer, gobble it, grab the
 *	next token, and flag that token as being in a non-default
 *	character set.
 *
 **************************************/
TOK	tok;
SYM	symbol;

tok = get_token();
if (tok && tok->tok_type == tok_introducer)
    {
    if (!(symbol = MSC_find_symbol (HSH_lookup (tok->tok_string+1), SYM_charset)))
	{
	TEXT	err_buffer [100];

	sprintf (err_buffer, "Character set not recognized: '%.50s'", 
		tok->tok_string);
	CPR_error (err_buffer);
	}
    tok = get_token();
    switch (sw_sql_dialect)
	{
	case 1:
            if (!(QUOTED(tok->tok_type))) 
	        CPR_error ("Can only tag quoted strings with character set");
	    else
	        tok->tok_charset = symbol;
	    break;

	default:
            if (!(SINGLE_QUOTED(tok->tok_type))) 
	        CPR_error ("Can only tag quoted strings with character set");
	    else
	        tok->tok_charset = symbol;
	    break;

	}
    }

return tok;
}

static BOOLEAN all_digits (
    UCHAR	*str1)
{
/**************************************
 *
 *	a l l _ d i g i t s
 *
 **************************************
 *
 * Functional description
 *	Return TRUE if the string consists entirely of digits.
 *
 **************************************/

for (; *str1; str1++)
    if (!(classes [*str1] & CHR_DIGIT))
	return FALSE;

return TRUE;
}

static arg_is_string (
    SLONG	argc,
    TEXT	**argvstring,
    TEXT	*errstring)
{
/**************************************
 *
 *	a r g _ i s _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Check the command line argument which follows
 *      a switch which requires a string argument.
 *      If there is a problem, explain and return.
 *
 **************************************/
TEXT  *str;

str = *++argvstring;

if (!argc || *str == '-')
    {
    ib_fprintf (ib_stderr, "%s", errstring);
    print_switches();
    return FALSE;
    }

return TRUE;
}

static SSHORT compare_ASCII7z (
    UCHAR	*str1,
    UCHAR	*str2)
{
/**************************************
 *
 *	c o m p a r e _ A S C I I 7 z
 *
 **************************************
 *
 * Functional description
 *	Compare two ASCII 7-bit strings, case insensitive.
 *	Strings are null-byte terminated.
 *	Return 0 if strings are equal,
 *	(negative) if str1 < str2 
 *	(positive) if str1 > str2
 *
 **************************************/

for (; *str1; str1++, str2++)
   if (UPPER7 (*str1) != UPPER7 (*str2))
	return (UPPER7 (*str1) - UPPER7 (*str2));

return 0;
}

static SLONG compile_module (
    SLONG	start_position)
{
/**************************************
 *
 *	c o m p i l e _ m o d u l e
 *
 **************************************
 *
 * Functional description
 *	Switches have been processed and files have been opened.
 *	Process a module and generate output.
 *
 **************************************/
SLONG	end_position;
REQ	request;
SCHAR	*p;

/* Reset miscellaneous pointers */

isc_databases = sw_databases;
requests = NULL;
events = NULL;
last_action = first_action = functions = NULL;

/* Position the input file and initialize various modules */

#ifndef mpexl
ib_fseek (input_file, start_position, 0);
#else
if (ib_fseek (input_file, start_position, 0) == -1)
    input_file = reposition_file (input_file, start_position);
#endif
input_char = input_buffer;

#if !(defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
trace_file = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, NULL_PTR);
#else
#ifndef __BORLANDC__
/* PC-like platforms can't delete a file that is open.  Therefore
   we will save the name of the temp file for later deletion. */

trace_file = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, trace_file_name);
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the GPRE
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (trace_file_name, p);
free (p);
trace_file = ib_fopen (trace_file_name, "w+");
if (!trace_file)
    trace_file = (IB_FILE*) -1;
#endif
#endif

if (trace_file == (IB_FILE*) -1)
    {
    trace_file = NULL;
    CPR_error ("Couldn't open scratch file");
    return 0;
    }

position = start_position;
MSC_init();
HSH_init();
PAR_init();
CMP_init();

/* Take a first pass at the module */

end_position = pass1();

/* finish up any based_ons that got deferred */

if (sw_language == lan_fortran)
    finish_based (first_action);

MET_fini (NULL);
PAR_fini();

if (errors)
    return end_position;

for (request = requests; request; request = request->req_next)
    CMP_compile_request (request);

#ifndef mpexl
ib_fseek (input_file, start_position, 0);
#else
if (ib_fseek (input_file, start_position, 0) == -1)
    input_file = reposition_file (input_file, start_position);
#endif
input_char = input_buffer;

if (!errors)
    pass2 (start_position);

return end_position;
}

static BOOLEAN file_rename (
    TEXT	*file_name,
    TEXT	*extension,
    TEXT	*new_extension)
{
/**************************************
 *
 *	f i l e _ r e n a m e
 *
 **************************************
 *
 * Functional description
 *	Add the appropriate extension to a file
 *	name, if there's not one already.  If
 *	the "appropriate" one is there and a
 *	new extension is given, use it.
 *
 **************************************/
TEXT	*p, *q, *terminator, *ext;

/* go to the end of the file name */

for (p = file_name; *p; p++)
    ;
terminator = p;

/* back up to the last extension (if any) */

#ifdef VMS
while ((p != file_name) && (*p != '.') && (*p != ']'))
#else
#if (defined WIN_NT || defined OS2_ONLY)
while ((p != file_name) && (*p != '.') && (*p != '/') && (*p != '\\'))
#else
while ((p != file_name) && (*p != '.') && (*p != '/'))
#endif
#endif
    p--;

/* 
 * There's a match and the file spec has no extension, 
 * so add extension.  
 */

if (*p != '.')		
    {
    while (*terminator++ = *extension++)
	;
    return TRUE;
    }

/*
 * There's a match and an extension.  If the extension in 
 * the table matches the one on the file, we don't want 
 * to add a duplicate.  Otherwise add it.
 */

ext = p;
for (q = extension; SAME (p, q); p++, q++)
    if (!*p)
	{
	if (new_extension)
	    while (*ext++ = *new_extension++)
		;
	return FALSE;
	}

#ifndef VMS
#ifndef mpexl
/* Didn't match extension, so add the extension */

while (*terminator++ = *extension++)
    ;
#endif    
#endif    

return TRUE;
}

static void finish_based (
    ACT	action)
{
/**************************************
 *
 *	f i n i s h _ b a s e d
 *
 **************************************
 *
 * Functional description
 *	Scan through the based_on actions
 *	looking for ones that were deferred
 *      because we didn't have a database yet.
 *
 *	Look at each action in turn, and if it's
 *	a based_on with a field name rather than a
 *	field block pointer, complete the name parse.
 *	If there's a database name, find the database,
 *	then the relation within the database, then
 *    	the field.  Otherwise, look through all databases
 *      for the relation.
 *
 **************************************/
DBB	db;
REL	relation;
FLD	field;
BAS	based_on;
SYM	symbol;
TEXT	s [128];

for (; action; action = action->act_rest)
    {
    if (action->act_type != ACT_basedon)
	continue;

    /* If there are no databases either on the command line or in
       this subroutine or main program, can't do a BASED_ON. */

    if (!isc_databases)
	{
	CPR_error ("No database defined.  Needed for a BASED_ON operation");
	continue;
	}

    based_on = (BAS) action->act_object;
    if (!based_on->bas_fld_name)
	continue;

    db = NULL;
    if (based_on->bas_db_name)
	{
	symbol = HSH_lookup (based_on->bas_db_name);
	for (; symbol; symbol = symbol->sym_homonym)
	    if (symbol->sym_type == SYM_database)
		break;

	if (symbol)
	    {
	    db = (DBB) symbol->sym_object;
	    relation = MET_get_relation (db, based_on->bas_rel_name, "");
	    if (!relation)
		{
		sprintf (s, "relation %s is not defined in database %s", 
		    based_on->bas_rel_name, based_on->bas_db_name);
		CPR_error (s);
		continue;
		}
	    field = MET_field (relation, based_on->bas_fld_name);
	    }
	else
	    {
	    if (based_on->bas_flags & BAS_ambiguous)
		{
		/* The reference could have been DB.RELATION.FIELD or
		   RELATION.FIELD.SEGMENT.  It's not the former.  Try
		   the latter. */

		based_on->bas_fld_name = based_on->bas_rel_name;
		based_on->bas_rel_name = based_on->bas_db_name;
		based_on->bas_db_name = NULL;
		based_on->bas_flags |= BAS_segment;
		}
	    else
		{
		sprintf (s, "database %s is not defined", based_on->bas_db_name);
		CPR_error (s);
		continue;
		}
	    }
	}

    if (!db)
	{
	field = NULL;
	for (db = isc_databases; db; db = db->dbb_next)
	    if (relation = MET_get_relation (db, based_on->bas_rel_name, ""))
		{
		if (field)
		    {
		    /* The field reference is ambiguous.  It exists in more
		       than one database. */

		    sprintf (s, "field %s in relation %s ambiguous", 
			based_on->bas_fld_name, based_on->bas_rel_name);
		    CPR_error (s);
		    break;
		    }
		field = MET_field (relation, based_on->bas_fld_name);
		}

	if (db)
	    continue;
	if (!relation && !field)
	    {
	    sprintf (s, "relation %s is not defined", based_on->bas_rel_name);
	    CPR_error (s);
	    continue;
	    }
	} 

    if (!field)
	{
	sprintf (s, "field %s is not defined in relation %s", 
		based_on->bas_fld_name, based_on->bas_rel_name);
	CPR_error (s); 
	continue;
	}

    if ((based_on->bas_flags & BAS_segment) && !(field->fld_flags & FLD_blob))
	{
	sprintf (s, "field %s is not a blob", field->fld_symbol->sym_string);
	CPR_error (s);
	continue;
	}

    based_on->bas_field = field;
    }
} 

static get_char (
    IB_FILE	*file)
{
/**************************************
 *
 *	g e t _ c h a r
 *
 **************************************
 *
 * Functional description
 *	Return a character to the input stream.
 *
 **************************************/

if (input_char != input_buffer)
    return (int) *--input_char;
else
    return ib_getc (file);
}

static BOOLEAN get_switches (
    int		argc,
    TEXT	**argv,
    IN_SW_TAB	in_sw_table,
    SW_TAB	sw_table,
    TEXT	**file_array)
{
/**************************************
 *
 *	g e t _ s w i t c h e s
 *
 **************************************
 *
 * Functional description
 *
 * Parse the input line arguments, saving
 * interesting switches in a switch table.
 * The first entry in the switch table is 
 * reserved for the language, and is set
 * later, even if specified here.
 *
 **************************************/
TEXT		*p, *q, *string;
IN_SW_TAB	in_sw_tab;
SW_TAB		sw_tab;
USHORT		in_sw;

/*
 * Read all the switches and arguments, acting only on those
 * that apply immediately, since we may find out more when
 * we try to open the file. 
 */

sw_tab = sw_table;

for (--argc; argc; argc--) 
    {
    string = *++argv;
    if (*string != '?')
	if (*string != '-')
	    {
	    if (!file_array[1])
		{
		if (!file_array[0])
		    file_array[0] = string;
		else
		    file_array[1] = string;
		continue;
		}
	    else
		{
		/* both input and output files have been defined, hence
		   there is an unknown switch */
		in_sw = IN_SW_GPRE_0;
		}
	    }
	else
	    {
 	    /* iterate through the switch table, looking for matches */

	    sw_tab++;
	    sw_tab->sw_in_sw = IN_SW_GPRE_0;
	    for (in_sw_tab = in_sw_table; q = in_sw_tab->in_sw_name; in_sw_tab++)
		{
		p = string + 1;

		/* handle orphaned hyphen case */

		if (!*p--)
		    break;

		/* compare switch to switch name in table */

		while (*p)
		    {
		    if (!*++p)
			sw_tab->sw_in_sw = in_sw_tab->in_sw;
		    if (UPPER7(*p) != *q++)
			break;
		    }
		/* end of input means we got a match.  stop looking */

		if (!*p)
		    break;
		}
	    in_sw = sw_tab->sw_in_sw;
	    }
	    
    /*
     * Check here for switches that affect file look ups
     * and -D so we don't lose their arguments.
     * Give up here if we find a bad switch.
     */

    if (*string == '?')
	in_sw = IN_SW_GPRE_0;

    switch (in_sw)
	{
	case IN_SW_GPRE_C : 
	    sw_language = lan_c;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_CXX : 
	    sw_language = lan_cxx;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_CPLUSPLUS :
	    sw_language = lan_cplusplus;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_G :
	    sw_language = lan_internal;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_F :
	    sw_language = lan_fortran;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_P :
	    sw_language = lan_pascal;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_X :
	    sw_external = TRUE;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_BAS : 
	    sw_language = lan_basic;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_PLI : 
	    sw_language = lan_pli;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_COB : 
	    sw_language = lan_cobol;
	    sw_tab--;
	    break;

	case IN_SW_GPRE_D :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -d requires database name:\n "))
		return FALSE;

	    file_array [2] = *++argv;
	    string = *argv;
	    if (*string == '=') 
		if (!arg_is_string (--argc, argv, "Command line syntax: -d requires database name:\n "))
		    return FALSE;
		else
		    file_array [2] = *++argv;
	    break;

	case IN_SW_GPRE_HANDLES :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -h requires handle package name\n"))
		return FALSE;
	    strcpy (ada_package, *++argv);
	    strcat (ada_package, ".");
	    break;

	case IN_SW_GPRE_SQLDA :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -sqlda requires NEW\n "))
		return FALSE;

	    if (**argv != 'n' || **argv != 'N')
	        {
    	        ib_fprintf (ib_stderr, "-sqlda :  Deprecated Feature: you must use XSQLDA\n ");
    	        print_switches();
    	        return FALSE;
	        }
	    break;

	case IN_SW_GPRE_SQLDIALECT :
	    {
	    int inp;
	    if (!arg_is_string (--argc, argv, "Command line syntax: -SQL_DIALECT requires value 1, 2 or 3 \n "))
		return FALSE;
	    ++argv;
	    inp = atoi (*argv);
	    if (inp <1 || inp > 3)
		{
    		ib_fprintf (ib_stderr, "Command line syntax: -SQL_DIALECT requires value 1, 2 or 3 \n ");
		print_switches();
    		return FALSE;
		}
	    else
		sw_sql_dialect = inp;
	    dialect_specified = 1;
	    break;
	    }

	case IN_SW_GPRE_Z :
	    if (!sw_version)
		ib_printf ("gpre version %s\n", GDS_VERSION);
	    sw_version = TRUE;
	    break;

	case IN_SW_GPRE_0 :
	    if (*string != '?')
		ib_fprintf (ib_stderr, "gpre: unknown switch %s\n", string);
	    print_switches();
	    return FALSE;

	case IN_SW_GPRE_USER :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -user requires user name string:\n "))
		return FALSE;
	    default_user = *++argv;
	    break;

	case IN_SW_GPRE_PASSWORD :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -password requires password string:\n "))
		return FALSE;
	    default_password = *++argv;
	    break;

	case IN_SW_GPRE_INTERP :
	    if (!arg_is_string (--argc, argv, "Command line syntax: -charset requires character set name:\n "))
		return FALSE;
	    default_lc_ctype = (TEXT *) *++argv;
	    break;

	}  
    }

sw_tab++;
sw_tab->sw_in_sw = IN_SW_GPRE_0;

return TRUE; 
}

static TOK get_token (void)
{
/**************************************
 *
 *	g e t _ t o k e n
 *
 **************************************
 *
 * Functional description
 *	Parse and return the next token.
 *
 **************************************/
SSHORT	c, c1, c2, next;
USHORT	peek, label;
TEXT	*p, *end;
UCHAR	class;
SYM	symbol;
SLONG	start_position;
int	start_line;

/* Save the information from the previous token */

prior_token = token;
prior_token.tok_position = last_position;

label = FALSE;
last_position = token.tok_position + token.tok_length + token.tok_white_space - 1;
start_line = line;
start_position = position;
token.tok_charset = NULL;

if (sw_sql && sw_language == lan_basic)
    classes ['\n'] = 0;

c = skip_white();

if (sw_sql && sw_language == lan_basic)
    classes ['\n'] = CHR_WHITE;

#ifdef BASIC
/* if BASIC language using SQL, '\n' = ';' unless preceeded by & */
if ((c == '\n') && (sw_language == lan_basic))
    {
    token.tok_string [0] = ';';
    token.tok_string [1] = 0;
    token.tok_type = tok_punct;
    token.tok_position = start_position + 1;
    token.tok_length = 0;
    token.tok_symbol = HSH_lookup (token.tok_string);
    token.tok_keyword = token.tok_symbol->sym_keyword;
    return &token;
    }
#endif

#ifdef COBOL
/* Skip over cobol line continuation characters */
if (sw_language == lan_cobol && !sw_ansi) 
    while (line_position == 1)
	{
	c = skip_white();
	start_line = line;
	}
#endif

/* Skip fortran line continuation characters */

if (sw_language == lan_fortran)
    {
    while (line_position == 6)
	{
	c = skip_white();
	start_line = line;
	}
    if (sw_sql && line != start_line)
	{
	return_char (c);
	token.tok_string [0] = ';';
	token.tok_string [1] = 0;
	token.tok_type = tok_punct;
	token.tok_length = 0;
	token.tok_white_space = 0;
	token.tok_position = start_position + 1;
	token.tok_symbol = HSH_lookup (token.tok_string);
	token.tok_keyword = (KWWORDS) token.tok_symbol->sym_keyword;
	return &token;
	}
    }

/* Get token rolling */

p = token.tok_string;
end = p + sizeof (token.tok_string);
*p++ = c;

if (c == EOF)
    {
    token.tok_symbol = NULL;
    token.tok_keyword = KW_none;
    return NULL;
    }

token.tok_position = position;
token.tok_white_space = 0;
class = classes [c];

if ((sw_language == lan_ada) && (c == '\''))
    {
    c1 = nextchar();
    c2 = nextchar();
    if (c2 != '\'')
	class = CHR_LETTER;
    return_char (c2);
    return_char (c1);
    }

if (sw_sql && (class & CHR_INTRODUCER))
    {
    while (classes [c = nextchar()] & CHR_IDENT)
	if (p < end)
	    *p++ = c;
    return_char (c);
    token.tok_type = tok_introducer;
    }
else if (class & CHR_LETTER)
    {
    while (TRUE)
	{
#if (! (defined JPN_EUC || defined JPN_SJIS) )
	while (classes [c = nextchar()] & CHR_IDENT)
	    *p++ = c;
#else
    p--;
    while (TRUE)
	{
	if (KANJI1 (c))
	    {
	    /* If it is a double byte kanji either EUC or SJIS
	       then handle both the bytes together */

	    *p++ = c;
	    c = nextchar();
	    if (!KANJI2 (c))
		{
		c = *(--p);
		break;
		}
	    else
		*p++ = c;
	    }
	else
	    {
#ifdef JPN_SJIS
	    if ((SJIS_SINGLE (c)) || (classes[c] & CHR_IDENT) )
#else
	    if (classes[c] & CHR_IDENT)
#endif
		*p++ = c;
	    else
		break;
	    }
	c = nextchar();
	}
#endif /* JPN_SJIS || JPN_EUC */
	if (c != '-' || sw_language != lan_cobol)
	    break;
	if (sw_language == lan_cobol && sw_ansi)
	    *p++ = c;
	else
	    *p++ = '_';
	}
    return_char (c);
    token.tok_type = tok_ident;
    }
else if (class & CHR_DIGIT)
    {
    if (sw_language == lan_fortran && line_position < 7)
  	label = TRUE;
    while (classes[c = nextchar()] & CHR_DIGIT)
	*p++ = c;
    if (label)
	{
	*p = 0;
	remember_label (token.tok_string);
	}
    if (c == '.')
	{
	*p++ = c;
	while (classes [c = nextchar()] & CHR_DIGIT)
	    *p++ = c;
	}
    if (!label && (c == 'E' || c == 'e'))
	{
	*p++ = c;
	c = nextchar();
	if (c == '+' || c == '-')
	    *p++ = c; 
	else
	    return_char (c);
	while (classes [c = nextchar()] & CHR_DIGIT)
	    *p++ = c;
	}
    return_char (c);
    token.tok_type = tok_number;
    }
else if ((class & CHR_QUOTE) || (class & CHR_DBLQUOTE))
    {
    token.tok_type = (class & CHR_QUOTE) ? tok_quoted : tok_dblquoted;
    for (;;)
	{
	next = nextchar();
	if (sw_language == lan_cobol && sw_ansi && next == '\n')
	    {
	    if (prior_line_position == 73)
		{
		/* should be a split literal */
		next = skip_white();
		if (next != '-' || line_position != 7)
		    {
		    CPR_error ("unterminated quoted string");
		    break;
		    }
		next = skip_white();
		if (next != c)
		    {
		    CPR_error ("unterminated quoted string");
		    break;
		    }
		next = nextchar();
		token.tok_white_space += line_position - 1;
		}
	    else
		{
		CPR_error ("unterminated quoted string");
		break;
		}
	    }
	else if (next == EOF || (next == '\n' && (p [-1] != '\\' || sw_sql)))
	    {
	    return_char (*p);

	    /*  Decrement, then increment line counter, for accuracy of 
		the error message for an unterminated quoted string. */

	    line--;
	    CPR_error ("unterminated quoted string");
	    line++;
	    break;
	    }

	/* If we can hold the literal do so, else assume it is in part
	   of program we do not care about */

	if (next == '\\' &&
	    !sw_sql &&
	    ((sw_language == lan_c) || (sw_language == lan_cxx)))
	    {
	    peek = nextchar();
	    if (peek == '\n')
		token.tok_white_space += 2;
	    else if (p < end)
		{
		*p++ = next;
		if (p < end)
		    *p++ = peek;
		}
	    continue;
	    }
	if (p < end)
	    *p++ = next;
	if (next == c)
	    /* If 2 quotes in a row, treat 2nd as literal - bug #1530 */
	    {
	    peek = nextchar();
	    if (peek != c)
		{
		return_char (peek);
		break;
		}
	    else
		token.tok_white_space++;
	    }
	}
    }
else if (c=='.' )
    {
    if (classes [c = nextchar()] & CHR_DIGIT)
	{
	*p++ = c;
        while (classes [c = nextchar()] & CHR_DIGIT)
            *p++ = c;
        if ((c == 'E' || c == 'e'))
	    {
	    *p++ = c;
	    c = nextchar();
	    if (c == '+' || c == '-')
	        *p++ = c; 
	    else
	        return_char (c);
	    while (classes [c = nextchar()] & CHR_DIGIT)
	        *p++ = c;
	    }
        return_char (c);
        token.tok_type = tok_number;
	}
    else
	{
        return_char (c);
        token.tok_type = tok_punct;
        *p++ = nextchar();
        *p = 0;
        if (!HSH_lookup (token.tok_string))
	    return_char (*--p);
	}
    }
else
    {
    token.tok_type = tok_punct;
    *p++ = nextchar();
    *p = 0;
    if (!HSH_lookup (token.tok_string))
	return_char (*--p);
    }
    
token.tok_length = p - token.tok_string;
*p++ = 0;
if (QUOTED(token.tok_type))
    {
    STRIP_QUOTES(token)
    /** If the dialect is 1 then anything that is quoted is
	a string. Don not lookup in the hash table to prevent 
	parsing confusion. 
    **/
    if (sw_sql_dialect !=1 )
        token.tok_symbol = symbol = HSH_lookup (token.tok_string);
    else
        token.tok_symbol = symbol = NULL;
    if (symbol && symbol->sym_type == SYM_keyword)
       token.tok_keyword = (KWWORDS) symbol->sym_keyword;
    else
       token.tok_keyword = KW_none;
    }
else if (sw_case)
    {
	if (!override_case)
	    {
            token.tok_symbol = symbol = HSH_lookup2 (token.tok_string);
            if (symbol && symbol->sym_type == SYM_keyword)
               token.tok_keyword = (KWWORDS) symbol->sym_keyword;
            else
               token.tok_keyword = KW_none;
	    }
	else
	    {
            token.tok_symbol = symbol = HSH_lookup (token.tok_string);
            if (symbol && symbol->sym_type == SYM_keyword)
               token.tok_keyword = (KWWORDS) symbol->sym_keyword;
            else
               token.tok_keyword = KW_none;
	    override_case = 0;
	    }
    }
else
    {
    token.tok_symbol = symbol = HSH_lookup (token.tok_string);
    if (symbol && symbol->sym_type == SYM_keyword)
       token.tok_keyword = (KWWORDS) symbol->sym_keyword;
    else
       token.tok_keyword = KW_none;
    }

/** Take care of GDML context variables. Context variables are inserted 
into the hash table as it is. There is no upper casing of the variable 
name done. Hence in all likelyhood we might have missed it while looking it
up if -e switch was specified. Hence
IF symbol is null AND it is not a quoted string AND -e switch was specified
THEN search again using HSH_lookup2().
**/
if ((token.tok_symbol == NULL) && (!QUOTED(token.tok_type)) && sw_case )
    {
    token.tok_symbol = symbol = HSH_lookup2 (token.tok_string);
    if (symbol && symbol->sym_type == SYM_keyword)
       token.tok_keyword = (KWWORDS) symbol->sym_keyword;
    else
       token.tok_keyword = KW_none;
    }

#ifdef BASIC
if (sw_language == lan_basic)
    {
    if ((int) token.tok_keyword == (int) KW_REM)
	for (CPR_token; (token.tok_type != tok_number) || !token.tok_first;
	     CPR_token())
	     ;
    if ((int) token.tok_keyword == (int) KW_BACK_SLASH)
	{
	/* if BASIC, treat a '\' as a ';' */

	token.tok_string [0] = ';';
	token.tok_string [1] = 0;
	token.tok_type = tok_punct;
	token.tok_length = 0;
	token.tok_position = start_position + 1;
	token.tok_symbol = HSH_lookup (token.tok_string);
	token.tok_keyword = token.tok_symbol->sym_keyword;
	}
    }
#endif

/* for FORTRAN, make note of the first token in a statement */

token.tok_first = first_position;
first_position = FALSE;

if (sw_trace)
    ib_puts (token.tok_string);

if (sw_language == lan_basic && (int)token.tok_keyword == (int)KW_AMPERSAND)
    {
    c = skip_white();
    return_char (c);
    return (CPR_token ());
    }

return &token;
}

static nextchar (void)
{
/**************************************
 *
 *	n e x t c h a r
 *
 **************************************
 *
 * Functional description
 *	Get the next character from the input stream.
 *      Also, for Fortran, mark the beginning of a statement
 *
 **************************************/
SSHORT	c;

position++;
line_position++;
if ((c = get_char (input_file)) == '\n')
    {
    line++;
    prior_line_position = line_position;
    line_position = 0;
    } 

/* For silly fortran, mark the first token in a statement so
   we can decide to start the database field substitution string
   with a continuation indicator if appropriate. */

if (line_position == 1)
    {
    first_position = TRUE;

    /* If the first character on a Fortran line is a tab, bump up the
       position indicator. */

    if (sw_language == lan_fortran && c == '\t')
	line_position = 7;
    }
 
/* if this is a continuation line, the next token is not
   the start of a statement. */

if (sw_language == lan_fortran && line_position == 6 && c != ' ' && c != '0')
    first_position = FALSE;

#ifdef COBOL
if (sw_language == lan_cobol &&
    (!sw_ansi && line_position == 1 && c == '-') ||
    (sw_ansi && line_position == 7 && c == '-'))
    first_position = FALSE;
#endif

if (position > traced_position)
    {
    traced_position = position;
    ib_fputc (c, trace_file);
    }

return c;
}

static SLONG pass1 (void)
{
/**************************************
 *
 *	p a s s 1
 *
 **************************************
 *
 * Functional description
 *	Make first pass at input file.  This involves
 *	passing thru tokens looking for keywords.  When
 *	a keyword is found, try to parse an action.  If
 *	the parse is successful (an action block is returned)
 *	link the new action into the system data structures
 *	for processing on pass 2.
 *
 **************************************/
ACT	action;
SLONG	start;

while (CPR_token())
    while (token.tok_symbol)
	{
	start = token.tok_position;
	if (action = PAR_action())
	    {
	    action->act_position = start;
	    if (!(action->act_flags & ACT_back_token))
		action->act_length = last_position - start;
	    else
		action->act_length = prior_token.tok_position + prior_token.tok_length - 1 - start;
	    if (first_action)
		last_action->act_rest = action;
	    else
		first_action = action;

	    /* Allow for more than one action to be generated by a token. */

	    do {
	    	last_action = action;
		if (action = action->act_rest)
		    {
		    if (action->act_type == ACT_database)
		     	{
			/* CREATE DATABASE has two actions the second one
			   is do generate global decl at the start of the
			   program file. */

			last_action->act_rest = NULL;
			action->act_rest = first_action;
			first_action = action;
			action->act_position = -1;
			action->act_length = -1;
			break;
			}
		   else
			{
			action->act_position = last_action->act_position;
			action->act_length = 0;
			}
		    }
	    } while (action);
	    if (last_action->act_flags & ACT_break)
		return last_position;
	    if (!token.tok_length && ((int) token.tok_keyword == (int) KW_SEMI_COLON))
		break;
	    }
	}

if (isc_databases &&
    (isc_databases->dbb_flags & DBB_sqlca) &&
    !isc_databases->dbb_filename)
    CPR_error ("No database specified");

return 0;
}

static void pass2 (
    SLONG	start_position)
{
/**************************************
 *
 *	p a s s 2
 *
 **************************************
 *
 * Functional description
 *	Make a second pass thru the input file turning actions into
 *	comments, substituting text for actions, and generating the
 *	output file.
 *
 **************************************/
SSHORT	c, d, prior, comment_start_len, to_skip;
SLONG	column, start;
SLONG	i, line, line_pending, current;
ACT	action;
BOOLEAN suppress_output, continue_flag, sw_block_comments;

c = 0;

suppress_output = FALSE;
sw_block_comments = sw_language == lan_c ||
		    sw_language == lan_cxx || 
		    sw_language == lan_pascal || 
		    sw_language == lan_pli;

/* Put out a distintive module header */

if (sw_language != lan_basic)
    if (!sw_first++)
	{
	for (i = 0; i < 5; i++)
	    ib_fprintf (out_file, 
		"%s********** Preprocessed module -- do not edit **************%s\n",
		comment_start, comment_stop);
	ib_fprintf (out_file, 
	    "%s**************** gpre version %s *********************%s\n",
	    comment_start, GDS_VERSION, comment_stop);
    
	}

if ((sw_language == lan_ada) && (ada_flags & ADA_create_database))
    ib_fprintf (out_file, "with unchecked_conversion;\nwith system;\n");

/*
if (sw_lines)
    ib_fprintf (out_file, "#line 1 \"%s\"\n", file_name);
*/

line = 0;
if (sw_lines)
    line_pending = TRUE;
else
    line_pending = FALSE;
current = 1 + start_position;
column = 0;

comment_start_len = strlen (comment_start);
to_skip = 0;

/* Dump text until the start of the next action, then process the action. */

for (action = first_action; action; action = action->act_rest)
    {
    /* Dump text until the start of the next action.  If a line marker
       is pending and we see an end of line, dump out the marker. */

#ifdef mpexl
    /* can't predict dangling terminators until we get a little brighter
       until then make sure they are not in illegal position */

    if ((c = get_char (input_file)) == '.')
	ib_fputs ("           ", out_file);
    return_char (c);
#endif

    for (; current < action->act_position; current++)
	{
	c = get_char (input_file);
	if (c == EOF)
	    {
	    CPR_error ("internal error -- unexpected EOF between actions");
	    return;
	    }
	if (c == '\n' || !line)
	    {
	    line++;
	    if (line_pending)
		{
		if (line == 1)	
		ib_fprintf (out_file, "#line %ld \"%s\"\n", line, file_name);
		else
		ib_fprintf (out_file, "\n#line %ld \"%s\"", line, file_name);
		
		line_pending = FALSE;
		}
	    if (line == 1 && c=='\n')
		line++;
	    column = -1;
	    }
	ib_putc (c, out_file);
	if (c == '\t')
	    column = (column + 8) & ~7;
	else
	    column++;
	}

    /* Determine if this action is one which requires line continuation
       handling in certain languages.  */

    continue_flag = (action->act_type == ACT_variable) ||
		    (action->act_type == ACT_segment) ||
		    (action->act_type == ACT_segment_length) ||
		    (action->act_type == ACT_title_text) ||
		    (action->act_type == ACT_title_length) ||
		    (action->act_type == ACT_terminator) ||
		    (action->act_type == ACT_entree_text) ||
		    (action->act_type == ACT_entree_length) ||
		    (action->act_type == ACT_entree_value);

    /* Unless the action is purely a marker, insert a comment initiator
       into the output stream. */

    start = column;
    if (!(action->act_flags & ACT_mark)) 
	{
	if (sw_language == lan_fortran)
	    {
	    ib_fputc ('\n', out_file);
	    ib_fputs (comment_start, out_file);
	    }
	else if (sw_language == lan_cobol)
	    if (continue_flag)
		suppress_output = TRUE;
	    else
		{
		ib_fputc ('\n', out_file);
		ib_fputs (comment_start, out_file);
		to_skip = (column < 7) ? comment_start_len - column : 0;
		column = 0;
		}
	else if (sw_language == lan_basic)
	    {
	    if (!continue_flag)
		ib_fputc ('\n', out_file);
	    else if (!(action->act_flags & ACT_first))
		    ib_fputs (" &\n", out_file);
	    ib_fputs (comment_start, out_file);
	    }
	else
	    ib_fputs (comment_start, out_file);
	}

    /* Next, dump the text of the action to the output stream. */

    for (i = 0; i <= action->act_length; i++, current++)
	{
	if (c == EOF)
	    {
	    CPR_error ("internal error -- unexpected EOF in action");
	    return;
	    }
	prior = c;
	c = get_char (input_file);
	if (!suppress_output)
	    {
	    /* close current comment to avoid nesting comments */
	    if (sw_block_comments && !(action->act_flags & ACT_mark) &&
		      c == comment_start [0])
		{
		return_char ((d = get_char (input_file)));
		if (d == comment_start [1])
		    ib_fputs (comment_stop, out_file);
		}
	    if (sw_language != lan_cobol || !sw_ansi || c == '\n' || to_skip-- <= 0)
		ib_putc (c, out_file);
	    if (c == '\n')
		{
		line++;
		if ((sw_language == lan_fortran) ||
		    (sw_language == lan_basic) ||
		    (sw_language == lan_ada) ||
		    (sw_language == lan_cobol))
		    {
		    ib_fputs (comment_start, out_file);
		    to_skip = (column < 7) ? comment_start_len - column : 0;
		    column = 0;
		    }
		}

	    /* reopen our comment at end of user's comment */

	    if (sw_block_comments && !(action->act_flags & ACT_mark) && 
		prior == comment_stop [0] && c == comment_stop [1])
		ib_fputs (comment_start, out_file);
	    }
	}

    /* Unless action was purely a marker, insert a comment terminator. */

    if (!(action->act_flags & ACT_mark) && !suppress_output)
	{
	ib_fputs (comment_stop, out_file);
	if ((sw_language == lan_fortran) ||
	    (sw_language == lan_cobol))
	    ib_fputc ('\n', out_file);
	if (sw_language == lan_basic)
	    if (!continue_flag)
		ib_fputc ('\n', out_file);
	    else
		ib_fputs (" &\n", out_file);

	}

    suppress_output = FALSE;
    (*gen_routine) (action, start);
    if (action->act_type == ACT_routine && 
	!action->act_object &&
	((sw_language == lan_c) || (sw_language == lan_cxx)))
	continue;

    if (action->act_flags & ACT_break)
	return;

    if (sw_lines)
	line_pending = TRUE;
    column = 0;
    to_skip = 0;
    }

/* We're out of actions -- dump the remaining text to the output stream. */

if (!line && line_pending)
    {
    ib_fprintf (out_file, "#line 1 \"%s\"\n", file_name);	
    line_pending = FALSE;
    }


while ((c = get_char (input_file)) != EOF)
    {
    if (c == '\n' && line_pending)
	{
	ib_fprintf (out_file, "\n#line %ld \"%s\"", line + 1, file_name);
	line_pending = FALSE;
	}
    if (c == EOF)
	{
	CPR_error ("internal error -- unexpected EOF in tail");
	return;
	}
    ib_putc (c, out_file);
    }

/* Last but not least, generate any remaining functions */

for (; functions; functions = functions->act_next)
    (*gen_routine) (functions, 0);
}

static void print_switches (void)
{
/**************************************
 *
 *    p r i n t _ s w i t c h e s
 *
 **************************************
 *
 * Functional description
 *      Print out the switch table as an
 *      aid to those who have forgotten or are fishing
 *      
 **************************************/
IN_SW_TAB	in_sw_tab;

ib_fprintf (ib_stderr, "\tlegal switches are:\n");
for (in_sw_tab = gpre_in_sw_table; in_sw_tab->in_sw; in_sw_tab++)
    if (in_sw_tab->in_sw_text)
	ib_fprintf (ib_stderr,"%s%s\n", in_sw_tab->in_sw_name,
	    in_sw_tab->in_sw_text);
}

static void remember_label (
    TEXT	*label_string)
{
/**************************************
 *
 *	r e m e m b e r _ l a b e l
 *
 **************************************
 *
 * Functional description
 *	Set a bit in the label vector indicating
 *      that a label has been used.  If the label
 *	is bigger than the vector, punt.
 *      
 **************************************/
UCHAR	target_byte;
SLONG	label;

label = atoi (label_string);
if (label < 8192)
    {
    target_byte = label & 7;
    label >>= 3;
    fortran_labels [label] |= 1 << target_byte;
    }
}

#ifdef mpexl
static IB_FILE *reposition_file (
    IB_FILE	*input_file,
    SLONG	start_position)
{
/**************************************
 *
 *	r e p o s i t i o n _ f i l e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	file_name [256], *p;
SLONG	position;

FFILEINFO (_mpe_fileno (ib_fileno (input_file)),
    INF_FID, file_name,
    ITM_END);
for (p = file_name; p < file_name + sizeof (file_name) - 1 && *p && *p != ' '; p++)
    ;
*p = 0;
input_file = ib_freopen (file_name, FOPEN_READ_TYPE, input_file);
if (position = start_position)
    do get_char (input_file); while (--position);

return input_file;
}
#endif

static void return_char (
    SSHORT	c)
{
/**************************************
 *
 *	r e t u r n _ c h a r
 *
 **************************************
 *
 * Functional description
 *	Return a character to the input stream.
 *
 **************************************/

--position;
--line_position;

/* note putting back a new line results in incorrect line_position value */

if (c == '\n')
    {
    --line;
    }

*input_char++ = c;
}

static SSHORT skip_white (void)
{
/**************************************
 *
 *	s k i p _ w h i t e
 *
 **************************************
 *
 * Functional description
 *	Skip over white space and comments in input stream
 *
 **************************************/
SSHORT	c, c2, next, class;

while (TRUE)
    {
    if ((c = nextchar()) == EOF)
	return c;

    c = c & 0xff;

    /* skip Fortran comments */

    if (sw_language == lan_fortran  &&
	line_position == 1 &&
	(c  == 'C' || c == 'c' || c == '*'))
	{
	while ((c = nextchar()) != '\n' && c != EOF)
	    ;
	continue;
 	}

#ifdef COBOL
    /* skip sequence numbers when ansi COBOL */

    if (sw_language == lan_cobol && sw_ansi)
	{
	while (line_position < 7 && (c = nextchar()) != '\n' && c != EOF)
	    ;
	}

    /* skip COBOL comments and conditional compilation */

    if (sw_language == lan_cobol &&
	(!sw_ansi && line_position == 1 &&
	    (c  == 'C' || c == 'c' || c == '*' || c == '/' || c == '\\') ||
	(sw_ansi && line_position == 7 && c != '\t' && c != ' ' && c != '-')))
	{
	while ((c = nextchar()) != '\n' && c != EOF)
	    ;
	continue;
 	}
#endif

    class = classes [c];
    if (class & CHR_WHITE)
	continue;

    /* skip in-line SQL comments */

    if (sw_sql && (c == '-'))
	{
	c2 = nextchar();
	if (c2 != '-')
	    return_char (c2);
	else
	    {
	    while ((c = nextchar()) != '\n' && c != EOF)
		;
	    last_position = position - 1;
	    continue;
	    }
	}

    /* skip C and PL/I comments */

    if (c == '/' && 
	(sw_language == lan_c || 
	 sw_language == lan_internal ||
	 sw_language == lan_cxx ||
	 sw_language == lan_pli))
	{
	if ((next = nextchar()) != '*')
	    {
	    if (sw_language == lan_cxx && next == '/')
		{
		while ((c = nextchar()) != '\n' && c != EOF)
		    ;
		continue;
 		}
	    return_char (next);
	    return c;
	    }
	c = nextchar();
	while ((next = nextchar()) != EOF && !(c == '*' && next == '/'))
	    c = next;
	continue;
	}

#ifndef sun
#ifdef APOLLO
    /* skip fortran embedded comments- Apollo case */

    if (c == '{' && sw_language == lan_fortran)
	{
	while ((c = nextchar()) != '\n' && c != EOF)
	    ;
	continue;
	}
#else
    /* skip fortran embedded comments on VMS or hpux or sgi */

    if (c == '!' && ((sw_language == lan_fortran) || (sw_language == lan_basic)))
	{
	/* If this character is a '!' followed by a '=', this is an
	   Interbase 'not equal' operator, not a Fortran comment.
	   Bug #307.  mao 6/14/89  */

	if ((c2 = nextchar()) == '=')
	    {
	    return_char (c2);
	    return c;
	    }
	else
	    {
	    if ((c = c2) != '\n' && c != EOF)
		while ((c = nextchar()) != '\n' && c != EOF)
		    ;
	    continue;
	    }
	}
#endif
#endif

    if (c == '-' && (sw_sql || sw_language == lan_ada))
	{
	if ((next = nextchar()) != '-')
	    {
	    return_char (next);
	    return c;
	    }
	while ((c = nextchar()) != EOF && c != '\n')
	    ;
	continue;
	}

    /* skip PASCAL comments - both types */

    if (c == '{' && 
	sw_language == lan_pascal)
	{
	while ((c = nextchar()) != EOF && c != '}')
	    ;
	continue;
	}

    if (c == '(' && sw_language == lan_pascal)
	{
	if ((next = nextchar()) != '*')
	    {
	    return_char (next);
	    return c;
	    }
	c = nextchar();
	while ((next = nextchar()) != EOF && !(c == '*' && next == ')'))
	    c = next;
	continue;
	}

    break;
    }

return c;
}
