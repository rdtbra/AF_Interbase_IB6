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
EXEC SQL BEGIN DECLARE SECTION;
EXEC SQL END DECLARE SECTION;
 
main ()
{
BSTREAM 				*g_in;
GDS__QUAD				g_id;
int				c;

EXEC SQL DECLARE C CURSOR FOR 
	SELECT GUIDEBOOK FROM TOURISM;
EXEC SQL OPEN C;

EXEC SQL FETCH C INTO :g_id;

while (!SQLCODE)
    {
    g_in = Bopen (&g_id, gds__database, gds__trans, "r");
    while ((c = getb (g_in)) != EOF)
	putchar (c);
    BLOB_close (g_in);
    EXEC SQL FETCH C INTO :g_id;
    }
if (SQLCODE != 100)
    gds__print_status (gds__status);
EXEC SQL
    COMMIT RELEASE;
exit (0);
}
