/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		exe_proto.h
 *	DESCRIPTION:	Prototype header file for exe.c
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

#ifndef _QLI_EXE_PROTO_H_
#define _QLI_EXE_PROTO_H_

extern void		EXEC_abort (void);
extern void		EXEC_execute (struct nod *);
extern void		*EXEC_open_blob (struct nod *);
extern struct file	*EXEC_open_output (struct nod *);
extern void		EXEC_poll_abort (void);
extern struct dsc	*EXEC_receive (struct msg *, struct par *);
extern void		EXEC_send (struct msg *);
extern void		EXEC_start_request (struct req *, struct msg *);
extern void		EXEC_top (struct nod *);

#endif /* _QLI_EXE_PROTO_H_ */
