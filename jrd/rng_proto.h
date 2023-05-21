/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		rng_proto.h
 *	DESCRIPTION:	Prototype header file for rng.c
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

#ifndef _JRD_RNG_PROTO_H_
#define _JRD_RNG_PROTO_H_

#ifdef PC_ENGINE
extern void		RNG_add_page (ULONG);
extern void		RNG_add_record (struct rpb *);
extern struct nod	*RNG_add_relation (struct nod *);
extern void		RNG_add_uncommitted_record (struct rpb *);
extern struct dsc	*RNG_begin (struct nod *, struct vlu *);
extern struct nod	*RNG_delete (struct nod *);
extern void		RNG_delete_ranges (struct req *);
extern struct nod	*RNG_end (struct nod *);
extern void		RNG_release_locks (struct rng *);
extern void		RNG_release_ranges (struct req *);
extern void		RNG_shutdown_attachment (struct att *);
#endif

#endif /* _JRD_RNG_PROTO_H_ */
