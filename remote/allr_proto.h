/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		allr_proto.h
 *	DESCRIPTION:	Prototype header file for allr.c
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

#ifndef _REMOTE_ALLR_PROTO_H_
#define _REMOTE_ALLR_PROTO_H_

extern UCHAR		* DLL_EXPORT ALLR_alloc (ULONG);
extern struct blk	* DLL_EXPORT ALLR_block (UCHAR, ULONG);
extern struct blk	*ALLR_clone (struct blk *);
extern void		ALLR_free (void *);
extern void		DLL_EXPORT ALLR_release (void *);
extern struct vec	*ALLR_vector (struct vec **, ULONG);

#endif /* _REMOTE_ALLR_PROTO_H_ */
