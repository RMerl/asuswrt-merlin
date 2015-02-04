<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Other Settings</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/merlin.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
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
var $j = jQuery.noConflict();
<% get_AiDisk_status(); %>
var PROTOCOL = "cifs";
var _layer_order = "";
var FromObject = "0";
var lastClickedObj = 0;
var disk_flag=0;
window.onresize = cal_panel_block;


function initial() {
	show_menu();
	initConntrackValues()
	set_rstats_location();
	hide_rstats_storage(document.form.rstats_location.value);
	hide_cstats(getRadioValue(document.form.cstats_enable));
	hide_cstats_ip(getRadioValue(document.form.cstats_all));
	if (document.form.usb_idle_exclude.value.indexOf("a") != -1)
		document.form.usb_idle_exclude_a.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("b") != -1)
		document.form.usb_idle_exclude_b.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("c") != -1)
		document.form.usb_idle_exclude_c.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("d") != -1)
		document.form.usb_idle_exclude_d.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("e") != -1)
		document.form.usb_idle_exclude_e.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("f") != -1)
		document.form.usb_idle_exclude_f.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("g") != -1)
		document.form.usb_idle_exclude_g.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("h") != -1)
		document.form.usb_idle_exclude_h.checked = true;
	if (document.form.usb_idle_exclude.value.indexOf("i") != -1)
		document.form.usb_idle_exclude_i.checked = true;

	if ((productid == "RT-AC56U") || (productid == "RT-AC68U") || (productid == "RT-AC87U"))
		document.getElementById("ct_established_default").innerHTML = "Default: 432000 (5 days)";

	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
}

function initial_dir(){
	var __layer_order = "0_0";
	var url = "/getfoldertree.asp";
	var type = "General";

	url += "?motion=gettree&layer_order=" + __layer_order + "&t=" + Math.random();
	$j.get(url,function(data){initial_dir_status();});
}

function initial_dir_status(data){
	if(data != "" && data.length != 2){
		get_layer_items("0");
		eval("var default_dir=" + data);
	} else {
		showhide("pathPicker", 0);
		disk_flag=1;
	}
}

// get folder
var dm_dir = new Array();
var WH_INT=0,Floder_WH_INT=0,General_WH_INT=0;
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
	for(var j=0;j<treeitems.length;j++){ // To hide folder 'Asusware'
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
			layer3_path = "/" + obj.title;
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
		$('createFolderBtn').className = "createFolderBtn";
		$('deleteFolderBtn').className = "deleteFolderBtn";
		$('modifyFolderBtn').className = "modifyFolderBtn";

		$('createFolderBtn').onclick = function(){};
		$('deleteFolderBtn').onclick = function(){};
		$('modifyFolderBtn').onclick = function(){};
	}
	else if(layer == 2){
		// chose Partition
		setSelectedPoolOrder(selectedObj.id);
		path_directory = build_array(selectedObj,layer);
		$('createFolderBtn').className = "createFolderBtn_add";
		$('deleteFolderBtn').className = "deleteFolderBtn";
		$('modifyFolderBtn').className = "modifyFolderBtn";

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
		$('createFolderBtn').className = "createFolderBtn";
		$('deleteFolderBtn').className = "deleteFolderBtn_add";
		$('modifyFolderBtn').className = "modifyFolderBtn_add";

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
	$('rstats_path').value = path_directory + "/" ;
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



function set_rstats_location()
{
	rstats_loc = '<% nvram_get("rstats_path"); %>';

	if (rstats_loc === "")
	{
		document.form.rstats_location.value = "0";
	}
	else if (rstats_loc === "*nvram")
	{
		document.form.rstats_location.value = "2";
	}
	else
	{
		document.form.rstats_location.value = "1";
	}

}

function hide_rstats_storage(_value){

	showhide("rstats_new_tr", ((_value == "1") || (_value == "2")));
	showhide("rstats_stime_tr", ((_value == "1") || (_value == "2")));
	showhide("rstats_path_tr", (_value == "1"));

}

function hide_cstats_ip(_value){

        showhide("cstats_inc_tr", ((_value == "0") && (getRadioValue(document.form.cstats_enable) == 1)));
}

function hide_cstats(_value){

        showhide("cstats_1_tr", (_value == "1"));
        showhide("cstats_2_tr", (_value == "1"));
        showhide("cstats_inc_tr", ((_value == "1") && (getRadioValue(document.form.cstats_all) == 0)));
        showhide("cstats_exc_tr", (_value == "1"));
}

function initConntrackValues(){

	tcp_array = document.form.ct_tcp_timeout.value.split(" ");

	document.form.tcp_established.value = tcp_array[1];
	document.form.tcp_syn_sent.value = tcp_array[2];
	document.form.tcp_syn_recv.value = tcp_array[3];
	document.form.tcp_fin_wait.value = tcp_array[4];
	document.form.tcp_time_wait.value = tcp_array[5];
	document.form.tcp_close.value = tcp_array[6];
	document.form.tcp_close_wait.value = tcp_array[7];
	document.form.tcp_last_ack.value = tcp_array[8];

	udp_array = document.form.ct_udp_timeout.value.split(" ");

	document.form.udp_unreplied.value = udp_array[0];
	document.form.udp_assured.value = udp_array[1];

}


function applyRule(){

	showLoading();

	if (document.form.rstats_location.value == "2")
	{
		document.form.rstats_path.value = "*nvram";
	} else if (document.form.rstats_location.value == "0")
	{
		document.form.rstats_path.value = "";
	}

	document.form.ct_tcp_timeout.value = "0 "+
		document.form.tcp_established.value +" " +
		document.form.tcp_syn_sent.value +" " +
		document.form.tcp_syn_recv.value +" " +
		document.form.tcp_fin_wait.value +" " +
		document.form.tcp_time_wait.value +" " +
		document.form.tcp_close.value +" " +
		document.form.tcp_close_wait.value +" " +
		document.form.tcp_last_ack.value +" 0";

	document.form.ct_udp_timeout.value = document.form.udp_unreplied.value + " "+document.form.udp_assured.value;

	excluded = "";
	if (document.form.usb_idle_exclude_a.checked)
		excluded += "a";
        if (document.form.usb_idle_exclude_b.checked)
                excluded += "b";
        if (document.form.usb_idle_exclude_c.checked)
                excluded += "c";
	if (document.form.usb_idle_exclude_d.checked)
		excluded += "d";
	if (document.form.usb_idle_exclude_e.checked)
		excluded += "e";
	if (document.form.usb_idle_exclude_f.checked)
		excluded += "f";
	if (document.form.usb_idle_exclude_g.checked)
		excluded += "g";
	if (document.form.usb_idle_exclude_h.checked)
		excluded += "h";
	if (document.form.usb_idle_exclude_i.checked)
		excluded += "i";

	document.form.usb_idle_exclude.value = excluded;

	if ( (excluded != "<% nvram_get("usb_idle_exclude"); %>") ||  (document.form.usb_idle_timeout.value != <% nvram_get("usb_idle_timeout"); %>) )
                document.form.action_script.value += ";restart_sdidle";

	if (getRadioValue(document.form.cstats_enable) != "<% nvram_get("cstats_enable"); %>") {
		if ( (getRadioValue(document.form.cstats_enable) == 1) && (<% nvram_get("ctf_disable"); %> == 0) )
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		else
			document.form.action_script.value += ";restart_firewall;restart_cstats";
	} else {
		document.form.action_script.value += ";restart_cstats";
	}
	document.form.submit();
}

function update_filter(o,v) {
	var i;
	var filterip = [];

	if (v.length>0) {
		filterip = v.split(',');
		for (i = 0; i < filterip.length; ++i) {
			if ((filterip[i] = fixIP(filterip[i])) == null) {
				filterip.splice(i,1);
			}
		}
		o.value = (filterip.length > 0) ? filterip.join(',') : '';
	}
}

function validate(){

	if ((document.form.rstats_location.value == "2") && (getRadioValue(document.form.cstats_enable) == "1")) {
		showhide('invalid_location', 1);
		document.form.rstats_location.focus();

		return false;
	}

	applyRule();
}

function done_validating(action){
        refreshpage();
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
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

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="aidiskForm" action="" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="test_flag" value="" disabled="disabled">
<input type="hidden" name="protocol" id="protocol" value="">
</form>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Tools_OtherSettings.asp">
<input type="hidden" name="next_page" value="Tools_OtherSettings.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_rstats;restart_conntrack;restart_leds">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="ct_tcp_timeout" value="<% nvram_get("ct_tcp_timeout"); %>">
<input type="hidden" name="ct_udp_timeout" value="<% nvram_get("ct_udp_timeout"); %>">
<input type="hidden" name="usb_idle_exclude" value="<% nvram_get("usb_idle_exclude"); %>">


<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div></td>
    <td valign="top">
        <div id="tabMenu" class="submenuBlock"></div>

      <!--===================================Beginning of Main Content===========================================-->
      <table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        <tr>
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
                <div>&nbsp;</div>
                <div class="formfonttitle">Tools - Other Settings</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Traffic Monitoring</td>
						</tr>
					</thead>
					<tr>
						<th>Traffic history location</th>
			        	<td>
						<select name="rstats_location" class="input_option" onchange="hide_rstats_storage(this.value);">
								<option value="0">RAM (Default)</option>
								<option value="1">Custom location</option>
								<option value="2">NVRAM</option>
							</select>
							<span id="invalid_location" style="display:none;" class="formfontdesc">Cannot use NVRAM if IPTraffic is enabled!</span>
			   			</td>
					</tr>

					<tr id="rstats_stime_tr">
						<th>Save frequency:</th>
						<td>
							<select name="rstats_stime" class="input_option" >
								<option value="1" <% nvram_match("rstats_stime", "1","selected"); %>>Every 1 hour</option>
			           				<option value="6" <% nvram_match("rstats_stime", "6","selected"); %>>Every 6 hours</option>
			           				<option value="12" <% nvram_match("rstats_stime", "12","selected"); %>>Every 12 hours</option>
			           				<option value="24" <% nvram_match("rstats_stime", "24","selected"); %>>Every 1 day</option>
			           				<option value="72" <% nvram_match("rstats_stime", "72","selected"); %>>Every 3 days</option>
			           				<option value="168" <% nvram_match("rstats_stime", "168","selected"); %>>Every 1 week</option>
							</select>
						</td>
					</tr>
					<tr id="rstats_path_tr">
						<th>Save history location<br><i>Directory must end with a '/'.</i></th>
						<td><input type="text" id="rstats_path" size=32 maxlength=90 name="rstats_path" class="input_32_table" value="<% nvram_get("rstats_path"); %>">
						<button id="pathPicker" onclick="get_disk_tree(); return false;">Select...</button></td>
					</tr>
					<tr id="rstats_new_tr">
		        			<th>Create or reset data files:<br><i>Enable if using a new location</i></th>
						<td>
       		       					<input type="radio" name="rstats_new" class="input" value="1" <% nvram_match_x("", "rstats_new", "1", "checked"); %>><#checkbox_Yes#>
	        		        		<input type="radio" name="rstats_new" class="input" value="0" <% nvram_match_x("", "rstats_new", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
					<tr>
				        	<th>Starting day of monthly cycle</th>
			        		<td><input type="text" maxlength="2" class="input_3_table" name="rstats_offset" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 31)" value="<% nvram_get("rstats_offset"); %>"></td>
			        	</tr>
					<tr id="cstats_enable_tr">
			        		<th>Enable IPTraffic (per IP monitoring)</i></th>
				        	<td>
	       		       				<input type="radio" name="cstats_enable" class="input" value="1" <% nvram_match_x("", "cstats_enable", "1", "checked"); %> onclick="hide_cstats(this.value);"><#checkbox_Yes#>
							<input type="radio" name="cstats_enable" class="input" value="0" <% nvram_match_x("", "cstats_enable", "0", "checked"); %> onclick="hide_cstats(this.value);"><#checkbox_No#>
						</td>
					</tr>
					<tr id="cstats_1_tr">
						<th>Create or reset IPTraffic data files</th>
						<td>
							<input type="radio" name="cstats_new" class="input" value="1" <% nvram_match_x("", "cstats_new", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="cstats_new" class="input" value="0" <% nvram_match_x("", "cstats_new", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
					<tr id="cstats_2_tr">
						<th>Monitor all IPs by default</th>
						<td>
							<input type="radio" name="cstats_all" class="input" value="1" <% nvram_match_x("", "cstats_all", "1", "checked"); %> onclick="hide_cstats_ip(this.value);"><#checkbox_Yes#>
							<input type="radio" name="cstats_all" class="input" value="0" <% nvram_match_x("", "cstats_all", "0", "checked"); %> onclick="hide_cstats_ip(this.value);"><#checkbox_No#>
						</td>
        			</tr>
					<tr id="cstats_inc_tr">
						<th>List of IPs to monitor (comma-separated):</th>
						<td>
							<input type="text" maxlength="512" class="input_32_table" name="cstats_include" onKeyPress="return validate_iplist(this,event);" onchange="update_filter(this,this.value);" value="<% nvram_get("cstats_include"); %>">
						</td>
					</tr>
					<tr id="cstats_exc_tr">
						<th>List of IPs to exclude (comma-separated):</th>
						<td>
							<input type="text" maxlength="512" class="input_32_table" name="cstats_exclude" onKeyPress="return validate_iplist(this,event);" onchange="update_filter(this,this.value);" value="<% nvram_get("cstats_exclude"); %>">
						</td>
					</tr>

				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
                                        <thead>
						<tr>
							<td colspan="2">Miscellaneous Options</td>
						</tr>
					</thead>

					<tr>
						<th>Resolve IPs on active connections list:<br><i>Can considerably slow down the display</i></th>
						<td>
							<input type="radio" name="webui_resolve_conn" class="input" value="1" <% nvram_match_x("", "webui_resolve_conn", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="webui_resolve_conn" class="input" value="0" <% nvram_match_x("", "webui_resolve_conn", "0", "checked"); %>><#checkbox_No#>
						</td>
	                                </tr>

					<tr>
						<th>Disk spindown idle time (in seconds)<br><i>0 = disable feature</i></th>
						<td>
							<input type="text" maxlength="6" class="input_12_table"name="usb_idle_timeout" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 0, 43200)"value="<% nvram_get("usb_idle_timeout"); %>">
						</td>
					</tr>
					<tr>
						<th>Exclude the following drives from spinning down</th>
						<td>
							<input type="checkbox" name="usb_idle_exclude_a">sda</input>
							<input type="checkbox" name="usb_idle_exclude_b">sdb</input>
							<input type="checkbox" name="usb_idle_exclude_c">sdc</input>
							<input type="checkbox" name="usb_idle_exclude_d">sdd</input>
							<input type="checkbox" name="usb_idle_exclude_e">sde</input>
							<input type="checkbox" name="usb_idle_exclude_f">sdf</input>
							<input type="checkbox" name="usb_idle_exclude_g">sdg</input>
							<input type="checkbox" name="usb_idle_exclude_h">sdh</input>
							<input type="checkbox" name="usb_idle_exclude_i">sdi</input>
						</td>
					</tr>
					<tr>
						<th>Stealth Mode (disable all LEDs)</th>
						<td>
							<input type="radio" name="led_disable" class="input" value="1" <% nvram_match_x("", "led_disable", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="led_disable" class="input" value="0" <% nvram_match_x("", "led_disable", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">TCP/IP settings</td>
						</tr>
					</thead>
 					<tr>
						<th>TCP connections limit</th>
						<td>
							<input type="text" maxlength="6" class="input_12_table" name="ct_max" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 256, 300000)" value="<% nvram_get("ct_max"); %>">
						</td>
						</tr>

						<tr>
							<th>TCP Timeout: Established</th>
							<td>
								<input type="text" maxlength="6" class="input_6_table" name="tcp_established" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 432000)" value="">
								<span id="ct_established_default">Default: 1200</span>
							</td>

						</tr>

 						<tr>
							<th>TCP Timeout: syn_sent</th>
							<td>
 								<input type="text" maxlength="5" class="input_6_table" name="tcp_syn_sent" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: syn_recv</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_syn_recv" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 60</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: fin_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_fin_wait" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: time_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_time_wait" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: close</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_close" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 10</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: close_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_close_wait" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 60</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: last_ack</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_last_ack" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 30</span>
							</td>
						</tr>

						<tr>
							<th>UDP Timeout: Assured</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="udp_assured" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 180</span>
							</td>
						</tr>

						<tr>
							<th>UDP Timeout: Unreplied</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="udp_unreplied" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1,86400)" value="">
								<span>Default: 30</span>
							</td>
						</tr>
					</table>
					<div class="apply_gen">
						<input name="button" type="button" class="button_gen" onclick="validate();" value="<#CTL_apply#>"/>
			        </div>
				</td></tr>
	        </tbody>
            </table>
            </td>

       </tr>
      </table>
      <!--===================================Ending of Main Content===========================================-->
    </td>
    <td width="10" align="center" valign="top">&nbsp;</td>
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

