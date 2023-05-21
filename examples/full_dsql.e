/*
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
#include <stdio.h>

#ifdef VMS
#define TIME_DEFINED
#include <time.h>
#endif

#ifdef apollo
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef sun
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef hpux
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef ultrix
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef DGUX
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef sgi
#define TIME_DEFINED
#include <sys/time.h>
#endif

#if (defined __osf__ && defined __alpha)
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef mpexl
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifndef TIME_DEFINED
#include <time.h>
#endif

extern char		*malloc();

DATABASE DB = COMPILETIME "atlas.gdb"
	         RUNTIME db_name;

char	db_name[128];
/**************************************
 *
 * Macro for aligning buffer space to boundaries 
 *   needed by a datatype on a given machine
 *
 **************************************/
#ifdef VMS
#define ALIGN(n,b)				(n)
#endif

#ifdef sun
#ifdef sparc
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif
#endif

#ifdef hpux
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef ultrix
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef sgi
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef _AIX
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef DGUX
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#if (defined __osf__ && defined __alpha)
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef mpexl
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifndef ALIGN
#define ALIGN(n,b)			((n+1) & ~1)
#endif

#define UPPER(c) 				((c >= 'a' && c<= 'z') ? c + 'A' - 'a': c )

static char		*alpha_months [] =
		    {
		    "January",
		    "February",
		    "March",
		    "April",
		    "May",
		    "June",
		    "July",
		    "August",
		    "September",
		    "October",
		    "November",
		    "December"
		    };

typedef struct vary {
    short		vary_length;
    char		vary_string [1];
} VARY;



main (argc, argv)
    int		argc;
    char	*argv[];
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Process sql statements until utterly bored.
 *
 **************************************/
char	  statement [512];
SQLDA	  *sqlda;

/* Get database name */

if (argc < 2)
    {
    fprintf (stderr, "No database specified on command line\n");
    exit (1);
    }
++argv;
strcpy (db_name, *argv);

/* Open database and start tansaction */

READY;
START_TRANSACTION;

/*	Set up SQLDA for SELECTs, allow up to 20 fields to be
	selected */

sqlda = (SQLDA*) malloc (SQLDA_LENGTH (20));
sqlda->sqln = 20;

/* Prompt for SQL statements and process them */

while (1)
    {
    /* get command */
    get_statement (statement);

    /* Check for command to leave */
    if ((UPPER (statement[0]) == 'E') &&
        (UPPER (statement[1]) == 'X') &&
        (UPPER (statement[2]) == 'I') &&
        (UPPER (statement[3]) == 'T'))
         {
	 if (gds__trans)
	     COMMIT;
	 FINISH;
	 break;
         }
    if ((UPPER (statement[0]) == 'Q') &&
        (UPPER (statement[1]) == 'U') &&
        (UPPER (statement[2]) == 'I') &&
        (UPPER (statement[3]) == 'T'))
         {
	 if (gds__trans)
	     ROLLBACK;
	 FINISH;
	 break;
         }

    process_statement (statement, sqlda);
    }
exit (0);
}


static get_statement (statement)
    char *statement;
{
/**************************************
 *
 *	g e t _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Get an SQL statment, or QUIT/EXIT command to process
 *
 **************************************/
char	 *p;
short	  c, blank, done;

blank = 1;
while (blank)
    {
    printf ("SQL> ");
    blank = 1;
    p = statement;
    done = 0;
    while (!done)
        switch (c = getchar ())
            {
            case EOF:
                strcpy (statement, "EXIT");
                return;

            case '\n':
                if (p == statement || p [-1] != '-')
                    done = 1;
                else
                    {
                    p [-1] = ' ';
                    printf ("CON> ");
                    }
                break;

            case '\t':
            case ' ':
                *p++ = ' ';
                break;
	    
            default:
                *p++ = c;
                blank = 0;
                break;
            }
    *p = 0;
    }
}


static print_item (s, var)
    char	    **s;
    SQLVAR	  *var;
{
/**************************************
 *
 *	p r i n t _ i t e m
 *
 **************************************
 *
 * Functional description
 *	Print a SQL data item as described.
 *
 **************************************/
char		*p, *string;
char		d [20];
VARY		*vary;
short		dtype;
struct tm	times;

p = *s;

dtype = var->sqltype & ~1;
if ((var->sqltype & 1) && (*var->sqlind < 0))
    {
    /* If field was missing print <null> */

    if ((dtype == SQL_TEXT) || (dtype == SQL_VARYING))
        sprintf (p, "%-*s ", var->sqllen, "<null>");
    else if (dtype == SQL_DATE)
        sprintf (p, "%-20s ", "<null>");
    else
        sprintf (p, "%*s ", var->sqllen, "<null>");
    }
else
    {
    /* Print field according to its data type */

    if (dtype == SQL_SHORT)
	sprintf (p, "%6d ", *(short*) (var->sqldata));
    else if (dtype == SQL_LONG)
#if !(defined __osf__ && defined __alpha)
	sprintf (p, "%11ld ", *(long*) (var->sqldata));
#else
	sprintf (p, "%11ld ", *(int*) (var->sqldata));
    else if (dtype == SQL_QUAD)
	sprintf (p, "%19ld ", *(long*) (var->sqldata));
#endif
    else if (dtype == SQL_FLOAT)
	sprintf (p, "%f ", *(float*) (var->sqldata));
    else if (dtype == SQL_DOUBLE)
	sprintf (p, "%f ", *(double*) (var->sqldata));
    else if (dtype == SQL_TEXT)
	sprintf (p, "%.*s ", var->sqllen, var->sqldata);
    else if (dtype == SQL_DATE)
        {
        gds__decode_date (var->sqldata, &times);
	sprintf (d, "%2d-%s-%4d ", times.tm_mday, alpha_months 
[times.tm_mon],
		 times.tm_year + 1900);
        sprintf (p, "%-20s ", d);
        }
    else if (dtype == SQL_VARYING)
	{
	vary = (VARY*) var->sqldata;
        string = vary->vary_string;
        string[vary->vary_length] = 0;
	sprintf (p, "%-*s ", 
		var->sqllen, vary->vary_string);
	}
    }

while (*p)
    ++p;

*s = p;
}


static print_line (sqlda)
    SQLDA	  *sqlda;
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
 **************************************/
char	    *p, line [1024];
SQLVAR	   *var, *end;

p = line;

for (var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; 
var++)
    print_item (&p, var);

*p++ = '\n';
*p = 0;
p = line;
while (*p)
    putchar(*p++);
}



static process_statement (string, sqlda)
    char	  *string;
    SQLDA	  *sqlda;
{
/**************************************
 *
 *	p r o c e s s _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Prepare and execute a dynamic SQL statement.
 *
 **************************************/
long	   buffer [512];
short	   length, lines, alignment, type, offset;
SQLVAR	  *var, *end;

EXEC SQL WHENEVER SQLERROR GO TO punt;

if (!gds__trans)
	START_TRANSACTION;
EXEC SQL PREPARE Q1 INTO sqlda FROM :string;

/* If the statement isn't a select, execute it and be done */

if (!sqlda->sqld)
    {
    EXEC SQL EXECUTE Q1;
    return;
    }
else if (sqlda->sqld > sqlda->sqln)
    {
    printf ("Statment not executed, can only select %d items\n", sqlda->sqln);
    return;
    }

/* Otherwise, open the cursor to start things up */

EXEC SQL DECLARE C CURSOR FOR Q1;
EXEC SQL OPEN C;

/* Set up SQLDA to receive data */

offset = 0;

for (var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; 
var++)
    {
    length = alignment = var->sqllen;
    type = var->sqltype & ~1;
    if (type == SQL_BLOB)
        {
        printf ("Statment not executed, cannot select blob fields\n");
        return;
        }
    if (type == SQL_TEXT)
	alignment = 1;
    else if (type == SQL_VARYING)
	{
	length += sizeof (short);
	alignment = sizeof (short);
	}
    offset = ALIGN (offset, alignment);
    var->sqldata = (char*) buffer + offset;
    offset += length;
    offset = ALIGN (offset, sizeof (short));
    var->sqlind = (short*) ((char*) buffer + offset);
    offset += sizeof  (short);
    }

/* Fetch and print records until EOF */

for (lines = 0;; ++lines)
    {
    EXEC SQL FETCH C USING DESCRIPTOR sqlda;
    if (SQLCODE)
	break;
    if (!lines)
	printf ("\n");
    print_line (sqlda);
    }

if (lines)
    printf ("\n");

EXEC SQL CLOSE C;
EXEC SQL WHENEVER SQLERROR CONTINUE;
return;

punt: 
    /* printf ("Statement failed, SQLCODE = %d\n\n", SQLCODE); */
    gds__print_status (gds__status);
}
