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
<title><#Web_Title#> - <#menu5_3_5#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

function applyRule(){
	if(validForm()){
		showLoading();
		document.form.submit();
	}
}

function validForm(){
	if(!validate_ipaddr_final(document.form.dmz_ip, 'dmz_ip')){
		return false;
	}
	
	return true;
}

function done_validating(action){
	refreshpage();
}

function initial(){
	show_menu(); 
	load_body();
	addOnlineHelp($("faq"), ["ASUSWRT", "DMZ"]);
}
</script>
</head>
<body onload="initial();" onunLoad="return unload_body();">

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">

<input type="hidden" name="current_page" value="Advanced_Exposed_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
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
		  <div class="formfonttitle"><#menu5_3#> - <#menu5_3_5#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#IPConnection_ExposedIP_sectiondesc#><br/><#IPConnection_BattleNet_sectionname#>: <#IPConnection_BattleNet_sectiondesc#></div>
			<div class="formfontdesc" style="margin-top:-10px;">
				<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;">DMZ FAQ</a>
			</div>


	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          <tr>
            <th><#IPConnection_ExposedIP_itemname#></th>
            <td>
              <input type="text" maxlength="15" class="input_15_table" name="dmz_ip" value="<% nvram_get("dmz_ip"); %>" onkeypress="return is_ipaddr(this, event)"/>
            </td>
          </tr>

        <!-- Viz 2011.04tr>
        <th width="30%"  align="right"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,20);"><#IPConnection_BattleNet_itemname#></a></th>
          	<td>
          		<input type="radio" value="1" name="sp_battle_ips" class="content_input_fd" onClick="return change_common_radio(this, 'IPConnection', 'sp_battle_ips', '1')" <% nvram_match("sp_battle_ips", "1", "checked"); %>><#checkbox_Yes#>
          		<input type="radio" value="0" name="sp_battle_ips" class="content_input_fd" onClick="return change_common_radio(this, 'IPConnection', 'sp_battle_ips', '0')" <% nvram_match("sp_battle_ips", "0", "checked"); %>><#checkbox_No#>
          	</td>
          </tr-->
       		
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
