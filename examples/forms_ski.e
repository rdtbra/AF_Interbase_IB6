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
DATABASE DB = 'atlas.gdb'

static char state_list[7][5] = {"CT", "MA", "ME", "NH", "RI", 
"VT", "Exit"};

main()
/*******************************
*  m a i n
*
*  Prompt the user for a state, then display menu of options
*  until user wants to quit.
*
*******************************/
{
int state_idx;

READY;
START_TRANSACTION;

state_idx = get_state();

while (state_idx != 7)
   {   
   CASE_MENU "Choose one"
       MENU_ENTREE "View Ski Areas": 
           view_ski_areas (state_list[state_idx]);
       MENU_ENTREE "Store New Ski Area":
           store_ski_area (state_list[state_idx]);
       MENU_ENTREE "Pick New State":
           state_idx = get_state();
       MENU_ENTREE "Exit New England Directory":
           state_idx = 7;
   END_MENU;
   }

DELETE_WINDOW;
COMMIT;
FINISH;
exit (0);
}

get_state ()
/*******************************
*  g e t _ s t a t e
*
*  Create a dynamic menu containing the 6 New England
*  states plus an "Exit" option.  Return the state or
*  signal to quit.
*
*******************************/
{
int i;

FOR_MENU M

   strcpy (M.TITLE_TEXT, "Choose a state");
   M.TITLE_LENGTH = strlen (M.TITLE_TEXT);

   for (i = 0; i < 7; i++)
       {
       PUT_ITEM E IN M
           strcpy (E.ENTREE_TEXT, state_list[i]);
           E.ENTREE_LENGTH = 4;
           E.ENTREE_VALUE = i;
       END_ITEM;
       }

    DISPLAY M;
    return M.ENTREE_VALUE;

END_MENU;
}            

view_ski_areas (state)
char *state;
/*******************************
*  v i e w _ s t a t e
*
*  Display the ski areas in the state selected.
*
*******************************/
{
int count = 0;

FOR FORM F IN NE_SKI_DIR

    strcpy (F.STATE, state);

    FOR SK IN SKI_AREAS WITH SK.STATE = state
        PUT_ITEM P IN F.SKI_TRAILS
            strcpy (P.NAME, SK.NAME);
            strcpy (P.TYPE, SK.TYPE);
            strcpy (P.CITY, SK.CITY);
        END_ITEM;
        count++;
    END_FOR;

    if (!count)
        strcpy (F.TAG, "No ski areas listed.");
    else
        sprintf (F.TAG, "%d ski areas listed.", count);
    
    DISPLAY F DISPLAYING *;

END_FORM;
}

store_ski_area (state)
char *state;
/*******************************
*  s t o r e _ s k i _ a r e a
*
*  Store a ski area record in the state selected.
*
*******************************/
{

FOR FORM F IN NEW_SKI_AREA

    strcpy (F.STATE, state);

    DISPLAY F DISPLAYING STATE
    ACCEPTING NAME, TYPE, CITY;

    STORE SK IN SKI_AREAS USING
       strcpy (SK.NAME, F.NAME);
       strcpy (SK.TYPE, F.TYPE);
       strcpy (SK.CITY, F.CITY);
       strcpy (SK.STATE, F.STATE);
    END_STORE;

END_FORM;    
}
