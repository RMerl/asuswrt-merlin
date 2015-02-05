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
<title><#Web_Title#> - System Information</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
p{
	font-weight: bolder;
}
</style>

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script>

hwacc = "<% nvram_get("ctf_disable"); %>";
hwacc_force = "<% nvram_get("ctf_disable_force"); %>";
arplist = [<% get_arp_table(); %>];
etherstate = "<% sysinfo("ethernet"); %>";
odmpid = "<% nvram_get("odmpid");%>";
ctf_fa = "<% nvram_get("ctf_fa_mode"); %>";

var $j = jQuery.noConflict();

overlib_str_tmp = "";
overlib.isOut = true;

function initial(){
	show_menu();
	if (!band5g_support) document.getElementById("wifi5_clients_tr").style.display = "none";
	if (based_modelid == "RT-AC87U") {
		document.getElementById("wifi5_clients_tr").style.display = "none";
		document.getElementById("wifi5_clients_tr_qtn").style.display = "";
		document.getElementById("qtn_version").style.display = "";
	}
	showbootTime();

	if (odmpid != "")
		document.getElementById("model_id").innerHTML = odmpid;
	else
		document.getElementById("model_id").innerHTML = productid;

	var buildno = '<% nvram_get("buildno"); %>';
	var firmver = '<% nvram_get("firmver"); %>'
	var extendno = '<% nvram_get("extendno"); %>';
	if ((extendno == "") || (extendno == "0"))
		document.getElementById("fwver").innerHTML = firmver + "." + buildno;
	else
		document.getElementById("fwver").innerHTML =  firmver + "." + buildno + '_' + extendno.split("-g")[0];

	update_temperatures();
	hwaccel_state();
	show_etherstate();
	updateClientList();
}

function update_temperatures(){
	$j.ajax({
		url: '/ajax_coretmp.asp',
		dataType: 'script',
		error: function(xhr){
			update_temperatures();
		},
		success: function(response){
			code = "<b>2.4 GHz:</b><span> " + curr_coreTmp_2_raw + "</span>";
			if (band5g_support) {
				code += "&nbsp;&nbsp;-&nbsp;&nbsp;<b>5 GHz:</b> <span>" + curr_coreTmp_5_raw + "</span>";
			}
			if ((based_modelid == "RT-N18U") || (based_modelid == "RT-AC56U") || (based_modelid == "RT-AC56S") || (based_modelid == "RT-AC68U") || (based_modelid == "RT-AC87U")) {
				code +="&nbsp;&nbsp;-&nbsp;&nbsp;<b>CPU:</b> <span>" + curr_coreTmp_cpu +"&deg;C</span>";
			}
			document.getElementById("temp_td").innerHTML = code;
			setTimeout("update_temperatures();", 3000);
		}
	});
}


function hwaccel_state(){
	if (hwacc == "1") {
		code = "Disabled";
		if (hwacc_force == "1")
			code += " <i>(by user)</i>";
		else {
			code += " <i> - incompatible with:<span>  ";	// Two trailing spaces
			if ('<% nvram_get("cstats_enable"); %>' == '1') code += 'IPTraffic, ';
			if (('<% nvram_get("qos_enable"); %>' == '1') && ('<% nvram_get("qos_type"); %>' == '0')) code += 'QoS, ';
			if ('<% nvram_get("sw_mode"); %>' == '2') code += 'Repeater mode, ';
			if ('<% nvram_get("ctf_disable_modem"); %>' == '1') code += 'USB modem, ';

			// We're disabled but we don't know why
			if (code.slice(-2) == "  ") code += "&lt;unknown&gt;, ";

			// Trim two trailing chars, either "  " or ", "
			code = code.slice(0,-2) + "</span></>";
		}
	} else if (hwacc == "0") {
		code = "<span>Enabled";
		if (ctf_fa != "") {
                        if (ctf_fa != "0")
                                code += " (CTF + FA)";
                        else
                                code += " (CTF only)";
                }
		code += "</span>";
	} else {
		code = "<span>N/A</span>";
	}

	document.getElementById("hwaccel").innerHTML = code;
}


function showbootTime(){
        Days = Math.floor(boottime / (60*60*24));        
        Hours = Math.floor((boottime / 3600) % 24);
        Minutes = Math.floor(boottime % 3600 / 60);
        Seconds = Math.floor(boottime % 60);
        
        document.getElementById("boot_days").innerHTML = Days;
        document.getElementById("boot_hours").innerHTML = Hours;
        document.getElementById("boot_minutes").innerHTML = Minutes;
        document.getElementById("boot_seconds").innerHTML = Seconds;
        boottime += 1;
        setTimeout("showbootTime()", 1000);
}

function show_etherstate(){
	var state, state2;
	var hostname, devicename, devicemac, overlib_str, port;
	var tmpPort;
	var code = '<table cellpadding="0" cellspacing="0" width="100%"><tr><th style="width:15%;">Port</th><th style="width:15%;">VLAN</th><th style="width:25%;">Link State</th>';
	code += '<th style="width:45%;">Last Device Seen</th></tr>';

	var code_ports = "";
	var entry;

	var t = etherstate.split('>');
	for (var i = 0; i < t.length; ++i) {
		var line = t[i].split(/[\s]+/);
		if (line[11])
			devicemac = line[11].toUpperCase();
		else
			devicemac = "";

		if (line[0] == "Port") {
			if (line[2] == "DOWN")
				state2 = "Down";
			else {
				state = line[2].replace("FD"," Full Duplex");
				state2 = state.replace("HD"," Half Duplex");
			}

			hostname = "";

			if (devicemac == "00:00:00:00:00:00") {
				devicename = '<span class="ClientName">&lt;none&gt;</span>';
			} else {
				overlib_str = "<p><#MAC_Address#>:</p>" + devicemac;

				if (clientList[devicemac])
					hostname = clientList[devicemac].name;

				if ((hostname != "") && (typeof hostname !== 'undefined')) {
					devicename = '<span class="ClientName" onclick="oui_query(\'' + devicemac +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ hostname +'</span>';
				} else {
					devicename = '<span class="ClientName" onclick="oui_query(\'' + devicemac +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ devicemac +'</span>'; 
				}
			}
			tmpPort = line[1].replace(":","");

			if (tmpPort == "8") {		// CPU Port
				continue;
			} else if ((based_modelid == "RT-AC56U") || (based_modelid == "RT-AC56S")) {
				tmpPort++;		// Port starts at 0
				if (tmpPort == "5") tmpPort = 0;	// Last port is WAN
			} else if (based_modelid == "RT-AC87U") {
				if (tmpPort == "4")
					continue;	// This is the internal LAN port
				if (tmpPort == "5") {
					tmpPort = "4";	// This is the LAN 4 port from QTN
					devicename = '<span class="ClientName">&lt;unknown&gt;</span>';
				}
			}
			if (tmpPort == "0") {
				port = "WAN";
			} else {
				if ((based_modelid == "RT-N16") || (based_modelid == "RT-AC87U"))  tmpPort = 5 - tmpPort;
				port = "LAN "+tmpPort;
			}
			entry = '<tr><td>' + port + '</td><td>' + (line[7] & 0xFFF) + '</td><td><span>' + state2 + '</span></td>';
			entry += '<td>'+ devicename +'</td></tr>';

			if (based_modelid == "RT-N16")
				code_ports = entry + code_ports;
			else
				code_ports += entry;
		}
	}
	code += code_ports + '</table>';
	document.getElementById("etherstate_td").innerHTML = code;
}

function updateClientList(e){
	$j.ajax({
		url: '/update_clients.asp',
		dataType: 'script', 
		error: function(xhr) {
			setTimeout("updateClientList();", 1000);
		},
		success: function(response){
			if(isJsonChanged(originData, originDataTmp)){
				show_etherstate();
			}

			setTimeout("updateClientList();", 3000);
		}
	});
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Tools_Sysinfo.asp">
<input type="hidden" name="next_page" value="Tools_Sysinfo.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="ct_tcp_timeout" value="<% nvram_get("ct_tcp_timeout"); %>">
<input type="hidden" name="ct_udp_timeout" value="<% nvram_get("ct_udp_timeout"); %>">



<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div></td>
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
                <div class="formfonttitle">Tools - System Information</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Router</td>
						</tr>
					</thead>
					<tr>
						<th>Model</th>
				        	<td id="model_id"><% nvram_get("productid"); %></td>
					</tr>
					<tr>
						<th>Firmware Version</th>
						<td id="fwver"></td>
					</tr>

					<tr>
						<th>Firmware Build</th>
						<td><% nvram_get("buildinfo"); %></td>
					</tr>
					<tr>
						<th>Bootloader (CFE)</th>
						<td><% sysinfo("cfe_version"); %></td>
					</tr>
					<tr>
						<th>Driver version</th>
						<td><% sysinfo("driver_version"); %></td>
					</tr>
					<tr id="qtn_version" style="display:none;">
						<th>Quantenna Firmware</th>
						<td><% sysinfo("qtn_version"); %></td>
					</tr>
					<tr>
						<th>Features</th>
						<td><% nvram_get("rc_support"); %></td>
					</tr>
					<tr>
						<th><#General_x_SystemUpTime_itemname#></a></th>
						<td><span id="boot_days"></span> <#Day#> <span id="boot_hours"></span> <#Hour#> <span id="boot_minutes"></span> <#Minute#> <span id="boot_seconds"></span> <#Second#></td>
					</tr>

					<tr>
						<th>Temperatures</th>
						<td id="temp_td"></td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">CPU</td>
						</tr>
					</thead>

					<tr>
						<th>CPU Model</th>
						<td><% sysinfo("cpu.model"); %>	</td>
					</tr>
					<tr>
						<th>CPU Frequency</th>
						<td><% sysinfo("cpu.freq"); %> MHz</td>
					</tr>
					<tr>
						<th>CPU Load Average (1, 5, 15 mins)</th>
						<td>
							<% sysinfo("cpu.load.1"); %>,&nbsp;
							<% sysinfo("cpu.load.5"); %>,&nbsp;
							<% sysinfo("cpu.load.15"); %>
						</td>
					</tr>

				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Memory</td>
						</tr>
					</thead>
 					<tr>
						<th>Total</th>
						<td> <% sysinfo("memory.total"); %>&nbsp;MB</td>
					</tr>

					<tr>
						<th>Free</th>
						<td> <% sysinfo("memory.free"); %>&nbsp;MB</td>
					</tr>

 					<tr>
						<th>Buffers</th>
						<td> <% sysinfo("memory.buffer"); %>&nbsp;MB</td>
					</tr>

					<tr>
						<th>Swap usage</th>
						<td><% sysinfo("memory.swap.used"); %> / <% sysinfo("memory.swap.total"); %>&nbsp;MB</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0"bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Internal Storage</td>
						</tr>
					</thead>
					<tr>
						<th>NVRAM usage</th>
						<td><% sysinfo("nvram.used"); %>&nbsp;/ <% sysinfo("nvram.total"); %> bytes</td>
					</tr>
					<tr>
						<th>JFFS</th>
						<td><% sysinfo("jffs.usage"); %></td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0"bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Network</td>
						</tr>
					</thead>
					<tr>
						<th>HW acceleration</th>
						<td id="hwaccel"></td>
					</tr>
					<tr>
						<th>Connections</th>
						<td><% sysinfo("conn.total"); %>&nbsp;/ <% sysinfo("conn.max"); %>&nbsp;&nbsp;-&nbsp;&nbsp;<% sysinfo("conn.active"); %> active</td>
					</tr>
					<tr>
						<th>Ethernet Ports</th>
						<td id="etherstate_td"><i><span>Querying switch...</span></i></td>
					</tr>
					<tr>
						<th>Wireless clients (2.4 GHz)</th>
						<td>
							Associated: <span><% sysinfo("conn.wifi.2.assoc"); %></span>&nbsp;&nbsp;-&nbsp;&nbsp;
							Authorized: <span><% sysinfo("conn.wifi.2.autho"); %></span>&nbsp;&nbsp;-&nbsp;&nbsp;
							Authenticated: <span><% sysinfo("conn.wifi.2.authe"); %></span>
						</td>
					</tr>
					<tr id="wifi5_clients_tr">
						<th>Wireless clients (5 GHz)</th>
						<td>
							Associated: <span><% sysinfo("conn.wifi.5.assoc"); %></span>&nbsp;&nbsp;-&nbsp;&nbsp;
							Authorized: <span><% sysinfo("conn.wifi.5.autho"); %></span>&nbsp;&nbsp;-&nbsp;&nbsp;
							Authenticated: <span><% sysinfo("conn.wifi.5.authe"); %></span>
						</td>
					</tr>
					<tr id="wifi5_clients_tr_qtn" style="display:none;">
						<th>Wireless clients (5 GHz)</th>
						<td>
                                                        Associated: <span><% sysinfo("conn.wifi.5.assoc"); %></span>
						</td>
					</tr>
				</table>
				</td>
				</tr>

				<tr class="apply_gen" valign="top" height="95px">
					<td>
						<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">
					</td>
				</tr>
	        </tbody>
            </table>
            </form>
            </td>

       </tr>
      </table>
      <!--===================================Ending of Main Content===========================================-->
    </td>
    <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>
<div id="footer"></div>
</body>
</html>

