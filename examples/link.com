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
$!
$! This .COM file is an example of how to create a
$! new user-defined library, not for linking an
$! InterBase application.
$!
$ link/nomap/exe='P1  'P1, sys$input/opt
sys$library:vaxcrtlg.exe/share
interbase:[syslib]gdsshr.exe/share
