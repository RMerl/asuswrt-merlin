This directory contains support files for running mDNS on Microsoft Windows 
and Windows CE/PocketPC. Building this code requires the Windows SDK 2003
or later. The CodeWarrior builds require CodeWarrior 8 or later and if using
CodeWarrior 8, the newer Windows headers from the Mac CodeWarrior 9 need to
be used.

mDNSWin32.c/.h
	
	Platform Support files that go below mDNS Core. These work on both Windows 
	and Windows CE/PocketPC.

DNSSD.c/.h

	High-level implementation of the DNS-SD API. This supports both "direct"
	(compiled-in mDNSCore) and "client" (IPC to service) usage. Conditionals
	can exclude either "direct" or "client" to reduce code size.

DNSSDDirect.c/.h

	Portable implementation of the DNS-SD API. This interacts with mDNSCore
	to perform all the real work of the DNS-SD API. This code does not rely
	on any platform-specifics so it should run on any platform with an mDNS
	platform plugin available. Software that cannot or does not want to use
	the IPC mechanism (e.g. Windows CE, VxWorks, etc.) can use this code 
	directly without any of the IPC pieces.

RMxClient.c/.h
	
	Client-side implementation of the DNS-SD IPC API. This handles sending 
	and receiving messages from the service to perform DNS-SD operations 
	and get DNS-SD responses.

RMxCommon.c/.h

	Common code between the RMxClient and RMxServer. This handles establishing
	and accepting connections, the underying message sending and receiving, 
	portable data packing and unpacking, and shared utility routines.

RMxServer.c/.h

	Server-side implementation of the DNS-SD IPC API. This listens for 
	and accepts connections from IPC clients, starts server sessions, and 
	acts as a mediator between the "direct" (compiled-in mDNSCore) code
	and the IPC client.

DNSServices is an obsolete higher-level API for using mDNS. New code should 
use the DNS-SD APIs.

DNSServiceDiscovery is an obsolete emulation layer that sits on top of 
DNSServices and provides the Mac OS X DNS Service Discovery API's on any 
platform. New code should use the DNS-SD APIs.

Tool.c is an example client that uses the services of mDNS Core.

ToolWin32.mcp is a CodeWarrior project (CodeWarrior for Windows version 8). 
ToolWin32.vcproj is a Visual Studio .NET 7 project. These projects build 
Tool.c to make DNSServiceTest.exe, a small Windows command-line tool to do all 
the standard DNS-SD stuff on Windows. It has the following features:

- Browse for browsing and/or registration domains.
- Browse for services.
- Lookup Service Instances.
- Register domains for browsing and/or registration.
- Register services.

For example, if you have a Windows machine running a Web server,
then you can make it advertise that it is offering HTTP on port 80
with the following command:

DNSServiceTest -rs "Windows Web Server" "_http._tcp." "local." 80 ""

To search for AFP servers, use this:

DNSServiceTest -bs "_afpovertcp._tcp." "local."

You can also do multiple things at once (e.g. register a service and
browse for it so one instance of the app can be used for testing).
Multiple instances can also be run on the same machine to discover each
other. There is a -help command to show all the commands, their
parameters, and some examples of using it.

DNSServiceBrowser contains the source code for a graphical browser application 
for Windows CE/PocketPC. The Windows CE/PocketPC version requires Microsoft 
eMbedded C++ 4.0 with SP2 installed and the PocketPC 2003 SDK.

