var selectedDiskOrder = -1;
var selectedPoolOrder = -1;
var selectedFolderOrder = -1;
var selectedDiskBarcode = "";
var selectedPoolBarcode = "";
var selectedFolderBarcode = "";

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

var usbDevicesList = [];
require(['/require/modules/diskList.js'], function(diskList){
	usbDevicesList = diskList.list();
});

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
	this.selectedPoolOrder = getPoolOrder(this.selectedPoolBarcode);
	this.selectedFolderOrder = -1;
}

function setSelectedFolderOrder(selectedId){
	this.selectedDiskBarcode = getDiskBarcode(selectedId.substring(1));
	this.selectedPoolBarcode = getPoolBarcode(selectedId.substring(1));
	this.selectedFolderBarcode = getFolderBarcode(selectedId.substring(1));

	this.selectedDiskOrder = getDiskOrder(this.selectedDiskBarcode);
	this.selectedPoolOrder = getPoolOrder(this.selectedPoolBarcode);
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

// =================================== Copy from /aidisk/AiDisk_folder_tree.js =========================================

var list_share_or_folder = 1; // 0: share, 1: folder.
var isLoading = 0;
var FromObject = "0";
var Items = -1;
var lastClickedObj = 0;
var clickedFolderBarCode = new Array();	// clickedFolderBarCode[poolName][folderName] = folderBarCode

function popupWindow(w,u){
	disableCheckChangedStatus();
	
	winW_H();
	
	document.getElementById(w).style.width = winW+"px";
	document.getElementById(w).style.height = winH+"px";
	document.getElementById(w).style.visibility = "visible";
	
	document.getElementById('popupframe').src = u;
}

function hidePop(flag){
	if(flag != "apply")
		enableCheckChangedStatus();

	setTimeout(function(){
		document.getElementById("popupframe").src = "";
	}, 100);

	document.getElementById('OverlayMask').style.visibility = "hidden";
}

function GetFolderItem(selectedObj, haveSubTree){
	var barcode, layer = 0;
	
	showClickedObj(selectedObj);
	
	barcode = selectedObj.id.substring(1);
	layer = get_layer(barcode);
	
	if(layer == 0)
		alert("Machine: Wrong");
	else if(layer == 1){
		// chose Disk
		setSelectedDiskOrder(selectedObj.id);
		onEvent();
	}
	else if(layer == 2){
		// chose Partition
		setSelectedPoolOrder(selectedObj.id);
		onEvent();
	}
	else if(layer == 3){
		// chose Shared-Folder
		setSelectedFolderOrder(selectedObj.id);
		onEvent();
		showApplyBtn();
	}
	
	if(haveSubTree)
		GetTree(barcode, 1);
}

function getSelectedStatusOfPool(pool){
	var status = "";
	
	outer_loop:
		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				if(usbDevicesList[i].partition[j].mountPoint == pool){
					status = usbDevicesList[i].partition[j].status;
					break outer_loop;
				}
			}
		}
	
	return status;
}

function showClickedObj(clickedObj){
	if(this.lastClickedObj != 0)
		this.lastClickedObj.className = "lastfolderClicked";  //this className set in AiDisk_style.css
	
	clickedObj.className = "folderClicked";

	this.lastClickedObj = clickedObj;
}

function GetTree(layer_order, v){
	if(layer_order == "0"){
		this.FromObject = layer_order;
		document.getElementById('d'+layer_order).innerHTML = '<span class="FdWait">. . . . . . . . . .</span>';
		setTimeout('get_layer_items("'+layer_order+'", "gettree")', 1);
		
		return;
	}
	
	if(document.getElementById('a'+layer_order).className == "FdRead"){
		document.getElementById('a'+layer_order).className = "FdOpen";
		document.getElementById('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"1.gif";
		
		this.FromObject = layer_order;
		
		document.getElementById('e'+layer_order).innerHTML = '<img src="/images/Tree/folder_wait.gif">';
		setTimeout('get_layer_items("'+layer_order+'", "gettree")', 1);
	}
	else if(document.getElementById('a'+layer_order).className == "FdOpen"){
		document.getElementById('a'+layer_order).className = "FdClose";
		document.getElementById('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"0.gif";
		
		document.getElementById('e'+layer_order).style.position = "absolute";
		document.getElementById('e'+layer_order).style.visibility = "hidden";
	}
	else if(document.getElementById('a'+layer_order).className == "FdClose"){
		document.getElementById('a'+layer_order).className = "FdOpen";
		document.getElementById('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"1.gif";
		
		document.getElementById('e'+layer_order).style.position = "";
		document.getElementById('e'+layer_order).style.visibility = "";
	}
	else
		alert("Error when show the folder-tree!");
}

function get_layer_items(new_layer_order, motion){
	if(list_share_or_folder == 1)
		document.aidiskForm.action = "/aidisk/getfolderarray.asp";
	else
		document.aidiskForm.action = "/aidisk/getsharearray.asp";
	
	document.getElementById("motion").value = motion;
	document.getElementById("layer_order").value = new_layer_order;
	document.aidiskForm.submit();
}

function get_tree_items(treeitems, motion){
	this.isLoading = 1;
	this.Items = treeitems;
	
	if(motion == "lookup")
		;
	else if(motion == "gettree"){
		if(this.Items && this.Items.length > 0){
			BuildTree();
		}
	}
}

function BuildTree(){
	var ItemText, ItemBarCode, ItemSub, ItemIcon;
	var vertline, isSubTree;
	var layer;
	var shown_permission = "";
	var short_ItemText = "";
	var shown_ItemText = "";
		
	var TempObject = "";
	
	for(var i = 0; i < this.Items.length; ++i){
		this.Items[i] = this.Items[i].split("#");
		
		var Item_size = 0;
		Item_size = this.Items[i].length;
		if(Item_size > 3){
			var temp_array = new Array(3);
			
			temp_array[2] = this.Items[i][Item_size-1];
			temp_array[1] = this.Items[i][Item_size-2];
			
			temp_array[0] = "";
			for(var j = 0; j < Item_size-2; ++j){
				if(j != 0)
					temp_array[0] += "#";
				temp_array[0] += this.Items[i][j];
			}
			this.Items[i] = temp_array;
		}
		
		ItemText = (this.Items[i][0]).replace(/^[\s]+/gi,"").replace(/[\s]+$/gi,"");
		ItemBarCode = this.FromObject+"_"+(this.Items[i][1]).replace(/^[\s]+/gi,"").replace(/[\s]+$/gi,"");
		ItemSub = parseInt((this.Items[i][2]).replace(/^[\s]+/gi,"").replace(/[\s]+$/gi,""));

		layer = get_layer(ItemBarCode.substring(1));
		
		if(layer == 3){
			if(ItemText.length > 21)
		 		short_ItemText = ItemText.substring(0,18)+"...";
		 	else
		 		short_ItemText = ItemText;
		}
		else
			short_ItemText = ItemText;
		
		shown_ItemText = showhtmlspace(short_ItemText);
		
		if(layer == 1)
			ItemIcon = 'disk';
		else if(layer == 2)
			ItemIcon = 'part';
		else
			ItemIcon = 'folders';
		
		SubClick = ' onclick="GetFolderItem(this, ';
		if(ItemSub <= 0){
			SubClick += '0);"';
			isSubTree = 'n';
		}
		else{
			SubClick += '1);"';
			isSubTree = 's';
		}
		
		if(i == this.Items.length-1){
			vertline = '';
			isSubTree += '1';
		}
		else{
			vertline = ' background="/images/Tree/vert_line.gif"';
			isSubTree += '0';
		}
		
		TempObject += 
'<table class="tree_table">\n'+
'<tr>\n'+
	// the line in the front.
	'<td class="vert_line">\n'+ 
		'<img id="a'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' class="FdRead" src="/images/Tree/vert_line_'+isSubTree+'0.gif">\n'+
	'</td>\n';
	
		if(layer == 3){
			TempObject += 		/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/
	'<td>\n'+
		'<div id="b'+ItemBarCode+'" class="FdText">\n'+		/* style="float:left; width:117px; overflow:hidden;"*/
			'<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">\n'+
			'<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>\n'+
		'</div>\n'+
	'</td>\n'+
	
	'<td><div id=\"f'+ItemBarCode+'" class="FileStatus" onclick="getChangedPermission(this);"></div></td>\n\n';
		}
		else if(layer == 2){
			TempObject += 
	'<td>\n';
			
			TempObject += 
'<table class="tree_table">\n'+
'<tr>\n';
			
			TempObject += 
	'<td class="vert_line">\n'+
			'<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">\n'+
	'</td>\n'+
	'<td class="FdText">\n'+
			'<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>\n'+
	'</td>\n';
			
			/*if(PROTOCOL == "cifs")
				TempObject += 
	'<td><div id=\"f'+ItemBarCode+'" class="FileStatus" onclick="getChangedPermission(this);"></div></td>\n\n';
			else//*/
				TempObject += 
	'<td></td>';
			
			TempObject += 
'</tr>\n'+
'</table>\n';
			
			TempObject += 
	'</td>\n'+
'</tr>\n';
			
			TempObject += 
'<tr><td></td>\n';
			
			TempObject += 
	'<td colspan=2><div id="e'+ItemBarCode+'" ></div></td>\n';
		}
		else{
			TempObject += 		/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/
	'<td>\n'+
		'<div id="b'+ItemBarCode+'" class="FdText">\n'+		/* style="float:left; width:117px; overflow:hidden;"*/
			'<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">\n'+
			'<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>\n'+
		'</div>\n'+
	'</td>\n'+
'</tr>\n';
			
			TempObject += 
'<tr><td></td>\n';
			
			TempObject += 
	'<td><div id="e'+ItemBarCode+'" ></div></td>\n';
		}
		
		TempObject += 
'</tr>\n';
	}
	
	TempObject += 
'</table>\n';
	
	document.getElementById("e"+this.FromObject).innerHTML = TempObject;
	
	// additional object
	if(layer == 3){
		for(var i = 0; i < this.Items.length; ++i){
			ItemText = (this.Items[i][0]).replace(/^[\s]+/gi,"").replace(/[\s]+$/gi,"");
			ItemBarCode = this.FromObject+"_"+(this.Items[i][1]).replace(/^[\s]+/gi,"").replace(/[\s]+$/gi,"");
			
			// record the barcode of the shown folder
			var usbPartitionMountPoint = usbDevicesList[getSelectedDiskOrder()].partition[getSelectedPoolOrder()].mountPoint;

			add_folderBarCode_list(usbPartitionMountPoint, ItemText, ItemBarCode);

			// decide if show the permission out
			if(this.selectedAccount != null && this.selectedAccount.length > 0)
				shown_permission = get_permission_of_folder(this.selectedAccount, usbPartitionMountPoint, ItemText, PROTOCOL);
			else
				shown_permission = 3;
			
			showPermissionRadio(ItemBarCode, shown_permission);
		}
	}
}

// 0: Share mode, 1: Account mode.
function get_manage_type(proto){
	if(proto == "cifs"){
		if(this.AM_to_cifs == 2 || this.AM_to_cifs == 4)
			return 1;
		else
			return 0;
	}
	else if(proto == "ftp"){ // SMB
		if(this.AM_to_ftp == 2)
			return 1;
		else
			return 0;
	}
	else if(proto == "webdav"){ // WEBDAV
		if(this.AM_to_webdav == 2)
			return 1;
		else
			return 0;
	}
	else
		alert("<#ALERT_OF_ERROR_Input2#>");
}

function getPoolDevice(barCode){
	var layer = get_layer(barCode);
	
	if(layer < 2)
		return "";
	
	
}

function showPermissionRadio(barCode, permission){
	var code = "";
	var layer = get_layer(barCode);
	var PoolBarCode, PoolDevice, PoolStatus;
	
	PoolBarCode = getPoolBarcode(barCode);
	PoolDevice = usbDevicesList[getSelectedDiskOrder()].partition[getSelectedPoolOrder()].mountPoint;
	PoolStatus = getSelectedStatusOfPool(PoolDevice);
	
	if(layer == 3
			//|| (layer == 2 && get_manage_type(PROTOCOL) == 1)
			){
		code += '<input type="radio" name="g'+barCode+'" value="3"';
		if(permission == 3)
			code += ' checked';
		else if(PROTOCOL == "cifs" && permission == 2)
			code += ' checked';
		else if(PROTOCOL == "webdav" && permission == 2)
			code += ' checked';
		
		if(PoolStatus != "rw"
				//|| (PROTOCOL == "ftp" && get_manage_type(PROTOCOL) == 0)
				|| get_manage_type(PROTOCOL) == 0
				)
			code += ' disabled';
		
		code += '>';
	}
	else
		code += '<span>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>\n';
	
	if(PROTOCOL == "ftp"){
		code += '<input type="radio" name="g'+barCode+'" value="2"';
		if(permission == 2)
			code += ' checked';
		
		if(PoolStatus != "rw"
			|| get_manage_type(PROTOCOL) == 0)
		code += ' disabled';
		
		code += '>';
	}
	else
		code += '<span>&nbsp;&nbsp;&nbsp;</span>\n';
	
	code += '<input type="radio" name="g'+barCode+'" value="1"';
	if(permission == 1
			//|| (layer == 2 && permission == 3 && PROTOCOL == "cifs")
			)
		code += ' checked';
	
	if(PoolStatus != "rw"
			//|| (PROTOCOL == "ftp" && get_manage_type(PROTOCOL) == 0)
			|| get_manage_type(PROTOCOL) == 0
			)
		code += ' disabled';
	
	code += '>';
	
	if(PROTOCOL == "cifs" || PROTOCOL == "webdav")
		code += '<span>&nbsp;&nbsp;&nbsp;</span>\n';
	
	code += '<input type="radio" name="g'+barCode+'" value="0"';
	if(permission == 0)
		code += ' checked';
	
	if(PoolStatus != "rw"
			//|| (PROTOCOL == "ftp" && get_manage_type(PROTOCOL) == 0)
			|| get_manage_type(PROTOCOL) == 0
			)
		code += ' disabled';
	
	code += '>';
	document.getElementById("f"+barCode).innerHTML = code;
}

function getChangedPermission(selectedObj){
	var selectedBarCode = selectedObj.id.substring(1);
	var selectedlayer = get_layer(selectedBarCode);
	var selectedPoolDevice, selectedFolder;
	var folderObj = document.getElementById("d"+selectedBarCode);
	var radioName = "g"+selectedBarCode;
	var permission, orig_permission;
	
	if(!this.selectedAccount)
		this.selectedAccount = "guest";

	if(selectedlayer == 2)
		setSelectedPoolOrder(selectedObj.id);
	else
		setSelectedFolderOrder(selectedObj.id);

	selectedPoolDevice = usbDevicesList[getSelectedDiskOrder()].partition[getSelectedPoolOrder()].mountPoint;

	permission = getValueofRadio(radioName);
	if(permission == -1){
		alert("Can't read the permission when change the radio!");	// system error msg. must not be translate
		return;
	}
	
	if(get_manage_type(PROTOCOL) == 0 && selectedlayer == 2 && permission > 0)
		permission = 3;
	
	if(!this.changedPermissions[this.selectedAccount])
		this.changedPermissions[this.selectedAccount] = new Array();
	
	if(!this.changedPermissions[this.selectedAccount][selectedPoolDevice])
		this.changedPermissions[this.selectedAccount][selectedPoolDevice] = new Array();
	
	if(selectedlayer == 2)
		selectedFolder = "";
	else
		selectedFolder = parent.get_sharedfolder_in_pool(selectedPoolDevice)[getSelectedFolderOrder()];
	
	this.changedPermissions[this.selectedAccount][selectedPoolDevice][selectedFolder] = permission;
	this.controlApplyBtn = 1;

	showApplyBtn();
	onEvent();
}

function getValueofRadio(radioName){
	var radioObjs = getElementsByName_iefix("input", radioName);
	var value;
	for(var i = 0; i < radioObjs.length; ++i)
		if(radioObjs[i].checked == true)
			//return parseInt(radioObjs[i].value);
			return radioObjs[i].value;
	
	return -1;
}

function add_folderBarCode_list(poolName, folderName, folderBarCode){
	if(!this.clickedFolderBarCode[poolName]){
		this.clickedFolderBarCode[poolName] = new Array();
	}
	
	this.clickedFolderBarCode[poolName][folderName] = folderBarCode;
}

function get_folderBarCode_in_pool(poolName, folderName){
	if(this.clickedFolderBarCode[poolName])
		if(this.clickedFolderBarCode[poolName][folderName])
			return this.clickedFolderBarCode[poolName][folderName];
	
	return "";
}