/*
 *	PROGRAM:	C preprocessor
 *	MODULE:		met.e
 *	DESCRIPTION:	Meta data interface to system
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
#include "../jrd/gds.h"
#include "../gpre/gpre.h"
#include "../jrd/license.h"
#include "../gpre/parse.h"
#include "../jrd/intl.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/hsh_proto.h"
#include "../gpre/jrdme_proto.h"
#include "../gpre/met_proto.h"
#include "../gpre/msc_proto.h"
#include "../gpre/par_proto.h"
#include "../jrd/constants.h"

#define MAX_USER_LENGTH		33
#define MAX_PASSWORD_LENGTH	33

DATABASE
    DB = FILENAME "yachts.lnk"
	 RUNTIME dbb->dbb_filename;

extern enum lan_t	sw_language;
extern USHORT		sw_cstring;
extern DBB		isc_databases;

static CONST UCHAR	blr_bpb [] = {	isc_bpb_version1,
					isc_bpb_source_type, 1, BLOB_blr,
					isc_bpb_target_type, 1, BLOB_blr};

#ifdef SCROLLABLE_CURSORS
static SCHAR	db_version_info [] = { gds__info_base_level };
#endif

static SLONG	array_size (FLD);
static void	get_array (DBB, TEXT *, FLD);
static int	get_intl_char_subtype (SSHORT *, UCHAR *, USHORT, DBB);
static int	resolve_charset_and_collation (SSHORT *, UCHAR *, UCHAR *);
static int	symbol_length (TEXT *);
static int	upcase (TEXT *, TEXT *);

FLD MET_context_field (
    CTX	    context,
    UCHAR    *string)
{
/**************************************
 *
 *	M E T _ c o n t e x t _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Lookup a field by name in a context.
 *	If found, return field block.  If not, return NULL.
 *
 **************************************/
SYM	symbol;
PRC	procedure;
FLD	field;
SCHAR	name [NAME_SIZE];
SSHORT	length;

if (context->ctx_relation)
    return (MET_field (context->ctx_relation, string));

if (!context->ctx_procedure)
    return NULL;

strcpy (name, string);
procedure = context->ctx_procedure;

/* At this point the procedure should have been scanned, so all its fields
   are in the symbol table. */

field = NULL;
for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_field &&
	     (field = (FLD) symbol->sym_object) &&
	     field->fld_procedure == procedure)
	return field;

return field;
}

BOOLEAN MET_database (
    DBB		dbb,
    BOOLEAN     print_version)
{
/**************************************
 *
 *	M E T _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Initialize meta data access to database.  If the
 *	database can't be opened, return FALSE.
 *
 **************************************/
SCHAR	dpb [MAX_PASSWORD_LENGTH + MAX_USER_LENGTH + 5], *d, *p;
SCHAR	buffer [16], *data;
SSHORT	l;
static CONST UCHAR
	sql_version_info [] = { isc_info_base_level,
				isc_info_ods_version,
				isc_info_db_sql_dialect,
			        isc_info_end };
    /* 
    ** Each info item requested will return 
    **
    **	  1 byte for the info item tag
    **	  2 bytes for the length of the information that follows
    **	  1 to 4 bytes of integer information
    **
    ** isc_info_end will not have a 2-byte length - which gives us
    ** some padding in the buffer.
    */
UCHAR	sql_buffer [sizeof(sql_version_info)*(1+2+4)];
UCHAR	*ptr;


#ifndef REQUESTER
if (sw_language == lan_internal)
    {
    JRDMET_init (dbb);
    return TRUE;
    }
#endif

DB = NULL;

if (!dbb->dbb_filename)
    {
    CPR_error ("No database specified");
    return FALSE;
    }

/* generate a dpb for the attach from the specified 
   compiletime user name and password */

d = dpb;
if ( dbb->dbb_c_user || dbb->dbb_c_password)
    {
    *d++ = gds__dpb_version1;
    if (p = dbb->dbb_c_user)
        {
        *d++ = gds__dpb_user_name;
        *d++ = strlen (p);
        while (*p)
	    *d++ = *p++;
        }
    if (p = dbb->dbb_c_password)
        {
        *d++ = gds__dpb_password;
        *d++ = strlen (p);
        while (*p)
	    *d++ = *p++;
        }
    }

if (gds__attach_database (gds__status, 0, GDS_VAL(dbb->dbb_filename), GDS_REF(DB), d - &dpb [0], dpb))
    {
    gds__print_status (gds__status);
    return FALSE;
    }

dbb->dbb_handle = DB;

if (sw_version && print_version)
    {
    ib_printf ("    Version(s) for database \"%s\"\n", dbb->dbb_filename);
    gds__version (&DB, NULL, NULL); 
    }

sw_ods_version = 0;
sw_server_version = 0;
if (isc_database_info (isc_status, &DB, sizeof (sql_version_info),
			 sql_version_info, sizeof (sql_buffer), sql_buffer))
    {
    gds__print_status (isc_status);
    return FALSE;
    }

ptr = sql_buffer;
while (*ptr != isc_info_end)
    {
    UCHAR	item;
    USHORT	length;

    item = (UCHAR) *ptr++;
    length = isc_vax_integer (ptr, sizeof (USHORT));
    ptr += sizeof (USHORT);
    switch (item)
	{
	case isc_info_base_level:
	    sw_server_version = (USHORT) ptr [1];
	    break;

	case isc_info_ods_version:
	    sw_ods_version = isc_vax_integer (ptr, length);
	    break;

	case isc_info_db_sql_dialect:
	    compiletime_db_dialect = isc_vax_integer (ptr, length);
	    break;
	case isc_info_error:
	    /* Error indicates that one of the  option
	       was not understood by thr server. Make sure it was
	       isc_info_db_sql_dialect and assume
	       that the database dialect is SQL_DIALECT_V5
	    */
	    if ( (sw_server_version != 0) && (sw_ods_version != 0) )
                compiletime_db_dialect =  SQL_DIALECT_V5;
	    else
	        ib_printf ("Internal error: Unexpected isc_info_value %d\n",
			item);
	    break;
	default:
	    ib_printf ("Internal error: Unexpected isc_info_value %d\n",
			item);
	    break;
	}
    ptr += length;
    }

if (!dialect_specified)
    sw_sql_dialect = compiletime_db_dialect;
if (sw_ods_version < 10)
    {
    if (sw_sql_dialect != compiletime_db_dialect)
	{
        char warn_mesg[100];
        sprintf (warn_mesg, "Pre 6.0 database. Cannot use dialect %d, Resetting to %d\n", sw_sql_dialect, compiletime_db_dialect);
        CPR_warn (warn_mesg);
	}
    sw_sql_dialect = compiletime_db_dialect;
    }

else if (sw_sql_dialect != compiletime_db_dialect)
    {
    char warn_mesg[100];
    sprintf (warn_mesg, "Client dialect set to %d. Compiletime database dialect is %d\n", sw_sql_dialect, compiletime_db_dialect);
    CPR_warn (warn_mesg);
    }

#ifdef DEBUG
if (sw_version && print_version)
    ib_printf ("Gpre Start up values. \
	\n\tServer Ver                   : %d\
        \n\tCompile time db Ver          : %d\
	\n\tCompile time db Sql Dialect  : %d\
	\n\tClient Sql dialect set to    : %d\n\n", \
	sw_server_version, sw_ods_version, compiletime_db_dialect, \
	sw_sql_dialect);
#endif
#ifdef SCROLLABLE_CURSORS
/* get the base level of the engine */
 
if (gds__database_info (gds__status, 
	GDS_REF(DB),
	sizeof (db_version_info),
	db_version_info, 
	sizeof (buffer), 
	buffer))
    {
    gds__print_status (gds__status);
    return FALSE;
    }

/* this seems like a lot of rigamarole to read one info item, 
   but it provides for easy extensibility in case we need 
   more info items later */

data = buffer;
while ((p = *data++) != gds__info_end)
    {
    l = isc_vax_integer (data, 2);
    data += 2;

    switch ((int) p)
	{
	/* This flag indicates the version level of the engine  
           itself, so we can tell what capabilities the engine 
           code itself (as opposed to the on-disk structure).
           Apparently the base level up to now indicated the major 
           version number, but for 4.1 the base level is being 
           incremented, so the base level indicates an engine version 
           as follows:
             1 == v1.x
             2 == v2.x
             3 == v3.x
             4 == v4.0 only
             5 == v4.1
           Note: this info item is so old it apparently uses an 
           archaic format, not a standard vax integer format.
        */

	case gds__info_base_level:
	    dbb->dbb_base_level = (USHORT) data [1];
	    break;

	default:
	    ;
	}

    data += l;
    }
#endif

return TRUE;
}

USHORT MET_domain_lookup (
    REQ		request,
    FLD		field,
    UCHAR	*string)
{
/**************************************
 *
 *	M E T _ d o m a i n _ l o o k u p
 *
 **************************************
 *
 * Functional description
 *	Lookup a domain by name.
 *	Initialize the size of the field.
 *
 **************************************/
SYM	symbol;
DBB	dbb;
SCHAR	name [NAME_SIZE];
FLD	d_field;
SSHORT	length;
SSHORT	found = FALSE;

strcpy (name,string);

/* Lookup domain.  If we find it in the hash table, and it is not the
 * field we a currently looking at, use it.  Else look it up from the
 * database. */

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_field) && 
	((d_field = (FLD) symbol->sym_object) && (d_field != field)))
	{
	field->fld_length = d_field->fld_length;
	field->fld_scale  = d_field->fld_scale;
	field->fld_sub_type = d_field->fld_sub_type;
	field->fld_dtype  = d_field->fld_dtype;
	field->fld_ttype = d_field->fld_ttype;
	field->fld_charset_id = d_field->fld_charset_id;
	field->fld_collate_id = d_field->fld_collate_id;
	field->fld_char_length = d_field->fld_char_length;
	return TRUE;
	}

if (!request)
    return FALSE;

dbb = request->req_database;
if (dbb->dbb_flags & DBB_sqlca) 
    return FALSE;

DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;

if (!(dbb->dbb_flags & DBB_v3))
    {
    FOR (REQUEST_HANDLE dbb->dbb_domain_request)
	F IN RDB$FIELDS WITH F.RDB$FIELD_NAME EQ name

        found = TRUE;
        field->fld_length = F.RDB$FIELD_LENGTH;
        field->fld_scale = F.RDB$FIELD_SCALE;
        field->fld_sub_type = F.RDB$FIELD_SUB_TYPE;
        field->fld_dtype = MET_get_dtype (F.RDB$FIELD_TYPE, F.RDB$FIELD_SUB_TYPE, &field->fld_length);

        switch (field->fld_dtype)
	    {
	    case dtype_text:
	    case dtype_cstring:
	        field->fld_flags |= FLD_text;
	        break;

	    case dtype_blob:
	        field->fld_flags |= FLD_blob;
	        field->fld_seg_length = F.RDB$SEGMENT_LENGTH;
	        break;
	    }

	if (field->fld_dtype <= dtype_any_text)
	    {
	    if (!F.RDB$CHARACTER_LENGTH.NULL)
	        field->fld_char_length = F.RDB$CHARACTER_LENGTH;
	    if (!F.RDB$CHARACTER_SET_ID.NULL)
	        field->fld_charset_id = F.RDB$CHARACTER_SET_ID;
	    if (!F.RDB$COLLATION_ID.NULL)
	        field->fld_collate_id = F.RDB$COLLATION_ID;
	    }
	
        field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id);

    END_FOR;
    }
else 
    {
    FOR (REQUEST_HANDLE dbb->dbb_domain_request)
  	F IN RDB$FIELDS WITH F.RDB$FIELD_NAME EQ name

        found = TRUE;
        field->fld_length = F.RDB$FIELD_LENGTH;
        field->fld_scale = F.RDB$FIELD_SCALE;
        field->fld_sub_type = F.RDB$FIELD_SUB_TYPE;
        field->fld_dtype = MET_get_dtype (F.RDB$FIELD_TYPE, F.RDB$FIELD_SUB_TYPE, &field->fld_length);

        switch (field->fld_dtype)
	    {
	    case dtype_text:
	    case dtype_cstring:
	        field->fld_flags |= FLD_text;
	        break;

	    case dtype_blob:
	        field->fld_flags |= FLD_blob;
	        field->fld_seg_length = F.RDB$SEGMENT_LENGTH;
	        break;
	    }

        field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id);

    END_FOR;
    }

return found;
}

BOOLEAN MET_get_domain_default (
    DBB		dbb,
    TEXT	*domain_name,
    TEXT	*buffer,
    USHORT	buff_length)
{
/********************************************************************
 *
 *	M E T _ g e t _ d o m a i n _ d e f a u l t
 *
 ********************************************************************
 *
 * Functional description
 *	Gets the default value for a domain of an existing table
 *
 ********************************************************************/
SLONG		*DB, *gds__trans;
SCHAR		name [NAME_SIZE];
SSHORT		length;
BOOLEAN		has_default;
ISC_STATUS	status_vect[20];
isc_blob_handle	blob_handle = NULL;
ISC_QUAD	*blob_id;
TEXT		*ptr_in_buffer;
STATUS		stat;


strcpy (name, domain_name);
has_default = FALSE;

if (dbb == NULL)
    return FALSE;

if ( (dbb->dbb_handle == NULL) && !MET_database (dbb, FALSE))
    CPR_exit (FINI_ERROR);

assert (dbb->dbb_transaction == NULL);
gds__trans = NULL;

DB = dbb->dbb_handle;

START_TRANSACTION;

FOR (REQUEST_HANDLE dbb->dbb_domain_request)
    FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME EQ name

    if (!FLD.RDB$DEFAULT_VALUE.NULL)
	{
	blob_id = &FLD.RDB$DEFAULT_VALUE;

	/* open the blob */
	stat = isc_open_blob2 (status_vect, &DB,  &gds__trans, &blob_handle,
			       blob_id, sizeof (blr_bpb), blr_bpb);
	if (stat)
	    {
	    gds__print_status (status_vect);
	    CPR_exit (FINI_ERROR);
	    }

	/* fetch segments. Assume buffer is big enough.  */
	ptr_in_buffer = buffer;
	while (TRUE)
	    {
	    stat = isc_get_segment (status_vect, &blob_handle, &length,
				    buff_length, ptr_in_buffer);
	    ptr_in_buffer = ptr_in_buffer + length;
	    buff_length = buff_length - length;

	    if (!stat)
		continue;
	    else if (stat == gds__segstr_eof)
		{
		/* null terminate the buffer */
		*ptr_in_buffer = 0;
		break;
		}
	    else CPR_exit (FINI_ERROR);
	    }
	isc_close_blob (status_vect, &blob_handle);

	/* the default string must be of the form:
	   blr_version4 blr_literal ..... blr_eoc OR
	   blr_version4 blr_null blr_eoc OR
	   blr_version4 blr_user_name  blr_eoc */
	assert (buffer[0] == blr_version4 || buffer[0] == blr_version5);
	assert (buffer[1] == blr_literal ||
		buffer[1] == blr_null ||
		buffer[1] == blr_user_name );

	has_default = TRUE;
	}
    else	/* default not found */
	{
        if (sw_sql_dialect > SQL_DIALECT_V5)
            buffer[0] = blr_version5;
        else
            buffer[0] = blr_version4;
	buffer[1] = blr_eoc;
	}
END_FOR
COMMIT;
return (has_default);
}

BOOLEAN MET_get_column_default (
    REL		relation,
    TEXT	*column_name,
    TEXT	*buffer,
    USHORT	buff_length)
{
/********************************************************************
 *
 *	M E T _ g e t _ c o l u m n _ d e f a u l t
 *
 ********************************************************************
 *
 * Functional description
 *	Gets the default value for a column of an existing table.
 *	Will check the default for the column of the table, if that is
 *	not present, will check for the default of the relevant domain
 *
 *	The default blr is returned in buffer. The blr is of the form
 *	blr_version4 blr_literal ..... blr_eoc
 *
 *	Reads the system tables RDB$FIELDS and RDB$RELATION_FIELDS.
 *
 ********************************************************************/
DBB		dbb;
SLONG		*DB, *gds__trans;
SCHAR		name [NAME_SIZE];
SSHORT		length;
BOOLEAN		has_default;
ISC_STATUS	status_vect[20];
isc_blob_handle	blob_handle = NULL;
ISC_QUAD	*blob_id;
TEXT		*ptr_in_buffer;
STATUS		stat;

strcpy (name, column_name);
has_default = FALSE;

if ( (dbb = relation->rel_database) == NULL)
    return FALSE;

if ( (dbb->dbb_handle == NULL) && !MET_database (dbb, FALSE))
    CPR_exit (FINI_ERROR);

assert (dbb->dbb_transaction == NULL);
gds__trans = NULL;

DB = dbb->dbb_handle;

START_TRANSACTION;

FOR (REQUEST_HANDLE dbb->dbb_field_request)
    RFR IN RDB$RELATION_FIELDS CROSS F IN RDB$FIELDS WITH
    RFR.RDB$FIELD_SOURCE EQ F.RDB$FIELD_NAME AND
    RFR.RDB$FIELD_NAME EQ name AND
    RFR.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string

    if (!RFR.RDB$DEFAULT_VALUE.NULL)
	{
	blob_id = &RFR.RDB$DEFAULT_VALUE;
	has_default = TRUE;
	}
    else if (!F.RDB$DEFAULT_VALUE.NULL)
	{
	blob_id = &F.RDB$DEFAULT_VALUE;
	has_default = TRUE;
        }

    if(has_default)
	{
	/* open the blob */
	stat = isc_open_blob2 (status_vect, &DB,  &gds__trans,
            &blob_handle, blob_id, sizeof (blr_bpb), blr_bpb);
	if (stat)
	    {
	    gds__print_status (status_vect);
	    CPR_exit (FINI_ERROR);
	    }

	/* fetch segments. Assuming here that the buffer is big enough.*/
	ptr_in_buffer = buffer;
	while (TRUE)
	    {
	    stat = isc_get_segment (status_vect, &blob_handle,
				    &length, buff_length, ptr_in_buffer);
	    ptr_in_buffer = ptr_in_buffer + length;
	    buff_length = buff_length - length;
	    if (!stat)
		continue;
	    else if (stat == gds__segstr_eof)
		{
		/* null terminate the buffer */
		*ptr_in_buffer = 0;
		break;
		}
	    else CPR_exit (FINI_ERROR);
	    }
	isc_close_blob (status_vect, &blob_handle);

	/* the default string must be of the form:
	   blr_version4 blr_literal ..... blr_eoc OR
	   blr_version4 blr_null blr_eoc OR
	   blr_version4 blr_user_name blr_eoc */

	assert (buffer[0] == blr_version4 || buffer[0] == blr_version5);
	assert (buffer[1] == blr_literal ||
		buffer[1] == blr_null ||
		buffer[1] == blr_user_name);
	}
    else
	{
	if (sw_sql_dialect > SQL_DIALECT_V5)
	    buffer[0] = blr_version5;
	else
	    buffer[0] = blr_version4;
	buffer[1] = blr_eoc;
	}

END_FOR;
COMMIT;
return (has_default);
}

LLS MET_get_primary_key (
    DBB		dbb,
    TEXT	*relation_name)
{
/********************************************************************
 *
 *	M E T _ g e t _ p r i m a r y _ k e y
 *
 ********************************************************************
 *
 * Functional description
 *	Lookup the fields for the primary key
 *	index on a relation, returning a list
 *	of the fields.
 *
 ********************************************************************/
LLS		fields = NULL, *ptr_fields;
SLONG		*DB, *gds__trans;
SCHAR		name [NAME_SIZE];
STR		field_name;
TEXT		*tmp;

strcpy (name, relation_name);

if ( dbb == NULL)
    return NULL;

if ( (dbb->dbb_handle == NULL) && !MET_database (dbb, FALSE))
    CPR_exit (FINI_ERROR);

assert (dbb->dbb_transaction == NULL);
gds__trans = NULL;

DB = dbb->dbb_handle;

START_TRANSACTION;

ptr_fields = &fields;

FOR (REQUEST_HANDLE dbb->dbb_primary_key_request)
    X IN RDB$INDICES CROSS
    Y IN RDB$INDEX_SEGMENTS
    OVER RDB$INDEX_NAME
    WITH X.RDB$RELATION_NAME EQ name
    AND X.RDB$INDEX_NAME STARTING "RDB$PRIMARY"
    SORTED BY Y.RDB$FIELD_POSITION

    field_name = MAKE_STRING (Y.RDB$FIELD_NAME);

    /* Strip off any trailing spaces from field name */
    tmp = field_name;
    while (*tmp && *tmp != ' ')
	*tmp++;
    *tmp = '\0';

    PUSH (field_name, ptr_fields);
    ptr_fields = &(*ptr_fields)->lls_next;

END_FOR
COMMIT;

return fields;
}

FLD MET_field (
    REL		relation,
    UCHAR	*string)
{
/**************************************
 *
 *	M E T _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Lookup a field by name in a relation.
 *	If found, return field block.  If not, return NULL.
 *
 **************************************/
SYM	symbol;
FLD	field;
DBB	dbb;
SCHAR	name [NAME_SIZE];
SSHORT	length;

strcpy (name, string);
length = strlen(name);

/* Lookup field.  If we find it, nifty.  If not, look it up in the
   database. */

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_keyword &&
	symbol->sym_keyword == (int) KW_DBKEY)
	return relation->rel_dbkey;
    else if (symbol->sym_type == SYM_field &&
	     (field = (FLD) symbol->sym_object) &&
	     field->fld_relation == relation)
	return field;

if (sw_language == lan_internal)
    return NULL;
    
dbb = relation->rel_database;

if (dbb->dbb_flags & DBB_sqlca) 
    return NULL;

DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;
field = NULL;

if (!(dbb->dbb_flags & DBB_v3))
    {
    FOR (REQUEST_HANDLE dbb->dbb_field_request)
	    RFR IN RDB$RELATION_FIELDS CROSS F IN RDB$FIELDS WITH
	    RFR.RDB$FIELD_SOURCE EQ F.RDB$FIELD_NAME AND
	    RFR.RDB$FIELD_NAME EQ name AND
	    RFR.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
    
        field = (FLD) ALLOC (FLD_LEN);
        field->fld_relation = relation;
        field->fld_next = relation->rel_fields;
        relation->rel_fields = field;
        field->fld_id = RFR.RDB$FIELD_ID;
        field->fld_length = F.RDB$FIELD_LENGTH;
        field->fld_scale = F.RDB$FIELD_SCALE;
        field->fld_position = RFR.RDB$FIELD_POSITION;
        field->fld_sub_type = F.RDB$FIELD_SUB_TYPE;
        field->fld_dtype = MET_get_dtype (F.RDB$FIELD_TYPE, F.RDB$FIELD_SUB_TYPE, &field->fld_length);
    
        switch (field->fld_dtype)
	    {
	    case dtype_text:
	    case dtype_cstring:
	        field->fld_flags |= FLD_text;
	        break;

	    case dtype_blob:
	        field->fld_flags |= FLD_blob;
	        field->fld_seg_length = F.RDB$SEGMENT_LENGTH;
	        break;
	    }

        if (F.RDB$DIMENSIONS && !(dbb->dbb_flags & DBB_no_arrays))
	    get_array (dbb, F.RDB$FIELD_NAME, field);

	if ((field->fld_dtype <= dtype_any_text) || (field->fld_dtype == dtype_blob))
	    {
	    if (!F.RDB$CHARACTER_LENGTH.NULL)
		field->fld_char_length = F.RDB$CHARACTER_LENGTH;
	    if (!F.RDB$CHARACTER_SET_ID.NULL)
		field->fld_charset_id = F.RDB$CHARACTER_SET_ID;
	    if (!RFR.RDB$COLLATION_ID.NULL)
		field->fld_collate_id = RFR.RDB$COLLATION_ID;
            else if (!F.RDB$COLLATION_ID.NULL)
		field->fld_collate_id = F.RDB$COLLATION_ID;
	    }

        field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id); 
        field->fld_symbol = symbol = MSC_symbol (SYM_field, name, length, field);
        HSH_insert (symbol);
        field->fld_global = symbol = MSC_symbol (SYM_field, RFR.RDB$FIELD_SOURCE, 
		symbol_length (RFR.RDB$FIELD_SOURCE), field);
    END_FOR;
    }
else 
    {
    FOR (REQUEST_HANDLE dbb->dbb_field_request)
 	    RFR IN RDB$RELATION_FIELDS CROSS F IN RDB$FIELDS WITH
	    RFR.RDB$FIELD_SOURCE EQ F.RDB$FIELD_NAME AND
	    RFR.RDB$FIELD_NAME EQ name AND
	    RFR.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
        field = (FLD) ALLOC (FLD_LEN);
        field->fld_relation = relation;
        field->fld_next = relation->rel_fields;
        relation->rel_fields = field;
        field->fld_id = RFR.RDB$FIELD_ID;
        field->fld_length = F.RDB$FIELD_LENGTH;
        field->fld_scale = F.RDB$FIELD_SCALE;
        field->fld_position = RFR.RDB$FIELD_POSITION;
        field->fld_sub_type = F.RDB$FIELD_SUB_TYPE;
        field->fld_dtype = MET_get_dtype (F.RDB$FIELD_TYPE, F.RDB$FIELD_SUB_TYPE, &field->fld_length);
        switch (field->fld_dtype)
	    {
	    case dtype_text:
	    case dtype_cstring:
	        field->fld_flags |= FLD_text;
	        break;

	    case dtype_blob:
	        field->fld_flags |= FLD_blob;
	        field->fld_seg_length = F.RDB$SEGMENT_LENGTH;
	        break;
	    }

        if (!(dbb->dbb_flags & DBB_no_arrays))
	    get_array (dbb, F.RDB$FIELD_NAME, field);

        field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id); 
        field->fld_symbol = symbol = MSC_symbol (SYM_field, name, length, field);
        HSH_insert (symbol);
        field->fld_global = symbol = MSC_symbol (SYM_field, RFR.RDB$FIELD_SOURCE, 
		symbol_length (RFR.RDB$FIELD_SOURCE), field);
    END_FOR;
    }

return field;
}

NOD MET_fields (
    CTX		context)
{
/**************************************
 *
 *	M E T _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *    Return a list of the fields in a relation
 *
 **************************************/
DBB	dbb;
FLD	field;
LLS	stack;
NOD	node, field_node;
REF	reference;
PRC	procedure;
REL	relation;
TEXT	*p;
int	count;

if (procedure = context->ctx_procedure)
    {
    node = MAKE_NODE (nod_list, procedure->prc_out_count);
    count = 0;
    for (field = procedure->prc_outputs; field; field = field->fld_next)
	{
	reference = (REF) ALLOC (REF_LEN);
	reference->ref_field = field;
	reference->ref_context = context;
	field_node = MSC_unary (nod_field, reference);
	node->nod_arg [field->fld_position] = field_node;
	count++;
	}
    return node;
    }

relation = context->ctx_relation;
if (relation->rel_meta)
    {
    for (count = 0, field = relation->rel_fields; field; field = field->fld_next)
	count++;
    node = MAKE_NODE (nod_list, count);
    count = 0;
    for (field = relation->rel_fields; field; field = field->fld_next)
	{
	reference = (REF) ALLOC (REF_LEN);
	reference->ref_field = field;
	reference->ref_context = context;
	field_node = MSC_unary (nod_field, reference);
	node->nod_arg [field->fld_position] = field_node;
	count++;
	}
    return node;
    }

if (sw_language == lan_internal)
    return NULL;
    
dbb = relation->rel_database;
DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;

count = 0;
stack = NULL;
FOR (REQUEST_HANDLE dbb->dbb_flds_request) 
	RFR IN RDB$RELATION_FIELDS CROSS FLD IN RDB$FIELDS WITH
	RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
	RFR.RDB$RELATION_NAME EQ relation->rel_symbol->sym_string
	SORTED BY RFR.RDB$FIELD_POSITION
    for (p = RFR.RDB$FIELD_NAME; *p && *p != ' '; p++)
	;
    *p = 0;
    PUSH (MET_field (relation, RFR.RDB$FIELD_NAME), &stack);
    count++;
END_FOR;

node = MAKE_NODE (nod_list, count);

while (stack)
    {
    field = (FLD) POP (&stack);
    reference = (REF) ALLOC (REF_LEN);
    reference->ref_field = field;
    reference->ref_context = context;
    field_node = MSC_unary (nod_field, reference);
    node->nod_arg[--count] = field_node;
    }

return node;
}

void MET_fini (
    DBB		end)
{
/**************************************
 *
 *	M E T _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Shutdown all attached databases.
 *
 **************************************/
DBB	dbb;

for (dbb = isc_databases; dbb && dbb != end; dbb = dbb->dbb_next)
    if (DB = dbb->dbb_handle)
	{
	if (gds__trans = dbb->dbb_transaction)
	    COMMIT;
	FINISH;
	dbb->dbb_handle = NULL;
	dbb->dbb_transaction = NULL;

	dbb->dbb_field_request = NULL;
	dbb->dbb_flds_request = NULL;
	dbb->dbb_relation_request = NULL;
	dbb->dbb_procedure_request = NULL;
	dbb->dbb_udf_request = NULL;
	dbb->dbb_trigger_request = NULL;
	dbb->dbb_proc_prms_request = NULL;
	dbb->dbb_proc_prm_fld_request = NULL;
	dbb->dbb_index_request = NULL;
	dbb->dbb_type_request = NULL;
	dbb->dbb_array_request = NULL;
	dbb->dbb_dimension_request = NULL;
	dbb->dbb_domain_request = NULL;
	dbb->dbb_generator_request = NULL;
	dbb->dbb_view_request = NULL;
	dbb->dbb_primary_key_request = NULL;
	}
}

SCHAR *MET_generator (
    TEXT	*string,
    DBB		dbb)
{
/**************************************
 *
 *	M E T _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Lookup a generator by name.
 *	If found, return string. If not, return NULL.
 *
 **************************************/
SYM	symbol;
SCHAR   *gen_name = NULL;
SCHAR	name [NAME_SIZE];
SSHORT	length;

strcpy (name, string);

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_generator) && 
			(dbb == (DBB)(symbol->sym_object )))
	return symbol->sym_string;

return NULL;
}

USHORT MET_get_dtype (
    USHORT	blr_dtype,
    USHORT	sub_type,
    USHORT	*length)
{
/**************************************
 *
 *	M E T _ g e t _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Compute internal datatype and length based on system relation field values.
 *
 **************************************/
USHORT	l, dtype;

l = *length;

switch (blr_dtype)
    {
    case blr_varying:
    case blr_text:
	dtype = dtype_text; 
	if (sw_cstring && !SUBTYPE_ALLOWS_NULLS(sub_type))
	    {
	    ++l;
	    dtype = dtype_cstring;
	    }
	break;

    case blr_cstring:
	dtype = dtype_cstring;
	++l;
	break;

    case blr_short: 
	dtype = dtype_short; 
	l = sizeof (SSHORT);
	break;

    case blr_long: 
	dtype = dtype_long; 
	l = sizeof (SLONG);
	break;

    case blr_quad: 
	dtype = dtype_quad; 
	l = sizeof (GDS__QUAD);
	break;

    case blr_float: 
	dtype = dtype_real; 
	l = sizeof (float);
	break;

    case blr_double: 
	dtype = dtype_double; 
	l = sizeof (double);
	break;

    case blr_blob:
	dtype = dtype_blob;
        l = sizeof (GDS__QUAD);
	break;

/** Begin sql date/time/timestamp **/
    case blr_sql_date:
	dtype = dtype_sql_date;
	l = sizeof (ISC_DATE);
	break;

    case blr_sql_time:
	dtype = dtype_sql_time;
	l = sizeof (ISC_TIME);
	break;

    case blr_timestamp:
	dtype = dtype_timestamp;
	l = sizeof (ISC_TIMESTAMP);
	break;
/** Begin sql date/time/timestamp **/

    case blr_int64:
	dtype = dtype_int64;
	l = sizeof (ISC_INT64);
	break;

    default:
	    CPR_error ("datatype not supported");
    }

*length = l;

return dtype;
}

PRC MET_get_procedure (
    DBB		dbb,
    TEXT	*string,
    TEXT	*owner_name)
{
/**************************************
 *
 *	M E T _ g e t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Lookup a procedure (represented by a token) in a database.
 *	Return a procedure block (if name is found) or NULL.
 *
 *	This function has been cloned into MET_get_udf
 *
 **************************************/
SYM	symbol;
FLD	*fld_list, field;
PRC	procedure;
USHORT	length, type, count;
SCHAR	name[NAME_SIZE], owner [NAME_SIZE];

strcpy (name, string);
strcpy (owner, owner_name);
procedure = NULL;

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_procedure &&
	(procedure = (PRC) symbol->sym_object) &&
	procedure->prc_database == dbb &&
	(!owner [0] ||
	  (procedure->prc_owner && !strcmp (owner, procedure->prc_owner->sym_string))))
	break;

if (!procedure)
    return NULL;

if (procedure->prc_flags & PRC_scanned)
    return procedure;

FOR (REQUEST_HANDLE dbb->dbb_procedure_request)
  X IN RDB$PROCEDURES WITH X.RDB$PROCEDURE_ID = procedure->prc_id;

    for (type = 0; type < 2; type++)
	{
	count = 0;
	if (type)
	    fld_list = &procedure->prc_outputs;
	else
	    fld_list = &procedure->prc_inputs;

	FOR (REQUEST_HANDLE dbb->dbb_proc_prms_request)
	  Y IN RDB$PROCEDURE_PARAMETERS WITH 
	  Y.RDB$PROCEDURE_NAME EQ name AND 
	  Y.RDB$PARAMETER_TYPE EQ type 
	  SORTED BY DESCENDING Y.RDB$PARAMETER_NUMBER

	    count++;

	    FOR (REQUEST_HANDLE dbb->dbb_proc_prm_fld_request)
	      F IN RDB$FIELDS WITH 
	      Y.RDB$FIELD_SOURCE EQ F.RDB$FIELD_NAME

		field = (FLD) ALLOC (FLD_LEN);
		field->fld_procedure = procedure;
		field->fld_next = *fld_list;
		*fld_list = field;
		field->fld_position = Y.RDB$PARAMETER_NUMBER;
		field->fld_length = F.RDB$FIELD_LENGTH;
		field->fld_scale = F.RDB$FIELD_SCALE;
		field->fld_sub_type = F.RDB$FIELD_SUB_TYPE;
		field->fld_dtype = MET_get_dtype (F.RDB$FIELD_TYPE,
				F.RDB$FIELD_SUB_TYPE, &field->fld_length);
		switch (field->fld_dtype)
		    {
		    case dtype_text:
		    case dtype_cstring:
			field->fld_flags |= FLD_text;
			if (!F.RDB$CHARACTER_LENGTH.NULL)
			    field->fld_char_length = F.RDB$CHARACTER_LENGTH;
			if (!F.RDB$CHARACTER_SET_ID.NULL)
			    field->fld_charset_id = F.RDB$CHARACTER_SET_ID;
			if (!F.RDB$COLLATION_ID.NULL)
			    field->fld_collate_id = F.RDB$COLLATION_ID;
			field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id);
			break;

		    case dtype_blob:
			field->fld_flags |= FLD_blob;
			field->fld_seg_length = F.RDB$SEGMENT_LENGTH;
			if (!F.RDB$CHARACTER_SET_ID.NULL)
			    field->fld_charset_id = F.RDB$CHARACTER_SET_ID;
			break;
		    }
		field->fld_symbol = MSC_symbol (SYM_field,
			Y.RDB$PARAMETER_NAME, 
			symbol_length (Y.RDB$PARAMETER_NAME), field);
		/* If output parameter, insert in symbol table as a
		   field. */
		if (type)
    		    HSH_insert (field->fld_symbol);

	    END_FOR;

	END_FOR;

	if (type)
	    procedure->prc_out_count = count;
	else
	    procedure->prc_in_count = count;
	}

END_FOR;
procedure->prc_flags |= PRC_scanned;

/* Generate a dummy relation to point to the procedure to use when procedure
   is used as a view. */

return procedure;
}

REL MET_get_relation (
    DBB		dbb,
    TEXT	*string,
    TEXT	*owner_name)
{
/**************************************
 *
 *	M E T _ g e t _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Lookup a relation (represented by a token) in a database.
 *	Return a relation block (if name is found) or NULL.
 *
 **************************************/
SYM	symbol;
REL	relation;
SCHAR	name [NAME_SIZE], owner [NAME_SIZE];

strcpy (name, string);
strcpy (owner, owner_name);

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_relation &&
	(relation = (REL) symbol->sym_object) &&
	relation->rel_database == dbb &&
	(!owner [0] ||
	  (relation->rel_owner && !strcmp (owner, relation->rel_owner->sym_string))))
	return relation;

return NULL;
}

INTLSYM MET_get_text_subtype (
    SSHORT	ttype)
{
/**************************************
 *
 *	M E T _ g e t _ t e x t _ s u b t y p e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
INTLSYM		p;

for (p = text_subtypes; p; p = p->intlsym_next)
    if (p->intlsym_ttype == ttype)
	return p;

return NULL;
}

UDF MET_get_udf (
    DBB		dbb,
    TEXT	*string)
{
/**************************************
 *
 *	M E T _ g e t _ u d f
 *
 **************************************
 *
 * Functional description
 *	Lookup a udf (represented by a token) in a database.
 *	Return a udf block (if name is found) or NULL.
 *
 *	This function was cloned from MET_get_procedure
 *
 **************************************/
SYM	symbol;
FLD	field;
UDF	udf;
USHORT	length, count;
SCHAR	name[NAME_SIZE];

strcpy (name, string);
udf = NULL;
for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_udf &&
	(udf = (UDF) symbol->sym_object) &&
	udf->udf_database == dbb)
	break;
if (!udf)
    return NULL;

if (udf->udf_flags & UDF_scanned)
    return udf;

if (dbb->dbb_flags & DBB_v3)
    {
    /* Version of V4 request without new V4 metadata */
    FOR (REQUEST_HANDLE dbb->dbb_udf_request)
	UDF_DEF IN RDB$FUNCTIONS CROSS
	UDF_ARG IN RDB$FUNCTION_ARGUMENTS
	WITH UDF_DEF.RDB$FUNCTION_NAME   EQ name AND
	     UDF_DEF.RDB$FUNCTION_NAME   EQ UDF_ARG.RDB$FUNCTION_NAME AND
	     UDF_DEF.RDB$RETURN_ARGUMENT != UDF_ARG.RDB$ARGUMENT_POSITION
	SORTED BY DESCENDING UDF_ARG.RDB$ARGUMENT_POSITION; 

	    field = (FLD) ALLOC (FLD_LEN);
	    field->fld_next = udf->udf_inputs;
	    udf->udf_inputs = field;
	    udf->udf_args++;
	    field->fld_position = UDF_ARG.RDB$ARGUMENT_POSITION;
	    field->fld_length = UDF_ARG.RDB$FIELD_LENGTH;
	    field->fld_scale = UDF_ARG.RDB$FIELD_SCALE;
	    field->fld_sub_type = UDF_ARG.RDB$FIELD_SUB_TYPE;
	    field->fld_dtype = MET_get_dtype (UDF_ARG.RDB$FIELD_TYPE,
		    UDF_ARG.RDB$FIELD_SUB_TYPE, &field->fld_length);
	    switch (field->fld_dtype)
		{
		case dtype_text:
		case dtype_cstring:
		    field->fld_flags |= FLD_text;
		    break;

		case dtype_blob:
		    field->fld_flags |= FLD_blob;
		    break;
		}
    END_FOR;
    }
else
    {
    /* Same request as above, but with V4 metadata also fetched */
    FOR (REQUEST_HANDLE dbb->dbb_udf_request)
	UDF_DEF IN RDB$FUNCTIONS CROSS
	UDF_ARG IN RDB$FUNCTION_ARGUMENTS
	WITH UDF_DEF.RDB$FUNCTION_NAME   EQ name AND
	     UDF_DEF.RDB$FUNCTION_NAME   EQ UDF_ARG.RDB$FUNCTION_NAME AND
	     UDF_DEF.RDB$RETURN_ARGUMENT != UDF_ARG.RDB$ARGUMENT_POSITION
	SORTED BY DESCENDING UDF_ARG.RDB$ARGUMENT_POSITION; 

	    field = (FLD) ALLOC (FLD_LEN);
	    field->fld_next = udf->udf_inputs;
	    udf->udf_inputs = field;
	    udf->udf_args++;
	    field->fld_position = UDF_ARG.RDB$ARGUMENT_POSITION;
	    field->fld_length = UDF_ARG.RDB$FIELD_LENGTH;
	    field->fld_scale = UDF_ARG.RDB$FIELD_SCALE;
	    field->fld_sub_type = UDF_ARG.RDB$FIELD_SUB_TYPE;
	    field->fld_dtype = MET_get_dtype (UDF_ARG.RDB$FIELD_TYPE,
		    UDF_ARG.RDB$FIELD_SUB_TYPE, &field->fld_length);
	    switch (field->fld_dtype)
		{
		case dtype_text:
		case dtype_cstring:
		    field->fld_flags |= FLD_text;
		    if (!UDF_ARG.RDB$CHARACTER_SET_ID.NULL)
			field->fld_charset_id = UDF_ARG.RDB$CHARACTER_SET_ID;
		    field->fld_ttype = INTL_CS_COLL_TO_TTYPE (field->fld_charset_id, field->fld_collate_id);
		    break;

		case dtype_blob:
		    field->fld_flags |= FLD_blob;
		    if (!UDF_ARG.RDB$CHARACTER_SET_ID.NULL)
			field->fld_charset_id = UDF_ARG.RDB$CHARACTER_SET_ID;
		    break;
		}
    END_FOR;
    }

udf->udf_flags |= UDF_scanned;

return udf;
}

REL MET_get_view_relation (
    REQ		request,
    UCHAR	*view_name,
    UCHAR	*relation_or_alias,
    USHORT	level)
{
/**************************************
 *
 *	M E T _ g e t _ v i e w _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Return TRUE if the passed view_name represents a 
 *	view with the passed relation as a base table 
 *	(the relation could be an alias).
 *
 **************************************/
DBB	dbb;
TEXT	*p;
REL	relation;

dbb = request->req_database;
DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;

relation = NULL;

FOR (REQUEST_HANDLE dbb->dbb_view_request, 
     LEVEL level)
    X IN RDB$VIEW_RELATIONS WITH
    X.RDB$VIEW_NAME EQ view_name

    for (p = X.RDB$CONTEXT_NAME; *p && *p != ' '; p++)
	;
    *p = 0;

    for (p = X.RDB$RELATION_NAME; *p && *p != ' '; p++)
	;
    *p = 0;

    if (!strcmp (X.RDB$RELATION_NAME, relation_or_alias) ||
	!strcmp (X.RDB$CONTEXT_NAME, relation_or_alias))
	return MET_get_relation (dbb, X.RDB$RELATION_NAME, "");

    if (relation = MET_get_view_relation (request, X.RDB$RELATION_NAME, relation_or_alias, level + 1))
	return relation;

END_FOR;

return NULL;
}

IND MET_index (
    DBB 	dbb,
    TEXT 	*string)
{
/**************************************
 *
 *	M E T _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Lookup an index for a database.
 *	Return an index block (if name is found) or NULL.
 *
 **************************************/
SYM	symbol;
IND	index;
SCHAR	name [NAME_SIZE];
SSHORT	length;

strcpy (name, string);
length = strlen(name);

for (symbol = HSH_lookup (name); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_index &&
	(index = (IND) symbol->sym_object) &&
	index->ind_relation->rel_database == dbb)
	return index;

if (sw_language == lan_internal)
    return NULL;
    
if (dbb->dbb_flags & DBB_sqlca) 
    return NULL;

DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;
index = NULL;

FOR (REQUEST_HANDLE dbb->dbb_index_request)
     X IN RDB$INDICES WITH X.RDB$INDEX_NAME EQ name

    index = (IND) ALLOC (IND_LEN);
    index->ind_symbol = symbol = MSC_symbol (SYM_index, name, length, index);
    index->ind_relation = MET_get_relation (dbb, X.RDB$RELATION_NAME, "");
    HSH_insert (symbol);

END_FOR;

return index;
}

void MET_load_hash_table (
    DBB		dbb)
{
/**************************************
 *
 *	M E T _ l o a d _ h a s h _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Load all of the relation names
 *      and user defined function names
 *      into the symbol (hash) table.
 *
 **************************************/
REL	relation;
PRC	procedure;
SYM	symbol;
FLD	dbkey;
UDF	udf;
TEXT	*p;
int	*handle, *handle2;
USHORT	post_v3_flag;
SLONG	length;
INTLSYM	iname;

/* If this is an internal ISC access method invocation, don't do any of this
   stuff */

if (sw_language == lan_internal)
    return;

if (!dbb->dbb_handle)
    if (!MET_database (dbb, FALSE))
	CPR_exit (FINI_ERROR);

if (dbb->dbb_transaction)
    /* we must have already loaded this one */
    return;

gds__trans = NULL;
DB = dbb->dbb_handle;

START_TRANSACTION;

dbb->dbb_transaction = gds__trans;
handle = handle2 = NULL;

/* Determine if the database is V3. */

post_v3_flag = FALSE;
handle = NULL;
FOR (REQUEST_HANDLE handle) 
    X IN RDB$RELATIONS WITH
    X.RDB$RELATION_NAME = 'RDB$PROCEDURES' AND 
    X.RDB$SYSTEM_FLAG = 1

    post_v3_flag = TRUE;

END_FOR;
gds__release_request (gds__status, GDS_REF (handle));

if (!post_v3_flag)
    dbb->dbb_flags |= DBB_v3;

/* Pick up all relations (necessary to parse parts of the GDML grammar) */

if (dbb->dbb_flags & DBB_v3)
    {
    FOR (REQUEST_HANDLE handle)
      X IN RDB$RELATIONS 

        relation = (REL) ALLOC (REL_LEN);
        relation->rel_database = dbb;
        relation->rel_next = dbb->dbb_relations;
        dbb->dbb_relations = relation;
        relation->rel_id = X.RDB$RELATION_ID;
        relation->rel_symbol = symbol = MSC_symbol (SYM_relation, X.RDB$RELATION_NAME, 
		symbol_length (X.RDB$RELATION_NAME), relation);
        HSH_insert (symbol);
        relation->rel_dbkey = dbkey = MET_make_field ("rdb$db_key", dtype_text, 
    	    (X.RDB$DBKEY_LENGTH) ? X.RDB$DBKEY_LENGTH : 8, FALSE);
        dbkey->fld_flags |= FLD_dbkey | FLD_text | FLD_charset;
        dbkey->fld_ttype = ttype_binary;

    END_FOR;
    }
else
    {
    FOR (REQUEST_HANDLE handle)
      X IN RDB$RELATIONS 

        relation = (REL) ALLOC (REL_LEN);
        relation->rel_database = dbb;
        relation->rel_next = dbb->dbb_relations;
        dbb->dbb_relations = relation;
        relation->rel_id = X.RDB$RELATION_ID;
        relation->rel_symbol = symbol = MSC_symbol (SYM_relation, X.RDB$RELATION_NAME, 
		symbol_length (X.RDB$RELATION_NAME), relation);
        HSH_insert (symbol);
        relation->rel_dbkey = dbkey = MET_make_field ("rdb$db_key", dtype_text, 
    	    (X.RDB$DBKEY_LENGTH) ? X.RDB$DBKEY_LENGTH : 8, FALSE);
        dbkey->fld_flags |= FLD_dbkey | FLD_text | FLD_charset;
        dbkey->fld_ttype = ttype_binary;

	if (!X.RDB$OWNER_NAME.NULL && (length = symbol_length (X.RDB$OWNER_NAME)))
	    relation->rel_owner = MSC_symbol (SYM_username, X.RDB$OWNER_NAME, length, NULL_PTR);

    END_FOR;
    }

gds__release_request (gds__status, GDS_REF (handle));

/* Pick up all procedures (necessary to parse parts of the GDML grammar) */

FOR (REQUEST_HANDLE handle)
    X IN RDB$PROCEDURES

    procedure = (PRC) ALLOC (REL_LEN);
    procedure->prc_database = dbb;
    procedure->prc_next = (PRC) dbb->dbb_procedures;
    dbb->dbb_procedures = (REL) procedure;
    procedure->prc_id = X.RDB$PROCEDURE_ID;
    procedure->prc_symbol = symbol = MSC_symbol (SYM_procedure, X.RDB$PROCEDURE_NAME, 
		symbol_length (X.RDB$PROCEDURE_NAME), procedure);
    HSH_insert (symbol);
    if (!X.RDB$OWNER_NAME.NULL && (length = symbol_length (X.RDB$OWNER_NAME)))
	procedure->prc_owner = MSC_symbol (SYM_username, X.RDB$OWNER_NAME, length, NULL_PTR);
END_FOR
    ON_ERROR
    /* assume pre V4 database, no procedures */
    END_ERROR;

if (handle)
    gds__release_request (gds__status, GDS_REF (handle));

/* Pickup any user defined functions.  If the database does not support UDF's,
   this may fail */

FOR (REQUEST_HANDLE handle)
     FUN IN RDB$FUNCTIONS CROSS ARG IN RDB$FUNCTION_ARGUMENTS WITH
	FUN.RDB$FUNCTION_NAME EQ ARG.RDB$FUNCTION_NAME AND
	FUN.RDB$RETURN_ARGUMENT EQ ARG.RDB$ARGUMENT_POSITION

    p = FUN.RDB$FUNCTION_NAME;
    length = symbol_length (p);
    p [length] = 0;

    udf = (UDF) ALLOC (UDF_LEN + length);
    strcpy (udf->udf_function, p);
    udf->udf_database = dbb;
    udf->udf_type = FUN.RDB$FUNCTION_TYPE;

    if (length = symbol_length (FUN.RDB$QUERY_NAME))
	{
	p = FUN.RDB$QUERY_NAME;
	p [length] = 0;
	}
    udf->udf_symbol = symbol = MSC_symbol (SYM_udf, p, strlen (p), udf);
    HSH_insert (symbol);

    udf->udf_length = ARG.RDB$FIELD_LENGTH;
    udf->udf_scale = ARG.RDB$FIELD_SCALE;
    udf->udf_sub_type = ARG.RDB$FIELD_SUB_TYPE;
    udf->udf_dtype = MET_get_dtype (ARG.RDB$FIELD_TYPE, ARG.RDB$FIELD_SUB_TYPE, &udf->udf_length);

    if (post_v3_flag)
	{
	FOR (REQUEST_HANDLE handle2)
	    V4ARG IN RDB$FUNCTION_ARGUMENTS 
	    CROSS CS IN RDB$CHARACTER_SETS 
	    CROSS COLL IN RDB$COLLATIONS WITH
	    V4ARG.RDB$CHARACTER_SET_ID EQ CS.RDB$CHARACTER_SET_ID AND
	    COLL.RDB$COLLATION_NAME EQ CS.RDB$DEFAULT_COLLATE_NAME AND
	    V4ARG.RDB$CHARACTER_SET_ID NOT MISSING AND
	    V4ARG.RDB$FUNCTION_NAME EQ ARG.RDB$FUNCTION_NAME AND
	    V4ARG.RDB$ARGUMENT_POSITION EQ ARG.RDB$ARGUMENT_POSITION;

	    udf->udf_charset_id = V4ARG.RDB$CHARACTER_SET_ID;
	    udf->udf_ttype = INTL_CS_COLL_TO_TTYPE (udf->udf_charset_id, COLL.RDB$COLLATION_ID);

	END_FOR;

	}
END_FOR
    ON_ERROR
    END_ERROR;

gds__release_request (gds__status, GDS_REF (handle));
if (handle2)
    gds__release_request (gds__status, GDS_REF (handle2));

/* Pick up all Collation names, might have several collations
 * for a given character set.
 * There can also be several alias names for a character set.
 */

FOR (REQUEST_HANDLE handle)
     CHARSET IN RDB$CHARACTER_SETS CROSS COLL IN RDB$COLLATIONS OVER RDB$CHARACTER_SET_ID;

    p = COLL.RDB$COLLATION_NAME;
    length = symbol_length (p);
    p [length] = 0;
    iname = (INTLSYM) ALLOC (INTLSYM_LEN + length);
    strcpy (iname->intlsym_name, p);
    iname->intlsym_database = dbb;
    iname->intlsym_symbol = symbol = MSC_symbol (SYM_collate, p, strlen (p), iname);
    HSH_insert (symbol);
    iname->intlsym_type       = INTLSYM_collation;
    iname->intlsym_flags      = 0;
    iname->intlsym_charset_id = COLL.RDB$CHARACTER_SET_ID;
    iname->intlsym_collate_id = COLL.RDB$COLLATION_ID;
    iname->intlsym_ttype      = INTL_CS_COLL_TO_TTYPE (iname->intlsym_charset_id, iname->intlsym_collate_id);
    iname->intlsym_bytes_per_char = 
			(CHARSET.RDB$BYTES_PER_CHARACTER.NULL)	      ? 1 :
			(CHARSET.RDB$BYTES_PER_CHARACTER);
    iname->intlsym_next = text_subtypes;
    text_subtypes = iname;

    FOR (REQUEST_HANDLE handle2)
	TYPE IN RDB$TYPES 
	WITH TYPE.RDB$FIELD_NAME = "RDB$COLLATION_NAME"
	 AND TYPE.RDB$TYPE       = COLL.RDB$COLLATION_ID
	 AND TYPE.RDB$TYPE_NAME != COLL.RDB$COLLATION_NAME;

	p = TYPE.RDB$TYPE_NAME;
	length = symbol_length (p);
	p [length] = 0;
	symbol = MSC_symbol (SYM_collate, p, length, iname);
	HSH_insert (symbol);
    END_FOR
	ON_ERROR
	END_ERROR;

END_FOR
    ON_ERROR
    /* assume pre V4 database, no collations */
    END_ERROR;

gds__release_request (gds__status, GDS_REF (handle));
if (handle2)
    gds__release_request (gds__status, GDS_REF (handle2));

/* Now pick up all character set names - with the subtype set to
 * the type of the default collation for the character set.
 */
FOR (REQUEST_HANDLE handle)
     CHARSET IN RDB$CHARACTER_SETS CROSS COLL IN RDB$COLLATIONS 
     WITH CHARSET.RDB$DEFAULT_COLLATE_NAME EQ COLL.RDB$COLLATION_NAME;

    p = CHARSET.RDB$CHARACTER_SET_NAME;
    length = symbol_length (p);
    p [length] = 0;
    iname = (INTLSYM) ALLOC (INTLSYM_LEN + length);
    strcpy (iname->intlsym_name, p);
    iname->intlsym_database = dbb;
    iname->intlsym_symbol = symbol = MSC_symbol (SYM_charset, p, strlen (p), iname);
    HSH_insert (symbol);
    iname->intlsym_type       = INTLSYM_collation;
    iname->intlsym_flags      = 0;
    iname->intlsym_charset_id = COLL.RDB$CHARACTER_SET_ID;
    iname->intlsym_collate_id = COLL.RDB$COLLATION_ID;
    iname->intlsym_ttype      = INTL_CS_COLL_TO_TTYPE (iname->intlsym_charset_id, iname->intlsym_collate_id);
    iname->intlsym_bytes_per_char = (CHARSET.RDB$BYTES_PER_CHARACTER.NULL) ?
	1 : (CHARSET.RDB$BYTES_PER_CHARACTER);

    FOR (REQUEST_HANDLE handle2)
	TYPE IN RDB$TYPES 
	WITH TYPE.RDB$FIELD_NAME = "RDB$CHARACTER_SET_NAME"
	 AND TYPE.RDB$TYPE       = CHARSET.RDB$CHARACTER_SET_ID
	 AND TYPE.RDB$TYPE_NAME != CHARSET.RDB$CHARACTER_SET_NAME;

	p = TYPE.RDB$TYPE_NAME;
	length = symbol_length (p);
	p [length] = 0;
	symbol = MSC_symbol (SYM_charset, p, length, iname);
	HSH_insert (symbol);
    END_FOR
	ON_ERROR
	END_ERROR;

END_FOR
    ON_ERROR
    /* assume pre V4 database, no character sets */
    END_ERROR;

gds__release_request (gds__status, GDS_REF (handle));
if (handle2)
    gds__release_request (gds__status, GDS_REF (handle2));

/* Pick up name of database default character set for SQL */

FOR (REQUEST_HANDLE handle)
    FIRST 1 DBB IN RDB$DATABASE 
    WITH DBB.RDB$CHARACTER_SET_NAME NOT MISSING
	p = DBB.RDB$CHARACTER_SET_NAME;
	length = symbol_length (p);
	p [length] = 0;
	dbb->dbb_def_charset = (TEXT *) ALLOC (length+1);
	strcpy (dbb->dbb_def_charset, p);
        if (!MSC_find_symbol (HSH_lookup (dbb->dbb_def_charset), SYM_charset))
	    CPR_warn ("Default character set for database is not known");
END_FOR
    ON_ERROR
    /* Assume V3 Db, no default charset */
    END_ERROR;

gds__release_request (gds__status, GDS_REF (handle));

/* Pick up all generators for the database */

FOR (REQUEST_HANDLE handle)
     X IN RDB$GENERATORS

    symbol = MSC_symbol (SYM_generator, X.RDB$GENERATOR_NAME,
		symbol_length (X.RDB$GENERATOR_NAME), dbb);
    HSH_insert (symbol);

END_FOR
    ON_ERROR
    END_ERROR;

gds__release_request (gds__status, GDS_REF (handle));

/* now that we have attached to the database, resolve the character set
 * request (if any) (and if we can)
 */

if (dbb->dbb_c_lc_ctype)
    {
    if (get_intl_char_subtype (&dbb->dbb_char_subtype, dbb->dbb_c_lc_ctype, strlen (dbb->dbb_c_lc_ctype), dbb))
	dbb->dbb_know_subtype = 1;
    else
	{
	TEXT	buffer [200];

	sprintf (buffer, "Cannot recognize character set '%s'", dbb->dbb_c_lc_ctype);
	PAR_error (buffer);
	}
    sw_know_interp = dbb->dbb_know_subtype;
    sw_interp = dbb->dbb_char_subtype;
    }
}

FLD MET_make_field (
    SCHAR	*name,
    SSHORT	dtype,
    SSHORT	length,
    BOOLEAN	insert_flag)
{
/**************************************
 *
 *	M E T _ m a k e _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Make a field symbol.
 *
 **************************************/
FLD	field;
SYM     symbol;

field = (FLD) ALLOC (FLD_LEN);
field->fld_length = length;
field->fld_dtype = dtype;
field->fld_symbol = symbol = MSC_symbol (SYM_field, name, strlen (name), field);
if (insert_flag)
    HSH_insert (symbol);

return field;
}

IND MET_make_index (
    SCHAR	*name)
{
/**************************************
 *
 *	M E T _ m a k e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Make an index symbol.
 *
 **************************************/
IND	index;

index = (IND) ALLOC (IND_LEN);
index->ind_symbol = MSC_symbol (SYM_index, name, strlen (name), index);

return index;
}

REL MET_make_relation (
    SCHAR	*name)
{
/**************************************
 *
 *	M E T _ m a k e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Make an relation symbol.
 *
 **************************************/
REL	relation;

relation = (REL) ALLOC (REL_LEN);
relation->rel_symbol = MSC_symbol (SYM_relation, name, strlen (name), relation);

return relation;
}

BOOLEAN MET_type (
    FLD		field,
    TEXT	*string,
    SSHORT	*ptr)
{
/**************************************
 *
 *	M E T _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Lookup a type name for a field.
 *
 **************************************/
REL	relation;
DBB	dbb;
SYM	symbol;
TYP	type;
UCHAR	buffer [32];	/* BASED ON RDB$TYPES.RDB$TYPE_NAME */
UCHAR	*p;

for (symbol = HSH_lookup (string); symbol; symbol = symbol->sym_homonym)
    if (symbol->sym_type == SYM_type &&
	(type = (TYP) symbol->sym_object) &&
	(!type->typ_field || type->typ_field == field))
	{
	*ptr = type->typ_value;
	return TRUE;
	}

relation = field->fld_relation;
dbb = relation->rel_database;
DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;

/* Force the name to uppercase, using C locale rules for uppercasing */
for (p = buffer; *string && p < &buffer [sizeof(buffer)-1]; p++, string++)
    *p = UPPER7 (*string);
*p = '\0';

FOR (REQUEST_HANDLE dbb->dbb_type_request)
    X IN RDB$TYPES WITH X.RDB$FIELD_NAME EQ field->fld_global->sym_string AND
			X.RDB$TYPE_NAME EQ buffer
    {
    type = (TYP) ALLOC (TYP_LEN);
    type->typ_field = field;
    *ptr = type->typ_value = X.RDB$TYPE;
    type->typ_symbol = symbol = MSC_symbol (SYM_type, string, strlen (string), type);
    HSH_insert (symbol);
    return TRUE;
    }
END_FOR
    ON_ERROR
    END_ERROR;

return FALSE;
}

BOOLEAN MET_trigger_exists (
    DBB 	dbb,
    TEXT 	*trigger_name)
{
/**************************************
 *
 *	M E T _ t r i g g e r _ e x i s t s
 *
 **************************************
 *
 * Functional description
 *	Lookup an index for a database.
 *
 * Return: TRUE if the trigger exists
 *	   FALSE otherwise
 *
 **************************************/
SCHAR	name [NAME_SIZE];
SSHORT	length;

strcpy (name, trigger_name);

DB = dbb->dbb_handle;
gds__trans = dbb->dbb_transaction;

FOR (REQUEST_HANDLE dbb->dbb_trigger_request)
     TRIG IN RDB$TRIGGERS WITH TRIG.RDB$TRIGGER_NAME EQ name

    return TRUE;

END_FOR;

return FALSE;
}

static SLONG array_size (
    FLD		field)
{
/**************************************
 *
 *	a r r a y _ s i z e
 *
 **************************************
 *
 * Functional description
 *	Compute and return the size of the array.
 *
 **************************************/
ARY     array_block;
DIM     dimension;
SLONG    count;

array_block = field->fld_array_info;
count = field->fld_array->fld_length;
for (dimension = array_block->ary_dimension; dimension; dimension = dimension->dim_next)
    count = count * (dimension->dim_upper - dimension->dim_lower + 1);

return count;
}

static void get_array (
    DBB		dbb,
    TEXT	*field_name,
    FLD		field)
{
/**************************************
 *
 *	g e t _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	See if field is array.
 *
 **************************************/
FLD	sub_field;
ARY     array_block;
DIM     dimension_block, last_dimension_block;

array_block = NULL;

FOR (REQUEST_HANDLE dbb->dbb_array_request) 
    F IN RDB$FIELDS WITH F.RDB$FIELD_NAME EQ field_name
    if (F.RDB$DIMENSIONS)
	{
	sub_field = (FLD) ALLOC (FLD_LEN);
	*sub_field = *field;
	field->fld_array = sub_field;
	field->fld_dtype = dtype_blob;
	field->fld_flags |= FLD_blob;
	field->fld_length = 8;
	field->fld_array_info = array_block = (ARY) ALLOC (ARY_LEN (F.RDB$DIMENSIONS));
	array_block->ary_dtype = sub_field->fld_dtype;
	}
END_FOR
    ON_ERROR
	{
	dbb->dbb_flags |= DBB_no_arrays;
	return;
	}
    END_ERROR;

if (!array_block)
    return;

FOR (REQUEST_HANDLE dbb->dbb_dimension_request)
    D IN RDB$FIELD_DIMENSIONS WITH D.RDB$FIELD_NAME EQ field_name
	SORTED BY ASCENDING D.RDB$DIMENSION
	array_block->ary_rpt [D.RDB$DIMENSION].ary_lower = D.RDB$LOWER_BOUND;
	array_block->ary_rpt [D.RDB$DIMENSION].ary_upper = D.RDB$UPPER_BOUND;
	dimension_block = (DIM) ALLOC (DIM_LEN);
	dimension_block->dim_number = D.RDB$DIMENSION;
	dimension_block->dim_lower = D.RDB$LOWER_BOUND;
	dimension_block->dim_upper = D.RDB$UPPER_BOUND;

	if (D.RDB$DIMENSION != 0)
	    {
	    last_dimension_block->dim_next = dimension_block;
	    dimension_block->dim_previous = last_dimension_block;
	    }
	else
	    array_block->ary_dimension = dimension_block;
	last_dimension_block = dimension_block;
END_FOR;

array_block->ary_dimension_count = last_dimension_block->dim_number + 1;
array_block->ary_size = array_size (field);
}

static int get_intl_char_subtype (
    SSHORT	*id,
    UCHAR	*name,
    USHORT	length,
    DBB		dbb)
{
/**************************************
 *
 *	g e t _ i n t l _ c h a r _ s u b t y p e 
 *
 **************************************
 *
 * Functional description
 *	Character types can be specified as either:
 *	   b) A POSIX style locale name "<collation>.<characterset>"
 *	   or
 *	   c) A simple <characterset> name (using default collation)
 *	   d) A simple <collation> name    (use charset for collation)
 *
 *	Given an ASCII7 string which could be any of the above, try to
 *	resolve the name in the order b, c, d.
 *	b) is only tried iff the name contains a period.
 *	(in which case c) and d) are not tried).
 *
 * Return:
 *	1 if no errors (and *id is set).
 *	0 if the name could not be resolved.
 *
 **************************************/
UCHAR	buffer [32];		/* BASED ON RDB$COLLATION_NAME */
UCHAR	*p;
UCHAR	*period = NULL;
UCHAR	*end_name;
int	found_it = 0;

assert (id != NULL);
assert (name != NULL);
assert (dbb != NULL);


DB = dbb->dbb_handle; 
if (!DB)
    return (0);
gds__trans = dbb->dbb_transaction;

end_name = name + length;
/* Force key to uppercase, following C locale rules for uppercasing */
/* At the same time, search for the first period in the string (if any) */
for  (p = (UCHAR *) buffer; 
      name < end_name && p < (UCHAR *)(buffer+sizeof(buffer)-1); 
      p++, name++)
    {
    *p = UPPER7 (*name);
    if ((*p == '.') && !period)
	period = p;
    };
*p = 0;

/* Is there a period, separating collation name from character set? */
if (period)
    {
    *period = 0;
    return (resolve_charset_and_collation (id, period+1, buffer));
    }

else
    {
    int	res;

    /* Is it a character set name (implying charset default collation sequence) */
    res = resolve_charset_and_collation (id, buffer, NULL);
    if (!res)
	{
	/* Is it a collation name (implying implementation-default character set) */
	res = resolve_charset_and_collation (id, NULL, buffer);
	}
    return (res);
    }
}

static int resolve_charset_and_collation (
    SSHORT	*id,
    UCHAR	*charset,
    UCHAR	*collation)
{
/**************************************
 *
 *	r e s o l v e _ c h a r s e t _ a n d _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Given ASCII7 name of charset & collation
 *	resolve the specification to a ttype (id) that implements
 *	it.
 *
 * Inputs:
 *	(charset) 
 *		ASCII7z name of characterset.
 *		NULL (implying unspecified) means use the character set
 *	        for defined for (collation).
 *
 *	(collation)
 *		ASCII7z name of collation.
 *		NULL means use the default collation for (charset).
 *
 * Outputs:
 *	(*id)	
 *		Set to subtype specified by this name.
 *
 * Return:
 *	1 if no errors (and *id is set).
 *	0 if either name not found.
 *	  or if names found, but the collation isn't for the specified
 *	  character set.
 *
 **************************************/
int	found = 0;
int	*request = NULL;

assert (id != NULL);

if (!DB)
    return (0);

if (collation == NULL)
    {
    if (charset == NULL)
	{
	FOR (REQUEST_HANDLE request)
	  FIRST 1 DBB IN RDB$DATABASE 
	  WITH DBB.RDB$CHARACTER_SET_NAME NOT MISSING
	   AND DBB.RDB$CHARACTER_SET_NAME != " ";

	    charset = (UCHAR *) DBB.RDB$CHARACTER_SET_NAME;

	END_FOR
	    ON_ERROR
	    /* Assume V3 DB, without default character set */
	    END_ERROR;
    
	gds__release_request (gds__status, GDS_REF (request));

	if (charset == NULL)
	    charset = (UCHAR *) DEFAULT_CHARACTER_SET_NAME;

	}

    FOR (REQUEST_HANDLE request) 
      FIRST 1   CS IN RDB$CHARACTER_SETS 
	CROSS COLL IN RDB$COLLATIONS 
	CROSS TYPE IN RDB$TYPES
	WITH TYPE.RDB$TYPE_NAME          EQ charset
	 AND TYPE.RDB$FIELD_NAME         EQ "RDB$CHARACTER_SET_NAME"
	 AND TYPE.RDB$TYPE               EQ CS.RDB$CHARACTER_SET_ID
	 AND CS.RDB$DEFAULT_COLLATE_NAME EQ COLL.RDB$COLLATION_NAME;

		found++;
		*id = MAP_CHARSET_TO_TTYPE (CS.RDB$CHARACTER_SET_ID);

    END_FOR
	ON_ERROR
	END_ERROR;

    gds__release_request (gds__status, GDS_REF (request));

    return (found);
    }
else if (charset == NULL)
    {
    FOR (REQUEST_HANDLE request) 
      FIRST 1   CS IN RDB$CHARACTER_SETS 
	CROSS COLL IN RDB$COLLATIONS 
	OVER RDB$CHARACTER_SET_ID
	WITH COLL.RDB$COLLATION_NAME     EQ collation;

	found++;
	*id = MAP_CHARSET_TO_TTYPE (CS.RDB$CHARACTER_SET_ID);
    END_FOR
	ON_ERROR
	END_ERROR;

    gds__release_request (gds__status, GDS_REF (request));

    return (found);
    }

FOR (REQUEST_HANDLE request) 
    FIRST 1   CS    IN RDB$CHARACTER_SETS 
	CROSS COL   IN RDB$COLLATIONS     OVER   RDB$CHARACTER_SET_ID
	CROSS NAME2 IN RDB$TYPES
	WITH COL.RDB$COLLATION_NAME       EQ collation
	 AND NAME2.RDB$TYPE_NAME          EQ charset
	 AND NAME2.RDB$FIELD_NAME         EQ "RDB$CHARACTER_SET_NAME"
	 AND NAME2.RDB$TYPE               EQ CS.RDB$CHARACTER_SET_ID;

	found++;
	*id = MAP_CHARSET_TO_TTYPE (CS.RDB$CHARACTER_SET_ID);
END_FOR
    ON_ERROR
    END_ERROR;

gds__release_request (gds__status, GDS_REF (request));

return (found);
}

static symbol_length (
    TEXT	*string)
{
/**************************************
 *
 *	s y m b o l _ l e n g t h
 *
 **************************************
 *
 * Functional description
 *	Compute significant length of symbol.
 *
 **************************************/
TEXT	*p;
int len;

len = strlen (string);

p = string + (len-1);

for (; p >= string && *p == ' '; p--)
    ;
if (p < string) 
    return 0;
++p;
return p - string;
}

static upcase (
    TEXT	*from,
    TEXT	*to)
{
/**************************************
 *
 *	u p c a s e
 *
 **************************************
 *
 * Functional description
 *	Upcase a string into another string.  Return
 *	length of string.
 *
 **************************************/
TEXT	*p, *end, c; 

p = to;
end = to + NAME_SIZE;

while (p < end && (c = *from++))
    {
    *p++ = UPPER (c);
#ifdef JPN_SJIS
    /* Do not upcase second byte of a sjis kanji character */

    if (SJIS1(c) && p < end && (c = *from++))
	*p++ = c;
#endif
    }

*p = 0;

return p - to;
}
