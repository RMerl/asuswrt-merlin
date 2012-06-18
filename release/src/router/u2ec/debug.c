/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include "typeconvert.h"
#include "usbsock.h"
#include "wdm-MJMN.h"
#include "usbdi.h"
#include "usb.h"

#if 0
char *PnpQueryIdString(BUS_QUERY_ID_TYPE Type)
{
	switch (Type)
	{
	case BusQueryDeviceID:
		return "BusQueryDeviceID";
	case BusQueryHardwareIDs:
		return "BusQueryHardwareIDs";
	case BusQueryCompatibleIDs:
		return "BusQueryCompatibleIDs";
	case BusQueryInstanceID:
		return "BusQueryInstanceID";
	case BusQueryDeviceSerialNumber:
		return "BusQueryDeviceSerialNumber";
	}
	return "Unknow";
}
#endif

char *MinorFunctionString (PIRP_SAVE pirp)
{
	switch (pirp->StackLocation.MinorFunction)
    {
	case IRP_MN_START_DEVICE:
	    return "IRP_MN_START_DEVICE";
	case IRP_MN_QUERY_REMOVE_DEVICE:
	    return "IRP_MN_QUERY_REMOVE_DEVICE";
	case IRP_MN_REMOVE_DEVICE:
	    return "IRP_MN_REMOVE_DEVICE";
	case IRP_MN_CANCEL_REMOVE_DEVICE:
	    return "IRP_MN_CANCEL_REMOVE_DEVICE";
	case IRP_MN_STOP_DEVICE:
	    return "IRP_MN_STOP_DEVICE";
	case IRP_MN_QUERY_STOP_DEVICE:
	    return "IRP_MN_QUERY_STOP_DEVICE";
	case IRP_MN_CANCEL_STOP_DEVICE:
	    return "IRP_MN_CANCEL_STOP_DEVICE";
	case IRP_MN_QUERY_DEVICE_RELATIONS:
	    return "IRP_MN_QUERY_DEVICE_RELATIONS";
	case IRP_MN_QUERY_INTERFACE:
	    return "IRP_MN_QUERY_INTERFACE";
	case IRP_MN_QUERY_CAPABILITIES:
	    return "IRP_MN_QUERY_CAPABILITIES";
	case IRP_MN_QUERY_RESOURCES:
	    return "IRP_MN_QUERY_RESOURCES";
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
	    return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
	case IRP_MN_QUERY_DEVICE_TEXT:
	    return "IRP_MN_QUERY_DEVICE_TEXT";
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
	    return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
	case IRP_MN_READ_CONFIG:
	    return "IRP_MN_READ_CONFIG";
	case IRP_MN_WRITE_CONFIG:
	    return "IRP_MN_WRITE_CONFIG";
	case IRP_MN_EJECT:
	    return "IRP_MN_EJECT";
	case IRP_MN_SET_LOCK:
	    return "IRP_MN_SET_LOCK";
	case IRP_MN_QUERY_ID:
	    return "IRP_MN_QUERY_ID";
	case IRP_MN_QUERY_PNP_DEVICE_STATE:
	    return "IRP_MN_QUERY_PNP_DEVICE_STATE";
	case IRP_MN_QUERY_BUS_INFORMATION:
	    return "IRP_MN_QUERY_BUS_INFORMATION";
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
	    return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
	case IRP_MN_SURPRISE_REMOVAL:
	    return "IRP_MN_SURPRISE_REMOVAL";

	default:
	    return "unknown_pnp_irp";
    }
	return 0;
}

char *MajorFunctionString (PIRP_SAVE irp)
{
    switch (irp->StackLocation.MajorFunction)
    {
		case IRP_MJ_CREATE:
			return "IRP_MJ_CREATE";
		case IRP_MJ_CREATE_NAMED_PIPE:
			return "IRP_MJ_CREATE_NAMED_PIPE";
		case IRP_MJ_CLOSE:
			return "IRP_MJ_CLOSE";
		case IRP_MJ_READ:
			return "IRP_MJ_READ";
		case IRP_MJ_WRITE:
			return "IRP_MJ_WRITE";
		case IRP_MJ_QUERY_INFORMATION:
			return "IRP_MJ_QUERY_INFORMATION";
		case IRP_MJ_SET_INFORMATION:
			return "IRP_MJ_SET_INFORMATION";
		case IRP_MJ_QUERY_EA:
			return "IRP_MJ_QUERY_EA";
		case IRP_MJ_SET_EA:
			return "IRP_MJ_SET_EA";
		case IRP_MJ_FLUSH_BUFFERS:
			return "IRP_MJ_FLUSH_BUFFERS";
		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			return "IRP_MJ_QUERY_VOLUME_INFORMATION";
		case IRP_MJ_SET_VOLUME_INFORMATION:
			return "IRP_MJ_SET_VOLUME_INFORMATION";
		case IRP_MJ_DIRECTORY_CONTROL:
			return "IRP_MJ_DIRECTORY_CONTROL";
		case IRP_MJ_FILE_SYSTEM_CONTROL:
			return "IRP_MJ_FILE_SYSTEM_CONTROL";
		case IRP_MJ_DEVICE_CONTROL:
			return "IRP_MJ_DEVICE_CONTROL";
		case IRP_MJ_INTERNAL_DEVICE_CONTROL:
			return "IRP_MJ_INTERNAL_DEVICE_CONTROL";
		case IRP_MJ_SHUTDOWN:
			return "IRP_MJ_SHUTDOWN";
		case IRP_MJ_LOCK_CONTROL:
			return "IRP_MJ_LOCK_CONTROL";
		case IRP_MJ_CLEANUP:
			return "IRP_MJ_CLEANUP";
		case IRP_MJ_CREATE_MAILSLOT:
			return "IRP_MJ_CREATE_MAILSLOT";
		case IRP_MJ_QUERY_SECURITY:
			return "IRP_MJ_QUERY_SECURITY";
		case IRP_MJ_SET_SECURITY:
			return "IRP_MJ_SET_SECURITY";
		case IRP_MJ_POWER:
			return "IRP_MJ_POWER";
		case IRP_MJ_SYSTEM_CONTROL:
			return "IRP_MJ_SYSTEM_CONTROL";
		case IRP_MJ_DEVICE_CHANGE:
			return "IRP_MJ_DEVICE_CHANGE";
		case IRP_MJ_QUERY_QUOTA:
			return "IRP_MJ_QUERY_QUOTA";
		case IRP_MJ_SET_QUOTA:
			return "IRP_MJ_SET_QUOTA";
		case IRP_MJ_PNP:
			return "IRP_MJ_PNP";
    }

	return "unknown";
}

#if 0
// ***********************************************************
//					Debug function
// ***********************************************************
char *PowerMinorFunctionString (unsigned char MinorFunction)
{
    switch (MinorFunction)
    {
	case IRP_MN_SET_POWER:
	    return "IRP_MN_SET_POWER";
	case IRP_MN_QUERY_POWER:
	    return "IRP_MN_QUERY_POWER";
	case IRP_MN_POWER_SEQUENCE:
	    return "IRP_MN_POWER_SEQUENCE";
	case IRP_MN_WAIT_WAKE:
	    return "IRP_MN_WAIT_WAKE";
	default:
	    return "unknown_power_irp";
    }
}

char *SystemPowerString(char *Type)
{
    switch (Type)
    {
	case PowerSystemUnspecified:
	    return "PowerSystemUnspecified";
	case PowerSystemWorking:
	    return "PowerSystemWorking";
	case PowerSystemSleeping1:
	    return "PowerSystemSleeping1";
	case PowerSystemSleeping2:
	    return "PowerSystemSleeping2";
	case PowerSystemSleeping3:
	    return "PowerSystemSleeping3";
	case PowerSystemHibernate:
	    return "PowerSystemHibernate";
	case PowerSystemShutdown:
	    return "PowerSystemShutdown";
	case PowerSystemMaximum:
	    return "PowerSystemMaximum";
	default:
	    return "UnKnown System Power State";
    }
}

char *DevicePowerString(unsigned char Type)
{
    switch (Type)
    {
	case PowerDeviceUnspecified:
	    return "PowerDeviceUnspecified";
	case PowerDeviceD0:
	    return "PowerDeviceD0";
	case PowerDeviceD1:
	    return "PowerDeviceD1";
	case PowerDeviceD2:
	    return "PowerDeviceD2";
	case PowerDeviceD3:
	    return "PowerDeviceD3";
	case PowerDeviceMaximum:
	    return "PowerDeviceMaximum";
	default:
	    return "UnKnown Device Power State";
    }
	return 0;
}
#endif
