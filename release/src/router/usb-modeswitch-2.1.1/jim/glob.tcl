# Implements a Tcl-compatible glob command based on readdir
#
# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE in this directory for licensing.

package require readdir

# Implements the Tcl glob command
#
# Usage: glob ?-nocomplain? pattern ...
#
# Patterns use 'string match' (glob) pattern matching for each
# directory level, plus support for braced alternations.
#
# e.g. glob "te[a-e]*/*.{c,tcl}"
#
# Note: files starting with . will only be returned if matching component
#       of the pattern starts with .
proc glob {args} {

	# If $dir is a directory, return a list of all entries
	# it contains which match $pattern
	#
	local proc glob.readdir_pattern {dir pattern} {
		set result {}

		# readdir doesn't return . or .., so simulate it here
		if {$pattern in {. ..}} {
			return $pattern
		}

		# If the pattern isn't actually a pattern...
		if {[string match {*[*?]*} $pattern]} {
			# Use -nocomplain here to return nothing if $dir is not a directory
			set files [readdir -nocomplain $dir]
		} elseif {[file isdir $dir] && [file exists $dir/$pattern]} {
			set files [list $pattern]
		} else {
			set files ""
		}

		foreach name $files {
			if {[string match $pattern $name]} {
				# Only include entries starting with . if the pattern starts with .
				if {[string index $name 0] eq "." && [string index $pattern 0] ne "."} {
					continue
				}
				lappend result $name
			}
		}

		return $result
	}

	# If the pattern contains a braced expression, return a list of
	# patterns with the braces expanded. {c,b}* => c* b*
	# Otherwise just return the pattern
	# Note: Only supports one braced expression. i.e. not {a,b}*{c,d}*
	proc glob.expandbraces {pattern} {
		# Avoid regexp for dependency reasons.
		# XXX: Doesn't handle backslashed braces
		if {[set fb [string first "\{" $pattern]] < 0} {
			return $pattern
		}
		if {[set nb [string first "\}" $pattern $fb]] < 0} {
			return $pattern
		}
		set before [string range $pattern 0 $fb-1]
		set braced [string range $pattern $fb+1 $nb-1]
		set after [string range $pattern $nb+1 end]

		lmap part [split $braced ,] {
			set pat $before$part$after
		}
	}

	# Core glob implementation. Returns a list of files/directories matching the pattern
	proc glob.glob {pattern} {
		set dir [file dirname $pattern]
		if {$dir eq $pattern} {
			# At the top level
			return [list $dir]
		}

		# Recursively expand the parent directory
		set dirlist [glob.glob $dir]
		set pattern [file tail $pattern]

		# Now collect the fiels/directories
		set result {}
		foreach dir $dirlist {
			set globdir $dir
			if {[string match "*/" $dir]} {
				set sep ""
			} elseif {$dir eq "."} {
				set globdir ""
				set sep ""
			} else {
				set sep /
			}
			foreach pat [glob.expandbraces $pattern] {
				foreach name [glob.readdir_pattern $dir $pat] {
					lappend result $globdir$sep$name
				}
			}
		}
		return $result
	}

	# Start of main glob
	set nocomplain 0

	if {[lindex $args 0] eq "-nocomplain"} {
		set nocomplain 1
		set args [lrange $args 1 end]
	}

	set result {}
	foreach pattern $args {
		lappend result {*}[glob.glob $pattern]
	}

	if {$nocomplain == 0 && [llength $result] == 0} {
		return -code error "no files matched glob patterns"
	}

	return $result
}
