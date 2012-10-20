<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#menu2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">

<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/aidisk/AiDisk_folder_tree.js"></script>
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
.upnp_button_table{
	width:730px;
	background-color:#15191b;
	margin-top:15px;
	margin-right:5px;
}
.upnp_button_table th{
	width:300px;
	height:40px;
	text-align:left;
	background-color:#1f2d35;
	font:Arial, Helvetica, sans-serif;
	font-size:12px;
	padding-left:10px;
	color:#FFFFFF;
	background: url(/images/general_th.gif) repeat;
}	
.upnp_button_table td{
	width:436px;
	height:40px;
	background-color:#475a5f;
	font:Arial, Helvetica, sans-serif;
	font-size:12px;
	padding-left:5px;
	color:#FFFFFF;
}	
.upnp_icon{
	background: url(/images/New_ui/USBExt/media_sever.jpg) no-repeat;
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
</style>
<script>
var $j = jQuery.noConflict();
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var dms_status = <% dms_info(); %>;
var _dms_dir = '<%nvram_get("dms_dir");%>';
<% get_AiDisk_status(); %>
<% disk_pool_mapping_info(); %>
var _layer_order = "";
var FromObject = "0";
var lastClickedObj = 0;
var disk_flag=0;
var PROTOCOL = "cifs";
window.onresize = cal_panel_block;

function initial(){
	show_menu();
	$("option5").innerHTML = '<table><tbody><tr><td><div id="index_img5"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	$("option5").className = "m5_r";

	if(dms_status[0] != "")
		$("dmsStatus").innerHTML = "Scanning.."

	if((calculate_height-3)*52 + 20 > 535)
		$("upnp_icon").style.height = (calculate_height-3)*52 -70 + "px";
	else
		$("upnp_icon").style.height = "500px";
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
		get_layer_items("0");
		eval("var default_dir=" + data);
	}
	else if( (_dms_dir == "/tmp/mnt" && data == "") || data == ""){
		$("noUSB").style.display = "";
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
	document.form.dms_dir.value = $("PATH").value;
	showLoading();
	FormActions("start_apply.htm", "apply", "restart_media", "3");
	document.form.submit();
}
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
		TempObject +='<img id="a'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' class="FdRead" src="/images/Tree/vert_line_'+isSubTree+'0.gif">';
		TempObject +='</td>';
	
		if(layer == 3){
		/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/	
			TempObject +='<td>';		
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
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
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'$("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
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

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<!-- floder tree-->
<div id="DM_mask" class="mask_bg"></div>
<div id="folderTree_panel" class="panel_folder" >
		<table><tr><td>
			<div class="machineName" style="width:200px;font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:15px;margin-left:30px;"><#Web_Title2#></div>
		</td>
		<td>
			<div style="width:240px;margin-top:17px;margin-left:125px;">
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
<form method="post" name="mediaserverForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_media">
<input type="hidden" name="action_wait" value="2">
<input type="hidden" name="current_page" value="/mediaserver.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="dms_enable" value="<% nvram_get("dms_enable"); %>">
<input type="hidden" name="daapd_enable" value="<% nvram_get("daapd_enable"); %>">
</form>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="mediaserver.asp">
<input type="hidden" name="next_page" value="mediaserver.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="dms_dir" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<form method="post" name="aidiskForm" action="" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="test_flag" value="" disabled="disabled">
<input type="hidden" name="protocol" id="protocol" value="">
</form>

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
<table>
  <tr>
  	<td>
				<div style="width:730px">
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#UPnPMediaServer#></span>
							</td>
							<td align="right">
								<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
				<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>

			<div class="formfontdesc"><#upnp_Desc#></div>	<!-- "upnp_Desc" is a untranslated string. -->
		</td>	<!--<span class="formfonttitle"></span> -->
  </tr>  
  <!--tr>
  	<td class="line_export"><img src="images/New_ui/export/line_export.png" /></td>
  </tr-->
   
  <tr>
   	<td>
   		<div class="upnp_button_table"> 
   		<table cellspacing="1">
   			<tr>
        	<th>Enable DLNA Media Server</th>
        	<td>
        			<div class="left" style="width:94px; position:relative; left:3%;" id="radio_dms_enable"></div>
							<div class="clear"></div>
							<script type="text/javascript">
									$j('#radio_dms_enable').iphoneSwitch('<% nvram_get("dms_enable"); %>', 
										 function() {
											submit_mediaserver("dms_enable", 0);
										 },
										 function() {
											submit_mediaserver("dms_enable", 1);
										 },
										 {
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
										 }
									);
							</script>
        		</td>
       	</tr>

   			<tr>
        	<th><#BasicConfig_EnableiTunesServer_itemname#></th>
        	<td>
        			<div class="left" style="width:94px; position:relative; left:3%;" id="radio_daapd_enable"></div>
							<div class="clear"></div>
							<script type="text/javascript">
									$j('#radio_daapd_enable').iphoneSwitch('<% nvram_get("daapd_enable"); %>', 
										 function() {
											submit_mediaserver("daapd_enable", 0);
										 },
										 function() {
											submit_mediaserver("daapd_enable", 1);
										 },
										 {
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
										 }
									);
							</script>
        		</td>
       	</tr>

        <!--tr>
          <th class="hintstyle_download" id="multiSetting_5">Media server directory</th>
          <td>
          <input type="text" id="PATH" class="input_25_table" style="margin-left:15px;height:25px;" value="<% nvram_get("dms_dir"); %>" onclick="showPanel();" readonly="readonly"/">
					<input type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
          </td>
        </tr-->
<tr>
        	<th>Media server directory</th>
        	<td>
				<input id="PATH" type="text"  class="input_25_table" style="margin-left:15px;height:25px;" value="<% nvram_show_chinese_char("dms_dir"); %>" onclick="get_disk_tree();" readonly="readonly"/">
				<input type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
				<div id="noUSB" style="color:#FC0;display:none;margin-left:17px;padding-top:2px;padding-bottom:2px;"><#no_usb_found#></div>
        	</td>
       	</tr>

   			<tr>
        	<th>Media Server Status</th>
        	<td><span id="dmsStatus" style="margin-left:15px">Idle</span>
        	</td>
       	</tr>
      	</table> 
      	</div>
    	</td> 
  </tr>  
  
  <tr>
  	<td>
  		<div id="upnp_icon" class="upnp_icon"></div>
  	</td>
  </tr>
</table>

<!--=====End of Main Content=====-->
		</td>

		<td width="20" align="center" valign="top"></td>
	</tr>
</table>
</div>

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

