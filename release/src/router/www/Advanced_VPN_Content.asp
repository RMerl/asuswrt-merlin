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
<script language="JavaScript" type="text/javascript" src="/form.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
<% wanlink(); %>

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var pptpd_clientlist_array_ori = '<% nvram_char_to_ascii("","pptpd_clientlist"); %>';
var pptpd_clientlist_array = decodeURIComponent(pptpd_clientlist_array_ori);

/* initial variables for openvpn start */
<% vpn_server_get_parameter(); %>;

var vpn_server_clientlist_array_ori = '<% nvram_char_to_ascii("","vpn_serverx_clientlist"); %>';
var vpn_server_clientlist_array = decodeURIComponent(vpn_server_clientlist_array_ori);
var openvpn_unit = '<% nvram_get("vpn_server_unit"); %>';
if (openvpn_unit == '1')
	var service_state = '<% nvram_get("vpn_server1_state"); %>';
else if (openvpn_unit == '2')
	var service_state = '<% nvram_get("vpn_server2_state"); %>';
else
	var service_state = false;	

var dualwan_mode = '<% nvram_get("wans_mode"); %>';

var pptpd_connected_clients = [];
var openvpnd_connected_clients = [];

var openvpn_eas = '<% nvram_get("vpn_serverx_start"); %>';
var openvpn_enabled = (openvpn_eas.indexOf(''+(openvpn_unit)) >= 0) ? "1" : "0";

function add_VPN_mode_Option(obj){
		free_options(obj);

		if(pptpd_support)
			add_option(obj, "PPTP", "pptpd", (document.form.VPNServer_mode.value == "pptpd"));
		if(openvpnd_support)
			add_option(obj, "OpenVPN", "openvpn", (document.form.VPNServer_mode.value == "openvpn"));	
}

function initial(){
	show_menu();
	addOnlineHelp($("faq"), ["ASUSWRT", "VPN"]);
	add_VPN_mode_Option(document.form.VPNServer_mode_select);

	match_vpn_mode();
	change_mode(document.form.VPNServer_mode_select);

	check_pptpd_broadcast();

	if(dualwan_mode == "lb"){
		document.getElementById('wan_ctrl').style.display = "none";
		document.getElementById('dualwan_ctrl').style.display = "";	
	}

	valid_wan_ip();
}

function pptpd_connected_status(){
	var rule_num = document.getElementById('pptpd_clientlist_table').rows.length;
	var username_status = "";	
		for(var x=0; x < rule_num; x++){
			var ind = x+1;
			username_status = "status"+ind;
			if(pptpd_connected_clients.length >0){
				for(var y=0; y<pptpd_connected_clients.length; y++){
					if(document.getElementById('pptpd_clientlist_table').rows[x].cells[1].title == pptpd_connected_clients[y].username){
						document.getElementById(username_status).innerHTML = '<a class="hintstyle2" href="javascript:void(0);" onClick="showPPTPClients(\''+pptpd_connected_clients[y].username+'\');"><#Connected#></a>';
							break;
					}		
				}
				if(document.getElementById(username_status).innerHTML == ""){
					document.getElementById(username_status).innerHTML = '<#Disconnected#>';
				}
			
			}else if(document.getElementById(username_status)){
				document.getElementById(username_status).innerHTML = '<#Disconnected#>';
			}	
		}
}

function openvpnd_connected_status(){
	var rule_num = document.getElementById("openvpnd_clientlist_table").rows.length;
	var username_status = "";
		for(var x=0; x < rule_num; x++){
			var ind = x;
			username_status = "conn"+ind;
			if(openvpnd_connected_clients.length >0){
				for(var y=0; y<openvpnd_connected_clients.length; y++){
					if(document.getElementById("openvpnd_clientlist_table").rows[x].cells[1].title == openvpnd_connected_clients[y].username){
							document.getElementById(username_status).innerHTML = '<a class="hintstyle2" href="javascript:void(0);" onClick="showOpenVPNClients(\''+openvpnd_connected_clients[y].username+'\');"><#Connected#></a>';
							break;
					}		
				}
				
				if(document.getElementById(username_status).innerHTML == ""){
					document.getElementById(username_status).innerHTML = '<#Disconnected#>';
				}
			
			}else if(document.getElementById(username_status)){				
				document.getElementById(username_status).innerHTML = '<#Disconnected#>';
			}	
		}
}


//check DUT is belong to private IP.
function valid_wan_ip() {
        // test if WAN IP is a private IP.
        var A_class_start = inet_network("10.0.0.0");
        var A_class_end = inet_network("10.255.255.255");
        var B_class_start = inet_network("172.16.0.0");
        var B_class_end = inet_network("172.31.255.255");
        var C_class_start = inet_network("192.168.0.0");
        var C_class_end = inet_network("192.168.255.255");
        
        var ip_obj = wanlink_ipaddr();
        var ip_num = inet_network(ip_obj);
        var ip_class = "";

        if(ip_num > A_class_start && ip_num < A_class_end)
                ip_class = 'A';
        else if(ip_num > B_class_start && ip_num < B_class_end)
                ip_class = 'B';
        else if(ip_num > C_class_start && ip_num < C_class_end)
                ip_class = 'C';
        else if(ip_num != 0){
        				//Public IP								
								return;
        }
				
				document.getElementById("privateIP_notes").innerHTML = "<#vpn_privateIP_hint#>"
				document.getElementById("privateIP_notes").style.display = "";
				return;
}

function match_vpn_mode(){
	if('<% nvram_get("VPNServer_mode"); %>' == 'pptpd')
		document.form.VPNServer_mode_select.value = 'pptpd';
	else{	//opevpn
		document.form.VPNServer_mode_select.value = 'openvpn';
	}
}

function get_group_value(mode){
			var mode_table = mode+"_clientlist_table";
			var rule_num = document.getElementById(mode_table).rows.length;
			var item_num = document.getElementById(mode_table).rows[0].cells.length;
			var tmp_value = "";	
			if(mode == 'pptpd')
				var start_row=0;
			else //openvpnd
				var start_row=1;

			for(i=start_row; i<rule_num; i++){
				tmp_value += "<"
				for(j=1; j<item_num-1; j++){

						if(document.getElementById(mode_table).rows[i].cells[j].innerHTML.lastIndexOf("...")<0){
							tmp_value += document.getElementById(mode_table).rows[i].cells[j].innerHTML;
						}else{
							tmp_value += document.getElementById(mode_table).rows[i].cells[j].title;
						}

						if(j != item_num-2)	
								tmp_value += ">";
				}
			}
			if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
					tmp_value = "";

			return tmp_value;
}


function applyRule(){

	if(document.form.VPNServer_mode.value == "pptpd"){
			document.form.action_script.value = "restart_vpnd";
			document.form.pptpd_clientlist.value = get_group_value("pptpd");
			document.form.vpn_serverx_clientlist.disabled = true;

	}else if (document.form.VPNServer_mode.value == "openvpn"){
			document.form.action_script.value = "restart_vpnd";
			document.form.action_script.value += ";restart_chpass";
			document.form.vpn_serverx_clientlist.value = get_group_value("openvpnd");
			document.form.pptpd_clientlist.disabled = true;
	}

	showLoading();
	document.form.submit();	
}

function addRow(obj, head){
	if(obj.name.search("pptpd_clientlist") >=0){
			if(head == 1)
				pptpd_clientlist_array += "<" /*&#60*/
			else
				pptpd_clientlist_array += ">" /*&#62*/

			pptpd_clientlist_array += obj.value;
			obj.value = "";
	}else if(obj.name.search("vpn_server_clientlist") >=0){
			if(head == 1)
				vpn_server_clientlist_array += "<" /*&#60*/
			else
				vpn_server_clientlist_array += ">" /*&#62*/

			vpn_server_clientlist_array += obj.value;
			obj.value = "";
	}
}

function validForm(mode){
	if(mode == "pptpd"){
			valid_username = document.form.pptpd_clientlist_username;
			valid_password = document.form.pptpd_clientlist_password;
	}else{	//	"openvpnd"
			valid_username = document.form.vpn_server_clientlist_username;
			valid_password = document.form.vpn_server_clientlist_password;
	}
	if(valid_username.value==""){
		alert("<#JS_fieldblank#>");
		valid_username.focus();
		return false;
	}else if(!Block_chars(valid_username, [" ", "@", "*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"", "&" ])){
		return false;
	}

	if(valid_password.value==""){
		alert("<#JS_fieldblank#>");
		valid_password.focus();
		return false;
	}else if(!Block_chars(valid_password, ["<", ">", "&"])){
		return false;
	}

	return true;
}

function addRow_Group(upper, flag){
	if(flag == 'pptpd'){
		var table_id = "pptpd_clientlist_table";
		username_obj = document.form.pptpd_clientlist_username;
		password_obj = document.form.pptpd_clientlist_password;
		var rule_num = document.getElementById(table_id).rows.length;
		var item_num = document.getElementById(table_id).rows[0].cells.length;		
		if(rule_num >= upper){
			alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
			return false;	
		}		
		
	}else{	//	 'openvpnd'
		var table_id = "openvpnd_clientlist_table";
		username_obj = document.form.vpn_server_clientlist_username;
		password_obj = document.form.vpn_server_clientlist_password;
		var rule_num = document.getElementById(table_id).rows.length;
		var item_num = document.getElementById(table_id).rows[0].cells.length;		
		if(rule_num >= upper+1){
			alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
			return false;	
		}
	}		

	if(validForm(flag)){
		//Viz check same rule  //match(username) is not accepted
		if(item_num >=2){
			for(i=0; i<rule_num; i++){	
				if(username_obj.value == document.getElementById(table_id).rows[i].cells[1].title){
					alert("<#JS_duplicate#>");
					username_obj.focus();
					username_obj.select();
					return false;
				}
			}
		}

		addRow(username_obj ,1);
		addRow(password_obj, 0);

		if(flag == "pptpd"){
			showpptpd_clientlist();
			pptpd_connected_status();
		}else{  //       "openvpnd"
			showopenvpnd_clientlist();
			openvpnd_connected_status();
		}
	}
}

function del_Row(rowdata, flag){
  
	var i=rowdata.parentNode.parentNode.rowIndex;
	if(flag == "pptpd"){
		document.getElementById('pptpd_clientlist_table').deleteRow(i);

		var pptpd_clientlist_value = "";
		for(k=0; k<document.getElementById('pptpd_clientlist_table').rows.length; k++){
			for(j=1; j<document.getElementById('pptpd_clientlist_table').rows[k].cells.length-1; j++){
				if(j == 1)
					pptpd_clientlist_value += "<";
				else{
					pptpd_clientlist_value += document.getElementById('pptpd_clientlist_table').rows[k].cells[1].innerHTML;
					pptpd_clientlist_value += ">";
					pptpd_clientlist_value += document.getElementById('pptpd_clientlist_table').rows[k].cells[2].innerHTML;
				}
			}
		}

		pptpd_clientlist_array = pptpd_clientlist_value;
		if(pptpd_clientlist_array == "")
			showpptpd_clientlist();

	}else{	//	 "openvpnd"
		document.getElementById('openvpnd_clientlist_table').deleteRow(i);
  
		var vpn_server_clientlist_value = "";
		for(k=1; k<document.getElementById('openvpnd_clientlist_table').rows.length; k++){
			for(j=1; j<document.getElementById('openvpnd_clientlist_table').rows[k].cells.length-1; j++){
				if(j==1)
					vpn_server_clientlist_value += "<";
				else{
					vpn_server_clientlist_value += document.getElementById('openvpnd_clientlist_table').rows[k].cells[1].innerHTML;
 					vpn_server_clientlist_value += ">";
					vpn_server_clientlist_value += document.getElementById('openvpnd_clientlist_table').rows[k].cells[2].innerHTML;
				}
			}
		}

		vpn_server_clientlist_array = vpn_server_clientlist_value;
		if(vpn_server_clientlist_array == "")
			showopenvpnd_clientlist();
	}

}


var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
function showpptpd_clientlist(){
	var pptpd_clientlist_row = pptpd_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="pptpd_clientlist_table">';
	if(pptpd_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < pptpd_clientlist_row.length; i++){
			overlib_str0[i] = "";
			overlib_str1[i] = "";
			code +='<tr id="row'+i+'">';
			var pptpd_clientlist_col = pptpd_clientlist_row[i].split('>');
			code +='<td width="15%" id="status'+i+'"></td>';
			for(var j = 0; j < pptpd_clientlist_col.length; j++){
				if(j == 0){
					if(pptpd_clientlist_col[0].length >32){
						overlib_str0[i] += pptpd_clientlist_col[0];
						pptpd_clientlist_col[0]=pptpd_clientlist_col[0].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str0[i]+'">'+ pptpd_clientlist_col[0] +'</td>';
					}else
						code +='<td width="35%" title="'+pptpd_clientlist_col[0]+'">'+ pptpd_clientlist_col[0] +'</td>';
				}
				else if(j == 1){
					if(pptpd_clientlist_col[1].length >32){
						overlib_str1[i] += pptpd_clientlist_col[1];
						pptpd_clientlist_col[1]=pptpd_clientlist_col[1].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str1[i]+'">'+ pptpd_clientlist_col[1] +'</td>';
					}else
						code +='<td width="35%" title="'+pptpd_clientlist_col[1]+'">'+ pptpd_clientlist_col[1] +'</td>';
				} 
			}
			code +='<td width="15%">';
			code +='<input class="remove_btn" onclick="del_Row(this, \'pptpd\');" value=""/></td></tr>';
		}
	}
	code +='</table>';
	document.getElementById("pptpd_clientlist_Block").innerHTML = code;
}


var overlib_str2 = new Array();	//Viz add 2013.10 for record longer VPN client username/pwd for OpenVPN
var overlib_str3 = new Array();	//Viz add 2013.10 for record longer VPN client username/pwd for OpenVPN
function showopenvpnd_clientlist(){
	var vpn_server_clientlist_row = vpn_server_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="openvpnd_clientlist_table">';

	code +='<tr id="row0"><td width="15%" id="conn0"></td><td width="35%"><% nvram_get("http_username"); %></td><td width="35%" style="text-align:center;">-</td><td width="15%" style="text-align:center;">-</td></tr>';
	if(vpn_server_clientlist_row.length > 1){

		for(var i = 1; i < vpn_server_clientlist_row.length; i++){
			overlib_str2[i] = "";
			overlib_str3[i] = "";
			code +='<tr id="row'+i+'">';
			var vpn_server_clientlist_col = vpn_server_clientlist_row[i].split('>');
			code +='<td width="15%" id="conn'+i+'"></td>';
			for(var j = 0; j < vpn_server_clientlist_col.length; j++){
				if(j == 0){
					if(vpn_server_clientlist_col[0].length >32){
						overlib_str2[i] += vpn_server_clientlist_col[0];
						vpn_server_clientlist_col[0] = vpn_server_clientlist_col[0].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str2[i]+'">'+ vpn_server_clientlist_col[0] +'</td>';
					}else
						code +='<td width="35%">'+ vpn_server_clientlist_col[0] +'</td>';
				}
				else if(j ==1){
					if(vpn_server_clientlist_col[1].length >32){
						overlib_str3[i] += vpn_server_clientlist_col[1];
						vpn_server_clientlist_col[1] = vpn_server_clientlist_col[1].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str3[i]+'">'+ vpn_server_clientlist_col[1] +'</td>';
					}else
						code +='<td width="35%">'+ vpn_server_clientlist_col[1] +'</td>';
				} 
			}
			code +='<td width="15%">';
			code +='<input class="remove_btn" onclick="del_Row(this, \'openvpnd\');" value=""/></td></tr>';
		}
	}
	code +='</table>';
	document.getElementById("openvpnd_clientlist_Block").innerHTML = code;
}

// test if Private ip
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
	var ip_num = inet_network(ip_obj.value);	//-1 means nothing
		
	//1~254
	if(obj_name.value.split(".")[3] < 1 || obj_name.value.split(".")[3] > 254){
		alert('<#JS_validrange#> ' + 1 + ' <#JS_validrange_to#> ' + 254);
		obj_name.focus();
		return false;
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

function parsePPTPClients(){	
	var Loginfo = document.getElementById("pptp_connected_info").firstChild.innerHTML;
	if (Loginfo == "") {return;}

	Loginfo = Loginfo.replace('\r\n', '\n');
	Loginfo = Loginfo.replace('\n\r', '\n');
	Loginfo = Loginfo.replace('\r', '\n');	

	var lines = Loginfo.split('\n');
	for (i = 0; i < lines.length; i++){
				var fields = lines[i].split(' ');
				if ( fields.length != 5 ) continue;		
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

function parseOpenVPNClients(){		//192.168.123.82:46954 10.8.0.6 pine\n	
	var Loginfo = document.getElementById("openvpn_connected_info").firstChild.innerHTML;
	if (Loginfo == "") {return;}

	Loginfo = Loginfo.replace('\r\n', '\n');
	Loginfo = Loginfo.replace('\n\r', '\n');
	Loginfo = Loginfo.replace('\r', '\n');

	var lines = Loginfo.split('\n');
	for (i = 0; i < lines.length; i++){
		var fields = lines[i].split(' ');
		if ( fields.length != 3 ) continue;
		openvpnd_connected_clients.push({"username":fields[2],"remoteIP":fields[0],"VPNIP":fields[1]});
	}
}

function showOpenVPNClients(uname, flag){
	var statusmenu = "";
	var statustitle = "";
	statustitle += "<div style=\"text-decoration:underline;\">VPN IP ( Remote IP )</div>";
	_caption = statustitle;
	 
	for (i = 0; i < openvpnd_connected_clients.length; i++){
		if(uname == openvpnd_connected_clients[i].username){
			statusmenu += "<div>"+openvpnd_connected_clients[i].VPNIP+" \t( "+openvpnd_connected_clients[i].remoteIP+" )</div>";
		}
	}

	return overlib(statusmenu, WIDTH, 260, OFFSETX, -360, LEFT, STICKY, CAPTION, _caption, CLOSETITLE, '');	
}

function check_pptpd_broadcast(){
	if(document.form.pptpd_broadcast.value =="ppp" || document.form.pptpd_broadcast.value =="br0ppp")
		document.form.pptpd_broadcast_ppp[0].checked = true;
	else
		document.form.pptpd_broadcast_ppp[1].checked = true;
}

function set_pptpd_broadcast(obj){
	var pptpd_temp;
	pptpd_temp = document.form.pptpd_broadcast.value;

	if(obj.value ==1){
		if(pptpd_temp == "br0")
			document.form.pptpd_broadcast.value="br0ppp";
		else
			document.form.pptpd_broadcast.value="ppp";
	}
	else{
		if(pptpd_temp == "br0ppp")
			document.form.pptpd_broadcast.value="br0";
		else
			document.form.pptpd_broadcast.value="disable";
	}
}

function change_vpn_unit(val){
	document.form.action_mode.value = "change_vpn_server_unit";
	document.form.action = "apply.cgi";
	document.form.target = "";
	document.form.submit();
}

function change_mode(obj){
	document.form.VPNServer_mode.value = document.form.VPNServer_mode_select.value
	if(obj.value == "pptpd"){
		document.getElementById('PPTP_setting').style.display = "";
		showpptpd_clientlist();
		document.getElementById('OpenVPN_setting').style.display = "none";
		document.getElementById('openvpn_export').style.display = "none";
		document.getElementById('openvpn_unit').style.display = "none";
		document.getElementById('pptpd_enable_switch').style.display = "";
		document.getElementById('openvpn_enable_switch').style.display = "none";
		parsePPTPClients();
		pptpd_connected_status();
	}else{	//openvpn
		document.getElementById('PPTP_setting').style.display = "none";
		document.getElementById('OpenVPN_setting').style.display = "";
		document.getElementById('openvpn_export').style.display = "";
		document.getElementById('openvpn_unit').style.display = "";
		document.getElementById('pptp_samba').style.display = "none";
		document.getElementById('pptpd_enable_switch').style.display = "none";
		document.getElementById('openvpn_enable_switch').style.display = "";
		if(openvpn_enabled == '0' || '<% nvram_get("VPNServer_mode"); %>' == 'pptpd')
			document.getElementById('openvpn_export').style.display = "none";
		else
			document.getElementById('openvpn_export').style.display = "";
		if ('<% nvram_get("vpn_server_userpass_auth"); %>' == '0')
			document.getElementById('openvpn_userauth_warn').style.display="";

		if(service_state == false || service_state != '2')
			document.getElementById('export_div').style.display = "none";

		if(!email_support)
			document.getElementById('exportViaEmail').style.display = "none";

		showopenvpnd_clientlist();
		parseOpenVPNClients();
		openvpnd_connected_status();
		check_vpn_server_state()
	}
}

function check_vpn_server_state(){
		if('<% nvram_get("VPNServer_mode"); %>' == 'openvpn' && openvpn_enabled == '1' && service_state != '2'){
				document.getElementById('export_div').style.display = "none";
				document.getElementById('openvpn_initial').style.display = "none";
				document.getElementById('openvpn_starting').style.display = "";
				update_vpn_server_state();
		}
}


var starting = 0;


function update_vpn_server_state(){
$.ajax({
    		url: '/ajax_openvpn_server.asp',
    		dataType: 'script',

    		error: function(xhr){
    				setTimeout("update_vpn_server_state();", 1000);
    		},

    		success: function(){
    				if(vpnd_state != '2' && (vpnd_errno == '1' || vpnd_errno == '2')){
    						document.getElementById('openvpn_initial').style.display = "none";    						
      					document.getElementById('openvpn_error_message').innerHTML = "<span>Routing conflict! <p>Please check your IP address configuration of client profile on advanced setting page or check routing table on system log.</span>";
      					document.getElementById('openvpn_error_message').style.display = "";
      			}
      			else if(vpnd_state != '2' && vpnd_errno == '4'){
      					document.getElementById('openvpn_initial').style.display = "none";
      					document.getElementById('openvpn_error_message').innerHTML = "<span>Certification Auth. /Server cert. /Server Key field error! <p>Please check your contents of Keys&Certification on advanced setting page.</span>";
      					document.getElementById('openvpn_error_message').style.display = "";
      			}
      			else if(vpnd_state != '2' && vpnd_errno == '5'){
      					document.getElementById('openvpn_initial').style.display = "none";
      					document.getElementById('openvpn_error_message').innerHTML = "<span>Diffle Hellman parameters field error!  <p>Please check your contents of Keys&Certification on advanced setting page.</span>";
      					document.getElementById('openvpn_error_message').style.display = "";
      			}
      			else if(vpnd_state == '-1' && vpnd_errno == '0'){
      					document.getElementById('openvpn_initial').style.display = "none";
      					document.getElementById('openvpn_error_message').innerHTML = "<span>OpenVPN server daemon start fail!  <p>Please check your device environment or contents on advanced setting page.</span>";
      					document.getElementById('openvpn_error_message').style.display = "";
      			}
      			else if(vpnd_state != '2'){
      					setTimeout("update_vpn_server_state();", 1000);
      			}
      			else{	// OpenVPN server ready , vpn_server1_state==2
      					setTimeout("location.href='Advanced_VPN_Content.asp';", 1000);
      					return;
						}
  			}
  		});	
}

function showMailPanel(){
	var checker = {
		server: document.mailConfigForm.PM_SMTP_SERVER.value,
		mailPort: document.mailConfigForm.PM_SMTP_PORT.value,
		user: document.mailConfigForm.PM_SMTP_AUTH_USER.value,
		pass: document.mailConfigForm.PM_SMTP_AUTH_PASS.value,
		end: 0
	}

	if(checker.server == "" || checker.mailPort == "" || checker.user == "" || checker.pass == ""){
		$("#mailConfigPanelContainer").fadeIn(300);
		$("#mailSendPanelContainer").fadeOut(300);
	}
	else{
		$("#mailConfigPanelContainer").fadeOut(300);
		$("#mailSendPanelContainer").fadeIn(300);
	}
}

function enable_openvpn(state){
	var tmp_value = "";

	for (var i=1; i < 3; i++) {
		if (i == openvpn_unit) {
			if (state == 1)
				tmp_value += ""+i+",";
		} else {
			if (document.form.vpn_serverx_start.value.indexOf(''+(i)) >= 0)
				tmp_value += ""+i+","
		}
	}
	document.form.vpn_serverx_start.value = tmp_value;
}

</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<div id="pptp_connected_info" style="display:none;"><pre><% nvram_dump("pptp_connected",""); %></pre></div>
<div id="openvpn_connected_info" style="display:none;"><pre><% nvram_dump("openvpn_connected",""); %></pre></div>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
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
			<!-- submit needed -->
			<input type="hidden" name="current_page" value="Advanced_VPN_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_VPN_Content.asp">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<!-- UI common -->
			<input type="hidden" name="VPNServer_mode" value="<% nvram_get("VPNServer_mode"); %>">
			<!-- pptp -->
			<input type="hidden" name="pptpd_enable" value="<% nvram_get("pptpd_enable"); %>">
			<input type="hidden" name="pptpd_broadcast" value="<% nvram_get("pptpd_broadcast"); %>">	
			<input type="hidden" name="pptpd_clientlist" value="<% nvram_get("pptpd_clientlist"); %>">
			<!-- input type="hidden" name="status_pptpd" value="<% nvram_dump("pptp_connected",""); %>" -->
			<!-- openvpn -->
			<input type="hidden" name="vpn_serverx_clientlist" value="<% nvram_get("vpn_serverx_clientlist"); %>">
			<input type="hidden" name="vpn_serverx_start" value="<% nvram_get("vpn_serverx_start"); %>">
			<!-- input type="hidden" name="status_openvpnd" value="<% nvram_dump("openvpn_connected",""); %>" -->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">				
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">VPN - <#BOP_isp_heart_item#></div>
									<div align="right" style="margin-top:-35px;">
										<select name="VPNServer_mode_select" class="input_option" onchange="change_mode(this);"></select>
										<select id="openvpn_unit" style="display:none;" name="vpn_server_unit" class="input_option" onChange="change_vpn_unit(this.value);">
											<option value="1" <% nvram_match("vpn_server_unit","1","selected"); %> >Server 1</option>
											<option value="2" <% nvram_match("vpn_server_unit","2","selected"); %> >Server 2</option>
										</select>
									</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										<thead>
										<tr>
											<td colspan="3" id="GWStatic"><#t2BC#></td>
										</tr>
										</thead>
										<tr>
											<th><#vpn_enable#></th>
											<td id="pptpd_enable_switch" style="display:none;">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="pptp_service_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden;">
												<script type="text/javascript">

													$('#pptp_service_enable').iphoneSwitch('<% nvram_get("pptpd_enable"); %>',
														function() {
															document.form.pptpd_enable.value = "1";
															document.form.action_script.value = "start_pptpd";
															parent.showLoading();
															document.form.submit();
															return true;
														},
														function() {
															document.form.pptpd_enable.value = "0";
															document.form.action_script.value = "stop_pptpd";
															parent.showLoading();
															document.form.submit();
															return true;
														},
														{
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														}
													);
												</script>
											</td>

											<td id="openvpn_enable_switch">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="openvpn_service_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden;">
												<script type="text/javascript">

													$('#openvpn_service_enable').iphoneSwitch(openvpn_enabled,
														function() {
															enable_openvpn(1);
															document.form.action_script.value = "start_vpnserver"+openvpn_unit;
															parent.showLoading();
															document.form.submit();
															return true;
														},
														function() {
															enable_openvpn(0);
															document.form.action_script.value = "stop_vpnserver"+openvpn_unit;
															parent.showLoading();
															document.form.submit();
															return true;
														},
														{
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														}
													);
												</script>
											</td>
										</tr>

										<tr id="pptp_samba">
											<th><#vpn_network_place#></th>
											<td>
													<input type="radio" value="1" name="pptpd_broadcast_ppp" onchange="set_pptpd_broadcast(this);"/><#checkbox_Yes#>
													<input type="radio" value="0" name="pptpd_broadcast_ppp" onchange="set_pptpd_broadcast(this);"/><#checkbox_No#>										
											</td>
										</tr>

										<tr id="openvpn_export" style="display:none;">
										<th><#vpn_export_ovpnfile#></th>
											<td>
												<div id="export_div">
	              									<input id="exportToLocal" class="button_gen" type="button" value="<#btn_Export#>" />
	              									<input id="exportViaEmail" class="button_gen" type="button" value="via Email" style="display:none;"/><!-- Viz hide it temp 2014.06 -->
												</div>
												<script type="text/javascript">
														document.getElementById("exportToLocal").onclick = function(){
															location.href = 'client<% nvram_get("vpn_server_unit"); %>.ovpn';
														}

														document.getElementById("exportViaEmail").onclick = function(){
															showMailPanel();
														}	
												</script>
												<div id="openvpn_starting" style="display:none;margin-left:5px;">
													<span>
														Starting the server...
														<img id="starting" src="images/InternetScan.gif" />
													</span>
												</div>
												<div id="openvpn_initial" style="display:none;margin-left:5px;">
													<span>
														Initializing the settings of the OpenVPN server, this may take a few minutes...
														<img id="initialing" src="images/InternetScan.gif" />
													</span>
												</div>
												<div id="openvpn_error_message" style="display:none;margin-left:5px;">              											
												</div>	
											</td>
										</tr>
									</table>
									<br>
									<div id="privateIP_notes" class="formfontdesc" style="display:none;color:#FFCC00;"></div>

									<div id="PPTP_setting" style="display:none;">
										<div class="formfontdesc"><#PPTP_desc#></div>
										<div id="wan_ctrl" class="formfontdesc"><#PPTP_desc2#> <% nvram_get("wan0_ipaddr"); %></div>
										<div id="dualwan_ctrl" style="display:none;" class="formfontdesc"><#PPTP_desc2#> <span class="formfontdesc">Primary WAN IP : <% nvram_get("wan0_ipaddr"); %> </sapn><span class="formfontdesc">Secondary WAN IP : <% nvram_get("wan1_ipaddr"); %> </sapn></div>
										<div class="formfontdesc" style="margin-top:-10px;font-weight: bolder;"><#PPTP_desc3#></div>
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
												<td colspan="4" id="GWStatic"><#Username_Pwd#>&nbsp;(<#List_limit#>&nbsp;64)</td>
											</tr>
											</thead>
											<tr>
												<th><#PPPConnection_x_WANLink_itemname#></th>
												<th><#PPPConnection_UserName_itemname#></th>
												<th><#PPPConnection_Password_itemname#></th>
												<th><#list_add_delete#></th>
											</tr>
											<tr>
												<td width="15%" style="text-align:center;">-
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="pptpd_clientlist_username" onKeyPress="return validator.isString(this, event)">
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="pptpd_clientlist_password" onKeyPress="return validator.isString(this, event)">
												</td>
												<td width="15%">
													<div><input type="button" class="add_btn" onClick="addRow_Group(64, 'pptpd');" value=""></div>
												</td>
											</tr>
										</table>        

										<div id="pptpd_clientlist_Block"></div>
										<!-- manually assigned the DHCP List end-->	
									</div>

									<div id="OpenVPN_setting" style="display:none;">
										<div class="formfontdesc">
											If you configure OpenVPN with username/password authentication, <#Web_Title2#> can automatically generate a ovpn file including Certification Authority key.  You can deliver this file with username and password to clients making them connect to your VPN server. If you need a more advanced authenticaton method (such as using signed certs), please go to VPN details.  You will need to manually prepare and provide signed certificates then.<br>
											Please refer to below FAQ for your client side setting.
											<ol>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/1A935B95-C237-4281-AE86-C824737D11F9/" target="_blank" style="text-decoration:underline;">Windows</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/C77ADCBF-F5C4-46B4-8A0D-B64F09AB881F/" target="_blank" style="text-decoration:underline;">Mac OS</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/37EC8F08-3F50-4F82-807E-6D2DCFE5146A/" target="_blank" style="text-decoration:underline;">iPhone/iPad</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/8DCA7DA6-E5A0-40C2-8AED-B9361E89C844/" target="_blank" style="text-decoration:underline;">Android</a>
											<ol>
										</div>
										<div id="openvpn_userauth_warn" style="display:none; color:#FFCC00;">NOTE: To use Username/Password based authtication you must first enable it on the VPN Details page.</div>
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
											<thead>
											<tr>
												<td colspan="4" id="GWStatic"><#Username_Pwd#>&nbsp;(<#List_limit#>&nbsp;64)</td>
											</tr>
											</thead>
											<tr>
												<th><#PPPConnection_x_WANLink_itemname#></th>
												<th><#PPPConnection_UserName_itemname#></th>
												<th><#PPPConnection_Password_itemname#></th>
												<th><#list_add_delete#></th>
											</tr>
											<tr>
												<td width="15%" style="text-align:center;">-
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="vpn_server_clientlist_username" onKeyPress="return validator.isString(this, event)">
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="vpn_server_clientlist_password" onKeyPress="return validator.isString(this, event)">
												</td>
												<td width="15%">
													<div><input type="button" class="add_btn" onClick="addRow_Group(64, 'openvpnd');" value=""></div>
												</td>
											</tr>
										</table>        

										<div id="openvpnd_clientlist_Block"></div>
										<!-- manually assigned the DHCP List end-->	
									</div>

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
			<td width="10" align="center" valign="top">&nbsp;</td>
		</td>
	</tr>
</table>
</form>

<div id="mailSendPanelContainer" class="hiddenPanelContainer">
	<div class="hiddenPanel">
		<form method="post" name="mailSendForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="restart_sendmail">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="flag" value="background">
			<input type="hidden" name="PM_MAIL_SUBJECT" value="My ovpn file">
			<input type="hidden" name="PM_MAIL_FILE" value="/www/client<% nvram_get("vpn_server_unit"); %>.ovpn">
			<input type="hidden" name="PM_LETTER_CONTENT" value="Here is the ovpn file.">

			<div class="panelTableTitle">
				<div>Send</div>
				<img style="width: 100%; height: 2px;" src="/images/New_ui/export/line_export.png">
			</div>

			<table border=0 align="center" cellpadding="5" cellspacing="0" class="FormTable panelTable">
				<tr>
					<th>PM_MAIL_TARGET</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MAIL_TARGET" value="">
					</td>
				</tr>
			</table>

			<div class="panelSubmiter">
				<input id="mailSendPannelCancel" class="button_gen" type="button" value="<#CTL_Cancel#>">
				<input id="mailSendPannelSubmiter" class="button_gen" type="button" value="Send">
				<img id="mailSendLoadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
				<script>
					document.getElementById("mailSendPannelCancel").onclick = function(){
						$("#mailSendPanelContainer").fadeOut(300);
					}
					document.getElementById("mailSendPannelSubmiter").onclick = function(){
						// ToDo: validator.

						$("#mailSendLoadingIcon").fadeIn(200);
						document.mailSendForm.submit();
						setTimeout(function(){
							document.mailSendForm.PM_MAIL_TARGET.value = "";
							$("#mailSendLoadingIcon").fadeOut(200);
							$("#mailSendPanelContainer").fadeOut(300);
						}, document.mailSendForm.action_wait.value*1000);
					}
				</script>
			</div>
		</form>
	</div>
</div>

<div id="mailConfigPanelContainer" class="hiddenPanelContainer">
	<div class="hiddenPanel">
		<form method="post" name="mailConfigForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="saveNvram">
			<input type="hidden" name="action_wait" value="3">
			<input type="hidden" name="PM_SMTP_SERVER" value="<% nvram_get("PM_SMTP_SERVER"); %>">
			<input type="hidden" name="PM_SMTP_PORT" value="<% nvram_get("PM_SMTP_PORT"); %>">
			<input type="hidden" name="PM_SMTP_AUTH_USER" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>">
			<input type="hidden" name="PM_SMTP_AUTH_PASS" value="<% nvram_get("PM_SMTP_AUTH_PASS"); %>">
			<input type="hidden" name="PM_MY_NAME" value="<% nvram_get("PM_MY_NAME"); %>">
			<input type="hidden" name="PM_MY_EMAIL" value="<% nvram_get("PM_MY_EMAIL"); %>">
	
			<div class="panelTableTitle">
				<div>Setup mail server</div>
				<img style="width: 100%; height: 2px;" src="/images/New_ui/export/line_export.png">
			</div>

			<table border=0 align="center" cellpadding="5" cellspacing="0" class="FormTable panelTable">
				<tr>
					<th>PM_SMTP_SERVER</th>
					<td valign="top">
				    <select style="width:350px;" name="PM_SMTP_SERVER_TMP" class="input_option">
							<option value="smtp.gmail.com" <% nvram_match( "PM_SMTP_SERVER", "smtp.gmail.com", "selected"); %>>Google Gmail</option>
				    </select>
						<script>
							var smtpList = new Array()
							smtpList = [
								{smtpServer: "smtp.gmail.com", smtpPort: "587", smtpDomain: "gmail.com"},
								{end: 0}
							];

							document.mailConfigForm.PM_SMTP_SERVER_TMP.onchange = function(){
								document.mailConfigForm.PM_SMTP_PORT_TMP.value = smtpList[this.selectedIndex].smtpPort;
								document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.value = "";		
								document.mailConfigForm.PM_SMTP_AUTH_PASS_TMP.value = "";		
								document.mailConfigForm.PM_MY_NAME_TMP.value = "";		
								document.mailConfigForm.PM_MY_EMAIL_TMP.value = "";		
							}
						</script>
					</td>
				</tr>
				<input type="hidden" name="PM_SMTP_PORT_TMP" value="<% nvram_get("PM_SMTP_PORT"); %>">  				      			
				<tr>
					<th>PM_SMTP_AUTH_USER</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_SMTP_AUTH_USER_TMP" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>">
						<script>
							document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.onkeyup = function(){
								document.mailConfigForm.PM_MY_NAME_TMP.value = this.value;
								document.mailConfigForm.PM_MY_EMAIL_TMP.value = this.value + "@" + smtpList[document.mailConfigForm.PM_SMTP_SERVER_TMP.selectedIndex].smtpDomain;
							}
						</script>
					</td>
				</tr>    				      			
				<tr>
					<th>PM_SMTP_AUTH_PASS</th>
					<td valign="top">
						<input type="password" class="input_32_table" name="PM_SMTP_AUTH_PASS_TMP" maxlength="100" value="">
					</td>
				</tr>    				      			
				<tr>
					<th>PM_MY_NAME (Optional)</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MY_NAME_TMP" value="<% nvram_get("PM_MY_NAME"); %>">
					</td>
				</tr>    				      			
				<tr>
					<th>PM_MY_EMAIL (Optional)</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MY_EMAIL_TMP" value="<% nvram_get("PM_MY_EMAIL"); %>">
					</td>
				</tr>    				      			
			</table>

			<div class="panelSubmiter">
				<input id="mailConfigPannelCancel" class="button_gen" type="button" value="<#CTL_Cancel#>">
				<input id="mailConfigPannelSubmiter" class="button_gen" type="button" value="<#CTL_onlysave#>">
				<img id="mailConfigLoadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
				<script>
					document.getElementById("mailConfigPannelCancel").onclick = function(){
						$("#mailConfigPanelContainer").fadeOut(300);
					}
					document.getElementById("mailConfigPannelSubmiter").onclick = function(){
						// ToDo: validator.

						document.mailConfigForm.PM_SMTP_SERVER.value = document.mailConfigForm.PM_SMTP_SERVER_TMP.value;
						if (document.mailConfigForm.PM_SMTP_PORT_TMP.value == "")
							document.mailConfigForm.PM_SMTP_PORT.value = smtpList[0].smtpPort;
						else
							document.mailConfigForm.PM_SMTP_PORT.value = document.mailConfigForm.PM_SMTP_PORT_TMP.value;
						document.mailConfigForm.PM_SMTP_AUTH_USER.value = document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.value;
						document.mailConfigForm.PM_SMTP_AUTH_PASS.value = document.mailConfigForm.PM_SMTP_AUTH_PASS_TMP.value;
						document.mailConfigForm.PM_MY_NAME.value = document.mailConfigForm.PM_MY_NAME_TMP.value;
						document.mailConfigForm.PM_MY_EMAIL.value = document.mailConfigForm.PM_MY_EMAIL_TMP.value;

						$("#mailConfigLoadingIcon").fadeIn(200);
						document.mailConfigForm.submit();
						setTimeout(function(){
							$("#mailConfigLoadingIcon").fadeOut(200);
							showMailPanel();
						}, document.mailConfigForm.action_wait.value*1000);
					}
				</script>
			</div>
		</form>
	</div>
</div>


<div id="footer"></div>
</body>
</html>
