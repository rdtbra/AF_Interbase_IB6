/*
 *	PROGRAM:	Interprocess server
 *	MODULE:		ips_proto.h
 *	DESCRIPTION:	Prototpe header file for ipserver.c
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

#ifndef _IPSRV_PROTO_H_
#define _IPSRV_PROTO_H_

extern	USHORT	IPS_init( HWND, USHORT, USHORT, USHORT);
extern	ULONG	IPS_start_thread( ULONG);

#endif /* _IPSRV_PROTO_H */ 
