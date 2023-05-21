/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		stats.e
 *	DESCRIPTION:	Record statistics manager
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

#include "../jrd/gds.h"

database db = "yachts.lnk";

extern SCHAR	*gds__alloc();
extern SLONG	gds__vax_integer();

#define ITEM_seq_reads	0
#define ITEM_idx_reads	1
#define ITEM_inserts	2
#define ITEM_updates	3
#define ITEM_deletes	4
#define ITEM_backouts	5
#define ITEM_purges	6
#define ITEM_expunges	7
#define ITEM_count	8

typedef struct stats {
    SSHORT	stats_count;
    SSHORT	stats_items;		/* Number of item per relation */
    SLONG	stats_counts [1];
} *STATS;

static STATS	expand_stats();
static		print_line();

static SCHAR	info_request [] = {
	gds__info_read_seq_count,
	gds__info_read_idx_count,
	gds__info_insert_count,
	gds__info_update_count,
	gds__info_delete_count,
	gds__info_backout_count,
	gds__info_purge_count,
	gds__info_expunge_count,
	gds__info_end};
	
static SCHAR	*headers[] = {
	"S-Reads",
	"I-Reads",
	"Inserts",
	"Updates",
	"Deletes",
	"Backouts",
	"Purges",
	"Expunges"
	};

static int	*database_handle;
static int	*request_handle;

stats_analyze (before, after, callback, arg)
    STATS	before, after;
    int		(*callback)();
    SCHAR	*arg;
{
/**************************************
 *
 *	s t a t s _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SLONG	delta [ITEM_count], *p, *end, *tail, *tail2, total;
SSHORT	relation_id;

if (!after)
    return;

end = delta + ITEM_count;

if (before)
    tail2 = before->stats_counts;

for (tail = after->stats_counts, relation_id = 0; 
     relation_id < after->stats_count; ++relation_id)
    {
    total = 0;
    for (p = delta ; p < end;)
	{
	total += *tail;
	*p++ = *tail++;
	}
    if (before && relation_id < before->stats_count)
	for (p = delta; p < end;)
	    {
	    total -= *tail2;
	    *p++ -= *tail2++;
	    }
    if (total)
	(*callback) (arg, relation_id, ITEM_count, headers, delta);
    }
}

stats_fetch (status_vector, db_handle, stats_ptr)
    SLONG	*status_vector;
    int		**db_handle;
    STATS	*stats_ptr;
{
/**************************************
 *
 *	s t a t s _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Gather statistics.
 *
 **************************************/
SCHAR	info_buffer [4096], *p, *end;
STATS	stats;
SSHORT	length, item;

if (gds__database_info (
	GDS_VAL (status_vector),
	GDS_VAL (db_handle),
	sizeof (info_request),
	info_request,
	sizeof (info_buffer),
	info_buffer))
    return status_vector [1];

if (stats = *stats_ptr)
    zap_longs (stats->stats_counts, stats->stats_count * ITEM_count);
else
    stats = expand_stats (stats_ptr, 64);

if (!stats)
    {
    status_vector [0] = gds_arg_gds;
    status_vector [1] = gds__virmemexh;
    status_vector [2] = gds_arg_end;
    return status_vector [1];
    }

for (p = info_buffer; p < info_buffer + sizeof (info_buffer) && *p != gds__info_end;)
    {
    length = gds__vax_integer (p + 1, 2);
    item = -1;
    switch (*p)
	{
	case gds__info_read_seq_count:  item = ITEM_seq_reads; break;
	case gds__info_read_idx_count:  item = ITEM_idx_reads; break;
	case gds__info_insert_count:  item = ITEM_inserts; break;
	case gds__info_update_count:  item = ITEM_updates; break;
	case gds__info_delete_count:  item = ITEM_deletes; break;
	case gds__info_backout_count:  item = ITEM_backouts; break;
	case gds__info_purge_count:  item = ITEM_purges; break;
	case gds__info_expunge_count:  item = ITEM_expunges; break;
	}
    if (item >= 0)
	{
	if (get_counts (status_vector, p + 3, length, stats_ptr, item))
	    break;
    p += 3 + length;
    }


return status_vector [1];
}

stats_print (db_handle, tr_handle, before, after)
    int		**db_handle;
    int		**tr_handle;
    STATS	before, after;
{
/**************************************
 *
 *	s t a t s _ p r i n t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
SLONG	init;

if (request_handle && database_handle != *db_handle)
    gds__release_request (gds__status, GDS_REF (request_handle));

db = database_handle = *db_handle;
gds__trans = *tr_handle;
init = 0;
stats_analyze (before, after, print_line, &init);
}

static STATS expand_stats (ptr, count)
    STATS	*ptr;
    SSHORT	count;
{
/**************************************
 *
 *	e x p a n d _ s t a t s
 *
 **************************************
 *
 * Functional description
 *	Make sure vector is big enough.
 *
 **************************************/
STATS	stats, old;
SLONG	*p, *q, *end, length;

/* If the thing is already big enough, don't do nothing */

if ((old = *ptr) && old->stats_count < count)
    return old;

count += 20;
length = sizeof (struct stats) + (ITEM_count * count - 1) * sizeof (SLONG);
stats = (STATS) gds__alloc (length);
/* FREE: apparently never freed */
if (!stats)			/* NOMEM: out of memory */
    return NULL;		/* leave *ptr unchanged */
zap_longs (stats->stats_counts, count * ITEM_count);
stats->stats_count = count;
stats->stats_items = ITEM_count;

if (old)
    {
    p = stats->stats_counts;
    q = old->stats_counts;
    end = q + ITEM_count * old->stats_count;
    while (q < end)
	*p++ = *q++;
    gds__free (old);
    }

return *ptr = stats;
}

static get_counts (status_vector, info, length, stats_ptr, item)
    STATUS	*status_vector;
    SCHAR	*info;
    SSHORT	length;
    STATS	*stats_ptr;
    SSHORT	item;
{
/**************************************
 *
 *	g e t _ c o u n t s
 *
 **************************************
 *
 * Functional description
 *	Pick up operation counts from information items.
 *
 **************************************/
SCHAR	*p, *end;
SSHORT	relation_id;
STATS	stats;

stats = *stats_ptr;

for (p = info, end = p + length; p < end; p += 6)
    {
    relation_id = gds__vax_integer (p, 2);
    if (relation_id >= stats->stats_count)
	if (!expand_stats (&stats, relation_id))
	    {
	    status_vector [0] = gds_arg_gds;
	    status_vector [1] = gds__virmemexh;
	    status_vector [2] = gds_arg_end;
	    return status_vector [1];
	    }
    stats->stats_counts [relation_id * ITEM_count + item] = gds__vax_integer (p + 2, 4);
    }
return status_vector [1];
}

static print_line (arg, relation_id, count, headers, counts)
    SCHAR	*arg;
    SSHORT	relation_id;
    SSHORT	count;
    SCHAR	**headers;
    SLONG	*counts;
{
/**************************************
 *
 *	p r i n t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Display data.
 *
 **************************************/
SLONG	*end;
SSHORT	n;

if (!*arg)
    {
    *arg = 1;
    ib_printf ("%32s ", " ");
    if (n = count)
	do ib_printf ("%10s", *headers++); while (--n);
    ib_printf ("\n");
    }

for (request_handle request_handle) x in rdb$relations with x.rdb$relation_id eq relation_id
    ib_printf ("%32s", x.rdb$relation_name);
    for (end = counts + count; counts < end; counts++)
	ib_printf ("%10d", *counts);
    ib_printf ("\n");
end_for;

}

static zap_longs (ptr, count)
    SLONG	*ptr;
    SSHORT	count;
{
/**************************************
 *
 *	z a p _ l o n g s
 *
 **************************************
 *
 * Functional description
 *	Zero a bunch of SLONGwords.
 *
 **************************************/

if (count)
    do *ptr++ = 0; while (--count);
}
