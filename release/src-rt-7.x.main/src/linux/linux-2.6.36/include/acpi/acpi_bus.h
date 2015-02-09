/*
 *  acpi_bus.h - ACPI Bus Driver ($Revision: 22 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __ACPI_BUS_H__
#define __ACPI_BUS_H__

#include <linux/device.h>

#include <acpi/acpi.h>

/* TBD: Make dynamic */
#define ACPI_MAX_HANDLES	10
struct acpi_handle_list {
	u32 count;
	acpi_handle handles[ACPI_MAX_HANDLES];
};

/* acpi_utils.h */
acpi_status
acpi_extract_package(union acpi_object *package,
		     struct acpi_buffer *format, struct acpi_buffer *buffer);
acpi_status
acpi_evaluate_integer(acpi_handle handle,
		      acpi_string pathname,
		      struct acpi_object_list *arguments, unsigned long long *data);
acpi_status
acpi_evaluate_reference(acpi_handle handle,
			acpi_string pathname,
			struct acpi_object_list *arguments,
			struct acpi_handle_list *list);

#ifdef CONFIG_ACPI

#include <linux/proc_fs.h>

#define ACPI_BUS_FILE_ROOT	"acpi"
extern struct proc_dir_entry *acpi_root_dir;

enum acpi_bus_removal_type {
	ACPI_BUS_REMOVAL_NORMAL = 0,
	ACPI_BUS_REMOVAL_EJECT,
	ACPI_BUS_REMOVAL_SUPRISE,
	ACPI_BUS_REMOVAL_TYPE_COUNT
};

enum acpi_bus_device_type {
	ACPI_BUS_TYPE_DEVICE = 0,
	ACPI_BUS_TYPE_POWER,
	ACPI_BUS_TYPE_PROCESSOR,
	ACPI_BUS_TYPE_THERMAL,
	ACPI_BUS_TYPE_POWER_BUTTON,
	ACPI_BUS_TYPE_SLEEP_BUTTON,
	ACPI_BUS_DEVICE_TYPE_COUNT
};

struct acpi_driver;
struct acpi_device;

/*
 * ACPI Driver
 * -----------
 */

typedef int (*acpi_op_add) (struct acpi_device * device);
typedef int (*acpi_op_remove) (struct acpi_device * device, int type);
typedef int (*acpi_op_start) (struct acpi_device * device);
typedef int (*acpi_op_suspend) (struct acpi_device * device,
				pm_message_t state);
typedef int (*acpi_op_resume) (struct acpi_device * device);
typedef int (*acpi_op_bind) (struct acpi_device * device);
typedef int (*acpi_op_unbind) (struct acpi_device * device);
typedef void (*acpi_op_notify) (struct acpi_device * device, u32 event);

struct acpi_bus_ops {
	u32 acpi_op_add:1;
	u32 acpi_op_start:1;
};

struct acpi_device_ops {
	acpi_op_add add;
	acpi_op_remove remove;
	acpi_op_start start;
	acpi_op_suspend suspend;
	acpi_op_resume resume;
	acpi_op_bind bind;
	acpi_op_unbind unbind;
	acpi_op_notify notify;
};

#define ACPI_DRIVER_ALL_NOTIFY_EVENTS	0x1	/* system AND device events */

struct acpi_driver {
	char name[80];
	char class[80];
	const struct acpi_device_id *ids; /* Supported Hardware IDs */
	unsigned int flags;
	struct acpi_device_ops ops;
	struct device_driver drv;
	struct module *owner;
};

/*
 * ACPI Device
 * -----------
 */

/* Status (_STA) */

struct acpi_device_status {
	u32 present:1;
	u32 enabled:1;
	u32 show_in_ui:1;
	u32 functional:1;
	u32 battery_present:1;
	u32 reserved:27;
};

/* Flags */

struct acpi_device_flags {
	u32 dynamic_status:1;
	u32 bus_address:1;
	u32 removable:1;
	u32 ejectable:1;
	u32 lockable:1;
	u32 suprise_removal_ok:1;
	u32 power_manageable:1;
	u32 performance_manageable:1;
	u32 wake_capable:1;	/* Wakeup(_PRW) supported? */
	u32 force_power_state:1;
	u32 reserved:22;
};

/* File System */

struct acpi_device_dir {
	struct proc_dir_entry *entry;
};

#define acpi_device_dir(d)	((d)->dir.entry)

/* Plug and Play */

typedef char acpi_bus_id[8];
typedef unsigned long acpi_bus_address;
typedef char acpi_device_name[40];
typedef char acpi_device_class[20];

struct acpi_hardware_id {
	struct list_head list;
	char *id;
};

struct acpi_device_pnp {
	acpi_bus_id bus_id;	/* Object name */
	acpi_bus_address bus_address;	/* _ADR */
	char *unique_id;	/* _UID */
	struct list_head ids;		/* _HID and _CIDs */
	acpi_device_name device_name;	/* Driver-determined */
	acpi_device_class device_class;	/*        "          */
};

#define acpi_device_bid(d)	((d)->pnp.bus_id)
#define acpi_device_adr(d)	((d)->pnp.bus_address)
char *acpi_device_hid(struct acpi_device *device);
#define acpi_device_name(d)	((d)->pnp.device_name)
#define acpi_device_class(d)	((d)->pnp.device_class)

/* Power Management */

struct acpi_device_power_flags {
	u32 explicit_get:1;	/* _PSC present? */
	u32 power_resources:1;	/* Power resources */
	u32 inrush_current:1;	/* Serialize Dx->D0 */
	u32 power_removed:1;	/* Optimize Dx->D0 */
	u32 reserved:28;
};

struct acpi_device_power_state {
	struct {
		u8 valid:1;
		u8 explicit_set:1;	/* _PSx present? */
		u8 reserved:6;
	} flags;
	int power;		/* % Power (compared to D0) */
	int latency;		/* Dx->D0 time (microseconds) */
	struct acpi_handle_list resources;	/* Power resources referenced */
};

struct acpi_device_power {
	int state;		/* Current state */
	struct acpi_device_power_flags flags;
	struct acpi_device_power_state states[4];	/* Power states (D0-D3) */
};

/* Performance Management */

struct acpi_device_perf_flags {
	u8 reserved:8;
};

struct acpi_device_perf_state {
	struct {
		u8 valid:1;
		u8 reserved:7;
	} flags;
	u8 power;		/* % Power (compared to P0) */
	u8 performance;		/* % Performance (    "   ) */
	int latency;		/* Px->P0 time (microseconds) */
};

struct acpi_device_perf {
	int state;
	struct acpi_device_perf_flags flags;
	int state_count;
	struct acpi_device_perf_state *states;
};

/* Wakeup Management */
struct acpi_device_wakeup_flags {
	u8 valid:1;		/* Can successfully enable wakeup? */
	u8 run_wake:1;		/* Run-Wake GPE devices */
	u8 always_enabled:1;	/* Run-wake devices that are always enabled */
	u8 notifier_present:1;  /* Wake-up notify handler has been installed */
};

struct acpi_device_wakeup_state {
	u8 enabled:1;
};

struct acpi_device_wakeup {
	acpi_handle gpe_device;
	u64 gpe_number;
	u64 sleep_state;
	struct acpi_handle_list resources;
	struct acpi_device_wakeup_state state;
	struct acpi_device_wakeup_flags flags;
	int prepare_count;
	int run_wake_count;
};

/* Device */

struct acpi_device {
	int device_type;
	acpi_handle handle;		/* no handle for fixed hardware */
	struct acpi_device *parent;
	struct list_head children;
	struct list_head node;
	struct list_head wakeup_list;
	struct acpi_device_status status;
	struct acpi_device_flags flags;
	struct acpi_device_pnp pnp;
	struct acpi_device_power power;
	struct acpi_device_wakeup wakeup;
	struct acpi_device_perf performance;
	struct acpi_device_dir dir;
	struct acpi_device_ops ops;
	struct acpi_driver *driver;
	void *driver_data;
	struct device dev;
	struct acpi_bus_ops bus_ops;
	enum acpi_bus_removal_type removal_type;	/* indicate for different removal type */
};

static inline void *acpi_driver_data(struct acpi_device *d)
{
	return d->driver_data;
}

#define to_acpi_device(d)	container_of(d, struct acpi_device, dev)
#define to_acpi_driver(d)	container_of(d, struct acpi_driver, drv)

/* acpi_device.dev.bus == &acpi_bus_type */
extern struct bus_type acpi_bus_type;

/*
 * Events
 * ------
 */

struct acpi_bus_event {
	struct list_head node;
	acpi_device_class device_class;
	acpi_bus_id bus_id;
	u32 type;
	u32 data;
};

extern struct kobject *acpi_kobj;
extern int acpi_bus_generate_netlink_event(const char*, const char*, u8, int);
void acpi_bus_private_data_handler(acpi_handle, void *);
int acpi_bus_get_private_data(acpi_handle, void **);
extern int acpi_notifier_call_chain(struct acpi_device *, u32, u32);
extern int register_acpi_notifier(struct notifier_block *);
extern int unregister_acpi_notifier(struct notifier_block *);

extern int register_acpi_bus_notifier(struct notifier_block *nb);
extern void unregister_acpi_bus_notifier(struct notifier_block *nb);
/*
 * External Functions
 */

int acpi_bus_get_device(acpi_handle handle, struct acpi_device **device);
void acpi_bus_data_handler(acpi_handle handle, void *context);
acpi_status acpi_bus_get_status_handle(acpi_handle handle,
				       unsigned long long *sta);
int acpi_bus_get_status(struct acpi_device *device);
int acpi_bus_get_power(acpi_handle handle, int *state);
int acpi_bus_set_power(acpi_handle handle, int state);
bool acpi_bus_power_manageable(acpi_handle handle);
bool acpi_bus_can_wakeup(acpi_handle handle);
#ifdef CONFIG_ACPI_PROC_EVENT
int acpi_bus_generate_proc_event(struct acpi_device *device, u8 type, int data);
int acpi_bus_generate_proc_event4(const char *class, const char *bid, u8 type, int data);
int acpi_bus_receive_event(struct acpi_bus_event *event);
#else
static inline int acpi_bus_generate_proc_event(struct acpi_device *device, u8 type, int data)
	{ return 0; }
#endif
int acpi_bus_register_driver(struct acpi_driver *driver);
void acpi_bus_unregister_driver(struct acpi_driver *driver);
int acpi_bus_add(struct acpi_device **child, struct acpi_device *parent,
		 acpi_handle handle, int type);
int acpi_bus_trim(struct acpi_device *start, int rmdevice);
int acpi_bus_start(struct acpi_device *device);
acpi_status acpi_bus_get_ejd(acpi_handle handle, acpi_handle * ejd);
int acpi_match_device_ids(struct acpi_device *device,
			  const struct acpi_device_id *ids);
int acpi_create_dir(struct acpi_device *);
void acpi_remove_dir(struct acpi_device *);

/*
 * Bind physical devices with ACPI devices
 */
struct acpi_bus_type {
	struct list_head list;
	struct bus_type *bus;
	/* For general devices under the bus */
	int (*find_device) (struct device *, acpi_handle *);
	/* For bridges, such as PCI root bridge, IDE controller */
	int (*find_bridge) (struct device *, acpi_handle *);
};
int register_acpi_bus_type(struct acpi_bus_type *);
int unregister_acpi_bus_type(struct acpi_bus_type *);
struct device *acpi_get_physical_device(acpi_handle);

struct acpi_pci_root {
	struct list_head node;
	struct acpi_device * device;
	struct acpi_pci_id id;
	struct pci_bus *bus;
	u16 segment;
	struct resource secondary;	/* downstream bus range */

	u32 osc_support_set;	/* _OSC state of support bits */
	u32 osc_control_set;	/* _OSC state of control bits */
};

/* helper */
acpi_handle acpi_get_child(acpi_handle, u64);
int acpi_is_root_bridge(acpi_handle);
acpi_handle acpi_get_pci_rootbridge_handle(unsigned int, unsigned int);
struct acpi_pci_root *acpi_pci_find_root(acpi_handle handle);
#define DEVICE_ACPI_HANDLE(dev) ((acpi_handle)((dev)->archdata.acpi_handle))

int acpi_enable_wakeup_device_power(struct acpi_device *dev, int state);
int acpi_disable_wakeup_device_power(struct acpi_device *dev);

#ifdef CONFIG_PM_SLEEP
int acpi_pm_device_sleep_state(struct device *, int *);
int acpi_pm_device_sleep_wake(struct device *, bool);
#else /* !CONFIG_PM_SLEEP */
static inline int acpi_pm_device_sleep_state(struct device *d, int *p)
{
	if (p)
		*p = ACPI_STATE_D0;
	return ACPI_STATE_D3;
}
static inline int acpi_pm_device_sleep_wake(struct device *dev, bool enable)
{
	return -ENODEV;
}
#endif /* !CONFIG_PM_SLEEP */

#endif				/* CONFIG_ACPI */

#endif /*__ACPI_BUS_H__*/
