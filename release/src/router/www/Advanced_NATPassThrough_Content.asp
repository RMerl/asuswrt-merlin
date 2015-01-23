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
<title><#Web_Title#> - NAT Pass-Through</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script>function initial(){
	show_menu();
}

function applyRule(){
	showLoading();
	document.form.submit();	
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
			<div id="mainMenu"></div>	
			<div id="subMenu"></div>		
		</td>						
    <td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
			
			<!--===================================Beginning of Main Content===========================================-->
			<input type="hidden" name="current_page" value="Advanced_NATPassThrough_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_NATPassThrough_Content.asp">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="restart_firewall;restart_pppoe_relay">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">

			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" valign="top"  >
								  <div>&nbsp;</div>
								  <div class="formfonttitle"><#menu5_3#> - <#NAT_passthrough_itemname#></div>
								  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								  <div class="formfontdesc"><#NAT_passthrough_desc#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										
										<tr>
											<th><#NAT_PPTP_Passthrough#></th>
											<td>
												<select name="fw_pt_pptp" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_pptp", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_pptp", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>			
											</td>
									  </tr>
							
										<tr>
											<th><#NAT_L2TP_Passthrough#></th>
											<td>
												<select name="fw_pt_l2tp" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_l2tp", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_l2tp", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>			
											</td>
									  </tr>
							
										<tr>
											<th><#NAT_IPSec_Passthrough#></th>
											<td>
												<select name="fw_pt_ipsec" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_ipsec", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_ipsec", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>			
											</td>
									  </tr>

						<tr>
  	         					<th><#NAT_RTSP_Passthrough#></th>
    	       					<td>
												<select name="fw_pt_rtsp" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_rtsp", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_rtsp", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>			
        	    				</td>
           					</tr>

						<tr>
							<th><#NAT_H323_Passthrough#></th>
						<td>
												<select name="fw_pt_h323" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_h323", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_h323", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>
						</td>
						</tr>

						<tr>
							<th><#NAT_SIP_Passthrough#></th>
						<td>
												<select name="fw_pt_sip" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_sip", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_sip", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>
						</td>
						</tr>

										<tr>
							<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,11);"><#PPPConnection_x_PPPoERelay_itemname#></a></th>
    	       					<td>
												<select name="fw_pt_pppoerelay" class="input_option">
													<option class="content_input_fd" value="0" <% nvram_match("fw_pt_pppoerelay", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("fw_pt_pppoerelay", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
												</select>
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
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
