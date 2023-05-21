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

DATABASE atlas = FILENAME "atlas.gdb";

EXEC SQL 
    INCLUDE SQLCA;

EXEC SQL 
    WHENEVER SQLERROR GO TO ERR;

char *string = "SELECT STATE_NAME, STATEHOOD FROM STATES ORDER BY STATEHOOD";
struct tm d;
char state_name[26];
short flag1, flag2;
SQLDA *sqlda;
GDS__QUAD state_date;

main ()
{                              
    READY;
    START_TRANSACTION;

    sqlda = (SQLDA*) malloc (SQLDA_LENGTH (2));
    sqlda->sqln = 2;

    EXEC SQL 
        PREPARE Q INTO sqlda FROM :string;

    sqlda->sqlvar[0].sqldata = state_name;
    state_name[25] = 0;
    sqlda->sqlvar[0].sqlind = &flag1;
    sqlda->sqlvar[0].sqltype = SQL_TEXT + 1;

    sqlda->sqlvar[1].sqldata = (char*) &state_date;
    sqlda->sqlvar[1].sqlind = &flag2;
    sqlda->sqlvar[1].sqltype = SQL_DATE + 1;

    EXEC SQL 
        DECLARE C CURSOR FOR Q;

    EXEC SQL 
        OPEN C;

    EXEC SQL
        FETCH C USING DESCRIPTOR sqlda;

    while (SQLCODE == 0)
	{          
        printf ("%s ", state_name);

        if (flag2 >=0) {
            gds__decode_date (&state_date, &d);
            printf ("%2d/%02d/%4d\n",
				        d.tm_mon + 1, d.tm_mday, d.tm_year + 1900);
        }
        else
            printf("<date missing>\n");

        EXEC SQL 
            FETCH C USING DESCRIPTOR sqlda;
    }

    EXEC SQL 
        ROLLBACK RELEASE;
    exit (0);

ERR: printf ("Data base error, SQLCODE = %d\n", SQLCODE);
     gds__print_status (gds__status);

     EXEC SQL 
        ROLLBACK RELEASE;
     exit (1);
}
