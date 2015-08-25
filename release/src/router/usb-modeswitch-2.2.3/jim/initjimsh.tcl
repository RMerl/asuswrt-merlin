# This pseudo-package is loaded from jimsh to add additional
# paths to $auto_path and to source ~/.jimrc

proc _jimsh_init {} {
	rename _jimsh_init {}

	# Add to the standard auto_path
	lappend p {*}[split [env JIMLIB {}] $::tcl_platform(pathSeparator)]
	lappend p {*}$::auto_path
	lappend p [file dirname [info nameofexecutable]]
	set ::auto_path $p

	if {$::tcl_interactive && [env HOME {}] ne ""} {
		foreach src {.jimrc jimrc.tcl} {
			if {[file exists [env HOME]/$src]} {
				uplevel #0 source [env HOME]/$src
				break
			}
		}
	}
}

if {$tcl_platform(platform) eq "windows"} {
	set jim_argv0 [string map {\\ /} $jim_argv0]
}

_jimsh_init
