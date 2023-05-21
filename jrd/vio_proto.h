/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		vio_proto.h
 *	DESCRIPTION:	Prototype header file for vio.c
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

#ifndef _JRD_VIO_PROTO_H_
#define _JRD_VIO_PROTO_H_

extern void		VIO_backout (TDBB, struct rpb *, struct tra *);
extern int		VIO_chase_record_version (TDBB, struct rpb *, struct rsb *, struct tra *, struct blk *);
#ifdef PC_ENGINE
extern int		VIO_check_if_updated (TDBB, struct rpb *);
#endif
extern void		VIO_data (TDBB, register struct rpb *, struct blk *);
extern void		VIO_erase (TDBB, struct rpb *, struct tra *);
#ifdef GARBAGE_THREAD
extern void		VIO_fini (TDBB);
#endif
extern int		VIO_garbage_collect (TDBB, struct rpb *, struct tra *);
extern struct rec	*VIO_gc_record (TDBB, struct rel *);
extern int		VIO_get (TDBB, struct rpb *, struct rsb *, struct tra *, struct blk *);
extern int		VIO_get_current (TDBB, struct rpb *, struct tra *, struct blk *, USHORT);
#ifdef GARBAGE_THREAD
extern void		VIO_init (TDBB);
#endif
extern void 		VIO_merge_proc_sav_points (TDBB, struct tra *, struct sav **);
extern void		VIO_modify (TDBB, struct rpb *, struct rpb *, struct tra *);
extern BOOLEAN		VIO_next_record (TDBB, struct rpb *, struct rsb *, struct tra *, struct blk *, BOOLEAN, BOOLEAN);
extern struct rec	*VIO_record (TDBB, register struct rpb *, struct fmt *, struct plb *);
extern void		VIO_start_save_point (TDBB, struct tra *);
extern void		VIO_store (TDBB, struct rpb *, struct tra *);
extern BOOLEAN		VIO_sweep (TDBB, struct tra *);
extern void		VIO_verb_cleanup (TDBB, struct tra *);

#endif /* _JRD_VIO_PROTO_H_ */
