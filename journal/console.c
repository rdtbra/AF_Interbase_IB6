/*
 *	PROGRAM:	JRD Journalling Subsystem
 *	MODULE:		console.c
 *	DESCRIPTION:	Journal server console program
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
#include "../jrd/common.h"
#ifdef SCO_UNIX
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include "../jrd/jrd.h"
#include "../jrd/jrn.h"
#include "../journal/journal.h"
#include "../journal/conso_proto.h"
#include "../journal/gjrn_proto.h"
#include "../journal/misc_proto.h"
#include "../jrd/isc_f_proto.h"

#ifdef APOLLO
#define APOLLO_JOURNALLING
#include <apollo/base.h>
#include <apollo/mbx.h>
#include <apollo/error.h>
#endif

#ifdef UNIX
#include <errno.h>
#define UNIX_JOURNALLING
#define BSD_SOCKETS
#endif

#ifdef NETWARE_386
#include <errno.h>
#define NETWARE_JOURNALLING
#define BSD_SOCKETS
#define EINTR           0
#undef PC_PLATFORM
#endif

#ifdef OS2_ONLY
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#include <os2.h>
#endif

#ifdef UNIXWARE
#define GETWD           getcwd
#endif

#ifdef BSD_SOCKETS
#define BSD_INCLUDES
#ifndef FCNTL_INCLUDED
#include <fcntl.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#define MAXHOSTNAMELEN  64        /* should be in <sys/param.h> */
#define HANDLE          int
#endif

#ifdef WIN_NT
#include <windows.h>
#define TEXT		SCHAR
#endif

#define START_COMMAND	"status"

static void		close_connection (SLONG);
#ifdef APOLLO
static void		error (status_$t, UCHAR *);
#else
static void		error (STATUS, UCHAR *);
#endif
static void		expand_dbname (TEXT *);
#ifdef BSD_SOCKETS
static SSHORT		find_address (TEXT *, struct sockaddr_in *, SSHORT);
#endif
static ENUM jrnr_t	get_reply (SLONG);
static SLONG		open_connection (SCHAR *, SSHORT);
static void		put_command (SLONG, UCHAR *, USHORT, UCHAR *, USHORT);

extern CMDS commands [];

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH         512
#endif

int CONSOLE_start_console (
    int		argc,
    SCHAR	**argv)
{
/**************************************
 *
 *	C O N S O L E _ s t a r t _ c o n s o l e
 *
 **************************************
 *
 * Functional description
 *	Journal console function.
 *
 **************************************/
JRNH		header;
TEXT		**end, directory [MAX_PATH_LENGTH], buffer [2 * MAX_PATH_LENGTH];
SLONG		channel;
USHORT		cycle, len, length;
ENUM jrnr_t	reply;
SSHORT		single_msg = FALSE;
SCHAR		*msg, *dir;
USHORT		sw_d, sw_v, sw_i;
TEXT		mssg [128];

dir = ".";
msg = NULL;
sw_i = sw_d = sw_v = FALSE;

for (end = argv + argc, argv++; argv < end; )
    {
    if ((*argv) [0] != '-')
	argv++;
    else
	{
	MISC_down_case (*argv++, buffer);
	switch (buffer [1])
	    {
	    case 'i':
		sw_i = TRUE;
		break;

	    case 'd':
		sw_d = TRUE;
		break;

	    case 'v':
		sw_v = TRUE;
		break;

	    case 'j':
		if (!(dir = *argv++))
		    MISC_print_journal_syntax();
		break;

	    case 'c':
		break;

	    case 's':
		single_msg = TRUE;
		if (!(msg = *argv++))
		    MISC_print_journal_syntax();
		break;

	    default:
		break;
	    }
	}
    }

len = ISC_expand_filename (dir, 0, directory);
if (directory [len-1] == '/')
    len--;
#ifdef WIN_NT
else if (directory [len-1] == '\\')
    len--;
#endif
directory [len++] = '/';
directory [len] = 0;

GJRN_get_msg (CONSOLE_PROG, mssg, NULL, NULL, NULL);	/* Msg 215 Journal server console program */
printf ("%s\n", mssg);
channel = open_connection (directory, FALSE);

header.jrnh_type = JRN_CONSOLE;
header.jrnh_version = JOURNAL_VERSION;

put_command (channel, &header, sizeof (header), START_COMMAND, sizeof (START_COMMAND) - 1);
get_reply (channel);

if (single_msg == TRUE)
    {
    MISC_down_case (msg, buffer);
    length = strlen (msg);

    if (length > strlen (START_COMMAND))
	length = strlen (START_COMMAND);

    /* attaching puts out status already.  So don't do anything */

    if (strncmp (START_COMMAND, buffer, (int) length))
	{
	/* Expand any filename present in the console command */

	expand_dbname (msg);

	if (length = strlen (msg))
	    {
	    put_command (channel, &header, sizeof (header), msg, length);
	    reply = get_reply (channel);
	    }
	}
    close_connection (channel);
    return 0;
    }

for (cycle = TRUE; cycle;)
    {
    printf ("gjrn> ");
    if (sw_service_gjrn)
	putc ('\001', stdout);
    fflush (stdout);
    if (!gets (buffer))
	break;

    /* Expand any filename present in the console command */

    expand_dbname (buffer);

    if (length = strlen (buffer))
	{
	put_command (channel, &header, sizeof (header), buffer, length);
	switch (reply = get_reply (channel))
	    {
	    case jrnr_shutdown:
	    case jrnr_exit:
		cycle = FALSE;
		break;
	    }
	}
    }

close_connection (channel);

return 0;
}

int CONSOLE_test_server (
    SCHAR	*journal_dir)
{
/**************************************
 *
 *	C O N S O L E _ t e s t _ s e r v e r
 *
 **************************************
 *
 * Functional description
 *	Checks if a server is running in the
 *	specified directory.
 *
 **************************************/
SLONG	channel;
TEXT	directory [MAX_PATH_LENGTH];
USHORT	len;

len = ISC_expand_filename (journal_dir, 0, directory);
if (directory [len-1] == '/')
    len--;
#ifdef WIN_NT
else if (directory [len-1] == '\\')
    len--;
#endif
directory [len++] = '/';
directory [len] = 0;

if ((channel = open_connection (directory, TRUE)) < 0)
    return FALSE;

close_connection (channel);

return TRUE;
}

#ifdef APOLLO_JOURNALLING
static void close_connection (
    SLONG	channel)
{
/**************************************
 *
 *	c l o s e _ c o n n e c t i o n		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Journal console program.
 *
 **************************************/
status_$t	status;

mbx_$close ((int*) channel, &status);
}
#endif

#ifdef OS2_ONLY
static void close_connection (
    SLONG	channel)
{
/**************************************
 *
 *	c l o s e _ c o n n e c t i o n		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Journal console program.
 *
 **************************************/
ULONG	status;

if (status = DosClose ((HFILE) channel))
    error (status, "DosClose (pipe)");
}
#endif

#ifdef BSD_SOCKETS
static void close_connection (
    SLONG	channel)
{
/**************************************
 *
 *	c l o s e _ c o n n e c t i o n		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Journal console program.
 *
 **************************************/

if (close (channel) < 0)
    error ((STATUS)errno, "close socket");
}
#endif

#ifdef WIN_NT
static void close_connection (
    SLONG	channel)
{
/**************************************
 *
 *	c l o s e _ c o n n e c t i o n		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Journal console program.
 *
 **************************************/

if (!CloseHandle ((HANDLE) channel))
    error (GetLastError(), "CloseHandle (pipe)");
}
#endif

#ifdef APOLLO_JOURNALLING
static void error (
    status_$t	status,
    UCHAR	*string)
{
/**************************************
 *
 *	e r r o r		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT    msg [128];

GJRN_get_msg (GEN_ERR, msg, string, NULL, NULL);	/* Msg 111 Error occurred during \"%s\" */
fprintf (stderr, "%s\n", msg);
error_$print (status);
exit (FINI_ERROR);
}
#endif

#ifdef OS2_ONLY
static void error (
    STATUS	status,
    UCHAR	*string)
{
/**************************************
 *
 *	e r r o r		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT    msg [128];
SLONG	l;

GJRN_get_msg (GEN_ERR, msg, string, NULL, NULL);	/* Msg 111 Error occurred during \"%s\" */
fprintf (stderr, "%s\n", msg);

if (DosGetMessage (0, 0, msg, sizeof (msg), status, "OSO001.MSG", &l) &&
    DosGetMessage (0, 0, msg, sizeof (msg), status, "OSO001H.MSG", &l))
    {
    GJRN_get_msg (DOS_ERR2, msg, NULL, NULL, NULL);	/* Msg 173 Dos error %d */
    fprintf (stderr, msg, status);
    fprintf (stderr, "\n");
    }
else
    {
    msg [l] = 0;
    fprintf (stderr, "%s\n", msg);
    }
    
exit (FINI_ERROR);
}
#endif

#ifdef BSD_SOCKETS
static void error (
    STATUS	status,
    UCHAR	*string)
{
/**************************************
 *
 *	e r r o r		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT	msg [128];

GJRN_get_msg (GEN_ERR, msg, string, NULL, NULL);	/* Msg 111 Error occurred during \"%s\" */
fprintf (stderr, "%s\n", msg);
perror (string);
exit (FINI_ERROR);
}
#endif

#ifdef WIN_NT
static void error (
    STATUS	status,
    UCHAR	*string)
{
/**************************************
 *
 *	e r r o r		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Report an error and punt.
 *
 **************************************/
TEXT	msg [512];
SSHORT	l;

GJRN_get_msg (GEN_ERR, msg, string, NULL, NULL);	/* Msg 111 Error occurred during \"%s\" */
fprintf (stderr, "%s\n", msg);

if (!(l = FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	status,
	GetUserDefaultLangID(),
	msg,
	sizeof (msg),
	NULL)))
    {
    GJRN_get_msg (NT_ERR, msg, string, NULL, NULL);		/* Msg 211 Windows NT error %d */
    fprintf (stderr, msg, status);
    fprintf (stderr, "\n");
    }
else
    fprintf (stderr, "%s\n", msg);
    
exit (FINI_ERROR);
}
#endif

static void expand_dbname (
    TEXT	*in)
{
/**************************************
 *
 *	e x p a n d _ d b n a m e
 *
 **************************************
 *
 * Functional description
 *	Expand file name in console command.  The input string is
 *	modified.
 *
 **************************************/
CMDS    *command;
TEXT	cmd [32], operand1 [JOURNAL_PATH_LENGTH], operand2 [JOURNAL_PATH_LENGTH], *p, *q;
TEXT	exp_op [JOURNAL_PATH_LENGTH];
USHORT	n;

cmd [0] = operand1 [0] = operand2 [0] = 0;

n = sscanf (in, "%s%s%s", cmd, operand1, operand2);

for (command = commands; p = command->cmds_string; command++)
    {
    q = cmd;
    while (*q && UPPER (*q) == *p++)
	q++;
    if (!*q)
	break;
    }

switch (command->cmds_command)
    {
    case cmd_shutdown:
    case cmd_exit:
    case cmd_status:
    case cmd_help:
    case cmd_version:
	break;

    case cmd_erase:
	if (!strlen (operand1))
	    {
	    sprintf (in, "%s", "HELP");
	    return;
	    }
	break;

    case cmd_drop:
    case cmd_disable:
    case cmd_archive:
	if (!strlen (operand1))
	    {
	    sprintf (in, "%s", "HELP");
	    return;
	    }
	ISC_expand_filename (operand1, 0, exp_op);
	sprintf (in, "%s %s", cmd, exp_op);
	break;

    case cmd_reset:
    case cmd_set:
	if ((!strlen (operand1)) || (!strlen (operand2)))
	    {
	    sprintf (in, "%s", "HELP");
	    return;
	    }
	ISC_expand_filename (operand2, 0, exp_op);
	sprintf (in, "%s %s %s", cmd, operand1, exp_op);
	break;

    default:
	sprintf (in, "%s", "HELP");
	return;
    }
}

#ifdef BSD_SOCKETS
static SSHORT find_address (
    TEXT		*filename,
    struct sockaddr_in	*address,
    SSHORT		test_only)
{
/**************************************
 *
 *	f i n d _ a d d r e s s		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Get the socket address established by the server by
 *	looking in the designated files.
 *
 **************************************/
FILE	*file;
SLONG	addr, family, port, version;
                                     
if (!(file = fopen (filename, "r")))
    {
    if (test_only)
	return (-1);

    error (errno, "journal socket file open");
    }

if (fscanf (file, "%ld %ld %ld %ld", &version, &addr, &family, &port) != 4)
    {
    if (test_only)
	return (-1);

    error (0, "journal socket file format");
    }
fclose (file);

if (version != JOURNAL_VERSION)
    {
    if (test_only)
	return (-1);

    error (0, " address version");
    }

address->sin_addr.s_addr = addr;
address->sin_family = family;
address->sin_port = port;

return TRUE;
}
#endif

#ifdef APOLLO_JOURNALLING
static ENUM jrnr_t get_reply (
    SLONG	channel)
{
/**************************************
 *
 *	g e t _ r e p l y		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
UCHAR		*p, buffer [MAX_PATH_LENGTH];
SLONG		l, length;
JRNR		*reply;
status_$t	status;

p = buffer + sizeof (JRNR);
l = sizeof (buffer);

for (;;)
    {
    reply = (JRNR*) buffer;
    mbx_$get_rec ((int*) channel, reply, l, &reply, &length, &status);
    if (status.all)
	error (status, "mbx_$get_rec");
    if (reply->jrnr_response != jrnr_msg)
	return reply->jrnr_response;
    p [reply->jrnr_page] = 0;
    printf ("%s\n", p);
    }
}
#endif

#ifdef OS2_ONLY
static ENUM jrnr_t get_reply (
    SLONG	channel)
{
/**************************************
 *
 *	g e t _ r e p l y		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
UCHAR	*p, buffer [MAX_PATH_LENGTH];
ULONG	length, len;
JRNR	*reply;
ULONG	status;

p = buffer + sizeof (JRNR);

for (;;)
    {
    reply = (JRNR*) buffer;

    /* first read enough to find out how much to read */

    length = sizeof (reply->jrnr_header);

    if ((status = DosRead ((HFILE) channel, buffer, length, &len)) ||
	len != length)
    	error (status, "DosRead (pipe)");

    /* read the rest of the response */

    length = reply->jrnr_header.jrnh_length - length;

    if ((status = DosRead ((HFILE) channel, &buffer [len], length, &len)) ||
	len != length)
    	error (status, "DosRead (pipe)");

    if (reply->jrnr_response != jrnr_msg)
	return reply->jrnr_response;

    p [reply->jrnr_page] = 0;
    printf ("%s\n", p);
    }
}
#endif

#ifdef BSD_SOCKETS
static ENUM jrnr_t get_reply (
    SLONG	channel)
{
/**************************************
 *
 *	g e t _ r e p l y		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
UCHAR		*p, buffer [MAX_PATH_LENGTH];
ULONG		len, length;
JRNR		*reply;
int		status;

p = buffer + sizeof (JRNR);

for (;;)
    {
    reply = (JRNR*) buffer;

    /* first read enough to find out how much to read */

    length = sizeof (reply->jrnr_header);

    if ((len = read (channel, buffer, length)) != length)
    	error (errno, "socket read");

    /* read the rest of the response */

    length = reply->jrnr_header.jrnh_length - length;

    if ((len = read (channel, &buffer[len], length)) != length)
    	error (errno, "socket read");

    if (reply->jrnr_response != jrnr_msg)
	return reply->jrnr_response;

    p [reply->jrnr_page] = 0;
    printf ("%s\n", p);
    }
}
#endif

#ifdef WIN_NT
static ENUM jrnr_t get_reply (
    SLONG	channel)
{
/**************************************
 *
 *	g e t _ r e p l y		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
UCHAR	*p, buffer [MAX_PATH_LENGTH];
ULONG	length, len;
JRNR	*reply;

p = buffer + sizeof (JRNR);

for (;;)
    {
    reply = (JRNR*) buffer;

    /* first read enough to find out how much to read */

    length = sizeof (reply->jrnr_header);

    if (!ReadFile ((HANDLE) channel, buffer, length, &len, NULL) ||
	len != length)
    	error (GetLastError(), "ReadFile (pipe)");

    /* read the rest of the response */

    length = reply->jrnr_header.jrnh_length - length;

    if (!ReadFile ((HANDLE) channel, &buffer [len], length, &len, NULL) ||
	len != length)
    	error (GetLastError(), "ReadFile (pipe)");

    if (reply->jrnr_response != jrnr_msg)
	return reply->jrnr_response;

    p [reply->jrnr_page] = 0;
    printf ("%s\n", p);
    }
}
#endif

#ifdef APOLLO_JOURNALLING
static SLONG open_connection (
    SCHAR	*filename,
    SSHORT	test_only)
{
/**************************************
 *
 *	o p e n _ c o n n e c t i o n		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Open connection to console program.
 *	If test_only, we are just testing to
 *	see if journal server is running.
 *
 **************************************/
TEXT		name [1024], msg [128];
SLONG		channel;
SSHORT		l;
status_$t	status;

strcpy (name, filename);
strcat (name, CONSOLE_FILE);
l = strlen (name);
mbx_$open (name, l, NULL, (SLONG) 0, &channel, &status);

if (status.all)
    {
    if (test_only)
	return (-1);

    GJRN_get_msg (MAILBOX_ERR, msg, name, NULL, NULL);	/* Msg 217 Can't open mailbox %s */
    printf ("%s\n", msg);
    error (status, "mbx_$open");
    }

return channel;
}
#endif

#ifdef OS2_ONLY
static SLONG open_connection (
    SCHAR	*filename,
    SSHORT	test_only)
{
/**************************************
 *
 *	o p e n _ c o n n e c t i o n		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Open connection to console program.
 *	If test_only, we are just testing to
 *	see if journal server is running.
 *
 **************************************/
TEXT	name [MAX_PATH_LENGTH];
ULONG	state, status;
HFILE	channel;

/* For now, ignore the input file path */

strcpy (name, "\\PIPE\\");
strcat (name, CONSOLE_FILE);

if (status = DosOpen (
	name, 
	&channel, 
	&state, 
	0, 		/* Initial allocation (not used) */
	FILE_NORMAL,	/* Pipe attribute (not used) */
	OPEN_ACTION_OPEN_IF_EXISTS, /* Open the file if it exists */
	0x00004012,	/* open mode: r/w, nosharing */
	0L))
    {
    if (test_only)
	return (-1);

    error (status, "DosOpen");
    }
	
return channel;
}
#endif

#ifdef BSD_SOCKETS
static SLONG open_connection (
    SCHAR	*filename,
    SSHORT	test_only)
{
/**************************************
 *
 *	o p e n _ c o n n e c t i o n		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Open connection to console program.
 *	If test_only, we are just testing to
 *	see if journal server is running.
 *
 **************************************/
struct sockaddr_in	address;
HANDLE	channel;
TEXT	name [MAX_PATH_LENGTH];

strcpy (name, filename);
strcat (name, CONSOLE_ADDR);

/* Allocate a port block and initialize a socket for communications */

if ((channel = (SLONG) socket (AF_INET, SOCK_STREAM, 0)) < 0)
    error (errno, "socket");
           
memset ((SCHAR*) &address, 0, sizeof (address));

/* Will return -1 only if test_only is set - otherwise fatal error */

if (find_address (name, &address, test_only) < 0)
    return -1;

if (connect (channel, &address, sizeof (address)) < 0)
    {
    if (test_only)
	return (-1);

    error (errno, "connect console");
    }
     
return channel;
}
#endif

#ifdef WIN_NT
static SLONG open_connection (
    SCHAR	*filename,
    SSHORT	test_only)
{
/**************************************
 *
 *	o p e n _ c o n n e c t i o n		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Open connection to console program.
 *	If test_only, we are just testing to
 *	see if journal server is running.
 *
 **************************************/
HANDLE	channel;
TEXT	name [MAX_PATH_LENGTH], *p, *q, c;

p = name;
strcpy (p, "\\\\.\\pipe");
p += strlen (p);

if (*filename != '/' && *filename != '\\')
    *p++ = '\\';

while (*filename)
    *p++ = ((c = *filename++) == ':') ? '_' : ((c == '/') ? '\\' : c);
    
strcpy (p, CONSOLE_FILE);

while (TRUE)
    {
    channel = CreateFile (name,
	GENERIC_READ | GENERIC_WRITE,
	0,
	NULL,
	OPEN_EXISTING,
	0,
	NULL);
    if (channel != INVALID_HANDLE_VALUE)
	break;

    if (GetLastError() != ERROR_PIPE_BUSY)
	{
	if (test_only)
	    return (-1);

	error (GetLastError(), "CreateFile (pipe)");
	}

    WaitNamedPipe (name, 2000L);
    }
     
return channel;
}
#endif

#ifdef APOLLO_JOURNALLING
static void put_command (
    SLONG	channel,
    UCHAR	*header,
    USHORT	header_length,
    UCHAR	*data,
    USHORT	data_length)
{
/**************************************
 *
 *	p u t _ c o m m a n d		( A P O L L O )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
TEXT		*p, buffer [MAX_PATH_LENGTH];
SLONG		l;
status_$t	status;

p = buffer;
l = header_length + data_length;

if (header_length)
    do *p++ = *header++; while (--header_length);

if (data_length)
    do *p++ = *data++; while (--data_length);

p = buffer;
mbx_$put_rec ((int*) channel, p, l, &status);

if (status.all)
    error (status, "mbx_$put_rec");
}
#endif

#ifdef OS2_ONLY
static void put_command (
    SLONG	channel,
    UCHAR	*header,
    USHORT	header_length,
    UCHAR	*data,
    USHORT	data_length)
{
/**************************************
 *
 *	p u t _ c o m m a n d		( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
TEXT	*p, buffer [MAX_PATH_LENGTH];
ULONG	l, len;
ULONG	status;
JRNH	*header_ptr;

p = buffer;
l = header_length + data_length;

header_ptr = (JRNH*) header;
header_ptr->jrnh_length = l;

if (header_length)
    do *p++ = *header++; while (--header_length);

if (data_length)
    do *p++ = *data++; while (--data_length);

if (status = DosWrite ((HFILE) channel, buffer, l, &len))
    error (status, "DosWrite (pipe)");
}
#endif

#ifdef BSD_SOCKETS
static void put_command (
    SLONG	channel,
    UCHAR	*header,
    USHORT	header_length,
    UCHAR	*data,
    USHORT	data_length)
{
/**************************************
 *
 *	p u t _ c o m m a n d		( U N I X )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
TEXT	*p, buffer [MAX_PATH_LENGTH];
USHORT	l;
JRNH	*header_ptr;

p = buffer;
l = header_length + data_length;
                 
header_ptr = (JRNH*) header;
header_ptr->jrnh_length = l;

if (header_length)
    do *p++ = *header++; while (--header_length);

if (data_length)
    do *p++ = *data++; while (--data_length);

if (write (channel, buffer, l) < 0)
    error (errno, "write socket");
}
#endif

#ifdef WIN_NT
static void put_command (
    SLONG	channel,
    UCHAR	*header,
    USHORT	header_length,
    UCHAR	*data,
    USHORT	data_length)
{
/**************************************
 *
 *	p u t _ c o m m a n d		( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Format and send off a command.
 *
 **************************************/
TEXT	*p, buffer [MAX_PATH_LENGTH];
ULONG	l, len;
JRNH	*header_ptr;

p = buffer;
l = header_length + data_length;
                 
header_ptr = (JRNH*) header;
header_ptr->jrnh_length = l;

if (header_length)
    do *p++ = *header++; while (--header_length);

if (data_length)
    do *p++ = *data++; while (--data_length);

if (!WriteFile ((HANDLE) channel, buffer, l, &len, NULL) ||
    l != len)
    error (GetLastError(), "WriteFile (pipe)");
}
#endif
