/*
 *      PROGRAM:        InterBase Utility programs
 *      MODULE:         util.c
 *      DESCRIPTION:    Utility routines for ibguard & ibserver
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>       /* for open() and fcntl() */
#include <errno.h>

#ifndef NO_FLOCK
#include <sys/file.h>    /* for flock() */
#endif

#include <unistd.h>      /* for fork() */

#ifdef SOLARIS
#include <wait.h>        /* for waitpid() */
#else
#include <sys/wait.h>    /* for waitpid() */
#endif


#include "../jrd/common.h"
#include "../jrd/gdsassert.h"
#include "../utilities/util_proto.h"
#include "../jrd/gds_proto.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif


pid_t UTIL_start_process (
    char        *process,
    char        **argv)
{
/**************************************
 *
 *      U T I L _ s t a r t _ p r o c e s s
 *
 **************************************
 *
 * Functional description
 *      
 *     This function is used to create the specified process, 
 *
 * Returns Codes:
 *	-1		Process spawn failed.
 *	pid		Successful creation. PID is returned. 
 *      
 *     Note: Make sure that the argument list ends with a null  
 *     and the first argument is large enough to hold the complete 
 *     expanded process name. (MAXPATHLEN recommended)
 *
 **************************************/
TEXT string [MAXPATHLEN];
pid_t pid;

assert (process != NULL);
assert (argv != NULL);

/* prepend InterBase home directory to the program name */
gds__prefix (string, process);

/* add place in argv for visibility to "ps" */
strcpy (argv[0], string);

if (!(pid = vfork()))
    {
    execv (string, argv);
    _exit (FINI_ERROR);
    }

return (pid);
}

int UTIL_wait_for_child (
    pid_t	child_pid)
{
/**************************************
 *
 *      U T I L _ w a i t _ f o r _ c h i l d
 *
 **************************************
 *
 * Functional description
 *      
 *     This function is used to wait for the child specified by child_pid
 *
 * Return code:
 *	0	Normal exit
 *	-1	Abnormal exit - unknown reason.
 *	Other	Abnormal exit - process error code returned.
 *
 **************************************/
int child_exit_status;

assert (child_pid != 0);

/* wait for the child process with child_pid to exit */

while (waitpid (child_pid, &child_exit_status, 0) == -1) 
    if (SYSCALL_INTERRUPTED (errno))
	continue;
    else
	return (errno);

/* Check for very specific conditions before we assume the child
   did a normal exit. */

if (WIFEXITED (child_exit_status) && 
    (WEXITSTATUS (child_exit_status) != 0))
    return (WEXITSTATUS (child_exit_status));

if (WCOREDUMP (child_exit_status) ||
    WIFSIGNALED (child_exit_status) ||
    !WIFEXITED (child_exit_status))
    return (-1);

return (0);
}

int UTIL_ex_lock (
    TEXT	*file)
{
/**************************************
 *
 *      U T I L _ e x _ l o c k              
 *
 **************************************
 *
 * Functional description
 *  
 *     This function is used to exclusively lock a file.
 *
 * Return Codes:
 *	-1		Failure to open file.
 *	-2		Failure to obtain exclusive lock on file.
 *	otherwise	Success - file descriptor is returned.
 *
 **************************************/

TEXT expanded_filename [MAXPATHLEN], tmp [MAXPATHLEN], hostname[64];
int fd_file; /* file fd for the opened and locked file */

#ifdef NO_FLOCK
struct flock    lock;
#endif

/* get the file name and prepend the complete path etc */
gds__prefix_lock (tmp, file);
sprintf (expanded_filename, tmp, ISC_get_host (hostname, sizeof (hostname)));

if ((fd_file = open (expanded_filename, O_RDWR | O_CREAT, 0666)) == -1)
    {
    ib_fprintf (ib_stderr, "Could not open %s for write\n", expanded_filename);
    return (-1);
    }

assert (fd_file != -2);

#ifdef NO_FLOCK
/* get an exclusive lock on the GUARD file without blocking on the call */
lock.l_type = F_WRLCK;
lock.l_whence = 0;
lock.l_start = 0;
lock.l_len = 0;
if (fcntl (fd_file, F_SETLK, &lock) == -1)
#else
if (flock (fd_file, LOCK_EX))
#endif
    {
    close (fd_file);
    return (-2);
    }
return (fd_file);
}

void UTIL_ex_unlock (
    int         fd_file)
{
/**************************************
 *
 *      U T I L _ e x _ l o c k              
 *
 **************************************
 *
 * Functional description
 *  
 *     This function is used to unlock the exclusive file.
 *
 **************************************/


#ifdef NO_FLOCK

struct flock    lock;

/* get an exclusive lock on the GUARD file with a block */
lock.l_type = F_UNLCK;
lock.l_whence = 0;
lock.l_start = 0;
lock.l_len = 0;
fcntl (fd_file, F_SETLKW, &lock);
#else
flock (fd_file, LOCK_UN);
#endif
close (fd_file);
}
