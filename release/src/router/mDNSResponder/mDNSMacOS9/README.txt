This directory contains support files for running mDNS on Mac OS 9
(and Carbon).

mDNS.mcp is a CodeWarrior 8 project file.

mDNSMacOS9.c and mDNSMacOS9.h are the Platform Support files that go below
mDNS Core.

"Mac OS Test Responder.c" and "Mac OS Test Searcher.c" build an example
standalone (embedded) mDNS Responder and Searcher, respectively.

"mDNSLibrary.c" builds a CFM Shared Library that goes in the Extensions Folder

The CFM Shared Library inplements the same "/usr/include/dns_sd.h" API
that exists on OS X, Windows, Linux, etc., one exception:

 - You don't need to call DNSServiceRefSockFD() to get a file descriptor,
   and add that file descriptor to your select loop (or equivalent),
   wait for data,
   and then call DNSServiceProcessResult() every time data arrives.

   On OS 9, your callback functions are called "by magic" without having
   to do any of this. Of course no magic comes without a price, and
   the magic being used here is an OT Notifier function. Your callback
   functions are called directly from the OT Notifier function that
   received the UDP packet from the wire, which is fast and efficient,
   but it does mean that you're being called at OT Notifier time -- so
   no QuickDraw calls. If you need to allocate memory in your callback
   function, use OTAllocMem(), not NewPtr() or NewHandle(). Typically
   what you'll do in your callback function is just update your program's
   data structures, and leave screen updating and UI to some foreground
   code.

"Searcher.c" and "Responder.c" build sample applications that link with
this CFM Shared Library in the Extensions Folder. You'll see that if
you try to run them without first putting the "Multicast DNS & DNS-SD"
library in the Extensions Folder and restarting, they will report an
error message.

"Searcher.c" builds a sample application that browses for HTTP servers.

"Responder.c" builds a sample application that advertises some test
services. By default it advertises a couple of (nonexistent) HTTP servers
called "Web Server One" and "Web Server Two". In addition, if it finds that
TCP port 548 is occupied, then it concludes that personal file sharing is
running, and advertises the existence of an AFP server. Similarly, if it
finds that TCP port 80 is occupied, then it concludes that personal Web
sharing is running, advertises the existence of an HTTP server.
