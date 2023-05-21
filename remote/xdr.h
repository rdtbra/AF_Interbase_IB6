/*
 *	PROGRAM:	External Data Representation
 *	MODULE:		xdr.h
 *	DESCRIPTION:	GDS version of Sun's XDR Package.
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

#ifndef _REMOTE_XDR_H_
#define _REMOTE_XDR_H_

#include "../jrd/common.h"

#ifdef VMS
#include "../remote/types.h"
#include "../remote/in.h"
#define TYPES_DEFINED
#endif

#ifdef WINDOWS_ONLY
typedef unsigned char   u_char;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef unsigned short	u_short;
typedef char *  	caddr_t;
#define TYPES_DEFINED
#endif
      
#ifdef WIN_NT
#include <sys/types.h>
#include <winsock.h>
typedef	char *	caddr_t;
#define TYPES_DEFINED
#endif

#ifdef mpexl
#include "../remote/types.h"
#include "../remote/in.h"
#define TYPES_DEFINED
#endif
    
#ifdef OS2_ONLY
#define BSD_SELECT
#include <types.h>
#include <netinet/in.h>
#include <sys/select.h>
#define TYPES_DEFINED
#endif

#ifndef TYPES_DEFINED
#include <sys/types.h>
#include <netinet/in.h>
#ifdef NETWARE_386
#undef caddr_t
typedef char *  caddr_t;
#endif
#ifdef _AIX
#include <sys/select.h>
#endif
#else
#undef TYPES_DEFINED
#endif

#define XDR_INT	int
#define bool_t	int
#define TRUE	1
#define FALSE	0
#ifndef enum_t
#define enum_t	enum xdr_op
#endif

#ifdef DELTA
#define u_int	uint
#endif

#define xdr_getpostn(xdr)	((*(*xdr).x_ops->x_getpostn)(xdr))
#define xdr_destroy(xdr)	(*(*xdr).x_ops->x_destroy)()

enum xdr_op { XDR_ENCODE = 0, XDR_DECODE = 1, XDR_FREE = 2 };

typedef struct {
    enum xdr_op	x_op;			/* operation; fast additional param */
    struct xdr_ops	{
        bool_t  (*x_getlong)();		/* get a long from underlying stream */
        bool_t  (*x_putlong)();		/* put a long to " */
        bool_t  (*x_getbytes)();	/* get some bytes from " */
        bool_t  (*x_putbytes)();	/* put some bytes to " */
        u_int   (*x_getpostn)();	/* returns bytes offset from beginning*/
        bool_t  (*x_setpostn)();	/* repositions position in stream */
        caddr_t (*x_inline)();		/* buf quick ptr to buffered data */
        XDR_INT (*x_destroy)();		/* free privates of this xdr_stream */
    } *x_ops;
    caddr_t	 x_public;		/* Users' data */
    caddr_t	 x_private;		/* pointer to private data */
    caddr_t	 x_base;		/* private used for position info */
    int	 x_handy;			/* extra private word */
} XDR;

/* Descriminated union crud */

typedef bool_t		(*xdrproc_t)();
#define NULL_xdrproc_t	((xdrproc_t) 0)

struct xdr_discrim {
    enum_t	value;
    xdrproc_t	proc;
};

#endif /* _REMOTE_XDR_H_ */

