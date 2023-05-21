/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		blob_proto.h
 *	DESCRIPTION:	Prototype Header file for blob.e
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

#ifndef _DSQL_BLOB_PROTO_H_
#define _DSQL_BLOB_PROTO_H_

extern STATUS API_ROUTINE	isc_blob_gen_bpb (STATUS *, ISC_BLOB_DESC *, ISC_BLOB_DESC *, USHORT, UCHAR *, USHORT *);
extern STATUS API_ROUTINE	isc_blob_lookup_desc (STATUS *, void **, void **, UCHAR *, UCHAR *, ISC_BLOB_DESC *, UCHAR *);
extern STATUS API_ROUTINE	isc_blob_set_desc (STATUS *, UCHAR *, UCHAR *, SSHORT, SSHORT, SSHORT, ISC_BLOB_DESC *);

#endif /*_DSQL_BLOB_PROTO_H_ */
