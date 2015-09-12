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
<title><#Web_Title#> - <#menu5_7_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>
<style>
p{
	font-weight: bolder;
}
</style>


<script>
<% get_leases_array(); %>

overlib_str_tmp = "";
overlib.isOut = true;

function initial() {
	show_menu();
	show_leases();
}

function show_leases() {
	var code, i, line;
	var Days, Hours, Minutes, Seconds;

	code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table">';
	code += '<thead><tr><td colspan="4">DHCP Leases</td></tr></thead>';
	code += '<tr><th width="20%">Time Left</th>';
	code += '<th width="20%">MAC</th>';
	code += '<th width="20%">IP Address</th>';
	code += '<th width="40%">Hostname</th>';
	code += '</tr>';

	if ("<% nvram_get("dhcp_enable_x"); %>" == "0") {
		code += '<tr><td colspan="4">DHCP server is disabled.</td></tr>';
	} else if (leasearray.length > 1) {
		for (i = 0; i < leasearray.length-1; ++i) {
			line = leasearray[i];
			code += '<tr>';

			Days = Math.floor(line[0] / (60*60*24));        
			Hours = Math.floor((line[0] / 3600) % 24);
			Minutes = Math.floor(line[0] % 3600 / 60);
			Seconds = Math.floor(line[0] % 60);
			code += '<td>' + Days + "d " + Hours + "h " + Minutes + "m "+ Seconds + "s" + '</td>';

			overlib_str = "<p><#MAC_Address#>:</p>" + line[1];
			code += '<td><span class="ClientName" onclick="oui_query(\'' + line[1] +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ line[1].toUpperCase() +'</span></td>'; 
			code += '<td>' + line[2] + '</td>';
			code += '<td>' + line[3] + '</td>';
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="4"><span>No active leases.</span></td></tr>';
	}

	code += '</tr></table>';
	document.getElementById("leaseblock").innerHTML = code;
}


</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_DHCPStatus_Content.asp">
<input type="hidden" name="next_page" value="Main_DHCPStatus_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
										<div class="formfonttitle"><#System_Log#> - <#menu5_7_3#></div>
										<div style="margin-lease:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
										<div class="formfontdesc"><#DHCPlease_title#></div>
										<br>
                                                                                <div style="margin-top:8px">
											<div id="leaseblock"></div>
										</div>
										<br>
										<div class="apply_gen">
											<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">
										</div>
									</td>
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
