/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_i_proto.h
 *	DESCRIPTION:	Prototype header file for isc_ipc.c
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

#ifndef _JRD_ISC_I_PROTO_H_
#define _JRD_ISC_I_PROTO_H_

extern void DLL_EXPORT	ISC_enter (void);
extern void DLL_EXPORT	ISC_enable (void);
extern void DLL_EXPORT	ISC_exit (void);
extern void DLL_EXPORT	ISC_inhibit (void);
#if !(defined mpexl || defined WIN_NT || defined OS2_ONLY)
extern int	ISC_kill (SLONG, SLONG);
#endif
#ifdef mpexl
extern int	ISC_kill (SLONG, SSHORT, SLONG);
#endif
#ifdef OS2_ONLY
extern int API_ROUTINE	ISC_kill (SLONG, ULONG);
#endif
#ifdef WIN_NT
extern int API_ROUTINE	ISC_kill (SLONG, SLONG, void *);
#endif
extern void API_ROUTINE	ISC_signal (int, FPTR_VOID, void *);
extern void API_ROUTINE	ISC_signal_cancel (int, FPTR_VOID, void *);
extern void DLL_EXPORT	ISC_signal_init (void);

#ifdef mpexl
extern int	ISC_comm_init (STATUS *, int);
extern int	ISC_get_port (int);
extern int	ISC_open_port (SLONG, SLONG, int, int, SLONG *);
extern int	ISC_wait (SSHORT, SLONG);
#endif

#endif /* _JRD_ISC_I_PROTO_H_ */
