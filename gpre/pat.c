/*
 *	PROGRAM:	Language Preprocessor
 *	MODULE:		pat.c
 *	DESCRIPTION:	Code generator pattern generator
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

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../gpre/gpre.h"
#include "../gpre/form.h"
#include "../gpre/pat.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/pat_proto.h"

extern TEXT	*ident_pattern;

typedef enum {
    NL,
    RH, RL, RT, RI, RS,	/* Request handle, level, transaction, ident, length */
    DH, DF,		/* Database handle, filename */
    TH,			/* Transaction handle */
    BH, BI,		/* Blob handle, blob_ident */
    FH,			/* Form handle */
    V1, V2,		/* Status vectors */
    I1, I2,		/* Identifier numbers */
    RF,	RE,		/* OS- and language-dependent REF and REF-end character */
    VF,	VE,		/* OS- and language-dependent VAL and VAL-end character */
    S1, S2, S3, S4, S5, S6, S7,
                        /* Arbitrary strings */
    N1,	N2, N3, N4,	/* Arbitrary number (SSHORT) */
    L1, L2,             /* Arbitrary number (SLONG) */
    PN, PL, PI,		/* Port number, port length, port ident */
    QN, QL, QI,		/* Second port number, port length, port ident */
    IF, EL, EN,		/* If, else, end */
    FR			/* Field reference */
} PAT_T;

static struct ops {
    PAT_T	ops_type;
    TEXT	ops_string [3];
} operators [] =
    {
    RH, "RH",
    RL, "RL",
    RT, "RT",
    RI, "RI",
    RS, "RS",
    DH, "DH",
    DF, "DF",
    TH, "TH",
    BH, "BH",
    BI, "BI",
    FH, "FH",
    V1, "V1",
    V2, "V2",
    I1, "I1",
    I2, "I2",
    S1, "S1",
    S2, "S2",
    S3, "S3",
    S4, "S4",
    S5, "S5",
    S6, "S6",
    S7, "S7",
    N1, "N1",
    N2, "N2",
    N3, "N3",
    N4, "N4",
    L1, "L1",
    L2, "L2",
    PN, "PN",
    PL, "PL",
    PI, "PI",
    QN, "QN",
    QL, "QL",
    QI, "QI",
    IF, "IF",
    EL, "EL",
    EN, "EN",
    RF, "RF",
    RE, "RE",
    VF, "VF",
    VE, "VE",
    FR, "FR",
    NL, ""
    };

void PATTERN_expand (
    USHORT	column,
    TEXT	*pattern,
    PAT		*args)
{
/**************************************
 *
 *	P A T T E R N _ e x p a n d
 *
 **************************************
 *
 * Functional description
 *	Expand a pattern.
 *
 **************************************/
struct ops	*operator;
TEXT		buffer [512], c, *p, temp1 [16], temp2 [16];
USHORT		sw_ident, sw_gen, n;
SSHORT		value;   /* value needs to be signed since some of the
			    values printed out are signed.  */
SLONG		long_value;
BOOLEAN		long_flag, handle_flag;
REF		reference;
TEXT		*string, *ref, *refend, *val, *valend, *q;

ref = "";
refend = "";
val = "";
valend = "";

#ifndef APOLLO
if ((sw_language == lan_c) || (sw_language == lan_cxx))
    {
    ref = "&";
    refend = "";
    }
else if (sw_language == lan_pli)
    {
    ref = "ADDR(";
    refend = ")";
    }
else if (sw_language == lan_pascal)
    {
    ref = "%%REF ";
    refend = "";
    }
else if (sw_language == lan_cobol)
    {
#ifdef mpexl
    ref = "@ ";
#else
    ref = "BY REFERENCE ";
#endif
    refend = "";
#ifdef mpexl
    val = "\\";
    valend = "\\";
#else
    val = "BY VALUE ";
    valend = "";
#endif
    }
else if (sw_language == lan_fortran)
    {
#ifdef VMS
    ref = "%REF(";
    refend = ")";
    val = "%VAL(";
    valend = ")";
#endif
#if (defined AIX || defined AIX_PPC)
    ref = "%REF(";
    refend = ")";
    val = "%VAL(";
    valend = ")";
#endif
    }
#endif

p = buffer;
*p++ = '\n';
sw_gen = TRUE;
for (n = column; n; --n)
    *p++ = ' ';

while (c = *pattern++)
    {
    if (c != '%')
	{
	if (sw_gen)
	    {
	    *p++ = c;
	    if ((c == '\n') && (*pattern))
		for (n = column; n; --n)
		    *p++ = ' ';
	    }
	continue;
	}
    sw_ident = FALSE;
    string = NULL;
    reference = NULL;
    handle_flag = long_flag = FALSE;
    for (operator = operators; operator->ops_type != NL; operator++)
	if (operator->ops_string [0] == pattern [0] &&
	    operator->ops_string [1] == pattern [1])
	    break;
    pattern += 2;
    switch (operator->ops_type)
	{
	case IF:
	    sw_gen = args->pat_condition;
	    continue;
	
	case EL:
	    sw_gen = !sw_gen;
	    continue;
	
	case EN:
	    sw_gen = TRUE;
	    continue;

	case RH:
	    handle_flag = TRUE;
	    string = args->pat_request->req_handle;
	    break;
	
	case RL:
	    string = args->pat_request->req_request_level;
	    break;
	
	case RS:
	    value = args->pat_request->req_length;
	    break;
	
	case RT:
	    handle_flag = TRUE;
	    string = args->pat_request->req_trans;
	    break;
	
	case RI:
	    value = args->pat_request->req_ident;
	    sw_ident = TRUE;
	    break;

	case DH:
	    handle_flag = TRUE;
	    string = args->pat_database->dbb_name->sym_string;
	    break;
	
	case DF:
	    string = args->pat_database->dbb_filename;
	    break;
	
	case PN:
	    value = args->pat_port->por_msg_number;
	    break;
	
	case PL:
	     value = args->pat_port->por_length;
	     break;
	    
	case PI:
	     value = args->pat_port->por_ident;
	     sw_ident = TRUE;
	     break;
	    
	case QN:
	    value = args->pat_port2->por_msg_number;
	    break;
	
	case QL:
	     value = args->pat_port2->por_length;
	     break;
	    
	case QI:
	     value = args->pat_port2->por_ident;
	     sw_ident = TRUE;
	     break;
	    
	case BH:
	    value = args->pat_blob->blb_ident;
	    sw_ident = TRUE;
	    break;

	case I1:
	     value = args->pat_ident1;
	     sw_ident = TRUE;
	     break;
	    
	case I2:
	     value = args->pat_ident2;
	     sw_ident = TRUE;
	     break;
	    
	case S1:
	    string = args->pat_string1;
	    break;
	     
	case S2:
	    string = args->pat_string2;
	    break;

	case S3:
	    string = args->pat_string3;
	    break;
	     
	case S4:
	    string = args->pat_string4;
	    break;
	
	case S5:
	    string = args->pat_string5;
	    break;
	     
	case S6:
	    string = args->pat_string6;
	    break;
	
	case S7:
	    string = args->pat_string7;
	    break;
	
	case V1:
	     string = args->pat_vector1;
	     break;

	case V2:
	     string = args->pat_vector2;
	     break;
	    
	case N1:
	    value = args->pat_value1;
	    break;
	
	case N2:
	    value = args->pat_value2;
	    break;
	
	case N3:
	    value = args->pat_value3;
	    break;
	
	case N4:
	    value = args->pat_value4;
	    break;
	
	case L1:
	    long_value = args->pat_long1;
	    long_flag = TRUE;
	    break;

	case L2:
	    long_value = args->pat_long2;
	    long_flag = TRUE;
	    break;

	case RF:
	    string = ref;
	    break;

	case RE:
	    string = refend;
	    break;

	case VF:
	    string = val;
	    break;

	case VE:
	    string = valend;
	    break;

	case FR:
	   reference = args->pat_reference;
	   break;

	default:
	    sprintf (buffer, "Unknown substitution \"%c%c\"", pattern [-2], pattern [-1]);
	    IBERROR (buffer);
	    continue;
	}
    if (!sw_gen)
	continue;
    if (string)
	{
	if (handle_flag && (sw_language == lan_ada))
	    for (q = ada_package; *q;)
		*p++ = *q++;
	while (*string)
	    *p++ = *string++;
	continue;
	}
    if (sw_ident)
	sprintf (p, ident_pattern, value);
    else if (reference)
	{
	if (!reference->ref_port)
	    sprintf (p, ident_pattern, reference->ref_ident);
	else
	    {
	    sprintf (temp1, ident_pattern, reference->ref_port->por_ident);
	    sprintf (temp2, ident_pattern, reference->ref_ident);
	    switch (sw_language)
		{
		case lan_basic:
		    sprintf (p, "%s::%s", temp1, temp2);
		    break;

		case lan_fortran:
		case lan_cobol:
		    strcpy (p, temp2);
		    break;

		default:
		    sprintf (p, "%s.%s", temp1, temp2);
		}
	    }
	}
    else if (long_flag)
	{
	if (sw_language == lan_basic)
	    sprintf (p, "%d%%", long_value);
	else
	    sprintf (p, "%d", long_value);
	}
    else
	{
	if (sw_language == lan_basic)
	    sprintf (p, "%d%%", value);
	else
	    sprintf (p, "%d", value);
	}

    while (*p)
	p++;
    }

*p = 0;
switch (sw_language)
    {
#ifdef ADA
    /*  Ada lines can be up to 120 characters long.  ADA_print_buffer
        handles this problem and ensures that GPRE output is <=120 characters.
    */
    case lan_ada:
	ADA_print_buffer (buffer, 0);
	break;
#endif

#ifdef BASIC
    case lan_basic:
    /*  Basic lines can be up to 72 characters long.  BAS_print_buffer
        handles this problem and ensures that GPRE output is <= 72 characters.
    */
	BAS_print_buffer (buffer);
	break;
#endif

#ifdef COBOL
    /*  COBOL lines can be up to 72 characters long.  COB_print_buffer
        handles this problem and ensures that GPRE output is <= 72 characters.
    */
    case lan_cobol:
	COB_print_buffer (buffer, TRUE);
	break;
#endif

#ifdef FORTRAN
    /*  In FORTRAN, characters beyond column 72 are ignored.  FTN_print_buffer
        handles this problem and ensures that GPRE output does not exceed this
        limit.
    */
    case lan_fortran:
	FTN_print_buffer (buffer);
	break;
#endif

    default:
	ib_fprintf (out_file, buffer);
	break;
    }
}
