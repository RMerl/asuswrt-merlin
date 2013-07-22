#!/bin/tcsh

# A small example program for using the new getopt(1) program.
# This program will only work with tcsh(1)
# An similar program using the bash(1) script language can be found
# as parse.bash

# Example input and output (from the tcsh prompt):
# ./parse.tcsh -a par1 'another arg' --c-long 'wow\!*\?' -cmore -b " very long "
# Option a
# Option c, no argument
# Option c, argument `more'
# Option b, argument ` very long '
# Remaining arguments:
# --> `par1'
# --> `another arg'
# --> `wow!*\?'

# Note that we had to escape the exclamation mark in the wow-argument. This
# is _not_ a problem with getopt, but with the tcsh command parsing. If you
# would give the same line from the bash prompt (ie. call ./parse.tcsh),
# you could remove the exclamation mark.

# This is a bit tricky. We use a temp variable, to be able to check the
# return value of getopt (eval nukes it). argv contains the command arguments
# as a list. The ':q`  copies that list without doing any substitutions:
# each element of argv becomes a separate argument for getopt. The braces
# are needed because the result is also a list.
set temp=(`getopt -s tcsh -o ab:c:: --long a-long,b-long:,c-long:: -- $argv:q`)
if ($? != 0) then 
  echo "Terminating..." >/dev/stderr
  exit 1
endif

# Now we do the eval part. As the result is a list, we need braces. But they
# must be quoted, because they must be evaluated when the eval is called.
# The 'q` stops doing any silly substitutions.
eval set argv=\($temp:q\)

while (1)
	switch($1:q)
	case -a:
	case --a-long:
		echo "Option a" ; shift 
		breaksw;
	case -b:
	case --b-long:
		echo "Option b, argument "\`$2:q\' ; shift ; shift
		breaksw
	case -c:
	case --c-long:
		# c has an optional argument. As we are in quoted mode,
		# an empty parameter will be generated if its optional
		# argument is not found.

		if ($2:q == "") then
			echo "Option c, no argument"
		else
			echo "Option c, argument "\`$2:q\'
		endif
		shift; shift
		breaksw
	case --:
		shift
		break
	default:
		echo "Internal error!" ; exit 1
	endsw
end

echo "Remaining arguments:"
# foreach el ($argv:q) created problems for some tcsh-versions (at least
# 6.02). So we use another shift-loop here:
while ($#argv > 0)
	echo '--> '\`$1:q\'
	shift
end
