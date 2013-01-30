#!/bin/sh

test $# -ge 2 || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }

# cd to objtree
cd -- "$2" || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }
# In separate objtree build, include/ might not exist yet
mkdir include 2>/dev/null

srctree="$1"

# (Re)generate include/applets.h
src="$srctree/include/applets.src.h"
dst="include/applets.h"
s=`sed -n 's@^//applet:@@p' -- "$srctree"/*/*.c "$srctree"/*/*/*.c`
old=`cat "$dst" 2>/dev/null`
# Why "IFS='' read -r REPLY"??
# This atrocity is needed to read lines without mangling.
# IFS='' prevents whitespace trimming,
# -r suppresses backslash handling.
new=`echo "/* DO NOT EDIT. This file is generated from applets.src.h */"
while IFS='' read -r REPLY; do
	test x"$REPLY" = x"INSERT" && REPLY="$s"
	printf "%s\n" "$REPLY"
done <"$src"`
if test x"$new" != x"$old"; then
	echo "  GEN     $dst"
	printf "%s\n" "$new" >"$dst"
fi

# (Re)generate include/usage.h
src="$srctree/include/usage.src.h"
dst="include/usage.h"
# We add line continuation backslash after each line,
# and insert empty line before each line which doesn't start
# with space or tab
# (note: we need to use \\\\ because of ``)
s=`sed -n -e 's@^//usage:\([ \t].*\)$@\1 \\\\@p' -e 's@^//usage:\([^ \t].*\)$@\n\1 \\\\@p' -- "$srctree"/*/*.c "$srctree"/*/*/*.c`
old=`cat "$dst" 2>/dev/null`
new=`echo "/* DO NOT EDIT. This file is generated from usage.src.h */"
while IFS='' read -r REPLY; do
	test x"$REPLY" = x"INSERT" && REPLY="$s"
	printf "%s\n" "$REPLY"
done <"$src"`
if test x"$new" != x"$old"; then
	echo "  GEN     $dst"
	printf "%s\n" "$new" >"$dst"
fi

# (Re)generate */Kbuild and */Config.in
{ cd -- "$srctree" && find -type d; } | while read -r d; do
	d="${d#./}"

	src="$srctree/$d/Kbuild.src"
	dst="$d/Kbuild"
	if test -f "$src"; then
		mkdir -p -- "$d" 2>/dev/null
		#echo "  CHK     $dst"

		s=`sed -n 's@^//kbuild:@@p' -- "$srctree/$d"/*.c`

		old=`cat "$dst" 2>/dev/null`
		new=`echo "# DO NOT EDIT. This file is generated from Kbuild.src"
		while IFS='' read -r REPLY; do
			test x"$REPLY" = x"INSERT" && REPLY="$s"
			printf "%s\n" "$REPLY"
		done <"$src"`
		if test x"$new" != x"$old"; then
			echo "  GEN     $dst"
			printf "%s\n" "$new" >"$dst"
		fi
	fi

	src="$srctree/$d/Config.src"
	dst="$d/Config.in"
	if test -f "$src"; then
		mkdir -p -- "$d" 2>/dev/null
		#echo "  CHK     $dst"

		s=`sed -n 's@^//config:@@p' -- "$srctree/$d"/*.c`

		old=`cat "$dst" 2>/dev/null`
		new=`echo "# DO NOT EDIT. This file is generated from Config.src"
		while IFS='' read -r REPLY; do
			test x"$REPLY" = x"INSERT" && REPLY="$s"
			printf "%s\n" "$REPLY"
		done <"$src"`
		if test x"$new" != x"$old"; then
			echo "  GEN     $dst"
			printf "%s\n" "$new" >"$dst"
		fi
	fi
done

# Last read failed. This is normal. Don't exit with its error code:
exit 0
