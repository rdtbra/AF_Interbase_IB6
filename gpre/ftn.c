/*
 *	PROGRAM:	General preprocessor
 *	MODULE:		FTN.c
 *	DESCRIPTION:	Fortran text generator
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
#include "../gpre/form.h"
#include "../gpre/pat.h"
#include "../gpre/cmp_proto.h"
#include "../gpre/gpre_proto.h"
#include "../gpre/lang_proto.h"
#include "../gpre/pat_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/isc_proto.h"

extern UCHAR		fortran_labels [];
extern DBB		isc_databases;
extern REQ		requests;
extern IB_FILE		*out_file;
extern struct dbd	global_db_list [];
extern USHORT		global_db_count;

static int 	align ();
static int	asgn_from (ACT, REF);
static int	asgn_to (ACT, REF);
static int	asgn_to_proc (REF);
static int	gen_any (ACT);
static int	gen_at_end (ACT);
static int	gen_based (ACT);
static int	gen_blob_close (ACT);
static int	gen_blob_end (ACT);
static int	gen_blob_for (ACT);
static int	gen_blob_open (ACT);
static int	gen_blr (int *, int, TEXT *);
static int	gen_clear_handles(ACT);
static int	gen_compatibility_symbol( TEXT *, TEXT *, TEXT *);
static int	gen_compile (ACT);
static int	gen_create_database (ACT);
static int	gen_cursor_close (ACT, REQ);
static int	gen_cursor_init (ACT);
static int	gen_cursor_open (ACT, REQ);
static int	gen_database (ACT);
static int	gen_database_data (ACT);
static int	gen_database_decls (ACT);
static int	gen_ddl (ACT);
static int	gen_drop_database (ACT);
static int	gen_dyn_close (ACT);
static int	gen_dyn_declare (ACT);
static int	gen_dyn_describe (ACT, BOOLEAN);
static int	gen_dyn_execute (ACT);
static int	gen_dyn_fetch (ACT);
static int	gen_dyn_immediate (ACT);
static int	gen_dyn_insert (ACT);
static int	gen_dyn_open (ACT);
static int	gen_dyn_prepare (ACT);
static int	gen_emodify (ACT);
static int	gen_estore (ACT);
static int	gen_end_fetch (void);
static int	gen_endfor (ACT);
static int	gen_erase (ACT);
static SSHORT	gen_event_block (ACT);
static int	gen_event_init (ACT);
static int	gen_event_wait (ACT);
static int	gen_fetch (ACT);
static int	gen_finish (ACT);
static int	gen_for (ACT);
static int	gen_form_display (ACT);
static int	gen_form_end (ACT);
static int	gen_form_for (ACT);
static int	gen_function( ACT);
static int	gen_get_or_put_slice (ACT, REF, BOOLEAN);
static int	gen_get_segment (ACT);
static int	gen_item_end (ACT);
static int	gen_item_for (ACT);
static int	gen_loop (ACT);
static int	gen_menu (ACT);
static int	gen_menu_display (ACT);
static int	gen_menu_end (ACT);
static int	gen_menu_entree (ACT);
static int	gen_menu_entree_att (ACT);
static int	gen_menu_for (ACT);
static int	gen_menu_item_end (ACT);
static int	gen_menu_item_for (ACT);
static int	gen_menu_request (REQ);
static TEXT	*gen_name (SCHAR *, REF, BOOLEAN);
static int	gen_on_error (ACT);
static int	gen_procedure (ACT);
static int	gen_put_segment (ACT);

/* RRK_?: the following prototype is differnet from C stuff */
static int	gen_raw (UCHAR *, enum req_t, int, int, int);

static int	gen_ready (ACT);
static int	gen_receive (ACT, POR);
static int	gen_release (ACT);
static int	gen_request (REQ);
static int	gen_request_data (REQ);
static int	gen_request_decls (REQ);
static int	gen_return_value (ACT);
static int	gen_routine (ACT);
static int	gen_s_end (ACT);
static int	gen_s_fetch (ACT);
static int	gen_s_start (ACT);
static int	gen_segment (ACT);
static int	gen_select (ACT);
static int	gen_send (ACT, POR);
static int	gen_slice (ACT);
static int	gen_start (ACT, POR);
static int	gen_store (ACT);
static int	gen_t_start (ACT);
static int 	gen_tpb(TPB);
static int	gen_tpb_data (TPB);
static int	gen_tpb_decls (TPB);
static int	gen_trans (ACT);
static int	gen_type (ACT);
static int	gen_update (ACT);
static int	gen_variable (ACT);
static int	gen_whenever (SWE);
static int	gen_window_create (ACT);
static int	gen_window_delete (ACT);
static int	gen_window_suspend (ACT);
static int	make_array_declaration (REF);
static TEXT	*make_name (TEXT *, SYM);
static int	make_ok_test (ACT, REQ);
static int	make_port (POR);
static int	make_ready (DBB, TEXT *, TEXT *, REQ);
static int	next_label (void);
static int	printa (SCHAR *, SCHAR *, ...);
static int	printb (SCHAR *,  ...);
static TEXT	*request_trans (ACT, REQ);
static int	status_and_stop (ACT);
static TEXT	*status_vector (ACT);
static int	t_start_auto (REQ, TEXT *, ACT, SSHORT);

static TEXT	output_buffer [512];
static int	first_flag;
static ADL      array_decl_list;

#ifdef VMS
#define INCLUDE_ISC_FTN  "       include  'interbase:[syslib]gds.for\' \n\n"
#define DOUBLE_DCL	"DOUBLE PRECISION"
#define I2CONST_1	"%VAL("
#define I2CONST_2	")"
#define I2_1		""
#define I2_2		""
#define VAL_1		"%VAL("
#define VAL_2		")"
#define REF_1		"%REF("
#define REF_2		")"
#define I4CONST_1	"%VAL("
#define I4CONST_2	")"
#define COMMENT		"C     "
#define INLINE_COMMENT	"!"
#define COMMA		","
#define NULL_SQLDA	"%VAL(0)"
#endif

#if (defined AIX || defined AIX_PPC)
#define INCLUDE_ISC_FTN  "       INCLUDE  '%s\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL	"DOUBLE PRECISION"
#define I2CONST_1	"%VAL("
#define I2CONST_2	")"
#define I2_1		""
#define I2_2		""
#define VAL_1		"%VAL("
#define VAL_2		")"
#define REF_1		"%REF("
#define REF_2		")"
#define I4CONST_1	"%VAL("
#define I4CONST_2	")"
#define COMMENT		"C     "
#define INLINE_COMMENT	"!"
#define COMMA		","
#endif

#ifdef APOLLO
#define INCLUDE_ISC_FTN  "%%include  '%s\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.ins.ftn"
#define DOUBLE_DCL	"REAL*8"
#define I2CONST_1	"INT2("
#define I2CONST_2	")"
#define I2_1		"INT2("
#define I2_2		")"
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	"INT4("
#define I4CONST_2	")"
#define COMMENT		"C      "
#define INLINE_COMMENT	""
#define COMMA		""
#define NULL_SQLDA	"0, 0"
#endif

#ifdef sun
#define INCLUDE_ISC_FTN  "       INCLUDE  '%s\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL      "DOUBLE PRECISION"
#define I2CONST_1	""
#define I2CONST_2	""
#define I2_1		""
#define I2_2		""
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	""
#define I4CONST_2	""
#define COMMENT		"*     "
#define INLINE_COMMENT  "\n*                "
#define COMMA		","
#endif

#ifdef linux
#define INCLUDE_ISC_FTN  "       INCLUDE  '/usr/interbase/include/gds.f\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL      "DOUBLE PRECISION"
#define I2CONST_1       ""
#define I2CONST_2       ""
#define I2_1            ""
#define I2_2            ""
#define VAL_1           ""
#define VAL_2           ""
#define REF_1           ""
#define REF_2           ""
#define I4CONST_1       ""
#define I4CONST_2       ""
#define COMMENT         "*     "
#define INLINE_COMMENT  "\n*                "
#define COMMA           ","
#endif

#ifdef sgi
#define INCLUDE_ISC_FTN  "       INCLUDE  '%s\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL      "DOUBLE PRECISION"
#define I2CONST_1	""
#define I2CONST_2	""
#define I2_1		""
#define I2_2		""
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	""
#define I4CONST_2	""
#define COMMENT		"C     "
#define INLINE_COMMENT  "!"
#define COMMA		","
#endif

#ifdef DGUX
#define INCLUDE_ISC_FTN  "       INCLUDE  '%s\' \n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL      "DOUBLE PRECISION"
#define I2CONST_1	""
#define I2CONST_2	""
#define I2_1		""
#define I2_2		""
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	""
#define I4CONST_2	""
#define COMMENT		"*     "
#define INLINE_COMMENT  "! "
#define COMMA		","
#define NULL_SQLDA	"%VAL(0)"
#endif

#ifdef hpux
#define INCLUDE_ISC_FTN  "       INCLUDE  '%s\' \n\n"
#define INCLUDE_FTN_FILE "include/gds.f"
#define DOUBLE_DCL      "DOUBLE PRECISION"
#define I2CONST_1	"ISC_INT2("
#define I2CONST_2	")"
#define I2_1		"ISC_INT2("
#define I2_2		")"
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	""
#define I4CONST_2	""
#define COMMENT		"*     "
#define INLINE_COMMENT  "!"
#define COMMA		","
#endif

#ifdef mpexl
#define INCLUDE_ISC_FTN	"      INCLUDE \'gds.f.starbase\' \n\n"
#define DOUBLE_DCL	"REAL*8"
#define I2CONST_1	"INT2("
#define I2CONST_2	")"
#define I2_1		"INT2("
#define I2_2		")"
#define VAL_1		""
#define VAL_2		""
#define REF_1		""
#define REF_2		""
#define I4CONST_1	"INT4("
#define I4CONST_2	")"
#define COMMENT		"C      "
#define INLINE_COMMENT	""
#define COMMA		","
#endif

#define COLUMN		"      "
#define INDENT		"   "
#define CONTINUE	"     +   "
#define COLUMN_INDENT	"          "

#ifdef APOLLO
#define ISC_EMBED_DSQL_CLOSE 		"isc_embed_dsql_close_"
#define ISC_EMBED_DSQL_DECLARE 		"isc_embed_dsql_declare_" 
#define ISC_EMBED_DSQL_DESCRIBE 	"isc_embed_dsql_describe_" 
#define ISC_EMBED_DSQL_DESCRIBE_BIND 	"isc_embed_dsql_describe_bind_" 
#define ISC_EMBED_DSQL_EXECUTE 		"isc_embed_dsql_execute_"
#define ISC_EMBED_DSQL_EXECUTE2		"isc_embed_dsql_execute2_"
#define ISC_EMBED_DSQL_EXECUTE_IMMEDIATE "isc_embed_dsql_execute_immed_"
#define ISC_EMBED_DSQL_EXECUTE_IMMEDIATE2 "isc_embed_dsql_execute_immed2_"
#define ISC_EMBED_DSQL_FETCH		"isc_embed_dsql_fetch_"
#define ISC_EMBED_DSQL_INSERT		"isc_embed_dsql_insert_"
#define ISC_EMBED_DSQL_OPEN		"isc_embed_dsql_open_"
#define ISC_EMBED_DSQL_OPEN2		"isc_embed_dsql_open2_"
#define ISC_EMBED_DSQL_PREPARE	 	"isc_embed_dsql_prepare_"
#define ISC_DSQL_ALLOCATE		"isc_dsql_alloc_statement2_"
#define ISC_DSQL_EXECUTE		"isc_dsql_execute_m_"
#define ISC_DSQL_FREE			"isc_dsql_free_statement_"
#define ISC_DSQL_SET_CURSOR		"isc_dsql_set_cursor_name_"

#define ISC_EVENT_WAIT			"ISC_EVENT_WAIT_"
#define ISC_EVENT_COUNTS		"ISC_EVENT_COUNTS_"

#define DSQL_I2CONST_1			""
#define DSQL_I2CONST_2			""
#else
#define ISC_EMBED_DSQL_CLOSE 		"isc_embed_dsql_close" 
#define ISC_EMBED_DSQL_DECLARE 		"isc_embed_dsql_declare" 
#define ISC_EMBED_DSQL_DESCRIBE 	"isc_embed_dsql_describe" 
#define ISC_EMBED_DSQL_DESCRIBE_BIND 	"isc_embed_dsql_describe_bind" 
#define ISC_EMBED_DSQL_EXECUTE 		"isc_embed_dsql_execute"
#define ISC_EMBED_DSQL_EXECUTE2		"isc_embed_dsql_execute2"
#define ISC_EMBED_DSQL_EXECUTE_IMMEDIATE "isc_embed_dsql_execute_immed"
#define ISC_EMBED_DSQL_EXECUTE_IMMEDIATE2 "isc_embed_dsql_execute_immed2"
#define ISC_EMBED_DSQL_FETCH		"isc_embed_dsql_fetch"
#define ISC_EMBED_DSQL_INSERT		"isc_embed_dsql_insert"
#define ISC_EMBED_DSQL_OPEN		"isc_embed_dsql_open"
#define ISC_EMBED_DSQL_OPEN2		"isc_embed_dsql_open2"
#define ISC_EMBED_DSQL_PREPARE	 	"isc_embed_dsql_prepare" 
#define ISC_DSQL_ALLOCATE		"isc_dsql_alloc_statement2"
#define ISC_DSQL_EXECUTE		"isc_dsql_execute_m"
#define ISC_DSQL_FREE			"isc_dsql_free_statement"
#define ISC_DSQL_SET_CURSOR		"isc_dsql_set_cursor_name"

#define ISC_EVENT_WAIT			"ISC_EVENT_WAIT"
#define ISC_EVENT_COUNTS		"ISC_EVENT_COUNTS"

#define DSQL_I2CONST_1			I2CONST_1
#define DSQL_I2CONST_2			I2CONST_2
#endif

#ifndef NULL_SQLDA
#define NULL_SQLDA	"0"
#endif

void FTN_action (
    ACT		action,
    int		column)
{
/**************************************
 *
 *	F T N  _ a c t i o n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (action->act_flags & ACT_break)
    first_flag = FALSE;

switch (action->act_type) 
    {
    case ACT_alter_database:
    case ACT_alter_domain:
    case ACT_alter_table:
    case ACT_alter_index:
    case ACT_create_domain:
    case ACT_create_generator:
    case ACT_create_index:
    case ACT_create_shadow:
    case ACT_create_table:
    case ACT_create_view:
    case ACT_declare_filter:
    case ACT_declare_udf:
    case ACT_drop_domain:
    case ACT_drop_filter:
    case ACT_drop_index:
    case ACT_drop_shadow:
    case ACT_drop_table :
    case ACT_drop_udf:
    case ACT_statistics:
    case ACT_drop_view  : gen_ddl (action);		break;
    case ACT_any	: gen_any (action); 		return;
    case ACT_at_end	: gen_at_end (action); 		return;
    case ACT_commit	: gen_trans (action); break;
    case ACT_commit_retain_context: gen_trans (action); break;
    case ACT_basedon	: gen_based (action);		return;
    case ACT_b_declare	: gen_database (action);	return;
    case ACT_blob_cancel: gen_blob_close (action);	return;
    case ACT_blob_close	: gen_blob_close (action);	break;
    case ACT_blob_create: gen_blob_open (action); 	break;
    case ACT_blob_for	: gen_blob_for (action);	return;
    case ACT_blob_handle: gen_segment (action); 	return;
    case ACT_blob_open	: gen_blob_open (action); 	break;
    case ACT_clear_handles: gen_clear_handles (action);	break;
    case ACT_create_database: gen_create_database (action);	break;
    case ACT_cursor	: gen_cursor_init (action); 	return;
    case ACT_database	: gen_database (action); 	return;
    case ACT_disconnect : gen_finish (action); break;
    case ACT_drop_database : gen_drop_database (action);	break;
    case ACT_dyn_close		: gen_dyn_close (action);	break;
    case ACT_dyn_cursor		: gen_dyn_declare (action);	break;
    case ACT_dyn_describe	: gen_dyn_describe (action, FALSE);	break;
    case ACT_dyn_describe_input	: gen_dyn_describe (action, TRUE);	break;
    case ACT_dyn_execute	: gen_dyn_execute (action);	break;
    case ACT_dyn_fetch		: gen_dyn_fetch (action);	break;
    case ACT_dyn_grant		: gen_ddl (action);		break;
    case ACT_dyn_immediate	: gen_dyn_immediate (action);	break;
    case ACT_dyn_insert		: gen_dyn_insert (action);	break;
    case ACT_dyn_open		: gen_dyn_open (action);	break;
    case ACT_dyn_prepare	: gen_dyn_prepare (action);	break;
    case ACT_dyn_revoke 	: gen_ddl (action);     	break;
    case ACT_close	: gen_s_end (action); break;
    case ACT_endblob	: gen_blob_end (action);	return;
    case ACT_enderror	: printa (COLUMN, "END IF");	return;
    case ACT_endfor	: gen_endfor (action); break;
    case ACT_endmodify	: gen_emodify (action); break;
    case ACT_endstore	: gen_estore (action); break;
    case ACT_erase	: gen_erase (action); 		return;
    case ACT_event_init	: gen_event_init (action); 	break;
    case ACT_event_wait	: gen_event_wait (action); 	break;
    case ACT_fetch	: gen_fetch (action); break;
    case ACT_finish	: gen_finish (action); break;
    case ACT_for	: gen_for (action); 		return;
    case ACT_form_display : gen_form_display (action);	break;
    case ACT_form_end	: gen_form_end (action);	break;
    case ACT_form_for	: gen_form_for (action);	return;
    case ACT_function	: gen_function (action);	return;
    case ACT_get_segment: gen_get_segment (action);	break;
    case ACT_get_slice  : gen_slice (action);		break;
    case ACT_hctef	: gen_end_fetch();		return;
    case ACT_insert	: gen_s_start (action);		break;
    case ACT_item_for	:
    case ACT_item_put	: gen_item_for (action);	return;
    case ACT_item_end	: gen_item_end (action);	break;
    case ACT_loop	: gen_loop (action);		break;
    case ACT_menu	: gen_menu (action);		return;
    case ACT_menu_end	: gen_menu_end (action);	break;
    case ACT_menu_display : gen_menu_display (action);	return;
    case ACT_menu_for	: gen_menu_for (action);	return;

    case ACT_title_text:
    case ACT_title_length:
    case ACT_terminator:
    case ACT_entree_text:
    case ACT_entree_length:
    case ACT_entree_value:	gen_menu_entree_att (action); return;

    case ACT_menu_entree: gen_menu_entree (action);	return;
    case ACT_open	: gen_s_start (action); break;
    case ACT_on_error	: gen_on_error (action);	return;
    case ACT_procedure	: gen_procedure (action);	return;
    case ACT_put_segment: gen_put_segment (action);	break;
    case ACT_put_slice  : gen_slice (action);		break;
    case ACT_ready	: gen_ready (action); 		return;
    case ACT_release	: gen_release (action);	break;
    case ACT_rfinish	: gen_finish (action); 		return;
    case ACT_prepare	: gen_trans (action); break;
    case ACT_rollback	: gen_trans (action); break;
    case ACT_routine	: gen_routine (action);		return;
    case ACT_s_end	: gen_s_end (action);		return;
    case ACT_s_fetch	: gen_s_fetch (action); break;
    case ACT_s_start	: gen_s_start (action); break;
    case ACT_select	: gen_select (action); break;
    case ACT_segment_length: gen_segment (action);	return;
    case ACT_segment	: gen_segment (action);		return;
    case ACT_sql_dialect: sw_sql_dialect = ((SDT)action->act_object)->sdt_dialect; return;
    case ACT_start	: gen_t_start (action); break;
    case ACT_store	: gen_store (action); break;
    case ACT_store2	: gen_return_value (action); break;
    case ACT_type	: gen_type (action); return;
    case ACT_update	: gen_update (action); break;
    case ACT_variable	: gen_variable (action);	return;
    case ACT_window_create	: gen_window_create (action);	return;
    case ACT_window_delete	: gen_window_delete (action);	return;
    case ACT_window_suspend	: gen_window_suspend (action);	return;

    default :
	return;
    }

if (action->act_flags & ACT_sql)
    gen_whenever (action->act_whenever);
}

void FTN_fini (void)
{
/**************************************
 *
 *	F T N _ f i n i
 *
 **************************************
 *
 * Functional description 
 *      Create a block data module at the 
 *      head of a preprocessed fortran file
 *      containing the initializations for
 *      all databases not declared as extern
 *
 **************************************/
DBD     db_list, end;
TEXT	*name;

if (!global_db_count)
    return;

ib_fprintf (out_file, "\n");
printa (COLUMN, "BLOCK DATA");
for (db_list = global_db_list, end = global_db_list + global_db_count; 
     db_list < end; 
     db_list++)
    {
    name = db_list->dbb_name;
    ib_fprintf (out_file, "%sINTEGER*4  %s                %s{ database handle }\n",
		COLUMN, name, INLINE_COMMENT);
    ib_fprintf (out_file, "%sCOMMON /%s/ %s\n", COLUMN, name, name);
    ib_fprintf (out_file, "%sDATA %s /0/             %s{ init database handle }\n",
		COLUMN, name, INLINE_COMMENT);
    }
if (sw_pyxis && (sw_window_scope != DBB_EXTERN))
    {
    ib_fprintf (out_file, "%sINTEGER*4  ISC_WINDOW        %s{ window handle }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2  ISC_HEIGHT        %s{ window height }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2  ISC_WIDTH         %s{ window width }\n", COLUMN, INLINE_COMMENT);
    printa (COLUMN, "COMMON /ISC_WINDOW/ISC_WINDOW");
    printa (COLUMN, "COMMON /ISC_HEIGHT/ISC_HEIGHT");
    printa (COLUMN, "COMMON /ISC_WIDTH/ISC_WIDTH");
    ib_fprintf (out_file, "%sDATA ISC_WINDOW /0/          %s{ init window handle }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sDATA ISC_HEIGHT /24/         %s{ init window height }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sDATA ISC_WIDTH /80/          %s{ init window width }\n", COLUMN, INLINE_COMMENT);
    }

printa (COLUMN, "END");
}

void FTN_print_buffer (
    TEXT	*output_buffer)
{
/**************************************
 *
 *	F T N _ p r i n t _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Print a statment, breaking it into
 *      reasonable 72 character hunks.
 *
 **************************************/
TEXT	s [73], err [128], *p, *q;

p = s;

for (q = output_buffer; *q; q++)
    {
    *p++ = *q;
#ifdef sun
    if (q [0] == '\n' && q [1] == '\0')
	{
	*p = 0;
	ib_fprintf (out_file, "%s", s);
	p = s;
	}
#endif
    if ((p - s) > 71)
	{
	for (p--; (*p != ',') && (*p != ' '); p--)
	    {
	    if ((p - s) < 10)
		{
		p += 51;
		q += 50;
		*p-- = 0;
		sprintf (err, "Output line overflow: %s", s);
		CPR_error (err);
		break;
		}
	    q--;
	    }

	*++p = 0;
	ib_fprintf (out_file, "%s\n", s);
	strcpy (s, CONTINUE);
	for (p = s; *p; p++)
	    ;
	}
    } 
*p = 0;
ib_fprintf (out_file, "%s", s);
output_buffer[0] = 0;
}

/* RRK_?: copy align from c_cxx */


static asgn_from (
    ACT		action,
    REF		reference)
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
SCHAR	*value, name[64], variable[20], temp[20];

for (; reference; reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (field->fld_array_info)
	if (!(reference->ref_flags & REF_array_elem))
	    {
	    printa (COLUMN, "CALL isc_qtoq (isc_blob_null, %s)", gen_name (name, reference, TRUE));
	    gen_get_or_put_slice (action, reference, FALSE);
	    continue;
	    }

    if (!reference->ref_source && !reference->ref_value)
	continue;
    gen_name (variable, reference, TRUE);
    if (reference->ref_source)
	value = gen_name (temp, reference->ref_source, TRUE);
    else
	value = reference->ref_value;
    if (reference->ref_value && (reference->ref_flags & REF_array_elem))
	field = field->fld_array;
    if (field->fld_dtype == dtype_blob ||
	field->fld_dtype == dtype_quad ||
	field->fld_dtype == dtype_date)
	sprintf (output_buffer, "%sCALL isc_qtoq (%s, %s)\n", 
	    COLUMN, value, variable);
    else
	if (!reference->ref_master || (reference->ref_flags & REF_literal))
	    sprintf (output_buffer, "%s%s = %s\n", COLUMN, variable, value);
	else 
	    {
	    sprintf (output_buffer, "%sIF (%s .LT. 0) THEN\n", COLUMN, value);
	    FTN_print_buffer (output_buffer);
	    sprintf (output_buffer, "%s%s = -1\n", COLUMN_INDENT, variable); 
	    FTN_print_buffer (output_buffer);
	    sprintf (output_buffer, "%sELSE\n", COLUMN);
	    FTN_print_buffer (output_buffer);
	    sprintf (output_buffer, "%s%s = 0\n", COLUMN_INDENT, variable);
	    FTN_print_buffer (output_buffer);
	    sprintf (output_buffer, "%sEND IF\n", COLUMN);  
	    }
    FTN_print_buffer (output_buffer);
    }
}

static asgn_to (
    ACT		action,
    REF		reference)
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
SCHAR	s [128];
REF	source;
FLD	field;

source = reference->ref_friend;
field = source->ref_field;
if (field->fld_array_info)
   {
   source->ref_value = reference->ref_value;
   gen_get_or_put_slice (action, source, TRUE);
   return;
   }

gen_name (s, source, TRUE);

if (field->fld_dtype == dtype_blob ||
    field->fld_dtype == dtype_quad ||
    field->fld_dtype == dtype_date)
    sprintf (output_buffer, "%sCALL isc_qtoq (%s, %s)\n", 
	COLUMN, s, reference->ref_value);
else
    sprintf (output_buffer, "%s%s = %s\n", 
	    COLUMN, reference->ref_value, s);
FTN_print_buffer (output_buffer);

/* Pick up NULL value if one is there */

if (reference = reference->ref_null)
    {
    sprintf (output_buffer, "%s%s = %s\n", 
	    COLUMN, reference->ref_value, gen_name (s, reference, TRUE));
    FTN_print_buffer (output_buffer);
    }
}

static asgn_to_proc (
    REF		reference)
{
/**************************************
 *
 *	a s g n _ t o _ p r o c
 *
 **************************************
 *
 * Functional description
 *	Build an assignment to a host language variable from
 *	a port variable.
 *
 **************************************/
FLD	field;
SCHAR	s[64];

for (; reference; reference = reference->ref_next)
    {
    if (!reference->ref_value)
	continue;
    field = reference->ref_field;
    gen_name (s, reference, TRUE);
    if (field->fld_dtype == dtype_blob ||
	field->fld_dtype == dtype_quad ||
	field->fld_dtype == dtype_date)
	sprintf (output_buffer, "%sCALL isc_qtoq (%s, %s)\n", 
		COLUMN, s, reference->ref_value);
    else
	sprintf (output_buffer, "%s%s = %s\n", 
		COLUMN, reference->ref_value, s);
    FTN_print_buffer (output_buffer);
    }
}

static gen_at_end (
    ACT		action)
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
SCHAR	s[20];

request = action->act_request;
printa (COLUMN, "IF (%s .EQ. 0) THEN", gen_name (s, request->req_eof, TRUE));
ib_fprintf (out_file, COLUMN);
}

static gen_based (
    ACT		action)
{
/**************************************
 *
 *	g e n _ b a s e d
 *
 **************************************
 *
 * Functional description
 *	Substitute for a BASED ON <field name> clause.
 *
 **************************************/
BAS	based_on;
FLD	field;
TEXT	s [64], *variable, first_flag;
USHORT	datatype;
SLONG	length;
DIM	dimension;
BOOLEAN last;

based_on = (BAS) action->act_object;
field = based_on->bas_field;

if (based_on->bas_flags & BAS_segment)
    {
    datatype = dtype_text;
    if (!(length = field->fld_seg_length))
	length = 256;
    }
else if (field->fld_array_info)
    datatype = field->fld_array_info->ary_dtype;
else
    datatype = field->fld_dtype;

switch (datatype) 
    {
    case dtype_short :
	ib_fprintf (out_file, "%sINTEGER*2%s", COLUMN, COLUMN);
	break;

    case dtype_long :
	ib_fprintf (out_file, "%sINTEGER*4%s", COLUMN, COLUMN);
	break;

    case dtype_date :
    case dtype_blob :
    case dtype_quad :
	ib_fprintf (out_file, "%sINTEGER*4%s", COLUMN, COLUMN);
	break;
	
    case dtype_text :
	ib_fprintf (out_file, "%sCHARACTER*%d%s", COLUMN,
	    (based_on->bas_flags & BAS_segment) ? length :
		((field->fld_array_info) ? field->fld_array->fld_length : field->fld_length),
	    COLUMN);
	break;

    case dtype_float:
	ib_fprintf (out_file, "%sREAL%s", COLUMN, COLUMN);
	break;

    case dtype_double:
	ib_fprintf (out_file, "%s%s%s", COLUMN, DOUBLE_DCL, COLUMN);
	break;

    default :
	sprintf (s, "datatype %d unknown\n",
	    field->fld_dtype);
	CPR_error (s);
	return;
    }

/* print the first variable, then precede the rest with commas */

first_flag = TRUE;

while (based_on->bas_variables)
    {
    variable = (TEXT*) POP (&based_on->bas_variables);  
    if (!first_flag)
	ib_fprintf (out_file, ",\n%s", CONTINUE);
    ib_fprintf (out_file, "%s", variable);
    first_flag = FALSE;
    if (field->fld_array_info && !(based_on->bas_flags & BAS_segment))
	{
	/*  Print out the dimension part of the declaration  */
	last = FALSE;
	ib_fprintf (out_file, "(" );

	for (dimension = field->fld_array_info->ary_dimension; dimension;
		dimension = dimension->dim_next)
	    {
	    if (dimension->dim_lower != 1)
		ib_fprintf (out_file, "%d:", dimension->dim_lower);

	    ib_fprintf (out_file, "%d", dimension->dim_upper);
	    if (dimension->dim_next)
		ib_fprintf (out_file, ", ");
	    }

	if (field->fld_dtype == dtype_quad ||
	    field->fld_dtype == dtype_date)
	    ib_fprintf (out_file, ",2");

	ib_fprintf (out_file, ")" );
	}

    else if (field->fld_dtype == dtype_blob ||
	field->fld_dtype == dtype_quad ||
	field->fld_dtype == dtype_date)
	ib_fprintf (out_file, "(2)");
    }    

ib_fprintf (out_file, "\n");
}

static gen_blob_close (
    ACT		action)
{
/**************************************
 *
 *	g e n _ b l o b _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Make a blob FOR loop.
 *
 **************************************/
TEXT	*command;
BLB	blob;

if (action->act_flags & ACT_sql)
    {
    gen_cursor_close (action, action->act_request);
    blob = (BLB) action->act_request->req_blobs;
    }
else
    blob = (BLB) action->act_object;

command = (action->act_type == ACT_blob_cancel) ? "CANCEL" : "CLOSE";
printa (COLUMN, "CALL ISC_%s_BLOB (%s, isc_%d)",
	command,
	status_vector (action),
	blob->blb_ident);

if (action->act_flags & ACT_sql)
    {
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    }

status_and_stop (action);
}

static gen_blob_end (
    ACT		action)
{
/**************************************
 *
 *	g e n _ b l o b _ e n d
 *
 **************************************
 *
 * Functional description
 *	End a blob FOR loop.
 *
 **************************************/
BLB 	blob;

blob = (BLB) action->act_object;
printa (COLUMN, "%sGOTO %d", INDENT, blob->blb_top_label);
printa ("", "%-6dCONTINUE", blob->blb_btm_label);
if (action->act_error)
    printa (COLUMN, "CALL ISC_CANCEL_BLOB (ISC_STATUS2, isc_%d)",
	blob->blb_ident);
else
    printa (COLUMN, "CALL ISC_CANCEL_BLOB (%s, isc_%d)",
	status_vector (NULL_PTR),
	blob->blb_ident);
}

static gen_blob_for (
    ACT		action)
{
/**************************************
 *
 *	g e n _ b l o b _ f o r
 *
 **************************************
 *
 * Functional description
 *	Make a blob FOR loop.
 *
 **************************************/
BLB	blob;

blob = (BLB) action->act_object;
blob->blb_top_label = next_label();
blob->blb_btm_label = next_label();
gen_blob_open (action);
if (action->act_error)
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0) GOTO %d\n", blob->blb_btm_label);
printa ("", "%-6dCONTINUE", blob->blb_top_label);
gen_get_segment (action);
printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0 .AND. ISC_STATUS(2) .NE. ISC_SEGMENT) THEN");
printa (COLUMN, "%s GOTO %d", INDENT, blob->blb_btm_label);
printa (COLUMN, "END IF");
}

static gen_blob_open (
    ACT		action)
{
/**************************************
 *
 *	g e n _ b l o b _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate the call to open (or create) a blob.
 *
 **************************************/
USHORT   column = 6;
BLB	blob;
PAT	args;
REF	reference;
TEXT	s [20];
TEXT	*pattern1 = "CALL ISC_%IFCREATE%ELOPEN%EN_BLOB2 (%V1, %RF%DH%RE, %RF%RT%RE, %RF%BH%RE, %RF%FR%RE, %N1, %I1)\n",
	*pattern2 = "CALL ISC_%IFCREATE%ELOPEN%EN_BLOB2 (%V1, %RF%DH%RE, %RF%RT%RE, %RF%BH%RE, %RF%FR%RE, 0, 0)\n";

if (action->act_flags & ACT_sql)
    {
    if (sw_auto)
	{
	t_start_auto (action->act_request, status_vector (action), action, TRUE);
	printa (COLUMN, "if (%s .ne. 0) then",
	    request_trans (action, action->act_request));
	}

    gen_cursor_open (action, action->act_request);
    blob = (BLB) action->act_request->req_blobs;
    reference = ((OPN) action->act_object)->opn_using;
    gen_name (s, reference, TRUE);
    }
else
    {
    blob = (BLB) action->act_object;
    reference = blob->blb_reference;
    }

args.pat_condition = action->act_type == ACT_blob_create;    /*  open or create blob  */
args.pat_vector1 = status_vector (action);                   /*  status vector        */
args.pat_database = blob->blb_request->req_database;         /*  database handle      */
args.pat_request = blob->blb_request;                        /*  transaction handle   */
args.pat_blob = blob;                                        /*  blob handle          */
args.pat_reference = reference;                              /*  blob identifier      */
args.pat_ident1 = blob->blb_bpb_ident;

if ((action->act_flags & ACT_sql) && action->act_type == ACT_blob_open)
    {
    printa (COLUMN, "CALL isc_qtoq (%s, %s)", 
	reference->ref_value, s);
    }

if (args.pat_value1 = blob->blb_bpb_length)
    PATTERN_expand (column, pattern1, &args);
else
    PATTERN_expand (column, pattern2, &args);

if (action->act_flags & ACT_sql)
    {
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    if (sw_auto)
	printa (COLUMN, "END IF");
    status_and_stop (action);
    if (action->act_type == ACT_blob_create)
	{
	printa (COLUMN, "IF (SQLCODE .EQ. 0) THEN");
	printa (COLUMN_INDENT, "CALL isc_qtoq (%s, %s)", 
	    s, reference->ref_value);
	printa (COLUMN, "ENDIF");
	}
    }
else
    status_and_stop (action);
}

static gen_blr (
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
int	from, to, len, c_len;
TEXT	c;

c_len = strlen (COMMENT);
len = strlen (string);
from = 0;
to = 80 - c_len;

while (from < len)
    {
    if (to < len)
	{
	c = string [to];
	string [to] = 0;
	}
    ib_fprintf (out_file, "%s%s\n", COMMENT, &string [from]);
    if (to < len)
	string [to] = c;
    from = to;    
    to = to + 80 - c_len;
    }

return TRUE;
}

static gen_compile (
    ACT		action)
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
REQ	request;
DBB	db;
SYM	symbol;
BLB	blob;

request = action->act_request;
db = request->req_database;
symbol = db->dbb_name;

/* generate automatic ready if appropriate */

if (sw_auto)
    t_start_auto (request, status_vector (action), action, TRUE);

/* 
 * always generate a compile, a test for the success of the compile,
 * and an end to the 'if not compiled test
 */

/* generate an 'if not compiled' */

if (sw_auto && (action->act_error || (action->act_flags & ACT_sql)))
    printa (COLUMN, "IF (%s .EQ. 0 .AND. %s .NE. 0) THEN", request->req_handle,
	request_trans (action, request));
else
    printa (COLUMN, "IF (%s .EQ. 0) THEN", request->req_handle);

sprintf (output_buffer,"%sCALL ISC_COMPILE_REQUEST%s (%s, %s, %s, %s%d%s, %sisc_%d%s)\n",
	COLUMN,
	(request->req_flags & REQ_exp_hand) ? "" : "2",
	status_vector (action),
	symbol->sym_string, 
	request->req_handle,
	I2CONST_1, request->req_length, I2CONST_2,
	REF_1, request->req_ident, REF_2);
FTN_print_buffer (output_buffer);
status_and_stop (action);
printa (COLUMN, "END IF");

/* If blobs are present, zero out all of the blob handles.  After this
   point, the handles are the user's responsibility */

if (blob = request->req_blobs)
    {
    for (; blob; blob = blob->blb_next)
	{
	sprintf (output_buffer, "%sisc_%d = 0\n", COLUMN, blob->blb_ident);
	FTN_print_buffer (output_buffer);
	}
    }
}



static gen_create_database (
    ACT		action)
{
/**************************************
 *
 *	g e n _ c r e a t e _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to create a database.
 *
 **************************************/
REQ	request;
DBB	db;
USHORT	save_sw_auto;
TEXT 	s1 [32], s2 [32];

request = ((MDBB) action->act_object)->mdbb_dpb_request;
db = (DBB) request->req_database;
sprintf (s1, "isc_%dl", request->req_ident);

if (request->req_flags & REQ_extend_dpb)
    {	
    sprintf (s2, "isc_%dp", request->req_ident);
    if (request->req_length)
	{
	sprintf (output_buffer, "%s%s = isc_%d\n", 
	    COLUMN, s2, request->req_ident);
	FTN_print_buffer (output_buffer);
	}
    if (db->dbb_r_user)
	{
	sprintf (output_buffer, 
	"%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_user_name, %s, %sLEN(%s)%s)\n",
	    COLUMN,
	    s2, s1, db->dbb_r_user,  
	    I2CONST_1, db->dbb_r_user, I2CONST_2);
	FTN_print_buffer (output_buffer);
	}
    if (db->dbb_r_password)
	{
	sprintf (output_buffer, 
	"%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_password, %s, %sLEN(%s)%s)\n",
	    COLUMN,
	    s2, s1, db->dbb_r_password,  
	    I2CONST_1, db->dbb_r_password, I2CONST_2);
	FTN_print_buffer (output_buffer);
	}

	/*
	** =========================================================
	** ==   SQL Role supports GPRE/Fortran
	** =========================================================
	*/

        if (db->dbb_r_sql_role)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_sql_role_name, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_sql_role,  
	        I2CONST_1, db->dbb_r_sql_role, I2CONST_2);
            FTN_print_buffer (output_buffer);
	    }

    if (db->dbb_r_lc_messages)
	{
	sprintf (output_buffer, 
	"%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_lc_messages, %s, %sLEN(%s)%s)\n",
	    COLUMN,
	    s2, s1, db->dbb_r_lc_messages,  
	    I2CONST_1, db->dbb_r_lc_messages, I2CONST_2);
	FTN_print_buffer (output_buffer);
	}
    if (db->dbb_r_lc_ctype)
	{
	sprintf (output_buffer, 
	"%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_lc_type, %s, %sLEN(%s)%s)\n",
	    COLUMN,
	    s2, s1, db->dbb_r_lc_ctype,  
	    I2CONST_1, db->dbb_r_lc_ctype, I2CONST_2);
	FTN_print_buffer (output_buffer);
	}
    }
else
    sprintf (s2, "isc_%d", request->req_ident);

if (request->req_length || request->req_flags & REQ_extend_dpb)
    sprintf (output_buffer,
	     "%sCALL ISC_CREATE_DATABASE (%s, %s%d%s, %s'%s'%s, %s, %s%s%s, %s, 0)\n",
	     COLUMN,
	     status_vector (action),
	     I2CONST_1, strlen (db->dbb_filename), I2CONST_2,
	     REF_1, db->dbb_filename, REF_2,
	     db->dbb_name->sym_string,
	     I2CONST_1, s1, I2CONST_2,
	     s2); 
else
    sprintf (output_buffer,
	     "%sCALL ISC_CREATE_DATABASE (%s, %s%d%s, %s'%s'%s, %s, %s0%s, 0, 0)\n",
	     COLUMN,
	     status_vector (action),
	     I2CONST_1, strlen (db->dbb_filename), I2CONST_2,
	     REF_1, db->dbb_filename, REF_2,
	     db->dbb_name->sym_string,
	     I2CONST_1, I2CONST_2);
FTN_print_buffer (output_buffer);
if (request && request->req_flags & REQ_extend_dpb)
    {
    if (request->req_length) {
	sprintf (output_buffer, "%sif (%s != isc_%d)\n", COLUMN, s2, request->req_ident);
        FTN_print_buffer (output_buffer);
    }
    sprintf(output_buffer, "%sCALL ISC_FREE (%s)\n", COLUMN, s2);
    FTN_print_buffer (output_buffer);

    /* reset the length of the dpb */

    sprintf (output_buffer, "%s%s = %d\n", COLUMN, s1, request->req_length);
    FTN_print_buffer (output_buffer);
    }
save_sw_auto = sw_auto;
sw_auto = TRUE;
printa (COLUMN, "IF (isc_status(2) .eq. 0) THEN");
gen_ddl (action);
printa (COLUMN, "END IF");
sw_auto = save_sw_auto;
status_and_stop (action);
}

static gen_cursor_close (
    ACT		action,
    REQ		request)
{
/**************************************
 *
 *	g e n _ c u r s o r _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for END_STREAM.
 *
 **************************************/

printa (COLUMN, "IF (isc_%ds .NE. 0) THEN", request->req_ident);
printa (COLUMN, "CALL %s (%s, isc_%ds, %s1%s)",
	ISC_DSQL_FREE,
	status_vector (action),
	request->req_ident,
	DSQL_I2CONST_1, DSQL_I2CONST_2);
printa (COLUMN, "IF (isc_status(2) .EQ. 0) THEN");
}

static gen_cursor_init (
    ACT		action)
{
/**************************************
 *
 *	g e n _ c u r s o r _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Generate text to initialize a cursor.
 *
 **************************************/

/* If blobs are present, zero out all of the blob handles.  After this
   point, the handles are the user's responsibility */

if (action->act_request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
    printa (COLUMN, "isc_%d = 0", action->act_request->req_blobs->blb_ident);
}

static gen_cursor_open (
    ACT		action,
    REQ		request)
{
/**************************************
 *
 *	g e n _ c u r s o r _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate text to open an embedded SQL cursor.
 *
 **************************************/
TEXT	s [64];

if (action->act_type != ACT_open)
    {
    if (sw_auto)
	printa (COLUMN, "IF (isc_%ds .EQ. 0 .AND. %s .NE. 0) THEN",
	    request->req_ident,
	    request->req_database->dbb_name->sym_string);
    else
	printa (COLUMN, "IF (isc_%ds .EQ. 0) THEN",
	    request->req_ident);
    }
else
    {
    if (sw_auto)
	printa (COLUMN, "IF (isc_%ds .EQ. 0 .AND. %s .NE. 0 .AND. %s .NE. 0) THEN",
	    request->req_ident, request->req_handle,
	    request->req_database->dbb_name->sym_string);
    else
	printa (COLUMN, "IF (isc_%ds .EQ. 0 .AND. %s .NE. 0) THEN",
	    request->req_ident, request->req_handle);
    }

printa (COLUMN, "CALL %s (%s, %s, isc_%ds)",
    ISC_DSQL_ALLOCATE,
    status_vector (action),
    request->req_database->dbb_name->sym_string,
    request->req_ident);
printa (COLUMN, "END IF");

if (sw_auto)
    printa (COLUMN, "IF (isc_%ds .NE. 0 .AND. %s .NE. 0) THEN",
	request->req_ident, request_trans (action, request));
else
    printa (COLUMN, "IF (isc_%ds .NE. 0) THEN",
	request->req_ident);
printa (COLUMN, "CALL %s (%s, isc_%ds, %s, %s0%s)",
    ISC_DSQL_SET_CURSOR,
    status_vector (action),
    request->req_ident,
    make_name (s, ((OPN) action->act_object)->opn_cursor),
    DSQL_I2CONST_1, DSQL_I2CONST_2);
printa (COLUMN, "IF (isc_status(2) .EQ. 0) THEN");
printa (COLUMN, "CALL %s (%s, %s, isc_%ds, %s0%s, 0, %s-1%s, %s0%s, 0)",
    ISC_DSQL_EXECUTE,
    status_vector (action),
    request_trans (action, request),
    request->req_ident,
    DSQL_I2CONST_1, DSQL_I2CONST_2,
    DSQL_I2CONST_1, DSQL_I2CONST_2,
    DSQL_I2CONST_1, DSQL_I2CONST_2);
printa (COLUMN, "IF (isc_status(2) .EQ. 0) THEN");
}

static gen_database (
    ACT		action)
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

if (first_flag++ != 0)
    return;

sprintf (output_buffer, "\n%s      **** GDS Preprocessor Definitions ****\n\n", COMMENT);
FTN_print_buffer (output_buffer);

gen_database_decls (action);
gen_database_data (action);

printa (COMMENT, "**** end of GPRE definitions ****\n");
} 

static gen_database_data (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d a t a b a s e _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Generate insertion text for global DATA statements.
 *
 **************************************/
DBB		db;
TPB		tpb;
REQ		request;
BOOLEAN		any_extern;
TEXT		include_buffer[512];

#ifndef mpexl
ISC_prefix (include_buffer, INCLUDE_FTN_FILE);
sprintf (output_buffer, INCLUDE_ISC_FTN, include_buffer);
#else
sprintf (output_buffer, INCLUDE_ISC_FTN);
#endif

FTN_print_buffer (output_buffer);

any_extern = FALSE;
for (db = isc_databases; db; db = db->dbb_next)
    {
#ifndef FTN_BLK_DATA
    if (db->dbb_scope != DBB_EXTERN)
	ib_fprintf (out_file, "%sDATA %s /0/               %s{ init database handle }\n", 
	    COLUMN, db->dbb_name->sym_string, INLINE_COMMENT);
    else
	any_extern = TRUE;
#endif
    for (tpb = db->dbb_tpbs; tpb; tpb = tpb->tpb_dbb_next)
	gen_tpb_data (tpb);
    }

ib_fprintf (out_file, "%sDATA ISC_NULL /0/            %s{ init null vector }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sDATA ISC_BLOB_NULL /0,0/     %s{ init null blob }\n", COLUMN, INLINE_COMMENT);
#ifndef FTN_BLK_DATA
if (!any_extern)
    ib_fprintf (out_file, "%sDATA GDS__TRANS /0/           %s{ init trans handle }\n", COLUMN, INLINE_COMMENT);
if (sw_pyxis && (sw_window_scope != DBB_EXTERN))
    {
    ib_fprintf (out_file, "%sDATA ISC_WINDOW /0/           %s{ init window handle }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sDATA ISC_HEIGHT /24/           %s{ init window height }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sDATA ISC_WIDTH /80/           %s{ init window width }\n", COLUMN, INLINE_COMMENT);
    }
#endif

for (request = requests; request; request = request->req_next)
    gen_request_data (request);
} 

static gen_database_decls (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d a t a b a s e _ d e c l s
 *
 **************************************
 *
 * Functional description
 *	Generate insertion text for global
 *      data declarations.
 *
 **************************************/
DBB		db;
REQ		request;
POR		port;
TPB		tpb;
BLB		blob;
TEXT		*name;
BOOLEAN		all_static, dcl_ndx_var;
REF		reference;
SSHORT		count, max_count, length, i, index;
LLS		stack_ptr;

ib_fprintf (out_file, "%sINTEGER*4  ISC_BLOB_NULL(2)  %s{ null blob handle }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  GDS__TRANS         %s{ default transaction handle }\n", COLUMN, INLINE_COMMENT);
if (sw_pyxis)
    {
    ib_fprintf (out_file, "%sINTEGER*4  ISC_WINDOW        %s{ window handle }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2  ISC_HEIGHT        %s{ window height }\n", COLUMN, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2  ISC_WIDTH         %s{ window width }\n", COLUMN, INLINE_COMMENT);
    }
ib_fprintf (out_file, "%sINTEGER*4  ISC_STATUS(20)    %s{ status vector }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  ISC_STATUS2(20)   %s{ status vector }\n", COLUMN, INLINE_COMMENT);

/* added for 3.3 compatibility */
ib_fprintf (out_file, "%sINTEGER*4  GDS__STATUS(20)    %s{ status vector }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  GDS__STATUS2(20)   %s{ status vector }\n", COLUMN, INLINE_COMMENT);

printa (COLUMN, "EQUIVALENCE    (ISC_STATUS(20), GDS__STATUS(20)) ");
printa (COLUMN, "EQUIVALENCE    (ISC_STATUS2(20), GDS__STATUS2(20)) ");
/* end of code added for 3.3 compatibility */

ib_fprintf (out_file, "%sINTEGER*4  ISC_NULL          %s{ dummy status vector }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  SQLCODE            %s{ SQL status code }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  ISC_SQLCODE       %s{ SQL status code translator }\n", COLUMN, INLINE_COMMENT);
ib_fprintf (out_file, "%sINTEGER*4  ISC_ARRAY_LENGTH  %s{ array return size }\n", COLUMN, INLINE_COMMENT);

all_static = TRUE;
dcl_ndx_var = FALSE;
for (db = isc_databases, count = 0; db; db = db->dbb_next)
    {
    all_static = all_static && (db->dbb_scope == DBB_STATIC);
    name = db->dbb_name->sym_string;
    ib_fprintf (out_file, "%sINTEGER*4  %s                %s{ database handle }\n",
	COLUMN, name, INLINE_COMMENT);
    
    ib_fprintf (out_file, "%sCHARACTER*256 ISC_%s        %s{ database file name }\n",
	COLUMN, name, INLINE_COMMENT);

    for (tpb = db->dbb_tpbs; tpb; tpb = tpb->tpb_dbb_next)
	{
	gen_tpb_decls (tpb);
	dcl_ndx_var = TRUE;
	}

#ifdef hpux 
    /* build fields to handle start_multiple */

    count++;
    ib_fprintf (out_file, "%sINTEGER*4      ISC_TEB%d_DBB   %s( vector db handle )\n", COLUMN, count, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*4      ISC_TEB%d_LEN   %s( vector tpb length )\n", COLUMN, count, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*4      ISC_TEB%d_TPB   %s( vector tpb handle )\n", COLUMN, count, INLINE_COMMENT);
#endif

    printa (COLUMN, "COMMON /%s/ %s", name, name);
    }

#ifdef hpux
/* declare array and set up equivalence for start_multiple vector */

length = 12;
ib_fprintf (out_file, "%sCHARACTER      ISC_TEB(%d)  %s( transaction vector )\n",
    COLUMN, length * count, INLINE_COMMENT);
for (i = 0; i < count; )
    { 
    index = i++ * length + 1;   
    printa (COLUMN, "EQUIVALENCE    (ISC_TEB(%d), ISC_TEB%d_DBB )",
	index, i);
    printa (COLUMN, "EQUIVALENCE    (ISC_TEB(%d), ISC_TEB%d_LEN )",
	index + 4, i);
    printa (COLUMN, "EQUIVALENCE    (ISC_TEB(%d) ,ISC_TEB%d_TPB )",
	index + 8, i);
    }
#endif

if (!all_static)
    {
    printa (COLUMN, "COMMON /GDS__TRANS/GDS__TRANS");
    printa (COLUMN, "COMMON /ISC_STATUS/ISC_STATUS");
    printa (COLUMN, "COMMON /ISC_STATUS2/ISC_STATUS2");
    printa (COLUMN, "COMMON /SQLCODE/SQLCODE");
    }
if (sw_pyxis && sw_window_scope != DBB_STATIC)
    {
    printa (COLUMN, "COMMON /ISC_WINDOW/ISC_WINDOW");
    printa (COLUMN, "COMMON /ISC_HEIGHT/ISC_HEIGHT");
    printa (COLUMN, "COMMON /ISC_WIDTH/ISC_WIDTH");
    }
    
array_decl_list = NULL;
for (request = requests; request; request = request->req_next)
    {
    gen_request_decls (request);
    for (port = request->req_ports; port; port = port->por_next)
	make_port (port);
    for (blob = request->req_blobs; blob; blob = blob->blb_next)
	{
	ib_fprintf (out_file, "%sINTEGER*4 isc_%d         %s{ blob handle }\n",
	    COLUMN, blob->blb_ident, INLINE_COMMENT);
	ib_fprintf (out_file, "%sCHARACTER*%d isc_%d      %s{ blob segment }\n",
	    COLUMN, blob->blb_seg_length, blob->blb_buff_ident, INLINE_COMMENT);
	ib_fprintf (out_file, "%sINTEGER*2 isc_%d         %s{ segment length }\n",
	    COLUMN, blob->blb_len_ident, INLINE_COMMENT);
	}

    /*  Array declarations  */

    if (port = request->req_primary)
	for (reference = port->por_references; reference;
	    reference = reference->ref_next)
	    if (reference->ref_field->fld_array_info)
		make_array_declaration (reference);
    }

/* Declare DATA statement index variable */

if (dcl_ndx_var || requests)
    printa (COLUMN, "INTEGER ISC_I");

/* generate event parameter block for each event in module */

max_count = 0;
for (stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next)
    {
    count = gen_event_block (stack_ptr->lls_object);
    max_count = MAX (count, max_count);
    }

if (max_count)
    {
    ib_fprintf (out_file, "%sINTEGER*4  ISC_EVENTS(%d)         %s{ event vector }\n", COLUMN, max_count, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*4  ISC_EVENT_NAMES(%d)    %s{ event buffer }\n", COLUMN, max_count, INLINE_COMMENT);
    ib_fprintf (out_file, "%sCHARACTER*31 ISC_EVENT_NAMES2(%d) %s{ event string buffer }\n", COLUMN, max_count, INLINE_COMMENT);
    }
} 
 
static gen_ddl (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d d l
 *
 **************************************
 *
 * Functional description
 *	Generate a call to update metadata.
 *
 **************************************/
REQ	request;

if (sw_auto)
    {
    t_start_auto (NULL_PTR, status_vector (action), action, TRUE);
    printa (COLUMN, "if (gds__trans .ne. 0) then");
    }

/* Set up command type for call to RDB_DDL */

request = action->act_request;

sprintf (output_buffer, "%sCALL isc_ddl (%s, %s, gds__trans, %s%d%s, isc_%d)\n",
    COLUMN,
    status_vector (action),
    request->req_database->dbb_name->sym_string,
    I2CONST_1, request->req_length, I2CONST_2,
    request->req_ident);

FTN_print_buffer(output_buffer);

if (sw_auto)
    {
    printa (COLUMN, "END IF");
    printa (COLUMN, "if (isc_status(2) .eq. 0)");
    printa (CONTINUE, "CALL isc_commit_transaction (%s, gds__trans)",
	status_vector (action));
    printa (COLUMN, "if (isc_status(2) .ne. 0)");
    printa (CONTINUE, "CALL isc_rollback_transaction (isc_status2, gds__trans)");     
    }

status_and_stop (action);
}

static gen_drop_database (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d r o p _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to create a database.
 *
 **************************************/
DBB	db;

db = (DBB) action->act_object;

sprintf (output_buffer,
	"%s CALL ISC_DROP_DATABASE (%s, %s%d%s, %s\'%s\'%s, RDB_K_DB_TYPE_GDS)\n",
	COLUMN,
	status_vector (action),
	I2_1, strlen (db->dbb_filename), I2_2,
	REF_1, db->dbb_filename, REF_2);
FTN_print_buffer (output_buffer);
status_and_stop (action);
}

static gen_dyn_close (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ c l o s e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s [64];

statement = (DYN) action->act_object;
printa (COLUMN, 
    "CALL %s (isc_status, %s)",
    ISC_EMBED_DSQL_CLOSE,
    make_name (s, statement->dyn_cursor_name));
status_and_stop (action);
}

static gen_dyn_declare (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ d e c l a r e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s1 [64], s2 [64];

statement = (DYN) action->act_object;
printa (COLUMN, 
    "CALL %s (isc_status, %s, %s)",
    ISC_EMBED_DSQL_DECLARE,
    make_name (s1, statement->dyn_statement_name),
    make_name (s2, statement->dyn_cursor_name));
status_and_stop (action);
}

static gen_dyn_describe (
    ACT		action,
    BOOLEAN	bind_flag)
{
/**************************************
 *
 *	g e n _ d y n _ d e s c r i b e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s [64];

statement = (DYN) action->act_object;
printa (COLUMN, 
    "CALL %s (isc_status, %s, %s%d%s, %s)",
    bind_flag ? ISC_EMBED_DSQL_DESCRIBE_BIND : ISC_EMBED_DSQL_DESCRIBE,
    make_name (s, statement->dyn_statement_name),
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    statement->dyn_sqlda);

status_and_stop (action);
}

static gen_dyn_execute (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
TEXT		*transaction, s1 [64], s2 [64], s3 [64], *sqlda, *sqlda2;
struct req	*request, req_const;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds__trans";
    request = NULL;
    }

if (sw_auto)
    {
    t_start_auto (request, status_vector (action), action, TRUE);
    printa (COLUMN, "if (%s .ne. 0) then", transaction);
    }

sqlda = statement->dyn_sqlda;
sqlda2 = statement->dyn_sqlda2;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "%s, 1", sqlda2);
    sqlda2 = s3;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "isc_baddress (%s)", sqlda2);
    sqlda2 = s3;
    }
#endif

printa (COLUMN, 
    (sqlda2) ?
	"CALL %s (isc_status, %s, %s, %s%d%s, %s, %s)" :
	"CALL %s (isc_status, %s, %s, %s%d%s, %s)",
    (sqlda2) ? ISC_EMBED_DSQL_EXECUTE2 : ISC_EMBED_DSQL_EXECUTE,
    transaction,
    make_name (s1, statement->dyn_statement_name),
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA,
    (sqlda2) ? sqlda2 : NULL_SQLDA);

if (sw_auto)
    printa (COLUMN, "END IF");

status_and_stop (action);
}

static gen_dyn_fetch (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s1 [64], s2 [64], *sqlda;

statement = (DYN) action->act_object;

sqlda = statement->dyn_sqlda;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
#endif

printa (COLUMN, 
    "SQLCODE = %s (isc_status, %s, %s%d%s, %s)",
    ISC_EMBED_DSQL_FETCH,
    make_name (s1, statement->dyn_cursor_name),
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA);

printa (COLUMN, "IF (SQLCODE .NE. 100) SQLCODE = ISC_SQLCODE (isc_status)");
}

static gen_dyn_immediate (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ i m m e d i a t e
 *
 **************************************
 *
 * Functional description
 *	Generate code for an EXECUTE IMMEDIATE dynamic SQL statement.
 *
 **************************************/
DYN		statement;
DBB		database;
TEXT		*transaction, s2 [64], s3 [64], *sqlda, *sqlda2;
struct req	*request, req_const;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds__trans";
    request = NULL;
    }

database = statement->dyn_database;

if (sw_auto)
    {
    t_start_auto (request, status_vector (action), action, TRUE);
    printa (COLUMN, "if (%s .ne. 0) then", transaction);
    }

sqlda = statement->dyn_sqlda;
sqlda2 = statement->dyn_sqlda2;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "%s, 1", sqlda2);
    sqlda2 = s3;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "isc_baddress (%s)", sqlda2);
    sqlda2 = s3;
    }
#endif

printa (COLUMN, 
    (sqlda2) ?
	"CALL %s (isc_status, %s, %s, %sLEN(%s)%s, %s%s%s, %s%d%s, %s, %s)" :
	"CALL %s (isc_status, %s, %s, %sLEN(%s)%s, %s%s%s, %s%d%s, %s)",
    (sqlda2) ? ISC_EMBED_DSQL_EXECUTE_IMMEDIATE2 : ISC_EMBED_DSQL_EXECUTE_IMMEDIATE,
    transaction,
    database->dbb_name->sym_string,
    DSQL_I2CONST_1, statement->dyn_string, DSQL_I2CONST_2,
    REF_1, statement->dyn_string, REF_2,
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA,
    (sqlda2) ? sqlda2 : NULL_SQLDA);

if (sw_auto)
    printa (COLUMN, "END IF");

status_and_stop (action);
}

static gen_dyn_insert (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ i n s e r t
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN	statement;
TEXT	s1 [64], s2 [64], *sqlda;

statement = (DYN) action->act_object;

sqlda = statement->dyn_sqlda;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
#endif

printa (COLUMN, 
    "%s (isc_status, %s, %s%d%s, %s)",
    ISC_EMBED_DSQL_INSERT,
    make_name (s1, statement->dyn_cursor_name),
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA);

status_and_stop (action);
}

static gen_dyn_open (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ o p e n
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
TEXT		*transaction, s1 [64], s2 [64], s3 [64], *sqlda, *sqlda2;
struct req	*request, req_const;

statement = (DYN) action->act_object;
if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds__trans";
    request = NULL;
    }

if (sw_auto)
    {
    t_start_auto (request, status_vector (action), action, TRUE);
    printa (COLUMN, "if (%s .ne. 0) then", transaction);
    }

sqlda = statement->dyn_sqlda;
sqlda2 = statement->dyn_sqlda2;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "%s, 1", sqlda2);
    sqlda2 = s3;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
if (sqlda2)
    {
    sprintf (s3, "isc_baddress (%s)", sqlda2);
    sqlda2 = s3;
    }
#endif

printa (COLUMN, 
    (sqlda2) ?
	"CALL %s (isc_status, %s, %s, %s%d%s, %s, %s)" :
	"CALL %s (isc_status, %s, %s, %s%d%s, %s)",
    (sqlda2) ? ISC_EMBED_DSQL_OPEN2 : ISC_EMBED_DSQL_OPEN,
    transaction,
    make_name (s1, statement->dyn_cursor_name),
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA,
    (sqlda2) ? sqlda2 : NULL_SQLDA);

if (sw_auto)
    printa (COLUMN, "END IF");

status_and_stop (action);
}

static gen_dyn_prepare (
    ACT		action)
{
/**************************************
 *
 *	g e n _ d y n _ p r e p a r e
 *
 **************************************
 *
 * Functional description
 *	Generate a dynamic SQL statement.
 *
 **************************************/
DYN		statement;
DBB		database;
TEXT		*transaction, s1 [64], s2 [64], *sqlda;
struct req	*request, req_const;

statement = (DYN) action->act_object;
database = statement->dyn_database;

if (statement->dyn_trans)
    {
    transaction = statement->dyn_trans;
    request = &req_const;
    request->req_trans = transaction;
    }
else
    {
    transaction = "gds__trans";
    request = NULL;
    }

if (sw_auto)
    {
    t_start_auto (request, status_vector (action), action, TRUE);
    printa (COLUMN, "if (%s .ne. 0) then", transaction);
    }

sqlda = statement->dyn_sqlda;
#ifdef APOLLO
if (sqlda)
    {
    sprintf (s2, "%s, 1", sqlda);
    sqlda = s2;
    }
#endif
#ifdef hpux
if (sqlda)
    {
    sprintf (s2, "isc_baddress (%s)", sqlda);
    sqlda = s2;
    }
#endif

printa (COLUMN, "CALL %s (isc_status, %s, %s, %s, %sLEN(%s)%s, %s%s%s, %s%d%s, %s)",
    ISC_EMBED_DSQL_PREPARE,
    database->dbb_name->sym_string,
    transaction,
    make_name (s1, statement->dyn_statement_name),
    DSQL_I2CONST_1, statement->dyn_string, DSQL_I2CONST_2,
    REF_1, statement->dyn_string, REF_2,
    DSQL_I2CONST_1, sw_sql_dialect, DSQL_I2CONST_2,
    (sqlda) ? sqlda : NULL_SQLDA);

if (sw_auto)
    printa (COLUMN, "END IF");

status_and_stop (action);
}

static gen_emodify (
    ACT		action)
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
SCHAR	s1[20], s2[20];

modify = (UPD) action->act_object;

for (reference= modify->upd_port->por_references; reference; 
    reference = reference->ref_next)	
    {
    if (!(source = reference->ref_source))
	continue;
    field = reference->ref_field;
    gen_name (s1, source, TRUE);
    gen_name (s2, reference, TRUE);
    if (field->fld_dtype == dtype_blob ||
	field->fld_dtype == dtype_quad ||
	field->fld_dtype == dtype_date)
	sprintf (output_buffer, "%sCALL isc_qtoq (%s, %s)\n", COLUMN, s1, s2);
    else
	sprintf (output_buffer, "%s%s = %s\n", COLUMN, s2, s1);
    FTN_print_buffer (output_buffer);
    if (field->fld_array_info)
	gen_get_or_put_slice (action, reference, FALSE);
    }

gen_send (action, modify->upd_port);
}

static gen_estore (
    ACT		action)
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

/* if this is a store...returning_values (aka store2)
   we already executed the store, so go home quietly */

if (request->req_type == REQ_store2)
    return;

gen_start (action, request->req_primary);
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "END IF");
}

static gen_end_fetch (void)
{
/**************************************
 *
 *	g e n _ e n d _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate an END IF for the IF generated for
 *	the AT_END clause.
 *
 **************************************/

printa (COLUMN, "END IF");
}

static gen_endfor (
    ACT		action)
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

if (request->req_sync)
    gen_send (action, request->req_sync);

printa (COLUMN, "GOTO %d", request->req_top_label);
printa ("", "%-6dCONTINUE", request->req_btm_label);
}

static gen_erase (
    ACT		action)
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
gen_send (action, erase->upd_port);
}

static SSHORT gen_event_block (
    ACT		action)
{
/**************************************
 *
 *	g e n _ e v e n t _ b l o c k
 *
 **************************************
 *
 * Functional description
 *	Generate event parameter blocks for use
 *	with a particular call to isc_event_wait.
 *
 **************************************/
NOD	init, list;
int	ident;

init = (NOD) action->act_object;

ident = CMP_next_ident();
init->nod_arg [2] = (NOD) ident;

printa (COLUMN, "INTEGER*4      isc_%dA", ident);
printa (COLUMN, "INTEGER*4      isc_%dB", ident);
printa (COLUMN, "INTEGER*2      isc_%dL", ident);

list = init->nod_arg [1];

return list->nod_count;
}

static gen_event_init (
    ACT		action)
{
/**************************************
 *
 *	g e n _ e v e n t _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for EVENT_INIT.
 *
 **************************************/
NOD	init, event_list, *ptr, *end, node;
REF	reference;
PAT	args;
SSHORT	column = 6, count;
TEXT	variable [20];
#if (!defined AIX && !defined AIX_PPC)
TEXT	*pattern1 = "ISC_%N1L = ISC_EVENT_BLOCK_A (%RFISC_%N1A%RE, %RFISC_%N1B%RE, %VF%S3%N2%S4%VE, %RFISC_EVENT_NAMES%RE)";
#else
TEXT	*pattern1 = "CALL ISC_EVENT_BLOCK_S (%RFISC_%N1A%RE, %RFISC_%N1B%RE, %VF%S3%N2%S4%VE, %RFISC_EVENT_NAMES%RE, %RFISC_%N1L%RE)";
#endif
TEXT	*pattern2 = "CALL %S1 (%V1, %RF%DH%RE, %VFISC_%N1L%VE, %VFISC_%N1A%VE, %VFISC_%N1B%VE)";
TEXT	*pattern3 = "CALL %S2 (ISC_EVENTS, %VFISC_%N1L%VE, %VFISC_%N1A%VE, %VFISC_%N1B%VE)";

init = (NOD) action->act_object;
event_list = init->nod_arg [1];

args.pat_database = (DBB) init->nod_arg [3];
args.pat_vector1 = status_vector (action);
args.pat_value1 = (int) init->nod_arg [2];
args.pat_value2 = (int) event_list->nod_count;
args.pat_string1 = ISC_EVENT_WAIT;
args.pat_string2 = ISC_EVENT_COUNTS;
args.pat_string3 = I2_1;
args.pat_string4 = I2_2;

/* generate call to dynamically generate event blocks */

for (ptr = event_list->nod_arg, count = 0, end = ptr + event_list->nod_count; ptr < end; ptr++)
    {
    count++;
    node = *ptr;
    if (node->nod_type == nod_field)
	{
	reference = (REF) node->nod_arg [0];
	gen_name (variable, reference, TRUE);
	printa (COLUMN, "ISC_EVENT_NAMES2(%d) = %s", count, variable);
	}
    else
	printa (COLUMN, "ISC_EVENT_NAMES2(%d) = %s", count, node->nod_arg[0]);

#if (!defined AIX && !defined AIX_PPC)
    printa (COLUMN, "ISC_EVENT_NAMES(%d) = ISC_BADDRESS (%sISC_EVENT_NAMES2(%d)%s)",
	count, REF_1, count, REF_2);
#else
    printa (COLUMN, "CALL ISC_BADDRESS (%sISC_EVENT_NAMES2(%d)%s, ISC_EVENT_NAMES(%d))",
	REF_1, count, REF_2, count);
#endif
    }

PATTERN_expand (column, pattern1, &args);

/* generate actual call to event_wait */

PATTERN_expand (column, pattern2, &args);

/* get change in event counts, copying event parameter block for reuse */

PATTERN_expand (column, pattern3, &args);
status_and_stop (action);      
}

static gen_event_wait (
    ACT		action)
{
/**************************************
 *
 *	g e n _ e v e n t _ w a i t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for EVENT_WAIT.
 *
 **************************************/
PAT	args;
NOD	event_init;
SYM	event_name, stack_name;
DBB	database;
LLS	stack_ptr;
ACT	event_action;
SSHORT	column = 6;
int	ident;
TEXT	s [64];
TEXT	*pattern1 = "CALL %S1 (%V1, %RF%DH%RE, %VFISC_%N1L%VE, %VFISC_%N1A%VE, %VFISC_%N1B%VE)";
TEXT	*pattern2 = "CALL %S2 (ISC_EVENTS, %VFISC_%N1L%VE, %VFISC_%N1A%VE, %VFISC_%N1B%VE)"; 

event_name = (SYM) action->act_object;

/* go through the stack of events, checking to see if the
   event has been initialized and getting the event identifier */

ident = -1;
for (stack_ptr = events; stack_ptr; stack_ptr = stack_ptr->lls_next)
    {
    event_action = (ACT) stack_ptr->lls_object;
    event_init = (NOD) event_action->act_object;
    stack_name = (SYM) event_init->nod_arg [0];
    if (!strcmp (event_name->sym_string, stack_name->sym_string))
	{
	ident = (int) event_init->nod_arg [2];
	database = (DBB) event_init->nod_arg [3];
	}
    }

if (ident < 0)
    {
    sprintf (s, "event handle \"%s\" not found", event_name->sym_string);
    return IBERROR (s);
    }

args.pat_database = database;
args.pat_vector1 = status_vector (action);
args.pat_value1 = (int) ident;
args.pat_string1 = ISC_EVENT_WAIT;
args.pat_string2 = ISC_EVENT_COUNTS;

/* generate calls to wait on the event and to fill out the events array */

PATTERN_expand (column, pattern1, &args);
PATTERN_expand (column, pattern2, &args);
status_and_stop (action);
}

static gen_fetch (
    ACT		action)
{
/**************************************
 *
 *	g e n _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Generate replacement text for the SQL FETCH statement.  The
 *	epilog FETCH statement is handled by GEN_S_FETCH (generate
 *	stream fetch).
 *
 **************************************/
REQ	request;
NOD	var_list;
int	i;
SCHAR	s[20];

request = action->act_request;
if (request->req_sync)
    {
    gen_send (action, request->req_sync);
    printa (COLUMN, "IF (SQLCODE .EQ. 0) THEN");
    }

gen_receive (action, request->req_primary);
printa (COLUMN, "IF (%s .NE. 0) THEN", gen_name (s, request->req_eof, TRUE));
printa (COLUMN, "SQLCODE = 0");
if (var_list = (NOD) action->act_object)
   for (i = 0; i < var_list->nod_count; i++)
	{
	asgn_to (action, var_list->nod_arg[i]);
	}
printa (COLUMN, "ELSE");
printa (COLUMN, "SQLCODE = 100");
printa (COLUMN, "END IF");

if (request->req_sync)
    printa (COLUMN, "END IF");
}  

static gen_finish (
    ACT		action)
{
/**************************************
 *
 *	g e n _ f i n i s h
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for FINISH
 *
 **************************************/
DBB	db;
RDY	ready;

db = NULL;

if (sw_auto  || ((action->act_flags & ACT_sql) &&
		(action->act_type != ACT_disconnect)))
    {
    printa (COLUMN, "IF (GDS__TRANS .NE. 0) THEN");
    printa (COLUMN, "    CALL ISC_%s_TRANSACTION (%s, GDS__TRANS)",
	(action->act_type != ACT_rfinish) ? "COMMIT" : "ROLLBACK",
	status_vector (action));
    status_and_stop (action);
    printa (COLUMN, "END IF");
    }

/* the user supplied one or more db_handles */

for (ready = (RDY) action->act_object; ready; ready = ready->rdy_next)
    {
    db = ready->rdy_database;
    printa (COLUMN, "IF (%s .NE. 0) THEN", db->dbb_name->sym_string);
    printa (COLUMN, "CALL ISC_DETACH_DATABASE (%s, %s)",
	    status_vector (action),
	    db->dbb_name->sym_string);
    status_and_stop (action);
    printa (COLUMN, "END IF");
    }

if (!db)
    for (db = isc_databases; db; db  = db->dbb_next)
	{
	if ((action->act_error || (action->act_flags & ACT_sql)) && (db != isc_databases))
	    printa (COLUMN, "IF (%s .NE. 0 .AND. ISC_STATUS(2) .EQ. 0) THEN", db->dbb_name->sym_string);
	else
	    printa (COLUMN, "IF (%s .NE. 0) THEN", db->dbb_name->sym_string);
	printa (COLUMN, "CALL ISC_DETACH_DATABASE (%s, %s)",
		status_vector (action),
		db->dbb_name->sym_string);
	status_and_stop (action);
	printa (COLUMN, "END IF");
	}
}

static gen_for (
    ACT		action)
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
POR	port;
REQ	request;
SCHAR	s[20];
REF     reference;

gen_s_start (action);
request = action->act_request;
request->req_top_label = next_label();
request->req_btm_label = next_label();
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0) GOTO %d\n", request->req_btm_label);

printa ("", "%-6dCONTINUE", request->req_top_label );
gen_receive (action, request->req_primary);
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "IF (%s .EQ. 0 .OR. ISC_STATUS(2) .NE. 0) GOTO %d\n", 
	   gen_name (s, request->req_eof, TRUE),
	   request->req_btm_label);
else
    printa (COLUMN, "IF (%s .EQ. 0) GOTO %d\n", 
	   gen_name (s, request->req_eof, TRUE),
	   request->req_btm_label);

if (port = action->act_request->req_primary)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_flags & REF_fetch_array)
	    gen_get_or_put_slice (action, reference, TRUE);
}

static gen_form_display (
    ACT		action)
{
/**************************************
 *
 *	g e n _ f o r m _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form interaction.
 *
 **************************************/
FINT	display;
REQ	request;
REF	reference, master;
POR	port;
DBB	dbb;
TEXT	s [32], out [16];
int	code;

display = (FINT) action->act_object;
request = display->fint_request;
dbb = request->req_database;
port = request->req_ports;

/* Initialize field options */

for (reference = port->por_references; reference;
     reference = reference->ref_next)
    if ((master = reference->ref_master) &&
	(code = CMP_display_code (display, master)) >= 0)
	printa (COLUMN, "%s = %d", gen_name (s, reference, TRUE), code);

if (display->fint_flags & FINT_no_wait)
    strcpy (out, "0");
else
    sprintf (out, "isc_%d", port->por_ident);

printa (COLUMN, "CALL PYXIS__DRIVE_FORM (isc_status, %s, %s, isc_window, %s, %sisc_%d%s, %s%s%s)",
	dbb->dbb_name->sym_string,
	request->req_trans,
	request->req_handle, 
	REF_1, port->por_ident, REF_2,
	REF_1, out, REF_2);
/*
status_and_stop (action);
*/
}

static gen_form_end (
    ACT		action)
{
/**************************************
 *
 *	g e n _ f o r m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form block.
 *
 **************************************/

printa (COLUMN, "CALL PYXIS__POP_WINDOW (isc_window)");
}

static gen_form_for (
    ACT		action)
{
/**************************************
 *
 *	g e n _ f o r m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate code for a form block.
 *
 **************************************/
REQ	request;
FORM	form;
DBB	dbb;

request = action->act_request;
form = request->req_form;
dbb = request->req_database;

/* Get database attach and transaction started */

if (sw_auto)
    t_start_auto (NULL_PTR, status_vector (action), action, TRUE);

/* Get form loaded first */

printa (COLUMN, "IF (%s .EQ. 0) THEN", request->req_form_handle);
printa (COLUMN_INDENT, "CALL PYXIS__LOAD_FORM (isc_status, %s, %s, %s, %s%d%s, %s'%s'%s)",
    dbb->dbb_name->sym_string,
    request->req_trans,
    request->req_form_handle,
    I2_1, strlen (form->form_name->sym_string), I2_2,
    REF_1, form->form_name->sym_string, REF_2);
status_and_stop (action);
printa (COLUMN, "END IF");
 
/* Get map compiled */

printa (COLUMN, "IF (%s .EQ. 0) THEN", request->req_handle);
printa (COLUMN_INDENT, "CALL PYXIS__COMPILE_MAP (isc_status, %s, %s, %s%d%s, isc_%d)",
    request->req_form_handle,
    request->req_handle,
    I2_1, request->req_length, I2_2,
    request->req_ident);
/*
status_and_stop (action);
*/
printa (COLUMN, "END IF");

/* Reset form to known state */

printa (COLUMN, "CALL PYXIS__RESET_FORM (isc_status, %s)",
    request->req_handle);
/*
status_and_stop (action);
*/
}

static gen_get_or_put_slice (
    ACT		action,
    REF		reference,
    BOOLEAN	get)
{
/**************************************
 *
 *	g e n _ g e t _ o r _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Generate a call to isc_get_slice
 *      or isc_put_slice for an array.
 *
 **************************************/
TEXT    s[25];

if (!(reference->ref_flags & REF_fetch_array))
    return;

if (get)
    {
    if (action->act_flags & ACT_sql)
	{
	sprintf (output_buffer,
	    "%sCALL ISC_GET_SLICE (%s, %s, %s, %s, %s%d%s, isc_%d, %s0%s, %s0%s, %s%d%s, %s, ISC_ARRAY_LENGTH)\n",
	    COLUMN,
	    status_vector (action),
	    action->act_request->req_database->dbb_name->sym_string,
	    action->act_request->req_trans,
	    gen_name (s, reference, TRUE),
	    I2CONST_1, reference->ref_sdl_length, I2CONST_2,
	    reference->ref_sdl_ident,
	    I2CONST_1, I2CONST_2,
	    I2CONST_1, I2CONST_2,
	    I4CONST_1, reference->ref_field->fld_array_info->ary_size, I4CONST_2,
	    reference->ref_value);
	}
    else
	{
	sprintf (output_buffer,
	    "%sCALL ISC_GET_SLICE (%s, %s, %s, %s, %s%d%s, isc_%d, %s0%s, %s0%s, %s%d%s, isc_%d, ISC_ARRAY_LENGTH)\n",
	    COLUMN,
	    status_vector (action),
	    action->act_request->req_database->dbb_name->sym_string,
	    action->act_request->req_trans,
	    gen_name (s, reference, TRUE),
	    I2CONST_1, reference->ref_sdl_length, I2CONST_2,
	    reference->ref_sdl_ident,
	    I2CONST_1, I2CONST_2,
	    I2CONST_1, I2CONST_2,
	    I4CONST_1, reference->ref_field->fld_array_info->ary_size, I4CONST_2,
	    reference->ref_field->fld_array_info->ary_ident);
	}
    }
else
    {
    if (action->act_flags & ACT_sql)
	{
	sprintf (output_buffer,
	    "%sCALL ISC_PUT_SLICE (%s, %s, %s, %s, %s%d%s, isc_%d, %s0%s, %s0%s, %s%d%s, %s)\n",
	    COLUMN,
	    status_vector (action),
	    action->act_request->req_database->dbb_name->sym_string,
	    action->act_request->req_trans,
	    gen_name (s, reference, TRUE),
	    I2CONST_1, reference->ref_sdl_length, I2CONST_2,
	    reference->ref_sdl_ident,
	    I2CONST_1, I2CONST_2,
	    I2CONST_1, I2CONST_2,
	    I4CONST_1, reference->ref_field->fld_array_info->ary_size, I4CONST_2,
	    reference->ref_value);
	}
    else
	{
	sprintf (output_buffer,
	    "%sCALL ISC_PUT_SLICE (%s, %s, %s, %s, %s%d%s, isc_%d, %s0%s, %s0%s, %s%d%s, isc_%d)\n",
	    COLUMN,
	    status_vector (action),
	    action->act_request->req_database->dbb_name->sym_string,
	    action->act_request->req_trans,
	    gen_name (s, reference, TRUE),
	    I2CONST_1, reference->ref_sdl_length, I2CONST_2,
	    reference->ref_sdl_ident,
	    I2CONST_1, I2CONST_2,
	    I2CONST_1, I2CONST_2,
	    I4CONST_1, reference->ref_field->fld_array_info->ary_size, I4CONST_2,
	    reference->ref_field->fld_array_info->ary_ident);
	}
    }

FTN_print_buffer (output_buffer);
}

static gen_get_segment (
    ACT		action)
{
/**************************************
 *
 *	g e n _ g e t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate the code to do a get segment.
 *
 **************************************/
BLB	blob;
REF	into;

if (action->act_flags & ACT_sql)
    blob = (BLB) action->act_request->req_blobs;
else
    blob = (BLB) action->act_object;

sprintf (output_buffer, 
    "%sISC_STATUS(2) = ISC_GET_SEGMENT (%s, isc_%d, isc_%d, %sLEN(isc_%d)%s, %sisc_%d%s)\n",
    COLUMN,
    status_vector (action),
    blob->blb_ident,
    blob->blb_len_ident,
    I2CONST_1, blob->blb_buff_ident, I2CONST_2,
    REF_1,blob->blb_buff_ident, REF_2);

FTN_print_buffer (output_buffer);

if (action->act_flags & ACT_sql)
    {
    status_and_stop (action);
    printa (COLUMN, "IF (SQLCODE .EQ. 0 .OR. SQLCODE .EQ. 101) THEN");
    into = action->act_object;
    printa (COLUMN_INDENT, "%s = isc_%d", into->ref_value, blob->blb_buff_ident);
    if (into->ref_null_value)
	printa (COLUMN_INDENT, "%s = isc_%d", into->ref_null_value, blob->blb_len_ident);
    printa (COLUMN, "ENDIF");
    }
else if (!action->act_error)
    {
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0 .AND. ISC_STATUS(2) .NE. ISC_SEGMENT");
    printa (CONTINUE, ".AND. ISC_STATUS(2) .NE. ISC_SEGSTR_EOF) THEN");
    printa (COLUMN, "    CALL ISC_PRINT_STATUS (ISC_STATUS)"); 
    printa (COLUMN, "    STOP");
    printa (COLUMN, "END IF");
    }
}

static gen_item_end (
    ACT		action)
{
/**************************************
 *
 *	g e n _ i t e m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate end of block for PUT_ITEM and FOR_ITEM.
 *
 **************************************/
REQ	request;
REF	reference, master;
POR	port;
DBB	dbb;
USHORT	value;
TEXT	s [32], index [16];

request = action->act_request;

if (request->req_type == REQ_menu)
    {
    gen_menu_item_end (action);
    return;
    }

if (action->act_pair->act_type == ACT_item_for)
    {
    gen_name (index, request->req_index, TRUE);
    printa (COLUMN_INDENT, "%s = %s + 1", index, index);
    printa (COLUMN, "GOTO %d", request->req_top_label);
    printa ("", "%-6dCONTINUE", request->req_btm_label);
    return;
    }

dbb = request->req_database;
port = request->req_ports;

/* Initialize field options */

for (reference = port->por_references; reference;
     reference = reference->ref_next)
    if (master = reference->ref_master)
	printa (COLUMN_INDENT, "%s = %d", gen_name (s, reference, TRUE), PYXIS__OPT_DISPLAY);

printa (COLUMN,
    "CALL PYXIS__INSERT (isc_status, %s, %s, %s, %sisc_%d%s)",
	dbb->dbb_name->sym_string,
	request->req_trans,
	request->req_handle, 
	REF_1, port->por_ident, REF_2);
/*
status_and_stop (action);
*/
}

static gen_item_for (
    ACT		action)
{
/**************************************
 *
 *	g e n _ i t e m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate insert text for FOR_ITEM and PUT_ITEM.
 *
 **************************************/
REQ	request, parent;
FORM	form;
TEXT	index [30];

request = action->act_request;

if (request->req_type == REQ_menu)
    {
    gen_menu_item_for (action);
    return;
    }

form = request->req_form;
parent = form->form_parent;

/* Get map compiled */

printa (COLUMN, "IF (%s .EQ. 0) THEN", request->req_handle);
printa (COLUMN_INDENT, "CALL PYXIS__COMPILE_SUB_MAP (isc_status, %s, %s, %s%d%s, isc_%d)",
    parent->req_handle,
    request->req_handle,
    I2_1, request->req_length, I2_2,
    request->req_ident);
/*
status_and_stop (action);
*/
printa (COLUMN, "END IF");

if (action->act_type != ACT_item_for)
    return;

/* Build stuff for item loop */

gen_name (index, request->req_index, TRUE);
printa (COLUMN, "%s = 1", index);
request->req_top_label = next_label();
request->req_btm_label = next_label();
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0) GOTO %d\n", request->req_btm_label);

printa ("", "%-6dCONTINUE", request->req_top_label );
printa (COLUMN,
    "CALL PYXIS__FETCH (isc_status, %s, %s, %s, %sisc_%d%s)",
	request->req_database->dbb_name->sym_string,
	request->req_trans,
	request->req_handle, 
	REF_1, request->req_ports->por_ident, REF_2);
/*
status_and_stop (action);
*/
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "IF (%s .EQ. 0 .OR. ISC_STATUS(2) .NE. 0) GOTO %d\n", 
	   index, request->req_btm_label);
else
    printa (COLUMN, "IF (%s .EQ. 0) GOTO %d\n", 
	   index, request->req_btm_label);
}

static gen_loop (
    ACT		action)
{
/**************************************
 *
 *	g e n _ l o o p
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
TEXT	name [20];

gen_s_start (action);
request = action->act_request;
port = request->req_primary;
printa (COLUMN, "IF (SQLCODE .EQ. 0) THEN");
gen_receive (action, port);
gen_name (name, port->por_references, TRUE);
printa (COLUMN, "IF (SQLCODE .EQ. 0 .AND. %s .EQ. 0) ", name);
printa (CONTINUE, "SQLCODE = 100");
printa (COLUMN, "END IF");
}

static gen_menu (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ	request;

request = action->act_request;
printa (COLUMN, "%ss = pyxis__menu (isc_window, %s, %s%d%s, isc_%d)",
    request->req_handle,
    request->req_handle,
    I2_1, request->req_length, I2_2,
    request->req_ident);
}

static gen_menu_display (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ d i s p l a y
 *
 **************************************
 *
 * Functional description
 *	Generate code for a menu interaction.
 *
 **************************************/
MENU		menu;
REQ		request, display_request;

request = action->act_request;
display_request = (REQ) action->act_object;
 
menu = NULL;

for (action = request->req_actions; action; action = action->act_next)
    if (action->act_type == ACT_menu_for)
	{
	menu = (MENU) action->act_object;
	break;
	}

printa (COLUMN,
    "CALL PYXIS__DRIVE_MENU (isc_window, %s, %s%d%s, isc_%d,",
	request->req_handle, 
	I2_1, display_request->req_length, I2_2,
	display_request->req_ident);

printa (CONTINUE,
     "isc_%dl, %sisc_%d%s,",
	menu->menu_title,
	REF_1, menu->menu_title, REF_2);

printa (CONTINUE,
	"isc_%d, isc_%dl, %sisc_%d%s, isc_%d)",
	menu->menu_terminator,
	menu->menu_entree_entree,
	REF_1, menu->menu_entree_entree, REF_2,
	menu->menu_entree_value);
}

static gen_menu_end (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ e n d
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

REQ	request;

request = action->act_request;
if (request->req_flags & REQ_menu_for)
    return;

printa (COLUMN, "END IF");
}

static gen_menu_entree (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ e n t r e e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ	request;

request = action->act_request;

if (action->act_flags & ACT_first_entree)
    printa (COLUMN, "IF (%ss .EQ. %d) THEN", request->req_handle, action->act_count);
else
    printa (COLUMN, "ELSE IF (%ss .EQ. %d) THEN", request->req_handle, action->act_count);
}

static gen_menu_entree_att (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ e n t r e e _ a t t
 *
 **************************************
 *
 * Functional description
 *
 *   Generate code for a reference to a menu or entree attribute.
 **************************************/
MENU	menu;
SSHORT	ident;
TEXT	*length;

menu = (MENU) action->act_object;

length = "";
switch (action->act_type)
    {
    case ACT_entree_text: 		ident = menu->menu_entree_entree;	break;
    case ACT_entree_length: 		ident = menu->menu_entree_entree;	length = "l";	break;
    case ACT_entree_value:	 	ident = menu->menu_entree_value;	break;
    case ACT_title_text: 		ident = menu->menu_title;		break;
    case ACT_title_length:	 	ident = menu->menu_title;		length = "l";	break;
    case ACT_terminator:	 	ident = menu->menu_terminator;		break;
    default:	ident = -1;	break;
    }

printa ("","%sisc_%d%s",
    (action->act_flags & ACT_first) ? COLUMN : CONTINUE,
    ident, length);

ib_fputs (CONTINUE, out_file );
}

static gen_menu_for (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate code for a menu block.
 *
 **************************************/
REQ	request;

request = action->act_request;

/* Get menu created */

if (!(request->req_flags & REQ_exp_hand))
    printa (COLUMN, "CALL PYXIS__INITIALIZE_MENU (%s)",
	request->req_handle);
}

static gen_menu_item_end (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ i t e m _ e n d
 *
 **************************************
 *
 * Functional description
 *	Generate end of block for PUT_ITEM and FOR_ITEM
 *	for a dynamic menu.
 *
 **************************************/
REQ	request;
ENTREE	entree;

entree = (ENTREE) action->act_pair->act_object;
request = entree->entree_request;

if (action->act_pair->act_type == ACT_item_for)
    {
    printa (COLUMN, "GOTO %d", request->req_top_label);
    printa ("", "%-6dCONTINUE", request->req_btm_label);
    return;
    }

printa (COLUMN,
    "CALL PYXIS__PUT_ENTREE (%s, isc_%dl, %sisc_%d%s, isc_%d)",
	request->req_handle,
	entree->entree_entree,
	REF_1, entree->entree_entree, REF_2,
	entree->entree_value);
}

static gen_menu_item_for (
    ACT		action)
{
/**************************************
 *
 *	g e n _ m e n u _ i t e m _ f o r
 *
 **************************************
 *
 * Functional description
 *	Generate insert text for FOR_ITEM and PUT_ITEM
 *	for a dynamic menu.
 *
 **************************************/
ENTREE	entree;
REQ	request;

if (action->act_type != ACT_item_for)
    return;

/* Build stuff for item loop */

entree = (ENTREE) action->act_object;
request = entree->entree_request;

request->req_top_label = next_label();
request->req_btm_label = next_label();

printa ("", "%-6dCONTINUE", request->req_top_label );
printa (COLUMN,
    "CALL PYXIS__GET_ENTREE (%s, isc_%dl, %sisc_%d%s, isc_%d, %sisc_%d%s)",
	request->req_handle,
	entree->entree_entree,
	REF_1, entree->entree_entree, REF_2,
	entree->entree_value,
	REF_1, entree->entree_end, REF_2);

printa (COLUMN, "IF (isc_%d .NE. 0) GOTO %d\n", 
	   entree->entree_end, request->req_btm_label);
}

static gen_menu_request (
    REQ	request)
{
/**************************************
 *
 *	g e n _ m e n u _ r e q u e s t
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a dynamic menu request.
 *
 **************************************/
ACT		action;
MENU		menu;
ENTREE		entree;

menu = NULL;
entree = NULL;

for (action = request->req_actions; action; action = action->act_next)
    {
    if (action->act_type == ACT_menu_for)
	{
	menu = (MENU) action->act_object;
	break;
	} 
    else if ((action->act_type == ACT_item_for) || (action->act_type == ACT_item_put))
	{
	entree = (ENTREE) action->act_object;
	break;
	}
    }

if (menu)
    {
    menu->menu_title = CMP_next_ident();
    menu->menu_terminator = CMP_next_ident();
    menu->menu_entree_value = CMP_next_ident();
    menu->menu_entree_entree = CMP_next_ident();
    ib_fprintf (out_file, "%sINTEGER*2 isc_%dl    %s{ TITLE_LENGTH }\n",
		COLUMN, menu->menu_title, INLINE_COMMENT);
    ib_fprintf (out_file, "%sCHARACTER*81 isc_%d    %s{ TITLE_TEXT }\n",
		COLUMN, menu->menu_title, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2 isc_%d    %s{ TERMINATOR }\n",
		COLUMN, menu->menu_terminator, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2 isc_%dl    %s{ ENTREE_LENGTH }\n",
		COLUMN, menu->menu_entree_entree, INLINE_COMMENT);
    ib_fprintf (out_file, "%sCHARACTER*81 isc_%d    %s{ ENTREE_TEXT }\n",
		COLUMN, menu->menu_entree_entree, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*4 isc_%d    %s{ ENTREE_VALUE }\n\n",
		COLUMN, menu->menu_entree_value, INLINE_COMMENT);
    }

if (entree)
    {
    entree->entree_entree = CMP_next_ident();
    entree->entree_value = CMP_next_ident();
    entree->entree_end = CMP_next_ident();
    ib_fprintf (out_file, "%sINTEGER*2 isc_%dl    %s{ ENTREE_LENGTH }\n",
		COLUMN, entree->entree_entree, INLINE_COMMENT);
    ib_fprintf (out_file, "%sCHARACTER*81 isc_%d    %s{ ENTREE_TEXT }\n",
		COLUMN, entree->entree_entree, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*4 isc_%d    %s{ ENTREE_VALUE }\n",
		COLUMN, entree->entree_value, INLINE_COMMENT);
    ib_fprintf (out_file, "%sINTEGER*2 isc_%d    %s{ }\n\n",
		COLUMN, entree->entree_end, INLINE_COMMENT);
    }
}

static TEXT *gen_name (
    SCHAR	*string,
    REF		reference,
    BOOLEAN     as_blob)
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

if (reference->ref_field->fld_array_info && !as_blob)
    sprintf (string, "isc_%d",
	reference->ref_field->fld_array_info->ary_ident);
else
    sprintf (string, "isc_%d", reference->ref_ident);

return string;
}

static gen_on_error (
    ACT		action)
{
/**************************************
 *
 *	g e n _ o n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Generate a block to handle errors.
 *
 **************************************/
ACT err_action;

err_action = (ACT) action->act_object;
if ((err_action->act_type == ACT_get_segment) || 
    (err_action->act_type == ACT_put_segment) ||
    (err_action->act_type == ACT_endblob))
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0 .AND. ISC_STATUS(2) .NE. ISC_SEGMENT .AND. ISC_STATUS(2) .NE. ISC_SEGSTR_EOF) THEN");
else
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0) THEN");
}

static gen_procedure (
    ACT		action)
{
/**************************************
 *
 *	g e n _ p r o c e d u r e
 *
 **************************************
 *
 * Functional description
 *	Generate code for an EXECUTE PROCEDURE.
 *
 **************************************/
PAT     args;
TEXT    *pattern;
REQ	request;
POR	in_port, out_port;
DBB	dbb;
USHORT	column = 6;

request = action->act_request;
in_port = request->req_vport;
out_port = request->req_primary;

dbb = request->req_database;
args.pat_database = dbb;
args.pat_request = action->act_request;
args.pat_vector1 = status_vector (action);
args.pat_request = request;
args.pat_port = in_port;
args.pat_port2 = out_port;
if (in_port && in_port->por_length)
    pattern = "CALL ISC_TRANSACT_REQUEST (%V1, %RF%DH%RE, %RF%RT%RE, %VF%RS%VE, %RF%RI%RE, %VF%PL%VE, %RF%PI%RE, %VF%QL%VE, %RF%QI%RE)\n";
else
    pattern = "CALL ISC_TRANSACT_REQUEST (%V1, %RF%DH%RE, %RF%RT%RE, %VF%RS%VE, %RI, %VF0%VE, 0, %VF%QL%VE, %RF%QI%RE)\n";

/* Get database attach and transaction started */

if (sw_auto)
    t_start_auto (NULL_PTR, status_vector (action), action, TRUE);

/* Move in input values */

asgn_from (action, request->req_values);

/* Execute the procedure */

PATTERN_expand (column, pattern, &args);

status_and_stop (action);

printa (COLUMN, "IF (SQLCODE .EQ. 0) THEN");

/* Move out output values */

asgn_to_proc (request->req_references);
printa (COLUMN, "END IF");
}

static gen_put_segment (
    ACT		action)
{
/**************************************
 *
 *	g e n _ p u t _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Generate the code to do a put segment.
 *
 **************************************/
BLB	blob;
REF	from;

if (action->act_flags & ACT_sql)
    {
    blob = (BLB) action->act_request->req_blobs;
    from = action->act_object;
    printa (COLUMN, "isc_%d = %s", blob->blb_len_ident, from->ref_null_value);
    printa (COLUMN, "isc_%d = %s", blob->blb_buff_ident, from->ref_value);
    }
else
    blob = (BLB) action->act_object;

sprintf (output_buffer, 
    "%sISC_STATUS(2) = ISC_PUT_SEGMENT (%s, isc_%d, %sisc_%d%s, %sisc_%d%s)\n",
    COLUMN,
    status_vector (action),
    blob->blb_ident,
    VAL_1, blob->blb_len_ident, VAL_2,
    REF_1, blob->blb_buff_ident, REF_2);
FTN_print_buffer (output_buffer);

status_and_stop (action);
}

static gen_raw (
    UCHAR	*blr,
    enum req_t	request_type,
    int		request_length,
    int		begin_c,
    int		end_c)
{
/**************************************
 *
 *	g e n _ r a w
 *
 **************************************
 *
 * Functional description
 *	Generate BLR in raw, numeric form.  Ughly but dense.
 *
 **************************************/
TEXT	buffer[80], *p;
UCHAR	*c;
int	blr_length;
union	{
    UCHAR	bytewise_blr[4];
    SLONG	longword_blr;
}	blr_hunk;

blr = blr + begin_c;
blr_length = end_c - begin_c + 1;
p = buffer;

while (blr_length)
    {
    for (c = blr_hunk.bytewise_blr, blr_hunk.longword_blr = 0; 
	 c < blr_hunk.bytewise_blr + sizeof(SLONG) ; c++)
 	{
	if (--blr_length)
	    *c = *blr++;
	else
	    {
	    if (request_type == REQ_slice)
		*c = isc_sdl_eoc;
	    else if ((request_type == REQ_ddl) || 
		(request_type == REQ_create_database) || 
		(request_type == REQ_form) || 
		(request_type == REQ_menu) || 
		(request_length != end_c + 1))
   		*c = *blr++;
	    else
  		*c = blr_eoc;
	    break;
	    }
	}
    if (blr_length)
	sprintf (p, "%d,", blr_hunk.longword_blr);
    else
	sprintf (p, "%d", blr_hunk.longword_blr);
    while (*p)
	p++;
    if (p - buffer > 50)
	{
	ib_fprintf (out_file, "%s%s\n", CONTINUE, buffer);
	p = buffer;
	*p = 0;
	}
    }

ib_fprintf (out_file, "%s%s/\n", CONTINUE, buffer);
}

static gen_ready (
    ACT		action)
{
/**************************************
 *
 *	g e n _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for READY
 *
 **************************************/
RDY	ready;
DBB	db;
TEXT	*filename, *vector;

vector = status_vector (action);

for (ready = (RDY) action->act_object; ready; ready = ready->rdy_next)
    {
    db = ready->rdy_database;
    if (!(filename = ready->rdy_filename))
	filename = db->dbb_runtime;
    if (action->act_error && (ready != (RDY) action->act_object))
	printa (COLUMN, "IF (ISC_STATUS(2) .EQ. 0) THEN");
    make_ready (db, filename, vector, ready->rdy_request);
    status_and_stop (action);
    if (action->act_error && (ready != (RDY) action->act_object))
	printa (COLUMN, "END IF");
    }
}

static gen_receive (
    ACT		action,
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
REQ	request;

request = action->act_request;

sprintf (output_buffer, "%sCALL ISC_RECEIVE (%s, %s, %s%d%s, %s%d%s, %sisc_%d%s, %s%s%s)\n",
    COLUMN,
    status_vector (action),
    request->req_handle,
    I2CONST_1, port->por_msg_number, I2CONST_2,
    I2CONST_1, port->por_length, I2CONST_2,
    REF_1, port->por_ident, REF_2,
    VAL_1, request->req_request_level, VAL_2);

FTN_print_buffer (output_buffer);

status_and_stop (action);
}

static gen_release (
    ACT		action)
{
/**************************************
 *
 *	g e n _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for RELEASE_REQUESTS
 *      For active databases, call isc_release_request.
 *      for all others, just zero the handle.  For the
 *      release request calls, ignore error returns, which
 *      are likely if the request was compiled on a database
 *      which has been released and re-readied.  If there is
 *      a serious error, it will be caught on the next statement.
 *
 **************************************/
DBB	db, exp_db;
REQ	request;

exp_db = (DBB) action->act_object;

for (request = requests; request; request = request->req_next)
    {
    db = request->req_database;
    if (exp_db && db  != exp_db)
	continue;
    if (!(request->req_flags & REQ_exp_hand))
	{
	printa (COLUMN, "IF %s", db->dbb_name->sym_string);
	printa (CONTINUE, "CALL ISC_RELEASE_REQUEST (ISC_STATUS, %S)",
		request->req_handle);
	printa (COLUMN, "%s = 0", request->req_handle);
	}
    }
}

static gen_request_data (
    REQ	request)
{
/**************************************
 *
 *	g e n _ r e q u e s t _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/
POR	port;
REF     reference;
BLB     blob;
TEXT	*string_type;
int	begin_i, end_i;

/* requests are generated as raw BLR in longword chunks 
   because FORTRAN is a miserable excuse for a language
   and doesn't allow byte value assignments to character
   fields. */

if (!(request->req_flags & (REQ_exp_hand | REQ_menu_for_item |
			    REQ_sql_blob_open | REQ_sql_blob_create)) &&
	 request->req_type != REQ_slice && request->req_type != REQ_procedure)
    ib_fprintf (out_file, "%sDATA %s /0/               %s{ init request handle }\n\n", 
	COLUMN, request->req_handle, INLINE_COMMENT);

if (request->req_type == REQ_form)
    ib_fprintf (out_file, "%sDATA %s /0/               %s{ init form handle }\n\n",
	COLUMN, request->req_form_handle, INLINE_COMMENT);

if (request->req_flags & (REQ_sql_blob_open | REQ_sql_blob_create))
    ib_fprintf (out_file, "%sDATA isc_%dS /0/             %s{ init SQL statement handle }\n\n", 
	COLUMN, request->req_ident, INLINE_COMMENT);

if (request->req_flags & REQ_sql_cursor)
    ib_fprintf (out_file, "%sDATA isc_%dS /0/             %s{ init SQL statement handle }\n\n", 
	COLUMN, request->req_ident, INLINE_COMMENT);

/*  Changed termination test in for-loop from <= to < to fix bug#840.
    We were generating data statements with bad bounds on the last data
    statement if the data size was divisible by 75.  mao 4/3/89
*/
if ((request->req_type == REQ_ready) ||
    (request->req_type == REQ_create_database))
    if (request->req_length || request->req_flags & REQ_extend_dpb)
        {
	ib_fprintf (out_file, "%sDATA isc_%dl /%d/               %s{ request length }\n\n", 
	    COLUMN, request->req_ident, request->req_length, INLINE_COMMENT);
	}
    
if (request->req_length)
    {
    for (begin_i = 0;
	 begin_i < request->req_length;
	 begin_i = begin_i + (75 * sizeof(SLONG)))
	{
	end_i = MIN(request->req_length - 1, begin_i + (75 * sizeof(SLONG)) - 1);
	printa (COLUMN, "DATA (isc_%d(ISC_I)%s ISC_I=%d,%d)  /", 
		request->req_ident,
		COMMA,
		(begin_i / sizeof(SLONG)) + 1,
		(end_i / sizeof(SLONG)) + 1);
	gen_raw (request->req_blr, request->req_type, request->req_length,
		 begin_i, end_i);
	}
    if (!(sw_raw))
	{
	printa (COMMENT, " ");
	printa (COMMENT, "FORMATTED REQUEST BLR FOR isc_%d = ", request->req_ident);
	switch (request->req_type)
	    {
	    case REQ_create_database:	
	    case REQ_ready:
		string_type = "DPB";
		if (PRETTY_print_cdb (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during parameter generation");
		break;

	    case REQ_ddl:	
		string_type = "DYN";
		if (PRETTY_print_dyn (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during dynamic DDL generation");
		break;

	    case REQ_form:
		string_type = "form map";
		if (PRETTY_print_form_map (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during form map generation");
		break;

	    case REQ_menu:
		string_type = "menu";
		if (PRETTY_print_menu (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during menu generation");
		break;

 	    case REQ_slice:
		string_type = "SDL";
		 if (PRETTY_print_sdl (request->req_blr, gen_blr, NULL_PTR, 0))
		     IBERROR ("internal error during SDL generation");
 		break;
 
	    default:
		string_type = "BLR";
		if (isc_print_blr (request->req_blr, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during BLR generation");
	    }
	}
    else
	{
	switch (request->req_type)
	    {
	    case REQ_create_database:	
	    case REQ_ready:
		string_type = "DPB";
		break;

	    case REQ_ddl:	
		string_type = "DYN";
		break;

	    case REQ_form:
		string_type = "form map";
		break;

	    case REQ_menu:
		string_type = "menu";
		break;

 	    case REQ_slice:
		string_type = "SDL";
 		break;

	    default:
		string_type = "BLR";
	    }
	}
    printa (COMMENT, " ");
    printa (COMMENT, "END OF %s STRING FOR REQUEST isc_%d\n",
	    string_type, request->req_ident);
    }

/*  Print out slice description language if there are arrays associated with request  */

for (port = request->req_ports; port; port = port->por_next)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_sdl)
	    {
	    for (begin_i = 0;
		 begin_i < reference->ref_sdl_length;
		 begin_i = begin_i + (75 * sizeof(SLONG)))
		{
		end_i = MIN(reference->ref_sdl_length - 1, begin_i + (75 * sizeof(SLONG)) - 1);
		printa (COLUMN, "DATA (isc_%d(ISC_I)%s ISC_I=%d,%d)  /", 
			reference->ref_sdl_ident,
			COMMA,
			(begin_i / sizeof(SLONG)) + 1,
			(end_i / sizeof(SLONG)) + 1);
		gen_raw (reference->ref_sdl, REQ_slice, reference->ref_sdl_length, begin_i, end_i);
		}
	    if (!(sw_raw))
		{
		printa (COMMENT, " ");
		if (PRETTY_print_sdl (reference->ref_sdl, gen_blr, NULL_PTR, 0))
		    IBERROR ("internal error during SDL generation");
		printa (COMMENT, " ");
		printa (COMMENT, "END OF SDL STRING FOR REQUEST isc_%d\n", reference->ref_sdl_ident);
		}
	}

/* Print out any blob parameter blocks required */

for (blob = request->req_blobs; blob; blob = blob->blb_next)
    if (blob->blb_bpb_length)
	{
	for (begin_i = 0;
	     begin_i < blob->blb_bpb_length;
	     begin_i = begin_i + (75 * sizeof(SLONG)))
	    {
	    end_i = MIN(blob->blb_bpb_length - 1, begin_i + (75 * sizeof(SLONG)) - 1);
	    printa (COLUMN, "DATA (isc_%d(ISC_I)%s ISC_I=%d,%d)  /", 
		    blob->blb_bpb_ident,
		    COMMA,
		    (begin_i / sizeof(SLONG)) + 1,
		    (end_i / sizeof(SLONG)) + 1);
	    gen_raw (blob->blb_bpb, REQ_for, blob->blb_bpb_length, begin_i, end_i);
	    printa (COMMENT, " ");
	    }
	}
}

static gen_request_decls (
    REQ	request)
{
/**************************************
 *
 *	g e n _ r e q u e s t _ d e c l s
 *
 **************************************
 *
 * Functional description
 *	Generate definitions associated with a single request.
 *
 **************************************/
int	length;
REF     reference;
BLB     blob;
POR	port;

if (!(request->req_flags & (REQ_exp_hand | REQ_menu_for_item |
			    REQ_sql_blob_open | REQ_sql_blob_create)) &&
	 request->req_type != REQ_slice && request->req_type != REQ_procedure)
    ib_fprintf (out_file, "%sINTEGER*4  %s             %s{ request handle }\n\n",
	COLUMN, request->req_handle, INLINE_COMMENT);

if (request->req_type == REQ_menu && !(request->req_flags & REQ_menu_for))
    ib_fprintf (out_file, "%sINTEGER*2  %ss             %s{ menu switch variable }\n\n",
	COLUMN, request->req_handle, INLINE_COMMENT);

if (request->req_type == REQ_menu)
    gen_menu_request (request);

if (request->req_type == REQ_form)
    ib_fprintf (out_file, "%sINTEGER*4  %s             %s{ form handle }\n\n",
	COLUMN, request->req_form_handle, INLINE_COMMENT);

/* generate the request as BLR long words */

length = (request->req_length + (sizeof (SLONG) - 1)) / sizeof (SLONG); 
if (length)
    ib_fprintf (out_file, "%sINTEGER*4      isc_%d(%d)    %s{ request BLR }\n",
	COLUMN, request->req_ident, length, INLINE_COMMENT);

/*  Generate declarations for the slice description language  */

for (port = request->req_ports; port; port = port->por_next)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_sdl)
	    {
	    length = (reference->ref_sdl_length + (sizeof(SLONG) - 1)) / sizeof(SLONG);
	    ib_fprintf (out_file, "%sINTEGER*4      isc_%d(%d)     %s{ request SDL }\n",
		COLUMN, reference->ref_sdl_ident, length, INLINE_COMMENT);
	    }

/* Print out any blob parameter block variable declarations required */
for (blob = request->req_blobs; blob; blob = blob->blb_next)
    if (blob->blb_const_from_type)
	{
	length = (blob->blb_bpb_length + (sizeof(SLONG) - 1)) / sizeof(SLONG);
	ib_fprintf (out_file, "%sINTEGER*4       isc_%d(%d)      %s{ blob parameter block }\n",
		COLUMN, blob->blb_bpb_ident, length, INLINE_COMMENT);
	} 

if (request->req_flags & REQ_sql_cursor)
    ib_fprintf (out_file, "%sINTEGER*4  isc_%dS             %s{ SQL statement handle }\n\n",
	COLUMN, request->req_ident, INLINE_COMMENT);

if ((request->req_type == REQ_ready) ||
    (request->req_type == REQ_create_database))
    {
    printa (COLUMN, "INTEGER*2  isc_%dl", request->req_ident);
    if (request->req_flags & REQ_extend_dpb)
        printa (COLUMN, "INTEGER*4  isc_%dp", request->req_ident);
    }
    

/* If this is a GET_SLICE/PUT_slice, allocate some variables */
 
if (request->req_type == REQ_slice)
    {
    printa (COLUMN, "INTEGER*4 isc_%dv (%d)", request->req_ident, MAX(request->req_slice->slc_parameters, 1));
    printa (COLUMN, "INTEGER*4  isc_%ds", request->req_ident);
    }
}

static gen_return_value (
    ACT		action)
{
/**************************************
 *
 *	g e n _ r e t u r n _ v a l u e
 *
 **************************************
 *
 * Functional description
 *	Generate receive call for a port
 *	in a store2 statement.
 *
 **************************************/
UPD	update;
REF	reference;
REQ	request;

request = action->act_request;

gen_start (action, request->req_primary);
if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "END IF");

update = (UPD) action->act_object;
reference = update->upd_references;
gen_receive (action, reference->ref_port);
}

static gen_routine (
    ACT		action)
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
BLB	blob;
REQ	request;
POR	port;


for (request = (REQ) action->act_object; request; request = request->req_routine)
    {
    for (port = request->req_ports; port; port = port->por_next)
	make_port (port);
    for (blob = request->req_blobs; blob; blob = blob->blb_next)
	{
	ib_fprintf (out_file, "%sINTEGER*4 isc_%d         %s{ blob handle }\n",
	    COLUMN, blob->blb_ident, INLINE_COMMENT);
	ib_fprintf (out_file, "%sCHARACTER*%d isc_%d      %s{ blob segment }\n",
	    COLUMN, blob->blb_seg_length, blob->blb_buff_ident, INLINE_COMMENT);
	ib_fprintf (out_file, "%sINTEGER*2 isc_%d         %s{ segment length }\n",
	    COLUMN, blob->blb_len_ident, INLINE_COMMENT);
	}
    }
}

static gen_s_end (
    ACT		action)
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

if (action->act_type == ACT_close)
    gen_cursor_close (action, request);

printa (COLUMN, "CALL ISC_UNWIND_REQUEST (%s, %s, %s%s%s)", 
    status_vector (action),
    request->req_handle,
    VAL_1, request->req_request_level, VAL_2);

if (action->act_type == ACT_close)
    {
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    }

status_and_stop (action);
}

static gen_s_fetch (
    ACT		action)
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
    gen_send (action, request->req_sync);

gen_receive (action, request->req_primary);
}

static gen_s_start (
    ACT		action)
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

gen_compile (action);

if (port = request->req_vport)
    asgn_from (action, port->por_references);

if (action->act_type == ACT_open)
    gen_cursor_open (action, request);

if (action->act_error || (action->act_flags & ACT_sql))
    make_ok_test (action, request);

gen_start (action, port);

if (action->act_error || (action->act_flags & ACT_sql))
    printa (COLUMN, "END IF");

if (action->act_type == ACT_open)
    {
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    printa (COLUMN, "END IF");
    status_and_stop (action);
    }
}

static gen_segment (
    ACT		action)
{
/**************************************
 *
 *	g e n _ s e g m e n t
 *
 **************************************
 *
 * Functional description
 *	Substitute for a segment, segment length, or blob handle.
 *
 **************************************/
BLB	blob;

blob = (BLB) action->act_object;

printa ("", "%sisc_%d",
    (action->act_flags & ACT_first) ? COLUMN : CONTINUE,
    (action->act_type == ACT_segment) ? blob->blb_buff_ident :
    (action->act_type == ACT_segment_length) ? blob->blb_len_ident :
    blob->blb_ident);
ib_fputs (CONTINUE, out_file );
}

static gen_select (
    ACT		action)
{
/**************************************
 *
 *	g e n _ s e l e c t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
REQ	request;
POR	port;
NOD	var_list;
int	i;
SCHAR	name[20];

request = action->act_request;
port = request->req_primary;
gen_name (name, request->req_eof, TRUE);

gen_s_start (action);
printa (COLUMN, "IF (SQLCODE .EQ. 0) THEN");
gen_receive (action, port);
printa (COLUMN, "IF (%s .NE. 0) THEN", name);
if (var_list = (NOD) action->act_object)
   for (i = 0; i < var_list->nod_count; i++)
	asgn_to (action, var_list->nod_arg[i]);

if (request->req_database->dbb_flags & DBB_v3)
    {
    gen_receive (action, port);
    printa (COLUMN, "IF (%s .NE. 0) THEN", name);
    printa (COLUMN, "SQLCODE = -1");
    printa (COLUMN, "ELSE");
    printa (COLUMN, "SQLCODE = 0");
    printa (COLUMN, "END IF");
    }

printa (COLUMN, "ELSE");
printa (COLUMN, "SQLCODE = 100");
printa (COLUMN, "END IF");
printa (COLUMN, "END IF");
}

static gen_send (
    ACT		action,
    POR		port)
{
/**************************************
 *
 *	g e n _ s e n d
 *
 **************************************
 *
 * Functional description
 *	Generate a send call for a port.
 *
 **************************************/
REQ	request;

request = action->act_request;
sprintf (output_buffer, "%s CALL ISC_SEND (%s, %s, %s%d%s, %s%d%s, %sisc_%d%s, %s%s%s)\n",
    COLUMN,
    status_vector (action),
    request->req_handle,
    I2CONST_1, port->por_msg_number, I2CONST_2,
    I2CONST_1, port->por_length, I2CONST_2,
    REF_1, port->por_ident, REF_2,
    VAL_1, request->req_request_level, VAL_2);

FTN_print_buffer (output_buffer);

status_and_stop (action);
}

static gen_slice (
    ACT		action)
{
/**************************************
 *
 *	g e n _ s l i c e
 *
 **************************************
 *
 * Functional description
 *	Generate support for get/put slice statement.
 *
 **************************************/
REQ	request, parent_request;
REF	reference, upper, lower;
SLC	slice;
PAT	args;
struct slc_repeat	*tail, *end;
TEXT    buffer[256], temp[64];
TEXT    *pattern1 = "CALL ISC_GET_SLICE (%V1, %RF%DH%RE, %RF%RT%RE, %RF%FR%RE, %N1, \
%I1, %N2, %I1v, %I1s, %RF%S5%RE, %RF%S6%RE)";
TEXT	*pattern2 = "CALL ISC_PUT_SLICE (%V1, %RF%DH%RE, %RF%RT%RE, %RF%FR%RE, %N1, \
%I1, %N2, %I1v, %I1s, %RF%S5%RE)";
SSHORT	column = 6;

request = action->act_request;
slice = (SLC) action->act_object;
parent_request = slice->slc_parent_request;

/* Compute array size */

sprintf (buffer, "isc_%ds = %d", request->req_ident, slice->slc_field->fld_array->fld_length);

for (tail = slice->slc_rpt, end = tail + slice->slc_dimensions; tail < end; ++tail)
    if (tail->slc_upper != tail->slc_lower)
	{
	lower = (REF) tail->slc_lower->nod_arg [0];
	upper = (REF) tail->slc_upper->nod_arg [0];
	if (lower->ref_value)
	    sprintf (temp, " * ( %s - %s + 1)", upper->ref_value, lower->ref_value);
	else
	    sprintf (temp, " * ( %s + 1)", upper->ref_value);
	strcat (buffer, temp);
	}
printa (COLUMN, buffer);

/* Make assignments to variable vector */

for (reference = request->req_values; reference; reference = reference->ref_next)
    printa (COLUMN, "isc_%dv [%d] = %s;", request->req_ident,
	    reference->ref_id, reference->ref_value);

args.pat_reference = slice->slc_field_ref;
args.pat_request = parent_request;				/* blob id request */
args.pat_vector1 = status_vector (action);			/* status vector */
args.pat_database = parent_request->req_database;		/* database handle */
args.pat_string1 = action->act_request->req_trans;		/* transaction handle */
args.pat_value1 = request->req_length;				/* slice descr. length */
args.pat_ident1 = request->req_ident;				/* request name */
args.pat_value2 = slice->slc_parameters * sizeof (SLONG);	/* parameter length */

reference = (REF) slice->slc_array->nod_arg [0];
args.pat_string5 = reference->ref_value;			/* array name */
args.pat_string6 = "isc_array_length";

PATTERN_expand (column, (action->act_type == ACT_get_slice) ? pattern1 : pattern2, &args);
}

static gen_start (
    ACT		action,
    POR		port)
{
/**************************************
 *
 *	g e n _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate either a START or START_AND_SEND depending
 *	on whether or a not a port is present.
 *
 **************************************/
REQ	request;
TEXT	*vector;
REF     reference;

request = action->act_request;
vector = status_vector (action);

if (port)
    {
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_field->fld_array_info)
	    gen_get_or_put_slice (action, reference, FALSE);

    sprintf (output_buffer, 
	"%sCALL ISC_START_AND_SEND (%s, %s, %s, %s%d%s, %s%d%s, %sisc_%d%s, %s%s%s)\n",
	COLUMN,
	vector,
	request->req_handle,	
	request_trans (action, request),
	I2CONST_1, port->por_msg_number, I2CONST_2,
	I2CONST_1, port->por_length, I2CONST_2,
	REF_1, port->por_ident, REF_2,
	I2CONST_1, request->req_request_level, I2CONST_2);
    }
else
    sprintf (output_buffer, "%sCALL ISC_START_REQUEST (%s, %s, %s, %s%s%s)\n",
	COLUMN,
	vector,
	request->req_handle,
	request_trans (action, request),
	I2CONST_1, request->req_request_level, I2CONST_2);

FTN_print_buffer (output_buffer);

status_and_stop (action);
}

static gen_store (
    ACT		action)
{
/**************************************
 *
 *	g e n _ s t o r e
 *
 **************************************
 *
 * Functional description
 *	Generate text for STORE statement.  This includes the compile
 *	call and any variable initialization required.
 *
 **************************************/
REQ	request;
REF	reference;
FLD	field;
POR	port;
TEXT	name[64];

request = action->act_request;
gen_compile (action);
if (action->act_error || (action->act_flags & ACT_sql))
    make_ok_test (action, request);

/* Initialize any blob fields */

port = request->req_primary;
for (reference = port->por_references; reference; 
     reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (field->fld_flags & FLD_blob)
	printa (COLUMN, "CALL isc_qtoq (isc_blob_null, %s)", gen_name (name, reference, TRUE));
    }
}

static gen_t_start (
    ACT		action)
{
/**************************************
 *
 *	g e n _ t _ s t a r t
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for START_TRANSACTION.
 *
 **************************************/
DBB		db;
TRA		trans;
TPB		tpb;
int		count;
TEXT		*filename;

/* if this is a purely default transaction, just let it through */

if (!action || !(trans = (TRA) action->act_object))
    {
    t_start_auto (NULL_PTR, status_vector(action), action, FALSE);
    return;
    }

/* build a complete statement, including tpb's.  Ready db's as req. */

if (sw_auto)
    for (tpb = trans->tra_tpb; tpb; tpb = tpb->tpb_tra_next)
	{
	db = tpb->tpb_database;
	if ((filename = db->dbb_runtime) ||
	    !(db->dbb_flags & DBB_sqlca))
	    {
	    printa (COLUMN, "IF (%s .EQ. 0) THEN", 
		db->dbb_name->sym_string);
	    make_ready (db, filename, status_vector (action), 0);
	    status_and_stop (action);
	    printa (COLUMN, "END IF");
	    }
	}
 
#ifdef hpux
/* If this is hpux we should be building a teb vector here 
   with the tpb address and length specified */

for (tpb = trans->tra_tpb, count = 0; tpb; tpb = tpb->tpb_tra_next)
    {
    count++;
    db = tpb->tpb_database;
    printa (COLUMN, "ISC_TEB%d_LEN = %d", 
	count, tpb->tpb_length);
    printa (COLUMN, "ISC_TEB%d_TPB = ISC_BADDRESS (ISC_TPB_%d)", 
	count, tpb->tpb_ident);
    printa (COLUMN, "ISC_TEB%d_DBB = ISC_BADDRESS (%s)", 
	count, db->dbb_name->sym_string);
    }

printa (COLUMN, "CALL ISC_START_MULTIPLE (%s, %s, %d, ISC_TEB)", 
    status_vector (action),
    (trans->tra_handle) ? trans->tra_handle : "gds__trans",
    trans->tra_db_count);

#else
     
printa (COLUMN, "CALL ISC_START_TRANSACTION (%s, %s, %s%d%s", 
    status_vector (action),
    (trans->tra_handle) ? trans->tra_handle : "GDS__TRANS",
    I2CONST_1, trans->tra_db_count, I2CONST_2);

for (tpb = trans->tra_tpb; tpb; tpb = tpb->tpb_tra_next)
    printa (CONTINUE, ", %s, %s%d%s, isc_tpb_%d", 
	tpb->tpb_database->dbb_name->sym_string,
	I2CONST_1, tpb->tpb_length, I2CONST_2,
	tpb->tpb_ident);

printa (CONTINUE, ")"); 
#endif

status_and_stop (action);
}

static gen_tpb_data (
    TPB	tpb)
{
/**************************************
 *
 *	g e n _ t p b _ d a t a
 *
 **************************************
 *
 * Functional description
 *	Initialize a TPB in the output file
 *
 **************************************/
TEXT	*p;
UCHAR	*text, *c;
int	length;
union	{
	UCHAR bytewise_tpb[4];
	SLONG longword_tpb;
	} tpb_hunk;

/*
 * TPBs are generated as raw BLR in longword chunks 
 * because FORTRAN is a miserable excuse for a language
 * and doesn't allow byte value assignments to character
 * fields.
 */

length = (tpb->tpb_length + (sizeof(SLONG) -1)) / sizeof(SLONG); 

printa (COLUMN, "DATA ISC_TPB_%d  /", 
	tpb->tpb_ident, COMMA, length);

text = tpb->tpb_string;
length = tpb->tpb_length;
strcpy (output_buffer, CONTINUE);
for (p = output_buffer; *p; p++)
    ;

while (length)
    {
    for (c = tpb_hunk.bytewise_tpb; 
	 c < tpb_hunk.bytewise_tpb + sizeof(SLONG) ; c++)
 	{
	*c = *text++;
	if (!(--length))
	    break;
	}
    if (length)
	sprintf (p, "%11d,", tpb_hunk.longword_tpb);
    else
	sprintf (p, "%d/\n", tpb_hunk.longword_tpb);
    p += 12;
    }

FTN_print_buffer (output_buffer);
sprintf (output_buffer, "%sEnd of data for ISC_TPB_%d\n",
	COMMENT, tpb->tpb_ident);
FTN_print_buffer (output_buffer);
}

static gen_tpb_decls (
    TPB	tpb)
{
/**************************************
 *
 *	g e n _ t p b _ d e c l s
 *
 **************************************
 *
 * Functional description
 *	Generate the declaration for a
 *      TPB in the output file
 *
 **************************************/
int length;

length = (tpb->tpb_length + (sizeof(SLONG) -1)) / sizeof(SLONG); 
ib_fprintf (out_file, "%sINTEGER*4      ISC_TPB_%d(%d)    %s{ transaction parameters }\n",
	COLUMN, tpb->tpb_ident, length, INLINE_COMMENT);
}

static gen_trans (
    ACT		action)
{
/**************************************
 *
 *	g e n _ t r a n s
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for COMMIT, ROLLBACK, PREPARE, and SAVE
 *
 **************************************/

if (action->act_type == ACT_commit_retain_context)
    printa (COLUMN, "CALL ISC_COMMIT_RETAINING (%s, %s)",
	    status_vector (action),
	    (action->act_object) ? (TEXT*)(action->act_object) : "GDS__TRANS");
else
    printa (COLUMN, "CALL ISC_%s_TRANSACTION (%s, %s)",
	    (action->act_type == ACT_commit) ? "COMMIT" :
		(action->act_type == ACT_rollback) ? "ROLLBACK" : "PREPARE",
	    status_vector (action),
	    (action->act_object) ? (TEXT*)(action->act_object) : "GDS__TRANS");
status_and_stop (action);
	
}

static gen_update (
    ACT		action)
{
/**************************************
 *
 *	g e n _ u p d a t e
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for UPDATE ... WHERE CURRENT OF ...
 *
 **************************************/
POR	port;
UPD	modify;

modify = (UPD) action->act_object;
port = modify->upd_port;
asgn_from (action, port->por_references);
gen_send (action, port);
}

static gen_variable (
    ACT		action)
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
SCHAR	s[20];
REF	reference;

reference = (REF) action->act_object;
printa ("","%s%s",
    (action->act_flags & ACT_first) ? COLUMN : CONTINUE,
    gen_name (s, reference, FALSE));

ib_fputs (CONTINUE, out_file );
}

static gen_whenever (
    SWE		label)
{
/**************************************
 *
 *	g e n _ w h e n e v e r
 *
 **************************************
 *
 * Functional description
 *	Generate tests for any WHENEVER clauses that may have been declared.
 *
 **************************************/
TEXT	*condition;

while (label)
    {
    switch (label->swe_condition)
	{
	case SWE_error:
	    condition = "SQLCODE .LT. 0";
	    break;

	case SWE_warning:
	    condition = "SQLCODE .EQ. 0 .AND. SQLCODE .NE. 100";
	    break;

	case SWE_not_found:
	    condition = "SQLCODE .EQ. 100";
	    break;
	}
    printa (COLUMN, "if (%s) goto %s", condition, label->swe_label);
    label = label->swe_next;
    }
}

static gen_window_create (
    ACT		action)
{
/**************************************
 *
 *	g e n _ w i n d o w _ c r e a t e
 *
 **************************************
 *
 * Functional description
 *	Create a new window.
 *
 **************************************/

printa (COLUMN, "CALL PYXIS__CREATE_WINDOW (isc_window, 0, 0, isc_width, isc_height)");
}

static gen_window_delete (
    ACT		action)
{
/**************************************
 *
 *	g e n _ w i n d o w _ d e l e t e
 *
 **************************************
 *
 * Functional description
 *	Delete a window.
 *
 **************************************/

printa (COLUMN, "CALL PYXIS__DELETE_WINDOW (isc_window)");
}

static gen_window_suspend (
    ACT		action)
{
/**************************************
 *
 *	g e n _ w i n d o w _ s u s p e n d
 *
 **************************************
 *
 * Functional description
 *	Suspend a window.
 *
 **************************************/

printa (COLUMN, "CALL PYXIS__SUSPEND_WINDOW (isc_window)");
}

static make_array_declaration (
    REF		reference)
{
/**************************************
 *
 *	m a k e _ a r r a y _ d e c l a r a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a declaration of an array in the
 *      output file.
 *
 **************************************/
FLD    field;
SCHAR   *name;
TEXT   s [64];
DIM    dimension;
ADL    this_array;

field = reference->ref_field;
name = field->fld_symbol->sym_string;

/*  Check to see if the array already has been
    declared in this routine or subroutine  */
if (array_decl_list)
    for (this_array = array_decl_list; this_array; this_array = this_array->adl_next)
	if (field->fld_array_info->ary_ident == this_array->adl_gds_ident)
	    return;

/*  If not, add it to the "declared" list and declare it  */
this_array = (ADL) ALLOC (ADL_LEN);
this_array->adl_gds_ident = field->fld_array_info->ary_ident;
if (array_decl_list)
    this_array->adl_next = array_decl_list;
else
    this_array->adl_next = NULL;
array_decl_list = this_array;

switch (field->fld_array_info->ary_dtype) 
    {
    case dtype_short :
	ib_fprintf (out_file, "%sINTEGER*2%s", COLUMN, COLUMN);
	break;

    case dtype_long :
	ib_fprintf (out_file, "%sINTEGER*4%s", COLUMN, COLUMN);
	break;

    case dtype_date :
    case dtype_blob :
    case dtype_quad :
	ib_fprintf (out_file, "%sINTEGER*4%s", COLUMN, COLUMN);
	break;
	
    case dtype_text :
	ib_fprintf (out_file, "%sCHARACTER*%d%s", 
		COLUMN, field->fld_array->fld_length, COLUMN);
	break;

    case dtype_float:
	ib_fprintf (out_file, "%sREAL%s", COLUMN, COLUMN);
	break;

    case dtype_double:
	ib_fprintf (out_file, "%s%s%s", COLUMN, DOUBLE_DCL, COLUMN);
	break;

    default :
	sprintf (s, "datatype %d unknown\n",
	    field->fld_dtype);
	CPR_error (s);
	return;
    }

/*  Print out the dimension part of the declaration  */
ib_fprintf (out_file, "isc_%d", field->fld_array_info->ary_ident);
ib_fprintf (out_file, "(" );

for (dimension = field->fld_array_info->ary_dimension; dimension;
	dimension = dimension->dim_next)
    {
    if (dimension->dim_lower != 1)
	ib_fprintf (out_file, "%d:", dimension->dim_lower);

    ib_fprintf (out_file, "%d", dimension->dim_upper);
    if (dimension->dim_next)
	ib_fprintf (out_file, ", ");
    }

if (field->fld_dtype == dtype_quad ||
    field->fld_dtype == dtype_date)
    ib_fprintf (out_file, ",2");

/*  Print out the database field  */

ib_fprintf (out_file, ")        %s{ %s }\n", INLINE_COMMENT, name);
}

static TEXT *make_name (
    TEXT	*string,
    SYM		symbol)
{
/**************************************
 *
 *	m a k e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Turn a symbol into a varying string.
 *
 **************************************/

sprintf (string, "%s'%s '%s", REF_1, symbol->sym_string, REF_2);

return string;
}

static make_ok_test (
    ACT		action,
    REQ		request)
{
/**************************************
 *
 *	m a k e _ o k _ t e s t
 *
 **************************************
 *
 * Functional description
 *	Generate code to test existence of compiled request with
 *	active transaction
 *
 **************************************/

if (sw_auto)
    printa (COLUMN, "IF (%s .NE. 0 .AND. %s .NE. 0) THEN",
	request_trans (action, request), request->req_handle);
else
    printa (COLUMN, "IF (%s .NE. 0) THEN", request->req_handle);
}

static make_port (
    POR	port)
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
SCHAR	*name, s [80];
USHORT	length;

length = (port->por_length + 3) & ~3;

printa (COLUMN, "CHARACTER      isc_%d(%d)", port->por_ident, length);

for (reference = port->por_references; reference; 
    reference = reference->ref_next)
    {
    field = reference->ref_field;
    if (symbol = field->fld_symbol)
	name = symbol->sym_string;
    else
	name = "<expression>";
    if (reference->ref_value && (reference->ref_flags & REF_array_elem))
	field = field->fld_array;
    switch (field->fld_dtype) 
	{
	case dtype_short:
	    ib_fprintf (out_file, "%sINTEGER*2      isc_%d      %s{ %s }\n",
		COLUMN, reference->ref_ident, INLINE_COMMENT, name);
	    break;

	case dtype_long:
	    ib_fprintf (out_file, "%sINTEGER*4      isc_%d      %s{ %s }\n",
		COLUMN, reference->ref_ident, INLINE_COMMENT, name);
	    break;

	case dtype_cstring:
	case dtype_text: 
	    ib_fprintf (out_file, "%sCHARACTER*%d   isc_%d      %s{ %s }\n",
		 COLUMN, field->fld_length, reference->ref_ident, INLINE_COMMENT, name);
	    break;

	case dtype_date:
	case dtype_quad:
	case dtype_blob:
	    ib_fprintf (out_file, "%sINTEGER*4      isc_%d(2)   %s{ %s }\n",
		COLUMN, reference->ref_ident, INLINE_COMMENT, name);
	    break;
	    
	case dtype_float:
	     ib_fprintf (out_file, "%sREAL          isc_%d      %s{ %s }\n",
		COLUMN, reference->ref_ident, INLINE_COMMENT, name);
	    break;

	case dtype_double:
	    ib_fprintf (out_file, "%s%s         isc_%d      %s{ %s }\n",
		COLUMN, DOUBLE_DCL, reference->ref_ident, INLINE_COMMENT, name);
	    break;

	default:
	    sprintf (s, "datatype %d unknown for field %s, msg %d",
		field->fld_dtype, name,
		port->por_msg_number);
	    CPR_error (s);
	    return;
	}
    }

for (reference = port->por_references; reference; 
     reference = reference->ref_next)
    printa (COLUMN, "EQUIVALENCE    (isc_%d(%d), isc_%d)",
	port->por_ident, reference->ref_offset + 1, reference->ref_ident);

printa (COLUMN, " ");
}

static make_ready (
    DBB		db,
    TEXT	*filename,
    TEXT	*vector,
    REQ		request)
{
/**************************************
 *
 *	m a k e _ r e a d y
 *
 **************************************
 *
 * Functional description
 *	Generate the actual ready call.
 *
 **************************************/
TEXT 	s1 [32], s2 [32];

if (request)
    {
    sprintf (s1, "isc_%dl", request->req_ident);

    if (request->req_flags & REQ_extend_dpb)
     	sprintf (s2, "isc_%dp", request->req_ident);
    else
   	sprintf (s2, "isc_%d", request->req_ident);
    /* if the dpb needs to be extended at runtime to include items
       in host variables, do so here; this assumes that there is 
       always a request generated for runtime variables */

    if (request->req_flags & REQ_extend_dpb)
	{
	if (request->req_length)
	    {
    	    sprintf (output_buffer, "%s%s = isc_%d\n", 
		COLUMN, s2, request->req_ident);
    	    FTN_print_buffer (output_buffer);
	    }
/* MMM */
	else
	    {
    	    sprintf (output_buffer, "%s%s = 0\n", 
		COLUMN, s2);
    	    FTN_print_buffer (output_buffer);
	    }

        if (db->dbb_r_user)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_user_name, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_user,  
	        I2CONST_1, db->dbb_r_user, I2CONST_2);
            FTN_print_buffer (output_buffer);
	    }
        if (db->dbb_r_password)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_password, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_password,  
	        I2CONST_1, db->dbb_r_password, I2CONST_2);
            FTN_print_buffer (output_buffer);
	    }

	/*
	** =========================================================
	** ==   SQL Role supports GPRE/Fortran
	** =========================================================
	*/

        if (db->dbb_r_sql_role)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_sql_role_name, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_sql_role,  
	        I2CONST_1, db->dbb_r_sql_role, I2CONST_2);
            FTN_print_buffer (output_buffer);
	    }

        if (db->dbb_r_lc_messages)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB(%s, %s, isc_dpb_lc_messages, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_lc_messages,  
	        I2CONST_1, db->dbb_r_lc_messages, I2CONST_2 );
            FTN_print_buffer (output_buffer);
	    }
        if (db->dbb_r_lc_ctype)
            {
    	    sprintf (output_buffer, 
	    "%sCALL ISC_MODIFY_DPB (%s, %s, isc_dpb_lc_type, %s, %sLEN(%s)%s)\n",
	        COLUMN,
	        s2, s1, db->dbb_r_lc_ctype,  
	        I2CONST_1, db->dbb_r_lc_ctype, I2CONST_2 );
            FTN_print_buffer (output_buffer);
	    }
	}
    }
if (filename)
    {
    sprintf (output_buffer,
	"%sISC_%s = %s\n", COLUMN, db->dbb_name->sym_string, filename);
    FTN_print_buffer (output_buffer);

    sprintf (output_buffer, 
	"%sCALL ISC_ATTACH_DATABASE (%s, %sLEN(%s)%s, %sISC_%s%s, %s, %s%s%s, %s)\n",
	COLUMN,
	vector,
	I2CONST_1, filename, I2CONST_2,
	REF_1, db->dbb_name->sym_string, REF_2,
	db->dbb_name->sym_string,
	I2CONST_1, 
	(request? s1 : "0"),
	I2CONST_2,
	(request? s2 : "0"));
	FTN_print_buffer (output_buffer);
    }
else
    {
    sprintf (output_buffer, "%sISC_%s = '%s'\n", 
	COLUMN, db->dbb_name->sym_string, db->dbb_filename);
    FTN_print_buffer (output_buffer);

    sprintf (output_buffer, 
	"%sCALL ISC_ATTACH_DATABASE (%s, %sLEN('%s')%s, %sISC_%s%s, %s, %s%s%s, %s)\n",
	COLUMN,
	vector,
	I2CONST_1, db->dbb_filename, I2CONST_2,
	REF_1, db->dbb_name->sym_string, REF_2,
	db->dbb_name->sym_string,
	I2CONST_1, 
	(request? s1 : "0"),
	I2CONST_2,
	(request? s2 : "0"));
	FTN_print_buffer (output_buffer);
    }
if (request && request->req_flags & REQ_extend_dpb)
    {
    if (request->req_length) {
	sprintf (output_buffer, "%sif (%s != isc_%d)\n", COLUMN, s2, request->req_ident);
        FTN_print_buffer (output_buffer);
    }
    sprintf(output_buffer, "%sCALL ISC_FREE (%s)\n", COLUMN, s2);
    FTN_print_buffer (output_buffer);

    /* reset the length of the dpb */

    sprintf (output_buffer, "%s%s = %d\n", COLUMN, s1, request->req_length);
    FTN_print_buffer (output_buffer);
    }
}

static next_label (void)
{
/**************************************
 *
 *	n e x t _ l a b e l
 *
 **************************************
 *
 * Functional description
 *	Looks at the label bitmap and allocates
 *      an unused label.  Marks the current
 *      label as used.
 *
 **************************************/
USHORT	label;
UCHAR	*byte, target_byte;

for (byte = fortran_labels; *byte == 255; byte++)
    ;

label = ((byte - fortran_labels) << 3);

for (target_byte = *byte; target_byte & 1; target_byte >>= 1)
    label++;

*byte |= 1 << (label & 7);   

return (label);
}      

static printa (
    SCHAR	*column,
    SCHAR	*string,
    ...)
{
/**************************************
 *
 *	p r i n t a
 *
 **************************************
 *
 * Functional description
 *	Print a fixed string at a particular COLUMN.
 *
 **************************************/
va_list	ptr;
SCHAR	s [256];

VA_START (ptr, string);
strcpy (s, column);
strcat (s, string);
strcat (s, "\n");
vsprintf (output_buffer, s, ptr);
FTN_print_buffer (output_buffer);
}

static TEXT *request_trans (
    ACT		action,
    REQ		request)
{
/**************************************
 *
 *	r e q u e s t _ t r a n s
 *
 **************************************
 *
 * Functional description
 *	Generate the appropriate transaction handle.
 *
 **************************************/
SCHAR	*trname;

if (action->act_type == ACT_open)
    {
    if (!(trname = ((OPN) action->act_object)->opn_trans))
	trname = "GDS__TRANS";
    return trname;
    }
else
    return (request) ? request->req_trans : "GDS__TRANS";
}

static status_and_stop (
    ACT		action)
{
/**************************************
 *
 *	s t a t u s _ a n d _ s t o p
 *
 **************************************
 *
 * Functional description
 *	Do the error handling ourselves
 *      until we figure out how to use the
 *      ISC_NULL from FORTRAN
 *
 **************************************/

if (action && (action->act_flags & ACT_sql))
    printa (COLUMN, "SQLCODE = ISC_SQLCODE (ISC_STATUS)");
else if (!action || !action->act_error)
    {
    printa (COLUMN, "IF (ISC_STATUS(2) .NE. 0) THEN");
    printa (COLUMN, "    CALL ISC_PRINT_STATUS (ISC_STATUS)"); 
    printa (COLUMN, "    STOP");
    printa (COLUMN, "END IF");
    }
}

static TEXT *status_vector (
    ACT		action)
{
/**************************************
 *
 *	s t a t u s _ v e c t o r
 *
 **************************************
 *
 * Functional description
 *	Generate the appropriate status vector parameter for a gds
 *	call depending on where or not the action has an error clause.
 *
 **************************************/

return "ISC_STATUS";
/* return (!action || !action->act_error) ? "ISC_NULL" : "ISC_STATUS"; */
}

static t_start_auto (
    REQ		request,
    TEXT	*vector,
    ACT		action,
    SSHORT	test)
{
/**************************************
 *
 *	t _ s t a r t _ a u t o
 *
 **************************************
 *
 * Functional description
 *	Generate substitution text for START_TRANSACTION,
 *      when it's being generated automatically by a compile
 *      call or one of the DDL commands.  Be careful not to
 *	continue after errors as that destroys evidence.
 *
 **************************************/
DBB	db;
int	count;
TEXT	*filename, buffer[256], temp[40], *trname;

buffer [0] = 0;
trname = request_trans (action, request);

/* this is a default transaction, make sure all databases are ready */

for (count = 0, db = isc_databases; db; db = db->dbb_next)
    {
    if (sw_auto)
	if ((filename = db->dbb_runtime) ||
	    !(db->dbb_flags & DBB_sqlca))
	    {
	    if (buffer [0])  
		printa (COLUMN, "IF (%s .EQ. 0 .AND. %s(2) .EQ. 0) THEN", 
		    db->dbb_name->sym_string, vector); 
	    else
		printa (COLUMN, "IF (%s .EQ. 0) THEN", 
		    db->dbb_name->sym_string); 
	    make_ready (db, filename, vector, 0);
	    printa (COLUMN, "END IF");
	    if (buffer [0])
		strcat (buffer, " .AND. ");
	    sprintf (temp, "%s .NE. 0", db->dbb_name->sym_string);
	    strcat (buffer, temp);
	    }

    count++;
#ifdef hpux
    printa (COLUMN, "ISC_TEB%d_LEN = 0", count);
    printa (COLUMN, "ISC_TEB%d_TPB = ISC_NULL", count);
    printa (COLUMN, "ISC_TEB%d_DBB = ISC_BADDRESS (%s)", count, 
	    db->dbb_name->sym_string);
#endif 
    }    

if (sw_auto)
    {
    if (!buffer [0])
	strcpy (buffer, ".TRUE.");
    if (test)
	printa (COLUMN, "IF ((%s) .AND. (%s .EQ. 0)) THEN", buffer, trname);
    else
	printa (COLUMN, "IF (%s) THEN", buffer);
    }

#ifdef hpux
printa (COLUMN_INDENT, "CALL ISC_START_MULTIPLE (%s, %s, %s%d%s, ISC_TEB)", 
	vector, trname, I2CONST_1, count, I2CONST_2);
#else
printa (COLUMN_INDENT, "CALL ISC_START_TRANSACTION (%s, %s, %s%d%s", 
	vector, trname, I2CONST_1, count, I2CONST_2);
   
for (db = isc_databases; db; db = db->dbb_next)
    printa (CONTINUE, ", %s, %s0%s, 0", 
	db->dbb_name->sym_string, I2CONST_1, I2CONST_2);
printa (CONTINUE, ")");
#endif

if (sw_auto)
    printa (COLUMN, "END IF");

status_and_stop (action);
}

#if 0		/* RRK_?: this column stuff was not used in 3.3
		   may be should not bother with it now */
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
 *	Align output to a specific column for output.  If the
 *	column is negative, don't do anything.
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
#endif		/* RRK_?: end of comment out */

#ifdef UNDEF		/* RRK_?: seems not in use */
static void asgn_sqlda_from (
    REF		reference,
    int		number,
    SCHAR	*string,
    int		column)
{
/**************************************
 *
 *	a s g n _ s q l d a _ f r o m
 *
 **************************************
 *
 * Functional description
 *	Build an assignment from a host language variable to
 *	a sqlda variable.
 *
 **************************************/
SCHAR	*value, temp[20];

for (; reference; reference = reference->ref_next)
    {
    align (column);
    if (reference->ref_source)
	value = gen_name (temp, reference->ref_source, TRUE);
    else
	value = reference->ref_value;
    ib_fprintf (out_file, "isc_to_sqlda (isc_sqlda, %d, %s, sizeof(%s), %s);", 
	     number, value, value, string);
    }
}
#endif

static int gen_any (
    ACT		action )
{
/**************************************
 *
 *	g e n _ a n y
 *
 **************************************
 *
 * Functional description
 *	Generate a function call for free standing ANY.  Somebody else
 *	will need to generate the actual function.
 *
 **************************************/
REQ	request;
POR	port;
REF	reference;

request = action->act_request;

ib_fprintf (out_file, "%s%s_r (&%s, &%s", COLUMN,
    request->req_handle, request->req_handle, request->req_trans);

if (port = request->req_vport)
    for (reference = port->por_references; reference; 
	 reference = reference->ref_next)
	ib_fprintf (out_file, ", %s", reference->ref_value);

ib_fprintf (out_file, ")");
}

static int gen_clear_handles (
    ACT		action )
{
/**************************************
 *
 *	g e n _ c l e a r _ h a n d l e s
 *
 **************************************
 *
 * Functional description
 *	Zap all know handles.
 *
 **************************************/
REQ	request;

for (request = requests; request; request = request->req_next)
    {
    if (!(request->req_flags & REQ_exp_hand))
	printa ("%s%s = 0;", COLUMN, request->req_handle);
    if (request->req_form_handle && 
	!(request->req_flags & REQ_exp_form_handle))
	printa ("%s%s = 0;", COLUMN, request->req_form_handle);
    }
}

static int gen_compatibility_symbol (
    TEXT	*symbol,
    TEXT	*v4_prefix,
    TEXT	*trailer)
{
/**************************************
 *
 *	g e n _ c o m p a t i b i l i t y _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *	Generate a symbol to ease compatibility with V3.
 *
 **************************************/
TEXT	*v3_prefix;

v3_prefix = (sw_language == lan_cxx) ? "isc_" : "isc_";

ib_fprintf (out_file, "#define %s%s\t%s%s%s\n", v3_prefix, symbol, v4_prefix,
    symbol, trailer);
}

static int gen_function (
    ACT		function)
{
/**************************************
 *
 *	g e n _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate a function for free standing ANY or statistical.
 *
 **************************************/
REQ	request;
POR	port;
REF	reference;
FLD	field;
ACT	action;
TEXT	*dtype, s[64];

action = (ACT) function->act_object;

if (action->act_type != ACT_any)
    {
    IBERROR ("can't generate function");
    return;
    }

request = action->act_request;

ib_fprintf (out_file, "static %s_r (request, transaction", 
    request->req_handle, request->req_handle, request->req_trans);

if (port = request->req_vport)
    for (reference = port->por_references; reference; 
	 reference = reference->ref_next)
	ib_fprintf (out_file, ", %s", gen_name (s, reference->ref_source, TRUE));

ib_fprintf (out_file, ")\n    isc_req_handle\trequest;\n    isc_tr_handle\ttransaction;\n",
    request->req_handle, request->req_trans);

if (port)
    for (reference = port->por_references; reference; 
	 reference = reference->ref_next)
	{
	field = reference->ref_field;
	switch (field->fld_dtype) 
	    {
	    case dtype_short:
		dtype = "short";
		break;

	    case dtype_long:
/* RRK_?:		dtype = DCL_LONG;  */
		break;

	    case dtype_cstring:
	    case dtype_text:
		dtype = "char*";
		break;

	    case dtype_quad:
/*		dtype = DCL_QUAD;	 */
		break;
		
	    case dtype_date:
	    case dtype_blob:
		dtype = "ISC_QUAD";
		break;
		
	    case dtype_real:
		dtype = "float";
		break;

	    case dtype_double:
		dtype = "double";
		break;

	    default:
		IBERROR ("gen_function: unsupported datatype");
		return;
	    }
	ib_fprintf (out_file, "    %s\t%s;\n", dtype,
	    gen_name (s, reference->ref_source, TRUE));
	}

ib_fprintf (out_file, "{\n");
for (port = request->req_ports; port; port = port->por_next)
    make_port (port);

ib_fprintf (out_file, "\n\n");
gen_s_start (action);
gen_receive (action, request->req_primary);

for (port = request->req_ports; port; port = port->por_next)
    for (reference = port->por_references; reference; reference = reference->ref_next)
	if (reference->ref_field->fld_array_info)
	    gen_get_or_put_slice (action, reference, TRUE);

port = request->req_primary;
ib_fprintf (out_file, "\nreturn %s;\n}\n", gen_name (s, port->por_references, TRUE));
}

static int gen_tpb (
    TPB	tpb )
{
/**************************************
 *
 *	g e n _ t p b
 *
 **************************************
 *
 * Functional description
 *	Generate a TPB in the output file
 *
 **************************************/
TEXT	*text, buffer[80], c, *p;
SSHORT	length, tpb_length;

printa ("%sstatic char\n",COLUMN);
p = buffer;

for (length = 0; length < 4; length++)
    *p++ = ' ';

sprintf (p, "isc_tpb_%d [%d] = {", tpb->tpb_ident, tpb->tpb_length);
while (*p)
    p++;

tpb_length = tpb->tpb_length;
text = (TEXT*) tpb->tpb_string;

while (--tpb_length >= 0)
    {
    c = *text++;
    if ((c >= 'A' && c <= 'Z') || c == '$' || c == '_')
	sprintf (p, "'%c'", c);
    else
	sprintf (p, "%d", c);
    while (*p)
	p++;
    if (tpb_length)
	*p++ = ',';
    if (p - buffer > 60)
	{
	*p = 0;
	ib_fprintf (out_file, " %s\n", buffer);
	p = buffer;
	for (length = 0; length < 8; length++)
	    *p++ = ' ';
	*p = 0;
	}
    }

ib_fprintf (out_file, "%s};\n", buffer);
}

static int gen_type (
    ACT		action)
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

printa ("%s%ld", COLUMN, action->act_object);
}

static int printb (
    TEXT	*string,
    ...)
{
/**************************************
 *
 *	p r i n t b
 *
 **************************************
 *
 * Functional description
 *	Print a fixed string at a particular column.
 *
 **************************************/
va_list	ptr;

VA_START (ptr, string);
ib_vfprintf (out_file, string, ptr);
}


