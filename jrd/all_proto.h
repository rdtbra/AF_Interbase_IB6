/*
 *	PROGRAM:	JRD Access Method
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

#ifndef _JRD_ALL_PROTO_H_
#define _JRD_ALL_PROTO_H_

#include "../jrd/jrd.h"
#include "../jrd/all.h"

extern struct blk	*ALL_alloc 	(struct plb *, UCHAR, ULONG, enum err_t);
#ifdef DEV_BUILD
extern void		ALL_check_memory (void);
#endif
#ifdef	WINDOWS_ONLY
extern BOOLEAN		ALL_check_size 	(UCHAR, ULONG);
#endif
extern TEXT		*ALL_cstring 	(TEXT *);
extern struct blk	*ALL_extend 	(struct blk **, ULONG);
extern void		ALL_fini 	(void);
extern void		ALL_free 	(SCHAR *);
extern ULONG		ALL_get_free_object (struct plb *, struct vec **, USHORT);
extern void		ALL_init 	(void);
extern SCHAR		*ALL_malloc 	(ULONG, enum err_t);
extern struct plb	*ALL_pool 	(void);
extern void		ALL_push 	(struct blk *, register struct lls **);
extern struct blk	*ALL_pop 	(register struct lls **);
extern void		ALL_release 	(register struct frb *);
extern void		ALL_rlpool 	(struct plb *);
extern SCHAR		*ALL_sys_alloc	(ULONG, enum err_t);
extern void		ALL_sys_free 	(SCHAR *);
extern ULONG		ALL_tail 	(UCHAR);
extern struct vec	*ALL_vector 	(struct plb *, struct vec **, USHORT);

#define ALL_RELEASE(block)	ALL_release ((struct frb *) block)

#endif /* _JRD_ALL_PROTO_H_ */
