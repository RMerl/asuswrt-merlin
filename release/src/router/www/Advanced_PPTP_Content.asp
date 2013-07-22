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
<title><#Web_Title#> - <#BOP_isp_heart_item#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script>
<% login_state_hook(); %>
var pptpd_clientlist_array_ori = '<% nvram_char_to_ascii("","pptpd_clientlist"); %>';
var pptpd_clientlist_array = decodeURIComponent(pptpd_clientlist_array_ori);
var dualwan_mode = '<% nvram_get("wans_mode"); %>';

function initial(){
	show_menu();	
	showpptpd_clientlist();
	addOnlineHelp($("faq"), ["ASUSWRT", "VPN"]);
	check_pptpd_broadcast();
	if(dualwan_mode == "lb"){
		$('wan_ctrl').style.display = "none";
		$('dualwan_ctrl').style.display = "";	
	}
		
	check_pptp_server();	
}

function applyRule(){
	var rule_num = $('pptpd_clientlist_table').rows.length;
	var item_num = $('pptpd_clientlist_table').rows[0].cells.length;
	var tmp_value = "";
	
	
	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){
			
			if($('pptpd_clientlist_table').rows[i].cells[j].innerHTML.lastIndexOf("...")<0){
				tmp_value += $('pptpd_clientlist_table').rows[i].cells[j].innerHTML;
			}else{				
				tmp_value += $('pptpd_clientlist_table').rows[i].cells[j].title;
			}					
			
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	
	document.form.pptpd_clientlist.value = tmp_value;

	showLoading();
	document.form.submit();	
}

function addRow(obj, head){
	if(head == 1)
		pptpd_clientlist_array += "<" /*&#60*/
	else
		pptpd_clientlist_array += ">" /*&#62*/
			
	pptpd_clientlist_array += obj.value;
	obj.value = "";
}

function validForm(){
	if(document.form.pptpd_clientlist_username.value==""){
		alert("<#JS_fieldblank#>");
		document.form.pptpd_clientlist_username.focus();
		return false;		
	}else if(!Block_chars(document.form.pptpd_clientlist_username, [" ", "@", "*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"" ])){
		return false;		
	}

	if(document.form.pptpd_clientlist_password.value==""){
		alert("<#JS_fieldblank#>");
		document.form.pptpd_clientlist_password.focus();
		return false;
	}else if(!Block_chars(document.form.pptpd_clientlist_password, ["<", ">"])){
		return false;		
	}
		
	return true;				
}

function addRow_Group(upper){
	var rule_num = $('pptpd_clientlist_table').rows.length;
	var item_num = $('pptpd_clientlist_table').rows[0].cells.length;		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}			

	if(validForm()){
		//Viz check same rule  //match(username) is not accepted
		if(item_num >=2){
			for(i=0; i<rule_num; i++){	
				if(document.form.pptpd_clientlist_username.value.toLowerCase() == $('pptpd_clientlist_table').rows[i].cells[0].innerHTML.toLowerCase()){
					alert("<#JS_duplicate#>");
					document.form.pptpd_clientlist_username.focus();
					document.form.pptpd_clientlist_username.select();
					return false;
				}	
			}
		}		
		
		addRow(document.form.pptpd_clientlist_username ,1);
		addRow(document.form.pptpd_clientlist_password, 0);
		showpptpd_clientlist();				
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('pptpd_clientlist_table').deleteRow(i);
  
  var pptpd_clientlist_value = "";
	for(k=0; k<$('pptpd_clientlist_table').rows.length; k++){
		for(j=0; j<$('pptpd_clientlist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				pptpd_clientlist_value += "<";
			else{
				pptpd_clientlist_value += $('pptpd_clientlist_table').rows[k].cells[0].firstChild.innerHTML;
				pptpd_clientlist_value += ">";
				pptpd_clientlist_value += $('pptpd_clientlist_table').rows[k].cells[1].firstChild.innerHTML;
			}
		}
	}

	pptpd_clientlist_array = pptpd_clientlist_value;
	if(pptpd_clientlist_array == "")
		showpptpd_clientlist();
}

function edit_Row(r){ 	
	var i=r.parentNode.parentNode.rowIndex;
	document.form.pptpd_clientlist_username.value = $('pptpd_clientlist_table').rows[i].cells[0].innerHTML;
	document.form.pptpd_clientlist_password.value = $('pptpd_clientlist_table').rows[i].cells[1].innerHTML; 	
	del_Row(r);	
}

var overlib_str0 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
var overlib_str1 = new Array();	//Viz add 2013.04 for record longer VPN client username/pwd
function showpptpd_clientlist(){	
	var pptpd_clientlist_row = pptpd_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="pptpd_clientlist_table">';
	if(pptpd_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{		
		for(var i = 1; i < pptpd_clientlist_row.length; i++){
			overlib_str0[i] = "";
			overlib_str1[i] = "";
			code +='<tr id="row'+i+'">';
			var pptpd_clientlist_col = pptpd_clientlist_row[i].split('>');
				for(var j = 0; j < pptpd_clientlist_col.length; j++){
						if(j == 0){
								if(pptpd_clientlist_col[0].length >32){
										overlib_str0[i] += pptpd_clientlist_col[0];
										pptpd_clientlist_col[0]=pptpd_clientlist_col[0].substring(0, 30)+"...";
										code +='<td width="40%" title="'+overlib_str0[i]+'">'+ pptpd_clientlist_col[0] +'</td>';
								}else
										code +='<td width="40%">'+ pptpd_clientlist_col[0] +'</td>';
						}
						else if(j ==1){
								if(pptpd_clientlist_col[1].length >32){
										overlib_str1[i] += pptpd_clientlist_col[1];
										pptpd_clientlist_col[1]=pptpd_clientlist_col[1].substring(0, 30)+"...";
										code +='<td width="40%" title="'+overlib_str1[i]+'">'+ pptpd_clientlist_col[1] +'</td>';
								}else
										code +='<td width="40%">'+ pptpd_clientlist_col[1] +'</td>';
						} 
				}
				code +='<td width="20%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	$("pptpd_clientlist_Block").innerHTML = code;
}

// test if Private ip
function valid_IP(obj_name, obj_flag){
		// A : 1.0.0.0~126.255.255.255
		// B : 127.0.0.0~127.255.255.255 (forbidden)
		// C : 128.0.0.0~255.255.255.254
		var A_class_start = inet_network("1.0.0.0");
		var A_class_end = inet_network("126.255.255.255");
		var B_class_start = inet_network("127.0.0.0");
		var B_class_end = inet_network("127.255.255.255");
		var C_class_start = inet_network("128.0.0.0");
		var C_class_end = inet_network("255.255.255.255");
		
		var ip_obj = obj_name;
		var ip_num = inet_network(ip_obj.value);	//-1 means nothing
		
		//1~254
		if(obj_name.value.split(".")[3] < 1 || obj_name.value.split(".")[3] > 254){
			alert('<#JS_validrange#> ' + 1 + ' <#JS_validrange_to#> ' + 254);
			obj_name.focus();
			return false;
		}
		
		
		if(ip_num > A_class_start && ip_num < A_class_end)
			return true;
		else if(ip_num > B_class_start && ip_num < B_class_end){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
		else if(ip_num > C_class_start && ip_num < C_class_end)
			return true;
		else{
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
}

function check_pptpd_broadcast(){
	if(document.form.pptpd_broadcast.value =="ppp" || document.form.pptpd_broadcast.value =="br0ppp")
		$('pptpd_broadcast_ppp_yes').checked="true";
	else
		$('pptpd_broadcast_ppp_no').checked="true";	
}

function set_pptpd_broadcast(obj){
	var pptpd_temp;	
	pptpd_temp = document.form.pptpd_broadcast.value;

	if(obj.value ==1){
		if(pptpd_temp == "br0")
			document.form.pptpd_broadcast.value="br0ppp";
		else	
			document.form.pptpd_broadcast.value="ppp";
	}
	else{
		if(pptpd_temp == "br0ppp")
			document.form.pptpd_broadcast.value="br0";
		else	
			document.form.pptpd_broadcast.value="disable";
	}
}

function check_pptp_server(){
	if(document.form.pptpd_enable[0].checked){
		$('pptp_samba').style.display = "";
		document.form.pptpd_broadcast.disabled = false;
	}
	else{
		$('pptp_samba').style.display = "none";
		document.form.pptpd_broadcast.disabled = true;
	}
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
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
			<input type="hidden" name="current_page" value="Advanced_PPTP_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_PPTP_Content.asp">
			<input type="hidden" name="next_host" value="">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="restart_pptpd">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="pptpd_clientlist" value="<% nvram_char_to_ascii("","pptpd_clientlist"); %>">
			<input type="hidden" name="pptpd_broadcast" value="<% nvram_get("pptpd_broadcast"); %>">	
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#BOP_isp_heart_item#> - <#t2BC#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc"><#PPTP_desc#></div>
									<div id="wan_ctrl" class="formfontdesc"><#PPTP_desc2#> <% nvram_get("wan0_ipaddr"); %></div>
									<div id="dualwan_ctrl" style="display:none;" class="formfontdesc"><#PPTP_desc2#> <span class="formfontdesc">Primary WAN IP : <% nvram_get("wan0_ipaddr"); %> </sapn><span class="formfontdesc">Secondary WAN IP : <% nvram_get("wan1_ipaddr"); %> </sapn></div>
									<div class="formfontdesc" style="margin-top:-10px;font-weight: bolder;"><#PPTP_desc3#></div>
									<div class="formfontdesc" style="margin-top:-10px;">
										(7) <a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;"><#BOP_isp_heart_item#> FAQ</a>
									</div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										<thead>
										<tr>
											<td colspan="3" id="GWStatic"><#t2BC#></td>
										</tr>
										</thead>											
										<tr>
											<th><#vpn_enable#></th>
											<td>
													<input type="radio" value="1" name="pptpd_enable" onclick="check_pptp_server();" <% nvram_match("pptpd_enable", "1", "checked"); %> /><#checkbox_Yes#>
													<input type="radio" value="0" name="pptpd_enable" onclick="check_pptp_server();" <% nvram_match("pptpd_enable", "0", "checked"); %> /><#checkbox_No#>	
											</td>
										</tr>
										<tr id="pptp_samba">
											<th><#vpn_network_place#></th>
											<td>
													<input type="radio" value="1" id="pptpd_broadcast_ppp_yes" name="pptpd_broadcast_ppp" onchange="set_pptpd_broadcast(this);"/><#checkbox_Yes#>
													<input type="radio" value="0" id="pptpd_broadcast_ppp_no" name="pptpd_broadcast_ppp" onchange="set_pptpd_broadcast(this);"/><#checkbox_No#>										
											</td>
										</tr>
									</table>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
										<thead>
										<tr>
											<td colspan="3" id="GWStatic"><#Username_Pwd#>&nbsp;(<#List_limit#>&nbsp;16)</td>
										</tr>
										</thead>								
										<tr>
											<th><#PPPConnection_UserName_itemname#></th>
											<th><#PPPConnection_Password_itemname#></th>
											<th>Add / Delete</th>
										</tr>			  
										<tr>
											<td width="40%">
												<input type="text" class="input_25_table" maxlength="64" name="pptpd_clientlist_username" onKeyPress="return is_string(this, event)">
											</td>
											<td width="40%">
												<input type="text" class="input_25_table" maxlength="64" name="pptpd_clientlist_password" onKeyPress="return is_string(this, event)">
											</td>
											<td width="20%">
												<div><input type="button" class="add_btn" onClick="addRow_Group(16);" value=""></div>
											</td>
										</tr>	 			  
									</table>        														
									<div id="pptpd_clientlist_Block"></div>	
									<!-- manually assigned the DHCP List end-->		
									<div class="apply_gen">
										<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
									</div>		
								</td>
							</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>
			<td width="10" align="center" valign="top">&nbsp;</td>
		</td>
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
