/*
 *	PROGRAM:	Preprocessor
 *	MODULE:		prett_proto.h
 *	DESCRIPTION:	Prototype header file for pretty.c
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

#ifndef _GPRE_PRETT_PROTO_H_
#define _GPRE_PRETT_PROTO_H_

extern int	PRETTY_print_cdb (SCHAR *, int (*)(), SCHAR *, SSHORT);
extern int	PRETTY_print_dyn (SCHAR *, int (*)(), SCHAR *, SSHORT);
extern int	PRETTY_print_form_map (SCHAR *, int (*)(), SCHAR *, SSHORT);
extern int	PRETTY_print_mblr (SCHAR *, int (*)(), SCHAR *, SSHORT);
extern int	PRETTY_print_menu (SCHAR *, int (*)(), SCHAR *, SSHORT);
extern int	PRETTY_print_sdl (SCHAR *, int (*)(), SCHAR *, SSHORT);

#endif /* _GPRE_PRETT_PROTO_H_ */
