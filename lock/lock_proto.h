/*
 *	PROGRAM:	JRD Lock Manager
 *	MODULE:		lock_proto.h
 *	DESCRIPTION:	Prototype header file for lock.c
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

#ifndef _LOCK_LOCK_PROTO_H_
#define _LOCK_LOCK_PROTO_H_

extern int	LOCK_convert (SLONG, UCHAR, SSHORT, int (*)(void *), void *, STATUS *);
extern int	LOCK_deq (SLONG);
extern UCHAR	LOCK_downgrade (SLONG, STATUS *);
extern SLONG	LOCK_enq (SLONG, SLONG, USHORT, UCHAR *, USHORT, UCHAR, int (*)(void *), void *, SLONG, SSHORT, STATUS *, SLONG);
extern void	LOCK_fini (STATUS *, SLONG *);
extern int	LOCK_init (STATUS *, SSHORT, SLONG, UCHAR, SLONG *);
extern void	LOCK_manager (SLONG);
extern SLONG	LOCK_query_data (SLONG, USHORT, USHORT);
extern SLONG	LOCK_read_data (SLONG);
extern SLONG	LOCK_read_data2 (SLONG, USHORT, UCHAR *, USHORT, SLONG);
extern void	LOCK_re_post (int (*)(void *), void *, SLONG);
extern BOOLEAN	LOCK_shut_manager (void);
#ifdef  WINDOWS_ONLY
extern void     LOCK_wep (void);
#endif
extern SLONG	LOCK_write_data (SLONG, SLONG);

#endif /* _LOCK_LOCK_PROTO_H_ */
