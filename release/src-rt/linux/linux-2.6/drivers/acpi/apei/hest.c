/*
 * APEI Hardware Error Souce Table support
 *
 * HEST describes error sources in detail; communicates operational
 * parameters (i.e. severity levels, masking bits, and threshold
 * values) to Linux as necessary. It also allows the BIOS to report
 * non-standard error sources to Linux (for example, chipset-specific
 * error registers).
 *
 * For more information about HEST, please refer to ACPI Specification
 * version 4.0, section 17.3.2.
 *
 * Copyright 2009 Intel Corp.
 *   Author: Huang Ying <ying.huang@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/kdebug.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <acpi/apei.h>

#include "apei-internal.h"

#define HEST_PFX "HEST: "

int hest_disable;
EXPORT_SYMBOL_GPL(hest_disable);

/* HEST table parsing */

static struct acpi_table_hest *__read_mostly hest_tab;

static const int hest_esrc_len_tab[ACPI_HEST_TYPE_RESERVED] = {
	[ACPI_HEST_TYPE_IA32_CHECK] = -1,	/* need further calculation */
	[ACPI_HEST_TYPE_IA32_CORRECTED_CHECK] = -1,
	[ACPI_HEST_TYPE_IA32_NMI] = sizeof(struct acpi_hest_ia_nmi),
	[ACPI_HEST_TYPE_AER_ROOT_PORT] = sizeof(struct acpi_hest_aer_root),
	[ACPI_HEST_TYPE_AER_ENDPOINT] = sizeof(struct acpi_hest_aer),
	[ACPI_HEST_TYPE_AER_BRIDGE] = sizeof(struct acpi_hest_aer_bridge),
	[ACPI_HEST_TYPE_GENERIC_ERROR] = sizeof(struct acpi_hest_generic),
};

static int hest_esrc_len(struct acpi_hest_header *hest_hdr)
{
	u16 hest_type = hest_hdr->type;
	int len;

	if (hest_type >= ACPI_HEST_TYPE_RESERVED)
		return 0;

	len = hest_esrc_len_tab[hest_type];

	if (hest_type == ACPI_HEST_TYPE_IA32_CORRECTED_CHECK) {
		struct acpi_hest_ia_corrected *cmc;
		cmc = (struct acpi_hest_ia_corrected *)hest_hdr;
		len = sizeof(*cmc) + cmc->num_hardware_banks *
			sizeof(struct acpi_hest_ia_error_bank);
	} else if (hest_type == ACPI_HEST_TYPE_IA32_CHECK) {
		struct acpi_hest_ia_machine_check *mc;
		mc = (struct acpi_hest_ia_machine_check *)hest_hdr;
		len = sizeof(*mc) + mc->num_hardware_banks *
			sizeof(struct acpi_hest_ia_error_bank);
	}
	BUG_ON(len == -1);

	return len;
};

int apei_hest_parse(apei_hest_func_t func, void *data)
{
	struct acpi_hest_header *hest_hdr;
	int i, rc, len;

	if (hest_disable)
		return -EINVAL;

	hest_hdr = (struct acpi_hest_header *)(hest_tab + 1);
	for (i = 0; i < hest_tab->error_source_count; i++) {
		len = hest_esrc_len(hest_hdr);
		if (!len) {
			pr_warning(FW_WARN HEST_PFX
				   "Unknown or unused hardware error source "
				   "type: %d for hardware error source: %d.\n",
				   hest_hdr->type, hest_hdr->source_id);
			return -EINVAL;
		}
		if ((void *)hest_hdr + len >
		    (void *)hest_tab + hest_tab->header.length) {
			pr_warning(FW_BUG HEST_PFX
		"Table contents overflow for hardware error source: %d.\n",
				hest_hdr->source_id);
			return -EINVAL;
		}

		rc = func(hest_hdr, data);
		if (rc)
			return rc;

		hest_hdr = (void *)hest_hdr + len;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(apei_hest_parse);

struct ghes_arr {
	struct platform_device **ghes_devs;
	unsigned int count;
};

static int __init hest_parse_ghes_count(struct acpi_hest_header *hest_hdr, void *data)
{
	int *count = data;

	if (hest_hdr->type == ACPI_HEST_TYPE_GENERIC_ERROR)
		(*count)++;
	return 0;
}

static int __init hest_parse_ghes(struct acpi_hest_header *hest_hdr, void *data)
{
	struct platform_device *ghes_dev;
	struct ghes_arr *ghes_arr = data;
	int rc;

	if (hest_hdr->type != ACPI_HEST_TYPE_GENERIC_ERROR)
		return 0;

	if (!((struct acpi_hest_generic *)hest_hdr)->enabled)
		return 0;
	ghes_dev = platform_device_alloc("GHES", hest_hdr->source_id);
	if (!ghes_dev)
		return -ENOMEM;

	rc = platform_device_add_data(ghes_dev, &hest_hdr, sizeof(void *));
	if (rc)
		goto err;

	rc = platform_device_add(ghes_dev);
	if (rc)
		goto err;
	ghes_arr->ghes_devs[ghes_arr->count++] = ghes_dev;

	return 0;
err:
	platform_device_put(ghes_dev);
	return rc;
}

static int __init hest_ghes_dev_register(unsigned int ghes_count)
{
	int rc, i;
	struct ghes_arr ghes_arr;

	ghes_arr.count = 0;
	ghes_arr.ghes_devs = kmalloc(sizeof(void *) * ghes_count, GFP_KERNEL);
	if (!ghes_arr.ghes_devs)
		return -ENOMEM;

	rc = apei_hest_parse(hest_parse_ghes, &ghes_arr);
	if (rc)
		goto err;
out:
	kfree(ghes_arr.ghes_devs);
	return rc;
err:
	for (i = 0; i < ghes_arr.count; i++)
		platform_device_unregister(ghes_arr.ghes_devs[i]);
	goto out;
}

static int __init setup_hest_disable(char *str)
{
	hest_disable = 1;
	return 0;
}

__setup("hest_disable", setup_hest_disable);

void __init acpi_hest_init(void)
{
	acpi_status status;
	int rc = -ENODEV;
	unsigned int ghes_count = 0;

	if (hest_disable) {
		pr_info(HEST_PFX "Table parsing disabled.\n");
		return;
	}

	if (acpi_disabled)
		goto err;

	status = acpi_get_table(ACPI_SIG_HEST, 0,
				(struct acpi_table_header **)&hest_tab);
	if (status == AE_NOT_FOUND) {
		pr_info(HEST_PFX "Table not found.\n");
		goto err;
	} else if (ACPI_FAILURE(status)) {
		const char *msg = acpi_format_exception(status);
		pr_err(HEST_PFX "Failed to get table, %s\n", msg);
		rc = -EINVAL;
		goto err;
	}

	rc = apei_hest_parse(hest_parse_ghes_count, &ghes_count);
	if (rc)
		goto err;

	rc = hest_ghes_dev_register(ghes_count);
	if (!rc) {
		pr_info(HEST_PFX "Table parsing has been initialized.\n");
		return;
	}

err:
	hest_disable = 1;
}
