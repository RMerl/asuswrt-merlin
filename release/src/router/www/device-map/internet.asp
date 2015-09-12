<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link rel="stylesheet" type="text/css" href="/NM_style.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<link rel="stylesheet" type="text/css" href="/index_style.css">
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

<% wanlink(); %>
<% first_wanlink(); %>
<% secondary_wanlink(); %>

//active wan
var wanip = wanlink_ipaddr();
var wannetmask = wanlink_netmask();
var wandns = wanlink_dns();
var wangateway = wanlink_gateway();
var wanxip = wanlink_xipaddr();
var wanxnetmask = wanlink_xnetmask();
var wanxdns = wanlink_xdns();
var wanxgateway = wanlink_xgateway();
var wan_lease = wanlink_lease();
var wan_expires = wanlink_expires();

var first_wanip = first_wanlink_ipaddr();
var first_wannetmask = first_wanlink_netmask();
var first_wandns = first_wanlink_dns();
var first_wangateway = first_wanlink_gateway();
var first_wanxip = first_wanlink_xipaddr();
var first_wanxnetmask = first_wanlink_xnetmask();
var first_wanxdns = first_wanlink_xdns();
var first_wanxgateway = first_wanlink_xgateway();
var first_lease = first_wanlink_lease();
var first_expires = first_wanlink_expires();

var secondary_wanip = secondary_wanlink_ipaddr();
var secondary_wannetmask = secondary_wanlink_netmask();
var secondary_wandns = secondary_wanlink_dns();
var secondary_wangateway = secondary_wanlink_gateway();
var secondary_wanxip = secondary_wanlink_xipaddr();
var secondary_wanxnetmask = secondary_wanlink_xnetmask();
var secondary_wanxdns = secondary_wanlink_xdns();
var secondary_wanxgateway = secondary_wanlink_xgateway();
var secondary_lease = secondary_wanlink_lease();
var secondary_expires = secondary_wanlink_expires();

var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var old_link_internet = -1;
var lanproto = '<% nvram_get("lan_proto"); %>';

var wanproto = '<% nvram_get("wan_proto"); %>';
var dslproto = '<% nvram_get("dsl0_proto"); %>';
var dslx_transmode = '<% nvram_get("dslx_transmode"); %>';

var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
var wans_lanport = '<% nvram_get("wans_lanport"); %>';
var wan0_primary = '<% nvram_get("wan0_primary"); %>';
var wans_mode = '<%nvram_get("wans_mode");%>';
var loadBalance_Ratio = '<%nvram_get("wans_lb_ratio");%>';

<% wan_get_parameter(); %>

var wan_unit = '<% nvram_get("wan_unit"); %>' || 0;

if(yadns_support){
	var yadns_enable = '<% nvram_get("yadns_enable_x"); %>';
	var yadns_mode = '<% nvram_get("yadns_mode"); %>';
	var yadns_servers = [ <% yadns_servers(); %> ];
}

function add_lanport_number(if_name)
{
	if (if_name == "lan") {
		return "lan" + wans_lanport;
	}
	return if_name;
}

function format_time(seconds, error)
{
	if (seconds <= 0)
		return error;
	var total = seconds;
	var value = "";
	var Seconds = total % 60; total = ~~(total / 60);
	var Minutes = total % 60; total = ~~(total / 60);
	var Hours   = total % 24;
	var Days = ~~(total / 24);
	if (Days != 0)
		value += Days.toString() + " <#Day#> ";
	if (Hours != 0)
		value += Hours.toString() + " <#Hour#> ";
	if (Minutes != 0)
		value += Minutes.toString() + " <#Minute#> ";
	if (Seconds != 0)
		value += Seconds.toString() + " <#Second#>";
	return value;
}

function initial(){
	flash_button();
	// if dualwan enabled , show dualwan status
	if(parent.wans_flag){
		var unit = parent.document.form.dual_wan_flag.value; // 0: Priamry WAN, 1: Secondary WAN
		var pri_if = wans_dualwan.split(" ")[0];
		var sec_if = wans_dualwan.split(" ")[1];
		pri_if = add_lanport_number(pri_if);
		sec_if = add_lanport_number(sec_if);
		pri_if = pri_if.toUpperCase();
		sec_if = sec_if.toUpperCase();

		if(sec_if != 'NONE'){
			document.getElementById("dualwan_row_main").style.display = "";	
			// DSLTODO, need ajax to update failover status
			document.getElementById('dualwan_mode_ctrl').style.display = "";
			document.getElementById('wan_enable_button').style.display = "none";
			
			if(wans_mode == "lb"){
				//document.getElementById("wansMode").value = 1;
				document.getElementById('dualwan_row_main').style.display = "none";
				//document.getElementById('loadbalance_config_ctrl').style.display = "";
				showtext(document.getElementById("loadbalance_config"), loadBalance_Ratio);
				
				if (wan0_primary == '1') {
					showtext($("#dualwan_current")[0], pri_if);
				}
				else {
					showtext($("#dualwan_current")[0], sec_if);		
				}				
				showtext(document.getElementById("dualwan_mode"), "<#dualwan_mode_lb#>");
				loadBalance_form(unit);
			}
			else{
				//document.getElementById("wansMode").value = 2;
				showtext(document.getElementById("dualwan_mode"), "<#dualwan_mode_fo#>");		
				failover_form(unit, pri_if, sec_if);
			}		
		}
	}
	else{
		var unit = 0;
		if ((wanlink_type() == "dhcp" || wanlink_xtype() == "dhcp") && wans_dualwan.split(" ")[0]!="usb") {
		document.getElementById('primary_lease_ctrl').style.display = "";
		document.getElementById('primary_expires_ctrl').style.display = "";
		}
	}
 
	if(sw_mode == 1){
		setTimeout("update_wanip();", 1);
		document.getElementById('goSetting').style.display = "";
		
		if(dualWAN_support){
			if(dsl_support && wans_dualwan.split(" ")[0] == "usb" && parent.document.form.dual_wan_flag.value == 0){
				document.getElementById("goDualWANSetting").style.display = "none";
				document.getElementById("dualwan_enable_button").style.display = "none";
			}			
			else if(parent.document.form.dual_wan_flag.value == 0){
				document.getElementById("goDualWANSetting").style.display = "none";
				document.getElementById("dualwan_enable_button").style.display = "";
			}
			else{
				document.getElementById("goDualWANSetting").style.display = "";
				document.getElementById("dualwan_enable_button").style.display = "none";	
			}
		}
		else{
			document.getElementById("goDualWANSetting").style.display = "none";
			document.getElementById("dualwan_enable_button").style.display = "none";	
		}
		
	}
	else{
		document.getElementById("rt_table").style.display = "none";
		document.getElementById("ap_table").style.display = "";
		if(sw_mode == 3)
			document.getElementById('RemoteAPtd').style.display = "none";
		
		if((sw_mode == 2 || sw_mode == 3 || sw_mode == 4) && decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>").length >= 28){
			showtext(document.getElementById("RemoteAP"), decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>").substring(0, 26)+"...");
			document.getElementById("RemoteAPtd").title = decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>");
		}else				
			showtext(document.getElementById("RemoteAP"), decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>"));
				
		if(lanproto == "static")
			showtext(document.getElementById("LanProto"), "<#BOP_ctype_title5#>");
		else
			showtext(document.getElementById("LanProto"), "<#BOP_ctype_title1#>");

		if(sw_mode == 2 || sw_mode == 4)
			document.getElementById('sitesurvey_tr').style.display = "";
	}

	update_connection_type(unit);

	if(yadns_support){
		if(yadns_enable != 0 && yadns_mode != -1){
			showtext(document.getElementById("yadns_mode"), get_yadns_modedesc(yadns_mode));
			showtext2($("#yadns_DNS1")[0], yadns_servers[0], yadns_servers[0]);
			showtext2($("#yadns_DNS2")[0], yadns_servers[1], yadns_servers[1]);
			document.getElementById('yadns_ctrl').style.display = "";
		}
	}

	if(parent.wans_flag){
		if(unit == 0){
			if(dsl_support && wans_dualwan.split(" ")[0] == "dsl" 
				&& (productid == "DSL-AC68U" || productid == "DSL-AC68R")){     //MODELDEP: DSL-AC68U,DSL-AC68R
				document.getElementById("divSwitchMenu").style.display = "";	
			}
			update_all_ip(first_wanip, first_wannetmask, first_wandns, first_wangateway , 0);
			update_all_xip(first_wanxip, first_wanxnetmask, first_wanxdns, first_wanxgateway, 0);
		}
		else if(unit == 1){
			update_all_ip(secondary_wanip, secondary_wannetmask, secondary_wandns, secondary_wangateway , 1);
			update_all_xip(secondary_wanxip, secondary_wanxnetmask, secondary_wanxdns, secondary_wanxgateway, 1);
		}
	}
	else{
		if(dsl_support && wans_dualwan.split(" ")[0] == "dsl"
			&& (productid == "DSL-AC68U" || productid == "DSL-AC68R")){     //MODELDEP: DSL-AC68U,DSL-AC68R
			document.getElementById("divSwitchMenu").style.display = "";
		}
		update_all_ip(wanip, wannetmask, wandns, wangateway , unit);
		update_all_xip(wanxip, wanxnetmask, wanxdns, wanxgateway, unit);
	}

}

function update_connection_type(dualwan_unit){
	if(parent.wans_flag){
		if(dualwan_unit == 0)
			var wanlink_type_conv = first_wanlink_type();
		else
			var wanlink_type_conv = secondary_wanlink_type();
	}
	else
		var wanlink_type_conv = wanlink_type();

	if (dsl_support) {
		
		if( wans_dualwan.split(" ")[dualwan_unit] == "dsl" ) {
			if(dslx_transmode == "atm") {
				if (dslproto == "pppoa" || dslproto == "ipoa")
					wanlink_type_conv = dslproto;
			}
		}
	}

	if(wanlink_type_conv == "dhcp")
		wanlink_type_conv = "<#BOP_ctype_title1#>";
	else if(wanlink_type_conv == "static")
		wanlink_type_conv = "<#BOP_ctype_title5#>";
	else if(wanlink_type_conv == "pppoe" || wanlink_type_conv == "PPPOE")
		wanlink_type_conv = "PPPoE";
	else if(wanlink_type_conv == "pptp")
		wanlink_type_conv = "PPTP";
	else if(wanlink_type_conv == "l2tp")
		wanlink_type_conv = "L2TP";
	else if(wanlink_type_conv == "pppoa")
		wanlink_type_conv = "PPPoA";
	else if(wanlink_type_conv == "ipoa")
		wanlink_type_conv = "IPoA";

	showtext($("#connectionType")[0], wanlink_type_conv);
}

function loadBalance_form(lb_unit){
	if(!parent.wans_flag)
		return 0;

	var pri_if = wans_dualwan.split(" ")[0];
	var sec_if = wans_dualwan.split(" ")[1];	
	pri_if = add_lanport_number(pri_if);
	sec_if = add_lanport_number(sec_if);
	pri_if = pri_if.toUpperCase();
	sec_if = sec_if.toUpperCase();

	if(lb_unit == 0){
		have_lease = (first_wanlink_type() == "dhcp" || first_wanlink_type() == "dhcp");
		document.getElementById("dualwan_row_primary").style.display = "";			
		showtext($("#dualwan_primary_if")[0], pri_if);
		document.getElementById("dualwan_row_secondary").style.display = "none";	
		update_connection_type(0);
		document.getElementById('primary_WANIP_ctrl').style.display = "";
		document.getElementById('primary_netmask_ctrl').style.display = "";
		document.getElementById('secondary_WANIP_ctrl').style.display = "none";
		document.getElementById('secondary_netmask_ctrl').style.display = "none";
		document.getElementById('primary_DNS_ctrl').style.display = "";
		document.getElementById('secondary_DNS_ctrl').style.display = "none";
		document.getElementById('primary_gateway_ctrl').style.display = "";
		document.getElementById('secondary_gateway_ctrl').style.display = "none";		
		document.getElementById('primary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('primary_expires_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('secondary_lease_ctrl').style.display = "none";
		document.getElementById('secondary_expires_ctrl').style.display = "none";
	}else{
		have_lease = (secondary_wanlink_type() == "dhcp" || secondary_wanlink_xtype() == "dhcp");
		document.getElementById("dualwan_row_primary").style.display = "none";
		document.getElementById("dualwan_row_secondary").style.display = "";	
		showtext($("#dualwan_secondary_if")[0], sec_if);
		update_connection_type(1);
		document.getElementById('primary_WANIP_ctrl').style.display = "none";
		document.getElementById('primary_netmask_ctrl').style.display = "none";
		document.getElementById('secondary_WANIP_ctrl').style.display = "";
		document.getElementById('secondary_netmask_ctrl').style.display = "";
		document.getElementById('primary_DNS_ctrl').style.display = "none";
		document.getElementById('secondary_DNS_ctrl').style.display = "";
		document.getElementById('primary_gateway_ctrl').style.display = "none";
		document.getElementById('secondary_gateway_ctrl').style.display = "";
		document.getElementById('primary_lease_ctrl').style.display = "none";
		document.getElementById('primary_expires_ctrl').style.display = "none";
		document.getElementById('secondary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('secondary_expires_ctrl').style.display = (have_lease) ? "" : "none";
	}
}

function failover_form(fo_unit, primary_if, secondary_if){
	if(fo_unit == 0){
		have_lease = (first_wanlink_type() == "dhcp" || first_wanlink_xtype() == "dhcp");
		showtext($("#dualwan_current")[0], primary_if);
		update_connection_type(0);
		document.getElementById('primary_WANIP_ctrl').style.display = "";
		document.getElementById('primary_netmask_ctrl').style.display = "";
		document.getElementById('secondary_WANIP_ctrl').style.display = "none";
		document.getElementById('secondary_netmask_ctrl').style.display = "none";
		document.getElementById('primary_DNS_ctrl').style.display = "";
		document.getElementById('secondary_DNS_ctrl').style.display = "none";
		document.getElementById('primary_gateway_ctrl').style.display = "";
		document.getElementById('secondary_gateway_ctrl').style.display = "none";
		document.getElementById('primary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('primary_expires_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('secondary_lease_ctrl').style.display = "none";
		document.getElementById('secondary_expires_ctrl').style.display = "none";		
	}
	else{
		have_lease = (secondary_wanlink_type() == "dhcp" || secondary_wanlink_xtype() == "dhcp");
		showtext($("#dualwan_current")[0], secondary_if);
		update_connection_type(1);
		document.getElementById('primary_WANIP_ctrl').style.display = "none";
		document.getElementById('primary_netmask_ctrl').style.display = "none";
		document.getElementById('secondary_WANIP_ctrl').style.display = "";
		document.getElementById('secondary_netmask_ctrl').style.display = "";
		document.getElementById('primary_DNS_ctrl').style.display = "none";
		document.getElementById('secondary_DNS_ctrl').style.display = "";
		document.getElementById('primary_gateway_ctrl').style.display = "none";
		document.getElementById('secondary_gateway_ctrl').style.display = "";
		document.getElementById('primary_lease_ctrl').style.display = "none";
		document.getElementById('primary_expires_ctrl').style.display = "none";
		document.getElementById('secondary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		document.getElementById('secondary_expires_ctrl').style.display = (have_lease) ? "" : "none";
	}
}

function update_all_ip(wanip, wannetmask, wandns, wangateway, unit){
	var dnsArray = wandns.split(" ");
	if(unit == 0){
		showtext($("#WANIP")[0], wanip);
		showtext($("#netmask")[0], wannetmask);
		showtext2($("#DNS1")[0], dnsArray[0], dnsArray[0]);
		showtext2($("#DNS2")[0], dnsArray[1], dnsArray[1]);
		showtext($("#gateway")[0], wangateway);

		if(parent.wans_flag){
			if (first_wanlink_type() == "dhcp") {
				showtext($("#lease")[0], format_time(first_lease, "Renewing..."));
				showtext($("#expires")[0], format_time(first_expires, "Expired"));
			}
		}
		else{
			if (wanlink_type() == "dhcp") {
				showtext($("#lease")[0], format_time(wan_lease, "Renewing..."));
				showtext($("#expires")[0], format_time(wan_expires, "Expired"));
			}
		}

	}
	else{
		showtext($("#secondary_WANIP")[0], wanip);
		showtext($("#secondary_netmask")[0], wannetmask);
		showtext2($("#secondary_DNS1")[0], dnsArray[0], dnsArray[0]);
		showtext2($("#secondary_DNS2")[0], dnsArray[1], dnsArray[1]);
		showtext($("#secondary_gateway")[0], wangateway);
		if (secondary_wanlink_type() == "dhcp") {
			showtext($("#secondary_lease")[0], format_time(secondary_lease, "Renewing..."));
			showtext($("#secondary_expires")[0], format_time(secondary_expires, "Expired"));
		}
	}
}
function update_all_xip(wanxip, wanxnetmask, wanxdns, wanxgateway, unit) {
	var dnsArray = wandns.split(" ");
	var have_dns = !(dnsArray[0] || dnsArray[1]);
	var dnsArray = wanxdns.split(" ");
	var have_ip = false;
	var have_gateway = false;
	var have_lease = false;
	var lease = 0;
	var expires = 0;

	var type = "";

	if(parent.wans_flag){
		type = (unit == 0) ? first_wanlink_xtype() : secondary_wanlink_xtype();
	}
	else
		type = wanlink_xtype();

	if (type == "dhcp" || type == "static") {
		have_ip = true;
		have_gateway = !(wanxgateway == "" || wanxgateway == "0.0.0.0");
		if (type == "dhcp") {
			have_lease = true;

			if(parent.wans_flag)
				lease = (unit == 0) ? first_wanlink_xlease() : secondary_wanlink_xlease();
			else
				lease = wanlink_xlease();

			if(parent.wans_flag)
				expires = (unit == 0) ? first_wanlink_xexpires() : secondary_wanlink_xexpires();
			else
				expires = wanlink_xexpires();
		}
	}

	if (unit == 0) {
		showtext2($("#xWANIP")[0], wanxip, have_ip);
		showtext2($("#xnetmask")[0], wanxnetmask, have_ip);
		showtext2($("#xDNS1")[0], dnsArray[0], have_dns && dnsArray[0]);
		showtext2($("#xDNS2")[0], dnsArray[1], have_dns && dnsArray[1]);
		showtext2($("#xgateway")[0], wanxgateway, have_gateway);
		showtext2($("#xlease")[0], format_time(lease, "Renewing..."), have_lease);
		showtext2($("#xexpires")[0], format_time(expires, "Expired"), have_lease);
	}
	else {
		showtext2($("#secondary_xWANIP")[0], wanxip, have_ip);
		showtext2($("#secondary_xnetmask")[0], wanxnetmask, have_ip);
		showtext2($("#secondary_xDNS1")[0], dnsArray[0], have_dns && dnsArray[0]);
		showtext2($("#secondary_xDNS2")[0], dnsArray[1], have_dns && dnsArray[1]);
		showtext2($("#secondary_xgateway")[0], wanxgateway, have_gateway);
		showtext2($("#secondary_xlease")[0], format_time(lease, "Renewing..."), have_lease);
		showtext2($("#secondary_xexpires")[0], format_time(expires, "Expired"), have_lease);
	}
}

function update_wan_state(state, auxstate){
	if(state == "2" && auxstate == "0")
		link_internet = 1;
	else
		link_internet = 0;
		
	return link_internet;
}

function update_wanip(e) {
  $.ajax({
    url: '/status.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_wanip();", 3000);
    },
    success: function(response) {
		if(parent.wans_flag){
			first_wanip = first_wanlink_ipaddr();
			first_wannetmask = first_wanlink_netmask();
			first_wandns = first_wanlink_dns();
			first_wangateway = first_wanlink_gateway();
			first_wanxip = first_wanlink_xipaddr();
			first_wanxnetmask = first_wanlink_xnetmask();
			first_wanxdns = first_wanlink_xdns();
			first_wanxgateway = first_wanlink_xgateway();
			first_lease = first_wanlink_lease();
			firset_expires = first_wanlink_expires();

			secondary_wanip = secondary_wanlink_ipaddr();
			secondary_wannetmask = secondary_wanlink_netmask();
			secondary_wandns = secondary_wanlink_dns();
			secondary_wangateway = secondary_wanlink_gateway();
			secondary_wanxip = secondary_wanlink_xipaddr();
			secondary_wanxnetmask = secondary_wanlink_xnetmask();
			secondary_wanxdns = secondary_wanlink_xdns();
			secondary_wanxgateway = secondary_wanlink_xgateway();
			secondary_lease = secondary_wanlink_lease();
			secondary_expires = secondary_wanlink_expires();
		}
		else{
			wanip = wanlink_ipaddr();
			wannetmask = wanlink_netmask();
			wandns = wanlink_dns();
			wangateway = wanlink_gateway();
			wanxip = wanlink_xipaddr();
			wanxnetmask = wanlink_xnetmask();
			wanxdns = wanlink_xdns();
			wanxgateway = wanlink_xgateway();
			wan_lease = wanlink_lease();
			wan_expires = wanlink_expires();
		}

		if(old_link_internet == -1)
			old_link_internet = update_wan_state(wanstate, wanauxstate);

		if(update_wan_state(wanstate, wanauxstate) != old_link_internet){
			refreshpage();
		}
		else{
			if(parent.wans_flag){
				update_all_ip(first_wanip, first_wannetmask, first_wandns, first_wangateway, 0);
				update_all_xip(first_wanxip, first_wanxnetmask, first_wanxdns, first_wanxgateway, 0);
				update_all_ip(secondary_wanip, secondary_wannetmask, secondary_wandns, secondary_wangateway, 1);
				update_all_xip(secondary_wanxip, secondary_wanxnetmask, secondary_wanxdns, secondary_wanxgateway, 1);
			}
			else{
				update_all_ip(wanip, wannetmask, wandns, wangateway, 0);
				update_all_xip(wanxip, wanxnetmask, wanxdns, wanxgateway, 0);
			}

			setTimeout("update_wanip();", 3000);
		}
    }
  });
}

function submitWANAction(status){
	switch(status){
		case 0:
			parent.showLoading();
			setTimeout('location.href = "/device-map/wan_action.asp?wanaction=Connect";', 1);
			break;
		case 1:
			parent.showLoading();
			setTimeout('location.href = "/device-map/wan_action.asp?wanaction=Disconnect";', 1);
			break;
		default:
			alert("No change!");
	}
}

function goQIS(){
	parent.location.href = '/QIS_wizard.htm';
}

function goToWAN(){
	if(parent.wans_flag){
		var wan_selected = parent.document.form.dual_wan_flag.value;

		if(wan_selected == 0){
			document.act_form.wan_unit.value = 0;
		}
		else if(wan_selected == 1){
			document.act_form.wan_unit.value = 1;
		}
		document.act_form.action_mode.value = "change_wan_unit";
		document.act_form.target = "";
		if(wans_dualwan.split(" ")[wan_selected].toUpperCase() == "USB"){
			if(gobi_support)
				document.act_form.current_page.value = "Advanced_MobileBroadband_Content.asp";
			else
				document.act_form.current_page.value = "Advanced_Modem_Content.asp";
		}
		else if(wans_dualwan.split(" ")[wan_selected].toUpperCase() == "WAN" || wans_dualwan.split(" ")[wan_selected].toUpperCase() == "LAN"){
			document.act_form.current_page.value = "Advanced_WAN_Content.asp";
		}
		else if(wans_dualwan.split(" ")[wan_selected].toUpperCase() == "DSL")
			document.act_form.current_page.value = "Advanced_DSL_Content.asp";

		document.act_form.submit();
	}
	else{
		if(dsl_support)
			parent.location.href = '/Advanced_DSL_Content.asp';
		else
			parent.location.href = '/Advanced_WAN_Content.asp';
	}
}

function goToDualWAN(){
	parent.location.href = '/Advanced_WANPort_Content.asp';
}

function gotoSiteSurvey(){
	if(sw_mode == 2)
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_rep&band='+'<% nvram_get("wl_unit"); %>';
	else
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_mb';
}

function manualSetup(){
	return 0;
}
</script>
</head>

<body class="statusbody" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="internetForm" id="form" action="/start_apply2.htm">
<input type="hidden" name="current_page" value="/index.asp">
<input type="hidden" name="next_page" value="/index.asp">
<input type="hidden" name="flag" value="Internet">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wan_if">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="wan_enable" value="<% nvram_get("wan_enable"); %>">
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>">
<input type="hidden" name="wan_unit" value="<% get_wan_unit(); %>">
<input type="hidden" name="dslx_link_enable" value="" disabled>
<input type="hidden" name="wans_mode" value='<% nvram_get("wans_mode"); %>'>
<table border="0" cellpadding="0" cellspacing="0">
	<tr>
		<td>
			<table width="100px" border="0" align="left" style="margin-left:8px;" cellpadding="0" cellspacing="0">
				<td>
					<div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="loadBalance_form(0);">
						<span id="span1" style="cursor:pointer;font-weight: bolder;">Primary</span>
					</div>
				</td>
				<td>
					<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="loadBalance_form(1);">
						<span id="span1" style="cursor:pointer;font-weight: bolder;">Secondary</span>
					</div>
				</td>
			</table>
		</td>
	</tr>
</table>
<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" id="rt_table">
<tr>
	<td>
		<div id="divSwitchMenu" style="margin-top:4px;margin-right:4px;float:right;display:none;">
			<div style="width:80px;height:30px;border-top-left-radius:8px;border-bottom-left-radius:8px;" class="block_filter_pressed">
				<div style="text-align:center;padding-top:5px;color:#93A9B1;font-size:14px">WAN</div>
			</div>
			<div style="width:80px;height:30px;margin:-32px 0px 0px 80px;border-top-right-radius:8px;border-bottom-right-radius:8px;" class="block_filter">
				<a href="/device-map/DSL_dashboard.asp"><div class="block_filter_name">DSL</div></a>
			</div>
		</div>
	</td>	
</tr>	
<tr id="wan_enable_button">
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px;"><#menu5_3_1#></p>
    		<div class="left" style="width:94px; float:right;" id="radio_wan_enable"></div>
				<div class="clear"></div>
				<script type="text/javascript">
						$('#radio_wan_enable').iphoneSwitch('<% nvram_get("wan_enable"); %>', 
							 function() {
								document.internetForm.wan_enable.value = "1";
								if (dsl_support) {
									document.internetForm.dslx_link_enable.value = "1";
									document.internetForm.dslx_link_enable.disabled = false;
								}
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 },
							 function() {
								document.internetForm.wan_enable.value = "0";
								if (dsl_support) {
									document.internetForm.dslx_link_enable.value = "0";
									document.internetForm.dslx_link_enable.disabled = false;
								}
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 }
						);
				</script>
    		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="dualwan_enable_button">
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px;"><#dualwan_enable#></p>
    		<div class="left" style="width:94px; float:right;" id="radio_dualwan_enable"></div>
				<div class="clear"></div>
				<script type="text/javascript">
						$('#radio_dualwan_enable').iphoneSwitch(parent.wans_flag, 
							 function() {
								if(wans_dualwan.split(" ")[0] == "usb")
									document.internetForm.wans_dualwan.value = wans_dualwan.split(" ")[0]+" wan";
								else
									document.internetForm.wans_dualwan.value = wans_dualwan.split(" ")[0]+" usb";
								document.internetForm.action_wait.value = '<% get_default_reboot_time(); %>';
								document.internetForm.action_script.value = "reboot";
								parent.showLoading();
								document.internetForm.submit();
								return true;
							 },
							 function() {
								document.internetForm.wans_dualwan.value = wans_dualwan.split(" ")[0]+" none";
								document.internetForm.wan_unit.value = 0;
								document.internetForm.action_wait.value = '<% get_default_reboot_time(); %>';
								document.internetForm.action_script.value = "reboot";
								document.internetForm.wans_mode.value = "fo";
								parent.showLoading();
								document.internetForm.submit();
								return true;
							 }
						);
				</script>
    		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id=dualwan_row_main style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">WAN Port</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_current"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id=dualwan_row_primary style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#wan_type#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_primary_if"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id=dualwan_row_secondary style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#wan_type#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_secondary_if"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="dualwan_mode_ctrl" style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#dualwan_mode#></p>

    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_mode"></p>
				<!--select style="*margin-top:-7px;" id="wansMode" class="input_option" onchange="">
					<option value="1">Load Balance</option>
					<option value="2">Fail Over</option>
				</select-->

      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr id="loadbalance_config_ctrl" style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#dualwan_mode_lb_setting#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="loadbalance_config"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" ><#Connectiontype#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="connectionType"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="primary_WANIP_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#WAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="WANIP"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xWANIP"></p>
    		<span id="wan_status" style="display:none"></span>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr id="primary_netmask_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#IPConnection_x_ExternalSubnetMask_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="netmask"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xnetmask"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_WANIP_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#WAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_WANIP"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xWANIP"></p>
    		<span id="wan_status" style="display:none"></span>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_netmask_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#IPConnection_x_ExternalSubnetMask_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_netmask"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xnetmask"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr style="display:none;" id="yadns_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#YandexDNS#></p>
    		<a href="/YandexDNS.asp" target="_parent">
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="yadns_mode"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="yadns_DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="yadns_DNS2"></p>
    		</a>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="primary_DNS_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">DNS</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS2"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xDNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xDNS2"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_DNS_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">DNS</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_DNS2"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xDNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xDNS2"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="primary_gateway_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#RouterConfig_GWStaticGW_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="gateway"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xgateway"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_gateway_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#RouterConfig_GWStaticGW_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_gateway"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xgateway"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="primary_lease_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#LANHostConfig_LeaseTime_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="lease"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xlease"></p>
    	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="primary_expires_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#LeaseExpires#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="expires"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xexpires"></p>
    	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_lease_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#LANHostConfig_LeaseTime_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_lease"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xlease"></p>
    	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_expires_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#LeaseExpires#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_expires"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xexpires"></p>
    	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr id="goDualWANSetting">
	<td height="50" style="padding:10px 15px 0px 15px;">
		<p class="formfonttitle_nwm" style="float:left;width:116px;"><#Dualwan_setting#></p>
		<input type="button" class="button_gen_long" onclick="goToDualWAN();" value="<#btn_go#>" style="position:absolute;right:25px;margin-top:-10px;margin-left:115px;">
		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
	</td>
</tr>
<tr id="goSetting" style="display:none">
	<td height="30" style="padding:10px 15px 0px 15px;">
		<p class="formfonttitle_nwm" style="float:left;width:116px;"><#btn_to_WAN#></p>
		<input type="button" class="button_gen_long" onclick="goToWAN();" value="<#btn_go#>" style="position:absolute;right:25px;margin-top:-10px;margin-left:115px;">
	</td>
</tr>

<!--tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px; "><#QIS#></p>
    		<input type="button" class="button_gen" value="<#btn_go#>" onclick="javascript:goQIS();">
    </td>
</tr-->  
</table>

<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" id="ap_table" style="display:none">
<tr>
    <td style="padding:5px 10px 5px 15px;" id="RemoteAPtd">
    		<p class="formfonttitle_nwm"><#statusTitle_AP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="RemoteAP"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#Connectiontype#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="LanProto"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#LAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_ipaddr"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#IPConnection_x_ExternalSubnetMask_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_netmask"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#IPConnection_x_ExternalGateway_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_gateway"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#HSDPAConfig_DNSServers_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_dns"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="sitesurvey_tr" style="display:none">
  <td height="50" style="padding:10px 15px 0px 15px;">
  	<p class="formfonttitle_nwm" style="float:left;"><#APSurvey_action_search_again_hint2#></p>
		<br />
  	<input type="button" class="button_gen" onclick="gotoSiteSurvey();" value="<#QIS_rescan#>" style="float:right;">
  	<!--input type="button" class="button_gen" onclick="manualSetup();" value="<#Manual_Setting_btn#>" style="float:right;"-->
		<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
  </td>
</tr>

</table>

</form>
<form method="post" name="act_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="wan_unit" value="">
<input type="hidden" name="current_page" value="">
</form>
</body>
</html>
