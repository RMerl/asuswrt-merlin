/*
 * Copyright (C) 2004 IBM Corporation
 *
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Dave Safford <safford@watson.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Maintained by: <tpmdd-devel@lists.sourceforge.net>
 *
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org	 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 * 
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/tpm.h>

enum tpm_timeout {
	TPM_TIMEOUT = 5,	/* msecs */
};

/* TPM addresses */
enum tpm_addr {
	TPM_SUPERIO_ADDR = 0x2E,
	TPM_ADDR = 0x4E,
};

extern ssize_t tpm_show_pubek(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_pcrs(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_caps(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_caps_1_2(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_store_cancel(struct device *, struct device_attribute *attr,
				const char *, size_t);
extern ssize_t tpm_show_enabled(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_active(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_owned(struct device *, struct device_attribute *attr,
				char *);
extern ssize_t tpm_show_temp_deactivated(struct device *,
					 struct device_attribute *attr, char *);

struct tpm_chip;

struct tpm_vendor_specific {
	const u8 req_complete_mask;
	const u8 req_complete_val;
	const u8 req_canceled;
	void __iomem *iobase;		/* ioremapped address */
	unsigned long base;		/* TPM base address */

	int irq;

	int region_size;
	int have_region;

	int (*recv) (struct tpm_chip *, u8 *, size_t);
	int (*send) (struct tpm_chip *, u8 *, size_t);
	void (*cancel) (struct tpm_chip *);
	u8 (*status) (struct tpm_chip *);
	void (*release) (struct device *);
	struct miscdevice miscdev;
	struct attribute_group *attr_group;
	struct list_head list;
	int locality;
	unsigned long timeout_a, timeout_b, timeout_c, timeout_d; /* jiffies */
	unsigned long duration[3]; /* jiffies */

	wait_queue_head_t read_queue;
	wait_queue_head_t int_queue;
};

struct tpm_chip {
	struct device *dev;	/* Device stuff */

	int dev_num;		/* /dev/tpm# */
	unsigned long is_open;	/* only one allowed */
	int time_expired;

	/* Data passed to and from the tpm via the read/write calls */
	u8 *data_buffer;
	atomic_t data_pending;
	struct mutex buffer_mutex;

	struct timer_list user_read_timer;	/* user needs to claim result */
	struct work_struct work;
	struct mutex tpm_mutex;	/* tpm is processing */

	struct tpm_vendor_specific vendor;

	struct dentry **bios_dir;

	struct list_head list;
	void (*release) (struct device *);
};

#define to_tpm_chip(n) container_of(n, struct tpm_chip, vendor)

static inline void tpm_chip_put(struct tpm_chip *chip)
{
	module_put(chip->dev->driver->owner);
}

static inline int tpm_read_index(int base, int index)
{
	outb(index, base);
	return inb(base+1) & 0xFF;
}

static inline void tpm_write_index(int base, int index, int value)
{
	outb(index, base);
	outb(value & 0xFF, base+1);
}
struct tpm_input_header {
	__be16	tag;
	__be32	length;
	__be32	ordinal;
}__attribute__((packed));

struct tpm_output_header {
	__be16	tag;
	__be32	length;
	__be32	return_code;
}__attribute__((packed));

struct	stclear_flags_t {
	__be16	tag;
	u8	deactivated;
	u8	disableForceClear;
	u8	physicalPresence;
	u8	physicalPresenceLock;
	u8	bGlobalLock;
}__attribute__((packed));

struct	tpm_version_t {
	u8	Major;
	u8	Minor;
	u8	revMajor;
	u8	revMinor;
}__attribute__((packed));

struct	tpm_version_1_2_t {
	__be16	tag;
	u8	Major;
	u8	Minor;
	u8	revMajor;
	u8	revMinor;
}__attribute__((packed));

struct	timeout_t {
	__be32	a;
	__be32	b;
	__be32	c;
	__be32	d;
}__attribute__((packed));

struct duration_t {
	__be32	tpm_short;
	__be32	tpm_medium;
	__be32	tpm_long;
}__attribute__((packed));

struct permanent_flags_t {
	__be16	tag;
	u8	disable;
	u8	ownership;
	u8	deactivated;
	u8	readPubek;
	u8	disableOwnerClear;
	u8	allowMaintenance;
	u8	physicalPresenceLifetimeLock;
	u8	physicalPresenceHWEnable;
	u8	physicalPresenceCMDEnable;
	u8	CEKPUsed;
	u8	TPMpost;
	u8	TPMpostLock;
	u8	FIPS;
	u8	operator;
	u8	enableRevokeEK;
	u8	nvLocked;
	u8	readSRKPub;
	u8	tpmEstablished;
	u8	maintenanceDone;
	u8	disableFullDALogicInfo;
}__attribute__((packed));

typedef union {
	struct	permanent_flags_t perm_flags;
	struct	stclear_flags_t	stclear_flags;
	bool	owned;
	__be32	num_pcrs;
	struct	tpm_version_t	tpm_version;
	struct	tpm_version_1_2_t tpm_version_1_2;
	__be32	manufacturer_id;
	struct timeout_t  timeout;
	struct duration_t duration;
} cap_t;

struct	tpm_getcap_params_in {
	__be32	cap;
	__be32	subcap_size;
	__be32	subcap;
}__attribute__((packed));

struct	tpm_getcap_params_out {
	__be32	cap_size;
	cap_t	cap;
}__attribute__((packed));

struct	tpm_readpubek_params_out {
	u8	algorithm[4];
	u8	encscheme[2];
	u8	sigscheme[2];
	__be32	paramsize;
	u8	parameters[12]; /*assuming RSA*/
	__be32	keysize;
	u8	modulus[256];
	u8	checksum[20];
}__attribute__((packed));

typedef union {
	struct	tpm_input_header in;
	struct	tpm_output_header out;
} tpm_cmd_header;

#define TPM_DIGEST_SIZE 20
struct tpm_pcrread_out {
	u8	pcr_result[TPM_DIGEST_SIZE];
}__attribute__((packed));

struct tpm_pcrread_in {
	__be32	pcr_idx;
}__attribute__((packed));

struct tpm_pcrextend_in {
	__be32	pcr_idx;
	u8	hash[TPM_DIGEST_SIZE];
}__attribute__((packed));

typedef union {
	struct	tpm_getcap_params_out getcap_out;
	struct	tpm_readpubek_params_out readpubek_out;
	u8	readpubek_out_buffer[sizeof(struct tpm_readpubek_params_out)];
	struct	tpm_getcap_params_in getcap_in;
	struct	tpm_pcrread_in	pcrread_in;
	struct	tpm_pcrread_out	pcrread_out;
	struct	tpm_pcrextend_in pcrextend_in;
} tpm_cmd_params;

struct tpm_cmd_t {
	tpm_cmd_header	header;
	tpm_cmd_params	params;
}__attribute__((packed));

ssize_t	tpm_getcap(struct device *, __be32, cap_t *, const char *);

extern void tpm_get_timeouts(struct tpm_chip *);
extern void tpm_gen_interrupt(struct tpm_chip *);
extern void tpm_continue_selftest(struct tpm_chip *);
extern unsigned long tpm_calc_ordinal_duration(struct tpm_chip *, u32);
extern struct tpm_chip* tpm_register_hardware(struct device *,
				 const struct tpm_vendor_specific *);
extern int tpm_open(struct inode *, struct file *);
extern int tpm_release(struct inode *, struct file *);
extern void tpm_dev_vendor_release(struct tpm_chip *);
extern ssize_t tpm_write(struct file *, const char __user *, size_t,
			 loff_t *);
extern ssize_t tpm_read(struct file *, char __user *, size_t, loff_t *);
extern void tpm_remove_hardware(struct device *);
extern int tpm_pm_suspend(struct device *, pm_message_t);
extern int tpm_pm_resume(struct device *);

#ifdef CONFIG_ACPI
extern struct dentry ** tpm_bios_log_setup(char *);
extern void tpm_bios_log_teardown(struct dentry **);
#else
static inline struct dentry ** tpm_bios_log_setup(char *name)
{
	return NULL;
}
static inline void tpm_bios_log_teardown(struct dentry **dir)
{
}
#endif
