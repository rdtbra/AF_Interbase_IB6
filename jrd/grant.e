/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		grant.e
 *	DESCRIPTION:	SQL Grant/Revoke Handler
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

#include <stdio.h>
#include <string.h>
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/scl.h"
#include "../jrd/acl.h"
#include "../jrd/irq.h"
#include "../jrd/blb.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../jrd/val.h"
#include "../jrd/met.h"
#include "../jrd/intl.h"
#ifdef NETWARE_386
#define V4_THREADING
#endif
#include "../jrd/nlm_thd.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/thd_proto.h"
#include "../jrd/ibsetjmp.h"

/* privileges given to the owner of a relation */

#define OWNER_PRIVS	SCL_control | SCL_read | SCL_write | SCL_delete | SCL_protect
#define VIEW_PRIVS	SCL_read | SCL_write | SCL_delete
#define ACL_BUFFER_SIZE	4096
#define DEFAULT_CLASS	"SQL$DEFAULT"

#define CHECK_ACL_BOUND(to, start, length_ptr, move_length)\
          {\
          if (((start)->str_data + *length_ptr) < (to + move_length))  {start = GRANT_realloc_acl(start, &to, length_ptr);} \
          }
#define CHECK_AND_MOVE(to, from, start, length_ptr) {CHECK_ACL_BOUND (to, start, length_ptr, 1); *(to)++ = from;}
#define CHECK_MOVE_INCR(to, from, start, length_ptr) {CHECK_ACL_BOUND (to, start, length_ptr, 1); *(to)++ = (from)++;}

DATABASE
    DB = STATIC "yachts.lnk";

static void	define_default_class	(TDBB, TEXT *, TEXT *,
						UCHAR *, USHORT);
static void	delete_security_class	(TDBB, TEXT *);
static void	finish_security_class (UCHAR **, USHORT, STR *, ULONG *);
static void	get_object_info		(TDBB, TEXT *, SSHORT,
						TEXT *, TEXT *, TEXT *,
						USHORT *);
static USHORT	get_public_privs	(TDBB, TEXT *, SSHORT);
static void	get_user_privs		(TDBB, UCHAR **, TEXT *,
						SSHORT, TEXT *, USHORT, 
                                                STR *, ULONG *);
static void	grant_user (UCHAR **, TEXT *, SSHORT, USHORT, 
                                   STR *, ULONG *);
static void	grant_views (UCHAR **, USHORT, STR *, ULONG *);
static void	purge_default_class (TEXT *, SSHORT);
static USHORT	save_field_privileges	(TDBB, STR *, UCHAR **,  
					TEXT *, TEXT *, USHORT, ULONG *);
static void	save_security_class	(TDBB, TEXT *,
						UCHAR *, USHORT);
static USHORT	trans_sql_priv (TEXT *);
static USHORT	terminate (TEXT *);
static SLONG 	squeeze_acl (UCHAR *, UCHAR **, TEXT *, SSHORT);
static BOOLEAN  check_string (TEXT *, TEXT *);

STR   GRANT_realloc_acl (
    STR  	start_ptr,
    UCHAR	**write_ptr,
    ULONG 	*buffer_length)
{
/**************************************
 *
 *	G R A N T _ r e a l l o c _ a c l 
 *
 **************************************
 *
 * Functional description
 *	The ACl list is greater than  the current length, Increase it by
 *	MAX_AXL_SIZE. Do a ERR_punt in case of no memory!
 *
 **************************************/

ULONG old_offset = (ULONG) (*write_ptr - start_ptr->str_data);
ULONG realloc_length = *buffer_length + ACL_BUFFER_SIZE;


/* realloc the new length, ERR_punt incase of no memory */
(void) ALL_extend (&start_ptr, realloc_length);

/* the write_ptr is set back to the same offset in the new buffer*/
*write_ptr = start_ptr->str_data + old_offset;


*buffer_length = realloc_length;

return (start_ptr);
}

int GRANT_privileges (
    TDBB	tdbb,
    SSHORT	phase,
    DFW		work)
{
/**************************************
 *
 *	G R A N T _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Compute access control list from SQL privileges.
 *	This calculation is tricky and involves interaction between 
 *	the relation-level and field-level privileges.  Do not change 
 *	the order of operations	lightly.
 *
 **************************************/

switch (phase)
    {
    case 1:
    case 2:
	return TRUE;

    case 3:
        {
	UCHAR	*acl, *temp_acl;
	UCHAR	*default_acl;
	TEXT	s_class [32], owner [32], default_class [32];
	STR str_buffer = NULL, str_default_buffer = NULL;
	USHORT	public, aggregate_public, restrct, view;
	ULONG length = ACL_BUFFER_SIZE, *length_ptr = &length;
	ULONG default_length = ACL_BUFFER_SIZE, *default_length_ptr = &default_length;

	JMP_BUF	env, *old_env;
        DBB	dbb;

	SET_TDBB (tdbb);

	restrct = FALSE;

	dbb = tdbb->tdbb_database;
	V4_JRD_MUTEX_LOCK (dbb->dbb_mutexes + DBB_MUTX_grant_priv);
	get_object_info (tdbb, work->dfw_name, work->dfw_id, owner,
			 s_class, default_class, &view);

	if (!s_class [0]) 
	    {
	    V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_grant_priv);
	    return FALSE;
	    }

	/* start the acl off by giving the owner all privileges */


	old_env = tdbb->tdbb_setjmp;
	tdbb->tdbb_setjmp = (UCHAR*) env;
	if (SETJMP (env))
	    {
            tdbb->tdbb_setjmp = (UCHAR*) old_env;
            if (str_buffer)
		{
  	    	ALL_release (str_buffer);
		}
            if (str_default_buffer)
  	        ALL_release (str_default_buffer);
            ERR_punt();
            }


 	str_buffer = (STR) ALLOCPV (type_str, ACL_BUFFER_SIZE);
 	str_default_buffer = (STR) ALLOCPV (type_str, ACL_BUFFER_SIZE);

	acl = str_buffer->str_data;

        CHECK_AND_MOVE (acl, ACL_version, str_buffer, length_ptr);
	grant_user (&acl, owner, obj_user, (work->dfw_id == obj_procedure) ?
	    (SCL_execute | OWNER_PRIVS) : OWNER_PRIVS, &str_buffer, length_ptr);

	/* Pick up any relation-level privileges */
        
	public = get_public_privs (tdbb, work->dfw_name, work->dfw_id);
        get_user_privs (tdbb, &acl, work->dfw_name, work->dfw_id,
			owner, public, &str_buffer, length_ptr);

	if (work->dfw_id == obj_relation)
	    {
            /* If we have the space to copy the acl list 
               no need to realloc */
            if (length > default_length)
                {
 	        (void) ALL_extend (&str_default_buffer, length);
                default_length = length;
                }
	    /* Now handle field-level privileges.  This might require adding
	       UPDATE privilege to the relation-level acl,  Therefore, save
	       off the relation acl because we need to add a default field
	       acl in that case. */

            MOVE_FAST (str_buffer->str_data, str_default_buffer->str_data, (int) (acl - str_buffer->str_data));
            default_acl = str_default_buffer->str_data + (acl - str_buffer->str_data);
            temp_acl = acl;

	    aggregate_public = save_field_privileges (tdbb, &str_buffer,
		&acl, work->dfw_name, owner, public, length_ptr);

	    /* SQL tables don't need the 'all priviliges to all views' acl anymore.
	       This special acl was only generated for SQL. */
	    /* grant_views (&acl, VIEW_PRIVS); */

	    /* finish off and store the security class for the relation */

	    finish_security_class (&acl, aggregate_public, &str_buffer, length_ptr);	
            
	    save_security_class (tdbb, s_class, str_buffer->str_data, acl - str_buffer->str_data); 

	    if (temp_acl != acl) /* relation privs were added? */
		restrct = TRUE;

	    /* if there have been privileges added at the relation level which
	       need to be restricted from other fields in the relation,
	       update the acl for them */

	    if (restrct)
		{
		finish_security_class (&default_acl, public, 
                                       &str_default_buffer, default_length_ptr);
		define_default_class (tdbb, work->dfw_name, default_class,
			str_default_buffer->str_data, default_acl - str_default_buffer->str_data);
		}
	    }
	else
	    {
	    finish_security_class (&acl, public, &str_buffer, length_ptr);	
	    save_security_class (tdbb, s_class, str_buffer->str_data, acl - str_buffer->str_data); 
	    }

        tdbb->tdbb_setjmp = (UCHAR*) old_env;
        if (str_buffer)
	    {
  	    ALL_release (str_buffer);
  	    ALL_release (str_default_buffer);
	    }

	V4_JRD_MUTEX_UNLOCK (dbb->dbb_mutexes + DBB_MUTX_grant_priv);
	break;
        }

    default:
        break;
    }

DFW_perform_system_work();

return FALSE;
}

static void define_default_class (
    TDBB	tdbb,
    TEXT	*relation_name,
    TEXT	*default_class,
    UCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	d e f i n e _ d e f a u l t _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Update the default security class for fields
 *	which have not been specifically granted
 *	any privileges.  We must grant them all
 *	privileges which were specifically granted
 *	at the relation level, but none of the
 *	privileges we added at the relation level
 *	for the purpose of accessing other fields.
 *
 **************************************/
DBB	dbb;
BLK	request;
DSC	desc;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

if (!*default_class)                           
    {      
#ifndef EXACT_NUMERICS
    sprintf (default_class, "%s%ld\0", DEFAULT_CLASS, 
	DPM_gen_id (tdbb, MET_lookup_generator (tdbb, DEFAULT_CLASS), 0, (SLONG) 1));
#else
    sprintf (default_class, "%s%" QUADFORMAT "d\0", DEFAULT_CLASS, 
	DPM_gen_id (tdbb, MET_lookup_generator (tdbb, DEFAULT_CLASS), 0, (SINT64) 1));
#endif
    
    request = (BLK) CMP_find_request (tdbb, irq_grant7, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request)
        REL IN RDB$RELATIONS
        WITH REL.RDB$RELATION_NAME EQ relation_name

	if (!REQUEST (irq_grant7))
	    REQUEST (irq_grant7) = request;
        MODIFY REL USING
            REL.RDB$DEFAULT_CLASS.NULL = FALSE;
	    jrd_vtof (default_class, REL.RDB$DEFAULT_CLASS,
		sizeof (REL.RDB$DEFAULT_CLASS));
        END_MODIFY;

    END_FOR;	

    if (!REQUEST (irq_grant7))
	REQUEST (irq_grant7) = request;
    }

save_security_class (tdbb, default_class, buffer, length);

desc.dsc_dtype = dtype_text;
desc.dsc_sub_type = 0;
desc.dsc_scale = 0;
desc.dsc_ttype = ttype_metadata;
desc.dsc_address = (UCHAR*) relation_name;
desc.dsc_length = strlen (desc.dsc_address);
DFW_post_work (dbb->dbb_sys_trans, dfw_scan_relation, &desc, 0);
}

static void delete_security_class (
    TDBB	tdbb,
    TEXT	*s_class)
{
/**************************************
 *
 *	d e l e t e _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Delete a security class.
 *
 **************************************/         
DBB	dbb;
BLK	handle;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

handle = NULL;

FOR (REQUEST_HANDLE handle)
	CLS IN RDB$SECURITY_CLASSES
	WITH CLS.RDB$SECURITY_CLASS EQ s_class
    ERASE CLS;
END_FOR;    

CMP_release (tdbb, handle);
}

static void finish_security_class (
    UCHAR	**acl_ptr,
    USHORT	public,
    STR	        *start_ptr,
    ULONG	*length_ptr)
{
/**************************************
 *
 *	f i n i s h _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Finish off a security class, putting
 *	in a wildcard for any public privileges.
 *
 **************************************/         
UCHAR 	*acl;
                            
acl = *acl_ptr;

if (public)
    {
    CHECK_AND_MOVE (acl, ACL_id_list, *start_ptr, length_ptr);
    SCL_move_priv (&acl, public, start_ptr, length_ptr);
    }

CHECK_AND_MOVE (acl, ACL_end, *start_ptr, length_ptr);

*acl_ptr = acl;
}

static USHORT get_public_privs (
    TDBB	tdbb,
    TEXT	*object_name,
    SSHORT	obj_type)
{
/**************************************
 *
 *	g e t _ p u b l i c _ p r i v s
 *
 **************************************
 *
 * Functional description
 *	Get public privileges for a particular object.
 *
 **************************************/         
DBB	dbb;
BLK	request;
USHORT	public;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

public = 0;

request = (BLK) CMP_find_request (tdbb, irq_grant5, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
       PRV IN RDB$USER_PRIVILEGES 
       WITH PRV.RDB$RELATION_NAME EQ object_name AND
       PRV.RDB$OBJECT_TYPE EQ obj_type AND
       PRV.RDB$USER EQ "PUBLIC" AND
       PRV.RDB$USER_TYPE EQ obj_user AND
       PRV.RDB$FIELD_NAME MISSING
    if (!REQUEST (irq_grant5))
	REQUEST (irq_grant5) = request;
    public |= trans_sql_priv (PRV.RDB$PRIVILEGE);
END_FOR;

if (!REQUEST (irq_grant5))
    REQUEST (irq_grant5) = request;

return public;
}

static void get_object_info (
    TDBB	tdbb,
    TEXT	*object_name,
    SSHORT	obj_type,
    TEXT	*owner,
    TEXT	*s_class,
    TEXT	*default_class,
    USHORT	*view)
{
/**************************************
 *
 *	g e t _ o b j e c t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	This could be done in MET_scan_relation () or MET_lookup_procedure,
 *	but presumably we wish to make sure the information we have is
 *	up-to-the-minute.
 *
 **************************************/         
DBB	dbb;
BLK	request;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

owner [0] = '\0';
s_class [0] = '\0';
default_class [0] = '\0';
*view = FALSE;

if (obj_type == obj_relation)
    {
    request = (BLK) CMP_find_request (tdbb, irq_grant1, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request) 
	REL IN RDB$RELATIONS WITH
	REL.RDB$RELATION_NAME EQ object_name

	if (!REQUEST (irq_grant1))
	    REQUEST (irq_grant1) = request;
	terminate (REL.RDB$SECURITY_CLASS);
	strcpy (s_class, REL.RDB$SECURITY_CLASS);
	terminate (REL.RDB$DEFAULT_CLASS);
	strcpy (default_class, REL.RDB$DEFAULT_CLASS);
	terminate (REL.RDB$OWNER_NAME);
	strcpy (owner, REL.RDB$OWNER_NAME);
	*view = (REL.RDB$VIEW_BLR.gds_quad_high ||
		 REL.RDB$VIEW_BLR.gds_quad_low) ? TRUE : FALSE;
    END_FOR;

    if (!REQUEST (irq_grant1))
	REQUEST (irq_grant1) = request;
    }
else
    {
    request = (BLK) CMP_find_request (tdbb, irq_grant9, IRQ_REQUESTS);

    FOR (REQUEST_HANDLE request) 
	REL IN RDB$PROCEDURES WITH
	REL.RDB$PROCEDURE_NAME EQ object_name

	if (!REQUEST (irq_grant9))
	    REQUEST (irq_grant9) = request;
	terminate (REL.RDB$SECURITY_CLASS);
	strcpy (s_class, REL.RDB$SECURITY_CLASS);
	strcpy (default_class, "");
	terminate (REL.RDB$OWNER_NAME);
	strcpy (owner, REL.RDB$OWNER_NAME);
	*view = FALSE;
    END_FOR;

    if (!REQUEST (irq_grant9))
	REQUEST (irq_grant9) = request;
    }
}

static void get_user_privs (
    TDBB	tdbb,
    UCHAR	**acl_ptr,
    TEXT	*object_name,
    SSHORT	obj_type,
    TEXT	*owner,
    USHORT	public,
    STR  	*start_ptr,
    ULONG	*length_ptr)
{
/**************************************
 *
 *	g e t _ u s e r _ p r i v s
 *
 **************************************
 *
 * Functional description
 *	Get privileges for a particular object.
 *
 **************************************/         
DBB	dbb;
BLK	request;
UCHAR 	*acl;
TEXT	user [32], *p;
USHORT	priv, l;
SSHORT	user_type;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

acl = *acl_ptr;
user [0] = 0;
user_type = -2;

request = (BLK) CMP_find_request (tdbb, irq_grant2, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) 
	PRV IN RDB$USER_PRIVILEGES 
	WITH PRV.RDB$RELATION_NAME EQ object_name AND
	PRV.RDB$OBJECT_TYPE EQ obj_type AND
	(PRV.RDB$USER NE "PUBLIC" OR PRV.RDB$USER_TYPE NE obj_user) AND
	(PRV.RDB$USER NE owner OR PRV.RDB$USER_TYPE NE obj_user) AND
	PRV.RDB$FIELD_NAME MISSING
	SORTED BY PRV.RDB$USER, PRV.RDB$USER_TYPE

    if (!REQUEST (irq_grant2))
	REQUEST (irq_grant2) = request;
    terminate (PRV.RDB$USER);
    if (strcmp (PRV.RDB$USER, user) || PRV.RDB$USER_TYPE != user_type)
	{
	if (user [0])
	    grant_user (&acl, user, user_type, priv, start_ptr, length_ptr);
	user_type = PRV.RDB$USER_TYPE;
	if (user_type == obj_user)
	    priv = public;
	else
	    priv = 0;
	strcpy (user, PRV.RDB$USER);
	}
    priv |= trans_sql_priv (PRV.RDB$PRIVILEGE);
END_FOR;       
             
if (!REQUEST (irq_grant2))
    REQUEST (irq_grant2) = request;

if (user [0])
    grant_user (&acl, user, user_type, priv, start_ptr, length_ptr);

*acl_ptr = acl; 
}

static void grant_user (
    UCHAR	**acl_ptr,
    TEXT	*user,
    SSHORT	user_type,
    USHORT	privs,
    STR 	*start_ptr,
    ULONG	*length_ptr)
{
/**************************************
 *
 *	g r a n t _ u s e r
 *
 **************************************
 *
 * Functional description
 *	Grant privileges to a particular user.
 *
 **************************************/         
UCHAR 	*acl;
UCHAR	length;
                            
acl = *acl_ptr;

CHECK_AND_MOVE (acl, ACL_id_list, *start_ptr, length_ptr);
switch (user_type)
    {
    case obj_user_group:
 	CHECK_AND_MOVE (acl, id_group, *start_ptr, length_ptr);
        break;

    case obj_sql_role:
 	CHECK_AND_MOVE (acl, id_sql_role, *start_ptr, length_ptr);
        break;

    case obj_user:
 	CHECK_AND_MOVE (acl, id_person, *start_ptr, length_ptr);
	break;

    case obj_procedure:
 	CHECK_AND_MOVE (acl, id_procedure, *start_ptr, length_ptr);
	break;

    case obj_trigger:
 	CHECK_AND_MOVE (acl, id_trigger, *start_ptr, length_ptr);
	break;

    case obj_view:
        CHECK_AND_MOVE (acl, id_view, *start_ptr, length_ptr);
	break;

    default:
	BUGCHECK (292); /* Illegal user_type */

    }

length = strlen(user);
CHECK_AND_MOVE (acl, (UCHAR ) length, *start_ptr, length_ptr);
if (length)
    {
    CHECK_ACL_BOUND (acl, *start_ptr, length_ptr, length);
    MOVE_FAST (user, acl, length);
    acl += length;
    }

SCL_move_priv (&acl, privs, start_ptr, length_ptr);

*acl_ptr = acl;
}

static void grant_views (
    UCHAR	**acl_ptr,
    USHORT	privs,
    STR  	*start_ptr,
    ULONG	*length_ptr)
{
/**************************************
 *
 *	g r a n t _ v i e w s
 *
 **************************************
 *
 * Functional description
 *	Grant privileges to all views.
 *
 **************************************/         
UCHAR 	*acl;
TEXT	*p;
                            
acl = *acl_ptr;

CHECK_AND_MOVE (acl, ACL_id_list, *start_ptr, length_ptr);
CHECK_AND_MOVE (acl, id_views, *start_ptr, length_ptr);
CHECK_AND_MOVE (acl, 0, *start_ptr, length_ptr);
SCL_move_priv (&acl, privs, start_ptr, length_ptr);

*acl_ptr = acl;
}

static void purge_default_class (
    TEXT	*object_name,
    SSHORT	obj_type)
{
/**************************************
 *
 *	p u r g e _ d e f a u l t _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Get rid of any previously defined 
 *	default security class for this relation.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
DSC	desc;

tdbb = GET_THREAD_DATA;
dbb = tdbb->tdbb_database;

request = (BLK) CMP_find_request (tdbb, irq_grant8, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request)
    REL IN RDB$RELATIONS
    WITH REL.RDB$RELATION_NAME EQ object_name
    AND REL.RDB$DEFAULT_CLASS NOT MISSING

    if (!REQUEST (irq_grant8))
	REQUEST (irq_grant8) = request;
    terminate (REL.RDB$DEFAULT_CLASS);
    delete_security_class (tdbb, REL.RDB$DEFAULT_CLASS);

    MODIFY REL USING
        REL.RDB$DEFAULT_CLASS.NULL = TRUE;
    END_MODIFY;

END_FOR;	

if (!REQUEST (irq_grant8))
    REQUEST (irq_grant8) = request;

desc.dsc_dtype = dtype_text;
desc.dsc_sub_type = 0;
desc.dsc_scale = 0;
desc.dsc_ttype = ttype_metadata;
desc.dsc_address = (UCHAR*) object_name;
desc.dsc_length = strlen (desc.dsc_address);
DFW_post_work (dbb->dbb_sys_trans, dfw_scan_relation, &desc, 0);
}

static USHORT save_field_privileges (
    TDBB	tdbb,
    STR  	*str_relation_buffer,
    UCHAR	**acl_ptr,
    TEXT	*relation_name,
    TEXT	*owner,
    USHORT	public,
    ULONG	*length_ptr)
{
/**************************************
 *
 *	s a v e _ f i e l d _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Compute the privileges for all fields within a relation.
 *	All fields must be given the initial relation-level privileges.
 *	Conversely, field-level privileges must be added to the relation 
 *	security class to be effective.
 *
 **************************************/
DBB	dbb;
BLK	request, request2;
TEXT	field_name [32], user [32], s_class [32];
UCHAR	*field_acl, *relation_acl;
USHORT	priv, field_public, aggregate_public;
USHORT	field_priv, relation_priv;
DSC	desc;
SSHORT	user_type;
int	field_buffer_start_index, i;

STR relation_buffer = *str_relation_buffer;

JMP_BUF	env, *old_env;
STR 	str_field_buffer = NULL, str_field_buffer_start = NULL;

ULONG field_length, start_length;
ULONG *field_length_ptr = &field_length, *start_length_ptr = &start_length;


SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

old_env = tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (str_field_buffer)
  	ALL_release (str_field_buffer);
    if (str_field_buffer_start)
  	ALL_release (str_field_buffer_start);
    ERR_punt();
    }
/* initialize the field-level acl buffer to include all relation-level privs */

str_field_buffer_start = ALLOCPV (type_str, *length_ptr);
str_field_buffer = ALLOCPV (type_str, *length_ptr);

field_length = start_length = *length_ptr;



MOVE_FAST (relation_buffer->str_data, str_field_buffer->str_data, (int) (*acl_ptr - relation_buffer->str_data));
field_buffer_start_index = (int) (*acl_ptr - relation_buffer->str_data);
field_acl = str_field_buffer->str_data + field_buffer_start_index;
relation_acl = relation_buffer->str_data + field_buffer_start_index;

/* remember this starting point for subsequent fields. */
for (i = 0; field_buffer_start_index > i; i++, str_field_buffer_start->str_data[i] = str_field_buffer->str_data[i]);

field_name [0] = 0;
user [0] = 0;
aggregate_public = public;

request = (BLK) CMP_find_request (tdbb, irq_grant6, IRQ_REQUESTS);
request2 = NULL;

FOR (REQUEST_HANDLE request)
	FLD IN RDB$RELATION_FIELDS CROSS
	PRV IN RDB$USER_PRIVILEGES
	OVER RDB$RELATION_NAME, RDB$FIELD_NAME
	WITH PRV.RDB$OBJECT_TYPE EQ obj_relation AND
	PRV.RDB$RELATION_NAME EQ relation_name AND
	PRV.RDB$FIELD_NAME NOT MISSING AND
	(PRV.RDB$USER NE owner OR PRV.RDB$USER_TYPE NE obj_user)
	SORTED BY PRV.RDB$FIELD_NAME, PRV.RDB$USER

    if (!REQUEST (irq_grant6))
	REQUEST (irq_grant6) = request;
    terminate (PRV.RDB$USER);
    terminate (PRV.RDB$FIELD_NAME);

    /* create a control break on field_name,user */

    if (strcmp (PRV.RDB$USER, user) ||
	strcmp (PRV.RDB$FIELD_NAME, field_name))
	{            
	/* flush out information for old user */

	if (user [0])
	    if (strcmp (user, "PUBLIC"))
		{
		field_priv = public | priv | squeeze_acl (str_field_buffer->str_data, &field_acl, user, user_type);
		grant_user (&field_acl, user, user_type, field_priv, &str_field_buffer, field_length_ptr);
		relation_priv = public | priv | squeeze_acl (relation_buffer->str_data, &relation_acl, user, user_type);
		grant_user (&relation_acl, user, user_type, relation_priv, str_relation_buffer, length_ptr);
		}
	    else
		field_public = field_public | public | priv;

	/* initialize for new user */

	priv = 0;
	strcpy (user, PRV.RDB$USER);
	user_type = PRV.RDB$USER_TYPE;
	}

    /* create a control break on field_name */

    if (strcmp (PRV.RDB$FIELD_NAME, field_name))
        {    
        /* finish off the last field, adding a wildcard at end, giving PUBLIC
	   all privileges available at the table level as well as those
	   granted at the field level */

	if (field_name [0])
	    {            
	    aggregate_public |= field_public;
	    finish_security_class (&field_acl, field_public | public, &str_field_buffer, field_length_ptr);
	    save_security_class (tdbb, s_class, str_field_buffer->str_data,
				 field_acl - str_field_buffer->str_data);
	    }

	/* initialize for new field */

	strcpy (field_name, PRV.RDB$FIELD_NAME);
        terminate (FLD.RDB$SECURITY_CLASS);
	strcpy (s_class, FLD.RDB$SECURITY_CLASS);
	if (!s_class [0])
	    {
	    /* There is no security class name for this field, then make one.
	       Note that the field security class name is set here and in the
	       pre-store trigger for rdb$user_privileges.  This field security
	       class name is removed in the pre erase trigger for
	       rdb$user_privileges. */
	    FOR (REQUEST_HANDLE request2)
		FLD2 IN RDB$RELATION_FIELDS WITH
		FLD2.RDB$RELATION_NAME EQ FLD.RDB$RELATION_NAME
		AND FLD2.RDB$FIELD_NAME EQ FLD.RDB$FIELD_NAME
		MODIFY FLD2
#ifndef EXACT_NUMERICS
    		    sprintf (s_class, "%s%ld\0", "SQL$GRANT", 
			     DPM_gen_id (tdbb, MET_lookup_generator (tdbb, "RDB$SECURITY_CLASS"), 
					 0, (SLONG) 1));
#else
    		    sprintf (s_class, "%s%" QUADFORMAT "d\0", "SQL$GRANT", 
			     DPM_gen_id (tdbb, MET_lookup_generator (tdbb, "RDB$SECURITY_CLASS"), 
					 0, (SINT64) 1));
#endif
	    	    jrd_vtof (s_class, FLD2.RDB$SECURITY_CLASS, sizeof (FLD2.RDB$SECURITY_CLASS));
		END_MODIFY;
	    END_FOR;
	    }
	field_public = 0;

	/* restart a security class at the end of the relation-level privs */

        for (i = 0; field_buffer_start_index > i; i++, str_field_buffer->str_data[i] = str_field_buffer_start->str_data[i]);
        field_acl = str_field_buffer->str_data + field_buffer_start_index;
	}

    priv |= trans_sql_priv (PRV.RDB$PRIVILEGE);
END_FOR;

if (!REQUEST (irq_grant6))
    REQUEST (irq_grant6) = request;

if (request2)
    CMP_release (tdbb, request2);

/* flush out the last user's info */

if (user [0])
    if (strcmp (user, "PUBLIC"))
	{
	field_priv = public | priv | squeeze_acl (str_field_buffer->str_data, &field_acl, user, user_type);
	grant_user (&field_acl, user, user_type, field_priv, &str_field_buffer, field_length_ptr);
	relation_priv = public | priv | squeeze_acl (relation_buffer->str_data, &relation_acl, user, user_type);
	grant_user (&relation_acl, user, user_type, relation_priv, str_relation_buffer, length_ptr);
	}
    else
        field_public = field_public | public | priv;

/* flush out the last field's info, and schedule a format update */

if (field_name [0])
    {
    aggregate_public |= field_public;
    finish_security_class (&field_acl, field_public | public, &str_field_buffer, field_length_ptr);
    save_security_class (tdbb, s_class, str_field_buffer->str_data,
			 field_acl - str_field_buffer->str_data);
    desc.dsc_dtype = dtype_text;
    desc.dsc_sub_type = 0;
    desc.dsc_scale = 0;
    desc.dsc_ttype = ttype_metadata;
    desc.dsc_address = (UCHAR*) relation_name;
    desc.dsc_length = strlen (desc.dsc_address);
    DFW_post_work (dbb->dbb_sys_trans, dfw_update_format, &desc, 0);
    }
      
*acl_ptr = relation_acl;

tdbb->tdbb_setjmp = (UCHAR*) old_env;
if (str_field_buffer)
    {
    ALL_release (str_field_buffer);
    ALL_release (str_field_buffer_start);
    }

return aggregate_public;
}

static void save_security_class (
    TDBB	tdbb,
    TEXT	*s_class,
    UCHAR	*buffer,
    USHORT	length)
{
/**************************************
 *
 *	s a v e _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Store or update the named security class.
 *
 **************************************/         
DBB		dbb;
BLK		request;
USHORT		found;
GDS__QUAD	blob_id;
BLB		blob;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

blob = BLB_create (tdbb, dbb->dbb_sys_trans, &blob_id);
BLB_put_segment (tdbb, blob, buffer, length);
BLB_close (tdbb, blob);

request = (BLK) CMP_find_request (tdbb, irq_grant3, IRQ_REQUESTS);

found = FALSE;
FOR (REQUEST_HANDLE request)
	CLS IN RDB$SECURITY_CLASSES
	WITH CLS.RDB$SECURITY_CLASS EQ s_class
    if (!REQUEST (irq_grant3))
	REQUEST (irq_grant3) = request;
    found = TRUE;
    MODIFY CLS
	CLS.RDB$ACL = blob_id;
    END_MODIFY;
END_FOR;

if (!REQUEST (irq_grant3))
    REQUEST (irq_grant3) = request;

if (!found)
    {
    request = (BLK) CMP_find_request (tdbb, irq_grant4, IRQ_REQUESTS);

    STORE (REQUEST_HANDLE request)
	    CLS IN RDB$SECURITY_CLASSES
	jrd_vtof (s_class, CLS.RDB$SECURITY_CLASS, sizeof (CLS.RDB$SECURITY_CLASS));
	CLS.RDB$ACL = blob_id;
    END_STORE;

    if (!REQUEST (irq_grant4))
	REQUEST (irq_grant4) = request;
    }
}

static USHORT trans_sql_priv (
    TEXT	*privileges)
{
/**************************************
 *
 *	t r a n s _ s q l _ p r i v
 *
 **************************************
 *
 * Functional description
 *	Map a SQL privilege letter into an internal privilege bit.
 *
 **************************************/
USHORT	priv;

priv = 0;

switch (UPPER7 (privileges [0]))
    {
    case 'S':	priv |= SCL_read;		break;
    case 'I':	priv |= SCL_sql_insert;		break;
    case 'U':	priv |= SCL_sql_update;		break;
    case 'D':	priv |= SCL_sql_delete;		break;
    case 'R':	priv |= SCL_sql_references;	break;
    case 'X':	priv |= SCL_execute;		break;
    }

return priv;
}

static USHORT terminate (
    TEXT	*string)
{
/**************************************
 *
 *	t e r m i n a t e
 *
 **************************************
 *
 * Functional description
 *	Zap trailing blank or null terminated string to null terminated string.
 *	Return length.
 *
 **************************************/
TEXT	*p, *q;
#define	BLANK	'\040'

/* Scan forwards to end-of-string -- record the last non-BLANK we saw */
q = string-1;
for (p = string; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

*(q+1) = '\0';

return (q+1) - string;
}

static SLONG squeeze_acl (
    UCHAR	*acl_base,
    UCHAR	**acl_ptr,
    TEXT        *user,
    SSHORT      user_type)
{
/**************************************
 *
 *	s q u e e z e _ a c l
 *
 **************************************
 *
 * Functional description
 *	Walk an access control list looking for a hit.  If a hit
 *	is found, return privileges and squeeze out that acl-element.
 *	The caller will use the returned privilege to insert a new
 *	privilege for the input user.
 *
 **************************************/
UCHAR		*dup_acl;
UCHAR		*acl;
UCHAR		*dest, *source;
SLONG		privilege = 0; 
USHORT		hit = FALSE;
UCHAR		c, *p;
int		length;
UCHAR		l;


/* Make sure that this half-finished acl looks good enough to process. */
**acl_ptr = 0;
acl = acl_base;

if (*acl++ != ACL_version)
    BUGCHECK (160); /* msg 160 wrong ACL version */

while (c = *acl++)
    switch (c)
	{
	case ACL_id_list:
	    dup_acl = acl-1;
	    hit = TRUE;
	    while (c = *acl++)
		{
		switch (c)
		    {
		    case id_person:
			if (user_type != obj_user)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
			break;

		    case id_sql_role:
			if (user_type != obj_sql_role)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
			break;

		    case id_view:
			if (user_type != obj_view)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
			break;

		    case id_procedure:
			if (user_type != obj_procedure)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
			break;

		    case id_trigger:
			if (user_type != obj_trigger)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
			break;

		    case id_project:
		    case id_organization:
			hit = FALSE;
			check_string (acl, user);
			break;

		    case id_views:
			hit = FALSE;
			break;

		    case id_node:
		    case id_user:
			hit = FALSE;
			for ( l = *acl++; l; acl++, l--);
			break;

		    case id_group:
			if (user_type != obj_user_group)
			    hit = FALSE;
			if (check_string (acl, user))
			    hit = FALSE;
		        break;
		    
		    default:
    	    		BUGCHECK (293); /* bad ACL */
		    }
		acl += *acl + 1;
		}
	    break;

	case ACL_priv_list:
	    if (hit)
		{
		while (c = *acl++)
		    switch (c)
			{
			case priv_control:
			    privilege |= SCL_control;
			    break;
                                                            
			case priv_read:
			    privilege |= SCL_read;
			    break;

			case priv_write:
			    privilege |= SCL_write;
			    break;

			case priv_sql_insert:
			    privilege |= SCL_sql_insert;
			    break;

			case priv_sql_delete:
			    privilege |= SCL_sql_delete;
			    break;

			case priv_sql_references:
			    privilege |= SCL_sql_references;
			    break;

			case priv_sql_update:
			    privilege |= SCL_sql_update;
			    break;

			case priv_delete:
			    privilege |= SCL_delete;
			    break;

			case priv_grant:
			    privilege |= SCL_grant;
			    break;

			case priv_protect:
			    privilege |= SCL_protect;
			    break;

			case priv_execute:
			    privilege |= SCL_execute;
			    break;

			default:
    	    		    BUGCHECK (293); /* bad ACL */
			}
		/* Squeeze out duplicate acl element. */
		dest = dup_acl;
		source = acl;
		length = *acl_ptr - source + 1;
		*acl_ptr = *acl_ptr - (source - dest);
		acl = dup_acl;
		for (; length; *dest++ = *source++, length--);
		}
	    else
	        while (*acl++)
		    ;
	    break;

	default:
    	    BUGCHECK (293); /* bad ACL */
	}

return privilege;
}

static BOOLEAN check_string (
    TEXT        *acl,
    TEXT        *string)
{
/**************************************
 *
 *      c h e c k _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *      Check a string against an acl string.  If they don't match,
 *      return TRUE.
 *
 **************************************/
USHORT  l;
TEXT    c1, c2;
	       
/* JPN: Since Kanji User names are not allowed, No need to fix this UPPER loop. */
			
if (l = *acl++)
    do {
	c1 = *acl++;
	c2 = *string++;
	if (UPPER7 (c1) != UPPER7 (c2))
	    return TRUE;
	} while (--l);
									   
return (*string && *string != ' ') ? TRUE : FALSE;
}

