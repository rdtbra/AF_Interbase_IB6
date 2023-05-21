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

DATABASE atlas = FILENAME "atlas.gdb";
EXEC SQL 
    INCLUDE SQLCA;

EXEC SQL 
    WHENEVER SQLERROR GO TO ERR;

char *string = "SELECT STATE_NAME, STATEHOOD FROM STATES ORDER BY STATEHOOD";
SQLDA *sqlda;
char state_date[12], state_name[26];
short flag1, flag2;

main ()
{                              
    READY;
    START_TRANSACTION;

    sqlda = (SQLDA*) malloc (SQLDA_LENGTH (2));
    sqlda->sqln = 2;

    EXEC SQL 
        PREPARE Q INTO sqlda FROM :string;

    sqlda->sqlvar[0].sqldata = state_name;
    state_name[sizeof(state_name) - 1] = 0;
    sqlda->sqlvar[1].sqllen = sizeof(state_name) - 1;
    sqlda->sqlvar[0].sqlind = &flag1;
    sqlda->sqlvar[0].sqltype = SQL_TEXT + 1;

/* Manually change the date's default size of eight chars */

    sqlda->sqlvar[1].sqldata = state_date;
    state_date[sizeof(state_date) - 1] = 0;
    sqlda->sqlvar[1].sqllen = sizeof(state_date) - 1;
    sqlda->sqlvar[1].sqlind = &flag2;
    sqlda->sqlvar[1].sqltype = SQL_TEXT + 1;

    EXEC SQL 
        DECLARE C CURSOR FOR Q;

    EXEC SQL 
        OPEN C;
    EXEC SQL
        FETCH C USING DESCRIPTOR sqlda;

    while (SQLCODE == 0)
	{          
        printf ("%s ", state_name);

	state_date[11] = '\0';

        if (flag2 >= 0)                    
            printf ("%12s\n", state_date);
        else
            printf ("<date missing>\n");

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
