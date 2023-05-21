/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		ail_proto.h
 *	DESCRIPTION:	Prototype Header file for ail.c
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

#ifndef _JRD_AIL_PROTO_H_
#define _JRD_AIL_PROTO_H_

extern void	AIL_add_log (void);
extern void	AIL_checkpoint_finish (STATUS *, struct dbb *, SLONG, TEXT *, SLONG, SLONG);
extern void	AIL_commit (SLONG);
extern void	AIL_disable (void);
extern void	AIL_drop_log (void);
extern void	AIL_drop_log_force (void);
extern void	AIL_enable (TEXT *, USHORT, UCHAR *, USHORT, SSHORT);
extern void	AIL_fini (void);
extern void 	AIL_get_file_list (struct lls **);
extern void	AIL_init (TEXT *, SSHORT, struct win *, USHORT, struct sbm **);
extern void	AIL_init_log_page (struct log_info_page *, SLONG);
extern void	AIL_journal_tid (void);
extern void	AIL_process_jrn_error (SLONG);
extern void	AIL_put (struct dbb *, STATUS *, struct jrnh *, USHORT, UCHAR *, USHORT, ULONG, ULONG, ULONG *, ULONG *);
extern void	AIL_recover_page (SLONG, struct pag *);
extern void	AIL_set_log_options (SLONG, SSHORT, USHORT, SLONG);
extern void	AIL_shutdown (SCHAR *, SLONG *, SLONG *, SLONG *, SSHORT);
extern void	AIL_upd_cntrl_pt (TEXT *, USHORT, ULONG, ULONG, ULONG);

#if (defined PC_PLATFORM && !defined NETWARE_386)
#define AIL_add_log()
#define AIL_commit(a)
#define AIL_disable()
#define AIL_drop_log()
#define AIL_drop_log_force()
#define AIL_enable(a,b,c,d,e)
#define AIL_fini()
#define AIL_put(a,b,c,d,e,f,g,h,i,j)
#define AIL_set_log_options(a,b,c,d)
#define AIL_shutdown(a,b,c,d,e)
#define AIL_upd_cntrl_pt(a,b,c,d,e)
#endif

#endif	/* _JRD_AIL_PROTO_H_ */

