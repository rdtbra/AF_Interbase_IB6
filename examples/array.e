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
DATABASE DB = 'atlas.gdb';

#define RAIN 1
#define TEMP 2

double tot_array();
double avg_array();
double avg_month();

double rain_array [12];
double temperature_array [12];

BASED_ON CITIES.CITY city;
BASED_ON CITIES.STATE state;

int *trans;

main()
{
	int cont = 1;

	READY;

	START_TRANSACTION;

        /* loop at main menu until EXIT is choosen */

	while (cont)
		{

		CASE_MENU "Array Demo - Pick an Operation"

			MENU_ENTREE        "Store / Modify Temperatures" :
				mod_temp();
	
			MENU_ENTREE        "Store / Modify Rain Fall" :
				mod_rain();
	
			MENU_ENTREE        "Report" :
				reports();
	
	       		MENU_ENTREE        "Exit"        :
				cont = 0;
		END_MENU;

		}

	COMMIT;

	DELETE_WINDOW;

	FINISH;
	exit (0);
}
/***********************************************************/
/*                                                         */
/* modify rain fall figures for a city and state           */
/* combination for 1973 - 1975                             */
/*                                                         */
/***********************************************************/
int mod_rain()
{
	int year;
	int i, j;
	int loop = 1;
	int loop2 = 0;

	trans = 0;

	START_TRANSACTION trans;
        
	/* go and get a city & state to operate on */

	if (get_city_state() == -1)
		{
		ROLLBACK trans;
		return;
		}

	/* loop on year to modify until EXIT is choosen */

	while (year = get_year(RAIN))
		{

		FOR FORM (TRANSACTION_HANDLE trans) F IN ARRAYS
                     
			FOR (TRANSACTION_HANDLE trans) FIRST 1 C IN CITIES WITH C.CITY = city AND C.STATE = state

				/* check and see if array had previously been populated */
				/* if not, set local array to all zero's                */
				/* else copy in values from database to local array     */

				if (C.RAIN_ARRAY.NULL == GDS__FALSE)
					{
					j = year - 1973;
					for (i = 0 ; i < 12; i++)
						rain_array[i] = C.RAIN_ARRAY[j][i];
					}
				else
					for (i = 0; i < 12; i++)
						rain_array[i] = 0;

			END_FOR;

			/* assign local array values to form fields */

			F.M1 = rain_array [0];
			F.M2 = rain_array [1];
			F.M3 = rain_array [2];
			F.M4 = rain_array [3];
			F.M5 = rain_array [4];
			F.M6 = rain_array [5];
			F.M7 = rain_array [6];
			F.M8 = rain_array [7];
			F.M9 = rain_array [8];
			F.M10 = rain_array [9];
			F.M11 = rain_array [10];
			F.M12 = rain_array [11];
	
			strcpy (F.MSG_LINE1, "          Store / Modify Rain Fall Entry");
			strcpy (F.MSG_LINE2, "Enter Rain Fall - F1 to SAVE or F2 to ROLLBACK");
			strcpy (F.STATE, state);
			strcpy (F.CITY, city);
			F.YEAR = year;

			loop = 1;
			loop2 = 0;

			while (loop)
				{

				DISPLAY F 
					DISPLAYING *
					CURSOR ON M1
					ACCEPTING M1, M2, M3, M4, M5, M6, M7, M8, M9, M10, M11, M12;


				switch (F.TERMINATOR) {
					case PYXIS__KEY_PF1:
							loop = 0;
							break;
					case PYXIS__KEY_PF2:
							loop2 = 1;
							loop = 0;
							break;
					default:
							strcpy (F.MSG_LINE2, "INVALID KEY - F1 to SAVE or F2 to ROLLBACK");
							break;
					}

				} /* while (loop) */

			/* if PF2 is pressed then don't update values in database */

			if (loop2)
				continue;

			/* update database using values in form */
			
			FOR (TRANSACTION_HANDLE trans) C IN CITIES WITH C.CITY = city AND C.STATE = state
				MODIFY C USING
					j = year - 1973;
					C.RAIN_ARRAY[j][0] = F.M1;
					C.RAIN_ARRAY[j][1] = F.M2;
					C.RAIN_ARRAY[j][2] = F.M3;
					C.RAIN_ARRAY[j][3] = F.M4;
					C.RAIN_ARRAY[j][4] = F.M5;
					C.RAIN_ARRAY[j][5] = F.M6;
					C.RAIN_ARRAY[j][6] = F.M7;
					C.RAIN_ARRAY[j][7] = F.M8;
					C.RAIN_ARRAY[j][8] = F.M9;
					C.RAIN_ARRAY[j][9] = F.M10;
					C.RAIN_ARRAY[j][10] = F.M11;
					C.RAIN_ARRAY[j][11] = F.M12;
				END_MODIFY;
			END_FOR;
                        
		END_FORM;

		}

	COMMIT trans;

	return;
}
/***********************************************************/
/*                                                         */
/* modify temperature figures for a city and state         */
/* combination for 1973 - 1975                             */
/*                                                         */
/***********************************************************/
int mod_temp()
{
	int year;
	int i, j;
	int loop = 1;
	int loop2 = 0;
                                              
	trans = 0;

	START_TRANSACTION trans;

	/* go and get a city & state to operate on */

	if (get_city_state() == -1)
		{
		ROLLBACK trans;
		return;
		}

	/* loop on year to modify until EXIT is choosen */

	while (year = get_year(TEMP) )
		{

		FOR FORM (TRANSACTION_HANDLE trans) F IN ARRAYS
                     
			FOR (TRANSACTION_HANDLE trans) FIRST 1 C IN CITIES WITH C.CITY = city AND C.STATE = state

				/* check and see if array had previously been populated */
				/* if not, set local array to all zero's                */
				/* else copy in values from database to local array     */

				if (C.TEMPERATURE_ARRAY.NULL == GDS__FALSE)
					{
					j = year - 1973;
					for (i = 0; i < 12; i++)
						temperature_array[i] = C.TEMPERATURE_ARRAY[j][i];
					}
				else
					for (i = 0; i < 12; i++)
						temperature_array[i] = 0;

			END_FOR;

			/* assign local array values to form fields */

			F.M1 = temperature_array [0];
			F.M2 = temperature_array [1];
			F.M3 = temperature_array [2];
			F.M4 = temperature_array [3];
			F.M5 = temperature_array [4];
			F.M6 = temperature_array [5];
			F.M7 = temperature_array [6];
			F.M8 = temperature_array [7];
			F.M9 = temperature_array [8];
			F.M10 = temperature_array [9];
			F.M11 = temperature_array [10];
			F.M12 = temperature_array [11];
	
			strcpy (F.MSG_LINE1, "          Store / Modify Temperatures Entry");
			strcpy (F.MSG_LINE2, "Enter Temperatures - F1 to SAVE or F2 to ROLLBACK");
			strcpy (F.STATE, state);
			strcpy (F.CITY, city);
			F.YEAR = year;

			loop = 1;
			loop2 = 0;

			while (loop)
				{

				DISPLAY F 
					DISPLAYING *
					CURSOR ON M1
					ACCEPTING M1, M2, M3, M4, M5, M6, M7, M8, M9, M10, M11, M12;

				switch (F.TERMINATOR) {
					case PYXIS__KEY_PF1:
							loop = 0;
							break;
					case PYXIS__KEY_PF2:
							loop2 = 1;
							loop = 0;
							break;
					default:
							strcpy (F.MSG_LINE2, "INVALID KEY - F1 to SAVE or F2 to ROLLBACK");
							break;
					}

				} /* while (loop) */

			/* if PF2 is pressed then don't update values in database */

			if (loop2)
				continue;

			/* update database using values in form */
			
			FOR (TRANSACTION_HANDLE trans) C IN CITIES WITH C.CITY = city AND C.STATE = state
				MODIFY C USING
					j = year - 1973;
					C.TEMPERATURE_ARRAY[j][0] = F.M1;
					C.TEMPERATURE_ARRAY[j][1] = F.M2;
					C.TEMPERATURE_ARRAY[j][2] = F.M3;
					C.TEMPERATURE_ARRAY[j][3] = F.M4;
					C.TEMPERATURE_ARRAY[j][4] = F.M5;
					C.TEMPERATURE_ARRAY[j][5] = F.M6;
					C.TEMPERATURE_ARRAY[j][6] = F.M7;
					C.TEMPERATURE_ARRAY[j][7] = F.M8;
					C.TEMPERATURE_ARRAY[j][8] = F.M9;
					C.TEMPERATURE_ARRAY[j][9] = F.M10;
					C.TEMPERATURE_ARRAY[j][10] = F.M11;
					C.TEMPERATURE_ARRAY[j][11] = F.M12;
				END_MODIFY;
			END_FOR;
                        
		END_FORM;

		}

	COMMIT trans;

	return;
}
/***********************************************************/
/*                                                         */
/* print a report for a city and state combination that    */
/* includes all the rain fall and temperature information  */
/* for 1973 through 1975.                                  */
/*                                                         */
/***********************************************************/
int reports()
{
	int year;
	int i, j;
	int loop = 1;
	int loop2 = 0;
	char *under_string  = "\t\t -----  -----  -----  -----  -----  -----  -----  -----  -----  -----  -----  -----   ------  ------\n";
	char *under_string2 = "\t\t -----  -----  -----  -----  -----  -----  -----  -----  -----  -----  -----  -----\n";
	char *print_string  = "\t%d\t %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f   %6.2f  %6.2f\n";
	char *print_string2 = "\tAvg\t %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f  %5.2f\n";
	char print_str[256];

	trans = 0;

	START_TRANSACTION trans;
                            
	/* go and get a city & state to operate on */

	get_city_state();

	FOR (TRANSACTION_HANDLE trans) FIRST 1 C IN CITIES WITH C.CITY = city AND C.STATE = state

		printf ("\n\n\t\t\t\t\tTemperature and Rain Fall Report\n\n");
		printf ("\n\t\t\t\t\t\t%s, %s\n\n\n", city, state);

		printf ("\t\t  Jan    Feb    Mar    Apr    May    Jun    Jul    Aug    Sep    Oct    Nov    Dec    Total    Avg.\n");
		printf ("\t\t----------------------------------------------------------------------------------------------------\n\n");

		printf ("Rain Fall\n");
		for (j = 1973; j < 1976; j++)
			{
			sprintf ( print_str, print_string,
				j,
				C.RAIN_ARRAY[j-1973][0],
				C.RAIN_ARRAY[j-1973][1],
				C.RAIN_ARRAY[j-1973][2],
				C.RAIN_ARRAY[j-1973][3],
				C.RAIN_ARRAY[j-1973][4],
				C.RAIN_ARRAY[j-1973][5],
				C.RAIN_ARRAY[j-1973][6],
				C.RAIN_ARRAY[j-1973][7],
				C.RAIN_ARRAY[j-1973][8],
				C.RAIN_ARRAY[j-1973][9],
				C.RAIN_ARRAY[j-1973][10],
				C.RAIN_ARRAY[j-1973][11],
				tot_array (C.RAIN_ARRAY, j-1973),
				avg_array (C.RAIN_ARRAY, j-1973) );
			printf (print_str);
			}
		printf (under_string);
		for (i = 0; i < 12; i++) 
			rain_array[i] = avg_month (C.RAIN_ARRAY, i);
		sprintf ( print_str, print_string2,
				rain_array [0],
				rain_array [1],
				rain_array [2],
				rain_array [3],
				rain_array [4],
				rain_array [5],
				rain_array [6],
				rain_array [7],
				rain_array [8],
				rain_array [9],
				rain_array [10],
				rain_array [11]);
		printf (print_str);
                                                
		printf ("\n");

		printf ("Temperatures\n");
		for (j = 1973; j < 1976; j++)
			{
			sprintf ( print_str, print_string,
				j,
				C.TEMPERATURE_ARRAY[j-1973][0],
				C.TEMPERATURE_ARRAY[j-1973][1],
				C.TEMPERATURE_ARRAY[j-1973][2],
				C.TEMPERATURE_ARRAY[j-1973][3],
				C.TEMPERATURE_ARRAY[j-1973][4],
				C.TEMPERATURE_ARRAY[j-1973][5],
				C.TEMPERATURE_ARRAY[j-1973][6],
				C.TEMPERATURE_ARRAY[j-1973][7],
				C.TEMPERATURE_ARRAY[j-1973][8],
				C.TEMPERATURE_ARRAY[j-1973][9],
				C.TEMPERATURE_ARRAY[j-1973][10],
				C.TEMPERATURE_ARRAY[j-1973][11],
				tot_array (C.TEMPERATURE_ARRAY, j-1973),
				avg_array (C.TEMPERATURE_ARRAY, j-1973) );
			printf (print_str);
			}
		printf (under_string);
		for (i = 0; i < 12; i++) 
			temperature_array[i] = avg_month (C.TEMPERATURE_ARRAY, i);
		sprintf ( print_str, print_string2,
				temperature_array [0],
				temperature_array [1],
				temperature_array [2],
				temperature_array [3],
				temperature_array [4],
				temperature_array [5],
				temperature_array [6],
				temperature_array [7],
				temperature_array [8],
				temperature_array [9],
				temperature_array [10],
				temperature_array [11] );
		printf (print_str);

		printf("\n\n");

	END_FOR;

	COMMIT trans;

	return;
}
/***********************************************************/
/*                                                         */
/* function to total array for one year (index) and        */
/* return that to the caller.                              */
/*                                                         */
/***********************************************************/
double tot_array (array_1, index)
double array_1 [3] [12];
int index;
{
	double accum;
	int i;

	accum = 0;

	for (i = 0; i < 12; i++)
		accum += array_1 [index] [i];

	return accum;
}
/***********************************************************/
/*                                                         */
/* function to total array for one year (index) and        */
/* return the average to the caller.                       */
/*                                                         */
/***********************************************************/
double avg_array (array_1, index)
double array_1 [3] [12];
int index;
{
	double avg, accum;
	int i;

	accum = 0;

	for (i = 0; i < 12; i++)
		accum += array_1 [index] [i];

	avg = accum / 12;

	return avg;
}
/***********************************************************/
/*                                                         */
/* function to total array for one month (index) and       */
/* return the average to the caller                        */
/*                                                         */
/***********************************************************/
double avg_month (array_1, index)
double array_1 [3] [12];
int index;
{
	double avg, accum;
	int i;

	accum = 0;

	for (i = 0; i < 3; i++)
		accum += array_1 [i] [index];

	avg = accum / 3;

	return avg;
}
/***********************************************************/
/*                                                         */
/* function to query the user for a city and state to      */
/* process and also to return the status of the query.     */
/*                                                         */
/***********************************************************/
int get_city_state()
{
	int loop = 0;


	while (loop == 0)
		{

		strcpy (state, "");
		strcpy (city, "");

		/* build a dynamic menu of State Codes to choose from */

	        FOR_MENU S

			strcpy (S.TITLE_TEXT, "Choose a STATE - F1 to ABORT");
			S.TITLE_LENGTH = strlen (S.TITLE_TEXT);

			FOR ST IN STATES SORTED BY ST.STATE
				PUT_ITEM SY IN S
					strcpy (SY.ENTREE_TEXT, ST.STATE);
					SY.ENTREE_LENGTH = strlen(SY.ENTREE_TEXT);
					SY.ENTREE_VALUE = 0;
				END_ITEM;
			END_FOR;
	
			DISPLAY S VERTICAL;

			/* If PF1 pressed then exit this routine and return EXIT status */

			if (S.TERMINATOR == PYXIS__KEY_PF1)
				return -1;
	
			strcpy (state, S.ENTREE_TEXT);
	                
		END_MENU;

		/* build a dynamic menu of Cities from the State code picked above */

	        FOR_MENU S
	
			strcpy (S.TITLE_TEXT, "Choose a CITY - F1 to ABORT");
			S.TITLE_LENGTH = strlen (S.TITLE_TEXT);
	
			FOR ST IN CITIES WITH ST.STATE = state SORTED BY ST.CITY
				PUT_ITEM SY IN S
					strcpy (SY.ENTREE_TEXT, ST.CITY);
					SY.ENTREE_LENGTH = strlen (SY.ENTREE_TEXT);
					SY.ENTREE_VALUE = 0;
				END_ITEM;

				/* Update counter to indicate a city was found for state code */

				loop++;

			END_FOR;
	
			DISPLAY S VERTICAL;
	
			/* If PF1 pressed then exit this routine and return EXIT status */

			if (S.TERMINATOR == PYXIS__KEY_PF1)
				return -1;
	
			strcpy (city, S.ENTREE_TEXT);

		END_MENU;

		/* Check loop variable to see if any cities were found to pick from   */
		/* if none found ask the user if they want to pick another state code */
		/* and try again                                                      */

		switch (loop)
			{
			case 0:
				/* no cities found for this state */

 				CASE_MENU "No Cities for Selected State - Pick Option"
					MENU_ENTREE        "Select Another State" :
						loop = 0;
	
					MENU_ENTREE        "EXIT" :
						loop = -1;
						return loop;
				END_MENU;
				break;
			default:
				/* city found and everything is fine - continue */

				break;
			}
	
		}

	return loop;
}
/***********************************************************/
/*                                                         */
/* function to query the user for the year to modify and   */
/* return that to the caller.                              */
/*                                                         */
/***********************************************************/
int get_year(type)
int type;
{
	if (type == RAIN)
		{
		CASE_MENU "Store / Modify Rain Fall - Pick Year"
			MENU_ENTREE        "1973" :
				return 1973;
	
			MENU_ENTREE        "1974" :
				return 1974;
	
			MENU_ENTREE        "1975" :
				return 1975;
	
	       		MENU_ENTREE        "EXIT" :
				return 0;
		END_MENU;
		}
	else
		{
		CASE_MENU "Store / Modify Temperatures - Pick Year"
			MENU_ENTREE        "1973" :
				return 1973;
	
			MENU_ENTREE        "1974" :
				return 1974;
	
			MENU_ENTREE        "1975" :
				return 1975;
	
	       		MENU_ENTREE        "EXIT" :
				return 0;
		END_MENU;
		}
}
