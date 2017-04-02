<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Wi-Fi Proxy</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script>
var $j = jQuery.noConflict();

function initial(){
	show_menu();

	if(!band5g_support){
		document.getElementById("wl_unit_field").style.display = "none";
	}
	else{
		if(document.form.wl_unit.value == 0){
			document.getElementById("table_proto_2").style.display = "";
		}
		else{
			document.getElementById("table_proto_5").style.display = "";
		}
	}

	if(productid == "RP-N53")
		document.getElementById("wifipxy_enable_prompt").style.display = "";
}

function enable_wifipxy_2(enable){
	document.form.wlc0_wifipxy.value = enable;
}

function enable_wifipxy_5(enable){
	document.form.wlc1_wifipxy.value = enable;
}

function applyRule(){
	showLoading();
	document.form.submit();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg">
  <table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	<tr>
	  <td>
		<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
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

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_WProxy_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WProxy_Content.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wlc0_wifipxy" value="<% nvram_get("wlc0_wifipxy"); %>">
<input type="hidden" name="wlc1_wifipxy" value="<% nvram_get("wlc1_wifipxy"); %>">

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
			  <td bgcolor="#4D595D" valign="top" height="360px">
				<div>&nbsp;</div>
				<div class="formfonttitle"><#menu5_1#> - Wi-Fi Proxy</div>
				<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
				<div class="formfontdesc">Enable this feature in Repeater mode to successfully extend certain hotspot (public Wi-Fi) network.</div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">

					<tr id="wl_unit_field" class="rept">
						<th><#Interface#></th>
						<td>
							<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
								<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
								<option class="content_input_fd" value="1"<% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
							</select>			
						</td>
					</tr>

					<tr id="table_proto_2" style="display:none">
						<th><a>Enable Wi-Fi Proxy?</a></th>
						<td>
							<input type="radio" name="wifipxy_enable_2" class="input" value="1" onclick="enable_wifipxy_2('1')" <% nvram_match("wlc0_wifipxy", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="wifipxy_enable_2" class="input" value="0" onclick="enable_wifipxy_2('0')" <% nvram_match("wlc0_wifipxy", "0", "checked"); %>><#checkbox_No#>
							<span id="wifipxy_enable_prompt" style="display:none;">(Only support 2.4GHz)</span>
						</td>
					</tr>

					<tr id="table_proto_5" style="display:none">
						<th><a>Enable Wi-Fi Proxy?</a></th>
						<td>
							<input type="radio" name="wifipxy_enable_5" class="input" value="1" onclick="enable_wifipxy_5('1')" <% nvram_match("wlc1_wifipxy", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="wifipxy_enable_5" class="input" value="0" onclick="enable_wifipxy_5('0')" <% nvram_match("wlc1_wifipxy", "0", "checked"); %>><#checkbox_No#>
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
		</tr>
	  </table>
	  <!--===================================End of Main Content===========================================-->
	</td>
	<td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>
</form>

<div id="footer"></div>
</body>
</html>
