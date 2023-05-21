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
main ()
{
GDS__QUAD  blob_id;
char		filename [30];
int		c;

EXEC SQL
    SELECT COUNT (*) FROM SOURCES INTO :c;

printf ("Please enter the name of a source file to store: ");
gets (filename);

BLOB_load (&blob_id, gds__database, gds__trans, filename);
EXEC SQL
    INSERT INTO SOURCES (FILENAME, SOURCE) VALUES
       (:filename, :blob_id);

if (SQLCODE && (SQLCODE != 100))
    {
    printf ("Encountered SQLCODE %d\n", SQLCODE);
    printf ("    expanded error message -\n");
    gds__print_status (gds__status);
    EXEC SQL
    	ROLLBACK RELEASE;
    exit (1);
    }
else
    {
    EXEC SQL COMMIT RELEASE;
    exit (0);
    }
}
