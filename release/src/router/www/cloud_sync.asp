<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/aidisk/AiDisk_folder_tree.js"></script>
<style type="text/css">
/* folder tree */
.mask_bg{
	position:absolute;	
	margin:auto;
	top:0;
	left:0;
	width:100%;
	height:100%;
	z-index:100;
	background:url(/images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	overflow:hidden;
}
.mask_floder_bg{
	position:absolute;	
	margin:auto;
	top:0;
	left:0;
	width:100%;
	height:100%;
	z-index:300;
	background:url(/images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	overflow:hidden;
}
.folderClicked{
	color:#569AC7;
	font-size:14px;
	cursor:pointer;
}
.lastfolderClicked{
	color:#FFFFFF;
	cursor:pointer;
}
#status_gif_Img_L{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -0px; width: 59px; height: 38px;
}
#status_gif_Img_LR{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -47px; width: 59px; height: 38px;
}
#status_gif_Img_R{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -97px; width: 59px; height: 38px;
}

#status_png_Img_error{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -0px; width: 59px; height: 38px;
}
#status_png_Img_L_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -47px; width: 59px; height: 38px;
}
#status_png_Img_R_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -95px; width: 59px; height: 38px;
}
#status_png_Img_LR_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -142px; width: 59px; height: 38px;
}
</style>
<script>
var $j = jQuery.noConflict();
<% login_state_hook(); %>
<% get_AiDisk_status(); %>
<% disk_pool_mapping_info(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';


var cloud_status = "";
var cloud_obj = "";
var cloud_msg = "";
var curRule = -1;
var enable_cloudsync = '<% nvram_get("enable_cloudsync"); %>';
<% UI_cloud_status(); %>
var cloud_sync = '<% nvram_show_chinese_char("cloud_sync"); %>';
/* type>user>password>url>rule>dir>enable */
var cloud_synclist_array = cloud_sync.replace(/>/g, "&#62").replace(/</g, "&#60"); 
var cloud_synclist_all = new Array(); 
var isEdit = 0;
var isonEdit = 0;
var maxrulenum = 1;
var rulenum = 1;
var disk_flag=0;
var FromObject = "0";
var lastClickedObj = 0;
var _layer_order = "";
var isNotIE = (navigator.userAgent.search("MSIE") == -1); 
var PROTOCOL = "cifs";
window.onresize = cal_panel_block;

function initial(){
	show_menu();
	showAddTable();
	showcloud_synclist();
	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
}

function initial_dir(){
	var __layer_order = "0_0";
	var url = "/getfoldertree.asp";
	var type = "General";

	url += "?motion=gettree&layer_order=" + __layer_order + "&t=" + Math.random();
	$j.get(url,function(data){initial_dir_status(data);});
}

function initial_dir_status(data){
	if(data != ""){
		eval("var default_dir=" + data);
		document.form.cloud_dir.value = "/mnt/" + default_dir.substr(0, default_dir.indexOf("#")) + "/MySyncFolder";
	}
	else{	
		$("noUSB").style.display = "";
		disk_flag=1;
	}
}

function addRow(obj, head){
	if(head == 1)
		cloud_synclist_array += "&#62";
	else
		cloud_synclist_array += "&#60";
			
	cloud_synclist_array += obj.value;
	obj.value = "";
}

function addRow_Group(upper){ 
	var rule_num = $('cloud_synclist_table').rows.length;
	var item_num = $('cloud_synclist_table').rows[0].cells.length;
	
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}							
	Do_addRow_Group();		
}

function Do_addRow_Group(){		
	addRow(document.form.cloud_sync_type ,1);
	addRow(document.form.cloud_sync_username, 0);
	addRow(document.form.cloud_sync_password, 0);
	addRow(document.form.cloud_sync_dir, 0);
	addRow(document.form.cloud_sync_rule, 0);
	showcloud_synclist();
}

function edit_Row(r){ 	
	document.form.cloud_username.value = cloud_synclist_all[r][1];
	document.form.cloud_password.value = cloud_synclist_all[r][2];
	document.form.cloud_dir.value = cloud_synclist_all[r][5].substring(4);
	document.form.cloud_rule.value = cloud_synclist_all[r][4];
}

function del_Row(r){
	if(isonEdit == 1)
		return false;

	var cloud_synclist_row = cloud_synclist_array.split('&#60'); // resample
	var i=r.parentNode.parentNode.rowIndex+1;
	var cloud_synclist_value = "";
	for(k=1; k<cloud_synclist_row.length; k++){
		if(k == i)
			continue;
		else
			cloud_synclist_value += "&#60";

		var cloud_synclist_col = cloud_synclist_row[k].split('&#62');
		for(j=0; j<cloud_synclist_col.length-1; j++){
			cloud_synclist_value += cloud_synclist_col[j];
			if(j != cloud_synclist_col.length-1)
				cloud_synclist_value += "&#62";
		}
	}
	isonEdit = 1;
	cloud_synclist_array = cloud_synclist_value;
	showcloud_synclist();
	document.form.cloud_sync.value = cloud_synclist_array.replace(/&#62/g, ">").replace(/&#60/g, "<");
	$("update_scan").style.display = '';
	FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
	showLoading();
	document.form.submit();
}

function showcloud_synclist(){
	if(cloud_synclist_array != ""){
		$("creatBtn").style.display = "none";
	}

	rulenum = 0;
	var cloud_synclist_row = cloud_synclist_array.split('&#60');
	var code = "";

	code +='<table width="99%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cloud_synclist_table">';
	if(enable_cloudsync == '0')
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#nosmart_sync#></td>';
	else if(cloud_synclist_array == "")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 0; i < cloud_synclist_row.length; i++){
			rulenum++;
			code +='<tr id="row'+i+'" height="55px">';
			var cloud_synclist_col = cloud_synclist_row[i].split('&#62');
			cloud_synclist_all[i] = cloud_synclist_col;
			var wid = [10, 25, 0, 0, 10, 30, 15];
			for(var j = 0; j < cloud_synclist_col.length; j++){
				if(j == 2 || j == 3){
					continue;
				}
				else{
					if(j == 0)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/ASUS-WebStorage.png"></td>';
					else if(j == 1)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><span style="font-size:16px;font-family: Calibri;font-weight: bolder;text-decoration:underline; cursor:pointer"  onclick="isEdit=1;showAddTable();">'+ cloud_synclist_col[j] +'</span></td>';
					else if(j == 4){
						curRule = cloud_synclist_col[j];
						if(cloud_synclist_col[j] == 2)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div id="status_gif_Img_L"></div></div></td>';//code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img id="statusImg" width="45px" src="/images/cloudsync/left.gif"></td>';
						else if(cloud_synclist_col[j] == 1)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div id="status_gif_Img_R"></div></div></td>';
						else
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div id="status_gif_Img_LR"></div></div></td>';
					}
					else if(j == 6){
						code +='<td width="'+wid[j]+'%" id="cloudStatus"></td>';
					}
					else{
						code +='<td width="'+wid[j]+'%"><span style="display:none;">'+ cloud_synclist_col[j] +'</span><span style="word-break:break-all;">'+ cloud_synclist_col[j].substr(4, cloud_synclist_col[j].length) +'</span></td>';
					}
				}
			}
			code +='<td width="10%"><input class="remove_btn" onclick="del_Row(this);" value=""/></td>';
		}
		updateCloudStatus();
	}
	code +='</table>';
	$("cloud_synclist_Block").innerHTML = code;
}

function updateCloudStatus(){
    $j.ajax({
    	url: '/cloud_status.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		updateCloudStatus();
    	},
    	success: function(response){
					if(cloud_status.toUpperCase() == "DOWNUP"){
						cloud_status = "SYNC";
						$("status_image").firstChild.id="status_gif_Img_LR";
						//$("statusImg").src = "/images/cloudsync/left_right.gif";
					}
					else if(cloud_status.toUpperCase() == "ERROR"){
						$("status_image").firstChild.id="status_png_Img_error";
						//$("statusImg").src = "/images/cloudsync/left_right_Error.png";
					}
					else if(cloud_status.toUpperCase() == "UPLOAD"){
						$("status_image").firstChild.id="status_gif_Img_L";
						//$("statusImg").src = "/images/cloudsync/left.gif";
					}
					else if(cloud_status.toUpperCase() == "DOWNLOAD"){
						$("status_image").firstChild.id="status_gif_Img_R";
						//$("statusImg").src = "/images/cloudsync/right.gif";
					}
					else if(cloud_status.toUpperCase() == "SYNC"){
						cloud_status = "Finish";
						if(curRule == 2){
							$("status_image").firstChild.id="status_png_Img_L_ok";
							//$("statusImg").src = "/images/cloudsync/left_ok.png";
						}else if(curRule == 1){
							$("status_image").firstChild.id="status_png_Img_R_ok";
							//$("statusImg").src = "/images/cloudsync/right_ok.png";
						}else{
							$("status_image").firstChild.id="status_png_Img_LR_ok";
							//$("statusImg").src = "/images/cloudsync/left_right_ok.png";
						}	
					}

					// handle msg
					var _cloud_msg = "";
					if(cloud_obj != ""){
						_cloud_msg +=  "<b>";
						_cloud_msg += cloud_status;
						_cloud_msg += ": </b><br />";
						_cloud_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponent(cloud_obj) + "</span>";
					}
					else if(cloud_msg){
						_cloud_msg += cloud_msg;
					}
					else{
						_cloud_msg += "No log.";
					}

					// handle status
					var _cloud_status;
					if(cloud_status != "")
						_cloud_status = cloud_status;
					else
						_cloud_status = "";

					if($("cloudStatus"))
						$("cloudStatus").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_msg +'\');">'+ _cloud_status +'</div>';

			 		setTimeout("updateCloudStatus();", 2000);
      }
   });
}

function validform(){
	if(!Block_chars(document.form.cloud_username, ["<", ">"]))
		return false;

	if(document.form.cloud_username.value == ''){
		alert("<#File_Pop_content_alert_desc1#>");
		return false;
	}

	if(!Block_chars(document.form.cloud_password, ["<", ">"]))
		return false;

	if(document.form.cloud_password.value == ''){
		alert("<#File_Pop_content_alert_desc6#>");
		return false;
	}

	if(document.form.cloud_dir.value.split("/").length < 4 || document.form.cloud_dir.value == ''){
		alert("<#ALERT_OF_ERROR_Input10#>");
		return false;
	}
	return true;
}

function applyRule(){
	if(validform()){
		isonEdit = 1;
		// 1 rule.
		cloud_synclist_array = '0&#62'+document.form.cloud_username.value+'&#62'+document.form.cloud_password.value+'&#62none&#62'+document.form.cloud_rule.value+'&#62'+"/tmp"+document.form.cloud_dir.value+'&#621';
		showcloud_synclist();
		document.form.cloud_sync.value = cloud_synclist_array.replace(/&#62/g, ">").replace(/&#60/g, "<");
		document.form.cloud_username.value = '';
		document.form.cloud_password.value = '';
		document.form.cloud_rule.value = '';
		document.form.cloud_dir.value = '';
		document.form.enable_cloudsync.value = 1;
		isEdit = 0;
		showAddTable();
		$("update_scan").style.display = '';
		FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
		showLoading();
		document.form.submit();
	}
}

function showAddTable(){
	if(isEdit == 1){ // edit
		$j("#cloudAddTable").fadeIn();
		$("creatBtn").style.display = "none";
		$j("#applyBtn").fadeIn();
		if(cloud_synclist_array != ""){
			edit_Row(0);
		}
	}
	else{ // list
		$("cloudAddTable").style.display = "none";
		if(cloud_synclist_array == ""){
			$j("#creatBtn").fadeIn();
			$("applyBtn").style.display = "none";
		}
		else{
			$("creatBtn").style.display = "none";
			$("applyBtn").style.display = "none";			
		}
	}
}

// get folder tree
var folderlist = new Array();
function get_disk_tree(){
	if(disk_flag == 1){
		alert('<#no_usb_found#>');
		return false;	
	}
	cal_panel_block();
	$j("#folderTree_panel").fadeIn(300);
	get_layer_items("0");
}
function get_layer_items(layer_order){
	$j.ajax({
    		url: '/gettree.asp?layer_order='+layer_order,
    		dataType: 'script',
    		error: function(xhr){
    			;
    		},
    		success: function(){
				get_tree_items(treeitems);					
  			}
		});
}
function get_tree_items(treeitems){
	document.aidiskForm.test_flag.value = 0;
	this.isLoading = 1;
	var array_temp = new Array();
	var array_temp_split = new Array();
	for(var j=0;j<treeitems.length;j++){ // To hide folder 'Download2' & 'asusware'
		array_temp_split[j] = treeitems[j].split("#");
		if( array_temp_split[j][0].match(/^Download2$/) || array_temp_split[j][0].match(/^asusware$/)	){
			continue;					
		}
		
		array_temp.push(treeitems[j]);
	}
	this.Items = array_temp;
	if(this.Items && this.Items.length >= 0){
		BuildTree();
	}	
}
function BuildTree(){
	var ItemText, ItemSub, ItemIcon;
	var vertline, isSubTree;
	var layer;
	var short_ItemText = "";
	var shown_ItemText = "";
	var ItemBarCode ="";
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
		
		if(layer == 2 && isSubTree == 'n1'){	// Uee to rebuild folder tree if disk without folder, Jieming add at 2012/08/29
			document.aidiskForm.test_flag.value = 1;			
		}
		
		TempObject +='<table class="tree_table" id="bug_test">';
		TempObject +='<tr>';
		// the line in the front.
		TempObject +='<td class="vert_line">';
		TempObject +='<img id="a'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' class="FdRead" src="/images/Tree/vert_line_'+isSubTree+'0.gif">';
		TempObject +='</td>';
	
		if(layer == 3){
			/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/
			TempObject +='<td>';		
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
			TempObject +='</td>';
			TempObject +='<td>';
			TempObject +='<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>';
			TempObject +='</td>';
		}
		else if(layer == 2){
			TempObject +='<td>';
			TempObject +='<table class="tree_table">';
			TempObject +='<tr>';
			TempObject +='<td class="vert_line">';
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
			TempObject +='</td>';
			TempObject +='<td class="FdText">';
			TempObject +='<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>';
			TempObject +='</td>\n';
			TempObject +='<td></td>';
			TempObject +='</tr>';
			TempObject +='</table>';
			TempObject +='</td>';
			TempObject +='</tr>';
			TempObject +='<tr><td></td>';
			TempObject +='<td colspan=2><div id="e'+ItemBarCode+'" ></div></td>';
		}
		else{
			/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/
			TempObject +='<td>';
			TempObject +='<table><tr><td>';
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
			TempObject +='</td><td>';
			TempObject +='<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>';
			TempObject +='</td></tr></table>';
			TempObject +='</td>';
			TempObject +='</tr>';
			TempObject +='<tr><td></td>';
			TempObject +='<td><div id="e'+ItemBarCode+'" ></div></td>';
		}
		TempObject +='</tr>';
	}
	TempObject +='</table>';
	$("e"+this.FromObject).innerHTML = TempObject;
}

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
function build_array(obj,layer){
	var path_temp ="/mnt";
	var layer2_path ="";
	var layer3_path ="";
	if(obj.id.length>6){
		if(layer ==3){
			layer3_path = "/" + $(obj.id).innerHTML;
			while(layer3_path.indexOf("&nbsp;") != -1)
				layer3_path = layer3_path.replace("&nbsp;"," ");
				
			if(obj.id.length >8)
				layer2_path = "/" + $(obj.id.substring(0,obj.id.length-3)).innerHTML;
			else
				layer2_path = "/" + $(obj.id.substring(0,obj.id.length-2)).innerHTML;
			
			while(layer2_path.indexOf("&nbsp;") != -1)
				layer2_path = layer2_path.replace("&nbsp;"," ");
		}
	}
	if(obj.id.length>4 && obj.id.length<=6){
		if(layer ==2){
			layer2_path = "/" + $(obj.id).innerHTML;
			while(layer2_path.indexOf("&nbsp;") != -1)
				layer2_path = layer2_path.replace("&nbsp;"," ");
		}
	}
	path_temp = path_temp + layer2_path +layer3_path;
	return path_temp;
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
		path_directory = build_array(selectedObj,layer);
		$('createFolderBtn').src = "/images/New_ui/advancesetting/FolderAdd.png";
		$('deleteFolderBtn').src = "/images/New_ui/advancesetting/FolderDel.png";
		$('modifyFolderBtn').src = "/images/New_ui/advancesetting/FolderMod.png";
		$('createFolderBtn').onclick = function(){};
		$('deleteFolderBtn').onclick = function(){};
		$('modifyFolderBtn').onclick = function(){};
	}
	else if(layer == 2){
		// chose Partition
		setSelectedPoolOrder(selectedObj.id);
		path_directory = build_array(selectedObj,layer);
		$('createFolderBtn').src = "/images/New_ui/advancesetting/FolderAdd_0.png";
		$('deleteFolderBtn').src = "/images/New_ui/advancesetting/FolderDel.png";
		$('modifyFolderBtn').src = "/images/New_ui/advancesetting/FolderMod.png";
		$('createFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popCreateFolder.asp');};		
		$('deleteFolderBtn').onclick = function(){};
		$('modifyFolderBtn').onclick = function(){};
		document.aidiskForm.layer_order.disabled = "disabled";
		document.aidiskForm.layer_order.value = barcode;
	}
	else if(layer == 3){
		// chose Shared-Folder
		setSelectedFolderOrder(selectedObj.id);
		path_directory = build_array(selectedObj,layer);
		$('createFolderBtn').src = "/images/New_ui/advancesetting/FolderAdd.png";
		$('deleteFolderBtn').src = "/images/New_ui/advancesetting/FolderDel_0.png";
		$('modifyFolderBtn').src = "/images/New_ui/advancesetting/FolderMod_0.png";
		$('createFolderBtn').onclick = function(){};		
		$('deleteFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popDeleteFolder.asp');};
		$('modifyFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popModifyFolder.asp');};
		document.aidiskForm.layer_order.disabled = "disabled";
		document.aidiskForm.layer_order.value = barcode;
	}

	if(haveSubTree)
		GetTree(barcode, 1);
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
		$('d'+layer_order).innerHTML = '<span class="FdWait">. . . . . . . . . .</span>';
		setTimeout('get_layer_items("'+layer_order+'", "gettree")', 1);		
		return;
	}
	
	if($('a'+layer_order).className == "FdRead"){
		$('a'+layer_order).className = "FdOpen";
		$('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"1.gif";		
		this.FromObject = layer_order;		
		$('e'+layer_order).innerHTML = '<img src="/images/Tree/folder_wait.gif">';
		setTimeout('get_layer_items("'+layer_order+'", "gettree")', 1);
	}
	else if($('a'+layer_order).className == "FdOpen"){
		$('a'+layer_order).className = "FdClose";
		$('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"0.gif";		
		$('e'+layer_order).style.position = "absolute";
		$('e'+layer_order).style.visibility = "hidden";
	}
	else if($('a'+layer_order).className == "FdClose"){
		$('a'+layer_order).className = "FdOpen";
		$('a'+layer_order).src = "/images/Tree/vert_line_s"+v+"1.gif";		
		$('e'+layer_order).style.position = "";
		$('e'+layer_order).style.visibility = "";
	}
	else
		alert("Error when show the folder-tree!");
}

function cancel_folderTree(){
	this.FromObject ="0";
	$j("#folderTree_panel").fadeOut(300);
}

function confirm_folderTree(){
	$('PATH').value = path_directory ;
	this.FromObject ="0";
	$j("#folderTree_panel").fadeOut(300);
}

function switchType(_method){
	return 0;

	if(isNotIE){
		document.form.cloud_password.type = _method ? "text" : "password";		
	}
}

function switchType_IE(obj){
	return 0;
	if(isNotIE) return;		
	
	var tmp = "";
	tmp = obj.value;
	if(obj.id.indexOf('text') < 0){		//password
		obj.style.display = "none";
		document.getElementById('cloud_password_text').style.display = "";
		document.getElementById('cloud_password_text').value = tmp;						
		document.getElementById('cloud_password_text').focus();
	}else{														//text					
		obj.style.display = "none";
		document.getElementById('cloud_password').style.display = "";
		document.getElementById('cloud_password').value = tmp;
	}
}

function cal_panel_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.25)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	

	}
	$("folderTree_panel").style.marginLeft = blockmarginLeft+"px";
}
</script>
</head>

<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>

<!-- floder tree-->
<div id="DM_mask" class="mask_bg"></div>
<div id="folderTree_panel" class="panel_folder" >
	<table><tr><td>
		<div class="machineName" style="width:200px;font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:15px;margin-left:30px;"><#Web_Title2#></div>
		</td>
		<td>
			<div style="width:240px;margin-top:15px;margin-left:125px;">
				<img id="createFolderBtn" src="/images/New_ui/advancesetting/FolderAdd.png" hspace="1" title="<#AddFolderTitle#>" onclick="">
				<img id="deleteFolderBtn" src="/images/New_ui/advancesetting/FolderDel.png" hspace="1" title="<#DelFolderTitle#>" onclick="">
				<img id="modifyFolderBtn" src="/images/New_ui/advancesetting/FolderMod.png" hspace="1" title="<#ModFolderTitle#>" onclick="">
			</div>
		</td></tr></table>
		<div id="e0" class="folder_tree"></div>
	<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">		
		<input class="button_gen" type="button" style="margin-left:27%;margin-top:18px;" onclick="cancel_folderTree();" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button"  onclick="confirm_folderTree();" value="<#CTL_ok#>">
	</div>
</div>
<div id="DM_mask_floder" class="mask_floder_bg"></div>
<!-- floder tree-->

<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_sync.asp">
<input type="hidden" name="next_page" value="cloud_sync.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_cloudsync">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="cloud_sync" value="">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<a href="cloud_main.asp"><div class="tab"><span>AiCloud</span></div></a>
						</td>
						<td>
							<div class="tabclick"><span>Smart Sync</span></div>
						</td>
						<td>
							<a href="cloud_settings.asp"><div class="tab"><span>Settings</span></div></a>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab"><span>Log</span></div></a>
						</td>
					</tr>
					</tbody>
				</table>
			</div>

		<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tbody>
						<tr>
						  <td bgcolor="#4D595D" valign="top">

						<div>&nbsp;</div>
						<div class="formfonttitle">AiCloud - Smart Sync</div>
						<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

						<div>
							<table width="700px" style="margin-left:25px;">
								<tr>
									<td>
										<img id="guest_image" src="/images/cloudsync/003.png" style="margin-top:10px;margin-left:-20px">
										<div align="center" class="left" style="margin-top:25px;margin-left:43px;width:94px; float:left; cursor:pointer;" id="radio_smartSync_enable"></div>
										<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
										<script type="text/javascript">
											$j('#radio_smartSync_enable').iphoneSwitch('<% nvram_get("enable_cloudsync"); %>',
												function() {
													document.enableform.enable_cloudsync.value = 1;
													showLoading();	
													document.enableform.submit();
												},
												function() {
													document.enableform.enable_cloudsync.value = 0;
													showLoading();	
													document.enableform.submit();	
												},
												{
													switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
												}
											);
										</script>			
										</div>

									</td>
									<td>&nbsp;&nbsp;</td>
									<td>
										<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:normal;">
												<#smart_sync1#><br />
												<#smart_sync2#>											
										</div>
									</td>
								</tr>
							</table>
						</div>

   					<table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="cloudlistTable" style="margin-top:30px;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud List</td>
	   					</tr>
	  					</thead>		  

    					<tr>
      					<th width="10%"><!--a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,2);"-->Provider<!--/a--></th>
    						<th width="25%"><#PPPConnection_UserName_itemname#></a></th>
      					<th width="10%">Rule</a></th>
      					<th width="30%">Folder</th>
      					<th width="15%">Status</th>
      					<th width="10%"><#CTL_del#></th>
    					</tr>

						</table>
	
						<div id="cloud_synclist_Block"></div>

					  <table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="cloudAddTable" style="margin-top:10px;display:none;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud List</td>
	   					</tr>
	  					</thead>		  

							<tr>
							<th width="30%" style="height:40px;font-family: Calibri;font-weight: bolder;">
								Provider
							</th>
							<td>
								<div><img style="margin-top: -2px;" src="/images/cloudsync/ASUS-WebStorage.png"></div>
								<div style="font-size:18px;font-weight: bolder;margin-left: 45px;margin-top: -27px;font-family: Calibri;">ASUS WebStorage</div>
							</td>
							</tr>
				            
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#AiDisk_Account#>
							</th>			
							<td>
							  <input type="text" maxlength="32" class="input_32_table" style="height: 25px;" id="cloud_username" name="cloud_username" value="" onKeyPress="">
							</td>
						  </tr>	

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#PPPConnection_Password_itemname#>
							</th>			
							<td>
								<input id="cloud_password" name="cloud_password" type="text" autocapitalization="off" onBlur="switchType(false);" onFocus="switchType(true);switchType_IE(this);" maxlength="32" class="input_32_table" style="height: 25px;" value="">
							  <input id="cloud_password_text" name="cloud_password_text" type="text" autocapitalization="off" onBlur="switchType_IE(this);" maxlength="32" class="input_32_table" style="height:25px; display:none;" value="">
							</td>
						  </tr>						  				
					  				
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Folder
							</th>
							<td>
			          <input type="text" id="PATH" class="input_32_table" style="height: 25px;" name="cloud_dir" value="" onclick=""/>
		  					<input name="button" type="button" class="button_gen_short" onclick="get_disk_tree();" value="Browser"/>
								<div id="noUSB" style="color:#FC0;display:none;margin-left: 3px;"><#no_usb_found#></div>
							</td>
						  </tr>

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Rule
							</th>
							<td>
								<select name="cloud_rule" class="input_option">
									<option value="0">Sync</option>
									<option value="1">Download to USB Disk</option>
									<option value="2">Upload to Cloud</option>
								</select>			
							</td>
						  </tr>
						</table>	
					
   					<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="cloudmessageTable" style="margin-top:7px;display:none;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud Message</td>
	   					</tr>
	  					</thead>		  
							<tr>
								<td>
									<textarea style="width:99%; border:0px; font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;" cols="63" rows="25" readonly="readonly" wrap=off ></textarea>
								</td>
							</tr>
						</table>

	  				<div class="apply_gen" id="creatBtn" style="margin-top:30px;display:none;">
							<input name="applybutton" id="applybutton" type="button" class="button_gen_long" onclick="isEdit=1;showAddTable();" value="<#AddAccountTitle#>" style="word-wrap:break-word;word-break:normal;">
							<img id="update_scan" style="display:none;" src="images/InternetScan.gif" />
	  				</div>

	  				<div class="apply_gen" style="margin-top:30px;display:none;" id="applyBtn">
	  					<input name="button" type="button" class="button_gen" onclick="isEdit=0;showAddTable();" value="<#CTL_Cancel#>"/>
	  					<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
	  				</div>

					  </td>
					</tr>				
					</tbody>	
				  </table>		
	
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>
<form method="post" name="enableform" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_sync.asp">
<input type="hidden" name="next_page" value="cloud_sync.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_cloudsync">
<input type="hidden" name="action_wait" value="2">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
</form>
<form method="post" name="aidiskForm" action="" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="test_flag" value="" disabled="disabled">
<input type="hidden" name="protocol" id="protocol" value="">
</form>
<!-- mask for disabling AiDisk -->
<div id="OverlayMask" class="popup_bg">
	<div align="center">
	<iframe src="" frameborder="0" scrolling="no" id="popupframe" width="400" height="400" allowtransparency="true" style="margin-top:150px;"></iframe>
	</div>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
</body>
</html>
