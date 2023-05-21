/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		utl_proto.h
 *	DESCRIPTION:	Prototype header file for utl.c
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

#ifndef _JRD_UTL_PROTO_H_
#define _JRD_UTL_PROTO_H_

#if !(defined _JRD_IBASE_H_ || defined DEFINED_GDS_QUAD)
typedef struct {
    SLONG	gds_quad_high;
    ULONG	gds_quad_low;
} GDS__QUAD;
#define DEFINED_GDS_QUAD
#endif

extern int API_ROUTINE		gds__blob_size (SLONG *, SLONG *, SLONG *, SLONG *);
extern void API_ROUTINE		isc_expand_dpb (SCHAR **, SSHORT *, ...);
extern int API_ROUTINE		gds__edit (TEXT *, USHORT);
extern SLONG API_ROUTINE	gds__event_block (SCHAR **, SCHAR **, USHORT, ...);
extern USHORT API_ROUTINE	gds__event_block_a (SCHAR **, SCHAR **, SSHORT, SCHAR **);
extern void API_ROUTINE		gds__event_block_s (SCHAR **, SCHAR **, SSHORT, SCHAR **, SSHORT *);
extern void API_ROUTINE		gds__event_counts (ULONG *, SSHORT, SCHAR *, SCHAR *);
extern void API_ROUTINE		gds__map_blobs (int *, int *);
extern void API_ROUTINE		gds__set_debug (int);
extern void API_ROUTINE		isc_set_login (UCHAR **, SSHORT *);
extern BOOLEAN API_ROUTINE	isc_set_path (TEXT *, USHORT, TEXT *);
extern void API_ROUTINE		isc_set_single_user (UCHAR **, SSHORT *, TEXT *);
extern int API_ROUTINE		gds__version (void **, FPTR_VOID, void *);
extern void API_ROUTINE		isc_format_implementation (USHORT, USHORT, TEXT *, USHORT, USHORT, TEXT *);
extern U_IPTR API_ROUTINE	isc_baddress (SCHAR *);
extern void API_ROUTINE		isc_baddress_s (SCHAR *, U_IPTR *);
extern int API_ROUTINE		BLOB_close (struct bstream *);
extern int API_ROUTINE		blob__display (SLONG *, void **, void **, TEXT *, SSHORT *);
extern int API_ROUTINE		BLOB_display (GDS__QUAD *, void *, void *, TEXT *);
extern int API_ROUTINE		blob__dump (SLONG *, void **, void **, TEXT *, SSHORT *);
extern int API_ROUTINE		BLOB_dump (GDS__QUAD *, void *, void *, SCHAR *);
extern int API_ROUTINE		blob__edit (SLONG *, void **, void **, TEXT *, SSHORT *);
extern int API_ROUTINE		BLOB_edit (GDS__QUAD *, void *, void *, SCHAR *);
extern int API_ROUTINE		BLOB_get (struct bstream *);
extern int API_ROUTINE		blob__load (SLONG *, void **, void **, TEXT *, SSHORT *);
extern int API_ROUTINE		BLOB_load (GDS__QUAD *, void *, void *, TEXT *);
extern int API_ROUTINE		BLOB_text_dump (GDS__QUAD *, void *, void *, SCHAR *);
extern int API_ROUTINE		BLOB_text_load (GDS__QUAD *, void *, void *, TEXT *);
extern struct bstream * API_ROUTINE Bopen (GDS__QUAD *, void *, void *, SCHAR *);
extern struct bstream * API_ROUTINE BLOB_open (void *, SCHAR *, int);
extern int API_ROUTINE		BLOB_put (SCHAR, struct bstream *);

#ifdef VMS
extern STATUS API_ROUTINE	gds__attach_database_d (STATUS *, struct dsc$descriptor_s *, int, SSHORT, SCHAR *, SSHORT);
extern void API_ROUTINE		gds__wake_init (void);
#endif

#endif /* _JRD_UTL_PROTO_H_ */
