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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();

<% login_state_hook(); %>
<% wanlink(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var cloud_sync = '<% nvram_get("cloud_sync"); %>';
var cloud_status;

function initial(){
	show_menu();
	addOnlineHelp($("faq0"), ["samba"]);
	addOnlineHelp($("faq1"), ["ASUSWRT", "port", "forwarding"]);
	addOnlineHelp($("faq2"), ["ASUSWRT", "DMZ"]);

	switch(valid_is_wan_ip(wanlink_ipaddr())){
		/* private */
		case 0: 
			$("accessMethod").innerHTML = '<#aicloud_disk_case0#>';
			break;
		/* public */
		case 1:
			$("privateIpOnly").style.display = "none";
			if('<% nvram_get("ddns_enable_x"); %>' == '1' && '<% nvram_get("ddns_hostname_x"); %>' != ''){
				$("accessMethod").innerHTML = '<#aicloud_disk_case11#> <a style="font-weight: bolder;text-decoration: underline;" href="http://<% nvram_get("ddns_hostname_x"); %>:8082" target="_blank">http://<% nvram_get("ddns_hostname_x"); %>:8082</a><br />';
				$("accessMethod").innerHTML += '<#aicloud_disk_case12#>';
			}
			else{
				$("accessMethod").innerHTML = '<#aicloud_disk_case21#><br />';
				$("accessMethod").innerHTML += '<#aicloud_disk_case22#>';
			}
			break;
	}
}

function valid_is_wan_ip(ip_obj){
  // test if WAN IP is a private IP.
  var A_class_start = inet_network("10.0.0.0");
  var A_class_end = inet_network("10.255.255.255");
  var B_class_start = inet_network("172.16.0.0");
  var B_class_end = inet_network("172.31.255.255");
  var C_class_start = inet_network("192.168.0.0");
  var C_class_end = inet_network("192.168.255.255");
  
  var ip_num = inet_network(ip_obj);
  if(ip_num > A_class_start && ip_num < A_class_end)
		return 0;
  else if(ip_num > B_class_start && ip_num < B_class_end)
		return 0;
  else if(ip_num > C_class_start && ip_num < C_class_end)
		return 0;
	else if(ip_num == 0)
		return 0;
  else
		return 1;
}

function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	return -2;
}
</script>
</head>
<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_main.asp">
<input type="hidden" name="next_page" value="cloud_main.asp">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
<input type="hidden" name="enable_webdav" value="<% nvram_get("enable_webdav"); %>">
<input type="hidden" name="webdav_aidisk" value="<% nvram_get("webdav_aidisk"); %>">
<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<div class="tabclick"><span>AiCloud</span></div>
						</td>
						<td>
							<a href="cloud_sync.asp"><div class="tab"><span>Smart Sync</span></div></a>
						</td>
					</tr>
					</tbody>
				</table>
			</div>
<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
							  <td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">AiCloud</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<table width="100%" height="550px" style="border-collapse:collapse;">

									  <tr bgcolor="#444f53">
									    <td colspan="5" bgcolor="#444f53" class="cloud_main_radius">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:break-all;">
													<#step_use_aicloud#>
												  <ol style="-webkit-margin-after: 0em;word-break:normal;">
										        <li style="margin-bottom:7px;">
															<#download_aicloud#> 
															<a href="http://event.asus.com/2012/nw/aicloud/edm/" target="_blank"><img border="0" src="/images/cloudsync/googleplay.png" width="100px"></a>
															<a href="http://event.asus.com/2012/nw/aicloud/edm/" target="_blank"><img src="/images/cloudsync/AppStore.png" border="0"  width="100px"></a>
														</li>
										        <li style="margin-bottom:7px;">															
															<#connect_through_wifi#>
														</li>
										        <li style="margin-bottom:7px;">
															<#access_service#>
														</li>
												  </ol>

												  <ul style="-webkit-margin-after: 0em;word-break:normal;" id="privateIpOnly">
										        <li style="margin-top:-5px;">
															<span>
																<#aicloud_for_privateIP1#>&nbsp;
																<#aicloud_for_privateIP2#>&nbsp;
	       												<#aicloud_for_privateIP3#>
															</span>
														</li>
												  </ul>
												  <ul style="-webkit-margin-after: 0em;word-break:normal;">
										        <li style="margin-top:-5px;">
															<span>
																<#aicloud_bandwidth1#>&nbsp;
																<#aicloud_bandwidth2#>&nbsp;
	       												<#aicloud_bandwidth3#>
															</span>
														</li>
												  </ul>												  

												</div>
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53" width="235px">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center"><img src="/images/cloudsync/001.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div>
												</div>
											</td>

									    <td width="6px">
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>

									    <td width="1px"></td>

									    <td>
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:break-all;">
													<#aicloud_disk1#>
													<#aicloud_disk2#><br />
													<div id="accessMethod"></div>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/AiDesk.png"-->
												</div>
											</td>

									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100px">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_clouddisk_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_clouddisk_enable').iphoneSwitch('<% nvram_get("webdav_aidisk"); %>', 
														 function(){
															document.form.webdav_aidisk.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();
															document.form.submit();
														 },
														 function() {
															document.form.webdav_aidisk.value = 0;
															if(document.form.webdav_proxy.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>
												</div>	
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>
										
									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center"><img src="/images/cloudsync/002.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Access</div>
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:break-all;">
													<#smart_access1#><br />
													<#smart_access2#><br />
													<#smart_access3#>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/SmartSync.png"-->
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartAccess_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartAccess_enable').iphoneSwitch('<% nvram_get("webdav_proxy"); %>', 
														 function() {
															document.form.webdav_proxy.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 function() {
															document.form.webdav_proxy.value = 0;
															if(document.form.webdav_aidisk.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>			
												</div>	
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Access</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center">
													<img src="/images/cloudsync/003.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Sync</div>
													<!--img width="50px" src="/images/cloudsync/Status_fin.png" style="margin-top:-50px;margin-left:80px;"-->
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;word-break:break-all;">
													<#smart_sync1#><br />
													<#smart_sync2#>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/SmartAccess.png"-->
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<!--div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartSync_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartSync_enable').iphoneSwitch('<% nvram_get("enable_cloudsync"); %>',
														function() {
															document.form.enable_cloudsync.value = 1;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();
														},
														function() {
															document.form.enable_cloudsync.value = 0;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();	
														},
														{
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														}
													);
												</script>			
												</div-->

						  					<input name="button" type="button" class="button_gen_short" onclick="location.href='/cloud_sync.asp'" value="<#btn_go#>"/>
											</td>
									  </tr>

									</table>

							  </td>
							</tr>				
							</tbody>	
				  	</table>			
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
