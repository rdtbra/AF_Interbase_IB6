/*
 *	PROGRAM:	Language Preprocessor
 *	MODULE:		noform.c
 *	DESCRIPTION:	Porting module for no forms support
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

#include "../gpre/gpre.h"
#include "../gpre/form.h"
#include "../gpre/form_proto.h"

typedef int	*HANDLE;

FLD FORM_lookup_field (
    FORM	form,
    HANDLE	object,
    UCHAR	*string)
{
/**************************************
 *
 *	F O R M _ l o o k u p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Lookup field in form.
 *
 **************************************/

return NULL;
}

FORM FORM_lookup_form (
    DBB		dbb,
    UCHAR	*string)
{
/**************************************
 *
 *	F O R M _ l o o k u p _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Lookup form.  This may be an instance already in
 *	use.
 *
 **************************************/

return NULL;
}

FORM FORM_lookup_subform (
    FORM	parent,
    FLD		field)
{
/**************************************
 *
 *	F O R M _ l o o k u p _ s u b f o r m
 *
 **************************************
 *
 * Functional description
 *	Lookup sub-form of a given form.
 *
 **************************************/

return NULL;
}
