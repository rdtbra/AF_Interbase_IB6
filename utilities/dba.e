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
#include <fcntl.h>
#include "../jrd/ibsetjmp.h"
#include "../jrd/common.h"
#include "../jrd/time.h"
#include "../jrd/gds.h"
#include "../jrd/ods.h"
#include "../jrd/license.h"
#include "../jrd/msg_encode.h"
#include "../jrd/gdsassert.h"
#ifndef SUPERSERVER
#include "../utilities/ppg_proto.h"
#endif
#include "../utilities/dbaswi.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_f_proto.h"
#include "../jrd/thd.h"
#include "../jrd/enc_proto.h"
#ifdef SUPERSERVER
#include "../utilities/cmd_util_proto.h"
#endif

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
#endif

#ifdef mpexl
#include <mpe.h>
#include "../jrd/mpexl.h"
#endif

/* For Netware the follow DB handle and isc_status is #defined to be a  */
/* local variable on the stack in main.  This is to avoid multiple      */
/* threading problems with module level statics.                        */
DATABASE DB = STATIC "yachts.lnk";
#define DB          db_handle
#define isc_status  status_vector

#define ALLOC(size)	alloc ((SLONG) size);
#define BUCKETS		5
#define WINDOW_SIZE	(1 << 17)

#if (defined WIN_NT || defined OS2_ONLY)
#include <stdlib.h>
#else
#ifndef linux
extern SCHAR	*sys_errlist[];
#endif
#endif

typedef struct dba_idx {
    struct dba_idx	*idx_next;
    SSHORT	idx_id;
    SSHORT	idx_depth;
    SLONG	idx_leaf_buckets;
    SLONG	idx_total_duplicates;
    SLONG	idx_max_duplicates;
    SLONG	idx_nodes;
    SLONG	idx_data_length;
    SLONG	idx_fill_distribution [BUCKETS];
    SCHAR	idx_name [32];
} *DBA_IDX;

typedef struct rel {
    struct rel	*rel_next;
    struct dba_idx	*rel_indexes;
    SLONG	rel_index_root;
    SLONG	rel_pointer_page;
    SLONG	rel_slots;
    SLONG	rel_data_pages;
    ULONG	rel_records;
    ULONG	rel_record_space;
    ULONG	rel_versions;
    ULONG	rel_version_space;
    ULONG	rel_max_versions;
    SLONG	rel_fill_distribution [BUCKETS];
    ULONG	rel_total_space;
    SSHORT	rel_id;
    SCHAR	rel_name [32];
} *REL;

/* kidnapped from jrd/pio.h and abused */

typedef struct dba_fil {
    struct dba_fil	*fil_next;		/* Next file in database */
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
} *DBA_FIL;

static SCHAR	*alloc (SLONG);
static void	analyze_data (REL, BOOLEAN);
static BOOLEAN	analyze_data_page (REL, DPG, BOOLEAN);
static ULONG	analyze_fragments (REL, RHDF);
static ULONG	analyze_versions (REL, RHDF);
static void	analyze_index (REL, DBA_IDX);
#ifdef APOLLO
static void	db_error (status_$t);
#else
#if (defined WIN_NT || defined mpexl || defined OS2_ONLY)
static void	db_error (SLONG);
#else
static void	db_error (int);
#endif
#endif
static DBA_FIL	db_open (UCHAR *, USHORT);
static PAG	db_read (SLONG);
static void	db_close (int);
static BOOLEAN	key_equality (SSHORT, SCHAR *, BTN);
static void	move (SCHAR *, SCHAR *, SSHORT);
static void	print_distribution (SCHAR *, SLONG *);
static void	truncate_name (SCHAR *);
static void	dba_error (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
static void	dba_print (USHORT, TEXT *, TEXT *, TEXT *, TEXT *, TEXT *);
#ifdef APOLLO
static int	map_length, map_count;
#endif

typedef struct blk {
    UCHAR	blk_type;
    UCHAR	blk_pool_id_mod;
    USHORT	blk_length;
} *BLK;

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"

#ifdef SUPERSERVER
#include <fcntl.h>
#if (defined WIN_NT || defined NETWARE_386)
#include <share.h>
#endif
#ifdef NETWARE_386
#include <fileengd.h>
#endif

#include "../jrd/pwd.h"
#include "../utilities/ppg_proto.h"

#define FPRINTF		SVC_fprintf

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
    DBA_FIL	files;
    REL	        relations;
    SSHORT	page_size;
    SLONG	page_number;
    PAG	        buffer1;
    PAG	        buffer2;
    PAG	        global_buffer;
    int         exit_code;
#ifdef SUPERSERVER
    SVC	        sw_outfile;
    mem         *head_of_mem_list;
    open_files  *head_of_files_list;
    SVC		dba_service_blk;
#else
    IB_FILE	*sw_outfile;
#endif
    STATUS	*dba_status;
    STATUS	dba_status_vector[ISC_STATUS_LENGTH];
} *TDBA;

#ifdef GET_THREAD_DATA
#undef GET_THREAD_DATA
#endif

#ifdef SUPERSERVER
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

#define GSTAT_MSG_FAC	21

#if defined (WIN95) && !defined (GUI_TOOLS)
static BOOLEAN  fAnsiCP = FALSE;
#define TRANSLATE_CP(a) if (!fAnsiCP) CharToOem(a, a)
#else
#define TRANSLATE_CP(a)
#endif



#ifdef SUPERSERVER
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
IN_SW_TAB	in_sw_tab;
REL	relation;
DBA_IDX	index;
HDR	header;
LIP	logp;
DBA_FIL	current;
SCHAR	**end, *name, *p, temp [1024],file_name [1024];
double	average;
BOOLEAN	sw_system, sw_data, sw_index, sw_version, sw_header, sw_log, sw_record, sw_relation;
SLONG	page;
SLONG	redir_in, redir_out, redir_err;
UCHAR	*vp, *vend;
isc_db_handle db_handle = NULL;
#ifdef SUPERSERVER
SVC	    sw_outfile;
int	argc;
char	**argv;
open_files	*open_file, *tmp1;
mem	*alloced, *tmp2; 
#else
IB_FILE	*sw_outfile;
#endif
TEXT	*q, *str, c;
UCHAR	*dpb;
SSHORT	dpb_length;
UCHAR	dpb_string [256];
UCHAR	buf[256];
UCHAR	pass_buff[128], user_buff[128], *password = pass_buff, *username = user_buff;
struct tdba	thd_context, *tddba;
JMP_BUF		env;
isc_tr_handle   transact1;
isc_req_handle  request1, request2, request3; 
STATUS          *status_vector;
#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
BOOL	fAnsiCP;
#endif

SET_THREAD_DATA;
SVC_PUTSPECIFIC_DATA;
memset (tddba, 0, sizeof(*tddba));
tddba->dba_env = (UCHAR*)env;

#ifdef SUPERSERVER
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
#ifdef SUPERSERVER
	STATUS	*status;
	int	i = 0, j;

	status = tddba->dba_service_blk->svc_status;
	if (status != status_vector)
	    {
	    while (*status && (++i < ISC_STATUS_LENGTH))
		status++;
	    for (j = 0; status_vector[j] && (i < ISC_STATUS_LENGTH); j++, i++)
		*status++ = status_vector[j];
	    }
#endif
        vector = status_vector;
        if (isc_interprete (s, &vector))
	        {
	        FPRINTF (tddba->sw_outfile, "%s\n", s); 
	        s [0] = '-';
	        while (isc_interprete (s + 1, &vector))
	            FPRINTF (tddba->sw_outfile, "%s\n", s);
	        } 
        }

    /* if there still exists a database handle, disconnect from the
     * server
     */
    FINISH;

#ifdef SUPERSERVER
	SVC_STARTED (service);
  	alloced = tddba->head_of_mem_list;
  	while (alloced != 0)
  	    { 
	    free(alloced->memory);
   	    alloced = alloced->mem_next;
	    }

	/* close files */
	open_file = tddba->head_of_files_list;
	while (open_file)
    	{
   	    db_close(open_file->desc); 
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

    /* Mark service thread as finished and cleanup memory being
     * used by service in case if client detached from the service
     */

    SVC_finish (service, SVC_finished);
#endif

    exit_code = tddba->exit_code;
    RESTORE_THREAD_DATA;

#ifdef SUPERSERVER
	return exit_code;
#else
	exit (exit_code);
#endif
    }

#ifdef SUPERSERVER
sw_outfile = tddba->sw_outfile = service;
#else
sw_outfile = tddba->sw_outfile = ib_stdout;
#endif

#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
fAnsiCP = FALSE;
#endif

/* Perform some special handling when run as an Interbase service.  The
   first switch can be "-svc" (lower case!) or it can be "-svc_re" followed
   by 3 file descriptors to use in re-directing ib_stdin, ib_stdout, and ib_stderr. */
tddba->dba_status = tddba->dba_status_vector;
status_vector = tddba->dba_status;

if (argc > 1 && !strcmp (argv [1], "-svc"))
    {
    argv++;
    argc--;
    }
#ifdef SUPERSERVER
else if (!strcmp (argv[1], "-svc_thd"))
    {
    tddba->dba_service_blk = service;
    tddba->dba_status = service->svc_status;
    argv++;
    argc--;
    }
#endif
#ifndef NETWARE_386
else if (argc > 4 && !strcmp (argv [1], "-svc_re"))
    {
    redir_in = atol (argv [2]);
    redir_out = atol (argv [3]);
    redir_err = atol (argv [4]);
#ifdef WIN_NT
#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
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
sw_log = sw_system = sw_data = sw_index = sw_version = sw_header = sw_record = sw_relation = FALSE;

MOVE_CLEAR (user_buff, sizeof (user_buff));
MOVE_CLEAR (pass_buff, sizeof (pass_buff));

for (end = argv + argc, ++argv; argv < end;)
    {
    str = *argv++;
    if (*str == '-')
	{
	if (!str [1])
	    str = "-*NONE*";
	for (in_sw_tab = dba_in_sw_table; q = in_sw_tab->in_sw_name; in_sw_tab++)
	    {
	    for (p = str + 1; c = *p++;)
		if (UPPER (c) != *q++)
		    break;
	    if (!c)
		break;
	    }
	in_sw_tab->in_sw_state = TRUE;
	if (!in_sw_tab->in_sw)
	    {
	    dba_print (20, str + 1, 0, 0, 0, 0); /* msg 20: unknown switch "%s" */
	    dba_print (21, 0, 0, 0, 0, 0); /* msg 21: Available switches: */
	    for (in_sw_tab = dba_in_sw_table; in_sw_tab->in_sw; in_sw_tab++)
		if (in_sw_tab->in_sw_msg)
		    dba_print (in_sw_tab->in_sw_msg, 0, 0, 0, 0, 0);
#ifdef SUPERSERVER
	    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
			   GSTAT_MSG_FAC, 1,
			   0, NULL,
			   0, NULL,
			   0, NULL,
			   0, NULL,
			   0, NULL);
#endif
	    dba_error (1, 0, 0, 0, 0, 0); /* msg 1: found unknown switch */
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_USERNAME)
	    {
	    strcpy (username, *argv++);
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_PASSWORD)
	    {
	    strcpy (password, *argv++);
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_SYSTEM)
	    {
	    sw_system = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_DATA)
	    {
	    sw_data = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_INDEX)
	    {
	    sw_index = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_VERSION)
	    {
	    sw_version = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_HEADER)
	    {
	    sw_header = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_LOG)
	    {
	    sw_log = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_DATAIDX)
	    {
	    sw_index = sw_data = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_RECORD)
	    {
	    sw_record = TRUE;
	    }
	else if (in_sw_tab->in_sw == IN_SW_DBA_RELATION)
	    {
	    sw_relation = TRUE;
	    while (argv < end && **argv != '-')
		{
		REL	*next;

		if (strlen (*argv) > 31)
		    {
		    argv++;
		    continue;
		    }
	    	relation = (REL) ALLOC (sizeof (struct rel));
    	    	strcpy (relation->rel_name, *argv++);
    	    	truncate_name (relation->rel_name);
		relation->rel_id = -1;
		for (next = &tddba->relations; *next; next = &(*next)->rel_next);
    	    	*next = relation;
		}
	    }
	}
    else
	name = str;
    }

if (!name)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
		   GSTAT_MSG_FAC, 2,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL);
#endif
    dba_error (2, 0, 0, 0, 0, 0); /* msg 2: please retry, giving a database name */
    }

if (!sw_data && !sw_index)
    sw_data = sw_index = TRUE;

if (sw_record && !sw_data)
    sw_data = TRUE;

#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
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
    dba_print (5, GDS_VERSION, 0, 0, 0, 0); /* msg 5: gstat version %s */

/* Open database and go to work */



    
current = db_open (name, strlen (name));
tddba->page_size = sizeof (temp);
tddba->global_buffer = (PAG) temp;
tddba->page_number = -1;
header = (HDR) db_read ((SLONG) 0);

#ifdef SUPERSERVER
SVC_STARTED (service);
#endif

/* ODS7 was not released as a shipping ODS and was replaced
 * by ODS 8 in version 4.0 of InterBase.  There will not be any
 * customer databases which have an ODS of 7 */

if (header->hdr_ods_version > ODS_VERSION ||
   (header->hdr_ods_version < ODS_VERSION8 &&
    header->hdr_ods_version != ODS_VERSION6))
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
		   GSTAT_MSG_FAC, 3,
		   isc_arg_number, ODS_VERSION,
		   isc_arg_number, header->hdr_ods_version,
		   0, NULL,
		   0, NULL,
		   0, NULL);
#endif
    dba_error (3, (TEXT *)ODS_VERSION, (TEXT *)header->hdr_ods_version,
	       0, 0, 0); /* msg 3: Wrong ODS version, expected %d, encountered %d? */
    }

#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
if (!fAnsiCP)
    AnsiToOem(name, file_name);
else
#endif
    strcpy(file_name, name);

dba_print (6, file_name, 0, 0, 0, 0); /* msg 6: \nDatabase \"%s\"\n */

tddba->page_size = header->hdr_page_size;
#ifdef APOLLO
map_count = map_length / tddba->page_size;
#endif /* APOLLO */
tddba->buffer1 = (PAG) ALLOC (tddba->page_size);
tddba->buffer2 = (PAG) ALLOC (tddba->page_size);
tddba->global_buffer = (PAG) ALLOC (tddba->page_size);
tddba->page_number = -1;

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
#if defined (WIN95) && !defined (GUI_TOOLS) && !defined (SUPERSERVER)
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

dba_print (7, 0, 0, 0, 0, 0); /* msg 7: \n\nDatabase file sequence: */
for (current = tddba->files; current->fil_next; current = current->fil_next)
    dba_print (8, current->fil_string, current->fil_next->fil_string, 0,
	       0, 0); /* msg 8: File %s continues as file %s */
dba_print (9, current->fil_string, (current == tddba->files) ? "only" : "last",
	   0, 0, 0); /* msg 9: File %s is the %s file\n */

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

/* Check to make sure that the user accessing the database is either
 * SYSDBA or owner of the database */
dpb = dpb_string;
*dpb++ = gds__dpb_version1;
*dpb++ = isc_dpb_gstat_attach;
*dpb++ = 0;

if (*username)
    {
    *dpb++ = gds__dpb_user_name;
    *dpb++ = strlen (username);
    strcpy (dpb, username);
    dpb += strlen (username);
}

if (*password)
    {
#ifdef SUPERSERVER
    *dpb++ = gds__dpb_password_enc;
#else
    *dpb++ = gds__dpb_password;
#endif
    *dpb++ = strlen (password);
    strcpy (dpb, password);
    dpb += strlen (password);
    }

dpb_length = dpb - dpb_string;

isc_attach_database (status_vector, 0, GDS_VAL(name), &DB, dpb_length, dpb_string);
if (status_vector [1])
    EXIT (FINI_ERROR);

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
    if (sw_relation)
	{
	truncate_name (X.RDB$RELATION_NAME);
	for (relation = tddba->relations; relation; relation = relation->rel_next)
	    if (!(strcmp (relation->rel_name, X.RDB$RELATION_NAME)))
		{
	    	relation->rel_id = X.RDB$RELATION_ID;
		break;
		}
	if (!relation)
	    continue;
	}
    else
	{
    	relation = (REL) ALLOC (sizeof (struct rel));
    	relation->rel_next = tddba->relations;
    	tddba->relations = relation;
    	relation->rel_id = X.RDB$RELATION_ID;
    	strcpy (relation->rel_name, X.RDB$RELATION_NAME);
    	truncate_name (relation->rel_name);
	}

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
	    index = (DBA_IDX) ALLOC (sizeof (struct dba_idx));
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

dba_print (10, 0, 0, 0, 0, 0); /* msg 10: \nAnalyzing database pages ...\n */

for (relation = tddba->relations; relation; relation = relation->rel_next)
    {
    if (relation->rel_id == -1)
	continue;
    if (sw_data)
	analyze_data (relation, sw_record);
    for (index = relation->rel_indexes; index; index = index->idx_next)
	analyze_index (relation, index);
    }

/* Print results */

for (relation = tddba->relations; relation; relation = relation->rel_next)
    {
    if (relation->rel_id == -1)
	continue;
    FPRINTF (sw_outfile, "%s (%d)\n", relation->rel_name, relation->rel_id);
    if (sw_data)
	{
	dba_print (11, (TEXT *)relation->rel_pointer_page,
		   (TEXT *)relation->rel_index_root, 0, 0, 0); /* msg 11: "    Primary pointer page: %ld, Index root page: %ld" */
	if (sw_record)
	    {
	    average = (relation->rel_records) ?
		(double) relation->rel_record_space / relation->rel_records : 0.0;
	    sprintf(buf,"%.2f", average);
	    FPRINTF (sw_outfile, "    Average record length: %s, total records: %ld\n",
		     buf, relation->rel_records);
/*	    dba_print(18, buf, relation->rel_records,
		  0, 0, 0); */ /* msg 18: "    Average record length: %s, total records: %ld */
	    average = (relation->rel_versions) ?
		(double) relation->rel_version_space / relation->rel_versions : 0.0;
	    sprintf(buf,"%.2f", average);
	    FPRINTF (sw_outfile, "    Average version length: %s, total versions: %ld, max versions: %ld\n",
		     buf, relation->rel_versions, relation->rel_max_versions);
/*	    dba_print(19, buf, relation->rel_versions, relation->rel_max_versions,
		  0, 0); */ /* msg 19: "    Average version length: %s, total versions: %ld, max versions: %ld */
	    }

	average = (relation->rel_data_pages) ?
			(double) relation->rel_total_space * 100 /
				((double) relation->rel_data_pages * (tddba->page_size - DPG_SIZE)) : 0;
	sprintf(buf,"%.0f%%", average);
	dba_print(12, relation->rel_data_pages, relation->rel_slots,
		  buf, 0, 0); /* msg 12: "    Data pages: %ld, data page slots: %ld, average fill: %s */
	dba_print (13, 0, 0, 0, 0, 0); /* msg 13: "    Fill distribution:" */
	print_distribution ("\t", relation->rel_fill_distribution);
	}
    FPRINTF (sw_outfile, "\n");
    for (index = relation->rel_indexes; index; index = index->idx_next)
	{
	dba_print (14, index->idx_name,  (TEXT *)index->idx_id, 0,
		   0, 0); /* msg 14: "    Index %s (%d)" */
	dba_print (15, (TEXT *)index->idx_depth,
		   (TEXT *)index->idx_leaf_buckets,
		   (TEXT *)index->idx_nodes, 0, 0);
		   /* msg 15: \tDepth: %d, leaf buckets: %ld, nodes: %ld */
	average = (index->idx_nodes) ?
	    index->idx_data_length / index->idx_nodes : 0;
	sprintf (buf, "%.2f", average);
	dba_print (16, buf, index->idx_total_duplicates,
		   index->idx_max_duplicates, 0, 0); /* msg 16: \tAverage data length: %s, total dup: %ld, max dup: %ld" */
	dba_print (17, 0, 0, 0, 0, 0); /* msg 17: \tFill distribution: */
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
#ifdef SUPERSERVER
mem *mem_list;
TDBA	tddba;

tddba = GET_THREAD_DATA;

block = p = malloc (size);
if (!p)
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
/* note: shouldn't we check for the NULL?? */
do *p++ = 0; while (--size);

if (!(mem_list = malloc(sizeof(mem))))
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
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
if (!p)
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
do *p++ = 0; while (--size);
#endif

return block;
}

static void analyze_data (
    REL		relation,
    BOOLEAN	sw_record)
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
	    if (!analyze_data_page (relation, db_read (*ptr), sw_record))
		dba_print (18, (TEXT *)*ptr, 0, 0, 0, 0); /* msg 18: "    Expected data on page %ld" */
	    }
	}
    }
}

static BOOLEAN analyze_data_page (
    REL		relation,
    DPG		page,
    BOOLEAN	sw_record)
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
SSHORT		bucket, space;
RHDF		header;
TDBA		tddba;
struct dpg_repeat	*tail, *end;

tddba = GET_THREAD_DATA;

if (page->dpg_header.pag_type != pag_data)
    return FALSE;

if (sw_record)
    {
    move (page, tddba->buffer2, tddba->page_size);
    page = (DPG) tddba->buffer2;
    }

space = page->dpg_count * sizeof (struct dpg_repeat);

for (tail = page->dpg_rpt, end = tail + page->dpg_count; tail < end; tail++)
    if (tail->dpg_offset && tail->dpg_length)
	{
	space += tail->dpg_length;
	if (sw_record)
	    {
	    header = (RHDF) ((SCHAR*) page + tail->dpg_offset);
	    if (!(header->rhdf_flags & (rhd_blob | rhd_chain | rhd_fragment)))
	    	{
	    	++relation->rel_records;
	    	relation->rel_record_space += tail->dpg_length;
		if (header->rhdf_flags & rhd_incomplete)
		    {
		    relation->rel_record_space -= RHDF_SIZE;
		    relation->rel_record_space += analyze_fragments (relation, header);
		    }
		else
		    relation->rel_record_space -= RHD_SIZE;

		if (header->rhdf_b_page)
		    relation->rel_version_space += analyze_versions (relation, header);
		}
	    }
	}

relation->rel_total_space += space;
bucket = (space * BUCKETS) / (tddba->page_size - DPG_SIZE);

if (bucket == BUCKETS)
    --bucket;

++relation->rel_fill_distribution [bucket];

return TRUE;
}

static ULONG analyze_fragments (
    REL		relation,
    RHDF	header)
{
/**************************************
 *
 *	a n a l y z e _ f r a g m e n t s
 *
 **************************************
 *
 * Functional description
 *	Analyze space used by a record's fragments.
 *
 **************************************/
ULONG		space;
SLONG		f_page;
USHORT		f_line;
TDBA		tddba;
DPG		page;
struct dpg_repeat	*index;

tddba = GET_THREAD_DATA;
space = 0;

while (header->rhdf_flags & rhd_incomplete)
    {
    f_page = header->rhdf_f_page;
    f_line = header->rhdf_f_line;
    page = (DPG) db_read (f_page);
    if (page->dpg_header.pag_type != pag_data ||
	page->dpg_relation != relation->rel_id ||
	page->dpg_count <= f_line)
	break;
    index = &page->dpg_rpt [f_line];
    if (!index->dpg_offset)
	break;
    space += index->dpg_length;
    space -= RHDF_SIZE;
    header = (RHDF) ((SCHAR*) page + index->dpg_offset);
    }

return space;
}

static void analyze_index (
    REL		relation,
    DBA_IDX		index)
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
BTN	node;
SSHORT	space, duplicates, key_length, l, dup, n;
UCHAR	key [256], *p, *q;
SLONG	number, page;
TDBA	tddba;

tddba = GET_THREAD_DATA;

index_root = (IRT) db_read (relation->rel_index_root);

if (index_root->irt_count <= index->idx_id ||
    !(page = index_root->irt_rpt [index->idx_id].irt_root))
    return;

bucket = (BTR) db_read (page);
index->idx_depth = bucket->btr_level + 1;

while (bucket->btr_level)
    {
    move (bucket->btr_nodes[0].btn_number, &page, sizeof (page));
    bucket = (BTR) db_read (page);
    }

duplicates = 0;
key_length = 0;

for (;;)
    {
    ++index->idx_leaf_buckets;
    for (node = bucket->btr_nodes;; node = (BTN) (node->btn_data + node->btn_length))
	{
	move (node->btn_number, &number, sizeof (number));
#ifdef IGNORE_NULL_IDX_KEY
	/* ignore this node and go to the next one */
	if (number == END_NON_NULL)
	    continue;
#endif  /* IGNORE_NULL_IDX_KEY */

	if (number == END_LEVEL || number == END_BUCKET)
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
	key_length = l;
	if (l = node->btn_length)
	    {
	    p = key + node->btn_prefix;
	    q = node->btn_data;
	    do *p++ = *q++; while (--l);
	    }	    	
	}
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
	dba_print (19, (TEXT *)page, (TEXT *)number, 0, 0, 0);
	/* mag 19: "    Expected b-tree bucket on page %ld from %ld" */
	break;
	}
    }
}

static ULONG analyze_versions (
    REL		relation,
    RHDF	header)
{
/**************************************
 *
 *	a n a l y z e _ v e r s i o n s
 *
 **************************************
 *
 * Functional description
 *	Analyze space used by a record's back versions.
 *
 **************************************/
ULONG		space, versions;
SLONG		b_page;
USHORT		b_line;
TDBA		tddba;
DPG		page;
struct dpg_repeat	*index;

tddba = GET_THREAD_DATA;
space = versions = 0;
b_page = header->rhdf_b_page;
b_line = header->rhdf_b_line;

while (b_page)
    {
    page = (DPG) db_read (b_page);
    if (page->dpg_header.pag_type != pag_data ||
	page->dpg_relation != relation->rel_id ||
	page->dpg_count <= b_line)
	break;
    index = &page->dpg_rpt [b_line];
    if (!index->dpg_offset)
	break;
    space += index->dpg_length;
    ++relation->rel_versions;
    ++versions;
    header = (RHDF) ((SCHAR*) page + index->dpg_offset);
    b_page = header->rhdf_b_page;
    b_line = header->rhdf_b_line;
    if (header->rhdf_flags & rhd_incomplete)
	{
	space -= RHDF_SIZE;
	space += analyze_fragments (relation, header);
	}
    else
	space -= RHD_SIZE;
    }

if (versions > relation->rel_max_versions)
    relation->rel_max_versions = versions;

return space;
}

#ifdef NETWARE_386
static void db_close (
    int file_desc)
{
/**************************************
 *
 *	d b _ c l o s e		( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *    Close an open file
 *
 **************************************/
 close (file_desc);
}

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
tddba->page_number = -1;

FPRINTF (tddba->sw_outfile, "%s\n", sys_errlist [status]);
EXIT (FINI_ERROR);
}

static DBA_FIL db_open (
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
DBA_FIL	fil;
open_files *file_list;
TEXT expanded_filename[256];
TDBA	tddba;

tddba = GET_THREAD_DATA;

ISC_expand_filename (file_name, file_length, expanded_filename);

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (expanded_filename) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (expanded_filename) + 1);
    MOVE_CLEAR (tddba_files, sizeof (struct dba_fil));
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, expanded_filename);
fil->fil_length = strlen (expanded_filename);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

#ifdef READONLY_DATABASE
if ((fil->fil_desc = FEsopen (expanded_filename, O_RDONLY | O_BINARY, SH_DENYNO, 0, 0, 
                          PrimaryDataStream)) == -1)
#else
if ((fil->fil_desc = FEsopen (expanded_filename, O_RDWR | O_BINARY, SH_DENYNO, 0, 0, 
                          PrimaryDataStream)) == -1)
#endif  /* READONLY_DATABASE */
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
		   GSTAT_MSG_FAC, 29,
		   isc_arg_string, expanded_filename,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL); /* msg 29: Can't open database file %s */
#endif
    db_error (errno);
    }

if (!(file_list = malloc(sizeof(tddba->files))))
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (lseek (fil->fil_desc, page_number * tddba->page_size, 0) == -1)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 30,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 30: Can't read a database page */
#endif
    db_error (errno);
    }

for (p = (SCHAR*) tddba->global_buffer, length = tddba->page_size; length > 0;)
    {
    l = read (fil->fil_desc, p, length);
    if (l <= 0)
	{
#ifdef SUPERSERVER
	CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
			GSTAT_MSG_FAC, 30,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL); /* msg 30: Can't read a database page */
#endif
	db_error (errno);
	}
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
TDDBA	tddba;

tddba = GET_THREAD_DATA;
tddba->page_number = -1;

error_$print (status);
EXIT (FINI_ERROR);
}

static DBA_FIL db_open (
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
DBA_FIL		fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
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
DBA_FIL		fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

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

tddba->global_buffer = (PAG) (fil->fil_region + (page_number - fil->fil_base) * tddba->page_size);
return tddba->global_buffer;
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
tddba->page_number = -1;

if (DosGetMessage (0, 0, s, sizeof (s), status, "OSO001.MSG", &len))
    sprintf (s, "unknown OS/2 error %ld", status);

FPRINTF (tddba->sw_outfile, "%s\n", s);
EXIT (FINI_ERROR);
}

static DBA_FIL db_open (
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
DBA_FIL	fil;
ULONG	action, open_mode, status;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (status = DosSetFilePtr (fil->fil_desc, (ULONG) (page_number * tddba->page_size), FILE_BEGIN, &new_offset))
    db_error (status);

if (status = DosRead (fil->fil_desc, tddba->global_buffer, tddba->page_size, &actual_length))
    db_error (status);
if (actual_length != tddba->page_size)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
		   GSTAT_MSG_FAC, 4,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL);
#endif
    dba_error (4, 0, 0, 0, 0, 0); /* msg 4: Unexpected end of database file. */
    }

return tddba->global_buffer;
}
#endif

#ifdef WIN_NT
static void db_close (
    int file_desc)
{
/**************************************
 *
 *	d b _ c l o s e ( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *    Close an open file
 *
 **************************************/
 CloseHandle ((HANDLE)file_desc);
}

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
tddba->page_number = -1;

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

static DBA_FIL db_open (
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
DBA_FIL	fil;
TDBA	tddba;
#ifdef SUPERSERVER
open_files *file_list;
#endif

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

if ((fil->fil_desc = CreateFile (file_name,
#ifdef READONLY_DATABASE
	GENERIC_READ,
#else
	GENERIC_READ | GENERIC_WRITE,
#endif  /* READONLY_DATABASE */
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_EXISTING,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
	0)) == INVALID_HANDLE_VALUE)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 29,
		    isc_arg_string, file_name,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 29: Can't open database file %s */
#endif
    db_error (GetLastError());
    }

#ifdef SUPERSERVER
if (!(file_list = malloc(sizeof(tddba->files))))
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
file_list->desc = fil->fil_desc;
file_list->open_files_next = 0;

if (tddba->head_of_files_list == 0)
    tddba->head_of_files_list = file_list;
else
   {
   file_list->open_files_next = tddba->head_of_files_list;
   tddba->head_of_files_list = file_list;
   }
#endif

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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (SetFilePointer (fil->fil_desc, (ULONG) (page_number * tddba->page_size), NULL, FILE_BEGIN) == -1)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 30,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 30: Can't read a database page */
#endif
    db_error (GetLastError());
    }

if (!ReadFile (fil->fil_desc, tddba->global_buffer, tddba->page_size, &actual_length, NULL))
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 30,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 30: Can't read a database page */
#endif
    db_error (GetLastError());
    }
if (actual_length != tddba->page_size)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status(tddba->dba_service_blk->svc_status,
		   GSTAT_MSG_FAC, 4,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL,
		   0, NULL);
#endif
    dba_error (4, 0, 0, 0, 0, 0); /* msg 4: Unexpected end of database file. */
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
tddba->page_number = -1;

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

static DBA_FIL db_open (
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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

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

static void db_close (
    int file_desc)
{
/**************************************
 *
 *	d b _ c l o s e
 *
 **************************************
 *
 * Functional description
 *    Close an open file
 *
 **************************************/
 close (file_desc);
}

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
tddba->page_number = -1;

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

static DBA_FIL db_open (
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
DBA_FIL	fil;
TDBA	tddba;
#ifdef SUPERSERVER
open_files *file_list;
#endif

tddba = GET_THREAD_DATA;

if (tddba->files)
    {
    for (fil = tddba->files; fil->fil_next; fil = fil->fil_next)
	;
    fil->fil_next = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_next->fil_min_page = fil->fil_max_page+1;
    fil = fil->fil_next;
    }
else /* empty list */
    {
    fil = tddba->files = (DBA_FIL) ALLOC (sizeof (struct dba_fil) + strlen (file_name) + 1);
    fil->fil_min_page = 0L;
    }

fil->fil_next = NULL;
strcpy (fil->fil_string, file_name);
fil->fil_length = strlen (file_name);
fil->fil_fudge = 0;
fil->fil_max_page = 0L;

#ifdef READONLY_DATABASE
if ((fil->fil_desc = open (file_name, O_RDONLY)) == -1)
#else
if ((fil->fil_desc = open (file_name, 2)) == -1)
#endif  /* READONLY_DATABASE */
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 29,
		    isc_arg_string, file_name,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 29: Can't open database file %s */
#endif
    db_error (errno);
    }

#ifdef SUPERSERVER
if (!(file_list = malloc(sizeof(tddba->files))))
    {
    /* NOMEM: return error */
    dba_error (31, 0, 0, 0, 0, 0);
    }
file_list->desc = fil->fil_desc;
file_list->open_files_next = 0;

if (tddba->head_of_files_list == 0)
    tddba->head_of_files_list = file_list;
else
   {
   file_list->open_files_next = tddba->head_of_files_list;
   tddba->head_of_files_list = file_list;
   }
#endif

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
DBA_FIL	fil;
TDBA	tddba;

tddba = GET_THREAD_DATA;

if (tddba->page_number == page_number)
    return tddba->global_buffer;

tddba->page_number = page_number;

for (fil = tddba->files; page_number > fil->fil_max_page && fil->fil_next;)
    fil = fil->fil_next;

page_number -= fil->fil_min_page - fil->fil_fudge;
if (lseek (fil->fil_desc, page_number * tddba->page_size, 0) == -1)
    {
#ifdef SUPERSERVER
    CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
		    GSTAT_MSG_FAC, 30,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL,
		    0, NULL); /* msg 30: Can't read a database page */
#endif
    db_error (errno);
    }
    
for (p = (SCHAR*) tddba->global_buffer, length = tddba->page_size; length > 0;)
    {
    l = read (fil->fil_desc, p, length);
    if(l < 0)
	{
#ifdef SUPERSERVER
	CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
			GSTAT_MSG_FAC, 30,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL); /* msg 30: Can't read a database page */
#endif
	db_error (errno);
	}
    if(!l)
	{
#ifdef SUPERSERVER
	CMD_UTIL_put_svc_status (tddba->dba_service_blk->svc_status,
			GSTAT_MSG_FAC, 4,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL,
			0, NULL);
#endif
	dba_error (4, 0, 0, 0, 0, 0); /* msg 4: Unexpected end of database file. */
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

static void dba_error (
    USHORT	errcode,
    TEXT	*arg1,
    TEXT	*arg2,
    TEXT	*arg3,
    TEXT	*arg4,
    TEXT	*arg5)
{
/**************************************
 *
 *	d b a _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Format and print an error message, then punt.
 *
 **************************************/
TDBA    tddba;

tddba = GET_THREAD_DATA;
tddba->page_number = -1;

dba_print (errcode, arg1, arg2, arg3, arg4, arg5);
EXIT (FINI_ERROR);
}

static void dba_print (
    USHORT	number,
    TEXT        *arg1,
    TEXT        *arg2,
    TEXT        *arg3,
    TEXT        *arg4,
    TEXT        *arg5)
{
/**************************************
 *
 *	d b a _ p r i n t
 *
 **************************************
 *
 * Functional description
 *      Retrieve a message from the error file, format it, and print it.
 *
 **************************************/
TEXT    buffer [256];
TDBA    tddba;

tddba = GET_THREAD_DATA;

gds__msg_format (NULL_PTR, GSTAT_MSG_FAC, number, sizeof (buffer), buffer,
		 arg1, arg2, arg3, arg4, arg5);
TRANSLATE_CP(buffer);
FPRINTF(tddba->sw_outfile, "%s\n", buffer);
}

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

memcpy (to, from, (int) length);
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

