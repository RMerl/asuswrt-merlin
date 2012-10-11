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

<title><#Web_Title#> - <#menu5_1_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
</script>

<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';

<% login_state_hook(); %>
<% wl_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_mac = login_mac_str();
var wl_macnum_x = '<% nvram_get("wl_macnum_x"); %>';
var smac = client_mac.split(":");
var simply_client_mac = smac[0] + smac[1] + smac[2] + smac[3] + smac[4] + smac[5];

var wl_maclist_x_array = '<% nvram_get("wl_maclist_x"); %>';

function initial(){
	show_menu();

	if(sw_mode == 2 && '<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>'){
		for(var i=3; i>=3; i--)
			$("MainTable1").deleteRow(i);
		for(var i=2; i>=0; i--)
			$("MainTable2").deleteRow(i);
		$("repeaterModeHint").style.display = "";
		$("wl_maclist_x_Block").style.display = "none";
		$("submitBtn").style.display = "none";
	}
	else
		show_wl_maclist_x();

	if(band5g_support == -1)	
		$("wl_unit_field").style.display = "none";
}

function show_wl_maclist_x(){
	var wl_maclist_x_row = wl_maclist_x_array.split('&#60');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table"  id="wl_maclist_x_table">'; 
	if(wl_maclist_x_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 1; i < wl_maclist_x_row.length; i++){
			code +='<tr id="row'+i+'">';

			var wl_maclist_x_col = wl_maclist_x_row[i].split('&#62');
			for(var j = 0; j < wl_maclist_x_col.length; j++){
				code +='<td width="40%">'+ wl_maclist_x_col[j] +'</td>';
			}
			if (j != 2) code +='<td width="40%"></td>';
			code +='<td width="20%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow(this);\" value=\"\"/></td></tr>';		
		}
	}	
  	code +='</tr></table>';
	
	$("wl_maclist_x_Block").innerHTML = code;
}


function deleteRow(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('wl_maclist_x_table').deleteRow(i);
  
  var wl_maclist_x_value = "";
	for(i=0; i<$('wl_maclist_x_table').rows.length; i++){
		wl_maclist_x_value += "&#60";
		wl_maclist_x_value += $('wl_maclist_x_table').rows[i].cells[0].innerHTML;
		wl_maclist_x_value += "&#62";
                wl_maclist_x_value += $('wl_maclist_x_table').rows[i].cells[1].innerHTML;
	}
	
	wl_maclist_x_array = wl_maclist_x_value;
	if(wl_maclist_x_array == "")
		show_wl_maclist_x();
}

function addRow(obj, obj2, upper){
	var rule_num = $('wl_maclist_x_table').rows.length;
	var item_num = $('wl_maclist_x_table').rows[0].cells.length;

	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}	
	
	if(obj.value==""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();			
		return false;
	}else if(!check_macaddr(obj, check_hwaddr_flag(obj))){
		obj.focus();
		obj.select();	
		return false;	
	}
		
		//Viz check same rule
		for(i=0; i<rule_num; i++){
			for(j=0; j<item_num-2; j++){	
				if(obj.value.toLowerCase() == $('wl_maclist_x_table').rows[i].cells[j].innerHTML.toLowerCase()){
					alert("<#JS_duplicate#>");
					return false;
				}	
			}		
		}		
				
		wl_maclist_x_array += "&#60";
		wl_maclist_x_array += obj.value;
		wl_maclist_x_array += "&#62";
		wl_maclist_x_array += obj2.value;
		obj.value = "";
		obj2.value = "";
		show_wl_maclist_x();
}

function applyRule(){
	var rule_num = $('wl_maclist_x_table').rows.length;
	var item_num = $('wl_maclist_x_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){	
			tmp_value += $('wl_maclist_x_table').rows[i].cells[j].innerHTML;
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	
		
	if(prevent_lock(tmp_value)){
		document.form.wl_maclist_x.value = tmp_value;
		showLoading();
		document.form.submit();	
	}
}

function done_validating(action){
	refreshpage();
}

function prevent_lock(rule_num){
	if(document.form.wl_macmode.value == "allow" && rule_num == ""){
		alert("<#FirewallConfig_MFList_accept_hint1#>");
		return false;
	}
	else
		return true;
}

function change_wl_unit(){
	FormActions("apply.cgi", "change_wl_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function check_macaddr(obj,flag){ //control hint of input mac address

	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		$("check_mac").style.display = "";
		return false;
	}else if(flag ==2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		$("check_mac").style.display = "";
		return false;		
	}else{	
		$("check_mac") ? $("check_mac").style.display="none" : true;
		return true;
	}	
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">	
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>

		<!--===================================Beginning of Main Content===========================================-->
<input type="hidden" name="current_page" value="Advanced_ACL_Content.asp">
<input type="hidden" name="next_page" value="Advanced_ACL_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
<input type="hidden" name="wl_maclist_x" value="">		
<input type="hidden" name="wl_subunit" value="-1">

<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_1#> - <#menu5_1_4#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#DeviceSecurity11a_display1_sectiondesc#></div>
		<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
			<thead>
			  <tr>
				<td colspan="2"><#t2BC#></td>
			  </tr>
			</thead>		

			<tr id="wl_unit_field">
				<th><#Interface#></th>
				<td>
					<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
						<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
						<option class="content_input_fd" value="1" <% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
					</select>			
				</td>
		  </tr>

			<tr id="repeaterModeHint" style="display:none;">
				<td colspan="2" style="color:#FFCC00;height:30px;" align="center"><#page_not_support_mode_hint#></td>
		  </tr>

			<tr>
				<th width="30%" >
					<a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,1);"><#FirewallConfig_MFMethod_itemname#></a>
				</th>
				<td>
					<select name="wl_macmode" class="input_option"  onChange="return change_common(this, 'DeviceSecurity11a', 'wl_macmode')">
						<option class="content_input_fd" value="disabled" <% nvram_match("wl_macmode", "disable","selected"); %>><#CTL_Disabled#></option>
						<option class="content_input_fd" value="allow" <% nvram_match("wl_macmode", "allow","selected"); %>><#FirewallConfig_MFMethod_item1#></option>
						<option class="content_input_fd" value="deny" <% nvram_match("wl_macmode", "deny","selected"); %>><#FirewallConfig_MFMethod_item2#></option>
					</select>
				</td>
			</tr>
		</table>
		
			<table id="MainTable2" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
			  <thead>
			  <tr>
				<td colspan="3"><#FirewallConfig_MFList_groupitemname#></td>
			  </tr>
			  </thead>

          		<tr>
	          		<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">
						<#FirewallConfig_MFList_groupitemname#>
					</th> 
					<th width="40%">Name</th>
					<th width="20%">Add / Delete</th>
          		</tr>
          		<tr>
            		<td width="40%">
              			<input type="text" maxlength="17" class="input_macaddr_table" name="wl_maclist_x_0" onKeyPress="return is_hwaddr(this,event)">
              		</td>
			<td width="40%">
				<input type="text" class="input_15_table" maxlenght="15" onKeypress="return is_alphanum(this,event);" name="wl_macname_x_0">
			</td>
              		<td width="20%">	
              			<input type="button" class="add_btn" onClick="addRow(document.form.wl_maclist_x_0, document.form.wl_macname_x_0, 32);" value="">
              		</td>
          		</tr>      		
        		</table>
	
			<div id="wl_maclist_x_Block"></div>
		
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
