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
<title><#Web_Title#> - IPTV</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

var original_switch_stb_x = '<% nvram_get("switch_stb_x"); %>';
var original_switch_wantag = '<% nvram_get("switch_wantag"); %>';

var wans_lanport = '<% nvram_get("wans_lanport"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var manual_stb = rc_support.search("manual_stb");

function initial(){
	if (dsl_support != -1) {
		document.form.action_script.value = "reboot";		
		document.form.action_wait.value = "<% get_default_reboot_time(); %>";				
	}
	show_menu();
	if(dsl_support == -1) {	
		ISP_Profile_Selection(original_switch_wantag);
	}
	document.form.switch_stb_x.value = original_switch_stb_x;	

	disable_udpxy();
	
	if(Rawifi_support == -1)	//rawifi platform without this item, by Viz 2012.01
		$('enable_eff_multicast_forward').style.display="";		

	if(parent.rc_support.search("manual_stb") == -1)
		document.form.switch_wantag.remove(6);
}

function load_ISP_profile() {
        if(document.form.switch_wantag.value == "unifi_home") {
		document.form.switch_stb_x.value = "4";
                document.form.switch_wan0tagid.value = "500";
                document.form.switch_wan0prio.value = "0";
                document.form.switch_wan1tagid.value = "600";
                document.form.switch_wan1prio.value = "0";
                document.form.switch_wan2tagid.value = "0";
                document.form.switch_wan2prio.value = "0";
        }
        else if(document.form.switch_wantag.value == "unifi_biz") {
		document.form.switch_stb_x.value = "0";
                document.form.switch_wan0tagid.value = "500";
                document.form.switch_wan0prio.value = "0";
                document.form.switch_wan1tagid.value = "0";
                document.form.switch_wan1prio.value = "0";
                document.form.switch_wan2tagid.value = "0";
                document.form.switch_wan2prio.value = "0";
        }
        else if(document.form.switch_wantag.value == "singtel_mio") {
		document.form.switch_stb_x.value = "6";
                document.form.switch_wan0tagid.value = "10";
                document.form.switch_wan0prio.value = "0";
                document.form.switch_wan1tagid.value = "20";
                document.form.switch_wan1prio.value = "4";
                document.form.switch_wan2tagid.value = "30";
                document.form.switch_wan2prio.value = "4";
        }
        else if(document.form.switch_wantag.value == "singtel_others") {
		document.form.switch_stb_x.value = "4";
                document.form.switch_wan0tagid.value = "10";
                document.form.switch_wan0prio.value = "0";
                document.form.switch_wan1tagid.value = "20";
                document.form.switch_wan1prio.value = "4";
                document.form.switch_wan2tagid.value = "0";
                document.form.switch_wan2prio.value = "0";
        }
        else if(document.form.switch_wantag.value == "m1_fiber") {
                document.form.switch_stb_x.value = "3";
                document.form.switch_wan0tagid.value = "1103";
                document.form.switch_wan0prio.value = "1";
                document.form.switch_wan1tagid.value = "0";
                document.form.switch_wan1prio.value = "0";
                document.form.switch_wan2tagid.value = "1107";
                document.form.switch_wan2prio.value = "1";
        }else{}

}

function ISP_Profile_Selection(isp){
	if(isp == "none"){
		$("wan_stb_x").style.display = "";
		$("wan_iptv_x").style.display = "none";
		$("wan_voip_x").style.display = "none";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";
		document.form.switch_wantag.value = "none";
		document.form.switch_stb_x.value = "0";
	}
  	else if(isp == "unifi_home"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "";
		$("wan_voip_x").style.display = "none";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";
		document.form.switch_wantag.value = "unifi_home";
		document.form.switch_stb_x.value = "4";
	}
	else if(isp == "unifi_biz"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "none";
		$("wan_voip_x").style.display = "none";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";
		document.form.switch_wantag.value = "unifi_biz";
		document.form.switch_stb_x.value = "0";
	}
	else if(isp == "singtel_mio"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "";
		$("wan_voip_x").style.display = "";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";	
		document.form.switch_wantag.value = "singtel_mio";
		document.form.switch_stb_x.value = "6";
	}
	else if(isp == "singtel_others"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "";
		$("wan_voip_x").style.display = "none";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";
		document.form.switch_wantag.value = "singtel_others";
		document.form.switch_stb_x.value = "4";
	}
	else if(isp == "m1_fiber"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "none";
		$("wan_voip_x").style.display = "";
		$("wan_internet_x").style.display = "none";
		$("wan_iptv_port4_x").style.display = "none";
		$("wan_voip_port3_x").style.display = "none";
		document.form.switch_wantag.value = "m1_fiber";
                document.form.switch_stb_x.value = "3";
	}
	else if(isp == "manual"){
		$("wan_stb_x").style.display = "none";
		$("wan_iptv_x").style.display = "";
		$("wan_voip_x").style.display = "";
		$("wan_internet_x").style.display = "";
		$("wan_iptv_port4_x").style.display = "";
		$("wan_voip_port3_x").style.display = "";
		document.form.switch_wantag.value = "manual";
		document.form.switch_stb_x.value = "6";
	}
}

function validForm(){
	if (dsl_support == -1) {
        if(document.form.switch_wantag.value == "manual")
        {
                if(document.form.switch_wan0tagid.value.length > 0)
                {
                        if(!validate_range(document.form.switch_wan0tagid, 2, 4094))
                                return false;
                }
                if(document.form.switch_wan1tagid.value.length > 0)
                {
                        if(!validate_range(document.form.switch_wan1tagid, 2, 4094))
                                return false;
                }
                if(document.form.switch_wan2tagid.value.length > 0)
                {
                        if(!validate_range(document.form.switch_wan2tagid, 2, 4094))
                                return false;
                }

                if(document.form.switch_wan0prio.value.length > 0 && !validate_range(document.form.switch_wan0prio, 0, 7))
                        return false;

                if(document.form.switch_wan1prio.value.length > 0 && !validate_range(document.form.switch_wan1prio, 0, 7))
                        return false;

                if(document.form.switch_wan2prio.value.length > 0 && !validate_range(document.form.switch_wan2prio, 0, 7))
                        return false;
        }
	}
	return true;
}

function applyRule(){
	// dualwan LAN port should not equal to IPTV port
	if (dualWAN_support != -1) {
		var tmp_pri_if = wans_dualwan_orig.split(" ")[0].toUpperCase();
		var tmp_sec_if = wans_dualwan_orig.split(" ")[1].toUpperCase();	
		if (tmp_pri_if != 'LAN' || tmp_sec_if != 'LAN') {
			var port_conflict = false;
			var iptv_port = document.form.switch_stb_x.value;
			if (wans_lanport == "1") {
				if (iptv_port == "1" || iptv_port == "5") {
					port_conflict = true;
				}
			}
			else if (wans_lanport == "2") {		
				if (iptv_port == "2" || iptv_port == "5") {
					port_conflict = true;
				}
			}
			else if (wans_lanport == "3") {				
				if (iptv_port == "3" || iptv_port == "6") {
					port_conflict = true;
				}
			}
			else if (wans_lanport == "4") {				
				if (iptv_port == "4" || iptv_port == "6") {
					port_conflict = true;
				}
			}
			if (port_conflict) {
				alert("IPTV port number is same as dual wan LAN port number");
				return;
			}
		}
	}

	if(dsl_support == -1) {	
		  if( (original_switch_stb_x != document.form.switch_stb_x.value) 
			||  (original_switch_wantag != document.form.switch_wantag.value)){
						FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
				}
			load_ISP_profile();			
	} 

	if(validForm()){

		if(wl6_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver

		if(document.form.udpxy_enable_x.value != 0 && document.form.udpxy_enable_x.value != ""){
			if(!validate_range(document.form.udpxy_enable_x, 1024, 65535)){
					document.form.udpxy_enable_x.focus();
					document.form.udpxy_enable_x.select();
					return false;
			}else{ 
				showLoading();
				document.form.submit();
			}
		}else{
			showLoading();
			document.form.submit();		
		}	
	}
}

function valid_udpxy(){
	if(document.form.udpxy_enable_x.value != 0 && document.form.udpxy_enable_x.value != ""){
		if(!validate_range(document.form.udpxy_enable_x, 1024, 65535)){
				document.form.udpxy_enable_x.focus();
				document.form.udpxy_enable_x.select();
				return false;
		}else 
			return true;
	}	
}

function disable_udpxy(){
	if(document.form.mr_enable_x.value == 1){
		return change_common_radio(document.form.mr_enable_x, 'RouterConfig', 'mr_enable_x', '1');
	}
	else{	
		return change_common_radio(document.form.mr_enable_x, 'RouterConfig', 'mr_enable_x', '0');
	}	
}// The input fieldof UDP proxy does not relate to Mutlicast Routing. 

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="/Advanced_IPTV_Content.asp">
<input type="hidden" name="next_page" value="/Advanced_IPTV_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_net">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_2#> - IPTV</div>
      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div class="formfontdesc"><#LANHostConfig_displayIPTV_sectiondesc#></div>
	  
	  
	  <!-- IPTV & VoIP Setting -->
	  
		<!--###HTML_PREP_START###-->		
	  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">

	  	<thead>
		<tr>
    	<td colspan="2">Port</td>
		</tr>
	
	</thead>
	    	<tr>
	    	<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,28);"><#Select_ISPfile#></a></th>
  		<td>
		    <select name="switch_wantag" class="input_option" onChange="ISP_Profile_Selection(this.value)">
						<option value="none" <% nvram_match( "switch_wantag", "none", "selected"); %>><#wl_securitylevel_0#></option>
						<option value="unifi_home" <% nvram_match( "switch_wantag", "unifi_home", "selected"); %>>Unifi-Home</option>
						<option value="unifi_biz" <% nvram_match( "switch_wantag", "unifi_biz", "selected"); %>>Unifi-Business</option>
						<option value="singtel_mio" <% nvram_match( "switch_wantag", "singtel_mio", "selected"); %>>Singtel-MIO</option>
						<option value="singtel_others" <% nvram_match( "switch_wantag", "singtel_others", "selected"); %>>Singtel-Others</option>
						<option value="m1_fiber" <% nvram_match("switch_wantag", "m1_fiber", "selected"); %>>M1-Fiber</option>
						<option value="manual" <% nvram_match( "switch_wantag", "manual", "selected"); %>>Manual</option>
		    </select>
  		</td>
		</tr>
		<tr id="wan_stb_x">
		<th width="30%"><#Layer3Forwarding_x_STB_itemname#></th>
		<td align="left">
		    <select name="switch_stb_x" class="input_option">
			<option value="0" <% nvram_match( "switch_stb_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
			<option value="1" <% nvram_match( "switch_stb_x", "1", "selected"); %>>LAN1</option>
			<option value="2" <% nvram_match( "switch_stb_x", "2", "selected"); %>>LAN2</option>
			<option value="3" <% nvram_match( "switch_stb_x", "3", "selected"); %>>LAN3</option>
			<option value="4" <% nvram_match( "switch_stb_x", "4", "selected"); %>>LAN4</option>
			<option value="5" <% nvram_match( "switch_stb_x", "5", "selected"); %>>LAN1 & LAN2</option>
			<option value="6" <% nvram_match( "switch_stb_x", "6", "selected"); %>>LAN3 & LAN4</option>
		    </select>
		</td>
		</tr>
		<tr id="wan_iptv_x">
	  	<th width="30%">IPTV STB Port</th>
	  	<td>LAN4</td>
		</tr>
		<tr id="wan_voip_x">
	  	<th width="30%">VoIP Port</th>
	  	<td>LAN3</td>
		</tr>
		<tr id="wan_internet_x">
	  	<th width="30%">Internet</th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan0tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan0tagid"); %>">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan0prio" class="input_6_table" maxlength="1" value="<% nvram_get( "switch_wan0prio"); %>">
	  	</td>
		</tr>
	    	<tr id="wan_iptv_port4_x">
	    	<th width="30%">IPTV (LAN port 4)</th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan1tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan1tagid"); %>">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan1prio" class="input_6_table" maxlength="1" value="<% nvram_get( "switch_wan1prio"); %>">
	  	</td>
		</tr>
		<tr id="wan_voip_port3_x">
	  	<th width="30%">VoIP (LAN port 3)</th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan2tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan2tagid"); %>">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan2prio" class="input_6_table" maxlength="1" value="<% nvram_get( "switch_wan2prio"); %>">
	  	</td>
		</tr>
		</table>
<!--###HTML_PREP_ELSE###-->
<!--
[DSL-N55U][DSL-N55U-B]
{DSL do not support unifw}
	  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
		<tr>
            	<td colspan="2">Port</td>
            	</tr>
		</thead>
		<tr id="wan_stb_x">
		<th width="30%"><#Layer3Forwarding_x_STB_itemname#></th>
		<td align="left">
		    <select name="switch_stb_x" class="input_option">
			<option value="0" <% nvram_match( "switch_stb_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
			<option value="1" <% nvram_match( "switch_stb_x", "1", "selected"); %>>LAN1</option>
			<option value="2" <% nvram_match( "switch_stb_x", "2", "selected"); %>>LAN2</option>
			<option value="3" <% nvram_match( "switch_stb_x", "3", "selected"); %>>LAN3</option>
			<option value="4" <% nvram_match( "switch_stb_x", "4", "selected"); %>>LAN4</option>
			<option value="5" <% nvram_match( "switch_stb_x", "5", "selected"); %>>LAN1 & LAN2</option>
			<option value="6" <% nvram_match( "switch_stb_x", "6", "selected"); %>>LAN3 & LAN4</option>
		    </select>
		</td>
		</tr>
		</table>
-->
<!--###HTML_PREP_END###-->	  
	  

		<!-- End of IPTV & VoIP -->	  
	  
		  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:10px;">
	  	<thead>
		<tr>
            	<td colspan="2"><#IPConnection_BattleNet_sectionname#></td>
            	</tr>
		</thead>

		<tr>
			<th><#RouterConfig_GWDHCPEnable_itemname#></th>
			<td>
				<select name="dr_enable_x" class="input_option">
				<option value="0" <% nvram_match("dr_enable_x", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
				<option value="1" <% nvram_match("dr_enable_x", "1","selected"); %> >Microsoft</option>
				<option value="2" <% nvram_match("dr_enable_x", "2","selected"); %> >RFC3442</option>
				<option value="3" <% nvram_match("dr_enable_x", "3","selected"); %> >RFC3442 & Microsoft</option>
				</select>
			</td>
		</tr>

			<!--tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,11);"><#RouterConfig_GWMulticastEnable_itemname#></a></th>
				<td>
					<input type="radio" value="1" name="mr_enable_x" class="input" onClick="disable_udpxy();" <% nvram_match("mr_enable_x", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" value="0" name="mr_enable_x" class="input" onClick="disable_udpxy();" <% nvram_match("mr_enable_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr-->	
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,11);"><#RouterConfig_GWMulticastEnable_itemname#></a></th>
				<td>
          <select name="mr_enable_x" class="input_option">
            <option value="0" <% nvram_match("mr_enable_x", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
           	<option value="1" <% nvram_match("mr_enable_x", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
          </select>
				</td>
			</tr>

					<tr id="enable_eff_multicast_forward" style="display:none;">
						<th>Enable efficient multicast forwarding</th>
						<td>
                  				<select name="emf_enable" class="input_option">
                    					<option value="0" <% nvram_match("emf_enable", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    					<option value="1" <% nvram_match("emf_enable", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  				</select>
						</td>
					</tr>
			<!-- 2008.03 James. patch for Oleg's patch. } -->
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(6, 6);"><#RouterConfig_IPTV_itemname#></a></th>
     		<td>
     			<input id="udpxy_enable_x" type="text" maxlength="5" class="input_6_table" name="udpxy_enable_x" value="<% nvram_get("udpxy_enable_x"); %>" onkeypress="return is_number(this,event);">
     		</td>
     	</tr>
		</table>	

		<div class="apply_gen">
			<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
		</div>
		
	  </td>
	</tr>

	</tbody>	
  </table>		
					
		</td>
	</form>					
				</tr>
			</table>				
			<!--===================================End of Main Content===========================================-->
</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
