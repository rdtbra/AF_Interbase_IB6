/*
 *	PROGRAM:	JRD Remote Interface/Server
 *	MODULE:		blk.h
 *	DESCRIPTION:	Block type definitions
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

BLKDEF (type_vec, vec, sizeof (((VEC) 0)->vec_object[0]))
BLKDEF (type_rdb, rdb, 0)
BLKDEF (type_fmt, fmt, sizeof (((FMT) 0)->fmt_desc[0]))
BLKDEF (type_rrq, rrq, sizeof (((RRQ) 0)->rrq_rpt [0]))
BLKDEF (type_rtr, rtr, 0)
BLKDEF (type_str, str, 1)			/* random string block */
BLKDEF (type_rbl, rbl, 1)
BLKDEF (type_port, port, 1)
BLKDEF (type_msg, message, 1)
BLKDEF (type_rsr, rsr, 0)
BLKDEF (type_rvnt, rvnt, 0)
BLKDEF (type_rpr, rpr, 0)
BLKDEF (type_rmtque, rmtque, 0)

