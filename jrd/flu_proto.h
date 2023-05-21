/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		flu_proto.h
 *	DESCRIPTION:	Prototype header file for flu.c, functions.c, builtin.c,
 *			and qatest.c
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

#ifndef _JRD_FLU_PROTO_H_
#define _JRD_FLU_PROTO_H_

extern struct mod * DLL_EXPORT FLU_lookup_module (TEXT *);
extern void DLL_EXPORT	       FLU_unregister_module (struct mod *);
extern int		       (*ISC_lookup_entrypoint (TEXT *, TEXT *, TEXT *)) (void);
extern int		       (*FUNCTIONS_entrypoint (TEXT *, TEXT *)) (void);
extern int		       (*BUILTIN_entrypoint (TEXT *, TEXT *)) (void);
extern int		       QATEST_entrypoint (ULONG *, void *, void *, void *);

#endif /* _JRD_FLU_PROTO_H_ */
