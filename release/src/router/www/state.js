//For operation mode;
var sw_mode = '<% nvram_get("sw_mode"); %>';
if(sw_mode == 3 && '<% nvram_get("wlc_psta"); %>' == 1)
	sw_mode = 2;
var productid = '<#Web_Title2#>';
var uptimeStr = "<% uptime(); %>";
var timezone = uptimeStr.substring(26,31);
var boottime = parseInt(uptimeStr.substring(32,42));
var newformat_systime = uptimeStr.substring(8,11) + " " + uptimeStr.substring(5,7) + " " + uptimeStr.substring(17,25) + " " + uptimeStr.substring(12,16);  //Ex format: Jun 23 10:33:31 2008
var systime_millsec = Date.parse(newformat_systime); // millsec from system
var JS_timeObj = new Date(); // 1970.1.1
var wan_route_x = "";
var wan_nat_x = "";
var wan_proto = "";
var test_page = 0;
var testEventID = "";
var httpd_dir = "/cifs1"
var hw_ver = '<% nvram_get("hardware_version"); %>';

// for detect if the status of the machine is changed. {
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var stopFlag = 0;

var gn_array_2g = <% wl_get_guestnetwork("0"); %>;
var gn_array_5g = <% wl_get_guestnetwork("1"); %>;

<% available_disk_names_and_sizes(); %>
<% disk_pool_mapping_info(); %>

function change_wl_unit_status(_unit){
	if(sw_mode == 2) return false;
	
	document.titleForm.wl_unit.disabled = false;
	document.titleForm.wl_unit.value = _unit;
	if(document.titleForm.current_page.value == "")
		document.titleForm.current_page.value = "Advanced_Wireless_Content.asp";
	if(document.titleForm.next_page.value == "")
		document.titleForm.next_page.value = "Advanced_Wireless_Content.asp";
	document.titleForm.action_mode.value = "change_wl_unit";
	document.titleForm.action = "apply.cgi";
	document.titleForm.target = "";
	document.titleForm.submit();
}

var banner_code, menu_code="", menu1_code="", menu2_code="", tab_code="", footer_code;
function show_banner(L3){// L3 = The third Level of Menu
	var banner_code = "";	

	banner_code +='<form method="post" name="titleForm" id="titleForm" action="/start_apply.htm" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="next_page" value="">\n';
	banner_code +='<input type="hidden" name="current_page" value="">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="">\n';
	banner_code +='<input type="hidden" name="action_wait" value="5">\n';
	banner_code +='<input type="hidden" name="wl_unit" value="" disabled>\n';
	banner_code +='<input type="hidden" name="wl_subunit" value="-1" disabled>\n';
	banner_code +='<input type="hidden" name="preferred_lang" value="<% nvram_get("preferred_lang"); %>">\n';
	banner_code +='<input type="hidden" name="flag" value="">\n';
	banner_code +='</form>\n';

	banner_code +='<form method="post" name="diskForm_title" action="/device-map/safely_remove_disk.asp" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="disk" value="">\n';
	banner_code +='</form>\n';

	banner_code +='<form method="post" name="internetForm_title" action="/start_apply2.htm" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="current_page" value="/index.asp">\n';
	banner_code +='<input type="hidden" name="next_page" value="/index.asp">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="restart_wan_if">\n';
	banner_code +='<input type="hidden" name="action_wait" value="5">\n';
	banner_code +='<input type="hidden" name="wan_enable" value="<% nvram_get("wan_enable"); %>">\n';
	banner_code +='<input type="hidden" name="wan_unit" value="<% get_wan_unit(); %>">\n';
	banner_code +='</form>\n';

	banner_code +='<div class="banner1" align="center"><img src="images/New_ui/asustitle.png" width="218" height="54" align="left">\n';
	banner_code +='<div style="margin-top:13px;margin-left:-90px;*margin-top:0px;*margin-left:0px;" align="center"><span id="modelName_top" onclick="this.focus();" class="modelName_top"><#Web_Title2#></span></div>';

	// logout, reboot
	banner_code +='<a href="javascript:logout();"><div style="margin-top:13px;margin-left:25px; *width:136px;" class="titlebtn" align="center"><span><#t1Logout#></span></div></a>\n';
	banner_code +='<a href="javascript:reboot();"><div style="margin-top:13px;margin-left:0px;*width:136px;" class="titlebtn" align="center"><span><#BTN_REBOOT#></span></div></a>\n';

	// language
	banner_code +='<ul class="navigation"><a href="#">';
	banner_code +='<% shown_language_css(); %>';
	banner_code +='</a></ul>';

	banner_code +='</div>\n';
	banner_code +='<table width="998" border="0" align="center" cellpadding="0" cellspacing="0" class="statusBar">\n';
	banner_code +='<tr>\n';
	banner_code +='<td background="images/New_ui/midup_bg.png" height="179" valign="top"><table width="764" border="0" cellpadding="0" cellspacing="0" height="30px" style="margin-left:230px;">\n';
	banner_code +='<tbody><tr>\n';
 	banner_code +='<td valign="center" class="titledown" width="auto">';

	// dsl does not support operation mode
	if (dsl_support	== -1) {
		banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;"><#menu5_6_1_title#>:</sapn><a href="/Advanced_OperationMode_Content.asp" style="color:white"><span id="sw_mode_span" class="title_link"></span></a>\n';
	}
	banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;"><#General_x_FirmwareVersion_itemname#></sapn><a href="/Advanced_FirmwareUpgrade_Content.asp" style="color:white;"><span id="firmver" class="title_link"></span></a> <small>(Merlin build)</small>\n';
	banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;" id="ssidTitle">SSID:</sapn>';
	banner_code +='<span onclick="change_wl_unit_status(0)" id="elliptic_ssid_2g" class="title_link"></span>';
	banner_code +='<span onclick="change_wl_unit_status(1)" id="elliptic_ssid_5g" style="margin-left:-5px;" class="title_link"></span>\n';
	banner_code +='</td>\n';

//	if(wifi_hw_sw_support != -1)
		banner_code +='<td width="30"><div id="wifi_hw_sw_status"></div></td>\n';

	if(cooler_support != -1)
		banner_code +='<td width="30"><div id="cooler_status"" style="display:none;"></div></td>\n';

	if(multissid_support != -1)
		banner_code +='<td width="30"><div id="guestnetwork_status""></div></td>\n';

	if(sw_mode != 3)
	  banner_code +='<td width="30"><div id="connect_status""></div></td>\n';

	if(usb_support != -1)
		banner_code +='<td width="30"><div id="usb_status"></div></td>\n';
	
	if(printer_support != -1)
	  banner_code +='<td width="30"><div id="printer_status"></div></td>\n';

	banner_code +='<td width="17"></td>\n';
	banner_code +='</tr></tbody></table></td></tr></table>\n';

	$("TopBanner").innerHTML = banner_code;

	show_loading_obj();	
	show_top_status();
	set_Dr_work();
	updateStatus_AJAX();
}

//Level 3 Tab
var tabtitle = new Array();
tabtitle[0] = new Array("", "<#menu5_1_1#>", "<#menu5_1_2#>", "<#menu5_1_3#>", "<#menu5_1_4#>", "<#menu5_1_5#>", "<#menu5_1_6#>");
tabtitle[1] = new Array("", "<#menu5_2_1#>", "<#menu5_2_2#>", "<#menu5_2_3#>", "IPTV", "Switch Control");
tabtitle[2] = new Array("", "<#menu5_3_1#>", "Dual WAN", "<#menu5_3_3#>", "<#menu5_3_4#>", "<#menu5_3_5#>", "<#menu5_3_6#>", "<#NAT_passthrough_itemname#>", "<#menu5_4_4#>");
tabtitle[3] = new Array("", "<#UPnPMediaServer#>", "<#menu5_4_1#>", "NFS Exports" , "<#menu5_4_2#>", "<#menu5_4_3#>");
tabtitle[4] = new Array("", "IPv6");
tabtitle[5] = new Array("", "<#BOP_isp_heart_item#>", "<#vpn_Adv#>", "OpenVPN Server Settings", "OpenVPN Client Settings", "OpenVPN Keys", "VPN Status");
tabtitle[6] = new Array("", "<#menu5_1_1#>", "<#menu5_5_2#>", "<#menu5_5_5#>", "<#menu5_5_3#>", "<#menu5_5_4#>");
tabtitle[7] = new Array("", "<#menu5_6_1#>", "<#menu5_6_2#>", "<#menu5_6_3#>", "<#menu5_6_4#>", "Performance tuning", "<#menu_dsl_setting#>");
tabtitle[8] = new Array("", "<#menu5_7_2#>", "<#menu5_7_3#>", "<#menu5_7_4#>", "<#menu5_7_5#>", "<#menu5_7_6#>", "<#menu_dsl_log#>","Connections");
tabtitle[9] = new Array("", "QoS", "<#traffic_monitor#>");
tabtitle[10] = new Array("", "Sysinfo", "WakeOnLAN", "Other Settings", "Run Cmd");

var tablink = new Array();
tablink[0] = new Array("", "Advanced_Wireless_Content.asp", "Advanced_WWPS_Content.asp", "Advanced_WMode_Content.asp", "Advanced_ACL_Content.asp", "Advanced_WSecurity_Content.asp", "Advanced_WAdvanced_Content.asp");
tablink[1] = new Array("", "Advanced_LAN_Content.asp", "Advanced_DHCP_Content.asp", "Advanced_GWStaticRoute_Content.asp", "Advanced_IPTV_Content.asp", "Advanced_SwitchCtrl_Content.asp");
tablink[2] = new Array("", "Advanced_WAN_Content.asp", "Advanced_WANPort_Content.asp", "Advanced_PortTrigger_Content.asp", "Advanced_VirtualServer_Content.asp", "Advanced_Exposed_Content.asp", "Advanced_ASUSDDNS_Content.asp", "Advanced_NATPassThrough_Content.asp", "Advanced_Modem_Content.asp");
tablink[3] = new Array("", "mediaserver.asp", "Advanced_AiDisk_samba.asp", "Advanced_AiDisk_NFS.asp", "Advanced_AiDisk_ftp.asp", "Advanced_AiDisk_others.asp");
tablink[4] = new Array("", "Advanced_IPv6_Content.asp");
tablink[5] = new Array("", "Advanced_PPTP_Content.asp", "Advanced_PPTPAdvanced_Content.asp", "Advanced_OpenVPNServer_Content.asp", "Advanced_OpenVPNClient_Content.asp", "Advanced_OpenVPN_Keys.asp", "Advanced_VPNStatus.asp");
tablink[6] = new Array("", "Advanced_BasicFirewall_Content.asp", "Advanced_URLFilter_Content.asp", "Advanced_KeywordFilter_Content.asp","Advanced_MACFilter_Content.asp", "Advanced_Firewall_Content.asp");
tablink[7] = new Array("", "Advanced_OperationMode_Content.asp", "Advanced_System_Content.asp", "Advanced_FirmwareUpgrade_Content.asp", "Advanced_SettingBackup_Content.asp", "Advanced_PerformanceTuning_Content.asp", "Advanced_ADSL_Content.asp");
tablink[8] = new Array("", "Main_LogStatus_Content.asp", "Main_DHCPStatus_Content.asp", "Main_WStatus_Content.asp", "Main_IPTStatus_Content.asp", "Main_RouteStatus_Content.asp", "Main_AdslStatus_Content.asp", "Main_ConnStatus_Content.asp");
tablink[9] = new Array("", "QoS_EZQoS.asp", "Main_TrafficMonitor_realtime.asp", "Main_TrafficMonitor_last24.asp", "Main_TrafficMonitor_daily.asp", "Main_TrafficMonitor_monthly.asp", "Main_TrafficMonitor_devrealtime.asp", "Main_TrafficMonitor_devdaily.asp", "Main_TrafficMonitor_devmonthly.asp", "Advanced_QOSUserPrio_Content.asp", "Advanced_QOSUserRules_Content.asp");
tablink[10] = new Array("", "Tools_Sysinfo.asp", "Tools_WOL.asp", "Tools_OtherSettings.asp", "Tools_RunCmd.asp");

//Level 2 Menu
menuL2_title = new Array("", "<#menu5_1#>", "<#menu5_2#>", "<#menu5_3#>", "<#menu5_4#>", "IPv6", "<#BOP_isp_heart_item#>", "<#menu5_5#>", "<#menu5_6#>", "<#System_Log#>");
menuL2_link  = new Array("", tablink[0][1], tablink[1][1], tablink[2][1], tablink[3][1], tablink[4][1], tablink[5][1], tablink[6][1], tablink[7][1], tablink[8][1]);

//Level 1 Menu
menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", "<#Menu_TrafficManager#>", "<#Parental_Control#>", "<#Menu_usb_application#>", "AiCloud", "Tools", "<#menu5#>");
menuL1_link = new Array("", "index.asp", "Guest_network.asp", "QoS_EZQoS.asp", "ParentalControl.asp", "APP_Installation.asp", "cloud_main.asp", tablink[10][1], "");

var rc_support = "<% nvram_get("rc_support"); %>"; 
var wl_vifnames = "<% nvram_get("wl_vifnames"); %>";
var traffic_L1_dx = 3;
var traffic_L2_dx = 10;

var band2g_support = rc_support.search("2.4G");
var band5g_support = rc_support.search("5G");
var live_update_support = rc_support.search("update"); 
var cooler_support = rc_support.search("fanctrl"); 
var power_support = rc_support.search("pwrctrl"); 
var dbwww_support = rc_support.search("dbwww"); 
var repeater_support = rc_support.search("repeater"); 

var psta_support = rc_support.search("psta"); // AC66U proxy sta support
if(psta_support != -1) repeater_support = 1;
var wl6_support = rc_support.search("wl6"); // BRCM wl6 support

var Rawifi_support = rc_support.search("rawifi");
var SwitchCtrl_support = rc_support.search("switchctrl");
var dsl_support = rc_support.search("dsl");
var dualWAN_support = rc_support.search("dualwan");
var ruisp_support = rc_support.search("ruisp");

var multissid_support = rc_support.search("mssid");
if(sw_mode == 2)
	multissid_support = -1;
if(multissid_support != -1)
	multissid_support = wl_vifnames.split(" ").length;
var no5gmssid_support = rc_support.search("no5gmssid");

var wifi_hw_sw_support = rc_support.search("wifi_hw_sw");
var usb_support = rc_support.search("usb"); 
var printer_support = rc_support.search("printer"); 
var appbase_support = rc_support.search("appbase");
var appnet_support = rc_support.search("appnet");
var media_support = rc_support.search("media");
var cloudsync_support = rc_support.search("cloudsync");
var modem_support = rc_support.search("modem"); 
var IPv6_support = rc_support.search("ipv6"); 

var ParentalCtrl2_support = rc_support.search("PARENTAL2");
var ParentalCtrl_support = rc_support.search("PARENTAL "); 

var pptpd_support = rc_support.search("pptpd"); 
var openvpnd_support = rc_support.search("openvpnd"); 
var dualWAN_support = rc_support.search("dualwan"); 
var WebDav_support = rc_support.search("webdav"); 
var HTTPS_support = rc_support.search("HTTPS"); 
var nodm_support = rc_support.search("nodm"); 
var wimax_support = rc_support.search("wimax");
var downsize_support = rc_support.search("sfp4m");

var nfsd_support = rc_support.search("nfsd");

var calculate_height = menuL1_link.length+tablink.length-1;

function remove_url(){
	remove_menu_item(2, "Advanced_Modem_Content.asp");
	
	if('<% nvram_get("start_aicloud"); %>' == '0')
		menuL1_link[6] = "cloud__main.asp"

	if(downsize_support != -1) {
		remove_menu_item(9, "Main_TrafficMonitor_realtime.asp");
	}

	if(dsl_support == -1) {
		remove_menu_item(7, "Advanced_ADSL_Content.asp");
		remove_menu_item(8, "Main_AdslStatus_Content.asp");
	}
	else {
		menuL2_link[3] = "Advanced_DSL_Content.asp";
		remove_menu_item(0, "Advanced_WMode_Content.asp");

		//no_op_mode
		var tmp_link = tablink[7][1];
		tablink[7][1]=tablink[7][2];
		tablink[7][2]=tablink[7][3];
		tablink[7][3]=tablink[7][4];
		tablink[7][4]=tablink[7][5];
		tablink[7][5]=tablink[7][6];
		tablink[7][6]=tmp_link;
		var tmp_title = tabtitle[7][1];
		tabtitle[7][1]=tabtitle[7][2];
		tabtitle[7][2]=tabtitle[7][3];
		tabtitle[7][3]=tabtitle[7][4];
		tabtitle[7][4]=tabtitle[7][5];
		tabtitle[7][5]=tabtitle[7][6];		
		tabtitle[7][6]=tmp_title;
		remove_menu_item(7, "Advanced_OperationMode_Content.asp");		
		menuL2_link[8] = tablink[7][1];
	}

	if(WebDav_support != -1) {
		tabtitle[3][2] = "<#menu5_4_1#> / Cloud Disk";
	}

	if(cloudsync_support == -1) {
		menuL1_title[6]="";
		menuL1_link[6]="";
	}
		
	if(sw_mode == 2){
		// Guest Network
		menuL1_title[2] ="";
		menuL1_link[2] ="";
		// Traffic Manager
		menuL1_title[3] ="";
		menuL1_link[3] ="";
		// Parental Ctrl
		menuL1_title[4] ="";
		menuL1_link[4] ="";				
		// AiCloud
		menuL1_title[6] ="";
		menuL1_link[6] ="";				
		// Wireless
		menuL2_title[1]="";
		menuL2_link[1]="";
		// WAN
		menuL2_title[3]="";
		menuL2_link[3]="";	
		// LAN
		remove_menu_item(1, "Advanced_DHCP_Content.asp");
		remove_menu_item(1, "Advanced_GWStaticRoute_Content.asp");
		remove_menu_item(1, "Advanced_IPTV_Content.asp");								
		remove_menu_item(1, "Advanced_SwitchCtrl_Content.asp");
		// VPN
		menuL2_title[5]="";
		menuL2_link[5]="";
		//IPv6
		menuL2_title[6]="";
		menuL2_link[6]="";
		// Firewall		
		menuL2_title[7]="";
		menuL2_link[7]="";
		// Log
		remove_menu_item(8, "Main_DHCPStatus_Content.asp");
		remove_menu_item(8, "Main_IPTStatus_Content.asp");
		remove_menu_item(8, "Main_RouteStatus_Content.asp");								
	}
	else if(sw_mode == 3){
		// Traffic Manager
		menuL1_title[3] ="";
		menuL1_link[3] ="";		
		// Parental Ctrl
		menuL1_title[4] ="";
		menuL1_link[4] ="";
		// AiCloud
		menuL1_title[6] ="";
		menuL1_link[6] ="";

		// Wireless
		remove_menu_item(0, "Advanced_WMode_Content.asp");
		remove_menu_item(0, "Advanced_WWPS_Content.asp");
		// WAN
		menuL2_title[3]="";
		menuL2_link[3]="";
		// LAN
		remove_menu_item(1, "Advanced_DHCP_Content.asp");
		remove_menu_item(1, "Advanced_GWStaticRoute_Content.asp");
		remove_menu_item(1, "Advanced_IPTV_Content.asp");
		remove_menu_item(1, "Advanced_SwitchCtrl_Content.asp");
		// VPN
		menuL2_title[5]="";
		menuL2_link[5]="";
		// IPv6
		menuL2_title[6]="";
		menuL2_link[6]="";
		// Firewall		
		menuL2_title[7]="";
		menuL2_link[7]="";
		// Log
		remove_menu_item(8, "Main_DHCPStatus_Content.asp");
		remove_menu_item(8, "Main_IPTStatus_Content.asp");
		remove_menu_item(8, "Main_RouteStatus_Content.asp");								
	}
	
	if(dualWAN_support == -1){
		remove_menu_item(2, "Advanced_WANPort_Content.asp");
		if (dsl_support != -1) {
			tablink[2][1] = "Advanced_DSL_Content.asp";
		}
	}
	else{
		var dualwan_pri_if = '<% nvram_get("wans_dualwan"); %>'.split(" ");
		if(dualwan_pri_if == 'lan') tablink[2][1] = "Advanced_WAN_Content.asp";
		else if(dualwan_pri_if == 'wan') tablink[2][1] = "Advanced_WAN_Content.asp";
		else if(dualwan_pri_if == 'usb') tablink[2][1] = "Advanced_Modem_Content.asp";
		else if(dualwan_pri_if == 'dsl') tablink[2][1] = "Advanced_DSL_Content.asp";
	}

	if(media_support == -1){
		tabtitle[3].splice(1, 1);
		tablink[3].splice(1, 1);	
	}

//	if(cooler_support == -1){
//		remove_menu_item(7, "Advanced_PerformanceTuning_Content.asp");
//	}

	if(ParentalCtrl2_support == -1){
		menuL1_title[4]="";
		menuL1_link[4]="";
	}

	if((ParentalCtrl_support == -1) || (ParentalCtrl2_support != -1))
		remove_menu_item(6, "Advanced_MACFilter_Content.asp");

	if(IPv6_support == -1){
		menuL2_title[5] = "";
		menuL2_link[5] = "";
	}
	
	if(multissid_support == -1){
		menuL1_title[2]="";
		menuL1_link[2]="";
	}

	if(usb_support == -1){
		menuL1_title[5]="";
		menuL1_link[5]="";
	}

	if(pptpd_support == -1){
		remove_menu_item(5, "Advanced_PPTP_Content.asp");
		if(openvpnd_support == -1){
			menuL2_title[6] = "";
			menuL2_link[6] = "";
		}
	}	

	if(openvpnd_support == -1){
		remove_menu_item(5,"Advanced_OpenVPNServer_Content.asp");
		remove_menu_item(5,"Advanced_OpenVPNClient_Content.asp");
		remove_menu_item(5,"Advanced_OpenVPN_Keys.asp");
		remove_menu_item(5,"Advanced_VPNStatus.asp");
	}

	if(nfsd_support == -1){
		remove_menu_item(3,"Advanced_AiDisk_NFS.asp")
	}

	if(SwitchCtrl_support == -1){
		remove_menu_item(1, "Advanced_SwitchCtrl_Content.asp");		
	}

  if(repeater_support != -1 && psta_support == -1){
    remove_menu_item(0, "Advanced_WMode_Content.asp");
  }
}

function remove_menu_item(L2, remove_url){
	var dx;
	for(var i = 0; i < tablink[L2].length; i++){
		dx = tablink[L2].getIndexByValue(remove_url);
		if(dx == -1)	//If not match, pass it
			return false;
		else if(dx == 1) //If removed url is the 1st tablink then replace by next tablink 
			menuL2_link.splice(L2+1, 1, tablink[L2][2]);
		else{
			tabtitle[L2].splice(dx, 1);
			tablink[L2].splice(dx, 1);
			break;
		}
	}
}

Array.prototype.getIndexByValue = function(value){
	var index = -1;
	for(var i=0; i<this.length; i++){
		if (this[i] == value){
			index = i;
			break;
		}
	}
	return index;
}

Array.prototype.getIndexByValue2D = function(value){
	for(var i=0; i<this.length; i++){
		if(this[i].getIndexByValue(value) != -1){
			return [i, this[i].getIndexByValue(value)]; // return [1-D_index, 2-D_index];
		}
	}
	return -1;
}

var current_url = location.pathname.substring(location.pathname.lastIndexOf('/') + 1);
function show_menu(){
	var L1 = 0, L2 = 0, L3 = 0;
	if(current_url == "") current_url = "index.asp";
	if (dualWAN_support != -1) {
		// fix dualwan showmenu
		if(current_url == "Advanced_DSL_Content.asp") current_url = "Advanced_WAN_Content.asp";
		if(current_url == "Advanced_Modem_Content.asp") current_url = "Advanced_WAN_Content.asp";
	}

	remove_url();
	// calculate L1, L2, L3.
	for(var i = 1; i < menuL1_link.length; i++){
		if(current_url == menuL1_link[i]){
			L1 = i;
			break;
		}
		else
			L1 = menuL1_link.length;
	}
	if(L1 == menuL1_link.length){
		for(var j = 0; j < tablink.length; j++){
			for(var k = 1; k < tablink[j].length; k++){
				if(current_url == tablink[j][k]){
					L2 = j+1;
					L3 = k;
					break;
				}
			}
		}
	}

	// special case for Traffic Manager and Tools menu
	if(L1 == traffic_L1_dx || L2 == traffic_L2_dx || L1 == 7){
		if(current_url.indexOf("Main_TrafficMonitor_") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			L3 = 2;
		}
		else if(current_url.indexOf("ParentalControl") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			L3 = 3;
		}
		else if(current_url.indexOf("cloud") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			L3 = 1;
		}
		else if(current_url.indexOf("Tools_") == 0){
			L1 = 7;
			L2 = 11;
			L3 = 1;
		}

		else{
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			L3 = 1;
		}
	}
	//end

	// tools
	if(current_url.indexOf("Tools_") == 0)
		L1 = 7;

	// cloud
	if(current_url.indexOf("cloud") == 0)
		L1 = 6;

	show_banner(L3);
	show_footer();
	browser_compatibility();	
	show_selected_language();
	autoFocus('<% get_parameter("af"); %>');

	// QIS wizard
	if(sw_mode == 2){
		if(psta_support == -1)
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'http://www.asusnetwork.net/QIS_wizard.htm?flag=sitesurvey\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
		else
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'/QIS_wizard.htm?flag=sitesurvey\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
	}else if(sw_mode == 3){
		menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'QIS_wizard.htm?flag=lanip\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n'; 	
	}else{
		menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'QIS_wizard.htm?flag=detect\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
	}	

	// Feature
	menu1_code += '<div class="m0_r" style="margin-top:10px;" id="option0"><table width="192px" height="37px"><tr><td><#menu5_1_1#></td></tr></table></div>\n';
	for(i = 1; i <= menuL1_title.length-2; i++){
		if(menuL1_title[i] == ""){
			calculate_height--;
			continue;
		}
		else if(L1 == i && (L2 <= 0 || L2 == traffic_L2_dx || L1 ==  7)){
		  //menu1_code += '<div class="m'+i+'_r" id="option'+i+'">'+'<table><tr><td><img border="0" width="50px" height="50px" src="images/New_ui/icon_index_'+i+'.png" style="margin-top:-3px;"/></td><td><div style="width:120px;">'+menuL1_title[i]+'</div></td></tr></table></div>\n';
		  menu1_code += '<div class="m'+i+'_r" id="option'+i+'">'+'<table><tr><td><div id="index_img'+i+'"></div></td><td><div id="menu_string'+i+'" style="width:120px;">'+menuL1_title[i]+'</div></td></tr></table></div>\n';
		}
		else{
		  menu1_code += '<div class="menu" id="option'+i+'" onclick="location.href=\''+menuL1_link[i]+'\'" style="cursor:pointer;"><table><tr><td><div id="index_img'+i+'"></div></td><td><div id="menu_string" style="width:120px;">'+menuL1_title[i]+'</div></td></tr></table></div>\n';
		}
	}
	menu1_code += '<div class="m0_r" style="margin-top:10px;" id="option0">'+'<table width="192px" height="37px"><tr><td><#menu5#></td></tr></table></div>\n'; 	
	$("mainMenu").innerHTML = menu1_code;

	// Advanced
	if(L2 != -1){ 
		for(var i = 1; i < menuL2_title.length; ++i){
			if(menuL2_link[i] == "Advanced_Wireless_Content.asp" && "<% nvram_get("wl_subunit"); %>" != "0" && "<% nvram_get("wl_subunit"); %>" != "-1")
				menuL2_link[i] = "javascript:change_wl_unit_status(" + <% nvram_get("wl_unit"); %> + ");";
			if(menuL2_title[i] == "" || i == 4){
				calculate_height--;
				continue;
			}
			else if(L2 == i){
				//menu2_code += '<div class="m'+i+'_r" id="option'+i+'">'+'<table><tr><td><img border="0" width="50px" height="50px" src="images/New_ui/icon_menu_'+i+'.png" style="margin-top:-3px;"/></td><td><div style="width:120px;">'+menuL2_title[i]+'</div></td></tr></table></div>\n';
				menu2_code += '<div class="m'+i+'_r" id="option'+i+'">'+'<table><tr><td><div id="menu_img'+i+'"></div></td><td><div id="option_str1" style="width:120px;">'+menuL2_title[i]+'</div></td></tr></table></div>\n';
			}else{				
				menu2_code += '<div class="menu" id="option'+i+'" onclick="location.href=\''+menuL2_link[i]+'\'" style="cursor:pointer;"><table><tr><td><div id="menu_img'+i+'"></div></td><td><div id="option_str1" style="width:120px;">'+menuL2_title[i]+'</div></td></tr></table></div>\n';
			}	
		}
	}
	$("subMenu").innerHTML = menu2_code;

	// Tabs
	if(L3){
		if(L2 == traffic_L2_dx){ // if tm
			tab_code = '<table border="0" cellspacing="0" cellpadding="0"><tr>\n';
			for(var i=1; i < tabtitle[traffic_L2_dx-1].length; ++i){
				if(tabtitle[traffic_L2_dx-1][i] == "")
					continue;
				else if(L3 == i)
					tab_code += '<td><div class="tabclick"><span>'+ tabtitle[traffic_L2_dx-1][i] +'</span></div></td>';
				else
					tab_code += '<td><a href="' + tablink[traffic_L2_dx-1][i] + '"><div class="tab"><span>'+ tabtitle[traffic_L2_dx-1][i] +'</span></div></a></td>';
			}
			tab_code += '</tr></table>\n';		
		}
		else{
			tab_code = '<table border="0" cellspacing="0" cellpadding="0"><tr>\n';
			for(var i=1; i < tabtitle[L2-1].length; ++i){
				if(tabtitle[L2-1][i] == "")
					continue;
				else if(L3 == i)
					tab_code += '<td><div class="tabclick"><span><table><tbody><tr><td>'+tabtitle[L2-1][i]+'</td></tr></tbody></table></span></div></td>';
				else
					tab_code += '<td><div class="tab"onclick="location.href=\''+tablink[L2-1][i]+'\'" style="cursor:pointer;"><span><table><tbody><tr><td>'+tabtitle[L2-1][i]+'</td></tr></tbody></table></span></div></td>';
			}
			tab_code += '</tr></table>\n';		
		}

		$("tabMenu").innerHTML = tab_code;
	}

	if(current_url == "index.asp" || current_url == "")
		cal_height();
	else
		setTimeout('cal_height();', 1);
}

function addOnlineHelp(obj, keywordArray){
	var faqLang = {
		EN : "en",
		TW : "en",
		CN : "en",
		CZ : "en",
		PL : "en",
		RU : "en",
		DE : "en",
		FR : "en",
		TR : "en",
		TH : "en",
		MS : "en",
		NO : "en",
		FI : "en",
		DA : "en",
		SV : "en",
		BR : "en",
		JP : "en",
		ES : "en"
	}

	// exception start
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "download" 
			&& (keywordArray[2] == "master" || keywordArray[2] == "tool")){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}		
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "ez" && keywordArray[2] == "printer"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}		
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "lpr"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}
	if(keywordArray[0] == "mac" && keywordArray[1] == "lpr"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}
	if(keywordArray[0] == "monopoly" && keywordArray[1] == "mode"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}			
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "VPN"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "DMZ"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}	
	if(keywordArray[0] == "set" && keywordArray[1] == "up" && keywordArray[2] == "specific" && keywordArray[3] == "IP" && keywordArray[4] == "addresses"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";		
	}
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "port" && keywordArray[2] == "forwarding"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";		
	}
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "port" && keywordArray[2] == "trigger"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.ES = "es-es";		
	}
	//keyword=UPnP
	if(keywordArray[0] == "UPnP"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}	
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "hard" && keywordArray[2] == "disk" 
		&& keywordArray[3] == "USB"){

	}	
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "Traffic" && keywordArray[2] == "Monitor"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";		
	}	
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "samba" && keywordArray[2] == "Windows"
			&& keywordArray[3] == "network" && keywordArray[4] == "place"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}
	if(keywordArray[0] == "samba"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}
	if(keywordArray[0] == "WOL" && keywordArray[1] == "wake" && keywordArray[2] == "on"
			&& keywordArray[3] == "lan"){
	}
	if(keywordArray[0] == "WOL" && keywordArray[1] == "BIOS"){
		faqLang.TW = "zh-tw";
		faqLang.CN = "zh-cn";
		faqLang.FR = "fr-fr";
		faqLang.ES = "es-es";
	}		
	// exception end	
	

	if(obj){
		obj.href = "http://support.asus.com/search.aspx?SLanguage=";
		obj.href += faqLang.<% nvram_get("preferred_lang"); %>;
		obj.href += "&keyword=";
		for(var i=0; i<keywordArray.length; i++){
			obj.href += keywordArray[i];
			obj.href += "%20";
		}
	}
}

function Block_chars(obj, keywordArray){
	// bolck ascii code 32~126 first
	var invalid_char = "";		
	for(var i = 0; i < obj.value.length; ++i){
		if(obj.value.charCodeAt(i) < '32' || obj.value.charCodeAt(i) > '126'){
			invalid_char += obj.value.charAt(i);
		}
	}
	if(invalid_char != ""){
		alert('<#JS_validstr2#>" '+ invalid_char +'" !');
		obj.focus();
		return false;
	}

	// check if char in the specified array
	if(obj.value){
		for(var i=0; i<keywordArray.length; i++){
				if( obj.value.indexOf(keywordArray[i]) >= 0){						
					alert(keywordArray+ " are invalid characters.");
					obj.focus();
					return false;
				}	
		}
	}
	
	return true;
}

function cal_height(){
	var tableClientHeight;
	var optionHeight = 52;
	var manualOffSet = 28;
	var table_height = Math.round(optionHeight*calculate_height - manualOffSet*calculate_height/14 - $("tabMenu").clientHeight);	
	if(navigator.appName.indexOf("Microsoft") < 0)
		var contentObj = document.getElementsByClassName("content");
	else
		var contentObj = getElementsByClassName_iefix("table", "content");

	if($("FormTitle") && current_url.indexOf("Advanced_AiDisk_ftp") != 0 && current_url.indexOf("Advanced_AiDisk_samba") != 0){
		table_height = table_height + 24;
		$("FormTitle").style.height = table_height + "px";
		tableClientHeight = $("FormTitle").clientHeight;
	}
	// index.asp
	else if($("NM_table")){
		var statusPageHeight = 720;
		if(table_height < 740)
			table_height = 740;
		$("NM_table").style.height = table_height + "px";
		$("NM_table_div").style.marginTop = (table_height - statusPageHeight)/2 + "px";
		tableClientHeight = $("NM_table").clientHeight;
	}
	// QoS_EZQoS.asp
	else if($("qos_table")){
		$("qos_table").style.height = table_height + "px";
		tableClientHeight = $("qos_table").clientHeight;
	}
	// aidisk.asp
	else if($("AiDiskFormTitle")){
		table_height = table_height + 24;
		$("AiDiskFormTitle").style.height = table_height + "px";
		tableClientHeight = $("AiDiskFormTitle").clientHeight;
	}
	// APP Install
	else if($("applist_table")){
		if(sw_mode == 2 || sw_mode == 3)
				table_height = table_height + 120;				
		else
				table_height = table_height + 40;	//2
		$("applist_table").style.height = table_height + "px";		
		tableClientHeight = $("applist_table").clientHeight;
		
		if(navigator.appName.indexOf("Microsoft") >= 0)
						contentObj[0].style.height = contentObj[0].clientHeight + 18 + "px";
	}
	// PrinterServ
	else if($("printerServer_table")){
		if(sw_mode == 2)
				table_height = table_height + 90;
		else
				table_height = table_height + 2;
		$("printerServer_table").style.height = table_height + "px";		
		tableClientHeight = $("printerServer_table").clientHeight;
		
		if(navigator.appName.indexOf("Microsoft") >= 0)
						contentObj[0].style.height = contentObj[0].clientHeight + 18 + "px";
	}	

	/*// overflow
	var isOverflow = parseInt(tableClientHeight) - parseInt(table_height);
	if(isOverflow >= 0){
		if(current_url.indexOf("Main_TrafficMonitor_realtime") == 0 && navigator.appName.indexOf("Microsoft") < 0)
			contentObj[0].style.height = contentObj[0].clientHeight + 45 + "px";
		else
			contentObj[0].style.height = contentObj[0].clientHeight + 19 + "px";	
	}*/
}

function show_footer(){
	footer_code = '<div align="center" class="bottom-image"></div>\n';
	footer_code +='<div align="center" class="copyright"><#footer_copyright_desc#></div><br>';

	// FAQ searching bar{
	footer_code += '<div style="margin-top:-75px;margin-left:205px;"><table width="765px" border="0" align="center" cellpadding="0" cellspacing="0"><tr>';
	footer_code += '<td width="20" align="right"><div id="bottom_help_icon" style="margin-right:3px;"></div></td><td width="100" id="bottom_help_title" align="left">Help & Support</td>';

	if(hw_ver.search("RTN12") != -1){	//MODELDEP : RT-N12
		if( hw_ver.search("RTN12HP") != -1){	//RT-N12HP
				footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12HP" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12HP" target="_blank">Utility</a></td>';
		}else if(hw_ver.search("RTN12B1") != -1){ //RT-N12B1
				footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.B1)" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.B1)" target="_blank">Utility</a></td>';
		}else if(hw_ver.search("RTN12C1") != -1){ //RT-N12C1
				footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.C1)" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.C1)" target="_blank">Utility</a></td>';
		}else if(hw_ver.search("RTN12D1") != -1){ //RT-N12D1
				footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.D1)" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N12%20(VER.D1)" target="_blank">Utility</a></td>';
		}else{	//RT-N12
				footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="<#bottom_Link#>" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="<#bottom_Link#>" target="_blank">Utility</a></td>';
		}
	}
	else	if("<#Web_Title2#>" == "RT-N66U"){	//MODELDEP : RT-N66U
		footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N66U%20(VER.B1)" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=RT-N66U%20(VER.B1)" target="_blank">Utility</a></td>';
	}
	else if("<#Web_Title2#>" == "DSL-N55U-B"){	//MODELDEP : DSL-N55U-B
		footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=DSL-N55U%20(VER.B1)" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="http://support.asus.com/download.aspx?SLanguage=en&m=DSL-N55U%20(VER.B1)" target="_blank">Utility</a></td>';
	}
	else{
		footer_code += '<td width="200" id="bottom_help_link" align="left">&nbsp&nbsp<a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="<#bottom_Link#>" target="_blank">Manual</a> | <a style="font-weight: bolder;text-decoration:underline;cursor:pointer;" href="<#bottom_Link#>" target="_blank">Utility</a></td>';	
	}			

	footer_code += '<td width="390" id="bottom_help_FAQ" align="right" style="font-family:Arial, Helvetica, sans-serif;">FAQ&nbsp&nbsp<input type="text" id="FAQ_input" name="FAQ_input" class="input_FAQ_table" maxlength="40"></td>';
	footer_code += '<td width="30" align="left"><div id="bottom_help_FAQ_icon" class="bottom_help_FAQ_icon" style="cursor:pointer;margin-left:3px;" target="_blank" onClick="search_supportsite();"></div></td>';
	footer_code += '</tr></table></div>\n';
	//}

	$("footer").innerHTML = footer_code;
	flash_button();
}

function search_supportsite(obj){
	var faqLang = {
		EN : "en",
		TW : "zh-tw",
		CN : "zh-cn",
		BR : "en",		
		CZ : "en",
		DA : "en",
		DE : "en",		
		ES : "es-es",
		FI : "en",
		FR : "fr-fr",
		IT : "en",
		JP : "en",				
		MS : "en",
		NO : "en",
		PL : "en",
		RU : "en",
		SV : "en",
		TH : "en",
		TR : "en",		
		UK : "en"
	}	
	
		var keywordArray = $("FAQ_input").value.split(" ");
		var faq_href;
		faq_href = "http://support.asus.com/search.aspx?SLanguage=";
		faq_href += faqLang.<% nvram_get("preferred_lang"); %>;
		faq_href += "&keyword=";
		for(var i=0; i<keywordArray.length; i++){
			faq_href += keywordArray[i];
			faq_href += "%20";
		}
		window.open(faq_href);
}

function browser_compatibility(){
	var isFirefox = navigator.userAgent.search("Firefox") > -1;
	var isOpera = navigator.userAgent.search("Opera") > -1;
	var isIE8 = navigator.userAgent.search("MSIE 8") > -1; 
	var isiOS = navigator.userAgent.search("iP") > -1; 
	var obj_inputBtn;

	if((isFirefox || isOpera) && document.getElementById("FormTitle")){
		document.getElementById("FormTitle").className = "FormTitle_firefox";
		if(current_url.indexOf("ParentalControl") == 0 || current_url.indexOf("Guest_network") == 0)
			document.getElementById("FormTitle").style.marginTop = "-140px";	
	}

	if(isiOS){
		/* language options */
		document.body.addEventListener("touchstart", mouseClick, false);

		obj_inputBtn = document.getElementsByClassName("button_gen");
		for(var i=0; i<obj_inputBtn.length; i++){
			obj_inputBtn[i].addEventListener('touchstart', function(){this.className = 'button_gen_touch';}, false);
			obj_inputBtn[i].addEventListener('touchend', function(){this.className = 'button_gen';}, false);
		}
		obj_inputBtn = document.getElementsByClassName("button_gen_long");
		for(var i=0; i<obj_inputBtn.length; i++){
			obj_inputBtn[i].addEventListener('touchstart', function(){this.className = 'button_gen_long_touch';}, false);
			obj_inputBtn[i].addEventListener('touchend', function(){this.className = 'button_gen_long';}, false);
		}
	}
}	

var mouseClick = function(){
	var e = document.createEvent('MouseEvent');
	e.initEvent('click', false, false);
	document.getElementById("modelName_top").dispatchEvent(e);
}

function show_top_status(){
	var ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0_ssid"); %>');
	var ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1_ssid"); %>');
	if(sw_mode == 2){
		if(psta_support == -1){
			if('<% nvram_get("wlc_band"); %>' == '0')
				ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0.1_ssid"); %>');
			else
				ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1.1_ssid"); %>');
		}
		else{
			// $("ssidTitle").style.display = "none";
			if('<% nvram_get("wlc_band"); %>' == '0'){
				$("elliptic_ssid_2g").style.display = "none";
				$("elliptic_ssid_5g").style.marginLeft = "";
			}
			else
				$("elliptic_ssid_5g").style.display = "none";
		}
		$('elliptic_ssid_2g').style.textDecoration="none";
		$('elliptic_ssid_2g').style.cursor="auto";
		$('elliptic_ssid_5g').style.textDecoration="none";
		$('elliptic_ssid_5g').style.cursor="auto";
	}
	
	if(band5g_support == -1){
		ssid_status_5g = "";
		if(ssid_status_2g.length > 23){
			ssid_status_2g = ssid_status_2g.substring(0,20) + "...";
			$("elliptic_ssid_2g").onmouseover = function(){
				if(sw_mode == 2 && '<% nvram_get("wlc_band"); %>' == 0)
					return overlib('<p style="font-weight:bold;">2.4GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0.1_ssid"); %>'));
				else
					return overlib('<p style="font-weight:bold;">2.4GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0_ssid"); %>'));
			};
			$("elliptic_ssid_2g").onmouseout = function(){
				nd();
			};
		}
	}
	else{
		if(ssid_status_2g.length > 12){
			ssid_status_2g = ssid_status_2g.substring(0,10) + "...";
			$("elliptic_ssid_2g").onmouseover = function(){
				if(sw_mode == 2 && '<% nvram_get("wlc_band"); %>' == 0)
					return overlib('<p style="font-weight:bold;">2.4GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0.1_ssid"); %>'));
				else
					return overlib('<p style="font-weight:bold;">2.4GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0_ssid"); %>'));
			};
			$("elliptic_ssid_2g").onmouseout = function(){
				nd();
			};
		}
	
		if(ssid_status_5g.length > 12){
			ssid_status_5g = ssid_status_5g.substring(0,10) + "...";
			$("elliptic_ssid_5g").onmouseover = function(){
				if(sw_mode == 2 && '<% nvram_get("wlc_band"); %>' == 1)
					return overlib('<p style="font-weight:bold;">5GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1.1_ssid"); %>'));
				else
					return overlib('<p style="font-weight:bold;">5GHz SSID:</p>'+decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1_ssid"); %>'));
			};
			$("elliptic_ssid_5g").onmouseout = function(){
				nd();
			};
		}
	}

	$("elliptic_ssid_2g").innerHTML = ssid_status_2g;
	$("elliptic_ssid_5g").innerHTML = ssid_status_5g;	

	showtext($("firmver"), document.form.firmver.value + "." + "<% nvram_get("buildno"); %>");
	
	// no_op_mode
	if (dsl_support	== -1) {
		if(sw_mode == "1")  // Show operation mode in banner, Viz 2011.11
			$("sw_mode_span").innerHTML = "<#wireless_router#>";
		else if(sw_mode == "2"){
			if(psta_support == -1)
				$("sw_mode_span").innerHTML = "<#OP_RE_item#>";
			else
				$("sw_mode_span").innerHTML = "Media bridge";
		}
		else if(sw_mode == "3")
			$("sw_mode_span").innerHTML = "<#OP_AP_item#>";
		else
			$("sw_mode_span").innerHTML = "Unknown";	
	}
}

function go_setting(page){
		location.href = page;
}

function go_setting_parent(page){
		parent.location.href = page;
}

function show_time(){	
	JS_timeObj.setTime(systime_millsec); // Add millsec to it.	
	JS_timeObj3 = JS_timeObj.toString();	
	JS_timeObj3 = checkTime(JS_timeObj.getHours()) + ":" +
				  			checkTime(JS_timeObj.getMinutes()) + ":" +
				  			checkTime(JS_timeObj.getSeconds());
	$('systemtime').innerHTML ="<a href='/Advanced_System_Content.asp'>" + JS_timeObj3 + "</a>";
	systime_millsec += 1000;		
	
	stime_ID = setTimeout("show_time();", 1000);
}

function checkTime(i)
{
if (i<10) 
  {i="0" + i}
  return i
}

function show_loading_obj(){
	var obj = $("Loading");
	var code = "";
	
	code +='<table cellpadding="5" cellspacing="0" id="loadingBlock" class="loadingBlock" align="center">\n';
	code +='<tr>\n';
	code +='<td width="20%" height="80" align="center"><img src="/images/loading.gif"></td>\n';
	code +='<td><span id="proceeding_main_txt"><#Main_alert_proceeding_desc4#></span><span id="proceeding_txt" style="color:#FFFFCC;"></span></td>\n';
	code +='</tr>\n';
	code +='</table>\n';
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->\n';
	
	obj.innerHTML = code;
}

var nav;

if(navigator.appName == 'Netscape')
	nav = true;
else{
	nav = false;
	document.onkeydown = MicrosoftEventHandler_KeyDown;
}

function MicrosoftEventHandler_KeyDown(){
	return true;
}

// display selected language in language bar, Jieming added at 2012/06/19
function show_selected_language(){
	switch($("preferred_lang").value){
		case 'EN':{
			$('selected_lang').innerHTML = "English";
			break;
		}
		case 'CN':{
			$('selected_lang').innerHTML = "简体中文";
			break;
		}	
		case 'TW':{
			$('selected_lang').innerHTML = "繁體中文";
			break;
		}
		case 'BR':{
			$('selected_lang').innerHTML = "Brazil";
			break;
		}
		case 'CZ':{
			$('selected_lang').innerHTML = "Česky";
			break;
		}
		case 'DA':{
			$('selected_lang').innerHTML = "Dansk";
			break;
		}	
		case 'DE':{
			$('selected_lang').innerHTML = "Deutsch";
			break;
		}
		case 'ES':{
			$('selected_lang').innerHTML = "Español";
			break;
		}
		case 'FI':{
			$('selected_lang').innerHTML = "Finsk";
			break;
		}
		case 'FR':{
			$('selected_lang').innerHTML = "Français";
			break;
		}
		case 'IT':{
			$('selected_lang').innerHTML = "Italiano";
			break;
		}		
		case 'JP':{
			$('selected_lang').innerHTML = "日本語";
			break;
		}
		case 'MS':{
			$('selected_lang').innerHTML = "Malay";
			break;
		}
		case 'NO':{
			$('selected_lang').innerHTML = "Norsk";
			break;
		}
		case 'PL':{
			$('selected_lang').innerHTML = "Polski";
			break;
		}
		case 'RU':{
			$('selected_lang').innerHTML = "Pусский";
			break;
		}
		case 'SV':{
			$('selected_lang').innerHTML = "Svensk";
			break;
		}		
		case 'TH':{
			$('selected_lang').innerHTML = "ไทย";
			break;
		}			
		case 'TR':{
			$('selected_lang').innerHTML = "Türkçe";
			break;
		}	
		case 'UK':{
			$('selected_lang').innerHTML = "Ukrainian";
			break;
		}		
	}
}


function submit_language(obj){
	//if($("select_lang").value != $("preferred_lang").value){
	if(obj.id != $("preferred_lang").value){
		showLoading();
		
		with(document.titleForm){
			action = "/start_apply.htm";
			
			if(location.pathname == "/")
				current_page.value = "/index.asp";
			else
				current_page.value = location.pathname;
				
			preferred_lang.value = obj.id;
			//preferred_lang.value = $("select_lang").value;
			flag.value = "set_language";
			
			submit();
		}
	}
	else
		alert("No change LANGUAGE!");
}

function change_language(){
	if($("select_lang").value != $("preferred_lang").value)
		$("change_lang_btn").disabled = false;
	else
		$("change_lang_btn").disabled = true;
}

function logout(){
	if(confirm('<#JS_logout#>')){
		setTimeout('location = "Logout.asp";', 1);
	}
}

function reboot(){
	if(confirm("<#Main_content_Login_Item7#>")){
		if(document.form.current_page){
			if(document.form.current_page.value.indexOf("Advanced_FirmwareUpgrade_Content")>=0
		  	 || document.form.current_page.value.indexOf("Advanced_SettingBackup_Content")>=0)
					document.form.removeAttribute('enctype');
		}

		FormActions("apply.cgi", "reboot", "", "<% get_default_reboot_time(); %>");
		document.form.submit();
	}
}

function kb_to_gb(kilobytes){
	if(typeof(kilobytes) == "string" && kilobytes.length == 0)
		return 0;
	
	return (kilobytes*1024)/(1024*1024*1024);
}

function simpleNum(num){
	if(typeof(num) == "string" && num.length == 0)
		return 0;
	
	return parseInt(kb_to_gb(num)*1000)/1000;
}

function simpleNum2(num){
	if(typeof(num) == "string" && num.length == 0)
		return 0;
	
	return parseInt(num*1000)/1000;
}

function simpleNum3(num){
	if(typeof(num) == "string" && num.length == 0)
		return 0;
	
	return parseInt(num)/1024;
}

function $(){
	var elements = new Array();
	
	for(var i = 0; i < arguments.length; ++i){
		var element = arguments[i];
		if(typeof element == 'string')
			element = document.getElementById(element);
		
		if(arguments.length == 1)
			return element;
		
		elements.push(element);
	}
	
	return elements;
}

function getElementsByName_iefix(tag, name){
	var tagObjs = document.getElementsByTagName(tag);
	var objsName;
	var targetObjs = new Array();
	var targetObjs_length;
	
	if(!(typeof(name) == "string" && name.length > 0))
		return [];
	
	for(var i = 0, targetObjs_length = 0; i < tagObjs.length; ++i){
		objsName = tagObjs[i].getAttribute("name");
		
		if(objsName && objsName.indexOf(name) == 0){
			targetObjs[targetObjs_length] = tagObjs[i];
			++targetObjs_length;
		}
	}
	
	return targetObjs;
}

function getElementsByClassName_iefix(tag, name){
	var tagObjs = document.getElementsByTagName(tag);
	var objsName;
	var targetObjs = new Array();
	var targetObjs_length;
	
	if(!(typeof(name) == "string" && name.length > 0))
		return [];
	
	for(var i = 0, targetObjs_length = 0; i < tagObjs.length; ++i){
		if(navigator.appName == 'Netscape')
			objsName = tagObjs[i].getAttribute("class");
		else
			objsName = tagObjs[i].getAttribute("className");
		
		if(objsName == name){
			targetObjs[targetObjs_length] = tagObjs[i];
			++targetObjs_length;
		}
	}
	
	return targetObjs;
}

function showtext(obj, str){
	if(obj)
		obj.innerHTML = str;//*/
}

function showhtmlspace(ori_str){
	var str = "", head, tail_num;
	
	head = ori_str;
	while((tail_num = head.indexOf(" ")) >= 0){
		str += head.substring(0, tail_num);
		str += "&nbsp;";
		
		head = head.substr(tail_num+1, head.length-(tail_num+1));
	}
	str += head;
	
	return str;
}

function showhtmland(ori_str){
	var str = "", head, tail_num;
	
	head = ori_str;
	while((tail_num = head.indexOf("&")) >= 0){
		str += head.substring(0, tail_num);
		str += "&amp;";
		
		head = head.substr(tail_num+1, head.length-(tail_num+1));
	}
	str += head;
	
	return str;
}

// A dummy function which just returns its argument. This was needed for localization purpose
function translate(str){
	return str;
}

function trim(val){
	val = val+'';
	for (var startIndex=0;startIndex<val.length && val.substring(startIndex,startIndex+1) == ' ';startIndex++);
	for (var endIndex=val.length-1; endIndex>startIndex && val.substring(endIndex,endIndex+1) == ' ';endIndex--);
	return val.substring(startIndex,endIndex+1);
}

function IEKey(){
	return event.keyCode;
}

function NSKey(){
	return event.which;
}

function is_string(o, event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	if(keyPressed >= 0 && keyPressed <= 126)
		return true;
	else{
		alert('<#JS_validchar#>');
		return false;
	}	
}

function is_alphanum(o, event){

	if (event.which == null)
		keyPressed = event.keyCode;	// IE
	 else if (event.which != 0 && event.charCode != 0)
		keyPressed = event.which	// All others
	else
		return true;			// Special key

	if ((keyPressed>=48&&keyPressed<=57) ||	//0-9
	   (keyPressed>=97&&keyPressed<=122) ||	//little EN
	   (keyPressed>=65&&keyPressed<=90) ||	//Large EN
	   (keyPressed==45) ||			//-
	   (keyPressed==46) ||			//.
	   (keyPressed==32)) return true;	//space

	return false;
}

function validate_string(string_obj, flag){
	if(string_obj.value.charAt(0) == '"'){
		if(flag != "noalert")
			alert('<#JS_validstr1#> ["]');
		
		string_obj.value = "";
		string_obj.focus();
		
		return false;
	}
	else{
		invalid_char = "";
		
		for(var i = 0; i < string_obj.value.length; ++i){
			if(string_obj.value.charAt(i) < ' ' || string_obj.value.charAt(i) > '~'){
				invalid_char = invalid_char+string_obj.value.charAt(i);
			}
		}
		
		if(invalid_char != ""){
			if(flag != "noalert")
				alert("<#JS_validstr2#> '"+invalid_char+"' !");
			string_obj.value = "";
			string_obj.focus();
			
			return false;
		}
	}
	
	return true;
}

function validate_hex(obj){
	var obj_value = obj.value
	var re = new RegExp("[^a-fA-F0-9]+","gi");
	
	if(re.test(obj_value))
		return false;
	else
		return true;
}

function validate_psk(psk_obj){
	var psk_length = psk_obj.value.length;
	
	if(psk_length < 8){
		alert("<#JS_passzero#>");
		psk_obj.value = "00000000";
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	if(psk_length > 64){
		alert("<#JS_passzero#>");
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	if(psk_length >= 8 && psk_length <= 63 && !validate_string(psk_obj)){
		alert("<#JS_PSK64Hex#>");
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	if(psk_length == 64 && !validate_hex(psk_obj)){
		alert("<#JS_PSK64Hex#>");
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	return true;
}

function validate_wlkey(key_obj){
	var wep_type = document.form.wl_wep_x.value;
	var iscurrect = true;
	var str = "<#JS_wepkey#>";

	if(wep_type == "0")
		iscurrect = true;	// do nothing
	else if(wep_type == "1"){
		if(key_obj.value.length == 5 && validate_string(key_obj)){
			document.form.wl_key_type.value = 1; /*Lock Add 11.25 for ralink platform*/
			iscurrect = true;
		}
		else if(key_obj.value.length == 10 && validate_hex(key_obj)){
			document.form.wl_key_type.value = 0; /*Lock Add 11.25 for ralink platform*/
			iscurrect = true;
		}
		else{
			str += "(<#WLANConfig11b_WEPKey_itemtype1#>)";
			
			iscurrect = false;
		}
	}
	else if(wep_type == "2"){
		if(key_obj.value.length == 13 && validate_string(key_obj)){
			document.form.wl_key_type.value = 1; /*Lock Add 11.25 for ralink platform*/
			iscurrect = true;
		}
		else if(key_obj.value.length == 26 && validate_hex(key_obj)){
			document.form.wl_key_type.value = 0; /*Lock Add 11.25 for ralink platform*/
			iscurrect = true;
		}
		else{
			str += "(<#WLANConfig11b_WEPKey_itemtype2#>)";
			
			iscurrect = false;
		}
	}
	else{
		alert("System error!");
		iscurrect = false;
	}
	
	if(iscurrect == false){
		alert(str);
		
		key_obj.focus();
		key_obj.select();
	}
	
	return iscurrect;
}

function validate_account(string_obj, flag){
	var invalid_char = "";

	if(string_obj.value.charAt(0) == ' '){
		if(flag != "noalert")
			alert('<#JS_validstr1#> [  ]');

		string_obj.value = "";
		string_obj.focus();

		if(flag != "noalert")
			return false;
		else
			return '<#JS_validstr1#> [&nbsp;&nbsp;&nbsp;]';
	}
	else if(string_obj.value.charAt(0) == '-'){
		if(flag != "noalert")
			alert('<#JS_validstr1#> [-]');

		string_obj.value = "";
		string_obj.focus();

		if(flag != "noalert")
			return false;
		else
			return '<#JS_validstr1#> [-]';
	}

	for(var i = 0; i < string_obj.value.length; ++i){
		if(string_obj.value.charAt(i) == '"'
				||  string_obj.value.charAt(i) == '/'
				||  string_obj.value.charAt(i) == '\\'
				||  string_obj.value.charAt(i) == '['
				||  string_obj.value.charAt(i) == ']'
				||  string_obj.value.charAt(i) == ':'
				||  string_obj.value.charAt(i) == ';'
				||  string_obj.value.charAt(i) == '|'
				||  string_obj.value.charAt(i) == '='
				||  string_obj.value.charAt(i) == ','
				||  string_obj.value.charAt(i) == '+'
				||  string_obj.value.charAt(i) == '*'
				||  string_obj.value.charAt(i) == '?'
				||  string_obj.value.charAt(i) == '<'
				||  string_obj.value.charAt(i) == '>'
				||  string_obj.value.charAt(i) == '@'
				||  string_obj.value.charAt(i) == ' '
				){
			invalid_char = invalid_char+string_obj.value.charAt(i);
		}
	}

	if(invalid_char != ""){
		if(flag != "noalert")
			alert("<#JS_validstr2#> ' "+invalid_char+" ' !");

		string_obj.value = "";
		string_obj.focus();

		if(flag != "noalert")
			return false;
		else
			return "<#JS_validstr2#> ' "+invalid_char+" ' !";
	}

	if(flag != "noalert")
		return true;
	else
		return "";
}

function checkDuplicateName(newname, targetArray){
	var existing_string = targetArray.join(',');
	existing_string = ","+existing_string+",";
	var newstr = ","+trim(newname)+",";
	
	var re = new RegExp(newstr, "gi");
	var matchArray = existing_string.match(re);
	
	if(matchArray != null)
		return true;
	else
		return false;
}

function alert_error_msg(error_msg){
	alert(error_msg);
	refreshpage();
}

function refreshpage(seconds){
	if(typeof(seconds) == "number")
		setTimeout("refreshpage()", seconds*1000);
	else
		location.href = location.href;
}

function hideLinkTag(){
	if(document.all){
		var tagObjs = document.all.tags("a");
		
		for(var i = 0; i < tagObjs.length; ++i)
			tagObjs(i).outerHTML = tagObjs(i).outerHTML.replace(">"," hidefocus=true>");
	}
}

function buttonOver(o){	//Lockchou 1206 modified
	o.style.color = "#FFFFFF";
	o.style.background = "url(/images/bgaibutton.gif) #ACCCE1";
	o.style.cursor = "hand";
}

function buttonOut(o){	//Lockchou 1206 modified
	o.style.color = "#000000";
	o.style.background = "url(/images/bgaibutton0.gif) #ACCCE1";
}

function flash_button(){
	if(navigator.appName.indexOf("Microsoft") < 0)
		return;
	
	var btnObj = getElementsByClassName_iefix("input", "button");
	
	for(var i = 0; i < btnObj.length; ++i){
		btnObj[i].onmouseover = function(){
				buttonOver(this);
			};
		
		btnObj[i].onmouseout = function(){
				buttonOut(this);
			};
	}
}

function no_flash_button(){
	if(navigator.appName.indexOf("Microsoft") < 0)
		return;
	
	var btnObj = getElementsByClassName_iefix("input", "button");
	
	for(var i = 0; i < btnObj.length; ++i){
		btnObj[i].onmouseover = "";
		
		btnObj[i].onmouseout = "";
	}
}

function gotoprev(formObj){
	var prev_page = formObj.prev_page.value;
	
	if(prev_page == "/")
		prev_page = "/index.asp";
	
	if(prev_page.indexOf('QIS') < 0){
		formObj.action = prev_page;
		formObj.target = "_parent";
		formObj.submit();
	}
	else{
		formObj.action = prev_page;
		formObj.target = "";
		formObj.submit();
	}
}

function add_option(selectObj, str, value, selected){
	var tail = selectObj.options.length;
	
	if(typeof(str) != "undefined")
		selectObj.options[tail] = new Option(str);
	else
		selectObj.options[tail] = new Option();
	
	if(typeof(value) != "undefined")
		selectObj.options[tail].value = value;
	else
		selectObj.options[tail].value = "";
	
	if(selected == 1)
		selectObj.options[tail].selected = selected;
}

function free_options(selectObj){
	if(selectObj == null)
		return;
	
	for(var i = selectObj.options.length-1; i >= 0; --i){
  		selectObj.options[i].value = null;
		selectObj.options[i] = null;
	}
}

function blocking(obj_id, show){
	var state = show?'block':'none';
	
	if(document.getElementById)
		$(obj_id).style.display = state;
	else if(document.layers)
		document.layers[obj_id].display = state;
	else if(document.all)
		document.all[obj_id].style.display = state;
}

function inputCtrl(obj, flag){
	if(flag == 0){
		obj.disabled = true;
		if(obj.type != "select-one")
			obj.style.backgroundColor = "#CCCCCC";
		if(obj.type == "radio" || obj.type == "checkbox")
			obj.style.backgroundColor = "#475A5F";
		if(obj.type == "text" || obj.type == "password")
			obj.style.backgroundImage = "url(/images/New_ui/inputbg_disable.png)";
	}
	else{
		obj.disabled = false;
		if(obj.type != "select-one")	
			obj.style.backgroundColor = "#FFF";
		if(obj.type == "radio" || obj.type == "checkbox")
			obj.style.backgroundColor = "#475A5F";
		if(obj.type == "text" || obj.type == "password")
			obj.style.backgroundImage = "url(/images/New_ui/inputbg.png)";
	}

	if(current_url.indexOf("Advanced_Wireless_Content") == 0
	|| current_url.indexOf("Advanced_WAN_Content") == 0
	|| current_url.indexOf("Guest_network") == 0
	|| current_url.indexOf("Advanced_PerformanceTuning_Content") == 0
	|| current_url.indexOf("Advanced_Modem_Content") == 0
	|| current_url.indexOf("Advanced_IPv6_Content") == 0
	|| current_url.indexOf("Advanced_WAdvanced_Content") == 0
	|| current_url.indexOf("Advanced_IPTV_Content") == 0
	|| current_url.indexOf("Advanced_WANPort_Content.asp") == 0
	|| current_url.indexOf("Advanced_ASUSDDNS_Content.asp") == 0)
	{
		if(obj.type == "checkbox")
			return true;
		if(flag == 0)
			obj.parentNode.parentNode.style.display = "none";
		else
			obj.parentNode.parentNode.style.display = "";
		return true;
	}
}

function inputHideCtrl(obj, flag){
	if(obj.type == "checkbox")
		return true;
	if(flag == 0)
		obj.parentNode.parentNode.style.display = "none";
	else
		obj.parentNode.parentNode.style.display = "";
	return true;
}

//Update current status via AJAX
var http_request_status = false;

function updateStatus_AJAX(){
	if(stopFlag == 1)
		return false;

	var ie = window.ActiveXObject;

	if(ie)
		makeRequest_status_ie('/ajax_status.asp');
	else
		makeRequest_status('/ajax_status.asp');
}

function makeRequest_status(url) {
	http_request_status = new XMLHttpRequest();
	if (http_request_status && http_request_status.overrideMimeType)
		http_request_status.overrideMimeType('text/xml');
	else
		return false;

	http_request_status.onreadystatechange = alertContents_status;
	http_request_status.open('GET', url, true);
	http_request_status.send(null);
}

var xmlDoc_ie;

function makeRequest_status_ie(file)
{
	xmlDoc_ie = new ActiveXObject("Microsoft.XMLDOM");
	xmlDoc_ie.async = false;
	if (xmlDoc_ie.readyState==4)
	{
		xmlDoc_ie.load(file);
		setTimeout("refresh_info_status(xmlDoc_ie);", 1000);
	}
}

function alertContents_status()
{
	if (http_request_status != null && http_request_status.readyState != null && http_request_status.readyState == 4)
	{
		if (http_request_status.status != null && http_request_status.status == 200)
		{
			var xmldoc_mz = http_request_status.responseXML;
			refresh_info_status(xmldoc_mz);
		}
	}
}

function updateUSBStatus(){
	if(current_url == "index.asp" || current_url == "")
		detectUSBStatusIndex();
	else
		detectUSBStatus();
}

function detectUSBStatus(){
	$j.ajax({
    		url: '/update_diskinfo.asp',
    		dataType: 'script',
    		error: function(xhr){
    			detectUSBStatus();
    		},
    		success: function(){
					return true;
  			}
  });
}

var link_status;
var link_auxstatus;
var link_sbstatus;
var usb_path1;
var usb_path2;
var usb_path1_tmp = "init";
var usb_path2_tmp = "init";
var usb_path1_removed;
var usb_path2_removed;
var usb_path1_removed_tmp = "init";
var usb_path2_removed_tmp = "init";

function refresh_info_status(xmldoc)
{
	var devicemapXML = xmldoc.getElementsByTagName("devicemap");
	var wanStatus = devicemapXML[0].getElementsByTagName("wan");
	link_status = wanStatus[0].firstChild.nodeValue;
	link_sbstatus = wanStatus[1].firstChild.nodeValue;
	link_auxstatus = wanStatus[2].firstChild.nodeValue;
	usb_path1 = wanStatus[3].firstChild.nodeValue;
	usb_path2 = wanStatus[4].firstChild.nodeValue;
	monoClient = wanStatus[5].firstChild.nodeValue;	
	cooler = wanStatus[6].firstChild.nodeValue;	
	_wlc_state = wanStatus[7].firstChild.nodeValue;	
	_wlc_sbstate = wanStatus[8].firstChild.nodeValue;	
	wifi_hw_switch = wanStatus[9].firstChild.nodeValue;
	usb_path1_removed = wanStatus[11].firstChild.nodeValue;	
	usb_path2_removed = wanStatus[12].firstChild.nodeValue;

	if(location.pathname == "/QIS_wizard.htm")
		return false;	

	// internet
	if(sw_mode == 1){
		if((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2")){
			$("connect_status").className = "connectstatuson";
			$("connect_status").onclick = function(){openHint(24,3);}
			if(location.pathname == "/" || location.pathname == "/index.asp")
				$("NM_connect_status").innerHTML = "<#Connected#>";
		}
		else{
			$("connect_status").className = "connectstatusoff";
			if(_wlc_sbstate == "wlc_sbstate=2")
				$("connect_status").onclick = function(){openHint(24,3);}
			else
				$("connect_status").onclick = function(){overHint(3);}

			if(location.pathname == "/" || location.pathname == "/index.asp"){
				$("NM_connect_status").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/QIS_wizard.htm?flag=detect"><#Disconnected#></a>';
				$("wanIP_div").style.display = "none";		
			}
		}
		$("connect_status").onmouseover = function(){overHint(3);}
		$("connect_status").onmouseout = function(){nd();}
	}
	else if(sw_mode == 2){
		if(psta_support != -1){
			if(wanStatus[10].firstChild.nodeValue == 'psta:wlc_state=1')
				_wlc_state = "wlc_state=2";
			else
				_wlc_state = "wlc_state=0";
		}

		if(_wlc_state == "wlc_state=2"){
			$("connect_status").className = "connectstatuson";
			$("connect_status").onclick = function(){openHint(24,3);}
			if(location.pathname == "/" || location.pathname == "/index.asp")
				$("NM_connect_status").innerHTML = "<#Connected#>";
		}
		else{
			$("connect_status").className = "connectstatusoff";
			if(location.pathname == "/" || location.pathname == "/index.asp")
			  $("NM_connect_status").innerHTML = "<#Disconnected#>";		 	
		}
		$("connect_status").onmouseover = function(){overHint(3);}
		$("connect_status").onmouseout = function(){nd();}
	}

	// wifi hw sw status
	if(wifi_hw_sw_support != -1){
		if(wifi_hw_switch == "wifi_hw_switch=0"){
			$("wifi_hw_sw_status").className = "wifihwswstatusoff";
			$("wifi_hw_sw_status").onclick = function(){}
			}
			else{
			$("wifi_hw_sw_status").className = "wifihwswstatuson";
			$("wifi_hw_sw_status").onclick = function(){}
				}
		$("wifi_hw_sw_status").onmouseover = function(){overHint(8);}
		$("wifi_hw_sw_status").onmouseout = function(){nd();}
	}	
	else	// No HW switch - reflect actual radio states
	{
		<%radio_status();%>

		if (band5g_support == -1)
			radio_5 = 1;

		if (radio_2 && radio_5)
		{
			$("wifi_hw_sw_status").className = "wifihwswstatuson";
			$("wifi_hw_sw_status").onclick = function(){}
		}
		else if (radio_2 || radio_5)
		{
			$("wifi_hw_sw_status").className = "wifihwswstatuspartial";  
			$("wifi_hw_sw_status").onclick = function(){}
		}
		else
		{
			$("wifi_hw_sw_status").className = "wifihwswstatusoff"; 
			$("wifi_hw_sw_status").onclick = function(){}
		}
		$("wifi_hw_sw_status").onmouseover = function(){overHint(8);}
		$("wifi_hw_sw_status").onmouseout = function(){nd();}
	}

	// usb
	if(usb_support != -1){
		if(current_url=="index.asp"||current_url==""){
			if((usb_path1_removed != usb_path1_removed_tmp && usb_path1_removed_tmp != "init")){
				location.href = "/index.asp";
			}
			else if(usb_path1_removed == "umount=0"){ // umount=0->umount=0, 0->storage
				if((usb_path1 != usb_path1_tmp && usb_path1_tmp != "init"))
					location.href = "/index.asp";
			}

			if((usb_path2_removed != usb_path2_removed_tmp && usb_path2_removed_tmp != "init")){
				location.href = "/index.asp";
			}
			else if(usb_path2_removed == "umount=0"){ // umount=0->umount=0, 0->storage
				if((usb_path2 != usb_path2_tmp && usb_path2_tmp != "init"))
					location.href = "/index.asp";
			}
		}

		if(usb_path1_removed == "umount=1")
			usb_path1 = "usb=";

		if(usb_path2_removed == "umount=1")
			usb_path2 = "usb=";

		if(usb_path1 == "usb=" && usb_path2 == "usb="){
			$("usb_status").className = "usbstatusoff";
			$("usb_status").onclick = function(){overHint(2);}
			if(printer_support != -1){
				$("printer_status").className = "printstatusoff";
				$("printer_status").onclick = function(){overHint(5);}
				$("printer_status").onmouseover = function(){overHint(5);}
				$("printer_status").onmouseout = function(){nd();}
			}
		}
		else{
			if(usb_path1 == "usb=printer" || usb_path2 == "usb=printer"){ // printer
				if((current_url == "index.asp" || current_url == "") && $("printerName0") == null && $("printerName1") == null)
					updateUSBStatus();
				if(printer_support != -1){
					$("printer_status").className = "printstatuson";
					$("printer_status").onmouseover = function(){overHint(6);}
					$("printer_status").onmouseout = function(){nd();}
					$("printer_status").onclick = function(){openHint(24,1);}
				}
				if(usb_path1 == "usb=" || usb_path2 == "usb=")
					$("usb_status").className = "usbstatusoff";			
				else
					$("usb_status").className = "usbstatuson";
			}
			else{ // !printer
				if((current_url == "index.asp" || current_url == "") && ($("printerName0") != null || $("printerName1") != null))
					location.href = "/index.asp";

				if(printer_support != -1){
					$("printer_status").className = "printstatusoff";
					$("printer_status").onmouseover = function(){overHint(5);}
					$("printer_status").onmouseout = function(){nd();}
				}

				$("usb_status").className = "usbstatuson";
			}
			$("usb_status").onclick = function(){openHint(24,2);}
		}
		$("usb_status").onmouseover = function(){overHint(2);}
		$("usb_status").onmouseout = function(){nd();}

		usb_path1_tmp = usb_path1;
		usb_path2_tmp = usb_path2;
		usb_path1_removed_tmp = usb_path1_removed;
		usb_path2_removed_tmp = usb_path2_removed;
	}

	// guest network
	if(multissid_support != -1 && gn_array_5g.length > 0){
		for(var i=0; i<gn_array_2g.length; i++){
			if(gn_array_2g[i][0] == 1 || gn_array_5g[i][0] == 1){
				$("guestnetwork_status").className = "guestnetworkstatuson";
				$("guestnetwork_status").onclick = function(){openHint(24,4);}
				break;
			}
			else{
				$("guestnetwork_status").className = "guestnetworkstatusoff";
				$("guestnetwork_status").onclick = function(){overHint(4);}
			}
		}
		$("guestnetwork_status").onmouseover = function(){overHint(4);}
		$("guestnetwork_status").onmouseout = function(){nd();}
	}
	else if(multissid_support != -1 && gn_array_5g.length == 0){
		for(var i=0; i<gn_array_2g.length; i++){
			if(gn_array_2g[i][0] == 1){
				$("guestnetwork_status").className = "guestnetworkstatuson";
				$("guestnetwork_status").onclick = function(){openHint(24,4);}
				break;
			}
			else{
				$("guestnetwork_status").className = "guestnetworkstatusoff";
			}
		}
		$("guestnetwork_status").onmouseover = function(){overHint(4);}
		$("guestnetwork_status").onmouseout = function(){nd();}
	}

	if(cooler_support != -1){
		if(cooler == "cooler=2"){
			$("cooler_status").className = "coolerstatusoff";
			$("cooler_status").onclick = function(){}
		}
		else{
			$("cooler_status").className = "coolerstatuson";
			$("cooler_status").onclick = function(){openHint(24,5);}
		}
		$("cooler_status").onmouseover = function(){overHint(7);}
		$("cooler_status").onmouseout = function(){nd();}
	}

	if(window.frames["statusframe"] && window.frames["statusframe"].stopFlag == 1 || stopFlag == 1){
		return 0;
	}

	setTimeout("updateStatus_AJAX();", 3000);
}

function db(obj){
	if(typeof console == 'object')
		console.log(obj);
}

function dbObj(obj){
	for(var j in obj){	
		if(j!="textContent" && j!="outerHTML" && j!="innerHTML" && j!="innerText" && j!="outerText")
			db(j+" : "+obj[j]);
	}
}

function FormActions(_Action, _ActionMode, _ActionScript, _ActionWait){
	if(_Action != "")
		document.form.action = _Action;
	if(_ActionMode != "")
		document.form.action_mode.value = _ActionMode;
	if(_ActionScript != "")
		document.form.action_script.value = _ActionScript;
	if(_ActionWait != "")
		document.form.action_wait.value = _ActionWait;
}

function change_wl_unit(){
	FormActions("apply.cgi", "change_wl_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function compareWirelessClient(target1, target2){
	if(target1.length != target2.length)
		return (target2.length-target1.length);
	
	for(var i = 0; i < target1.length; ++i)
		for(var j = 0; j < 3; ++j)
			if(target1[i][j] != target2[i][j])
					return 1;
	
	return 0;
}

function addNewScript(scriptName){
	var script = document.createElement("script");
	script.type = "text/javascript";
	script.src = scriptName;
	document.getElementsByTagName("head")[0].appendChild(script);
}

var cookie_help = {
	set: function(key, value, days) {
		document.cookie = key + '=' + value + '; expires=' +
			(new Date(new Date().getTime() + ((days ? days : 14) * 86400000))).toUTCString() + '; path=/';
	}
};

function getCookie_help(c_name){
	if (document.cookie.length > 0){ 
		c_start=document.cookie.indexOf(c_name + "=")
		if (c_start!=-1){ 
			c_start=c_start + c_name.length+1 
			c_end=document.cookie.indexOf(";",c_start)
			if (c_end==-1) c_end=document.cookie.length
			return unescape(document.cookie.substring(c_start,c_end))
		} 
	}
	return null;
}

function unload_body(){
}

function enableCheckChangedStatus(){
}

function disableCheckChangedStatus(){
	stopFlag = 1;
}

function check_if_support_dr_surf(){
}

function check_changed_status(){
}

function get_changed_status(){
}

function set_changed_status(){
}

function set_Dr_work(){
}

function showHelpofDrSurf(){
}

function showDrSurf(){
}

function slowHide(){
}

function hideHint(){
}

function drdiagnose(){
}

function isMobile(){
	var mobiles = new Array(
								"midp", "j2me", "avant", "docomo", "novarra", "palmos", "palmsource",
								"240x320", "opwv", "chtml", "pda", "windows ce", "mmp/",
								"blackberry", "mib/", "symbian", "wireless", "nokia", "hand", "mobi",
								"phone", "cdm", "up.b", "audio", "sie-", "sec-", "samsung", "htc",
								"mot-", "mitsu", "sagem", "sony", "alcatel", "lg", "eric", "vx",
								"NEC", "philips", "mmm", "xx", "panasonic", "sharp", "wap", "sch",
								"rover", "pocket", "benq", "java", "pt", "pg", "vox", "amoi",
								"bird", "compal", "kg", "voda", "sany", "kdd", "dbt", "sendo",
								"sgh", "gradi", "jb", "dddi", "moto", "iphone", "android",
								"iPod", "incognito", "webmate", "dream", "cupcake", "webos",
								"s8000", "bada", "googlebot-mobile");
 	var ua = navigator.userAgent.toLowerCase();
	var _isMobile = false;
	for(var i = 0; i < mobiles.length; i++){
		if(ua.indexOf(mobiles[i]) > 0){
			_isMobile = true;
			break;
		}
	}

	return _isMobile;
}

var stopAutoFocus;
function autoFocus(str){
	if(str == "")
		return false;

	stopAutoFocus = 0;
	if(document.form){
		for(var i = 0; i < document.form.length; i++){
			if(document.form[i].name == str){
				var sec = 600;
				var maxAF = 20;
				if(navigator.userAgent.toLowerCase().search("webkit") < 0){
					window.onclick = function(){stopAutoFocus=1;document.form[i].style.border='';}
					for(var j=0; j<maxAF; j++){
						setTimeout("if(stopAutoFocus==0)document.form["+i+"].style.border='1px solid #56B4EF';", sec*j++);
						setTimeout("if(stopAutoFocus==0)document.form["+i+"].style.border='';", sec*j);							
					}
				}
				else{
					window.onclick = function(){stopAutoFocus=1;}
					document.form[i].focus();
					for(var j=1; j<maxAF; j++){
						setTimeout("if(stopAutoFocus==0)document.form["+i+"].blur();", sec*j++);
						setTimeout("if(stopAutoFocus==0)document.form["+i+"].focus();", sec*j);
					}
				}
				break;
			}
		}
	}
}

function charToAscii(str){
	var retAscii = "";
	for(var i = 0; i < str.length; i++){
		retAscii += "%";
		retAscii += str.charCodeAt(i).toString(16).toUpperCase();
	}
	return retAscii;
}

function set_variable(_variable, _val){
	var NewInput = document.createElement("input");
	NewInput.type = "hidden";
	NewInput.name = _variable;
	NewInput.value = _val;
	document.form.appendChild(NewInput);
}
