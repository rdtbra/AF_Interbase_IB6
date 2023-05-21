/*
 *	PROGRAM:	C preprocess
 *	MODULE:		int.c
 *	DESCRIPTION:	Code generate for internal JRD modules
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
#include "../jrd/common.h"
#include <stdarg.h>
#include "../jrd/gds.h"
#include "../gpre/gpre.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/lang_proto.h"
#include "../jrd/gds_proto.h"

static void	align (int);
static void	asgn_from (REF, int);
static void	asgn_to (REF);
static void	gen_at_end (ACT, int);
static int	gen_blr (int *, int, TEXT *);
static void	gen_compile (REQ, int);
static void	gen_database (ACT, int);
static void	gen_emodify (ACT, int);
static void	gen_estore (ACT, int);
static void	gen_endfor (ACT, int);
static void	gen_erase (ACT, int);
static void	gen_for (ACT, int);
static TEXT	*gen_name (TEXT *, REF);
static void	gen_raw (REQ);
static void	gen_receive (REQ, POR);
static void	gen_request (REQ);
static void	gen_routine (ACT, int);
static void	gen_s_end (ACT, int);
static void	gen_s_fetch (ACT, int);
static void	gen_s_start (ACT, int);
static void	gen_send (REQ, POR, int);
static void	gen_start (REQ, POR, int);
static void	gen_type (ACT, int);
static void	gen_variable (ACT, int);
static void	make_port (POR, int);
static void	printa (int, TEXT *, ...);

static int	first_flag = 0;

#define INDENT 3
#define BEGIN		printa (column, "{")
#define END		printa (column, "}")

#if !(defined JPN_SJIS || defined JPN_EUC)
#define GDS_VTOV	"gds__vtov"
#define JRD_VTOF	"jrd_vtof"
#define VTO_CALL	"%s (%s, %s, %d);"
#else
#define GDS_VTOV	"gds__vtov2"
#define JRD_VTOF	"jrd_vtof2"
#define VTO_CALL	"%s (%s, %s, %d, %d);"
#endif

void INT_action (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	I N T _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

/* Put leading braces where required */

switch (action->act_type) 
    {
    case ACT_for:
    case ACT_insert:
    case ACT_modify:
    case ACT_store:
    case ACT_s_fetch:
    case ACT_s_start:
	BEGIN;
	align (column);
    }

switch (action->act_type) 
    {
    case ACT_at_end	: gen_at_end (action, column);		return;
    case ACT_b_declare	:
    case ACT_database	: gen_database (action, column); 	return;
    case ACT_endfor	: gen_endfor (action, column); break;
    case ACT_endmodify	: gen_emodify (action, column); break;
    case ACT_endstore	: gen_estore (action, column); break;
    case ACT_erase	: gen_erase (action, column); 		return;
    case ACT_for	: gen_for (action, column); 		return;
    case ACT_hctef	: break;
    case ACT_insert	: 
    case ACT_routine	: gen_routine (action, column);		return;
    case ACT_s_end	: gen_s_end (action, column);		return;
    case ACT_s_fetch	: gen_s_fetch (action, column);		return;
    case ACT_s_start	: gen_s_start (action, column);	break;
    case ACT_type	: gen_type (action, column);		return;
    case ACT_variable	: gen_variable (action, column);	return;
    default :
	return;
    }

/* Put in a trailing brace for those actions still with us */

END;
}
 
static void align (
    int	column)
{
/**************************************
 *
 *	a l i g n
 *
 **************************************
 *
 * Functional description
 *	Align output to a specific column for output.
 *
 **************************************/
int	i;

if (column < 0)
    return;

ib_putc ('\n', out_file);

for (i = column / 8; i; --i)
    ib_putc ('\t', out_file);

for (i = column % 8; i; --i)
    ib_putc (' ', out_file);
}

static void asgn_from (
    REF	reference,
    int	column)
{
/**************************************
 *
 *	a s g n _ f r o m
 *
 **************************************
 *
 * Functional description
 *	Build an assignment from a host language variable to
 *	a port variable.
 *
 **************************************/
FLD	field;
TEXT	*value, variable [20], temp [20];

for (; reference; reference = reference->ref_next)
    {
    field = reference->ref_field;
    align (column);
    gen_name (variable, reference);
    if (reference->ref_source)
	value = gen_name (temp, reference->ref_source);
    else
	value = reference->ref_value;

    /* To avoid chopping off a double byte kanji character in between
       the two bytes, generate calls to gds__ftof2 gds$_vtof2,
       gds$_vtov2 and jrd_vtof2 wherever necessary */

    if (!field || field->fld_dtype == dtype_text)
	ib_fprintf (out_file, VTO_CALL,
	    JRD_VTOF,
	    value,
	    variable,
	    field->fld_length,
	    sw_interp);
    else if (!field || field->fld_dtype == dtype_cstring)
	ib_fprintf (out_file, VTO_CALL,
	    GDS_VTOV,
	    value,
	    variable,
	    field->fld_length,
	    sw_interp);
    else
	ib_fprintf (out_file, "%s = %s;", variable, value);
    }

}

static void asgn_to (
    REF	reference)
{
/**************************************
 *
 *	a s g n _ t o
 *
 **************************************
 *
 * Functional description
 *	Build an assignment to a host language variable from
 *	a port variable.
 *
 **************************************/
FLD	field;
REF	source;
TEXT	s [20];

source = reference->ref_friend;
field = source->ref_field;
gen_name (s, source);

#if (! (defined JPN_SJIS || defined JPN_EUC) )

if (!field || field->fld_dtype == dtype_text)
    ib_fprintf (out_file, "gds__ftov (%s, %d, %s, sizeof (%s));", 
	s,
	field->fld_length,
	reference->ref_value,
	reference->ref_value);
else if (!field || field->fld_dtype == dtype_cstring)
    ib_fprintf (out_file, "gds__vtov (%s, %s, sizeof (%s));", 
	s,
	reference->ref_value,
	reference->ref_value);

#else

/* To avoid chopping off a double byte kanji character in between
   the two bytes, generate calls to gds__ftof2 gds$_vtof2 and
   gds$_vtov2 wherever necessary */

if (!field || field->fld_dtype == dtype_text)
    ib_fprintf (out_file, "gds__ftov2 (%s, %d, %s, sizeof (%s), %d);", 
	s,
	field->fld_length,
	reference->ref_value,
	reference->ref_value,
	sw_interp);
else if (!field || field->fld_dtype == dtype_cstring)
    ib_fprintf (out_file, "gds__vtov2 (%s, %s, sizeof (%s), %d);", 
	s,
	reference->ref_value,
	reference->ref_value,
	sw_interp);

#endif
else
    ib_fprintf (out_file, "%s = %s;", reference->ref_value, s);
}

static void gen_at_end (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ a t _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate code for AT END clause of FETCH.
 *
 **************************************/
REQ	request;
TEXT	s [20];

request = action->act_request;
printa (column, "if (!%s) ", gen_name (s, request->req_eof));
}

static int gen_blr (
    int		*user_arg,
    int		offset,
    TEXT	*string)
{
/**************************************
 *
 *	g e n _ b l r
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/

ib_fprintf (out_file, "%s\n", string);

return TRUE;
}

static void gen_compile (
    REQ	request,
    int	column)
{
/**************************************
 *
 *	g e n _ c o m p i l e
 *
 **************************************
 *
 * Functional description
 *	Generate text to compile a request.
 *
 **************************************/
DBB	db;
SYM	symbol;

column += INDENT;
db = request->req_database;
symbol = db->dbb_name;
ib_fprintf (out_file, "if (!%s)", request->req_handle);
align (column);
ib_fprintf (out_file, "%s = (BLK) CMP_compile2 (tdbb, jrd_%d, TRUE);",
	request->req_handle, request->req_ident);
}

static void gen_database (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate insertion text for the database statement.
 *
 **************************************/
REQ	request;

if (first_flag++ != 0)
    return;

align (0);
for (request = requests; request; request = request->req_next)
    gen_request (request);
}

static void gen_emodify (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ e m o d i f y
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_MODIFY.
 *
 **************************************/
UPD	modify;
REF	reference, source;
FLD	field;
TEXT	s1 [20], s2 [20];

modify = (UPD) action->act_object;

for (reference= modify->upd_port->por_references; reference; 
    reference = reference->ref_next)	
    {
    if (!(source = reference->ref_source))
	continue;
    field = reference->ref_field;
    align (column);

#if (! (defined JPN_SJIS || defined JPN_EUC) )

    if (field->fld_dtype == dtype_text)
	ib_fprintf (out_file, "jrd_ftof (%s, %d, %s, %d);",
	    gen_name (s1, source),
	    field->fld_length,
	    gen_name (s2, reference),
	    field->fld_length);
    else if (field->fld_dtype == dtype_cstring)
	ib_fprintf (out_file, "gds__vtov (%s, %s, %d);", 
	    gen_name (s1, source),
	    gen_name (s2, reference),
	    field->fld_length);

#else

    /* To avoid chopping off a double byte kanji character in between
       the two bytes, generate calls to gds__ftof2 gds$_vtof2 and
       gds$_vtov2 wherever necessary
       Cannot find where jrd_fof is defined. It needs Japanization too */

    if (field->fld_dtype == dtype_text)
	ib_fprintf (out_file, "jrd_ftof (%s, %d, %s, %d);",
	    gen_name (s1, source),
	    field->fld_length,
	    gen_name (s2, reference),
	    field->fld_length);
    else if (field->fld_dtype == dtype_cstring)
	ib_fprintf (out_file, "gds__vtov2 (%s, %s, %d, %d);", 
	    gen_name (s1, source),
	    gen_name (s2, reference),
	    field->fld_length,
	    sw_interp);

#endif
    else
	ib_fprintf (out_file, "%s = %s;",
	    gen_name (s1, reference),
	    gen_name (s2, source));
    }

gen_send (action->act_request, modify->upd_port, column);
}

static void gen_estore (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ e s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STORE.
 *
 **************************************/
REQ	request;

request = action->act_request;
align (column);
gen_compile (request, column);
gen_start (request, request->req_primary, column);
}

static void gen_endfor (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ e n d f o r
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/
REQ	request;

request = action->act_request;
column += INDENT;

if (request->req_sync)
    gen_send (request, request->req_sync, column);

END;
}

static void gen_erase (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ e r a s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for ERASE.
 *
 **************************************/
UPD	erase;

erase = (UPD) action->act_object;
gen_send (erase->upd_request, erase->upd_port, column);
}

static void gen_for (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ f o r 
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FOR statement.
 *
 **************************************/
REQ	request;
TEXT	s [20];

gen_s_start (action, column);
    
align (column);
request = action->act_request;
ib_fprintf (out_file, "while (1)");
    column += INDENT;
    BEGIN;
    align (column);
    gen_receive (action->act_request, request->req_primary);
    align (column);
    ib_fprintf (out_file, "if (!%s) break;", gen_name (s, request->req_eof));
}

static TEXT *gen_name (
    TEXT	*string,
    REF		reference)
{
/**************************************
 *
 *	g e n _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Generate a name for a reference.  Name is constructed from
 *	port and parameter idents.
 *
 **************************************/

sprintf (string, "jrd_%d.jrd_%d", 
    reference->ref_port->por_ident, 
    reference->ref_ident);

return string;
}

static void gen_raw (
    REQ	request)
{
/**************************************
 *
 *	g e n _ r a w
 *
 **************************************
 *
 * Functional description
 *	Generate BLR in raw, numeric form.  Ugly but dense.
 *
 **************************************/
UCHAR	*blr, c;
TEXT	buffer[80], *p;
int	blr_length;

blr = request->req_blr;
blr_length = request->req_length;
p = buffer;
align (0);

while (--blr_length)
    {
    c = *blr++;
    if ((c >= 'A' && c <= 'Z') || c == '$' || c == '_')
	sprintf (p, "'%c',", c);
    else
	sprintf (p, "%d,", c);
    while (*p)
	p++;
    if (p - buffer > 60)
	{
	ib_fprintf (out_file, "%s\n", buffer);
	p = buffer;
	*p = 0;
	}
    }

ib_fprintf (out_file, "%s%d", buffer, blr_eoc);
}

static void gen_receive (
    REQ		request,
    POR		port)
{
/**************************************
 *
 *	g e n _ r e c e i v e
 *
 **************************************
 *
 * Functional description
 *	Generate a send or receive call for a port.
 *
 **************************************/

ib_fprintf (out_file, "EXE_receive (tdbb, %s, %d, %d, &jrd_%d);",
    request->req_handle,	port->por_msg_number,
    port->por_length,		port->por_ident);
}

static void gen_request (
    REQ	request)
{
/**************************************
 *
 *	g e n _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/

if (!(request->req_flags & REQ_exp_hand))
    ib_fprintf (out_file, "static void\t*%s;\t/* request handle */\n", 
		request->req_handle);

ib_fprintf (out_file, "static CONST UCHAR\tjrd_%d [%d] =", 
		request->req_ident, request->req_length);
align (INDENT);
ib_fprintf (out_file, "{\t/* blr string */\n", request->req_ident);

if (sw_raw)
    gen_raw (request);
else
    gds__print_blr (request->req_blr, (FPTR_VOID) gen_blr, NULL_PTR, 0);

printa (INDENT, "};\t/* end of blr string */\n");
}

static void gen_routine (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ r o u t i n e
 *
 **************************************
 *
 * Functional description
 *	Process routine head.  If there are requests in the
 *	routine, insert local definitions.
 *
 **************************************/
REQ	request;
POR	port;

for (request = (REQ) action->act_object; request; request = request->req_routine)
    for (port = request->req_ports; port; port = port->por_next)
	make_port (port, column + INDENT);
}

static void gen_s_end (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ s _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STREAM.
 *
 **************************************/
REQ	request;

request = action->act_request;
printa (column, "EXE_unwind (tdbb, %s);", request->req_handle);
}

static void gen_s_fetch (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ s _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FETCH.
 *
 **************************************/
REQ	request;

request = action->act_request;
if (request->req_sync)
    gen_send (request, request->req_sync, column);

gen_receive (action->act_request, request->req_primary);
}

static void gen_s_start (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ s _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate text to compile and start a stream.  This is
 *	used both by START_STREAM and FOR
 *
 **************************************/
REQ	request;
POR	port;

request = action->act_request;
gen_compile (request, column);

if (port = request->req_vport)
    asgn_from (port->por_references, column);

gen_start (request, port, column);
}

static void gen_send (
    REQ		request,
    POR		port,
    int		column)
{
/**************************************
 *
 *	g e n _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Generate a send or receive call for a port.
 *
 **************************************/
align (column);
ib_fprintf (out_file, "EXE_send (tdbb, %s, %d, %d, &jrd_%d);",
    request->req_handle,
    port->por_msg_number,
    port->por_length,
    port->por_ident);
}

static void gen_start (
    REQ	request,
    POR	port,
    int	column)
{
/**************************************
 *
 *	g e n _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate a START.
 *
 **************************************/
align (column);

ib_fprintf (out_file, "EXE_start (tdbb, %s, %s);", request->req_handle, request->req_trans);

if (port)
    gen_send (request, port, column);
}

static void gen_type (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	g e n _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Substitute for a variable reference.
 *
 **************************************/

printa (column, "%ld", action->act_object);
}

static void gen_variable (
    ACT	action,
    int	column)
{
/**************************************
 *
 *	g e n _ v a r i a b l e 
 *
 **************************************
 *
 * Functional description
 *	Substitute for a variable reference.
 *
 **************************************/
TEXT	s [20];

align (column);
ib_fprintf (out_file, gen_name (s, action->act_object));

}

static void make_port (
    POR	port,
    int	column)
{
/**************************************
 *
 *	m a k e _ p o r t
 *
 **************************************
 *
 * Functional description
 *	Insert a port record description in output.
 *
 **************************************/
FLD	field;
REF	reference;
SYM	symbol;
TEXT	*name, s [50];

printa (column, "struct {");

for (reference = port->por_references; reference; 
    reference = reference->ref_next)
    {
    align (column + INDENT);
    field = reference->ref_field;
    symbol = field->fld_symbol;
    name = symbol->sym_string;
    switch (field->fld_dtype) 
	{
	case dtype_short:
	    ib_fprintf (out_file, "    SSHORT jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	case dtype_long:
	    ib_fprintf (out_file, "    SLONG  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

/** Begin sql date/time/timestamp **/
	case dtype_sql_date:
	    ib_fprintf (out_file, "    ISC_DATE  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	case dtype_sql_time:
	    ib_fprintf (out_file, "    ISC_TIME  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	case dtype_timestamp:
	    ib_fprintf (out_file, "    ISC_TIMESTAMP  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;
/** End sql date/time/timestamp **/

	case dtype_int64:
	    ib_fprintf (out_file, "    ISC_INT64  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	case dtype_quad:
	case dtype_blob:
	    ib_fprintf (out_file, "    GDS__QUAD  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;
	    
	case dtype_cstring:
	case dtype_text:
	    ib_fprintf (out_file, "    TEXT  jrd_%d [%d];\t/* %s */",
		reference->ref_ident, field->fld_length, name);
	    break;

	case dtype_float:
	    ib_fprintf (out_file, "    float  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	case dtype_double:
	    ib_fprintf (out_file, "    double  jrd_%d;\t/* %s */",
		reference->ref_ident, name);
	    break;

	default:
	    sprintf (s, "datatype %d unknown for field %s, msg %d",
		field->fld_dtype, name,
		port->por_msg_number);
	    CPR_error (s);
	    return;
	}
    }
align (column);
ib_fprintf (out_file,"} jrd_%d;", port->por_ident);
}

static void printa (
    int		column,
    TEXT	*string,
    ...)
{
/**************************************
 *
 *	p r i n t a
 *
 **************************************
 *
 * Functional description
 *	Print a fixed string at a particular column.
 *
 **************************************/
va_list	ptr;

VA_START (ptr, string);
align (column);
ib_vfprintf (out_file, string, ptr);
}
