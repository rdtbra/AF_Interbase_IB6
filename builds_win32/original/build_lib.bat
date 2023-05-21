@echo off
rem The contents of this file are subject to the Interbase Public
rem License Version 1.0 (the "License"); you may not use this file
rem except in compliance with the License. You may obtain a copy
rem of the License at http://www.Inprise.com/IPL.html
rem
rem Software distributed under the License is distributed on an
rem "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
rem or implied. See the License for the specific language governing
rem rights and limitations under the License.
rem
rem The Original Code was created by Inprise Corporation
rem and its predecessors. Portions created by Inprise Corporation are
rem Copyright (C) Inprise Corporation.
rem
rem All Rights Reserved.
rem Contributor(s): ______________________________________.
echo ON
del BUILD_FAIL
del BUILD_OK
SET FLAGS=%1 %2

set OLD_USER=%ISC_USER%
set OLD_PASSWORD=%ISC_PASSWORD%

set ISC_USER=builder
set ISC_PASSWORD=builder

cd jrd
cp iberror.h ..\interbase\include
if errorlevel 1 goto run_codes
make %FLAGS% -fmakefile.lib dot_e_files
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib gds.h
if errorlevel 1 goto fail
cd ..

cd utilities
make %FLAGS% -fmakefile.lib utilities.rsp
make %FLAGS% -DCLIENT -fmakefile.lib utilities.rsp
if errorlevel 1 goto fail
cd ..

cd alice
make %FLAGS% -fmakefile.lib alice.rsp
if errorlevel 1 goto fail
cd ..

cd burp
make %FLAGS% -fmakefile.lib burp.rsp
if errorlevel 1 goto fail
cd..

cd ipserver
make %FLAGS% -fmakefile.lib ipserver.rsp
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib ipserver.rsp
if errorlevel 1 goto fail
cd ..

cd dsql
make %FLAGS% -fmakefile.lib dot_e_files
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib dsql.rsp
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib dsql.rsp
if errorlevel 1 goto fail
cd ..

cd lock
make %FLAGS% -fmakefile.lib lock.rsp
if errorlevel 1 goto fail
cd ..

cd remote
make %FLAGS% -DCLIENT -fmakefile.lib remote.rsp
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib remote.rsp
if errorlevel 1 goto fail
cd ..

cd wal
make %FLAGS% -fmakefile.lib wal.rsp
if errorlevel 1 goto fail
cd ..

cd jrd
make %FLAGS% -fmakefile.lib gds32.dll
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib gds32.dll
if errorlevel 1 goto fail
cd ..

cd intl
make %FLAGS% -fmakefile.lib gdsintl.dll
if errorlevel 1 goto fail
cd ..

cd remote
make %FLAGS% -fmakefile.lib nt_server.exe
if errorlevel 1 goto fail
cd ..

cd burp
make %FLAGS% -DCLIENT -fmakefile.lib burp.exe
if errorlevel 1 goto fail
cd ..

cd gpre
make %FLAGS% -DCLIENT -fmakefile.lib gpre.exe
if errorlevel 1 goto fail
cd ..

cd dudley
make %FLAGS% -DCLIENT -fmakefile.lib dudley.exe
if errorlevel 1 goto fail
cd ..

cd isql
make %FLAGS% -DCLIENT -fmakefile.lib isql.exe
if errorlevel 1 goto fail
cd ..

cd lock
make %FLAGS% -DCLIENT -fmakefile.lib iblockpr.exe
if errorlevel 1 goto fail
cd ..

cd qli
make %FLAGS% -DCLIENT -fmakefile.lib qli.exe
if errorlevel 1 goto fail
cd ..

cd utilities
make %FLAGS% -DCLIENT -fmakefile.lib print_pool.exe
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib gsec.exe
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib install_reg.exe
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib install_svc.exe
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib dba.exe
if errorlevel 1 goto fail
cd ..

cd msgs
make %FLAGS% -DCLIENT -fmakefile.lib all
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib interbase.msg
if errorlevel 1 goto fail
cd ..

cd alice
make %FLAGS% -fmakefile.lib -DCLIENT gfix.exe
if errorlevel 1 goto fail
cd ..

cd iscguard
make %FLAGS% -fmakefile.lib -DCLIENT iscguard.exe
if errorlevel 1 goto fail
cd ..

cd extlib
make %FLAGS% -fmakefile.lib ib_util.dll 
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib ib_udf.dll 
if errorlevel 1 goto fail
cd ..

cd example5
make %FLAGS% -DCLIENT -fmakefile.lib all
if errorlevel 1 goto fail
cd ..

rem install built files, and misc
cd jrd
make %FLAGS% -fmakefile.lib install_server
if errorlevel 1 goto fail
make %FLAGS% -DCLIENT -fmakefile.lib install_client
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib install_config
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib isc4.gdb
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib include
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib install_qli_help
if errorlevel 1 goto fail
make %FLAGS% -fmakefile.lib install_messages
if errorlevel 1 goto fail
cd ..

cd example5
make %FLAGS% -fmakefile.lib install
if errorlevel 1 goto fail
cd ..

goto success

:success
ECHO SUCCESS
ECHO OK>BUILD_OK
goto end

:run_codes
echo You need to run codes.exe in JRD before building

:fail
cd ..
rem restore users INCLUDE environment variable if GUI build failed
if "%OLD_INCLUDE%"=="" goto include_ok
set INCLUDE=%OLD_INCLUDE%
set OLD_INCLUDE=
:include_ok

ECHO FAIL
ECHO FAIL>BUILD_FAIL
goto end

:end
set ISC_USER=%OLD_USER%
set ISC_PASSWORD=%OLD_PASSWORD%

