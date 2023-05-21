/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		license.h
 *	DESCRIPTION:	Internal licensing parameters
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

#ifndef _JRD_LICENSE_H_
#define _JRD_LICENSE_H_

#include "../jrd/build_no.h"

#ifdef hpux
#ifdef hp9000s300
#ifdef HP300
#define IB_PLATFORM	"H3-V"
#endif
#ifdef HM300
#define IB_PLATFORM	"HM-V"
#endif
#else
#ifdef HP700
#define IB_PLATFORM	"HP-V"
#endif
#ifdef HP800
#define IB_PLATFORM	"HO-V"
#endif
#ifdef HP10
#define IB_PLATFORM	"HU-V"
#endif /* HP10 */
#endif
#endif

#ifdef mpexl
#define IB_PLATFORM	"HX-V"		/* HP MPE/XL */
#endif

#ifdef apollo
#if _ISP__A88K
#define IB_PLATFORM	"AP-V"
#else
#define IB_PLATFORM	"AX-V"
#endif
#endif

#ifdef sun
#ifdef sparc
#ifdef SOLARIS
#define IB_PLATFORM	"SO-V"
#else
#define IB_PLATFORM	"S4-V"
#endif
#endif
#ifdef i386
#define IB_PLATFORM     "SI-V"
#endif
#ifdef SUN3_3
#define IB_PLATFORM	"SU-V"
#endif
#ifndef IB_PLATFORM
#define IB_PLATFORM	"S3-V"
#endif
#endif

#ifdef ultrix
#ifdef mips
#define IB_PLATFORM	"MU-V"
#else
#define IB_PLATFORM	"UL-V"
#endif
#endif

#ifdef VMS
#ifdef __ALPHA
#define IB_PLATFORM     "AV-V"
#else
#define IB_PLATFORM	"VM-V"
#endif
#endif

#ifdef MAC
#define IB_PLATFORM	"MA-V"
#endif

#ifdef PC_PLATFORM
#ifdef WINDOWS_ONLY
#define IB_PLATFORM     "WS-V"
#else
#ifdef DOS_ONLY
#define IB_PLATFORM     "DS-V"
#endif
#endif
#undef NODE_CHECK
#define NODE_CHECK(val,resp) 
#endif

#ifdef NETWARE_386
#define IB_PLATFORM     "NW-V"
#endif

#ifdef OS2_ONLY
#define IB_PLATFORM     "O2-V"
#endif

#ifdef AIX
#define IB_PLATFORM	"IA-V"
#endif

#ifdef AIX_PPC
#define IB_PLATFORM	"PA-V"
#endif

#ifdef IMP
#define IB_PLATFORM	"IM-V"
#endif

#ifdef DELTA
#define IB_PLATFORM	"DL-V"
#endif

#ifdef XENIX
#ifdef SCO_UNIX
#define IB_PLATFORM	"SI-V"	/* 5.5 SCO Port */
#else
#define IB_PLATFORM	"XN-V"
#endif
#endif

#ifdef sgi
#define IB_PLATFORM	"SG-V"
#endif

#ifdef DGUX
#ifdef DG_X86
#define IB_PLATFORM     "DI-V"          /* DG INTEL */
#else
#define IB_PLATFORM	"DA-V"		/* DG AViiON */
#define M88K_DEFINED
#endif /* DG_X86 */
#endif /* DGUX */

#ifdef WIN_NT
#ifdef i386
#if (defined SUPERCLIENT || defined SUPERSERVER)
#if (defined WIN95)
#define IB_PLATFORM	"WI-V"
#else
#define IB_PLATFORM	"NIS-V"
#endif /* WIN95 */
#else
#define IB_PLATFORM	"NI-V"
#endif
#else
#ifdef alpha
#define IB_PLATFORM	"NA-V"
#else
#ifdef mips
#define IB_PLATFORM	"NM-V"
#else /* PowerPC */
#define IB_PLATFORM	"NP-V"
#endif
#endif
#endif
#endif

#ifdef NeXT
#ifdef	i386
#define IB_PLATFORM	"XI-V"
#else	/* m68040 */
#define IB_PLATFORM	"XM-V"
#endif
#endif

#ifdef EPSON
#define IB_PLATFORM	"EP-V"		/* epson */
#endif

#ifdef _CRAY
#define IB_PLATFORM	"CR-V"		/* Cray */
#endif

#ifdef ALPHA_NT
#define	IB_PLATFORM	"AN-V"		/* Alpha NT */
#endif

#ifdef DECOSF
#define	IB_PLATFORM	"AO-V"		/* Alpha OSF-1 */
#endif

#ifdef M88K
#define	IB_PLATFORM	"M8-V"		/* Motorola 88k */
#endif

#ifdef UNIXWARE
#define	IB_PLATFORM	"UW-V"		/* Unixware */
#endif

#ifdef NCR3000
#define	IB_PLATFORM	"NC-V"		/* NCR3000 */
#endif

#ifdef LINUX
#define IB_PLATFORM     "LI-B"         /* Linux on Intel */
#endif

#ifndef GDS_VERSION
#define GDS_VERSION	IB_PLATFORM IB_MAJOR_VER "." IB_MINOR_VER "." IB_REV_NO "." IB_BUILD_NO
#endif

#endif /* _JRD_LICENSE_H_ */

