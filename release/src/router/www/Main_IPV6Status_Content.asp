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

<style>
.pinholeheader{
        background-color:#475a5f;
        color:#FFCC00;
	font-size: 125%;
}
</style>

<script>

<% ipv6_pinholes(); %>

function initial() {
	show_menu();
	show_pinholes();
}


function show_pinholes() {
	var code, i, rule

	code = '<table width="100%" id="24G" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">';
	code += '<thead><tr>';
	code += '<td width=34%">Remote</td>';
	code += '<td width=12%">Port</td>';
	code += '<td width=34%">Local</td>';
	code += '<td width=12%">Port</td>';
	code += '<td width=8%">Proto</td>';
	code += '</tr></thead>';

	if (pinholes.length > 1) {
		for (i = 0; i < pinholes.length-1; ++i) {
			rule = pinholes[i];
			code += '<tr>';
			code += '<td>' + rule[0] + '</td>';	// Remote IP
			code += '<td>' + rule[1] + '</td>';	// Remote port
			code += '<td>' + rule[2] + '</td>';	// Local IP
			code += '<td>' + rule[3] + '</td>';	// Local Port
			code += '<td>' + rule[4] + '</td>';	// Protocol
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="7">No pinhole configured.</td></tr>';
	}

	code += '</tr></table>';
	$("pinholesblock").innerHTML = code;
}


</script>
</head>

<body onload="initial();">
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
										<br>
										<div class="pinholeheader">IPv6 pinhole rules opened in the firewall through UPnP/IGD2:</div>
										<br>
										<div id="pinholesblock"></div>
										<br>
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
