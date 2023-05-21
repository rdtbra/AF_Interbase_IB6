/*
 *	PROGRAM:	Alice (All Else) Utility
 *	MODULE:		alice_proto.h
 *	DESCRIPTION:	Prototype header file for alice.c
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

#ifndef _ALICE_ALICE_PROTO_H_
#define _ALICE_ALICE_PROTO_H_

typedef int (DLL_EXPORT* OUTPUTPROC) (SLONG, UCHAR *);

extern void	ALICE_down_case (TEXT *, TEXT *);
extern int	ALICE_gfix (int, char **, OUTPUTPROC, SLONG);
extern void	ALICE_print (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	ALICE_error (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	ALICE_print_status (STATUS *);

#endif /* _ALICE_ALICE_PROTO_H_ */
