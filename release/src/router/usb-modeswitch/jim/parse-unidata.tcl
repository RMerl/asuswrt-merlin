#!/usr/bin/env tclsh

# Generate UTF-8 case mapping tables
#
# (c) 2010 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE for licence details.
#/

# Parse the unicode data from: http://unicode.org/Public/UNIDATA/UnicodeData.txt
# to generate case mapping tables

set f [open [lindex $argv 0]]
set extoff 0
puts "static const struct casemap unicode_case_mapping\[\] = \{"
while {[gets $f buf] >= 0} {
	foreach {code name class x x x x x x x x x upper lower} [split $buf ";"] break
	set code 0x$code
	if {$code <= 0x7f} {
		continue
	}
	if {$code > 0xffff} {
		break
	}
	if {$class ne "Lu" && $class ne "Ll"} {
		continue
	}
	if {$upper eq ""} {
		set upper $code
	} else {
		set upper 0x$upper
	}
	if {$lower eq ""} {
		set lower $code
	} else {
		set lower 0x$lower
	}
	if {$upper == $code && $lower == $code} {
		continue
	}
	set l [expr {$lower - $code}]
	set u [expr {$upper - $code}]
	if {abs($u) > 127 || abs($l) > 127} {
		# Can't encode both in one byte, so use indirection
		lappend jumptable $code $lower $upper
		set l -128
		set u $extoff
		incr extoff
		if {$extoff > 0xff} {
			error "Too many entries in the offset table!"
		}
	}
	set entry [string tolower "$code, $l, $u"]
	puts "    { $entry },"
}
close $f
puts "\};\n"

# Now the jump table
puts "static const struct caseextmap unicode_extmap\[\] = \{"
foreach {c l u} $jumptable {
	puts "    { $l, $u },"
}
puts "\};\n"
