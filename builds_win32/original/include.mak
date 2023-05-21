# The contents of this file are subject to the Interbase Public
# License Version 1.0 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy
# of the License at http://www.Inprise.com/IPL.html
#
# Software distributed under the License is distributed on an
# "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
# or implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code was created by Inprise Corporation
# and its predecessors. Portions created by Inprise Corporation are
# Copyright (C) Inprise Corporation.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
#------------------------------ INCLUDE.MAK ------------------------------------#
#	This is the common makefile that is included in the component
#	makefiles.  This set of makefiles is designed to work with
#	Borland Make, and will use both Borland and Microsoft compilers.
#
#	There are three command-line defines flags that are used by the
#	makefiles:
#		-DDEV - build a dev build (default: production build)
#		-DBORLAND - use Borland C (default: Microsoft C)
#		-DCLIENT - build client side stuff (default: server)
#
#	Depending on the options passed in, the output files go in 
#	different directories.  This allows one source tree to
#	target DEV and PROD builds on multiple compilers without
#	clobbering eachother.
#
#	The last complexity is the CLIENT vs SERVER libraries.  If you need
#	to link a program, use one of the following link flags.
#		SHRLIB_LINK = client link library
#		SVRLIB_LINK = server link library
#
#	This is NOT tested with Borland C, if you want to build using Borland C,
#	you'll need to do a little work.
#
#-------------------------------------------------------------------------------#



#
# Turn on automatic dependency checking.  Only seems to work with Borland C.
#
.autodepend
# Change this macro to use a different unix system for some things
# Most of this is setup at refresh or setup time, so set setup_dirs.ksh and refrsh.ksh
UNIX_DEV_ROOT=toolsdbs:/dbs/source/v6.0

LOCAL_DBA_PASSWORD=masterke

# Unfortunately, MAKEFLAGS does not prepend a '-', and does not pick
# up command line defines, so we will check for the ones we care about.

!if "$(MAKEFLAGS)" != ""
     MAKEFLAGS = -$(MAKEFLAGS)
!endif

ROOT = ..
!if ! $d(IBSERVER)
     IBSERVER = \ibserver
!endif

#
# Set the COMPILER variable, default to Microsoft C.
#
!if $d(BORLAND)
!undef BORLAND
COMPILER=BC
RSP_CONTINUE=+
!else
!if $d(DELPHI)
COMPILER=DP
RSP_CONTINUE=
!else
COMPILER=MS
RSP_CONTINUE=
!endif
!endif

#
# Set the Version variable, default to PROD_BUILD.
# 	Check for the -DCLIENT switch, and alter the .path.obj accordingly
#
!if $d(DEV)
!undef DEV
VERSION=        DEV
VERSION_FLAG=   -DDEV_BUILD
INSTALL_ROOT=   ..\IB_DEBUG
#DEBUG_OBJECTS= grammar.obj dbg.obj dbt.obj dmp.obj
DEBUG_OBJECTS=  nodebug.obj
!if $d(CLIENT)
.path.obj=$(COMPILER)_obj\clientd
MAKEFLAGS = $(MAKEFLAGS) -DDEV -DCLIENT  
!else
.path.obj=$(COMPILER)_obj\bind
MAKEFLAGS = $(MAKEFLAGS) -DDEV 
!endif
!else
VERSION=        PROD
VERSION_FLAG=   -DPROD_BUILD
INSTALL_ROOT=   ..\INTERBASE
DEBUG_OBJECTS=  nodebug.obj
!if $d(CLIENT)
.path.obj=$(COMPILER)_obj\client
MAKEFLAGS = $(MAKEFLAGS) -DPROD -DCLIENT  
!else
.path.obj=$(COMPILER)_obj\bin
MAKEFLAGS = $(MAKEFLAGS) -DPROD
!endif
!endif

!if $d(XNET)
MAKEFLAGS = $(MAKEFLAGS) -DXNET
VERSION_FLAG = $(VERSION_FLAG) -DXNET
XNETFLAG=  -DXNET
!else
XNETFLAG=
!endif

!if $d(GEN_CEDRT)
VERSION_FLAG = $(VERSION_FLAG) -DGEN_CERT
!endif

!if !$d(BRC32)
BRC32=			brc32.exe
!endif
#
# Borland C
#	Setup the macros for the Borland C compiler.
#	
!if $(COMPILER) == BC
!message Borland C Compiler
CC=                     bcc32
LINK=                   tlink32
IMPLIB=                 echo tlib
VENDOR_CFLAGS=          -w- -g0 -4 -pc -N -a -WM -WD -I..\interbase\include -n$(.path.obj)
O_OBJ_SWITCH=           -o
O_EXE_SWITCH=           -e
DLLENTRY=
CONLIBSDLL=             
ADVAPILIB=
MPRLIB=
WSOCKLIB=

WIN_NT_GDSSHR=          gds32_nt_bc4.dll
SHRLIB_LINK=            $(ROOT)\jrd\$(.path.obj)\gds32.lib
WIN_NT_GDSINTL=         gdsintl_nt_bc4.dll

!if $(VERSION) == PROD
VERSION_CFLAGS=         -DFLINTSTONE
LINK_OPTS=              -WM
!else
VERSION_CFLAGS=         -v
LINK_OPTS=              -WM
LD_OPTS=                -v
!endif
!endif

#
# Microsoft C
#	Setup the macros for the Microsoft C compiler.
#
!if $(COMPILER) == MS
!message Microsoft C Compiler
CC=                     cl
LINK=                   link -machine:i386
IMPLIB=                 lib -machine:i386
VENDOR_CFLAGS=          -W3 -G4 -Gd -MD -I..\interbase\include -Fo$(.path.obj)\ -DWIN32_LEAN_AND_MEAN
O_OBJ_SWITCH=           -Fo
O_EXE_SWITCH=           -o
DLLENTRY=               @12
CONLIBSDLL=             msvcrt.lib kernel32.lib
ADVAPILIB=              advapi32.lib
MPRLIB=                 mpr.lib
WSOCKLIB=               wsock32.lib

SVR_STATIC_LIB=		eng32lib.lib
CLIENT_STATIC_LIB=	gds32lib.lib
WIN_NT_GDSSHR=          gds32_nt_ms.dll
SHRLIB_LINK=            $(ROOT)\jrd\$(.path.obj)\gds32_ms.lib
SVRLIB_LINK=		$(ROOT)\jrd\$(.path.obj)\$(SVR_STATIC_LIB)
WIN_NT_GDSINTL=         gdsintl_nt_ms.dll
VER_RES_LINK=		/link $(ROOT)\jrd\$(.path.obj)\$(VERSION_RES)

!if $(VERSION) == PROD
# Microsoft C
#VERSION_CFLAGS=	-Ob2gtp -DWIN95
#Comment the line above and
#uncomment the next line if building with VISUAL C++ 5.0
VERSION_CFLAGS=		-Ob2iytp -Gs -DWIN95

#Compile the server to have a 2meg stack.  This is needed for some
#queries that require deep recursion to complete
LINK_OPTS=		-stack:2097152

!else
VERSION_CFLAGS=         -Zi -FR -DWIN95
LINK_OPTS=              -debug:full -debugtype:cv 
!endif
!endif

!ifdef CLIENT
CFLAGS= $(VERSION_CFLAGS) $(VENDOR_CFLAGS) -DSERVER_SHUTDOWN -DIPSERV -DSUPERCLIENT -DGOVERNOR -DNOMSG -D_X86_=1 -DWIN32 -DI386 -DEXACT_NUMERICS
!else
CFLAGS= $(VERSION_CFLAGS) $(VENDOR_CFLAGS) -DSERVER_SHUTDOWN -DSUPERSERVER -DGOVERNOR -DNOMSG -D_X86_=1 -DWIN32 -DI386 -DEXACT_NUMERICS
!endif

!ifdef MFC
CFLAGS= $(CFLAGS) -D_WINDOWS -D_AFXDLL
!endif

GDSSHR_LINK=            $(SHRLIB_LINK) $(CONLIBSDLL)
VERSION_RC=		version_95.rc
VERSION_RES=		version_95.res

.path.res=		$(.path.obj)
.rc.res:
	$(BRC32) -r -w32 -fo$@ $<

.SUFFIXES: .c .e
.e.c:
	$(CP) $< $*_tmp.e
	$(EXPAND_DBNAME) $*_tmp.e
	$(GPRE) $(GPRE_FLAGS) $*_tmp.e $*.c 
	$(RM) $*_tmp.e

.SUFFIXES: .bin .obj .c .cpp
.c.obj:
	$(CC) -c @&&?
	$(CFLAGS)
	$(VERSION_FLAG) $<
?

.c.bin:
	$(CC) -c $(PIC_FLAGS) $(VERSION_FLAG) $(O_OBJ_SWITCH)$*.bin $<

.cpp.obj:
	$(CC) -c @&&?
	$(CFLAGS)
	$(VERSION_FLAG) $<
?
#------------------------- Misc Utilities --------------------------------
COMPRESS_DBNAME=        $(ROOT)\compress_dbs
EXPAND_DBNAME=          $(ROOT)\expand_dbs


#------------------------- Directory macros ------------------------------
MSGSDIR=		toolsdbs:/dbs/source/v6.0/msgs
#EXAMPLES_DBS=          $(DB_DIR)\examples\
HLPDIR=                 $(DB_DIR)\qli\
JRNDIR=                 $(DB_DIR)\journal\
#MSGSDIR=                $(DB_DIR)\msgs\

#------------------------- Target Names ----------------------------------
NT_EXAMPLES=            nt_examples
NT_SERVER=              nt_server.exe
REG_HELP=               isc_ins_hlp.dat

#------------------------- OS macro defines ------------------------------
SH=                     echo
RM=                     -del /Q
MV=                     move
TOUCH=                  touch
CP=                     copy
ECHO=                   echo
GPRE=                   $(IBSERVER)\bin\gpre.exe
ISQL=			$(IBSERVER)\bin\isql.exe -sql_dialect 1
GSEC=			$(IBSERVER)\bin\gsec.exe
GBAK=			$(IBSERVER)\bin\gbak.exe
QUIET_ECHO=             @echo
CD=                     cd
CAT=                    cat

#------------------------- Maintenance Rules ------------------------------

clean::
	$(RM) $(.path.obj)\*.*

allclean::
	$(RM) ms_obj\bind\*.*
	$(RM) ms_obj\bin\*.*
	$(RM) ms_obj\clientd\*.*
	$(RM) ms_obj\client\*.*
	$(RM) bc_obj\bind\*.*
	$(RM) bc_obj\bin\*.*
	$(RM) bc_obj\clientd\*.*
	$(RM) bc_obj\client\*.*

# Should remove the following after we are stable
#INTL=                   intl
#INTL_CFLAGS=            $(CFLAGS)
#INTL_MISC=              $(INTL_OBJECTS)
#INTL_TARGET=            gdsintl.dll
#ACCESS_METHOD=          gdslib.win_nt
#FORM_OBJECTS=           noform.o
#GDS_LINK=               $(GDSSHR_LINK)
#GDSLIB_BACKEND=         ..\jrd\gds_b.lib
#LANGUAGES=              cc vms_cxx make9 gdl1
#REMOTE_BURP_LINK=       ..\remote\$(.path.obj)\xdr.obj
#REMOTE_GDSSHR=          $(GDSSHR)
#REMOTE_GDSSHR_LINK=     $(SVRLIB_LINK) $(CONLIBSDLL) $(WSOCKLIB) $(ADVAPILIB) $(MPRLIB)
#IO_OBJECTS=             winnt.obj
#CHMOD=                  echo chmod
#CHMOD_6=                echo chmod 666
#CHMOD_7=                echo chmod 777
#CHMOD_S7=               echo chmod 0677
#YACC=                   echo
#SPECIAL_OPT=            sh ..\special_opt
#SHRLIB_LINK=            ..\interbase\lib\gds32_ms.lib
#GDSSHR=                 ..\interbase\bin\gds32.dll
#GDSLIB_LINK=            ..\jrd\gds_b.lib $(CONLIBSDLL) $(WSOCKLIB) $(ADVAPILIB) $(MPRLIB)
#-------------------------




