/*
 *    Program type:  API Interface
 *
 *    Description:
 *    This program prompts for and executes unknown SQL statements.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <ibase.h>
#include "align.h"
#include "example.h"

#define    MAXLEN    1024

process_statement (XSQLDA ISC_FAR * ISC_FAR * sqlda, char ISC_FAR *query);
void print_column (XSQLVAR ISC_FAR * var);
int get_statement (char ISC_FAR * buf);

typedef struct vary {
    short          vary_length;
    char           vary_string [1];
} VARY;

isc_db_handle      db = NULL;
isc_tr_handle      trans = NULL;
isc_stmt_handle    stmt = NULL;
long               status[20];
int                ret;

#ifndef ISC_INT64_FORMAT

/* Define a format string for printf.  Printing of 64-bit integers
   is not standard between platforms */

#if (defined(_MSC_VER) && defined(WIN32)) || (defined(__BORLANDC__) && defined(__WIN32__))
#define	ISC_INT64_FORMAT	"I64"
#else
#define	ISC_INT64_FORMAT	"ll"
#endif
#endif


int main (ARG(int, argc), ARG(char **, argv))
ARGLIST(int    argc)
ARGLIST(char **argv)
{
    long                   query[MAXLEN];
    XSQLDA    ISC_FAR *    sqlda;
    char                   db_name[128];

    if (argc < 2)
    {
        printf("Enter the database name:  ");
        gets(db_name);
    }
    else
    {
        strcpy(db_name, argv[1]);
    }

    if (isc_attach_database(status, 0, db_name, &db, 0, NULL))
    {
        printf("Could not open database %s\n", db_name);
        ERREXIT(status, 1);
    }                      

    /* 
    *    Allocate enough space for 20 fields.  
    *    If more fields get selected, re-allocate SQLDA later.
     */
    sqlda = (XSQLDA ISC_FAR *) malloc(XSQLDA_LENGTH (20));
    sqlda->sqln = 20;
    sqlda->version = 1;

    /* Allocate a global statement */
    if (isc_dsql_allocate_statement(status, &db, &stmt))
    {
	free (sqlda);
        ERREXIT(status,1)
    }

    /*
     *    Process SQL statements.
     */
    ret = get_statement((char ISC_FAR *) query);
    /* Use break on error or exit */
    while (ret != 1)
    {
        /* We must pass the address of sqlda, in case it
        ** gets re-allocated 
        */
        ret = process_statement((XSQLDA ISC_FAR * ISC_FAR *) &sqlda,
                                (char ISC_FAR *) query);
            if (ret == 1)
                break;
        ret = get_statement((char ISC_FAR *) query);
    }

    free (sqlda);
    if (trans)
        if (isc_commit_transaction(status, &trans))
	{
	    ERREXIT(status,1);
	};

    if (isc_detach_database(status, &db))
    {
	ERREXIT(status,1);
    };

    return ret;
}

/* 
**  Function:  process_statement
**  Process submitted statement.  On any fundamental error, return status 1,
**  which will do an isc_print_status and exit the program.
**  On user errors, found in parsing or executing go to status 2,
**  which will print the error and continue.
*/

process_statement (ARG(XSQLDA ISC_FAR * ISC_FAR *, sqldap),
                   ARG(char ISC_FAR *, query))
ARGLIST(XSQLDA  **sqldap)
ARGLIST(char    *query)
{
    long            buffer[MAXLEN];
    XSQLDA  ISC_FAR *sqlda;
    XSQLVAR ISC_FAR *var;
    short           num_cols, i;
    short           length, alignment, type, offset;
    long            fetch_stat;
    static char     stmt_info[] = { isc_info_sql_stmt_type };
    char            info_buffer[20];
    short           l;
    long            statement_type;

    sqlda = *sqldap;

    /* Start a transaction if we are not in one */
    if (!trans)
        if (isc_start_transaction(status, &trans, 1, &db, 0, NULL))
        {
            ERREXIT(status, 1)
        }

    if (isc_dsql_prepare(status, &trans, &stmt, 0, query, SQL_DIALECT_V6, sqlda))
    {
        ERREXIT(status,2)
    }

    /* What is the statement type of this statement? 
    **
    ** stmt_info is a 1 byte info request.  info_buffer is a buffer
    ** large enough to hold the returned info packet
    ** The info_buffer returned contains a isc_info_sql_stmt_type in the first byte, 
    ** two bytes of length, and a statement_type token.
    */
    if (!isc_dsql_sql_info(status, &stmt, sizeof (stmt_info), stmt_info,
        sizeof (info_buffer), info_buffer))
    {
        l = (short) isc_vax_integer((char ISC_FAR *) info_buffer + 1, 2);
        statement_type = isc_vax_integer((char ISC_FAR *) info_buffer + 3, l);
    }


    /*
     *    Execute a non-select statement.
     */
    if (!sqlda->sqld)
    {
        if (isc_dsql_execute(status, &trans, &stmt, SQL_DIALECT_V6, NULL))
        {
            ERREXIT(status,2)
        }

        /* Commit DDL statements if that is what sql_info says */

        if (trans && (statement_type == isc_info_sql_stmt_ddl))
        {
            printf ("\tCommitting...\n");
            if (isc_commit_transaction(status, &trans))
            {
                ERREXIT(status, 2)
            }    
        }

        return 0;
    }

    /*
     *    Process select statements.
     */

    num_cols = sqlda->sqld;

    /* Need more room. */
    if (sqlda->sqln < num_cols)
    {
        *sqldap = sqlda = (XSQLDA ISC_FAR *) realloc(sqlda,
                                                XSQLDA_LENGTH (num_cols));
        sqlda->sqln = num_cols;
        sqlda->version = 1;

        if (isc_dsql_describe(status, &stmt, SQL_DIALECT_V6, sqlda))
        {
            ERREXIT(status,2)
        }

        num_cols = sqlda->sqld;
    }

    /*
     *     Set up SQLDA.
     */
    for (var = sqlda->sqlvar, offset = 0, i = 0; i < num_cols; var++, i++)
    {
        length = alignment = var->sqllen;
        type = var->sqltype & ~1;

        if (type == SQL_TEXT)
            alignment = 1;
        else if (type == SQL_VARYING)
        {   
            length += sizeof (short) + 1;
            alignment = sizeof (short);
        }
        /*  RISC machines are finicky about word alignment
        **  So the output buffer values must be placed on
        **  word boundaries where appropriate
        */
        offset = ALIGN (offset, alignment);
        var->sqldata = (char ISC_FAR *) buffer + offset;
        offset += length;
        offset = ALIGN (offset, sizeof (short));
        var->sqlind = (short*) ((char ISC_FAR *) buffer + offset);
        offset += sizeof  (short);
    }

    if (isc_dsql_execute(status, &trans, &stmt, SQL_DIALECT_V6, NULL))
    {
        ERREXIT(status,2)
    }

    /*
     *    Print rows.
     */

    while ((fetch_stat = isc_dsql_fetch(status, &stmt, SQL_DIALECT_V6, sqlda)) == 0)
    {
        for (i = 0; i < num_cols; i++)
        {
            print_column((XSQLVAR ISC_FAR *) &sqlda->sqlvar[i]);
        }
        printf("\n");
    }

    /* Close cursor */
    if (isc_dsql_free_statement(status, &stmt, DSQL_close))
    {
	ERREXIT (status,2);
    };

    if (fetch_stat != 100L)
    {
        ERREXIT(status,2)
    }

    return 0;
}

/*
 *    Print column's data.
 */
void print_column (ARG(XSQLVAR ISC_FAR *, var))
ARGLIST(XSQLVAR    *var)
{
    short       dtype;
    char        data[MAXLEN], *p;
    char        blob_s[20], date_s[25];
    VARY        *vary;
    short       len; 
    struct tm   times;
    ISC_QUAD    bid;

    dtype = var->sqltype & ~1;
    p = data;

    /* Null handling.  If the column is nullable and null */
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
		if (var->sqlscale > 0) len += var->sqlscale;
                break;
            case SQL_LONG:
                len = 11;
		if (var->sqlscale > 0) len += var->sqlscale;
                break;
	    case SQL_INT64:
		len = 21;
		if (var->sqlscale > 0) len += var->sqlscale;
		break;
            case SQL_FLOAT:
                len = 15;
                break;
            case SQL_DOUBLE:
                len = 24;
                break;
	    case SQL_TIMESTAMP:
		len = 24;
		break;
	    case SQL_TYPE_DATE:
		len = 10;
		break;
	    case SQL_TYPE_TIME:
		len = 13;
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
            case SQL_LONG:
	    case SQL_INT64:
		{
		ISC_INT64	value;
		short		field_width;
		short		dscale;
		switch (dtype)
		    {
		    case SQL_SHORT:
			value = (ISC_INT64) *(short ISC_FAR *) var->sqldata;
			field_width = 6;
			break;
		    case SQL_LONG:
			value = (ISC_INT64) *(long ISC_FAR *) var->sqldata;
			field_width = 11;
			break;
		    case SQL_INT64:
			value = (ISC_INT64) *(ISC_INT64 ISC_FAR *) var->sqldata;
			field_width = 21;
			break;
		    }
		dscale = var->sqlscale;
		if (dscale < 0)
		    {
		    ISC_INT64	tens;
		    short	i;

		    tens = 1;
		    for (i = 0; i > dscale; i--)
			tens *= 10;

		    if (value >= 0)
			sprintf (p, "%*" ISC_INT64_FORMAT "d.%0*" ISC_INT64_FORMAT "d",
				field_width - 1 + dscale, 
				(ISC_INT64) value / tens,
				-dscale, 
				(ISC_INT64) value % tens);
		    else if ((value / tens) != 0)
			sprintf (p, "%*" ISC_INT64_FORMAT "d.%0*" ISC_INT64_FORMAT "d",
				field_width - 1 + dscale, 
				(ISC_INT64) (value / tens),
				-dscale, 
				(ISC_INT64) -(value % tens));
		    else
			sprintf (p, "%*s.%0*" ISC_INT64_FORMAT "d",
				field_width - 1 + dscale, 
				"-0",
				-dscale, 
				(ISC_INT64) -(value % tens));
		    }
		else if (dscale)
		    sprintf (p, "%*" ISC_INT64_FORMAT "d%0*d", 
			    field_width, 
			    (ISC_INT64) value,
			    dscale, 0);
		else
		    sprintf (p, "%*" ISC_INT64_FORMAT "d%",
			    field_width, 
			    (ISC_INT64) value);
		};
                break;

            case SQL_FLOAT:
                sprintf(p, "%15g ", *(float ISC_FAR *) (var->sqldata));
                break;

            case SQL_DOUBLE:
		sprintf(p, "%24f ", *(double ISC_FAR *) (var->sqldata));
                break;

	    case SQL_TIMESTAMP:
		isc_decode_timestamp((ISC_TIMESTAMP ISC_FAR *)var->sqldata, &times);
		sprintf(date_s, "%04d-%02d-%02d %02d:%02d:%02d.%04d",
				times.tm_year + 1900,
				times.tm_mon+1,
				times.tm_mday,
				times.tm_hour,
				times.tm_min,
				times.tm_sec,
				((ISC_TIMESTAMP *)var->sqldata)->timestamp_time % 10000);
		sprintf(p, "%*s ", 24, date_s);
		break;

	    case SQL_TYPE_DATE:
		isc_decode_sql_date((ISC_DATE ISC_FAR *)var->sqldata, &times);
		sprintf(date_s, "%04d-%02d-%02d",
				times.tm_year + 1900,
				times.tm_mon+1,
				times.tm_mday);
		sprintf(p, "%*s ", 10, date_s);
		break;

	    case SQL_TYPE_TIME:
		isc_decode_sql_time((ISC_TIME ISC_FAR *)var->sqldata, &times);
		sprintf(date_s, "%02d:%02d:%02d.%04d",
				times.tm_hour,
				times.tm_min,
				times.tm_sec,
				(*((ISC_TIME *)var->sqldata)) % 10000);
		sprintf(p, "%*s ", 13, date_s);
		break;

            case SQL_BLOB:
            case SQL_ARRAY:
                /* Print the blob id on blobs or arrays */
                bid = *(ISC_QUAD ISC_FAR *) var->sqldata;
                sprintf(blob_s, "%08x:%08x", bid.isc_quad_high, bid.isc_quad_low);
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
 *    Prompt for and get input.
 *    Statements are terminated by a semicolon.
 */
int get_statement (ARG(char ISC_FAR *,buf))
ARGLIST(char *buf)
{
    short   c;
    char    *p;
    int     cnt;

    p = buf;
    cnt = 0;
    printf("SQL> ");

    for (;;)
    {
        if ((c = getchar()) == EOF)
            return 1;

        if (c == '\n')
        {
            /* accept "quit" or "exit" to terminate application */

            if (!strncmp(buf, "exit", 4))
                return 1;
            if (!strncmp(buf, "quit", 4))
                return 1;

            /* Search back through white space looking for ';'.*/
            while (cnt && isspace(*(p - 1)))
            {
                p--;
                cnt--;
            }
            if (*(p - 1) == ';')
            {
                *p++ = '\0';

                return 0;
            }
            *p++ = ' ';
            printf("CON> ");
        }
        else
        {
            *p++ = (char)c;
        }
        cnt++;
    }
}

