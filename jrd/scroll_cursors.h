/*
 *	MODULE:		scroll_cursors.h
 *	DESCRIPTION:	OSRI entrypoints and defines for SCROLLABLE_CURSORS
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

#ifndef _JRD_SCROLL_CURSORS_H_
#define _JRD_SCROLL_CURSORS_H_

/* ALL THE FOLLOWING DEFINITIONS HAVE BEEN TAKEN OUT OF JRD/IBASE.H 
   WHEN SCROLLABLE_CURSORS ARE TOTALLY IMPLEMENTED, THE FOLLOWING 
   DEFINITIONS NEED TO BE PUT IN THE PROPER INCLUDE FILE.

   This was done so that IB 5.0 customers do not see any functions
   they are not supposed to see.
   As per Engg. team's decision on 23-Sep-1997 
*/

#ifdef SCROLLABLE_CURSORS
ISC_STATUS  ISC_EXPORT isc_dsql_fetch2 (ISC_STATUS ISC_FAR *, 
				       isc_stmt_handle ISC_FAR *, 
				       unsigned short, 
				       XSQLDA ISC_FAR *,
				       unsigned short,
				       signed long);
#endif

#ifdef SCROLLABLE_CURSORS
ISC_STATUS  ISC_EXPORT isc_dsql_fetch2_m (ISC_STATUS ISC_FAR *, 
					 isc_stmt_handle ISC_FAR *, 
					 unsigned short, 
					 char ISC_FAR *, 
					 unsigned short, 
					 unsigned short, 
					 char ISC_FAR *,
					 unsigned short,
					 signed long);
#endif

#ifdef SCROLLABLE_CURSORS
ISC_STATUS  ISC_EXPORT isc_embed_dsql_fetch2 (ISC_STATUS ISC_FAR *, 
					     char ISC_FAR *, 
					     unsigned short, 
					     XSQLDA ISC_FAR *,
					     unsigned short, 
					     signed long);
#endif

#ifdef SCROLLABLE_CURSORS
ISC_STATUS  ISC_EXPORT isc_receive2 (ISC_STATUS ISC_FAR *, 
				    isc_req_handle ISC_FAR *, 
				    short, 
			 	    short, 
				    void ISC_FAR *, 
				    short, 
				    unsigned short, 
				    unsigned long);
#endif

/****** Add the following commented lines in the #else part of..
#else                                    __cplusplus || __STDC__ 
ISC_STATUS  ISC_EXPORT isc_receive2();
******/

/****************************************/
/* Scroll direction for isc_dsql_fetch2 */
/****************************************/

#define isc_fetch_next			   0
#define isc_fetch_prior			   1
#define isc_fetch_first			   2
#define isc_fetch_last			   3
#define isc_fetch_absolute		   4
#define isc_fetch_relative		   5

#endif  /* _JRD_SCROLL_CURSORS_H_ */
