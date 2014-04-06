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
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/general.js"></script>
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
.rsstatus_gif_Img_L{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -0px; width: 59px; height: 38px;
}
.rsstatus_gif_Img_LR{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -47px; width: 59px; height: 38px;
}
.rsstatus_gif_Img_R{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -97px; width: 59px; height: 38px;
}
.rsstatus_png_Img_error{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -0px; width: 59px; height: 38px;
}
.rsstatus_png_Img_L_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -47px; width: 59px; height: 38px;
}
.rsstatus_png_Img_R_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -95px; width: 59px; height: 38px;
}
.rsstatus_png_Img_LR_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -142px; width: 59px; height: 38px;
}

.status_gif_Img_L{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -0px; width: 59px; height: 38px;
}
.status_gif_Img_LR{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -47px; width: 59px; height: 38px;
}
.status_gif_Img_R{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -97px; width: 59px; height: 38px;
}

.status_png_Img_error{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -0px; width: 59px; height: 38px;
}
.status_png_Img_L_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -47px; width: 59px; height: 38px;
}
.status_png_Img_R_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -95px; width: 59px; height: 38px;
}
.status_png_Img_LR_ok{
	background-image:url(images/cloudsync/left_right_done.png);
	background-position: -0px -142px; width: 59px; height: 38px;
}

.FormTable th {
	font-size: 14px;
}

.invitepopup_bg{
	position:absolute;	
	margin: auto;
	top: 0;
	left: 0;
	width:100%;
	height:100%;
	background-color: #444F53;
	filter:alpha(opacity=94);  /*IE5、IE5.5、IE6、IE7*/
	background-repeat: repeat;
	overflow:hidden;
	display:none;
	z-index:998;
	visibility:visible;
	background:rgba(0, 0, 0, 0.85) none repeat scroll 0 0 !important;
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

// invitation
var getflag = '<% get_parameter("flag"); %>';
var decode_flag = f23.s52d(getflag);
var decode_array = decode_flag.split(">");
var isInvite = false;
if(decode_array.length == 4)
	isInvite = true;

var cloud_status = "";
var cloud_obj = "";
var cloud_msg = "";
var cloud_fullcapa="";
var cloud_usedcapa="";
<% UI_cloud_status(); %>

var rs_rulenum = 0;
var rs_status = "";
var rs_obj = "";
var rs_msg = "";
var rs_fullcapa="0";
var rs_usedcapa="0";
<% UI_rs_status(); %>

var curRule = -1;
var enable_cloudsync = '<% nvram_get("enable_cloudsync"); %>';
var cloud_sync =decodeURIComponent('<% nvram_char_to_ascii("","cloud_sync"); %>');
/* type>user>password>url>rule>dir>enable */
var cloud_synclist_array = cloud_sync.replace(/>/g, "&#62").replace(/</g, "&#60"); 
var cloud_synclist_all = new Array(); 
var showEditTable = 0;
var isonEdit = -1;
var rulenum;
var disk_flag=0;
var FromObject = "0";
var lastClickedObj = 0;
var _layer_order = "";
var PROTOCOL = "cifs";
window.onresize = cal_panel_block;

function initial(){
	show_menu();	
	showAddTable();
	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
	setTimeout("showcloud_synclist();", 300);

	if(!rrsut_support)
		$("rrsLink").style.display = "none";
	else{
		if(getflag != ""){
			setTimeout("showInvitation(getflag);", 300);
		}
	}
}

// Invitation
function showInvitation(){
	if(window.scrollTo)
		window.scrollTo(0,0);

	cal_panel_block();
	if(!isInvite){
		$("invitationInfo").innerHTML = "<br/> <#aicloud_invitation_invalid#>";
		$j("#invitation").fadeIn(300);
		$j("#invitationBg").fadeIn(300);
	}
	else{
		var htmlCode = "";
		htmlCode += "<table width='98%' style='margin-top:15px'>";
		htmlCode += "<tr height='40px'><td width='30%'>Descript</td><td style='font-weight:bolder;'>" + decode_array[0] + "</td></tr>";
		htmlCode += "<tr height='40px'><td width='30%'>Rule</td><td style='font-weight:bolder;'>" + parseRule(decode_array[2]) + "</td></tr>";
		htmlCode += "<tr height='40px'><td width='30%'>Destination</td><td style='font-weight:bolder;'>" + decode_array[1] + "</td></tr>";
		
		htmlCode += "<tr height='40px'><td width='30%'>Local path</td><td>";
		htmlCode += "<input type='text' id='PATH_rs' class='input_20_table' style='margin-left:0px;height:23px;' name='cloud_dir' value='' onclick=''/>";
		htmlCode += "<input name='button' type='button' class='button_gen_short' style='margin-left:5px;' onclick='get_disk_tree();$(\"folderTree_panel\").style.marginLeft+=30;' value='Browser'/>";
		htmlCode += "</td></tr>";
		
		if(decode_array[3] != ""){
			htmlCode += "<tr id='verification' height='40px'><td width='30%'>Verification</td><td><input id='veriCode' type='text' onkeypress='return is_number(this,event)' class='input_6_table' style='margin-left:0px;' maxlength='4' value=''>";
			htmlCode += "<span style='color:#FC0;display:none;margin-left:5px;' id='codeHint'>Invalid verification code!</span></td></tr>";
		}

		htmlCode += "</table>";
	
		$("invitationInfo").innerHTML = htmlCode;
		$j("#invitation").fadeIn(300);
		$j("#invitationBg").fadeIn(300);
	}
}

function parseRule(_rule){
	if(_rule == 2)
		return "Client to host";
	else if(_rule == 1)
		return "Host to client"
	else
		return "Two way sync";
}

function cancel_invitation(){
	$j("#invitation").fadeOut(300);
	$j("#invitationBg").fadeOut(300);
}

function confirm_invitation(){
	if(!isInvite){
		$j("#invitation").fadeOut(300);
		$j("#invitationBg").fadeOut(300);
		return false;
	}

	if($("veriCode")){
		if($("veriCode").value != decode_array[3]){
			$j("#codeHint").fadeOut(300);
			$j("#codeHint").fadeIn(300);
			return false;
		}
	}
	
	if($("PATH_rs")){
		if($("PATH_rs").value == ""){
			alert("<#JS_Shareblanktest#>");
			$("PATH_rs").focus();
			return false;
		}

		if($("PATH_rs").value.search("/tmp") != 0){
			$("PATH_rs").value = "/tmp" + $("PATH_rs").value;
		}		
	}

	if(cloud_sync != "")
		cloud_sync += "<";

	cloud_sync += "1>" + f23.s52d(getflag) + ">" + $("PATH_rs").value;
	cloud_sync.replace(":/", "/");

	document.enableform.cloud_sync.value = cloud_sync;
	document.enableform.cloud_sync.disabled = false;	
	document.enableform.enable_cloudsync.value = 1;
	showLoading();
	document.enableform.submit();
}
// End

function initial_dir(){
	var __layer_order = "0_0";
	var url = "/getfoldertree.asp";
	var type = "General";

	url += "?motion=gettree&layer_order=" + __layer_order + "&t=" + Math.random();
	$j.get(url,function(data){initial_dir_status(data);});
}

function initial_dir_status(data){
	if(data != "" && data.length != 2){
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

function del_Row(_rulenum){
	var cloud_synclist_row;
	cloud_synclist_array = cloud_sync.replace(/>/g, "&#62").replace(/</g, "&#60"); 
	cloud_synclist_row = cloud_synclist_array.split('&#60');
	cloud_synclist_row[_rulenum-1] = "";
	cloud_synclist_array = "";

	for(var i=0; i<cloud_synclist_row.length; i++){
		if(cloud_synclist_row[i] == "")
			continue;
		if(cloud_synclist_array != "")
 			cloud_synclist_array += "<";
		cloud_synclist_array += cloud_synclist_row[i];
	}

	document.form.cloud_sync.value = cloud_synclist_array.replace(/&#62/g, ">").replace(/&#60/g, "<");
	$("update_scan").style.display = '';
	FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
	showLoading();
	document.form.submit();
}

var hasWebStorageAcc = false;
function showcloud_synclist(){
	var rsnum = 0;
	var cloud_synclist_row = cloud_synclist_array.split('&#60');
	var code = "";
	var dir_temp = "";
	var dir_path = "";
	rulenum = 0;

	code +='<table width="99%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cloud_synclist_table">';
	if(enable_cloudsync == '0' && cloud_synclist_row != ""){
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#nosmart_sync#></td>';
		hasWebStorageAcc = true;
	}
	else if(enable_cloudsync == '0'){
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#nosmart_sync#></td>';
	}
	else if($("usb_status").className == "usbstatusoff")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#no_usb_found#></td>';
	else if(cloud_synclist_array == "")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 0; i < cloud_synclist_row.length; i++){
			rulenum++;
			code +='<tr id="row'+i+'" height="55px">';
			var cloud_synclist_col = cloud_synclist_row[i].split('&#62');
			var wid = [10, 25, 0, 0, 10, 30, 15];

			if(cloud_synclist_col[0] == 0){ // ASUS WebStorage
				$("creatBtn").style.display = "none";
				hasWebStorageAcc = true;
				cloud_synclist_all[i] = cloud_synclist_col;
				for(var j = 0; j < cloud_synclist_col.length; j++){
					if(j == 2 || j == 3){ 
						continue;
					}
					else{
						if(j == 0)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/ASUS-WebStorage.png"></td>';
						else if(j == 1){
							if(cloud_synclist_col[j].length > 20)
								code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><span style="font-size:16px;font-family: Calibri;font-weight: bolder;text-decoration:underline; cursor:pointer"  onclick="isonEdit='+rulenum+';showEditTable=1;showAddTable('+rulenum+');" title='+cloud_synclist_col[j]+'>'+ cloud_synclist_col[j].substring(0, 17) +'...</span></td>';
							else
								code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><span style="font-size:16px;font-family: Calibri;font-weight: bolder;text-decoration:underline; cursor:pointer"  onclick="isonEdit='+rulenum+';showEditTable=1;showAddTable('+rulenum+');">'+ cloud_synclist_col[j] +'</span></td>';
						}	
						else if(j == 4){
							curRule = cloud_synclist_col[j];
							if(cloud_synclist_col[j] == 2)
								code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div class="status_gif_Img_L"></div></div></td>';//code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img id="statusImg" width="45px" src="/images/cloudsync/left.gif"></td>';
							else if(cloud_synclist_col[j] == 1)
								code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div class="status_gif_Img_R"></div></div></td>';
							else
								code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="status_image"><div class="status_gif_Img_LR"></div></div></td>';
						}
						else if(j == 6){
							code +='<td width="'+wid[j]+'%" id="cloudStatus"></td>';
						}
						else{
							dir_temp = cloud_synclist_col[j].split("/");
							if(dir_temp[dir_temp.length-1].length > 21){
								dir_path = "/" + dir_temp[3] + "/" +dir_temp[4].substring(0,18) + "...";
								code +='<td width="'+wid[j]+'%"><span style="display:none;">'+ cloud_synclist_col[j] +'</span><span style="word-break:break-all;" title='+cloud_synclist_col[j].substr(8, cloud_synclist_col[j].length)+'>'+ dir_path +'</span></td>';
							}	
							else{
								dir_path = cloud_synclist_col[j].substr(8, cloud_synclist_col[j].length);
								code +='<td width="'+wid[j]+'%"><span style="display:none;">'+ cloud_synclist_col[j] +'</span><span style="word-break:break-all;" >'+ dir_path +'</span></td>';
							}		
						}
					}
				}
				code += '<td width="10%"><input class="remove_btn" onclick="del_Row('+rulenum+');" value=""/></td>';
			}
			else if(cloud_synclist_col[0] == 1){ // Router Sync
				for(var j = 0; j < cloud_synclist_col.length; j++){
					if(j == 0)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/rssync.png"></td>';
					else if(j == 1)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><span style="font-size:16px;font-family:Calibri;font-weight:bolder;" onclick="">'+ cloud_synclist_col[j] +'</span></td>';
					else if(j == 3){
						curRule = cloud_synclist_col[j];
						if(cloud_synclist_col[j] == 2)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="rsstatus_image_'+rsnum+'"><div class="rsstatus_gif_Img_L"></div></div></td>';//code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img id="rsstatusImg" width="45px" src="/images/cloudsync/left.gif"></td>';
						else if(cloud_synclist_col[j] == 1)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="rsstatus_image_'+rsnum+'"><div class="rsstatus_gif_Img_R"></div></div></td>';
						else
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><div id="rsstatus_image_'+rsnum+'"><div class="rsstatus_gif_Img_LR"></div></div></td>';
					}
					else if(j == 5)
						code +='<td width="'+wid[j]+'%"><span style="display:none;">'+ cloud_synclist_col[j] +'</span><span style="word-break:break-all;">'+ cloud_synclist_col[j].substr(4, cloud_synclist_col[j].length) +'</span></td>';
				}				
				code +='<td width="'+wid[j]+'%" id="rsStatus_'+rsnum+'">Waiting...</td>';
				code += '<td width="10%"><input class="remove_btn" onclick="del_Row('+rulenum+');" value=""/></td>';
				rsnum++;
			}
			else{
				code += '';
			}

			if(updateCloudStatus_counter == 0){
				updateCloudStatus();
				updateCloudStatus_counter++;
			}
		}
	}
	code +='</table>';
	$("cloud_synclist_Block").innerHTML = code;
}

var updateCloudStatus_counter = 0;
var captcha_flag = 0;
function updateCloudStatus(){
    $j.ajax({
    	url: '/cloud_status.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		updateCloudStatus();
    	},
    	success: function(response){
					// webstorage
					if($("cloudStatus")){
						if(cloud_status.toUpperCase() == "DOWNUP"){
							cloud_status = "SYNC";
							$("status_image").firstChild.className="status_gif_Img_LR";
						}
						else if(cloud_status.toUpperCase() == "ERROR"){
							$("status_image").firstChild.className="status_png_Img_error";
						}
						else if(cloud_status.toUpperCase() == "INPUT CAPTCHA"){
							$("status_image").firstChild.className="status_png_Img_error";
						}
						else if(cloud_status.toUpperCase() == "UPLOAD"){
							$("status_image").firstChild.className="status_gif_Img_L";
						}
						else if(cloud_status.toUpperCase() == "DOWNLOAD"){
							$("status_image").firstChild.className="status_gif_Img_R";
						}
						else if(cloud_status.toUpperCase() == "SYNC"){
							cloud_status = "Finish";
							if(curRule == 2){
								$("status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule == 1){
								$("status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								$("status_image").firstChild.className="status_png_Img_LR_ok";
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
							if(cloud_msg == "Need to enter the CAPTCHA"){
								if(captcha_flag == 0){
									showEditTable = 1;
									showAddTable(1);
									$('captcha_tr').style.display = "";															
									autoFocus('captcha_field');	
									$('captcha_iframe').src= CAPTCHA_URL;						
									captcha_flag = 1;
								}
							}
							_cloud_msg += cloud_msg;
						}
						else{
							_cloud_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						var _cloud_status;
						if(cloud_status != "")
							_cloud_status = cloud_status;
						else
							_cloud_status = "";
	
						$("cloudStatus").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_msg +'\');">'+ _cloud_status +'</div>';
					}

					// Router Sync
					if(rs_rulenum == "") rs_rulenum = 0;
					if($("rsStatus_"+rs_rulenum)){
						if(rs_status.toUpperCase() == "DOWNUP"){
							rs_status = "SYNC";
							$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_gif_Img_LR";
						}
						else if(rs_status.toUpperCase() == "ERROR"){
							$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_png_Img_error";
						}
						else if(rs_status.toUpperCase() == "UPLOAD"){
							$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_gif_Img_L";
						}
						else if(rs_status.toUpperCase() == "DOWNLOAD"){
							$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_gif_Img_R";
						}
						else if(rs_status.toUpperCase() == "SYNC"){
							rs_status = "Finish";
							if(curRule == 2){
								$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_png_Img_L_ok";
							}else if(curRule == 1){
								$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_png_Img_R_ok";
							}else{
								$("rsstatus_image_"+rs_rulenum).firstChild.className="rsstatus_png_Img_LR_ok";
							}	
						}
	
						// handle msg
						var _rs_msg = "";
						if(rs_obj != ""){
							_rs_msg +=  "<b>";
							_rs_msg += rs_status;
							_rs_msg += ": </b><br />";
							_rs_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponent(rs_obj) + "</span>";
						}
						else if(rs_msg){
							_rs_msg += rs_msg;
						}
						else{
							_rs_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						$("rsStatus_"+rs_rulenum).innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _rs_msg +'\');">'+ rs_status +'</div>';
					}

			 		setTimeout("updateCloudStatus();", 1500);
      }
   });
}

function validform(){
	if(!validate_string(document.form.cloud_dir))
		return false;

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

	/*if(document.form.cloud_password.value.length < 8){ //disable to check length of password temporary, Jieming added at 2013.08.13
		alert("<#cloud_list_password#>");
		return false;
	}*/
	
	if(document.form.cloud_dir.value.split("/").length < 4 || document.form.cloud_dir.value == ''){
		alert("<#ALERT_OF_ERROR_Input10#>");
		return false;
	}
	return true;
}

function applyRule(){
	if(validform()){
		var cloud_synclist_row;
		var cloud_synclist_col;
		var cloud_synclist_array_tmp = cloud_sync; 

		if(isonEdit != -1){
			cloud_synclist_row = cloud_synclist_array_tmp.split('<');
			cloud_synclist_row[isonEdit-1] = "";

			// rebuild cloud_synclist_array_tmp
			cloud_synclist_array_tmp = "";		
			for(var i=0; i<cloud_synclist_row.length; i++){
				if(cloud_synclist_row[i] == "")
					continue;
				if(cloud_synclist_array_tmp != "")
		 			cloud_synclist_array_tmp += "<";
				cloud_synclist_array_tmp += cloud_synclist_row[i];
			}
		}
		
		if(cloud_sync != ""){
			cloud_synclist_row = cloud_synclist_array_tmp.split('<');
			for(i=0;i< cloud_synclist_row.length;i++){
				cloud_synclist_col = cloud_synclist_row[i].split('>');
				if(document.form.cloud_username.value == cloud_synclist_col[1])
					cloud_synclist_array_tmp = "";
				else
					cloud_synclist_array_tmp += '<';			
			}
		}

		if(document.form.captcha_field.value == "" && document.form.security_code_field.value == "")
			cloud_synclist_array_tmp += '0>'+document.form.cloud_username.value+'>'+document.form.cloud_password.value+'>none>'+document.form.cloud_rule.value+'>'+"/tmp"+document.form.cloud_dir.value+'>1';
		else
			cloud_synclist_array_tmp += '0>'+document.form.cloud_username.value+'>'+document.form.cloud_password.value+'>'+document.form.security_code_field.value +'#'+ document.form.captcha_field.value+'>'+document.form.cloud_rule.value+'>'+"/tmp"+document.form.cloud_dir.value+'>1';

		
		showcloud_synclist();
		document.form.cloud_sync.value = cloud_synclist_array_tmp;
		document.form.cloud_username.value = '';
		document.form.cloud_password.value = '';
		document.form.cloud_rule.value = '';
		document.form.cloud_dir.value = '';
		document.form.enable_cloudsync.value = 1;
		showEditTable = 0;
		showAddTable();
		$("update_scan").style.display = '';
		FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
		showLoading();

		if(document.form.cloud_sync.value.charAt(0) == "<")
			document.form.cloud_sync.value = document.form.cloud_sync.value.substring(1);

		document.form.submit();
	}
}

function showAddTable(r){
	if(showEditTable == 1){ // edit
		$j("#cloudAddTable").fadeIn();
		$("creatBtn").style.display = "none";
		$j("#applyBtn").fadeIn();

		if(typeof r != "undefined"){
			edit_Row(r-1);
		}
	}
	else{ // list
		$("cloudAddTable").style.display = "none";
		if(!hasWebStorageAcc){
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
			//layer3_path = "/" + $(obj.id).innerHTML;
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
			//layer2_path = "/" + $(obj.id).innerHTML;
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
	if($('PATH_rs'))
		$('PATH_rs').value = path_directory ;
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
	$("invitation").style.marginLeft = blockmarginLeft+"px";
}

var captcha_flag = 0;
function refresh_captcha(){
	if(captcha_flag == 0){
		var captcha_url = 'http://sg03.asuswebstorage.com/member/captcha/?userid='+document.form.cloud_username.value;
		$('captcha_iframe').setAttribute("src", captcha_url);
	}
	else{
		document.getElementById('captcha_iframe').src = document.getElementById('captcha_iframe').src;
	}
}

</script>
</head>

	<body onload="initial();" onunload="return unload_body();">
	<div id="TopBanner"></div>
	
<div id="invitationBg" class="invitepopup_bg">
	<div id="invitation" class="panel_folder" style="margin-top:30px;">
		<table>
			<tr>
				<td>
					<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:20px;margin-left:30px;">You have got a new invitation!</div>
				</td>
			</tr>
		</table>
		<div id="invitationInfo" class="folder_tree" style="word-break:break-all;z-index:999;"></div>
		<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">		
			<input class="button_gen" type="button" style="margin-left:27%;margin-top:18px;" onclick="cancel_invitation();" value="<#CTL_Cancel#>">
			<input class="button_gen" type="button"  onclick="confirm_invitation();" value="<#CTL_ok#>">
		</div>
	</div>
</div>

<!-- floder tree-->
<div id="DM_mask" class="mask_bg"></div>
<div id="folderTree_panel" class="panel_folder" style="z-index:1000;">
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
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_sync.asp">
<input type="hidden" name="next_page" value="cloud_sync.asp">
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
							<a id="rrsLink" href="cloud_router_sync.asp"><div class="tab"><span>Sync Server</span></div></a>
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
	   						<td colspan="6" id="cloud_synclist"><#aicloud_cloud_list#></td>
	   					</tr>
	  					</thead>		  

    					<tr>
      					<th width="10%"><#Provider#></th>
    						<th width="25%"><#PPPConnection_UserName_itemname#></a></th>
      					<th width="10%"><#Cloudsync_Rule#></a></th>
      					<th width="30%"><#FolderName#></th>
      					<th width="15%"><#PPPConnection_x_WANLink_itemname#></th>
      					<th width="10%"><#CTL_del#></th>
    					</tr>

						</table>
	
						<div id="cloud_synclist_Block">
							<table width="99%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cloud_synclist_table">
								<tbody>
									<tr height="55px">
										<td style="color:#FFCC00;" colspan="6">Detecting...</td>
									</tr>
								</tbody>
							</table>
						</div>

					  <table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="cloudAddTable" style="margin-top:10px;display:none;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist"><#aicloud_cloud_list#></td>
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
							  <input type="text" maxlength="32" class="input_30_table" style="height: 23px;" id="cloud_username" name="cloud_username" value="" onKeyPress="">
							</td>
						  </tr>	

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#PPPConnection_Password_itemname#>
							</th>			
							<td>
								<input id="cloud_password" name="cloud_password" type="password" autocapitalization="off" onBlur="switchType(this, false);" onFocus="switchType(this, true);" maxlength="25" class="input_30_table" style="height: 23px;" value="">
							</td>
						  </tr>						  				
					  				
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Folder
							</th>
							<td>
			          <input type="text" id="PATH" class="input_30_table" style="height: 23px;" name="cloud_dir" value="" onclick=""/>
		  					<input name="button" type="button" class="button_gen" onclick="get_disk_tree();" value="Browser"/>
								<div id="noUSB" style="color:#FC0;display:none;margin-left: 3px;"><#no_usb_found#></div>
							</td>
						  </tr>

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Rule
							</th>
							<td>
								<select name="cloud_rule" class="input_option">
									<option value="0"><#Cloudsync_Rule_sync#></option>
									<option value="1"><#Cloudsync_Rule_dl#></option>
									<option value="2"><#Cloudsync_Rule_ul#></option>
								</select>			
							</td>
						  </tr>
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Security code
							</th>
							<td>
								<div style="color:#FC0;"><input id="security_code_field" name="security_code_field" type="text" maxlength="6" class="input_32_table" style="height: 23px;width:100px;margin-right:10px;" >OTP Authentication</div>
							</td>
						  </tr>
						  <tr height="45px;" id="captcha_tr" style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Captcha
							</th>			
							<td style="height:85px;">
								<div style="height:25px;"><input id="captcha_field" name="captcha_field" type="text" maxlength="6" class="input_32_table" style="height: 23px;width:100px;margin-top:8px;" ></div>
								<div id="captcha_hint" style="color:#FC0;height:25px;margin-top:10px;">Please input the captcha</div>						
								<div>
									<iframe id="captcha_iframe" frameborder="0" scrolling="no" src="" style="width:230px;height:80px;*width:210px;*height:87px;margin:-60px 0 0 160px;*margin-left:165px;"></iframe>
								</div>
								<div style="color:#FC0;text-decoration:underline;height:35px;margin:-35px 0px 0px 380px;cursor:pointer" onclick="refresh_captcha();"><#CTL_refresh#></div>   
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
							<input name="applybutton" id="applybutton" type="button" class="button_gen_long" onclick="showEditTable=1;showAddTable();" value="<#AddAccountTitle#>" style="word-wrap:break-word;word-break:normal;">
							<img id="update_scan" style="display:none;" src="images/InternetScan.gif" />
	  				</div>

	  				<div class="apply_gen" style="margin-top:30px;display:none;" id="applyBtn">
	  					<input name="button" type="button" class="button_gen" onclick="showEditTable=0;showAddTable();" value="<#CTL_Cancel#>"/>
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
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_cloudsync">
<input type="hidden" name="action_wait" value="2">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
<input type="hidden" name="cloud_sync" value="" disabled>
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
