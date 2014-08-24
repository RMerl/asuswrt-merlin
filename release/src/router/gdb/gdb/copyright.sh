#!/bin/sh
# Automatically update copyright for GDB, the GNU debugger.
#
# Copyright (C) 2007 Free Software Foundation, Inc.
#
# This file is part of GDB.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Usage: cd src/gdb && sh ./copyright.sh
# To use a different version of emacs, set the EMACS environment
# variable before running.

# After running, update those files mentioned in $byhand by hand.
# Always review the output of this script before committing it!
# A useful command to review the output is:
#  filterdiff -x \*.c -x \*.cc -x \*.h -x \*.exp updates.diff
# This removes the bulk of the changes which are most likely
# to be correct.

####
# Configuration
####

# As of Emacs 22.0 (snapshot), wrapping and copyright updating do not
# handle these file types - all reasonable:
#  Assembly (weird comment characters, e.g. "!"); .S usually has C
#            comments, which are fine)
#  Fortran ("c" comment character)
#  igen
#  Autoconf input (dnl)
#  texinfo (@c)
#  tex (%)
#  *.defs as C
#   man
# So these need to be done by hand, as needed
byhand="
*.s
*.f
*.f90
*.igen
*.ac
*.texi
*.texinfo
*.tex
*.defs
*.1
"

# Files which should not be modified, either because they are
# generated, non-FSF, or otherwise special (e.g. license text,
# or test cases which must be sensitive to line numbering).
prunes="
COPYING
COPYING.LIB
CVS
configure
copying.c
gdbarch.c
gdbarch.h
fdl.texi
gpl.texi
gdbtk
gdb.gdbtk
osf-share
aclocal.m4
step-line.inp
step-line.c
"

####
# Main program
####

: ${EMACS:=emacs}

# Disable filename expansion, so that we can get at the glob patterns
# from $byhand.
set -f

version=`$EMACS --version | sed 's/GNU Emacs \([0-9]*\)\..*/\1/; 1q'`
if test "$version" -lt 22; then
  echo "error: $EMACS is too old; use at least an Emacs 22.0.XX snapshot." >&2
  exit 1
fi

if test $# -lt 1; then
  dir=.
else
  dir=$1
fi

if ! test -f doc/gdbint.texinfo; then
  echo "\"$dir\" is not a GDB source directory."
  exit 1
fi

cat > copytmp.el <<EOF
(load "copyright")
(setq vc-cvs-stay-local nil
      message-log-max t)
(setq fsf-regexp "Free[#; \t\n]+Software[#; \t\n]+Foundation,[#; \t\n]+Inc\."
      fsf-copyright-regexp (concat copyright-regexp "[#; \t\n]+" fsf-regexp)
      generated-regexp "THIS FILE IS MACHINE GENERATED WITH CGEN")

(defun gdb-copyright-update (filename)
  (widen)
  (goto-char (point-min))
  (if (and (not (re-search-forward generated-regexp (+ (point) copyright-limit) t))
	   (re-search-forward fsf-copyright-regexp (+ (point) copyright-limit) t))
      (progn
	(setq copyright-update t
	      copyright-query nil
	      fill-column 78
	      start (copy-marker (match-beginning 0))
	      end (progn
		    (re-search-backward fsf-regexp)
		    (re-search-forward fsf-regexp
		     (+ (point) copyright-limit) t)
		    (point-marker))
	      fsf-start (copy-marker (match-beginning 0)))
	(replace-match "Free_Software_Foundation,_Inc." t t)
	(copyright-update)
	(fill-region-as-paragraph start end)
	(replace-string "_" " " nil fsf-start end))
    (message (concat "WARNING: No copyright message found in " filename))))

EOF

for f in $prunes $byhand; do
  prune_opts="$prune_opts -name $f -prune -o"
done

for f in $(find "$dir" "$dir/../include/gdb" "$dir/../sim" \
           $prune_opts -type f -print); do
  cat >> copytmp.el <<EOF
(switch-to-buffer (find-file "$f"))
(setq backup-inhibited t)
(setq write-file-hooks '())
(gdb-copyright-update "$f")
(save-buffer)
(kill-buffer (buffer-name))
EOF
done

cat >> copytmp.el <<EOF
(delete-file "copytmp.el")
;; Comment out the next line to examine the message buffer.
(kill-emacs)
EOF

exec $EMACS --no-site-file -q -l ./copytmp.el
