/* $Id: upnpsoap.h,v 1.10 2012/09/27 15:46:18 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef UPNPSOAP_H_INCLUDED
#define UPNPSOAP_H_INCLUDED

/* ExecuteSoapAction():
 * this method executes the requested Soap Action */
void
ExecuteSoapAction(struct upnphttp *, const char *, int);

/* SoapError():
 * sends a correct SOAP error with an UPNPError code and
 * description */
void
SoapError(struct upnphttp * h, int errCode, const char * errDesc);

#endif

