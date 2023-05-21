/*
 *      PROGRAM:        JRD Remote Interface/Server
 *      MODULE:         allr.c
 *      DESCRIPTION:    Internal block allocator
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

#include <stdlib.h>
#include <string.h>
#include "../jrd/ibsetjmp.h"
#include "../remote/remote.h"
#include "../jrd/codes.h"
#include "../remote/allr_proto.h"
#include "../remote/remot_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/thd_proto.h"

#define BLKDEF(type, root, tail)	sizeof (struct root), tail,

static CONST struct {
    SSHORT       typ_root_length;
    SSHORT       typ_tail_length;
} REM_block_sizes[] = {
    0,0,
#include "../remote/blk.h"
    0
};

#undef BLKDEF

#ifdef SUPERSERVER
SLONG allr_delta_alloc=0;
#endif

UCHAR * DLL_EXPORT ALLR_alloc (
    ULONG        size)
{
/**************************************
 *
 *      A L L R _ a l l o c
 *
 **************************************
 *
 * Functional description
 *      Allocate a block.
 *
 **************************************/
UCHAR	*block;
TRDB	trdb;
STATUS	*status_vector;

if (block = gds__alloc ((SLONG)size))
#ifdef SUPERSERVER
{
    allr_delta_alloc += size;
    return block;
}
#else
    return block;
#endif
/* FREE: caller must free - usually using ALLR_release */
/* NOMEM: post a user level error, if we have a status vector,
 *        otherwise just an error return */

trdb = GET_THREAD_DATA;
if (status_vector = trdb->trdb_status_vector)
    {
    *status_vector++ = gds_arg_gds;
    *status_vector++ = gds__virmemexh;
    *status_vector = gds_arg_end;
    }

LONGJMP (*trdb->trdb_setjmp, gds__virmemexh);
return ((UCHAR *) NULL); 	/* Added to remove compiler error */
}

BLK DLL_EXPORT ALLR_block (
    UCHAR	type,
    ULONG		count)
{
/**************************************
 *
 *      A L L R _ b l o c k
 *
 **************************************
 *
 * Functional description
 *      Allocate a block from a given pool and initialize the block.
 *      This is the primary block allocation routine.
 *
 **************************************/
register BLK     block;
register USHORT  size;
USHORT           tail;
USHORT		 ucount;

if (type <= (UCHAR) type_MIN || type >= (UCHAR) type_MAX)
    {
    TRDB	trdb;
    STATUS	*status_vector;
    TEXT	errmsg [128];

    trdb = GET_THREAD_DATA;
    if (status_vector = trdb->trdb_status_vector)
	{
	status_vector [0] = gds_arg_gds;
	status_vector [1] = gds__bug_check;
	status_vector [2] = gds_arg_string;
	status_vector [4] = gds_arg_end;
	if (gds__msg_lookup (NULL_PTR, JRD_BUGCHK, 150, sizeof (errmsg), errmsg, NULL) < 1)
	    status_vector [3] = (STATUS) "request to allocate invalid block type";
	else
	    {
	    status_vector [3] = (STATUS) errmsg;
	    REMOTE_save_status_strings (trdb->trdb_status_vector);
	    }
	}

    LONGJMP (*trdb->trdb_setjmp, gds__bug_check);
    }

/* Compute block length, recasting count to make sure the calculation
   comes out right on 2 byte platforms (like the PC) */

size = REM_block_sizes [type].typ_root_length;
ucount = (USHORT)count;
if ((tail = REM_block_sizes [type].typ_tail_length) &&
    ucount >= 1)
    size += (ucount - 1) * tail;

block = (BLK) ALLR_alloc ((ULONG) size);
/* NOMEM: callee handled */
/* FREE:  caller must handle - use ALLR_release */

block->blk_type = type;
block->blk_length = size;

if (size -= sizeof (struct blk))
    memset ((SCHAR*) block + sizeof (struct blk), 0, size);

return block;
}

BLK ALLR_clone (
    BLK         block)
{
/**************************************
 *
 *      A L L R _ c l o n e
 *
 **************************************
 *
 * Functional description
 *      Clone a block.
 *
 *	Caller responsible for free'ing the clone
 *
 **************************************/
BLK     clone;
UCHAR    *p, *q;
USHORT   l;

l = block->blk_length;
clone = (BLK) ALLR_alloc ((ULONG) l);
/* NOMEM: ALLR_alloc() handled */
/* FREE:  caller must handle  - use ALLR_release */

p = (UCHAR*) clone;
q = (UCHAR*) block;
do *p++ = *q++; while (--l);

return clone;
}

void ALLR_free (
    void        *block)
{
/**************************************
 *
 *      A L L R _ f r e e
 *
 **************************************
 *
 * Functional description
 *      Free a block.
 *
 **************************************/

#ifdef SUPERSERVER
allr_delta_alloc -= gds__free (block);
#else
gds__free (block);
#endif
}

void DLL_EXPORT ALLR_release (
    void	*block)
{
/**************************************
 *
 *      A L L R _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *      Release a structured block.
 *
 **************************************/

ALLR_free (block);
}

VEC ALLR_vector (
    VEC		*ptr,
    ULONG	count)
{
/**************************************
 *
 *      A L L R _ v e c t o r
 *
 **************************************
 *
 * Functional description
 *      Allocate a vector.
 *
 **************************************/
VEC	vector, new_vector;
BLK	*p, *q, *end;

++count;

if (!(vector = *ptr))
    {
    vector = *ptr =  (VEC) ALLR_block (type_vec, count);
    vector->vec_count = count;
    return vector;
    }

/* If it fits, do it */

if (count <= vector->vec_count)
    return vector;

new_vector = *ptr = (VEC) ALLR_block (type_vec, count);
new_vector->vec_count = count;

p = new_vector->vec_object;
q = vector->vec_object;
end = q + (int) vector->vec_count;
while (q < end)
    *p++ = *q++;
ALLR_release (vector);

return new_vector;
}
