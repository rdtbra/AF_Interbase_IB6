/*
 *	PROGRAM:	Interbase Message file edit program
 *	MODULE:		change_msgs.e
 *	DESCRIPTION:	Allow limited change of messages in database
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
#include "../jrd/gds.h"
#include "../jrd/common.h"

DATABASE DB = "msg.gdb";

#define FAC_SQL_POSITIVE	14
#define FAC_SQL_NEGATIVE	13

#define LOWER_A         'a'
#define UPPER_A         'A'
#define LOWER_Z         'z'
#define UPPER_Z         'Z'

static void	ascii_str_upper (UCHAR *);
static void	explicit_print (TEXT *);
static int	get_sql_class (UCHAR *);
static int	get_sql_subclass (UCHAR *);
static int	get_symbol (UCHAR *);
static SSHORT	mustget (SCHAR *);
static int	translate (SCHAR *, SCHAR *, SSHORT);

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
 *	Top level routine.  
 *
 **************************************/
SCHAR	facility [20], text [256], module [32], routine [32];
UCHAR   sql_class [32], sql_sub_class [32];
SCHAR	input [200], yesno [100];
SCHAR	symbol[30];
SCHAR	nstring [32];
SCHAR	*p, *q;
SSHORT	count;
SSHORT  sql_number;
SSHORT	msg_number;

ib_printf ("\nHit Ctrl-D (or Ctrl-Z) at prompt to exit level\n");
ib_printf ("You will be prompted for facility, module, routine and message text\n");
ib_printf ("You *must* enter module and routine names for each message; be prepared\n");
ib_printf ("You may assign an optional symbol for the message\n");
ib_printf ("Escape sequences may be entered and will be translated to single bytes\n");

READY;
START_TRANSACTION;

for (;;)
    {
    ib_printf ("Facility: ");
    if (!gets (facility))
	break;
    count = 0;
    msg_number = 0;
    ascii_str_upper (facility);
    FOR X IN FACILITIES WITH X.FACILITY = facility
	count++;
	for (;;)
	    {
            BOOLEAN sys_error = FALSE;
	    
	    ib_printf ("Message number (%d) ? ", msg_number+1);
	    if (!gets (input))
	        break;
	    if (!input[0])
		msg_number++;
	    else
		msg_number = atoi (input);
	    if (msg_number <= 0)
		break;
	    ib_printf ("Facility: %s\n", X.FACILITY);
	    FOR Y IN MESSAGES WITH
		Y.FAC_CODE EQ X.FAC_CODE
		AND Y.NUMBER EQ msg_number;

		ib_printf (" Message: %d\n", Y.NUMBER);
		ib_printf ("  Module: %s\n", Y.MODULE);
		ib_printf (" Routine: %s\n", Y.ROUTINE);
		ib_printf ("    Text: ");
		explicit_print (Y.TEXT);
		ib_printf ("\n");
		ib_printf ("  Symbol: %s\n", Y.SYMBOL);
	    END_FOR

            FOR Z IN SYSTEM_ERRORS WITH X.FAC_CODE EQ Z.FAC_CODE AND 
	        Z.NUMBER EQ msg_number;
    	        ib_printf ("SQLCODE: %d\n", Z.SQL_CODE);
	        ib_printf ("   SQL_CLASS:    %s\n", Z.SQL_CLASS);
	        ib_printf ("SQL_SUBCLASS: %s\n", Z.SQL_SUBCLASS);
	        sys_error = TRUE;
      	    END_FOR

	    ib_printf (" Modify? ");
	    if (mustget (yesno) && (yesno [0] == 'y' || yesno [0] == 'Y'))
	        {
	        module [0] = 0;
	        ib_printf (" Module: ");
	        if (!gets (module))
		    break;
	        routine [0] = 0;
	        ib_printf ("Routine: ");
	        if (!gets (routine))
	            break;
	        text [0] = 0;
	        ib_printf ("   Text: ");
	        if (!gets (text))
		    break;
	        symbol [0] = 0;
	        ib_printf (" Symbol: ");
	        if (!gets (symbol))
		    break;
	        if (sys_error || X.FAC_CODE == 0)
		    {
    		    ib_printf ("SQLCODE: ");
  		    if (mustget (nstring))
			sql_number = atoi (nstring);
                    if (!get_sql_class (sql_class))
		        /* continue */ ;
		    if (!get_sql_subclass (sql_sub_class))
		        /* continue */ ;
		    }

	        FOR Y IN MESSAGES WITH
		  Y.FAC_CODE EQ X.FAC_CODE
		  AND Y.NUMBER EQ msg_number;
		    MODIFY Y USING
		    if (module [0])
			strcpy (Y.MODULE, module);
		    if (routine [0])
			strcpy (Y.ROUTINE, routine);
		    if (text [0])
                    	while (!translate (text, Y.TEXT, sizeof (Y.TEXT)))
                        {
                        ib_printf ("Message too long: max length: %d\n",
                                	sizeof (Y.TEXT));
			(void) mustget (text);
			}
		    if (symbol [0])
			strcpy (Y.SYMBOL, symbol);
		    END_MODIFY;
		END_FOR;

	        FOR Z IN SYSTEM_ERRORS WITH
	    	  Z.FAC_CODE EQ X.FAC_CODE
	    	  AND Z.NUMBER EQ msg_number;
		    MODIFY Z USING
			if (symbol [0])
			    strcpy (Z.GDS_SYMBOL, symbol);
			if (sql_number != 0)
			    Z.SQL_CODE = sql_number;
			if (sql_class [0])	
			    strcpy (Z.SQL_CLASS, sql_class);
			if (sql_sub_class [0])	
			    strcpy (Z.SQL_SUBCLASS, sql_sub_class);
		    END_MODIFY;
                END_FOR;
		}
	    }
    END_FOR;
    if (!count)
	{
	ib_printf ("Facilty %s not found\n  Known facilities are:\n", facility);
	FOR F IN FACILITIES SORTED BY F.FACILITY
	    ib_printf ("    %s\n", F.FACILITY);
	END_FOR;
	}
    }

ib_printf ("\n\nCommitting changes...");
COMMIT;
FINISH;
ib_printf ("done.\n");

exit (FINI_OK);
}

static void ascii_str_upper (
    UCHAR       *str)
{
/**************************************
 *
 *      a s c i i _ s t r _ u p p e r
 *
 **************************************
 *
 * Functional description
 *      change a string to all upper case
 *
 **************************************/
	       
while (*str)
    {
    /* subtract 32 if necessary */

    if (*str >= LOWER_A && *str <= LOWER_Z)
       *str += (UPPER_A - LOWER_A);
    str++;
    }
}

static void explicit_print (
    TEXT	*string)
{
/**************************************
 *
 *	e x p l i c i t _ p r i n t
 *
 **************************************
 *
 * Functional description
 *	Let it all hang out: print line
 *      with explicit \n \b \t \f etc.
 *      to make changing messages easy
 *
 **************************************/
TEXT *p;

p = string;

while (*p)
    {
    switch (*p)
        {
        case '\n':
            ib_putchar ('\\');
            ib_putchar ('n');
            break;
        case '\t':
            ib_putchar ('\\');
            ib_putchar ('t');
            break;
        case '\f':
            ib_putchar ('\\');
            ib_putchar ('f');
            break;
        case '\b':
            ib_putchar ('\\');
            ib_putchar ('b');
            break;
        case '\r':
            ib_putchar ('\\');
            ib_putchar ('r');
            break;
        case '\v':
            ib_putchar ('\\');
            break;
        case '\\':
            ib_putchar ('\\');
            ib_putchar ('\\');
            break;
        case '\"':
            ib_putchar ('\\');
            ib_putchar ('\"');
            break;
        case '\'':
            ib_putchar ('\\');
            ib_putchar ('\'');
            break;
        default:
            ib_putchar (*p);
        }
    *p++;
    }
}

static int get_sql_class (
    UCHAR 	*sql_class)
{
/**************************************
 *
 *	g e t _ s q l _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	get a two character sql_class string
 *	return TRUE if we get one, otherwise FALSE
 *
 **************************************/
SSHORT length;

while (1)
    {
    ib_printf ("   SQLCLASS: ");
    gets (sql_class);
    length = strlen (sql_class);    
    if (!length)
        break;

    if (length == 2)
	return TRUE;
    else
        ib_fprintf (ib_stderr, "Sqlclass is two characters!\n");
    }

return FALSE;
}

static int get_sql_subclass (
    UCHAR 	*sql_sub_class)
{
/**************************************
 *
 *	g e t _ s q l _ s u b c l a s s
 *
 **************************************
 *
 * Functional description
 *	get a three character sql_subclass string
 *	return TRUE if we get one, otherwise FALSE
 *
 **************************************/
SSHORT length;

while (1)
    {
    ib_printf ("SQLSUBCLASS: ");
    gets (sql_sub_class);
    length = strlen (sql_sub_class);    
    if (!length)
        break;

    if (length == 3)
	return TRUE;
    else
        ib_fprintf (ib_stderr, "Sqlsubclass is three characters!\n");
    }
    return FALSE;
}

static int get_symbol (
    UCHAR 	*symbol)
{
/**************************************
 *
 *	g e t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	insist on getting the symbol
 *	return TRUE when we get one
 *
 **************************************/
SSHORT length;

while (TRUE)
    {
    ib_fprintf (ib_stderr, "Symbols are required for system errors!\n");
    ib_printf (" Symbol: ");
    gets (symbol);
    if (strlen (symbol))
	return TRUE;
    }
}

static SSHORT mustget (
    SCHAR	*s)
{
/**************************************
 *
 *	m u s t g e t
 *
 **************************************
 *
 * Functional description
 *	gets & returns a string.  Returns FALSE
 *	if string is empty.
 *
 **************************************/

if (!gets (s))
    return FALSE;

return (s[0] != 0);
}

static int translate (
    SCHAR  	*source,
    SCHAR  	*target,
    SSHORT	length)
{
/**************************************
 *
 *	t r a n s l a t e
 *
 **************************************
 *
 * Functional description
 * 	make explicit escape sequences into
 *	ascii, returns length ok?
 *
 **************************************/
SCHAR    *p, *q;

p = target;
q = source;

while ( *q )
    {
    if (!--length)
        return 0;
    if ( *q == '\\' )
        { 
        *q++;
        switch ( *q )
            {
	    case 'n':
	        *p++ = '\n';
	        break;
	    case 't':
		*p++ = '\t';
		break;
	    case 'f':
	        *p++ = '\f';
		break;
	    case 'a':
	        *p++ = '\a';
	        break;
	    case 'b':
	        *p++ = '\b';
	        break;
	    case 'r':
	        *p++ = '\r';
	        break;
	    case 'v':
	        *p++ = '\v';
	        break;
	    case '\\':
	        *p++ = '\\';
	        break;
	    case '\"':
	        *p++ = '\"';
	        break;
	    case '\'':
	        *p++ = '\'';
	        break;
	    default:
	        ib_printf ("\n\n*** Escape sequence not understood; being copied unchanged ***\n\n");
	        *p++ = '\\';
	        *p++ = *q;
	    }
	    *q++;				
	}
    else
        *p++ = *q++;    
    }
*p = 0;

return 1;
}
