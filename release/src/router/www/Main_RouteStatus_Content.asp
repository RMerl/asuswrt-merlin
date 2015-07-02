<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_7_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script>
<% get_route_array(); %>
function initial() {
	show_menu();
	show_routev4();
	show_routev6();
}


function show_routev4() {
	var code, i, line;

	code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table">';
	code += '<thead><tr><td colspan="8">IPv4 Routing table</td></tr></thead>';
	code += '<tr><th width="19%">Destination</th>';
	code += '<th width="19%">Gateway</th>';
	code += '<th width="16%">Genmask</th>';
	code += '<th width="11%">Flags</th>';
	code += '<th width="10%">Metric</th>';
	code += '<th width="8%">Ref</th>';
	code += '<th width="8%">Use</th>';
	code += '<th width="9%">Iface</th>';
	code += '</tr>';

	if (routearray.length > 1) {
		for (i = 0; i < routearray.length-1; ++i) {
			line = routearray[i];

			code += '<tr>';
			code += '<td>' + line[0] + '</td>';
			code += '<td>' + line[1] + '</td>';
			code += '<td>' + line[2] + '</td>';
			code += '<td>' + line[3] + '</td>';
			code += '<td>' + line[4] + '</td>';
			code += '<td>' + line[5] + '</td>';
			code += '<td>' + line[6] + '</td>';
			code += '<td>' + line[7] + '</td>';
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="8"><span>No IPv4 routes.</span></td></tr>';
	}

	code += '</tr></table>';
	document.getElementById("routev4block").innerHTML = code;
}


function show_routev6() {
	var code, i, line;

	code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table">';
	code += '<thead><tr><td colspan="6">IPv6 Routing table</td></tr></thead>';
	code += '<tr><th width="50%">Destination<br><span style="color:#FFCC00;">Next Hop</span></th>';
	code += '<th width="10%">Flags</th>';
	code += '<th width="10%">Metric</th>';
	code += '<th width="10%">Ref</th>';
	code += '<th width="10%">Use</th>';
	code += '<th width="10%">Iface</th>';
	code += '</tr>';

	if (routev6array.length > 1) {
		for (i = 0; i < routev6array.length-1; ++i) {
			line = routev6array[i];

			code += '<tr>';
			code += '<td>' + line[0] + '<br><span style="color:#FFCC00;">' + line[1] + '</span></td>';
			code += '<td>' + line[2] + '</td>';
			code += '<td>' + line[3] + '</td>';
			code += '<td>' + line[4] + '</td>';
			code += '<td>' + line[5] + '</td>';
			code += '<td>' + line[6] + '</td>';
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="6"><span>No IPv6 routes.</span></td></tr>';
	}

	code += '</tr></table>';
	document.getElementById("routev6block").innerHTML = code;
}

</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_RouteStatus_Content.asp">
<input type="hidden" name="next_page" value="Main_RouteStatus_Content">
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
		<div  id="mainMenu"></div>
		<div  id="subMenu"></div>
	</td>
    <td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			<tr>
				<td valign="top" >          
					<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
						<tr bgcolor="#4D595D">
							<td valign="top">
								<div>&nbsp;</div>
								<div class="formfonttitle"><#System_Log#> - <#menu5_7_6#></div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc"><#Route_title#></div>
								<div style="margin-top:8px">
									<div id="routev4block"></div>
								</div>
								<br>
								<div style="margin-top:8px">
									<div id="routev6block"></div>
								</div>
								<div class="apply_gen">
									<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">
								</div>
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
