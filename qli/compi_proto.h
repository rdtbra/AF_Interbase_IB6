/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		compi_proto.h
 *	DESCRIPTION:	Prototype header file for compile.c
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

#ifndef _QLI_COMPI_PROTO_H_
#define _QLI_COMPI_PROTO_H_

extern struct nod	*CMPQ_compile (struct nod *);
extern void		CMP_alloc_temp (struct nod *);
extern int		CMP_node_match (struct nod *, struct nod *);

#endif /* _QLI_COMPI_PROTO_H_ */
