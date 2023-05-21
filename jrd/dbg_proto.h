/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dbg_proto.h
 *	DESCRIPTION:	Prototype header file for dbg.c
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

#ifndef _JRD_DBG_PROTO_H_
#define _JRD_DBG_PROTO_H_

/* Don't declare DBG_supervisor in _ANSI_PROTOTYPES_, it screws up val.c */

extern int	DBG_supervisor();

extern int	DBG_all (void);
extern int	DBG_analyze (int);
extern int	DBG_bdbs (void);
extern int	DBG_precedence (void);
extern int	DBG_block (register struct blk *);
extern int	DBG_check (int);
extern int	DBG_close (void);
extern int	DBG_eval (int);
extern int	DBG_examine (int *);
extern int	DBG_init (void);
extern int	DBG_open (void);
extern int	DBG_pool (int);
extern int	DBG_pretty (register struct nod *, register int);
extern int	DBG_rpb (struct rpb *);
extern int	DBG_smb (struct smb *, int);
extern int	DBG_verify (void);
extern int	DBG_window (int *);
extern int	DBG_memory (void);

#endif /* _JRD_DBG_PROTO_H_ */
