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
#set these variables as desired to your build
DB_PATH="toolsdbs:/dbs/source/v6.0" #path to look for db in .e files
OS_NAME="WIN_NT" #OS name for marion, and directory for builds components
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

function echo_syntax {
   echo 'REFRESH.KSH: Local InterBase for Win32 refresh script.'
   echo 'SYNTAX: refresh [-NOBUILDS] [-NOCOPY] [-FORCE] [-LOCAL_METADATA] '
   echo '                [-SINCE:dd-mon-yyyy] [ -COMPONENT:component ... | -NOCOMPONENTS ]'
   echo '	-NOBUILDS do not refresh build files (-NB)'
   echo '	-NOCOPY copy things out of win_nt/original, etc (-NC)'
   echo '	-FORCE refresh all files, regardless of date (-F)'
   echo '	-LOCAL_METADATA use local metadata DB for GPRE pre-processing (-LM)'
   echo '	-SINCE:dd-mon-yyyy refresh all files changed since date (-S:)'
   echo '	-COMPONENT:component refresh only files in component (-C:)'
   echo '	-NOCOMPONENTS refresh only files in $OS_NAME/original (-N)'
   echo 'NOTES: Arguments are case sensitive. Argument order does not matter'
   echo '	You may specify more than one -C: option for multiple components.'
   echo '	The -SINCE option over-rides -FORCE.'
   echo '	In -SINCE dd is [1-31] mon is Jan Feb etc. Century is required.'
   echo '	last_fetch is only updated for full refresh (eg, without -C:,-N, -NC or -NB)'
   echo '	Refresh assumes each component contains a proper marion.arg.'
   echo 'REFRESH without options refreshes files changed since the date in last_fetch'
}

#set default command line arguments
FORCE=FALSE
NOCOMPONENTS=FALSE
BUILDS=TRUE
COPYBUILDS=TRUE
LOCAL_METADATA=FALSE
SINCE=
BITMAP=FALSE
BITMAPROOT=
COMPONENTS=
MAPPED_G_DRIVE=FALSE

#parse command line
for arg in $*
do
    echo OPTION:\'$arg\'
    MATCH=FALSE
    case ${arg} in
        ('-NOBUILDS')
              BUILDS=FALSE
              MATCH=TRUE
              echo SUPRESS REFRESH OF ${OS_NAME}/original
        ;;
        ('-NOCOPY')
              COPYBUILDS=FALSE
              MATCH=TRUE
              echo SUPRESS COPY OF BUILD FILES OUTSIDE OF ${OS_NAME}/oringal
        ;;
        ('-LOCAL_METADATA')
              LOCAL_METADATA=TRUE
              MATCH=TRUE
              echo Use local metadata database file for GPRE pre-processing
        ;;

        ('-NOCOMPONENTS')
              NOCOMPONENTS=TRUE
              MATCH=TRUE
              echo SUPRESS REFRESH OF COMPONENTS
        ;;
        ('-FORCE')
              FORCE=TRUE
              MATCH=TRUE
              echo IGNORING LAST_FETCH DATE
        ;;
        ('-SINCE:'*[1-9]-[A-Z][a-z][a-z]-[1-2][0-9][0-9][0-9])
              SINCE=${arg#'-SINCE:'}
              echo REFRESHING FILES CHANGED SINCE: $SINCE
              SINCE="-s ${SINCE}"
              MATCH=TRUE
        ;;
        ('-COMPONENT:'*) 
              COMPONENTS="${COMPONENTS} "${arg#'-COMPONENT:'}
              MATCH=TRUE
        ;;
        ('-BITMAP:'*) 
              BITMAPROOT="${arg#'-BITMAP:'}"
	      BITMAP=TRUE
              MATCH=TRUE
        ;;
        ('-NB')
              BUILDS=FALSE
              MATCH=TRUE
              echo SUPRESS REFRESH OF BUILD FILES IN ${OS_NAME}/ORIGINAL
        ;;
        ('-NC')
              COPYBUILDS=FALSE
              MATCH=TRUE
              echo SUPRESS COPY OF BUILD FILES OUTSIDE OF ${OS_NAME}/ORIGINAL
        ;;
        ('-LM')
              LOCAL_METADATA=TRUE
              MATCH=TRUE
              echo Use local metadata database file for GPRE pre-processing
        ;;
        ('-N')
              NOCOMPONENTS=TRUE
              MATCH=TRUE
              echo SUPRESS REFRESH OF COMPONENTS
        ;;
        ('-F')
              FORCE=TRUE
              MATCH=TRUE
              echo IGNORING LAST_FETCH DATE
        ;;
        ('-S:'*[1-9]-[A-Z][a-z][a-z]-[1-2][0-9][0-9][0-9])
              SINCE=${arg#'-S:'}
              echo REFRESHING FILES CHANGED SINCE: $SINCE
              SINCE="-s ${SINCE}"
              MATCH=TRUE
        ;;
        ('-C:'*) 
              COMPONENTS="${COMPONENTS} "${arg#'-C:'}
              MATCH=TRUE
        ;;
        ('-BM:'*) 
              BITMAPROOT="${arg#'-BM:'}"
	      BITMAP=TRUE
              MATCH=TRUE
        ;;
    esac
    if [ "${MATCH}" = "FALSE" ] 
    then 
        echo ERROR: INCOMPREHENSABLE ARGUMENT $arg 1>&2
        echo_syntax 1>&2
        exit 1
    fi
done

if [ "${BITMAP}" = "TRUE" ] 
then
    if [ "${BITMAPROOT}" = "" ]
    then
	BITMAPROOT=z:/build_resources
	echo REFRESH: USING DEFAULT BITMAP SOURCE DIRCTORY ${BITMAPROOT}
	if ! [ -d z:/ ]
	then
	    echo REFRESH: ATTEMPTING to MAP Z: to IBNTSRV/RD for bitmaps
	    net use /yes z: \\\\IBNTSRV\\RD
	    if ! [ -d z:/ ]
	    then 
		echo REFRESH: MAP Z: IBNTSRV/RD FAILED 1>&2
		echo_syntax 1>&2
		exit 1;
	    fi
	    echo REFRESH: z: MAPPPED TO IBNTSRV/RD 
	    MAPPED_G_DRIVE=TRUE
	fi
    fi
    if ! [ -d "${BITMAPROOT}/svrmgr" -o -d "${BITMAPROOT}/wisql" ] 
    then
	echo ERROR: ${BITMAPROOT} is not an interbase tree. SVRMGR or WISQL not found 1>&2
	echo_syntax 1>&2
	exit 1
    fi
    echo REFRESH: fetching bitmaps from ${BITMAPROOT}/svrmgr, ${BITMAPROOT}/wisql, ${BITMAPROOT}/commdiag, and ${BITMAPROOT}/regcfg
    cp ${BITMAPROOT}/remote/*.ico remote
    cp ${BITMAPROOT}/jrd/*.cur jrd
    cp ${BITMAPROOT}/iscguard/*.ico iscguard

    if [ "${MAPPED_G_DRIVE}" = "TRUE" ]
    then
	echo REFRESH: unmapping z:
	net use z: /DELETE /YES
    fi
fi

if [ "${COMPONENTS}" ]
then
     echo REFRESHING COMPONENTS: $COMPONENTS
     IB_COMPONENTS=$COMPONENTS
fi

if [ "${NOCOMPONENTS}" = "TRUE" ]
then
     COMPONENTS=
     IB_COMPONENTS=
fi

if ! [ "${FORCE}" = "TRUE" ] 
then
    if ! [ -f last_fetch ]
    then
        echo ERROR: last_fetch file not found. Use refresh -F
        echo REFRESH FAILED
        echo_syntax 1>&2
        exit 1
    fi
    if ! [ "$SINCE" ] 
    then
        SINCE=$(cat last_fetch)
        echo REFRESHING FILES CHANGED SINCE $SINCE
        SINCE="-s ${SINCE}"
    fi
fi

cd $OS_NAME/original
#refresh build files in original
if [ "${BUILDS}" = "TRUE" ]
then
	echo Fetching BUILD_WIN32
	marion -v -a $SINCE -o $OS_NAME -x
fi

if [ "${COPYBUILDS}" = "TRUE" ]
then
    echo Delivering files from ${OS_NAME}/orginal
    #build expand_dbs.bat and compress_dbs.bat in root build dir
    echo CREATING expand_dbs and compress_dbs
    sed 's/$(SYSTEM)/'${OS_NAME}/ expand_dbs.bat > ../../expand_dbs.bat
    sed 's/$(SYSTEM)/'${OS_NAME}/ compress_dbs.bat > ../../compress_dbs.bat
    #build expand.sed and compress.sed
    sed 's@$(DB_DIR)@'${DB_PATH}@ expand.sed > ../expand.sed
    sed 's@$(DB_DIR)@'${DB_PATH}@ compress.sed > ../compress.sed
    #check if user wants local metadata database
    if [ "${LOCAL_METADATA}" = "TRUE" ]
    then
	sed 's@$(DB_DIR)@'${DB_PATH}@ expand_local.sed > ../expand.sed
	sed 's@$(DB_DIR)@'${DB_PATH}@ compress_local.sed > ../compress.sed
	echo Creating local METADATA.GDB for GPRE, also known as yachts.gdb
	isql -i metadata.sql
    fi
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
    echo DELIVERING MARION FILES
    cp *template.bat ../..
fi
cd ../..

#refresh component files
echo REFRESHING COMPONENTS
for comp in ${IB_COMPONENTS}
do
    HEADER_FLAG=
    for check_header in ${NO_HEADER_COMPS}
    do
        if [ "${check_header}" = "${comp}" ]
        then
            HEADER_FLAG=-x
            echo REFRESH: Striping headers from files in $comp
        fi
    done
    if [ -d "${comp}" ]
    then
        echo REFRESH: Fetching $comp
        cd $comp
        marion -v -a $SINCE -o $OS_NAME $HEADER_FLAG
        if [ "${BUILDS}" = "TRUE" -a -f "../${OS_NAME}/original/make.${comp}" ]
        then
            echo REFRESH: Delivering ../${OS_NAME}/original/make.${comp} '->' makefile.lib
            cp ../${OS_NAME}/original/make.${comp} makefile.lib
        fi
        cd ..
    elif [ "${comp}" = "outliner" ]
    then
	echo REFRESH:Fetching outliner to svrmgr/outliner
	cd svrmgr/outliner
        marion -v -a $SINCE -o $OS_NAME $HEADER_FLAG
        cd ../..
    else
        echo WARNING: Component directory $comp not found. Files not refreshed.
    fi
done

#Modify component depends.mak files for NT
./depends_mak.bat

if [ "${COMPONENTS}" = "" -a "${BUILDS}" = "TRUE" -a "${COPYBUILDS}" = "TRUE"  ]
then
    echo STORING FETCH DATE $(date +"%d-%h-%Y")
    date +"%d-%h-%Y" >last_fetch
fi
echo REFRESH OK
exit 0
