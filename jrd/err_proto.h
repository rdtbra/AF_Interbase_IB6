/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		err_proto.h
 *	DESCRIPTION:	Prototype header file for err.c
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

#ifndef _JRD_ERR_PROTO_H_
#define _JRD_ERR_PROTO_H_

#ifndef REQUESTER
#include "../jrd/jrd.h"
#include "../jrd/btr.h"

extern BOOLEAN	DLL_EXPORT ERR_post_warning (STATUS, ...);
extern void	ERR_assert (TEXT *, int);
extern void DLL_EXPORT	ERR_bugcheck (int);
extern void DLL_EXPORT	ERR_bugcheck_msg (TEXT *);
extern void DLL_EXPORT	ERR_corrupt (int);
extern void DLL_EXPORT	ERR_duplicate_error (enum idx_e, 
						    struct rel *, 
						    USHORT);
extern void DLL_EXPORT	ERR_error (int);
extern void DLL_EXPORT	ERR_error_msg (TEXT *);
extern void DLL_EXPORT	ERR_post (STATUS, ...);
extern void DLL_EXPORT	ERR_punt (void);
extern void DLL_EXPORT	ERR_warning (STATUS, ...);
extern void DLL_EXPORT	ERR_log (int, int, TEXT *);
#endif  /* REQUESTER */

extern TEXT * DLL_EXPORT ERR_cstring (TEXT *);
extern TEXT * DLL_EXPORT ERR_string (TEXT *, int);

#ifdef  WINDOWS_ONLY
extern void	ERR_wep (void);
#endif

#endif /* _JRD_ERR_PROTO_H_ */
