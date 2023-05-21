/*
 *	PROGRAM:	Security data base manager
 *	MODULE:		secur_proto.h
 *	DESCRIPTION:	Prototype header file for security.e
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

#ifndef _UTILITIES_SECUR_PROTO_H_
#define _UTILITIES_SECUR_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif

extern SSHORT	SECURITY_exec_line (STATUS *, void *, 
			struct user_data *, 
			void (*)(void *, USER_DATA, BOOLEAN),
			void *);
extern void	SECURITY_msg_get (USHORT, TEXT *);
extern void     SECURITY_get_db_path (TEXT *, TEXT *);

#ifdef __cplusplus
};
#endif

#endif /* _UTILITIES_SECUR_PROTO_H_ */
