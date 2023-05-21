/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		sqz.c
 *	DESCRIPTION:	Record compression/decompression
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

#include "../jrd/jrd.h"
#include "../jrd/sqz.h"
#include "../jrd/req.h"
#include "../jrd/all_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/sqz_proto.h"
#include "../jrd/thd_proto.h"

USHORT SQZ_apply_differences (
    REC		record,
    SCHAR	*differences,
    SCHAR	*end)
{
/**************************************
 *
 *	S Q Z _ a p p l y _ d i f f e r e n c e s
 *
 **************************************
 *
 * Functional description
 *	Apply a differences (delta) to a record.  Return the length.
 *
 **************************************/
SCHAR	*p;
SCHAR	*p_end;
SSHORT	l;
USHORT	length;

if (end - differences > MAX_DIFFERENCES)
    BUGCHECK (176); /* msg 176 bad difference record */

p = (SCHAR*) record->rec_data;
p_end = (SCHAR *) p + record->rec_length;
while (differences < end && p < p_end)
    {
    l = *differences++;
    if (l > 0)
	{
	if (p + l > p_end)
	    BUGCHECK (177); /* msg 177 applied differences will not fit in record */
	memcpy (p, differences, l);
	p += l;
	differences += l;
	}
    else
	p += -l;
    }

length = (p - (SCHAR*) record->rec_data);
if (length > record->rec_length || differences < end)
    BUGCHECK (177); /* msg 177 applied differences will not fit in record */

return length;
}

USHORT SQZ_compress (
    DCC			dcc,
    register SCHAR	*input,
    register SCHAR	*output,
    register int	space)
{
/**************************************
 *
 *	S Q Z _ c o m p r e s s
 *
 **************************************
 *
 * Functional description
 *	Compress a string into an area of known length.  If it doesn't fit,
 *	return the number of bytes that did.
 *
 **************************************/
register SSHORT	length;
register SCHAR	*control;
SCHAR		*start;

start = input;

while (TRUE)
    {
    control = dcc->dcc_string;
    while (control < dcc->dcc_end)
	if (--space <= 0)
	    {
	    if (space == 0)
		*output = 0;
	    return input - start;
	    }
	else if ((length = *output++ = *control++) & 128)
	    {
	    --space;
	    *output++ = *input;
	    input += (-length) & 255;
	    }
	else
	    {
	    if ((space -= length) < 0)
		{
		length += space;
		output [-1] = length;
		if (length > 0)
		    {
		    MOVE_FAST(input, output, length);
		    input += length;
		    }
		return input - start;
		}
	    if (length > 0)
		{
		MOVE_FAST(input, output, length);
		output += length;
		input += length;
		}
	    }
    if (!(dcc = dcc->dcc_next))
	BUGCHECK (178); /* msg 178 record length inconsistent */
    }
}

USHORT SQZ_compress_length (
    DCC			dcc,
    register SCHAR	*input,
    register int	space)
{
/**************************************
 *
 *	S Q Z _ c o m p r e s s _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Same as SQZ_compress without the output.  If it doesn't fit,
 *	return the number of bytes that did.
 *
 **************************************/
register SSHORT	length;
register SCHAR	*control;
SCHAR		*start;

start = input;

while (TRUE)
    {
    control = dcc->dcc_string;
    while (control < dcc->dcc_end)
	if (--space <= 0)
	    return input - start;
	else if ((length = *control++) & 128)
	    {
	    --space;
	    input += (-length) & 255;
	    }
	else
	    {
	    if ((space -= length) < 0)
		{
		length += space;
		input += length;
		return input - start;
		}
	    input += length;
	    }
    if (!(dcc = dcc->dcc_next))
	BUGCHECK (178); /* msg 178 record length inconsistent */
    }
}


SCHAR *SQZ_decompress (
    register SCHAR	*input,
    USHORT		length,
    register SCHAR	*output,
    SCHAR		*output_end)
{
/**************************************
 *
 *	S Q Z _ d e c o m p r e s s
 *
 **************************************
 *
 * Functional description
 *	Decompress a compressed string into a buffer.  Return the address
 *	where the output stopped.
 *
 **************************************/
register SCHAR	*last, c;
register SSHORT	l;

last = input + length;

while (input < last)
    {
    l = *input++;
    if (l < 0)
	{
	c = *input++;
	
	if ((output -l) > output_end)
		BUGCHECK (179); /* msg 179 decompression overran buffer */
	memset (output, (UCHAR) c, (-1*l) );
	output -= l;
	}
    else
	{
	if ((output + l) > output_end)
		BUGCHECK (179); /* msg 179 decompression overran buffer */
	MOVE_FAST(input,output,l);
	output +=l;
	input +=l;
	}
    }

if (output > output_end)
    BUGCHECK (179); /* msg 179 decompression overran buffer */

return output;
}

USHORT SQZ_differences (
    SCHAR	*rec1,
    USHORT	length1,
    SCHAR	*rec2,
    USHORT	length2,
    SCHAR	*out,
    int		length)
{
/**************************************
 *
 *	S Q Z _ d i f f e r e n c e s
 *
 **************************************
 *
 * Functional description
 *	Compute differences between two records.  The difference
 *	record, when applied to the first record, produces the
 *	second record.  
 *
 *	    difference_record	:= <control_string>...
 *
 *	    control_string	:= <positive_integer> <positive_integer data bytes>
 *				:= <negative_integer>
 *
 *	Return the total length of the differences string.  
 *
 **************************************/
SCHAR	*p, *yellow, *end, *end1, *end2, *start;
SLONG	l;  /* This could be more than 32K since the Old and New records 
	       could be the same for more than 32K characters. 
	       MAX record size is currently 64K. Hence it is defined as a SLONG */

#define STUFF(val)	if (out < end) *out++ = val; else return 32000;
/* WHY IS THIS RETURNING 32000 ??? 
 * It returns a large Positive value to indicate to the caller that we ran out
 * of buffer space in the 'out' argument. Thus we could not create a
 * successful differences record. Now it is upto the caller to check the
 * return value of this function and figure out whether the differences record
 * was created or not. Check prepare_update() (JRD/vio.c) for further
 * information. Of course, the size for a 'differences' record is not expected
 * to go near 32000 in the future. If the case arises where we want to store
 * differences record of 32000 bytes and more, please change the return value
 * above to accomodate a failure value. 
 * 
 * This was investigated as a part of solving bug 10206, bsriram - 25-Feb-1999. 
 */

start = out;
end = out + length;
end1 = rec1 + MIN (length1, length2);
end2 = rec2 + length2;

while (end1 - rec1 > 2)
    {
    if (rec1 [0] != rec2 [0] || rec1 [1] != rec2 [1])
	{
	p = out++;

        /* cast this to LONG to take care of OS/2 pointer arithmetic
           when rec1 is at the end of a segment, to avoid wrapping around */

	yellow = (SCHAR*) MIN ((U_IPTR) end1, ((U_IPTR) rec1 + 127)) - 1;
	while (rec1 <= yellow && 
	       (rec1 [0] != rec2 [0] ||
		(rec1 [1] != rec2 [1] && rec1 < yellow)))
	    {
	    STUFF (*rec2++);
	    ++rec1;
	    }
	*p = out - p - 1;
	continue;
	}
    for (p = rec2; rec1 < end1 && *rec1 == *rec2; rec1++, rec2++)
	;
    l = p - rec2;
    while (l < -127)
	{
	STUFF (-127);
	l += 127;
	}
    if (l)
	STUFF (l);
    }

while (rec2 < end2)
    {
    p = out++;

    /* cast this to LONG to take care of OS/2 pointer arithmetic
       when rec1 is at the end of a segment, to avoid wrapping around */

    yellow = (SCHAR*) MIN ((U_IPTR) end2, ((U_IPTR) rec2 + 127));
    while (rec2 < yellow)
	STUFF (*rec2++);
    *p = out - p - 1;
    }

return out - start;
}

void SQZ_fast (
    DCC			dcc,
    register SCHAR	*input,
    register SCHAR	*output)
{
/**************************************
 *
 *	S Q Z _ f a s t
 *
 **************************************
 *
 * Functional description
 *	Compress a string into an sufficiently large area.  Don't
 *	check nuttin' -- go for speed, man, raw SPEED!
 *
 **************************************/
register SCHAR	*control;
register SSHORT	length;

while (TRUE)
    {
    control = dcc->dcc_string;
    while (control < dcc->dcc_end)
	{
	length = *control++;
	*output++ = length;
	if (length < 0)
	    {
	    *output++ = *input;
	    input -= length;
	    }
	else if (length > 0)
	    {
	    MOVE_FAST(input, output, length);
	    output += length;
	    input += length;
	    }
	}
    if (!(dcc = dcc->dcc_next))
	break;
    }
}

USHORT SQZ_length (
    TDBB		tdbb,
    register SCHAR	*data,
    register int	length,
    DCC			dcc)
{
/**************************************
 *
 *	S Q Z _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Compute the compressed length of a record.  While we're at it, generate
 *	the control string for subsequent compression.
 *
 **************************************/
USHORT		count;
register USHORT	max;
SCHAR		c, *end, *start, *control, *end_control;

SET_TDBB (tdbb);

dcc->dcc_next = NULL;
control = dcc->dcc_string;
end_control = dcc->dcc_string + sizeof (dcc->dcc_string);
end = &data [length];
length = 0;

while (count = end - data)
    {
    start = data;

    /* Find length of non-compressable run */

    if ((max = count - 1) > 1)
	do
	    if (data[0] != data[1] || data[0] != data[2])
		data++;
	    else
		{
		count = data - start;
		break;
		}
	while (--max > 1);
    data = start + count;

    /* Non-compressable runs are limited to 127 bytes */

    while (count)
	{
	max = MIN (count, 127);
	length += 1 + max;
	count -= max;
	*control++ = max;
	if (control == end_control)
	    {
	    dcc->dcc_end = control;
	    if (dcc->dcc_next = tdbb->tdbb_default->plb_dccs)
		{
		dcc = dcc->dcc_next;
		tdbb->tdbb_default->plb_dccs = dcc->dcc_next;
		dcc->dcc_next = NULL;
		assert (dcc->dcc_pool == tdbb->tdbb_default);
		}
	    else
		{
	    	dcc->dcc_next = (DCC) ALLOCD (type_dcc);
	    	dcc = dcc->dcc_next;
		dcc->dcc_pool = tdbb->tdbb_default;
		}
	    control = dcc->dcc_string;
	    end_control = dcc->dcc_string + sizeof (dcc->dcc_string);
	    }
	}

    /* Find compressible run.  Compressable runs are limited to 128 bytes */

    if ((max = MIN (128, end - data)) >= 3)
	{
	start = data;
	c = *data;
	do
	    if (*data != c)
		break;
	    else
		data++;
	while (--max);
	*control++ = start - data;
	length += 2;
	if (control == end_control)
	    {
	    dcc->dcc_end = control;
	    if (dcc->dcc_next = tdbb->tdbb_default->plb_dccs)
		{
		dcc = dcc->dcc_next;
		tdbb->tdbb_default->plb_dccs = dcc->dcc_next;
		dcc->dcc_next = NULL;
		assert (dcc->dcc_pool == tdbb->tdbb_default);
		}
	    else
		{
	    	dcc->dcc_next = (DCC) ALLOCD (type_dcc);
	    	dcc = dcc->dcc_next;
		dcc->dcc_pool = tdbb->tdbb_default;
		}
	    control = dcc->dcc_string;
	    end_control = dcc->dcc_string + sizeof (dcc->dcc_string);
	    }
	}
    }

dcc->dcc_end = control;

return length;
}
