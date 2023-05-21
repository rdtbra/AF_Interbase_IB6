/*
 *	PROGRAM:	JRD Access method
 *	MODULE:		log_proto.h		
 *	DESCRIPTION:	Prototype Header file for log.c 
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

#ifndef _JRD_LOG_PROTO_H_
#define _JRD_LOG_PROTO_H_

#ifdef REPLAY_OSRI_API_CALLS_SUBSYSTEM

extern void	LOG_call (enum log_t, ...);
extern void	LOG_disable (void);
extern void	LOG_enable (TEXT *, USHORT);
extern void	LOG_fini (void);
extern void	LOG_init (TEXT *, USHORT);

#endif /* REPLAY_OSRI_API_CALLS_SUBSYSTEM */

#endif	/* _JRD_LOG_PROTO_H_ */
