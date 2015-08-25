
#ifndef UPNPC_H
#define UPNPC_H

//#include <miniwget.h>
#include <pjlib.h>
#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

PJ_BEGIN_DECL

// DEAN for upnp port mapping
typedef struct upnp_s {
    struct UPNPUrls upnpurls;
    struct IGDdatas igddata;
    pj_char_t extaddr[40];	/* external ip address of upper router. */
    pj_char_t lanaddr[64];	/* my ip address on the LAN */
    pj_bool_t inited;
} upnp_t;

const char * protofix(const char * proto);

int InitUpnp(upnp_t *upnp);


#if 1
void DisplayInfos(struct UPNPUrls * urls,
                         struct IGDdatas * data);
void GetConnectionStatus(struct UPNPUrls * urls,
                               struct IGDdatas * data);
void ListRedirections(struct UPNPUrls * urls,
                             struct IGDdatas * data);
void NewListRedirections(struct UPNPUrls * urls,
                                struct IGDdatas * data);
#endif

/* Test function 
 * 1 - get connection type
 * 2 - get extenal ip address
 * 3 - Add port mapping
 * 4 - get this port mapping from the IGD */
int SetRedirectAndTest(struct UPNPUrls * urls,
                               struct IGDdatas * data,
							   const char * iaddr,
							   const char * iport,
							   const char * eport,
                               const char * proto,
                               const char * leaseDuration);
int
RemoveRedirect(struct UPNPUrls * urls,
               struct IGDdatas * data,
			   const char * eport,
			   const char * proto);

#if 0
void GetFirewallStatus(struct UPNPUrls * urls, struct IGDdatas * data);
/* Test function 
 * 1 - Add pinhole
 * 2 - Check if pinhole is working from the IGD side */
void SetPinholeAndTest(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * remoteaddr, const char * eport,
					const char * intaddr, const char * iport,
					const char * proto, const char * lease_time);
/* Test function
 * 1 - Check if pinhole is working from the IGD side
 * 2 - Update pinhole */
void GetPinholeAndUpdate(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * uniqueID, const char * lease_time);
/* Test function 
 * Get pinhole timeout
 */
void GetPinholeOutboundTimeout(struct UPNPUrls * urls, struct IGDdatas * data,
					const char * remoteaddr, const char * eport,
					const char * intaddr, const char * iport,
					const char * proto);
void
GetPinholePackets(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID);
void
CheckPinhole(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID);
void
RemovePinhole(struct UPNPUrls * urls,
               struct IGDdatas * data, const char * uniqueID);
#endif

PJ_END_DECL

#endif /* UPNPC_H */
