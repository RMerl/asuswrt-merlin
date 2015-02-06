<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - Web Protector</title>
<link rel="stylesheet" type="text/css" href="ParentalControl.css">
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style>
#switch_menu{
	text-align:right
}
#switch_menu span{
	border-radius:4px;
	font-size:16px;
	padding:3px;
}
/*#switch_menu span:hover{
	box-shadow:0px 0px 5px 3px white;
	background-color:#97CBFF;
}*/
.click:hover{
	box-shadow:0px 0px 5px 3px white;
	background-color:#97CBFF;
}
.clicked{
	background-color:#2894FF;
	box-shadow:0px 0px 5px 3px white;

}
.click{
	background:#8E8E8E;
}
</style>
<script>
var $j = jQuery.noConflict();
window.onresize = cal_agreement_block;
var client_list_array = '<% get_client_detail_info(); %>';	
var client_list_row = client_list_array.split('<');
var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var custom_name_row = custom_name.split('<');
var apps_filter = "<% nvram_get("wrs_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");
var wrs_filter = "<% nvram_get("wrs_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");
var wrs_app_filter = "<% nvram_get("wrs_app_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");

var wrs_id_array = [["1,2,3,4,5,6,8", "9,10,14,15,16,25,26", "11"],
					["24", "51", "53,89", "42", ""],
					["56,70,71", "57"],
					["", "69", "23"]];
				
var apps_id_array = [["22", "", ""],
					 ["", "0,6,15", "", "", "21"],
					 ["3", "1"],
					 ["8", "4", ""]];					 

var over_var = 0;
var isMenuopen = 0;
var curState = '<% nvram_get("wrs_enable"); %>';

function initial(){
	show_menu();
	//show_inner_tab();
	document.getElementById("_AiProtection_HomeSecurity").innerHTML = '<table><tbody><tr><td><div class="_AiProtection_HomeSecurity"></div></td><td><div style="width:120px;">AiProtection</div></td></tr></tbody></table>';
	document.getElementById("_AiProtection_HomeSecurity").className = "menu_clicked";
	translate_category_id();
	genMain_table();
	if('<% nvram_get("wrs_enable"); %>' == 1)
		showhide("list_table",1);
	else
		showhide("list_table",0);
	
	showLANIPList();	
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function hideClients_Block(){
	$("pull_arrow").src = "/images/arrow-down.gif";
	$('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;

}

function setClientIP(devname, macaddr){
	document.form.PC_devicename.value = devname;
	document.form.PC_mac.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

var previous_obj = "";
function show_subCategory(obj){
	var sub_category_state = obj.className
	var sub_category = $j(obj).siblings()[1];
	var category_desc = $j(obj).siblings()[2];
	
	if(sub_category_state == "closed"){	
		sub_category.style.display = "";
		if(category_desc)
			category_desc.style.display = "none";
		
		obj.setAttribute("class", "opened");
		if(previous_obj != ""){		//Hide another category
			$j(previous_obj).siblings()[1].style.display = "none";
			if($j(previous_obj).siblings()[2])
				$j(previous_obj).siblings()[2].style.display = "";
			
			previous_obj.setAttribute("class", "closed");	
		}

		previous_obj = obj;
	}
	else{		
		sub_category.style.display = "none";
		if($j(previous_obj).siblings()[2])
			$j(previous_obj).siblings()[2].style.display = "";
		
		obj.setAttribute("class", "closed");		
		if($j(previous_obj).siblings()[1] = sub_category){			//To handle open, close the same category
			$j(previous_obj).siblings()[1] = "";
			previous_obj = "";	
		}
	}
}
	
function set_category(obj, flag){
	var checked_flag = 0;

	if(flag == 0){
		var children_length = $j(obj).siblings()[1].children.length;
		if(obj.checked == true){
			for(i=0; i<children_length; i++){
				$j(obj).siblings()[1].children[i].children[1].checked = true;	
			}
		}
		else{
			for(i=0; i<children_length; i++){
				$j(obj).siblings()[1].children[i].children[1].checked = false;	
			}	
		}
	}
	else{
		var parent_category = $j(obj.parentNode.parentNode).siblings()[1];
		var sibling = $j(obj.parentNode).siblings();
		var sibling_length = $j(obj.parentNode).siblings().length;
		
		if(obj.checked == true){
			if(parent_category.checked != true)
				parent_category.checked = true;
		}
		else{
			for(i=0; i<sibling_length; i++){
				if(sibling[i].children[1].checked == true){
					checked_flag = 1;			
				}
			}

			if(checked_flag == 0){
				parent_category.checked =false;		
			}	
		}
	}	
}

function deleteRow_main(obj){
	 var item_index = obj.parentNode.parentNode.rowIndex;
		$(obj.parentNode.parentNode.parentNode.parentNode.id).deleteRow(item_index);
	
	var target_mac = obj.parentNode.parentNode.children[1].title;
	var apps_filter_row = apps_filter.split("<");
	var apps_filter_temp = "";
	for(i=0;i<apps_filter_row.length;i++){
		var apps_filter_col = apps_filter_row[i].split(">");	
			if(apps_filter_col[1] != target_mac){
				if(apps_filter_temp == ""){
					apps_filter_temp += apps_filter_row[i];			
				}
				else{
					apps_filter_temp += "<" + apps_filter_row[i];				
				}			
			}
	}

	apps_filter = apps_filter_temp;
	genMain_table();
}

function addRow_main(obj, length){	
	var category_array = $j(obj.parentNode).siblings()[2].children;
	var subCategory_array;
	var category_checkbox;
	var enable_checkbox = $j(obj.parentNode).siblings()[0].children[0];
	var first_catID = 0;
	var invalid_char = "";
	var blank_category = 0;
	var apps_filter_row =  apps_filter.split("<");
	var apps_filter_col = "";
	
	if(!validator.string(document.form.PC_devicename))
		return false;
	
	if(document.form.PC_devicename.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.PC_devicename.focus();
		return false;
	}
	
	for(var i = 0; i < document.form.PC_devicename.value.length; ++i){
		if(document.form.PC_devicename.value.charAt(i) == '<' || document.form.PC_devicename.value.charAt(i) == '>'){
			invalid_char += document.form.PC_devicename.value.charAt(i);
			document.form.PC_devicename.focus();
			alert("<#JS_validstr2#> ' "+invalid_char + " '");
			return false;			
		}
	}	
	
	for(i=0; i < category_array.length;i++){
		subCategory_array = category_array[i].children[2].children;	
		for(j=0;j<subCategory_array.length;j++){	
			category_checkbox = category_array[i].children[2].children[j].children[1];	
			if(category_checkbox.checked)
				blank_category = 1;
		}
	}
	
	for(i=0;i<apps_filter_row.length;i++){
		apps_filter_col = apps_filter_row[i].split(">");
		if(apps_filter_col[1] == document.form.PC_mac.value){
			alert("<#JS_duplicate#>");
			document.form.PC_mac.value = "";
			document.form.PC_devicename.value = "";
			return false;
		}
	}
	
	if(blank_category == 0){
		alert("The Content Category can not be empty");
		return false;
	}

	if(apps_filter == ""){
		apps_filter += enable_checkbox.checked ? 1:0;
	}	
	else{
		apps_filter += "<";
		apps_filter += enable_checkbox.checked ? 1:0;
	}	

	if(document.form.PC_mac.value == "")
		apps_filter += ">" + document.form.PC_devicename.value + ">";
	else
		apps_filter += ">" + document.form.PC_mac.value + ">";

	/* To check which checkbox is checked*/
	for(i=0; i < category_array.length;i++){
		first_catID = 0;
		subCategory_array = category_array[i].children[2].children;	
		for(j=0;j<subCategory_array.length;j++){	
			category_checkbox = category_array[i].children[2].children[j].children[1];
			if(first_catID == 0){	
				apps_filter += category_checkbox.checked ? 1:0;
				first_catID = 1;				
			}
			else{	
				apps_filter += ",";
				apps_filter += category_checkbox.checked ? 1:0;		
			}
			
			if(category_checkbox.checked)
				blank_category = 1;
		}
		
		if(i != category_array.length -1)
			apps_filter += ">";
	}
	
	document.form.PC_mac.value = "";
	document.form.PC_devicename.value = "";
	genMain_table();	
}
					 
function genMain_table(){
	var category_name = ["<#AiProtection_filter_Adult#>", "<#AiProtection_filter_message#>", "<#AiProtection_filter_p2p#>", "<#AiProtection_filter_stream#>"];
	var sub_category_name = [["<#AiProtection_filter_Adult1#>", "<#AiProtection_filter_Adult2#>", "<#AiProtection_filter_Adult3#>"],
							 ["<#AiProtection_filter_Adult3#>", "<#AiProtection_filter_Adult5#>", "<#AiProtection_filter_Adult6#>", "<#AiProtection_filter_Adult7#>", "<#AiProtection_filter_Adult8#>"],
							 ["<#AiProtection_filter_p2p1#>", "<#AiProtection_filter_p2p2#>"],
							 ["<#AiProtection_filter_stream1#>", "<#AiProtection_filter_stream2#>", "<#AiProtection_filter_stream3#>"]];
	var category_desc = ["<#AiProtection_filter_Adult_desc#>", 
						 "<#AiProtection_filter_message_desc#>", 
						 "<#AiProtection_filter_p2p_desc#>", 
						 "<#AiProtection_filter_stream_desc#>"];
		
	var match_flag = 0;
	var apps_filter_row = apps_filter.split("<");
	var code = "";	
	code += '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code += '<thead><tr>';
	code += '<td colspan="5"><#ConnectedClient#>&nbsp;(<#List_limit#>&nbsp;16)</td>';
	code += '</tr></thead>';	
	code += '<tbody>';
	code += '<tr>';
	code += '<th width="5%" height="30px" title="<#select_all#>">';
	code += '<input id="selAll" type="checkbox" onclick="selectAll(this, 0);" value="">';
	code += '</th>';
	code += '<th width="40%">Client\'s MAC address</th>';
	code += '<th width="40%"><#AiProtection_filter_category#></th>';
	code += '<th width="10%"><#list_add_delete#></th>';
	code += '</tr>';
	code += '<tr id="main_element">';	
	code += '<td style="border-bottom:2px solid #000;" title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>">';
	code += '<input type="checkbox" checked="">';
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;">';
	code += '<input type="text" maxlength="17" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="PC_devicename" onkeypress="return validator.isHWAddr(this,event)" onclick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}" placeholder="<#AiProtection_client_select#>">';
	code += '<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code += '<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>';	
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;text-align:left;">';
	for(i=0;i<category_name.length;i++){
		code += '<div style="font-size:14px;font-weight:bold">';
		code += '<img class="closed" src="/images/Tree/vert_line_s10.gif" onclick="show_subCategory(this);">';
		code += '<input type="checkbox" onclick="set_category(this, 0);">'+category_name[i];
		code += '<div style="display:none;font-size:12px;">';
		for(j=0;j<sub_category_name[i].length;j++){
			code += '<div style="margin-left:20px;"><img src="/images/Tree/vert_line_s01.gif"><input type="checkbox" onclick="set_category(this, 1);">' + sub_category_name[i][j] + '</div>';		
		}
	
		code += '</div>';
		code += '<div style="margin-left:25px;color:#FC0;font-style:italic;font-size:12px;font-weight:normal;">'+ category_desc[i] +'</div>';
		code += '</div>';	
	}
	
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onclick="addRow_main(this, 16)" value=""></td>';
	code += '</tr>';
	if(apps_filter == ""){
		code += '<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td></tr>';
	}
	else{
		for(k=0;k< apps_filter_row.length;k++){
			var apps_filter_col = apps_filter_row[k].split('>');
			var cat_flag = new Array(apps_filter_col[2]);
			var apps_client_name = "";
			
			for(i=0;i<custom_name_row.length;i++){
				var custom_name_col = custom_name_row[i].split(">");
				if(custom_name_col[1] == apps_filter_col[1]){
					apps_client_name =  custom_name_col[0];
					match_flag = 1;
				}
			}
			
			if(match_flag == 0){		//if doesn't match client name by custom client list
				for(i=1;i<client_list_row.length;i++){
					var client_list_col = client_list_row[i].split('>');
					if(client_list_col[3] == apps_filter_col[1]){
						apps_client_name = client_list_col[1];
						match_flag = 1;			
					}				
				}		
			}

			code += '<tr>';
			code += '<td title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>">';
			if(apps_filter_col[0] == 1)
				code += '<input type="checkbox" checked>';
			else	
				code += '<input type="checkbox">';
							
			code += '</td>';	
			if(match_flag == 1){
				code += '<td title="'+apps_filter_col[1]+'">'+ apps_client_name + '<br>(' +  apps_filter_col[1]  +')</td>';
			}
			else{
				if(apps_filter_col[1] == ""){		//input manually
					code += '<td title="'+document.form.PC_devicename.value+'">'+ document.form.PC_devicename.value +'</td>';	
				}
				else{
					code += '<td title="'+apps_filter_col[1]+'">'+ apps_filter_col[1] +'</td>';	
				}	
			}
			
			code += '<td style="text-align:left;">';		
			for(i=0;i<category_name.length;i++){
				var cate_flag_array = new Array(apps_filter_col[i+2]);
				var cate_flag_array_col = cate_flag_array[0].split(",");
				var cate_flag = cate_flag_array[0].indexOf('1');

				code += '<div>';
				code += '<img class="closed" src="/images/Tree/vert_line_s10.gif" onclick="show_subCategory(this);">';
				if(cate_flag != -1){
					code += '<input type="checkbox" onclick="set_category(this, 0);" checked>'+category_name[i];
				}
				else{
					code += '<input type="checkbox" onclick="set_category(this, 0);">'+category_name[i];
				}

				code += '<div style="display:none;">';
				for(j=0;j<sub_category_name[i].length;j++){
					if(cate_flag_array_col[j] == 1){
						code += '<div style="margin-left:20px;"><img src="/images/Tree/vert_line_s01.gif"><input type="checkbox" onclick="set_category(this, 1);" checked>' + sub_category_name[i][j] + '</div>';
					}
					else{
						code += '<div style="margin-left:20px;"><img src="/images/Tree/vert_line_s01.gif"><input type="checkbox" onclick="set_category(this, 1);">' + sub_category_name[i][j] + '</div>';
					}
				}
			
				code += '</div>';
				code += '</div>';
			}
			
			code += '</td>';
			code += '<td><input class="remove_btn" type="button" onclick="deleteRow_main(this);"></td>';
			code += '</tr>';
		
			match_flag = 0;
		}
	}
	
	code += '</tbody>';	
	code += '</table>';
	$('mainTable').innerHTML = code;
	showLANIPList();
}

function showLANIPList(){
	var code = "";
	var show_name = "";
	var custom_col_temp = new Array();
	var match_flag = 0;
	var custom_name_temp = "";
	if(custom_name_row != ""){
		for(i=0;i<custom_name_row.length;i++){
			custom_col_temp[i] = custom_name_row[i].split('>');	
		}
	}
	
	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');
		if(client_list_col[1] && client_list_col[1].length > 20)
			show_name = client_list_col[1].substring(0, 16) + "..";
		else
			show_name = client_list_col[1];
			
		if(custom_name != ""){	
			for(k=0;k<custom_name_row.length;k++){
				if(custom_col_temp[k][1] == client_list_col[3]){
					match_flag = 1;									
					custom_name_temp = custom_col_temp[k][0];
				}
			}
		}		

		if(match_flag){
			code += '<a><div style="height:auto;" onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+ custom_name_temp +'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[3]+'</strong> ';
			if(show_name && show_name.length > 0)
				code += '( '+custom_name_temp+')';
				
			match_flag =0;	
		}
		else{
			if(client_list_col[1])
				code += '<a><div style="height:auto;" onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[1]+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[3]+'</strong> ';
			else
				code += '<a><div style="height:auto;" onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[3]+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[3]+'</strong> ';			
		
			if(show_name && show_name.length > 0)
				code += '( '+show_name+')';
		}	
				
		code += ' </div></a>';
	}
	
	code +="<!--[if lte IE 6.5]><script>alert(\"<#ALERT_TO_CHANGE_BROWSER#>\");</script><![endif]-->";	
	$("ClientList_Block_PC").innerHTML = code;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function edit_table(){
	var apps_filter_temp = "";
	var first_element = $j('#main_element').siblings();
	
	for(k=1;k<first_element.length;k++){
		var enable_checkbox = first_element[k].children[0].children[0].checked ? 1:0;
		var target_mac = first_element[k].children[1].title;
		var category_array = first_element[k].children[2].children;
		var subCategory_array;
		var category_checkbox;
		var first_catID = 0;
		var blank_category = 0;
		
		if(k == 1)
			apps_filter_temp += enable_checkbox;
		else{
			apps_filter_temp += "<" + enable_checkbox;;
		}
		
		apps_filter_temp += ">" + target_mac + ">";
		
		for(i=0; i < category_array.length;i++){
			first_catID = 0;
			subCategory_array = category_array[i].children[2].children;
			for(j=0;j<subCategory_array.length;j++){
				category_checkbox = category_array[i].children[2].children[j].children[1];
				if(category_checkbox.checked)
					blank_category = 1;
				
				if(first_catID == 0){	
					apps_filter_temp += category_checkbox.checked ? 1:0;	
					first_catID = 1;				
				}
				else{	
					apps_filter_temp += ",";
					apps_filter_temp += category_checkbox.checked ? 1:0;		
				}
			}			
			
			if(i != category_array.length -1)
				apps_filter_temp += ">";
		}		
		
		if(blank_category == 0){
				alert("The Content Category can not be empty");
				return false;
		}
	}
	
	
	apps_filter = apps_filter_temp;
	return true;
}
						
function applyRule(){
	var apps_filter_row = "";
	
	if(document.form.PC_devicename.value != ""){
		alert("You must press add icon to add a new rule first.");
		return false;
	}
	
	if(apps_filter != ""){
		if(edit_table())
			apps_filter_row =  apps_filter.split("<");
		else	
			return false;
	}	

	var wrs_rulelist = "";
	var apps_rulelist = "";
	for(i=0;i<apps_filter_row.length;i++){
		var apps_filter_col =  apps_filter_row[i].split(">");	
		for(j=0;j<apps_filter_col.length;j++){
			if(j == 0){		
				if(wrs_rulelist == ""){
					wrs_rulelist += apps_filter_col[j] + ">";
					apps_rulelist += apps_filter_col[j] + ">";
				}	
				else{	
					wrs_rulelist += "<" + apps_filter_col[j] + ">";	
					apps_rulelist += "<" + apps_filter_col[j] + ">";				
				}	
			}
			else if(j == 1){
				wrs_rulelist += apps_filter_col[j] + ">";
				apps_rulelist += apps_filter_col[j] + ">";
			}
			else{
				var cate_id_array = apps_filter_col[j].split(",");
				var wrs_first_cate = 0;
				var apps_first_cate = 0;
				for(k=0;k<cate_id_array.length;k++){
					if(cate_id_array[k] == 1){						
						if(wrs_first_cate == 0){						
							if(wrs_id_array[j-2][k] != ""){
								wrs_rulelist += wrs_id_array[j-2][k];
								wrs_first_cate = 1;
							}
						}
						else{
							if(wrs_id_array[j-2][k] != ""){
								wrs_rulelist += "," + wrs_id_array[j-2][k];
							}
						}
						
						if(apps_first_cate == 0){						
							if(apps_id_array[j-2][k] != ""){
								apps_rulelist += apps_id_array[j-2][k];
								apps_first_cate = 1;
							}
						}
						else{
							if(apps_id_array[j-2][k] != ""){
								apps_rulelist += "," + apps_id_array[j-2][k];
							}
						}		
					}						
				}
				
				if(j != apps_filter_col.length-1){
					wrs_rulelist += ">";
					apps_rulelist += ">";
				}
			}
		}	
	}

	document.form.action_script.value = "restart_wrs;restart_firewall";
	document.form.wrs_rulelist.value = wrs_rulelist;
	document.form.wrs_app_rulelist.value = apps_rulelist;
	showLoading();	
	document.form.submit();
}

function translate_category_id(){
	//var mapping_array_init = [[0],[0,0,0], [0,0,0,0,0], [0,0], [0,0,0]];
	var mapping_array_init = [[0,0,0], [0,0,0,0,0], [0,0], [0,0,0]];
	var apps_filter_row = "";
	var wrs_filter_row = "";
	var wrs_app_filter_row = "";
	if(apps_filter != "")
		apps_filter_row =  apps_filter.split("<");
	
	if(wrs_filter != "" || wrs_app_filter != ""){
		wrs_filter_row =  wrs_filter.split("<");
		wrs_app_filter_row =  wrs_app_filter.split("<");	
	}
	
	var apps_filter_temp = "";
	var wrs_filter_temp = "";
	var wrs_app_filter_temp = "";
	for(i=0;i<wrs_filter_row.length;i++){
		var apps_filter_col = apps_filter_row[i].split(">");
		var wrs_filter_col = wrs_filter_row[i].split(">");
		var wrs_app_filter_col = wrs_app_filter_row[i].split(">");

		for(j=0;j<wrs_filter_col.length;j++){
			if(j == 0){
				if(apps_filter_temp == "")
					apps_filter_temp += wrs_filter_col[j] + ">";
				else
					apps_filter_temp += "<" + wrs_filter_col[j] + ">";
			}
			else if(j == 1){
				apps_filter_temp += wrs_filter_col[j] + ">";	
			}
			else{
					for(k=0;k<mapping_array_init[j-2].length;k++){	
						if(wrs_filter_col[j] != "" && wrs_app_filter_col[j] != ""){
	
							if(wrs_id_array[j-2][k] != ""){
								if(wrs_filter_col[j].indexOf(wrs_id_array[j-2][k]) != -1){
									mapping_array_init[j-2][k] = 1;
								}
							}
							
							if(apps_id_array[j-2][k] != ""){
								if(wrs_app_filter_col[j].indexOf(apps_id_array[j-2][k]) != -1){
									mapping_array_init[j-2][k] = 1;	
								}	
							}						
						}
						else if(wrs_filter_col[j] != ""){
							if(wrs_filter_col[j].indexOf(wrs_id_array[j-2][k]) != -1){
								mapping_array_init[j-2][k] = 1;
							}
						}
						else if(wrs_app_filter_col[j] != ""){
							if(wrs_app_filter_col[j].indexOf(apps_id_array[j-2][k]) != -1){
								mapping_array_init[j-2][k] = 1;
							}
						}
						else{
							mapping_array_init[j-2][k] = 0;
						}
						
						if(k == 0)
							apps_filter_temp += mapping_array_init[j-2][k];	
						else
							apps_filter_temp += "," + mapping_array_init[j-2][k];
						
					}	
	
				if(j != apps_filter_col.length-1 )
					apps_filter_temp += ">";
			}
		}

		//mapping_array_init = [[0], [0,0,0], [0,0,0,0,0], [0,0], [0,0,0]];
		mapping_array_init = [[0,0,0], [0,0,0,0,0], [0,0], [0,0,0]];
	}

	apps_filter = apps_filter_temp;
}

function show_inner_tab(){
	var code = "";
	if(document.form.current_page.value == "ParentalControl.asp"){		
		code += '<span class="clicked">Time Limits</span>';
		code += '<a href="AiProtection_WebProtector.asp">';
		code += "<span style=\"margin-left:10px\" class=\"click\"><#AiProtection_filter#></span>";
		code += '</a>';
	}
	else{
		code += '<a href="ParentalControl.asp">';
		code += '<span class="click">Time Limits</span>';
		code += '</a>';
		code += "<span style=\"margin-left:10px\" class=\"clicked\"><#AiProtection_filter#></span>";	
	}

	$('switch_menu').innerHTML = code;
}

function show_tm_eula(){
	if(document.form.preferred_lang.value == "JP"){
		$j.get("JP_tm_eula.htm", function(data){
			$('agreement_panel').innerHTML= data;
		});
	}
	else{
		$j.get("tm_eula.htm", function(data){
			$('agreement_panel').innerHTML= data;
		});
	}
	dr_advise();
	cal_agreement_block();
	$j("#agreement_panel").fadeIn(300);
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
	$j('#iphone_switch').animate({backgroundPosition: -37}, "slow", function() {});
	curState = 0;
	$("hiddenMask").style.visibility = "hidden";
}

function eula_confirm(){
	document.form.TM_EULA.value = 1;
	document.form.wrs_enable.value = 1;	
	document.form.wrs_app_enable.value = 1;	
	applyRule();
}

</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="AiProtection_WebProtector.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="PC_mac" value="">
<input type="hidden" name="wrs_enable" value="<% nvram_get("wrs_enable"); %>">
<input type="hidden" name="wrs_enable_ori" value="<% nvram_get("wrs_enable"); %>">
<input type="hidden" name="wrs_app_enable" value="<% nvram_get("wrs_app_enable"); %>">
<input type="hidden" name="wrs_rulelist" value="">
<input type="hidden" name="wrs_app_rulelist" value="">
<input type="hidden" name="TM_EULA" value="<% nvram_get("TM_EULA"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div  id="mainMenu"></div>	
			<div  id="subMenu"></div>		
		</td>						
		<td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>	
		<!--===================================Beginning of Main Content===========================================-->		
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0" >
			<tr>
				<td valign="top" >	
					<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
						<tr>
							<td bgcolor="#4D595D" valign="top">
								<div>&nbsp;</div>
								<!--div class="formfonttitle"></div-->
								<div style="margin-top:-5px;">
									<table width="730px">
										<tr>
											<td align="left">
												<div class="formfonttitle" style="width:400px">AiProtection - <#AiProtection_filter#></div>
											</td>
											<td>
												<div id="switch_menu" style="margin:-20px 0px 0px -20px;">
													<div style="width:173px;height:30px;border-top-left-radius:8px;border-bottom-left-radius:8px;" class="block_filter_pressed">
														<div style="text-align:center;padding-top:5px;color:#93A9B1;font-size:14px"><#AiProtection_filter#></div>
													</div>
													<a href="ParentalControl.asp">
														<div style="width:172px;height:30px;margin:-32px 0px 0px 173px;border-top-right-radius:8px;border-bottom-right-radius:8px;" class="block_filter">
															<div class="block_filter_name"><#Time_Scheduling#></div>
														</div>
													</a>
												</div>
											</td>
										</tr>
									</table>
								</div>
								<div style="margin:0px 0px 10px 5px;"><img src="/images/New_ui/export/line_export.png"></div>
								<div id="PC_desc">
									<table width="700px" style="margin-left:25px;">
										<tr>
											<td>
												<img id="guest_image" src="/images/New_ui/Web_Apps_Restriction.png">
											</td>
											<td>&nbsp;&nbsp;</td>
											<td style="font-style: italic;font-size: 14px;">
												<span><#AiProtection_filter_desc1#></span>
												<ol>
													<li><#AiProtection_filter_desc2#></li>
													<li><#AiProtection_filter_desc3#></li>
													<li><#AiProtection_filter_desc4#></li>
												</ol>
												<span><#AiProtection_filter_note#></span>
												<div><a style="text-decoration:underline;" href="http://www.asus.com/us/support/FAQ/1008720/" target="_blank"><#Parental_Control#> FAQ</a></div>
											</td>
										</tr>
									</table>
								</div>
								<br>
			<!--=====Beginning of Main Content=====-->
								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<tr>
										<th><#AiProtection_filter#></th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_web_restrict_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_web_restrict_enable').iphoneSwitch('<% nvram_get("wrs_enable"); %>',
														function(){
															curState = 1;
															if(document.form.TM_EULA.value == 0){
																show_tm_eula();
																return;
															}	
															
															document.form.wrs_enable.value = 1;	
															document.form.wrs_app_enable.value = 1;
															showhide("list_table",1);
															
														},
														function(){
															document.form.wrs_enable.value = 0;
															document.form.wrs_app_enable.value = 0;
															showhide("list_table",0);
															document.form.wrs_rulelist.disabled = true;
															document.form.wrs_app_rulelist.disabled = true;
															if(document.form.wrs_enable_ori.value == 1){
																applyRule();
															}
															else{
																curState = 0;
															}
																														
														}
													);
												</script>			
											</div>
										</td>
									</tr>								
									<tr style="display:none;">
										<th>Enable Block Notification page</th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="block_notification_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#block_notification_enable').iphoneSwitch('<% nvram_get(""); %>',
														function(){
									
														},
														function(){
																					
														}
													);
												</script>			
											</div>
										</td>			
									</tr>									
								</table>				
								<table id="list_table" width="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="display:none">
									<tr>
										<td valign="top" align="center">
											<div id="mainTable" style="margin-top:10px;"></div> 
											<div id="ctrlBtn" style="text-align:center;margin-top:20px;">
												<input class="button_gen" type="button" onclick="applyRule();" value="<#CTL_apply#>">											
												<div style="width:135px;height:55px;position:absolute;bottom:5px;right:5px;background-image:url('images/New_ui/tm_logo_power.png');"></div>
											</div>
										</td>	
									</tr>
								</table>
							</td>
						</tr>
					</table>
				</td>         
			</tr>
		</table>				
		<!--===================================Ending of Main Content===========================================-->		
	</td>		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</form>
</body>
</html>

