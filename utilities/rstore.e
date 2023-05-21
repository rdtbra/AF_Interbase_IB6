/*
 *	PROGRAM:	JRD Rebuild scrambled database
 *	MODULE:		rstore.e
 *	DESCRIPTION:	Store page headers for analysis
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
#include "../utilities/rebuild.h"
#include "../utilities/rebui_proto.h"
#include "../utilities/rstor_proto.h"
#include "../jrd/gds_proto.h"

DATABASE DB = STATIC FILENAME "rebuild.gdb";

static void	store_headers (RBDB);

void RSTORE (
    RBDB		rbdb)
{
/**************************************
 *
 *	R S T O R E
 *
 **************************************
 *
 * Functional description
 *	write the contents of page headers
 *	into a database.
 *
 **************************************/

READY
    ON_ERROR
	gds__print_status (gds__status);
	ib_printf ("can't open the documentaton database\n");
	FINISH; 
	return;
    END_ERROR;
START_TRANSACTION
    ON_ERROR
	gds__print_status (gds__status);
	ib_printf ("can't start a transaction on the documentaton database\n");
	FINISH; 
	return;
    END_ERROR;
store_headers (rbdb);
COMMIT;
FINISH;
}

static void store_headers (
    RBDB		rbdb)
{
/**************************************
 *
 *	s t o r e _ h e a d e r s
 *
 **************************************
 *
 * Functional description
 *	write the contents of page headers
 *	into a database.
 *
 **************************************/
ULONG	page_number;
PAG	page;
HDR	header;
BLP	blob;
BTR	bucket;
DPG	data;
IRT	index_root;
PIP	pip;
PPG	pointer;
GDS__QUAD temp;

page_number = 0;

while (page = RBDB_read (rbdb, page_number))
    {
    STORE P IN FULL_PAGES USING 
	{
	P.NUMBER = page_number;                         
	P.TYPE = page->pag_type;                           
	P.FLAGS = page->pag_flags;                         
	P.CHECKSUM = page->pag_checksum;                      
#ifndef ODS_4
	P.GENERATION = page->pag_generation;                     
#endif
	P.RELATION.NULL = TRUE;                       
	P.SEQUENCE.NULL = TRUE;                       
	P.BLP_LEAD_PAGE.NULL = TRUE;                  
	P.BLP_LENGTH.NULL = TRUE;                     
	P.BTR_SIBLING.NULL = TRUE;                    
	P.BTR_LENGTH.NULL = TRUE;                     
	P.BTR_ID.NULL = TRUE;                         
	P.BTR_LEVEL.NULL = TRUE;                      
	P.DPG_COUNT.NULL = TRUE;                      
	P.IRT_COUNT.NULL = TRUE;                      
	P.HDR_PAGE_SIZE.NULL = TRUE;                 
	P.HDR_ODS_VERSION.NULL = TRUE;                
	P.HDR_PAGES.NULL = TRUE;                     
	P.HDR_OLDEST_TRANS.NULL = TRUE;               
	P.HDR_OLDEST_ACTIVE.NULL = TRUE;              
	P.HDR_NEXT_TRANS.NULL = TRUE;                 
	P.HDR_FLAGS.NULL = TRUE;                      
	P.HDR_CREATION_DATE.NULL = TRUE;              
#ifndef ODS_4
	P.HDR_ATTACHMENT_ID.NULL = TRUE;              
	P.HDR_IMPLEMENTATION.NULL = TRUE;             
	P.HDR_SHADOW_COUNT.NULL = TRUE;               
#endif
	P.PIP_MIN.NULL = TRUE;                        
	P.PPG_NEXT.NULL = TRUE;                       
	P.PPG_COUNT.NULL = TRUE;                     
	P.PPG_MIN_SPACE.NULL = TRUE;                  
	P.PPG_MAX_SPACE.NULL = TRUE;                  
	P.TIP_NEXT.NULL = TRUE;                       

        
	switch (page->pag_type)
	    {    
	    case pag_header:
		header = (HDR) page;
		P.HDR_PAGE_SIZE.NULL = FALSE;				 
		P.HDR_ODS_VERSION.NULL = FALSE;				
		P.HDR_PAGES.NULL = FALSE;					 
		P.HDR_OLDEST_TRANS.NULL = FALSE;			   
		P.HDR_OLDEST_ACTIVE.NULL = FALSE;			  
		P.HDR_NEXT_TRANS.NULL = FALSE;				 
		P.SEQUENCE.NULL = FALSE;				   
		P.HDR_FLAGS.NULL = FALSE;					  
		P.HDR_CREATION_DATE.NULL = FALSE;			  
#ifndef ODS_4
		P.HDR_ATTACHMENT_ID.NULL = FALSE;			  
		P.HDR_IMPLEMENTATION.NULL = FALSE;			 
		P.HDR_SHADOW_COUNT.NULL = FALSE;			   
#endif
		P.HDR_PAGE_SIZE = header->hdr_page_size;
		P.HDR_ODS_VERSION = header->hdr_ods_version;
		P.HDR_PAGES = header->hdr_PAGES;
		P.HDR_OLDEST_TRANS = header->hdr_oldest_transaction;
		P.HDR_OLDEST_ACTIVE = header->hdr_oldest_active;
		P.HDR_NEXT_TRANS = header->hdr_next_transaction;
		P.SEQUENCE =  header->hdr_sequence;
		P.HDR_FLAGS = header->hdr_flags;					  
		temp.gds_quad_high = header->hdr_creation_date[0];
		temp.gds_quad_low = header->hdr_creation_date[1];
		P.HDR_CREATION_DATE = temp;
#ifndef ODS_4
		P.HDR_ATTACHMENT_ID = header->hdr_attachment_id;
		P.HDR_IMPLEMENTATION  = header->hdr_implementation;
		P.HDR_SHADOW_COUNT = header->hdr_shadow_count;
#endif
		break;

 	    case pag_pages:
		pip = (PIP) page;
		P.PIP_MIN.NULL = FALSE;
		P.PIP_MIN =  pip->pip_min;
		break;

	    case pag_transactions:
		P.TIP_NEXT.NULL = FALSE;
		P.TIP_NEXT = ((TIP)page)->tip_next;
		break;

	    case pag_pointer:
		pointer = (PPG) page;
		P.PPG_NEXT.NULL = FALSE;					   
		P.PPG_COUNT.NULL = FALSE;					 
		P.PPG_MIN_SPACE.NULL = FALSE;				  
		P.PPG_MAX_SPACE.NULL = FALSE;
		P.RELATION.NULL = FALSE;				 
		P.SEQUENCE.NULL = FALSE;
		P.SEQUENCE = pointer->ppg_sequence;
		P.PPG_NEXT = pointer->ppg_next;
		P.RELATION =  pointer->ppg_relation;
		P.PPG_COUNT = pointer->ppg_count;
		P.PPG_MIN_SPACE = pointer->ppg_min_space;
		P.PPG_MAX_SPACE =  pointer->ppg_max_space; 
		break;

	    case pag_data:
		data = (DPG) page;
		P.DPG_COUNT.NULL = FALSE;
		P.RELATION.NULL = FALSE;
		P.SEQUENCE.NULL = FALSE;					  
		P.SEQUENCE = data->dpg_sequence;
		P.RELATION = data->dpg_relation;
		P.DPG_COUNT = data->dpg_count;
		break;

	    case pag_root:
 		index_root = (IRT) page;
		P.IRT_COUNT.NULL = FALSE;
		P.RELATION.NULL = FALSE;
		P.IRT_COUNT = index_root->irt_count;
		P.RELATION = index_root->irt_relation; 
		break;  

	    case pag_index:
		bucket = (BTR) page;
		P.BTR_SIBLING.NULL = FALSE;					
		P.BTR_LENGTH.NULL = FALSE;					 
		P.BTR_ID.NULL = FALSE;						 
		P.BTR_LEVEL.NULL = FALSE; 
		P.RELATION.NULL = FALSE;  
		P.BTR_SIBLING = bucket->btr_sibling;
		P.RELATION =  bucket->btr_relation;
		P.BTR_LENGTH = bucket->btr_length;
		P.BTR_ID = bucket->btr_id;
		P.BTR_LEVEL = bucket->btr_level;
		break;

	    case pag_blob:
		blob = (BLP) page;
		P.BLP_LEAD_PAGE.NULL = FALSE;				  
		P.BLP_LENGTH.NULL = FALSE;
		P.SEQUENCE.NULL = FALSE;
		P.BLP_LEAD_PAGE = blob->blp_lead_page;
		P.SEQUENCE = blob->blp_sequence;
		P.BLP_LENGTH = blob->blp_length; 
		break;	

#ifndef ODS_4
	    case pag_ids:
#endif
	    default:
		break;
	    }
	}
    END_STORE
	ON_ERROR
	    gds__print_status (gds__status);
	    ib_printf ("can't store into the documentaton database\n");
	    FINISH; 
	    return;
	END_ERROR;
    page_number++; 
    }
}
