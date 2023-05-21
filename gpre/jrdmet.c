/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		jrdmet.c
 *	DESCRIPTION:	Non-database meta data for internal JRD stuff
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

#include "../jrd/gds.h"
#include "../jrd/common.h"
#include "../jrd/ods.h"

struct blk {
    SLONG	blk_header;
};

#include "../gpre/gpre.h"
#include "../jrd/constants.h"
#define GPRE
#include "../jrd/ini.h"
#undef GPRE
#include "../gpre/hsh_proto.h"
#include "../gpre/jrdme_proto.h"

void JRDMET_init (
    DBB	db)
{
/**************************************
 *
 *	J R D M E T _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize in memory meta data.
 *
 **************************************/
REL		relation;
SYM		symbol;
FLD		field;
TYP		type;
RTYP		*rtype;
UCHAR		*relfld, *fld;
int		n;
struct gfld	*gfield;

relfld = relfields;

while (relfld [RFLD_R_NAME])
    {
    relation = (REL) ALLOC (REL_LEN);
    relation->rel_database = db;
    relation->rel_next  = db->dbb_relations;
    relation->rel_id = relfld [RFLD_R_ID];
    db->dbb_relations = relation;
    relation->rel_symbol = symbol = (SYM) ALLOC (SYM_LEN);
    symbol->sym_type = SYM_relation;
    symbol->sym_object = (CTX) relation;
    symbol->sym_string = names [relfld [RFLD_R_NAME]];
    HSH_insert (symbol);

    for (n = 0, fld = relfld + RFLD_RPT; fld [RFLD_F_NAME]; n++, fld += RFLD_F_LENGTH)
	{
	relation->rel_fields = field;
	gfield = (fld [RFLD_F_UPD_MINOR]) ?
	    &gfields [fld [RFLD_F_UPD_ID]] : &gfields [fld [RFLD_F_ID]];
	field = (FLD) ALLOC (FLD_LEN);
	field->fld_relation = relation;
	field->fld_next = relation->rel_fields;
	field->fld_id = n;
	field->fld_length = gfield->gfld_length;
	field->fld_dtype = gfield->gfld_dtype;
	field->fld_sub_type = gfield->gfld_sub_type;
	if (field->fld_dtype == dtype_varying ||
	    field->fld_dtype == dtype_text)
	    {
	    field->fld_dtype = dtype_cstring; 
	    field->fld_flags |= FLD_text;
	    ++field->fld_length;
	    if (gfield->gfld_sub_type == dsc_text_type_metadata)
		{
	        field->fld_flags |= FLD_charset;
		field->fld_charset_id = CS_METADATA;
		field->fld_collate_id = COLLATE_NONE;
		field->fld_ttype = ttype_metadata;
		}
	    else
		{
	        field->fld_flags |= FLD_charset;
		field->fld_charset_id = CS_NONE;
		field->fld_collate_id = COLLATE_NONE;
		field->fld_ttype = ttype_none;
		}
	    }
	else if (field->fld_dtype == dtype_blob)
	    {
	    field->fld_dtype = dtype_blob;
	    field->fld_flags |= FLD_blob;
	    if (gfield->gfld_sub_type == BLOB_text)
		field->fld_charset_id = CS_METADATA;
	    }

	field->fld_symbol = symbol = (SYM) ALLOC (SYM_LEN);
	symbol->sym_type = SYM_field;
	symbol->sym_object = (CTX) field;
	symbol->sym_string = names [fld [RFLD_F_NAME]];
	HSH_insert (symbol);

	field->fld_global = symbol = (SYM) ALLOC (SYM_LEN);
	symbol->sym_type = SYM_field;
	symbol->sym_object = (CTX) field;
	symbol->sym_string = names [gfield->gfld_name];
	}
    relfld = fld + 1;
    }

for (rtype = types; rtype->rtyp_name; rtype++)
    {
    type = (TYP) ALLOC (TYP_LEN);
    type->typ_symbol = symbol = (SYM) ALLOC (SYM_LEN);
    type->typ_value = rtype->rtyp_value;
    symbol->sym_type = SYM_type;
    symbol->sym_object = (CTX) type;
    symbol->sym_string = rtype->rtyp_name;
    HSH_insert (symbol);
    }
}
