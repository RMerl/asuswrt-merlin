# No need for package support in the bootstrap jimsh, but
# Tcl extensions call package require
proc package {args} {}
