/*
 *	PROGRAM:	JRD Lock Manager
 *	MODULE:		manager.c
 *	DESCRIPTION:	Lock manager process
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

#include "../jrd/common.h"
#include "../jrd/ib_stdio.h"
#include "../jrd/jrd.h"
#include "../jrd/lck.h"
#ifdef LINKS_EXIST
#include "../isc_lock/lock_proto.h"
#else
#include "../lock/lock_proto.h"
#endif  


void main (
    int		argc,
    char	**argv)
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
STATUS	status_vector [20];
SLONG	owner_handle;

if (setreuid (0, 0) < 0)
    ib_printf ("lock manager: couldn't set uid to superuser\n");

if (argc < 2)
    divorce_terminal (0);

status_vector [1] = 0;
owner_handle = 0;
if (!LOCK_init (status_vector, TRUE, getpid(), LCK_OWNER_process, &owner_handle))
    LOCK_manager (owner_handle);

LOCK_fini(status_vector, &owner_handle);
}
