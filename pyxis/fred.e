/*
 *	PROGRAM:	PYXIS Form Package
 *	MODULE:		fred.e
 *	DESCRIPTION:	Forms editor
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

#include <stdio.h>

#include "../jrd/common.h"
#include "../jrd/gds.h"
#include "../jrd/license.h"
#define FRED_SOURCE	1
#include "../pyxis/pyxis.h"

DATABASE
    db = STATIC "forms.gdb";

static OBJ	form_names(), relations();
static OBJ	form_menu, relation_menu, name_prompt, file_prompt,
		help_prompt, resize_form;
static OBJ	get_form(), get_relation();


extern SCHAR	*gds__alloc(), *PYXIS_get_keyname();
extern OBJ	PYXIS_relation_form(), PYXIS_create_object(),
		PYXIS_get_attribute_value(), PYXIS_menu(),
		PYXIS_relation_fields(), PYXIS_load();
extern ATT	PYXIS_find_object(), PYXIS_find_attribute();

typedef enum opt_t {
    e_none = 0,
    e_exit,
    e_help,
    e_change,
    e_create,
    e_edit,
    e_delete,
    e_automatic,
    e_replace,
    e_resize,
    e_store,
    e_write,
    e_print,
    e_trash
} OPT_T;

static TEXT	form_resize [] =
#include "../pyxis/size_form.h"
		, form_file_name [] =
#include "../pyxis/file_name.h"
		, form_form_name [] =
#include "../pyxis/form_name.h"
;



main (argc, argv)
    int		argc;
    TEXT	*argv[];
{
/**************************************
 *
 *	m a i n
 *
 **************************************
 *
 * Functional description
 *	Top level function for form editor.
 *
 **************************************/
TEXT	*filename, *inputfile, *logfile, *p, **end, sw_version;
WIN	window;

/* Setting defaults and processing switches */

gds__width = 0;
gds__height = 40;
filename = inputfile = logfile = NULL;
sw_version = FALSE;

#ifdef VMS
argc = VMS_parse (&argv, argc);
#endif

for (end = argv + argc, argv++; argv < end;)
    {
    p = *argv++;
    if (*p == '-')
	switch (UPPER (p [1]))
	    {
	    case 'H':
		gds__height = atoi (*argv++);
		break;

	    case 'W':
		gds__width = atoi (*argv++);
		break;

	    case 'Z':
		if (!sw_version)
		    printf ("fred version %s\n", GDS_VERSION);
 		sw_version = TRUE;
		break;

            case 'L':
                if (argv < end)
                    {
                    logfile = *argv++;
                    break;
                    }
                else
                    {
                    fprintf (stderr, "Please retry, supplying a log file name /n");
                    exit (FINI_ERROR);
                    }

            case 'F':
                if (argv < end)
                    {
                    inputfile = *argv++;
                    break;
                    }
                else
                    {
                    fprintf (stderr, "Please retry, supplying an input file name /n");
                    exit (FINI_ERROR);
                    }

	    default:
		fprintf (stderr, "unknown switch %s\n", p);
		exit (FINI_ERROR);
	    }
    else
	filename = p;
    }

if (!filename)
    {
    fprintf (stderr, "fred: database file name is required\n");
    exit (FINI_ERROR);
    }

/* Initialize the forms package, fire up the database, create a window
   and generally get ready to roll. */	    

READY GDS_VAL (filename) AS db;

if (sw_version)
    {
    printf ("    Version(s) for database \"%s\"\n", filename);
    gds__version (&db, NULL_PTR, NULL_PTR); 
    }

START_TRANSACTION;
CREATE_WINDOW;

PYXIS_define_forms_relation (&db);
window = (WIN) gds__window;

if ((inputfile && !PYXIS_trace_in (gds__window, inputfile)) ||
      (logfile && !PYXIS_trace_out (gds__window, logfile))) 
        fprintf (stderr, "fred: can't open inputfile/logfile\n");
else
    while (top_menu (window))
        ;

DELETE_WINDOW;
COMMIT;
FINISH;
exit (FINI_OK);
}

static center_menu (menu, offset)
    OBJ		menu;
    USHORT	offset;
{
/**************************************
 *
 *	c e n t e r _ m e n u
 *
 **************************************
 *
 * Functional description
 *	Format a nice, center, externally visible menu.
 *
 **************************************/
USHORT	x;

x = (gds__width - GET_VALUE (menu, att_width)) / 2 + offset;
PYXIS_position (menu, x, 1);
}

static create_form (window)
    WIN		window;
{
/**************************************
 *
 *	c r e a t e _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Create a new form.
 *
 **************************************/
OBJ	form, relation;

if (relation = get_relation (window))
    form = PYXIS_relation_form (window, relation);
else
    form = PYXIS_create_object (NULL_PTR, 0);

edit_form (window, form, NULL_PTR);
}

static delete_form (window)
    WIN		window;
{
/**************************************
 *
 *	d e l e t e _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Delete an existing form from the database.
 *
 **************************************/
TEXT	form_name [32];
OBJ	form;

/* Get form.  If this returns a null string, something was
   wrong with the form.  Since we want to delete it, this
   is ok.  Then delete the form */

get_form (window, form_name);

FOR X IN PYXIS$FORMS WITH X.PYXIS$FORM_NAME EQ form_name
    ERASE X;
END_FOR;

pyxis__delete (&form_menu);
form_menu = NULL;

return TRUE;
}

static edit_form (window, form, form_name)
    WIN		window;
    OBJ		form;
    TEXT	*form_name;
{
/**************************************
 *
 *	e d i t _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Edit an form.  Start with the existing forms.
 *
 **************************************/
TEXT	*p;
STATUS	status [20];
USHORT	change;

/* Eat up options until exit */

PYXIS_push_form (window, form, TRUE);
change = FALSE;

for (;;)
    CASE_MENU (HORIZONTAL) "Edit type:"
	MENU_ENTREE "EDIT":
	    change |= PYXIS_edit (window, form, 
		&db, &gds__trans);

	MENU_ENTREE "REFORMAT":
	    PYXIS_format_form (form, gds__width, gds__height);
	    change = TRUE;

	MENU_ENTREE "SIZE":
	    size_form (window, form);

	MENU_ENTREE "Exit":
	    save_form (window, form, form_name);
	    pyxis__pop_window (&window);
	    return;
    END_MENU;
}

static OBJ form_names ()
{
/**************************************
 *
 *	f o r m _ n a m e s
 *
 **************************************
 *
 * Functional description
 *	Get menu of all forms, etc.
 *
 **************************************/
OBJ	menu;

menu = PYXIS_create_object (NULL_PTR, 0);

FOR X IN PYXIS$FORMS SORTED BY X.PYXIS$FORM_NAME
    zap_string (X.PYXIS$FORM_NAME);
    PYXIS_create_entree (menu, X.PYXIS$FORM_NAME, 0, 0);
END_FOR;

return menu;
}

static OBJ get_form (window, form_buffer)
    WIN		window;
    UCHAR	*form_buffer;
{
/**************************************
 *
 *	g e t _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Get a form off the form menu.
 *
 **************************************/
OBJ	entree, form;
ATT	attribute;
STATUS	status [20];
TEXT	*form_name;

*form_buffer = 0;

if (!form_menu)
    {
    form_menu = form_names ();
    PYXIS_reference (form_menu);
    PYXIS_create_entree (form_menu, "Exit", attype_numeric, e_exit);
    PYXIS_format_menu (form_menu, "Select Form for Operation", FALSE);
    center_menu (form_menu, 0);
    }

/* Before we start, mark all forms as inactive */

for (attribute = NULL;
     attribute = PYXIS_find_object (form_menu, attribute, att_entree, TRUE);)
    if ((form = GET_OBJECT (attribute->att_value, att_entree_value)) &&
	(SLONG) form != (SLONG) e_exit)
	REPLACE_ATTRIBUTE (form, att_inactive, attype_numeric, TRUE);

PYXIS_push_form (window, form_menu, TRUE);
entree = PYXIS_menu (window, form_menu);
pyxis__pop_window (&window);

if (!entree)
    return NULL;

if ((OPT_T) GET_VALUE (entree, att_entree_value) == e_exit)
    return NULL;

form_name = GET_STRING (entree, att_literal_string);
strcpy (form_buffer, form_name);

if (form = GET_OBJECT (entree, att_entree_value))
    {
    REPLACE_ATTRIBUTE (form, att_inactive, attype_numeric, FALSE);
    return form;
    }

pyxis__load_form (
    status,
    &db, 
    &gds__trans,
    &form,
    NULL_PTR,
    form_name);

if (!form)
    return NULL;

REPLACE_ATTRIBUTE (entree, att_entree_value, attype_object, form);
REPLACE_ATTRIBUTE (form, att_inactive, attype_numeric, FALSE);

return form;
}

static OBJ get_relation (window)
    WIN		window;
{
/**************************************
 *
 *	g e t _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Pick a relation off a menu.
 *
 **************************************/
OBJ	entree, relation;
ATT	attribute;
TEXT	*relation_name;

if (!relation_menu)
    {
    relation_menu = relations ();
    PYXIS_reference (relation_menu);
    PYXIS_create_entree (relation_menu, "None of the above", attype_numeric, e_exit);
    PYXIS_format_menu (relation_menu, "Select Relation", FALSE);
    center_menu (relation_menu, 0);
    }

/* Before we start, mark all relations as inactive */

for (attribute = NULL;
     attribute = PYXIS_find_object (relation_menu, attribute, att_entree, TRUE);)
    if ((relation = GET_OBJECT (attribute->att_value, att_entree_value)) &&
	(SLONG) relation != (SLONG) e_exit)
	REPLACE_ATTRIBUTE (relation, att_inactive, attype_numeric, TRUE);

PYXIS_push_form (window, relation_menu, TRUE);
entree = PYXIS_menu (window, relation_menu);
pyxis__pop_window (&window);

if (!entree || (SLONG) (relation = GET_OBJECT (entree, att_entree_value)) == (SLONG) e_exit)
    return NULL;

/* Get the relation from either the selected entree or from the database */

if (relation)
    REPLACE_ATTRIBUTE (relation, att_inactive, attype_numeric, FALSE);
else
    {
    relation_name = GET_STRING (entree, att_literal_string);
    relation = PYXIS_relation_fields (&db,
	&gds__trans, relation_name);
    PYXIS_format_menu (relation, "Select Field(s)", FALSE);
    PYXIS_box (relation);
    PUT_ATTRIBUTE (entree, att_entree_value, attype_object, relation);
    PUT_ATTRIBUTE (relation, att_display_x, attype_numeric, strlen (relation_name));
    }

return relation;
}

static manual_load (handle, def)
    OBJ		*handle;
    TEXT	*def;
{
/**************************************
 *
 *	m a n u a l _ l o a d 
 *
 **************************************
 *
 * Functional description
 *	Manually load a form from a literal, unless it is
 *	already loaded.
 *
 **************************************/

if (!*handle)
    *handle = PYXIS_load (&def);
}

static OBJ relations ()
{
/**************************************
 *
 *	r e l a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Get menu of all relations, etc.
 *
 **************************************/
OBJ	menu, entree;

menu = PYXIS_create_object (NULL_PTR, 0);

FOR X IN RDB$RELATIONS WITH X.RDB$SYSTEM_FLAG == 0 SORTED BY X.RDB$RELATION_NAME
    zap_string (X.RDB$RELATION_NAME);
    PYXIS_create_entree (menu, X.RDB$RELATION_NAME, 0, NULL_PTR);
END_FOR;

return menu;
}

static size_form (window, form)
    WIN		window;
    OBJ		form;
{
/**************************************
 *
 *	s i z e _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Change size or outline of form.
 *
 **************************************/
USHORT	c, width, height, x, y, box;

manual_load (&resize_form, form_resize);
PYXIS_compute_size (resize_form, &x, &y);
x = (gds__width - x) / 2;
y = (gds__height - y) / 2;
PYXIS_position (resize_form, x, y);

/* Compute "current" size of form */

box = GET_VALUE (form, att_box);
width = GET_VALUE (form, att_width);
height = GET_VALUE (form, att_height);
PYXIS_compute_size (form, &x, &y);

if (!width)
    width = x + 2;
if (!height)
    height = y + 2;

FOR FORM (FORM_HANDLE resize_form TRANSPARENT) X IN SIZE_FORM
    X.WIDTH = width;
    X.HEIGHT = height;
    strcpy (X.OUTLINE_FORM, (box) ? "Y" : "N");
    DISPLAY X DISPLAYING * ACCEPTING *;
    if (X.TERMINATOR != PYXIS__KEY_ENTER)
	{
	pyxis__pop_window (&gds__window);
	return;
	}
    if (X.WIDTH.STATE == PYXIS__OPT_USER_DATA)
	REPLACE_ATTRIBUTE (form, att_width, attype_numeric, X.WIDTH);
    if (X.HEIGHT.STATE == PYXIS__OPT_USER_DATA)
	REPLACE_ATTRIBUTE (form, att_height, attype_numeric, X.HEIGHT);
    if (X.OUTLINE_FORM.STATE == PYXIS__OPT_USER_DATA)
	if (X.OUTLINE_FORM [0] == 'Y')
	    {
	    REPLACE_ATTRIBUTE (form, att_box, attype_numeric, TRUE);
	    REPLACE_ATTRIBUTE (form, att_blank, attype_numeric, TRUE);
	    REPLACE_ATTRIBUTE (form, att_border, attype_numeric, TRUE);    
	    REPLACE_ATTRIBUTE (form, att_width, attype_numeric, X.WIDTH);
	    REPLACE_ATTRIBUTE (form, att_height, attype_numeric, X.HEIGHT);
	    }	
	else
	    {
	    PYXIS_delete_named_attribute (form, att_box);
	    PYXIS_delete_named_attribute (form, att_blank);
	    PYXIS_delete_named_attribute (form, att_border);
	    }
END_FORM;
}

static save_form (window, form, form_name)
    WIN		window;
    OBJ		form;
    TEXT	*form_name;
{
/**************************************
 *
 *	s a v e _ f o r m
 *
 **************************************
 *
 * Functional description
 *	Save, rename, or junk a form.
 *
 **************************************/
OBJ	entree;
ATT	attribute;
OPT_T	option;
TEXT	*p, *file_name;

if (form_name)
    CASE_MENU (HORIZONTAL) "Retention options:"
	MENU_ENTREE "SAVE":
	    option = e_replace;
	MENU_ENTREE "RENAME":
	    option = e_store;
	MENU_ENTREE "DISCARD":
	    option = e_trash;
	MENU_ENTREE "EXTERNAL FILE":
	    option = e_write;
    END_MENU
else
    CASE_MENU (HORIZONTAL) "Retention options:"
	MENU_ENTREE "SAVE":
	    option = e_store;
	MENU_ENTREE "DISCARD":
	    option = e_trash;
	MENU_ENTREE "EXTERNAL FILE":
	    option = e_write;
    END_MENU;

switch (option)
    {
    case e_trash:
	pyxis__delete (&form);
	return;

    case e_replace:
	break;

    case e_store:
	manual_load (&name_prompt, form_form_name);
	FOR FORM (TAG FORM_HANDLE name_prompt) F IN FORM_NAME
	    for (;;)
		{
	 	DISPLAY F ACCEPTING FORM_NAME WAKING ON FORM_NAME;
		if (F.FORM_NAME.STATE != PYXIS__OPT_USER_DATA)
		    return;
		if (validate_name (F.FORM_NAME))
		    break;
		}
	    form_name = F.FORM_NAME;
	END_FORM;
	break;

    case e_write:
	if (!file_prompt)
	    {
	    p = form_file_name;
	    file_prompt = PYXIS_load (&p);
	    }
	FOR FORM (TAG FORM_HANDLE file_prompt) F IN FILE_NAME
	    DISPLAY F ACCEPTING FILE_NAME WAKING ON FILE_NAME;
	    if (F.FILE_NAME.STATE == PYXIS__OPT_USER_DATA)
		write_file (form, F.FILE_NAME);
	END_FORM;
	return;
    }

/* The form is either replacing or stored new.  In either case,
   handle it. */

PYXIS_store_form (
    &db, 
    &gds__trans,
    form_name, form);

/* If a form by that name already exists, replace it.  Otherwise rebuild
   the form menu */

if (form_menu)
    {
    for (attribute = NULL;
	 attribute = PYXIS_find_object (form_menu, attribute, att_entree, TRUE);)
	{
	entree = (OBJ) attribute->att_value;
	if ((p = GET_STRING (entree, att_literal_string)) &&
	    !strcmp (p, form_name))
	    {
	    REPLACE_ATTRIBUTE (entree, att_entree_value, attype_object, form);
	    return;
	    }
        }
    pyxis__delete (&form_menu);
    form_menu = NULL;
    }
}

static top_menu (window)
    WIN		window;
{
/**************************************
 *
 *	t o p _ m e n u
 *
 **************************************
 *
 * Functional description
 *	Main menu for forms editor.
 *
 **************************************/
OBJ	form;
TEXT	name_buffer [32];

CASE_MENU (VERTICAL) "Pick one, please"
    MENU_ENTREE "EDIT FORM":
	if (form = get_form (window, name_buffer))
	    edit_form (window, PYXIS_clone (form), name_buffer);
    
    MENU_ENTREE "CREATE FORM":
	create_form (window);

    MENU_ENTREE "DELETE FORM":
	delete_form (window);
    
    MENU_ENTREE "COMMIT":
	COMMIT;
	START_TRANSACTION;

    MENU_ENTREE "ROLLBACK":
	ROLLBACK;
	START_TRANSACTION;

    MENU_ENTREE "Exit Form Editor":
	return FALSE;

END_MENU;

return TRUE;
}

static write_file (form, file_name)
    OBJ		form;
    TEXT	*file_name;
{
/**************************************
 *
 *	w r i t e _ f i l e
 *
 **************************************
 *
 * Functional description
 *	Write a raw form image to a file.
 *
 **************************************/
FILE	*file;
TEXT	*buffer, *ptr, static_buffer [1024];
USHORT	length, l;

/* Open output file */

if (!(file = fopen (file_name, "w")))
    return FAILURE;

/* Get a buffer large enough to hold form representation */

length = PYXIS_dump_length (form);
if (length < sizeof (static_buffer))
    buffer = static_buffer;
else
    buffer = gds__alloc (length + 4);

/* Dump form to memory */

ptr = buffer;
PYXIS_dump (form, &ptr);

/* Dump form to file */

fputs ("\"", file);

for (ptr = buffer, l = 2; length; --length, l++, ptr++)
    {
    putc (*ptr, file);
    if (l == 60)
	{
	fputs ("\\\n", file);
	l = 0;
	}
    }

fputs ("\"\n", file);
fclose (file);

if (buffer != static_buffer)
    gds__free (buffer);

return 0;
}

static validate_name (string)
    TEXT	*string;
{
/**************************************
 *
 *	v a l i d a t e _ n a m e
 *
 **************************************
 *
 * Functional description
 *	Validate a file and/or form name.
 *
 **************************************/

for (; *string; string++)
    if (*string == ' ' ||
	*string == '\t')
	return FALSE;

return TRUE;
}

static zap_string (string)
    TEXT	*string;
{
/**************************************
 *
 *	z a p _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Zap trailing blanks in a string.
 *
 **************************************/

while (*string && *string != ' ')
    string++;

*string = 0;
}
