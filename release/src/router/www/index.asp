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

var all_disks;
var all_disk_interface;
if(usb_support != -1){
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

function initial(){
	show_menu();

	var isIE6 = navigator.userAgent.search("MSIE 6") > -1;
	if(isIE6)
		alert("We've detected that you're using IE6. Please Try Google Chrome, IE7 or higher for best browsing experience.");

	if(psta_support != -1 && sw_mode == 2)
		show_middle_status('<% nvram_get("wlc_auth_mode"); %>', "", 0);		
	else
		show_middle_status(document.form.wl_auth_mode_x.value, document.form.wl_wpa_mode.value, parseInt(document.form.wl_wep_x.value));

	set_default_choice();
	show_client_status();		

	if(parent.usb_support == -1){
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
		var ddnsName = decodeURIComponent('<% nvram_char_to_ascii("", "ddns_hostname_x"); %>');
		$("index_status").innerHTML = '<span style="word-break:break-all;">' + wanlink_ipaddr() + '</span>'
		if('<% nvram_get("ddns_enable_x"); %>' == '0')
			$("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=ddns_enable_x"><#btn_go#></a>';
		else if(ddnsName == '')
			$("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=DDNSName">Sign up</a>';
		else if(ddnsName == isMD5DDNSName())
			$("ddnsHostName").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/Advanced_ASUSDDNS_Content.asp?af=DDNSName">Sign up</a>';
		else{
			$("ddnsHostName").innerHTML = '<span style="word-break:break-all;">'+ ddnsName +'</span>';
		}

		if(wanlink_ipaddr() == '0.0.0.0' || wanlink_ipaddr() == '')
			$("wanIP_div").style.display = "none";
	}

	var NM_table_img = getCookie("NM_table_img");
	if(NM_table_img != "" && NM_table_img != null){
		customize_NM_table(NM_table_img);
		$("bgimg").options[NM_table_img[4]].selected = 1;
	}
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
			return 0;
			clickEvent($("iconRouter"));
			$("statusframe").src = "/device-map/router.asp";
			show_device();
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

function getCookie(c_name)
{
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

function show_middle_status(auth_mode, wpa_mode, wl_wep_x){
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
	if(usb_support == -1){
		usb_path1_index = "";
		usb_path2_index = "";
	}
	else
		all_disks = foreign_disks().concat(blank_disks());

	switch(usb_path1_index){
		case "storage":
			for(var i = 0; i < all_disks.length; ++i){
				if(foreign_disk_interface_names()[i] == "1"){
					disk_html(0, i);	
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
	
	icon_html_code += '<a href="device-map/disk.asp" target="statusframe">\n';

	if(device_order == 0)
		icon_html_code += '<div id="iconUSBdisk_'+all_disk_order+'" style="margin-top:20px;" class="iconUSBdisk" onclick="setSelectedDiskOrder(this.id);clickEvent(this);"></div>\n';		
	else
		icon_html_code += '<div id="iconUSBdisk_'+all_disk_order+'" class="iconUSBdisk" onclick="setSelectedDiskOrder(this.id);clickEvent(this);"></div>\n';

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
	icon_html_code += '    <div id="iconPrinter'+printer_order+'" class="iconPrinter" onclick="clickEvent(this);"></div>\n';
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
		icon = "iconInternet";
		stitle = "<#statusTitle_Internet#>";
		$("statusframe").src = "/device-map/internet.asp";
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
		$("statusframe").src = "/device-map/disk.asp";
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
		lastClicked.style.backgroundPosition = '0% 0%';
	}

	obj.style.backgroundPosition = '0% 101%';

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
	else 
		page
	window.open("/device-map/"+page.toLowerCase()+".asp","statusframe");
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
<input type="hidden" name="wl_wpa_mode" value="<% nvram_get("wl0_wpa_mode"); %>">
<input type="hidden" name="wl_wep_x" value="<% nvram_get("wl0_wep_x"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="apps_action" value="">
<input type="hidden" name="apps_path" value="">
<input type="hidden" name="apps_name" value="">
<input type="hidden" name="apps_flag" value="">
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
		<div id="NM_shift" style="margin-top:-160px;"></div>
		<div id="NM_table" class="NM_table" >
		<div id="NM_table_div">
			<table id="_NM_table" border="0" cellpadding="0" cellspacing="0" height="720" style="opacity:.95;" >
				<tr>
					<td width="40" rowspan="11" valign="center"></td>
					<td height="115" align="right" class="NM_radius_left" valign="middle" bgcolor="#444f53" onclick="showstausframe('Internet');">
						<a href="/device-map/internet.asp" target="statusframe"><div id="iconInternet" onclick="clickEvent(this);"></div></a>
					</td>
					<td colspan="2" valign="middle" bgcolor="#444f53" class="NM_radius_right" onclick="" style="padding:5px;cursor:auto;">
						<div>
							<span id="NM_connect_title" style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;"><#statusTitle_Internet#>:</span>
							<strong id="NM_connect_status" class="index_status" style="font-size:14px;"><#QKSet_Internet_Setup_fail_method1#>...</strong>
						</div>
						<div id="wanIP_div" style="margin-top:5px;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">WAN IP:</span>
							<strong id="index_status" class="index_status" style="font-size:14px;"></strong>
						</div>
						<div id="ddnsHostName_div" style="margin-top:5px;">
							<span style="font-size:12px;font-family: Verdana, Arial, Helvetica, sans-serif;">DDNS:</span>
							<strong id="ddnsHostName" class="index_status" style="font-size:14px;"></strong>
						</div>
					</td>
					<td width="40" rowspan="11" valign="top">
						<div class="statusTitle" id="statusTitle_NM">
							<div id="helpname" style="padding-top:10px;font-size:16px;"></div>
						</div>							
						<div>
							<iframe id="statusframe" class="NM_radius_bottom" style="margin-left:45px;margin-top:-2px;" name="statusframe" width="320" height="680" frameborder="0" allowtransparency="true" style="background-color:transparent; margin-left:10px;" src="device-map/router.asp"></iframe>
						</div>
					</td>	
				</tr>			
				<tr>
					<td colspan="5" align="center" height="19px">
						<img style="margin-left:-365px;*margin-left:-185px;" src="/images/New_ui/networkmap/line_one.png">
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
