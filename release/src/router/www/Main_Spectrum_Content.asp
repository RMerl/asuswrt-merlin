<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7, IE=EmulateIE10" />
<meta name="svg.render.forceflash" content="false" />	
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Spectrum</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script src='svg.js' data-path="/svghtc/" data-debug="false"></script>	
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type='text/javascript'>

var bpc_us = new Array();
var bpc_ds = new Array();
var snr = new Array();
bpc_us = [<% show_file_content("/var/tmp/spectrum-bpc-us"); %>];
bpc_ds = [<% show_file_content("/var/tmp/spectrum-bpc-ds"); %>];
snr = [<% show_file_content("/var/tmp/spectrum-snr"); %>];
var spec_running = '<% nvram_get("spectrum_hook_is_running"); %>';

if(based_modelid == "DSL-AC68U" && "<% nvram_get("dslx_transmode"); %>" == "atm")
	var delay_time = 15;
else
	var delay_time = 45;

// disable auto log out
AUTOLOGOUT_MAX_MINUTE = 0;

function initial(){
	show_menu();
	if(wan_line_state != "up"){
			document.getElementById('btn_refresh').style.display="none";
			document.getElementById('signals_update').style.display="none";		
	}else if(bpc_us.length == 0 && bpc_ds.length == 0 && snr.length == 0 && spec_running == 0){
			document.getElementById('btn_refresh').style.display="";
	}else if(bpc_us.length == 0 && bpc_ds.length == 0 && snr.length == 0 && spec_running == 1){
			refresh_signals();
	}
	else{
			document.getElementById('btn_refresh').style.display="none";
			document.getElementById('signals_update').innerHTML = "<#Spectrum_refresh#> ";
			document.getElementById('signals_update').innerHTML += delay_time;
			document.getElementById('signals_update').style.display="";
			update_spectrum();
	}
}

function update_spectrum(){
  $.ajax({
    url: '/ajax_signals.asp',
    dataType: 'script', 
	
    error: function(xhr){
      setTimeout("update_spectrum();", 3000);
    },
    success: function(response){
    	setTimeout("document.getElementById('signals_collect_scan').style.display=\"none\";", delay_time*1000);
    	setTimeout("document.getElementById('signals_collect').style.display=\"none\";", delay_time*1000);
    	setTimeout("document.getElementById('signals_update').style.display=\"\";", delay_time*1000);
	setTimeout("update_spectrum();", 15000); //Keep refresh ajax shortly to avoid 2 times delay_time to update Spectrum image (especially for vdsl)
    }		
  });
}

function refresh_signals(){
		document.getElementById('btn_refresh').style.display="none";
		document.getElementById('signals_collect_scan').style.display="";
		document.getElementById('signals_collect').innerHTML = "<#Spectrum_gathering#> ";
		document.getElementById('signals_collect').innerHTML += delay_time;
		document.getElementById('signals_collect').style.display="";		
		update_spectrum();
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
			<input type="hidden" name="current_page" value="Main_Spectrum_Content.asp">
			<input type="hidden" name="next_page" value="Main_Spectrum_Content.asp">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">

			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td>
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" height="100px">
									  <div>&nbsp;</div>
									  <div class="formfonttitle"><#Menu_TrafficManager#> - Spectrum</div>
									  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									  <div class="formfontdesc"><#Spectrum_desc#></div>
										<span id="signals_update" style="margin-left:5px;margin-top:10px;color:#FFCC00;display:none;"></span>
									</td>
								</tr>
								
								<tr>
									<td height="15" bgcolor="#4D595D" valign="bottom"><div style="font-weight: bold;margin-left:24px;margin-bottom:-10px;">Signal-to-noise ratio in dB</div></td>
								</tr>
								<tr>
									<td bgcolor="#4D595D" valign="top">
										<table width="700px" border="0" align="center" cellpadding="0" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
											<tr>
												<td>
													<div style="margin-left:-10px;">
														<!--========= svg =========-->
														<!--[if IE]>
															<div id="svg-table_snr" align="left">
															<object id="graph_snr" src="SNR.svg" classid="image/svg+xml" width="700" height="200">
															</div>
														<![endif]-->
														<!--[if !IE]>-->
															<object id="graph_snr" data="SNR.svg" type="image/svg+xml" width="700" height="200">
														<!--<![endif]-->
															</object>
											 			<!--========= svg =========-->
													</div>
												</td>
											</tr>																							
										</table>										
									</td>
					  		</tr>
					  		<tr>
									<td height="15" bgcolor="#4D595D" align="center" valign="top"><div style="font-weight: bold;margin-top:-10px;margin-left:-100px;">Carrier</div><div style="font-weight:bold;margin-top:-21px;margin-left:60px;color:#ADD8E6;">( Frequency in kHz )</div></td>
								</tr>
					  		
								<tr>
									<td height="15" bgcolor="#4D595D" valign="bottom"><div style="font-weight: bold;margin-left:24px;margin-bottom:-10px;">Bits</div></td>
								</tr>					  		
					  		<tr>
									<td bgcolor="#4D595D" valign="top">
										<table width="700px" border="0" align="center" cellpadding="0" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">											
											<tr>
												<td>
													<div style="margin-left:-10px;">
														<!--========= svg =========-->
														<!--[if IE]>
															<div id="svg-table_bpc" align="left">
															<object id="graph_bpc" src="BPC.svg" classid="image/svg+xml" width="700" height="200">
															</div>
														<![endif]-->
														<!--[if !IE]>-->
															<object id="graph_bpc" data="BPC.svg" type="image/svg+xml" width="700" height="200">
														<!--<![endif]-->
															</object>
											 			<!--========= svg =========-->
													</div>
												</td>
											</tr>
										</table>
									</td>
					  		</tr>
					  		<tr>
									<td height="15px" bgcolor="#4D595D" align="center" valign="top"><div style="font-weight: bold;margin-top:-10px;margin-left:-100px;">Carrier</div><div style="font-weight:bold;margin-top:-21px;margin-left:60px;color:#ADD8E6;">( Frequency in kHz )</div></td>
								</tr>

								<tr>
									<td height="150px" bgcolor="#4D595D" align="center" valign="top">
										<div class="apply_gen">
											<input id="btn_refresh" class="button_gen" style="display:none;" onclick="refresh_signals();" type="button" value="<#QIS_rescan#>"/>
										</div>
										<div>
											<img id="signals_collect_scan" style="display:none;" src="images/InternetScan.gif" />
											<span id="signals_collect"></span>
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
