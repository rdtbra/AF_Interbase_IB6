/*
 *      PROGRAM:        JRD access method
 *      MODULE:         common.h
 *      DESCRIPTION:    Common descriptions for all GDS programs
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

#ifndef _JRD_COMMON_H_
#define _JRD_COMMON_H_

/* 
  do not use links in source code to maintain platform neutraility
*/ 

#undef LINKS_EXIST


#ifdef PROD_BUILD
#ifdef DEV_BUILD
#undef DEV_BUILD
#endif
#endif

#ifdef SUPERSERVER
#define GOVERNOR 1
#define CANCEL_OPERATION
#endif

/* this define indicates places where code was merged in 
   from the Harbor Software contract to do the Windows 3.1 port */

#ifndef HARBOR_MERGE
#define HARBOR_MERGE
#endif 


/* Linux for Intel platforms*/
#ifdef LINUX
#define QUADFORMAT "ll"
#define QUADCONST(n) (n##LL)
#define MMAP_SUPPORTED

#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED

#define VA_START(list,parmN)    va_start (list, parmN)
#define UNIX    1
#define IEEE    1

#ifdef i386
#define I386    1
#define VAX     1
#define IMPLEMENTATION  60 /* next higher unique number, See you later  */
#endif /* i386 */

#define SETPGRP         setpgrp ()
#define ATEXIT(c)       atexit (c)

#define MEMMOVE(from,to,length)		memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))

#define VOLATILE        volatile
#endif /* LINUX */

/* Apollo platforms */

#ifdef apollo
#define BSD_UNIX        1
#define APOLLO          1
#define IMPLEMENTATION  25
#define                 IEEE
#define ENUM            short enum
#define SETPGRP         setpgrp (0, 0)
#define INTL
#define SYS_ARG		isc_arg_domain
#if _ISP__A88K
#define DN10000         1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       4
#else
#define ALIGN(n,b)      ((n+1) & ~1)
#endif

#endif


/* SUN platforms--the 386i is obsolete */

#ifdef sun
#define MMAP_SUPPORTED

/* Defined KILLER_SIGNALS for Sun - as we were getting lots of lockups
 * using pipe server.
 * 1995-February-24 David Schnepper 
 */
#define KILLER_SIGNALS  

#ifdef SOLARIS

#ifndef PIPE_IS_SHRLIB
/* This is NOT defined when building the special shared-pipe library
 * which customers can use to avoid the problems with signals & threads
 * in Solaris
 */
#define SOLARIS_MT	1
#endif

/*  Define the following only on platforms whose standard I/O
 *  implementation is so weak that we wouldn't be able to fopen
 *  a file whose underlying file descriptor would be > 255.
 *  Hey, we're not running on PDP-11's any more: would it kill you
 *  to use a short instead of a char to hold the fileno?  :-(
 */
#define NEED_IB_STDIO

/* Define the following macro only on platforms which supply the library
   routine xdr_hyper.  For other platforms, omit the definition of this
   macro, and we will supply that function where we need it. */
#define PLATFORM_SUPPLIES_XDR_HYPER

#define ATEXIT(c)       atexit (c)
#define SETPGRP         setpgrp ()
#define SIGACTION_SUPPORTED
#define NO_FLOCK
#define CONST           const
#define VOLATILE        volatile
#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
/*********   Reason for introducing MEMMOVE macro.

  void *memcpy( void *s1, const void *s2, size_t n);
  void *memmove( void *s1, const void *s2, size_t n);

  The memcpy() function copies n characters from the string pointed to by the
  s2 parameter into the location pointed to by the s1 parameter.  When copy-
  ing overlapping strings, the behavior of this function is unreliable.

  The memmove() function copies n characters from the string at the location
  pointed to by the s2 parameter to the string at the location pointed to by
  the s1 parameter.  Copying takes place as though the n number of characters
  from string s2 are first copied into a temporary location having n bytes
  that do not overlap either of the strings pointed to by s1 and s2. Then, n
  number of characters from the temporary location are copied to the string
  pointed to by s1. Consequently, this operation is nondestructive and
  proceeds from left to right.
  The above text is taken from the Digital UNIX man pages.

     For maximum portability, memmove should be used when the memory areas
     indicated by s1 and s2 may overlap, and memcpy used for faster copying
     between non-overlapping areas. 

**********/

#define STRICMP(s1, s2)		strcasecmp(s1, s2)

#ifndef SOLARIS26

/* If our build platform is at least Solaris release 2.6, then unistd.h
   declares these functions, so we must not define them with macros. */

#define setreuid(ruid,euid)     ((setuid (ruid) == -1 || seteuid (euid) == -1) ? -1 : 0)
#define setregid(rgid,egid)     ((setgid (rgid) == -1 || setegid (egid) == -1) ? -1 : 0)
#endif

/* The following define is the prefix to go in front of a "d" or "u"
   format item in a ib_printf() format string, to indicate that the argument
   is an SINT64 or UINT64. */
#define QUADFORMAT "ll"
/* The following macro creates a quad-sized constant, possibly one
   which is too large to fit in a long int. */
#define QUADCONST(n) (n##LL)

#else  /* SOLARIS */

#define ATEXIT(c)       on_exit (c, 0)
#define BSD_UNIX        1
#define SETPGRP         setpgrp (0, 0)

#endif /* SOLARIS */

#define UNIX            1
#define                 IEEE

#ifdef sparc
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       4
#define IMPLEMENTATION  30
#define DOUBLE_ALIGN    8
#define INTL
#else  /* sparc */

#ifdef i386
#define I386            1
#define VAX             1
#define IMPLEMENTATION  32
#else  /* i386 */
#define ALIGN(n,b)      ((n+1) & ~1)
#define IMPLEMENTATION  28
#endif /* i386 */

#endif /* sparc */

#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#endif /* sun */


/* MAC AUX--this port was never completed */ 

#ifdef MAC
#define NO_PYXIS
#define SETPGRP         setpgrp (0, 0)
#define UNIX            1
#define ALIGN(n,b)      ((n+1) & ~1)
#define IMPLEMENTATION  34
#define                 IEEE
#define ATEXIT(c)       atexit (c)
#endif


/* NeXT platforms */

#ifdef NeXT
#define MMAP_SUPPORTED
#define NO_PYXIS
#define INTL
#define INTL_BACKEND
#define SETPGRP         setpgrp (0, 0)
#define BSD_UNIX        1
#define UNIX            1
#define ALIGNMENT       4
#define DOUBLE_ALIGN    8
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))

#ifdef  i386
#define I386            1
#define VAX             1
#endif

#define IMPLEMENTATION  46
#define                 IEEE

#endif /* NeXT */


/* HP/UX platforms */

#if (defined(hpux) || defined(HPUX))

#ifndef hpux
#define hpux
#endif

#define ATEXIT(c)       atexit (c)
#define KILLER_SIGNALS
#define SETPGRP         setpgid (0, 0)
#define UNIX            1
#define CURSES_KEYPAD   1
#define INTL
#define NO_FLOCK

#ifdef hp9000s300
#define ALIGN(n,b)      ((n+1) & ~1)
#define ALIGNMENT       4
#define DOUBLE_ALIGN    4
#define IMPLEMENTATION  40
#define SHMEM_PICKY
#else
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define DOUBLE_ALIGN    8
#define IMPLEMENTATION  31
#endif

#define                 IEEE
#include "../jrd/pragma.h"
#define setreuid(ruid,euid)     setresuid (ruid, euid, -1)
#define setregid(rgid,egid)     setresgid (rgid, egid, -1)

/* The following define is the prefix to go in front of a "d" or "u"
   format item in a ib_printf() format string, to indicate that the argument
   is an SINT64 or UINT64. */
#define QUADFORMAT "ll"
/* The following macro creates a quad-sized constant, possibly one
   which is too large to fit in a long int. */
#define QUADCONST(n) (n##LL)

#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define STRICMP(s1, s2)			strcasecmp(s1, s2)
#endif /* hpux */


/* DEC Ultrix */

#ifdef ultrix
#define BSD_UNIX        1
#define INTL
#define INTL_BACKEND
#define UNIX            1
#define VAX             1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define SETPGRP         setpgrp (0, 0)

#ifdef mips
#define IMPLEMENTATION  36
#define                 IEEE
#define ALIGNMENT       4
#define DOUBLE_ALIGN    8
#else
#define CTO32L(p)       (*(long*) p)
#define IMPLEMENTATION  26
#define SHMEM_PICKY
#endif

#endif /* ultrix */


/* PC Platforms are NetWare, Windows 3.1, and Windows and DOS Clients */

#ifdef PC_PLATFORM

#define SYS_ARG		isc_arg_dos
#ifndef NETWARE_386
#define VA_START(list,parmN)    va_start (list, parmN)
#define NO_PYXIS
#define I386            1
#define VAX                             /* Use VAX style byte swapping */
#define ATEXIT(c)
#define NO_NFS
#undef LINKS_EXIST
#define IEEE                            /* IEEE floating point arith.  */
#define CONST           const

#ifdef WINDOWS_ONLY
#define IMPLEMENTATION  55
#define ALIGNMENT       4
#define DOUBLE_ALIGN    8
#define ALIGN(n,b)      ((n + b - 1) & ~((ULONG)(b) - 1))
#define ROUNDUP(n,b)    (((n) + (b) - 1) & ~((ULONG)(b) - 1))

#define SPACE_IN_SEG(a)         ( 65536L - (ULONG)( FP_OFF( a))) 
#define CROSSES_SEG(a,s)        (( 65536L - (ULONG)( FP_OFF( a))) < (s)) 

#define MOVE_FAST(from,to,length)       SEG_move( to, from, (ULONG)(length))
#define MOVE_FASTER(from,to,length)     SEG_move( to, from, (ULONG)(length))
#define MOVE_CLEAR(to,length)           SEG_set( to, 0, (ULONG)(length))
#define STRICMP(s1, s2)			stricmp(s1, s2)

#define NOMSG

/* Turn on GUI_TOOLS define to signify that this platform will build and
 * use GUI_TOOLS.  If it is not defined, then you will use the command line
 * version of these tools (if available).
*/
#define GUI_TOOLS

#include <windows.h>
/* 
** In win16, memory is owned by the calling app and not the DLL unless
** the GMEM_DDESHARE flag is used.  This means that any memory which
** is meant to persist for the life of the DLL must be allocated with
** GMEM_DDESHARE or it will be freed when the calling application exits
** and other apps using the DLL will trap when the memory is referenced.
** Under IB16, gds__alloc ends up calling the runtime sub-segment
** allocator, malloc(), which uses the GMEM_SHARE flag to alloc memory. */

#ifdef DEBUG_GDS_ALLOC
#define ALLOC_LIB_MEMORY(size)   gds__alloc_debug ((size),(TEXT *)__FILE__,(ULONG)__LINE__)
#else
#define ALLOC_LIB_MEMORY(size)   gds__alloc (size)
#endif

#define FREE_LIB_MEMORY(block)  gds__free (block)

/* for Windows, turn on stack reduction methods in all cases, 
   whether routines are commonly used or not */

#define STACK_REDUCTION  
#define STACK_EFFICIENT

#define BLOB_PTR        UCHAR HUGE_PTR

#else  /* WINDOWS_ONLY */
#define IMPLEMENTATION  47
#define MEMMOVE(from,to,length)       memmove (to, from, (int) (length))
#define MOVE_FAST(from,to,length)       memmove (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memmove (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define STRICMP(s1, s2)			stricmp(s1, s2)
#endif  /* WINDOWS_ONLY */

#define API_ROUTINE     __far __cdecl __loadds __export
#define API_ROUTINE_VARARG      API_ROUTINE
#define FAR_VARIABLE    __far

/* added for routines which are not to used by customers. */
#define INTERNAL_API_ROUTINE	API_ROUTINE

#ifndef DLL_EXPORT
#define DLL_EXPORT      API_ROUTINE     
#endif

#endif  /* ifndef NETWARE_386 */


/* DOS does not compare the high order word of far pointers, 
   so we must cast them to huge in comparions; also, for routines
   which return a double, they must be given a pascal type so that
   they will work in a multithreaded environment */

#define HUGE_PTR        huge
#define PASCAL_ROUTINE  pascal

#endif  /* PC_PLATFORM */


/* DEC VAX/VMS and AlphaVMS */

#ifdef VMS
#define VAX             1
#define VAX_FLOAT       1
#define ALIGN(n,b)      (n)
#define ALIGNMENT       4
#define NO_NFS
#define CTO32L(p)       (*(long*)p)
#define NO_CHECKSUM	1
#define ISC_EXT_LIB_PATH_ENV	"interbase_ext_lib_path:"
#define SYS_ARG		isc_arg_vms

#if __ALPHA
#define IMPLEMENTATION  53
#include <ints.h>
#define ATOM_DEFINED
typedef int64		SATOM;		/* 64 bit */
typedef unsigned int64	UATOM;
#else

#ifndef GATEWAY
#define IMPLEMENTATION  27
#else
#define IMPLEMENTATION  33
#endif

#endif /* __ALPHA */

#define FINI_OK         1
#define FINI_ERROR      44
#define STARTUP_ERROR   46 /* this is also used in iscguard.h, make sure these match */
#define INTL

#endif /* VMS */


/* IBM AIX RS/6000 and IBM AIX PowerPC */

#ifdef _AIX     /* IBM AIX */
#ifndef _POWER  /* IBM RS/6000 */
#define AIX
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define SETPGRP         setpgid (0, 0)
#define ATEXIT(c)       atexit (c)
#define vfork           fork
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       4
#define IMPLEMENTATION  35
#define                 IEEE
#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define INTL
#define VOLATILE	volatile
#else           /* AIX PowerPC */
#define AIX_PPC
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define SETPGRP         setpgid (0, 0)
#define ATEXIT(c)       atexit (c)
#define vfork           fork
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       4
#define IMPLEMENTATION  35
#define                 IEEE
#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define INTL
#define VOLATILE	volatile
#endif   /* IBM PowerPC */
#endif  /* IBM AIX */


/* OS/2 */

#if (defined(__IBMC__) && defined(__OS2__)) || (defined(__BORLANDC__) && defined(__OS2__))
#define OS2_ONLY
#define NO_PYXIS
#define NO_NFS
#define I386            1
#define VAX
#undef LINKS_EXIST
#define IEEE
#define IMPLEMENTATION  29
#define INTL
#define VA_START(list,parmN)    va_start (list, parmN)

#define SYS_ARG		isc_arg_dos

#ifdef __IBMC__
#define API_ROUTINE     _System
#define API_ROUTINE_VARARG      API_ROUTINE
#define CLIB_ROUTINE    _Optlink
#define THREAD_ROUTINE  _System
#define INTERNAL_API_ROUTINE	API_ROUTINE
#else
#define API_ROUTINE     __stdcall
#define API_ROUTINE_VARARG      API_ROUTINE
#define CLIB_ROUTINE    __stdcall
#define THREAD_ROUTINE  __stdcall
#define INTERNAL_API_ROUTINE	API_ROUTINE
#define _System         __syscall
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

#endif  /* OS2_ONLY */


/* Windows NT */

#if (defined(_MSC_VER) && defined(WIN32)) || (defined(__BORLANDC__) && defined(__WIN32__))
#define WIN_NT
#define NO_PYXIS
#define NOINITGROUPS
#define NO_NFS
#define VAX             
#undef LINKS_EXIST

#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define MEMMOVE(from,to,length)         memmove ((void *)to, (void *)from, (size_t) length)
#define STRICMP(s1, s2)			stricmp(s1, s2)

#define CONST		const
#define VOLATILE	volatile
#define SYS_ARG		isc_arg_win32

typedef          __int64 SINT64;
typedef unsigned __int64 UINT64;
#define INT64_DEFINED

/* The following define is the prefix to go in front of a "d" or "u"
   format item in a ib_printf() format string, to indicate that the argument
   is an SINT64 or UINT64. */
#define QUADFORMAT "I64"
/* The following macro creates a quad-sized constant, possibly one
   which is too large to fit in a long int.  The Microsoft compiler does
   not permit the LL suffix which some other platforms require, but it
   handles numbers up to the largest 64-bit integer correctly without such
   a suffix, so the macro definition is trivial. */
#define QUADCONST(n) (n)

#ifdef _X86_
#define i386            
#ifndef I386
#define I386
#endif            
#define IMPLEMENTATION  50
#endif

#ifdef _PPC_
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define DOUBLE_ALIGN    8
#define IMPLEMENTATION	57
#endif

#define ATEXIT(c)       atexit (c)
#define                 IEEE
#define INTL
#define VA_START(list,parmN)    va_start (list, parmN)
#define API_ROUTINE     __stdcall
#define API_ROUTINE_VARARG      __cdecl
#define CLIB_ROUTINE    __cdecl
#define THREAD_ROUTINE  __stdcall
#define INTERNAL_API_ROUTINE	API_ROUTINE

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

#define BOOLEAN_DEFINED
typedef unsigned char   BOOLEAN;

#ifdef DEV_BUILD
#define DEBUG_GDS_ALLOC
#define ALLOC_LIB_MEMORY(size)   gds__alloc_debug ((size),(TEXT *)__FILE__,(ULONG)__LINE__)
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	260
#endif

#endif /* WIN_NT */


/* SCO */

#ifndef SCO_EV

#ifdef M_I386

#define SMALL_FILE_NAMES
#define XENIX           1
#define NOHOSTNAME
#define KILLER_SIGNALS
#define SYSV_SIGNALS
#define ATEXIT(c)       atexit (c)
#define vfork           fork

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define I386            1
#define VAX             1
#define UNIX            1
#define CURSES_KEYPAD   1
#define NOINITGROUPS

#ifdef M_UNIX
#define SCO_UNIX        1
#define SETPGRP         setpgid (0, 0)
#define IMPLEMENTATION  42
#define INTL
#define INTL_BACKEND
#else
#define ALIGN(n,b)      ((n+1) & ~1)
#define SETPGRP
#define IMPLEMENTATION  37
#endif

#define                 IEEE
#undef LINKS_EXIST

#endif /* M_I386 */

#else /* ifndef SCO_EV */

#define XENIX           1
#define I386            1
#define VAX             1
#define UNIX            1
#define SCO_UNIX        1
#define                 IEEE
#define SETPGRP         setpgrp()
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define ATEXIT(c)       atexit (c)
/*
#define KILLER_SIGNALS
*/
#define INTL
#define IMPLEMENTATION  59
#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)    memcpy (to, from, (unsigned int) (length))
#define MOVE_FASTER(from,to,length)  memcpy (to, from, (unsigned int) (length))
#define MOVE_CLEAR(to,length)        memset (to, 0, (unsigned int) (length))

#define VOLATILE volatile	/* 5.5 SCO Port */
#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif
/*  These functions are supported so we don't need the defines
#define setreuid(ruid,euid)     setuid(euid)
#define setregid(rgid,egid)     setgid(egid)
*/

#endif  /* ifndef SCO_EV */


#ifdef DGUX
#define UNIX            1
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define SETPGRP         setpgid (0, 0)
#define ATEXIT(c)       atexit (c)
#define CURSES_KEYPAD   1
#ifndef DG_X86
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define IMPLEMENTATION  38
#define DOUBLE_ALIGN    8
#else
#define IMPLEMENTATION        58
#define CONST         const
#define VOLATILE      volatile
#define I386          1
#define VAX           1
#endif  /* DG_X86 */
#define                 IEEE
#define INTL
#define INTL_BACKEND

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#endif /* DGUX */


#if (defined(__osf__) && defined(__alpha))
#include <stddef.h>
#define DECOSF
#define INTL
#define NO_PYXIS
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define HAS_64BIT_POINTERS /* if a machine has 64 bit pointers you need this */
#define SETPGRP         setpgrp (0, 0)
#define UNIX            1
#define VAX             1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8   
#define DOUBLE_ALIGN    8  
#define IMPLEMENTATION  52
#define                 IEEE
#define ATEXIT(c)       atexit (c)

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#define VOLATILE        volatile
#endif

#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define LONG_DEFINED

typedef int             SLONG;
typedef unsigned int    ULONG;
#define QUAD_DEFINED
typedef long            SQUAD;
typedef unsigned long   UQUAD;
#define NATIVE_QUAD
#define ATOM_DEFINED
typedef long		SATOM;		/* 64 bit */
typedef unsigned long	UATOM;

#endif /* DECOSF */


#ifdef sgi
#define vfork           fork
#define KILLER_SIGNALS
#define SETPGRP         setpgid (0, 0)
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define IMPLEMENTATION  41
#define DOUBLE_ALIGN    8
#define                 IEEE
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define INTL
#define VOLATILE	volatile
#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#endif /* sgi */


#ifdef mpexl
#define ATEXIT(c)       atexit (c)
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define IMPLEMENTATION  39
#define NO_NFS
#define DOUBLE_ALIGN    8
#define                 IEEE
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define SYS_ARG		isc_arg_mpexl
#endif


#ifdef _CRAY
#define KILLER_SIGNALS
#include <stddef.h>
#define SETPGRP         setpgid (0, 0)
#define vfork           fork
#define ATEXIT(c)       atexit (c)
#define UNIX            1
#define HAS_64BIT_POINTERS /* if a machine has 64 bit pointers you need this */
#define CURSES_KEYPAD   1
#define AOF32L(l)       (((char*) &l) + 4)
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8
#define IMPLEMENTATION  43
#define DOUBLE_ALIGN    8
#define                 IEEE
#define _BSD_SIGNALS    1
#define OFFSET(struct,fld)      offsetof(struct, fld)
#define OFFSETA(struct,fld)     offsetof(struct, fld)
#define ODS_ALIGNMENT   8
#define SHIFTLONG       3
#define BITS_PER_LONG   64
#define LOG2_BITS_PER_LONG      6
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define CHAR_DEFINED
typedef signed char     SCHAR;
typedef unsigned char   UCHAR;
#define ATOM_DEFINED
typedef long		SATOM;		/* 64 bit */
typedef unsigned long	UATOM;
#endif


#ifdef NETWARE_386
/*
 * In ..\remote, there will a duplicate definition for DWORD
 * ocurring from <nwdstype.h>. Chech for this
 * and undefine it before defining DWORD.
 */

#ifdef DWORD
#undef DWORD
#endif

typedef char *	caddr_t;
/*
 * The DWORD declaration is needed for compiling the JRD lib.
 * (First compilation error will start from isc.c)
 */

typedef unsigned long       DWORD;

#define SMALL_FILE_NAMES
#define NOHOSTNAME
#define ATEXIT(c)       atexit (c)

#define SYS_ARG		isc_arg_dos
#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define I386            1
#define VAX             1
#define IEEE            1
#define ALIGN(n,b)      ((n+1) & ~1)
#define OLD_ALIGNMENT
#define NO_NFS
#define SETPGRP
#define IMPLEMENTATION 54 
#undef LINKS_EXIST
#define VA_START(list,parmN)    va_start (list, parmN)
#define CHAR_DEFINED

typedef signed char     SCHAR;
typedef unsigned char   UCHAR;

#define INTL
#define INTL_BACKEND
#define SYNC_WRITE_DEFAULT      1

/* turn on stack reduction methods for only those routines 
   which are not commonly used; since this is a server, we 
   want to allocate buffers on the stack for oft-used routines */

#define STACK_REDUCTION 

#endif /* NETWARE_386 */


#ifdef DELTA
#define NO_PYXIS
#define INTL
#define INTL_BACKEND
#define SETPGRP         setpgrp()
#define NOINITGROUPS
#define KILLER_SIGNALS
#define SYSV_SIGNALS
#define SYSV_SHMEM
#define SMALL_FILE_NAMES
#define vfork           fork
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n+1) & ~1)
#define ALIGNMENT       4
#define DOUBLE_ALIGN    4
#define IMPLEMENTATION  45
#define                 IEEE
#define SHMEM_PICKY

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#undef LINKS_EXIST
#define INTL
#define setreuid(ruid,euid)     setuid(euid)
#define setregid(rgid,egid)     setgid(egid)

#endif /* DELTA */


#ifdef M88K
#define INTL
#define SETPGRP         setpgrp()
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#define ALIGNMENT       8   
#define DOUBLE_ALIGN    8  
#define IMPLEMENTATION  48
#define                 IEEE
#define ATEXIT(c)       atexit (c)

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define setreuid(ruid,euid)     setuid(euid)
#define setregid(rgid,egid)     setgid(egid)

#endif /* M88K */


#ifdef UNIXWARE
#define INTL
#define SETPGRP         setpgrp()
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define NO_PYXIS
#define I386            1
#define VAX             1
#define UNIX            1
#define CURSES_KEYPAD   1
#define IMPLEMENTATION  49
#define                 IEEE
#define ATEXIT(c)       atexit (c)
#define vfork           fork

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))
#define setreuid(ruid,euid)     setuid (euid)
#define setregid(rgid,egid)     setgid (egid)

#endif /* UNIXWARE */


#ifdef NCR3000
#define INTL
#define SETPGRP         setpgrp()
#define KILLER_SIGNALS
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define NO_PYXIS
#define I386            1
#define VAX             1
#define UNIX            1
#define CURSES_KEYPAD   1
#define IMPLEMENTATION  56
#define                 IEEE
#define ATEXIT(c)       atexit (c)

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define MEMMOVE(from,to,length)       memmove ((void *)to, (void *)from, (size_t) length)
#define MOVE_FAST(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)   memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)         memset (to, 0, (int) (length))
#define setreuid(ruid,euid)   setuid (euid)
#define setregid(rgid,egid)   setgid (egid)

#define SYSCALL_INTERRUPTED(err)        (((err) == EINTR) || ((err) == ERESTART))

#endif /* NCR3000 */


#ifdef IMP
#define NO_PYXIS
#define SETPGRP         setpgrp()
#define KILLER_SIGNALS
#define SYSV_SIGNALS
#define SYSV_SHMEM
#define SMALL_FILE_NAMES
#define vfork           fork
#define UNIX            1
#define CURSES_KEYPAD   1
#define ALIGN(n,b)      ((n+1) & ~1)
#define ALIGNMENT       4
#define DOUBLE_ALIGN    4
#define IMPLEMENTATION  44
#define                 IEEE
#define SHMEM_PICKY
#undef LINKS_EXIST
#define INTL
#define INTL_BACKEND
#endif


#ifdef EPSON
#define I386            1
#define VAX             1
#define UNIX            1
#define                 IEEE
#define SETPGRP         setpgrp()
#define SIGACTION_SUPPORTED
#define MMAP_SUPPORTED
#define NO_FLOCK
#define SMALL_FILE_NAMES
#define ATEXIT(c)       atexit (c)
#define KILLER_SIGNALS
#define INTL
#define IMPLEMENTATION  51
#define NO_PYXIS
#define MOVE_FAST(from,to,length)       memcpy (to, from, (int) (length))
#define MOVE_FASTER(from,to,length)     memcpy (to, from, (int) (length))
#define MOVE_CLEAR(to,length)           memset (to, 0, (int) (length))

#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif

#define setreuid(ruid,euid)     setuid(euid)
#define setregid(rgid,egid)     setgid(egid)

#endif /* EPSON */

#ifdef UNIX
#define NO_CHECKSUM     1
#define SYS_ARG		isc_arg_unix
#endif /* UNIX */

/* various declaration modifiers */

#ifndef HUGE_PTR
#define HUGE_PTR        
#endif

#ifndef FAR_VARIABLE
#define FAR_VARIABLE
#endif

#ifndef PASCAL_ROUTINE
#define PASCAL_ROUTINE
#endif

#ifndef API_ROUTINE
#define API_ROUTINE
#define API_ROUTINE_VARARG
#define INTERNAL_API_ROUTINE	API_ROUTINE
#endif

#ifndef CLIB_ROUTINE
#define CLIB_ROUTINE
#endif

#ifndef THREAD_ROUTINE
#define THREAD_ROUTINE
#endif

#ifndef CONST
/* To handle platforms which don't support ANSI keyword "const" */
#define CONST           /* nothing */
#endif

#ifndef VOLATILE
/* To handle platforms which don't support ANSI keyword "volatile" */
#define VOLATILE        /* nothing */
#endif



/* alignment macros */

#ifndef OLD_ALIGNMENT
#ifndef WINDOWS_ONLY
#ifdef I386
/* Using internal alignment optimal for 386 processor and above
 */
#define ALIGNMENT       4
#define DOUBLE_ALIGN    8
#define ALIGN(n,b)      ((n + b - 1) & ~(b - 1))
#endif
#endif
#endif

#ifndef ALIGNMENT
#define ALIGNMENT       2
#endif

#ifndef SHIFTLONG
#define SHIFTLONG       2
#define BITS_PER_LONG   32
#define LOG2_BITS_PER_LONG      5
#endif

#ifndef DOUBLE_ALIGN
#define DOUBLE_ALIGN    4
#endif


/* common return values */

#ifndef FINI_OK
#define FINI_OK         0
#define FINI_ERROR      1
#define STARTUP_ERROR   2 /* this is also used in iscguard.h, make sure these match */
#endif

#ifndef NULL
#define NULL            0L
#endif

#define NULL_PTR        ((void*) 0)
#define TRUE            1
#define FALSE           0
#define SUCCESS         0
#define FAILURE         1


/* data type definitions */

#ifndef CHAR_DEFINED			/* 8 bit */
typedef char            SCHAR;
typedef unsigned char   UCHAR;
#else
#undef CHAR_DEFINED
#endif

#ifndef SHORT_DEFINED			/* 16 bit */
typedef short           SSHORT;
typedef unsigned short  USHORT;
#else
#undef SHORT_DEFINED
#endif

#ifndef LONG_DEFINED			/* 32 bit */
typedef long            SLONG;
typedef unsigned long   ULONG;
#else
#undef LONG_DEFINED
#endif

#ifndef INT64_DEFINED		        /* 64 bit */
typedef          long long int SINT64;
typedef unsigned long long int UINT64;
#else
#undef INT64_DEFINED
#endif

#ifndef QUAD_DEFINED			/* 64 bit */
typedef struct {
    SLONG	high;
    ULONG	low;
}			SQUAD;
#endif

/* Enable support for READONLY databases in InterBase, IB 6.0 (Kinobi) project */
#ifndef READONLY_DATABASE
#define READONLY_DATABASE
#endif

#ifndef ATOM_DEFINED			/* 32 or 64 bit */
typedef long		SATOM;
typedef unsigned long	UATOM;
#else
#undef ATOM_DEFINED
#endif

#ifndef BOOLEAN_DEFINED
typedef USHORT          BOOLEAN;
#else
#undef BOOLEAN_DEFINED
#endif

#ifndef _ISC_TIMESTAMP_
typedef SLONG		ISC_DATE;
typedef ULONG		ISC_TIME;
typedef struct {
    ISC_DATE	timestamp_date;
    ISC_TIME	timestamp_time;
} ISC_TIMESTAMP;
#define _ISC_TIMESTAMP_	1
#endif

#define GDS_DATE	ISC_DATE
#define GDS_TIME	ISC_TIME
#define GDS_TIMESTAMP	ISC_TIMESTAMP

typedef char            TEXT;		/* To be expunged over time */
typedef unsigned char	STEXT;		/* Signed text - very rare */
typedef unsigned char	UTEXT;		/* Unsigned text - common */
typedef unsigned char	BYTE;		/* Unsigned byte - common */
typedef char		SBYTE;		/* Signed byte - rare usage */
typedef long            STATUS;
typedef long            IPTR;
typedef unsigned long   U_IPTR;
typedef void            (*FPTR_VOID)();
typedef int             (*FPTR_INT)();
typedef ULONG		RCRD_OFFSET;
typedef USHORT		FLD_LENGTH;

#ifndef ENUM
#define ENUM            enum
#endif

#ifndef BLOB_PTR
#define BLOB_PTR        UCHAR
#endif


/* variable argument definitions */

#ifndef APOLLO
#ifdef SCO
#if __STDC__
#define VA_START(list,parmN)    va_start (list, parmN)
#endif
#else
#ifdef __STDC__
#define VA_START(list,parmN)    va_start (list, parmN)
#endif
#endif  /* SCO */
#endif

#ifndef VA_START
#define VA_START(list,parmN)    va_start (list)
#endif


/* conditional compilation macros */


/* MAX and MIN for datatypes */

#define MAX_UCHAR		((UCHAR)0xFF)
#define MIN_UCHAR		0x00

#define MAX_SCHAR		0x7F
#define MIN_SCHAR		(-MAX_SCHAR-1)

#define MAX_USHORT		((USHORT)0xFFFF)
#define MIN_USHORT		0x0000

#define MAX_SSHORT		0x7FFF
#define MIN_SSHORT		(-MAX_SSHORT-1)

#define MAX_ULONG		((ULONG)0xFFFFFFFF)
#define MIN_ULONG		0x00000000

#define MAX_SLONG		0x7FFFFFFF
#define MIN_SLONG		(-MAX_SLONG-1)

#define MAX_UINT64              ((UINT64) QUADCONST(0xFFFFFFFFFFFFFFFF))
#define MIN_UINT64              QUADCONST(0x0000000000000000)

#define MAX_SINT64              QUADCONST(0x7FFFFFFFFFFFFFFF)
#define MIN_SINT64              (-MAX_SINT64-1)


/* commonly used macros */

#ifndef MAX
#define MAX(a,b)                (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)                (((a) < (b)) ? (a) : (b))
#endif

#define UPPER(c)                (((c) >= 'a' && (c)<= 'z') ? (c) - 'a' + 'A' : (c))
#define LOWWER(c)               (((c) >= 'A' && (c)<= 'Z') ? (c) - 'A' + 'a' : (c))

/* UPPER7 & LOWWER7 are to be used when the data is known to
   be 7-bit ASCII, eg: internal data, OS data.  NOT user data.  */

#define UPPER7(c)               (((c) >= 'a' && (c)<= 'z') ? (c) - 'a' + 'A' : (c))
#define LOWWER7(c)              (((c) >= 'A' && (c)<= 'Z') ? (c) - 'A' + 'a' : (c))

#ifndef ROUNDUP
#define ROUNDUP(n,b)            (((n) + (b) - 1) & ~((b) - 1))
#endif
#define ROUNDUP_LONG(len)       ROUNDUP (len, sizeof (SLONG))

#define JRD_BUGCHK 15           /* facility code for bugcheck messages */               
#ifndef OFFSET
#define OFFSET(struct,fld)      ((int) &((struct) 0)->fld)
#define OFFSETA(struct,fld)     ((int) ((struct) 0)->fld)
#endif

#ifndef ODS_ALIGNMENT
#define ODS_ALIGNMENT           4
#endif

#ifndef SYSCALL_INTERRUPTED
#define SYSCALL_INTERRUPTED(err)        ((err) == EINTR)
#endif

#ifndef STRICMP
#define STRICMP(s1, s2)			strcasecmp(s1, s2)
#endif


/* data conversion macros */

#ifndef CTO32L
#ifdef VAX
#define CTO32L(p) ((((SCHAR*)(p))[3] << 24) | (((UCHAR*)(p))[2] << 16) | (((UCHAR*)(p))[1] << 8) | (((UCHAR*)(p)) [0]))
#else
#define CTO32L(p) ((((SCHAR*)(p))[0] << 24) | (((UCHAR*)(p))[1] << 16) | (((UCHAR*)(p))[2] << 8) | (((UCHAR*)(p)) [3]))
#endif
#endif

#ifndef AOF32L
#define AOF32L(l)               &l
#endif


/* data movement and allocation macros */

#ifndef MOVE_FAST
#define MOVE_FAST(from,to,length)       MOV_fast (from, to, (ULONG) (length))
#endif

#ifndef MOVE_FASTER
#define MOVE_FASTER(from,to,length)     MOV_faster (from, to, (ULONG) (length))
#endif

#ifndef MEMMOVE
/* Use character by character copy function */
#define MEMMOVE(from,to,length)       MOV_fast (from, to, (ULONG) (length))
#endif

#ifndef MOVE_CLEAR
#define MOVE_CLEAR(to,length)           MOV_fill (to, (ULONG) (length))
#endif

#ifndef ALLOC_LIB_MEMORY        
#define ALLOC_LIB_MEMORY(size)          gds__alloc (size)
#endif

#ifndef FREE_LIB_MEMORY
#define FREE_LIB_MEMORY(block)          gds__free (block)
#endif


#ifdef DEV_BUILD

/* Define any debugging symbols and macros here.  This 
   ifdef will be executed during development builds. */

#ifdef NETWARE_386
#define DEV_REPORT(msg)         ConsolePrintf (msg)
#define BREAKPOINT(x)           Breakpoint(x)
#define TRACE(msg)              ConsolePrintf (msg)
#endif

#ifdef WINDOWS_ONLY
#include <windows.h>
#define DEV_REPORT(msg)         OutputDebugString(msg)
#define TRACE(msg)              OutputDebugString(msg)
#endif

#ifdef WIN_NT
#define TRACE(msg)              gds__log (msg)
#define DEV_REPORT(msg)         gds__log (msg)
#endif

#ifndef TRACE
#define TRACE(msg)              ib_fprintf (ib_stderr, (msg))
#endif

#ifndef DEV_REPORT
#define DEV_REPORT(msg)         ib_fprintf (ib_stderr, (msg))
#endif

#ifndef BREAKPOINT
#define BREAKPOINT(x)           GDS_breakpoint(x)
#endif

#endif  /* DEV_BUILD */

#ifndef DEV_BUILD
#define DEV_REPORT(msg)         gds__log (msg)
#define BREAKPOINT(x)           /* nothing */
#define TRACE(msg)         	/* nothing */
#endif


/* shared library definitions */

#ifdef SHLIB_DEFS
#ifndef LOCAL_SHLIB_DEFS
#include "../jrd/shdef.h"
#endif
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT              
#endif


/* commonly used buffer sizes for dynamic buffer allocation */

#define BUFFER_XLARGE   2048
#define BUFFER_LARGE    1024
#define BUFFER_MEDIUM   512
#define BUFFER_SMALL    256
#define BUFFER_TINY     128

#ifndef DOUBLE_MULTIPLY
#define DOUBLE_MULTIPLY(a,b)    (((double) (a)) * ((double) (b)))
#endif

#ifndef DOUBLE_DIVIDE
#define DOUBLE_DIVIDE(a,b)      (((double) (a)) / ((double) (b)))
#endif

#ifndef ISC_EXT_LIB_PATH_ENV
#define ISC_EXT_LIB_PATH_ENV	"INTERBASE_EXT_LIB_PATH"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN		1024
#endif

#define ISC_STATUS_LENGTH	20

/* switch name and state table.  This structure should be used in all
 * command line tools to facilitate parsing options.*/
typedef struct in_sw_tab_t {
    int         in_sw;
    int         in_spb_sw;
    TEXT        *in_sw_name;
    ULONG	in_sw_value;			/* alice specific field */
    ULONG	in_sw_requires;			/* alice specific field */
    ULONG	in_sw_incompatibilities;	/* alice specific field */
    USHORT      in_sw_state;
    USHORT      in_sw_msg;
    USHORT      in_sw_min_length;
    TEXT	*in_sw_text;
    
    } *IN_SW_TAB;

#endif /* _JRD_COMMON_H_ */
