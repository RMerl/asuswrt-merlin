﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - VPN Status</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<style>
	.statcell { width:25% !important; text-align:left !important; }
</style>

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language-:JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
vpnc_clientlist_array = decodeURIComponent('<% nvram_char_to_ascii("","vpnc_clientlist"); %>');

function initial(){
	show_menu();

	if (openvpnd_support) {
		setTimeout("refreshData()",1000);
		if (based_modelid == "RT-AC3200") {
			showhide("client3", 0);
			showhide("client4", 0);
			showhide("client5", 0);
		}
	} else {
		showhide("server1", 0);
		showhide("server2", 0);
		showhide("client1", 0);
		showhide("client2", 0);
		showhide("client3", 0);
		showhide("client4", 0);
		showhide("client5", 0);
	}
}


function refreshData(){
	$.ajax({
		url: 'ajax_vpn_status.asp',
		dataType: 'script',
		error: function(xhr){
			refreshData();
		},
		success: function(response){
			displayData();
		}
	});
}

function displayData(){
	var state_srv_run = " - Running";
	var state_srv_stop = " - <span style=\"background-color: transparent; color: white;\">Stopped</span>";
	var state_clnt_ced = " - Connected";
	var state_clnt_cing = " - Connecting...";
	var state_clnt_err = " - Error connecting";
	var state_clnt_disc = " - <span style=\"background-color: transparent; color: white;\">Stopped</span>";
	var state_desc, tmp;

	if (server1pid > 0)
		document.getElementById("server1_Block_Running").innerHTML = state_srv_run;
	else
		document.getElementById("server1_Block_Running").innerHTML = state_srv_stop;

	if (server2pid > 0)
		document.getElementById("server2_Block_Running").innerHTML = state_srv_run;
	else
		document.getElementById("server2_Block_Running").innerHTML = state_srv_stop;

	for (var unit = 1; unit < (based_modelid == "RT-AC3200" ? 3 : 6); unit++) {
		switch (unit) {
			case 1:
				client_state = vpnc_state_t1;
				client_errno = vpnc_errno_t1;
				tmp = "<% nvram_get("vpn_client1_addr"); %>";
				client_server = " ("+ tmp.shorter(42) +
				                " <% nvram_get("vpn_client1_proto"); %>" +
				                ":<% nvram_get("vpn_client1_port"); %>)";
				client_desc = "<span style=\"background-color: transparent; color: white;\"><% nvram_get("vpn_client1_desc"); %></span>";
				break;
			case 2:
				client_state = vpnc_state_t2;
				client_errno = vpnc_errno_t2;
				tmp = "<% nvram_get("vpn_client2_addr"); %>";
				client_server = " ("+ tmp.shorter(42) + 
				                " <% nvram_get("vpn_client2_proto"); %>" +
				                ":<% nvram_get("vpn_client2_port"); %>)";
				client_desc = "<span style=\"background-color: transparent; color: white;\"><% nvram_get("vpn_client2_desc"); %></span>";
				break;
			case 3:
				client_state = vpnc_state_t3;
				client_errno = vpnc_errno_t3;
				tmp = "<% nvram_get("vpn_client3_addr"); %>";
				client_server = " ("+ tmp.shorter(42) + 
				                " <% nvram_get("vpn_client3_proto"); %>" +
				                ":<% nvram_get("vpn_client3_port"); %>)";
				client_desc = "<span style=\"background-color: transparent; color: white;\"><% nvram_get("vpn_client3_desc"); %></span>";
				break;
			case 4:
				client_state = vpnc_state_t4;
				client_errno = vpnc_errno_t4;
				tmp = "<% nvram_get("vpn_client4_addr"); %>";
				client_server = " ("+ tmp.shorter(42) + 
				                " <% nvram_get("vpn_client4_proto"); %>" +
				                ":<% nvram_get("vpn_client4_port"); %>)";
				client_desc = "<span style=\"background-color: transparent; color: white;\"><% nvram_get("vpn_client4_desc"); %></span>";
				break;
			case 5:
				client_state = vpnc_state_t5;
				client_errno = vpnc_errno_t5;
				tmp = "<% nvram_get("vpn_client5_addr"); %>";
				client_server = " ("+ tmp.shorter(42) + 
				                " <% nvram_get("vpn_client5_proto"); %>" +
				                ":<% nvram_get("vpn_client5_port"); %>)";
				client_desc = "<span style=\"background-color: transparent; color: white;\"><% nvram_get("vpn_client5_desc"); %></span>";
				break;
		}

		switch (client_state) {
			case "0":
				document.getElementById("client"+unit+"_Block_Running").innerHTML = client_desc + state_clnt_disc;
				break;
			case "1":
				document.getElementById("client"+unit+"_Block_Running").innerHTML = client_desc + state_clnt_cing + client_server;
				break;
			case "2":
				document.getElementById("client"+unit+"_Block_Running").innerHTML = client_desc + state_clnt_ced + client_server;
				break;
			case "-1":
				code = state_clnt_err;
				if (client_errno == 1 || client_errno == 2 || client_errno == 3)
					code += " - <#vpn_openvpn_conflict#>";
				else if(client_errno == 4 || client_errno == 5 || client_errno == 6)
					code += " - <#qis_fail_desc1#>";
				document.getElementById("client"+unit+"_Block_Running").innerHTML = client_desc + code;
				break;
		}
	}        

	parseStatus(vpn_server1_status, "server1_Block", "", "");
	parseStatus(vpn_server2_status, "server2_Block", "", "");
	parseStatus(vpn_client1_status, "client1_Block", vpn_client1_ip, vpn_client1_rip);
	parseStatus(vpn_client2_status, "client2_Block", vpn_client2_ip, vpn_client2_rip);
	if (based_modelid != "RT-AC3200") {
		parseStatus(vpn_client3_status, "client3_Block", vpn_client3_ip, vpn_client3_rip);
		parseStatus(vpn_client4_status, "client4_Block", vpn_client4_ip, vpn_client4_rip);
		parseStatus(vpn_client5_status, "client5_Block", vpn_client5_ip, vpn_client5_rip);
	}

	if (pptpd_support) {
		if (pptpdpid > 0)
			document.getElementById("pptp_Block_Running").innerHTML = state_srv_run;
		else
			document.getElementById("pptp_Block_Running").innerHTML = state_srv_stop;
		parsePPTPClients();

	} else {
		showhide("pptpserver", 0);
	}

	if ( (vpnc_support) && (vpnc_clientlist_array != "") ) {
		show_vpnc_rulelist();
	}

	setTimeout("refreshData()",2000);
}


function applyRule(){
	showLoading();
	document.form.submit();
}


function parsePPTPClients() {
	text = document.form.status_pptp.value;

	if (text == "") {
		return;
	}

	var lines = text.split('\n');

	code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table"><thead><tr><td colspan="4">Connected Clients</td></tr></thead><tr>';
	code += '<th style="text-align:left;">Username</th><th style="text-align:left;">Interface</th><th style="text-align:left;">Remote IP</th><th style="text-align:left;">Client IP</th>';

	for (i = 0; i < lines.length; ++i)
	{
		var done = false;

		var fields = lines[i].split(' ');
		if ( fields.length != 5 ) continue;

		code +='<tr><td style="text-align:left;">' + fields[4] + '</td><td style="text-align:left;">' + fields[1] + '</td><td style="text-align:left;">' + fields[2] + '</td><td style="text-align:left;">' + fields[3] +'</td></tr>';
	}
	code +='</table>';

	document.getElementById('pptp_Block').innerHTML = code;
}


function parseStatus(text, block, ipaddress, ripaddress){
	document.getElementById(block).innerHTML = "";
	var code = "";

	var lines = text.split('>');
	var staticStats = false;

	var routeTableEntries = new Array();
	var clientTableEntries = new Array();
	var statsTableEntries = new Array();
	var staticstatsTableEntries = new Array();

	var clientPtr = 0;
	var routePtr = 0;
	var statsPtr = 0;
	var staticstatsPtr = 0;

// Parse data
	for (i = 0; text != '' && i < lines.length; ++i)
	{
		var done = false;

		var fields = lines[i].split(',');
		if (fields.length == 0) continue;

		switch (fields[0])
		{
		case "TITLE":
			break;
                case "TIME":
                case "Updated":
			break;
		case "HEADER":
			switch (fields[1])
			{
			case "CLIENT_LIST":
				clientTableHeaders = fields.slice(2,fields.length-1);
				break;
			case "ROUTING_TABLE":
				routeTableHeaders = fields.slice(2,fields.length-1);
				break;
			default:
				break;
			}
			break;
		case "CLIENT_LIST":
			clientTableEntries[clientPtr++] = fields.slice(1,fields.length-1);
			break;
		case "ROUTING_TABLE":
			routeTableEntries[routePtr++] = fields.slice(1,fields.length-1);
			break;
//		case "GLOBAL_STATS":
//			statsTableEntries[statsPtr++] = fields.slice(1);
//			break;
		case "OpenVPN STATISTICS":
			staticStats = true;
			break;
		case "END":
			done = true;
			break;
		default:
			if (staticStats)
			{
				staticstatsTableEntries[staticstatsPtr++] = fields;
			}
			break;
		}
		if (done) break;
	}


/* Spit it out */

/*** Clients ***/

	if (clientPtr > 0) {
		code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table"><thead><tr><td colspan="6">Clients</td></tr></thead><tr>';

// Headers
		for (i = 0; i < (clientTableHeaders.length - 2); ++i)
		{
			switch (i) {
				case 0: // CN, merge with user
					code +='<th style="text-align:left;">' + clientTableHeaders[0] + '<br><span style="color: cyan; background: transparent;">' + clientTableHeaders[clientTableHeaders.length-2] + '</span></th>';
					break;
				case 2: // Virtual IPv4, merge with IPv6
					code +='<th style="text-align:left;">' + clientTableHeaders[2] + '<br><span style="color: cyan; background: transparent;">' + clientTableHeaders[3] + '</span></th>';
					break;
				case 3: // IPv6, merged with IPv4 field
				case 7: // Connected since time_t
					break;
				case 4: // dl/up amount
				case 5:
					code +='<th style="text-align:left;">' + clientTableHeaders[i].replace("Bytes","MBytes") + '</th>';
					break;
				default:
					code +='<th style="text-align:left;">' + clientTableHeaders[i] + '</th>';
			}
		}

		code += '</tr>';

// Clients
		for (i = 0; i < clientTableEntries.length; ++i)
		{
			code += '<tr>';
			for (j = 0; j < (clientTableEntries[i].length-2); ++j)
			{
				switch (j) {
					case 0:	// CN, merge with user
						if (clientTableEntries[i][8] == "UNDEF") {
							clientTableEntries[i][8] = "";
						}
						code += '<td style="vertical-align:top; white-space:nowrap; text-align:left;">' + clientTableEntries[i][0] + '<br><span style="color: cyan; background: transparent;">' + clientTableEntries[i][8] +'</span></td>';
						break;
					case 2:	// virtual IPv4, merge with IPv6
						code += '<td style="vertical-align:top; text-align:left;">' + clientTableEntries[i][2] + '<br><span style=""color: cyan; background: transparent;">' + clientTableEntries[i][3] +'</span></td>';
						break;
					case 3:	// IPv6, merged with IPv4 field
					case 7: // connected since time_t
						break;
					case 4:
					case 5: // dl/up amount
						code += '<td style="vertical-align:top; text-align:left;">' + Number(clientTableEntries[i][j]/1024/1024).toFixed(2).toLocaleString() + '</td>';
						break;
					default:
						code += '<td style="vertical-align:top; text-align:left;">' + clientTableEntries[i][j] + '</td>';
				}
			}
			code += '</tr>';
		}
		code += '</table><br>';
	}

	document.getElementById(block).innerHTML += code;


/*** Routes ***/

	if (routePtr > 0) {
		code = '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table"><thead><tr><td colspan="' + routeTableHeaders.length + '">Routes</td></tr></thead><tr>';

		// Headers
		for (i = 0; i < routeTableHeaders.length; ++i)
		{
			code +='<th style="text-align:left;">' + routeTableHeaders[i] + '</th>';
		}
		code += '</tr>';

		// Routes
		for (i = 0; i < routeTableEntries.length; ++i)
		{
			code += '<tr>';
			for (j = 0; j < routeTableEntries[i].length; ++j)
			{
				code += '<td style="white-space:nowrap; text-align:left;">' + routeTableEntries[i][j] + '</td>';
			}
			code += '</tr>';
		}
		code += '</table><br>';
	}

	document.getElementById(block).innerHTML += code;


	// Reset it, since we don't know which block we'll show next
	code = "";


/*** Stats ***/

	if (statsPtr > 0) {
		code += '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table"><thead><tr><td colspan="4">Statistics</td></tr></thead>';

		for (i = 0; i < statsTableEntries.length; ++i)
		{
			if (i % 2 == 0) code += '<tr>';
			code += '<th class="statcell">' + statsTableEntries[i][0] +'</th>';
			code += '<td class="statcell">' + Number(statsTableEntries[i][1]).toLocaleString() +'</td>';
			if (i % 2 == 1) code += '</tr>';
		}
		if (i % 2 == 0) code += '</tr>';
		code += '</table>';
	}


/*** Static Stats ***/

	if (staticstatsPtr > 0) {

		code += '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_table"><thead><tr><td colspan="4">Statistics</td></tr></thead>';

                if (ipaddress != "") {
			code += '<tr><th class="statcell">Public IP</th>';
			code += '<td class="statcell">' + ripaddress +'</td>';
			code += '<th class="statcell">Local IP</th>';
			code += '<td class="statcell">' + ipaddress +'</td></tr>';
                }

		for (i = 0; i < staticstatsTableEntries.length; ++i)
		{
			if (i % 2 == 0) code += '<tr>';
			code += '<th class="statcell">' + staticstatsTableEntries[i][0] +'</th>';
			code += '<td class="statcell">' + Number(staticstatsTableEntries[i][1]).toLocaleString() +'</td>';
			if (i % 2 == 1) code += '</tr>';
		}
		if (i % 2 == 0) code += '</tr>';
		code += '</table>';
	}
	document.getElementById(block).innerHTML += code;
}


function show_vpnc_rulelist(){
	if(vpnc_clientlist_array[0] == "<")
		vpnc_clientlist_array = vpnc_clientlist_array.split("<")[1];

	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
	var code = "";
	code +='<table style="margin-bottom:30px;" width="70%" border="1" align="left" cellpadding="4" cellspacing="0" class="list_table" id="vpnc_clientlist_table">';
	code +='<tr><th style="height:30px; width:20%;">Status</th>';
	code +='<th style="width:65%;"><div><#IPConnection_autofwDesc_itemname#></div></th>';
	code +='<th style="width:15%;"><div><#QIS_internet_vpn_type#></div></th></tr>';
	if(vpnc_clientlist_array == "")
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i=0; i<vpnc_clientlist_row.length; i++){
			overlib_str0[i] = "";
			overlib_str1[i] = "";
			code +='<tr id="row'+i+'">';

			var vpnc_clientlist_col = vpnc_clientlist_row[i].split('>');

			if(vpnc_clientlist_col[1] == document.form.vpnc_proto.value.toUpperCase() &&
				vpnc_clientlist_col[2] == document.form.vpnc_heartbeat_x.value &&
				vpnc_clientlist_col[3] == document.form.vpnc_pppoe_username.value)
			{
				if(vpnc_state_t == 0) // initial
					code +='<td><img src="/images/InternetScan.gif"></td>';
				else if(vpnc_state_t == 1) // disconnect
					code +='<td style="color: red;">Error!</td>';
				else // connected
					code +='<td><span>Connected<span></td>';
			}
			else
				code +='<td>Disconnected</td>';

			for(var j=0; j<vpnc_clientlist_col.length; j++){
				if(j == 0){
					if(vpnc_clientlist_col[0].length >32){
						overlib_str0[i] += vpnc_clientlist_col[0];
						vpnc_clientlist_col[0]=vpnc_clientlist_col[0].substring(0, 30)+"...";
						code +='<td style="text-align: left;"title="'+overlib_str0[i]+'">'+ vpnc_clientlist_col[0] +'</td>';
					}else{
						code +='<td style="text-align: left;">'+ vpnc_clientlist_col[0] +'</td>';
					}
				}
				else if(j == 1){
					code += '<td>'+ vpnc_clientlist_col[1] +'</td>';
				}
			}

		}
	}
	code +='</table>';

	document.getElementById("vpnc_clientlist_Block").innerHTML = code;
}


</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_VPNStatus.asp">
<input type="hidden" name="next_page" value="Advanced_VPNStatus.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="0">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="status_pptp" value="<% nvram_dump("pptp_connected",""); %>">
<input type="hidden" name="vpnc_proto" value="<% nvram_get("vpnc_proto"); %>">
<input type="hidden" name="vpnc_pppoe_username" value="<% nvram_get("vpnc_pppoe_username"); %>">
<input type="hidden" name="vpnc_heartbeat_x" value="<% nvram_get("vpnc_heartbeat_x"); %>">

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
                <div class="formfonttitle">VPN - Status</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" id="pptpserver" class="FormTable"></br>
					<thead>
						<tr>
							<td>PPTP VPN Server<span id="pptp_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="pptp_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="server1" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN Server 1<span id="server1_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="server1_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="server2" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN Server 2<span id="server2_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="server2_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="client1" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN <span id="client1_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="client1_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="client2" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN <span id="client2_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="client2_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="client3" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN <span id="client3_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="client3_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="client4" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN <span id="client4_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="client4_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="client5" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
					<thead>
						<tr>
							<td>OpenVPN <span id="client5_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="client5_Block"></div>
						</td>
					</tr>

				</table>
				<br>
				<table width="100%" id="vpnc" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td>PPTP/L2TP Clients<span id="vpnc_Block_Running" style="background: transparent;"></span></td>
						</tr>
					</thead>
					<tr>
						<td style="border: none;">
							<div id="vpnc_clientlist_Block"></div>
						</td>
					</tr>

				</table>

				<div class="apply_gen">
					<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_refresh#>"/>
				</div>
			  </td></tr>
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

