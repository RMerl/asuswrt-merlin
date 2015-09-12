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
<title><#Web_Title#> - <#dualwan#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
.ISPProfile{
	display:none;
}
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;	
	margin-left:2px;
	*margin-left:-353px;
	width:346px;
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
#ClientList_Block_PC div:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}

#detect_time_confirm{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:20000;
	margin-left: 40%;
	margin-top:10%;
	background-color:#232629;
	width:400px;
	box-shadow: 3px 3px 10px #000;
	font:13px Arial, Helvetica, sans-serif;
}
</style>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>


var wans_caps = '<% nvram_get("wans_cap"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_routing_rulelist_array = '<% nvram_get("wans_routing_rulelist"); %>';
var wans_flag;
var switch_stb_x = '<% nvram_get("switch_stb_x"); %>';
var wans_caps_primary;
var wans_caps_secondary;
var wandog_fb_count_orig = '<% nvram_get("wandog_fb_count"); %>';
var wandog_maxfail_orig = '<% nvram_get("wandog_maxfail"); %>';
var mobile_enable_orig = '';
if(gobi_support){
	if(usb_index == 0)
		mobile_enable_orig = '<% nvram_get("wan0_enable"); %>';
	else if(usb_index == 1)
		mobile_enable_orig = '<% nvram_get("wan1_enable"); %>';
}
var wans_mode_orig = '<% nvram_get("wans_mode"); %>';
var wans_standby_orig = '<% nvram_get("wans_standby"); %>';
var min_detect_interval = 2;
var min_fo_detect_count = 5;



var country = new Array("None", "China");
var country_n_isp = new Array;
country_n_isp[0] = new Array("");
country_n_isp[1] = new Array("edu","telecom","mobile","unicom");

function initial(){
	show_menu();
	wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;
	wans_caps_primary = wans_caps;
	wans_caps_secondary = wans_caps;
	
	addWANOption(document.form.wans_primary, wans_caps_primary.split(" "));
	addWANOption(document.form.wans_second, wans_caps_secondary.split(" "));

    if(based_modelid == "4G-AC55U"){
    	document.getElementById("wans_mode_option").style.display = "none";
    	document.getElementById("wans_mode_fo").style.display = "";

    	if( wans_mode_orig == "lb")
    		document.form.wans_mode.value = "fo";
    	else
    		document.form.wans_mode.value = wans_mode_orig;

		document.getElementById("fo_detection_count_hd").innerHTML = "<#Failover_retry_count#>";
		document.getElementById("fo_seconds").style.display = "none";
		document.getElementById("fo_tail_msg").style.display = "";		
    	document.getElementById("wandog_title").innerHTML = "<#Enable_user_target#>";

    	update_consume_bytes();
    }
    else
    	document.form.wans_mode.value = wans_mode_orig;

	form_show(wans_flag);		
	setTimeout("showLANIPList();", 1000);

	if(based_modelid == "RT-AC87U"){ //MODELDEP: RT-AC87 : Quantenna port
                document.form.wans_lanport1.remove(0);   //Primary LAN1
                document.form.wans_lanport2.remove(0);   //Secondary LAN1
	}

	if(based_modelid == "4G-AC55U"){
		update_detection_time();
	}
}

function form_show(v){
	if(v == 0){ //DualWAN disabled
		inputCtrl(document.form.wans_second, 0);
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
		document.getElementById('Routing_rules_table').style.display = "none";
		document.getElementById('wans_RoutingRules_Block').style.display = "none";	
		document.form.wans_primary.value = wans_dualwan_orig.split(" ")[0];	
		appendLANoption1(document.form.wans_primary);
		appendModeOption2("0");
		document.form.wandog_enable_radio[1].checked = true;		
		document.form.wandog_enable_radio[0].disabled = true;
		document.form.wandog_enable_radio[1].disabled = true;
		document.getElementById("wans_mode_tr").style.display = "none";
		document.getElementById("watchdog_table").style.display = "none";		
		document.getElementById("routing_table").style.display = "none";
		document.getElementById("wans_standby_tr").style.display = "none";
		inputCtrl(document.form.wans_standby, 0);
	}else{ //DualWAN enabled
		document.form.wans_primary.value = wans_dualwan_orig.split(" ")[0];
		if(wans_dualwan_orig.split(" ")[1] == "none"){

			if(wans_dualwan_orig.split(" ")[0] == "dsl"){
				
				if(wans_caps.search("wan") >= 0)
					document.form.wans_second.value = "wan";
				else if(wans_caps.search("usb") >= 0)
					document.form.wans_second.value = "usb";
				else
					document.form.wans_second.value = "lan";
			}
			else if(wans_dualwan_orig.split(" ")[0] == "wan"){

				if(wans_caps.search("usb") >= 0)
					document.form.wans_second.value = "usb";
				else
					document.form.wans_second.value = "lan";

			}
			else{
				if(wans_caps.search("wan") >= 0)
					document.form.wans_second.value = "wan";
			}
		}	
		else
			document.form.wans_second.value = wans_dualwan_orig.split(" ")[1];
		
		appendLANoption1(document.form.wans_primary);
		appendLANoption2(document.form.wans_second);

		if(document.form.wans_mode.value == "lb")
			document.getElementById("wans_mode_option").value = "lb";
		else{
			document.getElementById("wans_mode_option").value = "fo";
		}

		if(gobi_support){
			if(document.form.wans_mode.value != "lb" && (document.form.wans_primary.value == "usb" || document.form.wans_second.value == "usb")){
				document.getElementById("wans_standby_tr").style.display = "";
				inputCtrl(document.form.wans_standby, 1);
			}
			else{
				document.getElementById("wans_standby_tr").style.display = "none";
				inputCtrl(document.form.wans_standby, 0);		
			}
		}

		appendModeOption(document.form.wans_mode.value);
		show_wans_rules();
		document.getElementById("wans_mode_tr").style.display = "";
	}		
}

function applyRule(){
	if(wans_flag == 1){
		document.form.wans_dualwan.value = document.form.wans_primary.value +" "+ document.form.wans_second.value;
		
		if(!dsl_support && (document.form.wans_dualwan.value == "usb lan" || document.form.wans_dualwan.value == "lan usb")){
			alert("WAN port should be selected in Dual WAN.");
			document.form.wans_primary.focus();
			return;
		}
		document.form.wan_unit.value = "<% nvram_get("wan_unit"); %>";
		if(document.form.wans_mode.value == "lb"){

			if(!validator.range(document.form.wans_lb_ratio_0, 1, 9))
					return false;
			if(!validator.range(document.form.wans_lb_ratio_1, 1, 9))
					return false;

			document.form.wans_lb_ratio.value = document.form.wans_lb_ratio_0.value + ":" + document.form.wans_lb_ratio_1.value;
			
			
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
			
			save_table();
		}
		else{ //fo or fb
			document.form.wans_lb_ratio.disabled = true;
			document.form.wan0_routing_isp_enable.disabled = true;
			document.form.wan0_routing_isp.disabled = true;	
			document.form.wan1_routing_isp_enable.disabled = true;
			document.form.wan1_routing_isp.disabled = true;
			document.form.wans_routing_rulelist.disabled =true;	
			if(document.form.wandog_enable_radio[0].checked)
				document.form.wandog_enable.value = "1";
			else
				document.form.wandog_enable.value = "0";

			if(!validator.range(document.form.wandog_interval, 1, 9))
				return false;
			if(!validator.range(document.form.wandog_delay, 0, 99))
				return false;
		}		
	}
	else{
		document.form.wans_mode.value = "fo";
		appendModeOption("fo");
		document.form.wans_lb_ratio.disabled = true;
		document.form.wan0_routing_isp_enable.disabled = true;
		document.form.wan0_routing_isp.disabled = true;	
		document.form.wan1_routing_isp_enable.disabled = true;
		document.form.wan1_routing_isp.disabled = true;
		document.form.wans_routing_rulelist.disabled =true;		
		document.form.wans_dualwan.value = document.form.wans_primary.value + " none";
		document.form.wan_unit.value = 0;
		document.form.wandog_enable.value = "0";
	}	

	if(document.form.wans_primary.value == "lan")
		document.form.wans_lanport.value = document.form.wans_lanport1.value;
	else if(document.form.wans_second.value =="lan")
		document.form.wans_lanport.value = document.form.wans_lanport2.value;
	else	
		document.form.wans_lanport.disabled = true;

	if (document.form.wans_primary.value == "lan" || document.form.wans_second.value == "lan") {
		var port_conflict = false;
		var lan_port_num = document.form.wans_lanport.value;
		
		if(switch_stb_x == lan_port_num)
			port_conflict = true;
		else if (switch_stb_x == '5' && (lan_port_num == '1' || lan_port_num == '2')) 
			port_conflict = true;					
		else if (switch_stb_x == '6' && (lan_port_num == '3' || lan_port_num == '4'))
			port_conflict = true;	
						
		if (port_conflict) {
			alert("<#RouterConfig_IPTV_conflict#>");
			return;
		}
	}	
	
	if (document.form.wans_primary.value == "dsl") document.form.next_page.value = "Advanced_DSL_Content.asp";
	if (document.form.wans_primary.value == "lan") document.form.next_page.value = "Advanced_WAN_Content.asp";
	if (document.form.wans_primary.value == "usb"){
		if(based_modelid == "4G-AC55U")
			document.form.next_page.value = "Advanced_MobileBroadband_Content.asp";
		else			
			document.form.next_page.value = "Advanced_Modem_Content.asp";
	} 

	if(wans_dualwan_orig.split(" ")[1] == "none")
		document.form.wan_unit.value = 0;

	if(document.form.wandog_enable_radio[0].checked == true){
		if(document.form.wandog_target.value == ""){
			alert("Target cannot be blank.");
			document.form.wandog_target.focus();
			return;
		}
	}

	if(wans_dualwan_orig != document.form.wans_dualwan.value){
		var reboot_time	= eval("<% get_default_reboot_time(); %>");
		if(based_modelid =="DSL-AC68U")
			reboot_time += 70;
		document.form.action_script.value = "reboot";
		document.form.action_wait.value = reboot_time;
	}

	if(document.form.wans_standby.value == "1" && document.form.wans_standby.value != wans_standby_orig){
		document.getElementById("detect_time_confirm").style.display = "block";
		document.form.detect_interval.value = min_detect_interval;
		add_option_count(document.form.detect_interval, document.form.detect_count, min_fo_detect_count);
		update_str_time();
		return;
	}

	showLoading();
	document.form.submit();

}

function addWANOption(obj, wanscapItem){
	free_options(obj);
	if(dsl_support && obj.name == "wans_second"){
		for(i=0; i<wanscapItem.length; i++){
			if(wanscapItem[i] == "dsl"){
				wanscapItem.splice(i,1);	
			}
		}
	}
	
	for(i=0; i<wanscapItem.length; i++){
		if(wanscapItem[i].length > 0){
			var wanscapName = wanscapItem[i].toUpperCase();
	               //MODELDEP: DSL-N55U, DSL-N55U-B, DSL-AC68U, DSL-AC68R
			if(wanscapName == "LAN" && 
            	(productid == "DSL-N55U" || productid == "DSL-N55U-B" || productid == "DSL-AC68U" || productid == "DSL-AC68R")) 
				wanscapName = "Ethernet WAN";
			else if(wanscapName == "LAN")
				wanscapName = "Ethernet LAN";
			else if(wanscapName == "USB" && based_modelid == "4G-AC55U")
				wanscapName = "Mobile Broadband";			
			obj.options[i] = new Option(wanscapName, wanscapItem[i]);
		}	
	}
}

function changeWANProto(obj){	
	if(wans_flag == 1){	//dual WAN on
		if(document.form.wans_primary.value == document.form.wans_second.value){
			if(obj.name == "wans_primary"){
				if (obj.value == "dsl"){				
					if(wans_caps.search("wan") >= 0)
						document.form.wans_second.value = "wan";
					else if(wans_caps.search("usb") >= 0)
						document.form.wans_second.value = "usb";
					else
						document.form.wans_second.value = "lan";
				}
				else if(obj.value == "wan"){				
					if(wans_caps.search("usb") >= 0)
						document.form.wans_second.value = "usb";
					else
						document.form.wans_second.value = "lan";
				}
				else{
					if(wans_caps.search("wan") >= 0)
						document.form.wans_second.value = "wan";					
				}
			}
			else if(obj.name == "wans_second"){
				if(obj.value == "wan"){
					if(wans_caps.search("dsl") >= 0)
						document.form.wans_primary.value = "dsl";
					else if(wans_caps.search("usb") >= 0)
						document.form.wans_primary.value = "usb";
					else
						document.form.wans_primary.value = "lan";
				}
				else{

					if(wans_caps.search("dsl") >= 0)
						document.form.wans_primary.value = "dsl";
					else if(wans_caps.search("wan") >= 0)
						document.form.wans_primary.value = "wan";
				}
			}			
		}

		if(gobi_support){
			if(document.form.wans_primary.value == "usb" || document.form.wans_second.value == "usb"){
				document.getElementById("wans_standby_tr").style.display = "";
				inputCtrl(document.form.wans_standby, 1);
			}
			else{
				document.getElementById("wans_standby_tr").style.display = "none";
				inputCtrl(document.form.wans_standby, 0);		
			}
		}		

		appendLANoption1(document.form.wans_primary);
		appendLANoption2(document.form.wans_second);
	}else
		appendLANoption1(document.form.wans_primary);
}

function appendLANoption1(obj){
	if(obj.value == "lan"){
		if(document.form.wans_lanport1){
			document.form.wans_lanport1.style.display = "";	
		}		
	}
	else if(document.form.wans_lanport1){
		document.form.wans_lanport1.style.display = "none";
	}
}

function appendLANoption2(obj){
	if(obj.value == "lan"){
		if(document.form.wans_lanport2){
			document.form.wans_lanport2.style.display = "";
		}	
	}
	else if(document.form.wans_lanport2){
		document.form.wans_lanport2.style.display = "none";
	}
}

function appendModeOption(v){
		var wandog_enable_orig = '<% nvram_get("wandog_enable"); %>';
		if(v == "lb"){
			document.getElementById("lb_note").style.display = "";
			//document.getElementById("lb_note2").style.display = "";
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
			appendcountry(document.form.wan0_isp_country);
			appendcountry(document.form.wan1_isp_country);				
			inputCtrl(document.form.wans_routing_enable[0], 1);
			inputCtrl(document.form.wans_routing_enable[1], 1);				
			if('<% nvram_get("wans_routing_enable"); %>' == 1){
				document.form.wans_routing_enable[0].checked = true;
				document.getElementById('Routing_rules_table').style.display = "";
				document.getElementById('wans_RoutingRules_Block').style.display = "";
			}
			else{
				document.form.wans_routing_enable[1].checked = true;
				document.getElementById('Routing_rules_table').style.display = "none";
				document.getElementById('wans_RoutingRules_Block').style.display = "none";
			}				
			
			appendModeOption2("0");
			document.form.wandog_enable_radio[1].checked = true;				
			document.form.wandog_enable_radio[0].disabled = true;
			document.form.wandog_enable_radio[1].disabled = true;
			document.getElementById("watchdog_table").style.display = "none";
			document.getElementById("routing_table").style.display = "";
			document.getElementById("fb_span").style.display = "none";
			document.form.wans_mode.value = "lb";
		}
		else{ //Failover / Failback
			document.getElementById('lb_note').style.display = "none";
			//document.getElementById("lb_note2").style.display = "none";
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
			document.getElementById('Routing_rules_table').style.display = "none";
			document.getElementById('wans_RoutingRules_Block').style.display = "none";
			
			document.form.wandog_enable_radio[0].disabled = false;
			document.form.wandog_enable_radio[1].disabled = false;
			if(wandog_enable_orig == "1")
				document.form.wandog_enable_radio[0].checked = true;
			else
				document.form.wandog_enable_radio[1].checked = true;
			appendModeOption2(wandog_enable_orig);

			document.getElementById("watchdog_table").style.display = "";
			document.getElementById("routing_table").style.display = "none";

			document.getElementById("fb_span").style.display = "";

			add_option_count(document.form.wandog_interval, document.form.wandog_maxfail, wandog_maxfail_orig);

			if(document.form.wans_mode.value == "fb" ? true : false)
			{
				document.getElementById("fb_checkbox").checked = true;
				document.getElementById("wandog_fb_count_tr").style.display = "";
				add_option_count(document.form.wandog_interval, document.form.wandog_fb_count, wandog_fb_count_orig);
				document.form.wans_mode.value = "fb";
			}
			else
			{
				document.getElementById("fb_checkbox").checked = false;
				document.getElementById("wandog_fb_count_tr").style.display = "none";
				document.form.wans_mode.value = "fo";
			}
		}
}

function appendModeOption2(v){
	if(v == "1"){
		inputCtrl(document.form.wandog_target, 1);
	}else{
		inputCtrl(document.form.wandog_target, 0);
	}
}

function addRow_Group(upper){
			var rule_num = document.getElementById('wans_RoutingRules_table').rows.length;
			var item_num = document.getElementById('wans_RoutingRules_table').rows[0].cells.length;	
			if(rule_num >= upper){
				alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
				return false;	
			}			

			if(document.form.wans_FromIP_x_0.value==""){
				document.form.wans_FromIP_x_0.value = "all";
			}
			else if(validator.validIPForm(document.form.wans_FromIP_x_0,2) != true){
				return false;
			} 
			
			if(document.form.wans_ToIP_x_0.value==""){
					document.form.wans_ToIP_x_0.value = "all";
			}
			else if(validator.validIPForm(document.form.wans_ToIP_x_0,2) != true){
				document.form.wans_FromIP_x_0.value = "";
				document.form.wans_ToIP_x_0.value = "";				
				return false;
			}				
		
			//Viz check same rule  //match(From IP, To IP) is not accepted
			if(item_num >=2){	
					for(i=0; i<rule_num; i++){	
							if((document.form.wans_FromIP_x_0.value == document.getElementById('wans_RoutingRules_table').rows[i].cells[0].innerHTML
									&& document.form.wans_ToIP_x_0.value == document.getElementById('wans_RoutingRules_table').rows[i].cells[1].innerHTML)
								|| (document.form.wans_FromIP_x_0.value == document.getElementById('wans_RoutingRules_table').rows[i].cells[0].innerHTML
										&& (document.form.wans_ToIP_x_0.value == 'all' || document.getElementById('wans_RoutingRules_table').rows[i].cells[1].innerHTML == 'all') )
								|| (document.form.wans_ToIP_x_0.value == document.getElementById('wans_RoutingRules_table').rows[i].cells[1].innerHTML
										&& (document.form.wans_FromIP_x_0.value == 'all' || document.getElementById('wans_RoutingRules_table').rows[i].cells[0].innerHTML == 'all') )		){
											
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
					}
					else{							
						code += '<td width="25%"><select class="input_option">';
						if(routing_rules_col[2] =="0")
							code += "<option value=\"0\" selected><#dualwan_primary#></option>";
						else
							code += "<option value=\"0\"><#dualwan_primary#></option>";

						if(routing_rules_col[2] =="1")
							code += "<option value=\"1\" selected><#dualwan_secondary#></option>";
						else
							code += "<option value=\"1\"><#dualwan_secondary#></option>";								

						code += '</select></td>';								
					}		
				}
				code +='<td width="15%">'<!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->;
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	document.getElementById("wans_RoutingRules_Block").innerHTML = code;
}

function save_table(){
	var rule_num = document.getElementById('wans_RoutingRules_table').rows.length;
	var item_num = document.getElementById('wans_RoutingRules_table').rows[0].cells.length;
	var tmp_value = "";
    var comp_tmp = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){							
			if(j==2){
				tmp_value += document.getElementById('wans_RoutingRules_table').rows[i].cells[2].firstChild.value;
			}else{						
				tmp_value += document.getElementById('wans_RoutingRules_table').rows[i].cells[j].innerHTML;
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

function appendcountry(obj){
	if(obj.name == "wan0_isp_country"){
		if(obj.value == 0){	//none
			document.form.wan0_isp_list.style.display = "none";
		}
		else{	//other country
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

function del_Row(obj){
  var i=obj.parentNode.parentNode.rowIndex;
  document.getElementById('wans_RoutingRules_table').deleteRow(i);
  
  var routing_rules_value = "";
	for(k=0; k<document.getElementById('wans_RoutingRules_table').rows.length; k++){
		for(j=0; j<document.getElementById('wans_RoutingRules_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				routing_rules_value += "&#60";
			else
				routing_rules_value += "&#62";
			routing_rules_value += document.getElementById('wans_RoutingRules_table').rows[k].cells[j].innerHTML;		
		}
	}
	
	wans_routing_rulelist_array = routing_rules_value;
	if(wans_routing_rulelist_array == "")
		show_wans_rules();
}


function showLANIPList(){
	//copy array from Main_Analysis page
	var APPListArray = [
		["Google ", "www.google.com"], ["Facebook", "www.facebook.com"], ["Youtube", "www.youtube.com"], ["Yahoo", "www.yahoo.com"],
		["Baidu", "www.baidu.com"], ["Wikipedia", "www.wikipedia.org"], ["Windows Live", "www.live.com"], ["QQ", "www.qq.com"],
		["Amazon", "www.amazon.com"], ["Twitter", "www.twitter.com"], ["Taobao", "www.taobao.com"], ["Blogspot", "www.blogspot.com"], 
		["Linkedin", "www.linkedin.com"], ["Sina", "www.sina.com"], ["eBay", "www.ebay.com"], ["MSN", "msn.com"], ["Bing", "www.bing.com"], 
		["Яндекс", "www.yandex.ru"], ["WordPress", "www.wordpress.com"], ["ВКонтакте", "www.vk.com"]
	];

	var code = "";
	for(var i = 0; i < APPListArray.length; i++){
		code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+APPListArray[i][1]+'\');"><strong>'+APPListArray[i][0]+'</strong></div></a>';
	}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	document.getElementById("ClientList_Block_PC").innerHTML = code;
}

function setClientIP(ipaddr){
	document.form.wandog_target.value = ipaddr;
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
		document.form.wandog_target.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function enable_lb_rules(flag){
	if(flag == "1"){
			document.getElementById('Routing_rules_table').style.display = "";
			document.getElementById('wans_RoutingRules_Block').style.display = "";	
	}
	else{
			document.getElementById('Routing_rules_table').style.display = "none";
			document.getElementById('wans_RoutingRules_Block').style.display = "none";
	}
}

var str0="";
function add_option_count(obj, obj_t, selected_flag){
		
		if(obj_t.name == "wandog_maxfail" || (obj_t.name == "wandog_fb_count" && document.getElementById("wandog_fb_count_tr").style.display == "") || obj_t.name == "detect_count"){
				
				free_options(obj_t);
				for(var i=1; i<100; i++){
						if(based_modelid == "4G-AC55U")
							str0= i;
						else
							str0 = i*parseInt(obj.value);

						if(selected_flag == i)
								add_option(obj_t, str0, i, 1);
						else
								add_option(obj_t, str0, i, 0);
				}
		}
		else{
			return;
		}		
}

function hotstandby_act(enable){
	var confirm_str_on = "<#Wans_standby_hint#>";
	if(enable){
		if(mobile_enable_orig == "0"){
			if(confirm(confirm_str_on)){
				if(usb_index == 0)
					document.form.wan0_enable.value = "1";
				else if(usb_index == 1)					
					document.form.wan1_enable.value = "1"
			}
		}
	}
}

function update_detection_time(){
	if(based_modelid == "4G-AC55U"){
		document.getElementById("fo_detection_time").innerHTML = parseInt(document.form.wandog_interval.value)*parseInt(document.form.wandog_maxfail.value);
		if(document.form.wans_mode.value == "fb")
			document.getElementById("fb_detection_time").innerHTML = parseInt(document.form.wandog_interval.value)*parseInt(document.form.wandog_fb_count.value);
	}
}

function update_consume_bytes(){
    var consume_warning_str;
    var interval_value = parseInt(document.form.wandog_interval.value);
    var consume_bytes;
    var MBytes = 1024*1024;

    if(based_modelid == "4G-AC55U"){
    consume_bytes = 86400/interval_value*128*30;
	consume_bytes = Math.ceil(consume_bytes/MBytes);
    consume_warning_str = "<#Detect_consume_warning1#> "+consume_bytes+" <#Detect_consume_warning2#>";
    document.getElementById("consume_bytes_warning").style.display= "";
    document.getElementById("consume_bytes_warning").innerHTML = consume_warning_str;
	}
}

function validate_interval_value(){
	if(!validator.numberRange(document.form.detect_interval, 1, 9)){
		document.form.detect_interval.focus();
	}
}

function update_str_time(){
	if(!validator.numberRange(document.form.detect_interval, 1, 9)){
		document.form.detect_interval.focus();
		return;
	}
	var detection_time = parseInt(document.form.detect_interval.value)*parseInt(document.form.detect_count.value);
	document.getElementById("str_detect_time").innerHTML = detection_time;
	document.getElementById("detection_time_value").innerHTML = detection_time;
}

function change_detect_settings(){
	document.getElementById("detect_time_confirm").style.display = "none";
	document.form.wandog_interval.value = document.form.detect_interval.value;
	document.form.wandog_maxfail.value = document.form.detect_count.value;
	showLoading();
	document.form.submit();
}

function remain_origins(){
	document.getElementById("detect_time_confirm").style.display = "none";
	showLoading();
	document.form.submit();
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
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="start_multipath">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
<input type="hidden" name="wan_unit" value="<% nvram_get("wan_unit"); %>">
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>">
<input type="hidden" name="wans_lanport" value="<% nvram_get("wans_lanport"); %>">
<input type="hidden" name="wans_lb_ratio" value="<% nvram_get("wans_lb_ratio"); %>">
<input type="hidden" name="wandog_enable" value="<% nvram_get("wandog_enable"); %>">
<input type="hidden" name="wan0_routing_isp_enable" value="<% nvram_get("wan0_routing_isp_enable"); %>">
<input type="hidden" name="wan0_routing_isp" value="<% nvram_get("wan0_routing_isp"); %>">
<input type="hidden" name="wan1_routing_isp_enable" value="<% nvram_get("wan0_routing_isp_enable"); %>">
<input type="hidden" name="wan1_routing_isp" value="<% nvram_get("wan1_routing_isp"); %>">
<input type="hidden" name="wans_routing_rulelist" value=''>
<input type="hidden" name="wan0_enable" value="<% nvram_get("wan0_enable"); %>">
<input type="hidden" name="wan1_enable" value="<% nvram_get("wan1_enable"); %>">
<!--===================================Beginning of Detection Time Confirm===========================================-->
<div id="detect_time_confirm" style="display:none;">
		<!--div style="margin:20px 30px 20px;"-->
		<table width="90%" border="0" align="left" cellpadding="4" cellspacing="0" style="margin:15px 20px 15px; text-align:left;">
			<tr><td colspan="2"><#Standby_hint1#></td></tr><tr><td colspan="2"><#Standby_hint2#>&nbsp;<span id="str_detect_time"></span>&nbsp;<#Second#>.</td></tr>
			<tr>
				<th style="width:30%;"><#Retry_interval#>:</th>
				<td>
					<input type="text" name="detect_interval" class="input_3_table" maxlength="1" value=""; placeholder="5" autocorrect="off" autocapitalize="off" onKeyPress="return validator.isNumber(this, event);" onblur="update_str_time();" style="width: 38px; margin: 0px;">&nbsp;&nbsp;<#Second#>
				</td>
			</tr>
			<tr>
				<th><#Retry_count#>:</th>
				<td>
					<select name="detect_count" class="input_option" onchange="update_str_time();" style="margin: 0px 0px;"></select>
					<span id="detect_tail_msg">&nbsp;( Detection Time: <span id="detection_time_value"></span>&nbsp;&nbsp;<#Second#>)</span>
				</td>
			</tr>
			</table>
		<!--/div-->
		<div style="padding-bottom:10px;width:100%;text-align:center;">
		<input id="yesButton" class="button_gen" type="button" value="<#checkbox_Yes#>" onclick="change_detect_settings();">
		<input id="noButton" class="button_gen" type="button" value="<#checkbox_No#>" onclick="remain_origins();">
		</div>	
</div>
<!--===================================End of Detection Time Confirm===========================================-->
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
								  <div class="formfonttitle"><#menu5_3#> - <#dualwan#></div>
								  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					  			<div class="formfontdesc"><#dualwan_desc#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										
			  						<thead>
			  						<tr>
										<td colspan="2"><#t2BC#></td>
			  						</tr>
			  						</thead>
			  						
										<tr id="wans_mode_enable_tr">
										<th><#dualwan_enable#></th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_dualwan_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$('#radio_dualwan_enable').iphoneSwitch(wans_dualwan_orig.split(' ')[1] != 'none',
														 function() {
															wans_flag = 1;
															inputCtrl(document.form.wans_second, 1);
															form_show(wans_flag);
														 },
														 function() {
															wans_flag = 0;
															document.form.wans_dualwan.value = document.form.wans_primary.value + ' none';
															form_show(wans_flag);
															document.form.wans_mode.value = "fo";
														 }
													);
												</script>
												</div>
											</td>
										</tr>

										<tr>
											<th><#dualwan_primary#></th>
											<td>
												<select name="wans_primary" class="input_option" onchange="changeWANProto(this);"></select>
												<select id="wans_lanport1" name="wans_lanport1" class="input_option" style="margin-left:7px;">
													<option value="1" <% nvram_match("wans_lanport", "1", "selected"); %>>LAN Port 1</option>
													<option value="2" <% nvram_match("wans_lanport", "2", "selected"); %>>LAN Port 2</option>
													<option value="3" <% nvram_match("wans_lanport", "3", "selected"); %>>LAN Port 3</option>
													<option value="4" <% nvram_match("wans_lanport", "4", "selected"); %>>LAN Port 4</option>
												</select>
											</td>
									  	</tr>
										<tr>
											<th><#dualwan_secondary#></th>
											<td>
												<select name="wans_second" class="input_option" onchange="changeWANProto(this);"></select>
												<select id="wans_lanport2" name="wans_lanport2" class="input_option" style="margin-left:7px;">
													<option value="1" <% nvram_match("wans_lanport", "1", "selected"); %>>LAN Port 1</option>
													<option value="2" <% nvram_match("wans_lanport", "2", "selected"); %>>LAN Port 2</option>
													<option value="3" <% nvram_match("wans_lanport", "3", "selected"); %>>LAN Port 3</option>
													<option value="4" <% nvram_match("wans_lanport", "4", "selected"); %>>LAN Port 4</option>												
												</select>											
											</td>
									  	</tr>

										<tr id="wans_mode_tr">
											<th><#dualwan_mode#></th>
											<td>
												<input type="hidden" name="wans_mode" value=''>
												<select id="wans_mode_option" class="input_option" onchange="appendModeOption(this.value);">
													<option value="fo"><#dualwan_mode_fo#></option>
													<option value="lb" <% nvram_match("wans_mode", "lb", "selected"); %>><#dualwan_mode_lb#></option>
												</select>
												<span id="wans_mode_fo" style="margin-left:5px; color:#FFF; display:none;"><#dualwan_mode_fo#></span>
												<span id="fb_span" style="display:none"><input type="checkbox" id="fb_checkbox"><#dualwan_failback_allow#></span>
										  		<script>
										  			document.getElementById("fb_checkbox").onclick = function(){
										  				document.form.wans_mode.value = (this.checked == true ? "fb" : "fo");
										  				document.getElementById("wandog_fb_count_tr").style.display = (this.checked == true ? "" : "none");
										  				if(document.getElementById("wandog_fb_count_tr").style.display == "")
															add_option_count(document.form.wandog_interval, document.form.wandog_fb_count, wandog_fb_count_orig);
														update_detection_time();
										  			}
										  		</script>
												<div id="lb_note" style="color:#FFCC00; display:none;"><#dualwan_lb_note#></div>
												<div id="lb_note2" style="color:#FFCC00; display:none;"><#dualwan_lb_note2#></div>
											</td>
									  	</tr>

							<tr id="wans_standby_tr" style="display:none;">
								<th><#Standby_str#></th>
						        <td>
									<select name="wans_standby" id="wans_standby" class="input_option" onchange="hotstandby_act(this.value);">
										<option value="1" <% nvram_match("wans_standby", "1", "selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
										<option value="0" <% nvram_match("wans_standby", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									</select>        
								</td>
							</tr>	

			          		<tr>
			            		<th><#dualwan_mode_lb_setting#></th>
			            		<td>
									<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_0" value="" onkeypress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>
									&nbsp; : &nbsp;
									<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_1" value="" onkeypress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>												
								</td>
			          		</tr>

			          		<tr class="ISPProfile">
			          			<th><#dualwan_isp_rules#></th>
			          			<td>
			          				<input type="radio" value="0" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);">None
				  									<input type="radio" value="1" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);"><#dualwan_primary#>
				  									<input type="radio" value="2" name="wans_isp_unit" class="content_input_fd" onClick="change_isp_unit(this.value);"><#dualwan_secondary#>
			          			</td>	
			          		</tr>	
			          		
			          		<tr class="ISPProfile">
			          			<th><#dualwan_isp_primary#></th>
			          			<td>
			          					<select name="wan0_isp_country" class="input_option" onchange="appendcountry(this);" value=""></select>
															<select name="wan0_isp_list" class="input_option" style="display:none;"value=""></select>
			          			</td>	
			          		</tr>
			          		
			          		<tr class="ISPProfile">
			          			<th><#dualwan_isp_secondary#></th>
			          			<td>
			          					<select name="wan1_isp_country" class="input_option" onchange="appendcountry(this);" value=""></select>
															<select name="wan1_isp_list" class="input_option" style="display:none;"value=""></select>
			          			</td>	
			          		</tr>			          		
			          		
									</table>
									
	<!-- -----------Enable Ping time watch dog start----------------------- -->			
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" style="margin-top:8px;" id="watchdog_table">
					<thead>
					<tr>
						<td colspan="2"><#dualwan_pingtime_wd#></td>
					</tr>
					</thead>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,4);"><#Delay#></a></th>
						<td>
		        		<input type="text" name="wandog_delay" class="input_3_table" maxlength="2" value="<% nvram_get("wandog_delay"); %>" onKeyPress="return validator.isNumber(this, event);" placeholder="0" autocorrect="off" autocapitalize="off">&nbsp;&nbsp;<#Second#>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,3);"><#Retry_interval#></a></th>
						<td>
		        		<input type="text" name="wandog_interval" class="input_3_table" maxlength="1" value="<% nvram_get("wandog_interval"); %>" onblur="add_option_count(this, document.form.wandog_maxfail, document.form.wandog_maxfail.value);add_option_count(this, document.form.wandog_fb_count, document.form.wandog_fb_count.value);update_consume_bytes();update_detection_time();" onKeyPress="return validator.isNumber(this, event);" placeholder="5" autocorrect="off" autocapitalize="off">&nbsp;&nbsp;<#Second#><div><span id="consume_bytes_warning" style="display:none;"></span></div>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,5);"><div id="fo_detection_count_hd"><#dualwan_pingtime_detect#></div></a></th>
						<td>
									<select name="wandog_maxfail" class="input_option" onchange="update_detection_time();">
									</select>&nbsp;&nbsp;<span id="fo_seconds" style="color:#FFFFFF;"><#Second#></span>
									<span id="fo_tail_msg" style="display: none">(<#dualwan_pingtime_detect#>: <span id="fo_detection_time"></span>&nbsp;&nbsp;<#Second#>)</span>
						</td>
					</tr>

					<tr id="wandog_fb_count_tr">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,6);"><div id="fb_detection_count_hd"><#dualwan_pingtime_fb_detect#></div></a></th>	
						<td>     		
									<select name="wandog_fb_count" class="input_option" onchange="update_detection_time();">
									</select>&nbsp;&nbsp;<span id="fb_seconds" style="color:#FFFFFF;"><#Second#></span>
									<span id="fb_tail_msg" style="display: none">(<#dualwan_pingtime_fb_detect#>: <span id="fb_detection_time"></span>&nbsp;&nbsp;<#Second#>)</span>
						</td>
					</tr>
					
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,1);"><div id="wandog_title"><#wandog_enable#></div></a></th>
				        <td>
					 		<input type="radio" value="1" id="wandog_enable_radio1" name="wandog_enable_radio" class="content_input_fd" <% nvram_match("wandog_enable", "1", "checked"); %> onClick="appendModeOption2(this.value);"><label for="wandog_enable_radio1"><#checkbox_Yes#></label>
	 						<input type="radio" value="0" id="wandog_enable_radio2" name="wandog_enable_radio" class="content_input_fd" <% nvram_match("wandog_enable", "0", "checked"); %> onClick="appendModeOption2(this.value);"><label for="wandog_enable_radio2"><#checkbox_No#></label>
						</td>
					</tr>	
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,2);"><#NetworkTools_target#></a></th>
						<td>
								<input type="text" class="input_32_table" name="wandog_target" maxlength="100" value="<% nvram_get("wandog_target"); %>" placeholder="ex: www.google.com" autocorrect="off" autocapitalize="off">
								<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_network_host#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
								<div id="ClientList_Block_PC" class="ClientList_Block_PC" style="display:none;"></div>
						</td>
					</tr>

				</table>
				<!-- -----------Enable Ping time watch dog end----------------------- -->									
												
				<!-- -----------Enable Routing rules table start----------------------- -->				
	    		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" style="margin-top:8px;" id="routing_table">
					  <thead>
					  <tr>
						<td colspan="2"><#dualwan_routing_rule#></td>
					  </tr>
					  </thead>		

          				<tr>
            				<th><#dualwan_routing_rule_enable#></th>
            				<td>
						  		<input type="radio" value="1" name="wans_routing_enable" onClick="enable_lb_rules(this.value)" class="content_input_fd" <% nvram_match("wans_routing_enable", "1", "checked"); %>><#checkbox_Yes#>
		 						<input type="radio" value="0" name="wans_routing_enable" onClick="enable_lb_rules(this.value)" class="content_input_fd" <% nvram_match("wans_routing_enable", "0", "checked"); %>><#checkbox_No#>
							</td>
		  			</tr>		  			  				
          		</table>									
									
				<!-- ----------Routing Rules Table  ---------------- -->
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;" id="Routing_rules_table">
			  	<thead>
					<tr><td colspan="4" id="Routing_table"><#dualwan_routing_rule_list#>&nbsp;(<#List_limit#>&nbsp;128)</td></tr>
			  	</thead>
			  
			  	<tr>
		  			<th><!--a class="hintstyle" href="javascript:void(0);"--><#FirewallConfig_LanWanSrcIP_itemname#><!--/a--></th>
        		<th><#FirewallConfig_LanWanDstIP_itemname#></th>
        		<th><#dualwan_unit#></th>
        		<th><#list_add_delete#></th>
			  	</tr>			  
			  	<tr>
			  			<!-- rules info -->
			  		
            			<td width="30%">            			
                		<input type="text" class="input_15_table" maxlength="18" name="wans_FromIP_x_0" style="" onKeyPress="return validator.isIPAddrPlusNetmask(this,event)" autocorrect="off" autocapitalize="off">
                	</td>
            			<td width="30%">
            				<input type="text" class="input_15_table" maxlength="18" name="wans_ToIP_x_0" onkeypress="return validator.isIPAddrPlusNetmask(this,event)" autocorrect="off" autocapitalize="off">
            			</td>
            			<td width="25%">
										<select name="wans_unit_x_0" class="input_option">
												<option value="0"><#dualwan_primary#></option>
												<option value="1"><#dualwan_secondary#></option>
										</select>            				
            			</td>            			
            			<td width="15%">
										<div> 
											<input type="button" class="add_btn" onClick="addRow_Group(128);" value="">
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
