/*
 *	PROGRAM:	JRD Write Ahead LOG APIs 
 *	MODULE:		walr_proto.h
 *	DESCRIPTION:	Prototype header file for walr.c
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

#ifndef _WAL_WALR_PROTO_H_
#define _WAL_WALR_PROTO_H_

extern SSHORT	WALR_close (STATUS *, struct walrs **);
extern SSHORT	WALR_fixup_log_header (STATUS *, struct walrs *);
extern SSHORT	WALR_get (STATUS *, struct walrs *, UCHAR *, USHORT *, SLONG *, SLONG *);
extern SSHORT	WALR_get_blk_timestamp (struct walrs *, SLONG *);
extern SSHORT	WALR_open (STATUS *, struct walrs **, SCHAR *, int, SCHAR **, SLONG *, SLONG, SLONG *, SSHORT);

#endif /* _WAL_WALR_PROTO_H_ */ 
