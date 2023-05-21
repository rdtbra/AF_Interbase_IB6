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

main()
{
char answer;
short found;
/* Create forms window */
gds__height = 20;
gds__width = 80;
CREATE_WINDOW;
/* Open database and start transaction */
READY;
START_TRANSACTION;
/* Loop until user leaves form without filling in a state code */
found = 1;
while (1)
	{
	FOR FORM F IN CITY_POPULATIONS
/* Set instructional message to be displayed in form.
* If user just entered state code for a non-existent * state, say so in the 
message. 
*/
		if (found)
			{
			strcpy (F.TAG, 
			"Enter State Code (enter nothing to exit)");
			found = 0;
			}
		else
			strcpy (F.TAG, 
			"State not found; Enter State Code (enter nothing to exit)");
/* Display form and await entering of state code */
		DISPLAY F DISPLAYING TAG ACCEPTING STATE
        	if (F.STATE.STATE == PYXIS__OPT_NULL) break;
		/* Look for state */
		FOR S IN STATES WITH S.STATE = F.STATE
			/* Note that state was found */
			found = 1;
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
			 * populations to be updated */
			DISPLAY F DISPLAYING STATE, STATE_NAME, 
			 CITY_POP_LINE.CITY,
				CITY_POP_LINE.POPULATION, TAG
				ACCEPTING CITY_POP_LINE.POPULATION;
			/* Perform modifications for any 
			 * updated populations */
			FOR_ITEM FC IN F.CITY_POP_LINE
				if (FC.POPULATION.STATE == 
				  PYXIS__OPT_USER_DATA)
					FOR C IN CITIES WITH C.CITY = 
					 FC.CITY
						AND C.STATE = F.STATE
						MODIFY C USING
							C.POPULATION = FC.POPULATION;
						END_MODIFY;	
					END_FOR;
			END_ITEM;
		END_FOR;
	END_FORM;
	}
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
