/*
 *	PROGRAM:	InterBase International support
 *	MODULE:		cs_unicode.c
 *	DESCRIPTION:	Character set definition for Unicode
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


#include "../intl/ldcommon.h"


extern USHORT	CV_wc_copy();


CHARSET_ENTRY (CS_unicode_101)
{
static CONST	WCHAR	space = 0x0020;

csptr->charset_version = 40;
csptr->charset_id = CS_UNICODE101;
csptr->charset_name = (ASCII *) "UNICODE";
csptr->charset_flags = 0;
csptr->charset_min_bytes_per_char = 2;
csptr->charset_max_bytes_per_char = 2;
csptr->charset_space_length = sizeof (space);
csptr->charset_space_character = (BYTE *) &space;	/* 0x0020 */
csptr->charset_well_formed = (FPTR_SHORT) NULL;
CV_convert_init (&csptr->charset_to_unicode, CS_UNICODE101, CS_UNICODE101,
		CV_wc_copy, NULL, NULL);
CV_convert_init (&csptr->charset_from_unicode, CS_UNICODE101, CS_UNICODE101,
		CV_wc_copy, NULL, NULL);
CHARSET_RETURN;
}
