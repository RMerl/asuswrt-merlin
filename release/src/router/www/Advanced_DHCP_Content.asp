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

<title><#Web_Title#> - <#menu5_2_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:27px;
	margin-left:10px;
	*margin-left:-263px;
	width:255px;
	*width:270px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>
<script>
var dhcp_staticlist_array = '<% nvram_get("dhcp_staticlist"); %>';

if(pptpd_support){
	var pptpd_clients = '<% nvram_get("pptpd_clients"); %>';
	var pptpd_clients_subnet = pptpd_clients.split(".")[0]+"."
				+pptpd_clients.split(".")[1]+"."
				+pptpd_clients.split(".")[2]+".";

	var pptpd_clients_start_ip = parseInt(pptpd_clients.split(".")[3].split("-")[0]);
	var pptpd_clients_end_ip = parseInt(pptpd_clients.split("-")[1]);
}

var dhcp_enable = '<% nvram_get("dhcp_enable_x"); %>';
var pool_start = '<% nvram_get("dhcp_start"); %>';
var pool_end = '<% nvram_get("dhcp_end"); %>';
var pool_subnet = pool_start.split(".")[0]+"."+pool_start.split(".")[1]+"."+pool_start.split(".")[2]+".";
var pool_start_end = parseInt(pool_start.split(".")[3]);
var pool_end_end = parseInt(pool_end.split(".")[3]);

var static_enable = '<% nvram_get("dhcp_static_x"); %>';
var dhcp_staticlists = '<% nvram_get("dhcp_staticlist"); %>';
var staticclist_row = dhcp_staticlists.split('&#60');

var lan_domain_curr = '<% nvram_get("lan_domain"); %>';
var dhcp_gateway_curr = '<% nvram_get("dhcp_gateway_x"); %>';
var dhcp_dns1_curr = '<% nvram_get("dhcp_dns1_x"); %>';
var dhcp_dns2_curr = '<% nvram_get("dhcp_dns2_x"); %>';
var dhcp_wins_curr = '<% nvram_get("dhcp_wins_x"); %>';

if(yadns_support){
	var yadns_enable = '<% nvram_get("yadns_enable_x"); %>';
	var yadns_mode = '<% nvram_get("yadns_mode"); %>';
}

var backup_mac = "";
var backup_ip = "";
var backup_name = "";

function initial(){
	show_menu();
	//Viz 2011.10{ for LAN ip in DHCP pool or Static list
	showtext(document.getElementById("LANIP"), '<% nvram_get("lan_ipaddr"); %>');
	if((inet_network(document.form.lan_ipaddr.value)>=inet_network(document.form.dhcp_start.value))&&(inet_network(document.form.lan_ipaddr.value)<=inet_network(document.form.dhcp_end.value))){
			document.getElementById('router_in_pool').style.display="";
	}else if(dhcp_staticlists != ""){
			for(var i = 1; i < staticclist_row.length; i++){
					var static_ip = staticclist_row[i].split('&#62')[1];
					if(static_ip == document.form.lan_ipaddr.value){
								document.getElementById('router_in_pool').style.display="";
  				}
			}
	}
	//}Viz 2011.10
	showdhcp_staticlist();
	setTimeout("showLANIPList();", 1000);

	if(pptpd_support){
		var chk_vpn = check_vpn();
		if(chk_vpn == true){
	 		document.getElementById("VPN_conflict").style.display = "";
	 		document.getElementById("VPN_conflict_span").innerHTML = "<#vpn_conflict_dhcp#>"+pptpd_clients;
		}
	}

	if(yadns_support){
		if(yadns_enable != 0 && yadns_mode != -1){
			document.getElementById("yadns_hint").style.display = "";
			document.getElementById("yadns_hint").innerHTML = "<span><#YandexDNS_settings_hint#></span>";
		}
	}

	//Viz keep this, disabled temporarily. if(!tmo_support){
			document.form.sip_server.disabled = true;
			document.form.sip_server.parentNode.parentNode.style.display = "none";
	//}else{
	//		document.form.sip_server.disabled = false;
	//		document.form.sip_server.parentNode.parentNode.style.display = "";
	//}

	addOnlineHelp(document.getElementById("faq"), ["set", "up", "specific", "IP", "address"]);
}

function addRow(obj, head){
	if(head == 1)
		dhcp_staticlist_array += "&#60"
	else
		dhcp_staticlist_array += "&#62"

	dhcp_staticlist_array += obj.value;

	obj.value = "";
}

function addRow_Group(upper){
	if(dhcp_enable != "1")
		document.form.dhcp_enable_x[0].checked = true;
	if(static_enable != "1")
		document.form.dhcp_static_x[0].checked = true;

	var rule_num = document.getElementById('dhcp_staticlist_table').rows.length;
	var item_num = document.getElementById('dhcp_staticlist_table').rows[0].cells.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.form.dhcp_staticmac_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.dhcp_staticmac_x_0.focus();
		document.form.dhcp_staticmac_x_0.select();
		return false;
	}else if(document.form.dhcp_staticip_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.dhcp_staticip_x_0.focus();
		document.form.dhcp_staticip_x_0.select();
		return false;
	}else if(check_macaddr(document.form.dhcp_staticmac_x_0, check_hwaddr_flag(document.form.dhcp_staticmac_x_0)) == true &&
		 validator.validIPForm(document.form.dhcp_staticip_x_0,0) == true &&
		 validate_dhcp_range(document.form.dhcp_staticip_x_0) == true){

		//Viz check same rule  //match(ip or mac) is not accepted
		if(item_num >=2){
			for(i=0; i<rule_num; i++){
					if(document.form.dhcp_staticmac_x_0.value.toLowerCase() == document.getElementById('dhcp_staticlist_table').rows[i].cells[0].innerHTML.toLowerCase()){
						alert("<#JS_duplicate#>");
						document.form.dhcp_staticmac_x_0.focus();
						document.form.dhcp_staticmac_x_0.select();
						return false;
					}
					if(document.form.dhcp_staticip_x_0.value == document.getElementById('dhcp_staticlist_table').rows[i].cells[1].innerHTML){
						alert("<#JS_duplicate#>");
						document.form.dhcp_staticip_x_0.focus();
						document.form.dhcp_staticip_x_0.select();
						return false;
					}
			}
		}

		addRow(document.form.dhcp_staticmac_x_0 ,1);
		addRow(document.form.dhcp_staticip_x_0, 0);
		addRow(document.form.dhcp_staticname_x_0, 0);

		showdhcp_staticlist();

		if (backup_mac != "") {
			backup_mac = "";
			backup_ip = "";
			backup_name = "";
			document.getElementById('dhcp_staticlist_table').rows[rule_num-1].scrollIntoView();
		}
	}else{
		return false;
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById('dhcp_staticlist_table').deleteRow(i);

  var dhcp_staticlist_value = "";
	for(k=0; k<document.getElementById('dhcp_staticlist_table').rows.length; k++){
		for(j=0; j<document.getElementById('dhcp_staticlist_table').rows[k].cells.length-1; j++){
			if(j == 0)
				dhcp_staticlist_value += "&#60";
			else
				dhcp_staticlist_value += "&#62";
			dhcp_staticlist_value += document.getElementById('dhcp_staticlist_table').rows[k].cells[j].innerHTML;
		}
	}

	dhcp_staticlist_array = dhcp_staticlist_value;
	if(dhcp_staticlist_array == "")
		showdhcp_staticlist();
}

function edit_Row(r){
	cancel_Edit();

	var i=r.parentNode.parentNode.rowIndex;
	document.form.dhcp_staticmac_x_0.value = document.getElementById('dhcp_staticlist_table').rows[i].cells[0].innerHTML;
	document.form.dhcp_staticip_x_0.value = document.getElementById('dhcp_staticlist_table').rows[i].cells[1].innerHTML;
	document.form.dhcp_staticname_x_0.value = document.getElementById('dhcp_staticlist_table').rows[i].cells[2].innerHTML;
	backup_mac = document.form.dhcp_staticmac_x_0.value;
	backup_ip = document.form.dhcp_staticip_x_0.value;
	backup_name = document.form.dhcp_staticname_x_0.value;
 	del_Row(r);
	document.form.dhcp_staticmac_x_0.focus();
}

function cancel_Edit(){
	if (backup_mac != "") {
		document.form.dhcp_staticmac_x_0.value = backup_mac;
		document.form.dhcp_staticip_x_0.value = backup_ip;
		document.form.dhcp_staticname_x_0.value = backup_name;
		addRow_Group(128);
	}
}

function showdhcp_staticlist(){
	var dhcp_staticlist_row = dhcp_staticlist_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="dhcp_staticlist_table">';
	if(dhcp_staticlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < dhcp_staticlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var dhcp_staticlist_col = dhcp_staticlist_row[i].split('&#62');
				for(var j = 0; j < dhcp_staticlist_col.length; j++){
					code +='<td width="28%">'+ dhcp_staticlist_col[j] +'</td>';		//IP  width="98"
				}
				if (j !=3) code +='<td width="28%"></td>';
				code +='<td width="16%"><input class="edit_btn" onclick="edit_Row(this);" value=""/>';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
	code +='</table>';
	document.getElementById("dhcp_staticlist_Block").innerHTML = code;
}

function applyRule(){
	cancel_Edit();

	if(validForm()){
		var rule_num = document.getElementById('dhcp_staticlist_table').rows.length;
		var item_num = document.getElementById('dhcp_staticlist_table').rows[0].cells.length;
		var tmp_value = "";

		for(i=0; i<rule_num; i++){
			tmp_value += "<";
			for(j=0; j<item_num-1; j++){
				tmp_value += document.getElementById('dhcp_staticlist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";

		document.form.dhcp_staticlist.value = tmp_value;

		// Only restart the whole network if needed
		if ((document.form.dhcp_wins_x.value != dhcp_wins_curr) ||
		    (document.form.dhcp_dns1_x.value != dhcp_dns1_curr) ||
		    (document.form.dhcp_dns2_x.value != dhcp_dns2_curr) ||
		    (document.form.dhcp_gateway_x.value != dhcp_gateway_curr) ||
		    (document.form.lan_domain.value != lan_domain_curr)) {

			document.form.action_script.value = "restart_net_and_phy";

		} else {
			document.form.action_script.value = "restart_dnsmasq";
			document.form.action_wait.value = 5;
		}

		showLoading();
		document.form.submit();
	}
}

function validate_dhcp_range(ip_obj){
	var ip_num = inet_network(ip_obj.value);
	var subnet_head, subnet_end;

	if(ip_num <= 0){
		alert(ip_obj.value+" <#JS_validip#>");
		ip_obj.value = "";
		ip_obj.focus();
		ip_obj.select();
		return 0;
	}

	subnet_head = getSubnet(document.form.lan_ipaddr.value, document.form.lan_netmask.value, "head");
	subnet_end = getSubnet(document.form.lan_ipaddr.value, document.form.lan_netmask.value, "end");

	if(ip_num <= subnet_head || ip_num >= subnet_end){
		alert(ip_obj.value+" <#JS_validip#>");
		ip_obj.value = "";
		ip_obj.focus();
		ip_obj.select();
		return 0;
	}

	return 1;
}

function validForm(){
	var re = new RegExp('^[a-zA-Z0-9][a-zA-Z0-9\.\-]*[a-zA-Z0-9]$','gi');
 	if((!re.test(document.form.lan_domain.value) || document.form.lan_domain.value.indexOf("asuscomm.com") > 0) && document.form.lan_domain.value != ""){
	alert("<#JS_validchar#>");
      document.form.lan_domain.focus();
      document.form.lan_domain.select();
	 	return false;
  }

	if(!validator.ipAddrFinal(document.form.dhcp_gateway_x, 'dhcp_gateway_x') ||
			!validator.ipAddrFinal(document.form.dhcp_dns1_x, 'dhcp_dns1_x') ||
			!validator.ipAddrFinal(document.form.dhcp_dns2_x, 'dhcp_dns2_x') ||
			!validator.ipAddrFinal(document.form.dhcp_wins_x, 'dhcp_wins_x'))
		return false;

	if(tmo_support && !validator.ipAddrFinal(document.form.sip_server, 'sip_server'))
		return false;

	if(!validate_dhcp_range(document.form.dhcp_start)
			|| !validate_dhcp_range(document.form.dhcp_end))
		return false;

	var dhcp_start_num = inet_network(document.form.dhcp_start.value);
	var dhcp_end_num = inet_network(document.form.dhcp_end.value);

	if(dhcp_start_num > dhcp_end_num){
		var tmp = document.form.dhcp_start.value;
		document.form.dhcp_start.value = document.form.dhcp_end.value;
		document.form.dhcp_end.value = tmp;
	}

//Viz 2011.10 check if DHCP pool in default pool{
	var default_pool = new Array();
	default_pool =get_default_pool(document.form.lan_ipaddr.value, document.form.lan_netmask.value);
	if((inet_network(document.form.dhcp_start.value) < inet_network(default_pool[0])) || (inet_network(document.form.dhcp_end.value) > inet_network(default_pool[1]))){
			if(confirm("<#JS_DHCP3#>")){ //Acceptable DHCP ip pool : "+default_pool[0]+"~"+default_pool[1]+"\n
				document.form.dhcp_start.value=default_pool[0];
				document.form.dhcp_end.value=default_pool[1];
			}else{return false;}
	}
//} Viz 2011.10 check if DHCP pool in default pool


	if(!validator.range(document.form.dhcp_lease, 120, 2678400))
 
		return false;

      	//Filtering ip address with leading zero
	document.form.dhcp_start.value = ipFilterZero(document.form.dhcp_start.value);
        document.form.dhcp_end.value = ipFilterZero(document.form.dhcp_end.value);

	return true;
}

function done_validating(action){
	refreshpage();
}

// Viz add 2011.10 default DHCP pool range{
function get_default_pool(ip, netmask){
	// --- get lan_ipaddr post set .xxx  By Viz 2011.10
	z=0;
	tmp_ip=0;
	for(i=0;i<document.form.lan_ipaddr.value.length;i++){
			if (document.form.lan_ipaddr.value.charAt(i) == '.')	z++;
			if (z==3){ tmp_ip=i+1; break;}
	}
	post_lan_ipaddr = document.form.lan_ipaddr.value.substr(tmp_ip,3);
	// --- get lan_netmask post set .xxx	By Viz 2011.10
	c=0;
	tmp_nm=0;
	for(i=0;i<document.form.lan_netmask.value.length;i++){
			if (document.form.lan_netmask.value.charAt(i) == '.')	c++;
			if (c==3){ tmp_nm=i+1; break;}
	}
	var post_lan_netmask = document.form.lan_netmask.value.substr(tmp_nm,3);

	var nm = new Array("0", "128", "192", "224", "240", "248", "252");
	for(i=0;i<nm.length;i++){
				 if(post_lan_netmask==nm[i]){
							gap=256-Number(nm[i]);
							subnet_set = 256/gap;
							for(j=1;j<=subnet_set;j++){
									if(post_lan_ipaddr < j*gap){
												pool_start=(j-1)*gap+1;
												pool_end=j*gap-2;
												break;
									}
							}

							var default_pool_start = subnetPostfix(document.form.dhcp_start.value, pool_start, 3);
							var default_pool_end = subnetPostfix(document.form.dhcp_end.value, pool_end, 3);
							var default_pool = new Array(default_pool_start, default_pool_end);
							return default_pool;
							break;
				 }
	}
	//alert(document.form.dhcp_start.value+" , "+document.form.dhcp_end.value);//Viz
}
// } Viz add 2011.10 default DHCP pool range

//Viz add 2012.02 DHCP client MAC { start

function showLANIPList(){
	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.ip == "offline") continue;
		if(clientObj.name.length > 30) clientObj.name = clientObj.name.substring(0, 28) + "..";

		htmlCode += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'';
		htmlCode += clientObj.mac;
		htmlCode += '\', \'';
		htmlCode += clientObj.ip;
                htmlCode += '\', \'';
                htmlCode += clientObj.name;
		htmlCode += '\');"><strong>';
		htmlCode += clientObj.ip + '</strong>&nbsp;&nbsp;(' + clientObj.name + ')';
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
	}

	document.getElementById("ClientList_Block_PC").innerHTML = htmlCode;
}


function setClientIP(macaddr, ipaddr,name){
	document.form.dhcp_staticmac_x_0.value = macaddr;
	document.form.dhcp_staticip_x_0.value = ipaddr;
        document.form.dhcp_staticname_x_0.value = trim(name);
	hideClients_Block();
	over_var = 0;
}


var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullLANIPList(obj){

	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';
		document.form.dhcp_staticmac_x_0.focus();
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

//Viz add 2012.02 DHCP client MAC } end
function check_macaddr(obj,flag){ //control hint of input mac address

	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		document.getElementById("check_mac").style.display = "";
		obj.focus();
		obj.select();
		return false;
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		document.getElementById("check_mac").style.display = "";
		obj.focus();
		obj.select();
		return false;
	}else{
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	}
}

function check_vpn(){		//true: (DHCP ip pool & static ip ) conflict with VPN clients

		if(pool_subnet == pptpd_clients_subnet
					&& ((pool_start_end >= pptpd_clients_start_ip && pool_start_end <= pptpd_clients_end_ip)
								|| (pool_end_end >= pptpd_clients_start_ip && pool_end_end <= pptpd_clients_end_ip)
								|| (pptpd_clients_start_ip >= pool_start_end && pptpd_clients_start_ip <= pool_end_end)
								|| (pptpd_clients_end_ip >= pool_start_end && pptpd_clients_end_ip <= pool_end_end))
					){
						return true;
		}

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
								return true;
  				}
			}
	}

	return false;
}


function validate_hostname(o){
	if ((o.value != "") && (validator.hostName(o) != "")) {
	        alert("Hostname must only contain alphanumeric characters, underline and dash symbol. The first character cannot be dash \"-\" or underline \"_\".");
	        o.select();
	}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_DHCP_Content.asp">
<input type="hidden" name="next_page" value="Advanced_GWStaticRoute_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="30">
<input type="hidden" name="action_script" value="restart_net_and_phy">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>">
<input type="hidden" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>">
<input type="hidden" name="dhcp_staticlist" value="">

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
	  <table width="760" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_2#> - <#menu5_2_2#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div class="formfontdesc"><#LANHostConfig_DHCPServerConfigurable_sectiondesc#></div>
      <div id="router_in_pool" class="formfontdesc" style="color:#FFCC00;display:none;"><#LANHostConfig_DHCPServerConfigurable_sectiondesc2#><span id="LANIP"></span></div>
      <div id="VPN_conflict" class="formfontdesc" style="color:#FFCC00;display:none;"><span id="VPN_conflict_span"></span></div>
      <div class="formfontdesc" style="margin-top:-10px;display:none;">
         <br>You can enter up to 128 static DHCP reservations.  If filled, the Name field content will be pushed to the
         client as the hostname.  If an invalid name is entered (such as one with spaces), then the name will only
         be used as a description on the webui itself (for example, "My Laptop").
      </div>
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
			  <thead>
			  <tr>
				<td colspan="2"><#t2BC#></td>
			  </tr>
			  </thead>

			  <tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,1);"><#LANHostConfig_DHCPServerConfigurable_itemname#></a></th>
				<td>
				  <input type="radio" value="1" name="dhcp_enable_x" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcp_enable_x', '1')" <% nvram_match("dhcp_enable_x", "1", "checked"); %>><#checkbox_Yes#>
				  <input type="radio" value="0" name="dhcp_enable_x" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcp_enable_x', '0')" <% nvram_match("dhcp_enable_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			  </tr>

			  <tr>
				<th>Hide DHCP/RA queries</th>
				<td>
				  <input type="radio" value="0" name="dhcpd_querylog" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_querylog', '0')" <% nvram_match("dhcpd_querylog", "0", "checked"); %>><#checkbox_Yes#>
				  <input type="radio" value="1" name="dhcpd_querylog" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_querylog', '1')" <% nvram_match("dhcpd_querylog", "1", "checked"); %>><#checkbox_No#>
				</td>
			  </tr>

			  <tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,2);"><#LANHostConfig_DomainName_itemname#></a></th>
				<td>
				  <input type="text" maxlength="32" class="input_25_table" name="lan_domain" value="<% nvram_get("lan_domain"); %>" autocorrect="off" autocapitalize="off">
				</td>
			  </tr>

			  <tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,3);"><#LANHostConfig_MinAddress_itemname#></a></th>
			  <td>
				<input type="text" maxlength="15" class="input_15_table" name="dhcp_start" value="<% nvram_get("dhcp_start"); %>" onKeyPress="return  validator.isIPAddr(this,event);" autocorrect="off" autocapitalize="off">
			  </td>
			  </tr>

			  <tr>
            <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,4);"><#LANHostConfig_MaxAddress_itemname#></a></th>
            <td>
              <input type="text" maxlength="15" class="input_15_table" name="dhcp_end" value="<% nvram_get("dhcp_end"); %>" onKeyPress="return validator.isIPAddr(this,event)" autocorrect="off" autocapitalize="off">
            </td>
			  </tr>

			  <tr>
            <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,5);"><#LANHostConfig_LeaseTime_itemname#></a></th>
            <td>
              <input type="text" maxlength="7" name="dhcp_lease" class="input_15_table" value="<% nvram_get("dhcp_lease"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off">
            </td>
			  </tr>

			  <tr>
            <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,6);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
            <td>
              <input type="text" maxlength="15" class="input_15_table" name="dhcp_gateway_x" value="<% nvram_get("dhcp_gateway_x"); %>" onKeyPress="return validator.isIPAddr(this,event)" autocorrect="off" autocapitalize="off">
            </td>
			  </tr>

			  <tr>
            <th>Sip Server</th>
            <td>
              <input type="text" maxlength="15" class="input_15_table" name="sip_server" value="<% nvram_get("sip_server"); %>" onKeyPress="return validator.isIPAddr(this,event)">
            </td>
			  </tr>
			</table>
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px">
			  <thead>
			  <tr>
				<td colspan="2"><#LANHostConfig_x_LDNSServer1_sectionname#></td>
			  </tr>
			  </thead>

			  <tr>
				<th width="200"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,7);">DNS Server 1</a></th>
				<td>
				  <input type="text" maxlength="15" class="input_15_table" name="dhcp_dns1_x" value="<% nvram_get("dhcp_dns1_x"); %>" onKeyPress="return validator.isIPAddr(this,event)" autocorrect="off" autocapitalize="off">
				  <div id="yadns_hint" style="display:none;"></div>
				</td>
			  </tr>
                          
			  <tr>
				<th width="200"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,7);">DNS Server 2</a></th>
				<td>
				  <input type="text" maxlength="15" class="input_15_table" name="dhcp_dns2_x" value="<% nvram_get("dhcp_dns2_x"); %>" onKeyPress="return validator.isIPAddr(this,event)">
				</td>
			  </tr>
                          <tr>
                                <th>Advertise router's IP in addition to user-specified DNS</th>
                                <td>
                                  <input type="radio" value="1" name="dhcpd_dns_router" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_dns_router', '1')" <% nvram_match("dhcpd_dns_router", "1", "checked"); %>><#checkbox_Yes#>
                                  <input type="radio" value="0" name="dhcpd_dns_router" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_dns_router', '0')" <% nvram_match("dhcpd_dns_router", "0", "checked"); %>><#checkbox_No#>
                                </td>
                          </tr>
			  <tr>
				<th>Forward local domain queries to upstream DNS</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="lan_dns_fwd_local"  onclick="return change_common_radio(this, 'LANHostConfig', 'lan_dns_fwd_local', '1')" <% nvram_match("lan_dns_fwd_local", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="lan_dns_fwd_local"  onclick="return change_common_radio(this, 'LANHostConfig', 'lan_dns_fwd_local', '0')" <% nvram_match("lan_dns_fwd_local", "0", "checked"); %> /><#checkbox_No#>
				</td>
			  </tr>
			  <tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,8);"><#LANHostConfig_x_WINSServer_itemname#></a></th>
				<td>
				  <input type="text" maxlength="15" class="input_15_table" name="dhcp_wins_x" value="<% nvram_get("dhcp_wins_x"); %>" onkeypress="return validator.isIPAddr(this,event)" autocorrect="off" autocapitalize="off"/>
				</td>
			  </tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" style="margin-top:8px;" >
		  	<thead>
		  		<tr>
					<td colspan="3"><#LANHostConfig_ManualDHCPEnable_itemname#></td>
		  		</tr>
		  	</thead>

		  	<tr>
     			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,9);"><#LANHostConfig_ManualDHCPEnable_itemname#></a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="dhcp_static_x"  onclick="return change_common_radio(this, 'LANHostConfig', 'dhcp_static_x', '1')" <% nvram_match("dhcp_static_x", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="dhcp_static_x"  onclick="return change_common_radio(this, 'LANHostConfig', 'dhcp_static_x', '0')" <% nvram_match("dhcp_static_x", "0", "checked"); %> /><#checkbox_No#>
				</td>
			  </tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
			  	<thead>
			  		<tr>
						<td colspan="4" id="GWStatic"><#LANHostConfig_ManualDHCPList_groupitemdesc#>&nbsp;(<#List_limit#>&nbsp;128)</td>
			  		</tr>
			  	</thead>

			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);"><#MAC_Address#></a></th>
					<th><#IPConnection_ExternalIPAddress_itemname#></th>
					<th>Hostname</th>
					<th><#list_add_delete#></th>
			  	</tr>
			  	<tr>
				<!-- client info -->
            			<td width="28%">
					<input type="text" class="input_20_table" maxlength="17" name="dhcp_staticmac_x_0" style="margin-left:-12px;width:170px;" onKeyPress="return validator.isHWAddr(this,event)" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off">
					<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;" onclick="pullLANIPList(this);" title="<#select_MAC#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
					<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
				</td>
            			<td width="28%">
            				<input type="text" class="input_15_table" maxlength="15" name="dhcp_staticip_x_0" onkeypress="return validator.isIPAddr(this,event)" autocorrect="off" autocapitalize="off">
            			</td>
            			<td width="28%">
					<input type="text" class="input_15_table" maxlenght="30" onkeypress="return is_alphanum(this, event);" onblur="validate_hostname(this);" name="dhcp_staticname_x_0" autocorrect="off" autocapitalize="off">
				</td>
				<td width="16%">
					<div>
						<input type="button" class="add_btn" onClick="addRow_Group(128);" value="">
					</div>
            			</td>
			  	</tr>
			  </table>

			  <div id="dhcp_staticlist_Block"></div>

        	<!-- manually assigned the DHCP List end-->
           	<div class="apply_gen">
           		<input type="button" name="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
            	</div>

      	  </td>
		</tr>
		</tbody>

	  </table>
	</td>
</form>
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
