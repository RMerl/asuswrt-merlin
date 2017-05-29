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
<title><#Web_Title#> - <#menu5_3_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="device-map/device-map.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" language="JavaScript" src="/js/jquery.js"></script>
<script>
var wItem = new Array(new Array("", "", "TCP"),
											new Array("FTP", "20,21", "TCP"),
											new Array("TELNET", "23", "TCP"),
											new Array("SMTP", "25", "TCP"),
											new Array("DNS", "53", "UDP"),
											new Array("FINGER", "79", "TCP"),
											new Array("HTTP", "80", "TCP"),
											new Array("POP3", "110", "TCP"),
											new Array("SNMP", "161", "UDP"),
											new Array("SNMP TRAP", "162", "UDP"),
											new Array("GRE", "47", "OTHER"),
											new Array("IPv6 Tunnel", "41", "OTHER"),
											new Array("IPSec VPN", "500,4500", "UDP"),
											new Array("Open VPN", "1194", "UDP"),
											new Array("PPTP VPN", "1723", "TCP"),
											new Array("L2TP / IPSec VPN", "500,1701,4500", "UDP")
											);

var wItem2 = new Array(new Array("", "", "TCP"),
											 new Array("Age of Empires", "2302:2400,6073", "BOTH"),
											 new Array("BitTorrent", "6881:6889", "TCP"),
											 new Array("Counter Strike(TCP)", "27030:27039", "TCP"),
											 new Array("Counter Strike(UDP)", "27000:27015,1200", "UDP"),
											 new Array("PlayStation2", "4658,4659", "BOTH"),
											 new Array("Warcraft III", "6112:6119,4000", "BOTH"),
											 new Array("WOW", "3724", "BOTH"),
											 new Array("Xbox Live", "3074", "BOTH"));

<% login_state_hook(); %>

var vts_rulelist_array = [];

var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var wans_mode ='<% nvram_get("wans_mode"); %>';
var dual_wan_lb_status = (check_dual_wan_status().status == "1" && check_dual_wan_status().mode == "lb") ? true : false;
var support_dual_wan_unit_flag = (mtwancfg_support && dual_wan_lb_status) ? true : false;
function initial(){
	show_menu();
	//parse nvram to array
	var parseNvramToArray = function(oriNvram) {
		var parseArray = [];
		var oriNvramRow = decodeURIComponent(oriNvram).split('<');

		for(var i = 0; i < oriNvramRow.length; i += 1) {
			if(oriNvramRow[i] != "") {
				var oriNvramCol = oriNvramRow[i].split('>');
				var eachRuleArray = new Array();
				var serviceName = oriNvramCol[0];
				eachRuleArray.push(serviceName);
				var sourceTarget = (oriNvramCol[5] == undefined) ? "" : oriNvramCol[5];
				eachRuleArray.push(sourceTarget);
				var portRange = oriNvramCol[1];
				eachRuleArray.push(portRange);
				var localIP = oriNvramCol[2];
				eachRuleArray.push(localIP);
				var localPort = oriNvramCol[3];
				eachRuleArray.push(localPort);
				var protocol = oriNvramCol[4];
				eachRuleArray.push(protocol);
				parseArray.push(eachRuleArray);
			}
		}
		return parseArray;
	};
	vts_rulelist_array["vts_rulelist_0"] = parseNvramToArray('<% nvram_char_to_ascii("","vts_rulelist"); %>');
	if(support_dual_wan_unit_flag) {
		document.getElementById("tr_wan_unit").style.display = "";
		vts_rulelist_array["vts_rulelist_1"] = parseNvramToArray('<% nvram_char_to_ascii("","vts1_rulelist"); %>');
	}
	Object.keys(vts_rulelist_array).forEach(function(key) {
		gen_vts_ruleTable_Block(key);
	});
	
	loadAppOptions();
	loadGameOptions();
	setTimeout("showDropdownClientList('setClientIP', 'ip>0', 'all', 'ClientList_Block_0', 'pull_arrow_0', 'online');", 1000);
	if(support_dual_wan_unit_flag) {
		setTimeout("showDropdownClientList('setClientIP', 'ip>1', 'all', 'ClientList_Block_1', 'pull_arrow_1', 'online');", 1000);
	}
	Object.keys(vts_rulelist_array).forEach(function(key) {
		showvts_rulelist(vts_rulelist_array[key], key);
	});
	addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "port", "forwarding"]);

	if(!parent.usb_support){
		document.getElementById('FTP_desc').style.display = "none";
		document.form.vts_ftpport.parentNode.parentNode.style.display = "none";
	}

	//if(dualWAN_support && wans_mode == "lb")
	//	document.getElementById("lb_note").style.display = "";

	if('<% get_parameter("af"); %>' == 'KnownApps' && '<% get_parameter("item"); %>' == 'ftp'){
		var KnownApps = document.form.KnownApps;
		KnownApps.options[1].selected = 1;
		change_wizard(KnownApps, 'KnownApps');
		if(addRow_Group(128, $("#vts_rulelist_0")[0])) applyRule();
	}

	if(based_modelid == "GT-AC5300")
		document.getElementById("VSGameList").parentNode.style.display = "none";
}

function isChange(){
	if((document.form.vts_enable_x[0].checked == true && '<% nvram_get("vts_enable_x"); %>' == '0') || 
				(document.form.vts_enable_x[1].checked == true && '<% nvram_get("vts_enable_x"); %>' == '1')){
		return true;
	}
	else if(document.form.vts_ftpport.value != '<% nvram_get("vts_ftpport"); %>'){
		return true;
	}
	else if(document.form.vts_rulelist.value != decodeURIComponent('<% nvram_char_to_ascii("","vts_rulelist"); %>')){
		return true;
	}
	else
		return false;
}

function applyRule(){
	if(parent.usb_support){
		if(!validator.numberRange(document.form.vts_ftpport, 1, 65535)){
			return false;	
		}	
	}	
	
	//parse array to nvram
	var parseArrayToNvram = function(_dataArray) {
		var tmp_value = "";
		for(var i = 0; i < _dataArray.length; i += 1) {
			if(_dataArray[i].length != 0) {
				tmp_value += "<";
				var serviceName = _dataArray[i][0];
				tmp_value += serviceName + ">";
				var portRange = _dataArray[i][2];
				tmp_value += portRange + ">";
				var localIP = _dataArray[i][3];
				tmp_value += localIP + ">";
				var localPort = _dataArray[i][4];
				tmp_value += localPort + ">";
				var protocol = _dataArray[i][5];
				tmp_value += protocol + ">";
				var sourceTarget = _dataArray[i][1];
				tmp_value += sourceTarget;
			}
		}
		return tmp_value;
	};
	

	document.form.vts_rulelist.value = parseArrayToNvram(vts_rulelist_array["vts_rulelist_0"]);
	if(support_dual_wan_unit_flag) {
		document.form.vts1_rulelist.disabled = false;
		document.form.vts1_rulelist.value = parseArrayToNvram(vts_rulelist_array["vts_rulelist_1"]);
	}
	
	/* 2014.04 Viz: No need to reboot for ctf enable models.
	if(ctf_disable == '0' && isChange()){
		document.form.action_script.value = "reboot";
		document.form.action_wait.value = "<% get_default_reboot_time(); %>";
	}*/	

	showLoading();
	document.form.submit();
}

function loadAppOptions(){
	free_options(document.form.KnownApps);
	add_option(document.form.KnownApps, "<#Select_menu_default#>", 0, 1);
	for(var i = 1; i < wItem.length; i++)
		add_option(document.form.KnownApps, wItem[i][0], i, 0);
}

function loadGameOptions(){
	free_options(document.form.KnownGames);
	add_option(document.form.KnownGames, "<#Select_menu_default#>", 0, 1);
	for(var i = 1; i < wItem2.length; i++)
		add_option(document.form.KnownGames, wItem2[i][0], i, 0);
}

function change_wizard(o, id){
	var wan_idx = 0;
	if(support_dual_wan_unit_flag) {
		wan_idx = document.getElementById("wans_unit").value;
	}
	if(id == "KnownApps"){
		var set_famous_server_value = function(wan_idx) {
			for(var i = 0; i < wItem.length; ++i){
				if(wItem[i][0] != null && o.value == i){
					if(wItem[i][2] == "TCP")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[0].selected = 1;
					else if(wItem[i][2] == "UDP")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[1].selected = 1;
					else if(wItem[i][2] == "BOTH")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[2].selected = 1;
					else if(wItem[i][2] == "OTHER")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[3].selected = 1;
					
					document.getElementById("vts_ipaddr_x_" + wan_idx + "").value = login_ip_str();
					document.getElementById("vts_port_x_" + wan_idx + "").value = wItem[i][1];
					document.getElementById("vts_desc_x_" + wan_idx + "").value = wItem[i][0]+" Server";
					break;
				}
			}
		};
		if(wan_idx != "2") {
			set_famous_server_value(wan_idx);
		}
		else {
			set_famous_server_value("0");
			set_famous_server_value("1");
		}

		var set_famous_ftp_value = function(wan_idx) {
			if(document.form.KnownApps.options[1].selected == 1){
				if(!parent.usb_support){
					document.getElementById("vts_port_x_" + wan_idx + "").value = "21";
				}
				document.getElementById("vts_lport_x_" + wan_idx + "").value = "21";
			}
			else {
				document.getElementById("vts_lport_x_" + wan_idx + "").value = "";
			}	
		};

		if(wan_idx != "2") {
			set_famous_ftp_value(wan_idx);
		}
		else {
			set_famous_ftp_value("0");
			set_famous_ftp_value("1");
		}
		document.getElementById("KnownApps").value = 0;
	}
	else if(id == "KnownGames"){
		var set_famous_game_value = function(wan_idx) {
			document.getElementById("vts_lport_x_" + wan_idx + "").value = "";
			for(var i = 0; i < wItem2.length; ++i){
				if(wItem2[i][0] != null && o.value == i){
					if(wItem2[i][2] == "TCP")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[0].selected = 1;
					else if(wItem2[i][2] == "UDP")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[1].selected = 1;
					else if(wItem2[i][2] == "BOTH")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[2].selected = 1;
					else if(wItem2[i][2] == "OTHER")
						document.getElementById("vts_proto_x_" + wan_idx + "").options[3].selected = 1;
					
					document.getElementById("vts_ipaddr_x_" + wan_idx + "").value = login_ip_str();
					document.getElementById("vts_port_x_" + wan_idx + "").value = wItem2[i][1];
					document.getElementById("vts_desc_x_" + wan_idx + "").value = wItem2[i][0];

					break;
				}
			}
		};

		if(wan_idx != "2") {
			set_famous_game_value(wan_idx);
		}
		else {
			set_famous_game_value("0");
			set_famous_game_value("1");
		}
		document.getElementById("KnownGames").value = 0;
	}
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(num, wan_idx){
	document.getElementById('vts_ipaddr_x_' + wan_idx + '').value = num;
	hideClients_Block(wan_idx);
}

function pullLANIPList(obj){
	var wan_idx = obj.id.split("_")[2];
	var element = document.getElementById('ClientList_Block_' + wan_idx + '');
	var isMenuopen = element.offsetWidth > 0 || element.offsetHeight > 0;
	if(isMenuopen == 0) {
		obj.src = "/images/arrow-top.gif"
		element.style.display = 'block';
		document.getElementById("vts_ipaddr_x_" + wan_idx + "").focus();
	}
	else
		hideClients_Block(wan_idx);
}

function hideClients_Block(wan_idx){
	document.getElementById("pull_arrow_" + wan_idx + "").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_' + wan_idx + '').style.display = 'none';
	validator.validIPForm(document.getElementById("vts_ipaddr_x_" + wan_idx + ""), 0);
}
/*----------} Mouse event of fake LAN IP select menu-----------------*/

var add_ruleList_array = new Array();
function validForm(_wan_idx){
	if(!Block_chars(document.getElementById("vts_desc_x_" + _wan_idx + ""), ["<" ,">" ,"'" ,"%"])) {
		return false;
	}
	
	if(!Block_chars(document.getElementById("vts_port_x_" + _wan_idx + ""), ["<" ,">"])) {
		return false;		
	}	

	if(document.getElementById("vts_proto_x_" + _wan_idx + "").value == "OTHER") {
		document.getElementById("vts_lport_x_" + _wan_idx + "").value = "";
		if (!check_multi_range(document.getElementById("vts_port_x_" + _wan_idx + ""), 1, 255, false))
			return false;
	}

	if(!check_multi_range(document.getElementById("vts_port_x_" + _wan_idx + ""), 1, 65535, true)) {
		return false;
	}
	
	
	if(document.getElementById("vts_lport_x_" + _wan_idx + "").value.length > 0
			&& !validator.numberRange(document.getElementById("vts_lport_x_" + _wan_idx + ""), 1, 65535)) {
		return false;	
	}
	
	if(document.getElementById("vts_ipaddr_x_" + _wan_idx + "").value == "") {
		alert("<#JS_fieldblank#>");
		document.getElementById("vts_ipaddr_x_" + _wan_idx + "").focus();
		document.getElementById("vts_ipaddr_x_" + _wan_idx + "").select();		
		return false;
	}
	if(!validator.validIPForm(document.getElementById("vts_ipaddr_x_" + _wan_idx + ""), 0)){			
		return false;	
	}
	
	
	add_ruleList_array = [];
	add_ruleList_array.push(document.getElementById("vts_desc_x_" + _wan_idx + "").value);
	add_ruleList_array.push(document.getElementById("vts_target_x_" + _wan_idx + "").value);
	add_ruleList_array.push(document.getElementById("vts_port_x_" + _wan_idx + "").value);
	add_ruleList_array.push(document.getElementById("vts_ipaddr_x_" + _wan_idx + "").value);
	add_ruleList_array.push(document.getElementById("vts_lport_x_" + _wan_idx + "").value);
	add_ruleList_array.push(document.getElementById("vts_proto_x_" + _wan_idx + "").value);
	return true;
}

function addRow_Group(upper, _this){
	var wan_idx = $(_this).closest("*[wanUnitID]").attr( "wanUnitID" );
	if(validForm(wan_idx)){
		if('<% nvram_get("vts_enable_x"); %>' != "1")
			document.form.vts_enable_x[0].checked = true;

		var rule_num = vts_rulelist_array["vts_rulelist_" + wan_idx + ""].length;
		if(rule_num >= upper){
			alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
			return false;
		}

		//match(Source Target + Port Range + Protocol) is not accepted
		var vts_rulelist_array_temp = vts_rulelist_array["vts_rulelist_" + wan_idx + ""].slice();
		var add_ruleList_array_temp = add_ruleList_array.slice();
		if(vts_rulelist_array_temp.length > 0) {
			add_ruleList_array_temp.splice(0, 1);//filter Service Name
			add_ruleList_array_temp.splice(2, 2);//filter Local IP and Local Port
			if(add_ruleList_array_temp[2] == "BOTH") { // BOTH is TCP and UDP
				for(var i = 0; i < vts_rulelist_array_temp.length; i += 1) {
					var currentRuleArrayTemp = vts_rulelist_array_temp[i].slice();
					currentRuleArrayTemp.splice(0, 1);//filter Service Name
					currentRuleArrayTemp.splice(2, 2);//filter Local IP and Local Port
					if(add_ruleList_array_temp[0] == currentRuleArrayTemp[0] && add_ruleList_array_temp[1] == currentRuleArrayTemp[1] && currentRuleArrayTemp[2] != "OTHER") {
						alert("<#JS_duplicate#>");
						document.getElementById("vts_port_x_" + wan_idx + "").focus();
						document.getElementById("vts_port_x_" + wan_idx + "").select();
						return false;
					}
				}
			}
			else {
				for(var i = 0; i < vts_rulelist_array_temp.length; i += 1) {
					var currentRuleArrayTemp = vts_rulelist_array_temp[i].slice();
					currentRuleArrayTemp.splice(0, 1);//filter Service Name
					currentRuleArrayTemp.splice(2, 2);//filter Local IP and Local Port
					if(add_ruleList_array_temp.toString() == currentRuleArrayTemp.toString()) {
						alert("<#JS_duplicate#>");
						document.getElementById("vts_port_x_" + wan_idx + "").focus();
						document.getElementById("vts_port_x_" + wan_idx + "").select();
						return false;
					}
					if(currentRuleArrayTemp[2] == "BOTH" && add_ruleList_array_temp[2] != "OTHER") {
						if(add_ruleList_array_temp[0] == currentRuleArrayTemp[0] && add_ruleList_array_temp[1] == currentRuleArrayTemp[1]) {
							alert("<#JS_duplicate#>");
							document.getElementById("vts_port_x_" + wan_idx + "").focus();
							document.getElementById("vts_port_x_" + wan_idx + "").select();
							return false;
						}
					}
				}
			}
			vts_rulelist_array["vts_rulelist_" + wan_idx + ""].push(add_ruleList_array);
		}
		else {
			vts_rulelist_array["vts_rulelist_" + wan_idx + ""].push(add_ruleList_array);
		}

		document.getElementById("vts_desc_x_" + wan_idx + "").value = "";
		document.getElementById("vts_target_x_" + wan_idx + "").value = "";
		document.getElementById("vts_port_x_" + wan_idx + "").value = "";
		document.getElementById("vts_ipaddr_x_" + wan_idx + "").value = "";
		document.getElementById("vts_lport_x_" + wan_idx + "").value = "";
		document.getElementById("vts_proto_x_" + wan_idx + "").value = "TCP";
		showvts_rulelist(vts_rulelist_array["vts_rulelist_" + wan_idx + ""], "vts_rulelist_" + wan_idx + "");
		return true;
	}
}

function validate_multi_range(val, mini, maxi, obj){

	var rangere=new RegExp("^([0-9]{1,5})\:([0-9]{1,5})$", "gi");
	if(rangere.test(val)){
		if(!validator.eachPort(obj, RegExp.$1, mini, maxi) || !validator.eachPort(obj, RegExp.$2, mini, maxi)){
				return false;								
		}else if(parseInt(RegExp.$1) >= parseInt(RegExp.$2)){
				alert("<#JS_validport#>");	
				return false;												
		}else				
			return true;	
	}else{
		if(!validate_single_range(val, mini, maxi)){	
					return false;											
				}
				return true;								
			}	
}
function validate_single_range(val, min, max) {
	for(j=0; j<val.length; j++){		//is_number
		if (val.charAt(j)<'0' || val.charAt(j)>'9'){			
			alert('<#JS_validrange#> ' + min + ' <#JS_validrange_to#> ' + max);
			return false;
		}
	}
	
	if(val < min || val > max) {		//is_in_range		
		alert('<#JS_validrange#> ' + min + ' <#JS_validrange_to#> ' + max);
		return false;
	}else	
		return true;
}	
var parse_port="";
function check_multi_range(obj, mini, maxi, allow_range){
	_objValue = obj.value.replace(/[-~]/gi,":");	// "~-" to ":"
	var PortSplit = _objValue.split(",");
	for(i=0;i<PortSplit.length;i++){
		PortSplit[i] = PortSplit[i].replace(/(^\s*)|(\s*$)/g, ""); 		// "\space" to ""
		PortSplit[i] = PortSplit[i].replace(/(^0*)/g, ""); 		// "^0" to ""	
		
		if(PortSplit[i] == "" ||PortSplit[i] == 0){
			alert("<#JS_ipblank1#>");
			obj.focus();
			obj.select();			
			return false;
		}
		if(allow_range)
			res = validate_multi_range(PortSplit[i], mini, maxi, obj);
		else	res = validate_single_range(PortSplit[i], mini, maxi, obj);
		if(!res){
			obj.focus();
			obj.select();
			return false;
		}						
		
		if(i ==PortSplit.length -1)
			parse_port = parse_port + PortSplit[i];
		else
			parse_port = parse_port + PortSplit[i] + ",";
			
	}
	obj.value = parse_port;
	parse_port ="";
	return true;	
}

function del_Row(_this){
	var row_idx = $(_this).closest("*[row_tr_idx]").attr( "row_tr_idx" );
	var wan_idx = $(_this).closest("*[wanUnitID]").attr( "wanUnitID" );
	
	vts_rulelist_array["vts_rulelist_" + wan_idx + ""].splice(row_idx, 1);
	showvts_rulelist(vts_rulelist_array["vts_rulelist_" + wan_idx + ""], "vts_rulelist_" + wan_idx + "");
}

function showvts_rulelist(_arrayData, _tableID) {
	var wan_idx = _tableID.split("_")[2];
	var width_array = [20, 20, 16, 18, 10, 10, 6];
	var handle_long_text = function(_len, _text, _width) {
		var html = "";
		if(_text.length > _len) {
			var display_text = "";
			display_text = _text.substring(0, (_len - 2)) + "...";
			html +='<td width="' +_width + '%" title="' + _text + '">' + display_text + '</td>';
		}
		else
			html +='<td width="' + _width + '%">' + _text + '</td>';
		return html;
	};
	var code = "";
	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table">';
	if(_arrayData.length == 0)
		code +='<tr><td style="color:#FFCC00;" colspan="7"><#IPConnection_VSList_Norule#></td></tr>';
	else {
		for(var i = 0; i < _arrayData.length; i += 1) {
			var eachValue = _arrayData[i];
			if(eachValue.length != 0) {
				code +='<tr row_tr_idx="' + i + '">';
				for(var j = 0; j < eachValue.length; j += 1) {
					switch(j) {
						case 0 :
						case 1 :
							code += handle_long_text(18, eachValue[j], width_array[j]);
							break;
						case 2 :
							code += handle_long_text(14, eachValue[j], width_array[j]);
							break;
						default :
							code +='<td width="' + width_array[j] + '%">' + eachValue[j] + '</td>';
							break;
					}
				}
				
				code +='<td width="14%"><input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
				code +='</tr>';
			}
		}
	}
	code +='</table>';
	document.getElementById("vts_rulelist_Block_" + wan_idx + "").innerHTML = code;	     
}

function changeBgColor(obj, num){
	if(obj.checked)
 		document.getElementById("row" + num).style.background='#FF9';
	else
 		document.getElementById("row" + num).style.background='#FFF';
}
function gen_vts_ruleTable_Block(_tableID) {
	var html = "";
	html += '<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">';
	
	html += '<thead>';
	html += '<tr>';
	var wan_title = "";
	var wan_idx = _tableID.split("_")[2];
	if(support_dual_wan_unit_flag) {
		switch(wan_idx) {
			case "0" :
				wan_title = "<#dualwan_primary#>&nbsp;";
				break;
			case "1" :
				wan_title = "<#dualwan_secondary#>&nbsp;";
				break;
		}
	}
	html += '<td colspan="7">' + wan_title + '<#IPConnection_VSList_title#>&nbsp;(<#List_limit#>&nbsp;128)</td>';
	html += '</tr>';
	html += '</thead>';

	html += '<tr>';
	html += '<th width="20%"><#BM_UserList1#></th>';
	html += '<th width="20%"><a class="hintstyle" href="javascript:void(0);" onClick="">Source IP</a></th>';
	html += '<th width="16%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,24);"><#FirewallConfig_LanWanSrcPort_itemname#></a></th>';
	html += '<th width="18%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,25);"><#IPConnection_VServerIP_itemname#></a></th>';
	html += '<th width="10%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,26);"><#IPConnection_VServerLPort_itemname#></a></th>';
	html += '<th width="10%"><#IPConnection_VServerProto_itemname#></th>';
	html += '<th width="6%"><#list_add_delete#></th>';
	html += '</tr>';

	html += '<tr>';
	html += '<td width="20%">';
	html += '<input type="text" maxlength="30" class="input_15_table" name="vts_desc_x_' + wan_idx + '" id="vts_desc_x_' + wan_idx + '" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"/>';
	html += '</td>';
	html += '<td width="20%">';
	html += '<input type="text" maxlength="30" class="input_15_table" name="vts_target_x_' + wan_idx + '" id="vts_target_x_' + wan_idx + '" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"/>';
	html += '</td>';
	html += '<td width="16%">';
	html += '<input type="text" maxlength="" class="input_12_table" name="vts_port_x_' + wan_idx + '" id="vts_port_x_' + wan_idx + '" onkeypress="return validator.isPortRange(this, event)" autocorrect="off" autocapitalize="off"/>';
	html += '</td>';
	html += '<td width="18%">';
	html += "<div style='display:inline-flex;'>";
	html += '<input type="text" maxlength="15" class="input_12_table" name="vts_ipaddr_x_' + wan_idx + '" id="vts_ipaddr_x_' + wan_idx + '" align="left" onkeypress="return validator.isIPAddr(this, event)" style="float:left;width:95px;"/ autocomplete="off" onClick="hideClients_Block(' + wan_idx + ');" autocorrect="off" autocapitalize="off">';
	html += "<img id='pull_arrow_" + wan_idx + "' class='pull_arrow' height='14px;' src='images/arrow-down.gif' align='right' onclick='pullLANIPList(this);' title=\"<#select_IP#>\">";
	html += "</div>";
	html += '<div id="ClientList_Block_' + wan_idx + '" class="clientlist_dropdown" style="margin-left:3px;"></div>';
	html += '</td>';
	html += '<td width="10%">';
	html += '<input type="text" maxlength="5"  class="input_6_table" name="vts_lport_x_' + wan_idx + '" id="vts_lport_x_' + wan_idx + '" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>';
	html += '</td>';
	html += '<td width="10%">';
	html += '<select name="vts_proto_x_' + wan_idx + '" id="vts_proto_x_' + wan_idx + '" class="input_option">';
	html += '<option value="TCP">TCP</option>';
	html += '<option value="UDP">UDP</option>';
	html += '<option value="BOTH">BOTH</option>';
	html += '<option value="OTHER">OTHER</option>';
	html += '</select>';
	html += '</td>';	
	html += '<td width="6%">';
	html += '<input type="button" class="add_btn" onClick="addRow_Group(128, this);" name="vts_rulelist_' + wan_idx + '" id="vts_rulelist_' + wan_idx + '" value="">';
	html += '</td>';
	html += '</tr>';
	html += '</table>';
	document.getElementById("vts_rulelist_Table_" + wan_idx + "").innerHTML = html;
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame" >
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_VirtualServer_Content.asp">
<input type="hidden" name="next_page" value="Advanced_VirtualServer_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vts_rulelist" value=''>
<input type="hidden" name="vts1_rulelist" value='' disabled>

<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div  id="mainMenu"></div>	
			<div  id="subMenu"></div>		
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
								<td bgcolor="#4D595D" valign="top"  >
								<div>&nbsp;</div>
								<div class="formfonttitle"><#menu5_3#> - <#menu5_3_4#></div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div>
									<div class="formfontdesc"><#IPConnection_VServerEnable_sectiondesc#></div>
									<ul style="margin-left:-25px; *margin-left:10px;">
										<div class="formfontdesc"><li><#FirewallConfig_Port80_itemdesc#></div>
										<div class="formfontdesc" id="FTP_desc"><li><#FirewallConfig_FTPPrompt_itemdesc#></div>
									</ul>	
								</div>		
									
								<div class="formfontdesc" style="margin-top:-10px;">
									<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;"><#menu5_3_4#>&nbspFAQ</a>
								</div>
								<div class="formfontdesc" id="lb_note" style="color:#FFCC00; display:none;"><#lb_note_portForwarding#></div>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
									<thead>
										<tr>
											<td colspan="4"><#t2BC#></td>
										</tr>
									</thead>
									<tr>
										<th><#IPConnection_VServerEnable_itemname#><input type="hidden" name="vts_num_x_0" value="<% nvram_get("vts_num_x"); %>" readonly="1" /></th>
											<td>
												<input type="radio" value="1" name="vts_enable_x" class="content_input_fd" onclick="return change_common_radio(this, 'IPConnection', 'vts_enable_x', '1')" <% nvram_match("vts_enable_x", "1", "checked"); %>><#checkbox_Yes#>
												<input type="radio" value="0" name="vts_enable_x" class="content_input_fd" onclick="return change_common_radio(this, 'IPConnection', 'vts_enable_x', '0')" <% nvram_match("vts_enable_x", "0", "checked"); %>><#checkbox_No#>
										</td>
									</tr>
									<tr>
										<th><#IPConnection_VSList_groupitemdesc#></th>
										<td id="vts_rulelist">
											<select name="KnownApps" id="KnownApps" class="input_option" onchange="change_wizard(this, 'KnownApps');">
											</select>
										</td>
									</tr>
									<tr>
										<th><#IPConnection_VSList_gameitemdesc#></th>
										<td id="VSGameList">
													<select name="KnownGames" id="KnownGames" class="input_option" onchange="change_wizard(this, 'KnownGames');">
													</select>
										</td>
									</tr>
									<tr id="tr_wan_unit" style="display: none;">
										<th><#dualwan_unit#></th>
										<td>
											<select id="wans_unit" class="input_option">
												<option value="0"><#dualwan_primary#></option>
												<option value="1"><#dualwan_secondary#></option>
												<option value="2">BOTH</option>
											</select>
										</td>
									</tr>
									<tr>
										<th><#IPConnection_VSList_ftpport#></th>
										<td>
											<input type="text" maxlength="5" name="vts_ftpport" class="input_6_table" value="<% nvram_get("vts_ftpport"); %>" onkeypress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off">
										</td>
									</tr>
								</table>
								<div id="vts_rulelist_Table_0" wanUnitID="0"></div>
								<div id="vts_rulelist_Block_0" wanUnitID="0"></div>

								<div id="vts_rulelist_Table_1" wanUnitID="1"></div>
								<div id="vts_rulelist_Block_1" wanUnitID="1"></div>
								
								<div class="apply_gen">
									<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
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
<script>

</script>
</body>
</html>
