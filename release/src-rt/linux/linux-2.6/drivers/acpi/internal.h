/*
 * acpi/internal.h
 * For use by Linux/ACPI infrastructure, not drivers
 *
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ACPI_INTERNAL_H_
#define _ACPI_INTERNAL_H_

#define PREFIX "ACPI: "

int init_acpi_device_notify(void);
int acpi_scan_init(void);
int acpi_sysfs_init(void);

#ifdef CONFIG_DEBUG_FS
int acpi_debugfs_init(void);
#else
static inline int acpi_debugfs_init(void) { return 0; }
#endif

/* --------------------------------------------------------------------------
                                  Power Resource
   -------------------------------------------------------------------------- */
int acpi_power_init(void);
int acpi_device_sleep_wake(struct acpi_device *dev,
                           int enable, int sleep_state, int dev_state);
int acpi_power_get_inferred_state(struct acpi_device *device, int *state);
int acpi_power_on_resources(struct acpi_device *device, int state);
int acpi_power_transition(struct acpi_device *device, int state);
int acpi_bus_init_power(struct acpi_device *device);

int acpi_wakeup_device_init(void);
void acpi_early_processor_set_pdc(void);

/* --------------------------------------------------------------------------
                                  Embedded Controller
   -------------------------------------------------------------------------- */
struct acpi_ec {
	acpi_handle handle;
	unsigned long gpe;
	unsigned long command_addr;
	unsigned long data_addr;
	unsigned long global_lock;
	unsigned long flags;
	struct mutex lock;
	wait_queue_head_t wait;
	struct list_head list;
	struct transaction *curr;
	spinlock_t curr_lock;
};

extern struct acpi_ec *first_ec;

int acpi_ec_init(void);
int acpi_ec_ecdt_probe(void);
int acpi_boot_ec_enable(void);
void acpi_ec_block_transactions(void);
void acpi_ec_unblock_transactions(void);
void acpi_ec_unblock_transactions_early(void);

/*--------------------------------------------------------------------------
                                  Suspend/Resume
  -------------------------------------------------------------------------- */
extern int acpi_sleep_init(void);

#ifdef CONFIG_ACPI_SLEEP
int acpi_sleep_proc_init(void);
int suspend_nvs_alloc(void);
void suspend_nvs_free(void);
int suspend_nvs_save(void);
void suspend_nvs_restore(void);
#else
static inline int acpi_sleep_proc_init(void) { return 0; }
static inline int suspend_nvs_alloc(void) { return 0; }
static inline void suspend_nvs_free(void) {}
static inline int suspend_nvs_save(void) { return 0; }
static inline void suspend_nvs_restore(void) {}
#endif

#endif /* _ACPI_INTERNAL_H_ */
