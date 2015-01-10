# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Loads some Tcl-compatible features.
# I/O commands, case, lassign, parray, errorInfo, ::tcl_platform, ::env
# try, throw, file copy, file delete -force

# Set up the ::env array
set env [env]

if {[info commands stdout] ne ""} {
	# Tcl-compatible I/O commands
	foreach p {gets flush close eof seek tell} {
		proc $p {chan args} {p} {
			tailcall $chan $p {*}$args
		}
	}
	unset p

	# puts is complicated by -nonewline
	#
	proc puts {{-nonewline {}} {chan stdout} msg} {
		if {${-nonewline} ni {-nonewline {}}} {
			tailcall ${-nonewline} puts $msg
		}
		tailcall $chan puts {*}${-nonewline} $msg
	}

	# read is complicated by -nonewline
	#
	# read chan ?maxchars?
	# read -nonewline chan
	proc read {{-nonewline {}} chan} {
		if {${-nonewline} ni {-nonewline {}}} {
			tailcall ${-nonewline} read {*}${chan}
		}
		tailcall $chan read {*}${-nonewline}
	}

	proc fconfigure {f args} {
		foreach {n v} $args {
			switch -glob -- $n {
				-bl* {
					$f ndelay $v
				}
				-bu* {
					$f buffering $v
				}
				-tr* {
					# Just ignore -translation
				}
				default {
					return -code error "fconfigure: unknown option $n"
				}
			}
		}
	}
}

# case var ?in? pattern action ?pattern action ...?
proc case {var args} {
	# Skip dummy parameter
	if {[lindex $args 0] eq "in"} {
		set args [lrange $args 1 end]
	}

	# Check for single arg form
	if {[llength $args] == 1} {
		set args [lindex $args 0]
	}

	# Check for odd number of args
	if {[llength $args] % 2 != 0} {
		return -code error "extra case pattern with no body"
	}

	# Internal function to match a value agains a list of patterns
	local proc case.checker {value pattern} {
		string match $pattern $value
	}

	foreach {value action} $args {
		if {$value eq "default"} {
			set do_action $action
			continue
		} elseif {[lsearch -bool -command case.checker $value $var]} {
			set do_action $action
			break
		}
	}

	if {[info exists do_action]} {
		set rc [catch [list uplevel 1 $do_action] result opts]
		if {$rc} {
			incr opts(-level)
		}
		return {*}$opts $result
	}
}

# fileevent isn't needed in Jim, but provide it for compatibility
proc fileevent {args} {
	tailcall {*}$args
}

# Second, option argument is a glob pattern
# Third, optional argument is a "putter" function
# 
proc parray {arrayname {pattern *} {puts puts}} {
	upvar $arrayname a

	set max 0
	foreach name [array names a $pattern]] {
		if {[string length $name] > $max} {
			set max [string length $name]
		}
	}
	incr max [string length $arrayname]
	incr max 2
	foreach name [lsort [array names a $pattern]] {
		$puts [format "%-${max}s = %s" $arrayname\($name\) $a($name)]
	}
}

# Implements 'file copy' - single file mode only
proc {file copy} {{force {}} source target} {
	try {
		if {$force ni {{} -force}} {
			error "bad option \"$force\": should be -force"
		}

		set in [open $source]

		if {$force eq "" && [file exists $target]} {
			$in close
			error "error copying \"$source\" to \"$target\": file already exists"
		}
		set out [open $target w]
		$in copyto $out
		$out close
	} on error {msg opts} {
		incr opts(-level)
		return {*}$opts $msg
	} finally {
		catch {$in close}
	}
}

# 'open "|..." ?mode?" will invoke this wrapper around exec/pipe
# Note that we return a lambda which also provides the 'pid' command
proc popen {cmd {mode r}} {
	lassign [socket pipe] r w
	try {
		if {[string match "w*" $mode]} {
			lappend cmd <@$r &
			set pids [exec {*}$cmd]
			$r close
			set f $w
		} else {
			lappend cmd >@$w &
			set pids [exec {*}$cmd]
			$w close
			set f $r
		}
		lambda {cmd args} {f pids} {
			if {$cmd eq "pid"} {
				return $pids
			}
			if {$cmd eq "close"} {
				$f close
				# And wait for the child processes to complete
				foreach p $pids { os.wait $p }
				return
			}
			tailcall $f $cmd {*}$args
		}
	} on error {error opts} {
		$r close
		$w close
		error $error
	}
}

# A wrapper around 'pid' which can return the pids for 'popen'
local proc pid {{chan {}}} {
	if {$chan eq ""} {
		tailcall upcall pid
	}
	if {[catch {$chan tell}]} {
		return -code error "can not find channel named \"$chan\""
	}
	if {[catch {$chan pid} pids]} {
		return ""
	}
	return $pids
}

# try/on/finally conceptually similar to Tcl 8.6
#
# Usage: try ?catchopts? script ?onclause ...? ?finallyclause?
#
# Where:
#   onclause is:       on codes {?resultvar? ?optsvar?} script
#
#   codes is: a list of return codes (ok, error, etc. or integers), or * for any
#
#   finallyclause is:  finally script
#
#
# Where onclause is: on codes {?resultvar? ?optsvar?}
proc try {args} {
	set catchopts {}
	while {[string match -* [lindex $args 0]]} {
		set args [lassign $args opt]
		if {$opt eq "--"} {
			break
		}
		lappend catchopts $opt
	}
	if {[llength $args] == 0} {
		return -code error {wrong # args: should be "try ?options? script ?argument ...?"}
	}
	set args [lassign $args script]
	set code [catch -eval {*}$catchopts [list uplevel 1 $script] msg opts]

	set handled 0

	foreach {on codes vars script} $args {
		switch -- $on \
			on {
				if {!$handled && ($codes eq "*" || [info returncode $code] in $codes)} {
					lassign $vars msgvar optsvar
					if {$msgvar ne ""} {
						upvar $msgvar hmsg
						set hmsg $msg
					}
					if {$optsvar ne ""} {
						upvar $optsvar hopts
						set hopts $opts
					}
					# Override any body result
					set code [catch [list uplevel 1 $script] msg opts]
					incr handled
				}
			} \
			finally {
				set finalcode [catch [list uplevel 1 $codes] finalmsg finalopts]
				if {$finalcode} {
					# Override any body or handler result
					set code $finalcode
					set msg $finalmsg
					set opts $finalopts
				}
				break
			} \
			default {
				return -code error "try: expected 'on' or 'finally', got '$on'"
			}
	}

	if {$code} {
		incr opts(-level)
		return {*}$opts $msg
	}
	return $msg
}

# Generates an exception with the given code (ok, error, etc. or an integer)
# and the given message
proc throw {code {msg ""}} {
	return -code $code $msg
}

# Helper for "file delete -force"
proc {file delete force} {path} {
	foreach e [readdir $path] {
		file delete -force $path/$e
	}
	file delete $path
}
