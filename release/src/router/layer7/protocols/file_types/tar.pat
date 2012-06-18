# Tar - tape archive.  Standard UNIX file archiver, not just for tapes.
# Pattern attributes: good notsofast notsofast subset
# Protocol groups: file

# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
tar
# /usr/share/magic
## POSIX tar archives
#257     string          ustar\0         POSIX tar archive
#257     string          ustar\040\040\0 GNU tar archive
# this is pretty general.  It's not a dictionary word, but still...
ustar
