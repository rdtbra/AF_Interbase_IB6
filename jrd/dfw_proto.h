/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dfw_proto.h
 *	DESCRIPTION:	Prototype header file for dfw.c
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

#ifndef _JRD_DFW_PROTO_H_
#define _JRD_DFW_PROTO_H_

extern USHORT	DFW_assign_index_type (struct dfw *, SSHORT, SSHORT);
extern void	DFW_delete_deferred (struct tra *, SLONG);
extern void	DFW_merge_work (struct tra *, SLONG, SLONG);
extern void	DFW_perform_system_work (void);
extern void	DFW_perform_work (struct tra *);
extern void	DFW_perform_post_commit_work (struct tra *);
extern void	DFW_post_work (struct tra *, ENUM dfw_t, struct dsc *, USHORT);
extern void	DFW_update_index (struct dfw *, USHORT, float);

#endif /* _JRD_DFW_PROTO_H_ */
