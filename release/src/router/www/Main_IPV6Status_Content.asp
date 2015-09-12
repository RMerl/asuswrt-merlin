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
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script language="JavaScript" type="text/javascript" src="/merlin.js"></script>

<style>
p{
	font-weight: bolder;
}
</style>


<script>

<% ipv6_pinholes(); %>
<% get_ipv6net_array(); %>

overlib_str_tmp = "";
overlib.isOut = true;

function initial() {
	show_menu();

	show_ipv6config();
	show_ipv6clients();

	if (igd2_support)
	{
		document.getElementById("pinholesdiv").style.display="";
		show_pinholes();
	}
}


function show_ipv6config() {
	var code, i, line

	code = '<table width="100%" id="ipv6config" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">';
	code += '<thead><tr><td colspan="2">IPv6 Configuration</td></tr></thead>';

	if (ipv6cfgarray.length > 1) {
		for (i = 0; i < ipv6cfgarray.length-1; ++i) {
			line = ipv6cfgarray[i];
                        code += '<tr><th>' + line[0] + '</th>';
			code += '<td>' + line[1] + '</td>';
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="2"><span>IPv6 Not enabled.</span></td></tr>';
	}

	code += '</tr></table>';
	document.getElementById("ipv6configblock").innerHTML = code;
}


function show_ipv6clients() {
	var code, i, line

	code = '<table width="100%" id="ipv6clients" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table">';
	code += '<thead><tr><td colspan="3">IPv6 Clients</td></tr></thead>';

	code += '<tr><th width="25%">Hostname</th>';
	code += '<th width="25%">MAC</th>';
	code += '<th width="50%">IP Address</th>';
	code += '</tr>';

	if (ipv6clientarray.length > 1) {
		for (i = 0; i < ipv6clientarray.length-1; ++i) {
			line = ipv6clientarray[i];
			code += '<tr><td>' + line[0].tagescape() + '</td>';

			overlib_str = "<p><#MAC_Address#>:</p>" + line[1];
			code += '<td><span class="ClientName" onclick="oui_query(\'' + line[1] +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ line[1].toUpperCase() +'</span></td>';

			code += '<td>' + line[2] + '</td>';
			code += '</tr>';
                }
	} else {
		code += '<tr><td colspan="3"><span>No IPv6 clients.</span></td></tr>';
	}
	code += '</table>';
	document.getElementById("ipv6clientsblock").innerHTML = code;
}


function show_pinholes() {
	var code, i, rule

	code = '<table width="100%" id="pinholes" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table">';
	code += '<thead><tr><td colspan="5">IPv6 pinhole rules opened in the firewall through UPnP/IGD2</td></tr></thead>';
	code += '<th width=34%">Remote</th>';
	code += '<th width=12%">Port</th>';
	code += '<th width=34%">Local</th>';
	code += '<th width=12%">Port</th>';
	code += '<th width=8%">Proto</th>';
	code += '</tr>';

        if ("<% nvram_get("upnp_pinhole_enable"); %>" == "0") {
                code += '<tr><td colspan="5">Pinhole support is currently disabled.</td></tr>';

	} else if (pinholes.length > 1) {
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
		code += '<tr><td colspan="5"><span>No pinhole configured.</span></td></tr>';
	}

	code += '</tr></table>';
	document.getElementById("pinholesblock").innerHTML = code;
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
											<div id="ipv6configblock"></div>
										</div>
										<br>
                                                                        	<div style="margin-top:8px">
											<div id="ipv6clientsblock"></div>
										</div>
										<br>
										<div id="pinholesdiv" style="display:none;">
											<div id="pinholesblock"></div>
										</div>
										<br><br>
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
