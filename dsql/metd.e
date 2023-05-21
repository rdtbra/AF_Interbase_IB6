/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		met.e
 *	DESCRIPTION:	Meta-data interface
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
#include "../dsql/dsql.h"
#include "../dsql/node.h"
#include "../dsql/sym.h"
#include "../jrd/gds.h"
#include "../jrd/align.h"
#include "../jrd/intl.h"
#include "../jrd/thd.h"
#include "../dsql/alld_proto.h"
#include "../dsql/ddl_proto.h" 
#include "../dsql/metd_proto.h"
#include "../dsql/hsh_proto.h" 
#include "../dsql/make_proto.h" 
#include "../jrd/gds_proto.h" 
#include "../jrd/sch_proto.h" 
#include "../jrd/thd_proto.h"
#include "../jrd/constants.h"

/* NOTE: The static definition of DB and gds__trans by gpre will not
         be used by the meta data routines.  Each of those routines has
         its own local definition of these variables. */

/**************************************************************
V4 Multi-threading changes.

-- direct calls to gds__ () & isc_ () entrypoints

	THREAD_EXIT;
	    gds__ () or isc_ () call.
	THREAD_ENTER;

-- calls through embedded GDML.

the following protocol will be used.  Care should be taken if
nested FOR loops are added.

    THREAD_EXIT;                // last statment before FOR loop 

    FOR ...............

	THREAD_ENTER;           // First statment in FOR loop 
	.....some C code....
	.....some C code....
	THREAD_EXIT;            // last statment in FOR loop 

    END_FOR;

    THREAD_ENTER;               // First statment after FOR loop 
***************************************************************/

ASSERT_FILENAME

DATABASE
    DB = STATIC "yachts.lnk";

static CONST UCHAR    blr_bpb [] =
                   { isc_bpb_version1,
                     isc_bpb_source_type, 1, BLOB_blr,
                     isc_bpb_target_type, 1, BLOB_blr};

static void	check_array (REQ, SCHAR *, FLD);
static void	convert_dtype (FLD, SSHORT);
static void	free_procedure (PRC);
static void	free_relation (DSQL_REL);
static TEXT	*metd_exact_name (TEXT *);

#ifdef  SUPERSERVER
static  REC_MUTX_T      rec_mutex; /* Recursive metadata mutex */
static  USHORT          rec_mutex_inited = 0;

#define METD_REC_LOCK   if ( !rec_mutex_inited)\
                        {\
                            rec_mutex_inited = 1;\
                            THD_rec_mutex_init (&rec_mutex);\
                        }\
                        THREAD_EXIT;\
                        THD_rec_mutex_lock (&rec_mutex);\
                        THREAD_ENTER;
#define METD_REC_UNLOCK THD_rec_mutex_unlock (&rec_mutex)
#else
#define METD_REC_LOCK
#define METD_REC_UNLOCK
#endif

void METD_drop_procedure (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ d r o p _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Drop a procedure from our metadata, and
 *	the next caller who wants it will 
 *	look up the new version. 
 *	
 *	Dropping will be achieved by marking the procedure
 *	as dropped.  Anyone with current access can continue
 *	accessing it.
 *
 **************************************/
PRC	procedure;
SYM	symbol;

METD_REC_LOCK;

/* If the symbol wasn't defined, we've got nothing to do */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
		name->str_length, SYM_procedure, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_procedure) &&
	    ((procedure = (PRC) symbol->sym_object) && 
	    (!(procedure->prc_flags & PRC_dropped))))
	break; 

if (symbol)
    {
    procedure = (PRC) symbol->sym_object;
    procedure->prc_flags |= PRC_dropped;
    }

/* mark other potential candidates as maybe dropped */

HSHD_set_flag( request->req_dbb, name->str_data, name->str_length,
               SYM_procedure, PRC_dropped);

METD_REC_UNLOCK;
}

void METD_drop_relation (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ d r o p _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Drop a relation from our metadata, and
 *	rely on the next guy who wants it to
 *	look up the new version. 
 *
 *      Dropping will be achieved by marking the relation
 *      as dropped.  Anyone with current access can continue
 *      accessing it.
 *
 **************************************/
DSQL_REL	relation;
SYM	symbol;


METD_REC_LOCK;

/* If the symbol wasn't defined, we've got nothing to do */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_relation, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_relation) &&
	    ((relation = (DSQL_REL) symbol->sym_object) &&
	    (!(relation->rel_flags & REL_dropped))))
	break; 

if (symbol)
    {
    relation = (DSQL_REL) symbol->sym_object;
    relation->rel_flags |= REL_dropped;
    }

/* mark other potential candidates as maybe dropped */

HSHD_set_flag( request->req_dbb, name->str_data, name->str_length,
               SYM_relation, REL_dropped);

METD_REC_UNLOCK;
}

INTLSYM METD_get_collation (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ g e t _ c o l l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Look up an international text type object.
 *	If it doesn't exist, return NULL.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
INTLSYM	iname;
SYM	symbol;

/* Start by seeing if symbol is already defined */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_intlsym, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_intlsym) && 
	(((INTLSYM) symbol->sym_object)->intlsym_type == INTLSYM_collation))
	return (INTLSYM) symbol->sym_object;


/* Now see if it is in the database */

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;
iname = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_collation])
          X IN RDB$COLLATIONS 
    CROSS Y IN RDB$CHARACTER_SETS OVER RDB$CHARACTER_SET_ID
     WITH X.RDB$COLLATION_NAME EQ name->str_data;

    THREAD_ENTER;

    iname = (INTLSYM) ALLOCV (type_intlsym, dbb->dbb_pool, name->str_length);
    strcpy (iname->intlsym_name, (TEXT*) name->str_data);
    iname->intlsym_type       = INTLSYM_collation;
    iname->intlsym_flags      = 0;
    iname->intlsym_charset_id = X.RDB$CHARACTER_SET_ID;
    iname->intlsym_collate_id = X.RDB$COLLATION_ID;
    iname->intlsym_ttype      = INTL_CS_COLL_TO_TTYPE (iname->intlsym_charset_id, iname->intlsym_collate_id);
    iname->intlsym_bytes_per_char = 
			(Y.RDB$BYTES_PER_CHARACTER.NULL)	      ? 1 :
    			(Y.RDB$BYTES_PER_CHARACTER);
    THREAD_EXIT;

END_FOR
   ON_ERROR
   /* Assume V3 database */
   END_ERROR;

THREAD_ENTER;

if (!iname)
    return NULL;

/* Store in the symbol table */

symbol = iname->intlsym_symbol = (SYM) ALLOCV (type_sym, dbb->dbb_pool, 0);
symbol->sym_object = (BLK) iname;
symbol->sym_string = iname->intlsym_name;
symbol->sym_length = name->str_length;
symbol->sym_type = SYM_intlsym;
symbol->sym_dbb = dbb;
HSHD_insert (symbol);

return iname;
}

void METD_get_col_default (
    REQ         request,
    TEXT        *for_rel_name,
    TEXT        *for_col_name,
    BOOLEAN     *has_default,
    TEXT        *buffer,
    USHORT      buff_length
    )
{
/*************************************************************
 *
 *  M E T D _ g e t _ c o l _ d e f a u l t
 *
 **************************************************************
 *
 * Function:
 *    Gets the default value for a column of an existing table.
 *    Will check the default for the column of the table, if that is 
 *    not present, will check for the default of the relevant domain
 * 
 *    The default blr is returned in buffer. The blr is of the form
 *    blr_version4 blr_literal ..... blr_eoc
 * 
 *    Reads the system tables RDB$FIELDS and RDB$RELATION_FIELDS.
 *
 **************************************************************/
SLONG     *DB, *gds__trans;
DBB       dbb;
long      status_vect[20];

ISC_QUAD  *blob_id;
TEXT      *ptr_in_buffer;
BLB       blob;

STATUS    stat;
TSQL      tdsql;
USHORT    length;
isc_blob_handle  blob_handle = NULL;

*has_default = FALSE;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

/* V4.x multi threading requirements */
THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_col_default]) 
    RFL IN RDB$RELATION_FIELDS CROSS
    FLD IN RDB$FIELDS WITH
    RFL.RDB$RELATION_NAME EQ for_rel_name   AND
    RFL.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME  AND
    RFL.RDB$FIELD_NAME EQ for_col_name

    THREAD_ENTER;

    if (!RFL.RDB$DEFAULT_VALUE.NULL)
        {
        blob_id = &RFL.RDB$DEFAULT_VALUE;
        *has_default = TRUE;
        }
    else if (!FLD.RDB$DEFAULT_VALUE.NULL)
        {
        blob_id = &FLD.RDB$DEFAULT_VALUE;
        *has_default = TRUE;
        }
    else
        *has_default = FALSE;

    if (*has_default)
        {
        /* open the blob */
	THREAD_EXIT;
        stat = isc_open_blob2 (status_vect, &DB,  &gds__trans, 
            &blob_handle, blob_id, sizeof (blr_bpb), blr_bpb);
	THREAD_ENTER;
        if (stat)
            ERRD_punt ();

        /* fetch segments. Assuming here that the buffer is big enough.*/
        ptr_in_buffer = buffer;
        while (TRUE)
            {
	    THREAD_EXIT;
            stat = isc_get_segment (status_vect, &blob_handle,
                &length, buff_length, ptr_in_buffer);
	    THREAD_ENTER;
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
            else ERRD_punt ();

            }
	THREAD_EXIT;
        isc_close_blob (status_vect, &blob_handle);
	THREAD_ENTER;

	/* the default string must be of the form:
	   blr_version4 blr_literal ..... blr_eoc  */
	assert ((buffer[0] == blr_version4)||(buffer[0] == blr_version5));
	assert (buffer[1] == blr_literal);

        }
    else
	{
        if (request->req_dbb->dbb_db_SQL_dialect > SQL_DIALECT_V5)
	    buffer[0] = blr_version5;
        else
	    buffer[0] = blr_version4;
	buffer[1] = blr_eoc;
        }

    THREAD_EXIT;

END_FOR
ON_ERROR
      ERRD_punt ();
END_ERROR

THREAD_ENTER;

}

INTLSYM METD_get_charset (
    REQ		request,
    USHORT	length,
    UCHAR	*name)
{
/**************************************
 *
 *	M E T D _ g e t _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *	Look up an international text type object.
 *	If it doesn't exist, return NULL.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
INTLSYM	iname;
SYM	symbol;

/* Start by seeing if symbol is already defined */

symbol = HSHD_lookup (request->req_dbb, name, length, SYM_intlsym, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_intlsym) && 
	(((INTLSYM)(symbol->sym_object))->intlsym_type == INTLSYM_charset))
	return (INTLSYM) symbol->sym_object;


/* Now see if it is in the database */

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;
iname = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_charset])
          X IN RDB$COLLATIONS 
    CROSS Y IN RDB$CHARACTER_SETS OVER RDB$CHARACTER_SET_ID
    CROSS Z IN RDB$TYPES
    WITH Z.RDB$TYPE EQ Y.RDB$CHARACTER_SET_ID
     AND Z.RDB$TYPE_NAME EQ name
     AND Z.RDB$FIELD_NAME EQ "RDB$CHARACTER_SET_NAME"
     AND Y.RDB$DEFAULT_COLLATE_NAME EQ X.RDB$COLLATION_NAME;

    THREAD_ENTER;

    iname = (INTLSYM) ALLOCV (type_intlsym, dbb->dbb_pool, length);
    strcpy (iname->intlsym_name, name);
    iname->intlsym_type       = INTLSYM_charset;
    iname->intlsym_flags      = 0;
    iname->intlsym_charset_id = X.RDB$CHARACTER_SET_ID;
    iname->intlsym_collate_id = X.RDB$COLLATION_ID;
    iname->intlsym_ttype      = INTL_CS_COLL_TO_TTYPE (iname->intlsym_charset_id, iname->intlsym_collate_id);
    iname->intlsym_bytes_per_char = 
			(Y.RDB$BYTES_PER_CHARACTER.NULL)	      ? 1 :
    			(Y.RDB$BYTES_PER_CHARACTER);

    THREAD_EXIT;

END_FOR
    ON_ERROR
    /* Assume V3 database */
    END_ERROR;

THREAD_ENTER;

if (!iname)
    return NULL;

/* Store in the symbol table */

symbol = iname->intlsym_symbol = (SYM) ALLOCV (type_sym, dbb->dbb_pool, 0);
symbol->sym_object = (BLK) iname;
symbol->sym_string = iname->intlsym_name;
symbol->sym_length = length;
symbol->sym_type = SYM_intlsym;
symbol->sym_dbb = dbb;
HSHD_insert (symbol);

return iname;
}

STR METD_get_default_charset (
    REQ		request)
{
/**************************************
 *
 *	M E T D _ g e t _ d e f a u l t _ c h a r s e t
 *
 **************************************
 *
 * Functional description
 *	Find the default character set for a database
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
BLK	*handle;
UCHAR	*p;
UCHAR	*str;
USHORT	length;

dbb = request->req_dbb;
if (dbb->dbb_flags & DBB_no_charset)
    return NULL;

if (dbb->dbb_dfl_charset)
    return request->req_dbb->dbb_dfl_charset;

/* Now see if it is in the database */

DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

handle = NULL_PTR;

THREAD_EXIT;

FOR (REQUEST_HANDLE handle)
    FIRST 1 DBB IN RDB$DATABASE 
    WITH DBB.RDB$CHARACTER_SET_NAME NOT MISSING;

    THREAD_ENTER;

    /* Terminate ASCIIZ string on first blank */
    metd_exact_name (DBB.RDB$CHARACTER_SET_NAME);
    length = strlen (DBB.RDB$CHARACTER_SET_NAME);
    dbb->dbb_dfl_charset = ALLOCV (type_str, dbb->dbb_pool, length);
    dbb->dbb_dfl_charset->str_length = length;
    dbb->dbb_dfl_charset->str_charset = NULL;
    str = DBB.RDB$CHARACTER_SET_NAME;
    for (p = dbb->dbb_dfl_charset->str_data; length; --length)
        *p++ = *str++;

    THREAD_EXIT;
END_FOR
    ON_ERROR
    /* Assume V3 database */
    END_ERROR;
/*************
THREAD_ENTER;

Un comment these if any code is added after the END_ERROR and 
gds__release request ()

THREAD_EXIT;
*************/
isc_release_request (gds__status, GDS_REF (handle));
THREAD_ENTER;

if (dbb->dbb_dfl_charset == NULL)
    dbb->dbb_flags |= DBB_no_charset;

return dbb->dbb_dfl_charset;
}

USHORT METD_get_domain (
    REQ		request,
    FLD		field,
    UCHAR	*name)
{
/**************************************
 *
 *	M E T D _ g e t _ d o m a i n
 *
 **************************************
 *
 * Functional description
 *	Fetch domain information for field defined as 'name'
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
USHORT	found = FALSE;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_domain])
	FLX IN RDB$FIELDS 
	WITH FLX.RDB$FIELD_NAME EQ name

    THREAD_ENTER;

    found = TRUE;
    field->fld_length = FLX.RDB$FIELD_LENGTH;
    field->fld_scale = FLX.RDB$FIELD_SCALE;
    field->fld_sub_type = FLX.RDB$FIELD_SUB_TYPE;

    field->fld_character_set_id = 0;
    if (!FLX.RDB$CHARACTER_SET_ID.NULL)
        field->fld_character_set_id = FLX.RDB$CHARACTER_SET_ID;
    field->fld_collation_id = 0;
    if (!FLX.RDB$COLLATION_ID.NULL)
        field->fld_collation_id = FLX.RDB$COLLATION_ID;
    field->fld_character_length = 0;
    if (!FLX.RDB$CHARACTER_LENGTH.NULL)
        field->fld_character_length = FLX.RDB$CHARACTER_LENGTH;

    if (!FLX.RDB$COMPUTED_BLR.NULL)
	field->fld_flags |= FLD_computed;

    convert_dtype (field, FLX.RDB$FIELD_TYPE);

    if (FLX.RDB$FIELD_TYPE == blr_blob)
	{
        field->fld_seg_length = FLX.RDB$SEGMENT_LENGTH;
	}

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

return found;
}

void METD_get_domain_default (
    REQ         request,
    TEXT        *domain_name,
    BOOLEAN     *has_default,
    TEXT        *buffer,
    USHORT      buff_length
    )
{
/*************************************************************
 *
 *  M E T D _ g e t _ d o m a i n _ d e f a u l t
 *
 **************************************************************
 *
 * Function:
 *    Gets the default value for a domain of an existing table.
 *
 **************************************************************/
SLONG     *DB, *gds__trans;
DBB       dbb;
ISC_STATUS status_vect[20];

ISC_QUAD  *blob_id;
TEXT      *ptr_in_buffer;
BLB       blob;

STATUS    stat;
TSQL      tdsql;
USHORT    length;
isc_blob_handle  blob_handle = NULL;

*has_default = FALSE;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

/* V4.x multi threading requirements */
THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_domain_2])
    FLD IN RDB$FIELDS WITH
    FLD.RDB$FIELD_NAME EQ domain_name

    THREAD_ENTER;

    if (!FLD.RDB$DEFAULT_VALUE.NULL)
        {
        blob_id = &FLD.RDB$DEFAULT_VALUE;
        *has_default = TRUE;
        }
    else
        *has_default = FALSE;

    if (*has_default)
        {
        /* open the blob */
	THREAD_EXIT;
        stat = isc_open_blob2 (status_vect, &DB,  &gds__trans, 
            &blob_handle, blob_id, sizeof (blr_bpb), blr_bpb);
	THREAD_ENTER;
        if (stat)
            ERRD_punt ();

        /* fetch segments. Assume buffer is big enough.  */
        ptr_in_buffer = buffer;
        while (TRUE)
            {
	    THREAD_EXIT;
            stat = isc_get_segment (status_vect,
		&blob_handle,
                &length, buff_length,
                ptr_in_buffer);
	    THREAD_ENTER;
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
            else ERRD_punt ();

            }
	THREAD_EXIT;
        isc_close_blob (status_vect, &blob_handle);
	THREAD_ENTER;

	/* the default string must be of the form:
	   blr_version4 blr_literal ..... blr_eoc  */
	assert ((buffer[0] == blr_version4)||(buffer[0] == blr_version5));
	assert (buffer[1] == blr_literal);

        }
    else
	{
        if (request->req_dbb->dbb_db_SQL_dialect > SQL_DIALECT_V5)
	    buffer[0] = blr_version5;
        else
	    buffer[0] = blr_version4;
	buffer[1] = blr_eoc;
        }

    THREAD_EXIT;

END_FOR
ON_ERROR
      ERRD_punt ();
END_ERROR

THREAD_ENTER;

}

UDF METD_get_function (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ g e t _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Look up a user defined function.  If it doesn't exist,
 *	return NULL.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
UDF	udf;
USHORT	return_arg;
SYM	symbol;

/* Start by seeing if symbol is already defined */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_udf, 0); 
if (symbol)
    return (UDF) symbol->sym_object;

/* Now see if it is in the database */

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;
udf = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_function])
    X IN RDB$FUNCTIONS WITH
    X.RDB$FUNCTION_NAME EQ name->str_data

    THREAD_ENTER;

    udf = (UDF) ALLOCV (type_udf, dbb->dbb_pool, name->str_length);
    udf->udf_next = dbb->dbb_functions;
    dbb->dbb_functions = udf;
    strcpy (udf->udf_name, (TEXT*) name->str_data);
    return_arg = X.RDB$RETURN_ARGUMENT;

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

if (!udf)
    return NULL;

/* Note: The following two requests differ in the fields which are
 *	 new since ODS7 (DBB_v3 flag).  One of the two requests
 *	 here will be cached in dbb_requests [ird_func_return],
 *	 the one that is appropriate for the database we are
 *	 working against.
 */
THREAD_EXIT;

if (dbb->dbb_flags & DBB_v3)
    {
    FOR (REQUEST_HANDLE dbb->dbb_requests [irq_func_return])
        X IN RDB$FUNCTION_ARGUMENTS WITH
        X.RDB$FUNCTION_NAME EQ name->str_data AND
        X.RDB$ARGUMENT_POSITION EQ return_arg

	THREAD_ENTER;

	udf->udf_dtype = (X.RDB$FIELD_TYPE != blr_blob) ?
	    gds_cvt_blr_dtype [X.RDB$FIELD_TYPE] : dtype_blob;
	udf->udf_scale = X.RDB$FIELD_SCALE;
	udf->udf_sub_type = X.RDB$FIELD_SUB_TYPE;
	udf->udf_length = X.RDB$FIELD_LENGTH;

	THREAD_EXIT;

    END_FOR
	ON_ERROR
	END_ERROR;
    }
else	/* V4 or greater dbb */
    {
    FOR (REQUEST_HANDLE dbb->dbb_requests [irq_func_return])
        X IN RDB$FUNCTION_ARGUMENTS WITH
        X.RDB$FUNCTION_NAME EQ name->str_data AND
        X.RDB$ARGUMENT_POSITION EQ return_arg

	THREAD_ENTER;

	udf->udf_dtype = (X.RDB$FIELD_TYPE != blr_blob) ?
	    gds_cvt_blr_dtype [X.RDB$FIELD_TYPE] : dtype_blob;
	udf->udf_scale = X.RDB$FIELD_SCALE;
	udf->udf_sub_type = X.RDB$FIELD_SUB_TYPE;
	udf->udf_length = X.RDB$FIELD_LENGTH;

	if (!X.RDB$CHARACTER_SET_ID.NULL)
	    udf->udf_character_set_id = X.RDB$CHARACTER_SET_ID;

	THREAD_EXIT;

    END_FOR
	ON_ERROR
	END_ERROR;
    }

THREAD_ENTER;

/* Adjust the return type & length of the UDF to account for 
 * cstring & varying.  While a UDF can return CSTRING, we convert it
 * to CHAR for manipulation as CSTRING is not a SQL type.
 * (Q: why not use varying?)
 */

if (udf->udf_dtype == dtype_cstring)
    {
    udf->udf_dtype = dtype_text;
    }
else if (udf->udf_dtype == dtype_varying)
    udf->udf_length += sizeof (USHORT);

/* Store in the symbol table */

symbol = udf->udf_symbol = (SYM) ALLOCV (type_sym, dbb->dbb_pool, 0);
symbol->sym_object = (BLK) udf;
symbol->sym_string = udf->udf_name;
symbol->sym_length = name->str_length;
symbol->sym_type = SYM_udf;
symbol->sym_dbb = dbb;
HSHD_insert (symbol);

return udf;
}

NOD METD_get_primary_key (
    REQ		request,
    STR		relation_name)
{
/**************************************
 *
 *	M E T D _ g e t _ p r i m a r y _ k e y
 *
 **************************************
 *
 * Functional description
 *	Lookup the fields for the primary key
 *	index on a relation, returning a list 
 *	node of the fields.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
NOD	list, field_node;
STR	field_name;
USHORT	count;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

list = NULL;
count = 0;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_primary_key])
    X IN RDB$INDICES CROSS
    Y IN RDB$INDEX_SEGMENTS 
    OVER RDB$INDEX_NAME
    WITH X.RDB$RELATION_NAME EQ relation_name->str_data
    AND X.RDB$INDEX_NAME STARTING "RDB$PRIMARY"
    SORTED BY Y.RDB$FIELD_POSITION

    THREAD_ENTER;

    if (!list)
        list = MAKE_node (nod_list, (int) X.RDB$SEGMENT_COUNT);
    field_name = MAKE_cstring (Y.RDB$FIELD_NAME);
    field_node = MAKE_node (nod_field_name, (int) e_fln_count);
    field_node->nod_arg [e_fln_name] = (NOD) field_name;
    list->nod_arg [count] = field_node;
    count++;

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;
 
return list;
}

PRC METD_get_procedure (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ g e t _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Look up a procedure.  If it doesn't exist, return NULL.
 *	If it does, fetch field information as well.
 *	If it is marked dropped, try to read from system tables
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
FLD	parameter, *ptr;
PRC	procedure, temp;
SYM	symbol;
SSHORT	type, count;


METD_REC_LOCK;

/* Start by seeing if symbol is already defined */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_procedure, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_procedure) && 
	    ((procedure = (PRC) symbol->sym_object) && 
	    (!(procedure->prc_flags & PRC_dropped))))
        {
        METD_REC_UNLOCK;
        return (PRC) symbol->sym_object;
        }

/* see if the procedure is the one currently being defined in this request */

if (((temp = request->req_procedure) != NULL) &&
    !strcmp (temp->prc_name, name->str_data))
    {
    METD_REC_UNLOCK;
    return temp;
    }

/* now see if it is in the database */

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;
procedure = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_procedure])
    X IN RDB$PROCEDURES WITH
    X.RDB$PROCEDURE_NAME EQ name->str_data

    THREAD_ENTER;

    metd_exact_name (X.RDB$OWNER_NAME);

    procedure = (PRC) ALLOCV (type_prc, dbb->dbb_pool,
			name->str_length + strlen (X.RDB$OWNER_NAME));
    procedure->prc_id = X.RDB$PROCEDURE_ID;

    procedure->prc_name = procedure->prc_data;
    procedure->prc_owner = procedure->prc_data + name->str_length + 1;
    strcpy (procedure->prc_name, (TEXT*) name->str_data);
    strcpy (procedure->prc_owner, (TEXT*) X.RDB$OWNER_NAME);

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

if (!procedure)
    {
    METD_REC_UNLOCK;
    return NULL;
    }

/* Lookup parameter stuff */

for (type = 0; type < 2; type++)
    {
    if (type)
        ptr = &procedure->prc_outputs;
    else
        ptr = &procedure->prc_inputs;
    count = 0;

    THREAD_EXIT;

    FOR (REQUEST_HANDLE dbb->dbb_requests [irq_parameters])
	PR IN RDB$PROCEDURE_PARAMETERS CROSS
	RFR IN RDB$FIELDS
	WITH RFR.RDB$FIELD_NAME EQ PR.RDB$FIELD_SOURCE
	AND PR.RDB$PROCEDURE_NAME EQ name->str_data
	AND PR.RDB$PARAMETER_TYPE = type
	SORTED BY DESCENDING PR.RDB$PARAMETER_NUMBER

	THREAD_ENTER;

	count++;
        /* allocate the field block */             

        metd_exact_name (PR.RDB$PARAMETER_NAME);
        parameter = (FLD) ALLOCV (type_fld, dbb->dbb_pool,
			strlen (PR.RDB$PARAMETER_NAME));
        parameter->fld_next = *ptr;
	*ptr = parameter;
                                      
        /* get parameter information */

        strcpy (parameter->fld_name, PR.RDB$PARAMETER_NAME);
        parameter->fld_id = PR.RDB$PARAMETER_NUMBER;
        parameter->fld_length = RFR.RDB$FIELD_LENGTH;
        parameter->fld_scale = RFR.RDB$FIELD_SCALE;
        parameter->fld_sub_type = RFR.RDB$FIELD_SUB_TYPE;
	parameter->fld_procedure = procedure;

        if (!RFR.RDB$CHARACTER_SET_ID.NULL)
	    parameter->fld_character_set_id = RFR.RDB$CHARACTER_SET_ID;

        if (!RFR.RDB$COLLATION_ID.NULL)
	    parameter->fld_collation_id = RFR.RDB$COLLATION_ID;

	convert_dtype (parameter, RFR.RDB$FIELD_TYPE);


        if (!RFR.RDB$NULL_FLAG) 
            parameter->fld_flags |= FLD_nullable;

        if (RFR.RDB$FIELD_TYPE == blr_blob)
	    {
            parameter->fld_seg_length = RFR.RDB$SEGMENT_LENGTH;
	    }

	THREAD_EXIT;

    END_FOR
	ON_ERROR
	END_ERROR;

    THREAD_ENTER;

    if (type)
	procedure->prc_out_count = count;
    else
	procedure->prc_in_count = count;
    }

/* Since we could give up control due to the THREAD_EXIT and THEAD_ENTER
 * calls, another thread may have added the same procedure in the mean time
 */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_procedure, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_procedure) && 
	    ((temp = (PRC) symbol->sym_object) && 
	    (!(temp->prc_flags & PRC_dropped))))
	{
	/* Get rid of all the stuff we just read in. Use existing one */

	free_procedure (procedure);
        METD_REC_UNLOCK;
        return (PRC) symbol->sym_object;
	}


/* store in the symbol table unless the procedure is not yet committed */

if (!(procedure->prc_flags & PRC_new_procedure))
    {
    /* add procedure in the front of the list */

    procedure->prc_next = dbb->dbb_procedures;
    dbb->dbb_procedures = procedure;

    symbol = procedure->prc_symbol = (SYM) ALLOCV (type_sym, dbb->dbb_pool, 0);
    symbol->sym_object = (BLK) procedure;
    symbol->sym_string = procedure->prc_name;
    symbol->sym_length = name->str_length;
    symbol->sym_type = SYM_procedure;
    symbol->sym_dbb = dbb;
    HSHD_insert (symbol);
    }

    METD_REC_UNLOCK;
    return procedure;
}

DSQL_REL METD_get_relation (
    REQ		request,
    STR		name)
{
/**************************************
 *
 *	M E T D _ g e t _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Look up a relation.  If it doesn't exist, return NULL.
 *	If it does, fetch field information as well.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
FLD	field, *ptr;
DSQL_REL	relation, temp;
SYM	symbol;
TSQL    tdsql;

tdsql = GET_THREAD_DATA;

METD_REC_LOCK;

/* Start by seeing if symbol is already defined */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_relation, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_relation) &&
	    ((relation = (DSQL_REL) symbol->sym_object) && 
	    (!(relation->rel_flags & REL_dropped))))
        {
        METD_REC_UNLOCK;
	return (DSQL_REL) symbol->sym_object;
        }

/* see if the relation is the one currently being defined in this request */

if (((temp = request->req_relation) != NULL) &&
    !strcmp(temp->rel_name, name->str_data))
    {
    METD_REC_UNLOCK;
    return temp;
    }

/* now see if it is in the database */

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;
relation = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_relation])
    X IN RDB$RELATIONS WITH
    X.RDB$RELATION_NAME EQ name->str_data

    /* allocate the relation block -- if the relation id has not yet 
       been assigned, and this is a type of request which does not 
       use ids, prepare a temporary relation block to provide 
       information without caching it */

    THREAD_ENTER;

    metd_exact_name (X.RDB$OWNER_NAME);

    if (!X.RDB$RELATION_ID.NULL)
	{
        relation = (DSQL_REL) ALLOCV (
	    type_dsql_rel, dbb->dbb_pool, name->str_length + strlen (X.RDB$OWNER_NAME));
        relation->rel_id = X.RDB$RELATION_ID;
	}
    else if (!DDL_ids (request))
	{
        relation = (DSQL_REL) ALLOCDV (type_dsql_rel, name->str_length + strlen (X.RDB$OWNER_NAME));
  	relation->rel_flags |= REL_new_relation;
	}

    /* fill out the relation information */
             
    if (relation)
	{
	relation->rel_name = relation->rel_data;
	relation->rel_owner = relation->rel_data + name->str_length + 1;
	strcpy (relation->rel_name, (TEXT*) name->str_data);
	strcpy (relation->rel_owner, (TEXT*) X.RDB$OWNER_NAME);
	if (!(relation->rel_dbkey_length = X.RDB$DBKEY_LENGTH))
	    relation->rel_dbkey_length = 8;
	}

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

if (!relation)
    {
    METD_REC_UNLOCK;
    return NULL;
    }

/* Lookup field stuff */

ptr = &relation->rel_fields;

THREAD_EXIT;

if (dbb->dbb_flags & DBB_v3)
    {
    FOR (REQUEST_HANDLE dbb->dbb_requests [irq_fields])
	    FLX IN RDB$FIELDS CROSS 
	    RFR IN RDB$RELATION_FIELDS
	    WITH FLX.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE
	    AND RFR.RDB$RELATION_NAME EQ name->str_data
	    SORTED BY RFR.RDB$FIELD_POSITION

	THREAD_ENTER;

	/* allocate the field block */             

	metd_exact_name (RFR.RDB$FIELD_NAME);

	/* Allocate from default or permanent pool as appropriate */

	if (relation->rel_flags & REL_new_relation)
	    *ptr = field = (FLD) ALLOCDV (type_fld, 
					  strlen (RFR.RDB$FIELD_NAME));
	else
	    *ptr = field = (FLD) ALLOCV (type_fld, dbb->dbb_pool,
					 strlen (RFR.RDB$FIELD_NAME));
	ptr = &field->fld_next;
	                                  
	/* get field information */

	strcpy (field->fld_name, RFR.RDB$FIELD_NAME);
	field->fld_id = RFR.RDB$FIELD_ID;
	field->fld_length = FLX.RDB$FIELD_LENGTH;
	field->fld_scale = FLX.RDB$FIELD_SCALE;
	field->fld_sub_type = FLX.RDB$FIELD_SUB_TYPE;

	if (!FLX.RDB$COMPUTED_BLR.NULL)
	    field->fld_flags |= FLD_computed;

	convert_dtype (field, FLX.RDB$FIELD_TYPE);

	if (FLX.RDB$FIELD_TYPE == blr_blob)
	    {
	    field->fld_seg_length = FLX.RDB$SEGMENT_LENGTH;
	    }

	if (!(dbb->dbb_flags & DBB_no_arrays))
	    check_array (request, FLX.RDB$FIELD_NAME, field);

	field->fld_flags |= FLD_nullable;

	THREAD_EXIT;

    END_FOR
	ON_ERROR
	END_ERROR;
    }
else /* V4 ODS8 dbb */
   {
    FOR (REQUEST_HANDLE dbb->dbb_requests [irq_fields])
	    FLX IN RDB$FIELDS CROSS 
	    RFR IN RDB$RELATION_FIELDS
	    WITH FLX.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE
	    AND RFR.RDB$RELATION_NAME EQ name->str_data
	    SORTED BY RFR.RDB$FIELD_POSITION

	THREAD_ENTER;

	/* allocate the field block */             

	metd_exact_name (RFR.RDB$FIELD_NAME);

	/* Allocate from default or permanent pool as appropriate */

	if (relation->rel_flags & REL_new_relation)
	    *ptr = field = (FLD) ALLOCDV (type_fld, 
					  strlen (RFR.RDB$FIELD_NAME));
	else
	    *ptr = field = (FLD) ALLOCV (type_fld, dbb->dbb_pool,
					 strlen (RFR.RDB$FIELD_NAME));

	ptr = &field->fld_next;
	                                  
	/* get field information */

	strcpy (field->fld_name, RFR.RDB$FIELD_NAME);
	field->fld_id = RFR.RDB$FIELD_ID;
	field->fld_length = FLX.RDB$FIELD_LENGTH;
	field->fld_scale = FLX.RDB$FIELD_SCALE;
	field->fld_sub_type = FLX.RDB$FIELD_SUB_TYPE;

	if (!FLX.RDB$COMPUTED_BLR.NULL)
	    field->fld_flags |= FLD_computed;

	convert_dtype (field, FLX.RDB$FIELD_TYPE);

	if (FLX.RDB$FIELD_TYPE == blr_blob)
	    {
	    field->fld_seg_length = FLX.RDB$SEGMENT_LENGTH;
	    }

	if (!FLX.RDB$DIMENSIONS.NULL && FLX.RDB$DIMENSIONS)
	    {
	    field->fld_element_dtype = field->fld_dtype;
	    field->fld_element_length = field->fld_length;
	    field->fld_dtype = dtype_array;
	    field->fld_length = sizeof (GDS__QUAD);
	    field->fld_dimensions = FLX.RDB$DIMENSIONS;
	    }

	if (!FLX.RDB$CHARACTER_SET_ID.NULL)
	    field->fld_character_set_id = FLX.RDB$CHARACTER_SET_ID;

	if (!RFR.RDB$COLLATION_ID.NULL)
	    field->fld_collation_id = RFR.RDB$COLLATION_ID;
	else if (!FLX.RDB$COLLATION_ID.NULL)
	    field->fld_collation_id = FLX.RDB$COLLATION_ID;

	if (!(RFR.RDB$NULL_FLAG || FLX.RDB$NULL_FLAG)) 
	    field->fld_flags |= FLD_nullable;

	THREAD_EXIT;

    END_FOR
	ON_ERROR
	END_ERROR;
    }

THREAD_ENTER;

/* Since we could give up control due to the THREAD_EXIT and THEAD_ENTER,
 * another thread may have added the same table in the mean time
 */

symbol = HSHD_lookup (request->req_dbb, name->str_data, 
			name->str_length, SYM_relation, 0); 
for (; symbol; symbol = symbol->sym_homonym)
    if ((symbol->sym_type == SYM_relation) &&
	    ((temp = (DSQL_REL) symbol->sym_object) && 
	    (!(temp ->rel_flags & REL_dropped))))
	{
	free_relation (relation);
        METD_REC_UNLOCK;
	return (DSQL_REL) symbol->sym_object;
	}


/* Add relation to front of list */

if (!(relation->rel_flags & REL_new_relation))
    {
    relation->rel_next = dbb->dbb_relations;
    dbb->dbb_relations = relation;

    /* store in the symbol table unless the relation is not yet committed */

    symbol = relation->rel_symbol = (SYM) ALLOCV (type_sym, dbb->dbb_pool, 0);
    symbol->sym_object = (BLK) relation;
    symbol->sym_string = relation->rel_name;
    symbol->sym_length = name->str_length;
    symbol->sym_type = SYM_relation;
    symbol->sym_dbb = dbb;
    HSHD_insert (symbol);
    }
METD_REC_UNLOCK;
return relation;
}

STR METD_get_trigger_relation (
    REQ		request,
    STR		name,
    USHORT	*trig_type)
{
/**************************************
 *
 *	M E T D _ g e t _ t r i g g e r _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Look up a trigger's relation and return it's current type
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
STR	relation;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

relation = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_trigger])
    X IN RDB$TRIGGERS WITH
    X.RDB$TRIGGER_NAME EQ name->str_data

    THREAD_ENTER;

    metd_exact_name (X.RDB$RELATION_NAME);

    relation = MAKE_string (X.RDB$RELATION_NAME, strlen (X.RDB$RELATION_NAME));
    *trig_type = X.RDB$TRIGGER_TYPE;

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

return relation;
}

USHORT METD_get_type (
    REQ		request,
    STR		name,
    UCHAR	*field,
    SSHORT	*value)
{
/**************************************
 *
 *	M E T D _ g e t _ t y p e 
 *
 **************************************
 *
 * Functional description
 *	Look up a symbolic name in RDB$TYPES
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;
USHORT	found;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

found = FALSE;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_type])
    X IN RDB$TYPES WITH
    X.RDB$FIELD_NAME EQ field AND
    X.RDB$TYPE_NAME EQ name->str_data;

    THREAD_ENTER;

    found = TRUE;
    *value = X.RDB$TYPE;

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

return found;
}

DSQL_REL METD_get_view_relation (
    REQ		request,
    UCHAR	*view_name,
    UCHAR	*relation_or_alias,
    USHORT	level)
{
/**************************************
 *
 *	M E T D _ g e t _ v i e w _ r e l a t i o n
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
SLONG	*DB, *gds__trans;
STR	relation_name;
DSQL_REL	relation;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

relation = NULL;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_view], 
     LEVEL level)
    X IN RDB$VIEW_RELATIONS WITH
    X.RDB$VIEW_NAME EQ view_name

    THREAD_ENTER;

    metd_exact_name (X.RDB$CONTEXT_NAME);
    metd_exact_name (X.RDB$RELATION_NAME);

    if (!strcmp (X.RDB$RELATION_NAME, relation_or_alias) ||
	!strcmp (X.RDB$CONTEXT_NAME, relation_or_alias))
	{
	relation_name = MAKE_string (X.RDB$RELATION_NAME, 
				     strlen (X.RDB$RELATION_NAME));
	relation = METD_get_relation (request, relation_name);
	ALLD_release (relation_name);
	return relation;
	}

    if (relation = METD_get_view_relation (request, X.RDB$RELATION_NAME, relation_or_alias, level + 1))
	return relation;

    THREAD_EXIT;

END_FOR
    ON_ERROR
    END_ERROR;

THREAD_ENTER;

return NULL;
}

static void check_array (
    REQ		request,
    TEXT	*field_name,
    FLD		field)
{
/**************************************
 *
 *	c h e c k _ a r r a y
 *
 **************************************
 *
 * Functional description
 *	Try to get field dimensions (RDB$FIELD_DIMENSIONS may not be
 *	defined in the meta-data).  If we're not successful, indicate
 *	so in the database block to avoid wasting further effort.
 *
 **************************************/
SLONG	*DB, *gds__trans;
DBB	dbb;

dbb = request->req_dbb;
DB = dbb->dbb_database_handle;
gds__trans = request->req_trans;

THREAD_EXIT;

FOR (REQUEST_HANDLE dbb->dbb_requests [irq_dimensions])
    X IN RDB$FIELDS WITH
    X.RDB$FIELD_NAME EQ field_name

    THREAD_ENTER;

    if (!X.RDB$DIMENSIONS.NULL && X.RDB$DIMENSIONS)
        {
        field->fld_element_dtype = field->fld_dtype;
        field->fld_element_length = field->fld_length;
        field->fld_dtype = dtype_array;
        field->fld_length = sizeof (GDS__QUAD);
        field->fld_dimensions = X.RDB$DIMENSIONS;
        }

    THREAD_EXIT;

END_FOR
    ON_ERROR
       dbb->dbb_flags |= DBB_no_arrays;
    END_ERROR;

THREAD_ENTER;
}

static void convert_dtype (
    FLD		field,
    SSHORT	field_type)
{
/**************************************
 *
 *	c o n v e r t _ d t y p e
 *
 **************************************
 *
 * Functional description
 *	Convert from the blr_<type> stored in system metadata
 *	to the internal dtype_* descriptor.  Also set field
 *	length.
 *
 **************************************/

/* fill out the type descriptor */

if (field_type == blr_text)
    {
    field->fld_dtype = dtype_text;
    }
else if (field_type == blr_varying)
    {
    field->fld_dtype = dtype_varying;
    field->fld_length += sizeof (USHORT);
    }
else if (field_type == blr_blob)
    {
    field->fld_dtype = dtype_blob;
    field->fld_length = type_lengths [field->fld_dtype];
    }
else
    {
    field->fld_dtype = gds_cvt_blr_dtype [field_type];
    field->fld_length = type_lengths [field->fld_dtype];

    assert (field->fld_dtype != dtype_null);
    }
}

static void free_procedure (
	PRC	procedure)
{
/**************************************
 *
 *	f r e e _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Free memory allocated for a procedure block and params
 *
 **************************************/
FLD     param, temp;

/* release the input & output parameter blocks */

for (param = procedure->prc_inputs; param;)
    {
    temp = param;
    param = param->fld_next;
    ALLD_release (temp);
    }

for (param = procedure->prc_outputs; param;)
    {
    temp = param;
    param = param->fld_next;
    ALLD_release (temp);
    }

/* release the procedure & symbol blocks */

ALLD_release (procedure);
}

static void free_relation (
	DSQL_REL	relation)
{
/**************************************
 *
 *	f r e e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Free memory allocated for a relation block and fields
 *
 **************************************/
FLD     field, temp;

/* release the field blocks */

for (field = relation->rel_fields; field;)
    {
    temp = field;
    field = field->fld_next;
    ALLD_release (temp);
    }

/* release the relation & symbol blocks */

ALLD_release (relation);
}

static TEXT *metd_exact_name (
    TEXT	*str)
{
/**************************************
 *
 *	m e t d _ e x a c t _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Trim trailing spaces off a metadata name.
 *
 *	SQL delimited identifier may have blank as part of the name
 *
 *	Parameters:  str - the string to terminate
 *	Returns:     str
 *
 **************************************/
TEXT    *p, *q;
#define	BLANK			'\040'

q = str - 1;
for (p = str; *p; p++)
    {
    if (*p != BLANK)
	q = p;
    }

*(q+1) = '\0';

return str;
}
