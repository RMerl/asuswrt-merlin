Asus RT-N16U Modded Firmware - build 3.0.0.3.144.11 (xx-July-2012)
==================================================================

About
-----
These builds of the RT-N16 firmware are versions I have modified and 
recompiled.  The goal is to do some minor improvements to Asus' firmware, 
without targeting at full-blown featuresets such as provided by excellent 
projects such as Tomato or DD-WRT.  This aims to be a more restrained 
alternative for those who prefer to stay closer to the original firmware.

The list of changes (so far):

- Based on the source code of release 3.0.0.3.144
- Added wol binary (wake-on-lan) (in addition to ether-wake already in the firmware)
- Added Tools menu to web interface (with WakeOnLan page)
- Added JFFS partition support (configurable under Administration->Advanced->System)
- Added user scripts that run on specific events
- Added SSHD (dropbear, configurable under Administration->Advanced->System)
- Clicking on the MAC address of an unidentified client will do a lookup in
  the OUI database (ported from DD-WRT).
- Enabled HTTPS access to web interface
- Start crond at boot time
- Optionally turn the WPS button into a radio enable/disable switch
- Optionally save traffic stats to disk (USB or JFFS partition)
- Display monthly traffic reports
- Monitor your router's temperature (under Administration -> Performance Tuning)
- Display active/tracked network connections
- Allows tweaking TCP/UDP connection tracking timeouts
- Fixed port forwarding where multiple ports are separated by a ","
- Added CIFS client support (for mounting remote SMB share on the router)
- Added layer7 iptables matching
- Added user-defined options for DHCP requests (required by some ISPs)


Installation
------------
Simply flash it like any regular update.  You should not need to reset to 
factory defaults, unless coming from a newer or a buggy version of Asus' 
original firmware.  You can revert back at any time by re-flashing an 
original Asus firmware.



Usage
-----

*JFFS*
JFFS is a writable section of the flash memory (around 12 MB) which will 
allow you to store small files (such as scripts) inside the router without 
needing to have a USB disk plugged in.  This space will survive reboot (but 
it *MIGHT NOT survive firmware flashing*, so back it up first before flashing!).  
It will also be available fairly early at boot (unlike a USB disk).

To enable this option, go to the Administration page, under the System tab.

First time you enable JFFS, it must be formatted.  This can be done through 
the web page, same page where you enable it.  Enabling/Disabling/Formating 
JFFS requires a reboot to take effect.


*User scripts:*
These are shell scripts that you can create, and which will be run when 
certain events occur:

- Services are started (boot): /jffs/scripts/services-start
- Services are stopped (reboot): /jffs/scripts/services-stop
- WAN interface comes up (includes if it went down and 
  back up): /jffs/scripts/wan-start
- Firewall is started (rules are applied): /jffs/scripts/firewall-start.
- Right after jffs is mounted, before any of the services get started:
  /jffs/scripts/init-start
- Just before a partition is mounted: /jffs/scripts/pre-mount (Be careful with 
  this script. This is run in a blocking call and will block the mounting of the 
  partition  for which it is invoked till its execution is complete. This is done 
  so that it can be used for things like running e2fsck on the partition before 
  mounting. This script is also passed the device path being mounted as an 
  argument which can be used in the script using $1)
- Just after a partition is mounted: /jffs/scripts/post-mount

Those scripts must all be located under /jffs/scripts/ (so JFFS support 
must be enabled first).  Don't forget to set them as executable:

   chmod a+rx /jffs/scripts/*

The wan-start script for example is perfect for using an IPv6 tunnel 
update script (such as the script I posted on my website to use with 
HE's TunnelBroker).  You could then put your IPv6 rules inside 
firewall-start.


* WakeOnLan *
There's a new Tools section on the web interface.  From there you can enter a 
target computer's MAC address to send it a WakeOnLan packet.  There is also a list 
of recently seen clients on that page - you can click on an entry to automatically 
enter it as your target.


* SSHD *
SSH support (through Dropbear) was re-enabled.  Password-based login will use 
the "admin" username (like telnet), and the same password as used to log on  
the web interface.  You can also optionally insert a RSA public key there 
for keypair-based authentication.  Note that to ensure you do not accidentally 
enter too much data there (and potentially filling the already overcrowded 
NVRAM space - what the hell was Asus thinking when they went with 32KB?), 
I am limiting this field to 512 characters max.


* HTTPS management *
I re-enabled HTTPS access in the firmware.  From the Administration->System 
page you can configure your router so it accepts connections on http, https 
or both.  You can also change the https port to a different one 
(default is 8443).


* WPS button mode - toggle radio *
You can configure the router so pressing the WPS button will 
toggle the radio on/off instead of starting WPS mode.
The option to enable this feature can be found on the 
Administration page, under the System tab.


* Crond *
Crond will automatically start at boot time.  You can 
put your cron batch in /var/spool/cron/crontabs/ .  The file 
must be named "admin" as this is the name of the system user.
Note that this location resides in RAM, so you would have to 
put your cron script somewhere such as in the jffs partition, 
and at boot time copy it to /var/spool/cron/crontabs/ using 
a init-start user script.


* Traffic history saving *
Under Tools -> Other Settings are options that will allow you 
to save your traffic history to disk, preserving it between 
router reboots (by default it is currently kept in RAM, 
so it will disappear when you reboot).

While possible to also save it to nvram, I have kept this 
option disabled, as nvram is currently too limited, 
and filling it up would cause people to lose all their 
settings.

You can save it to a custom location (for 
example, "/jffs/" if you have jffs enabled), or 
/mnt/sda1/ if you have a USB disk plugged in.
Save frequency is also configurable - it is recommended 
to keep that frequency lower (for example, once a day) 
if you are saving to jffs, to reduce wearing out 
your flash RAM.

Also, a new "Monthly" page has been added to the Traffic 
Monitor pages.


* Display active connections *
There is a new page under System Log called "Connections".
This page will list the currently tracked network connections.
You can enable name resolution for IPs by setting the 
webui_resolve_conn setting to "1" in nvram:

	nvram set webui_resolve_conn=1
	nvram commit

This option will be added to the WebUI at a later time.
Note that name resolution can slow down the loading of 
this page, especially if you have a lot of tracked 
connections (for instance while torrenting).



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



Notes
-----
To make it simple to determine which version you are running, I am simply 
appending another digit to determine my build version.  
For example, 3.0.0.3.108 becomes 3.0.0.3.108.1.


Source code
-----------
The source code with all my modifications can be found 
on Github, at:

https://github.com/RMerl/asuswrt-merlin


History
-------
3.0.0.3.144.11:
   - CHANGED: Increased dhcp options from 32 to 128 characters


3.0.0.3.144.10:
   - NEW: First release for the RT-N16.
   
   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://forums.smallnetbuilder.com/showthread.php?t=7047 as RMerlin)
Asus Forums (http://vip.asus.com/forum/topic.aspx?board_id=11&model=RT-N66U%20(VER.B1)&SLanguage=en-us) as RMerlin.
Website: http://www.lostrealm.ca/
Email: rmerl@lostrealm.ca

Drop me a note if you are using this firmware and are enjoying it.  If you really like it and want 
to give more than a simple "Thank you", there is also a Paypal donation button on my website.  I 
wouldn't mind being able to afford a second RT-N66U, so I can work on this firmware without 
constantly cutting my Internet access :)


--- 
Eric Sauvageau
