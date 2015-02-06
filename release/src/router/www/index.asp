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
<link href="/images/map-iconRouter_iphone.png" rel="apple-touch-icon" />
<title><#Web_Title#> - <#menu1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="NM_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<link rel="stylesheet" type="text/css" href="/device-map/device-map.css">
<style type="text/css">
.contentM_qis{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:20000;
	background-color:#2B373B;
	display:block;
	margin-left: 23%;
	margin-top: 20px;
	width:420px;
	height:250px;
	box-shadow: 3px 3px 10px #000;
	display: none;
}
.contentM_usericon{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:20000;
	background-color:#2B373B;
	display:block;
	margin-left: 23%;
	margin-top: 20px;
	width:650px;
	height:250px;
	box-shadow: 3px 3px 10px #000;
	display: none;
}
.contentM_qis_manual{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	margin-left: -30px;
	margin-left: -100px \9; 
	margin-top:-400px;
	width:740px;
	box-shadow: 3px 3px 10px #000;
}
#client_image{
	cursor: pointer;
	width: 102px;
	height: 77px;
	background-image: url('/images/New_ui/networkmap/client-list.png');
	background-repeat: no-repeat;
	background-position:50% 61.10%;
}
#client_image:hover{
	background-image: url('/images/New_ui/networkmap/client-listover.png');
}
#custom_image table{
	border: 1px solid #000000;
	border-collapse: collapse;
}
#custom_image div{
	background-image:url('/images/New_ui/networkmap/client-list.png');
	background-repeat:no-repeat;
	height:55px;
	width:55px;
	cursor:pointer;
	background-position:-21px 0%;
}
#custom_image div:hover{
	background-image:url('/images/New_ui/networkmap/client-listover.png');
}
#custom_image td:hover{
	border-radius: 7px;
	background-color:#84C1FF;
}
.disabled{
	background:#475a5f;
	width:200px;
	height:15px;
	padding:5px 0px 5px 5px;
	margin-left:2px;
	border-radius:5px;
}
#ipLockIcon{
	cursor: pointer;
	margin-top:-2px;
	background-repeat:no-repeat;
	height: 25px;
	width: 25px;
}
.dhcp{
	background-image: url('/images/New_ui/networkmap/unlock.png');
}
.manual{
	background-image: url('/images/New_ui/networkmap/lock.png');
}
#tdUserIcon:hover{
	cursor: pointer;
	background-color:#2B373B !important;
}
#divUserIcon{
	cursor: pointer;
	background-image:none !important;
	border: 2px dashed #A1A99F;
	width: 50px !important;
	height: 50px !important;
	text-align: center;
	line-height: 50px;
	color: #A1A99F;
}
#divUserIcon:hover{
	cursor: pointer;
	background-image:none !important;
	border: 2px dashed #37CB0F;
	width: 50px !important;
	height: 50px !important;
	text-align: center;
	line-height: 50px;
	color: #37CB0F;
}
#uploadIcon{
	cursor: pointer;
	display: block !important;
	opacity: 0 !important;
	overflow: hidden !important;
	width: 55px !important;
	height:55px !important;
	margin-top: -52px;
	margin-left: -2px;
}
#canvasUserIcon{
	cursor: pointer;
	position: relative; 
	top: 6px;
	left: 18px;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
}
.imgClientIcon{
	position: relative; 
	width: 52px;
	height: 52px;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
}
</style>
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();	
var userIconBase64 = "NoIcon";
var userIconHideFlag = false;
var custom_usericon_del = "";
var editClientImageFlag = false;

if(location.pathname == "/"){
	if('<% nvram_get("x_Setting"); %>' == '0'){
		if(tmo_support){
			location.href = '/MobileQIS_Login.asp';
		}
		else{
			location.href = '/QIS_wizard.htm?flag=welcome';
		}
	}	
	else if('<% nvram_get("w_Setting"); %>' == '0' && sw_mode != 2)
		location.href = '/QIS_wizard.htm?flag=wireless';
}

// Live Update
var webs_state_update= '<% nvram_get("webs_state_update"); %>';
var webs_state_error= '<% nvram_get("webs_state_error"); %>';
var webs_state_info= '<% nvram_get("webs_state_info"); %>';


// WAN
<% wanlink(); %>var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var Dev3G = '<% nvram_get("d3g"); %>';
var flag = '<% get_parameter("flag"); %>';
var wan0_primary = '<% nvram_get("wan0_primary"); %>';
var wan1_primary = '<% nvram_get("wan1_primary"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_mode = '<%nvram_get("wans_mode");%>';
var dhcp_staticlist_orig = decodeURIComponent('<% nvram_char_to_ascii("", "dhcp_staticlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<")

if(wans_dualwan_orig.search(" ") == -1)
	var wans_flag = 0;
else
	var wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;

// USB function
var currentUsbPort = new Number();
var usbPorts = new Array();

// Wireless
var wlc_band = '<% nvram_get("wlc_band"); %>';
window.onresize = function() {
	if(document.getElementById("edit_client_block").style.display == "block") {
		cal_panel_block("edit_client_block");
	}
	if(document.getElementById("edit_usericon_block").style.display == "block") {
		cal_panel_block("edit_usericon_block");
	}
} 

function initial(){   
	show_menu();
	var isIE6 = navigator.userAgent.search("MSIE 6") > -1;
	if(isIE6)
		alert("<#ALERT_TO_CHANGE_BROWSER#>");
	
	if(dualWAN_support && sw_mode == 1){
		check_dualwan(wans_flag);	
	}
	
	if(sw_mode == 4)
		show_middle_status('<% nvram_get("wlc_auth_mode"); %>', 0);		
	else
		show_middle_status(document.form.wl_auth_mode_x.value, parseInt(document.form.wl_wep_x.value));

	set_default_choice();

	var get_client_detail_info = '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
	show_client_status(get_client_detail_info.split('<').length - 1);
	document.networkmapdRefresh.client_info_tmp.value = get_client_detail_info.replace(/\s/g, "");

	if(!parent.usb_support || usbPortMax == 0){
		$("line3_td").height = '40px';
		$("line3_img").src = '/images/New_ui/networkmap/line_dualwan.png';
		$("clients_td").colSpan = "3";
		$("clients_td").width = '350';
		$("clientspace_td").style.display = "none";
		$("usb_td").style.display = "none";
	}

	for(var i=0; i<usbPortMax; i++){
		var tmpDisk = new newDisk();
		tmpDisk.usbPath = i+1;
		show_USBDevice(tmpDisk);
		$("usbPathContainer_"+parseInt(i+1)).style.display = "";
	}
	
	if(document.getElementById('usbPathContainer_2').style.display == "none")
		document.getElementById('space_block').style.display = "";
	
	check_usb3();

 	require(['/require/modules/diskList.js'], function(diskList){
 		var usbDevicesList = diskList.list();
		for(var i=0; i<usbDevicesList.length; i++){
		  var new_option = new Option(usbDevicesList[i].deviceName, usbDevicesList[i].deviceIndex);
			document.getElementById('deviceOption_'+usbDevicesList[i].usbPath).options.add(new_option);
			document.getElementById('deviceOption_'+usbDevicesList[i].usbPath).style.display = "";

			if(typeof usbPorts[usbDevicesList[i].usbPath-1].deviceType == "undefined" || usbPorts[usbDevicesList[i].usbPath-1].deviceType == "")
				show_USBDevice(usbDevicesList[i]);
		}
	});

	showMapWANStatus(sw_mode);

	if(sw_mode != "1"){
		$("wanIP_div").style.display = "none";
		$("ddnsHostName_div").style.display = "none";
		$("NM_connect_title").style.fontSize = "14px";
		$("NM_connect_status").style.fontSize = "20px";
		if(sw_mode == 2 || sw_mode == 4){
			$('wlc_band_div').style.display = "";
			$('dataRate_div').style.display = "";
			if(wlc_band == 0)
				$('wlc_band_status').innerHTML = "2.4GHz"; 
			else	
				$('wlc_band_status').innerHTML = "5GHz";
		}
		$('NM_connect_title').innerHTML = "<#parent_AP_status#> :";

		document.getElementById("ipaddr_field").disabled = true;
		document.getElementById("ipLockIcon").style.display = "none";
	}
	else{
		$("index_status").innerHTML = '<span style="word-break:break-all;">' + wanlink_ipaddr() + '</span>'

		setTimeout("show_ddns_status();", 2000);
		
		if(wanlink_ipaddr() == '0.0.0.0' || wanlink_ipaddr() == '')
			$("wanIP_div").style.display = "none";
	}

	var NM_table_img = cookie.get("NM_table_img");
	if(NM_table_img != "" && NM_table_img != null){
		customize_NM_table(NM_table_img);
		$("bgimg").options[NM_table_img[4]].selected = 1;
	}
	update_wan_status();

	if(smart_connect_support){
		if(localAP_support && sw_mode != 2)
		show_smart_connect_status();
	}


	document.list_form.dhcp_staticlist.value = dhcp_staticlist_orig;
}

function show_smart_connect_status(){
	document.getElementById("SmartConnectName").style.display = "";
	document.getElementById("SmartConnectStatus").style.display = "";
	var smart_connect_x = '<% nvram_get("smart_connect_x"); %>';

        if(smart_connect_x == '0')
                $("SmartConnectStatus").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_Wireless_Content.asp">Off</a>';
        else if(smart_connect_x == '1')
                $("SmartConnectStatus").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_Wireless_Content.asp">On</a>';

	setTimeout("show_smart_connect_status();", 2000);
}

function show_ddns_status(){
	var ddns_enable = '<% nvram_get("ddns_enable_x"); %>';
	var ddns_server_x = '<% nvram_get("ddns_server_x");%>';
	var ddnsName = decodeURIComponent('<% nvram_char_to_ascii("", "ddns_hostname_x"); %>');

	$("ddns_fail_hint").className = "notificationoff";
        if( ddns_enable == '0')
                $("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=ddns_enable_x"><#btn_go#></a>';
        else if(ddnsName == '')
                $("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=DDNSName">Sign up</a>';
        else if(ddnsName == isMD5DDNSName())
                $("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=DDNSName">Sign up</a>';
        else{
                $("ddnsHostName").innerHTML = '<span>'+ ddnsName +'</span>';
                if( ddns_enable == '1' ) {
			if(!((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2")) ) //link down
				$("ddns_fail_hint").className = "notificationon";
			if( ddns_server_x == 'WWW.ASUS.COM' ) { //ASUS DDNS
			    if( (ddns_return_code.indexOf('200')==-1) && (ddns_return_code.indexOf('220')==-1) && (ddns_return_code.indexOf('230')==-1))
				$("ddns_fail_hint").className = "notificationon";
			}
			else { //Other ddns service
			    if(ddns_updated != '1' || ddns_return_code=='unknown_error' || ddns_return_code=="auth_fail")
                        	$("ddns_fail_hint").className = "notificationon";
			}
                }
	}
	setTimeout("show_ddns_status();", 2000);
}


var isMD5DDNSName = function(){
	var macAddr = '<% nvram_get("lan_hwaddr"); %>'.toUpperCase().replace(/:/g, "");
	return "A"+hexMD5(macAddr).toUpperCase()+".asuscomm.com";
}

function customize_NM_table(img){
	$("NM_table").style.background = "url('/images/" + img +"')";
	cookie.set("NM_table_img", img, 365);
}

function set_default_choice(){
	var icon_name;
	if(flag && flag.length > 0){
		if(flag == "Internet"){
			$("iconRouter").style.backgroundPosition = '0% 0%';
			clickEvent($("iconInternet"));
		}
		else if(flag == "Client"){
			$("iconRouter").style.backgroundPosition = '0% 0%';
			clickEvent($("iconClient"));
		}
		else if(flag == "USBdisk"){
			$("iconRouter").style.backgroundPosition = '0% 0%';
			clickEvent($("iconUSBdisk"));
		}
		else{
			clickEvent($("iconRouter"));
			return;
		}

		if(flag == "Router2g")
			icon_name = "iconRouter";
		else
			icon_name = "icon"+flag;

		clickEvent($(icon_name));
	}
	else
		clickEvent($("iconRouter"));
}

function showMapWANStatus(flag){
	if(sw_mode == "3"){
		showtext($("NM_connect_status"), "<div style='margin-top:10px;'><#WLANConfig11b_x_APMode_itemname#></div>");
	}
	else if(sw_mode == "2"){
		showtext($("NM_connect_title"), "<div style='margin-top:10px;'><#statusTitle_AP#>:</div><br>");
	}
	else
		return 0;
}

function show_middle_status(auth_mode, wl_wep_x){
	var security_mode;
	switch (auth_mode){
		case "open":
				security_mode = "Open System";
				break;
		case "shared":
				security_mode = "Shared Key";
				break;
		case "psk":
				security_mode = "WPA-Personal";
				break;
		case "psk2":
				security_mode = "WPA2-Personal";
				break;
		case "pskpsk2":
				security_mode = "WPA-Auto-Personal";
				$("wl_securitylevel_span").style.fontSize = "16px";
				break;
		case "wpa":
				security_mode = "WPA-Enterprise";
				break;
		case "wpa2":
				security_mode = "WPA2-Enterprise";
				break;
		case "wpawpa2":
				security_mode = "WPA-Auto-Enterprise";
				$("wl_securitylevel_span").style.fontSize = "16px";
				break;
		case "radius":
				security_mode = "Radius with 802.1x";
				$("wl_securitylevel_span").style.fontSize = "16px";
				break;		
		default:
				security_mode = "Unknown Auth";	
	}
	
	$("wl_securitylevel_span").innerHTML = security_mode;

	if(auth_mode == "open" && wl_wep_x == 0)
		$("iflock").src = "images/New_ui/networkmap/unlock.png";
	else
		$("iflock").src = "images/New_ui/networkmap/lock.png"
}

function show_client_status(num){
	$("clientNumber").innerHTML = "<#Full_Clients#>: <span id='_clientNumber'>" + num + "</span>";
}

function show_USBDevice(device){
	if(!usb_support || typeof device != "object")
		return false;

	switch(device.deviceType){
		case "storage":
			disk_html(device);
			break;

		case "printer":
			printer_html(device);
			break;

		case "audio":

		case "webcam":

		case "modem":
			modem_html(device);
			break;

		default:
			no_device_html(device.usbPath);
	}

	currentUsbPort = device.usbPath-1;
	usbPorts[currentUsbPort] = device;
}

function disk_html(device){
	var icon_html_code = '';
	icon_html_code += '<a target="statusframe">\n';
	icon_html_code += '<div id="iconUSBdisk_'+device.usbPath+'" class="iconUSBdisk" onclick="setSelectedDiskOrder(this.id);clickEvent(this);"></div>\n';
	icon_html_code += '<div id="ring_USBdisk_'+device.usbPath+'" class="iconUSBdisk" style="display:none;z-index:1;"></div>\n';
	icon_html_code += '</a>\n';
	$("deviceIcon_" + device.usbPath).innerHTML = icon_html_code;

	showDiskInfo(device);

	// show ring
	check_status(device);
	// check_status2(usb_path1_pool_error, device.usbPath);
}

function showDiskInfo(device){
	var dec_html_code = '';
	var percentbar = 0;

	if(device.mountNumber > 0){
		percentbar = simpleNum2((device.totalSize - device.totalUsed)/device.totalSize*100);
		percentbar = Math.round(100 - percentbar);		

		//dec_html_code += '<p id="diskDesc'+ device.usbPath +'" style="margin-top:5px;"><#Availablespace#>:</p><div id="diskquota" align="left" style="margin-top:5px;margin-bottom:10px;">\n';
		dec_html_code += '<div id="diskquota" align="left" style="margin-top:5px;margin-bottom:10px;">\n';
		dec_html_code += '<img src="images/quotabar.gif" width="' + percentbar + '" height="13">';
		dec_html_code += '</div>\n';
	}
	else{
		dec_html_code += '<div class="style1"><strong id="diskUnmount'+ device.usbPath +'"><#DISK_UNMOUNTED#></strong></div>\n';
	}

	$("deviceDec_"+device.usbPath).innerHTML = dec_html_code;
}

function printer_html(device){
	var icon_html_code = '';
	icon_html_code += '<a href="device-map/printer.asp" target="statusframe">\n';
	icon_html_code += '<div id="iconPrinter_' + device.usbPath + '" class="iconPrinter" onclick="clickEvent(this);"></div>\n';
	icon_html_code += '</a>\n';
	$("deviceIcon_" + device.usbPath).innerHTML = icon_html_code;

	if(device.serialNum == '<% nvram_get("u2ec_serial"); %>')
		$("deviceDec_" + device.usbPath).innerHTML = '<div style="margin:10px;"><#CTL_Enabled#></div>';
	else
		$("deviceDec_" + device.usbPath).innerHTML = '<div style="margin:10px;"><#CTL_Disabled#></div>';
}

function modem_html(device){
	var icon_html_code = '';	
	icon_html_code += '<a href="device-map/modem.asp" target="statusframe">\n';
	icon_html_code += '<div id="iconModem_' + device.usbPath + '" class="iconmodem" onclick="clickEvent(this);"></div>\n';
	icon_html_code += '</a>\n';
	$("deviceIcon_" + device.usbPath).innerHTML = icon_html_code;

	$("deviceDec_" + device.usbPath).innerHTML = '<div style="margin:10px;">' + device.deviceName + '</div>';
}

function no_device_html(device_seat){
	var device_icon = $("deviceIcon_"+device_seat);
	var device_dec = $("deviceDec_"+device_seat);
	var icon_html_code = '';
	var dec_html_code = '';
	
	icon_html_code += '<div class="iconNo"></div>';
	dec_html_code += '<div style="margin:10px" id="noUSB'+ device_seat +'">';

	if(rc_support.search("usbX") > -1)
		dec_html_code += '<#NoDevice#>';
	else
		dec_html_code += '<#CTL_nonsupported#>';

	dec_html_code += '</div>\n';
	device_icon.innerHTML = icon_html_code;
	device_dec.innerHTML = dec_html_code;
}

var avoidkey;
var lastClicked;
var lastName;
var clicked_device_order;

function get_clicked_device_order(){
	return clicked_device_order;
}

function clickEvent(obj){
	var icon;
	var stitle;
	var seat;
	clicked_device_order = -1;

	if(obj.id.indexOf("Internet") > 0){
		if(!dualWAN_support){
			check_wan_unit();
		}

		icon = "iconInternet";
		stitle = "<#statusTitle_Internet#>";
		$("statusframe").src = "/device-map/internet.asp";

		if(parent.wans_flag){
			if(obj.id.indexOf("primary") != -1)
				stitle = "Primary WAN status"
			else
				stitle = "Secondary WAN status"
		}
	}
	else if(obj.id.indexOf("Router") > 0){
		icon = "iconRouter";
		stitle = "<#menu5_7_1#>";
	}
	else if(obj.id.indexOf("Client") > 0){
		icon = "iconClient";
		stitle = "<#statusTitle_Client#>";
	}
	else if(obj.id.indexOf("USBdisk") > 0){
		icon = "iconUSBdisk";
		stitle = "<#statusTitle_USB_Disk#>";
		currentUsbPort = obj.id.slice(-1) - 1;
	}
	else if(obj.id.indexOf("Modem") > 0){
		seat = obj.id.indexOf("Modem")+5;
		clicked_device_order = obj.id.slice(-1);
		currentUsbPort = obj.id.slice(-1) - 1;
		icon = "iconmodem";
		stitle = "<#menu5_4_4#>";
		$("statusframe").src = "/device-map/modem.asp";
	}
	else if(obj.id.indexOf("Printer") > 0){
		seat = obj.id.indexOf("Printer") + 7;
		clicked_device_order = parseInt(obj.id.substring(seat, seat+1));
		currentUsbPort = obj.id.slice(-1) - 1;
		icon = "iconPrinter";
		stitle = "<#statusTitle_Printer#>";
		$("statusframe").src = "/device-map/printer.asp";
	}
	else if(obj.id.indexOf("No") > 0){
		icon = "iconNo";
	}
	else
		alert("mouse over on wrong place!");

	if(lastClicked){
		if(lastClicked.id.indexOf("USBdisk") > 0)
			lastClicked.style.backgroundPosition = '0% -4px';
		else
			lastClicked.style.backgroundPosition = '0% 0%';
	}

	if(obj.id.indexOf("USBdisk") > 0){	// To control USB icon outter ring's color
		if(diskUtility_support){
			if(!usbPorts[obj.id.slice(-1)-1].hasErrPart){
				$("statusframe").src = "/device-map/disk.asp";	
				obj.style.backgroundPosition = '0% -103px';
			}
			else{
				$("statusframe").src = "/device-map/disk_utility.asp";
				obj.style.backgroundPosition = '0% -202px';
			}
		}
		else{
			$("statusframe").src = "/device-map/disk.asp";	
			obj.style.backgroundPosition = '0% -103px';
		}		
	}
	else{
		obj.style.backgroundPosition = '0% 101%';
	}

	$('helpname').innerHTML = stitle;	
	avoidkey = icon;
	lastClicked = obj;
	lastName = icon;
}

function mouseEvent(obj, key){
	var icon;
	
	if(obj.id.indexOf("Internet") > 0)
		icon = "iconInternet";
	else if(obj.id.indexOf("Router") > 0)
		icon = "iconRouter";
	else if(obj.id.indexOf("Client") > 0)
		icon = "iconClient";
	else if(obj.id.indexOf("USBdisk") > 0)
		icon = "iconUSBdisk";
	else if(obj.id.indexOf("Printer") > 0)
		icon = "iconPrinter";
	else if(obj.id.indexOf("No") > 0)
		icon = "iconNo";
	else
		alert("mouse over on wrong place!");
	
	if(avoidkey != icon){
		if(key){ //when mouseover
			obj.style.background = 'url("/images/map-'+icon+'_r.gif") no-repeat';
		}
		else {  //when mouseout
			obj.style.background = 'url("/images/map-'+icon+'.gif") no-repeat';
		}
	}
}//end of mouseEvent

function MapUnderAPmode(){// if under AP mode, disable the Internet icon and show hint when mouseover.
		$("iconInternet").style.background = "url(/images/New_ui/networkmap/map-iconInternet-d.png) no-repeat";
		$("iconInternet").style.cursor = "default";
		
		$("iconInternet").onmouseover = function(){
			writetxt("<#underAPmode#>");
		}
		$("iconInternet").onmouseout = function(){
			writetxt(0);
		}
		$("iconInternet").onclick = function(){
			return false;
		}
		$("clientStatusLink").href = "javascript:void(0)";
		$("clientStatusLink").style.cursor = "default";	
		$("iconClient").style.background = "url(/images/New_ui/networkmap/map-iconClient-d.png) no-repeat";
		$("iconClient").style.cursor = "default";
}

function showstausframe(page){
	clickEvent($("icon"+page));
	if(page == "Client")
		page = "clients";
	else if(page.indexOf('Internet') == 0){
		if(page == "Internet_secondary")
			document.form.dual_wan_flag.value = 1;
		else	
			document.form.dual_wan_flag.value = 0;
			
		page = "Internet";
	}
	
	window.open("/device-map/"+page.toLowerCase()+".asp","statusframe");
}

function check_status(_device){
	var diskOrder = _device.usbPath;
	document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/USB_2.png)";	
	document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -4px';
	document.getElementById('iconUSBdisk_'+diskOrder).style.position = "absolute";
	document.getElementById('iconUSBdisk_'+diskOrder).style.marginTop = "0px";

	/*if(navigator.appName.indexOf("Microsoft") >= 0)
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "0px";
	else*/
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "34px";

	document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";	
	document.getElementById('ring_USBdisk_'+diskOrder).style.display = "";

	if(!diskUtility_support)
		return true;

	var i, j;
	var got_code_0, got_code_1, got_code_2, got_code_3;
	for(i = 0; i < _device.partition.length; ++i){
		switch(parseInt(_device.partition[i].fsck)){
			case 0: // no error.
				got_code_0 = 1;
				break;
			case 1: // find errors.
				got_code_1 = 1;
				break;
			case 2: // proceeding...
				got_code_2 = 1;
				break;
			default: // don't or can't support.
				got_code_3 = 1;
				break;
		}
	}
	
	if(got_code_1){
		// red
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -6px';
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 99%';
	}
	else if(got_code_2 || got_code_3){
		// white
	}
	else{
		// blue
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -5px';
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 50%';
	}
}

function check_wan_unit(){   //To check wan_unit, if USB Modem plug in change wan_unit to 1
	if(wan0_primary == 1 && document.form.wan_unit.value == 1)
		change_wan_unit(0);
	else if(wan1_primary == 1 && document.form.wan_unit.value == 0)
		change_wan_unit(1);
}
function change_wan_unit(wan_unit_flag){
	document.form.wan_unit.value = wan_unit_flag;	
	document.form.wl_auth_mode_x.disabled = true;	
	document.form.wl_wep_x.disabled = true;		
	FormActions("/apply.cgi", "change_wan_unit", "", "");
	document.form.submit();
}

function show_ddns_fail_hint() {
	var str="";
	if( !((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2")) )
		str = "<#Disconnected#>";
	else if(ddns_server = 'WWW.ASUS.COM') {
		var ddnsHint = getDDNSState(ddns_return_code, "<%nvram_get("ddns_hostname_x");%>", "<%nvram_get("ddns_old_name");%>");
		if(ddnsHint != "")
			str = ddnsHint;
	}
	else 
		str = "<#LANHostConfig_x_DDNS_alarm_2#>";

	overlib(str);
}

function check_dualwan(flag){
	if(flag == 0){		//single wan
		$('single_wan_icon').style.display = "";
		$('single_wan_status').style.display = "";
		$('single_wan_line').style.display = "";
		$('primary_wan_icon').style.display = "none";
		$('secondary_wan_icon').style.display = "none";
		$('primary_wan_line').style.display = "none";
		$('secondary_wan_line').style.display = "none";
		$('dual_wan_gap').style.display = "none";
	}
	else{
		$('single_wan_icon').style.display = "none";
		$('single_wan_status').style.display = "none";
		$('single_wan_line').style.display = "none";
		$('primary_wan_icon').style.display = "";
		$('secondary_wan_icon').style.display = "";
		$('primary_wan_line').style.display = "";
		$('secondary_wan_line').style.display = "";
		$('dual_wan_gap').style.display = "";
	}
}

function update_wan_status(e) {
	if(stopFlag == 1) return false;

	$j.ajax({
		url: '/status.asp',
		dataType: 'script', 

		error: function(xhr) {
			setTimeout("update_wan_status();", 3000);
		},
		success: function(response) {
			wanlink_status = wanlink_statusstr();
			wanlink_ipaddr = wanlink_ipaddr();
			secondary_wanlink_status = secondary_wanlink_statusstr();
			secondary_wanlink_ipaddr = secondary_wanlink_ipaddr();	
			change_wan_state(wanlink_status,secondary_wanlink_status);
			setTimeout("update_wan_status();", 3000);
		}
	});
}

function change_wan_state(primary_status, secondary_status){
	if (!dualWAN_support)
		return true;

	if(wans_mode == "fo" || wans_mode == "fb"){
		if(wan_unit == 0){
			$('primary_status').innerHTML = primary_status;
			if(primary_status == "Disconnected"){				
				$('primary_line').className = "primary_wan_disconnected";
			}	
			else{
				$('primary_line').className = "primary_wan_connected";
			}
			
			if(secondary_wanlink_ipaddr != '0.0.0.0' && secondary_status != 'Disconnected')
				secondary_status = "Standby";	
				
			$('seconday_status').innerHTML = secondary_status;	
			if(secondary_status == 'Disconnected'){
				$('secondary_line').className = "secondary_wan_disconnected";
			}	
			else if(secondary_status == 'Standby'){
				$('secondary_line').className = "secondary_wan_standby";
			}	
			else{
				$('secondary_line').className = "secondary_wan_connected";
			}
		}
		else{	//wan_unit : 1
			if(wanlink_ipaddr != '0.0.0.0' && primary_status != 'Disconnected')
				primary_status = "Standby";
				
			$('primary_status').innerHTML = primary_status;
			if(primary_status == 'Disconnected'){
				$('primary_line').className = "primary_wan_disconnected";
			}	
			else if(primary_status == 'Standby'){
				$('primary_line').className = "primary_wan_standby";
			}	
			else{
				$('primary_line').className = "primary_wan_connected";
			}
			
			$('seconday_status').innerHTML = secondary_status;
			if(secondary_status == "Disconnected"){
				$('secondary_line').className = "secondary_wan_disconnected";
			}	
			else{
				$('secondary_line').className = "secondary_wan_connected";
			}
		}	
	}
	else{	//lb
		$('primary_status').innerHTML = primary_status;
		$('seconday_status').innerHTML = secondary_status;
		if(primary_status == "Disconnected"){				
			$('primary_line').className = "primary_wan_disconnected";
		}	
		else{
			$('primary_line').className = "primary_wan_connected";
		}
		
		if(secondary_status == "Disconnected"){
			$('secondary_line').className = "secondary_wan_disconnected";
		}	
		else{
			$('secondary_line').className = "secondary_wan_connected";
		}	
	}
}

function validForm(){
	var validateIpRange = function(ip_obj){
		var retFlag = 1
		var ip_num = inet_network(ip_obj.value);
		
		if(ip_num <= 0){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.value = document.getElementById("ipaddr_field_orig").value;
			ip_obj.focus();
			retFlag = 0;
		}
		else if(ip_num <= getSubnet('<% nvram_get("lan_ipaddr"); %>', '<% nvram_get("lan_netmask"); %>', "head") ||
			 ip_num >= getSubnet('<% nvram_get("lan_ipaddr"); %>', '<% nvram_get("lan_netmask"); %>', "end")){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.value = document.getElementById("ipaddr_field_orig").value;
			ip_obj.focus();
			retFlag = 0;
		}
		else if(!validator.validIPForm(document.getElementById("ipaddr_field"), 0)){
			ip_obj.value = document.getElementById("ipaddr_field_orig").value;
			ip_obj.focus();
			retFlag = 0;
		}

		document.list_form.dhcp_staticlist.value.split("<").forEach(function(element, index){
			if(element.indexOf(document.getElementById("ipaddr_field").value) != -1){
				if(element.indexOf(document.getElementById("macaddr_field").value) == -1){
					alert("<#JS_duplicate#>");
					ip_obj.value = document.getElementById("ipaddr_field_orig").value;
					ip_obj.focus();
					retFlag = 0;
				}
			}
		});
		
		return retFlag;
	}

	if(validateIpRange(document.getElementById("ipaddr_field")) == true ){		
		if(document.getElementById("ipLockIcon").className != "dhcp") {	// only ipLockIcon is lock then update dhcp_staticlist
			document.list_form.dhcp_staticlist.value.split("<").forEach(function(element, index){
				if(element.indexOf(document.getElementById("macaddr_field").value) != -1){
					var tmpArray = document.list_form.dhcp_staticlist.value.split("<")
					tmpArray[index] = document.getElementById("macaddr_field").value;
					tmpArray[index] += ">";
					tmpArray[index] += document.getElementById("ipaddr_field").value;
					tmpArray[index] += ">";
					tmpArray[index] += document.getElementById("client_name").value;
					document.list_form.dhcp_staticlist.value = tmpArray.join("<");
				}
			});
		}

	}
	else		
		return false;

	showtext($("alert_msg1"), "");

	if(document.getElementById('client_name').value.length == 0){
		alert("<#File_Pop_content_alert_desc1#>");
		document.getElementById('client_name').focus();
		document.getElementById('client_name').select();
		return false;
	}
	else if(document.getElementById('client_name').value.indexOf(">") != -1 || document.getElementById('client_name').value.indexOf("<") != -1){
		alert("<#JS_validstr2#> '<', '>'");
		document.getElementById('client_name').focus();
		document.getElementById('client_name').select();
		document.getElementById('client_name').value = "";	
		return false;
	}
	return true;
}

var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
function edit_confirm(){
	if(validForm()){
		document.list_form.custom_clientlist.disabled = false;
		// customize device name
		var originalCustomListArray = new Array();
		var onEditClient = new Array();

		originalCustomListArray = custom_name.split('<');
		onEditClient[0] = document.getElementById('client_name').value.trim();
		onEditClient[1] = document.getElementById('macaddr_field').value;
		onEditClient[2] = 0;
		onEditClient[3] = document.getElementById('client_image').className.replace("type", "");
		onEditClient[4] = "";
		onEditClient[5] = "";

		for(var i=0; i<originalCustomListArray.length; i++){
			if(originalCustomListArray[i].split('>')[1] == onEditClient[1]){
				onEditClient[4] = originalCustomListArray[i].split('>')[4]; // set back callback for ROG device
				onEditClient[5] = originalCustomListArray[i].split('>')[5]; // set back keeparp for ROG device
				originalCustomListArray.splice(i, 1); // remove the selected client from original list
			}
		}

		originalCustomListArray.push(onEditClient.join('>'));
		custom_name = originalCustomListArray.join('<');
		document.list_form.custom_clientlist.value = custom_name;

		// static IP list
		if(document.list_form.dhcp_staticlist.value == dhcp_staticlist_orig){
			document.list_form.action_script.value = "saveNvram";
			document.list_form.action_wait.value = "1";
			document.list_form.flag.value = "background";
			document.list_form.dhcp_staticlist.disabled = true;
			document.list_form.dhcp_static_x.disabled = true;
			dhcp_staticlist_orig = document.list_form.dhcp_staticlist.value;
		}
		else {
			document.list_form.action_script.value = "restart_net_and_phy";
			document.list_form.action_wait.value = "35";
			document.list_form.flag.value = "";
			document.list_form.dhcp_staticlist.disabled = false;
			document.list_form.dhcp_static_x.value = 1;
			document.list_form.dhcp_static_x.disabled = false;
		}

		// handle user image
		document.list_form.custom_usericon.disabled = true;
		if(usericon_support) {
			document.list_form.custom_usericon.disabled = false;
			var clientMac = document.getElementById("macaddr_field").value.replace(/\:/g, "");
			if(userIconBase64 != "NoIcon") {
				document.list_form.custom_usericon.value = clientMac + ">" + userIconBase64;
			}
			else {
				document.list_form.custom_usericon.value = clientMac + ">noupload";
			}
		}

		// submit list_form
		document.list_form.submit();

		// display waiting effect
		if(document.list_form.flag.value == "background"){
			$("loadingIcon").style.display = "";
			setTimeout(function(){
				document.getElementById("statusframe").contentWindow.refreshpage();
			}, document.list_form.action_wait.value * 1000);
		}
		else{
			hideEditBlock(); 
			setTimeout(function(){
				refreshpage();
			}, document.list_form.action_wait.value * 1000);
		}
	}		
}

function edit_cancel(){
	$('edit_client_block').style.display = "none";
	$("hiddenMask").style.visibility = "hidden";
	$("dr_sweet_advise").style.display = "";
	show_custom_image("cancel");

	// disable event listener
	$j(document).mouseup(function(e){});
	$j("#statusframe").contents().mouseup(function(e){});
}

function edit_delete(){
	var target_mac = $('macaddr_field').value;
	var custom_name_row = custom_name.split('<');
	var custom_name_row_temp = custom_name.split('<');
	var custom_name_temp = "";
	var match_delete_flag = 0;
	
	for(i=0;i<custom_name_row.length;i++){
		var custom_name_col = custom_name_row[i].split('>');

		if(target_mac == custom_name_col[1]){
			match_delete_flag = 1;
		}
		else{
			if(custom_name_temp != ""){
				custom_name_temp += "<";
			}
		
			for(j=0;j< custom_name_col.length;j++){
				if(j == custom_name_col.length-1)
					custom_name_temp += custom_name_col[j];
				else	
					custom_name_temp += custom_name_col[j] + ">";				
			}
		}
	}
	
	if(match_delete_flag == 1){
		document.list_form.custom_clientlist.value = custom_name_temp;
		custom_name = custom_name_temp;
		$("loadingIcon").style.display = "";
		document.list_form.submit();
		document.getElementById("statusframe").contentWindow.refreshpage();

		setTimeout("$('loadingIcon').style.display='none'", 3500);
		setTimeout("$('deleteBtn').style.display='none'", 3500);
	}
}

function show_custom_image(flag){
	editClientImageFlag = true;
	if(flag == "cancel"){	
		$('edit_client_block').style.height = "250px";
		$j('#custom_image').fadeOut(100);	
	}	
	else if($('custom_image').style.display == "none"){
		$('edit_client_block').style.height = "400px";
		$j('#custom_image').fadeIn(200);		
	}
	else{
		$('edit_client_block').style.height = "250px";
		$j('#custom_image').fadeOut(100);	
	}	
}

function hide_custom_image(){
	$('edit_client_block').style.height = "250px";
	$j('#custom_image').fadeOut(100);	
}

function select_image(type){
	var sequence = type.substring(4,type.length);
	document.getElementById("client_image").style.display = "none";
	document.getElementById("canvasUserIcon").style.display = "none";
	$j('#custom_image').fadeOut(100);
	$('edit_client_block').style.height = "250px";
	document.getElementById('client_image').className = type;

	var userImageFlag = false;
	if(!editClientImageFlag) {
		if(usericon_support) {
			var clientMac = document.getElementById('macaddr_field').value.replace(/\:/g, "");
			userIconBase64 = getUploadIcon(clientMac);
			if(userIconBase64 != "NoIcon") {
				document.getElementById("client_image").style.display = "none";
				document.getElementById("canvasUserIcon").style.display = "";
				var img = document.createElement("img");
				img.src = userIconBase64;
				var canvas = document.getElementById("canvasUserIcon");
				var ctx = canvas.getContext("2d");
				ctx.clearRect(0,0,69,69);
				ctx.drawImage(img, 0, 0, 69, 69);
				userImageFlag = true;
			}
		}
	}

	if(!userImageFlag) {
		userIconBase64 = "NoIcon";
		document.getElementById("client_image").style.display = "";
		if(type == "type0" || type == "type6")
			document.getElementById('client_image').style.backgroundSize = "131px";
		else
			document.getElementById('client_image').style.backgroundSize = "130px";
	}
}

function hideEditBlock(){
	document.getElementById('edit_client_block').style.display = "none";
	document.getElementById('edit_usericon_block').style.display = "none";
	document.getElementById('loadingIcon').style.display = 'none';
	document.getElementById('loadingUserIcon').style.display = 'none';
	document.getElementById('deleteBtn').style.display ='none';
}

//check user icon num is over 100 or not.
function userIconNumLimit(mac) {
	var flag = true;
	var uploadIconMacList = getUploadIconList().replace(/\.log/g, "");
	var selectMac = mac.replace(/\:/g, "");
	var existFlag = (uploadIconMacList.search(selectMac) == -1) ? false : true;
	//check mac exist or not
	if(!existFlag) {
		var userIconCount = getUploadIconCount();
		if(userIconCount >= 100) {	//mac not exist, need check use icnon number whether over 100 or not.
			flag = false;
		}
	}
	return flag;
}

function popupEditBlock(clientObj){
	document.getElementById("divUserIcon").style.display = "none";
	//1.check rc_support
	if(usericon_support) {
		//2.check browswer support File Reader and Canvas or not.
		if(isSupportFileReader() && isSupportCanvas()) {
			document.getElementById("divUserIcon").style.display = "";
			//Setting drop event
			var holder = document.getElementById("divDropClientImage");
			holder.ondragover = function () { return false; };
			holder.ondragend = function () { return false; };
			holder.ondrop = function (e) {
				e.preventDefault();
				var userIconLimitFlag = userIconNumLimit(document.getElementById("macaddr_field").value);
				if(userIconLimitFlag) {	//not over 100	
					var file = e.dataTransfer.files[0];
					//check image
					if(file.type.search("image") != -1) {
						document.getElementById("client_image").style.display = "none";
						document.getElementById("canvasUserIcon").style.display = "";
						var reader = new FileReader();
						reader.onload = function (event) {
							var img = document.createElement("img");
							img.src = event.target.result;
							var canvas = document.getElementById("canvasUserIcon");
							var ctx = canvas.getContext("2d");
							ctx.clearRect(0,0,69,69);
							setTimeout(function() {
								ctx.drawImage(img, 0, 0, 69, 69);
								var dataURL = canvas.toDataURL("image/jpeg");
								userIconBase64 = dataURL;
							}, 100); //for firefox FPS(Frames per Second) issue need delay
						};
						reader.readAsDataURL(file);
						return false;
					}
					else {
						alert("<#Setting_upload_hint#>");
						return false;
					}
				}
				else {	//over 100 then let usee select delete icon or nothing
					showClientIconList();
				}
			};
		} 
	}

	editClientImageFlag = false;

	if(document.getElementById("edit_client_block").style.display != "none" && document.getElementById('client_name').value == clientObj.name){
		$j("#edit_client_block").fadeOut(300);
	}
	else{
		document.getElementById('client_name').value = clientObj.name;
		document.getElementById('ipaddr_field_orig').value = clientObj.ip;
		document.getElementById('ipaddr_field').value = clientObj.ip;
		document.getElementById('macaddr_field').value = clientObj.mac;
		select_image("type" + parseInt(clientObj.type));
		if(dhcp_staticlist_orig.search(clientObj.mac + ">" + clientObj.ip) != -1) { //check mac>ip is combination the the ipLockIcon is manual
			document.getElementById("ipLockIcon").className = "manual";
		}
		else {
			document.getElementById("ipLockIcon").className = "dhcp";
		}

		// hide block btn
		// document.getElementById("blockBtn").style.display = (clientObj.isWL && document.maclist_form.wl0_macmode.value != "allow") ? "" : "none";
		cal_panel_block("edit_client_block");
		$j("#edit_client_block").fadeIn(300);
	}

	// hide client panel 
	$j(document).mouseup(function(e){
		if(!$j("#edit_client_block").is(e.target) && $j("#edit_client_block").has(e.target).length === 0 && !userIconHideFlag) {
			setTimeout( function() {userIconHideFlag = false;}, 1000);
			edit_cancel();
		}
		else {
			setTimeout( function() {userIconHideFlag = false;}, 1000);
		}
	});
	$j("#statusframe").contents().mouseup(function(e){
		if(!$j("#edit_client_block").is(e.target) && $j("#edit_client_block").has(e.target).length === 0 && !userIconHideFlag) {
			setTimeout( function() {userIconHideFlag = false;}, 1000);
			edit_cancel();
		}
		else {
			setTimeout( function() {userIconHideFlag = false;}, 1000);
		}
	});
}

function cal_panel_block(obj){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1020){	
		winPadding = (winWidth-1020)/2;	
		winWidth = 1040;
		blockmarginLeft= (winWidth*0.23)+winPadding;
	}
	else if(winWidth <=1020){
		blockmarginLeft= (winWidth)*0.23+document.body.scrollLeft;	
	}

	document.getElementById(obj).style.marginLeft = blockmarginLeft + "px";
}

function check_usb3(){
	if(based_modelid == "DSL-AC68U" || based_modelid == "RT-AC3200" || based_modelid == "RT-AC87U" || based_modelid == "RT-AC69U" || based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" || based_modelid == "RT-AC55U" || based_modelid == "RT-N18U" || based_modelid == "TM-AC1900"){
		document.getElementById('usb1_image').src = "images/New_ui/networkmap/USB3.png";
	}
	else if(based_modelid == "RT-N65U"){
		document.getElementById('usb1_image').src = "images/New_ui/networkmap/USB3.png";
		document.getElementById('usb2_image').src = "images/New_ui/networkmap/USB3.png";
	}
}


function addToList(macAddr){
	if(document.list_form.dhcp_staticlist.value.indexOf(macAddr) == -1){
		document.list_form.dhcp_staticlist.value += "<";
		document.list_form.dhcp_staticlist.value += macAddr;
		document.list_form.dhcp_staticlist.value += ">";
		document.list_form.dhcp_staticlist.value += document.getElementById("ipaddr_field").value;
		document.list_form.dhcp_staticlist.value += ">";
		document.list_form.dhcp_staticlist.value += document.getElementById("client_name").value;
	}
}

function delFromList(macAddr){
	document.list_form.dhcp_staticlist.value.split("<").forEach(function(element, index){
		if(element.indexOf(macAddr) != -1){
			var tmpArray = document.list_form.dhcp_staticlist.value.split("<")
			tmpArray.splice(index, 1);
			document.list_form.dhcp_staticlist.value = tmpArray.join("<");
		}
	})
}

function showClientIconList() {
	var confirmFlag = true;
	confirmFlag = confirm("The client icon over upload limting, please remove at least one client icon then try to upload again.");
	if(confirmFlag) {
		edit_cancel();
		$j("#edit_usericon_block").fadeIn(10);
		cal_panel_block("edit_usericon_block");
		showClientIcon();
		document.getElementById("uploadIcon").value = "";
		return false;
	}
	else {
		document.getElementById("uploadIcon").value = "";
		return false;
	}
}

function showClientIcon() {
	genClientList();
	var uploadIconMacList = getUploadIconList().replace(/\.log/g, "");
	var custom_usericon_row = uploadIconMacList.split('>');
	var code = "";
	var clientIcon = "";
	var custom_usericon_length = custom_usericon_row.length;
	code +='<table width="95%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="usericon_table">';
	if(custom_usericon_length == 1) {
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
		document.getElementById('edit_usericon_block').style.height = "145px";
	}
	else {
		for(var i = 0; i < custom_usericon_length; i += 1) {
			if(custom_usericon_row[i] != "") {
				var formatMac = custom_usericon_row[i].slice(0,2) + ":" + custom_usericon_row[i].slice(2,4) + ":" + custom_usericon_row[i].slice(4,6) + ":" + 
								custom_usericon_row[i].slice(6,8) + ":" + custom_usericon_row[i].slice(8,10)+ ":" + custom_usericon_row[i].slice(10,12);
				code +='<tr id="row' + i + '">';
				var clientObj = clientList[formatMac];
				var clientName = "";
				if(clientObj != undefined) {
					clientName = clientObj.name;
				}
				code +='<td width="45%">'+ clientName +'</td>';
				code +='<td width="30%">'+ formatMac +'</td>';
				clientIcon = getUploadIcon(custom_usericon_row[i]);
				code +='<td width="15%"><img id="imgClientIcon_'+ i +'" class="imgClientIcon" src="' + clientIcon + '"</td>';
				code +='<td width="10%"><input class="remove_btn" onclick="delClientIcon(this);" value=""/></td></tr>';
			}
		}
		document.getElementById('edit_usericon_block').style.height = (61 * custom_usericon_length + 50) + "px";
	}
	code +='</table>';
	document.getElementById("usericon_block").innerHTML = code;
};

function delClientIcon(rowdata) {
	var delIdx = rowdata.parentNode.parentNode.rowIndex;
	var delMac = rowdata.parentNode.parentNode.childNodes[1].innerHTML;
	document.getElementById("usericon_table").deleteRow(delIdx);
	custom_usericon_del += delMac + ">";
	var trCount = $j( "#usericon_table tr" ).length;
	document.getElementById('edit_usericon_block').style.height = (61 * (trCount + 1) + 50) + "px";
	if(trCount == 0) {
		var code = "";
		code +='<table width="95%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="usericon_table">';
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
		code +='</table>';
		document.getElementById('edit_usericon_block').style.height = "145px";
		document.getElementById("usericon_block").innerHTML = code;
	}
}

function btUserIconEdit() {
	document.list_form.custom_clientlist.disabled = true;
	document.list_form.dhcp_staticlist.disabled = true;
	document.list_form.custom_usericon.disabled = true;
	document.list_form.custom_usericon_del.disabled = false;
	document.list_form.custom_usericon_del.value = custom_usericon_del.replace(/\:/g, "");

	// submit list_form
	document.list_form.submit();
	document.getElementById("loadingUserIcon").style.display = "";
	setTimeout(function(){
		document.getElementById("statusframe").contentWindow.refreshpage();
		custom_usericon_del = "";
		document.list_form.custom_usericon_del.disabled = true;
	}, document.list_form.action_wait.value * 1000);
}
function btUserIconCancel() {
	custom_usericon_del = "";
	$j("#edit_usericon_block").fadeOut(100);
}

function previewImage(imageObj) {
	var userIconLimitFlag = userIconNumLimit(document.getElementById("macaddr_field").value);
	
	if(userIconLimitFlag) {	//not over 100
		var checkImageExtension = function (imageFileObject) {
		var  picExtension= /\.(jpg|jpeg|gif|png|bmp|ico)$/i;  //analy extension
			if (picExtension.test(imageFileObject)) 
				return true;
			else
				return false;
		};

		//1.check image extension
		if (!checkImageExtension(imageObj.value)) {
			alert("<#Setting_upload_hint#>");
			imageObj.focus();
		}
		else {
			//2.Re-drow image
			document.getElementById("client_image").style.display = "none";
			document.getElementById("canvasUserIcon").style.display = "";
			var fileReader = new FileReader(); 
			fileReader.onload = function (fileReader) {
				var img = document.createElement("img");
				img.src = fileReader.target.result;
				var canvas = document.getElementById("canvasUserIcon");
				var ctx = canvas.getContext("2d");
				ctx.clearRect(0,0,69,69);
				setTimeout(function() {
					ctx.drawImage(img, 0, 0, 69, 69);
					var dataURL = canvas.toDataURL("image/jpeg");
					userIconBase64 = dataURL;
				}, 100); //for firefox FPS(Frames per Second) issue need delay
			}
			fileReader.readAsDataURL(imageObj.files[0]);
			$j('#custom_image').fadeOut(100);
			userIconHideFlag = true;
			document.getElementById('edit_client_block').style.height = "250px";
		}
	}
	else {	//over 100 then let usee select delete icon or nothing
		showClientIconList();
	}
}
</script>
</head>

<body onunload="return unload_body();">
<noscript>
	<div class="popup_bg" style="visibility:visible; z-index:999;">
		<div style="margin:200px auto; width:300px; background-color:#006699; color:#FFFFFF; line-height:150%; border:3px solid #FFF; padding:5px;"><#not_support_script#></p></div>
	</div>
</noscript>

<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br>
				<div id="disconnect_hint" style="display:none;"><#Main_alert_proceeding_desc2#></div>
				<br>
		    </div>
			<div id="wireless_client_detect" style="margin-left:10px;position:absolute;display:none">
				<img src="images/loading.gif">
				<div style="margin:-45px 0 0 75px;"><#QKSet_Internet_Setup_fail_method1#></div>
			</div> 
			<div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:100px; "></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="index.asp">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_auth_mode_x" value="<% nvram_get("wl0_auth_mode_x"); %>">
<input type="hidden" name="wl_wep_x" value="<% nvram_get("wl0_wep_x"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="apps_action" value="">
<input type="hidden" name="apps_path" value="">
<input type="hidden" name="apps_name" value="">
<input type="hidden" name="apps_flag" value="">
<input type="hidden" name="wan_unit" value="<% nvram_get("wan_unit"); %>">
<input type="hidden" name="dual_wan_flag" value="">
</form>
<!-- Start for Editing client list-->
<form method="post" name="list_form" id="list_form" action="/start_apply2.htm" target="hidden_frame">
	<input type="hidden" name="current_page" value="index.asp">
	<input type="hidden" name="next_page" value="index.asp">
	<input type="hidden" name="modified" value="0">
	<input type="hidden" name="flag" value="background">
	<input type="hidden" name="action_mode" value="apply">
	<input type="hidden" name="action_script" value="saveNvram">
	<input type="hidden" name="action_wait" value="1">
	<input type="hidden" name="custom_clientlist" value="">
	<input type="hidden" name="dhcp_staticlist" value="" disabled>
	<input type="hidden" name="dhcp_static_x" value='<% nvram_get("dhcp_static_x"); %>' disabled>
	<input type="hidden" name="custom_usericon" value="">
	<input type="hidden" name="custom_usericon_del" value="" disabled>
</form>

<form method="post" name="maclist_form" id="maclist_form" action="/start_apply2.htm" target="hidden_frame">
	<input type="hidden" name="current_page" value="index.asp">
	<input type="hidden" name="next_page" value="index.asp">
	<input type="hidden" name="modified" value="0">
	<input type="hidden" name="flag" value="">
	<input type="hidden" name="action_mode" value="apply_new">
	<input type="hidden" name="action_script" value="restart_wireless">
	<input type="hidden" name="action_wait" value="5">
	<input type="hidden" name="wl0_maclist_x" value="<% nvram_get("wl0_maclist_x"); %>">
	<input type="hidden" name="wl1_maclist_x" value="<% nvram_get("wl1_maclist_x"); %>">
	<input type="hidden" name="wl2_maclist_x" value="<% nvram_get("wl2_maclist_x"); %>">
	<input type="hidden" name="wl0_macmode" value="deny">
	<input type="hidden" name="wl1_macmode" value="deny">
	<input type="hidden" name="wl2_macmode" value="deny">
</form>
<!-- update Client List -->
<form method="post" name="networkmapdRefresh" action="/apply.cgi" target="hidden_frame">
	<input type="hidden" name="action_mode" value="update_client_list">
	<input type="hidden" name="action_script" value="">
	<input type="hidden" name="action_wait" value="1">
	<input type="hidden" name="current_page" value="httpd_check.xml">
	<input type="hidden" name="next_page" value="httpd_check.xml">
	<input type="hidden" name="client_info_tmp" value="">	
</form>

<div id="edit_usericon_block" class="contentM_usericon">
	<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
		<thead>
			<tr>
				<td colspan="4">Client upload icon&nbsp;(<#List_limit#>&nbsp;100)</td>
			</tr>
		</thead>
		<tr>
			<th width="45%">Client Name</th>
			<th width="30%">MAC</th>
			<th width="15%">Upload icon</th>
			<th width="10%"><#CTL_del#></th>
		</tr>
	</table>
	<div id="usericon_block"></div>
	<div style="margin-top:5px;padding-bottom:10px;width:100%;text-align:center;">
		<input class="button_gen" type="button" onclick="btUserIconCancel();" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button" onclick="btUserIconEdit();" value="<#CTL_ok#>">
		<img id="loadingUserIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
	</div>	
</div>
<div id="edit_client_block" class="contentM_qis">
	<div style="text-align:right"><img src="/images/button-close.gif" style="width:30px;cursor:pointer" onclick="edit_cancel();"></div>
	<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0" style="margin-top: -30px;width: 99%;padding:5px 10px 0px 10px;">
		<tr>
			<td colspan="2">
				<div style="margin:5px 0px -5px 5px;font-family:Arial, Helvetica, sans-serif;font-size:16px;font-weight:bolder">
					Client profile
				</div>
			</td>
		</tr>
		<tr>
			<td colspan="2">
				<img style="width:100%;height:2px" src="/images/New_ui/networkmap/linetwo2.png">
			</td>
		</tr>
		<tr>
			<td>
				<div style="background-color:#172327;border-radius:10px;width:105px;height:87px;padding-top:7px;" id="divDropClientImage">
					<div id="client_image" onclick="show_custom_image();"></div>
					<canvas id="canvasUserIcon" width="69px" height="69px" onclick="show_custom_image();"></canvas>
				</div>
			</td>
			<td>
				<div>
					<table width="99%;"align="center" cellpadding="4" cellspacing="0">
						<tr>
							<td>
								<input id="client_name" name="client_name" type="text" value="" class="input_32_table disabled" maxlength="32" style="float:left;width:200px;">							
								<script>
									document.getElementById("client_name").onclick = function(){
										$j(this).removeClass("disabled");
									}
									document.getElementById("client_name").onblur = function(){
										if (document.getElementById("ipLockIcon").className == "static") {
											delFromList(document.getElementById("macaddr_field").value);
											addToList(document.getElementById("macaddr_field").value);
                                                                                }
										$j(this).addClass("disabled");
									}
								</script>
							</td>
						</tr>
						<tr>
							<td>
								<input id="macaddr_field" type="text" value="" class="input_32_table disabled" style="float:left;width:200px;" disabled>
							</td>
						</tr>
						<tr>
							<td>
								<input id="ipaddr_field_orig" type="hidden" value="" disabled="">
								<input id="ipaddr_field" type="text" value="" class="input_32_table disabled" style="float:left;width:200px;" onkeypress="return validator.isIPAddr(this,event)">
								<script>
									document.getElementById("ipaddr_field").onclick = function(){
										$j(this).removeClass("disabled");
									}
									document.getElementById("ipaddr_field").onblur = function(){
										$j(this).addClass("disabled");
									}
									document.getElementById("ipaddr_field").onkeypress = function(){
										document.getElementById("ipLockIcon").className = "manual";
										delFromList(document.getElementById("macaddr_field").value);
										addToList(document.getElementById("macaddr_field").value);
									}
								</script>

								<div style="margin-top: 2px;margin-left: 215px;"><div id="ipLockIcon" class="dhcp" title="Binding IP and MAC Address"></div></div>
								<script>
									document.getElementById("ipLockIcon").onclick = function(){
										if(this.className == "dhcp"){
											this.className = "manual";
											delFromList(document.getElementById("macaddr_field").value);
											addToList(document.getElementById("macaddr_field").value);
										}
										else{
											this.className = "dhcp";
											delFromList(document.getElementById("macaddr_field").value);
											document.getElementById("ipaddr_field").value = document.getElementById("ipaddr_field_orig").value;
										}
									}
								</script>
							</td>
						</tr>
					</table>
				</div>
			</td>
		</tr>

		<tr>
			<td colspan="2">
				<div id="custom_image" style="display:none;">
					<table width="99%;" border="1" align="center" cellpadding="4" cellspacing="0">
						<tr>
							<td>
								<div class="type1" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type2" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type4" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type5" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type7" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type8" onclick="select_image(this.className);"></div>
							</td>
						</tr>
						<tr>
							<td>
								<div class="type9" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type10" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type11" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type12" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type13" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type14" onclick="select_image(this.className);"></div>
							</td>
						</tr>
						<tr>
							<td>
								<div class="type15" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type16" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type17" onclick="select_image(this.className);"></div>
							</td>
							<td>
								<div class="type18" onclick="select_image(this.className);"></div>
							</td>
							<td id="tdUserIcon">
								<div id="divUserIcon">Upload
									<input type="file" name="uploadIcon" id="uploadIcon" onchange="previewImage(this);" />
								</div>
							</td>
							<td>
							</td>
						</tr>	
					</table>
		 		</div>	
			</td>
		</tr>
	</table>		
	<div style="margin-top:5px;padding-bottom:10px;width:100%;text-align:center;">
		<input class="button_gen" type="button" onclick="edit_delete();" id="deleteBtn" value="<#CTL_del#>" style="display:none;">
		<input class="button_gen" type="button" id="blockBtn" value="<#Block#>" title="<#block_client#>" style="display:none;">
		<script>
			document.maclist_form.wl0_maclist_x.value = (function(){
				var wl0_maclist_x_array = '<% nvram_get("wl0_maclist_x"); %>'.split("&#60");

				if(wl_info.band5g_support){
					'<% nvram_get("wl1_maclist_x"); %>'.split("&#60").forEach(function(element, index){
						if(wl0_maclist_x_array.indexOf(element) == -1) wl0_maclist_x_array.push(element);
					});
				}

				if(wl_info.band5g_2_support){
					'<% nvram_get("wl2_maclist_x"); %>'.split("&#60").forEach(function(element, index){
						if(wl0_maclist_x_array.indexOf(element) == -1) wl0_maclist_x_array.push(element);
					});
				}

				return wl0_maclist_x_array.join("<");
			})();

			document.getElementById("blockBtn").onclick = function(){
				document.maclist_form.wl0_maclist_x.value = document.maclist_form.wl0_maclist_x.value + "<" + document.getElementById("macaddr_field").value;
				document.maclist_form.wl1_maclist_x.value = document.maclist_form.wl0_maclist_x.value;
				document.maclist_form.wl2_maclist_x.value = document.maclist_form.wl0_maclist_x.value;
				document.maclist_form.submit();
				hideEditBlock();
			}
		</script>
		<input class="button_gen" type="button" onclick="edit_confirm();" value="<#CTL_ok#>">
		<img id="loadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
	</div>	
</div>
<!-- End for Editing client list-->	
<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td valign="top" width="17">&nbsp;</td>
		
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="204">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		
		<td align="left" valign="top" class="bgarrow">
		
		<!--=====Beginning of Network Map=====-->
		<div id="tabMenu"></div><br>
		<div id="NM_shift" style="margin-top:-150px;"></div>
		<div id="NM_table" class="NM_table" >
		<div id="NM_table_div">
			<table id="_NM_table" border="0" cellpadding="0" cellspacing="0" height="720" style="opacity:.95;" >
				<tr>
					<td width="40px" rowspan="11" valign="center"></td>
					<!--== Dual WAN ==-->
					<td id="primary_wan_icon" width="160px;" height="155" align="center" class="NM_radius" valign="middle" bgcolor="#444f53" onclick="showstausframe('Internet_primary');" style="display:none">
						<a href="/device-map/internet.asp" target="statusframe"><div id="iconInternet_primary" onclick="clickEvent(this);"></div></a>
						<div><#dualwan_primary#>:</div>
						<div><strong id="primary_status"></strong></div>
					</td>
					<td id="dual_wan_gap" width="40px" style="display:none">
					</td>
					<td id="secondary_wan_icon" width="160px;" height="155" align="center" class="NM_radius" valign="middle" bgcolor="#444f53" onclick="showstausframe('Internet_secondary');" style="display:none">
						<a href="/device-map/internet.asp" target="statusframe"><div id="iconInternet_secondary" onclick="clickEvent(this);"></div></a>
						<div><#dualwan_secondary#>:</div>
						<div><strong id="seconday_status"></strong></div>
					</td>
					<!--== single WAN ==-->
					<td id="single_wan_icon" height="115" align="right" class="NM_radius_left" valign="middle" bgcolor="#444f53" onclick="showstausframe('Internet');" >
						<a href="/device-map/internet.asp" target="statusframe"><div id="iconInternet" onclick="clickEvent(this);"></div></a>
					</td>
					<td id="single_wan_status" colspan="2" valign="middle" bgcolor="#444f53" class="NM_radius_right" onclick="" style="padding:5px;cursor:auto;width:180px;">
						<div>
							<span id="NM_connect_title" style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#statusTitle_Internet#>:</span>
							<br>
							<strong id="NM_connect_status" class="index_status" style="font-size:14px;"><#QKSet_Internet_Setup_fail_method1#>...</strong>
						</div>
						<div id="wanIP_div" style="margin-top:5px;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">WAN IP:</span>
							<strong id="index_status" class="index_status" style="font-size:14px;"></strong>
						</div>
						<div id="ddnsHostName_div" style="margin-top:5px;word-break:break-all;word-wrap:break-word;width:180px;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">DDNS:</span>
							<strong id="ddnsHostName" class="index_status" style="font-size:14px;"><#QIS_detectWAN_desc2#></strong>
							<span id="ddns_fail_hint" class="notificationoff" onClick="show_ddns_fail_hint();" onMouseOut="nd();"></span>
						</div>
						<div id="wlc_band_div" style="margin-top:5px;display:none">
							<span style="font-size:14px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#Interface#>:</span>
							<strong id="wlc_band_status" class="index_status" style="font-size:14px;"></strong>
						</div>
						<div id="dataRate_div" style="margin-top:5px;display:none">
							<span style="font-size:14px;font-family: Verdana, Arial, Helvetica, sans-serif;">Link rate:</span>
							<strong id="speed_status" class="index_status" style="font-size:14px;"></strong>
						</div>
						
					</td>
					<td width="40px" rowspan="11" valign="top">
						<div class="statusTitle" id="statusTitle_NM">
							<div id="helpname" style="padding-top:10px;font-size:16px;"></div>
						</div>							
						<div>
							<iframe id="statusframe" class="NM_radius_bottom" style="margin-left:45px;margin-top:-2px;" name="statusframe" width="320" height="735" frameborder="0" allowtransparency="true" style="background-color:transparent; margin-left:10px;" src="device-map/router_status.asp"></iframe>
						</div>
					</td>	
				</tr>			
				<tr>
					<!--==line of dual wan==-->
					<td id="primary_wan_line"  height="40px" style="display:none;">
						<div id="primary_line" class="primary_wan_connected"></div>
					</td>
					<td id="secondary_wan_line" colspan="2" height="40px"  style="display:none;">
						<div id="secondary_line" class="secondary_wan_connected"></div>
					</td>
					<!--==line of single wan==-->
					<td id="single_wan_line" colspan="3" align="center" height="19px">
						<div id="single_wan" class="single_wan_connected"></div>
					</td>
				</tr>			
				<tr>
					<td height="115" align="right" bgcolor="#444f53" class="NM_radius_left" onclick="showstausframe('Router');">
						<a href="device-map/router.asp" target="statusframe"><div id="iconRouter" onclick="clickEvent(this);"></div></a>
					</td>
					<td colspan="2" valign="middle" bgcolor="#444f53" class="NM_radius_right" onclick="showstausframe('Router');">
						<div>
						<span id="SmartConnectName" style="font-size:14px;font-family: Verdana, Arial, Helvetica, sans-serif; display:none">Smart Connect Status: </span>
						</div>
						<div>
						<strong id="SmartConnectStatus" class="index_status" style="font-size:14px; display:none"><a style="color:#FFF;text-decoration:underline;" href="/
						Advanced_Wireless_Content.asp">On</a></strong>
						</div>
						<span style="font-size:14px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#Security_Level#>: </span>
						<br/>  
						<strong id="wl_securitylevel_span" class="index_status"></strong>
						<img id="iflock">
						
					</td>

				</tr>			
				<tr>
					<td id="line3_td" colspan="3" align="center" height="52px">
					<img id="line3_img" src="/images/New_ui/networkmap/line_two.png">
					</td>
				</tr>
				<tr>
					<td id="clients_td" height="170" width="150" bgcolor="#444f53" align="center" valign="top" class="NM_radius" style="padding-bottom:15px;">
						<div id="clientsContainer" onclick="showstausframe('Client');">
							<a id="clientStatusLink" href="device-map/clients.asp" target="statusframe">
							<div id="iconClient" style="margin-top:20px;" onclick="clickEvent(this);"></div>
							</a>
							<div class="clients" id="clientNumber" style="cursor:pointer;"></div>
						</div>
						<!--div id="" onclick="">
							<img style="margin-top:15px;width:150px;height:2px" src="/images/New_ui/networkmap/linetwo2.png">
							<a id="" href="device-map/clients.asp" target="statusframe">
							<div id="iconClient" style="margin-top:20px;" onclick=""></div>
							</a>
							<div class="clients" id="" style="cursor:pointer;">Wireless Clients:</div>
						</div-->
					</td>

					<td width="36" rowspan="6" id="clientspace_td"></td>

					<td id="usb_td" width="160" bgcolor="#444f53" align="center" valign="top" class="NM_radius" style="padding-bottom:5px;">
						<div id="usbPathContainer_1" style="display:none">
							<div style="margin-top:20px;margin-bottom:10px;" id="deviceIcon_1"></div>
							<div><img id="usb1_image" src="images/New_ui/networkmap/USB2.png"></div>
							<div style="margin:10px 0px;">
								<span id="deviceText_1"></span>
								<select id="deviceOption_1" class="input_option" style="display:none;height:20px;width:130px;font-size:12px;"></select>	
							</div>
							<div id="deviceDec_1"></div>
						</div>
						<div id="usbPathContainer_2" style="display:none">
							<img style="margin-top:5px;width:150px;height:2px" src="/images/New_ui/networkmap/linetwo2.png">
							<div style="margin-top:15px;margin-bottom:10px;" id="deviceIcon_2"></div>
							<div><img id="usb2_image" src="images/New_ui/networkmap/USB2.png"></div>
							<div style="margin:10px 0px;">
								<span id="deviceText_2"></span>
								<select id="deviceOption_2" class="input_option" style="display:none;height:20px;width:130px;font-size:12px;"></select>	
							</div>						
							<div id="deviceDec_2"></div>
						</div>
					</td>
				</tr>
				<tr>
					<td id="space_block" colspan="3" align="center" height="150px" style="display:none;"></td>
				</tr>
			</table>
		</div>
	</div>
<!--==============Ending of hint content=============-->
    </td>
  </tr>
</table>
<div id="navtxt" class="navtext" style="position:absolute; top:50px; left:-100px; visibility:hidden; font-family:Arial, Verdana"></div>
<div id="footer"></div>
<select id="bgimg" onChange="customize_NM_table(this.value);" class="input_option_left" style="display:none;">
	<option value="wall0.gif">dark</option>
	<option value="wall1.gif">light</option>
</select>		

<script>
	if(flag == "Internet" || flag == "Client")
		$("statusframe").src = "";

	initial();

	document.getElementById('deviceOption_1').onchange = function(){
	 	require(['/require/modules/diskList.js'], function(diskList){
	 		diskList.update(function(){
		 		var usbDevicesList = diskList.list();
				show_USBDevice(usbDevicesList[document.getElementById('deviceOption_1').value]);
				setSelectedDiskOrder('iconUSBdisk_1');

				if(usbDevicesList[document.getElementById('deviceOption_1').value].deviceType == "modem")
					clickEvent(document.getElementById('iconModem_1'));
				else if(usbDevicesList[document.getElementById('deviceOption_1').value].deviceType == "printer")
					clickEvent(document.getElementById('iconPrinter_1'));
				else
					clickEvent(document.getElementById('iconUSBdisk_1'));
			});
		});
	}

	document.getElementById('deviceOption_2').onchange = function(){
	 	require(['/require/modules/diskList.js'], function(diskList){
	 		diskList.update(function(){
		 		var usbDevicesList = diskList.list();
				show_USBDevice(usbDevicesList[document.getElementById('deviceOption_2').value]);
				setSelectedDiskOrder('iconUSBdisk_2');

				if(usbDevicesList[document.getElementById('deviceOption_2').value].deviceType == "modem")
					clickEvent(document.getElementById('iconModem_2'));
				else if(usbDevicesList[document.getElementById('deviceOption_2').value].deviceType == "printer")
					clickEvent(document.getElementById('iconPrinter_2'));
				else
					clickEvent(document.getElementById('iconUSBdisk_2'));
			});
		});
	}

	var manualUpdate = false;
	setTimeout(function(){
		document.networkmapdRefresh.submit();
	}, 3500);
</script>
</body>
</html>
