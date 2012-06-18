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
<title><#Web_Title#> - Dual WAN</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
var wans_caps = '<% nvram_get("wans_cap"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_routing_rulelist_array = '<% nvram_get("wans_routing_rulelist"); %>';
var wans_flag;
var switch_stb_x = '<% nvram_get("switch_stb_x"); %>';
var wans_caps_primary;
var wans_caps_secondary;
<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

var $j = jQuery.noConflict();

function initial(){
	show_menu();
	wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;
	wans_caps_primary = wans_caps;
	wans_caps_secondary = wans_caps;
	//wans_caps_secondary = wans_caps.replace("dsl", "").replace("  ", " ");

	addWANOption(document.form.wans_primary, wans_caps_primary.split(" "));
	document.form.wans_primary.value = wans_dualwan_orig.split(" ")[0];
	if(wans_dualwan_orig.search(" ") < 0 || wans_dualwan_orig.split(" ")[1] == "none"){ // single wan
		form_show(wans_flag);		
	}
	else{ // init dual WAN		
		form_show(wans_flag);		
	}
}

function form_show(v){
	if(v == 0){

		inputCtrl(document.form.wans_second, 0);
		inputCtrl(document.form.wans_mode, 0);
		inputCtrl(document.form.wans_lb_ratio_0, 0);
		inputCtrl(document.form.wans_lb_ratio_1, 0);
		inputCtrl(document.form.wans_isp_unit[0], 0);
		inputCtrl(document.form.wans_isp_unit[1], 0);
		inputCtrl(document.form.wans_isp_unit[2], 0);
		inputCtrl(document.form.wan0_isp_country, 0);
		inputCtrl(document.form.wan0_isp_list, 0);
		inputCtrl(document.form.wan1_isp_country, 0);
		inputCtrl(document.form.wan1_isp_list, 0);		
		document.form.wans_routing_enable[1].checked = true;		
		document.form.wans_routing_enable[0].disabled = true;
		document.form.wans_routing_enable[1].disabled = true;		
	}else if(v == 1){		
		changeWANProto(document.form.wans_primary);
		document.form.wans_second.value = wans_dualwan_orig.split(" ")[1];
		appendLANoption2(document.form.wans_second);
		appendModeOption('<% nvram_get("wans_mode"); %>');			
	}
	
	show_wans_rules();	
}

function applyRule(){
	
	if(wans_flag == 1){
		document.form.wans_dualwan.value = document.form.wans_primary.value +" "+ document.form.wans_second.value;
	}else{
		document.form.wans_dualwan.value = document.form.wans_primary.value + " none";
	}	
		
	if(document.form.wans_lanport1 && !document.form.wans_lanport2)
			document.form.wans_lanport.value = document.form.wans_lanport1.value;
	else if(!document.form.wans_lanport1 && document.form.wans_lanport2)
			document.form.wans_lanport.value = document.form.wans_lanport2.value;
			
	var tmp_pri_if = wans_dualwan_orig.split(" ")[0].toUpperCase();
	var tmp_sec_if = wans_dualwan_orig.split(" ")[1].toUpperCase();	
	if (tmp_pri_if != 'LAN' || tmp_sec_if != 'LAN') {
		var port_conflict = false;
		var lan_port_num = document.form.wans_lanport.value;
		if (switch_stb_x == '1') {
			if (lan_port_num == '1') {
				port_conflict = true;
			}		
		}
		else if (switch_stb_x == '2') {
			if (lan_port_num == '2') {
				port_conflict = true;
			}
		}
		else if (switch_stb_x == '3') {
			if (lan_port_num == '3') {
				port_conflict = true;
			}
		}
		else if (switch_stb_x == '4') {
			if (lan_port_num == '4') {
				port_conflict = true;
			}		
		}
		else if (switch_stb_x == '5') {
			if (lan_port_num == '1' || lan_port_num == '2') {
				port_conflict = true;
			}		
		}
		else if (switch_stb_x == '6') {
			if (lan_port_num == '3' || lan_port_num == '4') {
				port_conflict = true;
			}				
		}
		if (port_conflict) {
			alert("IPTV port number is same as dual wan LAN port number");
			return;
		}
	}
		
	document.form.wans_lb_ratio.value = document.form.wans_lb_ratio_0.value + ":" + document.form.wans_lb_ratio_1.value;
	
	if (document.form.wans_primary.value == "dsl") document.form.next_page = "Advanced_DSL_Content.asp";
	if (document.form.wans_primary.value == "lan") document.form.next_page = "Advanced_WAN_Content.asp";
	if (document.form.wans_primary.value == "usb") document.form.next_page = "Advanced_Modem_Content.asp";

	if(document.form.wans_mode.value != "fo"){	//lb, rt					 
			
			if(document.form.wan0_isp_country.options[0].selected == true){
					document.form.wan0_routing_isp.value = country[document.form.wan0_isp_country.value];
			}else{
					document.form.wan0_routing_isp.value = country[document.form.wan0_isp_country.value].toLowerCase()+"_"+country_n_isp[document.form.wan0_isp_country.value][document.form.wan0_isp_list.value];
			}
			
			if(document.form.wan1_isp_country.options[0].selected == true){
					document.form.wan1_routing_isp.value = country[document.form.wan1_isp_country.value];
			}else{
					document.form.wan1_routing_isp.value = country[document.form.wan1_isp_country.value].toLowerCase()+"_"+country_n_isp[document.form.wan1_isp_country.value][document.form.wan1_isp_list.value];
			}			
			
	}else if(document.form.wans_mode.value == "fo"){
			/*document.form.wan0_routing_isp_enable.value = 0;	
			document.form.wan1_routing_isp_enable.value = 0;*/
	}
	
	save_table();
	showLoading();
	document.form.submit();
}

function addWANOption(obj, wanscapItem){
	free_options(obj);

	for(i=0; i<wanscapItem.length; i++){
		if(wanscapItem[i].length > 0){
			obj.options[i] = new Option(wanscapItem[i].toUpperCase(), wanscapItem[i]);
		}
	}		
	if(obj.name == 'wans_second')
		appendLANoption2(obj);
}

function changeWANProto(obj){	
	if(wans_flag == 1){
		var dx = wans_caps_secondary.split(" ").getIndexByValue(obj.value);
		var wans_caps_secondary_tmp_array = wans_caps_secondary.split(" ");
		wans_caps_secondary_tmp_array.splice(dx, 1);
	  addWANOption(document.form.wans_second, wans_caps_secondary.split(" "));
	}		
	appendLANoption1(obj);
}

function appendLANoption1(obj){
	if(obj.value == "lan"){
		if(!document.form.wans_lanport1){
			var childsel=document.createElement("select");
			childsel.setAttribute("id","wans_lanport1");
			childsel.setAttribute("name","wans_lanport1");
			childsel.setAttribute("class","input_option");
			obj.parentNode.appendChild(childsel);
			document.form.wans_lanport1.options[0] = new Option("LAN Port 1", "1");
			document.form.wans_lanport1.options[1] = new Option("LAN Port 2", "2");
			document.form.wans_lanport1.options[2] = new Option("LAN Port 3", "3");
			document.form.wans_lanport1.options[3] = new Option("LAN Port 4", "4");

		}else{
			document.form.wans_lanport1.style.display = "";	
		}		
		
		if('<% nvram_get("wans_lanport"); %>' == 0)
				document.form.wans_lanport1.selectedIndex = 0;
		else
				document.form.wans_lanport1.selectedIndex = '<% nvram_get("wans_lanport"); %>' - 1;		
		
	}
	else if(document.form.wans_lanport1){
		document.form.wans_lanport1.style.display = "none";
	}
}

function validSecond(){
	if(document.form.wans_second.value == document.form.wans_primary.value){
		alert("This value cannot be the same as the Primary.");
		document.form.wans_second.focus();
		return false;
	}
}

function appendLANoption2(obj){
	if(obj.value == "lan"){
		if(!document.form.wans_lanport2){
			var childsel=document.createElement("select");
			childsel.setAttribute("id","wans_lanport2");
			childsel.setAttribute("name","wans_lanport2");
			childsel.setAttribute("class","input_option");
			obj.parentNode.appendChild(childsel);
			document.form.wans_lanport2.options[0] = new Option("LAN Port 1", "1");
			document.form.wans_lanport2.options[1] = new Option("LAN Port 2", "2");
			document.form.wans_lanport2.options[2] = new Option("LAN Port 3", "3");
			document.form.wans_lanport2.options[3] = new Option("LAN Port 4", "4");

		}else{
			document.form.wans_lanport2.style.display = "";
		}	

		if('<% nvram_get("wans_lanport"); %>' == 0)
				document.form.wans_lanport2.selectedIndex = 0;
		else
				document.form.wans_lanport2.selectedIndex = '<% nvram_get("wans_lanport"); %>' - 1;		
		
	}
	else if(document.form.wans_lanport2){
		document.form.wans_lanport2.style.display = "none";
	}	
}

function appendModeOption(v){
		if(v == "fo"){
				inputCtrl(document.form.wans_lb_ratio_0, 0);
				inputCtrl(document.form.wans_lb_ratio_1, 0);
				inputCtrl(document.form.wans_isp_unit[0], 0);
				inputCtrl(document.form.wans_isp_unit[1], 0);
				inputCtrl(document.form.wans_isp_unit[2], 0);
				inputCtrl(document.form.wan0_isp_country, 0);
				inputCtrl(document.form.wan0_isp_list, 0);				
				inputCtrl(document.form.wan1_isp_country, 0);
				inputCtrl(document.form.wan1_isp_list, 0);				
				document.form.wans_routing_enable[1].checked = true;				
				document.form.wans_routing_enable[0].disabled = true;
				document.form.wans_routing_enable[1].disabled = true;

		}else{	//lb, rt
				inputCtrl(document.form.wans_lb_ratio_0, 1);
				inputCtrl(document.form.wans_lb_ratio_1, 1);
				document.form.wans_lb_ratio_0.value = '<% nvram_get("wans_lb_ratio"); %>'.split(':')[0];
				document.form.wans_lb_ratio_1.value = '<% nvram_get("wans_lb_ratio"); %>'.split(':')[1];				
				
				inputCtrl(document.form.wans_isp_unit[0], 1);
				inputCtrl(document.form.wans_isp_unit[1], 1);
				inputCtrl(document.form.wans_isp_unit[2], 1);
				if('<% nvram_get("wan0_routing_isp_enable"); %>' == 1 && '<% nvram_get("wan1_routing_isp_enable"); %>' == 0){
						document.form.wans_isp_unit[1].checked =true;
						change_isp_unit(1);
				}else if('<% nvram_get("wan0_routing_isp_enable"); %>' == 0 && '<% nvram_get("wan1_routing_isp_enable"); %>' == 1){
						document.form.wans_isp_unit[2].checked =true;
						change_isp_unit(2);
				}else{
						document.form.wans_isp_unit[0].checked =true;
						change_isp_unit(0);
				}				
				
				Load_ISP_country();
				appendcoutry(document.form.wan0_isp_country);
				appendcoutry(document.form.wan1_isp_country);
				
				inputCtrl(document.form.wans_routing_enable[0], 1);
				inputCtrl(document.form.wans_routing_enable[1], 1);				
				if('<% nvram_get("wans_routing_enable"); %>' == 1)
					document.form.wans_routing_enable[0].checked = true;
				else
					document.form.wans_routing_enable[1].checked = true;	
					
		}
}

function addRow_Group(upper){
			var rule_num = $('wans_RoutingRules_table').rows.length;
			var item_num = $('wans_RoutingRules_table').rows[0].cells.length;	
			if(rule_num >= upper){
					alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
					return false;	
			}			

			if(document.form.wans_FromIP_x_0.value==""){
					document.form.wans_FromIP_x_0.value = "all";
			}else if(valid_IP_form(document.form.wans_FromIP_x_0,2) != true){
					return false;
			} 
			
			if(document.form.wans_ToIP_x_0.value==""){
					document.form.wans_ToIP_x_0.value = "all";
			}else if(valid_IP_form(document.form.wans_ToIP_x_0,2) != true){
					document.form.wans_FromIP_x_0.value = "";
					document.form.wans_ToIP_x_0.value = "";				
					return false;
			}				
		
			//Viz check same rule  //match(From IP, To IP) is not accepted
			if(item_num >=2){	
					for(i=0; i<rule_num; i++){	
							if((document.form.wans_FromIP_x_0.value == $('wans_RoutingRules_table').rows[i].cells[0].innerHTML
									&& document.form.wans_ToIP_x_0.value == $('wans_RoutingRules_table').rows[i].cells[1].innerHTML)
								|| (document.form.wans_FromIP_x_0.value == $('wans_RoutingRules_table').rows[i].cells[0].innerHTML
										&& (document.form.wans_ToIP_x_0.value == 'all' || $('wans_RoutingRules_table').rows[i].cells[1].innerHTML == 'all') )
								|| (document.form.wans_ToIP_x_0.value == $('wans_RoutingRules_table').rows[i].cells[1].innerHTML
										&& (document.form.wans_FromIP_x_0.value == 'all' || $('wans_RoutingRules_table').rows[i].cells[0].innerHTML == 'all') )		){
											
									alert("<#JS_duplicate#>");
									document.form.wans_FromIP_x_0.focus();
									document.form.wans_FromIP_x_0.select();
									return false;
							}					
					}
			}
		
			addRow(document.form.wans_FromIP_x_0 ,1);
			addRow(document.form.wans_ToIP_x_0, 0);
			addRow(document.form.wans_unit_x_0, 0);
			document.form.wans_unit_x_0.value = 0;
			show_wans_rules();		
					
}

function addRow(obj, head){
			if(head == 1)
				wans_routing_rulelist_array += "&#60"
			else
				wans_routing_rulelist_array += "&#62"
			
			wans_routing_rulelist_array += obj.value;

			obj.value = "";		
}

function show_wans_rules(){
	var wans_rules_row = wans_routing_rulelist_array.split('&#60');
	var code = "";			

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="wans_RoutingRules_table">';
	if(wans_rules_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < wans_rules_row.length; i++){
			code +='<tr id="row'+i+'">';
			var routing_rules_col = wans_rules_row[i].split('&#62');
				for(var j = 0; j < routing_rules_col.length; j++){
					if(j != 2){
							code +='<td width="30%">'+ routing_rules_col[j] +'</td>';		//IP  width="98"
					}else{
							
							code += '<td width="25%"><select class="input_option">';
							if(routing_rules_col[2] =="0")
									code += '<option value="0" selected>Primary WAN</option>';
							else
									code += '<option value="0">Primary WAN</option>';

							if(routing_rules_col[2] =="1")
									code += '<option value="1" selected>Secondary WAN</option>';
							else
									code += '<option value="1">Secondary WAN</option>';								

							code += '</select></td>';								
					}		
				}
				code +='<td width="15%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	$("wans_RoutingRules_Block").innerHTML = code;
}


function save_table(){
	var rule_num = $('wans_RoutingRules_table').rows.length;
	var item_num = $('wans_RoutingRules_table').rows[0].cells.length;
	var tmp_value = "";
     var comp_tmp = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){							
			if(j==2){
				tmp_value += $('wans_RoutingRules_table').rows[i].cells[2].firstChild.value;
			}else{						
					tmp_value += $('wans_RoutingRules_table').rows[i].cells[j].innerHTML;
			}
			
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	
	document.form.wans_routing_rulelist.value = tmp_value;
}

function change_isp_unit(v){
	
	inputCtrl(document.form.wan0_isp_country, 1);
	inputCtrl(document.form.wan1_isp_country, 1);
	
	if(v == 1){
			document.form.wan0_routing_isp_enable.value = 1;
			document.form.wan1_routing_isp_enable.value = 0;
			document.form.wan0_isp_country.disabled = false;
			document.form.wan0_isp_list.disabled = false;			
			document.form.wan1_isp_country.disabled = true;
			document.form.wan1_isp_list.disabled = true;
	}else if(v == 2){
			document.form.wan0_routing_isp_enable.value = 0;
			document.form.wan1_routing_isp_enable.value = 1;
			document.form.wan0_isp_country.disabled = true;
			document.form.wan0_isp_list.disabled = true;
			document.form.wan1_isp_country.disabled = false;
			document.form.wan1_isp_list.disabled = false;
	}else{
			document.form.wan0_routing_isp_enable.value = 0;
			document.form.wan1_routing_isp_enable.value = 0;
			document.form.wan0_isp_country.disabled = true;
			document.form.wan0_isp_list.disabled = true;
			document.form.wan1_isp_country.disabled = true;
			document.form.wan1_isp_list.disabled = true;			
	}	
}

var country = new Array("None", "China");
var country_n_isp = new Array;
country_n_isp[0] = new Array("");
country_n_isp[1] = new Array("edu","telecom","mobile","unicom");

function Load_ISP_country(){
		var country_num = country.length;
		for(c = 0; c < country_num; c++){
				document.form.wan0_isp_country.options[c] = new Option(country[c], c);
				if(document.form.wan0_isp_country.options[c].text.toLowerCase() == '<% nvram_get("wan0_routing_isp"); %>'.split("_")[0])
						document.form.wan0_isp_country.options[c].selected =true;
												
				document.form.wan1_isp_country.options[c] = new Option(country[c], c);		
				if(document.form.wan1_isp_country.options[c].text.toLowerCase() == '<% nvram_get("wan1_routing_isp"); %>'.split("_")[0])
						document.form.wan1_isp_country.options[c].selected =true;
						
		}
		if('<% nvram_get("wan0_routing_isp"); %>' == "")
				document.form.wan0_isp_country.options[0].selected =true;
		if('<% nvram_get("wan1_routing_isp"); %>' == "")
				document.form.wan1_isp_country.options[0].selected =true;				
}

function appendcoutry(obj){
		if(obj.name == "wan0_isp_country"){
				if(obj.value == 0){	//none
							document.form.wan0_isp_list.style.display = "none";
				}else{	//other country
							var wan0_country_isp_num = country_n_isp[obj.value].length;
							document.form.wan0_isp_list.style.display = "";
							for(j = 0; j < wan0_country_isp_num; j++){
									document.form.wan0_isp_list.options[j] = new Option(country_n_isp[obj.value][j], j);
									if(document.form.wan0_isp_list.options[j].text == '<% nvram_get("wan0_routing_isp"); %>'.split("_")[1])
											document.form.wan0_isp_list.options[j].selected =true;
							}
							
				}				
		}else if(obj.name == "wan1_isp_country"){
				if(obj.value == 0){	//none
							document.form.wan1_isp_list.style.display = "none";					
				}else{	//other country
							var wan1_country_isp_num = country_n_isp[obj.value].length;
							document.form.wan1_isp_list.style.display = "";
							for(j = 0; j < wan1_country_isp_num; j++){
									document.form.wan1_isp_list.options[j] = new Option(country_n_isp[obj.value][j], j);
									if(document.form.wan1_isp_list.options[j].text == '<% nvram_get("wan1_routing_isp"); %>'.split("_")[1])
											document.form.wan1_isp_list.options[j].selected =true;
							}
					
				}		
		}

}



function is_ipaddr_plus_netmask(o,event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	if (is_functionButton(event)){
		return true;
	}

	if((keyPressed > 46 && keyPressed < 58)){
		j = 0;
		
		for(i = 0; i < o.value.length; i++){
			if(o.value.charAt(i) == '.'){
				j++;
			}
		}
		
		if(j < 3 && i >= 3){
			if(o.value.charAt(i-3) != '.' && o.value.charAt(i-2) != '.' && o.value.charAt(i-1) != '.'){
				o.value = o.value+'.';
			}
		}
		
		return true;
	}
	else if(keyPressed == 46){
		j = 0;
		
		for(i = 0; i < o.value.length; i++){
			if(o.value.charAt(i) == '.'){
				j++;
			}
		}
		
		if(o.value.charAt(i-1) == '.' || j == 3){
			return false;
		}
		
		return true;
	}
	
	return false;
}


function del_Row(obj){
  var i=obj.parentNode.parentNode.rowIndex;
  $('wans_RoutingRules_table').deleteRow(i);
  
  var routing_rules_value = "";
	for(k=0; k<$('wans_RoutingRules_table').rows.length; k++){
		for(j=0; j<$('wans_RoutingRules_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				routing_rules_value += "&#60";
			else
				routing_rules_value += "&#62";
			routing_rules_value += $('wans_RoutingRules_table').rows[k].cells[j].innerHTML;		
		}
	}
	
	wans_routing_rulelist_array = routing_rules_value;
	if(wans_routing_rulelist_array == "")
			show_wans_rules();
}

</script>
</head>
<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WANPort_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WANPort_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>">
<input type="hidden" name="wans_lanport" value="<% nvram_get("wans_lanport"); %>">
<input type="hidden" name="wans_lb_ratio" value="<% nvram_get("wans_lb_ratio"); %>">
<input type="hidden" name="wan0_routing_isp_enable" value="<% nvram_get("wan0_routing_isp_enable"); %>">
<input type="hidden" name="wan0_routing_isp" value="<% nvram_get("wan0_routing_isp"); %>">
<input type="hidden" name="wan1_routing_isp_enable" value="<% nvram_get("wan0_routing_isp_enable"); %>">
<input type="hidden" name="wan1_routing_isp" value="<% nvram_get("wan1_routing_isp"); %>">
<input type="hidden" name="wans_routing_rulelist" value=''>

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
					<td valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" valign="top">
								  <div>&nbsp;</div>
								  <div class="formfonttitle">Wan Port Setup</div>
								  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					  			<div class="formfontdesc"><#Layer3Forwarding_x_ConnectionType_sectiondesc#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										
			  						<thead>
			  						<tr>
												<td colspan="2"><#t2BC#></td>
			  						</tr>
			  						</thead>
			  						
										<tr>
										<th>Enable Dual Wan</th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_dualwan_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_dualwan_enable').iphoneSwitch(wans_dualwan_orig.split(' ')[1] != 'none',
														 function() {												 													 	
															wans_flag = 1;
															inputCtrl(document.form.wans_second, 1);
															inputCtrl(document.form.wans_mode, 1);
															form_show(wans_flag);
															
															/*
															document.form.wans_dualwan.value = '<% nvram_get("wans_dualwan"); %>';
															save_table();
															showLoading();
															document.form.submit();
															*/
														 },
														 function() {
															wans_flag = 0;
															document.form.wans_dualwan.value = document.form.wans_primary.value + ' none';
															form_show(wans_flag);
															
															/*
															save_table();
															showLoading();
															document.form.submit();
															*/
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>			
												</div>	
											</td>
										</tr>

										<tr>
											<th>Primary WAN</th>
											<td>
												<select name="wans_primary" class="input_option" onchange="changeWANProto(this);" onblur="validSecond();"></select>
											</td>
									  </tr>
										<tr>
											<th>Secondary WAN</th>
											<td>
												<select name="wans_second" class="input_option" onchange="appendLANoption2(this);" onblur="validSecond();"></select>
											</td>
									  </tr>

										<tr>
											<th>Multi WAN Mode</th>
											<td>
												<select name="wans_mode" class="input_option" onchange="appendModeOption(this.value);">
													<option value="fo" <% nvram_match("wans_mode", "fo", "selected"); %>>Fail Over</option>
													<option value="lb" <% nvram_match("wans_mode", "lb", "selected"); %>>LoadBalance</option>
													<!--option value="rt" <% nvram_match("wans_mode", "rt", "selected"); %>>Routing</option-->
												</select>			
											</td>
									  </tr>

			          		<tr>
			            		<th>Load Balance Configuration</th>
			            		<td>
												<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_0" value="" onkeypress="return is_number(this,event);" />
												&nbsp; : &nbsp;
												<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_1" value="" onkeypress="return is_number(this,event);" />
											</td>
			          		</tr>

			          		<tr>
			          			<th>Import ISP profile rules</th>
			          			<td>
			          				<input type="radio" value="0" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);">None
				  							<input type="radio" value="1" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);">Primary WAN
				  							<input type="radio" value="2" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);">Secondary WAN
			          			</td>	
			          		</tr>	
			          		
			          		<tr>
			          			<th>Country / ISP profile rules for Primary WAN</th>
			          			<td>
			          					<select name="wan0_isp_country" class="input_option" onchange="appendcoutry(this);" value=""></select>
													<select name="wan0_isp_list" class="input_option" style="display:none;"value=""></select>
			          			</td>	
			          		</tr>
			          		
			          		<tr>
			          			<th>Country / ISP profile rules for Secondary WAN</th>
			          			<td>
			          					<select name="wan1_isp_country" class="input_option" onchange="appendcoutry(this);" value=""></select>
													<select name="wan1_isp_list" class="input_option" style="display:none;"value=""></select>
			          			</td>	
			          		</tr>			          		
			          		
									</table>
				<!-- -----------Enable Routing rules table ----------------------- -->
					<br>
	    		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
					  <thead>
					  <tr>
						<td colspan="2">Enable Routing rules for dual WAN?</td>
					  </tr>
					  </thead>		

          	<tr>
            	<th>Enable the Routing rules?</th>
            	<td>
						  		<input type="radio" value="1" name="wans_routing_enable" class="content_input_fd" <% nvram_match("wans_routing_enable", "1", "checked"); %>><#checkbox_Yes#>
		 							<input type="radio" value="0" name="wans_routing_enable" class="content_input_fd" <% nvram_match("wans_routing_enable", "0", "checked"); %>><#checkbox_No#>
							</td>
		  			</tr>		  			  				
          </table>									
									
				<!-- ----------Routing Rules Table  ---------------- -->
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;" >
			  	<thead>
			  		<tr><td colspan="4" id="Routing_table">Routing rules</td></tr>
			  	</thead>
			  
			  	<tr>
		  			<th><!--a class="hintstyle" href="javascript:void(0);"-->Source IP<!--/a--></th>
        		<th>Destination IP</th>
        		<th>WAN Unit</th>
        		<th>Add / Delete</th>
			  	</tr>			  
			  	<tr>
			  			<!-- rules info -->
							<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
			  		
            			<td width="30%">            			
                		<input type="text" class="input_15_table" maxlength="18" name="wans_FromIP_x_0" style="" onKeyPress="return is_ipaddr_plus_netmask(this,event)">
                	</td>
            			<td width="30%">
            				<input type="text" class="input_15_table" maxlength="18" name="wans_ToIP_x_0" onkeypress="return is_ipaddr_plus_netmask(this,event)">
            			</td>
            			<td width="25%">
										<select name="wans_unit_x_0" class="input_option">
												<option value="0">Primary WAN</option>
												<option value="1">Secondary WAN</option>
										</select>            				
            			</td>            			
            			<td width="15%">
										<div> 
											<input type="button" class="add_btn" onClick="addRow_Group(32);" value="">
										</div>
            			</td>
			  	</tr>	 			  
			  </table>        			
        			
			  <div id="wans_RoutingRules_Block"></div>
        			
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
