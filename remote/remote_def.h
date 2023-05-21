/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		remote_def.h
 *	DESCRIPTION:	Common descriptions
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

#ifndef _REMOTE_REMOTE_DEF_H_
#define _REMOTE_REMOTE_DEF_H_

#ifdef VMS
#define ARCHITECTURE		arch_vms
#endif

#ifdef ultrix
#ifdef mips
#define ARCHITECTURE		arch_mips_ultrix
#else
#define ARCHITECTURE		arch_ultrix
#endif
#endif

#ifdef apollo
#ifdef DN10000
#define ARCHITECTURE		arch_apollo_dn10k
#else
#define ARCHITECTURE		arch_apollo
#endif
#endif

#ifdef sun
#ifdef sparc
#define ARCHITECTURE		arch_sun4
#endif
#ifdef i386
#define ARCHITECTURE		arch_sun386
#endif
#ifndef ARCHITECTURE
#define ARCHITECTURE		arch_sun
#endif
#endif

#ifdef hpux
#ifdef hp9000s300
#define ARCHITECTURE		arch_hpux_68k
#else
#define ARCHITECTURE		arch_hpux
#endif
#endif

#ifdef mpexl
#define ARCHITECTURE		arch_hpmpexl
#endif

#ifdef IMP
#define ARCHITECTURE		arch_imp
#endif

#ifdef DELTA
#define ARCHITECTURE		arch_delta
#endif

#ifdef M88K
#define ARCHITECTURE		arch_m88k
#endif

#ifdef UNIXWARE
#define ARCHITECTURE		arch_unixware
#endif

#ifdef NCR3000
#define ARCHITECTURE		arch_ncr3000
#endif

#ifdef DOS_ONLY
#define ARCHITECTURE            arch_intel_32
#endif

#ifdef NETWARE_386
#define ARCHITECTURE            arch_msdos
#endif

#if (defined AIX || defined AIX_PPC)
#define ARCHITECTURE		arch_rt
#endif

#ifdef DECOSF
#define ARCHITECTURE		arch_decosf
#endif

#ifdef sgi
#define ARCHITECTURE		arch_sgi
#endif

#ifdef DGUX
#ifdef DG_X86
#define ARCHITECTURE		arch_dg_x86
#else
#define ARCHITECTURE		arch_aviion
#endif /* DG_X86 */
#endif /* DGUX */

#ifdef XENIX
#define ARCHITECTURE		arch_sco
#endif

#ifdef SCO_EV
#define ARCHITECTURE		arch_sco_ev
#endif

#ifdef NeXT
#ifdef I386
#define ARCHITECTURE		arch_next_386
#else
#define ARCHITECTURE		arch_next
#endif
#endif

#ifdef EPSON
#define ARCHITECTURE		arch_epson
#endif

#ifdef _CRAY
#define ARCHITECTURE		arch_cray
#endif

#ifdef I386
#ifndef ARCHITECTURE
#define ARCHITECTURE		arch_intel_32
#endif
#endif

#ifdef _PPC_
#define ARCHITECTURE		arch_nt_ppc
#endif

#ifdef LINUX
#define ARCHITECTURE		arch_linux
#endif


#define SRVR_server		1	/* 0x0001 server */
#define SRVR_multi_client	2	/* 0x0002 multi-client server */
#define SRVR_debug		4	/* 0x0004 */
#define SRVR_mpexl		8	/* 0x0008 NetIPC protocol (MPE/XL) */
#define SRVR_inet		8	/* 0x0008 Inet protocol */
#define SRVR_pipe		16	/* 0x0010 Named pipe protocol (NT) */
#define SRVR_spx		32	/* 0x0020 SPX protocol (NetWare) */
#define SRVR_ipc		64	/* 0x0040 IPC protocol for Win95 */
#define SRVR_non_service	128	/* 0x0080 Not running as an NT service */
#define SRVR_high_priority	256	/* 0x0100 fork off server at high priority */
#define SRVR_auto_unload	512	/* 0x0200 Unload server after last disconnect */
#define SRVR_xnet               1024    /* 0x0400 ipc protocol via server */
#define SRVR_thread_per_port	2048	/* 0x0800 Bind thread to a port */
#define SRVR_no_icon		4096	/* 0x1000 Tell the server not to show the icon */
#endif /* _REMOTE_REMOTE_DEF_H_ */

