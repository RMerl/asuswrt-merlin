#!/bin/sh
#

N=`git clean -n | wc -l`
C=`git diff --stat HEAD | wc -l`

test x"$N" != x"0" && {
	echo "The tree has uncommitted changes!!! see stderr"
	echo "The tree has uncommitted changes!!!" >&2

	echo "git clean -n" >&2
	git clean -n >&2

	test x"$C" != x"0" && {
		echo "git diff -p --stat HEAD" >&2
		git diff -p --stat HEAD >&2
	}

	exit 1
}

test x"$C" != x"0" && {
	echo "The tree has uncommitted changes!!! see stderr"
	echo "The tree has uncommitted changes!!!" >&2

	echo "git diff -p --stat HEAD" >&2
	git diff -p --stat HEAD >&2

	exit 1
}

echo "clean tree"
exit 0
