/*
 *  Host Resources 'wrapper' interface
 *	calls the per-group interfaces from 'hr_*.h'
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

	config_require(host/hr_system)
	config_require(host/hr_device)
	config_require(host/hr_other)
	config_require(host/hr_proc)
	config_require(host/hr_network)
	config_require(host/hr_print)
	config_require(host/hr_disk)
	config_require(host/hr_partition)

	config_version_require((host/hr_storage, 5.7, host/hrh_storage))
	config_version_require((host/hr_filesys, 5.7, host/hrh_filesys))

	config_version_require((host/hr_swinst,  5.6, host/hrSWInstalledTable))
	config_version_require((host/hr_swrun,   5.6, host/hrSWRunTable))

/* add the host resources mib to the default mibs to load */
config_add_mib(HOST-RESOURCES-MIB)
config_add_mib(HOST-RESOURCES-TYPES)

/*********************************************
 *
 *   A few words about the design of the Host Resources
 *     implementation - particularly as regards the hrDevice
 *     group and hrDeviceIndex.  This (and hrStorageIndex) make 
 *     use of the non-consecutive nature of SNMP instance identifiers.
 *
 *   hrDeviceIndex is structured in a 'major/minor' form,
 *     with the high end indicating the type of device
 *     (following the enumerations of hrDeviceType) and the low
 *     end being used to differentiate between devices of that type.
 *
 *   The implementation of walking through the available devices
 *     uses a pair of arrays of functions - indexed by hrDeviceType
 *     These are used to perform any initialisation needed for that
 *     type of device, and to step through the instances of that type. 
 *   This latter 'get_next' routing returns the hrDeviceIndex (including
 *     the hrDeviceType major number), or -1 if there are no further
 *     instances of that type.
 *   When all devices of a particular type have been processed, the
 *     initialisation function for the next device type is called,
 *     and so on until all devices have been proceesed.
 *   
 *   Similar arrays are used to provide type-specific functions to
 *     return the "common" device information (description, ID, status
 *     and error count), and to save any internal structures needed
 *     to provide these.
 *   A final array is used to indicate whether hrDeviceIndexes are
 *     returned in a random order, or strictly increasing.  In the
 *     latter case, this allows the search for a particular index to
 *     terminate as soon as the 'next' device is reached, without needing
 *     to check the rest of them.  Similarly, once a particular type of
 *     device has been scanned, further types will not be examined unless
 *     a suitable index has not yet been found.
 *
 *   The index used for hrFSIndex is also used as hrStorageIndex,
 *     for those storage areas corresponding to filestore.
 *     Additional storage areas (such as memory or swap space) are
 *     distinguished by index values greater than a defined constant.
 *     Currently these are individually defined entries, but other
 *     ranges could be implemented similarly.
 *   If hrFSIndex was re-implemented to reflect internal identifiers,
 *     it would be possible to reverse the sense of the current
 *     implementation, with non-filestore storage having indices
 *     less than a defined constant, rather than greater.
 *
 *
 *   Much of the detailed implementation of this group (as opposed to
 *     the implementation infrastructure outlined about) is likely to
 *     be very system-specific.
 *   The initial implementation (for HP-UX 9 and Linux) should be
 *     regarded as a 'proof of concept' example, rather than as
 *     finished, releasable code.  This particularly hold for the
 *     disk device discovery code, which is gross in the extreme,
 *     and should never have seen the light of day!
 *   Hopefully this can be ripped out and given a quiet burial as
 *     soon as is decently possible.  
 *
 *   Now it's up to the rest of you to hammer this into some sort of
 *     sensible shape.
 *                                Dave Shield
 *   
 *********************************************/
