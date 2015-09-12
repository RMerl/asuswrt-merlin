<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#UPnPMediaServer#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/form.js"></script>
<style type="text/css">
.upnp_table{
	height: 790px;
	width:750px;
	padding:5px; 
	padding-top:20px; 
	margin-top:-17px; 
	position:relative;
	background-color:#4d595d;
	align:left;
	-webkit-border-top-right-radius: 05px;
	-webkit-border-bottom-right-radius: 5px;
	-webkit-border-bottom-left-radius: 5px;
	-moz-border-radius-topright: 05px;
	-moz-border-radius-bottomright: 5px;
	-moz-border-radius-bottomleft: 5px;
	border-top-right-radius: 05px;
	border-bottom-right-radius: 5px;
	border-bottom-left-radius: 5px;
}
.line_export{
	height:20px;
	width:736px;
}
.upnp_icon{
	width:736px;
	height:500px;
	margin-top:15px;
	margin-right:5px;
}
/* folder tree */
.mask_bg{
	position:absolute;	
	margin:auto;
	top:0;
	left:0;
	width:100%;
	height:100%;
	z-index:100;
	/*background-color: #FFF;*/
	background:url(images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	/*visibility:hidden;*/
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
	/*background-color: #FFF;*/
	background:url(images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	/*visibility:hidden;*/
	overflow:hidden;
}
.folderClicked{
	color:#569AC7;
	font-size:14px;
	cursor:text;
}
.lastfolderClicked{
	color:#FFFFFF;
	cursor:pointer;
}
.dlna_path_td{
 padding-left:15px;
 text-align:left;
}
</style>
<script>
var dms_status = <% dms_info(); %>;
var _dms_dir = '<%nvram_get("dms_dir");%>';
<% get_AiDisk_status(); %>
var _layer_order = "";
var FromObject = "0";
var lastClickedObj = 0;
var disk_flag=0;
var PROTOCOL = "cifs";
window.onresize = function() {
	if(document.getElementById("folderTree_panel").style.display == "block") {
		cal_panel_block("folderTree_panel", 0.25);
	}
} 

var dms_dir_x_array = '<% nvram_get("dms_dir_x"); %>';
var dms_dir_type_x_array = '<% nvram_get("dms_dir_type_x"); %>';

function dlna_path_display(){
	if("<% nvram_get("dms_enable"); %>" == 1){
		document.form.dms_friendly_name.parentNode.parentNode.parentNode.style.display = "";
		document.form.dms_dir_manual_x[0].parentNode.parentNode.style.display = "";
		document.getElementById("dmsStatus").parentNode.parentNode.style.display = "";		
		//document.getElementById("dlna_path_div").style.display = "";
		//show_dlna_path();		
		set_dms_dir(document.form.dms_dir_manual);
	}
	else{
		document.form.dms_friendly_name.parentNode.parentNode.parentNode.style.display = "none";
		document.form.dms_dir_manual_x[0].parentNode.parentNode.style.display = "none";
		document.getElementById("dmsStatus").parentNode.parentNode.style.display = "none";		
		document.getElementById("dlna_path_div").style.display = "none";
	}
}

function daapd_display(){
	if("<% nvram_get("daapd_enable"); %>" == 1)
		document.form.daapd_friendly_name.parentNode.parentNode.parentNode.style.display = "";
	else
		document.form.daapd_friendly_name.parentNode.parentNode.parentNode.style.display = "none";
}

function initial(){
	show_menu();
	document.getElementById("_APP_Installation").innerHTML = '<table><tbody><tr><td><div class="_APP_Installation"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	document.getElementById("_APP_Installation").className = "menu_clicked";

	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
	check_dir_path();
	
	daapd_display();
	dlna_path_display();
	do_get_friendly_name("daapd");
	do_get_friendly_name("dms");
	check_dms_status();
	
	if((calculate_height-3)*52 + 20 > 535)
		document.getElementById("upnp_icon").style.height = (calculate_height-3)*52 -70 + "px";
	else
		document.getElementById("upnp_icon").style.height = "500px";
		
	if(noiTunes_support){		
		document.getElementById("iTunes_div").style.display = "none";		
	}
		
}

function check_dms_status(){
	 $.ajax({
    	url: '/ajax_dms_status.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		check_dms_status();
    	},
    	success: function(response){
					if(dms_status[0] != "")
							document.getElementById("dmsStatus").innerHTML = "Scanning..";
					else		
							document.getElementById("dmsStatus").innerHTML = "Idle";
					setTimeout("check_dms_status();",5000);
      }
  });	
}

function initial_dir(){
	var __layer_order = "0_0";
	var url = "/getfoldertree.asp";
	var type = "General";

	url += "?motion=gettree&layer_order=" + __layer_order + "&t=" + Math.random();
	$.get(url,function(data){initial_dir_status(data);});
}

function initial_dir_status(data){
	if(data != "" && data.length != 2){		
		get_layer_items("0");
	}
	else if( (_dms_dir == "/tmp/mnt" && data == "") || data == ""){
		document.getElementById("noUSB").style.display = "";
		disk_flag=1;
	}
}

function submit_mediaserver(server, x){
	var server_type = eval('document.mediaserverForm.'+server);

	showLoading();
	if(x == 1)
		server_type.value = 0;
	else
		server_type.value = 1;

	document.mediaserverForm.flag.value = "nodetect";	
	document.mediaserverForm.submit();
}

// get folder 
var dm_dir = new Array(); 
var WH_INT=0,Floder_WH_INT=0,General_WH_INT=0;
var folderlist = new Array();

function applyRule(){	
	
		if(validForm()){
			if(document.form.dms_enable.value == 1 && document.form.dms_dir_manual.value == 1){
						var rule_num = document.getElementById("dlna_path_table").rows.length;
						var item_num = document.getElementById("dlna_path_table").rows[0].cells.length;
						var dms_dir_tmp_value = "";
						var dms_dir_type_tmp_value = "";
		  			
						if(item_num >1){
							for(i=0; i<rule_num; i++){			
								dms_dir_tmp_value += "<";
								dms_dir_tmp_value += document.getElementById("dlna_path_table").rows[i].cells[0].title;
						
								var type_translate_tmp = "";
								dms_dir_type_tmp_value += "<";
								type_translate_tmp += document.getElementById("dlna_path_table").rows[i].cells[1].innerHTML.indexOf("Audio")>=0? "A":""; 
								type_translate_tmp += document.getElementById("dlna_path_table").rows[i].cells[1].innerHTML.indexOf("Image")>=0? "P":"";
								type_translate_tmp += document.getElementById("dlna_path_table").rows[i].cells[1].innerHTML.indexOf("Video")>=0? "V":"";			
								dms_dir_type_tmp_value += type_translate_tmp;			
							}
						}
		  			
						document.form.dms_dir_x.value = dms_dir_tmp_value;
						document.form.dms_dir_type_x.value = dms_dir_type_tmp_value;	
			}
		}
		else{
			return false;
		}
			

	if(document.form.dms_enable.value == 0){
		document.form.dms_friendly_name.disabled = true;
		document.form.dms_dir_x.disabled = true;
		document.form.dms_dir_type_x.disabled = true;
	}
	if(document.form.dms_dir_manual.value == 0){
		document.form.dms_dir_x.disabled = true;
		document.form.dms_dir_type_x.disabled = true;		
	}
	
	showLoading();
	FormActions("start_apply.htm", "apply", "restart_media", "5");
	document.form.submit();
}

function validForm(){

if(document.form.daapd_enable.value == 1){	
	if(document.form.daapd_friendly_name.value.length == 0){
		showtext(document.getElementById("alert_msg1"), "<#JS_fieldblank#>");
		document.form.daapd_friendly_name.focus();
		document.form.daapd_friendly_name.select();
		return false;
	}
	else{
		
		var alert_str1 = validator.hostName(document.form.daapd_friendly_name);
		if(alert_str1 != ""){
			showtext(document.getElementById("alert_msg1"), alert_str1);
			document.getElementById("alert_msg1").style.display = "";
			document.form.daapd_friendly_name.focus();
			document.form.daapd_friendly_name.select();
			return false;
		}else{
			document.getElementById("alert_msg1").style.display = "none";
  	}

		document.form.daapd_friendly_name.value = trim(document.form.daapd_friendly_name.value);
	}	
}

if(document.form.dms_enable.value == 1){
	if(document.form.dms_friendly_name.value.length == 0){
		showtext(document.getElementById("alert_msg2"), "<#JS_fieldblank#>");
		document.form.dms_friendly_name.focus();
		document.form.dms_friendly_name.select();
		return false;
	}
	else{
		
		var alert_str2 = validator.hostName(document.form.dms_friendly_name);
		if(alert_str2 != ""){
			showtext(document.getElementById("alert_msg2"), alert_str2);
			document.getElementById("alert_msg2").style.display = "";
			document.form.dms_friendly_name.focus();
			document.form.dms_friendly_name.select();
			return false;
		}else{
			document.getElementById("alert_msg2").style.display = "none";
  	}

		document.form.dms_friendly_name.value = trim(document.form.dms_friendly_name.value);
	}	
}	
	
	return true;	
}

function get_disk_tree(){
	if(disk_flag == 1){
		alert('<#no_usb_found#>');
		return false;	
	}
	cal_panel_block("folderTree_panel", 0.25);
	$("#folderTree_panel").fadeIn(300);
	get_layer_items("0");
}
function get_layer_items(layer_order){
	$.ajax({
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
	for(var j=0;j<treeitems.length;j++){
		//treeitems[j] : "Download2#22#0"
		array_temp_split[j] = treeitems[j].split("#"); 
		// Mipsel:asusware  Mipsbig:asusware.big  Armel:asusware.arm  // To hide folder 'asusware'
		if( array_temp_split[j][0].match(/^asusware$/)	|| array_temp_split[j][0].match(/^asusware.big$/) || array_temp_split[j][0].match(/^asusware.arm$/) ){
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
		 		short_ItemText = ItemText.substring(0,30)+"...";
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
		TempObject +='<img id="a'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' class="FdRead" src="/images/Tree/vert_line_'+isSubTree+'0.gif">';
		TempObject +='</td>';
	
		if(layer == 3){
		/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/	
			TempObject +='<td>';		
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
			TempObject +='</td>';
			TempObject +='<td>';
			TempObject +='<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>\n';		
			TempObject +='</td>';
		}
		else if(layer == 2){
			TempObject +='<td>';
			TempObject +='<table class="tree_table">';
			TempObject +='<tr>';
			TempObject +='<td class="vert_line">';
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
			TempObject +='</td>';
			TempObject +='<td class="FdText">';
			TempObject +='<span id="d'+ItemBarCode+'"'+SubClick+' title="'+ItemText+'">'+shown_ItemText+'</span>';
			TempObject +='</td>';
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
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
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
	document.getElementById("e"+this.FromObject).innerHTML = TempObject;
}

function build_array(obj,layer){
	var path_temp ="/mnt";
	var layer2_path ="";
	var layer3_path ="";
	if(obj.id.length>6){
		if(layer ==3){
			//layer3_path = "/" + document.getElementById(obj.id).innerHTML;
			layer3_path = "/" + obj.title;
			while(layer3_path.indexOf("&nbsp;") != -1)
				layer3_path = layer3_path.replace("&nbsp;"," ");
				
			if(obj.id.length >8)
				layer2_path = "/" + document.getElementById(obj.id.substring(0,obj.id.length-3)).innerHTML;
			else
				layer2_path = "/" + document.getElementById(obj.id.substring(0,obj.id.length-2)).innerHTML;
			
			while(layer2_path.indexOf("&nbsp;") != -1)
				layer2_path = layer2_path.replace("&nbsp;"," ");
		}
	}
	if(obj.id.length>4 && obj.id.length<=6){
		if(layer ==2){
			//layer2_path = "/" + document.getElementById(obj.id).innerHTML;
			layer2_path = "/" + obj.title;
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
		document.getElementById('createFolderBtn').className = "createFolderBtn";
		document.getElementById('deleteFolderBtn').className = "deleteFolderBtn";
		document.getElementById('modifyFolderBtn').className = "modifyFolderBtn";
		
		document.getElementById('createFolderBtn').onclick = function(){};
		document.getElementById('deleteFolderBtn').onclick = function(){};
		document.getElementById('modifyFolderBtn').onclick = function(){};
	}
	else if(layer == 2){
		// chose Partition
		setSelectedPoolOrder(selectedObj.id);
		path_directory = build_array(selectedObj,layer);
		document.getElementById('createFolderBtn').className = "createFolderBtn_add";
		document.getElementById('deleteFolderBtn').className = "deleteFolderBtn";
		document.getElementById('modifyFolderBtn').className = "modifyFolderBtn";

		document.getElementById('createFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popCreateFolder.asp');};		
		document.getElementById('deleteFolderBtn').onclick = function(){};
		document.getElementById('modifyFolderBtn').onclick = function(){};
		document.aidiskForm.layer_order.disabled = "disabled";
		document.aidiskForm.layer_order.value = barcode;		
	}
	else if(layer == 3){
		// chose Shared-Folder
		setSelectedFolderOrder(selectedObj.id);
		path_directory = build_array(selectedObj,layer);
		document.getElementById('createFolderBtn').className = "createFolderBtn";
		document.getElementById('deleteFolderBtn').className = "deleteFolderBtn_add";
		document.getElementById('modifyFolderBtn').className = "modifyFolderBtn_add";

		document.getElementById('createFolderBtn').onclick = function(){};		
		document.getElementById('deleteFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popDeleteFolder.asp');};
		document.getElementById('modifyFolderBtn').onclick = function(){popupWindow('OverlayMask','/aidisk/popModifyFolder.asp');};
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
function cancel_folderTree(){
	this.FromObject ="0";
	$("#folderTree_panel").fadeOut(300);
}
function confirm_folderTree(){
	document.getElementById('PATH').value = path_directory ;
	this.FromObject ="0";
	$("#folderTree_panel").fadeOut(300);
	//check_dir_path();
}

function check_dir_path(){
	var dir_array = document.getElementById('PATH').value.split("/");
	if(dir_array[dir_array.length - 1].length > 21)
		document.getElementById('PATH').value = "/" + dir_array[1] + "/" + dir_array[2] + "/" + dir_array[dir_array.length - 1].substring(0,18) + "...";
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById("dlna_path_table").deleteRow(i);
  
  var dms_dir_x_tmp = "";
  var dms_dir_type_x_tmp = "";  
	for(var k=0; k<document.getElementById("dlna_path_table").rows.length; k++){
			var tmp_type = "";
			dms_dir_x_tmp += "&#60";
			dms_dir_x_tmp += document.getElementById("dlna_path_table").rows[k].cells[0].title;
			dms_dir_type_x_tmp += "&#60";
				tmp_type += document.getElementById("dlna_path_table").rows[k].cells[1].innerHTML.indexOf("Audio")>=0? "A " : "";
				tmp_type += document.getElementById("dlna_path_table").rows[k].cells[1].innerHTML.indexOf("Image")>=0? "P " : "";
				tmp_type += document.getElementById("dlna_path_table").rows[k].cells[1].innerHTML.indexOf("Video")>=0? "V " : "";
			dms_dir_type_x_tmp += tmp_type;
	}
	
	dms_dir_x_array = dms_dir_x_tmp;
	dms_dir_type_x_array = dms_dir_type_x_tmp;
	
	if(dms_dir_x_array == "")
		show_dlna_path();
}

function addRow_Group(upper){
	var dms_dir_type_x_tmp = "";
	var rule_num = document.getElementById("dlna_path_table").rows.length;
	var item_num = document.getElementById("dlna_path_table").rows[0].cells.length;		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}
	
	if(document.getElementById("PATH").value==""){
		alert("<#JS_fieldblank#>");
		document.getElementById("PATH").focus();
		document.getElementById("PATH").select();
		return false;
	}else if(document.getElementById("PATH").value.indexOf("<") >= 0){
		alert("<#JS_validstr2#>&nbsp; <");
		document.getElementById("PATH").focus();
		document.getElementById("PATH").select();
		return false;
	}
	
	if(!document.form.type_A_audio.checked && !document.form.type_P_image.checked && !document.form.type_V_video.checked){
		alert("Shared Content Type is not set yet.");
		return false;
	}
	else{
		dms_dir_type_x_tmp += document.form.type_A_audio.checked? "A" : "";
		dms_dir_type_x_tmp += document.form.type_P_image.checked? "P" : "";
		dms_dir_type_x_tmp += document.form.type_V_video.checked? "V" : "";
	}
	
	//Viz check same rule  //match(path) is not accepted
		if(item_num >=2){
			for(i=0; i<rule_num; i++){
					if(document.getElementById("PATH").value.toLowerCase() == document.getElementById("dlna_path_table").rows[i].cells[0].title.toLowerCase()){
						alert("<#JS_duplicate#>");
						document.getElementById("PATH").focus();
						document.getElementById("PATH").select();
						return false;
					}	
			}
		}	
	
	addRow_dir_x(document.getElementById("PATH"));
	addRow_dir_type_x(dms_dir_type_x_tmp);
	document.getElementById("PATH").value = "/mnt";
	document.form.type_A_audio.checked = true;
	document.form.type_P_image.checked = true;
	document.form.type_V_video.checked = true;
	
	show_dlna_path();
}

function addRow_dir_x(obj){
	dms_dir_x_array += "&#60"			
	dms_dir_x_array += obj.value;	
}

function addRow_dir_type_x(v){
	dms_dir_type_x_array += "&#60"			
	dms_dir_type_x_array += v;
}

function show_dlna_path(){
	var dms_dir_x_array_row = dms_dir_x_array.split('&#60');
	var dms_dir_type_x_array_row = dms_dir_type_x_array.split('&#60');	
	var code = "";
	
	code +='<table width="98%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="dlna_path_table">';
	if(dms_dir_x_array_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{		
		for(var i = 1; i < dms_dir_x_array_row.length; i++){
			var tmp_type = "";
			code +='<tr id="row'+i+'">';
			
			code +='<td width="45%" class="dlna_path_td" title="'+ dms_dir_x_array_row[i] +'">'+ dms_dir_x_array_row[i] +'</td>';
				tmp_type += dms_dir_type_x_array_row[i].indexOf("A")>=0? "Audio " : "";
				tmp_type += dms_dir_type_x_array_row[i].indexOf("P")>=0? "Image " : "";
				tmp_type += dms_dir_type_x_array_row[i].indexOf("V")>=0? "Video " : "";
			code +='<td width="40%" class="dlna_path_td">'+ tmp_type +'</td>';
				
			code +='<td width="15%">';
			code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
	
	code +='</table>';
	document.getElementById("dlna_path_Block").innerHTML = code;	
}

function do_get_friendly_name(v){
if(v == "daapd"){
	var friendly_name_daapd	= "";
	if("<% nvram_get("daapd_friendly_name"); %>" != "")
		friendly_name_daapd = "<% nvram_get("daapd_friendly_name"); %>";
	else if("<% nvram_get("odmpid"); %>" != "")
		friendly_name_daapd = "<% nvram_get("odmpid"); %>";
	else
		friendly_name_daapd = "<% nvram_get("productid"); %>";
	
	document.form.daapd_friendly_name.value = friendly_name_daapd;
}	
else if(v == "dms"){	
	var friendly_name_dms	= "";
	if("<% nvram_get("dms_friendly_name"); %>" != "")
		friendly_name_dms = "<% nvram_get("dms_friendly_name"); %>";
	else if("<% nvram_get("odmpid"); %>" != "")	
		friendly_name_dms = "<% nvram_get("odmpid"); %>";
	else	
		friendly_name_dms = "<% nvram_get("productid"); %>";
	
	document.form.dms_friendly_name.value = friendly_name_dms;
}	
}

function set_dms_dir(obj){
	if(obj.value == 1){	//manual path
		document.getElementById("dlna_path_div").style.display = "";
		document.form.dms_dir_manual.value = 1;
		show_dlna_path();
	}
	else{		//default path
		document.getElementById("dlna_path_div").style.display = "none";
		document.form.dms_dir_manual.value = 0;
	}
}

</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<!-- floder tree-->
<div id="DM_mask" class="mask_bg"></div>
<div id="folderTree_panel" class="panel_folder" >
		<table><tr><td>
			<div class="machineName" style="width:200px;font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:15px;margin-left:30px;"><#Web_Title2#></div>
		</td>
		<td>
			<div style="width:240px;margin-top:14px;margin-left:135px;">
				<table >
					<tr>
						<td><div id="createFolderBtn" class="createFolderBtn" title="<#AddFolderTitle#>"></div></td>
						<td><div id="deleteFolderBtn" class="deleteFolderBtn" title="<#DelFolderTitle#>"></div></td>
						<td><div id="modifyFolderBtn" class="modifyFolderBtn" title="<#ModFolderTitle#>"></div></td>
					</tr>
				</table>
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
<form method="post" name="mediaserverForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_media">
<input type="hidden" name="action_wait" value="2">
<input type="hidden" name="current_page" value="/mediaserver.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="daapd_enable" value="<% nvram_get("daapd_enable"); %>">
</form>

<form method="post" name="aidiskForm" action="" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="test_flag" value="" disabled="disabled">
<input type="hidden" name="protocol" id="protocol" value="">
</form>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="mediaserver.asp">
<input type="hidden" name="next_page" value="mediaserver.asp">
<input type="hidden" name="flag" value="nodetect">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="daapd_enable" value="<% nvram_get("daapd_enable"); %>">
<input type="hidden" name="dms_dir" value="">
<input type="hidden" name="dms_enable" value="<% nvram_get("dms_enable"); %>">
<input type="hidden" name="dms_dir_x" value="">
<input type="hidden" name="dms_dir_type_x" value="">
<input type="hidden" name="dms_dir_manual" value="<% nvram_get("dms_dir_manual"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
  <td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
		<br>

<!--=====Beginning of Main Content=====-->
<div id="upnp_table" class="upnp_table" align="left" border="0" cellpadding="0" cellspacing="0">
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
  <tr>
  	<td>
				<div>
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#UPnPMediaServer#></span>
							</td>
							<td align="right">
								<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
				<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>

			<div id="upnp_desc_id" class="formfontdesc"><#upnp_Desc#></div>
		</td>
  </tr>  

  <tr>
   	<td>
   		<div id="iTunes_div">
   		<table id="iTunes" width="98%" border="1" align="center" cellpadding="4" cellspacing="1" bordercolor="#6b8fa3" class="FormTable">
 				<thead>
					<tr><td colspan="2">iTunes Server</td></tr>
				</thead>  
   			<tr>
        	<th><#BasicConfig_EnableiTunesServer_itemname#></th>
        	<td>
        			<div class="left" style="width:94px; position:relative; left:3%;" id="radio_daapd_enable"></div>
							<div class="clear"></div>
							<script type="text/javascript">
									$('#radio_daapd_enable').iphoneSwitch('<% nvram_get("daapd_enable"); %>', 
										 function() {
										 	document.form.daapd_friendly_name.parentNode.parentNode.parentNode.style.display = "";										 	
											document.form.daapd_enable.value = 1;
										 },
										 function() {
											document.form.daapd_friendly_name.parentNode.parentNode.parentNode.style.display = "none";
											document.form.daapd_enable.value = 0;
											do_get_friendly_name("daapd");
										 }
									);
							</script>
        		</td>
       	</tr>
       	<tr>
       		<th><#iTunesServer_itemname#></th>
					<td>
						<div><input name="daapd_friendly_name" type="text" style="margin-left:15px;" class="input_15_table" maxlength="32" value="" autocorrect="off" autocapitalize="off"><br/><div id="alert_msg1" style="color:#FC0;margin-left:10px;"></div></div>
					</td>
      	</tr>
      	</table> 
      </div>	
      <div style="margin-top:10px;">
   		<table id="dlna" width="98%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
   			<thead>
					<tr><td colspan="2"><#UPnPMediaServer#></td></tr>
				</thead>
   			<tr>
        	<th><#DLNA_enable#></th>
        	<td>
        			<div class="left" style="width:94px; position:relative; left:3%;" id="radio_dms_enable"></div>
							<div class="clear"></div>
							<script type="text/javascript">
									$('#radio_dms_enable').iphoneSwitch('<% nvram_get("dms_enable"); %>', 
										 function() {
										 	document.form.dms_friendly_name.parentNode.parentNode.parentNode.style.display = "";
										 	document.form.dms_dir_manual_x[0].parentNode.parentNode.style.display = "";
											document.getElementById("dmsStatus").parentNode.parentNode.style.display = "";
											//document.getElementById("dlna_path_div").style.display = "";
											//show_dlna_path();
											set_dms_dir(document.form.dms_dir_manual);
											document.form.dms_enable.value = 1;									
										 },
										 function() {
										 	document.form.dms_friendly_name.parentNode.parentNode.parentNode.style.display = "none";
										 	document.form.dms_dir_manual_x[0].parentNode.parentNode.style.display = "none";
											document.getElementById("dmsStatus").parentNode.parentNode.style.display = "none";
											document.getElementById("dlna_path_div").style.display = "none";
											document.form.dms_enable.value = 0;
											do_get_friendly_name("dms");
										 }
									);
							</script>
							<div id="noUSB" style="color:#FC0;display:none;margin-left:17px;padding-top:2px;padding-bottom:2px;"><#no_usb_found#></div>
        		</td>
       	</tr>
       	<tr>
       		<th><#DLNA_itemname#></th>
					<td>
						<div><input name="dms_friendly_name" type="text" style="margin-left:15px;" class="input_15_table" maxlength="32" value="" autocorrect="off" autocapitalize="off"><br/><div id="alert_msg2" style="color:#FC0;margin-left:10px;"></div></div>
					</td>
      	</tr>
   			<tr>
        	<th><#DLNA_status#></th>
        	<td><span id="dmsStatus" style="margin-left:15px">Idle</span>
        	</td>
       	</tr>
   			<tr>
        	<th><#DLNA_path_setting#></th>
        	<td>
        		<input type="radio" value="0" name="dms_dir_manual_x" class="input" onClick="set_dms_dir(this);" <% nvram_match("dms_dir_manual", "0", "checked"); %>><#DLNA_path_shared#>
						<input type="radio" value="1" name="dms_dir_manual_x" class="input" onClick="set_dms_dir(this);" <% nvram_match("dms_dir_manual", "1", "checked"); %>><#DLNA_path_manual#>
        	</td>
       	</tr>	
      	</table> 
      	</div>
      	
      	<div id="dlna_path_div">
      	<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
			  	<thead>
			  		<tr>
						<td colspan="3" id="GWStatic"><#DLNA_path_manual#>&nbsp;(<#List_limit#>&nbsp;10)</td>
			  		</tr>
			  	</thead>

			  	<tr>
		  			<th><#DLNA_directory#></th>
        		<th><#DLNA_contenttype#></th>
        		<th><#list_add_delete#></th>
			  	</tr>			  
			  	<tr>
						<td width="45%">
								<input id="PATH" type="text" class="input_30_table" value="" onclick="get_disk_tree();" autocorrect="off" autocapitalize="off" readonly="readonly"/" placeholder="<#Select_menu_default#>" >
						</td>
						<td width="40%">
								<input type="checkbox" class="input" name="type_A_audio" checked>&nbsp;Audio&nbsp;&nbsp;
								<input type="checkbox" class="input" name="type_P_image" checked>&nbsp;Image&nbsp;&nbsp;
								<input type="checkbox" class="input" name="type_V_video" checked>&nbsp;Video
						</td>
						<td width="15%">
								<input type="button" class="add_btn" onClick="addRow_Group(10);" value="">
						</td>
			  	</tr>		  
			  </table>
			  <div id="dlna_path_Block"></div>
			  
      	</div>
       <div class="apply_gen">
           		<input type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
       </div>      	
    	</td> 
  </tr>    
  
  <tr>
  	<td>
  		<div id="upnp_icon" class="upnp_icon" style="display:none;"></div>
  	</td>
  </tr>
</table>

<!--=====End of Main Content=====-->
		</td>

		<td width="20" align="center" valign="top"></td>
	</tr>
</table>
</div>
</form>


<div id="footer"></div>

<!-- mask for disabling AiDisk -->
<div id="OverlayMask" class="popup_bg">
	<div align="center">
	<iframe src="" frameborder="0" scrolling="no" id="popupframe" width="400" height="400" allowtransparency="true" style="margin-top:150px;"></iframe>
	</div>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
</body>
</html>

