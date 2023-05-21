/*
 *	PROGRAM:	JRD Backup and Restore program  
 *	MODULE:		burp_proto.h
 *	DESCRIPTION:	Prototype header file for burp.c
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

#ifndef _BURP_BURP_PROTO_H_
#define _BURP_BURP_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int (DLL_EXPORT* OUTPUTPROC) (SLONG, UCHAR *);

extern int  BURP_gbak (int, char **, OUTPUTPROC, SLONG);
extern void	BURP_abort (void);
extern void	BURP_svc_error (USHORT, USHORT, void *, USHORT, void *, USHORT, void *, USHORT, void *, USHORT, void *);
extern void	BURP_error (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	BURP_print_status (STATUS *);
extern void	BURP_error_redirect (STATUS *, USHORT, TEXT *, TEXT *);
extern void	BURP_msg_partial (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	BURP_msg_put (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	BURP_msg_get (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	BURP_output_version (TEXT *, TEXT *);
extern void	BURP_print (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
extern void	BURP_verbose (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);

#ifdef __cplusplus
};
#endif

#endif	/*  _BURP_BURP_PROTO_H_  */

