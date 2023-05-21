/*
 *
 *	PROGRAM:	Security data base manager
 *	MODULE:		security.e
 *	DESCRIPTION:	Security routines
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
#include <stdlib.h>
#include <string.h>
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/pwd.h"
#include "../jrd/enc_proto.h"
#include "../jrd/gds_proto.h"
#include "gsec.h"
#include "secur_proto.h"

DATABASE
    DB = STATIC FILENAME "isc.gdb";

#define MAX_PASSWORD_LENGTH	31

SSHORT SECURITY_exec_line (
    STATUS	*isc_status,
    void	*DB,
    USER_DATA	user_data,
    void	(* display_func) (void *, USER_DATA, BOOLEAN),
    void	*callback_arg)
{
/*************************************
 *
 *	S E C U R I T Y _ e x e c _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Process a command line for the security data base manager.
 *	This is used to add and delete users from the user information
 *	database (isc.gdb).   It also displays information
 *	about current users and allows modification of current
 *	users' parameters.   
 *	Returns 0 on success, otherwise returns a Gsec message number
 *	and the status vector containing the error info.
 *	The syntax is:
 *
 *	Adding a new user:
 *
 *	    gsec -add <name> [ <parameter> ... ]    -- command line
 *	    add <name> [ <parameter> ... ]          -- interactive
 *
 *	Deleting a current user:
 *
 *	    gsec -delete <name>     -- command line
 *	    delete <name>           -- interactive
 *
 *	Displaying all current users:
 *
 *	    gsec -display           -- command line
 *	    display                 -- interactive
 *
 *	Displaying one user:
 *
 *	    gsec -display <name>    -- command line
 *	    display <name>          -- interactive
 *
 *	Modifying a user's parameters:
 *
 *	    gsec -modify <name> <parameter> [ <parameter> ... ] -- command line
 *	    modify <name> <parameter> [ <parameter> ... ]       -- interactive
 *
 *	Get help:
 *
 *	    gsec -help              -- command line
 *	    ?                       -- interactive
 *	    help                    -- interactive
 *
 *	Quit interactive session:
 *
 *	    quit                    -- interactive
 *
 *	where <parameter> can be one of:
 *
 *	    -uid <uid>
 *	    -gid <gid>
 *	    -fname <firstname>
 *	    -mname <middlename>
 *	    -lname <lastname>
 *
 **************************************/
SCHAR		encrypted1 [MAX_PASSWORD_LENGTH+2];
SCHAR		encrypted2 [MAX_PASSWORD_LENGTH+2];
TEXT		buff [MSG_LENGTH];
STATUS		tmp_status [20];
void		*gds__trans;
BOOLEAN		found;
SSHORT		ret = 0;

gds__trans = NULL;
START_TRANSACTION
ON_ERROR
    return GsecMsg75;	/* gsec error */
END_ERROR;

switch (user_data->operation)
    {
    case ADD_OPER:
	/* this checks the "entered" flags for each parameter (except the name)
	   and makes all non-entered parameters null valued */

	STORE U IN USERS USING
	    strcpy (U.USER_NAME, user_data->user_name);
	    if (user_data->uid_entered)
		{
		U.UID = user_data->uid;
		U.UID.NULL = ISC_FALSE;
		}
	    else
		U.UID.NULL = ISC_TRUE;
	    if (user_data->gid_entered)
		{
		U.GID = user_data->gid;
		U.GID.NULL = ISC_FALSE;
		}
	    else
		U.GID.NULL = ISC_TRUE;
	    if (user_data->group_name_entered)
		{
		strcpy (U.GROUP_NAME, user_data->group_name);
		U.GROUP_NAME.NULL = ISC_FALSE;
		}
	    else
		U.GROUP_NAME.NULL = ISC_TRUE;
	    if (user_data->password_entered)
		{
		strcpy (encrypted1,
		    ENC_crypt (user_data->password, PASSWORD_SALT));
		strcpy (encrypted2, ENC_crypt (&encrypted1 [2], PASSWORD_SALT));
		strcpy (U.PASSWD, &encrypted2 [2]);
		U.PASSWD.NULL = ISC_FALSE;
		}
	    else
		U.PASSWD.NULL = ISC_TRUE;
	    if (user_data->first_name_entered)
		{
		strcpy (U.FIRST_NAME, user_data->first_name);
		U.FIRST_NAME.NULL = ISC_FALSE;
		}
	    else
		U.FIRST_NAME.NULL = ISC_TRUE;
	    if (user_data->middle_name_entered)
		{
		strcpy (U.MIDDLE_NAME, user_data->middle_name);
		U.MIDDLE_NAME.NULL = ISC_FALSE;
		}
	    else
		U.MIDDLE_NAME.NULL = ISC_TRUE;
	    if (user_data->last_name_entered)
		{
		strcpy (U.LAST_NAME, user_data->last_name);
		U.LAST_NAME.NULL = ISC_FALSE;
		}
	    else
		U.LAST_NAME.NULL = ISC_TRUE;
	END_STORE
	ON_ERROR
	    ret = GsecMsg19;	/* gsec - add record error */
	END_ERROR;
	break;

    case MOD_OPER:
	/* this updates an existing record, replacing all fields that are
	   entered, and for those that were specified but not entered, it
	   changes the current value to the null value */

	found = FALSE;
	FOR U IN USERS WITH U.USER_NAME EQ user_data->user_name
	    found = TRUE;
	    MODIFY U USING
		if (user_data->uid_entered)
		    {
		    U.UID = user_data->uid;
		    U.UID.NULL = ISC_FALSE;
		    }
		else if (user_data->uid_specified)
		    U.UID.NULL = ISC_TRUE;
		if (user_data->gid_entered)
		    {
		    U.GID = user_data->gid;
		    U.GID.NULL = ISC_FALSE;
		    }
		else if (user_data->gid_specified)
		    U.GID.NULL = ISC_TRUE;
		if (user_data->group_name_entered)
		    {
		    strcpy (U.GROUP_NAME, user_data->group_name);
		    U.GROUP_NAME.NULL = ISC_FALSE;
		    }
		else if (user_data->group_name_specified)
		    U.GROUP_NAME.NULL = ISC_TRUE;
		if (user_data->password_entered)
		    {
		    strcpy (encrypted1,
			ENC_crypt (user_data->password, PASSWORD_SALT));
		    strcpy (encrypted2,
			ENC_crypt (&encrypted1 [2], PASSWORD_SALT));
		    strcpy (U.PASSWD, &encrypted2 [2]);
		    U.PASSWD.NULL = ISC_FALSE;
		    }
		else if (user_data->password_specified)
		    U.PASSWD.NULL = ISC_TRUE;
		if (user_data->first_name_entered)
		    {
		    strcpy (U.FIRST_NAME, user_data->first_name);
		    U.FIRST_NAME.NULL = ISC_FALSE;
		    }
		else if (user_data->first_name_specified)
		    U.FIRST_NAME.NULL = ISC_TRUE;
		if (user_data->middle_name_entered)
		    {
		    strcpy (U.MIDDLE_NAME, user_data->middle_name);
		    U.MIDDLE_NAME.NULL = ISC_FALSE;
		    }
		else if (user_data->middle_name_specified)
		    U.MIDDLE_NAME.NULL = ISC_TRUE;
		if (user_data->last_name_entered)
		    {
		    strcpy (U.LAST_NAME, user_data->last_name);
		    U.LAST_NAME.NULL = ISC_FALSE;
		    }
		else if (user_data->last_name_specified)
		    U.LAST_NAME.NULL = ISC_TRUE;
	    END_MODIFY
	    ON_ERROR
		ret = GsecMsg20;
	    END_ERROR;
	END_FOR
	ON_ERROR
	    ret = GsecMsg21;
	END_ERROR;
	if (!ret && !found)
	    ret = GsecMsg22;
	break;

    case DEL_OPER:
	/* looks up the specified user record and deletes it */

	found = FALSE;
	FOR U IN USERS WITH U.USER_NAME EQ user_data->user_name
	    found = TRUE;
	    ERASE U
	    ON_ERROR
		ret = GsecMsg23;	/* gsec - delete record error */
	    END_ERROR;
	END_FOR
	ON_ERROR
	    ret = GsecMsg24;	/* gsec - find/delete record error */
	END_ERROR;

	if (!ret && !found)
	    ret = GsecMsg22;	/* gsec - record not found for user: */
	break;

    case DIS_OPER:
	/* gets either the desired record, or all records, and displays them */

	found = FALSE;
	if (!user_data->user_name_entered)
	    {
	    FOR U IN USERS
		user_data->uid = U.UID;
		user_data->gid = U.GID;
		*(user_data->sys_user_name) = '\0';
		strcpy (user_data->user_name, U.USER_NAME);
		strcpy (user_data->group_name, U.GROUP_NAME);
		strcpy (user_data->password, U.PASSWD);
		strcpy (user_data->first_name, U.FIRST_NAME);
		strcpy (user_data->middle_name, U.MIDDLE_NAME);
		strcpy (user_data->last_name, U.LAST_NAME);
		display_func (callback_arg, user_data, !found);

		found = TRUE;
	    END_FOR
	    ON_ERROR
		ret = GsecMsg28;	/* gsec - find/display record error */
	    END_ERROR;
	    }
	else
	    {
	    FOR U IN USERS WITH U.USER_NAME EQ user_data->user_name
		user_data->uid = U.UID;
		user_data->gid = U.GID;
		*(user_data->sys_user_name) = '\0';
		strcpy (user_data->user_name, U.USER_NAME);
		strcpy (user_data->group_name, U.GROUP_NAME);
		strcpy (user_data->password, U.PASSWD);
		strcpy (user_data->first_name, U.FIRST_NAME);
		strcpy (user_data->middle_name, U.MIDDLE_NAME);
		strcpy (user_data->last_name, U.LAST_NAME);
		display_func (callback_arg, user_data, !found);

		found = TRUE;
	    END_FOR
	    ON_ERROR
		ret = GsecMsg28;	/* gsec - find/display record error */
	    END_ERROR;
	    }
	break;

    default: 
	ret = GsecMsg16;	/* gsec - error in switch specifications */
	break;
    }

/* rollback if we have an error using tmp_status to avoid 
   overwriting the error status which the caller wants to see */

if (ret)
    isc_rollback_transaction (tmp_status, &gds__trans);
else
    {
    COMMIT
    ON_ERROR
	return GsecMsg75;	/* gsec error */
    END_ERROR;
    }

return ret;
}

void SECURITY_get_db_path (
    TEXT	*server,
    TEXT	*buffer)
{
/**************************************
 *
 *	S E C U R I T Y _ g e t _ d b _ p a t h
 *
 **************************************
 *
 * Functional description
 *	Gets the path to the security database
 *	from server	
 *
 *  NOTE:  Be sure that server specifies the 
 *         protocol being used to connect.
 *
 *         for example:  
 *		server:
 *         would connect via tcpip
 *
 **************************************/
#define RESPBUF	256
STATUS		status [20];
isc_svc_handle	svc_handle = NULL;
TEXT		svc_name[256];
TEXT		sendbuf[] = { isc_info_svc_user_dbpath };
TEXT		respbuf[RESPBUF], *p = respbuf;
USHORT		path_length;

/* Whatever is defined for a given platform as a name for
   the security database is used as a default in case we
   are unable to get the path from server
*/
strcpy (buffer, USER_INFO_NAME);

if (server)
    sprintf (svc_name, "%sanonymous", server);
else
#ifdef WIN_NT
    sprintf (svc_name, "anonymous");
#else
    sprintf (svc_name, "localhost:anonymous");
#endif

if (isc_service_attach (status, 0, svc_name, &svc_handle, 0, NULL))
    return;

if (isc_service_query (status, &svc_handle, NULL, 0, NULL, 1, sendbuf, RESPBUF, respbuf))
    {
    isc_service_detach (status, &svc_handle);
    return;
    }

if (*p == isc_info_svc_user_dbpath)
    {
    p++;
    path_length = (USHORT) isc_vax_integer (p, sizeof(short));
    p += sizeof (USHORT);
    strncpy (buffer, p, path_length);
    buffer[path_length] = '\0';
    }

isc_service_detach (status, &svc_handle);
}

void SECURITY_msg_get (
    USHORT	number,
    TEXT	*msg)
{
/**************************************
 *
 *	S E C U R I T Y _ m s g _ g e t
 *
 **************************************
 *
 * Functional description
 *	Retrieve a message from the error file
 *
 **************************************/

gds__msg_format (NULL_PTR, GSEC_MSG_FAC, number, MSG_LENGTH, msg,
    NULL, NULL, NULL, NULL, NULL);
}
