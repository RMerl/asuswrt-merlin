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
<title><#Web_Title#> - <#BOP_isp_heart_item#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="menu_style.css">
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script language="JavaScript" type="text/javascript" src="/form.js"></script>
<style>
.contentM_qis{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:block;
	margin-left: 450px;
	margin-top: -450px;
	width:340px;
	height:160px;
	box-shadow: 3px 3px 10px #000;
	display:none;
}
</style>
<script>

<% wanlink(); %>
<% secondary_wanlink(); %>
window.onresize = function() {
	if(document.getElementById("edit_sr_block").style.display == "block") {
		cal_panel_block("edit_sr_block", 0.35);
	}
}
var pptpd_clientlist_array_ori = '<% nvram_char_to_ascii("","pptpd_clientlist"); %>';
var pptpd_clientlist_array = decodeURIComponent(pptpd_clientlist_array_ori);
var pptpd_connected_clients = [];
var pptpd_sr_rulelist_array_ori = '<% nvram_char_to_ascii("","pptpd_sr_rulelist"); %>';
var pptpd_sr_rulelist_array = decodeURIComponent(pptpd_sr_rulelist_array_ori);
var pptpd_sr_edit_username = "";

var max_shift = "";	/*MODELDEP (include dict #PPTP_desc2# #vpn_max_clients# #vpn_maximum_clients#) : 
				RT-AC5300/RT-AC3200/RT-AC3100/RT-AC88U/RT-AC87U/RT-AC68U/RT-AC66U/RT-AC56U/RT-N66U/RT-N18U */
if(based_modelid == "RT-AC5300" || based_modelid == "RT-AC3200" || based_modelid == "RT-AC3100" ||
		based_modelid == "RT-AC88U" || based_modelid == "RT-AC87U" || based_modelid == "RT-AC68U" ||
		based_modelid == "RT-AC66U" || based_modelid == "RT-AC56U" ||
		based_modelid == "RT-N66U" || based_modelid == "RT-N18U"){
	max_shift = parseInt("29");
}
else{
	max_shift = parseInt("9");
}

function initial(){
	var dualwan_mode = '<% nvram_get("wans_mode"); %>';
	var pptpd_dns1_orig = '<% nvram_get("pptpd_dns1"); %>';
	var pptpd_dns2_orig = '<% nvram_get("pptpd_dns2"); %>';
	var pptpd_wins1_orig = '<% nvram_get("pptpd_wins1"); %>';
	var pptpd_wins2_orig = '<% nvram_get("pptpd_wins2"); %>';
	var pptpd_clients = '<% nvram_get("pptpd_clients"); %>';
	
	show_menu();		
	addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "VPN"]);
	
	formShowAndHide(document.form.pptpd_enable.value, document.form.VPNServer_mode.value);	
	if(dualwan_mode == "lb"){
		var wan0_ipaddr = wanlink_ipaddr();
		var wan1_ipaddr = secondary_wanlink_ipaddr();		document.getElementById("wan_ctrl").style.display = "none";
		document.getElementById("dualwan_ctrl").style.display = "";	
		document.getElementById("dualwan_ctrl").innerHTML = "<#PPTP_desc2#> <span class=\"formfontdesc\">Primary WAN IP : " + wan0_ipaddr + " </sapn><span class=\"formfontdesc\">Secondary WAN IP : " + wan1_ipaddr + "</sapn>";
		//check DUT is belong to private IP. //realip doesn't support lb
		if(validator.isPrivateIP(wan0_ipaddr) && validator.isPrivateIP(wan1_ipaddr)){
			document.getElementById("privateIP_notes").style.display = "";
		}
	}
	else {
		var wan_primary = '<% nvram_get("wan_primary"); %>';
		var wan_ipaddr = "";
		if(wan_primary == 0) {	//primary
			wan_ipaddr = wanlink_ipaddr();
		}
		else {	//secondary
			wan_ipaddr = secondary_wanlink_ipaddr();
		}
		document.getElementById("wan_ctrl").innerHTML = "<#PPTP_desc2#>" +  wan_ipaddr;
		//check DUT is belong to private IP.
		if(realip_support){
			if(!external_ip)
				document.getElementById("privateIP_notes").style.display = "";
		}
		else if(validator.isPrivateIP(wan_ipaddr)){
			document.getElementById("privateIP_notes").style.display = "";
		}
	}
	//setting pptpd_ms_network_option and pptpd_broadcast_option	
	if(document.form.pptpd_ms_network.value == "1") {
		document.form.pptpd_ms_network_option[0].checked = true;
		document.form.pptpd_broadcast_option[0].checked = true;
		document.form.pptpd_broadcast.value = "1";
		document.form.pptpd_broadcast_option[1].disabled = true;
		document.getElementById("pptpd_broadcast_hint").style.display = "";
	}
	else {
		document.form.pptpd_ms_network_option[1].checked = true;
		if(document.form.pptpd_broadcast.value == "1") {
			document.form.pptpd_broadcast_option[0].checked = true;
		}
		else {
			document.form.pptpd_broadcast_option[1].checked = true;
		}	
	}

	/* Advanced Setting start */
	/* check_dns_wins start */
	if(pptpd_dns1_orig == "" && pptpd_dns2_orig == "") {
		document.form.pptpd_dnsenable_x[0].checked = true;
		change_pptpd_radio(document.form.pptpd_dnsenable_x[0]);
	}
	else {
		document.form.pptpd_dnsenable_x[1].checked = true;
		change_pptpd_radio(document.form.pptpd_dnsenable_x[1]);
	}		
				
	if(pptpd_wins1_orig == "" && pptpd_wins2_orig == "") {
		document.form.pptpd_winsenable_x[0].checked = true;
		change_pptpd_radio(document.form.pptpd_winsenable_x[0]);
	}
	else {
		document.form.pptpd_winsenable_x[1].checked = true;
		change_pptpd_radio(document.form.pptpd_winsenable_x[1]);
	}	
			
	/* check_dns_wins end */
	/* clientIPRange start */
	if (pptpd_clients != "") {
		document.form._pptpd_clients_start.value = pptpd_clients.split("-")[0];
		document.form._pptpd_clients_end.value = pptpd_clients.split("-")[1];
		document.getElementById("pptpd_subnet").innerHTML = pptpd_clients.split(".")[0] 
													+ "." + pptpd_clients.split(".")[1] 
													+ "." + pptpd_clients.split(".")[2] 
													+ ".";
	}
	
	/* clientIPRange end */
	/* MPPE start */
	if (document.form.pptpd_mppe.value == 0) {
		document.form.pptpd_mppe.value = (1 | 4 | 8);
	}
	
	document.form.pptpd_mppe_128.checked = (document.form.pptpd_mppe.value & 1);
	document.form.pptpd_mppe_40.checked = (document.form.pptpd_mppe.value & 4);
	document.form.pptpd_mppe_no.checked = (document.form.pptpd_mppe.value & 8);
	/* MPPE end */		
	check_vpn_conflict();
	/* Advanced Setting end */
}

function formShowAndHide(server_enable, server_type) {
	if(server_enable == "1"){
		document.getElementById("trVPNServerMode").style.display = "";
		document.getElementById('pptp_samba').style.display = "";
		document.getElementById('PPTP_setting').style.display = "";
		document.getElementById("tbAdvanced").style.display = "none";	
		showpptpd_clientlist();
		parsePPTPClients();
		pptpd_connected_status();
		document.getElementById("divApply").style.display = "";
	}
	else{
		document.getElementById("trVPNServerMode").style.display = "none";
		document.getElementById("pptp_samba").style.display = "none";
		document.getElementById("PPTP_setting").style.display = "none";
		document.getElementById("tbAdvanced").style.display = "none";
		if(document.form.VPNServer_mode.value != "pptpd") {
			document.getElementById("divApply").style.display = "none";
		}
	}
}

function pptpd_connected_status(){
	var rule_num = document.getElementById('pptpd_clientlist_table').rows.length;
	var username_status = "";	
	for(var x=0; x < rule_num; x++){
		var ind = x+1;
		username_status = "status"+ind;
		if(pptpd_connected_clients.length >0){
			for(var y=0; y<pptpd_connected_clients.length; y++) {								
				if(document.getElementById('pptpd_clientlist_table').rows[x].cells[1].title == pptpd_connected_clients[y].username){
					document.getElementById(username_status).innerHTML = '<a class="hintstyle2" href="javascript:void(0);" onClick="showPPTPClients(\''+pptpd_connected_clients[y].username+'\');"><#Connected#></a>';
					break;
				}		
			}
			
			if(document.getElementById(username_status).innerHTML == "") {
				document.getElementById(username_status).innerHTML = "<#Disconnected#>";
			}
		}
		else if(document.getElementById(username_status)) {
			document.getElementById(username_status).innerHTML = "<#Disconnected#>";
		}	
	}
}

function applyRule() {
	var confirmFlag = true;

	if(confirmFlag){
		var get_group_value = function () {
			var rule_num = document.getElementById("pptpd_clientlist_table").rows.length;
			var item_num = document.getElementById("pptpd_clientlist_table").rows[0].cells.length;
			var tmp_value = "";	

			for(var i = 0; i < rule_num; i += 1) {
				tmp_value += "<"		
				for(var j = 1; j < item_num - 2; j += 1) {
					if(document.getElementById("pptpd_clientlist_table").rows[i].cells[j].innerHTML.lastIndexOf("...") < 0) {
						tmp_value += document.getElementById("pptpd_clientlist_table").rows[i].cells[j].innerHTML;
					}
					else {
						tmp_value += document.getElementById("pptpd_clientlist_table").rows[i].cells[j].title;
					}					
					if(j != item_num - 3)
						tmp_value += ">";
				}
			}
			if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
				tmp_value = "";
						
			return tmp_value;
		};

		if(document.form.pptpd_enable.value == "1") {
			document.form.VPNServer_mode.value = 'pptpd';
			document.form.action_script.value = "restart_pptpd";
			document.form.pptpd_clientlist.value = get_group_value();
			document.form.pptpd_sr_rulelist.value = pptpd_sr_rulelist_array;
			document.form.pptpd_enable.value = "1";
			if(!validator.isLegalIP(document.form._pptpd_clients_start, "")) {
				document.form._pptpd_clients_start.focus();
				document.form._pptpd_clients_start.select();
				return false;		
			}
			
			if(document.form._pptpd_clients_start.value.split(".")[3] == 255) {
				alert(document.form._pptpd_clients_start.value + " <#JS_validip#>");
				document.form._pptpd_clients_start.focus();
				document.form._pptpd_clients_start.select();
				return false;		
			}

			if(!validator.numberRange(document.form._pptpd_clients_end, 1, 254)) {
				document.form._pptpd_clients_end.focus();
				document.form._pptpd_clients_end.select();
				return false;
			}	

			document.form.pptpd_clients.value = document.form._pptpd_clients_start.value + "-" + document.form._pptpd_clients_end.value;
			document.form.pptpd_mppe.value = 0;
			if (document.form.pptpd_mppe_128.checked)
				document.form.pptpd_mppe.value |= 1;
			if (document.form.pptpd_mppe_40.checked)
				document.form.pptpd_mppe.value |= 4;
			if (document.form.pptpd_mppe_no.checked)
				document.form.pptpd_mppe.value |= 8;

			if(document.form.pptpd_dnsenable_x[0].checked == true) {
				document.form.pptpd_dns1.value = "";
				document.form.pptpd_dns2.value = "";
			}
			else {
				if(document.form.pptpd_dns1.value == "") {
					alert("<#JS_fieldblank#>");
					document.form.pptpd_dns1.focus();
					document.form.pptpd_dns1.select();
					return false;	
				}
				else if(document.form.pptpd_dns2.value == "") {
					alert("<#JS_fieldblank#>");
					document.form.pptpd_dns2.focus();
					document.form.pptpd_dns2.select();
					return false;	
				}
					
				if(!validator.validIPForm(document.form.pptpd_dns1, 0)){
					document.form.pptpd_dns1.focus();
					document.form.pptpd_dns1.select();
					return false;
				} 
				else if(!validator.validIPForm(document.form.pptpd_dns2, 0)) {
					document.form.pptpd_dns2.focus();
					document.form.pptpd_dns2.select();
					return false;
				}
			}				
		
			if(document.form.pptpd_winsenable_x[0].checked == true) {
				document.form.pptpd_wins1.value = "";
				document.form.pptpd_wins2.value = "";
			}
			else {
				if(document.form.pptpd_wins1.value == "") {
					alert("<#JS_fieldblank#>");
					document.form.pptpd_wins1.focus();
					document.form.pptpd_wins1.select();
					return false;
				}
				else if(document.form.pptpd_wins2.value == "") {				
					alert("<#JS_fieldblank#>");
					document.form.pptpd_wins2.focus();
					document.form.pptpd_wins2.select();
					return false;
				}		
				
				if(!validator.validIPForm(document.form.pptpd_wins1, 0)) {
					document.form.pptpd_wins1.focus();
					document.form.pptpd_wins1.select();
					return false;
				}
				else if(!validator.validIPForm(document.form.pptpd_wins2, 0)) {
					document.form.pptpd_wins2.focus();
					document.form.pptpd_wins2.select();
					return false;
				}		
			}		
		
			if(check_pptpd_clients_range() == false)
				return false;

			check_vpn_conflict();	
			if(!validator.range(document.form.pptpd_mru, 576, 1492)) {
				document.form.pptpd_mru.focus();
				document.form.pptpd_mru.select();	
				return false;
			}
			
			if(!validator.range(document.form.pptpd_mtu, 576, 1492)) {
				document.form.pptpd_mtu.focus();
				document.form.pptpd_mtu.select();	
				return false;
			}		
		}
		else {		//disable server
			document.form.action_script.value = "stop_pptpd";
			document.form.pptpd_enable.value = "0";
			document.form.pptpd_clientlist.value = get_group_value();
			document.form.pptpd_sr_rulelist.value = pptpd_sr_rulelist_array;
		}

		showLoading();
		document.form.submit();
	}
}

function addRow(obj, head){
	if(head == 1)
		pptpd_clientlist_array += "<" /*&#60*/
	else	
		pptpd_clientlist_array += ">" /*&#62*/
			
	pptpd_clientlist_array += obj.value;
	obj.value = "";
}

function validForm(){
	var valid_username = document.form.pptpd_clientlist_username;
	var valid_password = document.form.pptpd_clientlist_password;

	if(valid_username.value == "") {
		alert("<#JS_fieldblank#>");
		valid_username.focus();
		return false;		
	}
	else if(!Block_chars(valid_username, [" ", "@", "*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"", "&" ])) {
		return false;		
	}

	if(valid_password.value == "") {
		alert("<#JS_fieldblank#>");
		valid_password.focus();
		return false;
	}
	else if(!Block_chars(valid_password, ["<", ">", "&"])) {
		return false;		
	}
		
	return true;
}

function addRow_Group(upper){
	var username_obj = document.form.pptpd_clientlist_username;
	var	password_obj = document.form.pptpd_clientlist_password;

	var rule_num = document.getElementById("pptpd_clientlist_table").rows.length;
	var item_num = document.getElementById("pptpd_clientlist_table").rows[0].cells.length;		
	if(rule_num >= upper) {
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}

	if(validForm()){
		//Viz check same rule  //match(username) is not accepted
		if(item_num >= 2) {
			for(var i = 0; i < rule_num; i +=1 ) {
				if(username_obj.value == document.getElementById("pptpd_clientlist_table").rows[i].cells[1].title) {
					alert("<#JS_duplicate#>");
					username_obj.focus();
					username_obj.select();
					return false;
				}	
			}
		}
		
		addRow(username_obj ,1);
		addRow(password_obj, 0);
		showpptpd_clientlist();
		pptpd_connected_status();
	}
}

function edit_Row(userName) {
	pptpd_sr_edit_username = userName;
	document.form.pptpd_sr_ipaddr.value = "";
	document.form.pptpd_sr_netmask.value = "";

	$("#edit_sr_block").fadeIn(300);
	cal_panel_block("edit_sr_block", 0.35);

	var pptpd_sr_rulelist_row = pptpd_sr_rulelist_array.split("<");
	for(var i = 1; i < pptpd_sr_rulelist_row.length; i += 1) {
		var pptpd_sr_rulelist_col = pptpd_sr_rulelist_row[i].split(">");
		if(pptpd_sr_rulelist_col[0] == pptpd_sr_edit_username) {
			document.form.pptpd_sr_ipaddr.value = pptpd_sr_rulelist_col[1];
			document.form.pptpd_sr_netmask.value = pptpd_sr_rulelist_col[2];
			break;
		}
	}
}

function del_Row(rowdata){
	var i = rowdata.parentNode.parentNode.rowIndex;
	var delUserName = rowdata.parentNode.parentNode.cells[1].innerHTML;
	document.getElementById("pptpd_clientlist_table").deleteRow(i);
	var pptpd_clientlist_value = "";
	var rowLength = document.getElementById("pptpd_clientlist_table").rows.length;
	for(var k = 0; k < rowLength; k += 1) {
		for(var j = 1; j < document.getElementById("pptpd_clientlist_table").rows[k].cells.length - 1; j += 1) {
			if(j == 1)
				pptpd_clientlist_value += "<";
			else {
				pptpd_clientlist_value += document.getElementById("pptpd_clientlist_table").rows[k].cells[1].innerHTML;
				pptpd_clientlist_value += ">";
				pptpd_clientlist_value += document.getElementById("pptpd_clientlist_table").rows[k].cells[2].innerHTML;
			}
		}
	}

	pptpd_clientlist_array = pptpd_clientlist_value;
	if(pptpd_clientlist_array == "")
		showpptpd_clientlist();

	//delete username in pptpd_sr_rulelist_array
	var pptpd_sr_rulelist_temp = "";
	var pptpd_sr_rulelist_row = pptpd_sr_rulelist_array.split("<");
	for(var i = 1; i < pptpd_sr_rulelist_row.length; i += 1) {
		var pptpd_sr_rulelist_col = pptpd_sr_rulelist_row[i].split(">");
		if(pptpd_sr_rulelist_col[0] != delUserName) {
			pptpd_sr_rulelist_temp += "<" + pptpd_sr_rulelist_row[i];
		}
	}
	pptpd_sr_rulelist_array = pptpd_sr_rulelist_temp;
}

var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
function showpptpd_clientlist(){
	var pptpd_clientlist_row = pptpd_clientlist_array.split('<');
	var code = "";
	var pptp_user_name = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="pptpd_clientlist_table">';
	if(pptpd_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="5"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < pptpd_clientlist_row.length; i++){
			overlib_str0[i] = "";
			overlib_str1[i] = "";
			code +='<tr id="row'+i+'">';
			var pptpd_clientlist_col = pptpd_clientlist_row[i].split('>');
			code +='<td width="15%" id="status'+i+'"></td>';
			for(var j = 0; j < pptpd_clientlist_col.length; j++){
				if(j == 0){
					pptp_user_name = pptpd_clientlist_col[0];
					if(pptpd_clientlist_col[0].length >28){
						overlib_str0[i] += pptpd_clientlist_col[0];
						pptpd_clientlist_col[0]=pptpd_clientlist_col[0].substring(0, 26)+"...";
						code +='<td width="30%" title="'+overlib_str0[i]+'">'+ pptpd_clientlist_col[0] +'</td>';
					}else
						code +='<td width="30%" title="'+pptpd_clientlist_col[0]+'">'+ pptpd_clientlist_col[0] +'</td>';
				}
				else if(j == 1){
					if(pptpd_clientlist_col[1].length >28){
						overlib_str1[i] += pptpd_clientlist_col[1];
						pptpd_clientlist_col[1]=pptpd_clientlist_col[1].substring(0, 26)+"...";
						code +='<td width="30%" title="'+overlib_str1[i]+'">'+ pptpd_clientlist_col[1] +'</td>';
					}else
						code +='<td width="30%">'+ pptpd_clientlist_col[1] +'</td>';
				} 
			}
			
			code +='<td width="15%">';
			code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td>';
			code +='<td width="10%">';
			code +='<input class="edit_btn" onclick="edit_Row(\''+ pptp_user_name +'\');" value=""/></td></tr>';
		}
	}
	
	code +='</table>';
	document.getElementById("pptpd_clientlist_Block").innerHTML = code;
}

function parsePPTPClients(){	
	var Loginfo = document.getElementById("pptp_connected_info").firstChild.innerHTML;
	var lines = Loginfo.split('\n');
	if(Loginfo == "")
		return;
	
	Loginfo = Loginfo.replace('\r\n', '\n');
	Loginfo = Loginfo.replace('\n\r', '\n');
	Loginfo = Loginfo.replace('\r', '\n');			
	for (i = 0; i < lines.length; i++){
		var fields = lines[i].split(' ');
		if(fields.length != 5) 
			continue;		
   		
		pptpd_connected_clients.push({"username":fields[4],"remoteIP":fields[3],"VPNIP":fields[2]});
	}
}

var _caption = "";
function showPPTPClients(uname) {
	var statusmenu = "";
	var statustitle = "";
	statustitle += "<div style=\"text-decoration:underline;\">VPN IP ( Remote IP )</div>";
	_caption = statustitle;
	
	for (i = 0; i < pptpd_connected_clients.length; i++){
		if(uname == pptpd_connected_clients[i].username){		
			statusmenu += "<div>"+pptpd_connected_clients[i].VPNIP+" \t( "+pptpd_connected_clients[i].remoteIP+" )</div>";
		}				
	}
		
	return overlib(statusmenu, WIDTH, 260, OFFSETX, -360, LEFT, STICKY, CAPTION, _caption, CLOSETITLE, '');	
}

function set_pptpd_broadcast(obj){
	if(obj.value == 1) {
		document.form.pptpd_ms_network.value = "1";
		document.form.pptpd_broadcast.value = "1";
		document.form.pptpd_broadcast_option[0].checked = true;
		document.form.pptpd_broadcast_option[1].disabled = true;
		document.getElementById("pptpd_broadcast_hint").style.display = "";
	}
	else{
		document.form.pptpd_ms_network.value = "0";
		document.form.pptpd_broadcast_option[1].disabled = false;
		document.getElementById("pptpd_broadcast_hint").style.display = "none";
	}
}

function setBroadcast(obj) {
	if(obj.value == 1) {
		document.form.pptpd_broadcast.value = "1";
		document.form.pptpd_broadcast_option[0].checked = true;
	}
	else{
		document.form.pptpd_broadcast.value = "0";
		document.form.pptpd_broadcast_option[1].checked = true;
	}
}

function switchMode(mode){
	if(mode == "1") {
		document.getElementById("pptp_samba").style.display = "";
		document.getElementById("PPTP_setting").style.display = "";
		document.getElementById("tbAdvanced").style.display = "none";
	}	
	else {
		document.getElementById("pptp_samba").style.display = "none";
		document.getElementById("PPTP_setting").style.display = "none";
		document.getElementById("tbAdvanced").style.display = "";
	}
}

function srCancel() {
	pptpd_sr_edit_username = "";
	$("#edit_sr_block").fadeOut(300);
}

function srConfirm() {
	var pptpd_sr_ipaddr = document.form.pptpd_sr_ipaddr.value
	var pptpd_sr_netmask = document.form.pptpd_sr_netmask.value
	var pptpd_sr_rulelist_array_temp = "";
	
	if(pptpd_sr_ipaddr != "" || pptpd_sr_netmask != "") {
		if(!validator.isLegalIP(document.form.pptpd_sr_ipaddr, "")) {
			return false;		
		}
		if(!validator.isLegalMask(document.form.pptpd_sr_netmask)) {
			return false;
		}
	}
	
	var usernameFlag = false;
	var pptpd_sr_rulelist_row = pptpd_sr_rulelist_array.split("<");
	for(var i = 1; i < pptpd_sr_rulelist_row.length; i += 1) {
		var pptpd_sr_rulelist_col = pptpd_sr_rulelist_row[i].split(">");
		if(pptpd_sr_rulelist_col[0] == pptpd_sr_edit_username) { //username exist in pptpd_sr_rulelist_array
			usernameFlag = true;
			if(pptpd_sr_ipaddr != "" && pptpd_sr_netmask != "") { //if not null, update value
				pptpd_sr_rulelist_array_temp += "<" + pptpd_sr_edit_username + ">"+ pptpd_sr_ipaddr + ">" + pptpd_sr_netmask;
			}
		}
		else { // add old username value
			pptpd_sr_rulelist_array_temp += "<" + pptpd_sr_rulelist_row[i];
		}
	}

	if(!usernameFlag) { //if username not exist, add value
		if(pptpd_sr_ipaddr != "" && pptpd_sr_netmask != "") {
			pptpd_sr_rulelist_array_temp += "<" + pptpd_sr_edit_username + ">" + pptpd_sr_ipaddr +  ">" + pptpd_sr_netmask;
		}
	}

	pptpd_sr_rulelist_array = pptpd_sr_rulelist_array_temp;

	srCancel();
}

/* Advanced Setting start */
function changeMppe(){
	if(!document.form.pptpd_mppe_128.checked && !document.form.pptpd_mppe_40.checked) {
		document.form.pptpd_mppe_no.checked = true;
	}		
}

function change_pptpd_radio(obj){
	if(obj.name == "pptpd_dnsenable_x"){
		document.form.pptpd_dns1.parentNode.parentNode.style.display = obj.value == 1 ? "none" : "";
		document.form.pptpd_dns2.parentNode.parentNode.style.display = obj.value == 1 ? "none" : "";
	}
	else if(obj.name == "pptpd_winsenable_x") {
		document.form.pptpd_wins1.parentNode.parentNode.style.display = obj.value == 1 ? "none" : "";
		document.form.pptpd_wins2.parentNode.parentNode.style.display = obj.value == 1? "none" : "";
	}
}

function setEnd() {
	var end = "";
	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0] 
		+ "." + document.form._pptpd_clients_start.value.split(".")[1]
		+ "." + document.form._pptpd_clients_start.value.split(".")[2]
		+ ".";
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);														
	document.getElementById("pptpd_subnet").innerHTML = pptpd_clients_subnet;

	end = pptpd_clients_start_ip + max_shift;
	if(end > 254) {
		end = 254;
	}
		
	if(!end) {
		document.form._pptpd_clients_end.value = "";
	}
	else {
		document.form._pptpd_clients_end.value = end;
	}	
		
}

function check_pptpd_clients_range(){
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);
	if(pptpd_clients_start_ip > pptpd_clients_end_ip) {
		alert("<#vlaue_haigher_than#> " + document.form._pptpd_clients_start.value);
		document.form._pptpd_clients_end.focus();
		document.form._pptpd_clients_end.select();
		setEnd();
		return false;
	}

	if( (pptpd_clients_end_ip - pptpd_clients_start_ip) > max_shift ) {
		alert("<#vpn_max_clients#>");
		document.form._pptpd_clients_start.focus();
		document.form._pptpd_clients_start.select();
		setEnd();
		return false;
	}	
}

function check_vpn_conflict() {		//if conflict with LAN ip & DHCP ip pool & static
	var pool_start = '<% nvram_get("dhcp_start"); %>';
	var pool_end = '<% nvram_get("dhcp_end"); %>';
	var pool_subnet = pool_start.split(".")[0]+"."+pool_start.split(".")[1]+"."+pool_start.split(".")[2]+".";
	var pool_start_end = parseInt(pool_start.split(".")[3]);
	var pool_end_end = parseInt(pool_end.split(".")[3]);
	var origin_lan_ip = '<% nvram_get("lan_ipaddr"); %>';
	var lan_ip_subnet = origin_lan_ip.split(".")[0]+"."+origin_lan_ip.split(".")[1]+"."+origin_lan_ip.split(".")[2]+".";
	var lan_ip_end = parseInt(origin_lan_ip.split(".")[3]);
	var dhcp_staticlists = '<% nvram_get("dhcp_staticlist"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");;
	var staticclist_row = dhcp_staticlists.split('<');
	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0] 
					   + "." + document.form._pptpd_clients_start.value.split(".")[1] 
					   + "." + document.form._pptpd_clients_start.value.split(".")[2] 
					   + ".";
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);
	
	//LAN ip
	if( lan_ip_subnet == pptpd_clients_subnet 
	 && lan_ip_end >= pptpd_clients_start_ip	
	 && lan_ip_end <= pptpd_clients_end_ip ){
		document.getElementById("pptpd_conflict").innerHTML = "<#vpn_conflict_LANIP#> <b>"+origin_lan_ip+"</b>";
		return;
	}

	//DHCP pool
	if(	pool_subnet == pptpd_clients_subnet 
		&& ( (pool_start_end >= pptpd_clients_start_ip && pool_start_end <= pptpd_clients_end_ip) 
		  || (pool_end_end >= pptpd_clients_start_ip && pool_end_end <= pptpd_clients_end_ip)
		  || (pptpd_clients_start_ip >= pool_start_end && pptpd_clients_start_ip <= pool_end_end)
		  || (pptpd_clients_end_ip >= pool_start_end && pptpd_clients_end_ip <= pool_end_end))){
		document.getElementById("pptpd_conflict").innerHTML = "<#vpn_conflict_DHCPpool#> <b>"+pool_start+" ~ "+pool_end+"</b>";
		return;
	}

	//DHCP static IP
	if(dhcp_staticlists != ""){
		for(var i = 1; i < staticclist_row.length; i++) {
			var static_subnet = "";
			var static_end = "";
			var static_ip = staticclist_row[i].split('>')[1];
			static_subnet = static_ip.split(".")[0] + "." + static_ip.split(".")[1] + "." + static_ip.split(".")[2] + ".";
			static_end = parseInt(static_ip.split(".")[3]);
			if( static_subnet == pptpd_clients_subnet 
			 && static_end >= pptpd_clients_start_ip 
			 && static_end <= pptpd_clients_end_ip){
				document.getElementById("pptpd_conflict").innerHTML = "<#vpn_conflict_DHCPstatic#> <b>"+static_ip+"</b>";
				return;
  			}				
  		}
	}

	document.getElementById("pptpd_conflict").innerHTML = "";	
}
/* Advanced Setting end */ 
</script>
</head>
<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<div id="pptp_connected_info" style="display:none;"><pre><% nvram_dump("pptp_connected",""); %></pre></div>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_VPN_PPTP.asp">
<input type="hidden" name="next_page" value="Advanced_VPN_PPTP.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="VPNServer_mode" value="pptpd">
<input type="hidden" name="pptpd_enable" value="<% nvram_get("pptpd_enable"); %>">
<input type="hidden" name="pptpd_ms_network" value="<% nvram_get("pptpd_ms_network"); %>">
<input type="hidden" name="pptpd_broadcast" value="<% nvram_get("pptpd_broadcast"); %>">	
<input type="hidden" name="pptpd_clientlist" value="<% nvram_get("pptpd_clientlist"); %>">
<input type="hidden" name="pptpd_clients" value="<% nvram_get("pptpd_clients"); %>">
<input type="hidden" name="pptpd_mppe" value="<% nvram_get("pptpd_mppe"); %>">	
<input type="hidden" name="pptpd_sr_rulelist" value="<% nvram_get("pptpd_sr_rulelist"); %>">
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
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div id="divVPNTitle" class="formfonttitle"><#BOP_isp_heart_item#> - PPTP</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div id="privateIP_notes" class="formfontdesc" style="display:none;color:#FC0;"><#vpn_privateIP_hint#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
										</thead>
										<tr>
											<th><#vpn_enable#></th>
											<td>
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_VPNServer_enable"></div>												
												<script type="text/javascript">
													$('#radio_VPNServer_enable').iphoneSwitch('<% nvram_get("pptpd_enable"); %>',
													function(){
														document.form.pptpd_enable.value = "1";
														formShowAndHide(1, "pptpd");						
													},
													function(){
														document.form.pptpd_enable.value = "0";
														formShowAndHide(0, "pptpd");
													}
													);
												</script>															
											</td>			
										</tr>
										<tr id="trVPNServerMode">
											<th><#vpn_Adv#></th>
											<td>
												<select id="selSwitchMode" onchange="switchMode(this.options[this.selectedIndex].value)" class="input_option">
													<option value="1" selected><#menu5_1_1#></option>
													<option value="2"><#menu5#></option>									
												</select>		
											</td>											
										</tr>
										<tr id="pptp_samba">
											<th><#vpn_network_place#></th>
											<td>
												<input type="radio" value="1" name="pptpd_ms_network_option" onClick="set_pptpd_broadcast(this);"/><#checkbox_Yes#>
												<input type="radio" value="0" name="pptpd_ms_network_option" onClick="set_pptpd_broadcast(this);"/><#checkbox_No#>
											</td>
										</tr>
									</table>										
									<div id="PPTP_setting" style="display:none;margin-top:8px;">
										<div class="formfontdesc"><#PPTP_desc#></div>
										<div id="wan_ctrl" class="formfontdesc"></div>
										<div id="dualwan_ctrl" style="display:none;" class="formfontdesc"></div>
										<div class="formfontdesc" style="margin-top:-10px;font-weight: bolder;"><#PPTP_desc3#></div>
										<div class="formfontdesc" style="margin-top:-10px;">(7) <#NSlookup_help#></div>
										<div class="formfontdesc" style="margin:-10px 0px 0px -15px;">
											<ul>
												<li>
													<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;"><#BOP_isp_heart_item#> FAQ</a>
												</li>
											</ul>
										</div>
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
											<thead>
											<tr>
												<td colspan="5"><#Username_Pwd#>&nbsp;(<#List_limit#>&nbsp;16)</td>
											</tr>
											</thead>								
											<tr>
												<th><#PPPConnection_x_WANLink_itemname#></th>
												<th><#HSDPAConfig_Username_itemname#></th>
												<th><#HSDPAConfig_Password_itemname#></th>
												<th><#list_add_delete#></th>
												<th><#pvccfg_edit#></th>
											</tr>			  
											<tr>
												<td width="15%" style="text-align:center;">-</td>
												<td width="30%">
													<input type="text" class="input_22_table" maxlength="64" name="pptpd_clientlist_username" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
												</td>
												<td width="30%">
													<input type="text" class="input_22_table" maxlength="64" name="pptpd_clientlist_password" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
												</td>
												<td width="15%">
													<div><input type="button" class="add_btn" onClick="addRow_Group(16);" value=""></div>
												</td>
												<td width="10%">-</td>
											</tr>	 			  
										</table>        
														
										<div id="pptpd_clientlist_Block"></div>
										<!-- manually assigned the DHCP List end-->		
									</div>																	
									<table id="tbAdvanced" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="display:none;margin-top:8px;">
										<thead>
										<tr>
											<td colspan="2"><#menu5#></td>
										</tr>
										</thead>							
										<tr>
											<th><#vpn_broadcast#></th>
											<td>
												<input type="radio" value="1" name="pptpd_broadcast_option" onClick="setBroadcast(this);"/><#checkbox_Yes#>
												<input type="radio" value="0" name="pptpd_broadcast_option" onClick="setBroadcast(this);"/><#checkbox_No#>
												<span id="pptpd_broadcast_hint" style="font-family: Lucida Console;color: #FFCC00;display: none;">When Network Place enabled, this must be enabled</span>
											</td>
										</tr>
										<tr>
											<th><#PPPConnection_Authentication_itemname#></th>
											<td>
												<select name="pptpd_chap" class="input_option">
													<option value="0" <% nvram_match("pptpd_chap", "0","selected"); %>><#Auto#></option>
													<option value="1" <% nvram_match("pptpd_chap", "1","selected"); %>>MS-CHAPv1</option>
													<option value="2" <% nvram_match("pptpd_chap", "2","selected"); %>>MS-CHAPv2</option>
												</select>			
											</td>
										</tr>
										<tr>
											<th><#MPPE_Encryp#></th>
							                <td>
												<input type="checkbox" class="input" name="pptpd_mppe_128" onClick="return changeMppe();">MPPE-128<br>
												<input type="checkbox" class="input" name="pptpd_mppe_40" onClick="return changeMppe();">MPPE-40<br>
												<input type="checkbox" class="input" name="pptpd_mppe_no" onClick="return changeMppe();"><#No_Encryp#>
											</td>
										</tr>
										<tr>
											<th><#IPConnection_x_DNSServerEnable_itemname#></th>
											<td>
										  		<input type="radio" name="pptpd_dnsenable_x" class="input" value="1" onclick="return change_pptpd_radio(this)" /><#checkbox_Yes#>
										  		<input type="radio" name="pptpd_dnsenable_x" class="input" value="0" onclick="return change_pptpd_radio(this)" /><#checkbox_No#>
											</td>
										</tr>									 								 
										<tr>
											<th><#IPConnection_x_DNSServer1_itemname#></th>
											<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns1" value="<% nvram_get("pptpd_dns1"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th><#IPConnection_x_DNSServer2_itemname#></th>
											<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns2" value="<% nvram_get("pptpd_dns2"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th><#IPConnection_x_WINSServerEnable_itemname#></th>
											<td>
											  	<input type="radio" name="pptpd_winsenable_x" class="input" value="1" onclick="return change_pptpd_radio(this)" /><#checkbox_Yes#>
											  	<input type="radio" name="pptpd_winsenable_x" class="input" value="0" onclick="return change_pptpd_radio(this)" /><#checkbox_No#>
											</td>
										</tr>
										<tr>
											<th><#IPConnection_x_WINSServer1_itemname#></th>
											<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins1" value="<% nvram_get("pptpd_wins1"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th><#IPConnection_x_WINSServer2_itemname#></th>
											<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins2" value="<% nvram_get("pptpd_wins2"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th>MRU</th>
											<td><input type="text" maxlength="4" class="input_15_table" name="pptpd_mru" value="<% nvram_get("pptpd_mru"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th>MTU</th>
											<td><input type="text" maxlength="4" class="input_15_table" name="pptpd_mtu" value="<% nvram_get("pptpd_mtu"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off"></td>
										</tr>
										<tr>
											<th><#vpn_client_ip#></th>
											<td>
												<input type="text" maxlength="15" class="input_15_table" name="_pptpd_clients_start" onBlur="setEnd();"  onKeyPress="return validator.isIPAddr(this, event);" value="" autocorrect="off" autocapitalize="off"/> ~
												<span id="pptpd_subnet" style="font-family: Lucida Console;color: #FFF;"></span><input type="text" maxlength="3" class="input_3_table" name="_pptpd_clients_end" value="" autocorrect="off" autocapitalize="off"/><span style="color:#FFCC00;"> <#vpn_maximum_clients#></span>
												<br /><span id="pptpd_conflict"></span>	
											</td>
										</tr>
									</table>
									<div id="divApply" class="apply_gen" style="display:none;">
										<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
									</div>
								</td>
							</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>
		</td>
	</tr>
</table>
<div id="edit_sr_block" class="contentM_qis">
	<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" style="margin-top:8px;">
		<thead>
			<tr>
				<td colspan="2">Static Route (Optional)</td>
			</tr>
		</thead>
		<tr>
			<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(6,1);"><#RouterConfig_GWStaticIP_itemname#></a></th>
			<td>
				<input type="text" class="input_20_table" maxlength="15" name="pptpd_sr_ipaddr" onKeyPress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr>
			<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(6,2);"><#RouterConfig_GWStaticMask_itemname#></a></th>
			<td>
				<input type="text" class="input_20_table" maxlength="15" name="pptpd_sr_netmask" onKeyPress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
	</table>
	<div style="margin-top:10px;text-align:center;">
		<input class="button_gen" type="button" onclick="srCancel();" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button" onclick="srConfirm();" value="<#CTL_ok#>">
	</div>	
</div>
</form>
<div id="footer"></div>
</body>
</html>
