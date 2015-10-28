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
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
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
											new Array("IPv6 Tunnel", "41", "OTHER"));

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

var overlib_str0 = new Array();	//Viz add 2011.07 for record longer virtual srvr rule desc
var overlib_str = new Array();	//Viz add 2011.07 for record longer virtual srvr portrange value


var vts_rulelist_array = "<% nvram_char_to_ascii("","vts_rulelist"); %>";
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var wans_mode ='<% nvram_get("wans_mode"); %>';

var backup_desc = "";
var backup_port = "";
var backup_ipaddr = "";
var backup_lport = "";
var backup_proto = "";

function initial(){
	show_menu();
	loadAppOptions();
	loadGameOptions();
	setTimeout("showLANIPList();", 1000);	
	showvts_rulelist();
	addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "port", "forwarding"]);

	if(!parent.usb_support){
		document.getElementById('FTP_desc').style.display = "none";
		document.form.vts_ftpport.parentNode.parentNode.style.display = "none";
	}

	//if(dualWAN_support && wans_mode == "lb")
	//	document.getElementById("lb_note").style.display = "";
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
	cancel_Edit();

	if(parent.usb_support){
		if(!validator.numberRange(document.form.vts_ftpport, 1, 65535)){
			return false;	
		}	
	}	
	
	var rule_num = document.getElementById('vts_rulelist_table').rows.length;
	var item_num = document.getElementById('vts_rulelist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){			
		
			if(document.getElementById('vts_rulelist_table').rows[i].cells[j].innerHTML.lastIndexOf("...")<0){
				tmp_value += document.getElementById('vts_rulelist_table').rows[i].cells[j].innerHTML;
			}else{
				tmp_value += document.getElementById('vts_rulelist_table').rows[i].cells[j].title;
			}		
			
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}

	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	

	document.form.vts_rulelist.value = tmp_value;
	
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
	if(id == "KnownApps"){
		document.getElementById("KnownGames").value = 0;
		
		for(var i = 0; i < wItem.length; ++i){
			if(wItem[i][0] != null && o.value == i){
				if(wItem[i][2] == "TCP")
					document.form.vts_proto_x_0.options[0].selected = 1;
				else if(wItem[i][2] == "UDP")
					document.form.vts_proto_x_0.options[1].selected = 1;
				else if(wItem[i][2] == "BOTH")
					document.form.vts_proto_x_0.options[2].selected = 1;
				else if(wItem[i][2] == "OTHER")
					document.form.vts_proto_x_0.options[3].selected = 1;
				
				document.form.vts_ipaddr_x_0.value = login_ip_str();
				document.form.vts_port_x_0.value = wItem[i][1];
				document.form.vts_desc_x_0.value = wItem[i][0]+" Server";				
				break;
			}
		}

		if(document.form.KnownApps.options[1].selected == 1){
				if(!parent.usb_support){
						document.form.vts_port_x_0.value = "21";
				}
				
				document.form.vts_lport_x_0.value = "21";
		}else{
				document.form.vts_lport_x_0.value = "";
		}	
	}
	else if(id == "KnownGames"){
		document.form.vts_lport_x_0.value = "";
		document.getElementById("KnownApps").value = 0;
		
		for(var i = 0; i < wItem2.length; ++i){
			if(wItem2[i][0] != null && o.value == i){
				if(wItem2[i][2] == "TCP")
					document.form.vts_proto_x_0.options[0].selected = 1;
				else if(wItem2[i][2] == "UDP")
					document.form.vts_proto_x_0.options[1].selected = 1;
				else if(wItem2[i][2] == "BOTH")
					document.form.vts_proto_x_0.options[2].selected = 1;
				else if(wItem2[i][2] == "OTHER")
					document.form.vts_proto_x_0.options[3].selected = 1;
				
				document.form.vts_ipaddr_x_0.value = login_ip_str();
				document.form.vts_port_x_0.value = wItem2[i][1];
				document.form.vts_desc_x_0.value = wItem2[i][0];
				
				break;
			}
		}
	}
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(num){
	document.form.vts_ipaddr_x_0.value = num;
	hideClients_Block();
	over_var = 0;
}

function showLANIPList(){
	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.isOnline) {
			if(clientObj.name.length > 30) clientObj.name = clientObj.name.substring(0, 28) + "..";

			htmlCode += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'';
			htmlCode += clientObj.ip;
			htmlCode += '\');"><strong>';
			htmlCode += clientObj.ip + '</strong>&nbsp;&nbsp;(' + clientObj.name + ')';
			htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
		}
	}

	document.getElementById("ClientList_Block").innerHTML = htmlCode;
}

function pullLANIPList(obj){
	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block").style.display = 'block';		
		document.form.vts_ipaddr_x_0.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block').style.display='none';
	isMenuopen = 0;
	validator.validIPForm(document.form.vts_ipaddr_x_0, 0);
}
/*----------} Mouse event of fake LAN IP select menu-----------------*/

function addRow(obj, head){
	if(head == 1)
		vts_rulelist_array += "<"
	else
		vts_rulelist_array += ">"
			
	vts_rulelist_array += obj.value;
	obj.value = "";
}

function validForm(){
	
	if(!Block_chars(document.form.vts_desc_x_0, ["<" ,">" ,"'" ,"%"])){
				return false;		
	}	
	if(!Block_chars(document.form.vts_port_x_0, ["<" ,">"])){
				return false;		
	}	

	if(document.form.vts_proto_x_0.value=="OTHER"){
		document.form.vts_lport_x_0.value = "";
		if (!check_multi_range(document.form.vts_port_x_0, 1, 255, false))
			return false;
	}

	if(!check_multi_range(document.form.vts_port_x_0, 1, 65535, true)){
		return false;
	}
	
	if(document.form.vts_lport_x_0.value.length > 0
			&& !validator.numberRange(document.form.vts_lport_x_0, 1, 65535)){
		return false;	
	}
	
	if(document.form.vts_ipaddr_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.vts_ipaddr_x_0.focus();
		document.form.vts_ipaddr_x_0.select();		
		return false;
	}
	if(document.form.vts_port_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.vts_port_x_0.focus();
		document.form.vts_port_x_0.select();		
		return false;
	}
	if(!validate_multi_range(document.form.vts_port_x_0, 1, 65535)
		|| !validator.validIPForm(document.form.vts_ipaddr_x_0, 0)){			
		return false;	
	}			
	
	return true;
}

function addRow_Group(upper){
	if(validForm()){
		if('<% nvram_get("vts_enable_x"); %>' != "1")
			document.form.vts_enable_x[0].checked = true;
		
		var rule_num = document.getElementById('vts_rulelist_table').rows.length;
		var item_num = document.getElementById('vts_rulelist_table').rows[0].cells.length;	
		if(rule_num >= upper){
				alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
				return;
		}	
		
//Viz check same rule  //match(out port+out_proto) is not accepted
	if(item_num >=2){
		for(i=0; i<rule_num; i++){
				if(entry_cmp(document.getElementById('vts_rulelist_table').rows[i].cells[4].innerHTML.toLowerCase(), document.form.vts_proto_x_0.value.toLowerCase(), 3)==0 
				|| document.form.vts_proto_x_0.value == 'BOTH'
				|| document.getElementById('vts_rulelist_table').rows[i].cells[4].innerHTML == 'BOTH'){
						
						if(overlib_str[i]){
							if(document.form.vts_port_x_0.value == overlib_str[i]){
									alert("<#JS_duplicate#>");
									document.form.vts_port_x_0.value =="";
									document.form.vts_port_x_0.focus();
									document.form.vts_port_x_0.select();							
									return;
							}
						}else{
							if(document.form.vts_port_x_0.value == document.getElementById('vts_rulelist_table').rows[i].cells[1].innerHTML){
									alert("<#JS_duplicate#>");
									document.form.vts_port_x_0.value =="";
									document.form.vts_port_x_0.focus();
									document.form.vts_port_x_0.select();							
									return;
							}
						}	
				}	
			}				
		}
			
		addRow(document.form.vts_desc_x_0 ,1);
		addRow(document.form.vts_port_x_0, 0);
		addRow(document.form.vts_ipaddr_x_0, 0);
		addRow(document.form.vts_lport_x_0, 0);
		addRow(document.form.vts_proto_x_0, 0);

		document.form.vts_proto_x_0.value="TCP";
		showvts_rulelist();

		if (backup_desc != "") {
			backup_desc = "";
			backup_port = "";
			backup_ipaddr = "";
			backup_lport = "";
			backup_proto = "";
			document.getElementById('vts_rulelist_table').rows[rule_num-1].scrollIntoView();
		}
	}
}

function validate_multi_range(val, mini, maxi){
	var rangere=new RegExp("^([0-9]{1,5})\:([0-9]{1,5})$", "gi");
	if(rangere.test(val)){
		
		if(!validator.eachPort(document.form.vts_port_x_0, RegExp.$1, mini, maxi) || !validator.eachPort(document.form.vts_port_x_0, RegExp.$2, mini, maxi)){
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
	obj.value = document.form.vts_port_x_0.value.replace(/[-~]/gi,":");	// "~-" to ":"
	var PortSplit = obj.value.split(",");
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
			res = validate_multi_range(PortSplit[i], mini, maxi);
		else	res = validate_single_range(PortSplit[i], mini, maxi);
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
	document.form.vts_port_x_0.value = parse_port;
	parse_port ="";
	return true;	
}


function edit_Row(r){ 	
	cancel_Edit();

	var i=r.parentNode.parentNode.rowIndex;

	if (document.getElementById('vts_rulelist_table').rows[i].cells[0].innerHTML.lastIndexOf("...") <0 ) {
		document.form.vts_desc_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[0].innerHTML;
	}else{
		document.form.vts_desc_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[0].title;
	}

	if (document.getElementById('vts_rulelist_table').rows[i].cells[1].innerHTML.lastIndexOf("...") <0 ) {
		document.form.vts_port_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[1].innerHTML;
	}else{
		document.form.vts_port_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[1].title;
	}

	document.form.vts_ipaddr_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[2].innerHTML; 
	document.form.vts_lport_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[3].innerHTML;
	document.form.vts_proto_x_0.value = document.getElementById('vts_rulelist_table').rows[i].cells[4].innerHTML;

	backup_desc = document.form.vts_desc_x_0.value;
	backup_port = document.form.vts_port_x_0.value;
	backup_ipaddr = document.form.vts_ipaddr_x_0.value;
	backup_lport = document.form.vts_lport_x_0.value;
	backup_proto = document.form.vts_proto_x_0.value;

	del_Row(r);
	document.form.vts_desc_x_0.focus();
}

function cancel_Edit(){
	if (backup_desc != "") {
		document.form.vts_desc_x_0.value = backup_desc;
		document.form.vts_port_x_0.value = backup_port;
		document.form.vts_ipaddr_x_0.value = backup_ipaddr;
		document.form.vts_lport_x_0.value = backup_lport;
		document.form.vts_proto_x_0.value = backup_proto;
		addRow_Group(128);
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById('vts_rulelist_table').deleteRow(i);
  
  var vts_rulelist_value = "";
	for(k=0; k<document.getElementById('vts_rulelist_table').rows.length; k++){
		for(j=0; j<document.getElementById('vts_rulelist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				vts_rulelist_value += "<";
			else
				vts_rulelist_value += ">";
				
			if(document.getElementById('vts_rulelist_table').rows[k].cells[j].innerHTML.lastIndexOf("...")<0){
				vts_rulelist_value += document.getElementById('vts_rulelist_table').rows[k].cells[j].innerHTML;
			}else{
				vts_rulelist_value += document.getElementById('vts_rulelist_table').rows[k].cells[j].title;
			}			
		}
	}
	
	vts_rulelist_array = vts_rulelist_value;
	if(vts_rulelist_array == "")
		showvts_rulelist();
}

function showvts_rulelist(){
	var vts_rulelist_row = decodeURIComponent(vts_rulelist_array).split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="vts_rulelist_table">';
	if(vts_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < vts_rulelist_row.length; i++){
			overlib_str0[i] ="";
			overlib_str[i] ="";			
			code +='<tr id="row'+i+'">';
			var vts_rulelist_col = vts_rulelist_row[i].split('>');
			var wid=[27, 15, 21, 10, 13];
				for(var j = 0; j < vts_rulelist_col.length; j++){
						if(j != 0 && j !=1){
							code +='<td width="'+wid[j]+'%">'+ vts_rulelist_col[j] +'</td>';					
						}else if(j==0){
							if(vts_rulelist_col[0].length >23){
								overlib_str0[i] += vts_rulelist_col[0];
								vts_rulelist_col[0]=vts_rulelist_col[0].substring(0, 21)+"...";
								code +='<td width="'+wid[j]+'%" title="'+overlib_str0[i]+'">'+ vts_rulelist_col[0] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%">'+ vts_rulelist_col[j] +'</td>';
						}else if(j==1){
							if(vts_rulelist_col[1].length >13){
								overlib_str[i] += vts_rulelist_col[1];
								vts_rulelist_col[1]=vts_rulelist_col[1].substring(0, 11)+"...";
								code +='<td width="'+wid[j]+'%" title='+overlib_str[i]+'>'+ vts_rulelist_col[1] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%">'+ vts_rulelist_col[j] +'</td>';
						}else{
						}
						
				}
				code +='<td width="14%"><input class="edit_btn" onclick="edit_Row(this);" value=""/>';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	document.getElementById("vts_rulelist_Block").innerHTML = code;	     
}

function changeBgColor(obj, num){
	if(obj.checked)
 		document.getElementById("row" + num).style.background='#FF9';
	else
 		document.getElementById("row" + num).style.background='#FFF';
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

		  <tr>
				<th><#IPConnection_VSList_ftpport#></th>
				<td>
			  	<input type="text" maxlength="5" name="vts_ftpport" class="input_6_table" value="<% nvram_get("vts_ftpport"); %>" onkeypress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off">
				</td>
		  </tr>

      	</table>			
      	
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
			<thead>
			<tr>
              		<td colspan="7"><#IPConnection_VSList_title#>&nbsp;(<#List_limit#>&nbsp;128)</td>
            	</tr>
 		  	</thead>
 		  	
          		<tr>
								<th><#BM_UserList1#></th>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,24);"><#FirewallConfig_LanWanSrcPort_itemname#></a></th>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,25);"><#IPConnection_VServerIP_itemname#></a></th>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,26);"><#IPConnection_VServerLPort_itemname#></a></th>
            		<th><#IPConnection_VServerProto_itemname#></th>
								<th><#list_add_delete#></th>
          		</tr>  
          		        
          		<tr>
  				<td width="27%">
  					<input type="text" maxlength="30" class="input_20_table" name="vts_desc_x_0" onKeyPress="return is_alphanum(this, event)" onblur="validator.safeName(this);" autocorrect="off" autocapitalize="off"/>
  				</td>
        			<td width="15%">
					<input type="text" maxlength="" class="input_12_table" name="vts_port_x_0" onkeypress="return validator.isPortRange(this, event)" autocorrect="off" autocapitalize="off"/>
				</td>
				<td width="21%">
					<input type="text" maxlength="15" class="input_15_table" name="vts_ipaddr_x_0" align="left" onkeypress="return validator.isIPAddr(this, event)" style="float:left;"/ autocomplete="off" onblur="if(!over_var){hideClients_Block();}" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off">
					<img id="pull_arrow" height="14px;" src="images/arrow-down.gif" align="right" onclick="pullLANIPList(this);" title="<#select_IP#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
					<div id="ClientList_Block" class="ClientList_Block"></div>
				</td>
				<td width="10%">
					<input type="text" maxlength="5"  class="input_6_table" name="vts_lport_x_0" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>
				</td>
				<td width="13%">
					<select name="vts_proto_x_0" class="input_option">
						<option value="TCP">TCP</option>
						<option value="UDP">UDP</option>
						<option value="BOTH">BOTH</option>
						<option value="OTHER">OTHER</option>
					</select>
				</td>
				<td width="14%">
					<input type="button" class="add_btn" onClick="addRow_Group(128);" name="vts_rulelist2" value="">
				</td>
				</tr>
				</table>		
				
				<div id="vts_rulelist_Block"></div>
				
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
