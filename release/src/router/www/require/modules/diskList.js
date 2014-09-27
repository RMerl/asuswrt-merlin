define(["/jquery.js"], function($j){

	var $j = jQuery.noConflict();
	var diskList = function(){};
	var usbDevicesList = new Array();

	/*hook*/
	<% available_disk_names_and_sizes(); %>
	<% disk_pool_mapping_info(); %>
	<% get_printer_info(); %>
	<% get_modem_info(); %>

	function genUSBDevices(){
		/*initial variable*/
		var initialValue = {
			"usbPortMax" :  '<% nvram_get("rc_support"); %>'.charAt('<% nvram_get("rc_support"); %>'.indexOf("usbX")+4),
			"apps_dev" : '<% nvram_get("apps_dev"); %>',
			"tm_device_name" : '<% nvram_get("tm_device_name"); %>',
			"apps_fsck_ret" : '<% apps_fsck_ret(); %>'.toArray(),
			"allUsbStatusArray" : '<% show_usb_path(); %>'.toArray()
		};
			
		/*usbDeviceList constructor*/
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
			this.serialNum = "";
			this.hasErrPart = false;
			this.hasAppDev = false;
			this.hasTM = false;
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

		var pool_name = new Array();
		var allPartIndex = 0;
		if(typeof pool_devices != "undefined") pool_name = pool_devices();
		
		while(usbDevicesList.length > 0) //initial array
			usbDevicesList.pop();                   

		for(var i=0; i<foreign_disk_interface_names().length; i++){
			if(foreign_disk_interface_names()[i].charAt(0) > initialValue.usbPortMax) continue;

			var tmpDisk = new newDisk();
			tmpDisk.usbPath = foreign_disk_interface_names()[i].charAt(0);
			tmpDisk.deviceIndex = i;
			tmpDisk.node = foreign_disk_interface_names()[i];
			tmpDisk.deviceName = decodeURIComponentSafe(foreign_disks()[i]);
			tmpDisk.deviceType = "storage";
			tmpDisk.mountNumber = foreign_disk_total_mounted_number()[i];

			var _mountedPart = 0;	
			while (_mountedPart < tmpDisk.mountNumber && allPartIndex < pool_name.length){
				if(pool_types()[allPartIndex] != "unknown" || pool_status()[allPartIndex] != "unmounted"){	
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
					_mountedPart++;
				}

				allPartIndex++;
			}

			if(tmpDisk.deviceName != "") usbDevicesList.push(tmpDisk);
		}

		for(var i=0; i<initialValue.allUsbStatusArray.length; i++){
			for(var j=0; j<initialValue.allUsbStatusArray[i].length; j++){
				if(initialValue.allUsbStatusArray[i][j][0].charAt(0) > initialValue.usbPortMax) continue;

				if(initialValue.allUsbStatusArray[i][j].join().search("storage") == -1 || initialValue.allUsbStatusArray[i][j].length == 0){
					tmpDisk = new newDisk();
					tmpDisk.usbPath = initialValue.allUsbStatusArray[i][j][0].charAt(0);
					tmpDisk.deviceIndex = usbDevicesList.length;
					tmpDisk.node = initialValue.allUsbStatusArray[i][j][0];
					tmpDisk.deviceType = initialValue.allUsbStatusArray[i][j][1];
					
					if(tmpDisk.deviceType == "printer" &&  typeof printer_pool != "undefined"){
						var idx = printer_pool().getIndexByValue(tmpDisk.node)
						if(idx == -1)
							continue;

						tmpDisk.manufacturer = printer_manufacturers()[idx];
						tmpDisk.deviceName = tmpDisk.manufacturer + " " + printer_models()[idx].replace(tmpDisk.manufacturer, "");
						tmpDisk.serialNum = printer_serialn()[idx];
					}
					else if(tmpDisk.deviceType == "modem" &&  typeof modem_pool != "undefined" ){
						var idx = modem_pool().getIndexByValue(tmpDisk.node)
						if(idx == -1)
							continue;

						tmpDisk.manufacturer = modem_manufacturers()[idx];
						tmpDisk.deviceName = tmpDisk.manufacturer + " " + modem_models()[idx].replace(tmpDisk.manufacturer, "");
						tmpDisk.serialNum = modem_serialn()[idx];
					}

					if(tmpDisk.deviceName != "") usbDevicesList.push(tmpDisk);
				}
			}	
		}

		return usbDevicesList;
	};

	var mounted_partition_old = 0;
	diskList.prototype = {
		update: function(callback){
			var mounted_partition = 0;
			
			$j.ajax({ 
				url: '/update_diskinfo.asp',
				dataType: 'script',

				success: function(){
					if(typeof window.usbDevicesListUpdated != "undefined"){
						for(i=0; i<window.usbDevicesListUpdated.length; i++){
							if(parseInt(window.usbDevicesListUpdated[i].mountNumber) > 0){
								mounted_partition += parseInt(window.usbDevicesListUpdated[i].mountNumber);
							}
						}

						if(mounted_partition != mounted_partition_old){
							callback();
						}
					}
				}
			}); 
		},

		list: function(){
			return (typeof window.usbDevicesListUpdated != "undefined") ? window.usbDevicesListUpdated : genUSBDevices();
		}
	}

	return new diskList();
});