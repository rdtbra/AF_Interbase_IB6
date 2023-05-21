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
$! This script builds the example of how to convert between
$! different character sets.
$!
$
$ define/nolog EXAMPLES interbase:[intl]
$
$ copy EXAMPLES:cs_convert.c  []
$ copy EXAMPLES:437_to_865.h  []
$ copy EXAMPLES:437_to_lat1.h []
$ copy EXAMPLES:865_to_lat1.h []
$ copy EXAMPLES:cs_load.gdl   []
$ copy EXAMPLES:cs_demo.gdl   []
$ copy EXAMPLES:cs_load.qli   []
$ copy EXAMPLES:products.lat1 []
$ copy EXAMPLES:users.lat1    []
$ copy EXAMPLES:clients.437   []
$ copy EXAMPLES:contacts.437  []
$
$! Build the UDF with the character set conversion functions
$
$ cc/G_FLOAT cs_convert.c
$ link/share=CONVERT.exe cs_convert,  sys$input/opt
sys$library:vaxcrtlg.exe/share
universal = c_convert_865_to_latin1
universal = c_convert_437_to_latin1
universal = c_convert_437_to_865
universal = c_convert_latin1_to_865
universal = c_convert_latin1_to_437
universal = c_convert_865_to_437
universal = n_convert_865_to_latin1
universal = n_convert_437_to_latin1
universal = n_convert_437_to_865
universal = n_convert_latin1_to_865
universal = n_convert_latin1_to_437
universal = n_convert_865_to_437
$
$! Define the logical InterBase will use to find the UDF library
$
$ define/nolog CONVERT_MODULE 'f$search( "CONVERT.EXE" )'
$
$! Define the demo databases and load them with data
$ 
$ gdef -z CS_LOAD.GDL
$ gdef -z CS_DEMO.GDL
$ qli  -z -a CS_LOAD.QLI
