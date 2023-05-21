/*
 *	PROGRAM:	InterBase International support
 *	MODULE:		ld.c
 *	DESCRIPTION:	Language Driver lookup & support routines
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

#include "../jrd/ib_stdio.h"
#include "../intl/ldcommon.h"

#define	EXTERN_texttype(name)	extern USHORT name (TEXTTYPE, SSHORT, SSHORT)
#define EXTERN_convert(name)	extern USHORT name (CSCONVERT, SSHORT, SSHORT)
#define EXTERN_charset(name)	extern USHORT name (CHARSET, SSHORT, SSHORT)

EXTERN_texttype (DOS101_init);
EXTERN_texttype (DOS101_c2_init);
EXTERN_texttype (DOS101_c3_init);
EXTERN_texttype (DOS101_c4_init);
EXTERN_texttype (DOS101_c5_init);
EXTERN_texttype (DOS101_c6_init);
EXTERN_texttype (DOS101_c7_init);
EXTERN_texttype (DOS101_c8_init);
EXTERN_texttype (DOS101_c9_init);
EXTERN_texttype (DOS101_c10_init);

EXTERN_texttype (DOS102_init);
EXTERN_texttype (DOS105_init);
EXTERN_texttype (DOS106_init);

EXTERN_texttype (DOS107_init);
EXTERN_texttype (DOS107_c1_init);
EXTERN_texttype (DOS107_c2_init);
EXTERN_texttype (DOS107_c3_init);

EXTERN_texttype (DOS852_c0_init);
EXTERN_texttype (DOS852_c1_init);
EXTERN_texttype (DOS852_c2_init);
#ifdef BUG_6925
EXTERN_texttype (DOS852_c3_init);
#endif
EXTERN_texttype (DOS852_c4_init);
EXTERN_texttype (DOS852_c5_init);
EXTERN_texttype (DOS852_c6_init);
EXTERN_texttype (DOS852_c7_init);
EXTERN_texttype (DOS852_c8_init);

EXTERN_texttype (DOS857_c0_init);
EXTERN_texttype (DOS857_c1_init);
#ifdef BUG_6925
EXTERN_texttype (DOS857_c2_init);
#endif

EXTERN_texttype (DOS860_c0_init);
EXTERN_texttype (DOS860_c1_init);

EXTERN_texttype (DOS861_c0_init);
EXTERN_texttype (DOS861_c1_init);

EXTERN_texttype (DOS863_c0_init);
EXTERN_texttype (DOS863_c1_init);

EXTERN_texttype (CYRL_c0_init);
EXTERN_texttype (CYRL_c1_init);
EXTERN_texttype (CYRL_c2_init);

EXTERN_texttype (LATIN1_cp_init);

EXTERN_texttype (LAT139_init);
EXTERN_texttype (LAT140_init);
EXTERN_texttype (LAT141_init);
EXTERN_texttype (LAT142_init);
EXTERN_texttype (LAT143_init);
EXTERN_texttype (LAT144_init);
EXTERN_texttype (LAT145_init);
EXTERN_texttype (LAT146_init);
EXTERN_texttype (LAT148_init);
EXTERN_texttype (LAT149_init);
EXTERN_texttype (LAT151_init);
EXTERN_texttype (LAT152_init);
EXTERN_texttype (LAT153_init);
EXTERN_texttype (LAT154_init);

EXTERN_texttype (WIN1250_c0_init);
EXTERN_texttype (WIN1250_c1_init);
EXTERN_texttype (WIN1250_c2_init);
EXTERN_texttype (WIN1250_c3_init);
EXTERN_texttype (WIN1250_c4_init);

EXTERN_texttype (WIN1251_c0_init);
EXTERN_texttype (WIN1251_c1_init);

EXTERN_texttype (WIN1252_c0_init);
EXTERN_texttype (WIN1252_c1_init);
EXTERN_texttype (WIN1252_c2_init);
EXTERN_texttype (WIN1252_c3_init);
EXTERN_texttype (WIN1252_c4_init);
EXTERN_texttype (WIN1252_c5_init);

EXTERN_texttype (WIN1253_c0_init);
EXTERN_texttype (WIN1253_c1_init);

EXTERN_texttype (WIN1254_c0_init);
EXTERN_texttype (WIN1254_c1_init);

EXTERN_texttype (NEXT_c0_init);
EXTERN_texttype (NEXT_c1_init);
EXTERN_texttype (NEXT_c2_init);
EXTERN_texttype (NEXT_c3_init);
EXTERN_texttype (NEXT_c4_init);

EXTERN_texttype (DOS160_init);
EXTERN_texttype (DOS160_c1_init);
EXTERN_texttype (DOS160_c2_init);
EXTERN_texttype (DOS160_c3_init);
EXTERN_texttype (DOS160_c4_init);
EXTERN_texttype (DOS160_c5_init);
EXTERN_texttype (DOS160_c6_init);
EXTERN_texttype (DOS160_c7_init);
EXTERN_texttype (DOS160_c8_init);
EXTERN_texttype (DOS160_c9_init);
EXTERN_texttype (DOS160_c10_init);

EXTERN_texttype (UNI200_init);
EXTERN_texttype (UNI201_init);

EXTERN_texttype (JIS220_init);
EXTERN_texttype (JIS230_init);

EXTERN_charset (CS_iso_latin1);
EXTERN_charset (CS_win1250);
EXTERN_charset (CS_win1251);
EXTERN_charset (CS_win1252);
EXTERN_charset (CS_win1253);
EXTERN_charset (CS_win1254);
EXTERN_charset (CS_next);
EXTERN_charset (CS_cyrl);
EXTERN_charset (CS_dos_437);
EXTERN_charset (CS_dos_850);
EXTERN_charset (CS_dos_852);
EXTERN_charset (CS_dos_857);
EXTERN_charset (CS_dos_860);
EXTERN_charset (CS_dos_861);
EXTERN_charset (CS_dos_863);
EXTERN_charset (CS_dos_865);
EXTERN_charset (CS_unicode_101);
EXTERN_charset (CS_unicode_fss);
EXTERN_charset (CS_sjis);
EXTERN_charset (CS_euc_j);

EXTERN_charset (CS_big_5);
EXTERN_charset (CS_gb_2312);
EXTERN_charset (CS_ksc_5601);
EXTERN_convert (CV_ksc_5601_dict_init);

EXTERN_texttype (BIG5_init);
EXTERN_texttype (KSC_5601_init);
EXTERN_texttype (GB_2312_init);

EXTERN_convert (CV_dos_437_x_latin1);
EXTERN_convert (CV_dos_865_x_latin1);
EXTERN_convert (CV_dos_437_x_dos_865);
EXTERN_convert (CVJIS_sjis_x_eucj);

#ifdef DEV_BUILD
void LD_assert (
    SCHAR	*filename,
    int		lineno)
{
/**************************************
 *
 *	L D _ a s s e r t
 *
 **************************************
 *
 * Functional description
 *
 *	Utility function for assert() macro 
 *	Defined locally (clone from jrd/err.c) as ERR_assert isn't
 *	a shared module entry point on all platforms, whereas gds__log is.
 *
 **************************************/
TEXT	buffer [100];

sprintf (buffer, "Assertion failed: component intl, file \"%s\", line %d\n", filename, lineno);
#if !(defined VMS || defined WIN_NT || defined OS2_ONLY)
gds__log (buffer);
#endif
ib_printf (buffer);
}
#endif

/* Note: Cannot use a lookup table here as SUN4 cannot init pointers inside
 * shared libraries
 */

#define DRIVER(num, name) \
	case (num): \
		*fun = (FPTR_SHORT) (name); \
		return (0);

#define CHARSET_INIT(num, name) \
	case (num): \
		*fun = (FPTR_SHORT) (name); \
		return (0);

#define CONVERT_INIT_BI(to_cs, from_cs, name)\
	if (((parm1 == (to_cs)) && (parm2 == (from_cs))) ||\
	    ((parm2 == (to_cs)) && (parm1 == (from_cs)))) \
	    {\
	    *fun = (FPTR_SHORT) (name);\
	    return (0);\
	    }

USHORT DLL_EXPORT LD_lookup (
    USHORT	objtype,
    FPTR_SHORT	*fun,
    SSHORT	parm1,
    SSHORT	parm2)
{
/**************************************
 *
 *	L D _ l o o k u p 
 *
 **************************************
 *
 * Functional description
 *
 *	Lookup an international object implementation via object type
 *	and id (one id for charsets & collations, two ids for converters).
 *
 *	Note: This routine is a cousin of intl/ld2.c:LD2_lookup
 *
 * Return:
 *	If object implemented by this module:
 *		(fun)	set to the object initializer
 *		0	returned	
 *	Otherwise:
 *		(fun)	set to nil.
 *		1	returned
 *
 **************************************/

switch (objtype)
    {
    case type_texttype:
	switch (parm1) 
	    {

#define CHARSET(name, cs_id, coll_id, bytes, num, cs_symbol, cp_symbol) \
	    case (cs_id): \
		*fun = (FPTR_SHORT) (cp_symbol); \
		return (0);
#define CSALIAS(name, cs_id)
#define COLLATION(name, cc_id, cs_id, coll_id, symbol) \
	    case (((coll_id) << 8) | (cs_id)): \
		*fun = (FPTR_SHORT) (symbol); \
		return (0);
#define COLLATE_ALIAS(name, coll_id)
#define END_CHARSET

#define _INTL_COMPONENT_

#include "../jrd/intlnames.h"

#undef CHARSET
#undef CSALIAS
#undef COLLATION
#undef COLLATE_ALIAS
#undef END_CHARSET

/*
	    DRIVER (CS_DOS_437, DOS101_init);
	    DRIVER (CS_DOS_850, DOS160_init);
	    DRIVER (CS_DOS_865, DOS107_init);
	    DRIVER (CS_LATIN1,  LATIN1_cp_init);
	    DRIVER (CS_UNICODE_FSS, UNI201_init);
	    DRIVER (CS_SJIS,    JIS220_init);
	    DRIVER (CS_EUCJ,    JIS230_init);

	    DRIVER (101, DOS101_init);
	    DRIVER (102, DOS102_init);
	    DRIVER (105, DOS105_init);
	    DRIVER (106, DOS106_init);
	    DRIVER (107, DOS107_init);
	    DRIVER (139, LAT139_init);
	    DRIVER (140, LAT140_init);
	    DRIVER (141, LAT141_init);
	    DRIVER (142, LAT142_init);
	    DRIVER (143, LAT143_init);
	    DRIVER (144, LAT144_init);
	    DRIVER (145, LAT145_init);
	    DRIVER (146, LAT146_init);
	    DRIVER (148, LAT148_init);
	    DRIVER (149, LAT149_init);
	    DRIVER (151, LAT151_init);
	    DRIVER (152, LAT152_init);
	    DRIVER (153, LAT153_init);
	    DRIVER (154, LAT154_init);
	    DRIVER (160, DOS160_init);
	    DRIVER (200, UNI200_init);
	    DRIVER (201, UNI201_init);
	    DRIVER (220, JIS220_init);
	    DRIVER (230, JIS230_init);
*/

	    default:
		*fun = NULL;
		return (1);
	    };
    case type_charset:
	switch (parm1)
	    {
#define CHARSET(name, cs_id, coll_id, bytes, num, cs_symbol, cp_symbol) \
	    case (cs_id): \
		*fun = (FPTR_SHORT) (cs_symbol); \
		return (0);
#define CSALIAS(name, cs_id)
#define COLLATION(name, cc_id, cs_id, coll_id, symbol)
#define COLLATE_ALIAS(name, coll_id)
#define END_CHARSET

#define _INTL_COMPONENT_


#include "../jrd/intlnames.h"

#undef CHARSET
#undef CSALIAS
#undef COLLATION
#undef COLLATE_ALIAS

/*
	    CHARSET_INIT (CS_LATIN1,     CS_iso_latin1);
	    CHARSET_INIT (CS_DOS_437,    CS_dos_437);
	    CHARSET_INIT (CS_DOS_850,    CS_dos_850);
	    CHARSET_INIT (CS_DOS_865,    CS_dos_865);
	    CHARSET_INIT (CS_UNICODE101, CS_unicode_101);
	    CHARSET_INIT (CS_UNICODE_FSS, CS_unicode_fss);
	    CHARSET_INIT (CS_SJIS,       CS_sjis);
	    CHARSET_INIT (CS_EUCJ,       CS_euc_j);
*/
	    default:
		*fun = NULL;
		return (1);
	    };
    case type_csconvert:
	{
	CONVERT_INIT_BI (CS_DOS_437, CS_LATIN1,  CV_dos_437_x_latin1);
	CONVERT_INIT_BI (CS_DOS_865, CS_LATIN1,  CV_dos_865_x_latin1);
	CONVERT_INIT_BI (CS_DOS_437, CS_DOS_865, CV_dos_437_x_dos_865);
	CONVERT_INIT_BI (CS_SJIS,    CS_EUCJ,    CVJIS_sjis_x_eucj);
	*fun = NULL;
	return (1);
	}
    default:
#ifdef DEV_BUILD
	assert (0);
#endif
	*fun = NULL;
	return (1);
    }
}

#undef DRIVER
#undef CHARSET_INIT
#undef CONVERT_INIT_BI

