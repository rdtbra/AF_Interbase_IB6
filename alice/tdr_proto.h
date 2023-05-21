/*
 *	PROGRAM:	Alice (All Else) Utility
 *	MODULE:		tdr_proto.h
 *	DESCRIPTION:	Prototype header file for tdr.c
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

#ifndef _ALICE_TDR_PROTO_H_
#define _ALICE_TDR_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void	TDR_list_limbo (int *, TEXT *, ULONG);
extern BOOLEAN	TDR_reconnect_multiple (int *, SLONG, TEXT *, ULONG);
extern void	TDR_shutdown_databases (TDR);
extern USHORT	TDR_analyze (TDR);
extern BOOLEAN	TDR_attach_database (STATUS *, TDR, TEXT *);
extern void	TDR_get_states (TDR);

#ifdef __cplusplus
};
#endif

#endif /* _ALICE_TDR_PROTO_H_ */
