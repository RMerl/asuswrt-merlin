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
<title><#Web_Title#> - <#BOP_isp_heart_item#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
.vpnbasic{
	display: none;
}
</style>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
/* Viz marked 2012.04 var pptpd_clientlist_array = '<% nvram_char_to_ascii("", "pptpd_clientlist"); %>';*/
var pptpd_clients = '<% nvram_get("pptpd_clients"); %>';

var pptpd_dns1_orig = '<% nvram_get("pptpd_dns1"); %>';
var pptpd_dns2_orig = '<% nvram_get("pptpd_dns2"); %>';
var pptpd_wins1_orig = '<% nvram_get("pptpd_wins1"); %>';
var pptpd_wins2_orig = '<% nvram_get("pptpd_wins2"); %>';

var origin_lan_ip = '<% nvram_get("lan_ipaddr"); %>';
var lan_ip_subnet = origin_lan_ip.split(".")[0]+"."+origin_lan_ip.split(".")[1]+"."+origin_lan_ip.split(".")[2]+".";
var lan_ip_end = parseInt(origin_lan_ip.split(".")[3]);
var alert_max = "<#vpn_max_clients#>";
var alert_over = "<#vlaue_haigher_than#> ";

var dhcp_enable = '<% nvram_get("dhcp_enable_x"); %>';
var pool_start = '<% nvram_get("dhcp_start"); %>';
var pool_end = '<% nvram_get("dhcp_end"); %>';
var pool_subnet = pool_start.split(".")[0]+"."+pool_start.split(".")[1]+"."+pool_start.split(".")[2]+".";
var pool_start_end = parseInt(pool_start.split(".")[3]);
var pool_end_end = parseInt(pool_end.split(".")[3]);

var static_enable = '<% nvram_get("dhcp_static_x"); %>';
var dhcp_staticlists = '<% nvram_get("dhcp_staticlist"); %>';
var staticclist_row = dhcp_staticlists.split('&#60');

function initial(){
	show_menu();
	//Viz marked 2012.04 pptpd_clientlist_array = decodeURIComponent(pptpd_clientlist_array);
	//Viz marked 2012.04  showpptpd_clientlist();
	check_dns_wins();	

	if (pptpd_clients != "") {
		document.form._pptpd_clients_start.value = pptpd_clients.split("-")[0];
		document.form._pptpd_clients_end.value = pptpd_clients.split("-")[1];
		$('pptpd_subnet').innerHTML = pptpd_clients.split(".")[0] + "." +
					      pptpd_clients.split(".")[1] + "." +
					      pptpd_clients.split(".")[2] + ".";
	}
	
	if (document.form.pptpd_mppe.value == 0)
		document.form.pptpd_mppe.value = (1 | 4 | 8);
	document.form.pptpd_mppe_128.checked = (document.form.pptpd_mppe.value & 1);
	//document.form.pptpd_mppe_56.checked = (document.form.pptpd_mppe.value & 2);
	document.form.pptpd_mppe_40.checked = (document.form.pptpd_mppe.value & 4);
	document.form.pptpd_mppe_no.checked = (document.form.pptpd_mppe.value & 8);

	check_vpn_conflict();
	
	addOnlineHelp($("faq"), ["ASUSWRT", "VPN"]);
}

function changeMppe(){
	if (!document.form.pptpd_mppe_128.checked &&
	    //!document.form.pptpd_mppe_56.checked &&
	    !document.form.pptpd_mppe_40.checked)
		document.form.pptpd_mppe_no.checked = true;
}

function applyRule(){
	/* Viz marked 2012.04
	var rule_num = $('pptpd_clientlist_table').rows.length;
	var item_num = $('pptpd_clientlist_table').rows[0].cells.length;
	var tmp_value = "";
	*/
	
	if(!valid_IP(document.form._pptpd_clients_start, "")){
			document.form._pptpd_clients_start.focus();
			document.form._pptpd_clients_start.select();
			return false;		
	}

	if(!validate_number_range(document.form._pptpd_clients_end, 1, 254)){
			document.form._pptpd_clients_end.focus();
			document.form._pptpd_clients_end.select();
			return false;
	}	
		
	/* Viz marked 2012.04 cuz these codes move to PPTP page
	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){																		//<td>			//<pre>
			tmp_value += $('pptpd_clientlist_table').rows[i].cells[j].childNodes[0].childNodes[0].nodeValue;
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	
	document.form.pptpd_clientlist.value = tmp_value;
	*/ //Viz marked 2012.04 
	

	document.form.pptpd_clients.value = document.form._pptpd_clients_start.value + "-" + document.form._pptpd_clients_end.value;

	document.form.pptpd_mppe.value = 0;
	if (document.form.pptpd_mppe_128.checked)
		document.form.pptpd_mppe.value |= 1;
	//if (document.form.pptpd_mppe_56.checked)
	//	document.form.pptpd_mppe.value |= 2;
	if (document.form.pptpd_mppe_40.checked)
		document.form.pptpd_mppe.value |= 4;
	if (document.form.pptpd_mppe_no.checked)
		document.form.pptpd_mppe.value |= 8;


	if(document.form.pptpd_dnsenable_x[0].checked == true){
				document.form.pptpd_dns1.value = "";
				document.form.pptpd_dns2.value = "";
	}else{
			if(document.form.pptpd_dns1.value == "" && document.form.pptpd_dns2.value == ""){
					alert("<#JS_fieldblank#>");
					document.form.pptpd_dns1.focus();
					document.form.pptpd_dns1.select();
					return false;	
			}
					
			if(!valid_IP_form(document.form.pptpd_dns1, 0) || !valid_IP_form(document.form.pptpd_dns2, 0))
					return false;
	}				
	
	if(document.form.pptpd_winsenable_x[0].checked == true){
				document.form.pptpd_wins1.value = "";
				document.form.pptpd_wins2.value = "";
	}else{
			if(document.form.pptpd_wins1.value == "" && document.form.pptpd_wins2.value == ""){				
					alert("<#JS_fieldblank#>");
					document.form.pptpd_wins1.focus();
					document.form.pptpd_wins1.select();					
					return false;	
			}		
		
			if(!valid_IP_form(document.form.pptpd_wins1, 0) || !valid_IP_form(document.form.pptpd_wins2, 0))
					return false;
	}		
	
	if(check_pptpd_clients_range() == false)
		return false;

	showLoading();
	document.form.submit();	
}

function addRow(obj, head){
	if(head == 1)
		pptpd_clientlist_array += "<" /*&#60*/
	else
		pptpd_clientlist_array += ">" /*&#62*/
			
	pptpd_clientlist_array += obj.value;

	obj.value = "";
}

function addRow_Group(upper){
	var rule_num = $('pptpd_clientlist_table').rows.length;
	var item_num = $('pptpd_clientlist_table').rows[0].cells.length;
		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}			
		
	if(document.form.pptpd_clientlist_username.value==""){
		alert("<#JS_fieldblank#>");
		document.form.pptpd_clientlist_username.focus();
		document.form.pptpd_clientlist_username.select();
		return false;
	}else if(document.form.pptpd_clientlist_password.value==""){
		alert("<#JS_fieldblank#>");
		document.form.pptpd_clientlist_password.focus();
		document.form.pptpd_clientlist_password.select();
		return false;
	}else{
		
		//Viz check same rule  //match(username) is not accepted
		if(item_num >=2){	
			for(i=0; i<rule_num; i++){	
					if(document.form.pptpd_clientlist_username.value.toLowerCase() == $('pptpd_clientlist_table').rows[i].cells[0].firstChild.innerHTML.toLowerCase()){
						alert("<#JS_duplicate#>");
						document.form.pptpd_clientlist_username.focus();
						document.form.pptpd_clientlist_username.select();
						return false;
					}	
			}
		}		
		
		if(!validate_string_ssid(document.form.pptpd_clientlist_username))
			return false;
		if(!validate_string_group(document.form.pptpd_clientlist_username))
			return false;

		if(!validate_string_ssid(document.form.pptpd_clientlist_password))
			return false;				
		if(!validate_string_group(document.form.pptpd_clientlist_password))
			return false;		
		
		addRow(document.form.pptpd_clientlist_username ,1);
		addRow(document.form.pptpd_clientlist_password, 0);
		showpptpd_clientlist();		
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('pptpd_clientlist_table').deleteRow(i);
  
  var pptpd_clientlist_value = "";
	for(k=0; k<$('pptpd_clientlist_table').rows.length; k++){
		for(j=0; j<$('pptpd_clientlist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				pptpd_clientlist_value += "&#60";
			else
				pptpd_clientlist_value += "&#62";
			pptpd_clientlist_value += $('pptpd_clientlist_table').rows[k].cells[j].innerHTML;		
		}
	}
	
	pptpd_clientlist_array = pptpd_clientlist_value;
	if(pptpd_clientlist_array == "")
		showpptpd_clientlist();
}

function edit_Row(r){ 	
	var i=r.parentNode.parentNode.rowIndex;
	document.form.pptpd_clientlist_username.value = $('pptpd_clientlist_table').rows[i].cells[0].innerHTML;
	document.form.pptpd_clientlist_password.value = $('pptpd_clientlist_table').rows[i].cells[1].innerHTML; 	
  del_Row(r);	
}

function showpptpd_clientlist(){
	var pptpd_clientlist_row = pptpd_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="pptpd_clientlist_table">';
	if(pptpd_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < pptpd_clientlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var pptpd_clientlist_col = pptpd_clientlist_row[i].split('>');
				for(var j = 0; j < pptpd_clientlist_col.length; j++){
					code +='<td width="40%"><pre>'+ pptpd_clientlist_col[j] +'</pre></td>';		//IP  width="98"
				}
				code +='<td width="20%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	$("pptpd_clientlist_Block").innerHTML = code;
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
			alert(obj_name.value+" <#JS_validip#>");
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

function setEnd(){
	var end="";
	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0]
															+"."+document.form._pptpd_clients_start.value.split(".")[1]
															+"."+document.form._pptpd_clients_start.value.split(".")[2]
															+".";
  var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);															
  															
	$('pptpd_subnet').innerHTML = pptpd_clients_subnet;
	
	end = pptpd_clients_start_ip+9;
	if(end >254)	end = 254;
	
	if(!end)
		document.form._pptpd_clients_end.value = "";
	else	
		document.form._pptpd_clients_end.value = end;
}

function check_vpn_conflict(){		//if conflict with LAN ip & DHCP ip pool & static
	
	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0] + "." +
				   document.form._pptpd_clients_start.value.split(".")[1] + "." +
				   document.form._pptpd_clients_start.value.split(".")[2] + ".";
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);
	
  //LAN ip
  if(lan_ip_subnet == pptpd_clients_subnet 
  		&& lan_ip_end >= pptpd_clients_start_ip 
  		&& lan_ip_end <= pptpd_clients_end_ip ){
  		$('pptpd_conflict').innerHTML = "<#vpn_conflict_LANIP#> <b>"+origin_lan_ip+"</b>";
  		return;
  }
	//DHCP pool
	if(pool_subnet == pptpd_clients_subnet
					&& ((pool_start_end >= pptpd_clients_start_ip && pool_start_end <= pptpd_clients_end_ip)								
								|| (pool_end_end >= pptpd_clients_start_ip && pool_end_end <= pptpd_clients_end_ip)								
								|| (pptpd_clients_start_ip >= pool_start_end && pptpd_clients_start_ip <= pool_end_end)
								|| (pptpd_clients_end_ip >= pool_start_end && pptpd_clients_end_ip <= pool_end_end))
					){
  		$('pptpd_conflict').innerHTML = "<#vpn_conflict_DHCPpool#> <b>"+pool_start+" ~ "+pool_end+"</b>";
  		return;
	}
	//DHCP static IP
	if(dhcp_staticlists != ""){
			for(var i = 1; i < staticclist_row.length; i++){
					var static_subnet ="";
					var static_end ="";					
					var static_ip = staticclist_row[i].split('&#62')[1];
					static_subnet = static_ip.split(".")[0]+"."+static_ip.split(".")[1]+"."+static_ip.split(".")[2]+".";
					static_end = parseInt(static_ip.split(".")[3]);
					if(static_subnet == pptpd_clients_subnet 
  						&& static_end >= pptpd_clients_start_ip 
  						&& static_end <= pptpd_clients_end_ip){
  							$('pptpd_conflict').innerHTML = "<#vpn_conflict_DHCPstatic#> <b>"+static_ip+"</b>";
  							return;
  				}				
  		}
	}
		
	$('pptpd_conflict').innerHTML = "";	
}

function check_pptpd_clients_range(){
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);
	
	if(pptpd_clients_start_ip > pptpd_clients_end_ip){
		alert(alert_over+document.form._pptpd_clients_start.value);
		document.form._pptpd_clients_start.focus();
    document.form._pptpd_clients_start.select();
    setEnd();
		return false;
	}
	
  if( (pptpd_clients_end_ip - pptpd_clients_start_ip) > 9 ){
      alert(alert_max);
			document.form._pptpd_clients_start.focus();
    	document.form._pptpd_clients_start.select();
    	setEnd();
      return false;
  }	
	
	return true;
}

function change_pptpd_radio(obj){
		if(obj.name == 'pptpd_dnsenable_x'){
				if(obj.value == 1){
						document.form.pptpd_dns1.parentNode.parentNode.style.display = "none";
						document.form.pptpd_dns2.parentNode.parentNode.style.display = "none";
				}else{
						document.form.pptpd_dns1.parentNode.parentNode.style.display = "";
						document.form.pptpd_dns2.parentNode.parentNode.style.display = "";					
				}
		}else if(obj.name == 'pptpd_winsenable_x'){
				if(obj.value == 1){
						document.form.pptpd_wins1.parentNode.parentNode.style.display = "none";
						document.form.pptpd_wins2.parentNode.parentNode.style.display = "none";
				}else{
						document.form.pptpd_wins1.parentNode.parentNode.style.display = "";
						document.form.pptpd_wins2.parentNode.parentNode.style.display = "";					
				}			
		}	
}

function check_dns_wins(){
		if(pptpd_dns1_orig == "" && pptpd_dns2_orig == ""){
				document.form.pptpd_dnsenable_x[0].checked = true;
				change_pptpd_radio(document.form.pptpd_dnsenable_x[0]);
		}else{
				document.form.pptpd_dnsenable_x[1].checked = true;
				change_pptpd_radio(document.form.pptpd_dnsenable_x[1]);
		}		
		
		
		if(pptpd_wins1_orig == "" && pptpd_wins2_orig == ""){
				document.form.pptpd_winsenable_x[0].checked = true;
				change_pptpd_radio(document.form.pptpd_winsenable_x[0]);
		}else{
				document.form.pptpd_winsenable_x[1].checked = true;
				change_pptpd_radio(document.form.pptpd_winsenable_x[1]);
		}		
		change_pptpd_radio(document.form.pptpd_winsenable_x);	
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
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
			<input type="hidden" name="current_page" value="Advanced_PPTPAdvanced_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_PPTPAdvanced_Content.asp">
			<input type="hidden" name="next_host" value="">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="restart_pptpd">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
			<input type="hidden" name="pptpd_clientlist" value="<% nvram_char_to_ascii("","pptpd_clientlist"); %>" disabled>
			<input type="hidden" name="pptpd_clients" value="<% nvram_get("pptpd_clients"); %>">
			<input type="hidden" name="pptpd_mppe" value="<% nvram_get("pptpd_mppe"); %>">	

			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" valign="top">
								  <div>&nbsp;</div>
								  <div class="formfonttitle"><#BOP_isp_heart_item#> - <#vpn_Adv#></div>
								  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								  <div class="formfontdesc"><#PPTP_desc#></div>
								  <!--div class="formfontdesc"><#PPTP_desc2#> <% nvram_get("wan0_ipaddr"); %></div>
								  <div class="formfontdesc" style="margin-top:-10px;font-weight: bolder;"><#PPTP_desc3#></div>
									<div class="formfontdesc" style="margin-top:-10px;">
										(7) <a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;">VPN server FAQ</a>
									</div-->

									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									  	<thead>
									  		<tr>
												<td colspan="3" id="GWStatic"><#t2BC#></td>
									  		</tr>
									  	</thead>
										
										<tr class="vpnbasic">
											<th><#vpn_enable#></th>
											<td>
												<select name="pptpd_enable" class="input_option" disabled>
													<option class="content_input_fd" value="0" <% nvram_match("pptpd_enable", "0","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="1"<% nvram_match("pptpd_enable", "1","selected"); %>><#btn_Enable#></option>
												</select>			
											</td>
									  </tr>
							
										<tr>
											<th><#vpn_broadcast#></th>
											<td>
												<select name="pptpd_broadcast" class="input_option">
													<option class="content_input_fd" value="disable" <% nvram_match("pptpd_broadcast", "disable","selected"); %>><#btn_disable#></option>
													<option class="content_input_fd" value="br0"<% nvram_match("pptpd_broadcast", "br0","selected"); %>>LAN to VPN Client</option>
                          <option class="content_input_fd" value="ppp" <% nvram_match("pptpd_broadcast", "ppp","selected"); %>>VPN Client to LAN</option>
													<option class="content_input_fd" value="br0ppp"<% nvram_match("pptpd_broadcast", "br0ppp","selected"); %>>Both</option>
												</select>			
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
												<!--input type="checkbox" class="input" name="pptpd_mppe_56" onClick="return changeMppe();">MPPE-56<br-->
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
			            		<th><a class="hintstyle" href="javascript:void(0);"><#IPConnection_x_DNSServer1_itemname#></a></th>
			            		<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns1" value="<% nvram_get("pptpd_dns1"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
			          		</tr>

			          		<tr>
			            		<th><a class="hintstyle" href="javascript:void(0);"><#IPConnection_x_DNSServer2_itemname#></a></th>
			            		<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns2" value="<% nvram_get("pptpd_dns2"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
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
			            		<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins1" value="<% nvram_get("pptpd_wins1"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
			          		</tr>

			          		<tr>
			            		<th><#IPConnection_x_WINSServer2_itemname#></th>
			            		<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins2" value="<% nvram_get("pptpd_wins2"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
			          		</tr>

			          		<tr>
			            		<th><#vpn_client_ip#></th>
			            		<td>
                          <input type="text" maxlength="15" class="input_15_table" name="_pptpd_clients_start" onBlur="setEnd();check_pptpd_clients_range();check_vpn_conflict();"  onKeyPress="return is_ipaddr(this, event);" value=""/> ~
                          <span id="pptpd_subnet" style="font-family: Lucida Console;color: #FFF;"></span><input type="text" maxlength="3" class="input_3_table" name="_pptpd_clients_end" onBlur="check_pptpd_clients_range();check_vpn_conflict();" value=""/><span style="color:#FFCC00;"> <#vpn_maximum_clients#></span>
                          <br><span id="pptpd_conflict"></span>	
											</td>
			          		</tr>

									</table>
<div class="vpnbasic">
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
									  	<thead>
									  		<tr>
												<td colspan="3" id="GWStatic"><#Username_Pwd#></td>
									  		</tr>
									  	</thead>
									  
									  	<tr>
								  			<th><#PPPConnection_UserName_itemname#></th>
						        		<th><#PPPConnection_Password_itemname#></th>
						        		<th>Add / Delete</th>
									  	</tr>			  
									  	<tr>
						          	<td width="40%">
						              <input type="text" class="input_25_table" maxlength="32" name="pptpd_clientlist_username" onKeyDown="return is_string_down(this, event)">
						            </td>
						            			<td width="40%">
						            				<input type="text" class="input_25_table" maxlength="32" name="pptpd_clientlist_password">
						            			</td>
						            			<td width="20%">
																<div> 
																	<input type="button" class="add_btn" onClick="addRow_Group(32);" value="">
																</div>
						            			</td>
									  	</tr>	 			  
								  </table>        			
						        			
				  <div id="pptpd_clientlist_Block"></div>
</div>        			
        	<!-- manually assigned the DHCP List end-->		
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
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
