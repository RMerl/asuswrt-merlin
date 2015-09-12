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
<title><#Web_Title#> - <#menu5_1_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
#pull_arrow{
 	float:center;
 	cursor:pointer;
 	border:2px outset #EFEFEF;
 	background-color:#CCC;
 	padding:3px 2px 4px 0px;
}
#WDSAPList{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	margin-top:24px;
	margin-left:220px;
	*margin-left:-150px;
	width:255px;
	*width:259px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#WDSAPList div{
	background-color:#576D73;
	height:20px;
	line-height:20px;
	text-decoration:none;
	padding-left:2px;
}
#WDSAPList a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
#WDSAPList div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/JavaScript" src="/js/jquery.js"></script>
<script>
<% wl_get_parameter(); %>

var wl_wdslist_array = '<% nvram_get("wl_wdslist"); %>';
var wds_aplist = "";


function initial(){
	show_menu();

	regen_band(document.form.wl_unit);

	if(based_modelid == "RT-AC87U" && document.form.wl_unit[1].selected == true){
		document.getElementById("wds_mode_field").style.display = "none";
		document.form.wl_mode_x.value="2";
	}

	change_wireless_bridge(document.form.wl_mode_x.value);	

	if((sw_mode == 2 || sw_mode == 4) && '<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>'){
		for(var i=5; i>=3; i--)
			document.getElementById("MainTable1").deleteRow(i);
		for(var i=2; i>=0; i--)
			document.getElementById("MainTable2").deleteRow(i);
		
		document.getElementById("repeaterModeHint").style.display = "";
		document.getElementById("wl_wdslist_Block").style.display = "none";
		document.getElementById("submitBtn").style.display = "none";
	}
	else{
		show_wl_wdslist();
	}
		
	if(!band5g_support){
		document.getElementById("wl_5g_mac").style.display = "none";
		document.getElementById("wl_unit_field").style.display = "none";
	}
	
	if(wl_info.band5g_2_support){
		document.getElementById("wl_5g_mac_2").style.display = "";
		document.getElementById("wl_5g_mac_th1").innerHTML = "5GHz-1 MAC";
	}

	wl_bwch_hint();
	if(based_modelid == "RT-AC55U" || based_modelid == "RT-AC55UHP" || based_modelid == "4G-AC55U")
		wl_vht_hint();
	setTimeout("wds_scan();", 500);
}

function show_wl_wdslist(){
	var wl_wdslist_row = wl_wdslist_array.split('&#60');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="wl_wdslist_table">'; 
	if(wl_wdslist_row.length == 1){
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	}		
	else{
		for(var i = 1; i < wl_wdslist_row.length; i++){
			code +='<tr id="row'+i+'">';
			code +='<td width="80%">'+ wl_wdslist_row[i] +'</td>';	
			code +='<td width="20%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow(this);\" value=\"\"/></td></tr>';
		}
	}
	
	code +='</tr></table>';	
	document.getElementById("wl_wdslist_Block").innerHTML = code;
}

function deleteRow(r){
	var i=r.parentNode.parentNode.rowIndex;
	document.getElementById('wl_wdslist_table').deleteRow(i);
	var wl_wdslist_value = "";
	for(i=0; i< document.getElementById('wl_wdslist_table').rows.length; i++){
		wl_wdslist_value += "&#60";
		wl_wdslist_value += document.getElementById('wl_wdslist_table').rows[i].cells[0].innerHTML;
	}
	
	wl_wdslist_array = wl_wdslist_value;
	if(wl_wdslist_array == "")
		show_wl_wdslist();
}

function addRow(obj, upper){
	var rule_num = document.getElementById('wl_wdslist_table').rows.length;
	var item_num = document.getElementById('wl_wdslist_table').rows[0].cells.length;
	
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}		
		
	if(obj.value==""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();
		return false;
	}else if (!check_macaddr(obj, check_hwaddr_flag(obj))){
		obj.focus();
		obj.select();		
		return false;
	}
		
		//Viz check same rule
	for(i=0; i<rule_num; i++){
		for(j=0; j<item_num-1; j++){	
			if(obj.value.toLowerCase() == document.getElementById('wl_wdslist_table').rows[i].cells[j].innerHTML.toLowerCase()){
				alert("<#JS_duplicate#>");
				return false;
			}	
		}		
	}
		
	wl_wdslist_array += "&#60";
	wl_wdslist_array += obj.value;
	obj.value = "";
	show_wl_wdslist();
}

function applyRule(){
	var rule_num = document.getElementById('wl_wdslist_table').rows.length;
	var item_num = document.getElementById('wl_wdslist_table').rows[0].cells.length;
	var tmp_value = "";
	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){	
			tmp_value += document.getElementById('wl_wdslist_table').rows[i].cells[j].innerHTML;
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<"){
		tmp_value = "";	
	}	
	
	document.form.wl_wdslist.value = tmp_value;
	if(document.form.wl_mode_x.value == "0"){
		inputRCtrl1(document.form.wl_wdsapply_x, 1);
		inputRCtrl2(document.form.wl_wdsapply_x, 1);
		if(based_modelid == "RT-AC55U" || based_modelid == "RT-AC55UHP" || based_modelid == "4G-AC55U")
		{
			inputRCtrl1(document.form.wl_wds_vht, 1);
			inputRCtrl2(document.form.wl_wds_vht, 1);
		}   
	}
	
	if(document.form.wl_mode_x.value == "1"){
		document.form.wl_wdsapply_x.value = "1";
	}
		
	if(wl6_support){
		document.form.action_wait.value = 8;
	}
	else{
		document.form.action_wait.value = 3;
	}
		
	showLoading();	
	document.form.submit();
}

function done_validating(action){
	refreshpage();
}

/*------------ Site Survey Start -----------------*/
function wds_scan(){
	var ajaxURL = '/wds_aplist_2g.asp';
	if('<% nvram_get("wl_unit"); %>' == '1')
		var ajaxURL = '/wds_aplist_5g.asp';
	else if('<% nvram_get("wl_unit"); %>' == '2')
		var ajaxURL = '/wds_aplist_5g_2.asp';

	$.ajax({
		url: ajaxURL,
		dataType: 'script',
		
		error: function(xhr){
			setTimeout("wds_scan();", 1000);
		},
		success: function(response){
			showLANIPList();
		}
	});
}

function setClientIP(num){
	document.form.wl_wdslist_0.value = wds_aplist[num][1];
	hideClients_Block();
	over_var = 0;
}

function rescan(){
	wds_aplist = "";
	showLANIPList()
	wds_scan();
}

function showLANIPList(){
	var code = "";
	var show_name = "";
	if(wds_aplist != ""){
		for(var i = 0; i < wds_aplist.length ; i++){
			wds_aplist[i][0] = decodeURIComponent(wds_aplist[i][0]);
			if(wds_aplist[i][0] && wds_aplist[i][0].length > 12)
				show_name = wds_aplist[i][0].substring(0, 10) + "..";
			else
				show_name = wds_aplist[i][0];
			
			if(wds_aplist[i][1]){
				code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP('+i+');"><strong>'+show_name+'</strong>';
				if(show_name && show_name.length > 0)
					code += '( '+wds_aplist[i][1]+')';
				else
					code += wds_aplist[i][1];
				
				code += ' </div></a>';
			}
		}
		code += '<div style="text-decoration:underline;font-weight:bolder;" onclick="rescan();"><#AP_survey#></div>';
	}
	else{
		code += '<div style="width:98px"><img height="15px" style="margin-left:5px;margin-top:2px;" src="/images/InternetScan.gif"></div>';
	}

	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	document.getElementById("WDSAPList").innerHTML = code;
}

function pullLANIPList(obj){	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("WDSAPList").style.display = 'block';		
		document.form.wl_wdslist_0.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}
var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('WDSAPList').style.display='none';
	isMenuopen = 0;
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<br><br><#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		document.getElementById("check_mac").style.display = "";
		return false;	
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<br><br><#IPConnection_x_illegal_mac#>";
		document.getElementById("check_mac").style.display = "";
		return false;			
	}else{
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	} 	
}
/*---------- Site Survey End -----------------*/


/* Control display chanspec hint when wl_bw=0(20/40/80) or wl_chanspec=0(Auto) */
function wl_bwch_hint(){ 
	if(wl6_support){
		document.getElementById("wl_bw_hint").style.display=(document.form.wl_bw.value == "0") ? "" : "none";
		document.getElementById("wl_ch_hint").style.display=(document.form.wl_chanspec.value == "0") ? "" : "none";
	}
	else{
		document.getElementById("wl_bw_hint").style.display=(document.form.wl_bw.value == "1") ? "" : "none";
		document.getElementById("wl_ch_hint").style.display=(document.form.wl_channel.value == "0") ? "" : "none";
	}
}

function wl_vht_hint(){ 
	var u='<% nvram_get("wl_unit"); %>';	
   	if(u=='1')
	   document.getElementById("wlvht").style.display ="";  
   	else
	   document.getElementById("wlvht").style.display ="none";  
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" value="<% nvram_get("productid"); %>" name="productid" >
<input type="hidden" name="current_page" value="Advanced_WMode_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WMode_Content.asp">
<input type="hidden" name="group_id" value="wl_wdslist">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="8">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" maxlength="15" size="15" name="x_RegulatoryDomain" value="<% nvram_get("x_RegulatoryDomain"); %>" readonly="1">
<input type="hidden" name="wl_wdsnum_x_0" value="<% nvram_get("wl_wdsnum_x"); %>" readonly="1">  
<input type="hidden" name="wl_nmode_x" value='<% nvram_get("wl_nmode_x"); %>'>
<input type="hidden" name="wl_bw" value='<% nvram_get("wl_bw"); %>'>
<input type="hidden" name="wl_channel" value='<% nvram_get("wl_channel"); %>'>
<input type="hidden" name="wl_chanspec" value='<% nvram_get("wl_chanspec"); %>'>
<input type="hidden" name="wl_nctrlsb_old" value='<% nvram_get("wl_nctrlsb"); %>'>
<input type="hidden" name="wl_wdslist" value=''>
<input type="hidden" name="wl_subunit" value="-1">
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
					<td align="left" valign="top" >		
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#menu5_1#> - <#menu5_1_3#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc"><#WLANConfig11b_display1_sectiondesc#></div>
									<div class="formfontdesc" style="color:#FFCC00;"><#ADSL_FW_note#><#WLANConfig11b_display2_sectiondesc#></div>
									<div class="formfontdesc"><#WLANConfig11b_display3_sectiondesc#>
										<ol>
											<li><#WLANConfig11b_display31_sectiondesc#></li>
											<li><#WLANConfig11b_display32_sectiondesc#></li>
											<li><#WLANConfig11b_display33_sectiondesc#></li>
											<li><#WLANConfig11b_display34_sectiondesc#></li>					
										</ol>					
									</div>
									<div id="wl_bw_hint" style="font-size:13px;font-family: Arial, Helvetica, sans-serif;color:#FC0;margin-left:28px;"><#WLANConfig11b_display41_sectiondesc#></div>
									<div id="wl_ch_hint" style="font-size:13px;font-family: Arial, Helvetica, sans-serif;color:#FC0;margin-left:28px;"><#WLANConfig11b_display42_sectiondesc#></div>	
									<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
										</thead>		  
										<tr>
											<th>2.4GHz MAC</th>
											<td>
												<input type="text" maxlength="17" class="input_20_table" id="wl0_hwaddr" name="wl0_hwaddr" value="<% nvram_get("wl0_hwaddr"); %>" readonly autocorrect="off" autocapitalize="off">
											</td>		
										</tr>					
										<tr id="wl_5g_mac">
											<th id="wl_5g_mac_th1">5GHz MAC</th>
											<td>
												<input type="text" maxlength="17" class="input_20_table" id="wl1_hwaddr" name="wl1_hwaddr" value="<% nvram_get("wl1_hwaddr"); %>" readonly autocorrect="off" autocapitalize="off">
											</td>		
										</tr>	
										<tr id="wl_5g_mac_2" style="display:none">
											<th>5GHz-2 MAC</th>
											<td>
												<input type="text" maxlength="17" class="input_20_table" id="wl2_hwaddr" name="wl2_hwaddr" value="<% nvram_get("wl2_hwaddr"); %>" readonly autocorrect="off" autocapitalize="off">
											</td>		
										</tr>			  
										<tr id="wl_unit_field">
											<th><#Interface#></th>
											<td>
												<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
													<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
													<option class="content_input_fd" value="1"<% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
												</select>			
											</td>
										</tr>
										<tr id="repeaterModeHint" style="display:none;">
											<td colspan="2" style="color:#FFCC00;height:30px;" align="center"><#page_not_support_mode_hint#></td>
										</tr>			
										<tr id="wds_mode_field">
											<th align="right">
												<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(1,1);">
												<#WLANConfig11b_x_APMode_itemname#></a>
											</th>
											<td>
												<select name="wl_mode_x" class="input_option" onChange="change_wireless_bridge(this.value);">
													<option value="0" <% nvram_match("wl_mode_x", "0","selected"); %>><#WLANConfig11b_x_APMode_option0#></option>
													<option value="1" <% nvram_match("wl_mode_x", "1","selected"); %>><#WLANConfig11b_x_APMode_option1#></option>
													<option value="2" <% nvram_match("wl_mode_x", "2","selected"); %>><#WLANConfig11b_x_APMode_option2#></option>
												</select>
											</td>
										</tr>
										<tr id="wlvht" class="wlvht" style="display:none;">
											<th>VHT MODE</th>
											<td>
												<input type="radio" value="1" name="wl_wds_vht" class="input" <% nvram_match("wl_wds_vht", "1", "checked"); %>><#checkbox_Yes#>
												<input type="radio" value="0" name="wl_wds_vht" class="input" <% nvram_match("wl_wds_vht", "0", "checked"); %>><#checkbox_No#>
											</td>
										</tr>	
										<tr>
											<th align="right">
												<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(1,3);">
												<#WLANConfig11b_x_BRApply_itemname#>
												</a>
											</th>
											<td>
												<input type="radio" value="1" name="wl_wdsapply_x" class="input" <% nvram_match("wl_wdsapply_x", "1", "checked"); %>><#checkbox_Yes#>
												<input type="radio" value="0" name="wl_wdsapply_x" class="input" <% nvram_match("wl_wdsapply_x", "0", "checked"); %>><#checkbox_No#>
											</td>
										</tr>			
									</table>	
									<table id="MainTable2" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable_table">
										<thead>
										<tr>
											<td colspan="4"><#WLANConfig11b_RBRList_groupitemdesc#>&nbsp;(<#List_limit#>&nbsp;4)</td>
										</tr>
										</thead>		
										<tr>
											<th width="80%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);"><#WLANConfig11b_RBRList_groupitemdesc#></th>
											<th class="edit_table" width="20%"><#list_add_delete#></th>		
										</tr>
										<tr>
											<td width="80%">
												<input type="text" style="margin-left:220px;float:left;" maxlength="17" class="input_macaddr_table" name="wl_wdslist_0" onKeyPress="return validator.isHWAddr(this,event)" autocorrect="off" autocapitalize="off">
												<img style="float:left;" id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_AP#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
												<div id="WDSAPList" class="WDSAPList">
													<div style="width:98px">
														<img height="15px" style="margin-left:5px;margin-top:2px;" src="/images/InternetScan.gif">
													</div>
												</div>
											</td>
											<td width="20%">	
												<input type="button" class="add_btn" onClick="addRow(document.form.wl_wdslist_0, 4);" value="">
											</td>
										</tr>
									</table>       		
									<div id="wl_wdslist_Block"></div>     		
									<div class="apply_gen">
										<input class="button_gen" id="submitBtn" onclick="applyRule()" type="button" value="<#CTL_apply#>" />
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
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
