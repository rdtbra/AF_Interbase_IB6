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
DATABASE DB = FILENAME "slides.gdb";

main ()

{
READY;
START_TRANSACTION;
FOR T IN TEXT
    printf ("seminar:\t%s\t\ttalk:\t%s\n", T.SEMINAR, T.TALK);
    printf ("filtered test:\n");
    OPEN_BLOB B IN T.TEXT FILTER FROM -1 TO 1; 
    GET_SEGMENT B;
    while (!gds__status [1] || gds__status [1] == gds__segment)
	{
	B.SEGMENT [B.LENGTH] = 0;
	printf ("\t%s", B.SEGMENT); 
	GET_SEGMENT B;
	}
    CLOSE_BLOB B;
    printf ("unfiltered test:\n");
    OPEN_BLOB BU IN T.TEXT; 
    GET_SEGMENT BU;
    while (!gds__status [1] || gds__status [1] == gds__segment)
	{
	BU.SEGMENT [BU.LENGTH] = 0;
	printf ("\t%s", BU.SEGMENT); 
	GET_SEGMENT BU;
	}
    CLOSE_BLOB BU;
    printf ("\n");
END_FOR;
COMMIT;
FINISH;
exit (0);
}
    
	
