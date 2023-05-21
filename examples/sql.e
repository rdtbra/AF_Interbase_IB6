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
/* CENSUS REPORT */
/* Include the SQL Communications Area. */
EXEC SQL
	INCLUDE SQLCA
/* Set up to catch SQL errors. Note: this will require that 
each routine containing executeable SQL commands has
an error handling section labeled 'err'.*/
EXEC SQL
	WHENEVER SQLERROR GO TO err;
/* Forward declarations */
static void print_error(), census_report();
main()
{
	/* Set up estimated data; if successful generate report. */
	if (setup()) census_report();
	/* Undo estimated census data. */
	EXEC SQL
		ROLLBACK;
	exit (0);
	/* Output error information. */
err:		print_error();
	exit (1);
}
static void print_error()

/* Print out error message */
{
printf ("Data base error, SQLCODE = %d\n", SQLCODE);
}
int setup()
/* Put in estimated data for missing 1960 & 1970 census data. */
/* Return 1 if successful, 0 otherwise.  */
{
	/* Replace missing 1960 census data with estimates. */
	EXEC SQL
		UPDATE POPULATIONS
		SET CENSUS_1960 = 
			(0.3 * (CENSUS_1980 - CENSUS_1950)) + CENSUS_1950
			WHERE CENSUS_1960 IS NULL;
	/* Replace missing 1970 census data with estimates. */
	EXEC SQL
		UPDATE POPULATIONS
		SET CENSUS_1970 = 
			(0.65 * (CENSUS_1980 - CENSUS_1950)) + CENSUS_1950
			WHERE CENSUS_1970 IS NULL;
	return (1);
	/* Error return */
err:	print_error();
	return (0);
}
static void census_report()
{
	/* Declare variables to receive data retrieved by SQL
	commands. */
	int pop_50, pop_60, pop_70, pop_80;
	char stname[26];
	/* Declare cursor for use in retrieving state/census data. */
  EXEC SQL
		DECLARE C CURSOR FOR
		SELECT STATE_NAME, CENSUS_1950, CENSUS_1960,
			CENSUS_1970, CENSUS_1980
		FROM STATES S, POPULATIONS P
		WHERE S.STATE = P.STATE
		ORDER S.STATE_NAME;
	/* Open cursor for use in retrieving state/census data. */
	EXEC SQL
		OPEN C;
	/* Print report header */
  printf ("     STATE                    ");
	printf ("                  POPULATION\n");
	printf ("                                1950        1960");
  printf ("        1970        1980\n\n");
	/* Retrieve first set of state/census data. */
	EXEC SQL
		FETCH C INTO :stname, :pop_50, :pop_60, :pop_70, :pop_80;
	/* Until end of data stream is reached, print out current set
	   of data and then fetch next set. */
	while (SQLCODE == 0)
		{
                printf ("%-25s   %9d   %9d   %9d   %9d\n",
			stname, pop_50,
			pop_60, pop_70, pop_80);
		/* Retrieve next set of state/census data. */
		EXEC SQL
			FETCH C INTO :stname, :pop_50, :pop_60,
				 :pop_70, :pop_80;
		}
	/* Close cursor. */
	EXEC SQL
		CLOSE C;
	return;
	/* Error return */
err:		print_error();
	return;
}
