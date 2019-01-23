Asuswrt-Merlin
==============

About
-----
Asuswrt is the name of the firmware Asus has developed for
their various router models.  Originally forked from Tomato, 
it has since grown into a very different product, removing 
some more technical features that were part of Tomato, but 
also adding a lot of new original features.

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
Asuswrt-Merlin is available in two separate branches:

- The original (legacy) Asuswrt-Merlin (up to version 380.xxx)
- The new generation (current) branch (version 382.xxx and newer)

The legacy 380.xx branch is no longer being actively developed.  
Support for the RT-N66U and RT-AC66U is being dropped, as 
these models will not be available on the new generation code base.



Devices supported on the legacy branch (380.xx):
 * RT-N66U
 * RT-AC66U
 * RT-AC66U_B1 (use the RT-AC68U firmware)
 * RT-AC56U
 * RT-AC68U, RT-AC68P, RT-AC68UF (including HW revision C1 and E1)
 * RT-AC87U
 * RT-AC3200
 * RT-AC88U
 * RT-AC3100
 * RT-AC5300
 * RT-AC1900 & RT-AC1900P (use the RT-AC68U firmware)

Devices supported on the new generation/current branch (382.xx and newer):
 * RT-AC66U_B1 (use the RT-AC68U firmware)
 * RT-AC56U
 * RT-AC68U, RT-AC68P, RT-AC68UF (including HW revision C1 and E1)
 * RT-AC1900 & RT-AC1900P (use the RT-AC68U firmware)
 * RT-AC87U
 * RT-AC3200
 * RT-AC88U
 * RT-AC3100
 * RT-AC5300
 * RT-AC86U

No longer supported:
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
   - Various bugfixes and optimizations
   - Some components were updated to newer versions, for improved
     stability and security
   - User scripts that run on specific events
   - Cron jobs
   - Ability to customize the config files used by the router services
   - Ability to turn off router LEDs
   - Entware easy setup script (alternative to Optware - the two are 
     mutually exclusive) (unavailable on RT-AC86U)
   - SNMP support (except for the RT-AC86U)
   - Nano text editor (for more user-friendly script editing)


Disk sharing:
   - Enable/disable the use of shorter share names
   - NFS sharing (through webui)
   - Allow or disable WAN access to the FTP server
   - Updated Samba version (3.6), with SMB2.0 support
   - TLS support for the FTP server


Networking:
   - Force acting as a Master Browser
   - Act as a WINS server
   - SSHD support for key-based authentication
   - Allows tweaking TCP/UDP connection tracking timeouts
   - CIFS client support (for mounting remote SMB share on the router)
   - User-defined options for WAN DHCP queries (required by some ISPs)
   - Advanced OpenVPN client and server support
   - Support for new OpenVPN 2.4 features like NCP and LZ4
   - Netfilter ipset module, for efficient blacklist implementation
   - Configurable min/max UPNP ports
   - DNS-based Filtering, can be applied globally or per client
   - Custom DDNS (through a user script)
   - TOR support, individual client control (based on experimental code
     from Asus)
   - Policy routing for the OpenVPN client (based on source or
     destination IPs), sometimes referred to as "selective routing")
   - DNSSEC support
   - fq_codel queue discipline for QoS (ARM-based models only)
   - Full cone NAT support (RT-AC86U only)
   - Detailed wireless troubleshooting information (RT-AC86U only)


Web interface:
   - Performance improvements
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring with graphical reports of
     historical data
   - Traffic report per IP (except on RT-AC86U)
   - Hostname field on the DHCP reservation page
   - System information summary page
   - Wifi icon reports the state of both radios
   - Wireless site survey
   - Advanced Wireless client list display, including automated refresh
   - Redesigned layout of the various System Log sections
   - Editable fields for some pages
   - User-provided SSL certificate


Some features that first appeared in Asuswrt-Merlin have since been 
integrated/enabled/re-implemented in the official firmware:

- HTTPS webui
- Turning WPS button into a radio on/off toggle
- Use shorter share names (folder name only)
- WakeOnLan web interface (with user-entered preset targets)
- clickable MACs on the client list for lookup in the OUI database
- Display active/tracked network connections
- VPN client connection state report
- OpenVPN client and server
- Configurable IPv6 firewall
- Persistent JFFS partition
- The various MAC/IP selection pulldowns will also display hostnames
  when possible instead of just NetBIOS names
- SSHD
- Improved compatibility with 3TB+ and Advanced Format HDDs
- Display the Ethernet port states
- Disk spindown after user-configurable inactivity timeout



Installation
------------
Simply flash it like any regular update.  You should not need to 
reset to factory defaults (see note below for exceptions).
You can revert back to an original Asus firmware at any time just
by flashing a firmware downloaded from Asus's website.

If the firmware upgrade fails, try rebooting your router to free 
up sufficient memory, without any USB disk plugged in,
then try flashing it again.

NOTE: resetting to factory default after flashing is 
strongly recommended for the following cases:

- Updating from a firmware version that is more than 3 releases older
- Switching from a Tomato/DD-WRT/OpenWRT firmware

If you run into any issue after an upgrade and you haven't done so,
try doing a factory default reset as well.

Always read the changelog, as mandatory resets will be mentionned 
there when they are necessary.

In all of these cases, do NOT load a saved copy of your settings!
This would be the same thing as NOT resetting at all, as you will 
simply re-enter any invalid setting you wanted to get rid of.  Make 
sure to create a new backup of your settings after reconfiguring.



Documentation
-------------
For documentation on how to use the features that are specific to 
Asuswrt-Merlin, as well as additional guides, please consult the
wiki:

https://github.com/RMerl/asuswrt-merlin/wiki



Source code
-----------
The source code can be found on Github.

Original legacy branch:
https://github.com/RMerl/asuswrt-merlin

New generation/current branch (382.xx and newer):
https://github.com/RMerl/asuswrt-merlin.ng

   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://www.snbforums.com/forums/asuswrt-merlin.42/ as RMerlin)
Website: https://asuswrt.lostrealm.ca/
Github: https://github.com/RMerl
Email: rmerl@lostrealm.ca
Twitter: https://twitter.com/RMerlinDev
IRC: #asuswrt on Freenode
Download: https://asuswrt.lostrealm.ca/download

Development news will be posted on Twitter and the support forums.  
You can also keep a closer eye on development as it happens, through 
the Github code repository.

For support questions, please use the SmallNetBuilder forums whenever 
possible.  There's a dedicated Asuswrt-Merlin sub-forum there, under 
the Asus Wireless section.  The community there is the primary source 
of technical support.

I want to give my special thanks to Asus for showing an interest in 
this project, and also providing me with support and development 
devices when needed.  I also want to thank everyone that has 
donated through Paypal.  Much appreciated!

Finally, my special thanks to r00t4rd3d for designing the 
Asuswrt-Merlin logo.



Disclaimer
----------
This is the part where you usually put a lot of legalese stuff that nobody 
reads. I'm not a lawyer, so I'll just make it simple, using my own words 
rather than some pre-crafted text that will bore you to death and that 
nobody but a highly paid lawyer would even understand anyway:

I take no responsibility for issues caused by this project. I do my best to 
ensure that everything works fine. If something goes wrong, my apologies.

The Asuswrt-merlin firmware is released under a GPL licence.  In short, you 
are free to use, redistribute and modify it, as long as all the associated 
licences are respected, and that any changes you make to the GPL code is 
made publicly available.

Copyrights belong to the appropriate individuals/entities, under the appropriate 
licences. GPL code is covered by GPL, proprietary code is Copyright their 
respective owners, yadda yadda.

I try my best to honor the licences (as far as I can understand them, as a 
normal human being). Anything GPL or otherwise open-sourced that I modify 
will see my changes published to Github at some point. A release might get 
delayed if I'm working using pre-release code. If it's GPL, it will eventually 
be published - no need to send a volley of legal threats at me.

In any other cases not covered, Common Sense prevails, and I shall also make use 
of Good Will.

Concerning privacy:

The only call back made by this firmware to me is when it checks for the
availability of a new version.  The automated check can be disabled if desired.
More info on the Wiki:

https://github.com/RMerl/asuswrt-merlin/wiki/RMerl/asuswrt-merlin/wiki/Privacy-disclosure


--- 
Eric Sauvageau
