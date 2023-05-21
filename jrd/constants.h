/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		constants.h
 *	DESCRIPTION:	Misc system constants
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

#ifndef _JRD_CONSTANTS_H_
#define _JRD_CONSTANTS_H_

/* BLOb Subtype definitions */

/* Subtypes < 0  are user defined 
 * Subtype  0    means "untyped" 
 * Subtypes > 0  are InterBase defined 
 */

#define BLOB_untyped	0

/* InterBase defined BLOb subtypes */

#define BLOB_text	1
#define BLOB_blr	2
#define BLOB_acl	3
#define BLOB_ranges	4
#define BLOB_summary	5
#define BLOB_format	6
#define BLOB_tra	7
#define BLOB_extfile	8



/* Column Limits */

#define	MAX_COLUMN_SIZE	32767	/* Bytes */



/* Misc constant values */

#define USERNAME_LENGTH		31	/* Bytes */

/* literal strings in rdb$ref_constraints to be used to identify
   the cascade actions for referential constraints. Used
   by isql/show and isql/extract for now. */

#define RI_ACTION_CASCADE "CASCADE"
#define RI_ACTION_NULL    "SET NULL"
#define RI_ACTION_DEFAULT "SET DEFAULT"
#define RI_ACTION_NONE    "NO ACTION"
#define RI_RESTRICT       "RESTRICT"


/* UDF Arguments are numbered from 0 to MAX_UDF_ARGUMENTS --
   arguement 0 is reserved for the return-type of the UDF */

#define MAX_UDF_ARGUMENTS	10

#endif /* _JRD_CONSTANTS_H_ */
