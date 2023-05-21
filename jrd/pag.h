/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pag.h
 *	DESCRIPTION:	Page interface definitions
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

#ifndef _JRD_PAG_H_
#define _JRD_PAG_H_

/* Page control block -- used by PAG to keep track of critical
   constants */

typedef struct pgc {
    struct blk	pgc_header;
    SLONG	pgc_high_water;		/* Lowest PIP with space */
    SLONG	pgc_ppp;		/* Pages per pip */
    SLONG	pgc_pip;		/* First pointer page */
    int		pgc_bytes;		/* Number of bytes of bit in PIP */
    int		pgc_tpt;		/* Transactions per TIP */
} *PGC;

#endif /* _JRD_PAG_H_ */
