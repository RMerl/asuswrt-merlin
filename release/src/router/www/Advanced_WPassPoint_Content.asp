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

function initial(){
	show_menu();

	if(!band5g_support)	
		$("wl1_hs2en_tr").style.display = "none";

}

function applyRule(){
	if(validForm()){
		
		if(document.form.wl0_hs2en_x[1].checked)
				document.form.wl0_hs2en.value = "0";
		else
				document.form.wl0_hs2en.value = "1";
		
		if(band5g_support){
			if(document.form.wl1_hs2en_x[1].checked)
				document.form.wl1_hs2en.value = "0";
			else
				document.form.wl1_hs2en.value = "1";			
		}
				
		showLoading();
		document.form.submit();
	}
}

function validForm(){		
	return true;
}


</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WPassPoint_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WPassPoint_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl0_hs2en" value="<% nvram_get("wl0_hs2en"); %>">
<input type="hidden" name="wl1_hs2en" value="<% nvram_get("wl1_hs2en"); %>">

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
		  <div class="formfonttitle"><#menu5_1#> - Passpoint (Hotspot)</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc">Passpoint transforms the way users connect to Wi-Fi. This technology makes the process of finding and connecting to Wi-Fi networks seamless. Passpoint also delivers greater security protection to better safeguard your data. Your T-Mobile SIM card provides the necessary authentication to automatically connect your device to the <#Web_Title2#> Router.
				<br><br>If your T-Mobile handset is Passpoint capable, it should automatically connect to your <#Web_Title2#> Router.
		  </div>
		  
		<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">

		<tr>
			<th width="30%">Enable 2.4GHz Passpoint</th>
			<td>
					<input type="radio" name="wl0_hs2en_x" value="1" <% nvram_match("wl0_hs2en", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" name="wl0_hs2en_x" value="0" <% nvram_match("wl0_hs2en", "0", "checked"); %>><#checkbox_No#>
			</td>
		</tr>
		<tr id="wl1_hs2en_tr">
			<th width="30%">Enable 5GHz Passpoint</th>
			<td>
					<input type="radio" name="wl1_hs2en_x" value="1" <% nvram_match("wl1_hs2en", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" name="wl1_hs2en_x" value="0" <% nvram_match("wl1_hs2en", "0", "checked"); %>><#checkbox_No#>
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
