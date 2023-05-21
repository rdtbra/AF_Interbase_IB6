/*
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

/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         seg_proto.h
 *      DESCRIPTION:    Prototype header file for seg.c
 *
 * copyright (c) 1995 by Borland International
 */

#ifndef _JRD_SEG_PROTO_H_
#define _JRD_SEG_PROTO_H_

#ifdef  WINDOWS_ONLY

#define memset(to,value,length) SEG_set( to, value, (ULONG)(length))
#define memcpy(to,from,length)  SEG_move( to, from, (ULONG)(length))
#define memcmp(op1,op2,length)  SEG_compare( op1, op2, (ULONG)(length))

extern  void    *SEG_move (UCHAR *, UCHAR *, ULONG);
extern  void    *SEG_set (UCHAR *, UCHAR, ULONG);
extern  int	 SEG_compare (UCHAR *, UCHAR *, ULONG);

#endif
#endif /* _JRD_SEG_PROTO_H_ */
