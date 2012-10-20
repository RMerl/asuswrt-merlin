var selectedDiskOrder = -1;
var selectedPoolOrder = -1;
var selectedFolderOrder = -1;

var selectedDiskBarcode = "";
var selectedPoolBarcode = "";
var selectedFolderBarcode = "";

function get_layer(barcode){
	var tmp, layer;
	
	layer = 0;
	while(barcode.indexOf('_') != -1){
		barcode = barcode.substring(barcode.indexOf('_'), barcode.length);
		++layer;
		barcode = barcode.substring(1);
	}
	
	return layer;
}

function getDiskBarcode(src_barcode){
	var layer = get_layer(src_barcode);
	var str_len, tmp_str;
	
	if(layer < 1)
		return "";
	else if(layer == 1)
		return src_barcode;
	
	str_len = src_barcode.indexOf('_');
	tmp_str = src_barcode.substring(str_len+1);
	
	str_len += tmp_str.indexOf('_')+1;
	
	return src_barcode.substring(0, str_len);
}

function getPoolBarcode(src_barcode){
	var layer = get_layer(src_barcode);
	var str_len, tmp_str;
	
	if(layer < 2)
		return "";
	else if(layer == 2)
		return src_barcode;
	
	str_len = getDiskBarcode(src_barcode).length;
	tmp_str = src_barcode.substring(str_len+1);
	
	str_len += tmp_str.indexOf('_')+1;
	
	return src_barcode.substring(0, str_len);
}

function getFolderBarcode(src_barcode){
	var layer = get_layer(src_barcode);
	var str_len, tmp_str;
	
	if(layer < 3)
		return "";
	else if(layer == 3)
		return src_barcode;
	
	str_len = getPoolBarcode(src_barcode).length;
	tmp_str = src_barcode.substring(str_len+1);
	
	str_len += tmp_str.indexOf('_')+1;
	
	return src_barcode.substring(0, str_len);
}

function getDiskOrder(disk_barcode){
	return parseInt(disk_barcode.substring(disk_barcode.indexOf('_')+1));
}

function getPoolOrder(pool_barcode){
	return parseInt(pool_barcode.substring(getDiskBarcode(pool_barcode).length+1));
}

function getFolderOrder(folder_barcode){
	return parseInt(folder_barcode.substring(getPoolBarcode(folder_barcode).length+1));
}

function setSelectedDiskOrder(selectedId){
	this.selectedDiskBarcode = getDiskBarcode(selectedId.substring(1));
	this.selectedPoolBarcode = "";
	this.selectedFolderBarcode = "";
	
	this.selectedDiskOrder = getDiskOrder(this.selectedDiskBarcode);
	this.selectedPoolOrder = -1;
	this.selectedFolderOrder = -1;
}

function setSelectedPoolOrder(selectedId){
	this.selectedDiskBarcode = getDiskBarcode(selectedId.substring(1));
	this.selectedPoolBarcode = getPoolBarcode(selectedId.substring(1));
	this.selectedFolderBarcode = "";
	
	this.selectedDiskOrder = getDiskOrder(this.selectedDiskBarcode);
	this.selectedPoolOrder = this.selectedDiskOrder+getPoolOrder(this.selectedPoolBarcode);
	this.selectedFolderOrder = -1;
}

function setSelectedFolderOrder(selectedId){
	this.selectedDiskBarcode = getDiskBarcode(selectedId.substring(1));
	this.selectedPoolBarcode = getPoolBarcode(selectedId.substring(1));
	this.selectedFolderBarcode = getFolderBarcode(selectedId.substring(1));
	
	this.selectedDiskOrder = getDiskOrder(this.selectedDiskBarcode);
	this.selectedPoolOrder = this.selectedDiskOrder+getPoolOrder(this.selectedPoolBarcode);
	this.selectedFolderOrder = 1+getFolderOrder(this.selectedFolderBarcode);
}

function getSelectedDiskOrder(){
	return this.selectedDiskOrder;
}

function getSelectedPoolOrder(){
	return this.selectedPoolOrder;
}

function getSelectedFolderOrder(){
	return this.selectedFolderOrder;
}

function getDiskIDfromOtherID(objID){
	var disk_id_pos, disk_id;
	
	disk_id_pos = objID.indexOf("_", 3);
	disk_id = objID.substring(0, disk_id_pos);
	
	return disk_id;
}

function getPoolIDfromOtherID(objID){
	var part_id_pos, part_id;
	
	part_id_pos = objID.lastIndexOf("_");
	part_id = objID.substring(0, part_id_pos);
	
	return part_id;
}

function getShareIDfromOtherID(objID){
	var share_id_pos, share_id;
	
	share_id_pos = objID.lastIndexOf("_")+1;
	share_id = objID.substring(share_id_pos);
	
	return share_id;
}

function computepools(diskorder, flag){
	var pools = new Array();
	var pools_size = new Array();
	var pools_available = new Array();
	var pools_type = new Array();
	var pools_size_in_use = new Array();
	
	for(var i = 0; i < pool_devices().length; ++i){
		if(per_pane_pool_usage_kilobytes(i, diskorder)[0] && per_pane_pool_usage_kilobytes(i, diskorder)[0] > 0){
			pools[pools.length] = pool_devices()[i];
			pools_size[pools_size.length] = per_pane_pool_usage_kilobytes(i, diskorder)[0];
			pools_available[pools_available.length] = per_pane_pool_usage_kilobytes(i, diskorder)[0]-pool_kilobytes_in_use()[i];
			pools_type[pools_type.length] = pool_types()[i];
			pools_size_in_use[pools_size_in_use.length] = pool_kilobytes_in_use()[i];
		}
	}

	if(flag == "name") return pools;
	if(flag == "size") return pools_size;
	if(flag == "available") return pools_available;
	if(flag == "type") return pools_type;
	if(flag == "size_in_use") return pools_size_in_use;
}

function computeallpools(all_disk_order, flag){
	var pool_array = computepools(all_disk_order, flag);
	var total_size = 0;
	
	if(all_disk_order >= foreign_disks().length)
		return getDiskTotalSize(all_disk_order);
	
	for(var i = 0; i < pool_array.length; ++i)
		total_size += pool_array[i];

	return simpleNum(total_size);
}

function getDiskMountedNum(all_disk_order){
	if(all_disk_order < foreign_disks().length)
		return foreign_disk_total_mounted_number()[all_disk_order];
	else
		return 0;
}

function getDiskName(all_disk_order){
	var disk_name;
	
	if(all_disk_order < foreign_disks().length)
		disk_name = decodeURIComponent(foreign_disks()[all_disk_order]);
	else
		disk_name = blank_disks()[all_disk_order-foreign_disks().length];
	
	return disk_name;
}

function getDiskPort(all_disk_order){
	var disk_port;
	
	if(all_disk_order < foreign_disks().length)
		disk_port = foreign_disk_interface_names()[all_disk_order];
	else
		disk_port = blank_disk_interface_names()[all_disk_order-foreign_disks().length];
	
	return disk_port;
}

function getDiskModelName(all_disk_order){
	var disk_model_name;
	
	if(all_disk_order < foreign_disks().length)
		disk_model_name = decodeURIComponent(foreign_disk_model_info()[all_disk_order]);
	else
		disk_model_name = blank_disk_model_info()[all_disk_order-foreign_disk_model_info().length];
		
	return disk_model_name;
}
/*
function getDiskTotalSize(all_disk_order){
	var TotalSize = 0;

	if(foreign_disks().length > 1){   //Lock add 2009.05.14 for N13U Rev.B1: some disk will format to 2 sub disk.
		for(var i=0; i<foreign_disk_total_size().length; i++){
			TotalSize = TotalSize + simpleNum(foreign_disk_total_size()[i]);
		}
	}	
	else if(all_disk_order < foreign_disks().length)
		TotalSize = simpleNum(foreign_disk_total_size()[all_disk_order]);
	else
		TotalSize = simpleNum(blank_disk_total_size()[all_disk_order-foreign_disk_total_size().length]);
		
	return TotalSize;
}*/

function getDiskTotalSize(all_disk_order){
	var TotalSize;
	
	if(all_disk_order < foreign_disks().length)
		TotalSize = simpleNum(foreign_disk_total_size()[all_disk_order]);
	else
		TotalSize = simpleNum(blank_disk_total_size()[all_disk_order-foreign_disk_total_size().length]);
		
	return TotalSize;
}
