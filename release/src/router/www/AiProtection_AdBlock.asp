<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - Ad Blocking</title>
<link rel="stylesheet" type="text/css" href="ParentalControl.css">
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style>
#switch_menu{
	text-align:right
}
#switch_menu span{
	border-radius:4px;
	font-size:16px;
	padding:3px;
}

.click:hover{
	box-shadow:0px 0px 5px 3px white;
	background-color:#97CBFF;
}
.clicked{
	background-color:#2894FF;
	box-shadow:0px 0px 5px 3px white;

}
.click{
	background:#8E8E8E;
}
</style>
<script>
var $j = jQuery.noConflict();

function initial(){
	show_menu();
	document.getElementById("_AiProtection_HomeSecurity").innerHTML = '<table><tbody><tr><td><div class="_AiProtection_HomeSecurity"></div></td><td><div style="width:120px;">AiProtection</div></td></tr></tbody></table>';
	document.getElementById("_AiProtection_HomeSecurity").className = "menu_clicked";
	register_event();
}
						
function applyRule(){
	showLoading();	
	document.form.submit();
}

function register_event(){
	document.getElementById('stream_ad_title').onmouseover = function(){overHint(89);}
	document.getElementById('stream_ad_title').onmouseout = function(){nd();}
	document.getElementById('popup_ad_title').onmouseover = function(){overHint(90);}
	document.getElementById('popup_ad_title').onmouseout = function(){nd();}
}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="AiProtection_AdBlock.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wrs;restart_firewall">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wrs_adblock_stream_ori" value="<% nvram_get("wrs_adblock_stream"); %>">
<input type="hidden" name="wrs_adblock_stream" value="<% nvram_get("wrs_adblock_stream"); %>">
<input type="hidden" name="wrs_adblock_popup" value="<% nvram_get("wrs_adblock_popup"); %>">
<input type="hidden" name="wrs_adblock_popup_ori" value="<% nvram_get("wrs_adblock_popup"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div  id="mainMenu"></div>	
			<div  id="subMenu"></div>		
		</td>						
		<td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>	
		<!--===================================Beginning of Main Content===========================================-->		
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0" >
			<tr>
				<td valign="top" >	
					<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
						<tr>
							<td bgcolor="#4D595D" valign="top">
								<div>&nbsp;</div>
								<!--div class="formfonttitle"></div-->
								<div style="margin-top:-5px;">
									<table width="730px">
										<tr>
											<td align="left">
												<div class="formfonttitle" style="width:400px">AiProtection - Ad Blocking</div>
											</td>
										</tr>
									</table>
								</div>
								<div style="margin:0px 0px 10px 5px;"><img src="/images/New_ui/export/line_export.png"></div>
								<div>
									<table width="700px" style="margin-left:25px;">
										<tr>
											<td>
												<img src="/images/New_ui/Web_Apps_Restriction.png">
											</td>
											<td>&nbsp;&nbsp;</td>
											<td style="font-style: italic;font-size: 14px;">
												<span>Ad Blocking helps you to block advertisement on the web and streaming video page. You can enjoy the content you visit, and spend less time on waiting.</span>
											</td>
										</tr>
									</table>
								</div>
								<br>
			<!--=====Beginning of Main Content=====-->
								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<tr>
										<th id="stream_ad_title" style="cursor:pointer">Streaming Ad Blocking</th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="stream_ad_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#stream_ad_enable').iphoneSwitch('<% nvram_get("wrs_adblock_stream"); %>',
														function(){
															document.form.wrs_adblock_stream.value = 1;	
															applyRule();
														},
														function(){
															document.form.wrs_adblock_stream.value = 0;
															applyRule();																													
														}
													);
												</script>			
											</div>
										</td>
									</tr>								
									<tr>
										<th id="popup_ad_title" style="cursor:pointer">Pop-Up window Ad Blocking</th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="pop_ad_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#pop_ad_enable').iphoneSwitch('<% nvram_get("wrs_adblock_popup"); %>',
														function(){
															document.form.wrs_adblock_popup.value = 1;
															applyRule();
														},
														function(){
															document.form.wrs_adblock_popup.value = 0;
															applyRule();					
														}
													);
												</script>			
											</div>
										</td>			
									</tr>									
								</table>				
							</td>
						</tr>
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

