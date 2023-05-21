/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_s_proto.h
 *	DESCRIPTION:	Prototype header file for isc_sync.c
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

#ifndef _JRD_ISC_S_PROTO_H_
#define _JRD_ISC_S_PROTO_H_

#include "../jrd/isc.h"

extern BOOLEAN			ISC_check_restart (void);
extern int			ISC_event_blocked (USHORT, 
							  struct event **, 
							  SLONG *);
extern SLONG 	DLL_EXPORT 	ISC_event_clear (struct event *);
extern void		   	ISC_event_fini (struct event *);
#ifndef NETWARE_386
extern int	DLL_EXPORT 	ISC_event_init (struct event *, int, int);
#else
extern int	DLL_EXPORT 	ISC_event_init (struct event *, 
						       ULONG *, int);
#endif
#if (defined WIN_NT || defined OS2_ONLY)
extern int			ISC_event_init_shared (struct event *, 
							      int, 
							      TEXT *, 
							      struct event *, 
							      USHORT);
#endif
#ifndef mpexl
extern int	DLL_EXPORT 	ISC_event_post (struct event *);
extern int	DLL_EXPORT 	ISC_event_wait (SSHORT, 
						       struct event **, 
						       SLONG *, 
						       SLONG,                                                                	       FPTR_VOID, 	      	      	      	      	      	      	               void *);
#else
extern int	DLL_EXPORT 	ISC_event_post (struct event *, 
						       SSHORT, 
						       SLONG, 
						       int);
extern int	DLL_EXPORT 	ISC_event_wait (SSHORT, 
						       struct event **, 
						       SLONG *, 
						       SSHORT, 
						       SLONG, 
						       FPTR_VOID, 
						       void *);
#endif
#ifdef WIN_NT
extern void			*ISC_make_signal (BOOLEAN, 
							 BOOLEAN, 
							 int, 
							 int);
#endif
extern UCHAR*  DLL_EXPORT 	ISC_map_file (STATUS *, 
						     TEXT *, 
						     FPTR_VOID, 
						     void *, 
						     SLONG,
						     struct sh_mem *);
#ifndef NETWARE_386
#ifdef NeXT
extern int     DLL_EXPORT       ISC_mutex_init (MTX, SLONG);
extern int     DLL_EXPORT       ISC_mutex_lock (MTX);
extern int     DLL_EXPORT       ISC_mutex_unlock (MTX);
#else
#ifdef WINDOWS_ONLY
extern int     DLL_EXPORT 	ISC_mutex_init (struct mtx *, ULONG *);
#elif !(defined WIN_NT || defined OS2_ONLY)
extern int     DLL_EXPORT 	ISC_mutex_init (struct mtx *, SLONG);
#else
extern int     DLL_EXPORT 	ISC_mutex_init (struct mtx *, TEXT *);
#endif
extern int     DLL_EXPORT 	ISC_mutex_lock (struct mtx *);
extern int     DLL_EXPORT 	ISC_mutex_lock_cond (struct mtx *);
extern int     DLL_EXPORT 	ISC_mutex_unlock (struct mtx *);
#endif
#endif
#ifdef NETWARE_386
extern int			ISC_mutex_init (ULONG *, ULONG *);
extern int			ISC_mutex_lock (ULONG *);
extern int			ISC_mutex_unlock (ULONG *);
extern void			ISC_semaphore_close (ULONG);
extern void			ISC_sync_init (void);
#endif

#ifdef SUPERSERVER
#ifdef UNIX
extern void                     ISC_exception_post (ULONG, TEXT *);
extern void			ISC_sync_signals_set (void); 
extern void			ISC_sync_signals_reset (void); 
#endif /* UNIX */

#ifdef WIN_NT
extern ULONG                    ISC_exception_post (ULONG, TEXT *);
#endif /* WIN_NT */

#endif /* SUPERSERVER */

#if (defined NETWARE_386 || defined PC_PLATFORM)
extern void			ISC_semaphore_open (ULONG *, ULONG);
#endif

extern UCHAR*   DLL_EXPORT 	ISC_remap_file (STATUS *, 
						       struct sh_mem *, 
						       SLONG, 
						       USHORT);
extern void			ISC_reset_timer (FPTR_VOID, 
							void *, 
							SLONG *, 
							void **);
extern void			ISC_set_timer (SLONG, 
						      FPTR_VOID, 
						      void *, 
						      SLONG *, 
						      void **);
extern void	DLL_EXPORT 	ISC_unmap_file (STATUS *, 
						       struct sh_mem *, 
						       USHORT);

#endif /* _JRD_ISC_S_PROTO_H_ */
