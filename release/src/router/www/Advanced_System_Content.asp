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
<title><#Web_Title#> - <#menu5_6_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="pwdmeter.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	margin-top:106px;
	*margin-top:96px;	
	margin-left:127px;
	width:345px;
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
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
time_day = uptimeStr.substring(5,7);//Mon, 01 Aug 2011 16:25:44 +0800(1467 secs since boot....
time_mon = uptimeStr.substring(9,12);
time_time = uptimeStr.substring(18,20);
dstoffset = '<% nvram_get("time_zone_dstoff"); %>';
<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var http_clientlist_array = '<% nvram_get("http_clientlist"); %>';
var accounts = [<% get_all_accounts(); %>];

function initial(){
	show_menu();
	show_http_clientlist();
	corrected_timezone();
	load_timezones();
	show_dst_chk();
	load_dst_m_Options();
	load_dst_w_Options();
	load_dst_d_Options();
	load_dst_h_Options();
	document.form.http_passwd2.value = "";
	chkPass("<% nvram_get("http_passwd"); %>", 'http_passwd');
	if(HTTPS_support == -1){
		$("https_tr").style.display = "none";
		$("https_lanport").style.display = "none";
		$("http_client_tr").style.display = "none";
		$("http_client_table").style.display = "none";
		$("http_clientlist_Block").style.display = "none";
	}
	else{
		showLANIPList();
		hide_https_lanport(document.form.http_enable.value);
		hide_https_wanport(document.form.http_enable.value);
	}	

	if(WebDav_support != -1){
		document.getElementById('http_username_span').style.display = "none";
		document.form.http_username.disabled = false;
	}else{
		document.getElementById('http_username_span').style.display = "";
		document.form.http_username.disabled = true;
		document.getElementById('http_username').style.display = "none";
	}
	
	if(wifi_hw_sw_support != -1){
			document.form.btn_ez_radiotoggle[0].disabled = true;
			document.form.btn_ez_radiotoggle[1].disabled = true;
			document.getElementById('btn_ez_radiotoggle_tr').style.display = "none";
	}else{
			document.getElementById('btn_ez_radiotoggle_tr').style.display = "";
			//document.form.btn_ez_radiotoggle[0].disabled = true;
			//document.form.btn_ez_radiotoggle[1].disabled = true;
			//document.getElementById('btn_ez_radiotoggle_tr').style.display = "none";			
	}
	
	if(sw_mode == 2){  // hide WPS button behavior under repeater mode
		$('btn_ez_radiotoggle_tr').style.display = "none";	
	}
	
	if(sw_mode != 1){
		$('misc_http_x_tr').style.display ="none";
		hideport(0);
		document.form.misc_http_x.disabled = true;
		document.form.misc_httpsport_x.disabled = true;
		document.form.misc_httpport_x.disabled = true;
	}
	else
		hideport(document.form.misc_http_x[0].checked);
	
	if(HTTPS_support == -1 || '<% nvram_get("http_enable"); %>' == 0)
		$("https_port").style.display = "none";
	else if('<% nvram_get("http_enable"); %>' == 1)
		$("http_port").style.display = "none";
}

var time_zone_tmp="";
var time_zone_s_tmp="";
var time_zone_e_tmp="";
var time_zone_withdst="";

function applyRule(){
	if(validForm()){
		var rule_num = $('http_clientlist_table').rows.length;
		var item_num = $('http_clientlist_table').rows[0].cells.length;
		var tmp_value = "";
	
		for(i=0; i<rule_num; i++){
			tmp_value += "<"		
			for(j=0; j<item_num-1; j++){	
				tmp_value += $('http_clientlist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)	
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";	
		document.form.http_clientlist.value = tmp_value;

		if(document.form.http_clientlist.value == "" && document.form.http_client[0].checked == 1){
			alert("<#JS_fieldblank#>");
			document.form.http_client_ip_x_0.focus();
			return false;
		}

		if(document.form.http_passwd2.value.length > 0)
			document.form.http_passwd.value = document.form.http_passwd2.value;

		if(document.form.time_zone_dst_chk.checked){	// Exist dstoffset
				time_zone_tmp = document.form.time_zone_select.value.split("_");	//0:time_zone 1:serial number
				time_zone_s_tmp = "M"+document.form.dst_start_m.value+"."+document.form.dst_start_w.value+"."+document.form.dst_start_d.value+"/"+document.form.dst_start_h.value;
				time_zone_e_tmp = "M"+document.form.dst_end_m.value+"."+document.form.dst_end_w.value+"."+document.form.dst_end_d.value+"/"+document.form.dst_end_h.value;
				document.form.time_zone_dstoff.value=time_zone_s_tmp+","+time_zone_e_tmp;
				document.form.time_zone.value = document.form.time_zone_select.value;
		}else{
				document.form.time_zone_dstoff.value="";
				document.form.time_zone.value = document.form.time_zone_select.value;
		}
		
		if(document.form.misc_http_x[1].checked == true){
				document.form.misc_httpport_x.disabled = true;
				document.form.misc_httpsport_x.disabled = true;
		}		
		if(document.form.misc_http_x[0].checked == true 
				&& document.form.http_enable[0].selected == true){
				document.form.misc_httpsport_x.disabled = true;
		}	
		if(document.form.misc_http_x[0].checked == true 
				&& document.form.http_enable[1].selected == true){
				document.form.misc_httpport_x.disabled = true;
		}

		showLoading();
		document.form.submit();
	}
}

function checkDuplicateName(newname, teststr){
	var existing_string = decodeURIComponent(teststr.join(','));
	existing_string = "," + existing_string + ",";
	var newstr = "," + trim(newname) + ","; 

	var re = new RegExp(newstr,"gi")
	var matchArray =  existing_string.match(re);
	if (matchArray != null)
		return true;
	else
		return false;
}

function validForm(){	
	showtext($("alert_msg1"), "");
	showtext($("alert_msg2"), "");

	var alert_str = validate_account(document.form.http_username, "noalert");

	if(alert_str != ""){
		showtext($("alert_msg1"), alert_str);
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	document.form.http_username.value = trim(document.form.http_username.value);

	if(document.form.http_username.value.length == 0){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc1#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	if(document.form.http_username.value == "root"
			|| document.form.http_username.value == "guest"
			|| document.form.http_username.value == "anonymous"
			){
		showtext($("alert_msg1"), "<#USB_Application_account_alert#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	if(document.form.http_username.value.length <= 1){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc2#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	if(document.form.http_username.value.length > 20){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc3#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	if(checkDuplicateName(document.form.http_username.value, accounts)
			&& document.form.http_username.value != decodeURIComponent(accounts[0])){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc5#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}

	if(document.form.http_passwd2.value != document.form.v_password2.value){
		showtext($("alert_msg2"),"*<#File_Pop_content_alert_desc7#>");
		if(document.form.http_passwd2.value.length <= 0){
			document.form.http_passwd2.focus();
			document.form.http_passwd2.select();
		}else{
			document.form.v_password2.focus();
			document.form.v_password2.select();
		}	

		return false;
	}

	if(!validate_string(document.form.http_passwd2)){
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(document.form.http_passwd2.value.length > 16){
		showtext($("alert_msg2"),"*<#LANHostConfig_x_Password_itemdesc#>");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(!validate_ipaddr_final(document.form.log_ipaddr, 'log_ipaddr')
			|| !validate_string(document.form.ntp_server0)
			)
		return false;

	if(document.form.time_zone_dst_chk.checked
			&& document.form.dst_start_m.value == document.form.dst_end_m.value
			&& document.form.dst_start_w.value == document.form.dst_end_w.value
			&& document.form.dst_start_d.value == document.form.dst_end_d.value){
		alert("<#FirewallConfig_URLActiveTime_itemhint4#>");
		return false;
	}

	if(document.form.http_passwd2.value.length > 0)
		alert("<#File_Pop_content_alert_desc10#>");
		
	if (document.form.misc_http_x[0].checked) {
		if (!validate_range(document.form.misc_httpport_x, 1024, 65535))
			return false;

		if (HTTPS_support != -1 &&
		    !validate_range(document.form.misc_httpsport_x, 1024, 65535))
			return false;
	}
	else{
		document.form.misc_httpport_x.value = '<% nvram_get("misc_httpport_x"); %>';
		document.form.misc_httpsport_x.value = '<% nvram_get("misc_httpsport_x"); %>';
	}	

	return true;
}

function done_validating(action){
	refreshpage();
}

function corrected_timezone(){
	var today = new Date();
	var StrIndex;	
	
	if(today.toString().lastIndexOf("-") > 0)
		StrIndex = today.toString().lastIndexOf("-");
	else if(today.toString().lastIndexOf("+") > 0)
		StrIndex = today.toString().lastIndexOf("+");

	if(StrIndex > 0){		
		//alert('dstoffset='+dstoffset+', 設定時區='+timezone+' , 當地時區='+today.toString().substring(StrIndex, StrIndex+5))
		if(timezone != today.toString().substring(StrIndex, StrIndex+5)){
			$("timezone_hint").style.display = "block";
			$("timezone_hint").innerHTML = "* <#LANHostConfig_x_TimeZone_itemhint#>";
		}
		else
			return;			
	}
	else
		return;	
}

function show_dst_chk(){
	var tzdst = new RegExp("^[a-z]+[0-9\-\.:]+[a-z]+", "i");
	// match "[std name][offset][dst name]"
	if(document.form.time_zone_select.value.match(tzdst)){
		document.getElementById("chkbox_time_zone_dst").style.display="";	
		document.getElementById("adj_dst").innerHTML = Untranslated.Adj_dst;
		if(!document.getElementById("time_zone_dst_chk").checked){
				document.form.time_zone_dst.value=0;
				document.getElementById("dst_start").style.display="none";
				document.getElementById("dst_end").style.display="none";
		}else{
				parse_dstoffset();
				document.form.time_zone_dst.value=1;
				document.getElementById("dst_start").style.display="";
				document.getElementById("dst_end").style.display="";
		}
	}else{
		document.getElementById("chkbox_time_zone_dst").style.display="none";
		document.getElementById("chkbox_time_zone_dst").checked=false;
		document.form.time_zone_dst.value=0;
		document.getElementById("dst_start").style.display="none";
		document.getElementById("dst_end").style.display="none";
	}	
}

var timezones = [
	["UTC12",	"(GMT-12:00) <#TZ01#>"],
	["UTC11",	"(GMT-11:00) <#TZ02#>"],
	["UTC10",	"(GMT-10:00) <#TZ03#>"],
	["NAST9DST",	"(GMT-09:00) <#TZ04#>"],
	["PST8DST",	"(GMT-08:00) <#TZ05#>"],
	["MST7DST_1",	"(GMT-07:00) <#TZ06#>"],
	["MST7_2",	"(GMT-07:00) <#TZ07#>"],
	["MST7DST_3",	"(GMT-07:00) <#TZ08#>"],
	["CST6DST_1",	"(GMT-06:00) <#TZ09#>"],
	["CST6DST_2",	"(GMT-06:00) <#TZ10#>"],
	["CST6DST_3",	"(GMT-06:00) <#TZ11#>"],
	["CST6DST_3_1",	"(GMT-06:00) <#TZ12#>"],
	["UTC6",	"(GMT-06:00) <#TZ13#>"],
	["EST5DST",	"(GMT-05:00) <#TZ14#>"],
	["UTC5_1",	"(GMT-05:00) <#TZ15#>"],
	["UTC5_2",	"(GMT-05:00) <#TZ16#>"],
	["AST4DST",	"(GMT-04:00) <#TZ17#>"],
	["UTC4_1",	"(GMT-04:00) <#TZ18#>"],
	["UTC4DST_2",	"(GMT-04:00) <#TZ19#>"],
	["NST3.30DST",	"(GMT-03:30) <#TZ20#>"],
	["EBST3DST_1",	"(GMT-03:00) <#TZ21#>"],
	["UTC3",	"(GMT-03:00) <#TZ22#>"],
	["EBST3DST_2",	"(GMT-03:00) <#TZ23#>"],
	["NORO2DST",	"(GMT-02:00) <#TZ24#>"],
	["EUT1DST",	"(GMT-01:00) <#TZ25#>"],
	["UTC1",	"(GMT-01:00) <#TZ26#>"],
	["GMT0DST_1",	"(GMT) <#TZ27#>"],
	["GMT0DST_2",	"(GMT) <#TZ28#>"],
	/*["GMT0DST_3",	"(GMT) <#TZ85#>"],	use adj_dst to desc*/
	["UTC-1DST_1",	"(GMT+01:00) <#TZ29#>"],
	["UTC-1_1_1",	"(GMT+01:00) <#TZ30#>"],
	["UTC-1_2",	"(GMT+01:00) <#TZ31#>"],
	["UTC-1_2_1",	"(GMT+01:00) <#TZ32#>"],
	["MET-1DST",	"(GMT+01:00) <#TZ33#>"],
	["MET-1DST_1",	"(GMT+01:00) <#TZ34#>"],
	["MEZ-1DST",	"(GMT+01:00) <#TZ35#>"],
	["MEZ-1DST_1",	"(GMT+01:00) <#TZ36#>"],
	["UTC-1_3",	"(GMT+01:00) <#TZ37#>"],
	["UTC-2DST",	"(GMT+02:00) <#TZ38#>"],
	["EST-2DST",	"(GMT+02:00) <#TZ39#>"],
	["UTC-2_1",	"(GMT+02:00) <#TZ40#>"],
	["UTC-2DST_2",	"(GMT+02:00) <#TZ41#>"],
	["IST-2DST",	"(GMT+02:00) <#TZ42#>"],
	["SAST-2",	"(GMT+02:00) <#TZ43#>"],
	["EET-2DST",	"(GMT+02:00) <#TZ43_2#>"],
	["UTC-3_1",	"(GMT+03:00) <#TZ46#>"],
	["UTC-3_2",	"(GMT+03:00) <#TZ47#>"],
	["IST-3DST",	"(GMT+03:00) <#TZ48#>"],
	["UTC-3.30DST",	"(GMT+03:30) <#TZ49#>"],
	["UTC-4_2",	"(GMT+04:00) <#TZ44#>"],
	["UTC-4_3",	"(GMT+04:00) <#TZ45#>"],
	["UTC-4_1",	"(GMT+04:00) <#TZ50#>"],
	["UTC-4DST_2",	"(GMT+04:00) <#TZ51#>"],
	["UTC-4.30",	"(GMT+04:30) <#TZ52#>"],
	["UTC-5",	"(GMT+05:00) <#TZ54#>"],
	["UTC-5.30_2",	"(GMT+05:30) <#TZ55#>"],
	["UTC-5.30_1",	"(GMT+05:30) <#TZ56#>"],
	["UTC-5.30",	"(GMT+05:30) <#TZ59#>"],
	["UTC-5.45",	"(GMT+05:45) <#TZ57#>"],
	["RFT-6DST",	"(GMT+06:00) <#TZ60#>"],
	["UTC-6_1",	"(GMT+06:00) <#TZ53#>"],
	["UTC-6",	"(GMT+06:00) <#TZ58#>"],
	["UTC-6.30",	"(GMT+06:30) <#TZ61#>"],
	["UTC-7",	"(GMT+07:00) <#TZ62#>"],
	["CST-8_2",	"(GMT+08:00) <#TZ63#>"],
	["CST-8",	"(GMT+08:00) <#TZ64#>"],
	["CST-8_1",	"(GMT+08:00) <#TZ65#>"],
	["SST-8",	"(GMT+08:00) <#TZ66#>"],
	["CCT-8",	"(GMT+08:00) <#TZ67#>"],
	["WAS-8DST",	"(GMT+08:00) <#TZ68#>"],
	["UTC-8DST",	"(GMT+08:00) <#TZ69#>"],
	["UTC-9_1",	"(GMT+09:00) <#TZ70#>"],
	["JST",		"(GMT+09:00) <#TZ71#>"],
	["CST-9.30DST",	"(GMT+09:30) <#TZ73#>"],
	["UTC-9.30",	"(GMT+09:30) <#TZ74#>"],
	["UTC-10_3",	"(GMT+10:00) <#TZ72#>"],
	["UTC-10_1",	"(GMT+10:00) <#TZ75#>"],
	["UTC-10_2",	"(GMT+10:00) <#TZ76#>"],
	["TST-10TDT",	"(GMT+10:00) <#TZ77#>"],
	["UTC-10_5",	"(GMT+10:00) <#TZ79#>"],
	["UTC-11_2",	"(GMT+11:00) <#TZ78#>"],
	["UTC-11",	"(GMT+11:00) <#TZ80#>"],
	["UTC-11_1",	"(GMT+11:00) <#TZ81#>"],
	["UTC-12",	"(GMT+12:00) <#TZ82#>"],
	["NZST-12DST",	"(GMT+12:00) <#TZ83#>"],
	["UTC-13",	"(GMT+13:00) <#TZ84#>"]];

function load_timezones(){
	free_options(document.form.time_zone_select);
	for(var i = 0; i < timezones.length; i++){
		add_tz_option(document.form.time_zone_select,
			timezones[i][1], timezones[i][0],
			(document.form.time_zone.value == timezones[i][0]));
	}
}

var dst_month = new Array("", "M1", "M2", "M3", "M4", "M5", "M6", "M7", "M8", "M9", "M10", "M11", "M12");
var dst_week = new Array("", "1", "2", "3", "4", "5");
var dst_day = new Array("0", "1", "2", "3", "4", "5", "6");
var dst_hour = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
													"13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23");
var dstoff_start_m,dstoff_start_w,dstoff_start_d,dstoff_start_h;
var dstoff_end_m,dstoff_end_w,dstoff_end_d,dstoff_end_h;

function parse_dstoffset(){	//Mm.w.d/h,Mm.w.d/h
	var dstoffset_startend = dstoffset.split(",");
		var dstoffset_start = dstoffset_startend[0];
		var dstoff_start = dstoffset_start.split(".");
			dstoff_start_m = dstoff_start[0];
			dstoff_start_w = dstoff_start[1];
			dstoff_start_d = dstoff_start[2].split("/")[0];
			dstoff_start_h = dstoff_start[2].split("/")[1];
		var dstoffset_end = dstoffset_startend[1];
		var dstoff_end = dstoffset_end.split(".");
			dstoff_end_m = dstoff_end[0];
			dstoff_end_w = dstoff_end[1];
			dstoff_end_d = dstoff_end[2].split("/")[0];
			dstoff_end_h = dstoff_end[2].split("/")[1];
}
															
function load_dst_m_Options(){
	free_options(document.form.dst_start_m);
	free_options(document.form.dst_end_m);
	for(var i = 1; i < dst_month.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==3){
				add_option(document.form.dst_start_m, dst_month[i], i, 1);
				add_option(document.form.dst_end_m, dst_month[i], i, 0);
			}else if(i==10){
				add_option(document.form.dst_start_m, dst_month[i], i, 0);
				add_option(document.form.dst_end_m, dst_month[i], i, 1);
			}else{
				add_option(document.form.dst_start_m, dst_month[i], i, 0);
				add_option(document.form.dst_end_m, dst_month[i], i, 0);
			}
		}else{
			if(dstoff_start_m =='M'+i)
				add_option(document.form.dst_start_m, dst_month[i], i, 1);
			else	
				add_option(document.form.dst_start_m, dst_month[i], i, 0);
			
			if(dstoff_end_m =='M'+i)
				add_option(document.form.dst_end_m, dst_month[i], i, 1);
			else
				add_option(document.form.dst_end_m, dst_month[i], i, 0);							
		}	
		
	}	
}

function load_dst_w_Options(){
	free_options(document.form.dst_start_w);
	free_options(document.form.dst_end_w);
	for(var i = 1; i < dst_week.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==2){
				add_option(document.form.dst_start_w, dst_week[i], i, 1);
				add_option(document.form.dst_end_w, dst_week[i], i, 1);
			}else{
				add_option(document.form.dst_start_w, dst_week[i], i, 0);
				add_option(document.form.dst_end_w, dst_week[i], i, 0);
			}
		}else{		
			if(dstoff_start_w == i)
				add_option(document.form.dst_start_w, dst_week[i], i, 1);
			else	
				add_option(document.form.dst_start_w, dst_week[i], i, 0);
			
			if(dstoff_end_w == i)
				add_option(document.form.dst_end_w, dst_week[i], i, 1);
			else
				add_option(document.form.dst_end_w, dst_week[i], i, 0);			
		}		
		
	}	
}

function load_dst_d_Options(){
	free_options(document.form.dst_start_d);
	free_options(document.form.dst_end_d);
	for(var i = 0; i < dst_day.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==0){
				add_option(document.form.dst_start_d, dst_day[i], i, 1);
				add_option(document.form.dst_end_d, dst_day[i], i, 1);
			}else{
				add_option(document.form.dst_start_d, dst_day[i], i, 0);
				add_option(document.form.dst_end_d, dst_day[i], i, 0);
			}
		}else{
			if(dstoff_start_d == i)
				add_option(document.form.dst_start_d, dst_day[i], i, 1);
			else	
				add_option(document.form.dst_start_d, dst_day[i], i, 0);
			
			if(dstoff_end_d == i)
				add_option(document.form.dst_end_d, dst_day[i], i, 1);
			else
				add_option(document.form.dst_end_d, dst_day[i], i, 0);			
		}
			
	}	
}

function load_dst_h_Options(){
	free_options(document.form.dst_start_h);
	free_options(document.form.dst_end_h);
	for(var i = 0; i < dst_hour.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==2){
				add_option(document.form.dst_start_h, dst_hour[i], i, 1);
				add_option(document.form.dst_end_h, dst_hour[i], i, 1);
			}else{
				add_option(document.form.dst_start_h, dst_hour[i], i, 0);
				add_option(document.form.dst_end_h, dst_hour[i], i, 0);
			}
		}else{
			if(dstoff_start_h == i)
				add_option(document.form.dst_start_h, dst_hour[i], i, 1);
			else	
				add_option(document.form.dst_start_h, dst_hour[i], i, 0);
			
			if(dstoff_end_h == i)
				add_option(document.form.dst_end_h, dst_hour[i], i, 1);
			else
				add_option(document.form.dst_end_h, dst_hour[i], i, 0);			
		}
	}	
}

function add_tz_option(selectObj, str, value, selected){
	var tail = selectObj.options.length;
	
	if(typeof(str) != "undefined")
		selectObj.options[tail] = new Option(str);
	else
		selectObj.options[tail] = new Option();
	
	if(typeof(value) != "undefined")
		selectObj.options[tail].value = value;
	else
		selectObj.options[tail].value = "";
	
	if(selected)
		selectObj.options[tail].selected = 1;
}

function hide_https_lanport(_value){
	$("https_lanport").style.display = (_value == "0") ? "none" : "";
}

function hide_https_wanport(_value){
	$("http_port").style.display = (_value == "1") ? "none" : "";	
	$("https_port").style.display = (_value == "0") ? "none" : "";	
}

// show clientlist
function show_http_clientlist(){
	var http_clientlist_row = http_clientlist_array.split('&#60');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="http_clientlist_table">'; 
	if(http_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i =1; i < http_clientlist_row.length; i++){
		code +='<tr id="row'+i+'">';
		code +='<td width="80%">'+ http_clientlist_row[i] +'</td>';		//Url keyword
		code +='<td width="20%">';
		code +="<input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow(this);\" value=\"\"/></td>";
		}
	}
  	code +='</tr></table>';
	
	$("http_clientlist_Block").innerHTML = code;
}

function deleteRow(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('http_clientlist_table').deleteRow(i);
  
  var http_clientlist_value = "";
	for(i=0; i<$('http_clientlist_table').rows.length; i++){
		http_clientlist_value += "&#60";
		http_clientlist_value += $('http_clientlist_table').rows[i].cells[0].innerHTML;
	}
	
	http_clientlist_array = http_clientlist_value;
	if(http_clientlist_array == "")
		show_http_clientlist();
}

function addRow(obj, upper){
	if('<% nvram_get("http_client"); %>' != "1")
		document.form.http_client[0].checked = true;
		
	//Viz check max-limit 
	var rule_num = $('http_clientlist_table').rows.length;
	var item_num = $('http_clientlist_table').rows[0].cells.length;		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}
			
	if(obj.value == ""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();			
		return false;
	}
	else if(valid_IP_form(obj, 0) != true){
		return false;
	}
	else{		
		//Viz check same rule
		for(i=0; i<rule_num; i++){
			for(j=0; j<item_num-1; j++){		//only 1 value column
				if(obj.value == $('http_clientlist_table').rows[i].cells[j].innerHTML){
					alert("<#JS_duplicate#>");
					return false;
				}	
			}
		}
		
		http_clientlist_array += "&#60";
		http_clientlist_array += obj.value;
		obj.value = "";		
		show_http_clientlist();
	}	
}

function keyBoardListener(evt){
	var nbr = (window.evt)?event.keyCode:event.which;
	if(nbr == 13)
		addRow(document.form.http_client_ip_x_0, 4);
}


//Viz add 2012.02 LAN client ip { start

function showLANIPList(){
	var code = "";
	var show_name = "";
	var client_list_array = '<% get_client_detail_info(); %>';	
	var client_list_row = client_list_array.split('<');	

	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');
		if(client_list_col[1] && client_list_col[1].length > 20)
			show_name = client_list_col[1].substring(0, 16) + "..";
		else
			show_name = client_list_col[1];	

		//client_list_col[]  0:type 1:device 2:ip 3:mac 4: 5: 6:
		code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[2]+'\');"><strong>'+client_list_col[2]+'</strong> ';
		
		if(show_name && show_name.length > 0)
				code += '( '+show_name+')';
		code += ' </div></a>';
		}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	$("ClientList_Block_PC").innerHTML = code;
}

function setClientIP(ipaddr){
	document.form.http_client_ip_x_0.value = ipaddr;
	hideClients_Block();
	over_var = 0;
}


var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	$("pull_arrow").src = "/images/arrow-down.gif";
	$('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullLANIPList(obj){
	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';		
		document.form.http_client_ip_x_0.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}
//Viz add 2012.02 LAN client ip } end 

function hideport(flag){
	$("accessfromwan_port").style.display = (flag == 1) ? "" : "none";
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_System_Content.asp">
<input type="hidden" name="next_page" value="Advanced_System_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_time;restart_httpd">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="time_zone_dst" value="<% nvram_get("time_zone_dst"); %>">
<input type="hidden" name="time_zone" value="<% nvram_get("time_zone"); %>">
<input type="hidden" name="time_zone_dstoff" value="<% nvram_get("time_zone_dstoff"); %>">
<input type="hidden" name="http_passwd" value="<% nvram_get("http_passwd"); %>">
<input type="hidden" name="http_clientlist" value="<% nvram_get("http_clientlist"); %>">

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
		<td valign="top">
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>

	<tr>
  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_6_adv#> - <#menu5_6_2#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#Syste_title#></div>

	  	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
	  	<tr>
          <td colspan="2"><#PASS_changepasswd#></td>
        </tr>
    	</thead>
        <tr>
          <th width="40%"><#Router_Login_Name#></th>
          <td>
				  	<div id="http_username_span" name="http_username_span" style="color:#FFFFFF;margin-left:8px;"><% nvram_get("http_username"); %></div>
						<input type="text" id="http_username" name="http_username" style="height:25px;" class="input_15_table" maxlength="20" value='<% nvram_get("http_username"); %>'><br/><span id="alert_msg1"></span>
          </td>
        </tr>

        <tr>
          <th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_new#></a></th>
          <td>
            <input type="password" autocapitalization="off" name="http_passwd2" onKeyPress="return is_string(this, event);" onkeyup="chkPass(this.value, 'http_passwd');" class="input_15_table" maxlength="17" />
            &nbsp;&nbsp;
            <div id="scorebarBorder" style="margin-left:140px; margin-top:-25px; display:none;" title="Strength of password">
            		<div id="score">Very Weak</div>
            		<div id="scorebar">&nbsp;</div>
            </div>
          </td>
        </tr>

        <tr>
          <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_retype#></a></th>
          <td>
            <input type="password" autocapitalization="off" name="v_password2" onKeyPress="return is_string(this, event);" class="input_15_table" maxlength="17" /><br/><span id="alert_msg2"></span>
          </td>
        </tr>
      </table>

<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
                <thead>
                <tr>
          <td colspan="2">Persistent JFFS2 partition</td>
        </tr>
        </thead>
           <tr>
              <th>Enable JFFS partition</th>
              <td>
                  <input type="radio" name="jffs2_on" class="input" value="1" <% nvram_match_x("LANHostConfig", "jffs2_on", "1", "checked"); %>><#checkbox_Yes#>
                  <input type="radio" name="jffs2_on" class="input" value="0" <% nvram_match_x("LANHostConfig", "jffs2_on", "0", "checked"); %>><#checkbox_No#>
              </td>
          </tr>
          <tr>
              <th>Format JFFS partition at next boot</th>
              <td>
                  <input type="radio" name="jffs2_format" class="input" value="1" <% nvram_match_x("LANHostConfig", "jffs2_format", "1", "checked"); %>><#checkbox_Yes#>
                  <input type="radio" name="jffs2_format" class="input" value="0" <% nvram_match_x("LANHostConfig", "jffs2_format", "0", "checked"); %>><#checkbox_No#>
              </td>
          </tr>
      </table>

      <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
      	<thead>
	  	<tr>
          <td colspan="2"><#t2Misc#></td>
        </tr>
    	</thead>
	<tr id="btn_ez_radiotoggle_tr">
		<th>WPS Button behavior</th>
		<td>
			<input type="radio" name="btn_ez_radiotoggle" class="input" value="1" <% nvram_match_x("", "btn_ez_radiotoggle", "1", "checked"); %>>Toggle Radio
			<input type="radio" name="btn_ez_radiotoggle" class="input" value="0" <% nvram_match_x("", "btn_ez_radiotoggle", "0", "checked"); %>>Activate WPS
		</td>
	</tr>
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,1)"><#LANHostConfig_x_ServerLogEnable_itemname#></a></th>
          <td><input type="text" maxlength="15" class="input_15_table" name="log_ipaddr" value="<% nvram_get("log_ipaddr"); %>" onKeyPress="return is_ipaddr(this, event)" ></td>
        </tr>
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,2)"><#LANHostConfig_x_TimeZone_itemname#></a></th>
          <td>
            <select name="time_zone_select" class="input_option" onchange="return change_common(this, 'LANHostConfig', 'time_zone_select')">
           		<option value="<% nvram_get("time_zone"); %>" selected><% nvram_get("time_zone"); %></option>
            </select>
          	<div>
          		<span id="chkbox_time_zone_dst" style="color:white;display:none;">
          			<input type="checkbox" name="time_zone_dst_chk" id="time_zone_dst_chk" <% nvram_match("time_zone_dst", "1", "checked"); %> class="input" onClick="return change_common(this,'LANHostConfig','time_zone_dst_chk')">
          			<label for="time_zone_dst_chk"><span id="adj_dst"></span></label>
          			<br>
          		</span>	
          		<span id="dst_start" style="color:white;display:none;">
          				<b>DST start time</b>
          				<select name="dst_start_m" class="input_option" onchange=""></select>&nbsp;month &nbsp;
          				<select name="dst_start_w" class="input_option" onchange=""></select>&nbsp;week &nbsp;
          				<select name="dst_start_d" class="input_option" onchange=""></select>&nbsp;weekday &nbsp;
          				<select name="dst_start_h" class="input_option" onchange=""></select>&nbsp;hour &nbsp;
          			<br>
          		</span>
          		<span id="dst_end" style="color:white;display:none;">
          			<b>DST end time</b>&nbsp;&nbsp;
    	      			<select name="dst_end_m" class="input_option" onchange=""></select>&nbsp;month &nbsp;
  	        			<select name="dst_end_w" class="input_option" onchange=""></select>&nbsp;week &nbsp;
	          			<select name="dst_end_d" class="input_option" onchange=""></select>&nbsp;weekday &nbsp;
          				<select name="dst_end_h" class="input_option" onchange=""></select>&nbsp;hour &nbsp;
          				<br>
          		</span>          			
            	<span id="timezone_hint" style="display:none;"></span>
          	</div>
            </td>
        </tr>
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,3)"><#LANHostConfig_x_NTPServer_itemname#></a></th>
          <td>
						<input type="text" maxlength="256" class="input_32_table" name="ntp_server0" value="<% nvram_get("ntp_server0"); %>" onKeyPress="return is_string(this, event);">
    	      <a href="javascript:openLink('x_NTPServer1')"  name="x_NTPServer1_link" style=" margin-left:5px; text-decoration: underline;"><#LANHostConfig_x_NTPServer1_linkname#>
			</td>
        </tr>

				<tr>
				  <th><#Enable_Telnet#></th>
				  <td>
				    <input type="radio" name="telnetd_enable" class="input" value="1" <% nvram_match_x("LANHostConfig", "telnetd_enable", "1", "checked"); %>><#checkbox_Yes#>
				    <input type="radio" name="telnetd_enable" class="input" value="0" <% nvram_match_x("LANHostConfig", "telnetd_enable", "0", "checked"); %>><#checkbox_No#>
				  </td>
				</tr>
                                
				<tr>
					<th>Enable SSH</th>
					<td>
						<input type="radio" name="sshd_enable" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_enable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_enable" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_enable", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
        
				<tr>
					<th>Allow SSH Port Forwarding</th>
					<td>
						<input type="radio" name="sshd_forwarding" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_forwarding", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_forwarding" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_forwarding", "0", "checked"); %>><#checkbox_No#>
           
					</td>
					</tr>

					<tr id="ssh_lanport">
						<th>SSH service port</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="sshd_port" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("sshd_port"); %>">
						</td>        
					</tr>

					<tr>
						<th>Allow SSH access from WAN</th>
						<td>
							<input type="radio" name="sshd_wan" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_wan", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="sshd_wan" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_wan", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
					<tr>
						<th>Allow SSH password login</th>
						<td>
							<input type="radio" name="sshd_pass" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_pass", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="sshd_pass" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_pass", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>

					<tr>
						<th>SSH Authentication key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="sshd_authkeys" cols="55" maxlength="1024"><% nvram_clean_get("sshd_authkeys"); %></textarea>
							<span id="ssh_alert_msg"></span>
						</td>
				</tr>

		  	<tr id="https_tr">
					<th><#WLANConfig11b_AuthenticationMethod_itemname#></th>
					<td>
				  	<select name="http_enable" class="input_option" onchange="hide_https_lanport(this.value);hide_https_wanport(this.value);">
							<option value="0" <% nvram_match("http_enable", "0", "selected"); %>>HTTP</option>
							<option value="1" <% nvram_match("http_enable", "1", "selected"); %>>HTTPS</option>
							<option value="2" <% nvram_match("http_enable", "2", "selected"); %>>BOTH</option>
				  	</select>
					</td>
		  	</tr>

		  	<tr id="https_lanport">
					<th>HTTPS Lan port</th>
          <td>
						<input type="text" maxlength="5" class="input_6_table" name="https_lanport" value="<% nvram_get("https_lanport"); %>">
					</td>
		  	</tr>
		  	
        <tr id="misc_http_x_tr">
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(8,2);"><#FirewallConfig_x_WanWebEnable_itemname#></a></th>
           	<td>
             		<input type="radio" value="1" name="misc_http_x" class="input" onClick="hideport(1);return change_common_radio(this, 'FirewallConfig', 'misc_http_x', '1')" <% nvram_match("misc_http_x", "1", "checked"); %>><#checkbox_Yes#>
             		<input type="radio" value="0" name="misc_http_x" class="input" onClick="hideport(0);return change_common_radio(this, 'FirewallConfig', 'misc_http_x', '0')" <% nvram_match("misc_http_x", "0", "checked"); %>><#checkbox_No#>
           	</td>
        </tr>   					
        
        <tr id="accessfromwan_port">
           	<th align="right"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(8,3);"><#FirewallConfig_x_WanWebPort_itemname#></a></th>
           	<td>
								<span style="margin-left:5px;" id="http_port">HTTP: <input type="text" maxlength="5" name="misc_httpport_x" class="input_6_table" value="<% nvram_get("misc_httpport_x"); %>" onKeyPress="return is_number(this,event);"/>&nbsp;&nbsp;</span>
								<span style="margin-left:5px;" id="https_port">HTTPS: <input type="text" maxlength="5" name="misc_httpsport_x" class="input_6_table" value="<% nvram_get("misc_httpsport_x"); %>" onKeyPress="return is_number(this,event);"/></span>
						</td>
        </tr>		  	

				<tr id="http_client_tr">
				  <th>Only allow specific IP</th>
				  <td>
				    <input type="radio" name="http_client" class="input" value="1" <% nvram_match_x("", "http_client", "1", "checked"); %>><#checkbox_Yes#>
				    <input type="radio" name="http_client" class="input" value="0" <% nvram_match_x("", "http_client", "0", "checked"); %>><#checkbox_No#>
				  </td>
				</tr>
      </table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="http_client_table">
				<thead>
					<tr>
						<td colspan="4">Specified IP</td>
					</tr>
				</thead>
			
			  <tr>
					<th width="80%"><#ConnectedClient#></th>
					<th width="20%">Add / Delete</th>
				</tr>

				<tr>
					<!-- client info -->
					<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>					
					
					<td width="80%">
				 		<input type="text" class="input_32_table" maxlength="15" name="http_client_ip_x_0"  onKeyPress="" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}">
            <img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;" onclick="pullLANIPList(this);" title="Select the device name of LAN clients." onmouseover="over_var=1;" onmouseout="over_var=0;">				 		
				 	</td>
				 	<td width="20%">	
				  		<input class="add_btn" type="button" onClick="addRow(document.form.http_client_ip_x_0, 4);" value="">
				 	</td>	
				</tr>
			</table>
     	<div id="http_clientlist_Block"></div>

     	<div class="apply_gen">
     		<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
     	</div>
      
      </td></tr>
</tbody>

</table></td>
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
