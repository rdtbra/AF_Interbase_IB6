/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		lls.h
 *	DESCRIPTION:	Linked list stack definitions
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

#ifndef _JRD_LLS_H_
#define _JRD_LLS_H_

#define LLS_PUSH(object, stack)		ALL_push ((struct blk *) object, stack)
#define LLS_POP(stack)			ALL_pop (stack)

typedef struct lls {
    struct blk	lls_header;
    struct blk	*lls_object;
    struct lls	*lls_next;
} *LLS;

#endif /* _JRD_LLS_H_ */
