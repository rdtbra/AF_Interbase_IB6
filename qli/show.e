/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		show.e
 *	DESCRIPTION:	Show environment commands
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
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../qli/parse.h"
#include "../qli/compile.h"
#include "../jrd/license.h"
#include "../qli/reqs.h"
#include "../jrd/flags.h"
#ifdef APOLLO
#include "../jrd/termtype.h"
#include "/sys/ins/base.ins.c"
#include "/sys/ins/pad.ins.c"
#include "/sys/ins/error.ins.c"
#endif
#include "../qli/all_proto.h"
#include "../qli/comma_proto.h"
#include "../qli/err_proto.h"
#include "../qli/meta_proto.h"
#include "../qli/proc_proto.h"
#include "../qli/show_proto.h"
#include "../jrd/gds_proto.h"

extern USHORT	QLI_columns, QLI_name_columns;

static void	array_dimensions (DBB, TEXT *);
static void	display_acl (DBB, SLONG [2]);
static void	display_blob (DBB, SLONG [2], TEXT *, USHORT, UCHAR *, int);
static void	display_blr (DBB, SLONG [2]);
static void	display_text (DBB, SLONG [2], TEXT *, int);
static void	display_procedure (DBB, UCHAR *, int *);
static USHORT	get_window_size (int);
static void	show_blob_info (SLONG *, SLONG *, USHORT, USHORT, DBB, TEXT *);
static void	show_blr (DBB, USHORT, SLONG *, USHORT, SLONG *);
static void	show_datatype (DBB, USHORT, USHORT, SSHORT, SSHORT, USHORT, USHORT);
static void	show_dbb (DBB);
static void	show_dbb_parameters (DBB);
static int	show_field_detail (DBB, TEXT *, TEXT *, SYN);
static int	show_field_instances (DBB, TEXT *);
static void	show_fields (REL, FLD);
static void	show_filt (QFL);
static int	show_filter_detail (DBB, TEXT *);
static void	show_filts (DBB);
static int	show_filters_detail (DBB);
static void	show_fld (SYN);
static void	show_func (QFN);
static int	show_func_detail (DBB, TEXT *);
static void	show_funcs (DBB);
static int	show_funcs_detail (DBB);
static int	show_insecure_fields (DBB, TEXT *, TEXT *);
static void	show_forms_db (DBB);
static int	show_forms_detail (DBB);
static void	show_gbl_field (SYN);
static int	show_gbl_field_detail (DBB, TEXT *);
static void	show_gbl_fields (DBB);
static int	show_gbl_fields_detail (DBB);
static int	show_indices_detail (REL);
static void	show_matching (void);
static void	show_names (TEXT *, USHORT, TEXT *);
static void	show_proc (QPR);
static void	show_procs (DBB);
static void	show_rel (REL);
static void	show_rel_detail (REL);
static void	show_rels (DBB, ENUM show_t, SSHORT);
static int	show_security_class_detail (DBB, TEXT *);
static int	show_security_classes_detail (DBB);
static void	show_string (USHORT, TEXT *, USHORT, TEXT *);
static void	show_sys_trigs (DBB);
static void	show_text_blob (DBB, TEXT *, USHORT, SLONG *, USHORT, SLONG *, int);
static void	show_trig (REL);
static int	show_trigger_detail (DBB, TEXT *);
static void	show_trigger_header (TEXT *, USHORT, USHORT, USHORT, SLONG *, DBB, TEXT *);
static void	show_trigger_messages (DBB, TEXT *);
static void	show_trigger_status (TEXT *, USHORT, USHORT, USHORT);
static void	show_trigs (DBB);
static int	show_triggers_detail (DBB);
static void	show_var (NAM);
static void	show_vars (void);
static void	show_versions (void);
static void	show_view (REL);
static int	show_views_detail (DBB);
static int	truncate_string (TEXT *);
static void	view_info (DBB, TEXT *, TEXT *, SSHORT, SSHORT);

static CONST SCHAR	db_items [] =
		{ gds__info_page_size, isc_info_db_size_in_pages, gds__info_end }; 
static CONST UCHAR	
#if (defined JPN_EUC || defined JPN_SJIS)
	text_bpb [] =
		{ gds__bpb_version1, gds__bpb_target_interp, 2,
		  (HOST_INTERP & 0xff), (HOST_INTERP >> 8) },
#endif /* (defined JPN_EUC || defined JPN_SJIS) */
	acl_bpb [] =
		{ gds__bpb_version1, gds__bpb_source_type, 1, 3,
		  gds__bpb_target_type, 1, 1 },
	blr_bpb [] =
		{ gds__bpb_version1, gds__bpb_source_type, 1, 2,
		  gds__bpb_target_type, 1, 1 };

#define NULL_BLOB(id)	(!id [0] && !id [1])

DATABASE
    DB = EXTERN FILENAME "yachts.lnk";

/*
    values for rdb$trigger_type, based from types.h
    ** Some better way should be found to pick this up dynamically (?) **
    until then, this must be kept in sync with jrd/types.h manually.
*/

#define PRE_STORE   1
#define POST_STORE  2
#define PRE_MODIFY  3
#define POST_MODIFY 4
#define PRE_ERASE   5
#define POST_ERASE  6

void SHOW_stuff (
    SYN		node)
{
/**************************************
 *
 *	S H O W _ s t u f f
 *
 **************************************
 *
 * Functional description
 *	Show various stuffs.
 *
 **************************************/
SYN		*ptr, value;
REL		relation;
DBB		dbb;
ENUM show_t	sw;
USHORT		i, count;
NAM		name;
TEXT		buffer[256];
SSHORT		width;

QLI_skip_line = TRUE;
ptr = node->syn_arg;

for (i = 0; i < node->syn_count; i++)
    {
    sw = (ENUM show_t) *ptr++;
    value = *ptr++;
    if (sw != show_matching_language && 
	sw != show_version && 
	sw != show_variable &&
	sw != show_variables &&
	CMD_check_ready ())
    	return;
    switch (sw)
	{
	case show_all:
	    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		{
		show_dbb (dbb);
		for (relation = dbb->dbb_relations; relation; 
		     relation = relation->rel_next)
		    {
		    if (!(relation->rel_flags & REL_system))
			{
			show_rel (relation);
			show_fields (relation, NULL_PTR);
			}
		    if (QLI_abort)
			return;
		    }
		ib_printf ("\n");
		}
	    if (QLI_variables)
		show_vars ();
	    break;

	case show_databases:
	    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		{
		show_dbb (dbb);
		show_dbb_parameters (dbb);
		}
	    break;

	case show_database:
            if (!(dbb = (DBB) value))
		dbb = QLI_databases;
	    show_dbb (dbb);
	    show_dbb_parameters (dbb);
	    for (relation = dbb->dbb_relations; relation; 
		 relation = relation->rel_next)
		     if (!(relation->rel_flags & REL_system))
			{
			show_rel (relation);
			show_fields (relation, NULL_PTR);
			}
	    ib_printf ("\n");
	    break;

	case show_relations:
	case show_system_relations:
            width = get_window_size (sizeof (buffer) - 1);
	    if (dbb = (DBB) value)
		show_rels (dbb, sw, width);
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    show_rels (dbb, sw, width);
	    break;
	
	case show_db_fields:
	    dbb = (DBB) value;
	    for (relation = dbb->dbb_relations; relation; 
		     relation = relation->rel_next)
		{
		if (!(relation->rel_flags & REL_system))
		    {
		    show_rel (relation);
		    show_fields (relation, NULL_PTR);
		    }
		if (QLI_abort)
		    return;
		}
	    break;

	case show_security_classes:
	    count = 0;
	    if (value)
		count += show_security_classes_detail (value);
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    count += show_security_classes_detail (dbb);
	    if (!count)
		ERRQ_msg_put (90, NULL, NULL, NULL, NULL, NULL); /* Msg90 No security classes defined */
	    break;
	    
	case show_security_class:
	    count = 0;
	    name = (NAM) value;
	    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		count += show_security_class_detail (dbb, name->nam_string);
	    if (!count)
		ERRQ_msg_put (91, name->nam_string, NULL, NULL, NULL, NULL); /* Msg91 Security class %s is not defined */
	    break;
	    
	case show_views:
	    count = 0;
	    if (value)
		count += show_views_detail (value);
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    count += show_views_detail (dbb);
	    if (!count)
		ERRQ_msg_put (92, NULL, NULL, NULL, NULL, NULL); /* Msg92 No views defined */
	    break;
	    
	case show_relation:
	    show_rel (value);
	    show_fields (value, NULL_PTR);
	    show_view (value);
	    show_rel_detail (value);
	    break;
	
	case show_indices:
	    if (value)
		{
		show_rel (value);
		if (!show_indices_detail (value))
		    ERRQ_msg_put (93, NULL, NULL, NULL, NULL, NULL); /* Msg93 No indices defined */
		break;
		}
	    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		{
		show_dbb (dbb);
		for (relation = dbb->dbb_relations; relation; 
		     relation = relation->rel_next)
		    if (!(relation->rel_flags & (REL_system | REL_view)))
			{
			show_rel (relation);
			if (!show_indices_detail (relation))
			    ERRQ_msg_put (94, NULL, NULL, NULL, NULL, NULL); /* Msg94 No indices defined */
			}
		}
	    break;

	case show_db_indices:
	    dbb = (DBB) value;
	    show_dbb (dbb);
	    for (relation = dbb->dbb_relations; relation; 
		relation = relation->rel_next)
		if (!(relation->rel_flags & (REL_system | REL_view)))
		    {
		    show_rel (relation);
		    if (!show_indices_detail (relation))
			ERRQ_msg_put (94, NULL, NULL, NULL, NULL, NULL); /* Msg94 No indices defined */
		    }
	    break;

	case show_field:
	    show_fld (value);
	    break;

	case show_filter:
	    show_filt (value);
	    break;

	case show_filters:
	    if (value)
		show_filts (value); 
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    show_filts (dbb);
	    break;

	case show_forms:
	    if (value)
		show_forms_db (value); 
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    show_forms_db (dbb);
	    break;

	case show_function:
	    show_func (value);
	    break;

	case show_functions:
	    if (value)
	        show_funcs (value);
	    else
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    show_funcs (dbb);
	    break;

	case show_matching_language:
	    show_matching ();
	    break;

	case show_procedures:
	    if (value)
		show_procs (value);
	    else 
		for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		    show_procs (dbb);
	    break;
	
	case show_procedure:
	    show_proc (value);
	    break;

	case show_version:
	    show_versions();
	    break;

	case show_variable:
	    show_var (value);
	    break;

	case show_variables:
	    show_vars();
	    break;

	case show_global_field:
	    show_gbl_field (value);
	    break;

	case show_global_fields:
	    show_gbl_fields (value);
	    break;

        case show_trigger:
	    show_trig (value);
	    break;

	case show_triggers:
	    show_trigs (value);
	    break;

	case show_system_triggers:
	    if (value)
		show_sys_trigs (value);
	    else
	    for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
		show_sys_trigs (dbb);
	    break;

	default:
	    BUGCHECK (7); /* Msg7 show option not implemented */
	}
    }
}

static void array_dimensions (
    DBB    	database,
    TEXT    	*field_name)
{
/**************************************
 *
 *	a r r a y _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Display the dimensions of an array
 *
 **************************************/
TEXT	s [128], *p;
             
MET_meta_transaction (database, FALSE);

s [0] = 0;

p = s;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_dimensions])
    D IN RDB$FIELD_DIMENSIONS WITH D.RDB$FIELD_NAME = field_name
    SORTED BY D.RDB$DIMENSION

	if (D.RDB$LOWER_BOUND != 1)
	    {
	    sprintf (p, "%d:", D.RDB$LOWER_BOUND); 
	    while (*++p)
		;
	    }
	sprintf (p, "%d, ", D.RDB$UPPER_BOUND);
	while (*++p)
	    ;
END_FOR;

--p;
*--p = 0;
	
ERRQ_msg_partial (479, s, NULL, NULL, NULL, NULL); /* Msg479 (%s) */
}

static void display_acl (
    DBB    	dbb,
    SLONG	blob_id [2])
{
/**************************************
 *
 *	d i s p l a y _ a c l
 *
 **************************************
 *
 * Functional description
 *	Display the contents of a security class
 *
 **************************************/

display_blob (dbb, blob_id, "\t    ", sizeof (acl_bpb), acl_bpb, TRUE);
}

static void display_blob (
    DBB    	dbb,
    SLONG	blob_id [2],
    TEXT	*prefix,
    USHORT	bpb_length,
    UCHAR	*bpb,
    int         strip_line)
{
/**************************************
 *
 *	d i s p l a y _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Display the contents of a security class
 *
 **************************************/
USHORT	length, buffer_length;
TEXT	buffer [256], *b;
int	*blob;
STATUS	status_vector [20], status;

blob = NULL;

if (gds__open_blob2 (status_vector, 
		GDS_REF (dbb->dbb_handle),
		GDS_REF (dbb->dbb_meta_trans),
		GDS_REF (blob),
		GDS_VAL (blob_id),
		bpb_length,
		GDS_VAL (bpb)))
    ERRQ_database_error (dbb, status_vector);

for (;;)
    {
    buffer_length = sizeof (buffer) - 1;
    status = gds__get_segment (status_vector,
	    GDS_REF (blob),
	    GDS_REF (length),
	    buffer_length,
	    buffer);
    if (status && status != gds__segment)
	break;
    buffer [length--] = 0;
    if (strip_line)
	{
	for (b = buffer + length; b >= buffer; )
            {
                if (*b == '\n' || *b == '\t' || *b == ' ')
                *b-- = 0;
            else
	        break;
	    }
	}
    else
	{
        if (buffer [length] == '\n')
            buffer [length] = 0;
        }
    if (buffer [0])
        ib_printf ("%s%s\n", prefix, buffer);
    }

gds__close_blob (status_vector,
	GDS_REF (blob));
}

static void display_blr (
    DBB    	dbb,
    SLONG	blob_id [2])
{
/**************************************
 *
 *	d i s p l a y _ b l r
 *
 **************************************
 *
 * Functional description
 *	Display the contents of a security class
 *
 **************************************/

display_blob (dbb, blob_id, "\t    ", sizeof (blr_bpb), blr_bpb, TRUE);
}

static void display_text (
    DBB    	dbb,
    SLONG	blob_id [2],
    TEXT	*prefix,
    int         strip)
{
/**************************************
 *
 *	d i s p l a y _ t e x t
 *
 **************************************
 *
 * Functional description
 *	Display a text blob.
 *
 **************************************/

#if (defined JPN_EUC || defined JPN_SJIS)
display_blob (dbb, blob_id, prefix, sizeof(text_bpb), text_bpb, strip);
#else
display_blob (dbb, blob_id, prefix, 0, NULL_PTR, strip);
#endif /* (defined JPN_EUC || defined JPN_SJIS) */
}

static void display_procedure (
    DBB		database,
    UCHAR	*name,
    int		*blob)
{
/**************************************
 *
 *	d i s p l a y _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Show definition of procedure.  Somebody
 *	else has already figured out what database
 *	it's in, and so forth.  
 *
 **************************************/
STATUS	status_vector [20];
SSHORT	length;
UCHAR	buffer [256];

ERRQ_msg_put (96, name, /* Msg96 Procedure %s in database %s (%s) */
	database->dbb_filename, database->dbb_symbol->sym_string, NULL, NULL);

while (PRO_get_line (blob, buffer, sizeof (buffer)))
    ib_printf ("    %s", buffer);

PRO_close (database, blob);
ib_printf ("\n");
}

static USHORT get_window_size (
    int max_width)
{
/**************************************
 *
 *	g e t _ w i n d o w _ s i z e 
 *
 **************************************
 *
 * Functional description
 *	Get the display window size.
 *
 **************************************/
SSHORT result;

#ifdef APOLLO
status_$t		status;
pad_$window_desc_t	window_desc;     
SSHORT			window_num;

if (QLI_name_columns)
    result = QLI_name_columns;
else
    {
    /* Find the current font and character size */
    pad_$inq_windows (stream_$ib_stdout, window_desc, 1, window_num, status);
    if (status.all)
        result = QLI_columns;
    else
        result = window_desc.width;
    }
#else
result = QLI_columns;
#endif                

if (max_width < result)
    result = max_width;
return result;
}
 
static void show_blob_info (
    SLONG	*blob_blr,
    SLONG	*blob_src,
    USHORT	msg_blr,
    USHORT	msg_src,
    DBB		database,
    TEXT	*relation_name)
{
/*****************************************************
 *
 *	s h o w _ b l o b _ i n f o
 *
 *****************************************************
 *
 * Functional description
 *	 Display source if possible or blr.
 *
 *****************************************************/

if (NULL_BLOB (blob_src))
    {
    ERRQ_msg_put (msg_blr, NULL, NULL, NULL, NULL, NULL);
    display_blr (database, blob_blr);
    }
else 
    { 
    ERRQ_msg_put (msg_src, relation_name, NULL, NULL, NULL, NULL);
    show_text_blob (database, "\t", 0, blob_src, 0, NULL_PTR, TRUE);
    }
}


static void show_blr (
    DBB		database,
    USHORT	source_msg,
    SLONG	*source,
    USHORT	blr_msg,
    SLONG	*blr)
{
/**************************************
 *
 *	s h o w _ b l r
 *
 **************************************
 *
 * Functional description
 *	Show either source or blr for a blr item along with
 *	appopriate messages.
 *
 **************************************/

if (source [0] || source [1])
    show_text_blob (database, "\t", source_msg, source, 0, NULL_PTR, FALSE);
else if (blr [0] || blr [1])
    {
    if (blr_msg)
	ERRQ_msg_put (blr_msg, NULL, NULL, NULL, NULL, NULL);
    display_blr (database, blr);
    }
}

static void show_datatype (
    DBB		database,
    USHORT	dtype,
    USHORT	length,
    SSHORT	scale,
    SSHORT	sub_type,
    USHORT	segment_length,
    USHORT	dimensions)
{
/**************************************
 *
 *	s h o w _ d a t a t y p e
 *
 **************************************
 *
 * Functional description
 *	Display datatype information.
 *
 **************************************/
SSHORT	msg;
TEXT	*p, *q, subtype [32];

msg = 0;
switch (dtype)
    {
    case dtype_text:
	msg = 97;
	break;
	
    case dtype_varying:
	msg = 98;
	break;
	
    case dtype_cstring:
	length -= 1;
	msg = 99;
	break;
	
    case dtype_short:
	msg = 100;
	break;

    case dtype_long:
	msg = 101;
	break;

    case dtype_quad:
	msg = 102;
	break;

    case dtype_real:
	msg = 103;
	break;

    case dtype_double:
	msg = 104;
	break;

    case dtype_blob:
	ERRQ_msg_partial (105, NULL, NULL, NULL, NULL, NULL); /* Msg105 blob */
	if (segment_length)
	    ERRQ_msg_partial (106, (TEXT*) (ULONG) segment_length, NULL, NULL, NULL, NULL); /* Msg106 , segment length %d */
	if (sub_type)
	    {
	    sprintf (subtype, "%d", sub_type);
	    if (database && (database->dbb_capabilities & DBB_cap_types))
		FOR (REQUEST_HANDLE database->dbb_requests [REQ_fld_subtype])
		    TYPE IN RDB$TYPES WITH TYPE.RDB$FIELD_NAME EQ "RDB$FIELD_SUB_TYPE" AND
					   TYPE.RDB$TYPE EQ sub_type
		    for (p = subtype, q = TYPE.RDB$TYPE_NAME; *q && *q != ' ';)
			*p++ = *q++;
		    *p = 0;
		END_FOR;
	    ERRQ_msg_partial (107, subtype, NULL, NULL, NULL, NULL);
	    }
	return;

    case dtype_sql_date:
	ERRQ_msg_partial (110, (TEXT*) (ULONG) length, NULL, NULL, NULL, NULL);
	ERRQ_msg_partial (107, "SQL DATE", NULL, NULL, NULL, NULL);
	return;

    case dtype_sql_time:
	ERRQ_msg_partial (110, (TEXT*) (ULONG) length, NULL, NULL, NULL, NULL);
	ERRQ_msg_partial (107, "SQL TIME", NULL, NULL, NULL, NULL);
	return;

    case dtype_timestamp:
	msg = 110;
	break;
	
    default:
	BUGCHECK (8); /* Msg8 show_fields: dtype not done */
    }

ERRQ_msg_partial (msg, (TEXT*) (ULONG) length, NULL, NULL, NULL, NULL);

if (dimensions)
    ERRQ_msg_partial (433, NULL, NULL, NULL, NULL, NULL); /* Msg433  array */

if (dtype == dtype_short || 
    dtype == dtype_long ||
    dtype == dtype_quad)
    if (scale)
	ERRQ_msg_partial (111, (TEXT*) (SLONG) scale, NULL, NULL, NULL, NULL); /* Msg111 , scale %d */

if (dtype == dtype_text || 
    dtype == dtype_varying ||
    dtype == dtype_cstring)
    if (sub_type == 1)
	ERRQ_msg_partial (112, NULL, NULL, NULL, NULL, NULL); /* Msg112 , subtype fixed */
    else if (sub_type != 0)
	{
	sprintf (subtype, "%d", sub_type);
	ERRQ_msg_partial (107, subtype, NULL, NULL, NULL, NULL);
	};
}

static void show_dbb (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ d b b
 *
 **************************************
 *
 * Functional description
 *	Print a database name and some
 *	functional stuff about it.
 *
 **************************************/
SYM	symbol;

if (database)
    {
    if (symbol = database->dbb_symbol)
	ERRQ_msg_put (113, /* Msg113 Database %s readied as %s */
	    database->dbb_filename, symbol->sym_string, NULL, NULL, NULL);
    else
	ERRQ_msg_put (114, database->dbb_filename, NULL, NULL, NULL, NULL); /* Msg114 Database %s */
    }
else
    ERRQ_msg_put (115, NULL, NULL, NULL, NULL, NULL); /* Msg115 No databases are currently ready */
}

static void show_dbb_parameters (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ d b b _ p a r a m e t e r s
 *
 **************************************
 *
 * Functional description
 *	Print stuff about a database.
 *
 **************************************/
UCHAR	*d, buffer[127], item;
int	length;
SLONG	allocation, page_size;
STATUS  status_vector [20];

if (!database)
    return;

if (gds__database_info (status_vector,                         
			GDS_REF (database->dbb_handle),
			sizeof (db_items), 
			db_items,
			sizeof (buffer),
			buffer))
    ERRQ_database_error (database, status_vector);

page_size = allocation = 0;

for (d = buffer; *d != gds__info_end; )
    {
    item = *d++;
    length = gds__vax_integer (d, 2);
    d += 2;
    switch (item)
	{
	case (isc_info_db_size_in_pages):
	    allocation = gds__vax_integer (d, length);
	    break;

	case (gds__info_page_size):
	    page_size = gds__vax_integer (d, length); 
	    break;

	case (gds__info_error):
	    break;
	}
    d += length;
    }

ERRQ_msg_put (116, (TEXT*) (SLONG) page_size, (TEXT*) allocation, NULL, NULL, NULL); /* Msg116 Page size is %d bytes.  Current allocation is %d pages. */

MET_meta_transaction (database, FALSE);

if (database->dbb_capabilities & DBB_cap_security)
    {
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_dbb])
	    D IN RDB$DATABASE
	show_string (260, D.RDB$SECURITY_CLASS, 0, NULL_PTR);
	show_text_blob (database, "\t", 261, &D.RDB$DESCRIPTION, 0, NULL_PTR, FALSE);
    END_FOR; 
    }
else
    {
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_dbb])
	    D IN RDB$DATABASE
	show_text_blob (database, "\t", 262, &D.RDB$DESCRIPTION, 0, NULL_PTR, FALSE);
    END_FOR;
    }
     
if (database->dbb_capabilities & DBB_cap_files)
    {
    if (database->dbb_capabilities & DBB_cap_shadowing)
        { 
        FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_files])
	    F IN DB.RDB$FILES SORTED BY F.RDB$SHADOW_NUMBER,F.RDB$FILE_START
	    if (F.RDB$SHADOW_NUMBER)
            	{
            	if (!(F.RDB$FILE_FLAGS & FILE_conditional))
                        ERRQ_msg_put (385, (TEXT*) (SLONG) F.RDB$SHADOW_NUMBER, F.RDB$FILE_NAME, (TEXT*) F.RDB$FILE_START, NULL, NULL); /* Msg385 Shadow %d, File:%s starting at page %d */
                } 
	    else
	        ERRQ_msg_put (263, F.RDB$FILE_NAME, (TEXT*) F.RDB$FILE_START, NULL, NULL, NULL); /* Msg263 File:%s starting at page %d */
        END_FOR;
        }
    else
        { 
        FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_files])
	        F IN DB.RDB$FILES SORTED BY F.RDB$FILE_START
            ERRQ_msg_put (263, F.RDB$FILE_NAME, (TEXT*) F.RDB$FILE_START, NULL, NULL, NULL); /* Msg263 File:%s starting at page %d */
        END_FOR;
	}
    }
} 

static int show_field_detail (
    DBB		database,
    TEXT	*relation_name,
    TEXT	*field_name,
    SYN		idx_node)
{
/**************************************
 *
 *	s h o w _ f i e l d _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show everything we know about local
 *	fields with the specified name in a particular
 *	database. 
 *
 **************************************/
USHORT	count, dimensions;
TEXT	*source_field, *p;

MET_meta_transaction (database, FALSE);
count = 0;

if (!(database->dbb_capabilities & DBB_cap_security))
    count += show_insecure_fields (database, relation_name, field_name);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_field])
	RFR IN RDB$RELATION_FIELDS CROSS 
	F IN RDB$FIELDS CROSS
	R IN RDB$RELATIONS WITH
	RFR.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME AND
	R.RDB$RELATION_NAME = RFR.RDB$RELATION_NAME AND
	(RFR.RDB$FIELD_NAME = field_name OR 
	RFR.RDB$QUERY_NAME = field_name OR 
	(RFR.RDB$QUERY_NAME MISSING AND F.RDB$QUERY_NAME = field_name))
    truncate_string (RFR.RDB$RELATION_NAME);
 
    if (relation_name && (strcmp (RFR.RDB$RELATION_NAME, relation_name)))
	continue;

    if ((dimensions = MET_dimensions (database, F.RDB$FIELD_NAME)) && 
	(idx_node && (dimensions != (USHORT) idx_node->syn_arg[s_idx_count]->syn_count)))
	    continue;

    if (count++)
	ib_printf ("\n");

    truncate_string (RFR.RDB$FIELD_NAME);
    truncate_string (F.RDB$FIELD_NAME);

    if (!R.RDB$VIEW_BLR.NULL)
        ERRQ_msg_put (495, RFR.RDB$FIELD_NAME, RFR.RDB$RELATION_NAME, 
	    database->dbb_symbol->sym_string, NULL, NULL); /* Msg495 Field %s in view %s of database %s */
    else 
        ERRQ_msg_put (496, RFR.RDB$FIELD_NAME, RFR.RDB$RELATION_NAME, 
	    database->dbb_symbol->sym_string, NULL, NULL); /* Msg495 Field %s in relation %s of database %s */
    ERRQ_msg_put (265, F.RDB$FIELD_NAME, NULL, NULL, NULL, NULL); /* Msg265 Global field %s */

    if (!RFR.RDB$BASE_FIELD.NULL)
	{
	truncate_string (RFR.RDB$BASE_FIELD);
	view_info (database, RFR.RDB$RELATION_NAME, RFR.RDB$BASE_FIELD,
		RFR.RDB$VIEW_CONTEXT, 0);
	}

    show_text_blob (database, "\t", 266, &RFR.RDB$DESCRIPTION, 339, &F.RDB$DESCRIPTION,  FALSE);
    ERRQ_msg_put (267, NULL, NULL, NULL, NULL, NULL); /* Msg267 Datatype information: */
    ib_printf ("\t");
    
    show_datatype (database, MET_get_datatype (F.RDB$FIELD_TYPE), F.RDB$FIELD_LENGTH,
		F.RDB$FIELD_SCALE, F.RDB$FIELD_SUB_TYPE,
		F.RDB$SEGMENT_LENGTH, dimensions); 
    if (dimensions && !idx_node)
	array_dimensions (database, F.RDB$FIELD_NAME);
    ib_printf ("\n");
    show_blr (database, 341, &F.RDB$COMPUTED_SOURCE, 341, &F.RDB$COMPUTED_BLR);
    show_blr (database, 342, &F.RDB$VALIDATION_SOURCE, 342, &F.RDB$VALIDATION_BLR);
    show_string (270, RFR.RDB$SECURITY_CLASS, 0, NULL_PTR);
    show_string (271, RFR.RDB$QUERY_NAME, 271, F.RDB$QUERY_NAME);
    show_string (273, RFR.RDB$EDIT_STRING, 273, F.RDB$EDIT_STRING);
    show_text_blob (database, "\t", 275, &RFR.RDB$QUERY_HEADER, 275, &F.RDB$QUERY_HEADER,  FALSE);
END_FOR;

return count;
} 

static int show_field_instances (
    DBB		database,
    TEXT	*field_name)
{
/**************************************
 *
 *	s h o w _ f i e l d _ i n s t a n c e s
 *
 **************************************
 *
 * Functional description
 *	Print a list of places where a global field
 *	is used.
 *
 **************************************/
int	count;
TEXT	*source_field, *p;

count = NULL;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_field_instance])
    RFR IN RDB$RELATION_FIELDS WITH RFR.RDB$FIELD_SOURCE = field_name
    SORTED BY RFR.RDB$RELATION_NAME
	if (!count++)
	    ERRQ_msg_put (335, field_name, database->dbb_symbol->sym_string, NULL, NULL, NULL); /* Msg335 Global field %s is used database %s as: */
	truncate_string (RFR.RDB$FIELD_NAME);
	truncate_string (RFR.RDB$RELATION_NAME);	
	ERRQ_msg_put (336, RFR.RDB$FIELD_NAME, RFR.RDB$RELATION_NAME, NULL, NULL, NULL); /* Msg336 %s in relation %s */
END_FOR;

return count;
}

static void show_fields (
    REL		relation,
    FLD		fields)
{
/**************************************
 *
 *	s h o w _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Show the fields for a relation.
 *
 **************************************/
DBB	database;
FLD	field;
SYM	symbol;
USHORT	max_name, l;

/* Find the long field name (including query name) */

if (relation)
    {
    if (!(relation->rel_flags & REL_fields))
	MET_fields (relation);
    fields = relation->rel_fields;
    database = relation->rel_database;
    MET_meta_transaction (database, FALSE);
    }
else
    database = NULL;

max_name = 0;

for (field = fields; field; field = field->fld_next)
    {
    l = field->fld_name->sym_length;
    if (symbol = field->fld_query_name)
	l += symbol->sym_length + 3;
    max_name = MAX (max_name, l);
    }

/* Now print the fields */

for (field = fields; field; field = field->fld_next)
    {
    if (QLI_abort)
	return;

    symbol = field->fld_name;
    l = symbol->sym_length;
    ib_printf ("        %s", symbol->sym_string);

    if (symbol = field->fld_query_name)
	{
	l += symbol->sym_length + 3;
	ib_printf (" (%s)", symbol->sym_string);
	}

    for (l = max_name + 12 - l; l; --l)
	ib_putchar (' ');
    show_datatype (database, field->fld_dtype,
        (field->fld_dtype == dtype_varying) ? field->fld_length - 2 : field->fld_length, 
	field->fld_scale, field->fld_sub_type, field->fld_segment_length, (field->fld_flags & FLD_array) ? TRUE : FALSE );
    if (database && field->fld_flags & FLD_array)
	array_dimensions (database, field->fld_name->sym_string);
    if (field->fld_flags & FLD_computed)
	ERRQ_msg_put (119, NULL, NULL, NULL, NULL, NULL); /* Msg119 (computed expression) */
    else
 	ib_printf ("\n");
    }
}

static void show_filt (
    QFL		filter)
{
/**************************************
 *
 *	s h o w _ f i l t
 *
 **************************************
 *
 * Functional description
 *	Show definition of filter.  If a particular database
 *	was specified, look there, otherwise, print all filters
 *	with that name.
 *
 **************************************/
int	count, any_supported;
DBB	database;
NAM	name;

count = 0;
name = filter->qfl_name;

if (database = filter->qfl_database)
    {
    if (!(database->dbb_capabilities & DBB_cap_filters))
	{
	 /* Msg439 Filters are not supported in database %s. */
	ERRQ_msg_put (439, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
	return;
	}
    count = show_filter_detail (database, name->nam_string);
    if (!count) 
	/* Msg440 Filter %s is not defined in database %s. */
	ERRQ_msg_put (440, name->nam_string, database->dbb_symbol->sym_string, NULL, NULL, NULL);
    }
else
    {
    any_supported = 0;
    for (database = QLI_databases; database; database = database->dbb_next)
	if (database->dbb_capabilities & DBB_cap_filters)
	    {
	    any_supported++;
	    count += show_filter_detail (database, name->nam_string);
	    }
    if (!count) 
	{
	if (any_supported)
	    /* Msg441 Filter %s is not defined in any open database */
	    ERRQ_msg_put (441, name->nam_string, NULL, NULL, NULL, NULL);
	else
	    /* Msg442 Filters are not supported in any open database. */
	    ERRQ_msg_put (442, NULL, NULL, NULL, NULL, NULL); 
	}
    }
} 

static show_filter_detail (
    DBB		database,
    TEXT	*filter_name)
{
/**************************************
 *
 *	s h o w _ f i l t e r _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Display the following information about a
 *	filter with the specified name in a particular
 *	database:  Filter name, filter library, input and
 *	output sub-types, and description.
 *
 **************************************/
int 	count;

MET_meta_transaction (database, FALSE);
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_filter_detail])
    F IN RDB$FILTERS WITH F.RDB$FUNCTION_NAME = filter_name

    count++;
    truncate_string (filter_name);

    ERRQ_msg_put (444, /* Msg444 Filter %s in database \"%s\" (%s): */
	    filter_name, database->dbb_filename, database->dbb_symbol->sym_string, NULL, NULL);
    ERRQ_msg_put (445, F.RDB$MODULE_NAME, NULL, NULL, NULL, NULL); /* Msg445 Filter library is %s */
    ERRQ_msg_put (446, (TEXT*) F.RDB$INPUT_SUB_TYPE, NULL, NULL, NULL, NULL); /* Msg446 Input sub-type is %d */
    ERRQ_msg_put (447, (TEXT*) F.RDB$OUTPUT_SUB_TYPE, NULL, NULL, NULL, NULL); /* Msg447 Output sub-type is %d */
    show_text_blob (database, "\t", 448, &F.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);
END_FOR;
return count;
}

static void show_filts (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f i l t s
 *
 * Functional description
 *	Show filter names in a database.
 *
 **************************************/

if (!(database->dbb_capabilities & DBB_cap_filters))
    /* Msg463 Filters are not supported for database %s. */
    ERRQ_msg_put (463, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); 
else if (!(show_filters_detail (database)))
    /* Msg464 no filters defined for database %s. */
    ERRQ_msg_put (464, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); 
}

static int show_filters_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f i l t e r s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show filter names in a database.
 *
 **************************************/
SSHORT	width;
TEXT	buffer[256];
int	count;

count = 0;
width = get_window_size (sizeof (buffer) - 1);
buffer[0] = 0;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_filters]) F IN RDB$FILTERS
    SORTED BY F.RDB$FUNCTION_NAME
    if (!count++)
	ERRQ_msg_put (449, database->dbb_filename, /* Msg449 Filters in database %s (%s): */
	    database->dbb_symbol->sym_string, NULL, NULL, NULL);
    show_names (F.RDB$FUNCTION_NAME, width, buffer);
END_FOR;

if (buffer[0])
    ib_printf ("%s\n", buffer);

return count;
}

static void show_fld (
    SYN		field_node)
{
/**************************************
 *
 *	s h o w _ f l d 
 *
 **************************************
 *
 * Functional description
 *	Show all details about a particular
 *	field.  What we've got coming in
 *	is a list of 1, 2, or 3 name blocks, 
 *	contain the [[database.]relation.]field
 *	specification.  Figure out what we've
 *	got and let somebody else  do all the work.
 *
 **************************************/
DBB	database;
NAM	field_name, relation_name;
SYN	*ptr;
SYM	symbol;
TEXT	*string, s[65];
int	count;
SYN	idx_node;

count = 0;
database = NULL;
field_name = NULL;
relation_name = NULL;
idx_node = NULL;

if (field_node->syn_type == nod_index)
    {
    idx_node = field_node;
    field_node = idx_node->syn_arg [s_idx_field];
    }

for (ptr = field_node->syn_arg + field_node->syn_count; --ptr >= field_node->syn_arg;)
    {
    if (!field_name)
	field_name = (NAM) *ptr;
    else if (!relation_name)
	relation_name = (NAM) *ptr;
    else
	for (symbol = ((NAM) *ptr)->nam_symbol; symbol;
		symbol = symbol->sym_homonym)
	    if (symbol->sym_type = SYM_database)
		{
		database = (DBB) symbol->sym_object;
		break;
		}          
    }

string = (relation_name)? relation_name->nam_string : NULL;

if (string)
    sprintf (s, "%s.%s", string, field_name->nam_string);
else
    sprintf (s, "%s", field_name->nam_string);

if (database)
    {
    count += show_field_detail (database, string, 
	field_name->nam_string, NULL_PTR);
    if (!count)
	ERRQ_msg_put (117, s, database->dbb_symbol->sym_string, NULL, NULL, NULL); /* Msg117 Field %s does not exist in database %s */
    }     
else
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	count += show_field_detail (database, string, 
	    field_name->nam_string, idx_node);
    if (!count)
	ERRQ_msg_put (118, s, NULL, NULL, NULL, NULL); /* Msg118 Field %s does not exist in any open database */
    }
}

static void show_func (
    QFN		func)
{
/**************************************
 *
 *	s h o w _ f u n c 
 *
 **************************************
 *
 * Functional description
 *	Show definition of function.  If a particular database
 *	was specified, look there, otherwise, print all functions
 *	with that name.
 *
 **************************************/
int	count, any_supported;
DBB	database;
NAM	name;

count = 0;
name = func->qfn_name;

if (database = func->qfn_database)
    {
    if (!(database->dbb_capabilities & DBB_cap_functions))
	{
	 /* Msg417 Functions are not supported in database %s. */
	ERRQ_msg_put (417, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
	return;
	}
    count = show_func_detail (database, name->nam_string);
    if (!count) 
	/* Msg422 Function %s is not defined in database %s. */
	ERRQ_msg_put (422, name->nam_string, database->dbb_symbol->sym_string, NULL, NULL, NULL);
    }
else
    {
    any_supported = 0;
    for (database = QLI_databases; database; database = database->dbb_next)
	if (database->dbb_capabilities & DBB_cap_functions)
	    {
	    any_supported++;
	    count += show_func_detail (database, name->nam_string);
	    }
    if (!count) 
	{
	if (any_supported)
	    /* Msg423 Function is %s not defined in any open database */
	    ERRQ_msg_put (423, name->nam_string, NULL, NULL, NULL, NULL);
	else
	    /* Msg420 Functions are not supported in any open database. */
	    ERRQ_msg_put (420, NULL, NULL, NULL, NULL, NULL); 
	}
    }
}

static show_func_detail (
    DBB		database,
    TEXT	*func_name)
{
/**************************************
 *
 *	s h o w _ f u n c _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Display the following information about a
 *	function with the specified name in a particular
 *	database:  Function name, query name, function 
 *	library, input and output argument datatypes, and 
 *	description.
 *
 **************************************/
int 	count;
USHORT	array;

MET_meta_transaction (database, FALSE);
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_func_detail])
    F IN RDB$FUNCTIONS WITH 
    (F.RDB$FUNCTION_NAME = func_name OR F.RDB$QUERY_NAME = func_name)

    count++;
    truncate_string (F.RDB$QUERY_NAME);
    truncate_string (F.RDB$FUNCTION_NAME);
    strcpy (func_name, F.RDB$FUNCTION_NAME);

    if (F.RDB$QUERY_NAME[0])
	ERRQ_msg_put (424, /* Msg424 Function %s (%s) in database \"%s\" (%s): */
	    F.RDB$FUNCTION_NAME, F.RDB$QUERY_NAME, database->dbb_filename, 
	    database->dbb_symbol->sym_string, NULL);
    else
	ERRQ_msg_put (425, /* Msg425 Function %s in database \"%s\" (%s): */
	    F.RDB$FUNCTION_NAME, database->dbb_filename, database->dbb_symbol->sym_string, NULL, NULL);
    
    ERRQ_msg_put (426, F.RDB$MODULE_NAME, NULL, NULL, NULL, NULL); /* Msg426 Function library is %s */

    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_func_args])
	FA IN RDB$FUNCTION_ARGUMENTS WITH 
	FA.RDB$FUNCTION_NAME = func_name SORTED BY FA.RDB$ARGUMENT_POSITION

	if (FA.RDB$ARGUMENT_POSITION == 0)
	    ERRQ_msg_partial (427, NULL, NULL, NULL, NULL, NULL); /* Msg427 Return argument is */
	else
	    ERRQ_msg_partial (428, (TEXT*) (SLONG) FA.RDB$ARGUMENT_POSITION, NULL, NULL, NULL, NULL); /* Msg428 Argument %d is */

        array = (FA.RDB$MECHANISM == 2);
	show_datatype (database, MET_get_datatype (FA.RDB$FIELD_TYPE),
	    FA.RDB$FIELD_LENGTH, FA.RDB$FIELD_SCALE, 
	    FA.RDB$FIELD_SUB_TYPE, 0, array);
	ib_printf ("\n");
    END_FOR;

    show_text_blob (database, "\t", 421, &F.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);

END_FOR;
return count;
}   

static void show_funcs (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f u n c s
 *
 **************************************
 *
 * Functional description
 *	Show function names in all ready databases,
 *	or a specific database.
 *
 **************************************/

if (!(database->dbb_capabilities & DBB_cap_functions))
    ERRQ_msg_put (461, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL);
    /* 461 - functions aren't supported in this database */    
if (!(show_funcs_detail (database)))
    ERRQ_msg_put (462, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); 
    /* 462 - no functions are defined in this database */
}

static int show_funcs_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f u n c s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show function names in a database.
 *
 **************************************/
SSHORT	width;
TEXT	buffer[256];
int	count;

count = NULL;
width = get_window_size (sizeof (buffer) - 1);
buffer[0] = 0;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_functions]) F IN RDB$FUNCTIONS
    SORTED BY F.RDB$FUNCTION_NAME
    if (!count++)
	ERRQ_msg_put (416, database->dbb_filename, /* Msg416 Functions in database %s (%s): */
	    database->dbb_symbol->sym_string, NULL, NULL, NULL);
    show_names (F.RDB$FUNCTION_NAME, width, buffer);
END_FOR;

if (buffer[0])
    ib_printf ("%s\n", buffer);

return count;
}

static int show_insecure_fields (
    DBB		database,
    TEXT	*relation_name,
    TEXT	*field_name)
{
/**************************************
 *
 *	s h o w _ i n s e c u r e _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Show everything we know about local
 *	fields with the specified name in a particular
 *	database, that doesn't have security classes
 *	defined.
 *
 **************************************/
int	count;
TEXT	*source_field, *p;

count = NULL;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_field])
	RFR IN RDB$RELATION_FIELDS CROSS 
	F IN RDB$FIELDS CROSS
	R IN RDB$RELATIONS WITH
	RFR.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME AND
	R.RDB$RELATION_NAME = RFR.RDB$RELATION_NAME AND
	(RFR.RDB$FIELD_NAME = field_name OR 
	RFR.RDB$QUERY_NAME = field_name OR 
	(RFR.RDB$QUERY_NAME MISSING AND F.RDB$QUERY_NAME = field_name))
    truncate_string (RFR.RDB$RELATION_NAME);
    if (relation_name && (strcmp (RFR.RDB$RELATION_NAME, relation_name)))
	continue;
    if (count++)
	ib_printf ("\n");
    truncate_string (RFR.RDB$FIELD_NAME);
    truncate_string (F.RDB$FIELD_NAME);

    if (R.RDB$VIEW_BLR.NULL)
        ERRQ_msg_put (496, RFR.RDB$FIELD_NAME, RFR.RDB$RELATION_NAME, 
	    database->dbb_symbol->sym_string, NULL, NULL); /* Msg495 Field %s in relation %s of database %s */
    else 
        ERRQ_msg_put (495, RFR.RDB$FIELD_NAME, RFR.RDB$RELATION_NAME, 
	    database->dbb_symbol->sym_string, NULL, NULL); /* Msg495 Field %s in view %s of database %s */
    ERRQ_msg_put (338, F.RDB$FIELD_NAME, NULL, NULL, NULL, NULL); /* Msg338 Global field %s */
    
    if (!RFR.RDB$BASE_FIELD.NULL)
	{
	truncate_string (RFR.RDB$BASE_FIELD);
	view_info (database, RFR.RDB$RELATION_NAME, RFR.RDB$BASE_FIELD,
		RFR.RDB$VIEW_CONTEXT, 0);
	}

    show_text_blob (database, "\t", 339, &RFR.RDB$DESCRIPTION, 339, &F.RDB$DESCRIPTION, FALSE);
    ERRQ_msg_partial (340, NULL, NULL, NULL, NULL, NULL); /* Msg340 Datatype information: */
    ib_printf ("\t");
    show_datatype (database, MET_get_datatype (F.RDB$FIELD_TYPE), F.RDB$FIELD_LENGTH,
		F.RDB$FIELD_SCALE, F.RDB$FIELD_SUB_TYPE,
		F.RDB$SEGMENT_LENGTH, MET_dimensions (database, F.RDB$FIELD_NAME));
    ib_printf ("\n");
    show_blr (database, 341, &F.RDB$COMPUTED_SOURCE, 341, &F.RDB$COMPUTED_BLR);
    show_blr (database, 342, &F.RDB$VALIDATION_SOURCE, 342, &F.RDB$VALIDATION_BLR);
    show_string (343, RFR.RDB$QUERY_NAME, 343, F.RDB$QUERY_NAME);
    show_string (346, RFR.RDB$EDIT_STRING, 346, F.RDB$EDIT_STRING);
    show_text_blob (database, "\t", 345, &RFR.RDB$QUERY_HEADER, 345, &F.RDB$QUERY_HEADER, FALSE);
END_FOR;

return count;
}

static void show_forms_db (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f o r m s _ d b 
 *
 **************************************
 *
 * Functional description
 *	Show what we know about forms
 *	for one database.
 *
 **************************************/
int	count;

if (!(show_forms_detail (database))) 
    ERRQ_msg_put (120, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); 
    /* Msg120 There are no forms defined for database %s */
}

static int show_forms_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ f o r m s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show everything we know about forms,
 *	meaning basically show form names
 *
 **************************************/
int	count;

count = NULL;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_forms1])
	Y IN RDB$RELATIONS WITH Y.RDB$RELATION_NAME = "PYXIS$FORMS"
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_forms2])
	    X IN PYXIS$FORMS
        if (!count)
	    ERRQ_msg_put (285, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg285 Forms in database %s */
	ib_printf ("    %s\t%s\n", X.PYXIS$FORM_NAME, X.PYXIS$FORM_TYPE);
        count++;
    END_FOR;
END_FOR;
return count;
}

static void show_gbl_field (
    SYN		field_node)
{
/**************************************
 *
 *	s h o w _ g b l _ f i e l d 
 *
 **************************************
 *
 * Functional description
 *	Show what we know about a global
 *	field.  It may be qualified with a
 *	database handle, in which case, show only
 *	the one global field in that database,
 *	otherwise show them all.
 *
 **************************************/
DBB	database;
SYN	*ptr;
NAM	field_name, name;
SYM	symbol;
int	count;

count = 0;
field_name = NULL;
database = NULL;

for (ptr = field_node->syn_arg + field_node->syn_count -1;
	ptr >= field_node->syn_arg; ptr--)
    {
    if (!field_name)
	field_name = (NAM) *ptr;
    else
	for (name = (NAM) *ptr, symbol = name->nam_symbol; 
		symbol; symbol = symbol->sym_homonym)
	    if (symbol->sym_type = SYM_database)
		{
		database = (DBB) symbol->sym_object;
		break;
		}          
    }

if (database)
    {
    count += show_gbl_field_detail (database, field_name->nam_string);
    if (!count)
	ERRQ_msg_put (122, /* Msg122 Global field %s does not exist in database %s */
		field_name->nam_string,	database->dbb_symbol->sym_string, NULL, NULL, NULL);  
    }     
else
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	count += show_gbl_field_detail (database, field_name->nam_string);
    if (!count)
	ERRQ_msg_put (123, field_name->nam_string, NULL, NULL, NULL, NULL); /* Msg123 Global field %s does not exist in any open database */
    }

}

static int show_gbl_field_detail (
    DBB		database,
    TEXT	*field_name)
{
/**************************************
 *
 *	s h o w _ g b l _ f i e l d _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show everything we know about a global
 *	field with the specified name in a particular
 *	database. 
 *
 **************************************/
int	count;
TEXT	*source_field, *p;

count = NULL;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_global_field])
	F IN RDB$FIELDS 
	WITH F.RDB$FIELD_NAME = field_name
    if (count++)
	ib_printf ("\n");
    truncate_string (F.RDB$FIELD_NAME);

    ERRQ_msg_put (276, F.RDB$FIELD_NAME, database->dbb_symbol->sym_string, NULL, NULL, NULL); /* Msg276 Global field %s in database %s */

    show_text_blob (database, "\t", 277, &F.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);
    ERRQ_msg_put (278, NULL, NULL, NULL, NULL, NULL); /* Msg278 Datatype information: */
    ib_printf ("\t");
    show_datatype (database, MET_get_datatype (F.RDB$FIELD_TYPE), F.RDB$FIELD_LENGTH,
		F.RDB$FIELD_SCALE, F.RDB$FIELD_SUB_TYPE,
		F.RDB$SEGMENT_LENGTH, MET_dimensions (database, F.RDB$FIELD_NAME));
    ib_printf ("\n");
    show_blr (database, 279, &F.RDB$COMPUTED_SOURCE, 341, &F.RDB$COMPUTED_BLR);
    show_blr (database, 280, &F.RDB$VALIDATION_SOURCE, 342, &F.RDB$VALIDATION_BLR);
    show_string (281, F.RDB$QUERY_NAME, 0, NULL_PTR);
    show_string (282, F.RDB$EDIT_STRING, 0, NULL_PTR);
    show_text_blob (database, "\t", 283, &F.RDB$QUERY_HEADER, 0, NULL_PTR,  FALSE);
END_FOR;

if (count && !(show_field_instances (database, field_name)))
    ERRQ_msg_put (284, field_name, database->dbb_symbol->sym_string, NULL, NULL, NULL); /* Msg284 %s is not used in any relations in database %s */

return count;
}

static void show_gbl_fields (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ g b l _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Show what we know about a global
 *	field.  It may be qualified with a
 *	database handle, in which case, show only
 *	the global fields in that database,
 *	otherwise show them all.
 *
 **************************************/
int	count;

count = 0;
if (database)
    {
    count += show_gbl_fields_detail(database); 
    if (!count)
	ERRQ_msg_put (124, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg124 There are no fields defined for database %s */
    }     
else
    {
    for (database = QLI_databases; database; database = database->dbb_next)
	count += show_gbl_fields_detail (database);
    if (!count)
	ERRQ_msg_put (125, NULL, NULL, NULL, NULL, NULL); /* Msg125 There are no fields defined in any open database */
    }
}

static int show_gbl_fields_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ g b l _ f i e l d s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show everything we know about 
 *	global fields in a particular
 *	database. 
 *
 **************************************/
int	count;
TEXT	*source_field, *p;

count = NULL;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_global_fields])
	F IN RDB$FIELDS WITH F.RDB$SYSTEM_FLAG MISSING OR
	F.RDB$SYSTEM_FLAG = 0 SORTED BY F.RDB$FIELD_NAME 

    if (!count++)
	ERRQ_msg_put (286, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg286 Global fields for database %s: */

    if (!F.RDB$COMPUTED_BLR.NULL)
	continue;
    ib_printf ("    %s", F.RDB$FIELD_NAME); 
    show_datatype (database, MET_get_datatype (F.RDB$FIELD_TYPE), F.RDB$FIELD_LENGTH,
		F.RDB$FIELD_SCALE, F.RDB$FIELD_SUB_TYPE,
		F.RDB$SEGMENT_LENGTH, MET_dimensions (database, F.RDB$FIELD_NAME));
    show_text_blob (database, "\t", 262, &F.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);
    ib_printf ("\n");
 END_FOR;

return count;
}

static int show_indices_detail (
    REL		relation)
{
/**************************************
 *
 *	s h o w _ i n d i c e s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show indices for a relation.  If none, return 0.
 *
 **************************************/
USHORT	count, type;
TEXT	*p;
DBB	database;

type = count = 0;
database = relation->rel_database;
MET_meta_transaction (database, FALSE);

if (database->dbb_capabilities & DBB_cap_idx_inactive)
    {
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_indices]) 
	    X IN RDB$INDICES CROSS Z IN RDB$RELATIONS 
	    WITH X.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
		AND Z.RDB$RELATION_NAME EQ X.RDB$RELATION_NAME
		AND Z.RDB$VIEW_BLR MISSING
	    SORTED BY X.RDB$INDEX_NAME

	++count;
 	if (database->dbb_capabilities & DBB_cap_index_type)
	    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_index_type])
		Y IN RDB$INDICES WITH Y.RDB$INDEX_NAME = X.RDB$INDEX_NAME
		type = Y.RDB$INDEX_TYPE;
	    END_FOR;

	for (p = X.RDB$INDEX_NAME; *p && *p != ' '; p++)
	    ;
	*p = 0;

	ERRQ_msg_put (450, X.RDB$INDEX_NAME,
	    (X.RDB$UNIQUE_FLAG) ? " (unique)" : "",
	    (type == 1) ? " (descending)" : "",
	    (X.RDB$INDEX_INACTIVE) ? " (inactive)" : "", NULL); /* Msg450 Index %s%s%s%s */

	FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_index]) 
		Y IN RDB$INDEX_SEGMENTS WITH 
		Y.RDB$INDEX_NAME EQ X.RDB$INDEX_NAME
		SORTED BY Y.RDB$FIELD_POSITION
	    for (p = Y.RDB$FIELD_NAME; *p && *p != ' '; p++)
		;
	    ib_printf ("            %s\n", Y.RDB$FIELD_NAME);
	END_FOR;                

#ifdef PC_ENGINE    
/***	if (!X.RDB$EXPRESSION_SOURCE.NULL)
	    show_text_blob (database, "\t    ", 0, &X.RDB$EXPRESSION_SOURCE, 0, NULL_PTR, TRUE);
	else if (!X.RDB$EXPRESSION_BLR.NULL)
	    {
	    ERRQ_msg_put (485, NULL, NULL, NULL, NULL, NULL);
	    display_blr (database, &X.RDB$EXPRESSION_BLR);
            } ***/
#endif
    END_FOR;
    }
else
    {
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_indices]) 
	    X IN RDB$INDICES CROSS Z IN RDB$RELATIONS 
	    WITH X.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
		AND Z.RDB$RELATION_NAME EQ X.RDB$RELATION_NAME
		AND Z.RDB$VIEW_BLR MISSING
	    SORTED BY X.RDB$INDEX_NAME
	++count;
	for (p = X.RDB$INDEX_NAME; *p && *p != ' '; p++)
	    ;
*p = 0;
	ERRQ_msg_put (290, X.RDB$INDEX_NAME,
	    (X.RDB$UNIQUE_FLAG) ? " (unique)" : "", NULL, NULL, NULL); /* Msg290 Index %s%s */
	FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_index]) 
		Y IN RDB$INDEX_SEGMENTS WITH 
		Y.RDB$INDEX_NAME EQ X.RDB$INDEX_NAME
		SORTED BY Y.RDB$FIELD_POSITION
	    for (p = Y.RDB$FIELD_NAME; *p && *p != ' '; p++)
		;
	    ib_printf ("            %s\n", Y.RDB$FIELD_NAME);
	END_FOR;                
    END_FOR;
    }

return count;
}

static void show_matching (void)
{
/**************************************
 *
 *	s h o w _ m a t c h i n g
 *
 **************************************
 *
 * Functional description
 *	Just print the QLI matching language string.
 *
 **************************************/
TEXT	buffer[256];

if (QLI_abort)
    return;
if (QLI_matching_language)
    {
    strncpy (buffer, QLI_matching_language->con_data, QLI_matching_language->con_desc.dsc_length);
    buffer [QLI_matching_language->con_desc.dsc_length] = 0;
    ib_printf ("\n\t%s\n", buffer);
    }
}

static void show_names (
    TEXT	*name,
    USHORT	width,
    TEXT	*buffer)
{
/**************************************
 *
 *	s h o w _ n a m e s
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
BASED ON QLI$PROCEDURES.QLI$PROCEDURE_NAME padded_name;
TEXT	*s;
SSHORT	len, l1, l2;

s = buffer + strlen (buffer);
len = sizeof (padded_name) - 1;
l1 = s - buffer;
if ((s != buffer) && 
    ((l1 + len + 4) > width))
    {
    ib_printf ("%s\n", buffer);
    buffer[0] = 0;
    s = buffer;
    }
*s++ = ' ';
*s++ = ' ';
*s++ = ' ';
*s++ = ' ';
strcpy (s, name);
while (*s)
    {
    s++;
    len--;
    }
while (len--)
    *s++ = ' ';
*s = 0;
}

static void show_proc (
    QPR		proc)
{
/**************************************
 *
 *	s h o w _ p r o c 
 *
 **************************************
 *
 * Functional description
 *	Show definition of procedure.  If a particular database
 *	was specified, look there, otherwise, print all procedures
 *	with that name.
 *
 **************************************/
int	*blob, count;
DBB	database;
NAM	name;

count = 0;
name = proc->qpr_name;

if (database = proc->qpr_database)
    {
    if (blob = PRO_fetch_procedure (database, name->nam_string))
	display_procedure (database, name->nam_string, blob);
    else
	ERRQ_msg_put (126, /* Msg126 Procedure %s not found in database %s */
		name->nam_string, database->dbb_symbol->sym_string, NULL, NULL, NULL);
    return;
    }

for (database = QLI_databases; database; database = database->dbb_next)
    if (blob = PRO_fetch_procedure (database, name->nam_string))
	{
	display_procedure (database, name->nam_string, blob);
	count++;
	}	

if (!count)
    {
    ERRQ_msg_put (127, name->nam_string, NULL, NULL, NULL, NULL); /* Msg127 Procedure %s not found */
    return;
    }
}

static void show_procs (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ p r o c s
 *
 **************************************
 *
 * Functional description
 *	Show procedures in a database
 *
 **************************************/
SSHORT	width;
TEXT	buffer[256];

width = get_window_size (sizeof (buffer) - 1);

PRO_setup (database);
ERRQ_msg_put (128, database->dbb_filename, /* Msg128 Procedures in database %s (%s): */
		database->dbb_symbol->sym_string, NULL, NULL, NULL);
buffer[0] = 0;
FOR (REQUEST_HANDLE database->dbb_scan_blobs) X IN DB.QLI$PROCEDURES
    SORTED BY X.QLI$PROCEDURE_NAME
    show_names (X.QLI$PROCEDURE_NAME, width, buffer);
END_FOR;
if (buffer[0])
    ib_printf ("%s\n", buffer);
}

static void show_rel (
    REL		relation)
{
/**************************************
 *
 *	s h o w _ r e l
 *
 **************************************
 *
 * Functional description
 *	Just print a relation name.  Almost mindless.
 *
 **************************************/

if (QLI_abort)
    return;

ib_printf ("    %s\n", relation->rel_symbol->sym_string);
}

static void show_rel_detail (
    REL		relation)
{
/**************************************
 *
 *	s h o w _ r e l _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Ask META to print description, 
 *	security class, etc.
 *
 **************************************/
DBB	database;
TEXT	*relation_name;
USHORT	msg;

relation_name = relation->rel_symbol->sym_string;
database = relation->rel_database;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_rel_detail])
	R IN RDB$RELATIONS WITH 
	R.RDB$RELATION_NAME = relation_name
    show_text_blob (database, "\t", 291, &R.RDB$DESCRIPTION, 0, NULL_PTR, FALSE);

    if (database->dbb_capabilities & DBB_cap_security)
	FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_rel_secur])
	    S IN RDB$RELATIONS WITH S.RDB$RELATION_NAME = R.RDB$RELATION_NAME
		AND S.RDB$SECURITY_CLASS NOT MISSING
	    show_string (292, S.RDB$SECURITY_CLASS, 0, NULL_PTR);
	END_FOR;

    if (database->dbb_capabilities & DBB_cap_extern_file)
	FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_rel_extern])
	    E IN RDB$RELATIONS WITH E.RDB$RELATION_NAME = R.RDB$RELATION_NAME
		AND E.RDB$EXTERNAL_FILE NOT MISSING
	    ERRQ_msg_put (293, E.RDB$EXTERNAL_FILE, NULL, NULL, NULL, NULL); /* Msg293 Stored in external file %s */
	END_FOR; 

/* OBSOLETE - Handling of DBB_cap_triggers (old V2 style triggers) */
/* OBSOLETE - Msg294 An erase trigger is defined for %s */
/* OBSOLETE - Msg295 A modify trigger is defined for %s */
/* OBSOLETE - Msg296 A store trigger is defined for %s */

    if (database->dbb_capabilities & DBB_cap_new_triggers)
	{
	msg = 380;
	FOR (REQUEST_HANDLE database->dbb_requests [REQ_new_trig_exists]) 
		X IN RDB$TRIGGERS WITH X.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
	    if (!X.RDB$TRIGGER_BLR.NULL)
		{
		if (msg)
		    {
		    ERRQ_msg_put (msg, NULL, NULL, NULL, NULL, NULL); /* Msg380 Trigger defined for this relation */
		    msg = 0;
		    }
		ib_printf ("\t");
		show_trigger_status (X.RDB$TRIGGER_NAME, X.RDB$TRIGGER_TYPE,
		    X.RDB$TRIGGER_INACTIVE, X.RDB$TRIGGER_SEQUENCE);
		}
	END_FOR;   
	}
END_FOR;
} 

static void show_rels (
    DBB		dbb,
    ENUM show_t	sw,
    SSHORT	width)
{
/**************************************
 *
 *	s h o w _ r e l s
 *
 **************************************
 *
 * Functional description
 *	Display information about a relations
 *      or system_relations for a specific database.
 *
 **************************************/
SYN		*ptr, value;
REL		relation;
TEXT		buffer[256];
 
switch (sw) 
    {
    case show_relations:
	{
	show_dbb (dbb);
        buffer[0] = 0;
	for (relation = dbb->dbb_relations; relation; 
		relation = relation->rel_next)
	    if (!(relation->rel_flags & REL_system))
                show_names (relation->rel_symbol->sym_string, width, buffer);
            if (buffer[0])
                ib_printf ("%s\n", buffer);
	    ib_printf ("\n"); 
	break;
	}
    case show_system_relations:
	{
	show_dbb (dbb);
        buffer[0] = 0;
	for (relation = dbb->dbb_relations; relation; 
	    relation = relation->rel_next)
	    if (relation->rel_flags & REL_system)
                show_names (relation->rel_symbol->sym_string, width, buffer);
            if (buffer[0])
                ib_printf ("%s\n", buffer);
	    ib_printf ("\n");
	}
    }
}
 
static int show_security_class_detail (
    DBB		database,
    TEXT	*name)
{
/**************************************
 *
 *	s h o w _ s e c u r i t y _ c l a s s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show security classes for a databases.
 *
 **************************************/
SSHORT	count;

count = 0;
MET_meta_transaction (database, FALSE);

if (!database->dbb_capabilities & DBB_cap_security)
    return FALSE;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_secur_class])
	    S IN RDB$SECURITY_CLASSES WITH S.RDB$SECURITY_CLASS = name
    if (truncate_string (S.RDB$SECURITY_CLASS))
	{
	ib_printf ("\t%s:\n", S.RDB$SECURITY_CLASS);	
	display_acl (database, &S.RDB$ACL);
	}
    count++;
END_FOR;

return count; 
}

static int show_security_classes_detail (
    DBB	database)
{
/**************************************
 *
 *	s h o w _ s e c u r i t y _ c l a s s e s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show security classes for a databases.
 *
 **************************************/
SSHORT	count;

count = 0;
MET_meta_transaction (database, FALSE);

if (!database->dbb_capabilities & DBB_cap_security)
    return FALSE;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_secur])
	    S IN RDB$SECURITY_CLASSES
    if (!count)
	ERRQ_msg_put (297, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg297 Security classes for database %s */

    if (truncate_string (S.RDB$SECURITY_CLASS))
	{
	ib_printf ("\t%s:\n", S.RDB$SECURITY_CLASS);	
	display_acl (database, &S.RDB$ACL);
	}
    count++;
END_FOR; 

return count;
}

static void show_string (
    USHORT	msg1,
    TEXT	*string1,
    USHORT	msg2,
    TEXT	*string2)
{
/**************************************
 *
 *	s h o w _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Show one of two strings and maybe a message, too.
 *
 **************************************/

if (string2 && (!*string1 || *string1 == ' '))
    {
    msg1 = msg2;
    string1 = string2;
    }

if (!*string1 || *string1 == ' ')
    return;

truncate_string (string1);
ERRQ_msg_put (msg1, string1, NULL, NULL, NULL, NULL);
}

static void show_sys_trigs (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ s y s _ t r i g s
 *
 **************************************
 *
 * Functional description
 *	Ask META to print trigger information
 *	for the system.  Since parse
 *	has already swallowed a possible database
 *	qualifier, just assume that the database
 *	is the right one.
 *
 **************************************/
USHORT	count;

MET_meta_transaction (database, FALSE);
count = 0;

if (database->dbb_capabilities & DBB_cap_new_triggers)
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_sys_triggers])
	X IN RDB$TRIGGERS WITH X.RDB$SYSTEM_FLAG EQ 1
	SORTED BY X.RDB$RELATION_NAME, X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE, X.RDB$TRIGGER_NAME
	if (!X.RDB$TRIGGER_BLR.NULL)
	    {
	    ERRQ_msg_put (379, X.RDB$RELATION_NAME, NULL, NULL, NULL, NULL); /* Msg379 System Triggers */
	    show_trigger_header (X.RDB$TRIGGER_NAME, X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE,
		X.RDB$TRIGGER_INACTIVE, &X.RDB$DESCRIPTION,
		database, X.RDB$RELATION_NAME);
	    show_blob_info (&X.RDB$TRIGGER_BLR, &X.RDB$TRIGGER_SOURCE,
		377, /* Msg377 Source for the trigger is not available.  Trigger BLR: */
		376, /* Msg376 Source for the trigger */
		database, X.RDB$RELATION_NAME);
	    show_trigger_messages (database, X.RDB$TRIGGER_NAME);
	    count++;
	    }
    END_FOR;

if (!count)
    ERRQ_msg_put (378, NULL, NULL, NULL, NULL, NULL); /* Msg378 No system triggers  */
}

static void show_text_blob (
    DBB		database,
    TEXT	*column,
    USHORT	msg1,
    SLONG	*blob1,
    USHORT	msg2,
    SLONG	*blob2,
    int         strip)
{
/**************************************
 *
 *	s h o w _ t e x t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Display a text blob.
 *
 **************************************/

if (blob2 && !blob1 [0] && !blob1 [1])
    {
    blob1 = blob2;
    msg1 = msg2;
    }

if (!blob1 [0] && !blob1 [1])
    return;

if (msg1)
    ERRQ_msg_put (msg1, NULL, NULL, NULL, NULL, NULL);

display_text (database, blob1, column, strip);
}

static void show_trig (
    REL		relation)
{
/**************************************
 *
 *	s h o w _ t r i g
 *
 **************************************
 *
 * Functional description
 *	Ask META to print trigger information
 *	for a particular relation.  Since parse
 *	has already swallowed a possible database
 *	qualifier, just assume that the database
 *	is the right one.
 *
 **************************************/

if (!(show_trigger_detail (relation->rel_database, 
	relation->rel_symbol->sym_string)))
    ERRQ_msg_put (129, relation->rel_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg129 No triggers are defined for relation %s */
}

static int show_trigger_detail (
    DBB		database,
    TEXT	*relation_name)
{
/**************************************
 *
 *	s h o w _ t r i g g e r _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show trigger definitions for a particular
 *	relation.
 *
 **************************************/
USHORT	count;

if (!(database->dbb_capabilities & DBB_cap_new_triggers))
    return 0;

MET_meta_transaction (database, FALSE);
count = 0;

/* New style triggers */

if (database->dbb_capabilities & DBB_cap_new_triggers)
    {
    ERRQ_msg_put (365, relation_name, NULL, NULL, NULL, NULL); /* Msg365 Triggers for relation %s */
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_new_trigger])
	    X IN RDB$TRIGGERS WITH X.RDB$RELATION_NAME EQ relation_name
	    AND (X.RDB$SYSTEM_FLAG MISSING OR X.RDB$SYSTEM_FLAG EQ 0)
	    SORTED BY X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE, X.RDB$TRIGGER_NAME
	if (!X.RDB$TRIGGER_BLR.NULL)
	    {
	    show_trigger_header (X.RDB$TRIGGER_NAME, X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE,
		X.RDB$TRIGGER_INACTIVE, &X.RDB$DESCRIPTION,
		database, relation_name);
	    show_blob_info (&X.RDB$TRIGGER_BLR, &X.RDB$TRIGGER_SOURCE,
		377, /* Msg377 Source for the trigger is not available.  Trigger BLR: */
		376, /* Msg376 Source for the trigger */
		database, relation_name);
	    show_trigger_messages (database, X.RDB$TRIGGER_NAME);
	    count++;
	    }
    END_FOR;
    return count;
    }

/* Old style triggers */
/* OBSOLETE - 1996-Aug-06 David Schnepper  */

/* OBSOLETE - Msg298 Triggers for relation %s: */
/* OBSOLETE - Msg299 Source for the erase trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg300 Erase trigger for relation %s: */
/* OBSOLETE - Msg301 Source for the modify trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg302 Modify trigger for relation %s: */
/* OBSOLETE - Msg303 Source for the store trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg304 Store trigger for relation %s: */

return count;
}

static void show_trigger_header (
    TEXT	*name,
    USHORT	type,
    USHORT	sequence,
    USHORT	inactive,
    SLONG	*description,
    DBB		database,
    TEXT	*relation_name)
{
/*****************************************************
 *
 *	s h o w _ t r i g g e r _ h e a d e r
 *
 *****************************************************
 *
 * Functional description
 * 	Display header information for a new style 
 *	trigger only.
 *
 *****************************************************/

show_trigger_status (name, type, inactive, sequence);
show_text_blob (database, "\t    ", 375, description, 0, NULL_PTR, TRUE);
}

static void show_trigger_messages (
    DBB		database,
    TEXT	*name)
{
/*****************************************************
 *
 *	s h o w _ t r i g g e r _ m e s s a g e s
 *
 *****************************************************
 *
 *	 Display error messages associated with 
 *       a new style (V3) trigger
 *
 *****************************************************/
int	first;

first = TRUE;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_trig_message])
    M IN RDB$TRIGGER_MESSAGES WITH M.RDB$TRIGGER_NAME = name 
    SORTED BY M.RDB$MESSAGE_NUMBER
    if (first)
	{
        ERRQ_msg_put (456, name, NULL, NULL, NULL, NULL);
	/* msg 456: Messages associated with %s:\n */
        first = FALSE;
	}
    ERRQ_msg_put (457, (TEXT*) M.RDB$MESSAGE_NUMBER, M.RDB$MESSAGE, NULL, NULL, NULL);
    /* msg 457:     message %ld:  %s\n */
END_FOR;

if (!first)
    ib_printf ("\n");
}

static void show_trigger_status (
    TEXT	*name,
    USHORT	type,
    USHORT	status,
    USHORT	sequence)
{
/*****************************************************
 *
 *	s h o w _ t r i g g e r _ s t a t u s
 *
 *****************************************************
 *
 *	 Display type and active status of a new style (V3) trigger
 *
 *****************************************************/
SCHAR	trigger_type[16], trigger_active[9];
int	msg_num;

switch (type)
    {
    case PRE_STORE:	msg_num = 367; break; /* Msg367 pre-store */
    case POST_STORE:	msg_num = 368; break; /* Msg368 post-store */
    case PRE_MODIFY:	msg_num = 369; break; /* Msg369 pre-modify */
    case POST_MODIFY:	msg_num = 370; break; /* Msg370 post-modify */
    case PRE_ERASE:	msg_num = 371; break; /* Msg371 pre-erase */
    case POST_ERASE:	msg_num = 372; break; /* Msg372 post-erase */
    default:		msg_num = 372; break; /* Should never happen, not worth an error */
    }

ERRQ_msg_format (msg_num, sizeof (trigger_type), trigger_type, NULL, NULL, NULL, NULL, NULL);
msg_num = (status) ? 374 : 373;
ERRQ_msg_format (msg_num, sizeof (trigger_active), trigger_active, NULL, NULL, NULL, NULL, NULL);

truncate_string (name);
ERRQ_msg_put (366, name, trigger_type, (TEXT*) (ULONG) sequence, trigger_active, NULL); /* Msg366 Name: %s, Type: %s, Sequence: %d, Active: %s */
}

static void show_trigs (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ t r i g s
 *
 **************************************
 *
 * Functional description
 *	Ask META to print all trigger information
 *	for a particular database or all databases.  
 *
 **************************************/

if (database)
    {
    if (!(show_triggers_detail (database)))
        ERRQ_msg_put (130, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg130 No triggers are defined in database %s */
    }
else
    for (database = QLI_databases; database; database = database->dbb_next)
	if (!(show_triggers_detail (database)))
	    ERRQ_msg_put (131, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg131 No triggers are defined in database %s */
}

static int show_triggers_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ t r i g g e r s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show trigger definitions for a particular
 *	database
 *
 **************************************/
USHORT	count;

MET_meta_transaction (database, FALSE);
count = 0;

if (!(database->dbb_capabilities & DBB_cap_new_triggers))
    return count;

if (database->dbb_capabilities & DBB_cap_new_triggers)
    {

/* New style triggers */

    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_new_triggers])
	    X IN RDB$TRIGGERS WITH X.RDB$SYSTEM_FLAG MISSING
	    OR X.RDB$SYSTEM_FLAG EQ 0 SORTED BY X.RDB$RELATION_NAME,
	    X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE, X.RDB$TRIGGER_NAME
	if (!X.RDB$TRIGGER_BLR.NULL)
	    {
	    truncate_string (X.RDB$RELATION_NAME);
	    ERRQ_msg_put (381, X.RDB$RELATION_NAME, NULL, NULL, NULL, NULL); /* Msg365 Triggers for relation %s */
	    show_trigger_header (X.RDB$TRIGGER_NAME, X.RDB$TRIGGER_TYPE, X.RDB$TRIGGER_SEQUENCE,
		X.RDB$TRIGGER_INACTIVE, &X.RDB$DESCRIPTION,
		database, X.RDB$RELATION_NAME);
	    show_blob_info (&X.RDB$TRIGGER_BLR, &X.RDB$TRIGGER_SOURCE,
		377, /* Msg377 Source for the trigger is not available.  Trigger BLR: */
		376, /* Msg376 Source for the trigger */
		database, X.RDB$RELATION_NAME);
	    show_trigger_messages (database, X.RDB$TRIGGER_NAME);
	    count++;
	    }
    END_FOR;
    }
else
    {

/* Old style triggers */
/* OBSOLETE - 1996-Aug-06 David Schnepper  */
/* OBSOLETE - Msg305 Triggers for relation %s: */

/* OBSOLETE - Msg306 Source for the erase trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg307 Erase trigger for relation %s: */

/* OBSOLETE - Msg308 Source for the modify trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg309 Modify trigger for relation %s: */

/* OBSOLETE - Msg310 Source for the store trigger is not available.  Trigger BLR: */
/* OBSOLETE - Msg311 Store trigger for relation %s: */
    }

return count;
}

static void show_var (
    NAM         var_name)
{
/**************************************
 *
 *	s h o w _ v a r
 *
 **************************************
 *
 * Functional description
 *	Display a global variables.
 *
 **************************************/
FLD	variable;

for (variable = QLI_variables; variable; variable = variable->fld_next)
    if (!strcmp (var_name->nam_string, variable->fld_name->sym_string))
	break;

if (!variable)
    {
    ERRQ_msg_put (474, var_name->nam_string, NULL, NULL, NULL, NULL); /* Msg474 Variable %s has not been declared */
    return;
    }

ERRQ_msg_put (471, variable->fld_name->sym_string, NULL, NULL, NULL, NULL); /* Msg471 Variable %s */
ERRQ_msg_put (475, NULL, NULL, NULL, NULL, NULL);
ib_printf ("\t");
show_datatype (NULL, variable->fld_dtype,
    (variable->fld_dtype == dtype_varying) ? variable->fld_length - 2 : variable->fld_length,
    variable->fld_scale, variable->fld_sub_type, variable->fld_segment_length,
    (variable->fld_flags & FLD_array) ? 1 : 0);
ib_printf ("\n");
if (variable->fld_query_name)
    ERRQ_msg_put (472, variable->fld_query_name->sym_string, NULL, NULL, NULL, NULL);
if (variable->fld_edit_string)
    ERRQ_msg_put (473, variable->fld_edit_string, NULL, NULL, NULL, NULL);
}

static void show_vars (void)
{
/**************************************
 *
 *	s h o w _ v a r s
 *
 **************************************
 *
 * Functional description
 *	Display global variables.  If there aren't any, don't
 *	display any.
 *
 **************************************/

ERRQ_msg_put (132, NULL, NULL, NULL, NULL, NULL); /* Msg132 Variables: */
show_fields (NULL_PTR, QLI_variables);
}

static void show_versions (void)
{
/**************************************
 *
 *	s h o w _ v e r s i o n s
 *
 **************************************
 *
 * Functional description
 *	Show the version for QLI and all databases.
 *
 **************************************/
DBB	dbb;

ERRQ_msg_put (133, GDS_VERSION, NULL, NULL, NULL, NULL); /* Msg133 QLI, version %s */

for (dbb = QLI_databases; dbb; dbb = dbb->dbb_next)
    {
    ERRQ_msg_put (134, dbb->dbb_filename, NULL, NULL, NULL, NULL); /* Msg134 Version(s) for database %s */
    gds__version (&dbb->dbb_handle, NULL_PTR, NULL_PTR);
    }
}

static void show_view (
    REL	relation)
{
/**************************************
 *
 *	s h o w _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Call MET to show the view definition
 *	if there is one.
 *
 **************************************/
DBB	database;

database = relation->rel_database;
MET_meta_transaction (database, FALSE);

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_view]) 
    X IN RDB$RELATIONS WITH X.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
	AND X.RDB$VIEW_BLR NOT MISSING
    if (X.RDB$VIEW_SOURCE.NULL)
	{
	ERRQ_msg_put (312, relation->rel_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg312 View source for relation %s is not available.  View BLR: */
	display_blr (database, &X.RDB$VIEW_BLR);
	}
    else
	{ 
	ERRQ_msg_put (313, relation->rel_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg313 Relation %s is a view defined as: */
	show_text_blob (database, "\t", 0, &X.RDB$VIEW_SOURCE, 0, NULL_PTR,  FALSE);
	}
END_FOR;
}

static int show_views_detail (
    DBB		database)
{
/**************************************
 *
 *	s h o w _ v i e w s _ d e t a i l
 *
 **************************************
 *
 * Functional description
 *	Show the view definitions.  If none, return 0.
 *
 **************************************/
int	count;

MET_meta_transaction (database, FALSE);
count = 0;

FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_views]) 
    X IN RDB$RELATIONS WITH X.RDB$VIEW_BLR NOT MISSING 
    SORTED BY X.RDB$RELATION_NAME
    if (!count++)
	ERRQ_msg_put (316, database->dbb_symbol->sym_string, NULL, NULL, NULL, NULL); /* Msg316 Views in database %s: */

    truncate_string (X.RDB$RELATION_NAME);
    ERRQ_msg_put (317, X.RDB$RELATION_NAME, NULL, NULL, NULL, NULL); /* Msg317 %s comprised of: */
    FOR (REQUEST_HANDLE database->dbb_requests [REQ_show_view_rel])
	V IN RDB$VIEW_RELATIONS 
	WITH V.RDB$VIEW_NAME = X.RDB$RELATION_NAME 
	SORTED BY V.RDB$RELATION_NAME
	    ib_printf ("	%s\n", V.RDB$RELATION_NAME);
    END_FOR;

END_FOR;

return count;
}                     

static int truncate_string (
    TEXT	*string)
{
/**************************************
 *
 *	t r u n c a t e _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Convert a blank filled string to
 *	a null terminated string without
 *	trailing blanks.  Because some
 *	strings contain embedded blanks
 *	(e.g. query headers & edit strings)
 *	truncate from the back forward.
 *	Return the number of characters found,
 *	not including the terminating null.
 *
 **************************************/
TEXT	*p;

for (p = string; *p; p++)
    ;

while (p > string && p [-1] == ' ')
    --p;

*p = 0;

return (p - string);
}

static void view_info (
    DBB		dbb,
    TEXT	*view,
    TEXT	*base_field,
    SSHORT	context,
    SSHORT	level)
{
/**************************************
 *
 *	v i e w _ i n f o
 *
 **************************************
 *
 * Functional description
 *
 *	field came from another relation.
 *	expand on this phenomenon.
 *
 **************************************/
TEXT	*rt, spaces [64], *p, *end;

FOR (REQUEST_HANDLE dbb->dbb_requests [REQ_show_view_field] LEVEL level)
	RFR IN RDB$RELATION_FIELDS CROSS VR IN RDB$VIEW_RELATIONS
	WITH RFR.RDB$FIELD_NAME = base_field AND
	RFR.RDB$RELATION_NAME = VR.RDB$RELATION_NAME AND
	VR.RDB$VIEW_NAME = view AND
	VR.RDB$VIEW_CONTEXT = context

    truncate_string (RFR.RDB$RELATION_NAME);
                                                                             
    /* create some space according to how many levels deep we are;
       this is to avoid using %*s in the ib_printf, which doesn't work for OS/2 */

    for (p = spaces, end = spaces + MIN ((level + 2) * 4, sizeof (spaces) - 1); p < end;)
	*p++ = ' ';
    *p = '\0';

    if (RFR.RDB$BASE_FIELD.NULL)
        ERRQ_msg_put (507, spaces, base_field, RFR.RDB$RELATION_NAME, NULL, NULL); 
        /* Msg507 %s Based on field %s of relation %s */
    else 
        ERRQ_msg_put (508, spaces, base_field, RFR.RDB$RELATION_NAME, NULL, NULL); 
        /* Msg508 %s Based on field %s of view %s */
    show_text_blob (dbb, "\t", 349, &RFR.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);

    if (!RFR.RDB$DESCRIPTION.NULL)
	{
	ERRQ_msg_put (349, spaces, base_field, NULL, NULL, NULL); /* Msg349 %sBase field description for %s: */
	show_text_blob (dbb, "\t\t", 0, &RFR.RDB$DESCRIPTION, 0, NULL_PTR,  FALSE);
	}

    if (!RFR.RDB$BASE_FIELD.NULL && (dbb->dbb_capabilities & DBB_cap_multi_trans))
	{
	truncate_string (RFR.RDB$BASE_FIELD);
	view_info (dbb, RFR.RDB$RELATION_NAME, RFR.RDB$BASE_FIELD,
		RFR.RDB$VIEW_CONTEXT, level + 1);
	}
END_FOR;
}
