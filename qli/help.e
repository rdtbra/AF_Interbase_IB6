/*
 *	PROGRAM:	JRD Command Oriented Query Language
 *	MODULE:		help.e
 *	DESCRIPTION:	Help module.
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

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../jrd/gds.h"
#include "../qli/dtr.h"
#include "../qli/compile.h"
#include "../qli/err_proto.h"
#include "../qli/help_proto.h"
#include "../qli/lex_proto.h"
#include "../jrd/gds_proto.h"

#define INDENT		"  "
#define COLUMN_WIDTH	20
#define RIGHT_MARGIN	70

#ifdef VMS
#define TARGET	"[syshlp]help.gdb"
#endif

#ifdef APOLLO
#define TARGET "help/help.gdb"
#endif

#ifdef UNIX
#define TARGET "help/help.gdb"
#endif

#ifdef PC_PLATFORM
#define TARGET "help\\help.gdb"
#endif

#if (defined WIN_NT || defined OS2_ONLY)
#define TARGET "help/help.gdb"
#endif

#ifdef mpexl
#define TARGET "help.gdb"
#endif

DATABASE
    HELP_DB	= STATIC "help.gdb" RUNTIME target;

static int	additional_topics (TEXT *, TEXT *, TEXT *);
static void	print_more (USHORT, USHORT, TEXT **, USHORT *, TEXT *, USHORT);
static void	print_topic (USHORT, USHORT, TEXT **, USHORT *, TEXT *, USHORT);
static TEXT	*strip (TEXT *);

void HELP_fini (void)
{
/**************************************
 *
 *	H E L P _ f i n i
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

if (HELP_DB)
    {
    COMMIT;
    FINISH;
    }
}

void HELP_help (
    SYN		node)
{
/**************************************
 *
 *	H E L P _ h e l p
 *
 **************************************
 *
 * Functional description
 *	Give the poor sucker help.
 *
 **************************************/
NAM	*ptr, *end, name;
USHORT	max_level;
TEXT	target [128], **topic, *topics [16];

if (!HELP_DB)
    {
    gds__prefix (target, TARGET);
    READY
	ON_ERROR
	    ERRQ_database_error (NULL_PTR, gds__status);
	END_ERROR;
    START_TRANSACTION;
    }

max_level = 0;
topic = topics;
*topic++ = "QLI";

for (ptr = (NAM*) node->syn_arg, end = ptr + node->syn_count; ptr < end; ptr++)
    *topic++ = (*ptr)->nam_string;

print_topic (0, node->syn_count, topics, &max_level, "", FALSE);

if (max_level < node->syn_count)
    print_topic (0, node->syn_count, topics, &max_level, "", TRUE);
}

static int additional_topics (
    TEXT	*parent,
    TEXT	*banner,
    TEXT	*string)
{
/**************************************
 *
 *	a d d i t i o n a l _ t o p i c s
 *
 **************************************
 *
 * Functional description
 *	Print a list of other known topics.
 *
 **************************************/
TEXT	*p, line [256], *ptr;
USHORT	topics, l;

/* Print list of know topics.  When we find the first, 
   print the banner, if any */

ptr = line;
topics = 0;
l = strlen (parent);

FOR X IN TOPICS WITH X.PARENT EQ parent SORTED BY X.TOPIC
    if (QLI_abort)
	return;
    p = X.TOPIC;
    while (*p && *p != ' ')
	p++;
    if (!(l = p - X.TOPIC))
	continue;
    if (++topics == 1 && banner)
	ib_printf ("%s\n", banner, string, parent);
    p = line + ((ptr - line + COLUMN_WIDTH - 1) / COLUMN_WIDTH) * COLUMN_WIDTH;
    if (p + l > line + RIGHT_MARGIN)
	{
	*ptr = 0;
	ib_printf ("%s%s\n", INDENT, line);
	p = ptr = line;
	}
    while (ptr < p)
	*ptr++ = ' ';
    p = X.TOPIC;
    do *ptr++ = *p++; while (--l);
    *ptr++ = ' ';
END_FOR;

if (ptr != line)
    {
    *ptr = 0;
    ib_printf ("%s%s\n", INDENT, line);
    }

return topics;
}

static void print_more (
    USHORT	level,
    USHORT	depth,
    TEXT	**topics,
    USHORT	*max_level,
    TEXT	*parent,
    USHORT	error_flag)
{
/**************************************
 *
 *	p r i n t _ m o r e
 *
 **************************************
 *
 * Functional description
 *	We have printed a topic with additional sub-topics.  Ask the user if he
 *	wants more.
 *
 **************************************/
TEXT	buffer [256], topic [80], prompt [80], *p, *q;

ERRQ_msg_get (502, prompt);		/* Msg502 "Subtopic? "	*/

/* Prompt the user for a line */

if (!LEX_get_line (prompt, buffer, sizeof (buffer)))
    return;

/* Upcase the response and zap the blanks */

topics [1] = p = topic;

for (q = buffer; *q && *q != '\n'; q++)
    if (*q != ' ')
	*p++ = UPPER (*q);
*p = 0;

/* If we got anything, print the new topic */

if (p != topic)
    print_topic (level + 1, depth + 1, topics + 1, max_level, parent, FALSE);
}

static void print_topic (
    USHORT	level,
    USHORT	depth,
    TEXT	**topics,
    USHORT	*max_level,
    TEXT	*parent,
    USHORT	error_flag)
{
/**************************************
 *
 *	p r i n t _ t o p i c
 *
 **************************************
 *
 * Functional description
 *	Lookup topic in path.  If we find a topic, and are not level
 *	zero, recurse.  Otherwise print out the topic.  In any case,
 *	remember the lowest level on which a topic was found.  If the
 *	error flag is set, print out a list of sub-topics.
 *
 **************************************/
USHORT	count;
TEXT	string [128], banner [128], *next, *p, *q, *path, buffer [128], prompt [80];

/* Copy the parent string inserting a blank at the end */

p = string;

if (*parent)
    {
    q = parent;
    while (*q)
	*p++ = *q++;
    *p++ = ' ';
    }

next = p;
count = 0;

FOR X IN TOPICS WITH X.FACILITY EQ "QLI" AND 
		    (X.TOPIC STARTING WITH *topics AND
		    X.PARENT EQ parent) OR 
		    (X.TOPIC = X.FACILITY AND 
		    X.TOPIC = *topics AND X.TOPIC = X.PARENT) 
    count++;
END_FOR;		    

if (count > 1)
    ERRQ_msg_put (80, (TEXT*) (ULONG) count, *topics, NULL, NULL, NULL); /* Msg80 [%d topics matched %s] */

count = 0;

FOR (LEVEL level) X IN TOPICS WITH X.FACILITY EQ "QLI" AND 
		    (X.TOPIC STARTING WITH *topics AND
		    X.PARENT EQ parent) OR 
		    (X.TOPIC = X.FACILITY AND 
		    X.TOPIC = *topics AND X.TOPIC = X.PARENT) 
		    SORTED BY X.TOPIC
    if (QLI_abort)
	return;     
    if (count)
	{
	ERRQ_msg_get (503, prompt);	/* Msg503 "\ntype <cr> for next topic or <EOF> to stop: " */
	if (!LEX_get_line (prompt, buffer, sizeof (buffer)))
	    return;
	}
    ++count;
    if (level > *max_level)
	*max_level = level;
    p = next;
    q = X.TOPIC;
    while (*q && *q != ' ')
	*p++ = *q++;
    *p = 0;
    if (level < depth)
	print_topic (level + 1, depth, topics + 1, max_level, string, error_flag);
    else
	{
	ib_printf ("\n%s\n\n", strip (string));
	QLI_skip_line = TRUE;
	FOR B IN X.TEXT
	    if (QLI_abort)
		return;
	    B.SEGMENT [B.LENGTH] = 0;
	    ib_printf ("%s%s", INDENT, B.SEGMENT);
	END_FOR;
	ERRQ_msg_format (81, sizeof (banner), banner, INDENT, NULL, NULL, NULL, NULL); /* Msg81 %sSub-topics available: */
	if (additional_topics (string, banner, INDENT))
	    print_more (level, depth, topics, max_level, string, error_flag);
	}
END_FOR;

if (!count && error_flag && level > *max_level)
    {
    path = strip (parent);
    ERRQ_msg_put (82, path, *topics, NULL, NULL, NULL); /* Msg82 No help is available for %s %s */
    ERRQ_msg_format (83, sizeof (banner), banner, path, NULL, NULL, NULL, NULL); /* Msg83 Sub-topics available for %s are: */
    additional_topics (parent, banner, path);
    }
}

static TEXT *strip (
    TEXT	*string)
{
/**************************************
 *
 *	s t r i p
 *
 **************************************
 *
 * Functional description
 *	Strip off the first topic in the path.
 *
 **************************************/
TEXT	*p;

p = string;

while (*p != ' ')
    if (!*p++)
	return string;

while (*p == ' ')
    if (!*p++)
	return string;

return p;
}
