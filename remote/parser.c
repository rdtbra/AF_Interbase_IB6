/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		parse.c
 *	DESCRIPTION:	BLR parser
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
#include "../jrd/ib_stdio.h"
#include "../jrd/gds.h"
#include "../remote/remote.h"
#include "../jrd/align.h"
#include "../jrd/gdsassert.h"
#include "../remote/parse_proto.h"

MSG DLL_EXPORT PARSE_messages (
    UCHAR	*blr,
    USHORT	blr_length)
{
/**************************************
 *
 *	P A R S E _ m e s s a g e s
 *
 **************************************
 *
 * Functional description
 *	Parse the messages of a blr request.  For each message, allocate
 *	a message (msg) and a format (fmt) block.  Return the number of
 *	messages found.  If an error occurs, return -1;
 *
 **************************************/
MSG	message, next;
FMT	format;
DSC	*desc;
USHORT	count, msg_number, offset, align, net_length;
SSHORT  version;

version = *blr++;
if ((version != blr_version4) && (version != blr_version5))
    return (MSG) -1;

if (*blr++ != blr_begin)
    return 0;

message = NULL;
net_length = 0;

while (*blr++ == blr_message)
    {
    msg_number = *blr++;
    count = *blr++;
    count += (*blr++) << 8;
    format = (FMT) ALLOCV (type_fmt, count);
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("PARSE_messages            allocate format  %x\n", format);
#endif
    format->fmt_count = count;
    offset = 0;
    for (desc = format->fmt_desc; count; --count, ++desc)
	{
	align = 4;
	switch (*blr++)
	    {
	    case blr_text:
		desc->dsc_dtype = dtype_text;
		desc->dsc_length = *blr++;
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_text];
		break;

	    case blr_varying:
		desc->dsc_dtype = dtype_varying;
		desc->dsc_length = *blr++ + sizeof (SSHORT);
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_varying];
		break;

	    case blr_cstring:
		desc->dsc_dtype = dtype_cstring;
		desc->dsc_length = *blr++;
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_cstring];
		break;

	    /* Parse the tagged blr types correctly */

	    case blr_text2:
		desc->dsc_dtype = dtype_text;
		desc->dsc_scale = *blr++;
		desc->dsc_scale += (*blr++) << 8;
		desc->dsc_length = *blr++;
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_text];
		break;

	    case blr_varying2:
		desc->dsc_dtype = dtype_varying;
		desc->dsc_scale = *blr++;
		desc->dsc_scale += (*blr++) << 8;
		desc->dsc_length = *blr++ + sizeof (SSHORT);
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_varying];
		break;

	    case blr_cstring2:
		desc->dsc_dtype = dtype_cstring;
		desc->dsc_scale = *blr++;
		desc->dsc_scale += (*blr++) << 8;
		desc->dsc_length = *blr++;
		desc->dsc_length += (*blr++) << 8;
		align = type_alignments [dtype_cstring];
		break;

	    case blr_short:
		desc->dsc_dtype = dtype_short;
		desc->dsc_length = sizeof (SSHORT);
		desc->dsc_scale = *blr++;
		align = type_alignments [dtype_short];
		break;

	    case blr_long:
		desc->dsc_dtype = dtype_long;
		desc->dsc_length = sizeof (SLONG);
		desc->dsc_scale = *blr++;
		align = type_alignments [dtype_long];
		break;

	    case blr_int64:
		desc->dsc_dtype = dtype_int64;
		desc->dsc_length = sizeof (SINT64);
		desc->dsc_scale = *blr++;
		align = type_alignments [dtype_int64];
		break;

	    case blr_quad:
		desc->dsc_dtype = dtype_quad;
		desc->dsc_length = sizeof (SLONG) * 2;
		desc->dsc_scale = *blr++;
		align = type_alignments [dtype_quad];
		break;

	    case blr_float:
		desc->dsc_dtype = dtype_real;
		desc->dsc_length = sizeof (float);
		align = type_alignments [dtype_real];
		break;

	    case blr_double:
#ifndef VMS
	    case blr_d_float:
#endif
		desc->dsc_dtype = dtype_double;
		desc->dsc_length = sizeof (double);
		align = type_alignments [dtype_double];
		break;

#ifdef VMS
	    case blr_d_float:
		desc->dsc_dtype = dtype_d_float;
		desc->dsc_length = sizeof (double);
		align = type_alignments [dtype_d_float];
		break;
#endif

/*          this case cannot occur as switch paramater is char and blr_blob
            is 261. blob_ids are actually passed around as blr_quad.

	    case blr_blob:
		desc->dsc_dtype = dtype_blob;
		desc->dsc_length = sizeof (SLONG) * 2;
		align = type_alignments [dtype_blob];
		break;
*/
	    
	    case blr_timestamp:
		desc->dsc_dtype = dtype_timestamp;
		desc->dsc_length = sizeof (SLONG) * 2;
		align = type_alignments [dtype_timestamp];
		break;
	    
	    case blr_sql_date:
		desc->dsc_dtype = dtype_sql_date;
		desc->dsc_length = sizeof (SLONG);
		align = type_alignments [dtype_sql_date];
		break;
	    
	    case blr_sql_time:
		desc->dsc_dtype = dtype_sql_time;
		desc->dsc_length = sizeof (ULONG);
		align = type_alignments [dtype_sql_time];
		break;
	    
	    default:
		assert (FALSE);
		ALLR_release (format);
		while (next = message)
		    {
		    message = message->msg_next;
		    ALLR_release (next->msg_address);
		    ALLR_release (next);
		    }
		return (MSG) -1;
	    }
	if (desc->dsc_dtype == dtype_varying)
	    net_length += 4 + ((desc->dsc_length - 2 + 3) & ~3);
	else
	    net_length += (desc->dsc_length + 3) & ~3;
	if (align > 1)
	    offset = ALIGN (offset, align);
	desc->dsc_address = (UCHAR*) offset;
	offset += desc->dsc_length;
	}
    format->fmt_length = offset;
    format->fmt_net_length = net_length;
    next = (MSG) ALLOCV (type_msg, format->fmt_length);
#ifdef REMOTE_DEBUG_MEMORY
    ib_printf ("PARSE_messages            allocate message %x\n", next);
#endif
    next->msg_next = message;
    message = next;
    message->msg_address = (UCHAR*) format;
    message->msg_number = msg_number;
    }

return message;
}

UCHAR *PARSE_prepare_messages (
    UCHAR	*blr,
    USHORT	blr_length)
{
/**************************************
 *
 *	P A R S E _ p r e p a r e _ m e s s a g e s
 *
 **************************************
 *
 * Functional description
 *	Parse the messages of a blr request and convert
 *	each occurrence of blr_d_float to blr_double.
 *
 *	This function is only called for protocol version 5 and below
 *
 **************************************/
UCHAR	*new_blr, *old_blr;
USHORT	count;
SSHORT	version;

new_blr = old_blr = blr;

version =  *blr++;
if (((version != blr_version4) && (version != blr_version5)) ||
    *blr++ != blr_begin)
    return old_blr;

while (*blr++ == blr_message)
    {
    blr++;
    count = *blr++;
    count += (*blr++) << 8;
    for (; count; --count)
	switch (*blr++)
	    {
	    case blr_text2:
	    case blr_varying2:
	    case blr_cstring2:
		blr += 4;	/* SUBTYPE word & LENGTH word */
		break;
	    case blr_text:
	    case blr_varying:
	    case blr_cstring:
		blr += 2;	/* LENGTH word */
		break;
	    case blr_short:
	    case blr_long:
	    case blr_int64:
	    case blr_quad:
		blr++;		/* SCALE byte */
		break;
	    case blr_float:
	    case blr_double:
	    case blr_timestamp:
	    case blr_sql_date:
	    case blr_sql_time:
		break;

	    case blr_d_float:
		if (new_blr == old_blr)
		    {
		    new_blr = (UCHAR*) ALLR_alloc ((SLONG) blr_length);
		    /* NOMEM: ALLR_alloc() handled */
		    /* FREE:  Never freed, blr_d_float is VMS specific */
#ifdef REMOTE_DEBUG_MEMORY
                    ib_printf ("PARSE_prepare_messages    allocate blr     %x\n", new_blr);
#endif
		    memcpy (new_blr, old_blr, blr_length);
		    blr = new_blr + (int) (blr - old_blr);
		    }

		blr [-1] = blr_double;
		break;

	    default:
		DEV_REPORT ("Unexpected BLR in PARSE_prepare_messages()");
		/* This old code would return, so we will also */
		return new_blr;
	    }
    }

return new_blr;
}
