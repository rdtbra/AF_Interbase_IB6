/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		nounix.c
 *	DESCRIPTION:	Dummy routines
 *
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

#ifdef __STDC__
int pipe (void)
#else
int pipe ()
#endif
{
return -1;
}

#ifdef __STDC__
int vfork (void)
#else
int vfork ()
#endif
{
return -1;
}
