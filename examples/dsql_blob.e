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

DATABASE DB = 'atlas.gdb';

EXEC SQL 
    INCLUDE SQLCA;


EXEC SQL
    WHENEVER SQLERROR GO TO ERR;
    
SQLDA *sqlda;
GDS__QUAD blob;
short flag;
char *query = "SELECT GUIDEBOOK FROM TOURISM                  \
	 WHERE STATE = 'NY'";

main ()
{
    READY;
    START_TRANSACTION;
    
    sqlda = (SQLDA*) malloc (SQLDA_LENGTH (1));
    sqlda->sqln = 1;
    
    EXEC SQL
        PREPARE Q INTO sqlda FROM :query;
    
    sqlda->sqlvar[0].sqldata = (char*) &blob;
    sqlda->sqlvar[0].sqlind  = &flag;
    
    EXEC SQL
        DECLARE C CURSOR FOR Q;
    
    EXEC SQL
        OPEN C;
    
    EXEC SQL
        FETCH C USING DESCRIPTOR sqlda;
    
    if (SQLCODE)
        gds__print_status (gds__status);
    else
        BLOB_display (&blob, DB, gds__trans, "");
    
    EXEC SQL
        COMMIT RELEASE;
    exit (0);
    ERR: printf ("Data base error, SQLCODE = %d\n", SQLCODE);
     gds__print_status (gds__status);

     EXEC SQL
        ROLLBACK RELEASE;

     exit (1);
}
