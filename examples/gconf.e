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
#include <stdio.h>

DATABASE DB = FILENAME  "atlas.gdb";

#define MAX_SMALL  256

char prog [MAX_SMALL];
char dbname [MAX_SMALL];
char template_name [MAX_SMALL];

static char *trim();

int VERSION = 0;

char *conv_type_text [] = {
                 "",
                 "Fixed Length fields",
                 "Fields Separated by single character",
                 "Fields separated by double quotes",
                 "Fields separated by double quotes & comma",
                 "Fields on separate lines (1 per field)" };

/* define macros for types of files to convert */
#define FIXED 1
#define SINGLE 2
#define QUOTES 3
#define QCOMMA 4
#define LINES 5


main (argc, argv)
    int argc;
    char *argv[];
/*******************************************
 *
 *        M A I N
 *
 *******************************************/
{
FILE *fp1;
int x, y;
int i;
char relation_name [31], pick_field [65], conv_char, temp [64];
char size[4];
int conv_type;
        
parse_args (argc, argv);
        
if ((fp1 = fopen (template_name, "w")) == NULL)
    {
    perror (prog);
    exit (1);
}

fprintf (fp1, "\n/* ----------------------------------------- */\n");
fprintf (fp1, "/* Template file for Interbase GCONV Utility */\n");
fprintf (fp1, "/* ----------------------------------------- */\n\n");

READY dbname AS DB;
START_TRANSACTION;

/* Build customer menu */
        
FOR_MENU S

    strcpy (S.TITLE_TEXT, "Pick a Relation - F1 to EXIT");
    S.TITLE_LENGTH = strlen (S.TITLE_TEXT);

    FOR X IN RDB$RELATIONS WITH X.RDB$SYSTEM_FLAG = 0 SORTED BY X.RDB$RELATION_NAME
        PUT_ITEM SY IN S
            strcpy (SY.ENTREE_TEXT, X.RDB$RELATION_NAME);
            SY.ENTREE_LENGTH = strlen (SY.ENTREE_TEXT);
            SY.ENTREE_VALUE = SY.ENTREE_LENGTH;
        END_ITEM;
    END_FOR;
        
    DISPLAY S VERTICAL;

    /* If PF1 pressed then exit this routine and return EXIT status */

    if (S.TERMINATOR == PYXIS__KEY_PF1)
        {
        fclose (fp1);
        DELETE_WINDOW;
        COMMIT;
        FINISH;
        exit (0);
        }
        
    strncpy (relation_name, S.ENTREE_TEXT, S.ENTREE_VALUE);
    relation_name[S.ENTREE_VALUE]=0;
        
END_MENU;

fprintf (fp1,"/* RELATION NAME */\n\n%s\n\n", trim (relation_name));        

/* Get conversion type   */

CASE_MENU "Choose Field Layout Type"
                                                                            
    MENU_ENTREE        "1. - Fixed Length fields                      " :
        conv_type = 1;
        
    MENU_ENTREE        "2. - Fields Separated by single character     " :
        conv_type = 2;
        
    MENU_ENTREE        "3. - Fields separated by double quotes        " :
        conv_type = 3;
        
    MENU_ENTREE        "4. - Fields separated by double quotes & comma" :
        conv_type = 4;
        
    MENU_ENTREE        "5. - Fields on separate lines (1 per field)   " :
        conv_type = 5;
        
END_MENU;

if (conv_type == SINGLE)
    {
    CASE_MENU "Choose Field Layout Type"
                                                                            
        MENU_ENTREE        "  - SPACE     " :
            conv_char = ' ';
        
        MENU_ENTREE        ", - COMMA     " :
            conv_char = ',';

        MENU_ENTREE        "# - POUND SIGN" :
            conv_char = '#';

        MENU_ENTREE        "~ - TILDE     " :
            conv_char = '~';

        MENU_ENTREE        ": - COLON     " :
            conv_char = ':';

        MENU_ENTREE        "; - SEMICOLON " :
            conv_char = ';';

    END_MENU;
        
    fprintf (fp1, "/* CONVERSION TYPE */\n\n%1d,'%c'\t/* %s */\n\n",
                        conv_type, conv_char, conv_type_text[conv_type]);        
    }
else
    fprintf (fp1, "/* CONVERSION TYPE */\n\n%1d\t/* %s */\n\n",
                        conv_type, conv_type_text[conv_type]);        

/* Get Fields  */

fprintf (fp1,"/* FIELDS */\n\n");

while (1)
    {
    FOR_MENU S
        strcpy (S.TITLE_TEXT, "Pick a Field - F1 or QUIT to Quit Selection");
        S.TITLE_LENGTH = strlen (S.TITLE_TEXT);

        PUT_ITEM SY IN S
            strcpy (SY.ENTREE_TEXT, "<QUIT>");
            SY.ENTREE_LENGTH = strlen (SY.ENTREE_TEXT);
            SY.ENTREE_VALUE = 0;
        END_ITEM;

        PUT_ITEM SY IN S
            strcpy (SY.ENTREE_TEXT, "[ALL]");
            SY.ENTREE_LENGTH = strlen (SY.ENTREE_TEXT);
            SY.ENTREE_VALUE = -1;
        END_ITEM;

        FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
          RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
          RF.RDB$SYSTEM_FLAG = 0 AND
          RF.RDB$RELATION_NAME = relation_name
          SORTED BY RF.RDB$FIELD_NAME
          REDUCED TO RF.RDB$FIELD_NAME
        
            PUT_ITEM SY IN S
                strcpy (SY.ENTREE_TEXT, RF.RDB$FIELD_NAME);
                SY.ENTREE_LENGTH = strlen (SY.ENTREE_TEXT);
                SY.ENTREE_VALUE = SY.ENTREE_LENGTH;
            END_ITEM;

        END_FOR;
        
        DISPLAY S VERTICAL;

        /* If PF1 pressed then exit this routine and return EXIT status */

        if ( S.TERMINATOR == PYXIS__KEY_PF1 || !S.ENTREE_VALUE )
            {
            fclose (fp1);  
            break;
            }

        if ( S.ENTREE_VALUE < 0 ) {
            FOR RF IN RDB$RELATION_FIELDS CROSS R IN RDB$FIELDS WITH 
              RF.RDB$FIELD_SOURCE = R.RDB$FIELD_NAME AND
              RF.RDB$SYSTEM_FLAG = 0 AND
              RF.RDB$RELATION_NAME = relation_name
              SORTED BY RF.RDB$FIELD_NAME
              REDUCED TO RF.RDB$FIELD_NAME
            {
                fprintf (fp1, "%s", trim (RF.RDB$FIELD_NAME));
                if ( conv_type == FIXED )
                    fprintf (fp1, ",%d", R.RDB$FIELD_LENGTH);
                fprintf ( fp1, "\n" );
            }
            END_FOR;
            fclose (fp1);  
            break;
        }
                
        strncpy (pick_field, S.ENTREE_TEXT, S.ENTREE_VALUE);
        pick_field[S.ENTREE_VALUE] = 0;

        if (conv_type == FIXED)
            {
            FOR_MENU Z
                strcpy (Z.TITLE_TEXT, "Choose Fixed Length for Field");
                Z.TITLE_LENGTH = strlen (Z.TITLE_TEXT);

                for (i = 1; i < 256; i++)
                    {
                    PUT_ITEM ZY IN Z
                        sprintf (ZY.ENTREE_TEXT, "%d", i);
                        ZY.ENTREE_LENGTH = strlen (ZY.ENTREE_TEXT);
                        ZY.ENTREE_VALUE = ZY.ENTREE_LENGTH;
                    END_ITEM;
                    }

                DISPLAY Z VERTICAL;

                strncpy (size, Z.ENTREE_TEXT, Z.ENTREE_VALUE);
                size[Z.ENTREE_VALUE] = 0;
            END_MENU;

            fprintf (fp1, "%s,%s\n", trim (pick_field), size);
            }
        else
            fprintf (fp1, "%s\n", trim (pick_field));

    END_MENU;
    }
            
DELETE_WINDOW;

COMMIT;

FINISH;
}

int parse_args (argc, argv)
/*****************************************************
 *
 *        p a r s e _ a r g s
 *
 *****************************************************/
    int argc;
    char *argv[];
{
register j;

dbname[0] = '\0';
template_name[0] = '\0';

strcpy (prog, argv[0]);

for (j = 1; j < argc;)
    {
    switch (argv[j][1])
        {

        case 'd' : /* database name */
        case 'D' :   
            strcpy (dbname, argv[j+1]);
            j += 2;
            break;
        case 'h' : /* help switch */
        case 'H' :  
            printf ("\nGCONF Help!\n\n");
            syntax ();
            exit (1);
            break;
        case 't' : /* template name */
        case 'T' :  
            strcpy (template_name, argv[j+1]);
            j += 2;
            break;
        case 'z' : /* gds version */
        case 'Z' :  
            VERSION = 1;
            j++;
            break;
        default:
            printf ("\ngconf: Invalid switch!\n\n");
            syntax ();
            exit (1);
            break;
        }
    }
if (!dbname[0])
    {
    printf ("\nPlease enter db pathname ('quit' to exit): ");
    gets (dbname);
    if (!strcmp (dbname, "quit")) 
        exit (1);
    }
if (!template_name[0])
    {
    printf ("\nPlease enter template pathname ('quit' to exit): "); 
    gets (template_name);
    if (!strcmp (template_name, "quit")) 
        exit (1);
    }
}

syntax ()
/**************************************
 *
 *        s y n t a x
 *
 **************************************/
{
printf ("Syntax: %s [-d (database name)] [-t (template name)] [-z] [-h]\n",prog);
printf ("\t-d: database name\n");
printf ("\t-h: print this help text\n");
printf ("\t-t: conversion template file name\n");
printf ("\t-z: print Interbase Version information\n");
printf ("\n\n");
}

static char *trim (temp_string)
    char *temp_string;
/*****************************************************
 *                                                   *
 *      t r i m                                      *
 *                                                   *
 *****************************************************/
{
char temp_string2 [255];
int j;

strcpy (temp_string2, temp_string);
for (j = 0; j < strlen (temp_string2); j++)
    {
    if (temp_string2[j] == ' ')
        {
        temp_string2[j] = NULL;
        break;
        }
    }
return (temp_string2);
}
