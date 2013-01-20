#
# Insert the header.....
#
1i\
# +++ Dependency line eater +++\
# \
# Makefile dependencies follow.  This must be the last section in\
# the Makefile.in file\
#

#
# Remove line continuations....
#
:FIRST
y/	/ /
s/^ *//
/\\$/{
N
y/	/ /
s/\\\n */ /
bFIRST
}
s/  */ /g

s;/usr/include/[^ ]* *;;g
s;/usr/lib/[^ ]* *;;g
s;/mit/cygnus[^ ]* *;;g
s;../[^ ]*lib/blkid/blkid[^ ]* *;;g
s;../[^ ]*lib/uuid/uuid.h[^ ]* *;;g

#
# Now insert a trailing newline...
#
$a\

