/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		inf_proto.h
 *	DESCRIPTION:	Prototype header file for inf.c
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

#ifndef _JRD_INF_PROTO_H_
#define _JRD_INF_PROTO_H_

extern int	INF_blob_info (struct blb *, SCHAR *, SSHORT, SCHAR *, SSHORT);
extern USHORT	DLL_EXPORT INF_convert (SLONG, SCHAR *);
extern int	INF_database_info (SCHAR *, SSHORT, SCHAR *, SSHORT);
extern SCHAR	*INF_put_item (SCHAR, USHORT, SCHAR *, SCHAR *, SCHAR *);
extern int	INF_request_info (struct req *, SCHAR *, SSHORT, SCHAR *, SSHORT);
extern int	INF_transaction_info (struct tra *, SCHAR *, SSHORT, SCHAR *, SSHORT);

#endif /* _JRD_INF_PROTO_H_ */
