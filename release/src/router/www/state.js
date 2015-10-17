document.write('<script type="text/javascript" src="/require/require.min.js"></script>');

/* String splice function */
String.prototype.splice = function( idx, rem, s ) {
    return (this.slice(0,idx) + s + this.slice(idx + Math.abs(rem)));
};

/* String repeat function */
String.prototype.repeat = function(times) {
   return (new Array(times + 1)).join(this);
};

/* Internet Explorer lacks this array method */
if (!('indexOf' in Array.prototype)) {
	Array.prototype.indexOf= function(find, i /*opt*/) {
		if (i===undefined) i= 0;
		if (i<0) i+= this.length;
		if (i<0) i= 0;
		for (var n= this.length; i<n; i++)
			if (i in this && this[i]===find)
				return i;
		return -1;
	};
}

/* add Array.prototype.forEach() in IE8 */
if(typeof Array.prototype.forEach != 'function'){
	Array.prototype.forEach = function(callback){
		for(var i = 0; i < this.length; i++){
			callback.apply(this, [this[i], i, this]);
		}
	};
}

String.prototype.toArray = function(){
	var ret = eval(this.toString());
	if(Object.prototype.toString.apply(ret) === '[object Array]')
		return ret;
	return [];
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

Array.prototype.del = function(n){
	if(n < 0)
		return this;
	else
		return this.slice(0,n).concat(this.slice(n+1,this.length));
}

// for compatibility jQuery trim on IE
String.prototype.trim = function() {
    return this.replace(/^\s+|\s+$/g, '');
}

var sw_mode = '<% nvram_get("sw_mode"); %>';
/*Media Bridge mode
Broadcom: sw_mode = 3, wlc_psta = 1
MTK/QCA: sw_mode = 2, wlc_psta = 1
*/ 
if((sw_mode == 2 || sw_mode == 3) && '<% nvram_get("wlc_psta"); %>' == 1)
	sw_mode = 4;
//for Broadcom New Repeater mode reference by Media Bridge mode
var new_repeater = false;	
if(sw_mode == 3 && '<% nvram_get("wlc_psta"); %>' == 2){
	sw_mode = 2;
	new_repeater = true;
}
var wlc_express = '<% nvram_get("wlc_express"); %>';
var isSwMode = function(mode){
	var ui_sw_mode = "rt";
	var sw_mode = '<% nvram_get("sw_mode"); %>';
	var wlc_psta = '<% nvram_get("wlc_psta"); %>';
	var wlc_express = '<% nvram_get("wlc_express"); %>';

	if(sw_mode == '2' && wlc_express == '0') ui_sw_mode = "re"; // Repeater
	else if(sw_mode == '3' && wlc_psta == '0') ui_sw_mode = "ap"; // AP
	else if(sw_mode == '3' && wlc_psta == '1') ui_sw_mode = "mb"; // MediaBridge
	else if(sw_mode == '2' && wlc_express != '0') ui_sw_mode = "ew"; // Express Way
	else if(sw_mode == '5') ui_sw_mode = 'hs'; // Hotspot
	else ui_sw_mode = "rt"; // Router

	return (ui_sw_mode == mode);
}

var productid = '<#Web_Title2#>';
var based_modelid = '<% nvram_get("productid"); %>';
var odmpid = '<% nvram_get("odmpid"); %>';
var hw_ver = '<% nvram_get("hardware_version"); %>';
var bl_version = '<% nvram_get("bl_version"); %>';
var uptimeStr = "<% uptime(); %>";
var timezone = uptimeStr.substring(26,31);
var boottime = parseInt(uptimeStr.substring(32,42));
var newformat_systime = uptimeStr.substring(8,11) + " " + uptimeStr.substring(5,7) + " " + uptimeStr.substring(17,25) + " " + uptimeStr.substring(12,16);  //Ex format: Jun 23 10:33:31 2008
var systime_millsec = Date.parse(newformat_systime); // millsec from system
var JS_timeObj = new Date(); // 1970.1.1
var test_page = 0;
var testEventID = "";
var httpd_dir = "/cifs1"
var isFromWAN = false;
var svc_ready = '<% nvram_get("svc_ready"); %>';
var qos_enable_flag = ('<% nvram_get("qos_enable"); %>' == 1) ? true : false;
var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
var qos_type_flag = "<% nvram_get("qos_type"); %>";

//territory_code sku
function in_territory_code(_ptn){
        return (ttc.search(_ptn) == -1) ? false : true;
}
var ttc = '<% nvram_get("territory_code"); %>';
var is_KR_sku = in_territory_code("KR");
var is_CN = in_territory_code("CN");

//wireless
var wl_nband_title = [];
var wl_nband_array = "<% wl_nband_info(); %>".toArray();
var band2g_count = 0;
var band5g_count = 0;
for (var j=0; j<wl_nband_array.length; j++) {
	if(wl_nband_array[j] == '2'){
		band2g_count++;
		wl_nband_title.push("2.4 GHz" + ((band2g_count > 1) ? ("-" + band2g_count) : ""));
	}
	else if(wl_nband_array[j] == '1'){
		band5g_count++;
		wl_nband_title.push("5 GHz" + ((band5g_count > 1) ? ("-" + band5g_count) : ""));
	}
}
if(wl_nband_title.indexOf("2.4 GHz-2") > 0) wl_nband_title[wl_nband_title.indexOf("2.4 GHz")] = "2.4 GHz-1";
if(wl_nband_title.indexOf("5 GHz-2") > 0) wl_nband_title[wl_nband_title.indexOf("5 GHz")] = "5 GHz-1";

var wl_info = {
	band2g_support:(function(){
				if(band2g_count > 0)
					return true;
				else
					return false;
			})(),
	band5g_support:(function(){
				if(band5g_count > 0)
					return true;
				else
					return false;
			})(),
	band5g_2_support:(function(){
				if(band5g_count == 2)
					return true;
				else
					return false;
			})(),
	band2g_total:band2g_count,
	band5g_total:band5g_count,
	wl_if_total:wl_nband_array.length
};
//wireless end

// parsing rc_support
var rc_support = '<% nvram_get("rc_support"); %>';
function isSupport(_ptn){
	if(_ptn == "rog"){
		var hasRogClient = false;
		decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<').forEach(function(element, index){
			if(element.split('>')[4] != "" && typeof element.split('>')[4] != "undefined" && rc_support.search(_ptn) != -1) hasRogClient = true;
		});
		return hasRogClient;
	}
	else if(_ptn == "mssid"){
		var wl_vifnames = '<% nvram_get("wl_vifnames"); %>';
		var multissid = rc_support.search("mssid");
		if(sw_mode == 2 || sw_mode == 4)
			multissid = -1;
		if(multissid != -1)
			multissid = wl_vifnames.split(" ").length;
		return multissid;
	}
	else if(_ptn == "aicloudipk"){
		//MODELDEP : DSL-N55U、DSL-N55U-B、RT-N56U
		if(based_modelid == "DSL-N55U" || based_modelid == "DSL-N55U-B" || based_modelid == "RT-N56U")
			return true;
		else
			return false;
	}
	else if(_ptn == "nwtool"){
		return true;
	}
	else if(_ptn == "11AC"){
		if(Rawifi_support || Qcawifi_support)
			return (rc_support.search(_ptn) == -1) ? false : true;
		else
			return ('<% nvram_get("wl_phytype"); %>' == 'v' ? true : false)
	}
	else if(_ptn == "wlopmode"){
		return ('<% nvram_get("wlopmode"); %>' == 7 ? true : false)
	}
	else if(_ptn == "wl_mfp"){
		return ('<% nvram_get("wl_mfp"); %>' == "" ? false: true)
	}
	else if(_ptn == "localap"){
		return (sw_mode == 4) ? false : true;
	}
	else if(_ptn == "concurrep"){
		return (based_modelid.search("RP-") != -1) ? true : false;
	}
	else
		return (rc_support.search(_ptn) == -1) ? false : true;
}

var igd2_support = isSupport("igd2");
var nfsd_support = isSupport("nfsd");
var wifilogo_support = isSupport("WIFI_LOGO"); 
var band2g_support = isSupport("2.4G"); 
var band5g_support = isSupport("5G");
var live_update_support = isSupport("update"); 
var cooler_support = isSupport("fanctrl");
var power_support = isSupport("pwrctrl");
var repeater_support = isSupport("repeater");
var concurrep_support = isSupport("concurrep");
var psta_support = isSupport("psta");
var wisp_support = isSupport("wisp");
var wl6_support = isSupport("wl6");
var Rawifi_support = isSupport("rawifi");
var Qcawifi_support = isSupport("qcawifi");
var wifi_logo_support = isSupport("wifilogo");
var SwitchCtrl_support = isSupport("switchctrl");
var dsl_support = isSupport("dsl");
var vdsl_support = isSupport("vdsl");
var dualWAN_support = isSupport("dualwan");
var ruisp_support = isSupport("ruisp");
var ssh_support = isSupport("ssh");
var snmp_support = isSupport("snmp");
var multissid_support = isSupport("mssid");
var no5gmssid_support = rc_support.search("no5gmssid");
var no5gmssid_support = isSupport("no5gmssid");
var wifi_hw_sw_support = isSupport("wifi_hw_sw");
var wifi_tog_btn_support = isSupport("wifi_tog_btn");
var default_psk_support = isSupport("defpsk");
var location_list_support = isSupport("loclist");
var cfg_wps_btn_support = isSupport("cfg_wps_btn");
var usb_support = isSupport("usbX");
var usbPortMax = rc_support.charAt(rc_support.indexOf("usbX")+4);
var printer_support = isSupport("printer"); 
var appbase_support = isSupport("appbase");
var appnet_support = isSupport("appnet");
var media_support = isSupport(" media");
var noiTunes_support = isSupport("noitunes");
var nomedia_support = isSupport("nomedia");
var noftp_support = isSupport("noftp");
var noaidisk_support = isSupport("noaidisk");
var cloudsync_support = isSupport("cloudsync");
var aicloudipk_support = isSupport("aicloudipk");
var yadns_hideqis = isSupport("yadns_hideqis");
var yadns_support = yadns_hideqis || isSupport("yadns");
var dnsfilter_support = isSupport("dnsfilter");
var manualstb_support = isSupport("manual_stb");
var wps_multiband_support = isSupport("wps_multiband");
var modem_support = isSupport("modem"); 
var IPv6_support = isSupport("ipv6"); 
var ParentalCtrl2_support = isSupport("PARENTAL2");
var pptpd_support = isSupport("pptpd"); 
var openvpnd_support = isSupport("openvpnd"); 
var vpnc_support = isSupport("vpnc"); 
var WebDav_support = isSupport("webdav"); 
var HTTPS_support = isSupport("HTTPS"); 
var nodm_support = isSupport("nodm"); 
var wimax_support = isSupport("wimax");
var downsize_4m_support = isSupport("sfp4m");
var downsize_8m_support = isSupport("sfp8m");
var hwmodeSwitch_support = isSupport("swmode_switch");
var diskUtility_support = isSupport("diskutility");
var networkTool_support = isSupport("nwtool");
var band5g_11ac_support = isSupport("11AC");
if("<% nvram_get("preferred_lang"); %>" == "UK" || based_modelid == "RT-N600")		//remove 80MHz(11ac) for UK , remove 80MHz(11ac) for MODELDEP: RT-N600
	band5g_11ac_support = false;
var optimizeXbox_support = isSupport("optimize_xbox");
var spectrum_support = isSupport("spectrum");
var mediareview_support = isSupport("wlopmode");var userRSSI_support = isSupport("user_low_rssi");
var timemachine_support = isSupport("timemachine");
var kyivstar_support = isSupport("kyivstar");
var email_support = isSupport("email");
var feedback_support = isSupport("feedback");
var swisscom_support = isSupport("swisscom");
var tmo_support = isSupport("tmo");
var wl_mfp_support = isSupport("wl_mfp");	// For Protected Management Frames, ARM platform
var bwdpi_support = isSupport("bwdpi");
var traffic_analyzer_support = bwdpi_support;

var adBlock_support = isSupport("adBlock");
var keyGuard_support = isSupport("keyGuard");
var rog_support = isSupport("rog");
var smart_connect_support = isSupport("smart_connect");
var rrsut_support = isSupport("rrsut");
var gobi_support = isSupport("gobi");
var findasus_support = isSupport("findasus");
var usericon_support = isSupport("usericon");
var localAP_support = isSupport("localap");
var ntfs_sparse_support = isSupport("sparse");
var tr069_support = isSupport("tr069");
var tor_support = isSupport("tor");
var stainfo_support = isSupport("stainfo");
var dhcp_override_support = isSupport("dhcp_override");
var wtfast_support = isSupport("wtfast") && !is_CN;
var powerline_support = isSupport("plc");
var reboot_schedule_support = isSupport("reboot_schedule");
var QISWIZARD = "QIS_wizard.htm";

var wl_version = "<% nvram_get("wl_version"); %>";
var sdk_version_array = new Array();
sdk_version_array = wl_version.split(".");
var sdk_7_up = (sdk_version_array[0] >= 7) ? true : false;

// Todo: Support repeater mode
/*if(isMobile() && sw_mode != 2 && !dsl_support)
	QISWIZARD = "MobileQIS_Login.asp";*/

//T-Mobile, force redirect to Mobile QIS page if client is mobile devices
if(tmo_support && isMobile()){	
	if(location.pathname != "/MobileQIS_Login.asp")
		location.href = "MobileQIS_Login.asp";
}
	
if(isMobile() && (based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || based_modelid == "RT-AC5300"))
	QISWIZARD = "QIS_wizard_m.htm";	
// for detect if the status of the machine is changed. {
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var stopFlag = 0;

//isFromWAN
if(tmo_support && (location.hostname.search('<% nvram_get("lan_ipaddr"); %>') == -1) && (location.hostname.search('cellspot.router') == -1)){
        isFromWAN = true;
}
else if(!tmo_support && (location.hostname.search('<% nvram_get("lan_ipaddr"); %>') == -1) && (location.hostname.search('router.asus') == -1)){
        isFromWAN = true;
}

var gn_array_2g = <% wl_get_guestnetwork("0"); %>;
var gn_array_5g = <% wl_get_guestnetwork("1"); %>;
var gn_array_5g_2 = <% wl_get_guestnetwork("2"); %>;

//notification value
var notice_acpw_is_default = '<% check_acpw(); %>';
var noti_auth_mode_2g = '<% nvram_get("wl0_auth_mode_x"); %>';
var noti_auth_mode_5g = '<% nvram_get("wl1_auth_mode_x"); %>';
var st_ftp_mode = '<% nvram_get("st_ftp_mode"); %>';
var st_ftp_force_mode = '<% nvram_get("st_ftp_force_mode"); %>';
var st_samba_mode = '<% nvram_get("st_samba_mode"); %>';
var st_samba_force_mode = '<% nvram_get("st_samba_force_mode"); %>';
var enable_samba = '<% nvram_get("enable_samba"); %>';
var enable_ftp = '<% nvram_get("enable_ftp"); %>';
// MODELDEP : DSL-AC68U Only for now
if(based_modelid == "DSL-AC68U"){
	var dla_modified = (vdsl_support == false) ? "0" :'<% nvram_get("dsltmp_dla_modified"); %>';	
	var dsl_loss_sync = "";
	if(dla_modified == "1")
		dsl_loss_sync = "1";
	else
		dsl_loss_sync = (vdsl_support == false) ? "0" :'<% nvram_get("dsltmp_syncloss"); %>';	
	var experience_fb = (dsl_support == false) ? "2" : '<% nvram_get("fb_experience"); %>';
}
else{
	var dla_modified = "0";
        var dsl_loss_sync = "0";
        var experience_fb = "2";
}
if(dsl_support){
	var noti_notif_Flag = '<% nvram_get("webs_notif_flag"); %>';
	var notif_hint_index = '<% nvram_get("webs_notif_index"); %>';
	var notif_hint_info = '<% nvram_get("webs_notif_info"); %>';    
	var notif_hint_infomation = notif_hint_info;
	if(notif_hint_infomation.charAt(0) == "+")      //remove start with '++'
		notif_hint_infomation = notif_hint_infomation.substring(2, notif_hint_infomation.length);
	var notif_msg = "";             
	var notif_hint_array = notif_hint_infomation.split("++");
	if(notif_hint_array[0] != ""){ 
		notif_msg = "<ol style=\"margin-left:-20px;*margin-left:20px;\">";
		for(var i=0; i<notif_hint_array.length; i++){
			if(i==0)
				notif_msg += "<li>"+notif_hint_array[i];
			else
				notif_msg += "<div><img src=\"/images/New_ui/export/line_export_notif.png\" style=\"margin-top:2px;margin-bottom:2px;margin-left:-20px;*margin-left:-20px;\"></div><li>"+notif_hint_array[i];
		}
		notif_msg += "</ol>";
    	}	
}  

var allUsbStatus = "";
var allUsbStatusTmp = "";
var allUsbStatusArray = '<% show_usb_path(); %>'.toArray();


var wan_line_state = "<% nvram_get("dsltmp_adslsyncsts"); %>";
var wan_diag_state = "<% nvram_get("dslx_diag_state"); %>";
var wlan0_radio_flag = "<% nvram_get("wl0_radio"); %>";
var wlan1_radio_flag = "<% nvram_get("wl1_radio"); %>";

//for high power model
var auto_channel = '<% nvram_get("AUTO_CHANNEL"); %>';
var is_high_power = auto_channel ? true : false;

function change_wl_unit_status(_unit){
	if(sw_mode == 2 || sw_mode == 4) return false;
	
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

var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_dualwan_array = new Array();
wans_dualwan_array = wans_dualwan_orig.split(" ");
var usb_index = wans_dualwan_array.getIndexByValue("usb");
var active_wan_unit = '<% get_wan_unit(); %>';
var wan0_enable = '<% nvram_get("wan0_enable"); %>';
var wan1_enable = '<% nvram_get("wan1_enable"); %>';
var dualwan_enabled = (wans_dualwan_orig.search("none") == -1) ? 1:0;

var realip_support = isSupport("realip");
var realip_state = "";
var realip_ip = "";
var external_ip = 0;

var banner_code, menu_code="", menu1_code="", menu2_code="", tab_code="", footer_code;
function show_banner(L3){// L3 = The third Level of Menu
	var banner_code = "";

	// creat a hidden iframe to cache offline page
	//banner_code +='<iframe width="0" height="0" frameborder="0" scrolling="no" src="/manifest.asp"></iframe>';

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
	banner_code +='<input type="hidden" name="wan_unit" value="">\n';
	if(gobi_support && (usb_index != -1) && (sim_state != "")){
		banner_code +='<input type="hidden" name="sim_order" value="">\n';
	}
	banner_code +='</form>\n';

	banner_code +='<form method="post" name="diskForm_title" action="/device-map/safely_remove_disk.asp" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="disk" value="">\n';
	banner_code +='</form>\n';
	
	banner_code +='<form method="post" name="noti_ftp" action="/aidisk/switch_share_mode.asp" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="protocol" value="ftp">\n';
	banner_code +='<input type="hidden" name="mode" value="account">\n';
	banner_code +='</form>\n';
	
	banner_code +='<form method="post" name="noti_samba" action="/aidisk/switch_share_mode.asp" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="protocol" value="cifs">\n';
	banner_code +='<input type="hidden" name="mode" value="account">\n';
	banner_code +='</form>\n';
	
	banner_code +='<form method="post" name="noti_experience_Feedback" action="/start_apply.htm" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="next_page" value="">\n';
	banner_code +='<input type="hidden" name="current_page" value="">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="">\n';
	banner_code +='<input type="hidden" name="action_wait" value="">\n';	
	banner_code +='<input type="hidden" name="fb_experience" value="1">\n';
	banner_code +='</form>\n';

	banner_code +='<form method="post" name="noti_notif_hint" action="/start_apply.htm" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="next_page" value="">\n';
	banner_code +='<input type="hidden" name="current_page" value="">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="">\n';
	banner_code +='<input type="hidden" name="action_wait" value="">\n';    
	banner_code +='<input type="hidden" name="noti_notif_Flag" value="0">\n';
	banner_code +='</form>\n'; 

	banner_code +='<form method="post" name="internetForm_title" action="/start_apply2.htm" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="current_page" value="/index.asp">\n';
	banner_code +='<input type="hidden" name="next_page" value="/index.asp">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="restart_wan_if">\n';
	banner_code +='<input type="hidden" name="action_wait" value="5">\n';
	banner_code +='<input type="hidden" name="wan_enable" value="<% nvram_get("wan_enable"); %>">\n';
	banner_code +='<input type="hidden" name="wan_unit" value="<% get_wan_unit(); %>">\n';
	banner_code +='<input type="hidden" name="modem_enable" value="<% nvram_get("modem_enable"); %>">\n';
	banner_code +='</form>\n';

	banner_code +='<form method="post" name="rebootForm" action="apply.cgi" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="action_mode" value="reboot">\n';
	banner_code +='<input type="hidden" name="action_script" value="">\n';
	banner_code +='<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">\n';
	banner_code +='</form>\n';
	
	banner_code +='<form method="post" name="canceldiagForm" action="apply.cgi" target="hidden_frame">\n';
	banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
	banner_code +='<input type="hidden" name="action_script" value="stop_dsl_diag">\n';
	banner_code +='<input type="hidden" name="action_wait" value="">\n';	
	banner_code +='</form>\n';
	
	if(bwdpi_support){
		banner_code +='<form method="post" name="qosDisableForm" action="/start_apply.htm" target="hidden_frame">\n';
		banner_code +='<input type="hidden" name="current_pgae" value="QoS_EZQoS.asp">\n';
		banner_code +='<input type="hidden" name="next_page" value="QoS_EZQoS.asp">\n';
		banner_code +='<input type="hidden" name="action_mode" value="apply">\n';
		banner_code +='<input type="hidden" name="action_script" value="reboot">\n';
		banner_code +='<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">\n';	
		banner_code +='<input type="hidden" name="qos_enable" value="0">\n';
		banner_code +='</form>\n';
	}	

	banner_code +='<div id="banner1" class="banner1" align="center"><img src="images/New_ui/asustitle.png" width="218" height="54" align="left">\n';
	banner_code +='<div style="margin-top:13px;margin-left:-90px;*margin-top:0px;*margin-left:0px;" align="center"><span id="modelName_top" onclick="this.focus();" class="modelName_top"><#Web_Title2#></span></div>';
	banner_code +='<div style="margin-left:25px;width:160px;margin-top:0px;float:left;" align="left"><span><a href="http://asuswrt.lostrealm.ca/" target="_blank"><img src="images/merlin-logo.png" style="border: 0;"></span></div>';

	// logout, reboot
	banner_code +='<a href="javascript:logout();"><div style="margin-top:13px;margin-left:25px; *width:136px;" class="titlebtn" align="center"><span><#t1Logout#></span></div></a>\n';
	banner_code +='<a href="javascript:reboot();"><div style="margin-top:13px;margin-left:0px;*width:136px;" class="titlebtn" align="center"><span><#BTN_REBOOT#></span></div></a>\n';

	// language
	banner_code +='<ul class="navigation">';
	banner_code +='<% shown_language_css(); %>';
	banner_code +='</ul>';

	banner_code +='</div>\n';
	banner_code +='<table width="998" border="0" align="center" cellpadding="0" cellspacing="0" class="statusBar">\n';
	banner_code +='<tr>\n';
	banner_code +='<td background="images/New_ui/midup_bg.png" height="179" valign="top"><table width="764" border="0" cellpadding="0" cellspacing="0" height="35px" style="margin-left:230px;">\n';
	banner_code +='<tbody><tr>\n';
 	banner_code +='<td valign="center" class="titledown" width="auto">';

	// dsl does not support operation mode
	if (!dsl_support) {
		banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;"><#menu5_6_1_title#>:</span><span class="title_link" style="text-decoration: none;" id="op_link"><a href="/Advanced_OperationMode_Content.asp" style="color:white"><span id="sw_mode_span" style="text-decoration: underline;"></span></a></span>\n';
	}
	banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;">Firmware:</span><a href="/Advanced_FirmwareUpgrade_Content.asp" style="color:white;"><span id="firmver" class="title_link"></span></a>\n';
	banner_code +='<span style="font-family:Verdana, Arial, Helvetica, sans-serif;" id="ssidTitle">SSID:</span>';
	banner_code +='<span onclick="change_wl_unit_status(0)" id="elliptic_ssid_2g" class="title_link"></span>';
	banner_code +='<span onclick="change_wl_unit_status(1)" id="elliptic_ssid_5g" class="title_link"></span>\n';
	if(wl_info.band5g_2_support)
		banner_code +='<span onclick="change_wl_unit_status(2)" id="elliptic_ssid_5g_2" class="title_link"></span>\n';
	banner_code +='</td>\n';

	banner_code +='<td width="30" id="notification_status1" class="notificationOn"><div id="notification_status" class="notificationOn"></div><div id="notification_desc" class=""></div></td>\n';

	if(bwdpi_support && qos_enable_flag && qos_type_flag == "1")
		banner_code +='<td width="30"><div id="bwdpi_status" class=""></div></td>\n';	

//	if(wifi_hw_sw_support)
		banner_code +='<td width="30"><div id="wifi_hw_sw_status" class="wifihwswstatusoff"></div></td>\n';

	if(cooler_support)
		banner_code +='<td width="30"><div id="cooler_status" class="" style="display:none;"></div></td>\n';

	if(multissid_support != -1)
		banner_code +='<td width="30"><div id="guestnetwork_status" class="guestnetworkstatusoff"></div></td>\n';

	if(dsl_support)
		banner_code +='<td width="30"><div id="adsl_line_status" class="linestatusdown"></div></td>\n';

	if(sw_mode != 3)
		banner_code +='<td width="30"><div id="connect_status" class="connectstatusoff"></div></td>\n';

	if(usb_support)
		banner_code +='<td width="30"><div id="usb_status"></div></td>\n';
	
	if(printer_support)
		banner_code +='<td width="30" style="display:none"><div id="printer_status" class="printstatusoff"></div></td>\n';

	/* Cherry Cho added in 2014/8/22. */
	if(((modem_support && hadPlugged("modem")) || gobi_support) && (usb_index != -1) && (sim_state != "")){	
		banner_code +='<td width="30"><div id="sim_status" class="simnone"></div></td>\n';
	}	

	if(gobi_support && (usb_index != -1) && (sim_state != "")){
		banner_code +='<td width="30"><div id="simsignal" class="simsignalno"><div class="img_wrap"><div id="signalsys" class="signalsysimg"></div></div></div></td>\n';
		if(roaming == "1")
			banner_code +='<td width="30"><div id="simroaming_status" class="simroamingoff"></div></td>\n';		
	}
	
	banner_code +='<td width="17"></td>\n';
	banner_code +='</tr></tbody></table></td></tr></table>\n';

	/* Traffic Limit Warning*/
	var setCookie = 0;
	traffic_warning_cookie = cookie.get(keystr);
	if(traffic_warning_cookie == null){
		setCookie = 1;
	}
	else{
		var cookie_year = traffic_warning_cookie.substring(0,4);
		var indexOfcolon = traffic_warning_cookie.indexOf(':');
		var cookie_month = traffic_warning_cookie.substring(5, indexOfcolon);

		if(cookie_year == date_year && cookie_month == date_month)
			traffic_warning_flag = parseInt(traffic_warning_cookie.substring(indexOfcolon + 1));
		else
			setCookie = 1;
	}

	if(setCookie){
		set_traffic_show("1");
	}

	if(gobi_support && (usb_index != -1) && (sim_state != "")){
		banner_code +='<div id="mobile_traffic_warning" class="traffic_warning" style="display: none; opacity:0;">\n';
		banner_code +='<div style="text-align:right"><img src="/images/button-close.gif" style="width:30px;cursor:pointer" onclick="slow_hide_warning();"></div>';
		banner_code +='<div style="margin:0px 30px 10px;">';
		banner_code += '<#Mobile_limit_warning#>';
		banner_code +='</div>';
		banner_code +='<span><input style="margin-left:30px;" type="checkbox" name="stop_show_chk" id="stop_show_chk"><#Mobile_stop_warning#></input></span>';
		banner_code +='<div style="margin-top:20px;padding-bottom:10px;width:100%;text-align:center;">';
		banner_code +='<input id="changeButton" class="button_gen" type="button" value="Change" onclick="setTrafficLimit();">';
		banner_code +='</div>';
		banner_code +='</div>';

		var show_flag = '<% get_parameter("show"); %>';
		if(show_flag != "0" && traffic_warning_flag && (modem_bytes_data_limit > 0)){
			var data_usage = rx_bytes + tx_bytes;
			if( data_usage >= modem_bytes_data_limit)
				setTimeout("show_traffic_warning();", 600);
		}
	}

	document.getElementById("TopBanner").innerHTML = banner_code;

	show_loading_obj();
	show_top_status();

	updateStatus();

}
function set_traffic_show(flag){ //0:hide 1:show
	traffic_warning_cookie = date_year + '.' + date_month + ':' + flag;
	cookie.set(keystr, traffic_warning_cookie, 1000);
	traffic_warning_flag = parseInt(flag);
}

var opacity = 0;
var inc = 1/50;
function slow_show_warning(){
	document.getElementById("mobile_traffic_warning").style.display = "";
	opacity = opacity + inc;
	document.getElementById("mobile_traffic_warning").style.opacity = opacity;
	if(document.getElementById("mobile_traffic_warning").style.opacity < 1)
		setTimeout("slow_show_warning();", 1);
}

function slow_hide_warning(){
	document.getElementById("mobile_traffic_warning").style.display = "none";
	opacity = document.getElementById("mobile_traffic_warning").style.opacity;
	if(opacity == 1 && document.getElementById("stop_show_chk").checked == true){
		set_traffic_show("0");
	}

	opacity = opacity - inc;
	document.getElementById("mobile_traffic_warning").style.opacity = opacity;
	if(document.getElementById("mobile_traffic_warning").style.opacity > 0)
		setTimeout("slow_hide_warning();", 1);
}

var clickListener = function(event){
	var traffic_waring_element = document.getElementById("mobile_traffic_warning");

	if(event.target.id != 'mobile_traffic_warning' && !traffic_waring_element.contains(event.target))
    	hide_traffic_warning();
};

function show_traffic_warning(){
	var statusframe= document.getElementById("statusframe");
	var statusframe_content;

	slow_show_warning();
	document.addEventListener('click', clickListener, false);
	if(statusframe){
		statusframe_content = statusframe.contentWindow.document;
		statusframe_content.addEventListener('click', clickListener, false);
	}
}

function hide_traffic_warning(){
	var statusframe= document.getElementById("statusframe");
	var statusframe_content;

	slow_hide_warning();
	// disable event listener
	document.removeEventListener('click', clickListener, false);
	if(statusframe){
		statusframe_content = statusframe.contentWindow.document;
		statusframe_content.removeEventListener('click', clickListener, false);
	}

}


// Level 3 Tab
var tabtitle = new Array();
tabtitle[0] = new Array("", "<#menu5_1_1#>", "<#menu5_1_2#>", "WDS", "<#menu5_1_4#>", "<#menu5_1_5#>", "<#menu5_1_6#>", "Site Survey");
tabtitle[1] = new Array("", "Passpoint");
tabtitle[2] = new Array("", "<#menu5_2_1#>", "<#menu5_2_2#>", "<#menu5_2_3#>", "IPTV", "<#Switch_itemname#>");
tabtitle[3] = new Array("", "<#menu5_3_1#>", "<#dualwan#>", "<#menu5_3_3#>", "<#menu5_3_4#>", "<#menu5_3_5#>", "<#menu5_3_6#>", "<#NAT_passthrough_itemname#>", "<#menu5_4_4#>");
tabtitle[4] = new Array("", "<#UPnPMediaServer#>", "<#menu5_4_1#>", "NFS Exports" , "<#menu5_4_2#>");
tabtitle[5] = new Array("", "IPv6");
tabtitle[6] = new Array("", "VPN Status", "PPTP Server", "OpenVPN Servers", "PPTP/L2TP Client", "OpenVPN Clients", "TOR");
tabtitle[7] = new Array("", "<#menu5_1_1#>", "<#menu5_5_2#>", "<#menu5_5_5#>", "<#menu5_5_4#>", "<#menu5_5_6#>");
tabtitle[8] = new Array("", "<#menu5_6_1#>", "<#menu5_6_2#>", "<#menu5_6_3#>", "<#menu5_6_4#>", "Performance tuning", "<#menu_dsl_setting#>", "<#menu_feedback#>", "SNMP", "TR-069");
tabtitle[9] = new Array("", "<#menu5_7_2#>", "<#menu5_7_4#>", "<#menu5_7_3#>", "IPv6", "<#menu5_7_6#>", "<#menu5_7_5#>", "<#menu_dsl_log#>", "<#Connections#>");
tabtitle[10] = new Array("", "<#Network_Analysis#>", "Netstat", "<#NetworkTools_WOL#>", "SMTP Client", "<#smart_connect_rule#>");
if(bwdpi_support){
	tabtitle[11] = new Array("", "<#Bandwidth_monitor#>", "<#menu5_3_2#>", "<#Adaptive_History#>", "<table style='margin-top:-10px \\9;'><tr><td><img src='/images/ROG_Logo.png' style='border:0px;width:32px;'></td><td>ROG First</td></tr></table>");
	tabtitle[12] = new Array("", "<#AiProtection_Home#>", "<#Parental_Control#>", "Ad Blocking", "Key Guard", "DNS Filtering");
	tabtitle[13] = new Array("", "Time Limits", "<#AiProtection_filter#>");
	tabtitle[14] = new Array("", "Traffic Monitor", "Statistic");
}
else{
	tabtitle[11] = new Array("", "<#menu5_3_2#>", "<#traffic_monitor#>", "<table style='margin-top:-10px \\9;'><tr><td><img src='/images/ROG_Logo.png' style='border:0px;width:32px;'></td><td>ROG First</td></tr></table>", "Spectrum");
	tabtitle[12] = new Array("", "<#Parental_Control#>", "<#YandexDNS#>", "DNS Filtering");
	tabtitle[13] = new Array("");
	tabtitle[14] = new Array("");
}
tabtitle[15] = new Array("", "Sysinfo", "Other Settings");

var tablink = new Array();
tablink[0] = new Array("", "Advanced_Wireless_Content.asp", "Advanced_WWPS_Content.asp", "Advanced_WMode_Content.asp", "Advanced_ACL_Content.asp", "Advanced_WSecurity_Content.asp", "Advanced_WAdvanced_Content.asp", "Advanced_Wireless_Survey.asp");
tablink[1] = new Array("", "Advanced_WPasspoint_Content.asp");
tablink[2] = new Array("", "Advanced_LAN_Content.asp", "Advanced_DHCP_Content.asp", "Advanced_GWStaticRoute_Content.asp", "Advanced_IPTV_Content.asp", "Advanced_SwitchCtrl_Content.asp");
tablink[3] = new Array("", "Advanced_WAN_Content.asp", "Advanced_WANPort_Content.asp", "Advanced_PortTrigger_Content.asp", "Advanced_VirtualServer_Content.asp", "Advanced_Exposed_Content.asp", "Advanced_ASUSDDNS_Content.asp", "Advanced_NATPassThrough_Content.asp", "Advanced_Modem_Content.asp");
tablink[4] = new Array("", "mediaserver.asp", "Advanced_AiDisk_samba.asp", "Advanced_AiDisk_NFS.asp", "Advanced_AiDisk_ftp.asp");
tablink[5] = new Array("", "Advanced_IPv6_Content.asp");
tablink[6] = new Array("", "Advanced_VPNStatus.asp", "Advanced_VPN_PPTP.asp", "Advanced_VPN_OpenVPN.asp", "Advanced_VPNClient_Content.asp", "Advanced_OpenVPNClient_Content.asp", "Advanced_TOR_Content.asp");
tablink[7] = new Array("", "Advanced_BasicFirewall_Content.asp", "Advanced_URLFilter_Content.asp", "Advanced_KeywordFilter_Content.asp", "Advanced_Firewall_Content.asp", "Advanced_Firewall_IPv6_Content.asp");
tablink[8] = new Array("", "Advanced_OperationMode_Content.asp", "Advanced_System_Content.asp", "Advanced_FirmwareUpgrade_Content.asp", "Advanced_SettingBackup_Content.asp", "Advanced_PerformanceTuning_Content.asp", "Advanced_ADSL_Content.asp", "Advanced_Feedback.asp", "Advanced_SNMP_Content.asp", "Advanced_TR069_Content.asp");
tablink[9] = new Array("", "Main_LogStatus_Content.asp", "Main_WStatus_Content.asp", "Main_DHCPStatus_Content.asp", "Main_IPV6Status_Content.asp", "Main_RouteStatus_Content.asp", "Main_IPTStatus_Content.asp", "Main_AdslStatus_Content.asp", "Main_ConnStatus_Content.asp");
tablink[10] = new Array("", "Main_Analysis_Content.asp", "Main_Netstat_Content.asp", "Main_WOL_Content.asp", "SMTP_Client.asp", "Advanced_Smart_Connect.asp");
if(bwdpi_support){
	tablink[11] = new Array("", "AdaptiveQoS_Bandwidth_Monitor.asp", "QoS_EZQoS.asp", "AdaptiveQoS_WebHistory.asp", "AdaptiveQoS_ROG.asp", "Main_Spectrum_Content.asp", "Advanced_QOSUserPrio_Content.asp", "Advanced_QOSUserRules_Content.asp", "AdaptiveQoS_Adaptive.asp", "Bandwidth_Limiter.asp");
	tablink[12] = new Array("", "AiProtection_HomeProtection.asp", "AiProtection_WebProtector.asp", "AiProtection_AdBlock.asp", "AiProtection_Key_Guard.asp", "DNSFilter.asp");
	tablink[13] = new Array("");
	tablink[14] = new Array("", "Main_TrafficMonitor_realtime.asp", "TrafficAnalyzer_Statistic.asp", "Main_TrafficMonitor_last24.asp", "Main_TrafficMonitor_daily.asp", "Main_TrafficMonitor_monthly.asp", "Main_TrafficMonitor_devrealtime.asp", "Main_TrafficMonitor_devdaily.asp", "Main_TrafficMonitor_devmonthly.asp");
}else{
	tablink[11] = new Array("", "QoS_EZQoS.asp", "Main_TrafficMonitor_realtime.asp", "AdaptiveQoS_ROG.asp", "Main_Spectrum_Content.asp", "Main_TrafficMonitor_last24.asp", "Main_TrafficMonitor_daily.asp", "Main_TrafficMonitor_monthly.asp", "Main_TrafficMonitor_devrealtime.asp", "Main_TrafficMonitor_devdaily.asp", "Main_TrafficMonitor_devmonthly.asp", "Advanced_QOSUserPrio_Content.asp", "Advanced_QOSUserRules_Content.asp", "Bandwidth_Limiter.asp");
	tablink[12] = new Array("", "ParentalControl.asp", "YandexDNS.asp", "DNSFilter.asp");
	tablink[13] = new Array("");
	tablink[14] = new Array("");
}
tablink[15] = new Array("", "Tools_Sysinfo.asp", "Tools_OtherSettings.asp");

// Level 2 Menu
menuL2_title = new Array("", "<#menu5_1#>", "Passpoint", "<#menu5_2#>", "<#menu5_3#>", "<#menu5_4#>", "IPv6", "VPN", "<#menu5_5#>", "<#menu5_6#>", "<#System_Log#>", "<#Network_Tools#>");
menuL2_link  = new Array("", tablink[0][1], tablink[1][1], tablink[2][1], tablink[3][1], tablink[4][1], tablink[5][1], tablink[6][1], tablink[7][1], tablink[8][1], tablink[9][1], tablink[10][1]);

 // Level 1 Menu
var AiProtection_title_add_br = "<#AiProtection_title#>";	//AiProtection		13
var Adaptive_QoS_add_br = "<#Adaptive_QoS#>";			//Adaptive QoS		13
var Traffic_Analyzer_add_br = "<#Traffic_Analyzer#>";		//Traffic Analyzer	17
var AiCloud_Title_add_br = "<#AiCloud_Title#>";			//AiCloud 2.0		12
if("<% nvram_get("preferred_lang"); %>" == "CN" || "<% nvram_get("preferred_lang"); %>" == "TW"){
	AiProtection_title_add_br = AiProtection_title_add_br.splice(13,0,"<br>");
	Adaptive_QoS_add_br = Adaptive_QoS_add_br.splice(13,0,"<br>");
	Traffic_Analyzer_add_br = Traffic_Analyzer_add_br.splice(17,0,"<br>");
	AiCloud_Title_add_br = AiCloud_Title_add_br.splice(12,0,"<br>");
}

if(bwdpi_support){	
	if(traffic_analyzer_support){	/* untranslated #Traffic_Analyzer# */
		if(wtfast_support){
			menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", AiProtection_title_add_br, Adaptive_QoS_add_br, "Game Boost", "<#Traffic_Analyzer#>" ,"<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
			menuL1_link = new Array("", "index.asp", "Guest_network.asp", "AiProtection_HomeSecurity.asp", "AdaptiveQoS_Bandwidth_Monitor.asp", "Advanced_WTFast_Content.asp", "Main_TrafficMonitor_realtime.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
		}
		else{
			menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", AiProtection_title_add_br, Adaptive_QoS_add_br, "<#Traffic_Analyzer#>" ,"<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
			menuL1_link = new Array("", "index.asp", "Guest_network.asp", "AiProtection_HomeSecurity.asp", "AdaptiveQoS_Bandwidth_Monitor.asp", "Main_TrafficMonitor_realtime.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
		}
	}
	else{
		if(wtfast_support){		
			menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", AiProtection_title_add_br, Adaptive_QoS_add_br, "Game Boost", "<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
			menuL1_link = new Array("", "index.asp", "Guest_network.asp", "AiProtection_HomeSecurity.asp", "AdaptiveQoS_Bandwidth_Monitor.asp", "Advanced_WTFast_Content.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
		}
		else{
			menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", AiProtection_title_add_br, Adaptive_QoS_add_br, "<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
			menuL1_link = new Array("", "index.asp", "Guest_network.asp", "AiProtection_HomeSecurity.asp", "AdaptiveQoS_Bandwidth_Monitor.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
		}
	}
}
else{
	if(wtfast_support){	
		menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", "<#Menu_TrafficManager#>", "<#Parental_Control#>", "Game Boost", "<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
		menuL1_link = new Array("", "index.asp", "Guest_network.asp", "QoS_EZQoS.asp", "ParentalControl.asp", "Advanced_WTFast_Content.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
	}
	else{
		menuL1_title = new Array("", "<#menu1#>", "<#Guest_Network#>", "<#Menu_TrafficManager#>", "<#Parental_Control#>", "<#Menu_usb_application#>", AiCloud_Title_add_br, "Tools", "<#menu5#>");
		menuL1_link = new Array("", "index.asp", "Guest_network.asp", "QoS_EZQoS.asp", "ParentalControl.asp", "APP_Installation.asp", "cloud_main.asp", "Tools_Sysinfo.asp", "");
	}	
}

var calculate_height = menuL1_link.length + menuL2_link.length - 1;

if(bwdpi_support){
	var traffic_L1_dx = 4;
	var tools_L1 = 8;
}
else{
	var traffic_L1_dx = 3;
	var tools_L1 = 7;
}

var traffic_L2_dx = 12;

if(wtfast_support){
	tools_L1++;
}

function remove_url(){
	remove_menu_item("Advanced_Modem_Content.asp");
	remove_menu_item("AiProtection_Group.asp");		//hide temporary for phrase 1 ASUSWRT 1.5, Jieming added at 2014/05/07
	
	if('<% nvram_get("start_aicloud"); %>' == '0')
		menuL1_link[6] = "cloud__main.asp"

	if(!networkTool_support){
		menuL2_title[11] = "";
		menuL2_link[11] = "";
	}

	if(!tmo_support) {
		menuL2_title[2]="";
		menuL2_link[2]="";
		remove_menu_item("SMTP_Client.asp");	
	}

	if(downsize_4m_support) {
		remove_menu_item("Main_ConnStatus_Content.asp");
		remove_menu_item("Main_TrafficMonitor_realtime.asp");		
	}
	
	if(downsize_8m_support) {
		//remove_menu_item("Main_ConnStatus_Content.asp");
	}	

	if(!feedback_support) {
		remove_menu_item("Advanced_Feedback.asp");
	}

	if(!dsl_support) {
		remove_menu_item("Advanced_ADSL_Content.asp");
		remove_menu_item("Main_AdslStatus_Content.asp");
		remove_menu_item("Main_Spectrum_Content.asp");
	}
	else {
		//no_op_mode
		remove_menu_item(7, "Advanced_OperationMode_Content.asp");
		
		if(!spectrum_support)		// not to support Spectrum page.
			remove_menu_item("Main_Spectrum_Content.asp");
	}

	if(hwmodeSwitch_support){
		remove_menu_item("Advanced_OperationMode_Content.asp");		
	}

	if(WebDav_support) {
		tabtitle[4][2] = "<#menu5_4_1#> / Cloud Disk";
	}
	
	if(!cloudsync_support && !aicloudipk_support){
		menuL1_title[6] = "";
		menuL1_link[6] = "";
	}

	if(based_modelid == "RT-N10U" || based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || based_modelid == "RT-AC5300"){	//MODELDEP
		remove_menu_item("Advanced_WMode_Content.asp");
	}
	
	if(based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1')	//MODELDEP	
		remove_menu_item("Advanced_WSecurity_Content.asp");

	if(based_modelid == "RT-N300"){  //MODELDEP
		remove_menu_item("Advanced_WMode_Content.asp");
		remove_menu_item("Advanced_IPTV_Content.asp");
		menuL2_title[7]="";
		menuL2_link[7]="";	
	}

	if(sw_mode == 2 || sw_mode == 4){
		// Guest Network
		menuL1_title[2] ="";
		menuL1_link[2] ="";

		if(bwdpi_support){
			menuL1_title[3] ="";            //Parental Control
			menuL1_link[3] ="";
			menuL1_title[4] ="";            //QoS
			menuL1_link[4] ="";
			remove_menuL1_item("TrafficAnalyzer_Statistic.asp");
		}
		else{
			menuL1_title[3] ="";            // Traffic Monitor
			menuL1_link[3] ="";
			menuL1_title[4] ="";            // Parental Control
			menuL1_link[4] ="";
			//menuL1_title[6] ="";            // AiCloud 2.0
			//menuL1_link[6] ="";
		}
		
		// Wireless
		if(sw_mode == 4){
			menuL2_title[1]="";
			menuL2_link[1]="";
		}
		else if(sw_mode == 2){
			if(userRSSI_support){
				remove_menu_item("Advanced_Wireless_Content.asp");
				remove_menu_item("Advanced_WWPS_Content.asp");
				remove_menu_item("Advanced_WMode_Content.asp");								
				remove_menu_item("Advanced_ACL_Content.asp");
				remove_menu_item("Advanced_WSecurity_Content.asp");
			}
			else{
				menuL2_title[1]="";
				menuL2_link[1]="";
			}

			if(wlc_express != 0){
				menuL2_title[1] = "";
				menuL2_link[1] = "";
			}
		}
		//Passpoint
		menuL2_title[2]="";
		menuL2_link[2]="";
		// WAN
		menuL2_title[4]="";
		menuL2_link[4]="";	
		// LAN
		remove_menu_item("Advanced_DHCP_Content.asp");
		remove_menu_item("Advanced_GWStaticRoute_Content.asp");
		remove_menu_item("Advanced_IPTV_Content.asp");								
		remove_menu_item("Advanced_SwitchCtrl_Content.asp");
		//IPv6
		menuL2_title[6]="";
		menuL2_link[6]="";
		// VPN
		menuL2_title[7]="";
		menuL2_link[7]="";
		// Firewall		
		menuL2_title[8]="";
		menuL2_link[8]="";
		// Log
		remove_menu_item("Main_DHCPStatus_Content.asp");
		remove_menu_item("Main_IPV6Status_Content.asp");
		remove_menu_item("Main_RouteStatus_Content.asp");
		remove_menu_item("Main_IPTStatus_Content.asp");
		remove_menu_item("Main_ConnStatus_Content.asp");
	}
	else if(sw_mode == 3){
		if(bwdpi_support){
			menuL1_title[3] ="";		//Parental Control
			menuL1_link[3] ="";
			menuL1_title[4] ="";		//QoS
			menuL1_link[4] ="";
			remove_menuL1_item("TrafficAnalyzer_Statistic.asp");
			//menuL1_title[7] ="";		//AiCloud 2.0
			//menuL1_link[7] ="";
		}
		else{
			menuL1_title[3] ="";		// Traffic Monitor
			menuL1_link[3] ="";
			menuL1_title[4] ="";		// Parental Control
			menuL1_link[4] ="";
			//menuL1_title[6] ="";		// AiCloud 2.0
			//menuL1_link[6] ="";
		}
		// WAN
		menuL2_title[4]="";
		menuL2_link[4]="";
		// LAN
		if(!dhcp_override_support)
			remove_menu_item("Advanced_DHCP_Content.asp");
		remove_menu_item("Advanced_GWStaticRoute_Content.asp");
		remove_menu_item("Advanced_IPTV_Content.asp");
		remove_menu_item("Advanced_SwitchCtrl_Content.asp");
		// IPv6
		menuL2_title[6]="";
		menuL2_link[6]="";
		// VPN
		menuL2_title[7]="";
		menuL2_link[7]="";
		// Firewall		
		menuL2_title[8]="";
		menuL2_link[8]="";
		// Log
		remove_menu_item("Main_DHCPStatus_Content.asp");
		remove_menu_item("Main_IPV6Status_Content.asp");
		remove_menu_item("Main_RouteStatus_Content.asp");
		remove_menu_item("Main_IPTStatus_Content.asp");
		remove_menu_item("Main_ConnStatus_Content.asp");										
	}
	
	if(!dualWAN_support){
		remove_menu_item("Advanced_WANPort_Content.asp");
		if (dsl_support) {
			menuL2_link[4] = "Advanced_DSL_Content.asp";
			tablink[3][1] = "Advanced_DSL_Content.asp";
		}
	}
	else{		
		var dualwan_pri_if = wans_dualwan_array[0];
		if(dualwan_pri_if == 'lan' || dualwan_pri_if == 'wan'){
			menuL2_link[4] = "Advanced_WAN_Content.asp";
			tablink[3][1] = "Advanced_WAN_Content.asp";
		}
		else if(dualwan_pri_if == 'usb'){
			if(based_modelid == '4G-AC55U'){
				menuL2_link[4] = "Advanced_MobileBroadband_Content.asp";
				tablink[3][1] = "Advanced_MobileBroadband_Content.asp";
			}
			else{
				menuL2_link[4] = "Advanced_Modem_Content.asp";
				tablink[3][1] = "Advanced_Modem_Content.asp";
			}
		}
		else if(dualwan_pri_if == 'dsl'){
			menuL2_link[4] = "Advanced_DSL_Content.asp";
			tablink[3][1] = "Advanced_DSL_Content.asp";
		}
	}

	if(!media_support || nomedia_support){
		remove_menu_item("mediaserver.asp");
	}

	if(!rog_support){
		remove_menu_item("AdaptiveQoS_ROG.asp");
	}

//	if(!cooler_support){
//		remove_menu_item("Advanced_PerformanceTuning_Content.asp");
//	}

	if(!ParentalCtrl2_support && !yadns_support && !dnsfilter_support){
		menuL1_title[4] = "";
		menuL1_link[4] = "";
	}
	else if(!ParentalCtrl2_support && yadns_support){
		remove_menu_item("ParentalControl.asp");
		menuL1_link[4] = "YandexDNS.asp";
	}
	else if(!ParentalCtrl2_support && dnsfilter_support){
		remove_menu_item("ParentalControl.asp");
		menuL1_link[4]="DNSFilter.asp";
	}
	else if(ParentalCtrl2_support) {
		if  (!yadns_support)
			remove_menu_item("YandexDNS.asp");
		if (!dnsfilter_support)
			remove_menu_item("DNSFilter.asp");
	}

	if(!IPv6_support){
		menuL2_title[6] = "";
		menuL2_link[6] = "";
		remove_menu_item("Main_IPV6Status_Content.asp");

		tabtitle[7][6] = "";	/*IPv6 Firewall*/
		tablink[7][6] = "";
		remove_menu_item("Advanced_Firewall_IPv6_Content.asp");
	}
	
	if(multissid_support == -1){
		menuL1_title[2] = "";
		menuL1_link[2] = "";
	}

	if(!usb_support){
		menuL1_title[5] = "";
		menuL1_link[5] = "";
	}

	if(noftp_support){
		remove_menu_item("Advanced_AiDisk_ftp.asp");
	}

	if(!pptpd_support && !openvpnd_support){
		if(!vpnc_support){
			menuL2_title[7] = "";
			menuL2_link[7] = "";
		}
		else{
			remove_menu_item("Advanced_VPN_PPTP.asp");
			remove_menu_item("Advanced_VPN_OpenVPN.asp");
			remove_menu_item("Advanced_OpenVPNClient_Content.asp");
		}
	}
	else if(pptpd_support && !openvpnd_support){
		if(!vpnc_support){
			remove_menu_item("Advanced_VPNClient_Content.asp");
			remove_menu_item("Advanced_VPN_OpenVPN.asp");
		}
		remove_menu_item("Advanced_VPN_OpenVPN.asp");
		remove_menu_item("Advanced_OpenVPNClient_Content.asp");
	}
	else if(!pptpd_support && openvpnd_support){
		if(!vpnc_support){
			remove_menu_item("Advanced_VPNClient_Content.asp");
		}
		remove_menu_item("Advanced_VPN_PPTP.asp");
	}

	if(!nfsd_support){
		remove_menu_item("Advanced_AiDisk_NFS.asp")
	}

	if(!SwitchCtrl_support){
		remove_menu_item("Advanced_SwitchCtrl_Content.asp");		
	}

	if(!tr069_support){
		remove_menu_item("Advanced_TR069_Content.asp");
	}

	if(!snmp_support){
		remove_menu_item("Advanced_SNMP_Content.asp");
	}
	if(!smart_connect_support){
		remove_menu_item("Advanced_Smart_Connect.asp");
	}
	
	if(!adBlock_support){
		remove_menu_item("AiProtection_AdBlock.asp");
	}
	if(!keyGuard_support){
		remove_menu_item("AiProtection_Key_Guard.asp");
	}

	if(!tor_support){
		remove_menu_item("Advanced_TOR_Content.asp");
	}

	if(wtfast_support && sw_mode != 1){
		remove_menuL1_item("Advanced_WTFast_Content.asp");
	}
}

function remove_menu_item(remove_url){
	var dx;	

	for(i = 0; i<tablink.length; i++){		
		dx = tablink[i].getIndexByValue(remove_url);
		if(dx == 1) //If the url to be removed is the 1st tablink then replace by next tablink 
			menuL2_link.splice(i+1, 1, tablink[i][2]);
		if(dx >= 0){
			tabtitle[i].splice(dx, 1);
			tablink[i].splice(dx, 1);
			break;
		}		
	}
}

function remove_menuL1_item(remove_url){
	var dx;

	for(i = 0; i<menuL1_link.length; i++){
		dx = menuL1_link.getIndexByValue(remove_url);
		if(dx >= 0){
			menuL1_link.splice(dx, 1);
			menuL1_title.splice(dx, 1);
			break;
		}
	}
}

String.prototype.shorter = function(len){
	var replaceWith = "...";

	if(this.length > len)
		return this.substring(0, len-replaceWith.length)+replaceWith;
	else
		return this.toString();
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

Array.prototype.getRowIndexByValue2D = function(value, col){
	for(var i=0; i<this.length; i++){
		if(typeof(col) != "undefined" && col >= 0) {
			if(this[i][col] == value)
				return i;
		}
		else {
			for(var j=0; j<this[i].length; j++) {
				if(this[i][j] == value)
					return i;
			}
		}
	}
	return -1;
}

Array.prototype.del = function(n){
　if(n < 0)
　　return this;
　else
　　return this.slice(0,n).concat(this.slice(n+1,this.length));
}

var current_url = location.pathname.substring(location.pathname.lastIndexOf('/') + 1);
function show_menu(){

	var L1 = 0, L2 = 0, L3 = 0;
	if(current_url == "") current_url = "index.asp";
	if (dualWAN_support){
		// fix dualwan showmenu
		if(current_url == "Advanced_DSL_Content.asp") current_url = "Advanced_WAN_Content.asp";
		if(current_url == "Advanced_VDSL_Content.asp") current_url = "Advanced_WAN_Content.asp";
		if(current_url == "Advanced_Modem_Content.asp") current_url = "Advanced_WAN_Content.asp";
		if(current_url == "Advanced_MobileBroadband_Content.asp") current_url = "Advanced_WAN_Content.asp";		
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
	if(L1 == traffic_L1_dx || L2 == traffic_L2_dx || L1 == tools_L1){
		if(current_url.indexOf("QoS_EZQoS") == 0 || current_url.indexOf("Advanced_QOSUserRules_Content") == 0 || current_url.indexOf("Advanced_QOSUserPrio_Content") == 0 || current_url.indexOf("Bandwidth_Limiter") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			if(bwdpi_support){
				L3 = 2;
			}
			else{
				L3 = 1;		
			}
		}
		else if(current_url.indexOf("AdaptiveQoS_ROG") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
		}
		else if(current_url.indexOf("AdaptiveQoS_WebHistory") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			L3 = 3;
		}
		else if(current_url.indexOf("Main_TrafficMonitor_") == 0){
			L1 = traffic_L1_dx; 
			L2 = traffic_L2_dx; 
			if(bwdpi_support){
				if(rog_support)
					L3 = 5;
				else
					L3 = 4;
			}
			else{
				L3 = 2;				
			}
			if(wtfast_support){
				L3++;
			}
		}		
		else if(current_url.indexOf("Main_Spectrum_") == 0){
			L1 = traffic_L1_dx;
			L2 = traffic_L2_dx;
			if(bwdpi_support){
				if(rog_support){
					L3 = 6;
				}
				else{
					L3 = 5;
				}
			}
			else
				L3 = 3;
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
	if(current_url.indexOf("Tools_") == 0) {
		traffic_L2_dx = 16;
		L1 = tools_L1;
		L2 = traffic_L2_dx;
	}

	// cloud
	if(current_url.indexOf("cloud") == 0){
		L1 = menuL1_link.indexOf("cloud_main.asp");
	}

	if(!bwdpi_support){
		if(current_url.indexOf("ParentalControl") == 0){
			traffic_L1_dx = 4;
			traffic_L2_dx = 13;
			traffic_L3_dx = 0;
			
			if(yadns_support || dnsfilter_support){
				traffic_L3_dx = 1;
			}
			
			L1 = traffic_L1_dx;	
			L2 = traffic_L2_dx;
			L3 = traffic_L3_dx;
		}	
	}
	else{
		if(current_url.indexOf("ParentalControl") == 0){
			traffic_L2_dx = 13;
			traffic_L3_dx = 2;				
			if(yadns_support || dnsfilter_support){
				traffic_L3_dx = 2;
			}
			
			L2 = traffic_L2_dx;
			L3 = traffic_L3_dx;		
		}	
	}
	if(traffic_analyzer_support){
		if(current_url.indexOf("TrafficAnalyzer_Statistic") == 0){
			traffic_L2_dx = 15;
			traffic_L3_dx = 2;
			L1 = 5;
			L2 = traffic_L2_dx;
			L3 = traffic_L3_dx;
			if (wtfast_support){
				L1++;
			}
		}
	
		if(current_url.indexOf("Main_TrafficMonitor") == 0){
			traffic_L2_dx = 15;
			traffic_L3_dx = 1;
			L1 = 5;
			L2 = traffic_L2_dx;
			L3 = traffic_L3_dx;
			if (wtfast_support){
				L1++;
			}
		}

	}
	
	if(current_url.indexOf("AdaptiveQoS_Adaptive") == 0){
			traffic_L1_dx = 4;
			traffic_L2_dx = 12;
			L1 = traffic_L1_dx;	
			L2 = traffic_L2_dx;
			L3 = 2;
	}
		
	if(current_url.indexOf("Advanced_DSL_Content") == 0 || 
		current_url.indexOf("Advanced_VDSL_Content") == 0 ||
		current_url.indexOf("Advanced_WAN_Content") == 0){
			traffic_L1_dx = 0;
			traffic_L2_dx = 4;
			L1 = traffic_L1_dx;	
			L2 = traffic_L2_dx;
			L3 = 1;
	}
	
	if(current_url.indexOf("YandexDNS") == 0){
		if(ParentalCtrl2_support && yadns_support){
			if(bwdpi_support){
				traffic_L1_dx = 3;
				L3 = 3;
			} else {
				traffic_L1_dx = 4;
				L3 = 2;
			}
			traffic_L2_dx = 13;
			L1 = traffic_L1_dx;	
			L2 = traffic_L2_dx;
		}	
	}

	if(current_url.indexOf("DNSFilter") == 0){
		if(ParentalCtrl2_support && dnsfilter_support){
			if(bwdpi_support){
				traffic_L1_dx = 3;
				if (adBlock_support) {
					L3 = 4;
				} else {
					L3 = 3;
				}
			} else {
				traffic_L1_dx = 4;
				L3 = 2;
			}
			traffic_L2_dx = 13;
			L1 = traffic_L1_dx;
			L2 = traffic_L2_dx;
		}
	}

	//Feedback Info
	if(current_url.indexOf("Feedback_Info") == 0){
		traffic_L1_dx = 0;
		traffic_L2_dx = 9;
		L1 = traffic_L1_dx;	
		L2 = traffic_L2_dx;
		L3 = 5;
	}
	
	show_banner(L3);
	show_footer();
	browser_compatibility();	
	show_selected_language();
	autoFocus('<% get_parameter("af"); %>');

	// QIS wizard
	if(sw_mode == 2){
		if(wlc_express == '1'){
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'/'+ QISWIZARD +'?flag=sitesurvey_exp2\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
		}
		else if(wlc_express == '2'){
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'/'+ QISWIZARD +'?flag=sitesurvey_exp5\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
		}
		else{
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'/'+ QISWIZARD +'?flag=sitesurvey_rep\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
		}
	}else if(sw_mode == 3){
		menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\''+ QISWIZARD +'?flag=lanip\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n'; 	
	}else if(sw_mode == 4){
		menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\'/'+ QISWIZARD +'?flag=sitesurvey_mb\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
	}else{
		if(tmo_support && isMobile())
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\''+ QISWIZARD +'?flag=wireless\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
		else
			menu1_code += '<div class="m_qis_r" style="margin-top:-170px;cursor:pointer;" onclick="go_setting(\''+ QISWIZARD +'?flag=detect\');"><table><tr><td><div id="index_img0"></div></td><td><div><#QIS#></div></td></tr></table></div>\n';
	}	

	// Feature
	menu1_code += '<div class="m0_r" style="margin-top:10px;" id="option0"><table width="192px" height="37px"><tr><td><#menu5_1_1#></td></tr></table></div>\n';
	for(i = 1; i <= menuL1_title.length-2; i++){
		if(menuL1_title[i] == ""){
			calculate_height--;
			continue;
		}
		else if(L1 == i && (L2 <= 0 || L2 == traffic_L2_dx)){		//clicked
			var clicked_manu = "_" + menuL1_link[i].split('.')[0];
			menu1_code += '<div class="menu_clicked" id="'+ clicked_manu +'">'+'<table><tr><td><div class="'+clicked_manu+'"></div></td><td><div id="menu_string'+i+'" style="width:120px;">'+menuL1_title[i]+'</div></td></tr></table></div>\n';
		}
		else{		//non-click	
			var menu_type = "_" + menuL1_link[i].split('.')[0];
			menu1_code += '<div class="menu"  id="'+ menu_type +'" onclick="location.href=\''+menuL1_link[i]+'\'" style="cursor:pointer;"><table><tr><td><div class="'+menu_type+'"></div></td><td><div id="menu_string" style="width:120px;">'+menuL1_title[i]+'</div></td></tr></table></div>\n';
		}
	}
	menu1_code += '<div class="m0_r" id="option0">'+'<table width="192px" height="37px"><tr><td><#menu5#></td></tr></table></div>\n'; 	
	document.getElementById("mainMenu").innerHTML = menu1_code;

	// Advanced
	if(L2 != -1){ 	
		for(var i = 1; i < menuL2_title.length; ++i){
			if(menuL2_link[i] == "Advanced_Wireless_Content.asp" && "<% nvram_get("wl_subunit"); %>" != "0" && "<% nvram_get("wl_subunit"); %>" != "-1")
				menuL2_link[i] = "javascript:change_wl_unit_status(" + <% nvram_get("wl_unit"); %> + ");";
			if(menuL2_title[i] == "" || i == 5){
				calculate_height--;
				continue;
			}
			else if(L2 == i){
				menu2_code += '<div class="menu_clicked" id="option'+i+'">'+'<table><tr><td><div id="menu_img'+i+'"></div></td><td><div id="option_str1" style="width:120px;">'+menuL2_title[i]+'</div></td></tr></table></div>\n';
			}else{		
				menu2_code += '<div class="menu" id="option'+i+'" onclick="location.href=\''+menuL2_link[i]+'\'" style="cursor:pointer;"><table><tr><td><div id="menu_img'+i+'"></div></td><td><div id="option_str1" style="width:120px;">'+menuL2_title[i]+'</div></td></tr></table></div>\n';
			}	
		}
	}
	document.getElementById("subMenu").innerHTML = menu2_code;

	// Tabs
	if(L3){
		if(L2 == traffic_L2_dx && L2 != 13){ // if tm
			tab_code = '<table border="0" cellspacing="0" cellpadding="0"><tr>\n';
			for(var i=1; i < tabtitle[traffic_L2_dx-1].length; ++i){
				if(tabtitle[traffic_L2_dx-1][i] == "")
					continue;
				else if(L3 == i)
					tab_code += '<td><div class="tabclick"><span>'+ tabtitle[traffic_L2_dx-1][i] +'</span></div></td>';
				else
					tab_code += '<td><a onclick="location.href=\'' + tablink[traffic_L2_dx-1][i] + '\'"><div class="tab"><span>'+ tabtitle[traffic_L2_dx-1][i] +'</span></div></a></td>';
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
		
		document.getElementById("tabMenu").innerHTML = tab_code;
	}

	if(current_url == "index.asp" || current_url == "")
		cal_height();
	else
		setTimeout('cal_height();', 1);

	/*-- notification start --*/
	cookie.unset("notification_history");
	if(notice_acpw_is_default == 1){	//case1
		if(default_psk_support){
			location.href = 'Main_Password.asp?nextPage=' + window.location.pathname.substring(1 ,window.location.pathname.length);
		}
		else{
			notification.array[0] = 'noti_acpw';
			notification.acpw = 1;
			notification.desc[0] = '<#ASUSGATE_note1#>';
			notification.action_desc[0] = '<#ASUSGATE_act_change#>';
			notification.clickCallBack[0] = "location.href = 'Advanced_System_Content.asp?af=http_passwd2';"
		}
	}else
		notification.acpw = 0;
/*
	if(isNewFW('<% nvram_get("webs_state_info"); %>')){	//case2
		notification.array[1] = 'noti_upgrade';
		notification.upgrade = 1;
		notification.desc[1] = '<#ASUSGATE_note2#>';
		notification.action_desc[1] = '<#ASUSGATE_act_update#>';
		notification.clickCallBack[1] = "location.href = 'Advanced_FirmwareUpgrade_Content.asp';"
	}else
*/
		notification.upgrade = 0;
	
	if(band2g_support && sw_mode != 4 && noti_auth_mode_2g == 'open'){ //case3-1
			notification.array[2] = 'noti_wifi_2g';
			notification.wifi_2g = 1;
			notification.desc[2] = '<#ASUSGATE_note3#>';
			notification.action_desc[2] = '<#ASUSGATE_act_change#> (2.4GHz)';
			notification.clickCallBack[2] = "change_wl_unit_status(0);";
	}else
		notification.wifi_2g = 0;
		
	if(band5g_support && sw_mode != 4 && noti_auth_mode_5g == 'open'){	//case3-2
			notification.array[3] = 'noti_wifi_5g';
			notification.wifi_5g = 1;
			notification.desc[3] = '<#ASUSGATE_note3#>';
			notification.action_desc[3] = '<#ASUSGATE_act_change#> (5 GHz)';
			notification.clickCallBack[3] = "change_wl_unit_status(1);";
	}else
		notification.wifi_5g = 0;

/*
	if(usb_support && !noftp_support && enable_ftp == 1 && st_ftp_mode == 1 && st_ftp_force_mode == '' ){ //case4_1
			notification.array[4] = 'noti_ftp';
			notification.ftp = 1;
			notification.desc[4] = '<#ASUSGATE_note4_1#>';
			notification.action_desc[4] = '<#web_redirect_suggestion_etc#>';
			notification.clickCallBack[4] = "showLoading();setTimeout('document.noti_ftp.submit();', 1);setTimeout('notification.redirectftp()', 2000);";
	}else if(usb_support && !noftp_support && enable_ftp == 1 && st_ftp_mode != 2){	//case4
			notification.array[4] = 'noti_ftp';
			notification.ftp = 1;
			notification.desc[4] = '<#ASUSGATE_note4#>';
			notification.action_desc[4] = '<#ASUSGATE_act_change#>';
			notification.clickCallBack[4] = "showLoading();setTimeout('document.noti_ftp.submit();', 1);setTimeout('notification.redirectftp()', 2000);";
	}else
*/
		notification.ftp = 0;
	
/*
	if(usb_support && enable_samba == 1 && st_samba_mode == 1 && st_samba_force_mode == ''){ //case5_1
			notification.array[5] = 'noti_samba';
			notification.samba = 1;
			notification.desc[5] = '<#ASUSGATE_note5_1#>';
			notification.action_desc[5] = '<#web_redirect_suggestion_etc#>';	
			notification.clickCallBack[5] = "showLoading();setTimeout('document.noti_samba.submit();', 1);setTimeout('notification.redirectsamba()', 2000);";
	}else if(usb_support && enable_samba == 1 && st_samba_mode != 4){	//case5
			notification.array[5] = 'noti_samba';
			notification.samba = 1;
			notification.desc[5] = '<#ASUSGATE_note5#>';
			notification.action_desc[5] = '<#ASUSGATE_act_change#>';
			notification.clickCallBack[5] = "showLoading();setTimeout('document.noti_samba.submit();', 1);setTimeout('notification.redirectsamba()', 2000);";
	}else
*/
		notification.samba = 0;

	//Higher priority: DLA intervened case dsltmp_dla_modified 0: default / 1:need to feedback / 2:Feedback submitted 
	//Lower priority: dsl_loss_sync  0: default / 1:need to feedback / 2:Feedback submitted
	// Only DSL-AC68U for now
	if(dsl_loss_sync == 1){         //case9(case10 act) + case6
		
		notification.loss_sync = 1;
		if(dla_modified == 1){
				notification.array[9] = 'noti_dla_modified';
				notification.desc[9] = Untranslated.ASUSGATE_note9;
				notification.action_desc[9] = Untranslated.ASUSGATE_DSL_setting;
				notification.clickCallBack[9] = "location.href = '/Advanced_ADSL_Content.asp?af=dslx_dla_enable';";
				notification.array[10] = 'noti_dla_modified_fb';
				notification.action_desc[10] = Untranslated.ASUSGATE_act_feedback;
				notification.clickCallBack[10] = "location.href = '/Advanced_Feedback.asp';";
		}
		else{
				notification.array[6] = 'noti_loss_sync';
				notification.desc[6] = Untranslated.ASUSGATE_note6;
				notification.action_desc[6] = Untranslated.ASUSGATE_act_feedback;
				notification.clickCallBack[6] = "location.href = '/Advanced_Feedback.asp';";
		}						
	}else
		notification.loss_sync = 0;
		
	//experiencing DSL issue experience_fb=0: notif, 1:no display again.
	if(experience_fb == 0){		//case7
			notification.array[7] = 'noti_experience_FB';
			notification.experience_FB = 1;
			notification.desc[7] = Untranslated.ASUSGATE_note7;
			notification.action_desc[7] = Untranslated.ASUSGATE_act_feedback;
			notification.clickCallBack[7] = "setTimeout('document.noti_experience_Feedback.submit();', 1);setTimeout('notification.redirectFeedback()', 1000);";
	}else
			notification.experience_FB = 0;

	//Notification hint-- null&0: default, 1:display info
	if(noti_notif_Flag == 1 && notif_msg != ""){               //case8
		notification.array[8] = 'noti_notif_hint';
		notification.notif_hint = 1;
		notification.desc[8] = notif_msg;
		notification.action_desc[8] = "<#CTL_ok#>";
		notification.clickCallBack[8] = "setTimeout('document.noti_notif_hint.submit();', 1);setTimeout('notification.redirectHint()', 100);"
	}else
		notification.notif_hint = 0;

	//DLA send debug log  -- 4: send by manual, else: nothing to show
	if(wan_diag_state == "4"){               //case11
		notification.array[11] = 'noti_send_debug_log';
		notification.send_debug_log = 1;
		notification.desc[11] = "-	Diagnostic DSL debug log capture completed.";
		notification.action_desc[11] = "Send debug log now";
		notification.clickCallBack[11] = "setTimeout('notification.redirectFeedbackInfo()', 1000);";
	
	}else
		notification.send_debug_log = 0;

	if( notification.acpw || notification.upgrade || notification.wifi_2g || notification.wifi_5g || notification.ftp || notification.samba || notification.loss_sync || notification.experience_FB || notification.notif_hint || notification.send_debug_log || notification.mobile_traffic || notification.sim_record || notification.external_ip){
		notification.stat = "on";
		notification.flash = "on";
		notification.run();
	}
	/*--notification end--*/
}

function addOnlineHelp(obj, keywordArray){
	var support_path = "support/Knowledge-searchV2/?";
	var faqLang = {
		EN : "/us/",
		TW : "/us/",
		CN : "/us/",
		CZ : "/us/",
		PL : "/us/",
		RU : "/us/",
		DE : "/us/",
		FR : "/us/",
		TR : "/us/",
		TH : "/us/",
		MS : "/us/",
		NO : "/us/",
		FI : "/us/",
		DA : "/us/",
		SV : "/us/",
		BR : "/us/",
		JP : "/us/",
		ES : "/us/",
		IT : "/us/",
		UK : "/us/",
		HU : "/us/",
		RO : "/us/",
		KR : "/us/"
	}

	// exception start
	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "download" 
			&& (keywordArray[2] == "master" || keywordArray[2] == "associated")){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.RU = "/ru/";
		faqLang.MS = "/my/";
		faqLang.SV = "/se/";
		faqLang.UK = "/ua/";
	
	}else if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "ez" && keywordArray[2] == "printer"){
		support_path = "support/Search-Result-Detail/552E63A3-FA2D-1265-7029-84467B3993E4/?";
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
        faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.SV = "/se/";
        faqLang.UK = "/ua/";
		faqLang.DA = "/dk/";
		faqLang.FI = "/fi/";
		faqLang.TR = "/tr/";
		faqLang.DE = "/de/";
		faqLang.PL = "/pl/";
		faqLang.CZ = "/cz/";
	
	}else if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "lpr"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
        faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.SV = "/se/";
        faqLang.UK = "/ua/";
        faqLang.FI = "/fi/";
        faqLang.TR = "/tr/";
        faqLang.DE = "/de/";
        faqLang.PL = "/pl/";
        faqLang.CZ = "/cz/";
		faqLang.BR = "/br/";
		faqLang.TH = "/th/";
	
	}else	if(keywordArray[0] == "mac" && keywordArray[1] == "lpr"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
        faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.SV = "/se/";
        faqLang.UK = "/ua/";
        faqLang.FI = "/fi/";
        faqLang.TR = "/tr/";
        faqLang.DE = "/de/";
        faqLang.PL = "/pl/";
        faqLang.CZ = "/cz/";
        faqLang.BR = "/br/";
        faqLang.TH = "/th/";
	}else	if(keywordArray[0] == "monopoly" && keywordArray[1] == "mode"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
	
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "IPv6"){
		faqLang.MS = "/my/";
	
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "VPN"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.MS = "/my/";
	
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "DMZ"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.MS = "/my/";
	
	}else	if(keywordArray[0] == "set" && keywordArray[1] == "up" && keywordArray[2] == "specific" && keywordArray[3] == "IP" && keywordArray[4] == "addresses"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";		
	
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "port" && keywordArray[2] == "forwarding"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.MS = "/my/";
		faqLang.RU = "/ru/";
		faqLang.UK = "/ua/";
	
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "port" && keywordArray[2] == "trigger"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.ES = "/es/";		
	
	}else	if(keywordArray[0] == "UPnP"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
        faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.UK = "/ua/";
        faqLang.PL = "/pl/";
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "hard" && keywordArray[2] == "disk" 
		&& keywordArray[3] == "USB"){

	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "Traffic" && keywordArray[2] == "Monitor"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.UK = "/ua/";
	}else	if(keywordArray[0] == "ASUSWRT" && keywordArray[1] == "samba" && keywordArray[2] == "Windows"
			&& keywordArray[3] == "network" && keywordArray[4] == "place"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.MY = "/my/";
	
	}else	if(keywordArray[0] == "samba"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
	
	}else	if(keywordArray[0] == "WOL" && keywordArray[1] == "wake" && keywordArray[2] == "on"
			&& keywordArray[3] == "lan"){
	
	}else	if(keywordArray[0] == "WOL" && keywordArray[1] == "BIOS"){
		faqLang.TW = "/tw/";
		faqLang.CN = ".cn/";
		faqLang.FR = "/fr/";
		faqLang.ES = "/es/";
		faqLang.RU = "/ru/";
        faqLang.MS = "/my/";
        faqLang.KR = "/kr/";
        faqLang.UK = "/ua/";
	}else{
		faqLang.BR = "/us/";
		faqLang.CN = "/us/";
		faqLang.CZ = "/us/";
		faqLang.DA = "/us/";
		faqLang.DE = "/us/";
		faqLang.EN = "/us/";
		faqLang.ES = "/us/";
		faqLang.FI = "/us/";
		faqLang.FR = "/us/";
		faqLang.HU = "/us/";
		faqLang.IT = "/us/";
		faqLang.JP = "/us/";
		faqLang.KR = "/us/";
		faqLang.MS = "/us/";
		faqLang.NO = "/us/";
		faqLang.PL = "/us/";
		faqLang.RO = "/us/";
		faqLang.RU = "/us/";
		faqLang.SV = "/us/";
		faqLang.TH = "/us/";
		faqLang.TR = "/us/";
		faqLang.TW = "/us/";
		faqLang.UK = "/us/";
	}		
	// exception end	

	if(obj){
		obj.href = "http://www.asus.com"+faqLang.<% nvram_get("preferred_lang"); %>;
		obj.href += support_path;
		obj.href += "keyword=";
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
				alert(keywordArray+ " <#JS_invalid_chars#>");
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
	var manualOffSet = 25;
	var table_height = Math.round(optionHeight*calculate_height - manualOffSet*calculate_height/14 - document.getElementById("tabMenu").clientHeight);	
	if(navigator.appName.indexOf("Microsoft") < 0)
		var contentObj = document.getElementsByClassName("content");
	else
		var contentObj = getElementsByClassName_iefix("table", "content");

	if(document.getElementById("FormTitle") && current_url.indexOf("Advanced_AiDisk_ftp") != 0 && current_url.indexOf("Advanced_AiDisk_samba") != 0 && current_url.indexOf("QoS_EZQoS") != 0){
		table_height = table_height + 24;
		document.getElementById("FormTitle").style.height = table_height + "px";
		tableClientHeight = document.getElementById("FormTitle").clientHeight;
	}
	// index.asp
	else if(document.getElementById("NM_table")){
		var statusPageHeight = 720;
		if(table_height < 800)
			table_height = 800;
		document.getElementById("NM_table").style.height = table_height + "px";

		tableClientHeight = document.getElementById("NM_table").clientHeight;
	}
	// aidisk.asp
	else if(document.getElementById("AiDiskFormTitle")){
		table_height = table_height + 24;
		document.getElementById("AiDiskFormTitle").style.height = table_height + "px";
		tableClientHeight = document.getElementById("AiDiskFormTitle").clientHeight;
	}
	// APP Install
	else if(document.getElementById("applist_table")){
		if(sw_mode == 2 || sw_mode == 3 || sw_mode == 4)
			table_height = table_height + 120;				
		else
			table_height = table_height + 40;	//2
		
		document.getElementById("applist_table").style.height = table_height + "px";		
		tableClientHeight = document.getElementById("applist_table").clientHeight;
		
		if(navigator.appName.indexOf("Microsoft") >= 0)
			contentObj[0].style.height = contentObj[0].clientHeight + 18 + "px";
	}
	// PrinterServ
	else if(document.getElementById("printerServer_table")){
		if(sw_mode == 2 || sw_mode == 4)
			table_height = table_height + 90;
		else
			table_height = table_height + 2;
		
		document.getElementById("printerServer_table").style.height = table_height + "px";		
		tableClientHeight = document.getElementById("printerServer_table").clientHeight;
		
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

function submitenter(myfield,e)
{
	var keycode;
	if (window.event) keycode = window.event.keyCode;
	else if (e) keycode = e.which;
	else return true;
	if (keycode == 13){
		search_supportsite();
		return false;
	}
	else
		return true;
}

function show_footer(){
	var href_lang = get_supportsite_lang();
	footer_code = '<div align="center" class="bottom-image"></div>\n';
	footer_code +='<div align="center" class="copyright"><#footer_copyright_desc#></div><br>';

	// FAQ searching bar{
	footer_code += '<div style="margin-top:-75px;margin-left:205px;"><table width="765px" border="0" align="center" cellpadding="0" cellspacing="0"><tr>';
	footer_code += '<td width="20" align="right"><div id="bottom_help_icon" style="margin-right:3px;"></div></td><td width="115" id="bottom_help_title" align="left"><#Help#> & <#Support#></td>';

	var tracing_path_Manual	= "/HelpDesk_Manual/?utm_source=asus-product&utm_medium=referral&utm_campaign=router";
	var tracing_path_Utility = "/HelpDesk_Download/?utm_source=asus-product&utm_medium=referral&utm_campaign=router";
	var model_name_supportsite = based_modelid.replace("-", "");
	model_name_supportsite = model_name_supportsite.replace("+", "Plus");

	if(based_modelid =="RT-N12" || hw_ver.search("RTN12") != -1){	//MODELDEP : RT-N12
		if( hw_ver.search("RTN12HP") != -1){	//RT-N12HP
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12HP"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12HP"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
		}else if(hw_ver.search("RTN12B1") != -1){ //RT-N12B1
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_B1"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_B1"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
		}else if(hw_ver.search("RTN12C1") != -1){ //RT-N12C1
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_C1"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_C1"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
		}else if(hw_ver.search("RTN12D1") != -1){ //RT-N12D1
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_D1"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/RTN12_D1"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
		}else{	//RT-N12
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/"+ model_name_supportsite + tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/" + model_name_supportsite + tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";	
		}
	}
	else if(based_modelid == "DSL-N55U"){	//MODELDEP : DSL-N55U
		footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/Networking/DSLN55U_Annex_A"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/Networking/DSLN55U_Annex_A"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
	}
	else if(based_modelid == "DSL-N55U-B"){	//MODELDEP : DSL-N55U-B								
		footer_codk_Download += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/Networking/DSLN55U_Annex_B"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/Networking/DSLN55U_Annex_B"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
	}
	else if(based_modelid == "RT-AC68R"){	//MODELDEP : RT-AC68R
		footer_codk_Download += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/us/supportonly/RT-AC68R"+ tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com/us/supportonly/RT-AC68R"+ tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";
	}
	else{
		if(based_modelid == "DSL-AC68U" || based_modelid == "DSL-AC68R" || based_modelid == "RT-N11P" || based_modelid == "RT-N300")
			href_lang = "/";	//global only

		if(odmpid == "RT-AC1200G+")
			model_name_supportsite = "RT-AC1200G-Plus";
		else if(odmpid == "RT-AC1200G")
			model_name_supportsite = "RT-AC1200G";
		else if(odmpid.search("RT-N12E_B") != -1)	//RT-N12E_B or RT-N12E_B1
			model_name_supportsite = "RTN12E_B1";

		if(tmo_support){
			footer_code += '<td width="310" id="bottom_help_link" align="left">&nbsp';
			footer_code += '&nbsp|&nbsp<a style="font-weight:bolder;text-decoration:underline;cursor:pointer;" onClick="show_contactus();">Contact ASUS</a><div id="contactus_block" style="position:absolute;z-index:999;width:280px;height:155px;margin-top:-200px;*margin-top:-180px;margin-left:0px;*margin-left:-100px;background-color:#2B373B;box-shadow: 3px 10px 10px #000;-webkit-border-radius: 5px;-moz-border-radius: 5px;border-radius:5px;display:none;"></div>';
		}
		else
			footer_code += "<td width=\"310\" id=\"bottom_help_link\" align=\"left\">&nbsp&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/"+ model_name_supportsite + tracing_path_Manual +"\" target=\"_blank\"><#Manual#></a>&nbsp|&nbsp<a style=\"font-weight: bolder;text-decoration:underline;cursor:pointer;\" href=\"http://www.asus.com"+ href_lang +"Networking/" + model_name_supportsite + tracing_path_Utility +"\" target=\"_blank\"><#Utility#></a>";	
	}

	if(dsl_support && feedback_support){
		footer_code += '&nbsp|&nbsp<a id="fb_link" href="/Advanced_Feedback.asp" target="_self" style="font-weight: bolder;text-decoration:underline;cursor:pointer;"><#menu_feedback#></a>';
	}
	else if(feedback_support){
		var location_href = '/Advanced_Feedback.asp?origPage=' + window.location.pathname.substring(1 ,window.location.pathname.length);
		footer_code += '&nbsp|&nbsp<a id="fb_link" href="'+location_href+'" target="_blank" style="font-weight: bolder;text-decoration:underline;cursor:pointer;"><#menu_feedback#></a>';
	}
	footer_code += '</td>';
	footer_code += '<td width="290" id="bottom_help_FAQ" align="right" style="font-family:Arial, Helvetica, sans-serif;">FAQ&nbsp&nbsp<input type="text" id="FAQ_input" class="input_FAQ_table" maxlength="40" onKeyPress="submitenter(this,event);" autocorrect="off" autocapitalize="off"></td>';
	footer_code += '<td width="30" align="left"><div id="bottom_help_FAQ_icon" class="bottom_help_FAQ_icon" style="cursor:pointer;margin-left:3px;" target="_blank" onClick="search_supportsite();"></div></td>';
	footer_code += '</tr></table></div>\n';
	//}

	document.getElementById("footer").innerHTML = footer_code;
	flash_button();
}

function show_contactus(){
	
	var contactus_info = "";
	contactus_info += "<table border=\"0\" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:280px;height:155px;padding:5px 10px 0px 10px;	-webkit-border-radius: 5px;-moz-border-radius: 5px;border-radius:5px;behavior: url(../PIE.htc);\">";
	contactus_info += "<tr><td>";
	contactus_info += "<div style=\"margin:5px 0px -5px 5px;font-family:Arial, Helvetica, sans-serif;font-size:14px;font-weight:bolder\">Contact us (Hotlines)</div>";
	contactus_info += "</td><td align=\"right\">";
	contactus_info += "<a href=\"javascript:close_contactus();\"><img width=\"18px\" height=\"18px\" src=\"/images/button-close2.png\" onmouseover=\"this.src='/images/button-close2.png'\" onmouseout=\"this.src='/images/button-close.png'\" border=\"0\"></a>";
	contactus_info += "</td></tr>";
	contactus_info += "<tr><td colspan=\"2\">";
	contactus_info += "<img style=\"width:90%;height:2px\" src=\"/images/New_ui/networkmap/linetwo2.png\">";
	contactus_info += "</td></tr>";
	contactus_info += "<tr><td colspan=\"2\"><div style=\"font-size:14px;margin-left:5px;text-shadow: 1px 1px 0px black;font-family: Arial, Helvetica, sans-serif;font-weight: bolder;\">Component Product Support :</div>";
	contactus_info += "<div style=\"font-size:16px;color :#FFCC00;margin-top:5px;margin-left:25px;text-shadow: 1px 1px 0px black;font-family: Arial, Helvetica, sans-serif;font-weight: bolder;margin-left:5px;\">1-812-282-2787</div>";
	contactus_info += "<br>";
	contactus_info += "<div style=\"font-size:13px;margin-left:5px;text-decoration:underline;font-family: Arial, Helvetica, sans-serif;\">Open Hour :</div>";
	contactus_info += "<div style=\"font-size:13px;margin-top:5px;margin-left:5px;font-family: Arial, Helvetica, sans-serif;font-weight: bolder;\">5:30 ~ 23:00 PST ( Monday ~ Friday )</div>";
	contactus_info += "<div style=\"font-size:13px;margin-left:5px;font-family: Arial, Helvetica, sans-serif;font-weight: bolder;\">6:00 ~ 15:00 PST ( Saturday ~ Sunday )</div>";
	contactus_info += "</td></tr>";
	contactus_info += "</table>";
	document.getElementById('contactus_block').innerHTML = contactus_info;
	document.getElementById('contactus_block').style.display = "";
	
}

function close_contactus(){
	document.getElementById('contactus_block').style.display = "none";
}

function get_supportsite_lang(obj){
	var faqLang = {
		EN : "/us/",
		TW : "/tw/",
		CN : ".cn/",
		BR : "/br/",
		CZ : "/cz/",
		DA : "/dk/",
		DE : "/de/",
		ES : "/es/",
		FI : "/fi/",
		FR : "/fr/",
		HU : "/hu/",
		IT : "/it/",
		JP : "/jp/",
		KR : "/kr/",
		MS : "/my/",
		NO : "/no/",
		PL : "/pl/",
		RO : "/ro/",
		RU : "/ru/",
		SV : "/se/",
		TH : "/th/",
		TR : "/tr/",
		UK : "/ua/"
	}	
	
	var href_lang = faqLang.<% nvram_get("preferred_lang"); %>;
	return href_lang;
}

function search_supportsite(obj){
	var faqLang = {
		EN : "/us/",
		TW : "/tw/",
		CN : ".cn/",
		BR : "/br/",
		CZ : "/cz/",
		DA : "/dk/",
		DE : "/de/",
		ES : "/es/",
		FI : "/fi/",
		FR : "/fr/",
		HU : "/hu/",
		IT : "/it/",
		JP : "/jp/",
		KR : "/kr/",
		MS : "/my/",
		NO : "/no/",
		PL : "/pl/",
		RO : "/ro/",
		RU : "/ru/",
		SV : "/se/",
		TH : "/th/",
		TR : "/tr/",
		UK : "/ua/"
	}	
	
		var keywordArray = document.getElementById("FAQ_input").value.split(" ");
		var faq_href;
		faq_href = "http://www.asus.com";		
		faq_href += faqLang.<% nvram_get("preferred_lang"); %>;
		faq_href += "support/Knowledge-searchV2/?utm_source=asus-product&utm_medium=referral&utm_campaign=router&";
		faq_href += "keyword=";
		for(var i=0; i<keywordArray.length; i++){
			faq_href += keywordArray[i];
			faq_href += "%20";
		}
		window.open(faq_href);
}

var isFirefox = navigator.userAgent.search("Firefox") > -1;
var isOpera = navigator.userAgent.search("Opera") > -1;
var isIE8 = navigator.userAgent.search("MSIE 8") > -1; 
var isiOS = navigator.userAgent.search("iP") > -1; 
function browser_compatibility(){
	var obj_inputBtn;

	if((isFirefox || isOpera) && document.getElementById("FormTitle")){
		document.getElementById("FormTitle").className = "FormTitle_firefox";
		if(current_url.indexOf("Guest_network") == 0)
			document.getElementById("FormTitle").style.marginTop = "-140px";
		if(current_url.indexOf("QoS_EZQoS.asp") == 0)
			document.getElementById("FormTitle").style.marginTop = "0px"
		/*if(current_url.indexOf("ParentalControl.asp") == 0 && !yadns_support)			//mark temporary, need to check 4M flash model. Jieming added at 2014/05/09
			document.getElementById("FormTitle").style.marginTop = "-140px";	*/
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
	if(!localAP_support){
		document.getElementById("ssidTitle").style.display = "none";
	}

	var ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0_ssid"); %>');
	var ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1_ssid"); %>');
	if(wl_info.band5g_2_support)
		var ssid_status_5g_2 =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl2_ssid"); %>');

	if(!band2g_support)
		ssid_status_2g = "";

	if(!band5g_support)
		ssid_status_5g = "";

	if(!wl_info.band5g_2_support)
		ssid_status_5g_2 = "";

	if(sw_mode == 4){
		if('<% nvram_get("wlc_band"); %>' == '0'){
			document.getElementById("elliptic_ssid_2g").style.display = "none";
			document.getElementById("elliptic_ssid_5g").style.marginLeft = "";
		}
		else
			document.getElementById("elliptic_ssid_5g").style.display = "none";

		document.getElementById('elliptic_ssid_2g').style.textDecoration="none";
		document.getElementById('elliptic_ssid_2g').style.cursor="auto";
		document.getElementById('elliptic_ssid_5g').style.textDecoration="none";
		document.getElementById('elliptic_ssid_5g').style.cursor="auto";
		if(wl_info.band5g_2_support){
			document.getElementById('elliptic_ssid_5g_2').style.textDecoration="none";
			document.getElementById('elliptic_ssid_5g_2').style.cursor="auto";
		}
	}
	else if(sw_mode == 2){
		if(concurrep_support){
			ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0.1_ssid"); %>');
			ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1.1_ssid"); %>');
			if(wl_info.band5g_2_support)
				ssid_status_5g_2 =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl2.1_ssid"); %>');

			if(wlc_express == '1'){
				document.getElementById('elliptic_ssid_2g').style.display = "none";
			}
			else if(wlc_express == '2'){
				document.getElementById('elliptic_ssid_5g').style.display = "none";
			}
			else if(wlc_express == '3'){
				if(wl_info.band5g_2_support)
					document.getElementById('elliptic_ssid_5g_2').style.display = "none";
			}
		}	
		else if('<% nvram_get("wlc_band"); %>' == '0')
			ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0.1_ssid"); %>');
		else if('<% nvram_get("wlc_band"); %>' == '2'){
			if(wl_info.band5g_2_support){
				ssid_status_5g_2 =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl2.1_ssid"); %>');
			}
		}
		else
			ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1.1_ssid"); %>');

		document.getElementById('elliptic_ssid_2g').style.textDecoration="none";
		document.getElementById('elliptic_ssid_2g').style.cursor="auto";
		document.getElementById('elliptic_ssid_5g').style.textDecoration="none";
		document.getElementById('elliptic_ssid_5g').style.cursor="auto";
		if(wl_info.band5g_2_support){
			document.getElementById('elliptic_ssid_5g_2').style.textDecoration="none";
			document.getElementById('elliptic_ssid_5g_2').style.cursor="auto";	
		}
	}
	
	var topbanner_ssid_2g = handle_show_str(ssid_status_2g);
	if(topbanner_ssid_2g.length >33){
		document.getElementById('elliptic_ssid_2g').innerHTML = extend_display_ssid(topbanner_ssid_2g)+"...";
	}
	else{
		document.getElementById('elliptic_ssid_2g').innerHTML = topbanner_ssid_2g;
	}
	document.getElementById('elliptic_ssid_2g').title = "2.4 GHz: \n"+ssid_status_2g;


	var topbanner_ssid_5g = handle_show_str(ssid_status_5g);
	if(topbanner_ssid_5g.length >33){
		document.getElementById('elliptic_ssid_5g').innerHTML = extend_display_ssid(topbanner_ssid_5g)+"...";
	}
	else{
		document.getElementById('elliptic_ssid_5g').innerHTML = topbanner_ssid_5g;
	}
	document.getElementById('elliptic_ssid_5g').title = "5 GHz: \n"+ssid_status_5g;


	if(wl_info.band5g_2_support){
		var topbanner_ssid_5g_2 = handle_show_str(ssid_status_5g_2);
		if(topbanner_ssid_5g_2.length >33){
			document.getElementById('elliptic_ssid_5g_2').innerHTML = extend_display_ssid(topbanner_ssid_5g_2)+"...";
		}
		else{
			document.getElementById('elliptic_ssid_5g_2').innerHTML = topbanner_ssid_5g_2;
		}
		document.getElementById('elliptic_ssid_5g_2').title = "5 GHz-2: \n"+ssid_status_5g_2;
	}

	if(smart_connect_support){
		if('<% nvram_get("smart_connect_x"); %>' == '1'){
			document.getElementById('elliptic_ssid_2g').title = "Smart Connect: \n"+ssid_status_2g;
			document.getElementById('elliptic_ssid_5g').style.display = "none";
			document.getElementById('elliptic_ssid_5g_2').style.display = "none";
		}else if('<% nvram_get("smart_connect_x"); %>' == '2'){
			document.getElementById('elliptic_ssid_2g').title = "2.4 GHz: \n"+ssid_status_2g;
			document.getElementById('elliptic_ssid_5g').title = "5 GHz Smart Connect: \n"+ssid_status_5g;
			document.getElementById('elliptic_ssid_5g').style.display = "";
			document.getElementById('elliptic_ssid_5g_2').style.display = "none";
		}else{
			document.getElementById('elliptic_ssid_2g').title = "2.4 GHz: \n"+ssid_status_2g;
			document.getElementById('elliptic_ssid_5g').style.display = "";
			document.getElementById('elliptic_ssid_5g_2').style.display = "";
		}
        }

	var swpjverno = '<% nvram_get("swpjverno"); %>';
	var buildno = '<% nvram_get("buildno"); %>';
	var firmver = '<% nvram_get("firmver"); %>'
	var extendno = '<% nvram_get("extendno"); %>';

	if(swpjverno == ''){
		if ((extendno == "") || (extendno == "0"))
			showtext(document.getElementById("firmver"), buildno);
		else
			showtext(document.getElementById("firmver"), buildno + '_' + extendno.split("-g")[0]);
	}
	else{
		showtext(document.getElementById("firmver"), swpjverno + '_' + extendno);
	}


	// no_op_mode
	if (!dsl_support) {
		if(sw_mode == "1")  // Show operation mode in banner, Viz 2011.11
			document.getElementById("sw_mode_span").innerHTML = "<#wireless_router#>";
		else if(sw_mode == "2"){
			if(wlc_express == 1)
				document.getElementById("sw_mode_span").innerHTML = "Express Way (2.4GHz)";
			else if(wlc_express == 2)
				document.getElementById("sw_mode_span").innerHTML = "Express Way (5GHz)";
			else
				document.getElementById("sw_mode_span").innerHTML = "<#OP_RE_item#>";
		}
		else if(sw_mode == "3")
			document.getElementById("sw_mode_span").innerHTML = "<#OP_AP_item#>";
		else if(sw_mode == "4")
			document.getElementById("sw_mode_span").innerHTML = "<#OP_MB_item#>";
		else
			document.getElementById("sw_mode_span").innerHTML = "Unknown";	

		if(hwmodeSwitch_support){	
			document.getElementById("op_link").innerHTML = document.getElementById("sw_mode_span").innerHTML;	
			document.getElementById("op_link").style.cursor= "auto";			
		}
	}
}

function extend_display_ssid(ssid){		//"&amp;"5&; "&lt;"4< ; "&gt;"4> ; "&nbsp;"6space
		//alert(ssid.substring(0,35)+" : "+ssid.substring(0,35).lastIndexOf("&nbsp;")+" <&nbsp>");
		if(ssid.substring(0,35).lastIndexOf("&nbsp;") >=30 || ssid.substring(0,35).lastIndexOf("&nbsp;") < 25 ){
				//alert(ssid.substring(0,34)+" : "+ssid.substring(0,34).lastIndexOf("&amp;")+" <&amp>");
				if(ssid.substring(0,34).lastIndexOf("&amp;") >=30 || ssid.substring(0,34).lastIndexOf("&amp;") < 26 ){
						//alert(ssid.substring(0,33)+" : "+ssid.substring(0,33).lastIndexOf("&lt;")+" <&lt>");
						if(ssid.substring(0,33).lastIndexOf("&lt;") >=30 || ssid.substring(0,34).lastIndexOf("&lt;") < 27 ){
								//alert(ssid.substring(0,33)+" : "+ssid.substring(0,33).lastIndexOf("&gt;")+" <&gt>");
								if(ssid.substring(0,33).lastIndexOf("&gt;") >=30 || ssid.substring(0,34).lastIndexOf("&gt;") < 27 ){
										//alert(ssid.substring(0,30));
										return ssid.substring(0,30);
								}
								else{
										switch(ssid.substring(0,33).lastIndexOf("&gt;")){
												case 27:
																	return ssid.substring(0,31);
																	break;
												case 28:
																	return ssid.substring(0,32);	
																	break;
												case 29:
																	return ssid.substring(0,33);		
																	break;	
										}
								}
						}
						else{
									switch(ssid.substring(0,33).lastIndexOf("&lt;")){
											case 27:
																return ssid.substring(0,31);
																break;
											case 28:
																return ssid.substring(0,32);
																break;
											case 29:
																return ssid.substring(0,33);
																break;

									}			
						}
				}
				else{
							switch (ssid.substring(0,34).lastIndexOf("&amp;")){
									case 26:
														return ssid.substring(0,31);
														break;
									case 27:
														return ssid.substring(0,32);
														break;
									case 28:
														return ssid.substring(0,33);
														break;
									case 29:
														return ssid.substring(0,19);
														break;

							}
				}
		}
		else{
					switch (ssid.substring(0,35).lastIndexOf("&nbsp;")){
									case 25:
														return ssid.substring(0,31);
														break;			
									case 26:
														return ssid.substring(0,32);
														break;
									case 27:
														return ssid.substring(0,33);
														break;
									case 28:
														return ssid.substring(0,34);
														break;
									case 29:
														return ssid.substring(0,35);
														break;					
					}
		}
}

function go_setting(page){
	if(tmo_support && isMobile()){
		location.href = "/MobileQIS_Login.asp";
	}
	else{
		location.href = page;
	}
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
	document.getElementById('systemtime').innerHTML ="<a href='/Advanced_System_Content.asp'>" + JS_timeObj3 + "</a>"; 
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
	var obj = document.getElementById("Loading");
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

// display selected language in language bar, Viz modified at 2013/03/22
function show_selected_language(){
	switch(document.getElementById("preferred_lang").value){
		case 'EN':{
			document.getElementById('selected_lang').innerHTML = "English";
			break;
		}
		case 'CN':{
			document.getElementById('selected_lang').innerHTML = "简体中文";
			break;
		}	
		case 'TW':{
			document.getElementById('selected_lang').innerHTML = "繁體中文";
			break;
		}
		case 'BR':{
			document.getElementById('selected_lang').innerHTML = "Portuguese(Brazil)&nbsp&nbsp";
			break;
		}
		case 'CZ':{
			document.getElementById('selected_lang').innerHTML = "Česky";
			break;
		}
		case 'DA':{
			document.getElementById('selected_lang').innerHTML = "Dansk";
			break;
		}	
		case 'DE':{
			document.getElementById('selected_lang').innerHTML = "Deutsch";
			break;
		}
		case 'ES':{
			document.getElementById('selected_lang').innerHTML = "Español";
			break;
		}
		case 'FI':{
			document.getElementById('selected_lang').innerHTML = "Suomi";
			break;
		}
		case 'FR':{
			document.getElementById('selected_lang').innerHTML = "Français";
			break;
		}
		case 'HU':{
			document.getElementById('selected_lang').innerHTML = "Hungarian";
			break;
		}
		case 'IT':{
			document.getElementById('selected_lang').innerHTML = "Italiano";
			break;
		}		
		case 'JP':{
			document.getElementById('selected_lang').innerHTML = "日本語";
			break;
		}
		case 'KR':{
			document.getElementById('selected_lang').innerHTML = "한국어";
                        break;
		}
		case 'MS':{
			document.getElementById('selected_lang').innerHTML = "Malay";
			break;
		}
		case 'NO':{
			document.getElementById('selected_lang').innerHTML = "Norsk";
			break;
		}
		case 'PL':{
			document.getElementById('selected_lang').innerHTML = "Polski";
			break;
		}
		case 'RO':{
			document.getElementById('selected_lang').innerHTML = "Romanian";
			break;
		}
		case 'RU':{
			document.getElementById('selected_lang').innerHTML = "Pусский";
			break;
		}
		case 'SV':{
			document.getElementById('selected_lang').innerHTML = "Svensk";
			break;
		}		
		case 'TH':{
			document.getElementById('selected_lang').innerHTML = "ไทย";
			break;
		}			
		case 'TR':{
			document.getElementById('selected_lang').innerHTML = "Türkçe";
			break;
		}	
		case 'UK':{
			document.getElementById('selected_lang').innerHTML = "Український";
			break;
		}		
	}
}


function submit_language(obj){
	if(obj.id != document.getElementById("preferred_lang").value){
		showLoading();
		
		with(document.titleForm){
			action = "/start_apply.htm";
			
			if(location.pathname == "/")
				current_page.value = "/index.asp";
			else
				current_page.value = location.pathname;
				
			preferred_lang.value = obj.id;
			//preferred_lang.value = document.getElementById("select_lang").value;
			flag.value = "set_language";
			
			submit();
		}
	}
	else
		alert("No change LANGUAGE!");
}

function change_language(){
	if(document.getElementById("select_lang").value != document.getElementById("preferred_lang").value)
		document.getElementById("change_lang_btn").disabled = false;
	else
		document.getElementById("change_lang_btn").disabled = true;
}

function logout(){
	if(confirm('<#JS_logout#>')){
		setTimeout('location = "Logout.asp";', 1);
	}
}

function reboot(){
	if(confirm("<#Main_content_Login_Item7#>")){
		document.rebootForm.submit();
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

function showtext2(obj, str, visible){
	if(obj){
		obj.innerHTML = str;
		obj.style.display = (visible) ? "" : "none";
	}
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
		document.getElementById(obj_id).style.display = state;
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
	|| current_url.indexOf("Advanced_WPasspoint_Content") == 0
	|| current_url.indexOf("Advanced_PerformanceTuning_Content") == 0
	|| current_url.indexOf("Advanced_Modem_Content") == 0
	|| current_url.indexOf("QIS_modem") == 0
	|| current_url.indexOf("Advanced_IPv6_Content") == 0
	|| current_url.indexOf("Advanced_WAdvanced_Content") == 0
	|| current_url.indexOf("Advanced_IPTV_Content") == 0
	|| current_url.indexOf("Advanced_WANPort_Content.asp") == 0
	|| current_url.indexOf("Advanced_ASUSDDNS_Content.asp") == 0
	|| current_url.indexOf("Advanced_DSL_Content.asp") == 0
	|| current_url.indexOf("Advanced_VDSL_Content.asp") == 0
	|| current_url.indexOf("Advanced_SwitchCtrl_Content.asp") == 0
	|| current_url.indexOf("router.asp") == 0
	|| current_url.indexOf("Advanced_MobileBroadband_Content") == 0
	|| current_url.indexOf("Advanced_Feedback") == 0
	){
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

function hadPlugged(deviceType){
	if(allUsbStatusArray.join().search(deviceType) != -1)
		return true;

	return false;
}

//Update current system status
var AUTOLOGOUT_MAX_MINUTE = parseInt('<% nvram_get("http_autologout"); %>') * 20;
function updateStatus(){
	if(stopFlag == 1) return false;
	if(AUTOLOGOUT_MAX_MINUTE == 1) location = "Logout.asp"; // 0:disable auto logout, 1:trigger auto logout. 

	require(['/require/modules/makeRequest.js'], function(makeRequest){
		if(AUTOLOGOUT_MAX_MINUTE != 0) AUTOLOGOUT_MAX_MINUTE--;
		makeRequest.start('/ajax_status.xml', refreshStatus, function(){stopFlag = 1;});
	});
}

var link_status;
var link_auxstatus;
var link_sbstatus;
var ddns_return_code = '<% nvram_get("ddns_return_code_chk");%>';
var ddns_updated = '<% nvram_get("ddns_updated");%>';
var vpnc_state_t = '';
var vpnc_sbstate_t = '';
var vpn_clientX_errno = '';
var vpnc_proto = '<% nvram_get("vpnc_proto");%>';
var vpnd_state;	
var vpnc_state_t1 = '';
var vpnc_state_t2 = '';
var vpnc_state_t3 = '';
var vpnc_state_t4 = '';
var vpnc_state_t5 = '';
var vpnc_errno_t1 = '';
var vpnc_errno_t2 = '';
var vpnc_errno_t3 = '';
var vpnc_errno_t4 = '';
var vpnc_errno_t5 = '';
var qtn_state_t = '';

//for mobile broadband
var sim_signal = '<% nvram_get("usb_modem_act_signal"); %>';
var sim_operation = '<% nvram_get("usb_modem_act_operation"); %>';
var sim_state = '<% nvram_get("usb_modem_act_sim"); %>';
var sim_isp = '<% nvram_get("modem_isp"); %>';
var sim_spn = '<% nvram_get("modem_spn"); %>';
var roaming = '<% nvram_get("modem_roaming"); %>';
var roaming_imsi = '<% nvram_get("modem_roaming_imsi"); %>';
var sim_imsi = '<% nvram_get("usb_modem_act_imsi"); %>';
var g3err_pin = '<% nvram_get("g3err_pin"); %>';
var pin_remaining_count = '<% nvram_get("usb_modem_act_auth_pin"); %>';
var usbState;
var usb_state = -1;
var usb_sbstate = -1;
var usb_auxstate = -1;	
var first_link_status = '';
var first_link_sbstatus = '';
var first_link_auxstatus = '';
var secondary_link_status = '';
var secondary_link_sbstatus = '';
var secondary_link_auxstatus = '';
var modem_bytes_data_limit = parseFloat('<% nvram_get("modem_bytes_data_limit"); %>');
var rx_bytes = parseFloat('<% nvram_get("modem_bytes_rx"); %>');
var tx_bytes = parseFloat('<% nvram_get("modem_bytes_tx"); %>');
var traffic_warning_cookie = '';
var traffic_warning_flag = '';
var keystr = 'traffic_warning_' + modem_bytes_data_limit;
var date = new Date();
var date_year = date.getFullYear();
var date_month = date.getMonth();
var modem_enable = '';
var modem_sim_order = '';
var wanConnectStatus = true;

function refreshStatus(xhr){
	if(xhr.responseText.search("Main_Login.asp") !== -1) top.location.href = "/index.asp";

	setTimeout(function(){updateStatus();}, 3000);	/* restart ajax */
	var devicemapXML = xhr.responseXML.getElementsByTagName("devicemap");
	var wanStatus = devicemapXML[0].getElementsByTagName("wan");
	link_status = wanStatus[0].firstChild.nodeValue;
	link_sbstatus = wanStatus[1].firstChild.nodeValue;
	link_auxstatus = wanStatus[2].firstChild.nodeValue;

	monoClient = wanStatus[3].firstChild.nodeValue;	
	_wlc_state = wanStatus[4].firstChild.nodeValue;
	_wlc_sbstate = wanStatus[5].firstChild.nodeValue;	
	_wlc_auth = wanStatus[6].firstChild.nodeValue;	
	wifi_hw_switch = wanStatus[7].firstChild.nodeValue;
	ddns_return_code = wanStatus[8].firstChild.nodeValue.replace("ddnsRet=", "");
	ddns_updated = wanStatus[9].firstChild.nodeValue.replace("ddnsUpdate=", "");
	wan_line_state = wanStatus[10].firstChild.nodeValue.replace("wan_line_state=", "");	
	wlan0_radio_flag = wanStatus[11].firstChild.nodeValue.replace("wlan0_radio_flag=", "");
	wlan1_radio_flag = wanStatus[12].firstChild.nodeValue.replace("wlan1_radio_flag=", "");
	data_rate_info_2g = wanStatus[13].firstChild.nodeValue.replace("data_rate_info_2g=", "");
	data_rate_info_5g = wanStatus[14].firstChild.nodeValue.replace("data_rate_info_5g=", "");
	data_rate_info_5g_2 = wanStatus[15].firstChild.nodeValue.replace("data_rate_info_5g_2=", "");
	wan_diag_state = wanStatus[16].firstChild.nodeValue.replace("wan_diag_state=", "");
	active_wan_unit = wanStatus[17].firstChild.nodeValue.replace("active_wan_unit=", "");
	wan0_enable = wanStatus[18].firstChild.nodeValue.replace("wan0_enable=", "");
	wan1_enable = wanStatus[19].firstChild.nodeValue.replace("wan1_enable=", "");
	wan0_realip_state = wanStatus[20].firstChild.nodeValue.replace("wan0_realip_state=", "");
	wan1_realip_state = wanStatus[21].firstChild.nodeValue.replace("wan1_realip_state=", "");
	wan0_ipaddr = wanStatus[22].firstChild.nodeValue.replace("wan0_ipaddr=", "");
	wan1_ipaddr = wanStatus[23].firstChild.nodeValue.replace("wan1_ipaddr=", "");
	wan0_realip_ip = wanStatus[24].firstChild.nodeValue.replace("wan0_realip_ip=", "");
	wan1_realip_ip = wanStatus[25].firstChild.nodeValue.replace("wan1_realip_ip=", "");

	var vpnStatus = devicemapXML[0].getElementsByTagName("vpn");

	var first_wanStatus = devicemapXML[0].getElementsByTagName("first_wan");
	first_link_status = first_wanStatus[0].firstChild.nodeValue;
	first_link_sbstatus = first_wanStatus[1].firstChild.nodeValue;
	first_link_auxstatus = first_wanStatus[2].firstChild.nodeValue;
	var secondary_wanStatus = devicemapXML[0].getElementsByTagName("second_wan");
	secondary_link_status = secondary_wanStatus[0].firstChild.nodeValue;
	secondary_link_sbstatus = secondary_wanStatus[1].firstChild.nodeValue;
	secondary_link_auxstatus = secondary_wanStatus[2].firstChild.nodeValue;

	var qtn_state = devicemapXML[0].getElementsByTagName("qtn");
	qtn_state_t = qtn_state[0].firstChild.nodeValue.replace("qtn_state=", "");

	var usbStatus = devicemapXML[0].getElementsByTagName("usb");
	allUsbStatus = usbStatus[0].firstChild.nodeValue.toString();
	modem_enable = usbStatus[1].firstChild.nodeValue.replace("modem_enable=", "");

	var simState = devicemapXML[0].getElementsByTagName("sim");
	sim_state = simState[0].firstChild.nodeValue.replace("sim_state=", "");
	sim_signal = simState[1].firstChild.nodeValue.replace("sim_signal=", "");	
	sim_operation = simState[2].firstChild.nodeValue.replace("sim_operation=", "");	
	sim_isp = simState[3].firstChild.nodeValue.replace("sim_isp=", "");
	roaming = simState[4].firstChild.nodeValue.replace("roaming=", "");
	roaming_imsi = simState[5].firstChild.nodeValue.replace("roaming_imsi=", "");
	sim_imsi = simState[6].firstChild.nodeValue.replace("sim_imsi=", "");			
	g3err_pin = simState[7].firstChild.nodeValue.replace("g3err_pin=", "");		
	pin_remaining_count = simState[8].firstChild.nodeValue.replace("pin_remaining_count=", "");		
	sim_spn = simState[9].firstChild.nodeValue.replace("sim_spn=", "");
	rx_bytes = parseFloat(simState[10].firstChild.nodeValue.replace("rx_bytes=", ""));
	tx_bytes = parseFloat(simState[11].firstChild.nodeValue.replace("tx_bytes=", ""));
	modem_sim_order = parseFloat(simState[12].firstChild.nodeValue.replace("modem_sim_order=", ""));	

	vpnc_proto = vpnStatus[0].firstChild.nodeValue.replace("vpnc_proto=", "");
	if(vpnc_support){
		vpnc_state_t1 = vpnStatus[3].firstChild.nodeValue.replace("vpn_client1_state=", "");
		vpnc_errno_t1 = vpnStatus[9].firstChild.nodeValue.replace("vpn_client1_errno=", "");
		vpnc_state_t2 = vpnStatus[4].firstChild.nodeValue.replace("vpn_client2_state=", "");
		vpnc_errno_t2 = vpnStatus[10].firstChild.nodeValue.replace("vpn_client2_errno=", "");
		vpnc_state_t3 = vpnStatus[5].firstChild.nodeValue.replace("vpn_client3_state=", "");
		vpnc_errno_t3 = vpnStatus[11].firstChild.nodeValue.replace("vpn_client3_errno=", "");
		vpnc_state_t4 = vpnStatus[6].firstChild.nodeValue.replace("vpn_client4_state=", "");
		vpnc_errno_t4 = vpnStatus[12].firstChild.nodeValue.replace("vpn_client4_errno=", "");
		vpnc_state_t5 = vpnStatus[7].firstChild.nodeValue.replace("vpn_client5_state=", "");
		vpnc_errno_t5 = vpnStatus[13].firstChild.nodeValue.replace("vpn_client5_errno=", "");
		vpnc_state_t = vpnStatus[1].firstChild.nodeValue.replace("vpnc_state_t=", "");//vpnc (pptp/l2tp)
	}

	vpnc_sbstate_t = vpnStatus[2].firstChild.nodeValue.replace("vpnc_sbstate_t=", "");

	if('<% nvram_get("vpn_server_unit"); %>' == 1)
		vpnd_state = vpnStatus[14].firstChild.nodeValue.replace("vpn_server1_state=", "");
	else    //unit 2
		vpnd_state = vpnStatus[15].firstChild.nodeValue.replace("vpn_server2_state=", "");

	if(location.pathname == "/"+ QISWIZARD)
		return false;	
	else if(location.pathname == "/Advanced_VPNClient_Content.asp")
		show_vpnc_rulelist();
	else if(location.pathname == "/Advanced_Feedback.asp")
		updateUSBStatus();
	
	//Adaptive QoS mode	
	if(bwdpi_support && qos_enable_flag && qos_type_flag == "1"){
		if(bwdpi_app_rulelist == "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<game")
			document.getElementById("bwdpi_status").className = "bwdpistatus_game";			
		else if(bwdpi_app_rulelist == "9,20<4<0,5,6,15,17<8<13,24<1,3,14<7,10,11,21,23<<media")
			document.getElementById("bwdpi_status").className = "bwdpistatus_media";
		else if(bwdpi_app_rulelist == "9,20<13,24<4<0,5,6,15,17<8<1,3,14<7,10,11,21,23<<web")
			document.getElementById("bwdpi_status").className = "bwdpistatus_web";
		else
			document.getElementById("bwdpi_status").className = "bwdpistatus_customize";			
		
		document.getElementById("bwdpi_status").onclick = function(){openHint(24,9);}
		document.getElementById("bwdpi_status").onmouseover = function(){overHint("A");}
		document.getElementById("bwdpi_status").onmouseout = function(){nd();}
	}

	// internet
	if(sw_mode == 1){
		//Viz add 2013.04 for dsl sync status
		if(dsl_support){
				if(wan_diag_state == "1" && allUsbStatus.search("storage") >= 0){
						document.getElementById("adsl_line_status").className = "linestatusdiag";
						document.getElementById("adsl_line_status").onclick = function(){openHint(24,8);}
				}else if(wan_line_state == "up"){
						document.getElementById("adsl_line_status").className = "linestatusup";
						document.getElementById("adsl_line_status").onclick = function(){openHint(24,6);}
				}else if(wan_line_state == "wait for init"){
						document.getElementById("adsl_line_status").className = "linestatuselse";
				}else if(wan_line_state == "init" || wan_line_state == "initializing"){
						document.getElementById("adsl_line_status").className = "linestatuselse";
				}else{
						document.getElementById("adsl_line_status").className = "linestatusdown";
				}
				document.getElementById("adsl_line_status").onmouseover = function(){overHint(9);}
				document.getElementById("adsl_line_status").onmouseout = function(){nd();}
		}

		if((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2")){
			document.getElementById("connect_status").className = "connectstatuson";
			if(location.pathname == "/" || location.pathname == "/index.asp"){
				document.getElementById("NM_connect_status").innerHTML = "<#Connected#>";
				document.getElementById('single_wan').className = "single_wan_connected";
			}	
			wanConnectStatus = true;
		}
		else if(dualwan_enabled &&
				((first_link_status == "2" && first_link_auxstatus == "0") || (first_link_status == "2" && first_link_auxstatus == "2")) ||
				((secondary_link_status == "2" && secondary_link_auxstatus == "0") || (secondary_link_status == "2" && secondary_link_auxstatus == "2"))){
			document.getElementById("connect_status").className = "connectstatuson";
			if(location.pathname == "/" || location.pathname == "/index.asp"){
				document.getElementById("NM_connect_status").innerHTML = "<#Connected#>";
				document.getElementById('single_wan').className = "single_wan_connected";
			}
			wanConnectStatus = true;
		}
		else{
			document.getElementById("connect_status").className = "connectstatusoff";
			if(location.pathname == "/" || location.pathname == "/index.asp"){
				document.getElementById("NM_connect_status").innerHTML = '<a style="color:#FFF;text-decoration:underline;" href="/'+ QISWIZARD +'?flag=detect"><#Disconnected#></a>';
				document.getElementById('single_wan').className = "single_wan_disconnected";
				document.getElementById("wanIP_div").style.display = "none";		
			}
			wanConnectStatus = false;
		}

		document.getElementById("connect_status").onclick = function(){openHint(24,3);}
		document.getElementById("connect_status").onmouseover = function(){overHint(3);}
		document.getElementById("connect_status").onmouseout = function(){nd();}
	}
	else if(sw_mode == 2 || sw_mode == 4){
		if(sw_mode == 4 || (sw_mode == 2 && new_repeater)){
			if(_wlc_auth.search("wlc_state=1") != -1 && _wlc_auth.search("wlc_state_auth=0") != -1)
				_wlc_state = "wlc_state=2";
			else
				_wlc_state = "wlc_state=0";
		}

		if(_wlc_state == "wlc_state=2"){
			document.getElementById("connect_status").className = "connectstatuson";
			document.getElementById("connect_status").onclick = function(){openHint(24,3);}
			if(location.pathname == "/" || location.pathname == "/index.asp"){
				document.getElementById("NM_connect_status").innerHTML = "<#Connected#>";
				document.getElementById('single_wan').className = "single_wan_connected";
			}
			wanConnectStatus = true;
		}
		else{
			document.getElementById("connect_status").className = "connectstatusoff";
			if(location.pathname == "/" || location.pathname == "/index.asp"){
				document.getElementById("NM_connect_status").innerHTML = "<#Disconnected#>";
				document.getElementById('single_wan').className = "single_wan_disconnected";
			}
			wanConnectStatus = false;
		}
		document.getElementById("connect_status").onmouseover = function(){overHint(3);}
		document.getElementById("connect_status").onmouseout = function(){nd();}
		
		if(location.pathname == "/" || location.pathname == "/index.asp"){
			if(wlc_band == 0)		// show repeater and media bridge date rate
				var speed_info = data_rate_info_2g;	
			else if (wlc_band == 1)
				var speed_info = data_rate_info_5g;
			else if (wlc_band == 2)
				var speed_info = data_rate_info_5g_2;
			
			document.getElementById('speed_status').innerHTML = speed_info;
		}	
	}

	// wifi hw sw status
	if(wifi_hw_sw_support){
		if(band5g_support){
			if(wlan0_radio_flag == "0" && wlan1_radio_flag == "0"){
					document.getElementById("wifi_hw_sw_status").className = "wifihwswstatusoff";
					document.getElementById("wifi_hw_sw_status").onclick = function(){}
			}
			else{
					document.getElementById("wifi_hw_sw_status").className = "wifihwswstatuson";
					document.getElementById("wifi_hw_sw_status").onclick = function(){}
			}
		}
		else{
			if(wlan0_radio_flag == "0"){
					document.getElementById("wifi_hw_sw_status").className = "wifihwswstatusoff";
					document.getElementById("wifi_hw_sw_status").onclick = function(){}
			}
			else{
					document.getElementById("wifi_hw_sw_status").className = "wifihwswstatuson";
					document.getElementById("wifi_hw_sw_status").onclick = function(){}
			}
		}
	}
	else	// No HW switch - reflect actual radio states
	{
		<%radio_status();%>

		if (!band5g_support)
			radio_5 = radio_2;

		if (wl_info.band5g_2_support)
			radio_5b = '<% nvram_get("wl2_radio"); %>';
		else
			radio_5b = radio_5;

		if (radio_2 && radio_5 && parseInt(radio_5b))
		{
			document.getElementById("wifi_hw_sw_status").className = "wifihwswstatuson";
			document.getElementById("wifi_hw_sw_status").onclick = function(){}
		}
		else if (radio_2 || radio_5 || parseInt(radio_5b))
		{
			document.getElementById("wifi_hw_sw_status").className = "wifihwswstatuspartial";  
			document.getElementById("wifi_hw_sw_status").onclick = function(){}
		}
		else
		{
			document.getElementById("wifi_hw_sw_status").className = "wifihwswstatusoff"; 
			document.getElementById("wifi_hw_sw_status").onclick = function(){}
		}
		document.getElementById("wifi_hw_sw_status").onmouseover = function(){overHint(8);}
		document.getElementById("wifi_hw_sw_status").onmouseout = function(){nd();}
	}

	// usb.storage
	if(usb_support){
		if(allUsbStatus != allUsbStatusTmp && allUsbStatusTmp != ""){
			if(current_url=="index.asp"||current_url=="")
				location.href = "/index.asp";
		}

	 	require(['/require/modules/diskList.js'], function(diskList){
	 		var usbDevicesList = diskList.list();
	 		var index = 0, find_nonprinter = 0, find_storage = 0, find_modem = 0;

			for(index = 0; index < usbDevicesList.length; index++){
				if(usbDevicesList[index].deviceType != "printer"){
					find_nonprinter = 1;
				}
				if(usbDevicesList[index].deviceType == "storage"){
					find_storage = 1;
				}
				if(usbDevicesList[index].deviceType == "modem"){
					find_modem = 1;
				}
				if(find_nonprinter && find_storage && find_modem)
					break;
			}

			if(find_nonprinter){
				document.getElementById("usb_status").className = "usbstatuson";		
			}
			else{
				document.getElementById("usb_status").className = "usbstatusoff";		
			}

			if(find_storage){
				document.getElementById("usb_status").onclick = function(){openHint(24,2);}				
			}
			else if(find_modem){
				document.getElementById("usb_status").onclick = function(){openHint(24,7);}	
			}			
			else{
				document.getElementById("usb_status").onclick = function(){overHint(2);}			
			}

			document.getElementById("usb_status").onmouseover = function(){overHint(2);}
			document.getElementById("usb_status").onmouseout = function(){nd();}

			allUsbStatusTmp = allUsbStatus;
		});
	}

	// usb.printer
	if(printer_support){
		if(allUsbStatus.search("printer") == -1){
			document.getElementById("printer_status").className = "printstatusoff";
			document.getElementById("printer_status").parentNode.style.display = "none";
			document.getElementById("printer_status").onmouseover = function(){overHint(5);}
			document.getElementById("printer_status").onmouseout = function(){nd();}
		}
		else{
			document.getElementById("printer_status").className = "printstatuson";
			document.getElementById("printer_status").parentNode.style.display = "";
			document.getElementById("printer_status").onmouseover = function(){overHint(6);}
			document.getElementById("printer_status").onmouseout = function(){nd();}
			document.getElementById("printer_status").onclick = function(){openHint(24,1);}
		}
	}

	// guest network
	if(multissid_support != -1 && (gn_array_5g.length > 0 || (wl_info.band5g_2_support && gn_array_5g_2.length > 0))){
		if(based_modelid == "RT-AC87U"){	//workaround for RT-AC87U
			for(var i=0; i<gn_array_2g.length; i++){
				if(gn_array_2g[i][0] == 1){
					document.getElementById("guestnetwork_status").className = "guestnetworkstatuson";
					document.getElementById("guestnetwork_status").onclick = function(){openHint(24,4);}
					break;
				}
				else{
					document.getElementById("guestnetwork_status").className = "guestnetworkstatusoff";
					document.getElementById("guestnetwork_status").onclick = function(){overHint(4);}
				}
			}
			
			for(var i=0; i<gn_array_5g.length; i++){
				if(gn_array_2g[i][0] == 1 || gn_array_5g[i][0] == 1){
					document.getElementById("guestnetwork_status").className = "guestnetworkstatuson";
					document.getElementById("guestnetwork_status").onclick = function(){openHint(24,4);}
					break;
				}
				else{
					document.getElementById("guestnetwork_status").className = "guestnetworkstatusoff";
					document.getElementById("guestnetwork_status").onclick = function(){overHint(4);}
				}
			}
		}else{
			for(var i=0; i<gn_array_2g.length; i++){
				if(gn_array_2g[i][0] == 1 || gn_array_5g[i][0] == 1 || (wl_info.band5g_2_support && gn_array_5g_2[i][0] == 1)){
					document.getElementById("guestnetwork_status").className = "guestnetworkstatuson";
					document.getElementById("guestnetwork_status").onclick = function(){openHint(24,4);}
					break;
				}
				else{
					document.getElementById("guestnetwork_status").className = "guestnetworkstatusoff";
					document.getElementById("guestnetwork_status").onclick = function(){overHint(4);}
				}
			}
		}
		
		document.getElementById("guestnetwork_status").onmouseover = function(){overHint(4);}
		document.getElementById("guestnetwork_status").onmouseout = function(){nd();}
		
	}
	else if(multissid_support != -1 && gn_array_5g.length == 0){
		for(var i=0; i<gn_array_2g.length; i++){
			if(gn_array_2g[i][0] == 1){
				document.getElementById("guestnetwork_status").className = "guestnetworkstatuson";
				document.getElementById("guestnetwork_status").onclick = function(){openHint(24,4);}
				break;
			}
			else{
				document.getElementById("guestnetwork_status").className = "guestnetworkstatusoff";
			}
		}
		document.getElementById("guestnetwork_status").onmouseover = function(){overHint(4);}
		document.getElementById("guestnetwork_status").onmouseout = function(){nd();}
	}

	if(cooler_support){
		if(cooler == "cooler=2"){
			document.getElementById("cooler_status").className = "coolerstatusoff";
			document.getElementById("cooler_status").onclick = function(){}
		}
		else{
			document.getElementById("cooler_status").className = "coolerstatuson";
			document.getElementById("cooler_status").onclick = function(){openHint(24,5);}
		}
		document.getElementById("cooler_status").onmouseover = function(){overHint(7);}
		document.getElementById("cooler_status").onmouseout = function(){nd();}
	}

	if(gobi_support && (usb_index != -1) && (sim_state != ""))//Cherry Cho added in 2014/8/25.
	{//Change icon according to status
		if(usb_index == 0)
			usbState = devicemapXML[0].getElementsByTagName("first_wan");
		else if(usb_index == 1)
			usbState = devicemapXML[0].getElementsByTagName("second_wan");
		usb_state = usbState[0].firstChild.nodeValue;
		usb_sbstate = usbState[1].firstChild.nodeValue;
		usb_auxstate = usbState[2].firstChild.nodeValue;

		if(roaming == "1"){
			if(usb_state == 2 && usb_sbstate == 0 && usb_auxstate == 0){
				if(roaming_imsi.length > 0 && roaming_imsi != sim_imsi.substr(0, roaming_imsi.length))
					document.getElementById("simroaming_status").className = "simroamingon";				
			}
		}

		document.getElementById("simsignal").onmouseover = function(){overHint(98)};
		document.getElementById("simsignal").onmouseout = function(){nd();}
		if( sim_state == '1'){			
			switch(sim_signal)
			{
				case '0':
					document.getElementById("simsignal").className = "simsignalno";
					break;
				case '1':
					document.getElementById("simsignal").className = "simsignalmarginal";	
					break;
				case '2':
					document.getElementById("simsignal").className = "simsignalok";	
					break;
				case '3':
					document.getElementById("simsignal").className = "simsignalgood";	
					break;	
				case '4':
					document.getElementById("simsignal").className = "simsignalexcellent";	
					break;											
				case '5':
					document.getElementById("simsignal").className = "simsignalfull";	
					break;	
				default:
					document.getElementById("simsignal").className = "simsignalno";
					break;
			}

			if(parseInt(sim_signal) > 0 && (usb_state == 2 && usb_sbstate == 0 && usb_auxstate == 0)){
				switch(sim_operation)
				{
					case 'Edge':
						document.getElementById("signalsys").innerHTML  = '<img src="/images/mobile/E.png">';
						break;
					case 'GPRS':
						document.getElementById("signalsys").innerHTML = '<img src="/images/mobile/G.png">';	
						break;
					case 'WCDMA':
					case 'CDMA':
					case 'EV-DO REV 0':	
					case 'EV-DO REV A':		
					case 'EV-DO REV B':
						document.getElementById("signalsys").innerHTML = '<img src="/images/mobile/3G.png">';	
							break;	
					case 'HSDPA':										
					case 'HSUPA':
						document.getElementById("signalsys").innerHTML = '<img src="/images/mobile/H.png">';	
						break;	
					case 'HSDPA+':										
					case 'DC-HSDPA+':
						document.getElementById("signalsys").innerHTML = '<img src="/images/mobile/H+.png">';	
						break;		
					case 'LTE':
						document.getElementById("signalsys").innerHTML = '<img src="/images/mobile/LTE.png">';	
						break;		
					case 'GSM':	
					default:
						document.getElementById("signalsys").innerHTML = "";
						break;
				}
			}
		}
		else{
			document.getElementById("simsignal").className = "simsignalno";
			document.getElementById("signalsys").innerHTML = "";
		}
	}	

	if(((modem_support && hadPlugged("modem")) || gobi_support) && (usb_index != -1) && (sim_state != "")){
		document.getElementById("sim_status").onmouseover = function(){overHint(99)};
		document.getElementById("sim_status").onmouseout = function(){nd();}	
		switch(sim_state)
		{
			case '-1':
				document.getElementById("sim_status").className = "simnone";	
				break;
			case '1':
				document.getElementById("sim_status").className = "simexist";			
				break;
			case '2':
			case '4':
				document.getElementById("sim_status").className = "simlock";
				document.getElementById("sim_status").onclick = function(){openHint(24,7);}
				break;
			case '3':
			case '5':
				document.getElementById("sim_status").className = "simfail";
				document.getElementById("sim_status").onclick = function(){openHint(24,7);}	
				break;
			case '6':
			case '-2':
			case '-10':
				document.getElementById("sim_status").className = "simfail";					
				break;
			default:
				break;
		}
	}

	var data_usage = tx_bytes + rx_bytes;
	if(gobi_support && (usb_index != -1) && (sim_state != "") && (modem_bytes_data_limit > 0) && (data_usage >= modem_bytes_data_limit)){
		notification.array[12] = 'noti_mobile_traffic';
		notification.mobile_traffic = 1;
		notification.desc[12] = "<#Mobile_limit_warning#>";
		notification.action_desc[12] = "<#ASUSGATE_act_change#>";
		notification.clickCallBack[12] = "setTrafficLimit();";
	}
	else{
		notification.array[12] = 'off';
		notification.mobile_traffic = 0;
	}

	if(gobi_support && (usb_index != -1) && (sim_state != "") && (modem_sim_order == -1)){
		notification.array[13] = 'noti_sim_record';
		notification.sim_record = 1;
		notification.desc[13] = "<#Mobile_record_limit_warning#>";
		notification.action_desc[13] = "Delete now";
		notification.clickCallBack[13] = "upated_sim_record();";
	}
	else{
		notification.array[13] = 'off';
		notification.sim_record = 0;
	}
	
	if(realip_support){
		if(active_wan_unit == "0"){
			realip_state = wan0_realip_state;  //0: init/no act  1: can't get external IP  2: get external IP
			if(realip_state == "2"){
				realip_ip = wan0_realip_ip;
				external_ip = (realip_ip == wan0_ipaddr)? 1:0;
			}
			else{
				external_ip = -1;
			}
		}
		else if(active_wan_unit == "1"){
			realip_state = wan1_realip_state;  //0: init/no act  1: can't get external IP  2: get external IP
			if(realip_state == "2"){
				realip_ip = wan1_realip_ip;
				external_ip = (realip_ip == wan1_ipaddr)? 1:0;
			}
			else{
				external_ip = -1;
			}
		}
	}

	if(realip_support && !external_ip){
		notification.array[14] = 'noti_external_ip';
		notification.external_ip = 1;
		notification.desc[14] = "<#external_ip_warning#>";
		notification.action_desc[14] = "<#ASUSGATE_act_change#>";
		if(active_wan_unit == 0)
			notification.clickCallBack[14] = "goToWAN(0);";
		else if(active_wan_unit == 1)
			notification.clickCallBack[14] = "goToWAN(1);";
	}
	else{
		notification.array[14] = 'off';
		notification.external_ip = 0;
	}

	if(notification.stat != "on" && (notification.mobile_traffic || notification.sim_record || notification.external_ip)){
		notification.stat = "on";
		notification.flash = "on";
		notification.run();
	}
	else if(notification.stat == "on" && !notification.mobile_traffic && !notification.sim_record && !notification.external_ip && !notification.upgrade && !notification.wifi_2g && 
			!notification.wifi_5g && !notification.ftp && !notification.samba && !notification.loss_sync && !notification.experience_FB && !notification.notif_hint && !notification.mobile_traffic && 
			!notification.send_debug_log){
		cookie.unset("notification_history");
		clearInterval(notification.flashTimer);
		document.getElementById("notification_status").className = "notification_off";
	}
	
	if(window.frames["statusframe"] && window.frames["statusframe"].stopFlag == 1 || stopFlag == 1){
		return 0;
	}
}	

var notification = {
	stat: "off",
	flash: "off",
	flashTimer: 0,
	hoverText: "",
	clickText: "",
	array: [],
	desc: [],
	action_desc: [],
	upgrade: 0,
	wifi_2g: 0,
	wifi_5g: 0,
	ftp: 0,
	samba: 0,
	loss_sync: 0,
	experience_FB: 0,
	notif_hint: 0,
	mobile_traffic: 0,
	send_debug_log: 0,
	clicking: 0,
	sim_record: 0,
	external_ip: 0,
	redirectftp:function(){location.href = 'Advanced_AiDisk_ftp.asp';},
	redirectsamba:function(){location.href = 'Advanced_AiDisk_samba.asp';},
	redirectFeedback:function(){location.href = 'Advanced_Feedback.asp';},
	redirectFeedbackInfo:function(){location.href = 'Feedback_Info.asp';},
	redirectHint:function(){location.href = location.href;},
	clickCallBack: [],
	notiClick: function(){
		// stop flashing after the event is checked.
		cookie.set("notification_history", [notification.upgrade, notification.wifi_2g ,notification.wifi_5g ,notification.ftp ,notification.samba ,notification.loss_sync ,notification.experience_FB ,notification.notif_hint, notification.mobile_traffic, notification.send_debug_log, notification.sim_record, notification.external_ip].join(), 1000);
		clearInterval(notification.flashTimer);
		document.getElementById("notification_status").className = "notification_on";

		if(notification.clicking == 0){
			var txt = '<div id="notiDiv"><table width="100%">'

			for(i=0; i<notification.array.length; i++){
				if(notification.array[i] != null && notification.array[i] != "off"){
					txt += '<tr><td><table id="notiDiv_table3" width="100%" border="0" cellpadding="0" cellspacing="0" bgcolor="#232629">';
		  			txt += '<tr><td><table id="notiDiv_table5" border="0" cellpadding="5" cellspacing="0" bgcolor="#232629" width="100%">';
		  			txt += '<tr><td valign="TOP" width="100%"><div style="white-space:pre-wrap;font-size:13px;color:white;cursor:text">' + notification.desc[i] + '</div>';
		  			txt += '</td></tr>';

		  			if( i == 2 ){					  				
		  				txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></td></tr>';
		  				if(band5g_support && notification.array[3] != null && notification.array[i] != "off")
		  						txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i+1] + '">' + notification.action_desc[i+1] + '</div></td></tr>';
		  				notification.array[3] = "off";
		  			}
					else if( i == 9){
		  				txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></td></tr>';
							if(notification.array[10] != null && notification.array[10] != "off")
									txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i+1] + '">' + notification.action_desc[i+1] + '</div></td></tr>';
							notification.array[10] = "off";
					}
		  			else{
	  					txt += '<tr><td><table width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></table></td></tr>';
		  			}

		  			txt += '</table></td></tr></table></td></tr>'
	  			}
			}
			txt += '</table></div>';

			document.getElementById("notification_desc").innerHTML = txt;
			notification.clicking = 1;
		}else{
			document.getElementById("notification_desc").innerHTML = "";
			notification.clicking = 0;
		}
	},
	
	run: function(){
		var tarObj = document.getElementById("notification_status");
		var tarObj1 = document.getElementById("notification_status1");

		if(tarObj === null)	
			return false;		

		if(this.stat == "on"){
			tarObj1.onclick = this.notiClick;
			tarObj.className = "notification_on";
			tarObj1.className = "notification_on1";
		}

		if(this.flash == "on" && cookie.get("notification_history") != [notification.upgrade, notification.wifi_2g ,notification.wifi_5g ,notification.ftp ,notification.samba ,notification.loss_sync ,notification.experience_FB ,notification.notif_hint, notification.mobile_traffic, notification.send_debug_log, notification.sim_record, notification.external_ip].join()){
			notification.flashTimer = setInterval(function(){
				tarObj.className = (tarObj.className == "notification_on") ? "notification_off" : "notification_on";
			}, 1000);
		}
	},

	reset: function(){
		this.stat = "off";
		this.flash = "off";
		this.flashTimer = 100;
		this.hoverText = "";
		this.clickText = "";
		this.upgrade = 0;
		this.wifi_2g = 0;
		this.wifi_5g = 0;
		this.ftp = 0;
		this.samba = 0;
		this.loss_sync = 0;
		this.experience_FB = 0;
		this.notif_hint = 0;
		this.mobile_traffic = 0;
		this.send_debug_log = 0;
		this.sim_record = 0;
		this.external_ip = 0;
		this.action_desc = [];
		this.desc = [];
		this.array = [];
		this.clickCallBack = [];
		this.run();
	}
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

function addNewScript(scriptName){
	var script = document.createElement("script");
	script.type = "text/javascript";
	script.src = scriptName;
	document.getElementsByTagName("head")[0].appendChild(script);
}

function addNewCSS(cssName){
	var cssNode = document.createElement('link');
	cssNode.type = 'text/css';
	cssNode.rel = 'stylesheet';
	cssNode.href = cssName;
	document.getElementsByTagName("head")[0].appendChild(cssNode);
}

function unload_body(){
}

function enableCheckChangedStatus(){
}

function disableCheckChangedStatus(){
	stopFlag = 1;
}

function get_changed_status(){
}

function isMobile(){
	/*if(!tmo_support)
		return false;*/

	if(	navigator.userAgent.match(/iPhone/i) || 
		navigator.userAgent.match(/iPod/i)    ||
		navigator.userAgent.match(/iPad/i)    ||
		(navigator.userAgent.match(/Android/i) && (navigator.userAgent.match(/Mobile/i) || navigator.userAgent.match(/Tablet/i))) ||
		//(navigator.userAgent.match(/Android/i) && navigator.userAgent.match(/Mobile/i))||			//Android phone
		(navigator.userAgent.match(/Opera/i) && (navigator.userAgent.match(/Mobi/i) || navigator.userAgent.match(/Mini/i))) ||		// Opera mobile or Opera Mini
		navigator.userAgent.match(/IEMobile/i)	||		// IE Mobile
		navigator.userAgent.match(/BlackBerry/i)		//BlackBerry
	 ){
		return true;
	}
	else{
		return false;
	}
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

function isPortConflict(_val){
	if(_val == '80')
		return "<#portConflictHint#> HTTP LAN port.";
	else if(_val == '<% nvram_get("dm_http_port"); %>')
		return "<#portConflictHint#> Download Master.";
	else if(_val == '<% nvram_get("webdav_http_port"); %>')
		return "<#portConflictHint#> Cloud Disk.";
	else if(_val == '<% nvram_get("webdav_https_port"); %>')
		return "<#portConflictHint#> Cloud Disk.";
	else
		return false;
}

//Jieming added at 2013.05.24, to switch type of password to text or password
//for IE, need to use two input field and ID should be "xxx", "xxx_text" 
var isNotIE = (navigator.userAgent.search("MSIE") == -1); 
function switchType(obj, showText, chkBox){
	if(chkBox == undefined) chkBox = false;
	var newType = showText ? "text" : "password";
	if(isNotIE){	//Not IE
		obj.type = newType;
	}
	else {	//IE
		if(obj.type != newType)
		{
			var input2 = document.createElement('input');
			input2.mergeAttributes(obj);
			input2.type = newType;
			input2.name = obj.name;
			input2.id = obj.id? obj.id : obj.name;
			input2.value = obj.value;

			obj.parentNode.replaceChild(input2,obj);
			if(showText && input2.id && !chkBox)
				setTimeout(function(){document.getElementById(input2.id).focus()},10);
		}
	}
}

function corrected_timezone(){
	var today = new Date();
	var StrIndex;	
	if(today.toString().lastIndexOf("-") > 0)
		StrIndex = today.toString().lastIndexOf("-");
	else if(today.toString().lastIndexOf("+") > 0)
		StrIndex = today.toString().lastIndexOf("+");

	if(StrIndex > 0){
		if(timezone != today.toString().substring(StrIndex, StrIndex+5)){
			document.getElementById("timezone_hint_div").style.display = "";
			document.getElementById("timezone_hint").innerHTML = "* <#LANHostConfig_x_TimeZone_itemhint#>";
		}
		else
			return;
	}
	else
		return;	
}

String.prototype.howMany = function(val){
	var result = this.toString().match(new RegExp(val ,"g"));
	var count = (result)?result.length:0;

	return count;
}

/* convert some special character for shown string */
function handle_show_str(show_str)
{
	show_str = show_str.replace(/\&/g, "&amp;");
	show_str = show_str.replace(/\</g, "&lt;");
	show_str = show_str.replace(/\>/g, "&gt;");
	show_str = show_str.replace(/\ /g, "&nbsp;");
	return show_str;
}

function decodeURIComponentSafe(_ascii){
	try{
		return decodeURIComponent(_ascii);
	}
	catch(err){
		return _ascii;
	}
}

var isNewFW = function(FWVer){
	var Latest_firmver = FWVer.split("_");

	if (odmpid == "TM-AC1900" && (bl_version == "2.1.2.2" || bl_version == "2.1.2.6"))
		return true;

	if(typeof Latest_firmver[0] !== "undefined" && typeof Latest_firmver[1] !== "undefined" && typeof Latest_firmver[2] !== "undefined"){
		var Latest_firm = parseInt(Latest_firmver[0]);
		var Latest_buildno = parseInt(Latest_firmver[1]);
		var Latest_extendno = parseInt(Latest_firmver[2].split("-g")[0]);

		current_firm = parseInt('<% nvram_get("firmver"); %>'.replace(/[.]/gi,""));
		current_buildno = parseInt('<% nvram_get("buildno"); %>');
		current_extendno = parseInt('<% nvram_get("extendno"); %>'.split("-g")[0]);
		if((current_buildno < Latest_buildno) || 
			 (current_firm < Latest_firm && current_buildno == Latest_buildno) ||
			 (current_extendno < Latest_extendno && current_buildno == Latest_buildno && current_firm == Latest_firm))
		{
			return true;
		}
	}
	
	return false;
}

function getBrowser_info(){
	var browser = {};
        var temp = navigator.userAgent.toUpperCase();

	if(temp.match(/RV:([\d.]+)\) LIKE GECKO/)){	     // for IE 11
		browser.ie = temp.match(/RV:([\d.]+)\) LIKE GECKO/)[1];
	}
	else if(temp.match(/MSIE ([\d.]+)/)){	   // for IE 10 or older
		browser.ie = temp.match(/MSIE ([\d.]+)/)[1];
	}
	else if(temp.match(/CHROME\/([\d.]+)/)){
		if(temp.match(/OPR\/([\d.]+)/)){		// for Opera 15 or newer
			browser.opera = temp.match(/OPR\/([\d.]+)/)[1];
		}
		else{
			browser.chrome = temp.match(/CHROME\/([\d.]+)/)[1];	     // for Google Chrome
		}
	}
	else if(temp.match(/FIREFOX\/([\d.]+)/)){
		browser.firefox = temp.match(/FIREFOX\/([\d.]+)/)[1];
	}
	else if(temp.match(/OPERA\/([\d.]+)/)){	 // for Opera 12 or older
		browser.opera = temp.match(/OPERA\/([\d.]+)/)[1];
	}
	else if(temp.match(/VERSION\/([\d.]+).*SAFARI/)){	       // for Safari
		browser.safari = temp.match(/VERSION\/([\d.]+).*SAFARI/)[1];
	}

	return browser;
}
function regen_band(obj_name){
	var band_desc = new Array();
	var band_value = new Array();
	current_band = '<% nvram_get("wl_unit"); %>';
	for(i=1;i<wl_info.band2g_total+1;i++){
		if(wl_info.band2g_total == 1)
			band_desc.push("2.4GHz");
		else
			band_desc.push("2.4GHz-"+i);
	}
	for(i=1;i<wl_info.band5g_total+1;i++){
		if(wl_info.band5g_total == 1)
			band_desc.push("5GHz");
		else
			band_desc.push("5GHz-"+i);	
	}
	for(i=0;i<wl_info.wl_if_total;i++)
		band_value.push(i);
	add_options_x2(obj_name, band_desc, band_value, current_band);
}

var cookie = {
	set: function(key, value, days) {
		document.cookie = key + '=' + value + '; expires=' +
			(new Date(new Date().getTime() + ((days ? days : 14) * 86400000))).toUTCString() + '; path=/';
	},

	get: function(key) {
		var r = ('; ' + document.cookie + ';').match(key + '=(.*?);');
		return r ? r[1] : null;
	},

	unset: function(key) {
		document.cookie = key + '=; expires=' +
			(new Date(1)).toUTCString() + '; path=/';
	}
};

//check browser support FileReader or not
function isSupportFileReader() {
	if(typeof window.FileReader === "undefined") {
		return false;
	}
	else {
		return true;
	}
}
//check browser support canvas or not
function isSupportCanvas() {
	var elem = document.createElement("canvas");
	return !!(elem.getContext && elem.getContext('2d'));
}
