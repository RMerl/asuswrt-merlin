

#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "uwb-internal.h"


/**
 * Start/stop scanning in a radio controller
 *
 * @rc:      UWB Radio Controlller
 * @channel: Channel to scan; encodings in WUSB1.0[Table 5.12]
 * @type:    Type of scanning to do.
 * @bpst_offset: value at which to start scanning (if type ==
 *                UWB_SCAN_ONLY_STARTTIME)
 * @returns: 0 if ok, < 0 errno code on error
 *
 * We put the command on kmalloc'ed memory as some arches cannot do
 * USB from the stack. The reply event is copied from an stage buffer,
 * so it can be in the stack. See WUSB1.0[8.6.2.4] for more details.
 */
int uwb_rc_scan(struct uwb_rc *rc,
		unsigned channel, enum uwb_scan_type type,
		unsigned bpst_offset)
{
	int result;
	struct uwb_rc_cmd_scan *cmd;
	struct uwb_rc_evt_confirm reply;

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_kzalloc;
	mutex_lock(&rc->uwb_dev.mutex);
	cmd->rccb.bCommandType = UWB_RC_CET_GENERAL;
	cmd->rccb.wCommand = cpu_to_le16(UWB_RC_CMD_SCAN);
	cmd->bChannelNumber = channel;
	cmd->bScanState = type;
	cmd->wStartTime = cpu_to_le16(bpst_offset);
	reply.rceb.bEventType = UWB_RC_CET_GENERAL;
	reply.rceb.wEvent = UWB_RC_CMD_SCAN;
	result = uwb_rc_cmd(rc, "SCAN", &cmd->rccb, sizeof(*cmd),
			    &reply.rceb, sizeof(reply));
	if (result < 0)
		goto error_cmd;
	if (reply.bResultCode != UWB_RC_RES_SUCCESS) {
		dev_err(&rc->uwb_dev.dev,
			"SCAN: command execution failed: %s (%d)\n",
			uwb_rc_strerror(reply.bResultCode), reply.bResultCode);
		result = -EIO;
		goto error_cmd;
	}
	rc->scanning = channel;
	rc->scan_type = type;
error_cmd:
	mutex_unlock(&rc->uwb_dev.mutex);
	kfree(cmd);
error_kzalloc:
	return result;
}

/*
 * Print scanning state
 */
static ssize_t uwb_rc_scan_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct uwb_dev *uwb_dev = to_uwb_dev(dev);
	struct uwb_rc *rc = uwb_dev->rc;
	ssize_t result;

	mutex_lock(&rc->uwb_dev.mutex);
	result = sprintf(buf, "%d %d\n", rc->scanning, rc->scan_type);
	mutex_unlock(&rc->uwb_dev.mutex);
	return result;
}

/*
 *
 */
static ssize_t uwb_rc_scan_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct uwb_dev *uwb_dev = to_uwb_dev(dev);
	struct uwb_rc *rc = uwb_dev->rc;
	unsigned channel;
	unsigned type;
	unsigned bpst_offset = 0;
	ssize_t result = -EINVAL;

	result = sscanf(buf, "%u %u %u\n", &channel, &type, &bpst_offset);
	if (result >= 2 && type < UWB_SCAN_TOP)
		result = uwb_rc_scan(rc, channel, type, bpst_offset);

	return result < 0 ? result : size;
}

/** Radio Control sysfs interface (declaration) */
DEVICE_ATTR(scan, S_IRUGO | S_IWUSR, uwb_rc_scan_show, uwb_rc_scan_store);
