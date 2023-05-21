/*
 *	PROGRAM:	Preprocessor
 *	MODULE:		msc_proto.h
 *	DESCRIPTION:	Prototype header file for msc.c
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

#ifndef _GPRE_MSC_PROTO_H_
#define _GPRE_MSC_PROTO_H_

#include "../gpre/parse.h"

extern ACT	MSC_action (REQ, enum act_t);
extern UCHAR	*MSC_alloc (int);
extern UCHAR	*MSC_alloc_permanent (int);
extern NOD	MSC_binary (NOD_T, NOD, NOD);
extern CTX	MSC_context (REQ);
extern void	MSC_copy (SCHAR *, int, SCHAR *);
extern SYM	MSC_find_symbol (SYM, enum sym_t);
extern void	MSC_free (SCHAR *);
extern void	MSC_free_request (REQ);
extern void	MSC_init (void);
extern BOOLEAN	MSC_match (KWWORDS);
extern BOOLEAN	MSC_member (NOD, LLS);
extern NOD	MSC_node (enum nod_t, SSHORT);
extern NOD	MSC_pop (LLS *);
extern PRV	MSC_privilege_block (void);
extern void	MSC_push (NOD, LLS *);
extern REF	MSC_reference (REF *);
extern REQ	MSC_request (enum req_t);
extern SCHAR	*MSC_string (TEXT *);
extern SYM	MSC_symbol (enum sym_t, TEXT *, USHORT, CTX);
extern NOD	MSC_unary (NOD_T, NOD);
extern USN	MSC_username (SCHAR *, USHORT);

#endif /* _GPRE_MSC_PROTO_H_ */
