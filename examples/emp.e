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
DATABASE DB = FILENAME 'emp.gdb';
main ()
{
BASED ON EMPLOYEES.EMPNO	empno;
char				buffer [64];

READY;
START_TRANSACTION;
printf ("\t\tEmployee Roster\n\n");
	FOR E IN EMPLOYEES WITH E.SUPER MISSING
    		trunc (E.FIRST_NAME, E.LAST_NAME, buffer);
    		printf (" %s\n", buffer);
    		print_next (0, E.EMPNO);
	END_FOR;
COMMIT;
FINISH;
exit (0);
}
    
static  print_next (level, super)
short				level;
BASED ON EMPLOYEES.EMPNO	super;
{
char	buffer [64], template[40];

sprintf (template, "%%%ds....%%s%%s", (level * 4 + 1));

	FOR (LEVEL level) E IN EMPLOYEES WITH E.SUPER = super
    		trunc (E.FIRST_NAME, E.LAST_NAME, buffer);
    		printf (template, " ", buffer, "\n");
    		print_next (level + 1, E.EMPNO); 
	END_FOR;
}

static trunc (str1, str2, out)
char	*str1, *str2, *out;

{
char	*p, *q;

for (p = str1, q = out; *p && *p != ' '; p++)
    *q++ = *p;
*q++ = ' ';
for (p = str2; *p && *p != ' '; p++)
    *q++ = *p;
*q = 0;
}
