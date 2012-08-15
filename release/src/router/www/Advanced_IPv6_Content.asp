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
<title><#Web_Title#> - IPv6</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>

<% login_state_hook(); %>
<% wan_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var wan_proto_orig = '<% nvram_get("wan_proto"); %>';
var ipv6_proto_orig = '<% nvram_get("ipv6_service"); %>';
var ipv6_tun6rd_dhcp = '<% nvram_get("ipv6_6rd_dhcp"); %>';
var ipv6_dnsenable = '<% nvram_get("ipv6_dnsenable"); %>'; 
var wan0_ipaddr = '<% nvram_get("wan0_ipaddr"); %>'; 


function initial(){
	show_menu();	
	showInputfield(ipv6_proto_orig);
	addOnlineHelp($("faq"), ["ASUSWRT", "IPv6"]);
}

function showInputfield(v){
	
	if(v == "static6"){
		if(wan_proto_orig == "l2tp" || wan_proto_orig == "pptp" || wan_proto_orig == "pppoe")
			inputCtrl(document.form.ipv6_ifdev_select, 1);
		else	
			inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);		
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);		
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 0);
		inputCtrl(document.form.ipv6_tun_ttl, 0);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);
		$("ipv6_lan_setting").style.display="";
		inputCtrl(document.form.ipv6_prefix, 1);
		inputCtrl(document.form.ipv6_prefix_length, 1);		
		$("ipv6_prefix_r").style.display="none";
		$("ipv6_prefix_length_r").style.display="none";
		inputCtrl(document.form.ipv6_rtr_addr, 1);
		if(v != ipv6_proto_orig){
				document.form.ipv6_prefix.value = "";
				document.form.ipv6_prefix_length.value = "";
				document.form.ipv6_rtr_addr.value = "";
		}else{
				document.form.ipv6_prefix.value = "<% nvram_get("ipv6_prefix"); %>";
				document.form.ipv6_prefix_length.value = "<% nvram_get("ipv6_prefix_length"); %>";
				document.form.ipv6_rtr_addr.value = "<% nvram_get("ipv6_rtr_addr"); %>";
		}
		$("ipv6_ipaddr_r").style.display="none";
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 1);
		inputCtrl(document.form.ipv6_dns2, 1);
		inputCtrl(document.form.ipv6_dns3, 1);
		
	}
	else if(v == "dhcp6"){
		if(wan_proto_orig == "l2tp" || wan_proto_orig == "pptp" || wan_proto_orig == "pppoe")
			inputCtrl(document.form.ipv6_ifdev_select, 1);
		else	
			inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);		
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);		
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 0);
		inputCtrl(document.form.ipv6_tun_ttl, 0);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);		
		$("ipv6_lan_setting").style.display="";
		inputCtrl(document.form.ipv6_prefix, 0);
		inputCtrl(document.form.ipv6_prefix_length, 0);
		$("ipv6_prefix_r").style.display="";
		$("ipv6_prefix_length_r").style.display="";
		inputCtrl(document.form.ipv6_rtr_addr, 0);
		$("ipv6_ipaddr_r").style.display="";
		if(v != ipv6_proto_orig){
				$("ipv6_prefix_span").innerHTML = "";
				$("ipv6_prefix_length_span").innerHTML = "";			
				$("ipv6_ipaddr_span").innerHTML = "";
		}else{
				$("ipv6_prefix_span").innerHTML = "<% nvram_get("ipv6_prefix"); %>";
				$("ipv6_prefix_length_span").innerHTML = "<% nvram_get("ipv6_prefix_length"); %>";
				$("ipv6_ipaddr_span").innerHTML = "<% nvram_get("ipv6_rtr_addr"); %>";
		}		
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 1);
		inputCtrl(document.form.ipv6_dnsenable[1], 1);
		if(ipv6_dnsenable==1){
				inputCtrl(document.form.ipv6_dns1, 0);
				inputCtrl(document.form.ipv6_dns2, 0);
				inputCtrl(document.form.ipv6_dns3, 0);
		}else{
				inputCtrl(document.form.ipv6_dns1, 1);
				inputCtrl(document.form.ipv6_dns2, 1);
				inputCtrl(document.form.ipv6_dns3, 1);			
		}		
		
	}
	else if(v == "6to4"){
		inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 1);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);		
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 1);
		inputCtrl(document.form.ipv6_tun_ttl, 1);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);		
		$("ipv6_lan_setting").style.display="";
		inputCtrl(document.form.ipv6_prefix, 0);
		inputCtrl(document.form.ipv6_prefix_length, 0);
		$("ipv6_prefix_r").style.display="";
		$("ipv6_prefix_length_r").style.display="";
		var calc_hex = calcIP6(wan0_ipaddr);
		$("ipv6_prefix_span").innerHTML="2002:"+calc_hex+"::";
		$("ipv6_prefix_length_span").innerHTML="48";
		inputCtrl(document.form.ipv6_rtr_addr, 0);
		$("ipv6_ipaddr_r").style.display="";
		if(v != ipv6_proto_orig){				
				$("ipv6_ipaddr_span").innerHTML = "";
		}else{
				$("ipv6_ipaddr_span").innerHTML = "<% nvram_get("ipv6_rtr_addr"); %>";
		}
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 1);
		inputCtrl(document.form.ipv6_dns2, 1);
		inputCtrl(document.form.ipv6_dns3, 1);
	}
	else if(v == "6in4"){
		inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 1);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);
		inputCtrl(document.form.ipv6_tun_addr, 1);
		inputCtrl(document.form.ipv6_tun_addrlen, 1);
		inputCtrl(document.form.ipv6_tun_mtu, 1);
		inputCtrl(document.form.ipv6_tun_ttl, 1);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);		
		$("ipv6_lan_setting").style.display="";
		inputCtrl(document.form.ipv6_prefix, 1);
		inputCtrl(document.form.ipv6_prefix_length, 1);
		$("ipv6_prefix_r").style.display="none";
		$("ipv6_prefix_length_r").style.display="none";
		inputCtrl(document.form.ipv6_rtr_addr, 0);
		$("ipv6_ipaddr_r").style.display="";
		if(v != ipv6_proto_orig){
				document.form.ipv6_prefix.value = "";
				document.form.ipv6_prefix_length.value = "";			
				$("ipv6_ipaddr_span").innerHTML = "";
		}else{
				$("ipv6_ipaddr_span").innerHTML = "<% nvram_get("ipv6_rtr_addr"); %>";	
		}
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 1);
		inputCtrl(document.form.ipv6_dns2, 1);
		inputCtrl(document.form.ipv6_dns3, 1);
		
	}
	else if(v == "6rd"){
		inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 1);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 1);		
		if(document.form.ipv6_6rd_dhcp[1].checked == true){
					inputCtrl(document.form.ipv6_6rd_router, 1);
					inputCtrl(document.form.ipv6_6rd_ip4size, 1);
		}else{
					inputCtrl(document.form.ipv6_6rd_router, 0);
					inputCtrl(document.form.ipv6_6rd_ip4size, 0);
		}
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 1);
		inputCtrl(document.form.ipv6_tun_ttl, 1);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);		
		$("ipv6_lan_setting").style.display="";
		inputCtrl(document.form.ipv6_rtr_addr, 0);
		$("ipv6_ipaddr_r").style.display="";
		if(document.form.ipv6_6rd_dhcp[1].checked == true)
				showInputfield2('ipv6_6rd_dhcp', 0);
		else
				showInputfield2('ipv6_6rd_dhcp', 1);
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 1);
		inputCtrl(document.form.ipv6_dns2, 1);
		inputCtrl(document.form.ipv6_dns3, 1);
		
	}
	else if(v == "other"){
		if(wan_proto_orig == "l2tp" || wan_proto_orig == "pptp" || wan_proto_orig == "pppoe")
			inputCtrl(document.form.ipv6_ifdev_select, 1);
		else	
			inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);		
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);		
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 0);
		inputCtrl(document.form.ipv6_tun_ttl, 0);		
		$("ipv6_wan_setting").style.display="";
		inputCtrl(document.form.ipv6_ipaddr, 1);
		inputCtrl(document.form.ipv6_prefix_len_wan, 1);
		inputCtrl(document.form.ipv6_gateway, 1);
		if(v != ipv6_proto_orig){
				document.form.ipv6_ipaddr.value="";
				document.form.ipv6_prefix_len_wan.value="";
				document.form.ipv6_gateway.value="";
		}
		$("ipv6_lan_setting").style.display="";
		$("ipv6_prefix_r").style.display="none";
		$("ipv6_prefix_length_r").style.display="none";
		
		inputCtrl(document.form.ipv6_prefix, 1);
		inputCtrl(document.form.ipv6_prefix_length, 1);
		inputCtrl(document.form.ipv6_rtr_addr, 1);
		if(v != ipv6_proto_orig){
				document.form.ipv6_prefix.value = "";
				document.form.ipv6_prefix_length.value = "";
				document.form.ipv6_rtr_addr.value = "";
		}else{
				document.form.ipv6_prefix.value = "<% nvram_get("ipv6_prefix"); %>";
				document.form.ipv6_prefix_length.value = "<% nvram_get("ipv6_prefix_length"); %>";
				document.form.ipv6_rtr_addr.value = "<% nvram_get("ipv6_rtr_addr"); %>";
		}
		$("ipv6_ipaddr_r").style.display="none";
		$("ipv6_dns_setting").style.display="";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 1);
		inputCtrl(document.form.ipv6_dns2, 1);
		inputCtrl(document.form.ipv6_dns3, 1);		
		
	}	
	else{		// disabled
		inputCtrl(document.form.ipv6_ifdev_select, 0);
		inputCtrl(document.form.ipv6_tun_v4end, 0);
		inputCtrl(document.form.ipv6_relay, 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[0], 0);
		inputCtrl(document.form.ipv6_6rd_dhcp[1], 0);
		inputCtrl(document.form.ipv6_6rd_router, 0);
		inputCtrl(document.form.ipv6_6rd_ip4size, 0);
		inputCtrl(document.form.ipv6_tun_addr, 0);
		inputCtrl(document.form.ipv6_tun_addrlen, 0);
		inputCtrl(document.form.ipv6_tun_mtu, 0);
		inputCtrl(document.form.ipv6_tun_ttl, 0);
		$("ipv6_wan_setting").style.display="none";
		inputCtrl(document.form.ipv6_ipaddr, 0);
		inputCtrl(document.form.ipv6_prefix_len_wan, 0);
		inputCtrl(document.form.ipv6_gateway, 0);		
		$("ipv6_lan_setting").style.display="none";
		inputCtrl(document.form.ipv6_prefix, 0);
		inputCtrl(document.form.ipv6_prefix_length, 0);
		$("ipv6_prefix_r").style.display="none";
		$("ipv6_prefix_length_r").style.display="none";
		inputCtrl(document.form.ipv6_rtr_addr, 0);
		$("ipv6_ipaddr_r").style.display="none";
		$("ipv6_dns_setting").style.display="none";
		inputCtrl(document.form.ipv6_dnsenable[0], 0);
		inputCtrl(document.form.ipv6_dnsenable[1], 0);
		inputCtrl(document.form.ipv6_dns1, 0);
		inputCtrl(document.form.ipv6_dns2, 0);
		inputCtrl(document.form.ipv6_dns3, 0);
		
	}		
}

//Viz add 2011.11 for Input Control{
function showInputfield2(s, v){
	if(s=='ipv6_6rd_dhcp' && v=='0'){		//manual
				inputCtrl(document.form.ipv6_6rd_router, 1);
				inputCtrl(document.form.ipv6_6rd_ip4size, 1);
	
				inputCtrl(document.form.ipv6_prefix, 1);
				inputCtrl(document.form.ipv6_prefix_length, 1);
				if(v != ipv6_tun6rd_dhcp || document.form.ipv6_service.value != ipv6_proto_orig){
						var calc_hex = calcIP6(wan0_ipaddr);
						document.form.ipv6_prefix.value = "2001:55c:"+calc_hex+"::";
						document.form.ipv6_prefix_length.value = "64";
						$("ipv6_ipaddr_span").innerHTML = "";						
				}else{
						document.form.ipv6_prefix.value = "<% nvram_get("ipv6_prefix"); %>";
						document.form.ipv6_prefix_length.value = "<% nvram_get("ipv6_prefix_length"); %>";
						$("ipv6_ipaddr_span").innerHTML = "<% nvram_get("ipv6_rtr_addr"); %>";
				}		
				$("ipv6_prefix_r").style.display="none";
				$("ipv6_prefix_length_r").style.display="none";

	}else if(s=='ipv6_6rd_dhcp' && v=='1'){
				inputCtrl(document.form.ipv6_6rd_router, 0);
				inputCtrl(document.form.ipv6_6rd_ip4size, 0);
	
				inputCtrl(document.form.ipv6_prefix, 0);
				inputCtrl(document.form.ipv6_prefix_length, 0);
				$("ipv6_prefix_r").style.display="";
				$("ipv6_prefix_length_r").style.display="";
				if(v != ipv6_tun6rd_dhcp || document.form.ipv6_service.value != ipv6_proto_orig){
						$("ipv6_prefix_span").innerHTML = "";
						$("ipv6_prefix_length_span").innerHTML = "";
						$("ipv6_ipaddr_span").innerHTML = "";						
				}else{
						$("ipv6_prefix_span").innerHTML = "<% nvram_get("ipv6_prefix"); %>";
						$("ipv6_prefix_length_span").innerHTML = "<% nvram_get("ipv6_prefix_length"); %>";
						$("ipv6_ipaddr_span").innerHTML = "<% nvram_get("ipv6_rtr_addr"); %>";					
				}					
	}
	
	if(s=='ipv6_dnsenable' && v=='1'){
				inputCtrl(document.form.ipv6_dns1, 0);
				inputCtrl(document.form.ipv6_dns2, 0);
				inputCtrl(document.form.ipv6_dns3, 0);
	}else if(s=='ipv6_dnsenable' && v=='0'){
				inputCtrl(document.form.ipv6_dns1, 1);
				inputCtrl(document.form.ipv6_dns2, 1);
				inputCtrl(document.form.ipv6_dns3, 1);
	}
	
}
// } Viz add 2011.11 for Input Control


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
			ip_obj.value = "";
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
		else if(ip_num > C_class_start && ip_num < C_class_end)
			return true;
		else{
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.value = "";
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
}

function ipv6_valid(obj){
	//var rangere=new RegExp("^[a-f0-9]{1,4}:([a-f0-9]{0,4}:){2,6}[a-f0-9]{1,4}$", "gi");	
	var rangere=new RegExp("^((([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){6}:[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){5}:([0-9A-Fa-f]{1,4}:)?[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){4}:([0-9A-Fa-f]{1,4}:){0,2}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){3}:([0-9A-Fa-f]{1,4}:){0,3}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){2}:([0-9A-Fa-f]{1,4}:){0,4}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){6}((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|(([0-9A-Fa-f]{1,4}:){0,5}:((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|(::([0-9A-Fa-f]{1,4}:){0,5}((\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b)\\.){3}(\\b((25[0-5])|(1\\d{2})|(2[0-4]\\d)|(\\d{1,2}))\\b))|([0-9A-Fa-f]{1,4}::([0-9A-Fa-f]{1,4}:){0,5}[0-9A-Fa-f]{1,4})|(::([0-9A-Fa-f]{1,4}:){0,6}[0-9A-Fa-f]{1,4})|(([0-9A-Fa-f]{1,4}:){1,7}:))$", "gi");
	if(rangere.test(obj.value)){
			//alert(obj.value+"good");	
			return true;
	}else{
			alert(obj.value+" <#JS_validip#>");
			obj.focus();
			obj.select();			
			return false;
	}	
}

function calcIP6(ip) {
	var octet = ip.split(".");
	base = 16;
	var octet1 = parseInt(octet[0]);
	var octet2 = parseInt(octet[1]);
	var octet3 = parseInt(octet[2]);
	var octet4 = parseInt(octet[3]);
	hextet21 = octet1.toString(base);
	hextet22 = octet2.toString(base);
	hextet31 = octet3.toString(base);
	hextet32 = octet4.toString(base);

	var hextetval21 = (octet1 < 16) ? "0" + hextet21 : hextet21;	
	var hextetval22 = (octet2 < 16) ? "0" + hextet22 : hextet22;
	var hextetval31 = (octet3 < 16) ? "0" + hextet31 : hextet31;
	var hextetval32 = (octet4 < 16) ? "0" + hextet32 : hextet32;

	var Calc_hex = hextetval21+hextetval22+":"+hextetval31+hextetval32;
	return Calc_hex;
}

function validForm(){
	
	if(document.form.ipv6_service.value=="static6" || document.form.ipv6_service.value=="other"){
		
				if(document.form.ipv6_service.value=="other"){
							if(!ipv6_valid(document.form.ipv6_ipaddr) || 
									!validate_range(document.form.ipv6_prefix_len_wan, 3, 64) ||
									!ipv6_valid(document.form.ipv6_gateway)){
										
										return false;
							}					
				}
		
				if(!ipv6_valid(document.form.ipv6_prefix) || 
						!validate_range(document.form.ipv6_prefix_length, 3, 64) ||
						!ipv6_valid(document.form.ipv6_rtr_addr)){
						return false;
				}
				
				if(document.form.ipv6_dns1.value != ""){
						if(!ipv6_valid(document.form.ipv6_dns1)) return false;
				}
				if(document.form.ipv6_dns2.value != ""){
						if(!ipv6_valid(document.form.ipv6_dns2)) return false;
				}
				if(document.form.ipv6_dns3.value != ""){
						if(!ipv6_valid(document.form.ipv6_dns3)) return false;
				}											
	}
		
	if(document.form.ipv6_service.value=="dhcp6"){
				if(document.form.ipv6_dnsenable[1].checked){
								if(document.form.ipv6_dns1.value=="" && document.form.ipv6_dns2.value=="" && document.form.ipv6_dns3.value==""){
										alert("<#JS_fieldblank#>");
										document.form.ipv6_dns1.focus();
										document.form.ipv6_dns1.select();
										return false;			
								}
								if(document.form.ipv6_dns1.value != ""){
										if(!ipv6_valid(document.form.ipv6_dns1)) return false;
								}
								if(document.form.ipv6_dns2.value != ""){
										if(!ipv6_valid(document.form.ipv6_dns2)) return false;
								}
								if(document.form.ipv6_dns3.value != ""){
										if(!ipv6_valid(document.form.ipv6_dns3)) return false;
								}						
				}
	}
	
	if(document.form.ipv6_service.value=="6to4" ||document.form.ipv6_service.value=="6in4" ||document.form.ipv6_service.value=="6rd" ){
			if(!validate_range_sp(document.form.ipv6_tun_mtu,1280,1480,0))  return false;  //MTU
			if(!validate_range_sp(document.form.ipv6_tun_ttl,0,255,255))  return false;  //TTL				

			if(document.form.ipv6_dns1.value != ""){
					if(!ipv6_valid(document.form.ipv6_dns1)) return false;
			}
			if(document.form.ipv6_dns2.value != ""){
					if(!ipv6_valid(document.form.ipv6_dns2)) return false;
			}
			if(document.form.ipv6_dns3.value != ""){
					if(!ipv6_valid(document.form.ipv6_dns3)) return false;
			}
	}	
	
	if(document.form.ipv6_service.value=="6to4"){
			if(!valid_IP(document.form.ipv6_relay, "")) return false;  //6to4 tun relay	
	}
	
	if(document.form.ipv6_service.value=="6in4"){
			if(!validate_iprange(document.form.ipv6_tun_v4end, "")) return false;  //6in4 tun endpoint	
			if(!ipv6_valid(document.form.ipv6_tun_addr)) return false;  //6in4 Client IPv6 Address			
			if(!validate_range(document.form.ipv6_tun_addrlen, 3, 64))  return false;
	}	
	
	if(document.form.ipv6_service.value=="6rd"){
			if(document.form.ipv6_service.value=="6rd" && document.form.ipv6_6rd_dhcp[1].checked){
				if(!validate_iprange(document.form.ipv6_6rd_router, ""))  return false;  //6rd ip4 router
				if(!validate_range(document.form.ipv6_6rd_ip4size,0,32))  return false;  //6rd ip4 router mask length
			}			
	}
	
	

			
return true;	
}


function applyRule(){
	if(validForm()){

		if(document.form.ipv6_service.value=="dhcp6")		//ipv6_dhcppd.value=1 when "dhcp6"
				document.form.ipv6_dhcppd.value=1;
		else
				document.form.ipv6_dhcppd.value=0;
				
		if(document.form.ipv6_ifdev_select.disabled){		// set ipv6_ifdev="eth" while interface is disabled.
				document.form.ipv6_ifdev.value = "eth";
		}else{
				document.form.ipv6_ifdev.value = document.form.ipv6_ifdev_select.value;
		}
				
		if(document.form.ipv6_service.value!="static6" && document.form.ipv6_service.value!="other")	//clean up ipv6_rtr_addr if not static6 or not other
				document.form.ipv6_rtr_addr.value = "";
			
		document.form.ipv6_accept_ra.value=1;			// 0/1/2 default:1	

		if(wl6_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver
			
		showLoading();		
		document.form.submit();
}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
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
<input type="hidden" name="current_page" value="Advanced_IPv6_Content.asp">
<input type="hidden" name="next_page" value="Advanced_IPv6_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_allnet">
<input type="hidden" name="action_wait" value="30">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wan_unit" value="0">
<input type="hidden" name="ipv6_dhcppd" value="<% nvram_get("ipv6_dhcppd"); %>">
<input type="hidden" name="ipv6_accept_ra" value="<% nvram_get("ipv6_accept_ra"); %>">
<input type="hidden" name="ipv6_ifdev" value="">
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
				<div class="formfonttitle">IPv6</div>
	      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
	      <div class="formfontdesc"><#LANHostConfig_display6_sectiondesc#></div>
				<div class="formfontdesc" style="margin-top:-10px;">
					<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;">IPv6 FAQ</a>
				</div>
				  
			<!---------------------------------------basic_config start------------------------------------>  
			<table id="basic_config" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				  <thead>
				  <tr>
						<td colspan="2"><#t2BC#></td>
				  </tr>
				  </thead>		

					<tr>
						<th><#Connectiontype#></th>
		     		<td>
							<select name="ipv6_service" class="input_option" onchange="showInputfield(this.value);">
								<option class="content_input_fd" value="disabled" <% nvram_match("ipv6_service", "disabled", "selected"); %>><#btn_disable#></option>
								<!--option class="content_input_fd" value="static6" <% nvram_match("ipv6_service", "static6", "selected"); %>>Native IPv6</option-->
								<option class="content_input_fd" value="dhcp6" <% nvram_match("ipv6_service", "dhcp6", "selected"); %>>Native with DHCP-PD</option>
								<option class="content_input_fd" value="6to4" <% nvram_match("ipv6_service", "6to4", "selected"); %>>Tunnel 6to4</option>
								<option class="content_input_fd" value="6in4" <% nvram_match("ipv6_service", "6in4", "selected"); %>>Tunnel 6in4</option>
								<option class="content_input_fd" value="6rd" <% nvram_match("ipv6_service", "6rd", "selected"); %>>Tunnel 6rd</option>
								<option class="content_input_fd" value="other" <% nvram_match("ipv6_service", "other", "selected"); %>><#IPv6_static_IP#></option>
								<!--option class="content_input_fd" value="slaac" <% nvram_match("ipv6_service", "slaac", "selected"); %>>SLAAC</option-->
								<!--option class="content_input_fd" value="icmp6" <% nvram_match("ipv6_service", "icmp6", "selected"); %>>ICMPv6</option-->
							</select>
		     		</td>
		     	</tr>

					<tr>
						<th><#wan_interface#></th>
		     		<td>
							<select name="ipv6_ifdev_select" class="input_option">
								<option class="content_input_fd" value="ppp" <% nvram_match("ipv6_ifdev", "ppp","selected"); %>>PPP</option>
								<option class="content_input_fd" value="eth" <% nvram_match("ipv6_ifdev", "eth","selected"); %>><#wan_ethernet#></option>
							</select>
		     		</td>
		     	</tr>

					<tr style="display:none;">
						<th>Server IPv4 Address</th>
		     		<td>
							<input type="text" maxlength="15" class="input_15_table" name="ipv6_tun_v4end" value="<% nvram_get("ipv6_tun_v4end"); %>">
		     		</td>
		     	</tr>

					<tr style="display:none;">
						<th>IPv4 Anycast Relay</th>
		     		<td>
							<input type="text" maxlength="15" class="input_15_table" name="ipv6_relay" value="<% nvram_get("ipv6_relay"); %>">
		     		</td>
		     	</tr>

					<tr style="display:none;">
						<th><#ipv6_6rd_dhcp_option#></th>
		     		<td>
								<input type="radio" name="ipv6_6rd_dhcp" class="input" value="1" onclick="showInputfield2('ipv6_6rd_dhcp', this.value);" <% nvram_match("ipv6_6rd_dhcp", "1","checked"); %>><#WLANConfig11b_WirelessCtrl_button1name#>
								<input type="radio" name="ipv6_6rd_dhcp" class="input" value="0" onclick="showInputfield2('ipv6_6rd_dhcp', this.value);" <% nvram_match("ipv6_6rd_dhcp", "0","checked"); %>><#btn_disable#>
		     		</td>
		     	</tr>

					<tr style="display:none;">
						<th>IPv4 Remote Router</th>
		     		<td>
							<input type="text" maxlength="15" class="input_15_table" name="ipv6_6rd_router" value="<% nvram_get("ipv6_6rd_router"); %>">
		     		</td>
		     	</tr>
					<tr style="display:none;">
						<th>IPv4 Router Mask Length</th>
		     		<td>
							<input type="text" maxlength="2" class="input_3_table" name="ipv6_6rd_ip4size" value="<% nvram_get("ipv6_6rd_ip4size"); %>">
		     		</td>
		     	</tr>
		     	
					<tr style="display:none;">
						<th><#ipv6_client_ip#></th>
		     		<td>
							<input type="text" maxlength="39" class="input_32_table" name="ipv6_tun_addr" value="<% nvram_get("ipv6_tun_addr"); %>">
		     		</td>
		     	</tr>		     	
					<tr style="display:none;">
						<th><#IPv6_Prefix_Length#></th>
		     		<td>
							<input type="text" maxlength="2" class="input_3_table" name="ipv6_tun_addrlen" value="<% nvram_get("ipv6_tun_addrlen"); %>">
		     		</td>
		     	</tr>
		     	
					<tr style="display:none;">
						<th><#tunnel_MTU#></th>
		     		<td>
							<input type="text" maxlength="4" class="input_6_table" name="ipv6_tun_mtu" value="<% nvram_get("ipv6_tun_mtu"); %>">
		     		</td>
		     	</tr>
					<tr style="display:none;">
						<th><#tunnel_TTL#></th>
		     		<td>
							<input type="text" maxlength="3" class="input_6_table" name="ipv6_tun_ttl" value="<% nvram_get("ipv6_tun_ttl"); %>">
		     		</td>
		     	</tr>		     			     	
			</table>	
			<!---------------------------------------basic_config end------------------------------------>  
			
			<!---------------------------------------IPv6 WAN setting start------------------------------->  
			<table id="ipv6_wan_setting" style="margin-top:8px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				  <thead>
				  <tr>
						<td colspan="2"><#IPv6_WAN_Setting#></td>
				  </tr>
				  </thead>
					<tr>
						<th><#IPv6_wan_addr#></th>
		     		<td>
						  	<input type="text" maxlength="39" class="input_32_table" name="ipv6_ipaddr" value="<% nvram_get("ipv6_ipaddr"); %>">
						</td>
					</tr>
					<tr>
						<th><#IPv6_wan_Prefix_len#></th>
						<td>
								<input type="text" maxlength="2" class="input_3_table" name="ipv6_prefix_len_wan" value="<% nvram_get("ipv6_prefix_len_wan"); %>">
		     		</td>
		     	</tr>
					<tr>
						<th><#IPv6_wan_gateway#></th>
		     		<td>
						  	<input type="text" maxlength="39" class="input_32_table" name="ipv6_gateway" value="<% nvram_get("ipv6_gateway"); %>">
						</td>
					</tr>										
			</table>
			<!---------------------------------------IPv6 WAN setting  end ------------------------------->  
			
			<!---------------------------------------IPv6 LAN setting start------------------------------->  
			<table id="ipv6_lan_setting" style="margin-top:8px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				  <thead>
				  <tr>
						<td colspan="2"><#IPv6_LAN_Setting#></td>
				  </tr>
				  </thead>		
					<tr>
						<th><#IPv6_lan_Prefix#></th>
		     		<td>
						  	<input type="text" maxlength="39" class="input_32_table" name="ipv6_prefix" value="<% nvram_get("ipv6_prefix"); %>">
						</td>
					</tr>
					<tr>
						<th><#Prefix_lan_Length#></th>
						<td>
								<input type="text" maxlength="2" class="input_3_table" name="ipv6_prefix_length" value="<% nvram_get("ipv6_prefix_length"); %>">
		     		</td>
		     	</tr>
					<tr id="ipv6_prefix_r">
						<th><#IPv6_lan_Prefix#></th>
		     		<td>
						  <div id="ipv6_prefix_span" name="ipv6_prefix_span" style="color:#FFFFFF;margin-left:8px;"></div>
						</td>
					</tr>
					<tr id="ipv6_prefix_length_r">
						<th><#Prefix_lan_Length#></th>
						<td>
						  <div id="ipv6_prefix_length_span" name="ipv6_prefix_length_span" style="color:#FFFFFF;margin-left:8px;"></div>
		     		</td>
		     	</tr>		     	
					<tr style="display:none;">
						<th><#ipv6_lan_addr#></th>	<!-- for Native w/o DHCP-->
		     		<td>
						  <input type="text" maxlength="39" class="input_32_table" name="ipv6_rtr_addr" value="<% nvram_get("ipv6_rtr_addr"); %>">
		     		</td>
		     	</tr>
					<tr id="ipv6_ipaddr_r">
						<th><#ipv6_lan_addr#></th>	<!-- for other ipv6 proto -->
		     		<td>
						  <div id="ipv6_ipaddr_span" name="ipv6_ipaddr_span" style="color:#FFFFFF;margin-left:8px;"></div>
		     		</td>
		     	</tr>
			</table>
			<!---------------------------------------IPv6 LAN setting end------------------------------->  	
			
			<!---------------------------------------IPv6 DNS setting start------------------------------->  	
			<table id="ipv6_dns_setting" style="margin-top:8px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				  <thead>
				  <tr>
						<td colspan="2"><#IPv6_DNS_Setting#></td>
				  </tr>
				  </thead>		
					<tr style="display:none;">
						<th><#IPConnection_x_DNSServerEnable_itemname#></th>
		     		<td>
								<input type="radio" name="ipv6_dnsenable" class="input" value="1" onclick="showInputfield2('ipv6_dnsenable', this.value);" <% nvram_match("ipv6_dnsenable", "1","checked"); %>><#WLANConfig11b_WirelessCtrl_button1name#>
								<input type="radio" name="ipv6_dnsenable" class="input" value="0" onclick="showInputfield2('ipv6_dnsenable', this.value);" <% nvram_match("ipv6_dnsenable", "0","checked"); %>><#btn_disable#>
		     		</td>
		     	</tr>
					<tr style="display:none;">
						<th><#ipv6_dns_serv#> 1</th>
		     		<td>
						  <input type="text" maxlength="39" class="input_32_table" name="ipv6_dns1" value="<% nvram_get("ipv6_dns1"); %>">
		     		</td>
		     	</tr>
					<tr style="display:none;">
						<th><#ipv6_dns_serv#> 2</th>
		     		<td>
						  <input type="text" maxlength="39" class="input_32_table" name="ipv6_dns2" value="<% nvram_get("ipv6_dns2"); %>">
		     		</td>
		     	</tr>
					<tr style="display:none;">
						<th><#ipv6_dns_serv#> 3</th>
		     		<td>
						  <input type="text" maxlength="39" class="input_32_table" name="ipv6_dns3" value="<% nvram_get("ipv6_dns3"); %>">
		     		</td>
		     	</tr>		     	
			</table>
			<!---------------------------------------IPv6 DNS setting end------------------------------->  
			
			<!---------------------------------------Auto Config start------------------------------->  
			<table id="auto_config" style="margin-top:8px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				  <thead>
				  <tr>
						<td colspan="2"><#ipv6_auto_config#></td>
				  </tr>
				  </thead>		
					<tr>
						<th><#Enable_Router_AD#></th>
		     		<td>
							<select name="ipv6_radvd" class="input_option">
								<option class="content_input_fd" value="1" <% nvram_match("ipv6_radvd", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
								<option class="content_input_fd" value="0" <% nvram_match("ipv6_radvd", "0","selected"); %>><#btn_disable#></option>
							</select>
		     		</td>
		     	</tr>
					<!--tr>
						<th>Receive Advertisement ?</th>
		     		<td>
							<select name="ipv6_accept_ra" class="input_option">
								<option class="content_input_fd" value="0" <% nvram_match("ipv6_accept_ra", "0","selected"); %>><#btn_disable#></option>
								<option class="content_input_fd" value="1" <% nvram_match("ipv6_accept_ra", "1","selected"); %>>From WAN</option>
								<option class="content_input_fd" value="2" <% nvram_match("ipv6_accept_ra", "2","selected"); %>>From LAN</option>
							</select>
		     		</td>
		     	</tr-->
			</table>
			<!---------------------------------------Auto Config end ------------------------------->  	
				
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
