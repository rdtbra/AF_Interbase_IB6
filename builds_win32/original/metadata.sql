/* The contents of this file are subject to the Interbase Public
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
/* This database file is used by GPRE for metadata lookup on behalf of the .e
 * files.
 */
SET SQL DIALECT 1;
CONNECT '../metadata.gdb';
DROP DATABASE;

CREATE DATABASE '../metadata.gdb' PAGE_SIZE 1024;

/* Domain definitions */
CREATE DOMAIN QLI$PROCEDURE_NAME AS CHAR(31);
CREATE DOMAIN QLI$PROCEDURE AS BLOB SUB_TYPE TEXT SEGMENT SIZE 80;
CREATE DOMAIN PYXIS$FORM AS BLOB SUB_TYPE 0 SEGMENT SIZE 80;
CREATE DOMAIN PYXIS$FORM_NAME AS CHAR(31);
CREATE DOMAIN PYXIS$FORM_TYPE AS CHAR(16);

/* Table: PYXIS$FORMS, Owner: DAVES */
CREATE TABLE PYXIS$FORMS (PYXIS$FORM_NAME PYXIS$FORM_NAME,
        PYXIS$FORM_TYPE PYXIS$FORM_TYPE,
        PYXIS$FORM PYXIS$FORM);

/* Table: QLI$PROCEDURES, Owner: BUILDER */
CREATE TABLE QLI$PROCEDURES (QLI$PROCEDURE_NAME QLI$PROCEDURE_NAME,
        QLI$PROCEDURE QLI$PROCEDURE);

/* Table: RDB$ROLES, Owner: BUILDER (For Interbase server < 5.0)
 * CREATE TABLE RDB$ROLES (RDB$ROLE_NAME CHAR(31) default null,
 *         RDB$OWNER_NAME CHAR(31) default null);
 */

/*  Index definitions for all user tables */
CREATE UNIQUE INDEX PYXIS$INDEX ON PYXIS$FORMS(PYXIS$FORM_NAME);
CREATE UNIQUE INDEX QLI$PROCEDURES_IDX1 ON QLI$PROCEDURES(QLI$PROCEDURE_NAME);

