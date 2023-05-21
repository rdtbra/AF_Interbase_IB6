/*
 *  Program type:   Embedded Dynamic SQL
 *
 *	Description:
 *		This program prompts for and executes unknown SQL statements.
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

#include "example.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <ibase.h>
#include "align.h"

#define	MAXLEN	1024
#define EOF -1

void process_statement (XSQLDA **sqlda, char *query);
void print_column (XSQLVAR *var);
int get_statement (char *buf);

typedef struct vary {
	short       vary_length;
	char        vary_string [1];
} VARY;

EXEC SQL
	SET DATABASE db = COMPILETIME "employee.gdb";



int main(ARG(int, argc), ARG(char **, argv))
ARGLIST(int argc)
ARGLIST(char **argv)
{
	char	query[MAXLEN];
	XSQLDA	*sqlda;
	char	db_name[128];

	if (argc < 2)
	{
		printf("Enter the database name:  ");
		gets(db_name);
	}
	else
		strcpy(db_name, *(++argv));

   	EXEC SQL
	        SET TRANSACTION;


	EXEC SQL
		CONNECT :db_name AS db;

	if (SQLCODE)
	{
		printf("Could not open database %s\n", db_name);
		return(1);
	}

	EXEC SQL
		COMMIT;


	/*
	 *	Allocate enough space for 20 fields.
	 *	If more fields get selected, re-allocate SQLDA later.
	 */
	sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH (20));
	sqlda->sqln = 20;
	sqlda->version = 1;


	/*
	 *	Process SQL statements.
	 */
	while (get_statement(query))
	{
		process_statement(&sqlda, query);
	}

	EXEC SQL
		DISCONNECT db;

	return(0);
}


void process_statement(ARG(XSQLDA ISC_FAR * ISC_FAR *, sqldap), 
		ARG(char ISC_FAR *, query))
	ARGLIST(XSQLDA	**sqldap)
	ARGLIST(char	*query)
{
	long	buffer[MAXLEN];
	XSQLVAR	*var;
	XSQLDA *sqlda;
	short	num_cols, i;
	short	length, alignment, type, offset;

	sqlda = *sqldap;
	EXEC SQL
		WHENEVER SQLERROR GO TO Error;

/*     Start transaction only when needed. */

        if ( !gds__trans )
         {
	    EXEC SQL
		    SET TRANSACTION;
         }

	EXEC SQL
		PREPARE q INTO SQL DESCRIPTOR sqlda FROM :query;

	EXEC SQL
		DESCRIBE q INTO SQL DESCRIPTOR sqlda;

	/*
	 *	Execute a non-select statement.
	 */
	if (!sqlda->sqld)
	{
		EXEC SQL
			EXECUTE q;

	return ;
	}

	/*
	 *	Process select statements.
	 */

	num_cols = sqlda->sqld;

	/* Need more room. */
	if (sqlda->sqln < num_cols)
	{
		free(sqlda);
		sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH (num_cols));
		sqlda->sqln = num_cols;
		sqlda->sqld = num_cols;
		sqlda->version = 1;

		EXEC SQL
			DESCRIBE q INTO SQL DESCRIPTOR sqlda;

		num_cols = sqlda->sqld;
	}

	/* Open the cursor. */
	EXEC SQL
		DECLARE c CURSOR FOR q;
	EXEC SQL
		OPEN c;

	/*
	 *	 Set up SQLDA.
	 */
	for (var = sqlda->sqlvar, offset = 0, i = 0; i < num_cols; var++, i++)
	{
		length = alignment = var->sqllen;
		type = var->sqltype & ~1;

		if (type == SQL_TEXT)
			alignment = 1;
		else if (type == SQL_VARYING)
		{   
			length += sizeof (short);
			alignment = sizeof (short);
		}   
		offset = ALIGN (offset, alignment);
		var->sqldata = (char*) buffer + offset;
		offset += length;
		offset = ALIGN (offset, sizeof (short));
		var->sqlind = (short*) ((char*) buffer + offset);
		offset += sizeof  (short);
	}

	/*
	 *	Print rows.
	 */
	while (SQLCODE == 0)
	{
		EXEC SQL
			FETCH c USING SQL DESCRIPTOR sqlda;

		if (SQLCODE == 100)
			break;

		for (i = 0; i < num_cols; i++)
		{
			print_column(&sqlda->sqlvar[i]);
		}
		printf("\n");
	}

	EXEC SQL
		CLOSE c;

	EXEC SQL
		COMMIT;
	return;

Error:


       EXEC SQL
           WHENEVER SQLERROR CONTINUE;

	printf("Statement failed.  SQLCODE = %d\n", SQLCODE);
	isc_print_status(gds__status);
}


/*
 *	Print column's data.
 */
void print_column(ARG(XSQLVAR ISC_FAR *, var))
ARGLIST(XSQLVAR	*var)
{
	short		dtype;
	char		data[MAXLEN], *p;
	char		blob_s[20], date_s[20];
	VARY		*vary;
	short		len; struct tm	times;
	ISC_QUAD	bid;

	dtype = var->sqltype & ~1;
	p = data;

	if ((var->sqltype & 1) && (*var->sqlind < 0))
	{
		switch (dtype)
		{
			case SQL_TEXT:
			case SQL_VARYING:
				len = var->sqllen;
				break;
			case SQL_SHORT:
				len = 6;
				break;
			case SQL_LONG:
				len = 10;
				break;
			case SQL_FLOAT:
			case SQL_DOUBLE:
				len = 22;
				break;
			case SQL_DATE:
				len = 10;
				break;
			case SQL_BLOB:
			case SQL_ARRAY:
			default:
				len = 17;
				break;
		}
		if ((dtype == SQL_TEXT) || (dtype == SQL_VARYING))
			sprintf(p, "%-*s ", len, "NULL");
		else
			sprintf(p, "%*s ", len, "NULL");
	}
	else
	{
		switch (dtype)
		{
			case SQL_TEXT:
				sprintf(p, "%.*s ", var->sqllen, var->sqldata);
				break;

			case SQL_VARYING:
				vary = (VARY*) var->sqldata;
				vary->vary_string[vary->vary_length] = '\0';
				sprintf(p, "%-*s ", var->sqllen, vary->vary_string);
				break;

			case SQL_SHORT:
				sprintf(p, "%6d ", *(short *) (var->sqldata));
				break;

			case SQL_LONG:
#if !(defined __osf__ && defined __alpha)
				sprintf(p, "%10d ", *(long *) (var->sqldata));
#else
				sprintf(p, "%10d ", *(int *) (var->sqldata));
#endif
				break;

			case SQL_FLOAT:
				sprintf(p, "%22f ", *(float *) (var->sqldata));
				break;

			case SQL_DOUBLE:
				/* Detect numeric/decimal scale and handle format */
				if (var->sqlscale)
					sprintf(p, "%22.*f ", -var->sqlscale, 
						*(double *) (var->sqldata));
				else
					sprintf(p, "%22f ", 
						*(double *) (var->sqldata));
				break;

			case SQL_DATE:
				isc_decode_date((ISC_QUAD *)var->sqldata, &times);
				sprintf(date_s, "%2d/%d/%2d",
						times.tm_mday, times.tm_mon+1, times.tm_year);
				sprintf(p, "%*s ", 10, date_s);
				break;

			case SQL_BLOB:
			case SQL_ARRAY:
				bid = *(ISC_QUAD *) var->sqldata;
				sprintf(blob_s, "%x:%x", bid.gds_quad_high, bid.gds_quad_low);
				sprintf(p, "%17s ", blob_s);
				break;

			default:
				break;
		}
	}

	while (*p)
	{
		putchar(*p++);
	}
	
}


/*
 *	Prompt for and get input.
 *	Statements are terminated by a semicolon.
 */
int get_statement(ARG(char ISC_FAR *, buf))
	ARGLIST(char	*buf)
{
	short	c;
	char	*p;
	int		cnt;

	p = buf;
	cnt = 0;
	printf("SQL> ");

	for (;;)
	{
		if ((c = getchar()) == EOF)
			return 0;

		if (c == '\n')
		{
			/* accept "quit" or "exit" to terminate application */

			if (!strncmp(buf, "exit;", 5))
				return 0;
			if (!strncmp(buf, "quit;", 5))
				return 0;

			/* Search back through any white space looking for ';'.*/
			while (cnt && isspace(*(p - 1)))
			{
				p--;
				cnt--;
			}
			if (*(p - 1) == ';')
			{
				*p++ = '\0';
				return 1;
			}

			*p++ = ' ';
			printf("CON> ");
		}
		else
			*p++ = c;
		cnt++;
	}
}
