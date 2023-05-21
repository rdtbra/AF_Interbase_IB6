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

DATABASE DB = 'atlas.gdb'

int	* state_menu_handle;

main()
{
char answer;
short found;
char	* valuep;

/* Open database and start transaction */
READY;
START_TRANSACTION;

/* Create the menu of state names.  The value of each 
 * entree is a pointer to the state code for that  
 * entree */

FOR_MENU (MENU_HANDLE state_menu_handle) M

	PUT_ITEM E IN M
		strcpy (E.ENTREE_TEXT, "EXIT");
		E.ENTREE_LENGTH = strlen (E.ENTREE_TEXT);
		E.ENTREE_VALUE = 0;
	END_ITEM

	FOR S IN STATES SORTED BY ASCENDING S.STATE_NAME
		PUT_ITEM E IN M
			strcpy (E.ENTREE_TEXT, S.STATE_NAME);
			E.ENTREE_LENGTH = strlen (E.ENTREE_TEXT);
			valuep = (char *) malloc (strlen (S.STATE) 
			  + 1);
			strcpy (valuep, S.STATE);
			E.ENTREE_VALUE = (long) valuep;
		END_ITEM
	END_FOR

END_MENU


/* Create forms window */
gds__height = 20;
gds__width = 80;
CREATE_WINDOW;

/* Loop until user selects EXIT from the States menu. */

while (1)
	{
	FOR FORM F IN CITY_POPULATIONS

		/* Set instructional message to be displayed 
		 * in form. */

		strcpy (F.TAG, "Choose a state (Choose EXIT when finished)");

		/* Display form and await selection of state */

		DISPLAY F DISPLAYING TAG NO_WAIT;

		FOR_MENU (MENU_HANDLE state_menu_handle) M
		     strcpy (M.TITLE_TEXT, "States Menu");
		     M.TITLE_LENGTH = strlen (M.TITLE_TEXT);

 		     DISPLAY M TRANSPARENT VERTICAL
		     if (M.ENTREE_VALUE == 0)
			 break;
		     valuep = (char *) M.ENTREE_VALUE;
		END_MENU

		/* Look for state */

		FOR S IN STATES WITH S.STATE = valuep

			/* Put city information into subform */
			FOR C IN CITIES WITH C.STATE = S.STATE 
			 SORTED BY C.CITY
				PUT_ITEM FC IN F.CITY_POP_LINE
					strcpy (FC.CITY, C.CITY);
					FC.POPULATION = C.POPULATION;
				END_ITEM;
			END_FOR;

			/* Put state information into form */
			strcpy (F.STATE, S.STATE);
			strcpy (F.STATE_NAME, S.STATE_NAME);
			strcpy (F.TAG, "Update populations if needed");

			/* Display current form and allow 
			   populations to be updated */
			DISPLAY F DISPLAYING STATE, STATE_NAME, 
			 CITY_POP_LINE.CITY,
				CITY_POP_LINE.POPULATION, TAG
				ACCEPTING CITY_POP_LINE.POPULATION;

			/* Perform modifications for any updated 
			 * populations */
			FOR_ITEM FC IN F.CITY_POP_LINE
				if (FC.POPULATION.STATE == 
				 PYXIS__OPT_USER_DATA)
					FOR C IN CITIES WITH C.CITY = FC.CITY
						AND C.STATE = F.STATE
						MODIFY C USING
							C.POPULATION = FC.POPULATION;
						END_MODIFY;	
					END_FOR;
			END_ITEM;
		END_FOR;

	END_FORM;
	}

/* Deallocate the entree values for the menu, except 
	the special EXIT entree.  */

FOR_MENU (MENU_HANDLE state_menu_handle) M

    FOR_ITEM E IN M
        if (E.ENTREE_VALUE != 0)
	    free (E.ENTREE_VALUE);
    END_ITEM

END_MENU


/* Make form go away */
DELETE_WINDOW;

/* Check to see whether or not to commit updates */

printf ("Do you want to commit the updates (Y/N): ");
fflush (stdout);
answer = getchar();
if ((answer == 'Y') || (answer == 'y'))
	COMMIT
else
	ROLLBACK;

/* Close down */
FINISH;
exit (0);
}
