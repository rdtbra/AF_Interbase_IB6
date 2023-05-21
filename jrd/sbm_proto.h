/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		sbm_proto.h
 *	DESCRIPTION:	Prototype header file for sbm.c
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

#ifndef _JRD_SBM_PROTO_H_
#define _JRD_SBM_PROTO_H_

#include "../jrd/sbm.h"

#ifdef DEV_BUILD
#include "../jrd/ib_stdio.h"
extern void		SBM_dump (IB_FILE *, register SBM);
#endif

extern struct sbm	**SBM_and (register struct sbm **, register struct sbm **);
extern int		SBM_clear (register struct sbm *, SLONG);
extern BOOLEAN		SBM_equal (register SBM, register SBM);
extern void		SBM_init (void);
extern int		SBM_next (register struct sbm *, SLONG *, enum rse_get_mode);
extern struct sbm	**SBM_or (register struct sbm **, register struct sbm **);
extern void		SBM_release (struct sbm *);
extern void		SBM_reset (struct sbm **);
extern void		SBM_set	(TDBB, struct sbm **, SLONG);
extern int		SBM_test (register struct sbm *, SLONG);
extern SLONG		SBM_size (struct sbm **);

#endif /* _JRD_SBM_PROTO_H_ */
