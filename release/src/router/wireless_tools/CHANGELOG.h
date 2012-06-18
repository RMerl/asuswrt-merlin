/*
 *	Wireless Tools
 *
 *		Jean II - HPLB 97->99 - HPL 99->07
 *
 * The changelog...
 *
 * This files is released under the GPL license.
 *     Copyright (c) 1997-2002 Jean Tourrilhes <jt@hpl.hp.com>
 */

/* --------------------------- HISTORY --------------------------- */
/*
 * wireless 16 :		(Jean Tourrilhes)
 * -----------
 *	o iwconfig, iwpriv & iwspy
 *
 * wireless 17 :		(Justin Seger)
 * -----------
 *	o Compile under glibc fix
 *	o merge iwpriv in iwconfig
 *	o Add Wavelan roaming support
 *	o Update man page of iwconfig
 *
 * wireless 18 :
 * -----------
 *		(From Andreas Neuhaus <andy@fasta.fh-dortmund.de>)
 *	o Many fix to remove "core dumps" in iwconfig
 *	o Remove useless headers in iwconfig
 *	o CHAR wide private ioctl
 *		(From Jean Tourrilhes)
 *	o Create iwcommon.h and iwcommon.c
 *	o Separate iwpriv again for user interface issues
 *	  The folllowing didn't make sense and crashed :
 *		iwconfig eth0 priv sethisto 12 15 nwid 100
 *	o iwspy no longer depend on net-tools-1.2.0
 *	o Reorganisation of the code, cleanup
 *	o Add ESSID stuff in iwconfig
 *	o Add display of level & noise in dBm (stats in iwconfig)
 *	o Update man page of iwconfig and iwpriv
 *	o Add xwireless (didn't check if it compiles)
 *		(From Dean W. Gehnert <deang@tpi.com>)
 *	o Minor fixes
 *		(Jan Rafaj <rafaj@cedric.vabo.cz>)
 *	o Cosmetic changes (sensitivity relative, freq list)
 *	o Frequency computation on double
 *	o Compile clean on libc5
 *		(From Jean Tourrilhes)
 *	o Move listing of frequencies to iwspy
 *	o Add AP address stuff in iwconfig
 *	o Add AP list stuff in iwspy
 *
 * wireless 19 :
 * -----------
 *		(From Jean Tourrilhes)
 *	o Allow for sensitivity in dBm (if < 0) [iwconfig]
 *	o Formatting changes in displaying ap address in [iwconfig]
 *	o Slightly improved man pages and usage display
 *	o Add channel number for each frequency in list [iwspy]
 *	o Add nickname... [iwconfig]
 *	o Add "port" private ioctl shortcut [iwpriv]
 *	o If signal level = 0, no range or dBms [iwconfig]
 *	o I think I now got set/get char strings right in [iwpriv]
 *		(From Thomas Ekstrom <tomeck@thelogic.com>)
 *	o Fix a very obscure bug in [iwspy]
 *
 * wireless 20 :
 * -----------
 *		(From Jean Tourrilhes)
 *	o Remove all #ifdef WIRELESS ugliness, but add a #error :
 *		we require Wireless Extensions 9 or nothing !  [all]
 *	o Switch to new 'nwid' definition (specific -> iw_param) [iwconfig]
 *	o Rewriten totally the encryption support [iwconfig]
 *		- Multiple keys, through key index
 *		- Flexible/multiple key size, and remove 64bits upper limit
 *		- Open/Restricted modes
 *		- Enter keys as ASCII strings
 *	o List key sizes supported and all keys in [iwspy]
 *	o Mode of operation support (ad-hoc, managed...) [iwconfig]
 *	o Use '=' to indicate fixed instead of ugly '(f)' [iwconfig]
 *	o Ability to disable RTS & frag (off), now the right way [iwconfig]
 *	o Auto as an input modifier for bitrate [iwconfig]
 *	o Power Management support [iwconfig]
 *		- set timeout or period and its value
 *		- Reception mode (unicast/multicast/all)
 *	o Updated man pages with all that ;-)
 *
 * wireless 21 :
 * -----------
 *		(from Alan McReynolds <alan_mcreynolds@hpl.hp.com>)
 *	o Use proper macros for compilation directives [Makefile]
 *		(From Jean Tourrilhes)
 *	o Put licensing info everywhere (almost). Yes, it's GPL !
 *	o Document the use of /etc/pcmcia/wireless.opts [PCMCIA]
 *	o Add min/max modifiers to power management parameters [iwconfig]
 *		-> requested by Lee Keyser-Allen for the Spectrum24 driver
 *	o Optionally output a second power management parameter [iwconfig]
 *	---
 *	o Common subroutines to display stats & power saving info [iwcommon]
 *	o Display all power management info, capability and values [iwspy]
 *	---
 *	o Optional index for ESSID (for Aironet driver) [iwcommon]
 *	o IW_ENCODE_NOKEY for write only keys [iwconfig/iwspy]
 *	o Common subrouting to print encoding keys [iwspy]
 *	---
 *	o Transmit Power stuff (dBm + mW) [iwconfig/iwspy]
 *	o Cleaner formatting algorithm when displaying params [iwconfig]
 *	---
 *	o Fix get_range_info() and use it everywhere - Should fix core dumps.
 *	o Catch WE version differences between tools and driver and
 *	  warn user. Thanks to Tobias Ringstrom for the tip... [iwcommon]
 *	o Add Retry limit and lifetime support. [iwconfig/iwlist]
 *	o Display "Cell:" instead of "Access Point:" in ad-hoc mode [iwconfig]
 *	o Header fix for glibc2.2 by Ross G. Miller <Ross_Miller@baylor.edu>
 *	o Move header selection flags in Makefile [iwcommon/Makefile]
 *	o Spin-off iwlist.c from iwspy.c. iwspy is now much smaller
 *	  After moving this bit of code all over the place, from iwpriv
 *	  to iwconfig to iwspy, it now has a home of its own... [iwspy/iwlist]
 *	o Wrote quick'n'dirty iwgetid.
 *	o Remove output of second power management parameter [iwconfig]
 *	  Please use iwlist, I don't want to bloat iwconfig
 *	---
 *	o Fix bug in display ints - "Allen Miu" <aklmiu@mit.edu> [iwpriv]
 *
 * wireless 22 :
 * -----------
 *		(From Jim Kaba <jkaba@sarnoff.com>)
 *	o Fix socket_open to not open all types of sockets [iwcommon]
 *		(From Michael Tokarev <mjt@tls.msk.ru>)
 *	o Rewrite main (top level) + command line parsing of [iwlist]
 *		(From Jean Tourrilhes)
 *	o Set commands should return proper success flag [iwspy/iwpriv]
 *	  requested by Michael Tokarev
 *	---
 *		(From Torgeir Hansen <torgeir@trenger.ro>)
 *	o Replace "strcpy(wrq.ifr_name," with strncpy to avoid buffer
 *	  overflows. This is OK because the kernel use strncmp...
 *	---
 *	o Move operation_mode in iwcommon and add NUM_OPER_MODE [iwconfig]
 *	o print_stats, print_key, ... use char * instead if FILE * [iwcommon]
 *	o Add `iw_' prefix to avoid namespace pollution [iwcommon]
 *	o Add iw_get_basic_config() and iw_set_basic_config() [iwcommon]
 *	o Move iw_getstats from iwconfig to iwcommon [iwcommon]
 *	o Move changelog to CHANGELOG.h [iwcommon]
 *	o Rename iwcommon.* into iwlib.* [iwcommon->iwlib]
 *	o Compile iwlib. as a dynamic or static library [Makefile]
 *	o Allow the tools to be compiled with the dynamic library [Makefile]
 *	--- Update to Wireless Extension 12 ---
 *	o Show typical/average quality in iwspy [iwspy]
 *	o Get Wireless Stats through ioctl instead of /proc [iwlib]
 *
 * wireless 23 :
 * -----------
 *	o Split iw_check_addr_type() into two functions mac/if [iwlib]
 *	o iw_in_addr() does appropriate iw_check_xxx itself  [iwlib]
 *	o Allow iwspy on MAC address even if IP doesn't check [iwspy]
 *	o Allow iwconfig ap on MAC address even if IP doesn't check [iwconfig]
 *	---
 *	o Fix iwlist man page about extra commands [iwlist]
 *	---
 *	o Fix Makefile rules for library compile (more generic) [Makefile]
 *	---
 *	o Set max length for all GET request with a iw_point [various]
 *	o Fix set IW_PRIV_TYPE_BYTE to be endian/align clean [iwpriv]
 *	---
 *		(From Kernel Jake <kerneljake@hotmail.com>)
 *	o Add '/' at the end of directories to create them [Makefile]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Replace "cp" with "install" to get permissions proper [Makefile]
 *	o Install Man-Pages at the proper location [Makefile]
 *	o Add automatic header selection based on libc/kernel [iwlib.h]
 *	---
 *	o Add "commit" to force parameters on the card [iwconfig]
 *	o Wrap ioctl() in iw_set/get_ext() wrappers [all]
 *	o Beautify set request error messages [iwconfig]
 *
 * wireless 24 :
 * -----------
 *	o Added common function to display frequency [iwlib]
 *	o Added handler to parse Wireless Events [iwlib]
 *	o Added tool to display Wireless Events [iwevent]
 *	o Pass command line to subroutines [iwlist]
 *	o Scanning support through SIOCSIWSCAN [iwlist]
 *	---
 *	o Added common function to display bitrate [iwlib]
 *	o Add bitrate/encoding scanning support [iwlist]
 *	o Allow to read scan results for non-root users [iwlist]
 *	o Set 5s timeout on waiting for scan results [iwlist]
 *	o Cleanup iwgetid & support ap+scheme display [iwgetid]
 *	o iwevent man page [iwevent]
 *		(From Guus Sliepen <guus@warande3094.warande.uu.nl>)
 *	o iwgetid man page [iwgetid]
 *	---
 *	o Add "#define WIRELESS_EXT > 13" around event code [iwlib]
 *	o Move iw_enum_devices() from iwlist.c to iwlib.c [iwlib]
 *	o Use iw_enum_devices() everywhere [iwconfig/iwspy/iwpriv]
 *		(From Pavel Roskin <proski@gnu.org>, rewrite by me)
 *	o Return proper error message on non existent interfaces [iwconfig]
 *	o Read interface list in /proc/net/wireless and not SIOCGIFCONF [iwlib]
 *	---
 *		(From Pavel Roskin <proski@gnu.org> - again !!!)
 *	o Don't loose flags when setting encryption key [iwconfig]
 *	o Add <time.h> [iwevent]
 *	---
 *		(From Casey Carter <Casey@Carter.net>)
 *	o Improved compilations directives, stricter warnings [Makefile]
 *	o Fix strict warnings (static func, unused args...) [various]
 *	o New routines to display/input Ethernet MAC addresses [iwlib]
 *	o Correct my english & spelling [various]
 *	o Get macaddr to compile [macaddr]
 *	o Fix range checking in max number of args [iwlist]
 *	---
 *	o Display time when we receive event [iwevent]
 *	---
 *	o Display time before event, easier to read [iwevent]
 *		(From "Dr. Michael Rietz" <rietz@mail.amps.de>)
 *	o Use a generic set of header, may end header mess [iwlib]
 *		(From Casey Carter <Casey@Carter.net>)
 *	o Zillions cleanups, small fixes and code reorg [all over]
 *	o Proper usage/help printout [iwevent, iwgetid, ...]
 *	---
 *	o Send broadcast address for iwconfig ethX ap auto/any [iwconfig]
 *	---
 *	o Send NULL address for iwconfig ethX ap off [iwconfig]
 *	o Add iw_in_key() helper (and use it) [iwlib]
 *	o Create symbolink link libiw.so to libiw.so.XX [Makefile]
 *		(From Javier Achirica <achirica@ttd.net>)
 *	o Always send TxPower flags to the driver [iwconfig]
 *		(From John M. Choi <johnchoi@its.caltech.edu>)
 *	o Header definition for Slackware (kernel 2.2/glibc 2.2) [iwlib]
 *
 * wireless 25 :
 * -----------
 *	o Remove library symbolic link before creating it [Makefile]
 *	o Display error and exit if WE < 14 [iwevent]
 *		(From Sander Jonkers <sander@grachtzicht.cjb.net>)
 *	o Fix iwconfig usage display to show "enc off" [iwconfig]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Formating : add spaces after cell/ap addr [iwconfig]
 *	---
 *	o Do driver WE source version verification [iwlib]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Cleanup user configurable options [Makefile]
 *	o add FORCE_WEXT_VERSION [Makefile]
 *	o Add uninstall directived [Makefile]
 *	o Cleanup version warnings [iwlib]
 *	o Fix iwconfig usage display to show "mode MODE" [iwconfig]
 *	o Replace "rm -f + ln -s" with "ln -sfn" in install [Makefile]
 *	---
 *	o Add various documentation in source code of [iwpriv]
 *	o Allow to get more than 16 private ioctl description [iwlib]
 *	o Ignore ioctl descriptions with null name [iwpriv]
 *	o Implement sub-ioctls (simple/iw_point) [iwpriv]
 *	---
 *	o Add DISTRIBUTIONS file with call for help [README]
 *	o Change iw_byte_size in iw_get_priv_size [iwlib]
 *	o Document various bugs of new driver API with priv ioctls [iwpriv]
 *	o Implement float/addr priv data types [iwpriv]
 *	o Fix off-by-one bug (priv_size <= IFNAMSIZ) [iwpriv]
 *	o Reformat/beautify ioctl list display [iwpriv]
 *	o Add "-a" command line to dump all read-only priv ioctls [iwpriv]
 *	o Add a sample showing new priv features [sample_priv_addr.c]
 *	o Update other samples with new driver API [sample_enc.c/sample_pm.c]
 *	---
 *	o Fix "iwpriv -a" to not call ioctls not returning anything [iwpriv]
 *	o Use IW_MAX_GET_SPY in increase number of addresses read [iwspy]
 *	o Finish fixing the mess of off-by-one on IW_ESSID_MAX_SIZE [iwconfig]
 *	o Do interface enumeration using /proc/net/dev [iwlib]
 *	---
 *	o Display various --version information [iwlib, iwconfig, iwlist]
 *	o Filled in Debian 2.3 & Red-Hat 7.3 sections in [DISTRIBUTIONS]
 *	o Filled in Red-Hat 7.2, Mandrake 8.2 and SuSE 8.0 in [DISTRIBUTIONS]
 *	o Display current freq/channel after the iwrange list [iwlist]
 *	o Display current rate after the iwrange list [iwlist]
 *	o Display current txpower after the iwrange list [iwlist]
 *	o Add BUILD_NOLIBM to build without libm [Makefile]
 *	o Fix infinite loop on unknown events/scan elements [iwlib]
 *	o Add IWEVCUSTOM support [iwevent, iwlist]
 *	o Add IWEVREGISTERED & IWEVEXPIRED support [iwevent]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Make $(DYNAMIC_LINK) relative (and not absolute) [Makefile]
 *	---
 *	o Replace all float occurence with double [iwlib, iwlist]
 *	o Implement iwgetid --mode [iwgetid]
 *	o Convert frequency to channel [iwlist, iwlib]
 *		(Suggested by Pavel Roskin <proski@gnu.org> - always him !)
 *	o Implement --version across the board [iwspy, iwevent, iwpriv]
 *	o Implement iwgetid --freq [iwgetid]
 *	o Display "Access Point/Cell" [iwgetid]
 *	---
 *	o New manpage about configuration (placeholder) [wireless.7]
 *	o Catch properly invalid arg to "iwconfig ethX key" [iwconfig]
 *	o Put placeholder for Passphrase to key conversion [iwlib]
 *	o Allow args of "iwconfig ethX key" in any order [iwconfig]
 *	o Implement token index for private commands [iwpriv]
 *	o Add IW_MODE_MONITOR for passive monitoring [iwlib]
 *		I wonder why nobody bothered to ask for it before ;-)
 *	o Mention distribution specific document in [PCMCIA]
 *	o Create directories before installing stuff in it [Makefile]
 *	---
 *	o Add Debian 3.0 and PCMCIA in [wireless.7]
 *	o Add iw_protocol_compare() in [iwlib]
 *	---
 *	o Complain about version mistmatch at runtime only once [iwlib]
 *	o Fix IWNAME null termination [iwconfig, iwlib]
 *	o "iwgetid -p" to display protocol name and check WE support [iwgetid]
 *
 * wireless 26 :
 * -----------
 *	o #define IFLA_WIRELESS if needed [iwlib]
 *	o Update man page with SuSE intruction (see below) [wireless.7]
 *		(From Alexander Pevzner <pzz@pzz.msk.ru>)
 *	o Allow to display all 8 bit rates instead of 7 [iwlist]
 *	o Fix retry lifetime to not set IW_RETRY_LIMIT flag [iwconfig]
 *		(From Christian Zoz <zoz@suse.de>)
 *	o Update SuSE configuration instructions [DISTRIBUTIONS]
 *	---
 *	o Update man page with regards to /proc/net/wireless [iwconfig.8]
 *	o Add NOLIBM version of iw_dbm2mwatt()/iw_mwatt2dbm() [iwlib]
 *	---
 *	o Fix "iwconfig ethX enc on" on WE-15 : set buffer size [iwconfig]
 *	o Display /proc/net/wireless before "typical data" [iwspy]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix uninstall [Makefile]
 *	o Change "Encryption mode" to "Security mode" [iwconfig/iwlist]
 *	---
 *	o Add kernel headers that will be removed from wireless.h [iwlib]
 *	o Remove IW_MAX_GET_SPY, people should use AP-List [iwspy]
 *	o Re-word List of "Access Points" to "Peers/Access-Points" [iwlist]
 *	o Add support for SIOCGIWTHRSPY event [iwevent/iwlib]
 *	o Add support for SIOCSIWTHRSPY/SIOCGIWTHRSPY ioctls [iwspy]
 *	---
 *	o Add SIOCGIWNAME/Protocol event display [iwlist scan/iwevent]
 *	o Add temporary encoding flag setting [iwconfig]
 *	o Add login encoding setting [iwlib/iwconfig]
 *	---
 *	o Fix corruption of encryption key setting when followed by another
 *	  setting starting with a valid hex char ('essid' -> 'E') [iwlib]
 *	o Fix iw_in_key() so that it parses every char and not bundle of
 *	  two so that 'enc' is not the valid key '0E0C' [iwlib]
 *	o Fix parsing of odd keys '123' is '0123' instead of '1203' [iwlib]
 *	---
 *	o Implement WE version tool redirector (need WE-16) [iwredir]
 *	o Add "make vinstall" to use redirector [Makefile]
 *	o Fix compilation warning in WE < 16 [iwlib, iwspy]
 *	o Allow to specify PREFIX on make command line [Makefile]
 *	---
 *	o Update wireless.h (more frequencies) [wireless.h]
 *	o Allow to escape ESSID="any" using "essid - any" [iwconfig]
 *	o Updated Red-Hat 9 wireless config instructions [DISTRIBUTIONS]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Replace all %d into %i so we can input hex/oct [iwlib, iwpriv]
 *	---
 *	o If >= WE-16, display kernel version in "iwconfig --version" [iwlib]
 *		(From Antonio Vilei <francesco.sigona@unile.it>)
 *	o Fix "wrq.u.bitrate.value = power;" => txpower [iwconfig]
 *		(From Casey Carter <Casey@Carter.net>)
 *	o Make iwlib.h header C++ friendly. [iwlib]
 *	---
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Make sure that KERNEL_SRC point to a valid directory [Makefile]
 *	o Print message if card support WE but has no version info [iwlib]
 *		(From Simon Kelley <simon@thekelleys.org.uk>)
 *	o If power has no mode, don't print garbage [iwlib]
 *	---
 *		(Bug reported by Guus Sliepen <guus@sliepen.eu.org>)
 *	o Don't cast "int power" to unsigned long in sscanf [iwconfig]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Add $(LDFLAGS) for final linking [Makefile]
 *
 * wireless 27 :
 * -----------
 *	o Add 'sed' magic to automatically get WT/WE versions [Makefile]
 *	o Change soname of iwlib to libiwWE.so.WT [Makefile]
 *		Now dynamicaly linked versioned install can work
 *	o Default to dynamic build, don't build static lib [Makefile]
 *	o Update installation instructions [INSTALL]
 *	o fflush(stdout), so that redirect to file/pipe works [iwevent]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Display properly interface name larger than 8 char [all]
 *	---
 *	o Implement auto/fixed frequencies [iwconfig]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Don't fail is ldconfig fails [Makefile]
 *	---
 *	o Fix one forgotten strcpy(ifname) -> strncpy change [iwconfig]
 *	o Fix all dangerous sprintf, replace with snprintf [iwlib]
 *	o Change iw_print_xxx() API to take buffer length [iwlib]
 *	---
 *	o "iwspy ethX +" did not work, fix wrq.u.data.length [iwspy]
 *	o Fix stupid bug in displaying link/ap/cell stats [iwspy]
 *	o Fix display of fixed length char private args [iwpriv]
 *	o Add raw output for shell scripts, options -r [iwgetid]
 *	o Tweak scheme output for freq and mode [iwgetid]
 *		(From Andreas Mohr)
 *	o Spelling fixes in README and man page
 *	---
 *	o Add definitions for older version of W-Ext [iwlib]
 *	o Always force compile with latest local version of wext [Makefile]
 *	o Change soname of iwlib back to libiw.so.WT [Makefile]
 *	o Get rid of redirector and "make vinstall" [Makefile/iwredir]
 *	o Convert any struct iw_range to latest version [iwlib]
 *	o Change run-time version warning to reflect new reality [iwlib]
 *	o Remove compile-time version warning [iwlib]
 *	o Add iw_get_kernel_we_version() to guess kernel WE version [iwlib]
 *	o Remove all #ifdef WIRELESS_EXT, use dynamic iwrange version [all]
 *	o Get/display wireless stats based on iwrange version [iwlib]
 *	o Get power and retry settings based on iwrange version [iwconfig]
 *	o Display power and retry settings based on iwrange version [iwlist]
 *	o Optimise use of iwrange : read on demand [iwevent]
 *	---
 *	o #include <wireless.h>, instead of using a #define [iwlib.h]
 *	o Copy latest wireless.XX.h as wireless.h and install it [Makefile]
 *	---
 *	o Fix various iwlist retry display bugs [iwlist]
 *	o Fix dynamic link to libiw back to be libiw.so (doh !) [Makefile]
 *	---
 *	o Trivial cleanups and docs updates
 *	---
 *	o Implement "iwconfig XXX txpower on" and fix "fixed" [iwconfig]
 *	o Always properly initialise sanlen before recvfrom() [iwevent]
 *	o Zero buffer so we don't print garbage after essid [iwgetid]
 *	o Document that 00:00:00:00:00:00 == no association [iwconfig.8]
 *		(From Guus Sliepen <guus@sliepen.eu.org>)
 *	o Fix doc typo : ad_hoc => ad-hoc [wireless.7/DISTRIBUTIONS.txt]
 *	---
 *		(From vda <vda@port.imtp.ilyichevsk.odessa.ua>)
 *	o Accept arbitrary number of private definitions [iwlib/iwpriv]
 *	---
 *	o Added Hotplug documentation [HOTPLUG.txt]
 *	o Add dependancies (gcc way), remove makedepend [Makefile]
 *		(From Maxime Charpenne <maxime.charpenne@free.fr>)
 *	o Traduction en francais des pages manuel [fr/*]
 *	o Fix some incorrect/ambiguous sentences [iwconfig.8/iwevent.8]
 *		(From Joshua Kwan <joshk@triplehelix.org>)
 *	o Add 'const' qualifier to iwlib API [iwlib.c/iwlib.h]
 *		(From Joey Hess <joey@dragon.kitenet.net>)
 *	o Add Debian schemes scripts [debian/ifscheme*]
 *	---
 *	o Add 'ifrename', complete rewrite of nameif [ifrename]
 *	o Update documentation about ifrename [HOTPLUG.txt]
 *		(From Joshua Kwan <joshk@triplehelix.org>)
 *	o Fix disabling of key/enc with iw_set_basic_config() & WE<13 [iwlib.c]
 *	---
 *	o Various bug fixes and improvements [ifrename]
 *	---
 *	o Man pages for ifrename [ifrename.8/iftab.5]
 *	o Update hotplug/ifrename documentation [HOTPLUG.txt]
 *	---
 *	o Read configuration from stdin [ifrename]
 *		(From Thomas Hood <jdthood@yahoo.co.uk>)
 *	o Spell check and updated man page [wireless.7]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Update and spellcheck documentation [HOTPLUG.txt]
 *	---
 *	o Spin-off 'ifscheme' in a separate package to please Guus Sliepen
 *	o Update documentation on 'ifscheme' [DISTRIBUTIONS.txt/README]
 *		(From dann frazier <dannf@debian.org>)
 *	o Spell check and updated man page [iwlist.8]
 *	---
 *	o Cache interface static data (ifname/iwrange) [iwevent.c]
 *	---
 *	o Change the policy to delete entry from cache [iwevent.c]
 *	o If no TxPower in iwrange, still print current TxPower [iwlist.c]
 *	o Use iw_get_basic_config() in iwconfig, bloat-- [iwconfig.c/iwlib.h]
 *	---
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix mode boundary checking in iw_get_basic_config() [iwlib.c]
 *	---
 *	o Improved priv documentation [iwpriv.c]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix token index bug : allow zero args [iwpriv.c]
 *	o Grammar fixes in priv documentation [iwpriv.c]
 *	---
 *	o Make iw_protocol_compare() smarter [iwlib.c]
 *	o Display freq->channel conversion in scan results [iwlist]
 *	o Display freq->channel conversion in events [iwevent]
 *	o Interface name takeover support [ifrename]
 *	o Update docu for Debian Sarge, various improvements [HOTPLUG.txt]
 *	o Set wireless parameters in the proper order [iwlib]
 *	---
 *		(Suggested by Pavel Roskin <proski@gnu.org>)
 *	o Be less dramatic is driver doesn't export txpower info [iwlist]
 *	o Be less dramatic is driver doesn't export bitrate info [iwlist]
 *	o Use 'updated' bits to disable printing some qual [iwlib]
 *	o Change the way we show 'updated' bits -> '=' vs. ':' [iwlib]
 *	o Cosmetic update to channel display [iwlist/iwevent]
 *	---
 *	o Easy scanning API (blocking & non-blocking) [iwlib]
 *	o Add channel only support to iwgetid [iwgetid]
 *	o Compile iwgetid with iwlib for above [Makefile/iwgetid]
 *		(From Loic Minier <lool@dooz.org> via Guus Sliepen)
 *	o Fix french man pages to not use special 'oe' char [fr/*.8]
 *		(From Thomas Hood <jdthood@yahoo.co.uk>)
 *	o Use hyphens instead of underscores in Debian docs [*.txt/*.7]
 *	---
 *	o Update for WE-17 (wrq.u.freq.flags, IW_QUAL_*) [all]
 *	o Use iw_get_ext() instead of ioctl() [iwgetid]
 *	o Retry getting scan results with larger buffer [iwlist/iwlib]
 *	o Display wireless event capabilities [iwlist]
 *	o Add support for relative TxPower (yick !) [iwconfig/iwlist]
 *	o Create iw_print_txpower() [iwlib]
 *	o Add "Set" prefix for all SET wireless events [iwevent]
 *		(Suggested by Pavel Roskin <proski@gnu.org>)
 *	o Add support for get_freq and get_essid events [iwevent]
 *	---
 *	o Reorganise iw_print_freq() => create iw_print_freq_value() [iwlib]
 *	o Create iw_channel_to_freq() and use it [iwlib/iwconfig/iwevent]
 *	o Fixup for WE-18 : Set scan takes an iw_point [iwlist/iwlib]
 *	o Fixup for WE-19 : Take care of IW_EV_POINT_OFF [iwlib]
 *	---
 *	o Introduces iw_sockets_close() [all]
 *	o Set proper size on SIOCGIWSTATS requests [iwlib]
 *	o Use iw_print_freq_value() in iwlist [iwlist]
 *	o Optimise iw_print_bitrate() [iwlib]
 *	o Fix wrq.u.data.flags => wrq.u.txpower.flags [iwconfig]
 *		(From Denis Ovsienko <pilot@altlinux.ru>)
 *	o Add dry-run support (only print name changes) [ifrename]
 *	---
 *	o Move WE_VERSION/WT_VERSION to iwlib.h [iwlib/Makefile]
 *	o Add support for new selector pcmciaslot [ifrename]
 *	o Improve/cleanup DEBUG/verbose output [ifrename]
 *	o Minor documentation updates [HOTPLUG.txt/DISTRIBUTIONS.txt]
 *		(From Francesco Potorti` <pot@potorti.it>)
 *	o Allow iwgetid to accept '-c' options [iwgetid]
 *		(From Ian Gulliver <ian@penguinhosting.net>)
 *	o Transform #define DEBUG into verbose command line switch [ifrename]
 *	---
 *		(From Dan Williams <dcbw@redhat.com>)
 *	o Fix buffer memory leak in scanning [iwlib/iwlist]
 *	---
 *	o Make sure gcc inline stuff properly [iwlib.h]
 *	o Update Hotplug documentation [HOTPLUG.txt]
 *	o Add support for new selector firmware [ifrename]
 *
 * wireless 28 :
 * -----------
 *	o Fix gcc inline hack when using kernel headers [iwlib.h]
 *		(From Denis Ovsienko <pilot@altlinux.ru>)
 *	o Allow '-n' without '-i', even though inefficient [ifrename]
 *		(From Thomas Hood <jdthood@aglu.demon.nl>)
 *	o Fix technical and spelling errors in Hotplug doc [HOTPLUG.txt]
 *	---
 *	o Include wireless.h as a local file, not a system file [iwlib.h]
 *	o Split install targets [Makefile]
 *		(Suggested by Jost Diederichs <jost@qdusa.com>)
 *	o Increase scanning timeout for MadWifi [iwlib/iwlist]
 *		(Suggested by Ned Ludd <solar@gentoo.org>)
 *	o Multicall version of the tools for embedded [iwmulticall]
 *	---
 *	o Fix some install Makefile targets broken in pre2 [Makefile]
 *	---
 *	o Add option for stripping symbols on tools [Makefile]
 *	o Add escaping of essid keyworks with -- in manpage [iwconfig.8]
 *	o Update sensitivity description [iwconfig.8]
 *	o Fix iw_print_timeval() for timezone [iwlib/iwevent]
 *		(Suggested by Jan Minar <jjminar@FastMail.FM>)
 *	o Escape interface name for --help/--version with -- [iwconfig]
 *	o Allow --help/--version to be interface names [iwlist]
 *		(From Martynas Dubauskis <martynas@gemtek.lt>)
 *	o Fix invalid sizeof for stat memcpy in easy scanning API [iwlib.c]
 *		(From Andreas Mohr <andi@rhlx01.fht-esslingen.de>)
 *	o Fix my horrendous spelling [HOTPLUG.txt/PCMCIA.txt/README/*.8]
 *	---
 *	o Make static lib use PIC objects [Makefile]
 *	o Add SYSFS selector support to ifrename [ifrename]
 *	o Fix a fd leak in pcmciaslot selector [ifrename]
 *	o Don't complain about eth0/wlan0 if takeover enabled [ifrename]
 *	o Update man pages for sysfs and eth0/wlan0 [ifrename.8]
 *	o Update man pages for frequ auto/off [iwconfig.8]
 *	o More clever manual loading and docking tricks [HOTPLUG.txt]
 *		(From Pavel Heimlich tropikhajma@seznam.cz)
 *	o Czech (cs) man pages [cs/*]
 *	---
 *	o Fudge patch below for better integration [iwconfig/iwevent/iwlist]
 *		(From Jouni Malinen <jkmaline@cc.hut.fi>)
 *	o WE-18/WPA event display [iwevent]
 *	o WE-18/WPA parameter display [iwconfig]
 *	---
 *	o Replace iw_pr_ether() with iw_saether_ntop() [iwlib]
 *	o Replace iw_in_ether() with iw_saether_aton() [iwlib]
 *	o Remove iw_get_mac_addr() -> unused and bad API [iwlib]
 *	o Add iw_mac_ntop() and iw_mac_aton() for any-len mac addr [iwlib]
 *	o Slim down iw_ether_aton() using iw_mac_ntop() [iwlib]
 *	o Slim down iw_in_key(), avoid memcpy [iwlib]
 *	o Add support for any-len mac addr selector [ifrename]
 *	---
 *	o Re-add temp output buffer in iw_in_key() to avoid corruptions [iwlib]
 *	---
 *	o Add WE-19 headers, compile with that as default
 *	o IW_EV_POINT_LEN has shrunk, so invert IW_EV_POINT_OFF fixup [iwlib]
 *	o Remove WE backward compat from iwlib.h header [iwlib]
 *	o Add support for IW_QUAL_DBM in iw_print_stats() [iwlib]
 *	o Add support for ARPHRD_IEEE80211 in iw_check_mac_addr_type() [iwlib]
 *		-> iwspy work on wifi0 netdev from airo.c
 *	---
 *	o Set flags to 0 before asking old power settings for 'on' [iwconfig]
 *		(Suggested by Denis Ovsienko <pilot@altlinux.ru>)
 *	o Ignore empty lines in iface enumeration iw_enum_devices() [iwlib]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix invalid buffer size in 'iwlist power' [iwlist]
 *		(Suggested by Francesco Potorti` <pot@gnu.org>)
 *	o Remove kernel headers, use glibc headers [iwlib]
 *	---
 *	o Show explicit state for SIOCSIWAP, not numbers [iwconfig/iwevent] 
 *	o Add WE-18 ioctls to the stream parser in standard_ioctl_hdr [iwlib]
 *		(From Chris Hessing <Chris.Hessing@utah.edu>)
 *	o Add GENIE parsing support in scan resuls [iwlist]
 *	---
 *	o Change iw_extract_event_stream() API to add value index [iwlib]
 *	o Scan : display bitrate values on a single line [iwlist]
 *	---
 *	o Revert to previous iw_extract_event_stream() API, debloat [iwlib]
 *	o Keep track of value index in [iwlist]
 *	---
 *	o Check event stream 'point' payload size to avoid overflow [iwlib]
 *	o Make all users of event stream 'point' safe to NULL [iwlist/iwevent]
 *	o 'iwconfig txpower 1dBm' should not be 'mW' [iwconfig]
 *	o Forward compat. to WE-21 : essid len is strlen, not +1 [iwconfig]
 *	---
 *	o Forgot one place where essid len was strlen+1 [iwlib]
 *	o Update definition of 'ap' and 'sens' to reflect reality [man]
 *	---
 *	o Introduce WE_MAX_VERSION to take into account forward compat [iwlib]
 *	o Add WE-20 headers, compile with that as default
 *	---
 *	o Fix 'inline' for gcc-4 as well. Grrr... [iwlib]
 *
 * wireless 29 :
 * -----------
 *	o Add new power value : 'power saving' [iwconfig/iwlist/iwlib]
 *	o Optimise getting iwrange when setting TxPower [iwconfig]
 *	o Optimise displaying current power values (better loop) [iwlist]
 *	---
 *	o Add modulation bitmasks ioctls [iwconfig/iwlist]
 *	o Add short and long retries [iwconfig/iwlist/iwlib]
 *	o Fix 'relative' power saving to not be *1000 [iwconfig/iwlib]
 *	o iw_print_pm_value() require we_version [iwlib]
 *	o Optimise displaying range power values (subroutine) [iwlist]
 *	---
 *	o Fix 'relative' retry to not be *1000 [iwconfig/iwlib]
 *	o iw_print_retry_value() require we_version [iwlib]
 *	o Optimise getting iwrange when setting PowerSaving [iwconfig]
 *	o Optimise displaying current retry values (better loop) [iwlist]
 *	o Optimise displaying range retry values (subroutine) [iwlist]
 *	---
 *	o Fix stupid bug in displaying range retry values [iwlist]
 *	---
 *	o Add support for unicast and broadcast bitrates [iwconfig/iwlist]
 *	---
 *	o Replace spaghetti code with real dispatcher in set_info() [iwconfig]
 *		Code is more readable, maintainable, and save 700 bytes...
 *	o Drop 'domain' alias for 'nwid'. Obsolete. [iwconfig]
 *	o Make iw_usage() use dispatcher data instead of hardcoded [iwconfig]
 *	o Factor out modifier parsing for retry/power [iwconfig]
 *	o Fix iwmulticall to compile with new dispatcher above [iwmulticall]
 *	o Add WE_ESSENTIAL compile option to drop 10kB [Makefile]
 *	---
 *	o Update manpages with new features above [man]
 *	---
 *	o Add temp variable to sscanf() to fix 64 bits issues [iwconfig]
 *	o De-inline get_pm_value/get_retry_value to reduce footprint [iwlist]
 *	o Optimise iw_print_ie_cipher/iw_print_ie_auth [iwlist]
 *	o Add "Memory footprint reduction" section in doc [README]
 *	o Add 'last' scan option for left-over scan [iwlist]
 *		(From Stavros Markou <smarkou@patras.atmel.com>)
 *	o Add 'essid' scan option for directed scan [iwlist]
 *	---
 *		(Bug reported by Henrik Brix Andersen <brix@gentoo.org>)
 *	o Fix segfault on setting bitrate (parse wrong arg) [iwconfig]
 *	---
 *	o Revert 'CC=gcc' to normal [Makefile]
 *	o Integrate properly patch below [iwlist]
 *		(From Brian Eaton <eaton.lists@gmail.com>)
 *	o More WPA support : iwlist auth/wpakeys/genie [iwlist]
 *	---
 *	o Tweak man pages : interface is often optional [iwlist.8/iwspy.8]
 *	o Drop obsolete port/roam code from [iwpriv]
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix bug where all auth masks use iw_auth_capa_name [iwlist]
 *		(From Dima Ryazanov <someone@berkeley.edu>)
 *	o Fix iw_scan()/iw_process_scan() for non-root -> EPERM [iwlib]
 *		(Bug reported by Arkadiusz Miskiewicz <arekm@pld-linux.org>)
 *	o Fix "iwconfig nickname" (was abreviated) [iwconfig]
 *		(Bug reported by Charles Plessy)
 *	o Invalid mode from driver segfault iwlist scan [iwlist]
 *		(From Aurelien Jacobs <aurel@gnuage.org>)
 *	o Replace index() with strchr() [iwlib/iwconfig/iwpriv]
 *		(From Jens Thoms Toerring)
 *	o Parser/printf/sscanf fixes and optimisation [iwconfig]
 *	---
 *		(From Pavel Roskin <proski@gnu.org>)
 *	o Fix bug extracting mountpoint of sysfs (wrong field) [ifrename]
 *		(Suggested by Pavel Roskin <proski@gnu.org>)
 *	o Read sysfs symlinks transparently [ifrename]
 *	---
 *	o Fix README header to talk about ifrename [README]
 *	o Add 'prevname' selector for udev compatibility [ifrename]
 *	o Read parent directory names in SYSFS selector [ifrename]
 *	o Make dry-run output compatible with udev [ifrename]
 *	o Update man page with useful SYSFS selectors [iftab.5]
 *	---
 *	o Factorise wildcard rewrite '*'->'%d' to hide it from -D -V [ifrename]
 *	o Reorganise sysfs description, better wording [iftab.5]
 *		(Suggested by Pavel Roskin <proski@gnu.org>)
 *	o Enhance explanation of arp and iwproto [iftab.5]
 *	---
 *		(Bug reported by Johannes Berg <johannes@sipsolutions.net>)
 *	o Band-aid for the 64->32bit iwevent/iwscan issues [iwlib]
 *	---
 *	o Better band-aid for the 64->32bit iwevent/iwscan issues [iwlib]
 *		(Suggested by Kay Sievers <kay.sievers@vrfy.org>)
 *	o Add udev compatible output, print new DEVPATH [ifrename]
 *	---
 *	o Fix DEVPATH output to use the real devpath from udev [ifrename]
 *	o Add udev rules for ifrename integration [19-udev-ifrename.rules]
 *	---
 *	o Add largest bitrate in easy scan API [iwlib]
 *	---
 *	o Debug version : output IW_EV_LCP_LEN [iwlist]
 *	---
 *		(Bug reported by Santiago Gala/Roy Marples)
 *	o Fix 64->32bit band-aid on 64 bits, target is local aligned [iwlib]
 *	---
 *		(Bug reported by Santiago Gala/Roy Marples)
 *	o More fix to the 64->32bit band-aid on 64 bits [iwlib]
 *	---
 *		(Bug reported by Dimitris Kogias)
 *	o Fix GENIE parsing os chipher/key_mngt [iwlist]
 *		(Bug reported by Guus Sliepen <guus@debian.org>)
 *	o Compiler warning on DEBUG code [iwlist]
 *	---
 *	o --version output WE_MAX_VERSION instead of WE_VERSION [iwlib]
 *	o Change iwstats dBm range to [-192;63] in iw_print_stats() [iwlib.c]
 *	o Implement iwstats IW_QUAL_RCPI in iw_print_stats()  [iwlib.c]
 *		(Bug reported by Guus Sliepen <guus@sliepen.eu.org>)
 *	o LINUX_VERSION_CODE removed, only use GENERIC_HEADERS [iwlib.h]
 *		(Bug reported by Johan Danielsson <joda11147@gmail.com>)
 *	o Fix OUI type check for WPA 1 IE [iwlist.c]
 *	---
 *		(Bug reported by Florent Daignière)
 *	o Don't look for "fixed" out of array in set_txpower_info() [iwconfig]
 */

/* ----------------------------- TODO ----------------------------- */
/*
 * One day, maybe...
 *
 * iwconfig :
 * --------
 *	Make disable a per encryption key modifier if some hardware
 *	requires it.
 *	IW_QUAL_RCPI
 *
 * iwspy :
 * -----
 *	Add an "auto" flag to have the driver cache the last n results
 *
 * iwlist :
 * ------
 *	Add scanning command line modifiers
 *	More scan types support
 *
 * ifrename :
 * --------
 *	Link Type should use readable form instead of numeric value
 *
 * Doc & man pages :
 * ---------------
 *	Update main doc.
 */
