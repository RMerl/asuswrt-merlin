<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Untitled Document</title>
<link rel="stylesheet" type="text/css" href="../NM_style.css">
<link rel="stylesheet" type="text/css" href="../form_style.css">
<script type="text/javascript" src="formcontrol.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<script>
<% wanlink(); %>
<% secondary_wanlink(); %>

var wanproto = '<% nvram_get("wan_proto"); %>';
var dslproto = '<% nvram_get("dsl0_proto"); %>';
var wanip = wanlink_ipaddr();
var wandns = wanlink_dns();
var wangateway = wanlink_gateway();
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var old_link_internet = -1;
var lan_proto = '<% nvram_get("lan_proto"); %>';

var wanxip = wanlink_xipaddr();
var wanxdns = wanlink_xdns();
var wanxgateway = wanlink_xgateway();

var secondary_wanip = secondary_wanlink_ipaddr();
var secondary_wandns = secondary_wanlink_dns();
var secondary_wangateway = secondary_wanlink_gateway();

var secondary_wanxip = secondary_wanlink_xipaddr();
var secondary_wanxdns = secondary_wanlink_xdns();
var secondary_wanxgateway = secondary_wanlink_xgateway();

var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
var wans_lanport = '<% nvram_get("wans_lanport"); %>';
var wan0_primary = '<% nvram_get("wan0_primary"); %>';
var wans_mode = '<%nvram_get("wans_mode");%>';
var loadBalance_Ratio = '<%nvram_get("wans_lb_ratio");%>';

<% wan_get_parameter(); %>

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

function showtext2(obj, str, visible)
{
	if (obj) {
		obj.innerHTML = str;
		obj.style.display = (visible) ? "" : "none";
	}
}

function initial(){
	flash_button();
	// if dualwan enabled , show dualwan status
	if(dualWAN_support){
		var pri_if = wans_dualwan.split(" ")[0];
		var sec_if = wans_dualwan.split(" ")[1];	
		pri_if = add_lanport_number(pri_if);
		sec_if = add_lanport_number(sec_if);
		pri_if = pri_if.toUpperCase();
		sec_if = sec_if.toUpperCase();

			if(sec_if != 'NONE'){
				$("dualwan_row_main").style.display = "";	
				// DSLTODO, need ajax to update failover status
				$('dualwan_mode_ctrl').style.display = "";
				$('wan_enable_button').style.display = "none";
				
				if(wans_mode == "lb"){
					$('dualwan_row_main').style.display = "none";
					$('loadbalance_config_ctrl').style.display = "";
					showtext($("loadbalance_config"), loadBalance_Ratio);
					
					if (wan0_primary == '1') {
						showtext($j("#dualwan_current")[0], pri_if);
					}
					else {
						showtext($j("#dualwan_current")[0], sec_if);		
					}				
					showtext($("dualwan_mode"), "Load Balance");
					loadBalance_form(parent.document.form.dual_wan_flag.value);  // 0: Priamry WAN, 1: Secondary WAN		
				}
				else if(wans_mode == "fo"){
					showtext($("dualwan_mode"), "Fail Over");		
					failover_form(parent.document.form.dual_wan_flag.value, pri_if, sec_if);
				}		
			}
	}
	else{
		if (wanlink_type() == "dhcp" || wanlink_xtype() == "dhcp") {
		$('primary_lease_ctrl').style.display = "";
		$('primary_expires_ctrl').style.display = "";
		}
		
	}

	if(sw_mode == 1){
		setTimeout("update_wanip();", 1);
		$('goSetting').style.display = "";
	}
	else{
		$("rt_table").style.display = "none";
		$("ap_table").style.display = "";
		if(sw_mode == 3)
			$('RemoteAPtd').style.display = "none";
		
		if((sw_mode == 2 || sw_mode == 3 || sw_mode == 4) && decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>").length >= 28){
			showtext($("RemoteAP"), decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>").substring(0, 26)+"...");
			$("RemoteAPtd").title = decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>");
		}else				
			showtext($("RemoteAP"), decodeURIComponent("<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>"));
				
		if(lan_proto == "static")
			showtext($("LanProto"), "<#BOP_ctype_title5#>");
		else
			showtext($("LanProto"), "<#BOP_ctype_title1#>");

		if(sw_mode == 2 || sw_mode == 4)
			$('sitesurvey_tr').style.display = "";
	}

	if(wanlink_type() == "dhcp")
		var wanlink_type_conv = "<#BOP_ctype_title1#>";
	else 	if(wanlink_type() == "pppoe" ||wanlink_type() == "PPPOE")
		var wanlink_type_conv = "PPPoE";
	else 	if(wanlink_type() == "static")
		var wanlink_type_conv = "<#BOP_ctype_title5#>";
	else 	if(wanlink_type() == "pptp")
		var wanlink_type_conv = "PPTP";
	else 	if(wanlink_type() == "l2tp")
		var wanlink_type_conv = "L2TP";
	else
		var wanlink_type_conv = wanlink_type();
		
	if (parent.dsl_support) {
		if (wanproto == "pppoe") {
			if (dslproto == "pppoa") wanlink_type_conv = "PPPoA";
		}
		else if (wanproto == "static") {
			if (dslproto == "ipoa") wanlink_type_conv = "IPoA";
		}		
	}

	showtext($j("#connectionType")[0], wanlink_type_conv);
	update_all_ip(wanip, wandns, wangateway , 0);
	update_all_xip(wanxip, wanxdns, wanxgateway, 0);
}
function update_connection_type(dualwan_unit){
	if(dualwan_unit == 0){
		if(wanlink_type() == "dhcp")
			var wanlink_type_conv = "<#BOP_ctype_title1#>";
		else 	if(wanlink_type() == "pppoe" ||wanlink_type() == "PPPOE")
			var wanlink_type_conv = "PPPoE";
		else 	if(wanlink_type() == "static")
			var wanlink_type_conv = "<#BOP_ctype_title5#>";
		else 	if(wanlink_type() == "pptp")
			var wanlink_type_conv = "PPTP";
		else 	if(wanlink_type() == "l2tp")
			var wanlink_type_conv = "L2TP";
		else
			var wanlink_type_conv = wanlink_type();
	}else{
		if(secondary_wanlink_type() == "dhcp")
			var wanlink_type_conv = "<#BOP_ctype_title1#>";
		else 	if(secondary_wanlink_type() == "pppoe" ||secondary_wanlink_type() == "PPPOE")
			var wanlink_type_conv = "PPPoE";
		else 	if(secondary_wanlink_type() == "static")
			var wanlink_type_conv = "<#BOP_ctype_title5#>";
		else 	if(secondary_wanlink_type() == "pptp")
			var wanlink_type_conv = "PPTP";
		else 	if(secondary_wanlink_type() == "l2tp")
			var wanlink_type_conv = "L2TP";
		else
			var wanlink_type_conv = secondary_wanlink_type();
	
	}
	showtext($j("#connectionType")[0], wanlink_type_conv);
}

function loadBalance_form(lb_unit){
	if(!dualWAN_support)
		return 0;

	var pri_if = wans_dualwan.split(" ")[0];
	var sec_if = wans_dualwan.split(" ")[1];	
	pri_if = add_lanport_number(pri_if);
	sec_if = add_lanport_number(sec_if);
	pri_if = pri_if.toUpperCase();
	sec_if = sec_if.toUpperCase();

	if(lb_unit == 0){
		have_lease = (wanlink_type() == "dhcp" || wanlink_xtype() == "dhcp");
		$("dualwan_row_primary").style.display = "";			
		showtext($j("#dualwan_primary_if")[0], pri_if);
		$("dualwan_row_secondary").style.display = "none";	
		update_connection_type(0);
		$('primary_WANIP_ctrl').style.display = "";
		$('secondary_WANIP_ctrl').style.display = "none";
		$('primary_DNS_ctrl').style.display = "";
		$('secondary_DNS_ctrl').style.display = "none";
		$('primary_gateway_ctrl').style.display = "";
		$('secondary_gateway_ctrl').style.display = "none";		
		$('primary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		$('primary_expires_ctrl').style.display = (have_lease) ? "" : "none";
		$('secondary_lease_ctrl').style.display = "none";
		$('secondary_expires_ctrl').style.display = "none";
	}else{
		have_lease = (secondary_wanlink_type() == "dhcp" || secondary_wanlink_xtype() == "dhcp");
		$("dualwan_row_primary").style.display = "none";
		$("dualwan_row_secondary").style.display = "";	
		showtext($j("#dualwan_secondary_if")[0], sec_if);
		update_connection_type(1);
		$('primary_WANIP_ctrl').style.display = "none";
		$('secondary_WANIP_ctrl').style.display = "";
		$('primary_DNS_ctrl').style.display = "none";
		$('secondary_DNS_ctrl').style.display = "";
		$('primary_gateway_ctrl').style.display = "none";
		$('secondary_gateway_ctrl').style.display = "";
		$('primary_lease_ctrl').style.display = "none";
		$('primary_expires_ctrl').style.display = "none";
		$('secondary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		$('secondary_expires_ctrl').style.display = (have_lease) ? "" : "none";
	}
}

function failover_form(fo_unit, primary_if, secondary_if){
	if(fo_unit == 0){
		have_lease = (wanlink_type() == "dhcp" || wanlink_xtype() == "dhcp");
		showtext($j("#dualwan_current")[0], primary_if);
		$('primary_WANIP_ctrl').style.display = "";
		$('secondary_WANIP_ctrl').style.display = "none";
		$('primary_DNS_ctrl').style.display = "";
		$('secondary_DNS_ctrl').style.display = "none";
		$('primary_gateway_ctrl').style.display = "";
		$('secondary_gateway_ctrl').style.display = "none";
		$('primary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		$('primary_expires_ctrl').style.display = (have_lease) ? "" : "none";
		$('secondary_lease_ctrl').style.display = "none";
		$('secondary_expires_ctrl').style.display = "none";		
	}
	else{
		have_lease = (secondary_wanlink_type() == "dhcp" || secondary_wanlink_xtype() == "dhcp");
		showtext($j("#dualwan_current")[0], secondary_if);
		$('primary_WANIP_ctrl').style.display = "none";
		$('secondary_WANIP_ctrl').style.display = "";
		$('primary_DNS_ctrl').style.display = "none";
		$('secondary_DNS_ctrl').style.display = "";
		$('primary_gateway_ctrl').style.display = "none";
		$('secondary_gateway_ctrl').style.display = "";
		$('primary_lease_ctrl').style.display = "none";
		$('primary_expires_ctrl').style.display = "none";
		$('secondary_lease_ctrl').style.display = (have_lease) ? "" : "none";
		$('secondary_expires_ctrl').style.display = (have_lease) ? "" : "none";
	}
}

function update_all_ip(wanip, wandns, wangateway, unit){
	var dnsArray = wandns.split(" ");
	if(unit == 0){		
		showtext($j("#WANIP")[0], wanip);
		showtext2($j("#DNS1")[0], dnsArray[0], dnsArray[0]);
		showtext2($j("#DNS2")[0], dnsArray[1], dnsArray[1]);
		showtext($j("#gateway")[0], wangateway);
		if (wanlink_type() == "dhcp") {
			var lease = wanlink_lease();
			var expires = wanlink_expires();
			showtext($j("#lease")[0], format_time(lease, "Renewing..."));
			showtext($j("#expires")[0], format_time(expires, "Expired"));
		}
	}
	else{
		showtext($j("#secondary_WANIP")[0], wanip);
		showtext2($j("#secondary_DNS1")[0], dnsArray[0], dnsArray[0]);
		showtext2($j("#secondary_DNS2")[0], dnsArray[1], dnsArray[1]);
		showtext($j("#secondary_gateway")[0], wangateway);
		if (secondary_wanlink_type() == "dhcp") {
			var lease = secondary_wanlink_lease();
			var expires = secondary_wanlink_expires();
			showtext($j("#secondary_lease")[0], format_time(lease, "Renewing..."));
			showtext($j("#secondary_expires")[0], format_time(expires, "Expired"));
		}
	}
}
function update_all_xip(wanxip, wanxdns, wanxgateway, unit) {
	var dnsArray = wandns.split(" ");
	var have_dns = !(dnsArray[0] || dnsArray[1]);
	var dnsArray = wanxdns.split(" ");
	var have_ip = false;
	var have_gateway = false;
	var have_lease = false;
	var lease = 0;
	var expires = 0;

	var type = (unit == 0) ? wanlink_xtype() : secondary_wanlink_xtype();
	if (type == "dhcp" || type == "static") {
		have_ip = true;
		have_gateway = !(wanxgateway == "" || wanxgateway == "0.0.0.0");
		if (type == "dhcp") {
			have_lease = true;
			lease = (unit == 0) ? wanlink_xlease() : secondary_wanlink_xlease();
			expires = (unit == 0) ? wanlink_xexpires() : secondary_wanlink_xexpires();
		}
	}

	if (unit == 0) {
		showtext2($j("#xWANIP")[0], wanxip, have_ip);
		showtext2($j("#xDNS1")[0], dnsArray[0], have_dns && dnsArray[0]);
		showtext2($j("#xDNS2")[0], dnsArray[1], have_dns && dnsArray[1]);
		showtext2($j("#xgateway")[0], wanxgateway, have_gateway);
		showtext2($j("#xlease")[0], format_time(lease, "Renewing..."), have_lease);
		showtext2($j("#xexpires")[0], format_time(expires, "Expired"), have_lease);
	}
	else {
		showtext2($j("#secondary_xWANIP")[0], wanxip, have_ip);
		showtext2($j("#secondary_xDNS1")[0], dnsArray[0], have_dns && dnsArray[0]);
		showtext2($j("#secondary_xDNS2")[0], dnsArray[1], have_dns && dnsArray[1]);
		showtext2($j("#secondary_xgateway")[0], wanxgateway, have_gateway);
		showtext2($j("#secondary_xlease")[0], format_time(lease, "Renewing..."), have_lease);
		showtext2($j("#secondary_xexpires")[0], format_time(expires, "Expired"), have_lease);
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
  $j.ajax({
    url: '/status.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_wanip();", 3000);
    },
    success: function(response) {
		wanip = wanlink_ipaddr();
		wandns = wanlink_dns();
		wangateway = wanlink_gateway();
		wanxip = wanlink_xipaddr();
		wanxdns = wanlink_xdns();
		wanxgateway = wanlink_xgateway();
		if(dualWAN_support){
			secondary_wanip = secondary_wanlink_ipaddr();
			secondary_wandns = secondary_wanlink_dns();
			secondary_wangateway = secondary_wanlink_gateway();
			secondary_wanxip = secondary_wanlink_xipaddr();
			secondary_wanxdns = secondary_wanlink_xdns();
			secondary_wanxgateway = secondary_wanlink_xgateway();
		}

		if(old_link_internet == -1)
			old_link_internet = update_wan_state(wanstate, wanauxstate);

		if(update_wan_state(wanstate, wanauxstate) != old_link_internet){
			refreshpage();
		}
		else{
			update_all_ip(wanip, wandns, wangateway, 0);
			update_all_xip(wanxip, wanxdns, wanxgateway, 0);
			if(dualWAN_support){
				update_all_ip(secondary_wanip, secondary_wandns, secondary_wangateway, 1);
				update_all_xip(secondary_wanxip, secondary_wanxdns, secondary_wanxgateway, 1);
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
	if(dualWAN_support)
		parent.location.href = '/Advanced_WANPort_Content.asp';
	else	
		parent.location.href = '/Advanced_WAN_Content.asp';
}

function gotoSiteSurvey(){
	if(sw_mode == 2)
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey&band='+'<% nvram_get("wl_unit"); %>';
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
<input type="hidden" name="wan_unit" value="<% get_wan_unit(); %>">
<!--input type="hidden" name="wlc_ssid" value="<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>" disabled-->
<input type="hidden" name="dslx_link_enable" value="" disabled>
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
<tr id="wan_enable_button">
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px;"><#ConnectionStatus#></p>
    		<div class="left" style="width:94px; float:right;" id="radio_wan_enable"></div>
				<div class="clear"></div>
				<script type="text/javascript">
						$j('#radio_wan_enable').iphoneSwitch('<% nvram_get("wan_enable"); %>', 
							 function() {
								document.internetForm.wan_enable.value = "1";
								if (parent.dsl_support) {
									document.internetForm.dslx_link_enable.value = "1";
									document.internetForm.dslx_link_enable.disabled = false;
								}
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 },
							 function() {
								document.internetForm.wan_enable.value = "0";
								if (parent.dsl_support) {
									document.internetForm.dslx_link_enable.value = "0";
									document.internetForm.dslx_link_enable.disabled = false;
								}
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 },
							 {
								switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
							 }
						);
				</script>
    		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>


<tr id=dualwan_row_main style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">Dual WAN</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_current"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id=dualwan_row_primary style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">WAN Type</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_primary_if"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id=dualwan_row_secondary style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">WAN Type</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_secondary_if"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="dualwan_mode_ctrl" style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">Dual WAN Mode</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="dualwan_mode"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr id="loadbalance_config_ctrl" style="display:none">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm">Load Balance Configuration</p>
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
<tr style="display:none;" id="secondary_WANIP_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#WAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_WANIP"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xWANIP"></p>
    		<span id="wan_status" style="display:none"></span>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="primary_DNS_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" >DNS</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS2"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xDNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xDNS2"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_DNS_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" >DNS</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_DNS2"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xDNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="secondary_xDNS2"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr id="primary_gateway_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" ><#RouterConfig_GWStaticGW_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="gateway"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="xgateway"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr style="display:none;" id="secondary_gateway_ctrl">
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" ><#RouterConfig_GWStaticGW_itemname#></p>
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
<tr id="goSetting" style="display:none">
	<td height="50" style="padding:10px 15px 0px 15px;">
		<p class="formfonttitle_nwm" style="float:left;width:138px;">Go to WAN setting</p>
		<input type="button" class="button_gen" onclick="goToWAN();" value="<#btn_go#>" style="margin-top:-10px;">
		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
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
</body>
</html>
