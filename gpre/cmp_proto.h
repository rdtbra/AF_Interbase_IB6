/*
 *	PROGRAM:	Preprocessor
 *	MODULE:		cmp_proto.h
 *	DESCRIPTION:	Prototype header file for cmp.c
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

#ifndef _GPRE_CMP_PROTO_H_
#define _GPRE_CMP_PROTO_H_

#include "../gpre/form.h"

extern void	CMP_check (register REQ, SSHORT);
extern void	CMP_compile_request (register REQ);
extern int	CMP_display_code (FINT, REF);
extern void	CMP_external_field (REQ, FLD);
extern void	CMP_init (void);
extern USHORT	CMP_next_ident (void);
extern void	CMP_stuff_symbol (REQ, SYM);
extern void	CMP_t_start (register TRA);

#endif /* _GPRE_CMP_PROTO_H_ */
