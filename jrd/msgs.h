/*
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
"unassigned code",
"arithmetic exception, numeric overflow, or string truncation",		/*1, arith_except                    */
"invalid database key",		/*2, bad_dbkey                       */
"file %s is not a valid database",		/*3, bad_db_format                   */
"invalid database handle (no active connection)",		/*4, bad_db_handle                   */
"bad parameters on attach or create database",		/*5, bad_dpb_content                 */
"unrecognized database parameter block",		/*6, bad_dpb_form                    */
"invalid request handle",		/*7, bad_req_handle                  */
"invalid BLOB handle",		/*8, bad_segstr_handle               */
"invalid BLOB ID",		/*9, bad_segstr_id                   */
"invalid parameter in transaction parameter block",		/*10, bad_tpb_content                 */
"invalid format for transaction parameter block",		/*11, bad_tpb_form                    */
"invalid transaction handle (expecting explicit transaction start)",		/*12, bad_trans_handle                */
"internal gds software consistency check (%s)",		/*13, bug_check                       */
"conversion error from string \"%s\"",		/*14, convert_error                   */
"database file appears corrupt (%s)",		/*15, db_corrupt                      */
"deadlock",		/*16, deadlock                        */
"attempt to start more than %ld transactions",		/*17, excess_trans                    */
"no match for first value expression",		/*18, from_no_match                   */
"information type inappropriate for object specified",		/*19, infinap                         */
"no information of this type available for object specified",		/*20, infona                          */
"unknown information item",		/*21, infunk                          */
"action cancelled by trigger (%ld) to preserve data integrity",		/*22, integ_fail                      */
"invalid request BLR at offset %ld",		/*23, invalid_blr                     */
"I/O error for file %.0s\"%s\"",		/*24, io_error                        */
"lock conflict on no wait transaction",		/*25, lock_conflict                   */
"corrupt system table",		/*26, metadata_corrupt                */
"validation error for column %s, value \"%s\"",		/*27, not_valid                       */
"no current record for fetch operation",		/*28, no_cur_rec                      */
"attempt to store duplicate value (visible to active transactions) in unique index \"%s\"",		/*29, no_dup                          */
"program attempted to exit without finishing database",		/*30, no_finish                       */
"unsuccessful metadata update",		/*31, no_meta_update                  */
"no permission for %s access to %s %s",		/*32, no_priv                         */
"transaction is not in limbo",		/*33, no_recon                        */
"invalid database key",		/*34, no_record                       */
"BLOB was not closed",		/*35, no_segstr_close                 */
"metadata is obsolete",		/*36, obsolete_metadata               */
"cannot disconnect database with open transactions (%ld active)",		/*37, open_trans                      */
"message length error (encountered %ld, expected %ld)",		/*38, port_len                        */
"attempted update of read-only column",		/*39, read_only_field                 */
"attempted update of read-only table",		/*40, read_only_rel                   */
"attempted update during read-only transaction",		/*41, read_only_trans                 */
"cannot update read-only view %s",		/*42, read_only_view                  */
"no transaction for request",		/*43, req_no_trans                    */
"request synchronization error",		/*44, req_sync                        */
"request referenced an unavailable database",		/*45, req_wrong_db                    */
"segment buffer length shorter than expected",		/*46, segment                         */
"attempted retrieval of more segments than exist",		/*47, segstr_eof                      */
"attempted invalid operation on a BLOB",		/*48, segstr_no_op                    */
"attempted read of a new, open BLOB",		/*49, segstr_no_read                  */
"attempted action on blob outside transaction",		/*50, segstr_no_trans                 */
"attempted write to read-only BLOB",		/*51, segstr_no_write                 */
"attempted reference to BLOB in unavailable database",		/*52, segstr_wrong_db                 */
"operating system directive %s failed",		/*53, sys_request                     */
"attempt to fetch past the last record in a record stream",		/*54, stream_eof                      */
"unavailable database",		/*55, unavailable                     */
"table %s was omitted from the transaction reserving list",		/*56, unres_rel                       */
"request includes a DSRI extension not supported in this implementation",		/*57, uns_ext                         */
"feature is not supported",		/*58, wish_list                       */
"unsupported on-disk structure for file %s; found %ld, support %ld",		/*59, wrong_ods                       */
"wrong number of arguments on call",		/*60, wronumarg                       */
"Implementation limit exceeded",		/*61, imp_exc                         */
"%s",		/*62, random                          */
"unrecoverable conflict with limbo transaction %ld",		/*63, fatal_conflict                  */
"internal error",		/*64, badblk                          */
"internal error",		/*65, invpoolcl                       */
"too many requests",		/*66, nopoolids                       */
"internal error",		/*67, relbadblk                       */
"block size exceeds implementation restriction",		/*68, blktoobig                       */
"buffer exhausted",		/*69, bufexh                          */
"BLR syntax error: expected %s at offset %ld, encountered %ld",		/*70, syntaxerr                       */
"buffer in use",		/*71, bufinuse                        */
"internal error",		/*72, bdbincon                        */
"request in use",		/*73, reqinuse                        */
"incompatible version of on-disk structure",		/*74, badodsver                       */
"table %s is not defined",		/*75, relnotdef                       */
"column %s is not defined in table %s",		/*76, fldnotdef                       */
"internal error",		/*77, dirtypage                       */
"internal error",		/*78, waifortra                       */
"internal error",		/*79, doubleloc                       */
"internal error",		/*80, nodnotfnd                       */
"internal error",		/*81, dupnodfnd                       */
"internal error",		/*82, locnotmar                       */
"page %ld is of wrong type (expected %ld, found %ld)",		/*83, badpagtyp                       */
"database corrupted",		/*84, corrupt                         */
"checksum error on database page %ld",		/*85, badpage                         */
"index is broken",		/*86, badindex                        */
"database handle not zero",		/*87, dbbnotzer                       */
"transaction handle not zero",		/*88, tranotzer                       */
"transaction--request mismatch (synchronization error)",		/*89, trareqmis                       */
"bad handle count",		/*90, badhndcnt                       */
"wrong version of transaction parameter block",		/*91, wrotpbver                       */
"unsupported BLR version (expected %ld, encountered %ld)",		/*92, wroblrver                       */
"wrong version of database parameter block",		/*93, wrodpbver                       */
"BLOB and array data types are not supported for %s operation",		/*94, blobnotsup                      */
"database corrupted",		/*95, badrelation                     */
"internal error",		/*96, nodetach                        */
"internal error",		/*97, notremote                       */
"transaction in limbo",		/*98, trainlim                        */
"transaction not in limbo",		/*99, notinlim                        */
"transaction outstanding",		/*100, traoutsta                       */
"connection rejected by remote interface",		/*101, connect_reject                  */
"internal error",		/*102, dbfile                          */
"internal error",		/*103, orphan                          */
"no lock manager available",		/*104, no_lock_mgr                     */
"context already in use (BLR error)",		/*105, ctxinuse                        */
"context not defined (BLR error)",		/*106, ctxnotdef                       */
"data operation not supported",		/*107, datnotsup                       */
"undefined message number",		/*108, badmsgnum                       */
"bad parameter number",		/*109, badparnum                       */
"unable to allocate memory from operating system",		/*110, virmemexh                       */
"blocking signal has been received",		/*111, blocking_signal                 */
"lock manager error",		/*112, lockmanerr                      */
"communication error with journal \"%s\"",		/*113, journerr                        */
"key size exceeds implementation restriction for index \"%s\"",		/*114, keytoobig                       */
"null segment of UNIQUE KEY",		/*115, nullsegkey                      */
"SQL error code = %ld",		/*116, sqlerr                          */
"wrong DYN version",		/*117, wrodynver                       */
"function %s is not defined",		/*118, funnotdef                       */
"function %s could not be matched",		/*119, funmismat                       */
"",		/*120, bad_msg_vec                     */
"database detach completed with errors",		/*121, bad_detach                      */
"database system cannot read argument %ld",		/*122, noargacc_read                   */
"database system cannot write argument %ld",		/*123, noargacc_write                  */
"operation not supported",		/*124, read_only                       */
"%s extension error",		/*125, ext_err                         */
"not updatable",		/*126, non_updatable                   */
"no rollback performed",		/*127, no_rollback                     */
"",		/*128, bad_sec_info                    */
"",		/*129, invalid_sec_info                */
"%s",		/*130, misc_interpreted                */
"update conflicts with concurrent update",		/*131, update_conflict                 */
"product %s is not licensed",		/*132, unlicensed                      */
"object %s is in use",		/*133, obj_in_use                      */
"filter not found to convert type %ld to type %ld",		/*134, nofilter                        */
"cannot attach active shadow file",		/*135, shadow_accessed                 */
"invalid slice description language at offset %ld",		/*136, invalid_sdl                     */
"subscript out of bounds",		/*137, out_of_bounds                   */
"column not array or invalid dimensions (expected %ld, encountered %ld)",		/*138, invalid_dimension               */
"record from transaction %ld is stuck in limbo",		/*139, rec_in_limbo                    */
"a file in manual shadow %ld is unavailable",		/*140, shadow_missing                  */
"secondary server attachments cannot validate databases",		/*141, cant_validate                   */
"secondary server attachments cannot start journaling",		/*142, cant_start_journal              */
"generator %s is not defined",		/*143, gennotdef                       */
"secondary server attachments cannot start logging",		/*144, cant_start_logging              */
"invalid BLOB type for operation",		/*145, bad_segstr_type                 */
"violation of FOREIGN KEY constraint \"%s\" on table \"%s\"",		/*146, foreign_key                     */
"minor version too high found %ld expected %ld",		/*147, high_minor                      */
"transaction %ld is %s",		/*148, tra_state                       */
"transaction marked invalid by I/O error",		/*149, trans_invalid                   */
"cache buffer for page %ld invalid",		/*150, buf_invalid                     */
"there is no index in table %s with id %d",		/*151, indexnotdefined                 */
"Your user name and password are not defined. Ask your database administrator to set up an InterBase login.",		/*152, login                           */
"invalid bookmark handle",		/*153, invalid_bookmark                */
"invalid lock level %d",		/*154, bad_lock_level                  */
"lock on table %s conflicts with existing lock",		/*155, relation_lock                   */
"requested record lock conflicts with existing lock",		/*156, record_lock                     */
"maximum indexes per table (%d) exceeded",		/*157, max_idx                         */
"enable journal for database before starting online dump",		/*158, jrn_enable                      */
"online dump failure. Retry dump",		/*159, old_failure                     */
"an online dump is already in progress",		/*160, old_in_progress                 */
"no more disk/tape space.  Cannot continue online dump",		/*161, old_no_space                    */
"journaling allowed only if database has Write-ahead Log",		/*162, no_wal_no_jrn                   */
"maximum number of online dump files that can be specified is 16",		/*163, num_old_files                   */
"error in opening Write-ahead Log file during recovery",		/*164, wal_file_open                   */
"invalid statement handle",		/*165, bad_stmt_handle                 */
"Write-ahead log subsystem failure",		/*166, wal_failure                     */
"WAL Writer error",		/*167, walw_err                        */
"Log file header of %s too small",		/*168, logh_small                      */
"Invalid version of log file %s",		/*169, logh_inv_version                */
"Log file %s not latest in the chain but open flag still set",		/*170, logh_open_flag                  */
"Log file %s not closed properly; database recovery may be required",		/*171, logh_open_flag2                 */
"Database name in the log file %s is different",		/*172, logh_diff_dbname                */
"Unexpected end of log file %s at offset %ld",		/*173, logf_unexpected_eof             */
"Incomplete log record at offset %ld in log file %s",		/*174, logr_incomplete                 */
"Log record header too small at offset %ld in log file %s",		/*175, logr_header_small               */
"Log block too small at offset %ld in log file %s",		/*176, logb_small                      */
"Illegal attempt to attach to an uninitialized WAL segment for %s",		/*177, wal_illegal_attach              */
"Invalid WAL parameter block option %s",		/*178, wal_invalid_wpb                 */
"Cannot roll over to the next log file %s",		/*179, wal_err_rollover                */
"database does not use Write-ahead Log",		/*180, no_wal                          */
"cannot drop log file when journaling is enabled",		/*181, drop_wal                        */
"reference to invalid stream number",		/*182, stream_not_defined              */
"WAL subsystem encountered error",		/*183, wal_subsys_error                */
"WAL subsystem corrupted",		/*184, wal_subsys_corrupt              */
"must specify archive file when enabling long term journal for databases with round-robin log files",		/*185, no_archive                      */
"database %s shutdown in progress",		/*186, shutinprog                      */
"refresh range number %ld already in use",		/*187, range_in_use                    */
"refresh range number %ld not found",		/*188, range_not_found                 */
"CHARACTER SET %s is not defined",		/*189, charset_not_found               */
"lock time-out on wait transaction",		/*190, lock_timeout                    */
"procedure %s is not defined",		/*191, prcnotdef                       */
"parameter mismatch for procedure %s",		/*192, prcmismat                       */
"Database %s: WAL subsystem bug for pid %d\
%s",		/*193, wal_bugcheck                    */
"Could not expand the WAL segment for database %s",		/*194, wal_cant_expand                 */
"status code %s unknown",		/*195, codnotdef                       */
"exception %s not defined",		/*196, xcpnotdef                       */
"exception %d",		/*197, except                          */
"restart shared cache manager",		/*198, cache_restart                   */
"invalid lock handle",		/*199, bad_lock_handle                 */
"long-term journaling already enabled",		/*200, jrn_present                     */
"Unable to roll over; please see InterBase log.",		/*201, wal_err_rollover2               */
"WAL I/O error.  Please see InterBase log.",		/*202, wal_err_logwrite                */
"WAL writer - Journal server communication error.  Please see InterBase log.",		/*203, wal_err_jrn_comm                */
"WAL buffers cannot be increased.  Please see InterBase log.",		/*204, wal_err_expansion               */
"WAL setup error.  Please see InterBase log.",		/*205, wal_err_setup                   */
"WAL writer synchronization error for the database %s",		/*206, wal_err_ww_sync                 */
"Cannot start WAL writer for the database %s",		/*207, wal_err_ww_start                */
"database %s shutdown",		/*208, shutdown                        */
"cannot modify an existing user privilege",		/*209, existing_priv_mod               */
"Cannot delete PRIMARY KEY being used in FOREIGN KEY definition.",		/*210, primary_key_ref                 */
"Column used in a PRIMARY/UNIQUE constraint must be NOT NULL.",		/*211, primary_key_notnull             */
"Name of Referential Constraint not defined in constraints table.",		/*212, ref_cnstrnt_notfound            */
"Non-existent PRIMARY or UNIQUE KEY specified for FOREIGN KEY.",		/*213, foreign_key_notfound            */
"Cannot update constraints (RDB$REF_CONSTRAINTS).",		/*214, ref_cnstrnt_update              */
"Cannot update constraints (RDB$CHECK_CONSTRAINTS).",		/*215, check_cnstrnt_update            */
"Cannot delete CHECK constraint entry (RDB$CHECK_CONSTRAINTS)",		/*216, check_cnstrnt_del               */
"Cannot delete index segment used by an Integrity Constraint",		/*217, integ_index_seg_del             */
"Cannot update index segment used by an Integrity Constraint",		/*218, integ_index_seg_mod             */
"Cannot delete index used by an Integrity Constraint",		/*219, integ_index_del                 */
"Cannot modify index used by an Integrity Constraint",		/*220, integ_index_mod                 */
"Cannot delete trigger used by a CHECK Constraint",		/*221, check_trig_del                  */
"Cannot update trigger used by a CHECK Constraint",		/*222, check_trig_update               */
"Cannot delete column being used in an Integrity Constraint.",		/*223, cnstrnt_fld_del                 */
"Cannot rename column being used in an Integrity Constraint.",		/*224, cnstrnt_fld_rename              */
"Cannot update constraints (RDB$RELATION_CONSTRAINTS).",		/*225, rel_cnstrnt_update              */
"Cannot define constraints on views",		/*226, constaint_on_view               */
"internal gds software consistency check (invalid RDB$CONSTRAINT_TYPE)",		/*227, invld_cnstrnt_type              */
"Attempt to define a second PRIMARY KEY for the same table",		/*228, primary_key_exists              */
"cannot modify or erase a system trigger",		/*229, systrig_update                  */
"only the owner of a table may reassign ownership",		/*230, not_rel_owner                   */
"could not find table/procedure for GRANT",		/*231, grant_obj_notfound              */
"could not find column for GRANT",		/*232, grant_fld_notfound              */
"user does not have GRANT privileges for operation",		/*233, grant_nopriv                    */
"table/procedure has non-SQL security class defined",		/*234, nonsql_security_rel             */
"column has non-SQL security class defined",		/*235, nonsql_security_fld             */
"Write-ahead Log without shared cache configuration not allowed",		/*236, wal_cache_err                   */
"database shutdown unsuccessful",		/*237, shutfail                        */
"Operation violates CHECK constraint %s on view or table %s",		/*238, check_constraint                */
"invalid service handle",		/*239, bad_svc_handle                  */
"database %s shutdown in %d seconds",		/*240, shutwarn                        */
"wrong version of service parameter block",		/*241, wrospbver                       */
"unrecognized service parameter block",		/*242, bad_spb_form                    */
"service %s is not defined",		/*243, svcnotdef                       */
"long-term journaling not enabled",		/*244, no_jrn                          */
"Cannot transliterate character between character sets",		/*245, transliteration_failed          */
"WAL defined; Cache Manager must be started first",		/*246, start_cm_for_wal                */
"Overflow log specification required for round-robin log",		/*247, wal_ovflow_log_required         */
"Implementation of text subtype %d not located.",		/*248, text_subtype                    */
"Dynamic SQL Error",		/*249, dsql_error                      */
"Invalid command",		/*250, dsql_command_err                */
"Data type for constant unknown",		/*251, dsql_constant_err               */
"Cursor unknown",		/*252, dsql_cursor_err                 */
"Data type unknown",		/*253, dsql_datatype_err               */
"Declared cursor already exists",		/*254, dsql_decl_err                   */
"Cursor not updatable",		/*255, dsql_cursor_update_err          */
"Attempt to reopen an open cursor",		/*256, dsql_cursor_open_err            */
"Attempt to reclose a closed cursor",		/*257, dsql_cursor_close_err           */
"Column unknown",		/*258, dsql_field_err                  */
"Internal error",		/*259, dsql_internal_err               */
"Table unknown",		/*260, dsql_relation_err               */
"Procedure unknown",		/*261, dsql_procedure_err              */
"Request unknown",		/*262, dsql_request_err                */
"SQLDA missing or incorrect version, or incorrect number/type of variables",		/*263, dsql_sqlda_err                  */
"Count of columns does not equal count of values",		/*264, dsql_var_count_err              */
"Invalid statement handle",		/*265, dsql_stmt_handle                */
"Function unknown",		/*266, dsql_function_err               */
"Column is not a BLOB",		/*267, dsql_blob_err                   */
"COLLATION %s is not defined",		/*268, collation_not_found             */
"COLLATION %s is not valid for specified CHARACTER SET",		/*269, collation_not_for_charset       */
"Option specified more than once",		/*270, dsql_dup_option                 */
"Unknown transaction option",		/*271, dsql_tran_err                   */
"Invalid array reference",		/*272, dsql_invalid_array              */
"Array declared with too many dimensions",		/*273, dsql_max_arr_dim_exceeded       */
"Illegal array dimension range",		/*274, dsql_arr_range_error            */
"Trigger unknown",		/*275, dsql_trigger_err                */
"Subselect illegal in this context",		/*276, dsql_subselect_err              */
"Cannot prepare a CREATE DATABASE/SCHEMA statement",		/*277, dsql_crdb_prepare_err           */
"must specify column name for view select expression",		/*278, specify_field_err               */
"number of columns does not match select list",		/*279, num_field_err                   */
"Only simple column names permitted for VIEW WITH CHECK OPTION",		/*280, col_name_err                    */
"No WHERE clause for VIEW WITH CHECK OPTION",		/*281, where_err                       */
"Only one table allowed for VIEW WITH CHECK OPTION",		/*282, table_view_err                  */
"DISTINCT, GROUP or HAVING not permitted for VIEW WITH CHECK OPTION",		/*283, distinct_err                    */
"FOREIGN KEY column count does not match PRIMARY KEY",		/*284, key_field_count_err             */
"No subqueries permitted for VIEW WITH CHECK OPTION",		/*285, subquery_err                    */
"expression evaluation not supported",		/*286, expression_eval_err             */
"gen.c: node not supported",		/*287, node_err                        */
"Unexpected end of command",		/*288, command_end_err                 */
"INDEX %s",		/*289, index_name                      */
"EXCEPTION %s",		/*290, exception_name                  */
"COLUMN %s",		/*291, field_name                      */
"Token unknown",		/*292, token_err                       */
"union not supported",		/*293, union_err                       */
"Unsupported DSQL construct",		/*294, dsql_construct_err              */
"column used with aggregate",		/*295, field_aggregate_err             */
"invalid column reference",		/*296, field_ref_err                   */
"invalid ORDER BY clause",		/*297, order_by_err                    */
"Return mode by value not allowed for this data type",		/*298, return_mode_err                 */
" External functions cannot have more than 10 parameters",		/*299, extern_func_err                 */
"alias %s conflicts with an alias in the same statement",		/*300, alias_conflict_err              */
"alias %s conflicts with a procedure in the same statement",		/*301, procedure_conflict_error        */
"alias %s conflicts with a table in the same statement",		/*302, relation_conflict_err           */
"Illegal use of keyword VALUE",		/*303, dsql_domain_err                 */
"segment count of 0 defined for index %s",		/*304, idx_seg_err                     */
"A node name is not permitted in a secondary, shadow, cache or log file name",		/*305, node_name_err                   */
"TABLE %s",		/*306, table_name                      */
"PROCEDURE %s",		/*307, proc_name                       */
"cannot create index %s",		/*308, idx_create_err                  */
"Write-ahead Log with shadowing configuration not allowed",		/*309, wal_shadow_err                  */
"there are %ld dependencies",		/*310, dependency                      */
"too many keys defined for index %s",		/*311, idx_key_err                     */
"Preceding file did not specify length, so %s must include starting page number",		/*312, dsql_file_length_err            */
"Shadow number must be a positive integer",		/*313, dsql_shadow_number_err          */
"Token unknown - line %ld, char %ld",		/*314, dsql_token_unk_err              */
"there is no alias or table named %s at this scope level",		/*315, dsql_no_relation_alias          */
"there is no index %s for table %s",		/*316, indexname                       */
"table %s is not referenced in plan",		/*317, no_stream_plan                  */
"table %s is referenced more than once in plan; use aliases to distinguish",		/*318, stream_twice                    */
"table %s is referenced in the plan but not the from list",		/*319, stream_not_found                */
"Invalid use of CHARACTER SET or COLLATE",		/*320, collation_requires_text         */
"Specified domain or source column does not exist",		/*321, dsql_domain_not_found           */
"index %s cannot be used in the specified plan",		/*322, index_unused                    */
"the table %s is referenced twice; use aliases to differentiate",		/*323, dsql_self_join                  */
"illegal operation when at beginning of stream",		/*324, stream_bof                      */
"the current position is on a crack",		/*325, stream_crack                    */
"database or file exists",		/*326, db_or_file_exists               */
"invalid comparison operator for find operation",		/*327, invalid_operator                */
"Connection lost to pipe server",		/*328, conn_lost                       */
"bad checksum",		/*329, bad_checksum                    */
"wrong page type",		/*330, page_type_err                   */
"Cannot insert because the file is readonly or is on a read only medium.",		/*331, ext_readonly_err                */
"multiple rows in singleton select",		/*332, sing_select_err                 */
"cannot attach to password database",		/*333, psw_attach                      */
"cannot start transaction for password database",		/*334, psw_start_trans                 */
"invalid direction for find operation",		/*335, invalid_direction               */
"variable %s conflicts with parameter in same procedure",		/*336, dsql_var_conflict               */
"Array/BLOB/DATE data types not allowed in arithmetic",		/*337, dsql_no_blob_array              */
"%s is not a valid base table of the specified view",		/*338, dsql_base_table                 */
"table %s is referenced twice in view; use an alias to distinguish",		/*339, duplicate_base_table            */
"view %s has more than one base table; use aliases to distinguish",		/*340, view_alias                      */
"cannot add index, index root page is full.",		/*341, index_root_page_full            */
"BLOB SUB_TYPE %s is not defined",		/*342, dsql_blob_type_unknown          */
"Too many concurrent executions of the same request",		/*343, req_max_clones_exceeded         */
"duplicate specification of %s - not supported",		/*344, dsql_duplicate_spec             */
"violation of PRIMARY or UNIQUE KEY constraint \"%s\" on table \"%s\"",		/*345, unique_key_violation            */
"server version too old to support all CREATE DATABASE options",		/*346, srvr_version_too_old            */
"drop database completed with errors",		/*347, drdb_completed_with_errs        */
"procedure %s does not return any values",		/*348, dsql_procedure_use_err          */
"count of column list and variable list do not match",		/*349, dsql_count_mismatch             */
"attempt to index BLOB column in index %s",		/*350, blob_idx_err                    */
"attempt to index array column in index %s",		/*351, array_idx_err                   */
"too few key columns found for index %s (incorrect column name?)",		/*352, key_field_err                   */
"cannot delete",		/*353, no_delete                       */
"last column in a table cannot be deleted",		/*354, del_last_field                  */
"sort error",		/*355, sort_err                        */
"sort error: not enough memory",		/*356, sort_mem_err                    */
"too many versions",		/*357, version_err                     */
"invalid key position",		/*358, inval_key_posn                  */
"segments not allowed in expression index %s",		/*359, no_segments_err                 */
"sort error: corruption in data structure",		/*360, crrp_data_err                   */
"new record size of %ld bytes is too big",		/*361, rec_size_err                    */
"Inappropriate self-reference of column",		/*362, dsql_field_ref                  */
"request depth exceeded. (Recursive definition?)",		/*363, req_depth_exceeded              */
"cannot access column %s in view %s",		/*364, no_field_access                 */
"dbkey not available for multi-table views",		/*365, no_dbkey                        */
"journal file wrong format",		/*366, jrn_format_err                  */
"intermediate journal file full",		/*367, jrn_file_full                   */
"The prepare statement identifies a prepare statement with an open cursor",		/*368, dsql_open_cursor_request        */
"InterBase error",		/*369, ib_error                        */
"Cache redefined",		/*370, cache_redef                     */
"Insufficient memory to allocate page buffer cache",		/*371, cache_too_small                 */
"Log redefined",		/*372, log_redef                       */
"Log size too small",		/*373, log_too_small                   */
"Log partition size too small",		/*374, partition_too_small             */
"Partitions not supported in series of log file specification",		/*375, partition_not_supp              */
"Total length of a partitioned log must be specified",		/*376, log_length_spec                 */
"Precision must be from 1 to 18",		/*377, precision_err                   */
"Scale must be between zero and precision",		/*378, scale_nogt                      */
"Short integer expected",		/*379, expec_short                     */
"Long integer expected",		/*380, expec_long                      */
"Unsigned short integer expected",		/*381, expec_ushort                    */
"Invalid ESCAPE sequence",		/*382, like_escape_invalid             */
"service %s does not have an associated executable",		/*383, svcnoexe                        */
"Failed to locate host machine.",		/*384, net_lookup_err                  */
"Undefined service %s/%s.",		/*385, service_unknown                 */
"The specified name was not found in the hosts file or Domain Name Services.",		/*386, host_unknown                    */
"user does not have GRANT privileges on base table/view for operation",		/*387, grant_nopriv_on_base            */
"Ambiguous column reference.",		/*388, dyn_fld_ambiguous               */
"Invalid aggregate reference",		/*389, dsql_agg_ref_err                */
"navigational stream %ld references a view with more than one base table",		/*390, complex_view                    */
"Attempt to execute an unprepared dynamic SQL statement.",		/*391, unprepared_stmt                 */
"Positive value expected",		/*392, expec_positive                  */
"Incorrect values within SQLDA structure",		/*393, dsql_sqlda_value_err            */
"invalid blob id",		/*394, invalid_array_id                */
"Operation not supported for EXTERNAL FILE table %s",		/*395, extfile_uns_op                  */
"Service is currently busy: %s",		/*396, svc_in_use                      */
"stack size insufficent to execute current request",		/*397, err_stack_limit                 */
"Invalid key for find operation",		/*398, invalid_key                     */
"Error initializing the network software.",		/*399, net_init_error                  */
"Unable to load required library %s.",		/*400, loadlib_failure                 */
"Unable to complete network request to host \"%s\".",		/*401, network_error                   */
"Failed to establish a connection.",		/*402, net_connect_err                 */
"Error while listening for an incoming connection.",		/*403, net_connect_listen_err          */
"Failed to establish a secondary connection for event processing.",		/*404, net_event_connect_err           */
"Error while listening for an incoming event connection request.",		/*405, net_event_listen_err            */
"Error reading data from the connection.",		/*406, net_read_err                    */
"Error writing data to the connection.",		/*407, net_write_err                   */
"Cannot deactivate index used by an Integrity Constraint",		/*408, integ_index_deactivate          */
"Cannot deactivate primary index",		/*409, integ_deactivate_primary        */
"Client/Server Express not supported in this release",		/*410, cse_not_supported               */
"",		/*411, tra_must_sweep                  */
"Access to databases on file servers is not supported.",		/*412, unsupported_network_drive       */
"Error while trying to create file",		/*413, io_create_err                   */
"Error while trying to open file",		/*414, io_open_err                     */
"Error while trying to close file",		/*415, io_close_err                    */
"Error while trying to read from file",		/*416, io_read_err                     */
"Error while trying to write to file",		/*417, io_write_err                    */
"Error while trying to delete file",		/*418, io_delete_err                   */
"Error while trying to access file",		/*419, io_access_err                   */
"A fatal exception occurred during the execution of a user defined function.",		/*420, udf_exception                   */
"connection lost to database",		/*421, lost_db_connection              */
"User cannot write to RDB$USER_PRIVILEGES",		/*422, no_write_user_priv              */
"token size exceeds limit",		/*423, token_too_long                  */
"Maximum user count exceeded.  Contact your database administrator.",		/*424, max_att_exceeded                */
"Your login %s is same as one of the SQL role name. Ask your database administrator to set up a valid InterBase login.",		/*425, login_same_as_role_name         */
"\"REFERENCES table\" without \"(column)\" requires PRIMARY KEY on referenced table",		/*426, reftable_requires_pk            */
"The username entered is too long.  Maximum length is 31 bytes.",		/*427, usrname_too_long                */
"The password specified is too long.  Maximum length is 8 bytes.",		/*428, password_too_long               */
"A username is required for this operation.",		/*429, usrname_required                */
"A password is required for this operation",		/*430, password_required               */
"The network protocol specified is invalid",		/*431, bad_protocol                    */
"A duplicate user name was found in the security database",		/*432, dup_usrname_found               */
"The user name specified was not found in the security database",		/*433, usrname_not_found               */
"An error occurred while attempting to add the user.",		/*434, error_adding_sec_record         */
"An error occurred while attempting to modify the user record.",		/*435, error_modifying_sec_record      */
"An error occurred while attempting to delete the user record.",		/*436, error_deleting_sec_record       */
"An error occurred while updating the security database.",		/*437, error_updating_sec_db           */
"sort record size of %ld bytes is too big",		/*438, sort_rec_size_err               */
"can not define a not null column with NULL as default value",		/*439, bad_default_value               */
"invalid clause --- '%s'",		/*440, invalid_clause                  */
"too many open handles to database",		/*441, too_many_handles                */
"size of optimizer block exceeded",		/*442, optimizer_blk_exc               */
"a string constant is delimited by double quotes",		/*443, invalid_string_constant         */
"DATE must be changed to TIMESTAMP",		/*444, transitional_date               */
"attempted update on read-only database",		/*445, read_only_database              */
"SQL dialect %s is not supported in this database",		/*446, must_be_dialect_2_and_up        */
"A fatal exception occurred during the execution of a blob filter.",		/*447, blob_filter_exception           */
"Access violation.  The code attempted to access a virtual address without privilege to do so.",		/*448, exception_access_violation      */
"Datatype misalignment.  The attempted to read or write a value that was not stored on a memory boundary.",		/*449, exception_datatype_missalignment*/
"Array bounds exceeded.  The code attempted to access an array element that is out of bounds.",		/*450, exception_array_bounds_exceeded */
"Float denormal operand.  One of the floating-point operands is too small to represent a standard float value.",		/*451, exception_float_denormal_operand*/
"Floating-point divide by zero.  The code attempted to divide a floating-point value by zero.",		/*452, exception_float_divide_by_zero  */
"Floating-point inexact result.  The result of a floating-point operation cannot be represented as a deciaml fraction.",		/*453, exception_float_inexact_result  */
"Floating-point invalid operand.  An indeterminant error occurred during a floating-point operation.",		/*454, exception_float_invalid_operand */
"Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.",		/*455, exception_float_overflow        */
"Floating-point stack check.  The stack overflowed or underflowed as the result of a floating-point operation.",		/*456, exception_float_stack_check     */
"Floating-point underflow.  The exponent of a floating-point operation is less than the magnitude allowed.",		/*457, exception_float_underflow       */
"Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.",		/*458, exception_integer_divide_by_zero*/
"Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.",		/*459, exception_integer_overflow      */
"An exception occurred that does not have a description.  Exception number %X.",		/*460, exception_unknown               */
"Stack overflow.  The resource requirements of the runtime stack have exceeded the memory available to it.",		/*461, exception_stack_overflow        */
"Segmentation Fault. The code attempted to access memory without priviledges.",		/*462, exception_sigsegv               */
"Illegal Instruction. The Code attempted to perfrom an illegal operation.",		/*463, exception_sigill                */
"Bus Error. The Code caused a system bus error.",		/*464, exception_sigbus                */
"Floating Point Error. The Code caused an Arithmetic Exception or a floating point exception.",		/*465, exception_sigfpe                */
"Cannot delete rows from external files.",		/*466, ext_file_delete                 */
"Cannot update rows in external files.",		/*467, ext_file_modify                 */
"Unable to perform operation.  You must be either SYSDBA or owner of the database",		/*468, adm_task_denied                 */
"Specified EXTRACT part does not exist in input datatype",		/*469, extract_input_mismatch          */
"Service %s requires SYSDBA permissions.  Reattach to the Service Manager using the SYSDBA account.",		/*470, insufficient_svc_privileges     */
"The file %s is currently in use by another process.  Try again later.",		/*471, file_in_use                     */
"Cannot attach to services manager",		/*472, service_att_err                 */
"Metadata update statement is not allowed by the current database SQL dialect %d",		/*473, ddl_not_allowed_by_db_sql_dial  */
"operation was cancelled",		/*474, cancelled                       */
"unexpected item in service parameter block, expected %s",		/*475, unexp_spb_form                  */
"Client SQL dialect %d does not support reference to %s datatype",		/*476, sql_dialect_datatype_unsupport  */
"user name and password are required while attaching to the services manager",		/*477, svcnouser                       */
"You created an indirect dependency on uncommitted metadata. You must roll back the current transaction.",		/*478, depend_on_uncommitted_rel       */
"The service name was not specified.",		/*479, svc_name_missing                */
"Too many Contexts of Relation/Procedure/Views. Maximum allowed is 127",		/*480, too_many_contexts               */
"data type not supported for arithmetic",		/*481, datype_notsup                   */
"Database dialect being changed from 3 to 1",		/*482, dialect_reset_warning           */
"Database dialect not changed.",		/*483, dialect_not_changed             */
"Unable to create database %s",		/*484, database_create_failed          */
"Database dialect %d is not a valid dialect.",		/*485, inv_dialect_specified           */
"Valid database dialects are %s.",		/*486, valid_db_dialects               */
"SQL warning code = %ld",		/*487, sqlwarn                         */
"DATE data type is now called TIMESTAMP",		/*488, dtype_renamed                   */
"Function %s is in %s, which is not in a permitted directory for external functions.",		/*489, extern_func_dir_error           */
"value exceeds the range for valid dates",		/*490, date_range_exceeded             */
"passed client dialect %d is not a valid dialect.",		/*491, inv_client_dialect_specified    */
"Valid client dialects are %s.",		/*492, valid_client_dialects           */
"Unsupported field type specified in BETWEEN predicate.",		/*493, optimizer_between_err           */
"Services functionality will be supported in a later version  of the product",		/*494, service_not_supported           */
"data base file name (%s) already given",		/*495, gfix_db_name                    */
"invalid switch %s",		/*496, gfix_invalid_sw                 */
"incompatible switch combination",		/*497, gfix_incmp_sw                   */
"replay log pathname required",		/*498, gfix_replay_req                 */
"number of page buffers for cache required",		/*499, gfix_pgbuf_req                  */
"numeric value required",		/*500, gfix_val_req                    */
"positive numeric value required",		/*501, gfix_pval_req                   */
"number of transactions per sweep required",		/*502, gfix_trn_req                    */
"\"full\" or \"reserve\" required",		/*503, gfix_full_req                   */
"user name required",		/*504, gfix_usrname_req                */
"password required",		/*505, gfix_pass_req                   */
"subsystem name",		/*506, gfix_subs_name                  */
"\"wal\" required",		/*507, gfix_wal_req                    */
"number of seconds required",		/*508, gfix_sec_req                    */
"numeric value between 0 and 32767 inclusive required",		/*509, gfix_nval_req                   */
"must specify type of shutdown",		/*510, gfix_type_shut                  */
"please retry, specifying an option",		/*511, gfix_retry                      */
"please retry, giving a database name",		/*512, gfix_retry_db                   */
"internal block exceeds maximum size",		/*513, gfix_exceed_max                 */
"corrupt pool",		/*514, gfix_corrupt_pool               */
"virtual memory exhausted",		/*515, gfix_mem_exhausted              */
"bad pool id",		/*516, gfix_bad_pool                   */
"Transaction state %d not in valid range.",		/*517, gfix_trn_not_valid              */
"unexpected end of input",		/*518, gfix_unexp_eoi                  */
"failed to reconnect to a transaction in database %s",		/*519, gfix_recon_fail                 */
"Transaction description item unknown",		/*520, gfix_trn_unknown                */
"\"read_only\" or \"read_write\" required",		/*521, gfix_mode_req                   */
"	-sql_dialect	set database dialect n",		/*522, gfix_opt_SQL_dialect            */
"Cannot SELECT RDB$DB_KEY from a stored procedure.",		/*523, dsql_dbkey_from_non_table       */
"Precision 10 to 18 changed from DOUBLE PRECISION in SQL dialect 1 to 64-bit scaled integer in SQL dialect 3",		/*524, dsql_transitional_numeric       */
"Use of %s expression that returns different results in dialect 1 and dialect 3",		/*525, dsql_dialect_warning_expr       */
"Database SQL dialect %d does not support reference to %s datatype",		/*526, sql_db_dialect_dtype_unsupport  */
"DB dialect %d and client dialect %d conflict with respect to numeric precision %d.",		/*527, isc_sql_dialect_conflict_num    */
"WARNING: Numeric literal %s is interpreted as a floating-point",		/*528, dsql_warning_number_ambiguous   */
"value in SQL dialect 1, but as an exact numeric value in SQL dialect 3.",		/*529, dsql_warning_number_ambiguous1  */
"WARNING: NUMERIC and DECIMAL fields with precision 10 or greater are stored",		/*530, dsql_warn_precision_ambiguous   */
"as approximate floating-point values in SQL dialect 1, but as 64-bit",		/*531, dsql_warn_precision_ambiguous1  */
"integers in SQL dialect 3.",		/*532, dsql_warn_precision_ambiguous2  */
"SQL role %s does not exist",		/*533, dyn_role_does_not_exist         */
"user %s has no grant admin option on SQL role %s",		/*534, dyn_no_grant_admin_opt          */
"user %s is not a member of SQL role %s",		/*535, dyn_user_not_role_member        */
"%s is not the owner of SQL role %s",		/*536, dyn_delete_role_failed          */
"%s is a SQL role and not a user",		/*537, dyn_grant_role_to_user          */
"user name %s could not be used for SQL role",		/*538, dyn_inv_sql_role_name           */
"SQL role %s already exists",		/*539, dyn_dup_sql_role                */
"keyword %s can not be used as a SQL role name",		/*540, dyn_kywd_spec_for_role          */
"SQL roles are not supported in on older versions of the database.  A backup and restore of the database is required.",		/*541, dyn_roles_not_supported         */
"Cannot rename domain %s to %s.  A domain with that name already exists.",		/*542, dyn_domain_name_exists          */
"Cannot rename column %s to %s.  A column with that name already exists in table %s.",		/*543, dyn_field_name_exists           */
"Column %s from table %s is referenced in %s",		/*544, dyn_dependency_exists           */
"Cannot change datatype for column %s.  Changing datatype is not supported for BLOB or ARRAY columns.",		/*545, dyn_dtype_invalid               */
"New size specified for column %s must be at least %d characters.",		/*546, dyn_char_fld_too_small          */
"Cannot change datatype for %s.  Conversion from base type %s to %s is not supported.",		/*547, dyn_invalid_dtype_conversion    */
"Cannot change datatype for column %s from a character type to a non-character type.",		/*548, dyn_dtype_conv_invalid          */
"found unknown switch",		/*549, gbak_unknown_switch             */
"page size parameter missing",		/*550, gbak_page_size_missing          */
"Page size specified (%ld) greater than limit (8192 bytes)",		/*551, gbak_page_size_toobig           */
"redirect location for output is not specified",		/*552, gbak_redir_ouput_missing        */
"conflicting switches for backup/restore",		/*553, gbak_switches_conflict          */
"device type %s not known",		/*554, gbak_unknown_device             */
"protection is not there yet",		/*555, gbak_no_protection              */
"page size is allowed only on restore or create",		/*556, gbak_page_size_not_allowed      */
"multiple sources or destinations specified",		/*557, gbak_multi_source_dest          */
"requires both input and output filenames",		/*558, gbak_filename_missing           */
"input and output have the same name.  Disallowed.",		/*559, gbak_dup_inout_names            */
"expected page size, encountered \"%s\"",		/*560, gbak_inv_page_size              */
"REPLACE specified, but the first file %s is a database",		/*561, gbak_db_specified               */
"database %s already exists.  To replace it, use the -R switch",		/*562, gbak_db_exists                  */
"device type not specified",		/*563, gbak_unk_device                 */
"gds_$blob_info failed",		/*564, gbak_blob_info_failed           */
"do not understand BLOB INFO item %ld",		/*565, gbak_unk_blob_item              */
"gds_$get_segment failed",		/*566, gbak_get_seg_failed             */
"gds_$close_blob failed",		/*567, gbak_close_blob_failed          */
"gds_$open_blob failed",		/*568, gbak_open_blob_failed           */
"Failed in put_blr_gen_id",		/*569, gbak_put_blr_gen_id_failed      */
"data type %ld not understood",		/*570, gbak_unk_type                   */
"gds_$compile_request failed",		/*571, gbak_comp_req_failed            */
"gds_$start_request failed",		/*572, gbak_start_req_failed           */
" gds_$receive failed",		/*573, gbak_rec_failed                 */
"gds_$release_request failed",		/*574, gbak_rel_req_failed             */
" gds_$database_info failed",		/*575, gbak_db_info_failed             */
"Expected database description record",		/*576, gbak_no_db_desc                 */
"failed to create database %s",		/*577, gbak_db_create_failed           */
"RESTORE: decompression length error",		/*578, gbak_decomp_len_error           */
"cannot find table %s",		/*579, gbak_tbl_missing                */
"Cannot find column for BLOB",		/*580, gbak_blob_col_missing           */
"gds_$create_blob failed",		/*581, gbak_create_blob_failed         */
"gds_$put_segment failed",		/*582, gbak_put_seg_failed             */
"expected record length",		/*583, gbak_rec_len_exp                */
"wrong length record, expected %ld encountered %ld",		/*584, gbak_inv_rec_len                */
"expected data attribute",		/*585, gbak_exp_data_type              */
"Failed in store_blr_gen_id",		/*586, gbak_gen_id_failed              */
"do not recognize record type %ld",		/*587, gbak_unk_rec_type               */
"Expected backup version 1, 2, or 3.  Found %ld",		/*588, gbak_inv_bkup_ver               */
"expected backup description record",		/*589, gbak_missing_bkup_desc          */
"string truncated",		/*590, gbak_string_trunc               */
" warning -- record could not be restored",		/*591, gbak_cant_rest_record           */
"gds_$send failed",		/*592, gbak_send_failed                */
"no table name for data",		/*593, gbak_no_tbl_name                */
"unexpected end of file on backup file",		/*594, gbak_unexp_eof                  */
"database format %ld is too old to restore to",		/*595, gbak_db_format_too_old          */
"array dimension for column %s is invalid",		/*596, gbak_inv_array_dim              */
"Expected XDR record length",		/*597, gbak_xdr_len_expected           */
"cannot open backup file %s",		/*598, gbak_open_bkup_error            */
"cannot open status and error output file %s",		/*599, gbak_open_error                 */
"blocking factor parameter missing",		/*600, gbak_missing_block_fac          */
"expected blocking factor, encountered \"%s\"",		/*601, gbak_inv_block_fac              */
"a blocking factor may not be used in conjunction with device CT",		/*602, gbak_block_fac_specified        */
"user name parameter missing",		/*603, gbak_missing_username           */
"password parameter missing",		/*604, gbak_missing_password           */
" missing parameter for the number of bytes to be skipped",		/*605, gbak_missing_skipped_bytes      */
"expected number of bytes to be skipped, encountered \"%s\"",		/*606, gbak_inv_skipped_bytes          */
"Bad attribute for RDB$CHARACTER_SETS",		/*607, gbak_err_restore_charset        */
"Bad attribute for RDB$COLLATIONS",		/*608, gbak_err_restore_collation      */
"Unexpected I/O error while reading from backup file",		/*609, gbak_read_error                 */
"Unexpected I/O error while writing to backup file",		/*610, gbak_write_error                */
"could not drop database %s (database might be in use)",		/*611, gbak_db_in_use                  */
"System memory exhausted",		/*612, gbak_sysmemex                   */
"Bad attributes for restoring SQL role",		/*613, gbak_restore_role_failed        */
"SQL role parameter missing",		/*614, gbak_role_op_missing            */
"page buffers parameter missing",		/*615, gbak_page_buffers_missing       */
"expected page buffers, encountered \"%s\"",		/*616, gbak_page_buffers_wrong_param   */
"page buffers is allowed only on restore or create",		/*617, gbak_page_buffers_restore       */
"size specification either missing or incorrect for file %s",		/*618, gbak_inv_size                   */
"file %s out of sequence",		/*619, gbak_file_outof_sequence        */
"can't join -- one of the files missing",		/*620, gbak_join_file_missing          */
" standard input is not supported when using join operation",		/*621, gbak_stdin_not_supptd           */
"standard output is not supported when using split operation",		/*622, gbak_stdout_not_supptd          */
"backup file %s might be corrupt",		/*623, gbak_bkup_corrupt               */
"database file specification missing",		/*624, gbak_unk_db_file_spec           */
"can't write a header record to file %s",		/*625, gbak_hdr_write_failed           */
"free disk space exhausted",		/*626, gbak_disk_space_ex              */
"file size given (%d) is less than minimum allowed (%d)",		/*627, gbak_size_lt_min                */
"service name parameter missing",		/*628, gbak_svc_name_missing           */
"Cannot restore over current database, must be SYSDBA or owner of the existing database.",		/*629, gbak_not_ownr                   */
"\"read_only\" or \"read_write\" required",		/*630, gbak_mode_req                   */
"unable to open database",		/*631, gsec_cant_open_db               */
"error in switch specifications",		/*632, gsec_switches_error             */
"no operation specified",		/*633, gsec_no_op_spec                 */
"no user name specified",		/*634, gsec_no_usr_name                */
"add record error",		/*635, gsec_err_add                    */
"modify record error",		/*636, gsec_err_modify                 */
"find/modify record error",		/*637, gsec_err_find_mod               */
"record not found for user: %s",		/*638, gsec_err_rec_not_found          */
"delete record error",		/*639, gsec_err_delete                 */
"find/delete record error",		/*640, gsec_err_find_del               */
"find/display record error",		/*641, gsec_err_find_disp              */
"invalid parameter, no switch defined",		/*642, gsec_inv_param                  */
"operation already specified",		/*643, gsec_op_specified               */
"password already specified",		/*644, gsec_pw_specified               */
"uid already specified",		/*645, gsec_uid_specified              */
"gid already specified",		/*646, gsec_gid_specified              */
"project already specified",		/*647, gsec_proj_specified             */
"organization already specified",		/*648, gsec_org_specified              */
"first name already specified",		/*649, gsec_fname_specified            */
"middle name already specified",		/*650, gsec_mname_specified            */
"last name already specified",		/*651, gsec_lname_specified            */
"invalid switch specified",		/*652, gsec_inv_switch                 */
"ambiguous switch specified",		/*653, gsec_amb_switch                 */
"no operation specified for parameters",		/*654, gsec_no_op_specified            */
"no parameters allowed for this operation",		/*655, gsec_params_not_allowed         */
"incompatible switches specified",		/*656, gsec_incompat_switch            */
"Invalid user name (maximum 31 bytes allowed)",		/*657, gsec_inv_username               */
"Warning - maximum 8 significant bytes of password used",		/*658, gsec_inv_pw_length              */
"database already specified",		/*659, gsec_db_specified               */
"database administrator name already specified",		/*660, gsec_db_admin_specified         */
"database administrator password already specified",		/*661, gsec_db_admin_pw_specified      */
"SQL role name already specified",		/*662, gsec_sql_role_specified         */
"The license file does not exist or could not be opened for read",		/*663, license_no_file                 */
"operation already specified",		/*664, license_op_specified            */
"no operation specified",		/*665, license_op_missing              */
"invalid switch",		/*666, license_inv_switch              */
"invalid switch combination",		/*667, license_inv_switch_combo        */
"illegal operation/switch combination",		/*668, license_inv_op_combo            */
"ambiguous switch",		/*669, license_amb_switch              */
"invalid parameter, no switch specified",		/*670, license_inv_parameter           */
"switch does not take any parameter",		/*671, license_param_specified         */
"switch requires a parameter",		/*672, license_param_req               */
"syntax error in command line",		/*673, license_syntx_error             */
"The certificate was not added.  A duplicate ID exists in the license file.",		/*674, license_dup_id                  */
"The certificate was not added.  Invalid certificate ID / Key combination.",		/*675, license_inv_id_key              */
"The certificate was not removed.  The key does not exist or corresponds to a temporary evaluation license.",		/*676, license_err_remove              */
"An error occurred updating the license file.  Operation cancelled.",		/*677, license_err_update              */
"The certificate could not be validated based on the information given.  Please recheck the ID and key information.",		/*678, license_err_convert             */
"Operation failed.  An unknown error occurred.",		/*679, license_err_unk                 */
"Add license operation failed, KEY: %s ID: %s",		/*680, license_svc_err_add             */
"Remove license operation failed, KEY: %s",		/*681, license_svc_err_remove          */
"The evaluation license has already been used on this server.  You need to purchase a non-evaluation license.",		/*682, license_eval_exists             */
"found unknown switch",		/*683, gstat_unknown_switch            */
"please retry, giving a database name",		/*684, gstat_retry                     */
"Wrong ODS version, expected %d, encountered %d",		/*685, gstat_wrong_ods                 */
"Unexpected end of database file.",		/*686, gstat_unexpected_eof            */
"Can't open database file %s",		/*687, gstat_open_err                  */
"Can't read a database page",		/*688, gstat_read_err                  */
"System memory exhausted",		/*689, gstat_sysmemex                  */
