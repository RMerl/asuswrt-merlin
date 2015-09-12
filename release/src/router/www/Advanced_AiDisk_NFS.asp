<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - NFS Exports</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">

<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
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
<% get_AiDisk_status(); %>
<% disk_pool_mapping_info(); %>
var PROTOCOL = "cifs";
var _layer_order = "";
var FromObject = "0";
var lastClickedObj = 0;
var disk_flag=0;
window.onresize = cal_panel_block;

var nfsd_enable = '<% nvram_get("nfsd_enable"); %>';
var nfsd_exportlist_array = '<% nvram_get("nfsd_exportlist"); %>';

function initial(){
	show_menu();
	document.getElementById("_APP_Installation").innerHTML = '<table><tbody><tr><td><div class="_APP_Installation"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	document.getElementById("_APP_Installation").className = "menu_clicked";

	shownfsd_exportlist();

	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
	check_dir_path();
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
		eval("var default_dir=" + data);
	}
	else {
		document.getElementById("EditExports").style.display = "none";
		document.getElementById("NoUSB").style.display = "";
		disk_flag=1;
	}
}

function submit_server(x){
	var server_type = eval('document.serverForm.nfsd_enable ');

	showLoading();
	if(x == 1)
		server_type.value = 0;
	else
		server_type.value = 1;

	document.serverForm.flag.value = "nodetect";
	document.serverForm.submit();
}

// get folder
var dm_dir = new Array();
var WH_INT=0,Floder_WH_INT=0,General_WH_INT=0;
var folderlist = new Array();


function apply(){
	var rule_num = document.getElementById('nfsd_exportlist_table').rows.length;
	var item_num = document.getElementById('nfsd_exportlist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"
		for(j=0; j<item_num-1; j++){
			tmp_value += document.getElementById('nfsd_exportlist_table').rows[i].cells[j].innerHTML;
			if(j != item_num-2)
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";

	document.form.nfsd_exportlist.value = tmp_value;

	showLoading();
	FormActions("start_apply.htm", "apply", "restart_nasapps", "5");
	document.form.submit();
}
function get_disk_tree(){
	if(disk_flag == 1){
		alert('<#no_usb_found#>');
		return false;
	}
	cal_panel_block();
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
	for(var j=0;j<treeitems.length;j++){ // To hide folder 'Download2'
		array_temp_split[j] = treeitems[j].split("#");
		if( array_temp_split[j][0].match(/^asusware$/)	){
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

	document.getElementById("folderTree_panel").style.marginLeft = blockmarginLeft+"px";
}

function addRow(obj, head){
	if(head == 1)
		nfsd_exportlist_array += "&#60"
	else
		nfsd_exportlist_array += "&#62"

	nfsd_exportlist_array += obj.value;

	obj.value = "";
}

function addRow_Group(upper){
	var rule_num = document.getElementById('nfsd_exportlist_table').rows.length;
	var item_num = document.getElementById('nfsd_exportlist_table').rows[0].cells.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.form.nfsd_path_x_0.value==""){
		alert("You must specify a path to export!");
		document.form.nfsd_path_x_0.focus();
		document.form.nfsd_path_x_0.select();
		return false;
	}else {

		if(item_num >=2){
			for(i=0; i<rule_num; i++){
				if(document.form.nfsd_path_x_0.value == document.getElementById('nfsd_exportlist_table').rows[i].cells[0].innerHTML){
					alert("<#JS_duplicate#>");
					document.form.nfsd_path_x_0.focus();
					document.form.nfsd_path_x_0.select();
					return false;
				}
			}
		}

		if(document.form.nfsd_accesslist_x_0.value=="")
			document.form.nfsd_accesslist_x_0.value="*";

		addRow(document.form.nfsd_path_x_0 ,1);
		addRow(document.form.nfsd_accesslist_x_0, 0);
		addRow(document.form.nfsd_mountoptions_x_0, 0);
		shownfsd_exportlist();
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById('nfsd_exportlist_table').deleteRow(i);

  var nfsd_exportlist_value = "";
	for(k=0; k<document.getElementById('nfsd_exportlist_table').rows.length; k++){
		for(j=0; j<document.getElementById('nfsd_exportlist_table').rows[k].cells.length-1; j++){
			if(j == 0)
				nfsd_exportlist_value += "&#60";
			else
				nfsd_exportlist_value += "&#62";
			nfsd_exportlist_value += document.getElementById('nfsd_exportlist_table').rows[k].cells[j].innerHTML;
		}
	}

	nfsd_exportlist_array = nfsd_exportlist_value;
	if(nfsd_exportlist_array == "")
		shownfsd_exportlist();
}

function shownfsd_exportlist(){
	var nfsd_exportlist_row = nfsd_exportlist_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="nfsd_exportlist_table">';
	if(nfsd_exportlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < nfsd_exportlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var nfsd_exportlist_col = nfsd_exportlist_row[i].split('&#62');
				for(var j = 0; j < nfsd_exportlist_col.length; j++){
					code +='<td width="30%" style="word-break: break-all;">'+ nfsd_exportlist_col[j] +'</td>';
				}
				code +='<td width="10%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
	code +='</table>';
	document.getElementById("nfsd_exportlist_Block").innerHTML = code;
}

function check_dir_path(){
	var dir_array = document.getElementById('PATH').value.split("/");
	if(dir_array[dir_array.length - 1].length > 21)
	document.getElementById('PATH').value = "/" + dir_array[1] + "/" + dir_array[2] + "/" + dir_array[dir_array.length - 1].substring(0,18) + "...";
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
				<table>
					<tr>
						<td><div id="createFolderBtn" class="createFolderBtn" title="<#AddFolderTitle#>"></div></td>
						<td><div id="deleteFolderBtn" class="deleteFolderBtn" title="<#DelFolderTitle#>"></div></td>
						<td><div id="modifyFolderBtn" class="modifyFolderBtn" title="<#ModFolderTitle#>"></div></td>
					<tr>
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
<form method="post" name="serverForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_nasapps">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="/Advanced_AiDisk_NFS.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="nfsd_enable" value="<% nvram_get("nfsd_enable"); %>">
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
<input type="hidden" name="current_page" value="Advanced_AiDisk_NFS.asp">
<input type="hidden" name="next_page" value="Advanced_AiDisk_NFS.asp">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="nfsd_exportlist" value="">

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
<!--=====Beginning of Main Content=====-->
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	        <tr>
                <td align="left" valign="top" >

					<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tr>
							<td bgcolor="#4D595D" valign="top"  >
								<div>&nbsp;</div>
								<div style="width:730px">
									<table width="730px">
										<tr>
											<td align="left">
												<span class="formfonttitle">NFS Exports</span>
											</td>
											<td align="right">
												<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
											</td>
										</tr>
									</table>
							  	</div>
								<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc">Export directories from plugged USB disks over NFS.</div>

					   			<table class="FormTable" width="100%" cellspacing="1">
					   				<tr>
						    	    	<th>Enable NFSD</th>
						    	    	<td>
					        				<div class="left" style="width:94px; position:relative; left:3%;" id="radio_nfsd_enable"></div>
											<div class="clear"></div>
											<script type="text/javascript">
												$('#radio_nfsd_enable').iphoneSwitch('<% nvram_get("nfsd_enable"); %>',
													 function() {
														submit_server(0);
													 },
													 function() {
														submit_server(1);
													 }
												);
											</script>
					        			</td>
									</tr>
									<tr>
									<th>Enable legacy (NFS V2) support</th>
										<td>
											<input type="radio" name="nfsd_enable_v2" class="input" value="1" <% nvram_match_x("", "nfsd_enable_v2", "1", "checked"); %>><#checkbox_Yes#>
											<input type="radio" name="nfsd_enable_v2" class="input" value="0" <% nvram_match_x("", "nfsd_enable_v2", "0", "checked"); %>><#checkbox_No#>
										</td>
									</tr>

								</table>

								<div><br><b>Access list:</b> Separate hosts with a space.  You can use the <b>192.168.1.0/24</b> form for a whole subnet.  <b>*</b> = any host.
									<br><b>Options:</b> Comma-separated list of options to apply to every hosts within that export.
								</div>

								<div><br><span id="NoUSB" style="display:none;color:#FFCC00;">You need to plug a USB disk to your router to be able to export folders!</span><br></div>

					   			<table class="FormTable" width="100%" cellspacing="1">
									<tr>
										<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
											<tr>
									  			<th width="30%">Path</th>
												<th width="30%">Access list</th>
												<th width="30%">Options</th>
												<th width="10%">Add / Delete</th>
										  	</tr>
										  	<tr  id="EditExports">
												<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>

												<td width="30%">
													<input id="PATH" type="text" class="input_20_table" style="margin-left:15px;height:25px;" onclick="get_disk_tree();" readonly="readonly" name="nfsd_path_x_0">
												</td>
					        	    			<td width="30%">
					        	    				<input type="text" class="input_20_table" maxlength="128" name="nfsd_accesslist_x_0">
					        	    			</td>
					        	    			<td width="30%">
													<input type="text" class="input_20_table" maxlenght="64" name="nfsd_mountoptions_x_0">
												</td>
												<td width="10%">
													<div>
														<input type="button" class="add_btn" onClick="addRow_Group(128);" value="">
													</div>
					        	    			</td>
										  	</tr>
										</table>

										<div id="nfsd_exportlist_Block"></div>

										<!-- manually assigned the nfsd List end-->
						    	       	<div class="apply_gen">
						    	       		<input type="button" name="button" class="button_gen" onclick="apply();" value="<#CTL_apply#>"/>
					        	    	</div>

									</tr>
								</table>
							</td>
						</tr>
					</table>
				</td>
			</tr>
		</table>
		<!--=====End of Main Content=====-->
	</td>

	<td width="20" align="center" valign="top"></td>
  </tr>
</table>
<div id="footer"></div>

<!-- mask for disabling AiDisk -->
<div id="OverlayMask" class="popup_bg">
	<div align="center">
		<iframe src="" frameborder="0" scrolling="no" id="popupframe" width="400" height="400" allowtransparency="true" style="margin-top:150px;"></iframe>
	</div>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
</form>

</body>
</html>

