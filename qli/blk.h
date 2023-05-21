/*
 *	PROGRAM:	JRD Command Oriented Query Language
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

BLKDEF (type_frb, frb, 0)
BLKDEF (type_hnk, hnk, 0)
BLKDEF (type_plb, plb, 0)
BLKDEF (type_vec, vec, sizeof (((VEC) 0)->vec_object[0]))
BLKDEF (type_dbb, dbb, 1)
BLKDEF (type_rel, rel, 0)
BLKDEF (type_fld, fld, 1)
BLKDEF (type_vcl, vcl, sizeof (((VCL) 0)->vcl_long[0]))
BLKDEF (type_req, req, 0)				/* Request block */
BLKDEF (type_nod, nod, sizeof (((NOD) 0)->nod_arg[0]))
BLKDEF (type_syn, nod, sizeof (((SYN) 0)->syn_arg[0]))
BLKDEF (type_lls, lls, 0)				/* linked list stack */
BLKDEF (type_str, str, 1)				/* random string block */
BLKDEF (type_tok, tok, 1)				/* token block */
BLKDEF (type_sym, sym, 1)				/* symbol block */
BLKDEF (type_msg, msg, 0)				/* Message block */
BLKDEF (type_nam, nam, 1)				/* Name node */
BLKDEF (type_ctx, ctx, 0)				/* Context block */
BLKDEF (type_con, con, 1)				/* Constant block */
BLKDEF (type_itm, itm, 0)				/* Print item */
BLKDEF (type_par, par, 0)				/* Parameter block */
BLKDEF (type_line, line, 1)				/* Input line block */
BLKDEF (type_brk, brk, 0)
BLKDEF (type_rpt, rpt, 0)
BLKDEF (type_pic, pic, 0)
BLKDEF (type_prt, prt, 0)
BLKDEF (type_map, map, 0)
BLKDEF (type_qpr, qpr, 0)
BLKDEF (type_qfn, qfn, 0)
BLKDEF (type_qfl, qfl, 0)
BLKDEF (type_frm, frm, 1)
BLKDEF (type_ffl, ffl, 1)
BLKDEF (type_men, men, 1)
BLKDEF (type_fun, fun, sizeof (((FUN) 0)->fun_arg[0]))
BLKDEF (type_rlb, rlb, 0)				/* Request language block */
