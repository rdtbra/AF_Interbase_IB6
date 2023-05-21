/*
 *	PROGRAM:	Preprocessor
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

#ifndef _GPRE_MET_PROTO_H_
#define _GPRE_MET_PROTO_H_

extern FLD	MET_context_field (CTX, UCHAR *);
extern BOOLEAN	MET_database (DBB, BOOLEAN);
extern USHORT	MET_domain_lookup (REQ, FLD, UCHAR *);
extern FLD	MET_field (REL, UCHAR *);
extern NOD	MET_fields (CTX);
extern void	MET_fini (DBB);
extern SCHAR	*MET_generator (TEXT *, DBB);
extern BOOLEAN	MET_get_column_default (REL, TEXT*, TEXT*, USHORT);
extern BOOLEAN	MET_get_domain_default (DBB, TEXT*, TEXT*, USHORT);
extern USHORT	MET_get_dtype (USHORT, USHORT, USHORT *);
extern LLS	MET_get_primary_key (DBB, TEXT *);
extern PRC	MET_get_procedure (DBB, TEXT *, TEXT *);
extern REL	MET_get_relation (DBB, TEXT *, TEXT *);
extern INTLSYM	MET_get_text_subtype (SSHORT);
extern UDF	MET_get_udf (DBB, TEXT *);
extern REL	MET_get_view_relation (REQ, UCHAR *, UCHAR *, USHORT);
extern IND	MET_index (DBB, TEXT *);
extern void	MET_load_hash_table (DBB);
extern FLD	MET_make_field (SCHAR *, SSHORT, SSHORT, BOOLEAN);
extern IND	MET_make_index (SCHAR *);
extern REL	MET_make_relation (SCHAR *);
extern BOOLEAN	MET_type (FLD, TEXT *, SSHORT *);
extern BOOLEAN	MET_trigger_exists (DBB, TEXT *);

#endif /* _GPRE_MET_PROTO_H_ */
