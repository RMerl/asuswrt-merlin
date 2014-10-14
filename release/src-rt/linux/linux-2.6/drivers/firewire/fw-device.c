/*
 * Device probing and sysfs code.
 *
 * Copyright (C) 2005-2006  Kristian Hoegsberg <krh@bitplanet.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/rwsem.h>
#include <asm/semaphore.h>
#include <linux/ctype.h>
#include "fw-transaction.h"
#include "fw-topology.h"
#include "fw-device.h"

void fw_csr_iterator_init(struct fw_csr_iterator *ci, u32 * p)
{
	ci->p = p + 1;
	ci->end = ci->p + (p[0] >> 16);
}
EXPORT_SYMBOL(fw_csr_iterator_init);

int fw_csr_iterator_next(struct fw_csr_iterator *ci, int *key, int *value)
{
	*key = *ci->p >> 24;
	*value = *ci->p & 0xffffff;

	return ci->p++ < ci->end;
}
EXPORT_SYMBOL(fw_csr_iterator_next);

static int is_fw_unit(struct device *dev);

static int match_unit_directory(u32 * directory, const struct fw_device_id *id)
{
	struct fw_csr_iterator ci;
	int key, value, match;

	match = 0;
	fw_csr_iterator_init(&ci, directory);
	while (fw_csr_iterator_next(&ci, &key, &value)) {
		if (key == CSR_VENDOR && value == id->vendor)
			match |= FW_MATCH_VENDOR;
		if (key == CSR_MODEL && value == id->model)
			match |= FW_MATCH_MODEL;
		if (key == CSR_SPECIFIER_ID && value == id->specifier_id)
			match |= FW_MATCH_SPECIFIER_ID;
		if (key == CSR_VERSION && value == id->version)
			match |= FW_MATCH_VERSION;
	}

	return (match & id->match_flags) == id->match_flags;
}

static int fw_unit_match(struct device *dev, struct device_driver *drv)
{
	struct fw_unit *unit = fw_unit(dev);
	struct fw_driver *driver = fw_driver(drv);
	int i;

	/* We only allow binding to fw_units. */
	if (!is_fw_unit(dev))
		return 0;

	for (i = 0; driver->id_table[i].match_flags != 0; i++) {
		if (match_unit_directory(unit->directory, &driver->id_table[i]))
			return 1;
	}

	return 0;
}

static int get_modalias(struct fw_unit *unit, char *buffer, size_t buffer_size)
{
	struct fw_device *device = fw_device(unit->device.parent);
	struct fw_csr_iterator ci;

	int key, value;
	int vendor = 0;
	int model = 0;
	int specifier_id = 0;
	int version = 0;

	fw_csr_iterator_init(&ci, &device->config_rom[5]);
	while (fw_csr_iterator_next(&ci, &key, &value)) {
		switch (key) {
		case CSR_VENDOR:
			vendor = value;
			break;
		case CSR_MODEL:
			model = value;
			break;
		}
	}

	fw_csr_iterator_init(&ci, unit->directory);
	while (fw_csr_iterator_next(&ci, &key, &value)) {
		switch (key) {
		case CSR_SPECIFIER_ID:
			specifier_id = value;
			break;
		case CSR_VERSION:
			version = value;
			break;
		}
	}

	return snprintf(buffer, buffer_size,
			"ieee1394:ven%08Xmo%08Xsp%08Xver%08X",
			vendor, model, specifier_id, version);
}

static int
fw_unit_uevent(struct device *dev, char **envp, int num_envp,
	       char *buffer, int buffer_size)
{
	struct fw_unit *unit = fw_unit(dev);
	char modalias[64];
	int length = 0;
	int i = 0;

	get_modalias(unit, modalias, sizeof(modalias));

	if (add_uevent_var(envp, num_envp, &i,
			   buffer, buffer_size, &length,
			   "MODALIAS=%s", modalias))
		return -ENOMEM;

	envp[i] = NULL;

	return 0;
}

struct bus_type fw_bus_type = {
	.name = "firewire",
	.match = fw_unit_match,
};
EXPORT_SYMBOL(fw_bus_type);

struct fw_device *fw_device_get(struct fw_device *device)
{
	get_device(&device->device);

	return device;
}

void fw_device_put(struct fw_device *device)
{
	put_device(&device->device);
}

static void fw_device_release(struct device *dev)
{
	struct fw_device *device = fw_device(dev);
	unsigned long flags;

	/*
	 * Take the card lock so we don't set this to NULL while a
	 * FW_NODE_UPDATED callback is being handled.
	 */
	spin_lock_irqsave(&device->card->lock, flags);
	device->node->data = NULL;
	spin_unlock_irqrestore(&device->card->lock, flags);

	fw_node_put(device->node);
	fw_card_put(device->card);
	kfree(device->config_rom);
	kfree(device);
}

int fw_device_enable_phys_dma(struct fw_device *device)
{
	return device->card->driver->enable_phys_dma(device->card,
						     device->node_id,
						     device->generation);
}
EXPORT_SYMBOL(fw_device_enable_phys_dma);

struct config_rom_attribute {
	struct device_attribute attr;
	u32 key;
};

static ssize_t
show_immediate(struct device *dev, struct device_attribute *dattr, char *buf)
{
	struct config_rom_attribute *attr =
		container_of(dattr, struct config_rom_attribute, attr);
	struct fw_csr_iterator ci;
	u32 *dir;
	int key, value;

	if (is_fw_unit(dev))
		dir = fw_unit(dev)->directory;
	else
		dir = fw_device(dev)->config_rom + 5;

	fw_csr_iterator_init(&ci, dir);
	while (fw_csr_iterator_next(&ci, &key, &value))
		if (attr->key == key)
			return snprintf(buf, buf ? PAGE_SIZE : 0,
					"0x%06x\n", value);

	return -ENOENT;
}

#define IMMEDIATE_ATTR(name, key)				\
	{ __ATTR(name, S_IRUGO, show_immediate, NULL), key }

static ssize_t
show_text_leaf(struct device *dev, struct device_attribute *dattr, char *buf)
{
	struct config_rom_attribute *attr =
		container_of(dattr, struct config_rom_attribute, attr);
	struct fw_csr_iterator ci;
	u32 *dir, *block = NULL, *p, *end;
	int length, key, value, last_key = 0;
	char *b;

	if (is_fw_unit(dev))
		dir = fw_unit(dev)->directory;
	else
		dir = fw_device(dev)->config_rom + 5;

	fw_csr_iterator_init(&ci, dir);
	while (fw_csr_iterator_next(&ci, &key, &value)) {
		if (attr->key == last_key &&
		    key == (CSR_DESCRIPTOR | CSR_LEAF))
			block = ci.p - 1 + value;
		last_key = key;
	}

	if (block == NULL)
		return -ENOENT;

	length = min(block[0] >> 16, 256U);
	if (length < 3)
		return -ENOENT;

	if (block[1] != 0 || block[2] != 0)
		/* Unknown encoding. */
		return -ENOENT;

	if (buf == NULL)
		return length * 4;

	b = buf;
	end = &block[length + 1];
	for (p = &block[3]; p < end; p++, b += 4)
		* (u32 *) b = (__force u32) __cpu_to_be32(*p);

	/* Strip trailing whitespace and add newline. */
	while (b--, (isspace(*b) || *b == '\0') && b > buf);
	strcpy(b + 1, "\n");

	return b + 2 - buf;
}

#define TEXT_LEAF_ATTR(name, key)				\
	{ __ATTR(name, S_IRUGO, show_text_leaf, NULL), key }

static struct config_rom_attribute config_rom_attributes[] = {
	IMMEDIATE_ATTR(vendor, CSR_VENDOR),
	IMMEDIATE_ATTR(hardware_version, CSR_HARDWARE_VERSION),
	IMMEDIATE_ATTR(specifier_id, CSR_SPECIFIER_ID),
	IMMEDIATE_ATTR(version, CSR_VERSION),
	IMMEDIATE_ATTR(model, CSR_MODEL),
	TEXT_LEAF_ATTR(vendor_name, CSR_VENDOR),
	TEXT_LEAF_ATTR(model_name, CSR_MODEL),
	TEXT_LEAF_ATTR(hardware_version_name, CSR_HARDWARE_VERSION),
};

static void
init_fw_attribute_group(struct device *dev,
			struct device_attribute *attrs,
			struct fw_attribute_group *group)
{
	struct device_attribute *attr;
	int i, j;

	for (j = 0; attrs[j].attr.name != NULL; j++)
		group->attrs[j] = &attrs[j].attr;

	for (i = 0; i < ARRAY_SIZE(config_rom_attributes); i++) {
		attr = &config_rom_attributes[i].attr;
		if (attr->show(dev, attr, NULL) < 0)
			continue;
		group->attrs[j++] = &attr->attr;
	}

	BUG_ON(j >= ARRAY_SIZE(group->attrs));
	group->attrs[j++] = NULL;
	group->groups[0] = &group->group;
	group->groups[1] = NULL;
	group->group.attrs = group->attrs;
	dev->groups = group->groups;
}

static ssize_t
modalias_show(struct device *dev,
	      struct device_attribute *attr, char *buf)
{
	struct fw_unit *unit = fw_unit(dev);
	int length;

	length = get_modalias(unit, buf, PAGE_SIZE);
	strcpy(buf + length, "\n");

	return length + 1;
}

static ssize_t
rom_index_show(struct device *dev,
	       struct device_attribute *attr, char *buf)
{
	struct fw_device *device = fw_device(dev->parent);
	struct fw_unit *unit = fw_unit(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(int)(unit->directory - device->config_rom));
}

static struct device_attribute fw_unit_attributes[] = {
	__ATTR_RO(modalias),
	__ATTR_RO(rom_index),
	__ATTR_NULL,
};

static ssize_t
config_rom_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fw_device *device = fw_device(dev);

	memcpy(buf, device->config_rom, device->config_rom_length * 4);

	return device->config_rom_length * 4;
}

static ssize_t
guid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fw_device *device = fw_device(dev);
	u64 guid;

	guid = ((u64)device->config_rom[3] << 32) | device->config_rom[4];

	return snprintf(buf, PAGE_SIZE, "0x%016llx\n",
			(unsigned long long)guid);
}

static struct device_attribute fw_device_attributes[] = {
	__ATTR_RO(config_rom),
	__ATTR_RO(guid),
	__ATTR_NULL,
};

struct read_quadlet_callback_data {
	struct completion done;
	int rcode;
	u32 data;
};

static void
complete_transaction(struct fw_card *card, int rcode,
		     void *payload, size_t length, void *data)
{
	struct read_quadlet_callback_data *callback_data = data;

	if (rcode == RCODE_COMPLETE)
		callback_data->data = be32_to_cpu(*(__be32 *)payload);
	callback_data->rcode = rcode;
	complete(&callback_data->done);
}

static int read_rom(struct fw_device *device, int index, u32 * data)
{
	struct read_quadlet_callback_data callback_data;
	struct fw_transaction t;
	u64 offset;

	init_completion(&callback_data.done);

	offset = 0xfffff0000400ULL + index * 4;
	fw_send_request(device->card, &t, TCODE_READ_QUADLET_REQUEST,
			device->node_id,
			device->generation, SCODE_100,
			offset, NULL, 4, complete_transaction, &callback_data);

	wait_for_completion(&callback_data.done);

	*data = callback_data.data;

	return callback_data.rcode;
}

static int read_bus_info_block(struct fw_device *device)
{
	static u32 rom[256];
	u32 stack[16], sp, key;
	int i, end, length;

	/* First read the bus info block. */
	for (i = 0; i < 5; i++) {
		if (read_rom(device, i, &rom[i]) != RCODE_COMPLETE)
			return -1;
		/*
		 * As per IEEE1212 7.2, during power-up, devices can
		 * reply with a 0 for the first quadlet of the config
		 * rom to indicate that they are booting (for example,
		 * if the firmware is on the disk of a external
		 * harddisk).  In that case we just fail, and the
		 * retry mechanism will try again later.
		 */
		if (i == 0 && rom[i] == 0)
			return -1;
	}

	/*
	 * Now parse the config rom.  The config rom is a recursive
	 * directory structure so we parse it using a stack of
	 * references to the blocks that make up the structure.  We
	 * push a reference to the root directory on the stack to
	 * start things off.
	 */
	length = i;
	sp = 0;
	stack[sp++] = 0xc0000005;
	while (sp > 0) {
		/*
		 * Pop the next block reference of the stack.  The
		 * lower 24 bits is the offset into the config rom,
		 * the upper 8 bits are the type of the reference the
		 * block.
		 */
		key = stack[--sp];
		i = key & 0xffffff;
		if (i >= ARRAY_SIZE(rom))
			/*
			 * The reference points outside the standard
			 * config rom area, something's fishy.
			 */
			return -1;

		/* Read header quadlet for the block to get the length. */
		if (read_rom(device, i, &rom[i]) != RCODE_COMPLETE)
			return -1;
		end = i + (rom[i] >> 16) + 1;
		i++;
		if (end > ARRAY_SIZE(rom))
			/*
			 * This block extends outside standard config
			 * area (and the array we're reading it
			 * into).  That's broken, so ignore this
			 * device.
			 */
			return -1;

		/*
		 * Now read in the block.  If this is a directory
		 * block, check the entries as we read them to see if
		 * it references another block, and push it in that case.
		 */
		while (i < end) {
			if (read_rom(device, i, &rom[i]) != RCODE_COMPLETE)
				return -1;
			if ((key >> 30) == 3 && (rom[i] >> 30) > 1 &&
			    sp < ARRAY_SIZE(stack))
				stack[sp++] = i + rom[i];
			i++;
		}
		if (length < i)
			length = i;
	}

	device->config_rom = kmalloc(length * 4, GFP_KERNEL);
	if (device->config_rom == NULL)
		return -1;
	memcpy(device->config_rom, rom, length * 4);
	device->config_rom_length = length;

	return 0;
}

static void fw_unit_release(struct device *dev)
{
	struct fw_unit *unit = fw_unit(dev);

	kfree(unit);
}

static struct device_type fw_unit_type = {
	.uevent		= fw_unit_uevent,
	.release	= fw_unit_release,
};

static int is_fw_unit(struct device *dev)
{
	return dev->type == &fw_unit_type;
}

static void create_units(struct fw_device *device)
{
	struct fw_csr_iterator ci;
	struct fw_unit *unit;
	int key, value, i;

	i = 0;
	fw_csr_iterator_init(&ci, &device->config_rom[5]);
	while (fw_csr_iterator_next(&ci, &key, &value)) {
		if (key != (CSR_UNIT | CSR_DIRECTORY))
			continue;

		/*
		 * Get the address of the unit directory and try to
		 * match the drivers id_tables against it.
		 */
		unit = kzalloc(sizeof(*unit), GFP_KERNEL);
		if (unit == NULL) {
			fw_error("failed to allocate memory for unit\n");
			continue;
		}

		unit->directory = ci.p + value - 1;
		unit->device.bus = &fw_bus_type;
		unit->device.type = &fw_unit_type;
		unit->device.parent = &device->device;
		snprintf(unit->device.bus_id, sizeof(unit->device.bus_id),
			 "%s.%d", device->device.bus_id, i++);

		init_fw_attribute_group(&unit->device,
					fw_unit_attributes,
					&unit->attribute_group);
		if (device_register(&unit->device) < 0)
			goto skip_unit;

		continue;

	skip_unit:
		kfree(unit);
	}
}

static int shutdown_unit(struct device *device, void *data)
{
	device_unregister(device);

	return 0;
}

static DECLARE_RWSEM(idr_rwsem);
static DEFINE_IDR(fw_device_idr);
int fw_cdev_major;

struct fw_device *fw_device_from_devt(dev_t devt)
{
	struct fw_device *device;

	down_read(&idr_rwsem);
	device = idr_find(&fw_device_idr, MINOR(devt));
	up_read(&idr_rwsem);

	return device;
}

static void fw_device_shutdown(struct work_struct *work)
{
	struct fw_device *device =
		container_of(work, struct fw_device, work.work);
	int minor = MINOR(device->device.devt);

	down_write(&idr_rwsem);
	idr_remove(&fw_device_idr, minor);
	up_write(&idr_rwsem);

	fw_device_cdev_remove(device);
	device_for_each_child(&device->device, NULL, shutdown_unit);
	device_unregister(&device->device);
}

static struct device_type fw_device_type = {
	.release	= fw_device_release,
};

/*
 * These defines control the retry behavior for reading the config
 * rom.  It shouldn't be necessary to tweak these; if the device
 * doesn't respond to a config rom read within 10 seconds, it's not
 * going to respond at all.  As for the initial delay, a lot of
 * devices will be able to respond within half a second after bus
 * reset.  On the other hand, it's not really worth being more
 * aggressive than that, since it scales pretty well; if 10 devices
 * are plugged in, they're all getting read within one second.
 */

#define MAX_RETRIES	10
#define RETRY_DELAY	(3 * HZ)
#define INITIAL_DELAY	(HZ / 2)

static void fw_device_init(struct work_struct *work)
{
	struct fw_device *device =
		container_of(work, struct fw_device, work.work);
	int minor, err;

	/*
	 * All failure paths here set node->data to NULL, so that we
	 * don't try to do device_for_each_child() on a kfree()'d
	 * device.
	 */

	if (read_bus_info_block(device) < 0) {
		if (device->config_rom_retries < MAX_RETRIES) {
			device->config_rom_retries++;
			schedule_delayed_work(&device->work, RETRY_DELAY);
		} else {
			fw_notify("giving up on config rom for node id %x\n",
				  device->node_id);
			if (device->node == device->card->root_node)
				schedule_delayed_work(&device->card->work, 0);
			fw_device_release(&device->device);
		}
		return;
	}

	err = -ENOMEM;
	down_write(&idr_rwsem);
	if (idr_pre_get(&fw_device_idr, GFP_KERNEL))
		err = idr_get_new(&fw_device_idr, device, &minor);
	up_write(&idr_rwsem);
	if (err < 0)
		goto error;

	device->device.bus = &fw_bus_type;
	device->device.type = &fw_device_type;
	device->device.parent = device->card->device;
	device->device.devt = MKDEV(fw_cdev_major, minor);
	snprintf(device->device.bus_id, sizeof(device->device.bus_id),
		 "fw%d", minor);

	init_fw_attribute_group(&device->device,
				fw_device_attributes,
				&device->attribute_group);
	if (device_add(&device->device)) {
		fw_error("Failed to add device.\n");
		goto error_with_cdev;
	}

	create_units(device);

	/*
	 * Transition the device to running state.  If it got pulled
	 * out from under us while we did the intialization work, we
	 * have to shut down the device again here.  Normally, though,
	 * fw_node_event will be responsible for shutting it down when
	 * necessary.  We have to use the atomic cmpxchg here to avoid
	 * racing with the FW_NODE_DESTROYED case in
	 * fw_node_event().
	 */
	if (atomic_cmpxchg(&device->state,
		    FW_DEVICE_INITIALIZING,
		    FW_DEVICE_RUNNING) == FW_DEVICE_SHUTDOWN)
		fw_device_shutdown(&device->work.work);
	else
		fw_notify("created new fw device %s (%d config rom retries)\n",
			  device->device.bus_id, device->config_rom_retries);

	/*
	 * Reschedule the IRM work if we just finished reading the
	 * root node config rom.  If this races with a bus reset we
	 * just end up running the IRM work a couple of extra times -
	 * pretty harmless.
	 */
	if (device->node == device->card->root_node)
		schedule_delayed_work(&device->card->work, 0);

	return;

 error_with_cdev:
	down_write(&idr_rwsem);
	idr_remove(&fw_device_idr, minor);
	up_write(&idr_rwsem);
 error:
	put_device(&device->device);
}

static int update_unit(struct device *dev, void *data)
{
	struct fw_unit *unit = fw_unit(dev);
	struct fw_driver *driver = (struct fw_driver *)dev->driver;

	if (is_fw_unit(dev) && driver != NULL && driver->update != NULL) {
		down(&dev->sem);
		driver->update(unit);
		up(&dev->sem);
	}

	return 0;
}

static void fw_device_update(struct work_struct *work)
{
	struct fw_device *device =
		container_of(work, struct fw_device, work.work);

	fw_device_cdev_update(device);
	device_for_each_child(&device->device, NULL, update_unit);
}

void fw_node_event(struct fw_card *card, struct fw_node *node, int event)
{
	struct fw_device *device;

	switch (event) {
	case FW_NODE_CREATED:
	case FW_NODE_LINK_ON:
		if (!node->link_on)
			break;

		device = kzalloc(sizeof(*device), GFP_ATOMIC);
		if (device == NULL)
			break;

		/*
		 * Do minimal intialization of the device here, the
		 * rest will happen in fw_device_init().  We need the
		 * card and node so we can read the config rom and we
		 * need to do device_initialize() now so
		 * device_for_each_child() in FW_NODE_UPDATED is
		 * doesn't freak out.
		 */
		device_initialize(&device->device);
		atomic_set(&device->state, FW_DEVICE_INITIALIZING);
		device->card = fw_card_get(card);
		device->node = fw_node_get(node);
		device->node_id = node->node_id;
		device->generation = card->generation;
		INIT_LIST_HEAD(&device->client_list);

		/*
		 * Set the node data to point back to this device so
		 * FW_NODE_UPDATED callbacks can update the node_id
		 * and generation for the device.
		 */
		node->data = device;

		/*
		 * Many devices are slow to respond after bus resets,
		 * especially if they are bus powered and go through
		 * power-up after getting plugged in.  We schedule the
		 * first config rom scan half a second after bus reset.
		 */
		INIT_DELAYED_WORK(&device->work, fw_device_init);
		schedule_delayed_work(&device->work, INITIAL_DELAY);
		break;

	case FW_NODE_UPDATED:
		if (!node->link_on || node->data == NULL)
			break;

		device = node->data;
		device->node_id = node->node_id;
		device->generation = card->generation;
		if (atomic_read(&device->state) == FW_DEVICE_RUNNING) {
			PREPARE_DELAYED_WORK(&device->work, fw_device_update);
			schedule_delayed_work(&device->work, 0);
		}
		break;

	case FW_NODE_DESTROYED:
	case FW_NODE_LINK_OFF:
		if (!node->data)
			break;

		/*
		 * Destroy the device associated with the node.  There
		 * are two cases here: either the device is fully
		 * initialized (FW_DEVICE_RUNNING) or we're in the
		 * process of reading its config rom
		 * (FW_DEVICE_INITIALIZING).  If it is fully
		 * initialized we can reuse device->work to schedule a
		 * full fw_device_shutdown().  If not, there's work
		 * scheduled to read it's config rom, and we just put
		 * the device in shutdown state to have that code fail
		 * to create the device.
		 */
		device = node->data;
		if (atomic_xchg(&device->state,
				FW_DEVICE_SHUTDOWN) == FW_DEVICE_RUNNING) {
			PREPARE_DELAYED_WORK(&device->work, fw_device_shutdown);
			schedule_delayed_work(&device->work, 0);
		}
		break;
	}
}
