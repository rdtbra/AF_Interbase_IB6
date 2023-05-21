/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		lex.c
 *	DESCRIPTION:	Lexical analyser
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../jrd/gds.h"
#if (defined JPN_SJIS || defined JPN_EUC)
#include "../jrd/kanji.h"
#endif
#include "../qli/all_proto.h"
#include "../qli/err_proto.h"
#include "../qli/hsh_proto.h"
#include "../qli/lex_proto.h"
#include "../qli/proc_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/utl_proto.h"

#ifdef VMS
#include <descrip.h>
#define SCRATCH		"gds_query"
#define LIB$_INPSTRTRU	0x15821c
#endif

#ifdef APOLLO
#include <apollo//base.h>
#include "/sys/ins/streams.ins.c"
#define SCRATCH		"gds_query"
#endif

#ifdef UNIX
#ifdef SMALL_FILE_NAMES
#define SCRATCH		"gds_q"
#else
#define SCRATCH		"gds_query"
#endif
#define UNIX_LINE	1
#endif

#ifdef PC_PLATFORM
#define SCRATCH		"I"
#define UNIX_LINE	1
#define PC_FILE_SEEK
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#include <io.h>
#define SCRATCH		"ib"
#define UNIX_LINE	1
#define PC_FILE_SEEK
#endif

#ifdef mpexl
#define SCRATCH		"gd"
#define UNIX_LINE	1
#define FOPEN_INPUT_TYPE	"r Tm"

/* errno.h is missing EINTR on mpexl */

#define EINTR		4	/* Interrupted system call */
#endif

#ifndef FOPEN_INPUT_TYPE
#define FOPEN_INPUT_TYPE	"r"
#endif

extern USHORT	sw_verify, sw_trace;

static BOOLEAN	get_line (IB_FILE *, TEXT *, USHORT);
static int	nextchar (BOOLEAN);
static void	next_line (BOOLEAN);
static void	retchar (SSHORT);
static BOOLEAN	scan_number (SSHORT, TEXT **);
static int	skip_white (void);

static LLS	QLI_statements;
static int	QLI_position;
static IB_FILE	*input_file = NULL, *trace_file = NULL;
static UCHAR	trace_file_name [128];
static SLONG	trans_limit;

#define TRANS_LIMIT	10

#define CHR_ident	1
#define CHR_letter	2
#define CHR_digit	4
#define CHR_quote	8
#define CHR_white	16
#define CHR_eol		32

#define CHR_IDENT	CHR_ident
#define CHR_LETTER	CHR_letter + CHR_ident
#define CHR_DIGIT	CHR_digit + CHR_ident
#define CHR_QUOTE	CHR_quote
#define CHR_WHITE	CHR_white
#define CHR_EOL		CHR_eol

static SCHAR	*eol_string = "*end_of_line*",
		*eof_string = "*end_of_file*";

static SCHAR	classes[256] = {
    0,0,0,0,0,0,0,0,
    0,CHR_WHITE,CHR_EOL,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    CHR_WHITE,0,CHR_QUOTE,CHR_IDENT,CHR_LETTER,0,0,CHR_QUOTE,
    0,0,0,0,0,0,0,0,
    CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,
    CHR_DIGIT,CHR_DIGIT,0,0,0,0,0,0,
    0,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,0,0,0,0,CHR_IDENT,
    0,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,0};


int LEX_active_procedure (void)
{
/**************************************
 *
 *	L E X _ a c t i v e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Return TRUE if we're running out of a
 *	procedure and FALSE otherwise.  Somebody
 *	somewhere may care.
 *
 **************************************/

return (QLI_line->line_type == line_blob) ? TRUE : FALSE;
}

void LEX_edit (
    SLONG	start,
    SLONG	stop)
{
/**************************************
 *
 *	L E X _ e d i t
 *
 **************************************
 *
 * Functional description
 *	Dump the last full statement into a scratch file, then
 *	push the scratch file on the input stack.
 *
 **************************************/
IB_FILE	*scratch;
TEXT	filename [128], *p;
SSHORT	c;

#ifndef __BORLANDC__
scratch = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, filename);
if (scratch == (IB_FILE*) -1)
    IBERROR (61); /* Msg 61 couldn't open scratch file */
#ifdef WIN_NT
stop--;
#endif
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the QLI
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (filename, p);
free (p);
scratch = ib_fopen (filename, "w+");
stop--;
if (!scratch)
    IBERROR (61); /* Msg 61 couldn't open scratch file */
#endif

#ifndef mpexl
if (ib_fseek (trace_file, start, 0))
    {
    ib_fseek (trace_file, (SLONG) 0, 2);
    IBERROR (59); /* Msg 59 ib_fseek failed */
    }
#else
if (ib_fclose (trace_file) ||
    !(trace_file = ib_fopen (trace_file_name, "r")) ||
    ib_fseek (trace_file, start, 0))
    {
    trace_file = ib_fopen (trace_file_name, "a");
    IBERROR (59); /* Msg 59 ib_fseek failed */
    }
#endif

while (++start <= stop)
    {
    c = ib_getc (trace_file);
    if (c == EOF)
	break;
    ib_putc (c, scratch);
    }

ib_fclose (scratch);

if (gds__edit (filename, TRUE))
    LEX_push_file (filename, TRUE);

unlink (filename);

#ifndef mpexl
ib_fseek (trace_file, (SLONG) 0, 2); 
#else
ib_fclose (trace_file);
trace_file = ib_fopen (trace_file_name, "a");
#endif
}

TOK LEX_edit_string (void)
{
/**************************************
 *
 *	L E X _ e d i t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Parse the next token as an edit_string.
 *
 **************************************/
TOK	token;
SSHORT	c, d;
TEXT	*p;

token = QLI_token;

do c = skip_white(); while (c == '\n');
p = token->tok_string;
*p = c;

if (!QLI_line)
    {
    token->tok_symbol = NULL;
    token->tok_keyword = KW_none;
    return NULL;
    }

while (!(classes [c] & (CHR_white | CHR_eol)))
    {
    *p++ = c;
    if (classes [c] & CHR_quote)
	for (;;)
	    {
	    if ((d = nextchar  (FALSE)) == '\n')
		{
		retchar (d);
		break;
		}
	    *p++ = d;
	    if (d == c)
		break;
	    }
    c = nextchar  (TRUE);    
    }
	    
retchar (c);

if (p [-1] == ',')
    retchar (*--p);

token->tok_length = p - token->tok_string;
*p = 0;
token->tok_symbol = NULL;
token->tok_keyword = KW_none;

if (sw_trace)
    ib_puts (token->tok_string);

return token;
}

TOK LEX_filename (void)
{
/**************************************
 *
 *	L E X _ f i l e n a m e
 *
 **************************************
 *
 * Functional description
 *	Parse the next token as a filename.
 *
 **************************************/
TOK	token;
SSHORT	c, save;
USHORT	class;
TEXT	*p;

token = QLI_token;

do c = skip_white(); while (c == '\n');
p = token->tok_string;
*p++ = c;
 
/* If there isn't a line, we're all done */

if (!QLI_line)
    {
    token->tok_symbol = NULL;
    token->tok_keyword = KW_none;
    return NULL;
    }

/* notice if this looks like a quoted file name */

if (classes [c] & CHR_quote)
    {
    token->tok_type = tok_quoted;
    save = c;
    }
else
    save = 0;

/* Look for white space or end of line, allowing embedded quoted strings. */

for (;;)
    {
    c = nextchar  (TRUE);
    class = classes [c];
    if (c == '"' && c != save)
	{
	*p++ = c;
	for (;;)
	    {
	    c = nextchar  (TRUE);
	    class = classes [c];
	    if ((class & CHR_eol) || c == '"')
		break;
	    *p++ = c;
	    }
	}

    if (class & (CHR_white | CHR_eol))
	break;
    *p++ = c;
    }
    
retchar (c);

/* Drop trailing semi-colon to avoid confusion */

if (p [-1] == ';')
   {
   retchar (c);
   --p;
    }

/* complain on unterminated quoted string */

if ((token->tok_type == tok_quoted) && (p[-1] != save))
    IBERROR (60); /* Msg 60 unterminated quoted string */

token->tok_length = p - token->tok_string;
*p = 0;
token->tok_symbol = NULL;
token->tok_keyword = KW_none;

if (sw_trace)
    ib_puts (token->tok_string);

return token;
}

void LEX_fini (void)
{
/**************************************
 *
 *	L E X _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Shut down LEX in a more or less clean way.
 *
 **************************************/

if (trace_file && (trace_file != (IB_FILE*) -1))
    {
    ib_fclose (trace_file);
    unlink (trace_file_name);
    }
}

void LEX_flush (void)
{
/**************************************
 *
 *	L E X _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush the input stream after an error.  
 *
 **************************************/

trans_limit = 0;

if (!QLI_line)
    return;

/* Pop off line sources until we're down to the last one. */

while (QLI_line->line_next)
    LEX_pop_line();

/* Look for a semi-colon */

if (QLI_semi)
    while (QLI_line && QLI_token->tok_keyword != KW_SEMI)
	LEX_token();
else
    while (QLI_line && QLI_token->tok_type != tok_eol)
	LEX_token();
}

#ifdef APOLLO
int LEX_get_line (
    TEXT	*prompt,
    TEXT	*buffer,
    int		size)
{
/**************************************
 *
 *	L E X _ g e t _ l i n e  ( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Give a prompt and read a line.  If the line is terminated by
 *	an EOL, return TRUE.  If the buffer is exhausted and non-blanks
 *	would be discarded, return an error.  If EOF is detected,
 *	return FALSE.  Regardless, a null terminated string is returned.
 *
 **************************************/
SLONG		length, junk_length;
TEXT		*p, *q, junk_buffer [256];
USHORT		overflow_flag;
status_$t	status;
stream_$sk_t	seek;

/* We're going to add a null to the end, so don't read too much */

--size;

if (prompt)
    {
    ib_fputs (prompt, ib_stdout);
    ib_fflush (ib_stdout);
    }

stream_$get_rec (stream_$ib_stdin, buffer, size, p, length, seek, status);

if (status.all)
    return FALSE;

/* If the line overflowed the buffer, complain. */

if (length < 0)
    {
    overflow_flag = FALSE;
    while (length < 0)
	{
	length = -length;
	junk_length = sizeof (junk_buffer);
	stream_$get_rec (stream_$ib_stdin, &junk_buffer, junk_length, 
	    q, length, seek, status);

	/* Permit excessively long lines if only blanks are ignored */

	if (length >= 0)
	    junk_length = length;
	while (junk_length--)
	    if (*q != ' ' && *q++ != '\n')
		{
		overflow_flag = TRUE;
		break;
		}
	}
    if (overflow_flag)
	{
	buffer [0] = 0;
	IBERROR (476); /* Msg 476 input line too long */
	}
    length = size;
    }

if (p == buffer)
    buffer [length] = 0;
else
    {
    q = buffer;
    if (length)
	do *q++ = *p++; while (--length);
    *q = 0;
    }

if (sw_verify)
    ib_fputs (buffer, ib_stdout); 

return TRUE;
}
#endif

#ifdef UNIX_LINE
int LEX_get_line (
    TEXT	*prompt,
    TEXT	*buffer,
    int		size)
{
/**************************************
 *
 *	L E X _ g e t _ l i n e  ( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Give a prompt and read a line.  If the line is terminated by
 *	an EOL, return TRUE.  If the buffer is exhausted and non-blanks
 *	would be discarded, return an error.  If EOF is detected,
 *	return FALSE.  Regardless, a null terminated string is returned.
 *
 **************************************/
TEXT	*p;
USHORT	overflow_flag;
SSHORT	c;

/* UNIX flavor */

if (prompt)
    ib_printf (prompt);

errno = 0;
p = buffer;
overflow_flag = FALSE;

while (TRUE)
    {
    c = ib_getc (input_file);
    if (c == EOF)
	{
	if (SYSCALL_INTERRUPTED(errno) && !QLI_abort)
	    {
	    errno = 0;
	    continue;
	    }

	/* The check for this actually being a terminal that is at
	   end of file is to prevent looping through a redirected
	   ib_stdin (e.g., a script file).  */

	if (prompt && isatty (ib_fileno (ib_stdin)))
	    {
	    ib_rewind (ib_stdin);
	    ib_putchar ('\n');
	    }
	if (QLI_abort)
	    continue;
	else
	    break;
	}
    if (--size > 0)
	*p++ = c;
    else if (c != ' ' && c != '\n')
	overflow_flag = TRUE;
    if (c == '\n')
	break;
    }

*p = 0;
if (c == EOF)
    return FALSE;

if (overflow_flag)
    {
    buffer [0] = 0;
    IBERROR (476); /* Msg 476 input line too long */
    }

if (sw_verify)
    ib_fputs (buffer, ib_stdout); 

return TRUE;
}
#endif

#ifdef VMS
int LEX_get_line (
    TEXT	*prompt,
    TEXT	*buffer,
    int		size)
{
/**************************************
 *
 *	L E X _ g e t _ l i n e  ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Give a prompt and read a line.  If the line is terminated by
 *	an EOL, return TRUE.  If the buffer is exhausted and non-blanks
 *	would be discarded, return an error.  If EOF is detected,
 *	return FALSE.  Regardless, a null terminated string is returned.
 *
 **************************************/
struct dsc$descriptor_s	line_desc, prompt_desc;
SLONG		status;
SSHORT		length;
SCHAR		*p;

/* We're going to add a null to the end, so don't read too much */

--size;

line_desc.dsc$b_class = DSC$K_CLASS_S;
line_desc.dsc$b_dtype = DSC$K_DTYPE_T;
line_desc.dsc$w_length = size;
line_desc.dsc$a_pointer = buffer;

if (prompt)
    {
    prompt_desc.dsc$b_class = DSC$K_CLASS_S;
    prompt_desc.dsc$b_dtype = DSC$K_DTYPE_T;
    prompt_desc.dsc$w_length = strlen (prompt);
    prompt_desc.dsc$a_pointer = prompt;
    status = lib$get_input (&line_desc, &prompt_desc, &length);
    }
else
    status = lib$get_input (&line_desc, NULL, &length);

p = buffer + length;

if (!(status & 1))
    {
    if (status != LIB$_INPSTRTRU)
	return FALSE;
    buffer [0] = 0;
    IBERROR (476); /* Msg 476 input line too long */
    }
else if (length < size)
    *p++ = '\n';

*p = 0;

if (sw_verify)
    {
    line_desc.dsc$w_length = length;
    lib$put_output (&line_desc);
    }

return TRUE;
}
#endif

void LEX_init (void)
{
/**************************************
 *
 *	L E X _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize for lexical scanning.  While we're at it, open a
 *	scratch trace file to keep all input.
 *
 **************************************/
TEXT	*p;

#ifndef __BORLANDC__
trace_file = gds__temp_file (TRUE, SCRATCH, trace_file_name);
if (trace_file == (IB_FILE*) -1)
    IBERROR (61); /* Msg 61 couldn't open scratch file */
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the QLI
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (trace_file_name, p);
free (p);
trace_file = ib_fopen (trace_file_name, "w+");
if (!trace_file)
    IBERROR (61); /* Msg 61 couldn't open scratch file */
#endif

QLI_token = (TOK) ALLOCPV (type_tok, MAXSYMLEN);

QLI_line = (LINE) ALLOCPV (type_line, 0);
QLI_line->line_size = sizeof (QLI_line->line_data);
QLI_line->line_ptr = QLI_line->line_data;
QLI_line->line_type = line_stdin;
QLI_line->line_source = (int*) ib_stdin;

QLI_semi = FALSE;
input_file = ib_stdin;
HSH_init();
}

void LEX_mark_statement (void)
{
/**************************************
 *
 *	L E X _ m a r k _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Push a position in the trace file onto
 *	the statement stack.
 *
 **************************************/
LLS	statement;
LINE	temp;

for (temp = QLI_line; 
	temp->line_next && QLI_statements;
	temp = temp->line_next)
    if (temp->line_next->line_position == (SLONG) QLI_statements->lls_object)
	return;

statement = (LLS) ALLOCP (type_lls);
statement->lls_object = (BLK) temp->line_position;
statement->lls_next = QLI_statements;
QLI_statements =  statement;
}

void LEX_pop_line (void)
{
/**************************************
 *
 *	L E X _ p o p _ l i n e
 *
 **************************************
 *
 * Functional description
 *	We're done with the current line source.  Close it out
 *	and release the line block.
 *
 **************************************/
LINE	temp;

temp =  QLI_line;
QLI_line = temp->line_next;

if (temp->line_type == line_blob)
    PRO_close (temp->line_database, temp->line_source);
else if (temp->line_type == line_file)
    ib_fclose ((IB_FILE*) temp->line_source);

ALL_release (temp);
}

void LEX_procedure (
    DBB		database,
    int		*blob)
{
/**************************************
 *
 *	L E X _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Push a blob-resident procedure onto the input line source
 *	stack;
 *
 **************************************/
LINE	temp;

temp = (LINE) ALLOCPV (type_line, QLI_token->tok_length);
temp->line_source = blob;
strncpy (temp->line_source_name, QLI_token->tok_string, QLI_token->tok_length);
temp->line_type = line_blob;
temp->line_database = database;
temp->line_size = sizeof (temp->line_data);
temp->line_ptr = temp->line_data;
temp->line_position = QLI_position; 
temp->line_next = QLI_line;
QLI_line = temp;
}

int LEX_push_file (
    TEXT	*filename,
    int		error_flag)
{
/**************************************
 *
 *	L E X _ p u s h _ f i l e 
 *
 **************************************
 *
 * Functional description
 *	Open a command file.  If the open succeedes, push the input
 *	source onto the source stack.  If the open fails, give an error
 *	if the error flag is set, otherwise return quietly.
 *
 **************************************/
TEXT	buffer [64];
IB_FILE	*file;
LINE	line;

if (!(file = ib_fopen (filename, FOPEN_INPUT_TYPE)))
    {
    sprintf (buffer, "%s.com", filename);
    if (!(file = ib_fopen (buffer, FOPEN_INPUT_TYPE)))
	{
	if (error_flag)
	    ERRQ_msg_put (67, filename, NULL, NULL, NULL, NULL); /* Msg 67 can't open command file \"%s\"\n */
	return FALSE;
	}
    }

line = (LINE) ALLOCPV (type_line, strlen (filename));
line->line_type = line_file;
line->line_source = (int*) file;
line->line_size = sizeof (line->line_data);
line->line_ptr = line->line_data;
*line->line_ptr = 0;
strcpy (line->line_source_name, filename);
line->line_next = QLI_line;
QLI_line = line;

return TRUE;
}

int LEX_push_string (
    TEXT	*string)
{
/**************************************
 *
 *	L E X _ p u s h _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Push a simple string on as an input source.
 *
 **************************************/
LINE	line;

line = (LINE) ALLOCPV (type_line, 0);
line->line_type = line_string;
line->line_size = strlen (string);
line->line_ptr = line->line_data;
strcpy (line->line_data, string);
line->line_data [line->line_size] = '\n';
line->line_next = QLI_line;
QLI_line = line;

return TRUE;
}

void LEX_put_procedure (
    void	*blob,
    SLONG	start,
    SLONG	stop)
{
/**************************************
 *
 *	L E X _ p u t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Write text from the scratch trace file into a blob.
 *
 **************************************/
STATUS	status_vector [20];
int	length;
SSHORT	l, c;
TEXT	buffer [1024], *p;

#ifndef mpexl
if (ib_fseek (trace_file, start, 0))
    {
    ib_fseek (trace_file, (SLONG) 0, 2);
    IBERROR (62); /* Msg 62 ib_fseek failed */
    }
#else
if (ib_fclose (trace_file) ||
    !(trace_file = ib_fopen (trace_file_name, "r")) ||
    ib_fseek (trace_file, start, 0))
    {
    trace_file = ib_fopen (trace_file_name, "a");
    IBERROR (62); /* Msg 62 ib_fseek failed */
    }
#endif

length = stop - start;

while (length)
    {
    p = buffer;
    while (length)
	{
	--length;
	*p++ = c = ib_getc (trace_file);
	if (c == '\n')
	    {
#ifdef PC_FILE_SEEK
	    /* account for the extra line-feed on OS/2 and Windows NT */

	    if (length)
		--length;
#endif
	    break;
	    }
	}
    if (l = p - buffer)
	if (gds__put_segment (status_vector, 
		GDS_REF (blob), 
		l, 
		buffer))
	    BUGCHECK (58); /* Msg 58 gds__put_segment failed */
    }

#ifndef mpexl
ib_fseek (trace_file, (SLONG) 0, 2); 
#else
ib_fclose (trace_file);
trace_file = ib_fopen (trace_file_name, "a");
#endif
}

void LEX_real (void)
{
/**************************************
 *
 *	L E X _ r e a l
 *
 **************************************
 *
 * Functional description
 *	If the token is an end of line, get the next token.
 *
 **************************************/

while (QLI_token->tok_type == tok_eol)
    LEX_token ();
}

LLS LEX_statement_list (void)
{
/**************************************
 *
 *	L E X _ s t a t e m e n t _ l i s t
 *
 **************************************
 *
 * Functional description
 *	Somebody outside needs to know
 *	where the top of the statement list
 *	is.
 *
 **************************************/

return QLI_statements;
}

TOK LEX_token (void)
{
/**************************************
 *
 *	L E X _ t o k e n
 *
 **************************************
 *
 * Functional description
 *	Parse and return the next token.
 *
 **************************************/
TOK	token;
SSHORT	c, next, peek;
TEXT	class, *p, *q;
SYM	symbol;
LINE	prior;

token = QLI_token;
p = token->tok_string;

/* Get next significant byte.  If it's the last EOL of a blob, throw it
   away */

for (;;)
    {
    c = skip_white();
    if (c != '\n' || QLI_line->line_type != line_blob)
	break;
    prior = QLI_line;
    next_line (TRUE);
    if (prior == QLI_line)
	break;
    }

/* If we hit end of file, make up a phoney token */

if (!QLI_line)
    {
    q = eof_string;
    while (*p++ = *q++)
	;
    token->tok_type = tok_eof;
    token->tok_keyword = KW_none;
    return NULL;
    }

*p++ = c;
QLI_token->tok_position = QLI_line->line_position + 
	QLI_line->line_ptr - QLI_line->line_data - 1;

/* On end of file, generate furious but phone end of line tokens */

#if (defined JPN_SJIS || defined JPN_EUC)
class = (JPN1_CHAR(c) ? CHR_LETTER : classes[c]);
#else
class = classes [c];
#endif

if (class & CHR_letter)
    {
#if (defined JPN_EUC || defined JPN_SJIS)

    p--;
    while(1)
        {
        if (KANJI1(c))
            {

            /* If it is a double byte kanji either EUC or SJIS
               then handle both the bytes together */

            *p++ = c;
            c = nextchar (TRUE);
            if (!KANJI2(c))
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
            if ((SJIS_SINGLE(c)) || (classes[c] & CHR_ident))
#else
            if (classes[c] & CHR_ident)
#endif
                *p++ = c;
            else
                break;
            }
        c = nextchar (TRUE);
        }

#else

    while (classes [c = nextchar  (TRUE)] & CHR_ident)
	*p++ = c;

#endif /* (defined JPN_EUC || defined JPN_SJIS) */
    retchar (c);
    token->tok_type = tok_ident;
    }
else if (((class & CHR_digit) || c == '.') && scan_number (c, &p))
    token->tok_type = tok_number;
else if (class & CHR_quote)
    {
    token->tok_type = tok_quoted;
    while (TRUE)
        {
	if (!(next = nextchar  (FALSE)) || next == '\n')
	    {
	    retchar (next);
	    IBERROR (63);  /* Msg 63 unterminated quoted string */
	    break;
            }
        *p++ = next;
	if ((p - token->tok_string) >= MAXSYMLEN)
	    ERRQ_msg_put (470, (TEXT*) MAXSYMLEN, NULL, NULL, NULL, NULL); /* Msg 470 literal too long */	    

        /* If there are 2 quotes in a row, interpret 2nd as a literal */
	    
        if (next == c)
            {
            peek = nextchar  (FALSE);
            retchar (peek);
            if (peek != c)
                break;
            nextchar  (FALSE);
            }
        } 
    }
else if (c == '\n')
    {
    token->tok_type = tok_eol;
    --p;
    q = "end of line";
    while (*q)
	*p++ = *q++;
    }
else
    {
    token->tok_type = tok_punct;
    *p++ = nextchar  (TRUE);
    if (!HSH_lookup (token->tok_string, 2))
	retchar (*--p);
    }
    
token->tok_length = p - token->tok_string;
*p = '\0';

if (token->tok_string [0] == '$' &&
    trans_limit < TRANS_LIMIT &&
    (p = getenv (token->tok_string + 1)))
    {
    LEX_push_string (p);
    ++trans_limit;
    token = LEX_token();
    --trans_limit;
    return token;
    }

token->tok_symbol = symbol = HSH_lookup (token->tok_string, token->tok_length);
if (symbol && symbol->sym_type == SYM_keyword)
    token->tok_keyword = (KWWORDS) symbol->sym_keyword;
else
    token->tok_keyword = KW_none;

if (sw_trace)
    ib_puts (token->tok_string);

return token;
}

static BOOLEAN get_line (
    IB_FILE	*file,
    TEXT	*buffer,
    USHORT	size)
{
/**************************************
 *
 *	g e t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Read a line.  If the line is terminated by
 *	an EOL, return TRUE.  If the buffer is exhausted and non-blanks
 *	would be discarded, return an error.  If EOF is detected,
 *	return FALSE.  Regardless, a null terminated string is returned.
 *
 **************************************/
TEXT	*p;
SLONG	length;
USHORT	overflow_flag;
SSHORT	c;

errno = 0;
p = buffer;
overflow_flag = FALSE;
length = size;

while (TRUE)
    {
    c = ib_getc (file);
    if (c == EOF)
	{
	if (SYSCALL_INTERRUPTED(errno) && !QLI_abort)
	    {
	    errno = 0;
	    continue;
	    }
	if (QLI_abort)
	    continue;
	else
	    break;
	}
    if (--length > 0)
	*p++ = c;
    else if (c != ' ' && c != '\n')
	overflow_flag = TRUE;
    if (c == '\n')
	break;
    }

*p = 0;
if (c == EOF)
    return FALSE;

if (overflow_flag)
    IBERROR (477); /* Msg 477 input line too long */

if (sw_verify)
    ib_fputs (buffer, ib_stdout); 

return TRUE;
}

static int nextchar (
    BOOLEAN	eof_ok)
{
/**************************************
 *
 *	n e x t c h a r
 *
 **************************************
 *
 * Functional description
 *	Get the next character from the input stream.
 *
 **************************************/
SSHORT	c;

/* Get the next character in the current line.  If we run out,
   get the next line.  If the line source runs out, pop the
   line source.  If we run out of line sources, we are distinctly
   at end of file. */

while (QLI_line)
    {
    if (c = *QLI_line->line_ptr++)
#if (defined JPN_SJIS || defined JPN_EUC)
	{
	c = c & 0xff;   /* negative values are bugs */
	return c;
	}
#else
	return c;
#endif 
    next_line (eof_ok);
    }

return 0;
}

static void next_line (
    BOOLEAN	eof_ok)
{
/**************************************
 *
 *	n e x t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Get the next line from the input stream.
 *
 **************************************/
TEXT	*p, *q, filename [256];
SSHORT	flag;

while (QLI_line)
    {
    flag = FALSE;

    /* Get next line from where ever.  If it comes from either the terminal
       or command file, check for another command file. */

    if (QLI_line->line_type == line_blob)
	{
	/* If the current blob segment contains another line, use it */

	if ((p = QLI_line->line_ptr) != QLI_line->line_data && p [-1] == '\n' && *p)
	    flag = TRUE;
	else
	    {
	    /* Initialize line block for retrieval */

	    p = QLI_line->line_data;
	    QLI_line->line_ptr = QLI_line->line_data;

	    flag = PRO_get_line (QLI_line->line_source, p, QLI_line->line_size);
	    if (flag && QLI_echo)
	        ib_printf ("%s", QLI_line->line_data);
	    }
	}
    else
	{
	/* Initialize line block for retrieval */

	QLI_line->line_ptr = QLI_line->line_data;
	p = QLI_line->line_data;

	if (QLI_line->line_type == line_stdin)
	    flag = LEX_get_line (QLI_prompt, p, (int) QLI_line->line_size);
	else if (QLI_line->line_type == line_file)
	    {
	    flag = get_line (QLI_line->line_source, p, QLI_line->line_size);
	    if (QLI_echo)
		ib_printf ("%s", QLI_line->line_data);
	    }
	if (flag)
	    {
	    for (q = p; classes [*q] & CHR_white; q++)
		;
	    if (*q == '@')
		{
		for (p = q + 1, q = filename; *p && *p != '\n';)
		    *q++ = *p++;
		*q = 0;
		QLI_line->line_ptr = (TEXT *) p;
		LEX_push_file (filename, TRUE);
		continue;
		}
	    }
	}

    /* If get got a line, we're almost done */

    if (flag)
	break;

    /* We hit end of file.  Either complain about the circumstances
       or just close out the current input source.  Don't close the
       input source if it's the terminal and we're at a continuation
       prompt. */

    if (eof_ok && (QLI_line->line_next || QLI_prompt != QLI_cont_string))
	{
	LEX_pop_line();
	return;
	}

    /* this is an unexpected end of file */

    if (QLI_line->line_type == line_blob)
	ERRQ_print_error (64, QLI_line->line_source_name, NULL, NULL, NULL, NULL);
	/* Msg 64 unexpected end of procedure in procedure %s */
    else if (QLI_line->line_type == line_file)
	ERRQ_print_error (65, QLI_line->line_source_name, NULL, NULL, NULL, NULL);
	/* Msg 65 unexpected end of file in file %s */
    else 
	{
	if (QLI_line->line_type == line_string)
	    LEX_pop_line();
	IBERROR (66); /* Msg 66 unexpected eof */
	}
    }

if (!QLI_line)
    return;

QLI_line->line_position = QLI_position;

/* Dump output to the trace file */

if (QLI_line->line_type == line_blob)
    while (*p)
	p++;
else
    {
    while (*p)
	ib_putc (*p++, trace_file);
    QLI_position += (TEXT *) p - QLI_line->line_data;
#ifdef PC_FILE_SEEK
    /* account for the extra line-feed on OS/2 and Windows NT
       to determine file position */

    QLI_position++;
#endif
    }

QLI_line->line_length = (TEXT *) p - QLI_line->line_data;
}

static void retchar (
    SSHORT	c)
{
/**************************************
 *
 *	r e t c h a r
 *
 **************************************
 *
 * Functional description
 *	Return a character to the input stream.
 *
 **************************************/

--QLI_line->line_ptr;
}

static BOOLEAN scan_number (
    SSHORT	c,
    TEXT	**ptr)
{
/**************************************
 *
 *	s c a n _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Pass on a possible numeric literal.
 *
 **************************************/
TEXT	*p;
SSHORT	dot;

p = *ptr;
dot = FALSE;

/* If this is a leading decimal point, check that the next
   character is really a digit, otherwise backout */

if (c == '.')
    {
    retchar (c = nextchar  (TRUE));
    if (!(classes [c] & CHR_digit))
	return FALSE;
    dot = TRUE;
    }

/* Gobble up digits up to a single decimal point */

for (;;)
    {
    c = nextchar  (TRUE);
    if (classes [c] & CHR_digit)
	*p++ = c;
    else if (!dot && c == '.')
	{
	*p++ = c;
	dot = TRUE;
	}
    else
	break;
    }

/* If this is an exponential, eat the exponent sign and digits */

if (UPPER (c) == 'E')
    {
    *p++ = c;
    c = nextchar  (TRUE);
    if (c == '+' || c == '-')
	{
	*p++ = c;
	c = nextchar  (TRUE);
	}
    while (classes [c] & CHR_digit)
	{
	*p++ = c;
	c = nextchar  (TRUE);
	}
    }

retchar (c);
*ptr = p;

return TRUE;
}

static int skip_white (void)
{
/**************************************
 *
 *	s k i p _ w h i t e
 *
 **************************************
 *
 * Functional description
 *	Skip over while space and comments in input stream
 *
 **************************************/
SSHORT	c, next, class;

while (TRUE)
    {
    c = nextchar  (TRUE);
    class = classes [c];
    if (class & CHR_white)
        continue;
    if (c == '/')
	{
        if ((next = nextchar  (TRUE)) != '*')
	    {
	    retchar (next);
	    return c;
	    }
	c = nextchar  (FALSE);
	while ((next = nextchar  (FALSE)) && !(c == '*' && next == '/'))
	    c = next;
	continue;
	}
    break;
    }

return c;
}
