<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#Menu_usb_application#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="app_installation.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script>


function initial(){
	show_menu();
	if(adBlock_support) {
		document.getElementById("adBlock_field").style.display = "";
		document.getElementById("adb_hdr").style.display = "";
	}
	if (dnsfilter_support) {
		document.getElementById("dnsfilter").style.display = "";
		document.getElementById("dnsf_hdr").style.display = "";
	}
	if(keyGuard_support){
		document.getElementById("keyGuard_field_h").style.display = "";
		document.getElementById("keyGuard_field").style.display = "";
	}
}

</script>
</head>
<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<div>
	<table class="content" align="center" cellspacing="0" style="margin:auto;">
		<tr>
			<td width="17">&nbsp;</td>	
			<!--=====Beginning of Main Menu=====-->
			<td valign="top" width="202">
				<div id="mainMenu"></div>
				<div id="subMenu"></div>
			</td>	
			<td valign="top">
				<div id="tabMenu" style="*margin-top: -160px;"></div>
				<br>
		<!--=====Beginning of Main Content=====-->
				<div class="app_table" id="applist_table">
					<table>
						<tr>
							<td class="formfonttitle">
								<div style="width:730px">
									<table width="730px">
										<tr>
											<td align="left">
												<span class="formfonttitle"><#AiProtection_title#></span>
											</td>
										</tr>
									</table>
								</div>
							</td>
						</tr> 
						<tr>
							<td class="line_export"><img src="images/New_ui/export/line_export.png" /></td>
						</tr>
						<tr>
							<td>
								<div class="formfontdesc" style="font-size:14px;font-style:italic;"><#AiProtection_desc#></div>
							</td> 
						</tr>					
					<!-- Service table -->
						<tr>
							<td valign="top" id="app_table_td" height="0px" style="padding-top:20px;">
								<div id="app_table">
									<table class="appsTable" align="center" style="margin:auto;border-collapse:collapse;">
										<tbody>
											<tr>
												<td align="center" class="app_table_radius_left" style="width:85px">
													<img style="margin-top:0px;" src="/images/New_ui/HomeProtection.png" onclick="location.href='AiProtection_HomeProtection.asp';">
												</td>
												<td class="app_table_radius_right" style="width:350px;height:120px;">
													<div class="app_name">
														<a style="text-decoration: underline;" href="AiProtection_HomeProtection.asp"><#AiProtection_Home#></a>
													</div>
													<div class="app_desc" style="height:60px;white-space:nowrap;">
														<li><#AiProtection_scan#></li>
														<li><#AiProtection_sites_blocking#></li>
														<li><#AiProtection_Vulnerability#></li>
														<li><#AiProtection_detection_blocking#></li>														
													</div>
												</td>
											</tr>
											<tr style="height:50px;"></tr>
	
											<tr>
												<td align="center" class="app_table_radius_left" style="width:85px;">
													<img style="margin-top:0px;" src="/images/New_ui/parental-control.png" onclick="location.href='AiProtection_WebProtector.asp';">
												</td>
												<td class="app_table_radius_right" style="width:350px;height:120px;">
													<div class="app_name">
														<a style="text-decoration: underline;" href="AiProtection_WebProtector.asp"><#Parental_Control#></a>
													</div>
													<div class="app_desc" style="height:60px;">
														<li><#Time_Scheduling#></li>
														<li><#AiProtection_filter#></li>											
													</div>
												</td>
											</tr>
											<tr id="adb_hdr" style="height:50px; display:none;"></tr>

											<tr id="adBlock_field" style="display:none">
												<td align="center" class="app_table_radius_left" style="width:85px;">
													<div style="text-align:center;background: url('/images/New_ui/Web_Apps_Restriction.png');width:130px;height:85px;margin-left:30px;"></div>
												</td>
												<td class="app_table_radius_right" style="width:350px;height:120px;">
													<div class="app_name">
														<a style="text-decoration: underline;" href="AiProtection_AdBlock.asp">Ad Blocking</a>
													</div>
													<div class="app_desc" style="height:60px;">
														<li>Streaming Ad Blocking</li>
														<li>Pop-Up window Ad Blocking</li>			
													</div>
												</td>
											</tr>
											<tr id="dnsf_hdr" style="height:50px; display:none;"></tr>

											<tr id="dnsfilter" style="display:none;">
												<td align="center" class="app_table_radius_left" style="width:85px;">
													<img style="margin-top:0px;" src="/images/New_ui/DnsFiltering.png" onclick="location.href='DNSFilter.asp';">
												</td>
												<td class="app_table_radius_right" style="width:350px;height:120px;"">
													<div class="app_name">
														<a style="text-decoration: underline;" href="DNSFilter.asp">DNS Filtering</a>
													</div>
													<div class="app_desc" style="height:60px;">
														<li>Block malicious websites</li>
														<li>Block mature websites</li>
													</div>
												</td>
											</tr>
											<tr id="keyGuard_field_h" style="display:none" style="height:50px;"></tr>
											<tr id="keyGuard_field" style="display:none">
												<td align="center" class="app_table_radius_left" style="width:85px;">
													<div style="text-align:center;background: url('/images/New_ui/Key-Guard.png');width:130px;height:85px;margin-left:30px;"></div>
												</td>
												<td class="app_table_radius_right" style="width:350px;height:120px;">
													<div class="app_name">
														<a style="text-decoration: underline;" href="AiProtection_Key_Guard.asp">Key Guard</a>
													</div>
													<div class="app_desc" style="height:60px;">
														<li>Key Guard</li>
													</div>
												</td>
											</tr>												
										</tbody>
									</table>
								
								</div>
							</td> 
						</tr>  
					</table>

					<div style="width:135px;height:55px;position:absolute;bottom:5px;right:5px;background-image:url('images/New_ui/tm_logo_power.png');"></div>
				</div>
		<!--=====End of Main Content=====-->
			</td>
			<td width="20" align="center" valign="top"></td>
		</tr>
	</table>
</div>

<div id="footer"></div>
</body>
</html>

