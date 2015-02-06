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
<title><#Web_Title#> - <#smart_connect_rule#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<style>
.ui-slider {
	position: relative;
	text-align: left;
}
.ui-slider .ui-slider-handle {
	position: absolute;
	width: 7px;
	height: 15px;
}
.ui-slider .ui-slider-range {
	position: absolute;
}
.ui-slider-horizontal {
	height: 6px;
}

.ui-widget-content {
	border: 1px inset #636e75;
	background-color:#666666;
}
.ui-state-default,
.ui-widget-content .ui-state-default,
.ui-widget-header .ui-state-default {
	border: 1px solid ;
	background: #9a9a9a;
	margin-top:-5px;
	margin-left:-4px;
}

/* Corner radius */

.ui-corner-all,
.ui-corner-top,
.ui-corner-left,
.ui-corner-tl {
	border-top-left-radius: 4px;
	border-top-right-radius: 4px;
	border-bottom-left-radius: 4px;
	border-bottom-right-radius: 4px;
}

.ui-slider-horizontal .ui-slider-range {
	top: 0;
	height: 100%;
}

#slider_wl0_bandutil .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;

}
#slider_wl0_bandutil .ui-slider-handle { border-color: #414141; }

#slider_wl1_bandutil .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;

}
#slider_wl1_bandutil .ui-slider-handle { border-color: #414141; }

#slider_wl2_bandutil .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;

}
#slider_wl2_bandutil .ui-slider-handle { border-color: #414141; }

#slider_wl0_steering_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_steering_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl0_steering_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_steering_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl1_steering_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl1_steering_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl1_steering_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl1_steering_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl2_steering_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl2_steering_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl2_steering_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl2_steering_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl0_sta_select_policy_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_sta_select_policy_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl0_sta_select_policy_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_sta_select_policy_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl1_sta_select_policy_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl1_sta_select_policy_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl1_sta_select_policy_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl1_sta_select_policy_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl2_sta_select_policy_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl2_sta_select_policy_phy_l .ui-slider-handle { border-color: #414141; }

#slider_wl2_sta_select_policy_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl2_sta_select_policy_phy_g .ui-slider-handle { border-color: #414141; }

#slider_wl0_if_qualify_policy .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_if_qualify_policy .ui-slider-handle { border-color: #414141; }

#slider_wl1_if_qualify_policy .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl1_if_qualify_policy .ui-slider-handle { border-color: #414141; }

#slider_wl2_if_qualify_policy .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl2_if_qualify_policy .ui-slider-handle { border-color: #414141; }

.bsd_greater {
	cursor:pointer;
	width:60px;
	height:23px;
}

.bsd_less {
	cursor:pointer;
	width:60px;
	height:23px;
}

.bsd_block_filter_name{
		text-align:center;
		color:#CCCCCC;
		font-size:12px
} 

.FormTable th{
	width:20%;
}

</style>
<script>
var $j = jQuery.noConflict();
<% wl_get_parameter(); %>

var wl0_names = new Array();
var wl1_names = new Array();
var wl2_names = new Array();
var wl_name = new Array();
var wl0_names = ['5GHz-1','5GHz-2','none'];
var wl1_names = ['5GHz-2','2.4GHz','none'];
var wl2_names = ['5GHz-1','2.4GHz','none'];
var wl_name = ['2.4GHz','5GHz-1','5GHz-2'];
var wl_ifnames = '<% nvram_get("wl_ifnames"); %>'.split(" ");

/* [bsd_steering_policy] */
var wl0_bsd_steering_policy = '<% nvram_get("wl0_bsd_steering_policy"); %>'.split(" ");	//[bw util%, x, x, RSSI threshold, phy rate, flag]
var wl0_bsd_steering_policy_bin = reverse_bin(createBinaryString(parseInt(wl0_bsd_steering_policy[6])));	//[OP, RSSI, VHT, NONVHT, N_RF, PHYRATE, L_BALANCE,...]
var wl1_bsd_steering_policy = '<% nvram_get("wl1_bsd_steering_policy"); %>'.split(" ");	//[bw util%, x, x, RSSI threshold, phy rate, flag]
var wl1_bsd_steering_policy_bin = reverse_bin(createBinaryString(parseInt(wl1_bsd_steering_policy[6])));	//[OP, RSSI, VHT, NONVHT, N_RF, PHYRATE, L_BALANCE,...]
var wl2_bsd_steering_policy = '<% nvram_get("wl2_bsd_steering_policy"); %>'.split(" ");	//[bw util%, x, x, RSSI threshold, phy rate, flag]
var wl2_bsd_steering_policy_bin = reverse_bin(createBinaryString(parseInt(wl2_bsd_steering_policy[6])));	//[OP, RSSI, VHT, NONVHT, N_RF, PHYRATE, L_BALANCE,...]

/* [bsd_sta_select_policy] */
var wl0_bsd_sta_select_policy = '<% nvram_get("wl0_bsd_sta_select_policy"); %>'.split(" ");	//[x, RSSI, phy rate less, phy rate greater, x, x, x, x, x, x, flag]
var wl0_bsd_sta_select_policy_bin = reverse_bin(createBinaryString(parseInt(wl0_bsd_sta_select_policy[10])));	//[OP, RSSI, VHT, NONVHT, x, PHYRATE, L_BALANCE,...]
var wl1_bsd_sta_select_policy = '<% nvram_get("wl1_bsd_sta_select_policy"); %>'.split(" ");	//[x, RSSI, phy rate less, phy rate greater, x, x, x, x, x, x, flag]
var wl1_bsd_sta_select_policy_bin = reverse_bin(createBinaryString(parseInt(wl1_bsd_sta_select_policy[10])));	//[OP, RSSI, VHT, NONVHT, x, PHYRATE, L_BALANCE,...]
var wl2_bsd_sta_select_policy = '<% nvram_get("wl2_bsd_sta_select_policy"); %>'.split(" ");	//[x, RSSI, phy rate less, phy rate greater, x, x, x, x, x, x, flag]
var wl2_bsd_sta_select_policy_bin = reverse_bin(createBinaryString(parseInt(wl2_bsd_sta_select_policy[10])));	//[OP, RSSI, VHT, NONVHT, x, PHYRATE, L_BALANCE,...]

/* [Interface Select and Qualify Procedures] */
var wl0_bsd_if_select_policy = '<% nvram_get("wl0_bsd_if_select_policy"); %>'.split(" ");
var wl0_bsd_if_qualify_policy = '<% nvram_get("wl0_bsd_if_qualify_policy"); %>'.split(" ");
var wl0_bsd_if_qualify_policy_bin = reverse_bin(createBinaryString(parseInt(wl0_bsd_if_qualify_policy[1])));
var wl1_bsd_if_select_policy = '<% nvram_get("wl1_bsd_if_select_policy"); %>'.split(" ");
var wl1_bsd_if_qualify_policy = '<% nvram_get("wl1_bsd_if_qualify_policy"); %>'.split(" ");
var wl1_bsd_if_qualify_policy_bin = reverse_bin(createBinaryString(parseInt(wl1_bsd_if_qualify_policy[1])));
var wl2_bsd_if_select_policy = '<% nvram_get("wl2_bsd_if_select_policy"); %>'.split(" ");
var wl2_bsd_if_qualify_policy = '<% nvram_get("wl2_bsd_if_qualify_policy"); %>'.split(" ");
var wl2_bsd_if_qualify_policy_bin = reverse_bin(createBinaryString(parseInt(wl2_bsd_if_qualify_policy[1])));
/*[bounce detect] */
var bsd_bounce_detect = '<% nvram_get("bsd_bounce_detect"); %>'.split(" ");	//[windows time in sec, counts, dwell time in sec]

function initial(){
	show_menu();
	register_event();
	handle_bsd_nvram();
}

function handle_bsd_nvram(){

	/* [bsd_steering_policy] - bsd_steering_policy setting */
	set_power(wl0_bsd_steering_policy[0],'wl0_bsd_steering_bandutil');
	if(wl0_bsd_steering_policy_bin[6] == 0){
		document.form.wl0_bsd_steering_balance[1].checked = true;
	}else{
		document.form.wl0_bsd_steering_balance[0].checked = true;
	}
	document.getElementById('wl0_bsd_steering_rssi').value = wl0_bsd_steering_policy[3];
	document.form.wl0_bsd_steering_rssi_s.value  = wl0_bsd_steering_policy_bin[1];
	set_power(wl0_bsd_steering_policy[4],'wl0_bsd_steering_phy_l');
	document.getElementById('wl0_bsd_steering_phy_l').value = wl0_bsd_steering_policy[4];	
	set_power(wl0_bsd_steering_policy[5],'wl0_bsd_steering_phy_g');
	document.getElementById('wl0_bsd_steering_phy_g').value = wl0_bsd_steering_policy[5];
	if(wl0_bsd_steering_policy_bin[2] == 0 && wl0_bsd_steering_policy_bin[3] == 0)	//all
		document.form.wl0_bsd_steering_vht_s.value = 0;
	else if(wl0_bsd_steering_policy_bin[2] == 0 && wl0_bsd_steering_policy_bin[3] == 1)	//ac only
		document.form.wl0_bsd_steering_vht_s.value = 1;
	else if(wl0_bsd_steering_policy_bin[2] == 1 && wl0_bsd_steering_policy_bin[3] == 0)	//legacy
		document.form.wl0_bsd_steering_vht_s.value = 2;

	set_power(wl1_bsd_steering_policy[0],'wl1_bsd_steering_bandutil');
	if(wl1_bsd_steering_policy_bin[6] == 0){
		document.form.wl1_bsd_steering_balance[1].checked = true;
	}else{
		document.form.wl1_bsd_steering_balance[0].checked = true;
	}
	document.getElementById('wl1_bsd_steering_rssi').value = wl1_bsd_steering_policy[3];
	document.form.wl1_bsd_steering_rssi_s.value  = wl1_bsd_steering_policy_bin[1];
	set_power(wl1_bsd_steering_policy[4],'wl1_bsd_steering_phy_l');
	document.getElementById('wl1_bsd_steering_phy_l').value = wl1_bsd_steering_policy[4];
	set_power(wl1_bsd_steering_policy[5],'wl1_bsd_steering_phy_g');
	document.getElementById('wl1_bsd_steering_phy_g').value = wl1_bsd_steering_policy[5];	
	if(wl1_bsd_steering_policy_bin[2] == 0 && wl1_bsd_steering_policy_bin[3] == 0)	//all
		document.form.wl1_bsd_steering_vht_s.value = 0;
	else if(wl1_bsd_steering_policy_bin[2] == 0 && wl1_bsd_steering_policy_bin[3] == 1)	//ac only
		document.form.wl1_bsd_steering_vht_s.value = 1;
	else if(wl1_bsd_steering_policy_bin[2] == 1 && wl1_bsd_steering_policy_bin[3] == 0)	//legacy
		document.form.wl1_bsd_steering_vht_s.value = 2;

	set_power(wl2_bsd_steering_policy[0],'wl2_bsd_steering_bandutil');
	if(wl2_bsd_steering_policy_bin[6] == 0){
		document.form.wl2_bsd_steering_balance[1].checked = true;
	}else{
		document.form.wl2_bsd_steering_balance[0].checked = true;
	}
	document.getElementById('wl2_bsd_steering_rssi').value = wl2_bsd_steering_policy[3];
	document.form.wl2_bsd_steering_rssi_s.value  = wl2_bsd_steering_policy_bin[1];
	set_power(wl2_bsd_steering_policy[4],'wl2_bsd_steering_phy_l');
	document.getElementById('wl2_bsd_steering_phy_l').value = wl2_bsd_steering_policy[4];
	set_power(wl2_bsd_steering_policy[5],'wl2_bsd_steering_phy_g');
	document.getElementById('wl2_bsd_steering_phy_g').value = wl2_bsd_steering_policy[5];

	if(wl2_bsd_steering_policy_bin[2] == 0 && wl2_bsd_steering_policy_bin[3] == 0)	//all
		document.form.wl2_bsd_steering_vht_s.value = 0;
	else if(wl2_bsd_steering_policy_bin[2] == 0 && wl2_bsd_steering_policy_bin[3] == 1)	//ac only
		document.form.wl2_bsd_steering_vht_s.value = 1;
	else if(wl2_bsd_steering_policy_bin[2] == 1 && wl2_bsd_steering_policy_bin[3] == 0)	//legacy
		document.form.wl2_bsd_steering_vht_s.value = 2;	

	/* [bsd_sta_select_policy] - bsd_sta_select_policy setting */
	document.getElementById('wl0_bsd_sta_select_policy_rssi').value = wl0_bsd_sta_select_policy[1];
	document.form.wl0_bsd_sta_select_policy_rssi_s.value  = wl0_bsd_sta_select_policy_bin[1];

	set_power(wl0_bsd_sta_select_policy[2],'wl0_bsd_sta_select_policy_phy_l');
	document.getElementById('wl0_bsd_sta_select_policy_phy_l').value = wl0_bsd_sta_select_policy[2];
	set_power(wl0_bsd_sta_select_policy[3],'wl0_bsd_sta_select_policy_phy_g');
	document.getElementById('wl0_bsd_sta_select_policy_phy_g').value = wl0_bsd_sta_select_policy[3];

	if(wl0_bsd_sta_select_policy_bin[2] == 0 && wl0_bsd_sta_select_policy_bin[3] == 0)	//all
		document.form.wl0_bsd_sta_select_policy_vht_s.value = 0;
	else if(wl0_bsd_sta_select_policy_bin[2] == 0 && wl0_bsd_sta_select_policy_bin[3] == 1)	//ac only
		document.form.wl0_bsd_sta_select_policy_vht_s.value = 1;
	else if(wl0_bsd_sta_select_policy_bin[2] == 1 && wl0_bsd_sta_select_policy_bin[3] == 0)	//legacy
		document.form.wl0_bsd_sta_select_policy_vht_s.value = 2;

	document.getElementById('wl1_bsd_sta_select_policy_rssi').value = wl1_bsd_sta_select_policy[1];
	document.form.wl1_bsd_sta_select_policy_rssi_s.value  = wl1_bsd_sta_select_policy_bin[1];

	set_power(wl1_bsd_sta_select_policy[2],'wl1_bsd_sta_select_policy_phy_l');
	document.getElementById('wl1_bsd_sta_select_policy_phy_l').value = wl1_bsd_sta_select_policy[2];
	set_power(wl1_bsd_sta_select_policy[3],'wl1_bsd_sta_select_policy_phy_g');
	document.getElementById('wl1_bsd_sta_select_policy_phy_g').value = wl1_bsd_sta_select_policy[3];	

	if(wl1_bsd_sta_select_policy_bin[2] == 0 && wl1_bsd_sta_select_policy_bin[3] == 0)	//all
		document.form.wl1_bsd_sta_select_policy_vht_s.value = 0;
	else if(wl1_bsd_sta_select_policy_bin[2] == 0 && wl1_bsd_sta_select_policy_bin[3] == 1)	//ac only
		document.form.wl1_bsd_sta_select_policy_vht_s.value = 1;
	else if(wl1_bsd_sta_select_policy_bin[2] == 1 && wl1_bsd_sta_select_policy_bin[3] == 0)	//legacy
		document.form.wl1_bsd_sta_select_policy_vht_s.value = 2;

	document.getElementById('wl2_bsd_sta_select_policy_rssi').value = wl2_bsd_sta_select_policy[1];
	document.form.wl2_bsd_sta_select_policy_rssi_s.value  = wl2_bsd_sta_select_policy_bin[1];

	set_power(wl2_bsd_sta_select_policy[2],'wl2_bsd_sta_select_policy_phy_l');
	document.getElementById('wl2_bsd_sta_select_policy_phy_l').value = wl2_bsd_sta_select_policy[2];
	set_power(wl2_bsd_sta_select_policy[3],'wl2_bsd_sta_select_policy_phy_g');
	document.getElementById('wl2_bsd_sta_select_policy_phy_g').value = wl2_bsd_sta_select_policy[3];	

	if(wl2_bsd_sta_select_policy_bin[2] == 0 && wl2_bsd_sta_select_policy_bin[3] == 0)	//all
		document.form.wl2_bsd_sta_select_policy_vht_s.value = 0;
	else if(wl2_bsd_sta_select_policy_bin[2] == 0 && wl2_bsd_sta_select_policy_bin[3] == 1)	//ac only
		document.form.wl2_bsd_sta_select_policy_vht_s.value = 1;
	else if(wl2_bsd_sta_select_policy_bin[2] == 1 && wl2_bsd_sta_select_policy_bin[3] == 0)	//legacy
		document.form.wl2_bsd_sta_select_policy_vht_s.value = 2;			

	/* [Interface Select and Qualify Procedures] - bsd_if_qualify_policy setting*/
	var wl0_bsd_if_select_policy_1st = wl_name[wl_ifnames.indexOf(wl0_bsd_if_select_policy[0])]; 
	var wl0_bsd_if_select_policy_2nd = wl_name[wl_ifnames.indexOf(wl0_bsd_if_select_policy[1])]; 
	var wl0_ifs_obj = document.form.wl0_bsd_if_select_policy_first;
	wl0_ifs_obj.value = wl0_names.indexOf(wl0_bsd_if_select_policy_1st);
	document.form.wl0_bsd_if_select_policy_second.value = (wl0_names.indexOf(wl0_bsd_if_select_policy_2nd) == -1 ? 2:wl0_names.indexOf(wl0_bsd_if_select_policy_2nd));
	change_bsd_if_select(wl0_ifs_obj);
	set_power(wl0_bsd_if_qualify_policy[0],'wl0_bsd_if_qualify_policy');
	document.getElementById('wl0_bsd_if_qualify_policy').value = wl0_bsd_if_qualify_policy[0];

	var wl1_bsd_if_select_policy_1st = wl_name[wl_ifnames.indexOf(wl1_bsd_if_select_policy[0])]; 
	var wl1_bsd_if_select_policy_2nd = wl_name[wl_ifnames.indexOf(wl1_bsd_if_select_policy[1])];
	var wl1_ifs_obj = document.form.wl1_bsd_if_select_policy_first;
	wl1_ifs_obj.value = wl1_names.indexOf(wl1_bsd_if_select_policy_1st);
	document.form.wl1_bsd_if_select_policy_second.value = (wl1_names.indexOf(wl1_bsd_if_select_policy_2nd) == -1 ? 2:wl1_names.indexOf(wl0_bsd_if_select_policy_2nd));
	change_bsd_if_select(wl1_ifs_obj);
	set_power(wl1_bsd_if_qualify_policy[0],'wl1_bsd_if_qualify_policy');
	document.getElementById('wl1_bsd_if_qualify_policy').value = wl1_bsd_if_qualify_policy[0];

	var wl2_bsd_if_select_policy_1st = wl_name[wl_ifnames.indexOf(wl2_bsd_if_select_policy[0])]; 
	var wl2_bsd_if_select_policy_2nd = wl_name[wl_ifnames.indexOf(wl2_bsd_if_select_policy[1])]; 
	var wl2_ifs_obj = document.form.wl2_bsd_if_select_policy_first;
	wl2_ifs_obj.value = wl2_names.indexOf(wl2_bsd_if_select_policy_1st);
	document.form.wl2_bsd_if_select_policy_second.value = (wl2_names.indexOf(wl2_bsd_if_select_policy_2nd) == -1 ? 2:wl2_names.indexOf(wl0_bsd_if_select_policy_2nd));
	change_bsd_if_select(wl2_ifs_obj);
	set_power(wl2_bsd_if_qualify_policy[0],'wl2_bsd_if_qualify_policy');
	document.getElementById('wl2_bsd_if_qualify_policy').value = wl2_bsd_if_qualify_policy[0];		

	/* [bounce detect] */
	document.form.windows_time_sec.value = bsd_bounce_detect[0];
	document.form.bsd_counts.value = bsd_bounce_detect[1];
	document.form.dwell_time_sec.value = bsd_bounce_detect[2];

}

function change_bsd_if_select(obj){
	var bws = new Array();
	var bwsDesc = new Array();
	bws = [0, 1, 2];
	if(obj == document.form.wl0_bsd_if_select_policy_first){
		bwsDesc = ['5GHz-1','5GHz-2','none'];
		bws.splice(obj.value, 1);
		bwsDesc.splice(obj.value, 1);
		add_options_x2(document.form.wl0_bsd_if_select_policy_second, bwsDesc, bws, document.form.wl0_bsd_if_select_policy_second.value);
	}else if(obj == document.form.wl1_bsd_if_select_policy_first){
		bwsDesc = ['5GHz-2','2.4GHz','none'];
		bws.splice(obj.value, 1);
		bwsDesc.splice(obj.value, 1);
		add_options_x2(document.form.wl1_bsd_if_select_policy_second, bwsDesc, bws, document.form.wl1_bsd_if_select_policy_second.value);		
	}else if(obj == document.form.wl2_bsd_if_select_policy_first){
		bwsDesc = ['5GHz-1','2.4GHz','none'];
		bws.splice(obj.value, 1);
		bwsDesc.splice(obj.value, 1);
		add_options_x2(document.form.wl2_bsd_if_select_policy_second, bwsDesc, bws, document.form.wl2_bsd_if_select_policy_second.value);		
	}
}

function reverse_bin(s){
    return s.split("").reverse().join("");
}

function createBinaryString (nMask) {
  // nMask must be between -2147483648 and 2147483647
  for (var nFlag = 0, nShifted = nMask, sMask = ""; nFlag < 32;
       nFlag++, sMask += String(nShifted >>> 31), nShifted <<= 1);
  return sMask;
}

function applyRule(){
	/* [bsd_steering_policy] - [bw util%, x, x, RSSI threshold, phy rate, flag] */
	var wl0_bsd_steering_policy_bin_t = wl0_bsd_steering_policy_bin.split("");
	wl0_bsd_steering_policy[0] = document.form.wl0_bsd_steering_bandutil.value;
	wl0_bsd_steering_policy[3] = document.form.wl0_bsd_steering_rssi.value;
	wl0_bsd_steering_policy[4] = document.form.wl0_bsd_steering_phy_l.value;
	wl0_bsd_steering_policy[5] = document.form.wl0_bsd_steering_phy_g.value;
	wl0_bsd_steering_policy_bin_t[1] = document.form.wl0_bsd_steering_rssi_s.value; 
	wl0_bsd_steering_policy_bin_t[6] = document.form.wl0_bsd_steering_balance.value;
	if(document.form.wl0_bsd_steering_vht_s.value == 0){
		wl0_bsd_steering_policy_bin_t[2] = 0;
		wl0_bsd_steering_policy_bin_t[3] = 0;
	}else if(document.form.wl0_bsd_steering_vht_s.value == 1){
		wl0_bsd_steering_policy_bin_t[2] = 0;
		wl0_bsd_steering_policy_bin_t[3] = 1;
	}else if(document.form.wl0_bsd_steering_vht_s.value == 2){
		wl0_bsd_steering_policy_bin_t[2] = 1;
		wl0_bsd_steering_policy_bin_t[3] = 0;
	}

	wl0_bsd_steering_policy[6] = '0x' + (parseInt(reverse_bin(wl0_bsd_steering_policy_bin_t.join("")),2)).toString(16);
	document.form.wl0_bsd_steering_policy.value = wl0_bsd_steering_policy.toString().replace(/,/g,' ');

	var wl1_bsd_steering_policy_bin_t = wl1_bsd_steering_policy_bin.split("");
	wl1_bsd_steering_policy[0] = document.form.wl1_bsd_steering_bandutil.value;
	wl1_bsd_steering_policy[3] = document.form.wl1_bsd_steering_rssi.value;
	wl1_bsd_steering_policy[4] = document.form.wl1_bsd_steering_phy_l.value;
	wl1_bsd_steering_policy[5] = document.form.wl1_bsd_steering_phy_g.value;
	wl1_bsd_steering_policy_bin_t[1] = document.form.wl1_bsd_steering_rssi_s.value; 
	wl1_bsd_steering_policy_bin_t[6] = document.form.wl1_bsd_steering_balance.value;

	if(document.form.wl1_bsd_steering_vht_s.value == 0){
		wl1_bsd_steering_policy_bin_t[2] = 0;
		wl1_bsd_steering_policy_bin_t[3] = 0;
	}else if(document.form.wl1_bsd_steering_vht_s.value == 1){
		wl1_bsd_steering_policy_bin_t[2] = 0;
		wl1_bsd_steering_policy_bin_t[3] = 1;
	}else if(document.form.wl1_bsd_steering_vht_s.value == 2){
		wl1_bsd_steering_policy_bin_t[2] = 1;
		wl1_bsd_steering_policy_bin_t[3] = 0;
	}

	wl1_bsd_steering_policy[6] = '0x' + (parseInt(reverse_bin(wl1_bsd_steering_policy_bin_t.join("")),2)).toString(16);
	document.form.wl1_bsd_steering_policy.value = wl1_bsd_steering_policy.toString().replace(/,/g,' ');

	var wl2_bsd_steering_policy_bin_t = wl2_bsd_steering_policy_bin.split("");
	wl2_bsd_steering_policy[0] = document.form.wl2_bsd_steering_bandutil.value;
	wl2_bsd_steering_policy[3] = document.form.wl2_bsd_steering_rssi.value;
	wl2_bsd_steering_policy[4] = document.form.wl2_bsd_steering_phy_l.value;
	wl2_bsd_steering_policy[5] = document.form.wl2_bsd_steering_phy_g.value;
	wl2_bsd_steering_policy_bin_t[1] = document.form.wl2_bsd_steering_rssi_s.value; 
	wl2_bsd_steering_policy_bin_t[6] = document.form.wl2_bsd_steering_balance.value;

	if(document.form.wl2_bsd_steering_vht_s.value == 0){
		wl2_bsd_steering_policy_bin_t[2] = 0;
		wl2_bsd_steering_policy_bin_t[3] = 0;
	}else if(document.form.wl2_bsd_steering_vht_s.value == 1){
		wl2_bsd_steering_policy_bin_t[2] = 0;
		wl2_bsd_steering_policy_bin_t[3] = 1;
	}else if(document.form.wl2_bsd_steering_vht_s.value == 2){
		wl2_bsd_steering_policy_bin_t[2] = 1;
		wl2_bsd_steering_policy_bin_t[3] = 0;
	}

	wl2_bsd_steering_policy[6] = '0x' + (parseInt(reverse_bin(wl2_bsd_steering_policy_bin_t.join("")),2)).toString(16);
	document.form.wl2_bsd_steering_policy.value = wl2_bsd_steering_policy.toString().replace(/,/g,' ');

	/* [bsd_sta_select_policy] - [x, RSSI, phy rate, x, x, x, x, x, x, flag] */
	var wl0_bsd_sta_select_policy_bin_t = wl0_bsd_sta_select_policy_bin.split("");
	wl0_bsd_sta_select_policy[1] = document.form.wl0_bsd_sta_select_policy_rssi.value;
	wl0_bsd_sta_select_policy[2] = document.form.wl0_bsd_sta_select_policy_phy_l.value;
	wl0_bsd_sta_select_policy[3] = document.form.wl0_bsd_sta_select_policy_phy_g.value;
	wl0_bsd_sta_select_policy_bin_t[1] = document.form.wl0_bsd_sta_select_policy_rssi_s.value;
	wl0_bsd_sta_select_policy_bin_t[6] = document.form.wl0_bsd_steering_balance.value;

	if(document.form.wl0_bsd_sta_select_policy_vht_s.value == 0){
		wl0_bsd_sta_select_policy_bin_t[2] = 0;
		wl0_bsd_sta_select_policy_bin_t[3] = 0;
	}else if(document.form.wl0_bsd_sta_select_policy_vht_s.value == 1){
		wl0_bsd_sta_select_policy_bin_t[2] = 0;
		wl0_bsd_sta_select_policy_bin_t[3] = 1;
	}else if(document.form.wl0_bsd_sta_select_policy_vht_s.value == 2){
		wl0_bsd_sta_select_policy_bin_t[2] = 1;
		wl0_bsd_sta_select_policy_bin_t[3] = 0;
	}

	wl0_bsd_sta_select_policy[10] = '0x' + (parseInt(reverse_bin(wl0_bsd_sta_select_policy_bin_t.join("")),2)).toString(16);
	document.form.wl0_bsd_sta_select_policy.value = wl0_bsd_sta_select_policy.toString().replace(/,/g,' ');

	var wl1_bsd_sta_select_policy_bin_t = wl1_bsd_sta_select_policy_bin.split("");
	wl1_bsd_sta_select_policy[1] = document.form.wl1_bsd_sta_select_policy_rssi.value;
	wl1_bsd_sta_select_policy[2] = document.form.wl1_bsd_sta_select_policy_phy_l.value;
	wl1_bsd_sta_select_policy[3] = document.form.wl1_bsd_sta_select_policy_phy_g.value;
	wl1_bsd_sta_select_policy_bin_t[1] = document.form.wl1_bsd_sta_select_policy_rssi_s.value;
	wl1_bsd_sta_select_policy_bin_t[6] = document.form.wl1_bsd_steering_balance.value;

	if(document.form.wl1_bsd_sta_select_policy_vht_s.value == 0){
		wl1_bsd_sta_select_policy_bin_t[2] = 0;
		wl1_bsd_sta_select_policy_bin_t[3] = 0;
	}else if(document.form.wl1_bsd_sta_select_policy_vht_s.value == 1){
		wl1_bsd_sta_select_policy_bin_t[2] = 0;
		wl1_bsd_sta_select_policy_bin_t[3] = 1;
	}else if(document.form.wl1_bsd_sta_select_policy_vht_s.value == 2){
		wl1_bsd_sta_select_policy_bin_t[2] = 1;
		wl1_bsd_sta_select_policy_bin_t[3] = 0;
	}

	wl1_bsd_sta_select_policy[10] = '0x' + (parseInt(reverse_bin(wl1_bsd_sta_select_policy_bin_t.join("")),2)).toString(16);
	document.form.wl1_bsd_sta_select_policy.value = wl1_bsd_sta_select_policy.toString().replace(/,/g,' ');

	var wl2_bsd_sta_select_policy_bin_t = wl2_bsd_sta_select_policy_bin.split("");
	wl2_bsd_sta_select_policy[1] = document.form.wl2_bsd_sta_select_policy_rssi.value;
	wl2_bsd_sta_select_policy[2] = document.form.wl2_bsd_sta_select_policy_phy_l.value;
	wl2_bsd_sta_select_policy[3] = document.form.wl2_bsd_sta_select_policy_phy_g.value;
	wl2_bsd_sta_select_policy_bin_t[1] = document.form.wl2_bsd_sta_select_policy_rssi_s.value;
	wl2_bsd_sta_select_policy_bin_t[6] = document.form.wl2_bsd_steering_balance.value;

	if(document.form.wl2_bsd_sta_select_policy_vht_s.value == 0){
		wl2_bsd_sta_select_policy_bin_t[2] = 0;
		wl2_bsd_sta_select_policy_bin_t[3] = 0;
	}else if(document.form.wl2_bsd_sta_select_policy_vht_s.value == 1){
		wl2_bsd_sta_select_policy_bin_t[2] = 0;
		wl2_bsd_sta_select_policy_bin_t[3] = 1;
	}else if(document.form.wl2_bsd_sta_select_policy_vht_s.value == 2){
		wl2_bsd_sta_select_policy_bin_t[2] = 1;
		wl2_bsd_sta_select_policy_bin_t[3] = 0;
	}

	wl2_bsd_sta_select_policy[10] = '0x' + (parseInt(reverse_bin(wl2_bsd_sta_select_policy_bin_t.join("")),2)).toString(16);
	document.form.wl2_bsd_sta_select_policy.value = wl2_bsd_sta_select_policy.toString().replace(/,/g,' ');

	/* [Interface Select and Qualify Procedures] */
	wl0_bsd_if_select_policy[0] = wl_ifnames[wl_name.indexOf(wl0_names[document.form.wl0_bsd_if_select_policy_first.value])];
	wl0_bsd_if_select_policy[1] = wl_ifnames[wl_name.indexOf(wl0_names[document.form.wl0_bsd_if_select_policy_second.value])];
	document.form.wl0_bsd_if_select_policy.value = wl0_bsd_if_select_policy.toString().replace(/,/g,' ');
	wl0_bsd_if_qualify_policy[0] = document.form.wl0_bsd_if_qualify_policy.value;
	document.form.wl0_bsd_if_qualify_policy.value = wl0_bsd_if_qualify_policy.toString().replace(/,/g,' ');

	wl1_bsd_if_select_policy[0] = wl_ifnames[wl_name.indexOf(wl1_names[document.form.wl1_bsd_if_select_policy_first.value])];
	wl1_bsd_if_select_policy[1] = wl_ifnames[wl_name.indexOf(wl1_names[document.form.wl1_bsd_if_select_policy_second.value])];
	document.form.wl1_bsd_if_select_policy.value = wl1_bsd_if_select_policy.toString().replace(/,/g,' ');
	wl1_bsd_if_qualify_policy[0] = document.form.wl1_bsd_if_qualify_policy.value;
	document.form.wl1_bsd_if_qualify_policy.value = wl1_bsd_if_qualify_policy.toString().replace(/,/g,' ');

	wl2_bsd_if_select_policy[0] = wl_ifnames[wl_name.indexOf(wl2_names[document.form.wl2_bsd_if_select_policy_first.value])];
	wl2_bsd_if_select_policy[1] = wl_ifnames[wl_name.indexOf(wl2_names[document.form.wl2_bsd_if_select_policy_second.value])];
	document.form.wl2_bsd_if_select_policy.value = wl2_bsd_if_select_policy.toString().replace(/,/g,' ');
	wl2_bsd_if_qualify_policy[0] = document.form.wl2_bsd_if_qualify_policy.value;
	document.form.wl2_bsd_if_qualify_policy.value = wl2_bsd_if_qualify_policy.toString().replace(/,/g,' ');

	bsd_bounce_detect[0] = document.form.windows_time_sec.value;
	bsd_bounce_detect[1] = document.form.bsd_counts.value;
	bsd_bounce_detect[2] = document.form.dwell_time_sec.value;
	document.form.bsd_bounce_detect.value = bsd_bounce_detect.toString().replace(/,/g,' ');;

	document.form.submit();

}

function restoreRule(){
	document.form.wl0_bsd_steering_policy.value = '<% nvram_default_get("wl0_bsd_steering_policy"); %>';
	document.form.wl1_bsd_steering_policy.value = '<% nvram_default_get("wl1_bsd_steering_policy"); %>';
	document.form.wl2_bsd_steering_policy.value = '<% nvram_default_get("wl2_bsd_steering_policy"); %>';
	document.form.wl0_bsd_sta_select_policy.value = '<% nvram_default_get("wl0_bsd_sta_select_policy"); %>';
	document.form.wl1_bsd_sta_select_policy.value = '<% nvram_default_get("wl1_bsd_sta_select_policy"); %>';	
	document.form.wl2_bsd_sta_select_policy.value = '<% nvram_default_get("wl2_bsd_sta_select_policy"); %>';
	document.form.wl0_bsd_if_select_policy.value = '<% nvram_default_get("wl0_bsd_if_select_policy"); %>';
	document.form.wl1_bsd_if_select_policy.value = '<% nvram_default_get("wl1_bsd_if_select_policy"); %>';
	document.form.wl2_bsd_if_select_policy.value = '<% nvram_default_get("wl2_bsd_if_select_policy"); %>';
	document.form.wl0_bsd_if_qualify_policy.value = '<% nvram_default_get("wl0_bsd_if_qualify_policy"); %>';
	document.form.wl1_bsd_if_qualify_policy.value = '<% nvram_default_get("wl1_bsd_if_qualify_policy"); %>';
	document.form.wl2_bsd_if_qualify_policy.value = '<% nvram_default_get("wl2_bsd_if_qualify_policy"); %>';
	document.form.bsd_bounce_detect.value = '<% nvram_default_get("bsd_bounce_detect"); %>';

	document.form.submit();
}

function register_event(){
	$j(function() {
		$j( "#slider_wl0_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl0_bsd_steering_bandutil_x').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_steering_bandutil');	  
			}
		}); 		
		$j( "#slider_wl1_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl1_bsd_steering_bandutil_x').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_steering_bandutil');	  
			}
		}); 
		$j( "#slider_wl2_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl2_bsd_steering_bandutil_x').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_steering_bandutil');	  
			}
		}); 		
		$j( "#slider_wl0_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl0_bsd_steering_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_steering_phy_l');	  
			}
		}); 	
		$j( "#slider_wl0_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl0_bsd_steering_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_steering_phy_g');	  
			}
		}); 			
		$j( "#slider_wl1_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl1_bsd_steering_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_steering_phy_l');	  
			}
		}); 	
		$j( "#slider_wl1_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl1_bsd_steering_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_steering_phy_g');	  
			}
		}); 		
		$j( "#slider_wl2_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl2_bsd_steering_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_steering_phy_l');	  
			}
		}); 	
		$j( "#slider_wl2_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl2_bsd_steering_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_steering_phy_g');	  
			}
		}); 			
		$j( "#slider_wl0_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl0_bsd_sta_select_policy_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_sta_select_policy_phy_l');	  
			}
		}); 
		$j( "#slider_wl0_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl0_bsd_sta_select_policy_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_sta_select_policy_phy_g');	  
			}
		}); 		
		$j( "#slider_wl1_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl1_bsd_sta_select_policy_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_sta_select_policy_phy_l');	  
			}
		}); 		
		$j( "#slider_wl1_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl1_bsd_sta_select_policy_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_sta_select_policy_phy_g');	  
			}
		}); 				
		$j( "#slider_wl2_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl2_bsd_sta_select_policy_phy_x_l').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_sta_select_policy_phy_l');	  
			}
		}); 	
		$j( "#slider_wl2_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl2_bsd_sta_select_policy_phy_x_g').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_sta_select_policy_phy_g'); 
			}
		}); 		
		$j( "#slider_wl0_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl0_bsd_if_qualify_policy_x').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl0_bsd_if_qualify_policy');	  
			}
		}); 	
		$j( "#slider_wl1_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl1_bsd_if_qualify_policy_x').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl1_bsd_if_qualify_policy');	  
			}
		}); 
		$j( "#slider_wl2_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl2_bsd_if_qualify_policy_x').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value,'wl2_bsd_if_qualify_policy');	  
			}
		}); 							
	});
}
function check_power(power_value,flag){

	if(power_value < 0){
		power_value = 0;
		alert("The minimun value of power is 0");
	}
	if(flag == 'per'){
		if(power_value > 100){
			power_value = 100;
			alert("The maximun value of power is 100");
		}
	}else if(flag == 'phyrate'){
		if(power_value > 1300){
			power_value = 1300;
			alert("The maximun value of PHY rate is 1300 Mpbs");
		}
	}
}

function set_power(power_value,flag){	

	if(flag == 'wl0_bsd_steering_bandutil'){
		$('slider_wl0_bandutil').children[0].style.width = power_value + "%";
		$('slider_wl0_bandutil').children[1].style.left = power_value + "%";
		document.form.wl0_bsd_steering_bandutil.value = power_value;
		document.getElementById('wl0_bsd_steering_bandutil_x').innerHTML = power_value;
		check_power(power_value,'per');		
	}else if(flag == 'wl1_bsd_steering_bandutil'){
		$('slider_wl1_bandutil').children[0].style.width = power_value + "%";
		$('slider_wl1_bandutil').children[1].style.left = power_value + "%";
		document.form.wl1_bsd_steering_bandutil.value = power_value;
		document.getElementById('wl1_bsd_steering_bandutil_x').innerHTML = power_value;
		check_power(power_value,'per');		
	}else if(flag == 'wl2_bsd_steering_bandutil'){
		$('slider_wl2_bandutil').children[0].style.width = power_value + "%";
		$('slider_wl2_bandutil').children[1].style.left = power_value + "%";
		document.form.wl2_bsd_steering_bandutil.value = power_value;
		document.getElementById('wl2_bsd_steering_bandutil_x').innerHTML = power_value;
		check_power(power_value,'per');		
	}else if(flag == 'wl0_bsd_steering_phy_l'){
		$('slider_wl0_steering_phy_l').children[0].style.width = power_value/6 + "%";
		$('slider_wl0_steering_phy_l').children[1].style.left = power_value/6 + "%";
		document.form.wl0_bsd_steering_phy_l.value = power_value;
		if(document.form.wl0_bsd_steering_phy_l.value == 0){
			document.getElementById('wl0_bsd_steering_phy_x_ld').style.display = ""; 
			document.getElementById('wl0_bsd_steering_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl0_bsd_steering_phy_x_ld').style.display = "none";
			document.getElementById('wl0_bsd_steering_phy_x_l0').style.display = "";
			document.getElementById('wl0_bsd_steering_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl1_bsd_steering_phy_l'){
		$('slider_wl1_steering_phy_l').children[0].style.width = power_value/13 + "%";
		$('slider_wl1_steering_phy_l').children[1].style.left = power_value/13 + "%";
		document.form.wl1_bsd_steering_phy_l.value = power_value;
		if(document.form.wl1_bsd_steering_phy_l.value == 0){
			document.getElementById('wl1_bsd_steering_phy_x_ld').style.display = ""; 
			document.getElementById('wl1_bsd_steering_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl1_bsd_steering_phy_x_ld').style.display = "none";
			document.getElementById('wl1_bsd_steering_phy_x_l0').style.display = "";
			document.getElementById('wl1_bsd_steering_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');						
	}else if(flag == 'wl2_bsd_steering_phy_l'){
		$('slider_wl2_steering_phy_l').children[0].style.width = power_value/13 + "%";
		$('slider_wl2_steering_phy_l').children[1].style.left = power_value/13 + "%";
		document.form.wl2_bsd_steering_phy_l.value = power_value;
		if(document.form.wl2_bsd_steering_phy_l.value == 0){
			document.getElementById('wl2_bsd_steering_phy_x_ld').style.display = ""; 
			document.getElementById('wl2_bsd_steering_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl2_bsd_steering_phy_x_ld').style.display = "none";
			document.getElementById('wl2_bsd_steering_phy_x_l0').style.display = "";
			document.getElementById('wl2_bsd_steering_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate'); 
	}else if(flag == 'wl0_bsd_steering_phy_g'){
		$('slider_wl0_steering_phy_g').children[0].style.width = power_value/6 + "%";
		$('slider_wl0_steering_phy_g').children[1].style.left = power_value/6 + "%";
		document.form.wl0_bsd_steering_phy_g.value = power_value;
		if(document.form.wl0_bsd_steering_phy_g.value == 0){
			document.getElementById('wl0_bsd_steering_phy_x_gd').style.display = ""; 
			document.getElementById('wl0_bsd_steering_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl0_bsd_steering_phy_x_gd').style.display = "none";
			document.getElementById('wl0_bsd_steering_phy_x_g0').style.display = "";
			document.getElementById('wl0_bsd_steering_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl1_bsd_steering_phy_g'){
		$('slider_wl1_steering_phy_g').children[0].style.width = power_value/13 + "%";
		$('slider_wl1_steering_phy_g').children[1].style.left = power_value/13 + "%";
		document.form.wl1_bsd_steering_phy_g.value = power_value;
		if(document.form.wl1_bsd_steering_phy_g.value == 0){
			document.getElementById('wl1_bsd_steering_phy_x_gd').style.display = ""; 
			document.getElementById('wl1_bsd_steering_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl1_bsd_steering_phy_x_gd').style.display = "none";
			document.getElementById('wl1_bsd_steering_phy_x_g0').style.display = "";
			document.getElementById('wl1_bsd_steering_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');						
	}else if(flag == 'wl2_bsd_steering_phy_g'){
		$('slider_wl2_steering_phy_g').children[0].style.width = power_value/13 + "%";
		$('slider_wl2_steering_phy_g').children[1].style.left = power_value/13 + "%";
		document.form.wl2_bsd_steering_phy_g.value = power_value;
		if(document.form.wl2_bsd_steering_phy_g.value == 0){
			document.getElementById('wl2_bsd_steering_phy_x_gd').style.display = ""; 
			document.getElementById('wl2_bsd_steering_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl2_bsd_steering_phy_x_gd').style.display = "none";
			document.getElementById('wl2_bsd_steering_phy_x_g0').style.display = "";
			document.getElementById('wl2_bsd_steering_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate'); 		
	}else if(flag == 'wl0_bsd_sta_select_policy_phy_l'){
		$('slider_wl0_sta_select_policy_phy_l').children[0].style.width = power_value/6 + "%";
		$('slider_wl0_sta_select_policy_phy_l').children[1].style.left = power_value/6 + "%";
		document.form.wl0_bsd_sta_select_policy_phy_l.value = power_value;
		if(document.form.wl0_bsd_sta_select_policy_phy_l.value == 0){
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_ld').style.display = ""; 
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_ld').style.display = "none";
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_l0').style.display = "";
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl1_bsd_sta_select_policy_phy_l'){
		$('slider_wl1_sta_select_policy_phy_l').children[0].style.width = power_value/13 + "%";
		$('slider_wl1_sta_select_policy_phy_l').children[1].style.left = power_value/13 + "%";
		document.form.wl1_bsd_sta_select_policy_phy_l.value = power_value;
		if(document.form.wl1_bsd_sta_select_policy_phy_l.value == 0){
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_ld').style.display = ""; 
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_ld').style.display = "none";
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_l0').style.display = "";
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl2_bsd_sta_select_policy_phy_l'){
		$('slider_wl2_sta_select_policy_phy_l').children[0].style.width = power_value/13 + "%";
		$('slider_wl2_sta_select_policy_phy_l').children[1].style.left = power_value/13 + "%";
		document.form.wl2_bsd_sta_select_policy_phy_l.value = power_value;
		if(document.form.wl2_bsd_sta_select_policy_phy_l.value == 0){
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_ld').style.display = ""; 
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_l0').style.display = "none";
		}else{
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_ld').style.display = "none";
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_l0').style.display = "";
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_l').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');		
	}else if(flag == 'wl0_bsd_sta_select_policy_phy_g'){
		$('slider_wl0_sta_select_policy_phy_g').children[0].style.width = power_value/6 + "%";
		$('slider_wl0_sta_select_policy_phy_g').children[1].style.left = power_value/6 + "%";
		document.form.wl0_bsd_sta_select_policy_phy_g.value = power_value;
		if(document.form.wl0_bsd_sta_select_policy_phy_g.value == 0){
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_gd').style.display = ""; 
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_gd').style.display = "none";
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_g0').style.display = "";
			document.getElementById('wl0_bsd_sta_select_policy_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl1_bsd_sta_select_policy_phy_g'){
		$('slider_wl1_sta_select_policy_phy_g').children[0].style.width = power_value/13 + "%";
		$('slider_wl1_sta_select_policy_phy_g').children[1].style.left = power_value/13 + "%";
		document.form.wl1_bsd_sta_select_policy_phy_g.value = power_value;
		if(document.form.wl1_bsd_sta_select_policy_phy_g.value == 0){
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_gd').style.display = ""; 
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_gd').style.display = "none";
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_g0').style.display = "";
			document.getElementById('wl1_bsd_sta_select_policy_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');
	}else if(flag == 'wl2_bsd_sta_select_policy_phy_g'){
		$('slider_wl2_sta_select_policy_phy_g').children[0].style.width = power_value/13 + "%";
		$('slider_wl2_sta_select_policy_phy_g').children[1].style.left = power_value/13 + "%";
		document.form.wl2_bsd_sta_select_policy_phy_g.value = power_value;
		if(document.form.wl2_bsd_sta_select_policy_phy_g.value == 0){
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_gd').style.display = ""; 
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_g0').style.display = "none";
		}else{
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_gd').style.display = "none";
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_g0').style.display = "";
			document.getElementById('wl2_bsd_sta_select_policy_phy_x_g').innerHTML = power_value;
		}
		check_power(power_value,'phyrate');			
	}else if(flag == 'wl0_bsd_if_qualify_policy'){
		$('slider_wl0_if_qualify_policy').children[0].style.width = power_value + "%";
		$('slider_wl0_if_qualify_policy').children[1].style.left = power_value + "%";
		document.form.wl0_bsd_if_qualify_policy.value = power_value;
		document.getElementById('wl0_bsd_if_qualify_policy_x').innerHTML = power_value;
		check_power(power_value,'per');				
	}else if(flag == 'wl1_bsd_if_qualify_policy'){
		$('slider_wl1_if_qualify_policy').children[0].style.width = power_value + "%";
		$('slider_wl1_if_qualify_policy').children[1].style.left = power_value + "%";
		document.form.wl1_bsd_if_qualify_policy.value = power_value;
		document.getElementById('wl1_bsd_if_qualify_policy_x').innerHTML = power_value;
		check_power(power_value,'per');		
	}else if(flag == 'wl2_bsd_if_qualify_policy'){
		$('slider_wl2_if_qualify_policy').children[0].style.width = power_value + "%";
		$('slider_wl2_if_qualify_policy').children[1].style.left = power_value + "%";
		document.form.wl2_bsd_if_qualify_policy.value = power_value;
		document.getElementById('wl2_bsd_if_qualify_policy_x').innerHTML = power_value;
		check_power(power_value,'per');
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


<input type="hidden" name="wl_nmode_x" value="<% nvram_get("wl_nmode_x"); %>">
<input type="hidden" name="wl_gmode_protection_x" value="<% nvram_get("wl_gmode_protection_x"); %>">
<input type="hidden" name="current_page" value="Advanced_Smart_Connect.asp">
<input type="hidden" name="next_page" value="Advanced_Smart_Connect.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl0_bsd_steering_policy" value="">
<input type="hidden" name="wl1_bsd_steering_policy" value="">
<input type="hidden" name="wl2_bsd_steering_policy" value="">
<input type="hidden" name="wl0_bsd_steering_bandutil" id="wl0_bsd_steering_bandutil" value="">
<input type="hidden" name="wl1_bsd_steering_bandutil" id="wl1_bsd_steering_bandutil" value="">
<input type="hidden" name="wl2_bsd_steering_bandutil" id="wl2_bsd_steering_bandutil" value="">
<input type="hidden" name="wl0_bsd_steering_phy_l" id="wl0_bsd_steering_phy_l" value="">
<input type="hidden" name="wl1_bsd_steering_phy_l" id="wl1_bsd_steering_phy_l" value="">
<input type="hidden" name="wl2_bsd_steering_phy_l" id="wl2_bsd_steering_phy_l" value="">
<input type="hidden" name="wl0_bsd_steering_phy_g" id="wl0_bsd_steering_phy_g" value="">
<input type="hidden" name="wl1_bsd_steering_phy_g" id="wl1_bsd_steering_phy_g" value="">
<input type="hidden" name="wl2_bsd_steering_phy_g" id="wl2_bsd_steering_phy_g" value="">
<input type="hidden" name="wl0_bsd_sta_select_policy" value="">
<input type="hidden" name="wl1_bsd_sta_select_policy" value="">
<input type="hidden" name="wl2_bsd_sta_select_policy" value="">
<input type="hidden" name="wl0_bsd_if_select_policy" value="">
<input type="hidden" name="wl1_bsd_if_select_policy" value="">
<input type="hidden" name="wl2_bsd_if_select_policy" value="">
<input type="hidden" name="wl0_bsd_sta_select_policy_phy_l" id="wl0_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl1_bsd_sta_select_policy_phy_l" id="wl1_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl2_bsd_sta_select_policy_phy_l" id="wl2_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl0_bsd_sta_select_policy_phy_g" id="wl0_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl1_bsd_sta_select_policy_phy_g" id="wl1_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl2_bsd_sta_select_policy_phy_g" id="wl2_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl0_bsd_if_qualify_policy" id="wl0_bsd_if_qualify_policy" value="">
<input type="hidden" name="wl1_bsd_if_qualify_policy" id="wl1_bsd_if_qualify_policy" value="">
<input type="hidden" name="wl2_bsd_if_qualify_policy" id="wl2_bsd_if_qualify_policy" value="">
<input type="hidden" name="bsd_bounce_detect" value="">
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
	<td align="left" valign="top" >
	  <table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_1#> - Smart Connect Rule</div>
     	  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      	  <div class="formfontdesc"><#smart_connect_hint#></div>
		
			<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable1">			
			<thead>
				<tr>
					<td colspan="4"><#smart_connect_Steering#></td>
				</tr>			
			</thead>
			<tr>
				<th><#Interface#></th>
				<td width="27%" align="center" >2.4GHz</td><td width="27%" align="center" >5GHz-1</td><td width="27%" align="center" >5GHz-2</td>
			</tr>
			<tr>
				<th><#smart_connect_Bandwidth#></th>
				<td>
					<div>
						<table>
						<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl0_bandutil" style="width:80px;"></div>
								</td>								
								<td style="border:0px;">
									<span id="wl0_bsd_steering_bandutil_x" name="wl0_bsd_steering_bandutil_x" style="color:white"></span>%	
								</td>									
						</tr>
						</table>
					</div>
				</td>				
				<td>
					<div>
						<table>
						<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl1_bandutil" style="width:80px;"></div>
								</td>								
								<td style="border:0px;">
									<span id="wl1_bsd_steering_bandutil_x" name="wl1_bsd_steering_bandutil_x" style="color:white"></span>%	
								</td>									
						</tr>
						</table>
					</div>
				</td>
				<td>
					<div>
						<table>
						<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl2_bandutil" style="width:80px;"></div>
								</td>								
								<td style="border:0px;">
									<span id="wl2_bsd_steering_bandutil_x" name="wl2_bsd_steering_bandutil_x" style="color:white"></span>%	
								</td>									
						</tr>
						</table>
					</div>
				</td>				
			</tr>	
			<tr>
				<th>Enable Load Balance</th>
					<td>
						<input type="radio" name="wl0_bsd_steering_balance" value="1"><#checkbox_Yes#>
						<input type="radio" name="wl0_bsd_steering_balance" value="0"><#checkbox_No#>
					</td>
					<td>
						<input type="radio" name="wl1_bsd_steering_balance" value="1"><#checkbox_Yes#>
						<input type="radio" name="wl1_bsd_steering_balance" value="0"><#checkbox_No#>
					</td>
					<td>
						<input type="radio" name="wl2_bsd_steering_balance" value="1"><#checkbox_Yes#>
						<input type="radio" name="wl2_bsd_steering_balance" value="0"><#checkbox_No#>
					</td>
			</tr>
			<tr>
				<th>RSSI</th>
				<td>
				<div>
					<table>
						<tr>				
					<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl0_bsd_steering_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
					</td>

						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl0_bsd_steering_rssi" name="wl0_bsd_steering_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>
				<td>
				<div>
					<table>
						<tr>				
					<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl1_bsd_steering_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
					</td>

						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl1_bsd_steering_rssi" name="wl1_bsd_steering_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>
				<td>
				<div>
					<table>
						<tr>				
					<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl2_bsd_steering_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
					</td>

						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl2_bsd_steering_rssi" name="wl2_bsd_steering_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>				
		  	</tr>
			<tr>
				<th>PHY Rate Less</th>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl0_steering_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl0_bsd_steering_phy_x_l0">< <span style="color:white;" name="wl0_bsd_steering_phy_x_l" id="wl0_bsd_steering_phy_x_l">300</span> Mbps</div>
							<div id="wl0_bsd_steering_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>				
					</table>				
				</div>					
				</td>				
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl1_steering_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl1_bsd_steering_phy_x_l0">< <span style="color:white;" name="wl1_bsd_steering_phy_x_l" id="wl1_bsd_steering_phy_x_l">300</span> Mbps</div>
							<div id="wl1_bsd_steering_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>				
					</table>				
				</div>					
				</td>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl2_steering_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl2_bsd_steering_phy_x_l0">< <span style="color:white;" name="wl2_bsd_steering_phy_x_l" id="wl2_bsd_steering_phy_x_l">300</span> Mbps</div>
							<div id="wl2_bsd_steering_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>					
					</table>				
				</div>					
				</td>				
			</tr>	
				<th>PHY Rate Greater</th>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl0_steering_phy_g" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl0_bsd_steering_phy_x_g0">> <span style="color:white;" name="wl0_bsd_steering_phy_x_g" id="wl0_bsd_steering_phy_x_g">300</span> Mbps</div>
							<div id="wl0_bsd_steering_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>				
					</table>				
				</div>					
				</td>				
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl1_steering_phy_g" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl1_bsd_steering_phy_x_g0">> <span style="color:white;" name="wl1_bsd_steering_phy_x_g" id="wl1_bsd_steering_phy_x_g">300</span> Mbps</div>
							<div id="wl1_bsd_steering_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>				
					</table>				
				</div>					
				</td>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl2_steering_phy_g" style="width:80px;"></div>
						</td>
						<td style="border:0px">
							<div id="wl2_bsd_steering_phy_x_g0">> <span style="color:white;" name="wl2_bsd_steering_phy_x_g" id="wl2_bsd_steering_phy_x_g">300</span> Mbps</div>
							<div id="wl2_bsd_steering_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>						
						</tr>					
					</table>				
				</div>					
				</td>				
			</tr>			
			<tr>
				<th>VHT</th>
					<td style="padding-left:12px">
						<select class="input_option" name="wl0_bsd_steering_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>		
					<td style="padding-left:12px">
						<select class="input_option" name="wl1_bsd_steering_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>											
					<td style="padding-left:12px">
						<select class="input_option" name="wl2_bsd_steering_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>																	
			</tr>				
		  	</table>
		  	<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">
			<thead>
				<tr>
					<td colspan="4"><#smart_connect_STA#></td>
				</tr>
			</thead>	
			<tr>
				<th>RSSI</th>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl0_bsd_sta_select_policy_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
						</td>													
						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl0_bsd_sta_select_policy_rssi" name="wl0_bsd_sta_select_policy_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl1_bsd_sta_select_policy_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
						</td>													
						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl1_bsd_sta_select_policy_rssi" name="wl1_bsd_sta_select_policy_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>								
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px; padding-left:0px">
							<select class="input_option" name="wl2_bsd_sta_select_policy_rssi_s">
								<option selected="" value="0" class="content_input_fd">Less</option>
								<option value="1" class="content_input_fd">Greater</option>
							</select>
						</td>													
						<td style="border:0px; padding-left:0px">
							<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl2_bsd_sta_select_policy_rssi" name="wl2_bsd_sta_select_policy_rssi" maxlength="4">
							<label style="margin-left:5px;">dBm</label>
						</td>			
						</tr>				
					</table>				
				</div>					
				</td>
		  	</tr>
			<tr>
				<th>PHY Rate Less</th>
				<td>
				<div>
					<table>
						<tr>	
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl0_sta_select_policy_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px;">
							<div id="wl0_bsd_sta_select_policy_phy_x_l0">< <span style="color:white;" name="wl0_bsd_sta_select_policy_phy_x_l" id="wl0_bsd_sta_select_policy_phy_x_l">300</span> Mbps</div>
							<div id="wl0_bsd_sta_select_policy_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>		
						</tr>				
					</table>				
				</div>					
				</td>

				<td>
				<div>
					<table>
						<tr>						
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl1_sta_select_policy_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px;">							
							<div id="wl1_bsd_sta_select_policy_phy_x_l0">< <span style="color:white;" name="wl1_bsd_sta_select_policy_phy_x_l" id="wl1_bsd_sta_select_policy_phy_x_l">300</span> Mbps</div>	
							<div id="wl1_bsd_sta_select_policy_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>		
						</tr>				
					</table>				
				</div>					
				</td>

				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl2_sta_select_policy_phy_l" style="width:80px;"></div>
						</td>
						<td style="border:0px;">							
							<div id="wl2_bsd_sta_select_policy_phy_x_l0">< <span style="color:white;" name="wl2_bsd_sta_select_policy_phy_x_l" id="wl2_bsd_sta_select_policy_phy_x_l">300</span> Mbps</div>
							<div id="wl2_bsd_sta_select_policy_phy_x_ld" style="display:none;"><#btn_disable#></div>
						</td>															
						</tr>				
					</table>				
				</div>					
				</td>
			</tr>	
			<tr>
			<tr>
				<th>PHY Rate Greater</th>
				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl0_sta_select_policy_phy_g" style="width:80px;"></div>	
						</td>
						<td style="border:0px;">							
							<div id="wl0_bsd_sta_select_policy_phy_x_g0">> <span style="color:white;" name="wl0_bsd_sta_select_policy_phy_x_g" id="wl0_bsd_sta_select_policy_phy_x_g">300</span> Mbps</div>
							<div id="wl0_bsd_sta_select_policy_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>		
						</tr>				
					</table>				
				</div>					
				</td>

				<td>
				<div>
					<table>
						<tr>					
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl1_sta_select_policy_phy_g" style="width:80px;"></div>	
						</td>
						<td style="border:0px;">
							<div id="wl1_bsd_sta_select_policy_phy_x_g0">> <span style="color:white;" name="wl1_bsd_sta_select_policy_phy_x_g" id="wl1_bsd_sta_select_policy_phy_x_g">300</span> Mbps</div>
							<div id="wl1_bsd_sta_select_policy_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>		
						</tr>				
					</table>				
				</div>					
				</td>

				<td>
				<div>
					<table>
						<tr>
						<td style="border:0px;width:35px; padding-left:0px">
							<div id="slider_wl2_sta_select_policy_phy_g" style="width:80px;"></div>	
						</td>
						<td style="border:0px;">							
							<div id="wl2_bsd_sta_select_policy_phy_x_g0">> <span style="color:white;" name="wl2_bsd_sta_select_policy_phy_x_g" id="wl2_bsd_sta_select_policy_phy_x_g">300</span> Mbps</div>
							<div id="wl2_bsd_sta_select_policy_phy_x_gd" style="display:none;"><#btn_disable#></div>
						</td>															
						</tr>				
					</table>				
				</div>					
				</td>
			</tr>	
			<tr>			
				<th>VHT</th>
					<td style="padding-left:12px">
						<select class="input_option" name="wl0_bsd_sta_select_policy_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>	
					<td style="padding-left:12px">
						<select class="input_option" name="wl1_bsd_sta_select_policy_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>										
					<td style="padding-left:12px">
						<select class="input_option" name="wl2_bsd_sta_select_policy_vht_s">
							<option selected="" value="0" class="content_input_fd"><#All#></option>
							<option value="1" class="content_input_fd">AC only</option>
							<option value="2" class="content_input_fd">not-allowed</option>
						</select>
					</td>																	
			</tr>	
		  	</table>

		  	<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">
			<thead>
				<tr>
					<td colspan="4"><#smart_connect_ISQP#></td>
				</tr>
			</thead>				
			<tr>
				<th><#Interface_target#></th>
					<td style="padding:0px 0px 0px 0px;">
					<div>
						<table>
						<tr>
							<td style="border:0px; padding:0px 0px 0px 3px;">1:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl0_bsd_if_select_policy_first" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-1</option>
									<option value="1" class="content_input_fd">5GHz-2</option>
								</select>
							</td>
							<td style="border:0px; padding:0px 0px 0px 7px;">2:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl0_bsd_if_select_policy_second" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-1</option>
									<option value="1" class="content_input_fd">5GHz-2</option>
									<option value="2" class="content_input_fd">none</option>
								</select>
							</td>							
						</tr>
						</table>
					</div>	
					</td>	
					<td style="padding:0px 0px 0px 0px;">
					<div>
						<table>
						<tr>
							<td style="border:0px; padding:0px 0px 0px 3px;">1:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl1_bsd_if_select_policy_first" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-2</option>
									<option value="1" class="content_input_fd">2.4GHz</option>
								</select>
							</td>
							<td style="border:0px; padding:0px 0px 0px 7px;">2:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl1_bsd_if_select_policy_second" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-2</option>
									<option value="1" class="content_input_fd">2.4GHz</option>
									<option value="2" class="content_input_fd">none</option>
								</select>
							</td>							
						</tr>
						</table>
					</div>	
					</td>	
					<td style="padding:0px 0px 0px 0px;">
					<div>
						<table>
						<tr>
							<td style="border:0px; padding:0px 0px 0px 3px;">1:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl2_bsd_if_select_policy_first" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-1</option>
									<option value="1" class="content_input_fd">2.4GHz</option>
								</select>
							</td>
							<td style="border:0px; padding:0px 0px 0px 7px;">2:</td>
							<td style="border:0px; padding:0px 0px 0px 1px;">
								<select class="input_option" name="wl2_bsd_if_select_policy_second" onChange="change_bsd_if_select(this);">
									<option selected="" value="0" class="content_input_fd">5GHz-1</option>
									<option value="1" class="content_input_fd">2.4GHz</option>
									<option value="2" class="content_input_fd">none</option>
								</select>
							</td>							
						</tr>
						</table>
					</div>	
					</td>																
		  	</tr>
			<tr>
				<th>Bandwidth Utilization</th>
				<td>
					<div>
						<table>
							<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl0_if_qualify_policy" style="width:80px;"></div>									
								</td>							
								<td style="border:0px;">
									<span id="wl0_bsd_if_qualify_policy_x" name="wl0_bsd_if_qualify_policy_x" style="color:white"></span>%
								</td>
						</tr>
						</table>
					</div>
				</td>
				<td>
					<div>
						<table>
							<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl1_if_qualify_policy" style="width:80px;"></div>									
								</td>							
								<td style="border:0px;">
									<span id="wl1_bsd_if_qualify_policy_x" name="wl1_bsd_if_qualify_policy_x" style="color:white"></span>%
								</td>
						</tr>
						</table>
					</div>
				</td>
				<td>
					<div>
						<table>
							<tr>
								<td style="border:0px;width:35px; padding-left:0px">
									<div id="slider_wl2_if_qualify_policy" style="width:80px;"></div>									
								</td>							
								<td style="border:0px;">
									<span id="wl2_bsd_if_qualify_policy_x" name="wl2_bsd_if_qualify_policy_x" style="color:white"></span>%
								</td>
						</tr>
						</table>
					</div>
				</td>								
			</tr>	

		  	</table>		

		  	<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">
			<thead>
				<tr>
					<td colspan="2"><#smart_connect_BD#></td>
				</tr>
			</thead>	
			<tr>
				<th><#smart_connect_STA_window#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="windows_time_sec" maxlength="4"> <#Second#>
					</td>								
		  	</tr>	
			<tr>
				<th><#smart_connect_STA_counts#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="bsd_counts" maxlength="4">
					</td>									
		  	</tr>
			<tr>
				<th><#smart_connect_STA_dwell#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="dwell_time_sec" maxlength="4"> <#Second#>
					</td>										
		  	</tr>		  			  						  	  	
		  	</table>			  	  	
			  
				<div class="apply_gen">
					<input type="button" id="restoreButton" class="button_gen" value="<#Setting_factorydefault_value#>" onclick="restoreRule();">
					<input type="button" id="applyButton" class="button_gen" value="<#CTL_apply#>" onclick="applyRule();">
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
	<td width="10" align="center" valign="top"></td>
  </tr>
</table>

<div id="footer"></div>
</body>
</html>
