Asuswrt-Merlin - build 378.50 beta 3 (xx-Feb-2015)
==================================================

About
-----
Asuswrt is the name of the common firmware Asus has developed 
for their various router models.  Originally forked from 
Tomato, it has since grown into a very different product, removing 
some more technical features that were part of Tomato, but 
also adding new original features such as support for dual WANs.

Asuswrt-merlin is a customized version of Asus's firmware. The goal is 
to provide bugfixes and minor enhancements to Asus's firmware, with also 
a few occasional feature additions.  This is done while retaining 
the look and feel of the original firmware, and also ensuring that 
the two codebases remain close enough so it will remain possible 
to keep up with any new features brought by Asus in the original firmware.

This project's goal is NOT to develop yet another firmware filled with 
many features that are rarely used by home users - that is already covered 
by other excellent projects such as Tomato or DD-WRT.

This more conservative approach will also help ensuring the highest 
level of stability possible.  Priority is given to stability over 
performance, and performance over features.



Supported Devices
-----------------
Supported devices are:
 * RT-N16
 * RT-N66U
 * RT-AC66U
 * RT-AC56U
 * RT-AC68U
 * RT-AC68P
 * RT-AC87U

NOTE: all the "R" versions (for example RT-N66R) are the same as their 
"U" counterparts, they are just different packages aimed at large 
retailers.  The firmware is 100% compatible with both U and R versions 
of the routers.  Same with the "W" variants that are simply white.


Features
--------
Here is a list of features that Asuswrt-merlin adds over the original 
firmware:

System:
   - Based on 3.0.0.4.378_3913 source code from Asus
   - Various bugfixes and optimizations
   - Some components were updated to newer versions, for improved
     stability and security
   - User scripts that run on specific events
   - Cron jobs
   - Ability to customize the config files used by the router services
   - LED control - put your router in "Stealth Mode" by turning off 
     all LEDs
   - Entware easy setup script (alternative to Optware - the two are 
     mutually exclusive) (not available on RT-AC56/RT-AC68/RT-AC87)
   - SNMP support (based on experimental code from Asus)


Disk sharing:
   - Enable/disable the use of shorter share names
   - Disk spindown after user-configurable inactivity timeout
   - NFS sharing (through webui)
   - Allow or disable WAN access to the FTP server
   - Updated Samba version (3.6)


Networking:
   - Force acting as a Master Browser
   - Act as a WINS server
   - Allows tweaking TCP/UDP connection tracking timeouts
   - CIFS client support (for mounting remote SMB share on the router)
   - Layer7 iptables matching (N16/N66/AC66 only)
   - User-defined options for WAN DHCP queries (required by some ISPs)
   - Advanced OpenVPN client and server support (all models except 
     RT-N16)
   - Netfilter ipset module, for efficient blacklist implementation
   - Configurable min/max UPNP ports
   - IPSec kernel support (N16/N66/AC66 only)
   - DNS-based Filtering, can be applied globally or per client
   - Custom DDNS (through a user script)

Web interface:
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring: added monthly, as well as per IP 
     monitoring
   - Name field on the DHCP reservation list and Wireless ACL list
   - System info summary page
   - Wireless client IP and hostname on the Wireless Log page
   - Wifi icon reports the state of both radios
   - Display the Ethernet port states
   - Wireless site survey


A few features that first appeared in Asuswrt-Merlin have since been 
integrated/enabled/re-implemented in the official firmware:

- 64K NVRAM for the RT-N66U
- HTTPS webui
- Turning WPS button into a radio on/off toggle
- Use shorter share names (folder name only)
- WakeOnLan web interface (with user-entered preset targets)
- clickable MACs on the client list for lookup in the OUI database
- Display active/tracked network connections
- VPN client connection state report
- DualWAN and Repeater mode (while it was still under development
  by Asus)
- OpenVPN client and server
- Configurable IPv6 firewall
- Persistent JFFS partition
- The various MAC/IP selection pulldowns will also display hostnames
  when possible instead of just NetBIOS names
- SSHD
- Improved compatibility with 3TB+ and Advanced Format HDDs



Installation
------------
Simply flash it like any regular update.  You should not need to 
reset to factory defaults (see note below for exceptions).
You can revert back to an original Asus firmware at any time just
by flashing a firmware downloaded from Asus's website.

NOTE: resetting to factory default after flashing is 
strongly recommended for the following cases:

- Updating from a firmware version that is more than 3 releases older
- Switching from a Tomato/DD-WRT/OpenWRT firmware

If upgrading from anything older and you experience issues, then 
consider doing a factory default reset then as well.

Always read the changelog, as mandatory resets will be mentionned 
there when they are necessary.

In all of these cases, do NOT load a saved copy of your settings!
This would be the same thing as NOT resetting at all, as you will 
simply re-enter any invalid setting you wanted to get rid of.  Make 
sure to create a new backup of your settings after reconfiguring.



Usage
-----

** JFFS **
JFFS is a writeable section of the flash memory which will allow you to 
store small files (such as scripts) inside the router without needing 
to have a USB disk plugged in.  This space will survive reboots (but it 
*MIGHT NOT survive firmware flashing*, so back it up first before 
flashing!).  It will also be available fairly early at boot (before 
USB disks).

The option is enabled by default.  You can however disable it, or 
reformat it from the Administration -> System page.

On that page you will also find an option called "Enable custom 
scripts and configs".  If you intend to use custom scripts or 
config files, then you need to enable this option.  This is not 
enabled by default so if you create a broken config/script that 
prevents the router from booting, you will still be able to regain 
access by doing a factory default reset.

Try to avoid doing too frequent writes to this partition, as it will
prematuraly wear out the flash storage.  This is a good place to put 
files that are written once like scripts or kernel modules, or that 
rarely get written to.  Storing files that constantly get written 
to (like very busy logfiles) is NOT recommended - use a 
USB disk for that.



** User scripts **
These are shell scripts that you can create, and which will be run when 
certain events occur.  Those scripts must be saved in /jffs/scripts/ ,
so, JFFS must be enabled, as well as the option to use custom 
scripts and configs.  This can be configured under Administration -> System.
Available scripts:

 * ddns-start: Script called at the end of a DDNS update process.
               This script is also called when setting the DDNS type
               to "Custom".  The script gets passed the WAN IP as 
               an argument.
               When handling a "Custom" DDNS, this script is also
               responsible for reporting the success or failure
               of the update process.  See the Custom DDNS section
               below for more information.
 * dhcpc-event: Called whenever a DHCP event occurs on the WAN 
                interface.  The type of event (bound, release, etc...) 
                is passed as an argument.
 * firewall-start: Firewall is started (filter rules have been applied)
                   The WAN interface will be passed as argument (for 
                   example. "eth0")
 * init-start: Right after jffs is mounted, before any of the services 
               get started
 * nat-start: nat rules (i.e. port forwards and such) have been applied 
              (nat table)
 * openvpn-event: Called whenever an OpenVPN server gets
                  started/stopped, or an OpenVPN client connects to a
                  remote server.  Uses the same syntax/parameters as
                  the "up" and "down" scripts in OpenVPN.
 * post-mount:  Just after a partition is mounted
 * pre-mount: Just before a partition is mounted.  Be careful with 
              this script.  This is run in a blocking call and will 
              block the mounting of the partition  for which it is 
              invoked till its execution is complete. This is done so  
              that it can be used for things like running e2fsck on the 
              partition before mounting. This script is also passed the 
              device path being mounted as an argument which can be 
              used in the script using $1.
 * qos-start: Called after both the iptables rules and tc configuration 
              are completed for QoS.  This script will be passed an
              argument, which will be "init" (when QoS is being
              initialized and it has setup the tc classes) or
              "rules" (when the iptables rules are being setup).
 * services-start: Initial service start at boot
 * services-stop: Services are stopped at shutdown/reboot
 * unmount: Just before unmounting a partition.  This is a blocking 
            script, so be careful with it.  The mount point is passed 
            as an argument to the script.
 * wan-start: WAN interface just came up (includes if it went down and 
              back up).  The WAN unit number will be passed as argument 
              (0 = primary WAN)

Don't forget to set them as executable:

   chmod a+rx /jffs/scripts/*

And like any Linux script, they need to start with a shebang:

   #!/bin/sh



** SSHD **
The router can be accessed over SSH (through Dropbear).  Password-based 
login will use the same username and password as telnet/web access.  
You can also optionally insert a RSA or ECDSA public key there for 
keypair-based authentication.  Finally, there is also an option to 
make SSH access available over WAN.



** Crond **
Crond will automatically start at boot time.  You can put your cron 
tasks in /var/spool/cron/crontabs/ .  The file must be named "admin" as 
this is the name of the system user.  Note that this location resides in 
RAM, so you would have to put your cron script somewhere such as in the 
jffs partition, and at boot time copy it to /var/spool/cron/crontabs/ 
using an init-start user script.

A simple way to manage your cron jobs is through the included "cru" 
command.  Just run "cru" to see the usage information.  You can then 
put your "cru" commands inside a user script to re-generate your cron 
jobs at boot time.



** Enhanced Traffic monitoring **
Under Tools -> Other Settings are options that will allow you to save 
your traffic history to disk, preserving it between router reboots (by 
default it is currently kept in RAM, so it will disappear when you 
reboot).

You can save it to a custom location (for example, "/jffs/" if you have 
jffs enabled), or /mnt/sda1/ if you have a USB disk plugged in.
Save frequency is also configurable - it is recommended to keep that 
frequency lower (for example, once a day) if you are saving to jffs, to 
reduce wearing out your flash memory.  Make sure not to forget the 
trailing slash ad the end of the path.

Note that the first time you use that option, you must tell the router 
to create the data file.  Make sure you set "Create or reset data 
files" to "Yes".

Also, Asuswrt-Merlin can track the traffic generated by each individual 
IP on your network.  This option is called IPTraffic.  To enable this, 
you must first set a custom location to store your traffic database 
(see above).  Once again, you must also tell it to create the new data 
file, by enabling "Create or reset IPTraffic data files".  Once done, 
enable the IPTraffic Monitoring option.  This will add three new 
entries to the Traffic Monitor page selector (on the Traffic Monitoring 
page).

You can optionally specify which IP to monitor, or exclude some IPs 
from monitoring.  Each IP must be separated by a comma.

It's strongly recommended that you assign a static IP to devices you 
wish to monitor to ensure they don't get a different IP over time, 
which would make the collected data somewhat unreliable.  The 
monitoring is done per IP, NOT per MAC.



** Adjustable TCP/IP connection tracking settings **
Under Tools -> Other Settings there are various parameters that lets 
you tweak the timeout values related to connection tracking for TCP and 
UDP connections.  You should be careful with those settings.  Most 
commonly, people will tweak the UDP timeout values to make them more 
VoIP-friendly, by using smaller timeouts.  Timeout values are in 
seconds.



** Mounting remote CIFS shares on the router **
You can mount remote SMB shares on your router.  The syntax will 
be something like this:

mount \\\\192.168.1.100\\ShareName /cifs1 -t cifs -o "username=User,password=Pass"

(backslashes must be doubled.)



** Disk Spindown when idle **
Jeff Gibbons's sd-idle-2.6 has been added to the firmware, allowing you 
to configure a timeout value (in seconds) on the Tools -> Other Settings 
page.  Plugged hard drives will stop spinning after being inactive 
for that specified period of time.  Note that services like Download 
Master might be generating background disk activity, preventing it from 
idling.



** OpenVPN (client and server) **
OpenVPN is an SSL-based VPN technology that is provided as a secure 
alternative to the PPTP VPN.  OpenVPN is far more secure and more 
flexible, however it is not as easy to configure, and requires the 
installation of a client software on your computer client.  The client 
can be obtained through this download page:

http://openvpn.net/index.php/open-source/downloads.html

Explaining the details of OpenVPN are beyond the scope of this 
documentation, and I am in no way an expert on OpenVPN.
Fortunately, there is a lot of available documentation and Howto guides 
out there.  I tried to stick to the same option descriptions as used by 
Tomato, so about any guide written for Tomato can easily be used to 
guide you on Asuswrt-Merlin.  For pointers, check the Wiki on the 
Asuswrt-Merlin Github repository.

You can provide your own custom client config files for the two server 
instances.  Store them in the /jffs/configs/openvpn/ccd1/ (and ccd2/) 
directory based on which server instance they belong to, with the 
filenames matching the client common names.  See the OpenVPN 
documentation for more details on the ccd directory.



** Customized config files **
The services executed by the router such as minidlna or dnsmasq relies 
on dynamically-generated config files.  There are various methods 
through which you can interact with these config files to customize 
them.

The first method is through custom configs.  You can append content to 
various configuration files that are created by the firmware, or even 
completely replace them with custom config files you have created.  
Those config override files must be stored in /jffs/configs/.  To have 
a config file appended to the one created by the firmware, simply add 
".add" at the end of the file listed below.  For example, 
/jffs/configs/dnsmasq.conf.add will be added at the end of the dnsmasq 
configuration file that is created by the firmware, while 
/jffs/configs/dnsmasq.conf would completely replace it.

Note that replacing a config file with your own implies that you 
properly fill in all the fields usually dynamically created by the 
firmware.  Since some of these entries require dynamic parameters, you 
might be better using the postconf scripts added in 374.36 (see the 
postconf scripts section below).

Also note that for customized config files to be available, you need 
to have both JFFS and the custom config and script support options 
enabled, under Administration -> System.

The list of available config overrides:

 * adisk.service
 * afpd.service
 * avahi-daemon.conf
 * dhcp6s.conf
 * dnsmasq.conf
 * exports (only exports.add supported)
 * fstab (only fstab supported, remember to create mount point
        through init-start first if it doesn't exist!)
 * group, gshadow, passwd, shadow (only .add versions supported)
 * hosts (for /etc/hosts)
 * minidlna.conf
 * mt-daap.service
 * pptpd.conf
 * profile (shell profile, only profile.add suypported)
 * radvd.conf
 * smb.conf
 * snmpd.conf
 * vsftpd.conf
 * upnp (for miniupnpd)

Also, you can put OpenVPN ccd files in the following directories:

  /jffs/configs/openvpn/ccd1/
  /jffs/configs/openvpn/ccd2/

The content of these will be copied to their respective 
server instance's ccd directory when the server is started.


** Postconf scripts **
A lot of the configuration files used by the router services
(such as dnsmasq) are dynamically generated by the firmware.  This
makes it hard for advanced users to apply modifications to these, short
of entirely replacing the config file.

Postconf scripts are the solution to that.  Those scripts are 
executed after the router has generated a configuration script, but 
before the related service gets started.  This means you can use those 
scripts to manipulate the configuration script, using tools such as 
"sed" for example.

Postconf scripts must be stored in /jffs/scripts/ .  JFFS must be 
enabled, as well as the option to use custom scripts and configs.  
This can be configured under Administration -> System.

The path/filename of the target config file is passed as argument to 
the postconf script.

The list of available postconf scripts is:

 * adisk.postconf (Time Machine)
 * afpd.postconf (Time Machine)
 * avahi-daemon.postconf (Time Machine)
 * dhcp6s.postconf
 * dnsmasq.postconf
 * exports.postconf
 * fstab.postconf
 * group.postconf
 * gshadow.postconf
 * hosts.postconf
 * minidlna.postconf
 * mt-daap.postconf
 * openvpnclient1.postconf (and openvpnclient2.postconf)
 * openvpnserver1.postconf (and openvpnserver2.postconf)
 * passwd.postconf
 * pptpd.postconf
 * radvd.postconf
 * shadow.postconf
 * smb.postconf
 * snmpd.postconf
 * upnp.postconf
 * vsftpd.postconf

To make things easier for novice users who don't want to 
learn the arcane details of using "sed", a script providing 
support functions is available.  The following dnsmasq.postconf 
script demonstrates how to modify the maximum number of leases 
in the dnsmasq configuration:

-----
     #!/bin/sh
     CONFIG=$1
     source /usr/sbin/helper.sh

     pc_replace "dhcp-lease-max=253" "dhcp-lease-max=100" $CONFIG
-----

Three functions are currently available through helper.sh:

   pc_replace "original string" "new string" "config filename"
   pc_insert "string to locate" "string to insert after" "config filename"
   pc_append "string to append" "config filename"

Note that postconf scripts are blocking the firmware while they run, to 
ensure the service only gets started once the script is done.  Make 
sure those scripts do exit properly, or the router will be stuck 
during boot, requiring a factory default reset to recover it.



** NFS Exports **
IMPORTANT: NFS sharing is still a bit unstable.

In addition to SMB and FTP, you can now also share any plugged 
hard disk through NFS.  The NFS Exports interface can be accessed 
from the USB Applications section, under Servers Center.  Click on the 
NFS Exports tab.

Select the folder you wish to export by clicking on the Path field.  
Under Access List you can enter IPs/Networks to which you wish to give 
access.  A few examples:

  192.168.1.0/24 - will give access to the whole local network
  192.168.1.10 192.168.1.11 - will give access to the two IPs (separate with spaces)

Entering nothing will allow anyone to access the export.

Under options you can enter the export options, separated by a comma.  
For example:

  rw,sync

For more info, search the web for documentation on the format of the 
/etc/exports file.  The same syntax for the access list and the options 
is used by the webui.

You can also manually generate an exports file by creating a file named 
/jffs/configs/exports.add , and entering your standard exports there.  
They will be added to any exports configured on the webui.

Note that by default, only NFSv3 is supported.  You can also enable 
NFSv2 support from that page, but this is not recommended, unless you 
are using an old NFS client that doesn't support V3.  NFSv2 has various 
filesystem-level limitations.



** Easy Entware setup **
Entware is an alternative to Optware.  They are both online software 
repositories that let you easily install additional software to your 
router (such as an Apache web server, or an Asterisk PBX).  The main 
benefit of Entware over Optware (which is used by Asus for their own 
Download Master) is it is very actively maintained, with recent 
software versions.

Entware and Optware cannot be used at the same time however, so you 
can't use Download Master while using Entware.

There is now a script to make setting up Entware ware easier.  
Access your router through SSH/Telnet, and run 
"entware-setup.sh".

Note that Entware requires the JFFS partition to be enabled, and an 
ext2/ext3 formatted USB disk (NTFS, HFS+ and FAT32 are not supported).

Also note that Entware is not available for the RT-AC56U, RT-AC68U 
or RT-AC87 due to the different CPU architecture.



** DNSFilter **
Under Parental Control there is a tab called DNSFilter.  On this 
page you can force the use of a DNS service that provides 
security/parental filtering.  This can be done globally, or on a 
per device basis.  Each of them can have a different type of filtering 
applied.  For example, you can have your LAN use OpenDNS's server to 
provide basic filtering, but force your children's devices to use 
Yandex's family DNS server that filters out malicious and adult
content.

If using a global filter, then specific devices can be told to 
bypass the global filter, by creating a client rule for these, 
and setting it to "No Filtering".

DNSFilter also lets you define up to three custom nameservers, for 
use in filtering rules.  This will let you use any unsupported 
filtering nameserver.

You can configure a filter rule to force your clients to 
use whichever DNS is provided by the router's DHCP server (if 
you changed it from the default value, otherwise it will be 
the router's IP).  Set the filtering rule to "Router" for this.

Note that DNSFilter will interfere with resolution of local 
hostnames.  This is a side effect of having devices forced to use 
a specific external nameserver.  If this is an issue for you, then set 
the default filter to "None", and only filter out specific devices.



** Layer7-based Netfilter module **
Support for layer7 rules in iptables has been enabled on MIPS-based
routers (RT-N16/N66/AC66).  You will need to manually configure the 
iptables rules to make use of it - there is no web interface exposing 
this.  The defined protocols can be found in /etc/l7-protocols.

To use it, you must first load the module:

   modprobe xt_layer7

An example iptable rules that would mark all SSH-related packets 
with the value "22", for processing later on in another chain:

   iptables -I FORWARD -m layer7 --l7proto ssh -j MARK --set-mark 22

These could be inserted in a firewall-start script, for example.

For more details on how to use layer7 filters, see the documentation on 
the project's website:

http://l7-filter.clearfoundation.com/



** Custom DDNS **
If you set the DDNS (dynamic DNS) service to "Custom", then you will be 
able to fully control the update process through a ddns-start user 
script.  That script could launch a custom DDNS update client, or run a 
simple "wget" on a provider's update URL.  The ddns-start script will 
be passed the WAN IP as an argument.

Note that the script will also be responsible for notifying the firmware 
on the success or failure of the process.  To do this you must simply 
run the following command:

   /sbin/ddns_custom_updated 0|1

0 = failure, 1 = successful update

If you cannot determine the success or failure, then report it as a 
success to ensure that the firmware won't continuously try to 
force an update.

Here is a working example, for afraid.org's free DDNS (you must update
the URL to use your private API key from afraid.org):

-----
    #!/bin/sh

    wget -q http://freedns.afraid.org/dynamic/update.php?your-private-key-goes-here -O - >/dev/null

    if [ $? -eq 0 ]; then
        /sbin/ddns_custom_updated 1
    else
        /sbin/ddns_custom_updated 0
    fi
-----

Finally, like all custom scripts, the option to support custom scripts and 
config files must be enabled under Administration -> System.



Source code
-----------
The source code with all my modifications can be found on Github, at:

https://github.com/RMerl/asuswrt-merlin



History
-------
378.50 (xx-xxx-2015)
   - CHANGED: Reverted back to vsftpd 2.x, as 3.0.2 doesn't work properly
              on MIPS architectures (and possibly other particular
              scenarios as well).
   - CHANGED: Added warning to the DDNS page if you set the type
              to Custom and either JFFS or custom script support isn't
              enabled
   - FIXED: A few unescaped quotes in the French dict breaking VPN pages
   - FIXED: MAC list would get corrupted when removing and re-adding
            entries on the MAC filter list
   - FIXED: AC68U CFE update wasn't written to flash due to permission
            issues
   - FIXED: Static Key field wasn't visible when using HMAC
   - FIXED: syslogd was always enforcing the -S switch
   - FIXED: When setting a static DHCP from the networkmap, the user-entered
            name wouldn't be used.  Now it gets used, and we rely on the rc
            daemon to properly handle it if it's not a valid hostname (it will
            simply not provide it to dnsmasq's static name list).


378.50 Beta 2 (31-Jan-2015)
   - NEW: Added custom config and postconf support for avahi, netatalk 
          and mt-daapd (iTunes server).
   - CHANGED: Moved the AC68U CFE update process to the same location
              as in GPL 3626 to see if it works more consistently.
   - FIXED: Non-DPI build of AC56U had incompatible Tuxera modules
   - FIXED: vsftpd wouldn't start if you had IPv6 enabled.
   - FIXED: Asus had disabled the NAT loopback fix on MIPS's iptables
            in GPL 3762.  Re-enabled.
   - FIXED: Wireless clients that hadn't communicated in a while wouldn't
            be properly shown on the Wireless log (patch by pinwing)
   - FIXED: QoS rules weren't applied properly when IPv6 was enabled
            (was changed in recent GPL - reverted it)
   - FIXED: Can't apply a Custom DDNS if you don't have something entered
            in the username/password fields (shown in other DDNS services)
   - FIXED: NFS page wasn't properly loading


378.50 Beta 1 (25-Jan-2015)
   - IMPORTANT: You must do a factory default reset, and manually
                reconfigure your setting.  Failure to do so can 
                lead to various issues with wifi, OpenVPN,
                and the new AC68U bootloader.

   - IMPORTANT: Please read this changelog, especially the changes 
                related to jffs, user scripts/config and OpenVPN.

   - NEW: Merged with Asus 378_3913 GPL code.  Most notable changes:
            * Trend Micro DPI engine for RT-AC68U
            * Updated Trend Micro engine for RT-AC87U
            * Updated Quantenna firmware/driver
            * Various updates to 3G/4G support and Dual WAN

   - NEW: ddns-start user script, executed after the DDNS update 
          was launched (can be used to update additional services)
   - NEW: Custom DDNS (handled through ddns-start script)
          See the documentation for how to create such
          a script.
   - NEW: Option to enable support for custom scripts and
          config files.  This option is disabled by default, so 
          if you have a broken script that prevents the router from 
          booting, doing a factory default reset will ensure that the 
          broken script won't be executed, and recover access to the
          router.  This is necessary since the JFFS2 partition is
          now enabled by default.
   - CHANGED: Added logo to DNSFilter on the AiProtection
              homepage (contributed by Piterel)
   - CHANGED: Updated Openssl to 1.0.0p
   - CHANGED: Merged Asus's newer NTP update code, with a fix
              to prevent hourly log spam from the update process
              when in a DST enabled timezone.
   - CHANGED: Updated vsftpd to 3.0.2 (newer version used by
              Asus on their Qualcomm-based routers)
   - CHANGED: the qos-start script will be passed an argument
              that will contain "init" (when setting up tc)
              or "rules" (when setting up iptables).
   - CHANGED: JFFS2 partition is now enabled by default, to be in
              sync with Asus, who are starting to make use of this
              partition.
   - CHANGED: The Local IP in an IPv6 firewall rule can now be
              left empty.
   - CHANGED: Download Master will now be downloaded at install time 
              rather than included in the firmware, to increase the
              amount of space available to JFFS - this matches
              the AC56/AC68. (N16, N66)
   - FIXED: Under certain conditions, the OpenVPN server page 
            would report an initializing state when it was 
            already running.
   - FIXED: First OpenVPN client/server instance wasn't properly
            run on the second CPU core, resulting in lower
            performance (AC56/AC68/AC87)
   - FIXED: Router IP wasn't advertised through DHCP as WINS
            server if WINS was enabled
   - FIXED: OpenVPN would crash if specifying "None" as
            the cipher (regression in OpenVPN 2.3.6)
   - FIXED: The "empty" category was removed by Asus a
            few months ago, preventing you from removing
            an assigned priority on the Adaptive QoS
            page.  Re-added it.
  - FIXED: Port triggers weren't written to the correct
           iptables chain (Asus bug)
  - FIXED: When moving from stock to this firmware, the OpenVPN
           Server 1 instance gets automatically enabled because 
           Asus hardcodes "1" into the nvram setting that handles
           start at wan.  Changed to a different nvram to resolve
           this conflict.  This means everyone must re-enable their
           OpenVPN server instance after upgrading from any version
           before 376.50.
  - FIXED: dnsmasq would run out of available leases if you had a 
           very small DHCP pool combined with many out-of-pool 
           reservations.  Now the limit will be either 253 or the 
           pool size, whichever is the largest (Asus issue)
  - FIXED: SSHD port forwarding couldn't be enabled/disabled
  - FIXED: DHCP log spam when using IPv6 with a Windows 8
           client (patch by pinwing)
  - FIXED: snmp exposes a lot of sensitive information such as
           login credentials, therefore all the custom Asus MIBs
           have been disabled.
  - FIXED: Very long SSIDs with special characters/spaces in them 
           would be shown as "undefined" in the banner.
  - FIXED: Curl would fail to access SSL sites due to lack of
           a CA bundle.


376.49_5 (9-Jan-2015)
   - FIXED: Vulnerability in infosvr (CVE-2014-9583) (Asus bug)
   - FIXED: Additional security issue in infosvr (incorrect memcpy()
            call) (Asus bug)


376.49_4 (27-Dec-2014)
   - FIXED: WAN page error when entering a hostname, and broken 
            UPNP FAQ link
   - FIXED: OpenVPN Server wasn't showing the Advertize DNS to
            Client option (regression from 3677 merge)
   - FIXED: bootloop when enabling Traditional QoS (or any other
            feature that forces CTF to be disabled) due to 
            FA being left enabled (Asus bug) (AC87)


376.49_2 (23-Dec-2014)
   - FIXED: Asus DDNS couldn't be configured on the webui
   - FIXED: OpenVPN server wouldn't let you edit user accounts
   - FIXED: Missing DLNA icon on clients (Asus bug) (N66, AC66)


376.49 (21-Dec-2014)
   - NEW: Merged with Asus GPL 376_3677.  This new code
          includes a lot of changes related to USB modem
          support.
  - NEW: IPv6 handling based on dnsmasq + odhcp6c.  This new
         code which has been developped by Asus these past few
         months but kept disabled so far has been enabled.
         Initial tests show much better reliability with
         different ISPs.
  - NEW: Added IPv6 support to DNSFilter (currently only 
         Yandex has IPv6 servers).  Note that unlike IPv4
         filtering, we cannot automatically NAT queries
         to the desire server, so the current implementation
         works like Asus's YandexDNS service, where IPv6 servers
         are simply returned to DHCPv6/RA client queries,
         and ip6tables ensures that you cannot override
         them, by rejecting connection to other DNS servers.
  - CHANGED: Merged newer DPI engine from 378_3123 beta
             (AC87)
  - CHANGED: Removed SSLv2 and v3 support from OpenSSL
             (we had already stopped using these in
             376.48, so this removes unused code)
  - CHANGED: The VPN webui is now a bit closer to Asus's code.
             This will mostly make it easier to keep in
             sync with future changes to that UI by
             Asus (they rearranged the layout a bit in
             376_36xx).
  - CHANGED: Updated OpenVPN to 2.3.6
  - CHANGED: Reverted to Asus's max-lease number calculation
             for dnsmasq
  - CHANGED: Hide wireless key on settings page unless field
             has focus (patch by John9527)
  - CHANGED: Ported USB 3.0 (XHCI) kernel driver from
             Netgear GPL (which seems to have in turn
             backported it from upstream kernel 3.x)
  - CHANGED: Updated Quantenna to v36.7.3.23 (AC87)
  - FIXED: vsftpd wasn't properly compiled with SSL
           support.
  - FIXED: MAC filtering couldn't be disabled on Guest
           networks (Asus bug) (Patch by John9527)
  - FIXED: Various fixes and tweaks to the new IPv6
           code from Pinwing and saintdev
  - FIXED: Editing a client on the networkmap would 
           cause any matching DHCP reservation entry to
           lost its hostname
  - REMOVED: The web redirection control setting was
             removed, as it is being replaced by the
             (simpler) redirection setting Asus added
             to the System page.


376.48_3 (20-Nov-2014)
   - FIXED: NAT loopback was broken on MIPS devices
            (backported Asus fix from 376_3626)


376.48_2 (8-Nov-2014)
   - FIXED: Samba would fail to start on the RT-N16 due to a
            missing library.


376.48_1 (7-Nov-2014)
   - FIXED: Max-lease calculation Asus introduced in 376_2769 is
            broken - re-hardcode it to 253 like they used to do in
            previous release.  Will be properly fixed once they
            release a newer GPL with this issue resolved.
            (Asus bug)

376.48 (7-Nov-2014)
   - NEW: Added the RT-AC68P to the list of supported devices
   - CHANGED: Use sha256 checksums instead of MD5 for improved
              security when validating your downloads.
              (note: checksums are also posted on the support
              forum at SmallNetBuilder)
   - CHANGED: Switched my fix for unmounted/hidden partition
              support with Asus's own fix from GPL 3561.
   - FIXED: Samba would fail to start if the router admin username contained
            upper case characters.  Samba was modified to have it try to
            local the UNIX user as provided (it was previously only
            trying upper and lower case versions) (Samba 3.6 bug)


376.48 Beta 3 (02-Nov-2014)
   - CHANGED: Updated miniupnpd to release 1.9 (plus upstream PCP fix)
   - FIXED: Couldn't edit share permissions for Samba if your disk
            contained an unmounted/hidden partition (Asus bug in 2769)
   - FIXED: Couldn't edit share permissions for Samba for the RT-N66U
            internal SDcard reader (Asus bug in 2769)
   - FIXED: Missing Max User field to Samba page (Asus bug)


376.48 Beta 2 (26-Oct-2014)
   - NEW: Added logo to the webui header
   - CHANGED: Samba 3.6 will now use libiconv to handle
              charset conversion (will resolve CP850
              warnings amongst other things)
  - CHANGED: Updated miniupnpd to 20141023 code from Github.
  - CHANGED: Updated dropbear to 2014.66.
  - CHANGED: Reverted NTP update code to GPL 2678 in hopes of
             resolving the few cases where it didn't work anymore.
  - FIXED: minidlna is once again able to use inotify for updates.
           A temporary workaround has been implemented where
           minidlna will be staticly linked with a threadsafe
           build of sqlite3, while BWDPI will continue to use
           the shared non-threadsafe library. (Asus bug)


376.48 Beta 1 (18-Oct-2014)
   - NEW: Merged with Asus 376_2769 AC87 GPL
   - NEW: Enabled numerous modules in net-snmp (based on the list
          used by OpenWRT)
   - NEW: Added postconf and custom config support for snmpd.conf
   - NEW: Added HID support to ARM kernel (AC56,AC68,AC87)
   - CHANGED: Reverted NAT loopback code to Asus's, since our own
              code is currently broken by recent FW code changes.
   - CHANGED: Updated openssl to 1.0.0o, resolving a few security issues.
   - CHANGED: Disabled SSLv2 and SSLv3 support for https access to the
              router webui.  IE6 users, your time is up - upgrade.
              TLS 1.0 is now the only supported protocol.
   - CHANGED: upgraded main Samba server from 3.0.x to 3.6.24.  This might
              cause a slight drop in performance, but should improve
              both reliability and security.
   - FIXED: DNSFilter client list dropdown would sometime be empty.
   - FIXED: DNS queries run on the router were forwarded to upstream
            nameservers instead of the local dnsmasq
   - FIXED: Re-added the USB HID kernel module needed for UPS monitoring
            (patch by ryzhov_al)
   - FIXED: Incorrect top margin on some pages such as AiCloud, and
            stretched font on the progress splash (Asus bug)
   - FIXED: URL and keyword filtering wasn't working under certain
            situations when CTF was enabled
   - FIXED: Mac Filtering wasn't working with Guest networks
            (Asus bug) (Patch by saintdev)
   - FIXED: Chosing a client on the MAC Filter page wasn't properly
            filling the Name field.  Also reorganized layout a bit.


376.47 (20-Sept-2014)
   - NEW: Added sha256 and sha512 HMAC support to dropbear (SSH)
   - CHANGED: Moved OpenVPN postconf scripts right before server/client
              gets started, so you can also use them to modify the other 
              generated files such as the exported ovpn config file.
  - FIXED: SSHD options visibility (patch by pinwing)
  - FIXED: EMF/IGMP settings were reverting to the select profile
           default (Asus bug introduced in GPL 2678)
  - FIXED: PPTP account list failed to display (regression in Beta 1)
  - FIXED: VPN server page was switching back to PPTP when changing
           OpenVPN unit and you were initially on the PPTP page
  - FIXED: Activity indicator wasn't shown during a networkmap
           scan


376.47 Beta 1 (14-Sept-2014)
   - NEW: Merged with Asus GPL 2678 (AC87)
   - NEW: Report Quantenna FW version on Sysinfo page
   - NEW: Enabled experimental FTP and Samba Cloud Sync support in AiCloud.
          This feature is still in development by Asus, so it might not be
          fully functional yet.
   - NEW: Enabled experimental SNMPD support, under Administration -> SNMP.
          This feature is still in development by Asus, so it might not be
          fully functional yet. (not available on the RT-N16)
   - NEW: Added option to enable WAN access to SNMPD, defaults to disabled.
          (Asus's implementation has it open to the WAN by default)
   - CHANGED: Re-increased max allowed FTP user limit to 10 (was reverted
              to 5 in the GPL merge when the setting was moved to the
              FTP page)
   - FIXED: PPTPD was getting enabled every time you clicked Apply while on 
            the PPTPD VPN Server page


376.46 (26-Aug-2014)
   - NEW: Merged with Asus GPL 2061.  This is essentially
          the new QTN driver for the AC87.
   - FIXED: Various webui issues with IE10/IE11 (patch by pinwing)
   - FIXED: OpenVPN Client page was visible on the RT-N16
   - FIXED: DHCP pool validation error on VPN Server advanced page.
   - FIXED: Couldn't edit the first VPN Client entry due to broken
            duplicate check (Asus bug)


376.45 (17-Aug-2014)
   - NEW: Compiled vsftpd with SSL support (must be manually
          configured if you intend to use it)
   - NEW: Report FA state (Level 2 CTF) on Sysinfo page.
   - CHANGED: Updated dropbear to 2014.65.
   - CHANGED: Updated openssl to 1.0.0n (numerous
              security fixes)
   - CHANGED: Updated lzo to 2.08
   - CHANGED: Reworked VPN Server pages to be more intuitive
   - FIXED: Garbled client dropdown selector on DNSFilter page
   - FIXED: The Comcast neighbour solicitation block wasn't
            enabled anymore (regression in 376.44) (Patch by
            Sinshiva)
   - FIXED: 5 GHz N+AC mode was incorrectly setting router to
            N-only mode (Asus bug, fix backported from 2381,
            additional fix by me for AC66)
   - FIXED: PControl page failing to display on French and
            Italian locales (Asus bug)
   - FIXED: IPv6 can occasionally fail to work properly when
            using a PPPoE WAN interface (patch by pinwing)


376.44 (3-Aug-2014)
   IMPORTANT: Make a backup of your JFFS partition if upgrading
              an RT-AC56U or RT-AC68U and you have stored files
              on that partition!  The partition layout has been 
              changed.

   - NEW: Merged with Asus's 376_2044 GPL.
          Summary of changes:
            * New networkmap, lets users edit device names,
              assign icons to devices, etc...
            * Reworked IPv6 support
            * New filesystem driver provider for NTFS/HFS+/FAT
            * Webui visual update
            * Updated components (minidlna, radvd, dnsmasq)
 
  - NEW: Added support for RT-AC87U.
  - CHANGED: Updated N66U wireless driver to Asus's 1071 build
  - CHANGED: Updated miniupnpd to Git head (as of 20140731)
  - CHANGED: The JFFS partition on ARM devices now uses
             Asus's code, which means the whole unused space
             is now used for the JFFS partition.
             (AC56, AC68)
  - CHANGED: Made all ARM models use the new filesystem drivers from Tuxera, 
             resulting in general improved USB disk performance (and 
             hopefully improved reliability as well) (AC56, AC68)
  - CHANGED: The wifi notification icon will now report
             channel and channel width for the 5 GHz band,
             as the extension channel wasn't always accurately
             reported.
  - CHANGED: Reworked layout of SSH settings on System page (based 
             on Asus's own WIP)
  - CHANGED: Allow FQDN (hostname + domain) rather than just
             hostnames on the WAN page (some ISPs require that)
  - FIXED: Missing mDNSResponder daemon preventing mt-daapd
           from working on MIPS devices (N16,N66,AC66)
  - FIXED: System Log wouldn't properly be positioned
           at the bottom (Patch by John9527)
  - FIXED: DNSFilter clients configured to bypass DNSFilter
           would still be prevented from using an IPv6 DNS.
  - FIXED: Incorrect IPv6 prefix if not a multiple of 8
           (patch by NickZ)
  - FIXED: OpenVPN firewall cleanup was missing rules
           (patch by sinshiva)
  - FIXED: Minidlna issues with Philips smart TVs
  - FIXED: SSHD brute force protection wasn't working if
           Dual WAN was enabled and set to LB mode.
  - FIXED: Miniupnpd error flood in Syslog when using a
           Plex server on your LAN (fix from upstream)
  - REMOVED: Reverted various IPv6-related patches as they
             conflicted with Asus's own changes.  These might
             make it back at a later time if deemed
             necessary.
  - REMOVED: Removed layer7 filtering support in Netfilter from 
             ARM devices due to compatibility issues (AC56,AC68)
  - REMOVED: Removed IPsec support from ARM devices due to
             compatibility issues (AC56, AC68)


374.43_2 (7-June-2014)
   - FIXED: NTFS disks couldn't be mounted (Paragon driver not
            loading due to a kernel change) (AC56, AC68)


374.43 (6-June-2014)
   - NEW: User-configurable refresh period to trigger a DDNS
          update after a certain number of days.
   - CHANGED: dnsmasq option 252 now defaults to an empty string,
              to silence broken clients such as Win7.
              Important: if you were previously using a customized
              252 reply (to use with a valid wpad/pac file), you 
              will need to use a postconf script to change the
              default config instead of appending your own
              config.
              If you use DNS-based WPAD setting, you will need
              to remove the 252 option using postconf, as IE will
              not query for the DNS entry if there is a 252
              option through DHCP, even if it fails to connect to it.

   - CHANGED: Updated miniupnpd to 1.8.20140523.
   - CHANGED: Updated openssl to 1.0.0m.
   - CHANGED: More backports from OpenSSL 1.0.2, improving SHA
              performance on ARM routers.
   - CHANGED: The JFFS2 partition is now disabled by default after
              a factory default reset.
   - FIXED: Media server page wouldn't let you enable the iTunes
            server unless you also enabled DLNA (Asus bug)
   - FIXED: Restricted guests still had access to the router (Asus
            bug introduced in GPL 4887)
   - FIXED: 6in4 traffic wasn't bypassing CTF if dualwan mode was
            either disabled or set to failover mode (AC56/AC68)
   - FIXED: Single character workgroups were rejected as invalid
            (Asus bug)
   - FIXED: Networks with SSIDs containing single quotes
            would break the client list (Asus bug)
   - FIXED: Traffic Monitor results are wrong on PPPoE connections
            (Asus bug) (Patch by pinwing, additional debugging 
            by fantom1)
   - FIXED: Crash if entering close to 64 MACs plus their names on
            the MAC filter page.


374.42_2 (16-May-2014)
   - FIXED: Time Machine support (AC56, AC68)


374.42 (9-May-2014)
   - NEW: Merged with Asus's 374_5656 GPL.
   - NEW: Added Comodo Secure DNS to supported DNSFilter services
   - FIXED: Download2 folder wasn't selectable anymore on the
            Media Server page.
   - FIXED: Pass correct valid and preferred lifetime to radvd when
            using DHCPv6-PD (Patch by pinwing)
   - FIXED: IPv6 connectivity could be lost after 1-2 hours due
            to the time shift caused by NTP at boot time
            (Patch by pinwing)
   - FIXED: Various IPv6 connectivity issues related to services
            being (re)started at the wrong time, or twice.
            (Patch by pinwing)
   - FIXED: Build system would sometime try to use the local system's
            header/libs - use a pkg-config wrapper to avoid this
            issue (Patch by ppuryear)
   - FIXED: Erratic 5G led blinking behaviour as the watchdog's software-
            based blinking was constantly writing to the wireless chip's 
            registers for led control. (AC68)
   - FIXED: LEDs weren't all turning back on when coming out of
            Stealth Mode (AC56)
   - CHANGED: Make the router use dnsmasq for internal name
              resolution rather than directly using the WAN DNS.
   - CHANGED: Upgraded OpenVPN to 2.3.4.
   - CHANGED: Upgraded miniupnpd to 1.8.20140422 (PCP-related fixes)


374.41 (18-Apr-2014)
   - NEW: Merged with Asus's 374_5047 GPL.  Notable changes:
       * Fixed RT-AC68U random reboots
       * Additionnal security fixes
       * Improved Media server, SMB and FTP webui
       * minidlna and radvd updates

   - NEW: PCP support (Port Control Protocol)
   - NEW: Option to allow/deny FTP access from WAN.  Default is to
          reject WAN connections.  The option can be found on the
          USB Servers -> FTP Share
   - NEW: Option to control web redirection while Internet is 
          down (configurable on the WAN page).
   - CHANGED: Upgraded miniupnpd to 1.8.20140401.
   - CHANGED: Disk idle exclusion now supports up to 9 disks.
   - FIXED: WOL wasn't working (Asus bug in 4887/5047)
   - FIXED: Replaced webui glue with permanent concrete.  It won't
            fall again.
   - FIXED: Language dropdown not properly shown with 8-bit 
            characters.
   - FIXED: Comcast's IPv6 network would flood the LAN with
            neighbour solicitation packets, which should normally
            not cross beyond their modem.  There is now an ip6tables
            rule to filter out those packets, preventing your log
            from being spammed with table overflows.  The filter is
            is enabled by default and can be disabled by setting the 
            "ipv6_neighsol_drop" nvram setting to "0". (rule suggested
            by diplomat7)
  - FIXED: EMF wasn't properly configured after wireless was
           restarted (patch from Vahur)
  - FIXED: Router crashing when more than around 30 static routes 
           were entered
  - FIXED: webui would die for some users when accessing the VPN Server 
           config page and there were connected OpenVPN clients
  - FIXED: Added missing iptables-save on ARM platform (AC56, AC68)
  - FIXED: nvram factory default reset would sometime fail on MIPS
           devices (N16, N66, AC66) (Patch by ryzhov_al)
  - FIXED: Under a certain situation the router could lose track of
           whether an OpenVPN server/client instance was running or not.
           This could result in the webui trying to restart it, and
           returning an error message because it was already running.
  - REMOVED: The Media server database location is no longer
             configurable, as we've switched to Asus's new 
             automatic location selection.
  - REMOVED: Removed the Run Cmd page as it was a security
             risk.  This is also needed to keep in line with 
             recent security fixes Asus applied to the
             httpd backend to limit what external processes
             it can run, otherwise any malicious page could
             run arbitrary commands on your router if you
             were currently logged on a separate tab.


374.40 (6-March-2014)
   - KNOWN ISSUE: Some people are experiencing random reboots
     with the RT-AC68U running firmwares based on recent Asus GPL.
     If you are are affected, please revert to 374.40 alpha4 for now.
     Asus are looking into the issue, which affects this model since
     374_4422.

   - FIXED: Asuswrt was calling wl_defaults() every time the
            wifi was restarted, causing Regulation Mode to be
            overwritten.  Now we force it to h mode if the
            router model and region requires DFS compliance
            (same as Asus's code, except we won't enforce 
            it to off in other scenarios, and will only do
            so if it was previously set to off).
   - FIXED: Advanced wireless page broken on Internet Explorer, due
            to missing Array.IndexOf() support in IE (Asus bug)
   - FIXED: Incorrect model detection prevented CPU temperature
            from being shown on the Sysinfo page on the "R" SKUs.


374.40 Beta 2 (5-March-2014)
   - FIXED: Numerous buffer overruns in networkmap that would result
            in crashes or empty/incomplete device list.  Was often 
            visible on networks hosting a Windows Home Server machine.
            (Asus bug)
   - FIXED: Site survey was reporting 5G as being disabled on RT-N16.
   - FIXED: Various issues related to the helper.sh script for postconf
   - FIXED: The OpenVPN instance wasn't restarted if it was currently 
            stopped due to a syntax error in its config and you had 
            just corrected it.
   - FIXED: Restarting the wireless service would stop emf/igs snooping
            until they were manually restarted/recconfigured. (Asus bug)
   - FIXED: Channels above 153 were missing on 5 GHz band if width
            is set to 40 MHz (Asus bug)
   - FIXED: reg_mode was being enforced to "h" (EU region) or "off"
            (others) since GPL 4422.  We now stick again to what's 
            set in the webui by the end user.
   - FIXED: Allow LAN traffic while dualwan mode is set to lb (issue
            caused by the default policy fix in beta 1)


374.40 Beta 1 (1-March-2014)
   - NEW: Merged with Asus's 374_4561 GPL.  Notable changes:
       * Various security-related fixes
       * Redesigned Parental Control webui
       * Notification in case of insecure configuration

   - NEW: Added OpenDNS Family Shield support to DNSFilter
   - NEW: Added support for up to three user-defined servers to DNSFilter
   - NEW: Added option to force DNSfilter clients to always use the DNS
          provided to them by the router's DHCP server (which will be
          the router itself if you didn't change it on the DHCP 
          webui page)
   - NEW: Option to disable the DHCP6 Server (code contributed by
          kdarbyshirebryant)
   - CHANGED: The RT-N66U is now compiled with EM enabled
              by default.  That means there will no longer be a separate
              experimental build for this.
   - CHANGED: Updated dropbear to 2014.63
   - CHANGED: New type of glue for the webui header
   - CHANGED: Switched to a shorter version numbering scheme
   - FIXED: RT-N16 firmware (missing files were obtained from
            the new GPL release Asus made for this model)
   - FIXED: Last24 page wasn't properly displaying the 
            Avg value (regression in 374.39)
   - FIXED: Clients with a configured IPv6 DNS would bypass
            DNSFilter.  DNSFilter-enabled clients will now 
            be prevented from using IPv6 nameservers, forcing 
            them through the (IPv4-only) filtering nameserver
   - FIXED: DNSFilter clients set to "None" would still be
            forced through your WAN-configured nameservers,
            preventing nameservers configured on the clients
            from working.  Now they will fully ignore the DNSFilter 
            settings.
   - FIXED: The global DNSFilter would sometime not get properly
            configured in the firewall.
   - FIXED: When the firewall was disabled, the FORWARD chain
            policy was still left to "DROP" - changed to "ACCEPT".
   - FIXED: typo in SMB config ("use spne go") (Asus bug)
   - FIXED: PPPoE with an MTU of 1500 requires the WAN interface 
            to have its MTU set at 1508 (patch by pinwing)
   - FIXED: IPv6 Prefix Delegation issues (patch by pinwing)
   - FIXED: MTU setting on IPv6 connections (patch by pinwing)


3.0.0.4.374.39 (31-Jan-2014)
   This version isn't available for the RT-N16 as support for the 
   SDK5 platform is currently broken in the latest GPL sources. 

   - NEW: Merged with Asus 374_583 GPL.  Notable changes:
      * USB hub support
   - NEW: DNS-based filtering.  Under Parental Control there is
          now a new tab called DNS Filter where you can enable 
          a DNS-based filtering service, and apply a specific 
          filter both globally and on a per-client basis.  Supported
          are: OpenDNS, Norton Connect Safe and YandexDNS.
   - NEW: helper.sh script, to simplify creation of postconf
          scripts.  See the postconf section for details.
   - CHANGED: Discontinued SDK5 builds for the RT-N66U.  The new EM
              builds resolved wifi range issues by running the SDK6
              driver set in Engineering Mode (driver provided by Asus).
              Look in the Experimental folder for the EM build - it will 
              eventually become the standard build for the N66U once
              it gets sufficiently tested.  You might need to do a 
              factory default reset after switching to an EM build,
              for best results.
  - CHANGED: Re-switched back to rp-pppoe 3.11 since nobody confirmed 
             that 3.10 worked better for them.
  - CHANGED: Allow PPPoE MTU up to 1500, for ISPs that support RFC 4638.
  - CHANGED: Additional webui performance improvement by caching CSS.
  - FIXED: DHCPv6 client failing to start if the router username was 
           changed from "admin" (Asus bug) (patch from Saintdev)
  - FIXED: DHCPv6 client failing to request an IP with some ISPs 
           such as Comcast (Asus bug) (patch from Saintdev)
  - FIXED: SMB shares were accessible over WAN, bypassing Netfilter
           (Asus bug) (AC56/AC68)
  - FIXED: USB read speed would be limited by the QoS upstream
           configuration (Asus bug) (AC56/AC68)
  - FIXED: Resolution of local machines with domain appended would fail 
           when using a nameserver that does not return nxdomain errors 
           (such as OpenDNS) (Asus bug)

           The new behaviour is configurable on the LAN-> DHCP page, 
           in case you run your own nameserver which is expected to 
           handle both local and remote domains.  Default is to not
           forward these (to allow OpenDNS to work properly).

  - FIXED: OpenVPN Client page - changing the local IP wouldn't always be 
           properly saved.
  - FIXED: Well-known services not properly applying settings on the
           Network Services Filtering page (Asus bug)
  - FIXED: Webui crash when importing an ovpn with invalid cert/keys
  - FIXED: resolv.conf not reverted to its original content after an 
           OpenVPN client that gets DNS pushed to it would disconnect.
  - FIXED: The average rates on the realtime traffic page would be 
           calculated based on the max number of samples (300) instead of 
           the currently collected number of samples (Asus bug)
  - REMOVED: YandexDNS has been removed, since its functionality is now
             provided by the new DNSFilter.


3.0.0.4.374.38_2 (17-Jan-2014):
   - CHANGED: Improved webui responsiveness by instructing the browser 
              to cache images.
   - CHANGED: Reverted minidlna to 374.37 code.  While the latest code 
              brings some fixes, it seems to also break functionality 
              for a small number of users.  Too many low-level changes 
              from the minidlna author to make it easy to debug.
   - FIXED: Syntax error in DHCPv6 client config (Asus bug)
   - FIXED: Domain field wasn't clearly identified on the webui 
            when DDNS set to Namecheap (Saintdev)
   - FIXED: Missing carriage return in dnsmasq.conf when PPTP VPN
            is enabled, causing LAN name resolution issues.
            (Asus bug)
   - FIXED: A few unescaped quotes in the French dict would break
            some webui pages (such as the Wireless page).
            (Asus bug)
   - FIXED: OpenVPN server export would always export the first 
            server instance configuration.
   - FIXED: Bogus "Config file is missing" error logged by pptpd when 
            it was starting (Asus bug)
   - FIXED: "Advertise DNS" wasn't visible if the page was loaded and 
            "Respond to DNS" was already enabled.


3.0.0.4.374.38_1 (12-Jan-2014):
   - FIXED: Tools -> Run Cmd page wasn't working (regression
            in 374.38)
   - FIXED: Router getting stuck on various webui changes due 
            to a broken precompiled emf module (AC56/AC68)


3.0.0.4.374.38 (11-Jan-2014):
   This version isn't available for the RT-N16 or the SDK5 build 
   of the RT-N66U as support for the SDK5 platform is currently 
   broken.  Please stick to 374.36 Beta 1 for the time being on 
   these two platforms.

   Note that the RT-N66U did get a newer wifi driver, so give it a 
   try, as it might have resolved or at least improved on the wifi 
   range issues.

   Remember to do a factory default reset if switching from SDK5 to 
   SDK6 builds!  Keep a backup of your existing settings in case you 
   decide to revert back to an SDK5 build.

   - NEW: Merged with 374_2078 GPL provided by Asus (From RT-N66U).
          Notable changes:
      * Updated SDK for MIPS devices - 6.30.163.2002 (r382208)
      * PPPoE HW acceleration should be fixed by the new SDK
      * Updated AiCloud closed source components (MIPS)

   - CHANGED: Reverted Parental Control code to our fixed code, 
              as I see Asus is still making fixes to their own 
              code past version 2078.
   - CHANGED: Updated AC56 and AC68U wifi driver and CTF to
              January 3rd builds (provided by Asus)
   - FIXED: emf/igs userspace tools were missing on ARM devices
   - FIXED: USB devices missing on MIPS devices (regression
            in 374.37)
   - FIXED: Wifi stability on ARM devices (regression in
            374.37)


3.0.0.4.374.37 (31-Dec-2013):
   * This build was pulled due to numerous issues *

   - NEW: Merged with Asus 374_501 GPL (from RT-AC68U).
          Notable changes in this version:
          * New SDK (wireless driver and CTF) for AC56/AC68
          * dnsmasq updated to 2.68
          * radvd updated to 1.9.5
          * Improved IPv6 support
          * Fixed Parental Control (A-M's own fix was replaced with
            this new one for consistency)
          * More details shown on Wireless Log page (their changes
            were merged with our own changes)
   - CHANGED: Dropbear default path will now include the locations
              inside /opt
   - CHANGED: Don't include a cert/key section in exported .ovpn if the
              router has "User authentication only" enabled
   - CHANGED: Display in which chain a given port forward rule is, on the
              Port Forwarding page.  Allows to distinguish manual forwards
              from upnp forwards.
   - CHANGED: The state of PPTP/L2TP client connections will be reported 
              on the VPN Status page.
   - CHANGED: Removed the display of global OpenVPN statistics on the
              VPN Status page.
   - CHANGED: Upgraded AiCloud binary components on MIPS routers to
              374_1631 build (N16/N66/AC66)
   - FIXED: OpenVPN clients with DNS set to "Strict" weren't properly
            setting dnsmasq to use "strict-order"
   - FIXED: Garbled resolv.conf generated when adding an OpenVPN client DNS
            to it
   - FIXED: OpenVPN Client static key was incorrectly processed when shown
            on the webui.


3.0.0.4.374.36 Beta 1 (23-Dec-2013):
   - NEW: Added ECDSA key support for SSH
   - NEW: postconf scripts.  This allow you to modify a generated
          config file (for example, smb.conf) before the service
          using it gets started.
   - NEW: layer7 Netfilter module on ARM devices (AC56, AC68).
          Note: traffic accounting must be manually enabled on
          these devices (see the Layer7 section in the FW's README)
   - CHANGED: Updated dropbear to 2013.62
   - CHANGED: Improved rendering of the VPN Status page
   - CHANGED: Extended retry period for WAN DHCP queries to 160 secs
              in Normal DHCP mode to give time to Charter to 
              unblacklist customers being accidentally blocked by them.
   - CHANGED: Downgraded rp-pppoe from 3.11 to 3.10 to see if it's
              more stable for some PPPoE users
   - FIXED: Some VPN client username/passwords were incorrectly handled
   - FIXED: When disabling Dual WAN, WAN unit wasn't being reset to 
            unit 0, preventing users from editing the correct unit 
            (Asus bug)
   - FIXED: If you replaced the Asus generated CA with your own, the 
            exported ovpn file would contain your CA with the 
            Asus-signed client cert/key.  Now, we only insert the 
            client cert/key if it was signed by the current CA.
   - FIXED: MSS clamping for clients connecting to the PPTPD server 
            (Asus bug)
   - FIXED: networkmap's DLNA detection was broken with some devices,
            and could result in very long delays during scan (Asus bug)
   - FIXED: Adjusted various timings in networkmap which should help 
            with device lists being incomplete especially after a 
            reboot.

  
3.0.0.4.374.35_4 (30-Nov-2013):
   - CHANGED: Added a VPN mode selector on the VPN Server Details page.
   - FIXED: JS error on the VPN Server Details page related to PPTP
   - FIXED: Clicking on "Apply" on VPN Details page would fail to
            apply your new settings to a running OpenVPN server.
   - FIXED: Some port forward rules were incorrectly generated when
            in load-balancing mode (Asus bug)
   - FIXED: After adding/removing a user to OpenVPN Server, the password
            file was not immediately updated.  Note that this fix will
            break backward compatibility with Asus as the nvram value
            storing the list of OpenVPN user/pass had to be renamed
            (so not to be instanced).
  - FIXED: VPN client not working on MIPS devices (N66/AC66).
  - FIXED: Various formatting issues with generated client.ovpn file


3.0.0.4.374.35_2 (24-Nov-2013):
   - FIXED: updown.sh script location was changed in
            339, causing issues with OpenVPN clients


3.0.0.4.374.35 (24-Nov-2013):
   - NEW: Merged with Asus 374_339 GPL (from RT-AC68U).
          Asus added some new features in this release:
          * Support for HFS+ and Time Machine (AC56/AC68U only)
          * OpenVPN support.  Their implementation uses the backend
            code from Asuswrt-Merlin but with a more
            simplistic, novice-friendly webui.  This required 
            adapting the current webui to be able to retain some 
            of their improvements without sacrificing the 
            flexibility of being able to have two separate server 
            and client configurations.

   - NEW: Support for Namecheap DDNS (Patch provided by saintdev)
   - NEW: Added qos-start user script
   - FIXED: Incorrect range validation for UPnP ports on WAN page.
   - FIXED: Accidentaly lock out of webui due to software hammering
            the router's webui without valid login credentials
   - FIXED: NAT Loopback broken with CTF enabled (AC56/AC68) (Asus bug)
   - FIXED: Backing up your settings would return an empty CFG file.
   - FIXED: Kernel panic when inserting ebtables rule (AC56/AC68,
            fix backported from kernel 2.6.37)
   - FIXED: If an IP/CIDR on the IPv6 firewall page was long enough
            to be shortened with "..." it would be incorrectly saved.
   - CHANGED: IPTraffic will now account for traffic going through
              an OpenVPN tunnel
   - CHANGED: VPN webui is now an hybrid of our original webui,
              along with Asus's own.  This allows the addition
              of these features developed by Asus:
              * Ability to export an ovpn config file to give to
                your clients
              * Support for username/password authentcation on
                the built-in server
              * Ability to import a tunnel provider's .ovpn
                config file to configure a client connection
                on the router


3.0.0.4.374.34_2 (01-Nov-2013):
   - FIXED: DNS resolution not working for VPN clients
            (bug in Asus 374_979)
   - FIXED: USB disk detection on AC56/AC68.
   - FIXED: Turbo mode option couldn't be saved (RT-AC68)


3.0.0.4.374.34 (30-Oct-2013):
   - NEW: Merged with Asus 374_979 (from RT-N66U).  
          AC56/AC68 AiCloud components taken from 374_217.
   - NEW: Added RT-AC68U support.
   - NEW: Added IPSec support to the kernel.  Userspace tools 
          such as StrongWAN must be installed from Optware/Entware,
          and manually configured.  (Patch provided by saintdev)
   - NEW: Adjustable MTU for DHCP/static IP WAN users
   - NEW: WAN interface name passed as argument to firewall-start
   - NEW: Configurable min/max ports allowed to be redirected by UPNP.
          This allows WHS users to change the min allowed port from 
          the default value of 1024 to allow UPNP forwarding of 
          HTTP/HTTPS.
   - NEW: Display CPU temperature on Sysinfo page (AC56 and AC68)
   - NEW: Display CPU chart on Performance page (AC56 and AC68)
   - CHANGED: UPnP rules will now be processed after manual 
              forwards and port trigger rules.
   - CHANGED: Site Survey now reports supported protocol.
   - CHANGED: Updated Dropbear to 2013.60.
   - CHANGED: Updated dnsmasq to 2.67 final.
   - FIXED: Some Traffic Monitor pages were missing the page tabs.
   - FIXED: The webui would allow you to enable SSHD while not 
            setting an authkey or enabling password-based authentication.
   - FIXED: 802.11h options should only be available on the 5 GHz band.
   - FIXED: Wifi icon hover would report 5G channel as undefined if
            2.4GHz radio was disabled.
   - FIXED: IPv6 clients list failed to properly merge IPs from similar 
            MACs (Asus bug)
   - FIXED: Minor layout issues with the Clients list
   - FIXED: Samba wasn't started at boot time if browser master or WINS
            was enabled and we had no USB disk plugged in.
   - FIXED: Router/minidlna crashes when processing very large image
            collections - various memory leaks plugged.
            (patches provided by Paulo Capani)
   - FIXED: Buffer overrun when entering more than 35 MACs on the
            filter list.  We now support up to 64 MACs.


3.0.0.4.374.33 (3-Oct-2013):
 * IMPORTANT *: RT-N66U users must revert back to factory defaults and
                manually reconfigure their settings if coming from a FW
                older than 3.0.0.4.374.xxx (applies to both Asus or
                Asuswrt-Merlin).

   - NEW: Merged with Asus 374_726 code from RT-AC66U GPL.  Notable changes:
            * RT-N66U now based on the SDK6 driver.  This resolved the
              numerous connectivity issues, at the expense of a shorter
              range (a separate SDK5 build based on driver 5.100 is 
              available in the Experimental folder as an alternative).
            * AiCloud 2.0
   - NEW: Added bonding.ko kernel module.
   - NEW: Repeater mode moved into regular builds.
   - NEW: Dual WAN moved into regular builds.
          Note that there are still a few issues left, such as recovery
          from failover mode when the primary WAN comes back up.
   - NEW: YandexDNS support moved into regular builds.  This is
          a DNS-based filter list, which can be configured under 
          Parental Control.
   - NEW: Added support for last seen devices on Ethernet port status 
          (Tools-> Sysinfo) for RT-AC56U.
   - NEW: Option to control 802.11 extensions that deal with
          regulations.  On the Wireless Professional page
          you can now enable 802.11d and 802.11h support.
   - CHANGED: robocfg now (almost) completely supports the
              Northstar platform (RT-AC56U)
   - CHANGED: Enabled Syn Cookies for ARM devices (RT-AC56U)
   - CHANGED: Allow selecting the Download2 folder for media server
              location.
   - CHANGED: MIPS builds optimized for mips32r2 code generation, which
              should improve general performance. (N16/N66/AC66)
   - CHANGED: More openssl backports from 1.0.2, adding
              mips32r2 support, improving performance
              especially for sha1 (RT-N16/N66/AC66)
   - CHANGED: Increased OpenVPN crt/key fields to allow up to 3499 
              characters - enough to accomodate even a 4096 bits key.
   - CHANGED: Removed the firewall rules for acsd since it no longer
              listens on a TCP socket.
   - FIXED: Samba binding to WAN interface would cause warnings
            about WINS/master browser (regression in 374)
   - FIXED: The ARM kernel was missing the Advanced IP Routing option,
            preventing some of the "ip" command functions from
            working (was breaking Astrill's plugin) (RT-AC56U)
   - FIXED: With FW 374 Asus changed the Samba priority from too high to
            too low (-19), resulting in poor sharing performance.
            I changed it to a priority of 0, providing more balanced 
            performance. (N16/N66/AC66)
   - FIXED: Some fields would allow invalid characters (such as
           single quotes) which might break the webui JS.  There might 
           still be a few unprotected fields.
   - FIXED: Memory leak in httpd service (Asus bug)
   - FIXED: Parental Control not working with certain schedules
            (patch provided by Makkie2002)
   - FIXED: Potential key truncation in httpd if one was to use very 
            large OpenVPN keys and certs in all fields of all four 
            instances.
   - FIXED: Samba would start sharing local disks even if all you 
            wanted was its WINS/Browser services.
   - FIXED: The JFFS formatting code could encounter a case
            where it wouldn't write back its cleared
            format flag.
   - FIXED: Restarting the wireless service would break
            stealth mode.
   - FIXED: The new thumbnail cache code Asus added in build 720's
            minidlna will prevent scanning from completing on very
            large collections.  Reverted that code for now.
   - FIXED: Wireless key field was automatically activated on
            page load, which could lead to accidental changes
            (issue introduced in 374_720).
   - FIXED: Router believed that NTP wasn't properly working after a
            LAN or wireless service restart (issue introduced in
            374_720).
   - FIXED: IPv6 client list was incorrectly displayed if a client
            didn't have a known hostname (Asus bug)


3.0.0.4.374.32 (24-Aug-2013):
   - NEW: Merged with Asus 374_168 GPL code.
   - NEW: wan-start script will get passed the WAN unit number as 
          argument
   - NEW: Webui option to select the location of the DLNA database 
          (patch by VinceV)
   - NEW: IPv6 firewalling.  Originally, Asuswrt would allow any IPv6 
          traffic to be forwarded to your LAN devices.  This new option 
          (enabled by default) will prevent traffic forwarding to LAN 
          devices.  You can also create firewall rules to allow inbound 
          traffic to specific hosts.  The firewall configuration can be 
          accessed through the "Firewall -> IPv6 Firewall" page.
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
            Control calendar under IE.
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
              means that HW acceleration for PPPoE is no longer 
              available for the RT-N66U, as it was new in the 5.110 SDK.
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
   - FIXED: Reapply layout fixes to Guest network and DHCP page (were 
            lost in a recent webui update)
   - FIXED: JFFS2 could get reformated again at each subsequent reboots.
   - FIXED: Devices with a NetBIOS name of 15 chars long would have 
            their name merged with the next device's.
   - FIXED: Empty Site Survey list if there was only one AP found
   - FIXED: Saved settings might fail to restore if they contained 
            OpenVPN or SSHD keys with CRLF line endings.  You should
            access the OpenVPN Keys page, click on Apply to re-save 
            them, then re-create any backup you had of your router 
            settings.
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
   - FIXED: PPTP/L2TP Internet connection unable to reconnect after  
            going down (unsure if Asus bug or 358.28 bug)
   - FIXED: Numerous bugs in the Per IP traffic monitoring causing 
            inaccurate traffic accounting if there was too much 
            traffic, or if update requests occured in a too short 
            period of time.
   - FIXED: Asking for traffic monitoring (regular and Per IP) database 
            to be re-created would re-create it again on next reboot if 
            in the mean time you didn't change any other settings, 
            causing the flag not to be cleared on the mtd partition.


3.0.0.4.354.28 Beta 1 (19-Apr-2013):
   - KNOWN ISSUE: 5 GHz 40 MHz is unreliable with some wireless 
                  cards (RT-N66U)
   - KNOWN ISSUE: Sort order is sometimes wrong on the Site Survey page
   - NEW: Wireless site survey (on the Wireless tab)
   - NEW: openvpn-event user script that gets run when a tunnel goes 
          up/down.  Read the OpenVPN documentation on the "up" and 
          "down" events for more on how to use this script, and to use 
          the passed parameters.
   - NEW: Enabled sftp support in Dropbear (the sftp server must be
          installed from Entware)
   - NEW: Option to prevent SSH port hammering (patch submited by dodava)
   - CHANGED: Merged with webui pages extract from the Asus released FW, 
              as they were more recent than the GPL ones
   - CHANGED: Port state on Sysinfo page now uses the new OUI lookup 
              code from Asus
   - CHANGED: Try to report on Sysinfo what is forcing HW acceleration 
              to be disabled
   - FIXED: Build 354 reduced minimum syslog level to WARNING - bumped 
            back to INFO as in previous versions (resolves DHCP events 
            not being logged).  Also ensured we readjusted it if DHCP 
            logging is enabled, to handle routers that got upgraded 
            with the new loglevel already set.
   - FIXED: Port numbering on the Sysinfo page for devices that has 
            them backward (untested) (RT-N16)
   - FIXED: Client list wasn't using the new OUI code from Asus (was 
            missing from the GPL archive)
   - FIXED: LAN traffic going through the NAT loopback would be counted 
            in the Per IP traffic monitoring.
   - FIXED: IE rendering of the Other Settings page when toggling Per 
            IP monitoring
   - FIXED: Cannot set webui to HTTPS-only (causes port conflict error) 
            (Asus bug in 354)
   - FIXED: Cannot create/modify folders in AiDisk
   - FIXED: Couldn't resolve LAN hostnames if WAN was down (the web 
            redirection would hijack all DNS queries).  Now, we let 
            dnsmasq handle both LAN and redirected queries.
  - FIXED: Fixed support for Broadcom Wimax devices
  - FIXED: smbpasswd wasn't properly updated when deleting a user 
           (Asus bug)
  - FIXED: Aicloud: handling of disks with multiple partitions on the 
           webui (Asus bug) (fix submitted by hshang)


3.0.0.4.354.27 Beta 1 (31-Mar-2013):
   - NEW: Merged with 3.0.0.4.354.  Notable changes:
            * New wireless driver
            * New Network Tools
            * WOL (Under Network Tools
            * HW acceleration support for PPPoE
            * DHCP Normal/aggressive behaviour.  Similar to the 270.25 
              implementation, except it can be enabled/disabled

          Asus considers build 354 to still be beta, so be advised that 
          there might still be some issues left (there are known issues 
          related to 3G/4G dongles for instance).

   - CHANGED: Removed WOL webui - Asus added their own WOL support  on 
              the Network Tools page.  You will have  to re-add your 
              WOL entries.
   - CHANGED: Removed System Log -> Connections page, and integrated it 
              into the new Network Tools -> Netstat page from Asus (as 
              NAT Connections)
   - CHANGED: Removed wol binary, since Asus's WOL page uses ether-wake.
   - CHANGED: Removed option to control SIP helper on Firewall page 
              (use the new Asus option from WAN - NAT PAssthrough page 
              instead)
   - CHANGED: WPS button when set as a radio toggle will now behave the 
              same way as Asus's firmware: pressing it will fully 
              enable/disable both radios in the webui, rather than just 
              toggle the state of the enabled radios.  This means the 
              button will override the webui, and radio states will 
              survive reboots.
   - FIXED: Avoid duplicate shares when using simpler share naming 
            using Asus's code from 354)
   - FIXED: Improved fdisk support for 4KB sector size
   - FIXED: openvpn: Client-specific entries weren't properly parsed
   - FIXED: dnsmasq warning in syslog if DHCP static leases are disabled


3.0.0.4.270.26b (17-Mar-2013):
   - FIXED: Volume labels with spaces were rejected (Asus used the same 
            code to validate hostnames and volume labels)


3.0.0.4.270.26 (15-Mar-2013):
   - NEW: ipset Netfilter support + userspace tool to create ipset lists.
   - CHANGED: Router's hostname is now set all the time, regardless of
              telnet/ssh states (and including in AP mode)
   - CHANGED: Added device name field on the LAN page, since it's now 
              relevant to the router's hostname (not just SMB).  Left 
              it on the SMB page as well, for those used to see it there.
   - CHANGED: Router will supply its device name when requesting an IP 
              while in AP mode.
   - CHANGED: Various webui lists were increased from 32 to 128 entries 
              allowed.
   - CHANGED: Improved networkmap:
               * Will also use DHCP hostnames and user-defined static
                 names instead of just NetBIOS names
               * Client list will show an animation while networkmap is
                 still busy scanning and resolving device names
               * Dropdown menus that use Networkmap to build a list
                 of devices will also display names in addition to IP/MAC.
   - CHANGED: Don't restart the whole network if you only changed DHCP 
              reservations (LAN -> DHCP page)
   - FIXED: Openvpn: Non-CBC ciphers weren't working (their use is still 
            not recommended)
   - FIXED: Proxy auto-configuration support (Asus bug)


3.0.0.4.270.25b (3-Mar-2013):
   - FIXED: Disabling DHCP logging would cause a syntax error in 
            dnsmasq's configuration (regression from dnsmasq update)
   - FIXED: Outbound VPN client traffic was dropped (regression from 
            firewall_2 fix)


3.0.0.4.270.25
   - NEW: NFS folder sharing.  Webui can be found on the 
          USB Applications -> Servers Center page (NFS Exports tab)
   - NEW: dhcpc-event and zcip-event scripts (called on WAN events)
   - NEW: Ccustom configs: group.add, gshadow.add, passwd.add, 
          shadow.add, exports.add
   - NEW: New script that will setup Entware for you (written by 
          ryzhov_al).  Run "entware-setup.sh" through SSH/Telnet to 
          launch the install process.
   - CHANGED: Added a folder picker to the Tools Other Settings page to 
              select a location to store your traffic data files.
   - CHANGED: Updated dnsmasq to 2.65 (backported from 3.0.0.4.334)
   - CHANGED: Enabled additional optimizations for openssl and openvpn 
              for a significant performance gain
   - CHANGED: Reverted wireless driver to build 220 (RT-AC66U only)
   - FIXED: Added missing badblocks program
   - FIXED: Timing issues under IE where resolved device names would 
            not display on certain pages (such as the Sysinfo page)
   - FIXED: VPN client "common name" wasn't getting saved
   - FIXED: DHCP client will be less aggressive in attempting to obtain
            a lease (wait 2 mins instead of 20 secs between attempts),
            should help with ISPs like Charter who will blacklist you 
            if you send too many Discovery packets in a short period of 
            time.
   - FIXED: Made profile.add be run after any Optware profile, so the 
            user changes will have priority over anything else.
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
   - NEW: Replaced Busybox fsck/mkfs tools with those from e2fsprogs, 
          should be more reliable.
   - CHANGED: Temperatures on Sysinfo page will now auto-update every 3 
              seconds.
   - CHANGED: Connections page now uses Ajax for slightly better rendering
   - CHANGED: Improved name resolution on traffic monitor page, now uses
              a device's hostname if it reported one.
   - CHANGED: Client List now uses our improved name resolution code,
              will overwrite names with those entered on the DHCP static
              lease page.
   - CHANGED: Updated to OpenVPN 2.3.0 and lzo 2.06.
   - CHANGED: Updated Busybox to 1.20.2 (with Oleg/wl500g patches 
              re-applied).  Lots of fixes, including GPT support in 
              fdisk.
   - CHANGED: Updated Miniupnpd to version 1.8.  NOTE: previous 
              versions were NOT affected by the recent UPNP exploit 
              disclosure.  This is just as an added security precaution.
   - FIXED: Temperature on Performance Tuning page would fail to update 
            if a radio was disabled.
   - FIXED: Various timing issues causing some TrafficMonitoring and the
            Sysinfo pages to often fail loading under IE.
   - FIXED: JS error on the Per Device pages if FW failed to load the 
            traffic history.
   - FIXED: ebtables were still broken, fixed by a complete rebuild.
   - FIXED: Some OpenVPN fields rejected -1 as being valid.
   - FIXED: Hide 5G radio info from Sysinfo page if router is \
            single band (RT-N16)
   - FIXED: Master Browser/WINS would not work if there was no USB disk 
            plugged.
   - FIXED: Samba would bind to the WAN interface while in router mode 
            (Asus bug)
   - FIXED: Backported various kernel fixes from Oleg/WL500G, Tomato 
            and Kernel.org to help improve HDD > 2 TB support (still 
            not perfect, some USB enclosures are simply not Linux 
            compatible)
   - FIXED: Display of Connections under IE
   - FIXED: Trying to apply settings on the System page with a username 
            containing a non-alphanum would incorrectly assume you just 
            tried to change to an account name that already existed 
            (Asus bug).
   - FIXED: Wouldn't enable wins in Samba if you had a WINS IP entered 
            on the DHCP configuration page.


3.0.0.4.266.23b (31-Dec-2012):
   - FIXED: The IE fix ended up breaking Firefox (and meanwhile, Chrome 
            worked fine no matter which method was used to build that 
            dropdown).

3.0.0.4.266.23 (31-Dec-2012):
   - NEW: Rebased on 3.0.0.4.266 (from the RT-AC66U GPL)
   - NEW: Tools icon contributed by Maximilian Czarnecki.
   - FIXED: Skip bad blocks while erasing MTD partition (fixes RT-AC66U
            failing to format JFFS2 partition due to bad blocks)
   - FIXED: Router would have no hostname if you enabled ssh but kept
            telnet disabled.
   - FIXED: Couldn't add new ebtables rules (regression in 264.22)
   - FIXED: customized minidlna.conf
   - FIXED: Traffic monitoring per IP is unreliable if HW acceleration
            is enabled.  Do not load CTF if booting with cstats enabled.
   - FIXED: Per Device traffic monitor pages missing under IE


3.0.0.4.264.22 (15-Dec-2012):
   - NEW: Rebased on 3.0.0.4.264 (from the RT-N53 GPL).
   - NEW: Traffic monitoring per IP added to the Traffic Monitor section.
          Based on the Tomato IPTraffic implementation by Teaman.
   - NEW: Option to disable the Netfilter SIP helper (Firewall page), 
          allows people to manually forward port 5060 to their own SIP 
          server
   - NEW: Option to enable/disable logging DHCP client queries 
          (LAN->DHCP page)
   - FIXED: Tabs would disappear while on the Monthly traffic page.
   - FIXED: Really fixed Firefox issue (the fix wasn't merged
            in release 260.21).
   - FIXED: Router crash if the list of MAC filters + their names got 
            too long.
   - FIXED: OpenVPN webui: TLS Reneg and Connection Retry wouldn't let 
            you enter -1 as value.
   - FIXED: Layout issues on the DHCP page (one in Asus code, another 
            in Merlin code)
   - FIXED: Beeline Corbina was unable to connect to PPTP/L2TP server 
            due to DNS issues.
   - CHANGED: System log starts at the bottom (backported from GPL 314)
   - CHANGED: Dual WAN is no longer enabled in regular builds - too many 
              issues with it at this point.  Regular USB failover 
              still works.


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
   - NEW: Wifi status icon will be half colored if only one radio is 
          enabled.
   - NEW: Wifi status icon popup will report the state of each radios.
   - NEW: upnp custom config file for miniupnpd
   - NEW: unmount user script
   - NEW: led_ctrl and makemime (for use in conjunction with sendmail) 
          applets.
   - NEW: Implemented control for network switch LEDs (all four at once)
   - NEW: Stealth Mode: option to disable all LEDs
   - NEW: Added CONFIG_IP_NF_RAW and CONFIG_NETFILTER_XT_TARGET_NOTRACK 
          modules.
   - FIXED: Radio toggle through WPS button would be overriden by a 
            scheduled radio.  Reverted "switch" to "toggle" code to 
            prevent this.
   - FIXED: You couldn't disable DMZ by clearing the IP field.
   - FIXED: You couldn't edit entered text in DHCP/MAC/etc name field
   - FIXED: clientid passing for some ISPs requiring it (like Sky UK)
            was broken with the DHCP client change of build 220.
   - FIXED: No longer reboot the router three times during boot time if 
            one of the radios is disabled by the user. (RT-N66U)
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
            o New "Enhanced interference management" option under 
              Wireless -> Professional.
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
Website: http://asuswrt.lostrealm.ca/
Github: https://github.com/RMerl/asuswrt-merlin
Email: rmerl@lostrealm.ca
Twitter: https://twitter.com/RMerlinDev
IRC: #asuswrt on DALnet
Download: http://asuswrt.lostrealm.ca/download

Development news will be posted on Twitter.  You can also keep a closer 
eye on development as it happens through the Github site.

For support questions, please use the SmallNetBuilder forums whenever 
possible.  There's a dedicated Asuswrt-Merlin sub-forum there, under 
the Asus Wireless section.

Drop me a note if you are using this firmware and are enjoying it.  If 
you really like it and want to give more than a simple "Thank you", 
there is also a Paypal donation button on my website.

I want to give my special thanks to Asus for showing an interest in 
this project, and also providing me with support and development 
devices when needed.  I also want to thank everyone that has 
donated through Paypal.  Much appreciated!

Finally, special thanks to r00t4rd3d for designing the Asuswrt-Merlin 
logo.



Disclaimer
----------
This is the part where you usually put a lot of legalese stuff that nobody 
reads. I'm not a lawyer, so I'll just make it simple, using my own words 
rather than some pre-crafted text that will bore you to death and that 
nobody but a highly paid lawyer would even understand anyway:

I take no responsibility for issues caused by this project. I do my best to 
ensure that everything works fine. If something goes wrong, my apologies.

Copyrights belong to the appropriate individuals/entities, under the appropriate 
licences. GPL code is covered by GPL, proprietary code is (c)Copyright their 
respective owners, yadda yadda.

I try my best to honor the licences (as far as I can understand them, as a 
normal human being). Anything GPL or otherwise open-sourced that I modify 
will see my changes published to Github at some point. A release might get 
delayed if I'm working using pre-release code. If it's GPL, it will eventually 
be published - no need to send a volley of legal threats at me.

In any other cases not covered, Common Sense prevails, and I shall also make use 
of Good Will.

Concerning privacy:

This firmware does not contact me back in any way whatsoever. Not even through 
any update checker - the only update code there is Asus's.


--- 
Eric Sauvageau
