/*
 *	PROGRAM:	InterBase International support
 *	MODULE:		lc_unicode.c
 *	DESCRIPTION:	Language Drivers in the Unicode family.
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

extern USHORT	famasc_key_length();
extern USHORT	famasc_string_to_key();
extern SSHORT	famasc_compare();
extern USHORT	famasc_to_upper();
extern USHORT	famasc_to_lower();
extern SSHORT	famasc_str_to_upper();
extern USHORT	CS_UTFFSS_fss_to_unicode();
extern SSHORT	CS_UTFFSS_fss_mbtowc();	
static SSHORT	wc_mbtowc();	

#define FAMILY_UNICODE_WIDE_BIN(id_number, name, charset, country) \
	cache->texttype_version =		IB_LANGDRV_VERSION; \
	cache->texttype_type =			(id_number); \
	cache->texttype_character_set =		(charset); \
	cache->texttype_country =		(country); \
	cache->texttype_bytes_per_char =	2; \
	cache->texttype_fn_init =		(name); \
	cache->texttype_fn_key_length =		famasc_key_length; \
	cache->texttype_fn_string_to_key =	famasc_string_to_key; \
	cache->texttype_fn_compare =		famasc_compare; \
	cache->texttype_fn_to_upper =		famasc_to_upper; \
	cache->texttype_fn_to_lower =		famasc_to_lower; \
	cache->texttype_fn_str_to_upper =	famasc_str_to_upper; \
	cache->texttype_collation_table =	(BYTE *) NULL; \
	cache->texttype_toupper_table =		(BYTE *) NULL; \
	cache->texttype_tolower_table =		(BYTE *) NULL; \
	cache->texttype_compress_table =	(BYTE *) NULL; \
	cache->texttype_expand_table =		(BYTE *) NULL; \
	cache->texttype_name =			POSIX; 

#define FAMILY_UNICODE_MB_BIN(id_number, name, charset, country) \
	cache->texttype_version =		IB_LANGDRV_VERSION; \
	cache->texttype_type =			(id_number); \
	cache->texttype_character_set =		(charset); \
	cache->texttype_country =		(country); \
	cache->texttype_bytes_per_char =	3; \
	cache->texttype_fn_init =		(name); \
	cache->texttype_fn_key_length =		famasc_key_length; \
	cache->texttype_fn_string_to_key =	famasc_string_to_key; \
	cache->texttype_fn_compare =		famasc_compare; \
	cache->texttype_fn_to_upper =		famasc_to_upper; \
	cache->texttype_fn_to_lower =		famasc_to_lower; \
	cache->texttype_fn_str_to_upper =	famasc_str_to_upper; \
	cache->texttype_collation_table =	(BYTE *) NULL; \
	cache->texttype_toupper_table =		(BYTE *) NULL; \
	cache->texttype_tolower_table =		(BYTE *) NULL; \
	cache->texttype_compress_table =	(BYTE *) NULL; \
	cache->texttype_expand_table =		(BYTE *) NULL; \
	cache->texttype_name =			POSIX; 


TEXTTYPE_ENTRY (UNI200_init)
{
static ASCII	POSIX[] = "C.UNICODE";

FAMILY_UNICODE_WIDE_BIN (200, UNI200_init, CS_UNICODE101, CC_C);
cache->texttype_fn_mbtowc = wc_mbtowc;

TEXTTYPE_RETURN;
}
#include "../intl/undef.h"

TEXTTYPE_ENTRY (UNI201_init)
{
static ASCII	POSIX[] = "C.UNICODE_FSS";

FAMILY_UNICODE_MB_BIN (201, UNI201_init, CS_UNICODE_FSS, CC_C);
cache->texttype_fn_to_wc = CS_UTFFSS_fss_to_unicode;
cache->texttype_fn_mbtowc = CS_UTFFSS_fss_mbtowc;

TEXTTYPE_RETURN;
}
#include "../intl/undef.h"

static SSHORT	wc_mbtowc (obj, wc, p, n)
    TEXTTYPE	*obj;
    WCHAR	*wc;
    NCHAR	*p;
    USHORT	n;
{
assert (obj);
assert (wc);
assert (p);

if (n < sizeof (WCHAR))
    return -1;
*wc = *(WCHAR *)p;
return sizeof (WCHAR);
}

#undef FAMILY_UNICODE_MB_BIN
#undef FAMILY_UNICODE_WIDE_BIN

