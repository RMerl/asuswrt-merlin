<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_7_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<style>
.wifiheader{
        background-color:#475a5f;
        color:#FFCC00;
}
p{
	font-weight: bolder;
}
</style>

<script>
overlib_str_tmp = "";
overlib.isOut = true;

var refreshRate = 3;
var timedEvent = 0;

<% get_wl_status(); %>;


function initial(){
	show_menu();
	refreshRate = getRefresh();
	get_wlclient_list();
}


function redraw(){
	if (dataarray24.length == 0) {
		document.getElementById('wifi24headerblock').innerHTML='<span class="wifiheader" style="font-size: 125%;">Wireless 2.4 GHz is disabled.</span>';
	} else {
		display_header(dataarray24, 'Wireless 2.4 GHz', document.getElementById('wifi24headerblock'));
		display_clients(wificlients24, document.getElementById('wifi24block'));
	}

	if (band5g_support)  {
		if (wl_info.band5g_2_support) {
			if (dataarray5.length == 0) {
				document.getElementById('wifi5headerblock').innerHTML='<span class="wifiheader" style="font-size: 125%;">Wireless 5 GHz-1 is disabled.</span>';
			} else {
				display_header(dataarray5, 'Wireless 5 GHz-1', document.getElementById('wifi5headerblock'));                                               
				display_clients(wificlients5, document.getElementById('wifi5block'));
			}
			if (dataarray52.length == 0) {
				document.getElementById('wifi52headerblock').innerHTML='<span class="wifiheader" style="font-size: 125%;">Wireless 5 GHz-2 is disabled.</span>';
			} else {
				display_header(dataarray52, 'Wireless 5 GHz-2', document.getElementById('wifi52headerblock'));
				display_clients(wificlients52, document.getElementById('wifi52block'));
			}
		} else {
			if (dataarray5.length == 0) {
				document.getElementById('wifi5headerblock').innerHTML='<span class="wifiheader" style="font-size: 125%;">Wireless 5 GHz is disabled.</span>';
			} else {
				display_header(dataarray5, 'Wireless 5 GHz', document.getElementById('wifi5headerblock'));
				display_clients(wificlients5, document.getElementById('wifi5block'));
			}
		}
	}
}


function display_clients(clientsarray, obj) {
	var code, i, client, overlib_str;

	code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">';
	code += '<thead><tr>';
	code += '<td width="15%">MAC</td>';
	code += '<td width="16%">IP</td>';
	code += '<td width="16%">Name</td><td width="10%">RSSI</td><td width="18%">Rx / Tx Rate</td><td width="12%">Connected</td>';
	code += '<td width="8%">Flags</td>';
	code += '</tr></thead>';

	if (clientsarray.length > 1) {
		for (i = 0; i < clientsarray.length-1; ++i) {
			client = clientsarray[i];
			code += '<tr>';

			// MAC
			overlib_str = "<p><#MAC_Address#>:</p>" + client[0];
			code += '<td><span style="margin-top:-15px; color: white;" class="link" onclick="oui_query(\'' + client[0] +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ client[0] +'</span></td>'; 

			code += '<td>' + client[1] + '</td>';	// IP
			code += '<td>' + client[2] + '</td>';	// Name
			code += '<td>' + client[3] + ' dBm</td>';	// RSSI
			code += '<td>' + client[4] + ' / ' + client[5] +' Mbps</td>';	// Rate
			code += '<td>' + client[6] + '</td>';	// Time
			code += '<td>' + client[7] + '</td>';	// Flags
			code += '</tr>';
		}
	} else {
		code += '<tr><td colspan="7">No clients</td></tr>';
	}

	code += '</tr></table>';
	obj.innerHTML = code;
}


function display_header(dataarray, title, obj) {
	var code;

	code = '<table width="100%" style="border: none;">';
	code += '<thead><tr><span class="wifiheader" style="font-size: 125%;">' + title +'</span></tr></thead>';
	code += '<tr><td colspan="3"><span class="wifiheader">SSID: </span>' + dataarray[0] + '</td><td colspan="2"><span class="wifiheader">Mode: </span>' + dataarray[6] + '</td></tr>';
	code += '<tr><td><span class="wifiheader">RSSI: </span>' + dataarray[1] + ' dBm</td> <td><span class="wifiheader">SNR: </span>' + dataarray[2] +' dB</td> <td>';
	code += '<span class="wifiheader">Noise: </span>' + dataarray[3] + ' dBm</td><td><span class="wifiheader">Channel: </span>'+ dataarray[4] + '</td>';
	code += '<td><span class="wifiheader">BSSID: </span>' + dataarray[5] +'</td></tr></table>';

	obj.innerHTML = code;
}


function get_wlclient_list() {

	if (timedEvent) {
		clearTimeout(timedEvent);
		timedEvent = 0;
	}

	$.ajax({
		url: '/ajax_wificlients.asp',
		dataType: 'script', 
		error: function(xhr){
				get_wlclient_list();
				},
		success: function(response){
			redraw();
			if (refreshRate > 0)
				timedEvent = setTimeout("get_wlclient_list();", refreshRate * 1000);
		}
	});

}


function getRefresh() {
	val  = parseInt(cookie.get('awrtm_wlrefresh'));

	if ((val != 0) && (val != 1) && (val != 3) && (val != 5) && (val != 10))
		val = 3;

	document.getElementById('refreshrate').value = val;

	return val;
}


function setRefresh(obj) {
	refreshRate = obj.value;
	cookie.set('awrtm_wlrefresh', refreshRate, 300);
	get_wlclient_list();
}


</script>
</head>
<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_WStatus_Content.asp">
<input type="hidden" name="next_page" value="Main_WStatus_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
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
		<!-- ===================================Beginning of Main Content===========================================-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >           
						<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3" class="FormTitle" id="FormTitle">
							<tr bgcolor="#4D595D">
								<td valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#System_Log#> - <#menu5_7_4#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">List of connected Wireless clients</div>

									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th>Automatically refresh list every</th>
											<td>
												<select name="refreshrate" class="input_option" onchange="setRefresh(this);" id="refreshrate">
													<option value="0">No refresh</option>
													<option value="1">1 second</option>
													<option value="3" selected>3 seconds</option>
													<option value="5">5 seconds</option>
													<option value="10">10 seconds</option>
												</select>
											</td>
										</tr>
									</table>
									<br>
									<div id="wifi24headerblock"></div>
									<div id="wifi24block"></div>
									<br><br>
									<div id="wifi5headerblock"></div>
									<div id="wifi5block"></div>
									<br><br>
									<div id="wifi52headerblock"></div>
									<div id="wifi52block"></div>
									<div>Flags: <span class="wifiheader">P</span>=Powersave Mode, <span class="wifiheader">S</span>=Short GI, <span class="wifiheader">T</span>=STBC, <span class="wifiheader">A</span>=Associated, <span class="wifiheader">U</span>=Authenticated, <span class="wifiheader">G</span>=Guest</div>
									<br>
									<div class="apply_gen">
										<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen" >
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
<div id="footer"></div>
</form>
</body>
</html>

