/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		log.c
 *	DESCRIPTION:	Log all OSRI calls
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
#include "../jrd/common.h"
#include <stdarg.h>
#ifdef WIN_NT
#include <io.h>
#endif
#include "../jrd/jrd.h"
#include "../jrd/log.h"
#include "../jrd/ods.h"
#include "../jrd/all_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/log_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/thd_proto.h"


#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM

#include "../jrd/arg.h"

/*
Documentation on the REPLAY_OSRI_API_CALLS_SUBSYSTEM.

This is currently removed as
    a) It is currently not used for any customers
    b) it doesn't work in 4.0
    c) Has a slight performance impact.
    d) It needs work to cover the full range of 4.0 API calls


This is a valuable function to re-activate in the product, but 
until it is I've #ifdef'ed it out under REPLAY_OSRI_API_CALLS_SUBSYSTEM.
I've also embedded the notes on how it works into log.c so they
can serve as a reference to a future completion of implementation.

Not reviewed, removal & documentation only

1996-Nov-07 David Schnepper 

================= dialogue history ================

From ccaro@wpo.borland.com Thu Nov  7 10:23 PST 1996

I think we should keep REPLAY in the product..

I think it will be an  invaluable tool for debugging complex
SuperServer interactions. While the CLASSIC architecture is well-tested,
multi-user interactions will only become more frenetic as the multi-user
server evolves to higher levels of SMP support.

------------  and ----------

From bkarwin@interbase.borland.com Thu Nov  7 13:19 PST 1996

I do not know of any customers sites who use REPLAY.  It is likely that
only Motorola knows of its existance.

This is the first I've heard of this feature, at least that I recall.
I believe the rest of support also did not know of this feature.  So it
may be safe to say that none of us use it.  :-)

It sounds like a good feature.  In fact, a customer asked recently if
there was a server-side feature akin to Delphi's "SQL Monitor" (which
logs all BDE and InterBase API calls), and we had to say no.  Even if
we had known about the REPLY feature, I think we would have said no,
since it is uncertified.

I think we should keep it in the product, and even do some development
on it, to eventually make it a supported feature that can be used
for diagnostics.

-Bill Karwin

-----------------------


   Documentation on the REPLAY_OSRI_API_CALLS_SUBSYSTEM.

-----------

From deej@borland.com Thu Nov  7 09:48 PST 1996

I'll confess to having written the REPLAY feature. 
I used to say that I wrote it to avoid going to Phoenix 
in August, when Motorola was having a problem that we 
could only reproduce by running their application.  
For whatever reason it was impossible to get the 
application inhouse. 

But I really wrote it as a general purpose diagnostic tool 
that would allow us to recreate customer problems without 
sending an engineer to a customer site, and without the 
necessity of figuring out a complex application. 

Here's the way it works: the customer makes a copy of their 
database to tape.  They start REPLAY logging, then run their 
application per usual.  When the error occurs, they stop 
logging.  They send the original database and the REPLAY log 
to us, and we re-run the log inhouse.  It's like a tape recorder 
which records all the API calls their application makes. 

The feature worked beautifully in the Motorola case, and I 
was able to reproduce two weeks' worth of the customer's 
operations in an hour, enabling me to fix the bug within a 
day when it might have taken several weeks at the customer 
site. 

Unfortunately, new API calls or parameters have been added 
and the REPLAY feature has not been updated.  At some point during 
the 4.0 cycle I updated REPLAY to take care of the new calls 
added to that point.   But REPLAY has never been maintained 
as a full-blown feature, meaning it was only tested on one platform, 
and certainly never had full testing to certify that it works 
for all API calls. 

I think it is best if we leave this as a latent feature, which 
can be dusted off if really needed in a future customer situation. 
At that time we can generate a t-kit with the feature turned on, 
so there is no need to maintain it as a fully supported feature. 
To be sure, InterBase is a much more reliable database than it 
was in 1990 and these types of situations occur less often. 
And the cost of fully productizing this particular feature is 
frightening. 

At 04:58 PM 11/6/96 -0800, David Schnepper wrote:
>
>Does anyone still use the REPLAY facility?
>
>For those that don't know, InterBase had the ability to record all API calls
>in a log file, with the idea being to make an audit trail for debugging
>problems found in the field.
>
>There also was (apparently) the ability to replay the log to see if the
>trouble still reproduced.
>
>The reason I ask is I have found that REPLAY for Blobs (at least) doesn't
>function in 4.O - there is a private API call gds__map_blobs() which was
>used for the replay function that calls into the engine in an unsafe way.
>
>I would like to turn REPLAY off, by turning off the #ifdef in common.h - but
>would like to check on whether anyone in-house is using it - or if
>it is likely that any knowledgable customer is using it.
>
>Aside: we'll get a slight performance gain on each API call by turning
>it off as well.
>
>Dave
>

*/

static void	error (TEXT *);
static void	log_char (SCHAR);
static void	log_flush (void);
static void	log_long (SLONG);
static void	log_pointer (SCHAR *);
static void	log_short (SSHORT);
static void	log_string (SSHORT, SCHAR *);
static void	log_teb (SSHORT, TEB *);
static void	open_log (TEXT *, SSHORT, SCHAR *);
         
#define PUT(c)	log_char(c)

void LOG_call (
    enum log_t	call_type,
    ...)
{
/**************************************
 *
 *	L O G _ c a l l
 *
 **************************************
 *
 * Functional description
 *	Log an OSRI call for later replay.  This is intended
 *	for use as a debugging utility to ease fixing 
 *	bugs in a production environment.
 *
 **************************************/
DBB	dbb;
SCHAR	*arg_type;
SSHORT	number;
SLONG	long_number;
SCHAR	*pointer;
SLONG	*long_pointer;
va_list ptr;  

dbb = GET_DBB;

if (!dbb || !dbb->dbb_log || !dbb->dbb_log->log_file)
    return;

PUT ((SCHAR) call_type);     

VA_START (ptr, call_type);

/* using the argument types described in the table, 
   pop the arguments off the call stack and put
   them into the log file */

for (arg_type = arg_types1 [(int) call_type]; *arg_type; arg_type++)
    switch (*arg_type)
        {
        case 'o':
        case 's':
	    number = va_arg (ptr, int);
	    log_short (number);
	    break;

        case 'l':
	    long_number = va_arg (ptr, SLONG);
	    log_long (long_number);
	    break;

	case 'p':
	    pointer = va_arg (ptr, SCHAR*);
	    log_pointer (pointer);
	    break;

	case 'r': 
	    pointer = va_arg (ptr, SCHAR*);
	    break;
	    
	case 'b':
	    pointer = va_arg (ptr, SCHAR*);
	    log_string (number, pointer);
	    break;
                  
	case 'f':
	    number = 0;
	case 'n':
	    pointer = va_arg (ptr, SCHAR*);
	    if (!number)
		number = strlen (pointer) + 1;
	    log_string (number, pointer);
	    break;
                  
	case 't':
	    pointer = va_arg (ptr, SCHAR*);
  	    log_teb (number, pointer);
	    break;

	case 'd':
	    long_pointer = va_arg (ptr, SLONG*);
  	    log_long (long_pointer[0]);
  	    log_long (long_pointer[1]);
	    break;

	default:
	    error ("argument type not known"); 
        }

va_end (ptr);

log_flush ();
}

void LOG_disable (void)
{
/**************************************
 *
 *	L O G _ d i s a b l e
 *
 **************************************
 *
 * Functional description
 *	Disable logging.
 *
 **************************************/
DBB	dbb;

dbb = GET_DBB;

/* Get header page and look for logging entry */

PAG_delete_clump_entry (HEADER_PAGE, HDR_log_name);

/* Next, if logging was really enabled, close the file */

if (dbb->dbb_log && dbb->dbb_log->log_file)
    LOG_fini();
}     

void LOG_enable (
    TEXT	*log_file_name,
    USHORT	l_length)
{
/**************************************
 *
 *	L O G _ e n a b l e
 *
 **************************************
 *
 * Functional description
 *	Enable replay logging for the database.
 *
 **************************************/
DBB	dbb;

dbb = GET_DBB;

/* if we are already enabled for another file, get rid of it */               

if (dbb->dbb_log)
    LOG_disable();

/* Get database header page and add the log name */

PAG_add_clump (HEADER_PAGE, HDR_log_name, l_length, log_file_name, CLUMP_ADD, 0);

/* open the log file for write to make sure that this 
   is the first entry in the file, and write the version number */

open_log (log_file_name, l_length, MODE_WRITE);                          
log_short (LOG_VERSION3);
LOG_fini();

/* now reopen for append to be a good citizen */

open_log (log_file_name, l_length, MODE_APPEND);                          
}

void LOG_fini (void)
{
/**************************************
 *
 *	L O G _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Close down the log file for replay logging.
 *
 **************************************/    
DBB	dbb;
LOG	log;

dbb = GET_DBB;

if (dbb && (log = dbb->dbb_log))
    {
    if (log->log_file)
        {
        log_flush();
        ib_fclose (log->log_file);
        }
    ALL_release (log->log_string);
    ALL_release (log);
    dbb->dbb_log = NULL;
    }
}

void LOG_init (
    TEXT	*name,
    USHORT	length)
{
/**************************************
 *
 *	L O G _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize the REPLAY logging system.
 *	Check the database to see if logging 
 *	is enabled.  If so, open the file to
 *	append to.
 *
 **************************************/  
USHORT	log_length;
TEXT	file_name [256];

/* Get header page and look for logging entry */

if (!PAG_get_clump (HEADER_PAGE, HDR_log_name, &log_length, file_name))
    return;
file_name [log_length] = 0;

open_log (file_name, log_length, MODE_APPEND);
}

static void error (
    TEXT	*error_string)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	Report a failure of the logging system,
 *	both to the terminal and to the log.
 *
 **************************************/  
DBB	dbb;
SSHORT	length;

dbb = GET_DBB;

ib_printf ("ERROR in logging system: %s\n", error_string);

if (dbb->dbb_log && dbb->dbb_log->log_file)
    {
    log_short (log_error);
    length = strlen (error_string);
    log_short (length);
    }
}

static void log_char (
    SCHAR	c)
{
/**************************************
 *
 *	l o g _ c h a r
 *
 **************************************
 *
 * Functional description
 *	Log a character to the fill buffer 
 *	for later flushing to the log.
 *
 **************************************/
DBB	dbb;
LOG	log;

dbb = GET_DBB;

log = dbb->dbb_log;            
*log->log_ptr++ = c;

/* this log flush could be done in the middle of an OSRI
   call and hence could be non multiuser compatible -- it
   is important that the log buffer be large enough that
   this doesn't happen very often */

if ((log->log_ptr - log->log_buffer) >= (LOG_BUFFER_LENGTH - 1))
    log_flush();

}

static void log_flush (void)
{
/**************************************
 *
 *	l o g _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush the contents of the log buffer to
 *	the file and do a file flush while we're
 *	at it.
 *
 **************************************/
DBB	dbb;
LOG	log;
UCHAR	*buffer;

dbb = GET_DBB;

log = dbb->dbb_log;
if (!(log->log_ptr - log->log_buffer))
    return;            

#ifdef mpexl
ISC_file_lock (_mpe_fileno (ib_fileno ((IB_FILE*) log->log_file)));
#endif          

buffer = log->log_buffer;
ib_fwrite (buffer, sizeof (*buffer), log->log_ptr - log->log_buffer, log->log_file);
log->log_ptr = log->log_buffer;

#ifdef mpexl
ISC_file_unlock (_mpe_fileno (ib_fileno ((IB_FILE*) log->log_file)));
#endif
                              
ib_fflush (log->log_file);
}

static void log_long (
    SLONG	number)
{
/**************************************
 *
 *	l o g _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Log a longword.
 *
 **************************************/
SLONG	vax_number;
USHORT	i;
SCHAR	*p;

vax_number = gds__vax_integer (&number, sizeof (number));
p = (SCHAR*) &vax_number;
for (i = 0; i < sizeof (number); i++)
    PUT (*p++);
}

static void log_pointer (
    SCHAR	*pointer)
{
/**************************************
 *
 *	l o g _ p o i n t e r
 *
 **************************************
 *
 * Functional description
 *	Log a pointer.
 *
 **************************************/
USHORT	i;
SCHAR	*p;

p = (SCHAR*) &pointer;
for (i = 0; i < sizeof (pointer); i++)
    PUT (*p++);
}

static void log_short (
    SSHORT	number)
{
/**************************************
 *
 *	l o g _ s h o r t
 *
 **************************************
 *
 * Functional description
 *	Log a shortword.
 *
 **************************************/
USHORT	vax_number;
USHORT	i;
SCHAR	*p;

vax_number = (USHORT) gds__vax_integer (&number, sizeof (number));
p = (SCHAR*) &vax_number;
for (i = 0; i < sizeof (number); i++)
    PUT (*p++);
}

static void log_string (
    SSHORT	buffer_size,
    SCHAR	*buffer)
{
/**************************************
 *
 *	l o g _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Log a buffer of unknown content.
 *
 **************************************/

while (buffer_size--)
    PUT (*buffer++);
}

static void log_teb (
    SSHORT	count,
    TEB		*vector)
{
/**************************************
 *
 *	l o g _ t e b
 *
 **************************************
 *
 * Functional description
 *	Log a transaction element block.
 *
 **************************************/
TEB	*end;

for (end = vector + count; vector < end; vector++)
    {
    log_pointer (*vector->teb_database);
    log_long ((SLONG) vector->teb_tpb_length);
    log_string (vector->teb_tpb_length, vector->teb_tpb);
    }
}

static void open_log (
    TEXT	*file_name,
    SSHORT	file_length,
    SCHAR	*mode)
{
/**************************************
 *
 *	o p e n _ l o g
 *
 **************************************
 *
 * Functional description
 *	Open the log file.
 *
 **************************************/  
DBB	dbb;
LOG	log;
#ifndef STACK_REDUCTION
SCHAR	*log_name, buffer [256];
#else
SCHAR	*log_name, *buffer;
#endif /* !STACK_REDUCTION */
void	*log_file;
int	mask;

#ifdef STACK_REDUCTION
buffer = (SCHAR *)gds__alloc ((SLONG)BUFFER_MEDIUM);
if(!buffer) /* NOMEM: */
  {
  error ("can't open log file (out of memory)");
  return;
  }
#endif /* STACK_REDUCTION */

dbb = GET_DBB;

if (dbb->dbb_log)
    LOG_fini();

if (file_length)
    log_name = file_name;
else
    gds__prefix (buffer, LOG_FILE_NAME);
    log_name = buffer;

#ifndef mpexl
mask = umask (0111);
#endif
log_file = ib_fopen (log_name, mode);
#ifndef mpexl
umask (mask);
#endif

if (!log_file)
    error ("can't open log file");
else
    {
    dbb->dbb_log = log = (LOG) ALLOCP (type_log);
    log->log_file = log_file;              
    log->log_string = (STR) ALLOCPV (type_str, LOG_BUFFER_LENGTH);
    log->log_ptr = log->log_buffer = log->log_string->str_data;
    }
#ifdef STACK_REDUCTION
if (buffer)
    gds__free ((SLONG *)buffer);
#endif /* STACK_REDUCTION */
}
#endif /* REPLAY_OSRI_API_CALLS_SUBSYSTEM */
