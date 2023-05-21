/*
 *	PROGRAM:	
 *	MODULE:		ibmgrswi.h
 *	DESCRIPTION:	ibmgr switches
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

#ifndef _IBMGR_IBMGRSWI_H_
#define _IBMGR_IBMGRSWI_H_

#include "../jrd/common.h"

/* Switch constants */
#define IN_SW_IBMGR_0		0	/* not a known switch */
#define IN_SW_IBMGR_START	1	/* start server */
#define IN_SW_IBMGR_ONCE	2	/* start server once */
#define IN_SW_IBMGR_FOREVER	3	/* start server, restart when it dies */
#define IN_SW_IBMGR_SHUT	4	/* shutdown server */
#define IN_SW_IBMGR_NOW		5	/* immidiate shutdown */
#define IN_SW_IBMGR_NOAT	6	/* no new attachments */
#define IN_SW_IBMGR_NOTR	7	/* no new transaction */
#define IN_SW_IBMGR_IGNORE	8	/* do not shutdown */
#define IN_SW_IBMGR_PASSWORD	9	/* DB admin's password */
#define IN_SW_IBMGR_USER	10	/* user's name */
#define IN_SW_IBMGR_HOST	11	/* host where server's running */
#define IN_SW_IBMGR_SET		12	/* sets host/user/password */
#define IN_SW_IBMGR_SHOW	13	/* shows host/user/password */
#define IN_SW_IBMGR_QUIT	14	/* exit command line interface */
#define IN_SW_IBMGR_HELP	15	/* print help */
#define IN_SW_IBMGR_Z		16	/* version */
#define IN_SW_IBMGR_PRINT	17	/* Print Stats */
#define IN_SW_IBMGR_POOL	18	/* Print pool */

#define IN_SW_IBMGR_AMBIG	99	/* ambiguous switch */

static struct in_sw_tab_t ibmgr_in_sw_table [] = {
    IN_SW_IBMGR_START,		0,	"START",	0, 0, 0, FALSE,	0,	2,	NULL,   /* start server */
    IN_SW_IBMGR_ONCE,		0,	"ONCE",		0, 0, 0, FALSE,	0,	1,	NULL,	/* start server once */
    IN_SW_IBMGR_FOREVER,	0,	"FOREVER",	0, 0, 0, FALSE,	0,	1,	NULL,	/* restart when server dies */
    IN_SW_IBMGR_SHUT,		0,	"SHUT",		0, 0, 0, FALSE,	0,	3,      NULL,   /* shutdown server */
    IN_SW_IBMGR_NOW,		0,	"NOW",		0, 0, 0, FALSE,	0,	3,      NULL,   /* immidiate shutdown */

/* The following switches should be activated when
   appropriate functionality is implemented
*/
/*    IN_SW_IBMGR_NOAT,		0,	"NOAT",		0, 0, 0, FALSE,	0,	3,      NULL, *//* no new attachments */
/*    IN_SW_IBMGR_NOTR,		0,	"NOTR",		0, 0, 0, FALSE,	0,	3,      NULL, *//* no new transaction */
/*    IN_SW_IBMGR_IGNORE,	0,	"IGN",		0, 0, 0, FALSE,	0,	1,      NULL,  *//* do not shutdown */
    IN_SW_IBMGR_PASSWORD,	0,	"PASSWORD",	0, 0, 0, FALSE,	0,	2,      NULL,	/* DB admin's password */
    IN_SW_IBMGR_USER,		0,	"USER",		0, 0, 0, FALSE,	0,	1,      NULL,   /* user's name */

/* We can shutdown any server, but can start only local
   thus we do not allow to change host for time being
*/
/*    IN_SW_IBMGR_HOST,		0,	"HOST",		0, 0, 0, FALSE,	0,	2,      NULL, *//* where server's running */

/* SET in an internal operation and should not be activated
   (unless we want a user to use it
*/
/*    IN_SW_IBMGR_SET,		0,	"SET",		0, 0, 0, FALSE,	0,	2,	NULL,  *//* sets host/user/password */
    IN_SW_IBMGR_SHOW,		0,	"SHOW",		0, 0, 0, FALSE,	0,	3,	NULL,	/* shows host/user/password */
    IN_SW_IBMGR_QUIT,		0,	"QUIT",		0, 0, 0, FALSE,	0,	1,      NULL,	/* exit command line */
    IN_SW_IBMGR_HELP,		0,	"HELP",		0, 0, 0, FALSE,	0,	2,      NULL,	/* print help */
    IN_SW_IBMGR_Z,		0,	"Z",		0, 0, 0, FALSE,	0,	1,      NULL,	/* version */
    IN_SW_IBMGR_PRINT,     	0,	"PRINT",	0, 0, 0, FALSE,	0,	2,      NULL,	/* Print stats */
    IN_SW_IBMGR_POOL,     	0,	"POOL",		0, 0, 0, FALSE,	0,	2,      NULL,   /* Print pool */
    IN_SW_IBMGR_0,		0,      NULL,		0, 0, 0, FALSE,	0,	0,	NULL	/* End of List */
};

#endif /* _IBMGR_IBMGRSWI_H_ */
