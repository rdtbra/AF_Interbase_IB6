/*
 *	PROGRAM:	Examples
 *	MODULE:		shrfilter.c
 *	DESCRIPTION:	Imported symbol initialization for shared filter library
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include <stdio.h>

int (*_libfun_strcmp)() = 0;
int (*_libfun_sprintf)() = 0;
int (*_libfun_system)() = 0;
FILE *(*_libfun_fopen)() = 0;
int (*_libfun_unlink)() = 0;
int (*_libfun_fprintf)() = 0;
int (*_libfun_fclose)() = 0;
int (*_libfun_fgetc)() = 0;

