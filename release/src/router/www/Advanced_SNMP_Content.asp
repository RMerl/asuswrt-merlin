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
<title><#Web_Title#> - SNMP</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script>snmpd_enable = '<% nvram_get("snmpd_enable"); %>';
//v3_auth_type = '<% nvram_get("v3_auth_type"); %>';
//v3_priv_type = '<% nvram_get("v3_priv_type"); %>';

function applyRule(){
	if(validForm()){
		showLoading();
		document.form.submit();
	}
}

function validForm(){
	if(document.form.roCommunity.value == document.form.rwCommunity.value) {
		document.form.roCommunity.focus();			
		alert("Get Community and Set Community must be different.");
		return false;
	}
	if(document.form.v3_auth_type.value != "NONE" && document.form.v3_auth_passwd.value.length < 8) {
		document.form.v3_auth_passwd.focus();
		alert("Password cannot be less than 8 characters.");
		return false;
	}

	if(document.form.v3_priv_type.value != "NONE" && document.form.v3_priv_passwd.value.length < 8) {
		document.form.v3_priv_passwd.focus();
		alert("Password cannot be less than 8 characters.");
		return false;
	}

	return true;
}

function done_validating(action){
	refreshpage();
}

function initial(){
	show_menu(); 
//	load_body();
	change_snmpd_enable(snmpd_enable);
}

function change_snmpd_enable(enabled){
	if(enabled == 1){
		inputCtrl(document.form.sysName, 1);
		inputCtrl(document.form.sysLocation, 1);
		inputCtrl(document.form.sysContact, 1);
		inputCtrl(document.form.roCommunity, 1);
		inputCtrl(document.form.rwCommunity, 1);
		inputCtrl(document.form.v3_auth_type, 1);
		change_v3_auth_type(document.form.v3_auth_type.value);
	}
	else
	{
		inputCtrl(document.form.sysName, 0);
		inputCtrl(document.form.sysLocation, 0);
		inputCtrl(document.form.sysContact, 0);
		inputCtrl(document.form.roCommunity, 0);
		inputCtrl(document.form.rwCommunity, 0);
		inputCtrl(document.form.v3_auth_type, 0);
		inputCtrl(document.form.v3_auth_passwd, 0);
		inputCtrl(document.form.v3_priv_type, 0);
		inputCtrl(document.form.v3_priv_passwd, 0);
	}
}

function change_v3_auth_type(type){
	if(type == "NONE" || type == ""){
		inputCtrl(document.form.v3_auth_passwd, 0);
		inputCtrl(document.form.v3_priv_type, 0);
		inputCtrl(document.form.v3_priv_passwd, 0);
	}
	else
	{
		inputCtrl(document.form.v3_auth_passwd, 1);
		change_v3_priv_type(document.form.v3_priv_type.value);
	}
}

function change_v3_priv_type(type){
	inputCtrl(document.form.v3_priv_type, 1);
	if(type == "NONE" || type == "")
		inputCtrl(document.form.v3_priv_passwd, 0);
	else
		inputCtrl(document.form.v3_priv_passwd, 1);
}

</script>
</head>
<body onload="initial();" onunLoad="return unload_body();">

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">

<input type="hidden" name="current_page" value="Advanced_SNMP_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_snmpd">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_6#> - SNMP</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"></div>

	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
         <tr>
            <th>Enable SNMP</th>
            <td>
		<input type="radio" name="snmpd_enable" class="input" value="1" onclick="change_snmpd_enable(1);" <% nvram_match_x("", "snmpd_enable", "1", "checked"); %>><#checkbox_Yes#>
		<input type="radio" name="snmpd_enable" class="input" value="0" onclick="change_snmpd_enable(0);" <% nvram_match_x("", "snmpd_enable", "0", "checked"); %>><#checkbox_No#>
            </td>
         </tr>

	<tr>
	   <th>Allow access from WAN</th>
	   <td>
		<input type="radio" name="snmpd_wan" class="input" value="1" <% nvram_match_x("", "snmpd_wan", "1", "checked"); %>><#checkbox_Yes#>
		<input type="radio" name="snmpd_wan" class="input" value="0" <% nvram_match_x("", "snmpd_wan", "0", "checked"); %>><#checkbox_No#>
	   </td>
	</tr>
        <tr>
          <th>System Name</th>
          <td><input type="text" maxlength="255" class="input_32_table" name="sysName" value="<% nvram_get("sysName"); %>" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>System Location</th>
          <td><input type="text" maxlength="255" class="input_32_table" name="sysLocation" value="<% nvram_get("sysLocation"); %>" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>System Contact</th>
          <td><input type="text" maxlength="255" class="input_32_table" name="sysContact" value="<% nvram_get("sysContact"); %>" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>SNMP Get Community</th>
          <td><input type="text" maxlength="32" class="input_32_table" name="roCommunity" value="<% nvram_get("roCommunity"); %>" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>SNMP Set Community</th>
          <td><input type="text" maxlength="32" class="input_32_table" name="rwCommunity" value="<% nvram_get("rwCommunity"); %>" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>SNMPv3 Authentication Type</th>
		<td>
		  <select name="v3_auth_type" class="input_option" onchange="change_v3_auth_type(this.value);">
		  	<option value="NONE" <% nvram_match("v3_auth_type", "NONE", "selected"); %>>NONE</option>
		  	<option value="MD5" <% nvram_match("v3_auth_type", "MD5", "selected"); %>>MD5</option>
		  	<option value="SHA" <% nvram_match("v3_auth_type", "SHA", "selected"); %>>SHA</option>
		  </select>
		</td>
        </tr>

        <tr>
          <th>SNMPv3 Authentication Password</th>
          <td><input type="text" maxlength="64" class="input_32_table" name="v3_auth_passwd" value="<% nvram_get("v3_auth_passwd"); %>" autocomplete="off" autocorrect="off" autocapitalize="off"></td>
        </tr>

        <tr>
          <th>SNMPv3 Privacy Type</th>
		<td>
		  <select name="v3_priv_type" class="input_option" onchange="change_v3_priv_type(this.value);">
		  	<option value="NONE" <% nvram_match("v3_priv_type", "NONE", "selected"); %>>NONE</option>
		  	<option value="DES" <% nvram_match("v3_priv_type", "DES", "selected"); %>>DES</option>
		  	<option value="AES" <% nvram_match("v3_priv_type", "AES", "selected"); %>>AES</option>
		  </select>
		</td>
        </tr>

        <tr>
          <th>SNMPv3 Privacy Password</th>
          <td><input type="text" maxlength="64" class="input_32_table" name="v3_priv_passwd" value="<% nvram_get("v3_priv_passwd"); %>" autocomplete="off" autocorrect="off" autocapitalize="off"></td>
        </tr>

        </table>
            <div class="apply_gen">
            	<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
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
