/*
 *	PROGRAM:	Interbase Message file modify for escape chars
 *	MODULE:		modify_messages.e
 *	DESCRIPTION:	Allow modification of messages with translatation of escape sequences
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

static int	explicit_print (TEXT *);

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
SCHAR	facility [20], transtext [256], newtext[256];
SCHAR	innumber[4];
SCHAR	*p, *q;
SSHORT	number;

ib_printf ("\nHit Ctrl-D (or Ctrl-Z) at prompt to exit level\n");
ib_printf ("You will be prompted for fac_code and message number\n");
ib_printf ("Escape sequences will be translated to single bytes\n");

READY;
START_TRANSACTION;

for (;;)
    {
    ib_printf ("Facility: ");
    if (!gets (facility))
	break;
   FOR Y IN FACILITIES WITH Y.FACILITY CONTAINING facility
	ib_printf ("Message number: ");
	if (!gets (innumber))
	    break;
	number = atoi (innumber);
	FOR X IN MESSAGES WITH X.FAC_CODE = Y.FAC_CODE AND X.NUMBER = number
	    ib_printf (" Current text:");
            explicit_print (X.TEXT);
	    ib_printf ("\n New text: ");	    
            if (!gets (newtext))
		break;

	    MODIFY X USING

               p = X.TEXT;
               q = newtext;

            /* translate scape sequences to single bytes */
		while (*q)
		    {
		    if (*q == '\\')
			{
			*q++;
			switch (*q)
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
				ib_printf ("\n\n*** Escape sequence not understood; being copied unchanged  ***\n\n");
				*p++ = '\\';
				*p++ = *q;
				break;
			    }
			*q++;				
			}
		    else
			*p++ = *q++;    
		}
		*p = 0;
	    END_MODIFY;
	END_FOR;
    END_FOR;
    }

COMMIT;
FINISH;
exit (FINI_OK);
}

static int explicit_print (
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
