/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		metd_proto.h
 *	DESCRIPTION:	Prototype Header file for metd.e
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

#ifndef _DSQL_METD_PROTO_H
#define _DSQL_METD_PROTO_H

extern void	METD_drop_procedure (struct req *, struct str *);
extern void	METD_drop_relation (struct req *, struct str *);
extern INTLSYM	METD_get_charset (struct req *, USHORT, UCHAR *);
extern INTLSYM	METD_get_collation (struct req *, struct str *);
extern void     METD_get_col_default (REQ, TEXT *, TEXT *, BOOLEAN *, TEXT *, USHORT);
extern STR	METD_get_default_charset (struct req *);
extern USHORT	METD_get_domain (struct req *, struct fld *, UCHAR *);
extern void     METD_get_domain_default (REQ, TEXT *, BOOLEAN *, TEXT *, USHORT);
extern UDF	METD_get_function (struct req *, struct str *);
extern NOD	METD_get_primary_key (struct req *, struct str *);
extern PRC	METD_get_procedure (struct req *, struct str *);
extern DSQL_REL	METD_get_relation (struct req *, struct str *);
extern STR	METD_get_trigger_relation (struct req *, struct str *, USHORT *);
extern USHORT	METD_get_type (struct req *, struct str *, UCHAR *, SSHORT *);
extern DSQL_REL	METD_get_view_relation (struct req *, UCHAR *, UCHAR *, USHORT);

#endif /*_DSQL_METD_PROTO_H */
