/***********************************************************************
 *                                                                     *
 *  gref.e   Author:  Paul McGee, Interbase Software                   *
 *          Purpose:  To provide a complete list of dependecies        *
 *                    for each database field.                         *
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

DATABASE CROSS_REF = FILENAME "atlas.gdb";

#include <stdio.h>
#include <ctype.h>

#define GET_STRING(p,b)        get_string (p, b, sizeof (b))
#define FALSE 0
#define TRUE 1
#define MAX 256
#define MAX_FIELDS 400

int VERSION = 0;
               
int single_field = 0;
char single_field_name [31];

char prog [64];
char db_name [41];
char title [81];

struct cross_ref_fields {
        char field_name [31];
        int field_code;
        int field_length;
        int field_segment;
        int field_computed;
        };

struct cross_ref_fields x_fields[MAX_FIELDS];
int num_of_fields = 0;

char type_codes [262] [31];

int parse_args();
int convert_upper();
int init_types(), get_fields(), get_single_field();
void center_print();
void cross_ref_relations(), cross_ref_views(), cross_ref_indexes();
void cross_ref_procedures(), cross_ref_triggers(), cross_ref_validation();
void cross_ref_computed_fields();

/**************************************
 *                                    *
 *   m a i n                          *
 *                                    *
 **************************************
 *                                    *
 * Functional description             *
 *                                    *
 **************************************/
main (argc,argv)
int argc;
char *argv[];
{

        register i; 

        parse_args(argc,argv);

        printf("\n\n");
        title[0]='\0';
        strcpy(title,"Dependency Listing for ");
        strcat(title,db_name);
        center_print(80,title);
        
        READY db_name AS CROSS_REF;

	if (VERSION) {
		printf("\n");
		gds__version (&CROSS_REF, NULL, NULL);
		printf("\n");
		}

        START_TRANSACTION;
        
        init_types();                              

        if (single_field)
                num_of_fields = get_single_field(x_fields);
        else                                         
                num_of_fields = get_fields(x_fields);

        for (i=0;i<num_of_fields;i++) {
                printf("\n\n%s\n------------------------------\n",x_fields[i].field_name);
                print_field_info(x_fields[i]);
                cross_ref_relations(x_fields[i].field_name);
                cross_ref_views(x_fields[i].field_name);
                cross_ref_indexes(x_fields[i].field_name);
                cross_ref_procedures(x_fields[i].field_name);
                cross_ref_triggers(x_fields[i].field_name);
                cross_ref_validation(x_fields[i].field_name);
                if (x_fields[i].field_computed == 1)
                        cross_ref_computed_fields(x_fields[i].field_name);
        }

        printf("\n\n\n\n\n");

        printf("Number of fields = %d\n",num_of_fields);

        ROLLBACK;
        FINISH CROSS_REF;
}

static get_string (prompt_string, response_string, length)
    char        *prompt_string, *response_string;
    register                length;
{
/**************************************
 *                                    *
 *        g e t _ s t r i n g         *
 *                                    *
 **************************************
 *                                    *
 * Functional description             *
 *      Write a prompt and read a     *
 *      string.  If the string        *
 *      overflows, try again.  If we  *
 *      get an EOF, return FAILURE.   *
 *                                    *
 **************************************/
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

/*******************************************
 *                                         *
 *     c e n t e r _ p r i n t             *
 *                                         *
 *******************************************/
void center_print(c_length,cstr)
int c_length;
char *cstr;
{
        register i, j, k;
        char center_string[255];

        for (i=0;i<c_length;i++)
                center_string[i]=' ';
        center_string[i]='\0';
        i = strlen(cstr);
        j = (c_length - i) / 2;
        for (k=0;k<i;k++)
                center_string[j+k] = cstr[k];
        printf("%s\n",center_string);
}

/**************************************
 *                                    *
 *        g e t _ f i e l d s         *
 *                                    *
 **************************************/
int get_fields (x)
struct cross_ref_fields *x;
{
        struct cross_ref_fields y;
        register c, d, e;
        
        c = 0;
        
        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                    RF.RDB$SYSTEM_FLAG = 0 AND
                    RF.RDB$FIELD_NAME NOT CONTAINING 'PYXIS' AND
                    RF.RDB$FIELD_NAME NOT CONTAINING 'PICTOR' AND
                    RF.RDB$FIELD_NAME NOT CONTAINING 'QLI$' 
                        SORTED BY RF.RDB$FIELD_NAME 
                        REDUCED TO RF.RDB$FIELD_NAME 
                {
                strcpy(y.field_name,"");
                y.field_code = 0;
                y.field_length = 0;
                y.field_segment = 0;
                y.field_computed = 0;
        
                strcpy(y.field_name,RF.RDB$FIELD_NAME);
                y.field_code = R.RDB$FIELD_TYPE;
                y.field_length = R.RDB$FIELD_LENGTH;
                if (R.RDB$FIELD_TYPE == 261)
                        y.field_segment = R.RDB$SEGMENT_LENGTH;
                else
                        y.field_segment = 0;
        
                x[c++]=y;
                }
        END_FOR;

        e = 0;

        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                R.RDB$COMPUTED_BLR NOT MISSING
                {
                for (d=0;d<c;d++) {
                        if (!strcmp(RF.RDB$FIELD_NAME,x[d].field_name)) {
                                x[d].field_computed = 1;
                                break;
                                }
                        }
                }
        END_FOR;

        return c;
}

/**************************************
 *                                    *
 *   g e t _ s i n g l e _ f i e l d  *
 *                                    *
 **************************************/
int get_single_field (x)
struct cross_ref_fields *x;
{
        struct cross_ref_fields y;
        register c, d, e;
        
        c = 0;
        
        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                    RF.RDB$SYSTEM_FLAG = 0 AND
                    RF.RDB$FIELD_NAME = single_field_name
                        SORTED BY RF.RDB$FIELD_NAME 
                        REDUCED TO RF.RDB$FIELD_NAME 
                {
                strcpy(y.field_name,"");
                y.field_code = 0;
                y.field_length = 0;
                y.field_segment = 0;
                y.field_computed = 0;
        
                strcpy(y.field_name,RF.RDB$FIELD_NAME);
                y.field_code = R.RDB$FIELD_TYPE;
                y.field_length = R.RDB$FIELD_LENGTH;
                if (R.RDB$FIELD_TYPE == 261)
                        y.field_segment = R.RDB$SEGMENT_LENGTH;
                else
                        y.field_segment = 0;
        
                x[c++]=y;
                }
        END_FOR;

        e = 0;

        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                R.RDB$COMPUTED_BLR NOT MISSING
                {
                for (d=0;d<c;d++) {
                        if (!strcmp(RF.RDB$FIELD_NAME,x[d].field_name)) {
                                x[d].field_computed = 1;
                                break;
                                }
                        }
                }
        END_FOR;

        return c;
}

/**************************************
 *                                    *
 * p r i n t _ f i e l d _ i n f o    *
 *                                    *
 **************************************/
print_field_info (fields)
struct cross_ref_fields fields;
{
/*        printf("%30s  ",fields.field_name); */
        printf("\n\tField information:\n\t------------------\n\t\t");
        printf("%s,",type_codes[fields.field_code]);
        if (fields.field_code == 261)
                printf(" segment length %d\n",fields.field_segment);
        else {
                printf(" length %d",fields.field_length);
                if (fields.field_computed == 1)
                        printf(" (computed field)\n");
                else
                        printf("\n");
            }
        return;
}

/**************************************
 *                                    *
 *        i n i t _ t y p e s         *
 *                                    *
 **************************************/
init_types()
{
        int i;
        for (i=0;i<262;i++)
                strcpy(type_codes[i],"");
        strcpy(type_codes[7],"short binary");
        strcpy(type_codes[8],"long binary");
        strcpy(type_codes[10],"short floating");
        strcpy(type_codes[14],"text");
        strcpy(type_codes[27],"long floating");
        strcpy(type_codes[35],"date");
        strcpy(type_codes[37],"varying text");
        strcpy(type_codes[261],"blob");
        return;
}

/*****************************************
 *                                       *
 * c r o s s _ r e f _ r e l a t i o n s *
 *                                       *
 *****************************************/
void cross_ref_relations(field_name)
char *field_name;
{
        int i = 0;

        printf("\n\tRelations:\n\t----------\n");
        FOR RF IN RDB$RELATION_FIELDS CROSS 
                R IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH
                RF.RDB$FIELD_NAME = field_name AND
                R.RDB$VIEW_BLR MISSING
                        SORTED BY RF.RDB$RELATION_NAME 
                {
                        printf("\t\t%s\n",RF.RDB$RELATION_NAME);
                        i++;
                }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*****************************************
 *                                       *
 * c r o s s _ r e f _ v i e w s         *
 *                                       *
 *****************************************/
void cross_ref_views(field_name)
char *field_name;
{
        int i = 0;

        printf("\n\tViews:\n\t------\n");
        FOR RF IN RDB$RELATION_FIELDS CROSS 
                R IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH
                RF.RDB$FIELD_NAME = field_name AND
                R.RDB$VIEW_BLR NOT MISSING
                        SORTED BY RF.RDB$RELATION_NAME 
                {
                        printf("\t\t%s\n",RF.RDB$RELATION_NAME);
                        i++;
                }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*****************************************
 *                                       *
 * c r o s s _ r e f _ i n d e x e s     *
 *                                       *
 *****************************************/
void cross_ref_indexes(field_name)
char *field_name;
{
        int i = 0;

        printf("\n\tIndexes:\n\t--------\n");
        FOR R IN RDB$INDEX_SEGMENTS WITH
                R.RDB$FIELD_NAME = field_name 
                        SORTED BY R.RDB$INDEX_NAME
                {
                        printf("\t\t%s\n",R.RDB$INDEX_NAME);
                        i++;
                }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*******************************************
 *                                         *
 * c r o s s _ r e f _ p r o c e d u r e s *
 *                                         *
 *******************************************/
void cross_ref_procedures(field_name)
char *field_name;
{
        int i = 0;
        
        register j;

        for (j=0;j<strlen(field_name);j++)
                if (field_name[j] == ' ') {
                        field_name[j] = '\0';
                        break;
                }

        printf("\n\tProcedures:\n\t-----------\n");
        FOR Q IN QLI$PROCEDURES WITH
                Q.QLI$PROCEDURE.CHAR[80] CONTAINING field_name 
                        SORTED BY Q.QLI$PROCEDURE_NAME
                {
                        printf("\t\t%s\n",Q.QLI$PROCEDURE_NAME);
                        i++;
                }
        END_FOR
		ON_ERROR
		END_ERROR;
        if (!i)
                printf("\t\t<none>\n");
}

/*******************************************
 *                                         *
 * c r o s s _ r e f _ t r i g g e r s     *
 *                                         *
 *******************************************/
void cross_ref_triggers(field_name)
char *field_name;
{
        int i = 0;
        
        register j;

        for (j=0;j<strlen(field_name);j++)
                if (field_name[j] == ' ') {
                        field_name[j] = '\0';
                        break;
                }

        printf("\n\tTriggers:\n\t---------\n");
        FOR R IN RDB$TRIGGERS WITH R.RDB$TRIGGER_SOURCE.CHAR[80] CONTAINING field_name
                SORTED BY R.RDB$RELATION_NAME
                {
                        printf("\t\t%s\n",R.RDB$TRIGGER_NAME);
                        i++;
                }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*******************************************
 *                                         *
 * c r o s s _ r e f _ v a l i d a t i o n *
 *                                         *
 *******************************************/
void cross_ref_validation(field_name)
char *field_name;
{
        BSTREAM *bs;
        int c;
        int i = 0;

        printf("\n\tValidation Source:\n\t------------------\n");

        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                    RF.RDB$FIELD_NAME = field_name AND
	                R.RDB$VALIDATION_BLR NOT MISSING
        	            SORTED BY RF.RDB$FIELD_NAME 
                {
                if (bs = Bopen (&R.RDB$VALIDATION_SOURCE, CROSS_REF, gds__trans, "r")) {
                        printf("\t\t");
                            c = getb (bs);
                            while (c != EOF) {
                                if (c == '\n') {
                                        putchar (c);
                                        printf("\t\t");
                                        }
                                else
                                        putchar (c);
                                c = getb (bs);
                                }
                            BLOB_close (bs);
                            }
                i++;
                    }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*****************************************************
 *                                                   *
 * c r o s s _ r e f _ c o m p u t e d _ f i e l d s *
 *                                                   *
 *****************************************************/
void cross_ref_computed_fields(field_name)
char *field_name;
{
        BSTREAM *bs;
        int c;
        int i = 0;

        printf("\n\tComputed Field Source:\n\t-----------------------\n");

        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
                RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
                    RF.RDB$FIELD_NAME = field_name 
                        SORTED BY RF.RDB$FIELD_NAME 
                        REDUCED TO RF.RDB$FIELD_NAME
                {
                if (bs = Bopen (&R.RDB$COMPUTED_SOURCE, CROSS_REF, gds__trans, "r")) {
                        printf("\t\t");
                            c = getb (bs);
                            while (c != EOF) {
                                if (c == '\n') {
                                        putchar (c);
                                        printf("\t\t");
                                        }
                                else
                                        putchar (c);
                                c = getb (bs);
                                }
                            BLOB_close (bs);
                            }
                i++;
                    }
        END_FOR;
        if (!i)
                printf("\t\t<none>\n");
}

/*****************************************************
 *                                                   *
 *      p a r s e _ a r g s                          *
 *                                                   *
 *****************************************************/
int parse_args(argc,argv)
int argc;
char *argv[];
{
       register j;

       db_name[0] = '\0';
       single_field_name[0] = '\0';

       strcpy(prog,argv[0]);

       for (j=1;j<argc;) {
              switch (argv[j][1]) {

                     case 'd' : /* database name */
                     case 'D' :   
                                   strcpy(db_name,argv[j+1]);
/*                                 printf("The database name is %s\n",db_name);  */
				   j+=2;
                                   break;
                     case 'f' : /* input file name */
                     case 'F' :   
                                   strcpy(single_field_name,argv[j+1]);
                                   single_field=1;
                                   convert_upper(single_field_name);
/*                                 printf("The single field name is %s\n",single_field_name);  */
				   j+=2;
                                   break;
                     case 'h' : /* help switch */
                     case 'H' : 
                                   printf("\nGREF Help!\n\n");
				   syntax();
                                   exit(1);
                                   break;
                     case 'z' : /* gds version switch */
                     case 'Z' : 
				   VERSION=1;
				   j++;
                                   break;
                     default:
                                   printf("\ngref: Invalid switch!\n\n");
				   syntax();
                                   exit(1);
                                   break;
                    }
       }
       if (!db_name[0]) {
              printf ("\nPlease enter database pathname ('quit' to exit): ");
              gets (db_name);
              if (!strcmp(db_name,"quit")) 
                     exit(1);
              }
}

/**************************************
 *                                    *
 *  c o n v e r t _ u p p e r         *
 *                                    *
 **************************************/
int convert_upper(target_string)
char *target_string;
{
       register i, j, k;
       char temp_string[MAX];

       strcpy(temp_string,target_string);
       for (i=0;;i++) {
              k = temp_string[i];
              if (!k)
                     break;
              temp_string[i] = islower(k) ? toupper(k) : k ;
       }
       strcpy(target_string,temp_string);
}


/**************************************
 *                                    *
 *  s y n t a x                       *
 *                                    *
 **************************************/
syntax()
{
	printf("Syntax: %s [-d (database name)] [-f (field name)] [-h] [-z]\n",prog);
	printf("\t-d: database name\n");
	printf("\t-f: field name to xref only\n");
	printf("\t-h: print this help text\n");
	printf("\t-z: print Interbase Version information\n");
	printf("\n\n");
}
