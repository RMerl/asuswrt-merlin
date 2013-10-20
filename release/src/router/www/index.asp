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
<link href="/images/map-iconRouter_iphone.png" rel="apple-touch-icon" />
<title><#Web_Title#> - <#menu1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="NM_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>
if(location.pathname == "/"){
	if('<% nvram_get("x_Setting"); %>' == '0')
		location.href = '/QIS_wizard.htm?flag=welcome';
	else if('<% nvram_get("w_Setting"); %>' == '0' && sw_mode != 2)
		location.href = '/QIS_wizard.htm?flag=wireless';
}
// for client_function.js
<% login_state_hook(); %>

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var Dev3G = '<% nvram_get("d3g"); %>';
var flag = '<% get_parameter("flag"); %>';

var webs_state_update= '<% nvram_get("webs_state_update"); %>';
var webs_state_error= '<% nvram_get("webs_state_error"); %>';
var webs_state_info= '<% nvram_get("webs_state_info"); %>';
var usb_path1_index = '<% nvram_get("usb_path1"); %>';
var usb_path2_index = '<% nvram_get("usb_path2"); %>';

<% wanlink(); %>
<% get_printer_info(); %>

var all_disks = "";
var all_disk_interface;
if(usb_support){
	all_disks = foreign_disks().concat(blank_disks());
	all_disk_interface = foreign_disk_interface_names().concat(blank_disk_interface_names());
}

var leases = [<% dhcp_leases(); %>];	// [[hostname, MAC, ip, lefttime], ...]
var arps = [<% get_arp_table(); %>];		// [[ip, x, x, MAC, x, type], ...]
var arls = [<% get_arl_table(); %>];		// [[MAC, port, x, x], ...]
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var ipmonitor = [<% get_static_client(); %>];	// [[IP, MAC, DeviceName, Type, http, printer, iTune], ...]
var networkmap_fullscan = '<% nvram_match("networkmap_fullscan", "0", "done"); %>'; //2008.07.24 Add.  1 stands for complete, 0 stands for scanning.;
var client_list_array = '<% get_client_detail_info(); %>';
var $j = jQuery.noConflict();	

var _apps_pool_error = '<% apps_fsck_ret(); %>';
if(_apps_pool_error != '')
	var apps_pool_error = eval(_apps_pool_error);
else
	var apps_pool_error = [[""]];

var usb_path1_pool_error = '<% nvram_get("usb_path1_pool_error"); %>';
var usb_path2_pool_error = '<% nvram_get("usb_path2_pool_error"); %>';

if(all_disks != "")
        var pool_name = pool_devices();

var wan0_primary = '<% nvram_get("wan0_primary"); %>';
var wan1_primary = '<% nvram_get("wan1_primary"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_mode = '<%nvram_get("wans_mode");%>';	
if(wans_dualwan_orig.search(" ") == -1)
	var wans_flag = 0;
else
	var wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;

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
	show_client_status();		

	if(!parent.usb_support){
		$("line3_td").height = '21px';
		$("line3_img").src = '/images/New_ui/networkmap/line_one.png';
		$("clients_tr").colSpan = "3";
		$("clients_tr").className = 'NM_radius';
		$("clients_tr").width = '350';
		$("clientspace_td").style.display = "none";
		$("usb1_tr").style.display = "none";
		$("usb2_tr").style.display = "none";
		$("bottomspace_tr").style.display = "";
	}

	if(rc_support.search("usbX") == -1 || rc_support.search("usbX1") > -1){
		$("deviceIcon_1").style.display = "none";
		$("deviceDec_1").style.display = "none";
	}

	show_device();
	showMapWANStatus(sw_mode);

	if(sw_mode != "1"){
		$("wanIP_div").style.display = "none";
		$("ddnsHostName_div").style.display = "none";
		$("NM_connect_title").style.fontSize = "14px";
		$("NM_connect_status").style.fontSize = "20px";
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
	var macAddr = '<% nvram_get("et0macaddr"); %>'.toUpperCase().replace(/:/g, "");
	return "A"+hexMD5(macAddr).toUpperCase()+".asuscomm.com";
}

function detectUSBStatusIndex(){
	$j.ajax({
    	url: '/update_diskinfo.asp',
    	dataType: 'script',
    	error: function(xhr){
    		detectUSBStatusIndex();
    	},
    	success: function(){
    		if((tmp_mount_0 == 0 && tmp_mount_0 != foreign_disk_total_mounted_number()[0]) 
						|| (tmp_mount_1 == 0 && tmp_mount_1 != foreign_disk_total_mounted_number()[1])){
						location.href = "/index.asp";
				return 0;
				}		
  		}
	});
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

function show_client_status(){
	var client_list_row = client_list_array.split('<');
	var client_number = client_list_row.length - 1;
	var client_str = "";
	var wired_num = 0, wireless_num = 0;	

	client_str += "<#Full_Clients#>: <span id='_clientNumber'>"+client_number+"</span>";
	$("clientNumber").innerHTML = client_str;
}

function show_device(){
	if(!usb_support){
		usb_path1_index = "";
		usb_path2_index = "";
		return true;
	}
	else
		all_disks = foreign_disks().concat(blank_disks());

	switch(usb_path1_index){
		case "storage":
			for(var i = 0; i < all_disks.length; ++i){
				if(foreign_disk_interface_names()[i] == "1"){
					disk_html(0, i);
					check_status(apps_pool_error, i);
					check_status2(usb_path1_pool_error, i);
					break;
				}
			}
			if(all_disk_interface.getIndexByValue("1") == -1)
				no_device_html(0);
			break;
		case "printer":
			printer_html(0, 0);
			break;
		case "audio":
		case "webcam":
		case "modem":
			modem_html(0, 0);
			break;
		default:
			no_device_html(0);
	}
	
	// show the upper usb device
	switch(usb_path2_index){
		case "storage":
			for(var i = 0; i < all_disks.length; ++i){
				if(foreign_disk_interface_names()[i] == "2"){
					disk_html(1, i);
					check_status(apps_pool_error, i);
					check_status2(usb_path2_pool_error, i);
					break;
				}
			}
			if(all_disk_interface.getIndexByValue("2") == -1)
				no_device_html(1);
			break;
		case "printer":
			printer_html(1, 1);
			break;
		case "audio":
		case "webcam":
		case "modem":
			modem_html(1, 1);
			break;
		default:
			no_device_html(1);
	}
}

function disk_html(device_order, all_disk_order){
	var device_icon = $("deviceIcon_"+device_order);
	var device_dec = $("deviceDec_"+device_order);
	var icon_html_code = '';
	var dec_html_code = '';
	
	var disk_model_name = "";
	var TotalSize;
	var mount_num = getDiskMountedNum(all_disk_order);
	var all_accessable_size;
	var percentbar = 0;

	if(all_disk_order < foreign_disks().length)
		disk_model_name = decodeURIComponent(foreign_disk_model_info()[all_disk_order]);
	else
		disk_model_name = blank_disks()[all_disk_order-foreign_disks().length];
	
	icon_html_code += '<a target="statusframe">\n';

	if(device_order == 0){
		icon_html_code += '<div id="iconUSBdisk_'+all_disk_order+'" style="margin-top:20px;" class="iconUSBdisk" onclick="setSelectedDiskOrder(this.id);clickEvent(this);"></div>\n';
		icon_html_code += '<div id="ring_USBdisk_'+all_disk_order+'" class="iconUSBdisk" style="display:none;z-index:1;"></div>\n';
	}
	else{
		icon_html_code += '<div id="iconUSBdisk_'+all_disk_order+'" class="iconUSBdisk" onclick="setSelectedDiskOrder(this.id);clickEvent(this);"></div>\n';
		icon_html_code += '<div id="ring_USBdisk_'+all_disk_order+'" class="iconUSBdisk" style="display:none;z-index:1;"></div>\n';
	}
	
	icon_html_code += '</a>\n';
	
	dec_html_code += '<div class="formfonttitle_nwm" style="text-shadow: 1px 1px 0px black;text-align:center;margin-top:10px;">'+disk_model_name+'</div>\n';

	if(mount_num > 0){
		if(all_disk_order < foreign_disks().length)
			TotalSize = simpleNum(foreign_disk_total_size()[all_disk_order]);
		else
			TotalSize = simpleNum(blank_disk_total_size()[all_disk_order-foreign_disks().length]);
		
		all_accessable_size = simpleNum2(computeallpools(all_disk_order, "size")-computeallpools(all_disk_order, "size_in_use"));
		percentbar = simpleNum2((all_accessable_size)/TotalSize*100);
		percentbar = Math.round(100-percentbar);		
		dec_html_code += '<p id="diskDesc'+ foreign_disk_interface_names()[all_disk_order] +'" style="margin-top:5px;"><#Availablespace#>:</p><div id="diskquota" align="left" style="margin-top:5px;margin-bottom:10px;">\n';
		dec_html_code += '<img src="images/quotabar.gif" width="'+percentbar+'" height="13">';
		dec_html_code += '</div>\n';
	}
	else{
		all_disk_order++;
		dec_html_code += '<span class="style1"><strong id="diskUnmount'+ all_disk_order +'"><#DISK_UNMOUNTED#></strong></span>\n';
	}

	device_icon.innerHTML = icon_html_code;
	device_dec.innerHTML = dec_html_code;
}

function printer_html(device_seat, printer_order){
	var printer_name = printer_manufacturers()[printer_order]+" "+printer_models()[printer_order];
	var printer_status = "";
	var device_icon = $("deviceIcon_"+device_seat);
	var device_dec = $("deviceDec_"+device_seat);
	var icon_html_code = '';
	var dec_html_code = '';
	
	if(printer_pool()[printer_order] != "")
		printer_status = '<#CTL_Enabled#>';
	else
		printer_status = '<#CTL_Disabled#>';
	
	icon_html_code += '<a href="device-map/printer.asp" target="statusframe">\n';
	icon_html_code += '<div id="iconPrinter'+printer_order+'" class="iconPrinter" onclick="clickEvent(this);"></div>\n';
	icon_html_code += '</a>\n';
	
	dec_html_code += '<div class="formfonttitle_nwm" style="text-shadow: 1px 1px 0px black;text-align:center;margin-top:10px;"><span id="printerName'+device_seat+'">'+ printer_name +'</span></div>\n';
	//dec_html_code += '<span class="style5">'+printer_status+'</span>\n';
	
	device_icon.innerHTML = icon_html_code;
	device_dec.innerHTML = dec_html_code;
}

function modem_html(device_seat, modem_order){
	var modem_name = Dev3G;
	var modem_status = "Connected";
	var device_icon = $("deviceIcon_"+device_seat);
	var device_dec = $("deviceDec_"+device_seat);
	var icon_html_code = '';
	var dec_html_code = '';
	
	icon_html_code += '<a href="device-map/modem.asp" target="statusframe">\n';
	icon_html_code += '    <div id="iconModem'+modem_order+'" class="iconmodem" onclick="clickEvent(this);"></div>\n';
	icon_html_code += '</a>\n';
	
	dec_html_code += modem_name+'<br>\n';
	//dec_html_code += '<span class="style1"><strong>'+modem_status+'</strong></span>\n';
	//dec_html_code += '<br>\n';
	//dec_html_code += '<img src="images/signal_'+3+'.gif" align="middle">';
	
	device_dec.className = "clients";
	device_icon.innerHTML = icon_html_code;
	device_dec.innerHTML = dec_html_code;
}

function no_device_html(device_seat){
	var device_icon = $("deviceIcon_"+device_seat);
	var device_dec = $("deviceDec_"+device_seat);
	var icon_html_code = '';
	var dec_html_code = '';
	
	icon_html_code += '<div class="iconNo"></div>';
	dec_html_code += '<br/><span id="noUSB'+ device_seat +'">';
	if(rc_support.search("usbX") > -1)
		dec_html_code += '<#NoDevice#>';
	else	dec_html_code += '<#CTL_nonsupported#>';
	dec_html_code += '</span>\n';
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
	}
	else if(obj.id.indexOf("Modem") > 0){
		seat = obj.id.indexOf("Modem")+5;
		clicked_device_order = parseInt(obj.id.substring(seat, seat+1));
		icon = "iconmodem";
		stitle = "<#menu5_4_4#>";
		$("statusframe").src = "/device-map/modem.asp";
	}
	else if(obj.id.indexOf("Printer") > 0){
		seat = obj.id.indexOf("Printer")+7;
		clicked_device_order = parseInt(obj.id.substring(seat, seat+1));
		icon = "iconPrinter";
		stitle = "<#statusTitle_Printer#>";
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
			for(i=0;i < apps_pool_error.length ; i++){
				if(pool_name[getSelectedDiskOrder()] == apps_pool_error[i][0]){
					if(apps_pool_error[i][1] == 1){
						$("statusframe").src = "/device-map/disk_utility.asp";
						obj.style.backgroundPosition = '0% -202px';					
					}	
					else{
						$("statusframe").src = "/device-map/disk.asp";	
						obj.style.backgroundPosition = '0% -103px';
					}	
				}		
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
	
		//showtext($("internetStatus"), "<#OP_AP_item#>");
		
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

function check_status(flag, diskOrder){
	document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/USB_2.png)";	
	document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
	document.getElementById('iconUSBdisk_'+diskOrder).style.position = "absolute";
	document.getElementById('iconUSBdisk_'+diskOrder).style.marginTop = "0px";
	if(navigator.appName.indexOf("Microsoft") >= 0)
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "0px";
	else
		document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "33px";

	document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";	
	document.getElementById('ring_USBdisk_'+diskOrder).style.display = "";

	if(!diskUtility_support)
		return true;

	var i, j;
	var got_code_0, got_code_1, got_code_2, got_code_3;

	got_code_0 = got_code_1 = got_code_2 = got_code_3 = 0;

	for(i = 0; i < pool_name.length; ++i){
		for(j = 0; j < flag.length; ++j){
			if(pool_name[i] == flag[j][0] && per_pane_pool_usage_kilobytes(i, diskOrder)[0] > 0){
				if(flag[j][1] == ""){
					got_code_3 = 1;
					continue;
				}

				switch(parseInt(flag[j][1])){
					case 3: // don't or can't support.
						got_code_3 = 1;
						break;
					case 2: // proceeding...
						got_code_2 = 1;
						break;
					case 1: // find errors.
						got_code_1 = 1;
						break;
					default: // no error.
						got_code_0 = 1;
						break;
				}
			}
		}
	}

	if(got_code_1){
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 101%';
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
	}
	else if(got_code_2 || got_code_3){
		// do nothing.
	}
	else{ // got_code_0
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 50%';
	}
}

function check_status2(flag, diskOrder){
	if(!diskUtility_support)
		return true;

	if(flag == 1){
		document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 101%';
		document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -3px';		
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
                str = "<#LANHostConfig_x_DDNS_alarm_hostname#> '<%nvram_get("ddns_hostname_x");%>' <#LANHostConfig_x_DDNS_alarm_registered_2#> '<%nvram_get("ddns_old_name");%>'.";
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

	if(wans_mode == "fo"){
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
					</td>
					<td width="40px" rowspan="11" valign="top">
						<div class="statusTitle" id="statusTitle_NM">
							<div id="helpname" style="padding-top:10px;font-size:16px;"></div>
						</div>							
						<div>
							<iframe id="statusframe" class="NM_radius_bottom" style="margin-left:45px;margin-top:-2px;" name="statusframe" width="320" height="680" frameborder="0" allowtransparency="true" style="background-color:transparent; margin-left:10px;" src="device-map/router.asp"></iframe>
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
						<span style="font-size:14px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#Security_Level#>: </span><br/><br/><strong id="wl_securitylevel_span" class="index_status"></strong>
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
					<td id="usb1_tr" width="160" bgcolor="#444f53" align="center" valign="top" class="NM_radius_top">
						<div style="margin-top:20px;" id="deviceIcon_0"></div><div id="deviceDec_0"></div>
					</td>
				</tr>				
				<tr id="usb2_tr">
					<td bgcolor="#444f53" align="center" valign="top" class="NM_radius_bottom"></td>
					<td height="150" bgcolor="#444f53" align="center" valign="top" class="NM_radius_bottom">
						<div style="margin-top:10px;" id="deviceIcon_1"></div>
						<div id="deviceDec_1"></div>
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
</script>
</body>
</html>
