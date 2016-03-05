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
<title><#Web_Title#> - <#Switch_itemname#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>

<script>
var lacp_support = isSupport("lacp");

function initial(){
	var ctf_disable = '<% nvram_get("ctf_disable"); %>';
	var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';

	show_menu();

	if(ctf_disable == 1){
		document.getElementById("ctfLevelDesc").innerHTML = "<#NAT_Acceleration_ctf_disable#>";
	}
	else{
		if(ctf_fa_mode == '2')
			document.getElementById("ctfLevelDesc").innerHTML = "<#NAT_Acceleration_ctf_fa_mode2#>";
		else
			document.getElementById("ctfLevelDesc").innerHTML = "<#NAT_Acceleration_ctf_fa_mode1#>";
	}

	if(lacp_support){
		document.getElementById("lacp_tr").style.display = "";
	}
	else{
		document.form.lacp_enabled.disabled = true;
	}

	if(based_modelid == "RT-AC5300R"){
		var new_str;
		new_str = document.getElementById("lacp_note").innerHTML.replace("LAN1", "LAN4");
		document.getElementById("lacp_note").innerHTML = new_str.replace("LAN2", "LAN8");
	}

}

function applyRule(){
	showLoading();
	document.form.submit();	
}

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
<input type="hidden" name="current_page" value="Advanced_SwitchCtrl_Content.asp">
<input type="hidden" name="next_page" value="Advanced_SwitchCtrl_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="action_wait" value="<% nvram_get("reboot_time"); %>">
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
				  					<td bgcolor="#4D595D" valign="top">
				  						<div>&nbsp;</div>
				  						<div class="formfonttitle"><#menu5_2#> - <#Switch_itemname#></div>
		      							<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
										<div class="formfontdesc"><#SwitchCtrl_desc#></div>

										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
											<tr>
												<th><#jumbo_frame#></th>
												<td>
													<select name="jumbo_frame_enable" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("jumbo_frame_enable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option class="content_input_fd" value="1" <% nvram_match("jumbo_frame_enable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
													</select>
												</td>
											</tr>

											<tr>
		      									<th><#NAT_Acceleration#></th>
												<td>
													<select name="ctf_disable_force" class="input_option">
														<option class="content_input_fd" value="1" <% nvram_match("ctf_disable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option class="content_input_fd" value="0" <% nvram_match("ctf_disable", "0","selected"); %>><#Auto#></option>
													</select>
													&nbsp
													<span id="ctfLevelDesc"></span>
												</td>
											</tr>     

											<tr style="display:none">
												<th>Enable GRO(Generic Receive Offload)</th>
												<td>
													<input type="radio" name="gro_disable_force" value="0" <% nvram_match("gro_disable_force", "0", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="gro_disable_force" value="1" <% nvram_match("gro_disable_force", "1", "checked"); %>><#checkbox_No#>
												</td>
											</tr>       

											<tr>
											<th>Spanning-Tree Protocol</th>
												<td>
													<select name="lan_stp" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("lan_stp", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option class="content_input_fd" value="1" <% nvram_match("lan_stp", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
													</select>
												</td>
											</tr>

											<tr id="lacp_tr" style="display:none;">
		      									<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(29,1);">Bonding/ Link aggregation</a></th><!--untranslated-->
												<td>
													<select name="lacp_enabled" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("lacp_enabled", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option class="content_input_fd" value="1" <% nvram_match("lacp_enabled", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
													</select>
													&nbsp
													<div id="lacp_desc"><span id="lacp_note">Please enable Bonding (802.3ad) support of your wired client and connect it to Router LAN1 and LAN2.</span><div><!--untranslated-->
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
		</td>
	    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
