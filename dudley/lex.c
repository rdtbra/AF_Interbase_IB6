/*
 *	PROGRAM:	JRD Data Definition Language
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../jrd/gds.h"
#include "../dudley/ddl.h"
#include "../dudley/parse.h"
#include "../jrd/kanji.h"
#include "../dudley/ddl_proto.h"
#include "../dudley/hsh_proto.h"
#include "../dudley/lex_proto.h"
#include "../jrd/gds_proto.h"

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"
#define SCRATCH		"gd"
#endif

#ifdef PC_PLATFORM
#ifndef NETWARE_386
#define SCRATCH		"I"
#endif
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#define SCRATCH		"ib"
#endif

#ifndef SCRATCH
#ifdef SMALL_FILE_NAMES
#define SCRATCH		"gds_q"
#else
#define SCRATCH		"gds_query_"
#endif
#endif

extern TEXT	*DDL_prompt;

static int	nextchar (void);
static void	retchar (SSHORT);
static int	skip_white (void);

/* Input line control */

static IB_FILE	*input_file, *trace_file;
static TEXT	*DDL_char, DDL_buffer [256], trace_file_name [128];

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
#define CHR_EOL		CHR_white

static SCHAR	classes[256] = {
    0,0,0,0,0,0,0,0,
    0,CHR_WHITE,CHR_EOL,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    CHR_WHITE,0,CHR_QUOTE,CHR_IDENT,CHR_IDENT,0,0,CHR_QUOTE,
    0,0,0,0,0,0,0,0,
    CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,CHR_DIGIT,
    CHR_DIGIT,CHR_DIGIT,0,0,0,0,0,0,
    0,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,0,0,0,0,CHR_LETTER,
    0,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,CHR_LETTER,
    CHR_LETTER,CHR_LETTER,CHR_LETTER,0};


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
SSHORT	c, length;
TEXT	*p;

token = &DDL_token;
p = token->tok_string;
*p++ = c = skip_white();

if (DDL_eof)
    {
    token->tok_symbol = NULL;
    token->tok_keyword = KW_none;
    return NULL;
    }

while (!(classes [c = nextchar()] & (CHR_white | CHR_eol)))
    *p++ = c;
    
retchar (c);
token->tok_length = p - token->tok_string;
*p = 0;
token->tok_symbol = NULL;
token->tok_keyword = KW_none;

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

if (trace_file != NULL)
    ib_fclose (trace_file);
if (trace_file_name [0])
    unlink (trace_file_name);
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
 *	Flush the input stream after an error.  For now, just look for
 *	an end of line.
 *
 **************************************/
SSHORT 	c;

while (!DDL_eof)
    {
    if ((c = nextchar()) == '\n')
	break;
    }
*DDL_char = 0;
}

void LEX_get_text (
    SCHAR	*buffer,
    TXT		text)
{
/**************************************
 *
 *	L E X _ g e t _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Write text from the scratch trace file into a buffer.
 *
 **************************************/
SLONG	start;
int	length;
TEXT	*p;

start = text->txt_position;
length = text->txt_length;

#ifndef mpexl
if (ib_fseek (trace_file, start, 0))
    {
    ib_fseek (trace_file, (SLONG) 0, 2);
    DDL_err (275, NULL, NULL, NULL, NULL, NULL); /* msg 275: ib_fseek failed */
    }
#else
if (ib_fclose (trace_file) ||
    !(trace_file = ib_fopen (trace_file_name, "r")) ||
    ib_fseek (trace_file, start, 0))
    {
    trace_file = ib_fopen (trace_file_name, "a");
    DDL_err (275, NULL, NULL, NULL, NULL, NULL); /* msg 275: ib_fseek failed */
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

void LEX_init (
    void	*file)
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
TEXT	*p, *end;

#if !(defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
trace_file = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, NULL_PTR);
#else
#ifndef __BORLANDC__
trace_file = (IB_FILE*) gds__temp_file (TRUE, SCRATCH, trace_file_name);
#else
/* When using Borland C, routine gds__temp_file is in a DLL which maps
   a set of I/O handles that are different from the ones in the GDEF
   process!  So we will get a temp name on our own.  [Note that
   gds__temp_file returns -1 on error, not 0] */

p = tempnam (NULL, SCRATCH);
strcpy (trace_file_name, p);
free (p);
trace_file = ib_fopen (trace_file_name, "w+");
if ( !trace_file)
    trace_file = (IB_FILE*) -1;
#endif
#endif
if (trace_file == (IB_FILE*) -1)
    DDL_err (276, NULL, NULL, NULL, NULL, NULL); /* msg 276: couldn't open scratch file */

#ifdef mpexl
FFILEINFO (_mpe_fileno (ib_fileno (trace_file)),
    INF_FID, trace_file_name,
    ITM_END);
for (p = trace_file_name, end = p + sizeof (trace_file_name) - 1; p < end && *p && *p != ' '; p++)
    ;
*p = 0;
#endif

input_file = (IB_FILE*) file;
DDL_char = DDL_buffer;
DDL_token.tok_position = 0;
DDL_description = FALSE;
DDL_line = 1;
}

void LEX_put_text (
    void	*blob,
    TXT		text)
{
/**************************************
 *
 *	L E X _ p u t _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Write text from the scratch trace file into a blob.
 *
 **************************************/
SLONG	start;
STATUS	status_vector [20];
int	length;
SSHORT	l, c;
TEXT	buffer [1024], *p;

start = text->txt_position;
length = text->txt_length;

#ifndef mpexl
if (ib_fseek (trace_file, start, 0))
    {
    ib_fseek (trace_file, (SLONG) 0, 2);
    DDL_err (275, NULL, NULL, NULL, NULL, NULL); /* msg 275: ib_fseek failed */
    }
#else
if (ib_fclose (trace_file) ||
    !(trace_file = ib_fopen (trace_file_name, "r")) ||
    ib_fseek (trace_file, start, 0))
    {
    trace_file = ib_fopen (trace_file_name, "a");
    DDL_err (275, NULL, NULL, NULL, NULL, NULL); /* msg 275: ib_fseek failed */
    }
#endif

while (length)
    {
    p = buffer;
    while (length)
	{
	--length;
	*p++ = c = ib_getc (trace_file);
	if (c == '\n')
	    break;
	}
    if (l = p - buffer)
	if (gds__put_segment (status_vector, 
		GDS_REF (blob), 
		l, 
		buffer))
	    DDL_err (277, NULL, NULL, NULL, NULL, NULL); /* msg 277: gds__put_segment failed */
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

if (DDL_token.tok_string [0] != '\n')
    return;

LEX_token ();
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
SSHORT	c, next;
TEXT	class, *p;
SYM	symbol;

token = &DDL_token;
p = token->tok_string;
*p++ = c = skip_white();

/* On end of file, generate furious but phony end of line tokens */

#if (defined JPN_SJIS || defined JPN_EUC)
class = (JPN1_CHAR (c) ? CHR_LETTER : classes [c]);
#else
class = classes [c];
#endif 

if (DDL_eof)
    {
    p = token->tok_string;
    *p++ = '*';
    *p++ = 'E';
    *p++ = 'O';
    *p++ = 'F';
    *p++ = '*';
    token->tok_symbol = NULL;
    token->tok_keyword = KW_none;
    token->tok_type = tok_eol;
    token->tok_length = p - token->tok_string;
    *p = '\0';
    return NULL;
    }
else if (class & CHR_letter)
    {
#if (! ( defined JPN_EUC || defined JPN_SJIS) )
    while (classes [c = nextchar()] & CHR_ident)
	*p++ = c;
#else

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
	    if ((SJIS_SINGLE (c)) || (classes [c] & CHR_ident))
#else
	    if (classes[c] & CHR_ident)
#endif
		*p++ = c;
	    else
		break;
	    }
	c = nextchar (TRUE);
	}
#endif /* JPN_SJIS || JPN_EUC */

    retchar (c);
    token->tok_type = tok_ident;
    }
else if (class & CHR_digit)
    {
    while (classes[c = nextchar()] & CHR_digit)
	*p++ = c;
    if (c == '.')
	{
	*p++ = c;
	while (classes [c = nextchar()] & CHR_digit)
	    *p++ = c;
	}
    retchar (c);
    token->tok_type = tok_number;
    }
else if ((class & CHR_quote) && !DDL_description)
    {
    token->tok_type = tok_quoted;
    do 
	{
	if (!(next = nextchar()) || next == '\n')
	    {
	    DDL_err (278, NULL, NULL, NULL, NULL, NULL); /* msg 278: unterminated quoted string */
	    break;
	    }
	*p++ = next;
	}
    while (next != c);
    }
else
    if (c == '\n')
	token->tok_type = tok_eol;
    else
	{
	token->tok_type = tok_punct;
	*p++ = nextchar();
	if (!HSH_lookup (token->tok_string, 2))
	    retchar (*--p);
	}
    
token->tok_length = p - token->tok_string;
*p = '\0';
token->tok_symbol = symbol = HSH_lookup (token->tok_string, token->tok_length);
if (symbol && symbol->sym_type == SYM_keyword)
    token->tok_keyword = (enum kwwords) symbol->sym_keyword;
else
    token->tok_keyword = KW_none;

if (DDL_trace)
    ib_puts (token->tok_string);

return token;
}

static int nextchar (void)
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
SCHAR	*end;

/* mark the end of the buffer */

end = DDL_buffer + sizeof (DDL_buffer);

/* If there isn't anything floating around, get a new line */

while (!(c = *DDL_char++))
    {
    DDL_char = DDL_buffer;
    if (DDL_interactive)
	{
	ib_printf (DDL_prompt);
	if (DDL_service)
	    ib_putc ('\001', ib_stdout);
	ib_fflush (ib_stdout);
	}
    while (c = ib_getc (input_file))
	{
	if (c == EOF && SYSCALL_INTERRUPTED(errno))
	    {
	    errno = 0;
	    continue;
	    }
	if (c == EOF)
	    break;
	if (DDL_char < end)
	    *DDL_char++ = c;
	else 
	    DDL_err (279, NULL, NULL, NULL, NULL, NULL);  /* msg 279: line too SLONG */
	if (c == '\n')
	    break;
	}
    *DDL_char = 0;
    if (c == EOF && DDL_char == DDL_buffer)
	{
#ifdef UNIX
	if (DDL_interactive)
	    ib_printf ("\n");
#endif
	DDL_eof = TRUE;
	return EOF;
	}
    DDL_char = DDL_buffer;
    ib_fputs (DDL_buffer, trace_file);
    }

DDL_token.tok_position++;
if (c == '\n')
    {
    ++DDL_line;
#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
    /* need to account for extra linefeed on newline */

    DDL_token.tok_position++;
#endif
    }

return c;
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

if (c == '\n')
    {
    --DDL_line;
#if (defined WIN_NT || defined OS2_ONLY || defined PC_PLATFORM)
    /* account for the extra linefeed in a newline */

    --DDL_token.tok_position;
#endif
    }

--DDL_token.tok_position;
--DDL_char;
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
 *	Skip over white space and comments in input stream
 *
 **************************************/
SSHORT	c, next, class;

while ((c = nextchar()) != EOF)
    {
#if (defined JPN_SJIS || defined JPN_EUC)
    c = c & 0xff;
#endif

    class = classes [c];
    if (class & CHR_white)
	continue;
    if (c == '/' )
	{
	if ((next = nextchar ()) != '*')
	    {
	    retchar (next);
	    return c;
	    }
	c = nextchar();
	while ((next = nextchar()) &&
	    (next != EOF) &&
	    !(c == '*' && next == '/'))
	    c = next;
	continue;
	}
    break;
    }

return c;
}
