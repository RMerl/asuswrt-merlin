To build packages for Solaris 10:

Requirements:
-------------

- Development environment including gcc (eg as shipped with Solaris 10)

- The Package tools from Solaris 10 or Solaris Nevada/Express.

- i.manifest and r.manifest scripts as supplied with Solaris Express
  in /usr/sadm/install/scripts/ or from OpenSolaris.org:

  http://cvs.opensolaris.org/source/xref/usr/src/pkgdefs/common_files/i.manifest
  http://cvs.opensolaris.org/source/xref/usr/src/pkgdefs/common_files/r.manifest
  
  i.manifest must be at least version 1.5. Place these scripts in
  this directory if you are using Solaris 10 GA (which does not ship with
  these scripts), or in the solaris/ directory in the Quagga source.


Package creation instructions:
------------------------------

1. Configure and build Quagga in the top level build directory as per
normal, eg:

	./configure --prefix=/usr/local/quagga \
		--localstatedir=/var/run/quagga
		--enable-gcc-rdynamic --enable-opaque-lsa --enable-ospf-te \
		--enable-multipath=64 --enable-user=quagga \
		--enable-ospfclient=yes --enable-ospfapi=yes  \
		--enable-group=quagga --enable-nssa --enable-opaque-lsa

You will need /usr/sfw/bin and /usr/ccs/bin in your path.

2. make install in the top-level build directory, it's a good idea to make
use of DESTDIR to install to an alternate root, eg:

	gmake DESTDIR=/var/tmp/qroot install

3. In this directory (solaris/), run make packages, specifying DESTDIR if
appropriate, eg:

	gmake DESTDIR=/var/tmp/qroot packages

This should result in 4 packages being created:

	quagga-libs-...-$ARCH.pkg 	- QUAGGAlibs
	quagga-daemons-...-$ARCH.pkg	- QUAGGAdaemons
	quagga-doc-...-$ARCH.pkg	- QUAGGAdoc
	quagga-dev-...-$ARCH.pkg	- QUAGGAdev
	quagga-smf-...-$ARCH.pkg	- QUAGGAsmf

QUAGGAlibs and QUAGGAdaemons are needed for daemon runtime. QUAGGAsmf
provides the required bits for Solaris 10+ SMF support.


Install and post-install configuration notes:
---------------------------------------------

- If you specified a user/group which does not exist per default on Solaris
  (eg quagga/quagga) you *must* create these before installing these on a
  system. The packages do *not* create the users.

- The configuration files are not created. You must create the configuration
  file yourself, either with your complete desired configuration, or else if
  you wish to use the telnet interface for further configuration you must
  create them containing at least:

	 password whatever

  The user which quagga runs as must have write permissions on this file, no
  other user should have read permissions, and you would also have to enable
  the telnet interface (see below).

- SMF notes:

  - QUAGGAsmf installs a svc:/network/routing/quagga service, with an
    instance for each daemon
  
  - The state of all instances of quagga service can be inspected with:
  
  	svcs -l svc:/network/routing/quagga
  
    or typically just with a shortcut of 'quagga':
    
    	svcs -l quagga
  
  - A specific instance of the quagga service can be inspected by specifying
    the daemon name as the instance, ie quagga:<daemon>:
    
    	svcs -l svc:/network/routing/quagga:zebra
    	svcs -l svc:/network/routing/quagga:ospfd
    	<etc>

    or typically just with the shortcut of 'quagga:<daemon>' or even
    <daemon>:
    
    	svcs -l quagga:zebra
    	svcs -l ospfd
    
    Eg:
    
    # # svcs -l ripd
    fmri         svc:/network/routing/quagga:ripd
    name         Quagga: ripd, RIPv1/2 IPv4 routing protocol daemon.
    enabled      true
    state        online
    next_state   none
    state_time   Wed Jun 15 16:21:02 2005
    logfile      /var/svc/log/network-routing-quagga:ripd.log
    restarter    svc:/system/svc/restarter:default
    contract_id  93 
    dependency   require_all/restart svc:/network/routing/quagga:zebra (online)
    dependency   require_all/restart file://localhost//usr/local/quagga/etc/ripd.conf (online)
    dependency   require_all/none svc:/system/filesystem/usr:default (online)
    dependency   require_all/none svc:/network/loopback (online)

  - Configuration of startup options is by way of SMF properties in a
    property group named 'quagga'. The defaults should automatically be
    inline with how you configured Quagga in Step 1 above. 
  
  - By default the VTY interface is disabled. To change this, see below for
    how to set the 'quagga/vty_port' property as appropriate for
    /each/ service. Also, the VTY is set to listen only to localhost by
    default, you may change the 'quagga/vty_addr' property as appropriate
    for both of the 'quagga' service and specific individual instances of
    the 'quagga' service (ie quagga:zebra, quagga:ospfd, etc..).
    
  - Properties belonging to the 'quagga' service are inherited by all
    instances. Eg:
    
    # svcprop -p quagga svc:/network/routing/quagga
    quagga/group astring root
    quagga/retain boolean false
    quagga/user astring root
    quagga/vty_addr astring 127.1
    quagga/vty_port integer 0
    
    # svcprop -p quagga svc:/network/routing/quagga:ospfd
    quagga/retain_routes boolean false
    quagga/group astring root
    quagga/retain boolean false
    quagga/user astring root
    quagga/vty_addr astring 127.1
    quagga/vty_port integer 0
    
    All instances will inherit these properties, unless the instance itself
    overrides these defaults. This also implies one can modify properties of
    the 'quagga' service and have them apply to all daemons.
    
    # svccfg -s svc:/network/routing/quagga \
    	setprop quagga/vty_addr = astring: ::1
    
    # svcprop -p quagga svc:/network/routing/quagga
    quagga/group astring root
    quagga/retain boolean false
    quagga/user astring root
    quagga/vty_port integer 0
    quagga/vty_addr astring ::1
    
    # # You *must* refresh instances to have the property change
    # # take affect for the 'running snapshot' of service state.
    # svcadm refresh quagga:ospfd
    
    # svcprop -p quagga svc:/network/routing/quagga:ospfd
    quagga/retain_routes boolean false
    quagga/group astring root
    quagga/retain boolean false
    quagga/user astring root
    quagga/vty_port integer 0
    quagga/vty_addr astring ::1
    
    Other daemon-specific options/properties may be available, however they
    are not yet honoured/used (eg ospfd/apiserver on svc:/network/ospf).

  - As SMF is dependency aware, restarting network/zebra will restart all the
    other daemons.
  
  - To upgrade from one set of Quagga packages to a newer release, one must
    first pkgrm the installed packages. When one pkgrm's QUAGGAsmf all
    property configuration will be lost, and any customisations will have to
    redone after installing the updated QUAGGAsmf package.
  
- These packages are not supported by Sun Microsystems, report bugs via the
  usual Quagga channels, ie Bugzilla. Improvements/contributions of course
  would be greatly appreciated.

