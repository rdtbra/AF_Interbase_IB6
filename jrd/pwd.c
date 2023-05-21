/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pwd.e
 *	DESCRIPTION:	User information database access
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
#include "../jrd/jrd.h"
#include "../jrd/pwd.h"
#include "../jrd/enc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/pwd_proto.h"
#include "../jrd/sch_proto.h"
#ifdef  WINDOWS_ONLY
#include "../jrd/jrd_proto.h"
#include "../jrd/thd_proto.h"
#endif

/* blr to search database for user name record */

static CONST char     pwd_request[] = {
      blr_version5,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_long, 0, 
	    blr_long, 0, 
	    blr_short, 0, 
	    blr_text, 34,0,
	 blr_message, 0, 1,0, 
	    blr_cstring, 129,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 5, 'U','S','E','R','S', 0, 
		     blr_first, 
			blr_literal, blr_short, 0, 1,0,
		     blr_boolean, 
			blr_eql, 
			   blr_field, 0, 9, 'U','S','E','R','_','N','A','M','E', 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_field, 0, 3, 'G','I','D', 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_field, 0, 3, 'U','I','D', 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_literal, blr_short, 0, 1,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_field, 0, 6, 'P','A','S','S','W','D', 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_short, 0, 0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };

typedef struct
{
    SLONG       gid;
    SLONG       uid;
    SSHORT      flag;
    SCHAR       password[34];
} user_record;

static CONST char     tpb[4] = { isc_tpb_version1,
			   isc_tpb_write,
			   isc_tpb_concurrency,
			   isc_tpb_wait };

static BOOLEAN  lookup_user (TEXT *, int *, int *, TEXT *);
static BOOLEAN  open_user_db (SLONG **, SLONG *);

/* kludge to make sure pwd sits below why.c on the PC (Win16) platform */

#ifdef  WINDOWS_ONLY

#define isc_attach_database(a,b,c,d,e,f) jrd8_attach_database(a,b,c,d,e,f,(SCHAR*)expanded_name)

#define isc_compile_request             jrd8_compile_request
#define isc_detach_database             jrd8_detach_database
#define isc_receive                     jrd8_receive
#define isc_rollback_transaction        jrd8_rollback_transaction
#define isc_start_and_send              jrd8_start_and_send
#define isc_start_transaction           jrd8_start_transaction

#endif

void PWD_get_user_dbpath (
    TEXT        *path_buffer)
{
/**************************************
 *
 *      P W D _ g e t _ u s e r _ d b p a t h
 *
 **************************************
 *
 * Functional description:
 *      Return the path to the user database.  This
 *      Centralizes the knowledge of where the db is
 *      so gsec, lookup_user, etc. can be consistent.
 *
 **************************************/

#if (defined VMS || defined WINDOWS_ONLY || defined WIN_NT || defined LINUX || defined SUPERSERVER)
gds__prefix (path_buffer, USER_INFO_NAME);
#else
strcpy (path_buffer, USER_INFO_NAME);
#endif 
}

void PWD_verify_user (
    TEXT        *name,
    TEXT        *user_name,
    TEXT        *password,
    TEXT        *password_enc,
    int         *uid,
    int         *gid,
    int         *node_id)
{
/**************************************
 *
 *      P W D _ v e r i f y _ u s e r
 *
 **************************************
 *
 * Functional description:
 *      Verify a user in the security database.
 *
 **************************************/
TEXT    *p, *q, pw1 [33], pw2 [33], pwt [33];
BOOLEAN notfound;

if (user_name)
    {
    for (p = name, q = user_name; *q; q++, p++)
	*p = UPPER7 (*q);
    *p = 0;
    }

/* Look up the user name in the userinfo database and use the parameters
   found there.  This means that another database must be accessed, and
   that means the current context must be saved and restored. */

notfound = lookup_user (name, uid, gid, pw1);

/* Punt if the user has specified neither a raw nor an encrypted password,
   or if the user has specified both a raw and an encrypted password, 
   or if the user name specified was not in the password database
   (or if there was no password database - it's still not found) */

if ((!password && !password_enc) || (password && password_enc) || notfound)
    ERR_post (gds__login, 0);

if (password)
    {
    strcpy (pwt, ENC_crypt (password, PASSWORD_SALT));
    password_enc = pwt + 2;
    }
strcpy (pw2, ENC_crypt (password_enc, PASSWORD_SALT));
if (strncmp (pw1, pw2 + 2, 11))
    ERR_post (gds__login, 0);

*node_id = 0;
}

static BOOLEAN lookup_user (
    TEXT        *user_name,
    int         *uid,
    int         *gid,
    TEXT        *pwd)
{
/**************************************
 *
 *      l o o k u p _ u s e r
 *
 **************************************
 *
 * Functional description:
 *      Look the name up in the local userinfo
 *      database.   If it is found, fill in the
 *      user and group ids, as found in the
 *      database for that user.   Also return
 *      the encrypted password.
 *
 **************************************/
BOOLEAN         notfound;               /* user found flag */
SLONG           *uinfo;                 /* database handle */
SLONG           *lookup_trans;          /* default transaction handle */
STATUS          status [20];            /* status vector */
SLONG           *lookup_req;            /* request handle */
TEXT            uname[129];             /* user name buffer */
user_record     user;                   /* user record */
#ifdef  WINDOWS_ONLY
SLONG           i;
STATUS          *status_vector;
#endif

/* Start by clearing the output data */

if (uid)
    *uid = 0;
if (gid)
    *gid = 0;
if (pwd)
    *pwd = '\0';
notfound = TRUE;
strncpy( uname, user_name, 129);

/* get local copies of handles, so this depends on no static variables */

/* the database is opened here, but this could also be done when the
   first database is opened */

if (open_user_db (&uinfo, status))
    {
    THREAD_ENTER;
#ifdef  WINDOWS_ONLY
    
    /* hijack status vector */
    
    status_vector = ((TDBB) GET_THREAD_DATA)->tdbb_status_vector;
    for ( i = 0; i < 20; i++)
	if ( ( status_vector[i] = status[i]) == 0)
	    break;
    ERR_punt();
#else
    ERR_post (gds__psw_attach, 0);
#endif    
    }

lookup_req = NULL;
lookup_trans = NULL;

if ( isc_start_transaction (status, &lookup_trans, (short) 1, &uinfo,
			    (short) sizeof( tpb), tpb))
    {
    isc_detach_database (status, &uinfo);
    THREAD_ENTER;
    ERR_post (gds__psw_start_trans, 0);
    }

if ( !isc_compile_request (status, &uinfo, &lookup_req,
			   (short) sizeof (pwd_request), pwd_request))
    if ( !isc_start_and_send (status, &lookup_req, &lookup_trans, (short) 0,
			     (short) sizeof( uname), uname, (short) 0))
	while (1)
	    {
	    isc_receive (status, &lookup_req, (short) 1, (short) sizeof( user),
			 &user, (short) 0);
	    if (!user.flag || status [1])
		break;
	    notfound = FALSE;
	    if (uid)
		*uid = user.uid;
	    if (gid)
		*gid = user.gid;
	    if (pwd)
		strncpy (pwd, user.password, 32);
	    }

/* the database is closed and the request is released here, but this
   could be postponed until all databases are closed */

isc_rollback_transaction (status, &lookup_trans);;
isc_detach_database (status, &uinfo);;
THREAD_ENTER;

return notfound;
}

static BOOLEAN open_user_db (
    SLONG       **uihandle,
    SLONG	*status)
{
/**************************************
 *
 *      o p e n _ u s e r _ d b
 *
 **************************************
 *
 * Functional description:
 *      Open the user information database
 *      and return the handle.
 *
 *      returns 0 if successfully opened
 *              1 if not
 *
 **************************************/
TEXT    user_info_name [MAX_PATH_LENGTH];
BOOLEAN notopened;              /* open/not open flag */
SLONG   *uinfo;                 /* database handle */
SCHAR   *p, *dpb, dpb_buffer [256];
SSHORT  dpb_len;
TEXT    locksmith_password_enc [33];
#ifdef  WINDOWS_ONLY
SCHAR   *expanded_name;
#endif

/* Encrypt and copy locksmith's password under the global scheduler's
   mutex since the implementation of the encryption isn't thread-safe
   on most of our platforms. */

strcpy (locksmith_password_enc, ENC_crypt (LOCKSMITH_PASSWORD, PASSWORD_SALT));
THREAD_EXIT;

/* initialize the data base's name */

notopened = FALSE;
uinfo = NULL;

PWD_get_user_dbpath (user_info_name);
#ifdef  WINDOWS_ONLY
expanded_name = (TEXT*) gds__alloc ((SLONG)(sizeof(TEXT)* MAX_PATH_LENGTH));
ISC_expand_filename( user_info_name, 0, expanded_name);
#endif

/* Perhaps build up a dpb */

dpb = dpb_buffer;

*dpb++ = gds__dpb_version1;
*dpb++ = gds__dpb_user_name;
*dpb++ = strlen (LOCKSMITH_USER);
p = LOCKSMITH_USER;
while (*p)
    *dpb++ = *p++;

*dpb++ = gds__dpb_password_enc;
p = locksmith_password_enc + 2;
*dpb++ = strlen (p);
while (*p)
    *dpb++ = *p++;
*dpb++ = isc_dpb_sec_attach;    /* Attachment is for the security database */
*dpb++ = 1;                     /* Parameter value length */
*dpb++ = TRUE;                  /* Parameter value */

dpb_len = dpb - dpb_buffer;

isc_attach_database (status, 0, user_info_name, &uinfo, dpb_len, dpb_buffer);

if (status [1] == gds__login)
    {
    /* we may be going against a V3 database which does not
     * understand the "hello, god" combination.
     */
    
    isc_attach_database (status, 0, user_info_name, &uinfo, 0, 0);
    }

if (status [1])
    notopened = TRUE;
*uihandle = uinfo;

#ifdef  WINDOWS_ONLY
gds__free( (SLONG*)expanded_name);
#endif

return notopened;
}
