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
<title><#Web_Title#> - VPN Client Settings</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
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
	margin-top: 10px;
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
.QISmain{
	width:730px;
	/*font-family:Verdana, Arial, Helvetica, sans-serif;*/
	font-size:14px;
	z-index:200;
	position:relative;
	background-color:balck:
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
               
.QISform_wireless li{	
	margin-top:10px;
}
.QISGeneralFont{
	font-family:Segoe UI, Arial, sans-serif;
	margin-top:10px;
	margin-left:70px;
	*margin-left:50px;
	margin-right:30px;
	color:white;
	LINE-HEIGHT:18px;
}	
.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;	
}
.vpnctype{
		font-weight: bolder;
	  background: transparent url(images/New_ui/tabclickspan.png) no-repeat;
		font-family: Arial, Helvetica, sans-serif;	 
    display: block;
    padding: 5px 4px 5px 12px;
    line-height: 20px;	
		font-size: 13px; 
}

.tab_NW {	
	font-weight: bolder;
	background: transparent url(images/New_ui/taba.png) no-repeat scroll right top;
	color: White;
	display: block;
	float: left;
	height: 28px;
	padding-right: 8px;      
	margin-right:2px;
	text-decoration: none;
	text-align:center;
	max-width:130px;
}

.formfonttitle_nwm1{
font-family:Arial, Helvetica, sans-serif;
font-size:12px;
color:#FFFFFF;
margin-left:5px;
margin-bottom:10px;
font-weight: bolder;
}

.input_15_table1{
	margin-left:2px;
	padding-left:0.4em;
	height:23px;
	width:360px;
	line-height:23px \9;	/*IE*/
	font-size:12px;
	font-family: Lucida Console;
	background-image:url(/images/New_ui/inputbg.png);
	border-width:0;
	color:#FFFFFF;
}

</style>
<script>
var $j = jQuery.noConflict();

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var vpnc_clientlist = decodeURIComponent('<% nvram_char_to_ascii("","vpnc_clientlist"); %>');
var vpnc_clientlist_array_ori = '<% nvram_char_to_ascii("","vpnc_clientlist"); %>';
var vpnc_clientlist_array = decodeURIComponent(vpnc_clientlist_array_ori);
var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd

function initial(){
	show_menu();
	show_vpnc_rulelist();
}

function submitQoS(){
	parent.showLoading();
	document.form.submit();		
}

function Add_profile(){
	document.form.vpnc_des_edit.value = "";
	document.form.vpnc_svr_edit.value = "";
	document.form.vpnc_account_edit.value = "";
	document.form.vpnc_pwd_edit.value = "";
	tabclickhandler(0);

	$("cancelBtn").style.display = "";
	$j("#vpnsvr_setting").fadeIn(300);
	if (vpnc_support == false){
		tabclickhandler(2);
		document.getElementById('pptpcTitle').style.display = "none";
		document.getElementById('l2tpcTitle').style.display = "none";
	}
}

function cancel_add_rule(){
	$j("#vpnsvr_setting").fadeOut(300);
}

function addRow_Group(upper, flag){
		var table_id = "vpnc_clientlist_table";
		var rule_num = $(table_id).rows.length;
		var item_num = $(table_id).rows[0].cells.length;		
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
			//Viz check same rule  //match(description) is not accepted
			if(item_num >= 2){
				for(i=0; i<rule_num; i++){
					if(description_obj.value.toLowerCase() == $(table_id).rows[i].cells[1].innerHTML.toLowerCase()){
						alert("<#JS_duplicate#>");
						description_obj.focus();
						description_obj.select();
						return false;
					}	
				}
			}

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

			if(vpnc_clientlist_array.charAt(0) == "<")
				vpnc_clientlist_array = vpnc_clientlist_array.substr(1,vpnc_clientlist_array.length);

			show_vpnc_rulelist();
			cancel_add_rule();
		}

		document.form.vpnc_clientlist.value = vpnc_clientlist_array;
		document.form.submit();
}

var duplicateCheck = {
	tmpStr: "",

	saveToTmpStr: function(obj, head){	
		if(head != 1)
			this.tmpStr += ">" /*&#62*/

		this.tmpStr += obj.value;
	},

	isDuplicate: function(){
		if(vpnc_clientlist_array.search(this.tmpStr)  != -1)
			return true;
		else
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
	}else if(!Block_chars(valid_des, ["*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"" ])){
		return false;		
	}

	if(valid_server.value==""){
		alert("<#JS_fieldblank#>");
		valid_server.focus();
		return false;
	}else if(!Block_chars(valid_server, ["<", ">"])){
		return false;		
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
	}
	else if(_type == 1){
		save_flag = "L2TP";
		document.form.vpnc_type.value = "L2TP";
		document.getElementById('l2tpcTitle').className = "vpnClientTitle_td_click";
	}
}

function show_vpnc_rulelist(){
	if(vpnc_clientlist_array[0] == "<")
		vpnc_clientlist_array = vpnc_clientlist_array.split("<")[1];

	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');	
	var code = "";
	code +='<table style="margin-bottom:30px;" width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="list_table" id="vpnc_clientlist_table">';
	if(vpnc_clientlist_array == "")
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
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
				if(vpnc_state_t == 0) // initial
					code +='<td width="10%"><img src="/images/InternetScan.gif"></td>';
				else if(vpnc_state_t == 1) // disconnect
					code +='<td width="10%"><img src="/images/button-close2.png" style="width:25px;"></td>';
				else // connected
					code +='<td width="10%"><img src="/images/checked_parentctrl.png" style="width:25px;"></td>';
			}
			else
				code +='<td width="10%">-</td>';

			for(var j=0; j<vpnc_clientlist_col.length; j++){
				if(j == 0){
					if(vpnc_clientlist_col[0].length >32){						
						overlib_str0[i] += vpnc_clientlist_col[0];							
						vpnc_clientlist_col[0]=vpnc_clientlist_col[0].substring(0, 30)+"...";							
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

			if(vpnc_clientlist_col[1] == document.form.vpnc_proto.value.toUpperCase() &&
				 vpnc_clientlist_col[2] == document.form.vpnc_heartbeat_x.value && 
				 vpnc_clientlist_col[3] == document.form.vpnc_pppoe_username.value)
			{
				code += '<td width="10%"><input class="remove_btn" type="button" onclick="del_Row(this, \'vpnc_enable\');" value=""/></td>';
				code += '<td width="25%"><input class="button_gen" type="button" onClick="connect_Row(this, \'disconnect\');" id="disonnect_btn" value="Disconnect" style="padding:0 0.3em 0 0.3em;" >';
			}
			else{
				code += '<td width="10%"><input class="remove_btn" type="button" onclick="del_Row(this, \'vpnc\');" value=""/></td>';
				code += '<td width="25%"><input class="button_gen" type="button" onClick="connect_Row(this, \'vpnc\');" id="Connect_btn" name="Connect_btn" value="Connect" style="padding:0 0.3em 0 0.3em;" >';
			}
		}
	}
  code +='</table>';
	$("vpnc_clientlist_Block").innerHTML = code;		
}


function connect_Row(rowdata, flag){
	if(flag == "disconnect"){
		document.form.vpnc_proto.value = "disable";
		document.form.vpnc_heartbeat_x.value = "";
		document.form.vpnc_pppoe_username.value = "";
		document.form.vpnc_pppoe_passwd.value = "";
	}
	else{
		var idx = rowdata.parentNode.parentNode.rowIndex;
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
				if(vpnc_clientlist_col[1] == "OpenVPN")
					document.form.vpn_client_unit.value = 1;	//1, 2
				else
					document.form.vpnc_heartbeat_x.value = vpnc_clientlist_col[2];
			} 
			else if(j ==3){
				if(vpnc_clientlist_col[1] == "OpenVPN")
					document.form.vpn_client1_username.value = vpnc_clientlist_col[3];
				else
					document.form.vpnc_pppoe_username.value = vpnc_clientlist_col[3];
			} 
			else if(j ==4){
				if(vpnc_clientlist_col[1] == "OpenVPN")				
					document.form.vpn_client1_password.value = vpnc_clientlist_col[4];
				else	
					document.form.vpnc_pppoe_passwd.value = vpnc_clientlist_col[4];
			} 
		}
		document.form.vpnc_clientlist.value = vpnc_clientlist_array;
	}

	rowdata.parentNode.innerHTML = "<img src='/images/InternetScan.gif'>";
	document.form.submit();	
}

function Edit_Row(rowdata, flag){
	$("cancelBtn").style.display = "none";

	var idx = rowdata.parentNode.parentNode.rowIndex;
	var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
	var vpnc_clientlist_col = vpnc_clientlist_row[idx].split('>');

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

	$j("#vpnsvr_setting").fadeIn(300);
	del_Row(rowdata, flag);
}

function del_Row(rowdata, flag){  
  var idx = rowdata.parentNode.parentNode.rowIndex;
  if(flag.search("vpnc") != -1){		
		$('vpnc_clientlist_table').deleteRow(idx);

		var vpnc_clientlist_value = "";
		var vpnc_clientlist_row = vpnc_clientlist_array.split('<');
		for(k=0; k<vpnc_clientlist_row.length; k++){
			if(k != idx){
				if(k != 0)
					vpnc_clientlist_value += "<";
				vpnc_clientlist_value += vpnc_clientlist_row[k];
			}
		}

		vpnc_clientlist_array = vpnc_clientlist_value;
		if(vpnc_clientlist_array == "")
			show_vpnc_rulelist();

		if(flag == "vpnc_enable"){
			document.form.vpnc_proto.value = "disable";
			document.form.vpnc_proto.disabled = false;

			document.form.action = "/start_apply.htm";
			document.form.enctype = "application/x-www-form-urlencoded";
			document.form.encoding = "application/x-www-form-urlencoded";

			document.form.action_script.value = "restart_vpncall";
		}
	}

	document.form.vpnc_clientlist.value = vpnc_clientlist_array;
	document.form.submit();
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
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="flag" value="vpnClient">
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
<input type="hidden" name="vpn_client_unit" value="1">
<input type="hidden" name="vpn_client1_username" value="<% nvram_get("vpn_client1_username"); %>">
<input type="hidden" name="vpn_client1_password" value="<% nvram_get("vpn_client1_password"); %>">

<div id="vpnsvr_setting"  class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
	<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
		<tr>
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
				<div id="vpnc_pptpl2tp">
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
			 		<tr>
						<th><#IPConnection_autofwDesc_itemname#></th>
					  <td>
					  	<input type="text" name="vpnc_des" id="vpnc_des_edit" value="" class="input_32_table" style="float:left;"></input>
					  </td>
		  		</tr>  
		
		  		<tr>
						<th>Server</th>
					  <td>
					  	<input type="text" name="vpnc_svr" id="vpnc_svr_edit" value="" class="input_32_table" style="float:left;"></input>
					  </td>
		  		</tr>  
		
		  		<tr>
						<th><#PPPConnection_UserName_itemname#></th>
					  <td>
					  	<input type="text" name="vpnc_account" id="vpnc_account_edit" value="" class="input_32_table" style="float:left;"></input>
					  </td>
		  		</tr>  
		
		  		<tr>
						<th><#PPPConnection_Password_itemname#></th>
					  <td>
					  	<input type="text" name="vpnc_pwd" id="vpnc_pwd_edit" value="" class="input_32_table" style="float:left;"></input>
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
		<input class="button_gen" type="button" onclick="addRow_Group(10,save_flag);" value="<#CTL_ok#>">	
	</div>	
      <!--===================================Ending of vpnsvr setting Content===========================================-->			
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
							  <td bgcolor="#4D595D" valign="top"  >
							  <div>&nbsp;</div>
							  <div class="formfonttitle">VPN - VPN Client</div>
					      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					      <div class="formfontdesc">
									VPN (Virtual Private Network) client is often applied to allow a person connecting to VPN server to access private or special resource securely over a public network.<br>
									However, some devices like set top box, smart TV or Blu-ray player are not able to install VPN software.<br>
									ASUSWRT VPN client allows all devices in the network to enjoy the VPN function without installing VPN software in each device.<br>
									
									To start a VPN connection, please follow the steps below:
									<ol>
									<li>Add profile
									<li>Select a VPN connection type (PPTP or L2TP)
									<li>Enter VPN authentication information provided by your VPN provider then connect.
									</ol>
								</div>
        			</tr>
							
							<tr>
								<td valign="top">
									<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">

									</table>
								</td>
							</tr>	
        			<tr>
          				<td>
							<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
							<thead>
							<tr>
									<td colspan="6" id="VPNServerList" style="border-right:none;height:22px;">VPN server list (Max limit:10)</td>
							</tr>
							</thead>			
							<tr>
									<th width="10%" style="height:30px;">Status</th>
									<th><div>Description</div></th>
									<th width="15%"><div>Type</div></th>
									<th width="10%"><div>Edit</div></th>
									<th width="10%"><div>Delete</div></th>
									<th width="25%"><div>Connection</div></th>
							</tr>											
							</table>          					

							<div id="vpnc_clientlist_Block"></div>
							<div class="apply_gen">
								<input class="button_gen" onclick="Add_profile()" type="button" value="Add profile">
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
