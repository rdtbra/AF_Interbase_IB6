/******************************************************************
These functions are to be used in a stack switching context only.
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
******************************************************************/
#ifndef __IUTLS_MATH_H
#define __IUTLS_MATH_H

#if (defined (WINDOWS_ONLY) && !defined (WIN_NT))
double MATH_multiply (double f1, double f2);
double MATH_divide (double f1, double f2);
#else
#define MATH_multiply(f1, f2) ((f1)*(f2))
#define MATH_divide(f1, f2) ((f1)/(f2))
#endif

#endif /* __IUTLS_MATH_H */
