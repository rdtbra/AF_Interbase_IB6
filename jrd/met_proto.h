/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_proto.h
 *	DESCRIPTION:	Prototype header file for met.c
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

#ifndef _JRD_MET_PROTO_H_
#define _JRD_MET_PROTO_H_

#include "../jrd/exe.h"
#include "../jrd/jrn.h"
#include "../jrd/blf.h"

extern void		MET_activate_shadow (TDBB);
extern ULONG	MET_align (struct dsc *, USHORT);
extern void		MET_change_fields (TDBB, struct tra *, struct dsc *);
extern struct fmt	*MET_current (TDBB, struct rel *);
extern void		MET_delete_dependencies (TDBB, TEXT *, USHORT);
extern void		MET_delete_shadow (TDBB, USHORT);
extern void		MET_error (TEXT *, ...);
extern SCHAR		*MET_exact_name (TEXT *);
extern struct fmt	*MET_format (TDBB, register struct rel *, USHORT);
extern BOOLEAN		MET_get_char_subtype (TDBB, SSHORT *, UCHAR *, USHORT);
extern struct nod	*MET_get_dependencies (TDBB, struct rel *, TEXT *, struct csb *, SLONG [2], struct req **, struct csb **, TEXT *, USHORT);
extern struct fld	*MET_get_field (struct rel *, USHORT);
extern void		MET_get_shadow_files (TDBB, USHORT);
extern int		MET_get_walinfo (TDBB, struct logfiles **, ULONG *, struct logfiles **);
extern void		MET_load_trigger	(TDBB,
							struct rel *,
							TEXT *, VEC *);
extern void		DLL_EXPORT MET_lookup_cnstrt_for_index (TDBB, TEXT *, TEXT *);
extern void		MET_lookup_cnstrt_for_trigger (TDBB, TEXT *, TEXT *, TEXT *);
extern void		MET_lookup_exception	(TDBB, SLONG,
							TEXT *, TEXT *);
extern SLONG		MET_lookup_exception_number (TDBB,
							    TEXT *);
extern int		MET_lookup_field (TDBB, struct rel *, TEXT *);
extern BLF		MET_lookup_filter (TDBB, SSHORT, SSHORT);
extern SLONG		MET_lookup_generator (TDBB, TEXT *);
extern void		DLL_EXPORT MET_lookup_index (TDBB, TEXT *, TEXT *, USHORT);
extern SLONG		MET_lookup_index_name (TDBB, TEXT *, SLONG *, SSHORT *);
extern int		MET_lookup_partner (TDBB, struct rel *, struct idx *, UCHAR *);
extern struct prc	*MET_lookup_procedure (TDBB, SCHAR *);
extern struct prc	*MET_lookup_procedure_id (TDBB, SSHORT, BOOLEAN, USHORT);
extern struct rel	*MET_lookup_relation (TDBB, SCHAR *);
extern struct rel	*MET_lookup_relation_id (TDBB, SLONG, BOOLEAN);
extern struct nod	*MET_parse_blob (TDBB, struct rel *, SLONG [2], struct csb **, struct req **, BOOLEAN, BOOLEAN);
extern struct nod	*MET_parse_sys_trigger (TDBB, struct rel *);
extern int		MET_post_existence (TDBB, struct rel *);
extern void		MET_prepare (TDBB, struct tra *, USHORT, UCHAR *);
extern struct prc	*MET_procedure (TDBB, int, USHORT);
extern struct rel	*MET_relation (TDBB, USHORT);
extern void		MET_release_existence (struct rel *);
extern void 		MET_release_triggers (TDBB, VEC*);
extern void		MET_remove_procedure (TDBB, int, PRC);
extern void		MET_revoke (TDBB, struct tra *, TEXT *, TEXT *, TEXT *);
extern TEXT		*MET_save_name (TDBB, TEXT *);
extern void		MET_scan_relation (TDBB, struct rel *);
extern TEXT		*MET_trigger_msg (TDBB, TEXT *, USHORT);
extern void		MET_update_shadow (TDBB, struct sdw *, USHORT);
extern void		MET_update_transaction (TDBB, struct tra *, USHORT);

#endif /* _JRD_MET_PROTO_H_ */
