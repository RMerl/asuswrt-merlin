Asuswrt-Merlin - build 3.0.0.4.374.33 (xx-xxx-2013)
===================================================

About
-----
Asuswrt is the firmware developped by Asus for their newer routers.  They are 
also porting it to some of their older models, like the RT-56U and RT-N16.  
While originally based on Tomato-RT, Asus has disabled some of the 
original Tomato features, and added others.

Asuswrt-merlin is a customized version, which I am developping.
The goal is to do some bugfixes and minor enhancements to Asus's firmware, 
without targeting at full-blown advanced featuresets such as provided by 
excellent projects like Tomato or DD-WRT.  Some of the features 
that had been disabled by Asus have also been re-enabled.
This aims to be a more restrained alternative for those who prefer to stay 
closer to the original firmware, with limited risks of seeing new 
features bring in new stability issues.  I value stability over 
performance, and performance over features.



Supported Devices
-----------------
Supported devices are:
 * RT-N16
 * RT-N66U and RT-N66R
 * RT-AC66U and RT-AC66R
 * RT-AC56U


Features
--------
Here is a list of features that Asuswrt-merlin brings over the original firmware:

System:
   - Based on RT-AC56U 3.0.0.4.374_119 sources from Asus
   - Various bugfixes and optimizations
   - Some components were updated to their latest versions, for improved stability
     and security
   - Persistent JFFS partition
   - User scripts that run on specific events
   - Cron jobs
   - Customized config files for router services
   - LED control - put your Dark Knight in Stealth Mode by turning off all LEDs
   - Entware easy setup script (alternative to Optware - the two are mutually exclusive)
     (not available on RT-AC56U)

Disk sharing:
   - Enable/disable the use of shorter share names
   - Disk spindown after user-configurable inactivity timeout
   - NFS sharing (through webui)

Networking:
   - Force acting as a Master Browser
   - Act as a WINS server
   - SSHD
   - Allows tweaking TCP/UDP connection tracking timeouts
   - CIFS client support (for mounting remote SMB share on the router)
   - Layer7 iptables matching
   - User-defined options for WAN DHCP queries (required by some ISPs)
   - Improved NAT loopback (based on code from phuzi0n from the DD-WRT forums)
   - OpenVPN client and server, based on code originally written by
     Keith Moyer for Tomato and reused with his permission. (RT-N66U, RT-AC66U)
   - Option to control Spanning-Tree Protocol support.
   - Netfilter ipset module, for efficient blacklist implemetnation
   - Configurable IPv6 firewall

Web interface:
   - Improved client list, with DHCP hostnames
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring: added monthly, as well as per IP monitoring
   - Name field on the DHCP reservation list and Wireless ACL list
   - System info summary page
   - Wireless client IP, hostname, rate and rssi on the Wireless Log page
   - Wifi icon reports the state of both radios
   - Display the Ethernet port states
   - The various MAC/IP selection pulldowns will also display hostnames
     when possible instead of just NetBIOS names
   - Wireless site survey


A few features that first debuted in Asuswrt-Merlin have since been 
integrated/enabled in the official firmware:

- 64K NVRAM (RT-N66U)
- HTTPS
- Turning WPS button into a radio on/off toggle
- Use shorter share names (folder name only)
- WakeOnLan web interface (with user-entered preset targets)
- clickable MACs on the client list for lookup in the OUI database
- Display active/tracked network connections
- VPN Status page


Installation
------------
Simply flash it like any regular update.  You should not need to reset to 
factory defaults (see note below for one exception).  
You can revert back to an original Asus firmware at any time just
by flashing a firmware downloaded from Asus's website.

NOTE: If you were still running a 32KB nvram firmware on an RT-N66U (which 
usually mean an original firmware older than 3.0.0.4.220), the first time 
you flash a 64KB-enabled firmware (such as Asuswrt-merlin) it will 
wipe ALL your current settings and revert back to factory default!
This is required to upgrade the nvram storage to 64 KB.

It's also strongly recommended to reset to factory defaults if 
upgrading from 3.0.0.4.2xx to 3.0.0.4.3xx, to ensure that the 
new wireless driver used in these builds inherit the new 
low-level default values.


Usage
-----

* JFFS *
JFFS is a writable section of the flash memory which will allow you to store small 
files (such as scripts) inside the router without needing to have a USB disk 
plugged in.  This space will survive reboot (but it *MIGHT NOT survive 
firmware flashing*, so back it up first before flashing!).  It will also be 
available fairly early at boot (before USB disks).

To enable this option, go to the Administration page, under the System tab.

First time you enable JFFS, it must be formatted.  This can be done through 
the web page, same page where you enable it.  Enabling/Disabling/Formating 
JFFS requires a reboot to take effect.

I do not recommend doing frequent writes to this area, as it will 
prematuraly wear out the flash storage.  This is a good place to 
put files that are written once like scripts or kernel modules, or 
that rarely get written to (like once a day).  Storing files that 
constantly get written to (like logfiles) is NOT recommended - use
a USB disk for that.



* User scripts *
These are shell scripts that you can create, and which will be run when 
certain events occur.  Those scripts must be saved in /jffs/scripts/ 
(so, JFFS must be enabled and formatted).  Available scripts:

- services-start:  Services are started (boot)
- services-stop:  Services are stopped (reboot)
- wan-start:  WAN interface just come up (includes if it went down and back up)
              The WAN unit number will be passed as argument (0 = primary WAN)
- firewall-start:  Firewall is started (filter rules have been applied)
- nat-start: nat rules (i.e. port forwards and such) have been applied (nat table)
- init-start:  Right after jffs is mounted, before any of the services get started
- pre-mount:  Just before a partition is mounted.  Be careful with 
  this script.  This is run in a blocking call and will block the mounting of the 
  partition  for which it is invoked till its execution is complete. This is done 
  so that it can be used for things like running e2fsck on the partition before 
  mounting. This script is also passed the device path being mounted as an 
  argument which can be used in the script using $1.
- post-mount:  Just after a partition is mounted
- unmount: Just before unmounting a partition.  This is a blocking script, so be
  careful with it.  The mount point is passed as an argument to the script.
- dhcpc-event: Called whenever a DHCP event occurs on the WAN interface.
               The type of event (bound, release, etc...) is passed as an argument.
- openvpn-event: Called whenever an OpenVPN server gets started/stopped, or an OpenVPN
                 client connects to a remote server.  Uses the same syntax/parameters
                 as the "up" and "down" scripts in OpenVPN.

Don't forget to set them as executable:

   chmod a+rx /jffs/scripts/*

And like any Linux script, they need to start with a shebang:

   #!/bin/sh



* WakeOnLan *
There's a WOL tab under the new Tools menu.  From there you can enter a
target computer's MAC address to send it a WakeOnLan packet.  You can also
create a list of MAC addresses that will be stored in nvram, and on
which you can click afterward to wake up one of the listed computers, without 
having to remember their MAC addresses.



* SSHD *
SSH support (through Dropbear) was re-enabled.  Password-based login will use 
the same username and password as telnet/web access.  You can also optionally 
insert a RSA public key there for keypair-based authentication.  There 
is also an option to make ssh access available over WAN.



* Crond *
Crond will automatically start at boot time.  You can 
put your cron tasks in /var/spool/cron/crontabs/ .  The file 
must be named "admin" as this is the name of the system user.
Note that this location resides in RAM, so you would have to 
put your cron script somewhere such as in the jffs partition, 
and at boot time copy it to /var/spool/cron/crontabs/ using 
an init-start user script.

A simple way to manage your cron jobs is through the
included "cru" command.  Just run "cru" to see the 
usage information.  You can then put your "cru" 
commands inside a user script to re-generate your cron jobs 
at boot time.


* Enhanced Traffic monitoring *
Under Tools -> Other Settings are options that will allow you 
to save your traffic history to disk, preserving it between 
router reboots (by default it is currently kept in RAM, 
so it will disappear when you reboot).

You can save it to a custom location (for 
example, "/jffs/" if you have jffs enabled), or 
/mnt/sda1/ if you have a USB disk plugged in.
Save frequency is also configurable - it is recommended 
to keep that frequency lower (for example, once a day) 
if you are saving to jffs, to reduce wearing out 
your flash memory.  Make sure not to forget the trailing 
slash ad the end of the path.

Note that the first time you use that option, you must 
tell the router to create the data file.  Make sure you 
set "Create or reset data files" to "Yes".

Also, Asuswrt-Merlin can track the traffic generated 
by each individual IP on your network.  This option 
is called IPTraffic.  To enable this, you 
must first set a custom location to store your 
traffic database (see above).  Once again, you 
must also tell it to create the new data file, 
by enabling "Create or reset IPTraffic data files".
Once done, enable the IPTraffic Monitoring option.  
This will add three new entries to the Traffic Monitor 
page selector (on the Traffic Monitoring page).

You can optionally specify which IP to monitor, 
or exclude some IPs from monitoring.  Each IP 
must be separated by a comma.

It's strongly recommended that you assign a static 
IP to devices you wish to monitor to ensure they 
don't get a different IP over time, which would 
make the collected data somewhat unreliable.
The monitoring is done per IP, NOT per MAC.



* Display active NAT connections *
On the System Log -> Connections tab you can view the list 
of the currently tracked connections.  You can enable name 
resolution for IPs on the Tools menu,  under "Other Settings".  
Note that name resolution can slow down the loading of this 
page, especially if you have a lot of tracked connections 
(for instance while torrenting).



* Adjust TCP/IP connection tracking settings *
Under Tools -> Other Settings there are various parameters 
that lets you tweak the timeout values related to connection 
tracking for TCP and UDP connections.  You should be careful with 
those settings.  Most commonly, people will tweak the UDP timeout 
values to make them more VoIP-friendly, by using smaller timeouts.
Timeout values are in seconds.



* Mounting remote CIFS shares on the router *
You can mount remote SMB shares on your router.  The syntax will 
be something like this:

mount \\\\192.168.1.100\\ShareName /cifs1 -t cifs -o "username=User,password=Pass"

(backslashes must be doubled.)



* Dual WAN (EXPERIMENTAL) *
*** Available in separate, experimental builds *** 
*** This feature is still a work in progress from Asus ***

Asuswrt originally support using a USB 3G/4G modem as a 
failover Internet connection.  Dual WAN is the next step, also 
developped by Asus but left disabled so far in their official 
releases (probably because this is still work in progress).  

The first improvement over USB failover is that it works not only 
with USB but with other ethernet devices, which can be plugged 
on one of the LAN ports that you will select as the secondary WAN 
interface.  The second difference is that in addition to failover 
mode, Dual WAN also supports a load balancing mode, allowing 
you to share both connections at once.

Keep in mind that Dual WAN is still an EXPERIMENTAL and UNSUPPORTED 
feature, until the time Asus finishes developping and testing it.  
Some things are expected to not work properly.



* Disk Spindown when idle *
Jeff Gibbons's sd-idle-2.6 has been added to the firmware, allowing you 
to configure a timeout value (in seconds) on the Tools -> Other Settings 
page.  Plugged hard drives will stop spinning after being inactive 
for that specified period of time.  Note that services like Download Master 
might be generating background disk activity, preventing it from idling.



* OpenVPN (client and server) *
OpenVPN is an SSL-based VPN technology that is provided as a 
secure alternative to the PPTP VPN already included by Asus.
OpenVPN is far more secure and more flexible, however it is 
not as easy to configure, and requires the installation of 
a client software on your computer client.  The client 
can be obtained through this download page:

http://openvpn.net/index.php/open-source/downloads.html

Explaining the details of OpenVPN are beyond the scope of this 
documentation, and I am in no way an expert on OpenVPN.
Fortunately, there is a lot of available documentation and 
Howto guides out there.  I tried to stick to the same option 
descriptions as used by Tomato, so about any guide written 
for Tomato can easily be used to guide you on 
Asuswrt-Merlin.  For pointers, check the Wiki on the Asuswrt-Merlin 
Github repository.

You can provide your own custom client config files for the 
two server instances.  Store them in the 
/jffs/configs/openvpn/ccd1/ (and ccd2/) directory based 
on which server instance they belong to, with the 
filenames matching the client common names.
See the OpenVPN documentation for more details on the ccd directory.



* Customized config files *
You can append content to various configuration files that are 
created by the firmware, or even completely replace them with 
custom config files you have created.  Those config override 
files must be stored in /jffs/configs/.  To have a config 
file appended to the one created by the firmware, simply 
add ".add" at the end of the file listed below.  For 
example, /jffs/configs/dnsmasq.conf.add will be added at the 
end of the dnsmasq configuration file that is created by 
the firmware, while /jffs/configs/dnsmasq.conf would completely 
replace it.

Note that replacing a config file with your own implies that you 
properly fill in all the fields usually dynamically created by 
the firmware.  Since some of these entries require 
dynamic parameters, you might want to dynamically generate
the config file through an init-start script which 
could retrieve parameters from nvram, and insert them in 
your custom config.

The list of available config overrides:

- dnsmasq.conf
- vsftpd.conf
- pptpd.conf
- dhcp6s.conf
- hosts (for /etc/hosts)
- smb.conf
- minidlna.conf
- profile (shell profile, only profile.add suypported)
- upnp (for miniupnpd)
- radvd.conf
- fstab (only fstab supported, remember to create mount point
        through init-start first if it doesn't exist!)
- group, gshadow, passwd, shadow (only .add versions supported)
- exports (only exports.add supported)

Also, you can put OpenVPN ccd files in the following directories:

  /jffs/configs/openvpn/ccd1/
  /jffs/configs/openvpn/ccd2/

The content of these will be copied to their respective 
server instance's ccd directory when the server is started.


* NFS Exports *
In addition to SMB and FTP, you can now also share any plugged 
hard disk through NFS.  The NFS Exports interface can be accessed 
from the USB Applications section, under Servers Center.  
Click on the NFS Exports tab.

Select the folder you wish to export by clicking on the Path 
field.  Under Access List you can enter IPs/Networks to which 
you wish to give access.  A few examples:

  192.168.1.0/24 - will give access to the whole local network
  192.168.1.10 192.168.1.11 - will give access to the two IPs (separate with spaces)

Entering nothing will allow anyone to access the export.

Under options you can enter the export options, separated by 
a comma.  For example:

  rw,sync

For more info, search the web for documentation on the format of the 
/etc/exports file.  The same syntax for the access list and the options 
is used by the webui.

You can also manually generate an exports file by creating a file 
named /jffs/configs/exports.add , and entering your standard exports 
there.  They will be added to any exports configured on the webui.

Note that by default, only NFSv3 is supported.  You can also 
enable NFSv2 support from that page, but this is not recommended, 
unless you are using an old NFS client that doesn't support V3.
NFSv2 has various filesystem-level limitations.



* Repeater Mode support (Experimental build only) *
The RTN16 and RT-N66U experimental build also add  Repeater mode 
support.  Note that this is a feature that is still a 
work-in-progress by Asus, and might not be working 100% as 
expected yet.



* Easy Entware setup *
Entware is an alternative to Optware.  They are both online 
software repositories that let you easily install additional 
software to your router (such as an Apache web server, or 
an Asterisk PBX).  The main benefit of Entware over 
Optware (which is used by Asus for their own Download 
Master) is it is very actively maintained, with recent 
software versions.

Entware and Optware cannot be used 
at the same time however, so you can't use Download Master 
while using Entware.

There is now a script to make setting up Entware ware easier.  
Access your router through SSH/Telnet, and run 
"entware-setup.sh".

Note that Entware requires the JFFS partition to be enabled, 
and an ext2/ext3 formatted USB disk (NTFS and FAT32 are 
not supported).

Also note that Entware is not available for the 
RT-AC56U.



* YandexDNS filtering (Experimental builds only) *
Asus is implementing support for the YandexDNS DNS-based 
filtering.  This service allows you to filter out 
dangerous websites at the DNS level.  You can configure 
which computer will make use of this service if you 
want, for example, to only filter your children's 
computer.  The settings can be found under 
Parental Control.

This feature is a work-in-progress by Asus, so it is 
only available in the Experimental builds of this 
firmware at this time, and might not be 100% working 
yet.

For more information visit http://dns.yandex.ru/ .



Source code
-----------
The source code with all my modifications can be found 
on Github, at:

https://github.com/RMerl/asuswrt-merlin



History
-------
3.9.0.4.374.33 (xx-xxx-2013):
   - NEW: Merged with Asus 374_119 code from RT-AC56U GPL (newer than 374_164 from N66U)
   - CHANGED: Enabled Syn Cookies for ARM devices (RT-AC56U)
   - CHANGED: Allow selecting the Download2 folder for media server
              location.
   - CHANGED: MIPS builds optimized for mips32r2 code generation.
              A quick iperf+openvpn benchmark showed a very measurable
              performance improvement. (N16/N66/AC66)
   - FIXED: Samba binding to WAN interface would cause warnings
            about WINS/master browser (regression in 374)
   - FIXED: The ARM kernel was missing the Advanced IP Routing option,
            preventing some of the "ip" command functions from
            working (was breaking Astrill's plugin) (RT-AC56U)
   - FIXED: Build 374 fixed the Samba priority which was set too
            high, by setting it to the lowest possible, resulting in
            low average Samba performance (and even lower under higher
            CPU load).  Now Samba will run with a default priority of
            0, providing more balanced performance. (N16/N66/AC66)
  - FIXED: Some fields would allow invalid characters (such as
           single quotes) which might break the webui JS.  There might 
           still be a few unprotected fields.
  - FIXED: Memory leak in httpd service (Asus bug)


3.0.0.4.374.32 (24-Aug-2013):
   - NEW: Merged with Asus 374_168 GPL code.
   - NEW: wan-start script will get passed the WAN unit number as argument
   - NEW: Webui option to select the location of the DLNA database (patch by
          VinceV)
   - NEW: IPv6 firewalling.  Originally, Asuswrt would allow any IPv6 traffic 
          to be forwarded to your LAN devices.  This new option (enabled by
          default) will prevent traffic forwarding to LAN devices.  You can
          also create firewall rules to allow inbound traffic to specific 
          hosts.  The firewall configuration can be accessed through the 
          "Firewall -> IPv6 Firewall" page.
   - CHANGED: Upgraded OpenVPN to 2.3.2
   - CHANGED: Implemented IPTraffic support in DualWAN - Load balanced
              mode (Experimental builds)
   - CHANGED: Updated miniupnpd to 20130730
   - CHANGED: Updated some prebuilt binaries (RT-AC56U)
   - CHANGED: Updated 2.6.36 kernel to the latest code used
              in 372_184 (RT-AC56U), includes some changes
              related to USB3, and PPP/CTF.
   - CHANGED: Smarter location selection for the DLNA database
              location to reduce the chances of having it in
              RAM if left to default location, filling it up 
              (patch by VinceV)
   - CHANGED: Updated e2fsprogs to 1.42.8 to be in sync with Asus
   - FIXED: Web server would crash if you entered too much data in
            OpenVPN key/cert fields.
   - FIXED: The ACSD service could be exploited by a LAN user to
            gain shell access to the router.  TCP connections to
            ACSD are now blocked by the firewall.
   - FIXED: You could not define time periods on the Parental
            Control calendar under IE (backported from 374)
   - FIXED: Wireless client list would sometime return incorrect
            hostname or be missing IP.
   - FIXED: Security issue with Samba and symlinks


3.0.0.4.372.31_2 (28-July-2013):
   - FIXED: Samba wouldn't start due to missing symlink (RT-AC56U)


3.0.0.4.372.31 (24-July-2013:
   - NEW: Merged with 372_1393 code from Asus.  Notes:
      * Beamforming support for RT-AC66U/RT-AC56U
      * RT-N66U driver still downgraded to build 270 (which
        means no HW acceleration for PPP, but more reliable
        connectivity on the 5 GHz band)
      * Minidlna was updated to 1.1.0
      * AiCloud security hole fixed
      * Parental Control ui still broken under IE10 (use Fx or Chrome
        for now)

   - NEW: YandexDNS.  Asus is currently implementing support in the
          firmware for this DNS-based filter.  This can be
          found under Parental Control.  See http://dns.yandex.ru/
          for more info (go go Google translate!).
          (Experimental builds only)
   - NEW: User-provided client config files (ccd) for OpenVPN server.
          See the OpenVPN and Custom Config sections of the firmware's
          documentation for more info.
   - CHANGED: Connections list under System Log will now progressively
              display the result while the router is still
              resolving IPs (if that option was enabled).
   - CHANGED: OpenVPN client password hidden by default (and added
              checkbox to display it similar to what Asus added
              to the System page)
   - FIXED: Sysinfo page was reporting IPv6 as reason for
            CTF to be disabled - since 372 that is only true
            for ARM devices.
   - FIXED: OpenVPN Server in TAP mode + DHCP wasn't routing
            properly (DHCP was overruling the default GW)


3.0.0.4.372_30_3 (11-July-2013):
   - NEW: Added support for newest RT-N66U hardware revision.
          This router has a new model of flash, you can NOT
          use any older FW on these. (RT-N66U)


3.0.0.4.372.30_2 (7-July-2013):
      (note: since people always thought adding a "b" meant "beta' 
       rather than revision "b", I am switching to Asus's new 
       numbering scheme, hence "30_2" for this revised 372.30.)

   - FIXED: NAT loopback (invalid iptable rules was silently accepted
            by iptables)
   - FIXED: Removed empty Yandex tab
   - FIXED: Entware setup script missing from all builds
   - FIXED: pptpd failing to start (was missing from build)
   - FIXED: OpenVPN server not starting if using a static key
   - FIXED: Disks plugged to USB 2.0 port weren't getting mounted
            (RT-AC56U)


3.0.0.4.372.30 (5-July-2013):
   - NEW: Merged with preliminary 372 code provided by Asus
          (initialy meant for the ARM environment)
   - NEW: RT-AC56U support.  Various bugs have been fixed 
          over the original FW that initially shipped with these routers.
          Thanks to Asus for providing a development sample.
   - NEW: Added JFFS support to RT-AC56U.
   - CHANGED: Downgraded wireless driver + CTF to build 270 version
              (RT-N66U, fixes 5 GHz stability issues).  Note that this
              means that HW acceleration for PPPoE is no longer available
              for the RT-N66U, as it was new in the 5.110 SDK.
   - CHANGED: Updated iptables-1.4.x to 1.4.14 (RT-AC56U)
   - CHANGED: Brought back the Connection page under System Logs
   - CHANGED: Updated e2fsprogs to 1.42.7.  Amongst other things
              this new version is more memory-efficient on large
              filesystems.
   - CHANGED: Renamed Advanced (Per IP) Traffic monitoring for
              IPTraffic (to match the Tomato name for that same
              functionality)
   - FIXED: GRO kills upload speed if CTF is disabled (patch provided 
            by Asus, RT-AC56U)
   - FIXED: Buffer overrun in NVRAM handling, leading to random crashes
            (Asus bug, RT-AC56U)
   - FIXED: NVRAM values getting corrupted or disappearing if using more
            than 32 KB (Asus bug, RT-AC56U)
   - FIXED: Reapply layout fixes to Guest network and DHCP page (were lost
            in a recent webui update)
   - FIXED: JFFS2 could get reformated again at each subsequent reboots.
   - FIXED: Devices with a NetBIOS name of 15 chars long would have their
            name merged with the next device's.
   - FIXED: Empty Site Survey list if there was only one AP found
   - FIXED: Saved settings might fail to restore if they contained 
            OpenVPN or SSHD keys with CRLF line endings.  You should
            access the OpenVPN Keys page, click on Apply to re-save them,
            then re-create any backup you had of your router settings.
   - FIXED: Numerous bugs in ipt_account for Kernel 2.6.36 (RT-AC56U)


3.0.0.4.354.29 Beta 1 (17-May-2013):
   - KNOWN ISSUE: 5 GHz 40 MHz is unreliable with some wireless
                  cards (RT-N66U)
   - NEW: RT-N16 is no longer an experimentally supported device.
          Thanks to Mike from Sapphyre Software for providing
          me with an RT-N16.
   - NEW: Report currently used channels when mousing over
          the wifi icon at the top of the webui
   - NEW: Sysinfo: Ethernet port state will report each port's VLAN ID.
   - CHANGED: Merged with webui content from 3.0.0.4.364
   - CHANGED: Increased list height on Site Survey page
   - CHANGED: Warn if trying to do a site survey with either
              radios disabled.
   - CHANGED: Updated to miniupnpd 1.8.20130426
   - CHANGED: Updated to dropbear 2013.58
   - FIXED: Syslogd must be restarted if we had to adjust its log 
            level for DHCP query logging.
   - FIXED: br0 would change MAC address when starting an OpenVPN server 
            with a tap interface.
   - FIXED: Sysinfo: Port numbering order (RT-N16)
   - FIXED: Sysinfo: 5G radio infos weren't hidden if the 
            router did not support that band (RT-N16)
   - FIXED: Wifi status icon would remain half-lit if 2.4 GHz was
            disabled on a router without 5 GHz support (RT-N16)
   - FIXED: Various fixes related to site survey display
   - FIXED: Improved compatibility with USB disks > 2 TB
            (must use ext2 or ext3)
   - FIXED: Unable to clear DMZ IP (fixed in 364 webui files)
   - FIXED: PPTP/L2TP Internet connection unable to connect at boot 
            (bug introduced in 358.28)
   - FIXED: PPTP/L2TP Internet connection unable to reconnect after going 
            down (unsure if Asus bug or 358.28 bug)
   - FIXED: Numerous bugs in the Per IP traffic monitoring causing inaccurate
            traffic accounting if there was too much traffic, or if update 
            requests occured in a too short period of time.
   - FIXED: Asking for traffic monitoring (regular and Per IP) database to be 
            re-created would re-create it again on next reboot if in the mean time
            you didn't change any other settings, causing the flag not to be
            cleared on the mtd partition.


3.0.0.4.354.28 Beta 1 (19-Apr-2013):
   - KNOWN ISSUE: 5 GHz 40 MHz is unreliable with some wireless 
                  cards (RT-N66U)
   - KNOWN ISSUE: Sort order is sometimes wrong on the Site Survey page
   - NEW: Wireless site survey (on the Wireless tab)
   - NEW: openvpn-event user script that gets run when a tunnel goes up/down.
          Read the OpenVPN documentation on the "up" and "down" events for more
          on how to use this script, and to use the passed parameters.
   - NEW: Enabled sftp support in Dropbear (the sftp server must be
          installed from Entware)
   - NEW: Option to prevent SSH port hammering (patch submited by dodava)
   - CHANGED: Merged with webui pages extract from the Asus released FW, as
              they were more recent than the GPL ones
   - CHANGED: Port state on Sysinfo page now uses the new OUI lookup code
              from Asus
   - CHANGED: Try to report on Sysinfo what is forcing HW acceleration to
              be disabled
   - FIXED: Build 354 reduced minimum syslog level to WARNING - bumped back
            to INFO as in previous versions (resolves DHCP events not being
            logged).  Also ensured we readjusted it if DHCP logging is 
            enabled, to handle routers that got upgraded with the new
            loglevel already set.
   - FIXED: Port numbering on the Sysinfo page for devices that has 
            them backward (untested) (RT-N16)
   - FIXED: Client list wasn't using the new OUI code from Asus (was missing
            from the GPL archive)
   - FIXED: LAN traffic going through the NAT loopback would be counted in the 
            Per IP traffic monitoring.
   - FIXED: IE rendering of the Other Settings page when toggling Per IP monitoring
   - FIXED: Cannot set webui to HTTPS-only (causes port conflict error) (Asus bug in 354)
   - FIXED: Cannot create/modify folders in AiDisk
   - FIXED: Couldn't resolve LAN hostnames if WAN was down (the web redirection would
            hijack all DNS queries).  Now, we let dnsmasq handle both LAN and redirected 
            queries.
  - FIXED: Fixed support for Broadcom Wimax devices
  - FIXED: smbpasswd wasn't properly updated when deleting a user (Asus bug)
  - FIXED: Aicloud: handling of disks with multiple partitions on the webui (Asus bug) 
           (fix submitted by hshang)


3.0.0.4.354.27 Beta 1 (31-Mar-2013):
   - NEW: Merged with 3.0.0.4.354.  Notable changes:
            * New wireless driver
            * New Network Tools
            * WOL (Under Network Tools
            * HW acceleration support for PPPoE
            * DHCP Normal/aggressive behaviour.  Similar to the 270.25 implementation,
              except it can be enabled/disabled

          Asus considers build 354 to still be beta, so be advised that there might 
          still be some issues left (there are known issues related to 3G/4G dongles 
          for instance).

   - CHANGED: Removed WOL webui - Asus added their own WOL support  on the Network 
              Tools page.  You will have  to re-add your WOL entries.
   - CHANGED: Removed System Log -> Connections page, and integrated it into
              the new Network Tools -> Netstat page from Asus (as NAT Connections)
   - CHANGED: Removed wol binary, since Asus's WOL page uses ether-wake.
   - CHANGED: Removed option to control SIP helper on Firewall page (use
              the new Asus option from WAN - NAT PAssthrough page instead)
   - CHANGED: WPS button when set as a radio toggle will now behave the same 
              way as Asus's firmware: pressing it will fully enable/disable both 
              radios in the webui, rather than just toggle the state of the 
              enabled radios.  This means the button will override the 
              webui, and radio states will survive reboots.
   - FIXED: Avoid duplicate shares when using simpler share naming (using
            Asus's code from 354)
   - FIXED: Improved fdisk support for 4KB sector size
   - FIXED: openvpn: Client-specific entries weren't properly parsed
   - FIXED: dnsmasq warning in syslog if DHCP static leases are disabled


3.0.0.4.270.26b (17-Mar-2013):
   - FIXED: Volume labels with spaces were rejected (Asus used the same code
            to validate hostnames and volume labels)


3.0.0.4.270.26 (15-Mar-2013):
   - NEW: ipset Netfilter support + userspace tool to create ipset lists.
   - CHANGED: Router's hostname is now set all the time, regardless of
              telnet/ssh states (and including in AP mode)
   - CHANGED: Added device name field on the LAN page, since it's now relevant
              to the router's hostname (not just SMB).  Left it on the SMB
              page as well, for those used to see it there.
   - CHANGED: Router will supply its device name when requesting an IP while
              in AP mode.
   - CHANGED: Various webui lists were increased from 32 to 128 entries allowed.
   - CHANGED: Improved networkmap:
               * Will also use DHCP hostnames and user-defined static
                 names instead of just NetBIOS names
               * Client list will show an animation while networkmap is
                 still busy scanning and resolving device names
               * Dropdown menus that use Networkmap to build a list
                 of devices will also display names in addition to IP/MAC.
   - CHANGED: Don't restart the whole network if you only changed DHCP reservations
              (LAN -> DHCP page)
   - FIXED: Openvpn: Non-CBC ciphers weren't working (their use is still not 
            recommended)
   - FIXED: Proxy auto-configuration support (Asus bug)


3.0.0.4.270.25b (3-Mar-2013):
   - FIXED: Disabling DHCP logging would cause a syntax error in dnsmasq's configuration
            (regression from dnsmasq update)
   - FIXED: Outbound VPN client traffic was dropped (regression from firewall_2 fix)


3.0.0.4.270.25
   - NEW: NFS folder sharing.  Webui can be found on the USB Applications ->
          Servers Center page (NFS Exports tab)
   - NEW: dhcpc-event and zcip-event scripts (called on WAN events)
   - NEW: Ccustom configs: group.add, gshadow.add, passwd.add, shadow.add, exports.add
   - NEW: New script that will setup Entware for you (written by ryzhov_al).
          Run "entware-setup.sh" through SSH/Telnet to launch the install process.
   - CHANGED: Added a folder picker to the Tools Other Settings page to select
              a location to store your traffic data files.
   - CHANGED: Updated dnsmasq to 2.65 (backported from 3.0.0.4.334)
   - CHANGED: Enabled additional optimizations for openssl and openvpn for 
              a significant performance gain
   - CHANGED: Reverted wireless driver to build 220 (RT-AC66U only)
   - FIXED: Added missing badblocks program
   - FIXED: Timing issues under IE where resolved device names would 
            not display on certain pages (such as the Sysinfo page)
   - FIXED: VPN client "common name" wasn't getting saved
   - FIXED: DHCP client will be less aggressive in attempting to obtain
            a lease (wait 2 mins instead of 20 secs between attempts),
            should help with ISPs like Charter who will blacklist you 
            if you send too many Discovery packets in a short period of time.
   - FIXED: Made profile.add be run after any Optware profile, so the user
            changes will have priority over anything else.
   - FIXED: WOL list corruption when removing an entry in some browsers
   - FIXED: No longer forward packets with a LAN IP as destination
            (Asus bug, fixed CDRouter test firewall_2)
   - FIXED: IPv6 WAN would have the wrong prefix length (Asus bug, patch 
            submitted by PiotrKa)



3.0.0.4.270.24 (13-Feb-2013):
   - NEW: Rebased on 3.0.0.4.270.  Notable changes:
      o New driver builds (these are NOT the new major versions that
        Asus are still working on)
      o NTP-related changes
   - NEW: Report CTF (HW Acceleration) state on Sysinfo page.
   - NEW: Display Ethernet port states on the Sysinfo page.
   - NEW: Replaced Busybox fsck/mkfs tools with those from e2fsprogs, should
          be more reliable.
   - CHANGED: Temperatures on Sysinfo page will now auto-update every 3 seconds.
   - CHANGED: Connections page now uses Ajax for slightly better rendering
   - CHANGED: Improved name resolution on traffic monitor page, now uses
              a device's hostname if it reported one.
   - CHANGED: Client List now uses our improved name resolution code,
              will overwrite names with those entered on the DHCP static
              lease page.
   - CHANGED: Updated to OpenVPN 2.3.0 and lzo 2.06.
   - CHANGED: Updated Busybox to 1.20.2 (with Oleg/wl500g patches re-applied).
              Lots of fixes, including GPT support in fdisk.
   - CHANGED: Updated Miniupnpd to version 1.8.  NOTE: previous versions were NOT
              affected by the recent UPNP exploit disclosure.  This is just as
              an added security precaution.
   - FIXED: Temperature on Performance Tuning page would fail to update if a radio
            was disabled.
   - FIXED: Various timing issues causing some TrafficMonitoring and the Sysinfo
            pages to often fail loading under IE.
   - FIXED: JS error on the Per Device pages if FW failed to load the 
            traffic history.
   - FIXED: ebtables were still broken, fixed by a complete rebuild.
   - FIXED: Some OpenVPN fields rejected -1 as being valid.
   - FIXED: Hide 5G radio info from Sysinfo page if router is single band (RT-N16)
   - FIXED: Master Browser/WINS would not work if there was no USB disk plugged.
   - FIXED: Samba would bind to the WAN interface while in router mode (Asus bug)
   - FIXED: Backported various kernel fixes from Oleg/WL500G, Tomato and Kernel.org to 
            help improve HDD > 2 TB support (still not perfect, some USB enclosures
            are simply not Linux compatible)
   - FIXED: Display of Connections under IE
   - FIXED: Trying to apply settings on the System page with a username containing
            a non-alphanum would incorrectly assume you just tried to change to
            an account name that already existed (Asus bug).
   - FIXED: Wouldn't enable wins in Samba if you had a WINS IP entered on the
            DHCP configuration page.


3.0.0.4.266.23b (31-Dec-2012):
   - FIXED: The IE fix ended up breaking Firefox (and meanwhile, Chrome worked
            fine no matter which method was used to build that dropdown).


3.0.0.4.266.23 (31-Dec-2012):
   - NEW: Rebased on 3.0.0.4.266 (from the RT-AC66U GPL)
   - NEW: Tools icon contributed by Maximilian Czarnecki.
   - FIXED: Skip bad blocks while erasing MTD partition (fixes RT-AC66U
            failing to format JFFS2 partition due to bad blocks)
   - FIXED: Router would have no hostname if you enabled ssh but kept
            telnet disabled.
   - FIXED: Couldn't add new ebtables rules (regression in 264.22)
   - FIXED: customized minidlna.conf
   - FIXED: Traffic monitoring per IP is unreliable if hardware acceleration
            is enabled.  Do not load CTF if booting with cstats enabled.
   - FIXED: Per Device traffic monitor pages missing under IE


3.0.0.4.264.22 (15-Dec-2012):
   - NEW: Rebased on 3.0.0.4.264 (from the RT-N53 GPL).
   - NEW: Traffic monitoring per IP added to the Traffic Monitor section.
          Based on the Tomato IPTraffic implementation by Teaman.
   - NEW: Option to disable the Netfilter SIP helper (Firewall page), allows
          people to manually forward port 5060 to their own SIP server
   - NEW: Option to enable/disable logging DHCP client queries (LAN->DHCP page)
   - FIXED: Tabs would disappear while on the Monthly traffic page.
   - FIXED: Really fixed Firefox issue (the fix wasn't merged
            in release 260.21).
   - FIXED: Router crash if the list of MAC filters + their names got too long.
   - FIXED: OpenVPN webui: TLS Reneg and Connection Retry wouldn't let 
            you enter -1 as value.
   - FIXED: Layout issues on the DHCP page (one in Asus code, another in Merlin code)
   - FIXED: Beeline Corbina was unable to connect to PPTP/L2TP server due to DNS
            issues.
   - CHANGED: System log starts at the bottom (backported from GPL 314)
   - CHANGED: Dual WAN is no longer enabled in regular builds - too many issues 
              with it at this point.  Regular USB failover still works.


3.0.0.4.260.21 (5-Dec-2012):
   - NEW: Rebased on 3.0.0.4.260.  This version should
          resolve issues with some Russian ISPs.  Note that
          the RT-N66U build still uses the wireless driver
          from release 220, as this seems to be the most stable
          at this time.
   - NEW: Option to force the router into becoming the SMB Master Browser.
   - NEW: Option to make the router act as a WINS server.
   - NEW: Option to control Spanning-Tree Protocol
   - NEW: fstab custom config file
   - FIXED: Firefox compatibility issues on the DHCP static and 
            MAC filter name fields.
   - FIXED: Wifi status icon wasn't accurately reporting states if they
            were changed by a radio schedule.
   - FIXED: QIS would report newer firmwares, potentially overwriting
            Asuswrt-Merlin with an original Asus firmware.
   - FIXED: Wifi LEDs would turn back on if radios were enabled while
            in Stealth Mode (now they turn back off after a few seconds)
   - FIXED: Webui would break if a network device had an invalid
            NetBIOS name (such as the Sonos Dock).


3.0.0.4.246.20 (14-Nov-2012):
   - NEW: Wifi status icon will be half colored if only one radio is enabled.
   - NEW: Wifi status icon popup will report the state of each radios.
   - NEW: upnp custom config file for miniupnpd
   - NEW: unmount user script
   - NEW: led_ctrl and makemime (for use in conjunction with sendmail) applets.
   - NEW: Implemented control for network switch LEDs (all four at once)
   - NEW: Stealth Mode: option to disable all LEDs
   - NEW: Added CONFIG_IP_NF_RAW and CONFIG_NETFILTER_XT_TARGET_NOTRACK modules.
   - FIXED: Radio toggle through WPS button would be overriden by a scheduled
            radio.  Reverted "switch" to "toggle" code to prevent this.
   - FIXED: You couldn't disable DMZ by clearing the IP field.
   - FIXED: You couldn't edit entered text in DHCP/MAC/etc name field
   - FIXED: clientid passing for some ISPs requiring it (like Sky UK)
            was broken with the DHCP client change of build 220.
   - FIXED: No longer reboot the router three times during boot time if one 
            of the radios is disabled by the user. (RT-N66U)
   - FIXED: Changing the router login name to anything other than "admin"
            would prevent radvd, ecmh and the cru script from working 
            properly - they all assumed "admin".  Made then use
            http_username instead (which is tied to the superuser)
   - CHANGED: Improved SMB and vsftpd read performance by up to 30%


3.0.0.4.246.19b (26-Oct-2012):
   - FIXED: Reverted wireless driver to build 220 version as the new 
            one caused various connection issues for some (RT-N66U).


3.0.0.4.246.19 (23-Oct-2012):
   - NEW: Rebased on 3.0.0.4.246.  Some notable changes:
            o New "Enhanced interference management" option under Wireless -> Professional.
            o Improved AiCloud webui
            o dnsmasq updated to 2.64

   - NEW: Option to enable simpler share names.  When enabled, the folder
          Share will be shared as "Share" instead of "Share (on sda1)".
          The option can be found on the Misc tab, under USB Application.
   - NEW: User customized config files for various services.  Those custom
          config entries can either be appended, or completely replace the 
          config file generated by the firmware.
   - NEW: Added Name field to the Wireless ACL page.
   - NEW: Added service applet to rc.  For example, "service restart_samba" will 
          restart the Samba service.  For advanced usage/debugging only.
   - NEW: Backported OpenSSL ASM optimization from 1.0.1, for significant performance
          improvements in applications such as OpenVPN or SSH when using AES.
   - NEW: Report the current CFE/Bootloader version on the Sysinfo page.
   - FIXED: Minor tweaks to the AiCloud pages so they can fit on a 15" laptop screen
            (some close buttons at the bottom were unreachable)
   - FIXED: Enabling SSH access from WAN didn't work if DualWAN
            was set to load-balancing.
   - FIXED: Removed MAC Filter page, as it doesn't work (not compatible
            with Parental Control).
   - FIXED: OpenVPN Client "Username Auth only" option was broken.
   - FIXED: Limit valid characters in a DHCP/WOL description to prevent 
            breaking the webui by using invalid ones such as quotes.
   - FIXED: OpenVPN Client wasn't properly applying DNS settings that
            the server was pushing to us.
   - FIXED: Wireless client list alignment in AP mode.
   - CHANGED: Less strict rules when validating user-entered MAC hwaddr.



3.0.0.4.220.18b (25-Sept-2012):
   - NEW: Report both rx and tx rates on wifi connections
   - FIXED: Handle cases where the wireless driver returns a speed of -1
   - FIXED: Removed rssi retrieval retries, as it would make the first access to
            the wireless page take forever if you had multiple connected clients.
            You will have to manually refresh the page the first time you access it
            if the RSSI is reported as "??".


3.0.0.4.220.18 (23-Sept-2012):
   - NEW: Added OpenVPN logging verbosity setting (vpn_loglevel, must be
          manually set to a value between 0 and 15, with 3 being the default).
   - FIXED: Buffer overrun in init code that would crash the router when 
            too many features were enabled at compile time.
   - FIXED: Re-enabled DualWAN (RT-N66U, RT-AC66U)
   - FIXED: Re-enabled Beceem (Wimax) support in RT-AC66U.
   - FIXED: OpenVPN 'Start with WAN' and 'Respond to DNS' settings were 
            not properly saved.
   - FIXED: First time a client's rssi is polled it would return 0.
   - FIXED: post-mount user script wasn't executed (regression in 220.17)
   - CHANGED: Added some info to the OpenVPN server and client pages.
   - CHANGED: Improved load time of the VPN Status page.


3.0.0.4.220.17 (18-Sept-2012):
   - NEW: Rebased on 3.0.0.4.220, which includes:
            * Fixes to IPv6 6rd
            * Fixes to AC66U Wifi + QoS
            * AiCloud
            * Interference mode once again enabled
  - NEW: Display last received rate and rssi for each clients on Wireless Log page.
  - FIXED: dnsmasq not listening to DNS requests from OpenVPN clients
           if you had just enabled the option on the webui.
  - FIXED: PPTP clients not always showing on VPN Status page.
  - CHANGED: Disabled DualWAN as it's currently broken in 220.
  - CHANGED: Disabled Beceem Wimax support in RT-AC66U as it bricks
             the router.
  - CHANGED: Removed firmware update checker to avoid accidental
             revert to original FW.


3.0.0.3.178.16 Beta:
   - NEW: (RT-N66U, RT-AC66U) Implemented OpenVPN, based on code written by
          Keith Moyer (from the Tomato project).
   - NEW: Added crontab command
   - FIXED: (RT-AC66U) Would crash when accessing a LAN device through either 
            VPN or the NAT Loopback (GRO is now disabled for that device)
   - FIXED: dnsmasq was listening to all interfaces by default, allowing 
            even dhcp requests to be serviced from the wan side if you
            had the firewall disabled (Asus bug) (fixed by dev0id)
   - FIXED: Default disk idle spindown now set to 0 (disabled).
   - FIXED: Corrupted WOL list when using IE.
   - CHANGED: Upgraded openssl to 1.0.0j.
   - CHANGED: Included fully functional openssl command (will allow you to
              create keypairs and certificates from the router).
   - CHANGED: Removed power adjustments from the Performance page, as they
              are redundant, and not as reliable.
   - CHANGED: (RT-N16) Disabled Dual WAN, as it exhibited many issues, and I 
              am unable to work on them without an actual router.


3.0.0.3.178.15 (17-Aug-2012):
   - NEW: Rebased on 3.0.0.3.178.  Notable fixes by Asus:
           * Radio turns back on based on schedule
           * Reorganized QoS pages
           * Turning WAN DHCP connection off will first release current DHCP lease
   - NEW: RT-AC66U officialy supported, with all the same features as the RT-N66U.
   - NEW: (RT-AC66U) Implemented JFFS support.  Limiting partition to 32 MB
          max, as using the whole 90+ MB available makes little sense for 
          JFFS, and was also displaying some issues.
   - NEW: Added nat-start user script, as NAT rules get applied separately from
          other firewall rules (firewall-start changes to the nat table are 
          being overwritten when the router starts NAT)
   - NEW: Added additional info to Sysinfo page
   - NEW: Added chroot applet
   - NEW: Option to allow SSH access from WAN
   - NEW: Option to exclude specific devices from idle spindown
   - FIXED: Performance page now uses the new Sysinfo API, and is now able
            to deal with cases where radios are disabled.


3.0.0.3.162.14b:
   - FIXED: Web server would crash for some people when accessing
            the Wireless Log page.


3.0.0.3.162.14:
   - NEW: Spin down disks after (user-configurable) inactivity timeout
          (using Jeff Gibbons' sd-idle-2.6)
   - NEW: System information page under the Tools menu.
   - NEW: Station list on the Wireless Log page will now report associated
          IP and hostnames (when possible).
   - CHANGED: Upgraded to MiniDLNA 1.0.25 (changelog:
              http://sourceforge.net/projects/minidlna/files/minidlna/1.0.25/)
   - CHANGED: Better integration of the Run Cmd page.
   - FIXED: Incorrect left menu rendering when under the Tools menu.


3.0.0.3.162.13:
   - NEW: Rebased on 3.0.0.3.162.
   - CHANGED: Switched to WPS radio toggle code Asus added,
              now on the Administration -> System tab.


3.0.0.3.157.12 Beta:
This is based on unreleased Asus code, which they have 
graciously provided me with.

   - NEW: Rebased on 3.0.0.3.157.  Notable changes from Asus:
      . IPv6 tunnel memory leak fixed
      . They fixed many issues, making some of my patches 
        no longer necessary, such as timezone DST, https auth, etc...
      . Upgraded radvd
   - NEW: Added link to the command shell page in Tools menu.
   - NEW: (RT-N16) Enabled power settings (EXPERIMENTAL)
   - NEW: Added "tee" command.
   - FIXED: NAT loopback rules would actually NAT every lan to lan
            connections instead of only those needing the loopback
            (bug in Asus's code).  Replaced with new code based on a
            suggestion from Phuzi0n on the DD-WRT forums.
   - FIXED: Accessing the WOL page would make it resend the last
            WOL request.
   - FIXED: 'cru' was using 'root' instead of 'admin'
   - CHANGED: Re-enabled Dual WAN (EXPERIMENTAL)
   - CHANGED: Made tracked connections load async from rest of the page
   - CHANGED: Increased hostname width on Connection status page
   - CHANGED: Improved WOL page functionality.


3.0.0.3.144.11 Beta (6-July-2012):
   - NEW: Name field added to DHCP reservation list
   - NEW: Webui option to enable resolving IPs on the Connections tab
   - NEW: Store a list of computer MACs to use as WOL targets
   - CHANGED: Increased dhcp options from 32 to 128 characters
   - FIXED: Brought max PPTPD password lenght back to 32 chars (Asus had reduced
     it to 16 in recent versions)
   - FIXED: Retrieve dhcpc options for the correct wan interface


3.0.0.3.144.10 (30-June-2012):
   - NEW: Rebased on 3.0.0.3.144.
   - NEW: Support for 64K NVRAM enabled.  ***First flash will
          wipe out ALL your settings!  And you cannot restore 
          from saved settings - you must manually reconfigure 
          everything.  Be warned!***
   - NEW: Enabled support for Broadcom Wimax devices
   - NEW: Added cifs kernel module (for mounting remote SMB shares)
   - NEW: Added layer7 iptables matching
   - NEW: Added user-options for DHCP on the WAN page
   - FIXED: Router crashing when connecting to it over Wifi
            and running the newer QoS code (disabled GRO)
   - FIXED: Router crashing when connecting to a network 
            device behind the router from over a VPN
            connection (disabled GRO).
   - FIXED: Incorrect timezone set unless enabling
            manual DST.


3.0.0.3.130.9 (10-June-2012):
   - NEW: Enabled new Dual WAN support from Asus
   - FIXED: no-ip DDNS entry would revert to Asus DDNS on webui


3.0.0.3.130.8 (8-June-2012):
*** Reverting to factory defaults BEFORE and AFTER flashing
this version is strongly recommended!  The newer Asus code base 
seems to have changed quite a few settings, so you'll want to 
not only start with the new default values, but also get rid 
of obsolete settings.  Otherwise you will be wasting a 
good amount of the limited available nvram. ***

   - KNOWN ISSUE: Memory leak when using IPv6 (bug in Asus's code 
                  and/or kernel code)

   - KNOWN ISSUE: PPTP VPN can randomly reboot the router if accessing 
                  a LAN device behind the router.  Workaround is to 
                  use an IP range outside of the local LAN
                  (i.e. 10.0.0.0 instead of 192.168.1.0), and either 
                  set your VPN to use the VPN tunnel as default 
                  gateway, or manually add a route to your VPN 
                  client.

   - NEW: Rebased patches on 3.0.0.4.130 (RT-N53U sources).
          Build 130 brings various code changes to IPv6, not sure 
          what else (as I have no changelog between 112 and 130).
          The QoS code remains from build 108, as build 130 is 
          unstable.
   - NEW: Added "diff" utility
   - NEW: Keyword-based filter (new in 130)
   - FIXED: Firmware/settings can now be uploaded over HTTPS
            (bug fixed by Asus)
   - FIXED: Buffer overflow in networkmap that would cause garbled 
            device names to appear on the clists list (bug in
            Asus's code)
   - FIXED: Firewall would break when applying a game preset that 
            had multiple ports separated by a "," (bug in Asus's
            code)
   - FIXED: WOL through webui wasn't working when IPv6 is enabled
   - FIXED: Memory leak in sit.ko (backported from Linux 2.6.25.3)
   - IMPROVED: /jffs/scripts/ will be created automatically if it
               doesn't exist (you must still make any new script 
               executable using "chmod a+rx script_filename")


3.0.0.3.108.7 (27-May-2012):
   - NEW: Added no-ip.com support to DDNS (patch submitted by Igor Pavlov)
   - NEW: Added webui page under System Log to display active/tracked
          network connections.
   - NEW: Added netstat-nat command.
   - NEW: Added pre-mount and post-mount user scripts (patch submitted by 
          Shantanu Goel)
   - NEW: Allows tweaking TCP/UDP connection tracking timeouts
   - FIXED: Removed check in Asus's code that would reject txpower > 80
            unless you clicked three times on Apply (?!).
            NOTE: Still not sure power setting even works, as I get
            -80db from the other end of the house no matter if I use 
            40 or 500 mW.


3.0.0.3.108.6 (14-May-2012):
   - NEW: HTTP access list (backported from build 112)
   - NEW: PPTP VPN encryption options (backported from build 112)
   - FIXED: Traffic history location was't properly saved
            when changed in webui.
   - FIXED: Disabled traffic history saving to nvram for now,
            to avoid people accidentally filling their limited nvram space.
   - FIXED: Missing bottom pixels from the bottom of General menu
   - FIXED: Removed invalid CSS attribute
   - FIXED: typo in VPN iptables entries (bug in Asus's code)


3.0.0.3.108.5 (5-May-2012):
   - NEW: Crond starts at boot time.
   - NEW: init-start is a new user script that will be run early on
          at boot time (right after jffs is mounted, and before any 
          service gets started)
  - NEW: Can save traffic history to a custom location (USB or 
         JFFS, for instance) to preserve it between reboots.
  - NEW: Added Monthly traffic page (ported from Tomato)
  - NEW: Added the Performance Tuning page (with temperature).
  - FIXED: Webui authentication was bypassed by the web server (bug in
           Asus's code)
  - FIXED: Httpd crash when uploading a FW or settings file over
           https - should simply fail now.  For now you have to 
           use http for flashing the FW or restoring your settings
           from a saved config file.


3.0.0.3.108.4 (28-Apr-2012):
   - NEW: Clicking on the MAC address of an unidentified client will do a lookup in
          the OUI database (ported from DD-WRT).
   - NEW: Added HTTPS access to web interface (configurable under Administration)
   - NEW: Option to turn the WPS button into a radio on/off toggle (under Administration)
   - FIXED: sshd would start even if disabled
   - CHANGE: Switched back to wol, as people report better compatibility with it.
             ether-wake remains available over Telnet.


3.0.0.3.108.3 (18-Apr-2012):
   - NEW: JFFS support (mounted under /jffs)
   - NEW: services-start, services-stop, wan-start and firewall-start user scripts,
          must be located in /jffs/scripts/ .
   - NEW: SSHD support
   - IMPROVED: Fleshed out this documentation, updated Contact info with SNB forum URL
   - CHANGE: Removed wol binary, and switched to ether-wake (from busybox) instead.
   - CHANGE: Added "Merlin build" next to the firmware version on web interface.
   
          
3.0.0.3.108.2 (14-Apr-2012):
   - NEW: Added WakeOnLan web page

   
3.0.0.3.108.1 (5-Apr-2012):
   - Initial release.
   
   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://forums.smallnetbuilder.com/showthread.php?t=7047 as RMerlin)
Asus Forums (http://vip.asus.com/forum/topic.aspx?board_id=11&model=RT-N66U%20(VER.B1)&SLanguage=en-us) as RMerlin.
Website: http://www.lostrealm.ca/
Github: https://github.com/RMerl/asuswrt-merlin
Email: rmerl@lostrealm.ca
Twitter: https://twitter.com/RMerlinDev
Download: http://wwww.mediafire.com/asuswrt-merlin/

Development news will be posted on Twitter.  You can also keep a closer eye 
on development as it happens through the Github site.

For support questions, please use the SmallNetBuilder forums whenever possible.  There's a 
dedicated Asuswrt-Merlin sub-forum there, under the Asus Wireless section.

Drop me a note if you are using this firmware and are enjoying it.  If you really like it and want 
to give more than a simple "Thank you", there is also a Paypal donation button on my website.

I want to give my special thanks to Asus for showing an interest in this project, 
and also providing me with support when needed.  Also, thank you everyone who has 
donated through Paypal.  Much appreciated!



--- 
Eric Sauvageau
