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

IB_BIN_COMPS="alice burp dsql dudley example5 extlib gpre intl ipserver isql iscguard jrd lock msgs qli register remote utilities wal"

echo setting up source directories...
for comp in ${IB_BIN_COMPS}
do
    echo creating component $comp ...
    mkdir $comp/ms_obj
    mkdir $comp/ms_obj/bin
    mkdir $comp/ms_obj/bind
    mkdir $comp/ms_obj/client
    mkdir $comp/ms_obj/clientd
done

mkdir builds
mkdir builds/original
mkdir ib_debug
mkdir ib_debug/bin
mkdir ib_debug/lib
mkdir ib_debug/intl
mkdir ib_debug/UDF
mkdir ib_debug/include
mkdir ib_debug/examples
mkdir ib_debug/examples/v4
mkdir ib_debug/examples/v5
mkdir ib_debug/help

setup_build

echo done

