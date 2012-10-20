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
<title><#Web_Title#> - <#menu5_3_1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>

<% login_state_hook(); %>
<% dsl_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var dsl_pvc_enabled = ["0", "0", "0", "0", "0", "0", "0", "0"];

var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
<% wan_get_parameter(); %>

if(dualWAN_support != -1){
	var wan_type_name = wans_dualwan.split(" ")[<% nvram_get("wan_unit"); %>];
	wan_type_name = wan_type_name.toUpperCase();
	switch(wan_type_name){
		case "WAN":
			break;
		case "LAN":
			location.href = "Advanced_WAN_Content.asp";
			break;
		case "USB":
			location.href = "Advanced_Modem_Content.asp";
			break;
		default:
			break;
	}
}

// get WAN setting list from hook.
// Index, VPI, VCI, Protocol
var DSLWANList = [ <% get_DSL_WAN_list(); %> ];

function chg_pvc(pvc_to_chg) {
	var iptv_row = 0;
	for(var i = 1; i < DSLWANList.length; i++){
		if (DSLWANList[i][0] != "0") {
			iptv_row++;
			if (pvc_to_chg == i) break;
		}
	}
	reset_item_select();

	if (pvc_to_chg != "0") {
		remove_item_from_select_bridge();
	}
	else
	{
		if (DSLWANList[0][3] == "bridge")
			remove_item_from_select_bridge();
	}
	disable_pvc_summary();
	enable_all_ctrl();
	change_dsl_unit_idx(pvc_to_chg,iptv_row);
	change_dsl_type(document.form.dsl_proto.value);
	fixed_change_dsl_type(document.form.dsl_proto.value);
}

function reset_item_select() {
	if (document.form.dsl_proto.options.length == 1)
	{
			var var_item;
			document.form.dsl_proto.options[0] = null;
			var_item = new Option("PPPoE", "pppoe");
			document.form.dsl_proto.options.add(var_item);
			var_item = new Option("PPPoA", "pppoa");
			document.form.dsl_proto.options.add(var_item);
			var_item = new Option("IPoA", "ipoa");
			document.form.dsl_proto.options.add(var_item);
			var_item = new Option("MER", "mer");
			document.form.dsl_proto.options.add(var_item);
			var_item = new Option("Bridge", "bridge");
			document.form.dsl_proto.options.add(var_item);
	}
}

function remove_item_from_select_bridge() {
	if (document.form.dsl_proto.options.length == 5)
	{
		document.form.dsl_proto.options[0] = null;
		document.form.dsl_proto.options[0] = null;
		document.form.dsl_proto.options[0] = null;
		document.form.dsl_proto.options[0] = null;
	}
}

function add_pvc() {
	var iptv_row = 0;
	for(var i = 1; i < DSLWANList.length; i++){
		if (DSLWANList[i][0] != "0") {
			iptv_row++;
		}
	}
	disable_pvc_summary();
	enable_all_ctrl();
// find a available VCI
	var avail_vci = 33;
	for(var i = 33; i < 100; i++) {
		var found = false;
		for(var j = 0; j < DSLWANList.length; j++){
			if (dsl_pvc_enabled[j] != "0") {
				if (i == DSLWANList[j][2]) {
					found = true;
					break;
				}
			}
		}
		if (found == false) {
			avail_vci = i;
			break;
		}
	}
// find a available PVC
	var avail_pvc = 7;
	var found_pvc = false;
	for(var j = 0; j < DSLWANList.length; j++){
		if (dsl_pvc_enabled[j] == "0") {
			found_pvc = true;
			avail_pvc = j;
			break;
		}
	}
	if (found_pvc == false) {
// no empty pvc , return
		return;
	}

	document.form.dsl_unit.value=avail_pvc.toString();
	document.form.dsl_enable.value="1";
	document.form.dsl_vpi.value="0";
	document.form.dsl_vci.value=avail_vci.toString();
	document.form.dsl_encap.value="0";
	document.form.dsl_svc_cat.value="0";
	document.form.dsl_pcr.value="0";
	document.form.dsl_scr.value="0";
	document.form.dsl_mbs.value="0";

	if (avail_pvc == 0) {
		document.form.dsl_proto.value="pppoe";
		document.form.dslx_nat.value="1";
		document.form.dslx_upnp_enable.value="1";
		document.form.dslx_link_enable.value="1";
		document.form.dslx_DHCPClient.value="1";
		document.form.dslx_ipaddr.value="0.0.0.0";
		document.form.dslx_netmask.value="0.0.0.0";
		document.form.dslx_gateway.value="0.0.0.0";
		document.form.dslx_pppoe_username.value="";
		document.form.dslx_pppoe_passwd.value="";
		document.form.dslx_pppoe_idletime.value="0";
		document.form.dslx_pppoe_mtu.value="1492";
		document.form.dslx_pppoe_service.value="";
		document.form.dslx_pppoe_ac.value="";
		document.form.dslx_pppoe_options.value="";
		document.form.dslx_hwaddr.value="";
		$("pvc_sel").innerHTML = "Internet PVC";
	}
	else {
		document.form.dsl_proto.value="bridge";
		$("pvc_sel").innerHTML = "IPTV PVC #"+(iptv_row+1).toString();
	}
	reset_item_select();
	if (avail_pvc != 0) {
		remove_item_from_select_bridge();
	}

	change_svc_cat("0");
	change_dsl_dhcp_enable();
	change_dsl_dns_enable();
	$("dslSettings").style.display = "";
	if (document.form.dsl_proto.value != "bridge") {
		$("t2BC").style.display = "";
		$("IPsetting").style.display = "";
		$("DNSsetting").style.display = "";
		$("PPPsetting").style.display = "";
		$("vpn_server").style.display = "";
	}
	else {
		$("t2BC").style.display = "none";
		$("IPsetting").style.display = "none";
		$("DNSsetting").style.display = "none";
		$("PPPsetting").style.display = "none";
		$("vpn_server").style.display = "none";
	}

	change_dsl_type(document.form.dsl_proto.value);
	fixed_change_dsl_type(document.form.dsl_proto.value);
}

function del_pvc(pvc_to_del) {
	var msg = "";
	if (pvc_to_del == "0") msg += "Internet PVC, "
	else msg += "IPTV PVC, ";
	msg += "VPI = "+DSLWANList[pvc_to_del][1].toString()+", VCI = "+DSLWANList[pvc_to_del][2].toString();
	msg += "\n\n";
	msg += "<#pvc_del_alert_desc#>";
	var answer = confirm (msg);
	if (answer == false) return;
	document.form.dsl_unit.value=pvc_to_del.toString();
	document.form.dsl_enable.value="0";
	del_pvc_submit();
}

function showDSLWANList(){
	var addRow;
	var cell = new Array(10);
	var config_num = 0;
	for(var i = 0; i < DSLWANList.length; i++){
		if (DSLWANList[i][0] != "0") {
			config_num++;
		}
	}
	if(config_num == 0){
		addRow = document.getElementById('DSL_WAN_table').insertRow(2);
		for (var i = 0; i <= 7; i++) {
			cell[i] = addRow.insertCell(i);
			if (i==3) cell[i].innerHTML = "no data";
			else cell[i].innerHTML = "&nbsp";
			cell[i].style.color = "white";
		}
		cell[8] = addRow.insertCell(8);
		cell[8].innerHTML = '<center><input class="add_btn" onclick="add_pvc();" value=""/></center>';
		cell[8].style.color = "white";
	}
	else{
		var row_count=0;
		for(var i = 0; i < DSLWANList.length; i++){
			if (DSLWANList[i][0] != "0") {
				addRow = document.getElementById('DSL_WAN_table').insertRow(row_count+2);
				row_count++;
				cell[0] = addRow.insertCell(0);
				cell[0].innerHTML = row_count.toString();
				cell[0].style.color = "white";
				for (var j = 1; j <= 2; j++) {
					cell[j] = addRow.insertCell(j);
					cell[j].innerHTML = DSLWANList[i][j];
					cell[j].style.color = "white";
				}
				cell[3] = addRow.insertCell(3);
				if (DSLWANList[i][3]=="pppoe") cell[3].innerHTML = "PPPoE";
				else if (DSLWANList[i][3]=="pppoa") cell[3].innerHTML = "PPPoA";
				else if (DSLWANList[i][3]=="mer") cell[3].innerHTML = "MER";
				else if (DSLWANList[i][3]=="bridge") cell[3].innerHTML = "Bridge";
				else if (DSLWANList[i][3]=="ipoa") cell[3].innerHTML = "IPoA";
				else cell[3].innerHTML = "Unknown";
				cell[3].style.color = "white";
				cell[4] = addRow.insertCell(4);
				if (DSLWANList[i][4]=="0") {
					cell[4].innerHTML = "LLC";
				}
				else {
					cell[4].innerHTML = "VC";
				}
				cell[4].style.color = "white";
				cell[5] = addRow.insertCell(5);
				if (i==0) {
					cell[5].innerHTML = "<center><img src=images/checked.gif border=0></center>";
				}
				else {
					cell[5].innerHTML = "";
				}
				cell[5].style.color = "white";
				cell[6] = addRow.insertCell(6);
				if (i!=0) {
					cell[6].innerHTML = "<center><img src=images/checked.gif border=0></center>";
				}
				else {
					cell[6].innerHTML = "";
				}
				cell[6].style.color = "white";
				cell[7] = addRow.insertCell(7);
				cell[7].innerHTML = '<center><span style="cursor:pointer;" onclick="chg_pvc('+i.toString()+')"><img src="/images/New_ui/accountedit.png"></span></center>';
				cell[7].style.color = "white";
				cell[8] = addRow.insertCell(8);
				cell[8].innerHTML = '<center><input class="remove_btn" onclick="del_pvc('+i.toString()+');" value=""/></center>';
				cell[8].style.color = "white";
			}
		}
		if (row_count <= 7) {
			addRow = document.getElementById('DSL_WAN_table').insertRow(row_count+2);
			for (var i = 0; i <= 7; i++) {
				cell[i] = addRow.insertCell(i);
				cell[i].innerHTML = "&nbsp";
				cell[i].style.color = "white";
			}
			cell[8] = addRow.insertCell(8);
			cell[8].innerHTML = '<center><input class="add_btn" onclick="add_pvc();" value=""/></center>';
			cell[8].style.color = "white";
		}
	}
}

function initial(){
	show_menu();
	showDSLWANList();
	// PVC init
	for(var i = 0; i < DSLWANList.length; i++){
		if (DSLWANList[i][0] != "0") dsl_pvc_enabled[i] = "1";
		if (dsl_pvc_enabled[i] == "0") {
			DSLWANList[i][1] = "0"
			var vci_num = 35 + i;
			DSLWANList[i][2] = vci_num.toString();
			if (i==0) DSLWANList[i][3] = "pppoe";
			else DSLWANList[i][3] = "bridge";
			DSLWANList[i][4] = "0";
			DSLWANList[i][5] = "0";
			DSLWANList[i][6] = "0";
			DSLWANList[i][7] = "0";
			DSLWANList[i][8] = "0";
		}
	}
	disable_all_ctrl();

	// WAN port
	genWANSoption();
	change_wan_unit();
	if(dualWAN_support == -1) {
		$("WANscap").style.display = "none";
	}
}

function change_wan_unit(){
	if(dualWAN_support == -1) return;
	if(document.form.wan_unit.options[document.form.wan_unit.selectedIndex].text == "LAN") {
		document.form.current_page.value = "Advanced_WAN_Content.asp";
	}
	else if(document.form.wan_unit.options[document.form.wan_unit.selectedIndex].text == "USB") {
		document.form.current_page.value = "Advanced_Modem_Content.asp";
	}
	else {
		return false;
	}

	FormActions("apply.cgi", "change_wan_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function genWANSoption(){
	if(dualWAN_support == -1) return;
	for(i=0; i<wans_dualwan.split(" ").length; i++)
		document.form.wan_unit.options[i] = new Option(wans_dualwan.split(" ")[i].toUpperCase(), i);
	document.form.wan_unit.selectedIndex = '<% nvram_get("wan_unit"); %>';

	if(wans_dualwan.search(" ") < 0 || wans_dualwan.split(" ")[1] == 'none')
		$("WANscap").style.display = "none";
}

function change_dsl_unit_idx(idx,iptv_row){
	// reset to old values
	if (idx == "0") $("pvc_sel").innerHTML = "Internet PVC";
	else $("pvc_sel").innerHTML = "IPTV PVC #"+iptv_row.toString();
	document.form.dsl_unit.value=idx.toString();
	document.form.dsl_enable.value="1";
	document.form.dsl_vpi.value=DSLWANList[idx][1];
	document.form.dsl_vci.value=DSLWANList[idx][2];
	document.form.dsl_proto.value=DSLWANList[idx][3];
	document.form.dsl_encap.value=DSLWANList[idx][4];
	document.form.dsl_svc_cat.value=DSLWANList[idx][5];
	document.form.dsl_pcr.value=DSLWANList[idx][6];
	document.form.dsl_scr.value=DSLWANList[idx][7];
	document.form.dsl_mbs.value=DSLWANList[idx][8];

	if (document.form.dsl_encap.value == "0") document.form.dsl_encap.selectedIndex = 0;
	else document.form.dsl_encap.selectedIndex = 1;
	change_svc_cat_current(document.form.dsl_svc_cat.value);
	change_dsl_dhcp_enable();
	change_dsl_dns_enable();
	$("dslSettings").style.display = "";
	if (document.form.dsl_proto.value != "bridge") {
		$("t2BC").style.display = "";
		$("IPsetting").style.display = "";
		$("DNSsetting").style.display = "";
		$("PPPsetting").style.display = "";
		$("vpn_server").style.display = "";
	}
	else {
		$("t2BC").style.display = "none";
		$("IPsetting").style.display = "none";
		$("DNSsetting").style.display = "none";
		$("PPPsetting").style.display = "none";
		$("vpn_server").style.display = "none";
	}
}

function del_pvc_submit(){
	showLoading();
	document.form.submit();
}

function exit_to_main(){
	enable_pvc_summary();
	disable_all_ctrl();
}

function applyRule(){
	var vpi_num = document.form.dsl_vpi.value;
	var vci_num = document.form.dsl_vci.value;

	for(var i = 0; i < DSLWANList.length; i++){
		if (i!=document.form.dsl_unit.value) {
			if (dsl_pvc_enabled[i] != "0") {
				if (vpi_num == DSLWANList[i][1] && vci_num == DSLWANList[i][2]) {
					alert("<#same_vpi_vci#>");
					return;
				}
			}
		}
	}

	if(document.form.dslx_dnsenable[1].checked == true && document.form.dsl_proto.value != "mer" && document.form.dslx_dns1.value == "" && document.form.dslx_dns2.value == "") {
		alert("<#dns_server_no_set#>");
		return;
	}

	if(validForm()){
		showLoading();

		inputCtrl(document.form.dslx_DHCPClient[0], 1);
		inputCtrl(document.form.dslx_DHCPClient[1], 1);
		if(!document.form.dslx_DHCPClient[0].checked){
			inputCtrl(document.form.dslx_ipaddr, 1);
			inputCtrl(document.form.dslx_netmask, 1);
			inputCtrl(document.form.dslx_gateway, 1);
		}

		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);
		if(!document.form.dslx_dnsenable[0].checked){
			inputCtrl(document.form.dslx_dns1, 1);
			inputCtrl(document.form.dslx_dns1, 1);
		}

		if (document.form.dsl_unit.value=='0' && document.form.dsl_enable.value=='1') {
			document.form.wan_enable.value = document.form.dslx_link_enable.value;
			document.form.wan_unit.value = '0';
			document.form.wan_enable.disabled = false;
			document.form.wan_unit.disabled = false;
		}

		if (document.form.dsl_unit.value == 0 && document.form.dsl_proto.value == "bridge")
			document.getElementById('bridgePPPoE_relay').innerHTML = '<input type="hidden" name="fw_pt_pppoerelay" value="1"> ';

		inputCtrl(document.form.dsl_pcr, 1);
		inputCtrl(document.form.dsl_scr, 1);
		inputCtrl(document.form.dsl_mbs, 1);

		if (document.form.dsl_proto.value == "bridge") {
			document.form.action_script.value = "reboot";		
			document.form.action_wait.value = "<% get_default_reboot_time(); %>";				
		}
		document.form.submit();
	}
}

// test if WAN IP & Gateway & DNS IP is a valid IP
// DNS IP allows to input nothing
function valid_IP(obj_name, obj_flag){
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
		var ip_num = inet_network(ip_obj.value);

		if(obj_flag == "DNS" && ip_num == -1){ //DNS allows to input nothing
			return true;
		}

		if(obj_flag == "GW" && ip_num == -1){ //GW allows to input nothing
			return true;
		}

		if(ip_num > A_class_start && ip_num < A_class_end)
			return true;
		else if(ip_num > B_class_start && ip_num < B_class_end){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
		else if(ip_num > C_class_start && ip_num < C_class_end)
			return true;
		else{
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
}

function validForm(){
	if(!document.form.dslx_DHCPClient[0].checked){// Set IP address by userself
		if(!valid_IP(document.form.dslx_ipaddr, "")) return false;  //WAN IP
		if(!valid_IP(document.form.dslx_gateway, "GW"))return false;  //Gateway IP

		if(document.form.dslx_gateway.value == document.form.dslx_ipaddr.value){
			alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
			return false;
		}

		// test if netmask is valid.
		var default_netmask = "";
		var wrong_netmask = 0;
		var netmask_obj = document.form.dslx_netmask;
		var netmask_num = inet_network(netmask_obj.value);

		if(netmask_num==0){
			var netmask_reverse_num = 0;		//Viz 2011.07 : Let netmask 0.0.0.0 pass
		}else{
		var netmask_reverse_num = ~netmask_num;
		}

		if(netmask_num < 0) wrong_netmask = 1;

		var test_num = netmask_reverse_num;
		while(test_num != 0){
			if((test_num+1)%2 == 0)
				test_num = (test_num+1)/2-1;
			else{
				wrong_netmask = 1;
				break;
			}
		}
		if(wrong_netmask == 1){
			alert(netmask_obj.value+" <#JS_validip#>");
			netmask_obj.value = default_netmask;
			netmask_obj.focus();
			netmask_obj.select();
			return false;
		}
	}

	if(!document.form.dslx_dnsenable[0].checked){
		if(!valid_IP(document.form.dslx_dns1, "DNS")) return false;  //DNS1
		if(!valid_IP(document.form.dslx_dns2, "DNS")) return false;  //DNS2
	}

	if(document.form.dsl_proto.value == "pppoe"
			|| document.form.dsl_proto.value == "pppoa"
			){
		if(!validate_string(document.form.dslx_pppoe_username)
				|| !validate_string(document.form.dslx_pppoe_passwd)
				)
			return false;

		if(!validate_number_range(document.form.dslx_pppoe_idletime, 0, 4294967295))
			return false;
	}

	if(document.form.dsl_proto.value == "pppoe"){
		if(!validate_number_range(document.form.dslx_pppoe_mtu, 576, 1492))
			return false;
//				|| !validate_number_range(document.form.dslx_pppoe_mru, 576, 1492))

		if(!validate_string(document.form.dslx_pppoe_service)
				|| !validate_string(document.form.dslx_pppoe_ac))
			return false;
	}

	return true;
}

function done_validating(action){
	refreshpage();
}

function disable_pvc_summary() {
	$("dsl_pvc_summary").style.display = "none";
	if(dualWAN_support != -1){
		$("WANscap").style.display = "none";
	}
}

function enable_pvc_summary() {
	$("dsl_pvc_summary").style.display = "";
	if(dualWAN_support != -1){
		$("WANscap").style.display = "";
	}
}

function disable_all_ctrl() {
	$("desc_default").style.display = "";
	$("desc_edit").style.display = "none";
	//$("dslpvc").style.display = "none";
	$("dslSettings").style.display = "none";
	$("PPPsetting").style.display = "none";
	$("DNSsetting").style.display = "none";
	$("IPsetting").style.display = "none";
	$("t2BC").style.display = "none";
	$("vpn_server").style.display = "none";
	$("btn_apply").style.display = "none";
}

function enable_all_ctrl() {
	$("desc_default").style.display = "none";
	$("desc_edit").style.display = "";
	//$("dslpvc").style.display = "";
	$("dslSettings").style.display = "";
	$("PPPsetting").style.display = "";
	$("DNSsetting").style.display = "";
	$("IPsetting").style.display = "";
	$("t2BC").style.display = "";
	$("vpn_server").style.display = "";
	$("btn_apply").style.display = "";
}

function change_dsl_type(dsl_type){
	change_dsl_dhcp_enable();
	change_dsl_dns_enable();

	if(dsl_type == "pppoe" || dsl_type == "pppoa"){
		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);

		inputCtrl(document.form.dslx_pppoe_username, 1);
		inputCtrl(document.form.dslx_pppoe_passwd, 1);
		inputCtrl(document.form.dslx_pppoe_idletime, 1);
		inputCtrl(document.form.dslx_pppoe_mtu, 1);
//		inputCtrl(document.form.dslx_pppoe_mru, 1);
		inputCtrl(document.form.dslx_pppoe_service, 1);
		inputCtrl(document.form.dslx_pppoe_ac, 1);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.dslx_pppoe_options, 1);
		// 2008.03 James. patch for Oleg's patch. }
	}
	else if(dsl_type == "ipoa"){
		inputCtrl(document.form.dslx_dnsenable[0], 0);
		inputCtrl(document.form.dslx_dnsenable[1], 0);

		inputCtrl(document.form.dslx_pppoe_username, 0);
		inputCtrl(document.form.dslx_pppoe_passwd, 0);
		inputCtrl(document.form.dslx_pppoe_idletime, 0);
		inputCtrl(document.form.dslx_pppoe_mtu, 0);
//		inputCtrl(document.form.dslx_pppoe_mru, 0);
		inputCtrl(document.form.dslx_pppoe_service, 0);
		inputCtrl(document.form.dslx_pppoe_ac, 0);
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.dslx_pppoe_options, 0);
		// 2008.03 James. patch for Oleg's patch. }
	}
	else if(dsl_type == "mer"){
		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);

		inputCtrl(document.form.dslx_pppoe_username, 0);
		inputCtrl(document.form.dslx_pppoe_passwd, 0);
		inputCtrl(document.form.dslx_pppoe_idletime, 0);
		inputCtrl(document.form.dslx_pppoe_mtu, 0);
//		inputCtrl(document.form.dslx_pppoe_mru, 0);
		inputCtrl(document.form.dslx_pppoe_service, 0);
		inputCtrl(document.form.dslx_pppoe_ac, 0);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.dslx_pppoe_options, 0);
		// 2008.03 James. patch for Oleg's patch. }
	}
	else if(dsl_type == "bridge") {
		inputCtrl(document.form.dslx_dnsenable[0], 0);
		inputCtrl(document.form.dslx_dnsenable[1], 0);
		inputCtrl(document.form.dslx_pppoe_username, 0);
		inputCtrl(document.form.dslx_pppoe_passwd, 0);
		inputCtrl(document.form.dslx_pppoe_idletime, 0);
		inputCtrl(document.form.dslx_pppoe_mtu, 0);
//		inputCtrl(document.form.dslx_pppoe_mru, 0);
		inputCtrl(document.form.dslx_pppoe_service, 0);
		inputCtrl(document.form.dslx_pppoe_ac, 0);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.dslx_pppoe_options, 0);
		// 2008.03 James. patch for Oleg's patch. }
	}
	else {
		alert("error");
	}
}


function fixed_change_dsl_type(dsl_type){
	if(!document.form.dslx_DHCPClient[0].checked){
		if(document.form.dslx_ipaddr.value.length == 0)
			document.form.dslx_ipaddr.focus();
		else if(document.form.dslx_netmask.value.length == 0)
			document.form.dslx_netmask.focus();
		else if(document.form.dslx_gateway.value.length == 0)
			document.form.dslx_gateway.focus();
	}

	if(dsl_type == "pppoe" || dsl_type == "pppoa"){
		document.form.dslx_dnsenable[0].checked = 1;
		document.form.dslx_dnsenable[1].checked = 0;
		change_common_radio(document.form.dslx_dnsenable, 'IPConnection', 'dslx_dnsenable', 0);
		inputCtrl(document.form.dslx_dns1, 0);
		inputCtrl(document.form.dslx_dns2, 0);
		inputCtrl(document.form.dslx_hwaddr, 1);
		inputCtrl(document.form.dslx_link_enable[0], 1);
		inputCtrl(document.form.dslx_link_enable[1], 1);
		inputCtrl(document.form.dslx_nat[0], 1);
		inputCtrl(document.form.dslx_nat[1], 1);
		inputCtrl(document.form.dslx_upnp_enable[0], 1);
		inputCtrl(document.form.dslx_upnp_enable[1], 1);
	}
	else if(dsl_type == "ipoa"){
		document.form.dslx_dnsenable[0].checked = 0;
		document.form.dslx_dnsenable[1].checked = 1;
		change_common_radio(document.form.dslx_dnsenable, 'IPConnection', 'dslx_dnsenable', 0);
		inputCtrl(document.form.dslx_hwaddr, 1);
		inputCtrl(document.form.dslx_link_enable[0], 1);
		inputCtrl(document.form.dslx_link_enable[1], 1);
		inputCtrl(document.form.dslx_nat[0], 1);
		inputCtrl(document.form.dslx_nat[1], 1);
		inputCtrl(document.form.dslx_upnp_enable[0], 1);
		inputCtrl(document.form.dslx_upnp_enable[1], 1);
	}
	else if(dsl_type == "mer"){
		inputCtrl(document.form.dslx_DHCPClient[0], 1);
		inputCtrl(document.form.dslx_DHCPClient[1], 1);
		inputCtrl(document.form.dslx_hwaddr, 1);
		inputCtrl(document.form.dslx_link_enable[0], 1);
		inputCtrl(document.form.dslx_link_enable[1], 1);
		inputCtrl(document.form.dslx_nat[0], 1);
		inputCtrl(document.form.dslx_nat[1], 1);
		inputCtrl(document.form.dslx_upnp_enable[0], 1);
		inputCtrl(document.form.dslx_upnp_enable[1], 1);
	}
	else if(dsl_type == "bridge"){
		document.form.dslx_dnsenable[0].checked = 1;
		document.form.dslx_dnsenable[1].checked = 0;
		change_common_radio(document.form.dslx_dnsenable, 'IPConnection', 'dslx_dnsenable', 0);

		inputCtrl(document.form.dslx_dns1, 0);
		inputCtrl(document.form.dslx_dns2, 0);
		inputCtrl(document.form.dslx_hwaddr, 0);
		inputCtrl(document.form.dslx_link_enable[0], 0);
		inputCtrl(document.form.dslx_link_enable[1], 0);
		inputCtrl(document.form.dslx_nat[0], 0);
		inputCtrl(document.form.dslx_nat[1], 0);
		inputCtrl(document.form.dslx_upnp_enable[0], 0);
		inputCtrl(document.form.dslx_upnp_enable[1], 0);
	}
	else {
		alert("error");
	}
}

function change_dsl_dns_enable(){
	var dsl_type = document.form.dsl_proto.value;

	if(dsl_type == "pppoe" || dsl_type == "pppoa" || dsl_type == "mer"){
		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);

		var wan_dnsenable = document.form.dslx_dnsenable[0].checked;
		//var wan_dnsenable = true;
		//var wan_dnsenable = false;

		inputCtrl(document.form.dslx_dns1, !wan_dnsenable);
		inputCtrl(document.form.dslx_dns2, !wan_dnsenable);
	}
	else if(dsl_type == "ipoa"){
	// value fix
		inputCtrl(document.form.dslx_dnsenable[0], 0);
		inputCtrl(document.form.dslx_dnsenable[1], 0);

		inputCtrl(document.form.dslx_dns1, 1);
		inputCtrl(document.form.dslx_dns2, 1);
	}
	else{	// dsl_type == bridge
		inputCtrl(document.form.dslx_dnsenable[0], 0);
		inputCtrl(document.form.dslx_dnsenable[1], 0);

		inputCtrl(document.form.dslx_dns1, 0);
		inputCtrl(document.form.dslx_dns2, 0);
	}

/*
	if(document.form.dslx_DHCPClient[0].checked){
		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);
	}
	else{		// dhcp NO
		document.form.dslx_dnsenable[0].checked = 0;
		document.form.dslx_dnsenable[1].checked = 1;
		change_common_radio(document.form.dslx_dnsenable, 'IPConnection', 'dslx_dnsenable', 0);

		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);
	}
*/
}

function change_dsl_dhcp_enable(){
	var dsl_type = document.form.dsl_proto.value;

	if(dsl_type == "pppoe" || dsl_type == "pppoa" || dsl_type == "mer"){
		inputCtrl(document.form.dslx_DHCPClient[0], 1);
		inputCtrl(document.form.dslx_DHCPClient[1], 1);

		var wan_dhcpenable = document.form.dslx_DHCPClient[0].checked;

		inputCtrl(document.form.dslx_ipaddr, !wan_dhcpenable);
		inputCtrl(document.form.dslx_netmask, !wan_dhcpenable);
		inputCtrl(document.form.dslx_gateway, !wan_dhcpenable);
	}
	else if(dsl_type == "ipoa"){
		document.form.dslx_DHCPClient[0].checked = 0;
		document.form.dslx_DHCPClient[1].checked = 1;
		inputCtrl(document.form.dslx_DHCPClient[0], 0);
		inputCtrl(document.form.dslx_DHCPClient[1], 0);
		inputCtrl(document.form.dslx_ipaddr, 1);
		inputCtrl(document.form.dslx_netmask, 1);
		inputCtrl(document.form.dslx_gateway, 1);
	}
	else{	// dsl_type == bridge
		document.form.dslx_DHCPClient[0].checked = 1;
		document.form.dslx_DHCPClient[1].checked = 0;
		inputCtrl(document.form.dslx_DHCPClient[0], 0);
		inputCtrl(document.form.dslx_DHCPClient[1], 0);
		inputCtrl(document.form.dslx_ipaddr, 0);
		inputCtrl(document.form.dslx_netmask, 0);
		inputCtrl(document.form.dslx_gateway, 0);
	}

	if(document.form.dslx_DHCPClient[0].checked){
		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);
	}
	else{		// dhcp NO
		document.form.dslx_dnsenable[0].checked = 0;
		document.form.dslx_dnsenable[1].checked = 1;
		change_common_radio(document.form.dslx_dnsenable, 'IPConnection', 'dslx_dnsenable', 0);

		inputCtrl(document.form.dslx_dnsenable[0], 1);
		inputCtrl(document.form.dslx_dnsenable[1], 1);
	}
}
/*
function changeDSLunit(){
	if(document.form.dsl_unit.value == 0){
		$("PPPsetting").style.display = "";
		$("DNSsetting").style.display = "";
		$("IPsetting").style.display = "";
		$("t2BC").style.display = "";
		$("vpn_server").style.display = "";
		inputCtrl(document.form.dsl_enable, 0);
	}
	else{
		$("PPPsetting").style.display = "none";
		$("DNSsetting").style.display = "none";
		$("IPsetting").style.display = "none";
		$("t2BC").style.display = "none";
		$("vpn_server").style.display = "none";
		inputCtrl(document.form.dsl_enable, 1);
	}
}
*/


function change_svc_cat(_value){
	if(_value == 0){
		document.form.dsl_pcr.value = "0";
		document.form.dsl_scr.value = "0";
		document.form.dsl_mbs.value = "0";
		inputCtrl(document.form.dsl_pcr, 0);
		inputCtrl(document.form.dsl_scr, 0);
		inputCtrl(document.form.dsl_mbs, 0);
	}
	else if(_value < 3){
		document.form.dsl_pcr.value = "300";
		document.form.dsl_scr.value = "0";
		document.form.dsl_mbs.value = "0";
		inputCtrl(document.form.dsl_pcr, 1);
		inputCtrl(document.form.dsl_scr, 0);
		inputCtrl(document.form.dsl_mbs, 0);
	}
	else{
		document.form.dsl_pcr.value = "300";
		document.form.dsl_scr.value = "299";
		document.form.dsl_mbs.value = "32";
		inputCtrl(document.form.dsl_pcr, 1);
		inputCtrl(document.form.dsl_scr, 1);
		inputCtrl(document.form.dsl_mbs, 1);
	}
} 

function change_svc_cat_current(_value){
	if(_value == 0){
		inputCtrl(document.form.dsl_pcr, 0);
		inputCtrl(document.form.dsl_scr, 0);
		inputCtrl(document.form.dsl_mbs, 0);
	}
	else if(_value < 3){
		inputCtrl(document.form.dsl_pcr, 1);
		inputCtrl(document.form.dsl_scr, 0);
		inputCtrl(document.form.dsl_mbs, 0);
	}
	else{
		inputCtrl(document.form.dsl_pcr, 1);
		inputCtrl(document.form.dsl_scr, 1);
		inputCtrl(document.form.dsl_mbs, 1);
	}
} 

function showMAC(){
	var tempMAC = "";
	document.form.dslx_hwaddr.value = login_mac_str();
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_DSL_Content.asp">
<input type="hidden" name="next_page" value="Advanced_DSL_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_dslwan_if 0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>" />
<input type="hidden" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>" />
<input type="hidden" name="dsl_unit" value="" />
<input type="hidden" name="dsl_enable" value="" />
<input type="hidden" name="wan_enable" value="" disabled>
<input type="hidden" name="wan_unit" value="" disabled>
<span id="bridgePPPoE_relay"></span>

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>

	<td height="430" valign="top">
	  <div id="tabMenu" class="submenuBlock"></div>

	  <!--===================================Beginning of Main Content===========================================-->
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
			<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
			<tbody>
				<tr height="10px">
		  			<td bgcolor="#4D595D" valign="top">
		  			<div>&nbsp;</div>
		  			<div class="formfonttitle"><#menu5_3#> - <#menu5_3_1#></div>
		  			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					<div id="desc_default" class="formfontdesc"><#dsl_wan_page_desc#></div>
		  			<div id="desc_edit" class="formfontdesc"><#Layer3Forwarding_x_ConnectionType_sectiondesc#></div>
					</td>
	  		</tr>
				<tr id="WANscap" height="10px">
					<td bgcolor="#4D595D" valign="top">
						<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2">WAN index</td>
							</tr>
							</thead>
							<tr>
								<th>WAN Type</th>
								<td align="left">
									<select id="" class="input_option" name="wan_unit" onchange="change_wan_unit();">
									</select>
								</td>
							</tr>
						</table>
					</td>
	  		</tr>

				<tr id="dsl_pvc_summary">
            <td bgcolor="#4D595D" valign="top">
            <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="DSL_WAN_table">
                <thead>
                <tr>
                    <td colspan="9">PVC Summary</td>
                </tr>
                </thead>
					<tr>
						<th style="width: 7%;"><center>Index</center></th>
						<th style="width: 9%;"><center>VPI</center></th>
						<th style="width: 9%;"><center>VCI</center></th>
						<th style="width: 12%;"><center>Protocol</center></th>
						<th style="width: 13%;"><center>Encapsulation</center></th>
						<th style="width: 10%;"><center>Internet</center></th>
						<th style="width: 10%;"><center>IPTV</center></th>
						<th style="width: 15%;"><center>Edit PVC</center></th>
						<th style="width: 15%;"><center>Add / Delete</center></th>
					</tr>
            </table>
            </td>
            </tr>


				<tr id="dslSettings">
					<td bgcolor="#4D595D" valign="top">
						<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2">PVC Settings</td>
							</tr>
							</thead>
							<tr>
								<th>PVC</th>
								<td><span id="pvc_sel" style="color:white;"></span></td>
							</tr>
							<tr>
								<th><#Layer3Forwarding_x_ConnectionType_itemname#></th>
								<td align="left">
									<select class="input_option" name="dsl_proto" onchange="change_dsl_type(this.value);fixed_change_dsl_type(this.value);">
										<option value="pppoe" <% nvram_match("dsl_proto", "pppoe", "selected"); %>>PPPoE</option>
										<option value="pppoa" <% nvram_match("dsl_proto", "pppoa", "selected"); %>>PPPoA</option>
										<option value="ipoa" <% nvram_match("dsl_proto", "ipoa", "selected"); %>>IPoA</option>
										<option value="mer" <% nvram_match("dsl_proto", "mer", "selected"); %>>MER</option>
										<option value="bridge" <% nvram_match("dsl_proto", "bridge", "selected"); %>>Bridge</option>
									</select>
								</td>
							</tr>
							<tr>
								<th>VPI</th>
								<td><input type="text" name="dsl_vpi" maxlength="3" class="input_12_table" value="<% nvram_get("dsl_vpi"); %>" onKeyPress="" onKeyUp="">&nbsp;0 - 255</td>
							</tr>
							<tr>
								<th>VCI</th>
								<td><input type="text" name="dsl_vci" maxlength="5" class="input_12_table" value="<% nvram_get("dsl_vci"); %>" onKeyPress="" onKeyUp="">&nbsp;0 - 65535</td>
							</tr>
							<tr>
								<th>Encapsulation Mode</th>
								<td align="left">
									<select id="" class="input_option" name="dsl_encap" onchange="">
										<option value="0" <% nvram_match("dsl_encap", "0", "selected"); %>>LLC</option>
										<option value="1" <% nvram_match("dsl_encap", "1", "selected"); %>>VC</option>
									</select>
								</td>
							</tr>
							<tr>
								<th>Service Category</th>
								<td align="left">
									<select id="" class="input_option" name="dsl_svc_cat" onchange="change_svc_cat(this.value);">
										<option value="0" <% nvram_match("dsl_svc_cat", "0", "selected"); %>>UBR without PCR</option>
										<option value="1" <% nvram_match("dsl_svc_cat", "1", "selected"); %>>UBR with PCR</option>
										<option value="2" <% nvram_match("dsl_svc_cat", "2", "selected"); %>>CBR</option>
										<option value="3" <% nvram_match("dsl_svc_cat", "3", "selected"); %>>VBR</option>
										<option value="4" <% nvram_match("dsl_svc_cat", "4", "selected"); %>>GFR</option>
										<option value="5" <% nvram_match("dsl_svc_cat", "5", "selected"); %>>NRT_VBR</option>
									</select>
								</td>
							</tr>
							<tr>
								<th>PCR</th>
								<td><input type="text" name="dsl_pcr" maxlength="4" class="input_12_table" value="<% nvram_get("dsl_pcr"); %>" onKeyPress="" onKeyUp="">&nbsp;1 - 1887</td>
							</tr>
							<tr>
								<th>SCR</th>
								<td><input type="text" name="dsl_scr" maxlength="4" class="input_12_table" value="<% nvram_get("dsl_scr"); %>" onKeyPress="" onKeyUp="">&nbsp;1 - 1887</td>
							</tr>
							<tr>
								<th>MBS</th>
								<td><input type="text" name="dsl_mbs" maxlength="3" class="input_12_table" value="<% nvram_get("dsl_mbs"); %>" onKeyPress="" onKeyUp="">&nbsp;1 - 300</td>
							</tr>
						</table>
					</td>
	  		</tr>


				<tr id="t2BC">
		  			<td bgcolor="#4D595D">
						<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						  <thead>
						  <tr>
							<td colspan="2"><#t2BC#></td>
						  </tr>
						  </thead>
							<tr>
								<th><#Enable_WAN#></th>
								<td>
									<input type="radio" name="dslx_link_enable" class="input" value="1" <% nvram_match("dslx_link_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="dslx_link_enable" class="input" value="0" <% nvram_match("dslx_link_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,22);"><#Enable_NAT#></a></th>
								<td>
									<input type="radio" name="dslx_nat" class="input" value="1" <% nvram_match("dslx_nat", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="dslx_nat" class="input" value="0" <% nvram_match("dslx_nat", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,23);"><#BasicConfig_EnableMediaServer_itemname#></a></th>
								<td>
									<input type="radio" name="dslx_upnp_enable" class="input" value="1" onclick="return change_common_radio(this, 'LANHostConfig', 'dslx_upnp_enable', '1')" <% nvram_match("dslx_upnp_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="dslx_upnp_enable" class="input" value="0" onclick="return change_common_radio(this, 'LANHostConfig', 'dslx_upnp_enable', '0')" <% nvram_match("dslx_upnp_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
						</table>
					</td>
				</tr>

				<tr id="IPsetting">
					<td bgcolor="#4D595D" id="ip_sect">
						<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2"><#IPConnection_ExternalIPAddress_sectionname#></td>
							</tr>
							</thead>

							<tr>
								<th><#Layer3Forwarding_x_DHCPClient_itemname#></th>
								<td>
									<input type="radio" name="dslx_DHCPClient" class="input" value="1" onclick="change_dsl_dhcp_enable();" <% nvram_match("dslx_DHCPClient", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="dslx_DHCPClient" class="input" value="0" onclick="change_dsl_dhcp_enable();" <% nvram_match("dslx_DHCPClient", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,1);"><#IPConnection_ExternalIPAddress_itemname#></a></th>
								<td><input type="text" name="dslx_ipaddr" maxlength="15" class="input_15_table" value="<% nvram_get("dslx_ipaddr"); %>" onKeyPress="return is_ipaddr(this,event);"></td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,2);"><#IPConnection_x_ExternalSubnetMask_itemname#></a></th>
								<td><input type="text" name="dslx_netmask" maxlength="15" class="input_15_table" value="<% nvram_get("dslx_netmask"); %>" onKeyPress="return is_ipaddr(this,event);"></td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,3);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
								<td><input type="text" name="dslx_gateway" maxlength="15" class="input_15_table" value="<% nvram_get("dslx_gateway"); %>" onKeyPress="return is_ipaddr(this,event);"></td>
							</tr>
						</table>
					</td>
	  		</tr>

	  		<tr id="DNSsetting">
	    		<td bgcolor="#4D595D" id="dns_sect">
						<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          		<thead>
            	<tr>
              <td colspan="2"><#IPConnection_x_DNSServerEnable_sectionname#></td>
            	</tr>
          		</thead>
         			<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,12);"><#IPConnection_x_DNSServerEnable_itemname#></a></th>
								<td>
			  					<input type="radio" name="dslx_dnsenable" class="input" value="1" onclick="change_dsl_dns_enable()" <% nvram_match("dslx_dnsenable", "1", "checked"); %> /><#checkbox_Yes#>
			  					<input type="radio" name="dslx_dnsenable" class="input" value="0" onclick="change_dsl_dns_enable()" <% nvram_match("dslx_dnsenable", "0", "checked"); %> /><#checkbox_No#>
								</td>
          		</tr>

          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,13);"><#IPConnection_x_DNSServer1_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="dslx_dns1" value="<% nvram_get("dslx_dns1"); %>" onkeypress="return is_ipaddr(this,event)" /></td>
          		</tr>
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,14);"><#IPConnection_x_DNSServer2_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="dslx_dns2" value="<% nvram_get("dslx_dns2"); %>" onkeypress="return is_ipaddr(this,event)" /></td>
          		</tr>
        		</table>
        	</td>
	  		</tr>

	  		<tr id="PPPsetting">
	    		<td bgcolor="#4D595D" id="account_sect">
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
            	<thead>
            	<tr>
              	<td colspan="2"><#PPPConnection_UserName_sectionname#></td>
            	</tr>
            	</thead>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,4);"><#PPPConnection_UserName_itemname#></a></th>
              	<td><input type="text" maxlength="64" class="input_32_table" name="dslx_pppoe_username" value="<% nvram_get("dslx_pppoe_username"); %>" onkeypress="return is_string(this, event)" onblur=""></td>
			</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,5);"><#PPPConnection_Password_itemname#></a></th>
              	<td><input type="password" maxlength="64" class="input_32_table" name="dslx_pppoe_passwd" value="<% nvram_get("dslx_pppoe_passwd"); %>"></td>
            	</tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,6);"><#PPPConnection_IdleDisconnectTime_itemname#></a></th>
              	<td>
                	<input type="text" maxlength="10" class="input_12_table" name="dslx_pppoe_idletime" value="<% nvram_get("dslx_pppoe_idletime"); %>" onkeypress="return is_number(this,event)" />
              	</td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,7);"><#PPPConnection_x_PPPoEMTU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="dslx_pppoe_mtu" class="input_6_table" value="<% nvram_get("dslx_pppoe_mtu"); %>" onKeyPress="return is_number(this,event);"/>&nbsp;576 - 1492</td>
            	</tr>
<!--
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,8);"><#PPPConnection_x_PPPoEMRU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="wan_pppoe_mru" class="input_6_table" value="<% nvram_get("wan_pppoe_mru"); %>" onKeyPress="return is_number(this,event);"/></td>
            	</tr>
-->
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,9);"><#PPPConnection_x_ServiceName_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="dslx_pppoe_service" value="<% nvram_get("dslx_pppoe_service"); %>" onkeypress="return is_string(this, event)" onblur=""/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,10);"><#PPPConnection_x_AccessConcentrator_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="dslx_pppoe_ac" value="<% nvram_get("dslx_pppoe_ac"); %>" onkeypress="return is_string(this, event)"/></td>
            	</tr>
            	<!-- 2008.03 James. patch for Oleg's patch. { -->
				<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,18);"><#PPPConnection_x_AdditionalOptions_itemname#></a></th>
				<td><input type="text" name="dslx_pppoe_options" value="<% nvram_get("dslx_pppoe_options"); %>" class="input_32_table" maxlength="255" onKeyPress="return is_string(this, event)" onBlur="validate_string(this)"></td>
				</tr>
				<!-- 2008.03 James. patch for Oleg's patch. } -->
				</table>
          </td>
	  </tr>
                <tr id="vpn_server">
                <td bgcolor="#4D595D">
      <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
		<tr>
            	<td colspan="2"><#PPPConnection_x_HostNameForISP_sectionname#></td>
            	</tr>
		</thead>
        	<tr>
          	<th ><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,16);"><#PPPConnection_x_MacAddressForISP_itemname#></a></th>
				<td>
					<input type="text" name="dslx_hwaddr" class="input_20_table" maxlength="17" value="<% nvram_get("dslx_hwaddr"); %>" onKeyPress="return is_hwaddr(this,event)" onblur="check_macaddr(this,check_hwaddr_temp(this))">
					<input type="button" class="button_gen" onclick="showMAC();" value="<#BOP_isp_MACclone#>">
				</td>
        	</tr>
		</table>
      	  </td>
				</tr>
				<tr id="btn_apply">
				<td bgcolor="#4D595D" valign="top">
				  <div class="apply_gen">
					<input class="button_gen" onclick="exit_to_main();" type="button" value="<#CTL_Cancel#>"/>
					<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_ok#>"/>
				  </div>
      	  </td>
      	  </tr>

</tbody>

</table>
</td>
</form>


			</table>
		</td>
		<!--===================================Ending of Main Content===========================================-->

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>

</body>
</html>
