/*
 *	PROGRAM:	Dynamic  SQL RUNTIME SUPPORT
 *	MODULE:		errd_proto.h
 *	DESCRIPTION:	Prototype Header file for errd_proto.h
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

#ifndef _DSQL_ERRD_PROTO_H_
#define _DSQL_ERRD_PROTO_H_

#ifdef DEV_BUILD
extern void	ERRD_assert_msg (CONST UCHAR *, CONST UCHAR *, ULONG);
#endif

extern void	ERRD_bugcheck (CONST TEXT *);
extern void	ERRD_error (int, CONST TEXT *);
extern void	ERRD_post (STATUS, ...);
extern BOOLEAN	ERRD_post_warning (STATUS, ...);
extern void	ERRD_punt (void);

#endif /* _DSQL_ERRD_PROTO_H_ */
