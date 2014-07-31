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
							<div class="tabclick"><span>AiCloud 2.0</span></div>
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
									<div class="formfonttitle">AiCloud 2.0</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

									<table width="100%" height="auto" style="border-collapse:collapse;">
									  <tr bgcolor="#444f53">
									    <td colspan="5" valign="top" bgcolor="#444f53" class="cloud_main_radius">
												<div style="padding:30px;width:95%;font-style:italic;font-size:20px;">
													<table>
														<tr style="cursor:pointer;" onclick="window.open('https://www.facebook.com/asus.n.america');">
															<td>
																<img src="/images/cloudsync/facebookIcon.png">
															</td>
															<td>
																<div style="margin-left:20px;">AiCloud 2.0 coming soon! Follow us on Facebook for the latest news and update information.</div>
															</td>
														</tr>
													</table>
												</div>
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
