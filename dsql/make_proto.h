/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		make_proto.h
 *	DESCRIPTION:	Prototype Header file for make.c
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

#ifndef _DSQL_MAKE_PROTO_H_
#define _DSQL_MAKE_PROTO_H_

#include "../dsql/sym.h"

extern struct nod	*MAKE_constant (struct str *, int);
extern struct str	*MAKE_cstring (CONST SCHAR *);
extern void		MAKE_desc (struct dsc *, struct nod *);
extern void		MAKE_desc_from_field (struct dsc *, struct fld *);
extern struct nod	*MAKE_field (struct ctx *, struct fld *, struct nod *);
extern struct nod	*MAKE_list (struct lls *);
extern struct nod	*MAKE_node (ENUM nod_t, int);
extern struct str	*MAKE_numeric (CONST UCHAR *, int);
extern struct par	*MAKE_parameter (struct msg *, USHORT, USHORT);
extern struct str	*MAKE_string (CONST UCHAR *, int);
extern struct sym	*MAKE_symbol (struct dbb *, CONST TEXT *, USHORT, ENUM sym_type, struct req *);
extern struct str	*MAKE_tagged_string (CONST UCHAR *, int, CONST TEXT *);
extern struct nod	*MAKE_variable (struct fld *, CONST TEXT *, USHORT, USHORT, USHORT, USHORT);

#endif	/* _DSQL_MAKE_PROTO_H_ */
