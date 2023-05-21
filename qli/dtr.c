/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		dtr.c
 *	DESCRIPTION:	Top level driving module
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

#define QLI_MAIN
#include "../jrd/ib_stdio.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../qli/compile.h"
#include "../jrd/perf.h"
#include "../jrd/license.h"
#include "../jrd/gds.h"
#include "../qli/all_proto.h"
#include "../qli/compi_proto.h"
#include "../qli/err_proto.h"
#include "../qli/exe_proto.h"
#include "../qli/form_proto.h"
#include "../qli/expan_proto.h"
#include "../qli/gener_proto.h"
#include "../qli/help_proto.h"
#include "../qli/lex_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/parse_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/perf_proto.h"

#ifdef VMS
#define STARTUP_FILE	"QLI_STARTUP"
#endif

#ifdef mpexl
#define STARTUP_FILE	"qlistart.pub"
#endif

#ifdef APOLLO
#define STARTUP_FILE	"~/user_data/qli_startup"
#include "../jrd/termtype.h"
#include "/sys/ins/base.ins.c"
#include "/sys/ins/pad.ins.c"
#include "/sys/ins/error.ins.c"
#endif

#ifndef STARTUP_FILE
#define STARTUP_FILE    "HOME"	/* Assume its Unix */
#endif

#ifdef APOLLO
std_$call 	gds__set_debug();
#endif

#ifdef mpexl
#include "../jrd/mpexl.h"

#define TRP_INTEGER_OVER	(1 << 4)
#define TRP_INTEGER_DIVIDE	(1 << 1)
#define TRP_FLOAT_OVER1		(1 << 16)
#define TRP_FLOAT_OVER2		(1 << 5)
#define TRP_FLOAT_OVER3		(1 << 3) 
#define TRP_FLOAT_UNDER1	(1 << 15)
#define TRP_FLOAT_UNDER2	(1 << 6)
#define TRP_FLOAT_UNDER3	(1 << 2)
#define TRP_FLOAT_DIVIDE1	(1 << 17)
#define TRP_FLOAT_DIVIDE2	(1 << 7)
#define TRP_FLOAT_DIVIDE3	(1 << 0)

static USHORT		enabled_traps;
#endif

#ifndef SIGQUIT
#define SIGQUIT		SIGINT
#define SIGPIPE		SIGINT
#endif

/* Program wide globals */

int		*QLI_env;		/* Error return environment */

TEXT		*QLI_error;
USHORT		sw_verify, sw_trace, sw_forms, sw_buffers;
USHORT		QLI_lines = 60, QLI_prompt_count, QLI_reprompt, QLI_name_columns = 0;

/* Let's define the default number of columns on a machine by machine basis */

#ifdef APOLLO
USHORT		QLI_columns = 256;
#else

#ifdef VMS
USHORT		QLI_columns = 80;
#else
USHORT		QLI_columns = 80;
#endif

#endif

extern TEXT	*QLI_prompt;

static void		enable_signals (void);
static USHORT		process_statement (USHORT);
#ifndef mpexl
static void CLIB_ROUTINE signal_arith_excp (USHORT, USHORT, USHORT);
#else
static void CLIB_ROUTINE signal_arith_excp (struct {SLONG, SLONG, SLONG, SLONG} *);
#endif
static void CLIB_ROUTINE signal_quit (void);
static BOOLEAN		yes_no (USHORT, TEXT *);

typedef struct answer_t  {
    TEXT	answer [30];
    BOOLEAN	value;
} *ANS ;

static yes_no_loaded = 0;
static struct answer_t	answer_table []= {
    "",	FALSE,		/* NO	*/
    "",	TRUE,		/* YES	*/
    NULL, 0,
};

int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Top level routine.  
 *
 **************************************/
TEXT	**arg_end, *p, *q, *r, *end, c, *startup_file, *application_file;
SCHAR    home_directory[256];
PLB	temp;
USHORT	flush_flag, banner_flag, version_flag, got_started;
#ifdef VMS           
USHORT	vms_tryagain_flag;
#endif
SLONG	debug_value;
jmp_buf	env;
int	status;

#ifdef __OVERLAY__
_OvrInitEms(0,0,0);
#endif

#ifdef APOLLO
if ((p = getenv ("TERM")) &&
    !strcmp (p, "vt100"))
    QLI_columns = 80;
#endif

/* Look at options, if any */

startup_file = STARTUP_FILE;

#ifdef UNIX
/* If a Unix system, get home directory from environment */
startup_file = getenv("HOME");
if (startup_file == NULL)
    {
    startup_file = ".qli_startup";
    }
else
    {
    strcpy(home_directory, startup_file);
    strcat(home_directory, "/.qli_startup");
    startup_file = home_directory;
    }
#endif

application_file = NULL;
ALLQ_init();
LEX_init();
version_flag = flush_flag = FALSE;
banner_flag = TRUE;
sw_buffers = 0;
strcpy (QLI_prompt_string, "QLI> ");
strcpy (QLI_cont_string, "CON> ");
QLI_prompt = QLI_prompt_string;
QLI_matching_language = 0;
QLI_default_user[0] = 0;
QLI_default_password[0] = 0;
QLI_charset[0] = 0;

#ifdef DEVBUILD
QLI_hex_output = 0;
#endif

#if (defined JPN_EUC  || defined JPN_SJIS)
QLI_interp = HOST_INTERP;
#endif /* (defined JPN_EUC  || defined JPN_SJIS) */


#ifdef VMS
argc = VMS_parse (&argv, argc);
#endif

for (arg_end = argv + argc, argv++; argv < arg_end;)
    {
    p = *argv++;
    if (*p++ != '-')
	{
	banner_flag = FALSE;
	LEX_pop_line();
	LEX_push_string (p - 1);
	continue;
	}
    while (c = *p++)
	switch (UPPER (c))
	    {
	    case 'A':
		if (argv >= arg_end)
                    {
		    ERRQ_msg_put (23, NULL, NULL, NULL, NULL, NULL); /* Msg23 Please retry, supplying an application script file name  */
		    exit (FINI_ERROR);
                    }

		application_file = *argv++;
		break;

	    case 'B':
		if (argv < arg_end && **argv != '-')
		    sw_buffers = atoi (*argv++);
		break;

	    case 'I':
		if (argv >= arg_end || **argv == '-')
                    startup_file = NULL;
                else
		    startup_file = *argv++;
		break;

	    case 'N':
		banner_flag = FALSE;
		break;

	    case 'P':
		if (argv >= arg_end || **argv == '-')
                    break;
	        r = QLI_default_password;
		end = r + sizeof (QLI_default_password) - 1;
		for (q = *argv++; *q && r < end;)
		    *r++ = *q++;
		*r = 0;
		break;

	    case 'T':
		sw_trace = TRUE;
		break;

	    case 'U':
		if (argv >= arg_end || **argv == '-')
                    break;
	        r = QLI_default_user;
		end = r + sizeof (QLI_default_user) - 1;
		for (q = *argv++; *q && r < end;)
		    *r++ = *q++;
		*r = 0;
		break;

	    case 'V':
		sw_verify = TRUE;
		break;

#ifndef PC_PLATFORM
	    case 'X':               
		debug_value = 1;
		gds__set_debug (debug_value);
		break;
#endif

	    /* This switch's name is arbitrary; since it is an internal 
	       mechanism it can be changed at will */

	    case 'Y':
		QLI_trace = TRUE;
		break;

	    case 'Z':
		version_flag = TRUE;
		break;

	    default:
		ERRQ_msg_put (469, (TEXT*) c, NULL, NULL, NULL, NULL); /* Msg469 qli: ignoring unknown switch %c */
		break;
	    }
    }

enable_signals();

if (banner_flag)
    ERRQ_msg_put (24, NULL, NULL, NULL, NULL, NULL); /* Msg24 Welcome to QLI Query Language Interpreter */

if (version_flag)
    ERRQ_msg_put (25, GDS_VERSION, NULL, NULL, NULL, NULL); /* Msg25 qli version %s */

if (application_file)
    LEX_push_file (application_file, TRUE);

if (startup_file)                         
    LEX_push_file (startup_file, FALSE);

#ifdef VMS           
vms_tryagain_flag = FALSE;
if (startup_file)                         
    vms_tryagain_flag = LEX_push_file (startup_file, FALSE);

/* If default value of startup file wasn't altered by the use of -i,
   and LEX returned FALSE (above), try the old logical name, QLI_INIT */

if (!vms_tryagain_flag && startup_file && !(strcmp (startup_file, STARTUP_FILE)))
    LEX_push_file ("QLI_INIT", FALSE); 
#endif                    

for (got_started = 0; !got_started;)
    {
    got_started = 1;
    if (setjmp (env))
        {
        /* try again */

        got_started = 0;
        ERRQ_pending();
        }
    else
	{
	QLI_env = env;
	PAR_token();
	}
    }
QLI_env = NULL;
QLI_error = NULL;

/* Loop until end of file or forced exit */

while (QLI_line)
    {
    temp = QLI_default_pool = ALLQ_pool ();
    flush_flag = process_statement (flush_flag);
    ERRQ_pending();
    if (sw_forms)
	FORM_reset ();
    ALLQ_rlpool (temp);
    }

FORM_fini();
HELP_fini();
MET_shutdown();
LEX_fini();
ALLQ_fini();
#ifdef DEBUG_GDS_ALLOC
/* Report any memory leaks noticed.  
 * We don't particularly care about QLI specific memory leaks, so all
 * QLI allocations have been marked as "don't report".  However, much
 * of the test-base uses QLI so having a report when QLI finishes
 * could find leaks within the engine.
 */
gds_alloc_report (0, __FILE__, __LINE__);
#endif
exit (FINI_OK);
}

static void enable_signals (void)
{
/**************************************
 *
 *	e n a b l e _ s i g n a l s
 *
 **************************************
 *
 * Functional description
 *	Enable signals.
 *
 **************************************/
SLONG	mask;
void	(*prev_handler)();

#ifndef mpexl
signal (SIGQUIT, signal_quit);
signal (SIGINT, signal_quit);
signal (SIGPIPE, signal_quit);
signal (SIGFPE, signal_arith_excp);
#else
RESETCONTROL();
XCONTRAP ((int) signal_quit, (int*) &prev_handler);
if (!enabled_traps)
    {
    enabled_traps = TRUE;
    HPENBLTRAP (0, &mask);
    mask |= TRP_INTEGER_OVER | TRP_INTEGER_DIVIDE |
	TRP_FLOAT_OVER1 | TRP_FLOAT_OVER2 | TRP_FLOAT_OVER3 |
	TRP_FLOAT_UNDER1 | TRP_FLOAT_UNDER2 | TRP_FLOAT_UNDER3 |
	TRP_FLOAT_DIVIDE1 | TRP_FLOAT_DIVIDE2 | TRP_FLOAT_DIVIDE3;
    HPENBLTRAP (mask, &mask);
    mask = TRP_INTEGER_OVER | TRP_INTEGER_DIVIDE |
	TRP_FLOAT_OVER1 | TRP_FLOAT_OVER2 | TRP_FLOAT_OVER3 |
	TRP_FLOAT_UNDER1 | TRP_FLOAT_UNDER2 | TRP_FLOAT_UNDER3 |
	TRP_FLOAT_DIVIDE1 | TRP_FLOAT_DIVIDE2 | TRP_FLOAT_DIVIDE3;
    XARITRAP (mask, (int) signal_arith_excp, &mask, (int*) &prev_handler);
    }
#endif
}

static USHORT process_statement (
    USHORT	flush_flag)
{
/**************************************
 *
 *	p r o c e s s _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Parse, compile, and execute a single statement.  If an input flush
 *	is required, return TRUE (or status), otherwise return FALSE.
 *
 **************************************/
SYN	syntax_tree;
BLK	expanded_tree, execution_tree;
DBB	dbb;
PERF	statistics;
TEXT	buffer [512], report [256], *p;
int	status;
jmp_buf	env;

/* Clear database active flags in preparation for a new statement */

QLI_abort = FALSE;
execution_tree = NULL;

for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
    dbb->dbb_flags &= ~DBB_active;

/* If the last statement wrote out anything to the terminal, skip a line */

if (QLI_skip_line)
    {
    ib_printf ("\n");
    QLI_skip_line = FALSE;
    }

/* Enable signal handling for the next statement.  Each signal will
   be caught at least once, then reset to allow the user to really
   kill the process */

enable_signals();

/* Enable error unwinding and enable the unwinding environment */

status = setjmp (env);

if (status)
    {
    GEN_release();
    return status;
    }

QLI_env = env;

/* Set up the appropriate prompt and get the first significant token.  If
   we don't get one, we're at end of file */

QLI_prompt = QLI_prompt_string;

/* This needs to be done after setting QLI_prompt to prevent
 * and infinite loop in LEX/next_line.
 */
/* If there was a prior syntax error, flush the token stream */

if (flush_flag)
    LEX_flush ();

while (QLI_token->tok_keyword == KW_SEMI)
    LEX_token();

PAR_real();

if (!QLI_line)
    return FALSE;

EXEC_poll_abort();

/* Mark the current token as starting the statement.  This is allows
   the EDIT command to find the last statement */

LEX_mark_statement ();

/* Change the prompt string to the continuation prompt, and parse
   the next statement */

QLI_prompt = QLI_cont_string;

if (!(syntax_tree = PARQ_parse ()))
    return FALSE;

EXEC_poll_abort();

/* If the statement was EXIT, force end of file on command input */

if (syntax_tree->syn_type == nod_exit)
    {
    QLI_line = NULL;
    return FALSE;
    }

/* If the statement was quit, ask the user if he want to rollback */

if (syntax_tree->syn_type == nod_quit)
    {
    QLI_line = NULL;
    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
	if ((dbb->dbb_transaction) && (dbb->dbb_flags & DBB_updates))
	    if (yes_no (460, dbb->dbb_symbol->sym_string))  /* Msg460 Do you want to rollback updates for <dbb>? */
		MET_transaction (nod_rollback, dbb);
	    else
		MET_transaction (nod_commit, dbb);
    return FALSE;
    }

/* Expand the statement.  It will return NULL is the statement was
   a command.  An error will be unwound */

if (!(expanded_tree = EXP_expand (syntax_tree)))
    return FALSE;

/* Compile the statement */

if (!(execution_tree = CMPQ_compile (expanded_tree)))
    return FALSE;

/* Generate any BLR needed to support the request */

if (!GEN_generate (execution_tree))
    return FALSE;

if (QLI_statistics)
    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
	if (dbb->dbb_flags & DBB_active)
	    {
	    if (!dbb->dbb_statistics)
		{
		dbb->dbb_statistics = (int*) gds__alloc ((SLONG) sizeof (PERF));
#ifdef DEBUG_GDS_ALLOC
		/* We don't care about QLI specific memory leaks for V4.0 */
		gds_alloc_flag_unfreed ((void *) dbb->dbb_statistics);	/* QLI: don't care */
#endif
		}
#ifndef PC_PLATFORM
	    perf_get_info (&dbb->dbb_handle, dbb->dbb_statistics);
#endif
	    }

/* Execute the request, for better or worse */

EXEC_top (execution_tree);

if (QLI_statistics)
    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
	if (dbb->dbb_flags & DBB_active)
		{
#ifndef PC_PLATFORM
		ERRQ_msg_get (505, report);
                /* Msg505 "    reads = !r writes = !w fetches = !f marks = !m\n" */
		ERRQ_msg_get (506, report + strlen (report));
                /* Msg506 "    elapsed = !e cpu = !u system = !s mem = !x, buffers = !b" */
		perf_get_info (&dbb->dbb_handle, &statistics);
		perf_format (dbb->dbb_statistics, &statistics,
		report, buffer, NULL_PTR);
#endif
	    ERRQ_msg_put (26, dbb->dbb_filename, buffer, NULL, NULL, NULL); /* Msg26 Statistics for database %s %s  */
	    QLI_skip_line = TRUE;
	    }

/* Release resources associated with the request */

GEN_release();

return FALSE;
}


#ifndef mpexl
static void CLIB_ROUTINE signal_arith_excp (
    USHORT	sig,
    USHORT	code,
    USHORT	scp)
{
/**************************************
 *
 *	s i g n a l _ a r i t h _ e x c p
 *
 **************************************
 *
 * Functional description
 *	Catch arithmetic exception.
 *
 **************************************/
USHORT	msg_number;

switch (code)
    {
#ifdef FPE_INOVF_TRAP
    case FPE_INTOVF_TRAP:
	msg_number = 14; /* Msg14 integer overflow */
	break;
#endif

#ifdef FPE_INTDIV_TRAP
    case FPE_INTDIV_TRAP:
	msg_number = 15; /* Msg15 integer division by zero */
	break;
#endif

#ifdef FPE_FLTOVF_TRAP
    case FPE_FLTOVF_TRAP:
	msg_number = 16; /* Msg16 floating overflow trap */
	break;
#endif

#ifdef FPE_FLTDIV_TRAP
    case FPE_FLTDIV_TRAP:
	msg_number = 17; /* Msg17 floating division by zero */
	break;
#endif

#ifdef FPE_FLTUND_TRAP
    case FPE_FLTUND_TRAP:
	msg_number = 18; /* Msg18 floating underflow trap */
	break;
#endif

#ifdef FPE_FLTOVF_FAULT
    case FPE_FLTOVF_FAULT:
	msg_number = 19; /* Msg19 floating overflow fault */
	break;
#endif

#ifdef FPE_FLTUND_FAULT
    case FPE_FLTUND_FAULT:
	msg_number = 20; /* Msg20 floating underflow fault */
	break;
#endif

    default:
	msg_number = 21; /* Msg21 arithmetic exception */
    }

signal (SIGFPE, signal_arith_excp);

IBERROR (msg_number);
}
#endif

#ifdef mpexl
static void CLIB_ROUTINE signal_arith_excp (
    struct {
	SLONG	instruction;
	SLONG	offset;
	SLONG	space_id;
	SLONG	error_code;
    }		*trap_struct)
{
/**************************************
 *
 *	s i g n a l _ a r i t h _ e x c p
 *
 **************************************
 *
 * Functional description
 *	Catch arithmetic exception.
 *
 **************************************/
USHORT	msg_number;

switch (trap_struct->error_code)
    {
    case TRP_INTEGER_OVER:
	msg_number = 14; /* Msg14 integer overflow */
	break;

    case TRP_INTEGER_DIVIDE:
	msg_number = 15; /* Msg15 integer division by zero */
	break;

    case TRP_FLOAT_OVER1:
    case TRP_FLOAT_OVER2:
    case TRP_FLOAT_OVER3:
	msg_number = 16; /* Msg16 floating overflow trap */
	break;

    case TRP_FLOAT_DIVIDE1:
    case TRP_FLOAT_DIVIDE2:
    case TRP_FLOAT_DIVIDE3:
	msg_number = 17; /* Msg17 floating division by zero */
	break;

    case TRP_FLOAT_UNDER1:
    case TRP_FLOAT_UNDER2:
    case TRP_FLOAT_UNDER3:
	msg_number = 18; /* Msg18 floating underflow trap */
	break;

    default:
	msg_number = 21; /* Msg21 arithmetic exception */
    }

IBERROR (msg_number);
}
#endif

static void CLIB_ROUTINE signal_quit (void)
{
/**************************************
 *
 *	s i g n a l _ q u i t
 *
 **************************************
 *
 * Functional description
 *	Stop whatever we happened to be doing.
 *
 **************************************/
void	(*prev_handler)();

#ifndef mpexl
signal (SIGQUIT, SIG_DFL);
signal (SIGINT, SIG_DFL);
#else
XCONTRAP (0, (int*) &prev_handler);
#endif

EXEC_abort();
}

static BOOLEAN yes_no (
    USHORT	number,
    TEXT	*arg1)
{
/**************************************
 *
 *	y e s _ n o
 *
 **************************************
 *
 * Functional description
 *	Put out a prompt that expects a yes/no
 *	answer, and keep trying until we get an
 *	acceptable answer (e.g. y, Yes, N, etc.)
 *
 **************************************/
TEXT	buffer [256], prompt [256], *p, *q, c;
ANS	response;

ERRQ_msg_format (number, sizeof (prompt), prompt, arg1, NULL, NULL, NULL, NULL);
if (!yes_no_loaded)
    {
    yes_no_loaded = 1;
    if (!ERRQ_msg_get (498, answer_table [0].answer))	/* Msg498 NO	*/
	strcpy (answer_table [0].answer, "NO");	/* default if msg_get fails */
    if (!ERRQ_msg_get (497, answer_table [1].answer))	/* Msg497 YES	*/
	strcpy (answer_table [1].answer, "YES");
    }

while (TRUE)
    {
    buffer [0] = 0;
    if (!LEX_get_line (prompt, buffer, sizeof (buffer)))
	return TRUE;
    for (response = answer_table; (TEXT*)response->answer; response++)
	{
	p = buffer;
	while (*p == ' ')
	    p++;
	if (*p == EOF)
	    return TRUE;
	for (q = response->answer; *p && UPPER (*p) == *q++; p++)
	    ;
	if (!*p || *p == '\n')
	    return response->value;
	}
    } 
}
