/*
 *	PROGRAM:	Data Definition Utility
 *	MODULE:		lex_proto.h
 *	DESCRIPTION:	Prototype header file for lex.c
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

#ifndef _DUDLEY_LEX_PROTO_H_
#define _DUDLEY_LEX_PROTO_H_

extern struct tok	*LEX_filename (void);
extern void		LEX_fini (void);
extern void		LEX_flush (void);
extern void		LEX_get_text (SCHAR *, TXT);
extern void		LEX_init (void *);
extern void		LEX_put_text (void *, TXT);
extern void		LEX_real (void);
extern struct tok	*LEX_token (void);

#endif /* _DUDLEY_LEX_PROTO_H_ */
