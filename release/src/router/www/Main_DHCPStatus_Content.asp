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
<link rel="stylesheet" type="text/css" href="/js/table/table.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/js/table/table.js"></script>
<style>
p{
	font-weight: bolder;
}
.tableApi_table th {
       height: 20px;
}
.data_tr {
       height: 30px;
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

function pad(num, size) {
    var s = "00" + num;
    return s.substr(s.length-size);
}

function show_leases() {
	var i, line;
	var Days, Hours, Minutes, Seconds;
	var tableStruct;

	leasearray.pop();	// Remove last empty element

	if (leasearray.length > 0) {
		for (i = 0; i < leasearray.length; ++i) {
			line = leasearray[i];
			Days = Math.floor(line[0] / (60*60*24));
			Hours = Math.floor((line[0] / 3600) % 24);
			Minutes = Math.floor(line[0] % 3600 / 60);
			Seconds = Math.floor(line[0] % 60);
			leasearray[i][0] = pad(Days,2) + "d " + pad(Hours,2) + "h " + pad(Minutes,2) + "m "+ pad(Seconds,2) + "s";

			overlib_str = "<p><#MAC_Address#>:</p>" + line[1];
			leasearray[i][1] = '<span class="ClientName" onclick="oui_query_full_vendor(\'' + line[1].toUpperCase() +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ line[1].toUpperCase() +'</span>';
		}

		tableStruct = {
			data: leasearray,
			container: "leaseblock",
			title: "DHCP Leases",
			header: [
				{
					"title" : "Time Left",
					"width" : "20%",
					"sort" : "str"
				},
				{
					"title" : "MAC Address",
					"width" : "20%",
					"sort" : "str"
				},
				{
					"title" : "IP Address",
					"width" : "20%",
					"sort" : "ip",
					"defaultSort" : "increase"
				},
				{
					"title" : "Hostname",
					"width" : "40%",
					"sort" : "str"
				}
			]
		}

		tableApi.genTableAPI(tableStruct);

	} else {
		document.getElementById("leaseblock").innerHTML = '<span style="color:#FFCC00;">No active leases.</span>';
	}
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
