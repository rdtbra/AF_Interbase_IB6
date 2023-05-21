/*
 *	PROGRAM:	Alice (All Else) Utility
 *	MODULE:		all_proto.h
 *	DESCRIPTION:	Prototype header file for all.c
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

#ifndef _ALICE_ALL_PROTO_H_
#define _ALICE_ALL_PROTO_H_

#include "../alice/lls.h"

#ifdef __cplusplus
extern "C" {
#endif

extern BLK	ALLA_alloc (PLB, UCHAR, int);
extern BLK	ALLA_extend (BLK *, int);
extern void	ALLA_fini (void);
extern void	ALLA_free (SCHAR *);
extern void	ALLA_init (void);
extern SCHAR	*ALLA_malloc (SLONG);
extern PLB	ALLA_pool (void);
extern void	ALLA_push (BLK, register LLS *);
extern BLK	ALLA_pop (register LLS *);
extern void	ALLA_release (register FRB);
extern void	ALLA_rlpool (PLB);
extern VEC	ALLA_vector (PLB, VEC *, USHORT);

#ifdef __cplusplus
};
#endif

#endif /* _ALICE_ALL_PROTO_H_ */
