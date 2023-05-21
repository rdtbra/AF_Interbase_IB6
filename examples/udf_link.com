$!  The contents of this file are subject to the Interbase Public
$!  License Version 1.0 (the "License"); you may not use this file
$!  except in compliance with the License. You may obtain a copy
$!  of the License at http://www.Inprise.com/IPL.html
$!
$!  Software distributed under the License is distributed on an
$!  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
$!  or implied. See the License for the specific language governing
$!  rights and limitations under the License.
$!
$!  The Original Code was created by Inprise Corporation
$!  and its predecessors. Portions created by Inprise Corporation are
$!  Copyright (C) Inprise Corporation.
$!
$!  All Rights Reserved.
$!  Contributor(s): ______________________________________.
$!
link/share=funclib.exe udf,  sys$input/opt
interbase:[syslib]gdsshr.exe/share
psect_attr = errno, noshr
psect_attr = stderr, noshr
psect_attr = stdin, noshr
psect_attr = stdout, noshr
psect_attr = sys_nerr, noshr
psect_attr = vaxc$errno, noshr
universal = fn_abs
universal = fn_upper_c
universal = fn_max
universal = fn_time
