Asuswrt-Merlin - build 380.61 (3-Aug-2016)
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
 * RT-AC68U & RT-AC68P
 * RT-AC87U
 * RT-AC3200
 * RT-AC88U
 * RT-AC3100
 * RT-AC5300
 * RT-AC1900 & RT-AC1900P (use the RT-AC68U firmware)

Devices that are no longer officially supported:
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
   - Layer7 iptables matching (N66/AC66 only)
   - User-defined options for WAN DHCP queries (required by some ISPs)
   - Advanced OpenVPN client and server support
   - Netfilter ipset module, for efficient blacklist implementation
   - Configurable min/max UPNP ports
   - IPSec kernel support (N66/AC66 only)
   - DNS-based Filtering, can be applied globally or per client
   - Custom DDNS (through a user script)
   - Advanced NAT loopback (as an alternative to the default one)
   - TOR support, individual client control (based on experimental code
     from Asus)
   - Policy routing for the OpenVPN client (based on source or
     destination IPs), sometimes referred to as "selective routing")
   - DNSSEC support
   - Experimental support for fq_codel in Traditionnal QoS
     (ARM-based models only)


Web interface:
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring: added monthly, as well as per IP 
     monitoring
   - Hostname field on the DHCP reservation page
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
Asuswrt-Merlin, please visit the Wiki:

https://github.com/RMerl/asuswrt-merlin/wiki



Source code
-----------
The source code with all my modifications can be found on Github, at:

https://github.com/RMerl/asuswrt-merlin


   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://www.snbforums.com/forums/asuswrt-merlin.42/ as RMerlin)
Website: https://asuswrt.lostrealm.ca/
Github: https://github.com/RMerl/asuswrt-merlin
Email: rmerl@lostrealm.ca
Twitter: https://twitter.com/RMerlinDev
IRC: #asuswrt on DALnet
Download: https://asuswrt.lostrealm.ca/download

Development news will be posted on Twitter.  You can also keep a closer 
eye on development as it happens through the Github site.

For support questions, please use the SmallNetBuilder forums whenever 
possible.  There's a dedicated Asuswrt-Merlin sub-forum there, under 
the Asus Wireless section.

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

This firmware does not contact me back in any way whatsoever. Not even through 
any update checker - the only update code there is Asus's.


--- 
Eric Sauvageau
