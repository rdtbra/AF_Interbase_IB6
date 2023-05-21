/***********************************************************************
 *                                                                     *
 *  gdump.e Author:  Paul McGee, Interbase Software                    *
 *          Purpose:  To provide a menu interface to output data       *
 *                    from the database to a text file.                *
 *                                                                     *
 *                                                                     *
 *  Interbase DOES NOT ASSUME RESPONSIBILITY FOR THE RESULTS OF        *
 *  USING THIS PROGRAM.  It is NOT covered by the normal Technical     *
 *  Support Agreements between Interbase and it's customers.           *
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

#ifdef VMS
#define TIME_DEFINED
#include <time.h>
#endif

#ifdef apollo
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef sun
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef hpux
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef ultrix
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef DGUX
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef sgi
#define TIME_DEFINED
#include <sys/time.h>
#endif

#if (defined __osf__ && defined __alpha)
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifdef mpexl
#define TIME_DEFINED
#include <sys/time.h>
#endif

#ifndef TIME_DEFINED
#include <time.h>
#endif

DATABASE DB = FILENAME  "atlas.gdb";

#define MAX_FIELDS 100    
#define MAX_BUFFER 8096
#define MAX_SMALL  256
#define MAIN_SIZE  80
#define VERSION "V3.2K"
#define UPPER(c)                 ((c >= 'a' && c<= 'z') ? c + 'A' - 'a': c )
           
static    char*  weekdays_3[]=
            {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static    char*  monthnames_3[]=
            {
             "Jan", "Feb", "Mar", "Apr", "May", "Jun",
             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };

static    int   month_lengths[]= 
            { 31,28,31,30,31,30,31,31,30,31,30,31 };

#ifdef sun
#define FUNC_KEYS 1
static    char* fk[]={ "NULL", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
                       "R9", "R11", "R15", "R8", "R14", "R10", "R12",
                       "NULL", "NULL"};
#endif

#ifdef VMS
#define FUNC_KEYS 1
static    char* fk[]={ "NULL", "PF1", "PF2", "PF3", "PF4", "F17", "F7", "F8",
                       "F9", "F10", "ENTER", "UP", "DOWN", "RIGHT", "LEFT",
                       "PREV", "NEXT" };
#endif

#ifdef ULTRIX
#define FUNC_KEYS 1
static    char* fk[]={ "NULL", "PF1", "PF2", "PF3", "PF4", "F17", "F7", "F8",
                       "F9", "F10", "ENTER", "UP", "DOWN", "RIGHT", "LEFT",
                       "PREV", "NEXT" };
#endif

#ifndef FUNC_KEYS
#define FUNC_KEYS 1
static    char* fk[]={ "NULL", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
                       "SF1", "SF2", "ENTER", "UP", "DOWN", "RIGHT", "LEFT",
                       "PGUP", "PGDWN" };
#endif

static char entry_form[]={
"24O18D`Exclusion Entry Screen`2N20.3N2.5N22.E25O21N3.23N72.\
13S`FIELD_1`35S`X(70)`20D``2N1.3N5.5N70.52N1.E25O21N3.23N72.\
13S`FIELD_2`35S`X(70)`20D``2N1.3N8.5N70.52N1.E25O21N3.23N72.\
13S`FIELD_3`35S`X(70)`20D``2N1.3N11.5N70.52N1.EE"
};

static unsigned char dyn_gdl[] = {
    gds__dyn_version_1, 
       gds__dyn_begin, 
          gds__dyn_def_rel, 11,0, 'P','Y','X','I','S','$','F','O','R','M','S',
             gds__dyn_end, 
          gds__dyn_def_global_fld, 15,0, 'P','Y','X','I','S','$','F','O','R','M','_','N','A','M','E',
             gds__dyn_fld_type, 2,0, 14,0,
             gds__dyn_fld_length, 2,0, 31,0,
             gds__dyn_fld_scale, 2,0, 0,0,
             gds__dyn_fld_sub_type, 2,0, 0,0,
             gds__dyn_end, 
          gds__dyn_def_local_fld, 15,0, 'P','Y','X','I','S','$','F','O','R','M','_','N','A','M','E',
             gds__dyn_rel_name, 11,0, 'P','Y','X','I','S','$','F','O','R','M','S',
             gds__dyn_fld_position, 2,0, 1,0,
             gds__dyn_end, 
          gds__dyn_def_global_fld, 15,0, 'P','Y','X','I','S','$','F','O','R','M','_','T','Y','P','E',
             gds__dyn_fld_type, 2,0, 14,0,
             gds__dyn_fld_length, 2,0, 16,0,
             gds__dyn_fld_scale, 2,0, 0,0,
             gds__dyn_fld_sub_type, 2,0, 0,0,
             gds__dyn_end, 
          gds__dyn_def_local_fld, 15,0, 'P','Y','X','I','S','$','F','O','R','M','_','T','Y','P','E',
             gds__dyn_rel_name, 11,0, 'P','Y','X','I','S','$','F','O','R','M','S',
             gds__dyn_fld_position, 2,0, 2,0,
             gds__dyn_end, 
          gds__dyn_def_global_fld, 10,0, 'P','Y','X','I','S','$','F','O','R','M',
             gds__dyn_fld_type, 2,0, 5,1,
             gds__dyn_fld_length, 2,0, 8,0,
             gds__dyn_fld_scale, 2,0, 0,0,
             gds__dyn_fld_sub_type, 2,0, 0,0,
             gds__dyn_fld_segment_length, 2,0, 'P',0,
             gds__dyn_end, 
          gds__dyn_def_local_fld, 10,0, 'P','Y','X','I','S','$','F','O','R','M',
             gds__dyn_rel_name, 11,0, 'P','Y','X','I','S','$','F','O','R','M','S',
             gds__dyn_fld_position, 2,0, 3,0,
             gds__dyn_end, 
          gds__dyn_end, 
    gds__dyn_eoc
};

int VERS      = 0;
int APPEND    = 0;
int ODOMETER  = 0;
int VERBOSE   = 0;
int PRINT_SQL = 0;
int COMMENTS  = 0;

GDS__QUAD null_blob_id = {0,0};

static char *trim();

char prog [MAX_SMALL];
char dbname [MAX_SMALL];
char output_name [MAX_SMALL];
FILE *fp1;

char comments[133];
char relation_name[32];

char conversion_type;
char conversion_char;

/* define macros for types of files to convert */

#define FIXED   '1'
#define SINGLE  '2'
#define QUOTES  '3'
#define QCOMMA  '4'
#define LINES   '5'

/* define field types */

struct convert_fields {
    char field_name [32];
    int field_code;
    int field_length;
    int user_length;
    int array_field;
    int array_num;
    };

struct convert_fields x_fields[MAX_FIELDS];

char WHERE_STRING [MAX_BUFFER];
char ORDER_STRING [MAX_BUFFER];

int total_picked;
int picked_fields [MAX_FIELDS];
int total_fields;
int total_records = 0;

typedef struct varying_text {
    short    vary_length;
    char     vary_string[1];
} *Varying_Text;

#define SHORT_BINARY 7
#define LONG_BINARY  8
#define SHORT_FLOAT  10
#define LONG_FLOAT   27
#define TEXT         14
#define VARYING      37
#define BLOB         261
#define DATE         35

#define F_ARRAY      262

#define F_CSTRING    40
#define F_QUAD       9
#define F_D_FLOAT    11
#define F_BLOB_ID    45

#define DATE_LENGTH  10

char DSQL_STRING [MAX_BUFFER];

#define MAX_ARRAYS   40

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

void move();

/**************************************
 *
 * Macro for aligning buffer space to boundaries 
 *   needed by a datatype on a given machine
 *
 **************************************/
#ifdef VMS
#define ALIGN(n,b)                                (n)
#endif

#ifdef sun
#ifdef sparc
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif
#endif

#ifdef hpux
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#ifdef ultrix
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#ifdef sgi
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#ifdef _AIX
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#ifdef DGUX
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#if (defined __osf__ && defined __alpha)
#define ALIGN(n,b)			((n + b - 1) & ~(b - 1))
#endif

#ifdef mpexl
#define ALIGN(n,b)                        ((n + b - 1) & ~(b - 1))
#endif

#ifndef ALIGN
#define ALIGN(n,b)                        ((n+1) & ~1)
#endif

EXEC SQL
       INCLUDE SQLDA;

SQLDA *sqlda;


main (argc, argv)
    int argc;
    char **argv;
/*******************************************
 *
 *        M A I N
 *
 *******************************************
*/
{
parse_args (argc, argv);
    
READY dbname AS DB;
ON_ERROR
        printf ( "\nUnable to Open database \"%s\"!\n", dbname );
        gds__print_status ( gds__status );
        exit (1);
END_ERROR;

if (VERS)
    {
    printf ("\n");
    gds__version ( &DB, NULL, NULL );
    printf ("\n");
    }

load_form();

START_TRANSACTION
ON_ERROR
        printf ( "\nUnable to Start Transaction!\n" );
        gds__print_status ( gds__status );
        exit (1);
END_ERROR;
                     
printf ("\nInterBase Extraction Utility: Version %s\n", VERSION);
printf ("\n------ Informational Messages ----------\n\n");

gds__width = 80;
gds__height = 24;

CREATE_WINDOW;

get_conv_type();

get_rel_name();  

get_fields();

get_where();

get_order();

DELETE_WINDOW;

setup_dsql ();

if (APPEND)
    printf ("Data being appended to \"%s\"\n", output_name);
else
    printf ("Data being written to \"%s\"\n", output_name);

printf ("Data format:\n\n");

print_fields ();

printf ("\n\n");

execute_dsql ();

printf ("\n\t%d record(s) written\n\n", total_records);

COMMIT
ON_ERROR
        printf ( "\nUnable to Commit Transaction!\n" );
        gds__print_status ( gds__status );
END_ERROR;


FINISH
ON_ERROR
        /* ignore errors on the FINISH */
        gds__print_status ( gds__status );
END_ERROR;

exit (0);
}


int get_conv_type()
/**************************************
 *
 *        g e t _ c o n v _ t y p e
 *
 **************************************
*/
{
/* Get conversion type   */

CASE_MENU  "GDUMP: Choose output file format"

    MENU_ENTREE        "Exit"        :
        DELETE_WINDOW;
        COMMIT;
        FINISH;
        fclose (fp1);
        exit (0);

    MENU_ENTREE        "1. Fixed length fields                      ":
        conversion_type = '1';

    MENU_ENTREE        "2. Fields separated by single character     ":
        conversion_type = '2';

    MENU_ENTREE        "3. Fields separated by double quotes        ":
        conversion_type = '3';

    MENU_ENTREE        "4. Fields separated by double quotes & comma":
        conversion_type = '4';

    MENU_ENTREE        "5. Fields on separate lines (1 per field)   ":
        conversion_type = '5';

END_MENU;
                                 
    /* Get seperator character */

if (conversion_type == '2') 
    {
    CASE_MENU  "GDUMP: Choose Separator Character"
            
        MENU_ENTREE     "  - SPACE        " :
            conversion_char = ' ';

        MENU_ENTREE     ", - COMMA        " :
            conversion_char = ',';

        MENU_ENTREE     "# - POUND SIGN   " :
            conversion_char = '#';

        MENU_ENTREE     "~ - TILDE        " :
            conversion_char = '~';

        MENU_ENTREE     ": - COLON        " :
            conversion_char = ':';

        MENU_ENTREE     "; - SEMICOLON    " :
            conversion_char = ';';

        MENU_ENTREE     "% - PERCENT      " :
            conversion_char = '%';

        MENU_ENTREE     "^ - HAT SIGN     " :
            conversion_char = '^';

        MENU_ENTREE     "@ - AT SIGN      " :
            conversion_char = '@';

        MENU_ENTREE     "! - EXCLAMATION  " :
            conversion_char = '!';

        MENU_ENTREE     "{ - LEFT BRACE   " :
            conversion_char = '{';

        MENU_ENTREE     "} - RIGHT BRACE  " :
            conversion_char = '}';

        MENU_ENTREE     "[ - RIGHT BRACKET" :
            conversion_char = '[';

        MENU_ENTREE     "] - LEFT BRACKET " :
            conversion_char = ']';

    END_MENU;
    }
else
    conversion_char = ' ';
}

int get_fields()
/**************************************
 *
 *        g e t _ f i e l d s
 *
 **************************************
*/
{
int i, j, k;

/* Initialize fields array  */
for ( i = 0; i < MAX_FIELDS; i++ ) {
    x_fields[i].field_name[0] = 0;
    x_fields[i].field_code    = 0;
    x_fields[i].field_length  = 0;
    x_fields[i].user_length   = 0;
    x_fields[i].array_field   = 0;
    x_fields[i].array_num     = 0;
    }

FOR_MENU F

    strcpy (F.TITLE_TEXT, "GDUMP: Pick a Field");
    F.TITLE_LENGTH = strlen (F.TITLE_TEXT);

    PUT_ITEM FY IN F
        strcpy (FY.ENTREE_TEXT, "EXIT");
        FY.ENTREE_LENGTH = strlen (FY.ENTREE_TEXT);
        FY.ENTREE_VALUE = -2;
    END_ITEM;

    PUT_ITEM FY IN F
        strcpy (FY.ENTREE_TEXT, "ALL");
        FY.ENTREE_LENGTH = strlen (FY.ENTREE_TEXT);
        FY.ENTREE_VALUE = -1;
    END_ITEM;
                                    
    i = 0;
    FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
      RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
      RF.RDB$SYSTEM_FLAG = 0 AND
      RF.RDB$RELATION_NAME = relation_name
      SORTED BY RF.RDB$FIELD_NAME
      REDUCED TO RF.RDB$FIELD_NAME
        PUT_ITEM FY IN F
            strcpy (FY.ENTREE_TEXT, RF.RDB$FIELD_NAME);
            FY.ENTREE_LENGTH = strlen (FY.ENTREE_TEXT);
            FY.ENTREE_VALUE = i;
        END_ITEM;
        strcpy (x_fields[i].field_name, RF.RDB$FIELD_NAME);
        x_fields[i].field_code = R.RDB$FIELD_TYPE;
        if ( !R.RDB$DIMENSIONS.NULL) {
            x_fields[i].array_field = 1;
            }
        x_fields[i].field_length = R.RDB$FIELD_LENGTH;
        x_fields[i].user_length = R.RDB$FIELD_LENGTH;
        if (VERBOSE > 9) {
            printf ( "\nget_fields:\t%2d: field_name = %s\n", i, x_fields[i].field_name );
            printf ( "get_fields:\t%2d: field_code = %d\n", i, x_fields[i].field_code );
            printf ( "get_fields:\t%2d: field_length = %d\n", i, x_fields[i].field_length );
            printf ( "get_fields:\t%2d: user_length = %d\n", i, x_fields[i].user_length );
            printf ( "get_fields:\t%2d: array_field = %d\n", i, x_fields[i].array_field );
            printf ( "get_fields:\t%2d: array_num = %d\n\n", i, x_fields[i].array_num );
            }
        i++;
    END_FOR
    ON_ERROR
        printf ( "\nUnable to get field information!\n" );
        gds__print_status ( gds__status );
        DELETE_WINDOW;
        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit (1);
    END_ERROR;

    total_fields = i;

    j = 0;
    k = 0;
    while (1)
        {
        DISPLAY F VERTICAL;

        if (F.ENTREE_VALUE == -2)
            {
            if (!k)
                {
                DELETE_WINDOW;
                fclose (fp1);

                ROLLBACK
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR;

                FINISH
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR

                exit (0);
                }
            else
                break;
            }

        k++;

        if (F.ENTREE_VALUE == -1)
            {
            for (i = 0; i < total_fields; i++)
                {
                if (x_fields[i].field_code != BLOB )
                    {
                    picked_fields[j++] = i;    
                    switch (x_fields[i].field_code)
                        {
                        case SHORT_BINARY:
                            x_fields[i].user_length = 6;
                            break;
                        case LONG_BINARY:
                            x_fields[i].user_length = 11;
                            break;
                        case SHORT_FLOAT:
                            x_fields[i].user_length = 16;
                            break;
                        case LONG_FLOAT:
                            x_fields[i].user_length = 16;
                            break;
                        case DATE:
                            x_fields[i].user_length = DATE_LENGTH;
                            break;
                        }
                    if ( x_fields[i].array_field && conversion_type != '5' )
                        {
                        printf ( "Array fields can only be printed with one field per line - format '5'\n");
                        DELETE_WINDOW;
                        ROLLBACK
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR;

                        FINISH
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR
                        exit (1);
                        }
                    if ( x_fields[i].array_field )
                        {
                        allocate_array ( relation_name, &x_fields[i], i );
                        }
                    }
                else if (x_fields[i].field_code == BLOB)
                    {
                    if ( conversion_type != '5' )
                        {
                        printf ( "Blobs can only be printed with one field per line - format '5'\n");
                        DELETE_WINDOW;
                        ROLLBACK
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR;

                        FINISH
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR
                        exit (1);
                        }
                    picked_fields[j++]=i;    
                    }
                }
            total_picked = j;
            return;
            }

        for (i = 0; i < total_fields; i++)
            if (!strcmp(x_fields[i].field_name,F.ENTREE_TEXT))
                break;
             
        if (x_fields[i].field_code == BLOB)
            {
            if ( conversion_type != '5' )
                {
                printf ( "Blobs can only be printed with one field per line - format '5'\n");
                DELETE_WINDOW;
                ROLLBACK
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR;

                FINISH
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR
                exit (1);
                }
            picked_fields[j++] = i;    
            }
        else
            {
            picked_fields[j++]=i;    
    
            switch (x_fields[i].field_code)
                {
                case SHORT_BINARY:
                    x_fields[i].user_length = 6;
                    break;
                case LONG_BINARY:
                    x_fields[i].user_length = 11;
                    break;
                case SHORT_FLOAT:
                    x_fields[i].user_length = 16;
                    break;
                case LONG_FLOAT:
                    x_fields[i].user_length = 16;
                    break;
                case DATE:
                    x_fields[i].user_length = DATE_LENGTH;
                    break;
                }
            if ( x_fields[i].array_field && conversion_type != '5' )
                {
                printf ( "Array fields can only be printed with one field per line - format '5'\n");
                DELETE_WINDOW;
                ROLLBACK
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR;

                FINISH
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR
                exit (1);
                }
            if ( x_fields[i].array_field )
                {
                allocate_array ( relation_name, &x_fields[i], i );
                }
            }
        }

    total_picked = j;

END_MENU;
}

int get_order ()
/**************************************
 *
 *        g e t _ o r d e r
 *
 **************************************
*/
{
int x, y, i, j, k, result;
int cont = 1;
int picked_field;
char order_num [10];
char mini_order_string [MAX_BUFFER / 2];

    CASE_MENU  "Do you Sort the output records?"

        MENU_ENTREE        "NO"  :
            return;

        MENU_ENTREE        "YES" :

    END_MENU;

FOR_MENU G

    PUT_ITEM GY IN G
        strcpy (GY.ENTREE_TEXT, "EXIT");
        GY.ENTREE_LENGTH = 4;
        GY.ENTREE_VALUE = -1;
    END_ITEM;

    strcpy (G.TITLE_TEXT, "GDUMP: Select Field to Sort By OR EXIT");
    G.TITLE_LENGTH = strlen (G.TITLE_TEXT);

    i = 0;
    for (i = 0; i < total_fields; i++)
        {
        /* go through all fields to grab name */
        for (j = 0; j < total_picked; j++)
            {
            if (picked_fields[j] == i)
                {
                if (x_fields[i].field_code != BLOB )
                    {
                    PUT_ITEM GY IN G
                        strcpy (GY.ENTREE_TEXT, x_fields[i].field_name);
                        GY.ENTREE_LENGTH = strlen (GY.ENTREE_TEXT);
                        GY.ENTREE_VALUE = j + 1;
                    END_ITEM;
                    j = total_picked;
                    }
                }
            }
        }

    j = 0;
    while (cont)
        {
        DISPLAY G VERTICAL;
        picked_field = G.ENTREE_VALUE;
        if (picked_field == -1)
            {
            cont = 0;
            continue;
            }
        if (!j)
            {
            strcpy (ORDER_STRING, "ORDER BY ");
            j = 1;
            }
        else
            strcat(ORDER_STRING, ", ");
                                 
        sprintf (order_num, "%d", picked_field);
        strcat (ORDER_STRING, order_num);
        }

END_MENU;

}

int get_rel_name()
/**************************************
 *
 *        g e t _ r e l _ n a m e
 *
 **************************************
*/
{
                             
FOR_MENU R

    strcpy (R.TITLE_TEXT, "GDUMP: Pick a Relation to Output");
    R.TITLE_LENGTH = strlen (R.TITLE_TEXT);

    PUT_ITEM RY IN R
        strcpy (RY.ENTREE_TEXT, "EXIT");
        RY.ENTREE_LENGTH = 4;
        RY.ENTREE_VALUE = 0;
    END_ITEM;

    FOR X IN RDB$RELATIONS WITH X.RDB$SYSTEM_FLAG = 0 SORTED BY X.RDB$RELATION_NAME
        PUT_ITEM RY IN R
            strcpy (RY.ENTREE_TEXT, X.RDB$RELATION_NAME);
            RY.ENTREE_LENGTH = strlen (RY.ENTREE_TEXT);
            RY.ENTREE_VALUE = 1;
        END_ITEM;
    END_FOR
    ON_ERROR
        printf ( "\nCan't get pick list of relations. Quiting!\n" );
        gds__print_status ( gds__status );
        ROLLBACK;
        FINISH;
        exit (1);
    END_ERROR;

    DISPLAY R VERTICAL;

    if (R.ENTREE_VALUE)
        strcpy (relation_name, R.ENTREE_TEXT);
    else
        {
        DELETE_WINDOW;

        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        fclose (fp1);
        exit (0);
        }

END_MENU;
}

int get_where ()
/**************************************
 *
 *        g e t _ w h e r e
 *
 **************************************
*/
{
int x, y, i, j, k, result;
int cont = 1;
int picked_field;
char where_operator [10];
char mini_where_string [MAX_BUFFER / 2];

CASE_MENU  "Do you want to exclude any records?"

    MENU_ENTREE        "NO"  :
        return;

    MENU_ENTREE        "YES" :

END_MENU;

FOR_MENU G

    PUT_ITEM GY IN G
        strcpy (GY.ENTREE_TEXT, "EXIT");
        GY.ENTREE_LENGTH = 4;
        GY.ENTREE_VALUE = -1;
    END_ITEM;

    strcpy (G.TITLE_TEXT, "GDUMP: Select Field for Operation");
    G.TITLE_LENGTH = strlen (G.TITLE_TEXT);

    for (i = 0; i < total_fields; i++)
        {
        for (j = 0;j < total_picked; j++)
            {
            if (picked_fields[j] == i)
                {
                PUT_ITEM GY IN G
                    strcpy (GY.ENTREE_TEXT, x_fields[i].field_name);
                    GY.ENTREE_LENGTH = strlen (GY.ENTREE_TEXT);
                    GY.ENTREE_VALUE = i;
                END_ITEM;
                j = total_picked;
                }
            }
        }

    j = 0;

    while (cont)
        {
        DISPLAY G VERTICAL;

        picked_field = G.ENTREE_VALUE;

        if (picked_field == -1)
            {
            cont = 0;
            continue;
            }

        CASE_MENU  "Select Operator"

            MENU_ENTREE        "EQUALS                 "  :
                strcpy (where_operator, "=");
    
            MENU_ENTREE        "NOT EQUAL              "  :
                strcpy (where_operator, "!=");
    
            MENU_ENTREE        "GREATER THAN           " :
                strcpy (where_operator, ">");

            MENU_ENTREE        "GREATER THAN OR EQUAL  " :
                strcpy (where_operator, ">=");
    
            MENU_ENTREE        "LESS THAN              " :
                strcpy (where_operator, "<");
    
            MENU_ENTREE        "LESS THAN OR EQUAL     " :
                strcpy (where_operator, "<=");
                            
            MENU_ENTREE        "LIKE                   " :
                strcpy (where_operator, "LIKE");
                            
            MENU_ENTREE        "NULL                   " :
                strcpy (where_operator, "IS NULL");
                            
            MENU_ENTREE        "NOT NULL               " :
                strcpy (where_operator, "IS NOT NULL");
                            
            MENU_ENTREE        "EXIT"        :
                cont=0;

        END_MENU;

        if (!j)
            {
            strcpy (WHERE_STRING, "WHERE ");
            j = 1;
            }

        strcat (WHERE_STRING, trim (x_fields[picked_field].field_name));
        strcat (WHERE_STRING, " ");
        strcat (WHERE_STRING, where_operator);

        if (strcmp (where_operator, "IS NULL") && strcmp (where_operator, "IS NOT NULL") && cont )
            {
            strcat (WHERE_STRING," ");
            switch (x_fields[picked_field].field_code)
                {
                case SHORT_BINARY:
                    strcpy (mini_where_string, "Enter interger value for the expression: ");
                    break;
                case LONG_BINARY:
                    strcpy (mini_where_string, "Enter interger value for the expression: ");
                    break;
                case SHORT_FLOAT:
                    strcpy (mini_where_string, "Enter floating point value for the expression: ");
                    break;
                case LONG_FLOAT:
                    strcpy (mini_where_string, "Enter floating point value for the expression: ");
                    break;
                case DATE:
                    strcpy (mini_where_string, "Enter date string for the expression: ");
                    break;
                case VARYING:
                    strcpy (mini_where_string, "Enter character string for the expression: ");
                    break;
                case TEXT:
                    strcpy (mini_where_string, "Enter character string for the expression: ");
                    break;
                }
            strcat (mini_where_string, trim(x_fields[picked_field].field_name));
            strcat (mini_where_string, " ");
            strcat (mini_where_string, where_operator);
    
            FOR FORM x IN GDUMP_ENTRY
                k = 1;
                while (k)
                    {
                    strcpy (x.field_1, mini_where_string);
                    strcpy (x.field_2, "");
                    strcpy (x.field_3, "");
                    DISPLAY x DISPLAYING * CURSOR ON  field_2 ACCEPTING  field_2
                        WAKING ON  field_2;
    
                    sprintf (x.field_3, "Press %s to accept and %s to edit", fk[1], fk[4]);
Re_Enter:
                    DISPLAY x DISPLAYING * CURSOR ON  field_2 ACCEPTING  field_2;
    
                    switch (x.TERMINATOR)
                        {
                        case PYXIS__KEY_PF1:
                            switch (x_fields[picked_field].field_code)
                                {
                                case SHORT_BINARY:
                                case LONG_BINARY:
                                case SHORT_FLOAT:
                                case LONG_FLOAT:
                                    strcat (WHERE_STRING, x.field_2);
                                    break;

                                case DATE:
                                case VARYING:
                                case TEXT:
                                    strcat (WHERE_STRING, "'");
                                    strcat (WHERE_STRING, x.field_2);
                                    strcat (WHERE_STRING, "'");
                                    break;
                                }
                            k = 0;
                            break;

                        case PYXIS__KEY_PF4:
                            strcpy (x.field_3,"");
                            break;

                        default:
                            sprintf (x.field_3, "Invalid Key !! Press %s to accept and %s to edit", fk[1], fk[4]);
                            goto Re_Enter;
                            break;
                        }
                    }
            END_FORM;
            }
    
        CASE_MENU  "Select Continuation"
    
            MENU_ENTREE        "AND"  :
                strcat (WHERE_STRING, " AND ");
    
            MENU_ENTREE        "OR "  :
                strcat (WHERE_STRING, " OR ");
    
            MENU_ENTREE        "EXIT "        :
                cont = 0;
    
        END_MENU;

        }

END_MENU;

}

static load_form ()
/**************************************
 *
 *        l o a d _ f o r m
 *
 **************************************
*/
{
    int count = 0;
    long *load_trans;
                                
    load_trans = 0L;

    START_TRANSACTION load_trans
    ON_ERROR
        printf ( "\nCan't Start Transaction to load form!\n" );
        gds__print_status ( gds__status );
        FINISH;
        exit (1);
    END_ERROR;

    FOR (TRANSACTION_HANDLE load_trans) l IN RDB$RELATIONS WITH
        l.RDB$RELATION_NAME = 'PYXIS$FORMS'
    {
        count++;
    }
    END_FOR
    ON_ERROR
        printf ( "\nCan't check if form exists!\n" );
        gds__print_status ( gds__status );
        ROLLBACK load_trans;
        FINISH;
        exit (1);
    END_ERROR;

    if (!count)
        {
        gds__ddl (gds__status, GDS_REF(DB), GDS_REF(load_trans), sizeof (dyn_gdl), dyn_gdl);
        if (!gds__status[1])
            {
            COMMIT load_trans;
            ON_ERROR
                printf ( "\nCan't Commit definition of PYXIS$FORMS Relation!\n" );
                gds__print_status ( gds__status );
                FINISH;
                exit (1);
            END_ERROR;

            START_TRANSACTION load_trans;
            ON_ERROR
                printf ( "\nCan't Start Transaction to load form!\n" );
                gds__print_status ( gds__status );
                FINISH;
                exit (1);
            END_ERROR;
            }
        else
            {
            printf ("Can't create PYXIS$FORMS relation in database \"%s\"!  Aborting!\n", dbname);
            gds__print_status (gds__status);
            ROLLBACK load_trans;
            FINISH;
            exit (1);
            }
        }

    FOR (TRANSACTION_HANDLE load_trans) l IN RDB$RELATIONS WITH
        l.RDB$RELATION_NAME = 'PYXIS$FORMS'
    {
        MODIFY l USING
            l.RDB$SYSTEM_FLAG = 2;
        END_MODIFY
        ON_ERROR  
            /* we're just setting the system flag to 2 - no big deal */
            printf ( "\nCan't change system flag to value of 2!\n" );
            gds__print_status ( gds__status );
            ROLLBACK load_trans;
            FINISH;
            exit (1);
        END_ERROR;
    }
    END_FOR
    ON_ERROR
        printf ( "\nCan't retrieve relation to change system flag!\n" );
        gds__print_status ( gds__status );
        ROLLBACK load_trans;
        FINISH;
        exit (1);
    END_ERROR;
    
    count = 0;
    FOR (TRANSACTION_HANDLE load_trans) l IN PYXIS$FORMS WITH
        l.PYXIS$FORM_NAME = 'GDUMP_ENTRY'
    {
        count++;
    }
    END_FOR
    ON_ERROR
        printf ( "\nCan't check if form exists!\n" );
        gds__print_status ( gds__status );
        ROLLBACK load_trans;
        FINISH;
        exit (1);
    END_ERROR;

    if (!count)
        store_form (entry_form, "GDUMP_ENTRY", load_trans);

    COMMIT load_trans
    ON_ERROR
        printf ( "\nCan't COMMIT checking/loading form!\n" );
        gds__print_status ( gds__status );
        ROLLBACK load_trans;
        FINISH;
        exit (1);
    END_ERROR;

    return;
}

int parse_args(argc, argv)
    int argc;
    char **argv;
/*****************************************************
 *
 *        p a r s e _ a r g s
 *
 *****************************************************
*/
{
char        **end, *p;

dbname[0] = NULL;
output_name[0] = NULL;

strcpy (prog, *argv);

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
        case 'A' : /* append to output file */
            APPEND = 1;
            if (VERBOSE)
                printf ("Appending to output file.\n");
            break;

        case 'C' : /* odometer count */
            ODOMETER = atoi (*argv++);
            if (VERBOSE)
                printf ("The odometer count is %d.\n", ODOMETER);
            break;

        case 'D' : /* database name */
            strcpy (dbname, *argv++);
            if (VERBOSE)
                printf("The database name is %s\n",dbname);
            break;

        case 'H' :
            printf ("\nGDUMP Help!\n\n");
            syntax ();
            exit (1);
            break;

        case 'O' : /* output name */
            strcpy (output_name, *argv++);
            if (VERBOSE)
                printf ("The output name is %s\n", output_name);
            break;

        case 'R' : /* put a C style comment after each record */
            sprintf (comments, "\/\* %s \*\/", *argv++);
            if (VERBOSE)
                printf("The comment is %s\n", comments);
            COMMENTS = 1;
            break;

        case 'S' : /* Print SQL Query */
            PRINT_SQL = 1;
            if (VERBOSE)
                printf ("The PRINT_SQL mode is %d\n", PRINT_SQL);
            break;

        case 'V' : /* verbose messages */
            VERBOSE = 1;
            PRINT_SQL = 1;
            VERS = 1;
            p++;
            if (*p != NULL)
                VERBOSE=atoi(p);
            printf ("The database name is %s\n", dbname);
            printf ("The verbose mode is %d\n", VERBOSE);
            printf ("The output name is %s\n", output_name);
            break;

        case 'Z' : /* gds version print */
            VERS = 1;
            break;

        default:
            printf("\ngdump: Invalid switch!\n\n");
            syntax();
            exit (1);
        }
    }
if (!dbname[0])
    {
    printf ("\nPlease enter db pathname ('quit' to exit): ");
    gets (dbname);
    if (!strcmp (dbname, "quit")) 
        exit (1);
    }
if (!output_name[0])
    {
    printf ("\nPlease enter output pathname ('quit' to exit): "); 
    gets (output_name);
    if (!strcmp (output_name, "quit")) 
        exit (1);
    }
}

int print_fields ()
/**************************************
 *
 *        p r i n t _ f i e l d s
 *
 **************************************
*/
{
register i;
int j;

for (i = 0; i < total_picked; i++)
    {
    j = picked_fields[i];
    switch (conversion_type)
        {
        case FIXED:
            printf ("%-*s", x_fields[j].user_length, x_fields[j].field_name);
            break;
        case SINGLE:
            printf ("%s", trim( x_fields[j].field_name));
            if (i + 1 < total_picked)
                printf("%c", conversion_char);
            break;
        case QUOTES:
            printf ("\"%s\"", trim( x_fields[j].field_name));
            break;
        case QCOMMA:
            printf ("\"%s\"", trim( x_fields[j].field_name));
            if (i + 1 < total_picked)
                printf("%s", ",");
            break;
        case LINES:
            printf ("%s\n", trim( x_fields[j].field_name));
            break;
        }
    }
}

static print_item (s, var)
    char    **s;
    SQLVAR    *var;
/**************************************
 *
 *        p r i n t _ i t e m
 *
 **************************************
 *
 * Functional description
 *    Print a SQL data item as described.
 *
 **************************************/
{
Varying_Text    vary;
char    *p;
short    dtype;
struct tm    times;

p = *s;

dtype = var->sqltype & ~1;

if ((var->sqltype & 1) && *var->sqlind)
    {
    /* field missing data */
    switch (dtype)
        {
        case SQL_SHORT:
            sprintf (p, "%d", 0);
            break;
        case SQL_LONG:
            sprintf (p, "%d", 0);
            break;
        case SQL_DOUBLE:
        case SQL_FLOAT:
            sprintf (p, "%f", 0);
            break;
        case SQL_TEXT:
            if (var->sqllen == 1)
                sprintf (p, "%1s", " ");
            else
                sprintf (p, "%*s", var->sqllen, " ");
            break;
        case SQL_VARYING:
            if (var->sqllen == 1)
                sprintf (p, "%1s", " ");
            else
                sprintf (p, "%*s", var->sqllen, " ");
            break;
        default:
            printf ( "\n\nERROR: print_item - unknown data type: %d.\n\n", dtype );
            DELETE_WINDOW;
            ROLLBACK
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR;

            FINISH
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR
            exit (1);
        }
    }
else
    {
    switch (dtype)
        {
        case SQL_SHORT:        
            sprintf (p, "%d", *(short*) (var->sqldata));
            break;
        case SQL_LONG:
            sprintf (p, "%d", *(long*) (var->sqldata));
            break;
        case SQL_DOUBLE:
            sprintf (p, "%f", *(double *)(var->sqldata));
            break;
        case SQL_FLOAT:
            sprintf (p, "%f", *(float *)(var->sqldata));
            break;
        case SQL_TEXT:
            if (var->sqllen == 1)
                {
                sprintf (p, "%1s", var->sqldata); 
                p++;
                *p = 0;
                }
            else
                {
                var->sqldata[var->sqllen] = NULL;
                sprintf (p, "%s", var->sqldata);
                }
            break;
        case SQL_VARYING:
            vary = (Varying_Text) var->sqldata;
            vary->vary_string[vary->vary_length] = 0;
            sprintf (p, "%*s%*s", vary->vary_length, vary->vary_string, 
                var->sqllen - vary->vary_length, " ");
            break;
        case SQL_DATE:
            gds__decode_date (var->sqldata, &times);
            sprintf (p, "%2d/%02d/%4d ", 
                    times.tm_mon + 1, times.tm_mday, times.tm_year + 1900);
            break;
        default:
            printf ( "\n\nERROR: print_item - unknown data type: %d.\n\n", dtype );
            DELETE_WINDOW;
            ROLLBACK
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR;

            FINISH
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR
            exit (1);
        }
    }

while (*p)
    ++p;

*s = p;
}

int setup_dsql ()
/**************************************
 *
 *        s e t u p _ d s q l
 *
 **************************************/
{
register i;
int j, k, l, tot_len;
char display_dummy[81];

strcpy (DSQL_STRING, "SELECT ");
for (i = 0; i < total_picked; i++)
    {
    j = picked_fields[i];
    strcat (DSQL_STRING, trim (x_fields[j].field_name));
    if (i < total_picked-1)
        strcat (DSQL_STRING, ", ");
    }
strcat (DSQL_STRING, " FROM ");
strcat (DSQL_STRING, trim (relation_name));
strcat (DSQL_STRING, " ");
strcat (DSQL_STRING, WHERE_STRING);
strcat (DSQL_STRING, " ");
strcat (DSQL_STRING, ORDER_STRING);

if (PRINT_SQL || VERBOSE ) {
    printf ( "\nDSQL Query:\n" );
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
}

static int execute_dsql ()
/**************************************
 *
 *        e x e c u t e _ d s q l
 *
 **************************************
*/
{
struct  tm  d;
int     cont_loop = 1;
int     i, j, k;
short   dtype;
char    *p;
char    pbuffer [MAX_BUFFER], buffer [MAX_BUFFER];
short    length, alignment, offset, lines;
SQLVAR    *var, *end;
int     picked;
long    blob_handle;
char    blob_segment [MAX_BUFFER];    /* blob segment */
unsigned short    blob_seg_length;        /* segment length */
    
if (APPEND)
    {
    if ((fp1 = fopen (output_name, "a")) == NULL)
        {
        perror (prog);
        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit (1);
        }
    }
else
    {
    if ((fp1 = fopen (output_name, "w")) == NULL)
        {
        perror (prog);
        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit (1);
        }
    }

sqlda = (SQLDA*) malloc (SQLDA_LENGTH (total_picked));
sqlda->sqln = total_picked;

EXEC SQL
    PREPARE Q INTO sqlda FROM :DSQL_STRING;

if ( SQLCODE ) {
    printf ( "\nCan't prepare DSQL query!\n" );
    gds__print_status ( gds__status );
    ROLLBACK
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR;

    FINISH
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR
    exit (1);
    }
                        
EXEC SQL 
    DECLARE C CURSOR FOR Q;

if ( SQLCODE ) {
    printf ( "\nCan't declare cursor!\n" );
    gds__print_status ( gds__status );
    ROLLBACK
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR;

    FINISH
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR
    exit (1);
    }

EXEC SQL 
    OPEN C;

if ( SQLCODE ) {
    printf ( "\nCan't open cursor!\n" );
    gds__print_status ( gds__status );
    ROLLBACK
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR;

    FINISH
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR
    exit (1);
    }
 
offset = 0;

for (var = sqlda->sqlvar, end = var + sqlda->sqld; var < end; var++)
    {
    length = alignment = var->sqllen;
    dtype = var->sqltype & ~1;
    if (dtype == SQL_TEXT)
        alignment = 1;
    else if (dtype == SQL_VARYING)
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

for (i = 0; i < sqlda->sqld; i++)
    {
    /* this will be fixed in V3.3 */
    if ( sqlda->sqlvar[i-1].sqlname_length > 30 ) {
        if (VERBOSE > 5) {
           printf ( "\nexecute_dsql: Previous field[%d] name length > 30!\n\n", i-1 );
           }
        if ( x_fields[i].array_field ) {
            sqlda->sqlvar[i].sqltype = SQL_ARRAY + 1;
            }
        else {
            switch ( x_fields[i].field_code ) {
                case SHORT_BINARY:
                    sqlda->sqlvar[i].sqltype = SQL_SHORT + 1;
                    break;
                case LONG_BINARY:
                    sqlda->sqlvar[i].sqltype = SQL_LONG + 1;
                    break;
                case SHORT_FLOAT:
                    sqlda->sqlvar[i].sqltype = SQL_FLOAT + 1;
                    break;
                case LONG_FLOAT:
                    sqlda->sqlvar[i].sqltype = SQL_DOUBLE + 1;
                    break;
                case TEXT:
                    sqlda->sqlvar[i].sqltype = SQL_TEXT + 1;
                    break;
                case VARYING:
                    sqlda->sqlvar[i].sqltype = SQL_VARYING + 1;
                    break;
                case BLOB:
                    sqlda->sqlvar[i].sqltype = SQL_BLOB + 1;
                    break;
                case DATE:
                    sqlda->sqlvar[i].sqltype = SQL_DATE + 1;
                    break;
                default:
                    printf ( "\nERROR: unknown field type: %d!\n", x_fields[i].field_code );
                    ROLLBACK
                    ON_ERROR
                       gds__print_status ( gds__status );
                    END_ERROR;
                
                    FINISH
                    ON_ERROR
                       gds__print_status ( gds__status );
                    END_ERROR
                    exit (1);
            } /* switch */
        } /* else */
    } /* if > 30  */

    if (VERBOSE)
        {
        printf ("Field # %d - %s\n", i, sqlda->sqlvar[i].sqlname);
        printf ("Field name len: %d\n", sqlda->sqlvar[i].sqlname_length);
        printf ("Field type:     %d\n", sqlda->sqlvar[i].sqltype);
        printf ("Field len:      %d\n", sqlda->sqlvar[i].sqllen);
        printf ("Field ind:      %d\n", sqlda->sqlvar[i].sqlind);
        printf ("Field data:     %ld\n", sqlda->sqlvar[i].sqldata);
        }
    }

EXEC SQL 
    FETCH C USING DESCRIPTOR sqlda;
                                           
if ( SQLCODE && SQLCODE != 100 ) {
    printf ( "\nCan't fetch!\n" );
    gds__print_status ( gds__status );
    ROLLBACK
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR;

    FINISH
    ON_ERROR
       gds__print_status ( gds__status );
    END_ERROR
    exit (1);
    }

i = 0;
while (cont_loop & !SQLCODE)
    {
    for (var = sqlda->sqlvar, end = var + sqlda->sqld, j = 0; var < end; var++)
        {
        k = picked_fields[j++]; /* now which picked field in the array this is */
        dtype = var->sqltype & ~1;
        switch (dtype)
            {
            case SQL_BLOB:
                if ((var->sqltype & 1) && *var->sqlind )
                    {
                    /* field missing data */
                    fprintf (fp1, "[EOB]\n" );
                    }
                else
                    {
                    /* get segments until through */
                    blob_handle = 0L;
                    gds__open_blob2 (gds__status, GDS_REF (DB), GDS_REF (gds__trans),
                                     GDS_REF (blob_handle), GDS_VAL(var->sqldata), 0, (char*) 0);
                    if (gds__status[1])
                        {
                        printf ( "Error occured reading blob in record %ld.\n", total_records );
                        gds__print_status (gds__status);
                        ROLLBACK
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR;

                        FINISH
                        ON_ERROR
                           gds__print_status ( gds__status );
                        END_ERROR
                        exit (1);
                        }
                    while (1)
                        {
                        gds__status[1] = gds__get_segment (gds__status, GDS_REF (blob_handle), 
                                                           GDS_REF (blob_seg_length), sizeof (blob_segment),
                                                           blob_segment);
                        if (gds__status[1] && (gds__status[1] != gds__segment))
                            break;
                        blob_segment[blob_seg_length] = NULL;
                        fprintf (fp1, "%s%s", trim (blob_segment),
                                blob_segment[blob_seg_length-1] == '\n' ? "" : "\n" );
                        }
                    gds__close_blob (gds__status, GDS_REF (blob_handle));
                    fprintf (fp1, "[EOB]\n" );
                    }
                break;

            case SQL_ARRAY:
                if ((var->sqltype & 1) && *var->sqlind )
                    {
                    /* field missing data */
                    fprintf (fp1, "[EOA]\n" );
                    }
                else
                    {
                    /* go thru dynamic array and print lines: 0,0,0,1,<value> */
                    print_array ( &x_fields[k], var->sqldata, fp1 );
                    fprintf (fp1, "[EOA]\n" );
                    }
                break;

            default:
                i++;
                p = pbuffer;
                print_item (&p, var);
                switch (conversion_type)
                    {
                    case FIXED:
                        fprintf (fp1,"%s", pbuffer);
                        break;
                    case SINGLE:
                        fprintf (fp1,"%s", trim (pbuffer));
                        if (i < total_picked)
                            fprintf (fp1, "%c", conversion_char);
                        break;
                    case QUOTES:
                        fprintf (fp1,"\"%s\"", trim (pbuffer));
                        break;
                    case QCOMMA:
                        fprintf (fp1,"\"%s\"", trim (pbuffer));
                        if (i < total_picked)
                            fprintf(fp1,"%s",",");
                        break;
                    case LINES:
                        fprintf (fp1,"%s\n", trim (pbuffer));
                        if (VERBOSE)
                            printf ( "\nexecute_dsql:\t%2d:%s\n", i - 1, pbuffer );
                        break;
                    }
            }
        }
    switch (conversion_type)
        {
        case FIXED:
        case SINGLE:
        case QUOTES:
        case QCOMMA:
            fprintf (fp1,"\n");
            break;
        case LINES:
            break;
        }

    if ( COMMENTS ) {
        fprintf ( fp1, "%s\n", comments );
        }
                                  
    total_records++;

    if (ODOMETER && !(total_records % ODOMETER))
        printf ("\t\t%d records written\n", total_records );

    EXEC SQL 
        FETCH C USING DESCRIPTOR sqlda;

    if ( SQLCODE && SQLCODE != 100 ) {
        printf ( "\nCan't fetch again!\n" );
        gds__print_status ( gds__status );
        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit (1);
        }

    if (SQLCODE)
        {
        fclose (fp1);
        cont_loop = 0;
        }

    i = 0;
    }

return;
}

store_form (temp_str, temp_str2, new_trans)
    char *temp_str;
    char *temp_str2;
    long *new_trans;
/**************************************
 *
 *        s t o r e _ f o r m
 *
 **************************************
*/
{
    int main_length;
    int start, end, diff;
    char form_str [MAX_BUFFER];
                                
    strcpy (form_str, temp_str);        

    STORE (TRANSACTION_HANDLE new_trans) f IN PYXIS$FORMS USING
    {
        strcpy (f.PYXIS$FORM_NAME, temp_str2);
        main_length = strlen (form_str);
        start = end = diff = 0;
        CREATE_BLOB FORM_BLOB IN f.PYXIS$FORM;
        while (main_length > end)
            {
            end = start + MAIN_SIZE;
            if (end > main_length)
                end = main_length;
            diff = end - start;
            FORM_BLOB.LENGTH = diff;
            strncpy (FORM_BLOB.SEGMENT, &form_str[start], diff);
            if (VERBOSE)
                printf ("Start: %d   - End: %d\n%s\n\n", start, end, FORM_BLOB.SEGMENT); 
            PUT_SEGMENT FORM_BLOB;
            start = end;
            }
        CLOSE_BLOB FORM_BLOB;
    }
    END_STORE
    ON_ERROR
        printf ( "\nCan't Store form!\n" );
        gds__print_status ( gds__status );
        ROLLBACK new_trans
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit (1);
    END_ERROR;

    return;
}

syntax ()
/**************************************
 *
 *        s y n t a x
 *
 **************************************
*/
{
    printf ("Syntax: %s [-d (database name)] [-o (output name)] [-a] [-c 100] [-r \"comment\"] [-s] [-z] [-h]\n", prog);
    printf ("\t-a: append to output file name\n");
    printf ("\t-c: print number of records processed at this interval\n");
    printf ("\t-d: database name\n");
    printf ("\t-h: print this help text\n");
    printf ("\t-o: output file name\n");
    printf ("\t-r: print the following quoted string after each record in C style comments\n");
    printf ("\t-s: print sql query\n");
    printf ("\t-z: print Interbase Version information\n");
    printf ("\n\n");
}

static char *trim(temp_string)
    char *temp_string;
/*****************************************************
 *
 *        t r i m
 *
 *****************************************************
*/
{
char *s, *end;
int i;

end = temp_string - 1;
s = end + strlen (temp_string);
while (s != end && *s == ' ')
    *s-- = 0;
return (temp_string);
}

static int allocate_array (rel_name, fields, field_num)
    char *rel_name;
    struct convert_fields *fields;
    int field_num;
/*****************************************************
 *
 *        a l l o c a t e _  a r r a y
 *
 *****************************************************
*/
{
    int i, j, k;
    long result, total_result;
    short found;

    fields->array_num = MAX_ARRAY_NUM;
    array_fields[MAX_ARRAY_NUM].valid = 1;
    array_fields[MAX_ARRAY_NUM].field_num = field_num;
    array_fields[MAX_ARRAY_NUM].field_size = fields->field_length;

    if (VERBOSE > 5) {
        printf ( "\nallocate_array: MAX_ARRAY_NUM = %d\n", MAX_ARRAY_NUM );
        printf ( "\nallocate_array: fields->field_name = %s\n", fields->field_name );
        printf ( "\nallocate_array: array_fields[%d].field_size = %d\n",MAX_ARRAY_NUM,
            array_fields[MAX_ARRAY_NUM].field_size );
        }

    isc_array_lookup_bounds (gds__status, &DB, &gds__trans, rel_name, 
        fields->field_name, &mydesc[MAX_ARRAY_NUM]);

    /* force all fields to be fixed - let the engine figure it out */
    if ( fields->field_code == VARYING ) {
        mydesc[MAX_ARRAY_NUM].array_desc_dtype = TEXT;
        }

    for ( j = 0; j < mydesc[MAX_ARRAY_NUM].array_desc_dimensions; j++ ) {
        array_fields[MAX_ARRAY_NUM].array_items[j].max_db_elements = 
            mydesc[MAX_ARRAY_NUM].array_desc_bounds[j].array_bound_upper -
            mydesc[MAX_ARRAY_NUM].array_desc_bounds[j].array_bound_lower + 1;

        result = 1;

        /* Multiply all subordinate dimensions together */
        for ( k = j + 1; k < mydesc[MAX_ARRAY_NUM].array_desc_dimensions; k++ ) {
            result *= (mydesc[MAX_ARRAY_NUM].array_desc_bounds[k].array_bound_upper -
                mydesc[MAX_ARRAY_NUM].array_desc_bounds[k].array_bound_lower + 1);
            }

        if (VERBOSE > 5) {
            printf ( "\nallocate_array: result = %d\n", result );
            }

        /* Now multiply that by the field size, giving the # of bytes */
        /* to move for every element of this dimension                */

        array_fields[MAX_ARRAY_NUM].array_items[j].index = result *
            array_fields[MAX_ARRAY_NUM].field_size;
        total_result += array_fields[MAX_ARRAY_NUM].array_items[j].index;
        if (VERBOSE > 5) {
            printf ( "\nallocate_array: array_fields[%d].array_items[%d].index = %d\n", MAX_ARRAY_NUM, j,
                array_fields[MAX_ARRAY_NUM].array_items[j].index );
            }
        }
    array_fields[MAX_ARRAY_NUM].tot_array_size = 
        (array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements) *
        array_fields[MAX_ARRAY_NUM].array_items[0].index;

    /* now allocated and initialize mem buffer for largest size of this array */
    if (VERBOSE > 5) {
        printf ( "\nallocate_array: array_fields[%d].array_items[0].max_db_elements = %d\n", MAX_ARRAY_NUM,
            array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements );
        printf ( "\nallocate_array: array_fields[%d].array_items[0].index = %d\n", MAX_ARRAY_NUM,
            array_fields[MAX_ARRAY_NUM].array_items[0].index );
        printf ( "\nallocate_array: array_fields[%d].array_items[0].max_db_elements ) *\n\t\
            array_fields[%d].array_items[0].index /\n\t\
            array_fields[%d].field_size = %d\n",MAX_ARRAY_NUM, MAX_ARRAY_NUM, MAX_ARRAY_NUM,
            ( array_fields[MAX_ARRAY_NUM].array_items[0].max_db_elements ) *
            array_fields[MAX_ARRAY_NUM].array_items[0].index /
            array_fields[MAX_ARRAY_NUM].field_size );
        printf ( "\nallocate_array: number of memory units = %d\n\n\n",
            ( array_fields[MAX_ARRAY_NUM].tot_array_size / array_fields[MAX_ARRAY_NUM].field_size ) );
        printf ( "\nallocate_array: array_fields[%d].tot_array_size = %d\n", MAX_ARRAY_NUM,
            array_fields[MAX_ARRAY_NUM].tot_array_size );
        }

    if ( ( array_buffers[MAX_ARRAY_NUM] = (char *) malloc ( array_fields[MAX_ARRAY_NUM].tot_array_size ) ) == NULL )
        {
            printf ( "\nallocate_array: Memory already exhausted before array number %d!\n\n", MAX_ARRAY_NUM );
            ROLLBACK
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR;

            FINISH
            ON_ERROR
               gds__print_status ( gds__status );
            END_ERROR
            exit (1);
        }            

    MAX_ARRAY_NUM++;
}

static int print_array ( fields, array_id, fp2 )
    struct convert_fields *fields;
    GDS__QUAD *array_id;
    FILE *fp2;
/*****************************************************
 *
 *        p r i n t  _  a r r a y
 *
 *****************************************************
*/
{
    int j, k;
    long i;
    ARRAY_DESC tmp_desc;
    short tmp_short;
    long tmp_long;
    GDS__QUAD tmp_quad;
    float tmp_float;
    double tmp_double;
    struct tm d;
    short array_elements[16];
    int len;
    char *p;
    char *s;
    int array_num;

    array_num = fields->array_num;

    if (VERBOSE)
        printf("\nprint_array:\tInitializing ARRAY.\n");

    for ( i = (long) array_buffers[array_num], s = (char *) array_buffers[array_num];
        i < (*array_buffers[array_num] + array_fields[array_num].tot_array_size); i++ )
            *s++ = 0;

    if (VERBOSE)
        printf ("print_array:\tInitialized ARRAY.\n\n");
    
    tmp_desc = mydesc[array_num];

    if (VERBOSE) {
        printf ("\nprint_array:\tTMP_DESC values\n" );
        printf ("\nprint_array:\tarray_desc_dtype = %d.\n", tmp_desc.array_desc_dtype );
        printf ("print_array:\tarray_desc_length = %d.\n", tmp_desc.array_desc_length );
        printf ("print_array:\tarray_desc_field_name = %s.\n", tmp_desc.array_desc_field_name );
        printf ("print_array:\tarray_desc_relation_name = %s.\n", tmp_desc.array_desc_relation_name );
        printf ("print_array:\tarray_desc_dimensions = %d.\n", tmp_desc.array_desc_dimensions );
        printf ("print_array:\tarray_desc_flags = %d.\n", tmp_desc.array_desc_flags );
        for ( j = 0; j < mydesc[array_num].array_desc_dimensions; j++ ) {
            printf ("\n\tprint_array:\tarray_desc_bounds[%d].array_bound_lower = %d.\n", j, tmp_desc.array_desc_bounds[j].array_bound_lower );
            printf ("\tprint_array:\tarray_desc_bounds[%d].array_bound_upper = %d.\n", j, tmp_desc.array_desc_bounds[j].array_bound_upper );
            }
        printf ("\nprint_array:\tactual_array_len = %d.\n",
            array_fields[array_num].tot_array_size );
        }

    iscx_array_get_slice ( gds__status, &DB, &gds__trans, array_id, &tmp_desc,
        array_buffers[array_num], &array_fields[array_num].tot_array_size );

    if (gds__status[1])
        {
        printf ( "\nError retrieving array! Exiting!\n\n" );
        gds__print_status(gds__status);
        printf ( "\n\n" );
        ROLLBACK
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR;

        FINISH
        ON_ERROR
           gds__print_status ( gds__status );
        END_ERROR
        exit(1);
        }

    for ( i = 0; i < 16; i++ )
        array_elements[i] = tmp_desc.array_desc_bounds[i].array_bound_lower;

    for ( i = 0, s = (char *) array_buffers[array_num]; i < array_fields[array_num].tot_array_size; ) {
        tmp_short = 0;
        tmp_long = 0;
        tmp_quad = null_blob_id;
        tmp_float = 0.0;
        tmp_double = 0.0;

        if (VERBOSE > 5)
            printf ( "\nprint_array:\tarray_dimensions = " );

        for ( j = 0; j < tmp_desc.array_desc_dimensions; j++ ) {

            /* if not the first element print a comma first */
            if ( j ) {
                fprintf ( fp2, "," );
                if (VERBOSE > 5)
                    printf ( "," );
                }

            /* print out the current element value */
            fprintf ( fp2, "%d", array_elements[j] );
            if (VERBOSE > 5)
                printf ( "%d", array_elements[j] );

            /* increment the last element value  */
            if ( j == tmp_desc.array_desc_dimensions - 1 )
                array_elements[j]++;

            }

        /* print the comma before the data value */
        fprintf ( fp2, "," );
    
        if (VERBOSE > 5)
            printf ( "\n\n" );

        /* if you're at the element's upper bound */
        /* then set the current to lower bound and increment the previous */
        for ( j = tmp_desc.array_desc_dimensions - 1; j >= 0; j-- ) {
            if (VERBOSE > 5)
                printf ( "print_array:\tarray_elements[%d] = %d\n", j, array_elements[j] );
            if ( j && array_elements[j] > tmp_desc.array_desc_bounds[j].array_bound_upper ) {
                array_elements[j] = tmp_desc.array_desc_bounds[j].array_bound_lower;
                array_elements[j-1]++;
                if (VERBOSE > 5)
                      printf ( "print_array:\tpopped array_elements[%d]\n", j-1 );
                }
            }

        switch ( fields->field_code ) {
            case TEXT  :
            case VARYING  :
                if (VERBOSE > 5)
                    printf ( "\nprint_array:\ttext: " );
                for ( k = 0; k < tmp_desc.array_desc_length; k++ ) {
                    if (VERBOSE > 5)
                        printf ( "%c", *s );
                    fputc ( *s++, fp2 );
                    }
                fputc ( '\0', fp2 );
                if (VERBOSE > 5)
                    printf ( "%c\n", '\0' );
                break;
            case SHORT_BINARY :
                move ( s, &tmp_short, sizeof (tmp_short) );
                fprintf ( fp2, "%d", tmp_short );
                if (VERBOSE > 5) {
                    printf ( "\nprint_array:\tshort: %d\n", tmp_short );
                    }
                break;
            case LONG_BINARY  :
                move ( s, &tmp_long, sizeof (tmp_long) );
                fprintf ( fp2, "%ld", tmp_long );
                p = (char *) &tmp_long;
                if (VERBOSE > 5) {
                    printf ( "\nprint_array:\tlong: %ld\n", tmp_long );
                    }
                break;
            case LONG_FLOAT:
                move ( s, &tmp_double, sizeof (tmp_double) );
                fprintf ( fp2, "%f", tmp_double );
                if (VERBOSE > 5) {
                    printf ( "\nprint_array:\tdouble: %f\n", tmp_double );
                    }
                break;
            case SHORT_FLOAT :
                move ( s, &tmp_float, sizeof (tmp_float) );
                fprintf ( fp2, "%f", tmp_float );
                if (VERBOSE > 5) {
                    printf ( "\nprint_array:\tfloat: %f\n", tmp_float );
                    }
                break;
            case DATE  :
                move ( s, &tmp_quad, sizeof (tmp_quad) );
                gds__decode_date ( &tmp_quad, &d );
                fprintf ( fp2, "%d-%s-%d", 
                    d.tm_mday, monthnames_3[d.tm_mon], d.tm_year + 1900 );
                if (VERBOSE > 5) {
                    printf ( "\nprint_array:\tdate: %d : %d\n",
                        tmp_quad.gds_quad_high, tmp_quad.gds_quad_low );
                    printf ( "print_array:\tdate: %d-%s-%d\n", 
                        d.tm_mday, monthnames_3[d.tm_mon], d.tm_year + 1900 );
                    }
                break;
            default:
                printf ( "\nUnknown field type! Type = %d.\n\n", fields->field_code );
                ROLLBACK
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR;
        
                FINISH
                ON_ERROR
                   gds__print_status ( gds__status );
                END_ERROR
                exit (1);
            }

        /* we've already incremented s with the string fields */
        if ( fields->field_code != TEXT && fields->field_code != VARYING  ) 
            s += tmp_desc.array_desc_length;

        i += tmp_desc.array_desc_length;

        fprintf ( fp2, "\n" );

        } /* end of FOR loop */

    if (VERBOSE)
        printf ( "\nprint_array:\tend printing array\n" );

    return;
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

iscx_array_get_slice (status, db_handle, trans_handle, array_id, 
                            desc, array, slice_length)
    long        *status;
    long        *db_handle;
    long        *trans_handle;
    GDS__QUAD        *array_id;
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

return isc_get_slice (status, db_handle, trans_handle, array_id, 
                      sdl_length, sdl_buffer, 0, 0, 
                      *slice_length, array, slice_length);
}
