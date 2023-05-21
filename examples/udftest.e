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
DATABASE ATLAS = FILENAME "atlas.gdb"

main()
{

READY ATLAS;
START_TRANSACTION;
printf("This program prints the states that have changed in population\n");
printf("by more than 250,000 people between 1970 and 1980.\n\n");

printf("State         census_1970          census_1980\n\n");
FOR P IN POPULATIONS WITH 
       ABS(P.CENSUS_1970 - P.CENSUS_1980) > 250000
   printf("%4s         %10d           %10d\n", p.state,
           p.census_1970, p.census_1980);
END_FOR;
ROLLBACK;
FINISH;
}
