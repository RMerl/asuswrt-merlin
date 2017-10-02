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
<link rel="stylesheet" type="text/css" href="/js/table/table.css">
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

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/js/table/table.js"></script>
<script>

var hwacc = "<% nvram_get("ctf_disable"); %>";
var hwacc_force = "<% nvram_get("ctf_disable_force"); %>";
var etherstate = "<% sysinfo("ethernet"); %>";
var rtkswitch = <% sysinfo("ethernet.rtk"); %>;
var odmpid = "<% nvram_get("odmpid");%>";
var ctf_fa = "<% nvram_get("ctf_fa_mode"); %>";

overlib_str_tmp = "";
overlib.isOut = true;

function initial(){
	show_menu();

	if (wl_info.band5g_2_support) {
		document.getElementById("wifi51_clients_th").innerHTML = "Wireless Clients (5 GHz-1)";
		document.getElementById("wifi5_2_clients_tr").style.display = "";
        } else if (based_modelid == "RT-AC87U") {
                document.getElementById("wifi5_clients_tr_qtn").style.display = "";
                document.getElementById("qtn_version").style.display = "";
        } else if (band5g_support) {
                document.getElementById("wifi5_clients_tr").style.display = "";
        }

	showbootTime();

	if (odmpid != "")
		document.getElementById("model_id").innerHTML = odmpid;
	else
		document.getElementById("model_id").innerHTML = productid;

	var buildno = '<% nvram_get("buildno"); %>';
	var extendno = '<% nvram_get("extendno"); %>';
	if ((extendno == "") || (extendno == "0"))
		document.getElementById("fwver").innerHTML = buildno;
	else
		document.getElementById("fwver").innerHTML = buildno + '_' + extendno;


	var rc_caps = "<% nvram_get("rc_support"); %>";
	var rc_caps_arr = rc_caps.split(' ').sort();
	rc_caps = rc_caps_arr.toString().replace(/,/g, " ");
	document.getElementById("rc_td").innerHTML = rc_caps;

	hwaccel_state();
	update_temperatures();
	updateClientList();
	update_sysinfo();
}

function update_temperatures(){
	$.ajax({
		url: '/ajax_coretmp.asp',
		dataType: 'script',
		error: function(xhr){
			update_temperatures();
		},
		success: function(response){
			code = "<b>2.4 GHz:</b><span> " + curr_coreTmp_2_raw + "</span>";

			if (band5g_support)
				code += "&nbsp;&nbsp;-&nbsp;&nbsp;<b>5 GHz:</b> <span>" + curr_coreTmp_5_raw + "</span>";

			if (curr_coreTmp_cpu != "")
				code +="&nbsp;&nbsp;-&nbsp;&nbsp;<b>CPU:</b> <span>" + curr_coreTmp_cpu +"&deg;C</span>";

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
	var line;
	var wan_array;
	var port_array= Array();

	if ((based_modelid == "RT-N16") || (based_modelid == "RT-AC87U")
	    || (based_modelid == "RT-AC3200") || (based_modelid == "RT-AC88U")
	    || (based_modelid == "RT-AC3100"))
		reversed = true;
	else
		reversed = false;

	var t = etherstate.split('>');
	for (var i = 0; i < t.length; ++i) {
		line = t[i].split(/[\s]+/);
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
					hostname = (clientList[devicemac].nickName == "") ? clientList[devicemac].hostname : clientList[devicemac].nickName;

				if ((typeof hostname !== 'undefined') && (hostname != "")) {
					devicename = '<span class="ClientName" onclick="oui_query_full_vendor(\'' + devicemac +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ hostname +'</span>';
				} else {
					devicename = '<span class="ClientName" onclick="oui_query_full_vendor(\'' + devicemac +'\');;overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">'+ devicemac +'</span>'; 
				}
			}
			port = line[1].replace(":","");

			if (port == "8") {		// CPU Port
				continue;
			} else if ((based_modelid == "RT-AC56U") || (based_modelid == "RT-AC56S") || (based_modelid == "RT-AC88U") || (based_modelid == "RT-AC3100")) {
				port++;		// Port starts at 0
				if (port == "5") port = 0;	// Last port is WAN
			} else if (based_modelid == "RT-AC87U") {
				if (port == "4")
					continue;	// This is the internal LAN port
				if (port == "10") {
					port = "4";	// This is LAN 4 (RTL) from QTN
					devicename = '<span class="ClientName">&lt;unknown&gt;</span>';
				}
			}
			if (port == "0") {
				wan_array = [ "WAN", (line[7] & 0xFFF), state2, devicename];
				continue;
			} else if (port > 4) {
				continue;	// Internal port
			} else {
				if (reversed) port = 5 - port;
			}

			if (reversed)
				port_array.unshift(["LAN "+ port, (line[7] & 0xFFF), state2, devicename]);
			else
				port_array.push(["LAN " + port, (line[7] & 0xFFF), state2, devicename]);

		}
	}

	if (based_modelid == "RT-AC88U")
	{
		for (var i = 0; i < rtkswitch.length; i++) {
			line = rtkswitch[i];
			if (line[1] == "0")
				state = "Down"
			else
				state = line[1] + " Mbps";

			port_array.push(['LAN ' +line[0] + ' (RTK)', 'NA', state, '&lt;unknown&gt;']);
		}

	}

	/* Add WAN last, so it can be always at the top */
	port_array.unshift(wan_array);

	var tableStruct = {
		data: port_array,
		container: "tableContainer",
		header: [
			{
				"title" : "Port",
				"width" : "15%"
			},
			{
				"title" : "VLAN",
				"width" : "15%"
			},
			{
				"title" : "Link State",
				"width" : "25%"
			},
			{
				"title" : "Last Device Seen",
				"width" : "45%"
			}
		]
	}

	if(tableStruct.data.length) {
		tableApi.genTableAPI(tableStruct);
	}
}


function show_connstate(){
	document.getElementById("conn_td").innerHTML = conn_stats_arr[0] + " / <% sysinfo("conn.max"); %>&nbsp;&nbsp;-&nbsp;&nbsp;" + conn_stats_arr[1] + " active";

	document.getElementById("wlc_24_td").innerHTML = "Associated: <span>" + wlc_24_arr[0] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
	                                                 "Authorized: <span>" + wlc_24_arr[1] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
	                                                 "Authenticated: <span>" + wlc_24_arr[2] + "</span>";

	if (band5g_support) {
		if (based_modelid == "RT-AC87U") {
			document.getElementById("wlc_5qtn_td").innerHTML = "Associated: <span>" +wlc_51_arr[0] + "</span>";
		} else {
			document.getElementById("wlc_51_td").innerHTML = "Associated: <span>" + wlc_51_arr[0] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
			                                                 "Authorized: <span>" + wlc_51_arr[1] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
			                                                 "Authenticated: <span>" + wlc_51_arr[2] + "</span>";
		}
	}

	if (wl_info.band5g_2_support) {
		document.getElementById("wlc_52_td").innerHTML = "Associated: <span>" + wlc_52_arr[0] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
		                                                 "Authorized: <span>" + wlc_52_arr[1] + "</span>&nbsp;&nbsp;-&nbsp;&nbsp;" +
		                                                 "Authenticated: <span>" + wlc_52_arr[2] + "</span>";
	}

}


function show_memcpu(){
	document.getElementById("cpu_stats_td").innerHTML = cpu_stats_arr[0] + ", " + cpu_stats_arr[1] + ", " + cpu_stats_arr[2];
	document.getElementById("mem_total_td").innerHTML = mem_stats_arr[0] + " MB";
	document.getElementById("mem_free_td").innerHTML = mem_stats_arr[1] + " MB";
	document.getElementById("mem_buffer_td").innerHTML = mem_stats_arr[2] + " MB";
	document.getElementById("mem_cache_td").innerHTML = mem_stats_arr[3] + " MB";
	document.getElementById("mem_swap_td").innerHTML = mem_stats_arr[4] + " / " + mem_stats_arr[5] + " MB";

	document.getElementById("nvram_td").innerHTML = mem_stats_arr[6] + " / " + <% sysinfo("nvram.total"); %> + " bytes";
	document.getElementById("jffs_td").innerHTML = mem_stats_arr[7];
}


function updateClientList(e){
	$.ajax({
		url: '/update_clients.asp',
		dataType: 'script',
		error: function(xhr) {
			setTimeout("updateClientList();", 1000);
		},
		success: function(response){
			setTimeout("updateClientList();", 3000);
		}
	});
}

function update_sysinfo(e){
	$.ajax({
		url: '/ajax_sysinfo.asp',
		dataType: 'script',
		error: function(xhr) {
			setTimeout("update_sysinfo();", 1000);
		},
		success: function(response){
			show_memcpu();
			show_etherstate();
			show_connstate();
			setTimeout("update_sysinfo();", 3000);
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
						<td id="rc_td"></td>
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
						<td id="cpu_stats_td"></td>
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
						<td id="mem_total_td"></td>
					</tr>

					<tr>
						<th>Free</th>
						<td id="mem_free_td"></td>
					</tr>

					<tr>
						<th>Buffers</th>
						<td id="mem_buffer_td"></td>
					</tr>

					<tr>
						<th>Cache</th>
						<td id="mem_cache_td"></td>
					</tr>

					<tr>
						<th>Swap</th>
						<td id="mem_swap_td"></td>
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
						<td id="nvram_td"></td>
					</tr>
					<tr>
						<th>JFFS</th>
						<td id="jffs_td"></td>
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
						<td id="conn_td"></td>
					</tr>
					<tr>
						<th>Ethernet Ports</th>
						<td>
							<div id="tableContainer" style="margin-top:-10px;"></div>
						</td>
					</tr>
					<tr>
						<th>Wireless clients (2.4 GHz)</th>
						<td id="wlc_24_td"></td>
					</tr>
					<tr id="wifi5_clients_tr" style="display:none;">
						<th id="wifi51_clients_th">Wireless clients (5 GHz)</th>
						<td id="wlc_51_td"></td>
					</tr>
					<tr id="wifi5_2_clients_tr" style="display:none;">
						<th>Wireless clients (5 GHz-2)</th>
						<td id="wlc_52_td"></td>
					</tr>
					<tr id="wifi5_clients_tr_qtn" style="display:none;">
						<th>Wireless clients (5 GHz)</th>
						<td id="wlc_5qtn_td"></td>
					</tr>
				</table>
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

