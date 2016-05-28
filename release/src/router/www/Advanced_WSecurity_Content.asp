<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_1_5#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script>
<% wl_get_parameter(); %>


function initial(){
	show_menu();

	// special case after modifing GuestNetwork
	if("<% nvram_get("wl_unit"); %>" == "-1" && "<% nvram_get("wl_subunit"); %>" == "-1"){
		var guestWLUnit = '<% get_parameter("gwlu"); %>';
		if(guestWLUnit === "1") {
			document.form.wl_unit.value = "1";
		}
		change_wl_unit();
	}

	regen_band(document.form.wl_unit);

	if(!band5g_support || based_modelid == "RT-AC87U")
		document.getElementById("wl_unit_field").style.display = "none";

	if(smart_connect_support && '<% nvram_get("smart_connect_x"); %>' == '1')
		document.getElementById("wl_unit_field").style.display = "none";

	if((sw_mode == 2 || sw_mode == 4) && '<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>'){
		for(var i=4; i>=2; i--)
			document.getElementById("MainTable1").deleteRow(i);
		document.getElementById("repeaterModeHint").style.display = "";
		document.getElementById("submitBtn").style.display = "none";
	}
}

function applyRule(){
	if(validForm()){
		showLoading()
		document.form.submit();
	}
}

function validForm(){
	if(sw_mode == 1 && IPv6_support){
		var notIPv6 = false;
		var notIPv4 = false;

		if(!ipv6_valid(document.form.wl_radius_ipaddr))
			notIPv6 = true;

		if(!validator.ipAddrFinal(document.form.wl_radius_ipaddr, 'wl_radius_ipaddr', 1))
			notIPv4 = true;

		if(notIPv6 && notIPv4){
			alert(document.form.wl_radius_ipaddr.value+" <#JS_validip#>");
			document.form.wl_radius_ipaddr.value = "";
			document.form.wl_radius_ipaddr.focus();
			document.form.wl_radius_ipaddr.select();
			return false;
		}
	}
	else{
		if(!validator.ipAddrFinal(document.form.wl_radius_ipaddr, 'wl_radius_ipaddr'))
			return false;
	}
	
	if(!validator.range(document.form.wl_radius_port, 0, 65535))
		return false;
	
	if(!validator.string(document.form.wl_radius_key))
		return false;
	
	return true;
}

function done_validating(action){
	refreshpage();
}

function ipv6_valid(obj){
	//var rangere=new RegExp("^[a-f0-9]{1,4}:([a-f0-9]{0,4}:){2,6}[a-f0-9]{1,4}$", "gi");
	var rangere=new RegExp("^((([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){6}:[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){5}:([0-9A-Fa-f]{1,4}:)?[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){4}:([0-9A-Fa-f]{1,4}:){0,2}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){3}:([0-9A-Fa-f]{1,4}:){0,3}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){2}:([0-9A-Fa-f]{1,4}:){0,4}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){6}((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|(([0-9A-Fa-f]{1,4}:){0,5}:((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|(::([0-9A-Fa-f]{1,4}:){0,5}((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|([0-9A-Fa-f]{1,4}::([0-9A-Fa-f]{1,4}:){0,5}[0-9A-Fa-f]{1,4})|(::([0-9A-Fa-f]{1,4}:){0,6}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){1,7}:))$", "gi");
	if(rangere.test(obj.value)){
			return true;
	}else{
			obj.focus();
			obj.select();
			return false;
	}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WSecurity_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WSecurity_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="-1">

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
		  <div class="formfonttitle"><#menu5_1#> - <#menu5_1_5#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#WLANAuthentication11a_display1_sectiondesc#></div>
		  
		<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">

		<tr id="wl_unit_field">
			<th><#Interface#></th>
			<td>
				<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
					<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
					<option class="content_input_fd" value="1"<% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
				</select>
			</td>
	  </tr>

		<tr id="repeaterModeHint" style="display:none;">
			<td colspan="2" style="color:#FFCC00;height:30px;" align="center"><#page_not_support_mode_hint#></td>
		</tr>

		<tr>
			<th>
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,1);">
			  	<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#></a>
			</th>
			<td>
				<input type="text" maxlength="39" class="input_32_table" name="wl_radius_ipaddr" value="<% nvram_get("wl_radius_ipaddr"); %>" onKeyPress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr>
			<th>
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,2);">
			  	<#WLANAuthentication11a_ExAuthDBPortNumber_itemname#></a>
			</th>
			<td>
				<input type="text" maxlength="5" class="input_6_table" name="wl_radius_port" value="<% nvram_get("wl_radius_port"); %>" onkeypress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off"/>
			</td>
		</tr>
		<tr>
			<th >
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(2,3);">
				<#WLANAuthentication11a_ExAuthDBPassword_itemname#></a>
			</th>
			<td>
				<input type="password" maxlength="64" class="input_32_table" name="wl_radius_key" value="<% nvram_get("wl_radius_key"); %>" autocorrect="off" autocapitalize="off">
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
