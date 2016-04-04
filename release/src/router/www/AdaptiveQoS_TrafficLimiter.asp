<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="css/icon.css">
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script>
<script type="text/javascript" src="/chart.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/form.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
window.onresize = function() {
	if(document.getElementById("alert_preference").style.display == "block") {
		cal_panel_block("alert_preference", 0.25);
	}
} 
var wans_caps = '<% nvram_get("wans_cap"); %>';
var wan_unit = '<% nvram_get("wan_unit"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var dualwan_enable = wans_dualwan_orig.search("none") == -1 ? true:false;
var dualwan_type = wans_dualwan_orig.split(" ");
var tl_cycle = '<% nvram_get("tl_cycle"); %>';
var tl0_limit_enable = '<% nvram_get("tl0_limit_enable"); %>';
var tl0_limit_max = '<% nvram_get("tl0_limit_max"); %>';
var tl1_limit_enable = '<% nvram_get("tl1_limit_enable"); %>';
var tl1_limit_max = '<% nvram_get("tl1_limit_max"); %>';
var tl0_alert_enable = '<% nvram_get("tl0_alert_enable"); %>';
var tl0_alert_max = '<% nvram_get("tl0_alert_max"); %>';
var tl1_alert_enable = '<% nvram_get("tl1_alert_enable"); %>';
var tl1_alert_max = '<% nvram_get("tl1_alert_max"); %>';
var info = {
	dualwan_enabled: "",
	primary_wan: {
		ifname: "",
		alert_enable: "",
		alert_max: "",
		limit_enable: "",
		limit_max: ""
	},
	secondary_wan: {
		ifname: "",
		alert_enable: "",
		alert_max: "",
		limit_enable: "",
		limit_max: ""		
	},
	current_wan:{
		ifname: "",
		alert_enable: "",
		alert_max: "",
		limit_enable: "",
		limit_max: ""
	},

	primary_if: "",
	secondary_if: "",
	current_wan_unit: "",
	cycle_start_date: "",
	current_timestamp: "",
	current_year: "",
	current_month: "",
	current_date: "",
	period1: {
		start_timestamp: "",		//traffic bar start
		end_timestamp: "",			//traffic bar end
		start_year: "",
		start_date: "",
		start_month: "",
		end_year: "",
		end_month: "",
		end_date: ""		
	},
	period2: {
		start_timestamp: "",
		end_timestamp: "",
		start_year: "",
		start_date: "",
		start_month: "",
		end_year: "",
		end_month: "",
		end_date: ""			
	}
}

function translate_traffic(flow){
	var flow_unit = "KB";
	if(flow > 1024){
		flow_unit = "MB";
		flow = flow/1024;
		if(flow > 1024){
			flow_unit = "GB";
			flow = flow/1024;
		}
	}

	return [flow.toFixed(2), flow_unit];
}

$(document).ready(function (){
	show_menu();
	
	generate_cycle_date();
	showclock();
	collect_info();
	check_date();
	
	$("#wan_type").html(info.current_wan.ifname.toUpperCase());
	if(info.current_wan.alert_enable != 0){
		document.form.tl_alert_enable.checked = true;		
	}
	else{
		document.form.tl_alert_enable.checked = false;
	}
	
	document.form.tl_alert_max.value = info.current_wan.alert_max;
	if(info.current_wan.limit_enable != 0){
		document.form.tl_limit_enable.checked = true;		
	}
	else{
		document.form.tl_limit_enable.checked = false;
	}
	
	document.form.tl_limit_max.value = info.current_wan.limit_max;
	if(!dualwan_enable){
		document.getElementById("wan_tab").style.display = "none";
	}
	
	if(document.form.tl_enable.value == 1){
		document.getElementById("time_field").style.visibility = "visible";
		$("#main_field").show();	
		$("#apply_div").show();
		corrected_timezone();
	}
	else{
		document.getElementById("time_field").style.visibility = "hidden";
		$("#main_field").hide();
		$("#apply_div").hide();
	}

	if(document.form.tl_limit_enable.checked){
		document.getElementById("limit_value_field").style.display = "table-cell";
		document.form.tl_limit_enable.value = "1";
	}	
	else{
		document.getElementById("limit_value_field").style.display = "none";
		document.form.tl_limit_enable.value = "0";
	}
	
	if(document.form.tl_alert_enable.checked){
		document.getElementById("alert_value_field").style.display = "table-cell";
		document.form.tl_alert_enable.value = "1";
	}	
	else{
		document.getElementById("alert_value_field").style.display = "none";
		document.form.tl_alert_enable.value = "0";
	}

return true;	
	
		$("#drag_line").draggable({
			axis:"y",
			scroll: true,
			drag: function(event, ui){
				if(ui.position.top < 544){
					ui.position.top = "544";
				}
				else if(ui.position.top > 844){
					ui.position.top = "844";
				}
			}
		}); 
		
		
});

function generate_cycle_date(){
	var code = "";
	for(i=1;i<=31;i++){
		if(i == tl_cycle)
			code += "<option selected>";
		else
			code += "<option>";
		
		code += i +"</option>";
	}
	
	$("#tl_cycle").html(code);
}

function showclock(){
	JS_timeObj.setTime(systime_millsec);
	systime_millsec += 1000;
	JS_timeObj2 = JS_timeObj.toString();	
	JS_timeObj2 = JS_timeObj2.substring(0,3) + ", " +
	              JS_timeObj2.substring(4,10) + "  " +
				  checkTime(JS_timeObj.getHours()) + ":" +
				  checkTime(JS_timeObj.getMinutes()) + ":" +
				  checkTime(JS_timeObj.getSeconds()) + "  " +
				  JS_timeObj.getFullYear();
	document.getElementById("system_time").innerHTML = JS_timeObj2;
	setTimeout("showclock()", 1000);
}


function render_bar(flag){
	var total_traffic = 0;
	var max_scale = "";
	var max_scale_unit = "KB";
	var max_scale_traffic = "";
	var code_scale = "";
	var temp = "";
	for(i=0;i<total_traffic_array.length;i++){		//collect Traffic-Date array
		total_traffic += total_traffic_array[i];
	}
	
	temp = translate_traffic(total_traffic);
	$("#total_traffic").html(temp[0] + " " + temp[1]);		// show total traffic info
	
	if(document.form.tl_limit_enable.checked){
		if((document.form.tl_limit_max.value *1024*1024) > total_traffic){
			total_traffic = document.form.tl_limit_max.value *1024*1024;
		}
	}
	else if(document.form.tl_alert_enable.checked){
		if((document.form.tl_alert_max.value *1024*1024) > total_traffic){
			total_traffic = document.form.tl_alert_max.value *1024*1024;
		}
	}

	//calculate max scale of chart
	if((total_traffic/1024) > 1){	//MB				
		max_scale_unit = "MB";
		if((total_traffic/1024/1024) > 1){	//GB	
			max_scale_unit = "GB";
		}
	}	
	
	if(max_scale_unit == "GB"){
		max_scale = Math.round((total_traffic/1024/1024)*4/3);
		max_scale_traffic = max_scale*1024*1024;
	}
	else if(max_scale_unit == "MB"){
		max_scale = Math.round((total_traffic/1024)*4/3);
		max_scale_traffic = max_scale*1024;
	}
	else{
		max_scale = Math.round(total_traffic*4/3);
		max_scale_traffic = max_scale;
	}

	//end calculate max scale of chart	
	var traffic_temp = 0;
	var max_scale_temp = 0;
	for(i=0;i<5;i++){
		max_scale_temp = (max_scale*(4-i)/4);
		if(i == 0){
			code_scale += '<div style="height:10px;border-bottom:1px solid transparent;position:relative;text-align:right">';	// top scale
		}
		else{
			code_scale += '<div style="height:74px;border-bottom:1px solid transparent;position:relative;text-align:right">';
		}
		
		code_scale += '<span style="position:absolute;bottom:0;right:5px;width:100px">' + max_scale_temp + ' ' + max_scale_unit + '</span>';
		code_scale += '</div>';
	}
	
	document.getElementById("scale").innerHTML = code_scale;
	var code = "";
	var time_temp = 0;
	if(flag == "p1"){
		time_temp = info.period1.start_timestamp;
	}
	else{
		time_temp = info.period2.start_timestamp;
	}

	var t = new Date();
	var date_temp = "";
	var date_string = "";
	var traffic_percentage = 0;
	var traffic_temp_array = new Array();
	for(i=0;i<total_traffic_array.length;i++){
		if(time_temp > info.current_timestamp){
			traffic_percentage = 0;
		}	
		else{
			traffic_percentage = 0;
			traffic_temp  += total_traffic_array[i];
			traffic_percentage = ((traffic_temp / max_scale_traffic)*100).toFixed(1);
			if(traffic_percentage < 1 && traffic_percentage != 0)
				traffic_percentage = 1;
		}
		
		t.setTime(time_temp);
		date_string = (t.getMonth()+1) + "/" + t.getDate() + " ";
		date_temp = t.getDate();
		traffic_temp_array = translate_traffic(traffic_temp);
		code += '<div style="display:table-cell;width:20px;height:300px;position:relative;">';
		code += '<div class="bar" style="width:15px;position:absolute;bottom:0;height:'+ traffic_percentage +'%" data-value="'+ traffic_temp_array[0] + traffic_temp_array[1] +'" data-date="'+ date_string +'"></div>';
		code += '<div style="position:absolute;bottom:-2em;width:15px;text-align:center">'+ date_temp +'</div>';
		code += '</div>';
		time_temp += 86400000;
	}

	var drag_percentage = 0;
	var drag_scale = 0;
	drag_percentage = (((document.form.tl_limit_max.value * 1024*1024)/max_scale_traffic)*100).toFixed(1);
	drag_scale = parseInt(844-(drag_percentage*3));
	current_position_limit = drag_scale;
	if(document.form.tl_limit_enable.checked){
		code += '<div id="drag_line_limit_div" style="width:620px;height:1px;background:#B71C1C;position:absolute;text-align:right;top:'+drag_scale+'px;visibility:visible;z-index:9;cursor:pointer;"><div id="drag_line_limit" style="width:100%;margin:-20px 0 0 -20px;font-size:18px;color:#B71C1C">'+document.form.tl_limit_max.value+' GB Cut-Off Internet</div></div>';	
	}
	else{
		code += '<div id="drag_line_limit_div" style="width:620px;height:1px;background:#B71C1C;position:absolute;text-align:right;top:'+drag_scale+'px;visibility:hidden;z-index:9"><div id="drag_line_limit" style="width:100%;margin:-20px 0 0 -20px;font-size:18px;color:#B71C1C">'+document.form.tl_limit_max.value+' GB Cut-Off Internet</div></div>';	
	}
	
	drag_percentage = (((document.form.tl_alert_max.value * 1024*1024)/max_scale_traffic)*100).toFixed(1);
	drag_scale = parseInt(844-(drag_percentage*3));
	current_position_alert = drag_scale;
	if(document.form.tl_alert_enable.checked){
		code += '<div id="drag_line_alert_div" style="width:620px;height:1px;background:#F57F17;position:absolute;text-align:right;top:'+drag_scale+'px;visibility:visible;cursor:pointer"><div id="drag_line_alert" style="width:100%;margin:-20px 0 0 -20px;font-size:18px;color:#F57F17">'+document.form.tl_alert_max.value+' GB Alert</div></div>';		
	}
	else{
		code += '<div id="drag_line_alert_div" style="width:620px;height:1px;background:#F57F17;position:absolute;text-align:right;top:'+drag_scale+'px;visibility:hidden;"><div id="drag_line_alert" style="width:100%;margin:-20px 0 0 -20px;font-size:18px;color:#F57F17">'+document.form.tl_alert_max.value+' GB Alert</div></div>';		
	}
	
	document.getElementById('bar').innerHTML = code;
	current_traffic_limit = document.form.tl_limit_max.value;	
	$("#drag_line_limit_div").draggable({
		axis:"y",
		scroll: true,
		drag: function(event, ui){
			if(ui.position.top < 544){
				ui.position.top = "544";
			}
			else if(ui.position.top > 844){
				ui.position.top = "844";
			}
			
	
			if(ui.position.top < current_position_limit){		//upper
				if(ui.position.top < 544)
					return false;
				
				gap = ui.position.top - 544;				
				current_traffic_limit = max_scale_traffic - (gap*(max_scale_traffic/300));
				current_traffic_div = (current_traffic_limit/1024/1024).toFixed(2);			
			}else{
				if(ui.position.top > 844)
					return false;
					
				gap = ui.position.top - 544;
				if(document.form.tl_alert_enable.checked){
					if((max_scale_traffic - (gap*(max_scale_traffic/300))) < (document.form.tl_alert_max.value*1024*1024))
						return false;
				}
			
				current_traffic_limit = max_scale_traffic - (gap*(max_scale_traffic/300));
				current_traffic_div = (current_traffic_limit/1024/1024).toFixed(2);
			}
			
			current_traffic_limit = current_traffic_limit/1024/1024;
			current_position_limit = ui.position.top;
			$("#drag_line_limit").html(current_traffic_div + " GB Cut-Off Internet");
		},
		stop: function(event, ui){
			document.form.tl_limit_max.value = current_traffic_limit.toFixed(2);
		}
	});
	
	current_traffic_alert = document.form.tl_alert_max.value;	
	$("#drag_line_alert_div").draggable({
		axis:"y",
		scroll: true,
		drag: function(event, ui){
			if(ui.position.top < 544){
				ui.position.top = "544";
			}
			else if(ui.position.top > 844){
				ui.position.top = "844";
			}
			
			if(ui.position.top < current_position_alert){		//upper
				if(ui.position.top < 544)
					return false;
				
				gap = ui.position.top - 544;
				if(document.form.tl_limit_enable.checked){
					if((max_scale_traffic - (gap*(max_scale_traffic/300))) > (document.form.tl_limit_max.value*1024*1024))
						return false;			
				}
								
				current_traffic_alert = max_scale_traffic - (gap*(max_scale_traffic/300));
				current_traffic_div = (current_traffic_alert/1024/1024).toFixed(2);			
			}else{
				if(ui.position.top > 844)
					return false;
					
				gap = ui.position.top - 544;
				current_traffic_alert = max_scale_traffic - (gap*(max_scale_traffic/300));
				current_traffic_div = (current_traffic_alert/1024/1024).toFixed(2);
			}
			
			current_traffic_alert = current_traffic_alert/1024/1024;
			current_position_limit = ui.position.top;
			
			$("#drag_line_alert").html(current_traffic_div + " GB Alert");
		},
		stop: function(event, ui){
			document.form.tl_alert_max.value = current_traffic_alert.toFixed(2);
		}
	}); 
}

function alert_control(flag){
	if(flag && flag != 0){
		if((parseFloat(document.form.tl_alert_max.value) > parseFloat(document.form.tl_limit_max.value)) && document.form.tl_limit_enable.checked){			
			document.form.tl_alert_max.value = (document.form.tl_limit_max.value*0.9).toFixed(2);
		}
		
		document.getElementById("alert_value_field").style.display = "table-cell";
		document.form.tl_alert_enable.value = "1";
		document.getElementById("drag_line_alert_div").style.visibility = "visible";
	}
	else{
		document.getElementById("alert_value_field").style.display = "none";
		document.form.tl_alert_enable.value = "0";
		document.getElementById("drag_line_alert_div").style.visibility = "hidden";
	}
	
	if($("#period_option")[0][0].selected){		// display time period 1
		render_bar("p1");
	}
	else{		// display time period 2
		render_bar("p2");
	}	
}
function limit_control(flag){
	if(flag && flag != 0){
		if((parseFloat(document.form.tl_alert_max.value) > parseFloat(document.form.tl_limit_max.value)) && document.form.tl_limit_enable.checked){
			document.form.tl_alert_max.value = (document.form.tl_limit_max.value*0.9).toFixed(2);
		}
		
		document.getElementById("limit_value_field").style.display = "table-cell";
		document.form.tl_limit_enable.value = "1";
		document.getElementById("drag_line_limit_div").style.visibility = "visible";
	}
	else{
		document.getElementById("limit_value_field").style.display = "none";
		document.form.tl_limit_enable.value = "0";
		document.getElementById("drag_line_limit_div").style.visibility = "hidden";
	}
	
	if($("#period_option")[0][0].selected){
		render_bar("p1");
	}
	else{
		render_bar("p2");
	}
}
function switch_wan(obj){
	var period_obj = document.getElementById("period_option");
	if(obj.id == "primary_tab"){
		info.current_wan_unit = 0;
		document.form.tl1_limit_enable.value = document.form.tl_limit_enable.value;
		document.form.tl1_limit_max.value = document.form.tl_limit_max.value;
		document.form.tl1_alert_enable.value = document.form.tl_alert_enable.value;
		document.form.tl1_alert_max.value = document.form.tl_alert_max.value;		
		$("#primary_div").attr("class", "block_filter_pressed");
		$("#primary_tab").attr("class", "block_filter_name_table_pressed");
		$("#secondary_div").attr("class", "block_filter");
		$("#secondary_tab").attr("class", "block_filter_name_table");
		document.getElementById("wan_type").innerHTML = info.primary_wan.ifname.toUpperCase();
		calculate_data(info, period_obj.value, info.primary_wan.ifname);
		info.current_wan.ifname = info.primary_wan.ifname;
		info.current_wan.alert_enable = info.primary_wan.alert_enable;
		info.current_wan.alert_max = info.primary_wan.alert_max;
		info.current_wan.limit_enable = info.primary_wan.limit_enable;
		info.current_wan.limit_max = info.primary_wan.limit_max;
	}
	else if(obj.id == "secondary_tab"){
		info.current_wan_unit = 1;
		document.form.tl0_limit_enable.value = document.form.tl_limit_enable.value;
		document.form.tl0_limit_max.value = document.form.tl_limit_max.value;
		document.form.tl0_alert_enable.value = document.form.tl_alert_enable.value;
		document.form.tl0_alert_max.value = document.form.tl_alert_max.value;		
		$("#primary_div").attr("class", "block_filter");
		$("#primary_tab").attr("class", "block_filter_name_table");
		$("#secondary_div").attr("class", "block_filter_pressed");
		$("#secondary_tab").attr("class", "block_filter_name_table_pressed");
		document.getElementById("wan_type").innerHTML = info.secondary_wan.ifname.toUpperCase();
		calculate_data(info, period_obj.value, info.secondary_wan.ifname);
		info.current_wan.ifname = info.secondary_wan.ifname;
		info.current_wan.alert_enable = info.secondary_wan.alert_enable;
		info.current_wan.alert_max = info.secondary_wan.alert_max;
		info.current_wan.limit_enable = info.secondary_wan.limit_enable;
		info.current_wan.limit_max = info.secondary_wan.limit_max;
	}

	if(info.current_wan.alert_enable != 0){
		document.form.tl_alert_enable.checked = true;		
	}
	else{
		document.form.tl_alert_enable.checked = false;
	}
	
	document.form.tl_alert_max.value = info.current_wan.alert_max;
	if(info.current_wan.limit_enable != 0){
		document.form.tl_limit_enable.checked = true;		
	}
	else{
		document.form.tl_limit_enable.checked = false;
	}
	
	document.form.tl_limit_max.value = info.current_wan.limit_max;
	alert_control(info.current_wan.alert_enable);
	limit_control(info.current_wan.limit_enable);	
}

function collect_info(){
	info.dualwan_enabled = dualwan_enable;
	info.primary_wan.ifname = dualwan_type[0];
	info.secondary_wan.ifname = dualwan_type[1];
	info.primary_wan.alert_enable = tl0_alert_enable;
	info.primary_wan.alert_max = tl0_alert_max;
	info.primary_wan.limit_enable = tl0_limit_enable;
	info.primary_wan.limit_max = tl0_limit_max;
	
	info.secondary_wan.alert_enable = tl1_alert_enable;
	info.secondary_wan.alert_max = tl1_alert_max;
	info.secondary_wan.limit_enable = tl1_limit_enable;
	info.secondary_wan.limit_max = tl1_limit_max;
	
	info.current_wan_unit = wan_unit;
	if(wan_unit == 0){		
		info.current_wan.ifname = dualwan_type[0];
		info.current_wan.alert_enable = tl0_alert_enable;
		info.current_wan.alert_max = tl0_alert_max;
		info.current_wan.limit_enable = tl0_limit_enable;
		info.current_wan.limit_max = tl0_limit_max;
	}	
	else{	
		info.current_wan.ifname = dualwan_type[1];
		info.current_wan.alert_enable = tl1_alert_enable;
		info.current_wan.alert_max = tl1_alert_max;
		info.current_wan.limit_enable = tl1_limit_enable;
		info.current_wan.limit_max = tl1_limit_max;		
	}	
}

function check_date(){
	var d = new Date();
	info.cycle_start_date = document.form.tl_cycle.value;	// the cycle of start date
	info.current_year = d.getFullYear();	// today's year
	info.current_month = d.getMonth()+1;	// today's month, return 0 ~ 11, so needed to plus 1
	info.current_date = d.getDate();	// today's date
	var date_string =  info.current_month + " " + info.current_date + " " + info.current_year;		
	info.current_timestamp = Date.parse(date_string);	//to get today's timestamp, format: 'Month Date Year'

	var timestamp = info.current_timestamp;
	var flag = 1;
	var current_date = info.current_date;
	var current_month = "";
	
	while(flag){		//to get
		if(info.cycle_start_date != current_date){
			timestamp = timestamp - 86400000;
			d.setTime(timestamp);		// let d set to previous day
			current_date = d.getDate();
			if(info.cycle_start_date > 28){		// to checke date 31th and Feb 29th
				if(info.current_month != d.getMonth()+1){
					if(info.cycle_start_date > current_date){
						timestamp = timestamp + 86400000;
						break;					
					}			
				}
			}		
		}
		else{
			flag = 0;
		}
	}
	
	d.setTime(timestamp);
	info.period1.start_timestamp = timestamp;
	info.period1.start_year = d.getFullYear();
	info.period1.start_month = d.getMonth() + 1;
	info.period1.start_date = d.getDate();
	info.period2.end_timestamp = timestamp;

	timestamp = timestamp - 86400000;
	d.setTime(timestamp);
	info.period2.end_year = d.getFullYear();
	info.period2.end_month = d.getMonth() + 1;
	info.period2.end_date = d.getDate();	
	current_date = d.getDate();
	
	flag = 1;
	while(flag){
		if(info.cycle_start_date != current_date){
			timestamp = timestamp - 86400000;
			d.setTime(timestamp);
			current_date = d.getDate();
			current_month = d.getMonth()+1;
			if(info.cycle_start_date > 28){		// to checke date 31th and Feb 29th
				if(current_month != d.getMonth()+1){
					if(info.cycle_start_date > current_date){
						timestamp = timestamp + 86400000;
						break;					
					}			
				}
			}			
		}
		else{
			flag = 0;
		}
	}
	
	info.period2.start_timestamp = timestamp;
	info.period2.start_year = d.getFullYear();
	info.period2.start_month = d.getMonth() + 1;
	info.period2.start_date = d.getDate();	
	
	flag = 1;
	timestamp = info.period1.start_timestamp + 86400000;
	d.setTime(timestamp);
	current_date = d.getDate();
	while(flag){
		if(info.cycle_start_date != current_date){
			timestamp = timestamp + 86400000;
			d.setTime(timestamp);
			current_date = d.getDate();			
		}
		else{
			flag = 0;
		}
	}

	info.period1.end_timestamp = timestamp;
	d.setTime(timestamp - 86400000);
	info.period1.end_year = d.getFullYear();
	info.period1.end_month = d.getMonth() + 1;
	info.period1.end_date = d.getDate();
	
	calculate_period();
	calculate_data(info, "p1", info.current_wan.ifname);
	
	document.timeForm.tl_date_start.value = info.period1.start_timestamp/1000;
	document.timeForm.submit();
	return true;
}

function calculate_data(obj, stage, ifname){
	var start_timestamp = 0;
	var end_flag = 0;
	if(ifname == null)
		ifname = info.primary_wan.ifname;
	
	if(stage == "p1"){
		start_timestamp = obj.period1.start_timestamp/1000;
		end_flag = obj.period1.end_timestamp/1000;	
	}
	else{
		start_timestamp = obj.period2.start_timestamp/1000;
		end_flag = obj.period2.end_timestamp/1000;		
	}

	var end_timestamp = start_timestamp + 86400;
	var index = 0
	total_traffic_array = new Array();
	while(start_timestamp != end_flag){		
		getWanData(ifname, start_timestamp, end_timestamp, info.current_wan_unit, index);
		start_timestamp = end_timestamp ;
		end_timestamp += 86400;
		index++;
	}

	if(stage == "p1"){
		setTimeout("render_bar('p1')",1000);
		document.timeForm.tl_date_start.value = info.period1.start_timestamp/1000;
		document.timeForm.submit();
	}	
	else{
		setTimeout("render_bar('p2')",1000);
		document.timeForm.tl_date_start.value = info.period2.start_timestamp/1000;
		document.timeForm.submit();
	}	
}

function calculate_period(){
	var code = "";
	
	code += "<option value='p1'>"+ info.period1.start_month + "/" + info.period1.start_date +  " ~ " + info.period1.end_month + "/" + info.period1.end_date + "</option>";
	code += "<option value='p2'>"+ info.period2.start_month + "/" + info.period2.start_date +  " ~ " + info.period2.end_month + "/" + info.period2.end_date + "</option>";
	$("#period_option").html(code);
}

var total_traffic_array = new Array();
//var total_traffic = 0;
function getWanData(ifname, start, end, unit, idx){
	$.ajax({
		url: '/ajax/getTrafficLimiter.asp?ifname=' +ifname+ '&start=' + start + '&end=' + end + ' &unit='+ unit,
		dataType: 'script',	
		error: function(xhr) {
			setTimeout("getWanData(ifname, start, end, unit, idx);", 1000);
		},
		success: function(response){
			total_traffic_array[idx] = traffic[0];
		}
	});
}

function apply(){
	if(document.form.tl_limit_max.value.split(".").length > 2){
		alert("Wrong Format of Limit Value!");
		document.form.tl_limit_max.focus();
		return false;
	}
	
	if(document.form.tl_alert_max.value.split(".").length > 2){
		alert("Wrong Format of Alert Value!");
		document.form.tl_alert_max.focus();
		return false;
	}
	
	if(document.form.tl_limit_enable.checked){
		if(document.form.tl_limit_max.value < 0){
			alert("The Limit value can't be minus!");
			document.form.tl_limit_max.focus();
			return false;			
		}
	}
	
	if(document.form.tl_alert_enable.checked){
		if(document.form.tl_alert_enable.value < 0){
			alert("The Alert value can't be minus!");
			document.form.tl_alert_enable.focus();
			return false;			
		}
	}

	if(document.form.tl_limit_enable.checked && document.form.tl_alert_enable.checked){
		if(parseFloat(document.form.tl_limit_max.value) < parseFloat(document.form.tl_alert_max.value)){
			alert("The Limit value can't less than Alert value!");
			document.form.tl_limit_max.focus();
			return false;	
		}
	}

	if(info.current_wan_unit == 0){
		document.form.tl0_limit_enable.value = document.form.tl_limit_enable.value;
		document.form.tl0_limit_max.value = document.form.tl_limit_max.value;
		document.form.tl0_alert_enable.value = document.form.tl_alert_enable.value;
		document.form.tl0_alert_max.value = document.form.tl_alert_max.value;
	}
	else{
		document.form.tl1_limit_enable.value = document.form.tl_limit_enable.value;
		document.form.tl1_limit_max.value = document.form.tl_limit_max.value;
		document.form.tl1_alert_enable.value = document.form.tl_alert_enable.value;
		document.form.tl1_alert_max.value = document.form.tl_alert_max.value;	
	}

	document.form.submit();
}
function show_alert_preference(){
	cal_panel_block("alert_preference", 0.25);
	check_smtp_server_type();
	$('#alert_preference').fadeIn(300);
	document.getElementById('mail_address').value = document.form.PM_MY_EMAIL.value;
	document.getElementById('mail_password').value = document.form.PM_SMTP_AUTH_PASS.value;
}

function close_alert_preference(){
	$('#alert_preference').fadeOut(100);
	document.form.PM_SMTP_SERVER.disabled = true;
	document.form.PM_SMTP_PORT.disabled = true;
	document.form.PM_MY_EMAIL.disabled = true;
	document.form.PM_SMTP_AUTH_USER.disabled = true;
	document.form.PM_SMTP_AUTH_PASS.disabled = true;
}
var smtpList = new Array();
smtpList = [
	{smtpServer: "smtp.gmail.com", smtpPort: "587", smtpDomain: "gmail.com"},
	{smtpServer: "smtp.aol.com", smtpPort: "587", smtpDomain: "aol.com"},
	{smtpServer: "smtp.qq.com", smtpPort: "587", smtpDomain: "qq.com"},
	{smtpServer: "smtp.163.com", smtpPort: "25", smtpDomain: "163.com"},
	{end: 0}
];
function apply_alert_preference(){
	document.form.PM_SMTP_SERVER.disabled = false;
	document.form.PM_SMTP_PORT.disabled = false;
	document.form.PM_MY_EMAIL.disabled = false;
	document.form.PM_SMTP_AUTH_USER.disabled = false;
	document.form.PM_SMTP_AUTH_PASS.disabled = false;
	var address_temp = document.getElementById('mail_address').value;
	var account_temp = document.getElementById('mail_address').value.split("@");	
	var mail_bit = 0;
	var server_index = document.getElementById("mail_provider").value;

	if(address_temp.indexOf('@') != -1){	
		if(account_temp[1] != "gmail.com" && account_temp[1] != "aol.com" && account_temp[1] != "qq.com" && account_temp[1] != "163.com"){
			alert("Wrong mail domain");
			document.getElementById('mail_address').focus();
			return false;
		}
		
		if(document.form.PM_MY_EMAIL.value == "" || document.form.PM_MY_EMAIL.value != address_temp)
			document.form.action_script.value += ";reset_tl_count";
				
		document.form.PM_MY_EMAIL.value = address_temp;	
	}
	else{	
		if(document.form.PM_MY_EMAIL.value == "" || document.form.PM_MY_EMAIL.value != address_temp)
			document.form.action_script.value += ";reset_tl_count";
			
		document.form.PM_MY_EMAIL.value = account_temp[0] + "@" +smtpList[server_index].smtpDomain;
	}

	document.form.PM_SMTP_AUTH_USER.value = account_temp[0];
	document.form.PM_SMTP_AUTH_PASS.value = document.getElementById('mail_password').value;	
	document.form.PM_SMTP_SERVER.value = smtpList[server_index].smtpServer;	
	document.form.PM_SMTP_PORT.value = smtpList[server_index].smtpPort;	
	$('#alert_preference').fadeOut(100);
	document.form.submit();
}


function check_smtp_server_type(){
	for(i = 0;i < smtpList.length; i++){
		if(smtpList[i].smtpServer == document.form.PM_SMTP_SERVER.value){
			document.getElementById("mail_provider").value = i;
			break;
		}
	}
}

function erase_traffic(){
	if(confirm("Are you sure you want to clear data? ")){
		document.form.action = "apply.cgi?interface=" + info.current_wan.ifname;
		document.form.action_mode.value = "traffic_resetcount";
		document.form.submit();
		location.href = "AdaptiveQoS_TrafficLimiter.asp";
	}

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
			document.getElementById("timezone_hint").style.visibility = "visible";
			document.getElementById("timezone_hint").innerHTML = "* <#LANHostConfig_x_TimeZone_itemhint#>";
		}
		else
			return;			
	}
	else
		return;	
}

function handle_value(){
	if(document.form.tl_limit_enable.checked && document.form.tl_alert_enable.checked){
		if(parseFloat(document.form.tl_alert_max.value) > parseFloat(document.form.tl_limit_max.value)){
			alert("The Limit value can't be less than the Alert value");
				return false;	
		}
	}
	
	if($("#period_option")[0][0].selected){
		render_bar("p1");
	}
	else{
		render_bar("p2");
	}	
}
</script>
<style>
#drag_line_limit_div::after {
  content: "";
  border-radius: 50%;
  width: 18px;
  height: 18px;
  display: block;
  position: absolute;
  right: -16px;
  margin-top: -9px;
  background: #B71C1C;
}
#drag_line_alert_div::after {
  content: "";
  border-radius: 50%;
  width: 18px;
  height: 18px;
  display: block;
  position: absolute;
  right: -16px;
  margin-top: -9px;
  background: #F57F17;
}
.bar{
	background: #2196F3;
}
.bar:hover {
  background: #90CAF9;
  cursor: pointer;
}
.bar:hover:before {
  color: white;
  content: attr(data-date) attr(data-value);
  position: relative;
  bottom: 45px;
  left: -15px;
  z-index: 9;
  font-size:16px;
}
.alertpreference{
	width:650px;
	height:260px;
	position:absolute;
	background: #000;
	z-index:10;
	margin-left:260px;
	border-radius:10px;
	padding:15px 10px 20px 10px;
	display: none;
}
</style>
</head>
<body>
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/AdaptiveQoS_TrafficLimiter.asp">
<input type="hidden" name="next_page" value="/AdaptiveQoS_TrafficLimiter.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reset_traffic_limiter">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="tl_enable" value="<% nvram_get("tl_enable"); %>">
<input type="hidden" name="tl0_alert_enable" value="<% nvram_get("tl0_alert_enable"); %>">
<input type="hidden" name="tl0_alert_max" value="<% nvram_get("tl0_alert_max"); %>">
<input type="hidden" name="tl1_alert_enable" value="<% nvram_get("tl1_alert_enable"); %>">
<input type="hidden" name="tl1_alert_max" value="<% nvram_get("tl1_alert_max"); %>">
<input type="hidden" name="tl0_limit_enable" value="<% nvram_get("tl0_limit_enable"); %>">
<input type="hidden" name="tl0_limit_max" value="<% nvram_get("tl0_limit_max"); %>">
<input type="hidden" name="tl1_limit_enable" value="<% nvram_get("tl1_limit_enable"); %>">
<input type="hidden" name="tl1_limit_max" value="<% nvram_get("tl1_limit_max"); %>">
<input type="hidden" name="PM_SMTP_SERVER" value="<% nvram_get("PM_SMTP_SERVER"); %>" disabled>
<input type="hidden" name="PM_SMTP_PORT" value="<% nvram_get("PM_SMTP_PORT"); %>" disabled>
<input type="hidden" name="PM_MY_EMAIL" value="<% nvram_get("PM_MY_EMAIL"); %>" disabled>
<input type="hidden" name="PM_SMTP_AUTH_USER" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>" disabled>
<input type="hidden" name="PM_SMTP_AUTH_PASS" value="" disabled>

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
			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle" style="height:820px;">
				<tr>
					<td bgcolor="#4D595D" valign="top">
					<div style="display:table;width:100%;">
						<div style="display:table-cell;font-size:14px;font-weight:bolder;text-shadow:1px 1px 0px #000;padding:10px;width:85%">Traffic Analyzer - Traffic Limiter</div>
						<div style="display:table-cell">
							<input class="button_gen_long" type="button" onclick="show_alert_preference();" value="Notification">
						</div>
					</div>
						<div><img src="images/New_ui/export/line_export.png" /></div>
						<div style="line-height:20px;font-size:14px;padding:10px;color:#DBDBDB">
							Traffic Limiter allows you to monitor your networking traffic and set limited monthly traffic usage. You can receive notification if traffic reach alert traffic and limit you access to Internet while it get max.
							<div>*Carrier data accounting may differ from your device</div>
						</div>
						<div style="display:table;width:100%;padding:10px">
							<div style="padding:10px;display:table-row">
								<div style="padding: 10px 0;font-size:14px;line-height:15px;width:200px;display:table-cell;vertical-align:middle;color:#DBDBDB">Enable Traffic Limiter</div>
								<div style="width:75px;">
									<div class="left" style="cursor:pointer;display:table-cell;" id="traffic_limiter_enable"></div>
									<script type="text/javascript">
										$('#traffic_limiter_enable').iphoneSwitch('<% nvram_get("tl_enable"); %>', 
											function() {
												document.form.tl_enable.value = 1;
												document.form.submit();
											},
											function() {
												document.form.tl_enable.value = 0;
												document.form.submit();
											}
										);
									</script>
								</div>
							</div>
							<div style="display:table-row;visibility:hidden" id="time_field">
								<div style="padding: 10px 0;font-size:14px;line-height:15px;width:200px;display:table-cell;vertical-align:middle;color:#DBDBDB"><#General_x_SystemTime_itemname#></div>
								<div style="display:table-cell;font-size:14px;vertical-align: middle;font-weight:bold;">
									<div id="system_time" name="system_time"></div>
								</div>
							</div>
							<div style="display:table-row;">
								<div style="display:table-cell;width:200px;"></div>
								<div id="timezone_hint" style="font-size:14px;display:table-cell;margin-left:70px;color:#FC0;visibility:hidden"><#General_x_SystemTime_dst#></div>
							</div>
						</div>			
<!--outside border-->	
<div style="padding:10px;" id="main_field">						
<div style="border:1px solid #FFF;padding:25px 5px 25px 5px;border-radius:3px;">
										<div id="wan_tab" style="display:table-cell;width:400px;position:absolute;margin: -40px 0 0 180px;">
											<div id="primary_div" style="width:173px;height:30px;border-top-left-radius:8px;border-bottom-left-radius:8px;" class="block_filter_pressed" >
												<table id="primary_tab" class="block_filter_name_table_pressed" onclick="switch_wan(this)"><tr><td style="line-height:13px;">Primary WAN</td></tr></table>
											</div>
											<div id="secondary_div" style="width:172px;height:30px;margin:-32px 0px 0px 173px;border-top-right-radius:8px;border-bottom-right-radius:8px;" class="block_filter">
												<table id="secondary_tab" class="block_filter_name_table" onclick="switch_wan(this)"><tr><td style="line-height:13px;">Secondary WAN</td></tr></table>
											</div>
										</div>
													
						<div style="padding:5px 10px;">
							<div style="font-size:14px;line-height:15px;display:table-cell;vertical-align:middle;width:180px;color:#DBDBDB">WAN Type</div>
							<div style="display:table-cell;padding-left:20px;font-size:14px;font-weight:bold;" id="wan_type"></div>
						</div>						
						<div style="padding:5px 10px;">
							<div style="font-size:14px;line-height:15px;display:table-cell;vertical-align:middle;width:180px;color:#DBDBDB">Start Date Per Month</div>
							<div style="display:table-cell;padding-left:20px;">
								<select class="input_option" id="tl_cycle" name="tl_cycle" onchange="check_date();"></select>							
							</div>
						</div>						
					
						<div style="padding:5px 10px;height:30px;">					
							<div style="font-size:14px;display:table-cell;width:180px;color:#DBDBDB;vertical-align:middle;height:25px;">
								<a class="hintstyle" href="javascript:void(0);" onclick="openHint(30, 1);" style="color:#DBDBDB">Send Alert</a>
							</div>
							<div style="font-size:14px;display:table-cell;padding-left:20px;color:#DBDBDB;vertical-align:middle;">								
								<input id="tl_alert_enable" name="tl_alert_enable" type="checkbox" onclick="alert_control(this.checked)" style="vertical-align:middle">
								<label for="tl_alert_enable">Enable</label>								
							</div>
							<div id="alert_value_field" style="display:table-cell;padding-left:20px;font-size:14px;color:#DBDBDB">							
								<div style="display:table-cell;width:35px;vertical-align:middle;">Alert</div>
								<div style="display:table-cell;padding: 0 10px;vertical-align:middle;">
									<input name="tl_alert_max" type="text" value="" class="input_6_table" maxlength="7" onkeypress="return validator.isNumberFloat(this,event);" onkeyup="handle_value(document.form.tl_limit_max.value, document.form.tl_alert_max.value);">						
								</div>
								<div style="display:table-cell;vertical-align:middle">GB</div>	
							</div>							
						</div>					
						<div style="padding:5px 10px;height:30px;">
							<div style="font-size:14px;display:table-cell;width:180px;color:#DBDBDB;vertical-align:middle;height:25px;">
								<a class="hintstyle" href="javascript:void(0);" onclick="openHint(30, 2);" style="color:#DBDBDB">Cut-Off Internet</a>
							</div>							
							<div style="display:table-cell;padding-left:20px;color:#DBDBDB;font-size:14px;vertical-align:middle;">
								<input type="checkbox" id="tl_limit_enable" name="tl_limit_enable" onclick="limit_control(this.checked)" style="vertical-align:middle">
								<label for="tl_limit_enable">Enable</label>
							</div>
							<div id="limit_value_field" style="display:table-cell;padding-left:20px;font-size:14px;color:#DBDBDB">
								<div style="display:table-cell;width:35px;vertical-align:middle;">Limit</div>
								<div style="display:table-cell;padding: 0 10px;vertical-align:middle;">
									<input name="tl_limit_max" type="text" value="" class="input_6_table" maxlength="7" onkeypress="return validator.isNumberFloat(this,event);" onkeyup="handle_value(document.form.tl_limit_max.value, document.form.tl_alert_max.value);">						
								</div>
								<div style="display:table-cell;vertical-align:middle">GB</div>
							</div>
						</div>
						
						<div style="padding:5px 10px;">
							<div style="font-size:14px;display:table-cell;vertical-align:middle;width:180px;color:#DBDBDB">Reset Traffic</div>
							<div style="display:table-cell;padding-left:20px;">
								<input class="button_gen" type="button" onclick="erase_traffic();" value="<#CTL_Reset_OOB#>">
							</div>
						</div>	
						<div>
							<div style="width:680px;height:85px;background-color:#37474F;margin-left:10px;">								
								<div style="display:table;width:100%;padding-top:20px;margin-left:50px;">
									<div style="display:table-row">									
										<div style="display:table-cell;width:50%">
											<div style="display:table-cell;width:100px;text-align:right;padding:0 10px;font-size:16px;color:#DBDBDB">Total Traffic</div>
											<div style="display:table-cell">
												<div style="display:table-cell;width:110px;text-align:right;font-size:20px;color:#2196F3;font-weight:bold" id="total_traffic"></div>
											</div>
										</div>
										<div style="display:table-cell;width:50%">
											<div style="display:table-cell;width:100px;text-align:right;padding:0 10px;font-size:16px;color:#DBDBDB">Period</div>
											<div style="display:table-cell">
												<select id="period_option" class="input_option" style="font-size: 16px;"onchange="calculate_data(info, this.value, info.current_wan.ifname);"></select>
											</div>
										</div>
									</div>
								</div>					
							</div>						
									<div style="width: 680px; height: 330px; margin-left:10px; display: block;color:#FFF;background-color:#37474F;">
										<div id="scale" style="display:inline-block; width: 50px; height: 100%;float:left;padding-left:12px;">
											<div style="height:10px;border-bottom:1px solid transparent;position:relative;text-align:right">
												<span style="position:absolute;bottom:0;right:5px;width:100px">100 MB</span>
											</div>
											<div style="height:74px;border-bottom:1px solid transparent;position:relative;text-align:right">
												<span style="position:absolute;bottom:0;right:5px;width:100px">75 MB</span>
											</div>
											<div style="height:74px;border-bottom:1px solid transparent;position:relative;text-align:right">
												<span style="position:absolute;bottom:0;right:5px;width:100px">50 MB</span>
											</div>
											<div style="height:74px;border-bottom:1px solid transparent;position:relative;text-align:right">
												<span style="position:absolute;bottom:0;right:5px;width:100px">25 MB</span>
											</div>
											<div style="height:74px;border-bottom:1px solid transparent;position:relative;text-align:right">
												<span style="position:absolute;bottom:0;right:5px;width:100px">0 MB</span>
											</div>								
										</div>
										
										<div id="bar" style="display:inline-block; width: 610px; height: 300px;border-bottom:1px solid #9E9E9E;border-left:1px solid #9E9E9E;"></div>
									</div>
						</div>	
</div>
</div>
<!--outside border-->						
						<div id="apply_div" style=" *width:136px;margin:10px 0 10px 300px;" class="titlebtn" align="center" onClick="apply();"><span><#CTL_apply#></span></div>		
					</td>  
				</tr>
			</table>
		<!--===================================End of Main Content===========================================-->
		</td>	
	</tr>
</table>
<div id="alert_preference" class="alertpreference" style="margin-top:-750px;">
	<table style="width:99%">
		<tr>
			<th>
				<div style="font-size:16px;"><#AiProtection_alert_pref#></div>		
			</th>		
		</tr>
			<td>
				<div class="formfontdesc" style="font-style: italic;font-size: 14px;"><#AiProtection_HomeDesc1#></div>
			</td>
		<tr>
			<td>
				<table class="FormTable" width="99%" border="1" align="center" cellpadding="4" cellspacing="0">
					<tr>
						<th><#Provider#></th>
						<td>
							<div>
								<select class="input_option" id="mail_provider">
									<option value="0">Google</option>							
									<option value="1">AOL</option>							
									<option value="2">QQ</option>							
									<option value="3">163</option>							
								</select>
							</div>
						</td>
					</tr>
					<!--tr>
						<th>Administrator</th>
						<td>
							<div>
								<input type="type" class="input_20_table" name="user_name" value="">
							</div>
						</td>
					</tr-->
					<tr>
						<th>Email</th>
						<td>
							<div>
								<input type="type" class="input_30_table" id="mail_address" value="">
							</div>
						</td>
					</tr>
					<tr>
						<th><#HSDPAConfig_Password_itemname#></th>
						<td>
							<div>
								<input type="password" class="input_30_table" id="mail_password" maxlength="100" value="" autocorrect="off" autocapitalize="off">
							</div>
						</td>
					</tr>
				</table>
			</td>
		</tr>
		<tr>
			<td>
				<div style="text-align:center;margin-top:20px;">
					<input class="button_gen" type="button" onclick="close_alert_preference();" value="<#CTL_close#>">
					<input class="button_gen_long" type="button" onclick="apply_alert_preference();" value="<#CTL_apply#>">
				</div>
			</td>		
		</tr>
	</table>
</div>
<div id="footer"></div>
</form>
<form method="post" name="timeForm" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="tl_date_start" value="">
</form>
</body>
</html>
