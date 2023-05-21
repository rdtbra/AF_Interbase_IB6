/*
 *      PROGRAM:        JRD Access Method
 *      MODULE:         quad.c
 *      DESCRIPTION:    Quad arithmetic simulation
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


#ifdef VAX
#define LOW_WORD        0
#define HIGH_WORD       1
#else
#define LOW_WORD        1
#define HIGH_WORD       0
#endif

SQUAD QUAD_add (
    SQUAD       *arg1,
    SQUAD       *arg2,
    FPTR_VOID   err)
{
/**************************************
 *
 *      Q U A D _ a d d
 *
 **************************************
 *
 * Functional description
 *      Add two quad numbers.
 *
 **************************************/
SQUAD temp;

(*err) (gds__badblk, 0);  /* not really badblk, but internal error */
/* IBERROR (224); */    	 /* msg 224 quad word arithmetic not supported */
return (temp);		 /* Added to remove compiler warnings */
}

SSHORT QUAD_compare (
    SQUAD       *arg1,
    SQUAD       *arg2)
{
/**************************************
 *
 *      Q U A D _ c o m p a r e
 *
 **************************************
 *
 * Functional description
 *      Compare two descriptors.  Return (-1, 0, 1) if a<b, a=b, or a>b.
 *
 **************************************/

if (((SLONG*) arg1) [HIGH_WORD] > ((SLONG*) arg2) [HIGH_WORD])
    return 1;
if (((SLONG*) arg1) [HIGH_WORD] < ((SLONG*) arg2) [HIGH_WORD])
    return -1;
if (((ULONG*) arg1) [LOW_WORD] > ((ULONG*) arg2) [LOW_WORD])
    return 1;
if (((ULONG*) arg1) [LOW_WORD] < ((ULONG*) arg2) [LOW_WORD])
    return -1;
return 0;
}

SQUAD QUAD_from_double (
    double      *d,
    FPTR_VOID   err)
{
/**************************************
 *
 *      Q U A D _ f r o m _ d o u b l e
 *
 **************************************
 *
 * Functional description
 *      Convert a double to a quad.
 *
 **************************************/
SQUAD temp;
 
(*err) (gds__badblk, 0);       	/* not really badblk, but internal error */
/* BUGCHECK (190); */ 		/* msg 190 conversion not supported for */
				/* specified data types */
return (temp);			/* Added to remove compiler warnings */
}

SQUAD QUAD_multiply (
    SQUAD       *arg1,
    SQUAD       *arg2,
    FPTR_VOID   err)
{
/**************************************
 *
 *      Q U A D _ m u l t i p l y
 *
 **************************************
 *
 * Functional description
 *      Multiply two quad numbers.
 *
 **************************************/
SQUAD temp;

(*err) (gds__badblk, 0); /* not really badblk, but internal error */
/* IBERROR (224); */  	 /* msg 224 quad word arithmetic not supported */
return (temp);		 /* Added to remove compiler warnings */
}

SQUAD QUAD_negate (
    SQUAD       *arg1,
    FPTR_VOID   err)
{
/**************************************
 *
 *      Q U A D _ n e g a t e
 *
 **************************************
 *
 * Functional description
 *      Negate a quad number.
 *
 **************************************/
SQUAD	temp;

(*err) (gds__badblk, 0); /* not really badblk, but internal error */
/* IBERROR (224); */     	 /* msg 224 quad word arithmetic not supported */
return (temp);		 /* Added to remove compiler warnings */
}

SQUAD QUAD_subtract (
    SQUAD       *arg1,
    SQUAD       *arg2,
    FPTR_VOID   err)
{
/**************************************
 *
 *      Q U A D _ s u b t r a c t
 *
 **************************************
 *
 * Functional description
 *      Subtract two quad numbers.
 *
 **************************************/
SQUAD temp;

(*err) (gds__badblk, 0); /* not really badblk, but internal error */
/* IBERROR (224); */	 /* msg 224 quad word arithmetic not supported */
return (temp);		 /* Added to remove compiler warnings */
}
