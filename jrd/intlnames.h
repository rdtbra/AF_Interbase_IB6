/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		intlnames.h
 *	DESCRIPTION:	International objects for metadata initialization
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

/*
 *	CHARSET
 *	Binding's of symbols to character set ids.
 *	With implementation ID of default collation 
 * 	CHARSET (name, ID, default_subtype, max_bytes_per_char, num_chars, symbol)
 *
 *	Note: "name" is official name per InterBase
 *
 */

/*
 *	CSALIAS
 *	Alternate name for a character set (to account for platforms
 *	naming the same character set by alternate ways).
 *	CSALIAS	(name, ID)
 */

/*
 *	COLLATION
 *	Binding's of symbols to Collation ID's and Character
 *	set id's.
 *	Collation ID tells us the locale.
 *	Character Set ID tells us the character set.
 *	By SQL II specification, the collation name must be unique.
 *
 *	COLLATION (name, collation_id, charset_id, subtype_id, symbol)
 *
 *	Note: "name" is official name per InterBase
 */

#ifndef _INTL_COMPONENT_

CHARSET ("NONE",	CS_NONE,	0,	1,	256,	cs_none_init, dummy)
END_CHARSET

CHARSET ("OCTETS",	CS_BINARY,	0,	1,	256,	cs_binary_init, dummy)
CSALIAS	("BINARY",	CS_BINARY)
END_CHARSET

CHARSET ("ASCII",	CS_ASCII,	0,	1,	256,	cs_ascii_init, dummy)
CSALIAS	("USASCII",	CS_ASCII)
CSALIAS	("ASCII7",	CS_ASCII)
END_CHARSET

/* V3 SUB_TYPE 201 */
CHARSET ("UNICODE_FSS",	CS_UNICODE_FSS,	0,	3,	256*256, cs_unicode_fss_init, dummy)
CSALIAS ("UTF_FSS",	CS_UNICODE_FSS)
CSALIAS ("SQL_TEXT",	CS_UNICODE_FSS)
END_CHARSET

#endif

/* V3 SUB_TYPE 220 */
CHARSET ("SJIS_0208",	CS_SJIS,	0,	2,	7007,	CS_sjis, JIS220_init)
CSALIAS ("SJIS",	CS_SJIS)
END_CHARSET

/* V3 SUB_TYPE 230 */
CHARSET ("EUCJ_0208",	CS_EUCJ,	0,	2,	7007,	CS_euc_j, JIS230_init)
CSALIAS ("EUCJ",	CS_EUCJ)
END_CHARSET

CHARSET ("DOS437",	CS_DOS_437,	0,	1,	256,	CS_dos_437, DOS101_init)
CSALIAS ("DOS_437",	CS_DOS_437)
	/* V3 SUB_TYPE 101 */
	COLLATION ("PDOX_ASCII",	CC_ASCII,	CS_DOS_437,	1, DOS101_init)
	/* V3 SUB_TYPE 102 */
	COLLATION ("PDOX_INTL",		CC_INTL,	CS_DOS_437,	2, DOS102_init)
	/* V3 SUB_TYPE 106 */
	COLLATION ("PDOX_SWEDFIN",	CC_SWEDFIN,	CS_DOS_437,	3, DOS106_init)
	COLLATION ("DB_DEU437",		CC_GERMANY,	CS_DOS_437,	4, DOS101_c2_init)
	COLLATION ("DB_ESP437",		CC_SPAIN,	CS_DOS_437,	5, DOS101_c3_init)
	COLLATION ("DB_FIN437",		CC_FINLAND,	CS_DOS_437,	6, DOS101_c4_init)
	COLLATION ("DB_FRA437",		CC_FRANCE,	CS_DOS_437,	7, DOS101_c5_init)
	COLLATION ("DB_ITA437",		CC_ITALY,	CS_DOS_437,	8, DOS101_c6_init)
	COLLATION ("DB_NLD437",		CC_NEDERLANDS,	CS_DOS_437,	9, DOS101_c7_init)
	COLLATION ("DB_SVE437",		CC_SWEDEN,	CS_DOS_437,	10, DOS101_c8_init)
	COLLATION ("DB_UK437",		CC_UK,		CS_DOS_437,	11, DOS101_c9_init)
	COLLATION ("DB_US437",		CC_US,		CS_DOS_437,	12, DOS101_c10_init)
END_CHARSET

/* V3 SUB_TYPE 160 */
CHARSET ("DOS850",	CS_DOS_850,	0,	1,	256,	CS_dos_850, DOS160_init)
CSALIAS ("DOS_850",	CS_DOS_850)
	COLLATION ("DB_FRC850",		CC_FRENCHCAN,	CS_DOS_850,	1, DOS160_c1_init)
	COLLATION ("DB_DEU850",		CC_GERMANY,	CS_DOS_850,	2, DOS160_c2_init)
	COLLATION ("DB_ESP850",		CC_SPAIN,	CS_DOS_850,	3, DOS160_c3_init)
	COLLATION ("DB_FRA850",		CC_FRANCE,	CS_DOS_850,	4, DOS160_c4_init)
	COLLATION ("DB_ITA850",		CC_ITALY,	CS_DOS_850,	5, DOS160_c5_init)
	COLLATION ("DB_NLD850",		CC_NEDERLANDS,	CS_DOS_850,	6, DOS160_c6_init)
	COLLATION ("DB_PTB850",		CC_PORTUGAL,	CS_DOS_850,	7, DOS160_c7_init)
	COLLATION ("DB_SVE850",		CC_SWEDEN,	CS_DOS_850,	8, DOS160_c8_init)
	COLLATION ("DB_UK850",		CC_UK,		CS_DOS_850,	9, DOS160_c9_init)
	COLLATION ("DB_US850",		CC_US,		CS_DOS_850,	10, DOS160_c10_init)
END_CHARSET

/* V3 SUB_TYPE 107 */
CHARSET ("DOS865",	CS_DOS_865,	0,	1,	256,	CS_dos_865, DOS107_init)
CSALIAS ("DOS_865",	CS_DOS_865)
	/* V3 SUB_TYPE 105 */
	COLLATION ("PDOX_NORDAN4",	CC_NORDAN,	CS_DOS_865,	1, DOS107_c1_init)
	COLLATION ("DB_DAN865",		CC_DENMARK,	CS_DOS_865,	2, DOS107_c2_init)
	COLLATION ("DB_NOR865",		CC_NORWAY,	CS_DOS_865,	3, DOS107_c3_init)
END_CHARSET

CHARSET ("ISO8859_1",	CS_LATIN1,      0,	1,	256,	CS_iso_latin1, LATIN1_cp_init)
CSALIAS ("ISO_8859_1",	CS_LATIN1)
CSALIAS ("ISO88591",	CS_LATIN1)
CSALIAS ("LATIN1",	CS_LATIN1)
CSALIAS ("ANSI",	CS_LATIN1)
	/* V3 SUB_TYPE 139 */
	COLLATION ("DA_DA",		CC_DENMARK,	CS_LATIN1,	1, LAT139_init)
	/* V3 SUB_TYPE 140 */
	COLLATION ("DU_NL",		CC_NEDERLANDS,	CS_LATIN1,	2, LAT140_init)
	/* V3 SUB_TYPE 141 */
	COLLATION ("FI_FI",		CC_FINLAND,	CS_LATIN1,	3, LAT141_init)
	/* V3 SUB_TYPE 142 */
	COLLATION ("FR_FR",		CC_FRANCE,	CS_LATIN1,	4, LAT142_init)
	/* V3 SUB_TYPE 143 */
	COLLATION ("FR_CA",		CC_FRENCHCAN,	CS_LATIN1,	5, LAT143_init)
	/* V3 SUB_TYPE 144 */
	COLLATION ("DE_DE",		CC_GERMANY,	CS_LATIN1,	6, LAT144_init)
	/* V3 SUB_TYPE 145 */
	COLLATION ("IS_IS",		CC_ICELAND,	CS_LATIN1,	7, LAT145_init)
	/* V3 SUB_TYPE 146 */
	COLLATION ("IT_IT",		CC_ITALY,	CS_LATIN1,	8, LAT146_init)
	/* V3 SUB_TYPE 148 */
	COLLATION ("NO_NO",		CC_NORWAY,	CS_LATIN1,	9, LAT148_init)
	/* V3 SUB_TYPE 149 */
	COLLATION ("ES_ES",		CC_SPAIN,	CS_LATIN1,	10, LAT149_init)
	/* V3 SUB_TYPE 151 */
	COLLATION ("SV_SV",		CC_SWEDEN,	CS_LATIN1,	11, LAT151_init)
	/* V3 SUB_TYPE 152 */
	COLLATION ("EN_UK",		CC_UK,		CS_LATIN1,	12, LAT152_init)
	/* V3 SUB_TYPE 153 */
	COLLATION ("EN_US",		CC_US,		CS_LATIN1,	14, LAT153_init)
	/* V3 SUB_TYPE 154 */
	COLLATION ("PT_PT",		CC_PORTUGAL,	CS_LATIN1,	15, LAT154_init)
END_CHARSET

CHARSET ("DOS852",	CS_DOS_852,	0,	1,	256,	CS_dos_852, DOS852_c0_init)
CSALIAS("DOS_852",	CS_DOS_852)
	COLLATION ("DB_CSY",	        CC_CZECH,	CS_DOS_852,	1, DOS852_c1_init)
	COLLATION ("DB_PLK",		CC_POLAND,	CS_DOS_852,	2, DOS852_c2_init)
#ifdef BUG_6925
	COLLATION ("DB_HUN",		CC_HUNGARY,	CS_DOS_852,	3, DOS852_c3_init)
#endif
	COLLATION ("DB_SLO",		CC_YUGOSLAVIA,	CS_DOS_852,	4, DOS852_c4_init)
	COLLATION ("PDOX_CSY",	        CC_CZECH,	CS_DOS_852,	5, DOS852_c5_init)
	COLLATION ("PDOX_PLK",		CC_POLAND,	CS_DOS_852,	6, DOS852_c6_init)
	COLLATION ("PDOX_HUN",		CC_HUNGARY,	CS_DOS_852,	7, DOS852_c7_init)
	COLLATION ("PDOX_SLO",		CC_YUGOSLAVIA,	CS_DOS_852,	8, DOS852_c8_init)
END_CHARSET

CHARSET ("DOS857",	CS_DOS_857,	0,	1,	256,	CS_dos_857, DOS857_c0_init)
CSALIAS("DOS_857",	CS_DOS_857)
	COLLATION ("DB_TRK",		CC_TURKEY,	CS_DOS_857,	1, DOS857_c1_init)
#ifdef BUG_6925
	COLLATION ("PDOX_TURK",	        CC_TURKEY,	CS_DOS_857,	2, DOS857_c2_init)
#endif
END_CHARSET

CHARSET ("DOS860",	CS_DOS_860,	0,	1,	256,	CS_dos_860, DOS860_c0_init)
CSALIAS("DOS_860",	CS_DOS_860)
	COLLATION ("DB_PTG860",		CC_PORTUGAL,	CS_DOS_860,	1, DOS860_c1_init)
END_CHARSET

CHARSET ("DOS861",	CS_DOS_861,	0,	1,	256,	CS_dos_861, DOS861_c0_init)
CSALIAS("DOS_861",	CS_DOS_861)
	COLLATION ("PDOX_ISL",		CC_ICELAND,	CS_DOS_861,	1, DOS861_c1_init)
END_CHARSET

CHARSET ("DOS863",	CS_DOS_863,	0,	1,	256,	CS_dos_863, DOS863_c0_init)
CSALIAS("DOS_863",	CS_DOS_863)
	COLLATION ("DB_FRC863",		CC_FRENCHCAN,	CS_DOS_863,	1, DOS863_c1_init)
END_CHARSET

CHARSET ("CYRL",	CS_CYRL,	0,	1,	256,	CS_cyrl, CYRL_c0_init)
	COLLATION ("DB_RUS",		CC_RUSSIA,	CS_CYRL,	1, CYRL_c1_init)
	COLLATION ("PDOX_CYRL",	        CC_RUSSIA,	CS_CYRL,	2, CYRL_c2_init)
END_CHARSET

CHARSET ("WIN1250",	CS_WIN1250,	0,	1,	256,	CS_win1250, WIN1250_c0_init)
CSALIAS ("WIN_1250",	CS_WIN1250)
	COLLATION ("PXW_CSY",		CC_CZECH,	CS_WIN1250,	1, WIN1250_c1_init)
	COLLATION ("PXW_HUNDC",	        CC_HUNGARY,	CS_WIN1250,	2, WIN1250_c2_init)
	COLLATION ("PXW_PLK",	        CC_POLAND,	CS_WIN1250,	3, WIN1250_c3_init)
	COLLATION ("PXW_SLOV",		CC_YUGOSLAVIA,	CS_WIN1250,	4, WIN1250_c4_init)
END_CHARSET

CHARSET ("WIN1251",	CS_WIN1251,	0,	1,	256,	CS_win1251, WIN1251_c0_init)
CSALIAS ("WIN_1251",	CS_WIN1251)
	COLLATION ("PXW_CYRL",		CC_RUSSIA,	CS_WIN1251,	1, WIN1251_c1_init)
END_CHARSET

CHARSET ("WIN1252",	CS_WIN1252,	0,	1,	256,	CS_win1252, WIN1252_c0_init)
CSALIAS ("WIN_1252",	CS_WIN1252)
	COLLATION ("PXW_INTL",		CC_INTL,	CS_WIN1252,	1, WIN1252_c1_init)
	COLLATION ("PXW_INTL850",	CC_INTL,	CS_WIN1252,	2, WIN1252_c2_init)
	COLLATION ("PXW_NORDAN4",	CC_NORDAN,	CS_WIN1252,	3, WIN1252_c3_init)
	COLLATION ("PXW_SPAN",		CC_SPAIN,	CS_WIN1252,	4, WIN1252_c4_init)
	COLLATION ("PXW_SWEDFIN",	CC_SWEDFIN,	CS_WIN1252,	5, WIN1252_c5_init)
END_CHARSET

CHARSET ("WIN1253",	CS_WIN1253,	0,	1,	256,	CS_win1253, WIN1253_c0_init)
CSALIAS ("WIN_1253",	CS_WIN1253)
	COLLATION ("PXW_GREEK",		CC_GREECE,	CS_WIN1253,	1, WIN1253_c1_init)
END_CHARSET

CHARSET ("WIN1254",	CS_WIN1254,	0,	1,	256,	CS_win1254, WIN1254_c0_init)
CSALIAS ("WIN_1254",	CS_WIN1254)
	COLLATION ("PXW_TURK",		CC_TURKEY,	CS_WIN1254,	1, WIN1254_c1_init)
END_CHARSET

CHARSET ("NEXT",	CS_NEXT,	0,	1,	256,	CS_next, NEXT_c0_init)
	/* V3 SUB_TYPE 180 */
	COLLATION ("NXT_US",		CC_US,		CS_NEXT,	1, NEXT_c1_init)
	/* V3 SUB_TYPE 181 */
	COLLATION ("NXT_DEU",		CC_GERMANY,	CS_NEXT,	2, NEXT_c2_init)
	/* V3 SUB_TYPE 182 */
	COLLATION ("NXT_FRA",		CC_FRANCE,	CS_NEXT,	3, NEXT_c3_init)
	/* V3 SUB_TYPE 183 */
	COLLATION ("NXT_ITA",		CC_ITALY,	CS_NEXT,	4, NEXT_c4_init)
	/* V3 SUB_TYPE 184 */
	COLLATION ("NXT_ESP",		CC_SPAIN,	CS_NEXT,	5, NEXT_c0_init)
END_CHARSET

CHARSET ("KSC_5601",    CS_KSC5601,     0,      2,      4888,   CS_ksc_5601, KSC_5601_init)
CSALIAS ("KSC5601",     CS_KSC5601)
CSALIAS ("DOS_949",     CS_KSC5601)
CSALIAS ("WIN_949",     CS_KSC5601)
        COLLATION("KSC_DICTIONARY",     CC_KOREA,       CS_KSC5601,     1, CV_ksc_5601_dict_init)
END_CHARSET

CHARSET ("BIG_5",       CS_BIG5,        0,      2,      13481,  CS_big_5, BIG5_init)
CSALIAS ("BIG5",        CS_BIG5)
CSALIAS ("DOS_950",     CS_BIG5)
CSALIAS ("WIN_950",     CS_BIG5)
END_CHARSET

CHARSET ("GB_2312",     CS_GB2312,      0,      2,      6763,   CS_gb_2312, GB_2312_init)
CSALIAS ("GB2312",      CS_GB2312)
CSALIAS ("DOS_936",     CS_GB2312)
CSALIAS ("WIN_936",     CS_GB2312)
END_CHARSET
