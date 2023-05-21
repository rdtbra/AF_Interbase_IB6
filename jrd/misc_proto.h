/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		misc_proto.h
 *	DESCRIPTION:	Prototype header file for misc.c
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

#ifndef _JRD_MISC_PROTO_H_
#define _JRD_MISC_PROTO_H_

extern SSHORT		MISC_build_parameters_block (UCHAR *, ...);
extern UCHAR		*MISC_pop (struct stk **);
extern struct stk	*MISC_push (UCHAR *, struct stk **);

#ifdef DEV_BUILD
extern ULONG	MISC_checksum_log_rec (UCHAR *, SSHORT, UCHAR *, SSHORT);
#endif

#endif /* _JRD_MISC_PROTO_H_ */
