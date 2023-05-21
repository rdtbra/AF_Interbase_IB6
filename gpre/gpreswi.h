/*
 *	PROGRAM:	Preprocessor
 *	MODULE:		gpreswi.h
 *	DESCRIPTION:	Gpre Command Switch definitions
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


#include "../jrd/common.h"

/*
 * Dynamic switch table type declaration.  Used to indicate that a
 * switch was set.  Allocated in MAIN.
 */
	 
typedef struct sw_tab_t {
	int	sw_in_sw;
} *SW_TAB;


/* 
 * Switch handling constants.  Note that IN_SW_COUNT must always be
 * one larger than the largest switch value
 */

#define	IN_SW_GPRE_0		0	/* not a known switch */
#define	IN_SW_GPRE_C		1	/* source is C */
#define	IN_SW_GPRE_D		2	/* SQL database declaration */
#define	IN_SW_GPRE_E		3 	/* accept either case */
#define	IN_SW_GPRE_F		4 	/* source is FORTRAN */
#define	IN_SW_GPRE_G		5 	/* internal GDS module */
#define	IN_SW_GPRE_I		6	/* use ID's rather than names */
#define	IN_SW_GPRE_M		7 	/* don't generate READY/START_TRANS */
#define	IN_SW_GPRE_N		8 	/* don't generate debug lines */
#define IN_SW_GPRE_O 	9	/* write output to standard out */
#define	IN_SW_GPRE_P		10 	/* source is PASCAL */
#define	IN_SW_GPRE_R		11 	/* generate raw BLR */
#define	IN_SW_GPRE_S		12 	/* generate normal, rather than C strings */
#define	IN_SW_GPRE_T		13	/* print trace messages */
#define	IN_SW_GPRE_X		14	/* database is EXTERNAL (used with /DATABASE only) */
#define IN_SW_GPRE_COB 	15    	/* source is (shudder) cobol */
#define IN_SW_GPRE_ANSI 	16	/* source is (worse and worse!) ansi format */
#define IN_SW_GPRE_BAS 	17	/* source is basic */
#define IN_SW_GPRE_PLI 	18	/* source is pli */
#define IN_SW_GPRE_ADA 	19	/* source is ada */
#define IN_SW_GPRE_HANDLES 	20	/* ada handles package */
#define IN_SW_GPRE_Z 	21	/* print software version */
#define IN_SW_GPRE_ALSYS 	22	/* source is alsys ada */
#define IN_SW_GPRE_D_FLOAT 	23	/* use blr_d_float for doubles */
#define IN_SW_GPRE_CXX 	24	/* source is C++ */
#define IN_SW_GPRE_SCXX 	25	/* source is C++ with Saber extension */
#define IN_SW_GPRE_SQLDA 	26	/* use old or new SQLDA */
#define IN_SW_GPRE_USER 	27	/* default username to use when attaching database */
#define IN_SW_GPRE_PASSWORD 	28	/* default password to use in attaching database */
#define IN_SW_GPRE_INTERP	29	/* default character set/collation */

#define IN_SW_GPRE_COUNT 	30	/* number of IN_SW values */
#define	IN_SW_GPRE_CPLUSPLUS	31	/* source is platform dependant C++ */
#define	IN_SW_GPRE_SQLDIALECT	32	/* SQL dialect passed */

static struct in_sw_tab_t gpre_in_sw_table [] = {
#ifdef ADA
    IN_SW_GPRE_ADA,	0,	"ADA",		0, 0, 0, FALSE,	0,	0,	"\t\textended ADA program",
#ifdef ALSYS_ADA
    IN_SW_GPRE_ALSYS,	0,	"ALSYS",	0, 0, 0, FALSE,	0,	0,	"\t\textended ADA program",
#endif
    IN_SW_GPRE_HANDLES,	0,	"HANDLES",	0, 0, 0, FALSE,	0,	0,	"\t\tADA handle package requires handle package name",
#endif
    IN_SW_GPRE_C,	0,	"C",		0, 0, 0, FALSE,	0,	0,	"\t\textended C program",
    IN_SW_GPRE_CXX,	0,	"CXX",		0, 0, 0, FALSE,	0,	0,	"\t\textended C++ program",
    IN_SW_GPRE_CPLUSPLUS,0,	"CPLUSPLUS",	0, 0, 0, FALSE,	0,	0,	"\textended C++ program",
    IN_SW_GPRE_D,	0,	"DATABASE",	0, 0, 0, FALSE,	0,	0,	"\tdatabase declaration requires database name",

    IN_SW_GPRE_D_FLOAT,	0,	"D_FLOAT",	0, 0, 0, FALSE,	0,	0,	"\t\tgenerate blr_d_float for doubles",
    IN_SW_GPRE_E,	0,	"EITHER_CASE",	0, 0, 0, FALSE,	0,	0,	"\taccept upper or lower case DML in C",
#ifdef FORTRAN
    IN_SW_GPRE_F,	0,	"FORTRAN",	0, 0, 0, FALSE,	0,	0,	"\t\textended FORTRAN program",
#endif
    IN_SW_GPRE_G,	0,	"GDS",		0, 0, 0, FALSE,	0,	0,	NULL,
    IN_SW_GPRE_I,	0,	"IDENTIFIERS",	0, 0, 0, FALSE,	0,	0,	NULL,
    IN_SW_GPRE_I,	0,	"IDS",		0, 0, 0, FALSE,	0,	0,	NULL,
    IN_SW_GPRE_INTERP,	0,	"CHARSET",	0, 0, 0, FALSE,	0,	0,	"\t\tDefault character set & format",
    IN_SW_GPRE_INTERP,	0,	"INTERPRETATION", 0, 0, 0, FALSE,0,	0,	NULL,
    IN_SW_GPRE_M,	0,	"MANUAL",	0, 0, 0, FALSE,	0,	0,	"\t\tdo not automatically ATTACH to a database",
    IN_SW_GPRE_N,	0,	"NO_LINES",	0, 0, 0, FALSE,	0,	0,	"\tdo not generate C debug lines",
    IN_SW_GPRE_O,	0,	"OUTPUT",	0, 0, 0, FALSE,	0,	0,	"\t\tsend output to standard out",
#ifdef PASCAL
    IN_SW_GPRE_P,	0,	"PASCAL",	0, 0, 0, FALSE,	0,	0,	"\t\textended PASCAL program",
#endif
    IN_SW_GPRE_PASSWORD,0,	"PASSWORD",	0, 0, 0, FALSE,	0,	0,	"\tdefault password",
    IN_SW_GPRE_R,	0,	"RAW",		0, 0, 0, FALSE,	0,	0,	"\t\tgenerate unformatted binary BLR",
    IN_SW_GPRE_SQLDIALECT,	0,	"SQL_DIALECT",	0, 0, 0, FALSE,	0,	0,	"\tSQL dialect to use",
    IN_SW_GPRE_S,	0,	"STRINGS",	0, 0, 0, FALSE,	0,	0,	NULL,
    IN_SW_GPRE_SQLDA,	0,	"SQLDA",	0, 0, 0, FALSE,	0,	0,	"\t\t***** Deprecated feature. ********",
    IN_SW_GPRE_T,	0,	"TRACE",	0, 0, 0, FALSE,	0,	0,	NULL,
    IN_SW_GPRE_USER,	0,	"USER",		0, 0, 0, FALSE,	0,	0,	"\t\tdefault user name",
#ifdef VMS
    IN_SW_GPRE_X,	0,	"EXTERNAL",	0, 0, 0, FALSE,	0,	0,	"\t\tEXTERNAL database (used with /DATABASE)",
#else
    IN_SW_GPRE_X,	0,	"X",		0, 0, 0, FALSE,	0,	0,	"\t\tEXTERNAL database (used with -DATABASE)",
#endif
#ifdef BASIC
    IN_SW_GPRE_BAS,	0,	"BASIC",	0, 0, 0, FALSE,	0,	0,	"\t\textended BASIC program",
#endif
#ifdef PLI
    IN_SW_GPRE_PLI,	0,	"PLI",		0, 0, 0, FALSE,	0,	0,	"\t\textended PLI program",
#endif
#ifdef COBOL
    IN_SW_GPRE_COB,	0,	"COB",		0, 0, 0, FALSE,	0,	0,	"\t\textended COBOL program",
    IN_SW_GPRE_ANSI,	0,	"ANSI",		0, 0, 0, FALSE,	0,	0,	"\t\tgenerate ANSI85 compatible COBOL",
#endif
    IN_SW_GPRE_Z,	0,	"Z",		0, 0, 0, FALSE,	0,	0,	"\t\tprint software version",
    IN_SW_GPRE_0,	0,	NULL,		0, 0, 0, FALSE,	0,	0,	NULL
};
