/*
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
#ifndef	_ALICE_ALICESWI_H_
#define	_ALICE_ALICESWI_H_

#include "../jrd/common.h"
#include "../jrd/ibase.h"

/* switch definitions */

#define sw_list		       0x1L		/* Byte 0, Bit 0 */
#define sw_prompt	       0x2L		
#define sw_commit	       0x4L		
#define sw_rollback	       0x8L		
#define sw_sweep	      0x10L		
#define sw_validate	      0x20L		
#define sw_no_update	      0x40L		
#define sw_full		      0x80L		
#define sw_mend		     0x100L		/* Byte 1, Bit 0 */
#define sw_all		     0x200L		
#define sw_enable	     0x400L		
#define sw_disable	     0x800L		
#define sw_ignore	    0x1000L		
#define sw_activate	    0x2000L		
#define sw_two_phase	    0x4000L		
#define sw_housekeeping	    0x8000L		
#define sw_kill		   0x10000L		/* Byte 2, Bit 0 */
#define sw_begin_log	   0x20000L		
#define sw_quit_log	   0x40000L		
#define sw_write	   0x80000L		
#define sw_use		  0x100000L		
#define sw_user		  0x200000L		
#define sw_password	  0x400000L		
#define sw_shut		  0x800000L		
#define sw_online	 0x1000000L		/* Byte 3, Bit 0 */
#define sw_cache	 0x2000000L		
#define sw_attach	 0x4000000L		
#define sw_force	 0x8000000L		
#define sw_tran		0x10000000L		
#define sw_buffers	0x20000000L		
#define sw_mode		0x40000000L		
#define sw_set_db_dialect	0x80000000L		
#define sw_z		       0x0L

#define SW_MEND         sw_mend | sw_validate | sw_full

#define IN_SW_ALICE_0			0	/* not a known switch */
#define IN_SW_ALICE_LIST		1
#define IN_SW_ALICE_PROMPT		2
#define IN_SW_ALICE_COMMIT		3
#define IN_SW_ALICE_ROLLBACK		4
#define IN_SW_ALICE_SWEEP		5
#define IN_SW_ALICE_VALIDATE		6
#define IN_SW_ALICE_NO_UPDATE		7
#define IN_SW_ALICE_FULL		8
#define IN_SW_ALICE_MEND		9
#define IN_SW_ALICE_ALL			10
#define IN_SW_ALICE_ENABLE		11
#define IN_SW_ALICE_DISABLE		12
#define IN_SW_ALICE_IGNORE		13
#define IN_SW_ALICE_ACTIVATE		14
#define IN_SW_ALICE_TWO_PHASE		15
#define IN_SW_ALICE_HOUSEKEEPING	16
#define IN_SW_ALICE_KILL		17
#define IN_SW_ALICE_BEGIN_LOG		18
#define IN_SW_ALICE_QUIT_LOG		19
#define IN_SW_ALICE_WRITE		20
#define IN_SW_ALICE_USE			21
#define IN_SW_ALICE_USER		22
#define IN_SW_ALICE_PASSWORD		23
#define IN_SW_ALICE_SHUT		24
#define IN_SW_ALICE_ONLINE		25
#define IN_SW_ALICE_CACHE		26
#define IN_SW_ALICE_ATTACH		27
#define IN_SW_ALICE_FORCE		28
#define IN_SW_ALICE_TRAN		29
#define IN_SW_ALICE_BUFFERS		30
#define IN_SW_ALICE_Z			31
#define IN_SW_ALICE_X			32	/* set debug mode on */
#define IN_SW_ALICE_HIDDEN_ASYNC	33
#define IN_SW_ALICE_HIDDEN_SYNC		34
#define IN_SW_ALICE_HIDDEN_USEALL	35
#define IN_SW_ALICE_HIDDEN_RESERVE	36
#define IN_SW_ALICE_HIDDEN_RDONLY	37
#define IN_SW_ALICE_HIDDEN_RDWRITE	38
#define IN_SW_ALICE_MODE		39
#define IN_SW_ALICE_HIDDEN_FORCE	40
#define IN_SW_ALICE_HIDDEN_TRAN		41
#define IN_SW_ALICE_HIDDEN_ATTACH	42
#define IN_SW_ALICE_SET_DB_SQL_DIALECT	43

#define ALICE_SW_ASYNC	"async"
#define ALICE_SW_SYNC	"sync"
#define ALICE_SW_MODE_RO	"read_only"
#define ALICE_SW_MODE_RW	"read_write"

/* Switch table */
static struct in_sw_tab_t alice_in_sw_table [] = {
    IN_SW_ALICE_ACTIVATE,	isc_spb_prp_activate,	"activate",	sw_activate,
	0,		~sw_activate,	FALSE,	25, 0, NULL,
	/* msg 25: \t-activate shadow file for database usage */
    IN_SW_ALICE_ATTACH,		0,	"attach",	sw_attach,
	sw_shut,	0,		FALSE,	26, 0, NULL,
	/* msg 26: \t-attach\tshutdown new database attachments */
#ifdef DEV_BUILD
    IN_SW_ALICE_BEGIN_LOG,	0,	"begin_log",	sw_begin_log,
	0,		~sw_begin_log,	FALSE,	27, 0, NULL,
	/* msg 27: \t-begin_log\tbegin logging for replay utility */
#endif
    IN_SW_ALICE_BUFFERS,	isc_spb_prp_page_buffers,	"buffers",	sw_buffers,
	0,		0,		FALSE,	28, 0, NULL,
	/* msg 28: \t-buffers\tset page buffers <n> */
    IN_SW_ALICE_COMMIT,		isc_spb_rpr_commit_trans,	"commit",	sw_commit,
	0,		~sw_commit,	FALSE,	29, 0, NULL,
	/* msg 29: \t-commit\t\tcommit transaction <tr / all> */
    IN_SW_ALICE_CACHE,		0,	"cache",	sw_cache,
	sw_shut,	0,		FALSE,	30, 0, NULL,
	/* msg 30: \t-cache\t\tshutdown cache manager */
#ifdef DEV_BUILD
    IN_SW_ALICE_DISABLE,	0,	"disable",	sw_disable,
	0,		0,		FALSE,	31, 0, NULL,
	/* msg 31: \t-disable\tdisable WAL */
#endif
    IN_SW_ALICE_FULL,		isc_spb_rpr_full,	"full",		sw_full,
	sw_validate,	0,		FALSE,	32, 0, NULL,
	/* msg 32: \t-full\t\tvalidate record fragments (-v) */
    IN_SW_ALICE_FORCE,		0,	"force",	sw_force,
	sw_shut,	0,		FALSE,	33, 0, NULL,
	/* msg 33: \t-force\t\tforce database shutdown */
    IN_SW_ALICE_HOUSEKEEPING,	isc_spb_prp_sweep_interval,	"housekeeping", sw_housekeeping,
	0,		0,		FALSE,	34, 0, NULL,
	/* msg 34: \t-housekeeping\tset sweep interval <n> */
    IN_SW_ALICE_IGNORE,		isc_spb_rpr_ignore_checksum,	"ignore",	sw_ignore,
	0,		0,		FALSE,	35, 0, NULL,
	/* msg 35: \t-ignore\t\tignore checksum errors */
    IN_SW_ALICE_KILL,		isc_spb_rpr_kill_shadows,	"kill", 	sw_kill,
	0, 		0,	 	FALSE,	36, 0, NULL,
	/* msg 36: \t-kill\t\tkill all unavailable shadow files */
    IN_SW_ALICE_LIST,		isc_spb_rpr_list_limbo_trans,	"list", 	sw_list,
	0, 		~sw_list, 	FALSE,	37, 0, NULL,
	/* msg 37: \t-list\t\tshow limbo transactions */
    IN_SW_ALICE_MEND,		isc_spb_rpr_mend_db,	"mend", 	SW_MEND,
	0,	 	~sw_no_update, 	FALSE,	38, 0, NULL,
	/* msg 38: \t-mend\t\tprepare corrupt database for backup */
#ifdef READONLY_DATABASE
    IN_SW_ALICE_MODE,	0,		"mode",		sw_mode,
	0,		~sw_mode,	FALSE,	109, 0, NULL,
	/* msg 109: \t-mode\t\tread_only or read_write */
#endif  /* READONLY_DATABASE */
    IN_SW_ALICE_NO_UPDATE,	isc_spb_rpr_check_db,	"no_update",	sw_no_update,
	sw_validate,	0,		FALSE,	39, 0, NULL,
	/* msg 39: \t-no_update\tread-only validation (-v) */
    IN_SW_ALICE_ONLINE,		isc_spb_prp_db_online,	"online",	sw_online,
	0,		0,		FALSE,	40, 0, NULL,
	/* msg 40: \t-online\t\tdatabase online */
    IN_SW_ALICE_PROMPT,		0,	"prompt",	sw_prompt,
	sw_list,	0, 		FALSE,	41, 0, NULL,
	/* msg 41: \t-prompt\t\tprompt for commit/rollback (-l) */
    IN_SW_ALICE_PASSWORD,	0,	"password",	sw_password,
	0,		0, 		FALSE,	42, 0, NULL,
	/* msg 42: \t-password\tdefault password */
#ifdef DEV_BUILD
    IN_SW_ALICE_QUIT_LOG,	0,	"quit_log",	sw_quit_log,
	0,		~sw_quit_log, 	FALSE,	43, 0, NULL,
	/* msg 43: \t-quit_log\tquit logging for replay utility */
#endif
    IN_SW_ALICE_ROLLBACK,	isc_spb_rpr_rollback_trans,	"rollback",	sw_rollback,
	0,		~sw_rollback, 	FALSE,	44, 0, NULL,
	/* msg 44: \t-rollback\trollback transaction <tr / all> */
    IN_SW_ALICE_SET_DB_SQL_DIALECT,
	isc_spb_prp_set_sql_dialect,
	"sql_dialect",
	sw_set_db_dialect,
	0,
	0,
	FALSE,
	111, 
	0, 
	NULL,
	/* msg 111: \t-SQL_dialect\t\set dataabse dialect n */
    IN_SW_ALICE_SWEEP,		isc_spb_rpr_sweep_db,	"sweep", 	sw_sweep,
	0,		~sw_sweep, 	FALSE,	45, 0, NULL,
	/* msg 45: \t-sweep\t\tforce garbage collection */
    IN_SW_ALICE_SHUT,		0,	"shut",		sw_shut,
	0,		~(sw_shut |
			  sw_attach |
			  sw_cache |
			  sw_force |
			  sw_tran),	FALSE,	46, 0, NULL,
	/* msg 46: \t-shut\t\tshutdown */
    IN_SW_ALICE_TWO_PHASE,	isc_spb_rpr_recover_two_phase,	"two_phase",	sw_two_phase,
	0,		~sw_two_phase, 	FALSE,	47, 0, NULL,
	/* msg 47: \t-two_phase\tperform automated two-phase recovery */
    IN_SW_ALICE_TRAN,		0,	"tran",		sw_tran,
	sw_shut,	0,		FALSE,	48, 0, NULL,
	/* msg 48: \t-tran\t\tshutdown transaction startup */
    IN_SW_ALICE_USE,		0,	"use",		sw_use,
	0,		~sw_use,	FALSE,	49, 0, NULL,
	/* msg 49: \t-use\t\tuse full or reserve space for versions */
    IN_SW_ALICE_USER,		0,	"user",		sw_user,
	0,		0,		FALSE,	50, 0, NULL,
	/* msg 50: \t-user\t\tdefault user name */
    IN_SW_ALICE_VALIDATE,	isc_spb_rpr_validate_db,	"validate",	sw_validate,
	0,		~sw_validate, 	FALSE,	51, 0, NULL,
	/* msg 51: \t-validate\tvalidate database structure */
    IN_SW_ALICE_WRITE,		0,	"write",	sw_write,
	0,		~sw_write,	FALSE,	52, 0, NULL,
	/* msg 52: \t-write\t\twrite synchronously or asynchronously */
#ifdef DEV_BUILD
    IN_SW_ALICE_X,		0,	"x",		0,
	0,		0,		FALSE,	53, 0, NULL,
	/* msg 53: \t-x\t\tset debug on */
#endif
    IN_SW_ALICE_Z,		0,	"z",		sw_z,
	0,		0,		FALSE,	54, 0, NULL,
	/* msg 54: \t-z\t\tprint software version number */
/************************************************************************/
/* WARNING: All new switches should be added right before this comments */
/************************************************************************/
/* The next nine 'virtual' switches are hidden from user and are needed
   for services API
 ************************************************************************/
    IN_SW_ALICE_HIDDEN_ASYNC,	isc_spb_prp_wm_async,		"write async",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_SYNC,	isc_spb_prp_wm_sync,		"write sync",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_USEALL,	isc_spb_prp_res_use_full,	"use full",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_RESERVE,	isc_spb_prp_res,		"use reserve",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_FORCE,	isc_spb_prp_shutdown_db,	"shut -force",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_TRAN,	isc_spb_prp_deny_new_transactions,	"shut -tran",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_ATTACH,	isc_spb_prp_deny_new_attachments,	"shut -attach",	0,	0,	0,	FALSE,	0,	0,	NULL,
#ifdef READONLY_DATABASE
    IN_SW_ALICE_HIDDEN_RDONLY,	isc_spb_prp_am_readonly,	"mode read_only",	0,	0,	0,	FALSE,	0,	0,	NULL,
    IN_SW_ALICE_HIDDEN_RDWRITE,	isc_spb_prp_am_readwrite,	"mode read_write",	0,	0,	0,	FALSE,	0,	0,	NULL,
#endif  /* READONLY_DATABASE */
/************************************************************************/
    IN_SW_ALICE_0,		0,	NULL,		0,
	0,		0,		FALSE,	0, 0, NULL
};

#endif /* _ALICE_ALICESWI_H_ */
