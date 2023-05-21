/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		user__proto.h
 *	DESCRIPTION:	Prototype Header file for user_dsql.c
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

#ifndef _DSQL_USER__PROTO_H_
#define _DSQL_USER__PROTO_H_

extern STATUS API_ROUTINE	gds__close (STATUS *, SCHAR *);
extern STATUS API_ROUTINE	gds__declare (STATUS *, SCHAR *,SCHAR *);
extern STATUS API_ROUTINE	gds__describe (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__describe_bind (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__dsql_finish (SLONG **);
extern STATUS API_ROUTINE	gds__execute (STATUS *, SLONG *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__execute_immediate (STATUS *, SLONG *, SLONG *, SSHORT *, SCHAR *);
extern STATUS API_ROUTINE	gds__fetch (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__fetch_a (STATUS *, int *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__open (STATUS *, SLONG *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__prepare (STATUS *, SLONG *, SLONG *, SCHAR *, SSHORT *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	gds__to_sqlda (SQLDA *, int, SCHAR *, int, SCHAR *);
extern STATUS API_ROUTINE	isc_close (STATUS *, SCHAR *);
extern STATUS API_ROUTINE	isc_declare (STATUS *, SCHAR *, SCHAR *);
extern STATUS API_ROUTINE	isc_describe (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_describe_bind (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_dsql_finish (SLONG **);
extern STATUS API_ROUTINE	isc_dsql_release (STATUS *, SCHAR *);
extern STATUS API_ROUTINE	isc_dsql_fetch_a (STATUS *, int *, int *, USHORT, int *);
#ifdef SCROLLABLE_CURSORS
extern STATUS API_ROUTINE	isc_dsql_fetch2_a (STATUS *, int *, int *, USHORT, int *, USHORT, SLONG);
#endif
extern STATUS API_ROUTINE	isc_embed_dsql_close (STATUS *, SCHAR *);
extern STATUS API_ROUTINE	isc_embed_dsql_declare (STATUS *, SCHAR *, SCHAR *);
extern STATUS API_ROUTINE	isc_embed_dsql_descr_bind (STATUS *, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_describe (STATUS *, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_describe_bind (STATUS *, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_exec_immed (STATUS *, SLONG **, SLONG **, USHORT, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_exec_immed2 (STATUS *, SLONG **, SLONG **, USHORT, SCHAR *, USHORT, XSQLDA *, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_execute (STATUS *, SLONG **, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_execute2 (STATUS *, SLONG **, SCHAR *, USHORT, XSQLDA *, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_execute_immed (STATUS *, SLONG **, SLONG **, USHORT, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_fetch (STATUS *, SCHAR *, USHORT, XSQLDA *);
#ifdef SCROLLABLE_CURSORS
extern STATUS API_ROUTINE	isc_embed_dsql_fetch2 (STATUS *, SCHAR *, USHORT, XSQLDA *, USHORT, SLONG);
#endif
extern STATUS API_ROUTINE	isc_embed_dsql_fetch_a (STATUS *, int *, SCHAR *, USHORT, XSQLDA *);
#ifdef SCROLLABLE_CURSORS
extern STATUS API_ROUTINE	isc_embed_dsql_fetch2_a (STATUS *, int *, SCHAR *, USHORT, XSQLDA *, USHORT, SLONG);
#endif
extern STATUS API_ROUTINE	isc_embed_dsql_insert (STATUS *, SCHAR *, USHORT, XSQLDA *);
extern void API_ROUTINE		isc_embed_dsql_length (UCHAR *, USHORT *);
extern STATUS API_ROUTINE	isc_embed_dsql_open (STATUS *, SLONG **, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_open2 (STATUS *, SLONG **, SCHAR *, USHORT, XSQLDA *, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_prepare (STATUS *, SLONG **, SLONG **, SCHAR *, USHORT, SCHAR *, USHORT, XSQLDA *);
extern STATUS API_ROUTINE	isc_embed_dsql_release (STATUS *, SCHAR *);
extern STATUS API_ROUTINE	isc_execute (STATUS *, SLONG *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_execute_immediate (STATUS *, SLONG *, SLONG *, SSHORT *, SCHAR *);
extern STATUS API_ROUTINE	isc_fetch (STATUS *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_fetch_a (STATUS *, int	*, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_open (STATUS *, SLONG *, SCHAR *, SQLDA *);
extern STATUS API_ROUTINE	isc_prepare (STATUS *, SLONG *, SLONG *, SCHAR *, SSHORT *, SCHAR *, SQLDA *);
extern int API_ROUTINE		isc_to_sqlda (SQLDA *, int, SCHAR *, int, SCHAR *);

#ifdef  WINDOWS_ONLY
extern void     UDSQL_wep (void);
#endif

#endif	/*  _DSQL_USER__PROTO_H_ */
