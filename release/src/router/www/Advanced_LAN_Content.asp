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
<title><#Web_Title#> - <#menu5_2_1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>

<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';


<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var origin_lan_ip = '<% nvram_get("lan_ipaddr"); %>';
var pptpd_clients = '<% nvram_get("pptpd_clients"); %>';
var pptpd_clients_subnet = pptpd_clients.split(".")[0]
															+"."+pptpd_clients.split(".")[1]
															+"."+pptpd_clients.split(".")[2]
															+".";	
	var pptpd_clients_start_ip = parseInt(pptpd_clients.split(".")[3].split("-")[0]);
	var pptpd_clients_end_ip = parseInt(pptpd_clients.split("-")[1]);


function initial(){
	show_menu();
	
	if(sw_mode == "1"){
		 $("table_proto").style.display = "none";
		 $("table_gateway").style.display = "none";
		 $("table_dnsenable").style.display = "none";
		 $("table_dns1").style.display = "none";
		 $("table_dns2").style.display = "none";
		 /*  Not needed to show out. Viz 2012.04
		 var chk_vpn = check_vpn();
		 if(chk_vpn){
		 		$("VPN_conflict").style.display = "";	
		 }*/
	}
	else{
		display_lan_dns(<% nvram_get("lan_dnsenable_x"); %>);
		change_ip_setting('<% nvram_get("lan_proto"); %>');
	}	
}

function applyRule(){
	if(validForm()){

		if(wl6_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver

		showLoading();
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
	if(sw_mode == 2 || sw_mode == 3){
		if(document.form.lan_dnsenable_x_radio[0].checked == 1)
			document.form.lan_dnsenable_x.value = 1;
		else
			document.form.lan_dnsenable_x.value = 0;
	
		if(document.form.lan_proto_radio[0].checked == 1){
			document.form.lan_proto.value = "dhcp";
			return true;
		}
		else{			
			document.form.lan_proto.value = "static";
			if(!valid_IP(document.form.lan_ipaddr, "")) return false;  //AP LAN IP 
			if(!valid_IP(document.form.lan_gateway, "GW"))return false;  //AP Gateway IP		

			if(document.form.lan_gateway.value == document.form.lan_ipaddr.value){
					alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
					document.form.lan_gateway.focus();
					document.form.lan_gateway.select();					
					return false;
			}				

			return true;
		}	
	}else
		document.form.lan_proto.value = "static";		
	
	//router mode : WAN IP conflict with LAN ip subnet
	if(sw_mode == 1 && document.form.wan_ipaddr_x.value != "0.0.0.0" && document.form.wan_ipaddr_x.value != "" 
		&& document.form.wan_netmask_x.value != "0.0.0.0" && document.form.wan_netmask_x.value != ""){
		if(matchSubnet2(document.form.wan_ipaddr_x.value, document.form.wan_netmask_x, document.form.lan_ipaddr.value, document.form.lan_netmask)){
					document.form.lan_ipaddr.focus();
					document.form.lan_ipaddr.select();
					alert("<#IPConnection_x_WAN_LAN_conflict#>");
					return false;
		}	
	}	
	
	var ip_obj = document.form.lan_ipaddr;
	var ip_num = inet_network(ip_obj.value);
	var ip_class = "";	
	
	if(!valid_IP(ip_obj, "")) return false;  //LAN IP 

	// test if netmask is valid.
	var netmask_obj = document.form.lan_netmask;
	var netmask_num = inet_network(netmask_obj.value);
	var netmask_reverse_num = ~netmask_num;
	var default_netmask = "";
	var wrong_netmask = 0;

	if(netmask_num < 0) wrong_netmask = 1;	

	if(ip_class == 'A')
		default_netmask = "255.0.0.0";
	else if(ip_class == 'B')
		default_netmask = "255.255.0.0";
	else
		default_netmask = "255.255.255.0";
	
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
	
	var subnet_head = getSubnet(ip_obj.value, netmask_obj.value, "head");
	var subnet_end = getSubnet(ip_obj.value, netmask_obj.value, "end");
	
	if(ip_num == subnet_head || ip_num == subnet_end){
		alert(ip_obj.value+" <#JS_validip#>");
		ip_obj.focus();
		ip_obj.select();
		return false;
	}
	
	// check IP changed or not
  // No matter it changes or not, it will submit the form
  //Viz modify 2011.10 for DHCP pool issue {
	if(sw_mode == "1"){
		var pool_change = changed_DHCP_IP_pool();
		if(!pool_change)
			return false;
		else{
			document.form.lan_ipaddr_rt.value = document.form.lan_ipaddr.value;
			document.form.lan_netmask_rt.value = document.form.lan_netmask.value;			
		}
	}				
	//}Viz modify 2011.10 for DHCP pool issue 

	return true;
}

function done_validating(action){
	refreshpage();
}

// step1. check IP changed. // step2. check Subnet is the same 
function changed_DHCP_IP_pool(){
	if(document.form.lan_ipaddr.value != origin_lan_ip){ // IP changed
		if(!matchSubnet(document.form.lan_ipaddr.value, document.form.dhcp_start.value, 3) ||
				!matchSubnet(document.form.lan_ipaddr.value, document.form.dhcp_end.value, 3)){ // Different Subnet
				document.form.dhcp_start.value = subnetPrefix(document.form.lan_ipaddr.value, document.form.dhcp_start.value, 3);
				document.form.dhcp_end.value = subnetPrefix(document.form.lan_ipaddr.value, document.form.dhcp_end.value, 3);				
		}
	}
	
	var post_lan_netmask='';
	var pool_start='';
	var pool_end='';
	var nm = new Array("0", "128", "192", "224", "240", "248", "252");
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
	post_lan_netmask = document.form.lan_netmask.value.substr(tmp_nm,3);

// Viz add 2011.10 default DHCP pool range{
	for(i=0;i<nm.length;i++){
				 if(post_lan_netmask==nm[i]){
							gap=256-Number(nm[i]);							
							subnet_set = 256/gap;
							for(j=1;j<=subnet_set;j++){
									if(post_lan_ipaddr < 1*gap && post_lan_ipaddr == 1){		//Viz add to avoid default (1st) LAN ip in DHCP pool (start)2011.11
												pool_start=2;
												pool_end=1*gap-2;
												break;										//Viz add to avoid default (1st) LAN ip in DHCP pool (end)2011.11
									}else if(post_lan_ipaddr < j*gap){
												pool_start=(j-1)*gap+1;
												pool_end=j*gap-2;
												break;						
									}
							}																	
							break;
				 }
	}
	
		var update_pool_start = subnetPostfix(document.form.dhcp_start.value, pool_start, 3);
		var update_pool_end = subnetPostfix(document.form.dhcp_end.value, pool_end, 3);	
		if((document.form.dhcp_start.value != update_pool_start) || (document.form.dhcp_end.value != update_pool_end)){
				if(confirm("<#JS_DHCP1#>")){
						document.form.dhcp_start.value = update_pool_start;
						document.form.dhcp_end.value = update_pool_end;
				}else{
						return false;	
				}
		}	
			
	return true;	
	//alert(document.form.dhcp_start.value+" , "+document.form.dhcp_end.value);//Viz
	// } Viz add 2011.10 default DHCP pool range	
	
}

function display_lan_dns(flag){
	inputCtrl(document.form.lan_dns1_x, conv_flag(flag));
	inputCtrl(document.form.lan_dns2_x, conv_flag(flag));
}

function change_ip_setting(flag){
	if(flag == "dhcp"){
		inputCtrl(document.form.lan_dnsenable_x_radio[0], 1);
		inputCtrl(document.form.lan_dnsenable_x_radio[1], 1);
		flag = 1; 
	}
	else if(flag == "static"){
		document.form.lan_dnsenable_x_radio[1].checked = 1;
		inputCtrl(document.form.lan_dnsenable_x_radio[0], 0);
		inputCtrl(document.form.lan_dnsenable_x_radio[1], 0);
		flag = 0; 
		display_lan_dns(flag);
	}

	inputCtrl(document.form.lan_ipaddr, conv_flag(flag));
	inputCtrl(document.form.lan_netmask, conv_flag(flag));
	inputCtrl(document.form.lan_gateway, conv_flag(flag));
}

function conv_flag(_flag){
	if(_flag == 0)
		_flag = 1;
	else
		_flag = 0;		
	return _flag;
}

function check_vpn(){		//true: lAN ip & VPN client ip conflict
		var lan_ip_subnet = origin_lan_ip.split(".")[0]+"."+origin_lan_ip.split(".")[1]+"."+origin_lan_ip.split(".")[2]+".";
		var lan_ip_end = parseInt(origin_lan_ip.split(".")[3]);
		if(lan_ip_subnet == pptpd_clients_subnet && lan_ip_end >= pptpd_clients_start_ip && lan_ip_end <= pptpd_clients_end_ip){
				return true;
		}else{
				return false;	
		}		
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:10000;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">

<input type="hidden" name="current_page" value="Advanced_LAN_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_net_and_phy">
<input type="hidden" name="action_wait" value="15">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wan_ipaddr_x" value="<% nvram_get("wan0_ipaddr"); %>">
<input type="hidden" name="wan_netmask_x" value="<% nvram_get("wan0_netmask"); %>" >
<input type="hidden" name="wan_gateway_x" value="<% nvram_get("wan0_gateway"); %>">
<input type="hidden" name="wan_proto" value="<% nvram_get("wan_proto"); %>">
<input type="hidden" name="lan_proto" value="<% nvram_get("lan_proto"); %>">
<input type="hidden" name="lan_dnsenable_x" value="<% nvram_get("lan_dnsenable_x"); %>">
<input type="hidden" name="lan_ipaddr_rt" value="<% nvram_get("lan_ipaddr_rt"); %>">
<input type="hidden" name="lan_netmask_rt" value="<% nvram_get("lan_netmask_rt"); %>">

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
		  <div class="formfonttitle"><#menu5_2#> - <#menu5_2_1#></div>
      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div class="formfontdesc"><#LANHostConfig_display1_sectiondesc#></div>
      <div id="VPN_conflict" class="formfontdesc" style="color:#FFCC00;display:none;"><#LANHostConfig_display1_sectiondesc2#></div>
		  
		  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
		  
			<tr id="table_proto">
			<th width="30%"><#LANHostConfig_x_LAN_DHCP_itemname#></th>
			<td>
				<input type="radio" name="lan_proto_radio" class="input" onclick="change_ip_setting('dhcp')" value="dhcp" <% nvram_match("lan_proto", "dhcp", "checked"); %>><#checkbox_Yes#>
				<input type="radio" name="lan_proto_radio" class="input" onclick="change_ip_setting('static')" value="static" <% nvram_match("lan_proto", "static", "checked"); %>><#checkbox_No#>
			</td>
			</tr>
            
		  <tr>
			<th width="30%">
			  <a class="hintstyle" href="javascript:void(0);" onClick="openHint(4,1);"><#IPConnection_ExternalIPAddress_itemname#></a>
			</th>			
			<td>
			  <input type="text" maxlength="15" class="input_15_table" id="lan_ipaddr" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>" onKeyPress="return is_ipaddr(this, event);">
			</td>
		  </tr>
		  
		  <tr>
			<th>
			  <a class="hintstyle"  href="javascript:void(0);" onClick="openHint(4,2);"><#IPConnection_x_ExternalSubnetMask_itemname#></a>
			</th>
			<td>
				<input type="text" maxlength="15" class="input_15_table" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>" onkeypress="return is_ipaddr(this, event);" >
			  <input type="hidden" name="dhcp_start" value="<% nvram_get("dhcp_start"); %>">
			  <input type="hidden" name="dhcp_end" value="<% nvram_get("dhcp_end"); %>">
			</td>
		  </tr>

			<tr id="table_gateway">
			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,3);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
			<td>
				<input type="text" name="lan_gateway" maxlength="15" class="input_15_table" value="<% nvram_get("lan_gateway"); %>" onKeyPress="return is_ipaddr(this, event);">
			</td>
			</tr>

			<tr id="table_dnsenable">
      <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,12);"><#IPConnection_x_DNSServerEnable_itemname#></a></th>
			<td>
				<input type="radio" name="lan_dnsenable_x_radio" value="1" onclick="display_lan_dns(1)" <% nvram_match("lan_dnsenable_x", "1", "checked"); %> /><#checkbox_Yes#>
			  <input type="radio" name="lan_dnsenable_x_radio" value="0" onclick="display_lan_dns(0)" <% nvram_match("lan_dnsenable_x", "0", "checked"); %> /><#checkbox_No#>
			</td>
      </tr>          		
          		
      <tr id="table_dns1">
      <th>
				<a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,13);"><#IPConnection_x_DNSServer1_itemname#></a>
			</th>
      <td>
				<input type="text" maxlength="15" class="input_15_table" name="lan_dns1_x" value="<% nvram_get("lan_dns1_x"); %>" onkeypress="return is_ipaddr(this, event)" >
			</td>
      </tr>

      <tr id="table_dns2">
      <th>
				<a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,14);"><#IPConnection_x_DNSServer2_itemname#></a>
			</th>
      <td>
				<input type="text" maxlength="15" class="input_15_table" name="lan_dns2_x" value="<% nvram_get("lan_dns2_x"); %>" onkeypress="return is_ipaddr(this, event)" >
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
	</form>					
				</tr>
			</table>				
			<!--===================================End of Main Content===========================================-->
</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
