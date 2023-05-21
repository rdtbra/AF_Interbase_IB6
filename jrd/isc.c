/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         isc.c
 *      DESCRIPTION:    General purpose but non-user routines.
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "../jrd/common.h"

#ifdef WIN_NT
#include <windows.h>
#endif

#include "../jrd/codes.h"
#include "../jrd/isc.h"
#include "../jrd/ibase.h"
#include "../jrd/jrd.h"
#include "../jrd/scl.h"
#include "../jrd/flu_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/jrd_proto.h"

/* Initialize the structure that hold all the config information */

static struct cfgtbl ISC_def_cfg_tbl[] = {
    ISCCFG_LOCKMEM, ISCCFG_LOCKMEM_KEY, ISCCFG_LOCKMEM_DEF, ISCCFG_LOCKMEM_DEF,
    ISCCFG_LOCKSEM, ISCCFG_LOCKSEM_KEY, ISCCFG_LOCKSEM_DEF, ISCCFG_LOCKSEM_DEF,
    ISCCFG_LOCKSIG, ISCCFG_LOCKSIG_KEY, ISCCFG_LOCKSIG_DEF, ISCCFG_LOCKSIG_DEF,
    ISCCFG_EVNTMEM, ISCCFG_EVNTMEM_KEY, ISCCFG_EVNTMEM_DEF, ISCCFG_EVNTMEM_DEF,
    ISCCFG_DBCACHE, ISCCFG_DBCACHE_KEY, ISCCFG_DBCACHE_DEF, ISCCFG_DBCACHE_DEF,
    ISCCFG_PRIORITY,ISCCFG_PRIORITY_KEY,ISCCFG_PRIORITY_DEF,ISCCFG_PRIORITY_DEF,
    ISCCFG_IPCMAP,  ISCCFG_IPCMAP_KEY,  ISCCFG_IPCMAP_DEF,  ISCCFG_IPCMAP_DEF,
    ISCCFG_MEMMIN,  ISCCFG_MEMMIN_KEY,  ISCCFG_MEMMIN_DEF,  ISCCFG_MEMMIN_DEF,
    ISCCFG_MEMMAX,  ISCCFG_MEMMAX_KEY,  ISCCFG_MEMMAX_DEF,  ISCCFG_MEMMAX_DEF,
    ISCCFG_LOCKORDER, ISCCFG_LOCKORDER_KEY, ISCCFG_LOCKORDER_DEF, ISCCFG_LOCKORDER_DEF,
    ISCCFG_ANYLOCKMEM, ISCCFG_ANYLOCKMEM_KEY, ISCCFG_ANYLOCKMEM_DEF, ISCCFG_ANYLOCKMEM_DEF,
    ISCCFG_ANYLOCKSEM, ISCCFG_ANYLOCKSEM_KEY, ISCCFG_ANYLOCKSEM_DEF, ISCCFG_ANYLOCKSEM_DEF,
    ISCCFG_ANYLOCKSIG, ISCCFG_ANYLOCKSIG_KEY, ISCCFG_ANYLOCKSIG_DEF, ISCCFG_ANYLOCKSIG_DEF,
    ISCCFG_ANYEVNTMEM, ISCCFG_ANYEVNTMEM_KEY, ISCCFG_ANYEVNTMEM_DEF, ISCCFG_ANYEVNTMEM_DEF,
    ISCCFG_LOCKHASH, ISCCFG_LOCKHASH_KEY, ISCCFG_LOCKHASH_DEF, ISCCFG_LOCKHASH_DEF,
    ISCCFG_DEADLOCK, ISCCFG_DEADLOCK_KEY, ISCCFG_DEADLOCK_DEF, ISCCFG_DEADLOCK_DEF,
    ISCCFG_LOCKSPIN, ISCCFG_LOCKSPIN_KEY, ISCCFG_LOCKSPIN_DEF, ISCCFG_LOCKSPIN_DEF,
    ISCCFG_CONN_TIMEOUT, ISCCFG_CONN_TIMEOUT_KEY, ISCCFG_CONN_TIMEOUT_DEF, ISCCFG_CONN_TIMEOUT_DEF,
    ISCCFG_DUMMY_INTRVL, ISCCFG_DUMMY_INTRVL_KEY, ISCCFG_DUMMY_INTRVL_DEF, ISCCFG_DUMMY_INTRVL_DEF,
    ISCCFG_TRACE_POOLS, ISCCFG_TRACE_POOLS_KEY, ISCCFG_TRACE_POOLS_DEF, ISCCFG_TRACE_POOLS_DEF,
    ISCCFG_REMOTE_BUFFER, ISCCFG_REMOTE_BUFFER_KEY, ISCCFG_REMOTE_BUFFER_DEF, ISCCFG_REMOTE_BUFFER_DEF,
    NULL, 0, 0, 0
};

static struct cfgtbl *ISC_cfg_tbl = NULL;

/* Temporary file management specific stuff */

#include "../jrd/fil.h"
#include "../jrd/dls_proto.h"

static BOOLEAN	dls_init = FALSE;
static BOOLEAN	dls_flag = FALSE;
static BOOLEAN	fdls_init = FALSE;
static BOOLEAN	fdls_flag = FALSE;


/* End of temporary file management specific stuff */

#ifdef sparc
#ifdef SOLARIS
#include <sys/utsname.h>
#endif
#endif

#ifdef SCO_EV
#include <sys/utsname.h>
#endif

#ifdef SUN3_3
typedef unsigned short  uid_t;
#endif

static TEXT     user_name [256];
static USHORT   ast_count;

/* VMS Specific Stuff */

#ifdef VMS

#include <rms.h>
#include <descrip.h>
#include <ssdef.h>
#include <jpidef.h>
#include <prvdef.h>
#include <secdef.h>
#include <lckdef.h>
#include "../jrd/lnmdef.h"
#include "../jrd/prv_m_bypass.h"
#include <signal.h>

#define LOGICAL_NAME_TABLE      "LNM$FILE_DEV"
#define WAKE_LOCK               "gds__process_%d"

static POKE     pokes;
static LKSB     wake_lock;
#endif /* of ifdef VMS */

/* Unix specific stuff */

#if (defined APOLLO || defined UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pwd.h>

#ifndef NeXT
#ifndef APOLLO
#include <unistd.h>
#endif
#endif

#ifndef O_RDWR
#include <fcntl.h>
#endif

#ifdef DGUX
#include <fcntl.h>
#endif

#ifdef DELTA
#include <sys/sysmacros.h>
#include <sys/param.h>
#endif

#ifdef IMP
typedef unsigned int    mode_t;
typedef int             pid_t;
#endif

extern struct passwd    *getpwnam(), *getpwuid();
#endif

/* Windows NT */

#if (defined (WIN_NT) || defined (WINDOWS_ONLY))

#include <windows.h>
#define TEXT            SCHAR

#ifdef WIN_NT
#include <process.h>
#include <signal.h>
static USHORT                   os_type;
static SECURITY_ATTRIBUTES      security_attr;
#define ROOTCHECK       "/intrbase/"
#else
#define ROOTCHECK       "c:/interbas/"
#endif /* WIN_NT */

#define MAXPATHLEN      512

static TEXT                     interbase_directory [MAXPATHLEN];
#endif

/* MS/DOS / OS2 Junk */

#ifdef OS2_ONLY

#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>

static PFN              servergetinfo;
#endif

/* HP MPE/XL specific stuff */

#ifdef mpexl

#include <fcntl.h>
#include <mpe.h>
#include "../jrd/mpexl.h"

#define FOPEN_READ_TYPE "r Tm"
#define FOPEN_WRITE_TYPE "w Tm"

static SSHORT   privmode_flag;
#endif

/* Apollo specific Stuff */

#ifdef APOLLO

#define aclm_$sid_tn    140

typedef SCHAR   *aclm_$iptr;
typedef SCHAR   aclm_$sid [aclm_$sid_tn];

std_$call aclm_$iptr    aclm_$init();
std_$call void          aclm_$get_sid();
std_$call void          aclm_$rls();
std_$call void          acl_$get_sid();

#include <apollo/base.h>
#include <apollo/pm.h>
#include <apollo/mutex.h>
#include <apollo/task.h>
#include <apollo/loader.h>
#include <apollo/ec2.h>
#include <apollo/ms.h>
#include <apollo/name.h>
#include <apollo/error.h>
#include <sys/signal.h>

typedef struct {
    uid_$t      pers;
    uid_$t      proj;
    uid_$t      org;
    uid_$t      subs;
    uid_$t      node;
} acl_$sid;

extern uid_$t   acl_$locksmith_uid;
#endif

static void	set_config_value (UCHAR, ULONG*, ULONG*);
#ifdef APOLLO
static void     get_po (TEXT *, TEXT *, TEXT *);
static TEXT     *scan_for_dot (TEXT *);
#endif
#ifdef mpexl
static void     privmode_handler (void);
#endif
#ifdef VMS
static void     blocking_ast (void);
static void     poke_ast (POKE);
static int      wait_test (SSHORT *);
#endif
#ifdef WIN_NT
static BOOLEAN  check_user_privilege (void);
#endif

#ifndef MAXHOSTLEN
#define MAXHOSTLEN      64
#endif

#ifndef FOPEN_READ_TYPE
#define FOPEN_READ_TYPE "r"
#define FOPEN_WRITE_TYPE "w"
#endif

#ifdef SHLIB_DEFS
#define strchr          (*_libgds_strchr)
#define geteuid         (*_libgds_geteuid)
#define getpwuid        (*_libgds_getpwuid)
#define _ctype          (*_libgds__ctype)
#define readlink        (*_libgds_readlink)
#define gethostname     (*_libgds_gethostname)
#define endpwent        (*_libgds_endpwent)
#define getegid         (*_libgds_getegid)

extern SCHAR            *strchr();
extern uid_t            geteuid();
extern struct passwd    *getpwuid();
extern SCHAR            _ctype [];
extern int              readlink();
extern void             gethostname();
extern void             endpwent();
extern gid_t            getegid();
#endif

#ifndef REQUESTER
void DLL_EXPORT ISC_ast_enter (void)
{
/**************************************
 *
 *      I S C _ a s t _ e n t e r
 *
 **************************************
 *
 * Functional description
 *      Enter ast mode.
 *
 **************************************/

++ast_count;
}
#endif

#ifndef REQUESTER
void DLL_EXPORT ISC_ast_exit (void)
{
/**************************************
 *
 *      I S C _ a s t _ e x i t
 *
 **************************************
 *
 * Functional description
 *      Enter ast mode.
 *
 **************************************/

--ast_count;
}
#endif

#ifdef mpexl
int ISC_check_privmode (void)
{
/**************************************
 *
 *      I S C _ c h e c k _ p r i v m o d e
 *
 **************************************
 *
 * Functional description
 *      Determine if we can get into priv mode.  If we can,
 *      set a flag to true.  Otherwise set it to false.
 *
 **************************************/
FPTR_INT        user_handler;

privmode_flag = 2;
XSYSTRAP ((SLONG) privmode_handler, (SLONG*) &user_handler);
GETPRIVMODE();
if (privmode_flag == 2 && ccode() == CCG)
    privmode_flag = 1;
XSYSTRAP ((SLONG) user_handler, (SLONG*) &user_handler);
if (privmode_flag == 2)
    GETUSERMODE();

return privmode_flag;
}
#endif

int DLL_EXPORT ISC_check_process_existence (
    SLONG       pid,
    SLONG       xl_pid,
    USHORT      super_user)
{
/**************************************
 *
 *      I S C _ c h e c k _ p r o c e s s _ e x i s t e n c e
 *
 **************************************
 *
 * Functional description
 *      Return TRUE if the indicated process
 *      exists.  FALSE otherwise.
 *
 **************************************/        

#if (defined APOLLO || defined UNIX)
#define CHECK_EXIST
return (kill ((int) pid, 0) == -1 &&
    (errno == ESRCH || (super_user && errno == EPERM)) ? FALSE : TRUE);
#endif

#ifdef mpexl
#define CHECK_EXIST
return AIF_get_pid (pid, xl_pid, NULL) ? FALSE : TRUE;
#endif

#ifdef OS2_ONLY
#define CHECK_EXIST
TEXT    name [40];
HEV     handle;

sprintf (name, "\\SEM32\\interbase_process%u", pid);
handle = 0;
if (DosOpenEventSem (name, &handle))
    return FALSE;

DosCloseEventSem (handle);
return TRUE;
#endif

#ifdef VMS
#define CHECK_EXIST
ULONG   item;

item = JPI$_PID;
return (lib$getjpi (&item, &pid, NULL, NULL, NULL, NULL) == SS$_NONEXPR) ?
    FALSE : TRUE;
#endif

#ifdef WIN_NT
#define CHECK_EXIST
HANDLE  handle;

if (!(handle = OpenProcess (PROCESS_DUP_HANDLE, FALSE, (DWORD) pid)) &&
    GetLastError() != ERROR_ACCESS_DENIED)
    return FALSE;

CloseHandle (handle);
return TRUE;
#endif

#ifndef CHECK_EXIST
return TRUE;
#else
#undef CHECK_EXIST
#endif
}
 
#ifdef VMS
int ISC_expand_logical_once (
    TEXT        *file_name,
    USHORT      file_length,
    TEXT        *expanded_name)
{
/**************************************
 *
 *      I S C _ e x p a n d _ l o g i c a l _ o n c e
 *
 **************************************
 *
 * Functional description
 *      Expand a logical name.  If it doesn't exist, return 0.
 *
 **************************************/
int     attr;
USHORT  l;
TEXT    *p;
ITM     items [2];
struct dsc$descriptor_s desc1, desc2;

if (!file_length)
    file_length = strlen (file_name);

ISC_make_desc (file_name, &desc1, file_length);
ISC_make_desc (LOGICAL_NAME_TABLE, &desc2, sizeof (LOGICAL_NAME_TABLE) - 1);

items[0].itm_length = 256;
items[0].itm_code = LNM$_STRING;
items[0].itm_buffer = expanded_name;
items[0].itm_return_length = &l;

items[1].itm_length = 0;
items[1].itm_code = 0;

attr = LNM$M_CASE_BLIND;

if (!(sys$trnlnm (&attr, &desc2, &desc1, NULL, items) & 1))
    {
    while (file_length--)
	*expanded_name++ = *file_name++;
    *expanded_name = 0;
    return 0;
    }

expanded_name [l] = 0;

return l;
}
#endif

void DLL_EXPORT ISC_get_config (
    TEXT        *config_file,
    IPCCFG      config_table)
{
/*************************************
 *
 *      I S C _ g e t _ c o n f i g
 *
 *************************************
 *
 * Functional description
 *      Read user-defined values for shared memory and
 *      semaphores, and temporary directory list
 *	from a configuration file.
 *
 ************************************/
IPCCFG	h;
struct	cfgtbl *tbl;

if (ISC_cfg_tbl == NULL)
    {
    IB_FILE        *fd;
    TEXT        *p, *q, buf[80];
    TEXT        buffer [256];
    int         n;
    TEXT        dir_name[MAX_PATH_LENGTH];
    ULONG       size;

    gds__prefix(buffer, config_file);
    config_file = buffer;

    /* Read the configuration file into a local buffer.  This
       local buffer knows about all valid configuration options.
       Later requests for configuration information will read it
       from the in-memory copy rather than the disk copy */

    ISC_cfg_tbl = ISC_def_cfg_tbl;
    if (fd = ib_fopen (config_file, FOPEN_READ_TYPE))
        {
	while (p = ib_fgets (buf, sizeof (buf), fd))
            {
            /* The same keyword can be used more than once  for
               temp directory definition hence it should be handled
               in its own way */
            if (strncmp (buf, ISCCFG_TMPDIR, sizeof(ISCCFG_TMPDIR) - 1) == 0)
                {
#ifdef SUPERSERVER
/*  Note that this should be #ifdef SERVER once we implement that flag.
 *  We want this code to execute in classic servers as well as superserver,
 *  but not in client libraries, or in tools which link with a private
 *  copy of our object file, such as alice and dudley.
 *  This needs to be changed in time for the first Classic port of V6.0
 */
                /* there is temp. directory definition */
                if (!dls_init)
                    if ( (n = sscanf(buf + sizeof(ISCCFG_TMPDIR) - 1,
                                     " %ld \"%[^\"]", &size, dir_name)) == 2 )
                        {
                        if (DLS_add_dir (size, dir_name))
                            dls_flag = TRUE;
                        else
                            {
                            gds__log ("Unable to use config parameter %s : No memory ",
				      ISCCFG_TMPDIR);
                            dls_flag = FALSE;
                            }
                        }
#endif
                continue;
                }

            /* The external function directory keyword can also be used
	       more than once, so handle it separately. */
            if ( 0 == strncmp (buf, ISCCFG_EXT_FUNC_DIR,
			       sizeof(ISCCFG_EXT_FUNC_DIR) - 1))
	        {
#ifdef SUPERSERVER
/*  Note that this should be #ifdef SERVER once we implement that flag.
 *  We want this code to execute in classic servers as well as superserver,
 *  but not in client libraries, or in tools which link with a private
 *  copy of our object file, such as alice and dudley.
 *  This needs to be changed in time for the first Classic port of V6.0
 */
		/* There is external function directory definition */
		if (!fdls_init)
		    if ( 1 == sscanf(buf + sizeof(ISCCFG_EXT_FUNC_DIR) - 1,
				     " \"%[^\"]", dir_name) )
		        {
			if ( DLS_add_func_dir (dir_name) )
			    fdls_flag = TRUE;
			else
			    {
			    gds__log ("Unable to use config parameter %s : No memory ",
				     ISCCFG_EXT_FUNC_DIR);
			    fdls_flag = FALSE;
			    }
		        }
#endif
		continue;
		}

            for (tbl = ISC_cfg_tbl; q = tbl->cfgtbl_keyword; tbl++)
		{
		p = buf;

		/* Scan until we see the end of the keyword or
		   a mismatch */

		while (*q && (*p == *q))
		    { p++; q++; };

		/* If we reached the end of the keyword we've got
		   a match!  The remainder of the input line had
		   better be spaces and numbers */

		if (!*q && (isspace (*p) || isdigit (*p)))
		    {
		    tbl->cfgtbl_value = atol (p);
		    break;
		    }
		} /* for */
            } /* while */
        ib_fclose (fd);
        } /* if ib_fopen */
    else
	{
	/* Error opening file. Log a message only if file exists */
	if (errno != ENOENT)
	    gds__log ("Unable to open config file \"%s\": errno = %d",
	              config_file, errno);
	}
    if (!dls_flag)
	{
	/* Temp. directory configuration has not been defined.
	   Let's make default configuration */
	if ( !(q = getenv ("INTERBASE_TMP")) && !(q = getenv ("TMP")) &&
	     !(q = getenv ("TEMP")) )
	    q = WORKDIR;
	if (!DLS_add_dir (ALLROOM, q))
	    {
	    /* we are out of memory, also we can only get here during
	       server start hence let us exit with an exit status indicating
	       to the guardian that the server need not be restarted */
	    gds__log ("Unable to set configuration parameters : No Memory");
	    exit (STARTUP_ERROR);
	    }
	dls_flag = TRUE;
	}
    dls_init = TRUE; /* Temp dir config should be read only once */

    fdls_init = TRUE; /* Ext func dir config should be read only once. */
    }  /* if ICS_cfg_tbl == NULL */

/* The master configuration options are in memory, now for
   this request for option values mark each one as not found
   and search the memory table for each one. */

for (h = config_table; h->ipccfg_keyword; h++)
    h->ipccfg_found = FALSE;

for (h = config_table; h->ipccfg_keyword; h++)
    {
    if (!h->ipccfg_parent_offset ||
	!h [h->ipccfg_parent_offset].ipccfg_found)
        {
	for (tbl = ISC_cfg_tbl; tbl->cfgtbl_keyword; tbl++)
	    {
	    if (!strcmp (tbl->cfgtbl_keyword, h->ipccfg_keyword))
		{
		*h->ipccfg_variable = tbl->cfgtbl_value;
		h->ipccfg_key = tbl->cfgtbl_key;
		h->ipccfg_found = TRUE;
		break;
		}
	    }
	}
    }
}

#ifndef SUPERCLIENT
int DLL_EXPORT ISC_set_config (
    TEXT        *config_file,
    IPCCFG      config_table)
{
/*************************************
 *
 *      I S C _ s e t _ c o n f i g
 *
 *************************************
 *
 * Functional description
 *      Set new values in the configuration file
 *	and make them take affect if possible.
 *
 ************************************/
IB_FILE	*fd = NULL;
IPCCFG	h;
struct	cfgtbl *t;
TEXT	buffer [256];
int	ret = 1;

if (config_file)
    {
    gds__prefix(buffer, config_file);
    config_file = buffer;
    fd = ib_fopen(config_file, FOPEN_WRITE_TYPE);
    }

if ((ISC_cfg_tbl != NULL) && (!config_file || fd))
    {
    ULONG	config_mask = 0;
    ULONG	*valptr = 0;


    ret = 0;

    /* If config_table = NULL then assume we want to restore defaults,
     * otherwise get the users values and use them.
     */

    for (t = ISC_def_cfg_tbl; t->cfgtbl_keyword; t++)
	{
	if (config_table)
	    {
	    for (h = config_table; h->ipccfg_variable; h++)
		if (h->ipccfg_keyword ?
			!strcmp(t->cfgtbl_keyword, h->ipccfg_keyword) :
			h->ipccfg_key == t->cfgtbl_key)
		    {
		    config_mask |= (1 << t->cfgtbl_key);
		    t->cfgtbl_value = *h->ipccfg_variable;
		    break;
		    }
	    }
	else
	    if (t->cfgtbl_value != t->cfgtbl_def_value)
		{
		config_mask |= (1 << t->cfgtbl_key);
		t->cfgtbl_value = t->cfgtbl_def_value;
		}
	}

    /* if one of the working set sizes has been set the make sure they're
     * both set.
     */

    if (config_mask & ((1 << ISCCFG_MEMMIN_KEY) | (1 << ISCCFG_MEMMAX_KEY)))
	config_mask |= ((1 << ISCCFG_MEMMIN_KEY) | (1 << ISCCFG_MEMMAX_KEY));

    for (t = ISC_def_cfg_tbl; config_mask && t->cfgtbl_keyword; t++)
	{
#ifdef SUPERSERVER
	if (config_mask & (1 << t->cfgtbl_key))
	    switch (t->cfgtbl_key)
		{
		case ISCCFG_MEMMIN_KEY:
		    valptr = &(t->cfgtbl_value);
		    break;
		case ISCCFG_MEMMAX_KEY:
		    set_config_value(t->cfgtbl_key, valptr, &(t->cfgtbl_value));
		    break;
		default:
		    set_config_value(t->cfgtbl_key, &(t->cfgtbl_value), NULL);
		    break;
		}
#endif	/* SUPERSERVER */
	if (fd)
	    {
	    char	*p,
			*q,
			buf[80];
	    int		tabs;

	    p = buf;
	    if (t->cfgtbl_value == t->cfgtbl_def_value)
		*p++ = '#';
 	    q = t->cfgtbl_keyword;
	    while (*q)
		*p++ = *q++;
	    tabs = 4 - (p - buf) / 8;
	    while (tabs--)
		*p++ = '\t';
	    *p = '\0';
	    if ( ib_fprintf (fd, "%s%ld\n", buf, t->cfgtbl_value) < 0)
		gds__log ("Error writing config file \"%s\" : errno = %d",
			  config_file, errno);
	    }
	}
    if (fd)
	{
	ib_fclose(fd);
	gds__log ("Config file \"%s\" has been modified.", config_file);
	}
    }
return ret;
}
#endif /* ! SUPERCLIENT */
 
#ifdef WINDOWS_ONLY
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t         ( WINDOWS )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
TEXT    *p, *q;

for (p = " Windows", q = string; *p; q++, p++)
    *q = *p;

*q = 0;

return string;
}
#endif
 
#ifdef DOS_ONLY
#ifndef GET_HOST
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t         ( D O S )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
TEXT    *p, *q;

for (p = " DOS", q = string; *p; q++, p++)
    *q = *p;

*q = 0;

return string;
}
#endif  /* GET_HOST */
#endif  /* DOS_ONLY */

#ifdef mpexl
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
SSHORT  status;

length--;
NSINFO (,,, &status, NS_LOC_NODELEN, &length, NS_LOC_NODENAME, string);
if (!status)
    string [length] = 0;
else
    strcpy (string, "local");

return string;
}
#endif

#ifdef OS2_ONLY
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( O S 2 )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
USHORT  actual_length;

#ifdef OBSOLETE
if (servergetinfo ||
    (servergetinfo = (PFN) ISC_lookup_entrypoint ("NETAPI", "NETSERVERGETINFO", NULL)))
    {
    (*servergetinfo) (NULL, 0, string, length, &actual_length);
    string [actual_length] = 0;
    }
else
#endif
    strcpy (string, "local");

return string;
}
#endif

#if (defined SOLARIS || defined SCO_EV)
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( S O L A R I S )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
struct utsname  name;

if (uname (&name) >= 0)
    strncpy (string, name.nodename, length);
else
    strcpy (string, "local");

return string;
}
#endif

#ifdef VMS
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( V M S )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
TEXT    *p;

if (!ISC_expand_logical_once ("SYS$NODE", sizeof ("SYS$NODE") - 1, string))
    strcpy (string, "local");
else
    {
    p = string;
    if (*p == '_')
	++p;

    for (; *p && *p != ':'; p++)
	if (*p >= 'A' && *p <= 'Z')
	    *p = *p - 'A' + 'a';

    *p = 0;
    }

return string;
}
#endif

#ifdef WIN_NT
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
SLONG   host_len;

host_len = length;
if (GetComputerName (string, &host_len))
    string [host_len] = 0;
else
    strcpy (string, "local");

return string;
}
#endif

#ifdef NETWARE_386
#define GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t         ( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/
SCHAR   buffer [49];

GetFileServerName (0, buffer);

if (strlen (buffer) > (length - 1))
    *string = 0;
else
    strcpy ((SCHAR*) string, buffer);

return string;
}
#endif

#ifndef GET_HOST
TEXT * INTERNAL_API_ROUTINE ISC_get_host (
    TEXT        *string,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ g e t _ h o s t                 ( G E N E R I C )
 *
 **************************************
 *
 * Functional description
 *      Get host name.
 *
 **************************************/

gethostname (string, length);

return string;
}
#endif

#ifdef WIN_NT
BOOLEAN ISC_get_ostype (void)
{
/**************************************
 *
 *      I S C _ g e t _ o s t y p e
 *
 **************************************
 *
 * Functional description
 *      Return the type of OS, TRUE for Windows/NT,
 *      FALSE for Chicago.
 *
 **************************************/
OSVERSIONINFO   OsVersionInfo;

if (!os_type)
    {
    os_type = 1;         /* Default to NT */
    /* The first time this routine is called we use the Windows API
       call GetVersion to determine whether Windows/NT or Chicago
       is running. */

    OsVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx ((LPOSVERSIONINFO) &OsVersionInfo))
	os_type = (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) ? 1 : 2;

    }

return (os_type != 2) ? TRUE : FALSE;
}
#endif

#ifdef WIN_NT
SSHORT ISC_get_registry_var (
    TEXT        *variable,
    TEXT        *buffer,
    SSHORT      buffer_length,
    void        **user_hkey)
{
/**************************************
 *
 *      I S C _ g e t _ r e g i s t r y _ v a r
 *
 **************************************
 *
 * Functional description
 *      Lookup a variable in the NT registry.
 *      Return TRUE if the lookup was successful,
 *      FALSE otherwise.
 *
 **************************************/
HKEY    hkey;
DWORD   len, type;

if (!user_hkey)
    {
    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,
	"SOFTWARE\\Borland\\InterBase\\CurrentVersion",
	0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
	return -1;
    }
else
    hkey = *((PHKEY) user_hkey);

len = buffer_length;
if (RegQueryValueEx (hkey, variable, NULL, &type, buffer, &len) != ERROR_SUCCESS)
    len = 0;

if (!user_hkey)
    RegCloseKey (hkey);

return len;
}
#endif

#ifdef WIN_NT
LPSECURITY_ATTRIBUTES ISC_get_security_desc (void)
{
/**************************************
 *
 *      I S C _ g e t _ s e c u r i t y _ d e s c
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
PSECURITY_DESCRIPTOR    p_security_desc;

if (security_attr.lpSecurityDescriptor)
    return &security_attr;

p_security_desc = (PSECURITY_DESCRIPTOR) gds__alloc ((SLONG) SECURITY_DESCRIPTOR_MIN_LENGTH);
/* FREE: apparently never freed */
if (!p_security_desc)           /* NOMEM: */
    return NULL;
#ifdef DEBUG_GDS_ALLOC
gds_alloc_flag_unfreed ((void *) p_security_desc);
#endif

if (!InitializeSecurityDescriptor (p_security_desc, SECURITY_DESCRIPTOR_REVISION) ||
    !SetSecurityDescriptorDacl (p_security_desc, TRUE, (PACL) NULL, FALSE))
    {
    gds__free (p_security_desc);
    return NULL;
    }

security_attr.nLength = sizeof (security_attr);
security_attr.lpSecurityDescriptor = p_security_desc;
security_attr.bInheritHandle = TRUE;

return &security_attr;
}
#endif

#ifdef APOLLO
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.  If user_string is given, attempt
 *      to get information from there.
 *      User string, if provided, may be of following forms:
 *              1. <name>
 *              3. <name>.<user_id>.<group_id>          (unix flavor)
 *                 <name>.-1.-1 indicates user verification will be needed
 *              4. <name>.<project>.<organization>      (apollo flavor)
 *                 <name>.. indicates user verification will be needed
 *
 *
 **************************************/
name_$pname_t   badf;
SSHORT          badflen;
aclm_$iptr      ip;
acl_$sid        sid;
SSHORT          length;
TEXT            pers [140], *org, *proj, *p, *q, lc_name[40];
int             node_id, egid, euid;
struct passwd   *passwd;
status_$t       status;

if ((p = user_string) && *p)
    {
    if (group)
	*group = -1;
    if (id)
	*id = -1;
    if (node)
	*node = 0;
    /* Get user name */
    for (q = name; *p && *p != '.'; p++)
	if (q)
	    *q++ = UPPER7 (*p);
    if (q)
	*q = 0;
    if(*p)
	p++;
    if (!*p)
	{
	get_po (name, project, organization);
	if (group || id)
	    {
	    for (q = name, p = lc_name; *q; q++, p++)
		*p = LOWWER7 (*q);
	    *p = 0;
	    if (passwd = getpwnam (name))
		{
		if (group)
		    *group = passwd->pw_gid;
		if (id)
		    *id = passwd->pw_uid;
		}
	    }
	return 0;
	}
    else if ((*p == '-') || (*p == '.'))
	{
	/* User will have to be looked up in security database */
	*project = 0;
	*organization = 0;
	return 0;
	}
    else if ((*p >= '0') && (*p <= '9'))
	{
	egid = atoi (p);
	if (group)
	    *group = egid;
	while (*p && (*p != '.'))
	    p++;
	if (*p == '.')
	    {
	    p++;
	    euid = atoi (p);
	    if (id)
		*id = euid;
	    }
	get_po (name, project, organization);
	return 0;
	}
    else
	/* There is more, and it is apollo flavor, use it */
	{
	for (q = project; *p && *p != '.'; p++)
	    if (q)
		*q++ = UPPER7 (*p);
	if (q)
	    *q = 0;
	if (*p)
	    p++;
	for (q = organization; *p && *p != '.'; p++)
	    if (q)
		*q++ = UPPER7 (*p);
	if (q)
	    *q = 0;

	if (group || id)
	    {
	    for (q = name, p = lc_name; *q; q++, p++)
		*p = LOWWER7 (*q);
	    *p = 0;
	    if (passwd = getpwnam (name))
		{
		if (group)
		    *group = passwd->pw_gid;
		if (id)
		    *id = passwd->pw_uid;
		}
	    }

	return (!strcmp (name, "USER") && !strcmp (project, "LOCKSMITH"));
	}
    }
else
    {
    /* Just get the info for the current process */

    ip = aclm_$init(0, 0, badf, &badflen, &status);
    aclm_$get_sid(ip, pers, &length, &status);
    for (p = pers; *p && *p != ' '; p++)
	;
    *p = 0;
    aclm_$rls (ip, &status);

    proj = scan_for_dot (pers);
    org = scan_for_dot (proj);
    sscanf (scan_for_dot (org), "%x", &node_id);

    if (name)
	strcpy (name, pers);

    if (id)
	*id = geteuid();

    if (group)
	*group = getegid();

    if (project)
	{
	strcpy (project, proj);
	if (organization)
	    strcpy (organization, org);
	if (node)
	    *node = node_id;
	}

    acl_$get_sid (sid, &status);

    return (sid.proj.high == acl_$locksmith_uid.high &&
	    sid.proj.low  == acl_$locksmith_uid.low);
    }
}
#endif

#ifdef PC_PLATFORM
#ifndef NETWARE_386
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   D O S
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.  (On a PC,
 *      there's only 1 user anyway...)
 *
 **************************************/

if (name)
    strcpy (name, (user_name [0]) ? user_name : "user");

if (id)
    *id = 0;

if (group)
    *group = 0;

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

return TRUE;
}
#endif
#endif

#ifdef OS2_ONLY
#ifndef NETWARE_386
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( O S 2 )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/

if (name)
    strcpy (name, (user_name [0]) ? user_name : "user");

if (id)
    *id = -1;

if (group)
    *group = -1;

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

return !strcmp (user_name, SYSDBA_USER_NAME);
}
#endif
#endif

#ifdef mpexl
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/
SLONG   capabilities;
TEXT    user [8], account [8], *p, *end;

WHO (NULL, &capabilities, NULL, user, NULL, account, NULL, NULL);

if (name)
    {
    for (p = user, end = p + sizeof (user); p < end && *p != ' ';)
	*name++ = *p++;
    *name++ = '.';
    for (p = account, end = p + sizeof (account); p < end && *p != ' ';)
	*name++ = *p++;
    *name = 0;
    }

if (id)
    *id = 0;

if (group)
    *group = 0;

if (project)
    {
    for (p = user, end = p + sizeof (user); p < end && *p != ' ';)
	*project++ = *p++;
    *project = 0;
    }

if (organization)
    {
    for (p = account, end = p + sizeof (account); p < end && *p != ' ';)
	*organization++ = *p++;
    *organization = 0;
    }

if (node)
    *node = 0;

return (capabilities & (1L << 31));
}
#endif
 
#ifdef UNIX
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( U N I X )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/
/* egid and euid need to be signed, uid_t is unsigned on SUN! */
SLONG           egid, euid; 
TEXT            *p, *q, user_name [256];
struct passwd   *passwd;

if (user_string && *user_string)
    {
    for (p = user_name, q = user_string; (*p = *q++) && *p != '.'; p++)
	;
    *p = 0;
    p = user_name;
    egid = euid = -1;
    if (*q)
	{
	egid = atoi (q);
	while (*q && (*q != '.'))
	    q++;
	if (*q == '.')
	    {
	    q++;
	    euid = atoi (q);
	    }
	}
    }
else
    {
    euid = (SLONG) geteuid();
    egid = (SLONG) getegid();
    passwd = getpwuid (euid);
    if (passwd)
	p = passwd->pw_name;
    else
	p = "";
    endpwent();
    }

if (name)
    strcpy (name, p);

if (id)
    *id = euid;

if (group)
    *group = egid;

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

return (euid == 0);
}
#endif

#ifdef VMS
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( V M S )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/
SLONG   status, privileges[2];
USHORT  uic [2], len0, len1, len2;
TEXT    *p, *q, *end, user_name [256];
ITM     items [4];

if (user_string && *user_string)
    {
    for (p = user_name, q = user_string; (*p = *q++) && *p != '.'; p++)
	;
    *p = 0;
    p = user_name;
    uic[0] = uic[1] = -1;
    if (*q)
	{
	uic[1] = atoi (q);
	while (*q && (*q != '.'))
	    q++;
	if (*q == '.')
	    {
	    q++;
	    uic[0] = atoi (q);
	    }
	}
    privileges[0] = 0;
    if (name)
	{
	for (p = user_name; *p && *p != ' ';)
	    *name++ = *p++;
	*name = 0;
	}
    }
else
    {
    items [0].itm_code = JPI$_UIC;
    items [0].itm_length = sizeof (uic);
    items [0].itm_buffer = uic;
    items [0].itm_return_length = &len0;

    items [1].itm_code = JPI$_USERNAME;
    items [1].itm_length = sizeof (user_name);
    items [1].itm_buffer = user_name;
    items [1].itm_return_length = &len1;

    items [2].itm_code = JPI$_CURPRIV;
    items [2].itm_length = sizeof (privileges);
    items [2].itm_buffer = privileges;
    items [2].itm_return_length = &len2;

    items [3].itm_code = 0;
    items [3].itm_length = 0;

    status = sys$getjpiw (NULL, NULL, NULL, items, NULL, NULL, NULL);

    if (!(status & 1))
	{
	len1 = 0;
	uic [0] = uic [1] = 0;
	}

    user_name [len1] = 0;

    if (name)
	{
	for (p = user_name; *p && *p != ' ';)
	    *name++ = *p++;
	*name = 0;
	}
    }

if (id)
    *id = uic [0];

if (group)
    *group = uic [1];

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

return (privileges[0] & (SLONG) PRV$M_BYPASS);
}
#endif
 
#ifdef WIN_NT
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/
SLONG   i, name_len;

if (id)
    *id = -1;

if (group)
    *group = -1;

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

if (name)
    {
    name [0] = 0;
    name_len = 128;
    if (GetUserName (name, &name_len))
	{
	name [name_len] = 0;

	/* NT user name is case insensitive */

	for (i = 0; i < name_len; i++)
	    name [i] = UPPER7 (name [i]);

/* This check is not internationalized, the security model needs to be
 * reengineered, especially on SUPERSERVER where none of these local
 * user (in process) assumptions are valid.
 */
	if (!strcmp(name, "ADMINISTRATOR"))
	    {
	    if (id)
		*id = 0;

	    if (group)
		*group = 0;
	    }
	}
    }

return check_user_privilege();
}
#endif

#ifdef NETWARE_386
int INTERNAL_API_ROUTINE ISC_get_user (
    TEXT        *name,
    int         *id,
    int         *group,
    TEXT        *project,
    TEXT        *organization,
    int         *node,
    TEXT        *user_string)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r   ( N E T W A R E _ 3 8 6 )
 *
 **************************************
 *
 * Functional description
 *      Find out who the user is.
 *
 **************************************/
SLONG   name_len;

if (id)
    *id = -1;

if (group)
    *group = -1;

if (project)
    *project = 0;

if (organization)
    *organization = 0;

if (node)
    *node = 0;

return !strcmp (name, "administrator");
}
#endif

#ifdef VMS
int ISC_make_desc (
    TEXT        *string,
    struct dsc$descriptor       *desc,
    USHORT      length)
{
/**************************************
 *
 *      I S C _ m a k e _ d e s c
 *
 **************************************
 *
 * Functional description
 *      Make a VMS string descriptor out of a string.  Return 
 *      length of string.
 *
 **************************************/

desc->dsc$b_dtype = DSC$K_DTYPE_T;
desc->dsc$b_class = DSC$K_CLASS_S;
desc->dsc$a_pointer = string;

if (length)
    desc->dsc$w_length = length;
else
    {
    desc->dsc$w_length = 0;
    while (*string++)
	++desc->dsc$w_length;
    }

return desc->dsc$w_length;
}
#endif
SLONG API_ROUTINE ISC_get_prefix (
    TEXT       *passed_string)
{
/**************************************
 *
 *      i s c _ g e t _ p r e f i x   
 *
 **************************************
 *
 * Functional description
 *      Parse the 'H' argument further for 'HL' 'HM' or 'H'
 *
 **************************************/

char  c  = *passed_string;
int arg_type;

switch (UPPER(c))
    {
    case '\0' :
	  arg_type = IB_PREFIX_TYPE;
	  break;

    case 'L' :
	  arg_type = IB_PREFIX_LOCK_TYPE;
          ++passed_string;
	  break;

    case 'M' :
	  arg_type = IB_PREFIX_MSG_TYPE;
          ++passed_string;
	  break;

    default :
          return (-1);
	  break;
    }
    return (gds__get_prefix(arg_type, ++passed_string));
}
void API_ROUTINE ISC_prefix (
    TEXT        *string,
    TEXT        *root)
{
/**************************************
 *
 *      i s c _ $ p r e f i x   
 *
 **************************************
 *
 * Functional description
 *      Wrapper function for gds__prefix.
 *
 **************************************/

gds__prefix(string, root);
return;
}
void API_ROUTINE ISC_prefix_lock (
    TEXT        *string,
    TEXT        *root)
{
/**************************************
 *
 *      i s c _ $ p r e f i x _ l o c k
 *
 **************************************
 *
 * Functional description
 *      Wrapper function for gds__prefix_lock.
 *
 **************************************/

gds__prefix_lock(string, root);
return;
}
void API_ROUTINE ISC_prefix_msg (
    TEXT        *string,
    TEXT        *root)
{
/**************************************
 *
 *      i s c _ $ p r e f i x _ m s g  
 *
 **************************************
 *
 * Functional description
 *      Wrapper function for gds__prefix_msg.
 *
 **************************************/

gds__prefix_msg(string, root);
return;
}

#ifndef REQUESTER
void ISC_set_user (
    TEXT        *string)
{
/**************************************
 *
 *      I S C _ s e t _ u s e r
 *
 **************************************
 *
 * Functional description
 *      Set user name for feeble operating system that don't
 *      support the concept, or support it badly.
 *
 **************************************/

strcpy (user_name, string);
}
#endif

#ifdef VMS
void ISC_wait (
    SSHORT      *iosb,
    SLONG       event_flag)
{
/**************************************
 *
 *      I S C _ w a i t         ( V M S )
 *
 **************************************
 *
 * Functional description
 *      Wait for a asynchronous VMS event.
 *
 **************************************/

if (*iosb)
    return;

/* If we're at AST state, and we're not running under
   a AST invoked thread, wait locally. */

if (ast_count && !gds__ast_active())
    {
    sys$synch (event_flag, iosb);
    return;
    }
	
gds__thread_wait (wait_test, iosb);
}
#endif

#ifdef VMS
void ISC_wake (
    SLONG       process_id)
{
/**************************************
 *
 *      I S C _ w a k e
 *
 **************************************
 *
 * Functional description
 *      Wake up another VMS process.  Process can be local or
 *      remote (but on the same CPU).
 *
 **************************************/
int     status;
POKE    poke;
TEXT    string [32];
struct dsc$descriptor_s desc;

/* Try to do a simple wake.  If this succeeds, we're done. */

status = sys$wake (&process_id, 0);
#ifdef __ALPHA
THREAD_wakeup();
#endif

if (status & 1)
    return;

/* Do wake by siezing a lock, delivering a blocking ast */

/* Find a free poke block to use */

for (poke = pokes; poke; poke = poke->poke_next)
    if (!poke->poke_use_count)
	break;

if (!poke)
    {
    poke = (POKE) gds__alloc ((SLONG) sizeof (struct poke));
    /* FREE: apparently never freed */
    if (!poke)          /* NOMEM: */
	{
	DEV_REPORT ("ISC_wake: out of memory condition");
	return;         /* error handling too difficult here */
	}
    poke->poke_next = pokes;
    pokes = poke;
    }

++poke->poke_use_count;

sprintf (string, WAKE_LOCK, process_id);
ISC_make_desc (string, &desc, 0);

status = sys$enq (
	0,                      /* event flag */
	LCK$K_PWMODE,           /* lock mode */
	&poke->poke_lksb,       /* Lock status block */
	LCK$M_SYSTEM,           /* flags */
	&desc,                  /* resource name */
	0,                      /* parent id */
	poke_ast,               /* ast address */
	poke,                   /* ast argument */
	0,                      /* blocking ast */
	0,                      /* access mode */
	0);
}
#endif

#ifdef VMS
void ISC_wake_init (void)
{
/**************************************
 *
 *      I S C _ w a k e _ i n i t
 *
 **************************************
 *
 * Functional description
 *      Set up to be awakened by another process thru a blocking AST.
 *
 **************************************/
int             status;
TEXT            string [32];
FPTR_INT        master;
struct dsc$descriptor_s desc;

#ifdef GATEWAY
/* See if the shared image knows abouts wake_init.
   If it does, rely on it. */

if (master = ISC_lookup_entrypoint ("GDSSHR", "gds__wake_init", NULL))
    {
    (*master)();
    return;
    }
#endif

/* If we already have lock, we're done */

if (wake_lock.lksb_lock_id)
    return;

sprintf (string, WAKE_LOCK, getpid());
ISC_make_desc (string, &desc, 0);

status = sys$enqw (
	0,                      /* event flag */
	LCK$K_PWMODE,           /* lock mode */
	&wake_lock,             /* Lock status block */
	LCK$M_SYSTEM,           /* flags */
	&desc,                  /* resource name */
	0,                      /* parent id */
	0,                      /* ast address */
	0,                      /* ast argument */
	blocking_ast,           /* blocking ast */
	0,                      /* access mode */
	0);
}
#endif

#ifdef VMS
static void blocking_ast (void)
{
/**************************************
 *
 *      b l o c k i n g _ a s t
 *
 **************************************
 *
 * Functional description
 *      Somebody else is trying to post a lock.
 *
 **************************************/
int     status;

/* Initially down grade the lock to let the other guy complete */

status = sys$enqw (
    0,                  /* event flag */
    LCK$K_NLMODE,       /* lock mode */
    &wake_lock,         /* Lock status block */
    LCK$M_CONVERT,      /* flags */
    0,                  /* resource name */
    0,                  /* parent id */
    0,                  /* ast address */
    0,                  /* ast argument */
    0,                  /* blocking ast */
    0,                  /* access mode */
    0);

/* Wake us up */

status = sys$wake (0, 0);

/* Now try to upgrade lock */

status = sys$enq (
    0,                  /* event flag */
    LCK$K_PRMODE,       /* lock mode */
    &wake_lock,         /* Lock status block */
    LCK$M_CONVERT,      /* flags */
    0,                  /* resource name */
    0,                  /* parent id */
    0,                  /* ast address */
    0,                  /* ast argument */
    blocking_ast,       /* blocking ast */
    0,                  /* access mode */
    0);
}
#endif

#ifdef WIN_NT
static BOOLEAN check_user_privilege (void)
{
/**************************************
 *
 *      c h e c k _ u s e r _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *      Check to see if the user belongs to the
 *      administrator group.  This routine was
 *      adapted from code in routine RunningAsAdminstrator
 *      in \mstools\samples\regmpad\regdb.c.
 *
 **************************************/
HANDLE          tkhandle;
TOKEN_GROUPS    *ptg;
DWORD           token_len;
SLONG           i;
PSID            admin_sid;
BOOLEAN         admin_priv;
SID_IDENTIFIER_AUTHORITY system_sid_authority = SECURITY_NT_AUTHORITY;

/* First we must open a handle to the access token for this thread. */

if (!OpenThreadToken (GetCurrentThread(), TOKEN_QUERY, FALSE, &tkhandle))
    if (GetLastError() == ERROR_NO_TOKEN)
	{
	/* If the thread does not have an access token, we'll examine the
	   access token associated with the process. */

	if (!OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &tkhandle))
	    {
	    CloseHandle( tkhandle);
	    return FALSE;
	    }
	}
    else
	return FALSE;

ptg = NULL;
token_len = 0;
while (TRUE)
    {
    /* Then we must query the size of the group information associated with
       the token.  This is guarenteed to fail the first time through
       because there is no buffer. */

    if (GetTokenInformation (tkhandle, TokenGroups, ptg, token_len, &token_len))
	break;

    /* If there had been a buffer, it's either too small or something
       else is wrong.  Either way, we can dispose of it. */

    if (ptg)
	gds__free (ptg);

    /* Here we verify that GetTokenInformation failed for lack of a large
       enough buffer. */

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
	CloseHandle( tkhandle);
	return FALSE;
	}

    /* Now we allocate a buffer for the group information. */

    if (!(ptg = (TOKEN_GROUPS*) gds__alloc ((SLONG) token_len)))
	{
	CloseHandle( tkhandle);
	return FALSE;           /* NOMEM: */
	}
    /* FREE: earlier in this loop, and at procedure return */
    }

/* Now we must create a System Identifier for the Admin group. */

if (!AllocateAndInitializeSid (&system_sid_authority, 2,
	SECURITY_BUILTIN_DOMAIN_RID,
	DOMAIN_ALIAS_RID_ADMINS,
	0, 0, 0, 0, 0, 0,
	&admin_sid))
    {
    gds__free (ptg);
    CloseHandle( tkhandle);
    return FALSE;
    }

/* Finally we'll iterate through the list of groups for this access
   token looking for a match against the SID we created above. */

admin_priv= FALSE;

for (i = 0; i < ptg->GroupCount; i++)
    if (EqualSid (ptg->Groups [i].Sid, admin_sid))
	{
	admin_priv= TRUE;
	break;
	}

/* Before we exit we must explicity deallocate the SID we created. */

FreeSid (admin_sid);
gds__free (ptg);
CloseHandle( tkhandle);
return admin_priv;
}
#endif

#ifdef APOLLO
static void get_po (
    TEXT        *name,
    TEXT        *proj,
    TEXT        *org)
{
/**************************************
 *
 *      g e t _ p o                     ( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *      Get default project & organizaation for named user.
 *
 **************************************/
IB_FILE    *tmp_file;
TEXT    tmp_file_name [20], *p, *q, *s, buffer[128];

/* This is a kludge to get the default project, organization, for a user */

if (proj)
    strcpy (proj, "NONE");
if (org)
    strcpy (org, "NONE");
strcpy (buffer, "/etc/edrgy -v ");
p = buffer + strlen (buffer);
for (q = name; *q; q++)
    *p++ = LOWWER7 (*q);
*p = 0;
strcat (buffer, " > ");
strcpy (tmp_file_name, "/tmp/gds_sid_XXXXXX");
mktemp (tmp_file_name);
strcat (buffer, tmp_file_name);
strcat (buffer, " 2> /dev/null");

if (system (buffer))
    return;
if (! (tmp_file = ib_fopen (tmp_file_name, "r")))
    return;
ib_fscanf (tmp_file, "%s", buffer);
unlink (tmp_file_name);
ib_fclose (tmp_file);

for (s = buffer, p = name; *p; p++, s++)
    if (*s != LOWWER7 (*p))
break;
if (*p)
    return;

if (*s++ != '[')
    return;

if (*s++ != '.')
    return;

for (p = proj; *s && *s != '.'; s++, p++)
    if (proj)
	*p = UPPER7 (*s);
*p = 0;
if (*s != '.')
    return;
s++;

for (p = org; *s && *s != ']'; s++, p++)
    if (org)
	*p = UPPER7 (*s);
*p = 0;
}
#endif

#ifdef VMS
static void poke_ast (
    POKE        poke)
{
/**************************************
 *
 *      p o k e _ a s t
 *
 **************************************
 *
 * Functional description
 *      Lock to poke event has completed.  Update value block
 *      and deque the lock.
 *
 **************************************/
int     status;
LKSB    *lksb;

lksb = &poke->poke_lksb;
status = sys$deq (lksb->lksb_lock_id, 0, 0, 0);
--poke->poke_use_count;
}
#endif

#ifdef mpexl
static void privmode_handler (void)
{
/**************************************
 *
 *      p r i v m o d e _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *      Handler called when a call to the GETPRIVMODE intrinsic
 *      fails.  Just set a flag.
 *
 **************************************/

privmode_flag = 0;
}
#endif

#ifdef APOLLO
static TEXT *scan_for_dot (
    TEXT        *string)
{
/**************************************
 *
 *      s c a n _ f o r _ d o t
 *
 **************************************
 *
 * Functional description
 *      Scan a string looking for a period, upcasing as we go.  Replace
 *      the period with a null, and return the address of the next character.
 *
 **************************************/

for (; *string && *string != '.'; string++)
    if (*string >= 'a' && *string <= 'z')
	*string -= 'a' - 'A';

if (*string)
    *string++ = 0;

return string;
}
#endif

#ifdef VMS
static int wait_test (
    SSHORT      *iosb)
{
/**************************************
 *
 *      w a i t _ t e s t
 *
 **************************************
 *
 * Functional description
 *      Return true if an I/O event is complete.
 *
 **************************************/

return *iosb;
}
#endif


#ifdef SUPERSERVER
#ifdef WIN_NT
#define SET_CONFIG_VALUE_DEFINED

static void set_config_value(
    UCHAR	key,
    ULONG	*value1_ptr,
    ULONG	*value2_ptr)
{
switch (key)
    {
    case ISCCFG_DBCACHE_KEY:
	JRD_set_cache_default(value1_ptr);
	break;
    case ISCCFG_PRIORITY_KEY:
	{
	ULONG	priority;
	if (*value1_ptr == 2)
	    priority = HIGH_PRIORITY_CLASS;
	else
	    priority = NORMAL_PRIORITY_CLASS;

	if (!SetPriorityClass(GetCurrentProcess(), priority))
	    {
	    gds__log("Unable to set process priority.");
	    }
	}
	break;
    case ISCCFG_MEMMAX_KEY:
    case ISCCFG_MEMMIN_KEY:
	if (*value1_ptr && *value2_ptr && ISC_get_ostype())
	    {
	    HANDLE hndl;

	    hndl = OpenProcess(PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
	    if (hndl)
		{
		ULONG value1 = *value1_ptr * 1024;
		ULONG value2 = *value2_ptr * 1024;

		if (!SetProcessWorkingSetSize(hndl, value1, value2))
		    {
		    switch (GetLastError())
			{
			case ERROR_ACCESS_DENIED:
			case ERROR_PRIVILEGE_NOT_HELD:
			    gds__log("Error setting process working set size, access denied.");
			    break;
			case ERROR_INVALID_PARAMETER:
		    	    gds__log("Error setting process working set size, improper range.");
			    break;
			case ERROR_NO_SYSTEM_RESOURCES:
		    	    gds__log("Error setting process working set size, insufficient memory.");
			    break;
			default:
		    	    gds__log("Error setting process working set size, unknown problem.");


			}
		    *value1_ptr = 0;
		    *value2_ptr = 0;
		    }
		CloseHandle(hndl);
		}

	    }
    }
}
#endif

#ifdef SET_CONFIG_VALUE_DEFINED
#undef SET_CONFIG_VALUE_DEFINED
#else
static void set_config_value(
    UCHAR	key,
    ULONG	*value1,
    ULONG	*value2)
{
switch (key)
    {
    case ISCCFG_DBCACHE_KEY:
	JRD_set_cache_default(value1);
	break;
    case ISCCFG_PRIORITY_KEY:
	/* 
	 * This is where you should call your OS specific function to set the 
	 * Process priority (if available). 
	*/
	break;
    case ISCCFG_MEMMAX_KEY:
    case ISCCFG_MEMMIN_KEY:
	/* 
	 * This is where you should call your OS specific function to set the 
	 * Process priority (if available). 
	*/
	break;
	}
}
#endif
#endif /* SUPERSERVER */

#ifdef UNIX
SLONG ISC_get_user_group_id (
    TEXT        *user_group_name)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r _ g r o u p _ n a n e
 *
 **************************************
 *
 * Functional description
 *
 *      Return user group id if user group name found,
 *      Otherwise return 0.
 *
 *                  ---  for UNIX platform  ---
 *
 **************************************/        

#include <grp.h>
extern	struct	group	*getgrnam();
struct	group	*user_group;
SLONG	n;

	n = 0;


	if (user_group = getgrnam (user_group_name))
		n = user_group->gr_gid ;

	return (n);
}
#else
SLONG ISC_get_user_group_id (
    TEXT        *user_group_name)
{
/**************************************
 *
 *      I S C _ g e t _ u s e r _ g r o u p _ n a n e
 *
 *                  ---  for non-UNIX platform  ---
 *
 **************************************
 *
 * Functional description
 *
 *      needed to be developed for non-unix platform
 *
 *      Currently it just returns 0.
 *
 **************************************/        

SLONG	n;

	n = 0;
	return (n);
}
#endif  /* end of ifdef UNIX */
