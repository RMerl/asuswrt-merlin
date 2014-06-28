# Create a single word alias (proc) for one or more words
# e.g. alias x info exists
# if {[x var]} ...
proc alias {name args} {
	set prefix $args
	proc $name args prefix {
		tailcall {*}$prefix {*}$args
	}
}

# Creates an anonymous procedure
proc lambda {arglist args} {
	set name [ref {} function lambda.finalizer]
	tailcall proc $name $arglist {*}$args
}

proc lambda.finalizer {name val} {
	rename $name {}
}

# Like alias, but creates and returns an anonyous procedure
proc curry {args} {
	set prefix $args
	lambda args prefix {
		tailcall {*}$prefix {*}$args
	}
}

# Returns the given argument.
# Useful with 'local' as follows:
#   proc a {} {...}
#   local function a 
#
#   set x [lambda ...]
#   local function $x
#
proc function {value} {
	return $value
}

# Tcl 8.5 lassign
proc lassign {list args} {
	# in case the list is empty...
	lappend list {}
	uplevel 1 [list foreach $args $list break]
	lrange $list [llength $args] end-1
}

# Returns a list of proc filename line ...
# with 3 entries for each stack frame (proc),
# (deepest level first)
proc stacktrace {} {
	set trace {}
	foreach level [range 1 [info level]] {
		lassign [info frame -$level] p f l
		lappend trace $p $f $l
	}
	return $trace
}

# Returns a human-readable version of a stack trace
proc stackdump {stacktrace} {
	set result {}
	set count 0
	foreach {l f p} [lreverse $stacktrace] {
		if {$count} {
			append result \n
		}
		incr count
		if {$p ne ""} {
			append result "in procedure '$p' "
			if {$f ne ""} {
				append result "called "
			}
		}
		if {$f ne ""} {
			append result "at file \"$f\", line $l"
		}
	}
	return $result
}

# Sort of replacement for $::errorInfo
# Usage: errorInfo error ?stacktrace?
proc errorInfo {msg {stacktrace ""}} {
	if {$stacktrace eq ""} {
		set stacktrace [info stacktrace]
	}
	lassign $stacktrace p f l
	if {$f ne ""} {
		set result "Runtime Error: $f:$l: "
	}
	append result "$msg\n"
	append result [stackdump $stacktrace]

	# Remove the trailing newline
	string trim $result
}

# Finds the current executable by searching along the path
# Returns the empty string if not found.
proc {info nameofexecutable} {} {
	if {[info exists ::jim_argv0]} {
		if {[string match "*/*" $::jim_argv0]} {
			return [file join [pwd] $::jim_argv0]
		}
		foreach path [split [env PATH ""] $::tcl_platform(pathSeparator)] {
			set exec [file join [pwd] $path $::jim_argv0]
			if {[file executable $exec]} {
				return $exec
			}
		}
	}
	return ""
}

# Script-based implementation of 'dict with'
proc {dict with} {dictVar args script} {
	upvar $dictVar dict
	set keys {}
	foreach {n v} [dict get $dict {*}$args] {
		upvar $n var_$n
		set var_$n $v
		lappend keys $n
	}
	catch {uplevel 1 $script} msg opts
	if {[info exists dict] && [dict exists $dict {*}$args]} {
		foreach n $keys {
			if {[info exists var_$n]} {
				dict set dict {*}$args $n [set var_$n]
			} else {
				dict unset dict {*}$args $n
			}
		}
	}
	return {*}$opts $msg
}

# Script-based implementation of 'dict merge'
# This won't get called in the trivial case of no args
proc {dict merge} {dict args} {
	foreach d $args {
		# Check for a valid dict
		dict size $d
		foreach {k v} $d {
			dict set dict $k $v
		}
	}
	return $dict
}
