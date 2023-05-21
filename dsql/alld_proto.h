/*
 *	PROGRAM:	Dynamic  SQL RUNTIME SUPPORT
 *	MODULE:		alld_proto.h
 *	DESCRIPTION:	Prototype Header file for alld.c
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

#ifndef _DSQL_ALLD_PROTO_H_
#define _DSQL_ALLD_PROTO_H_

extern BLK	ALLD_alloc (struct plb *, UCHAR, ULONG);
extern BLK	ALLD_extend (struct blk * *, ULONG);
extern void	ALLD_fini (void);
extern void	ALLD_free (SCHAR *);
extern USHORT	ALLD_init (void);
extern UCHAR	*ALLD_malloc (ULONG);
extern PLB	ALLD_pool (void);
extern BLK	ALLD_pop (register struct lls * *);
extern void	ALLD_push (struct blk *, register struct lls * *);
extern void	ALLD_release (register struct frb *);
extern void	ALLD_rlpool (struct plb *);

#endif /* _DSQL_ALLD_PROTO_H_ */
