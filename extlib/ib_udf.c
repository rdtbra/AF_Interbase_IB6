/*
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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ib_util.h"
#include "ib_udf.h"


int MATHERR( struct exception *e )
{
    return 1;
}

double EXPORT IB_UDF_abs(
    double* a)
{
    return (*a < 0.0) ? -*a : *a;
}

double EXPORT IB_UDF_acos(
    double* a)
{
    return (acos(*a)); 
}

char* EXPORT IB_UDF_ascii_char(
    int* a)
{
    char* b;
    b = (char*) ib_util_malloc(2);
    *b = (char)(*a);
    /* let us not forget to NULL terminate */
    b[1] = '\0';
    return (b); 
}

int EXPORT IB_UDF_ascii_val(
    char* a)
{
    return ((int)(*a)); 
}

double EXPORT IB_UDF_asin(
    double* a)
{
    return (asin(*a)); 
}

double EXPORT IB_UDF_atan(
    double* a)
{
    return (atan(*a)); 
}

double EXPORT IB_UDF_atan2(
    double* a,
    double* b)
{
    return (atan2(*a,*b)); 
}

long EXPORT IB_UDF_bin_and(
    long* a,
    long* b)
{
    return (*a & *b); 
}

long EXPORT IB_UDF_bin_or(
    long* a,
    long* b)
{
    return (*a | *b); 
}

long EXPORT IB_UDF_bin_xor(
    long* a,
    long* b)
{
    return (*a ^ *b); 
}

double EXPORT IB_UDF_ceiling(
    double* a)
{
    return (ceil(*a)); 
}

double EXPORT IB_UDF_cos(
    double* a)
{
    return (cos(*a)); 
}

double EXPORT IB_UDF_cosh(
    double* a)
{
    return (cosh(*a)); 
}

double EXPORT IB_UDF_cot(
    double* a)
{
    return (1.0 / tan(*a)); 
}

double EXPORT IB_UDF_div(
    long* a,
    long* b)
{
    if (*b != 0) {
	div_t div_result = div (*a,*b);
	return (div_result.quot); 
    }
    else 
	/* This is a Kludge!  We need to return INF, 
	   but this seems to be the only way to do 
	   it since there seens to be no constant for it. */
	return (1/tan(0));
    
}

double EXPORT IB_UDF_floor(
    double* a)
{
    return (floor(*a)); 
}

double EXPORT IB_UDF_ln(
    double* a)
{
    return (log(*a)); 
}

double EXPORT IB_UDF_log(
    double* a,
    double* b)
{
    return (log(*a)/log(*b)); 
}

double EXPORT IB_UDF_log10(
    double* a)
{
    return (log10(*a)); 
}

char* EXPORT IB_UDF_lower(
    char* s)
{
    char *buf;
    char *p;

    buf = (char*) ib_util_malloc (strlen (s) + 1);
    p = buf;
    while (*s)
	if (*s >= 'A' && *s <= 'Z') 
	    {
	    *p++ = *s++ - 'A' + 'a';
	    }
	else
	    *p++ = *s++;
    *p = '\0';

    return buf;
}

char* EXPORT IB_UDF_ltrim(
    char* s)
{
    char *buf;
    char *p;
    long length;

    if (length = strlen(s))
    {
	while (*s == ' ')       /* skip leading blanks */
	    s++;
	
	length = strlen(s);
	buf = (char*) ib_util_malloc (length + 1);
	memcpy(buf, s, length); 
	p = buf + length;
	*p = '\0';
    }
    else
	return NULL;

    return buf;
}

double EXPORT IB_UDF_mod(
    long* a,
    long* b)
{
    if (*b != 0) {
	div_t div_result = div (*a,*b);
	return (div_result.rem); 
    }
    else
	/* This is a Kludge!  We need to return INF, 
	   but this seems to be the only way to do 
	   it since there seens to be no constant for it. */
	return (1/tan(0));
}

double EXPORT IB_UDF_pi()
{
    return (IB_PI); 
}

double EXPORT IB_UDF_rand()
{
    srand( (unsigned)time (NULL));
    return ((float) rand() / (float) RAND_MAX); 
}

char* EXPORT IB_UDF_rtrim(
    char* s)
{
    char* p;
    char* q;
    char* buf;
    long length;

    if (length = strlen(s))
    {
	p = s + length - 1;
	while ((s != p) && (*p == ' '))
	    p--;
	length =  p - s + 1;
	buf = (char*) ib_util_malloc (length + 1);
	memcpy (buf, s, length);
	q = buf + length;
	*q = '\0';
    }
    else
	return NULL;

    return buf;
}

int EXPORT IB_UDF_sign(
    double* a)
{
    if (*a > 0)
	return 1;
    if (*a < 0)
	return -1;
    /* If neither is true then it eauals 0 */
    return 0;
}

double EXPORT IB_UDF_sin(
    double* a)
{
    return (sin(*a)); 
}

double EXPORT IB_UDF_sinh(
    double* a)
{
    return (sinh(*a)); 
}

double EXPORT IB_UDF_sqrt(
    double* a)
{
    return (sqrt(*a)); 
}

char* EXPORT IB_UDF_substr(
    char* s,
    short *m,
    short *n)
{
    char* p;
    char* buf;
    long length;

    if (length = strlen(s))
    {
	if (*m > *n || *m < 1 || *n < 1 ||
	    *m > length || *n > length) 
	    return NULL; 

	/* we want from the mth char to the
	   nth char inclusive, so add one to
	   the length. */
	length = *n - *m + 1;
	buf = (char*) ib_util_malloc (length + 1);
	memcpy(buf, s + *m - 1, length); 
	p = buf + length;
	*p = '\0';
    }
    else
	return NULL;

    return buf;
}

int EXPORT IB_UDF_strlen(
    char* a)
{
    return (strlen(a)); 
}

double EXPORT IB_UDF_tan(
    double* a)
{
    return (tan(*a)); 
}

double EXPORT IB_UDF_tanh(
    double* a)
{
    return (tanh(*a)); 
}



