/*
 *	PROGRAM:	Data Definition Utility
 *	MODULE:		expr_proto.h
 *	DESCRIPTION:	Prototype header file for expr.c
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

#ifndef _DUDLEY_EXPR_PROTO_H_
#define _DUDLEY_EXPR_PROTO_H_

extern NOD	EXPR_boolean (USHORT *);
extern NOD	EXPR_rse (USHORT);
extern NOD	EXPR_statement (void);
extern NOD	EXPR_value (USHORT *, USHORT *);

#endif /* _DUDLEY_EXPR_PROTO_H_ */
