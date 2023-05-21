/*
 *	PROGRAM:	JRD Remote Interface
 *	MODULE:		remote.c
 *	DESCRIPTION:	Common routines for remote interface/server
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

#include <string.h>
#include "../jrd/gds.h"
#include "../remote/remote.h"
#include "../jrd/file_params.h"
#include "../jrd/gdsassert.h"
#include "../jrd/isc.h"
#include "../remote/allr_proto.h"
#include "../remote/proto_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/gds_proto.h"

#define DUMMY_INTERVAL		60	/* seconds */
#define CONNECTION_TIMEOUT	180	/* seconds */
#define ATTACH_FAILURE_SPACE	2048	/* bytes */

static TEXT	*attach_failures = NULL, *attach_failures_ptr;

static SLONG  conn_timeout   = CONNECTION_TIMEOUT;
static SLONG  dummy_interval = DUMMY_INTERVAL;

#define CONNECTION_TIMEOUT_IDX    0
#define DUMMY_PACKET_INTRVL_IDX   1

static struct ipccfg   INET_cfgtbl [] = {
   ISCCFG_CONN_TIMEOUT, 0, &conn_timeout,   0, 0,
   ISCCFG_DUMMY_INTRVL, 0, &dummy_interval, 0, 0,
   NULL,                0, NULL,            0, 0
};

static void	cleanup_memory (void *);
static SLONG    get_parameter (UCHAR **);

#ifdef SHLIB_DEFS
#define xdrmem_create	(*_libgds_xdrmem_create)
#endif

extern void		xdrmem_create();

void DLL_EXPORT REMOTE_cleanup_transaction (
    RTR		transaction)
{
/**************************************
 *
 *	R E M O T E _ c l e a n u p _ t r a n s a c t i o n
 *
 **************************************
 *
 * Functional description
 *	A transaction is being committed or rolled back.  
 *	Purge any active messages in case the user calls
 *	receive while we still have something cached.
 *
 **************************************/
RRQ	request, level;
RSR	statement;

for (request = transaction->rtr_rdb->rdb_requests; request; request = request->rrq_next)
    {
    if (request->rrq_rtr == transaction)
	{
	REMOTE_reset_request (request, NULL_PTR);
	request->rrq_rtr = NULL;
	}
    for (level = request->rrq_levels; level; level = level->rrq_next)
	if (level->rrq_rtr == transaction)
	    {
	    REMOTE_reset_request (level, NULL_PTR);
	    level->rrq_rtr = NULL;
	    }
    }

for (statement = transaction->rtr_rdb->rdb_sql_requests; statement; statement = statement->rsr_next)
    if (statement->rsr_rtr == transaction)
	{
	REMOTE_reset_statement(statement);
	statement->rsr_flags &= ~RSR_fetched;
	statement->rsr_rtr = NULL;
	}
}

ULONG REMOTE_compute_batch_size (
    PORT        port,
    USHORT	buffer_used,
    P_OP	op_code,
    FMT		format)
{
/**************************************
 *
 *	R E M O T E _ c o m p u t e _ b a t c h _ s i z e
 *
 **************************************
 *
 * Functional description
 *
 * When batches of records are returned, they are returned as
 *    follows:
 *     <op_fetch_response> <data_record 1>
 *     <op_fetch_response> <data_record 2>
 * 	...
 *     <op_fetch_response> <data_record n-1>
 *     <op_fetch_response> <data_record n>
 * 
 * end-of-batch is indicated by setting p_sqldata_messages to
 * 0 in the op_fetch_response.  End of cursor is indicated
 * by setting p_sqldata_status to a non-zero value.  Note
 * that a fetch CAN be attempted after end of cursor, this
 * is sent to the server for the server to return the appropriate
 * error code. 
 * 
 * Each data block has one overhead packet
 * to indicate the data is present.
 * 
 * (See also op_send in receive_msg() - which is a kissing cousin
 *  to this routine)
 * 
 * Here we make a guess for the optimal number of records to
 * send in each batch.  This is important as we wait for the
 * whole batch to be received before we return the first item
 * to the client program.  How many are cached on the client also
 * impacts client-side memory utilization.
 * 
 * We optimize the number by how many can fit into a packet.
 * The client calculates this number (n from the list above)
 * and sends it to the server.
 * 
 * I asked why it is that the client doesn't just ask for a packet 
 * full of records and let the server return however many fits in 
 * a packet.  According to Sudesh, this is because of a bug in 
 * Superserver which showed up in the WIN_NT 4.2.x kits.  So I 
 * imagine once we up the protocol so that we can be sure we're not 
 * talking to a 4.2 kit, then we can make this optimization. 
 *           - Deej 2/28/97
 * 
 * Note: A future optimization can look at setting the packet 
 * size to optimize the transfer.
 *
 * Note: This calculation must use worst-case to determine the
 * packing.  Should the data record have VARCHAR data, it is
 * often possible to fit more than the packing specification
 * into each packet.  This is also a candidate for future 
 * optimization.
 * 
 * The data size is either the XDR data representation, or the
 * actual message size (rounded up) if this is a symmetric
 * architecture connection. 
 * 
 **************************************/

#define	MAX_PACKETS_PER_BATCH	 4 /* packets    - picked by SWAG */
#define MIN_PACKETS_PER_BATCH	 2 /* packets    - picked by SWAG */
#define DESIRED_ROWS_PER_BATCH	20 /* data rows  - picked by SWAG */
#define MIN_ROWS_PER_BATCH	10 /* data rows  - picked by SWAG */

USHORT	op_overhead = xdr_protocol_overhead (op_code);
USHORT	num_packets;
ULONG	row_size;
ULONG	result;

#ifdef DEBUG
ib_fprintf (ib_stderr, "port_buff_size = %d fmt_net_length = %d fmt_length = %d overhead = %d\n",
	port->port_buff_size , 
	format->fmt_net_length,
	format->fmt_length,
	op_overhead);
#endif

if (port->port_flags & PORT_symmetric)
    {
    /* Same architecture connection */
    row_size = 
	(ROUNDUP (format->fmt_length, 4) 
	    + op_overhead);
    }
else 
    {
    /* Using XDR for data transfer */
    row_size =
	(ROUNDUP (format->fmt_net_length, 4) 
	    + op_overhead);
    }

num_packets = ((DESIRED_ROWS_PER_BATCH * row_size)  /* data set */
		+ buffer_used			    /* used in 1st pkt */
		+ (port->port_buff_size - 1))	    /* to round up */
	   / port->port_buff_size;
if (num_packets > MAX_PACKETS_PER_BATCH)
    {
    num_packets = ((MIN_ROWS_PER_BATCH * row_size)          /* data set */
		    + buffer_used			    /* used in 1st pkt */
		    + (port->port_buff_size - 1))	    /* to round up */
	       / port->port_buff_size;
    }
num_packets = MAX (num_packets, MIN_PACKETS_PER_BATCH);

/* Now that we've picked the number of packets in a batch,
   pack as many rows as we can into the set of packets */

result = (num_packets * port->port_buff_size - buffer_used) / row_size;

/* Must always send some messages, even if message size is more 
   than packet size. */

result = MAX (result, MIN_ROWS_PER_BATCH);

#ifdef DEBUG
{
char *p;
if (p = getenv ("DEBUG_BATCH_SIZE"))
    result = atoi (p);
ib_fprintf (ib_stderr, "row_size = %d num_packets = %d\n", 
	row_size, num_packets);
ib_fprintf (ib_stderr, "result = %d\n", result);
}
#endif

return result;
}

RRQ DLL_EXPORT REMOTE_find_request (
    RRQ		request,
    USHORT	level)
{
/**************************************
 *
 *	R E M O T E _ f i n d _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Find sub-request if level is non-zero.
 *
 **************************************/
MSG		msg;
FMT		format;
struct rrq_repeat	*tail, *end;

/* See if we already know about the request level */

for (;;)
    {
    if (request->rrq_level == level)
	return request;
    if (!request->rrq_levels)
	break;
    request = request->rrq_levels;
    }

/* This is a new level -- make up a new request block. */

request->rrq_levels = (RRQ) ALLR_clone (request);
/* NOMEM: handled by ALLR_clone, FREE: REMOTE_remove_request() */
#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("REMOTE_find_request       allocate request %x\n", request->rrq_levels);
#endif
request = request->rrq_levels;
request->rrq_level = level;
request->rrq_levels = NULL;

/* Allocate message block for known messages */

tail = request->rrq_rpt;
end = tail + request->rrq_max_msg;
for (; tail <= end; tail++)
    {
    if (!(format = tail->rrq_format))
	continue;
    tail->rrq_xdr = msg = (MSG) ALLOCV (type_msg, format->fmt_length);
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("REMOTE_find_request       allocate message %x\n", msg);
#endif
    msg->msg_next = msg;
#ifdef SCROLLABLE_CURSORS
    msg->msg_prior = msg; 
#endif
    msg->msg_number = tail->rrq_message->msg_number;
    tail->rrq_message = msg;
    }

return request;
}

void DLL_EXPORT REMOTE_free_packet (
    PORT	port,
    PACKET	*packet)
{
/**************************************
 *
 *	R E M O T E _ f r e e _ p a c k e t
 *
 **************************************
 *
 * Functional description
 *	Zero out a packet block.
 *
 **************************************/
XDR	xdr;
USHORT	n;

if (packet)
    {
    xdrmem_create (&xdr, packet, sizeof (PACKET), XDR_FREE);
    xdr.x_public = (caddr_t) port;

    for (n = (USHORT) op_connect; n < (USHORT) op_max; n++)
    	{
    	packet->p_operation = (P_OP) n;
    	xdr_protocol (&xdr, packet);
    	}
#ifdef DEBUG_XDR_MEMORY
    /* All packet memory allocations should now be voided. */

    for (n = 0; n < P_MALLOC_SIZE; n++)
	assert (packet->p_malloc[n].p_operation == op_void);
#endif
    packet->p_operation = op_void;
    }
}

void DLL_EXPORT REMOTE_get_timeout_params (
    PORT        port,
    UCHAR       *dpb, 
    USHORT      dpb_length)
{
/**************************************
 *
 *	R E M O T E _ g e t _ t i m e o u t _ p a r a m s
 *
 **************************************
 *
 * Functional description
 *	Determine the connection timeout parameter values for this newly created
 *	port.  If the client did a specification in the DPB, use those values.
 *	Otherwise, see if there is anything in the configuration file.  The
 *	configuration file management code will set the default values if there
 *	is no other specification.
 *
 **************************************/
UCHAR   *p, *end;
USHORT  len;
BOOLEAN	got_dpb_connect_timeout, got_dpb_dummy_packet_interval;

assert (isc_dpb_connect_timeout == isc_spb_connect_timeout);
assert (isc_dpb_dummy_packet_interval == isc_spb_dummy_packet_interval);

got_dpb_connect_timeout = FALSE;
got_dpb_dummy_packet_interval = FALSE;
port->port_flags &= ~PORT_dummy_pckt_set;

if (dpb && dpb_length)
    {
    p = dpb;
    end = p + dpb_length;

    if (*p++ == gds__dpb_version1)
	{
	while (p < end)
	    switch (*p++)
		{
		case gds__dpb_connect_timeout:
		    port->port_connect_timeout = get_parameter(&p);
		    got_dpb_connect_timeout = TRUE;
		    break;

		case gds__dpb_dummy_packet_interval:
		    port->port_dummy_packet_interval = get_parameter(&p);
		    got_dpb_dummy_packet_interval = TRUE;
                    port->port_flags |= PORT_dummy_pckt_set;
		    break;
	    
		case isc_dpb_sys_user_name:
		    /** Store the user name in thread specific storage.
			We need this later while expanding filename to
			get the users home directory.
			Normally the working directory is stored in
			the attachment but in this case the attachment is 
			not yet created.
			Also note that the thread performing this task
			has already called THREAD_ENTER
		    **/
		    {
		    char *t_data;
		    int l, i=0;

		    THD_init_data();

		    l = *(p++);
		    if (l)
		        {
		        t_data = malloc(l+1);
		        do
			    { 
			    t_data[i] = *p;
			    if (t_data[i]=='.') t_data[i]=0;
			    i++;p++;
			    }while (--l);
		        }
		    else
		        t_data = malloc(1);
		    t_data[i] = 0;


		    THD_putspecific_data((void*)t_data);

		    }
		    break;
	    
		default:
		    /* Skip over this parameter - not important to us */
		    len = *p++;
		    p += len;
		    break;
		}
	}
    }

if (!got_dpb_connect_timeout || !got_dpb_dummy_packet_interval)
    {
    /* Didn't find all parameters in the dpb, fetch configuration
       information from the configuration file and set the missing
       values */

    ISC_get_config (LOCK_HEADER, INET_cfgtbl);

    if (!got_dpb_connect_timeout)
	port->port_connect_timeout = conn_timeout;

    if (!got_dpb_dummy_packet_interval)
        {
        if (INET_cfgtbl[DUMMY_PACKET_INTRVL_IDX].ipccfg_found)
           port->port_flags |= PORT_dummy_pckt_set;
        port->port_dummy_packet_interval = dummy_interval;
        }
    }
/* Insure a meaningful keepalive interval has been set. Otherwise, too
   many keepalive packets will drain network performance. */

if (port->port_dummy_packet_interval < 0)
    port->port_dummy_packet_interval = DUMMY_INTERVAL;

port->port_dummy_timeout = port->port_dummy_packet_interval;
#ifdef DEBUG
ib_printf ("REMOTE_get_timeout dummy = %d conn = %d\n", port->port_dummy_packet_interval, 
		port->port_connect_timeout);
ib_fflush (ib_stdout);
#endif
}

STR DLL_EXPORT REMOTE_make_string (
    SCHAR	*input)
{
/**************************************
 *
 *	R E M O T E _ m a k e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Copy a given string to a permanent location, returning
 *	address of new string.
 *
 **************************************/
STR	string;

string = (STR) ALLOCV (type_str, strlen (input));
#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("REMOTE_make_string        allocate string  %x\n", string);
#endif
strcpy (string->str_data, input);

return string;
}

void DLL_EXPORT REMOTE_release_messages (
    MSG		messages)
{
/**************************************
 *
 *	R E M O T E _ r e l e a s e _ m e s s a g e s
 *
 **************************************
 *
 * Functional description
 *	Release a circular list of messages.
 *
 **************************************/
MSG	message, temp;

if (message = messages)
    while (TRUE)
	{
	temp = message;
	message = message->msg_next;	    	    
#ifdef REMOTE_DEBUG_MEMORY
	ib_printf ("REMOTE_release_messages   free message     %x\n", temp);
#endif
	ALLR_release (temp);
	if (message == messages)
	    break;
	}
}

void DLL_EXPORT REMOTE_release_request (
    RRQ		request)
{
/**************************************
 *
 *	R E M O T E _ r e l e a s e _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Release a request block and friends.
 *
 **************************************/
MSG		message, temp;
RDB		rdb;
RRQ		*p, next;
struct rrq_repeat	*tail, *end;

rdb = request->rrq_rdb;

for (p = &rdb->rdb_requests; *p; p = &(*p)->rrq_next)
    if (*p == request)
	{
	*p = request->rrq_next;
	break;
	}

/* Get rid of request and all levels */

for (;;)
    {
    tail = request->rrq_rpt;
    end = tail + request->rrq_max_msg;
    for (; tail <= end; tail++)
	if (message = tail->rrq_message)
	    {
	    if (!request->rrq_level)
                {
#ifdef REMOTE_DEBUG_MEMORY
                ib_printf ("REMOTE_release_request    free format      %x\n", tail->rrq_format);
#endif
                ALLR_release (tail->rrq_format);
                }
	    REMOTE_release_messages (message);
	    }
    next = request->rrq_levels;
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("REMOTE_release_request    free request     %x\n", request);
#endif
    ALLR_release (request);
    if (!(request = next))
	break;
    }
}

void DLL_EXPORT REMOTE_reset_request (
    RRQ		request,
    MSG		active_message)
{
/**************************************
 *
 *	R E M O T E _ r e s e t _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Clean up a request in preparation to use it again.  Since
 *	there may be an active message (start_and_send), exercise
 *	some care to avoid zapping that message.
 *
 **************************************/
MSG		message;
struct rrq_repeat	*tail, *end;

tail = request->rrq_rpt;

for (end = tail + request->rrq_max_msg; tail <= end; tail++)
    if ((message = tail->rrq_message) != NULL && message != active_message)
	{
	tail->rrq_xdr = message;
	tail->rrq_rows_pending = 0;
	tail->rrq_reorder_level = 0;
	tail->rrq_batch_count = 0;
	while (TRUE)
	    {
	    message->msg_address = NULL;
	    message = message->msg_next;
	    if (message == tail->rrq_message)
		break;
	    }
	}

/* Initialize the request status to SUCCESS */

request->rrq_status_vector [1] = 0;
}

void DLL_EXPORT REMOTE_reset_statement (
    RSR		statement)
{
/**************************************
 *
 *	R E M O T E _ r e s e t _ s t a t e m e n t
 *
 **************************************
 *
 * Functional description
 *	Reset a statement by releasing all buffers except 1
 *
 **************************************/
MSG	message, temp;

if ((!statement) || (!(message = statement->rsr_message)))
    return;

/* Reset all the pipeline counters */

statement->rsr_rows_pending=0;
statement->rsr_msgs_waiting=0;
statement->rsr_reorder_level=0;
statement->rsr_batch_count=0;

/* only one entry */

if (message->msg_next == message)
    return;

/* find the entry before statement->rsr_message */

for (temp = message->msg_next; temp->msg_next != message; temp = temp->msg_next)
    ;

temp->msg_next = message->msg_next;
message->msg_next = message;
#ifdef SCROLLABLE_CURSORS 
message->msg_prior = message; 
#endif

statement->rsr_buffer = statement->rsr_message;

REMOTE_release_messages (temp);
}

void REMOTE_save_status_strings (
    STATUS	*vector)
{
/**************************************
 *
 *	R E M O T E _ s a v e _ s t a t u s _ s t r i n g s
 *
 **************************************
 *
 * Functional description
 *	There has been a failure during attach/create database.
 *	The included string have been allocated off of the database block,
 *	which is going to be released before the message gets passed 
 *	back to the user.  So, to preserve information, copy any included
 *	strings to a special buffer.
 *
 **************************************/
TEXT	*p;
STATUS	status;
USHORT	l;

if (!attach_failures)
    {
    attach_failures = (TEXT*) ALLOC_LIB_MEMORY ((SLONG) ATTACH_FAILURE_SPACE);
    /* FREE: freed by exit handler cleanup_memory() */
    if (!attach_failures)	/* NOMEM: don't bother trying to copy */
	return;
#ifdef DEBUG_GDS_ALLOC
    /* This buffer is freed by the exit handler - but some of the
     * reporting mechanisms will report it anyway, so flag it as
     * "known unfreed" to get the reports quiet.
     */
    gds_alloc_flag_unfreed ((void *) attach_failures);
#endif DEBUG_GDS_ALLOC
    attach_failures_ptr = attach_failures;
    gds__register_cleanup (cleanup_memory, NULL_PTR);
    }

while (*vector)
    {
    status = *vector++;
    switch (status)
	{
	case gds_arg_cstring:
	    l = *vector++;

	case gds_arg_interpreted:
	case gds_arg_string:
	    p = (TEXT*) *vector;
	    if (status != gds_arg_cstring)
		l = strlen (p) + 1;

	    /* If there isn't any more room in the buffer,
	       start at the beginning again */

	    if (attach_failures_ptr + l > attach_failures + ATTACH_FAILURE_SPACE)
		attach_failures_ptr = attach_failures;
 	    *vector++ = (STATUS) attach_failures_ptr;
	    while (l--)
		*attach_failures_ptr++ = *p++;
	    break;

	default:
	    ++vector;
	    break;
	}
    }
}

OBJCT DLL_EXPORT REMOTE_set_object (
    PORT	port,
    BLK		object,
    OBJCT	slot)
{
/**************************************
 *
 *	R E M O T E _ s e t _ o b j e c t
 *
 **************************************
 *
 * Functional description
 *	Set an object into the object vector.
 *
 **************************************/
VEC	vector, new_vector;
BLK	*p, *q, *end;

/* If it fits, do it */

if (((vector = port->port_object_vector) != NULL) &&
    slot < vector->vec_count)
    {
    vector->vec_object [slot] = object;
    return slot;
    }

/* Prevent the creation of object handles that can't be
   transferred by the remote protocol. */

if (slot + 10 > MAX_OBJCT_HANDLES)
    return (OBJCT) NULL;

port->port_object_vector = new_vector = (VEC) ALLOCV (type_vec, slot + 10);
#ifdef REMOTE_DEBUG_MEMORY
ib_printf ("REMOTE_set_object         allocate vector  %x\n", new_vector);
#endif
port->port_objects = new_vector->vec_object;
new_vector->vec_count = slot + 10;

if (vector)
    {
    p = new_vector->vec_object;
    q = vector->vec_object;
    end = q + (int) vector->vec_count;
    while (q < end)
	*p++ = *q++;
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("REMOTE_release_request    free vector      %x\n", vector);
#endif
    ALLR_release (vector);
    }

new_vector->vec_object [slot] = object;

return slot;
}

#ifdef	WINDOWS_ONLY
void	REMOTE_wep( void)
{
/**************************************
 *
 *	R E M O T E _ w e p
 *
 **************************************
 *
 * Functional description
 *	Call cleanup_memory for WEP.
 *
 **************************************/

cleanup_memory( NULL);
}
#endif

static void cleanup_memory (
    void	*block)
{
/**************************************
 *
 *	c l e a n u p _ m e m o r y
 *
 **************************************
 *
 * Functional description
 *	Cleanup any allocated memory.
 *
 **************************************/

if (attach_failures)
    FREE_LIB_MEMORY (attach_failures);

gds__unregister_cleanup (cleanup_memory, NULL_PTR);
attach_failures = NULL;
}

static SLONG get_parameter (
    UCHAR	**ptr)
{
/**************************************
 *
 *	g e t _ p a r a m e t e r
 *
 **************************************
 *
 * Functional description
 *	Pick up a VAX format parameter from a parameter block, including the
 *	length byte.
 *	This is a clone of jrd/jrd.c:get_parameter()
 *
 **************************************/
SLONG	parameter;
SSHORT	l;

l = *(*ptr)++;
parameter = gds__vax_integer (*ptr, l);
*ptr += l;

return parameter;
}
