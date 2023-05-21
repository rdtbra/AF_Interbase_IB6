/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ini_proto.h
 *	DESCRIPTION:	Prototype header file for ini.c
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

#ifndef _JRD_INI_PROTO_H_
#define _JRD_INI_PROTO_H_

extern void		INI_format (TEXT *);
extern USHORT 		INI_get_trig_flags (TEXT * );
extern void		INI_init (void);
extern void		INI_init2 (void);
extern struct trg	*INI_lookup_sys_trigger (struct rel *, struct trg *, UCHAR **, UCHAR *, SCHAR **, USHORT *);
extern void		INI_update_database (void);

#endif /* _JRD_INI_PROTO_H_ */
