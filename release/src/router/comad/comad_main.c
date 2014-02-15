/*
 * COMA daemon (Linux)
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: comad_main.c 399877 2013-05-02 05:23:42Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shutils.h>

#define ASCII_0 48

/* service main entry */
int
main(void)
{
	FILE *fp;
	int pwr_down_event;
	char lan_ifname[32], *lan_ifnames, *next;

	if (daemon(1, 1) == -1) {
		printf("err from daemonize.\n");
		goto cleanup;
	}

	while (1) {
		system("cat /proc/bcm947xx/coma > /tmp/coma");
		if (!(fp = fopen("/tmp/coma", "r"))) {
			printf("Open /tmp/coma failed\n");
			system("rm /tmp/coma");
			goto cleanup;
		}
		pwr_down_event = fgetc(fp) - ASCII_0;
		fclose(fp);
		system("rm /tmp/coma");
		if (pwr_down_event)
		{
			/* down WiFi adapter */
			lan_ifnames = nvram_safe_get("lan_ifnames");
			foreach(lan_ifname, lan_ifnames, next) {
				if (!strncmp(lan_ifname, "eth", 3)) {
					eval("wl", "-i", lan_ifname, "down");
				}
			}

			system("echo \"1\" > /proc/bcm947xx/coma");
		}
		sleep(1);
	}

cleanup:
	return 0;
}
