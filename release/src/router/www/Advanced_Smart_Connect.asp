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
<link rel="stylesheet" type="text/css" href="/device-map/device-map.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
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

#slider_wl0_bsd_steering_bandutil .ui-slider-range,
#slider_wl1_bsd_steering_bandutil .ui-slider-range,
#slider_wl2_bsd_steering_bandutil .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;

}
#slider_wl0_bsd_steering_bandutil .ui-slider-handle,
#slider_wl1_bsd_steering_bandutil .ui-slider-handle,
#slider_wl2_bsd_steering_bandutil .ui-slider-handle
 { border-color: #414141; }

#slider_wl0_bsd_steering_phy_l .ui-slider-handle,
#slider_wl1_bsd_steering_phy_l .ui-slider-handle,
#slider_wl2_bsd_steering_phy_l .ui-slider-handle
 { border-color: #414141; }

#slider_wl0_bsd_steering_phy_l .ui-slider-range,
#slider_wl1_bsd_steering_phy_l .ui-slider-range,
#slider_wl2_bsd_steering_phy_l .ui-slider-range
 {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}

#slider_wl0_bsd_steering_phy_g .ui-slider-range,
#slider_wl1_bsd_steering_phy_g .ui-slider-range,
#slider_wl2_bsd_steering_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_bsd_steering_phy_g .ui-slider-handle,
#slider_wl1_bsd_steering_phy_g .ui-slider-handle,
#slider_wl2_bsd_steering_phy_g .ui-slider-handle
 { border-color: #414141; }

#slider_wl0_bsd_sta_select_policy_phy_l .ui-slider-range,
#slider_wl1_bsd_sta_select_policy_phy_l .ui-slider-range,
#slider_wl2_bsd_sta_select_policy_phy_l .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_bsd_sta_select_policy_phy_l .ui-slider-handle,
#slider_wl1_bsd_sta_select_policy_phy_l .ui-slider-handle,
#slider_wl2_bsd_sta_select_policy_phy_l .ui-slider-handle
 { border-color: #414141; }

#slider_wl0_bsd_sta_select_policy_phy_g .ui-slider-range,
#slider_wl1_bsd_sta_select_policy_phy_g .ui-slider-range,
#slider_wl2_bsd_sta_select_policy_phy_g .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_bsd_sta_select_policy_phy_g .ui-slider-handle,
#slider_wl1_bsd_sta_select_policy_phy_g .ui-slider-handle,
#slider_wl2_bsd_sta_select_policy_phy_g .ui-slider-handle
 { border-color: #414141; }

#slider_wl0_bsd_if_qualify_policy .ui-slider-range,
#slider_wl1_bsd_if_qualify_policy .ui-slider-range,
#slider_wl2_bsd_if_qualify_policy .ui-slider-range {
	background: #0084d9; 

	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider_wl0_bsd_if_qualify_policy .ui-slider-handle,
#slider_wl1_bsd_if_qualify_policy .ui-slider-handle,
#slider_wl2_bsd_if_qualify_policy .ui-slider-handle
 { border-color: #414141; }

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

.FormTable td span{
	color:#FFFFFF;
}

.steering_off_0, .steering_off_1, .steering_off_2{
	display: none;
}

.steering_on_0, .steering_on_1, .steering_on_2{
	display: block;
	color:#FFFFFF;
}

.steering_unuse_off_0, .steering_unuse_off_1, .steering_unuse_off_2{
	display: none;
}

.steering_unuse_on_0, .steering_unuse_on_1, .steering_unuse_on_2{
	display: block;
	color:#FFFFFF;
	text-align:center;
}
</style>
<script>

<% wl_get_parameter(); %>

var wl0_names = new Array();
var wl1_names = new Array();
var wl2_names = new Array();
var wl_name = new Array();
var wl0_names = ['5GHz-1','5GHz-2','none'];
var wl1_names = ['5GHz-2','2.4GHz','none'];
var wl2_names = ['5GHz-1','2.4GHz','none'];
var wl_name = ['2.4GHz','5GHz-1','5GHz-2'];
var wl_names = [wl0_names,wl1_names,wl2_names];

var wl_ifnames = '<% nvram_get("wl_ifnames"); %>'.split(" ");

var start_band_idx=0;

if('<% nvram_get("smart_connect_x"); %>' == 2){
start_band_idx = 1;
/* [bsd_steering_policy] */
var bsd_steering_policy = ['<% nvram_get("wl0_bsd_steering_policy"); %>'.split(" "),
			   '<% nvram_get("wl1_bsd_steering_policy_x"); %>'.split(" "),
			   '<% nvram_get("wl2_bsd_steering_policy_x"); %>'.split(" ")];

var bsd_steering_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_steering_policy[0][6]))),
			       reverse_bin(createBinaryString(parseInt(bsd_steering_policy[1][6]))),
			       reverse_bin(createBinaryString(parseInt(bsd_steering_policy[2][6])))];

/* [bsd_sta_select_policy] */
var bsd_sta_select_policy = ['<% nvram_get("wl0_bsd_sta_select_policy"); %>'.split(" "),
			     '<% nvram_get("wl1_bsd_sta_select_policy_x"); %>'.split(" "),
			     '<% nvram_get("wl2_bsd_sta_select_policy_x"); %>'.split(" ")];

var bsd_sta_select_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[0][10]))),
				 reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[1][10]))),
				 reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[2][10])))];

/* [Interface Select and Qualify Procedures] */
var bsd_if_select_policy = ['<% nvram_get("wl0_bsd_if_select_policy"); %>'.split(" "),
			    '<% nvram_get("wl1_bsd_if_select_policy_x"); %>'.split(" "),
			    '<% nvram_get("wl2_bsd_if_select_policy_x"); %>'.split(" ")]

var bsd_if_qualify_policy = ['<% nvram_get("wl0_bsd_if_qualify_policy"); %>'.split(" "),
			     '<% nvram_get("wl1_bsd_if_qualify_policy_x"); %>'.split(" "),
			     '<% nvram_get("wl2_bsd_if_qualify_policy_x"); %>'.split(" ")]

var bsd_if_qualify_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[0][1]))),
				 reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[1][1]))),
				 reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[2][1])))]

/*[bounce detect] */
var bsd_bounce_detect = '<% nvram_get("bsd_bounce_detect_x"); %>'.split(" "); //[windows time in sec, counts, dwell time in sec]
}else{
/* [bsd_steering_policy] */  
//[bw util%, x, x, RSSI threshold, phy rate, flag]
var bsd_steering_policy = ['<% nvram_get("wl0_bsd_steering_policy"); %>'.split(" "),
			   '<% nvram_get("wl1_bsd_steering_policy"); %>'.split(" "),
			   '<% nvram_get("wl2_bsd_steering_policy"); %>'.split(" ")];

//[OP, RSSI, VHT, NONVHT, N_RF, PHYRATE, L_BALANCE,...]
var bsd_steering_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_steering_policy[0][6]))),
                               reverse_bin(createBinaryString(parseInt(bsd_steering_policy[1][6]))),
                               reverse_bin(createBinaryString(parseInt(bsd_steering_policy[2][6])))];

/* [bsd_sta_select_policy] */
var bsd_sta_select_policy = ['<% nvram_get("wl0_bsd_sta_select_policy"); %>'.split(" "),
			     '<% nvram_get("wl1_bsd_sta_select_policy"); %>'.split(" "),
			     '<% nvram_get("wl2_bsd_sta_select_policy"); %>'.split(" ")];

var bsd_sta_select_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[0][10]))),
				 reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[1][10]))),
				 reverse_bin(createBinaryString(parseInt(bsd_sta_select_policy[2][10])))];

/* [Interface Select and Qualify Procedures] */
var bsd_if_select_policy = ['<% nvram_get("wl0_bsd_if_select_policy"); %>'.split(" "),
			    '<% nvram_get("wl1_bsd_if_select_policy"); %>'.split(" "),
			    '<% nvram_get("wl2_bsd_if_select_policy"); %>'.split(" ")]

var bsd_if_qualify_policy = ['<% nvram_get("wl0_bsd_if_qualify_policy"); %>'.split(" "),
			     '<% nvram_get("wl1_bsd_if_qualify_policy"); %>'.split(" "),
			     '<% nvram_get("wl2_bsd_if_qualify_policy"); %>'.split(" ")]

var bsd_if_qualify_policy_bin = [reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[0][1]))),
				 reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[1][1]))),
				 reverse_bin(createBinaryString(parseInt(bsd_if_qualify_policy[2][1])))]

/*[bounce detect] */
var bsd_bounce_detect = '<% nvram_get("bsd_bounce_detect"); %>'.split(" ");	//[windows time in sec, counts, dwell time in sec]
}

function initial(){
	show_menu();
	gen_bsd_steering_div();
	gen_bsd_sta_select_div();
	gen_bsd_if_select_div();
	register_event();
	handle_bsd_nvram();
	bsd_disable('<% nvram_get("smart_connect_x"); %>');
}

function bsd_disable(val){
	if(val == '2'){
		document.form.wl1_bsd_steering_policy_x.disabled = false;
		document.form.wl2_bsd_steering_policy_x.disabled = false;
		document.form.wl1_bsd_sta_select_policy_x.disabled = false;
		document.form.wl2_bsd_sta_select_policy_x.disabled = false;
		document.form.wl1_bsd_if_select_policy_x.disabled = false;
		document.form.wl2_bsd_if_select_policy_x.disabled = false;
		document.form.wl1_bsd_if_qualify_policy_x.disabled = false;
		document.form.wl2_bsd_if_qualify_policy_x.disabled = false;
		document.form.bsd_bounce_detect_x.disabled = false;
		document.form.bsd_ifnames_x.disabled = false;
	}else{
		document.form.wl0_bsd_steering_policy.disabled = false;
		document.form.wl1_bsd_steering_policy.disabled = false;
		document.form.wl2_bsd_steering_policy.disabled = false;
		document.form.wl0_bsd_sta_select_policy.disabled = false;
		document.form.wl1_bsd_sta_select_policy.disabled = false;
		document.form.wl2_bsd_sta_select_policy.disabled = false;
		document.form.wl0_bsd_if_select_policy.disabled = false;
		document.form.wl1_bsd_if_select_policy.disabled = false;
		document.form.wl2_bsd_if_select_policy.disabled = false;
		document.form.wl0_bsd_if_qualify_policy.disabled = false;
		document.form.wl1_bsd_if_qualify_policy.disabled = false;
		document.form.wl2_bsd_if_qualify_policy.disabled = false;
		document.form.bsd_bounce_detect.disabled = false;
		document.form.bsd_ifnames.disabled = false;
	}
}

function change_lb(val,idx){
	if(val == 1){
		var obj_steer_class = document.getElementsByClassName('steering_on_'+idx);
		var obj_steer_unset_class = document.getElementsByClassName('steering_unuse_off_'+idx);
		for(x=obj_steer_class.length-1;x>=0;x--){
			obj_steer_class[x].className='steering_off_'+idx;
		}
		for(x=obj_steer_unset_class.length-1;x>=0;x--){
			obj_steer_unset_class[x].className='steering_unuse_on_'+idx;
		}
	}else{
		var obj_steer_class = document.getElementsByClassName('steering_off_'+idx);
		var obj_steer_unset_class = document.getElementsByClassName('steering_unuse_on_'+idx);
		for(x=obj_steer_unset_class.length-1;x>=0;x--){
			obj_steer_unset_class[x].className='steering_unuse_off_'+idx;
		}			
		for(x=obj_steer_class.length-1;x>=0;x--){
			obj_steer_class[x].className='steering_on_'+idx;
		}				
	}
}

function gen_bsd_steering_div(flag){
	
	var code = "";

	code +='<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable1">';
	code +='<thead><tr><td colspan="4"><#smart_connect_Steering#></td></tr></thead>';
	code +='<tr><th width="20%"><#Interface#></th>';
	if(start_band_idx == '1')
		code +='<td width="40%" align="center" >5GHz-1</td><td width="40%" align="center" >5GHz-2</td>';
	else
		code +='<td width="27%" align="center" >2.4GHz</td><td width="27%" align="center" >5GHz-1</td><td width="27%" align="center" >5GHz-2</td>';
	code +='</tr>';

	code +='<tr><th>Enable Load Balance</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td>';
		code +='<input onclick="change_lb(1,'+i+');" type="radio" name="wl'+i+'_bsd_steering_balance" value="1"><#checkbox_Yes#>';
		code +='<input onclick="change_lb(0,'+i+');" type="radio" name="wl'+i+'_bsd_steering_balance" value="0"><#checkbox_No#>';
		code +='</td>';
	}
	code +='</tr>';

		code +='<tr><th><#smart_connect_Bandwidth#></th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){	
		code +='<td><span class="steering_on_'+i+'"><div><table><tr>';
		code +='<td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_steering_bandutil" style="width:80px;"></div></td>';
		code +='<td style="border:0px;">';
		code +='<span id="wl'+i+'_bsd_steering_bandutil_t" name="wl0_bsd_steering_bandutil_t" style="color:white"></span>%';
		code +='</td></tr></table></div></span><span class="steering_unuse_off_'+i+'">- -</span></td>';
	}
	code +='</tr>';

	code +='<tr><th>RSSI</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><span class="steering_on_'+i+'"><div><table><tr><td style="border:0px; padding-left:0px">';
		code +='<select class="input_option" name="wl'+i+'_bsd_steering_rssi_s">';
		code +='<option selected="" value="0" class="content_input_fd">Less</option>';
		code +='<option value="1" class="content_input_fd">Greater</option>';
		code +='</select></td>';
		code +='<td style="border:0px; padding-left:0px">';
		code +='<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl'+i+'_bsd_steering_rssi" name="wl'+i+'_bsd_steering_rssi" maxlength="4">';
		code +='<label style="margin-left:5px;">dBm</label></td></tr></table></div></span><span class="steering_unuse_off_'+i+'">- -</span></td>';
	}
	code +='</tr>';
		code +='<tr><th>PHY Rate Less</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><span class="steering_on_'+i+'"><div><table><tr><td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_steering_phy_l" style="width:80px;"></div></td>';
		code +='<td style="border:0px">';
		code +='<div id="wl'+i+'_bsd_steering_phy_l0_t">< <span style="color:white;" name="wl'+i+'_bsd_steering_phy_l_t" id="wl'+i+'_bsd_steering_phy_l_t">300</span> Mbps</div>';
		code +='<div id="wl'+i+'_bsd_steering_phy_ld_t" style="display:none;"><#btn_disable#></div>';
		code +='</td></tr></table></div></span><span class="steering_unuse_off_'+i+'">- -</span></td>';
	}
	code +='</tr>';

	code +='<tr><th>PHY Rate Greater</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><span class="steering_on_'+i+'"><div><table><tr>';
		code +='<td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_steering_phy_g" style="width:80px;"></div>';
		code +='</td>';
		code +='<td style="border:0px">';
		code +='<div id="wl'+i+'_bsd_steering_phy_g0_t">> <span style="color:white;" name="wl'+i+'_bsd_steering_phy_g_t" id="wl'+i+'_bsd_steering_phy_g_t">300</span> Mbps</div>';
		code +='<div id="wl'+i+'_bsd_steering_phy_gd_t" style="display:none;"><#btn_disable#></div>';
		code +='</td></tr></table></div></span><span class="steering_unuse_off_'+i+'">- -</span></td>';
	}
	code +='</tr>';

	code +='<tr><th>VHT</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td style="padding-left:12px"><span class="steering_on_'+i+'">';
		code +='<select class="input_option" name="wl'+i+'_bsd_steering_vht_s">';
		code +='<option selected="" value="0" class="content_input_fd"><#All#></option>';
		code +='<option value="1" class="content_input_fd">AC only</option>';
		code +='<option value="2" class="content_input_fd">not-allowed</option>';
		code +='</select></span><span class="steering_unuse_off_'+i+'">- -</span></td>';
	}
	code +='</tr>';
	code +='</table>';

	document.getElementById("bsd_steering_div").innerHTML = code;
}

function gen_bsd_sta_select_div(){
	var code = "";

	code +='<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">';
	code +='<thead><tr><td colspan="4"><#smart_connect_STA#></td></tr></thead>';
	
	code +='<tr><th width="20%">RSSI</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		if('<% nvram_get("smart_connect_x"); %>' == '2')
			code +='<td width="40%"><div><table><tr>';
		else
			code +='<td width="27%"><div><table><tr>';
		code +='<td style="border:0px; padding-left:0px;">';
		code +='<select class="input_option" name="wl'+i+'_bsd_sta_select_policy_rssi_s">';
		code +='<option selected="" value="0" class="content_input_fd">Less</option>';
		code +='<option value="1" class="content_input_fd">Greater</option>';
		code +='</select></td>';
		code +='<td style="border:0px; padding-left:0px">';
		code +='<input type="text" onkeypress="return validator.isNegativeNumber(this,event)" value="100" class="input_3_table" id="wl'+i+'_bsd_sta_select_policy_rssi" name="wl'+i+'_bsd_sta_select_policy_rssi" maxlength="4">';
		code +='<label style="margin-left:5px;">dBm</label>';
		code +='</td></tr></table></div></td>';
	}
	code +='</tr>';

	code +='<tr><th>PHY Rate Less</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><div><table><tr>';
		code +='<td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_sta_select_policy_phy_l" style="width:80px;"></div>';
		code +='</td><td style="border:0px;">';
		code +='<div id="wl'+i+'_bsd_sta_select_policy_phy_l0_t">< <span style="color:white;" name="wl'+i+'_bsd_sta_select_policy_phy_l_t" id="wl'+i+'_bsd_sta_select_policy_phy_l_t">300</span> Mbps</div>';
		code +='<div id="wl'+i+'_bsd_sta_select_policy_phy_ld_t" style="display:none;"><#btn_disable#></div>';
		code +='</td></tr></table></div></td>';
	}
	code +='</tr>';

	code +='<tr><th>PHY Rate Greater</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><div><table><tr>';
		code +='<td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_sta_select_policy_phy_g" style="width:80px;"></div>';
		code +='</td><td style="border:0px;">';
		code +='<div id="wl'+i+'_bsd_sta_select_policy_phy_g0_t">> <span style="color:white;" name="wl'+i+'_bsd_sta_select_policy_phy_g_t" id="wl'+i+'_bsd_sta_select_policy_phy_g_t">300</span> Mbps</div>';
		code +='<div id="wl'+i+'_bsd_sta_select_policy_phy_gd_t" style="display:none;"><#btn_disable#></div>';
		code +='</td></tr></table></div></td>';
	}
	code +='</tr>';

	code +='<tr><th>VHT</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td style="padding-left:12px">';
		code +='<select class="input_option" name="wl'+i+'_bsd_sta_select_policy_vht_s">';
		code +='<option selected="" value="0" class="content_input_fd"><#All#></option>';
		code +='<option value="1" class="content_input_fd">AC only</option>';
		code +='<option value="2" class="content_input_fd">not-allowed</option>';
		code +='</select></td>';
	}
	code +='</tr></table>';

	document.getElementById("bsd_sta_select_div").innerHTML = code;
}

function gen_bsd_if_select_div(){
	var code="";

	code +='<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">';
	code +='<thead><tr><td colspan="4"><#smart_connect_ISQP#></td></tr></thead>';

	code +='<tr><th width="20%"><#Interface_target#></th>';
     if('<% nvram_get("smart_connect_x"); %>' != 2){
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td width="27%" style="padding:0px 0px 0px 0px;"><div><table><tr>';
		code +='<td style="border:0px; padding:0px 0px 0px 3px;">1:</td>';
		code +='<td style="border:0px; padding:0px 0px 0px 1px;">';
		code +='<select class="input_option" name="wl'+i+'_bsd_if_select_policy_first" onChange="change_bsd_if_select(this);">';
		code +='<option selected="" value="0" class="content_input_fd">';
		if(i == 0){
			code +='5GHz-1';
		}else if(i == 1){
			code +='5GHz-2';
		}else{
			code +='5GHz-1';
		}
		code +='</option><option value="1" class="content_input_fd">';
		if(i == 0){
			code +='5GHz-2';
		}else if(i == 1){
			code +='2.4GHz';
		}else{
			code +='2.4GHz';
		}		
		code +='</option>';
		code +='</select></td>';
		code +='<td style="border:0px; padding:0px 0px 0px 7px;">2:</td>';
		code +='<td style="border:0px; padding:0px 0px 0px 1px;">';
		code +='<select class="input_option" name="wl'+i+'_bsd_if_select_policy_second" onChange="change_bsd_if_select(this);">';
		code +='<option selected="" value="0" class="content_input_fd">';
		if(i == 0){
			code +='5GHz-1';
		}else if(i == 1){
			code +='2.4GHz';
		}else{
			code +='5GHz-1';
		}	
		code +='</option>';
		code +='<option value="1" class="content_input_fd">';
		if(i == 0){
			code +='5GHz-2';
		}else if(i == 1){
			code +='2.4GHz';
		}else{
			code +='2.4GHz';
		}		
		code +='</option>';
		code +='<option value="2" class="content_input_fd">';
		code +='none';		
		code +='</option>';
		code +='</select></td></tr></table></div></td>';
	}
     }else{
		code +='<td width="40%">5GHz-2</td><td width="40%">5GHz-1</td>'
	}
	code +='</tr>';

	code +='<tr><th><#smart_connect_Bandwidth#></th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td><div><table><tr>';
		code +='<td style="border:0px;width:35px; padding-left:0px">';
		code +='<div id="slider_wl'+i+'_bsd_if_qualify_policy" style="width:80px;"></div>';
		code +='</td><td style="border:0px;">';
		code +='<span id="wl'+i+'_bsd_if_qualify_policy_t" name="wl'+i+'_bsd_if_qualify_policy_t" style="color:white"></span>%';
		code +='</td></tr></table></div></td>';
	}
	code +='</tr>';

	code +='<tr><th>VHT</th>';
	for(i = start_band_idx; i < wl_info.wl_if_total; i++){
		code +='<td style="padding-left:12px">';
		code +='<select onchange="check_vht(this,'+i+');" class="input_option" name="wl'+i+'_bsd_if_qualify_policy_vht_s">';
		code +='<option selected="" value="0" class="content_input_fd"><#All#></option>';
		code +='<option value="1" class="content_input_fd">AC only</option>';
		code +='</select></td>';
	}	
	code +='</tr></table>';

	document.getElementById("bsd_if_select_div").innerHTML = code;
}

function check_vht(obj,idx){
	var vht_desc = new Array();
	var vht_value = new Array();
	var obj_name = 'document.form.wl'+idx+'_bsd_steering_vht_s';
	var obj_name1 = 'document.form.wl'+idx+'_bsd_sta_select_policy_vht_s';
	var obj_value;
	var obj_value1;

	if(bsd_steering_policy_bin[idx][2] == 0 && bsd_steering_policy_bin[idx][3] == 0)	//all
		obj_value = 0;
	else if(bsd_steering_policy_bin[idx][2] == 0 && bsd_steering_policy_bin[idx][3] == 1)	//ac only
		obj_value = 1;
	else if(bsd_steering_policy_bin[idx][2] == 1 && bsd_steering_policy_bin[idx][3] == 0)	//legacy
		obj_value = 2;	

	if(bsd_sta_select_policy_bin[idx][2] == 0 && bsd_sta_select_policy_bin[idx][3] == 0)	//all
		obj_value1 = 0;
	else if(bsd_sta_select_policy_bin[idx][2] == 0 && bsd_sta_select_policy_bin[idx][3] == 1)	//ac only
		obj_value1 = 1;
	else if(bsd_sta_select_policy_bin[idx][2] == 1 && bsd_sta_select_policy_bin[idx][3] == 0)	//legacy
		obj_value1 = 2;	

	if(obj.value == 1){
		vht_desc = ["AC only"];
		vht_value = [1];
		add_options_x2(eval(obj_name), vht_desc, vht_value, 1);
		add_options_x2(eval(obj_name1), vht_desc, vht_value, 1);
	}else if(obj.value == 0){
		vht_desc = ["<#All#>","AC only","not-allowed"];
		vht_value = [0,1,2];
		add_options_x2(eval(obj_name), vht_desc, vht_value, obj_value);
		add_options_x2(eval(obj_name1), vht_desc, vht_value, obj_value1);
	}
}

function handle_bsd_nvram(){

	var bsd_if_select_policy_idx = [];

  for(i = start_band_idx; i < wl_info.wl_if_total; i++){
	/* [bsd_steering_policy] - bsd_steering_policy setting */
	set_bandutil_qualify_power(bsd_steering_policy[i][0],'wl'+i+'_bsd_steering_bandutil');
	if(bsd_steering_policy_bin[i][6] == 0){
		document.form['wl'+i+'_bsd_steering_balance'][1].checked = true;
		change_lb(0,i);
	}else{
		document.form['wl'+i+'_bsd_steering_balance'][0].checked = true;
		change_lb(1,i);
	}
	document.getElementById('wl'+i+'_bsd_steering_rssi').value = bsd_steering_policy[i][3];
	document.form['wl'+i+'_bsd_steering_rssi_s'].value  = bsd_steering_policy_bin[i][1];
	set_lg_power(bsd_steering_policy[i][4],'wl'+i+'_bsd_steering_phy_l',i);
	document.getElementById('wl'+i+'_bsd_steering_phy_l').value = bsd_steering_policy[i][4];	
	set_lg_power(bsd_steering_policy[i][5],'wl'+i+'_bsd_steering_phy_g',i);
	document.getElementById('wl'+i+'_bsd_steering_phy_g').value = bsd_steering_policy[i][5];
	if(bsd_steering_policy_bin[i][2] == 0 && bsd_steering_policy_bin[i][3] == 0)	//all
		document.form['wl'+i+'_bsd_steering_vht_s'].value = 0;
	else if(bsd_steering_policy_bin[i][2] == 0 && bsd_steering_policy_bin[i][3] == 1)	//ac only
		document.form['wl'+i+'_bsd_steering_vht_s'].value = 1;
	else if(bsd_steering_policy_bin[i][2] == 1 && bsd_steering_policy_bin[i][3] == 0)	//legacy
		document.form['wl'+i+'_bsd_steering_vht_s'].value = 2;	

	/* [bsd_sta_select_policy] - bsd_sta_select_policy setting */
	document.getElementById('wl'+i+'_bsd_sta_select_policy_rssi').value = bsd_sta_select_policy[i][1];
	document.form['wl'+i+'_bsd_sta_select_policy_rssi_s'].value  = bsd_sta_select_policy_bin[i][1];

	set_lg_power(bsd_sta_select_policy[i][2],'wl'+i+'_bsd_sta_select_policy_phy_l',i);
	document.getElementById('wl'+i+'_bsd_sta_select_policy_phy_l').value = bsd_sta_select_policy[i][2];
	set_lg_power(bsd_sta_select_policy[i][3],'wl'+i+'_bsd_sta_select_policy_phy_g',i);
	document.getElementById('wl'+i+'_bsd_sta_select_policy_phy_g').value = bsd_sta_select_policy[i][3];

	if(bsd_sta_select_policy_bin[i][2] == 0 && bsd_sta_select_policy_bin[i][3] == 0)	//all
		document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value = 0;
	else if(bsd_sta_select_policy_bin[i][2] == 0 && bsd_sta_select_policy_bin[i][3] == 1)	//ac only
		document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value = 1;
	else if(bsd_sta_select_policy_bin[i][2] == 1 && bsd_sta_select_policy_bin[i][3] == 0)	//legacy
		document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value = 2;	

	if('<% nvram_get("smart_connect_x"); %>' != 2){
		/* [Interface Select and Qualify Procedures] - bsd_if_qualify_policy setting*/
		var bsd_if_select_policy_1st = wl_name[wl_ifnames.indexOf(bsd_if_select_policy[i][0])]; 
		var bsd_if_select_policy_2nd = wl_name[wl_ifnames.indexOf(bsd_if_select_policy[i][1])]; 
		bsd_if_select_policy_idx[i] = [bsd_if_select_policy_1st,bsd_if_select_policy_2nd];
		document.form['wl'+i+'_bsd_if_select_policy_first'].value = wl_names[i].indexOf(bsd_if_select_policy_idx[i][0]);
		document.form['wl'+i+'_bsd_if_select_policy_second'].value = (wl_names[i].indexOf(bsd_if_select_policy_idx[i][1]) == -1 ? 2:wl_names[i].indexOf(bsd_if_select_policy_idx[i][1]));
		change_bsd_if_select(eval(document.form['wl'+i+'_bsd_if_select_policy_first']));
	}
	set_bandutil_qualify_power(bsd_if_qualify_policy[i][0],'wl'+i+'_bsd_if_qualify_policy');
	document.getElementById('wl'+i+'_bsd_if_qualify_policy').value = bsd_if_qualify_policy[i][0];	

	if(bsd_if_qualify_policy_bin[i][1] == 0 && bsd_if_qualify_policy_bin[i][2] == 0){	//all
		document.form['wl'+i+'_bsd_if_qualify_policy_vht_s'].value = 0;
		check_vht(eval(document.form['wl'+i+'_bsd_if_qualify_policy_vht_s']),i);
	}else if(bsd_if_qualify_policy_bin[i][1] == 0 && bsd_if_qualify_policy_bin[i][2] == 1){	//ac only
		document.form['wl'+i+'_bsd_if_qualify_policy_vht_s'].value = 1;
		check_vht(eval(document.form['wl'+i+'_bsd_if_qualify_policy_vht_s']),i);
	}
  }		
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
	var bsd_steering_policy_bin_t = [];
	var bsd_sta_select_policy_bin_t = [];
	var bsd_if_qualify_policy_bin_t = [];
  for(i = start_band_idx; i < wl_info.wl_if_total; i++){
	/* [bsd_steering_policy] - [bw util%, x, x, RSSI threshold, phy rate, flag] */
	bsd_steering_policy_bin_t[i] = bsd_steering_policy_bin[i].split("");
	bsd_steering_policy[i][0] = document.form['wl'+i+'_bsd_steering_bandutil'].value;
	bsd_steering_policy[i][3] = document.form['wl'+i+'_bsd_steering_rssi'].value;
	bsd_steering_policy[i][4] = document.form['wl'+i+'_bsd_steering_phy_l'].value;
	bsd_steering_policy[i][5] = document.form['wl'+i+'_bsd_steering_phy_g'].value;
	bsd_steering_policy_bin_t[i][1] = document.form['wl'+i+'_bsd_steering_rssi_s'].value; 
	bsd_steering_policy_bin_t[i][6] = document.form['wl'+i+'_bsd_steering_balance'].value;
	if(document.form['wl'+i+'_bsd_steering_vht_s'].value == 0){
		bsd_steering_policy_bin_t[i][2] = 0;
		bsd_steering_policy_bin_t[i][3] = 0;
	}else if(document.form['wl'+i+'_bsd_steering_vht_s'].value == 1){
		bsd_steering_policy_bin_t[i][2] = 0;
		bsd_steering_policy_bin_t[i][3] = 1;
	}else if(document.form['wl'+i+'_bsd_steering_vht_s'].value == 2){
		bsd_steering_policy_bin_t[i][2] = 1;
		bsd_steering_policy_bin_t[i][3] = 0;
	}

	bsd_steering_policy[i][6] = '0x' + (parseInt(reverse_bin(bsd_steering_policy_bin_t[i].join("")),2)).toString(16);
	if('<% nvram_get("smart_connect_x"); %>' != '2')
		document.form['wl'+i+'_bsd_steering_policy'].value = bsd_steering_policy[i].toString().replace(/,/g,' ');
	else
		document.form['wl'+i+'_bsd_steering_policy_x'].value = bsd_steering_policy[i].toString().replace(/,/g,' ');

	/* [bsd_sta_select_policy] - [x, RSSI, phy rate, x, x, x, x, x, x, flag] */
	bsd_sta_select_policy_bin_t[i] = bsd_sta_select_policy_bin[i].split("");
	bsd_sta_select_policy[i][1] = document.form['wl'+i+'_bsd_sta_select_policy_rssi'].value;
	bsd_sta_select_policy[i][2] = document.form['wl'+i+'_bsd_sta_select_policy_phy_l'].value;
	bsd_sta_select_policy[i][3] = document.form['wl'+i+'_bsd_sta_select_policy_phy_g'].value;
	bsd_sta_select_policy_bin_t[i][1] = document.form['wl'+i+'_bsd_sta_select_policy_rssi_s'].value;
	bsd_sta_select_policy_bin_t[i][6] = document.form['wl'+i+'_bsd_steering_balance'].value;

	if(document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value == 0){
		bsd_sta_select_policy_bin_t[i][2] = 0;
		bsd_sta_select_policy_bin_t[i][3] = 0;
	}else if(document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value == 1){
		bsd_sta_select_policy_bin_t[i][2] = 0;
		bsd_sta_select_policy_bin_t[i][3] = 1;
	}else if(document.form['wl'+i+'_bsd_sta_select_policy_vht_s'].value == 2){
		bsd_sta_select_policy_bin_t[i][2] = 1;
		bsd_sta_select_policy_bin_t[i][3] = 0;
	}

	bsd_sta_select_policy[i][10] = '0x' + (parseInt(reverse_bin(bsd_sta_select_policy_bin_t[i].join("")),2)).toString(16);
	if('<% nvram_get("smart_connect_x"); %>' != '2')
		document.form['wl'+i+'_bsd_sta_select_policy'].value = bsd_sta_select_policy[i].toString().replace(/,/g,' ');	
	else
		document.form['wl'+i+'_bsd_sta_select_policy_x'].value = bsd_sta_select_policy[i].toString().replace(/,/g,' ');			

	/* [Interface Select and Qualify Procedures] */
	bsd_if_qualify_policy[i][0] = document.form['wl'+i+'_bsd_if_qualify_policy'].value;
	bsd_if_qualify_policy_bin_t[i] = bsd_if_qualify_policy_bin[i].split("");
	if(document.form['wl'+i+'_bsd_if_qualify_policy_vht_s'].value == 0){
		bsd_if_qualify_policy_bin_t[i][1] = 0;
		bsd_if_qualify_policy_bin_t[i][2] = 0;
	}else if(document.form['wl'+i+'_bsd_if_qualify_policy_vht_s'].value == 1){
		bsd_if_qualify_policy_bin_t[i][1] = 0;
		bsd_if_qualify_policy_bin_t[i][2] = 1;
  	}	
  	bsd_if_qualify_policy[i][1] = '0x' + (parseInt(reverse_bin(bsd_if_qualify_policy_bin_t[i].join("")),2)).toString(16);
  	if('<% nvram_get("smart_connect_x"); %>' != '2')
		document.form['wl'+i+'_bsd_if_qualify_policy'].value = bsd_if_qualify_policy[i].toString().replace(/,/g,' ');
	else
		document.form['wl'+i+'_bsd_if_qualify_policy_x'].value = bsd_if_qualify_policy[i].toString().replace(/,/g,' ');

	bsd_if_select_policy[i][0] = wl_ifnames[wl_name.indexOf(wl_names[i][document.form['wl'+i+'_bsd_if_select_policy_first'].value])];
	bsd_if_select_policy[i][1] = wl_ifnames[wl_name.indexOf(wl_names[i][document.form['wl'+i+'_bsd_if_select_policy_second'].value])];
	if('<% nvram_get("smart_connect_x"); %>' != '2')
		document.form['wl'+i+'_bsd_if_select_policy'].value = bsd_if_select_policy[i].toString().replace(/,/g,' ');
	else
		document.form['wl'+i+'_bsd_if_select_policy_x'].value = bsd_if_select_policy[i].toString().replace(/,/g,' ');

  }

	bsd_bounce_detect[0] = document.form.windows_time_sec.value;
	bsd_bounce_detect[1] = document.form.bsd_counts.value;
	bsd_bounce_detect[2] = document.form.dwell_time_sec.value;
	if('<% nvram_get("smart_connect_x"); %>' != '2')
		document.form.bsd_bounce_detect.value = bsd_bounce_detect.toString().replace(/,/g,' ');
	else
		document.form.bsd_bounce_detect_x.value = bsd_bounce_detect.toString().replace(/,/g,' ');

	document.form.submit();

}

function restoreRule(){
	if('<% nvram_get("smart_connect_x"); %>' == '2'){
		document.form.wl1_bsd_steering_policy_x.value = '<% nvram_default_get("wl1_bsd_steering_policy_x"); %>';
		document.form.wl2_bsd_steering_policy_x.value = '<% nvram_default_get("wl2_bsd_steering_policy_x"); %>';
		document.form.wl1_bsd_sta_select_policy_x.value = '<% nvram_default_get("wl1_bsd_sta_select_policy_x"); %>';
		document.form.wl2_bsd_sta_select_policy_x.value = '<% nvram_default_get("wl2_bsd_sta_select_policy_x"); %>';
		document.form.wl1_bsd_if_select_policy_x.value = '<% nvram_default_get("wl1_bsd_if_select_policy_x"); %>';
		document.form.wl2_bsd_if_select_policy_x.value = '<% nvram_default_get("wl2_bsd_if_select_policy_x"); %>';
		document.form.wl1_bsd_if_qualify_policy_x.value = '<% nvram_default_get("wl1_bsd_if_qualify_policy_x"); %>';
		document.form.wl2_bsd_if_qualify_policy_x.value = '<% nvram_default_get("wl2_bsd_if_qualify_policy_x"); %>';
		document.form.bsd_bounce_detect_x.value = '<% nvram_default_get("bsd_bounce_detect_x"); %>';
		document.form.bsd_ifnames_x.value = '<% nvram_default_get("bsd_ifnames"); %>';
	}else{
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
		document.form.bsd_ifnames.value = '<% nvram_default_get("bsd_ifnames"); %>';
	}
	document.form.submit();
}

function register_event(){
	$(function() {
		$( "#slider_wl0_bsd_steering_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl0_bsd_steering_bandutil_t').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl0_bsd_steering_bandutil',0);	  
			}
		}); 		
		$( "#slider_wl1_bsd_steering_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl1_bsd_steering_bandutil_t').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl1_bsd_steering_bandutil',1);  
			}
		}); 
		$( "#slider_wl2_bsd_steering_bandutil" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_bandutil').value = ui.value; 
				document.getElementById('wl2_bsd_steering_bandutil_t').innerHTML = ui.value;
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl2_bsd_steering_bandutil',0);  
			}
		}); 	
		$( "#slider_wl0_bsd_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl0_bsd_steering_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl0_bsd_steering_phy_l',0);	  
			}
		}); 	
		$( "#slider_wl0_bsd_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl0_bsd_steering_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl0_bsd_steering_phy_g',0);	  
			}
		}); 			
		$( "#slider_wl1_bsd_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl1_bsd_steering_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl1_bsd_steering_phy_l',1);	  
			}
		}); 	
		$( "#slider_wl1_bsd_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl1_bsd_steering_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl1_bsd_steering_phy_g',1);	  
			}
		}); 		
		$( "#slider_wl2_bsd_steering_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_phy_l').value = ui.value; 
				document.getElementById('wl2_bsd_steering_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl2_bsd_steering_phy_l',2);	  
			}
		}); 	
		$( "#slider_wl2_bsd_steering_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_steering_phy_g').value = ui.value; 
				document.getElementById('wl2_bsd_steering_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl2_bsd_steering_phy_g',2);	  
			}
		}); 			
		$( "#slider_wl0_bsd_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl0_bsd_sta_select_policy_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl0_bsd_sta_select_policy_phy_l',0);	  
			}
		}); 
		$( "#slider_wl0_bsd_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 600,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl0_bsd_sta_select_policy_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl0_bsd_sta_select_policy_phy_g',0);	  
			}
		}); 		
		$( "#slider_wl1_bsd_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl1_bsd_sta_select_policy_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl1_bsd_sta_select_policy_phy_l',1);	  
			}
		}); 		
		$( "#slider_wl1_bsd_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl1_bsd_sta_select_policy_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl1_bsd_sta_select_policy_phy_g',1);	  
			}
		}); 				
		$( "#slider_wl2_bsd_sta_select_policy_phy_l" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_sta_select_policy_phy_l').value = ui.value; 
				document.getElementById('wl2_bsd_sta_select_policy_phy_l_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl2_bsd_sta_select_policy_phy_l',2);	  
			}
		}); 	
		$( "#slider_wl2_bsd_sta_select_policy_phy_g" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 1300,
			value:1,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_sta_select_policy_phy_g').value = ui.value; 
				document.getElementById('wl2_bsd_sta_select_policy_phy_g_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_lg_power(ui.value,'wl2_bsd_sta_select_policy_phy_g',2); 
			}
		}); 		
		$( "#slider_wl0_bsd_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl0_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl0_bsd_if_qualify_policy_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl0_bsd_if_qualify_policy');	  
			}
		}); 	
		$( "#slider_wl1_bsd_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl1_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl1_bsd_if_qualify_policy_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl1_bsd_if_qualify_policy');	  
			}
		}); 
		$( "#slider_wl2_bsd_if_qualify_policy" ).slider({
			orientation: "horizontal",
			range: "min",
			min:0,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl2_bsd_if_qualify_policy').value = ui.value; 
				document.getElementById('wl2_bsd_if_qualify_policy_t').innerHTML = ui.value; 
			},
			stop:function(event, ui){
				set_bandutil_qualify_power(ui.value,'wl2_bsd_if_qualify_policy');	  
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

function set_bandutil_qualify_power(power_value,flag){
		document.getElementById('slider_'+flag).children[0].style.width = power_value + "%";
		document.getElementById('slider_'+flag).children[1].style.left = power_value + "%";
		document.form[flag].value = power_value;
		document.getElementById(flag+'_t').innerHTML = power_value;
		check_power(power_value,'per');
}

function set_lg_power(power_value,flag,idx){
	var divd;
	if(idx == 0)
		var divd = 6;
	else
		var divd = 13;
	document.getElementById('slider_'+flag).children[0].style.width = power_value/divd + "%";
	document.getElementById('slider_'+flag).children[1].style.left = power_value/divd + "%";
	document.form[flag].value = power_value;
	if(document.form[flag].value == 0){
		document.getElementById(flag+'d_t').style.display = ""; 
		document.getElementById(flag+'0_t').style.display = "none";
	}else{
		document.getElementById(flag+'d_t').style.display = "none";
		document.getElementById(flag+'0_t').style.display = "";
		document.getElementById(flag+'_t').innerHTML = power_value;
	}
	check_power(power_value,'phyrate');	
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
<input type="hidden" name="bsd_ifnames" value="eth1 eth2 eth3" disabled>
<input type="hidden" name="wl0_bsd_steering_policy" value="" disabled>
<input type="hidden" name="wl1_bsd_steering_policy" value="" disabled>
<input type="hidden" name="wl2_bsd_steering_policy" value="" disabled>
<input type="hidden" name="wl0_bsd_steering_bandutil" id="wl0_bsd_steering_bandutil" value="">
<input type="hidden" name="wl1_bsd_steering_bandutil" id="wl1_bsd_steering_bandutil" value="">
<input type="hidden" name="wl2_bsd_steering_bandutil" id="wl2_bsd_steering_bandutil" value="">
<input type="hidden" name="wl0_bsd_steering_phy_l" id="wl0_bsd_steering_phy_l" value="">
<input type="hidden" name="wl1_bsd_steering_phy_l" id="wl1_bsd_steering_phy_l" value="">
<input type="hidden" name="wl2_bsd_steering_phy_l" id="wl2_bsd_steering_phy_l" value="">
<input type="hidden" name="wl0_bsd_steering_phy_g" id="wl0_bsd_steering_phy_g" value="">
<input type="hidden" name="wl1_bsd_steering_phy_g" id="wl1_bsd_steering_phy_g" value="">
<input type="hidden" name="wl2_bsd_steering_phy_g" id="wl2_bsd_steering_phy_g" value="">
<input type="hidden" name="wl0_bsd_sta_select_policy" value="" disabled>
<input type="hidden" name="wl1_bsd_sta_select_policy" value="" disabled>
<input type="hidden" name="wl2_bsd_sta_select_policy" value="" disabled>
<input type="hidden" name="wl0_bsd_if_select_policy" value="" disabled>
<input type="hidden" name="wl1_bsd_if_select_policy" value="" disabled>
<input type="hidden" name="wl2_bsd_if_select_policy" value="" disabled>
<input type="hidden" name="wl0_bsd_sta_select_policy_phy_l" id="wl0_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl1_bsd_sta_select_policy_phy_l" id="wl1_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl2_bsd_sta_select_policy_phy_l" id="wl2_bsd_sta_select_policy_phy_l" value="">
<input type="hidden" name="wl0_bsd_sta_select_policy_phy_g" id="wl0_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl1_bsd_sta_select_policy_phy_g" id="wl1_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl2_bsd_sta_select_policy_phy_g" id="wl2_bsd_sta_select_policy_phy_g" value="">
<input type="hidden" name="wl0_bsd_if_qualify_policy" id="wl0_bsd_if_qualify_policy" value="" disabled>
<input type="hidden" name="wl1_bsd_if_qualify_policy" id="wl1_bsd_if_qualify_policy" value="" disabled>
<input type="hidden" name="wl2_bsd_if_qualify_policy" id="wl2_bsd_if_qualify_policy" value="" disabled>
<input type="hidden" name="bsd_bounce_detect" value=""  disabled>
<input type="hidden" name="bsd_ifnames_x" value="eth2 eth3" disabled>
<input type="hidden" name="wl1_bsd_steering_policy_x" value="" disabled>
<input type="hidden" name="wl2_bsd_steering_policy_x" value="" disabled>
<input type="hidden" name="wl1_bsd_sta_select_policy_x" value="" disabled>
<input type="hidden" name="wl2_bsd_sta_select_policy_x" value="" disabled>
<input type="hidden" name="wl1_bsd_if_select_policy_x" value="" disabled>
<input type="hidden" name="wl2_bsd_if_select_policy_x" value="" disabled>
<input type="hidden" name="wl1_bsd_if_qualify_policy_x" id="wl1_bsd_if_qualify_policy_x" value="" disabled>
<input type="hidden" name="wl2_bsd_if_qualify_policy_x" id="wl2_bsd_if_qualify_policy_x" value="" disabled>
<input type="hidden" name="bsd_bounce_detect_x" value=""  disabled>
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
		  <div class="formfonttitle"><#menu5_1#> - <#smart_connect_rule#></div>
     	  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      	  <div class="formfontdesc"><#smart_connect_hint#></div>
		  <div style="text-align:right;margin-top:-36px;padding-bottom:3px;"><input type="button" class="button_gen" value="View List" onClick="pop_clientlist_listview(true)"></div>
		  <div id="bsd_steering_div"></div>

		  <div id="bsd_sta_select_div"></div>

		  <div id="bsd_if_select_div"></div>

		  	<table cellspacing="0" cellpadding="4" bordercolor="#6b8fa3" border="1" align="center" width="100%" class="FormTable" id="MainTable2" style="margin-top:10px">
			<thead>
				<tr>
					<td colspan="2"><#smart_connect_BD#></td>
				</tr>
			</thead>	
			<tr>
				<th width="20%"><#smart_connect_STA_window#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="windows_time_sec" maxlength="4" autocorrect="off" autocapitalize="off"> <#Second#>
					</td>								
		  	</tr>	
			<tr>
				<th><#smart_connect_STA_counts#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="bsd_counts" maxlength="4" autocorrect="off" autocapitalize="off">
					</td>									
		  	</tr>
			<tr>
				<th><#smart_connect_STA_dwell#></th>
					<td>
						<input type="text" onkeypress="return validator.isNumber(this,event)" value="100" class="input_6_table" name="dwell_time_sec" maxlength="4" autocorrect="off" autocapitalize="off"> <#Second#>
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
