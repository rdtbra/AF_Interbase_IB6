/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		misc.c
 *	DESCRIPTION:	Miscellaneous routines
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

#ifdef NETWARE_386
#include <sys/stat.h>
#include <errno.h>
#endif

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../jrd/common.h"
#include <stdarg.h>      
#include "../jrd/dsc.h"
#include "../jrd/misc.h"
#include "../jrd/gds_proto.h"
#include "../jrd/misc_proto.h"

#ifdef	WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

#define STUFF(p,value)		{*p++ = value;}
#define STUFF_BYTES(p,value,len) {memcpy (p, (SCHAR*) (value), len); p += len;}
#define STUFF_SHORT(p,value)	STUFF_BYTES (p, &value, sizeof (SSHORT))
#define STUFF_LONG(p,value)	STUFF_BYTES (p, &value, sizeof (SLONG))

SSHORT MISC_build_parameters_block (
    UCHAR	*buffer,
    ...)
{
/**************************************
 *
 *	M I S C _ b u i l d _ p a r a m e t e r s _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Stuffs the passed buffer with various argument information coming via
 *	va_list.  An argument type of 0 signifies the end of argument list.
 *	Returns the total number of bytes put (stuffed) in the passed buffer.
 *
 **************************************/
va_list	ptr;  
UCHAR	*p, *q;
SCHAR	arg_type, ch;
USHORT	sh;
SLONG	l;

p = buffer;
VA_START (ptr, buffer);

/* using the argument types in the parameter list, 
   pop the arguments off the call stack and put
   them into the passed buffer */

while (arg_type = va_arg (ptr, SCHAR))
    switch (arg_type)
	{
	case dtype_byte: /* byte */
	    ch = va_arg (ptr, SCHAR);
	    STUFF (p, ch);
	    break;

	case dtype_short: /* short value */
	    sh = va_arg (ptr, USHORT);
	    STUFF_SHORT (p, sh);
	    break;

	case dtype_long: /* long value */
	    l = (SLONG) va_arg (ptr, SLONG);
	    STUFF_LONG (p, l);
	    break;

	case dtype_cstring: /* null-terminated string */
	    q = va_arg (ptr, UCHAR*);
	    STUFF_BYTES (p, q, strlen (q) + 1);
	    break;

	case dtype_varying: /* short value followed by a value with that many bytes */
	    sh = va_arg (ptr, USHORT);
	    STUFF_SHORT (p, sh);
	    q = va_arg (ptr, UCHAR*);
	    STUFF_BYTES (p, q, sh);
	    break;
	}

va_end (ptr);

return (p-buffer);
}

#ifdef DEV_BUILD
ULONG MISC_checksum_log_rec (
    UCHAR        *header,
    SSHORT       h_len,
    UCHAR        *data,
    SSHORT       d_len)
{
/**************************************
 *
 *      M I S C _ c h e c k s u m _ l o g _ r e c
 *
 **************************************
 *
 * Functional description
 *      Checksum a log record - header and data are passed separately.
 *
 **************************************/
ULONG    checksum;
UCHAR    *p;

checksum = 0;

if (h_len)
    {
    p = header;
    do checksum += *p++; while (--h_len);
    }

if (d_len)
    {
    p = data;
    do checksum += *p++; while (--d_len);
    }

return checksum;
}
#endif /* DEV_BUILD */

#ifdef NETWARE_386
SCHAR *MISC_mktemp (
    SCHAR	*filename,
    SCHAR	*prefix)
{
/**************************************
 *
 *      M I S C _ m k t e m p
 *
 **************************************
 *
 * Functional description
 *      mktemp for NLM.
 *      filename points to a directory name and a prefix.
 *
 **************************************/
SCHAR   local_fname[256];
struct  stat    statblk;
SSHORT	prefix_len;
SCHAR   name[L_tmpnam];

prefix_len = strlen (prefix);
if (prefix_len > 4)
    { /* Limit the prefix to 4 charaters to leave some space 
         for temp file subscript */
    filename[strlen(filename) - (prefix_len-4)] = '\0';
    prefix_len = 4;
    }

for (;;)
    {
    ib_tmpnam (name);
    /* Create a full directory path name for a temp file */
    sprintf (local_fname, "%s%s", filename, &name[prefix_len]);
    /* Make sure that the file does not already exist */
    if ((stat(local_fname, &statblk) == -1) && (errno == ENOENT))
	{
	strcpy (filename, local_fname);
	return filename;
	}
    }
}
#endif

UCHAR *MISC_pop (
    STK		*stack)
{
/**************************************
 *
 *	M I S C _ p o p 
 *
 **************************************
 *
 * Functional description
 *	Pops an object off a linked list stack.
 *	Returns the address of the popped object.
 *
 **************************************/
STK	node;
UCHAR	*object;

node = *stack;
object = node->stk_object;
*stack = node->stk_next;
gds__free (node);

return object;
}

STK MISC_push (
    UCHAR	*object,
    STK		*stack)
{
/**************************************
 *
 *	M I S C _ p u s h 
 *
 **************************************
 *
 * Functional description
 *	Push an object on the passed stack. 
 *	Returns the address of the new top of the stack.
 *
 **************************************/
STK	node;

node = (STK) gds__alloc ((SLONG)sizeof (struct stk));
/* FREE: in MISC_pop(), NOMEM: return a NULL top of stack */
if (!node)
    return NULL;
node->stk_object = object;
node->stk_next = *stack;
*stack = node;

return node;
}

