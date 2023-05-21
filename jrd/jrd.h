/*
 *      PROGRAM:        JRD access method
 *      MODULE:         jrd.h
 *      DESCRIPTION:    Common descriptions
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

#ifndef _JRD_JRD_H_
#define _JRD_JRD_H_

#include "../jrd/gdsassert.h"
#include "../jrd/common.h"
#include "../jrd/dsc.h"

#ifdef DEV_BUILD
#ifndef WINDOWS_ONLY
#define DEBUG                   if (debug) DBG_supervisor();
#else 
#define DEBUG
#endif /* WINDOWS_ONLY */
#define VIO_DEBUG               /* remove this for production build */
#define WALW_DEBUG              /* remove this for production build */
#else /* PROD */
#define DEBUG
#undef VIO_DEBUG
#undef WALW_DEBUG             
#endif

#define MAX_PATH_LENGTH         512

#define BUGCHECK(number)        ERR_bugcheck (number)
#define CORRUPT(number)         ERR_corrupt (number)
#define IBERROR(number)         ERR_error (number)

/* Error delivery type */

typedef enum err_t {
    ERR_jmp,		/* longjmp */
    ERR_val,		/* value */
    ERR_ref		/* reference */
} ERR_T;

#define BLKCHK(blk, type)       if (((BLK) (blk))->blk_type != (UCHAR) (type)) BUGCHECK (147)

/* DEV_BLKCHK is used for internal consistency checking - where
 * the performance hit in production build isn't desired.
 * (eg: scatter this everywhere)
 */
#ifdef DEV_BUILD
#define DEV_BLKCHK(blk,type)    if (((BLK) (blk)) != NULL) {BLKCHK (blk, type);}
#else
#define DEV_BLKCHK(blk,type)    /* nothing */
#endif

#define ALLOCB(type)            ALL_alloc (dbb->dbb_bufferpool, type, 0, ERR_jmp)
#define ALLOCBV(type,repeat)    ALL_alloc (dbb->dbb_bufferpool, type, repeat, ERR_jmp)
#define ALLOCBR(type)           ALL_alloc (dbb->dbb_bufferpool, type, 0, ERR_val)
#define ALLOCBVR(type,repeat)   ALL_alloc (dbb->dbb_bufferpool, type, repeat, ERR_val)
#define ALLOCD(type)            ALL_alloc (tdbb->tdbb_default, type, 0, ERR_jmp)
#define ALLOCDV(type,repeat)    ALL_alloc (tdbb->tdbb_default, type, repeat, ERR_jmp)
#define ALLOCP(type)            ALL_alloc (dbb->dbb_permanent, type, 0, ERR_jmp)
#define ALLOCPV(type,repeat)    ALL_alloc (dbb->dbb_permanent, type, repeat, ERR_jmp)
#define ALLOCT(type)            ALL_alloc (transaction->tra_pool, type, 0, ERR_jmp)
#define ALLOCTV(type,repeat)    ALL_alloc (transaction->tra_pool, type, repeat, ERR_jmp)


/* Thread data block / IPC related data blocks */

#include "../jrd/thd.h"
#include "../jrd/isc.h"

/* definition of block types for data allocation */

#define BLKDEF(type, root, tail) type,
enum blk_t
    {
    type_MIN = 0,
#ifndef GATEWAY
#include "../jrd/blk.h"
#else
#include ".._gway/gway/blk.h"
#endif
    type_MAX
    };
#undef BLKDEF

/* Block types */

typedef struct blk {
    UCHAR       blk_type;
    UCHAR       blk_pool_id_mod;
    USHORT      blk_length;
} *BLK;

/* the database block, the topmost block in the metadata 
   cache for a database */

#ifdef GATEWAY
#include ".._gway/gway/gway.h"
#else
#define HASH_SIZE 101

typedef struct dbb {
    struct blk  dbb_header;
    struct dbb  *dbb_next;              /* Next database block in system */
    struct att  *dbb_attachments;       /* Active attachments */
    struct bcb  *dbb_bcb;               /* Buffer control block */
    struct vec  *dbb_relations;         /* relation vector */
    struct vec  *dbb_procedures;        /* scanned procedures */
    struct lck  *dbb_lock;              /* granddaddy lock */
    struct tra  *dbb_sys_trans;         /* system transaction */
    struct fil  *dbb_file;              /* files for I/O operations */
    struct sdw  *dbb_shadow;            /* shadow control block */
    struct lck  *dbb_shadow_lock;       /* lock for synchronizing addition of shadows */
    SLONG       dbb_shadow_sync_count;  /* to synchronize changes to shadows */
    struct lck  *dbb_retaining_lock;    /* lock for preserving commit retaining snapshot */
    struct plc  *dbb_connection;        /* connection block */
    struct pgc  *dbb_pcontrol;          /* page control */
    struct vcl  *dbb_t_pages;           /* pages number for transactions */
    struct vcl  *dbb_gen_id_pages;      /* known pages for gen_id */
    struct blf  *dbb_blob_filters;      /* known blob filters */
    struct lls	*dbb_modules;		/* external function/filter modules */
    MUTX_T      *dbb_mutexes;           /* DBB block mutexes */
    WLCK_T      *dbb_rw_locks;          /* DBB block read/write locks */
    REC_MUTX_T  dbb_sp_rec_mutex;       /* Recursive mutex for accessing/updating stored procedure metadata */
    SLONG       dbb_sort_size;          /* Size of sort space per sort */
    
    UATOM	dbb_ast_flags;		/* flags modified at AST level */
    ULONG       dbb_flags;
    USHORT      dbb_ods_version;        /* major ODS version number */
    USHORT      dbb_minor_version;      /* minor ODS version number */
    USHORT      dbb_minor_original;     /* minor ODS version at creation */
    USHORT      dbb_page_size;          /* page size */
    USHORT      dbb_dp_per_pp;          /* data pages per pointer page */
    USHORT      dbb_max_records;        /* max record per data page */
    USHORT      dbb_use_count;          /* active count of threads */
    USHORT      dbb_shutdown_delay;     /* seconds until forced shutdown */
    USHORT      dbb_refresh_ranges;     /* active count of refresh ranges */
    USHORT      dbb_prefetch_sequence;  /* sequence to pace frequency of prefetch requests */
    USHORT      dbb_prefetch_pages;     /* prefetch pages per request */
    struct str  *dbb_spare_string;      /* random buffer */
    struct str  *dbb_filename;          /* filename string */
#ifdef ISC_DATABASE_ENCRYPTION
    struct str  *dbb_encrypt_key;       /* encryption key */
#endif

    struct plb  *dbb_permanent;
    struct plb  *dbb_bufferpool;
    struct vec  *dbb_pools;             /* pools */
    struct vec  *dbb_internal;          /* internal requests */
    struct vec  *dbb_dyn_req;           /* internal dyn requests */
    struct jrn  *dbb_journal;           /* journal block */

    SLONG       dbb_oldest_active;      /* Cached "oldest active" transaction */
    SLONG       dbb_oldest_transaction; /* Cached "oldest interesting" transaction */
    SLONG	dbb_oldest_snapshot;	/* Cached "oldest snapshot" of all active transactions */
    SLONG       dbb_next_transaction;   /* Next transaction id used by NETWARE */
    SLONG       dbb_attachment_id;      /* Next attachment id for ReadOnly DB's */
    SLONG       dbb_page_incarnation;   /* Cache page incarnation counter */
    ULONG	dbb_page_buffers;	/* Page buffers from header page */

    EVENT_T     dbb_writer_event [1];   /* Event to wake up cache writer */
    EVENT_T     dbb_reader_event [1];   /* Event to wake up cache reader */
#ifdef GARBAGE_THREAD
    EVENT_T	dbb_gc_event [1];	/* Event to wake up garbage collector */
#endif
    struct att  *dbb_update_attachment; /* Attachment with update in process */
    struct btb  *dbb_update_que;        /* Attachments waiting for update */
    struct btb  *dbb_free_btbs;         /* Unused btb blocks */

    SLONG       dbb_current_memory;
    SLONG       dbb_max_memory;
    SLONG       dbb_reads;
    SLONG       dbb_writes;
    SLONG       dbb_fetches;
    SLONG       dbb_marks;
    SLONG       dbb_last_header_write;  /* Transaction id of last header page physical write */
    SLONG       dbb_flush_cycle;        /* Current flush cycle */
    SLONG       dbb_sweep_interval;     /* Transactions between sweep */
    SLONG       dbb_lock_owner_handle;  /* Handle for the lock manager */

#ifdef ISC_DATABASE_ENCRYPTION
    int         (*dbb_encrypt)();       /* External encryption routine */
    int         (*dbb_decrypt)();       /* External decryption routine */
#endif

    struct map  *dbb_blob_map;          /* mapping of blobs for REPLAY */ 
    struct log  *dbb_log;               /* log file for REPLAY */
    struct vec  *dbb_text_objects;      /* intl text type descriptions */
    struct vec  *dbb_charsets;          /* intl character set descriptions */
    struct wal  *dbb_wal;               /* WAL handle for WAL API */
    struct tpc  *dbb_tip_cache;         /* cache of latest known state of all transactions in system */
    struct vcl	*dbb_pc_transactions;	/* active precommitted transactions */
    struct sym  *dbb_hash_table [HASH_SIZE];    /* keep this at the end */
} *DBB;

/* bit values for dbb_flags */

#define DBB_no_garbage_collect 	0x1L
#define DBB_damaged         	0x2L
#define DBB_exclusive       	0x4L 		/* Database is accessed in exclusive mode */
#define DBB_bugcheck        	0x8L		/* Bugcheck has occurred */
#ifdef GARBAGE_THREAD
#define DBB_garbage_collector	0x10L		/* garbage collector thread exists */
#define DBB_gc_active		0x20L		/* ... and is actively working. */
#define DBB_gc_pending		0x40L		/* garbage collection requested */
#endif
#define DBB_force_write     	0x80L 		/* Database is forced write */
#define DBB_no_reserve     	0x100L 		/* No reserve space for versions */
#define DBB_add_log         	0x200L 		/* write ahead log has been added */
#define DBB_delete_log      	0x400L 		/* write ahead log has been deleted */
#define DBB_cache_manager   	0x800L 		/* Shared cache manager */
#define DBB_DB_SQL_dialect_3 	0x1000L 	/* database SQL dialect 3 */
#define DBB_read_only    	0x2000L		/* DB is ReadOnly (RO). If not set, DB is RW */ 
#define DBB_being_opened_read_only 0x4000L	/* DB is being opened RO. If unset, opened as RW */ 
#define DBB_not_in_use      	0x8000L 	/* DBB to be ignored while attaching */
#define DBB_lck_init_done   	0x10000L 	/* LCK_init() called for the database */
#define DBB_sp_rec_mutex_init 	0x20000L 	/* Stored procedure mutex initialized */
#define DBB_sweep_in_progress 	0x40000L 	/* A database sweep operation is in progress */
#define DBB_security_db     	0x80000L 	/* ISC security database */
#define DBB_sweep_thread_started 0x100000L 	/* A database sweep thread has been started */
#define DBB_suspend_bgio	0x200000L	/* Suspend I/O by background threads */
#define DBB_being_opened    	0x400000L 	/* database is being attached to */

/* dbb_ast_flags */

#define DBB_blocking        	0x1L 	/* Exclusive mode is blocking */
#define DBB_get_shadows     	0x2L 	/* Signal received to check for new shadows */
#define DBB_assert_locks    	0x4L 	/* Locks are to be asserted */
#define DBB_shutdown       	0x8L 	/* Database is shutdown */
#define DBB_shut_attach     	0x10L 	/* no new attachments accepted */
#define DBB_shut_tran       	0x20L 	/* no new transactions accepted */
#define DBB_shut_force      	0x40L 	/* forced shutdown in progress */
#define DBB_shutdown_locks  	0x80L 	/* Database locks release by shutdown */

/* Database attachments */

#define DBB_read_seq_count      0
#define DBB_read_idx_count      1
#define DBB_update_count        2
#define DBB_insert_count        3
#define DBB_delete_count        4
#define DBB_backout_count       5
#define DBB_purge_count         6
#define DBB_expunge_count       7
#define DBB_max_count           8

/* Database mutexes and read/write locks */

#define DBB_MUTX_init_fini      0       /* During startup and shutdown */
#define DBB_MUTX_statistics     1       /* Memory size and counts */
#define DBB_MUTX_replay         2       /* Replay logging */
#define DBB_MUTX_dyn            3       /* Dynamic ddl */
#define DBB_MUTX_cache          4       /* Process-private cache management */
#define DBB_MUTX_clone          5       /* Request cloning */
#define DBB_MUTX_cmp_clone      6       /* Compiled request cloning */
#ifndef NETWARE_386
#define DBB_MUTX_max            7
#else
#define DBB_MUTX_udf            7       /* UDF lookup */
#define DBB_MUTX_grant_priv     8       /* Generate user privileges */
#define DBB_MUTX_get_class      9      	/* Read a security class */
#define DBB_MUTX_max            10
#endif

#define DBB_WLCK_pools          0       /* Pool manipulation */
#define DBB_WLCK_files          1       /* DB and shadow file manipulation */
#define DBB_WLCK_max            2

/* Flags to indicate normal internal requests vs. dyn internal requests */

#define IRQ_REQUESTS            1
#define DYN_REQUESTS            2


/* Errors during validation - will be returned on info calls */

#define VAL_PAG_WRONG_TYPE          0
#define VAL_PAG_CHECKSUM_ERR        1
#define VAL_PAG_DOUBLE_ALLOC        2
#define VAL_PAG_IN_USE              3
#define VAL_PAG_ORPHAN              4
#define VAL_BLOB_INCONSISTENT       5
#define VAL_BLOB_CORRUPT            6
#define VAL_BLOB_TRUNCATED          7
#define VAL_REC_CHAIN_BROKEN        8
#define VAL_DATA_PAGE_CONFUSED      9
#define VAL_DATA_PAGE_LINE_ERR      10
#define VAL_INDEX_PAGE_CORRUPT      11
#define VAL_P_PAGE_LOST             12
#define VAL_P_PAGE_INCONSISTENT     13
#define VAL_REC_DAMAGED             14
#define VAL_REC_BAD_TID             15
#define VAL_REC_FRAGMENT_CORRUPT    16
#define VAL_REC_WRONG_LENGTH        17
#define VAL_INDEX_ROOT_MISSING      18
#define VAL_TIP_LOST                19
#define VAL_TIP_LOST_SEQUENCE       20
#define VAL_TIP_CONFUSED            21
#define VAL_REL_CHAIN_ORPHANS       22
#define VAL_INDEX_MISSING_ROWS      23
#define VAL_INDEX_ORPHAN_CHILD      24
#define VAL_MAX_ERROR               25


/* the attachment block; one is created for each attachment
   to a database */

typedef struct att {
    struct blk  att_header;
    struct dbb  *att_database;          /* Parent databasea block */
    struct att  *att_next;              /* Next attachment to database */
    struct att  *att_blocking;          /* Blocking attachment, if any */
    struct usr  *att_user;              /* User identification */
    struct tra  *att_transactions;      /* Transactions belonging to attachment */
    struct tra  *att_dbkey_trans;       /* transaction to control db-key scope */
    struct req  *att_requests;          /* Requests belonging to attachment */
    struct scb  *att_active_sorts;      /* Active sorts */
    struct lck  *att_id_lock;           /* Attachment lock (if any) */
    SLONG       att_attachment_id;      /* Attachment ID */
    SLONG       att_lock_owner_handle;  /* Handle for the lock manager */
    SLONG       att_event_session;      /* Event session id, if any */
    struct scl  *att_security_class;    /* security class for database */
    struct scl  *att_security_classes;  /* security classes */
    struct vcl  *att_counts [DBB_max_count];
    struct vec  *att_relation_locks;    /* explicit persistent locks for relations */
    struct bkm  *att_bookmarks;         /* list of bookmarks taken out using this attachment */
    struct lck  *att_record_locks;      /* explicit or implicit record locks taken out during attachment */
    struct vec  *att_bkm_quick_ref;     /* correspondence table of bookmarks */
    struct vec  *att_lck_quick_ref;     /* correspondence table of locks */
    ULONG	att_flags;              /* Flags describing the state of the attachment */
    SSHORT      att_charset;            /* user's charset specified in dpb */
    struct str  *att_lc_messages;       /* attachment's preference for message natural language */
    struct lck  *att_long_locks;        /* outstanding two phased locks */
    struct vec  *att_compatibility_table;       /* hash table of compatible locks */
    struct vcl  *att_val_errors;
    struct str	*att_working_directory; /* Current working directory is cached*/
} *ATT;


/* Attachment flags */

#define ATT_no_cleanup          1       /* Don't expunge, purge, or garbage collect */
#define ATT_shutdown            2       /* attachment has been shutdown */
#define ATT_shutdown_notify     4       /* attachment has notified client of shutdown */
#define ATT_shutdown_manager    8       /* attachment requesting shutdown */
#define ATT_lck_init_done       16      /* LCK_init() called for the attachment */
#define ATT_exclusive           32      /* attachment wants exclusive database access */
#define ATT_attach_pending      64      /* Indicate attachment is only pending */
#define ATT_exclusive_pending   128     /* Indicate exclusive attachment pending */
#define ATT_gbak_attachment	256     /* Indicate GBAK attachment */
#define ATT_security_db		512	/* Indicates an implicit attachment to the security db */
#ifdef GARBAGE_THREAD
#define ATT_notify_gc		1024	/* Notify garbage collector to expunge, purge .. */
#define ATT_disable_notify_gc	2048	/* Temporarily perform own garbage collection */
#define ATT_garbage_collector	4096	/* I'm a garbage collector */

#define ATT_NO_CLEANUP	(ATT_no_cleanup | ATT_notify_gc)
#else
#define ATT_NO_CLEANUP	ATT_no_cleanup
#endif

#ifdef CANCEL_OPERATION
#define ATT_cancel_raise	8192	/* Cancel currently running operation */
#define ATT_cancel_disable	16384	/* Disable cancel operations */
#endif

#define ATT_gfix_attachment	32768	/* Indicate a GFIX attachment */
#define ATT_gstat_attachment	65536	/* Indicate a GSTAT attachment */

/* Procedure block */

typedef struct prc {
    struct blk  prc_header;
    USHORT      prc_id; 
    USHORT      prc_flags;
    USHORT      prc_inputs;
    USHORT      prc_outputs;
    struct nod  *prc_input_msg;
    struct nod  *prc_output_msg;
    struct fmt  *prc_input_fmt;
    struct fmt  *prc_output_fmt;
    struct fmt  *prc_format;
    struct vec  *prc_input_fields;      /* vector of field blocks */
    struct vec  *prc_output_fields;     /* vector of field blocks */
    struct req  *prc_request;           /* compiled procedure request */
    struct str  *prc_security_name;     /* pointer to security class name for procedure */
    USHORT      prc_use_count;          /* requests compiled with relation */
    struct lck  *prc_existence_lock;    /* existence lock, if any */
    struct str  *prc_name;              /* pointer to ascic name */
    USHORT      prc_alter_count;        /* No. of times the procedure was altered*/

} *PRC;

#define PRC_scanned           1       /* Field expressions scanned */
#define PRC_system            2
#define PRC_obsolete          4       /* Procedure known gonzo */
#define PRC_being_scanned     8       /* New procedure needs dependencies during scan */
#define PRC_blocking          16      /* Blocking someone from dropping procedure*/
#define PRC_create            32      /* Newly created */
#define PRC_being_altered     64      /* Procedure is getting altered */

#define MAX_PROC_ALTER        64      /* No. of times an in-cache procedure can be altered */


/* Parameter block */

typedef struct prm {
    struct blk  prm_header;
    USHORT      prm_number;     
    struct dsc  prm_desc;
    TEXT        *prm_name;              /* pointer to asciiz name */
    TEXT        prm_string [2];         /* one byte for ALLOC and one for the terminating null */
} *PRM;

/* Primary dependencies from all foreign references to relation's
   primary/unique keys */

typedef struct prim {
    struct vec  *prim_reference_ids;
    struct vec  *prim_relations;
    struct vec  *prim_indexes;
} *PRIM;

/* Foreign references to other relations' primary/unique keys */

typedef struct frgn {
    struct vec  *frgn_reference_ids;
    struct vec  *frgn_relations;
    struct vec  *frgn_indexes;
} *FRGN;

/* Relation block; one is created for each relation referenced
   in the database, though it is not really filled out until
   the relation is scanned */

typedef struct rel {
    struct blk  rel_header;
    USHORT      rel_id; 
    USHORT      rel_flags;
    USHORT      rel_current_fmt;        /* Current format number */
    UCHAR	rel_length;		/* length of ascii relation name */
    struct fmt  *rel_current_format;    /* Current record format */
    TEXT        *rel_name;              /* pointer to ascii relation name */
    struct vec  *rel_formats;           /* Known record formats */
    TEXT        *rel_owner_name;        /* pointer to ascii owner */
    struct vcl  *rel_pages;             /* vector of pointer page numbers */
    struct vec  *rel_fields;            /* vector of field blocks */

    struct rse  *rel_view_rse;          /* view record select expression */
    struct vcx  *rel_view_contexts;     /* linked list of view contexts */

    TEXT        *rel_security_name;     /* pointer to security class name for relation */
    struct ext  *rel_file;              /* external file name */
    SLONG       rel_index_root;         /* index root page number */
    SLONG       rel_data_pages;         /* count of relation data pages */

    struct vec  *rel_gc_rec;            /* vector of records for garbage collection */
#ifdef GARBAGE_THREAD
    struct sbm	*rel_gc_bitmap;		/* garbage collect bitmap of data page sequences */
#endif

    USHORT      rel_slot_space;         /* lowest pointer page with slot space */
    USHORT      rel_data_space;         /* lowest pointer page with data page space */
    USHORT      rel_use_count;          /* requests compiled with relation */
    USHORT	rel_sweep_count;	/* sweep and/or garbage collector threads active */
    SSHORT	rel_scan_count;		/* concurrent sequential scan count */

    struct lck  *rel_existence_lock;    /* existence lock, if any */
    struct lck  *rel_interest_lock;     /* interest lock to ensure compatibility of relation and record locks */
    struct lck  *rel_record_locking;    /* lock to start record locking on relation */

    ULONG       rel_explicit_locks;     /* count of records explicitly locked in relation */
    ULONG       rel_read_locks;         /* count of records read locked in relation (implicit or explicit) */
    ULONG       rel_write_locks;        /* count of records write locked in relation (implicit or explicit) */
    ULONG       rel_lock_total;         /* count of records locked since database first attached */

    struct idl  *rel_index_locks;       /* index existence locks */
    struct idb  *rel_index_blocks;      /* index blocks for caching index info */
    struct vec  *rel_pre_erase;         /* Pre-operation erase trigger */
    struct vec  *rel_post_erase;        /* Post-operation erase trigger */
    struct vec  *rel_pre_modify;        /* Pre-operation modify trigger */
    struct vec  *rel_post_modify;       /* Post-operation modify trigger */
    struct vec  *rel_pre_store;         /* Pre-operation store trigger */
    struct vec  *rel_post_store;        /* Post-operation store trigger */
    struct prim rel_primary_dpnds;      /* foreign dependencies on this relation's primary key */
    struct frgn rel_foreign_refs;       /* foreign references to other relations' primary keys */
} *REL;

#define REL_scanned             1       /* Field expressions scanned (or being scanned) */
#define REL_system              2
#define REL_deleted             4       /* Relation known gonzo */
#define REL_get_dependencies    8       /* New relation needs dependencies during scan */
#define REL_force_scan          16      /* system relation has been updated since ODS change, force a scan */
#define REL_check_existence     32      /* Existence lock released pending drop of relation */
#define REL_blocking            64      /* Blocking someone from dropping relation */
#define REL_sys_triggers        128     /* The relation has system triggers to compile */
#define REL_sql_relation        256     /* Relation defined as sql table */
#define REL_check_partners      512     /* Rescan primary dependencies and foreign references */
#define	REL_being_scanned	1024	/* relation scan in progress */
#define REL_sys_trigs_being_loaded 2048 /* System triggers being loaded */
#define REL_deleting		4096	/* relation delete in progress */

/* Field block, one for each field in a scanned relation */

typedef struct fld {
    struct blk  fld_header;
    struct nod  *fld_validation;        /* validation clause, if any */
    struct nod  *fld_not_null;          /* if field cannot be NULL */
    struct nod  *fld_missing_value;     /* missing value, if any */
    struct nod  *fld_computation;       /* computation for virtual field */
    struct nod  *fld_source;            /* source for view fields */
    struct nod  *fld_default_value;     /* default value, if any */
    TEXT        *fld_security_name;     /* pointer to security class name for field */
    struct arr  *fld_array;             /* array description, if array */
    TEXT        *fld_name;              /* Field name */
    UCHAR	fld_length;		/* Field name length */
    UCHAR       fld_string [2];         /* one byte for ALLOC and one for the terminating null */
} *FLD;


/* Index block to cache index information */

typedef struct idb {
    struct blk  idb_header;
    struct idb  *idb_next;              
    struct nod  *idb_expression;        /* node tree for index expression */
    struct req  *idb_expression_request;/* request in which index expression is evaluated */
    struct dsc  idb_expression_desc;    /* descriptor for expression result */
    struct lck  *idb_lock;              /* lock to synchronize changes to index */
    UCHAR       idb_id;
} *IDB;

/* view context block to cache view aliases */

typedef struct vcx {
    struct blk  vcx_header;
    struct vcx  *vcx_next;              
    struct str  *vcx_context_name;
    struct str  *vcx_relation_name;
    USHORT      vcx_context;
} *VCX;
#endif

/* general purpose vector */

typedef struct vec {
    struct blk  vec_header;
    ULONG       vec_count;
    struct blk  *vec_object [1];
} *VEC;

typedef struct vcl {
    struct blk  vcl_header;
    ULONG       vcl_count;
    SLONG       vcl_long [1];
} *VCL;

#define TEST_VECTOR(vector,number)      ((vector && number < vector->vec_count) ? \
					  vector->vec_object [number] : NULL)
/* general purpose queue */

typedef struct que {
    struct que	*que_forward;
    struct que	*que_backward;
} *QUE;


/* symbol definitions */

typedef ENUM sym_t {
    SYM_rel,		/* relation block */
    SYM_fld,		/* field block */
    SYM_fun,		/* UDF function block */
    SYM_prc,		/* stored procedure block */
    SYM_sql,		/* SQL request cache block */
    SYM_blr		/* BLR request cache block */
} SYM_T;

typedef struct sym {
    struct blk  sym_header;
    TEXT        *sym_string;    /* address of asciz string */
/*  USHORT	sym_length; */	/* length of asciz string */
    SYM_T       sym_type;       /* symbol type */
    struct blk  *sym_object;    /* general pointer to object */
    struct sym  *sym_collision; /* collision pointer */
    struct sym  *sym_homonym;   /* homonym pointer */
} *SYM ;

/* Random string block -- jack of all kludges */

typedef struct str {
    struct blk  str_header;
    USHORT      str_length;
    UCHAR       str_data[2];    /* one byte for ALLOC and one for the NULL */
} *STR;

/* Transaction element block */

typedef struct teb {
    ATT         *teb_database;
    int         teb_tpb_length;
    UCHAR       *teb_tpb;
} TEB;

/* Blocking Thread Block */

typedef struct btb {
    struct blk  btb_header;
    struct btb  *btb_next;
    SLONG       btb_thread_id;
} *BTB;

/* Lock levels */

#define LCK_none        0
#define LCK_null        1
#define LCK_SR          2
#define LCK_PR          3
#define LCK_SW          4
#define LCK_PW          5
#define LCK_EX          6

#define LCK_read        LCK_PR
#define LCK_write       LCK_EX

#define LCK_WAIT        TRUE
#define LCK_NO_WAIT     FALSE

/* Lock query data aggregates */

#define LCK_MIN		1
#define LCK_MAX		2
#define LCK_CNT		3
#define LCK_SUM		4
#define LCK_AVG		5
#define LCK_ANY		6

/* Window block for loading cached pages into */

typedef struct win {
    SLONG       win_page;
    struct pag  *win_buffer;
    struct exp  *win_expanded_buffer;
    struct bdb  *win_bdb;
    SSHORT	win_scans;
    USHORT	win_flags;
} WIN;

#define	WIN_large_scan		1	/* large sequential scan */
#define WIN_secondary		2	/* secondary stream */
#define	WIN_garbage_collector	4	/* garbage collector's window */
#define WIN_garbage_collect	8	/* scan left a page for garbage collector */

/* define used for journaling start transaction */

#define MOD_START_TRAN  100

/* Thread specific database block */

typedef struct tdbb {
    struct thdd tdbb_thd_data;
    struct dbb  *tdbb_database;
    struct att  *tdbb_attachment;
    struct tra  *tdbb_transaction;
    struct req  *tdbb_request;
    struct plb  *tdbb_default;
    STATUS      *tdbb_status_vector;
    UCHAR       *tdbb_setjmp;
    USHORT      tdbb_inhibit;           /* Inhibit context switch if non-zero */
    SSHORT      tdbb_quantum;           /* Cycles remaining until voluntary schedule */
    USHORT	tdbb_flags;
    struct iuo  tdbb_mutexes;
    struct iuo  tdbb_rw_locks;
    struct iuo  tdbb_pages;
    void	*tdbb_sigsetjmp;
} *TDBB;

#define	TDBB_sweeper		1	/* Thread sweeper or garbage collector */
#define TDBB_no_cache_unwind	2	/* Don't unwind page buffer cache */
#define TDBB_prc_being_dropped	4       /* Dropping a procedure  */	

/* Threading macros */

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#ifdef V4_THREADING
#define PLATFORM_GET_THREAD_DATA ((TDBB) THD_get_specific())
#endif

#ifdef MULTI_THREAD
#if (defined APOLLO || defined DECOSF || defined NETWARE_386 || \
	defined NeXT || defined SOLARIS_MT || defined WIN_NT || \
	defined OS2_ONLY || defined HP10 || defined LINUX)
#define PLATFORM_GET_THREAD_DATA ((TDBB) THD_get_specific())
#endif
#endif

#ifdef WINDOWS_ONLY
#define PLATFORM_GET_THREAD_DATA ((TDBB) THD_get_specific())
#endif

#ifndef PLATFORM_GET_THREAD_DATA
extern TDBB     gdbb;
#define PLATFORM_GET_THREAD_DATA (gdbb)
#endif

/* Define GET_THREAD_DATA off the platform specific version.
 * If we're in DEV mode, also do consistancy checks on the
 * retrieved memory structure.  This was originally done to
 * track down cases of no "PUT_THREAD_DATA" on the NLM. 
 *
 * This allows for NULL thread data (which might be an error by itself)
 * If there is thread data, 
 * AND it is tagged as being a TDBB.
 * AND it has a non-NULL tdbb_database field, 
 * THEN we validate that the structure there is a database block.
 * Otherwise, we return what we got.
 * We can't always validate the database field, as during initialization
 * there is no tdbb_database set up.
 */
#ifdef DEV_BUILD
#define GET_THREAD_DATA (((PLATFORM_GET_THREAD_DATA) && \
			 (((THDD)(PLATFORM_GET_THREAD_DATA))->thdd_type == THDD_TYPE_TDBB) && \
			 (((TDBB)(PLATFORM_GET_THREAD_DATA))->tdbb_database)) \
			 ? ((((TDBB)(PLATFORM_GET_THREAD_DATA))->tdbb_database->dbb_header.blk_type == type_dbb) \
			    ? (PLATFORM_GET_THREAD_DATA) \
			    : (BUGCHECK (147), (PLATFORM_GET_THREAD_DATA))) \
			 : (PLATFORM_GET_THREAD_DATA))
#define CHECK_DBB(dbb)   assert ((dbb) && ((dbb)->dbb_header.blk_type == type_dbb))
#define CHECK_TDBB(tdbb) assert ((tdbb) && \
	(((THDD)(tdbb))->thdd_type == THDD_TYPE_TDBB) && \
	((!(tdbb)->tdbb_database)||(tdbb)->tdbb_database->dbb_header.blk_type == type_dbb))
#else
/* PROD_BUILD */
#define GET_THREAD_DATA (PLATFORM_GET_THREAD_DATA)
#define CHECK_TDBB(tdbb)/* nothing */
#define CHECK_DBB(dbb)/* nothing */
#endif

#define GET_DBB         (((TDBB) (GET_THREAD_DATA))->tdbb_database)

/*-------------------------------------------------------------------------*
 * macros used to set tdbb and dbb pointers when there are not set already *
 *-------------------------------------------------------------------------*/

#define	NULL_TDBB	((TDBB) NULL)
#define	NULL_DBB	((DBB) NULL)
#define	SET_TDBB(tdbb)	if ((tdbb) == NULL_TDBB) { (tdbb) = GET_THREAD_DATA; }; CHECK_TDBB (tdbb)
#define	SET_DBB(dbb)	if ((dbb)  == NULL_DBB)  { (dbb)  = GET_DBB; }; CHECK_DBB(dbb);

#ifdef V4_THREADING
#define V4_JRD_MUTEX_LOCK(mutx)         JRD_mutex_lock (mutx)
#define V4_JRD_MUTEX_UNLOCK(mutx)       JRD_mutex_unlock (mutx)
#define V4_JRD_RW_LOCK_LOCK(wlck,type)  JRD_wlck_lock (wlck, type)
#define V4_JRD_RW_LOCK_UNLOCK(wlck)     JRD_wlck_unlock (wlck)
#else
#define V4_JRD_MUTEX_LOCK(mutx)
#define V4_JRD_MUTEX_UNLOCK(mutx)
#define V4_JRD_RW_LOCK_LOCK(wlck,type)
#define V4_JRD_RW_LOCK_UNLOCK(wlck)
#endif

#ifdef ANY_THREADING
#define THD_JRD_MUTEX_LOCK(mutx)        JRD_mutex_lock (mutx)
#define THD_JRD_MUTEX_UNLOCK(mutx)      JRD_mutex_unlock (mutx)
#else
#define THD_JRD_MUTEX_LOCK(mutx)
#define THD_JRD_MUTEX_UNLOCK(mutx)
#endif

/* global variables for engine */


#ifndef REQUESTER
#ifndef SHLIB_DEFS
#ifdef JRD_MAIN
int             debug;
#else
extern int      debug;
#endif
#else
extern int      debug;
#endif
#endif /* REQUESTER */

/* Define the xxx_THREAD_DATA macors.  These are needed in the whole 
   component, but they are defined differently for use in jrd.c (JRD_MAIN)
   Here we have a function which sets some flags, and then calls THD_put_specific
   so in this case we define the macro as calling that function. */
#ifdef JRD_MAIN
#define SET_THREAD_DATA		tdbb = &thd_context;\
				MOVE_CLEAR (tdbb, sizeof (struct tdbb));\
				JRD_set_context (tdbb)
#define RESTORE_THREAD_DATA	JRD_restore_context()
#else
#define SET_THREAD_DATA		tdbb = &thd_context;\
				MOVE_CLEAR (tdbb, sizeof (*tdbb));\
				THD_put_specific (tdbb);\
				tdbb->tdbb_thd_data.thdd_type = THDD_TYPE_TDBB
#define RESTORE_THREAD_DATA	THD_restore_specific()
#endif /* JRD_MAIN */

#endif /* _JRD_JRD_H_ */
