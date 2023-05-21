/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		hsh_proto.h
 *	DESCRIPTION:	Prototype Header file for hsh.c
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

#ifndef _DSQL_HSH_PROTO_H_
#define _DSQL_HSH_PROTO_H_

extern void	HSHD_fini (void);
extern void	HSHD_finish (void *);
extern void	HSHD_init (void);
extern void     HSHD_insert (struct sym *);
extern SYM      HSHD_lookup (void *, TEXT *, SSHORT, SYM_TYPE, USHORT);
extern void     HSHD_remove (struct sym *);
extern void     HSHD_set_flag (void *, TEXT *, SSHORT, SYM_TYPE, SSHORT);

#endif /*_DSQL_HSH_PROTO_H_*/
