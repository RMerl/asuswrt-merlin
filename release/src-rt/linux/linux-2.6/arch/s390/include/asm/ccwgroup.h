#ifndef S390_CCWGROUP_H
#define S390_CCWGROUP_H

struct ccw_device;
struct ccw_driver;

/**
 * struct ccwgroup_device - ccw group device
 * @creator_id: unique number of the driver
 * @state: online/offline state
 * @count: number of attached slave devices
 * @dev: embedded device structure
 * @cdev: variable number of slave devices, allocated as needed
 */
struct ccwgroup_device {
	unsigned long creator_id;
	enum {
		CCWGROUP_OFFLINE,
		CCWGROUP_ONLINE,
	} state;
/* private: */
	atomic_t onoff;
	struct mutex reg_mutex;
/* public: */
	unsigned int count;
	struct device	dev;
	struct ccw_device *cdev[0];
};

/**
 * struct ccwgroup_driver - driver for ccw group devices
 * @max_slaves: maximum number of slave devices
 * @driver_id: unique id
 * @probe: function called on probe
 * @remove: function called on remove
 * @set_online: function called when device is set online
 * @set_offline: function called when device is set offline
 * @shutdown: function called when device is shut down
 * @prepare: prepare for pm state transition
 * @complete: undo work done in @prepare
 * @freeze: callback for freezing during hibernation snapshotting
 * @thaw: undo work done in @freeze
 * @restore: callback for restoring after hibernation
 * @driver: embedded driver structure
 */
struct ccwgroup_driver {
	int max_slaves;
	unsigned long driver_id;

	int (*probe) (struct ccwgroup_device *);
	void (*remove) (struct ccwgroup_device *);
	int (*set_online) (struct ccwgroup_device *);
	int (*set_offline) (struct ccwgroup_device *);
	void (*shutdown)(struct ccwgroup_device *);
	int (*prepare) (struct ccwgroup_device *);
	void (*complete) (struct ccwgroup_device *);
	int (*freeze)(struct ccwgroup_device *);
	int (*thaw) (struct ccwgroup_device *);
	int (*restore)(struct ccwgroup_device *);

	struct device_driver driver;
};

extern int  ccwgroup_driver_register   (struct ccwgroup_driver *cdriver);
extern void ccwgroup_driver_unregister (struct ccwgroup_driver *cdriver);
int ccwgroup_create_from_string(struct device *root, unsigned int creator_id,
				struct ccw_driver *cdrv, int num_devices,
				const char *buf);

extern int ccwgroup_probe_ccwdev(struct ccw_device *cdev);
extern void ccwgroup_remove_ccwdev(struct ccw_device *cdev);

#define to_ccwgroupdev(x) container_of((x), struct ccwgroup_device, dev)
#define to_ccwgroupdrv(x) container_of((x), struct ccwgroup_driver, driver)
#endif
