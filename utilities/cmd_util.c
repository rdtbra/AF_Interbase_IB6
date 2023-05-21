/*
 *	PROGRAM:	Command line utilities
 *	MODULE:		cmd_util.c
 *	DESCRIPTION:	Common functions
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
#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/msg_encode.h"
#include "../jrd/iberr.h"

#ifdef WIN_NT
#include <io.h>
#include <windows.h>
#include "../jrd/pwd.h" 
#define TEXT		SCHAR
#endif

typedef struct blk {
    UCHAR       blk_type;
    UCHAR       blk_pool_id_mod;
    USHORT      blk_length;
} *BLK;

#include "../jrd/svc.h"
#include "../jrd/svc_proto.h"

void CMD_UTIL_put_svc_status (
    STATUS      *svc_status,
    USHORT      facility,
    USHORT      errcode,
    USHORT      arg1_t,
    void        *arg1,
    USHORT      arg2_t,
    void        *arg2,
    USHORT      arg3_t,
    void        *arg3,
    USHORT      arg4_t,
    void        *arg4,
    USHORT      arg5_t,
    void        *arg5)
{
/**************************************
 *
 *      C M D _ U T I L _ p u t _ s v c _ s t a t u s
 *
 **************************************
 *
 * Functional description
 *	Append error codes to the status vector
 *
 **************************************/
STATUS  *status;
STATUS  tmp_status[ISC_STATUS_LENGTH], warning_status[ISC_STATUS_LENGTH];
int     i, tmp_status_len = 0, status_len = 0, err_status_len = 0;
int     warning_count = 0, warning_indx = 0;
BOOLEAN duplicate = FALSE;

/* stuff the status into temp buffer */

MOVE_CLEAR(tmp_status, sizeof (tmp_status));
status = tmp_status;
*status++ = isc_arg_gds;
*status++ = ENCODE_ISC_MSG(errcode, facility);
tmp_status_len = 3;

if (arg1)
    {
    SVC_STATUS_ARG (arg1_t, arg1);
    tmp_status_len += 2;
    }
if (arg2)
    {
    SVC_STATUS_ARG (arg2_t, arg2);
    tmp_status_len += 2;
    }
if (arg3)
    {
    SVC_STATUS_ARG (arg3_t, arg3);
    tmp_status_len += 2;
    }
if (arg4)
    {
    SVC_STATUS_ARG (arg4_t, arg4);
    tmp_status_len += 2;
    }
if (arg5)
    {
    SVC_STATUS_ARG (arg5_t, arg5);
    tmp_status_len += 2;
    }

*status++ = isc_arg_end;

if (svc_status[0] != gds_arg_gds ||
    (svc_status[0] == gds_arg_gds && svc_status[1] == 0 &&
     svc_status[2] != isc_arg_warning))
    {
    MOVE_FASTER(tmp_status, svc_status, sizeof (STATUS) * tmp_status_len);
    }
else
    {
    PARSE_STATUS(svc_status, status_len, warning_indx);
    if (status_len)
	--status_len;

    /* check for duplicated error code */
    for (i = 0; i < ISC_STATUS_LENGTH; i++)
	{
	if (svc_status[i] == gds_arg_end && i == status_len)
	    break; /* end of argument list */

	if (i && i == warning_indx)
	    break; /* vector has no more errors */

	if (svc_status[i] == tmp_status[1] && i &&
	    svc_status[i - 1] != isc_arg_warning &&
	    i + tmp_status_len - 2 < ISC_STATUS_LENGTH &&
	    (memcmp (&svc_status[i], &tmp_status[1],
		     sizeof (STATUS) * (tmp_status_len - 2)) == 0))
	    {
	    /* duplicate found */
	    duplicate = TRUE;
	    break;
	    }
	}
    if (!duplicate)
	{
	/* if the status_vector has only warnings then adjust err_status_len */
	if ((err_status_len = i) == 2 && warning_indx)
	    err_status_len = 0;

	if (warning_indx)
	    {
	    /* copy current warning(s) to a temp buffer */
	    MOVE_CLEAR(warning_status, sizeof(warning_status));
	    MOVE_FASTER(&svc_status[warning_indx], warning_status,
			sizeof (STATUS) * (ISC_STATUS_LENGTH - warning_indx));
	    PARSE_STATUS(warning_status, warning_count, warning_indx);
	    }

	/* add the status into a real buffer right in between last error
	   and first warning */
	if ((i = err_status_len + tmp_status_len) < ISC_STATUS_LENGTH)
	    {
	    MOVE_FASTER(tmp_status, &svc_status[err_status_len],
			sizeof (STATUS) * tmp_status_len);
	    /* copy current warning(s) to the status_vector */
	    if (warning_count && i + warning_count - 1 < ISC_STATUS_LENGTH)
		{
		MOVE_FASTER(warning_status, &svc_status[i - 1],
			    sizeof (STATUS) * warning_count);
		}
	    }
	} 
    }
}
