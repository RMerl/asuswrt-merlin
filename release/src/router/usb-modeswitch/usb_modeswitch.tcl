#!/usr/bin/tclsh

# Wrapper (tcl) for usb_modeswitch, called from
# /lib/udev/rules.d/40-usb_modeswitch.rules
# (part of data pack "usb-modeswitch-data") via
# /lib/udev/usb_modeswitch
#
# Does ID check on newly discovered USB devices and calls
# the mode switching program with the matching parameter
# file from /usr/share/usb_modeswitch
#
# Part of usb-modeswitch-2.3.0 package
# (C) Josua Dietze 2009-2016

set arg0 [lindex $argv 0]
if [regexp {\.tcl$} $arg0] {
	if [file exists $arg0] {
		set argv [lrange $argv 1 end]
		source $arg0
		exit
	}
}

# Setting of these switches is done in the global config
# file (/etc/usb_modeswitch.conf) if available

set flags(logging) 1
set flags(noswitching) 0
set flags(stordelay) 0
set flags(logwrite) 0
# also settable in device config files
set flags(nombim) 0

# Execution starts at file bottom

proc {Main} {argv argc} {

global scsi usb config match device flags setup devdir loginit

set flags(config) ""
Log "[ParseGlobalConfig]"

# The facility to add a symbolic link pointing to the
# ttyUSB port which provides interrupt transfer, i.e.
# the port to connect through.
# Will check for interrupt endpoint in ttyUSB port (lowest if
# there is more than one); if found, return "gsmmodem[n]" name
# to udev for symlink creation

# This is run once for every port of LISTED devices by
# a udev rule

if {[lindex $argv 0] == "--symlink-name"} {
	puts -nonewline [SymLinkName [lindex $argv 1]]
	SafeExit
}

if {[lindex $argv 0] == "--switch-systemd"} {
	set argList [split [lindex $argv 1] _]
	Log "\nStarted via systemd"
} else {
	if {[lindex $argv 0] == "--switch-upstart"} {
		Log "\nStarted via upstart"
	}
	set argList [split [lindex $argv 1] /]
}
if [string length [lindex $argList 1]] {
	set device [lindex $argList 1]
} else {
	set device "noname"
}
if {$flags(stordelay) > 0} {
	SetStorageDelay $flags(stordelay)
}

Log "Raw args from udev: [lindex $argv 1]\n"

if {$device == "noname"} {
	Log "\nNo data from udev. Exit"
	SafeExit
}

if {![regexp -- {--switch-} [lindex $argv 0]]} {
	Log "\nNo command given. Exit"
	SafeExit
}

set setup(dbdir) /usr/share/usb_modeswitch
set setup(dbdir_etc) /etc/usb_modeswitch.d


if {![file exists $setup(dbdir)] && ![file exists $setup(dbdir_etc)]} {
	Log "\nError: no config database found in /usr/share or /etc. Exit"
	SafeExit
}
set bindir /usr/sbin

set devList1 {}
set devList2 {}


# arg 0: the bus id for the device (udev: %b), often ommitted
# arg 1: the "kernel name" for the device (udev: %k)
#
# Used to determine the top directory for the device in sysfs

set ifChk 0
if {[string length [lindex $argList 0]] == 0} {
	if {[string length [lindex $argList 1]] == 0} {
		Log "No device number values given from udev! Exit"
		SafeExit
	} else {
		if {![regexp {(.*?):} [lindex $argList 1] d dev_top]} {
			if {![regexp {([0-9]+-[0-9]+\.?[0-9]*.*)} [lindex $argList 1] d dev_top]} {
				Log "Could not determine device dir from udev values! Exit"
				SafeExit
			}
		}
	}
} else {
	set dev_top [lindex $argList 0]
	regexp {(.*?):} $dev_top d dev_top
}

set devdir /sys/bus/usb/devices/$dev_top
if {![file isdirectory $devdir]} {
	Log "Top device directory not found ($devdir)! Exit"
	SafeExit
}
Log "Use top device dir $devdir"

set iface 0
Log "Check class of first interface ..."
set config(class) [IfClass 0 $devdir]
if {$config(class) < 0} {
	Log " No access to interface 0. Exit"
	SafeExit
}
Log " Interface 0 class is $config(class)."

set ifdir [file tail [IfDir $iface $devdir]]
regexp {:([0-9]+\.[0-9]+)$} $ifdir d iface

set flags(logwrite) 1

# Mapping of the short string identifiers (in the config
# file names) to the long name used here
#
# If we need them it's a snap to add new attributes here!

set match(sVe) scsi(vendor)
set match(sMo) scsi(model)
set match(sRe) scsi(rev)
set match(uMa) usb(manufacturer)
set match(uPr) usb(product)
set match(uSe) usb(serial)


# Now reading the USB attributes
if {![ReadUSBAttrs $devdir]} {
	Log "USB attributes not found in sysfs tree. Exit"
	SafeExit
}
set config(vendor) $usb(idVendor)
set config(product) $usb(idProduct)


if $flags(logging) {
	Log "\n----------------\nUSB values from sysfs:"
	foreach attr {manufacturer product serial} {
		Log "  $attr\t$usb($attr)"
	}
	Log "----------------"
}

if $flags(noswitching) {
	SysLog "usb_modeswitch: switching disabled, no action for $usb(idVendor):$usb(idProduct)"
	Log "\nSwitching globally disabled. Exit"
	SafeExit
}

if {$usb(bNumConfigurations) == "1"} {
	set configParam "-u -1"
	Log "bNumConfigurations is 1 - don't check for active configuration"
} else {
	set configParam ""
}

# Check (and switch) for operating system if Huawei device present

set flags(os) "linux"
if {$usb(idVendor) == "12d1" && [regexp -nocase {android} [exec cat /proc/version]]} {
	set flags(os) "android"
}
if {$flags(os) == "android"} {
	set configList [ConfigGet conflist $usb(idVendor):#android]
} else {
	set configList [ConfigGet conflist $usb(idVendor):$usb(idProduct)]
}

if {[llength $configList] == 0} {
	Log "Aargh! Config file missing for $usb(idVendor):$usb(idProduct)! Exit"
	SafeExit
}
Log "ConfigList: $configList"

# Check if there is more than one config file for this USB ID,
# which would make an attribute test necessary. If so, check if
# SCSI values are needed

set scsiNeeded 0
if {[llength $configList] > 1} {
	if [regexp {:s} $configList] {
		set scsiNeeded 1
	}
}
if $scsiNeeded {
	if [ReadSCSIAttrs $devdir:$iface] {
		Log "----------------\nSCSI values from sysfs:"
		foreach attr {vendor model rev} {
			Log " $attr\t$scsi($attr)"
		}
		Log "----------------"
	} else {
		Log "Could not get SCSI attributes, exclude devices with SCSI match"
	}
} else {
	Log "SCSI attributes not needed, move on"
}

# General wait - some devices need this
after 500

# Now check for a matching config file. Matching is done
# by MatchDevice

set report {}
foreach mconfig $configList {

	# skipping installer leftovers like "*.rpmnew"
	if [regexp {\.(dpkg|rpm)} $mconfig] {continue}

	Log "Check config: $mconfig"
	if [MatchDevice $mconfig] {
		Log "! matched. Read config data"
#		set flags(config) $mconfig
		if [string length $usb(busnum)] {
			set busParam "-b [string trimleft $usb(busnum) 0]"
			set devParam "-g [string trimleft $usb(devnum) 0]"
		} else {
			set busParam ""
			set devParam ""
		}
		set flags(config) [ConfigGet conffile $mconfig]
		ParseDeviceConfig $flags(config)
		if [regexp -nocase {/[0-9a-f]+:#} $flags(config)] {
			Log "Note: Using generic manufacturer configuration for \"$flags(os)\""
		}
		if $flags(nombim) {
			set config(NoMBIMCheck) 1
		}
		if {$config(WaitBefore) != ""} {
			Log "Delay time of $config(WaitBefore) seconds"
			append config(WaitBefore) "000"
			after $config(WaitBefore)
			Log " wait is over, start mode switch"
		}
		if {$config(NoMBIMCheck)==0 && $usb(bNumConfigurations) > 1} {
			Log "Device may have an MBIM configuration, check driver ..."
			if [CheckMBIM] {
				Log " driver for MBIM devices is available"
				Log "Find MBIM configuration number ..."
				if [catch {set cfgno [exec /usr/sbin/usb_modeswitch -j -Q $busParam $devParam -v $usb(idVendor) -p $usb(idProduct)]} err] {
					Log "Error when trying to find MBIM configuration, switch to legacy modem mode"
				} else {
					set cfgno [string trim $cfgno]
					if {$cfgno > 0} {
						set config(Configuration) $cfgno
						set config(DriverModule) ""
						set flags(config) "Configuration=$cfgno"
					} else {
						Log " No MBIM configuration found, switch to legacy modem mode"
					}
				}
			} else {
				Log " no MBIM driver found, switch to legacy modem mode"
			}
		}
		if [PantechAutoSwitch] {
			Log "Waiting for Pantech auto-modeswitch"
			set report "ok:busdev"
			break
		}
		UnbindDriver $devdir $ifdir
		# Now we are actually switching
		if $flags(logging) {
			Log "Command to be run:\nusb_modeswitch -W -D $configParam $busParam $devParam -v $usb(idVendor) -p $usb(idProduct) -f \$flags(config)"
			set report [exec /usr/sbin/usb_modeswitch -W -D $configParam $busParam $devParam -v $usb(idVendor) -p $usb(idProduct) -f "$flags(config)" 2>@1]
			Log "\nVerbose debug output of usb_modeswitch and libusb follows"
			Log "(Note that some USB errors are to be expected in the process)"
			Log "--------------------------------"
			Log $report
			Log "--------------------------------"
			Log "(end of usb_modeswitch output)\n"
		} else {
			set report [exec /usr/sbin/usb_modeswitch -Q -D $configParam $busParam $devParam -v $usb(idVendor) -p $usb(idProduct) -f "$flags(config)" 2>@1]
		}
		break
	} else {
		Log "* no match, don't use this config"
	}
}

# Switching is complete; success checking was either
# done by usb_modeswitch and logged via syslog OR bus/dev
# parameter were used; then we do check for success HERE

if {$config(Configuration) != ""} {
	set ifdir [regsub {(\d):\d+\.0} $ifdir "\\1:$config(Configuration).0"]
}

if [regexp {ok:busdev} $report] {
	if [CheckSuccess $devdir] {
		Log "Mode switching was successful, found $usb(idVendor):$usb(idProduct) ($usb(manufacturer): $usb(product))"
		SysLog "usb_modeswitch: switched to $usb(idVendor):$usb(idProduct) on [format %03d $usb(busnum)]/[format %03d $usb(devnum)]"
	} else {
		Log "\nTarget config not matching - current values are"
		LogAttributes
		Log "\nMode switching may have failed. Exit"
		SafeExit
	}
} else {
	if {![file isdirectory $devdir]} {
		Log "Device directory in sysfs is gone! Something went wrong, abort"
		SafeExit
	}
	if {![regexp {ok:} $report]} {
		Log "\nCore program reported switching failure. Exit"
		SafeExit
	}
	# Give the device another second if it's not fully back yet
	if {![file exists $devdir/idProduct]} {
		after 1000
	}
	ReadUSBAttrs $devdir $ifdir
}

# Checking for bound drivers if there is an interface with class 0xff

if {$config(DriverModule) != "" && [regexp {ok:} $report]} {
	if [HasFF $devdir] {
		AddToList link_list $usb(idVendor):$usb(idProduct)
	} else {
		set config(DriverModule) ""
		Log " No vendor-specific class found, skip driver check"
	}
}

# If module is set (it is by default), driver shall be loaded.
# If not, then NoDriverLoading is active

if {$config(DriverModule) != ""} {
	if {[string length "$usb(idVendor)$usb(idProduct)"] < 8} {
		if {![regexp {ok:(\w{4}):(\w{4})} $report d usb(idVendor) usb(idProduct)]} {
			Log "No target vendor/product ID found or given, can't continue. Abort"
			SafeExit
		}
	}
	# wait for any drivers to bind automatically
	after 1500
	Log "Now check for bound driver ..."
	if {![file exists $devdir/$ifdir/driver]} {
		Log " no driver has bound to interface 0 yet"

		# If device is known, the sh wrapper will take care, else:
		if {[InBindList $usb(idVendor):$usb(idProduct)] == 0} {
			Log "Device is not in \"bind_list\" yet, bind it now"

			# Load driver
			CheckDriverBind $usb(idVendor) $usb(idProduct)

			# Old/slow systems may take a while to create the devices
			set counter 0
			while {![file exists $devdir/$ifdir/driver]} {
				if {$counter == 14} {break}
				after 500
				incr counter
			}
			if {$counter == 14} {
				Log " driver binding failed"
			} else {
				Log " driver was bound to the device"
				AddToList bind_list $usb(idVendor):$usb(idProduct)
			}
		}
	} else {
		Log " driver has bound, device is known"
		if {[llength [glob -nocomplain $devdir/$ifdir/ttyUSB*]] > 0} {
			AddToList link_list $usb(idVendor):$usb(idProduct)
		}
	}
} else {
	# Just in case "NoDriverLoading" was added after the first bind
	RemoveFromBindList $usb(idVendor):$usb(idProduct)
}

if [regexp {ok:$} $report] {
	# "NoDriverLoading" was set
	Log "No driver check or bind for this device"
}

# In newer kernels there is a switch to avoid the use of a device
# reset (e.g. from usb-storage) which would possibly switch back
# a mode-switching device to initial mode
if [regexp {ok:} $report] {
	Log "Check for AVOID_RESET_QUIRK kernel attribute"
	if [file exists $devdir/avoid_reset_quirk] {
		if [catch {exec echo "1" >$devdir/avoid_reset_quirk 2>/dev/null} err] {
			Log " Error setting the attribute: $err"
		} else {
			Log " AVOID_RESET_QUIRK activated"
		}
	} else {
		Log " not present in this kernel"
	}
}

Log "\nAll done, exit\n"
SafeExit

}
# end of proc {Main}


proc {ReadSCSIAttrs} {topdir} {

global scsi
set counter 0
set sysdir $topdir
Log "Check storage tree in sysfs ..."
while {$counter < 20} {
	Log " loop $counter/20"
	if {![file isdirectory $sysdir]} {
		# Device is gone. Unplugged? Switched by kernel?
		Log " sysfs device tree is gone; abort SCSI value check"
		return 0
	}
	# Searching the storage/SCSI tree; might take a while
	if {[set dirList [glob -nocomplain $topdir/host*]] != ""} {
		set sysdir [lindex $dirList 0]
		if {[set dirList [glob -nocomplain $sysdir/target*]] != ""} {
			set sysdir [lindex $dirList 0]
			regexp {.*target(.*)} $sysdir d subdir
			if {[set dirList [glob -nocomplain $sysdir/$subdir*]] != ""} {
				set sysdir [lindex $dirList 0]
				if [file exists $sysdir/vendor] {
					Log " Storage tree is ready"
					break
				}
			}
		}
	}
	after 500
	incr counter
}
if {$counter == 20} {
	Log "SCSI tree not found; you may want to check if this path/file exists:"
	Log "$sysdir/vendor\n"
	return 0
}

Log "Read SCSI values ..."
foreach attr {vendor model rev} {
	if [file exists $sysdir/$attr] {
		set rc [open $sysdir/$attr r]
		set scsi($attr) [read -nonewline $rc]
		close $rc
	} else {
		set scsi($attr) ""
		Log "Warning: SCSI attribute \"$attr\" not found."
	}
}
return 1

}
# end of proc {ReadSCSIAttrs}


proc {ReadUSBAttrs} {dir args} {

global usb

set attrList {idVendor idProduct bConfigurationValue manufacturer product serial devnum busnum bNumConfigurations}
set mandatoryList {idVendor idProduct bNumConfigurations}
set result 1
if {$args != ""} {
	lappend attrList "$args/bInterfaceClass"
	lappend mandatoryList "$args/bInterfaceClass"
}
foreach attr $attrList {
	if [file exists $dir/$attr] {
		set rc [open $dir/$attr r]
		set usb($attr) [string trim [read -nonewline $rc]]
		close $rc
	} else {
		set usb($attr) ""
		if {[lsearch $mandatoryList $attr] > -1} {
			set result 0
		}
		if {$attr == "serial"} {continue}
		Log "   Warning: USB attribute \"$attr\" not found"
	}
}
return $result

}
# end of proc {ReadUSBAttrs}


proc {MatchDevice} {config} {

global scsi usb match

set devinfo [file tail $config]
set infoList [split $devinfo :]
set stringList [lrange $infoList 2 end]
if {[llength $stringList] == 0} {return 1}

foreach teststring $stringList {
	if {$teststring == "?"} {return 0}
	set tokenList [split $teststring =]
	set id [lindex $tokenList 0]
	set matchstring [lindex $tokenList 1]
	set blankstring ""
	regsub -all {_} $matchstring { } blankstring
	Log "match $match($id)"
	Log "  string1 (exact):  $matchstring"
	Log "  string2 (blanks): $blankstring"
	Log " device string: [set $match($id)]"
	if {!([string match *$matchstring* [set $match($id)]] || [string match *$blankstring* [set $match($id)]])} {
		return 0
	}
}
return 1

}
# end of proc {MatchDevice}


proc {ParseGlobalConfig} {} {

global flags
set configFile ""
set places [list /etc/usb_modeswitch.conf /etc/sysconfig/usb_modeswitch /etc/default/usb_modeswitch]
foreach cfg $places {
	if [file exists $cfg] {
		set configFile $cfg
		break
	}
}
if {$configFile == ""} {return}

set rc [open $configFile r]
while {![eof $rc]} {
	gets $rc line
	if [regexp {^#} [string trim $line]] {continue}
	if [regexp {DisableMBIMGlobal\s*=\s*([^\s]+)} $line d val] {
		if [regexp -nocase {1|yes|true} $val] {
			set flags(nombim) 1
		} else {
			set flags(nombim) 0
		}
	}
	if [regexp {DisableSwitching\s*=\s*([^\s]+)} $line d val] {
		if [regexp -nocase {1|yes|true} $val] {
			set flags(noswitching) 1
		}
	}
	if [regexp {EnableLogging\s*=\s*([^\s]+)} $line d val] {
		if [regexp -nocase {1|yes|true} $val] {
			set flags(logging) 1
		} else {
			set flags(logging) 0
		}
	}
	if [regexp {SetStorageDelay\s*=\s*([^\s]+)} $line d val] {
		if [regexp {\d+} $val] {
			set flags(stordelay) $val
		}
	}

}
return "Use global config file: $configFile"

}
# end of proc {ParseGlobalConfig}


proc ParseDeviceConfig {cfg} {

global config
set config(DriverModule) ""
set config(DriverIDPath) ""
set config(WaitBefore) ""
set config(TargetVendor) ""
set config(TargetProduct) ""
set config(TargetClass) ""
set config(Configuration) ""
set config(NoMBIMCheck) 0
set config(PantechMode) 0
set config(CheckSuccess) 20
set loadDriver 1

foreach pname [lsort [array names config]] {
	if [regexp -line "^\[^# \]*?$pname.*?= *(0x(\\w+)|\"(\[0-9a-fA-F,\]+)\"|(\[0-9\]+)) *\$" $cfg d config($pname)] {
#		Log "config: $pname set to $config($pname)"
	}
}

if [regexp -line {^[^#]*?NoDriverLoading.*?=.*?(1|yes|true).*?$} $cfg] {
	set loadDriver 0
	Log "config: NoDriverLoading is set to active"
}

# For general driver loading; TODO: add respective device names.
# Presently only useful for HSO devices (which are recounted now)
if $loadDriver {
	if {$config(DriverModule) == ""} {
		set config(DriverModule) "option"
		set config(DriverIDPath) "/sys/bus/usb-serial/drivers/option1"
	} else {
		if {$config(DriverIDPath) == ""} {
			set config(DriverIDPath) "/sys/bus/usb/drivers/$config(DriverModule)"
		}
	}
	Log "Driver module is \"$config(DriverModule)\", ID path is $config(DriverIDPath)\n"
} else {
	Log "Driver will not be handled by usb_modeswitch"
}
set config(WaitBefore) [string trimleft $config(WaitBefore) 0]

}
# end of proc {ParseDeviceConfig}


proc ConfigGet {command config} {

global setup usb flags

switch $command {

	conflist {
		# Unpackaged configs first; sorting is essential for priority
		set configList [lsort -decreasing [glob -nocomplain $setup(dbdir_etc)/$config*]]
		set configList [concat $configList [lsort -decreasing [glob -nocomplain $setup(dbdir)/$config*]]]
		eval lappend configList [glob -nocomplain $setup(dbdir)/$usb(idVendor):#$flags(os)]
		if [file exists $setup(dbdir)/configPack.tar.gz] {
			Log "Found packed config collection $setup(dbdir)/configPack.tar.gz"
			if [catch {set packedList [exec tar -tzf $setup(dbdir)/configPack.tar.gz 2>/dev/null]} err] {
				Log "Error: problem opening config package; tar returned\n $err"
				return {}
			}
			set packedList [split $packedList \n]
			set packedConfigList [lsort -decreasing [lsearch -glob -all -inline $packedList $config*]]
			lappend packedConfigList [lsearch -inline $packedList $usb(idVendor):#$flags(os)]
			# Now add packaged configs with a mark, again sorted for priority
			foreach packedConfig $packedConfigList {
				lappend configList "pack/$packedConfig"
			}
		}
		return $configList
	}
	conffile {
		if [regexp {^pack/} $config] {
			set config [regsub {pack/} $config {}]
			Log "Extract config $config from collection $setup(dbdir)/configPack.tar.gz"
			set configContent [exec tar -xzOf $setup(dbdir)/configPack.tar.gz $config 2>/dev/null]
		} else {
			if [regexp [list $setup(dbdir_etc)] $config] {
				Log "Use config file from override folder $setup(dbdir_etc)"
				SysLog "usb_modeswitch: use overriding config file $config; make sure this is intended"
				SysLog "usb_modeswitch: please report any new or corrected settings; otherwise, check for outdated files"
			}
			set rc [open $config r]
			set configContent [read $rc]
			close $rc
		}
		return $configContent
	}
}

}
# end of proc {ConfigGet}

proc {Log} {msg} {

global flags device loginit

if {$flags(logging) == 0} {return}

if $flags(logwrite) {
	if [string length $loginit] {
		exec echo "\nUSB_ModeSwitch log from [clock format [clock seconds]]" >/var/log/usb_modeswitch_$device
		exec echo "$loginit" >>/var/log/usb_modeswitch_$device
		set loginit ""
	}
	exec echo $msg >>/var/log/usb_modeswitch_$device
} else {
	append loginit "\n$msg"
}

}
# end of proc {Log}


# Writing the log file and exit
proc {SafeExit} {} {

global flags
set flags(logwrite) 1
Log ""
exit

}
# end of proc {SafeExit}


proc {SymLinkName} {path} {
global device flags

proc {hasInterrupt} {ifDir} {
	if {[llength [glob -nocomplain $ifDir/ttyUSB*]] == 0} {
		Log "  no ttyUSB interface - skip endpoint check"
		return 0
	}
	foreach epDir [glob -nocomplain $ifDir/ep_*] {
		set e [file tail $epDir]
		Log "  check $e ..."
		if [file exists $epDir/type] {
			set rc [open $epDir/type r]
			set type [read $rc]
			close $rc
			if [regexp {Interrupt} $type] {
				Log "  $e has interrupt transfer type"
				return 1
			}
		}
	}
	return 0
}

set loginit "usb_modeswitch called with --symlink-name\n parameter: $path\n"

# In case the device path is returned as /class/tty/ttyUSB,
# get the USB device path from linked tree "device"
set linkpath /sys$path/device
if [file exists $linkpath] {
	if {[file type $linkpath] == "link"} {
		set rawpath [file readlink $linkpath]
		set trimpath [regsub -all {\.\./} $rawpath {}]
 		if [file isdirectory /sys/$trimpath] {
			append loginit "\n Use path $path\n"
			set path /$trimpath
		}
	}
}
if {![regexp {([0-9]+-[0-9]+[\.0-9]*:[^/]*).*(ttyUSB[0-9]+)} $path d myDev myPort]} {
	if $flags(logging) {
		set device [clock clicks]
		set flags(logwrite) 1
		Log "$loginit\nThis is not a ttyUSB port. Abort"
	}
	return ""
}

set device ttyUSB_$myDev
set flags(logwrite) 1
Log "$loginit\nMy name is $myPort\n"

if {![regexp {(.*?[0-9]+)\.([0-9]+)/ttyUSB} /sys$path d ifRoot ifNum]} {
	Log "Could not find interface in path\n $path. Abort"
	return ""
}

set ifDir $ifRoot.$ifNum

Log "Check my endpoints ...\n in $ifDir"
if [hasInterrupt $ifDir] {
	Log "\n--> I am an interrupt port"
	set rightPort 1
} else {
	Log "\n--> I am not an interrupt port\n"
	set rightPort 0
}

# There are devices with more than one interrupt interface.
# Assume that the lowest of these is usable. Check all
# possible lower interfaces

if { $rightPort && ($ifNum > 0) } {
	Log "\nLook for lower ports with interrupt endpoints"
	for {set i 0} {$i < $ifNum} {incr i} {
		set ifDir $ifRoot.$i
		Log " in ifDir $ifDir ..."
		if [hasInterrupt $ifDir] {
			Log "\n--> found an interrupt interface below me\n"
			set rightPort 0
			break
		}
	}
}
if {$rightPort == 0} {
	Log "Return empty name and exit"
	return ""
}

Log "\n--> No interrupt interface below me\n"

cd /dev
set idx 2
set symlinkName "gsmmodem"
while {$idx < 256} {
	if {![file exists $symlinkName]} {
		set placeholder [open /dev/$symlinkName w]
		close $placeholder
		break
	}
	set symlinkName gsmmodem$idx
	incr idx
}
if {$idx == 256} {return ""}

Log "Return symlink name \"$symlinkName\" and exit"
return $symlinkName

}
# end of proc {SymLinkName}


# Load and bind driver (default "option")
#
proc {CheckDriverBind} {vid pid} {
global config

foreach fn {/sbin/modprobe /usr/sbin/modprobe} {
	if [file exists $fn] {
		set loader $fn
	}
}
Log "Module loader is $loader"

set idfile $config(DriverIDPath)/new_id
if {![file exists $idfile]} {
	if {$loader == ""} {
		Log "Can't do anymore without module loader; get \"modtools\"!"
		return
	}
	Log "\nTry to load module \"$config(DriverModule)\""
	if [catch {set result [exec $loader -v $config(DriverModule)]} err] {
		Log " Running \"$loader $config(DriverModule)\" gave an error:\n  $err"
	} else {
		Log " Module was loaded successfully:\n$result"
	}
} else {
	Log "Module is active already"
}
set i 0
while {$i < 50} {
	if [file exists $idfile] {
		break
	}
	after 20
	incr i
}
if {$i < 50} {
	Log "Try to add ID to driver \"$config(DriverModule)\""
	SysLog "usb_modeswitch: add device ID $vid:$pid to driver \"$config(DriverModule)\""
	SysLog "usb_modeswitch: please report the device ID to the Linux USB developers!"
	if [catch {exec echo "$vid $pid ff" >$idfile} err] {
		Log " Error adding ID to driver:\n  $err"
	} else {
		Log " ID added to driver; check for new devices in /dev"
	}
} else {
	Log " \"$idfile\" not found, check if kernel version is at least 2.6.27"
	Log "Fall back to \"usbserial\""
	set config(DriverModule) usbserial
	Log "\nTry to unload driver \"usbserial\""
	if [catch {exec $loader -r usbserial} err] {
		Log " Running \"$loader -r usbserial\" gave an error:\n  $err"
		Log "No more fallbacks"
		return
	}
	after 50
	Log "\nTry to load driver \"usbserial\" with device IDs"
	if [catch {set result [exec $loader -v usbserial vendor=0x$vid product=0x$pid]} err] {
		Log " Running \"$loader usbserial\" gave an error:\n  $err"
	} else {
		Log " Driver was loaded successfully:\n$result"
	}
}

}
# end of proc {CheckDriverBind}


# Check if USB ID is listed as needing driver binding
proc {InBindList} {id} {

set listfile /var/lib/usb_modeswitch/bind_list
if {![file exists $listfile]} {return 0}
set rc [open $listfile r]
set buffer [read $rc]
close $rc
if [string match *$id* $buffer] {
Log "Found $id in bind_list"
	return 1
} else {
Log "No $id in bind_list"
	return 0
}

}
# end of proc {InBindList}

# Add USB ID to list of devices needing later treatment
proc {AddToList} {name id} {

set listfile /var/lib/usb_modeswitch/$name
if [file exists $listfile] {
	set rc [open $listfile r]
	set buffer [read $rc]
	close $rc
	if [string match *$id* $buffer] {
		return
	}
	set idList [split [string trim $buffer] \n]
}
lappend idList $id
set buffer [join $idList "\n"]
if [catch {set lc [open $listfile w]}] {return}
puts $lc $buffer
close $lc

}
# end of proc {AddToList}


# Remove USB ID from bind list (NoDriverLoading is set)
proc {RemoveFromBindList} {id} {

set listfile /var/lib/usb_modeswitch/bind_list
if [file exists $listfile] {
	set rc [open $listfile r]
	set buffer [read $rc]
	close $rc
	set idList [split [string trim $buffer] \n]
} else {
	return
}
set idx [lsearch $idList $id]
if {$idx > -1} {
	set idList [lreplace $idList $idx $idx]
} else {
	return
}
if {[llength $idList] == 0} {
	file delete $listfile
	return
}
set buffer [join $idList "\n"]
if [catch {set lc [open $listfile w]}] {return}
puts $lc $buffer
close $lc

}
# end of proc {RemoveFromBindList}


proc {CheckSuccess} {devdir} {

global config usb flags

# For Cisco AM10, target device not on same port
if {$usb(idVendor) == "1307" && $usb(idProduct) == "1169"} {
	set devdir [string range $devdir 0 end-1]2
}
set ifdir [file tail [IfDir 0 $devdir]]
if {[string length $config(TargetClass)] || [string length $config(Configuration)]} {
	set config(TargetVendor) $usb(idVendor)
	set config(TargetProduct) $usb(idProduct)
}
Log "Check success of mode switch for max. $config(CheckSuccess) seconds ..."

set expected 1
for {set i 1} {$i <= $config(CheckSuccess)} {incr i} {
	after 1000
	if {![file isdirectory $devdir]} {
		Log " Wait for device file system ($i sec.) ..."
		continue
	} else {
		Log " Read attributes ..."
	}
	set ifdir [IfDir 0 $devdir]
	if {$ifdir == ""} {continue}
	set ifdir [file tail $ifdir]
	if {![ReadUSBAttrs $devdir $ifdir]} {
		Log " Essential attributes are missing, continue wait ..."
		continue
	}
	if [string length $config(Configuration)] {
		if {$usb(bConfigurationValue) != $config(Configuration)} {continue}
	}
	if [string length $config(TargetClass)] {
		if {![regexp $usb($ifdir/bInterfaceClass) $config(TargetClass)]} {
			if {$config(class) != $usb($ifdir/bInterfaceClass} {
				set expected 0
			} else {continue}
		}
	}
	if {![regexp $usb(idVendor) $config(TargetVendor)]} {
		if {![regexp $usb(idVendor) $config(vendor)]} {
			set expected 0
		} else {continue}
	}
	if {![regexp $usb(idProduct) $config(TargetProduct)]} {
		if {![regexp $usb(idProduct) $config(product)]} {
			set expected 0
		} else {continue}
	}
	# Arriving here means that device attributes have changed
	if $expected {
		Log " All attributes matched"
	} else {
		if [regexp -nocase {/[0-9a-f]+:#} $flags(config)] {
			Log " idProduct has changed after generic mode-switch, assume success"
		} else {
			Log " Attributes are different but target values are unexpected:"
			LogAttributes
		}
	}
	break
}
if {$i > 20} {return 0} else {return 1}

}
# end of proc {CheckSuccess}


proc {IfDir} {iface devdir} {

set allfiles [glob -nocomplain $devdir/*]
set files [glob -nocomplain $devdir/*.$iface]
if {[llength $files] == 0} {
	return ""
}
set ifdir [lindex $files 0]
if {![file isdirectory $ifdir]} {
	return ""
}
return $ifdir

}
# end of proc {IfDir}

proc {IfClass} {iface devdir} {

set ifdir [IfDir $iface $devdir]

if {![file exists $ifdir/bInterfaceClass]} {
	return -1
}
set rc [open $ifdir/bInterfaceClass r]
set c [read $rc]
close $rc
return [string trim $c]

}
# end of proc {IfClass}


proc {SysLog} {msg} {

global flags
if {![info exists flags(logger)]} {
	set flags(logger) ""
	foreach fn {/bin/logger /usr/bin/logger} {
		if [file exists $fn] {
			set flags(logger) $fn
		}
	}
	Log "Logger is $flags(logger)"
}
if {$flags(logger) == ""} {
	Log "Can't add system message, no syslog helper found"
	return
}
catch {exec $flags(logger) -p syslog.notice "$msg" 2>/dev/null}

}
# end of proc {SysLog}

proc {SetStorageDelay} {secs} {

Log "Adjust delay for USB storage devices ..."
set attrib /sys/module/usb_storage/parameters/delay_use
if {![file exists $attrib]} {
	Log "Error: could not find delay_use attribute"
	return
}
if [catch {set ch [open $attrib r+]} err] {
	Log "Error: could not access delay_use attribute: $err"
	return
}
if {[read $ch] < $secs} {
	seek $ch 0 start
	puts -nonewline $ch $secs
	Log " Delay set to $secs seconds\n"
} else {
	Log " Current value is higher than $secs. Leave it alone\n"
}
close $ch

}
# end of proc {SetStorageDelay}

proc {CheckMBIM} {} {

set kversion [exec uname -r]
if [llength [glob -nocomplain /lib/modules/$kversion/kernel/drivers/net/usb/cdc_mbim*]] {return 1}
if [file exists /sys/bus/usb/drivers/cdc_mbim] {return 1}
return 0

}

proc {CheckQMI} {} {

set kversion [exec uname -r]
if [llength [glob -nocomplain /lib/modules/$kversion/kernel/drivers/net/usb/qmi_wwan*]] {return 1}
if [file exists /sys/bus/usb/drivers/cdc_mbim] {return 1}
return 0

}

proc {PantechAutoSwitch} {} {

global config flags
if {$config(PantechMode) == 3} {return 1}
if {$config(PantechMode) == 1} {
	if {"$config(vendor):$config(product)" == "10a9:6080"} {
		set flags(config) [regsub {PantechMode *= *1} $flags(config) "PantechMode=2"]
		Log " PantechMode changed to 2"
		return 0
	} elseif [CheckQMI] {
		set flags(config) [regsub {PantechMode *= *1} $flags(config) "PantechMode=4"]
		Log " PantechMode changed to 4"
		return 0
	} else {
		return 1
	}
} else {return 0}

}

proc UnbindDriver {devdir ifdir} {

set att $devdir/$ifdir/driver/unbind
if [file exists $att] {
	Log "Unbinding driver"
	exec echo -n "$ifdir" > $att
}

}

proc {LogAttributes} {} {

global flags usb
if $flags(logging) {
	set attrList {idVendor idProduct bConfigurationValue manufacturer product serial}
	foreach attr [lsort [array names usb]] {
		Log "    [format %-26s $attr:] $usb($attr)"
	}
}

}

proc {HasFF} {devdir} {

set i 0
while {[set dir [IfDir $i $devdir]] != ""} {
	set c [exec cat $dir/bInterfaceClass]
	if {$c == "ff"} {return 1}
	incr i
}
return 0

}


# The actual entry point
Main $argv $argc
