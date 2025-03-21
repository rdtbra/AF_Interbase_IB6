/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		window.h
 *	DESCRIPTION:	Common descriptions
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

#ifndef WINDOW_H
#define WINDOW_H

#define APP_HSIZE			220
#define APP_VSIZE			150
#define APP_NAME			"InterBase Server"

#define ON_NOTIFYICON		WM_USER + 2

#define WIN_TEXTLEN			128
#define MSG_STRINGLEN		64
#define DRV_STRINGLEN		32
#define TMP_STRINGLEN		512


#define CHECK_VOLUME(a)		((a)->dbcv_devicetype == DBT_DEVTYP_VOLUME)
#define CHECK_USAGE(a)		((a)->dbcv_unitmask & ulInUseMask)

static  char *szClassName       = "IB_Server";

#define	IP_CONNECT_MESSAGE	WM_USER + 1

#endif // WINDOW_H
