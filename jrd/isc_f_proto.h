/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_f_proto.h
 *	DESCRIPTION:	Prototype header file for isc_file.c
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

#ifndef _JRD_ISC_FILE_PROTO_H_
#define _JRD_ISC_FILE_PROTO_H_

extern int 		ISC_analyze_nfs (TEXT *, TEXT *);
extern int		ISC_analyze_pclan (TEXT *, TEXT *);
extern int DLL_EXPORT	ISC_analyze_spx (TEXT *, TEXT *);
extern int DLL_EXPORT	ISC_analyze_tcp (TEXT *, TEXT *);
extern BOOLEAN DLL_EXPORT   ISC_check_if_remote (TEXT *, BOOLEAN);
extern int		ISC_expand_filename (TEXT *, USHORT, TEXT *);
extern int		ISC_expand_logical (TEXT *, USHORT, TEXT *);
extern int		ISC_expand_share (TEXT *, TEXT *);
extern int		ISC_file_lock (SSHORT);
extern int		ISC_file_unlock (SSHORT);
extern void		ISC_parse_filename (TEXT *, 
						TEXT *, 
						TEXT *, 
						TEXT *);
extern int		ISC_strip_filename (TEXT *);

#endif /* _JRD_ISC_FILE_PROTO_H_ */

