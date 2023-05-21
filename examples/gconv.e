/***********************************************************************
 *                                                                     *
 *  gconv.e  Author:  Paul McGee, Borland International                *
 *          Purpose:  To provide an import facility for different      *
 *                    types of external files.                         *
 *                                                                     *
 *                                                                     *
 *  Borland   DOES NOT ASSUME RESPONSIBILITY FOR THE RESULTS OF        *
 *  USING THIS PROGRAM.  It is NOT covered by the normal Technical     *
 *  Support Agreements between Borland and it's customers.             *
 *                                                                     *
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
 ***********************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <time.h>

/*DATABASE CONVERT = FILENAME "atlas.gdb";*/
#ifdef apollo
#include "/interbase/include/gds.ins.c"
#else
#include "/usr/interbase/include/gds.h"
#endif

long
   *CONVERT;                /* database handle */

long
   SQLCODE;                /* SQL status code */

long isc_status_vec[20];
long *isc_def_trans;

typedef struct {
    long                gds_quad_high;
    unsigned long        gds_quad_low;
} ISC_QUADW;

ISC_QUADW null_blob_id = {0,0};

static int
   *dummy_status_null = 0;        /* dummy status vector */


#define VERSION_NUM    "3.3F"

#ifndef NULL
#define NULL        '\0'
#endif

#define BELL  putchar(7)
#define FALSE 0
#define TRUE  1

#define MAX_BUFFER  32767 /* 32K buffer for reads from disk */
#define MAX         3072
#define LINE_MAX    3072
#define MAX_FIELDS  100
#define MAX_SMALL   256
#define MAX_ARRAYS  40
#define PRINT_LEVEL 100
#define MISSING     -1

#define UPPER(c)                 ((c >= 'a' && c<= 'z') ? c + 'A' - 'a': c )
#define SKIP_TO(s, c)                while (*s && (*s != c)) s++
#define SKIP_TO_LEN(s, c, l)                while (*s && (*s != c)) { s++; l++; }
#define EAT_WHITE(s)                while (*s && ((*s == ` `) || (*s == '\t'))) s++
#define LETTER(c)        (c >= 'A' && c <= 'Z')
#define DIGIT(c)        (c >= '0' && c <= '9')

static char        *months [] =
    {
    "JANUARY",
    "FEBRUARY",
    "MARCH",
    "APRIL",
    "MAY",
    "JUNE",
    "JULY",
    "AUGUST",
    "SEPTEMBER",
    "OCTOBER",
    "NOVEMBER",
    "DECEMBER",
    0
    };

/* define macros for types of files to convert */
#define FIXED '1'
#define SINGLE '2'
#define QUOTES '3'
#define QCOMMA '4'
#define LINES '5'

/* define field types */

#define F_ARRAY        262

#define F_CHAR       14
#define F_VARY       37
#define F_CSTRING    40
#define F_DATE       35
#define F_SHORT      7
#define F_LONG       8
#define F_QUAD       9
#define F_FLOAT      10
#define F_DOUBLE     27
#define F_D_FLOAT    11
#define F_BLOB       261
#define F_BLOB_ID    45


SQLDA *sqlda;
char DSQL_STRING [2000];

char conversion_type;

/* for SINGLE, specify separator character - example:  1,'~' , default is comma */

char separator = NULL;
char quote = NULL;

char input_buffer[MAX_BUFFER];
char db_name [MAX_SMALL], template_name [MAX_SMALL], target_relation [MAX_SMALL],
     input_file [MAX_SMALL], line_char[MAX_SMALL];
char title [81];
int  ignore_cols [MAX_FIELDS];
long start_line = 0;
long end_line = 0;
long lines_processed = 0;

int GCONV_EOF = 0;
int PRINT_VERSION = 0;
int DEBUG_PRINT = 0;

struct input_fields_t
    {
    char field_name [31];
    int field_code;
    int array_field_type;
    int field_length;
    int user_length;
    int max_length;
    short ind_field;
    int len_field;
    ISC_QUADW blob;
    ISC_QUADW array;
    int array_num;
    int array_start;
    int sqlda_match;
    };
struct input_fields_t input_fields[MAX_FIELDS];

int num_of_fields = 0;
int VALIDATE = 0;
int BLOBS = 0;
int ARRAYS = 0;
int COMMENTS = 0;
char BLOB_END [MAX_SMALL];
char ARRAY_END [MAX_SMALL];

typedef struct {
   short   array_bound_lower;
   short   array_bound_upper;
} ARRAY_BOUND;
 
typedef struct {
   unsigned char    array_desc_dtype;
   char             array_desc_scale;
   unsigned short   array_desc_length;
   char             array_desc_field_name [32];
   char             array_desc_relation_name [32];
   short            array_desc_dimensions;
   short            array_desc_flags;
   ARRAY_BOUND      array_desc_bounds [16];
} ARRAY_DESC;

typedef struct {
   long index;
   short max_db_elements;
   short current_rec_elements;
   short end_element;
   } ARRAY_ITEM;

typedef struct {
   short valid;
   short field_size;
   short field_num;
   long tot_array_size;
   ARRAY_ITEM array_items[16];
   } ARRAY_FIELD;
                                     
ARRAY_FIELD array_fields[MAX_ARRAYS];

char *array_buffers[MAX_ARRAYS];

int MAX_ARRAY_NUM = 0;
ARRAY_DESC mydesc[MAX_ARRAYS];

void center_print(), check_garbage(), get_blob(), 
    get_array(), get_ignore(), move();
char *trim();

/**************************************
 *
 *        m a i n
 *
 **************************************
 *
 * Functional description
 *
 *        Utility for loading ascii data files into database.
 *        See gconv.readme for more info.
 *
 **************************************/
main (argc, argv)
    int argc;
    char **argv;
{
    FILE *fp1;
    char input_string [MAX], *s;
    int i, j, k;
    int num_of_recs = 0;
    int recs_processed = 0;
    int num_of_ignores = 0;
    int match = 0;

    parse_args (argc, argv);

    title[0] = NULL;
    printf ("\n");
    strcpy (title, "File Conversion of ");
    strcat (title, input_file);
    strcat (title," to ");
    strcat (title, db_name);
    center_print (80, title);
    printf ("\n");

    /* READY db_name AS CONVERT */
    isc_attach_database (isc_status_vec, 0, db_name, &CONVERT, 0, (char*) 0);;
    if (isc_status_vec[1])
        {
        printf ("\nAborting: Error opening %s!\n", db_name);
        isc_print_status (isc_status_vec);
        exit (1);
        }

    if (PRINT_VERSION) 
        {
        printf ( "\tgconv version: V%s.\n", VERSION_NUM);
        isc_version (&CONVERT, NULL, NULL );
        printf ("\n\n");
        }

    /* START_TRANSACTION isc_def_trans */    
    isc_start_transaction (isc_status_vec, &isc_def_trans, 1, &CONVERT, 0, (char*) 0);
    if (isc_status_vec[1])
        {
        printf ("\nAborting: Error starting transaction!\n");
        isc_print_status (isc_status_vec);
        isc_detach_database (*dummy_status_null, CONVERT);
        exit (1);
        }
    
    if (VALIDATE)
        {
        printf ("Validating max field lengths only!\n");
        validate_max_size (input_fields);
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (0);
        }

    num_of_fields = conversion_info (input_fields);
    if (DEBUG_PRINT)
        printf ("\n\nnum of fields = %d\n", num_of_fields);

    get_fields (input_fields, target_relation, num_of_fields);
    
    /* check for valid conversion type if BLOBS or ARRAYS are present */
    if (BLOBS || ARRAYS)
        {
        if (conversion_type != LINES)
            {
            BELL;
            printf ("Abort: %s are only supported using file type 5 (LINES)\n", BLOBS ? "Blobs" : "Arrays" );
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
            }
        }

    num_of_ignores = setup_dsql (input_fields, target_relation, num_of_fields);

    sqlda = (SQLDA*) malloc (SQLDA_LENGTH (num_of_fields - num_of_ignores));
    sqlda->sqln = num_of_fields - num_of_ignores;

    /* EXEC SQL PREPARE Q FROM :DSQL_STRING; */
    isc_embed_dsql_prepare (isc_status_vec, &CONVERT, &isc_def_trans, "Q ", (unsigned short) 0, DSQL_STRING, 0, 0L);
    SQLCODE = isc_sqlcode (isc_status_vec);

    if ( SQLCODE ) {
        printf ( "\nUnable to PREPARE the DSQL string!\n" );
        isc_print_status ( isc_status_vec );
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }
    
    sqlda->sqld = sqlda->sqln;
        
    for (i = 0, k = 0; i < num_of_fields; i++)
        {
        /* this is checking for fields to exclude */

        input_fields[i].sqlda_match = -1;
    
        if (!ignore_cols [i])
            {
            if (input_fields[i].field_code != F_BLOB && input_fields[i].field_code != F_ARRAY)
                {
                sqlda->sqlvar[k].sqltype = SQL_TEXT + 1;
                }
            else if ( input_fields[i].field_code == F_BLOB ) 
                {
                sqlda->sqlvar[k].sqltype = SQL_BLOB + 1;
                sqlda->sqlvar[k].sqllen = 8;
                }
            else /* must be an array */
                {
                sqlda->sqlvar[k].sqltype = SQL_ARRAY + 1;
                sqlda->sqlvar[k].sqllen = 8;
                }
            sqlda->sqlvar[k].sqlind = &input_fields[i].ind_field;
            input_fields[i].sqlda_match = k;
            if (DEBUG_PRINT)
                printf ("\n%s\n\tField %d SQLDA match is %d.\n\tk = %d.\n\n",
                        input_fields[i].field_name, i, input_fields[i].sqlda_match, k);
            k++;
            }
        }

    if ((fp1 = fopen (input_file, "r")) == NULL)
        {
        printf ("Error opening \"%s\" for reading.\n", input_file);
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }

    while (!GCONV_EOF)
        {
        for (i = 0; i < num_of_fields; i++)
            {
            input_fields[i].ind_field = 0;
            if ( input_fields[i].field_code != F_BLOB && input_fields[i].field_code != F_ARRAY )
                    input_fields[i].len_field = 0;
            }

        switch (conversion_type)
            {
            case FIXED:       
            case SINGLE:     
            case QUOTES:    
            case QCOMMA:   
                get_whole_line (input_string, fp1, TRUE);
                break;
            case LINES:   
                build_whole_line (input_string, fp1, num_of_fields);
                break;
            }

        if (GCONV_EOF) 
            break;

        recs_processed++;
        for (i = 0, s = input_string; i < num_of_fields; i++)
            get_input_field (&sqlda->sqlvar[input_fields[i].sqlda_match],
                &input_fields[i], &s, separator, quote, 
                recs_processed);

        if (DEBUG_PRINT)
            {
            printf ("\nInput data:\n");
            for (i = 0; i < num_of_fields; i++) {
                printf ("\t%2d: len = %d: match = %d\n", 
                    i, input_fields[i].len_field, input_fields[i].sqlda_match);
                printf ("\t%2d: field_name = %s.\n", i, input_fields[i].field_name );
                }

            printf ("\nSQLDA data:\n");
            for (i = 0, j = 0; i < num_of_fields; i++) {
                if (ignore_cols[i])
                    printf ( "\t%2d: IGNORED!\n", i );
                else {
                    printf ( "\t%2d:\tLen  = %d.\t\tInd  = %d.\n", i,
                        sqlda->sqlvar[j].sqllen,
                        *sqlda->sqlvar[j].sqlind );
                    printf ( "\t   \tData = %*s.\n",   
                        sqlda->sqlvar[j].sqllen > 0 ? sqlda->sqlvar[j].sqllen : 1,
                        sqlda->sqlvar[j].sqllen > 0 ? sqlda->sqlvar[j].sqldata : "" );
                    j++;
                    }
                }
            printf ("\n\n");
            }

        /* EXEC SQL EXECUTE Q USING DESCRIPTOR sqlda */

        isc_embed_dsql_execute (isc_status_vec, &isc_def_trans, "Q ", 0, sqlda);
        SQLCODE = isc_sqlcode (isc_status_vec);

        if (DEBUG_PRINT) {
            printf ("SQLCODE: %d\n\n", SQLCODE);
            }

        if ( SQLCODE ) {
            printf ("Error occurred around line # %ld\n", lines_processed);
            printf ( "\nUnable to EXECUTE the DSQL string!\n" );
            isc_print_status ( isc_status_vec );
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
            }
    
        if (SQLCODE == 0)
            num_of_recs++;

        if ((num_of_recs % PRINT_LEVEL) == 0)
            printf ("Processed %d records.\n", num_of_recs);
        }

    printf ("\n\n");
    printf ("Processed %d record(s).\n\n",num_of_recs);

    /* EXEC SQL COMMIT WORK */
    
    isc_commit_transaction (isc_status_vec, &isc_def_trans);
    SQLCODE = isc_sqlcode (isc_status_vec);
  
    if ( SQLCODE ) {
        printf ( "\nUnable to COMMIT the INSERTS!\n" );
        isc_print_status ( isc_status_vec );
        exit (1);
        }

    /* FINISH */
    isc_detach_database ( isc_status_vec, &CONVERT );

    printf ("FINISHED");
    printf ("\n\n");

    exit(0);
} 

static int rollback_and_finish ( trans, dbhandle )
    long **trans, **dbhandle;
{
    long status[20];

    /* ROLLBACK */
    isc_rollback_transaction ( status, trans);
    if (status[1])
    {
        printf ( "\nError rollback transaction!\n" );
        isc_print_status (status);
    }

    /* FINISH */
    isc_detach_database ( status, dbhandle);
    if (status[1])
    {
        printf ( "\nError detaching from database!\n" );
        isc_print_status (status);
    }
}

static build_whole_line (input_string, fp1, count)
    char *input_string; 
    FILE *fp1;
    int  count;
{
/**************************************
 *
 *        b u i l d _ w h o l e _ l i n e
 *
 **************************************
 *
 * Functional description
 * 
 *        handle LINES (newline terminated fields);
 *        arbitrary end_of_field is '\n' which we
 *        supply; blobs get stuck in input_fields list
 * 
 **************************************/
    short   i;
    char    *bufptr;

    bufptr = input_string;

    for (i = 0; i < count; i++)
        {
        if (input_fields[i].field_code == F_BLOB)
            get_blob (&input_fields[i], fp1);
        else if (input_fields[i].field_code == F_ARRAY)
            get_array (&input_fields[i], fp1 );
        else
            {
            bufptr += get_whole_line (bufptr, fp1, TRUE); 
            *bufptr++ = '\n';
            }
    
        if (GCONV_EOF) 
            break;
        } 
    *bufptr = 0;
}


void center_print (c_length, cstr)
    int c_length;
    char *cstr;
/*******************************************
 *
 *        c e n t e r _ p r i n t
 *
 *******************************************
 *
 * Functional description
 * 
 *        Print string 'cstr' at centered on a line 'c_length' long.
 * 
 *******************************************/
{
    int len;

    len = strlen (cstr - c_length) / 2;
    printf ("%*s%s\n", len, " ", cstr);
}

int check_field (input_field, string, separator, quote, rec_num, last)
    struct input_fields_t *input_field;
    char **string, separator, quote;
    int rec_num, last;
{
/**************************************
 *
 *        c h e c k _ f i e l d
 *
 **************************************
 *
 * Functional description
 *
 *        Get next input field, update max length for field if appropriate,
 *        check on char[] and varying fields for exceeeding database field length.
 *
 **************************************/
    int len;
    char end, *s, *start;

    if (input_field->field_code == F_BLOB || input_field->field_code == F_ARRAY )
        return;

    start = s = *string;
    if (quote || separator)
        {
        if (quote)
            {
            if (quote && (*s != quote))
                {
                printf ("ERROR: Quote (%c) expected found %c, record #%d, field %s.\n",
                        quote, *s, rec_num, input_field->field_name);
                s++;
                return;
                }
            s++;
            }

        end = quote ? quote : separator;
        len = 0;
        while (*s && (*s != end))
            {
            s++;
            len++;
            }

    /* handle quote, quote + separator, separator */
        if (quote)
            {
            if (*s != quote)
                {
                printf ("ERROR: Quote (%c) expected found end of line, record #%d, field %s\n", 
                        quote, rec_num, input_field->field_name);
                return;
                }
            else
                *s++ = 0;
            }
        if (separator)
            if (last || *s == separator)
                *s++ = 0;
            else
                {
                printf ("ERROR: Separator (%c) expected, record #%d, field %s\n", 
                        separator,  rec_num, input_field->field_name);
                return;
               }
        }
    else
        {
        len = input_field->user_length;
        s += len;
        }

    *string = s;
    
    s = start + len - 1;
    while ((s >= start) && (*s == ' '))
        {
        s--;
        len--;
        }
    
    if (!len)
        {
        printf ("Record #%d, field %s is MISSING\n", 
                rec_num, input_field->field_name);
        return;
        }

    if (len > input_field->max_length)
        input_field->max_length = len;

    if ((input_field->field_length < len ) &&
             (input_field->field_code == F_CHAR || input_field->field_code == F_VARY))
        too_big (rec_num, input_field->field_name, input_field->field_length,
                    len, start, rec_num);
}


void check_garbage (target_string)
    char *target_string;
/**************************************
 *
 *        c h e c k _ g a r b a g e
 *
 **************************************
 *
 * Functional description
 *
 *    Check that a string contains alpha characters and/or
 *    '_' (unserscore). Place a null in first position that
 *    is not one of these.
 *
 **************************************/
{
    char *s, *end;

    for (s = target_string; *s; s++)
        if (!isalnum(*s) && *s != '_')
            {
            *s = 0;
            break;
            }
}

int validate_max_size (input_fields)
    struct input_fields_t *input_fields;
/**************************************
 *
 *        c h e c k _ m a x _ s i z e
 *
 **************************************
 *
 * Functional description
 *
 *        Set max length for field in input file,
 *        check on char[] and varying fields for exceeeding database field length.
 *
 **************************************/
{
    FILE *fp1;
    int current_length;
    int i, j;
    char input_string [MAX], *s;
    int recs_processed = 0;
    
    num_of_fields = conversion_info (input_fields);
    get_fields (input_fields, target_relation, num_of_fields);
        
    if ((fp1 = fopen (input_file, "r")) == NULL)
        {
        printf ("Error opening \"%s\" for reading.\n", input_file);
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }
    
    while (!GCONV_EOF)
        {
        switch (conversion_type)
            {
            case FIXED:       
            case SINGLE:     
            case QUOTES:    
            case QCOMMA:   
                get_whole_line (input_string, fp1, TRUE);
                break;
    
            case LINES:   
                build_whole_line (input_string, fp1, num_of_fields);
                break;
            }

        if (GCONV_EOF) 
            break;

        /* if we allow comments then just continue to the next line     */
        /* if we find the first 2 chars in the string equal the C style */
        if ( COMMENTS && !strncmp (input_string, "/*", 2 ) )
            continue;

        recs_processed++;
        for (i = 0, s = input_string; i < num_of_fields; i++)
            check_field (&input_fields[i], &s, separator, quote, 
                             recs_processed, (i == (num_of_fields - 1)));
        if ((recs_processed % PRINT_LEVEL) == 0)
            printf ("Processed %d records.\n", recs_processed);
        }

    printf ("\n\n");
    printf ("Processed %d record(s).\n\n",recs_processed);
    for (i = 0; i < num_of_fields; i++)
        if ((input_fields[i].field_code == F_CHAR || input_fields[i].field_code == F_VARY))
            printf ("Field # %d: %-30s = %d.\n",
                     i, input_fields[i].field_name, input_fields[i].max_length);
        else
            printf ("Field # %d: ** %s is not string datatype **.\n",
                    i, input_fields[i].field_name);
    
    printf ("\n\n");
    printf ("FINISHED");
    printf ("\n\n");
}   


int conversion_info (fields)
    struct input_fields_t *fields;
/**************************************
 *
 *        c o n v e r s i o n _ i n f o
 *
 **************************************
 *
 * Functional description
 *
 *        Read template file for control information for processing
 *        of designated data file.
 *
 **************************************/
{
    FILE *fp;
    char temp_string[MAX], temp_string2[MAX], *s, *s1;
    int i, j, k;
    struct input_fields_t y;
    
    if ((fp = fopen(template_name,"r")) == NULL)
        {
        printf ("Error opening \"%s\" for reading.\n", template_name);
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }

    strcpy (BLOB_END, "[EOB]");
    strcpy (ARRAY_END, "[EOA]");

    /* get relation name */
    if (get_valid_str (temp_string, fp, LINE_MAX))
        {
        printf ("Premature end of file\n");
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }

    trim (temp_string);
    strcpy (target_relation, temp_string);
    convert_upper (target_relation);

    if (DEBUG_PRINT)
        printf ("conversion_info: relation name = %s\n", target_relation);

    /* get type of conversion and if needed the separator character */
    conversion_type = NULL;
    if (get_valid_str (temp_string, fp, LINE_MAX))
        {
        printf ("Premature end of file\n");
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }

    conversion_type = temp_string[0];
    if (conversion_type == SINGLE)
        {
        /* SINGLE */
        s = temp_string;
        SKIP_TO (s, '\047');
        s++;
        separator = *s;
        }           
    else if (conversion_type == LINES)
        separator = '\n';
    else if ((conversion_type == QUOTES) || (conversion_type == QCOMMA))
        {
        quote = '"';
        if (conversion_type == QCOMMA)
            separator = ',';
        }
    
    if (DEBUG_PRINT)
        printf ("conversion_info: conversion type = %c, separator = %c.\n",
             conversion_type, separator);

    /* get database fields now, if FIXED then get fixed length */
    j = 0;
    while (!feof (fp))
        {
        y.field_name[0] = NULL;
        y.field_code = 0;
        y.array_field_type = 0;
        y.field_length = 0;
        y.user_length = 0;
        y.max_length = 0;
        y.ind_field = 0;
        y.len_field = 0;
        y.blob = null_blob_id;
        y.array = null_blob_id;
        y.array_num = 0;
        y.array_start = -99;
        y.sqlda_match = 0;
    
        if (get_valid_str (temp_string, fp, LINE_MAX))
            break;
    
        /* If blobs or arrays are going to be input and something other than */
        /* the default "[EOF]" is wanted, read it after the    */
        /* conversion type info: example BLOB,"***"        */
        /* example ARRAY,"***"        */
    
        if (!strncmp (temp_string, "BLOB", 4))
            {
            s = temp_string;
            SKIP_TO (s, '"');
            s++;
            s1 = s;
            SKIP_TO (s1, '"');
            *s1 = NULL;
            strcpy (BLOB_END, s);
            if (DEBUG_PRINT)
                printf ("BLOB_END = \"%s\".\n", BLOB_END);
            }
        if (!strncmp (temp_string, "ARRAY", 4))
            {
            s = temp_string;
            SKIP_TO (s, '"');
            s++;
            s1 = s;
            SKIP_TO (s1, '"');
            *s1 = NULL;
            strcpy (ARRAY_END, s);
            if (DEBUG_PRINT)
                printf ("ARRAY_END = \"%s\".\n", ARRAY_END);
            }
        else
            {
            s = temp_string;
            SKIP_TO (s, ',');
            if ( *s ) {
                *s++ = NULL;
                if (conversion_type == FIXED)
                    {
                    /* get length for FIXED field */
                    y.user_length = atoi (s);
                    }
                else
                    {
                    /* get starting index for array field */
                    y.array_start = atoi (s);
                    }
                }
            strcpy (y.field_name, temp_string);
            check_garbage (y.field_name);
            convert_upper (y.field_name);
            fields[j++] = y;
            }
        if (DEBUG_PRINT)
            printf ("conversion_info: processed field : = %s\n", y.field_name);
        }
    return j;
}

int convert_upper(target_string)
    char *target_string;
/**************************************
 *
 *        c o n v e r t _ u p p e r
 *
 **************************************
 *
 * Functional description
 *
 *        Convert a string to upper case.
 *
 **************************************/
{
    char *s;

    s = target_string;

    while (*s)
        {
        *s = UPPER (*s);
        s++;
        }
}

void get_blob (input_field, fp2)
    struct input_fields_t *input_field;
    FILE *fp2;
/**************************************
 *
 *        g e t _ b l o b
 *
 **************************************
 *
 * Functional description
 *
 *        Read data into a blob.
 *
 **************************************/
{
    int i, j;
    long blob_handle;
    short blob_length;
    char temp_string [MAX];

    if (VALIDATE)
        {
        skip_blob(fp2);
        return;
        }
    i = 0;
    blob_handle = NULL;
    input_field->blob = null_blob_id;      

    if (DEBUG_PRINT)
        printf("get_blob:\tCreating BLOB.\n");
    
    if (isc_create_blob2 (isc_status_vec, GDS_REF (CONVERT), GDS_REF (isc_def_trans),
                          GDS_REF (blob_handle), GDS_REF(input_field->blob), 0, 0))
        isc_print_status (isc_status_vec);
    
    if (DEBUG_PRINT)
        printf ("get_blob:\tCreated BLOB.\n");
    
    get_whole_line (temp_string, fp2, FALSE);
    
    if (DEBUG_PRINT)
        printf ("get_blob:\t%s\n", temp_string);
    
    /* check to see if BLOB is missing! */
    if ( !strncmp (temp_string, BLOB_END, strlen(BLOB_END) ) ) {
        input_field->ind_field = MISSING;
        input_field->len_field = 0;
        input_field->blob = null_blob_id;      
        isc_cancel_blob ( isc_status_vec, GDS_REF (blob_handle ));
        if (DEBUG_PRINT)
            printf ("get_blob:\tBlob field is MISSING!\n");
        return;
        }

    while (!GCONV_EOF && strncmp (temp_string, BLOB_END, strlen(BLOB_END)))
        {
        blob_length = strlen (temp_string);
        if (isc_put_segment (isc_status_vec, GDS_REF (blob_handle),
                              blob_length, temp_string))
            isc_print_status (isc_status_vec);
        i++;
        get_whole_line (temp_string, fp2, FALSE);

        if (DEBUG_PRINT)
            printf ("get_blob:\t%s\n", temp_string);
        }
    
    if (isc_close_blob (isc_status_vec, GDS_REF (blob_handle)))
        isc_print_status (isc_status_vec);
}

void get_array (input_field, fp2)
    struct input_fields_t *input_field;
    FILE *fp2;
/**************************************
 *
 *        g e t _ a r r a y
 *
 **************************************
 *
 * Functional description
 *
 *        Read data into a array.
 *
 **************************************/
{
    int j;
    long i;
    long element_start_loc;
    long actual_array_len;
    ARRAY_DESC tmp_desc;
    char temp_string [MAX];
    char temp_string2 [MAX];
    short tmp_short;
    long tmp_long;
    ISC_QUADW tmp_quad;
    float tmp_float;
    double tmp_double;
    struct tm d;
    short array_elements[16];
    short element_length;
    int len;
    int array_start;
    char *p;
    char *s, *s1, *s2;

    if (VALIDATE)
        {
        skip_array(fp2);
        return;
        }

    /* find out if the data file has a different starting index than the db */
    array_start = input_field->array_start != -99 ? 
        input_field->array_start : mydesc[input_field->array_num].array_desc_bounds[0].array_bound_lower;

    if (DEBUG_PRINT)
        {
        printf("\n\nget_array:\tArray starting index = %d\n\n", array_start );
        }

    if (DEBUG_PRINT)
        printf("get_array:\tInitializing ARRAY.\n");

    for ( i = (long) array_buffers[input_field->array_num], s = (char *) array_buffers[input_field->array_num];
        i < (*array_buffers[input_field->array_num] + array_fields[input_field->array_num].tot_array_size); i++ )
            *s++ = 0;

    if (DEBUG_PRINT)
        printf ("get_array:\tInitialized ARRAY.\n\n");
    
    get_whole_line (temp_string, fp2, TRUE);

    if (DEBUG_PRINT)
        printf ("\nget_array:\tstart = %s\n", temp_string);
     
    /* check to see if ARRAY is missing! */
    if ( !strncmp (temp_string, ARRAY_END, strlen(ARRAY_END) ) ) {
        input_field->array = null_blob_id;
        input_field->ind_field = MISSING;
        input_field->len_field = 0;
        if (DEBUG_PRINT)
            printf ("\nget_array:\tarray field is MISSING!\n");
        return;
        }

    while (!GCONV_EOF && strncmp (temp_string, ARRAY_END, strlen(ARRAY_END)))
        {
        s = s1 = temp_string;
        len = 0;
        tmp_short = 0;
        tmp_long = 0;
        tmp_quad = null_blob_id;
        tmp_float = 0.0;
        tmp_double = 0.0;

        /* go through and get the array element's location, ie 4,3,5,1 */
        /* this would correspond in C to [4][3][5][1]                  */

        for ( j = 0; j < mydesc[input_field->array_num].array_desc_dimensions; j++ ) {
            SKIP_TO_LEN (s, ',', len);
            *s++ = NULL;
            array_elements[j] = atoi ( s1 );
            /* now adjust the element index to match the db */
            if ( array_start > mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower )
                array_elements[j] -= array_start -
                    mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower;
            if ( array_start < mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower )
                array_elements[j] += mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower -
                    array_start;

            s1 = &temp_string[++len];
            if (DEBUG_PRINT) {
                printf ( "get_array:\tj = %d : array_elements[j] = %d\n", j, array_elements[j] );
                printf ( "get_array:\tlen = %d\n", len ) ;
                printf ( "get_array:\tremaining temp_string = %s\n", s1 );
                }
            if ( array_elements[j] > mydesc[input_field->array_num].array_desc_bounds[j].array_bound_upper ) {
                printf ( "\nAbort: array subscript %d, value %d, exceeds array's upper bound of %d!\n\n",
                    j, array_elements[j],
                    mydesc[input_field->array_num].array_desc_bounds[j].array_bound_upper );
                printf ( "Array element line is: %s.\n\n", temp_string );
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit ( 1 ) ;
                }
            if ( array_elements[j] < mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower ) {
                printf ( "\nAbort: array subscript %d, value %d, exceeds array's lower bound of %d!\n\n",
                    j, array_elements[j],
                    mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower );
                printf ( "Array element line is: %s.\n\n", temp_string );
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit ( 1 ) ;
                }
            /* Keep track of the highest array element used so put_slice is efficient */
            if ( array_elements[j] > array_fields[input_field->array_num].array_items[j].end_element )
                array_fields[input_field->array_num].array_items[j].end_element = array_elements[j];
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tarray_elements[%d] = %d\n", j, array_elements[j] );
                    printf ( "get_array:\tarray_fields[%d].array_items[%d].end_element = %d\n", 
                        input_field->array_num, j, array_fields[input_field->array_num].array_items[j].end_element );
                    }
            }

        /* now go through and convert the rest of the info to the native  */
        /* format, ie if the array is an array of longs, convert the rest */
        /* of temp_string into the temporary long field called tmp_long   */
             
        switch ( input_field->array_field_type ) {
            case F_CHAR  :
            case F_VARY  :
                p = s1;
                /* don't add 1 to strlen - all fixed
                element_length = strlen ( s1 ) + 1;
                */
                element_length = strlen ( s1 );
                if (DEBUG_PRINT) 
                    printf ( "get_array:\ttext: %s\n", s1 ); 
                break;
            case F_SHORT :
                tmp_short = atoi ( s1 );
                element_length = sizeof(tmp_short);
                p = (char *) &tmp_short;
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tshort_txt: %s\n", s1 ); 
                    printf ( "get_array:\tshort: %d\n", tmp_short ); 
                    }
                break;
            case F_LONG  :
                tmp_long = atol ( s1 );
                element_length = sizeof(tmp_long);
                p = (char *) &tmp_long;
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tlong_txt: %s\n", s1 ); 
                    printf ( "get_array:\tlong: %d\n", tmp_long ); 
                    }
                break;
            case F_DOUBLE:
                sscanf ( s1, "%f", &tmp_float );
                tmp_double = (double) tmp_float;
                element_length = sizeof(tmp_double);
                p = (char *) &tmp_double;
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tdouble_txt: %s\n", s2 );
                    printf ( "get_array:\tdouble: %10.2f\n", tmp_double );
                    }
                break;
            case F_FLOAT :
                sscanf ( s1, "%f", &tmp_float );
                element_length = sizeof(tmp_float);
                p = (char *) &tmp_float;
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tfloat_txt: %s\n", s1 ); 
                    printf ( "get_array:\tfloat: %10.2f\n", tmp_float ); 
                    }
                break;
            case F_DATE  :
                parse_date ( &tmp_quad, s1 );
                element_length = sizeof(tmp_quad);
                p = (char *) &tmp_quad;
                if (DEBUG_PRINT) {
                    printf ( "get_array:\tdate_txt: %s\n", s1 ); 
                    printf ( "get_array:\tdate: %d : %d\n", 
                        tmp_quad.gds_quad_high, tmp_quad.gds_quad_low ); 
                    }
                break;
            default:
                printf ( "\nUnknown field type! Type = %d.\n\n", input_field->array_field_type );
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
            }
                         
        element_start_loc = 0;
        for ( j = 0; j < mydesc[input_field->array_num].array_desc_dimensions; j++ ) {
            /* remember we're dealing with C, so convert back to a zero based array */
            /* for computing the index into the malloced memory */
            element_start_loc += 
                ( array_elements[j] - mydesc[input_field->array_num].array_desc_bounds[j].array_bound_lower) *
                array_fields[input_field->array_num].array_items[j].index;
            if (DEBUG_PRINT) {
                printf ("\nget_array:\telement = %d, index = %d\n", array_elements[j], 
                    array_fields[input_field->array_num].array_items[j].index );
                printf ("get_array:\trunning total = %d.\n", element_start_loc );
                }
            }
                
        if (DEBUG_PRINT) {
            printf ("\nget_array:\tmoving data to array\n");
            printf ("get_array:\tstarting address of location = %d\n", element_start_loc);
            printf ("get_array:\tnumber of bytes to move = %d\n\n", element_length);
        }

        move ( p, ((array_buffers[input_field->array_num]) + element_start_loc), element_length );

        get_whole_line (temp_string, fp2, TRUE);

        if (DEBUG_PRINT) 
            printf ("\nget_array:\tstart = %s\n", temp_string);

        } /* end of WHILE loop */

    if (DEBUG_PRINT)
        printf ( "\nget_array:\tstoring array\n" );

    tmp_desc = mydesc[input_field->array_num];
    for ( j = 0, actual_array_len = 0; j < mydesc[input_field->array_num].array_desc_dimensions; j++ ) {
        /* if we actually have reached the last dimension add 1 to get the all the data */
        if ( (j + 1) == mydesc[input_field->array_num].array_desc_dimensions ) 
            {
            actual_array_len += array_fields[input_field->array_num].array_items[j].index *
                (array_fields[input_field->array_num].array_items[j].end_element -
                tmp_desc.array_desc_bounds[j].array_bound_lower + 1);
            }
        else 
            {
            actual_array_len += array_fields[input_field->array_num].array_items[j].index *
                (array_fields[input_field->array_num].array_items[j].end_element -
                tmp_desc.array_desc_bounds[j].array_bound_lower);
            }

        /* set the upper bounds only to what is necessary */
        tmp_desc.array_desc_bounds[j].array_bound_upper = 
            array_fields[input_field->array_num].array_items[j].end_element;

        /* set the field length in tmp_desc to be the actual length in the array */
        tmp_desc.array_desc_length = 
            array_fields[input_field->array_num].field_size;

        if (DEBUG_PRINT) {
            printf ("\nget_array:\tend element = %d, index = %d\n", array_elements[j], 
                array_fields[input_field->array_num].array_items[j].index );
            printf ("get_array:\trunning total = %d.\n", actual_array_len );
            }
        }

    input_field->array = null_blob_id;

    if (DEBUG_PRINT) {
        printf ("\nget_array:\tTMP_DESC values\n\n" );
        printf ("\nget_array:\tarray_desc_dtype = %d.\n", tmp_desc.array_desc_dtype );
        printf ("get_array:\tarray_desc_length = %d.\n", tmp_desc.array_desc_length );
        printf ("get_array:\tarray_desc_field_name = %s.\n", tmp_desc.array_desc_field_name );
        printf ("get_array:\tarray_desc_relation_name = %s.\n", tmp_desc.array_desc_relation_name );
        printf ("get_array:\tarray_desc_dimensions = %d.\n", tmp_desc.array_desc_dimensions );
        printf ("get_array:\tarray_desc_flags = %d.\n", tmp_desc.array_desc_flags );
        for ( j = 0; j < mydesc[input_field->array_num].array_desc_dimensions; j++ ) {
            printf ("\n\tget_array:\tarray_desc_bounds[%d].array_bound_lower = %d.\n", j, tmp_desc.array_desc_bounds[j].array_bound_lower );
            printf ("\tget_array:\tarray_desc_bounds[%d].array_bound_upper = %d.\n", j, tmp_desc.array_desc_bounds[j].array_bound_upper );
            }
        printf ("\nget_array:\tactual_array_len = %d.\n", actual_array_len );
        }

    iscx_array_put_slice ( isc_status_vec, &CONVERT, &isc_def_trans, &input_field->array, &tmp_desc,
        array_buffers[input_field->array_num], &actual_array_len );

    if (isc_status_vec[1])
        {
        printf ( "\nError storing array! Exiting!\n\n" );
        isc_print_status(isc_status_vec);
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        printf ( "\n\n" );
        exit(1);
        }
}

int get_fields (fields, relation_name, count)
    struct input_fields_t *fields;
    char *relation_name;
    int count;
/**************************************
 *
 *        g e t _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *
 *        Get information about relation's fields from database's
 *        metadata.
 *
 **************************************/
{
    int i, j, k;
    long result, total_result;
    short found;
    ISC_QUADW computed_blr;
    short field_type;
    short dimensions;
    short field_length;
    short cblr_ind, ftype_ind, dim_ind, flength_ind;

    char *SEARCH_STRING = {"SELECT R.RDB$COMPUTED_BLR, R.RDB$DIMENSIONS, R.RDB$FIELD_TYPE, R.RDB$FIELD_LENGTH FROM \
                                RDB$RELATION_FIELDS RF, RDB$FIELDS R WHERE \
                                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND \
                                RF.RDB$RELATION_NAME = ? AND \
                                RF.RDB$FIELD_NAME = ?"};

/*
    char *SEARCH_STRING = {"SELECT R.RDB\$COMPUTED_BLR, R.RDB\$DIMENSIONS, R.RDB\$FIELD_TYPE, R.RDB\$FIELD_LENGTH FROM \
                                RDB\$RELATION_FIELDS RF, RDB\$FIELDS R WHERE \
                                RF.RDB\$FIELD_SOURCE = R.RDB\$FIELD_NAME AND \
                                RF.RDB\$RELATION_NAME = ? AND \
                                RF.RDB\$FIELD_NAME = ?"};
*/

    /* Declare input and output SQLDA structures */
    SQLDA *sqlda_in, *sqlda_out;
                                                   
    /* Size input sqlda for 2 input parameters */
    sqlda_in = (SQLDA*) malloc (SQLDA_LENGTH (2));
    sqlda_in->sqln = 2;

    /* Size output sqlda for 4 fields */
    sqlda_out = (SQLDA*) malloc (SQLDA_LENGTH (4));
    sqlda_out->sqln = 4;

    /* EXEC SQL PREPARE Q2 INTO sqlda_out FROM :SEARCH_STRING */
    isc_embed_dsql_prepare (isc_status_vec, &CONVERT, &isc_def_trans, "Q2 ", (unsigned short) 0, SEARCH_STRING, 0, sqlda_out);
    SQLCODE = isc_sqlcode (isc_status_vec);

    if ( SQLCODE ) {
        printf ( "\nUnable to PREPARE the SEARCH string! SQLCODE = %d.\n", SQLCODE );
        isc_print_status ( isc_status_vec );
        rollback_and_finish ( &isc_def_trans, &CONVERT );
        exit (1);
        }
    
    sqlda_in->sqld = sqlda_in->sqln;

    /* Fill in datatype for Input Parameters */
    sqlda_in->sqlvar[0].sqltype = SQL_TEXT;
    sqlda_in->sqlvar[1].sqltype = SQL_TEXT;

    /* Fill in locations of indicator fields and data fields for output */
 
    sqlda_out->sqlvar[0].sqldata = (char*) &computed_blr;
    sqlda_out->sqlvar[0].sqlind  = &cblr_ind;

    sqlda_out->sqlvar[1].sqldata = (char*) &dimensions;
    sqlda_out->sqlvar[1].sqlind  = &dim_ind;

    sqlda_out->sqlvar[2].sqldata = (char*) &field_type;
    sqlda_out->sqlvar[2].sqlind  = &ftype_ind;

    sqlda_out->sqlvar[3].sqldata = (char*) &field_length;
    sqlda_out->sqlvar[3].sqlind  = &flength_ind;

    MAX_ARRAY_NUM = 0;

    for (i = 0; i < count; i++)
        {

        /* Fill in data for Input Parameters */
        sqlda_in->sqlvar[0].sqllen = strlen(relation_name);
        sqlda_in->sqlvar[1].sqllen = strlen(fields[i].field_name);

        sqlda_in->sqlvar[0].sqldata = relation_name;
        sqlda_in->sqlvar[1].sqldata = fields[i].field_name;
                      
        /* EXEC SQL DECLARE C CURSOR FOR Q2 */
        isc_embed_dsql_declare (isc_status_vec, "Q2 ", "C ");
        SQLCODE = isc_sqlcode (isc_status_vec);

        if ( SQLCODE ) {
            printf ( "\nUnable to DECLARE the CURSOR! SQLCODE = %d.\n", SQLCODE );
            isc_print_status ( isc_status_vec );
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
        }
    
        /* EXEC SQL OPEN C USING DESCRIPTOR sqlda_in */
        isc_embed_dsql_open (isc_status_vec, &isc_def_trans, "C ", 0, sqlda_in);
        SQLCODE = isc_sqlcode (isc_status_vec);

        if ( SQLCODE ) {
            printf ( "\nUnable to OPEN the CURSOR! SQLCODE = %d.\n", SQLCODE );
            isc_print_status ( isc_status_vec );
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
        }
    
        /* EXEC SQL FETCH C USING DESCRIPTOR sqlda_out */
        SQLCODE = isc_embed_dsql_fetch (isc_status_vec, "C ", 0, sqlda_out);
        if (SQLCODE != 100) SQLCODE = isc_sqlcode (isc_status_vec);

        if (SQLCODE == 100)
            {
            printf ("ERROR: field %s not found in relation %s!\n",
                    fields[i].field_name, relation_name);
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
            }

        if ( SQLCODE ) {
            printf ( "\nUnable to FETCH using the CURSOR! SQLCODE = %d.\n", SQLCODE );
            isc_print_status ( isc_status_vec );
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
        }
    
        if (DEBUG_PRINT)
            {
            printf ("\nGET_FIELDS: SQLDA data:\n");
            for (k = 0, j = 0; k < 4; k++) {
                    printf ( "\t%2d:\tName = %-31s.\tLen  = %d.\t\tInd  = %d.\n", k,
                        sqlda_out->sqlvar[j].sqlname,
                        sqlda_out->sqlvar[j].sqllen,
                        *sqlda_out->sqlvar[j].sqlind );
                    j++;
                }
            printf ("\n\n");
            }

        if (cblr_ind >= 0)
            {
            printf ("ERROR: %s is a computed field!\n", fields[i].field_name);
            rollback_and_finish ( &isc_def_trans, &CONVERT );
            exit (1);
            }
      
        if ( dim_ind >= 0 ) {
            ARRAYS = 1;
            fields[i].field_code = F_ARRAY;
            fields[i].array_field_type = field_type;
            fields[i].array_num = MAX_ARRAY_NUM;
            array_fields[MAX_ARRAY_NUM].valid = 1;
            array_fields[MAX_ARRAY_NUM].field_num = i;
            array_fields[MAX_ARRAY_NUM].field_size = field_length;

            if (DEBUG_PRINT) {
                printf ( "\nget_fields: MAX_ARRAY_NUM = %d\n", MAX_ARRAY_NUM );
                printf ( "\nget_fields: array_fields[%d].field_size = %d\n",MAX_ARRAY_NUM,
                    array_fields[MAX_ARRAY_NUM].field_size );
                }

            isc_array_lookup_bounds (isc_status_vec, &CONVERT, &isc_def_trans, target_relation, 
                fields[i].field_name, &mydesc[MAX_ARRAY_NUM]);

            /* force all fields to be fixed - let the engine figure it out */
            if ( field_type == F_VARY ) {
                fields[i].array_field_type = F_CHAR;
                mydesc[MAX_ARRAY_NUM].array_desc_dtype = F_CHAR;
                }

            for ( j = 0; j < mydesc[MAX_ARRAY_NUM].array_desc_dimensions; j++ ) {

                array_fields[MAX_ARRAY_NUM].array_items[j].max_db_elements = 
                    mydesc[MAX_ARRAY_NUM].array_desc_bounds[j].array_bound_upper -
                    mydesc[MAX_ARRAY_NUM].array_desc_bounds[j].array_bound_lower + 1;

                if (DEBUG_PRINT) {
                    printf ( "\nget_fields: array_fields[%d].array_items[%d].max_db_elements = %d\n\n",
                        MAX_ARRAY_NUM, j,
                        array_fields[MAX_ARRAY_NUM].array_items[j].max_db_elements );
                    }

                /* Multiply all subordinate dimensions together */
                result = 1;
                for ( k = j + 1; k < mydesc[MAX_ARRAY_NUM].array_desc_dimensions; k++ ) {
                    result *= ( mydesc[MAX_ARRAY_NUM].array_desc_bounds[k].array_bound_upper -
                        mydesc[MAX_ARRAY_NUM].array_desc_bounds[k].array_bound_lower + 1 );
                    if (DEBUG_PRINT)
                        printf ( "get_fields:\t\tresult = %d\n", result );
                    }

                /* Now multiply that by the field size, giving the # of bytes */
                /* to move for every element of this dimension                */

                array_fields[MAX_ARRAY_NUM].array_items[j].index = result *
                    array_fields[MAX_ARRAY_NUM].field_size;
                total_result += array_fields[MAX_ARRAY_NUM].array_items[j].index;
                if (DEBUG_PRINT) {
                    printf ( "\nget_fields: array_fields[%d].array_items[%d].index = %d\n", MAX_ARRAY_NUM, j,
                        array_fields[MAX_ARRAY_NUM].array_items[j].index );
                    }
                }
            array_fields[MAX_ARRAY_NUM].tot_array_size = 
                (array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements) *
                array_fields[MAX_ARRAY_NUM].array_items[0].index;

            /* now allocated and initialize mem buffer for largest size of this array */
            if (DEBUG_PRINT) {
                printf ( "\nget_fields: array_fields[%d].array_items[0].max_db_elements = %d\n", MAX_ARRAY_NUM,
                    array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements );
                printf ( "\nget_fields: array_fields[%d].array_items[0].index = %d\n", MAX_ARRAY_NUM,
                    array_fields[MAX_ARRAY_NUM].array_items[0].index );
                printf ( "\nget_fields: array_fields[%d].tot_array_size = %d\n", MAX_ARRAY_NUM,
                    array_fields[MAX_ARRAY_NUM].tot_array_size );
                printf ( "\nget_fields: array_fields[%d].array_items[0].max_db_elements ) *\n\t\
                    array_fields[%d].array_items[0].index /\n\t\
                    array_fields[%d].field_size = %d\n", MAX_ARRAY_NUM, MAX_ARRAY_NUM, MAX_ARRAY_NUM,
                    ( array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements ) *
                    array_fields[MAX_ARRAY_NUM].array_items[0].index /
                    array_fields[MAX_ARRAY_NUM].field_size );
                printf ( "\nget_fields: number of memory units = %d\n",
                    ( array_fields[MAX_ARRAY_NUM].tot_array_size / array_fields[MAX_ARRAY_NUM].field_size ) );
                }
            if ( ( array_buffers[MAX_ARRAY_NUM] = (char *) malloc ( array_fields[MAX_ARRAY_NUM].tot_array_size ) ) == NULL )
                {
                printf ( "\nMemory already exhausted before array number %d!\n\n", MAX_ARRAY_NUM );
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
            else if (DEBUG_PRINT) {
                    printf ( "\nget_fields: starting address of buffer [%d]: %ld\n",
                            MAX_ARRAY_NUM,
                            array_buffers[MAX_ARRAY_NUM] );
                    }

            MAX_ARRAY_NUM++;
            }
        else {
            fields[i].field_code = field_type;
    
            switch ( fields[i].field_code ) {
                case F_DATE:
                        fields[i].field_length = 25;
                        break;
                      case F_BLOB:
                        BLOBS = 1;
                    default:
                        fields[i].field_length = field_length;
                    break;
                }
            }

        if (DEBUG_PRINT) {
            printf ( "\nget_fields:%2d\t\tfield_name          = %s.\n", i, fields[i].field_name );
            printf ( "get_fields:%2d\t\tfield_code..........= %d.\n", i, fields[i].field_code );
            printf ( "get_fields:%2d\t\tarray_field_type    = %d.\n", i, fields[i].array_field_type );
            printf ( "get_fields:%2d\t\tfield_length........= %d.\n", i, fields[i].field_length );
            printf ( "get_fields:%2d\t\tuser_length         = %d.\n", i, fields[i].user_length );
            printf ( "get_fields:%2d\t\tmax_length..........= %d.\n", i, fields[i].max_length );
            printf ( "get_fields:%2d\t\tind_field           = %d.\n", i, fields[i].ind_field );
            printf ( "get_fields:%2d\t\tlen_field...........= %d.\n", i, fields[i].len_field );
            printf ( "get_fields:%2d\t\tarray_num           = %d.\n", i, fields[i].array_num );
            printf ( "get_fields:%2d\t\tarray_start.........= %d.\n", i, fields[i].array_start );
            printf ( "get_fields:%2d\t\tsqlda_match         = %d.\n", i, fields[i].sqlda_match );
            printf ( "get_fields:%2d\t\tblob.gds_quad_high..= %d.\n", i, fields[i].blob.gds_quad_high );
            printf ( "get_fields:%2d\t\tblob.gds_quad_low   = %d.\n", i, fields[i].blob.gds_quad_low );
            printf ( "get_fields:%2d\t\tarray.gds_quad_high.= %d.\n", i, fields[i].array.gds_quad_high );
            printf ( "get_fields:%2d\t\tarray.gds_quad_low  = %d.\n\n", i, fields[i].array.gds_quad_low );
            }

        isc_close ( isc_status_vec, "C " );
        }

        if (DEBUG_PRINT) {
            printf ( "\nReleasing!!\n" );
            }

        isc_dsql_release ( isc_status_vec, "Q2 " );
        if ( isc_status_vec[1] ) {
                printf ( "\nError on release:\n" );
                isc_print_status ( isc_status_vec );
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
}

int get_input_field (sql_var, input_field, string, separator, quote, rec_num)
    SQLVAR *sql_var;
    struct input_fields_t *input_field;
    char **string, separator, quote;
    int rec_num;
/**************************************
 *
 *        g e t _ i n p u t _ f i e l d
 *
 **************************************
 *
 *        Note: if separator & quote both are NULL, 
 *        then we are processing fixed length fields;
 *        Blobs are a simple assignment;
 *
 **************************************/
{
    int len;
    char end, *s, *start;

    if (input_field->field_code == F_BLOB)
        {
        sql_var->sqldata = (char*) &input_field->blob;
        return;
        }

    if (input_field->field_code == F_ARRAY)
        {
        sql_var->sqldata = (char*) &input_field->array;
        return;
        }

    if (DEBUG_PRINT) {
            printf ( "\nget_input_field:\n\trec_num = %d : ", rec_num );
            printf ( "\tfield = %s : string = %s.\n", input_field->field_name, *string );
            }

    sql_var->sqldata = start = s = *string;
    if (quote || separator)
        {
        if (quote)
            {
            if (quote && (*s != quote))
                {
                printf ("ERROR: Quote (%c) expected found %s!\n", quote, *s);
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
            s++;
            }

        sql_var->sqldata = start = s;
        end = quote ? quote : separator;
        len = 0;
        while (*s && (*s != end))
            {
            s++;
            len++;
            }
    
        if (quote)
            {
            if (*s != quote)
                {
                printf ("ERROR: Quote (%c) expected found end of line!\n", quote);
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
            else
                s++;
            }
        if (separator && (*s == separator))
            s++;
        }
    else
        {
        len = input_field->user_length;
        s += len;
        }

    *string = s;

    s = start + len - 1;
    while ((s >= start) && (*s == ' '))
        {
        s--;
        len--;
        }
    if (!len)
        input_field->ind_field = MISSING;
    else {
        sql_var->sqllen = len;
        input_field->len_field = len;
        }

    if ((input_field->field_length < len ) &&
         (input_field->field_code == F_CHAR || input_field->field_code == F_VARY))
        too_big (rec_num, input_field->field_name, input_field->field_length, len, sql_var->sqldata, rec_num);
}

void get_ignore (list)
    char *list;
/*************************************
 *
 *        g e t _ i g n o r e
 *
 *************************************
 *
 * Functional description
 *
 *        Process a list of fields (designated by input position number)
 *        to be ignored (not loaded).
 *        'ignore_cols' is set up as an array of the position numbers
 *        (first position is 0) of the fields to be ignored. the end of
 *        the list is indicated by -1 for a position number.
 *
 **************************************/
{
    char *s;
    int i;

    s = list;
    while (*s)
        {
        i = atoi (s) - 1;
        if (i >= MAX_FIELDS)
            {
            fprintf (stdout, "\ngconv: Invalid ignore value!\n\n" );
            syntax ();
            exit (1);
            }
        ignore_cols[i] = 1;
        while (*s)
            if (*s++ == ',')
                break;
        }
}

int get_line_range ()
/************************************
 *
 *        g e t _ l i n e _ r a n g e
 *
 ************************************
 *
 * Functional description
 *
 *        Process a input line range specification.
 *
 **************************************/
{
    char tmp_line_range[MAX_SMALL];
    int i = 0;
    int j = 0;
    int k;

    k =  strlen (line_char);

    for (i = 0; i < k; i++)
        {
        if (line_char[i] == '-' )
            break;
        }

    line_char[i] = NULL;

    strcpy (tmp_line_range, line_char);
    
    /* don't have to decrement because get_whole_line checking takes care of it */
    start_line = atoi (tmp_line_range); 
                    
    strcpy (tmp_line_range, &line_char[i+1]); /* let end_line = null or 0 */
    
    /* don't have to decrement because get_whole_line checking takes care of it */
    end_line = atoi (tmp_line_range);
    
    if (DEBUG_PRINT)
        printf ("get_line_range: start = %d\tend = %d.\n",
                 start_line, end_line);
    
    return;
} 

static get_string (prompt_string, response_string, length)
    char        *prompt_string, *response_string;
    int                length;
/**************************************
 *
 *        g e t _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *
 *        Write a prompt and read a string.  If the string overflows,
 *        try again.  If we get an EOF, return FAILURE.
 *
 **************************************/
{
    char        *p;
    short        l, c;

    while (1)
        {
        printf ("%s: ", prompt_string);
        for (p = response_string, l = length; l; --l)
            {
                c = getchar();
                if (c == EOF)
                    return FALSE;
                if (c == '\n')
                    {
                    *p = 0;
                    return TRUE;
                    }
                *p++ = c;
                }
        while (getchar() != '\n')
                ;
        printf ("** Maximum field length is %d characters **\n", length - 1);
        }
}

static int get_whole_line (target_string, fp, check_for_comments)
    char *target_string;
    FILE *fp;
    int check_for_comments;
/**************************************
 *
 *        g e t _ w h o l e _ l i n e
 *
 ***************************************
 *
 * Functional description
 *
 *        Get a line of input from data file (do buffered reads).
 *
 **************************************/
{
    static long buff_index = 0;
    static long buff_end = 0;

    int ch;
    int i, j;
    char temp_string [MAX];

    for (;;)
        { /* process only lines between start_line and end_line */

        i = 0;

        if (buff_index == buff_end)
            {
            buff_index = 0;
            buff_end = fread (input_buffer, sizeof (char), MAX_BUFFER, fp);
            if (DEBUG_PRINT) 
                printf ("get_whole_line buffer: %ld\n", buff_end);
            if ( buff_end == 0)
                {
                GCONV_EOF = 1;
                break;
                }
             }

        for (;;)
            {
            ch = input_buffer[buff_index++];
            if (ch == '\n') 
                    break;
            temp_string[i++] = ch;
            if (buff_index == buff_end)
                {
                buff_index = 0;
                buff_end = fread (input_buffer, sizeof (char), MAX_BUFFER, fp);
                if (DEBUG_PRINT) 
                    printf ("get_whole_line buffer: %ld\n", buff_end);
                if (buff_end == 0)
                    {
                    GCONV_EOF = 1;
                    break;
                    }
                }
            }

        temp_string[i] = NULL;

        strcpy (target_string, temp_string);
                 
        lines_processed++;
                           
        if ( check_for_comments && !strncmp ( target_string, "/*", 2 ) )
            {
            target_string[0] = 0;
            }
        else
            {
            if (lines_processed >= start_line &&
                (end_line == 0 || lines_processed <= end_line))
                break;
            else if (lines_processed > end_line && end_line != 0)
                {
                /* return EOF to fp */ 
                GCONV_EOF = 1;
                if (DEBUG_PRINT) 
                    printf ("get_whole_line : GCONV_EOF = %d\n", GCONV_EOF);
                break;
                }
            } /* else - check_for_comments */
        } /* for ;; */

    if (DEBUG_PRINT) 
        printf ("get_whole_line: %ld : %s\n", lines_processed, target_string);

    return i;
}

int get_valid_str (target_string, fp, line_length)
    char *target_string;
    FILE *fp;
    int line_length;
/**************************************
 *
 *        g e t _ v a l i d _ s t r
 *
 **************************************
 *
 * Functional description
 *
 *        Get a line of input from template file.
 *
 **************************************/
{
    int l;

    fgets (target_string, line_length, fp);

    if (DEBUG_PRINT)
        printf ("\nget_valid_str: %s\n", target_string);

    while (((!strncmp (target_string, "/*", 2)) || 
          (target_string[0] == ' ') || 
          (target_string[0] == '\n')) && 
          (!feof (fp)))
        fgets (target_string, line_length, fp);

    if (feof (fp)) 
        return (1);

    l = strlen (target_string) - 1;
    if (target_string[l] == '\n') 
        target_string[l] = NULL;

    return (0);
}


int setup_dsql (x, relation_name, c)
    struct input_fields_t *x;
    char *relation_name;
    int c;
/**************************************
 *
 *        s e t u p _ d s q l
 *
 **************************************
 *
 * Functional description
 *
 *        Set up the DSQL command (INSERT ....)
 *
 **************************************/
{
    int i;
    int num_ignores = 0;
    int j, k, l, tot_len;
    char display_dummy[81];

    strcpy (DSQL_STRING, "INSERT INTO ");
    strcat (DSQL_STRING, relation_name);
    strcat (DSQL_STRING, " (");

    for (i = 0; i < c; i++)
        {
        /* check to see if any columns should be excluded */
        if (!ignore_cols [i])
            {
            if (DSQL_STRING [strlen (DSQL_STRING) - 1] == '(' )
                strcat (DSQL_STRING, x[i].field_name);
            else
                {
                strcat (DSQL_STRING, ", ");
                strcat (DSQL_STRING, x[i].field_name);
                }
            }
    
        }

    strcat (DSQL_STRING, ") VALUES (");

    for (i = 0; i < c; i++)
        {
        /* check to see if any columns should be excluded */
        if (!ignore_cols [i])
            {
            if (DSQL_STRING [strlen (DSQL_STRING) - 1] == '?' )
                strcat (DSQL_STRING, ", ?");
            else
                strcat (DSQL_STRING, "?");
            }
        }

    strcat (DSQL_STRING, ")");

    if (DEBUG_PRINT) {
        printf ( "\nDSQL string:\n" );
        printf ( "\nQuery length: %d.\n\n", strlen ( DSQL_STRING ) );
        l = 0;
        tot_len = 80;
        while ( strlen(&DSQL_STRING[l]) > 80 ) {
            /* go backward until a space */
            for ( k = tot_len; DSQL_STRING[l+k-1] != ' '; k-- );
            strncpy ( display_dummy, &DSQL_STRING[l], k );
            display_dummy[k] = '\0';
            printf ( "%*s%s\n", 80 - tot_len, "", display_dummy );
            if ( !l )
                tot_len = 76;
            l += k;
            }
        printf ( "%*s%s\n\n", 80 - tot_len, "", &DSQL_STRING[l] );
        }

    return num_ignores;
}

int skip_blob (fp)
    FILE  *fp;
{
/**************************************
 *
 *        s k i p _ b l o b
 *
 **************************************
 *
 * Functional description
 *
 *        Skip over blob data in data file.
 *
 **************************************/
    char temp_string [MAX];
    
    while (!GCONV_EOF)
        {
        get_whole_line (temp_string, fp);
        if (!strncmp (temp_string, BLOB_END, strlen(BLOB_END)))
            return;
        else
            if (GCONV_EOF)
                {
                printf ("Unexpected EOF while reading blob. Exiting");
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
        }
    return;
}

int skip_array (fp)
    FILE  *fp;
{
/**************************************
 *
 *        s k i p _ a r r a y
 *
 **************************************
 *
 * Functional description
 *
 *        Skip over array data in data file.
 *
 **************************************/
    char temp_string [MAX];
    
    while (!GCONV_EOF)
        {
        get_whole_line (temp_string, fp);
        if (!strncmp (temp_string, ARRAY_END, strlen(ARRAY_END)))
            return;
        else
            if (GCONV_EOF)
                {
                printf ("Unexpected EOF while reading array. Exiting");
                rollback_and_finish ( &isc_def_trans, &CONVERT );
                exit (1);
                }
        }
    return;
}

int parse_args (argc, argv)
    int argc;
    char **argv;
/*************************************
 *
 *        p a r s e _ a r g s
 *
 *************************************
 *
 * Functional description
 *
 *        Process the command line arguments.
 *
 **************************************/
{
    char        **end, *p, *c;
    int i, j;

    db_name[0] = NULL;
    template_name[0] = NULL;
    input_file[0] = NULL;

    /* Inititialize globals */

    for (i = 0; i < MAX_FIELDS; i++)
        ignore_cols[i] = 0;

    for (i = 0; i < MAX_ARRAYS; i++) {
        array_fields[i].valid = 0;
        array_fields[i].field_size = 0;
        for ( j = 0; j < 16; j++ ) {
                array_fields[i].array_items[j].index = 0;
                array_fields[i].array_items[j].max_db_elements = 0;
                array_fields[i].array_items[j].current_rec_elements = 0;
                array_fields[i].array_items[j].end_element = 0;
                }
        }

    for (end = argv + argc, ++argv; argv < end;)
        {
        p = *argv++;
        if (*p++ != '-')
            {
            printf ("don't understand '%s'\n", --p);
            exit (1);
            }
        switch (UPPER (*p))
            {
            case 'C' : /* allow C style comments in the data file */
                COMMENTS = 1;
                break;
    
            case 'D' : /* database name */
                strcpy (db_name, *argv++);
                break;
    
            case 'F' : /* input file name */
                strcpy (input_file, *argv++);
                break;
    
            case 'H' : /* help switch */
                fprintf (stdout, "\nGCONV Help!\n\n" );
                syntax ();
                exit (1);
                break;

            case 'I' : /* ignore columns for input */
                get_ignore (*argv);
                argv++;
                break;

            case 'L' : /* line ranges */
                strcpy (line_char, *argv++);
                get_line_range ();
                break;

            case 'T' : /* template name */
                strcpy (template_name, *argv++);
                break;
    
            case 'v' : /* validate max field lengths of text file */
                VALIDATE = 1;
                break;
    
            case 'X' : /* do debugging */
                DEBUG_PRINT = 1;
                break;

            case 'Z' : /* print version of db accesses */
                PRINT_VERSION = 1;
                break;
    
            default:
                fprintf (stdout, "\ngconv: Invalid switch!\n\n" );
                syntax ();
                exit (1);
                break;
            }
        }

    if (DEBUG_PRINT)
        {
        printf ("The database name is %s\n", db_name);
        printf ("The input file name is %s\n", input_file);
        printf ("The template name is %s\n", template_name);
        printf ("The validate option (-v) = %d\n", VALIDATE);
        printf ("The comments option (-c) = %d\n", COMMENTS);
        printf ("The version (-z) = %d\n", PRINT_VERSION);
        }

    if (!db_name[0])
        {
        get_string ("\nPlease enter db pathname ('quit' to exit): ",
                    db_name, sizeof (db_name));
        for (c = db_name; *c  && *c != ' '; c++)
            ;
        *c = 0;
        if (!strcmp (db_name, "quit")) 
            exit (1);
        }
    if (!input_file[0])
        {
        get_string ("\nPlease enter ASCII file pathname ('quit' to exit): " ,
                     input_file, sizeof (input_file));
        if (!strcmp (input_file, "quit")) 
            exit (1);
        }
    if (!template_name[0])
        {
        get_string ("\nPlease enter template pathname ('quit' to exit): ",
                   template_name, sizeof (template_name));
        if (!strcmp (input_file, "quit")) 
            exit (1);
         }
}


int syntax ()
/**************************************
 *
 *        s y n t a x
 *
 **************************************
 *
 * Functional description
 *
 *        Print out command syntax info.
 *
 **************************************/
{
    fprintf (stdout, "Syntax: GCONV [-d (database name)] [-f (input name)] [-i 3,4,5] [-l 9-999] [-t (template name)] [-c] [-z] [-h]\n");
    fprintf (stdout, "\t-c: allow C style (/* ... */ comments in data file\n");
    fprintf (stdout, "\t-d: database name\n");
    fprintf (stdout, "\t-f: input file name\n");
    fprintf (stdout, "\t-h: print this help text\n");
    fprintf (stdout, "\t-i: ignore columns for input (comma list)\n");
    fprintf (stdout, "\t-l: line range (ending line number must be preceeded by \"-\")\n");
    fprintf (stdout, "\t-t: conversion template file name\n");
    fprintf (stdout, "\t-v: validate and report max sizes of input file fields\n");
    fprintf (stdout, "\t-z: print Interbase Version information\n");
    fprintf (stdout, "\n\n");
}

int too_big (field_num, field_name, field_length, actual_length, field_contents, rec_number)
    int field_num;
    char *field_name;
    int field_length;
    int actual_length;
    char *field_contents;
    int rec_number;
/**************************************
 *
 *        t o o _ b i g
 *
 **************************************
 *
 * Functional description
 *
 *        Print out message re field length exceeeded (i.e. input data for
 *        a char or varying field is longer than field defined in database.)
 *
 **************************************/
{
    printf ("Length exceeded for FIELD %d: %s. \n\t\tMax Length = %d, Actual = %d\n\n",
            field_num, field_name, field_length, actual_length);
    printf ("Record # %d Contents = %*s.\n",rec_number, actual_length, field_contents);
    printf ("\n");
}

char *trim (temp_string)
    char *temp_string;
/*************************************
 *
 *        t r i m
 *
 *************************************
{
 *
 * Functional description
 *
 *        Strip trailing blanks.
 *
 **************************************/
{
    char *s, *end;
    int i;

    end = temp_string - 1;
    s = end + strlen (temp_string);
    while (s != end && *s == ' ')
        *s-- = 0;

    return (temp_string);
}

static void move (from, to, length)
    char        *from;
    char        *to;
    short        length;
{
/**************************************
 *
 *        m o v e
 *
 **************************************
 *
 * Functional description
 *        Move some bytes.
 *
 **************************************/

    if (length)
        do *to++ = *from++; while (--length);
}

int parse_date (quad_date, value)
    ISC_QUADW   *quad_date;
    char        *value;
{
/**************************************
 *
 *        p a r s e _ d a t e
 *
 **************************************
 *
 * Functional description
 *        parse date string 
 *
 **************************************/
    char            *p, *end, **month_ptr, *m;
    unsigned char        c, temp [15], *t;
    unsigned short        length, n, month_position, i, components [7];
    struct tm d;

    length = strlen (value);
    p = value;
    end = p + length;
    month_position = 0;

    for (i = 0; i < 7; i++)
        components [i] = 0;

    /* Parse components */

    for (i = 0; i < 7; i++)
        {

        /* Skip leading blanks.  If we run out of characters, we're done
           with parse.  */

        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p == end)
            break;
    
        /* Handle digit or character strings */
    
        c = UPPER (*p);
        if (DIGIT (c))
                {
                n = 0;
                while (p < end && DIGIT (*p))
                    n = n * 10 + *p++ - '0';
                }
        else if (LETTER (c))
                {
                t = temp;
                while (p < end && LETTER (c))
                    {
                    c = UPPER (*p);
                if (!LETTER (c))
                    break;
                *t++ = c;
                p++;
                }
            *t = 0;
            month_ptr = months;
            while (TRUE)
                {
                if (!*month_ptr)
                    {
            
                        /* it's not a month name, so it's either a magic word or
                           a non-date string.  If there are more characters, it's bad */
            
                        while (++p < end)
                           if (*p != ' ' && *p != '\t' && *p != 0)
                                return FALSE;
                    
                        return FALSE;
                    }
                for (t = temp, m = *month_ptr++; *t && *t == *m; t++, m++)
                        ;
                if (!*t)
                    break;
            }
            n = month_ptr - months;
            month_position = i;
            }
        else
                return FALSE;
        components [i] = n;
        while (p < end && (*p == ' ' || *p == '\t'))
                p++;
        if (*p == '/' || *p == '-' || *p == ',' || *p == ':')
            {
            p++;
            continue;
            }
        if (*p == '.')
            {
            if (!month_position && i < 2)
                month_position = 1;
            p++;
            continue;
            }
        }

    /* Slide things into day, month, year form */

    if (!components [0] && !components [1] && !components [2])
        {
        d.tm_mon = d.tm_mday = 0;
        }
    if (month_position)
        {
        d.tm_mday = components [0];
        d.tm_mon = components [1];
        }
    else
        {
        d.tm_mday = components [1];
        d.tm_mon = components [0];
        }

    d.tm_year = components [2];

/*
    if (d.tm_year < 50)
        d.tm_year += 2000;
    else if (d.tm_year < 100)
        d.tm_year += 1900;     
*/
    if ( d.tm_year > 1900 ) 
        d.tm_year -= 1900;

    isc_encode_date ( &d, quad_date );
        
    if (DEBUG_PRINT)
        {
        printf ( "\nparse_date:\tquad_high = %d\n", quad_date->gds_quad_high );
        printf ( "parse_date:\tquad_low = %d\n", quad_date->gds_quad_low );
        printf ( "parse_date:\td.tm_mon = %d\n", d.tm_mon );
        printf ( "parse_date:\td.tm_mday = %d\n", d.tm_mday );
        printf ( "parse_date:\td.tm_year = %d\n\n", d.tm_year );
        }

    return TRUE;
}

iscx_array_put_slice (status, db_handle, trans_handle, array_id, 
                            desc, array, slice_length)
    long        *status;
    long        *db_handle;
    long        *trans_handle;
    ISC_QUADW        *array_id;
    ARRAY_DESC        *desc;
    void        *array;
    long        *slice_length;
{
/**************************************
 *
 *        i s c x _ a r r a y _ p u t _ s l i c e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
char        sdl_buffer [1024];
short        sdl_length;

sdl_length = sizeof (sdl_buffer);

if (isc_array_gen_sdl (status, desc, &sdl_length, sdl_buffer, &sdl_length))
    return status [1];

return isc_put_slice (status, db_handle, trans_handle, array_id, 
                      sdl_length, sdl_buffer, 0, 0, 
                      *slice_length, array);
}
