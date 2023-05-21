/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         alt.c
 *      DESCRIPTION:    Alternative entrypoints
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

#define IS_VALID_SERVER(server) if (!server || !*server) \
				    { \
                                    status[0] = isc_arg_gds; \
                                    status[1] = isc_bad_protocol; \
                                    status[2] = isc_arg_end; \
                                    return NULL; \
				    }

#if (defined APOLLO && defined __STDC__)
#define VA_START(list,parmN)	va_start (list, parmN)
#endif

#include <stdarg.h>

#include "../jrd/pwd.h"
#include "../jrd/gds.h"
#include "../jrd/gds_proto.h"				 
#include "../jrd/utl_proto.h"
#include "../jrd/why_proto.h"
#include "../utilities/gsec.h"
#include "../utilities/secur_proto.h"

typedef struct teb {
    void	**teb_database;
    int		teb_tpb_length;
    UCHAR	*teb_tpb;
} TEB;

void* open_security_db (STATUS *, TEXT *, TEXT *, int, TEXT *);
void  get_security_error (STATUS *, int);

SLONG API_ROUTINE_VARARG isc_event_block (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    USHORT	count,
    ...)
{
/**************************************
 *
 *      i s c _ e v e n t _ b l o c k
 *
 **************************************
 *
 * Functional description
 *      Create an initialized event parameter block from a
 *      variable number of input arguments.
 *      Return the size of the block.
 *
 *	Return 0 if any error occurs.
 *
 **************************************/
register SCHAR	*p, *q;
SCHAR		*end;
SLONG		length;
USHORT		i;
va_list		ptr;

VA_START (ptr, count);

/* calculate length of event parameter block, 
   setting initial length to include version
   and counts for each argument */

length = 1;
i = count;
while (i--)
    {
    q = va_arg (ptr, SCHAR*);
    length += strlen (q) + 5;
    }

p = *event_buffer = (SCHAR*) gds__alloc ((SLONG)length);
/* FREE: apparently never freed */
if (!*event_buffer)	/* NOMEM: */
    return 0;
if ((*result_buffer = (SCHAR*) gds__alloc ((SLONG)length)) == NULL)	/* NOMEM: */
/* FREE: apparently never freed */
    {
    gds__free (*event_buffer);
    *event_buffer = NULL;
    return 0;
    }

#ifdef DEBUG_GDS_ALLOC
/* I can find no place where these are freed */
/* 1994-October-25 David Schnepper  */
gds_alloc_flag_unfreed ((void *) *event_buffer);
gds_alloc_flag_unfreed ((void *) *result_buffer);
#endif DEBUG_GDS_ALLOC
	 
/* initialize the block with event names and counts */

*p++ = 1;  
		
VA_START (ptr, count);

i = count;
while (i--)
    {
    q = va_arg (ptr, SCHAR*);

    /* Strip the blanks from the ends */

    for (end = q + strlen (q); --end >= q && *end == ' ';)
	;
    *p++ = end - q + 1;
    while (q <= end)
	*p++ = *q++;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    }

return (int) (p - *event_buffer);
}

USHORT API_ROUTINE isc_event_block_a (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    USHORT	count,
    TEXT	**name_buffer)
{
/**************************************
 *
 *	i s c _ e v e n t _ b l o c k _ a 
 *
 **************************************
 *
 * Functional description
 *	Create an initialized event parameter block from a
 *	vector of input arguments. (Ada needs this)
 *	Assume all strings are 31 characters long.
 *	Return the size of the block.
 *
 **************************************/
#define 	MAX_NAME_LENGTH		31
register SCHAR	*p, *q;
SCHAR		*end;
TEXT		**nb;
SLONG		length;
USHORT		i;

/* calculate length of event parameter block, 
   setting initial length to include version
   and counts for each argument */

i = count;
nb = name_buffer;
length = 0;
while (i--)
    {
    q =  *nb++;

    /* Strip trailing blanks from string */

    for (end = q + MAX_NAME_LENGTH ; --end >= q && *end == ' ';)
	;
    length += end - q + 1 + 5;
    }


i = count;
p = *event_buffer = (SCHAR*) gds__alloc ((SLONG)length);
/* FREE: apparently never freed */
if (!(*event_buffer))	/* NOMEM: */
    return 0;
if ((*result_buffer = (SCHAR*) gds__alloc ((SLONG)length)) == NULL)	/* NOMEM: */
/* FREE: apparently never freed */
    {
    gds__free (*event_buffer);
    *event_buffer = NULL;
    return 0;
    }

#ifdef DEBUG_GDS_ALLOC
/* I can find no place where these are freed */
/* 1994-October-25 David Schnepper  */
gds_alloc_flag_unfreed ((void *) *event_buffer);
gds_alloc_flag_unfreed ((void *) *result_buffer);
#endif DEBUG_GDS_ALLOC

*p++ = 1;  
                
nb = name_buffer;

while (i--)
    {
    q =  *nb++;

    /* Strip trailing blanks from string */

    for (end = q + MAX_NAME_LENGTH ; --end >= q && *end == ' ';)
	;
    *p++ = end - q + 1;
    while (q <= end)
	*p++ = *q++;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    }

return (p - *event_buffer);
}

void API_ROUTINE isc_event_block_s (
    SCHAR	**event_buffer,
    SCHAR	**result_buffer,
    USHORT	count,
    TEXT	**name_buffer,
    USHORT	*return_count)
{
/**************************************
 *
 *	i s c _ e v e n t _ b l o c k _ s
 *
 **************************************
 *
 * Functional description
 *	THIS IS THE COBOL VERSION of gds__event_block_a for Cobols
 *	that don't permit return values.
 *
 **************************************/

*return_count = isc_event_block_a (event_buffer, result_buffer, count, name_buffer);
}

STATUS API_ROUTINE_VARARG gds__start_transaction (
    STATUS	*status_vector,
    void	**tra_handle,
    SSHORT	count,
    ...)
{
TEB     tebs [16], *teb, *end;
STATUS	status;
va_list ptr;

if (count <= sizeof (tebs) / sizeof (struct teb))
    teb = tebs;
else 
    teb = (TEB*) gds__alloc (((SLONG)sizeof (struct teb) * count));
    /* FREE: later in this module */

if (!teb)	/* NOMEM: */
    {
    status_vector [0] = gds_arg_gds;
    status_vector [1] = gds__virmemexh;
    status_vector [2] = gds_arg_end;
    return status_vector [1];
    }

end = teb + count;
VA_START (ptr, count);

for (; teb < end; teb++)
    {
    teb->teb_database = va_arg (ptr, void**);
    teb->teb_tpb_length = va_arg (ptr, int);
    teb->teb_tpb = va_arg (ptr, UCHAR*);
    }

teb = end - count;

status = isc_start_multiple (status_vector, tra_handle, count, teb);

if (teb != tebs)
    gds__free (teb);

return status;
}

STATUS API_ROUTINE gds__attach_database (
    STATUS	*status_vector,
    SSHORT	file_length,
    SCHAR       *file_name,
    void	**db_handle,
    SSHORT	dpb_length,
    SCHAR       *dpb)
{
return isc_attach_database (GDS_VAL(status_vector),
	file_length,
	GDS_VAL(file_name),
	GDS_VAL(db_handle),
	dpb_length,
	GDS_VAL(dpb));
}

STATUS API_ROUTINE gds__blob_info (
    STATUS	*status_vector,
    void	**blob_handle,
    SSHORT	msg_length,
    SCHAR	*msg,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
return isc_blob_info (GDS_VAL(status_vector),
	GDS_VAL(blob_handle),
	msg_length,
	GDS_VAL(msg),
	buffer_length,
	GDS_VAL(buffer));
}

STATUS API_ROUTINE gds__cancel_blob (
    STATUS	*status_vector,
    void	**blob_handle)
{
return isc_cancel_blob (GDS_VAL(status_vector),
	GDS_VAL(blob_handle));
}

STATUS API_ROUTINE gds__cancel_events (
    STATUS	*status_vector,
    void	**db_handle,
    SLONG	*event_id)
{
return isc_cancel_events (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(event_id));
}

STATUS API_ROUTINE gds__close_blob (
    STATUS	*status_vector,
    void	**blob_handle)
{
return isc_close_blob (GDS_VAL(status_vector),
	GDS_VAL(blob_handle));
}

STATUS API_ROUTINE gds__commit_retaining (
    STATUS	*status_vector,
    void	**tra_handle)
{
return isc_commit_retaining (GDS_VAL(status_vector),
	GDS_VAL(tra_handle));
}

STATUS API_ROUTINE gds__commit_transaction (
    STATUS	*status_vector,
    void	**tra_handle)
{
return isc_commit_transaction (GDS_VAL(status_vector),
	GDS_VAL(tra_handle));
}

STATUS API_ROUTINE gds__compile_request (
    STATUS	*status_vector,
    void	**db_handle,
    void	**req_handle,
    SSHORT	blr_length,
    SCHAR	*blr)
{
return isc_compile_request (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(req_handle),
	blr_length,
	GDS_VAL(blr));
}

STATUS API_ROUTINE gds__compile_request2 (
    STATUS	*status_vector,
    void	**db_handle,
    void	**req_handle,
    SSHORT	blr_length,
    SCHAR	*blr)
{
return isc_compile_request2 (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(req_handle),
	blr_length,
	GDS_VAL(blr));
}

STATUS API_ROUTINE gds__create_blob (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    void	**blob_handle,
    GDS_QUAD	*blob_id)
{
return isc_create_blob (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(blob_handle),
	GDS_VAL(blob_id));
}

STATUS API_ROUTINE gds__create_blob2 (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    void	**blob_handle,
    GDS_QUAD	*blob_id,
    SSHORT	bpb_length,
    SCHAR	*bpb)
{
return isc_create_blob2 (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(blob_handle),
	GDS_VAL(blob_id),
	bpb_length,
	GDS_VAL(bpb));
}

STATUS API_ROUTINE gds__create_database (
    STATUS	*status_vector,
    SSHORT	file_length,
    SCHAR	*file_name,
    void	**db_handle,
    SSHORT	dpb_length,
    SCHAR	*dpb,
    SSHORT	db_type)
{
return isc_create_database (GDS_VAL(status_vector),
	file_length,
	GDS_VAL(file_name),
	GDS_VAL(db_handle),
	dpb_length,
	GDS_VAL(dpb),
	db_type);
}

STATUS API_ROUTINE isc_database_cleanup (
    STATUS	*status_vector,
    void	**db_handle,
    void        (*routine)(),
    SCHAR	*arg)
{

return gds__database_cleanup (GDS_VAL(status_vector),
	db_handle,
	routine,
	(SLONG) arg);
}

STATUS API_ROUTINE gds__database_info (
    STATUS	*status_vector,
    void	**db_handle,
    SSHORT	msg_length,
    SCHAR	*msg,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
return isc_database_info (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	msg_length,
	GDS_VAL(msg),
	buffer_length,
	GDS_VAL(buffer));
}

STATUS API_ROUTINE gds__detach_database (
    STATUS	*status_vector,
    void	**db_handle)
{
return isc_detach_database (GDS_VAL(status_vector),
	GDS_VAL(db_handle));
}

STATUS API_ROUTINE gds__get_segment (
    STATUS	*status_vector,
    void	**blob_handle,
    USHORT	*return_length,
    USHORT	buffer_length,
    SCHAR	*buffer)
{
return isc_get_segment (GDS_VAL(status_vector),
	GDS_VAL(blob_handle),
	GDS_VAL(return_length),
	buffer_length,
	GDS_VAL(buffer));
}

STATUS API_ROUTINE gds__get_slice (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    GDS_QUAD	*array_id,
    SSHORT	sdl_length,
    SCHAR	*sdl,
    SSHORT	parameters_leng,
    SLONG	*parameters,
    SLONG	slice_length,
    void	*slice,
    SLONG	*return_length)
{
return isc_get_slice (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(array_id),
	sdl_length,
	GDS_VAL(sdl),
	parameters_leng,
	GDS_VAL(parameters),
	slice_length,
	GDS_VAL((SCHAR*) slice),
	GDS_VAL(return_length));
}

STATUS API_ROUTINE gds__open_blob (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    void	**blob_handle,
    GDS_QUAD	*blob_id)
{
return isc_open_blob (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(blob_handle),
	GDS_VAL(blob_id));
}

STATUS API_ROUTINE gds__open_blob2 (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    void	**blob_handle,
    GDS_QUAD	*blob_id,
    SSHORT	bpb_length,
    SCHAR	*bpb)
{
return isc_open_blob2 (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(blob_handle),
	GDS_VAL(blob_id),
	bpb_length,
	GDS_VAL(bpb));
}

STATUS API_ROUTINE gds__prepare_transaction (
    STATUS	*status_vector,
    void	**tra_handle)
{
return isc_prepare_transaction (GDS_VAL(status_vector),
	GDS_VAL(tra_handle));
}

STATUS API_ROUTINE gds__prepare_transaction2 (
    STATUS	*status_vector,
    void	**tra_handle,
    SSHORT	msg_length,
    SCHAR	*msg)
{
return isc_prepare_transaction2 (GDS_VAL(status_vector),
	GDS_VAL(tra_handle),
	msg_length,
	GDS_VAL(msg));
}

STATUS API_ROUTINE gds__put_segment (
    STATUS	*status_vector,
    void	**blob_handle,
    USHORT	segment_length,
    SCHAR	*segment)
{
return isc_put_segment (GDS_VAL(status_vector),
	GDS_VAL(blob_handle),
	segment_length,
	GDS_VAL(segment));
}

STATUS API_ROUTINE gds__put_slice (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    GDS_QUAD	*array_id,
    SSHORT	sdl_length,
    SCHAR	*sdl,
    SSHORT	parameters_leng,
    SLONG	*parameters,
    SLONG	slice_length,
    void	*slice)
{
return isc_put_slice (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	GDS_VAL(array_id),
	sdl_length,
	GDS_VAL(sdl),
	parameters_leng,
	GDS_VAL(parameters),
	slice_length,
	GDS_VAL((SCHAR*) slice));
}

STATUS API_ROUTINE gds__que_events (
    STATUS	*status_vector,
    void	**db_handle,
    SLONG	*event_id,
    SSHORT	events_length,
    SCHAR	*events,
    void         (*ast_address)(),
    void         *ast_argument)
{
return isc_que_events (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(event_id),
	events_length,
	GDS_VAL(events),
	GDS_VAL(ast_address),
	GDS_VAL((int*) ast_argument));
}

STATUS API_ROUTINE gds__receive (
    STATUS	*status_vector,
    void	**req_handle,
    SSHORT	msg_type,
    SSHORT	msg_length,
    void	*msg,
    SSHORT	req_level)
{
return isc_receive (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	msg_type,
	msg_length,
	GDS_VAL((SCHAR*) msg),
	req_level);
}

STATUS API_ROUTINE gds__reconnect_transaction (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    SSHORT	msg_length,
    SCHAR	*msg)
{
return isc_reconnect_transaction (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	msg_length,
	GDS_VAL(msg));
}

STATUS API_ROUTINE gds__release_request (
    STATUS	*status_vector,
    void	**req_handle)
{
return isc_release_request (GDS_VAL(status_vector),
	GDS_VAL(req_handle));
}

STATUS API_ROUTINE gds__request_info (
    STATUS	*status_vector,
    void	**req_handle,
    SSHORT	req_level,
    SSHORT	msg_length,
    SCHAR	*msg,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
return isc_request_info (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	req_level,
	msg_length,
	GDS_VAL(msg),
	buffer_length,
	GDS_VAL(buffer));
}

STATUS API_ROUTINE gds__rollback_transaction (
    STATUS	*status_vector,
    void	**tra_handle)
{
return isc_rollback_transaction (GDS_VAL(status_vector),
	GDS_VAL(tra_handle));
}

STATUS API_ROUTINE gds__seek_blob (
    STATUS	*status_vector,
    void	**blob_handle,
    SSHORT	mode,
    SLONG	offset,
    SLONG	*result)
{
return isc_seek_blob (GDS_VAL(status_vector),
	GDS_VAL(blob_handle),
	mode,
	offset,
	GDS_VAL (result));
}

STATUS API_ROUTINE gds__send (
    STATUS	*status_vector,
    void	**req_handle,
    SSHORT	msg_type,
    SSHORT	msg_length,
    void	*msg,
    SSHORT	req_level)
{
return isc_send (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	msg_type,
	msg_length,
	GDS_VAL((SCHAR*) msg),
	req_level);
}

STATUS API_ROUTINE gds__start_and_send (
    STATUS	*status_vector,
    void	**req_handle,
    void	**tra_handle,
    SSHORT	msg_type,
    SSHORT	msg_length,
    void	*msg,
    SSHORT	req_level)
{
return isc_start_and_send (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	GDS_VAL(tra_handle),
	msg_type,
	msg_length,
	GDS_VAL((SCHAR*) msg),
	req_level);
}

STATUS API_ROUTINE gds__start_multiple (
    STATUS	*status_vector,
    void	**tra_handle,
    SSHORT	db_count,
    void        *teb_vector)
{
return isc_start_multiple (GDS_VAL(status_vector),
	GDS_VAL(tra_handle),
	db_count,
	GDS_VAL((SCHAR*) teb_vector));
}

STATUS API_ROUTINE gds__start_request (
    STATUS	*status_vector,
    void	**req_handle,
    void	**tra_handle,
    SSHORT	req_level)
{
return isc_start_request (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	GDS_VAL(tra_handle),
	req_level);
}

STATUS API_ROUTINE gds__transaction_info (
    STATUS	*status_vector,
    void	**tra_handle,
    SSHORT	msg_length,
    SCHAR	*msg,
    SSHORT	buffer_length,
    SCHAR	*buffer)
{
return isc_transaction_info (GDS_VAL(status_vector),
	GDS_VAL(tra_handle),
	msg_length,
	GDS_VAL(msg),
	buffer_length,
	GDS_VAL(buffer));
}

STATUS API_ROUTINE gds__unwind_request (
    STATUS	*status_vector,
    void	**req_handle,
    SSHORT	req_level)
{
return isc_unwind_request (GDS_VAL(status_vector),
	GDS_VAL(req_handle),
	req_level);
}

STATUS API_ROUTINE gds__ddl (
    STATUS	*status_vector,
    void	**db_handle,
    void	**tra_handle,
    SSHORT	ddl_length,
    SCHAR	*ddl)
{
return isc_ddl (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	GDS_VAL(tra_handle),
	ddl_length,
	GDS_VAL(ddl));
}

void API_ROUTINE isc_event_counts (
    ULONG	*result_vector,
    SSHORT	length,
    SCHAR	*before,
    SCHAR	*after)
{
gds__event_counts (GDS_VAL(result_vector),
	length,
	GDS_VAL(before),
	GDS_VAL(after));
}

SLONG API_ROUTINE isc_free (
    SCHAR	*blk) 
{

return gds__free ((SLONG *)blk);
}

SLONG API_ROUTINE isc_ftof (
    SCHAR	*string1,
    USHORT	length1,
    SCHAR	*string2,
    USHORT	length2)
{
return gds__ftof (GDS_VAL(string1),
	length1,
	GDS_VAL(string2),
	length2);
}

STATUS API_ROUTINE isc_print_blr (
    SCHAR	*blr,
    void         (*callback)(),
    void         *callback_argument,
    SSHORT	language)
{
return gds__print_blr (blr, callback, callback_argument, language);
}

STATUS API_ROUTINE isc_print_status (
    STATUS	*status_vector)
{
return gds__print_status (GDS_VAL(status_vector));
}

void API_ROUTINE isc_qtoq (
    GDS_QUAD	*quad1,
    GDS_QUAD	*quad2)
{
gds__qtoq (quad1, quad2);
}

SLONG API_ROUTINE isc_sqlcode (
    STATUS	*status_vector)
{
return gds__sqlcode (GDS_VAL(status_vector));
}

void API_ROUTINE isc_sqlcode_s (
    STATUS	*status_vector,
    ULONG	*sqlcode)
{
*sqlcode = gds__sqlcode (status_vector);
return;
}

void API_ROUTINE isc_vtof (
    SCHAR	*string1,
    SCHAR	*string2,
    USHORT	length)
{
gds__vtof (string1,
	string2,
	length);
}

void API_ROUTINE isc_vtov (
    SCHAR	*string1,
    SCHAR	*string2,
    SSHORT	length)
{
gds__vtov (string1,
	string2,
	length);
}

void API_ROUTINE gds__decode_date (
    GDS_QUAD	*date,
    void	*time_structure)
{
isc_decode_date (date, time_structure);
}     

void API_ROUTINE gds__encode_date (
    void	*time_structure,
    GDS_QUAD	*date)
{
isc_encode_date (time_structure, date);
}

SLONG API_ROUTINE isc_vax_integer (
    SCHAR	*input,
    SSHORT	length)
{
return gds__vax_integer (input, length);
}

#ifndef REQUESTER
STATUS API_ROUTINE isc_wait_for_event (
    STATUS	*status_vector,
    void	**db_handle,
    SSHORT	events_length,
    SCHAR	*events,
    SCHAR	*events_update)
{
return gds__event_wait (GDS_VAL(status_vector),
	GDS_VAL(db_handle),
	events_length,
	GDS_VAL(events),
	GDS_VAL(events_update));
}
#endif

STATUS API_ROUTINE isc_interprete (
    SCHAR	*buffer,
    STATUS	**status_vector_p)
{
return gds__interprete (buffer,
	status_vector_p);
}

int API_ROUTINE isc_version (
    void	**db_handle,
    void         (*callback)(),
    void         *callback_argument)
{
return gds__version (db_handle,
	callback, callback_argument);
}

void API_ROUTINE isc_set_debug (
    int         flag)
{
#ifndef NETWARE_386
#ifndef SUPERCLIENT
#ifndef REQUESTER
gds__set_debug (flag);
#endif
#endif
#endif
}

#ifdef PC_PLATFORM
void gds__extend_dpb (
    SCHAR	**dpb,
    SSHORT	*dpb_size,
    SCHAR	*user_name,
    SCHAR	*password)
{
isc_expand_dpb (dpb, dpb_size,
	gds__dpb_user_name, user_name,
	gds__dpb_password, password,
	0);
}
#endif


int API_ROUTINE isc_blob_display (
    STATUS	*status_vector,
    GDS_QUAD	*blob_id,
    void	**database,
    void	**transaction,
    SCHAR	*field_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	i s c _ b l o b _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	PASCAL callable version of EDIT_blob.
 *
 **************************************/

if (status_vector)
    status_vector[1] = 0;

return blob__display (blob_id, database, transaction, field_name, name_length);
}

int API_ROUTINE isc_blob_dump (
    STATUS	*status_vector,
    GDS_QUAD	*blob_id,
    void	**database,
    void	**transaction,
    SCHAR	*file_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	i s c _ b l o b _ d u m p
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable dump
 *	into an internal dump call.
 *
 **************************************/

if (status_vector)
    status_vector[1] = 0;

return blob__dump (blob_id, database, transaction, file_name, name_length);
}

int API_ROUTINE isc_blob_edit (
    STATUS	*status_vector,
    GDS_QUAD	*blob_id,
    void	**database,
    void	**transaction,
    SCHAR	*field_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	b l o b _ $ e d i t
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable edit
 *	into an internal edit call.
 *
 **************************************/

if (status_vector)
    status_vector[1] = 0;

return blob__edit (blob_id, database, transaction, field_name, name_length);
}

int API_ROUTINE isc_add_user (
	STATUS		 *status,
	USER_SEC_DATA    *user_data)
{
/**************************************
 *
 *      i s c _ a d d _ u s e r
 *
 **************************************
 *
 * Functional description
 *      Adds a user to the server's security
 *	database.
 *      Return 0 if the user was added
 *
 *	    Return > 0 if any error occurs.
 *
 **************************************/
USHORT	retval = 0, l;
struct	user_data userInfo;
void	*db_handle;

userInfo.operation = ADD_OPER;

if (user_data->user_name)
    {
    if (strlen (user_data->user_name) > 32)
	{
        status[0] = isc_arg_gds;
        status[1] = isc_usrname_too_long;
        status[2] = isc_arg_end;
        return status[1];
	}   

    for (l = 0; user_data->user_name[l] != ' ' && l < strlen(user_data->user_name); l++)
	userInfo.user_name[l] = UPPER (user_data->user_name[l]);

    userInfo.user_name[l] = '\0';
    userInfo.user_name_entered = TRUE;
    }
else
    {
    status[0] = isc_arg_gds;
    status[1] = isc_usrname_required;
    status[2] = isc_arg_end;
    return status[1];
    }

if (user_data->password)
    {
    if (strlen(user_data->password) > 8)
        {
        status[0] = isc_arg_gds;
        status[1] = isc_password_too_long;
        status[2] = isc_arg_end;
        return status[1];
        }

    for (l = 0; l < strlen(user_data->password) && user_data->password[l] != ' '; l++)
	userInfo.password[l] = user_data->password[l];

    userInfo.password[l] = '\0';
    userInfo.password_entered = TRUE;
    userInfo.password_specified = TRUE;
    }
else
    {
    status[0] = isc_arg_gds;
    status[1] = isc_password_required;
    status[2] = isc_arg_end;
    return status[1];
    }


if ((user_data->sec_flags & sec_uid_spec) && (userInfo.uid_entered = user_data->uid))
    {
    userInfo.uid = user_data->uid;
    userInfo.uid_specified = TRUE;
    }
else
    {
    userInfo.uid_specified = FALSE;
    userInfo.uid_entered = FALSE;
    }

if ((user_data->sec_flags & sec_gid_spec) && (userInfo.gid_entered = user_data->gid)) 
    {
    userInfo.gid = user_data->gid;
    userInfo.gid_specified = TRUE;
    }
else
    {
    userInfo.gid_specified = FALSE;
    userInfo.gid_entered = FALSE;
    }

if ((user_data->sec_flags & sec_group_name_spec) && user_data->group_name)
    {
    int l = MIN(ALT_NAME_LEN-1, strlen (user_data->group_name));
    strncpy (userInfo.group_name, user_data->group_name, l);
    userInfo.group_name[l] = '\0';
    userInfo.group_name_entered = TRUE;
    userInfo.group_name_specified = TRUE;
    }
else
    {
    userInfo.group_name_entered = FALSE;
    userInfo.group_name_specified = FALSE;
    }

if ((user_data->sec_flags & sec_first_name_spec) && user_data->first_name)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->first_name));
    strncpy (userInfo.first_name, user_data->first_name, l);
    userInfo.first_name[l] = '\0';
    userInfo.first_name_entered = TRUE;
    userInfo.first_name_specified = TRUE;
    }
else
    {
    userInfo.first_name_entered = FALSE;
    userInfo.first_name_specified = FALSE;
    }

if ((user_data->sec_flags & sec_middle_name_spec) && user_data->middle_name)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->middle_name));
    strncpy (userInfo.middle_name, user_data->middle_name, l);
    userInfo.middle_name[l] = '\0';
    userInfo.middle_name_entered = TRUE;
    userInfo.middle_name_specified = TRUE;
    }
else
    {
    userInfo.middle_name_entered = FALSE;
    userInfo.middle_name_specified = FALSE;
    }

if ((user_data->sec_flags & sec_last_name_spec) && user_data->last_name)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->last_name));
    strncpy (userInfo.last_name, user_data->last_name, l);
    userInfo.last_name[l] = '\0';
    userInfo.last_name_entered = TRUE;
    userInfo.last_name_specified = TRUE;
    }
else
    {
    userInfo.last_name_entered = FALSE;
    userInfo.last_name_specified = FALSE;
    }

db_handle = open_security_db (status, 
			      user_data->dba_user_name,
			      user_data->dba_password,
			      user_data->protocol,
			      user_data->server);
if (db_handle)
    {
    STATUS user_status[20];
    retval = SECURITY_exec_line (status,
    	      		         db_handle,
				 &userInfo,
				 NULL,
				 NULL);
    /* if retval != 0 then there was a gsec error */
    if (retval)
        get_security_error (status, retval);

    isc_detach_database (user_status, &db_handle);
    }
return status[1];  /* security database not opened */
}

int API_ROUTINE isc_blob_load (
    STATUS	*status_vector,
    GDS_QUAD	*blob_id,
    void	**database,
    void	**transaction,
    SCHAR	*file_name,
    SSHORT	*name_length)
{
/**************************************
 *
 *	i s c _ b l o b _ l o a d
 *
 **************************************
 *
 * Functional description
 *	Translate a pascal callable load
 *	into an internal load call.
 *
 **************************************/

if (status_vector)
    status_vector[1] = 0;

return blob__load (blob_id, database, transaction, file_name, name_length);
}

int API_ROUTINE isc_delete_user (
	STATUS		 *status,
	USER_SEC_DATA    *user_data)
{
/**************************************
 *
 *      i s c _ d e l e t e _ u s e r
 *
 **************************************
 *
 * Functional description
 *      Deletes a user from the server's security
 *	    database.
 *      Return 0 if the user was deleted
 *
 *	    Return > 0 if any error occurs.
 *
 **************************************/
USHORT retval = 0, l;
void *db_handle;
struct user_data userInfo;

userInfo.operation = DEL_OPER;

if (user_data->user_name)
    {
    if (strlen (user_data->user_name) > 32)
	{
        status[0] = isc_arg_gds;
        status[1] = isc_usrname_too_long;
        status[2] = isc_arg_end;
        return status[1];
	}   

    for (l = 0; user_data->user_name[l] != ' ' && l < strlen(user_data->user_name); l++)
	userInfo.user_name[l] = UPPER (user_data->user_name[l]);

    userInfo.user_name[l] = '\0';
    userInfo.user_name_entered = TRUE;
    }
else
    {
    status[0] = isc_arg_gds;
    status[1] = isc_usrname_required;
    status[2] = isc_arg_end;
    return status[1];
    }

db_handle = open_security_db (status, 
			      user_data->dba_user_name,
			      user_data->dba_password,
			      user_data->protocol,
			      user_data->server);
if (db_handle)
	{
	STATUS user_status[20];
	retval = SECURITY_exec_line (status,
		      		     db_handle,
				     &userInfo,
				     NULL,
				     NULL);
	/* if retval != 0 then there was a gsec error */
	if (retval)
	    get_security_error (status, retval);

	isc_detach_database (user_status, &db_handle);
	}
return status[1];  /* security database not opened */
}

int API_ROUTINE isc_modify_user (
	STATUS		 *status,
	USER_SEC_DATA    *user_data)
{
/**************************************
 *
 *      i s c _ m o d i f y _ u s e r
 *
 **************************************
 *
 * Functional description
 *      Adds a user to the server's security
 *	database.
 *      Return 0 if the user was added
 *
 *	    Return > 0 if any error occurs.
 *
 **************************************/
USHORT	retval = 0, l;
struct	user_data userInfo;
void	*db_handle;

userInfo.operation = MOD_OPER;

if (user_data->user_name)
    {
    if (strlen (user_data->user_name) > 32)
	{
        status[0] = isc_arg_gds;
        status[1] = isc_usrname_too_long;
        status[2] = isc_arg_end;
        return status[1];
	}   

    for (l = 0; user_data->user_name[l] != ' ' && l < strlen(user_data->user_name); l++)
	userInfo.user_name[l] = UPPER (user_data->user_name[l]);

    userInfo.user_name[l] = '\0';
    userInfo.user_name_entered = TRUE;
    }
else
    {
    status[0] = isc_arg_gds;
    status[1] = isc_usrname_required;
    status[2] = isc_arg_end;
    return status[1];
    }

if (user_data->sec_flags & sec_password_spec)
    {
    if (strlen(user_data->password) > 8)
        {
        status[0] = isc_arg_gds;
        status[1] = isc_password_too_long;
        status[2] = isc_arg_end;
        return status[1];
        }

    for (l = 0; l < strlen(user_data->password) && user_data->password[l] != ' '; l++)
	userInfo.password[l] = user_data->password[l];

    userInfo.password[l] = '\0';
    userInfo.password_entered = TRUE;
    userInfo.password_specified = TRUE;
    }
else
    {
    userInfo.password_specified = FALSE;
    userInfo.password_entered = FALSE;
    }
	

if (user_data->sec_flags & sec_uid_spec)
    {
    userInfo.uid = user_data->uid;
    userInfo.uid_specified = TRUE;
    userInfo.uid_entered = TRUE;
    }
else
    {
    userInfo.uid_specified = FALSE;
    userInfo.uid_entered = FALSE;
    }

if (user_data->sec_flags & sec_gid_spec)
    {
    userInfo.gid = user_data->gid;
    userInfo.gid_specified = TRUE;
    userInfo.gid_entered = TRUE;
    }
else
    {
    userInfo.gid_specified = FALSE;
    userInfo.gid_entered = FALSE;
    }

if (user_data->sec_flags & sec_group_name_spec)
    {
    int l = MIN(ALT_NAME_LEN-1, strlen (user_data->group_name));
    strncpy (userInfo.group_name, user_data->group_name, l);
    userInfo.group_name[l] = '\0';
    userInfo.group_name_entered = TRUE;
    userInfo.group_name_specified = TRUE;
    }
else
    {
    userInfo.group_name_entered = FALSE;
    userInfo.group_name_specified = FALSE;
    }

if (user_data->sec_flags & sec_first_name_spec)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->first_name));
    strncpy (userInfo.first_name, user_data->first_name, l);
    userInfo.first_name[l] = '\0';
    userInfo.first_name_entered = TRUE;
    userInfo.first_name_specified = TRUE;
    }
else
    {
    userInfo.first_name_entered = FALSE;
    userInfo.first_name_specified = FALSE;
    }

if (user_data->sec_flags & sec_middle_name_spec)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->middle_name));
    strncpy (userInfo.middle_name, user_data->middle_name, l);
    userInfo.middle_name[l] = '\0';
    userInfo.middle_name_entered = TRUE;
    userInfo.middle_name_specified = TRUE;
    }
else
    {
    userInfo.middle_name_entered = FALSE;
    userInfo.middle_name_specified = FALSE;
    }

if (user_data->sec_flags & sec_last_name_spec)
    {
    int l = MIN(NAME_LEN-1, strlen (user_data->last_name));
    strncpy (userInfo.last_name, user_data->last_name, l);
    userInfo.last_name[l] = '\0';
    userInfo.last_name_entered = TRUE;
    userInfo.last_name_specified = TRUE;
    }
else
    {
    userInfo.last_name_entered = FALSE;
    userInfo.last_name_specified = FALSE;
    }

db_handle = open_security_db (status, 
			      user_data->dba_user_name,
			      user_data->dba_password,
			      user_data->protocol,
			      user_data->server);
if (db_handle)
    {
    STATUS user_status[20];
    retval = SECURITY_exec_line (status,
	      		         db_handle,
			         &userInfo,
			         NULL,
			         NULL);
    /* if retval != 0 then there was a gsec error */
    if (retval)
        get_security_error (status, retval);
    isc_detach_database (user_status, &db_handle);
    }
return status[1];  /* security database not opened */
}

void* open_security_db (
	STATUS *status,
	TEXT *username,
	TEXT *password,
	int  protocol,
	TEXT *server)
{
/**************************************
 *
 *      o p e n _ s e c u r i t y _ d b
 *
 **************************************
 *
 * Functional description
 *     Opens the security database 
 *
 * Returns the database handle if successful
 * Returns NULL otherwise
 *
 **************************************/
short	dpb_length, l = 0;
char	dpb_buffer [256], *dpb, *p;
TEXT	default_security_db [512], connect_string[1024], *database;
void	*db_handle;
TEXT    sec_server[256];

db_handle = NULL; 

switch (protocol)
	{
	case sec_protocol_tcpip:
	    IS_VALID_SERVER (server);
	    sprintf (sec_server, "%s:", server);
            SECURITY_get_db_path (sec_server, default_security_db);
            sprintf (connect_string, "%s%s", sec_server, default_security_db);
            break;

	case sec_protocol_netbeui:
	    IS_VALID_SERVER (server);
    	    sprintf (sec_server, "\\\\%s\\", server);
            SECURITY_get_db_path (sec_server, default_security_db);
            sprintf (connect_string, "%s%s", sec_server, default_security_db);
	    break;

	case sec_protocol_spx:
	    IS_VALID_SERVER (server);
            sprintf (sec_server, "%s@", server);
            SECURITY_get_db_path (sec_server, default_security_db);
            sprintf (connect_string, "%s%s", sec_server, default_security_db);
	    break;

	case sec_protocol_local:
            SECURITY_get_db_path (NULL, default_security_db);
            sprintf (connect_string, "%s", default_security_db);
	    break;

	default:
            status[0] = isc_arg_gds;
            status[1] = isc_bad_protocol;
            status[2] = isc_arg_end;
            return NULL;
	}

database = connect_string;
dpb = dpb_buffer;
*dpb++ = isc_dpb_version1;

if (username)
    {
    *dpb++ = isc_dpb_user_name;
    *dpb++ = strlen (username);
    for (p = username; *p;)
        *dpb++ = *p++;
    }
	
if (password)
    {
    *dpb++ = isc_dpb_password;
    *dpb++ = strlen (password);
    for (p = password; *p;)
        *dpb++ = *p++;
    }

dpb_length = dpb - dpb_buffer;

if (isc_attach_database (status, 0, database, &db_handle, dpb_length, dpb_buffer))
    db_handle = NULL;

return db_handle;
}

void get_security_error (
	STATUS *status,
	int  gsec_err)
{
/**************************************
 *
 *      g e t _ s e c u r i t y _ e r r o r
 *
 **************************************
 *
 * Functional description
 *
 *    Converts the gsec error code to an isc
 *    error code and adds it to the status vector
 **************************************/

switch (gsec_err)
    {
    case GsecMsg19: 	/* gsec - add record error */ 
        status[0] = isc_arg_gds;
        status[1] = isc_error_adding_sec_record;
        status[2] = isc_arg_end;
        return;

    case GsecMsg20:	/* gsec - modify record error */
        status[0] = isc_arg_gds;
        status[1] = isc_error_modifying_sec_record;
        status[2] = isc_arg_end;
        return;

    case GsecMsg21:	/* gsec - find/modify record error */
        status[0] = isc_arg_gds;
        status[1] = isc_error_modifying_sec_record;
        status[2] = isc_arg_end;
        return;

    case GsecMsg22:	/* gsec - record not found for user: */
        status[0] = isc_arg_gds;
        status[1] = isc_usrname_not_found;
        status[2] = isc_arg_end;
        return;

    case GsecMsg23:	/* gsec - delete record error */
        status[0] = isc_arg_gds;
        status[1] = isc_error_deleting_sec_record;
        status[2] = isc_arg_end;
        return;

    case GsecMsg24:	/* gsec - find/delete record error */
        status[0] = isc_arg_gds;
        status[1] = isc_error_deleting_sec_record;
        status[2] = isc_arg_end;
        return;

    case GsecMsg75:	/* gsec error */
        status[0] = isc_arg_gds;
        status[1] = isc_error_updating_sec_db;
        status[2] = isc_arg_end;
        return;

    default:
        return;
    }
}
