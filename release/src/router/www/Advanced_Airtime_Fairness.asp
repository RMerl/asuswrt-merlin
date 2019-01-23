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
<title><#Web_Title#> - Airtime Fairness</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<link rel="stylesheet" type="text/css" href="device-map/device-map.css">
<script type="text/javascript" src="state.js"></script>
<script type="text/javascript" src="general.js"></script>
<script type="text/javascript" src="popup.js"></script>
<script type="text/javascript" src="help.js"></script>
<script type="text/javascript" src="validator.js"></script>
<script type="text/javascript" src="js/jquery.js"></script>
<script type="text/javascript" src="calendar/jquery-ui.js"></script> 
<script type="text/javascript" src="client_function.js"></script>
<script type="text/javascript" src="jquery.xdomainajax.js"></script>
<style>
#pull_arrow{
 	float:center;
 	cursor:pointer;
 	border:2px outset #EFEFEF;
 	background-color:#CCC;
 	padding:3px 2px 4px 0px;
	*margin-left:-3px;
	*margin-top:1px;
}

.WL_MAC_Block{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:27px;	
	margin-left:54px;
	*margin-left:-133px;
	width:255px;
	text-align:left;	
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
.WL_MAC_Block div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

.WL_MAC_Block a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
.WL_MAC_Block div:hover, .WL_MAC_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
.ui-slider {
	position: relative;
	text-align: left;
}
.ui-slider .ui-slider-handle {
	position: absolute;
	width: 12px;
	height: 12px;
}
.ui-slider .ui-slider-range {
	position: absolute;
}
.ui-slider-horizontal {
	height: 6px;
}

.ui-widget-content {
	border: 2px solid #000;
	background-color:#000;
}
.ui-state-default,
.ui-widget-content .ui-state-default,
.ui-widget-header .ui-state-default {
	border: 1px solid ;
	background: #e6e6e6;
	margin-top:-4px;
	margin-left:-6px;
}

/* Corner radius */
.ui-corner-all,
.ui-corner-top,
.ui-corner-left,
.ui-corner-tl {
	border-top-left-radius: 4px;
}
.ui-corner-all,
.ui-corner-top,
.ui-corner-right,
.ui-corner-tr {
	border-top-right-radius: 4px;
}
.ui-corner-all,
.ui-corner-bottom,
.ui-corner-left,
.ui-corner-bl {
	border-bottom-left-radius: 4px;
}
.ui-corner-all,
.ui-corner-bottom,
.ui-corner-right,
.ui-corner-br {
	border-bottom-right-radius: 4px;
}

.ui-slider-horizontal .ui-slider-range {
	top: 0;
	height: 100%;
}
.slider {
	width: 200px;
	float: left;
	margin-top: 7px;
	margin-left: 20px;
} 
.ui-slider-range {
	background: #93E7FF; 
	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
.ui-slider-handle { 
	border-color: #93E7FF; 
}
</style>
<script>
<% wl_get_parameter(); %>
var wl_atf_sta_array = decodeURIComponent('<% nvram_char_to_ascii("", "wl_atf_sta"); %>');
var wl_atf_ssid_array = decodeURIComponent('<% nvram_char_to_ascii("", "wl_atf_ssid"); %>');
var wl_atf_sta_list_array = new Array();
var wl_atf_ssid_list_array = new Array();

var radio_2 = '<% nvram_get("wl0_radio"); %>';
var radio_5 = '<% nvram_get("wl1_radio"); %>';
var radio_5_2 = '<% nvram_get("wl2_radio"); %>';
var radio_status = 0;
var gn_array = "";
var unit = "<% nvram_get("wl_unit"); %>";
function initial(){
	show_menu();
	showWLMACList();
	
	regen_band(document.form.wl_unit);

	var wl_atf_sta_row = wl_atf_sta_array.split("<");
	for(var i = 0; i < wl_atf_sta_row.length; i += 1) {
		if(wl_atf_sta_row[i] != "") {
			var wl_atf_sta_col = wl_atf_sta_row[i].split(">");
			wl_atf_sta_list_array[wl_atf_sta_col[0]] = wl_atf_sta_col[1];
		}
	}

	if(wl_atf_ssid_array == "") {
		wl_atf_ssid_array +=  "<0";
		var wl_vifnames = '<% nvram_get("wl_vifnames"); %>';
		var gn_count = wl_vifnames.split(" ").length;
		for(var wl_idx = 0; wl_idx < gn_count; wl_idx += 1) {
			wl_atf_ssid_array +=  "<0";
		}
	}

	var gen_wl_interface = function(_name, _value) {
		var wl_interface_option  = document.createElement("option");
		wl_interface_option.text = _name;
		wl_interface_option.value = _value;
		document.form.wl_interface.add(wl_interface_option);
	};

	switch(parseInt(unit)){
		case 0:
			gn_array = gn_array_2g;
			radio_status = parseInt(radio_2);
			break;
		case 1:			
			gn_array = gn_array_5g;
			radio_status = parseInt(radio_2);
			break;
		case 2:
			gn_array = gn_array_5g_2;
			radio_status = parseInt(radio_2);
			break;
	}

	var wl_ssid_atf_row = wl_atf_ssid_array.split("<");
	var wl_id = ["", "wl_main", "wl_gn1", "wl_gn2", "wl_gn3"];
	for(var j = 0; j < wl_ssid_atf_row.length; j += 1) {
		if(wl_id[j] != "") {
			wl_atf_ssid_list_array[wl_id[j]] = wl_ssid_atf_row[j];
			switch(wl_id[j]) {
				case "wl_main" : 
					gen_wl_interface('<% nvram_get("wl_ssid"); %>', wl_id[j]);
					break;
				case "wl_gn1" : 
					gen_wl_interface(gn_array[0][1], wl_id[j]);
					break;
				case "wl_gn2" : 
					gen_wl_interface(gn_array[1][1], wl_id[j]);
					break;
				case "wl_gn3" : 
					gen_wl_interface(gn_array[2][1], wl_id[j]);
					break;
			}
		}
	}

	if(!band5g_support)
		document.getElementById("wl_unit_field").style.display = "none";

	var roaming_assistant = parseInt(document.form.wl_atf_mode.value);
	switch(roaming_assistant) {
		case 0 :
			document.getElementById("byClientBlock").style.display = "";
			show_wl_atf_by_client();
			break;
		case 1 :
			document.getElementById("bySSIDBlock").style.display = "";
			show_wl_atf_by_ssid();
			break;
		case 2 :
			document.getElementById("byClientBlock").style.display = "";
			show_wl_atf_by_client();
			document.getElementById("bySSIDBlock").style.display = "";
			show_wl_atf_by_ssid();
			break;	
	}

	$(function() {
		//register_event_client_edit
		$( "#slider_client_edit" ).slider({
			orientation: "horizontal",
			range: "min",
			min: 1,
			max: 100,
			value: 1,
			slide: function(event, ui) {
				document.getElementById("wl_pct_client_edit").value = ui.value; 
			},
			stop: function(event, ui) {
				document.getElementById("wl_pct_client_edit").value = ui.value;
			}
		});
		//register_event_ssid_edit
		$( "#slider_ssid_edit" ).slider({
			orientation: "horizontal",
			range: "min",
			min: 1,
			max: 100,
			value: 1,
			slide: function(event, ui) {
				document.getElementById("wl_pct_ssid_edit").value = ui.value; 
			},
			stop: function(event, ui) {
				document.getElementById("wl_pct_ssid_edit").value = ui.value;
			}
		}); 
	});
}
function register_event_ssid() {
	Object.keys(wl_atf_ssid_list_array).forEach(function(key) {
		var setted_pct = parseInt(wl_atf_ssid_list_array[key]);
		var setted_ssid = key;
		var slider_id = key.replace("wl", "slider_ssid");
		var input_id = "";
		var index = "";
		var start_value = 0;
		var rest_value = 0;
		$(function() {
			$( "#" + slider_id + "" ).slider({
				orientation: "horizontal",
				range: "min",
				min: 1,
				max: 100,
				value: setted_pct,
				start: function(event, ui) {
						start_value = ui.value;
						rest_value = parseInt(document.getElementById("rest_of_ssid").innerHTML);
				},
				slide:function(event, ui) {
					if(ui.handle.offsetParent != null) {
						input_id = ui.handle.offsetParent.id.replace("slider", "wl_pct_edit");
						var slide_value = parseInt(ui.value) - parseInt(start_value);
						var reset_value_temp = parseInt(rest_value) - slide_value;
						if(reset_value_temp < 0)
							return false;
						document.getElementById(input_id).value = ui.value;
						document.getElementById("rest_of_ssid").innerHTML = reset_value_temp;
					} 
				},
				stop:function(event, ui) {
					if(ui.handle.offsetParent != null) {
						input_id = ui.handle.offsetParent.id.replace("slider", "wl_pct_edit");
						document.getElementById(input_id).value = ui.value;
						index = ui.handle.offsetParent.id.replace("slider_ssid", "wl");
						updateRestSSIDPct(document.getElementById(input_id), index);
					}
				}
			}); 
		});
	});
}
function register_event_client(_list) {
	var getSettedPCT = function(_index) {
		var setted_pct = 0;
		setted_pct = (wl_atf_sta_list_array[_index] == undefined) ? 1 : parseInt(wl_atf_sta_list_array[_index]);
		return setted_pct;
	};
	var clientMac = "";
	var list_array = _list.split("<");
	var input_id = "";
	var start_value = 0;
	var rest_value = 0;
	for(var i = 0; i < list_array.length; i += 1) {
		if(list_array[i] != "") {
			clientMac = list_array[i].replace(/\_/g,":");
			$(function() {
				$( "#slider_client_" + list_array[i] ).slider({
					orientation: "horizontal",
					range: "min",
					min: 1,
					max: 100,
					value: getSettedPCT(clientMac),
					start: function(event, ui) {
						start_value = ui.value;
						rest_value = parseInt(document.getElementById("rest_of_clients").innerHTML);
					},
					slide: function(event, ui) {
						if(ui.handle.offsetParent != null) {
							input_id = ui.handle.offsetParent.id.replace("slider", "wl_pct_edit");
							var slide_value = parseInt(ui.value) - parseInt(start_value);
							var reset_value_temp = parseInt(rest_value) - slide_value;
							if(reset_value_temp < 0)
								return false;
							document.getElementById(input_id).value = ui.value;
							document.getElementById("rest_of_clients").innerHTML = reset_value_temp;
						}
					},
					stop: function(event, ui) {
						if(ui.handle.offsetParent != null) {
							input_id = ui.handle.offsetParent.id.replace("slider", "wl_pct_edit");
							document.getElementById(input_id).value = ui.value;
							var index = ui.handle.offsetParent.id.replace("slider", "wl");
							clientMac = input_id.replace("wl_pct_edit_client_", "").replace(/\_/g,":");
							updateRestClientPct(document.getElementById(input_id), clientMac);
						}
					}
				}); 
			});
		}
	}
}
function pullWLMACList(obj){	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("WL_MAC_List_Block").style.display = "block";
		document.form.wl_atf_sta_edit.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}
function _change_wl_unit_status(__unit){
	document.titleForm.current_page.value = "Advanced_WAdvanced_Content.asp?af=wl_radio";
	document.titleForm.next_page.value = "Advanced_WAdvanced_Content.asp?af=wl_radio";
	change_wl_unit_status(__unit);
}

function show_wl_atf_by_ssid() {
	var getSettedPCT = function(_index) {
		var setted_pct = 0;
		setted_pct = parseInt(wl_atf_ssid_list_array[_index]);
		return setted_pct;
	};

	var setted_total_pct = 0;
	var code = "";
	var setted_pct = 0;
	code += '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table"  id="wl_atf_by_ssid_table">';
	if(radio_status) {
		code += '<tr><td width="50%" align="center">';
		code += '<div>Rest of SSID</div>';/*untranslated*/
		code += '</td><td width="40%" id="rest_of_ssid">100</td><td width="10%">-</td></tr>';

		setted_pct = getSettedPCT("wl_main");
		wl_atf_ssid_list_array["wl_main"] = setted_pct;
		setted_total_pct += setted_pct;

		if(setted_pct != 0) {
			code += '<tr><td width="50%" align="center">';
			code += '<div><% nvram_get("wl_ssid"); %></div></td>';
			code += '<td width="40%">';
			code += '<div style="float:left;padding-left:15px;"><input type="text" maxlength="3" class="input_3_table" name="wl_pct_edit_ssid_main" id="wl_pct_edit_ssid_main"  onKeyPress="return validator.isNumber(this,event)" onBlur="updateRestSSIDPct(this, \'wl_main\');" autocorrect="off" autocapitalize="off" value=' + setted_pct  + '></div>';
			code += '<div id="slider_ssid_main" class="slider"></div>';
			code += '</td>';
			code +='<td width="10%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow_ssid(this, \'wl_main\');\" value=\"\"/></td></tr>';
		}

		for(var i = 0; i < gn_array.length; i += 1) {
			setted_pct = getSettedPCT("wl_gn" + (i + 1));
			wl_atf_ssid_list_array["wl_gn" + (i + 1)] = setted_pct;
			setted_total_pct += setted_pct;
			if(setted_pct != 0) {
				code += '<tr><td width="50%" align="center">';
				code += '<div>' + gn_array[i][1] + '</div></td>';
				code += '<td width="40%">';
				code += '<div style="float:left;padding-left:15px;"><input type="text" maxlength="3" class="input_3_table" name="wl_pct_edit_ssid_gn' + (i + 1) + '" id="wl_pct_edit_ssid_gn' + (i + 1) + '"  onKeyPress="return validator.isNumber(this,event)" onBlur="updateRestSSIDPct(this, \'wl_gn' + (i + 1) + '\');" autocorrect="off" autocapitalize="off" value=' + setted_pct  + '></div>';
				code += '<div id="slider_ssid_gn' + (i + 1) + '" class="slider"></div>';
				code += '</td>';
				code +='<td width="10%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow_ssid(this, \'wl_gn' + (i + 1) + '\');\" value=\"\"/></td></tr>';
			}
		}
	}
	else {
		document.getElementById("tr_wl_pct_ssid_edit").style.display = "none";
		code += '<tr><td width="60%" align="center" style="color:#FFCC00;">';
		code += 'Turn Wi-Fi on to operate the Airtime Fairness.';/*untranslated*/
		code += '&nbsp;';
		code += '<a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(' + parseInt(unit) + ');"><#btn_go#></a>&nbsp;' + wl_nband_title[unit] + '';
		code += '</td></tr>';
	}
	
	code +='</table>';

	document.getElementById("wl_atf_ssid_Block").innerHTML = code;
	if(radio_status)
		document.getElementById("rest_of_ssid").innerHTML = 100 - setted_total_pct;

	register_event_ssid();
}

var over_var = 0;
var isMenuopen = 0;
function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById("WL_MAC_List_Block").style.display="none";
	isMenuopen = 0;
}
function showWLMACList(){
	var code = "";
	var show_macaddr = "";
	var wireless_flag = 0;
	for(i=0;i<clientList.length;i++){
		if(clientList[clientList[i]].isWL != 0){		//0: wired, 1: 2.4GHz, 2: 5GHz, filter clients under current band
			wireless_flag = 1;
			var clientName = (clientList[clientList[i]].nickName == "") ? clientList[clientList[i]].name : clientList[clientList[i]].nickName;
			code += '<a title=' + clientList[i] + '><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientmac(\''+clientList[i]+'\');"><strong>'+clientName+'</strong> ';
			code += ' </div></a>';
		}
	}
			
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	document.getElementById("WL_MAC_List_Block").innerHTML = code;
	
	if(wireless_flag == 0)
		document.getElementById("pull_arrow").style.display = "none";
	else
		document.getElementById("pull_arrow").style.display = "";
}
function setClientmac(macaddr){
	document.form.wl_atf_sta_edit.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

function show_wl_atf_by_client() {
	var register_event_list_client = "";
	var setted_total_pct = 0;
	var code = "";
	code += '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table"  id="wl_atf_by_client_table">';
	code += '<tr><td width="50%" align="center">';
	code += '<div>Rest of Clients</div>';/*untranslated*/
	code += '</td><td width="40%" id="rest_of_clients">100</td><td width="10%">-</td></tr>';
	if(Object.keys(wl_atf_sta_list_array).length != 0) {
		//user icon
		var userIconBase64 = "NoIcon";
		var clientName, deviceType, deviceVender;
		Object.keys(wl_atf_sta_list_array).forEach(function(key) {
			setted_total_pct += parseInt(wl_atf_sta_list_array[key]);
			var clientMac = key;
			if(clientList[clientMac]) {
				clientName = (clientList[clientMac].nickName == "") ? clientList[clientMac].name : clientList[clientMac].nickName;
				deviceType = clientList[clientMac].type;
				deviceVender = clientList[clientMac].dpiVender;
			}
			else {
				clientName = "New device";
				deviceType = 0;
				deviceVender = "";
			}
			code +='<tr id="row_'+clientMac+'">';
			code +='<td width="50%" align="center">';
			code += '<table style="width:100%;"><tr><td style="width:30%;height:56px;border:0px;">';
			if(clientList[clientMac] == undefined) {
				code += '<div style="height:56px;float:right;" class="clientIcon type0" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'\', \'\', \'ATF\')"></div>';
			}
			else {
				if(usericon_support) {
					userIconBase64 = getUploadIcon(clientMac.replace(/\:/g, ""));
				}
				if(userIconBase64 != "NoIcon") {
					code += '<div style="width:81px;text-align:center;float:right;" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'\', \'\', \'ATF\')"><img class="imgUserIcon_card" src="' + userIconBase64 + '"></div>';
				}
				else if( (deviceType != "0" && deviceType != "6") || deviceVender == "") {
					code += '<div style="height:56px;float:right;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' +clientMac + '\', this, \'\', \'\', \'ATF\')"></div>';
				}
				else if(deviceVender != "" ) {
					var venderIconClassName = getVenderIconClassName(deviceVender.toLowerCase());
					if(venderIconClassName != "") {
						code += '<div style="height:56px;float:right;" class="venderIcon ' + venderIconClassName + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'\', \'\', \'ATF\')"></div>';
					}
					else {
						code += '<div style="height:56px;float:right;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'\', \'\', \'ATF\')"></div>';
					}
				}
			}
			code += '</td><td style="width:70%;border:0px;">';
			code += '<div>' + clientName + '</div>';
			code += '</td></tr></table>';
			code += '</td>';
			code += '<td width="40%">';
			code += '<div style="float:left;padding-left:15px;"><input type="text" maxlength="3" class="input_3_table" id="wl_pct_edit_client_' + clientMac.replace(/\:/g,"_") + '" name="wl_pct_edit_client_' + clientMac.replace(/\:/g,"_") + '" value=' + wl_atf_sta_list_array[clientMac] + ' onKeyPress="return validator.isNumber(this,event)" onBlur="updateRestClientPct(this, \'' + clientMac + '\');" autocorrect="off" autocapitalize="off"></div>';
			code += '<div id="slider_client_' + clientMac.replace(/\:/g,"_")+ '" class="slider"></div>';
			code += '</td>';
			code +='<td width="10%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow_client(this, \'' + clientMac + '\');\" value=\"\"/></td></tr>';
			register_event_list_client += "<" + clientMac.replace(/\:/g,"_");
		});
	}	
	
	code +='</table>';
	document.getElementById("wl_atf_client_Block").innerHTML = code;
	document.getElementById("rest_of_clients").innerHTML = 100 - setted_total_pct;

	register_event_client(register_event_list_client);
}
function updateRestSSIDPct(_obj, _unit) {
	if(!validator.range(_obj, 1, 100)) {
		return false;
	}

	var ori_value = parseInt(_obj.value);

	wl_atf_ssid_list_array[_unit] = _obj.value;
	var setted_total_pct_temp = 0;
	Object.keys(wl_atf_ssid_list_array).forEach(function(key) {
		var setted_pct = wl_atf_ssid_list_array[key];
		setted_total_pct_temp += parseInt(setted_pct);
	});

	//handle overflow value
	var overflow = setted_total_pct_temp - 100;
	if(overflow > 0) {
		_obj.value = ori_value - overflow;
		wl_atf_ssid_list_array[_unit] = _obj.value;
	}

	show_wl_atf_by_ssid();
}
function updateRestClientPct(_obj, _clientMac) {
	if(!validator.range(_obj, 1, 100)) {
		return false;
	}

	var ori_value = parseInt(_obj.value);

	wl_atf_sta_list_array[_clientMac] = _obj.value;
	var setted_total_pct_temp = 0;
	Object.keys(wl_atf_sta_list_array).forEach(function(key) {
		var setted_pct = wl_atf_sta_list_array[key];
		setted_total_pct_temp += parseInt(setted_pct);
	});
	//handle overflow value
	var overflow = setted_total_pct_temp - 100;
	if(overflow > 0) {
		_obj.value = ori_value - overflow;
		wl_atf_sta_list_array[_clientMac] = _obj.value;
	}

	show_wl_atf_by_client();
}
function updateSlider(_obj, _mode) {
	if(_obj.value == "") {
		_obj.value = 1;
	}

	if(!validator.range(_obj, 1, 100)) {
		return false;
	}

	document.getElementById('slider_' + _mode + '_edit').children[0].style.width = _obj.value + "%";
	document.getElementById('slider_' + _mode + '_edit').children[1].style.left = _obj.value + "%";
}
function deleteRow_client(r, delMac){
	var i = r.parentNode.parentNode.rowIndex;
	delete wl_atf_sta_list_array[delMac];
	document.getElementById('wl_atf_by_client_table').deleteRow(i);

	show_wl_atf_by_client();
}
function deleteRow_ssid(r, _interface){
	var i = r.parentNode.parentNode.rowIndex;
	wl_atf_ssid_list_array[_interface] = 0;
	document.getElementById('wl_atf_by_ssid_table').deleteRow(i);

	show_wl_atf_by_ssid();
}
function addRow_client(upper) {
	var rule_num = document.getElementById('wl_atf_by_client_table').rows.length;
	var _macObj = document.form.wl_atf_sta_edit;
	var _pct = document.form.wl_pct_client_edit;

	var mac = _macObj.value.toUpperCase();
	if(rule_num >= upper) {
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}	
	
	if(mac == "") {
		alert("<#JS_fieldblank#>");
		_macObj.focus();
		_macObj.select();			
		return false;
	}
	else if(!check_macaddr(_macObj, check_hwaddr_flag(_macObj))) {
		_macObj.focus();
		_macObj.select();	
		return false;	
	}

	if(wl_atf_sta_list_array[mac] != null) {
		alert("<#JS_duplicate#>");
		return false;
	}		
	
	if(_pct.value == "") {
		alert("<#JS_fieldblank#>");
		_pct.focus();
		_pct.select();
		return false;
	}
	else if(!validator.range(_pct, 1, 100)) {
		return false;
	}

	var setted_total_pct_temp = 0;
	Object.keys(wl_atf_sta_list_array).forEach(function(key) {
		var setted_mac = key;
		var setted_pct = wl_atf_sta_list_array[key];
		setted_total_pct_temp += parseInt(setted_pct);
	});
	setted_total_pct_temp = setted_total_pct_temp +  parseInt(_pct.value.trim());

	if(setted_total_pct_temp > 100) {
		alert("Sum of Client is greater than 100");/*untranslated*/
		return false;
	}

	wl_atf_sta_list_array[mac] = _pct.value.trim()

	_macObj.value = "";
	_pct.value = "1";
	$( "#slider_client_edit" ).slider("option", "value", 1);

	document.getElementById('slider_client_edit').children[0].style.width = "1%";
	document.getElementById('slider_client_edit').children[1].style.left = "1%";

	show_wl_atf_by_client();
}
function addRow_ssid() {
	var _pct = document.form.wl_pct_ssid_edit;
	var _wl_interface = document.form.wl_interface;

	if(wl_atf_ssid_list_array[_wl_interface.value] != 0) {
		alert("<#JS_duplicate#>");
		return false;
	}		
	
	if(_pct.value == "") {
		alert("<#JS_fieldblank#>");
		_pct.focus();
		_pct.select();
		return false;
	}
	else if(!validator.range(_pct, 1, 100)) {
		return false;
	}

	var setted_total_pct_temp = 0;
	Object.keys(wl_atf_ssid_list_array).forEach(function(key) {
		var setted_mac = key;
		var setted_pct = wl_atf_ssid_list_array[key];
		setted_total_pct_temp += parseInt(setted_pct);
	});
	setted_total_pct_temp = setted_total_pct_temp +  parseInt(_pct.value.trim());

	if(setted_total_pct_temp > 100) {
		alert("Sum of Client is greater than 100");/*untranslated*/
		return false;
	}

	wl_atf_ssid_list_array[_wl_interface.value] = _pct.value.trim()

	_wl_interface.value = document.form.wl_interface.options[0].value
	_pct.value = "1";
	$( "#slider_ssid_edit" ).slider("option", "value", 1);

	document.getElementById('slider_ssid_edit').children[0].style.width = "1%";
	document.getElementById('slider_ssid_edit').children[1].style.left = "1%";

	show_wl_atf_by_ssid();
}
function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		document.getElementById("check_mac").style.display = "";
		return false;
	}else if(flag ==2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		document.getElementById("check_mac").style.display = "";
		return false;		
	}else{	
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	}	
}
function change_wl_atf_mode(_obj) {
	document.getElementById("bySSIDBlock").style.display = "none";
	document.getElementById("byClientBlock").style.display = "none";
	var mode = parseInt(_obj.value);
	switch(mode) {
		case 0 :
			document.getElementById("byClientBlock").style.display = "";
			show_wl_atf_by_client();
			break;
		case 1 :
			document.getElementById("bySSIDBlock").style.display = "";
			show_wl_atf_by_ssid();
			break;
		case 2 :
			document.getElementById("byClientBlock").style.display = "";
			show_wl_atf_by_client();
			document.getElementById("bySSIDBlock").style.display = "";
			show_wl_atf_by_ssid();
			break;	
	}	
}
function applyRule() {
	document.form.wl_atf_ssid.disabled = true;
	document.form.wl_atf_sta.disabled = true;
	var atf_mode = parseInt(document.form.wl_atf_mode.value);

	var wl_atf_ssid_temp = "";
	var validForm_ssid = function(_mode) {
		var setted_total_pct_temp = 0;
		wl_atf_ssid_list_array.sort();
		Object.keys(wl_atf_ssid_list_array).sort().forEach(function(key) {
			var setted_ssid = key;
			var setted_pct = wl_atf_ssid_list_array[key];
			setted_total_pct_temp += parseInt(setted_pct);
			if(setted_ssid == "wl_main") {
				wl_atf_ssid_temp = "<" + setted_pct + wl_atf_ssid_temp;
			}
			else {
				wl_atf_ssid_temp += "<" + setted_pct;
			}
		});
	
		if(setted_total_pct_temp > 100) {
			alert("Sum of SSID is greater than 100");/*untranslated*/
			return false;
		}

		return true;
	};
	var wl_atf_sta_temp = "";
	var validForm_client = function() {
		var setted_total_pct_temp = 0;
		Object.keys(wl_atf_sta_list_array).forEach(function(key) {
			var setted_mac = key;
			var setted_pct = wl_atf_sta_list_array[key];
			setted_total_pct_temp += parseInt(setted_pct);
			wl_atf_sta_temp += "<" + setted_mac + ">" + setted_pct;
		});

		if(setted_total_pct_temp > 100) {
			alert("Sum of Client is greater than 100");/*untranslated*/
			return false;
		}

		return true;
	};

	switch(atf_mode) {
		case 0 :
			if(validForm_client()) {
				document.form.wl_atf_sta.disabled = false;
				document.form.wl_atf_sta.value = wl_atf_sta_temp;	
			}
			break;
		case 1 :
			if(validForm_ssid()) {
				document.form.wl_atf_ssid.disabled = false;
				document.form.wl_atf_ssid.value = wl_atf_ssid_temp;
			}
			break;
		case 2 :
			if(validForm_client() && validForm_ssid()) {
				document.form.wl_atf_ssid.disabled = false;
				document.form.wl_atf_ssid.value = wl_atf_ssid_temp;
				document.form.wl_atf_sta.disabled = false;
				document.form.wl_atf_sta.value = wl_atf_sta_temp;
			}
			break;
	}

	document.form.submit();
}
</script>
</head>
<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Airtime_Fairness.asp">
<input type="hidden" name="next_page" value="Advanced_Airtime_Fairness.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl_atf_sta" value="<% nvram_get("wl_atf_sta"); %>">
<input type="hidden" name="wl_atf_ssid" value="<% nvram_get("wl_atf_ssid"); %>">

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
			<div id="captive_portal_setting" class="captive_portal_setting_bg"></div>
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td align="left" valign="top">
						<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#menu5_1#> - Airtime Fairness<!--untranslated--></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">Airtime Faimess gives equal amounts of air time(instead of equal number of frames) to each client regardless of its theoretical data rate. This will ensure highter download speed to latest devices when slower devices are connected to the same AP. In ASUS <#Web_Title2#>, you can also define VIP client to get the a guarantied airtime.<!--untranslated--></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
										</thead>
										<tr>
											<th>Enable Air Time Fainess<!--untranslated--></th>
											<td>
												<input type="radio" name="wl_atf" id="wl_atf_en" value="1" <% nvram_match("wl_atf", "1", "checked"); %>>
												<label for="wl_atf_en"><#checkbox_Yes#></label>
												<input type="radio" name="wl_atf" id="wl_atf_dis" value="0" <% nvram_match("wl_atf", "0", "checked"); %>>
												<label for="wl_atf_dis"><#checkbox_No#></label>
											</td>
										</tr>
										<tr id="wl_unit_field">
											<th><#Interface#></th>
											<td>
												<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
												<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
												<option class="content_input_fd" value="1" <% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
											</select>
											</td>
										</tr>
										<tr>
											<th>Roaming assistant<!--untranslated--></th>
											<td>
												<select name="wl_atf_mode" class="input_option" onChange="change_wl_atf_mode(this);">
												<option class="content_input_fd" value="0" <% nvram_match("wl_atf_mode", "0","selected"); %>>By Client<!--untranslated--></option>
												<option class="content_input_fd" value="1" <% nvram_match("wl_atf_mode", "1","selected"); %>>By SSID<!--untranslated--></option>
												<option class="content_input_fd" value="2" <% nvram_match("wl_atf_mode", "2","selected"); %>>By SSID and Client<!--untranslated--></option>
											</select>
											</td>
										</tr>
									</table>
									<div id="byClientBlock" style="display:none;">
										<table id="byClientTable" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
											<thead>
											<tr>
												<td colspan="3">By Client Device<!--untranslated-->&nbsp;(<#List_limit#>&nbsp;32)</td>
											</tr>
											</thead>
											<tr>
												<th width="50%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">Client Name (MAC address)<!--untranslated--></th> 
												<th width="40%">Reserved Percentage<!--untranslated--></th>
												<th width="10%"><#list_add_delete#></th>
											</tr>
											<tr>
												<td width="50%">
													<input type="text" maxlength="17" class="input_macaddr_table" name="wl_atf_sta_edit" onKeyPress="return validator.isHWAddr(this,event)" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off" placeholder="ex: <% nvram_get("lan_hwaddr"); %>" style="width:255px;">
													<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;display:none;" onclick="pullWLMACList(this);" title="<#select_wireless_MAC#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
													<div id="WL_MAC_List_Block" class="WL_MAC_Block"></div>
												</td>
												<td width="40%">
													<div style="float:left;padding-left:15px;">
														<input type="text" maxlength="3" class="input_3_table" name="wl_pct_client_edit" id="wl_pct_client_edit" onKeyPress="return validator.isNumber(this,event)" onBlur="updateSlider(this, 'client');" autocorrect="off" autocapitalize="off" value="1">
													</div>
													<div id="slider_client_edit" class="slider"></div>
												</td>
												<td width="10%">
													<input type="button" class="add_btn" onClick="addRow_client(32);" value="">
												</td>
											</tr>
										</table>
										<div id="wl_atf_client_Block"></div>
									</div>
									<div id="bySSIDBlock" style="display:none;">
										<table id="bySSIDTable" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
											<thead>
											<tr>
												<td colspan="3">By WLAN Network (SSID)<!--untranslated--></td>
											</tr>
											</thead>
											<tr>
												<th width="50%"><#QIS_finish_wireless_item1#></th> 
												<th width="40%">Reserved Percentage<!--untranslated--></th>
												<th width="10%"><#list_add_delete#></th>
											</tr>
											<tr id="tr_wl_pct_ssid_edit">
												<td width="50%">
													<select name="wl_interface" class="input_option"></select>
												</td>
												<td width="40%">
													<div style="float:left;padding-left:15px;">
														<input type="text" maxlength="3" class="input_3_table" name="wl_pct_ssid_edit" id="wl_pct_ssid_edit" onKeyPress="return validator.isNumber(this,event)" onBlur="updateSlider(this, 'ssid');" autocorrect="off" autocapitalize="off" value="1">
													</div>
													<div id="slider_ssid_edit" class="slider"></div>
												</td>
												<td width="10%">
													<input type="button" class="add_btn" onClick="addRow_ssid();" value="">
												</td>
											</tr>
										</table>
										<div id="wl_atf_ssid_Block"></div>
									</div>
									<div id="submitBtn" class="apply_gen">
										<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
									</div>	
								</td>
							</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
<!--===================================End of Main Content===========================================-->
</form>
<div id="footer"></div>
</body>
</html>
