/*
 * It's time to update and print the tourist information we have for New
 * England and the Pacific Northwest.  This program first prints and
 * optionally modifies the tourist information for each state, then creates 
 * a database that is a guide to Northern US seacoasts.
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

DATABASE atlas = FILENAME "atlas.gdb";
DATABASE guide = FILENAME "c_guide.gdb";

#include <stdio.h>

static char *state_list[] =
	{"ME", "NH", "MA", "RI", "CT", "OR", "WA", "AK", 0};

#define GET_STRING(p,b)	get_string (p, b, sizeof (b))
#define FALSE 0
#define TRUE 1

main ()
{
/**************************************
 *
 *   m a i n
 *
 **************************************
 *
 * Functional description
 *
 * The main routine establishes a list of
 * states from the main atlas database.  Its
 * subroutines allow the user to update guidebook 
 * descriptions of those states, and store them
 * in a separate database.
 **************************************/


READY atlas;
READY "nc_guide.gdb" AS guide;

clean_up (state_list);
transfer_data (state_list);

FINISH atlas;
FINISH guide;
exit (0);
}

static clean_up (states_list)
char	*states_list[];

{
/**************************************
 *
 *   c l e a n _ u p
 *
 **************************************
 *
 * Functional description
 *
 * At the users' choice, edit the tourism blob for selected states,
 * load a new one from a named file, or accept text from standard input
 **************************************/

char	filename[40], choice[4], *state;
BSTREAM	*tour;
int	*update_tr;
short	c;

/* start a named transaction for update, reserving the necessary relations */

update_tr = 0;
START_TRANSACTION update_tr CONCURRENCY READ_WRITE 
	RESERVING atlas.tourism FOR WRITE,
	atlas.states FOR READ;

for (state = *states_list++; state; state = *states_list++ )
	{                                                                    
	FOR (TRANSACTION_HANDLE update_tr) s IN atlas.STATES 
		CROSS t IN atlas.TOURISM OVER state WITH s.state = state
		{
		printf ("Here's the information on %s.\n\n", s.state_name);
		BLOB_display (&t.guidebook, atlas, update_tr, "guide_book");
		GET_STRING ("\nIs this information up to date? (Y/N) ", choice);
		if (*choice != 'y' && *choice != 'Y')
		MODIFY t USING
			{
 			GET_STRING ("\nEnter F to update from a file, E to edit the description ",
				choice); 
			if (*choice == 'F' || *choice == 'f')
				{
				GET_STRING ("Enter full path name of input file", filename);
				BLOB_load (&t.guidebook, atlas, update_tr, filename);
				}
			else if (*choice == 'E' || *choice == 'e')
				BLOB_edit (&t.guidebook, atlas, update_tr, "guidebook");
			else
				{
				printf ("Enter new information from standard input.  ");
          			printf ("Terminate with <EOF>.\n");
          			if (tour = Bopen (&t.guidebook, atlas, update_tr, "w"))
					{
					while ((c = getchar ()) != EOF)
						putb (c, tour);
					rewind (stdin);
					}
				BLOB_close (tour);
          			}
			}
		END_MODIFY;
		}
	END_FOR;
	}
COMMIT update_tr;
}


static get_string (prompt_string, response_string, length)
    char	*prompt_string, *response_string;
    int		length;
{
/**************************************
 *
 *	g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Write a prompt and read a string.  If the string overflows,
 *	try again.  If we get an EOF, return FAILURE.
 *
 **************************************/
char	*p;
short	l, c;

while (1)
    {
    printf ("%s: ", prompt_string);
    for (p = response_string, l = length; l; --l)
	{
	c = getchar();
	if (c == EOF)
	    return FALSE;
	if (c == '\n')
	    {
	    *p = 0;
	    return TRUE;
	    }
	*p++ = c;
	}
    while (getchar() != '\n')
	;
    printf ("** Maximum field length is %d characters **\n", length - 1);
    }
}

static transfer_data (states_list)
	char	*states_list[];

{
/**************************************
 *
 *   t r a n s f e r _ d a t a
 *
 **************************************
 *
 * Functional description
 *
 * Move the tourism information for selected
 * states to the guide_book database.
 **************************************/

int	*trans_tr;
short	c;
BSTREAM	*b_in, *b_out;
char	*state;

trans_tr = 0;

START_TRANSACTION trans_tr CONCURRENCY READ_WRITE
	RESERVING atlas.tourism FOR READ,
	guide.tourism FOR WRITE;

for (state = *states_list++; state; state = *states_list++ )
	{                                                                    
	FOR (TRANSACTION_HANDLE trans_tr) s IN atlas.STATES 
		CROSS t IN atlas.TOURISM OVER state WITH s.state = state
		{
		STORE (TRANSACTION_HANDLE trans_tr) new IN guide.TOURISM USING
			strcpy (new.state_name, s.state_name);
			strcpy (new.state, s.state);
			strcpy (new.city, t.city);
          		strcpy (new.zip, t.zip); 

			/* One way to copy a blob */
			CREATE_BLOB n_addr IN new.office;
			FOR o_addr IN t.office
				strncpy (n_addr.SEGMENT, o_addr.SEGMENT, sizeof(n_addr.SEGMENT));
				n_addr.LENGTH = o_addr.LENGTH;
				PUT_SEGMENT n_addr;
			END_FOR;
			CLOSE_BLOB n_addr;

			/* another way to copy a blob */
			b_in = Bopen (&t.guidebook, atlas, trans_tr, "r");
			b_out = Bopen (&new.guidebook, guide, trans_tr, "w");
			while ((c = getb (b_in)) != EOF)
	   			putb (c, b_out);
    	 		BLOB_close (b_in);
   	   		BLOB_close (b_out);
		END_STORE;
 		}
	END_FOR;
	}
PREPARE trans_tr
	ON_ERROR
		{
		printf ("Error preparing a multi_database transaction\n");
		printf ("Please manually rollback limbo transactions in GUIDE and ATLAS\n");
		ROLLBACK trans_tr;
		return;
		}
	END_ERROR;

COMMIT trans_tr
	ON_ERROR
		{
		printf ("Error committing a prepared multi_database transaction\n");
		printf ("Please manually commit limbo transactions in GUIDE and ATLAS\n");
		}
	END_ERROR;
}








                                                                             
 
