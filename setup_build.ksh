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
##########################################################################
#set -x
DB_PATH="toolsdbs:/dbs/source/v6.0" #path to look for db in .e files
OS_NAME="builds_win32" # directory for builds_win32 components
#Components for refresh
#note that outliner is a special case, see main loop
#bitmaps/icons for the GUI tools are handled seperatly. they are assumed to
#reside in an interbase tree in svrmgr, wisql and svrmgr/outliner
#if BITMAP is turned on, we copy svrmgr/*.bmp, svrmgr/*.ico svrmgr/outliner/*.bmp
#and wisql/*.ico. if icons or bitmaps are added in other dirctories, they
#should be added to the if bitmap section.
IB_COMPONENTS="alice burp dsql dudley example5 extlib gpre intl ipserver isql iscguard jrd lock msgs qli remote register utilities wal"
#Files to use from BUILD component
#Components for marion to extract without headers
NO_HEADER_COMPS="examples example5"

cd builds_win32/original

    echo Delivering files from builds_win32/original
    #build expand_dbs.bat and compress_dbs.bat in root build dir
    echo CREATING expand_dbs and compress_dbs
    sed 's/$(SYSTEM)/'builds_win32/ expand_dbs.bat > ../../expand_dbs.bat
    sed 's/$(SYSTEM)/'builds_win32/ compress_dbs.bat > ../../compress_dbs.bat
    #build expand.sed and compress.sed
    sed 's@$(DB_DIR)@'builds_win32@ expand.sed > ../expand.sed
    sed 's@$(DB_DIR)@'builds_win32@ compress.sed > ../compress.sed
    #check if user wants local metadata database
    sed 's@$(DB_DIR)@'${DB_PATH}@ expand_local.sed > ../expand.sed
    sed 's@$(DB_DIR)@'${DB_PATH}@ compress_local.sed > ../compress.sed
    echo Creating local METADATA.GDB for GPRE, also known as yachts.gdb
    isql -i metadata.sql

    echo DELIVERING ROOT BUILD FILES
    cp build_lib.bat include.mak std.mk ../..
    echo DELIVERING MISC FILES
    cp gdsalias.asm ../../jrd
    cp gdsintl.bind ../../intl/gdsintl.bind
    cp gds32.bind ../../jrd
    cp jrd32.bind ../../jrd/ibeng32.bind
    cp expand_cfile.bat ../..
    cp depends.sed ../
    cp depends_mak.bat ../..
    cp build_no.ksh ../..
    cp debug_entry.bind ../../jrd
    cp iblicen.bind     ../../register/iblicen.bind
    cp ib_udf.bind     ../../extlib/ib_udf.bind
    cp ib_util.bind     ../../extlib/ib_util.bind
    echo DELIVERING '*template.bat' FILES
    cp *template.bat ../..

cd ../..

for comp in ${IB_COMPONENTS}
do
    if [ -d "${comp}" ]
    then
        cd ${comp}
        if [ -f "../builds_win32/original/make.${comp}" ]
        then
            echo Delivering ../builds_win32/original/make.${comp} '->' makefile.lib
            cp ../builds_win32/original/make.${comp} makefile.lib
        fi
        cd ..
    fi
done

#Modify component depends.mak files for NT
./depends_mak.bat

echo SETUP_BUILD  OK
exit 0
