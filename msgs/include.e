/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		include.e
 *	DESCRIPTION:	Include file generator
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
#include "../jrd/common.h"

DATABASE DB = "source/msgs/msg.gdb";

IB_FILE	*output;

static int	check_option (SCHAR *, SSHORT *, SCHAR **);
static int	define_format (SCHAR *, SCHAR *);
static int	gen (SCHAR *, SCHAR *);
static int	process_line (IB_FILE *, SCHAR *);
static int	put_error (IB_FILE *, SCHAR **, SCHAR *, SSHORT);
static int	put_symbol (IB_FILE *, SCHAR *, SCHAR *, SLONG, SSHORT);
static int	put_symbols (IB_FILE *, SCHAR **, SCHAR *, SSHORT);

#define OPT_byte	1
#define OPT_signed	2
#define OPT_sans_dollar	4

static SSHORT	defaults;
static SCHAR	*formats [40], *format_ptr, format_buffer [1024];

void main (
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
 *	Generate include files.
 *
 **************************************/
SCHAR	**end, *p, *file, *language, *db_name, sw_all;

file = db_name = 0;
language = "C";
sw_all = 0;

for (end = argv + argc, ++argv; argv < end;)
    {
    p = *argv++;
    if (*p == '-')
	switch (UPPER (p [1]))
	    {
	    case 'F':
		file = *argv++;
		continue;

	    case 'L':
		language = *argv++;
		continue;

	    case 'D':
		db_name = *argv++;
		continue;

	    case 'A':
		sw_all = TRUE;
		continue;
	    }
    ib_printf ("Bad switch %s\n", *p);
    exit (FINI_ERROR);
    }

if (db_name)
    READY db_name AS DB;
else
    READY;

START_TRANSACTION;

if (sw_all)
    FOR X IN TEMPLATES
	gen (X.LANGUAGE, X.IB_FILE);
    END_FOR
else
    gen (language, file);

COMMIT;
FINISH;
exit (FINI_OK);
}

static int check_option (
    SCHAR	*string,
    SSHORT	*option_ptr,
    SCHAR	**format_ptr)
{
/**************************************
 *
 *	c h e c k _ o p t i o n
 *
 **************************************
 *
 * Functional description
 *	Check an word for a known option.
 *
 **************************************/
SCHAR	**format;

if (!strcmp (string, "BYTE"))
    {
    *option_ptr |= OPT_byte;
    return TRUE;
    }

if (!strcmp (string, "SIGNED"))
    {
    *option_ptr |= OPT_signed;
    return TRUE;
    }

if (!strcmp (string, "SANS_DOLLAR"))
    {
    *option_ptr |= OPT_sans_dollar;
    return TRUE;
    }

if (!format_ptr)
    return FALSE;

for (format = formats; *format; format += 2)
    if (!strcmp (string, *format))
	{
	*format_ptr = format [1];
	return TRUE;
	}

return FALSE;
}

static int define_format (
    SCHAR	*name,
    SCHAR	*format)
{
/**************************************
 *
 *	d e f i n e _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *	Define named format.
 *
 **************************************/
SCHAR	**ptr, *p;

if (!name)
    {
    ib_printf ("Format redefined\n");
    return;
    }

/* Find next free slot */

for (ptr = formats; *ptr; ptr += 2)
    ;

/* Copy in format name */

*ptr++ = format_ptr;

while (*format_ptr++ = *name++)
    ;

/* Copy in format */

*ptr++ = format_ptr;

while (*format_ptr++ = *format++)
    ;

/* Zero next entry */

*ptr = NULL;
}

static int gen (
    SCHAR	*language,
    SCHAR	*filename)
{
/**************************************
 *
 *	g e n
 *
 **************************************
 *
 * Functional description
 *	Generate an include file for a single language.
 *
 **************************************/
SCHAR	*end;

format_ptr = format_buffer;
formats [0] = NULL;
defaults = 0;

FOR X IN TEMPLATES WITH X.LANGUAGE EQ language
    if (!filename)
	filename = X.IB_FILE;
    if (!(output = ib_fopen (filename, "w")))
	{
	ib_perror (X.IB_FILE);
	return;
	}
    ib_printf ("Generating %s for %s\n", filename, X.LANGUAGE);
    FOR Y IN X.TEMPLATE
	end = Y.SEGMENT + Y.LENGTH;
	*end = 0;
	if (Y.LENGTH && end [-1] == '\n')
	    end [-1] = 0;
	process_line (output, Y.SEGMENT);
    END_FOR;
    ib_fclose (output);
END_FOR;
}

static int process_line (
    IB_FILE	*file,
    SCHAR	*line_ptr)
{
/**************************************
 *
 *	p r o c e s s _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Process an output line.
 *
 **************************************/
SCHAR	*line, c, *p, *q, **word, *words [10], stuff [512], *format;
SSHORT	options;

line = line_ptr;

/* If the line doesn't start with a $, just copy it */

if (*line != '$')
    {
    ib_fprintf (output, "%s\n", line);
    return;
    }

p = stuff;
format = 0;
options = defaults;
word = words;
++line;

while (*line)
    {
    *word = 0;
    while (*line == ' ' || *line == '\t')
	line++;
    if (*line == '"')
	{
	++line;
	break;
	}
    *word++ = p;
    while (*line && *line != ' ' && *line != '\t' && *line != '"')
	*p++ = *line++;
    *p++ = 0;
    if (check_option (word [-1], &options, &format))
	--word;
    }

if (!format)
    {
    for (format = p, line; *line;)
	if ((c = *line++) != '\\')
	    *p++ = c;
	else if (*line == 'n')
	    {
	    *p++ = '\n';
	    ++line;
	    }
	else if (*line == 't')
	    {
	    *p++ = '\t';
	    ++line;
	    }
	else
	    *p++ = c;
    *p = 0;
    }

if (!words [0])
    {
    ib_printf ("no class for line\n");
    return;
    }

if (!strcmp (words [0], "SYMBOLS"))
    put_symbols (file, words + 1, format, options);
else if (!strcmp (words [0], "ERROR"))
    put_error (file, words + 1, format, options);
else if (!strcmp (words [0], "FORMAT"))
    define_format (words [1], format);
else if (!strcmp (words [0], "SET") && words [1])
    defaults = options;
else
    ib_printf ("don't understand %s\n", words [0]);

}

static int put_error (
    IB_FILE	*file,
    SCHAR	**words,
    SCHAR	*format,
    SSHORT	options)
{
/**************************************
 *
 *	p u t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Error codes to a file.
 *
 **************************************/
SCHAR	*option, symbol [64];
SSHORT	count;

option = words [0];
count = 0;
options &= ~(OPT_byte | OPT_signed);

if (!option || !strcmp (option, "MAJOR") || !strcmp (option, "MINOR"))
    FOR X IN SYSTEM_ERRORS SORTED BY X.CODE
	strcpy (symbol, "gds__");
	strcat (symbol, X.GDS_SYMBOL);
	put_symbol (file, format, symbol, (X.CODE + gds_err_base) * gds_err_factor, options);
    END_FOR;
}

static int put_symbol (
    IB_FILE	*file,
    SCHAR	*format,
    SCHAR	*symbol,
    SLONG	value,
    SSHORT	options)
{
/**************************************
 *
 *	p u t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Write out a symbol.
 *
 **************************************/
SCHAR	*p, buffer [64];

if (!format)
    {
    ib_printf ("No format for symbol\n");
    return;
    }

if (options & OPT_byte)
    {
    if (value > 255)
	return;
    value = (UCHAR) value;
    }

if (options & OPT_signed)
    value = (SCHAR) value;

if (options & OPT_sans_dollar)
    {
    for (p = buffer; *symbol; symbol++)
	if (*symbol != '$')
	    *p++ = *symbol;
    *p = 0;
    symbol = buffer;
    }

ib_fprintf (file, format, symbol, value);
}

static int put_symbols (
    IB_FILE	*file,
    SCHAR	**words,
    SCHAR	*format,
    SSHORT	options)
{
/**************************************
 *
 *	p u t _ s y m b o l s
 *
 **************************************
 *
 * Functional description
 *	Dump symbols to a file.
 *
 **************************************/
SSHORT	count;

count = 0;

if (words [1] && strcmp (words [1], "*"))
    {
    FOR X IN SYMBOLS WITH X.CLASS EQ words [0] AND X.TYPE EQ words [1] SORTED BY X.SEQUENCE
	++count;
	put_symbol (file, format, X.SYMBOL, X.VALUE, options);
    END_FOR;
    if (!count)
	ib_printf ("No symbols found for %s/%s\n", words [0], words [1]);
    }
else
    {
    FOR X IN SYMBOLS WITH X.CLASS EQ words [0] SORTED BY X.SEQUENCE
	++count;
	put_symbol (file, format, X.SYMBOL, X.VALUE, options);
    END_FOR;
    if (!count)
	ib_printf ("No symbols found for %s\n", words [0]);
    }
}
