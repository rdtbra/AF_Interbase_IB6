/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		gener_proto.h
 *	DESCRIPTION:	Prototype header file for gener.c
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

#ifndef _QLI_GENER_PROTO_H_
#define _QLI_GENER_PROTO_H_

extern struct nod	*GEN_generate (struct nod *);
extern void		GEN_release (void);
extern struct rlb	*GEN_rlb_extend (struct rlb *);
extern void		GEN_rlb_release (struct rlb *);

#endif /* _QLI_GENER_PROTO_H_ */
