$!  The contents of this file are subject to the Interbase Public
$!  License Version 1.0 (the "License"); you may not use this file
$!  except in compliance with the License. You may obtain a copy
$!  of the License at http://www.Inprise.com/IPL.html
$!
$!  Software distributed under the License is distributed on an
$!  "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
$!  or implied. See the License for the specific language governing
$!  rights and limitations under the License.
$!
$!  The Original Code was created by Inprise Corporation
$!  and its predecessors. Portions created by Inprise Corporation are
$!  Copyright (C) Inprise Corporation.
$!
$!  All Rights Reserved.
$!  Contributor(s): ______________________________________.
$!
$ on control_y then goto punt
$ on control_c then goto punt
$ on warning then goto punt
$ on error then goto punt
$ @gds$common:[interbase]start_interbase.com
$ @interbase:[sysexe]gds_login.com
$ set def interbase:[examples]
$ type sys$input

    InterBase installation verification procedure.

$ set verify
$ gbak/replace atlas.gbak atlas.gdb
$ gbak/replace emp.gbak emp.gdb
$ qli
ready atlas.gdb
show relations
show states

print count of states

/* Hmm, I thought there were 50 states.  Check it out. */

for states sorted by desc statehood
    print state_name, capital, statehood

/* D.C. isn't really a state -- eliminate it */

for states with capital missing
    list then erase

print count of states

report cities cross states over state -
    with population not missing -
    sorted by state, desc population
set report_name = "M a j o r   C i t i e s" / "( b y   s t a t e )"
at top of state print state_name
print city, population using z,zzz,zz9
at bottom of state print col 10, count | " cities in " | state_name, skip
end_report
$
$ purge *.gdb
$ set noverify
$ type sys$input

	The installation verification procedure for InterBase has run
	successfully.

$ set file/protection=(w:rew) atlas.gdb,emp.gdb
$ purge INTERBASE:[EXAMPLES]*.*
$
$ exit
$
$ punt:
$ set noverify
$ type sys$input

	The installation verification procedure for InterBase has 
	failed.   Please call 1-800-437-7367 for assistance.

$
