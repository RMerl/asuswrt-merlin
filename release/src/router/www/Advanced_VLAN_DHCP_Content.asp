<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - VLAN DHCP</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/JavaScript" src="/jquery.js"></script>
<script>
<% wanlink(); %>
var $j = jQuery.noConflict();
var alert_over = "<#vlaue_haigher_than#> ";
var subnet_rulelist_array = decodeURIComponent("<% nvram_char_to_ascii("","subnet_rulelist"); %>");
var subnet_rulelist_row = subnet_rulelist_array.split('<');

function initial() {
	show_menu();

	switchSubnetValue("LAN1");

	if(sfexp_support) {
		document.getElementById("tbDNSWINS").style.display = "";
	}
}

// test if Private ip
function valid_IP(obj_name) {
	// A : 1.0.0.0~126.255.255.255
	// B : 127.0.0.0~127.255.255.255 (forbidden)
	// C : 128.0.0.0~255.255.255.254
	var A_class_start = inet_network("1.0.0.0");
	var A_class_end = inet_network("126.255.255.255");
	var B_class_start = inet_network("127.0.0.0");
	var B_class_end = inet_network("127.255.255.255");
	var C_class_start = inet_network("128.0.0.0");
	var C_class_end = inet_network("255.255.255.255");
		
	var ip_obj = obj_name;
	var ip_num = inet_network(ip_obj.value);	//-1 means nothing
		
	//1~254
	if(obj_name.value.split(".")[3] < 1 || obj_name.value.split(".")[3] > 254) {
		alert(obj_name.value+" <#JS_validip#>");
		obj_name.focus();
		return false;
	}
			
	if(ip_num > A_class_start && ip_num < A_class_end) {
		obj_name.value = ipFilterZero(ip_obj.value); //Filtering ip address with leading zero
		return true;
	}
	else if(ip_num > B_class_start && ip_num < B_class_end) {
		alert(ip_obj.value+" <#JS_validip#>");
		ip_obj.focus();
		ip_obj.select();
		return false;
	}
	else if(ip_num > C_class_start && ip_num < C_class_end) {
		obj_name.value = ipFilterZero(ip_obj.value); //Filtering ip address with leading zero
		return true;
	}
	else {
		alert(ip_obj.value+" <#JS_validip#>");
		ip_obj.focus();
		ip_obj.select();
		return false;
	}
}

function validForm() {
	if(!valid_IP(document.form.tGatewayIP)) {
		document.form.tGatewayIP.focus();
		document.form.tGatewayIP.select();
		return false;		
	}

	if(document.form.tSubnetMask.value == "") {
		alert("<#JS_fieldblank#>");
		document.form.tSubnetMask.focus();
		document.form.tSubnetMask.select();
		return false;
	}
	else if(!validMask(document.form.tSubnetMask)) {
		return false;
	}

	if(!checkGatewayIP()) {
		document.form.tGatewayIP.focus();
		document.form.tGatewayIP.select();
		return false;
	}

	if(document.form.tDHCPStart.value == "") {
		alert("<#JS_fieldblank#>");
		document.form.tDHCPStart.focus();
		document.form.tDHCPStart.select();
		return false;
	}
	else if(document.form.tDHCPEnd.value == "") {
		alert("<#JS_fieldblank#>");
		document.form.tDHCPEnd.focus();
		document.form.tDHCPEnd.select();
		return false;
	}
	else {
		//1.check IP whether is legal or not
		if(!valid_IP(document.form.tDHCPStart)) {
			document.form.tDHCPStart.focus();
			document.form.tDHCPStart.select();
			return false;		
		}
		else if(!valid_IP(document.form.tDHCPEnd)) {
			document.form.tDHCPEnd.focus();
			document.form.tDHCPEnd.select();
			return false;		
		}

		//2.check IP pool range is legal
		var ipGateway = inet_network(document.form.tGatewayIP.value);
		var ipPoolStart = inet_network(document.form.tDHCPStart.value);
		var ipPoolEnd = inet_network(document.form.tDHCPEnd.value);
		var ipPoolRangeArray = calculatorIPPoolRange().split(">");
		var ipPoolLegalStart = inet_network(ipPoolRangeArray[0]);
		var ipPoolLegalEnd = inet_network(ipPoolRangeArray[1]);
		if(ipPoolLegalStart > ipPoolStart || ipPoolLegalEnd < ipPoolStart) {
			alert(document.form.tDHCPStart.value + " <#JS_validip#>");
			document.form.tDHCPStart.focus();
			document.form.tDHCPStart.select();
			return false;
		}
		else if(ipPoolLegalEnd < ipPoolEnd || ipPoolLegalStart > ipPoolEnd) {
			alert(document.form.tDHCPEnd.value + " <#JS_validip#>");
			document.form.tDHCPEnd.focus();
			document.form.tDHCPEnd.select();
			return false;
		}

		//3.check whether the End IP > Start IP or not
		if(ipPoolStart > ipPoolEnd) {
			alert(alert_over + document.form.tDHCPStart.value);
			document.form.tDHCPEnd.focus();
			document.form.tDHCPEnd.select();
			return false;
		}
		
		//4.check IP pool start/end whether conflic gateway IP
		if(ipPoolStart === ipGateway) {
			alert("Conflict with Gateway IP " + document.form.tGatewayIP.value);
			document.form.tDHCPStart.focus();
			document.form.tDHCPStart.select();
			return false;
		}
		else if(ipPoolEnd === ipGateway) {
			alert("Conflict with Gateway IP " + document.form.tGatewayIP.value);
			document.form.tDHCPEnd.focus();
			document.form.tDHCPEnd.select();
			return false;
		}	
	}

	if(!validate_range(document.form.tLeaseTime, 120, 604800)) {
		return false;
	}

	if(sfexp_support) {
		if(!validate_ipaddr_final(document.form.vlan_dns1, 'vlan_dns1') || 
			!validate_ipaddr_final(document.form.vlan_dns2, 'vlan_dns2') || 
			!validate_ipaddr_final(document.form.vlan_wins, 'vlan_wins')) 
			return false;
	}

	return true;
}

function handleSubnetRulelist() {
	var editSubnetRulelist = function () {
		var radioDHCPValue = "1";
		if(document.form.radioDHCPEnable[1].checked) {
			radioDHCPValue = document.form.radioDHCPEnable[1].value;
		}

		subnet_rulelist_array += "<";
		subnet_rulelist_array += document.form.selSubnet.value;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += document.form.tGatewayIP.value;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += document.form.tSubnetMask.value;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += radioDHCPValue;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += document.form.tDHCPStart.value;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += document.form.tDHCPEnd.value;
		subnet_rulelist_array += ">";
		subnet_rulelist_array += document.form.tLeaseTime.value;
		if(sfexp_support) {
			subnet_rulelist_array += ">";
			subnet_rulelist_array += document.form.vlan_dns1.value;
			subnet_rulelist_array += ">";
			subnet_rulelist_array += document.form.vlan_dns2.value;
			subnet_rulelist_array += ">";
			subnet_rulelist_array += document.form.vlan_wins.value;
		}
	};

	if(subnet_rulelist_array.search(document.form.selSubnet.value) !== -1) {
		subnet_rulelist_array = "";
		for(var i = 1; i < subnet_rulelist_row.length; i +=1 ) {
			var subnet_rulelist_col = subnet_rulelist_row[i].split('>');
			if(subnet_rulelist_col[0] == document.form.selSubnet.value) {
				editSubnetRulelist();
			}
			else {
				subnet_rulelist_array += "<";
				subnet_rulelist_array += subnet_rulelist_row[i];
			}
		}
	}
	else {
		editSubnetRulelist();
	}
}

function applyRule() {
	if(validForm()) {
		showLoading();
		handleSubnetRulelist();

		var bRestartFlag = false;
		var vlan_rulelist_array = decodeURIComponent("<% nvram_char_to_ascii("","vlan_rulelist"); %>");
		var vlan_rulelist_row = vlan_rulelist_array.split('<');
		for(var i = 1; i < vlan_rulelist_row.length; i +=1 ) {
			var vlan_rulelist_col = vlan_rulelist_row[i].split('>');
			if(vlan_rulelist_col[4].search(document.form.selSubnet.value) !== -1) { //if vlan_rulelist not setted Subnet, just saving nvram only.
				bRestartFlag = true;
			}
		}

		if(bRestartFlag) {
			document.form.action_script.value = "restart_net_and_phy";
			document.form.action_wait.value = 30;
		}
		else {
			document.form.action_script.value = "";
			document.form.action_wait.value = 3;
		}

		document.form.subnet_rulelist.value = subnet_rulelist_array;
		document.form.submit();
	}
}

function switchSubnetValue(selectSubnet) {
	document.form.tGatewayIP.value = "";
	document.form.radioDHCPEnable[0].checked = true;
	document.form.tSubnetMask.value = "";
	document.form.tDHCPStart.value = "";
	document.form.tDHCPEnd.value = "";
	document.form.tLeaseTime.value = "86400";
	if(sfexp_support) {
		document.form.vlan_dns1.value = "";
		document.form.vlan_dns2.value = "";
		document.form.vlan_wins.value = "";
	}

	for(var i = 1; i < 9; i +=1 ) {
		if(subnet_rulelist_row[i] != undefined) {
			var subnet_rulelist_col = subnet_rulelist_row[i].split('>');
			if(subnet_rulelist_col[0] != undefined && selectSubnet == subnet_rulelist_col[0]) {
				document.form.tGatewayIP.value = subnet_rulelist_col[1];
				if (subnet_rulelist_col[3] === "0") {
					document.form.radioDHCPEnable[1].checked = true;
				}
				document.form.tSubnetMask.value = subnet_rulelist_col[2];
				document.form.tDHCPStart.value = subnet_rulelist_col[4];
				document.form.tDHCPEnd.value = subnet_rulelist_col[5];
				document.form.tLeaseTime.value = subnet_rulelist_col[6];
				if(subnet_rulelist_col[7] != undefined && sfexp_support) {
					document.form.vlan_dns1.value = subnet_rulelist_col[7];
					document.form.vlan_dns2.value = subnet_rulelist_col[8];
					document.form.vlan_wins.value = subnet_rulelist_col[9];
				}
			}
		}
	}
}

function checkGatewayIP() {
	var lanIPAddr = document.form.tGatewayIP.value;
	var lanNetMask = document.form.tSubnetMask.value;
	var ipConflict;
	var alertMsg = function (type, ipAddr, netStart, netEnd) {
		alert("*Conflict with " + type + " IP: " + ipAddr + ",\n" + "Network segment is " + netStart + " ~ " + netEnd);
	};

	//1.check Wan IP
	ipConflict = checkIPConflict("WAN", lanIPAddr, lanNetMask);
	if(ipConflict.state) {
		alertMsg("WAN", ipConflict.ipAddr, ipConflict.netLegalRangeStart, ipConflict.netLegalRangeEnd);
		return false;
	}

	//2.check Lan IP
	ipConflict = checkIPConflict("LAN", lanIPAddr, lanNetMask);
	if(ipConflict.state) {
		alertMsg("LAN", ipConflict.ipAddr, ipConflict.netLegalRangeStart, ipConflict.netLegalRangeEnd);
		return false;
	}

	//3.check PPTP
	if(pptpd_support) {
		ipConflict = checkIPConflict("PPTP", lanIPAddr, lanNetMask);
		if(ipConflict.state) {
			alertMsg("PPTP", ipConflict.ipAddr, ipConflict.netLegalRangeStart, ipConflict.netLegalRangeEnd);
			return false;
		}
	}

	//4.check OpenVPN
	if(openvpnd_support) {
		ipConflict = checkIPConflict("OpenVPN", lanIPAddr, lanNetMask);
		if(ipConflict.state) {
			alertMsg("OpenVPN", ipConflict.ipAddr, ipConflict.netLegalRangeStart, ipConflict.netLegalRangeEnd);
			return false;
		}
	}

	//5.check VLAN1~8
	var subnet_rulelist_row_num = subnet_rulelist_row.length;
	for(var VLANIndex = 1; VLANIndex < subnet_rulelist_row_num; VLANIndex += 1) {
		if(subnet_rulelist_row[VLANIndex].search(document.form.selSubnet.value) === -1) {
			var subnet_rulelist_col = subnet_rulelist_row[VLANIndex].split('>');
			ipConflict = checkIPConflict("VLAN" + VLANIndex, lanIPAddr, lanNetMask);
			if(ipConflict.state) {			
				alertMsg("V" + subnet_rulelist_col[0], ipConflict.ipAddr, ipConflict.netLegalRangeStart, ipConflict.netLegalRangeEnd);
				return false;
			}
		}
	}

	return true;
}

function checkIPLegality() {
	//check IP legal
	if(document.form.tGatewayIP.value !== "") {
		if(!valid_IP(document.form.tGatewayIP)) {
			document.form.tGatewayIP.focus();
			document.form.tGatewayIP.select();
			return false;		
		}
	}

	//setting IP pool range
	if(document.form.tGatewayIP.value !== "" && document.form.tSubnetMask.value !== "") {
		var ipPoolRangeArray = calculatorIPPoolRange().split(">");
		document.form.tDHCPStart.value = ipPoolRangeArray[0];
		document.form.tDHCPEnd.value = ipPoolRangeArray[1];
	}
}

function validMask(obj_name) {
	var wrong_netmask = 0;
	var netmask_obj = obj_name;
	var netmask_num = inet_network(netmask_obj.value);
	if(netmask_num !== -1) {
		if(netmask_num === 0) {
			var netmask_reverse_num = 0;
		}
		else {
			var netmask_reverse_num = ~netmask_num;
		}
			
		if(netmask_num < 0) wrong_netmask = 1;

		var test_num = netmask_reverse_num;
		while(test_num !== 0){
			if((test_num + 1) % 2 === 0)
				test_num = (test_num + 1) / 2 - 1;
			else{
				wrong_netmask = 1;
				break;
			}
		}
		if(wrong_netmask === 1){
			alert(netmask_obj.value+" is not a valid Mask address!");
			netmask_obj.focus();
			netmask_obj.select();
			return false;
		}

		//mask must less than 255.255.255.252
		var maskMaximum = inet_network("255.255.255.252");
		if(maskMaximum < netmask_num) {
			alert("Netmask must less than 255.255.255.252");
			netmask_obj.focus();
			netmask_obj.select();
			return false;
		}

	}
	return true;
}
function checkMaskLegality() {
	//check IP legal
	if(document.form.tSubnetMask.value !== "") {
		if(!validMask(document.form.tSubnetMask)) {
			return false;		
		}
	}
	
	//setting IP pool range
	if(document.form.tGatewayIP.value !== "" && document.form.tSubnetMask.value !== "") {
		var ipPoolRangeArray = calculatorIPPoolRange().split(">");
		document.form.tDHCPStart.value = ipPoolRangeArray[0];
		document.form.tDHCPEnd.value = ipPoolRangeArray[1];
	}
}

function calculatorIPPoolRange() {
	var gatewayIPArray = document.form.tGatewayIP.value.split(".");
	var netMaskArray = document.form.tSubnetMask.value.split(".");
	var ipPoolStartArray  = new Array();
	var ipPoolEndArray  = new Array();
	var ipPoolStart = "";
	var ipPoolEnd = "";

	ipPoolStartArray[0] = (gatewayIPArray[0] & 0xFF) & (netMaskArray[0] & 0xFF); 
	ipPoolStartArray[1] = (gatewayIPArray[1] & 0xFF) & (netMaskArray[1] & 0xFF); 
	ipPoolStartArray[2] = (gatewayIPArray[2] & 0xFF) & (netMaskArray[2] & 0xFF); 
	ipPoolStartArray[3] = (gatewayIPArray[3] & 0xFF) & (netMaskArray[3] & 0xFF);
	ipPoolStartArray[3] += 1;

	ipPoolEndArray[0] = (gatewayIPArray[0] & 0xFF) | (~netMaskArray[0] & 0xFF); 
	ipPoolEndArray[1] = (gatewayIPArray[1] & 0xFF) | (~netMaskArray[1] & 0xFF); 
	ipPoolEndArray[2] = (gatewayIPArray[2] & 0xFF) | (~netMaskArray[2] & 0xFF); 
	ipPoolEndArray[3] = (gatewayIPArray[3] & 0xFF) | (~netMaskArray[3] & 0xFF); 
	ipPoolEndArray[3] -= 1;

	ipPoolStart = ipPoolStartArray[0] + "." + ipPoolStartArray[1] + "." + ipPoolStartArray[2] + "." + ipPoolStartArray[3];
	if(inet_network(ipPoolStart) === inet_network(document.form.tGatewayIP.value)) {
		ipPoolStart = ipPoolStartArray[0] + "." + ipPoolStartArray[1] + "." + ipPoolStartArray[2] + "." + (parseInt(ipPoolStartArray[3]) + 1);
	}
	ipPoolEnd = ipPoolEndArray[0] + "." + ipPoolEndArray[1] + "." + ipPoolEndArray[2] + "." + ipPoolEndArray[3];

	return ipPoolStart + ">" + ipPoolEnd;
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_VLAN_DHCP_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_net_and_phy">
<input type="hidden" name="action_wait" value="30">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="subnet_rulelist" value="<% nvram_char_to_ascii("","subnet_rulelist"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
			<!--===================================Beginning of Main Content===========================================-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td align="left" valign="top">
						<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">DHCP setting</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">Select the LAN you want to setup and fill out gateway information for IP assignment if you enable DHCP server.</div>
									<table id="tbVLANGroup" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
									<thead>
										<tr>
											<td colspan="2">Configuration</td>
										</tr>
									</thead>
										<tr>
											<th>Subnet</th>
											<td>
												<select name="selSubnet" class="input_option" onchange="switchSubnetValue(this.value)">
													<option value="LAN1">LAN1</option>
													<option value="LAN2">LAN2</option>
													<option value="LAN3">LAN3</option>
													<option value="LAN4">LAN4</option>
													<option value="LAN5">LAN5</option>
													<option value="LAN6">LAN6</option>
													<option value="LAN7">LAN7</option>
													<option value="LAN8">LAN8</option>
												</select>
												&nbsp;Please choice which LAN you want to set.
											</td>
										</tr>
										<tr>
											<th>Enable the DHCP Server</th>
											<td>
												<input type="radio" value="1" name="radioDHCPEnable" class="content_input_fd" checked><#checkbox_Yes#>
				  								<input type="radio" value="0" name="radioDHCPEnable" class="content_input_fd"><#checkbox_No#>
											</td>
										</tr>
										<tr>
											<th>Gateway IP Address</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="tGatewayIP" onBlur="checkIPLegality();" onKeyPress="return is_ipaddr(this,event);">
											</td>
										</tr>
										<tr>
										<tr>
											<th>Subnet Mask</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="tSubnetMask" onBlur="checkMaskLegality();" onKeyPress="return is_ipaddr(this,event);">
											</td>
										</tr>
										<tr>
											<th>IP Pool Starting Address</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="tDHCPStart"  onKeyPress="return is_ipaddr(this,event);">
											</td>
										</tr>
										<tr>
											<th>IP Pool Ending Address</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="tDHCPEnd"  onKeyPress="return is_ipaddr(this,event);">
												</div>
											</td>
										</tr>
										<tr>
											<th>Lease Time</th>
											<td>
												<input type="text" maxlength="6" class="input_25_table" name="tLeaseTime" onKeyPress="return is_number(this,event);">
											</td>
										</tr>
									</table>
									<table id="tbDNSWINS" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="margin-top:8px;display:none;">
										<thead>
										<tr>
											<td colspan="2">DNS and WINS Server Setting (Optional)</td>
										</tr>
										</thead>	
										<tr>
											<th>DNS Server 1</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="vlan_dns1" onKeyPress="return is_ipaddr(this,event)">
											</td>
										</tr>
										<tr>
											<th>DNS Server 2</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="vlan_dns2" onKeyPress="return is_ipaddr(this,event)">
											</td>
										</tr>
										<tr>
											<th>WINS Server</th>
											<td>
												<input type="text" maxlength="15" class="input_25_table" name="vlan_wins"  onkeypress="return is_ipaddr(this,event)"/>
											</td>
										</tr>
									</table>
									<div class="apply_gen">
										<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
									</div>
								</td>
							</tr>
						</tbody>	
						</table>					
					</td>	
				</tr>
			</table>				
			<!--===================================End of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>	
<div id="footer"></div>
</body>
</html>

