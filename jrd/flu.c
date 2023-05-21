/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		flu.c
 *	DESCRIPTION:	Function Lookup Code
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

#include "../jrd/common.h"
#include "../jrd/flu.h"
#include "../jrd/gdsassert.h"
#ifdef SHLIB_DEFS
#define FUNCTIONS_entrypoint	(*_libgds_FUNCTIONS_entrypoint)
#endif
#include "../jrd/flu_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/dls_proto.h"
#include <string.h>

/* VMS Specific Stuff */

#ifdef VMS

#include <descrip.h>
#include <ssdef.h>

static int	condition_handler (int *, int *, int *);

#endif

/* HP-UX specific stuff */

#if (defined HP700 || defined HP10)
#include <dl.h>
#include <shl.h>
#include <unistd.h>
#include <libgen.h>
#define IB_UDF_DIR 	"UDF/"
#endif

#ifdef HM300
#define _A_OUT_INCLUDED
#include <dl.h>
#include <shl.h>
#endif

/* SGI, EPSON, UNIXWARE, M88K, DECOSF specific stuff */

#if (defined SOLARIS || defined sgi || defined EPSON || defined M88K || defined UNIXWARE || defined NCR3000 || defined DECOSF || defined SCO_EV || defined linux)
#include <dlfcn.h>
#define DYNAMIC_SHARED_LIBRARIES
#include <unistd.h>
#include <libgen.h>
#define IB_UDF_DIR              "UDF/"
#endif

/* DG specific stuff */

#ifdef DGUX
#include <sys/stat.h>
#include <sys/types.h>

#define NON_DL_COMPATIBLE

#ifndef NON_DL_COMPATIBLE
#include <dlfcn.h>
#endif
#endif

/* MS/DOS / OS2 Junk */

#ifdef OS2_ONLY
#define INCL_DOSMODULEMGR
#include <os2.h>
#endif

/* Apollo specific Stuff */

#ifdef APOLLO
std_$call FPTR_INT	kg_$lookup();
#include <apollo/base.h>
#include <apollo/task.h>
#include <apollo/loader.h>
#endif

/* Windows NT stuff */

#ifdef WIN_NT
#include <windows.h>
#include <stdlib.h>
#include <io.h>
#define IB_UDF_DIR		"UDF\\"
/* Where is the international character set module found? */
#define IB_INTL_DIR		"intl\\"
#endif

/* Windows 3.1 stuff */

#ifdef WINDOWS_ONLY
#include <windows.h>
#include <dir.h>
#include "../jrd/gds_proto.h"

typedef struct {
    TEXT	module_name [16];
    HINSTANCE	handle;
} MODULE_CACHE;

#define MODULE_CACHE_SIZE	12

static MODULE_CACHE     load_cache [MODULE_CACHE_SIZE + 1] = {0, NULL};

HARBOR_MERGE
extern HINSTANCE hDLLInstance;
static void	cleanup (void *);
static void     cleanup_cache (void *);
#endif

/* HP MPE/XL specific stuff */

#ifdef mpexl
#include "../jrd/mpexl.h"
#endif

/* Where is the international character set module found? */
#ifndef IB_INTL_DIR
#define IB_INTL_DIR		"intl/"
#endif

static MOD	FLU_modules = NULL_PTR;	/* External function/filter modules */

/* prototypes for private functions */
static MOD search_for_module ( TEXT *, TEXT * );
#ifdef WIN_NT
static void adjust_loadlib_name ( TEXT *, TEXT * );
#endif


MOD FLU_lookup_module (
    TEXT	*module)
{
/**************************************
 *
 *	F L U _ l o o k u p _ m o d u l e
 *
 **************************************
 *
 * Functional description
 *	Lookup external function module descriptor.
 *
 **************************************/
MOD	mod;
USHORT	length;
#ifndef WIN_NT
TEXT	*p;

for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;
#endif

length = strlen (module);

for (mod = FLU_modules; mod; mod = mod->mod_next)
    if (mod->mod_length == length && !strcmp (mod->mod_name, module))
	return mod;

return NULL_PTR;
}

void FLU_unregister_module (
    MOD		module)
{
/**************************************
 *
 *	F L U _ u n r e g i s t e r _ m o d u l e
 *
 **************************************
 *
 * Functional description
 *	Unregister interest in an external function module.
 *	Unload module if it's uninteresting.
 *
 **************************************/
MOD	*mod;

/* Module is in-use by other databases.*/

if (--module->mod_use_count > 0)
    return;

/* Unlink from list of active modules, unload it, and release memory. */

for (mod = &FLU_modules; *mod; mod = &(*mod)->mod_next)
    if (*mod == module)
	{
	*mod = module->mod_next;
	break;
	}

#if (defined HP700 || defined HP10)
shl_unload (module->mod_handle);
#endif

#if defined(SOLARIS) || defined(LINUX)
dlclose (module->mod_handle);
#endif
#ifdef WIN_NT
FreeLibrary (module->mod_handle);
#endif

gds__free (module);
}

#ifdef APOLLO
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( A P O L L O - S R 1 0 )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
SLONG		global_type;
SSHORT		len;
loader_$kg_lookup_opts	lookup_opts;

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

REPLACE THIS COMPILER ERROR WITH A CALL TO GDS__VALIDATE_EXT_LIB_PATH TO
VALIDATE THE PATH OF THE MODULE, AND/OR CODE TO VERIFY THAT THE MODULE IS
FOUND EITHER IN $INTERBASE/UDF OR $INTERBASE/intl OR IN ONE OF THE
DIRECTORIES NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

len = strlen (name);
global_type = loader_$kg_symbol;
lookup_opts = loader_$kg_force_load | loader_$kg_function_only;
loader_$kg_lookup (name, len, global_type, lookup_opts, &function);

return (FPTR_INT) function;
}
#endif

#ifdef OS2_ONLY
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( O S 2 )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
HMODULE	module_handle;
TEXT	buffer [64];
PFN 	*function;

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

/* Start by looking up or loading module */

REPLACE THIS COMPILER ERROR WITH A CALL TO GDS__VALIDATE_EXT_LIB_PATH TO
VALIDATE THE PATH OF THE MODULE AND/OR CODE TO VERIFY THAT THE MODULE IS
FOUND EITHER IN $INTERBASE/UDF, $INTERBASE/intl, OR IN ONE OF THE
DIRECTORIES NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

if (DosLoadModule (buffer, sizeof (buffer), module, &module_handle))
    return (FPTR_INT) NULL;

/* Lookup function */

if (DosQueryProcAddr (module_handle, 0, name, &function))
    return (FPTR_INT) NULL;

return (FPTR_INT) function;
}
#endif

#ifdef VMS
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( V M S )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
struct dsc$descriptor	mod_desc, nam_desc;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

if (ib_path_env_var == NULL) 
    strcpy (absolute_module, module);
else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
    return NULL;

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS FOUND
EITHER IN $INTERBASE:UDF, or $INTERBASE:intl, OR IN ONE OF THE DIRECTORIES
NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

for (p = absolute_module; *p && *p != ' '; p++)
    ;

ISC_make_desc (absolute_module, &mod_desc, p - absolute_module);

for (p = name; *p && *p != ' '; p++)
    ;

ISC_make_desc (name, &nam_desc, p - name);
VAXC$ESTABLISH (condition_handler);

if (!(lib$find_image_symbol (&mod_desc, &nam_desc, &function, NULL) & 1))
    return NULL;

return function;
}
#endif

#if (defined AIX || defined AIX_PPC)
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( A I X )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;

if (ib_path_env_var == NULL) 
    strcpy (absolute_module, module);
else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
    return NULL;

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS FOUND
EITHER IN $INTERBASE/UDF, OR $INTERBASE/intl, OR IN ONE OF THE DIRECTORIES
NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

function = load (absolute_module, 0, NULL);

return function;
}
#endif

#if (defined HP700 || defined HP10)
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( H P 9 0 0 0 s 7 0 0 )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
MOD		mod;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;
for (p = name; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;

if (!*module || !*name)
    return NULL;

/* Check if external function module has already been loaded */

if (!(mod = FLU_lookup_module (module)))
    {
    USHORT length ;

#ifdef EXT_LIB_PATH
    if (ib_path_env_var == NULL) 
	strcpy (absolute_module, module);
    else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
	return NULL;
#else
    strcpy (absolute_module, module);
#endif /* EXT_LIB_PATH */

    length = strlen (absolute_module);

    /* call search_for_module with the supplied name, 
       and if unsuccessful, then with <name>.sl . */
    mod = search_for_module (absolute_module, name);
    if (!mod)
        {
	strcat (absolute_module, ".sl");
	mod = search_for_module (absolute_module, name);
        }
    if (!mod) return NULL;
    
    assert (mod->mod_handle);	/* assert that we found the module */
    mod->mod_use_count = 0;
    mod->mod_length = length;
    strcpy (mod->mod_name, module);
    mod->mod_next = FLU_modules;
    FLU_modules = mod;
    }

++mod->mod_use_count;

if (shl_findsym (&mod->mod_handle, name, TYPE_PROCEDURE, &function))
    return NULL;

return function;
}

static MOD search_for_module ( TEXT *module,
			       TEXT *name )
{
/**************************************
 *
 *	s e a r c h _ f o r _ m o d u l e	( H P 9 0 0 0 s 7 0 0 )
 *
 **************************************
 *
 * Functional description
 *	Look for a module (as named in a 'DECLARE EXTERNAL FUNCTION'
 *      statement.
 *
 **************************************/
MOD		mod;
char           *dirp;
TEXT		ib_lib_path[MAXPATHLEN];
TEXT		absolute_module[MAXPATHLEN];
FDLS            *dir_list;
BOOLEAN         found_module;

strcpy (absolute_module, module);

if (!(mod = (MOD) gds__alloc (sizeof (struct mod) +
			      strlen (absolute_module))))
  return NULL;

dirp = dirname (absolute_module);
if (('.' == dirp[0]) && ('\0' == dirp[1]))
  {
    /*  We have a simple module name, without a directory. */
    
    gds__prefix (ib_lib_path, IB_UDF_DIR);
    strncat (ib_lib_path, module,
	     MAXPATHLEN - strlen (ib_lib_path) - 1);
    if (!access (ib_lib_path, R_OK | X_OK))
      {
	/* Module is in the default UDF directory: load it. */
	if (!(mod->mod_handle = shl_load (ib_lib_path, BIND_DEFERRED, 0)))
	  {
	    gds__free (mod);
	    return NULL;
	  }
      }
    else
      {
	gds__prefix (ib_lib_path, IB_INTL_DIR);
	strncat (ib_lib_path, module,
		 MAXPATHLEN - strlen (ib_lib_path) - 1);
	if (!access (ib_lib_path, R_OK | X_OK))
	  {
	    /* Module is in the international character set directory:
	       load it. */
	    if (!(mod->mod_handle = shl_load (ib_lib_path,
					      BIND_DEFERRED, 0)))
	      {
		gds__free (mod);
		return NULL;
	      }
	  }
	else
	  {
	    /*  The module is not in a default directory, so ...
	     *  use the EXTERNAL_FUNCTION_DIRECTORY lines from isc_config.
	     */
	    dir_list = DLS_get_func_dirs();
	    found_module = FALSE;
	    while (dir_list && !found_module)
	      {
		strcpy (ib_lib_path, dir_list->fdls_directory);
		strcat (ib_lib_path, "/");
		strncat (ib_lib_path, module,
			 MAXPATHLEN - strlen (ib_lib_path) - 1);
		if (!access (ib_lib_path, R_OK | X_OK))
		  {
		    if (!(mod->mod_handle = shl_load (ib_lib_path,
						      BIND_DEFERRED, 0)))
		      {
			gds__free (mod);
			return NULL;
		      }
		    found_module = TRUE;
		  }
		dir_list = dir_list->fdls_next;
	      }
	    if (!found_module)
	      {
		gds__free (mod);
		return NULL;
	      }
	  }	/* else module is not in the INTL directory */
      } /* else module is not in the default UDF directory, so ... */
  } /* if dirname is "." */
else
  {
    /*  The module name includes a directory path.
     *  The directory must be the standard UDF directory, or the
     *  the standard international directory, or listed in
     *  an EXTERNAL_FUNCTION_DIRECTORY line in isc_config,
     *  and the module must be accessible in that directory.
     */
    gds__prefix (ib_lib_path, IB_UDF_DIR);
    ib_lib_path [strlen(ib_lib_path) - 1] = '\0'; /* drop trailing "/" */
    found_module = ! strcmp (ib_lib_path, dirp);
    if (!found_module)
      {
	gds__prefix (ib_lib_path, IB_INTL_DIR);
	ib_lib_path [strlen(ib_lib_path) - 1] = '\0'; /* drop trailing / */
	found_module = ! strcmp (ib_lib_path, dirp);
      }
    if (!found_module)
      {
	/* It's not the default directory, so try the ones listed
	 * in EXTERNAL_FUNCTION_DIRECTORY lines in isc_config.
	 */
	dir_list = DLS_get_func_dirs();
	while (dir_list && !found_module)
	  {
	    if (! strcmp (dir_list->fdls_directory, dirp))
	      found_module = TRUE;
	    dir_list = dir_list->fdls_next;
	  }
      }
    if (found_module)
      found_module =  (!access (module, R_OK | X_OK)) &&
	(0 != (mod->mod_handle = shl_load (module, BIND_DEFERRED, 0)));
    if (!found_module)
      {
	gds__free (mod);
	return NULL;
      }
  } /* else module name includes a directory path, so ... */
return mod;
}
#endif

#ifdef HM300
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( H P 9 0 0 0 s 3 0 0 )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
shl_t		handle;
TEXT		buffer [256];
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;
p = buffer;
*p++ = '_';
for (; (*p = *name) && *name != ' '; p++, name++)
    ;
*p = 0;

if (ib_path_env_var == NULL) 
    strcpy (absolute_module, module);
else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
    return NULL;

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS FOUND
EITHER IN $INTERBASE/UDF, OR $INTERBASE/intl, OR IN ONE OF THE DIRECTORIES
NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

if (!(handle = shl_load (absolute_module, BIND_DEFERRED, 0L)) ||
    shl_findsym (&handle, buffer, TYPE_PROCEDURE, &function))
    return NULL;

return function;
}
#endif

#ifdef DYNAMIC_SHARED_LIBRARIES
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( SVR4 including SOLARIS )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
MOD		mod;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

#ifdef NON_DL_COMPATIBLE
return NULL;
#else
for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;
for (p = name; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;

if (!*module || !*name)
    return NULL;

/* Check if external function module has already been loaded */

if (!(mod = FLU_lookup_module (module)))
    {
    USHORT length ;

#ifdef EXT_LIB_PATH
    if (ib_path_env_var == NULL) 
	strcpy (absolute_module, module);
    else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
	return NULL;
#else
    strcpy (absolute_module, module);
#endif /* EXT_LIB_PATH */

    length = strlen (absolute_module);
    
    /* call search_for_module with the supplied name, 
       and if unsuccessful, then with <name>.so . */
    mod = search_for_module (absolute_module, name);
    if (!mod)
        {
	strcat (absolute_module, ".so");
	mod = search_for_module (absolute_module, name);
        }
    if (!mod) return NULL;
    
    assert (mod->mod_handle);	/* assert that we found the module */
    mod->mod_use_count = 0;
    mod->mod_length = length;
    strcpy (mod->mod_name, module);
    mod->mod_next = FLU_modules;
    FLU_modules = mod;
    }

++mod->mod_use_count;

return dlsym (mod->mod_handle, name);
#endif /* NON_DL_COMPATIBLE */
}

static MOD search_for_module ( TEXT *module,
			       TEXT *name )
{
/**************************************
 *
 *	s e a r c h _ f o r _ m o d u l e	( SVR4 including SOLARIS )
 *
 **************************************
 *
 * Functional description
 *	Look for a module (as named in a 'DECLARE EXTERNAL FUNCTION'
 *      statement.
 *
 **************************************/
MOD		mod;
char           *dirp;
TEXT		ib_lib_path[MAXPATHLEN];
TEXT		absolute_module[MAXPATHLEN];  /* for _access()  ???   */
FDLS            *dir_list;
BOOLEAN         found_module;

strcpy (absolute_module, module);

if (!(mod = (MOD) gds__alloc (sizeof (struct mod) +
			      strlen (absolute_module))))
    return NULL;
dirp = dirname (absolute_module);
if (('.' == dirp[0]) && ('\0' == dirp[1]))
    {
    /*  We have a simple module name without a directory. */
    
    gds__prefix (ib_lib_path, IB_UDF_DIR);
    strncat (ib_lib_path, module,
	     MAXPATHLEN - strlen (ib_lib_path) - 1);
    if (!access (ib_lib_path, R_OK))
        {
	/* Module is in the standard UDF directory: load it. */
	if (!(mod->mod_handle = dlopen (ib_lib_path, RTLD_LAZY)))
	    {
	    gds__free (mod);
	    return NULL;
	    }
	}
    else
        {
	gds__prefix (ib_lib_path, IB_INTL_DIR);
	strncat (ib_lib_path, module,
		 MAXPATHLEN - strlen (ib_lib_path) - 1);
	if (!access (ib_lib_path, R_OK))
	    {
	    /* Module is in the default directory: load it. */
	    if (!(mod->mod_handle = dlopen (ib_lib_path, RTLD_LAZY)))
	        {
		gds__free (mod);
		return NULL;
		}
	    }
	else
	    {
	    /*  The module is not in the default directory, so ...
	     *  use the EXTERNAL_FUNCTION_DIRECTORY lines from isc_config.
	     */
	    dir_list = DLS_get_func_dirs();
	    found_module = FALSE;
	    while (dir_list && !found_module)
	        {
		strcpy (ib_lib_path, dir_list->fdls_directory);
		strcat (ib_lib_path, "/");
		strncat (ib_lib_path, module,
			 MAXPATHLEN - strlen (ib_lib_path) - 1);
		if (!access (ib_lib_path, R_OK))
		    {
		    if (!(mod->mod_handle = dlopen (ib_lib_path,
						    RTLD_LAZY)))
		        {
			gds__free (mod);
			return NULL;
			}
		    found_module = TRUE;
		    }
		dir_list = dir_list->fdls_next;
		}
	    if (!found_module)
	        {
		gds__free (mod);
		return NULL;
		}
	    } /* else module is not in the INTL directory */
	} /* else module is not in the default directory, so ... */
    } /* if *dirp is "." */
else
    {
    /*  The module name includes a directory path.
     *  The directory must be the standard UDF directory, or the
     *  standard international directory, or listed in
     *  an EXTERNAL_FUNCTION_DIRECTORY line in isc_config,
     *  and the module must be accessible in that directory.
     */
    gds__prefix (ib_lib_path, IB_UDF_DIR);
    ib_lib_path [strlen(ib_lib_path) - 1] = '\0'; /* drop trailing "/" */
    found_module = ! strcmp (ib_lib_path, dirp);
    if (!found_module)
        {
	gds__prefix (ib_lib_path, IB_INTL_DIR);
	ib_lib_path [strlen(ib_lib_path) - 1] = '\0'; /* drop trailing / */
	found_module = ! strcmp (ib_lib_path, dirp);
	}
    if (!found_module)
        {
	/* It's not the default directory, so try the ones listed
	 * in EXTERNAL_FUNCTION_DIRECTORY lines in isc_config.
	 */
	dir_list = DLS_get_func_dirs();
	while (dir_list && !found_module)
	    {
	    if (! strcmp (dir_list->fdls_directory, dirp))
	      found_module = TRUE;
	    dir_list = dir_list->fdls_next;
	    }
	}
    if (found_module)
        found_module = (!access (module, R_OK)) &&
	  (0 !=  (mod->mod_handle = dlopen (module, RTLD_LAZY)));
    if (!found_module)
        {
	gds__free (mod);
	return NULL;
        }
    } /* else module name includes a directory path, so ... */
return mod;
}
#endif

#ifdef DGUX
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( D G U X )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
void		*handle;
struct stat	stdbuf;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

#ifdef NON_DL_COMPATIBLE
return NULL;
#else
for (p = module; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;
for (p = name; *p && *p != ' '; p++)
    ;
if (*p)
    *p = 0;

if (!*module ||
    stat (module, &stdbuf))
    return NULL;

if (ib_path_env_var == NULL) 
    strcpy (absolute_module, module);
else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
    return NULL;

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS FOUND
EITHER IN $INTERBASE/UDF, OR $INTERBASE/intl, OR IN ONE OF THE DIRECTORIES
NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

if (!(handle = dlopen (absolute_module, RTLD_LAZY)))
    return NULL;

return dlsym (handle, name);
#endif
}
#endif

#ifdef WIN_NT
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *	If ib_path_env_var is defined, make sure 
 *	that the module is in the path defined
 *	by that environment variable.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p;
MOD		mod;
TEXT		absolute_module[MAXPATHLEN];  /* for _access()     */
TEXT		ib_lib_path[MAXPATHLEN];

 if (function = FUNCTIONS_entrypoint (module, name))
     return function;

 for (p = name; *p && *p != ' '; p++)
     ;
 if (*p)
     *p = 0;

 /* Check if external function module has already been loaded */

 if (!(mod = FLU_lookup_module (module)))
     {
     USHORT length ;

#ifdef EXT_LIB_PATH
     if (ib_path_env_var == NULL) 
         strcpy (absolute_module, module);
     else if (!gds__validate_lib_path (module, ib_path_env_var, absolute_module, sizeof(absolute_module)))
         return NULL;
#else
     strcpy (absolute_module, module);
#endif /* EXT_LIB_PATH */

     length = strlen (absolute_module);

     /* call search_for_module with the supplied name, and if unsuccessful,
	then with <name>.DLL (if the name did not have a trailing "."). */
     mod = search_for_module (absolute_module, name);
     if (!mod)
         {
	 if ( (absolute_module [length -1] != '.') &&
	      stricmp(absolute_module+length-4, ".DLL") )
	     {
	     strcat (absolute_module, ".DLL");
	     mod = search_for_module (absolute_module, name);
	     }
	 }
     if (!mod) return NULL;
     
     assert (mod->mod_handle);	/* assert that we found the module */
     mod->mod_use_count = 0;
     mod->mod_length = length;
     strcpy (mod->mod_name, module);
     mod->mod_next = FLU_modules;
     FLU_modules = mod;
     }

 ++mod->mod_use_count;

 /* The Borland compiler prefixes an '_' for functions compiled
    with the __cdecl calling convention. */

 if (!(function = (FPTR_INT) GetProcAddress (mod->mod_handle, name)))
     {
     TEXT	buffer [128];
     
     p = buffer;
     *p++ = '_';
     while (*p++ = *name++)
         ;
     function = (FPTR_INT) GetProcAddress (mod->mod_handle, buffer);
     }

 return function;
}

static MOD search_for_module ( TEXT *module,
			       TEXT *name )
{
/**************************************
 *
 *	s e a r c h _ f o r _ m o d u l e	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Look for a module (as named in a 'DECLARE EXTERNAL FUNCTION'
 *      statement.  Adjust for differences between behavior of
 *      _access() and LoadLibrary() w.r.t. implicit .DLL suffix.
 *
 **************************************/
MOD		mod;
/*  We need different versions of the file name to pass to _access()
 *  and to LoadLibrary(), because LoadLibrary() implicitly appends
 *  .DLL to the supplied name and access does not.  We need to control
 *  exactly what name is being used by each routine.
 */
TEXT		ib_lib_path[MAXPATHLEN];      /* for access()      */
TEXT		absolute_module[MAXPATHLEN];  /* for _access()  ???   */
TEXT            loadlib_name[MAXPATHLEN];     /* for LoadLibrary() */
TEXT            drive_buf[_MAX_DRIVE];
TEXT            dir_buf[_MAX_DIR];
FDLS            *dir_list;
USHORT          length;
BOOLEAN         found_module;

 length = strlen (module);
 
 if (!(mod = (MOD) gds__alloc (sizeof (struct mod) + length)))
   return NULL;
 
 /* Set absolute_module to the drive and directory prefix part of
    module, like dirname() under Unix. */
 _splitpath (module, drive_buf, dir_buf, (char *)0, (char *)0);
 strcpy (absolute_module, drive_buf);
 strcat (absolute_module, dir_buf);
 
 if (0 == absolute_module[0])
     {
     /*  We have a simple module name without a directory. */
     
     gds__prefix (ib_lib_path, IB_UDF_DIR);
     strncat (ib_lib_path, module,
	      MAXPATHLEN - strlen (ib_lib_path) - 1);
     adjust_loadlib_name (ib_lib_path, loadlib_name);
     if (!_access (ib_lib_path, 4 /* read permission */))
         {
	 /* Module is in the default UDF directory: load it. */
	 if (!(mod->mod_handle = LoadLibrary (loadlib_name)))
	     { 
	     gds__free (mod);
	     return NULL;
	     }
	 }
     else
         {
	 gds__prefix (ib_lib_path, IB_INTL_DIR);
	 strncat (ib_lib_path, module,
		  MAXPATHLEN - strlen (ib_lib_path) - 1);
	 if (!_access (ib_lib_path, 4 /* read permission */))
	     {
	     /* Module is in the default international library: load it */
	     if (!(mod->mod_handle = LoadLibrary (ib_lib_path)))
	         { 
		 gds__free (mod);
		 return NULL;
	         }
	     }
	 else
	     {
	     /*  The module is not in the default directory, so ...
	      *  use the EXTERNAL_FUNCTION_DIRECTORY lines from isc_config.
	      */
	     dir_list = DLS_get_func_dirs();
	     found_module = FALSE;
	     while (dir_list && !found_module)
	         {
		 strcpy (ib_lib_path, dir_list->fdls_directory);
		 strcat (ib_lib_path, "\\");
		 strncat (ib_lib_path, module,
			  MAXPATHLEN - strlen (ib_lib_path) - 1);
		 if (!_access (ib_lib_path, 4 /* read permission */ ))
		     {
		     if (!(mod->mod_handle = LoadLibrary (ib_lib_path)))
		         { 
			 gds__free (mod);
			 return NULL;
			 }
		     found_module = TRUE;
		     }
		 dir_list = dir_list->fdls_next;
		 }
	     if (!found_module)
	         {
		 gds__free (mod);
		 return NULL;
		 }
	     } /* else module is not in the INTL directory */
	 } /* else module is not in the default UDF directory, so ... */
     } /* if path part of name is empty */
 else
     {
     /*  The module name includes a directory path.
      *  The directory must be either $INTERBASE/UDF, or $INTERBASE/intl
      *  or listed in an EXTERNAL_FUNCTION_DIRECTORY line in isc_config,
      *  and the module must be accessible in that directory.
      */
     gds__prefix (ib_lib_path, IB_UDF_DIR);
     found_module = ! stricmp (ib_lib_path, absolute_module);
     if (!found_module)
         {
	 gds__prefix (ib_lib_path, IB_INTL_DIR);
	 found_module = ! stricmp (ib_lib_path, absolute_module);
	 }
     if (!found_module)
         {
	 /* It's not the default directory, so try the ones listed
	  * in EXTERNAL_FUNCTION_DIRECTORY lines in isc_config.
	  */
	 dir_list = DLS_get_func_dirs();
	 while (dir_list && !found_module)
	     {
	     if (! stricmp (dir_list->fdls_directory, absolute_module))
	       found_module = TRUE;
	     dir_list = dir_list->fdls_next;
	     }
	 }
     if (found_module)
       found_module = (!_access (module, 4 /* read ok? */)) &&
	 (0 !=  (mod->mod_handle = LoadLibrary (module)));
     if (!found_module)
         {
	 gds__free (mod);
	 return NULL;
	 }
     } /* else module name includes a directory path, so ... */
 return mod;
}

static void adjust_loadlib_name ( TEXT *access_path,
				  TEXT *load_path )
{
/**************************************
 *
 *	a d j u s t _ l o a d l i b _ n a m e	( W I N _ N T )
 *
 **************************************
 *
 * Functional description
 *	Look for a module (as named in a 'DECLARE EXTERNAL FUNCTION'
 *      statement.  Adjust for differences between behavior of
 *      _access() and LoadLibrary() w.r.t. implicit .DLL suffix.
 *
 **************************************/
  
  USHORT  length;
  
  length = strlen ( access_path );
  strcpy ( load_path, access_path );

  /*  Adjust the names to be passed to _access() and LoadLibrary(),
   *  so that they will refer to the same file, despite the implicit
   *  adjustments performed by LoadLibrary.
   *  Input     _access() arg   LoadLibrary() arg  LoadLibrary() uses...
   *  -------   -------------   -----------------  ------------------
   *  foo.      foo             foo.               foo
   *  foo       foo             foo.               foo
   *  foo.dll   foo.dll         foo.dll            foo.dll
   */
  if (stricmp (access_path+length-4, ".dll"))
      {
      if ('.' == access_path [ length - 1 ])
	  {
	  access_path [ length - 1 ]= '\0';
	  }
      else
	  {
	  load_path [ length     ] = '.';
	  load_path [ length + 1 ] = '\0';
	  }
      }
}
#endif

#ifdef WINDOWS_ONLY
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t	( W I N D O W S _ O N L Y )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;
TEXT		*p, *q, buffer [256];
HINSTANCE	handle = NULL;
UINT		fuErrorMode;
int		i;
TEXT		absolute_module[MAXPATHLEN];

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

/* Remove any trailing whitespace from the module name */
for (p = module, q = buffer; (*q = *p) && *p++ != ' '; q++)
    ;
*q = 0;


/* Try to load the DLL, first checking our cache in case it's loaded */
if (buffer != NULL && *buffer)
    {
    for (i = 0; *(load_cache[i].module_name) && handle == NULL; i++)
    	{
    	if (!strcmp (load_cache[i].module_name, buffer))
	    handle = load_cache[i].handle;
    	}

    /* If it isn't in the cache, try loading it */

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS FOUND
EITHER IN $INTERBASE/UDF, OR $INTERBASE/intl, OR IN ONE OF THE DIRECTORIES
NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

    if (handle == NULL)
    	{
    	/* 
    	** Set the error mode so that Windows won't display a dialog box
    	** if it can't load the DLL.
    	*/
	if (ib_path_env_var == NULL) 
	    strcpy (absolute_module, buffer);
	else if (!gds__validate_lib_path (buffer, ib_path_env_var, absolute_module, sizeof(absolute_module)))
	    return NULL;
    	fuErrorMode = SetErrorMode (SEM_NOOPENFILEERRORBOX);
    	handle = LoadLibrary (absolute_module);
    	(void) SetErrorMode (fuErrorMode);

	if (handle >= HINSTANCE_ERROR)
	    {
	    HARBOR_MERGE
	    /* 
	    ** if the module loaded is ourselves, 
	    ** we have reloaded ourselves, undo it. 
	    */
	    if (handle == hDLLInstance)
	    	FreeLibrary (handle);
	    else
		{
	        /* If it loaded, save in our cache if there is room */
	        if (i < MODULE_CACHE_SIZE)
		    {
	            strcpy (load_cache[i].module_name, buffer);
	            load_cache[i].handle = handle;
	            *(load_cache[i+1].module_name) = 0;
	            load_cache[i+1].handle = 0;
		    }
		else
	    	    gds__register_cleanup (cleanup, (void *)handle);
		}
	    }

        }
    }

if (handle < HINSTANCE_ERROR)
    {
    return NULL;
    }


/* Remove trailing whitespace from the function name */
for (p = name, q = buffer; (*q = *p) && *p++ != ' '; q++)
    ;
*q = 0;

HARBOR_MERGE

if (!(function = (FPTR_INT) GetProcAddress (handle, buffer)))
    {
    /* 
    ** The compiler prefixes an '_' for functions compiled
    **  with the __cdecl calling convention. 
    */
    q = buffer;
    *q++ = '_';
    for (p = name; (*q = *p) && *p++ != ' '; q++)
	;
    *q = 0;
    function = (FPTR_INT) GetProcAddress (handle, buffer);
    }

return function;
}
#endif

#ifdef mpexl
#define LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t	( M P E / X L )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
void	(*function)();
TEXT	formal [64], procname [64], *p;
SLONG	length, status;

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

if (*module)
    {
    /* If we have a library name, use it */

    formal [0] = ' ';
    for (p = &formal [1]; *p = *module++; p++)
	;
    *p = ' ';
    module = formal;
    }
else
    {
    /* There's no explicit library, use the first XL found */

REPLACE THIS COMPILER ERROR WITH A CALL TO GDS__VALIDATE_EXT_LIB_PATH
TO VALIDATE THE PATH OF THE MODULE.

REPLACE THIS COMPILER ERROR WITH CODE TO VERIFY THAT THE MODULE IS
FOUND EITHER IN $INTERBASE/UDF, OR $INTERBASE/intl OR IN ONE OF THE
DIRECTORIES NAMED IN EXTERNAL_FUNCTION_DIRECTORY LINES IN ISC_CONFIG.

    HPFIRSTLIBRARY (formal, &status, &length);
    module = (!status && length) ? formal : NULL;
    }

procname [0] = ' ';
for (p = &procname [1]; *p = *name++; p++)
    ;
*p = ' ';

HPGETPROCPLABEL (procname, (int*) &function, &status, module, FALSE);

return function;
}
#endif

#ifndef LOOKUP
FPTR_INT ISC_lookup_entrypoint (
    TEXT	*module,
    TEXT	*name,
    TEXT	*ib_path_env_var)
{
/**************************************
 *
 *	I S C _ l o o k u p _ e n t r y p o i n t  ( d e f a u l t )
 *
 **************************************
 *
 * Functional description
 *	Lookup entrypoint of function.
 *
 **************************************/
FPTR_INT	function;

if (function = FUNCTIONS_entrypoint (module, name))
    return function;

return NULL;
}
#endif

#ifdef WINDOWS_ONLY
void    FLU_wep( void)
{
/**************************************
 *
 *      F L U _ w e p
 *
 **************************************
 *
 * Functional description
 *      Call cleanup_cache for WEP.
 *
 **************************************/

TRACE ("Called flu.c FLU_wep...\n");
TRACE ("flu.c FLU_wep:  calling cleanup_cache()\n");
cleanup_cache( NULL);
TRACE ("Returning from flu.c FLU_wep...\n");
}

static void cleanup (
    void	*handle)
{
/**************************************
 *
 *	c l e a n u p
 *
 **************************************
 *
 * Functional description
 *	Unload any libraries that were
 *	dynamically loaded.
 *
 **************************************/

FreeLibrary ((HINSTANCE) handle);

}

static void cleanup_cache (
    void        *handle)
{
/**************************************
 *
 *      c l e a n u p _ c a c h e
 *
 **************************************
 *
 * Functional description
 *      Unload any cached libraries that were
 *      dynamically loaded.
 *
 **************************************/
SSHORT  i;

TRACE ("Called flu.c cleanup_cache...\n");
for (i = 0; *(load_cache[i].module_name); i++)
{
    TRACE ("flu.c cleanup_cache:  calling FreeLibrary() for ");
    TRACE (load_cache[i].module_name);
    TRACE ("\n");

    if ( load_cache[i].handle)
	FreeLibrary ((HINSTANCE) load_cache[i].handle);
}

*(load_cache[0].module_name) = 0;
load_cache[0].handle = 0;
TRACE ("Returning from flu.c cleanup_cache...\r\n");
}
#endif

#ifdef VMS
static int condition_handler (
    int		*sig,
    int		*mech,
    int		*enbl)
{
/**************************************
 *
 *	c o n d i t i o n _ h a n d l e r
 *
 **************************************
 *
 * Functional description
 *	Ignore signal from "lib$find_symbol".
 *
 **************************************/

return SS$_CONTINUE;
}
#endif
