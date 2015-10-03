
lltdd: Link Layer Topology Discovery Daemon
-------------------------------------------
14 April 2005, Austin Donnelly

This is a reference implementation of the Ortelius topology discovery
protocol, written in C for fairly recent Unix/POSIX style operating
systems.  The aims are:
  * fully featured
  * reference implementation
  * easily ported


Building
--------

Support for different Unix flavours is achieved by selecting an
appropriate OS Layer (OSL).  Edit the Makefile to change OS_LAYER to
pick an appropriate osl-FOO.c file for your OS.  See the Porting
section below if there is no suitable OSL and you want to write your
own.

You will need GNU make to run the Makefile since it uses the
$(patsubst) extension to calculate the list of .o files to build.  If
you do not have a GNU make you can manually edit the OBJFILES
definition to explicitly list all the .o files needed.

Summary:
 1) edit Makefile to select OS_LAYER
 2)   $ make
 3) test run:
      $ su
      # ./lltdd -d eth0  # (or whatever the appropriate interface name is)
    expect output similar to:
finished mapping: going quiescent
ENTER: Quiescent
lltdd: listening on interface 00:50:04:4d:19:23
      ^C
 4) install lltdd binary somewhere appropriate and arrange for lltdd
    to be run as root when an interface is brought up.



Usage
-----

usage: lltdd [-d] [-t TRACELEVEL] INTERFACE
        Runs a link-layer topology discovery daemon on INTERFACE (eg eth0)
        -d : don't background, and log moderate tracing to stdout (debug mode)
        -t TRACELEVEL : select tracing by adding together:
                0x01 : BAND network load control calculations
                0x02 : packet dump of protocol exchange
                0x04 : Charge mechanism for protection against denial of service
                0x08 : system information TLVs (type-length-value)
                0x10 : multiple mapper arbitration ("Already" logic)

The INTERFACE name is mandatory, and will normally be supplied by
whatever script starts lltdd in response to an interface being brought
up.  This is probably the same script that might start a DHCP daemon
for the interface.  You should run a separate lltdd process for each
Ethernet interface in the system.

The -d option is for testing/debugging.  Normally lltdd will
automatically put itself in the background and detach from its
controlling terminal.  The -d option suppresses this behaviour, and
also redirects tracing away from syslog, towards stderr.  This means
you can easily see the trace output, and kill the daemon with ^C.

The -t option allows you to dial up or down the verbosity of tracing.
There are 5 subsystems you can toggle the tracing for independently:
BAND, packet IO, Charge, TLVs, and the Already logic.  For example to
turn on tracing of BAND (0x01) and TLVs (0x08) you add them together
(9) and use "-t 9".  Maximum tracing can be enabled with "-t 255".  By
default no tracing is enabled beyond startup and finish messages, but
if you specify -d without -t then you get packet tracing (a reasonable
default).


Architecture
------------

Overview:

Initially lltdd runs as root, allowing it to open a raw Ethernet
socket to receive Ortelius protocol packets (ethertype XXX) and ARP
packets.  It then drops root privileges and goes into a select() loop
listening for Ortelius packets on the socket.  This is termed the
Quiescent state.

The lltdd wakes from Quiescent on receiving a MapBegin protocol packet
from a mapper application.  The lltdd enables promiscuous mode on the
interface, and starts recording the source and destination Ethernet
addresses for Probe-type protocol packets, and ARP Response packets to
specific addresses.

When topology discovery is over the mapper will send a Reset protocol
packet, which causes lltdd to move back into the Quiescent state and
disable promiscuous mode.  Quiescent is also entered on timeout if
lltdd doesn't hear from the mapper for a while (around 60 seconds).

The Ortelius protocol is Ethernet-layer only, so will propagate only
within a single Ethernet broadcast domain, typically a single VLAN or
IP subnet.

Subsystems:

          -----------------------------
         |  state.c            	       |
          -----------------------------
	    ^  	       	 	     ^
	    |state_rx_*()      	     | state_*_timeout()
	    |  	       	       	     |
	 -------------------------   |
	| packetio.c	 	  |  |
	|   packetio_recv_handler |  |
       	 -^--^--------------------   |
	 /  /socket    	^      	     |
    TLVs/  /read &     	| IO events  | timer events
       /  / writes     	|      	     |
    -----v---  	       ----------------
   |  OSL    | 	      |	 event.c       |
    ---------  	       ----------------
      ^	               	   ^	     ^ 	       	user space
......|....................|.........|.....................
      |	       	       	   v   	     | 		kernel
  information for     Ortelius      ARP
  TLV population      socket        socket


OSL and event.c are the main points of contact with the underlying OS.
The file packetio.c handles reception of packets, filtering out
uninteresting packets (in case the kernel cannot), parsing and
validating them for correctness, retransmitting lost responses to
requests, and passing protocol message receipt indications
(state_rx_*()) to the higher-level protocol state machine in state.c.
State.c clocks its state machine according to these indications,
possibly updating its session state, and calling back down to
packetio.c to transmit a response if required.  State.c may also call
into event.c to setup a timer event (ie state_*_timeout() function) to
be called when a certain time passes.  These timeouts might cause
further state transitions, and/or responses to be produced.

The Ortelius protocol allows lltdd to supply the mapper with a small
amount of information about the system, represented as TLVs.  The
information for these is provided by the OSL, since accessing it tends
to be an OS-specific operation.


Porting
-------

The OSL subsystem is responsible for hiding the specifics of setting
up raw Ethernet sockets, discovering information about the host and
interface for inclusion as TLV properties, enabling promiscuous mode,
becoming a backgrounded daemon, recording the daemon's PID in a
pidfile, dropping root privileges.

If you are porting to a Unix-like (POSIX) OS, you should be able to
just write a new osl-FOO.c file which implements the API loosly
documented in osl.h.  It is likely that some code can be reused from
osl-linux.c.

If you are porting to a non-Unix environment then you may need to also
change event.c and the tracing support for syslog in util.c.

The file event.c is a wrapper around select(), managing a list of
functions which should be called at particular times, and socket fds
which should be watched for data becoming available to read.  The
daemon is single threaded for simplicity, and event.c is where
asynchrony between IO and timeouts is handled.  If you are merging
this code with yours where you already has a main select loop then you
will need to substantially modify event.c to call down to your select
loop API.

The osl-linux.c port uses POSIX.1e capabilities to drop privileges,
rather than changing uid to (say) nobody.  This is so lltdd can retain
CAP_NET_ADMIN needed to enable or disable promiscuous mode.  Ports to
systems without capabilities will need to assess how best to drop
privileges while still retaining the ability to enable/disable
promiscuous mode and ARP listening, and query interface parameters.

Almost all memory allocations are done in main.c when starting up; the
only dynamic allocations are by event.c as it manipulates the linked
list of pending timeout events.  All allocations (except a strdup() in
main.c) go through xmalloc() and xfree() wrappers in util.c should you
need to customise or instrument the memory allocation.  Nothing larger
than 160 bytes is allocated on the stack in one go (although obviously
the maximum possible stack size will be much larger than this).

The data section is empty, other than symbols supplied by the runtime.
This means the implementation is amenable to being run from read-only
storage, eg directly from ROM.

Floating point is only used in random_uniform() in util.c: you may be
able to remove this dependency on floating point (eg for porting to
kernel or other minimalist environments) with suitable knowledge of
your platform's rand() implementation.


Typical TLVs provided by lltdd:

hostid: this is a globally unique 6-byte identifier (the MAC address)
    used to identify the physical device.  On a device with multiple
    MAC addresses, one is chosen as the "hostid".

net_flags: identifies various characteristics of the physical device
    or the network interface currently used by lltdd.

physical_medium: identifies the physical medium of a network interface
    using values published by IANA for the ifType object that is defined
    in MIB-II's ifTable.  Unless if your device is an end-host with a
    wireless NIC, this value is always expected to be 6.

ipv4addr: the IPv4 address of the network interface that lltdd is using.

ipv6addr: the IPv6 address of the network interface that lltdd is using.
    Be warned that if you report an IPv6 address, you are expected to
    support IPv6 on all network services you expose on the interface.
    For example, your web server may receive subsequent HTTP requests
    over IPv6.

link_speed: the network interface's nominal data rate, expressed in
    100bit/sec units.  This is an optional TLV.

tsc_ticks_per_sec: identifies how fast the timestamp counters run in
    ticks per second.  This is an optional TLV, but is highly recommended
    if your device supports the LLTD QoS-extensions.

icon_image: this is an icon image expressed in the Microsoft .ICO format
    exactly as represented in a disk file.  The maximum size of this icon
    is 32768 bytes.  This is an optional TLV, unless if a jumbo_icon TLV
    is specified.

jumbo_icon: this is a larger version of icon_image that will only be
    requested if the network is small enough.  Even if you do supply an
    image for this TLV, you MUST still have another one that conforms to
    the restrictions of the icon_image TLV.  In essence, you may have
    up to two icon images per device.  This is an optional TLV.

qos_flags: identifies various characteristics of the physical device or the
    network interface as related to QoS functionality.  This is an optional
    TLV, but is highly recommended if your device supports the LLTD QoS-
    extensions.


TLVs required on end-hosts with wireless interface:

bssid: this is the 6-byte hardware (MAC) address of the wireless access
    point the interface is currently associated with.

ssid: this TLV always complements the existence of the bssid TLV.

wireless_mode: OID_802_11_INFRASTRUCTURE_MODE as documented in the most
    recent Windows DDK.  This TLV is optional.

max_op_rate: identifies the maximum data rate at which the radio can run
    on its 802.11 interface.  The data rate is encoded in units of 0.5Mbps.

wl_physical_medium: identifies the 802.11 physical medium in use.


TLVs required on intermediate or multifunction devices, e.g. APs and bridges:

acesspt_assns: allows an access point to report the wireless hosts that are
    associated with it.

component_tbl: identifies the logical components in your multifunction device.


You should have received a protocol specification document, and an
annotated state-transition diagram together with this code.  You will
probably need to refer to those additional documents while reading
this code to fully understand it.

Manifest:

band.c      - BAND (Block-Adjust Neighbour Discovery): network load limiter.
bandfuncs.h - prototypes for BAND.
band.h      - BAND data structure.
event.c     - central select() loop unifying IO events with timed events.
event.h     - prototypes for events.
main.c      - main() and command line option parsing.
Makefile    - build rules.
osl.h       - portable OS Layer (OSL) API.
osl-linux.c - Linux implementation of OSL API.
packetio.c  - packet receive handler, validation, and packet formatting & transmission.
packetio.h  - prototypes.
protocol.h  - Ortelius frame formats and other protocol specifics.
qospktio.c  - QoS-extensions.
README      - this file.
seeslist.c  - fixed size circular queue holding src/dst of observed packets
seeslist.h  - prototypes.
session.h   - per-interface protocol state.
state.c     - main protocol state machine.
state.h     - prototypes.
tlv.c       - Type-Length-Value (TLV) formatting routines.
tlvdef.h    - definition of supported TLVs.
tlv.h       - prototypes.
util.c      - misc utilities.
util.h      - prototypes.


# end of README
