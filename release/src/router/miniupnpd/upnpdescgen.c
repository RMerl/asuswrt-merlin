/* $Id: upnpdescgen.c,v 1.51 2009/09/06 21:26:36 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2009 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#ifdef ENABLE_EVENTS
#include "getifaddr.h"
#include "upnpredirect.h"
#endif
#include "upnpdescgen.h"
#include "miniupnpdpath.h"
#include "upnpglobalvars.h"
#include "upnpdescstrings.h"

static const char * const upnptypes[] =
{
	"string",
	"boolean",
	"ui2",
	"ui4"
};

static const char * const upnpdefaultvalues[] =
{
	0,
	"Unconfigured"
};

static const char * const upnpallowedvalues[] =
{
	0,		/* 0 */
	"DSL",	/* 1 */
	"POTS",
	"Cable",
	"Ethernet",
	0,
	"Up",	/* 6 */
	"Down",
	"Initializing",
	"Unavailable",
	0,
	"TCP",	/* 11 */
	"UDP",
	0,
	"Unconfigured",	/* 14 */
	"IP_Routed",
	"IP_Bridged",
	0,
	"Unconfigured",	/* 18 */
	"Connecting",
	"Connected",
	"PendingDisconnect",
	"Disconnecting",
	"Disconnected",
	0,
	"ERROR_NONE",	/* 25 */
	0,
	"",		/* 27 */
	0
};

static const char xmlver[] = 
	"<?xml version=\"1.0\"?>\r\n";
static const char root_service[] =
	"scpd xmlns=\"urn:schemas-upnp-org:service-1-0\"";
static const char root_device[] = 
	"root xmlns=\"urn:schemas-upnp-org:device-1-0\"";

/* root Description of the UPnP Device 
 * fixed to match UPnP_IGD_InternetGatewayDevice 1.0.pdf 
 * presentationURL is only "recommended" but the router doesn't appears
 * in "Network connections" in Windows XP if it is not present. */
static const struct XMLElt rootDesc[] =
{
/* 0 */
	{root_device, INITHELPER(1,2)},
	{"specVersion", INITHELPER(3,2)},
#if defined(ENABLE_L3F_SERVICE) || defined(HAS_DUMMY_SERVICE)
	{"device", INITHELPER(5,13)},
#else
	{"device", INITHELPER(5,12)},
#endif
	{"/major", "1"},
	{"/minor", "0"},
/* 5 */
	{"/deviceType", "urn:schemas-upnp-org:device:InternetGatewayDevice:1"},
	{"/friendlyName", ROOTDEV_FRIENDLYNAME},	/* required */
	{"/manufacturer", ROOTDEV_MANUFACTURER},		/* required */
/* 8 */
	{"/manufacturerURL", ROOTDEV_MANUFACTURERURL},	/* optional */
	{"/modelDescription", ROOTDEV_MODELDESCRIPTION}, /* recommended */
	{"/modelName", ROOTDEV_MODELNAME},	/* required */
	{"/modelNumber", modelnumber},
	{"/modelURL", ROOTDEV_MODELURL},
	{"/serialNumber", serialnumber},
	{"/UDN", uuidvalue},	/* required */
#if defined(ENABLE_L3F_SERVICE) || defined(HAS_DUMMY_SERVICE)
	{"serviceList", INITHELPER(57,1)},
	{"deviceList", INITHELPER(18,1)},
	{"/presentationURL", presentationurl},	/* recommended */
#else
	{"deviceList", INITHELPER(18,1)},
	{"/presentationURL", presentationurl},	/* recommended */
	{0,0},
#endif
/* 18 */
	{"device", INITHELPER(19,13)},
/* 19 */
	{"/deviceType", "urn:schemas-upnp-org:device:WANDevice:1"}, /* required */
	{"/friendlyName", WANDEV_FRIENDLYNAME},
	{"/manufacturer", WANDEV_MANUFACTURER},
	{"/manufacturerURL", WANDEV_MANUFACTURERURL},
	{"/modelDescription" , WANDEV_MODELDESCRIPTION},
	{"/modelName", WANDEV_MODELNAME},
	{"/modelNumber", WANDEV_MODELNUMBER},
	{"/modelURL", WANDEV_MODELURL},
	{"/serialNumber", serialnumber},
	{"/UDN", uuidvalue},
	{"/UPC", WANDEV_UPC},
/* 30 */
	{"serviceList", INITHELPER(32,1)},
	{"deviceList", INITHELPER(38,1)},
/* 32 */
	{"service", INITHELPER(33,5)},
/* 33 */
	{"/serviceType",
			"urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1"},
	/*{"/serviceId", "urn:upnp-org:serviceId:WANCommonInterfaceConfig"}, */
	{"/serviceId", "urn:upnp-org:serviceId:WANCommonIFC1"}, /* required */
	{"/controlURL", WANCFG_CONTROLURL},
	{"/eventSubURL", WANCFG_EVENTURL},
	{"/SCPDURL", WANCFG_PATH},
/* 38 */
	{"device", INITHELPER(39,12)},
/* 39 */
	{"/deviceType", "urn:schemas-upnp-org:device:WANConnectionDevice:1"},
	{"/friendlyName", WANCDEV_FRIENDLYNAME},
	{"/manufacturer", WANCDEV_MANUFACTURER},
	{"/manufacturerURL", WANCDEV_MANUFACTURERURL},
	{"/modelDescription", WANCDEV_MODELDESCRIPTION},
	{"/modelName", WANCDEV_MODELNAME},
	{"/modelNumber", WANCDEV_MODELNUMBER},
	{"/modelURL", WANCDEV_MODELURL},
	{"/serialNumber", serialnumber},
	{"/UDN", uuidvalue},
	{"/UPC", WANCDEV_UPC},
	{"serviceList", INITHELPER(51,1)},
/* 51 */
	{"service", INITHELPER(52,5)},
/* 52 */
	{"/serviceType", "urn:schemas-upnp-org:service:WANIPConnection:1"},
	/* {"/serviceId", "urn:upnp-org:serviceId:WANIPConnection"}, */
	{"/serviceId", "urn:upnp-org:serviceId:WANIPConn1"},
	{"/controlURL", WANIPC_CONTROLURL},
	{"/eventSubURL", WANIPC_EVENTURL},
	{"/SCPDURL", WANIPC_PATH},
/* 57 */
#ifdef HAS_DUMMY_SERVICE
	{"service", INITHELPER(58,5)},
/* 58 */
	{"/serviceType", "urn:schemas-dummy-com:service:Dummy:1"},
	{"/serviceId", "urn:dummy-com:serviceId:dummy1"},
	{"/controlURL", "/dummy"},
	{"/eventSubURL", "/dummy"},
	{"/SCPDURL", DUMMY_PATH},
#endif
#ifdef ENABLE_L3F_SERVICE
	{"service", INITHELPER(58,5)},
/* 58 */
	{"/serviceType", "urn:schemas-upnp-org:service:Layer3Forwarding:1"},
	{"/serviceId", "urn:upnp-org:serviceId:Layer3Forwarding1"},
	{"/controlURL", L3F_CONTROLURL}, /* The Layer3Forwarding service is only */
	{"/eventSubURL", L3F_EVENTURL}, /* recommended, not mandatory */
	{"/SCPDURL", L3F_PATH},
#endif
	{0, 0}
};

/* WANIPCn.xml */
/* see UPnP_IGD_WANIPConnection 1.0.pdf
static struct XMLElt scpdWANIPCn[] =
{
	{root_service, {INITHELPER(1,2)}},
	{0, {0}}
};
*/
static const struct argument AddPortMappingArgs[] =
{
	{1, 11},
	{1, 12},
	{1, 14},
	{1, 13},
	{1, 15},
	{1, 9},
	{1, 16},
	{1, 10},
	{0, 0}
};

static const struct argument GetExternalIPAddressArgs[] =
{
	{2, 7},
	{0, 0}
};

static const struct argument DeletePortMappingArgs[] = 
{
	{1, 11},
	{1, 12},
	{1, 14},
	{0, 0}
};

static const struct argument SetConnectionTypeArgs[] =
{
	{1, 0},
	{0, 0}
};

static const struct argument GetConnectionTypeInfoArgs[] =
{
	{2, 0},
	{2, 1},
	{0, 0}
};

static const struct argument GetStatusInfoArgs[] =
{
	{2, 2},
	{2, 4},
	{2, 3},
	{0, 0}
};

static const struct argument GetNATRSIPStatusArgs[] =
{
	{2, 5},
	{2, 6},
	{0, 0}
};

static const struct argument GetGenericPortMappingEntryArgs[] =
{
	{1, 8},
	{2, 11},
	{2, 12},
	{2, 14},
	{2, 13},
	{2, 15},
	{2, 9},
	{2, 16},
	{2, 10},
	{0, 0}
};

static const struct argument GetSpecificPortMappingEntryArgs[] =
{
	{1, 11},
	{1, 12},
	{1, 14},
	{2, 13},
	{2, 15},
	{2, 9},
	{2, 16},
	{2, 10},
	{0, 0}
};

static const struct action WANIPCnActions[] =
{
	{"AddPortMapping", AddPortMappingArgs}, /* R */
	{"GetExternalIPAddress", GetExternalIPAddressArgs}, /* R */
	{"DeletePortMapping", DeletePortMappingArgs}, /* R */
	{"SetConnectionType", SetConnectionTypeArgs}, /* R */
	{"GetConnectionTypeInfo", GetConnectionTypeInfoArgs}, /* R */
	{"RequestConnection", 0}, /* R */
	{"ForceTermination", 0}, /* R */
	{"GetStatusInfo", GetStatusInfoArgs}, /* R */
	{"GetNATRSIPStatus", GetNATRSIPStatusArgs}, /* R */
	{"GetGenericPortMappingEntry", GetGenericPortMappingEntryArgs}, /* R */
	{"GetSpecificPortMappingEntry", GetSpecificPortMappingEntryArgs}, /* R */
	{0, 0}
};
/* R=Required, O=Optional */

static const struct stateVar WANIPCnVars[] =
{
/* 0 */
	{"ConnectionType", 0, 0/*1*/}, /* required */
	{"PossibleConnectionTypes", 0|0x80, 0, 14, 15},
	 /* Required
	  * Allowed values : Unconfigured / IP_Routed / IP_Bridged */
	{"ConnectionStatus", 0|0x80, 0/*1*/, 18, 20}, /* required */
	 /* Allowed Values : Unconfigured / Connecting(opt) / Connected
	  *                  PendingDisconnect(opt) / Disconnecting (opt)
	  *                  Disconnected */
	{"Uptime", 3, 0},	/* Required */
	{"LastConnectionError", 0, 0, 25},	/* required : */
	 /* Allowed Values : ERROR_NONE(req) / ERROR_COMMAND_ABORTED(opt)
	  *                  ERROR_NOT_ENABLED_FOR_INTERNET(opt)
	  *                  ERROR_USER_DISCONNECT(opt)
	  *                  ERROR_ISP_DISCONNECT(opt)
	  *                  ERROR_IDLE_DISCONNECT(opt)
	  *                  ERROR_FORCED_DISCONNECT(opt)
	  *                  ERROR_NO_CARRIER(opt)
	  *                  ERROR_IP_CONFIGURATION(opt)
	  *                  ERROR_UNKNOWN(opt) */
	{"RSIPAvailable", 1, 0}, /* required */
	{"NATEnabled", 1, 0},    /* required */
	{"ExternalIPAddress", 0|0x80, 0, 0, 254}, /* required. Default : empty string */
	{"PortMappingNumberOfEntries", 2|0x80, 0, 0, 253}, /* required >= 0 */
	{"PortMappingEnabled", 1, 0}, /* Required */
	{"PortMappingLeaseDuration", 3, 0}, /* required */
	{"RemoteHost", 0, 0},   /* required. Default : empty string */
	{"ExternalPort", 2, 0}, /* required */
	{"InternalPort", 2, 0}, /* required */
	{"PortMappingProtocol", 0, 0, 11}, /* required allowedValues: TCP/UDP */
	{"InternalClient", 0, 0}, /* required */
	{"PortMappingDescription", 0, 0}, /* required default: empty string */
	{0, 0}
};

static const struct serviceDesc scpdWANIPCn =
{ WANIPCnActions, WANIPCnVars };

/* WANCfg.xml */
/* See UPnP_IGD_WANCommonInterfaceConfig 1.0.pdf */

static const struct argument GetCommonLinkPropertiesArgs[] =
{
	{2, 0},
	{2, 1},
	{2, 2},
	{2, 3},
	{0, 0}
};

static const struct argument GetTotalBytesSentArgs[] =
{
	{2, 4},
	{0, 0}
};

static const struct argument GetTotalBytesReceivedArgs[] =
{
	{2, 5},
	{0, 0}
};

static const struct argument GetTotalPacketsSentArgs[] =
{
	{2, 6},
	{0, 0}
};

static const struct argument GetTotalPacketsReceivedArgs[] =
{
	{2, 7},
	{0, 0}
};

static const struct action WANCfgActions[] =
{
	{"GetCommonLinkProperties", GetCommonLinkPropertiesArgs}, /* Required */
	{"GetTotalBytesSent", GetTotalBytesSentArgs},             /* optional */
	{"GetTotalBytesReceived", GetTotalBytesReceivedArgs},     /* optional */
	{"GetTotalPacketsSent", GetTotalPacketsSentArgs},         /* optional */
	{"GetTotalPacketsReceived", GetTotalPacketsReceivedArgs}, /* optional */
	{0, 0}
};

/* See UPnP_IGD_WANCommonInterfaceConfig 1.0.pdf */
static const struct stateVar WANCfgVars[] =
{
	{"WANAccessType", 0, 0, 1},
	/* Allowed Values : DSL / POTS / Cable / Ethernet 
	 * Default value : empty string */
	{"Layer1UpstreamMaxBitRate", 3, 0},
	{"Layer1DownstreamMaxBitRate", 3, 0},
	{"PhysicalLinkStatus", 0|0x80, 0, 6, 6},
	/*  allowed values : 
	 *      Up / Down / Initializing (optional) / Unavailable (optionnal)
	 *  no Default value 
	 *  Evented */
	{"TotalBytesSent", 3, 0},	   /* Optional */
	{"TotalBytesReceived", 3, 0},  /* Optional */
	{"TotalPacketsSent", 3, 0},    /* Optional */
	{"TotalPacketsReceived", 3, 0},/* Optional */
	/*{"MaximumActiveConnections", 2, 0},	// allowed Range value // OPTIONAL */
	{0, 0}
};

static const struct serviceDesc scpdWANCfg =
{ WANCfgActions, WANCfgVars };

#ifdef ENABLE_L3F_SERVICE
/* Read UPnP_IGD_Layer3Forwarding_1.0.pdf */
static const struct argument SetDefaultConnectionServiceArgs[] =
{
	{1, 0}, /* in */
	{0, 0}
};

static const struct argument GetDefaultConnectionServiceArgs[] =
{
	{2, 0}, /* out */
	{0, 0}
};

static const struct action L3FActions[] =
{
	{"SetDefaultConnectionService", SetDefaultConnectionServiceArgs}, /* Req */
	{"GetDefaultConnectionService", GetDefaultConnectionServiceArgs}, /* Req */
	{0, 0}
};

static const struct stateVar L3FVars[] =
{
	{"DefaultConnectionService", 0|0x80, 0, 0, 255}, /* Required */
	{0, 0}
};

static const struct serviceDesc scpdL3F =
{ L3FActions, L3FVars };
#endif

/* strcat_str()
 * concatenate the string and use realloc to increase the
 * memory buffer if needed. */
static char *
strcat_str(char * str, int * len, int * tmplen, const char * s2)
{
	int s2len;
	s2len = (int)strlen(s2);
	if(*tmplen <= (*len + s2len))
	{
		if(s2len < 256)
			*tmplen += 256;
		else
			*tmplen += s2len + 1;
		str = (char *)realloc(str, *tmplen);
	}
	/*strcpy(str + *len, s2); */
	memcpy(str + *len, s2, s2len + 1);
	*len += s2len;
	return str;
}

/* strcat_char() :
 * concatenate a character and use realloc to increase the
 * size of the memory buffer if needed */
static char *
strcat_char(char * str, int * len, int * tmplen, char c)
{
	if(*tmplen <= (*len + 1))
	{
		*tmplen += 256;
		str = (char *)realloc(str, *tmplen);
	}
	str[*len] = c;
	(*len)++;
	return str;
}

/* iterative subroutine using a small stack
 * This way, the progam stack usage is kept low */
static char *
genXML(char * str, int * len, int * tmplen,
                   const struct XMLElt * p)
{
	unsigned short i, j;
	unsigned long k;
	int top;
	const char * eltname, *s;
	char c;
	struct {
		unsigned short i;
		unsigned short j;
		const char * eltname;
	} pile[16]; /* stack */
	top = -1;
	i = 0;	/* current node */
	j = 1;	/* i + number of nodes*/
	for(;;)
	{
		eltname = p[i].eltname;
		if(!eltname)
			return str;
		if(eltname[0] == '/')
		{
			if(p[i].data && p[i].data[0])
			{
				/*printf("<%s>%s<%s>\n", eltname+1, p[i].data, eltname); */
				str = strcat_char(str, len, tmplen, '<');
				str = strcat_str(str, len, tmplen, eltname+1);
				str = strcat_char(str, len, tmplen, '>');
				str = strcat_str(str, len, tmplen, p[i].data);
				str = strcat_char(str, len, tmplen, '<');
				str = strcat_str(str, len, tmplen, eltname);
				str = strcat_char(str, len, tmplen, '>');
			}
			for(;;)
			{
				if(top < 0)
					return str;
				i = ++(pile[top].i);
				j = pile[top].j;
				/*printf("  pile[%d]\t%d %d\n", top, i, j); */
				if(i==j)
				{
					/*printf("</%s>\n", pile[top].eltname); */
					str = strcat_char(str, len, tmplen, '<');
					str = strcat_char(str, len, tmplen, '/');
					s = pile[top].eltname;
					for(c = *s; c > ' '; c = *(++s))
						str = strcat_char(str, len, tmplen, c);
					str = strcat_char(str, len, tmplen, '>');
					top--;
				}
				else
					break;
			}
		}
		else
		{
			/*printf("<%s>\n", eltname); */
			str = strcat_char(str, len, tmplen, '<');
			str = strcat_str(str, len, tmplen, eltname);
			str = strcat_char(str, len, tmplen, '>');
			k = (unsigned long)p[i].data;
			i = k & 0xffff;
			j = i + (k >> 16);
			top++;
			/*printf(" +pile[%d]\t%d %d\n", top, i, j); */
			pile[top].i = i;
			pile[top].j = j;
			pile[top].eltname = eltname;
		}
	}
}

/* genRootDesc() :
 * - Generate the root description of the UPnP device.
 * - the len argument is used to return the length of
 *   the returned string. 
 * - tmp_uuid argument is used to build the uuid string */
char *
genRootDesc(int * len)
{
	char * str;
	int tmplen;
	tmplen = 2048;
	str = (char *)malloc(tmplen);
	if(str == NULL)
		return NULL;
	* len = strlen(xmlver);
	/*strcpy(str, xmlver); */
	memcpy(str, xmlver, *len + 1);
	str = genXML(str, len, &tmplen, rootDesc);
	str[*len] = '\0';
	return str;
}

/* genServiceDesc() :
 * Generate service description with allowed methods and 
 * related variables. */
static char *
genServiceDesc(int * len, const struct serviceDesc * s)
{
	int i, j;
	const struct action * acts;
	const struct stateVar * vars;
	const struct argument * args;
	const char * p;
	char * str;
	int tmplen;
	tmplen = 2048;
	str = (char *)malloc(tmplen);
	if(str == NULL)
		return NULL;
	/*strcpy(str, xmlver); */
	*len = strlen(xmlver);
	memcpy(str, xmlver, *len + 1);
	
	acts = s->actionList;
	vars = s->serviceStateTable;

	str = strcat_char(str, len, &tmplen, '<');
	str = strcat_str(str, len, &tmplen, root_service);
	str = strcat_char(str, len, &tmplen, '>');

	str = strcat_str(str, len, &tmplen,
		"<specVersion><major>1</major><minor>0</minor></specVersion>");

	i = 0;
	str = strcat_str(str, len, &tmplen, "<actionList>");
	while(acts[i].name)
	{
		str = strcat_str(str, len, &tmplen, "<action><name>");
		str = strcat_str(str, len, &tmplen, acts[i].name);
		str = strcat_str(str, len, &tmplen, "</name>");
		/* argument List */
		args = acts[i].args;
		if(args)
		{
			str = strcat_str(str, len, &tmplen, "<argumentList>");
			j = 0;
			while(args[j].dir)
			{
				str = strcat_str(str, len, &tmplen, "<argument><name>New");
				p = vars[args[j].relatedVar].name;
				if(0 == memcmp(p, "PortMapping", 11)
				   && 0 != memcmp(p + 11, "Description", 11)) {
					if(0 == memcmp(p + 11, "NumberOfEntries", 15))
						str = strcat_str(str, len, &tmplen, "PortMappingIndex");
					else
						str = strcat_str(str, len, &tmplen, p + 11);
				} else {
					str = strcat_str(str, len, &tmplen, p);
				}
				str = strcat_str(str, len, &tmplen, "</name><direction>");
				str = strcat_str(str, len, &tmplen, args[j].dir==1?"in":"out");
				str = strcat_str(str, len, &tmplen,
						"</direction><relatedStateVariable>");
				str = strcat_str(str, len, &tmplen, p);
				str = strcat_str(str, len, &tmplen,
						"</relatedStateVariable></argument>");
				j++;
			}
			str = strcat_str(str, len, &tmplen,"</argumentList>");
		}
		str = strcat_str(str, len, &tmplen, "</action>");
		/*str = strcat_char(str, len, &tmplen, '\n'); // TEMP ! */
		i++;
	}
	str = strcat_str(str, len, &tmplen, "</actionList><serviceStateTable>");
	i = 0;
	while(vars[i].name)
	{
		str = strcat_str(str, len, &tmplen,
				"<stateVariable sendEvents=\"");
#ifdef ENABLE_EVENTS
		str = strcat_str(str, len, &tmplen, (vars[i].itype & 0x80)?"yes":"no");
#else
		/* for the moment allways send no. Wait for SUBSCRIBE implementation
		 * before setting it to yes */
		str = strcat_str(str, len, &tmplen, "no");
#endif
		str = strcat_str(str, len, &tmplen, "\"><name>");
		str = strcat_str(str, len, &tmplen, vars[i].name);
		str = strcat_str(str, len, &tmplen, "</name><dataType>");
		str = strcat_str(str, len, &tmplen, upnptypes[vars[i].itype & 0x0f]);
		str = strcat_str(str, len, &tmplen, "</dataType>");
		if(vars[i].iallowedlist)
		{
		  str = strcat_str(str, len, &tmplen, "<allowedValueList>");
		  for(j=vars[i].iallowedlist; upnpallowedvalues[j]; j++)
		  {
		    str = strcat_str(str, len, &tmplen, "<allowedValue>");
		    str = strcat_str(str, len, &tmplen, upnpallowedvalues[j]);
		    str = strcat_str(str, len, &tmplen, "</allowedValue>");
		  }
		  str = strcat_str(str, len, &tmplen, "</allowedValueList>");
		}
		/*if(vars[i].defaultValue) */
		if(vars[i].idefault)
		{
		  str = strcat_str(str, len, &tmplen, "<defaultValue>");
		  /*str = strcat_str(str, len, &tmplen, vars[i].defaultValue); */
		  str = strcat_str(str, len, &tmplen, upnpdefaultvalues[vars[i].idefault]);
		  str = strcat_str(str, len, &tmplen, "</defaultValue>");
		}
		str = strcat_str(str, len, &tmplen, "</stateVariable>");
		/*str = strcat_char(str, len, &tmplen, '\n'); // TEMP ! */
		i++;
	}
	str = strcat_str(str, len, &tmplen, "</serviceStateTable></scpd>");
	str[*len] = '\0';
	return str;
}

/* genWANIPCn() :
 * Generate the WANIPConnection xml description */
char *
genWANIPCn(int * len)
{
	return genServiceDesc(len, &scpdWANIPCn);
}

/* genWANCfg() :
 * Generate the WANInterfaceConfig xml description. */
char *
genWANCfg(int * len)
{
	return genServiceDesc(len, &scpdWANCfg);
}

#ifdef ENABLE_L3F_SERVICE
char *
genL3F(int * len)
{
	return genServiceDesc(len, &scpdL3F);
}
#endif

#ifdef ENABLE_EVENTS
static char *
genEventVars(int * len, const struct serviceDesc * s, const char * servns)
{
	char tmp[16];
	const struct stateVar * v;
	char * str;
	int tmplen;
	tmplen = 512;
	str = (char *)malloc(tmplen);
	if(str == NULL)
		return NULL;
	*len = 0;
	v = s->serviceStateTable;
	str = strcat_str(str, len, &tmplen, "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\" xmlns:s=\"");
	str = strcat_str(str, len, &tmplen, servns);
	str = strcat_str(str, len, &tmplen, "\">");
	while(v->name) {
		if(v->itype & 0x80) {
			str = strcat_str(str, len, &tmplen, "<e:property><s:");
			str = strcat_str(str, len, &tmplen, v->name);
			str = strcat_str(str, len, &tmplen, ">");
			//printf("<e:property><s:%s>", v->name);
			switch(v->ieventvalue) {
			case 0:
				break;
			case 253:	/* Port mapping number of entries magical value */
				snprintf(tmp, sizeof(tmp), "%d", upnp_get_portmapping_number_of_entries());
				str = strcat_str(str, len, &tmplen, tmp);
				break;
			case 254:	/* External ip address magical value */
				if(use_ext_ip_addr)
					str = strcat_str(str, len, &tmplen, use_ext_ip_addr);
				else {
					char ext_ip_addr[INET_ADDRSTRLEN];
					if(getifaddr(ext_if_name, ext_ip_addr, INET_ADDRSTRLEN) < 0) {
						str = strcat_str(str, len, &tmplen, "0.0.0.0");
					} else {
						str = strcat_str(str, len, &tmplen, ext_ip_addr);
					}
				}
				/*str = strcat_str(str, len, &tmplen, "0.0.0.0");*/
				break;
			case 255:	/* DefaultConnectionService magical value */
				str = strcat_str(str, len, &tmplen, uuidvalue);
				str = strcat_str(str, len, &tmplen, ":WANConnectionDevice:1,urn:upnp-org:serviceId:WANIPConn1");
				//printf("%s:WANConnectionDevice:1,urn:upnp-org:serviceId:WANIPConn1", uuidvalue);
				break;
			default:
				str = strcat_str(str, len, &tmplen, upnpallowedvalues[v->ieventvalue]);
				//printf("%s", upnpallowedvalues[v->ieventvalue]);
			}
			str = strcat_str(str, len, &tmplen, "</s:");
			str = strcat_str(str, len, &tmplen, v->name);
			str = strcat_str(str, len, &tmplen, "></e:property>");
			//printf("</s:%s></e:property>\n", v->name);
		}
		v++;
	}
	str = strcat_str(str, len, &tmplen, "</e:propertyset>");
	//printf("</e:propertyset>\n");
	//printf("\n");
	//printf("%d\n", tmplen);
	str[*len] = '\0';
	return str;
}

char *
getVarsWANIPCn(int * l)
{
	return genEventVars(l,
                        &scpdWANIPCn,
	                    "urn:schemas-upnp-org:service:WANIPConnection:1");
}

char *
getVarsWANCfg(int * l)
{
	return genEventVars(l,
	                    &scpdWANCfg,
	                    "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1");
}

#ifdef ENABLE_L3F_SERVICE
char *
getVarsL3F(int * l)
{
	return genEventVars(l,
	                    &scpdL3F,
	                    "urn:schemas-upnp-org:service:Layer3Forwarding:1");
}
#endif
#endif
