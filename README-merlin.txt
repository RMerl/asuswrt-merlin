Asuswrt-Merlin - build 378.56 (25-Oct-2015)
===========================================

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
 * RT-N66U
 * RT-AC66U
 * RT-AC56U
 * RT-AC68U
 * RT-AC68P
 * RT-AC87U
 * RT-AC3200
 * RT-AC88U

Devices that are no longer officially supported (but forked builds might
exist from other developers):
 * RT-N16


NOTE: all the "R" versions (for example RT-N66R) are the same as their 
"U" counterparts, they are just different packages aimed at large 
retailers.  The firmware is 100% compatible with both U and R versions 
of the routers.  Same with the "W" variants that are simply white.


Features
--------
Here is a list of features that Asuswrt-merlin adds over the original 
firmware:

System:
   - Based on 3.0.0.4.378_9177 source code from Asus
   - Various bugfixes and optimizations
   - Some components were updated to newer versions, for improved
     stability and security
   - User scripts that run on specific events
   - Cron jobs
   - Ability to customize the config files used by the router services
   - LED control - put your router in "Stealth Mode" by turning off 
     all LEDs
   - Entware easy setup script (alternative to Optware - the two are 
     mutually exclusive)
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
   - Advanced NAT loopback (as an alternative to the default one)
   - TOR support, individual client control
   - Policy routing for the OpenVPN client (based on source or
     destination IPs), sometimes referred to as "selective routing")


Web interface:
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring: added monthly, as well as per IP 
     monitoring
   - Name field on the DHCP reservation list and Wireless ACL list
   - System info summary page
   - Wifi icon reports the state of both radios
   - Display the Ethernet port states
   - Wireless site survey
   - Advanced Wireless client list display, including automated refresh
   - Redesigned layout of the various System Log sections
   - Editable fields for some pages


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

The option is enabled by default.  You can however disable it (NOT
recommended, as various features such as the Traffic Analyzer 
will depend on it), or, reformat it from the 
Administration -> System page.

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
 * post-mount: Just after a partition is mounted
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
 * torrc (for the Tor config file)
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
 * torrc.postconf
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
ext2/ext3/ext4 formatted USB disk (NTFS, HFS+ and FAT32 are not supported).

Since 378.51 Entware also has a ARM-based repository, for 
AC56/AC68/AC87/AC3200 routers, provided by Zyxmon.



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



OpenVPN client policy routing
-----------------------------
When configuring your router to act as an OpenVPN client (for instance 
to connect your whole LAN to an OpenVPN tunnel provider), you can 
define policies that determines which clients, or which destinations 
should be routed through the tunnel, rather than having all of your
traffic automatically routed through it.

On the OpenVPN Clients page, set "Redirect Internet traffic" to 
"Policy Rules".  A new section will appear below, where you can 
add routing rules.  The "Source IP" is your local client, while 
"Destination" is the remote server on the Internet.  The field can be 
left empty (or set to 0.0.0.0) to signify "any IP".  You can also 
specify a whole subnet, in CIDR notation (for example, 74.125.226.112/30).

The Iface field lets you determine if matching traffic should be sent 
through the VPN tunnel or through your regular Internet access (WAN).
This allows you to define exceptions (WAN rules being processed 
before the VPN rules).

Here are a few examples.

To have all your clients use the VPN tunnel when trying to 
access an IP from this block that belongs to Google:

	RouteGoogle	0.0.0.0		74.125.0.0/16	VPN

Or, to have a computer routed through the tunnel except for requests sent
to your ISP's SMTP server (assuming a fictious IP of 10.10.10.10 for your 
ISP's SMTP server):

	PC1		192.168.1.100	0.0.0.0		VPN
	PC1-bypass	192.168.1.100	10.10.10.10	WAN

Another setting exposed when enabling Policy routing is to prevent your 
routed clients from accessing the Internet if the VPN tunnel goes down.  
To do so, enable "Block routed clients if tunnel goes down".



Source code
-----------
The source code with all my modifications can be found on Github, at:

https://github.com/RMerl/asuswrt-merlin


   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://www.snbforums.com/forums/asuswrt-merlin.42/ as RMerlin)
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
