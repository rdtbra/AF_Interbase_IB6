/*
 *	PROGRAM:	InterBase International support
 *	MODULE:		lc_gb2312.c
 *	DESCRIPTION:	Language Drivers in the GB2312 family.  
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
#include "../jrd/license.h"

/* These macros have a duplicate in cv_gb2312.c */
#define	GB1(uc)	((UCHAR)((uc)&0xff)>=0xa1 && \
			 (UCHAR)((uc)&0xff)<=0xfe) /* GB2312 1st-byte */
#define	GB2(uc)	((UCHAR)((uc)&0xff)>=0xa1 && \
			 (UCHAR)((uc)&0xff)<=0xfe) /* GB2312 2nd-byte */

extern USHORT	famasc_key_length();
extern USHORT	famasc_string_to_key();
extern SSHORT	famasc_compare();
extern USHORT	famasc_to_upper();
extern USHORT	famasc_to_lower();
extern SSHORT	famasc_str_to_upper();
extern USHORT	CVGB_gb2312_byte2short();
extern SSHORT	CVGB_gb2312_mbtowc();


#define FAMILY_MULTIBYTE(id_number, name, charset, country) \
	cache->texttype_version =		IB_LANGDRV_VERSION; \
	cache->texttype_type =			(id_number); \
	cache->texttype_character_set =		(charset); \
	cache->texttype_country =		(country); \
	cache->texttype_bytes_per_char =	2; \
	cache->texttype_fn_init =		(name); \
	cache->texttype_fn_key_length =		famasc_key_length; \
	cache->texttype_fn_string_to_key =	famasc_string_to_key; \
	cache->texttype_fn_compare =		famasc_compare; \
        cache->texttype_fn_to_upper =           famasc_to_upper; \
        cache->texttype_fn_to_lower =           famasc_to_lower; \
        cache->texttype_fn_str_to_upper =       famasc_str_to_upper; \
	cache->texttype_collation_table =	(BYTE *) NULL; \
	cache->texttype_toupper_table =		(BYTE *) NULL; \
	cache->texttype_tolower_table =		(BYTE *) NULL; \
	cache->texttype_compress_table =	(BYTE *) NULL; \
	cache->texttype_expand_table =		(BYTE *) NULL; \
	cache->texttype_name =			POSIX; 


TEXTTYPE_ENTRY (GB_2312_init)
{
static CONST ASCII	POSIX[] = "C.GB_2312";

FAMILY_MULTIBYTE (2312, GB_2312_init, CS_GB2312, CC_C);
cache->texttype_fn_to_wc = CVGB_gb2312_byte2short;
cache->texttype_fn_mbtowc = CVGB_gb2312_mbtowc;

TEXTTYPE_RETURN;
}
#include "../intl/undef.h"


#undef FAMILY_MULTIBYTE
