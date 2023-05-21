/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		apollo.h
 *	DESCRIPTION:	Apollo Specific IO
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

#ifndef _JRD_APOLLO_H_
#define _JRD_APOLLO_H_

std_$call void	name_$resolve(),
		file_$read_lock_entryu();

typedef SLONG	node_t;
typedef SLONG	network_t;

typedef enum {
    file_$nr_xor_lw,
    file_$cowriters
} file_$obj_mode_t;

typedef enum {
    file_$all,
    file_$read,
    file_$read_intend_write,
    file_$chng_read_to_write,
    file_$write,
    file_$chng_write_to_read,
    file_$chng_read_to_riw,
    file_$chng_write_to_riw,
    file_$mark_for_del
} file_$acc_mode_t;

typedef struct {
    uid_$t		ouid;
    uid_$t		puid;
    file_$obj_mode_t	omode;
    file_$acc_mode_t	amode;
    SSHORT		transid;
    node_t		node;
} file_$lock_entry_t;

#define CHECK		if (status.all) error (status);
#define NODE_MASK	0xFFFFF

#endif /* _JRD_APOLLO_H_ */
