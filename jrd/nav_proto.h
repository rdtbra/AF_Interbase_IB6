/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		nav_proto.h
 *	DESCRIPTION:	Prototype header file for nav.c
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

#ifndef _JRD_NAV_PROTO_H_
#define _JRD_NAV_PROTO_H_

#include "../jrd/rse.h"

#ifdef SCROLLABLE_CURSORS
extern struct exp	*NAV_expand_index (register struct win *, struct irsb_nav *);
#endif
extern BOOLEAN		NAV_get_record (struct rsb *, struct irsb_nav *, struct rpb *, enum rse_get_mode);

#ifdef PC_ENGINE
extern BOOLEAN		NAV_find_record (struct rsb *, USHORT, USHORT, struct nod *);
extern void		NAV_get_bookmark (struct rsb *, struct irsb_nav *, struct bkm *);
extern BOOLEAN		NAV_reset_position (struct rsb *, struct rpb *);
extern BOOLEAN		NAV_set_bookmark (struct rsb *, struct irsb_nav *, struct rpb *, struct bkm *);
#endif

#endif /* _JRD_NAV_PROTO_H_ */
