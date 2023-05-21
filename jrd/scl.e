/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		scl.e
 *	DESCRIPTION:	Security class handler
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
#include "../jrd/ibsetjmp.h"
#include "../jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/ods.h"
#include "../jrd/scl.h"
#include "../jrd/pwd.h"
#include "../jrd/acl.h"
#include "../jrd/blb.h"
#include "../jrd/irq.h"
#include "../jrd/obj.h"
#include "../jrd/req.h"
#include "../jrd/tra.h"
#include "../jrd/gdsassert.h"
#include "../jrd/all_proto.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/enc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/isc_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/pwd_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/thd_proto.h"

#ifdef VMS
#define UIC_BASE	8
#else
#define UIC_BASE	10
#endif

#define	BLOB_BUFFER_SIZE	4096	/* used to read in acl blob */

#define CHECK_ACL_BOUND(to, start, length_ptr, check_length)\
          {\
          if (((start)->str_data + *length_ptr) < (to + check_length))  {start = GRANT_realloc_acl( start, &to, length_ptr);} \
          }
#define CHECK_AND_MOVE(to, from, start, length_ptr) {CHECK_ACL_BOUND (to, start, length_ptr, 1); *(to)++ = from;}
#define CHECK_MOVE_INCR(to, from, start, length_ptr) {CHECK_ACL_BOUND (to, start, length_ptr, 1); *(to)++ = (from)++;}


DATABASE
    DB = FILENAME "ODS.RDB";

static BOOLEAN	check_hex (TEXT *, USHORT);
static BOOLEAN	check_number (TEXT *, USHORT);
static BOOLEAN	check_user_group (TEXT *, USHORT, STR *, ULONG *);
static BOOLEAN	check_string (TEXT *, TEXT *);
static SLONG	compute_access	(TDBB, SCL, REL, TEXT *, TEXT *);
static TEXT	*save_string (TEXT *, TEXT **);
static SLONG	walk_acl (TDBB, TEXT *, REL, TEXT *, TEXT *, STR *, ULONG *);

typedef struct {
    USHORT	p_names_priv;
    USHORT	p_names_acl;
    CONST TEXT	*p_names_string;
} P_NAMES;

static CONST P_NAMES	p_names [] =
    {
    SCL_protect, priv_protect, "protect",
    SCL_control, priv_control, "control",
    SCL_delete, priv_delete, "delete",
    SCL_sql_insert, priv_sql_insert, "insert/write",
    SCL_sql_update, priv_sql_update, "update/write",
    SCL_sql_delete, priv_sql_delete, "delete/write",
    SCL_write, priv_write, "write",
    SCL_read, priv_read, "read/select",
    SCL_grant, priv_grant, "grant",
    SCL_sql_references, priv_sql_references, "references",
    SCL_execute, priv_execute, "execute",
    0, 0, ""
    };

void SCL_check_access (
    SCL		s_class,
    REL		view,
    TEXT	*trg_name,
    TEXT	*prc_name,
    USHORT	mask,
    TEXT	*type,
    TEXT	*name)
{
/**************************************
 *
 *	S C L _ c h e c k _ a c c e s s
 *
 **************************************
 *
 * Functional description
 *	Check security class for desired permission.  Check first that
 *	the desired access has been granted to the database then to the
 *	object in question.
 *
 **************************************/
TDBB	tdbb;
P_NAMES	*names;
SCL	att_class;
ATT	attachment;

tdbb = GET_THREAD_DATA;

if (s_class && (s_class->scl_flags & SCL_corrupt))
    ERR_post (gds__no_priv, gds_arg_string, "(ACL unrecognized)",
	gds_arg_string, "security_class", gds_arg_string, s_class->scl_name, 0);

attachment = tdbb->tdbb_attachment;

if ((att_class = attachment->att_security_class) &&
    !(att_class->scl_flags & mask))
    {
    type = "DATABASE";
    name = "";
    }
else if (!s_class ||
	 (mask & s_class->scl_flags) ||
	 ((view || trg_name || prc_name) &&
	  (compute_access (tdbb, s_class, view, trg_name, prc_name) & 
	   mask)))
    return;

/*
** allow the database owner to back up a database even if he does not have
** read access to all the tables in the database
*/
if ((attachment->att_flags & ATT_gbak_attachment) && (mask & SCL_read))
    return;

for (names = p_names; names->p_names_priv; names++)
    if (names->p_names_priv & mask)
	break;

ERR_post (gds__no_priv, gds_arg_string, names->p_names_string,
	gds_arg_string, type, gds_arg_string, ERR_cstring (name), 0);
}

void SCL_check_index (
    TDBB	tdbb,
    TEXT	*index_name,
    USHORT	mask)
{
/******************************************************
 *
 *	S C L _ c h e c k _ i n d e x 
 *
 ******************************************************
 *
 * Functional description
 *	Given a index name (as a TEXT), check for a 
 *      set of privileges on the table that the index is on and 
 *      on the fields involved in that index.  
 *
 *******************************************************/
VOLATILE BLK	request;
SCL	s_class, default_s_class;
TEXT    reln_name[32];
DBB	dbb;
JMP_BUF env, *old_env;


SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;
s_class = default_s_class = NULL;

/* no security to check for if the index is not yet created */

if (!index_name || !*index_name)
    return;
    
request = NULL;

/* No need to cache this request handle, it's only used when
   new constraints are created */

FOR (REQUEST_HANDLE request) IND IN RDB$INDICES 
    CROSS REL IN RDB$RELATIONS 
    OVER RDB$RELATION_NAME
    WITH IND.RDB$INDEX_NAME EQ index_name
    strcpy (reln_name, REL.RDB$RELATION_NAME);
    if (!REL.RDB$SECURITY_CLASS.NULL)
	s_class = SCL_get_class (REL.RDB$SECURITY_CLASS);
    if (!REL.RDB$DEFAULT_CLASS.NULL)
	default_s_class = SCL_get_class (REL.RDB$DEFAULT_CLASS);
END_FOR;

CMP_release (tdbb, request);

/* check if the relation exixts. It may not have been created yet.
   Just return in that case. */
   
if (!reln_name || !*reln_name)
    return;

SCL_check_access (s_class, NULL, NULL, NULL, mask, "TABLE", reln_name);

request = NULL;

/* set up the setjmp mechanism, so that we can release the request
   in case of error in SCL_check_access */

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (request)
        CMP_release (tdbb, request);
    LONGJMP (tdbb->tdbb_setjmp, (int) tdbb->tdbb_status_vector [1]);
    }


/* check if the field used in the index has the appropriate
   permission. If the field in question does not have a security class
   defined, then the default security class for the table applies for that
   field. */
   
/* No need to cache this request handle, it's only used when
   new constraints are created */

FOR (REQUEST_HANDLE request) ISEG IN RDB$INDEX_SEGMENTS
    CROSS RF IN RDB$RELATION_FIELDS 
    OVER RDB$FIELD_NAME
    WITH RF.RDB$RELATION_NAME EQ reln_name 
     AND ISEG.RDB$INDEX_NAME EQ index_name

    if (!RF.RDB$SECURITY_CLASS.NULL)
	{
	s_class = SCL_get_class (RF.RDB$SECURITY_CLASS);
        SCL_check_access (s_class, NULL, NULL, NULL, mask, 
			  "COLUMN", RF.RDB$FIELD_NAME);
	}
    else
        SCL_check_access (default_s_class, NULL, NULL, NULL, mask, 
			  "COLUMN", RF.RDB$FIELD_NAME);

END_FOR;

CMP_release (tdbb, request);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
}

void SCL_check_procedure (
    DSC		*dsc_name,
    USHORT	mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Given a procedure name, check for a set of privileges.  The
 *	procedure in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
SCL	s_class;
TEXT	*p, *q, *endp, *endq, name [32];

tdbb = GET_THREAD_DATA;

/* Get the name in CSTRING format, ending on NULL or SPACE */

assert (dsc_name->dsc_dtype == dtype_text);

for (p = name, endp = name + sizeof(name)-1, 
     q = dsc_name->dsc_address, endq = q + dsc_name->dsc_length;
     q < endq && p < endp && *q && *q != ' ';)
     *p++ = *q++;
*p = 0;

dbb = tdbb->tdbb_database;

s_class = NULL;

request = (BLK) CMP_find_request (tdbb, irq_p_security, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) PRC IN RDB$PROCEDURES WITH
	PRC.RDB$PROCEDURE_NAME EQ name
    if (!REQUEST (irq_p_security))
	REQUEST (irq_p_security) = request;
    if (!PRC.RDB$SECURITY_CLASS.NULL)
	s_class = SCL_get_class (PRC.RDB$SECURITY_CLASS);
END_FOR;

if (!REQUEST (irq_p_security))
    REQUEST (irq_p_security) = request;

SCL_check_access (s_class, NULL, NULL, name, mask, "PROCEDURE", name);
}

void SCL_check_relation (
    DSC		*dsc_name,
    USHORT	mask)
{
/**************************************
 *
 *	S C L _ c h e c k _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Given a relation name, check for a set of privileges.  The
 *	relation in question may or may not have been created, let alone
 *	scanned.  This is used exclusively for meta-data operations.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
BLK	request;
SCL	s_class;
TEXT	*p, *q, *endp, *endq, name [32];

tdbb = GET_THREAD_DATA;

/* Get the name in CSTRING format, ending on NULL or SPACE */

assert (dsc_name->dsc_dtype == dtype_text);

for (p = name, endp = name + sizeof(name)-1, 
     q = dsc_name->dsc_address, endq = q + dsc_name->dsc_length;
     q < endq && p < endp && *q && *q != ' ';)
     *p++ = *q++;
*p = 0;

dbb = tdbb->tdbb_database;

s_class = NULL;

request = (BLK) CMP_find_request (tdbb, irq_v_security, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) REL IN RDB$RELATIONS WITH
	REL.RDB$RELATION_NAME EQ name
    if (!REQUEST (irq_v_security))
	REQUEST (irq_v_security) = request;
    if (!REL.RDB$SECURITY_CLASS.NULL)
	s_class = SCL_get_class (REL.RDB$SECURITY_CLASS);
END_FOR;

if (!REQUEST (irq_v_security))
    REQUEST (irq_v_security) = request;

SCL_check_access (s_class, NULL, NULL, NULL, mask, "TABLE", name);
}

SCL SCL_get_class (
    TEXT	*string)
{
/**************************************
 *
 *	S C L _ g e t _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Look up security class first in memory, then in database.  If
 *	we don't find it, just return NULL.  If we do, return a security
 *	class block.
 *
 **************************************/
TDBB	tdbb;
DBB	dbb;
SCL	s_class;
TEXT	name [32], *p, *q;
ATT	attachment;
USHORT	name_length = 0;

tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

/* Name may be absent or terminated with NULL or blank.  Clean up name. */

if (!string)
    return NULL;

MET_exact_name (string);
name_length = strlen (string);
p = name;
while (name_length--)
    *p++ = *string++;
*p = 0;

if (!name [0])
    return NULL;

attachment = tdbb->tdbb_attachment;

/* Look for the class already known */

for (s_class = attachment->att_security_classes; s_class; s_class = s_class->scl_next)
    if (!strcmp (name, s_class->scl_name))
	return s_class;

/* Class isn't known.  So make up a new security class block */

s_class = (SCL) ALLOCPV (type_scl, p - name);
p = name;
q = s_class->scl_name;
while (*q++ = *p++)
    ;
s_class->scl_flags = compute_access (tdbb, s_class, NULL_PTR,
					NULL_PTR, NULL_PTR);

if (s_class->scl_flags & SCL_exists)
    {
    s_class->scl_next = attachment->att_security_classes;
    attachment->att_security_classes = s_class;
    return s_class;
    }

ALL_release (s_class);

return NULL;
}

int SCL_get_mask (
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	S C L _ g e t _ m a s k
 *
 **************************************
 *
 * Functional description
 *	Get a protection mask for a named object.  If field and
 *	relation names are present, get access to field.  If just
 *	relation name, get access to relation.  If neither, get
 *	access for database.
 *
 **************************************/
TDBB	tdbb;
REL	relation;
FLD	field;
SSHORT	id;
USHORT	access;
SCL	s_class;
ATT	attachment;

tdbb = GET_THREAD_DATA;

attachment = tdbb->tdbb_attachment;

/* Start with database security class */

access = (s_class = attachment->att_security_class) ? s_class->scl_flags : -1;

/* If there's a relation, track it down */

if (relation_name &&
    (relation = MET_lookup_relation (tdbb, relation_name)))
    {
    MET_scan_relation (tdbb, relation);
    if (s_class = SCL_get_class (relation->rel_security_name))
	access &= s_class->scl_flags;
    if (field_name &&
	(id = MET_lookup_field (tdbb, relation, field_name)) >= 0 &&
	(field = MET_get_field (relation, id)) &&
	(s_class = SCL_get_class (field->fld_security_name)))
	access &= s_class->scl_flags;
    }

return access & (SCL_read | SCL_write | SCL_delete | SCL_control | SCL_grant |
		 SCL_sql_insert | SCL_sql_update | SCL_sql_delete |
		 SCL_protect | SCL_sql_references | SCL_execute);
}

void SCL_init (
    BOOLEAN	create,
    TEXT	*sys_user_name,
    TEXT	*user_name,
    TEXT	*password,
    TEXT	*password_enc,
    TEXT	*sql_role,
    TDBB	tdbb)
{
/**************************************
 *
 *	S C L _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Check database access control list.
 *
 *	Checks the userinfo database to get the
 *	password and other stuff about the specified
 *	user.   Compares the password to that passed
 *	in, encrypting if necessary.
 *
 **************************************/
DBB	dbb;
BLK	handle, handle1;
VOLATILE BLK    request;
USR	user;
TEXT	name [129], project [33], organization [33], *p;
USHORT	length;
int	id, group, wheel, node_id;
TEXT	locksmith_password[20];
TEXT	locksmith_password_enc[33];
TEXT	user_locksmith[20];
TEXT	role_name[33], login_name [129], *q;
USHORT  major_version, minor_original;

SET_TDBB (tdbb);
dbb =  tdbb->tdbb_database;
major_version  = (SSHORT) dbb->dbb_ods_version;
minor_original = (SSHORT) dbb->dbb_minor_original;

*project = *organization = *name = *role_name = *login_name = '\0';

node_id = 0;
if (!user_name)
    wheel = ISC_get_user (name, &id, &group, project, organization, &node_id, sys_user_name);
else
    wheel = 0;

#ifdef APOLLO
if (user_name || !project[0])
#else
if (user_name || (id == -1))
#endif
    {
    /* This is under #ifdef temporarily until it can be fixed for all servers */

    if ((user_name == NULL) || ((password_enc == NULL) && (password == NULL)))
	ERR_post (gds__login, 0);

    strcpy (locksmith_password, LOCKSMITH_PASSWORD);
    strcpy (user_locksmith, LOCKSMITH_USER);
    strcpy (locksmith_password_enc, ENC_crypt (locksmith_password, PASSWORD_SALT));
    if (strcmp (user_name, user_locksmith) || (password_enc == NULL) ||
	strcmp (password_enc, locksmith_password_enc + 2))
	PWD_verify_user (name, user_name, password, password_enc, &id, 
			 &group, &node_id);

    /* if the name from the user database is defined as SYSDBA,
       we define that user id as having system privileges */

    if (!strcmp (name, SYSDBA_USER_NAME))
	wheel = 1;
    }

/* In case we became WHEEL on an OS that didn't require name SYSDBA,
 * (Like Unix) force the effective Database User name to be SYSDBA.
 */
if (wheel)
    strcpy (name, SYSDBA_USER_NAME);

/***************************************************************
**
** skip reading system relation RDB$ROLES when attaching pre ODS_9_0 database
**
****************************************************************/

if (ENCODE_ODS(major_version, minor_original) >= ODS_9_0)
{
if (strlen (name) != 0)
    {
    for (p = login_name, q = name; *p++ = UPPER7 (*q); q++);

    if (!create)
        {
        request = (BLK) CMP_find_request (tdbb, irq_get_role_name, IRQ_REQUESTS);
        
        FOR (REQUEST_HANDLE request) X IN RDB$ROLES
            WITH X.RDB$ROLE_NAME EQ login_name

            if (!REQUEST (irq_get_role_name))
                REQUEST (irq_get_role_name) = request;

	    EXE_unwind (tdbb, request);
            ERR_post (isc_login_same_as_role_name, 
                      gds_arg_string, ERR_cstring (login_name), 0);

        END_FOR;

        if (!REQUEST (irq_get_role_name))
            REQUEST (irq_get_role_name) = request;
        }
    }
}

if (sql_role != NULL)
    {
    strcpy (role_name, sql_role);
    }
else
    if (strlen (name) != 0)
        strcpy (role_name, "NONE");

length = strlen (name) + strlen (role_name) + strlen (project) +
         strlen (organization) + 4; /* for the terminating nulls */
tdbb->tdbb_attachment->att_user = user = (USR) ALLOCPV (type_usr, length);
p = user->usr_data;
user->usr_user_name = save_string (name, &p);
user->usr_project_name = save_string (project, &p);
user->usr_org_name = save_string (organization, &p);
user->usr_sql_role_name = save_string (role_name, &p);
user->usr_user_id = id;
user->usr_group_id = group;
user->usr_node_id = node_id;
if (wheel)
    user->usr_flags |= USR_locksmith;

#ifdef mpexl
/* Convert all periods in the user name to underscores */

if (p = user->usr_user_name)
    for (; *p; p++)
	if (*p == '.')
	    *p = '_';
#endif

handle = handle1 = NULL;

if (!create)
    {
    FOR (REQUEST_HANDLE handle) X IN RDB$DATABASE
        if (!X.RDB$SECURITY_CLASS.NULL)
           tdbb->tdbb_attachment->att_security_class = SCL_get_class (X.RDB$SECURITY_CLASS);
    END_FOR;
    CMP_release (tdbb, handle);
    
    FOR (REQUEST_HANDLE handle1)
    	FIRST 1 REL IN RDB$RELATIONS WITH REL.RDB$RELATION_NAME EQ "RDB$DATABASE"
	if (!REL.RDB$OWNER_NAME.NULL &&
	    (p = user->usr_user_name) && *p)
	    {
	    *name = strlen (p);
	    strcpy (name + 1, p);
	    if (!check_string (name, REL.RDB$OWNER_NAME))
	    	user->usr_flags |= USR_owner;
	    }
    END_FOR;
    CMP_release (tdbb, handle1);
    }
else
    user->usr_flags |= USR_owner;
    
}

void SCL_move_priv (
    UCHAR	**acl_ptr,
    USHORT	mask,
    STR *       start_ptr,
    ULONG *     length_ptr)
{
/**************************************
 *
 *	S C L _ m o v e _ p r i v
 *
 **************************************
 *
 * Functional description
 *	Given a mask of privileges, move privileges types to acl.
 *
 **************************************/
UCHAR	*p;
P_NAMES	*priv;

p = *acl_ptr;

/* terminate identification criteria, and move privileges */

CHECK_AND_MOVE (p, ACL_end, *start_ptr, length_ptr);
CHECK_AND_MOVE (p, ACL_priv_list, *start_ptr, length_ptr);

for (priv = p_names; priv->p_names_priv; priv++)
    if (mask & priv->p_names_priv)
    CHECK_AND_MOVE (p, priv->p_names_acl, *start_ptr, length_ptr);

CHECK_AND_MOVE (p, 0, *start_ptr, length_ptr);

*acl_ptr = p;
}

SCL SCL_recompute_class (
    TDBB	tdbb,
    TEXT	*string)
{
/**************************************
 *
 *	S C L _ r e c o m p u t e _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Something changed with a security class, recompute it.  If we
 *	can't find it, return NULL.
 *
 **************************************/
SCL	s_class;

SET_TDBB (tdbb);

if (!(s_class = SCL_get_class (string)))
    return NULL;

s_class->scl_flags = compute_access (tdbb, s_class,
					NULL_PTR, NULL_PTR, NULL_PTR);

if (s_class->scl_flags & SCL_exists)
    return s_class;

/* Class no long exists -- get rid of it! */

SCL_release (s_class);

return NULL;
}

void SCL_release (
    SCL		s_class)
{
/**************************************
 *
 *	S C L _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Release an unneeded and unloved security class.
 *
 **************************************/
TDBB	tdbb;
SCL	*next;
ATT	attachment;

tdbb = GET_THREAD_DATA;

attachment = tdbb->tdbb_attachment;

for (next = &attachment->att_security_classes; *next; next = &(*next)->scl_next)
    if (*next == s_class)
	{
	*next = s_class->scl_next;
	break;
	}

ALL_release (s_class);
}

static BOOLEAN check_hex (
    TEXT	*acl,
    USHORT	number)
{
/**************************************
 *
 *	c h e c k _ h e x
 *
 **************************************
 *
 * Functional description
 *	Check a string against and acl numeric string.  If they don't match,
 *	return TRUE.
 *
 **************************************/
USHORT	l;
TEXT	c;
int	n;

n = 0;
if (l = *acl++)
    do {
	c = *acl++;
	n *= 10;
	if (c >= '0' && c <= '9')
	    n += c - '0';
	else if (c >= 'a' && c <= 'f')
	    n += c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    n += c - 'A' + 10;
    } while (--l);

return (n != number);
}

static BOOLEAN check_number (
    TEXT	*acl,
    USHORT	number)
{
/**************************************
 *
 *	c h e c k _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *	Check a string against and acl numeric string.  If they don't match,
 *	return TRUE.
 *
 **************************************/
USHORT	l;
int	n;

n = 0;
if (l = *acl++)
    do n = n * UIC_BASE + *acl++ - '0'; while (--l);

return (n != number);
}

static BOOLEAN check_user_group (
    TEXT	*acl,
    USHORT	number,
    STR  *      start_ptr,
    ULONG *     length_ptr)
{
/**************************************
 *
 *	c h e c k _ u s e r _ g r o u p
 *
 **************************************
 *
 * Functional description
 *
 *	Check a string against an acl numeric string.  
 *
 * logic:
 *
 *  If the string contains user group name, 
 *    then 
 *      converts user group name to numeric user group id.
 *    else
 *      converts character user group id to numeric user group id.
 *
 *	Check numeric user group id against an acl numeric string.  
 *  If they don't match, return TRUE.
 *
 **************************************/
USHORT	l;
SLONG	n;
TEXT	*user_group_name;
TEXT	one_char;
STR	buffer;
TDBB	tdbb;
DBB	dbb;
JMP_BUF env, *old_env;


tdbb = GET_THREAD_DATA;
dbb =  tdbb->tdbb_database;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (buffer)
         ALL_release (buffer);
    LONGJMP (tdbb->tdbb_setjmp, (int) tdbb->tdbb_status_vector [1]);
    }


buffer  = (STR) ALLOCPV (type_str, *length_ptr);

	n = 0;
	if (l = *acl++)
	    {
	    if ( isdigit (*acl) ) /* this is a group id */
	        {
	        do n = n * UIC_BASE + *acl++ - '0'; while (--l);
	        }
	    else	/* processing group name */
	        {
	        user_group_name = buffer->str_data;
	        do  { 
	            one_char = *acl++;
	            *user_group_name++ = LOWWER (one_char);
	            } while (--l);
	        *user_group_name = '\0';
	        user_group_name = buffer->str_data;
	        /*
	        ** convert unix group name to unix group id
	        */
	        n = ISC_get_user_group_id (user_group_name);
	        }
	    }

        ALL_release (buffer);
        tdbb->tdbb_setjmp = (UCHAR*) old_env;
	return (n != number);
}

static BOOLEAN check_string (
    TEXT	*acl,
    TEXT	*string)
{
/**************************************
 *
 *	c h e c k _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Check a string against and acl string.  If they don't match,
 *	return TRUE.
 *
 **************************************/
USHORT	l;
TEXT	c1, c2;


/* 
 * add these asserts to catch calls to this function with NULL,
 * the caller to this function must check to ensure that the arguments are not
 * NULL  - Shaunak Mistry 03-May-99.
 */

assert (string != NULL);
assert (acl != NULL);

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

static SLONG compute_access (
    TDBB	tdbb,
    SCL		s_class,
    REL		view,
    TEXT	*trg_name,
    TEXT	*prc_name)
{
/**************************************
 *
 *	c o m p u t e _ a c c e s s
 *
 **************************************
 *
 * Functional description
 *	Compute access for security class.  If a relation block is
 *	present, it is a view, and we should check for enhanced view
 *	access permissions.  Return a flag word of recognized privileges.
 *
 **************************************/
DBB	dbb;
BLK	request;
BLB	blob;
SLONG	privileges;
TEXT	*acl;
TEXT	*buffer;
VOLATILE STR     str_buffer = NULL;
JMP_BUF env, *old_env;
SLONG length = BLOB_BUFFER_SIZE, *length_ptr = &length;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

privileges = SCL_scanned;

old_env = (JMP_BUF*) tdbb->tdbb_setjmp;
tdbb->tdbb_setjmp = (UCHAR*) env;
if (SETJMP (env))
    {
    tdbb->tdbb_setjmp = (UCHAR*) old_env;
    if (str_buffer)
         ALL_release (str_buffer);
    LONGJMP (tdbb->tdbb_setjmp, (int) tdbb->tdbb_status_vector [1]);
    }

/* Get some space that's not off the stack */
str_buffer = (STR) ALLOCPV (type_str, BLOB_BUFFER_SIZE);
buffer = str_buffer->str_data;


request = (BLK) CMP_find_request (tdbb, irq_l_security, IRQ_REQUESTS);

FOR (REQUEST_HANDLE request) X IN RDB$SECURITY_CLASSES 
	WITH X.RDB$SECURITY_CLASS EQ s_class->scl_name
    if (!REQUEST (irq_l_security))
	REQUEST (irq_l_security) = request;
    privileges |= SCL_exists;
    blob = BLB_open (tdbb, dbb->dbb_sys_trans, &X.RDB$ACL);
    acl = buffer;
    while (TRUE)
	{
	acl += BLB_get_segment (tdbb, blob, acl, 
                (length - ((acl - buffer) * (sizeof(buffer[0])))));
	if (blob->blb_flags & BLB_eof)
	    break;
        /* there was not enough space, realloc point acl to the correct location */
        if (blob->blb_fragment_size)
            {
	    ULONG old_offset = (ULONG) (acl - buffer);
            length += BLOB_BUFFER_SIZE;
            (void) ALL_extend (&str_buffer, length);
            buffer = str_buffer->str_data;
	    acl = buffer + old_offset;
            }
	}
    BLB_close (tdbb, blob);
    if (acl != buffer)
	privileges |= walk_acl (tdbb, buffer, view, trg_name, prc_name, &str_buffer, length_ptr);
END_FOR;
    
if (!REQUEST (irq_l_security))
    REQUEST (irq_l_security) = request;


ALL_release (str_buffer);

tdbb->tdbb_setjmp = (UCHAR*) old_env;
return privileges;
}

static TEXT *save_string (
    TEXT	*string,
    TEXT	**ptr)
{
/**************************************
 *
 *	s a v e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	If a string is non-null, copy it to a work area and return a
 *	pointer.
 *
 **************************************/
TEXT	*p, *start;

if (!*string)
    return NULL;

start = p = *ptr;

while (*p++ = *string++)
    ;

*ptr = p;

return start;
}

static SLONG walk_acl (
    TDBB	tdbb,
    TEXT	*acl,
    REL		view,
    TEXT	*trg_name,
    TEXT	*prc_name,
    STR         *start_ptr,
    ULONG       *length_ptr)
{
/**************************************
 *
 *	w a l k _ a c l
 *
 **************************************
 *
 * Functional description
 *	Walk an access control list looking for a hit.  If a hit
 *	is found, return privileges.
 *
 **************************************/
struct usr	user;
SLONG		privilege; 
USHORT		hit;
TEXT		c, *p, *role_name;
BOOLEAN		is_member = FALSE, equivalent_proc_nm;
VOLATILE BLK    request;
DBB		dbb;

SET_TDBB (tdbb);
dbb = tdbb->tdbb_database;

/* Munch ACL.  If we find a hit, eat up privileges */

user = *tdbb->tdbb_attachment->att_user;
role_name = user.usr_sql_role_name;

if (view && (view->rel_flags & REL_sql_relation))
    {
    /* Use the owner of the view to perform the sql security
       checks with: (1) The view user must have sufficient privileges
       to the view, and (2a) the view owner must have sufficient 
       privileges to the base table or (2b) the view must have
       sufficient privileges on the base table. */
    user.usr_user_name = view->rel_owner_name;
    }
privilege = 0;

if (*acl++ != ACL_version)
    BUGCHECK (160); /* msg 160 wrong ACL version */

if (user.usr_flags & USR_locksmith)
    return -1 & ~SCL_corrupt;

while (c = *acl++)
    switch (c)
	{
	case ACL_id_list:
	    hit = TRUE;
	    while (c = *acl++)
		{
		switch (c)
		    {
		    case id_person:
			if (!(p = user.usr_user_name) ||
			    check_string (acl, p))
			    hit = FALSE;
			break;

		    case id_project:
			if (!(p = user.usr_project_name) ||
			    check_string (acl, p))
			    hit = FALSE;
			break;

		    case id_organization:
			if (!(p = user.usr_org_name) || check_string (acl, p))
			    hit = FALSE;
			break;

		    case id_group:
			if (check_user_group (acl, user.usr_group_id, start_ptr, length_ptr))
			    hit = FALSE;
			break;

		    case id_sql_role:
		        if (role_name  == NULL || check_string ( acl, role_name ))
		            hit = FALSE;
		        else
		            {
		            TEXT   login_name [129], *p, *q;
		            for (p = login_name, q = user.usr_user_name;
		                *p++ = UPPER7 (*q); q++);
		            hit = FALSE;
		            request = (BLK) CMP_find_request (tdbb, irq_get_role_mem,
		                                                  IRQ_REQUESTS);

		            FOR (REQUEST_HANDLE request) U IN RDB$USER_PRIVILEGES WITH
		                (U.RDB$USER         EQ login_name             OR
		                 U.RDB$USER         EQ "PUBLIC")              AND
		                U.RDB$USER_TYPE     EQ obj_user               AND
		                U.RDB$RELATION_NAME EQ user.usr_sql_role_name AND
		                U.RDB$OBJECT_TYPE   EQ obj_sql_role           AND
		                U.RDB$PRIVILEGE     EQ "M"

		                if (!REQUEST (irq_get_role_mem))
		                    REQUEST (irq_get_role_mem) = request;

		                if (!U.RDB$USER.NULL)
		                    hit = TRUE;
		            END_FOR;

		            if (!REQUEST (irq_get_role_mem))
		                REQUEST (irq_get_role_mem) = request;
		            }
			break;

		    case id_view:
			if (!view || check_string (acl, view->rel_name))
			    hit = FALSE;
			break;

		    case id_procedure:
			if (!prc_name || check_string (acl, prc_name))
			    hit = FALSE;
			break;

		    case id_trigger:
			if (!trg_name || check_string (acl, trg_name))
			    hit = FALSE;
			break;

		    case id_views:
			/* Disable this catch-all that messes up the view security.
			   Note that this id_views is not generated anymore, this code
			   is only here for compatibility.  id_views was only 
			   generated for SQL. */
			hit = FALSE;
			if (!view)
			    hit = FALSE;
			break;

		    case id_user:
			if (check_number (acl, user.usr_user_id))
			    hit = FALSE;
			break;

		    case id_node:
			if (check_hex (acl, user.usr_node_id))
			    hit = FALSE;
			break;
		    
		    default:
			return SCL_corrupt;
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
			    /* note that READ access must imply REFERENCES
			       access for upward compatibility of existing
			       security classes */

			    privilege |= SCL_read | SCL_sql_references;
			    break;

			case priv_write:
			    privilege |= SCL_write | SCL_sql_insert | SCL_sql_update | SCL_sql_delete;
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
			    return SCL_corrupt;
			}
		/* For a relation the first hit does not give the privilege. 
		   Because, there could be some permissions for the table 
		   (for user1) and some permissions for a column on that 
		   table for public/user2, causing two hits.
                   Hence, we do not return at this point.
		   -- Madhukar Thakur (May 1, 1995)
		*/
		}
	    else
	        while (*acl++)
		    ;
	    break;

	default:
	    return SCL_corrupt;
	}

return privilege;
}
