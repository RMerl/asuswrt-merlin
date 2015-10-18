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
<title><#Web_Title#> - <#vpnc_title#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style type="text/css">
.contentM_qis{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:none;
	margin-left: 30%;
	top: 290px;
	width:650px;
}
.contentM_qis_manual{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	margin-left: -30px;
	margin-left: -100px \9; 
	margin-top:-400px;
	width:740px;
	box-shadow: 3px 3px 10px #000;
}

.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless thead{
	font-size:15px;
	line-height:20px;
	color:#FFFFFF;	
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	font-weight:bolder;
	color: #FFFFFF;
	text-align:left; 
}

.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;	
}
</style>
<script>


var vpnc_clientlist = decodeURIComponent('<% nvram_char_to_ascii("","vpnc_clientlist"); %>');
var vpnc_clientlist_array_ori = '<% nvram_char_to_ascii("","vpnc_clientlist"); %>';
var vpnc_clientlist_array = decodeURIComponent(vpnc_clientlist_array_ori);
var vpnc_pptp_options_x_list_array = decodeURIComponent('<% nvram_char_to_ascii("", "vpnc_pptp_options_x_list"); %>');
var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var restart_vpncall_flag = 0; //Viz add 2014.04 for Edit Connecting rule then restart_vpncall

function initial(){
	show_menu();
	show_vpnc_rulelist();
}

function Add_profile(){
	document.form.vpnc_des_edit.value = "";
	document.form.vpnc_svr_edit.value = "";
	document.form.vpnc_account_edit.value = "";
	document.form.vpnc_pwd_edit.value = "";
	document.form.selPPTPOption.value = "auto";
	document.getElementById("pptpOptionHint").style.display = "none";
	tabclickhandler(0);
	document.getElementById("cancelBtn").style.display = "";
	document.getElementById("pptpcTitle").style.display = "";
	document.getElementById("l2tpcTitle").style.display = "";

        $("#vpnc_settings").fadeIn(300);
}

function cancel_add_rule(){
	restart_vpncall_flag = 0;
	idx_tmp = "";
	$("#vpnc_settings").fadeOut(300);
}

function addRow_Group(upper, flag, idx){
	idx = parseInt(idx);
	if(idx >= 0){		//idx: edit row		
		var table_id = "vpnc_clientlist_table";
		var rule_num = document.getElementById(table_id).rows.length;
		var item_num = document.getElementById(table_id).rows[0].cells.length;
		if(flag == 'PPTP' || flag == 'L2TP'){
			description_obj = document.form.vpnc_des_edit;
			type_obj = document.form.vpnc_type;
			server_obj = document.form.vpnc_svr_edit;
			username_obj = document.form.vpnc_account_edit;
			password_obj = document.form.vpnc_pwd_edit;

		}

		if(validForm(flag)){
			var vpnc_clientlist_row = vpnc_clientlist_array.split('<');			
			duplicateCheck.saveTotmpIdx(idx);
			duplicateCheck.tmpStr = "";
			duplicateCheck.saveToTmpStr(type_obj, 0);
			duplicateCheck.saveToTmpStr(server_obj, 0);
			duplicateCheck.saveToTmpStr(username_obj, 0);
			duplicateCheck.saveToTmpStr(password_obj, 0);
			if(duplicateCheck.isDuplicate()){
				username_obj.focus();
				username_obj.select();
				alert("<#JS_duplicate#>")
				return false;
			}
					
			vpnc_clientlist_row[idx] = description_obj.value+">"+type_obj.value+">"+server_obj.value+">"+username_obj.value+">"+password_obj.value;
			vpnc_clientlist_array = vpnc_clientlist_row.join("<");

			//edit vpnc_pptp_options_x_list			
			if(vpnc_pptp_options_x_list_array == "") {
				//idx: NaN=Add i=edit row
				vpnc_pptp_options_x_list_array = reFillEmptyPPTPOPtion(idx, document.form.selPPTPOption.value, vpnc_clientlist_array); 
			}
			else {
				vpnc_pptp_options_x_list_array = handlePPTPOPtion(idx, document.form.selPPTPOption.value, vpnc_clientlist_array);
			}

			document.form.vpnc_pptp_options_x_list.value = vpnc_pptp_options_x_list_array;
			document.form.vpnc_pptp_options_x_list.value = vpnc_pptp_options_x_list_array;					

			show_vpnc_rulelist();
			
			if(restart_vpncall_flag == 1){	//restart_vpncall
				var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
				var vpnc_clientlist_col = vpnc_clientlist_row[idx].split('>');
				for(var j=0; j<vpnc_clientlist_col.length; j++){
					if(j == 0){
						document.form.vpnc_des_edit.value = vpnc_clientlist_col[0];
					}
					else if(j ==1){
						if(vpnc_clientlist_col[1] == "PPTP")
							document.form.vpnc_proto.value = "pptp";
						else if(vpnc_clientlist_col[1] == "L2TP")
							document.form.vpnc_proto.value = "l2tp";
						else	//OpenVPN
							document.form.vpnc_proto.value = "openvpn";
					} 
					else if(j ==2){
//						if(vpnc_clientlist_col[1] == "OpenVPN")
//							document.form.vpn_client_unit.value = 1;	//1, 2
//						else
							document.form.vpnc_heartbeat_x.value = vpnc_clientlist_col[2];
					} 
					else if(j ==3){
//						if(vpnc_clientlist_col[1] == "OpenVPN")
//							document.form.vpn_client1_username.value = vpnc_clientlist_col[3];
//						else
							document.form.vpnc_pppoe_username.value = vpnc_clientlist_col[3];
					} 
					else if(j ==4){
//						if(vpnc_clientlist_col[1] == "OpenVPN")
//							document.form.vpn_client1_password.value = vpnc_clientlist_col[4];
//						else
							document.form.vpnc_pppoe_passwd.value = vpnc_clientlist_col[4];
					} 
				}

				// update vpnc_pptp_options_x
				document.form.vpnc_pptp_options_x.value = "";
				if(vpnc_clientlist_col[1] == "PPTP" && document.form.selPPTPOption.value != "auto") {
					document.form.vpnc_pptp_options_x.value = document.form.selPPTPOption.value;
				}

				document.form.vpnc_clientlist.value = vpnc_clientlist_array;	
				document.getElementById("vpnc_clientlist_table").rows[idx].cells[0].innerHTML = "-";
				document.getElementById("vpnc_clientlist_table").rows[idx].cells[5].innerHTML = "<img src='/images/InternetScan.gif'>";
				document.form.submit();
			}
			else{
				document.form.vpnc_clientlist.value = vpnc_clientlist_row.join("<");
				document.form.submit();		
			}

			cancel_add_rule();
		}
	}
	else{	//Add Rule
		
		var rule_num = document.getElementById("vpnc_clientlist_table").rows.length;
		var item_num = document.getElementById("vpnc_clientlist_table").rows[0].cells.length;		
		if(rule_num >= upper){
			alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
			return false;	
		}

		type_obj = document.form.vpnc_type;
		description_obj = document.form.vpnc_des_edit;
		server_obj = document.form.vpnc_svr_edit;
		username_obj = document.form.vpnc_account_edit;
		password_obj = document.form.vpnc_pwd_edit;

		if(validForm(flag)){
			duplicateCheck.tmpIdx = -1;
			duplicateCheck.tmpStr = "";
			duplicateCheck.saveToTmpStr(type_obj, 0);
			duplicateCheck.saveToTmpStr(server_obj, 0);
			duplicateCheck.saveToTmpStr(username_obj, 0);
			duplicateCheck.saveToTmpStr(password_obj, 0);
			if(duplicateCheck.isDuplicate()){
				alert("<#JS_duplicate#>")
				return false;
			}

			addRow(description_obj ,1);
			addRow(type_obj, 0);
			addRow(server_obj, 0);
			addRow(username_obj, 0);
			addRow(password_obj, 0);
			if(vpnc_clientlist_array.charAt(0) == "<")	//rempve the 1st "<"
				vpnc_clientlist_array = vpnc_clientlist_array.substr(1,vpnc_clientlist_array.length);

			//add vpnc_pptp_options_x_list
			if(vpnc_pptp_options_x_list_array == "") {		//idx: NaN=Add i=edit row
				vpnc_pptp_options_x_list_array = reFillEmptyPPTPOPtion(idx, document.form.selPPTPOption.value, vpnc_clientlist_array);
			}
			else{
				vpnc_pptp_options_x_list_array = handlePPTPOPtion(idx, document.form.selPPTPOption.value, vpnc_clientlist_array);
			}

			show_vpnc_rulelist();
			cancel_add_rule();	
			document.form.vpnc_pptp_options_x_list.value = vpnc_pptp_options_x_list_array;
			document.form.vpnc_clientlist.value = vpnc_clientlist_array;
			document.form.submit();
		}				
	}	
}

function reFillEmptyPPTPOPtion(idx, objValue, vpncClientList) {
	var finalPPTPOptionsList = "";
	var currentVPNCList = vpncClientList;
	var currentVPNCListNum = currentVPNCList.split("<").length;
	if(idx >= 0){ 
		for(var i = 0; i < currentVPNCListNum; i += 1){
			if(i == idx){
				finalPPTPOptionsList += "<" + objValue;
			}
			else{
				finalPPTPOptionsList += "<auto";
			}
		}
	}
	else{ //add
		for(var i = 1; i < currentVPNCListNum; i += 1) {
			finalPPTPOptionsList += "<auto";
		}
		
		finalPPTPOptionsList += "<" + objValue;
	}
	
	return finalPPTPOptionsList;
}

function handlePPTPOPtion(idx, objValue, vpncClientList) {
	var finalPPTPOptionsList = "";
	var currentVPNCList = vpncClientList;
	var currentVPNCListNum = currentVPNCList.split("<").length;
	var origPPTPOptionsList = vpnc_pptp_options_x_list_array;
	var origPPTPOptionsListArray = origPPTPOptionsList.split("<");
	var origPPTPOptionsListArrayNum = origPPTPOptionsListArray.length;
	if(idx >= 0){ //edit
		for(var i = 0; i < currentVPNCListNum; i += 1) {
			if(i == idx) {
				if(objValue == "") {
					finalPPTPOptionsList += "<auto";
				}
				else {
					finalPPTPOptionsList += "<" + objValue;
				}
			}
			else {
				if(origPPTPOptionsListArray[i + 1] == "undefined") {
					finalPPTPOptionsList += "<auto";
				}
				else {
					finalPPTPOptionsList += "<" +  origPPTPOptionsListArray[i + 1];
				}
			}
		}
	}
	else{ //add
		for(var i = 1; i < currentVPNCListNum; i += 1){
			if(origPPTPOptionsListArray[i] == "undefined"){
				finalPPTPOptionsList += "<auto";
			}
			else{
				finalPPTPOptionsList += "<" + origPPTPOptionsListArray[i];
			}
		}
		
		finalPPTPOptionsList += "<" + objValue;
	}
	
	return finalPPTPOptionsList;
}

var duplicateCheck = {
	tmpIdx: "-1",
	saveTotmpIdx: function(obj){
		this.tmpIdx = obj;	
	},
	tmpStr: "",
	saveToTmpStr: function(obj, head){	
		if(head != 1)
			this.tmpStr += ">" /*&#62*/

		this.tmpStr += obj.value;
	},
	isDuplicate: function(){
		var vpnc_clientlist_row = vpnc_clientlist_array.split('<');		
		var index_del = parseInt(this.tmpIdx);
		for(var i=0; i<vpnc_clientlist_row.length; i++){		
			if(index_del == "" && index_del != 0){
				if(vpnc_clientlist_row[i].search(this.tmpStr) >= 0){
					return true;	
				}
			}
			else if(i != index_del){
				if(vpnc_clientlist_row[i].search(this.tmpStr) >= 0){
					return true;	
				}		
			}
		}

		return false;		
	}
}

function addRow(obj, head){
	if(head == 1)
		vpnc_clientlist_array += "<" /*&#60*/
	else
		vpnc_clientlist_array += ">" /*&#62*/

	vpnc_clientlist_array += obj.value;
}

function validForm(mode){
	valid_des = document.form.vpnc_des_edit;
	valid_server = document.form.vpnc_svr_edit;
	valid_username = document.form.vpnc_account_edit;
	valid_password = document.form.vpnc_pwd_edit;

	if(valid_des.value==""){
		alert("<#JS_fieldblank#>");
		valid_des.focus();
		return false;		
	}
	else if(!Block_chars(valid_des, ["*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"" ])){
		return false;		
	}

	if(valid_server.value==""){
		alert("<#JS_fieldblank#>");
		valid_server.focus();
		return false;
		}
		else{
			var isIPAddr = valid_server.value.replace(/\./g,"");
			var re = /^[0-9]+$/;
			if(!re.test(isIPAddr)) { //DDNS
				if(!Block_chars(valid_server, ["<", ">"])) {
					return false;
				}		
			}	
			else { // IP
				if(!validator.isLegalIP(valid_server,"")) {
					return false;
				}
			}
	}

	if(valid_username.value==""){
		alert("<#JS_fieldblank#>");
		valid_username.focus();
		return false;
	}else if(!Block_chars(valid_username, ["<", ">"])){
		return false;		
	}

	if(valid_password.value==""){
		alert("<#JS_fieldblank#>");
		valid_password.focus();
		return false;
	}else if(!Block_chars(valid_password, ["<", ">"])){
		return false;		
	}

	return true;
}

var save_flag;	//type of Saving profile
function tabclickhandler(_type){
	document.getElementById('pptpcTitle').className = "vpnClientTitle_td_unclick";
	document.getElementById('l2tpcTitle').className = "vpnClientTitle_td_unclick";
	if(_type == 0){
		save_flag = "PPTP";
		document.form.vpnc_type.value = "PPTP";
		document.getElementById('pptpcTitle').className = "vpnClientTitle_td_click";
		document.getElementById('trPPTPOptions').style.display = "";
	}
	else if(_type == 1){
		save_flag = "L2TP";
		document.form.vpnc_type.value = "L2TP";
		document.getElementById('l2tpcTitle').className = "vpnClientTitle_td_click";
		document.getElementById('trPPTPOptions').style.display = "none";
	}
}

function show_vpnc_rulelist(){	
	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
	var code = "";
	code +='<table style="margin-bottom:30px;" width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="list_table" id="vpnc_clientlist_table">';
	if(vpnc_clientlist_array == "")
		code +='<tr><td style="color:#FC0;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i=0; i<vpnc_clientlist_row.length; i++){
			overlib_str0[i] = "";
			overlib_str1[i] = "";
			code +='<tr id="row'+i+'">';
			var vpnc_clientlist_col = vpnc_clientlist_row[i].split('>');
			if(vpnc_clientlist_col[1] == document.form.vpnc_proto.value.toUpperCase() &&
				vpnc_clientlist_col[2] == document.form.vpnc_heartbeat_x.value &&
				vpnc_clientlist_col[3] == document.form.vpnc_pppoe_username.value)
			{
				if(vpnc_state_t == 0 || vpnc_state_t ==1) // Initial or Connecting
						code +='<td width="10%"><img title="<#CTL_Add_enrollee#>" src="/images/InternetScan.gif"></td>';
				else if(vpnc_state_t == 2) // Connected
						code +='<td width="10%"><img title="<#Connected#>" src="/images/checked_parentctrl.png" style="width:25px;"></td>';
					else if(vpnc_state_t == 4 && vpnc_sbstate_t == 2)
						code +="<td width=\"10%\"><img title=\"<#qis_fail_desc1#>\" src=\"/images/button-close2.png\" style=\"width:25px;\"></td>";
				else // Stop connection
						code +='<td width="10%"><img title="<#ConnectionFailed#>" src="/images/button-close2.png" style="width:25px;"></td>';
			}
			else
				code +='<td width="10%">-</td>';

			for(var j=0; j<vpnc_clientlist_col.length; j++){
				if(j == 0){
					if(vpnc_clientlist_col[0].length >28){						
						overlib_str0[i] += vpnc_clientlist_col[0];							
						vpnc_clientlist_col[0]=vpnc_clientlist_col[0].substring(0, 25)+"...";							
						code +='<td title="'+overlib_str0[i]+'">'+ vpnc_clientlist_col[0] +'</td>';
					}else{
						code +='<td>'+ vpnc_clientlist_col[0] +'</td>';					
					}
				}
				else if(j == 1){
					code += '<td width="15%">'+ vpnc_clientlist_col[1] +'</td>';
				}
			}

			// EDIT
		 	code += '<td width="10%"><input class="edit_btn" type="button" onclick="Edit_Row(this, \'vpnc\');" value=""/></td>';
			if( vpnc_clientlist_col[1] == document.form.vpnc_proto.value.toUpperCase() 
			 && vpnc_clientlist_col[2] == document.form.vpnc_heartbeat_x.value
			 && vpnc_clientlist_col[3] == document.form.vpnc_pppoe_username.value){		//matched connecting rule
				code += '<td width="10%"><input class="remove_btn" type="button" onclick="del_Row(this, \'vpnc_enable\');" value=""/></td>';
				code += '<td width="25%"><input class="button_gen" type="button" onClick="connect_Row(this, \'disconnect\');" id="disonnect_btn" value="Deactivate" style="padding:0 0.3em 0 0.3em;" >';
			}
			else{		// This rule is not connecting
				code += '<td width="10%"><input class="remove_btn" type="button" onclick="del_Row(this, \'vpnc\');" value=""/></td>';
				code += '<td width="25%"><input class="button_gen" type="button" onClick="connect_Row(this, \'vpnc\');" id="Connect_btn" name="Connect_btn" value="Activate" style="padding:0 0.3em 0 0.3em;" >';
			}
		}
	}

 	code +='</table>';
	document.getElementById("vpnc_clientlist_Block").innerHTML = code;		
}


function connect_Row(rowdata, flag){
	var idx = rowdata.parentNode.parentNode.rowIndex;
	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
	var vpnc_clientlist_col = vpnc_clientlist_row[idx].split('>');
	
	if(flag == "disconnect"){	//Disconnect the connected rule 
		document.form.vpnc_proto.value = "disable";
		document.form.vpnc_heartbeat_x.value = "";
		document.form.vpnc_pppoe_username.value = "";
		document.form.vpnc_pppoe_passwd.value = "";
		if(vpnc_clientlist_col[1] == "PPTP") {
			document.form.vpnc_pptp_options_x.value = "";
		}

	}
	else{		//"vpnc" making connection
		for(var j=0; j<vpnc_clientlist_col.length; j++){
			if(j == 0){
				document.form.vpnc_des_edit.value = vpnc_clientlist_col[0];
			}
			else if(j ==1){
				if(vpnc_clientlist_col[1] == "PPTP")
					document.form.vpnc_proto.value = "pptp";
				else if(vpnc_clientlist_col[1] == "L2TP")
					document.form.vpnc_proto.value = "l2tp";
				else	//OpenVPN
					document.form.vpnc_proto.value = "openvpn";
			} 
			else if(j ==2){
//				if(vpnc_clientlist_col[1] == "OpenVPN")
//					document.form.vpn_client_unit.value = 1;	//1, 2
//				else
					document.form.vpnc_heartbeat_x.value = vpnc_clientlist_col[2];
			} 
			else if(j ==3){
//				if(vpnc_clientlist_col[1] == "OpenVPN")
//					document.form.vpn_client1_username.value = vpnc_clientlist_col[3];
//				else
					document.form.vpnc_pppoe_username.value = vpnc_clientlist_col[3];
			} 
			else if(j ==4){
//				if(vpnc_clientlist_col[1] == "OpenVPN")
//					document.form.vpn_client1_password.value = vpnc_clientlist_col[4];
//				else
					document.form.vpnc_pppoe_passwd.value = vpnc_clientlist_col[4];
			
				document.form.vpnc_auto_conn.value = 1;
			}	
		}

		//handle vpnc_pptp_options_x
		if(vpnc_clientlist_col[1] == "PPTP"){
			var origPPTPOptionsList = vpnc_pptp_options_x_list_array;
			if(origPPTPOptionsList != ""){
				var origPPTPOptionsListArray = origPPTPOptionsList.split("<");
				var setPPTPOption = origPPTPOptionsListArray[idx + 1];
				document.form.vpnc_pptp_options_x.value = "";
				if(setPPTPOption != "auto" && setPPTPOption != "undefined") {
					document.form.vpnc_pptp_options_x.value = setPPTPOption;
				}	
			}
			else{
				document.form.vpnc_pptp_options_x.value = "";
			}
		}
	}
	
	document.form.vpnc_pptp_options_x_list.value = vpnc_pptp_options_x_list_array;
	document.form.vpnc_clientlist.value = vpnc_clientlist_array;	
	rowdata.parentNode.innerHTML = "<img src='/images/InternetScan.gif'>";
	document.form.submit();	
}

var idx_tmp = "";
function Edit_Row(rowdata, flag){
	document.getElementById("cancelBtn").style.display = "";
	idx_tmp = rowdata.parentNode.parentNode.rowIndex; //update idx
	var idx = rowdata.parentNode.parentNode.rowIndex;
	if(document.getElementById("vpnc_clientlist_table").rows[idx].cells[0].innerHTML != "-")
		restart_vpncall_flag = 1;
	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
	var vpnc_clientlist_col = vpnc_clientlist_row[idx].split('>');	
	//get idx of PPTP option value
	var pptpOptionValue = "";
	var origPPTPOptionsList = vpnc_pptp_options_x_list_array;			
	if(idx >= 0){
		var origPPTPOptionsListArray = origPPTPOptionsList.split("<");
		if(origPPTPOptionsListArray.length == 1) { //avoid vpnc_pptp_options_x_list is null
			pptpOptionValue = "auto";
		}
		else{
			if(origPPTPOptionsListArray[idx + 1] == "undefined"){
				pptpOptionValue = "auto";
			}
			else{
				pptpOptionValue = origPPTPOptionsListArray[idx + 1];
			}
			
		}
	}
	else{	//default is auto
		pptpOptionValue = "auto";
	}

	document.form.selPPTPOption.value = pptpOptionValue;
	pptpOptionChange();
	for(var j=0; j<vpnc_clientlist_col.length; j++){
		if(j == 0){
			document.form.vpnc_des_edit.value = vpnc_clientlist_col[0];
		}
		else if(j == 1){
			if(vpnc_clientlist_col[1] == "PPTP")
				tabclickhandler(0);
			else if(vpnc_clientlist_col[1] == "L2TP")
				tabclickhandler(1);
			else
				tabclickhandler(2);
		} 
		else if(j == 2){
			document.form.vpnc_svr_edit.value = vpnc_clientlist_col[2];
		} 
		else if(j == 3){
			document.form.vpnc_account_edit.value = vpnc_clientlist_col[3];
		} 
		else if(j == 4){
			document.form.vpnc_pwd_edit.value = vpnc_clientlist_col[4];
		} 
	}

	$("#vpnc_settings").fadeIn(300);
	if(vpnc_clientlist_col[1] == "PPTP"){
		document.getElementById("pptpcTitle").style.display = "";
		document.getElementById("l2tpcTitle").style.display = "none";
		document.getElementById("trPPTPOptions").style.display = "";
	}else if(vpnc_clientlist_col[1] == "L2TP"){
		document.getElementById("pptpcTitle").style.display = "none";
		document.getElementById("l2tpcTitle").style.display = "";
		document.getElementById("trPPTPOptions").style.display = "none";
	}	
}

function del_Row(rowdata, flag){
	var idx = rowdata.parentNode.parentNode.rowIndex;
	document.getElementById("vpnc_clientlist_table").deleteRow(idx);
	var vpnc_clientlist_value = "";
	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');	
	var vpnc_clientlist_col_delete = vpnc_clientlist_row[idx].split('>');
	for(k=0; k<vpnc_clientlist_row.length; k++){
		if(k != idx){				
			vpnc_clientlist_value += "<"+vpnc_clientlist_row[k];
		}
	}

	if(vpnc_clientlist_value.charAt(0) == "<")	//remove the 1st "<"
		vpnc_clientlist_value = vpnc_clientlist_value.substr(1,vpnc_clientlist_value.length);		

	vpnc_clientlist_array = vpnc_clientlist_value;
	if(vpnc_clientlist_array == "")
		show_vpnc_rulelist();

	//del vpnc_pptp_options_x_list
	var tempPPTPOptionsValue = "";
	var origPPTPOptionsList = vpnc_pptp_options_x_list_array;
	var origPPTPOptionsListArray = origPPTPOptionsList.split("<");
	var origPPTPOptionsListArrayNum = origPPTPOptionsListArray.length;
	for(var index = 0; index < origPPTPOptionsListArrayNum; index += 1) {
		if(index != (idx + 1)) {	//save exist items
			tempPPTPOptionsValue += "<"+ origPPTPOptionsListArray[index];
		}
	}
	
	if(tempPPTPOptionsValue.charAt(0) == "<") {	//remove the 1st "<"
		tempPPTPOptionsValue = tempPPTPOptionsValue.substr(1,tempPPTPOptionsValue.length);
	}
	
	vpnc_pptp_options_x_list_array = tempPPTPOptionsValue;
	document.form.vpnc_pptp_options_x_list.value = vpnc_pptp_options_x_list_array;

	if(flag == "vpnc_enable"){	//remove connected rule.

		document.form.vpnc_proto.value = "disable";
		document.form.vpnc_proto.disabled = false;
		document.form.action = "/start_apply.htm";
		document.form.enctype = "application/x-www-form-urlencoded";
		document.form.encoding = "application/x-www-form-urlencoded";
		document.form.action_script.value = "restart_vpncall";	
	}

	document.form.vpnc_clientlist.value = vpnc_clientlist_array;
	document.form.submit();
}

function pptpOptionChange() {
	document.getElementById("pptpOptionHint").style.display = "none";
	if(document.form.selPPTPOption.value == "+mppe-40") {
		document.getElementById("pptpOptionHint").style.display = "";
	}
}

</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_VPNClient_Content.asp">
<input type="hidden" name="next_page" value="Advanced_VPNClient_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="flag" value="background">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_vpncall">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vpnc_pppoe_username" value="<% nvram_get("vpnc_pppoe_username"); %>">
<input type="hidden" name="vpnc_pppoe_passwd" value="<% nvram_get("vpnc_pppoe_passwd"); %>">
<input type="hidden" name="vpnc_heartbeat_x" value="<% nvram_get("vpnc_heartbeat_x"); %>">
<input type="hidden" name="vpnc_dnsenable_x" value="1">
<input type="hidden" name="vpnc_proto" value="<% nvram_get("vpnc_proto"); %>">
<input type="hidden" name="vpnc_clientlist" value='<% nvram_clean_get("vpnc_clientlist"); %>'>
<input type="hidden" name="vpnc_type" value="PPTP">
<input type="hidden" name="vpnc_auto_conn" value="<% nvram_get("vpnc_auto_conn"); %>">
<input type="hidden" name="vpnc_pptp_options_x" value="<% nvram_get("vpnc_pptp_options_x"); %>">
<input type="hidden" name="vpnc_pptp_options_x_list" value="<% nvram_get("vpnc_pptp_options_x_list"); %>">

<div id="vpnc_settings"  class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
	<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
		<tr style="height:32px;">
			<td>		
				<table width="100%" border="0" align="left" cellpadding="0" cellspacing="0" class="vpnClientTitle">
					<tr>
			  		<td width="50%" align="center" id="pptpcTitle" onclick="tabclickhandler(0);">PPTP</td>
			  		<td width="50%" align="center" id="l2tpcTitle" onclick="tabclickhandler(1);">L2TP</td>
					</tr>
				</table>
			</td>
		</tr>
		<tr>
			<td>
				<!---- vpnc_pptp/l2tp start  ---->
				<div>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
					<tr>
						<th><#IPConnection_autofwDesc_itemname#></th>
						<td>
						  	<input type="text" maxlength="64" name="vpnc_des_edit" value="" class="input_32_table" style="float:left;" autocorrect="off" autocapitalize="off"></input>
						</td>
					</tr>

					<tr>
						<th><#BOP_isp_heart_item#></th>
						<td>
							<input type="text" maxlength="64" name="vpnc_svr_edit" value="" class="input_32_table" style="float:left;" autocorrect="off" autocapitalize="off"></input>
						</td>
					</tr>

					<tr>
						<th><#HSDPAConfig_Username_itemname#></th>
						<td>
							<input type="text" maxlength="64" name="vpnc_account_edit" value="" class="input_32_table" style="float:left;" autocomplete="off" autocorrect="off" autocapitalize="off"></input>
						</td>
					</tr>

					<tr>
						<th><#HSDPAConfig_Password_itemname#></th>
						<td>
							<input type="text" maxlength="64" name="vpnc_pwd_edit" value="" class="input_32_table" style="float:left;" autocomplete="off" autocorrect="off" autocapitalize="off"></input>
						</td>
					</tr>
					<tr id="trPPTPOptions">
						<th><#PPPConnection_x_PPTPOptions_itemname#></th>
						<td>
							<select name="selPPTPOption" class="input_option" onchange="pptpOptionChange();">
								<option value="auto"><#Auto#></option>
								<option value="-mppc"><#No_Encryp#></option>
								<option value="+mppe-40">MPPE 40</option>
								<option value="+mppe-128">MPPE 128</option>
							</select>
							<div id="pptpOptionHint" style="display:none;">
								<span><#PPTPOptions_OpenVPN_hint#><!--untranslated--></span>
							</div>
						</td>	
					</tr>
					</table>
		 		</div>
		 		<!---- vpnc_pptp/l2tp end  ---->		 			 	
			</td>
		</tr>
	</table>
	<div style="margin-top:5px;padding-bottom:10px;width:100%;text-align:center;">
		<input class="button_gen" type="button" onclick="cancel_add_rule();" id="cancelBtn" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button" onclick="addRow_Group(10,save_flag, idx_tmp);" value="<#CTL_ok#>">	
	</div>	
      <!--===================================Ending of vpnc setting Content===========================================-->			
</div>
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
							<div>&nbsp;</div>
							<div class="formfonttitle">VPN - <#vpnc_title#></div>
							<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
							<div class="formfontdesc">
								<#vpnc_desc1#><br>
								<#vpnc_desc2#><br>
								<#vpnc_desc3#><br><br>		
								<#vpnc_step#>
								<ol>
 									<li><#vpnc_step1#>
 									<li><#vpnc_step2#>
 									<li><#vpnc_step3#>
								</ol>
							</div>
						</td>		
        			</tr>						
         			<tr>
           				<td>



							<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
								<thead>
									<tr>
										<td colspan="6" id="VPNServerList" style="border-right:none;height:22px;"><#BOP_isp_VPN_list#> <span id="rules_limit" style="color:#FFFFFF"></span></td>
									</tr>
								</thead>			
									<tr>
										<th width="10%" style="height:30px;"><#PPPConnection_x_WANLink_itemname#></th>
										<th><div><#IPConnection_autofwDesc_itemname#></div></th>
										<th width="15%"><div><#QIS_internet_vpn_type#></div></th>
										<th width="10%"><div><#pvccfg_edit#></div></th>
										<th width="10%"><div><#CTL_del#></div></th>
										<th width="25%"><div><#Connecting#></div></th>
									</tr>											
							</table>          					
							<div id="vpnc_clientlist_Block"></div>
							<div class="apply_gen">
								<input class="button_gen_long" onclick="Add_profile()" type="button" value="<#vpnc_step1#>">
							</div>
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
</form>
<div id="footer"></div>
</body>
</html>

