/*
 *	PROGRAM:	JRD Backup and Restore Program
 *	MODULE:		burpswi.h
 *	DESCRIPTION:	Burp switches
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

#ifndef _BURP_BURPSWI_H_
#define _BURP_BURPSWI_H_

#include "../jrd/common.h"
#include "../jrd/ibase.h"

/* Local copies of global variables.  They wil be copied into
   a data structure. */

/* Local variables */

#define IN_SW_BURP_0         0       /* the unknowable switch */
#define IN_SW_BURP_B         1       /* backup */
#define IN_SW_BURP_C         2       /* create_database */
#define IN_SW_BURP_F         3       /* file names and starting page */
#define IN_SW_BURP_M         4       /* backup only metadata */
#define IN_SW_BURP_N         5       /* do not restore validity conditions */
#define IN_SW_BURP_P         6       /* specify output page size */
#define IN_SW_BURP_R         7       /* replace existing database */
#define IN_SW_BURP_U         9       /* don't back up security information */
#define IN_SW_BURP_V         10      /* verify actions */
#define IN_SW_BURP_Z         11      /* print version number */
#define IN_SW_BURP_D         12      /* backup file on tape - APOLLO only */
#define IN_SW_BURP_E         13      /* expand (no compress) */
#define IN_SW_BURP_Y         14      /* redirect/suppress status and error output */
#define IN_SW_BURP_L         15      /* ignore limbo transactions */
#define IN_SW_BURP_T         16      /* build a 'transportable' backup (V4 default) */
#define IN_SW_BURP_O         17      /* commit after each relation */
#define IN_SW_BURP_I         18      /* deactivate indexes */
#define IN_SW_BURP_K         19      /* kill any shadows defined on database*/
#define IN_SW_BURP_G         20      /* inhibit garbage collection */
#define IN_SW_BURP_IG        21      /* database is largely trash try anyway */
#define IN_SW_BURP_FA        22      /* blocking factor */
#define IN_SW_BURP_US        23      /* use all space on data page */
#define IN_SW_BURP_OL        24      /* write RDB$DESCRIPTIONS & SOURCE in old manner */
#define IN_SW_BURP_7         25      /* force creation of an ODS 7 database */
#define IN_SW_BURP_USER      26      /* default user name to use on attach */
#define IN_SW_BURP_PASS      27      /* default password to use on attach */
#define IN_SW_BURP_S         28      /* skip some number of bytes if find a bad attribute */
#define IN_SW_BURP_NT        29      /* build a "non-transportable" backup (V3 default) */
#define IN_SW_BURP_BUG8183   30      /* use workaround to allow restore database
                                   v3.3 with comment field inside of index
                                   definition */
#define IN_SW_BURP_ROLE      31      /* default SQL role to use on attach */
#define IN_SW_BURP_CO        32      /* convert external tables to internal tables during backup */
#define IN_SW_BURP_BU        33      /* specify page buffers for cache */
#define IN_SW_BURP_SE        34      /* use services manager */
#define IN_SW_BURP_MODE      35      /* database could be restored ReadOnly */
/**************************************************************************/
/* The next two 'virtual' switches are hidden from user and are needed    */
/* for services API                                                       */
/**************************************************************************/
#define IN_SW_BURP_HIDDEN_RDONLY	36
#define	IN_SW_BURP_HIDDEN_RDWRITE	37
/**************************************************************************/
    /* used 0BCDEFGILMNOPRSTUVYZ    available AHJQWX */

#define BURP_SW_MODE_RO	"read_only"
#define BURP_SW_MODE_RW	"read_write"

static struct in_sw_tab_t burp_in_sw_table [] = {
    IN_SW_BURP_B,    0,                 	"BACKUP_DATABASE",  0, 0, 0, FALSE, 60,	0, NULL,
                /* msg 60: %sBACKUP_DATABASE backup database to file */
    IN_SW_BURP_BU,   isc_spb_res_buffers,       "BUFFERS",          0, 0, 0, FALSE, 257, 0, NULL,
                /* msg 257: %sBU(FFERS) override default page buffers */
    IN_SW_BURP_C,    isc_spb_res_create,        "CREATE_DATABASE",   0, 0, 0, FALSE, 73, 0, NULL,
                /* msg 73: %sCREATE_DATABASE create database from backup file */
    IN_SW_BURP_CO,   isc_spb_bkp_convert,       "CONVERT",          0, 0, 0, FALSE, 254, 0, NULL,
                /* msg 254: %sCO(NVERT)  backup external files as tables */
#ifdef APOLLO
    /* -DEVICE may be followed by 'ct' or 'mt' where 'mt' is the default */
    IN_SW_BURP_D,    0,                 	"DEVICE",           0, 0, 0, FALSE, 62,	0, NULL,
                /* msg 62: %sDEVICE backup file device type on APOLLO (CT or MT) */
#endif
    IN_SW_BURP_E,    isc_spb_bkp_expand,        "EXPAND",           0, 0, 0, FALSE, 97,	0, NULL,
                /* msg 97: %sEXPAND no data compression */
    IN_SW_BURP_F,    0,				"FILE_NAMES",	    0, 0, 0, FALSE, 0, 0, NULL,
    IN_SW_BURP_FA,   isc_spb_bkp_factor,	"FACTOR",	    0, 0, 0, FALSE, 181, 0, NULL,
                /* msg 181; %sFACTOR  blocking factor */
    IN_SW_BURP_G,    isc_spb_bkp_no_garbage_collect, "GARBAGE_COLLECT",  0, 0, 0, FALSE, 177, 0, NULL,
                /* msg 177:%sGARBAGE_COLLECT inhibit garbage collection */
    IN_SW_BURP_I,    isc_spb_res_deactivate_idx, "INACTIVE",	    0, 0, 0, FALSE, 78, 0, NULL,
                /* msg 78:%sINACTIVE deactivate indexes during restore */
    IN_SW_BURP_IG,   isc_spb_bkp_ignore_checksums,   "IGNORE",	    0, 0, 0, FALSE, 178, 0, NULL,
                /* msg 178:%sIGNORE ignore bad checksums */
    IN_SW_BURP_K,    isc_spb_res_no_shadow,	"KILL",		    0, 0, 0, FALSE, 172, 0, NULL,
                /* msg 172:%sKILL restore without creating shadows */
    IN_SW_BURP_L,    isc_spb_bkp_ignore_limbo,	"LIMBO",	    0, 0, 0, FALSE, 98, 0, NULL,
                /* msg 98 ignore transactions in limbo */
    IN_SW_BURP_M,    isc_spb_bkp_metadata_only,	"METADATA",	    0, 0, 0, FALSE, 0, 0, NULL,
    IN_SW_BURP_M,    0,				"META_DATA",	    0, 0, 0, FALSE, 63, 0, NULL,
                /* msg 63: %sMETA_DATA backup metadata only */
#ifdef READONLY_DATABASE
    IN_SW_BURP_MODE, 0,				"MODE",		    0, 0, 0, FALSE, 278, 0, NULL,
                /* msg 278: %sMODE read_only or read_write access */
#endif  /* READONLY_DATABASE */
    IN_SW_BURP_N,    isc_spb_res_no_validity,	"NO_VALIDITY",	    0, 0, 0, FALSE, 187, 0, NULL,
                /* msg 187: %sN(O_VALIDITY) do not restore database validity conditions */
    IN_SW_BURP_NT,   isc_spb_bkp_non_transportable,      "NT",	    0, 0, 0, FALSE, 239, 0, NULL,
                /* msg 239: %sNT Non-Transportable backup file format */
    IN_SW_BURP_O,    isc_spb_res_one_at_a_time,	"ONE_AT_A_TIME",    0, 0, 0, FALSE, 99, 0, NULL,
                /* msg 99: %sONE_AT_A_TIME restore one relation at a time */
    IN_SW_BURP_OL,   isc_spb_bkp_old_descriptions,   "OLD_DESCRIPTIONS", 0, 0, 0, FALSE, 186, 0, NULL,
                /* msg 186: %sOLD_DESCRIPTIONS save old style metadata descriptions */
    IN_SW_BURP_P,    isc_spb_res_page_size,	"PAGE_SIZE",	    0, 0, 0, FALSE, 101, 0, NULL,
                /* msg 101: %sPAGE_SIZE override default page size */
    IN_SW_BURP_PASS, 0,				"PASSWORD",	    0, 0, 0, FALSE, 190, 0, NULL,
                /* msg 190: %sPA(SSWORD) InterBase password */
    IN_SW_BURP_R,    isc_spb_res_replace,	"REPLACE_DATABASE", 0, 0, 0, FALSE, 112, 0, NULL,
                /* msg 112: %sREPLACE_DATABASE replace database from backup file */
/**************************************************************
** msg 252: %sRO(LE) InterBase SQL role
***************************************************************/
    IN_SW_BURP_ROLE, isc_spb_sql_role_name,	"ROLE",		    0, 0, 0, FALSE, 252, 0, NULL,
    IN_SW_BURP_S,    0,				"SKIP_BAD_DATA",    0, 0, 0, FALSE, 0, 0, NULL,
    IN_SW_BURP_SE,   0,				"SERVICE",	    0, 0, 0, FALSE, 277, 0, NULL,
		/* msg 277: %sSE(RVICE) use services manager */
    IN_SW_BURP_T,    0,				"TRANSPORTABLE",    0, 0, 0, FALSE, 175, 0, NULL,
                /* msg 175: %sTRANSPORTABLE transportable backup -- data in XDR format */
/*
    IN_SW_BURP_U,    0,				"UNPROTECTED",	    0, 0, 0, FALSE, 0, 0, NULL,
*/
    IN_SW_BURP_US,   isc_spb_res_use_all_space,	"USE_ALL_SPACE",    0, 0, 0, FALSE, 276, 0, NULL,
                /* msg 276: %sUSE_(ALL_SPACE) do not reserve space for record versions */
    IN_SW_BURP_USER, 0,				"USER",		    0, 0, 0, FALSE, 191, 0, NULL,
                /* msg 191: %sUSER InterBase user name */
    IN_SW_BURP_V,    isc_spb_verbose,		"VERBOSE",	    0, 0, 0, FALSE, 0, 0, NULL,
    IN_SW_BURP_V,    0,				"VERIFY",	    0, 0, 0, FALSE, 113, 0, NULL,
                /* msg 113: %sVERIFY report each action taken */
    IN_SW_BURP_Y,    0,				"Y",		    0, 0, 0, FALSE, 109, 0, NULL,
                /* msg 109: %sY redirect/suppress output (file path or OUTPUT_SUPPRESS) */
    IN_SW_BURP_Z,    0,				"Z",		    0, 0, 0, FALSE, 104, 0, NULL,
                /* msg 104: %sZ print version number */
#ifdef DEV_BUILD
    IN_SW_BURP_7,    0,				"7",		    0, 0, 0, FALSE, 0, 0, NULL,
#endif
/* next switch is a hidden option in case of bug_no 8183 */
    IN_SW_BURP_BUG8183,  0,	"BUG_8183",	0, 0, 0, FALSE, 0, 0, NULL,
#ifdef READONLY_DATABASE
/**************************************************************************/
/* The next two 'virtual' switches are hidden from user and are needed    */
/* for services API                                                       */
/**************************************************************************/
    IN_SW_BURP_HIDDEN_RDONLY,	isc_spb_res_am_readonly,	"mode read_only",   0, 0, 0, FALSE, 0, 0, NULL,
    IN_SW_BURP_HIDDEN_RDWRITE,	isc_spb_res_am_readwrite,	"mode read_write",  0, 0, 0, FALSE, 0, 0, NULL,
/**************************************************************************/
#endif
    IN_SW_BURP_0,    	 0,	NULL,           0, 0, 0, FALSE, 0, 0, NULL
};


/* Definitions for GSPLIT */
#define	IN_SW_SPIT_0	0   /* the unknowable switch */
#define	IN_SW_SPIT_SP	30  /* splits back up files */
#define	IN_SW_SPIT_JT	31  /* joins back up files */


static struct in_sw_tab_t spit_in_sw_table [] = {
	IN_SW_SPIT_SP,	0,	"SPLIT_BK_FILE", 0, 0, 0, FALSE, 0, 0, NULL,
	IN_SW_SPIT_JT,	0,	"JOIN_BK_FILE",  0, 0, 0, FALSE, 0, 0, NULL,
	IN_SW_SPIT_0,	0,	NULL,		 0, 0, 0, FALSE, 0, 0, NULL
};

#endif /* _BURP_BURP_H_ */
