/*
 *	PROGRAM:	Language Preprocessor
 *	MODULE:		form_trn.c
 *	DESCRIPTION:	Form manager dtype translator
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

#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/dsc.h"
#include "../gpre/form__proto.h"

extern USHORT	MET_get_dtype (USHORT, USHORT, USHORT *);

USHORT FORM_TRN_dtype (
    USHORT	pyxis_dtype)
{
/**************************************
 *
 *	F O R M _ T R N _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Takes a pyxis dtype and transforms
 *	it into a standard InterBase dtype.
 *
 **************************************/
USHORT	gpre_dtype, length;

switch (pyxis_dtype)
    {
    case dtype_text:
	gpre_dtype = MET_get_dtype (blr_text, 0, &length);
	break;

    case dtype_cstring:
	gpre_dtype = MET_get_dtype (blr_cstring, 0, &length);
	break;

    case dtype_varying:
	gpre_dtype = MET_get_dtype (blr_varying, 0, &length);
	break;

    case dtype_short:
	gpre_dtype = MET_get_dtype (blr_short, 0, &length);
	break;

    case dtype_long:
	gpre_dtype = MET_get_dtype (blr_long, 0, &length);
	break;

    case dtype_quad:
	gpre_dtype = MET_get_dtype (blr_quad, 0, &length);
	break;

    case dtype_real:
	gpre_dtype = MET_get_dtype (blr_float, 0, &length);
	break;

    case dtype_double:
	gpre_dtype = MET_get_dtype (blr_double, 0, &length);
	break;

    /* dtype_sql_date & dtype_sql_time not supported in pyxis */
    case dtype_sql_date:
    case dtype_sql_time:
    case dtype_int64:
	gpre_dtype = dtype_null;
	break;

    case dtype_timestamp:
	gpre_dtype = MET_get_dtype (blr_timestamp, 0, &length);
	break;

    case dtype_blob:
	gpre_dtype = MET_get_dtype (blr_blob, 0, &length);
	break;

    case dtype_d_float:
	gpre_dtype = MET_get_dtype (blr_d_float, 0, &length);
	break;

    }

return gpre_dtype;
}
