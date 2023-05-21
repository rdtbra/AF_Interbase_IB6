/*
 *	PROGRAM:	BLR Pretty Printer
 *	MODULE:		pretty.c
 *	DESCRIPTION:	BLR Pretty Printer
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
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"

#ifdef sun
#ifndef SOLARIS
extern int	ib_printf();
#endif
#endif

#define ADVANCE_PTR(ptr) while (*ptr) ptr++;

#define PRINT_VERB 	if (print_verb (control, level)) return -1
#define PRINT_DYN_VERB 	if (print_dyn_verb (control, level)) return -1
#define PRINT_SDL_VERB 	if (print_sdl_verb (control, level)) return -1
#define PRINT_LINE	print_line (control, offset)
#define PRINT_BYTE	print_byte (control, offset)
#define PRINT_CHAR	print_char (control, offset)
#define PRINT_WORD	print_word (control, offset)
#define PRINT_LONG	print_long (control, offset)
#define PRINT_STRING	print_string (control, offset)
#define BLR_BYTE	*(control->ctl_blr)++
#define PUT_BYTE(byte)	*(control->ctl_ptr)++ = byte
#define NEXT_BYTE	*(control->ctl_blr)
#define CHECK_BUFFER	if (control->ctl_ptr > control->ctl_buffer + sizeof (control->ctl_buffer) - 20) PRINT_LINE

typedef struct ctl {
    SCHAR	*ctl_blr;		/* Running blr string */
    SCHAR	*ctl_blr_start;		/* Original start of blr string */
    int		(*ctl_routine)();	/* Call back */
    SCHAR	*ctl_user_arg;		/* User argument */
    TEXT	*ctl_ptr;
    SSHORT	ctl_language;
    SSHORT	ctl_level;
    TEXT	ctl_buffer [256];
} *CTL;

static int	blr_format (CTL, TEXT *, ...);
static int	error (CTL, int, TEXT *, int *);
static int	indent (CTL, SSHORT);
static int	print_blr_dtype (CTL, BOOLEAN);
static int	print_blr_line (CTL, USHORT, UCHAR *);
static int	print_byte (CTL, SSHORT);
static int	print_char (CTL, SSHORT);
static int	print_dyn_verb (CTL, SSHORT);
static int	print_line (CTL, SSHORT);
static SLONG	print_long (CTL, SSHORT);
static int	print_sdl_verb (CTL, SSHORT);
static int	print_string (CTL, SSHORT);
static int	print_word (CTL, SSHORT);

static TEXT	*dyn_table [] = {
#include "../gpre/dyntable.h"
    0
};

static TEXT	*cdb_table [] = {
#include "../gpre/cdbtable.h"
    0
};

static TEXT	*sdl_table [] = {
#include "../gpre/sdltable.h"
    0
};

static TEXT	*map_strings [] = {     
    "FIELD2",
    "FIELD1",
    "MESSAGE",
    "TERMINATOR",
    "TERMINATING_FIELD",
    "OPAQUE",
    "TRANSPARENT",
    "TAG",
    "SUB_FORM",
    "ITEM_INDEX",
    "SUB_FIELD"
};

static TEXT	*menu_strings [] = {
    "LABEL",
    "ENTREE",
    "OPAQUE",
    "TRANSPARENT",
    "HORIZONTAL",
    "VERTICAL"
};

PRETTY_print_cdb (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ c d b
 *
 **************************************
 *
 * Functional description
 *	Pretty print create database parameter buffer thru callback routine.
 *
 **************************************/
struct ctl	ctl, *control;
SCHAR		temp [32];
SCHAR		*p;
int		offset;
SSHORT		parameter, level, length, i;

control = &ctl;
offset = level = 0;

if (!routine)
    {
    routine = (int(*)()) ib_printf;
    user_arg = "%.4d %s\n";
    }

control->ctl_routine = routine;
control->ctl_user_arg = user_arg;
control->ctl_blr = control->ctl_blr_start = blr;
control->ctl_ptr = control->ctl_buffer;
control->ctl_language = language;

indent (control, level);
i = BLR_BYTE;
if (*control->ctl_blr)
    sprintf ((SCHAR*) temp, "gds__dpb_version%d, ", i);
else
    sprintf ((SCHAR*) temp, "gds__dpb_version%d", i);
blr_format (control, temp);
PRINT_LINE;

while (parameter = BLR_BYTE)
    {
    if (parameter > (sizeof (cdb_table) / sizeof (cdb_table [0])) ||
	!(p = cdb_table [parameter]))
	return error (control, 0, "*** cdb parameter %d is undefined ***\n", (int *)parameter);
    indent (control, level);
    blr_format (control, p);
    PUT_BYTE (',');
    if (length = PRINT_BYTE)
	do PRINT_CHAR; while (--length);
    PRINT_LINE;
    }

return 0;
}

PRETTY_print_dyn (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ d y n
 *
 **************************************
 *
 * Functional description
 *	Pretty print dynamic DDL thru callback routine.
 *
 **************************************/
struct ctl	ctl, *control;
int		offset;
SSHORT		version, level;

control = &ctl;
offset = level = 0;

if (!routine)
    {
    routine = (int(*)()) ib_printf;
    user_arg = "%.4d %s\n";
    }

control->ctl_routine = routine;
control->ctl_user_arg = user_arg;
control->ctl_blr = control->ctl_blr_start = blr;
control->ctl_ptr = control->ctl_buffer;
control->ctl_language = language;

version = BLR_BYTE;

if (version != gds__dyn_version_1)
    return error (control, offset, "*** dyn version %d is not supported ***\n", (int *)version);

blr_format (control, "gds__dyn_version_1, ");
PRINT_LINE;
level++;
PRINT_DYN_VERB;

if (BLR_BYTE != (SCHAR) gds__dyn_eoc)
    return error (control, offset, "*** expected dyn end-of-command  ***\n", 0);

blr_format (control, "gds__dyn_eoc");
PRINT_LINE;

return 0;
}

PRETTY_print_form_map (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ f o r m _ m a p
 *
 **************************************
 *
 * Functional description
 *	Pretty print a form map.
 *
 **************************************/
struct ctl	ctl, *control;
SCHAR		c;
int		offset, n;
SSHORT		version, level;

control = &ctl;
offset = level = 0;

if (! routine)
    {
    routine = (int(*)()) ib_printf;
    user_arg = "%.4d %s\n";
    }

control->ctl_routine = routine;
control->ctl_user_arg = user_arg;
control->ctl_blr = control->ctl_blr_start = blr;
control->ctl_ptr = control->ctl_buffer;
control->ctl_language = language;

version = BLR_BYTE;

if (version != PYXIS__MAP_VERSION1)
    return error (control, offset, "*** dyn version %d is not supported ***\n", (int *)version);

blr_format (control, "PYXIS__MAP_VERSION1,");
PRINT_LINE;
level++;

while ((c = BLR_BYTE) != PYXIS__MAP_END)
    {
    indent (control, level);
    if (c >= PYXIS__MAP_FIELD2 && c <= PYXIS__MAP_SUB_FIELD)
	blr_format (control, "PYXIS__MAP_%s, ", map_strings [c - PYXIS__MAP_FIELD2]);
    switch (c)
	{
	case PYXIS__MAP_MESSAGE:
	    PRINT_BYTE;
	    for (n = PRINT_WORD; n; --n)
		{
		PRINT_LINE;
		indent (control, level + 1);
		print_blr_dtype (control, TRUE);
		}
	    break;

	case PYXIS__MAP_SUB_FORM:
	    PRINT_STRING;
	    break;

	case PYXIS__MAP_SUB_FIELD:
	    PRINT_STRING;

	case PYXIS__MAP_FIELD1:
	case PYXIS__MAP_FIELD2:
	    PRINT_STRING;
	    PRINT_LINE;
	    indent (control, level + 1);
	    PRINT_WORD;
	    if (c != PYXIS__MAP_FIELD1)
		PRINT_WORD;
	    break;

	case PYXIS__MAP_TERMINATOR:
	case PYXIS__MAP_TERMINATING_FIELD:
	case PYXIS__MAP_ITEM_INDEX:
	    PRINT_WORD;
	    break;

	case PYXIS__MAP_OPAQUE:
	case PYXIS__MAP_TRANSPARENT:
	case PYXIS__MAP_TAG:
	    break;

	default:
	    return error (control, offset, "*** invalid form map ***\n", 0);
	}
    PRINT_LINE;
    }

blr_format (control, "PYXIS__MAP_END");
PRINT_LINE;

return 0;
}

PRETTY_print_mblr (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ m b l r
 *
 **************************************
 *
 * Functional description
 *	Pretend we're printing MBLR, but print dynamic
 *	DDL instead.
 *
 **************************************/

PRETTY_print_dyn (blr, routine, user_arg, language);
}

PRETTY_print_menu (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ m e n u
 *
 **************************************
 *
 * Functional description
 *	Pretty print a menu definition.
 *
 **************************************/
struct ctl	ctl, *control;
SCHAR		c;
int		offset;
SSHORT		version, level;

control = &ctl;
offset = level = 0;

if (! routine)
    {
    routine = (int(*)()) ib_printf;
    user_arg = "%.4d %s\n";
    }

control->ctl_routine = routine;
control->ctl_user_arg = user_arg;
control->ctl_blr = control->ctl_blr_start = blr;
control->ctl_ptr = control->ctl_buffer;
control->ctl_language = language;

version = BLR_BYTE;

if (version != PYXIS__MENU_VERSION1)
    return error (control, offset, "*** menu version %d is not supported ***\n", (int *)version);

blr_format (control, "PYXIS__MENU_VERSION1,");
PRINT_LINE;
level++;

while ((c = BLR_BYTE) != PYXIS__MENU_END)
    {
    indent (control, level);
    if (c >= PYXIS__MENU_LABEL && c <= PYXIS__MENU_VERTICAL)
	blr_format (control, "PYXIS__MENU_%s, ", menu_strings [c - PYXIS__MENU_LABEL]);
    switch (c)
	{
	case PYXIS__MENU_ENTREE:
	    PRINT_BYTE;

	case PYXIS__MENU_LABEL:
	    PRINT_STRING;
	    break;

	case PYXIS__MENU_HORIZONTAL:
	case PYXIS__MENU_VERTICAL:
	case PYXIS__MENU_OPAQUE:
	case PYXIS__MENU_TRANSPARENT:
	    break;

	default:
	    return error (control, offset, "*** invalid MENU ***\n", 0);
	}
    PRINT_LINE;
    }

blr_format (control, "PYXIS__MENU_END");
PRINT_LINE;

return 0;
}

PRETTY_print_sdl (
    SCHAR	*blr,
    int		(*routine)(),
    SCHAR	*user_arg,
    SSHORT	language)
{
/**************************************
 *
 *	P R E T T Y _ p r i n t _ s d l
 *
 **************************************
 *
 * Functional description
 *	Pretty print slice description language.
 *
 **************************************/
struct ctl	ctl, *control;
int		offset;
SSHORT		version, level;

control = &ctl;
offset = level = 0;

if (! routine)
    {
    routine = (int(*)()) ib_printf;
    user_arg = "%.4d %s\n";
    }

control->ctl_routine = routine;
control->ctl_user_arg = user_arg;
control->ctl_blr = control->ctl_blr_start = blr;
control->ctl_ptr = control->ctl_buffer;
control->ctl_language = language;

version = BLR_BYTE;

if (version != gds__sdl_version1)
    return error (control, offset, "*** sdl version %d is not supported ***\n", (int *)version);

blr_format (control, "gds__sdl_version1, ");
PRINT_LINE;
level++;

while (NEXT_BYTE != (SCHAR) gds__sdl_eoc)
    PRINT_SDL_VERB;

offset = control->ctl_blr - control->ctl_blr_start;
blr_format (control, "gds__sdl_eoc");
PRINT_LINE;

return 0;
}

static blr_format (
    CTL		control,
    TEXT	*string,
    ...)
{
/**************************************
 *
 *	b l r _ f o r m a t
 *
 **************************************
 *
 * Functional description
 *	Format an utterance.
 *
 **************************************/
va_list	ptr;

VA_START (ptr, string);
vsprintf (control->ctl_ptr, string, ptr);
while (*control->ctl_ptr)
   control->ctl_ptr++;
}

static error (
    CTL		control,
    int		offset,
    TEXT	*string,
    int		*arg)
{
/**************************************
 *
 *	e r r o r
 *
 **************************************
 *
 * Functional description
 *	Put out an error msg and punt.
 *
 **************************************/

PRINT_LINE;
sprintf (control->ctl_ptr, string, arg);
ib_fprintf (ib_stderr, control->ctl_ptr);
ADVANCE_PTR (control->ctl_ptr);
PRINT_LINE;

return -1;
}

static indent (
    CTL		control,
    SSHORT	level)
{
/**************************************
 *
 *	i n d e n t
 *
 **************************************
 *
 * Functional description
 *	Indent for pretty printing.
 *
 **************************************/

level *= 3;
while (--level >= 0)
    PUT_BYTE (' ');
}

static print_blr_dtype (
    CTL		control,
    BOOLEAN	print_object)
{
/**************************************
 *
 *	p r i n t _ b l r _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Print a datatype sequence and return the length of the
 *	data described.
 *
 **************************************/
unsigned short	dtype;
SCHAR		*string;
SSHORT		length, offset;

dtype = BLR_BYTE;

/* Special case blob (261) to keep down the size of the
   jump table */

switch (dtype)
    {
    case blr_short:
	string = "short";
	length = 2;
	break;

    case blr_long:
	string = "long";
	length = 4;
	break;

    case blr_quad:
	string = "quad";
	length = 8;
	break;
    
/** Begin date/time/timestamp **/
    case blr_sql_date:
	string = "sql_date";
	length = sizeof(ISC_DATE);
	break;

    case blr_sql_time:
	string = "sql_time";
	length = sizeof(ISC_TIME);
	break;

    case blr_timestamp:
	string = "timestamp";
	length = sizeof(ISC_TIMESTAMP);
	break;
/** End date/time/timestamp **/

    case blr_int64:
	string = "int64";
	length = sizeof(ISC_INT64);
	break;
    
    case blr_float:
	string = "float";
	length = 4;
	break;
    
    case blr_double:
	string = "double";
	length = 8;
	break;
    
    case blr_d_float:
	string = "d_float";
	length = 8;
	break;
    
    case blr_text:
	string = "text";
	break;

    case blr_cstring:
	string = "cstring";
	break;

    case blr_varying:
	string = "varying";
	break;

    case blr_text2:
	string = "text2";
	break;

    case blr_cstring2:
	string = "cstring2";
	break;

    case blr_varying2:
	string = "varying2";
	break;

    case blr_blob_id:
	string = "blob_id";
	length = 8;
	break;

    default:
	error (control, 0, "*** invalid data type ***",0);
    }

blr_format (control, "blr_%s, ", string);

if (!print_object)
    return length;

switch (dtype)
    {
    case blr_text:
	length = PRINT_WORD;
	break;

    case blr_varying:
	length = PRINT_WORD + 2;
	break;

    case blr_text2:
	PRINT_WORD;
	length = PRINT_WORD;
	break;

    case blr_varying2:
	PRINT_WORD;
	length = PRINT_WORD + 2;
	break;

    case blr_short:
    case blr_long:
    case blr_quad:
	PRINT_BYTE;
	break;

    case blr_blob_id:
	PRINT_WORD;
	break;

    default:
	if (dtype == blr_cstring)
	    length = PRINT_WORD;
	if (dtype == blr_cstring2)
	    {
	    PRINT_WORD;
	    length = PRINT_WORD;
	    }
	break;
    }

return length;
}


static print_blr_line (
    CTL		control,
    USHORT	offset,
    UCHAR	*line)
{
/**************************************
 *
 *	p r i n t _ b l r _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Print a line of pretty-printed BLR.
 *
 **************************************/
USHORT	comma;
UCHAR	c;

indent (control, control->ctl_level);
comma = FALSE;

while (c = *line++)
    {
    PUT_BYTE (c);
    if (c == ',')
	comma = TRUE;
    else if (c != ' ')
	comma = FALSE;
    }

if (!comma)
    PUT_BYTE (',');

PRINT_LINE;
}

static print_byte (
    CTL	control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ b y t e
 *
 **************************************
 *
 * Functional description
 *	Print a byte as a numeric value and return same.
 *
 **************************************/
UCHAR	v;

v = BLR_BYTE;
sprintf (control->ctl_ptr, (control->ctl_language) ? "chr(%d), " : "%d, ", v);
ADVANCE_PTR (control->ctl_ptr);

return v;
}

static print_char (
    CTL	control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ c h a r
 *
 **************************************
 *
 * Functional description
 *	Print a byte as a numeric value and return same.
 *
 **************************************/
UCHAR	c;
SSHORT	printable;

c = BLR_BYTE;
printable = (c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9' ||
	     c == '$' || c == '_');

sprintf (control->ctl_ptr, (printable) ? 
    "'%c'," : 
    (control->ctl_language) ? "chr(%d)," : "%d,", c);
ADVANCE_PTR (control->ctl_ptr);

CHECK_BUFFER;

return c;
}

static print_dyn_verb (
    CTL		control,
    SSHORT	level)
{
/**************************************
 *
 *	p r i n t _ d y n _ v e r b
 *
 **************************************
 *
 * Functional description
 *	Primary recursive routine to print dynamic DDL.
 *
 **************************************/
int	offset, length, size;
UCHAR	operator;
TEXT	*p;

offset = control->ctl_blr - control->ctl_blr_start;
operator = BLR_BYTE;

size = sizeof (dyn_table) / sizeof (dyn_table [0]);
if (operator > size ||
    operator <= 0 ||
    !(p = dyn_table [operator]))
    return error (control, offset, "*** dyn operator %d is undefined ***\n", (int *)operator);

indent (control, level);
blr_format (control, p);
PUT_BYTE (',');
PUT_BYTE (' ');
++level;

switch (operator)
    {
    case gds__dyn_begin:
    case gds__dyn_mod_database:
	PRINT_LINE;
	while (NEXT_BYTE != gds__dyn_end)
	    PRINT_DYN_VERB;
	PRINT_DYN_VERB;
	return 0;

    case gds__dyn_view_blr:
    case gds__dyn_fld_validation_blr:
    case gds__dyn_fld_computed_blr:
    case gds__dyn_trg_blr:
    case gds__dyn_fld_missing_value:
    case gds__dyn_prc_blr:
    case gds__dyn_fld_default_value:
	length = PRINT_WORD;
	PRINT_LINE;
	if (length)
	    {
	    control->ctl_level = level;
	    gds__print_blr (control->ctl_blr, (FPTR_VOID) print_blr_line, control, control->ctl_language);
	    control->ctl_blr += length;
	    }
	return 0;

    case gds__dyn_scl_acl:
    case gds__dyn_log_check_point_length:
    case gds__dyn_log_num_of_buffers:
    case gds__dyn_log_buffer_size:
    case gds__dyn_log_group_commit_wait:
    case gds__dyn_idx_inactive:
	length = PRINT_WORD;
	while (length--)
	    PRINT_BYTE;
	PRINT_LINE;
	return 0;

    case gds__dyn_view_source:
    case gds__dyn_fld_validation_source:
    case gds__dyn_fld_computed_source:
    case gds__dyn_description:
    case gds__dyn_prc_source:
    case gds__dyn_fld_default_source:
	length = PRINT_WORD;
	while (length--)
	    PRINT_CHAR;
	PRINT_LINE;
	return 0;

#if (defined JPN_SJIS || defined JPN_EUC) 

    case gds__dyn_view_source2:
    case gds__dyn_fld_validation_source2:
    case gds__dyn_fld_computed_source2:
    case gds__dyn_description2:
    case gds__dyn_prc_source2:
	PRINT_WORD;
	length = PRINT_WORD;
	while (length--)
	    PRINT_CHAR;
	PRINT_LINE;
	return 0;

    case gds__dyn_fld_edit_string2:
    case gds__dyn_fld_query_header2:
    case gds__dyn_trg_msg2:
    case gds__dyn_trg_source2:
	PRINT_WORD;
	if (length = PRINT_WORD)
  	    do PRINT_CHAR; while (--length);
	PRINT_LINE;
	return 0;

#endif

    case gds__dyn_del_exception:
	if (length = PRINT_WORD)
	    do PRINT_CHAR; while (--length);
	return 0;

    case gds__dyn_fld_not_null:
    case gds__dyn_sql_object:
    case gds__dyn_drop_log:
    case gds__dyn_drop_cache:
    case gds__dyn_def_default_log:
    case gds__dyn_log_file_serial:
    case gds__dyn_log_file_raw:
    case gds__dyn_log_file_overflow:
    case gds__dyn_single_validation:
    case gds__dyn_del_default:
    case gds__dyn_del_validation:
    case gds__dyn_idx_statistic:
    case gds__dyn_foreign_key_delete:
    case gds__dyn_foreign_key_update:
    case gds__dyn_foreign_key_cascade:
    case gds__dyn_foreign_key_default:
    case gds__dyn_foreign_key_null:
    case gds__dyn_foreign_key_none:
   
	PRINT_LINE;
	return 0;

    case gds__dyn_end:
	PRINT_LINE;
	return 0;
    }

if (length = PRINT_WORD)
    do PRINT_CHAR; while (--length);

PRINT_LINE;

switch (operator)
    {
    case gds__dyn_def_database:
    case gds__dyn_def_dimension:
    case gds__dyn_def_exception:
    case gds__dyn_def_file:
    case gds__dyn_def_log_file:
    case gds__dyn_def_cache_file:
    case gds__dyn_def_filter:
    case gds__dyn_def_function:
    case gds__dyn_def_function_arg:
    case gds__dyn_def_generator:
    case gds__dyn_def_global_fld:
    case gds__dyn_def_idx:
    case gds__dyn_def_local_fld:
    case gds__dyn_def_rel:
    case gds__dyn_def_procedure:
    case gds__dyn_def_parameter:
    case gds__dyn_def_security_class:
    case gds__dyn_def_shadow:
    case gds__dyn_def_sql_fld:
    case gds__dyn_def_trigger:
    case gds__dyn_def_trigger_msg:
    case gds__dyn_def_view:
    case gds__dyn_delete_database:
    case gds__dyn_delete_dimensions:
    case gds__dyn_delete_filter:
    case gds__dyn_delete_function:
    case gds__dyn_delete_global_fld:
    case gds__dyn_delete_idx:
    case gds__dyn_delete_local_fld:
    case gds__dyn_delete_rel:
    case gds__dyn_delete_procedure:
    case gds__dyn_delete_parameter:
    case gds__dyn_delete_security_class:
    case gds__dyn_delete_trigger:
    case gds__dyn_delete_trigger_msg:
    case gds__dyn_delete_shadow:
    case gds__dyn_mod_exception:
    case gds__dyn_mod_global_fld:
    case gds__dyn_mod_idx:
    case gds__dyn_mod_local_fld:
    case gds__dyn_mod_procedure:
    case gds__dyn_mod_rel:
    case gds__dyn_mod_security_class:
    case gds__dyn_mod_trigger:
    case gds__dyn_mod_trigger_msg:
    case gds__dyn_rel_constraint:
    case gds__dyn_mod_view:
    case gds__dyn_grant:
    case gds__dyn_revoke:
    case gds__dyn_view_relation: 
    	while (NEXT_BYTE != gds__dyn_end)
	    PRINT_DYN_VERB;
	PRINT_DYN_VERB;
	return 0;
    }

return 0;
}

static print_line (
    CTL		control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Invoke callback routine to print (or do something with) a line.
 *
 **************************************/

*control->ctl_ptr = 0;
(*control->ctl_routine) (control->ctl_user_arg, offset, control->ctl_buffer);
control->ctl_ptr = control->ctl_buffer;
}

static SLONG print_long (
    CTL		control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ l o n g
 *
 **************************************
 *
 * Functional description
 *	Print a VAX word as a numeric value an return same.
 *
 **************************************/
UCHAR	v1, v2, v3, v4;

v1 = BLR_BYTE;
v2 = BLR_BYTE;
v3 = BLR_BYTE;
v4 = BLR_BYTE;
sprintf (control->ctl_ptr,
   (control->ctl_language) ? "chr(%d),chr(%d),chr(%d),chr(%d) " : "%d,%d,%d,%d, ", v1, v2, v3, v4);
ADVANCE_PTR (control->ctl_ptr);

return v1 | (v2 << 8) | (v3 << 16) | (v4 << 24);
}

static print_sdl_verb (
    CTL		control,
    SSHORT	level)
{
/**************************************
 *
 *	p r i n t _ s d l _ v e r b
 *
 **************************************
 *
 * Functional description
 *	Primary recursive routine to print slice description language.
 *
 **************************************/
int	offset, n;
TEXT	*p;
SCHAR	operator;

offset = control->ctl_blr - control->ctl_blr_start;
operator = BLR_BYTE;

if (operator > (sizeof (sdl_table) / sizeof (sdl_table [0])) ||
    operator <= 0 ||
    ! (p = sdl_table [operator]))
    return error (control, offset, "*** SDL operator %d is undefined ***\n", (int *)operator);

indent (control, level);
blr_format (control, p);
PUT_BYTE (',');
PUT_BYTE (' ');
++level;
n = 0;

switch (operator)
    {
    case gds__sdl_begin:
	PRINT_LINE;
	while (NEXT_BYTE != gds__sdl_end)
	    PRINT_SDL_VERB;
	PRINT_SDL_VERB;
	return 0;
    
    case gds__sdl_struct:
	n = PRINT_BYTE;
	while (n--)
	    {
	    PRINT_LINE;
	    indent (control, level + 1);
	    offset = control->ctl_blr - control->ctl_blr_start;
	    print_blr_dtype (control, TRUE);
	    }
	break;
    
    case gds__sdl_scalar:
	PRINT_BYTE;

    case gds__sdl_element:
	n = PRINT_BYTE;
	PRINT_LINE;
	while (n--)
	    PRINT_SDL_VERB;
	return 0;
    
    case gds__sdl_field:
    case gds__sdl_relation:
	PRINT_STRING;
	break;
    
    case gds__sdl_fid:
    case gds__sdl_rid:
    case gds__sdl_short_integer:
	PRINT_WORD;
	break;

    case gds__sdl_variable:
    case gds__sdl_tiny_integer:
	PRINT_BYTE;
	break;
    
    case gds__sdl_long_integer:
	PRINT_LONG;
	break;

    case gds__sdl_add:
    case gds__sdl_subtract:
    case gds__sdl_multiply:
    case gds__sdl_divide:
	PRINT_LINE;
	PRINT_SDL_VERB;
	PRINT_SDL_VERB;
	return 0;

    case gds__sdl_negate:
	PRINT_LINE;
	PRINT_SDL_VERB;
	return 0;

    case gds__sdl_do3:
	n++;
    case gds__sdl_do2:
	n++;
    case gds__sdl_do1:
	n += 2;
	PRINT_BYTE;
	PRINT_LINE;
	while (n--)
	    PRINT_SDL_VERB;
	return 0;
    }

PRINT_LINE;

return 0;
}

static print_string (
    CTL	control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Print a byte-counted string.
 *
 **************************************/
SSHORT	n;

n = PRINT_BYTE;
while (--n >= 0)
    PRINT_CHAR;

PUT_BYTE (' ');
}

static print_word (
    CTL	control,
    SSHORT	offset)
{
/**************************************
 *
 *	p r i n t _ w o r d
 *
 **************************************
 *
 * Functional description
 *	Print a VAX word as a numeric value an return same.
 *
 **************************************/
UCHAR	v1, v2;

v1 = BLR_BYTE;
v2 = BLR_BYTE;
sprintf (control->ctl_ptr,
   (control->ctl_language) ? "chr(%d),chr(%d), " : "%d,%d, ", v1, v2);
ADVANCE_PTR (control->ctl_ptr);

return (v2 << 8) | v1;
}
