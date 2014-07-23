<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE9"/>
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
#custom_image td:hover{
	border-radius: 7px;
	background-color:#84C1FF;
}
</style>
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();	

if(location.pathname == "/"){
	if('<% nvram_get("x_Setting"); %>' == '0')
		location.href = '/QIS_wizard.htm?flag=welcome';
	else if('<% nvram_get("w_Setting"); %>' == '0' && sw_mode != 2)
		location.href = '/QIS_wizard.htm?flag=wireless';
}
<% login_state_hook(); %>


// Live Update
var webs_state_update= '<% nvram_get("webs_state_update"); %>';
var webs_state_error= '<% nvram_get("webs_state_error"); %>';
var webs_state_info= '<% nvram_get("webs_state_info"); %>';


// WAN
<% wanlink(); %>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var Dev3G = '<% nvram_get("d3g"); %>';
var flag = '<% get_parameter("flag"); %>';
var wan0_primary = '<% nvram_get("wan0_primary"); %>';
var wan1_primary = '<% nvram_get("wan1_primary"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_mode = '<%nvram_get("wans_mode");%>';	
if(wans_dualwan_orig.search(" ") == -1)
	var wans_flag = 0;
else
	var wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;


// Client list
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_list_array = '<% get_client_detail_info(); %>';

// USB function
var currentUsbPort = new Number();
var usbPorts = new Array();

// Wireless
var wlc_band = '<% nvram_get("wlc_band"); %>';
window.onresize = cal_panel_block;	

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

	var clientNumber = '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<').length - 1;
	show_client_status(clientNumber);		

	if(!parent.usb_support || usbPortMax == 0){
		$("line3_td").height = '40px';
		$("line3_img").src = '/images/New_ui/networkmap/line_dualwan.png';
		$("clients_tr").colSpan = "3";
		$("clients_tr").className = 'NM_radius';
		$("clients_tr").width = '350';
		$("clientspace_td").style.display = "none";
		$("usb1_html").style.display = "none";
		$("usb2_html").style.display = "none";
		$("bottomspace_tr").style.display = "";
	}

	for(var i=0; i<usbPortMax; i++){
		var tmpDisk = new newDisk();
		tmpDisk.usbPath = i+1;
		show_USBDevice(tmpDisk);
		$("usbPathContianer_"+parseInt(i+1)).style.display = "";
	}
	
	for(var i=0; i<usbDevices.length; i++){
	  var new_option = new Option(usbDevices[i].deviceName, usbDevices[i].deviceIndex);
		document.getElementById('deviceOption_'+usbDevices[i].usbPath).options.add(new_option);
		document.getElementById('deviceOption_'+usbDevices[i].usbPath).style.display = "";

		if(typeof usbPorts[usbDevices[i].usbPath-1].deviceType == "undefined" || usbPorts[usbDevices[i].usbPath-1].deviceType == "")
			show_USBDevice(usbDevices[i]);
	}

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
	}
	else{
		$("index_status").innerHTML = '<span style="word-break:break-all;">' + wanlink_ipaddr() + '</span>'

		setTimeout("show_ddns_status();", 2000);
		
		if(wanlink_ipaddr() == '0.0.0.0' || wanlink_ipaddr() == '')
			$("wanIP_div").style.display = "none";
	}

	var NM_table_img = getCookie("NM_table_img");
	if(NM_table_img != "" && NM_table_img != null){
		customize_NM_table(NM_table_img);
		$("bgimg").options[NM_table_img[4]].selected = 1;
	}
	update_wan_status();
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
	setCookie(img);
}

function setCookie(color){
	document.cookie = "NM_table_img=" + color;
}

function getCookie(c_name){
	if (document.cookie.length>0){ 
		c_start=document.cookie.indexOf(c_name + "=");
		if (c_start!=-1){ 
			c_start=c_start + c_name.length+1;
			c_end=document.cookie.indexOf(";",c_start);
			if (c_end==-1) c_end=document.cookie.length;
			return unescape(document.cookie.substring(c_start,c_end));
		} 
	}
	return null;
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

		dec_html_code += '<p id="diskDesc'+ device.usbPath +'" style="margin-top:5px;"><#Availablespace#>:</p><div id="diskquota" align="left" style="margin-top:5px;margin-bottom:10px;">\n';
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
	dec_html_code += '<div style="margin:20px" id="noUSB'+ device_seat +'">';

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
			lastClicked.style.backgroundPosition = '0% -3px';
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
	else if(obj.id.indexOf("Client") > 0){
		if(wan_route_x == "IP_Bridged")
			return;

		icon = "iconClient";
	}
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
	document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
	document.getElementById('iconUSBdisk_'+diskOrder).style.position = "absolute";
	document.getElementById('iconUSBdisk_'+diskOrder).style.marginTop = "0px";

	/*if(navigator.appName.indexOf("Microsoft") >= 0)
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "0px";
	else*/
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "35px";

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
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 101%';
	}
	else if(got_code_2 || got_code_3){
		// white
	}
	else{
		// blue
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
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
		if(ddns_return_code == 'register,203')
               alert("<#LANHostConfig_x_DDNS_alarm_hostname#> '<%nvram_get("ddns_hostname_x");%>' <#LANHostConfig_x_DDNS_alarm_registered#>");
        else if(ddns_return_code.indexOf('233')!=-1)
                str = "<#LANHostConfig_x_DDNS_alarm_hostname#> '<%nvram_get("ddns_hostname_x");%>' <#LANHostConfig_x_DDNS_alarm_registered_2#> '<%nvram_get("ddns_old_name");%>'";
	  	else if(ddns_return_code.indexOf('297')!=-1)
        		str = "<#LANHostConfig_x_DDNS_alarm_7#>";
	  	else if(ddns_return_code.indexOf('298')!=-1)
    			str = "<#LANHostConfig_x_DDNS_alarm_8#>";
	  	else if(ddns_return_code.indexOf('299')!=-1)
    			str = "<#LANHostConfig_x_DDNS_alarm_9#>";
  		else if(ddns_return_code.indexOf('401')!=-1)
	    		str = "<#LANHostConfig_x_DDNS_alarm_10#>";
	  	else if(ddns_return_code.indexOf('407')!=-1)
    			str = "<#LANHostConfig_x_DDNS_alarm_11#>";
		else if(ddns_return_code.indexOf('-1')!=-1)
			str = "<#LANHostConfig_x_DDNS_alarm_2#>";
	  	else if(ddns_return_code =='no_change')
    			str = "<#LANHostConfig_x_DDNS_alarm_nochange#>";
	        else if(ddns_return_code == 'Time-out')
        	        str = "<#LANHostConfig_x_DDNS_alarm_1#>";
	        else if(ddns_return_code =='unknown_error')
        	        str = "<#LANHostConfig_x_DDNS_alarm_2#>";
	  	else if(ddns_return_code =='')
    			str = "<#LANHostConfig_x_DDNS_alarm_2#>";
		else if(ddns_return_code =='connect_fail')
                	str = "<#qis_fail_desc7#>";
                else if(ddns_return_code =='auth_fail')
                        str = "<#qis_fail_desc1#>";
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
		else{
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
	else{
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
	showtext($("alert_msg1"), "");
	if(document.getElementById('client_name').value.length == 0){
			alert("<#File_Pop_content_alert_desc1#>");
			document.getElementById('client_name').focus();
			document.getElementById('client_name').select();
			return false;
	}
	else{

		var alert_str = validate_hostname(document.getElementById('client_name'));

		if(alert_str != ""){
			alert(alert_str);
			document.getElementById('client_name').focus();
			document.getElementById('client_name').select();
			return false;
		}
		else
			return true;
	
	}	
}	

var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
function edit_confirm(){
	if(validForm()){
			var originalCustomListArray = new Array();
			var onEditClient = new Array();

			originalCustomListArray = custom_name.split('<');
			onEditClient[0] = document.getElementById('client_name').value.trim();
			onEditClient[1] = document.getElementById('macaddr_field').innerHTML;
			onEditClient[2] = 0;
			onEditClient[3] = document.getElementById('client_image').className.replace("type", "");
			onEditClient[4] = "";
			onEditClient[5] = "";

			for(var i=0; i<originalCustomListArray.length; i++){
				if(originalCustomListArray[i].split('>')[1] == onEditClient[1]){
						originalCustomListArray.splice(i, 1);
				}
			}

			originalCustomListArray.push(onEditClient.join('>'));
			custom_name = originalCustomListArray.join('<');	
			document.list_form.custom_clientlist.value = custom_name;
			document.list_form.submit();

			$("loadingIcon").style.display = "";
			document.getElementById("statusframe").contentWindow.refreshpage();
	}		
}

function edit_cancel(){
	$('edit_client_block').style.display = "none";
	$("hiddenMask").style.visibility = "hidden";
	$("dr_sweet_advise").style.display = "";
	show_custom_image("cancel");
}

function edit_delete(){
	var target_mac = $('macaddr_field').innerHTML;
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
	$j('#custom_image').fadeOut(100);
	$('edit_client_block').style.height = "250px";
	document.getElementById('client_image').className = type;

	if(type == "type0" || type == "type6")
		document.getElementById('client_image').style.backgroundSize = "74px";
	else
		document.getElementById('client_image').style.backgroundSize = "130px";		
}

function hideEditBlock(){
	document.getElementById('edit_client_block').style.display = "none";
	document.getElementById('loadingIcon').style.display = 'none';
	document.getElementById('deleteBtn').style.display ='none';
}

function popupEditBlock(clientObj){
	document.getElementById('client_name').value = clientObj.name;
	document.getElementById('ipaddr_field').innerHTML = clientObj.ip;
	document.getElementById('macaddr_field').innerHTML = clientObj.mac;
	select_image("type" + parseInt(clientObj.type));

	cal_panel_block();
	$j("#edit_client_block").fadeIn(300);
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

	if(winWidth >1020){	
		winPadding = (winWidth-1020)/2;	
		winWidth = 1040;
		blockmarginLeft= (winWidth*0.23)+winPadding;
	}
	else if(winWidth <=1020){
		blockmarginLeft= (winWidth)*0.23+document.body.scrollLeft;	
	}

	$("edit_client_block").style.marginLeft = blockmarginLeft + "px";
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
				<br>
		    </div>
			<div id="wireless_client_detect" style="margin-left:10px;position:absolute;display:none">
				<img src="images/loading.gif">
				<div style="margin:-45px 0 0 75px;"><#QKSet_Internet_Setup_fail_method1#></div>
			</div> 
			<div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px; "></div>
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
<form method="post" name="list_form" id="list_form" action="/start_apply.htm" target="hidden_frame">
	<input type="hidden" name="current_page" value="index.asp">
	<input type="hidden" name="next_page" value="index.asp">
	<input type="hidden" name="modified" value="0">
	<input type="hidden" name="flag" value="background">
	<input type="hidden" name="action_mode" value="apply">
	<input type="hidden" name="action_script" value="saveNvram">
	<input type="hidden" name="action_wait" value="1">
	<input type="hidden" name="custom_clientlist" value="">
</form>

<div id="edit_client_block" class="contentM_qis" style="box-shadow: 3px 3px 10px #000;display:none;">
	<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0" style="padding:5px 10px 0px 10px;">
		<tr>
			<td colspan="2">
				<div style="margin:5px 0px -5px 5px;font-family:Arial, Helvetica, sans-serif;font-size:16px;font-weight:bolder">Client profile</div>
			</td>
		</tr>
		<tr>
			<td colspan="2">
				<img style="width:90%;height:2px" src="/images/New_ui/networkmap/linetwo2.png">
			</td>
		</tr>
		<tr>
			<td>
				<div style="background-color:#172327;border-radius:10px;width:105px;height:87px;padding-top:7px;">
					<div id="client_image" onclick="show_custom_image();"></div>
				</div>
			</td>
			<td>
				<div style="width:320px">
					<table width="99%;"align="center" cellpadding="4" cellspacing="0">
						<tr>
							<td>
								<input id="client_name" name="client_name" type="text" value="" class="input_32_table" style="float:left;width:200px;"></input>								
							</td>
						</tr>
						<tr>
							<td>
								<div id="macaddr_field" style="font-family:Lucida Console;background:#475a5f;width:200px;height:18px;padding:5px 0px 0px 5px;margin-left:2px;border-radius:5px;"></div>
							</td>
						</tr>
						<tr>
							<td>
								<div id="ipaddr_field" style="font-family:Lucida Console;background:#475a5f;width:200px;height:18px;padding:5px 0px 0px 5px;margin-left:2px;border-radius:5px;"></div>
							</td>
						</tr>
					</table>
				</div>
			</td>
		</tr>

		<tr>
			<td colspan="2">
				<div id="custom_image" style="width:390px;display:none;">
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
							</td>
							<td>
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
		<input class="button_gen" type="button" onclick="edit_cancel();" id="cancelBtn" value="<#CTL_Cancel#>">
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
					<td id="single_wan_status" colspan="2" valign="middle" bgcolor="#444f53" class="NM_radius_right" onclick="" style="padding:5px;cursor:auto;">
						<div>
							<span id="NM_connect_title" style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#statusTitle_Internet#>:</span>
							<br>
							<strong id="NM_connect_status" class="index_status" style="font-size:14px;"><#QKSet_Internet_Setup_fail_method1#>...</strong>
						</div>
						<div id="wanIP_div" style="margin-top:5px;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">WAN IP:</span>
							<strong id="index_status" class="index_status" style="font-size:14px;"></strong>
						</div>
						<div id="ddnsHostName_div" style="margin-top:5px;word-break:break-all;word-wrap:break-word;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">DDNS:</span>
							<strong id="ddnsHostName" class="index_status" style="font-size:14px;"><#QIS_detectWAN_desc2#></strong>
							<span id="ddns_fail_hint" class="notificationoff" style="position: absolute;margin-top:-5px;" onClick="show_ddns_fail_hint();" onMouseOut="nd();"></span>
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
							<iframe id="statusframe" class="NM_radius_bottom" style="margin-left:45px;margin-top:-2px;" name="statusframe" width="320" height="680" frameborder="0" allowtransparency="true" style="background-color:transparent; margin-left:10px;" src="device-map/router_status.asp"></iframe>
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
					<td id="single_wan_line" colspan="5" height="19px">
						<div id="single_wan" class="single_wan_connected"></div>
					</td>
				</tr>			
				<tr>
					<td height="115" align="right" bgcolor="#444f53" class="NM_radius_left" onclick="showstausframe('Router');">
						<a href="device-map/router.asp" target="statusframe"><div id="iconRouter" onclick="clickEvent(this);"></div></a>
					</td>
					<td colspan="2" valign="middle" bgcolor="#444f53" class="NM_radius_right" onclick="showstausframe('Router');">
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
					<td id="clients_tr" width="150" height="170" bgcolor="#444f53" align="center" valign="top" class="NM_radius_top" onclick="showstausframe('Client');">
						<a id="clientStatusLink" href="device-map/clients.asp" target="statusframe"><!--lock 1226-->
							<div id="iconClient" style="margin-top:20px;" onclick="clickEvent(this);"></div>
						</a>
						<div class="clients" id="clientNumber" style="cursor:pointer;"></div>
					</td>

					<td width="36" rowspan="6" id="clientspace_td"></td>

					<td id="usb1_html" width="160" bgcolor="#444f53" align="center" valign="top" class="NM_radius_top">
						<div id="usbPathContianer_1" style="display:none">
							<div style="margin-top:20px;margin-bottom:10px;" id="deviceIcon_1"></div>
							<div>
								<span id="deviceText_1"></span>
								<select id="deviceOption_1" class="input_option" style="display:none;height:20px;width:130px;font-size:12px;"></select>	
							</div>
							<div id="deviceDec_1"></div>
						</div>
					</td>

				</tr>				
				
				<tr id="usb2_html">
					<td bgcolor="#444f53" align="center" valign="top" class="NM_radius_bottom">
						<!--a id="" href="device-map/clients.asp" target="statusframe">
							<div id="iconClient" style="margin-top:20px;" onclick=""></div>
						</a>	
						<div class="clients" id="" style="cursor:pointer;">Wireless Clients:</div-->
					</td>
					<td height="150" bgcolor="#444f53" align="center" valign="top" class="NM_radius_bottom">
						<div id="usbPathContianer_2" style="display:none">
							<img style="margin-top:15px;width:150px;height:2px" src="/images/New_ui/networkmap/linetwo2.png">
							<div style="margin-top:10px;margin-bottom:10px;" id="deviceIcon_2"></div>
							<div>
								<span id="deviceText_2"></span>
								<select id="deviceOption_2" class="input_option" style="display:none;height:20px;width:130px;font-size:12px;"></select>	
							</div>
							<div id="deviceDec_2"></div>
						</div>
					</td>
				</tr>

				<tr id="bottomspace_tr" style="display:none">
					<td colspan="3" height="200px"></td>
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
		show_USBDevice(usbDevices[this.value]);
		setSelectedDiskOrder('iconUSBdisk_1');

		if(usbDevices[this.value].deviceType == "modem")
			clickEvent(document.getElementById('iconModem_1'));
		else if(usbDevices[this.value].deviceType == "printer")
			clickEvent(document.getElementById('iconPrinter_1'));
		else
			clickEvent(document.getElementById('iconUSBdisk_1'));
	}

	document.getElementById('deviceOption_2').onchange = function(){
		show_USBDevice(usbDevices[this.value]);
		setSelectedDiskOrder('iconUSBdisk_2');

		if(usbDevices[this.value].deviceType == "modem")
			clickEvent(document.getElementById('iconModem_2'));
		else if(usbDevices[this.value].deviceType == "printer")
			clickEvent(document.getElementById('iconPrinter_2'));
		else
			clickEvent(document.getElementById('iconUSBdisk_2'));
	}

	setTimeout("document.networkmapdRefresh.submit();", 2000);
</script>
</body>

<form method="post" name="networkmapdRefresh" action="/apply.cgi" target="hidden_frame">
	<input type="hidden" name="action_mode" value="refresh_networkmap">
	<input type="hidden" name="action_script" value="">
	<input type="hidden" name="action_wait" value="1">
	<input type="hidden" name="current_page" value="httpd_check.htm">
	<input type="hidden" name="next_page" value="httpd_check.htm">
</form>

</html>
