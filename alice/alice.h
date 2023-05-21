/*
 *	PROGRAM:	Alice
 *	MODULE:		alice.h
 *	DESCRIPTION:	Block definitions
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

#ifndef _ALICE_ALICE_H_
#define _ALICE_ALICE_H_

#include "../jrd/ib_stdio.h"

#include "../jrd/ibase.h"
#include "../jrd/ibsetjmp.h"
#include "../jrd/thd.h"
#include "../alice/all.h"
#include "../alice/alice_proto.h"


#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define BLKDEF(type, root, tail) type,
enum blk_t
    {
    type_MIN = 0,
#include "../alice/blk.h"
    type_MAX
    };
#undef BLKDEF

#define VAL_INVALID_DB_VERSION		0
#define VAL_RECORD_ERRORS		1
#define VAL_BLOB_PAGE_ERRORS		2
#define VAL_DATA_PAGE_ERRORS		3
#define VAL_INDEX_PAGE_ERRORS		4
#define VAL_POINTER_PAGE_ERRORS		5
#define VAL_TIP_PAGE_ERRORS		6
#define VAL_PAGE_ERRORS			7
#define MAX_VAL_ERRORS			8

typedef struct user_action {
    ULONG	ua_switches;
    UCHAR	*ua_user;
    UCHAR	*ua_password;
    UCHAR	ua_use;
    UCHAR	ua_force;
    BOOLEAN	ua_read_only;
    SLONG	ua_shutdown_delay;
    SLONG	ua_sweep_interval;
    SLONG	ua_transaction;
    SLONG	ua_page_buffers;
    USHORT	ua_debug;
    SLONG	ua_val_errors [MAX_VAL_ERRORS];
    TEXT	ua_log_file [MAXPATHLEN];
    USHORT	ua_db_SQL_dialect;
} *USER_ACTION;



/*  String block: used to store a string of constant length. */

typedef struct str {
    struct blk	str_header;
    USHORT	str_length;
    UCHAR	str_data[2];
} *STR;


/*  Transaction block: used to store info about a multidatabase transaction. */

typedef struct tdr {
    struct blk	tdr_header;
    struct tdr	*tdr_next;   		/* next subtransaction */
    SLONG	tdr_id;			/* database-specific transaction id */
    struct str	*tdr_fullpath;		/* full (possibly) remote pathname */
    TEXT	*tdr_filename;		/* filename within full pathname */
    struct str	*tdr_host_site;		/* host for transaction */
    struct str	*tdr_remote_site;	/* site for remote transaction */
    SLONG	*tdr_handle;		/* reconnected transaction handle */
    SLONG	*tdr_db_handle;		/* reattached database handle */
    USHORT	tdr_db_caps;		/* capabilities of database */
    USHORT	tdr_state;		/* see flags below */
} *TDR;

/* Transaction Description Record */

#define TDR_VERSION		1
#define TDR_HOST_SITE		1
#define TDR_DATABASE_PATH	2
#define TDR_TRANSACTION_ID	3
#define TDR_REMOTE_SITE		4

/* flags for tdr_db_caps */

#define CAP_none		0	
#define CAP_transactions	1	/* db has a RDB$TRANSACTIONS relation */

/* flags for tdr_state */

#define TRA_none	0	/* transaction description record is missing */
#define TRA_limbo	1	/* has been prepared */
#define TRA_commit	2	/* has committed */
#define TRA_rollback	3	/* has rolled back */
#define TRA_unknown	4	/* database couldn't be reattached, state is unknown */


/* a couple of obscure blocks used only in data allocator routines */

typedef struct vec {
    struct blk	vec_header;
    ULONG	vec_count;
    struct blk	*vec_object[1];
} *VEC;

typedef struct vcl {
    struct blk	vcl_header;
    ULONG	vcl_count;
    SLONG	vcl_long[1];
} *VCL;

/* Global switches and data */

#include "../jrd/svc.h"

typedef struct tgbl {
    struct thdd 	tgbl_thd_data;
    struct user_action	ALICE_data;
    PLB			ALICE_permanent_pool;
    PLB			ALICE_default_pool;
    STATUS		status_vector [ISC_STATUS_LENGTH];
    VEC			pools;
    UCHAR       	*alice_env;
    int         	exit_code;
    OUTPUTPROC  	output_proc;
    SLONG       	output_data;
    IB_FILE        	*output_file;
    SVC			service_blk;
    isc_db_handle	db_handle;
    isc_tr_handle	tr_handle;
    STATUS		*status;
    USHORT		sw_redirect;
    USHORT		sw_service;
    USHORT		sw_service_thd;


} *TGBL;

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#ifdef SUPERSERVER
#define GET_THREAD_DATA		((TGBL) THD_get_specific())
#define SET_THREAD_DATA		THD_put_specific ((THDD) tdgbl); \
				tdgbl->tgbl_thd_data.thdd_type=THDD_TYPE_TALICE
#define RESTORE_THREAD_DATA	THD_restore_specific();
#else
extern struct tgbl *gdgbl;

#define GET_THREAD_DATA		(gdgbl)
#define SET_THREAD_DATA		gdgbl = tdgbl; \
				tdgbl->tgbl_thd_data.thdd_type = THDD_TYPE_TGBL
#define RESTORE_THREAD_DATA
#endif

#define EXIT(code)		{  tdgbl->exit_code = (code); \
				   if (tdgbl->alice_env != NULL) \
					LONGJMP(tdgbl->alice_env, 1);  }

#define	NOOUTPUT	2

#endif /* _ALICE_ALICE_H_ */
