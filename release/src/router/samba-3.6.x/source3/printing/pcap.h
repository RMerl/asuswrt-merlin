/*
   Unix SMB/CIFS implementation.
   printcap headers

   Copyright (C) Karl Auer 1993-1998

   Re-working by Martin Kiff, 1994

   Re-written again by Andrew Tridgell

   Modified for SVID support by Norm Jacobs, 1997

   Modified for CUPS support by Michael Sweet, 1999

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PRINTING_PCAP_H_
#define _PRINTING_PCAP_H_

struct pcap_cache;

/* The following definitions come from printing/pcap.c  */

bool pcap_cache_add_specific(struct pcap_cache **ppcache, const char *name, const char *comment, const char *location);
void pcap_cache_destroy_specific(struct pcap_cache **ppcache);
bool pcap_cache_add(const char *name, const char *comment, const char *location);
bool pcap_cache_loaded(void);
bool pcap_cache_replace(const struct pcap_cache *cache);
void pcap_printer_fn_specific(const struct pcap_cache *, void (*fn)(const char *, const char *, const char *, void *), void *);
void pcap_printer_fn(void (*fn)(const char *, const char *, const char *, void *), void *);

void pcap_cache_reload(struct tevent_context *ev,
		       struct messaging_context *msg_ctx,
		       void (*post_cache_fill_fn)(struct tevent_context *,
						  struct messaging_context *));
bool pcap_printername_ok(const char *printername);

/* The following definitions come from printing/print_aix.c  */

bool aix_cache_reload(void);

/* The following definitions come from printing/print_cups.c  */

bool cups_cache_reload(struct tevent_context *ev,
		       struct messaging_context *msg_ctx,
		       void (*post_cache_fill_fn)(struct tevent_context *,
						  struct messaging_context *));

/* The following definitions come from printing/print_iprint.c  */

bool iprint_cache_reload(void);

/* The following definitions come from printing/print_svid.c  */

bool sysv_cache_reload(void);

/* The following definitions come from printing/print_standard.c  */
bool std_pcap_cache_reload(const char *pcap_name);

#endif /* _PRINTING_PCAP_H_ */
