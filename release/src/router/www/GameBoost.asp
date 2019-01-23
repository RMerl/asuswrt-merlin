<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#Game_Boost#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<style>
body{
	margin: 0;
	color:#FFF;
}
.switch{
	position: relative;
	width: 200px;
	height: 70px;
}

.switch input{
	cursor: pointer;
	height: 100%;
	opacity: 0;
	position: absolute;
	width: 100%;
	z-index: 100;
	left:0;
}
.container{
	background-color: #444;
	width:100%;
	height:100%;
}
.container::after{
	content: '';
	background-color:#999;
	width:50px;
	height:40px;
	position:absolute;
	left: 0;
	top: 0;
	border-top-left-radius:5px;
	border-bottom-left-radius:5px;
}
@-moz-document url-prefix(){ 		/*Firefox Hack*/
	.container::after{
		top:0;
	}
}
@supports (-ms-accelerator:true) {		/*Edge Browser Hack, not work on Edge 38*/
  	.container::after{
		top:0;
	}
}

.switch input:checked~.container{
	background: #D30606;
}
.switch input:checked~.container::after{
	left: 50px;
	border-top-right-radius:5px;
	border-bottom-right-radius:5px;
}
@media all and (-ms-high-contrast:none)
{
    *::-ms-backdrop, .container::after { margin-top: 0px} /* IE11 */
}
.btn{
	background-color: #990000;
	color: #EBE8E8;
}
.btn:hover{
	background-color: #D30606;
	color: #FFF;
}
</style>

<script>
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';
var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
function initial(){
	show_menu();
	if((document.form.qos_enable.value == '1') && (document.form.qos_type.value == '1') && (bwdpi_app_rulelist.indexOf('game') != -1)){
		document.getElementById("game_boost_enable").checked = true;
	}
	else{
		document.getElementById("game_boost_enable").checked = false;
	}
	
}
function check_game_boost(){
	if(document.getElementById("game_boost_enable").checked){
		document.form.qos_enable.value = '1';
		document.form.qos_type.value = '1';
		document.form.bwdpi_app_rulelist.disabled = false;
		document.form.bwdpi_app_rulelist.value = "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<game";
	}
	else{
		document.form.qos_enable.value = '0';
		document.form.bwdpi_app_rulelist.disabled = true;
	}
	
	if(ctf_disable == 1){
		document.form.action_script.value = "restart_qos;restart_firewall";
	}
	else{
		if(ctf_fa_mode == "2"){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
		else{
			if(document.form.qos_type.value == 0)
				FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
			else{				
				document.form.action_script.value = "restart_qos;restart_firewall";
			}					
		}
	}
	
	document.form.submit();
}
</script>
</head>
<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="GameBoost.asp">
<input type="hidden" name="next_page" value="GameBoost.asp">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos;restart_firewall">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_type" value="<% nvram_get("qos_type"); %>">
<input type="hidden" name="qos_type_ori" value="<% nvram_get("qos_type"); %>">
<input type="hidden" name="bwdpi_app_rulelist" value="<% nvram_get("bwdpi_app_rulelist"); %>">
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
				<div id="applist_table" style="background:url('images/New_ui/mainimage_img_Game.jpg');background-repeat: no-repeat;border-radius:3px;">
					<table style="padding-left:10px;">
						<tr>
							<td class="formfonttitle">
								<div style="width:730px;padding-top:10px;">
									<table width="730px">
										<tr>
											<td align="left">
												
												<div style="display:table-cell;background:url('/images/New_ui/game.svg');width:77px;height:77px;"></div>
												<div class="formfonttitle" style="display:table-cell;font-size:26px;font-weight:bold;color:#EBE8E8;vertical-align:middle"><#Game_Boost#></div>
											</td>
										</tr>
									</table>
								</div>
							</td>
						</tr> 
					<!-- Service table -->
						<tr>
							<td valign="top" height="0px">
								<div>
									<table style="border-collapse:collapse;width:100%">
										<tbody>
											<tr>
												<td style="width:200px">
													<div style="padding: 5px 0;font-size:20px;"><#Game_Boost_internet#></div>													
												</td>
												<td colspan="2">
													<div style="padding: 5px 10px;font-size:20px;color:#FFCC66">WTFast GPN</div>
												</td>
											</tr>
											<tr>
												<td colspan="3">									
													<div style="width:100%;height:1px;background-color:#D30606"></div>
												</td>
											</tr>		
											<tr>
												<td align="center" style="width:85px">
													<img style="padding-right:10px;;" src="/images/New_ui/GameBoost_WTFast.png" >
												</td>
												<td style="width:400px;height:120px;">
													<div style="font-size:16px;color:#949393;padding-left:10px;">
														<#Game_Boost_desc#>
													</div>
												</td>
												<td>
													<div class="btn" style="margin:auto;width:100px;height:40px;text-align:center;line-height:40px;font-size:18px;cursor:pointer;border-radius:5px;" onclick="location.href='Advanced_WTFast_Content.asp';"><#btn_go#></div>
												</td>
											</tr>
											<tr style="height:50px;"></tr>
											<tr>
												<td>
													<div style="padding: 5px 0;font-size:20px;"><#Game_Boost_lan#></div>
												</td>
												<td colspan="2">
													<div style="padding: 5px 10px;font-size:20px;color:#FFCC66;"><#Game_Boost_lan_title#></div>
												</td>
											</tr>
											<tr>
												<td colspan="3">									
													<div style="width:100%;height:1px;background-color:#D30606"></div>
												</td>
											</tr>	
											<tr>
												<td align="center" style="width:85px;">
													<div style="width:97px;height:71px;background:url('images/New_ui/GameBoost_QoS.png');no-repeat"></div>
												</td>
												<td style="width:400px;height:120px;">
													<div style="height:60px;font-size:16px;color:#A0A0A0;padding-left:10px;">
														<#Game_Boost_lan_desc#>
													</div>
												</td>
												<td>
													<div class="switch" style="margin:auto;width:100px;height:40px;text-align:center;line-height:40px;font-size:18px">
														<input id="game_boost_enable" type="checkbox" onclick="check_game_boost();">
														<div class="container" style="display:table;border-radius:5px;">
															<div style="display:table-cell;width:50%;">
																<!--div style="background:url('check.svg') no-repeat;width:40px;height:40px;margin: auto"></div-->
																<div>ON</div>
															</div>
															<div style="display:table-cell">
																<!--div style="background:url('x.svg') no-repeat;width:40px;height:40px;margin: auto"></div-->
																<div>OFF</div>
															</div>
														</div>
													</div>
												</td>
											</tr>
									
											<tr style="height:50px;"></tr>
											<tr>
												<td>
													<div style="padding: 5px 0;font-size:20px;"><#Game_Boost_AiProtection#></div>
												</td>
												<td colspan="2">
													<div style="padding: 5px 10px;font-size:20px;color:#FFCC66;"><#AiProtection_title#></div>
												</td>
											</tr>
											<tr>
												<td colspan="3">									
													<div style="width:100%;height:1px;background-color:#D30606"></div>
												</td>
											</tr>
											<tr>
												<td align="center" style="width:85px;">
													<div style="background:url('/images/New_ui/GameBoost_AiProtection.png')no-repeat;width:97px;height:71px;"></div>
												</td>
												<td style="width:400px;height:120px;vertical-align:initial">
													<div style="height:60px;margin-top:15px;font-size:16px;color:#949393;padding-left:10px;">
														<#Game_Boost_AiProtection_desc#>
													</div>
												</td>
												<td>
													<div class="btn" style="margin:auto;width:100px;height:40px;text-align:center;line-height:40px;font-size:18px;cursor:pointer;border-radius:5px;" onclick="location.href='AiProtection_HomeProtection.asp';"><#btn_go#></div>
												</td>
											</tr>	
										</tbody>
									</table>	
								</div>
							</td> 
						</tr>  
					</table>
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

