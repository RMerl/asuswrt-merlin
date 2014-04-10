#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>

#include <DNSServiceDiscovery/DNSServiceDiscovery.h>

extern void do_print(char *host);

void handle_msg(CFMachPortRef port, void *msg, CFIndex size, void *info);

void reply_callback(
    struct sockaddr     *interface,
    struct sockaddr     *address,
    const char          *txtRecord,
    DNSServiceDiscoveryReplyFlags                               flags,
    void                                *context
) {
	struct sockaddr_in *sin = address;

	if( address->sa_family == AF_INET ) {
		do_print(inet_ntoa(sin->sin_addr));
	}
}

void browser_callback(
    DNSServiceBrowserReplyResultType                    resultType,
// One of DNSServiceBrowserReplyResultType
    const char          *replyName,
    const char          *replyType,
    const char          *replyDomain,
    DNSServiceDiscoveryReplyFlags                               flags,
        // DNS Service Discovery reply flags information
    void                        *context
) {
	mach_port_t port;
	CFMachPortRef cfprt;
	CFRunLoopSourceRef rls;
	dns_service_discovery_ref client;
	CFMachPortContext ctx = { 0, 0, NULL, NULL, NULL };
	Boolean sf;

	client = DNSServiceResolverResolve(replyName, replyType, replyDomain, reply_callback, NULL);

	port = DNSServiceDiscoveryMachPort(client);
	cfprt = CFMachPortCreateWithPort(kCFAllocatorDefault, port, handle_msg, &ctx, &sf);
	rls = CFMachPortCreateRunLoopSource(NULL, cfprt, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
}

void handle_msg(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	DNSServiceDiscovery_handleReply(msg);
}

void browse()
{
	dns_service_discovery_ref client;
	CFMachPortContext ctx = { 0, 0, NULL, NULL, NULL };
	Boolean sf;
	mach_port_t port;
	CFMachPortRef cfprt;
	CFRunLoopSourceRef rls;
	client = DNSServiceBrowserCreate("_mountd._tcp", "", browser_callback, NULL);
	port = DNSServiceDiscoveryMachPort(client);
	cfprt = CFMachPortCreateWithPort(kCFAllocatorDefault, port, handle_msg, &ctx, &sf);
	rls = CFMachPortCreateRunLoopSource(NULL, cfprt, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRunLoopRun();
	//sleep(10);
}
