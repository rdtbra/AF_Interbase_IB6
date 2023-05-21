/*
 *	PROGRAM:        InterBase International support
 *	MODULE: 	cs_ksc.c
 *	DESCRIPTION:	Character set definitions for KSC-5601.
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

#include "../intl/ldcommon.h"

extern	USHORT	CVKSC_ksc_to_unicode();
extern	USHORT	CVKSC_unicode_to_ksc();
extern	USHORT	CVKSC_check_ksc();

CHARSET_ENTRY(CS_ksc_5601)
{
static CONST	char	space = 0x20;

#include "../intl/cs_ksc5601.h"

csptr->charset_version = 40;  
csptr->charset_id = CS_KSC5601;  
csptr->charset_name = (ASCII *) "KSC_5601";  
csptr->charset_flags = 0;  
csptr->charset_min_bytes_per_char = 1;  
csptr->charset_max_bytes_per_char = 2;  
csptr->charset_space_length = 1;  
csptr->charset_space_character = (BYTE *) &space;  
csptr->charset_well_formed = (FPTR_SHORT) CVKSC_check_ksc;

CV_convert_init (&csptr->charset_to_unicode, CS_UNICODE, CS_KSC5601, 
		CVKSC_ksc_to_unicode, to_unicode_mapping_array, to_unicode_map);
CV_convert_init (&csptr->charset_from_unicode, CS_KSC5601, CS_UNICODE,  
		CVKSC_unicode_to_ksc, from_unicode_mapping_array, 
		from_unicode_map);

CHARSET_RETURN;
} 
