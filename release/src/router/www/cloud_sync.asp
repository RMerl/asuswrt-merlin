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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/form.js"></script>
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

.status_gif_Img_2{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -0px; width: 59px; height: 38px;
}
.status_gif_Img_1{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -97px; width: 59px; height: 38px;
}
.status_gif_Img_0{
	background-image:url(images/cloudsync/left_right_trans.gif);
	background-position: 10px -47px; width: 59px; height: 38px;
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
.cloudListUserName{
	font-size:16px;
	font-family: Calibri;
	font-weight: bolder;
	text-decoration:underline; 
	cursor:pointer;
}
.contentM_qis{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	margin-left:226px;
	margin-top: 10px;
	width:740px;
	box-shadow: 3px 3px 10px #000;
	display: none;
}
</style>
<script>

window.onresize = function() {
	if(document.getElementById("cloudAddTable_div").style.display == "block") {
		cal_panel_block("cloudAddTable_div", 0.2);
	}
	if(document.getElementById("folderTree_panel").style.display == "block") {
		cal_panel_block("folderTree_panel", 0.25);
	}
	if(document.getElementById("invitation").style.display == "block") {
		cal_panel_block("invitation", 0.25);
	}
}
<% get_AiDisk_status(); %>
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

var curRule = {
	WebStorage: -1,
	RouterSync: -1,
	Dropbox:-1,
	SambaClient: -1,
	UsbClient: -1,
	end: 0
}

var enable_cloudsync = '<% nvram_get("enable_cloudsync"); %>';
var cloud_sync = decodeURIComponentSafe('<% nvram_char_to_ascii("","cloud_sync"); %>');
var cloud_synclist_array = cloud_sync.replace(/>/g, "&#62").replace(/</g, "&#60"); 
var cloud_synclist_all = new Array(); 
var editRule = -1;
var rulenum;
var disk_flag=0;
var FromObject = "0";
var lastClickedObj = 0;
var _layer_order = "";
var PROTOCOL = "cifs";

function initial(){
	show_menu();	
	showAddTable();
	document.aidiskForm.protocol.value = PROTOCOL;
	initial_dir();
	setTimeout("showcloud_synclist();", 300);

	if(!rrsut_support)
		document.getElementById("rrsLink").style.display = "none";
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

	cal_panel_block("invitation", 0.25);
	if(!isInvite){
		document.getElementById("invitationInfo").innerHTML = "<br/> <#aicloud_invitation_invalid#>";
		$("#invitation").fadeIn(300);
		$("#invitationBg").fadeIn(300);
	}
	else{
		var htmlCode = "";
		htmlCode += "<table width='98%' style='margin-top:15px'>";
		htmlCode += "<tr height='40px'><td width='30%'><#IPConnection_autofwDesc_itemname#></td><td style='font-weight:bolder;'>" + decode_array[0] + "</td></tr>";
		htmlCode += "<tr height='40px'><td width='30%'><#Cloudsync_Rule#></td><td style='font-weight:bolder;'>" + parseRule(decode_array[2]) + "</td></tr>";
		htmlCode += "<tr height='40px'><td width='30%'>Destination</td><td style='font-weight:bolder;'>" + decode_array[1] + "</td></tr>";
		
		htmlCode += "<tr height='40px'><td width='30%'><#sync_router_localfolder#></td><td>";
		htmlCode += "<input type='text' id='PATH_rs' class='input_20_table' style='margin-left:0px;height:23px;' name='cloud_dir' value='' onclick=''/>";
		htmlCode += "<input name='button' type='button' class='button_gen_short' style='margin-left:5px;' onclick='get_disk_tree(1);document.getElementById(\"folderTree_panel\").style.marginLeft+=30;' value='<#Cloudsync_browser_folder#>'/>";
		htmlCode += "</td></tr>";
		
		if(decode_array[3] != ""){
			htmlCode += "<tr id='verification' height='40px'><td width='30%'>Verification</td><td><input id='veriCode' type='text' onkeypress='return validator.isNumber(this,event)' class='input_6_table' style='margin-left:0px;' maxlength='4' value=''>";
			htmlCode += "<span style='color:#FC0;display:none;margin-left:5px;' id='codeHint'><#JS_Invalid_Vcode#></span></td></tr>";
		}

		htmlCode += "</table>";
	
		document.getElementById("invitationInfo").innerHTML = htmlCode;
		$("#invitation").fadeIn(300);
		$("#invitationBg").fadeIn(300);
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
	$("#invitation").fadeOut(300);
	$("#invitationBg").fadeOut(300);
}

function confirm_invitation(){
	if(!isInvite){
		$("#invitation").fadeOut(300);
		$("#invitationBg").fadeOut(300);
		return false;
	}

	if(document.getElementById("veriCode")){
		if(document.getElementById("veriCode").value != decode_array[3]){
			$("#codeHint").fadeOut(300);
			$("#codeHint").fadeIn(300);
			return false;
		}
	}
	
	if(document.getElementById("PATH_rs")){
		if(document.getElementById("PATH_rs").value == ""){
			alert("<#JS_Shareblanktest#>");
			document.getElementById("PATH_rs").focus();
			return false;
		}

		if(document.getElementById("PATH_rs").value.search("/tmp") != 0){
			document.getElementById("PATH_rs").value = "/tmp" + document.getElementById("PATH_rs").value;
		}		
	}

	if(cloud_sync != "")
		cloud_sync += "<";

	cloud_sync += "1>" + f23.s52d(getflag) + ">" + document.getElementById("PATH_rs").value;
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
	$.get(url,function(data){initial_dir_status(data.split(",")[0]);});
}

function initial_dir_status(data){
	if(data != "" && data.length != 2){
		var default_dir = data.replace(/\"/g, "");
		document.form.cloud_dir.value = "/mnt/" + default_dir.substr(0, default_dir.indexOf("#")) + "/MySyncFolder";
	}
	else{
		document.getElementById("noUSB0").style.display = "";	
		document.getElementById("noUSB").style.display = "";
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
	var rule_num = document.getElementById('cloud_synclist_table').rows.length;
	var item_num = document.getElementById('cloud_synclist_table').rows[0].cells.length;
	
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
	var showOneProvider = function (imgName, providerName) {
		var htmlCode = '<div><img style="margin-top: -2px;" src="'+ imgName +'"></div>';
		htmlCode+= '<div style="font-size:18px;font-weight: bolder;margin-left: 45px;margin-top: -27px;font-family: Calibri;">'+ providerName +'</div>';

		document.getElementById("divOneProvider").innerHTML = htmlCode;
		document.getElementById("divOneProvider").style.display = "";
		document.getElementById("povider_tr").style.display = "none";
	};

	if(cloud_synclist_all == "")
		return true;
		
	if(cloud_synclist_all[r][0] == 0){
		change_service("WebStorage");
		document.form.cloud_username.value = cloud_synclist_all[r][1];
		document.form.cloud_password.value = cloud_synclist_all[r][2];
		document.form.cloud_rule.value = cloud_synclist_all[r][4];
		document.form.cloud_dir.value = cloud_synclist_all[r][5].substring(4);	
		showOneProvider("/images/cloudsync/ASUS-WebStorage.png", "ASUS WebStorage");
	}
	else if(cloud_synclist_all[r][0] == 3){
		change_service("Dropbox");
		document.form.cloud_username.value = cloud_synclist_all[r][2];
		document.form.cloud_password.value = cloud_synclist_all[r][3];
		document.form.cloud_rule.value = cloud_synclist_all[r][5];
		document.form.cloud_dir.value = cloud_synclist_all[r][6].substring(4);	
		showOneProvider("/images/cloudsync/dropbox.png", "Dropbox");
	}
	else if(cloud_synclist_all[r][0] == 4){
		change_service("Samba");
		document.form.sambaclient_name.value = cloud_synclist_all[r][1];
		document.form.sambaclient_ip.value = cloud_synclist_all[r][2].substring(6);
		document.form.sambaclient_sharefolder.value = cloud_synclist_all[r][3];
		document.form.cloud_username.value = cloud_synclist_all[r][4];
		document.form.cloud_password.value = cloud_synclist_all[r][5];
		document.form.cloud_rule.value = cloud_synclist_all[r][6];
		document.form.cloud_dir.value = cloud_synclist_all[r][7].substring(4);	
		showOneProvider("/images/cloudsync/ftp_server.png", "Samba");
	}
	else if(cloud_synclist_all[r][0] == 5){
		change_service("Usb");
		document.form.usbclient_name.value = cloud_synclist_all[r][1];
		document.form.usbclient_ip.value = cloud_synclist_all[r][2].substring(6);
		document.form.usbclient_sharefolder.value = cloud_synclist_all[r][3];
		document.form.cloud_username.value = cloud_synclist_all[r][4];
		document.form.cloud_password.value = cloud_synclist_all[r][5];
		document.form.cloud_rule.value = cloud_synclist_all[r][6];
		document.form.cloud_dir.value = cloud_synclist_all[r][7].substring(4);	
		showOneProvider("/images/cloudsync/ftp_server.png", "Usb");
	}
	else{
		var ftp_protocol_temp ="";
		change_service("FTP");
		ftp_protocol_temp = cloud_synclist_all[r][4].split(":")[0];			// to check first element ftp or others.
		document.form.cloud_username.value = cloud_synclist_all[r][2];
		document.form.cloud_password.value = cloud_synclist_all[r][3];
		if(ftp_protocol_temp == "ftp"){
			document.form.ftp_url.value = cloud_synclist_all[r][4].substring(6);
			document.getElementById('ftp_protocol')[0].selected = "selected";
		}
		document.form.ftp_root_path.value = cloud_synclist_all[r][5];
		document.form.cloud_rule.value = cloud_synclist_all[r][7];
		document.form.cloud_dir.value = cloud_synclist_all[r][8].substring(4);	
		showOneProvider("/images/cloudsync/ftp_server.png", "FTP Server");
	}
}

function delRow(_rulenum){
	document.form.cloud_sync.value = cloud_sync.split('<').del(_rulenum-1).join("<");
	FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
	showLoading();
	document.form.submit();
}

var iCountSamba = 0;
var iCountUsb = 0;
function showcloud_synclist(){
	var rsnum = 0;
	var cloud_synclist_row = cloud_synclist_array.split('&#60');
	var code = "";
	rulenum = 0;

	code +='<table width="99%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cloud_synclist_table">';
	if(enable_cloudsync == '0' && cloud_synclist_array != "")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#nosmart_sync#></td>';
	else if(document.getElementById("usb_status").className == "usbstatusoff")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#no_usb_found#></td>';
	else if(cloud_synclist_array == "")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 0; i < cloud_synclist_row.length; i++){
			rulenum++;
			code +='<tr id="row'+i+'" height="55px">';
			var cloud_synclist_col = cloud_synclist_row[i].split('&#62');
			cloud_synclist_all[i] = cloud_synclist_col;
			var wid = [10, 25, 10, 30, 15, 10];
			var cloudListTableItem = {
				provider: "",
				icon: "",
				username: "",
				token: "",
				rule: "",
				ruleId: "",
				path: "",
				syncStatus: "",
				syncStatusId: 0,
				syncStatusDefaultStr: " - ",
				end: ""
			}	

			if(cloud_synclist_col[0] == 0){ // ASUS WebStorage
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "ASUS-WebStorage.png";
				cloudListTableItem.username = cloud_synclist_col[1];
				cloudListTableItem.rule = cloud_synclist_col[4];
				cloudListTableItem.ruleId = "status_image";
				cloudListTableItem.path = cloud_synclist_col[5];
				cloudListTableItem.syncStatus = cloud_synclist_col[6];
				cloudListTableItem.syncStatusId = "cloudStatus";
				cloudListTableItem.syncStatusDefaultStr = "Waiting..."
				curRule.WebStorage = cloudListTableItem.rule; 
			}
			else if(cloud_synclist_col[0] == 1){ // Router to router sync
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "rssync.png";
				cloudListTableItem.username = cloud_synclist_col[1];
				cloudListTableItem.rule = cloud_synclist_col[3];
				cloudListTableItem.ruleId = "rsstatus_image_" + rsnum;
				cloudListTableItem.path = cloud_synclist_col[5];
				cloudListTableItem.syncStatusId = "rsStatus_" + rsnum;
				cloudListTableItem.syncStatusDefaultStr = "Waiting..."
				rsnum++;
				curRule.RouterSync = cloudListTableItem.rule;
			}
			else if(cloud_synclist_col[0] == 2){ // FTP Client
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "ftp_server.png";
				if(cloud_synclist_col[2] == "" && cloud_synclist_col[3] == "")
					cloudListTableItem.username = "anonymous";
				else
					cloudListTableItem.username = cloud_synclist_col[2];			
				
				cloudListTableItem.rule = cloud_synclist_col[7];
				cloudListTableItem.ruleId = "ftpclient_status_image";
				cloudListTableItem.path = cloud_synclist_col[8];
				cloudListTableItem.syncStatusId = "cloudStatus_ftpclient";
				cloudListTableItem.syncStatusDefaultStr = "Waiting...";
				curRule.ftpclient = cloudListTableItem.rule; 
			}
			else if(cloud_synclist_col[0] == 3){ // Dropbox
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "dropbox.png";
				cloudListTableItem.username = cloud_synclist_col[2];
				cloudListTableItem.token = cloud_synclist_col[3];
				cloudListTableItem.rule = cloud_synclist_col[5];
				cloudListTableItem.ruleId = "dropbox_status_image";
				cloudListTableItem.path = cloud_synclist_col[6];
				cloudListTableItem.syncStatusId = "cloudStatus_dropbox";
				cloudListTableItem.syncStatusDefaultStr = "Waiting...";
				curRule.Dropbox = cloudListTableItem.rule;
			}
			else if(cloud_synclist_col[0] == 4){ // SambaClient
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "ftp_server.png";
				cloudListTableItem.username = cloud_synclist_col[1];
				cloudListTableItem.rule = cloud_synclist_col[6];
				cloudListTableItem.ruleId = "sambaclient_status_image"
				cloudListTableItem.path = cloud_synclist_col[7];
				cloudListTableItem.syncStatusId = "cloudStatus_sambaclient";
				cloudListTableItem.syncStatusDefaultStr = "Waiting..."
				curRule.SambaClient = cloudListTableItem.rule;
				iCountSamba++;
			}
			else if(cloud_synclist_col[0] == 5){ // UsbClient
				cloudListTableItem.provider = cloud_synclist_col[0];
				cloudListTableItem.icon = "ftp_server.png";
				cloudListTableItem.username = cloud_synclist_col[1];
				cloudListTableItem.rule = cloud_synclist_col[6];
				cloudListTableItem.ruleId = "usbclient_status_image"
				cloudListTableItem.path = cloud_synclist_col[7];
				cloudListTableItem.syncStatusId = "cloudStatus_usbclient";
				cloudListTableItem.syncStatusDefaultStr = "Waiting..."
				curRule.UsbClient = cloudListTableItem.rule;
				iCountUsb++;
			}
			else{
				continue;
			}

			// HTML constructor
			code += '<td width="'+wid[0]+'%"><img src="/images/cloudsync/'+ cloudListTableItem.icon +'"></td>';

			code += '<td width="'+wid[1]+'%"><span';

			if(cloudListTableItem.provider == 3){
				code += ' id="cloudListUserName_' + cloudListTableItem.username + '"';
				getDropBoxClientName(cloudListTableItem.token, cloudListTableItem.username);
			}

			code += ' class="cloudListUserName" onclick="editRule='+rulenum+';showAddTable('+cloudListTableItem.provider+','+i+');" title=' + cloudListTableItem.username + '>' + cloudListTableItem.username.shorter(20) + '</span></td>';

			code += '<td width="'+wid[2]+'%"><div id="' + cloudListTableItem.ruleId + '"><div class="status_gif_Img_' + cloudListTableItem.rule + '"></div></div></td>';
			code += '<td width="'+wid[3]+'%"><span style="word-break:break-all;" title=' + cloudListTableItem.path.substr(8, cloudListTableItem.path.length)+ '>' + cloudListTableItem.path.substr(8, cloudListTableItem.path.length).shorter(20) +'</span></td>';
			code += '<td width="'+wid[4]+'%" id="' + cloudListTableItem.syncStatusId + '">' + cloudListTableItem.syncStatusDefaultStr + '</td>';
			code += '<td width="'+wid[5]+'%"><input class="remove_btn" onclick="delRow('+rulenum+');" value=""/></td>';

			if(updateCloudStatus_counter == 0){
				updateCloudStatus();
				updateCloudStatus_counter++;
			}
		}
	}

	code +='</table>';
	document.getElementById("cloud_synclist_Block").innerHTML = code;
}

function getDropBoxClientName(token, uid){
    $.ajax({
    	url: 'https://api.dropbox.com/1/account/info?access_token=' + token,
    	dataType: 'json', 
    	error: function(xhr){
      		getDropBoxClientName();
    	},
    	success: function(response){
    		if(document.getElementById("cloudListUserName_" + uid)) {
    			document.getElementById("cloudListUserName_" + uid).innerHTML = response.email.shorter(20);
    			document.getElementById("cloudListUserName_" + uid).title = response.email;
    		}
    		else
      			getDropBoxClientName();    			
    	}
    })
}

var updateCloudStatus_counter = 0;
var captcha_flag = 0;
function updateCloudStatus(){
    $.ajax({
    	url: '/cloud_status.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		updateCloudStatus();
    	},
    	success: function(response){
					// webstorage
					if(document.getElementById("cloudStatus")){
						if(cloud_status.toUpperCase() == "DOWNUP"){
							cloud_status = "SYNC";
							document.getElementById("status_image").firstChild.className="status_gif_Img_0";
						}
						else if(cloud_status.toUpperCase() == "ERROR"){
							document.getElementById("status_image").firstChild.className="status_png_Img_error";
						}
						else if(cloud_status.toUpperCase() == "INPUT CAPTCHA"){
							document.getElementById("status_image").firstChild.className="status_png_Img_error";
						}
						else if(cloud_status.toUpperCase() == "UPLOAD"){
							document.getElementById("status_image").firstChild.className="status_gif_Img_2";
						}
						else if(cloud_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("status_image").firstChild.className="status_gif_Img_1";
						}
						else if(cloud_status.toUpperCase() == "SYNC"){
							cloud_status = "Finish";
							if(curRule.WebStorage == 2){
								document.getElementById("status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule.WebStorage == 1){
								document.getElementById("status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("status_image").firstChild.className="status_png_Img_LR_ok";
							}	
						}
	
						// handle msg
						var _cloud_msg = "";
						if(cloud_obj != ""){
							_cloud_msg +=  "<b>";
							_cloud_msg += cloud_status;
							_cloud_msg += ": </b><br />";
							_cloud_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(cloud_obj) + "</span>";
						}
						else if(cloud_msg){
							if(cloud_msg == "Need to enter the CAPTCHA"){
								if(captcha_flag == 0){
									for(var i = 0; i < cloud_synclist_all.length; i += 1) {
										if(cloud_synclist_all[i][0] == 0){ //ASUS WebStorage
											showAddTable(0, i);
											editRule = i + 1;
										}
									}
									document.getElementById('captcha_tr').style.display = "";															
									autoFocus('captcha_field');	
									document.getElementById('captcha_iframe').src = CAPTCHA_URL;
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
	
						document.getElementById("cloudStatus").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_msg +'\');">'+ _cloud_status +'</div>';
					}

					//dropbox
					if(document.getElementById("cloudStatus_dropbox")){
						if( cloud_dropbox_status.toUpperCase() == "DOWNUP"){
							 cloud_dropbox_status = "SYNC";
							document.getElementById("dropbox_status_image").firstChild.className="status_gif_Img_0";
						}
						else if( cloud_dropbox_status.toUpperCase() == "ERROR"){
							document.getElementById("dropbox_status_image").firstChild.className="status_png_Img_error";
						}
						else if( cloud_dropbox_status.toUpperCase() == "INPUT CAPTCHA"){
							document.getElementById("dropbox_status_image").firstChild.className="status_png_Img_error";
						}
						else if( cloud_dropbox_status.toUpperCase() == "UPLOAD"){
							document.getElementById("dropbox_status_image").firstChild.className="status_gif_Img_2";
						}
						else if( cloud_dropbox_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("dropbox_status_image").firstChild.className="status_gif_Img_1";
						}
						else if( cloud_dropbox_status.toUpperCase() == "SYNC"){
							 cloud_dropbox_status = "Finish";
							if(curRule.Dropbox == 2){
								document.getElementById("dropbox_status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule.Dropbox == 1){
								document.getElementById("dropbox_status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("dropbox_status_image").firstChild.className="status_png_Img_LR_ok";
							}	
						}

	
						// handle msg
						var _cloud_dropbox_msg = "";
						if(cloud_dropbox_obj != ""){
							_cloud_dropbox_msg +=  "<b>";
							_cloud_dropbox_msg += cloud_dropbox_status;
							_cloud_dropbox_msg += ": </b><br />";
							_cloud_dropbox_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(cloud_dropbox_obj) + "</span>";
						}
						else if(cloud_dropbox_msg){
							_cloud_dropbox_msg += cloud_dropbox_msg;
						}
						else{
							_cloud_dropbox_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						var _cloud_dropbox_status;
						if(cloud_dropbox_status != "")
							_cloud_dropbox_status = cloud_dropbox_status;
						else
							_cloud_dropbox_status = "";
	
						document.getElementById("cloudStatus_dropbox").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_dropbox_msg +'\');">'+ _cloud_dropbox_status +'</div>';
					}
					
					// ftp client
					if(document.getElementById("cloudStatus_ftpclient")){
						if( cloud_ftpclient_status.toUpperCase() == "DOWNUP"){
							 cloud_ftpclient_status = "SYNC";
							document.getElementById("ftpclient_status_image").firstChild.className="status_gif_Img_0";
						}
						else if( cloud_ftpclient_status.toUpperCase() == "ERROR"){
							document.getElementById("ftpclient_status_image").firstChild.className="status_png_Img_error";
						}
						else if( cloud_ftpclient_status.toUpperCase() == "UPLOAD"){
							document.getElementById("ftpclient_status_image").firstChild.className="status_gif_Img_2";
						}
						else if( cloud_ftpclient_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("ftpclient_status_image").firstChild.className="status_gif_Img_1";
						}
						else if( cloud_ftpclient_status.toUpperCase() == "SYNC"){
							 cloud_ftpclient_status = "Finish";
							if(curRule.ftpclient == 2){
								document.getElementById("ftpclient_status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule.ftpclient == 1){
								document.getElementById("ftpclient_status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("ftpclient_status_image").firstChild.className="status_png_Img_LR_ok";
							}	
						}

	
						// handle msg
						var _cloud_ftpclient_msg = "";
						if(cloud_ftpclient_obj != ""){
							_cloud_ftpclient_msg +=  "<b>";
							_cloud_ftpclient_msg += cloud_ftpclient_status;
							_cloud_ftpclient_msg += ": </b><br />";
							_cloud_ftpclient_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(cloud_ftpclient_obj) + "</span>";
						}
						else if(cloud_dropbox_msg){
							_cloud_ftpclient_msg += cloud_ftpclient_msg;
						}
						else{
							_cloud_ftpclient_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						var _cloud_ftpclient_status;
						if(cloud_ftpclient_status != "")
							_cloud_ftpclient_status = cloud_ftpclient_status;
						else
							_cloud_ftpclient_status = "";
	
						document.getElementById("cloudStatus_ftpclient").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_ftpclient_msg +'\');">'+ _cloud_ftpclient_status +'</div>';
					}

					// usb client
					
					if(document.getElementById("cloudStatus_usbclient")){
						if( cloud_usbclient_status.toUpperCase() == "DOWNUP"){
							 cloud_usbclient_status = "SYNC";
							document.getElementById("usbclient_status_image").firstChild.className="status_gif_Img_0";
						}
						else if( cloud_usbclient_status.toUpperCase() == "ERROR"){
							document.getElementById("usbclient_status_image").firstChild.className="status_png_Img_error";
						}
						else if( cloud_usbclient_status.toUpperCase() == "UPLOAD"){
							document.getElementById("usbclient_status_image").firstChild.className="status_gif_Img_2";
						}
						else if( cloud_usbclient_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("usbclient_status_image").firstChild.className="status_gif_Img_1";
						}
						else if( cloud_usbclient_status.toUpperCase() == "SYNC"){
							 cloud_usbclient_status = "Finish";
							if(curRule.usbclient == 2){
								document.getElementById("usbclient_status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule.usbclient == 1){
								document.getElementById("usbclient_status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("usbclient_status_image").firstChild.className="status_png_Img_LR_ok";
							}	
						}

	
						// handle msg
						var _cloud_usbclient_msg = "";
						if(cloud_usbclient_obj != ""){
							_cloud_usbclient_msg +=  "<b>";
							_cloud_usbclient_msg += cloud_usbclient_status;
							_cloud_usbclient_msg += ": </b><br />";
							_cloud_usbclient_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(cloud_usbclient_obj) + "</span>";
						}
						else if(cloud_usbclient_msg){
							_cloud_usbclient_msg += cloud_usbclient_msg;
						}
						else{
							_cloud_usbclient_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						var _cloud_usbclient_status;
						if(cloud_usbclient_status != "")
							_cloud_usbclient_status = cloud_usbclient_status;
						else
							_cloud_v_status = "";
	
						document.getElementById("cloudStatus_usbclient").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_usbclient_msg +'\');">'+ _cloud_usbclient_status +'</div>';
					}

					// samba client
					
					if(document.getElementById("cloudStatus_sambaclient")){
						if( cloud_sambaclient_status.toUpperCase() == "DOWNUP"){
							 cloud_sambaclient_status = "SYNC";
							document.getElementById("sambaclient_status_image").firstChild.className="status_gif_Img_0";
						}
						else if( cloud_sambaclient_status.toUpperCase() == "ERROR"){
							document.getElementById("sambaclient_status_image").firstChild.className="status_png_Img_error";
						}
						else if( cloud_sambaclient_status.toUpperCase() == "UPLOAD"){
							document.getElementById("sambaclient_status_image").firstChild.className="status_gif_Img_2";
						}
						else if( cloud_sambaclient_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("sambaclient_status_image").firstChild.className="status_gif_Img_1";
						}
						else if( cloud_sambaclient_status.toUpperCase() == "SYNC"){
							 cloud_sambaclient_status = "Finish";
							if(curRule.SambaClient == 2){
								document.getElementById("sambaclient_status_image").firstChild.className="status_png_Img_L_ok";
							}else if(curRule.SambaClient == 1){
								document.getElementById("sambaclient_status_image").firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("sambaclient_status_image").firstChild.className="status_png_Img_LR_ok";
							}	
						}

	
						// handle msg
						var _cloud_sambaclient_msg = "";
						if(cloud_sambaclient_obj != ""){
							_cloud_sambaclient_msg +=  "<b>";
							_cloud_sambaclient_msg += cloud_sambaclient_status;
							_cloud_sambaclient_msg += ": </b><br />";
							_cloud_sambaclient_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(cloud_sambaclient_obj) + "</span>";
						}
						else if(cloud_sambaclient_msg){
							_cloud_sambaclient_msg += cloud_sambaclient_msg;
						}
						else{
							_cloud_sambaclient_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						var _cloud_sambaclient_status;
						if(cloud_sambaclient_status != "")
							_cloud_sambaclient_status = cloud_sambaclient_status;
						else
							_cloud_v_status = "";
	
						document.getElementById("cloudStatus_sambaclient").innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_sambaclient_msg +'\');">'+ _cloud_sambaclient_status +'</div>';
					}
					
					// Router Sync
					if(rs_rulenum == "") rs_rulenum = 0;

					if(document.getElementById("rsStatus_"+rs_rulenum)){
						if(rs_status.toUpperCase() == "DOWNUP"){
							rs_status = "SYNC";
							document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_gif_Img_0";
						}
						else if(rs_status.toUpperCase() == "ERROR"){
							document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_png_Img_error";
						}
						else if(rs_status.toUpperCase() == "UPLOAD"){
							document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_gif_Img_2";
						}
						else if(rs_status.toUpperCase() == "DOWNLOAD"){
							document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_gif_Img_1";
						}
						else if(rs_status.toUpperCase() == "SYNC"){
							rs_status = "Finish";
							if(curRule.RouterSync == 2){
								document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_png_Img_L_ok";
							}else if(curRule.RouterSync == 1){
								document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_png_Img_R_ok";
							}else{
								document.getElementById("rsstatus_image_"+rs_rulenum).firstChild.className="status_png_Img_LR_ok";
							}	
						}
	
						// handle msg
						var _rs_msg = "";
						if(rs_obj != ""){
							_rs_msg +=  "<b>";
							_rs_msg += rs_status;
							_rs_msg += ": </b><br />";
							_rs_msg += "<span style=\\'word-break:break-all;\\'>" + decodeURIComponentSafe(rs_obj) + "</span>";
						}
						else if(rs_msg){
							_rs_msg += rs_msg;
						}
						else{
							_rs_msg += "<#aicloud_no_record#>";
						}
	
						// handle status
						document.getElementById("rsStatus_"+rs_rulenum).innerHTML = '<div style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _rs_msg +'\');">'+ rs_status +'</div>';
					}

			 		setTimeout("updateCloudStatus();", 1500);
      }
   });
}

function convStr(_str){
	if(_str.toUpperCase() == "SYNC")
		return "Finish";
	else
		return _str;
}

function validform(){	
	if(document.getElementById('select_service').innerHTML != "WebStorage"
	&& document.getElementById('select_service').innerHTML != "Dropbox"
	&& document.getElementById('select_service').innerHTML != "FTP server"
	&& document.getElementById('select_service').innerHTML != "Samba"
	&& document.getElementById('select_service').innerHTML != "Usb"){
		alert("Please select the provider!!");
		return false;
	}
	
	if(!validator.string(document.form.cloud_dir))
		return false;

	if(!Block_chars(document.form.cloud_username, ["<", ">"]))
		return false;

	if(document.getElementById('select_service').innerHTML == "Samba"){
		if(document.form.sambaclient_name.value == ''){
			alert("The Samba can't NULL");
			document.form.sambaclient_name.focus();
			return false;
		}
		else if(document.form.sambaclient_ip.value == ''){
			alert("The Server address can't NULL");
			document.form.sambaclient_ip.focus();
			return false;
		}
		else if(document.form.sambaclient_sharefolder.value == ''){
			alert("The Share folder can't NULL");
			document.form.sambaclient_sharefolder.focus();
			return false;
		}
	}

	if(document.getElementById('select_service').innerHTML == "Usb"){
		if(document.form.usbclient_sharefolder.value == ''){
			alert("The Share folder can't NULL");
			document.form.usbclient_sharefolder.focus();
			return false;
		}
	}
	
	if((document.getElementById('select_service').innerHTML != "FTP server") && (document.getElementById('select_service').innerHTML != "Usb") ){		// to allow ftp client could use anonymous/anonymous, blank field, Jieming added at 2013/12/09
		if(document.form.cloud_username.value == ''){
			alert("<#File_Pop_content_alert_desc1#>");
			document.form.cloud_username.focus();
			return false;
		}
	}


	if(!Block_chars(document.form.cloud_password, ["<", ">"]))
		return false;

	if((document.getElementById('select_service').innerHTML != "FTP server") && (document.getElementById('select_service').innerHTML != "Usb")){		// to allow ftp client could use anonymous/anonymous, blank field, Jieming added at 2013/12/09
		if(document.form.cloud_password.value == ''){
			alert("<#File_Pop_content_alert_desc6#>");
			document.form.cloud_password.focus();
			return false;
		}
	}

	/*if(document.form.cloud_password.value.length < 8){ //disable to check length of password temporary, Jieming added at 2013.08.13
		alert("<#cloud_list_password#>");
		return false;
	}*/
	
	//if(document.form.cloud_dir.value.split("/").length < 4 || document.form.cloud_dir.value == ''){
	if(document.form.cloud_dir.value == ''){
		alert("<#ALERT_OF_ERROR_Input10#>");
		document.form.cloud_dir.focus();
		return false;
	}

	if(document.getElementById('select_service').innerHTML == "Usb"){		
		if(document.form.usbclient_sharefolder.value.search(document.form.cloud_dir.value) == 0 || document.form.cloud_dir.value.search(document.form.usbclient_sharefolder.value) == 0){
			alert("The Share folder can't be set the same path.");
			document.form.usbclient_sharefolder.focus();
			return false;
		}
	}

	// add mode need check account whether had created or not.
	if(editRule == -1) {
		// ASUS WebStorage and Dropbox only suport one accoumt
		var cloud_sync_array = cloud_sync.split('<');
		var selProvider = document.getElementById("select_service").innerHTML;
		var selProviderIdx = -1;
		var repeatHint = "";
		switch (selProvider) {
			case "WebStorage" :
				selProviderIdx = 0;
				repeatHint = "You had created an ASUS WebStorage account.";
				break;
			case "Dropbox" :
				selProviderIdx = 3;
				repeatHint = "You had created a Dropbox account.";
				break;
		}
		for(var i = 0; i < cloud_sync_array.length; i += 1) {
			if(cloud_sync_array[i][0] == selProviderIdx) {
				alert(repeatHint);
				return false;
			}
		}
	}

	return true;
}

function applyRule(){
	if(validform()){
		var cloud_synclist_rules = cloud_sync.split('<');
		var newRule = new Array();
		var cloud_list_temp = new Array();

		if(document.getElementById('select_service').innerHTML == "WebStorage"){
			newRule.push(0);
			newRule.push(document.form.cloud_username.value);
			newRule.push(document.form.cloud_password.value);

			if(document.form.captcha_field.value == "" && document.form.security_code_field.value == ""){
				newRule.push("none");
			}
			else{
				newRule.push(document.form.security_code_field.value+'#'+document.form.captcha_field.value);
			}

			newRule.push(document.form.cloud_rule.value);
			newRule.push("/tmp"+document.form.cloud_dir.value);
			newRule.push(1);
		}
		else if(document.getElementById('select_service').innerHTML == "Dropbox"){
			newRule.push(3);
			newRule.push(1);
			newRule.push(document.form.cloud_username.value);
			newRule.push(document.form.cloud_password.value);
			newRule.push(1);
			newRule.push(document.form.cloud_rule.value);
			newRule.push("/tmp"+document.form.cloud_dir.value);
		}
		else if(document.getElementById('select_service').innerHTML == "FTP server"){

			newRule.push(2);
			newRule.push(0);
			newRule.push(document.form.cloud_username.value);
			newRule.push(document.form.cloud_password.value);
			document.form.ftp_url.value = document.getElementById('ftp_protocol').value + document.form.ftp_url.value
			newRule.push(document.form.ftp_url.value);
			newRule.push(document.form.ftp_root_path.value);
			newRule.push(1);
			newRule.push(document.form.cloud_rule.value);
			newRule.push("/tmp"+document.form.cloud_dir.value);
		}
		else if(document.getElementById('select_service').innerHTML == "Samba"){
			//[0] = Provider, [1] = Work Group, [2] = Server IP address, [3] = Server share folder, [4] = Username, [5] = Password, [6] = Cloud rule, [7]  = Cloud dir
			newRule.push(4);
			newRule.push(document.form.sambaclient_name.value);
			newRule.push("smb://" + document.form.sambaclient_ip.value);
			newRule.push(document.form.sambaclient_sharefolder.value);
			newRule.push(document.form.cloud_username.value);
			newRule.push(document.form.cloud_password.value);
			newRule.push(document.form.cloud_rule.value);
			newRule.push("/tmp"+document.form.cloud_dir.value);	
		}
		else if(document.getElementById('select_service').innerHTML == "Usb"){
			//[0] = Provider, [1] = Work Group, [2] = Server IP address, [3] = Server share folder, [4] = Username, [5] = Password, [6] = Cloud rule, [7]  = Cloud dir
			newRule.push(5);
			newRule.push(document.form.usbclient_name.value);
			newRule.push(document.form.usbclient_ip.value);
			newRule.push(document.form.usbclient_sharefolder.value);
			newRule.push(document.form.cloud_username.value);
			newRule.push(document.form.cloud_password.value);
			newRule.push(document.form.cloud_rule.value);
			newRule.push("/tmp"+document.form.cloud_dir.value);	
		}

		if(editRule != -1){
			cloud_synclist_rules[editRule-1] = newRule.join(">");
		}
		else{
			cloud_synclist_rules.push(newRule.join(">"));
		}
		
		
		/* To avoid first element of array will be null, Jieming added at 2013.11.13 */
		if(cloud_synclist_rules[0] == ""){
			for(i=0;i<cloud_synclist_rules.length -1;i++){
				cloud_list_temp[i] = cloud_synclist_rules[i+1];			
			}
			document.form.cloud_sync.value = cloud_list_temp.join("<");
		}
		else{
			document.form.cloud_sync.value = cloud_synclist_rules.join("<");
		}
	
		FormActions("start_apply.htm", "apply", "restart_cloudsync", "2");
		showLoading();
		document.form.submit();
	}
}

function convSrv(val){
	if(val == 0) return "webstorage";

	/* Todo */
	//else if(val == 1) return "routersync";

	else if(val == 2) return "ftpserver";
	else if(val == 3) return "dropbox";
	else if(val == 4) return "sambaclient";
	else if(val == 5) return "usbclient";
	else if(val == 9) return "new_rule";
	else  return "unknown";
}

function showAddTable(srv, row_number){
	var _srv = convSrv(srv);
	cal_panel_block("cloudAddTable_div", 0.2);

	if(_srv == "webstorage"
	|| _srv == "ftpserver"
	|| _srv == "dropbox"
	|| _srv == "sambaclient"
	|| _srv == "usbclient"){
		$("#cloudAddTable_div").fadeIn();
		document.getElementById("creatBtn").style.display = "none";
		$("#applyDiv").fadeIn();
		edit_Row(row_number);	
	}
	else if(_srv == "new_rule"){
		$("#cloudAddTable_div").fadeIn();
		document.getElementById("creatBtn").style.display = "none";
		document.getElementById("divOneProvider").style.display = "none";
		document.getElementById("povider_tr").style.display = "";
		editRule = -1;
		$("#applyDiv").fadeIn();
		change_service("WebStorage");
		document.getElementById("cloud_username").value = "";
		document.getElementById("cloud_password").value = "";
		document.getElementById("PATH").value = "";
		document.getElementById("sambaclient_ip").value = "";
		document.getElementById("sambaclient_sharefolder").value = "";
		document.getElementById("usbclient_ip").value = "";
		document.getElementById("usbclient_sharefolder").value = "";
		if(iCountSamba!=0)
			document.getElementById("sambaclient_name").value = "WORKGROUP(" + iCountSamba + ")";
		else
			document.getElementById("sambaclient_name").value = "WORKGROUP";
		if(iCountUsb!=0)
			document.getElementById("usbclient_name").value = "WORKGROUP(" + iCountUsb + ")";
		else
			document.getElementById("usbclient_name").value = "WORKGROUP";
	}
	else{
		$("#cloudAddTable_div").fadeOut();
		$("#creatBtn").fadeIn();
		document.getElementById("applyDiv").style.display = "none";
	}
}

// get folder tree
var folderlist = new Array();
function get_disk_tree(flag){
	if(disk_flag == 1){
		alert('<#no_usb_found#>');
		return false;	
	}
	
	cal_panel_block("folderTree_panel", 0.25);
	if(flag==0){
		document.getElementById("btn_confirm_folder0").style.display = "";
		document.getElementById("btn_confirm_folder").style.display = "none";
	}
	else{
		document.getElementById("btn_confirm_folder0").style.display = "none";
		document.getElementById("btn_confirm_folder").style.display = "";
	}
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

		//Specific folder 'Download2/Complete'
		if( array_temp_split[j][0].match(/^Download2$/) ){
			treeitems[j] = "Download2/Complete"+"#"+array_temp_split[j][1]+"#"+array_temp_split[j][2];
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
		TempObject +='<img id="a'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' class="FdRead" src="/images/Tree/vert_line_'+isSubTree+'0.gif">';
		TempObject +='</td>';
	
		if(layer == 3){
			/*a: connect_line b: harddisc+name  c:harddisc  d:name e: next layer forder*/
			TempObject +='<td>';		
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
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
			TempObject +='<img id="c'+ItemBarCode+'" onclick=\'document.getElementById("d'+ItemBarCode+'").onclick();\' src="/images/New_ui/advancesetting/'+ItemIcon+'.png">';
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

function confirm_folderTree0(){
	document.getElementById('usbclient_sharefolder').value = path_directory ;
	this.FromObject ="0";
	$("#folderTree_panel").fadeOut(300);
}

function confirm_folderTree(){
	if(document.getElementById('PATH_rs'))
		document.getElementById('PATH_rs').value = path_directory ;
	document.getElementById('PATH').value = path_directory ;
	this.FromObject ="0";
	$("#folderTree_panel").fadeOut(300);
}

function change_service(obj){
	$('#WebStorage').parent().css('display','none');
	$('#Dropbox').parent().css('display','none');
	$('#ftp_server').parent().css('display','none');	
	$('#Samba').parent().css('display','none');
	$('#Usb').parent().css('display','none');	
	
	if(obj == "WebStorage"){
		// document.getElementById('select_service').style.background = "url('/images/cloudsync/ASUS-WebStorage.png') no-repeat";
		document.getElementById('select_service').innerHTML = "WebStorage";  	
		document.getElementById('sambaclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_url').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_port').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_root_path').parentNode.parentNode.style.display = "none";
		//document.getElementById('cloud_rule').parentNode.parentNode.style.display = "";
		document.form.security_code_field.disabled = false;
		document.getElementById("security_code_tr").style.display = "";
		document.getElementById("cloud_username_tr").style.display = "";
		document.getElementById("cloud_password_tr").style.display = "";
		document.getElementById("applyBtn").style.display = "";
		document.getElementById("authBtn").style.display = "none";
		document.getElementById("authHint").style.display = "none";
	}
	else if(obj == "Dropbox"){
		// document.getElementById('select_service').style.background = "url('/images/cloudsync/dropbox.png') no-repeat"; 
		document.getElementById('select_service').innerHTML = "Dropbox"; 	
		document.getElementById('sambaclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_url').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_port').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_root_path').parentNode.parentNode.style.display = "none";
		//document.getElementById('cloud_rule').parentNode.parentNode.style.display = "";
		document.form.security_code_field.disabled = true;
		document.getElementById("security_code_tr").style.display = "none";
		document.getElementById("cloud_username_tr").style.display = "none";
		document.getElementById("cloud_password_tr").style.display = "none";
		document.getElementById("applyBtn").style.display = "none";
		document.getElementById("authBtn").style.display = "";
		document.getElementById("authHint").style.display = "none";
	}
	else if(obj == "FTP"){
		// document.getElementById('select_service').style.background = "url('/images/cloudsync/ftp_server.png') no-repeat";
		document.getElementById('select_service').innerHTML = "FTP server"; 	
		document.getElementById('sambaclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('sambaclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_url').parentNode.parentNode.style.display = "";
		document.getElementById('ftp_port').parentNode.parentNode.style.display = "";
		document.getElementById('ftp_root_path').parentNode.parentNode.style.display = "";
		//document.getElementById('cloud_rule').parentNode.parentNode.style.display = "none";
		document.form.security_code_field.disabled = true;
		document.getElementById("security_code_tr").style.display = "none";
		document.getElementById("cloud_username_tr").style.display = "";
		document.getElementById("cloud_password_tr").style.display = "";
		document.getElementById("applyBtn").style.display = "";
		document.getElementById("authBtn").style.display = "none";
		document.getElementById("authHint").style.display = "none";
	}
	else if(obj == "Samba"){
		document.getElementById('select_service').innerHTML = "Samba";  	
		document.getElementById('sambaclient_name').parentNode.parentNode.style.display = "";
		document.getElementById('sambaclient_ip').parentNode.parentNode.style.display = "";
		document.getElementById('sambaclient_sharefolder').parentNode.parentNode.style.display = "";
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_url').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_port').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_root_path').parentNode.parentNode.style.display = "none";
		document.form.security_code_field.disabled = false;
		document.getElementById("security_code_tr").style.display = "none";
		document.getElementById("cloud_username_tr").style.display = "";
		document.getElementById("cloud_password_tr").style.display = "";
		document.getElementById("applyBtn").style.display = "";
		document.getElementById("authBtn").style.display = "none";
		document.getElementById("authHint").style.display = "none";
	}
	else if(obj == "Usb"){
		// document.getElementById('select_service').style.background = "url('/images/cloudsync/ftp_server.png') no-repeat";
		document.getElementById('select_service').innerHTML = "Usb"; 	
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_name').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_ip').parentNode.parentNode.style.display = "none";
		document.getElementById('usbclient_sharefolder').parentNode.parentNode.style.display = "";
		document.getElementById('ftp_url').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_port').parentNode.parentNode.style.display = "none";
		document.getElementById('ftp_root_path').parentNode.parentNode.style.display = "none";
		//document.getElementById('cloud_rule').parentNode.parentNode.style.display = "none";
		document.form.security_code_field.disabled = true;
		document.getElementById("security_code_tr").style.display = "none";
		document.getElementById("cloud_username_tr").style.display = "none";
		document.getElementById("cloud_password_tr").style.display = "none";
		document.getElementById("applyBtn").style.display = "";
		document.getElementById("authBtn").style.display = "none";
		document.getElementById("authHint").style.display = "none";
	}

	var ss_support = '<% nvram_get("ss_support"); %>';	
	$("#povider_tr").hover(
		function(){     // for mouse enter event
			if(isSupport(ss_support, "asuswebstorage"))
				$('#WebStorage').parent().css('display','block');
			if(isSupport(ss_support, "dropbox"))
				$('#Dropbox').parent().css('display','block');
			if(isSupport(ss_support, "ftp"))
				$('#ftp_server').parent().css('display','block');
			if(isSupport(ss_support, "samba"))
				$('#Samba').parent().css('display','block');
			if(isSupport(ss_support, "usb"))
				$('#Usb').parent().css('display','block');

		},
		function(){		// for mouse leave event
			$('#WebStorage').parent().css('display','none');
			$('#Dropbox').parent().css('display','none');
			$('#ftp_server').parent().css('display','none');		
			$('#Samba').parent().css('display','none');
			$('#Usb').parent().css('display','none');
		}	
	);
}

// parsing ss_support (Smart Sync)
function isSupport(_nvramvalue, _ptn){
	return (_nvramvalue.search(_ptn) == -1) ? false : true;
}

var captcha_flag = 0;
function refresh_captcha(){
	if(captcha_flag == 0){
		var captcha_url = 'http://sg03.asuswebstorage.com/member/captcha/?userid='+document.form.cloud_username.value;
		document.getElementById('captcha_iframe').setAttribute("src", captcha_url);
	}
	else{
		document.getElementById('captcha_iframe').src = document.getElementById('captcha_iframe').src;
	}
}

//- Login dropbox
function dropbox_login(){
	var base64Encode = function(input) {
		var keyStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
		var output = "";
		var chr1, chr2, chr3, enc1, enc2, enc3, enc4;
		var i = 0;
		var utf8_encode = function(string) {
			string = string.replace(/\r\n/g,"\n");
			var utftext = "";
			for (var n = 0; n < string.length; n++) {
				var c = string.charCodeAt(n);
				if (c < 128) {
					utftext += String.fromCharCode(c);
				}
				else if((c > 127) && (c < 2048)) {
					utftext += String.fromCharCode((c >> 6) | 192);
					utftext += String.fromCharCode((c & 63) | 128);
				}
				else {
					utftext += String.fromCharCode((c >> 12) | 224);
					utftext += String.fromCharCode(((c >> 6) & 63) | 128);
					utftext += String.fromCharCode((c & 63) | 128);
				}
			}
			return utftext;
		};
		input = utf8_encode(input);
		while (i < input.length) {
			chr1 = input.charCodeAt(i++);
			chr2 = input.charCodeAt(i++);
			chr3 = input.charCodeAt(i++);
			enc1 = chr1 >> 2;
			enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
			enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
			enc4 = chr3 & 63;
			if (isNaN(chr2)) {
				enc3 = enc4 = 64;
			}
			else if (isNaN(chr3)) {
				enc4 = 64;
			}
			output = output + 
			keyStr.charAt(enc1) + keyStr.charAt(enc2) + 
			keyStr.charAt(enc3) + keyStr.charAt(enc4);
		}
		return output;
	};
	var b = window.location.href.indexOf("/",window.location.protocol.length+2);
	var app_key = "qah4ku73k3qmigj";
	var redirect_url = "https://oauth.asus.com/aicloud/dropbox.html";			
	var callback_url = window.location.href.slice(0,b) + "/dropbox_callback.htm,onDropBoxLogin"; 

	//workaround for encode issue, if the original string is not a multiple of 6, the base64 encode result will display = at the end
	//Then Dropbox will encode the url twice, the char = will become %3D, and callback oauth.asus.com will cause url not correct.
	//So need add not use char at callback_url for a multiple of 6
	var remainder = callback_url.length % 6;
	if(remainder != 0) {
		var not_use = "";
		for(var i = remainder; i < 6; i += 1) {
			not_use += ",";
		}
		callback_url += not_use; 
	}
	var url = "https://www.dropbox.com/1/oauth2/authorize?response_type=token&client_id=" + app_key;
	url += "&redirect_uri=" + encodeURIComponent(redirect_url);
	url += "&state=base64_" + base64Encode(callback_url);
	url += "&force_reapprove=true";
			
	window.open(url,"mywindow","menubar=1,resizable=0,width=630,height=550");
}

//- Login success callback function
function onDropBoxLogin(token, uid){
	if(token.search("error") == -1){
		document.form.cloud_username.value = uid;
		document.form.cloud_password.value = token;
		document.getElementById("applyBtn").style.display = "";
		document.getElementById("authBtn").style.display = "none";
		document.getElementById("authHint").style.display = "";
	}
	else{
		document.getElementById("applyBtn").style.display = "none";
		document.getElementById("authBtn").style.display = "";
		document.getElementById("authHint").style.display = "none";
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
					<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:20px;margin-left:30px;"><#Cloudsync_Get_Invitation#></div>	<!-- You have got a new invitation! -->
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
		<input class="button_gen" type="button" id="btn_confirm_folder0" onclick="confirm_folderTree0();" value="<#CTL_ok#>" style="display:none;">
		<input class="button_gen" type="button" id="btn_confirm_folder" onclick="confirm_folderTree();" value="<#CTL_ok#>" style="display:none;">
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
<div id="cloudAddTable_div" class="contentM_qis">
					<table width="97%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="cloudAddTable" style="margin-top:10px;margin-bottom:10px;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist"><#aicloud_cloud_list#></td>
	   					</tr>
	  					</thead>		  
							<tr>
								<th width="30%" style="height:40px;font-family: Calibri;font-weight: bolder;"><#Provider#></th>
								<td>				
									<div id="divOneProvider" style="display:none;"></div>				
									<ul id="povider_tr" class="navigation" style="margin:-15px 0 0 -39px;*margin:-20px 0 0 0;">  
										<li >
											<dl>
												<dt>
													<div id="select_service" style="height:35px;margin:-4px 0px 0px 0px;padding:4px;" value=""><#AddAccountTitle#></div>
												</dt> 
												<dd style="text-align: center;font-weight: bold;font-size: 13px;padding:3px;width:139px;" onclick="change_service('WebStorage');"> 
													<div id="WebStorage" style="background: url('/images/cloudsync/ASUS-WebStorage.png') no-repeat; height:35px;"><a style="text-align:left;padding:10px 0px 0px 45px;">WebStorage</a></div>
												</dd>
												<dd style="text-align: center;font-weight: bold;font-size: 13px;padding:3px;width:139px;" onclick="change_service('Dropbox');">  
													<div id="Dropbox" style="background: url('/images/cloudsync/dropbox.png') no-repeat; height:35px;"><a style="text-align:left;padding:10px 0px 0px 45px;">Dropbox</a></div>								
												</dd>
												<dd style="text-align: center;font-weight: bold;font-size: 13px;padding:3px;width:139px;" onclick="change_service('FTP');">  
													<div id="ftp_server" style="background: url('/images/cloudsync/ftp_server.png') no-repeat; height:35px;"><a style="text-align:left;padding:10px 0px 0px 45px;">FTP server</a></div>
												</dd>
												<dd style="text-align: center;font-weight: bold;font-size: 13px;padding:3px;width:139px;" onclick="change_service('Samba');">  
													<div id="Samba" style="background: url('/images/cloudsync/ftp_server.png') no-repeat; height:35px;"><a style="text-align:left;padding:10px 0px 0px 45px;">Samba</a></div>
												</dd>
												<dd style="text-align: center;font-weight: bold;font-size: 13px;padding:3px;width:139px;" onclick="change_service('Usb');">  
													<div id="Usb" style="background: url('/images/cloudsync/ftp_server.png') no-repeat; height:35px;"><a style="text-align:left;padding:10px 0px 0px 45px;">Usb</a></div>
												</dd>
											</dl>
										</li>
									</ul>
								</td>
							</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#Server_Name#>	<!-- Server Name -->
							</th>			
							<td>
							  <input type="text" class="input_32_table" maxlength="32" style="height: 23px;" id="sambaclient_name" name="sambaclient_name" autocorrect="off" autocapitalize="off">
							  &nbsp;
							  <span><#feedback_optional#></span>
							</td>
						</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#>
							</th>			
							<td>
								<span>smb://</span>
							  <input type="text"  class="input_32_table" style="height: 23px;" id="sambaclient_ip" name="sambaclient_ip" value="" autocorrect="off" autocapitalize="off">
							</td>
						</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#Cloudsync_Shared_Folder#><!-- Server share folder -->
							</th>			
							<td>
							  <input type="text"  class="input_32_table" style="height: 23px;" id="sambaclient_sharefolder" name="sambaclient_sharefolder" value="" autocorrect="off" autocapitalize="off">
							</td>
						</tr>	


						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Server Name
							</th>			
							<td>
							  <input type="text" class="input_32_table" maxlength="32" style="height: 23px;" id="usbclient_name" name="usbclient_name" autocorrect="off" autocapitalize="off">
							  &nbsp;
							  <span>(Optional)</span>
							</td>
						</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#>
							</th>			
							<td>
								<span>usb://</span>
							  <input type="text"  class="input_32_table" style="height: 23px;" id="usbclient_ip" name="usbclient_ip" value="" autocorrect="off" autocapitalize="off">
							</td>
						</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Server share folder
							</th>			
							<td>
							  <input type="text"  class="input_32_table" style="height: 23px;" id="usbclient_sharefolder" name="usbclient_sharefolder" value="" autocorrect="off" autocapitalize="off">
							  <input name="button" type="button" class="button_gen_short" onclick="get_disk_tree(0);" value="<#Cloudsync_browser_folder#>"/>
								<div id="noUSB0" style="color:#FC0;display:none;margin-left: 3px;"><#no_usb_found#></div>
							</td>
						</tr>	


						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#>
							</th>			
							<td>
								<select id="ftp_protocol" name="ftp_protocol" class="input_option">
									<option value="ftp://">FTP</option>								
								</select>
								<input type="text" maxlength="32" class="input_32_table" style="height: 23px;" id="ftp_url" name="ftp_url" value="" autocorrect="off" autocapitalize="off">
							</td>
						</tr>		
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#IPConnection_VSList_ftpport#>
							</th>			
							<td>
							  <input type="text" maxlength="32" class="input_32_table" style="height: 23px;" id="ftp_port" name="ftp_port" value="21" autocorrect="off" autocapitalize="off">
							</td>
						</tr>	
						<tr style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#Cloudsync_Remote_Path#>	<!-- Remote Path -->
							</th>			
							<td>
							  <input type="text" class="input_32_table" style="height: 23px;" id="ftp_root_path" name="ftp_root_path" value="" autocorrect="off" autocapitalize="off">
							</td>
						</tr>	
							
						  <tr id="cloud_username_tr">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#AiDisk_Account#>
							</th>			
							<td>
							  <input type="text" maxlength="32"class="input_32_table" style="height: 23px;" id="cloud_username" name="cloud_username" value="" autocorrect="off" autocapitalize="off">
							</td>
						  </tr>	

						  <tr id="cloud_password_tr">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#HSDPAConfig_Password_itemname#>
							</th>			
							<td>
								<input id="cloud_password" name="cloud_password" type="password" autocapitalization="off" onBlur="switchType(this, false);" onFocus="switchType(this, true);" class="input_32_table" style="height: 23px;" value="" autocorrect="off" autocapitalize="off">
							</td>
						  </tr>						  				
					  								
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#routerSync_folder#>
							</th>
							<td>
							<input type="text" id="PATH" class="input_32_table" style="height: 23px;" name="cloud_dir" value="" onclick="" autocomplete="off" autocorrect="off" autocapitalize="off"/>
		  					<input name="button" type="button" class="button_gen_short" onclick="get_disk_tree(1);" value="<#Cloudsync_browser_folder#>"/>
								<div id="noUSB" style="color:#FC0;display:none;margin-left: 3px;"><#no_usb_found#></div>
							</td>
						  </tr>

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#Cloudsync_Rule#>
							</th>
							<td>
								<select id="cloud_rule" name="cloud_rule" class="input_option">
									<option value="0"><#Cloudsync_Rule_sync#></option>
									<option value="1"><#Cloudsync_Rule_dl#></option>
									<option value="2"><#Cloudsync_Rule_ul#></option>
								</select>			
							</td>
						  </tr>

						  <tr id="security_code_tr">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#routerSync_Security_code#>
							</th>
							<td>
								<div style="color:#FC0;"><input id="security_code_field" name="security_code_field" type="text" maxlength="6" class="input_32_table" style="height: 23px;width:100px;margin-right:10px;" autocorrect="off" autocapitalize="off"><#OTP_Auth#><!--OTP Authentication--></div>
							</td>
						  </tr>
						  <tr height="45px;" id="captcha_tr" style="display:none;">
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#Captcha#>	<!-- Captcha -->
							</th>			
							<td style="height:85px;">
								<div style="height:25px;"><input id="captcha_field" name="captcha_field" type="text" maxlength="6" class="input_32_table" style="height: 23px;width:100px;margin-top:8px;" autocomplete="off" autocorrect="off" autocapitalize="off"></div>
								<div id="captcha_hint" style="color:#FC0;height:25px;margin-top:10px;"><#Captcha_note#></div>	<!-- Please input the captcha -->
								<div>
									<iframe id="captcha_iframe" frameborder="0" scrolling="no" src="" style="width:230px;height:80px;*width:210px;*height:87px;margin:-60px 0 0 160px;*margin-left:165px;"></iframe>
								</div>
								<div style="color:#FC0;text-decoration:underline;height:35px;margin:-35px 0px 0px 380px;cursor:pointer" onclick="refresh_captcha();"><#CTL_refresh#></div>   
							</td>
						  </tr>
						</table>
							<div class="apply_gen" style="margin-top:20px;margin-bottom:10px;display:none;background-color: #2B373B;" id="applyDiv">
	  					<input name="button" type="button" class="button_gen" onclick="showAddTable();" value="<#CTL_Cancel#>"/>
	  					<input id="applyBtn" name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
	  					<input id="authBtn" name="button" type="button" class="button_gen" onclick="dropbox_login()" value="Authenticate"/>
	  					<span id="authHint" style="color:#FC0;display:none">Authenticated!</span>
	  				</div>
</div>
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
							<a href="cloud_main.asp"><div class="tab"><span>AiCloud 2.0</span></div></a>
						</td>
						<td>
							<div class="tabclick"><span><#smart_sync#></span></div>
						</td>
						<td>
							<a id="rrsLink" href="cloud_router_sync.asp"><div class="tab"><span><#Server_Sync#></span></div></a>
						</td>
						<td>
							<a href="cloud_settings.asp"><div class="tab"><span><#Settings#></span></div></a>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab"><span><#Log#></span></div></a>
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
						<div class="formfonttitle">AiCloud 2.0 - <#smart_sync#></div>
						<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

						<div>
							<table width="700px" style="margin-left:25px;">
								<tr>
									<td style="width:214px;">
										<img id="guest_image" src="/images/cloudsync/003.png" style="margin-top:10px;margin-left:-20px">
										<div align="center" class="left" style="margin-top:25px;margin-left:43px;width:94px; float:left; cursor:pointer;" id="radio_smartSync_enable"></div>
										<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
										<script type="text/javascript">
											$('#radio_smartSync_enable').iphoneSwitch('<% nvram_get("enable_cloudsync"); %>',
												function() {
													document.enableform.enable_cloudsync.value = 1;
													showLoading();	
													document.enableform.submit();
												},
												function() {
													document.enableform.enable_cloudsync.value = 0;
													showLoading();	
													document.enableform.submit();	
												}
											);
										</script>			
										</div>

									</td>
									<td>&nbsp;&nbsp;</td>
									<td>
										<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:normal;">
											<#smart_sync_help#> <a href="http://aicloud-faq.asuscomm.com/aicloud-faq/" style="text-decoration:underline;font-weight:bolder;">http://aicloud-faq.asuscomm.com/aicloud-faq/</a>
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
    					<th width="25%"><#HSDPAConfig_Username_itemname#></a></th>
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
										<td style="color:#FFCC00;" colspan="6"><#QKSet_detect_sanglass#>...</td>	<!-- Detecting -->
									</tr>
								</tbody>
							</table>
						</div>


					
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
							<input name="applybutton" id="applybutton" type="button" class="button_gen_long" onclick="showAddTable(9);" value="<#AddAccountTitle#>" style="word-wrap:break-word;word-break:normal;">
							<img id="update_scan" style="display:none;" src="images/InternetScan.gif" />
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
</div>
</body>
</html>
