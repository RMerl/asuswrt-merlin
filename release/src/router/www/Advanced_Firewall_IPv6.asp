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
<title><#Web_Title#> - IPv6 Firewall</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
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
	new Array("SNMP TRAP", "162", "UDP"));

<% login_state_hook(); %>

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var overlib_str0 = new Array();
var overlib_str = new Array();

var ipv6_fw_rulelist_array = "<% nvram_char_to_ascii("","ipv6_fw_rulelist"); %>";

function initial(){
	show_menu();
	loadAppOptions();
	showipv6_fw_rulelist();
}

function isChange(){
	if((document.form.ipv6_fw_enable[0].checked == true && '<% nvram_get("ipv6_fw_enable"); %>' == '0') || 
				(document.form.ipv6_fw_enable[1].checked == true && '<% nvram_get("ipv6_fw_enable"); %>' == '1')){
		return true;
	}
	else if(document.form.ipv6_fw_rulelist.value != decodeURIComponent('<% nvram_char_to_ascii("","ipv6_fw_rulelist"); %>')){
		return true;
	}
	else
		return false;
}

function applyRule(){

	var rule_num = $('ipv6_fw_rulelist_table').rows.length;
	var item_num = $('ipv6_fw_rulelist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){			
		
			if($('ipv6_fw_rulelist_table').rows[i].cells[j].innerHTML.lastIndexOf("...")<0){
				tmp_value += $('ipv6_fw_rulelist_table').rows[i].cells[j].innerHTML;
			}else{
				tmp_value += $('ipv6_fw_rulelist_table').rows[i].cells[j].title;
			}		
			
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}

	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	

	document.form.ipv6_fw_rulelist.value = tmp_value;

	showLoading();
	document.form.submit();
}

function loadAppOptions(){
	free_options(document.form.KnownApps);
	add_option(document.form.KnownApps, "<#Select_menu_default#>", 0, 1);
	for(var i = 1; i < wItem.length; i++)
		add_option(document.form.KnownApps, wItem[i][0], i, 0);
}


function change_wizard(o, id){
	for(var i = 0; i < wItem.length; ++i){
		if(wItem[i][0] != null && o.value == i){
			if(wItem[i][2] == "TCP")
				document.form.ipv6_fw_proto_x_0.options[0].selected = 1;
			else if(wItem[i][2] == "UDP")
				document.form.ipv6_fw_proto_x_0.options[1].selected = 1;
			else if(wItem[i][2] == "BOTH")
				document.form.ipv6_fw_proto_x_0.options[2].selected = 1;
			else if(wItem[i][2] == "OTHER")
				document.form.ipv6_fw_proto_x_0.options[3].selected = 1;
				
			document.form.ipv6_fw_port_x_0.value = wItem[i][1];
			document.form.ipv6_fw_desc_x_0.value = wItem[i][0]+" Server";				
			break;
		}
	}
}

function addRow(obj, head){
	if(head == 1)
		ipv6_fw_rulelist_array += "<"
	else
		ipv6_fw_rulelist_array += ">"
			
	ipv6_fw_rulelist_array += obj.value;
	obj.value = "";
}

function validForm(){
	if(!Block_chars(document.form.ipv6_fw_desc_x_0, ["<" ,">" ,"'" ,"%"])){
				return false;		
	}	

	if(!Block_chars(document.form.ipv6_fw_port_x_0, ["<" ,">"])){
				return false;		
	}	

	if(document.form.ipv6_fw_proto_x_0.value=="OTHER"){
		if (!check_multi_range(document.form.ipv6_fw_port_x_0, 1, 255, false))
			return false;
	}

	if(!check_multi_range(document.form.ipv6_fw_port_x_0, 1, 65535, true)){
		return false;
	}

	if(document.form.ipv6_fw_lipaddr_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.ipv6_fw_lipaddr_x_0.focus();
		document.form.ipv6_fw_lipaddr_x_0.select();		
		return false;
	}

	if(document.form.ipv6_fw_port_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.ipv6_fw_port_x_0.focus();
		document.form.ipv6_fw_port_x_0.select();		
		return false;
	}

	if(!validate_multi_range(document.form.ipv6_fw_port_x_0, 1, 65535)
		|| !ipv6_valid(document.form.ipv6_fw_lipaddr_x_0, 0)
		|| (document.form.ipv6_fw_ripaddr_x_0.value != "" && !ipv6_valid(document.form.ipv6_fw_ripaddr_x_0, 1))) {
		return false;
	}
	
	return true;
}

function addRow_Group(upper){
	if(validForm()){
		if('<% nvram_get("ipv6_fw_enable"); %>' != "1")
			document.form.ipv6_fw_enable[0].checked = true;
		
		var rule_num = $('ipv6_fw_rulelist_table').rows.length;
		var item_num = $('ipv6_fw_rulelist_table').rows[0].cells.length;	
		if(rule_num >= upper){
				alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
				return;
		}	

		addRow(document.form.ipv6_fw_desc_x_0 ,1);
		addRow(document.form.ipv6_fw_ripaddr_x_0, 0);
		addRow(document.form.ipv6_fw_lipaddr_x_0, 0);
		addRow(document.form.ipv6_fw_port_x_0, 0);
		addRow(document.form.ipv6_fw_proto_x_0, 0);
		document.form.ipv6_fw_proto_x_0.value="TCP";
		showipv6_fw_rulelist();
	}
}

function validate_multi_range(val, mini, maxi){
	var rangere=new RegExp("^([0-9]{1,5})\:([0-9]{1,5})$", "gi");
	if(rangere.test(val)){
		
		if(!validate_each_port(document.form.ipv6_fw_port_x_0, RegExp.$1, mini, maxi) || !validate_each_port(document.form.ipv6_fw_port_x_0, RegExp.$2, mini, maxi)){
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
	obj.value = document.form.ipv6_fw_port_x_0.value.replace(/[-~]/gi,":");	// "~-" to ":"
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
	document.form.ipv6_fw_port_x_0.value = parse_port;
	parse_port ="";
	return true;	
}


function edit_Row(r){ 	
	var i=r.parentNode.parentNode.rowIndex;
  	
	document.form.ipv6_fw_desc_x_0.value = $('ipv6_fw_rulelist_table').rows[i].cells[0].innerHTML;
	document.form.ipv6_fw_ripaddr_x_0.value = $('ipv6_fw_rulelist_table').rows[i].cells[1].innerHTML; 
	document.form.ipv6_fw_lipaddr_x_0.value = $('ipv6_fw_rulelist_table').rows[i].cells[2].innerHTML;
	document.form.ipv6_fw_port_x_0.value = $('ipv6_fw_rulelist_table').rows[i].cells[3].innerHTML;
	document.form.ipv6_fw_proto_x_0.value = $('ipv6_fw_rulelist_table').rows[i].cells[4].innerHTML;

	del_Row(r);	
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('ipv6_fw_rulelist_table').deleteRow(i);
  
  var ipv6_fw_rulelist_value = "";
	for(k=0; k<$('ipv6_fw_rulelist_table').rows.length; k++){
		for(j=0; j<$('ipv6_fw_rulelist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				ipv6_fw_rulelist_value += "<";
			else
				ipv6_fw_rulelist_value += ">";
				
			if($('ipv6_fw_rulelist_table').rows[k].cells[j].innerHTML.lastIndexOf("...")<0){
				ipv6_fw_rulelist_value += $('ipv6_fw_rulelist_table').rows[k].cells[j].innerHTML;
			}else{
				ipv6_fw_rulelist_value += $('ipv6_fw_rulelist_table').rows[k].cells[j].title;
			}			
		}
	}
	
	ipv6_fw_rulelist_array = ipv6_fw_rulelist_value;
	if(ipv6_fw_rulelist_array == "")
		showipv6_fw_rulelist();
}

function showipv6_fw_rulelist(){
	var ipv6_fw_rulelist_row = decodeURIComponent(ipv6_fw_rulelist_array).split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="ipv6_fw_rulelist_table">';
	if(ipv6_fw_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < ipv6_fw_rulelist_row.length; i++){
			overlib_str0[i] ="";
			overlib_str[i] ="";			
			code +='<tr id="row'+i+'">';
			var ipv6_fw_rulelist_col = ipv6_fw_rulelist_row[i].split('>');
			var wid=[15, 24, 24, 14, 11];
				for(var j = 0; j < ipv6_fw_rulelist_col.length; j++){
						if(j==0){
							if(ipv6_fw_rulelist_col[0].length >10){
								overlib_str0[i] += ipv6_fw_rulelist_col[0];
								ipv6_fw_rulelist_col[0]=ipv6_fw_rulelist_col[0].substring(0, 8)+"...";
								code +='<td width="'+wid[j]+'%" title="'+overlib_str0[i]+'">'+ ipv6_fw_rulelist_col[0] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%">'+ ipv6_fw_rulelist_col[j] +'</td>';
						}else if(j==1){
							if(ipv6_fw_rulelist_col[1].length >22){
								overlib_str[i] += ipv6_fw_rulelist_col[1];
								ipv6_fw_rulelist_col[1]=ipv6_fw_rulelist_col[1].substring(0, 20)+"...";
								code +='<td width="'+wid[j]+'%" title='+overlib_str[i]+'>'+ ipv6_fw_rulelist_col[1] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%">'+ ipv6_fw_rulelist_col[j] +'</td>';
                                                }else if(j==2){
                                                        if(ipv6_fw_rulelist_col[2].length >22){
                                                                overlib_str[i] += ipv6_fw_rulelist_col[2];
                                                                ipv6_fw_rulelist_col[2]=ipv6_fw_rulelist_col[2].substring(0, 20)+"...";
                                                                code +='<td width="'+wid[j]+'%" title='+overlib_str[i]+'>'+ ipv6_fw_rulelist_col[2] +'</td>';
                                                        }else
                                                                code +='<td width="'+wid[j]+'%">'+ ipv6_fw_rulelist_col[j] +'</td>';
                                                }else if(j==3){
                                                        if(ipv6_fw_rulelist_col[3].length >12){
                                                                overlib_str[i] += ipv6_fw_rulelist_col[3];
                                                                ipv6_fw_rulelist_col[3]=ipv6_fw_rulelist_col[3].substring(0, 10)+"...";
                                                                code +='<td width="'+wid[j]+'%" title='+overlib_str[i]+'>'+ ipv6_fw_rulelist_col[3] +'</td>';
                                                        }else
                                                                code +='<td width="'+wid[j]+'%">'+ ipv6_fw_rulelist_col[j] +'</td>';
						}else{
							code +='<td width="'+wid[j]+'%">'+ ipv6_fw_rulelist_col[j] +'</td>';
						}
						
				}
				code +='<td width="12%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
	code +='</table>';
	$("ipv6_fw_rulelist_Block").innerHTML = code;	     
}


function ipv6_valid(obj, cidr){

	var rangere_cidr=new RegExp("^\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\s*(\/(\d|\d\d|1[0-1]\d|12[0-8]))$", "gi");
	var rangere=new RegExp("^\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\s*", "gi");

	if((rangere.test(obj.value)) || (cidr == 1 && rangere_cidr.test(obj.value))) {
		//alert(obj.value+"good");
		return true;
	}else{
		alert(obj.value+" <#JS_validip#>");
		obj.focus();
		obj.select();
		return false;
	}
}


function changeBgColor(obj, num){
	if(obj.checked)
 		$("row" + num).style.background='#FF9';
	else
 		$("row" + num).style.background='#FFF';
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame" >
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Firewall_IPv6.asp">
<input type="hidden" name="next_page" value="Advanced_Firewall_IPv6.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="ipv6_fw_rulelist" value=''>

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
		<div class="formfonttitle"><#menu5_5#> - IPv6 Firewall</div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<div>
			<div class="formfontdesc">All outbound traffic coming from IPv6 hosts on your LAN is allowed, as well
                        as related inbound traffic.  Any other inbound traffic must be specifically allowed here.</div>
			<div class="formfontdesc">You can leave the remote IP empty to allow traffic from any remote host.
                        A subnet can also be specified (2001::111:222:333/64 for example).
		</div>
			
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
					  <thead>
					  <tr>
						<td colspan="4"><#t2BC#></td>
					  </tr>
					  </thead>

          	<tr>
            	<th>Enable IPv6 Firewall</th>
            	<td>
            		<input type="radio" value="1" name="ipv6_fw_enable" <% nvram_match("ipv6_fw_enable", "1", "checked"); %>><#checkbox_Yes#>
            		<input type="radio" value="0" name="ipv6_fw_enable" <% nvram_match("ipv6_fw_enable", "0", "checked"); %>><#checkbox_No#>
		</td>
		</tr>
		<tr>
			<th><#IPConnection_VSList_groupitemdesc#></th>
			<td id="ipv6_fw_rulelist">
			  		<select name="KnownApps" id="KnownApps" class="input_option" onchange="change_wizard(this, 'KnownApps');">
			  		</select>
			</td>
		</tr>
		  
      	</table>			
      	
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
			<thead>
			<tr>
              		<td colspan="7">Inbound Firewall Rules&nbsp;(<#List_limit#>&nbsp;128)</td>
            	</tr>
 		  	</thead>
 		  	
          		<tr>
				<th><#BM_UserList1#></th>
				<th>Remote IP/CIDR</th>
	            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,25);"><#IPConnection_VServerIP_itemname#></a></th>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,24);"><#FirewallConfig_LanWanSrcPort_itemname#></a></th>
	            		<th><#IPConnection_VServerProto_itemname#></th>
				<th>Add / Delete</th>
          		</tr>  
          		        
          		<tr>
  				<td width="15%">
  					<input type="text" maxlength="30" class="input_12_table" name="ipv6_fw_desc_x_0" onKeyPress="return is_string(this, event)"/>
  				</td>
				<td width="24%">
					<input type="text" maxlength="45" class="input_18_table" name="ipv6_fw_ripaddr_x_0" align="left" style="float:left;" autocomplete="off">
				</td>
				<td width="24%">
					<input type="text" maxlength="45" class="input_18_table" name="ipv6_fw_lipaddr_x_0" align="left" style="float:left;" autocomplete="off">
                                </td>
				<td width="14%">
					<input type="text" maxlength="" class="input_12_table" name="ipv6_fw_port_x_0" onkeypress="return is_portrange(this, event)"/>
				</td>
				<td width="11%">
					<select name="ipv6_fw_proto_x_0" class="input_option">
						<option value="TCP">TCP</option>
						<option value="UDP">UDP</option>
						<option value="BOTH">BOTH</option>
						<option value="OTHER">OTHER</option>
					</select>
				</td>
				<td width="12%">
					<input type="button" class="add_btn" onClick="addRow_Group(128);" name="ipv6_fw_rulelist2" value="">
				</td>
				</tr>
				</table>		
				
				<div id="ipv6_fw_rulelist_Block"></div>
				
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
