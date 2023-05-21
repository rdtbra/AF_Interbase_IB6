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
EXEC SQL BEGIN DECLARE SECTION;
EXEC SQL END DECLARE SECTION;

char		*months[]  = { 
	    "January", "February", "March", "April", 
	    "May", "June", "July", "August", "September", 
	    "October", "November", "December"
	    };

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

#ifdef mpexl
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifndef TIME_DEFINED
#include <time.h>
#endif

main ()
{
GDS__QUAD	date;
char		state [26];
struct tm	time;
short		year;

EXEC SQL DECLARE C CURSOR FOR
	SELECT S.STATE_NAME, S.STATEHOOD FROM STATES S
		WHERE S.STATEHOOD IS NOT NULL
		ORDER BY S.STATEHOOD;


EXEC SQL OPEN C;

if (!SQLCODE)
    EXEC SQL FETCH C INTO :state, :date;
    
while (!SQLCODE)
    {
    gds__decode_date (&date, &time);
    year = 1900 + time.tm_year;
    printf ("%s entered the union on %s %d, %4d\n",
		state, months[time.tm_mon],
		time.tm_mday, year);  
    EXEC SQL FETCH C INTO :state, :date; 
    }
if (SQLCODE != 100)
    gds__print_status (gds__status);
EXEC SQL
	  COMMIT RELEASE;
exit (0);
}
