/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		event_proto.h
 *	DESCRIPTION:	Prototype Header file for event.c
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

#ifndef _JRD_EVENT_PROTO_H_
#define _JRD_EVENT_PROTO_H_

extern int		EVENT_cancel (SLONG);
extern SLONG		EVENT_create_session (STATUS *);
extern int		EVENT_delete_session (SLONG);
extern struct evh	*EVENT_init (STATUS *, USHORT);
extern int		EVENT_post (STATUS *, USHORT, TEXT *, USHORT, TEXT *, USHORT);
extern SLONG		EVENT_que (STATUS *, SLONG, USHORT, TEXT *, USHORT, UCHAR *, FPTR_VOID, void *);

#endif	/* _JRD_EVENT_PROTO_H_ */
