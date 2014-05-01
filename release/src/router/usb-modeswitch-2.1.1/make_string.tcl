#!/usr/bin/env tclsh

# (c) Josua Dietze 2012
#
# Usage: make_string.tcl source.tcl >jim-source.c

# Converts a Tcl source file into C source suitable
# for using as an embedded script.

set source [lindex $argv 0]

if {![string match *.tcl $source]} {
	error "Source $source is not a .tcl file"
}

# Read the Tcl source and convert to C macro
set sourcelines {}
set f [open $source]
while {[gets $f buf] >= 0} {
	# Remove comment lines
	regsub {^[ \t]*#.*$} $buf "" buf
	# Remove leading whitespaces
	set buf [string trimleft $buf]
	# Escape quotes and backlashes
	set buf [string map [list \\ \\\\ \" \\"] $buf]
	if [string length $buf] {
		lappend sourcelines "$buf\\n"
	}
}
close $f
puts "#define RAW \"[join $sourcelines ""]\""
