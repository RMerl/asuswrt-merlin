<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - System Information</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/nameresolv.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

hwacc = "<% nvram_get("ctf_disable"); %>";
hwacc_force = "<% nvram_get("ctf_disable_force"); %>";
arplist = [<% get_arp_table(); %>];
etherstate = "<% sysinfo("ethernet"); %>";

var $j = jQuery.noConflict();

function initial(){
	show_menu();
        if (!rc_support.search("5G")) $("wifi5_clients_tr").style.display = "none";
	showbootTime();
	update_temperatures();
	hwaccel_state();
	show_etherstate();
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
			if (rc_support.search("5G")) {
				code += "&nbsp;&nbsp;-&nbsp;&nbsp;<b>5 GHz:</b> <span>" + curr_coreTmp_5_raw + "</span>";
			}
			$("temp_td").innerHTML = code;
			setTimeout("update_temperatures();", 3000);
		}
	});
}


function hwaccel_state(){
	if (hwacc == "1") {
		code = "<span>Disabled</span>";
		if (hwacc_force == "1")
			code += " <i>(by user)</i>";
		else
			code += " <i>(incompatible feature enabled)</i>";
	} else if (hwacc == "0") {
		code = "<span>Enabled</span>";
	} else {
		code = "<span>N/A</span>";
	}

	$("hwaccel").innerHTML = code;
}


function showbootTime(){
        Days = Math.floor(boottime / (60*60*24));        
        Hours = Math.floor((boottime / 3600) % 24);
        Minutes = Math.floor(boottime % 3600 / 60);
        Seconds = Math.floor(boottime % 60);
        
        $("boot_days").innerHTML = Days;
        $("boot_hours").innerHTML = Hours;
        $("boot_minutes").innerHTML = Minutes;
        $("boot_seconds").innerHTML = Seconds;
        boottime += 1;
        setTimeout("showbootTime()", 1000);
}

function show_etherstate(){
	var state, state2;
	var hostname, devicename, overlib_str, port;
	var tmpPort;
	var code = '<table cellpadding="0" cellspacing="0" width="100%"><tr><th>Port</th><th>Link State</th><th>Last Device Seen</th></tr>';

	var t = etherstate.split('>');

	for (var i = 0; i < t.length; ++i) {
		var line = t[i].split(/[\s]+/);
		if (line[0] == "Port") {
			if (line[2] == "DOWN")
				state2 = "Down";
			else {
				state = line[2].replace("FD"," Full Duplex");
				state2 = state.replace("HD"," Half Duplex");
			}

			hostname = "";

			if (line[11] == "00:00:00:00:00:00") {
				devicename = '<span class="ClientName">&lt;none&gt;</span>';
			} else {
				overlib_str = "<p><#MAC_Address#></p>" + line[11];

				// Retrieve through lease list, else walk down arp cache and retrieve from hostname cache
				for (var j = 0; j < arplist.length; ++j) {
					if (arplist[j][3] == line[11].toUpperCase()) {
						hostname = hostnamecache[arplist[j][0]];
						break;
					}
				}

//				if (hostname == "") hostname = retHostName(line[11]);

				if (hostname != "") {
					devicename = '<span class="ClientName" onmouseover="return overlib(\''+overlib_str +'\');" onmouseout="nd();">'+ hostname +'</span>';
				} else {
					devicename = '<span class="ClientName" onclick="getOUIFromMAC(\'' + line[11] +'\');" style="cursor:pointer; text-decoration:underline;">'+ line[11] +'</span>'; 
				}
			}
			tmpPort = line[1].replace(":","");
			if (tmpPort == "0") {
				port = "WAN";
			} else if (tmpPort == "8") {
				break;
			} else {
				port = "LAN "+tmpPort;
			}
			code += '<tr><td width="15%">'+port+'</td><td width="30%"><span>' + state2 + '</span></td><td width="55%">'+ devicename +'</td></tr>';
		}
	}
	code += '</table>';
	$("etherstate_td").innerHTML = code;

	if (hostnamecache['ready'] == 0) setTimeout(show_etherstate, 500);
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
<input type="hidden" name="next_host" value="">
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
				        	<td><% nvram_get("productid"); %></td>
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
					<tr>
						<th>Features</th>
						<td><% nvram_get("rc_support"); %></td>
					</tr>
					<tr>
						<th><#General_x_SystemUpTime_itemname#></a></th>
						<td><span id="boot_days"></span> <#Day#> <span id="boot_hours"></span> <#Hour#> <span id="boot_minutes"></span> <#Minute#> <span id="boot_seconds"></span> <#Second#></td>
					</tr>

					<tr>
						<th>Radios temperature</th>
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
                                                <th>CPU Load</th>
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
						</tr>
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

