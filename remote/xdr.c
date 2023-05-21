/*
 *	PROGRAM:	External Data Representation
 *	MODULE:		xdr.c
 *	DESCRIPTION:	GDS version of Sun's register XDR Package.
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

#include <string.h>
#include "../remote/remote.h"
#include "../remote/xdr.h"
#include "../jrd/common.h"
#include "../remote/allr_proto.h"
#include "../remote/proto_proto.h"
#include "../remote/xdr_proto.h"
#include "../jrd/gds_proto.h"

#ifdef  WINDOWS_ONLY
#include "../jrd/seg_proto.h"
#endif

#ifdef DOS_ONLY
#ifdef WINDOWS_ONLY
#include "../remote/ntoh_proto.h"
#else
unsigned long  FAR PASCAL ntohl (unsigned long);
unsigned long  FAR PASCAL htonl (unsigned long);
#endif  /* WINDOWS_ONLY */
#define		SWAP_DOUBLE
#endif  /* DOS_ONLY */

#ifdef OS2_ONLY
#include <utils.h>
#define		SWAP_DOUBLE
#endif

#ifdef M_I386
#define		SWAP_DOUBLE
#endif

#ifdef WIN_NT
#define		SWAP_DOUBLE
#endif

#ifdef NETWARE_386
#define         SWAP_DOUBLE
#endif

#ifdef VMS
extern double	MTH$CVT_D_G(), MTH$CVT_G_D();
#endif

#ifdef BURP
#include "../burp/misc_proto.h"	/* Was "../burp/misc_pro.h" -Jeevan */
#define XDR_ALLOC(size)	    MISC_alloc_burp (size)
#define XDR_FREE(block)	    MISC_free_burp (block)
#endif

#ifndef XDR_ALLOC
#define XDR_ALLOC(size)	    gds__alloc (size)
#define XDR_FREE(block)	    gds__free (block)
#endif

#ifdef DEBUG_XDR_MEMORY
#define DEBUG_XDR_ALLOC(xdrvar, addr, len) \
			xdr_debug_memory (xdrs, XDR_DECODE, xdrvar, addr, (ULONG) len)
#define DEBUG_XDR_FREE(xdrvar, addr, len) \
			xdr_debug_memory (xdrs, XDR_FREE, xdrvar, addr, (ULONG) len)
#else
#define DEBUG_XDR_ALLOC(xdrvar, addr, len)
#define DEBUG_XDR_FREE(xdrvar, addr, len)
#endif /* DEBUG_XDR_MEMORY */

/* Sun's XDR documentation says this should be "MAXUNSIGNED", but
   for InterBase purposes, limiting strings to 65K is more than
   sufficient. */

#define MAXSTRING_FOR_WRAPSTRING	65535

static XDR_INT	mem_destroy (register XDR *);
static bool_t	mem_getbytes (register XDR *, register SCHAR *, register u_int);
static bool_t	mem_getlong (register XDR *, register SLONG *);
static u_int	mem_getpostn (register XDR *);
static caddr_t  mem_inline (register XDR *, u_int);
static bool_t	mem_putbytes (register XDR *, register SCHAR *, register u_int);
static bool_t	mem_putlong (register XDR *, register SLONG *);
static bool_t	mem_setpostn (register XDR *, u_int);

static struct xdr_ops	mem_ops =
	{
	mem_getlong,
	mem_putlong,
	mem_getbytes,
	mem_putbytes,
	mem_getpostn,
	mem_setpostn,
	mem_inline,
	mem_destroy};

#define GETBYTES	 (*xdrs->x_ops->x_getbytes) 
#define GETLONG		 (*xdrs->x_ops->x_getlong) 
#define PUTBYTES	 (*xdrs->x_ops->x_putbytes) 
#define PUTLONG		 (*xdrs->x_ops->x_putlong) 

static SCHAR	zeros [4] = {0,0,0,0};

bool_t xdr_bool (
    register XDR	*xdrs,
    register bool_t	*bp)
{
/**************************************
 *
 *	x d r _ b o o l
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = *bp;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*bp = (bool_t) temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

bool_t xdr_bytes (
    register XDR	*xdrs,
    register SCHAR	**bpp,
    u_int		*lp,
    register u_int	maxlength)
{
/**************************************
 *
 *	x d r _ b y t e s 
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free a string.
 *
 **************************************/
SLONG	length;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	length = *lp;
	if (length > maxlength ||
	    !PUTLONG (xdrs, &length) ||
	    !PUTBYTES (xdrs, *bpp, length))
	    return FALSE;
	if ((length = (4 - length) & 3) != 0)
	    return PUTBYTES (xdrs, zeros, length);
	return TRUE;

    case XDR_DECODE:
	if (!*bpp)
	    {
	    *bpp = (TEXT*) XDR_ALLOC ((SLONG) (maxlength + 1));
	    /* FREE: via XDR_FREE call to this procedure */
	    if (!*bpp)	/* NOMEM: */
	    	return FALSE;
	    DEBUG_XDR_ALLOC (bpp, *bpp, (maxlength + 1));
	    }
	if (!GETLONG (xdrs, &length) ||
	    length > maxlength ||
	    !GETBYTES (xdrs, *bpp, length))
	    return FALSE;
	if ((length = (4 - length) & 3) != 0)
	    return GETBYTES (xdrs, zeros, length);
	*lp = (u_int) length;
	return TRUE;

    case XDR_FREE:
	if (*bpp)
	    {
	    XDR_FREE (*bpp);
	    DEBUG_XDR_FREE (bpp, *bpp, (maxlength + 1));
	    *bpp = NULL;
	    }
	return TRUE;
    }

return FALSE;
}

bool_t xdr_double (
    register XDR	*xdrs,
    register double	*ip)
{
/**************************************
 *
 *	x d r _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
#ifdef VAX_FLOAT
SSHORT		t1;
#endif
union
    {
    double	temp_double;
    SLONG	temp_long [2];
    SSHORT	temp_short [4];
    }		temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
#ifdef	WINDOWS_ONLY
	memcpy ((UCHAR *)(&temp.temp_double), (UCHAR *)ip, sizeof (double));
#else
	temp.temp_double = *ip;
#endif
#ifdef VAX_FLOAT
#ifdef NETWARE_386
/* bug in WATCOM C */
	if (temp.temp_long [0] || temp.temp_long [1])
#else
	if (temp.temp_double != 0)
#endif
	    temp.temp_short [0] -= 0x20;
	t1 = temp.temp_short [0];
	temp.temp_short [0] = temp.temp_short [1];
	temp.temp_short [1] = t1;
	t1 = temp.temp_short [2];
	temp.temp_short [2] = temp.temp_short [3];
	temp.temp_short [3] = t1;
#endif
#ifdef SWAP_DOUBLE
	if (PUTLONG (xdrs, &temp.temp_long [1]) && 
	    PUTLONG (xdrs, &temp.temp_long [0]))
	    return TRUE;
	return FALSE;
#else
	if (PUTLONG (xdrs, &temp.temp_long [0]) && 
	    PUTLONG (xdrs, &temp.temp_long [1]))
	    return TRUE;
	return FALSE;
#endif

    case XDR_DECODE:
#ifdef SWAP_DOUBLE
	if (!GETLONG (xdrs, &temp.temp_long [1]) ||
	    !GETLONG (xdrs, &temp.temp_long [0]))
	    return FALSE;
#else
	if (!GETLONG (xdrs, &temp.temp_long [0]) ||
	    !GETLONG (xdrs, &temp.temp_long [1]))
	    return FALSE;
#endif
#ifdef VAX_FLOAT
	t1 = temp.temp_short [0];
	temp.temp_short [0] = temp.temp_short [1];
	temp.temp_short [1] = t1;
	t1 = temp.temp_short [2];
	temp.temp_short [2] = temp.temp_short [3];
	temp.temp_short [3] = t1;
	if (!temp.temp_long[1] && !(temp.temp_long[0] ^ 0x8000))
	    temp.temp_long[0] = 0;
	else if (temp.temp_long[1] || temp.temp_long[0])
	    temp.temp_short [0] += 0x20;
#endif
#ifdef	WINDOWS_ONLY
	memcpy ((UCHAR *)ip, (UCHAR *)(&temp.temp_double), sizeof (double));
#else
	*ip = temp.temp_double;
#endif
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

#ifdef VMS
bool_t xdr_d_float (xdrs, ip)
    register XDR	*xdrs;
    register double	*ip;
{
/**************************************
 *
 *	x d r _ d _ f l o a t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
double		temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = MTH$CVT_D_G (ip);
	return xdr_double (xdrs, &temp);

    case XDR_DECODE:
	if (!xdr_double (xdrs, ip))
	    return FALSE;
	*ip = MTH$CVT_G_D (ip);
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }
}       
#endif

bool_t xdr_enum (
    register XDR	*xdrs,
    enum_t		*ip)
{
/**************************************
 *
 *	x d r _ e n u m
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = (SLONG) *ip;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*ip = (enum_t) temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

bool_t xdr_float (
    register XDR	*xdrs,
    register float	*ip)
{
/**************************************
 *
 *	x d r _ f l o a t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
#ifdef VAX_FLOAT
SSHORT		t1;
union {
    float	temp_float;
    SLONG	temp_long;
    USHORT	temp_short [2];
    }		temp;
#endif

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
#ifdef VAX_FLOAT
	temp.temp_float = *ip;
	if (temp.temp_float)
	    {
	    t1 = temp.temp_short [0];
	    temp.temp_short [0] = temp.temp_short [1];
	    temp.temp_short [1] = t1 - 0x100;
	    }
	if (!PUTLONG (xdrs, &temp))
	    return FALSE;
	return TRUE;
#else
	return PUTLONG (xdrs, ip); 
#endif

    case XDR_DECODE:
#ifdef VAX_FLOAT
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	if (!(temp.temp_long ^ 0x80000000))
	    temp.temp_long = 0;
	else if (temp.temp_long)
	    {
	    t1 = temp.temp_short [1];
	    temp.temp_short [1] = temp.temp_short [0];
	    temp.temp_short [0] = t1 + 0x100;
	    }
	*ip = temp.temp_float;
	return TRUE;
#else
	return GETLONG (xdrs, ip);
#endif

    case XDR_FREE:
	return TRUE;
    } 

return FALSE;
}

/**
	This routine is duplicated in remote/protocol.c for IMP.
**/
bool_t xdr_free (
    xdrproc_t	proc,
    SCHAR	*objp)
{
/**************************************
 *
 *	x d r _ f r e e
 *
 **************************************
 *
 * Functional description
 *	Perform XDR_FREE operation on an XDR structure
 *
 **************************************/
XDR	xdrs;

xdrs.x_op = XDR_FREE;

return (*proc) (&xdrs, objp);
}

bool_t xdr_int (
    register XDR	*xdrs,
    register int	*ip)
{
/**************************************
 *
 *	x d r _ i n t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = *ip;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*ip = (int) temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

bool_t xdr_long (
    register XDR	*xdrs,
    register SLONG	*ip)
{
/**************************************
 *
 *	x d r _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	return PUTLONG (xdrs, ip); 

    case XDR_DECODE:
	return GETLONG (xdrs, ip);

    case XDR_FREE:
	return TRUE;
    } 

return FALSE;
}

bool_t xdr_opaque (
    register XDR	*xdrs,
    register SCHAR	*p,
    register u_int	len)
{
/**************************************
 *
 *	x d r _ o p a q u e
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free an opaque object.
 *
 **************************************/
SCHAR	trash [4];
SSHORT	l;

l = (4 - len) & 3;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	if (!PUTBYTES (xdrs, p, len))
	    return FALSE;
	if (l)
	    return PUTBYTES (xdrs, trash, l);
	return TRUE;

    case XDR_DECODE:
	if (!GETBYTES (xdrs, p, len))
	    return FALSE;
	if (l)
	    return GETBYTES (xdrs, trash, l);
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

bool_t xdr_short (
    register XDR	*xdrs,
    register SSHORT	*ip)
{
/**************************************
 *
 *	x d r _ s h o r t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = *ip;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*ip = temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

bool_t xdr_string (
    register XDR	*xdrs,
    register SCHAR	**sp,
    register u_int	maxlength)
{
/**************************************
 *
 *	x d r _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Encode, decode, or free a string.
 *
 **************************************/
SCHAR	trash [4];
u_long	length;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	length = strlen (*sp);
	if (length > maxlength ||
	    !PUTLONG (xdrs, &length) ||
	    !PUTBYTES (xdrs, *sp, length))
	    return FALSE;
	if ((length = (4 - length) & 3) != 0)
	    return PUTBYTES (xdrs, trash, length);
	return TRUE;

    case XDR_DECODE:
	if (!*sp)
	    {
	    *sp = (TEXT*) XDR_ALLOC ((SLONG) (maxlength + 1));
	    /* FREE: via XDR_FREE call to this procedure */
	    if (!*sp)	/* NOMEM: return error */
	    	return FALSE;
	    DEBUG_XDR_ALLOC (sp, *sp, (maxlength + 1));
	    }
	if (!GETLONG (xdrs, &length) ||
	    length > maxlength ||
	    !GETBYTES (xdrs, *sp, length))
	    return FALSE;
	(*sp) [length] = 0;
	if ((length = (4 - length) & 3) != 0)
	    return GETBYTES (xdrs, trash, length);
	return TRUE;

    case XDR_FREE:
	if (*sp)
	    {
	    XDR_FREE (*sp);
	    DEBUG_XDR_FREE (sp, *sp, (maxlength + 1));
	    *sp = NULL;
	    }
	return TRUE;
    }

return FALSE;
}

bool_t xdr_u_int (
    register XDR	*xdrs,
    register u_int	*ip)
{
/**************************************
 *
 *	x d r _ u _ i n t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = *ip;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*ip = temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;

    default:
	return FALSE;	
    }
}

bool_t xdr_u_long (
    register XDR	*xdrs,
    register u_long	*ip)
{
/**************************************
 *
 *	x d r _ u _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	return PUTLONG (xdrs, ip); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, ip))
	    return FALSE;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    } 

return FALSE;
}

bool_t xdr_u_short (
    register XDR	*xdrs,
    register u_short	*ip)
{
/**************************************
 *
 *	x d r _ u _ s h o r t
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/
SLONG	temp;

switch (xdrs->x_op)
    {
    case XDR_ENCODE:
	temp = *ip;
	return PUTLONG (xdrs, &temp); 

    case XDR_DECODE:
	if (!GETLONG (xdrs, &temp))
	    return FALSE;
	*ip = (u_int) temp;
	return TRUE;

    case XDR_FREE:
	return TRUE;
    }

return FALSE;
}

int xdr_union (
    XDR		*xdrs,
    enum_t	*dscmp,
    SCHAR	*unp,
    struct xdr_discrim	*choices,
    xdrproc_t	dfault)
{
/**************************************
 *
 *	x d r _ u n i o n
 *
 **************************************
 *
 * Functional description
 *	Descriminated union.  Yuckola.
 *
 **************************************/
SCHAR	*op;

if (!xdr_int (xdrs, dscmp))
    return FALSE;

for (; choices->proc; ++choices)
    if (*dscmp == choices->value)
	{
	return (*choices->proc) (xdrs, unp);
	}

if (dfault)
    return (*dfault) (xdrs, unp);

return FALSE;
}

int xdr_void (void)
{
/**************************************
 *
 *	x d r _ v o i d
 *
 **************************************
 *
 * Functional description
 *	Do next to nothing.
 *
 **************************************/

return TRUE;
}

bool_t xdr_wrapstring (
    register XDR	*xdrs,
    register SCHAR	**strp)
{
/**************************************
 *
 *	x d r _ w r a p s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Map from external to internal representation (or vice versa).
 *
 **************************************/

return xdr_string (xdrs, strp, MAXSTRING_FOR_WRAPSTRING);
}

int xdrmem_create (
    register XDR	*xdrs,
    SCHAR		*addr,
    u_int		len,
    enum xdr_op		x_op)
{
/**************************************
 *
 *	x d r m e m _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Initialize an "in memory" register XDR stream.
 *
 **************************************/

xdrs->x_base = xdrs->x_private = addr;
xdrs->x_handy = len;
xdrs->x_ops = &mem_ops;
xdrs->x_op = x_op;

return TRUE;
}

static XDR_INT mem_destroy (
    register XDR	*xdrs)
{
/**************************************
 *
 *	m e m _ d e s t r o y
 *
 **************************************
 *
 * Functional description
 *	Destroy a stream.  A no-op.
 *
 **************************************/
return TRUE;
}

static bool_t mem_getbytes (
    register XDR	*xdrs,
    register SCHAR	*buff,
    register u_int	count)
{
/**************************************
 *
 *	m e m _ g e t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Get a bunch of bytes from a memory stream if it fits.
 *
 **************************************/
SLONG	bytecount = count;

if ((xdrs->x_handy -= bytecount) < 0)
    {
    xdrs->x_handy += bytecount;
    return FALSE;
    }

if (bytecount)
    {
    memcpy (buff, xdrs->x_private, bytecount);
    xdrs->x_private += bytecount;
    }

return TRUE;
}

static bool_t mem_getlong (
    register XDR	*xdrs,
    register SLONG	*lp)
{
/**************************************
 *
 *	m e m _ g e t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
register SLONG	*p;

if ((xdrs->x_handy -= sizeof (SLONG))  < 0)
    {
    xdrs->x_handy += sizeof (SLONG);
    return FALSE;
    }

p = (SLONG*) xdrs->x_private;
*lp = ntohl (*p++);
xdrs->x_private = (SCHAR*) p;

return TRUE;
}

static u_int mem_getpostn (
    register XDR		*xdrs)
{
/**************************************
 *
 *	m e m _ g e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Get the current position (which is also current length) from stream.
 *
 **************************************/

return (u_int) (xdrs->x_private - xdrs->x_base);
}

static caddr_t mem_inline (
    register XDR	*xdrs,
    u_int		bytecount)
{
/**************************************
 *
 *	m e m _  i n l i n e
 *
 **************************************
 *
 * Functional description
 *	Return a pointer to somewhere in the buffer.
 *
 **************************************/

if (bytecount > (xdrs->x_private + xdrs->x_handy) - xdrs->x_base)
    return FALSE;

return xdrs->x_base + bytecount;
}

static bool_t mem_putbytes (
    register XDR	*xdrs,
    register SCHAR	*buff,
    register u_int	count)
{
/**************************************
 *
 *	m e m _ p u t b y t e s
 *
 **************************************
 *
 * Functional description
 *	Put a bunch of bytes to a memory stream if it fits.
 *
 **************************************/
SLONG	bytecount = count;

if ((xdrs->x_handy -= bytecount) < 0)
    {
    xdrs->x_handy += bytecount;
    return FALSE;
    }

if (bytecount)
    {
    memcpy (xdrs->x_private, buff, bytecount);
    xdrs->x_private += bytecount;
    }

return TRUE;
}

static bool_t mem_putlong (
    register XDR	*xdrs,
    register SLONG	*lp)
{
/**************************************
 *
 *	m e m _ p u t l o n g
 *
 **************************************
 *
 * Functional description
 *	Fetch a longword into a memory stream if it fits.
 *
 **************************************/
register SLONG	*p;

if ((xdrs->x_handy -= sizeof (SLONG))  < 0)
    {
    xdrs->x_handy += sizeof (SLONG);
    return FALSE;
    }

p = (SLONG*) xdrs->x_private;
*p++ = htonl (*lp);
xdrs->x_private = (SCHAR*) p;

return TRUE;
}

static bool_t mem_setpostn (
    register XDR	*xdrs,
    u_int		bytecount)
{
/**************************************
 *
 *	m e m _ s e t p o s t n
 *
 **************************************
 *
 * Functional description
 *	Set the current position (which is also current length) from stream.
 *
 **************************************/
u_int	length;

length = (u_int) ((xdrs->x_private - xdrs->x_base) + xdrs->x_handy);

if (bytecount > length)
    return FALSE;

xdrs->x_handy = length - bytecount;
xdrs->x_private = xdrs->x_base + bytecount;

return TRUE;
}

