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
<title><#Web_Title#> - Passpoint</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
<% wl_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var ouilist_orig = '<% nvram_get("wl_ouilist"); %>';
var domainlist_orig = '<% nvram_get("wl_domainlist"); %>';
//var domainlist_orig = 'dsgf.dsdf dfgdf.dgdfg.dfg dffd.dhdg';
var realmlist_orig = '<% nvram_get("wl_realmlist"); %>';
var the3gpplist_orig = '<% nvram_get("wl_3gpplist"); %>';
//var the3gpplist_orig = '245:456;324345:78768;4545:3434;';

function initial(){
	show_menu();
	
	get_ssid_info();
	get_wireless_info();
	get_ouilist();
	get_domainlist();
	get_realmlist();
	get_3gpplist();
	get_passpoint_info();
}

function get_ssid_info(){
		//based_modelid+"<br>
		document.getElementById("ssid_info").innerHTML = decodeURIComponent('<% nvram_char_to_ascii("", "wl_ssid"); %>');
																							
}

function get_wireless_info(){
		//based_modelid+"<br>
		document.getElementById("wireless_info").innerHTML = "Auth mode : <% nvram_get("wl_auth_mode_x"); %>";
}

function get_ouilist(){
		//Split by ';'	
		var oui_array = ouilist_orig.split(";");
		for(var i=0; i<oui_array.length; i++){
			switch(i){
				case 0:
						document.form.wl_ouilist_0.value = oui_array[0];
						break;
				case 1:
						document.form.wl_ouilist_1.value = oui_array[1];
						break;
				case 2:
						document.form.wl_ouilist_2.value = oui_array[2];
						break;
				case 3:
						document.form.wl_ouilist_3.value = oui_array[3];
						break;	
				default:
						break;		
			}
		}
}

function get_domainlist(){
		//Split by '\s'
		var domain_array = domainlist_orig.split(" ");
		for(var i=0; i<domain_array.length; i++){
			switch(i){
				case 0:
						document.form.wl_domainlist_0.value = domain_array[0];
						break;
				case 1:
						document.form.wl_domainlist_1.value = domain_array[1];
						break;
				case 2:
						document.form.wl_domainlist_2.value = domain_array[2];
						break;
				case 3:
						document.form.wl_domainlist_3.value = domain_array[3];
						break;	
				case 4:
						document.form.wl_domainlist_4.value = domain_array[4];
						break;
				case 5:
						document.form.wl_domainlist_5.value = domain_array[5];
						break;
				case 6:
						document.form.wl_domainlist_6.value = domain_array[6];
						break;
				case 7:
						document.form.wl_domainlist_7.value = domain_array[7];
						break;						
				default:
						break;		
			}
		}	
	
}


function get_realmlist(){
		//Split by '?'
		var realm_array = realmlist_orig.split("?"); 
		for(var i=0; i<realm_array.length; i++){
			switch(i){
				case 0:
						document.form.wl_realmlist_0.value = realm_array[0];
						break;
				case 1:
						document.form.wl_realmlist_1.value = realm_array[1];
						break;
				case 2:
						document.form.wl_realmlist_2.value = realm_array[2];
						break;
				case 3:
						document.form.wl_realmlist_3.value = realm_array[3];
						break;	
				case 4:
						document.form.wl_realmlist_4.value = realm_array[4];
						break;
				default:
						break;		
			}
		}
}

function get_3gpplist(){
		//Split by ';' for CN info & Split by ':' for Country and Network
		var the3gpp_array = the3gpplist_orig.split(";"); 
		for(var i=0; i<the3gpp_array.length; i++){
			var the3gpp_country_network = the3gpp_array[i].split(":");
			if(the3gpp_country_network[1]){
					switch(i){
						case 0:
								document.form.wl_3gpplist_0_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_0_1.value = the3gpp_country_network[1];
								break;
						case 1:
								document.form.wl_3gpplist_1_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_1_1.value = the3gpp_country_network[1];
								break;
						case 2:
								document.form.wl_3gpplist_2_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_2_1.value = the3gpp_country_network[1];
								break;
						case 3:
								document.form.wl_3gpplist_3_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_3_1.value = the3gpp_country_network[1];
								break;	
						case 4:
								document.form.wl_3gpplist_4_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_4_1.value = the3gpp_country_network[1];
								break;
						case 5:
								document.form.wl_3gpplist_5_0.value = the3gpp_country_network[0];
								document.form.wl_3gpplist_5_1.value = the3gpp_country_network[1];
								break;						
						default:
								break;		
					}
			}		
		}
	
}

function get_passpoint_info(){
		if("<% nvram_get("wl0_hs2en"); %>" == "1")
			var wl0hs2en = "ON";
		else	
			var wl0hs2en = "OFF";
		document.getElementById("passpoint_info").innerHTML = "2.4GHz: "+wl0hs2en;
			
		if(band5g_support){	
			if("<% nvram_get("wl1_hs2en"); %>" == "1")
				var wl1hs2en = "ON";
			else	
				var wl1hs2en = "OFF";
				
			document.getElementById("passpoint_info").innerHTML += "<br>5GHz: "+wl1hs2en;
		}						
}

function applyRule(){
	if(validForm()){
		
		if(document.form.wl_u11en_x[1].checked)
				document.form.wl0_u11en.value = "0";
		else
				document.form.wl0_u11en.value = "1";
		
		document.form.wl0_iwnettype.value = document.form.wl_iwnettype_x.value;
		
		for(var y=0;y<=3;y++){
				if(document.form.wl0_ouilist.value == "" && document.getElementById("wl_ouilist_"+y).value != "")
						document.form.wl0_ouilist.value = document.getElementById("wl_ouilist_"+y).value;
				else if(document.form.wl0_ouilist.value != "" && document.getElementById("wl_ouilist_"+y).value != ""){
						document.form.wl0_ouilist.value += ";";
						document.form.wl0_ouilist.value += document.getElementById("wl_ouilist_"+y).value;
				}		
		}
		//alert(document.form.wl0_ouilist.value);
		for(var y=0;y<=7;y++){
				if(document.form.wl0_domainlist.value == "" && document.getElementById("wl_domainlist_"+y).value != "")
						document.form.wl0_domainlist.value = document.getElementById("wl_domainlist_"+y).value;
				else if(document.form.wl0_domainlist.value != "" && document.getElementById("wl_domainlist_"+y).value != ""){
						document.form.wl0_domainlist.value += " ";
						document.form.wl0_domainlist.value += document.getElementById("wl_domainlist_"+y).value;
				}		
		}
		//alert(document.form.wl0_domainlist.value);		
		
		for(var y=0;y<=4;y++){
				if(document.form.wl0_realmlist.value == "" && document.getElementById("wl_realmlist_"+y).value != "")
						document.form.wl0_realmlist.value = document.getElementById("wl_realmlist_"+y).value;
				else if(document.form.wl0_realmlist.value != "" && document.getElementById("wl_realmlist_"+y).value != ""){
						document.form.wl0_realmlist.value += "?";
						document.form.wl0_realmlist.value += document.getElementById("wl_realmlist_"+y).value;
				}		
		}
		//alert(document.form.wl0_realmlist.value);
		
		for(var y=0;y<=5;y++){
				if(document.form.wl0_3gpplist.value == "" && (document.getElementById("wl_3gpplist_"+y+"_0").value != "" || document.getElementById("wl_3gpplist_"+y+"_1").value != ""))
						document.form.wl0_3gpplist.value = document.getElementById("wl_3gpplist_"+y+"_0").value+":"+document.getElementById("wl_3gpplist_"+y+"_1").value;
				else if(document.form.wl0_3gpplist.value != "" && (document.getElementById("wl_3gpplist_"+y+"_0").value != "" || document.getElementById("wl_3gpplist_"+y+"_1").value != "")){
						document.form.wl0_3gpplist.value += ";";
						document.form.wl0_3gpplist.value += document.getElementById("wl_3gpplist_"+y+"_0").value+":"+document.getElementById("wl_3gpplist_"+y+"_1").value;
				}		
		}
		//alert(document.form.wl0_3gpplist.value);		
		
		document.form.wl1_u11en.value = document.form.wl0_u11en.value;
		document.form.wl1_iwnettype.value = document.form.wl0_iwnettype.value;
		document.form.wl1_ouilist.value = document.form.wl0_ouilist.value;
		document.form.wl1_domainlist.value = document.form.wl0_domainlist.value;
		document.form.wl1_realmlist.value = document.form.wl0_realmlist.value;
		document.form.wl1_3gpplist.value = document.form.wl0_3gpplist.value;
				
		showLoading();
		document.form.submit();
	}
}

function validForm(){		

	/*
	if(!validate_ipaddr_final(document.form.wl_radius_ipaddr, 'wl_radius_ipaddr'))
		return false;
	
	if(!validate_range(document.form.wl_radius_port, 0, 65535))
		return false;
	
	if(!validate_string(document.form.wl_radius_key))
		return false;
	*/
	if(!validate_range(document.form.wl_maxassoc, 1, 128))
			return false;				
	
	return true;
}

function enable_u11(flag){
	if(flag == 1){
		document.getElementById("MainTable2").style.display = "";
		
	}
	else{
		document.getElementById("MainTable2").style.display = "none";
		
	}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Passpoint_hidden.asp">
<input type="hidden" name="next_page" value="Passpoint_hidden.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl0_u11en" value="">
<input type="hidden" name="wl0_iwnettype" value="">
<input type="hidden" name="wl0_ouilist" value="">
<input type="hidden" name="wl0_domainlist" value="">
<input type="hidden" name="wl0_realmlist" value="">
<input type="hidden" name="wl0_3gpplist" value="">
<input type="hidden" name="wl1_u11en" value="">
<input type="hidden" name="wl1_iwnettype" value="">
<input type="hidden" name="wl1_ouilist" value="">
<input type="hidden" name="wl1_domainlist" value="">
<input type="hidden" name="wl1_realmlist" value="">
<input type="hidden" name="wl1_3gpplist" value="">

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		
		<td valign="top" width="202">				
		<div id="mainMenu"></div>	
		<div id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle">Passpoint (Hotspot)</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc">Passpoint transforms the way users connect to Wi-Fi. This technology makes the process of finding and connecting to Wi-Fi networks seamless. Passpoint also delivers greater security protection to better safeguard your data. Your T-Mobile SIM card provides the necessary authentication to automatically connect your device to the <#Web_Title2#> Router.
				<br><br>If your T-Mobile handset is Passpoint capable, it should automatically connect to your <#Web_Title2#> Router.
		  </div>
		  
		<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
		<tr>	
			<th><#Interface#></th>
			<td>
				<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
					<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
					<option class="content_input_fd" value="1"<% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
				</select>			
			</td>	
		</tr>	
		<tr>
			<th width="30%">Passpoint SSID</th>
			<td>
				<!-- content? -->
				<div id="ssid_info"></div>
			</td>
		</tr>			
		<tr>
			<th width="30%">General Information</th>
			<td>
				<!-- content? -->
				<div id="wireless_info"></div>
			</td>
		</tr>
		<tr style="display:none;">
			<th width="30%">802.11u Status</th>
			<td>
					<input type="radio" name="wl_u11en_x" value="1" onClick="enable_u11(1);" <% nvram_match("wl_u11en", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" name="wl_u11en_x" value="0" onClick="enable_u11(0);" <% nvram_match("wl_u11en", "0", "checked"); %>><#checkbox_No#>
			</td>
		</tr>				
		<tr>
			<th width="30%">802.11u Network Type</th>
			<td>
					<select name="wl_iwnettype_x" class="input_option">
	  				<option value="0" <% nvram_match("wl_iwnettype", "0", "selected"); %>>Private Network</option>
	  				<option value="1" <% nvram_match("wl_iwnettype", "1", "selected"); %>>Private Network with Guest Access</option>
	  				<option value="2" <% nvram_match("wl_iwnettype", "2", "selected"); %>>Chargable Public Network</option>
	  				<option value="3" <% nvram_match("wl_iwnettype", "3", "selected"); %>>Free Public Network</option>
	  				<option value="4" <% nvram_match("wl_iwnettype", "4", "selected"); %>>Emergency Services Only Network</option>
	  				<option value="5" <% nvram_match("wl_iwnettype", "5", "selected"); %>>Personal Device Network</option>
	  				<option value="14" <% nvram_match("wl_iwnettype", "14", "selected"); %>>Test or Experimental</option>
	  				<option value="15" <% nvram_match("wl_iwnettype", "15", "selected"); %>>Wildcard</option>
				  </select>
			</td>
		</tr>
		</table>
				
		<table id="MainTable2" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" style="margin-top:10px;">
			<tr>
				<th width="30%">OUI List</th>
				<td>					
					OUI Name<br>
					<input type="text" id="wl_ouilist_0" name="wl_ouilist_0" class="input_15_table" maxlength="16" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_ouilist_1" name="wl_ouilist_1" class="input_15_table" maxlength="16" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_ouilist_2" name="wl_ouilist_2" class="input_15_table" maxlength="16" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_ouilist_3" name="wl_ouilist_3" class="input_15_table" maxlength="16" value="" onkeypress="return is_string(this, event)">
				</td>
			</tr>
			<tr>
				<th width="30%">Domain List</th>
				<td>
					<input type="text" id="wl_domainlist_0" name="wl_domainlist_0" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_1" name="wl_domainlist_1" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_2" name="wl_domainlist_2" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_3" name="wl_domainlist_3" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_4" name="wl_domainlist_4" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_5" name="wl_domainlist_5" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_6" name="wl_domainlist_6" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_domainlist_7" name="wl_domainlist_7" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)">
				</td>
			</tr>
			<tr>
				<th width="30%">Realm List</th>
				<td>					
					<input type="text" id="wl_realmlist_0" name="wl_realmlist_0" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_realmlist_1" name="wl_realmlist_1" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_realmlist_2" name="wl_realmlist_2" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_realmlist_3" name="wl_realmlist_3" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_realmlist_4" name="wl_realmlist_4" class="input_32_table" maxlength="32" value="" onkeypress="return is_string(this, event)">
				</td>
			</tr>			
			<tr>
				<th width="30%">Cellular Network Information List</th>
				<td>										
					Country Code : Network Code<br>
					<input type="text" id="wl_3gpplist_0_0" name="wl_3gpplist_0_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_0_1" name="wl_3gpplist_0_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_3gpplist_1_0" name="wl_3gpplist_1_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_1_1" name="wl_3gpplist_1_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_3gpplist_2_0" name="wl_3gpplist_2_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_2_1" name="wl_3gpplist_2_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_3gpplist_3_0" name="wl_3gpplist_3_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_3_1" name="wl_3gpplist_3_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_3gpplist_4_0" name="wl_3gpplist_4_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_4_1" name="wl_3gpplist_4_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"><br>
					<input type="text" id="wl_3gpplist_5_0" name="wl_3gpplist_5_0" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)"> : <input type="text" id="wl_3gpplist_5_1" name="wl_3gpplist_5_1" class="input_6_table" maxlength="5" value="" onkeypress="return is_string(this, event)">
				</td>
			</tr>
			<tr>
				<th width="30%">Passpoint Status</th>
				<td>
					<!-- content? -->
					<div id="passpoint_info"></div>
				</td>
			</tr>			
			<tr>
				<th width="30%">Number of Users allowed to connect</th>
				<td>
					<input type="text" name="wl_maxassoc" class="input_3_table" maxlength="3" value="<% nvram_get("wl_maxassoc"); %>" onkeypress="return is_number(this, event)">
				</td>
			</tr>
			<tr>
			<th>
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,1);">
			  	<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#></a>			  
			</th>
			<td>
				<input type="text" maxlength="15" class="input_15_table" name="wl_radius_ipaddr" value="<% nvram_get("wl_radius_ipaddr"); %>" onKeyPress="return is_ipaddr(this, event)">
			</td>
		</tr>
		<tr>
			<th>
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,2);">
			  	<#WLANAuthentication11a_ExAuthDBPortNumber_itemname#></a>
			</th>
			<td>
				<input type="text" maxlength="5" class="input_6_table" name="wl_radius_port" value="<% nvram_get("wl_radius_port"); %>" onkeypress="return is_number(this,event)"/>
			</td>
		</tr>
		<tr>
			<th >
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,3);">
				<#WLANAuthentication11a_ExAuthDBPassword_itemname#></a>
			</th>
			<td>
				<input type="password" autocapitalization="off" maxlength="64" class="input_32_table" name="wl_radius_key" value="<% nvram_get("wl_radius_key"); %>">
			</td>
		</tr>
		</table>
		
			<div id="submitBtn" class="apply_gen">
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
<!--===================================Ending of Main Content===========================================-->		

	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
