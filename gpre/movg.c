/*
 *	PROGRAM:	Gpre support
 *	MODULE:		movg.c
 *	DESCRIPTION:	Data mover and converter and comparator, etc.
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
#include <stdarg.h>

#include "../jrd/codes.h"
#include "../jrd/iberr.h"
#include "../jrd/dsc.h"
#include "../gpre/movg_proto.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/thd_proto.h"

static void	post_error (void);

void MOVG_move ( DSC	*from, DSC	*to)
{
/**************************************
 *
 *	M O V G _ m o v e
 *
 **************************************
 *
 * Functional description
 *	Move (and possible convert) something to something else.
 *
 **************************************/

CVT_move (from, to, (FPTR_VOID) post_error);
}

static void post_error (void)
{
/**************************************
 *
 *	p o s t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	A conversion error occurred.  Complain.
 *
 **************************************/

CPR_error ("conversion error: illegal string literal");
CPR_abort();
}
