/*
 *	PROGRAM:	Microsoft LanManager includes
 *	MODULE:		lanman.h
 *	DESCRIPTION:	Files to be included for GDS support of MSLanManager
 *                remote interface
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

#ifndef _REMOTE_LANMAN_H_
#define _REMOTE_LANMAN_H_

#include <netcons.h>
#include <nmpipe.h>
#include <server.h>
#include <shares.h>
#include <neterr.h>

/*
#define PRIORITYINC                                            /
#define INCL_BASE                                              /
#include <os2.h>

#define PRIORITYDATA                                           /
PIDINFO pidinfo;                                               \
USHORT priority;

#define PRIORITYUP DosGetPrty( PRTYS_THREAD, &priority, 0 );   /
                   DosGetPid( &pidinfo );                      \
                   DosSetPrty( PRTYS_THREAD, PRTYC_REGULAR, 0, pidinfo.tid );

#define PRIORITYDOWN DosSetPrty( PRTYS_THREAD, priority, 0, pidinfo.tid );
*/

#endif /* _REMOTE_LANMAN_H_ */
