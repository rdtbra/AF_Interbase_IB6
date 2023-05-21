/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dba.e
 *	DESCRIPTION:	Database analysis tool
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
#include <errno.h>
#include <string.h>
#include "../jrd/ibsetjmp.h"
#include "../jrd/common.h"
#include "../jrd/time.h"
#include "../jrd/gds.h"
#include "../jrd/ods.h"
#include "../jrd/license.h"
#include "../utilities/ppg_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/thd.h"
#include "../jrd/enc_proto.h"

#ifdef APOLLO
#include "/sys/ins/base.ins.c"
#include "/sys/ins/ms.ins.c"
#include "/sys/ins/error.ins.c"
#endif

#ifdef OS2_ONLY
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#include <os2.h>
#include <io.h>
#endif

#ifdef WIN_NT
#include <io.h>
#include <windows.h>
#include "../jrd/pwd.h" 
#define TEXT		SCHAR
#endif

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"
#endif

/* For Netware the follow DB handle and isc_status is #defined to be a  */
/* local variable on the stack in main.  This is to avoid multiple      */
/* threading problems with module level statics.                        */
DATABASE DB = STATIC "/gds/dev_nt/utilities/yachts.lnk";
#define DB          db_handle
#define isc_status  status_vector

#define ALLOC(size)	alloc ((SLONG) size);
#define BUCKETS		5
#define WINDOW_SIZE	(1 << 17)

#if (defined WIN_NT || defined OS2_ONLY)
#include <stdlib.h>
#else
extern SCHAR	*sys_errlist[];
#endif

typedef struct rel {
    struct rel	*rel_next;
    struct idx	*rel_indexes;
    SLONG	rel_index_root;
    SLONG	rel_pointer_page;
    SLONG	rel_slots;
    SLONG	rel_data_pages;
    SLONG	rel_fill_distribution [BUCKETS];
    ULONG	rel_total_space;
    SSHORT	rel_id;
    SCHAR	rel_name [32];
} *REL;

typedef struct idx {
    struct idx	*idx_next;
    SSHORT	idx_id;
    SSHORT	idx_depth;
    SLONG	idx_leaf_buckets;
    SLONG	idx_total_duplicates;
    SLONG	idx_max_duplicates;
    SLONG	idx_nodes;
    SLONG	idx_data_length;
    SLONG	idx_fill_distribution [BUCKETS];
    SCHAR	idx_name [32];
} *IDX;

/* kidnapped from jrd/pio.h and abused */

typedef struct fil {
    struct fil	*fil_next;		/* Next file in database */
    ULONG	fil_min_page;		/* Minimum page number in file */
    ULONG	fil_max_page;		/* Maximum page number in file */
    USHORT	fil_fudge;		/* Fudge factor for page relocation */
#ifdef APOLLO
    SCHAR	*fil_region;
    int		fil_base;
#endif
#ifdef OS2_ONLY
    SLONG	fil_desc;
#endif
#ifdef WIN_NT
    void	*fil_desc;
#endif
#if !(defined APOLLO || defined OS2_ONLY || defined WIN_NT)
    int		fil_desc;
#endif
    USHORT	fil_length;		/* Length of expanded file name */
    SCHAR	fil_string [1];		/* Expanded file name */
} *FIL;

static SCHAR	*alloc (SLONG);
static void	analyze_data (REL);
static BOOLEAN	analyze_data_page (REL, DPG);
static void	analyze_index (REL, IDX);
#ifdef APOLLO
static void	db_error (status_$t);
#else
#if (defined WIN_NT || defined mpexl || defined OS2_ONLY)
static void	db_error (SLONG);
#else
static void	db_error (int);
#endif
#endif
static FIL	db_open (UCHAR *, USHORT);
static PAG	db_read (SLONG);
static BOOLEAN	key_equality (SSHORT, SCHAR *, BTN);
static void	move (SCHAR *, SCHAR *, SSHORT);
static void	print_distribution (SCHAR *, SLONG *);
static void	print_header (HDR);
static void	truncate_name (SCHAR *);

#ifdef APOLLO
static int	map_length, map_count;
#endif

static SCHAR	*months [] = {
	"Jan", "Feb", "Mar", 
	"Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", 
	"Oct", "Nov", "Dec" };

static TEXT	help_text[] = "Available switches:\n\
    -a      analyze data and index pages\n\
    -d      analyze data pages\n\
    -i      analyze index leaf pages\n\
    -s      analyze system relations\n\
    -z      display version number\n";

#ifdef NETWARE_386
#include <fcntl.h>
#include <share.h>
#include <fileengd.h>
 
typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id_mod;
    USHORT	blk_length;
} *BLK;

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"
#include "../jrd/pwd.h"

#define FPRINTF		SVC_netware_fprintf

typedef struct open_files
{
  int           desc;
  struct open_files *open_files_next;
} open_files;

typedef struct mem
{
  void       *memory;
  struct mem *mem_next;
} mem;
#endif

#ifndef FPRINTF
#define FPRINTF 	ib_fprintf
#endif

/* threading declarations for thread data */

typedef struct tdba {
    struct thdd tdba_thd_data;
    UCHAR       *dba_env;
    FIL	        files;
    REL	        relations;
    SSHORT	    page_size;
    PAG	        buffer1;
    PAG	        global_buffer;
    int         exit_code;
#ifdef NETWARE_386
    SVC	        sw_outfile;
    mem         *head_of_mem_list;
    open_files  *head_of_files_list;
#else
    IB_FILE	    *sw_outfile;
#endif
} *TDBA;

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#ifdef NETWARE_386
#define GET_THREAD_DATA	        ((TDBA) THD_get_specific())
#define SET_THREAD_DATA         {  tddba = &thd_context;           \
                		   THD_put_specific ((THDD) tddba);\
				   tddba->tdba_thd_data.thdd_type = THDD_TYPE_TDBA; }
#define RESTORE_THREAD_DATA     THD_restore_specific()
#else
static struct tdba *gddba;

#define GET_THREAD_DATA	        (gddba)
#define SET_THREAD_DATA         gddba = tddba = &thd_context; \
				tddba->tdba_thd_data.thdd_type = THDD_TYPE_TDBA
#define RESTORE_THREAD_DATA     
#endif

#define EXIT(code)	            {  tddba->exit_code = code;        \
                                   LONGJMP(tddba->dba_env, 1);  }

#ifdef mpexl
#define GET_PRIVMODE	if (privmode_flag < 0) privmode_flag = ISC_check_privmode();\
			if (privmode_flag > 0) GETPRIVMODE();
#define GET_USERMODE	if (privmode_flag > 1) GETUSERMODE();

static SSHORT	privmode_flag = -1;
#endif

#ifdef NETWARE_386
int main_gstat(
    SVC		service)
#else

int CLIB_ROUTINE main (
    int		argc,
    char	**argv)
#endif
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Gather information from system relations to do analysis
 *	of a database.
 *
 **************************************/
REL	relation;
IDX	index;
HDR	header;
LIP	logp;
FIL	current;
SCHAR	**end, *name, *p, temp [1024],file_name [1024];
double	average;
SSHORT	n;
USHORT	sw_system, sw_data, sw_index, sw_version, sw_header, sw_log;
SLONG	page;
SLONG	redir_in, redir_out, redir_err;
UCHAR	*vp, *vend;
isc_db_handle db_handle = NULL;
#ifdef NETWARE_386
SVC	    sw_outfile;
int	argc;
char	**argv;
open_files	*open_file, *tmp1;
mem	*alloced, *tmp2; 
#else
IB_FILE	*sw_outfile;
#endif
TEXT	*q;
UCHAR	*dpb;
SSHORT	dpb_length;
UCHAR	dpb_string [100];
SCHAR	password_enc[33];
struct tdba	thd_context, *tddba;
JMP_BUF		env;
isc_tr_handle   transact1;
isc_req_handle  request1, request2, request3; 
long            status_vector [20];
#if defined (WIN95) && !defined (GUI_TOOLS)
BOOL	fAnsiCP;
#endif

SET_THREAD_DATA;
memset (tddba, 0, sizeof(*tddba));
memset (&status_vector, 0, sizeof(status_vector));
tddba->dba_env = env;

#ifdef NETWARE_386
/* Reinitialize static variables for multi-threading */
argc = service->svc_argc;
argv = service->svc_argv;
#endif

if (SETJMP(env))
    {
    int     exit_code;
    /* free mem */

    if (status_vector[1])
        {
        STATUS	*vector;
        SCHAR	s [1024];

        vector = status_vector;
        if (isc_interprete (s, &vector))
	        {
	        FPRINTF (tddba->sw_outfile, "%s\n", s); 
	        s [0] = '-';
	        while (isc_interprete (s + 1, &vector))
	            FPRINTF (tddba->sw_outfile, "%s\n", s);
	        } 
        }

#ifdef NETWARE_386
  	alloced = tddba->head_of_mem_list;
  	while (alloced != 0)
  	    { 
	    free(alloced->memory);
   	    alloced = alloced->mem_next;
	    }

	/* close files */
	open_file = tddba->head_of_files_list;
	while (open_file != 0)
    	{
   	    close(open_file->desc);
	    open_file = open_file->open_files_next;
    	}
	
	/* free linked lists */
	while (tddba->head_of_files_list != 0)
    	{
	    tmp1 = tddba->head_of_files_list;
	    tddba->head_of_files_list = tddba->head_of_files_list->open_files_next;
	    free(tmp1);
    	}
  
	while (tddba->head_of_mem_list != 0)
    	{
	    tmp2 = tddba->head_of_mem_list;
	    tddba->head_of_mem_list = tddba->head_of_mem_list->mem_next;
	    free(tmp2);
    	}

	service->svc_handle = 0;
    if (service->svc_service->in_use != NULL)
        *(service->svc_service->in_use) = FALSE;

    /* Mark service thread as finished. */
    service->svc_flags |= SVC_finished;

    /* If service is detached, cleanup memory being used by service. */
    if (service->svc_flags & SVC_detached)
        SVC_cleanup (service);
#endif

    exit_code = tddba->exit_code;
    RESTORE_THREAD_DATA;

#ifdef NETWARE_386
	return exit_code;
#else
	exit (exit_code);
#endif
    }

#ifdef NETWARE_386
sw_outfile = tddba->sw_outfile = service;
#else
sw_outfile = tddba->sw_outfile = ib_stderr;
#endif

#if defined (WIN95) && !defined (GUI_TOOLS)
fAnsiCP = FALSE;
#endif

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
    }
#ifndef NETWARE_386
else if (argc > 4 && !strcmp (argv [1], "-svc_re"))
    {
    redir_in = atol (argv [2]);
    redir_out = atol (argv [3]);
    redir_err = atol (argv [4]);
#ifdef WIN_NT
#if defined (WIN95) && !defined (GUI_TOOLS)
    fAnsiCP = TRUE;
#endif
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

name = NULL;
sw_log = sw_system = sw_data = sw_index = sw_version = sw_header = FALSE;

for (end = argv + argc, ++argv; argv < end;)
    {
    p = *argv++;
    if (*p == '-')
	switch (UPPER (p [1]))
	    {
	    case 'S':
		sw_system = TRUE;
		break;

	    case 'D':
		sw_data = TRUE;
		break;

	    case 'I':
		sw_index = TRUE;
		break;

	    case 'A':
		sw_index = sw_data = TRUE;
		break;

	    case 'Z':
		sw_version = TRUE;
		break;

	    case 'H':
		sw_header = TRUE;
		break;

	    case 'L':
		sw_log = TRUE;
		break;

	    default:
		FPRINTF (sw_outfile, "%s\n", help_text);
		EXIT (FINI_ERROR);
	    }
    else
	name = p;
    }

if (!name)
    {
    FPRINTF (sw_outfile, "please retry, giving a database name\n");
    EXIT (FINI_ERROR);
    }

if (!sw_data && !sw_index)
    sw_data = sw_index = TRUE;

#if defined (WIN95) && !defined (GUI_TOOLS)
if (!fAnsiCP)
    {
    ULONG	ulConsoleCP;

    ulConsoleCP = GetConsoleCP();
    if (ulConsoleCP == GetACP())
	fAnsiCP = TRUE;
    else
	if (ulConsoleCP != GetOEMCP())
	    {
	    FPRINTF(sw_outfile,
		"WARNING: The current codepage is not supported.  Any use of any\n"
		"         extended characters may result in incorrect file names.\n");
	    }
}
#endif
if (sw_version)
    FPRINTF (sw_outfile, "gstat version %s\n", GDS_VERSION);

/* Open database and go to work */

current = db_open (name, strlen (name));
tddba->page_size = sizeof (temp);
tddba->global_buffer = (PAG) temp;
header = (HDR) db_read ((SLONG) 0);

if (header->hdr_ods_version != ODS_VERSION && header->hdr_ods_version != ODS_VERSION6)
    {
    FPRINTF (sw_outfile, "Wrong ODS version, expected %d, encountered %d?\n", ODS_VERSION,
	header->hdr_ods_version);
    EXIT (FINI_ERROR);
    }

#if defined (WIN95) && !defined (GUI_TOOLS)
if (!fAnsiCP)
    AnsiToOem(name, file_name);
else
#endif
    strcpy(file_name, name);

FPRINTF (sw_outfile, "\nDatabase \"%s\"\n\n", file_name);

tddba->page_size = header->hdr_page_size;
#ifdef APOLLO
map_count = map_length / tddba->page_size;
#endif /* APOLLO */
tddba->buffer1 = (PAG) ALLOC (tddba->page_size);
tddba->global_buffer = (PAG) ALLOC (tddba->page_size);

/* gather continuation files */

page = HEADER_PAGE;
do
    {
    if (page != HEADER_PAGE)
	current = db_open (file_name, strlen (file_name));
    do
        {
	header = (HDR) db_read ((SLONG) page);
	if (current != tddba->files)
	    current->fil_fudge = 1; /* ignore header page once read it */
	*file_name = '\0';
	for (vp = header->hdr_data, vend = vp + header->hdr_page_size; 
	     vp < vend && *vp != HDR_end; vp += 2 + vp [1])
	    {
            if (*vp == HDR_file)
            {
		memcpy (file_name, vp + 2, vp [1]);
		*(file_name+vp[1]) = '\0';
#if defined (WIN95) && !defined (GUI_TOOLS)
		if (!fAnsiCP)
		    AnsiToOem(file_name, file_name);
#endif
            }
	    if (*vp == HDR_last_page)
		memcpy (&current->fil_max_page, vp + 2, sizeof (current->fil_max_page));
            }
        }
    while (page = header->hdr_next_page);
    page = current->fil_max_page + 1; /* first page of next file */
    }
while (*file_name);

/* Print header page */

page = HEADER_PAGE;
do
    {
    header = (HDR) db_read ((SLONG) page);
    PPG_print_header (header, page, sw_outfile);
    }
while (page = header->hdr_next_page);

if (sw_header)
    EXIT (FINI_OK);

/* print continuation file sequence */

FPRINTF (sw_outfile, "\n\nDatabase file sequence:\n");
for (current = tddba->files; current->fil_next; current = current->fil_next)
    FPRINTF (sw_outfile, "File %s continues as file %s\n", current->fil_string,
	current->fil_next->fil_string);
FPRINTF (sw_outfile, "File %s is the %s file\n\n", current->fil_string,
    (current == tddba->files) ? "only" : "last");

/* print log page */

page = LOG_PAGE;
do
    {
    logp = (LIP) db_read ((SLONG) page);
    PPG_print_log (logp, page, sw_outfile);
    }
while (page = logp->log_next_page);

if (sw_log)
    EXIT (FINI_OK);

#if (defined NETWARE_386 || defined WIN_NT)
dpb = dpb_string;
*dpb++ = gds__dpb_version1;
*dpb++ = gds__dpb_user_name;
*dpb++ = strlen (LOCKSMITH_USER);
q = LOCKSMITH_USER;
while (*q)
    *dpb++ = *q++;

*dpb++ = gds__dpb_password_enc;
strcpy (password_enc, (char *)ENC_crypt (LOCKSMITH_PASSWORD, PASSWORD_SALT));
q = password_enc + 2;
*dpb++ = strlen (q);
while (*q)
    *dpb++ = *q++;

dpb_length = dpb - dpb_string;

isc_attach_database (status_vector, 0, GDS_VAL(name), &DB, dpb_length, dpb_string);
if (status_vector [1])
    EXIT (FINI_ERROR);
#else
READY GDS_VAL (name) AS DB;
ON_ERROR
    EXIT (FINI_ERROR);
END_ERROR
#endif

if (sw_version)
    gds__version (&DB, NULL, NULL);
transact1 = 0;
START_TRANSACTION transact1 READ_ONLY;
ON_ERROR
    EXIT (FINI_ERROR);
END_ERROR

request1 = NULL;
request2 = NULL;
request3 = NULL;

FOR (TRANSACTION_HANDLE transact1 REQUEST_HANDLE request1)
    X IN RDB$RELATIONS SORTED BY DESC X.RDB$RELATION_NAME
    if (!sw_system && X.RDB$SYSTEM_FLAG)
	continue;
    if (!X.RDB$VIEW_BLR.NULL || !X.RDB$EXTERNAL_FILE.NULL)
	continue;
    relation = (REL) ALLOC (sizeof (struct rel));
    relation->rel_next = tddba->relations;
    tddba->relations = relation;
    relation->rel_id = X.RDB$RELATION_ID;
    strcpy (relation->rel_name, X.RDB$RELATION_NAME);
    truncate_name (relation->rel_name);
    FOR (TRANSACTION_HANDLE transact1 REQUEST_HANDLE request2)
        Y IN RDB$PAGES WITH  Y.RDB$RELATION_ID EQ relation->rel_id AND
	    Y.RDB$PAGE_SEQUENCE EQ 0
	if (Y.RDB$PAGE_TYPE == pag_pointer)
	    relation->rel_pointer_page = Y.RDB$PAGE_NUMBER;
	if (Y.RDB$PAGE_TYPE == pag_root)
	    relation->rel_index_root = Y.RDB$PAGE_NUMBER;
    END_FOR;
    ON_ERROR
        EXIT (FINI_ERROR);
    END_ERROR
    if (sw_index)
	FOR (TRANSACTION_HANDLE transact1 REQUEST_HANDLE request3)
        Y IN RDB$INDICES WITH  Y.RDB$RELATION_NAME EQ relation->rel_name
		SORTED BY DESC Y.RDB$INDEX_NAME
	    if (Y.RDB$INDEX_INACTIVE)
		continue;
	    index = (IDX) ALLOC (sizeof (struct idx));
	    index->idx_next = relation->rel_indexes;
	    relation->rel_indexes = index;
	    strcpy (index->idx_name, Y.RDB$INDEX_NAME);
	    truncate_name (index->idx_name);
	    index->idx_id = Y.RDB$INDEX_ID - 1;
	END_FOR;
    ON_ERROR
        EXIT (FINI_ERROR);
    END_ERROR
END_FOR;
ON_ERROR
    EXIT (FINI_ERROR);
END_ERROR

if (request1)
    isc_release_request (status_vector, &request1);
if (request2)
    isc_release_request (status_vector, &request2);
if (request3)
    isc_release_request (status_vector, &request3);
    
COMMIT transact1;
ON_ERROR
    EXIT (FINI_ERROR);
END_ERROR
FINISH;
ON_ERROR
    EXIT (FINI_ERROR);
END_ERROR

FPRINTF (sw_outfile, "\nAnalyzing database pages ...\n\n");

for (relation = tddba->relations; relation; relation = relation->rel_next)
    {
    if (sw_data)
	analyze_data (relation);
    for (index = relation->rel_indexes; index; index = index->idx_next)
	analyze_index (relation, index);
    }

/* Print results */

for (relation = tddba->relations; relation; relation = relation->rel_next)
    {
    FPRINTF (sw_outfile, "%s (%d)\n", relation->rel_name, relation->rel_id);
    if (sw_data)
	{
	FPRINTF (sw_outfile, "    Primary pointer page: %ld, Index root page: %ld\n",
	    relation->rel_pointer_page, relation->rel_index_root);
	average = (relation->rel_data_pages) ?
			(double) relation->rel_total_space * 100 /
				((double) relation->rel_data_pages * (tddba->page_size - DPG_SIZE)) : 0;
	FPRINTF (sw_outfile, "    Data pages: %ld, data page slots: %ld, average fill: %.0f%%\n",
	    relation->rel_data_pages, relation->rel_slots, average);
	FPRINTF (sw_outfile, "    Fill distribution:\n");
	print_distribution ("\t", relation->rel_fill_distribution);
	}
    FPRINTF (sw_outfile, "\n");
    for (index = relation->rel_indexes; index; index = index->idx_next)
	{
	FPRINTF (sw_outfile, "    Index %s (%d)\n", index->idx_name, index->idx_id);
	FPRINTF (sw_outfile, "\tDepth: %d, leaf buckets: %ld, nodes: %ld\n",
	    index->idx_depth, index->idx_leaf_buckets, index->idx_nodes);
	average = (index->idx_nodes) ?
	    index->idx_data_length / index->idx_nodes : 0;
	FPRINTF (sw_outfile, "\tAverage data length: %.2f, total dup: %ld, max dup: %ld\n",
	    average,
	    index->idx_total_duplicates, index->idx_max_duplicates);
	FPRINTF (sw_outfile, "\tFill distribution:\n");
	print_distribution ("\t    ", index->idx_fill_distribution);
	FPRINTF (sw_outfile, "\n");
	}
    }

EXIT (FINI_OK);

return 0;
}

static SCHAR *alloc (
    SLONG	size)
{
/**************************************
 *
 *	a l l o c
 *
 **************************************
 *
 * Functional description
 *	Allocate and zero a piece of memory.
 *
 **************************************/
SCHAR	*block, *p;
#ifdef NETWARE_386
mem *mem_list;
TDBA	tddba;

tddba = GET_THREAD_DATA;

block = p = malloc (size);
do *p++ = 0; while (--size);

mem_list = malloc(sizeof(mem));
mem_list->memory = block;
mem_list->mem_next = 0;

if (tddba->head_of_mem_list == 0)
 tddba->head_of_mem_list = mem_list;
else
{
 mem_list->mem_next = tddba->head_of_mem_list;
 tddba->head_of_mem_list = mem_list;
}
#else
block = p = gds__alloc (size);
do *p++ = 0; while (--size);
#endif

return block;
}

static void analyze_data (
    REL		relation)
{
/**************************************
 *
 *	a n a l y z e _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Analyze data pages associated with relation.
 *
 **************************************/
PPG	pointer_page;
DPG	data_page;
SSHORT	n;
SLONG	next_pp, *ptr, *end;
TDBA	tddba;

tddba = GET_THREAD_DATA;

pointer_page = (PPG) tddba->buffer1;

for (next_pp = relation->rel_pointer_page; next_pp; next_pp = pointer_page->ppg_next)
    {
    move (db_read (next_pp), pointer_page, tddba->page_size);
    for (ptr = pointer_page->ppg_page, end = ptr + pointer_page->ppg_count;
	 ptr < end; ptr++)
	{
	++relation->rel_slots;
	if (*ptr)
	    {
	    ++relation->rel_data_pages;
	    if (!analyze_data_page (relation, db_read (*ptr)))
		FPRINTF (tddba->sw_outfile, "    Expected data on page %ld\n", *ptr);
	    }
	}
    }
}

static BOOLEAN analyze_data_page (
    REL		relation,
    DPG		page)
{
/**************************************
 *
 *	a n a l y z e _ d a t a _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Analyze space utilization for data page.
 *
 **************************************/
SSHORT		bucket, space, base;
struct dpg_repeat	*tail, *end;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (page->dpg_header.pag_type != pag_data)
    return FALSE;

space = page->dpg_count * sizeof (struct dpg_repeat);

for (tail = page->dpg_rpt, end = tail + page->dpg_count; tail < end; tail++)
    if (tail->dpg_offset && tail->dpg_length)
	space += tail->dpg_length;

relation->rel_total_space += space;
bucket = (space * BUCKETS) / (tddba->page_size - DPG_SIZE);

if (bucket == BUCKETS)
    --bucket;

++relation->rel_fill_distribution [bucket];

return TRUE;
}

static void analyze_index (
    REL		relation,
    IDX		index)
{
/**************************************
 *
 *	a n a l y z e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
BTR	bucket;
IRT	index_root;
IRTD	*desc;
BTN	node;
SSHORT	n, space, duplicates, key_length, l, dup, page_count;
UCHAR	key [256], *p, *q;
SLONG	number, page, prior_page, right_sibling, node_count, node_page_count;
TDBA	tddba;

tddba = GET_THREAD_DATA;

index_root = (IRT) db_read (relation->rel_index_root);

if (index_root->irt_count <= index->idx_id ||
    !(page = index_root->irt_rpt [index->idx_id].irt_root))
    return;

bucket = (BTR) db_read (page);

if (bucket->btr_length > tddba->page_size)
    {
    FPRINTF (tddba->sw_outfile, "\n***ERROR: page size is too large: %ld\n", bucket->btr_length);
    return;
    }

index->idx_depth = bucket->btr_level + 1;

/*** go down through the levels, printing all pages ***/

while (bucket->btr_level)
    {
    FPRINTF (tddba->sw_outfile, "\nlevel %ld:\n", bucket->btr_level);

    right_sibling = page;
    move (bucket->btr_nodes[0].btn_number, &page, sizeof (page));

    node_count = 0;
    prior_page = 0;
    for (page_count = 0; right_sibling; right_sibling = bucket->btr_sibling, page_count++)
        {
        FPRINTF (tddba->sw_outfile, "page: %ld\n", right_sibling);
        bucket = (BTR) db_read (right_sibling);

	if (bucket->btr_left_sibling != prior_page)
	    {
            FPRINTF (tddba->sw_outfile, "\n***ERROR: left sibling is %ld\n", bucket->btr_left_sibling);
	    return;
	    }

	if (bucket->btr_length > tddba->page_size)
	    {
            FPRINTF (tddba->sw_outfile, "\n***ERROR: page size is too large: %ld\n", bucket->btr_length);
	    return;
	    }

	prior_page = right_sibling;

	if (bucket->btr_header.pag_flags & btr_not_propogated)
	    {
            FPRINTF (tddba->sw_outfile, "\n***ERROR: non-leaf bucket marked not propogated\n");
	    return;
	    }
	    
        /* count the nodes on page as well */

        node_page_count = 0;
        FPRINTF (tddba->sw_outfile, "\nnodes:");
        for (node = bucket->btr_nodes;; node = (BTN) (node->btn_data + node->btn_length))
            {
	    move (node->btn_number, &number, sizeof (number));

	    if (!(node_page_count % 4))
                FPRINTF (tddba->sw_outfile, "\n");	     
            FPRINTF (tddba->sw_outfile, "[%ld:%d,%d,", number, node->btn_prefix, node->btn_length);

 	    /* print out the prefix */

	    if (l = node->btn_prefix)
	        {
	        p = key;
	        FPRINTF (tddba->sw_outfile, "(");
	        do  {
	            FPRINTF (tddba->sw_outfile, "\'%d\'", *p);
	            p++; 
	            } while (--l);    
	        FPRINTF (tddba->sw_outfile, ")");
	        }	    	

  	    if (l = node->btn_length)
	        {
	        p = key + node->btn_prefix;
	        q = node->btn_data;
	        do {
	           FPRINTF (tddba->sw_outfile, "\'%d\'", *q);
	           *p++ = *q++; 
	           } while (--l);
	        }	    	

	    FPRINTF (tddba->sw_outfile, "] ");

	    if (number < 0)
	        break;

            if (!node->btn_length && !node->btn_prefix && 
                (bucket->btr_left_sibling || node > bucket->btr_nodes))
		{
                FPRINTF (tddba->sw_outfile, "\n***ERROR: node prefix and length 0\n");
		return;
		}

	    node_page_count++;
	    }

        if (!node_page_count)
	    {
            FPRINTF (tddba->sw_outfile, "\n***ERROR: page empty\n");
	    return;
	    }

        FPRINTF (tddba->sw_outfile, "\nnodes on page: %d\n", node_page_count);

        node_count += node_page_count;
        }

    FPRINTF (tddba->sw_outfile, "\npage count: %d\n", page_count);
    FPRINTF (tddba->sw_outfile, "node count: %d\n", node_count);

    bucket = (BTR) db_read (page);
    if (bucket->btr_length > tddba->page_size)
        {
        FPRINTF (tddba->sw_outfile, "\n***ERROR: page size is too large: %ld\n", bucket->btr_length);
	return;
	}
    }
    
duplicates = 0;
key_length = 0;

FPRINTF (tddba->sw_outfile, "\nlevel 0:");

for (;;)
    {

/*** print the pages involved at the leaf ***/

if (bucket->btr_header.pag_flags & btr_not_propogated)
    FPRINTF (tddba->sw_outfile, "\n\n(page %ld):", page);
else
    FPRINTF (tddba->sw_outfile, "\n\npage %ld:", page);

    ++index->idx_leaf_buckets;
    node_page_count = 0;
    for (node = bucket->btr_nodes;; node = (BTN) (node->btn_data + node->btn_length))
	{
	move (node->btn_number, &number, sizeof (number));

	if (!(node_page_count % 4))
            FPRINTF (tddba->sw_outfile, "\n");	     
        FPRINTF (tddba->sw_outfile, "[%ld:%d,%d,", number, node->btn_prefix, node->btn_length);

	/* print out the prefix */

	if (l = node->btn_prefix)
	    {
	    p = key;
	    FPRINTF (tddba->sw_outfile, "(");
	    do  {
	        FPRINTF (tddba->sw_outfile, "\'%d\'", *p++);
	        } while (--l);    
	    FPRINTF (tddba->sw_outfile, ")");
	    }	    	

	/* print out the node data */

	if (l = node->btn_length)
	    {
	    q = node->btn_data;
	    do {
	       FPRINTF (tddba->sw_outfile, "\'%d\'", *q++);
	       } while (--l);    
	    }	    	

	FPRINTF (tddba->sw_outfile, "] ");

	if (number < 0)
	    break;

	++index->idx_nodes;
	index->idx_data_length += node->btn_length;
	l = node->btn_length + node->btn_prefix;
	if (node == bucket->btr_nodes)
	    dup = key_equality (key_length, key, node);
	else
	    dup = !node->btn_length && l == key_length;
	if (dup)
	    {
	    ++index->idx_total_duplicates;
	    ++duplicates;
	    }
	else
	    {
	    if (duplicates > index->idx_max_duplicates)
		index->idx_max_duplicates = duplicates;
	    duplicates = 0;
	    }

	/* save the key */

	key_length = l;
	if (l = node->btn_length)
	    {
	    p = key + node->btn_prefix;
	    q = node->btn_data;
	    do {
	       *p++ = *q++; 
	       } while (--l);    
	    }	    	

	node_page_count++;
	}

    FPRINTF (tddba->sw_outfile, "\nnode count: %ld", node_page_count);

    if (duplicates > index->idx_max_duplicates)
	index->idx_max_duplicates = duplicates;
    space = bucket->btr_length - OFFSETA (BTR, btr_nodes);
    n = (space * BUCKETS) / (tddba->page_size - OFFSETA (BTR, btr_nodes));
    if (n == BUCKETS)
	--n;
    ++index->idx_fill_distribution [n];

    if (number == END_LEVEL)
	break;
    number = page;
    page = bucket->btr_sibling;
    bucket = (BTR) db_read (page);
    
    if (bucket->btr_header.pag_type != pag_index)
	{
	FPRINTF (tddba->sw_outfile, "\n***ERROR: Expected b-tree bucket on page %ld from %ld\n", page, number);
	break;
	}

    if (bucket->btr_length > tddba->page_size)
        {
        FPRINTF (tddba->sw_outfile, "\n***ERROR: page size is too large: %ld\n", bucket->btr_length);
        return;
        }

    if (bucket->btr_left_sibling != number)
        {
        FPRINTF (tddba->sw_outfile, "\n***ERROR: left sibling is %ld\n", bucket->btr_left_sibling);
	return;
	}

    }

FPRINTF (tddba->sw_outfile, "\n");
}

#ifdef NETWARE_386
static void db_error (
    int 	status)
{
/**************************************
 *
 *	d b _ e r r o r		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*p;
TDBA	tddba;

tddba = GET_THREAD_DATA;

FPRINTF (tddba->sw_outfile, "%s\n", sys_errlist [status]);
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
FIL	fil;
open_files *file_list;
TEXT expanded_filename[256];
TDBA	tddba;

tddba = GET_THREAD_DATA;

ISC_expand_filename (file_name, file_length, expanded_filename);

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (expanded_filename) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (expanded_filename) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, expanded_filename);
fil->fil_length = strlen (expanded_filename);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

if ((fil->fil_desc = FEsopen (expanded_filename, O_RDWR | O_BINARY, SH_DENYNO, 0, 0, 
                          PrimaryDataStream)) == -1)
    db_error (errno);

file_list = malloc(sizeof(tddba->files));
file_list->desc = fil->fil_desc;
file_list->open_files_next = 0;

if (tddba->head_of_files_list == 0)
 tddba->head_of_files_list = file_list;
else
{
 file_list->open_files_next = tddba->head_of_files_list;
 tddba->head_of_files_list = file_list;
}

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SCHAR	*p;
SSHORT	length, l;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (lseek (fil->fil_desc, page_number * tddba->page_size, 0) == -1)
 db_error (errno);

for (p = (SCHAR*) tddba->global_buffer, length = tddba->page_size; length > 0;)
    {
    l = read (fil->fil_desc, p, length);
    if (l <= 0)
	  db_error (errno);
    p += l;
    length -= l;
    }

return tddba->global_buffer;
}
#endif

#ifdef APOLLO
static void db_error (
    status_$t	status)
{
/**************************************
 *
 *	d b _ e r r o r   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

error_$print (status);
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
status_$t	status;
FIL		fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;
map_count = 64;
map_length = 32768; /*page_size * map_count;*/
fil->fil_base = 0;

fil->fil_region = ms_$mapl (*file_name, file_length, 
	(SLONG) 0, map_length, 
	ms_$cowriters, ms_$wr, (SSHORT) -1,
	map_length, status);

if (status.all)
    db_error (status);

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d   ( a p o l l o )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
status_$t	status;
SLONG		section, length;
FIL		fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (page_number < fil->fil_base ||
    page_number - fil->fil_base >= map_count)
    {
    fil->fil_base = (page_number / map_count) * map_count;
    section = fil->fil_base * tddba->page_size;
    length = map_length;
    fil->fil_region = ms_$remap (fil->fil_region, section, 
	    map_length, length, status);
    if (status.all)
	db_error (status);
    }

return (PAG) (fil->fil_region + (page_number - fil->fil_base) * tddba->page_size);
}
#endif

#ifdef OS2_ONLY
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT		s [128];
SLONG		len;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (DosGetMessage (0, 0, s, sizeof (s), status, "OSO001.MSG", &len))
    sprintf (s, "unknown OS/2 error %ld", status);

FPRINTF (tddba->sw_outfile, "%s\n", s);
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
FIL	fil;
ULONG	action, open_mode, status;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

open_mode = OPEN_ACCESS_READWRITE |	/* Access mode -- read/write */
	OPEN_SHARE_DENYNONE |		/* Share mode -- permit read/write */
	OPEN_FLAGS_NOINHERIT |		/* Inheritance mode -- don't */
	OPEN_FLAGS_FAIL_ON_ERROR |	/* Fail errors -- return code */
	OPEN_FLAGS_WRITE_THROUGH |	/* Write thru -- absolutely */
	(0 << 15);			/* DASD open -- no */

status = DosOpen (
    file_name,		/* File pathname */
    &fil->fil_desc,	/* File handle */
    &action,		/* Action taken */
    100,		/* Initial size */
    FILE_NORMAL,	/* File attributes */
    OPEN_ACTION_OPEN_IF_EXISTS,	/* Open Flag (truncate if file exists) */
    open_mode,		/* Open mode (see above) */
    0L);		/* Reserved */

if (status)
    db_error (status);

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
ULONG	status, new_offset, actual_length;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (status = DosSetFilePtr (fil->fil_desc, (ULONG) (page_number * tddba->page_size), FILE_BEGIN, &new_offset))
    db_error (status);

if (status = DosRead (fil->fil_desc, tddba->global_buffer, tddba->page_size, &actual_length))
    db_error (status);
if (actual_length != tddba->page_size)
    {
    FPRINTF (tddba->sw_outfile, "Unexpected end of database file.\n");
    EXIT (FINI_ERROR);
    }

return tddba->global_buffer;
}
#endif

#ifdef WIN_NT
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
TEXT		s [128];
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (!FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	status,
	GetUserDefaultLangID(),
	s,
	sizeof (s),
	NULL))
    sprintf (s, "unknown Windows NT error %ld", status);

FPRINTF (tddba->sw_outfile, "%s\n", s);
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

if ((fil->fil_desc = CreateFile (file_name,
	GENERIC_READ | GENERIC_WRITE,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_EXISTING,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
	0)) == INVALID_HANDLE_VALUE)
    db_error (GetLastError());

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SLONG	actual_length;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (SetFilePointer (fil->fil_desc, (ULONG) (page_number * tddba->page_size), NULL, FILE_BEGIN) == -1)
    db_error (GetLastError());

if (!ReadFile (fil->fil_desc, tddba->global_buffer, tddba->page_size, &actual_length, NULL))
    db_error (GetLastError());
if (actual_length != tddba->page_size)
    {
    FPRINTF (tddba->sw_outfile, "Unexpected end of database file.\n");
    EXIT (FINI_ERROR);
    }

return tddba->global_buffer;
}
#endif

#ifdef mpexl
static void db_error (
    SLONG	status)
{
/**************************************
 *
 *	d b _ e r r o r		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SSHORT		l, errcode;
t_mpe_status	code_stat, errmsg_stat;
SLONG		found;
TEXT		s [128];
TDBA	tddba;

tddba = GET_THREAD_DATA;

l = sizeof (s) - 1;
if (status & 0xFFFF)
    {
    code_stat.word = status;
    HPERRMSG (3, 1,, code_stat.decode, s, &l, &errmsg_stat.word);
    found = errmsg_stat.word;
    }
else
    {
    errcode = status >> 16;
    FERRMSG (&errcode, s, &l);
    found = (ccode() == CCE) ? 0 : 1;
    }
if (!found)
    s [l] = 0;
else
    {
    errcode = status >> 16;
    sprintf (s, "uninterpreted MPE/XL code %d from subsystem %d", errcode, (USHORT) status);
    }

FPRINTF (tddba->sw_outfile, "%s\n", s);
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *
 **************************************/
UCHAR	temp [128], *p;
SLONG	status, domain, access, exclusive, multi, random, buffering,
	disposition, filecode, privlev;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

temp [0] = ' ';
for (p = &temp [1]; *p = *file_name++; p++)
    ;
*p = ' ';

domain = DOMAIN_OPEN_PERM;
access = ACCESS_RW;
exclusive = EXCLUSIVE_SHARE;
multi = MULTIREC_YES;
filecode = FILECODE_DB;
privlev = PRIVLEV_DB;
random = WILL_ACC_RANDOM;
buffering = INHIBIT_BUF_YES;
disposition = DISP_KEEP;

GET_PRIVMODE
HPFOPEN (&fil->fil_desc, &status,
    OPN_FORMAL, temp,
    OPN_DOMAIN, &domain,
    OPN_ACCESS_TYPE, &access,
    OPN_EXCLUSIVE, &exclusive,
    OPN_MULTIREC, &multi,
    OPN_FILECODE, &filecode,
    OPN_PRIVLEV, &privlev,
    OPN_WILL_ACCESS, &random,
    OPN_INHIBIT_BUF, &buffering,
    OPN_DISP, &disposition,
    ITM_END);
GET_USERMODE

if (status)
    db_error (status);

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d		( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SSHORT	errcode;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
GET_PRIVMODE
FREADDIR ((SSHORT) fil->fil_desc, (SCHAR*) tddba->global_buffer, (SSHORT) -tddba->page_size,
    page_number * ((tddba->page_size - 1) / 1024 + 1));
if (ccode() != CCE)
    {
    FCHECK ((SSHORT) fil->fil_desc, &errcode, NULL, NULL, NULL);
    GET_USERMODE
    db_error ((SLONG) errcode << 16);
    }
GET_USERMODE

return tddba->global_buffer;
}
#endif

#ifndef NETWARE_386
#ifndef APOLLO
#ifndef OS2_ONLY
#ifndef WIN_NT
#ifndef mpexl
static void db_error (
    int		status)
{
/**************************************
 *
 *	d b _ e r r o r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SCHAR	*p;
TDBA	tddba;

tddba = GET_THREAD_DATA;

#ifndef VMS
FPRINTF (tddba->sw_outfile, "%s\n", sys_errlist [status]);
#else
if ((p = strerror (status)) ||
    (p = strerror (EVMSERR, status)))
    FPRINTF (tddba->sw_outfile, "%s\n", p);
else
    FPRINTF (tddba->sw_outfile, "uninterpreted code %x\n", status);
#endif
EXIT (FINI_ERROR);
}

static FIL db_open (
    UCHAR	*file_name,
    USHORT	file_length)
{
/**************************************
 *
 *	d b _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Open a database file.
 *      Put the file on an ordered list.
 *
 **************************************/
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (FIL) ALLOC (sizeof (struct fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

if ((fil->fil_desc = open (file_name, 2)) == -1)
    db_error (errno);

return fil;
}

static PAG db_read (
    SLONG	page_number)
{
/**************************************
 *
 *	d b _ r e a d
 *
 **************************************
 *
 * Functional description
 *	Read a database page.
 *
 **************************************/
SCHAR	*p;
SSHORT	length, l;
FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (lseek (fil->fil_desc, page_number * tddba->page_size, 0) == -1)
    db_error (errno);
    
for (p = (SCHAR*) tddba->global_buffer, length = tddba->page_size; length > 0;)
    {
    l = read (fil->fil_desc, p, length);
    if(l < 0)
	db_error (errno);
    if(!l)
      {
      FPRINTF (tddba->sw_outfile, "Unexpected end of database file.\n");
      EXIT (FINI_ERROR);
      }
    p += l;
    length -= l;
    }

return tddba->global_buffer;
}
#endif
#endif
#endif
#endif
#endif

static BOOLEAN key_equality (
    SSHORT	length,
    SCHAR	*key,
    BTN		node)
{
/**************************************
 *
 *	k e y _ e q u a l i t y
 *
 **************************************
 *
 * Functional description
 *	Check a B-tree node against a key for equality.
 *
 **************************************/
SSHORT	l;
SCHAR	*p;

if (length != node->btn_length + node->btn_prefix)
    return FALSE;

if (!(l = node->btn_length))
    return TRUE;

p = (SCHAR*) node->btn_data;
key += node->btn_prefix;

do
    if (*p++ != *key++)
	return FALSE;
while (--l);

return TRUE;
}

static void move (
    SCHAR	*from,
    SCHAR	*to,
    SSHORT	length)
{
/**************************************
 *
 *	m o v e
 *
 **************************************
 *
 * Functional description
 *	Move some stuff.
 *
 **************************************/

do *to++ = *from++; while (--length);
}

static void print_distribution (
    SCHAR	*prefix,
    SLONG	*vector)
{
/**************************************
 *
 *	p r i n t _ d i s t r i b u t i o n
 *
 **************************************
 *
 * Functional description
 *	Print distribution as percentages.
 *
 **************************************/
SSHORT	n;
TDBA	tddba;

tddba = GET_THREAD_DATA;

for (n = 0; n < BUCKETS; n++)
    {
    FPRINTF (tddba->sw_outfile, "%s%2d - %2d%% = %ld\n",
	prefix,
	n * 100 / BUCKETS,
	(n + 1) * 100 / BUCKETS - 1,
	vector [n]);
    }
}

static void print_header (
    HDR		header)
{
/**************************************
 *
 *	p r i n t _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Print database header page.
 *
 **************************************/
UCHAR	*p, *end;
SLONG	number;
struct tm	time;
SSHORT	flags;
#ifdef NETWARE_386
SVC	    sw_outfile;
#else
IB_FILE	*sw_outfile;
#endif
TDBA	tddba;

tddba = GET_THREAD_DATA;
sw_outfile = tddba->sw_outfile;

FPRINTF (sw_outfile, "Database header page information:\n");
FPRINTF (sw_outfile, "    Page size\t\t%d\n", header->hdr_page_size);
#ifdef gds_version3
FPRINTF (sw_outfile, "    ODS version\t\t%d.%d\n", header->hdr_ods_version,
    header->hdr_ods_minor);
#else
FPRINTF (sw_outfile, "    ODS version\t\t%d\n", header->hdr_ods_version);
#endif
FPRINTF (sw_outfile, "    Oldest transaction\t%ld\n", header->hdr_oldest_transaction);
FPRINTF (sw_outfile, "    Oldest active\t\t%ld\n", header->hdr_oldest_active);
FPRINTF (sw_outfile, "    Oldest snapshot\t\t%ld\n", header->hdr_oldest_snapshot);
FPRINTF (sw_outfile, "    Next transaction\t%ld\n", header->hdr_next_transaction);

#ifdef gds_version3

/*
FPRINTF (sw_outfile, "    Sequence number    %d\n", header->hdr_sequence);
FPRINTF (sw_outfile, "    Creation date    \n", header->hdr_creation_date);
*/

FPRINTF (sw_outfile, "    Next attachment ID\t%ld\n", header->hdr_attachment_id);
FPRINTF (sw_outfile, "    Implementation ID\t%ld\n", header->hdr_implementation);
FPRINTF (sw_outfile, "    Shadow count\t%ld\n", header->hdr_shadow_count);
#endif

gds__decode_date (header->hdr_creation_date, &time);
FPRINTF (sw_outfile, "    Creation date\t%s %d, %d %d:%02d:%02d\n",
	months [time.tm_mon], time.tm_mday, time.tm_year + 1900,
	time.tm_hour, time.tm_min, time.tm_sec);

if (flags = header->hdr_flags)
    {
    FPRINTF (sw_outfile, "    Attributes\t\t");
    if (flags & hdr_force_write)
	{
	FPRINTF (sw_outfile, "force write");
	if (flags & hdr_no_reserve)
	    FPRINTF (sw_outfile, ", ");
	}
    if (flags & hdr_no_reserve)
	FPRINTF (sw_outfile, "no reserve");
    FPRINTF (sw_outfile, "\n");
    }

FPRINTF (sw_outfile, "\n    Variable header data:\n");

for (p = header->hdr_data, end = p + header->hdr_page_size; 
     p < end && *p != HDR_end; p += 2 + p [1])
    switch (*p)
	{
	case HDR_root_file_name:
	    FPRINTF (sw_outfile, "\tRoot file name: %*s\n", p [1], p + 2);
	    break;

	case HDR_journal_server:
	    FPRINTF (sw_outfile, "\tJournal server: %*s\n", p [1], p + 2);
	    break;

	case HDR_file:
	    FPRINTF (sw_outfile, "\tContinuation file: %*s\n", p [1], p + 2);
	    break;

	case HDR_last_page:
	    move (p + 2, &number, sizeof (number));
	    FPRINTF (sw_outfile, "\tLast logical page: %ld\n", number);
	    break;

	case HDR_unlicensed:
	    move (p + 2, &number, sizeof (number));
	    FPRINTF (sw_outfile, "\tUnlicensed accesses: %ld\n", number);
	    break;

#ifdef gds_version3
	case HDR_sweep_interval:
	    move (p + 2, &number, sizeof (number));
	    FPRINTF (sw_outfile, "\tSweep interval: %ld\n", number);
	    break;

	case HDR_log_name:
	    FPRINTF (sw_outfile, "\tReplay logging file: %*s\n", p [1], p + 2);
	    break;

#endif

	default:
	    FPRINTF (sw_outfile, "\tUnrecognized option %d, length %d\n", p [0], p [1]);
	}

FPRINTF (sw_outfile, "\t*END*\n");
}

static void truncate_name (
    SCHAR	*string)
{
/**************************************
 *
 *	t r u n c a t e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Zap trailing blanks.
 *
 **************************************/

for (; *string; ++string)
    if (*string == ' ')
	{
	*string = 0;
	return;
	}
}

