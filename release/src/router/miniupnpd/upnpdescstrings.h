/* $Id: upnpdescstrings.h,v 1.5 2007/02/09 10:12:52 nanard Exp $ */
/* miniupnp project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006 Thomas Bernard
 * This software is subject to the coditions detailed in
 * the LICENCE file provided within the distribution */
#ifndef __UPNPDESCSTRINGS_H__
#define __UPNPDESCSTRINGS_H__

#include "version.h"
#include "config.h"

/* strings used in the root device xml description */
#define ROOTDEV_FRIENDLYNAME		RT_BUILD_NAME
#define ROOTDEV_MANUFACTURER		"ASUSTeK Computer Inc."
#define ROOTDEV_MANUFACTURERURL		OS_URL
#define ROOTDEV_MODELNAME		RT_BUILD_NAME
#define ROOTDEV_MODELDESCRIPTION	RT_BUILD_NAME
#define ROOTDEV_MODELURL		OS_URL

#define WANDEV_FRIENDLYNAME			"WANDevice"
#define WANDEV_MANUFACTURER			"MiniUPnP"
#define WANDEV_MANUFACTURERURL			"http://miniupnp.free.fr/"
#define WANDEV_MODELNAME			"WAN Device"
#define WANDEV_MODELDESCRIPTION			"WAN Device"
#define WANDEV_MODELNUMBER			UPNP_VERSION
#define WANDEV_MODELURL				"http://miniupnp.free.fr/"
#define WANDEV_UPC				"MINIUPNPD"

#define WANCDEV_FRIENDLYNAME		"WANConnectionDevice"
#define WANCDEV_MANUFACTURER		WANDEV_MANUFACTURER
#define WANCDEV_MANUFACTURERURL		WANDEV_MANUFACTURERURL
#define WANCDEV_MODELNAME			"MiniUPnPd"
#define WANCDEV_MODELDESCRIPTION	"MiniUPnP daemon"
#define WANCDEV_MODELNUMBER			UPNP_VERSION
#define WANCDEV_MODELURL			"http://miniupnp.free.fr/"
#define WANCDEV_UPC					"MINIUPNPD"

#endif

