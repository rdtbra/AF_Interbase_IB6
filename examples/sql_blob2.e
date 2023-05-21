/*
 *	 This short program selects a single record
 *	 and prints the contents of a blob field in
 * the record.
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

#ifdef apollo
#define NULL_STATUS *gds__null
#else
#define NULL_STATUS (long*) 0
#endif

main ()
{
GDS__QUAD	blob_id;
long		blob_handle;
char		segment [60], string [61];
unsigned short	length;

EXEC SQL SELECT T.GUIDEBOOK INTO :blob_id FROM TOURISM T WITH
	T.STATE = "ME";
if (SQLCODE)
    {
    gds__print_status (gds__status);
    exit (1);
    }
    
/* the blob handle must be initialized to zero before the blob 
open call */

blob_handle = 0;  		

gds__open_blob (NULL_STATUS, GDS_REF(gds__database), GDS_REF(gds__trans), 
                GDS_REF(blob_handle), GDS_REF(blob_id));
for (;;)
    {
    gds__status[1] = gds__get_segment (NULL_STATUS,
			    GDS_REF(blob_handle), GDS_REF(length), sizeof (segment), GDS_VAL(segment));
    if (gds__status[1] && (gds__status[1] != gds__segment)) 
				 break;
    strncpy (string, segment, length);
    string[length] = 0;
    printf ("%s\n", string);
    }
if (gds__status[1] != gds__segstr_eof)
    gds__print_status (gds__status);

EXEC SQL
	  COMMIT RELEASE;
exit (0);
}
