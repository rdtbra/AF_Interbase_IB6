/*
 *	PROGRAM:	Interbase Engine
 *	MODULE:		envelope.e
 *	DESCRIPTION:	Envelope generator
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
#include "../jrd/gds.h"

database db = "source/jrd/gds_functions.gdb";

typedef enum mech_t {
    mech_unkown = 1,
    mech_status_vector,
    mech_char_pointer,
    mech_cstring,
    mech_long,
    mech_short,
    mech_handle,
    mech_short_pointer,
    mech_long_pointer,
    mech_quad_pointer,
    mech_array_pointer,
    mech_routine_pointer,
    mech_teb_vector,
    mech_sqlda,
    mech_int,
    mech_vax_desc,
    mech_int_pointer
} MECH_T;

static IB_FILE	*output;



main (argc, argv)
    int		argc;
    SCHAR	**argv;
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Generate envelope.
 *
 **************************************/
IB_FILE	*input;
TEXT	*p, **end, *envelope, *component;
SSHORT	c;

ready;
start_transaction;
output = ib_stdout;
envelope = "isc";
component = "JRD";

for (end = argv + argc, ++argv; argv < end;)
    {
    p = *argv++;
    if (*p == '-')
	switch (p [1])
	    {
	    case 'o':
		output = ib_fopen (*argv++, "w");
		break;

	    case 'e':
		envelope = *argv++;
		break;

	    case 'c':
		component = *argv++;
		break;

	    default:
		ib_fprintf (ib_stderr, "don't understand switch \"%s\"\n", p);
		abort (1);
	    }
    else
	ib_fprintf (ib_stderr, "don't understand \"%s\"\n", p);
    }

for x in boilerplates with x.envelope_name eq envelope
    for y in x.boilerplate
	y.segment [y.length] = 0;
	ib_fprintf (output, y.segment);
    end_for;
end_for;

for f in functions cross e in envelopes over function_name with e.envelope_name eq envelope
    and f.component eq component
	sorted by f.function_type, f.function_name
    if (strcmp (e.ifdef, ""))
	ib_fprintf (output, "#ifdef %s\n", e.ifdef);
    gen_function (f.function_name, e.new_name, f.std_call, f.return_type, e.add_status_vector);
    if (strcmp (e.ifdef, ""))
	ib_fprintf (output, "#endif\n");
end_for;

commit;
finish;
ib_fclose (output);
}

static gen_function (name, new_name, std_call, return_type, add_status_vector)
    TEXT	*name;
    TEXT	*new_name;
    USHORT	std_call;
    TEXT	*return_type;
    USHORT	add_status_vector;
{
/**************************************
 *
 *	g e n _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate an envelope subroutine for an architectural defined
 *	function (std_$call on Apollo).
 *
 **************************************/
TEXT	*separator, *dtype;
USHORT	n;

ib_fprintf (output, "%s %s (", return_type, new_name);
separator = "";

if (add_status_vector)
    {
    ib_fprintf (output, "status_vector");
    separator = ", ";
    }

for x in arguments with x.function_name eq name sorted by x.position
    ib_fprintf (output, "%s%s", separator, x.argument_name);
    separator = ", ";
end_for;

ib_fprintf (output, ")\n");
n = 0;

if (add_status_vector)
    ib_fprintf (output, "    long	*status_vector;\n");

for x in arguments cross y in argument_types 
	over argument_type 
	with x.function_name eq name sorted by x.position
    if (x.position != ++n)
	ib_fprintf (ib_stderr, "argument %s of %s is out of sequence\n", x.argument_name, name);
    switch (get_dtype (y.argument_type))
	{
	case mech_status_vector:
	    ib_fprintf (output, "    STATUS	*%s;\n", x.argument_name);
	    break;
	
	case mech_handle:
	    ib_fprintf (output, "    long	*%s;\n", x.argument_name);
	    break;

	case mech_short:
	    ib_fprintf (output, "    short	%s;\n", x.argument_name);
	    break;

	case mech_long:
	    ib_fprintf (output, "    long	%s;\n", x.argument_name);
	    break;
	
	case mech_int:
	    ib_fprintf (output, "    int	%s;\n", x.argument_name);
	    break;
	
	case mech_char_pointer:
	    ib_fprintf (output, "    char	*%s;\n", x.argument_name);
	    break;

	case mech_short_pointer:
	    ib_fprintf (output, "    short	*%s;\n", x.argument_name);
	    break;

	case mech_long_pointer:
	    ib_fprintf (output, "    long	*%s;\n", x.argument_name);
	    break;

	case mech_int_pointer:
	    ib_fprintf (output, "    int	*%s;\n", x.argument_name);
	    break;

	case mech_quad_pointer:
	    ib_fprintf (output, "    GDS__QUAD	*%s;\n", x.argument_name);
	    break;

	case mech_array_pointer:
	    ib_fprintf (output, "    char		*%s;\n", x.argument_name);
	    break;

	case mech_routine_pointer:
	    ib_fprintf (output, "    int	(*%s)();\n", x.argument_name);
	    break;

	case mech_teb_vector:
	    ib_fprintf (output, "    TEB	*%s;\n", x.argument_name);
	    break;

	case mech_sqlda:
	    ib_fprintf (output, "    SQLDA	*%s;\n", x.argument_name);
	    break;

	case mech_vax_desc:
	    ib_fprintf (output, "    long	*%s;\n", x.argument_name);
	    break;

	default:
	    ib_printf ("gen: don't understand argument_type %s in %s\n", y.argument_type, name);
	}
end_for;

ib_fprintf (output, "{\n");

if (add_status_vector)
    ib_fprintf (output, "if (status_vector)\n    {\n    status_vector [0] = 1;\n    status_vector [1] = 0;\n    }\n");

if (!strcmp (return_type, "void"))
    ib_fprintf (output, "%s (", name);
else
    ib_fprintf (output, "return %s (", name);

separator = "";

for x in arguments cross y in argument_types 
	over argument_type 
	with x.function_name eq name sorted by x.position
    switch (get_dtype (y.argument_type))
	{
	case mech_short:
	case mech_long:
	    ib_fprintf (output, "%s%s", separator, x.argument_name);
	    break;

	default:
	    if (std_call)
		ib_fprintf (output, "%sGDS_VAL(%s)", separator, x.argument_name);
	    else
		ib_fprintf (output, "%s%s", separator, x.argument_name);
	}
    separator = ",\n\t";
end_for;

ib_fprintf (output, ");\n}\n\n");
}

static get_dtype (string)
    TEXT	*string;
{
/**************************************
 *
 *	g e t _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Translate from argument type string to name.
 *
 **************************************/

#define TEST(name, mech)	if (!strcmp (string, name)) return mech

TEST ("short", mech_short);
TEST ("long", mech_long);
TEST ("handle", mech_handle);
TEST ("status_vector", mech_status_vector);
TEST ("char_pointer", mech_char_pointer);
TEST ("filename", mech_char_pointer);
TEST ("short_pointer", mech_short_pointer);
TEST ("long_pointer", mech_long_pointer);
TEST ("blob_id", mech_quad_pointer);
TEST ("array_pointer", mech_array_pointer);
TEST ("routine_pointer", mech_routine_pointer);
TEST ("teb_vector", mech_teb_vector);
TEST ("sqlda", mech_sqlda);
TEST ("int", mech_int);
TEST ("int_pointer", mech_int_pointer);
TEST ("vax_desc", mech_vax_desc);

ib_printf ("don't understand mechanism %s\n", string);
}
