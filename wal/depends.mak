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
# depends.mak - wal
# Created by 'make depends.mak'
# Created on 1998-11-17
divorce.o: divorce.c
divorce.o: source/jrd/common.h
divorce.o: source/jrd/divor_proto.h
driver.o: driver.c
driver.o: source/jrd/blk.h
driver.o: source/jrd/common.h
driver.o: source/jrd/dsc.h
driver.o: source/jrd/fil.h
driver.o: source/jrd/flags.h
driver.o: source/jrd/gds_proto.h
driver.o: source/jrd/gdsassert.h
driver.o: source/jrd/isc.h
driver.o: source/jrd/isc_f_proto.h
driver.o: source/jrd/jrd.h
driver.o: source/jrd/jrn.h
driver.o: source/jrd/misc.h
driver.o: source/jrd/misc_proto.h
driver.o: source/jrd/thd.h
driver.o: wal.h
driver.o: wal_proto.h
driver.o: walc_proto.h
driver.o: walf_proto.h
driver.o: walr_proto.h
driver.o: wstat_proto.h
wal.o: source/jrd/blk.h
wal.o: source/jrd/codes.h
wal.o: source/jrd/common.h
wal.o: source/jrd/dsc.h
wal.o: source/jrd/fil.h
wal.o: source/jrd/gds_proto.h
wal.o: source/jrd/gdsassert.h
wal.o: source/jrd/iberr_proto.h
wal.o: source/jrd/isc.h
wal.o: source/jrd/isc_proto.h
wal.o: source/jrd/isc_s_proto.h
wal.o: source/jrd/jrd.h
wal.o: source/jrd/jrn.h
wal.o: source/jrd/misc.h
wal.o: source/jrd/thd.h
wal.o: source/jrd/thd_proto.h
wal.o: source/jrd/time.h
wal.o: wal.c
wal.o: wal.h
wal.o: wal_proto.h
wal.o: walc_proto.h
wal_prnt.o: source/jrd/common.h
wal_prnt.o: source/jrd/dsc.h
wal_prnt.o: source/jrd/fil.h
wal_prnt.o: source/jrd/gds_proto.h
wal_prnt.o: source/jrd/isc.h
wal_prnt.o: source/jrd/isc_f_proto.h
wal_prnt.o: source/jrd/misc.h
wal_prnt.o: source/jrd/thd.h
wal_prnt.o: wal.h
wal_prnt.o: wal_prnt.c
wal_prnt.o: wal_proto.h
wal_prnt.o: wstat_proto.h
walc.o: source/jrd/blk.h
walc.o: source/jrd/codes.h
walc.o: source/jrd/common.h
walc.o: source/jrd/dsc.h
walc.o: source/jrd/fil.h
walc.o: source/jrd/file_params.h
walc.o: source/jrd/flags.h
walc.o: source/jrd/gds_proto.h
walc.o: source/jrd/gdsassert.h
walc.o: source/jrd/iberr_proto.h
walc.o: source/jrd/isc.h
walc.o: source/jrd/isc_i_proto.h
walc.o: source/jrd/isc_proto.h
walc.o: source/jrd/isc_s_proto.h
walc.o: source/jrd/isc_signal.h
walc.o: source/jrd/jrd.h
walc.o: source/jrd/jrn.h
walc.o: source/jrd/llio.h
walc.o: source/jrd/llio_proto.h
walc.o: source/jrd/misc.h
walc.o: source/jrd/thd.h
walc.o: source/jrd/time.h
walc.o: wal.h
walc.o: walc.c
walc.o: walc_proto.h
walc.o: walf_proto.h
walf.o: source/interbase/include/iberror.h
walf.o: source/jrd/common.h
walf.o: source/jrd/dsc.h
walf.o: source/jrd/fil.h
walf.o: source/jrd/gds.h
walf.o: source/jrd/gds_proto.h
walf.o: source/jrd/iberr_proto.h
walf.o: source/jrd/isc.h
walf.o: source/jrd/llio.h
walf.o: source/jrd/llio_proto.h
walf.o: source/jrd/misc.h
walf.o: source/jrd/misc_proto.h
walf.o: source/jrd/thd.h
walf.o: wal.h
walf.o: walf.c
walf.o: walf_proto.h
walr.o: source/jrd/codes.h
walr.o: source/jrd/common.h
walr.o: source/jrd/dsc.h
walr.o: source/jrd/fil.h
walr.o: source/jrd/gds_proto.h
walr.o: source/jrd/iberr_proto.h
walr.o: source/jrd/isc.h
walr.o: source/jrd/llio.h
walr.o: source/jrd/llio_proto.h
walr.o: source/jrd/misc.h
walr.o: source/jrd/thd.h
walr.o: wal.h
walr.o: walf_proto.h
walr.o: walr.c
walr.o: walr_proto.h
walw.o: source/jrd/blk.h
walw.o: source/jrd/build_no.h
walw.o: source/jrd/codes.h
walw.o: source/jrd/common.h
walw.o: source/jrd/dsc.h
walw.o: source/jrd/fil.h
walw.o: source/jrd/gds_proto.h
walw.o: source/jrd/gdsassert.h
walw.o: source/jrd/iberr.h
walw.o: source/jrd/iberr_proto.h
walw.o: source/jrd/isc.h
walw.o: source/jrd/isc_i_proto.h
walw.o: source/jrd/isc_s_proto.h
walw.o: source/jrd/jrd.h
walw.o: source/jrd/jrn.h
walw.o: source/jrd/jrn_proto.h
walw.o: source/jrd/license.h
walw.o: source/jrd/llio.h
walw.o: source/jrd/llio_proto.h
walw.o: source/jrd/misc.h
walw.o: source/jrd/misc_proto.h
walw.o: source/jrd/thd.h
walw.o: source/jrd/time.h
walw.o: wal.h
walw.o: wal_proto.h
walw.o: walc_proto.h
walw.o: walf_proto.h
walw.o: walw.c
walw.o: walw_proto.h
wstatus.o: source/jrd/common.h
wstatus.o: source/jrd/dsc.h
wstatus.o: source/jrd/isc.h
wstatus.o: source/jrd/misc.h
wstatus.o: source/jrd/thd.h
wstatus.o: wal.h
wstatus.o: walc_proto.h
wstatus.o: wstat_proto.h
wstatus.o: wstatus.c
