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
# 
# Assumes ROOT has been set to the top of the subdirectory in which
# the top level make takes place.
#
# Imports: LINCLUDEPATH, LLIBPATH, LCFLAGS, LLFLAGS, LOBJPATH
#-----------------------------------------------------------------------

# Automatically create dependencies for #included files
.AUTODEPEND

# Directory containing sources which are refreshed nightly from Marion
!if $d(WIN32)
!if !$d(DEVSRC)
DEVSRC=h:\nightly\build
!endif
!if !$d(TOOLPATH)
TOOLPATH=c:\bc5
!endif
!else
!if !$d(DEVSRC)
DEVSRC=g:\gds4
!endif
!if !$d(TOOLPATH)
TOOLPATH=c:\bc5
!endif
!endif

#Define a concatenate and file delete macro
CAT=    type
RM=     del

# The build_lib script passes in -DDEV so we will trap it and set the DEBUG
# macro.

!if $d(DEV)
DEBUG = 1
!endif
 
# Unfortunately, MAKEFLAGS does not prepend a '-', and does not pick
# up command line defines, so we will check for the ones we care about.

!if "$(MAKEFLAGS)" != ""
    MAKEFLAGS = -$(MAKEFLAGS)
!endif
!if $d(DEBUG) 
MAKEFLAGS = $(MAKEFLAGS) -DDEBUG 
!endif
!if $d(WIN)
MAKEFLAGS = $(MAKEFLAGS) -DWIN 
!endif


#Define the compiler and linker to use
!if $d(WIN32)
CC= bcc32
TLINK= tlink32
#kludge untill bc5 resource compiler is working
!if ! $d(BRC32)
RC= brc32
!else
RC= $(BRC32)
!endif
IMPLIB= implib
TASM= tasm32
!else
CC= bcc
TLINK= tlink
RC= brc
IMPLIB= implib
TASM= tasm
!endif

# Set up OS specific compiler flags and the directory for object files
#----------------------------- DOS --------------------------------------
!if $d(DOS)

DESTSTRING=Dos(DOS)
!if !$d(LOBJPATH)
.path.obj=bcc\dos
!else
.path.obj=$(LOBJPATH)
!endif
MODEL=l
OSFLAGS=-DDOS_ONLY -h -m$(MODEL)
LFLAGS=/Tde/c
LEFLAGS=/Tde/c

#--------------------------- WINDOWS ------------------------------------
!else

DESTSTRING=Windows(WIN)

!if $d(WIN32)
!if $d(DELPHI)
.path.obj=dp_obj\client
!else
.path.obj=bc_obj\client
!endif
!else
.path.obj=bcc\win
!endif

MODEL=l
WIN_DEFINES=-DDOS_ONLY;WINDOWS_ONLY

# Compile for windows DLL, SS!=DS
!if $d(WIN32)

DLL_OSFLAGS=-WD 
DLL_LFLAGS=/Tpd

APP_OSFLAGS=-W 
APP_LFLAGS=/Tpe
!else
DLL_OSFLAGS=-m$(MODEL)! -WD $(WIN_DEFINES)
DLL_LFLAGS=/Twd/P-/M

APP_OSFLAGS=-m$(MODEL) -WS $(WIN_DEFINES)
APP_LFLAGS=/Twe/P- -M
!endif

!endif

#Define the installation directories for DEBUG & non-DEBUG objects
!if $d(DEBUG)
!if $d(WIN32)
INSTALLBIN = $(ROOT)\ib_debug\bin
INSTALLLIB = $(ROOT)\ib_debug\lib
SHRLIB_PATH = $(ROOT)\jrd\ms_obj\clientd
GBKLIB_PATH = $(ROOT)\burp\$(.path.obj)
!else
INSTALLBIN = $(ROOT)\wind
INSTALLLIB = $(INSTALLBIN)
!endif
!else
!if $d(WIN32)
INSTALLBIN = $(ROOT)\interbase\bin
INSTALLLIB = $(ROOT)\interbase\lib
INSTALLINC = $(ROOT)\interbase\include
INSTALLEXAMPLE = $(ROOT)\interbase\examples
SHRLIB_PATH = $(ROOT)\jrd\ms_obj\client
GBKLIB_PATH = $(ROOT)\burp\$(.path.obj)
!else
INSTALLBIN = $(ROOT)\interbas\bin
INSTALLLIB = $(ROOT)\interbas\lib
INSTALLINC = $(ROOT)\interbas\include
INSTALLEXAMPLE = $(ROOT)\interbas\examples
!endif
!endif


# If debug is defined, turn on compiler/linker flags & set obj directory
#----------------------------- DEBUG -------------------------------------
!if $d(DEBUG)
DESTSTRING=$(DESTSTRING)_Debug(D)
.path.obj=$(.path.obj)d
CDB=-v -N -vi- -Od -DDEBUG_GDS_ALLOC -DDEV_BUILD
!if $d(WIN32)
LDB=/v
!else
LDB=/v/s
!endif
!endif

# Set up default path for response files, libraries and executables
.path.rsp=$(.path.obj)
.path.dll=$(.path.obj)
.path.exe=$(.path.obj)
.path.lib=$(.path.obj)

#--------------------- COMPILER AND LINKER FLAGS ------------------------

# Set lib and include path, prepending any local path set in makefile
LIBPATH=$(TOOLPATH)\lib;$(.path.obj)
INCLUDEPATH=$(TOOLPATH)\include;$(ROOT)\jrd;$(ROOT)\interbase\include

!if $d(LLIBPATH)
LIBPATH=$(LLIBPATH);$(LIBPATH)
!endif
!if $d(LINCLUDEPATH)
INCLUDEPATH=$(LINCLUDEPATH);$(INCLUDEPATH)
!endif

# The Cx and Lx flags allow extra fl be enabled from the command line.
# e.g. make -DCx=-DSPX  will define SPX when compiling.
!if $d(WIN32)
DLL_CFLAGS=-g255 -w-stu -w-sus -w-pia -w-pro -w-par -a4 -3 -R -r- -d -n$(.path.obj) -I$(INCLUDEPATH) -DNOMSG -D_X86_ -DWIN32_LEAN_AND_MEAN $(DLL_OSFLAGS) $(CDB) $(Cx) 
DLL_LFLAGS=$(LDB) $(DLL_LFLAGS) /L$(LIBPATH) $(Lx)

APP_CFLAGS=-w-par -a4 -R -r- -d -n$(.path.obj) -I$(INCLUDEPATH) -DSUPERCLIENT -D_X86_ $(APP_OSFLAGS) $(CDB) $(Cx) $(LCFLAGS)
APP_LFLAGS=$(LDB) $(APP_LFLAGS) /L$(LIBPATH) $(Lx)
!else
DLL_CFLAGS=-g255 -w-stu -w-sus -w-pia -w-pro -w-par -3 -R -r- -d -Fc -Vf -n$(.path.obj) -I$(INCLUDEPATH) -DPC_PLATFORM $(DLL_OSFLAGS) $(CDB) $(Cx) $(LCFLAGS)
DLL_LFLAGS=$(LDB) $(DLL_LFLAGS) /L$(LIBPATH) $(Lx)

APP_CFLAGS=-w-par -R -r- -d -n$(.path.obj) -I$(INCLUDEPATH) -DPC_PLATFORM;REQUESTER $(APP_OSFLAGS) $(CDB) $(Cx) $(LCFLAGS)
APP_LFLAGS=$(LDB) $(APP_LFLAGS) /L$(LIBPATH) $(Lx)
!endif

!if $d(DELPHI)
DLL_CFLAGS = -u- $(DLL_CFLAGS) -DDELPHI_OBJ 
APP_CFLAGS = -u- $(APP_CFLAGS) -DDELPHI_OBJ 
!endif


#Define for the type of tools on this platform
!if $d(GUI_TOOLS)
DLL_CFLAGS = $(DLL_CFLAGS) -DGUI_TOOLS 
APP_CFLAGS = -WM- $(APP_CFLAGS) -DGUI_TOOLS 
!endif
!if $d(CONSOLE_TOOLS)
DLL_CFLAGS = $(DLL_CFLAGS) -DCONSOLE_TOOLS 
APP_CFLAGS = $(APP_CFLAGS) -DCONSOLE_TOOLS 
!endif

!if $d(APPBUILD)
CFLAGS=$(APP_CFLAGS)
LFLAGS=$(APP_LFLAGS)
!else
CFLAGS=$(DLL_CFLAGS)
LFLAGS=$(DLL_LFLAGS)
!endif

#--------------------------- DEFAULT RULES -------------------------------

.c.obj:
	$(CC) -c @&&<
		$(CFLAGS)
< $<

.cpp.obj:
   $(CC) -c @&&<
	  $(CFLAGS)
< $<

.asm.obj:
	$(TASM) -m$(MODEL) $. , $**


MAKE: showbuild all

showbuild:
	@echo Building: $(DESTSTRING)


makefile.tmp: makefile.mak
	copy makefile.mak makefile.tmp
	touch makefile.tmp
	del $(.path.rsp)\*.rsp


# Remove all the output files for the target environment (e.g. dos debug)
clean::
	$(RM) $(.path.obj)\*.*

# Remove the output files for ALL target environments
allclean::
	$(RM) bcc\dos\*.*
	$(RM) bcc\dosd\*.*
	$(RM) bcc\win\*.*
	$(RM) bcc\wind\*.*


srcs::
	if not exist bcc\win\*.* mkdir bcc
	if not exist bcc\win\*.* mkdir bcc\win
	if not exist bcc\wind\*.* mkdir bcc\wind
	if not exist $(ROOT)\interbase\*.* mkdir $(ROOT)\interbase
	if not exist $(ROOT)\interbase\bin\*.* mkdir $(ROOT)\interbase\bin
	if not exist $(ROOT)\interbase\lib\*.* mkdir $(ROOT)\interbase\lib
	if not exist $(ROOT)\interbase\include\*.* mkdir $(ROOT)\interbase
	if not exist $(ROOT)\wind\*.* mkdir $(ROOT)\wind
