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
<title><#Web_Title#> - <#ipv6_info#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
</head>

<body onload="show_menu();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_IPV6Status_Content.asp">
<input type="hidden" name="next_page" value="Main_IPV6Status_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">

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
						<td valign="top">
							<table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
								<tbody>
								<tr bgcolor="#4D595D">
									<td valign="top">
										<div>&nbsp;</div>
										<div class="formfonttitle"><#System_Log#> - <#ipv6_info#></div>
										<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
										<div class="formfontdesc"><#ipv6_info_desc#></div>
										<div style="margin-top:8px">   
											<textarea cols="63" rows="25" readonly="readonly" wrap="off" style="width:99%;font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("ipv6_network.log", "ipv6_network.sh"); %></textarea>
										</div>
										<div class="apply_gen">
											<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">
										</div>
									</td><!--==magic 2008.11 del name ,if there are name, when the form was sent, the textarea also will be sent==-->
								</tr>
								</tbody>		  
							</table>	
						</td>
					</tr>
				</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
<div id="footer"></div>
</form>
</body>
</html>
