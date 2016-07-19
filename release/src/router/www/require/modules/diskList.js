define([], function(){
	var diskList = {
		list: function(){
			var usb_port_storage_status = [<% usb_port_stor_act(); %>][0];
			<% available_disk_names_and_sizes(); %>
			<% disk_pool_mapping_info(); %>
			<% get_printer_info(); %>
			<% get_modem_info(); %>

			var decodeURIComponentSafe = function(_ascii){
				try{return decodeURIComponent(_ascii);}
				catch(err){return _ascii;}
			}

			var initialValue = {
				"usbPortMax" :  '<% nvram_get("rc_support"); %>'.charAt('<% nvram_get("rc_support"); %>'.indexOf("usbX")+4),
				"apps_dev" : '<% nvram_get("apps_dev"); %>',
				"tm_device_name" : '<% nvram_get("tm_device_name"); %>',
				"apps_fsck_ret" : [<% apps_fsck_ret(); %>][0],
				"allUsbStatusArray" : [<% show_usb_path(); %>][0]
			};

			/* Add the internal SD card reader to the existing USB ports */
			if (based_modelid == "RT-N66U") initialValue.usbPortMax++;

			var newDisk = function(){
				this.usbPath = "";
				this.deviceIndex = "";
				this.node = "";
				this.manufacturer = "";
				this.deviceName = "";
				this.deviceType = "";
				this.totalSize = "";
				this.totalUsed = "";
				this.mountNumber = "";
				this.partNumber = "";
				this.serialNum = "";
				this.hasErrPart = false;
				this.hasAppDev = false;
				this.hasTM = false;
				this.isBusy = false;
				this.partition = new Array(0);
			}

			var newPartition = function(){
				this.partName = "";
				this.mountPoint = "";
				this.isAppDev = false;
				this.isTM = false;
				this.fsck = "";
				this.size = "";
				this.used = "";
				this.format = "unknown";
				this.status = "unmounted";
			}

			var allPartIndex = 0;
			var usbDevicesListUpdated = [];

			// storage
			for(var i=0; i<foreign_disk_interface_names().length; i++){
				if(foreign_disk_interface_names()[i].charAt(0) > initialValue.usbPortMax) continue;

				var tmpDisk = new newDisk();
				tmpDisk.usbPath = foreign_disk_interface_names()[i].charAt(0);
				tmpDisk.deviceIndex = i;
				tmpDisk.node = foreign_disk_interface_names()[i];
				tmpDisk.deviceName = decodeURIComponentSafe(foreign_disks()[i]);
				tmpDisk.deviceType = "storage";
				tmpDisk.mountNumber = foreign_disk_total_mounted_number()[i];
				tmpDisk.partNumber = (foreign_disk_pool_number()[i] == 0) ? 1 : parseInt(foreign_disk_pool_number()[i]);
				tmpDisk.isBusy = (usb_port_storage_status[i] == 1) ? true : false;

				var partIndex = 0;
				while (partIndex < tmpDisk.partNumber && allPartIndex < pool_devices().length){
					var tmpParts = new newPartition();
					tmpParts.partName = pool_names()[allPartIndex];
					tmpParts.mountPoint = pool_devices()[allPartIndex];
					if(tmpParts.mountPoint == initialValue.apps_dev){
						tmpParts.isAppDev = true;
						tmpDisk.hasAppDev = true;
					}
					if(tmpParts.mountPoint == initialValue.tm_device_name){
						tmpParts.isTM = true;
						tmpDisk.hasTM = true;
					}
					tmpParts.size = parseInt(pool_kilobytes()[allPartIndex]);
					tmpParts.used = parseInt(pool_kilobytes_in_use()[allPartIndex]);
					tmpParts.format = pool_types()[allPartIndex];
					tmpParts.status = pool_status()[allPartIndex];
					if(initialValue.apps_fsck_ret.length > 0) {
						tmpParts.fsck = initialValue.apps_fsck_ret[allPartIndex][1];
						if(initialValue.apps_fsck_ret[allPartIndex][1] == 1){
							tmpDisk.hasErrPart = true;
						}
					}

					tmpDisk.partition.push(tmpParts);
					tmpDisk.totalSize = parseInt(tmpDisk.totalSize + tmpParts.size);
					tmpDisk.totalUsed = parseInt(tmpDisk.totalUsed + tmpParts.used);

					partIndex++;
					allPartIndex++;
				}

				if(tmpDisk.deviceName != "") usbDevicesListUpdated.push(tmpDisk);
			}

			// modem and printer
			for(var i=0; i<initialValue.allUsbStatusArray.length; i++){
				for(var j=0; j<initialValue.allUsbStatusArray[i].length; j++){
					if(initialValue.allUsbStatusArray[i][j][0].charAt(0) > initialValue.usbPortMax) continue;

					if(initialValue.allUsbStatusArray[i][j].join().search("storage") == -1){
						tmpDisk = new newDisk();
						tmpDisk.usbPath = initialValue.allUsbStatusArray[i][j][0].charAt(0);
						tmpDisk.deviceIndex = usbDevicesListUpdated.length;
						tmpDisk.node = initialValue.allUsbStatusArray[i][j][0];
						tmpDisk.deviceType = initialValue.allUsbStatusArray[i][j][1];
						
						if(tmpDisk.deviceType == "printer" && typeof printer_pool != "undefined"){
							var idx = printer_pool().getIndexByValue(tmpDisk.node)
							if(idx == -1) continue;

							tmpDisk.manufacturer = printer_manufacturers()[idx];
							tmpDisk.deviceName = tmpDisk.manufacturer + " " + printer_models()[idx].replace(tmpDisk.manufacturer, "");
							tmpDisk.serialNum = printer_serialn()[idx];
						}
						else if(tmpDisk.deviceType == "modem" && typeof modem_pool != "undefined" ){
							var idx = modem_pool().getIndexByValue(tmpDisk.node)
							if(idx == -1) continue;

							tmpDisk.manufacturer = modem_manufacturers()[idx];
							tmpDisk.deviceName = tmpDisk.manufacturer + " " + modem_models()[idx].replace(tmpDisk.manufacturer, "");
							tmpDisk.serialNum = modem_serialn()[idx];
						}

						if(tmpDisk.deviceName != "") usbDevicesListUpdated.push(tmpDisk);
					}
				}	
			}

			return usbDevicesListUpdated;
		}
	}

	return diskList;
});
