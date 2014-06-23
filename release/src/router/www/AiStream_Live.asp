<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE9"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="/device-map/device-map.css" />
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style type="text/css">
.splitLine{
	background-image: url('/images/New_ui/export/line_export.png');
	background-repeat: no-repeat;
	height: 3px;
	width: 100%;
	margin-bottom: 7px;
}
#sortable div table tr:hover{
	cursor: pointer;
	color: #000;
	background-color: #66777D;
	font-weight: bolder;
}
#sortable div table{
	font-family:Verdana;
	width: 100%;
	border-spacing: 0px;
}
.trafficIcons{
	width:56px;
	height:56px;
	background-image:url('/images/New_ui/networkmap/client-list.png');
	background-repeat:no-repeat;
	border-radius:10px;
	margin-left:10px;
	background-position:50% 61.10%;
}
.trafficIcons_clicked{
	width:56px;
	height:56px;
	background-image:url('/images/New_ui/networkmap/client-list.png');
	background-repeat:no-repeat;
	border-radius:10px;
	margin-left:10px;
	background-position:50% 64.40%;
}

.qosLevel{
	background-color: inherit;
}
.qosLevel0{
	background-color: blue;
}
.qosLevel1{
	background-color: #0072E3;
}
.qosLevel2{
	background-color: #46A3FF;
}
.qosLevel3{
	background-color: #97CBFF;
}
.qosLevel4{
	background-color: #D2E9FF;
}

#indicator_upload, #indicator_download{
	-webkit-transform:rotate(-123deg);
    -moz-transform:rotate(-123deg);
    -o-transform:rotate(-123deg);
    msTransform:rotate(-123deg);
    transform:rotate(-123deg);
}

</style>
<script>
// disable auto log out
AUTOLOGOUT_MAX_MINUTE = 0;

var $j = jQuery.noConflict();
window.onresize = cal_agreement_block;
var client_list_array = '<% get_client_detail_info(); %>';
var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var qos_rulelist = "<% nvram_get("qos_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");

function register_event(){
	$j(function() {
		$j( "#sortable" ).sortable();
		$j( "#sortable" ).disableSelection();
		$j("#priority_block div").draggable({helper:"clone",revert:true,revertDuration:10}); 
		$j("[id^='icon_']").droppable({
			drop: function(event,ui){
				this.style.backgroundColor = event.toElement.style.backgroundColor;
				regen_qos_rule(this, ui.draggable[0].id);				
			}
		});
	});
} 

function initial(){
	show_menu();
	show_clients();
	register_event();
	//redraw_unit();
}

var download_maximum = 100 * 1024;
var upload_maximum = 100 * 1024;
function redraw_unit(){
	var upload_node = $('upload_unit').children;
	var download_node = $('download_unit').children;
	var upload = upload_maximum/1024;
	var download = download_maximum/1024;
	
	for(i=0;i<5;i++){   // length could be $('upload_unit').children.length -2
		if(i == 4){
			if(upload < 5){
				if(upload > parseInt(upload)){
					if(upload > (parseInt(upload) + 0.5))
						upload_node[i].innerHTML =  parseInt(upload) + 1;
					else
						upload_node[i].innerHTML = parseInt(upload) + 0.5;					
				}	
				else
					upload_node[i].innerHTML = parseInt(upload);	
			}
			else{			
				upload_node[i].innerHTML = Math.ceil(upload);
			}
			
			if(download < 5){
				if(download < parseInt(download)){
					if(download > (parseInt(download + 0.5)))
						download_node[i].innerHTML = parseInt(download) + 1;
					else	
						download_node[i].innerHTML = parseInt(download) + 0.5;
				}
				else
					download_node[i].innerHTML =  parseInt(download);	
			}
			else{
				download_node[i].innerHTML = Math.ceil(download);
			}			
		}
		else{
			if(upload < 5){
				upload_node[i].innerHTML =(upload*(i+1)/5).toFixed(1);
			}
			else{
				upload_node[i].innerHTML = Math.round(upload*(i+1)/5);
			}
			
			if(download < 5){
				download_node[i].innerHTML = (download*(i+1)/5).toFixed(1);	
			}
			else{
				download_node[i].innerHTML = Math.round(download*(i+1)/5);
			}			
		}	
	}
}

// for speedmeter 
router_traffic_old = new Array();
function calculate_router_traffic(traffic){
	router_traffic_new = new Array();
	router_traffic_new = traffic;
	var tx = router_traffic_new[0] - router_traffic_old[0];
	var rx = router_traffic_new[1] - router_traffic_old[1];
	tx = tx*8/2;		// translate to bits
	rx = rx*8/2;
	var tx_kb = tx/1024;
	var rx_kb = rx/1024;
	var tx_mb = tx/1024/1024;
	var rx_mb = rx/1024/1024;
	var angle = 0;
	var rotate = "";
/* angle mapping table: [0M: -123deg, 1M: -90deg, 5M: -58deg, 10M: -33deg, 20M: -1deg, 30M: 30deg, 50M: 58deg, 75M: 88deg, 100M: 122deg]*/

	
	/*if(tx_kb > upload_maximum){
		upload_maximun = tx_kb;
		redraw_unit();
	}	

	if(rx_kb > download_maximum){
		download_maximum = rx_kb;
		redraw_unit();
	}*/	

	if(router_traffic_old.length != 0){
		/*if((tx/1024) < 1024){
			document.getElementById('upload_speed').innerHTML = tx_kb.toFixed(1);
			//document.getElementById('upload_speed_unit').innerHTML = "KB";
		}
		else{
			document.getElementById('upload_speed').innerHTML = tx_mb.toFixed(1);
			//document.getElementById('upload_speed_unit').innerHTML = "MB";	
		}*/
		
		//angle = (tx_mb - lower unit)/(upper unit - lower unit)*(degree in the range) + (degree of previous scale) + (degree of 0M)
		document.getElementById('upload_speed').innerHTML = tx_mb.toFixed(2);
		if(tx_mb <= 1){
			angle = (tx_mb*33) + (-123);
		}
		else if(tx_mb > 1 && tx_mb <= 5){
			angle = ((tx_mb-1)/4)*32 + 33 +(-123);
					
		}
		else if(tx_mb > 5 && tx_mb <= 10){
			angle = ((tx_mb - 5)/5)*25 + (33 + 32) + (-123);
		}
		else if(tx_mb > 10 && tx_mb <= 20){
			angle = ((tx_mb - 10)/10)*32 + (33 + 32 + 25) + (-123)
		}
		else if(tx_mb > 20 && tx_mb <= 30){
			angle = ((tx_mb - 20)/10)*31 + (33 + 32 + 25 + 32) + (-123);	
		}
		else if(tx_mb > 30 && tx_mb <= 50){
			angle = ((tx_mb - 30)/20)*28 + (33 + 32 + 25 + 32 + 31) + (-123);	
		}
		else if(tx_mb > 50 && tx_mb <= 75){
			angle = ((tx_mb - 50)/25)*30 + (33 + 32 + 25 + 32 + 31 + 28) + (-123);
		}
		else if(tx_mb >75 && tx_mb <= 100){
			angle = ((tx_mb - 75)/25)*34 + 	(33 + 32 + 25 + 32 + 31 + 28 + 30) + (-123);
		}
		else{		// exception case temporally, upload traffic exceed 100Mb.
			angle = 123;		
		}
		
		/*if(tx_kb < upload_maximum/5){		
			angle = (tx_kb/(upload_maximum/5))*0.3*260 + (-130);
		}
		else if((tx_kb >= upload_maximum/5) && (tx_kb < upload_maximum*2/5)){
			angle = ((tx_kb - (upload_maximum/5))/(upload_maximum/5))*0.25*260 + (260*0.3) + (-130);	
		}
		else if((tx_kb >= upload_maximum*2/5) && (tx_kb < upload_maximum*3/5)){
			angle = ((tx_kb - (upload_maximum*2/5))/(upload_maximum/5))*0.2*260 + (260*0.55) + (-130);
		}
		else if((tx_kb >= upload_maximum*3/5) && (tx_kb < upload_maximum*4/5)){
			angle = ((tx_kb - (upload_maximum*3/5))/(upload_maximum/5))*0.15*260 + (260*0.75) + (-130);
		}
		else{
			angle = ((tx_kb - (upload_maximum*4/5))/(upload_maximum/5))*0.1*260 + (260*0.9) + (-130);		
		}*/

		rotate = "rotate("+angle.toFixed(1)+"deg)";
		$j('#indicator_upload').css({
		"-webkit-transform": rotate,
        "-moz-transform": rotate,
        "-o-transform": rotate,
        "msTransform": rotate,
        "transform": rotate
		});

		/*if(rx_kb < 1024){						
			document.getElementById('download_speed').innerHTML = rx_kb.toFixed(1);
			//document.getElementById('download_speed_unit').innerHTML = "KB";			
		}
		else{				
			document.getElementById('download_speed').innerHTML = rx_mb.toFixed(1);
			//document.getElementById('download_speed_unit').innerHTML = "MB";
		}*/	
		
		//angle = (rx_mb - lower unit)/(upper unit - lower unit)*(degree in the range) + (degree of previous scale) + (degree of 0M)
		document.getElementById('download_speed').innerHTML = rx_mb.toFixed(2);
		if(rx_mb <= 1){
			angle = (rx_mb*33) + (-123);
		}
		else if(rx_mb > 1 && rx_mb <= 5){
			angle = ((rx_mb-1)/4)*32 + 33 +(-123);
					
		}
		else if(rx_mb > 5 && rx_mb <= 10){
			angle = ((rx_mb - 5)/5)*25 + (33 + 32) + (-123);
		}
		else if(rx_mb > 10 && rx_mb <= 20){
			angle = ((rx_mb - 10)/10)*32 + (33 + 32 + 25) + (-123)
		}
		else if(rx_mb > 20 && rx_mb <= 30){
			angle = ((rx_mb - 20)/10)*31 + (33 + 32 + 25 + 32) + (-123);	
		}
		else if(rx_mb > 30 && rx_mb <= 50){
			angle = ((rx_mb - 30)/20)*28 + (33 + 32 + 25 + 32 + 31) + (-123);	
		}
		else if(rx_mb > 50 && rx_mb <= 75){
			angle = ((rx_mb - 50)/25)*30 + (33 + 32 + 25 + 32 + 31 + 28) + (-123);
		}
		else if(rx_mb >75 && rx_mb <= 100){
			angle = ((rx_mb - 75)/25)*34 + 	(33 + 32 + 25 + 32 + 31 + 28 + 30) + (-123);
		}
		else{		// exception case temporally, download traffic exceed 100Mb.
			angle = 123;		
		}
		
		/*if(rx_kb < download_maximum/5){		
			angle = (rx_kb/(download_maximum/5))*0.3*260 + (-130);
		}
		else if((rx_kb >= download_maximum/5) && (rx_kb < download_maximum*2/5)){
			angle = ((rx_kb - (download_maximum/5))/(download_maximum/5))*0.25*260 + (260*0.3) + (-130);
		}
		else if((rx_kb >= download_maximum*2/5) && (rx_kb < download_maximum*3/5)){
			angle = ((rx_kb - (download_maximum*2/5))/(download_maximum/5))*0.2*260 + (260*0.55) + (-130);
		}
		else if((rx_kb >= download_maximum*3/5) && (rx_kb < download_maximum*4/5)){
			angle = ((rx_kb - (download_maximum*3/5))/(download_maximum/5))*0.15*260 + (260*0.75) + (-130);
		}
		else{
			angle = ((rx_kb - (download_maximum*4/5))/(download_maximum/5))*0.1*260 + (260*0.9) + (-130);		
		}*/

		rotate = "rotate("+angle.toFixed(1)+"deg)";
		$j('#indicator_download').css({
		"-webkit-transform": rotate,
        "-moz-transform": rotate,
        "-o-transform": rotate,
        "msTransform": rotate,
        "transform": rotate
		});
	}	
	
	
	router_traffic_old = [];
	router_traffic_old = router_traffic_new;
}

function show_clients(){
	var code = "";

	if(clientList.length == 0){
		setTimeout("show_clients();", 500);
		return false;
	}
	
	for(i=0; i<clientList.length; i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.isGateway || !clientObj.isOnline)
			continue;
		
		code += '<div>';
		code += '<table><tr>';
		code += '<td style="width:70px;">';		
		code += '<div id="icon_'+i+'" onclick="show_apps(this);" class="closed trafficIcons type' + clientObj.type + ' qosLevel' + clientObj.qosLevel + '"></div>';			
		code += '</td>';
		code += '<td style="width:180px;">';
		code += '<div style="font-family:Courier New,Courier,mono;" title="' + clientObj.mac + '">'+ clientObj.name +'</div>';		
		code += '</td>';		
		code += '<td><div><table>';
		code += '<tr>';
		code += '<td style="width:385px">';
		code += '<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">';	
		code += '<div id="'+clientObj.mac+'_upload_bar" style="width:0%;background-color:#93E7FF;height:8px;black;border-radius:5px;"></div>';
		code += '</div>';
		code += '</td>';
		code += '<td style="text-align:right;">';
		code += '<div id="'+clientObj.mac+'_upload" style="width:45px;">0.0</div>';
		code += '</td>';
		code += '<td style="width:20px;">';
		code += '<div id="'+clientObj.mac+'_upload_unit">Kb</div>';
		code += '</td>';
		code += '<td style="width:20px;">';
		code += '<div style="width:0;height:0;border-width:0 7px 10px 7px;border-style:solid;border-color:#444F53 #444F53 #93E7FF #444F53;"></div>';
		code += '</td>';		
		code += '</tr>';			
		code += '<tr>';
		code += '<td>';
		code +=	'<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">';
		code += '<div id="'+clientObj.mac+'_download_bar" style="width:0%;background-color:#93E7FF;height:8px;black;border-radius:5px;"></div>';
		code +=	'</div>';
		code += '</td>';
		code += '<td style="text-align:right;">';
		code += '<div id="'+clientObj.mac+'_download" style="width:45px;">0.0</div>';
		code += '</td>';
		code += '<td style="width:20px;">';
		code += '<div id="'+clientObj.mac+'_download_unit" >Kb</div>';	
		code += '</td>';
		code += '<td style="width:20px;">';
		code += '<div style="width:0;height:0;border-width:10px 7px 0 7px;border-style:solid;border-color:#93E7FF #444F53 #444F53 #444F53;"></div>';
		code += '</td>';	
		code += '</tr>';
		code += '</table></div></td>';
		code += '</tr></table>';
		code += '<div class="splitLine"></div>';
		code += '</div>';
	}

	$('sortable').innerHTML = code;
	update_device_tarffic();
}

var previous_click_device = new Object();
function show_apps(obj){
	var parent_obj = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var parent_obj_temp = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var client_mac = obj.parentNode.parentNode.childNodes[1].firstChild.title;
	var clientObj = clientList[client_mac];
	var code = "";

	if(document.form.TM_EULA.value == 0){
		document.form.TM_EULA.value = 1;
		dr_advise();
		cal_agreement_block();
		$j("#agreement_panel").fadeIn(300);
		return false;
	}
	
	if(obj.className.indexOf("closed") == -1){			//close device's APPs
		clearTimeout(apps_time_flag);
		apps_time_flag = "";
		apps_traffic_old = [];
		var first_element = parent_obj_temp.firstChild;
		if(first_element.nodeName == "#text"){	
			parent_obj_temp.removeChild(first_element);
			first_element = parent_obj_temp.firstChild;
		}
		
		var last_element = parent_obj.lastChild;
		if(last_element.nodeName == "#text"){
			parent_obj.removeChild(last_element);
			last_element = parent_obj.lastChild;
		}
		
		$j(parent_obj_temp).empty();
		parent_obj_temp.appendChild(first_element);
		parent_obj_temp.appendChild(last_element);
		obj.setAttribute("class", "closed trafficIcons type" + clientObj.type + " qosLevel" + clientObj.qosLevel);
	}
	else{
		if(apps_time_flag != ""){
			cancel_previous_device_apps(previous_click_device);
			previous_click_device = obj;
		}
		else{			
			previous_click_device = obj;	
		}
	
		var last_element = parent_obj.lastChild;
		if(last_element.nodeName == "#text"){
			parent_obj.removeChild(last_element);
			last_element = parent_obj.lastChild;
		}
			
		var new_element = document.createElement("table");
		parent_obj.removeChild(last_element);
		parent_obj.appendChild(new_element);
		parent_obj.appendChild(last_element);
		obj.setAttribute("class", "opened trafficIcons_clicked type" + clientObj.type + "_clicked qosLevel" + clientObj.qosLevel);
		update_apps_tarffic(client_mac, obj, new_element);		
	}	
}

function cancel_previous_device_apps(obj){
	var parent_obj = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var parent_obj_temp = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var client_mac = obj.parentNode.parentNode.childNodes[1].firstChild.title;
	var clientObj = clientList[client_mac];

	clearTimeout(apps_time_flag);
	apps_time_flag = "";
	apps_traffic_old = [];
	var first_element = parent_obj_temp.firstChild;
	if(first_element.nodeName == "#text"){	
		parent_obj_temp.removeChild(first_element);
		first_element = parent_obj_temp.firstChild;
	}
	var last_element = parent_obj.lastChild;
	if(last_element.nodeName == "#text"){
		parent_obj.removeChild(last_element);
		last_element = parent_obj.lastChild;
	}
	
	$j(parent_obj_temp).empty();
	parent_obj_temp.appendChild(first_element);
	parent_obj_temp.appendChild(last_element);
	obj.setAttribute("class", "closed trafficIcons type" + clientObj.type + " qosLevel" + clientObj.qosLevel);
}

function render_apps(apps_array, obj_icon, apps_field){
	var code = "";
	
	for(i=0;i<apps_array.length;i++){
		code +='<tr>';
		code +='<td style="width:70px;">';
		code +='<div style="width:40px;height:40px;background-image:url(\'../images/New_ui/networkmap/client-list.png\');background-repeat:no-repeat;background-position:52% -10px;background-color:#1F2D35;border-radius:7px;margin-left:45px;background-size:67px"></div>';
		code +='</td>';
		code +='<td style="width:230px;border-top:1px dotted #333;">';
		code +='<div id="'+ apps_array[i][0] +'" style="font-family:Courier New,Courier,mono">'+apps_array[i][0]+'</div>';
		code +='</td>';

		code +='<td style="border-top:1px dotted #333;">';
		code +='<div style="margin-left:15px;">';
		code +='<table>';
		code +='<tr>';
		code +='<td style="width:305px">';
		code +='<div style="height:6px;padding:3px;background-color:#000;border-radius:10px;">';
		code +='<div id="'+apps_array[i][0]+'_upload_bar" style="width:0%;background-color:#93E7FF;height:6px;black;border-radius:5px;"></div>';
		code +='</div>';
		code +='</td>';
		code +='<td style="text-align:right;">';
		code +='<div id="'+apps_array[i][0]+'_upload">0.0</div>';
		code +=	'</td>';
		code +='<td style="width:30px;">';
		code +='<div id="'+apps_array[i][0]+'_upload_unit">Kb</div>';
		code +='</td>';															
		code +='</tr>';
		
		code +='<tr>';
		code +='<td>';
		code +='<div style="height:6px;padding:3px;background-color:#000;border-radius:10px;">';
		code +='<div id="'+apps_array[i][0]+'_download_bar" style="width:0%;background-color:#93E7FF;height:6px;black;border-radius:5px;"></div>';
		code +='</div>';
		code +='</td>';
		code +='<td style="text-align:right;">';
		code +='<div id="'+apps_array[i][0]+'_download">0.0</div>';
		code +='</td>';
		code +='<td style="width:30px;">';
		code +='<div id="'+apps_array[i][0]+'_download_unit">Kb</div>';
		code +='</td>';																	
		code +='</tr>';
		code +='</table>';
		code +='</div>';
		code +='</td>';
		code +='</tr>';		
	}

	if(code == ""){
		code = "<tr><td colspan='3' style='text-align:center;color:#FFCC00'><div style='padding:5px 0px;border-top:solid 1px #333;'>No Traffic in the list</div></td></tr>";
	}

	$j(apps_field).empty();
	$j(apps_field).append(code);
	calculate_apps_traffic(apps_array);
}

client_traffic_old = new Array();
function calculate_traffic(array_traffic){
	var client_traffic_new = new Array();
	var client_list_row = client_list_array.split('<');
	var client_list_col = new Array();
	
	for(i=0;i< array_traffic.length;i++){
		for(j=0;j<client_list_row.length;j++){
			client_list_col = client_list_row[j].split('>');
			if(client_list_col[3] == array_traffic[i][0]){
				client_traffic_new[i] = array_traffic[i][0];	
				client_traffic_new[array_traffic[i][0]] = {"tx":array_traffic[i][1], "rx":array_traffic[i][2]};								
			}	
		}	
	}
	
	for(i=0;i< client_traffic_new.length;i++){
		if(client_traffic_old.length != 0){
			var diff_tx = 0;
			var diff_rx = 0;
			var diff_tx_kb = 0;
			var diff_rx_kb = 0;
			var diff_tx_mb = 0;
			var diff_rx_mb = 0;
			var tx_width = 0;
			var rx_width = 0;
			
			diff_tx = (client_traffic_old[client_traffic_new[i]]) ? client_traffic_new[client_traffic_new[i]].tx - client_traffic_old[client_traffic_new[i]].tx : 0;
			diff_rx = (client_traffic_old[client_traffic_new[i]]) ? client_traffic_new[client_traffic_new[i]].rx - client_traffic_old[client_traffic_new[i]].rx : 0;
			
			diff_tx = diff_tx*8/2;
			diff_rx = diff_rx*8/2;
	
	
			diff_tx_kb = diff_tx/1024;
			diff_rx_kb = diff_rx/1024;
			diff_tx_mb = diff_tx/1024/1024;
			diff_rx_mb = diff_rx/1024/1024;
				
			//upload traffic	
			if((diff_tx/1024) < upload_maximum/5){
				if(diff_tx == 0){
					try{
						$(client_traffic_new[i]+'_upload_bar').style.width = "0%";
					}
					catch(e){
						console.log("[" + i + "] " + client_traffic_new[i]);
					}
				}
				else{				
					tx_width = parseInt(diff_tx_kb/(upload_maximum/5)*30);
					if(diff_tx_kb.toFixed(1) >= 0.1 && tx_width < 1)
						tx_width = 1;
					
					$(client_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";

				}

				if(diff_tx_kb < 1024){	
					$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_kb.toFixed(1);
					$(client_traffic_new[i]+'_upload_unit').innerHTML = "Kb";	
				}
				else{
					$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_mb.toFixed(1) ; 
					$(client_traffic_new[i]+'_upload_unit').innerHTML = "Mb";	
				}
			}
			else if((diff_tx_kb >= upload_maximum/5) && (diff_tx_kb < upload_maximum*2/5)){
				tx_width = parseInt((diff_tx_kb - (upload_maximum/5))/(upload_maximum/5)*25);
				tx_width += 30;
				$(client_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";
				$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_mb.toFixed(1) 
				$(client_traffic_new[i]+'_upload_unit').innerHTML = "Mb";			
			}
			else if((diff_tx_kb >= upload_maximum*2/5) && (diff_tx_kb < upload_maximum*3/5)){
				tx_width = parseInt((diff_tx_kb - (upload_maximum*2/5))/(upload_maximum/5)*20);
				tx_width += 55;
				$(client_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";
				$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_mb.toFixed(1) 
				$(client_traffic_new[i]+'_upload_unit').innerHTML = "Mb";
			
			
			}
			else if((diff_tx_kb >= upload_maximum*3/5) && (diff_tx_kb < upload_maximum*4/5)){
				tx_width = parseInt((diff_tx_kb - (upload_maximum*3/5))/(upload_maximum/5)*15);
				tx_width += 75;
				$(client_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";
				$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_mb.toFixed(1) 
				$(client_traffic_new[i]+'_upload_unit').innerHTML = "Mb";
			}
			else{
				tx_width = parseInt((diff_tx_kb - (upload_maximum*4/5))/(upload_maximum/5)*15);
				tx_width += 90;
				if(tx_width > 100)
					tx_width = 100;
					
				$(client_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";
				$(client_traffic_new[i]+'_upload').innerHTML = diff_tx_mb.toFixed(1) 
				$(client_traffic_new[i]+'_upload_unit').innerHTML = "Mb";				
			}
				
			// download traffic
			if(diff_rx_kb < download_maximum/5){		//30%
				if(diff_rx == 0){					
					try{
						$(client_traffic_new[i]+'_download_bar').style.width = "0%";
					}
					catch(e){
						console.log("[" + i + "] " + client_traffic_new[i]);
					}			
				}	
				else{
					rx_width = parseInt(diff_rx_kb/(download_maximum/5)*30);
					if( diff_rx_kb.toFixed(1) >= 0.1 &&  rx_width < 1)
						rx_width = 1;
						
					$(client_traffic_new[i]+'_download_bar').style.width = rx_width + "%";
				}	
					
				if(diff_rx_kb < 1024){
					$(client_traffic_new[i]+'_download').innerHTML = diff_rx_kb.toFixed(1);
					$(client_traffic_new[i]+'_download_unit').innerHTML = "Kb";
				
				}
				else{
					$(client_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
					$(client_traffic_new[i]+'_download_unit').innerHTML = "Mb";				
				}
									
			}
			else if((diff_rx_kb >= download_maximum/5) && (diff_rx_kb < download_maximum*2/5)){		//	25%
				rx_width = parseInt((diff_rx_kb - (download_maximum/5))/(download_maximum/5)*25);
				rx_width += 30;
				$(client_traffic_new[i]+'_download_bar').style.width = rx_width + "%";
				$(client_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
				$(client_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
			}
			else if((diff_rx_kb >= download_maximum*2/5) && (diff_rx_kb < download_maximum*3/5)){		// 20%
				rx_width = parseInt((diff_rx_kb - (download_maximum*2/5))/(download_maximum/5)*20);
				rx_width += 55;
				$(client_traffic_new[i]+'_download_bar').style.width = rx_width + "%";				
				$(client_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
				$(client_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
			}
			else if((diff_rx_kb >= download_maximum*3/5) && (diff_rx_kb <download_maximum*4/5)){		//	15%
				rx_width = parseInt((diff_rx_kb - (download_maximum*3/5))/(download_maximum/5)*15);
				rx_width += 75;
				$(client_traffic_new[i]+'_download_bar').style.width = rx_width + "%";	
				$(client_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
				$(client_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
			}
			else{		//10%
				rx_width = parseInt((diff_rx_kb - (download_maximum*4/5))/(download_maximum/5)*10);
				rx_width += 90;
				if(rx_width > 100)
					rx_width = 100;
				
				$(client_traffic_new[i]+'_download_bar').style.width = rx_width + "%";	
				$(client_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
				$(client_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
			}	

		}
		
	}

	client_traffic_old = [];
	client_traffic_old = client_traffic_new;	
}

apps_traffic_old = new Array();

function calculate_apps_traffic(apps_traffic){
	apps_traffic_new = new Array();
	var traffic_flag = 0;
	for(i=0;i< apps_traffic.length;i++){	
		apps_traffic_new[i] = apps_traffic[i][0];	
		apps_traffic_new[apps_traffic[i][0]] = {"tx":apps_traffic[i][1], "rx":apps_traffic[i][2]};	
	}
	
	for(i=0;i< apps_traffic_new.length;i++){
		if(apps_traffic_old.length != 0){
			var diff_tx = 0;
			var diff_rx = 0;
			var diff_tx_kb = 0;
			var diff_rx_kb = 0;
			var diff_tx_mb = 0;
			var diff_rx_mb = 0;
			var tx_width = 0;
			var rx_width = 0;

			diff_tx = (apps_traffic_old[apps_traffic_new[i]]) ? apps_traffic_new[apps_traffic_new[i]].tx - apps_traffic_old[apps_traffic_new[i]].tx : 0;
			diff_rx = (apps_traffic_old[apps_traffic_new[i]]) ? apps_traffic_new[apps_traffic_new[i]].rx - apps_traffic_old[apps_traffic_new[i]].rx : 0;
			
			diff_tx = diff_tx*8/2;
			diff_rx = diff_rx*8/2;
			
			diff_tx_kb = diff_tx/1024;
			diff_rx_kb = diff_rx/1024;
			diff_tx_mb = diff_tx/1024/1024;
			diff_rx_mb = diff_rx/1024/1024;

			if(diff_tx ==0 && diff_rx == 0){
				$(apps_traffic_new[i]).parentNode.parentNode.style.display = "none";			
			}
			else{
				traffic_flag = 1;
				if((diff_tx/1024) < upload_maximum/5){
					if(diff_tx == 0){
						try{
							$(apps_traffic_new[i]+'_upload_bar').style.width = "0%";
						}
						catch(e){
							console.log("[" + i + "] " + apps_traffic_new[i]);
						}
					}
					else{				
						tx_width = parseInt(diff_tx_kb/(upload_maximum/5)*30);
						if(diff_tx_kb.toFixed(1) >= 0.1 && tx_width < 1)
							tx_width = 1;
						
						$(apps_traffic_new[i]+'_upload_bar').style.width = tx_width + "%";

					}

					if(diff_tx_kb < 1024){	
						$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_kb.toFixed(1);
						$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Kb";	
					}
					else{
						$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_mb.toFixed(1) ; 
						$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Mb";	
					}
				}
				else if((diff_tx_kb >= upload_maximum/5) && (diff_tx_kb < upload_maximum*2/5)){
					tx_width = parseInt((diff_tx_kb - (upload_maximum/5))/(upload_maximum/5)*25);
					tx_width += 30;
					$(apps_traffic_new[i] + '_upload_bar').style.width = tx_width + "%";
					$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_mb.toFixed(1) 
					$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Mb";			
				}
				else if((diff_tx_kb >= upload_maximum*2/5) && (diff_tx_kb < upload_maximum*3/5)){
					tx_width = parseInt((diff_tx_kb - (upload_maximum*2/5))/(upload_maximum/5)*20);
					tx_width += 55;
					$(apps_traffic_new[i] + '_upload_bar').style.width = tx_width + "%";
					$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_mb.toFixed(1) 
					$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Mb";
				
				
				}
				else if((diff_tx_kb >= upload_maximum*3/5) && (diff_tx_kb < upload_maximum*4/5)){
					tx_width = parseInt((diff_tx_kb - (upload_maximum*3/5))/(upload_maximum/5)*15);
					tx_width += 75;
					$(apps_traffic_new[i] + '_upload_bar').style.width = tx_width + "%";
					$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_mb.toFixed(1) 
					$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Mb";
				}
				else{
					tx_width = parseInt((diff_tx_kb - (upload_maximum*4/5))/(upload_maximum/5)*15);
					tx_width += 90;
					if(tx_width > 100)
						tx_width = 100;
						
					$(apps_traffic_new[i] + '_upload_bar').style.width = tx_width + "%";
					$(apps_traffic_new[i] + '_upload').innerHTML = diff_tx_mb.toFixed(1) 
					$(apps_traffic_new[i] + '_upload_unit').innerHTML = "Mb";				
				}
				
				if(diff_rx_kb < download_maximum/5){		//30%
					if(diff_rx == 0){					
						try{
							$(apps_traffic_new[i]+'_download_bar').style.width = "0%";
						}
						catch(e){
							console.log("[" + i + "] " + apps_traffic_new[i]);
						}			
					}	
					else{
						rx_width = parseInt(diff_rx_kb/(download_maximum/5)*30);
						if(diff_rx_kb.toFixed(1) >= 0.1 && rx_width < 1)
							rx_width = 1;
							
						$(apps_traffic_new[i]+'_download_bar').style.width = rx_width + "%";
					}	
						
					if(diff_rx_kb < 1024){
						$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_kb.toFixed(1);
						$(apps_traffic_new[i]+'_download_unit').innerHTML = "Kb";
					
					}
					else{
						$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
						$(apps_traffic_new[i]+'_download_unit').innerHTML = "Mb";				
					}								
				}
				else if((diff_rx_kb >= download_maximum/5) && (diff_rx_kb < download_maximum*2/5)){		//	25%
					rx_width = parseInt((diff_rx_kb - (download_maximum/5))/(download_maximum/5)*25);
					rx_width += 30;
					$(apps_traffic_new[i]+'_download_bar').style.width = rx_width + "%";
					$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
					$(apps_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
				}
				else if((diff_rx_kb >= download_maximum*2/5) && (diff_rx_kb < download_maximum*3/5)){		// 20%
					rx_width = parseInt((diff_rx_kb - (download_maximum*2/5))/(download_maximum/5)*20);
					rx_width += 55;
					$(apps_traffic_new[i]+'_download_bar').style.width = rx_width + "%";				
					$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
					$(apps_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
				}
				else if((diff_rx_kb >= download_maximum*3/5) && (diff_rx_kb <download_maximum*4/5)){		//	15%
					rx_width = parseInt((diff_rx_kb - (download_maximum*3/5))/(download_maximum/5)*15);
					rx_width += 75;
					$(apps_traffic_new[i]+'_download_bar').style.width = rx_width + "%";	
					$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
					$(apps_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
				}
				else{		//10%
					rx_width = parseInt((diff_rx_kb - (download_maximum*4/5))/(download_maximum/5)*10);
					rx_width += 90;
					if(rx_width > 100)
						rx_width = 100;
					
					$(apps_traffic_new[i]+'_download_bar').style.width = rx_width + "%";	
					$(apps_traffic_new[i]+'_download').innerHTML = diff_rx_mb.toFixed(1);
					$(apps_traffic_new[i]+'_download_unit').innerHTML = "Mb";	
				}
			}		

		}
		
	}
	
	apps_traffic_old = [];
	apps_traffic_old = apps_traffic_new;	
}

function update_device_tarffic() {
  $j.ajax({
    url: '/getTraffic.asp',
    dataType: 'script',	
    error: function(xhr) {
		setTimeout("update_device_tarffic();", 2000);
    },
    success: function(response){
		calculate_router_traffic(router_traffic);
		calculate_traffic(array_traffic);	
		setTimeout("update_device_tarffic();", 2000);
    }
  });
}

var apps_time_flag = "";
function update_apps_tarffic(mac, obj, new_element) {
  $j.ajax({
    url: '/getTraffic.asp?client='+mac,
    dataType: 'script',	
    error: function(xhr) {
		setTimeout("update_apps_tarffic('"+mac+"');", 2000);
    },
    success: function(response){		
		render_apps(array_traffic, obj, new_element);	
		apps_time_flag = setTimeout((function (mac,obj,new_element){ return function (){ update_apps_tarffic(mac,obj,new_element); } })(mac,obj,new_element), 2000);		
    }
  });
}


function regen_qos_rule(obj, priority){
	var qos_rulelist_row =  qos_rulelist.split("<");
	var target_name = $j(obj.parentNode).siblings()[0].children[0].innerHTML;
	var target_mac = $j(obj.parentNode).siblings()[0].children[0].title;
	var rule_temp = "";
	var match_flag = 0;
	
	for(i=1;i<qos_rulelist_row.length;i++){
		var qos_rulelist_col = qos_rulelist_row[i].split(">");
		
		rule_temp += "<";
		for(j=0;j<qos_rulelist_col.length;j++){
			if(target_mac == qos_rulelist_col[1]){		//for device already in the rule list
				if(j == 0)
					rule_temp += target_name + ">"; 
				else if(j == 1)
					rule_temp += target_mac + ">";
				else if(j == qos_rulelist_col.length-1){
					rule_temp += priority;
				}
				else				
					rule_temp += ">";
					
				match_flag = 1;
			}
			else{
				rule_temp += qos_rulelist_col[j];
				if(j != qos_rulelist_col.length-1)
					rule_temp += ">";	
			}	
				
		}

	}
	
	if(match_flag != 1)		//new rule
		rule_temp += "<" + target_name + ">" + target_mac + ">>>>" + priority;
	
	qos_rulelist = rule_temp;
}

function applyRule(){
	document.form.qos_rulelist.value = qos_rulelist;
	document.form.submit();
}

function cal_agreement_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.25)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	

	}

	$("agreement_panel").style.marginLeft = blockmarginLeft+"px";
}


function cancel(){
	$j("#agreement_panel").fadeOut(100);
	document.form.TM_EULA.value = 0;
	$j('#iphone_switch').animate({backgroundPosition: -37}, 300);
	$("hiddenMask").style.visibility = "hidden";
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;">
	<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:25px;margin-left:30px;height: 35px;">Trend Micro End User License Agreement</div>
	<div class="folder_tree"><b>IMPORTANT:</b> THE FOLLOWING AGREEMENT (“AGREEMENT”) SETS FORTH THE TERMS AND CONDITIONS UNDER WHICH TREND MICRO INCORPORATED OR AN AFFILIATE LICENSOR ("TREND MICRO") IS WILLING TO LICENSE THE “SOFTWARE” AND ACCOMPANYING “DOCUMENTATION”  TO “YOU” AS AN INDIVIDUAL USER . BY ACCEPTING THIS AGREEMENT, YOU ARE ENTERING INTO A BINDING LEGAL CONTRACT WITH TREND MICRO. THE TERMS AND CONDITIONS OF THE AGREEMENT THEN APPLY TO YOUR USE OF THE SOFTWARE. PLEASE PRINT THIS AGREEMENT FOR YOUR RECORDS AND SAVE A COPY ELECTRONICALLY
<br><br>
You must read and accept this Agreement before You use Software. If You are an individual, then You must be at least 18 years old and have attained the age of majority in the state, province or country where You live to enter into this Agreement. If You are using the Software, You accept this Agreement by selecting the "I accept the terms of the license agreement" button or box below.  If You do not agree to the terms of this Agreement, select "I do not accept the terms of the license agreement". Then no Agreement will be formed and You will not be permitted to use the Software.
<br><br>
<b>NOTE:</b> SECTIONS 5 AND 14 OF THIS AGREEMENT LIMITS TREND MICRO’S LIABILITY. SECTIONS 6 AND 14 LIMIT OUR WARRANTY OBLIGATIONS AND YOUR REMEDIES. SECTION 4 SETS FORTH IMPORTANT RESTRICTIONS ON THE USE OF THE SOFTWARE AND OTHER TOOLS PROVIDED BY TREND MICRO. THE ATTACHED PRIVACY AND SECURITY STATEMENT DESCRIBES THE INFORMATION YOU CAUSE TO BE SENT TO TREND MICRO WHEN YOU USE THE SOFTWARE. BE SURE TO READ THESE SECTIONS CAREFULLY BEFORE ACCEPTING THE AGREEMENT.
<br><br>
<b>1.	APPLICABLE AGREEMENT AND TERMS.</b><br>
This Agreement applies to  the Trend Micro Services (which includes but is not limited to features and functionality such as deep packet inspection, AppID signature, device ID signature, parental controls, Quality of Service (QoS) for improved performance and Security Services) (the “Software”) for use with certain ASUS routers. All rights in this Agreement are subject to Your acceptance of this Agreement. <br><br> 
2.	SUBSCRIPTION LICENSE.<br> 
Trend Micro grants You and members of Your household a non-exclusive, non-transferable, non-assignable right to use the Software during the Subscription Term (as defined in Section 4 below) but only on the ASUS Router on which it was pre-installed. Trend Micro reserves the right to enhance, modify, or discontinue the Software or to impose new or different conditions on its use at any time without notice. The Software may require Updates to work effectively. “Updates” are new patterns, definitions or rules for the Software’s security components and minor enhancements to the Software and accompanying documentation. Updates are only available for download and use during Your Subscription Term and are subject to the terms of Trend Micro’s end user license agreement in effect on the date the Updates are available for download. Upon download, Updates become “Software” for the purposes of this Agreement.<br><br> 
3.	SUBSCRIPTION TERM.<br>
The “Subscription Term” starts on the date You purchase the ASUS router with the pre-installed Software and ends (a) when such ASUS router is in the end of its useful life as announced by ASUS (generally 3-5 years) or (b) on such other termination date as set by Trend Micro upon notice.<br><br>  
4.	USE RESTRICTIONS.<br>
The Software and any other software or tools provided by Trend Micro are licensed not sold. Trend Micro and its suppliers own their respective title, copyright and the trade secret, patent rights and other intellectual property rights in the Software and their respective copyright in the documentation, and reserve all respective rights not expressly granted to You in this Agreement. You agree that You will not rent, loan, lease or sublicense the Software, use components of the Software separately, exploit the Software for any commercial purposes, or use the Software to provide services to others. You also agree not to attempt to reverse engineer, decompile, modify, translate, disassemble, discover the source code of, or create derivative works from, any part of the Software. You agree not to permit third parties to benefit from the use or functionality of the Software via a timesharing, service bureau or other arrangement.  You agree not to encourage conduct that would constitute a criminal offense or engage in any activity that otherwise interferes with the use and enjoyment of the Software by other or utilize the Software or service to track or monitor the location and activities of any individual without their express consent.  You also agree not to authorize others to undertake any of these prohibited acts.  You may only use the Software in the region for which the Software was authorized to be used or distributed by Trend Micro.<br><br>  
5.	LIMITED LIABILITY<br>
<div style="text-indent:24px;">A. SUBJECT TO SECTION 5(B) BELOW AND TO THE EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL TREND MICRO OR ITS SUPPLIERS BE LIABLE TO YOU (i) FOR ANY LOSSES WHICH WERE NOT REASONABLY FORSEEABLE AT THE TIME OF ENTERING INTO THIS AGREEMENT OR (ii) FOR ANY CONSEQUENTIAL, SPECIAL, INCIDENTAL OR INDIRECT DAMAGES OF ANY KIND OR FOR LOST OR CORRUPTED DATA OR MEMORY, SYSTEM CRASH, DISK/SYSTEM DAMAGE, LOST PROFITS OR SAVINGS, OR LOSS OF BUSINESS, ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE SOFTWARE. THESE LIMITATIONS APPLY EVEN IF TREND MICRO HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES AND REGARDLESS OF THE FORM OF ACTION, WHETHER FOR BREACH OF CONTRACT, NEGLIGENCE, STRICT PRODUCT LIABILITY OR ANY OTHER CAUSE OF ACTION OR THEORY OF LIABILITY.</div>
<div style="text-indent:24px;">B. SECTION 5(A) DOES NOT SEEK TO LIMIT OR EXCLUDE THE LIABILITY OF TREND MICRO OR ITS SUPPLIERS IN THE EVENT OF DEATH OR PERSONAL INJURY CAUSED BY ITS NEGLIGENCE OR FOR FRAUD OR FOR ANY OTHER LIABILITY FOR WHICH IT IS NOT PERMITTED BY LAW TO EXCLUDE.</div>     
<div style="text-indent:24px;">C. SUBJECT TO SECTIONS 5(A) AND 5(B) ABOVE, IN NO EVENT WILL THE AGGREGATE LIABILITY OF TREND MICRO OR ITS SUPPLIERS FOR ANY CLAIM, WHETHER FOR BREACH OF CONTRACT, NEGLIGENCE, STRICT PRODUCT LIABILITY OR ANY OTHER CAUSE OF ACTION OR THEORY OF LIABILITY, EXCEED THE FEES FOR THE SOFTWARE AND SERVICE, AS APPLICABLE, PAID OR OWED BY YOU.</div>
<div style="text-indent:24px;">D. THE LIMITATIONS OF LIABILITY IN THIS SECTION 5 ARE BASED ON THE FACT THAT CUSTOMERS USE THEIR COMPUTERS FOR DIFFERENT PURPOSES. THEREFORE, ONLY YOU CAN IMPLEMENT BACK-UP PLANS AND SAFEGUARDS APPROPRIATE TO YOUR NEEDS IN THE EVENT AN ERROR IN THE SOFTWARE CAUSES PROBLEMS AND RELATED DATA LOSSES. FOR THESE REASONS, YOU AGREE TO THE LIMITATIONS OF LIABILITY IN THIS SECTION 5 AND ACKNOWLEDGE THAT WITHOUT YOUR AGREEMENT TO THIS PROVISION, THE SOFTWARE  WOULD NOT BE AVAILABLE ROYALTY-FREE.</div>
<br><br> 
6.	NO WARRANTY.<br>
Trend Micro does not warrant that the Software will meet Your requirements or that Your use of the Software will be uninterrupted, error-free, timely or secure, that the results that may be obtained from Your use of the Software will be accurate or reliable; or that any errors or problems will be fixed or corrected. GIVEN THE NATURE AND VOLUME OF MALICIOUS AND UNWANTED ELECTRONIC CONTENT, TREND MICRO DOES NOT WARRANT THAT THE SOFTWARE IS COMPLETE OR ACCURATE OR THAT THEY DETECT, REMOVE OR CLEAN ALL, OR ONLY, MALICIOUS OR UNWANTED APPLICATIONS AND FILES.  SEE SECTION 7 FOR ADDITIONAL RIGHTS YOU MAY HAVE.  THE TERMS OF THIS AGREEMENT ARE IN LIEU OF ALL WARRANTIES, (EXPRESS OR IMPLIED), CONDITIONS, UNDERTAKINGS, TERMS AND OBLIGATIONS IMPLIED BY STATUTE, COMMON LAW, TRADE USAGE, COURSE OF DEALING OR OTHERWISE, INCLUDING BUT NOT LIMITED TO IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS, ALL OF WHICH ARE HEREBY EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW. ANY IMPLIED WARRANTIES RELATING TO THE SOFTWARE WHICH CANNOT BE DISCLAIMED SHALL BE LIMITED TO 30 DAYS (OR THE MINIMUM LEGAL REQUIREMENT) FROM THE DATE YOU ACQUIRE THE SOFTWARE.<br><br>  
7.	CONSUMER AND DATA PROTECTION.<br>
SOME COUNTRIES, STATES AND PROVINCES, INCLUDING MEMBER STATES OF THE EUROPEAN ECONOMIC AREA, DO NOT ALLOW CERTAIN EXCLUSIONS OR LIMITATIONS OF LIABILITY, SO THE ABOVE EXCLUSION OR LIMITATION OF LIABILITIES AND DISCLAIMERS OF WARRANTIES (SECTIONS 5 AND 6) MAY NOT FULLY APPLY TO YOU. YOU MAY HAVE ADDITIONAL RIGHTS AND REMEDIES. SUCH POSSIBLE RIGHTS OR REMEDIES, IF ANY, SHALL NOT BE AFFECTED BY THIS AGREEMENT.<br><br>  
8.	DATA PROTECTION REGULATIONS.<br>
The use of the Software may be subject to data protection laws or regulations in certain jurisdictions.  You are responsible for determining how and if You need to comply with those laws or regulations.  Trend Micro processes personal or other data received from You in the context of providing or supporting products or services only as a data processor on Your behalf as required to provide services to You or to perform obligations.  Such data may be transferred to servers of Trend Micro and its suppliers outside Your jurisdiction (including outside the European Union).<br><br>  
9.	NOTIFICATION TO OTHER USERS.<br> 
It is Your responsibility to notify all individuals whose activities will be monitored of the capabilities and functionality of Software, including but not limited to the following: (a) use of the Internet may be recorded and reported, and (b) personal data is automatically collected during the course of the operation of Software and that such data may be transferred and processed outside the European Union, which may not ensure an adequate level of protection.  In the event of any breach of the representations and warranties in this Section, Trend Micro may with prior notice and without prejudice to its other rights, suspend the performance of Software until You can show to Trend Micro's satisfaction that any such breach has been cured.<br><br>  
10.	BACK-UP.<br>
For as long as You use the Software, You agree regularly to back-up Your Computer programs and files (“Data”) on a separate media. You acknowledge that the failure to do so may cause You to lose Data in the event that any error in the Software causes Computer problems, and that Trend Micro is not responsible for any such Data loss.<br><br>  
11.	TERMINATION.<br>
Trend Micro may terminate Your rights under this Agreement and Your access to the Software immediately and without notice if You fail to comply with any material term or condition of this Agreement. Sections 1 and 4 through 17 survive any termination of the Agreement.<br><br>  
12.	FORCE MAJEURE.<br>
Trend Micro will not be liable for any alleged or actual loss or damages resulting from delays or failures in performance caused by Your acts, acts of civil or military authority, governmental priorities, earthquake, fire, flood, epidemic, quarantine, energy crisis, strike, labor trouble, war, riot, terrorism, accident, shortage, delay in transportation, or any other cause beyond its reasonable control.  Trend Micro shall resume the performance of its obligations as soon as reasonably possible.<br><br>  
13.	EXPORT CONTROL.<br>
The Software is subject to export controls under the U.S. Export Administration Regulations. Therefore, the Software may not be exported or re-exported to entities within, or residents or citizens of, embargoed countries or countries subject to applicable trade sanctions, nor to prohibited or denied persons or entities without proper government licenses. Information about such restrictions can be found at the following websites:  www.treas.gov/offices/enforcement/ofac/ and www.bis.doc.gov/complianceandenforcement/liststocheck.htm. As of the Date above, countries embargoed by the U.S. include Cuba, Iran, North Korea, Sudan (North) and Syria.  You are responsible for any violation of the U.S. export control laws related to the Software. By accepting this Agreement, You confirm that You are not a resident or citizen of any country currently embargoed by the U.S. and that You are not otherwise prohibited from receiving the Software.<br><br>  
14.	BINDING ARBITRATION AND CLASS ACTION WAIVER FOR U.S. RESIDENTS<br>
PLEASE READ THIS SECTION CAREFULLY.  THIS SECTION AFFECTS YOUR LEGAL RIGHTS CONCERNING ANY DISPUTES BETWEEN YOU AND TREND MICRO.  FOR PURPOSES OF THIS SECTION, “TREND MICRO” MEANS TREND MICRO INCORPORATED AND ITS SUBSIDIARIES, AFFILIATES, OFFICERS, DIRECTORS, EMPLOYEES, AGENTS AND SUPPLIERS.<br>
<div style="text-indent:24px;"><b>A. DISPUTE.</b>  As used in this Agreement, “Dispute” means any dispute, claim, demand, action, proceeding or other controversy between You and Trend Micro concerning the Software and Your or Trend Micro’s obligations and performance under this Agreement or with respect to the Software, whether based in contact, warranty, tort (including any limitation, fraud, misrepresentation, fraudulent inducement, concealment, omission, negligence, conversion, trespass, strict liability and product liability), statue (including, without limitation, consumer protection and unfair competition statutes), regulation, ordinance, or any other legal or equitable basis or theory.  "Dispute" will be given the broadest possible meaning allowable under law.</div>
<div style="text-indent:24px;"><b>B. INFORMAL NEGOTIATION.</b>  You and Trend Micro agree to attempt in good faith to resolve any Dispute before commencing arbitration.  Unless You and Trend Micro otherwise agree in writing, the time for informal negotiation will be 60 days from the date on which You or Trend Micro mails a notice of the Dispute ("Notice of Dispute") as specified in Section 14C (Notice of Dispute). You and Trend Micro agree that neither will commence arbitration before the end of the time for informal negotiation.</div>
<div style="text-indent:24px;"><b>C. NOTICE OF DISPUTE.</b> If You give a Notice of Dispute to Trend Micro, You must send by U.S. Mail to Trend Micro Incorporated, ATTN: Arbitration Notice, Legal Department, 10101 N. De Anza Blvd, Cupertino, CA 95014, a written statement setting forth (a) Your name, address, and contact information, (b) Your Software serial number, if You have one, (c) the facts giving rise to the Dispute, and (d) the relief You seek. If Trend Micro gives a Notice of Dispute to You, we will send by U.S. Mail to Your billing address if we have it, or otherwise to Your e-mail address, a written statement setting forth (a) Trend Micro’s contact information for purposes of efforts to resolve the Dispute, (b) the facts giving rise to the Dispute, and (c) the relief Trend Micro seeks.</div>
<div style="text-indent:24px;"><b>D. BINDING ARBITRATION.</b> YOU AND TREND MICRO AGREE THAT IF YOU AND TREND MICRO DO NOT RESOLVE ANY DISPUTE BY INFORMAL NEGOTIATION AS SET FORTH ABOVE, ANY EFFORT TO RESOLVE THE DISPUTE WILL BE CONDUCTED EXCLUSIVELY BY BINDING ARBITRATION IN ACCORDANCE WITH THE ARBITRATION PROCEDURES SET FORTH BELOW. YOU UNDERSTAND AND ACKNOWLEDGE THAT BY AGREEING TO BINDING ARBITRATION, YOU ARE GIVING UP THE RIGHT TO LITIGATE (OR PARTICIPATE IN AS A PARTY OR CLASS MEMBER) ALL DISPUTES IN COURT BEFORE A JUDGE OR JURY. INSTEAD, YOU UNDERSTAND AND AGREE THAT ALL DISPUTES WILL BE RESOLVED BEFORE A NEUTRAL ARBITRATOR, WHOSE DECISION WILL BE BINDING AND FINAL, EXCEPT FOR A LIMITED RIGHT OF APPEAL UNDER THE FEDERAL ARBITRATION ACT. ANY COURT WITH JURISDICTION OVER THE PARTIES MAY ENFORCE THE ARBITRATOR'S AWARD. THE ONLY DISPUTES NOT COVERED BY THE AGREEMENT TO NEGOTIATE INFORMALLY AND ARBITRATE ARE DISPUTES ENFORCING, PROTECTING, OR CONCERNING THE VALIDITY OF ANY OF YOUR OR TREND MICRO’S (OR ANY OF YOUR OR TREND MICRO’S LICENSORS) INTELLECTUAL PROPERTY RIGHTS.</div>
<div style="text-indent:24px;"><b>E. SMALL CLAIMS COURT.</b> Notwithstanding the above, You have the right to litigate any Dispute in small claims court, if all requirements of the small claims court, including any limitations on jurisdiction and the amount at issue in the Dispute, are satisfied. Notwithstanding anything to the contrary herein, You agree to bring a Dispute in small claims court only in Your county of residence or Santa Clara County, California. </div>
<div style="text-indent:24px;"><b>F. CLASS ACTION WAIVER.</b> TO THE EXTENT ALLOWED BY LAW, YOU AND TREND MICRO AGREE THAT ANY PROCEEDINGS TO RESOLVE OR LITIGATE ANY DISPUTE, WHETHER IN ARBITRATION, IN COURT, OR OTHERWISE, WILL BE CONDUCTED SOLELY ON AN INDIVIDUAL BASIS, AND THAT NEITHER YOU NOR TREND MICRO WILL SEEK TO HAVE ANY DISPUTE HEARD AS A CLASS ACTION, A REPRESENTATIVE ACTION, A COLLECTIVE ACTION, A PRIVATE ATTORNEY-GENERAL ACTION, OR IN ANY PROCEEDING IN WHICH YOU OR TREND MICRO ACTS OR PROPOSES TO ACT IN A REPRESENTATIVE CAPACITY. YOU AND TREND MICRO FURTHER AGREE THAT NO ARBITRATION OR PROCEEDING WILL BE JOINED, CONSOLIDATED, OR COMBINED WITH ANOTHER ARBITRATION OR PROCEEDING WITHOUT THE PRIOR WRITTEN CONSENT OF YOU, TREND MICRO, AND ALL PARTIES TO ANY SUCH ARBITRATION OR PROECCEDING. </div>
<div style="text-indent:24px;"><b>G. ARBITRATION PROCEDURE.</b> The arbitration of any Dispute will be conducted by and according to the Commercial Arbitration Rules of the American Arbitration Association (the "AAA"). Information about the AAA, and how to commence arbitration before it, is available at www.adr.org. If You are an individual consumer and use the Software for personal or household use, or if the value of the Dispute is $75,000 or less, the Supplementary Procedures for Consumer-Related Disputes of the AAA will also apply. If the AAA rules or procedures conflict with the provisions of this Agreement, the provisions of this Agreement will govern. You may request a telephonic or in-person hearing by following the AAA rules and procedures. Where the value of a Dispute is $10,000 or less, any hearing will be through a telephonic hearing unless the arbitrator finds good cause to hold an in-person hearing. The arbitrator has the power to make any award of damages to the individual party asserting a claim that would be available to a court of law. The arbitrator may award declaratory or injunctive relief only in favor of the individual party asserting a claim, and only to the extent required to provide relief on that party's individual claim.</div>
<div style="text-indent:24px;"><b>H. ARBITRATION LOCATION.</b> You agree to commence arbitration only in Your county of residence or in Santa Clara County, California. Trend Micro agrees to commence arbitration only in Your county of residence.</div>
<div style="text-indent:24px;"><b>I. COSTS AND ATTORNEY'S FEES.</b> In a dispute involving $75,000 or less, Trend Micro will reimburse for payment of Your filing fees, and pay the AAA administrative fees and the arbitrator's fees and expenses, incurred in any arbitration You commence against Trend Micro unless the arbitrator finds it frivolous or brought for an improper purpose. Trend Micro will pay all filing and AAA administrative fees, and the arbitrator's fees and expenses, incurred in any arbitration Trend Micro commences against You. If a Dispute involving $75,000 or less proceeds to an award at the arbitration after You reject the last written settlement offer Trend Micro made before the arbitrator was appointed ("Trend Micro's Last Written Offer"), and the arbitrator makes an award in Your favor greater than Trend Micro's Last Written Offer, Trend Micro will pay You the greater of the award or $1,000, plus twice Your reasonable attorney's fees, if any, and reimburse any expenses (including expert witness fees and costs) that Your attorney reasonably accrues for investigating, preparing, and pursuing Your claim in arbitration, as determined by the arbitrator or agreed to by You and Trend Micro. In any arbitration You commence, Trend Micro will seek its AAA administrative fees or arbitrator's fees and expenses, or Your filing fees it reimbursed, only if the arbitrator finds the arbitration frivolous or brought for an improper purpose. Trend Micro will not seek its attorney's fees or expenses from You. In a Dispute involving more than $75,000, the AAA rules will govern payment of filing and AAA administrative fees and arbitrator's fees and expenses. Fees and expenses are not counted in determining how much a Dispute involves.</div>
<div style="text-indent:24px;"><b>J. IF CLASS ACTION WAIVER ILLEGAL OR UNENFORCEABLE.</b> If the class action waiver (which includes a waiver of private attorney-general actions) in Section 14F (class action waiver) is found to be illegal or unenforceable as to all or some parts of a Dispute, whether by judicial, legislative, or other action, then the remaining paragraphs in Section 14 will not apply to those parts. Instead, those parts of the Dispute will be severed and proceed in a court of law, with the remaining parts proceeding in arbitration. The definition of "Dispute" in this section will still apply to this Agreement. You and Trend Micro irrevocably consent to the exclusive jurisdiction and venue of the state or federal courts in Santa Clara County, California, USA, for all proceedings in court under this paragraph.</div>
<div style="text-indent:24px;"><b>K. YOUR RIGHT TO REJECT CHANGES TO ARBITRATION AGREEMENT.</b> Notwithstanding anything to the contrary in this Agreement, Trend Micro agrees that if it makes any change to Section 14 (other than a change to the notice address) while You are authorized to use the Software, You may reject the change by sending us written notice within 30 days of the change by U.S. Mail to the address in Section 14C. By rejecting the change, You agree that You will informally negotiate and arbitrate any Dispute between us in accordance with the most recent version of Section 14 before the change You rejected.</div>
<div style="text-indent:24px;"><b>L. SEVERABILITY.</b> If any provision of Section 14 and its subsections, other than Section 14F (class action waiver), is found to be illegal or unenforceable, that provision will be severed from Section 14, but the remainder paragraphs of Section 14 will remain in full force and effect. Section 14J says what happens if Section 14F (class action waiver) is found to be illegal or unenforceable.</div>
<br>
15.	GENERAL.<br>
This Agreement and Subscription Term constitute the entire agreement between You and Trend Micro. Unless the Software is subject to an existing, written contract signed by Trend Micro, this Agreement supersedes any prior agreement or understanding, whether written or oral, relating to the subject matter of this Agreement. In the event that any provision of this Agreement is found invalid, that finding will not affect the validity of the remaining parts of this Agreement. Trend Micro may assign or subcontract some or all of its obligations under this Agreement to qualified third parties or its affiliates and/or subsidiaries, provided that no such assignment or subcontract shall relieve Trend Micro of its obligations under this Agreement.<br><br>  

16.	GOVERNING LAW/TREND MICRO LICENSING ENTITY.<br>
<b>North America:</b>  If You are located in the United States or Canada, the Licensor is: Trend Micro Incorporated, 10101 N. De Anza Blvd., Cupertino, CA 95014. Fax:  (408) 257-2003 and this Agreement is governed by the laws of the State of California, USA. <br><br>  
<b>Latin America:</b> If You are located in Spanish Latin America (other than in any countries embargoed by the U.S.), the Licensor is: Trend Micro Latinoamérica, S. A. de C. V., Insurgentes Sur No. 813, Piso 11, Col. Nápoles, 03810 México, D. F.  Tel: 3067-6000 and this Agreement is governed by the laws of Mexico.  If You are located in Brazil, the Licensor is Trend Micro do Brasil, LTDA, Rua Joaquim Floriano, 1.120 – 2º andar, CEP 04534-004, São Paulo/Capital, Brazil and this Agreement is governed by the laws of Brazil.<br><br>
<b>Europe, Middle East and Africa (other than countries embargoed by the U.S):</b>  If You are located in the United Kingdom, this Agreement is governed by the laws of England and Wales. If You are located in Austria, Germany or Switzerland, this Agreement is governed by the laws of the Federal Republic of Germany. If You are located in France, this Agreement is governed by the laws of France. If You are located in Italy, this Agreement is governed by the laws of Italy.  If You are located in Europe, the Licensor is: Trend Micro EMEA Limited, a company incorporated in Ireland under number 364963 and having its registered office at IDA Business and Technology Park, Model Farm Road, Cork, Ireland.  Fax: +353-21 730 7 ext. 373.<br><br>
If You are located in Africa or the Middle East (other than in those countries embargoed by the U.S.), or Europe (other than Austria, France, Germany, Italy, Switzerland or the U.K.), the Licensor is: Trend Micro EMEA Limited, a company incorporated in Ireland under number 364963 and having its registered office at IDA Business and Technology Park, Model Farm Road, Cork, Ireland.  Fax: +353-21 730 7 ext. 373 and this Agreement is governed by the laws of the Republic of Ireland.<br><br>
<b>Asia Pacific (other than Japan or countries embargoed by the U.S):</b>  If You are located in Australia or New Zealand, the Licensor is: Trend Micro Australia Pty Limited, Suite 302, Level 3, 2-4 Lyon Park Road, North Ryde, New South Wales, 2113, Australia, Fax: +612 9887 2511 or Tel: +612 9870 4888 and this Agreement is governed by the laws of New South Wales, Australia.<br><br>
If You are located in Hong Kong, India, Indonesia, Malaysia, the Philippines, Singapore, Thailand or Vietnam, the Licensor is: Trend Taiwan Incorporated, 8F, No.198, Tun-Hwa S. Road, Sec. 2, Taipei 106, Taiwan. If You are located in Hong Kong, this Agreement is governed by the laws of Hong Kong. If You are located in India, this Agreement is governed by the laws of India. If You are located in Indonesia, Malaysia, the Philippines, or Singapore, this Agreement is governed by the laws of Singapore. If You are located in Thailand, this Agreement is governed by the laws of Thailand.  If You are located in Vietnam, this Agreement is governed by the laws of Viet Nam.<br><br>
<b>Japan:</b>  If You are located in Japan, the licensor is Trend Micro Incorporated, Shinjuku MAYNDS Tower, 1-1 Yoyogi 2-Chome, Shibuya-ku, Tokyo 151-0053, Japan and this agreement is governed by laws of Japan. <br><br>
The United Nations Convention on Contracts for the International Sale of Goods and the conflict of laws provisions of Your state or country of residence do not apply to this Agreement under the laws of any country.
<br><br>
17.	GOVERNMENT LICENSEES.<br>
If the entity on whose behalf You are acquiring the Software is any unit or agency of the United States Government, then that Government entity acknowledges that the Software, (i) was developed at private expense, (ii) is commercial in nature, (iii) is not in the public domain, and (iv) is "Restricted Computer Software" as that term is defined in Clause 52.227 19 of the Federal Acquisition Regulations (FAR) and is "Commercial Computer Software" as that term is defined in Subpart 227.471 of the Department of Defense Federal Acquisition Regulation Supplement (DFARS). The Government agrees that (i) if the Software is supplied to the Department of Defense (DoD), the Software is classified as "Commercial Computer Software" and the Government is acquiring only "restricted rights" in the Software and its documentation as that term is defined in Clause 252.227 7013(c)(1) of the DFARS, and (ii) if the Software is supplied to any unit or agency of the United States Government other than DoD, the Government's rights in the Software and its documentation will be as defined in Clause 52.227 19(c)(2) of the FAR.
<br><br>
<b>18.	QUESTIONS.</b>
If You have a question about the Software, visit: www.trendmicro.com/support/consumer. Direct all questions about this Agreement to: legalnotice@trendmicro.com.
<br><br>
THE SOFTWARE IS PROTECTED BY INTELLECTUAL PROPERTY LAWS AND INTERNATIONAL TREATY PROVISIONS. UNAUTHORIZED REPRODUCTION OR DISTRIBUTION IS SUBJECT TO CIVIL AND CRIMINAL PENALTIES.
<br><br>
<b>PRIVACY AND SECURITY STATEMENT REGARDING FORWARDED DATA</b><br>
In addition to product registration information, Trend Micro will receive information from You and Your router on which the Software or any support software tools are installed (such as IP or MAC address, location, content, device ID or name, etc) to enable Trend Micro to provide the functionality of the Software and related support services (including content synchronization, status relating to installation and operation of the Software, device tracking and service improvements, etc).  
<br>
By using the Software, You will also cause certain information (“Forwarded Data”) to be sent to Trend Micro-owned or -controlled servers for security scanning and other purposes as described in this paragraph.  This Forwarded Data may include information on potential security risks as well as URLs of websites visited that the Software deem potentially fraudulent and/or executable files or content that are identified as potential malware. Forwarded Data may also include email messages identified as spam or malware that contains personally identifiable information or other sensitive data stored in files on Your router.  This Forwarded Data is necessary to enable Trend Micro to detect malicious behavior, potentially fraudulent websites and other Internet security risks, for product analysis and to improve its services and Software and their functionality and to provide You with the latest threat protection and features.  
<br>
You can only opt out of sending Forwarded Data by not using, or uninstalling or disabling the Software.  All Forwarded Data shall be maintained in accordance with Trend Micro’s Privacy Policy which can be found at www.trendmicro.com. You agree that the Trend Micro Privacy Policy as may be amended from time to time shall be applicable to You. Trend Micro reserves the title, ownership and all rights and interests to any intellectual property or work product resulting from its use and analysis of Forwarded Data. 
</div>
	<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">
		<input class="button_gen" type="button" style="margin-left:27%;margin-top:18px;" onclick="cancel();" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button"  onclick="applyRule();" value="<#CTL_ok#>">	
	</div>
</div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/AiStream_Live.asp">
<input type="hidden" name="next_page" value="/AiStream_Live.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos;">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="qos_rulelist" value="">
<input type="hidden" name="TM_EULA" value="<% nvram_get("TM_EULA"); %>">
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
			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle">
			<tr>
				<td bgcolor="#4D595D" valign="top">
					<table width="760px" border="0" cellpadding="4" cellspacing="0">
						<tr>
							<td bgcolor="#4D595D" valign="top">
								<table width="100%">
									<tr>
										<td class="formfonttitle" align="left">								
											<div>Adaptive QoS - Live</div>
										</td>
										<td>
											<div>
												<table align="right">
													<tr>
														<td>
															<div class="formfonttitle" style="margin-bottom:0px;">Apps analysis</div>
														</td>
														<td >
															<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="TM_EULA_enable"></div>
															<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
																<script type="text/javascript">
																		$j('#TM_EULA_enable').iphoneSwitch('<% nvram_get("TM_EULA"); %>',
																			function(){
																				document.form.TM_EULA.value = 1;
																				dr_advise();
																				cal_agreement_block();
																				$j("#agreement_panel").fadeIn(300);
																			},
																			function(){
																				document.form.TM_EULA.value = 0;
																				if(document.form.TM_EULA.value == '<% nvram_get("TM_EULA"); %>')
																					return false;
																															
																				applyRule();
																			},
																					{
																					switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
																			});
																</script>			
															</div>
														</td>														
													</tr>
												</table>
											</div>
										</td>
									</tr>
								</table>	
							</td>
						</tr>
						<tr>
							<td height="5" bgcolor="#4D595D" valign="top"><img src="images/New_ui/export/line_export.png" /></td>
						</tr>
						<tr>
							<td>
								<div>
									<table style="width:99%;">
										<tr>
											<td id="upload_unit" style="width:50%;">
												<div style="position:absolute;margin-left:75px;font-size:16px;">Upload</div>
												<div style="position:absolute;margin:12px 0px 0px 112px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:-8px 0px 0px 222px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:50px 0px 0px 300px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:121px 0px 0px 296px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:150px 0px 0px 275px;font-size:16px;display:none;"></div>
												<div id="upload_speed" style="position:absolute;margin:150px 0px 0px 187px;font-size:16px;width:60px;text-align:center;">0.00</div>
												<div style="background-image:url('images/New_ui/speedmeter.png');height:188px;width:270px;background-repeat:no-repeat;margin:-10px 0px 0px 70px"></div>
												<div id="indicator_upload" style="background-image:url('images/New_ui/indicator.png');position:absolute;height:100px;width:50px;background-repeat:no-repeat;margin:-110px 0px 0px 194px;"></div>
											</td>
											<td id="download_unit">	
												<div style="position:absolute;font-size:16px;">Download</div>
												<div style="position:absolute;margin:12px 0px 0px 88px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:-6px 0px 0px 203px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:50px 0px 0px 275px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:120px 0px 0px 270px;font-size:16px;display:none;"></div>
												<div style="position:absolute;margin:150px 0px 0px 250px;font-size:16px;display:none;"></div>
												<div id="download_speed" style="position:absolute;margin:150px 0px 0px 130px;font-size:16px;text-align:center;width:60px;">0.00</div>
												<div style="background-image:url('images/New_ui/speedmeter.png');height:188px;width:270px;background-repeat:no-repeat;margin:-10px 0px 0px 10px"></div>
												<div id="indicator_download" style="background-image:url('images/New_ui/indicator.png');position:absolute;height:100px;width:50px;background-repeat:no-repeat;margin:-110px 0px 0px 133px;"></div>		
											</td>
										</tr>
										<!--tr style="text-align:center;font-size:22px;">
											<td>
												<div>
													<table style="width:99%">
														<tr>
															<td style="width:50%;text-align:right">Upload:</td>
															<td id="upload_speed" style="width:20%;text-align:center;color:rgb(238,198,38);">0.0</td>
															<td id="upload_speed_unit" style="width:30%;display:none;">Mb</td>
														</tr>
													</table>
												</div>
											</td>
											
											<td>
												<div>
													<table style="width:99%">
														<tr>
															<td style="width:30%;text-align:right">Download:</td>
															<td id="download_speed" style="width:20%;text-align:center;color:rgb(238,198,38);">0.0</td>
															<td id="download_speed_unit" style="width:34%;text-align:left;display:none;">Mb</td>
														</tr>
													</table>
												</div>										
											</td>
										</tr-->
									</table>	
								</div>		
							</td>
						</tr>

						<tr>
							<td>
								<div style="margin-bottom:-5px;margin-top:10px;">
									<table>									
										<tr>
											<th style="width:50%;font-family: Arial, Helvetica, sans-serif;font-size:16px;text-align:right;padding-left:15px;"></th>
											<td>
												<div id="priority_block">
													<table>
														<tr>
															<td>
																<!--div id="0" style="cursor:pointer;background-color:#F01F09;width:80px;border-radius:10px;text-align:center;color:black;">Highest</div-->
																
																<div id="0" style="cursor:pointer;background-color:#444F53;width:80px;border-radius:10px;text-align:center;box-shadow:0px 2px black;">
																	<table>
																		<tr>
																			<td style="width:25px;"><div style="width:15px;height:15px;border-radius:10px;background-color:#F01F09;margin-left:5px;"></div></td>
																			<td>Highest</td>															
																		</tr>
																	</table>
																</div>
															</td>
															<td>
																<!--div id="1" style="cursor:pointer;background-color:#F08C09;width:80px;border-radius:10px;text-align:center;color:black;">High</div-->
																<div id="1" style="cursor:pointer;background-color:#444F53;width:80px;border-radius:10px;text-align:center;box-shadow:0px 2px black;">
																	<table>
																		<tr>
																			<td style="width:25px;"><div style="width:15px;height:15px;border-radius:10px;background-color:#F08C09;margin-left:5px;"></div></td>
																			<td>High<td>
																		</tr>
																	</table>
																</div>	
															</td>
															<td>
																<!--div id="2" style="cursor:pointer;background-color:#F3DD09;width:80px;border-radius:10px;text-align:center;color:black;">Medium</div-->
																<div id="2" style="cursor:pointer;background-color:#444F53;width:80px;border-radius:10px;text-align:center;box-shadow:0px 2px black;">
																	<table>
																		<tr>
																			<td style="width:25px;"><div style="width:15px;height:15px;border-radius:10px;background-color:#F3DD09;margin-left:5px;"></div></td>
																			<td>Medium<td>
																		</tr>
																	</table>
																</div>
															</td>
															<td>
																<!--div id="3" style="cursor:pointer;background-color:#59E920;width:80px;border-radius:10px;text-align:center;color:black;">Low</div-->												
																<div id="3" style="cursor:pointer;background-color:#444F53;width:80px;border-radius:10px;text-align:center;box-shadow:0px 2px black;">
																	<table>
																		<tr>
																			<td style="width:25px;"><div style="width:15px;height:15px;border-radius:10px;background-color:#59E920;margin-left:5px;"></div></td>
																			<td>Low<td>
																		</tr>
																	</table>						
																</div>												
															</td>
															<td>
																<!--div id="4" style="cursor:pointer;background-color:#58CCED;width:80px;border-radius:10px;text-align:center;color:black;">Lowest</div-->												
																<div id="4" style="cursor:pointer;background-color:#444F53;width:80px;border-radius:10px;text-align:center;box-shadow:0px 2px black;">
																	<table>
																		<tr>
																			<td style="width:25px;"><div style="width:15px;height:15px;border-radius:10px;background-color:#58CCED;margin-left:5px;"></div></td>
																			<td>Lowest<td>
																		</tr>
																	</table>											
																</div>												
															</td>
														</tr>
													</table>
												</div>
											<td>
										</tr>
									</table>
								</div>
							</td>
						</tr>
						
						<tr>					
							<td>
								<div colspan="2" style="width:99%;height:510px;background-color:#444f53;border-radius:10px;margin-left:4px;overflow:auto;">
									<div id="sortable" style="padding-top:5px;">
										<div style="width: 100%;text-align: center;margin-top: 50px;">
											<img src="/images/InternetScan.gif" style="width: 50px;">
										</div>
    								</div>
								</div>
							</td>
						</tr>
						<tr>
							<td>
								<div style=" *width:136px;margin:5px 0px 0px 300px;" class="titlebtn" align="center" onClick="applyRule();"><span><#CTL_apply#></span></div>
							</td>
						</tr>		
					</table>
				</td>  
			</tr>
			</table>
			<!--===================================End of Main Content===========================================-->
		</td>		
	</tr>
</table>
<div id="footer"></div>
