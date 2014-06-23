<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE9"/>
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
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style>
#switch_menu{
	text-align:right
}
#switch_menu span{
	/*border:1px solid #222;*/
	
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
					["", "69", "", "23"]];

var apps_id_array = [["", "", ""],
					 ["", "0,15", "", "", "21"],
					 ["3", "1"],
					 ["8", "4", "13", ""]];
var over_var = 0;
var isMenuopen = 0;

function initial(){
	show_menu();
	//show_inner_tab();
	translate_category_id();
	genMain_table();
	if('<% nvram_get("wrs_enable"); %>' == 1)
		showhide("list_table",1);
	else
		showhide("list_table",0);
	
	showLANIPList();
	if(document.eula_form.TM_EULA.value == 1)
		$('eula_hint').style.display = "none";
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
	
	if(!validate_string(document.form.PC_devicename))
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

	if(apps_filter == ""){
		apps_filter += enable_checkbox.checked ? 1:0;
	}	
	else{
		apps_filter += "<";
		apps_filter += enable_checkbox.checked ? 1:0;
	}	
	
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
		}
		
		if(i != category_array.length -1)
			apps_filter += ">";
	}
	
	genMain_table();	
}
					 
function genMain_table(){
	var category_name = ["Adult", "Instant Message and Communication", "P2P and File Transfer", "Streaming and Entertainment"];
	var sub_category_name = [["Pornography", "Illegal and Violence", "Gambling"],
							 ["Internet Telephony", "Instant Mssaging", "Virtual Community", "Blog", "Mobile"],
							 ["File transfer", "Peer to Peer"],
							 ["Games", "Streaming media", "General Web service", "Internet Radio and TV"]];

	var category_desc = ["Block adult content can prevent child from visiting sexy, violence and illegal related content.", 
						 "Block IM and communication content can prevent child from addicted to social networking usage.", 
						 "Block P2P and file transfer content can keep your network in a better transmission quality.", 
						 "Block streaming and entertainment content can prevent child from spending long time on Internet entertainment."];
	var match_flag = 0;
	var apps_filter_row = apps_filter.split("<");
	var code = "";	
	code += '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code += '<thead><tr>';
	code += '<td colspan="5"><#ConnectedClient#>&nbsp;(<#List_limit#>：&nbsp;16)</td>';
	code += '</tr></thead>';	
	code += '<tbody>';
	code += '<tr>';
	code += '<th width="5%" height="30px" title="Select all">';
	code += '<input id="selAll" type="checkbox" onclick="selectAll(this, 0);" value="">';
	code += '</th>';
	code += '<th width="40%">Client name</th>';
	code += '<th width="40%">Content Category</th>';
	code += '<th width="10%"><#list_add_delete#></th>';
	code += '</tr>';
	code += '<tr id="main_element">';	
	code += '<td style="border-bottom:2px solid #000;" title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>">';
	code += '<input type="checkbox" checked="">';
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;">';
	code += '<input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="PC_devicename" onkeypress="" onclick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}" placeholder="Please select the client name">';
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
				code += '<td title="'+apps_filter_col[1]+'">'+ apps_client_name +'</td>';
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
	
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
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
	}
	
	apps_filter = apps_filter_temp;
}
						
function applyRule(){
	var apps_filter_row = "";
	
	if(apps_filter != "")
		edit_table();

	if(apps_filter != "")
		apps_filter_row =  apps_filter.split("<");
	
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
	var mapping_array_init = [[0,0,0], [0,0,0,0,0], [0,0], [0,0,0,0]];
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

		mapping_array_init = [[0,0,0], [0,0,0,0,0], [0,0], [0,0,0,0]];
	}

	apps_filter = apps_filter_temp;
}

function show_inner_tab(){
	var code = "";
	if(document.form.current_page.value == "ParentalControl.asp"){		
		code += '<span class="clicked">Time Limits</span>';
		code += '<a href="ParentalControl_WebProtector.asp">';
		code += '<span style="margin-left:10px" class="click">Web & App Restrictions</span>';
		code += '</a>';
	}
	else{
		code += '<a href="ParentalControl.asp">';
		code += '<span class="click">Time Limits</span>';
		code += '</a>';
		code += '<span style="margin-left:10px" class="clicked">Web & App Restrictions</span>';	
	}

	$('switch_menu').innerHTML = code;
}

function show_tm_eula(){
	document.eula_form.TM_EULA.value = 1;
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
	document.eula_form.TM_EULA.value = 0;
	$("hiddenMask").style.visibility = "hidden";
}
function eula_confirm(){
	document.eula_form.submit();

}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;">
	<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:25px;margin-left:30px;">Trend Micro End User License Agreement</div>
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
		<input class="button_gen" type="button"  onclick="eula_confirm();" value="<#CTL_ok#>">	
	</div>
</div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="eula_form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="ParentalControl_WebProtector.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="TM_EULA" value="<% nvram_get("TM_EULA"); %>">
</form>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="ParentalControl_WebProtector.asp">
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
								<div>
									<table width="730px">
										<tr>
											<td align="left">
												<span class="formfonttitle">AiProtection - Web & Apps Restrictions</span>
											</td>
											<td>
												<div id="switch_menu" style="margin:-20px 0px 0px 125px;">													
													<div style="background-image:url('images/New_ui/left-dark.png');width:173px;height:40px;">
														<div style="text-align:center;padding-top:9px;color:#93A9B1;font-size:14px">Web & Apps Restrictions</div>
													</div>
													<a href="ParentalControl.asp">
														<div style="background-image:url('images/New_ui/right-light.png');width:101px;height:40px;margin:-40px 0px 0px 173px;">
															<div style="text-align:center;padding-top:9px;color:#FFFFFF;font-size:14px">Time Limits</div>
														</div>
													</a>
												</div>
											</td>
										</tr>
									</table>
								</div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div id="PC_desc">
									<table width="700px" style="margin-left:25px;">
										<tr>
											<td>
												<img id="guest_image" src="/images/New_ui/Web_Apps_Restriction.png">
											</td>
											<td>&nbsp;&nbsp;</td>
											<td style="font-style: italic;font-size: 14px;">
												<span>Web & Apps Restrictions allows you to block access maliciously and adult contents. To use this feature, check the content categories you want to limit for specific client.</span>					
											</td>
										</tr>
									</table>
								</div>
								<br>
			<!--=====Beginning of Main Content=====-->
								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<tr>
										<th>Enable Web & Apps Restrictions</th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_ParentControl_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_ParentControl_enable').iphoneSwitch('<% nvram_get("wrs_enable"); %>',
														function(){
															document.form.wrs_enable.value = 1;	
															document.form.wrs_app_enable.value = 1;	
															showhide("list_table",1);	
														},
														function(){
															document.form.wrs_enable.value = 0;
															document.form.wrs_app_enable.value = 0;																
															showhide("list_table",0);
															document.form.wrs_rulelist.disabled = true;
															if(document.form.wrs_enable_ori.value == 1)
																applyRule();															
														},
															{
																switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														});
												</script>			
											</div>
											<div id="eula_hint" style="margin:-26px 0px 0px 110px;">
												Agree with the <span style="text-decoration:underline;cursor:pointer;" onclick="show_tm_eula();">EULA</span> to enjoy premium function with Trend Micro
											</div>
										</td>
									</tr>								
									<tr>
										<th>Enable Block Notification page</th>
										<td>
											<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="block_notification_enable"></div>
											<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#block_notification_enable').iphoneSwitch('<% nvram_get(""); %>',
														function(){
									
														},
														function(){
																					
														},
															{
																switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														});
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
