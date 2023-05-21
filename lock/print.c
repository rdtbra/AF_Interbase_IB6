/*
 *      PROGRAM:        JRD Lock Manager
 *      MODULE:         print.c
 *      DESCRIPTION:    Lock Table printer
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
#include <stdlib.h>
#include <string.h>
#include "../jrd/common.h"
#include "../jrd/file_params.h"
#include "../jrd/jrd.h"
#include "../jrd/lck.h"
#include "../jrd/isc.h"
#include "../jrd/time.h"
#ifdef LINKS_EXIST
#include "../isc_lock/lock.h"
#else
#include "../lock/lock.h"
#endif /* LINKS_EXIST */
#include "../jrd/gdsassert.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/isc_s_proto.h"
#include "../lock/prtv3_proto.h"

#if (defined DELTA || defined sgi || defined ultrix)
#include <sys/types.h>
#endif
#include <sys/stat.h>
#if !(defined mpexl || defined WIN_NT || defined OS2_ONLY)
#include <sys/param.h>
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#include <winbase.h>
#include <io.h>
#include <process.h>
#endif

#ifdef NETWARE_386
typedef struct blk {
    UCHAR       blk_type;
    UCHAR       blk_pool_id_mod;
    USHORT      blk_length;
} *BLK;
#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"
#define exit(code)      {service->svc_handle = 0;                           \
									    \
		    /* Mark service thread as finished. */                  \
		    /* If service is detached, cleanup memory being used    \
		       by service. */                                       \
		    SVC_finish (service, SVC_finished);			    \
									    \
		    return (code);}                                         

#define FPRINTF         SVC_netware_fprintf
#endif

#ifndef FPRINTF
#define FPRINTF         ib_fprintf
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN      256
#endif

#ifdef NETWARE_386
typedef SVC OUTFILE;
#else
typedef IB_FILE *OUTFILE;
#endif

#define SW_I_ACQUIRE	1
#define SW_I_OPERATION	2
#define SW_I_TYPE	4
#define SW_I_WAIT	8

struct waitque {
    USHORT	waitque_depth;
    PTR		waitque_entry [30];
};
#define COUNT(x)	(sizeof (x)/ sizeof ((x)[0]))

static void	prt_lock_activity (OUTFILE, LHB, USHORT, USHORT, USHORT);
static void     prt_lock_init (void);
static void     prt_history (OUTFILE, LHB, PTR, SCHAR *);
static void     prt_lock (OUTFILE, LHB, LBL, USHORT);
static void     prt_owner (OUTFILE, LHB, OWN, BOOLEAN, BOOLEAN);
static void 	prt_owner_wait_cycle (OUTFILE, LHB, OWN, USHORT, struct waitque *);
static void     prt_request (OUTFILE, LHB, LRQ);
static void     prt_que (OUTFILE, LHB, SCHAR *, SRQ, USHORT);
static void     prt_que2 (OUTFILE, LHB, SCHAR *, SRQ, USHORT);

static CONST TEXT     history_names[][10] = {
	"n/a", "ENQ", "DEQ", "CONVERT", "SIGNAL", "POST", "WAIT",
	"DEL_PROC", "DEL_LOCK", "DEL_REQ", "DENY", "GRANT", "LEAVE",
	"SCAN", "DEAD", "ENTER", "BUG", "ACTIVE", "CLEANUP", "DEL_OWNER"
};

static CONST UCHAR	compatibility[] = {

/*				Shared	Prot	Shared	Prot
		none	null	 Read	Read	Write	Write	Exclusive */

/* none */	1,	  1,	   1,	  1,	  1,	  1,	  1,
/* null */	1,	  1,	   1,	  1,	  1,	  1,	  1,
/* SR */	1,	  1,	   1,     1,	  1,	  1,	  0,
/* PR */	1,	  1,	   1,	  1,	  0,	  0,	  0,
/* SW */	1,	  1,	   1,	  0,	  1,	  0,	  0,
/* PW */	1,	  1,	   1,	  0,	  0,	  0,	  0,
/* EX */	1,	  1,	   0,	  0,	  0,	  0,	  0 };

#define COMPATIBLE(st1, st2)	compatibility [st1 * LCK_max + st2]


#ifdef NETWARE_386

int main_lock_print (
    SVC service)

#else /* Non-Netware declaration */

int CLIB_ROUTINE main (
    int         argc,
    char        *argv[])

#endif
{
/**************************************
 *
 *      m a i n
 *
 **************************************
 *
 * Functional description
 *	Check switches passed in and prepare to dump the lock table
 *	to ib_stdout.
 *
 **************************************/
BOOLEAN         sw_requests, sw_owners, sw_locks, sw_history, sw_nobridge;
USHORT          sw_series, sw_interactive, sw_intervals, sw_seconds;
BOOLEAN		sw_consistency;
BOOLEAN		sw_waitlist;
BOOLEAN		sw_file;
LHB		LOCK_header, header = NULL;
SLONG           LOCK_size_mapped = DEFAULT_SIZE;
int             orig_argc;
SCHAR           **orig_argv;
TEXT            *lock_file, buffer [MAXPATHLEN];
TEXT            expanded_lock_filename [MAXPATHLEN];
TEXT            hostname [64];
USHORT          i;
SLONG           length;
SH_MEM_T        shmem_data;
SHB             shb;
SRQ             que, slot;
SCHAR           *p, c;
STATUS          status_vector [20];
SLONG           redir_in, redir_out, redir_err;
SLONG		hash_total_count, hash_lock_count, hash_min_count, hash_max_count;
float           bottleneck;
#ifdef NETWARE_386
int             argc;
char            **argv;
#endif
OUTFILE     	outfile;
#ifdef MANAGER_PROCESS
OWN		manager;
int		manager_pid;
#endif

#ifdef NETWARE_386
argc = service->svc_argc;
argv = service->svc_argv;
#endif

#ifdef NETWARE_386
outfile = service;
#else
outfile = ib_stdout;
#endif

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
#ifdef NETWARE_386
    /* Get the output file name from the next argv[] etc. */
#endif
    }
#ifndef NETWARE_386
else if (argc > 4 && !strcmp (argv [1], "-svc_re"))
    {
    redir_in = atol (argv [2]);
    redir_out = atol (argv [3]);
    redir_err = atol (argv [4]);
#ifdef WIN_NT
    redir_in = _open_osfhandle (redir_in, 0);
    redir_out = _open_osfhandle (redir_out, 0);
    redir_err = _open_osfhandle (redir_err, 0);
#endif
    if (redir_in != 0)
	if (dup2 ((int) redir_in, 0))
	    close ((int) redir_in);
    if (redir_out != 1)
	if (dup2 ((int) redir_out, 1))
	    close ((int) redir_out);
    if (redir_err != 2)
	if (dup2 ((int) redir_err, 2))
	    close ((int) redir_err);
    argv += 4;
    argc -= 4;
    }
#endif

orig_argc = argc;
orig_argv = argv;

/* Handle switches, etc. */

argv++;
sw_consistency = FALSE;
sw_waitlist = FALSE;
sw_file = FALSE;
sw_requests = sw_locks = sw_history = sw_nobridge = FALSE;
sw_owners = TRUE;
sw_series  = sw_interactive = sw_intervals = sw_seconds = 0;

while (--argc)
    {
    p = *argv++;
    if (*p++ != '-')
	{
	FPRINTF (outfile, "Valid switches are: -o, -p, -l, -r, -a, -h, -n, -s <n>, -c, -i <n> <n>\n");
	exit(FINI_OK);
	}
    while (c = *p++)
	switch (c)
	    {
	    case 'o':
	    case 'p':
		sw_owners = TRUE;
		break;

	    case 'c':
		sw_nobridge = TRUE;
		sw_consistency = TRUE;
		break;

	    case 'l':
		sw_locks = TRUE;
		break;

	    case 'r':
		sw_requests = TRUE;
		break;
	    
	    case 'a':
		sw_locks = TRUE;
		sw_owners = TRUE;
		sw_requests = TRUE;
		sw_history = TRUE;
		break;
	    
	    case 'h':
		sw_history = TRUE;
		break;

	    case 's':
		if (argc > 1)
		    sw_series = atoi (*argv++);
		if (!sw_series)
		    {
		    FPRINTF (outfile, "Please specify a positive value following option -s\n");
		    exit (FINI_OK);
		    }
		--argc;
		break;

	    case 'n':
		sw_nobridge = TRUE;
		break;

	    case 'i':
		while (c = *p++)
		    switch (c)
		    	{
		    	case 'a':
		    	    sw_interactive |= SW_I_ACQUIRE;
			    break;
			
		    	case 'o':
		       	    sw_interactive |= SW_I_OPERATION;
			    break;
			
		    	case 't':
		       	    sw_interactive |= SW_I_TYPE;
			    break;
			
		   	case 'w':
		       	    sw_interactive |= SW_I_WAIT;
			    break;
			
		    	default:
		       	    FPRINTF (outfile, "Valid interactive switches are: a, o, t, w\n");
			    exit (FINI_OK);
			    break;
		    	}
		if (!sw_interactive)
		    sw_interactive = (SW_I_ACQUIRE | SW_I_OPERATION | SW_I_TYPE | SW_I_WAIT);
		sw_nobridge = TRUE;
		sw_seconds = sw_intervals = 1;
		if (argc > 1)
		    {
		    sw_seconds = atoi (*argv++);
		    --argc;
		    if (argc > 1)
		    	{
		       	sw_intervals = atoi (*argv++);
			--argc;
			}
		    if (!(sw_seconds > 0) || (sw_intervals < 0))
		    	{
			FPRINTF (outfile, "Please specify 2 positive values for option -i\n");
			exit (FINI_OK);
			}
		    }
		--p;
		break;

	    case 'w':
		sw_nobridge = TRUE;
		sw_waitlist = TRUE;
		break;

	    case 'f':
		sw_nobridge = TRUE;
		sw_file = TRUE;
		if (argc > 1)
		    {
		    lock_file = *argv++;
		    --argc;
		    }
		else
		    {
		    FPRINTF (outfile, "Usage: -f <filename>\n");
		    exit (FINI_OK);
		    };
		break;

	    default:
		FPRINTF (outfile, "Valid switches are: -o, -p, -l, -r, -a, -h, -n, -s <n>, -c, -i <n> <n>\n");
		exit (FINI_OK);
		break;
	    }
    }

if (!sw_file)
    {
    gds__prefix_lock (buffer, LOCK_FILE);
    lock_file = buffer;
    }

#ifdef UNIX
shmem_data.sh_mem_semaphores = 0;
#endif

#ifdef UNIX
LOCK_size_mapped = 0;           /* Use length of existing segment */
#else
LOCK_size_mapped = DEFAULT_SIZE; /* length == 0 not supported by all non-UNIX */
#endif
LOCK_header = (LHB) ISC_map_file (status_vector,
	lock_file,
	prt_lock_init,
	NULL_PTR,
	-LOCK_size_mapped,      /* Negative to NOT truncate file */
	&shmem_data);

sprintf (expanded_lock_filename, lock_file, 
	ISC_get_host (hostname, sizeof (hostname)));

/* Make sure the lock file is valid - if it's a zero length file we 
 * can't look at the header without causing a BUS error by going
 * off the end of the mapped region.
 */

if (LOCK_header && shmem_data.sh_mem_length_mapped < sizeof (struct lhb))
    {
    /* Mapped file is obviously too small to really be a lock file */
    FPRINTF (outfile, "Unable to access lock table - file too small.\n%s\n",
	     expanded_lock_filename);
    exit (FINI_OK);
    }

if (LOCK_header && LOCK_header->lhb_length > shmem_data.sh_mem_length_mapped)
    {
    length = LOCK_header->lhb_length;
#if (!(defined UNIX) || (defined MMAP_SUPPORTED))
    LOCK_header = (LHB) ISC_remap_file (status_vector, &shmem_data, length, FALSE);
#endif
    }

if (!LOCK_header)
    {
    FPRINTF (outfile, "Unable to access lock table.\n%s\n", 
	     expanded_lock_filename);
    gds__print_status (status_vector);
    exit (FINI_OK);
    }

LOCK_size_mapped = shmem_data.sh_mem_length_mapped;

/* if we can't read this version - admit there's nothing to say and return. */

if ((LOCK_header->lhb_version != SS_LHB_VERSION) &&
   (LOCK_header->lhb_version != CLASSIC_LHB_VERSION))
    {
    FPRINTF (outfile, "\tUnable to read lock table version %d.\n",
		LOCK_header->lhb_version);
    exit (FINI_OK);
    }

/* Print lock activity report */

if (sw_interactive)
    {
    prt_lock_activity (outfile, LOCK_header, sw_interactive, sw_seconds, sw_intervals);
    exit (FINI_OK);
    }

#ifdef NETWARE_386
/* For Netware SuperServer - we will have other threads munging the
 * lock table while we are trying to print it over the slow remote
 * link.  Therefore, ALWAYS insist on a consistent copy before doing
 * the dump.
 */
sw_consistency = TRUE;
#endif

if (sw_consistency)
    {
    /* To avoid changes in the lock file while we are dumping it - make
     * a local buffer, lock the lock file, copy it, then unlock the
     * lock file to let processing continue.  Printing of the lock file
     * will continue from the in-memory copy.
     */
    header = (LHB) gds__alloc (LOCK_header->lhb_length);
    if (!header)
	{
	FPRINTF (outfile, "Insufficient memory for consistent lock statistics.\n");
#ifndef NETWARE_386
	FPRINTF (outfile, "Try omitting the -c switch.\n");
#endif
	exit (FINI_OK);
    }

    ISC_mutex_lock (LOCK_header->lhb_mutex);
    memcpy (header, LOCK_header, LOCK_header->lhb_length);
    ISC_mutex_unlock (LOCK_header->lhb_mutex);
    LOCK_header = header;
    }

/* Print lock header block */

FPRINTF (outfile, "LOCK_HEADER BLOCK\n");
FPRINTF (outfile, "\tVersion: %d, Active owner: %6d, Length: %6d, Used: %6d\n",
	 LOCK_header->lhb_version, LOCK_header->lhb_active_owner, 
	 LOCK_header->lhb_length, LOCK_header->lhb_used);

#ifdef MANAGER_PROCESS
manager_pid = 0;
if (LOCK_header->lhb_manager)
    {
    manager = (OWN) ABS_PTR (LOCK_header->lhb_manager);
    manager_pid = manager->own_process_id;
    }

FPRINTF (outfile, "\tLock manager pid: %6d\n", manager_pid);
#endif

FPRINTF (outfile, "\tSemmask: 0x%X, Flags: 0x%04X\n", 
	 LOCK_header->lhb_mask, LOCK_header->lhb_flags);

FPRINTF (outfile, "\tEnqs: %6d, Converts: %6d, Rejects: %6d, Blocks: %6d\n",
	 LOCK_header->lhb_enqs, LOCK_header->lhb_converts,
	 LOCK_header->lhb_denies, LOCK_header->lhb_blocks);

FPRINTF (outfile, "\tDeadlock scans: %6d, Deadlocks: %6d, Scan interval: %3d\n",
	 LOCK_header->lhb_scans, LOCK_header->lhb_deadlocks, LOCK_header->lhb_scan_interval);

FPRINTF (outfile, "\tAcquires: %6d, Acquire blocks: %6d, Spin count: %3d\n",
	LOCK_header->lhb_acquires, LOCK_header->lhb_acquire_blocks, LOCK_header->lhb_acquire_spins);

if (LOCK_header->lhb_acquire_blocks)
    {
    bottleneck = (float) ((100. * LOCK_header->lhb_acquire_blocks) / LOCK_header->lhb_acquires);
    FPRINTF (outfile, "\tMutex wait: %3.1f%%\n", bottleneck);
    }
else
    FPRINTF (outfile, "\tMutex wait: 0.0%%\n");

hash_total_count = hash_max_count = 0;
hash_min_count = 10000000;
for (slot = LOCK_header->lhb_hash, i = 0; i < LOCK_header->lhb_hash_slots; slot++, i++)
    {
    hash_lock_count = 0;
    for (que = (SRQ) ABS_PTR (slot->srq_forward); que != slot;
	que = (SRQ) ABS_PTR (que->srq_forward))
    	{
	++hash_total_count;
	++hash_lock_count;
	}
    if (hash_lock_count < hash_min_count)
       	hash_min_count = hash_lock_count;
    if (hash_lock_count > hash_max_count)
        hash_max_count = hash_lock_count;
    }

FPRINTF (outfile, "\tHash slots: %4d, ", LOCK_header->lhb_hash_slots);

FPRINTF (outfile, "Hash lengths (min/avg/max): %4d/%4d/%4d\n",
	 hash_min_count, (hash_total_count / LOCK_header->lhb_hash_slots), hash_max_count);

if (LOCK_header->lhb_secondary != LHB_PATTERN)
    {
    shb = (SHB) ABS_PTR (LOCK_header->lhb_secondary);
    FPRINTF (outfile, "\tRemove node: %6d, Insert queue: %6d, Insert prior: %6d\n",
	     shb->shb_remove_node, shb->shb_insert_que, shb->shb_insert_prior);
    }

prt_que (outfile, LOCK_header, "\tOwners", &LOCK_header->lhb_owners, 
	 OFFSET (OWN, own_lhb_owners));
prt_que (outfile, LOCK_header, "\tFree owners", &LOCK_header->lhb_free_owners,
	 OFFSET (OWN, own_lhb_owners));
prt_que (outfile, LOCK_header, "\tFree locks", &LOCK_header->lhb_free_locks,
	 OFFSET (LBL, lbl_lhb_hash));
prt_que (outfile, LOCK_header, "\tFree requests", &LOCK_header->lhb_free_requests,
	 OFFSET (LRQ, lrq_lbl_requests));

/* Print lock ordering option */

FPRINTF (outfile, "\tLock Ordering: %s\n", 
	 (LOCK_header->lhb_flags & LHB_lock_ordering) ? "Enabled" : "Disabled");
FPRINTF (outfile, "\n");
    
/* Print known owners */

if (sw_owners)
    {
#ifdef SOLARIS_MT
    /* The Lock Starvation recovery code on Solaris rotates the owner
       queue once per acquire.  This makes it difficult to read the
       printouts when multiple runs are made.  So we scan the list
       of owners once to find the lowest owner_id, and start the printout
       from there. */
    PTR	least_owner_id = 0x7FFFFFFF;
    SRQ least_owner_ptr = &LOCK_header->lhb_owners;
    QUE_LOOP (LOCK_header->lhb_owners, que)
	{
	OWN	this_owner;
	this_owner = (OWN)((UCHAR*) que - OFFSET (OWN, own_lhb_owners)); 
	if (REL_PTR (this_owner) < least_owner_id)
	    {
	    least_owner_id = REL_PTR (this_owner);
	    least_owner_ptr = que;
	    }
	}
    que = least_owner_ptr;
    do 
	{
	if (que != &LOCK_header->lhb_owners)
	    prt_owner (outfile, LOCK_header,
		   (OWN)((UCHAR*) que - OFFSET (OWN, own_lhb_owners)), 
		   sw_requests, sw_waitlist);
	que = QUE_NEXT ((*que));
	}
    while (que != least_owner_ptr);
#else
    QUE_LOOP (LOCK_header->lhb_owners, que)
	{
	prt_owner (outfile, LOCK_header,
		   (OWN)((UCHAR*) que - OFFSET (OWN, own_lhb_owners)), 
		   sw_requests, sw_waitlist);
	}
#endif /* SOLARIS_MT */
    }

/* Print known locks */

if (sw_locks || sw_series)
    for (slot = LOCK_header->lhb_hash, i = 0; i < LOCK_header->lhb_hash_slots; 
	    slot++, i++)
	for (que = (SRQ) ABS_PTR (slot->srq_forward); que != slot;
	     que = (SRQ) ABS_PTR (que->srq_forward))
	    prt_lock (outfile, LOCK_header, 
		      (LBL)((UCHAR*) que - OFFSET (LBL, lbl_lhb_hash)), 
		      sw_series);
    
if (sw_history)
    prt_history (outfile, LOCK_header, LOCK_header->lhb_history, "History");

if (LOCK_header->lhb_secondary != LHB_PATTERN)
    prt_history (outfile, LOCK_header, shb->shb_history, "Event log");

#if !(defined WIN_NT || defined OS2_ONLY || defined NETWARE_386)
if (!sw_nobridge)
    {
    FPRINTF (outfile, "\nBRIDGE RESOURCES\n\n");
    V3_lock_print (orig_argc, orig_argv);
    }
#endif

if (header)
    gds__free (header);
exit (FINI_OK);
}

static void prt_lock_activity (
    OUTFILE	outfile,
    LHB		header,
    USHORT	flag,
    USHORT	seconds,
    USHORT	intervals)
{
/**************************************
 *
 *	p r t _ l o c k _ a c t i v i t y
 *
 **************************************
 *
 * Functional description
 *	Print a time-series lock activity report 
 *
 **************************************/
struct tm d;
ULONG	clock, i;
struct lhb	base, prior;
ULONG	factor;

clock = time (NULL);
d = *localtime (&clock);

FPRINTF (outfile, "%02d:%02d:%02d ", d.tm_hour, d.tm_min, d.tm_sec);

if (flag & SW_I_ACQUIRE)
    FPRINTF (outfile, "acquire/s acqwait/s  %%acqwait acqrtry/s rtrysuc/s ");
	     
if (flag & SW_I_OPERATION)
    FPRINTF (outfile, "enqueue/s convert/s downgrd/s dequeue/s readata/s wrtdata/s qrydata/s ");

if (flag & SW_I_TYPE)
    FPRINTF (outfile, "  dblop/s  rellop/s pagelop/s tranlop/s relxlop/s idxxlop/s misclop/s ");

if (flag & SW_I_WAIT)
    {
    FPRINTF (outfile, "   wait/s  reject/s timeout/s blckast/s  dirsig/s  indsig/s ");
    FPRINTF (outfile, " wakeup/s dlkscan/s deadlck/s avlckwait(msec)");
    }

FPRINTF (outfile, "\n");

base = *header;
prior = *header;
if (intervals == 0)
    memset (&base, 0, sizeof (base));

for (i = 0; i < intervals; i++)
    {
#ifdef WIN_NT
    Sleep ((DWORD) seconds * 1000);
#else
#ifdef NETWARE_386
    delay (seconds * 1000);
#else
    sleep (seconds);
#endif
#endif
    clock = time (NULL);
    d = *localtime (&clock);
    
    FPRINTF (outfile, "%02d:%02d:%02d ", d.tm_hour, d.tm_min, d.tm_sec);

    if (flag & SW_I_ACQUIRE)
    	{
    	FPRINTF (outfile, "%9d %9d %9d %9d %9d ",
    	    (header->lhb_acquires - prior.lhb_acquires) / seconds,
    	    (header->lhb_acquire_blocks - prior.lhb_acquire_blocks) / seconds,
    	    (header->lhb_acquires - prior.lhb_acquires) ?
		(100 * (header->lhb_acquire_blocks - prior.lhb_acquire_blocks)) /
		(header->lhb_acquires - prior.lhb_acquires) : 0,
	    (header->lhb_acquire_retries - prior.lhb_acquire_retries) / seconds,
    	    (header->lhb_retry_success - prior.lhb_retry_success) / seconds);

	prior.lhb_acquires = header->lhb_acquires;
	prior.lhb_acquire_blocks = header->lhb_acquire_blocks;
	prior.lhb_acquire_retries = header->lhb_acquire_retries;
	prior.lhb_retry_success = header->lhb_retry_success;
	}
    
    if (flag & SW_I_OPERATION)
    	{
    	FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d ",
	    (header->lhb_enqs - prior.lhb_enqs) / seconds,
    	    (header->lhb_converts - prior.lhb_converts) / seconds,
	    (header->lhb_downgrades - prior.lhb_downgrades) / seconds,
    	    (header->lhb_deqs - prior.lhb_deqs) / seconds,
    	    (header->lhb_read_data - prior.lhb_read_data) / seconds,
    	    (header->lhb_write_data - prior.lhb_write_data) / seconds,
    	    (header->lhb_query_data - prior.lhb_query_data) / seconds);
	
	prior.lhb_enqs = header->lhb_enqs;
	prior.lhb_converts = header->lhb_converts;
	prior.lhb_downgrades = header->lhb_downgrades;
	prior.lhb_deqs = header->lhb_deqs;
	prior.lhb_read_data = header->lhb_read_data;
	prior.lhb_write_data = header->lhb_write_data;
	prior.lhb_query_data = header->lhb_query_data;
	}
    
    if (flag & SW_I_TYPE)
    	{
    	FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d ",
	    (header->lhb_operations [LCK_database] - prior.lhb_operations [LCK_database]) / seconds,
	    (header->lhb_operations [LCK_relation] - prior.lhb_operations [LCK_relation]) / seconds,
	    (header->lhb_operations [LCK_bdb] - prior.lhb_operations [LCK_bdb]) / seconds,
	    (header->lhb_operations [LCK_tra] - prior.lhb_operations [LCK_tra]) / seconds,
	    (header->lhb_operations [LCK_rel_exist] - prior.lhb_operations [LCK_rel_exist]) / seconds,
	    (header->lhb_operations [LCK_idx_exist] - prior.lhb_operations [LCK_idx_exist]) / seconds,
	    (header->lhb_operations [0] - prior.lhb_operations [0]) / seconds);
	
	prior.lhb_operations [LCK_database] = header->lhb_operations [LCK_database];
	prior.lhb_operations [LCK_relation] = header->lhb_operations [LCK_relation];
	prior.lhb_operations [LCK_bdb] = header->lhb_operations [LCK_bdb];
	prior.lhb_operations [LCK_tra] = header->lhb_operations [LCK_tra];
	prior.lhb_operations [LCK_rel_exist] = header->lhb_operations [LCK_rel_exist];
	prior.lhb_operations [LCK_idx_exist] = header->lhb_operations [LCK_idx_exist];
	prior.lhb_operations [0] = header->lhb_operations [0];
	}
    
    if (flag & SW_I_WAIT)
    	{
    	FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d %9d %9d %9d ",
	    (header->lhb_waits - prior.lhb_waits) / seconds,
    	    (header->lhb_denies - prior.lhb_denies) / seconds,
	    (header->lhb_timeouts - prior.lhb_timeouts) / seconds,
    	    (header->lhb_blocks - prior.lhb_blocks) / seconds,
	    (header->lhb_direct_sigs - prior.lhb_direct_sigs) / seconds,
	    (header->lhb_indirect_sigs - prior.lhb_indirect_sigs) / seconds,
	    (header->lhb_wakeups - prior.lhb_wakeups) / seconds,
    	    (header->lhb_scans - prior.lhb_scans) / seconds,
    	    (header->lhb_deadlocks - prior.lhb_deadlocks) / seconds,
	    (header->lhb_waits - prior.lhb_waits) ?
	       (header->lhb_wait_time - prior.lhb_wait_time) / (header->lhb_waits - prior.lhb_waits) : 0);
	
	prior.lhb_waits = header->lhb_waits;
    	prior.lhb_denies = header->lhb_denies;
	prior.lhb_timeouts = header->lhb_timeouts;
    	prior.lhb_blocks = header->lhb_blocks;
	prior.lhb_direct_sigs = header->lhb_direct_sigs;
	prior.lhb_indirect_sigs = header->lhb_indirect_sigs;
	prior.lhb_wakeups = header->lhb_wakeups;
    	prior.lhb_scans = header->lhb_scans;
    	prior.lhb_deadlocks = header->lhb_deadlocks;
	prior.lhb_wait_time = header->lhb_wait_time;
	}
    
    FPRINTF (outfile, "\n");
    }

factor = seconds * intervals;
if (factor < 1)
    factor = 1;

FPRINTF (outfile, "\nAverage: ");
if (flag & SW_I_ACQUIRE)
    {
    FPRINTF (outfile, "%9d %9d %9d %9d %9d ",
    	(header->lhb_acquires - base.lhb_acquires) / (factor),
    	(header->lhb_acquire_blocks - base.lhb_acquire_blocks) / (factor),
    	(header->lhb_acquires - base.lhb_acquires) ?
	    (100 * (header->lhb_acquire_blocks - base.lhb_acquire_blocks)) /
	    (header->lhb_acquires - base.lhb_acquires) : 0,
	(header->lhb_acquire_retries - base.lhb_acquire_retries) / (factor),
    	(header->lhb_retry_success - base.lhb_retry_success) / (factor));
    }
    
if (flag & SW_I_OPERATION)
    {
    FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d ",
	(header->lhb_enqs - base.lhb_enqs) / (factor),
    	(header->lhb_converts - base.lhb_converts) / (factor),
	(header->lhb_downgrades - base.lhb_downgrades) / (factor),
    	(header->lhb_deqs - base.lhb_deqs) / (factor),
    	(header->lhb_read_data - base.lhb_read_data) / (factor),
    	(header->lhb_write_data - base.lhb_write_data) / (factor),
    	(header->lhb_query_data - base.lhb_query_data) / (factor));
    }
    
if (flag & SW_I_TYPE)
    {
    FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d ",
	(header->lhb_operations [LCK_database] - base.lhb_operations [LCK_database]) / (factor),
	(header->lhb_operations [LCK_relation] - base.lhb_operations [LCK_relation]) / (factor),
	(header->lhb_operations [LCK_bdb] - base.lhb_operations [LCK_bdb]) / (factor),
	(header->lhb_operations [LCK_tra] - base.lhb_operations [LCK_tra]) / (factor),
	(header->lhb_operations [LCK_rel_exist] - base.lhb_operations [LCK_rel_exist]) / (factor),
	(header->lhb_operations [LCK_idx_exist] - base.lhb_operations [LCK_idx_exist]) / (factor),
	(header->lhb_operations [0] - base.lhb_operations [0]) / (factor));
    }
    
if (flag & SW_I_WAIT)
    {
    FPRINTF (outfile, "%9d %9d %9d %9d %9d %9d %9d %9d %9d %9d ",
	(header->lhb_waits - base.lhb_waits) / (factor),
    	(header->lhb_denies - base.lhb_denies) / (factor),
	(header->lhb_timeouts - base.lhb_timeouts) / (factor),
    	(header->lhb_blocks - base.lhb_blocks) / (factor),
	(header->lhb_direct_sigs - base.lhb_direct_sigs) / (factor),
	(header->lhb_indirect_sigs - base.lhb_indirect_sigs) / (factor),
	(header->lhb_wakeups - base.lhb_wakeups) / (factor),
    	(header->lhb_scans - base.lhb_scans) / (factor),
    	(header->lhb_deadlocks - base.lhb_deadlocks) / (factor),
	(header->lhb_waits - base.lhb_waits) ?
	   (header->lhb_wait_time - base.lhb_wait_time) / (header->lhb_waits - base.lhb_waits) : 0);
    }

FPRINTF (outfile, "\n");
}

static void prt_lock_init (void)
{
/**************************************
 *
 *      l o c k _ i n i t
 *
 **************************************
 *
 * Functional description
 *      Initialize a lock table to looking -- i.e. don't do
 *      nuthin.
 *
 **************************************/
}

static void prt_history (
    OUTFILE outfile,
    LHB         LOCK_header,
    PTR         history_header,
    SCHAR       *title)
{
/**************************************
 *
 *      p r t _ h i s t o r y
 *
 **************************************
 *
 * Functional description
 *      Print history list of lock table.
 *
 **************************************/
HIS     history;

FPRINTF (outfile, "%s:\n", title);

for (history = (HIS) ABS_PTR (history_header); TRUE;
     history = (HIS) ABS_PTR (history->his_next))
    {
    if (history->his_operation)
	FPRINTF (outfile, "    %s:\towner = %6d, lock = %6d, request = %6d\n",
	    history_names [history->his_operation], 
	    history->his_process, history->his_lock, 
	    history->his_request);
    if (history->his_next == history_header)
	break;
    }
}

static void prt_lock (
    OUTFILE outfile,
    LHB         LOCK_header,
    LBL         lock,
    USHORT      sw_series)
{
/**************************************
 *
 *      p r t _ l o c k
 *
 **************************************
 *
 * Functional description
 *      Print a formatted lock block 
 *
 **************************************/
SRQ     que;
LRQ     request;

if (sw_series && lock->lbl_series != sw_series)
    return;

FPRINTF (outfile, "LOCK BLOCK %6d\n", REL_PTR (lock));
FPRINTF (outfile, "\tSeries: %d, Parent: %6d, State: %d, size: %d length: %d data: %d\n",
	lock->lbl_series, lock->lbl_parent, lock->lbl_state,
	lock->lbl_size, lock->lbl_length, lock->lbl_data);

if (lock->lbl_length == 4)
    {
    SLONG   key;
    UCHAR   *p, *q, *end;
    p = (UCHAR*) &key;
    q = lock->lbl_key;
    for (end = q + 4; q < end; q++)
	*p++ = *q;
    FPRINTF (outfile, "\tKey: %06u,", key);
    }
else
    {
    UCHAR   c, *p, *q, *end, temp [512];
    q = lock->lbl_key;
    for (p = temp, end = q + lock->lbl_length; q < end; q++)
	{
	c = *q;
	if ((c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') ||
	    c == '/')
	    *p++ = c;
	else
	    {
	    sprintf (p, "<%d>", c);
	    while (*p)
		p++;
	    }
	}
    *p = 0;
    FPRINTF (outfile, "\tKey: %s,", temp);
    }

FPRINTF (outfile, " Flags: 0x%02X, Pending request count: %6d\n", 
	lock->lbl_flags, lock->lbl_pending_lrq_count);

prt_que (outfile, LOCK_header, "\tHash que", &lock->lbl_lhb_hash,
	OFFSET (LBL, lbl_lhb_hash));

prt_que (outfile, LOCK_header, "\tRequests", &lock->lbl_requests,
	OFFSET (LRQ, lrq_lbl_requests));

QUE_LOOP (lock->lbl_requests, que)
    {
    request = (LRQ) ((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
    FPRINTF (outfile, "\t\tRequest %6d, Owner: %6d, State: %d (%d), Flags: 0x%02X\n",
	REL_PTR (request), request->lrq_owner, request->lrq_state,
	request->lrq_requested, request->lrq_flags);
    }

FPRINTF (outfile, "\n");
}

static void prt_owner (
    OUTFILE 	outfile,
    LHB         LOCK_header,
    OWN         owner,
    BOOLEAN     sw_requests,
    BOOLEAN     sw_waitlist)
{
/**************************************
 *
 *      p r t _ o w n e r
 *
 **************************************
 *
 * Functional description
 *      Print a formatted owner block.
 *
 **************************************/
SRQ     que;

FPRINTF (outfile, "OWNER BLOCK %6d\n", REL_PTR (owner));
#ifndef mpexl
FPRINTF (outfile, "\tOwner id: %6d, type: %1d, flags: 0x%02X, pending: %6d, semid: %6d ", 
    owner->own_owner_id, owner->own_owner_type, 
    (owner->own_flags | (UCHAR) owner->own_ast_flags), owner->own_pending_request,
    owner->own_semaphore & ~OWN_semavail);
if (owner->own_semaphore & OWN_semavail)
    FPRINTF (outfile, "(available)\n");
else
    FPRINTF (outfile, "\n");

FPRINTF (outfile, "\tProcess id: %6d, UID: 0x%X  %s\n",
    owner->own_process_id,
    owner->own_process_uid,
    ISC_check_process_existence (owner->own_process_id, owner->own_process_uid, FALSE) ? "Alive" : "Dead");
#ifdef SOLARIS_MT
FPRINTF (outfile, "\tLast Acquire: %6d (%6d ago)",
    owner->own_acquire_time, 
    LOCK_header->lhb_acquires - owner->own_acquire_time);
#ifdef DEV_BUILD
FPRINTF (outfile, " sec %9ld (%3d sec ago)",
    owner->own_acquire_realtime,
    time (NULL) - owner->own_acquire_realtime);
#endif /* DEV_BUILD */
FPRINTF (outfile, "\n");
#endif /* SOLARIS_MT */
{
UCHAR	tmp;
tmp = (owner->own_flags | (UCHAR) owner->own_ast_flags 
	| (UCHAR) owner->own_ast_hung_flags );
FPRINTF (outfile, "\tFlags: 0x%02X ", tmp);
FPRINTF (outfile, " %s", (tmp & OWN_hung)    ? "hung" : "    ");
FPRINTF (outfile, " %s", (tmp & OWN_blocking)? "blkg" : "    ");
FPRINTF (outfile, " %s", (tmp & OWN_starved) ? "STRV" : "    ");
FPRINTF (outfile, " %s", (tmp & OWN_signal)  ? "sgnl" : "    ");
FPRINTF (outfile, " %s", (tmp & OWN_wakeup)  ? "wake" : "    ");
FPRINTF (outfile, " %s", (tmp & OWN_scanned) ? "scan" : "    ");
FPRINTF (outfile, "\n");
}
#else  /* mpexl */
FPRINTF (outfile, "\tProcess id: %6d.%6d flags: 0x%02X, pending: %6d\n",
    owner->own_process_id, owner->own_process_uid,
    (owner->own_flags | (UCHAR) owner->own_ast_flags), owner->own_pending_request);
FPRINTF (outfile, "\tAsync port: %6d, Sync port: %6d\n",
    owner->own_mpexl_async_port, owner->own_mpexl_sync_port);
#endif /* mpexl */

prt_que (outfile, LOCK_header, "\tRequests", &owner->own_requests,
	OFFSET (LRQ, lrq_own_requests));
prt_que (outfile, LOCK_header, "\tBlocks", &owner->own_blocks,
	OFFSET (LRQ, lrq_own_blocks));

if (sw_waitlist)
    {
    struct waitque owner_list;
    owner_list.waitque_depth = 0;
    prt_owner_wait_cycle (outfile, LOCK_header, owner, 8, &owner_list);
    }

FPRINTF (outfile, "\n");

if (sw_requests)
    QUE_LOOP (owner->own_requests, que)
	prt_request (outfile, LOCK_header, 
		     (LRQ)((UCHAR*) que - OFFSET (LRQ, lrq_own_requests)));
}

static void prt_owner_wait_cycle (
    OUTFILE 		outfile,
    LHB        		LOCK_header,
    OWN         	owner,
    USHORT		indent,
    struct waitque	*waiters)
{
/**************************************
 *
 *      p r t _ o w n e r _ w a i t _ c y c l e
 *
 **************************************
 *
 * Functional description
 *	For the given owner, print out the list of owners
 *	being waited on.  The printout is recursive, up to
 *	a limit.  It is recommended this be used with 
 *	the -c consistency mode.
 *
 **************************************/
USHORT	i;

for (i = indent; i; i--)
    FPRINTF (outfile, " ");

/* Check to see if we're in a cycle of owners - this might be
   a deadlock, or might not, if the owners haven't processed
   their blocking queues */

for (i = 0; i < waiters->waitque_depth; i++)
    if (REL_PTR (owner) == waiters->waitque_entry [i])
	{
	FPRINTF (outfile, "%6d (potential deadlock).\n", REL_PTR (owner));
	return;
	};

FPRINTF (outfile, "%6d waits on ", REL_PTR (owner));

if (!owner->own_pending_request)
    FPRINTF (outfile, "nothing.\n");
else
    {
    SRQ	que;
    LRQ  owner_request;
    LBL	lock;
    USHORT counter;
    BOOLEAN owner_conversion;

    if (waiters->waitque_depth > COUNT (waiters->waitque_entry))
	{
	FPRINTF (outfile, "Dependency too deep\n");
	return;
	};

   waiters->waitque_entry [waiters->waitque_depth++] = REL_PTR (owner);

   FPRINTF (outfile, "\n");
   owner_request = ABS_PTR (owner->own_pending_request);
   assert (owner_request->lrq_type == type_lrq);
   owner_conversion = (owner_request->lrq_state > LCK_null) ? TRUE : FALSE;

   lock = ABS_PTR (owner_request->lrq_lock);
   assert (lock->lbl_type == type_lbl);

   counter = 0;
   QUE_LOOP (lock->lbl_requests, que)
	{
	OWN	lock_owner;
	LRQ     lock_request;

	if (counter++ > 50)
	    {
	    for (i = indent+6; i; i--)
		FPRINTF (outfile, " ");
	    FPRINTF (outfile, "printout stopped after %d owners\n", counter-1);
	    break;
	    }

	lock_request = (LRQ)((UCHAR*) que - OFFSET (LRQ, lrq_lbl_requests));
	assert (lock_request->lrq_type == type_lrq);


	if (LOCK_header->lhb_flags & LHB_lock_ordering && !owner_conversion)
            {

	    /* Requests AFTER our request can't block us */
	    if (owner_request == lock_request)
		break;

	    if (COMPATIBLE (owner_request->lrq_requested, 
			MAX (lock_request->lrq_state, lock_request->lrq_requested)))
		continue;
	    }
	else
	    {

	    /* Requests AFTER our request CAN block us */
	    if (lock_request == owner_request)
		continue;

	    if (COMPATIBLE (owner_request->lrq_requested, lock_request->lrq_state))
		continue;
	    };
	lock_owner = ABS_PTR (lock_request->lrq_owner);
	prt_owner_wait_cycle (outfile, LOCK_header, lock_owner, indent+4, waiters);
	}
   waiters->waitque_depth--;
   }
}

static void prt_request (
    OUTFILE outfile,
    LHB         LOCK_header,
    LRQ         request)
{
/**************************************
 *
 *      p r t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *      Print a format request block.
 *
 **************************************/

FPRINTF (outfile, "REQUEST BLOCK %6d\n", REL_PTR (request));
FPRINTF (outfile, "\tOwner: %6d, Lock: %6d, State: %d, Mode: %d, Flags: 0x%02X\n",
	request->lrq_owner, request->lrq_lock, request->lrq_state,
	request->lrq_requested, request->lrq_flags);
FPRINTF (outfile, "\tAST: 0x%X, argument: 0x%X\n", request->lrq_ast_routine,
	request->lrq_ast_argument);
prt_que2 (outfile, LOCK_header, "\tlrq_own_requests", &request->lrq_own_requests, OFFSET (LRQ, lrq_own_requests)); 
prt_que2 (outfile, LOCK_header, "\tlrq_lbl_requests", &request->lrq_lbl_requests, OFFSET (LRQ, lrq_lbl_requests)); 
prt_que2 (outfile, LOCK_header, "\tlrq_own_blocks  ", &request->lrq_own_blocks, OFFSET (LRQ, lrq_own_blocks)); 
FPRINTF (outfile, "\n");
}

static void prt_que (
    OUTFILE outfile,
    LHB         LOCK_header,
    SCHAR       *string,
    SRQ         que,
    USHORT      que_offset)
{
/**************************************
 *
 *      p r t _ q u e
 *
 **************************************
 *
 * Functional description
 *      Print the contents of a self-relative que.
 *
 **************************************/
SLONG   count, offset;
SRQ     next;

offset = REL_PTR (que);

if (offset == que->srq_forward && offset == que->srq_backward)
    {
    FPRINTF (outfile, "%s: *empty*\n", string);
    return;
    }

count = 0;

QUE_LOOP ((*que), next)
    ++count;

FPRINTF (outfile, "%s (%ld):\tforward: %6d, backward: %6d\n", 
    string, count, 
    que->srq_forward - que_offset, 
    que->srq_backward - que_offset);
}

static void prt_que2 (
    OUTFILE outfile,
    LHB         LOCK_header,
    SCHAR       *string,
    SRQ         que,
    USHORT      que_offset)
{
/**************************************
 *
 *      p r t _ q u e 2
 *
 **************************************
 *
 * Functional description
 *      Print the contents of a self-relative que.
 *      But don't try to count the entries, as they might be invalid
 *
 **************************************/
SLONG   offset;

offset = REL_PTR (que);

if (offset == que->srq_forward && offset == que->srq_backward)
    {
    FPRINTF (outfile, "%s: *empty*\n", string);
    return;
    }

FPRINTF (outfile, "%s:\tforward: %6d, backward: %6d\n", 
    string,
    que->srq_forward - que_offset, 
    que->srq_backward - que_offset);
}
