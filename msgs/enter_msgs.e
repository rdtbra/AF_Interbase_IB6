/*
 *	PROGRAM:	Interbase Message file entry program
 *	MODULE:		enter_messages.e
 *	DESCRIPTION:	Allow entry of messages to database
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

static void	ascii_str_upper (UCHAR *);
static int	get_sql_class (UCHAR *);
static int	get_sql_code (SSHORT *);
static int	get_sql_subclass (UCHAR *);
static int	store_sql_msg (void);
static int	translate (UCHAR *, UCHAR *, SSHORT);

#define FAC_SQL_POSITIVE	14
#define FAC_SQL_NEGATIVE	13

#define	LOWER_A		'a'
#define	UPPER_A		'A'
#define LOWER_Z		'z'
#define UPPER_Z		'Z'

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

/* Note that the lengths of these variables are based on the lengths of the
 * fields in the message database */

UCHAR	facility [20], text [256], module [32], routine [32];
UCHAR	sql_class [32], sql_sub_class [32];
UCHAR	symbol[30], pub[1];
UCHAR	nstring [32];
UCHAR	*p, *q;
SSHORT	count;
SSHORT	sql_num, sign;
BOOLEAN public;

/* Setup the 'strings' so that element 0 is really 0 */
memset (facility, 0, sizeof (facility));
memset (text, 0, sizeof (text));
memset (module, 0, sizeof (module));
memset (routine, 0, sizeof (routine));
memset (sql_class, 0, sizeof (sql_class));
memset (sql_sub_class, 0, sizeof (sql_sub_class));
memset (symbol, 0, sizeof(symbol));
memset (pub, 0, sizeof (pub));
memset (nstring, 0, sizeof (nstring));

ib_printf ("\nHit Ctrl-D (or Ctrl-Z) at prompt to exit level\n");
ib_printf ("You will be prompted for facility, module, routine and message text\n");
ib_printf ("You *must* enter module and routine names for each message; be prepared\n");
ib_printf ("You will be returned a message number for the message\n");
ib_printf ("You may assign an optional symbol for the message\n");
ib_printf ("Escape sequences may be entered and will be translated to single bytes\n");

READY;
START_TRANSACTION;

for (;;)
    {
    ib_printf ("Facility: ");
    if (!gets (facility))
	break;
    ascii_str_upper (facility);
    count = 0;
    FOR X IN FACILITIES WITH X.FACILITY = facility
	count++;
        if (X.FAC_CODE == FAC_SQL_POSITIVE || X.FAC_CODE == FAC_SQL_NEGATIVE)
            {
            if (!store_sql_msg())
                break;
            }
        else
        {   
	MODIFY X USING
	    for (;;)
		{
		ib_printf (" Module: ");
		if (!gets (module))
        	    break;
		ib_printf (" Routine: ");
		if (!gets (routine))
		    break;
		ib_printf (" Text: ");
		if (!gets (text))
		    break;
		symbol[0] = 0;

		/* All JRD messages are public.  Only ask if entering messages
		 * in a component other than JRD */
		if (X.FAC_CODE != 0)
		    {
		    ib_printf ("Public [y/n]: ");
		    gets(pub);
		    if (*pub == 'Y' || *pub == 'y')
		      public = TRUE;
		    else
		      public = FALSE;
		    }
		else
		    public = TRUE;

		ib_printf (" Symbol: ");
		if (!gets (symbol))
		    break;

		if (public)
		    {
		    if (!symbol [0])
			if (!get_symbol(symbol))
        		    break;

	            if (!get_sql_code (&sql_num))
		        break;
		    
	            if (!get_sql_class (sql_class))
		        /* continue */ ;

	            if (!get_sql_subclass (sql_sub_class))
		        /* continue */ ;

		    STORE S IN SYSTEM_ERRORS USING
    		    	S.FAC_CODE = X.FAC_CODE;
    		    	S.NUMBER = X.MAX_NUMBER;

			S.SQL_CODE = sql_num;

		    	strcpy (S.GDS_SYMBOL, symbol);
		    	if (sql_class[0])
			{
			    strcpy (S.SQL_CLASS, sql_class);
			    S.SQL_CLASS.NULL = FALSE;
			}
		    	else
			    S.SQL_CLASS.NULL = TRUE;
		        if (sql_sub_class[0])
			    {
			    strcpy (S.SQL_SUBCLASS, sql_sub_class);
			    S.SQL_SUBCLASS.NULL = FALSE;
		            }
		        else
			    S.SQL_SUBCLASS.NULL = TRUE;
    		    END_STORE;

		    }

		STORE Y IN MESSAGES USING
    		    Y.FAC_CODE = X.FAC_CODE;
    		    Y.NUMBER = X.MAX_NUMBER;
		    strcpy (Y.MODULE, module);
		    strcpy (Y.ROUTINE, routine); 
		    strcpy (Y.SYMBOL, symbol); 
                    if (!translate (text, Y.TEXT, sizeof (Y.TEXT)))
                        {
                        ib_printf ("Message too long: max length: %d\n",
                               sizeof (Y.TEXT));
                        break;
                        }
    		    ib_printf ("Message number: %d\n", X.MAX_NUMBER);
    		    X.MAX_NUMBER = X.MAX_NUMBER+1;
    		END_STORE;
		}
	END_MODIFY;
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

COMMIT;
FINISH;
exit (FINI_OK);
}

static void ascii_str_upper (
    UCHAR  	*str)
{
/**************************************
 *
 *	a s c i i _ s t r _ u p p e r
 *
 **************************************
 *
 * Functional description
 * 	change a string to all upper case
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

static get_sql_class (
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
    ib_printf (" SQLCLASS: ");
    gets (sql_class);
    length = strlen (sql_class);    
    if (length == 0 || !isalnum (sql_class [0]))
        break;

    if (length == 2)
	return TRUE;
    else
        ib_fprintf (ib_stderr, "Sqlclass is two characters!\n");
    }

return FALSE;
}

static int get_sql_code (
    SSHORT	*sql_code)
{
/**************************************
 *
 *	g e t _ s q l _ c o d e
 *
 **************************************
 *
 * Functional description
 *	prompt for sql_code and convert it to a number
 *	return TRUE on success, otherwise FALSE
 *	Issue a warning if the SQL code is not in the SQLERR facility
 *
 **************************************/
UCHAR	nstring [32];
UCHAR	*p;
SSHORT	sign;
SSHORT	sql_num;
SSHORT  srch_num, got_it;
int	ret;

ret = FALSE;
while (1)
    {
    ib_printf (" SQLCODE: ");
    if (!gets (nstring))
	break;
    p = nstring;
    sign = 1;
    sql_num = 0;

    /* skip spaces */
    while (*p && *p == ' ')
	p++;

    /* Allow for leading sign */
    if (*p == '+')
	p++;
    else if (*p == '-')
	{
	p++;
	sign = -1;
	}

    /* skip spaces again */
    while (*p && *p == ' ')
	p++;

    /* and if anything is left, convert to a number .. */
    if (*p && isdigit (*p))
	{
	sql_num = sign * atoi (p);
	*sql_code = sql_num;
	ret = TRUE;
	break;
	}
    else
	{
	ib_fprintf (ib_stderr, "SQLCode is now required!\n");
	}
    }

/* make sure that the SQL code is in the SQLERR facility */
if (ret)
    {
    got_it = 0;
    if (sql_num < 0)
	{
        srch_num = 1000 + sql_num;
        FOR M IN MESSAGES WITH M.FAC_CODE = FAC_SQL_NEGATIVE 
	    AND M.NUMBER = srch_num 
                got_it = 1;
        END_FOR;
	}
    else
	{
        srch_num = sql_num;
        FOR M IN MESSAGES WITH M.FAC_CODE = FAC_SQL_POSITIVE 
	    AND M.NUMBER = srch_num 
                got_it = 1;
        END_FOR;
	}
    if (!got_it)
	{
 	ib_printf ("Warning: SQL code %d is not in the SQLERR facility.\n", sql_num);
	ib_printf ("You will need to add it there too.\n");
	}
    }
return ret;
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
    ib_printf (" SQLSUBCLASS: ");
    gets (sql_sub_class);
    length = strlen (sql_sub_class);    
    if (length == 0 || !isalnum (sql_sub_class [0]))
        break;

    if (length == 3)
	return TRUE;
    else
	{
        ib_fprintf (ib_stderr, "Sqlsubclass is three characters!\n");
	continue;
	}
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
 *	or FALSE if user breaks out
 *
 **************************************/
SSHORT length;

while (1)
    {
    ib_fprintf (ib_stderr, "Symbols are required for system errors!\n");
    ib_printf (" Symbol: ");
    gets (symbol);
    if (!strlen (symbol) || !isalnum (symbol [0]))
	return FALSE;
    else
	return TRUE;
    }
}

static int store_sql_msg (void)
{
/**************************************
 *
 *	s t o r e _ s q l _ m s g
 *
 **************************************
 *
 * Functional description
 *	sql messages key off the sql code. The
 *	compound message number works out to
 *	the offset from 1400.
 *
 **************************************/
UCHAR	text [256], nstring [32];
UCHAR	*p, *q;
SSHORT	sql_num;

for (;;)
    {
    ib_printf ("Enter the sqlcode: ");
    if (!gets (nstring))
        break;;

    sql_num = atoi (nstring);
    ib_printf (" Text: ");
    if (!gets (text))
        break;

    STORE Y IN MESSAGES USING
       if (sql_num < 0)
           {
           Y.NUMBER = 1000 + sql_num;
           Y.FAC_CODE = FAC_SQL_NEGATIVE;
           }
       else
           {
           Y.NUMBER =  sql_num;
           Y.FAC_CODE = FAC_SQL_POSITIVE;
           }

    /* translate escape sequences to single bytes */

    if (!translate (text, Y.TEXT, sizeof (Y.TEXT)))
        {
        ib_printf ("Message too long: max length: %d\n", sizeof (Y.TEXT));
        return 0;
        }
    END_STORE;
    }

return 1;
} 

static translate (
    UCHAR  	*source,
    UCHAR  	*target,
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
 **************************************/
UCHAR    *p, *q;

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
