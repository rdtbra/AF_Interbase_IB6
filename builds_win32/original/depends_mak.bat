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

rem This batch file is used to modify the depends.mak file
rem in each component and make it usable on NT. The depends.mak
rem files are being generated on a UNIX system (Solaris)
rem This will allow some form of dependency checking while doing
rem MAKE on NT.
rem 
rem Script built by: Charlie Caro
rem Checked in by: B.Sriram, 08-Mar-1999

echo Expanding depends.mak

cd alice
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\burp
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\dsql
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\dudley
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\gpre
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\intl
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\isql
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\jrd
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\lock
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\msgs
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\qli
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\remote
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\utilities
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..\wal
sed -f ..\builds_win32\depends.sed depends.mak > .\sed.TMP
copy .\sed.TMP depends.mak
del .\sed.TMP

cd ..

