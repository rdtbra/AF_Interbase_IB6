/*
 *	PROGRAM:	Alice (All Else) Utility
 *	MODULE:		tdr.c
 *	DESCRIPTION:	Routines for automated transaction recovery
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
#include "../jrd/gds.h"
#include "../jrd/common.h"
#include "../alice/alice.h"
#include "../alice/aliceswi.h"
#include "../alice/all.h"
#include "../alice/alloc.h"
#include "../alice/info.h"
#include "../alice/alice_proto.h"
#include "../alice/all_proto.h"
#include "../alice/met_proto.h"
#include "../alice/tdr_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/svc_proto.h"

static ULONG	ask (void);
static void	print_description (TDR);
static void	reattach_database (TDR);
static void	reattach_databases (TDR);
static BOOLEAN	reconnect (int *, SLONG, TEXT *, ULONG);


#ifdef GUI_TOOLS
#define NEWLINE	"\r\n"
#else
#define NEWLINE	"\n"
#endif

static UCHAR
    limbo_info [] =	{gds__info_limbo, gds__info_end};




/* 
** The following routines are shared by the command line gfix and
** the windows server manager.  These routines should not contain
** any direct screen I/O (i.e. ib_printf/ib_getc statements).
*/
USHORT TDR_analyze (
    TDR		trans)
{
/**************************************
 *
 *	T D R _ a n a l y z e 
 *
 **************************************
 *
 * Functional description
 *	Determine the proper action to take
 *	based on the state of the various
 *	transactions.
 *
 **************************************/
USHORT	state, advice = TRA_none;
    
if (trans == NULL)
    return TRA_none;
    
/* if the tdr for the first transaction is missing,
   we can assume it was committed */

state = trans->tdr_state;
if (state == TRA_none)
    state = TRA_commit;
else if (state == TRA_unknown)
    advice = TRA_unknown;

for (trans = trans->tdr_next; trans; trans = trans->tdr_next)
    {
    switch (trans->tdr_state)
	{
	/* an explicitly committed transaction necessitates a check for the
	   perverse case of a rollback, otherwise a commit if at all possible */

        case TRA_commit:
            if (state == TRA_rollback)
                {
  	        ALICE_print (105, 0, 0, 0, 0, 0); /* msg 105: Warning: Multidatabase transaction is in inconsistent state for recovery. */
		ALICE_print (106, trans->tdr_id, 0, 0, 0, 0); /* msg 106: Transaction %ld was committed, but prior ones were rolled back. */
	        return 0;
	        }
	    else
	        advice = TRA_commit;
	    break;

	/* a prepared transaction requires a commit if there are missing
	   records up to now, otherwise only do something if somebody else
	   already has */

        case TRA_limbo:
  	    if (state == TRA_none)
	        advice = TRA_commit;
	    else if (state == TRA_commit)
	        advice = TRA_commit;
	    else if (state == TRA_rollback)
	        advice = TRA_rollback;
	    break;

	/* an explicitly rolled back transaction requires a rollback unless a
           transaction has committed or is assumed committed */

        case TRA_rollback:
	    if ((state == TRA_commit) ||
	        (state == TRA_none))
  	        {
  	        ALICE_print (105, 0, 0, 0, 0, 0); /* msg 105: Warning: Multidatabase transaction is in inconsistent state for recovery. */
		ALICE_print (107, trans->tdr_id, 0, 0, 0, 0); /* msg 107: Transaction %ld was rolled back, but prior ones were committed. */

	        return 0;
	        }
	    else 
	        advice = TRA_rollback;
	    break;

	/* a missing TDR indicates a committed transaction if a limbo one hasn't
	   been found yet, otherwise it implies that the transaction wasn't
	   prepared */

        case TRA_none:
	    if (state == TRA_commit)
		advice = TRA_commit;
	    else if (state == TRA_limbo)
		advice = TRA_rollback;
	    break;

	/* specifically advise TRA_unknown to prevent assumption that all are
	   in limbo */

        case TRA_unknown:		
	    if (!advice)
		advice = TRA_unknown;
	    break;

	default:
	    ALICE_print (67, trans->tdr_state,0,0,0,0); /* msg 67: Transaction state %d not in valid range. */
	    return 0;
	}
    }
    
return advice;
}


BOOLEAN TDR_attach_database (
    STATUS	*status_vector,
    TDR		trans,
    TEXT	*pathname)
{
/**************************************
 *
 *	T D R _ a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Attempt to attach a database with a given pathname.
 *
 **************************************/
UCHAR	dpb [128], *d, *q;
USHORT	dpb_length;
TGBL    tdgbl;

tdgbl = GET_THREAD_DATA;

#ifndef GUI_TOOLS
if (tdgbl->ALICE_data.ua_debug)
    ALICE_print (68, pathname,0,0,0,0); /* msg 68: ATTACH_DATABASE: attempted attach of %s */
#endif
                   
d = dpb;             

*d++ = gds__dpb_version1;
*d++ = gds__dpb_no_garbage_collect;
*d++ = 0;
*d++ = isc_dpb_gfix_attach;
*d++ = 0;

if (tdgbl->ALICE_data.ua_user)
    {
    *d++ = gds__dpb_user_name;
    *d++ = strlen (tdgbl->ALICE_data.ua_user);
    for (q = tdgbl->ALICE_data.ua_user; *q;)
	*d++ = *q++;
    }

if (tdgbl->ALICE_data.ua_password)
    {
    if (!tdgbl->sw_service_thd)
	*d++ = gds__dpb_password;
    else
	*d++ = gds__dpb_password_enc;
    *d++ = strlen (tdgbl->ALICE_data.ua_password);
    for (q = tdgbl->ALICE_data.ua_password; *q;)
	*d++ = *q++;
    }

dpb_length = d - dpb;
if (dpb_length == 1)
    dpb_length = 0;

trans->tdr_db_handle = NULL;

gds__attach_database (
    status_vector,
    0,
    GDS_VAL (pathname),
    GDS_REF (trans->tdr_db_handle),
    dpb_length,
    GDS_VAL (dpb));

if (status_vector [1])
    {
#ifndef GUI_TOOLS
    if (tdgbl->ALICE_data.ua_debug)
	{
        ALICE_print (69,0,0,0,0,0); /* msg 69:  failed */
	ALICE_print_status (status_vector);
	}
#endif
    return FALSE;
    }

MET_set_capabilities (status_vector, trans);

#ifndef GUI_TOOLS
if (tdgbl->ALICE_data.ua_debug)
    ALICE_print (70,0,0,0,0,0); /* msg 70:  succeeded */
#endif

return TRUE;
}


void TDR_get_states (
    TDR		trans)
{
/**************************************
 *
 *	T D R _ g e t _ s t a t e s
 *
 **************************************
 *
 * Functional description
 *	Get the state of the various transactions
 *	in a multidatabase transaction.
 *
 **************************************/
STATUS	status_vector [20];
TDR	ptr;

for (ptr = trans; ptr; ptr = ptr->tdr_next)
    MET_get_state (status_vector, ptr);
}


void TDR_shutdown_databases (
    TDR		trans)
{
/**************************************
 *
 *	T D R _ s h u t d o w n _ d a t a b a s e s
 *
 **************************************
 *
 * Functional description
 *	Detach all databases associated with
 *	a multidatabase transaction.
 *
 **************************************/
TDR	ptr;
STATUS	status_vector [20];

for (ptr = trans; ptr; ptr = ptr->tdr_next)
    gds__detach_database (status_vector, GDS_REF (ptr->tdr_db_handle));
}


#ifndef GUI_TOOLS
/*
** The following routines are only for the command line utility. 
** This should really be split into two files...
*/

void TDR_list_limbo (
    int		*handle,
    TEXT    	*name,
    ULONG	switches)
{
/**************************************
 *
 *	T D R _ l i s t _ l i m b o
 *
 **************************************
 *
 * Functional description
 *	List transaction stuck in limbo.  If the prompt switch is set,
 *	prompt for commit, rollback, or leave well enough alone.
 *
 **************************************/
UCHAR	buffer [1024], *ptr;
STATUS	status_vector [20];
SLONG	id;
USHORT	item, flag, length;
TDR	trans;
TGBL	tdgbl;

tdgbl = GET_THREAD_DATA;

if (gds__database_info (status_vector,
	GDS_REF (handle),
	sizeof (limbo_info),
	limbo_info,
	sizeof (buffer),
	buffer))
    {
    ALICE_print_status (status_vector);
    return;
    }

ptr = buffer;
flag = TRUE;

while (flag)
    {
    item = *ptr++;
    length = gds__vax_integer (ptr, 2);
    ptr += 2;
    switch (item)
	{
	case gds__info_limbo:
	    id = gds__vax_integer (ptr, length);
	    if (switches & (sw_commit | sw_rollback | sw_two_phase | sw_prompt))
		{
		TDR_reconnect_multiple (handle, id, name, switches);
		ptr += length;
		break;
		}
	    if (!tdgbl->sw_service_thd)
		ALICE_print (71, id, 0, 0, 0, 0); /* msg 71: Transaction %d is in limbo. */
  	    if (trans = MET_get_transaction (status_vector, handle, id))
		{
#ifdef SUPERSERVER
		SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_multi_tra_id);
		SVC_putc (tdgbl->service_blk, (UCHAR) id);
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 8));
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 16));
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 24));
#endif
		reattach_databases (trans);
		TDR_get_states (trans);
	    	TDR_shutdown_databases (trans);
		print_description (trans);
		}
#ifdef SUPERSERVER
	    else
		{
		SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_single_tra_id);
		SVC_putc (tdgbl->service_blk, (UCHAR) id);
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 8));
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 16));
		SVC_putc (tdgbl->service_blk, (UCHAR) (id >> 24));
		}
#endif
	    ptr += length;
	    break;
	
	case gds__info_truncated:
	    if (!tdgbl->sw_service_thd)
		ALICE_print (72, 0, 0, 0, 0, 0); /* msg 72: More limbo transactions than fit.  Try again */

	case gds__info_end:
	    flag = FALSE;
	    break;
	
	default:
	    if (!tdgbl->sw_service_thd)
		ALICE_print (73, item, 0, 0, 0, 0); /* msg 73: Unrecognized info item %d */
	}
    }
}


BOOLEAN TDR_reconnect_multiple (
    int		*handle,
    SLONG	id,
    TEXT        *name,
    ULONG	switches)
{
/**************************************
 *
 *	T D R _ r e c o n n e c t _ m u l t i p l e
 *
 **************************************
 *
 * Functional description
 *	Check a transaction's TDR to see if it is
 *	a multi-database transaction.  If so, commit
 *	or rollback according to the user's wishes.
 *	Object strongly if the transaction is in a
 *	state that would seem to preclude committing 
 *	or rolling back, but essentially do what the 
 *	user wants.  Intelligence is assumed for the 
 *	gfix user.
 *
 **************************************/
TDR	trans, ptr;
STATUS	status_vector [20];
USHORT	advice, error;
UCHAR	description [1024];
UCHAR	warning [256];

error = FALSE;

/* get the state of all the associated transactions */
        
if (!(trans = MET_get_transaction (status_vector, handle, id)))
    return reconnect (handle, id, name, switches);

reattach_databases (trans);
TDR_get_states (trans);

/* analyze what to do with them; if the advice contradicts the user's
   desire, make them confirm it; otherwise go with the flow. */

advice = TDR_analyze (trans);

if (!advice)
    {
    print_description (trans);
    switches = ask();
    }
else
    {
    switch (advice)
        {
        case TRA_rollback:
            if (switches & sw_commit)
            	{
                ALICE_print (74, trans->tdr_id, 0, 0, 0, 0); /* msg 74: "A commit of transaction %ld will violate two-phase commit. */
                print_description (trans);
	        switches = ask();
	        }
            else if (switches & sw_rollback) 
  	    	switches |= sw_rollback;
            else if (switches & sw_two_phase) 
  	    	switches |= sw_rollback;
            else if (switches & sw_prompt)
            	{
            	ALICE_print (75, trans->tdr_id, 0, 0, 0, 0); /* msg 75: A rollback of transaction %ld is needed to preserve two-phase commit. */
    	    	print_description (trans);
            	switches = ask();
            	}
	    break;

        case TRA_commit:
            if (switches & sw_rollback)
            	{
            	ALICE_print (76, trans->tdr_id, 0, 0, 0, 0); /* msg 76: Transaction %ld has already been partially committed. */
            	ALICE_print (77, 0, 0, 0, 0, 0); /* msg 77: A rollback of this transaction will violate two-phase commit. */
   	    	print_description (trans);
            	switches = ask();
            	}
            else if (switches & sw_commit)
	    	switches |= sw_commit;
            else if (switches & sw_two_phase)
            	switches |= sw_commit;
            else if (switches & sw_prompt)
            	{
            	ALICE_print (78, trans->tdr_id, 0, 0, 0, 0); /* msg 78: Transaction %ld has been partially committed. */
            	ALICE_print (79, 0, 0, 0, 0, 0); /* msg 79: A commit is necessary to preserve the two-phase commit. */
            	print_description (trans);
            	switches = ask();
            	} 
	    break;

        case TRA_unknown:
            ALICE_print (80, 0, 0, 0, 0, 0); /* msg 80: Insufficient information is available to determine */
	    ALICE_print (81, trans->tdr_id, 0, 0, 0, 0); /* msg 81: a proper action for transaction %ld. */
	    print_description (trans);
	    switches = ask();
	    break;

        default:
            if (!(switches & (sw_commit | sw_rollback)))
            	{
  	    	ALICE_print (82, trans->tdr_id, 0, 0, 0, 0); /* msg 82: Transaction %ld: All subtransactions have been prepared. */
	    	ALICE_print (83, 0, 0, 0, 0, 0); /* msg 83: Either commit or rollback is possible. */
	    	print_description (trans);
	    	switches = ask();
	    	}   
        }
    }

if (switches != (ULONG) -1)
    {
    /* now do the required operation with all the subtransactions */

    if (switches & (sw_commit | sw_rollback))
	for (ptr = trans; ptr; ptr = ptr->tdr_next)
	    if (ptr->tdr_state == TRA_limbo)
                reconnect (ptr->tdr_db_handle, ptr->tdr_id, ptr->tdr_filename, switches);
    }
else
    {
    ALICE_print (84, 0, 0, 0, 0, 0); /* msg 84: unexpected end of input */
    error = TRUE;
    }

/* shutdown all the databases for cleanliness' sake */

TDR_shutdown_databases (trans);

return error;
}


static void print_description (
    TDR		trans)
{
/**************************************
 *
 *	p r i n t _ d e s c r i p t i o n
 *
 **************************************
 *
 * Functional description
 *	format and print description of a transaction in
 *	limbo, including all associated transactions
 *	in other databases.
 *
 **************************************/
TDR	ptr;
BOOLEAN	prepared_seen;
TGBL	tdgbl;
int	i;

tdgbl = GET_THREAD_DATA;

if (!trans)
    return;

if (!tdgbl->sw_service_thd)
    ALICE_print (92, 0, 0, 0, 0, 0); /* msg 92:   Multidatabase transaction: */
                      
prepared_seen = FALSE;
for (ptr = trans; ptr; ptr = ptr->tdr_next)
    {
    if (ptr->tdr_host_site)
	{
#ifndef SUPERSERVER
	ALICE_print (93, ptr->tdr_host_site->str_data, 0, 0, 0, 0); /* msg 93: Host Site: %s */
#else
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_host_site);
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_host_site->str_data));
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_host_site->str_data) >> 8);
	for (i = 0; i < strlen (ptr->tdr_host_site->str_data); i++);
	    SVC_putc (tdgbl->service_blk, (UCHAR) ptr->tdr_host_site->str_data[i]);
#endif
	}

    if (ptr->tdr_id)
#ifndef SUPERSERVER
	ALICE_print (94, ptr->tdr_id, 0, 0, 0, 0); /* msg 94: Transaction %ld */
#else
	{
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_id);
	SVC_putc (tdgbl->service_blk, (UCHAR) ptr->tdr_id);
	SVC_putc (tdgbl->service_blk, (UCHAR) (ptr->tdr_id >> 8));
	SVC_putc (tdgbl->service_blk, (UCHAR) (ptr->tdr_id >> 16));
	SVC_putc (tdgbl->service_blk, (UCHAR) (ptr->tdr_id >> 24));
	}
#endif

    switch (ptr->tdr_state)
	{
	case TRA_limbo:
#ifndef SUPERSERVER
	    ALICE_print (95, 0, 0, 0, 0, 0); /* msg 95: has been prepared. */
#else
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state);
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state_limbo);
#endif
	    prepared_seen = TRUE;
	    break;

	case TRA_commit:
#ifndef SUPERSERVER
	    ALICE_print (96, 0, 0, 0, 0, 0); /* msg 96: has been committed. */
#else
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state);
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state_commit);
#endif
	    break;

	case TRA_rollback:
#ifndef SUPERSERVER
	    ALICE_print (97, 0, 0, 0, 0, 0); /* msg 97: has been rolled back. */
#else
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state);
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state_rollback);
#endif
	    break;

	case TRA_unknown:
#ifndef SUPERSERVER
	    ALICE_print (98, 0, 0, 0, 0, 0); /* msg 98: is not available. */
#else
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state);
	    SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_state_unknown);
#endif
	    break;

	default:                 
#ifndef SUPERSERVER
	    if (prepared_seen)
		ALICE_print (99, 0, 0, 0, 0, 0); /* msg 99: is not found, assumed not prepared. */
	    else
		ALICE_print (100, 0, 0, 0, 0, 0); /* msg 100: is not found, assumed to be committed. */
#endif
	    break;
	}

    if (ptr->tdr_remote_site)
	{
#ifndef SUPERSERVER
	ALICE_print (101, ptr->tdr_remote_site->str_data, 0, 0, 0, 0); /*msg 101: Remote Site: %s */
#else
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_remote_site);
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_remote_site->str_data));
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_remote_site->str_data) >> 8);
	for (i = 0; i < strlen (ptr->tdr_remote_site->str_data); i++)
	    SVC_putc (tdgbl->service_blk, (UCHAR) ptr->tdr_remote_site->str_data[i]);
#endif
	}

    if (ptr->tdr_fullpath)
	{
#ifndef SUPERSERVER
	ALICE_print (102, ptr->tdr_fullpath->str_data, 0, 0, 0, 0); /* msg 102: Database Path: %s */
#else
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_db_path);
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_fullpath->str_data));
	SVC_putc (tdgbl->service_blk, (UCHAR) strlen (ptr->tdr_fullpath->str_data) >> 8);
	for (i = 0; i < strlen (ptr->tdr_fullpath->str_data); i++)
	    SVC_putc (tdgbl->service_blk, (UCHAR) ptr->tdr_fullpath->str_data[i]);
#endif
	}

    }

/* let the user know what the suggested action is */

switch (TDR_analyze(trans))
    {
    case TRA_commit:
#ifndef SUPERSERVER
	ALICE_print (103, 0, 0, 0, 0, 0); /* msg 103: Automated recovery would commit this transaction. */
#else
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise);
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise_commit);
#endif
	break;

    case TRA_rollback:
#ifndef SUPERSERVER
	ALICE_print (104, 0, 0, 0, 0, 0); /* msg 104: Automated recovery would rollback this transaction. */
#else
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise);
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise_rollback);
#endif
	break;

    default:
#ifdef SUPERSERVER
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise);
	SVC_putc (tdgbl->service_blk, (UCHAR) isc_spb_tra_advise_unknown);
#endif
	break;
    }

}


static ULONG ask (void)
{
/**************************************
 *
 *	a s k
 *
 **************************************
 *
 * Functional description
 *	Ask the user whether to commit or rollback.
 *
 **************************************/
UCHAR	response [32], *p;
int	c;
int	*transaction;
STATUS	status_vector [20];
ULONG	switches;
TGBL    tdgbl;

tdgbl = GET_THREAD_DATA;

switches = 0;

while (TRUE)
    {
    ALICE_print (85, 0, 0, 0, 0, 0); /* msg 85: Commit, rollback, or neither (c, r, or n)? */
    if (tdgbl->sw_service)
	ib_putc ('\001', ib_stdout);
    ib_fflush (ib_stdout);
    for (p = response; (c = ib_getchar()) != '\n' && c != EOF;)
	*p++ = c;
    if (c == EOF && p == response)
	return (ULONG) -1;
    *p = 0;
    ALICE_down_case (response, response);
    if (!strcmp (response, "n") ||
        !strcmp (response, "c") || 
        !strcmp (response, "r"))
	break;
    }

if (response [0] == 'c')
    switches |= sw_commit;
else if (response [0] == 'r')
    switches |= sw_rollback;

return switches;
}

static void reattach_database (
    TDR		trans)
{
/**************************************
 *
 *	r e a t t a c h _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate pathnames for a given database 
 *	until the database is successfully attached.
 *
 **************************************/
STATUS	status_vector [20];
UCHAR	buffer [1024], *p, *q, *start;
USHORT	protocols = 0;
STR     string;
TGBL	tdgbl;

tdgbl = GET_THREAD_DATA;
       
/* determine what protocols are allowable for this OS */

#ifdef VMS
protocols |= DECNET_PROTOCOL | VMS_TCP_PROTOCOL;
#else
protocols |= TCP_PROTOCOL;
#endif

#ifdef ultrix
protocols |= DECNET_PROTOCOL;
#endif

#ifdef APOLLO
protocols |= APOLLO_PROTOCOL;
#endif

#ifdef OS2
protocols |= MSLAN_PROTOCOL;
#endif

ISC_get_host (buffer, sizeof (buffer));

/* if this is being run from the same host
   (or if this is apollo land), just
   try to reconnect using the same pathname */

if ((protocols & APOLLO_PROTOCOL) ||
    !strcmp (buffer, trans->tdr_host_site->str_data))
    {
    if (TDR_attach_database (status_vector, trans, trans->tdr_fullpath->str_data))
        return;
    }    

/* try going through the previous host with all available
   protocols, using chaining to try the same method of
   attachment originally used from that host */

else if (trans->tdr_host_site)
    {
    for (p = buffer, q = trans->tdr_host_site->str_data; *q;)
        *p++ = *q++;
    start = p;

    if (protocols & DECNET_PROTOCOL)
	{
	p = start;
	*p++ = ':';
	*p++ = ':';
        for (q = trans->tdr_fullpath->str_data; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
	}

    if (protocols & VMS_TCP_PROTOCOL)
	{
	p = start;
	*p++ = '^';
        for (q = trans->tdr_fullpath->str_data; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
	}

    if (protocols & TCP_PROTOCOL)
	{
	p = start;
	*p++ = ':';
        for (q = trans->tdr_fullpath->str_data; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
	}
    }    

/* attaching using the old method didn't work;
   try attaching to the remote node directly */

if (trans->tdr_remote_site)
    {
    for (p = buffer, q = trans->tdr_remote_site->str_data; *q;)
        *p++ = *q++;
    start = p;

    if (protocols & DECNET_PROTOCOL)
        {
        p = start;
        *p++ = ':';
        *p++ = ':';
        for (q = (UCHAR*) trans->tdr_filename; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
        }

    if (protocols & VMS_TCP_PROTOCOL)
        {
        p = start;
        *p++ = '^';
        for (q = (UCHAR*) trans->tdr_filename; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
        }

    if (protocols & TCP_PROTOCOL)
        {
        p = start;
        *p++ = ':';
        for (q = (UCHAR*) trans->tdr_filename; *q;)
            *p++ = *q++;
        *q = 0;
        if (TDR_attach_database (status_vector, trans, buffer))
            return;
        }

    if (protocols & MSLAN_PROTOCOL)
        {
        p = buffer;
        *p++ = '\\';
        *p++ = '\\';
        for (q = trans->tdr_remote_site->str_data; *q;)
            *p++ = *q++;
        for (q = (UCHAR*) trans->tdr_filename; *q;)
            *p++ = *q++;
        *q = 0;                   

        if (TDR_attach_database (status_vector, trans, buffer))
            return;
        }
    }
     
/* we have failed to reattach; notify the user
   and let them try to succeed where we have failed */

ALICE_print (86, trans->tdr_id, 0, 0, 0, 0); /* msg 86: Could not reattach to database for transaction %ld. */
ALICE_print (87, trans->tdr_fullpath->str_data, 0, 0, 0, 0); /* msg 87: Original path: %s */

for (;;)
    {
    ALICE_print (88, 0, 0, 0, 0, 0); /* msg 88: Enter a valid path:  */
    for (p = buffer; (*p = ib_getchar()) != '\n'; p++)
	;
    *p = 0;
    if (!buffer[0])
	break;
    p = buffer;
    while (*p == ' ')
	*p++;
    if (TDR_attach_database (status_vector, trans, p))
	{
	string = (STR) ALLOCDV (type_str, strlen (p) + 1);
	strcpy (string->str_data, p);
	string->str_length = strlen (p);
	trans->tdr_fullpath = string;
	trans->tdr_filename = (TEXT *)string->str_data;
	return;
	}
    ALICE_print (89, 0, 0, 0, 0, 0); /* msg 89: Attach unsuccessful. */
    }
}


static void reattach_databases (
    TDR		trans)
{
/**************************************
 *
 *	r e a t t a c h _ d a t a b a s e s
 *
 **************************************
 *
 * Functional description
 *	Attempt to locate all databases used in 
 *	a multidatabase transaction.
 *
 **************************************/
TDR	ptr;

for (ptr = trans; ptr; ptr = ptr->tdr_next)
    reattach_database (ptr);
}


static BOOLEAN reconnect (
    int		*handle,
    SLONG	number,
    TEXT	*name,
    ULONG	switches)
{
/**************************************
 *
 *	r e c o n n e c t
 *
 **************************************
 *
 * Functional description
 *	Commit or rollback a named transaction.
 *
 **************************************/
int	*transaction;
SLONG	id;
STATUS	status_vector [20];

id = gds__vax_integer (&number, 4);
transaction = NULL;
if (gds__reconnect_transaction (status_vector,
	    GDS_REF (handle),
	    GDS_REF (transaction),
	    sizeof (id),
	    GDS_REF (id)))
    {
    ALICE_print (90, name, 0, 0, 0, 0); /* msg 90: failed to reconnect to a transaction in database %s */
    ALICE_print_status (status_vector);
    return TRUE;
    }

if (!(switches & (sw_commit | sw_rollback)))
    {
    ALICE_print (91, number, 0, 0, 0, 0); /* msg 91: Transaction %ld: */
    switches = ask();
    if (switches == (ULONG) -1)
	{
	ALICE_print (84, 0, 0, 0, 0, 0); /* msg 84: unexpected end of input */
	return TRUE;
	}
    }

if (switches & sw_commit)
    gds__commit_transaction (status_vector, GDS_REF (transaction));
else if (switches & sw_rollback)
    gds__rollback_transaction (status_vector, GDS_REF (transaction));
else
    return FALSE;

if (status_vector [1])
    {
    ALICE_print_status (status_vector);
    return TRUE;
    }

return FALSE;
}
#endif /* GUI_TOOLS */


