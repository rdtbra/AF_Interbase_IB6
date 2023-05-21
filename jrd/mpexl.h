/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		mpexl.h
 *	DESCRIPTION:	MPE/XL specific physical IO
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

#ifndef _JRD_MPEXL_H_
#define _JRD_MPEXL_H_

#pragma intrinsic	HPERRMSG, FERRMSG
#pragma intrinsic	HPFOPEN, FCLOSE, FFILEINFO, FCONTROL
#pragma intrinsic	FPOINT, FREAD, FWRITE, FCHECK, FREADDIR, FWRITEDIR
#pragma intrinsic	XCONTRAP, HPENBLTRAP, XARITRAP, RESETCONTROL, XSYSTRAP
#pragma intrinsic	HPGETPROCPLABEL, HPFIRSTLIBRARY, HPMYFILE, HPMYPROGRAM
#pragma intrinsic	HPCICOMMAND, HPCIGETVAR
#pragma intrinsic	FLOCK, FUNLOCK, FINTEXIT, FINTSTATE, IOWAIT, IODONTWAIT
#pragma intrinsic	CREATEPROCESS, GETPRIVMODE, GETUSERMODE
#pragma intrinsic	RPMCREATE, RPMGETSTRING
#pragma intrinsic	GETPROCINFO, FATHER, KILL

#pragma intrinsic	WHO, CLOCK, TIMER, ALMANAC

#pragma intrinsic	NSINFO, IPCERRMSG

#define ITM_END		0
#define OPN_FORMAL	2
#define OPN_DOMAIN	3
#define OPN_RECFM	6
#define OPN_FILE_TYPE	10
#define OPN_ACCESS_TYPE	11
#define OPN_LOCKING	12
#define OPN_EXCLUSIVE	13
#define OPN_MULTIACC	14
#define OPN_MULTIREC	15
#define OPN_NOWAIT	16
#define OPN_SMAP	18
#define OPN_LRECL	19
#define OPN_LMAP	21
#define OPN_FILE_SIZE	35
#define OPN_ALLOCATE	36
#define OPN_FILECODE	37
#define OPN_PRIVLEV	38
#define OPN_WILL_ACCESS	39
#define OPN_BLKFACTOR	40
#define OPN_INHIBIT_BUF	46
#define OPN_EXTENTS	47
#define OPN_DISP	50
#define OPN_FORMAT	53

#define INF_FID		1
#define INF_FOPTIONS	2
#define INF_AOPTIONS	3
#define INF_CURPTR	9
#define INF_EOFPTR	10
#define INF_NUMRECS	10
#define INF_FLIMIT	11
#define INF_NWRITERS	34
#define INF_NREADERS	35
#define INF_MOD_TIME	52
#define INF_MOD_DATE	53
#define INF_NODE	61
#define INF_UFID	63
#define INF_LRECL	67

#define CTL_TIMEOUT	4
#define CTL_REWIND	5
#define CTL_WRITE_EOF	6
#define CTL_ABORT	43
#define CTL_WAIT	45
#define CTL_WRITE_ID	46
#define CTL_NONDESTRUCT	47
#define CTL_ARM_AST	48

#define CP_STDIN	8
#define CP_STDLIST	9
#define CP_ACT		10
#define CP_INFO		11
#define CP_INFOLEN	12
#define CP_XL		19
#define CP_XLLEN	24

#define RPM_STROPT	20000
#define RPM_CREATE	22003
#define RPM_INFOSTR	22011
#define RPM_INFOLEN	22012

#define NS_LOC_NODELEN	18
#define NS_LOC_NODENAME	19

#define VAR_INT_VALUE	1
#define VAR_STR_VALUE	2
#define VAR_BOOL_VALUE	3
#define VAR_LENGTH	11
#define VAR_RECURSE	12
#define VAR_TYPE	13

#define DOMAIN_CREATE	0
#define DOMAIN_OPEN_PERM	1
#define DOMAIN_OPEN	3
#define DOMAIN_CREATE_PERM	4
#define RECFM_F		0
#define RECFM_V		1
#define FTYPE_MSG	6
#define ACCESS_READ	0
#define ACCESS_WRITE	1
#define ACCESS_APPEND	3
#define ACCESS_RW	5
#define LOCKING_YES	1
#define LOCKING_NO	0
#define EXCLUSIVE_NOSHR	1
#define EXCLUSIVE_RSHARE	2
#define EXCLUSIVE_SHARE	3
#define FILECODE_DB	-3050
#define PRIVLEV_DB	2
#define MULTIACC_GMULT	2
#define MULTIREC_YES	1
#define NOWAIT_NO	0
#define NOWAIT_YES	1
#define WILL_ACC_RANDOM	1
#define INHIBIT_BUF_YES	1
#define DISP_ASIS	0
#define DISP_KEEP	1
#define DISP_DELETE	4
#define FORMAT_BINARY	0
#define FORMAT_ASCII	1

#endif /* _JRD_MPEXL_H_ */
