/*
 *        PROGRAM:        JRD Access Method
 *        MODULE:         iberr.h
 *        DESCRIPTION:    Interbase error handling definitions.
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

#ifndef _JRD_IBERR_H_
#define _JRD_IBERR_H_

#include "../jrd/gdsassert.h"

#define MAX_ERRMSG_LEN  128
#define MAX_ERRSTR_LEN	255

/* The following macro is used to stuff variable number of error message
   arguments from stack to the status_vector.   This macro should be the 
   first statement in a routine where it is invoked. */

/* Get the addresses of the argument vector and the status vector, and do
   word-wise copy. */

/** Check that we never overrun the status vector.  The status
 *  vector is 20 elements.  The maximum is 3 entries for a 
 * type.  So check for 17 or less
 */

#define STUFF_STATUS(status_vector,status)      \
{						\
va_list args;                                   \
STATUS  *p, *q;                                 \
int     type, len;                              \
                                                \
VA_START (args, status);                        \
p = status_vector;                              \
                                                \
*p++ = gds_arg_gds;                             \
*p++ = status;                                  \
                                                \
while ((type = va_arg (args, int)) && ((p - status_vector) < 17))    \
    switch (*p++ = type)                        \
    {                                           \
    case gds_arg_gds:                           \
        *p++ = (STATUS) va_arg (args, STATUS);  \
        break;                                  \
                                                \
    case gds_arg_string:                        \
        q = (STATUS) va_arg (args, TEXT*);	\
	if (strlen ((TEXT*)q) >= MAX_ERRSTR_LEN)\
	    {					\
	    *(p -1) = gds_arg_cstring;		\
	    *p++ = (STATUS) MAX_ERRSTR_LEN;	\
	    }					\
	*p++ = (STATUS) q;			\
        break;                                  \
                                                \
    case gds_arg_interpreted:                   \
	*p++ = (STATUS) va_arg (args, TEXT*);   \
	break;                                  \
						\
    case gds_arg_cstring:                       \
	len = (int) va_arg (args, int);		\
        *p++ = (STATUS) (len >= MAX_ERRSTR_LEN) ? MAX_ERRSTR_LEN : len;\
        *p++ = (STATUS) va_arg (args, TEXT*);   \
        break;                                  \
                                                \
    case gds_arg_number:                        \
        *p++ = (STATUS) va_arg (args, SLONG);   \
        break;                                  \
                                                \
    case gds_arg_vms:                           \
    case gds_arg_unix:                          \
    case gds_arg_domain:                        \
    case gds_arg_dos:                           \
    case gds_arg_mpexl:                         \
    case gds_arg_next_mach:                     \
    case gds_arg_netware:                       \
    case gds_arg_win32:                         \
    default:                                    \
        *p++ = (STATUS) va_arg (args, int);     \
        break;                                  \
    }						\
*p = gds_arg_end;				\
}
/* end of STUFF_STATUS */			

/* The following macro calculates status vector length,
   and finds the very first warning.
*/
   
#define PARSE_STATUS(status_vector, length, warning)	\
{							\
STATUS	*p;						\
int	i;						\
							\
p = status_vector;					\
warning = 0;						\
length = 0;						\
							\
for (i = 0; p[i] != gds_arg_end; i++, length++)		\
    switch (p[i])					\
	{						\
	case isc_arg_warning:				\
	    if (!warning)				\
		warning = i; /* find the very first */	\
	    /* break is not used on purpose */		\
	case gds_arg_gds:				\
	case gds_arg_string:				\
	case gds_arg_number:				\
	case gds_arg_vms:				\
	case gds_arg_unix:				\
	case gds_arg_domain:				\
	case gds_arg_dos:				\
	case gds_arg_mpexl:				\
	case gds_arg_next_mach:				\
	case gds_arg_netware:				\
	case gds_arg_win32:				\
	    i++;					\
	    length++;					\
	    break;					\
							\
	case gds_arg_cstring:				\
	    i += 2;					\
	    length += 2;				\
	    break;					\
							\
	default:					\
	    assert (FALSE);				\
	    break;					\
	}						\
if (i) length++;	/* gds_arg_end is counted */	\
}
/* end of PARSE_STATUS */

#define INIT_STATUS(status)	status [0] = gds_arg_gds;\
				status [1] = 0;\
				status [2] = gds_arg_end

#endif /* _JRD_IBERR_H_ */
