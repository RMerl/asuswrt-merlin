/* -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/*
 * Author:  Daniel S. Haischt <me@daniel.stefan.haischt.name>
 * Purpose: Zeroconf facade, that abstracts access to a
 *          particular Zeroconf implementation
 * Doc:     http://www.dns-sd.org/
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "afp_zeroconf.h"
#include "afp_config.h"

#ifdef HAVE_MDNS
#include "afp_mdns.h"
#elif defined (HAVE_AVAHI)
#include "afp_avahi.h"
#endif


/*
 * Functions (actually they are just facades)
 */
void zeroconf_register(const AFPObj *configs)
{
#if defined (HAVE_MDNS)
  LOG(log_debug, logtype_afpd, "Attempting to register with mDNS using mDNSResponder");

	md_zeroconf_register(configs);
#elif defined (HAVE_AVAHI)
  LOG(log_debug, logtype_afpd, "Attempting to register with mDNS using Avahi");

	av_zeroconf_register(configs);
#endif
}

void zeroconf_deregister(void)
{
#if defined (HAVE_MDNS)
  LOG(log_debug, logtype_afpd, "Attempting to de-register mDNS using mDNSResponder");
	md_zeroconf_unregister();
#elif defined (HAVE_AVAHI)
  LOG(log_debug, logtype_afpd, "Attempting to de-register mDNS using Avahi");
	av_zeroconf_unregister();
#endif
}
