##
##  install -- Install a program, script or datafile
##  Copyright (c) 1997-2000 Ralf S. Engelschall <rse@engelschall.com>
##  Originally written for shtool
##
##  This file is part of shtool and free software; you can redistribute
##  it and/or modify it under the terms of the GNU General Public
##  License as published by the Free Software Foundation; either version
##  2 of the License, or (at your option) any later version.
##
##  This file is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
##  General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
##  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
##

str_tool="install"
str_usage="[-v] [-t] [-c] [-C] [-s] [-m<mode>] [-o<owner>] [-g<group>] [-e<ext>] <file> [<file> ...] <path>"
arg_spec="2+"
opt_spec="v.t.c.C.s.m:o:g:e:"
opt_v=no
opt_t=no
opt_c=no
opt_C=no
opt_s=no
opt_m=""
opt_o=""
opt_g=""
opt_e=""

. ./sh.common

#   determine source(s) and destination 
argc=$#
srcs=""
while [ $# -gt 1 ]; do
    srcs="$srcs $1"
    shift
done
dstpath="$1"

#   type check for destination
dstisdir=0
if [ -d $dstpath ]; then
    dstpath=`echo "$dstpath" | sed -e 's:/$::'`
    dstisdir=1
fi

#   consistency check for destination
if [ $argc -gt 2 -a $dstisdir = 0 ]; then
    echo "$msgprefix:Error: multiple sources require destination to be directory" 1>&2
    exit 1
fi

#   iterate over all source(s)
for src in $srcs; do
    dst=$dstpath

    #  If destination is a directory, append the input filename
    if [ $dstisdir = 1 ]; then
        dstfile=`echo "$src" | sed -e 's;.*/\([^/]*\)$;\1;'`
        dst="$dst/$dstfile"
    fi

    #  Add a possible extension to src and dst
    if [ ".$opt_e" != . ]; then
        src="$src$opt_e"
        dst="$dst$opt_e"
    fi

    #  Check for correct arguments
    if [ ".$src" = ".$dst" ]; then
        echo "$msgprefix:Warning: source and destination are the same - skipped" 1>&2
        continue
    fi
    if [ -d "$src" ]; then
        echo "$msgprefix:Warning: source \`$src' is a directory - skipped" 1>&2
        continue
    fi

    #  Make a temp file name in the destination directory
    dsttmp=`echo $dst |\
            sed -e 's;[^/]*$;;' -e 's;\(.\)/$;\1;' -e 's;^$;.;' \
                -e "s;\$;/#INST@$$#;"`

    #  Verbosity
    if [ ".$opt_v" = .yes ]; then
        echo "$src -> $dst" 1>&2
    fi

    #  Copy or move the file name to the temp name
    #  (because we might be not allowed to change the source)
    if [ ".$opt_C" = .yes ]; then
        opt_c=yes
    fi
    if [ ".$opt_c" = .yes ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "cp $src $dsttmp" 1>&2
        fi
        cp $src $dsttmp || exit $?
    else
        if [ ".$opt_t" = .yes ]; then
            echo "mv $src $dsttmp" 1>&2
        fi
        mv $src $dsttmp || exit $?
    fi

    #  Adjust the target file
    #  (we do chmod last to preserve setuid bits)
    if [ ".$opt_s" = .yes ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "strip $dsttmp" 1>&2
        fi
        strip $dsttmp || exit $?
    fi
    if [ ".$opt_o" != . ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "chown $opt_o $dsttmp" 1>&2
        fi
        chown $opt_o $dsttmp || exit $?
    fi
    if [ ".$opt_g" != . ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "chgrp $opt_g $dsttmp" 1>&2
        fi
        chgrp $opt_g $dsttmp || exit $?
    fi
    if [ ".$opt_m" != . ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "chmod $opt_m $dsttmp" 1>&2
        fi
        chmod $opt_m $dsttmp || exit $?
    fi

    #   Determine whether to do a quick install
    #   (has to be done _after_ the strip was already done)
    quick=no
    if [ ".$opt_C" = .yes ]; then
        if [ -r $dst ]; then
            if cmp -s $src $dst; then
                quick=yes
            fi
        fi
    fi

    #   Finally install the file to the real destination
    if [ $quick = yes ]; then
        if [ ".$opt_t" = .yes ]; then
            echo "rm -f $dsttmp" 1>&2
        fi
        rm -f $dsttmp
    else
        if [ ".$opt_t" = .yes ]; then
            echo "rm -f $dst && mv $dsttmp $dst" 1>&2
        fi
        rm -f $dst && mv $dsttmp $dst
    fi
done

