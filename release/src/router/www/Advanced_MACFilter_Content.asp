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
<title><#Web_Title#> - <#menu5_5_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_mac = login_mac_str();
var macfilter_num_x = '<% nvram_get("macfilter_num_x"); %>';
var smac = client_mac.split(":");
var simply_client_mac = smac[0] + smac[1] + smac[2] + smac[3] + smac[4] + smac[5];
var macfilter_rulelist_array = '<% nvram_get("macfilter_rulelist"); %>';

function initial(){
	show_menu();
	load_body();
	showmacfilter_rulelist();
}

function showmacfilter_rulelist(){
	var macfilter_rulelist_row = macfilter_rulelist_array.split('&#60');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="macfilter_rulelist_table">'; 
	if(macfilter_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 1; i < macfilter_rulelist_row.length; i++){
			code +='<tr id="row'+i+'">';

			var macfilter_rulelist_col = macfilter_rulelist_row[i].split('&#62');
			for(var j = 0; j < macfilter_rulelist_col.length; j++){
				code +='<td width="40%">'+ macfilter_rulelist_col[j] +'</td>';
			}
			if (j != 2) code +='<td width="40%"></td>';
			code +='<td width="20%">';		
			code +="<input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow(this);\" value=\"\"/></td>";
		}

	}
  	code +='</tr></table>';
	
	$("macfilter_rulelist_Block").innerHTML = code;
}

function deleteRow(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('macfilter_rulelist_table').deleteRow(i);
  
  var macfilter_rulelist_value = "";
	for(i=0; i<$('macfilter_rulelist_table').rows.length; i++){
		macfilter_rulelist_value += "&#60";
		macfilter_rulelist_value += $('macfilter_rulelist_table').rows[i].cells[0].innerHTML;
		macfilter_rulelist_value += "&#62";
                macfilter_rulelist_value += $('macfilter_rulelist_table').rows[i].cells[1].innerHTML;
	}
	
	macfilter_rulelist_array = macfilter_rulelist_value;
	if(macfilter_rulelist_array == "")
		showmacfilter_rulelist();
}

function addRow(obj, obj2, upper){
		var rule_num = $('macfilter_rulelist_table').rows.length;//rule lists 
		var item_num = $('macfilter_rulelist_table').rows[0].cells.length; //3 column
		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}					
	
	if(obj.value==""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();		
		return false;
	}else if(!check_macaddr(obj, check_hwaddr_flag(obj))){
		obj.focus();
		obj.select();		
		return false;	
	}	
	
		
		//Viz check same rule
		for(i=0; i<rule_num; i++){
			for(j=0; j<item_num-2; j++){		//only 1 value column
				//alert(obj.value+","+$('macfilter_rulelist_table').rows[i].cells[j].innerHTML);
				if(obj.value.toLowerCase() == $('macfilter_rulelist_table').rows[i].cells[j].innerHTML.toLowerCase()){
					alert("<#JS_duplicate#>");
					return false;
				}	
			}
		}
		
		macfilter_rulelist_array += "&#60";
		macfilter_rulelist_array += obj.value;
		macfilter_rulelist_array += "&#62";
		macfilter_rulelist_array += obj2.value;
		obj.value = "";
		obj2.value = "";
		showmacfilter_rulelist();
		
}

function applyRule(){
	if(prevent_lock()){
		var rule_num = $('macfilter_rulelist_table').rows.length;//幾組rule list 
		var item_num = $('macfilter_rulelist_table').rows[0].cells.length; //兩欄位
		var tmp_value = "";
	
		for(i=0; i<rule_num; i++){
			tmp_value += "<"		
			for(j=0; j<item_num-1; j++){	
				//Viz alert($('macfilter_rulelist_table').rows[i].cells[j].innerHTML);
				tmp_value += $('macfilter_rulelist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)	
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";	
		
		document.form.macfilter_rulelist.value = tmp_value;
		// Viz alert(tmp_value);
		
		showLoading();
		document.form.submit();
}
	else
		return false;
}

function prevent_lock(){
	if(document.form.macfilter_enable_x.value == "1"){
		if(macfilter_num_x == 0){
			if(confirm("<#FirewallConfig_MFList_accept_hint1#>")){
				return true;
			}
			else
				return false;
		}
		else
			return true;
	}
	else
		return true;
}

function done_validating(action){
	refreshpage();
}
function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		$("check_mac").style.display = "";
		return false;
	}if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";		
		$("check_mac").style.display = "";
		return false;		
	}else{
		$("check_mac") ? $("check_mac").style.display="none" : true;
		return true;
	}	
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<input type="hidden" name="current_page" value="Advanced_MACFilter_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="macfilter_rulelist" value="<% nvram_get("macfilter_rulelist"); %>">

<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_5#> - <#menu5_5_3#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#FirewallConfig_display5_sectiondesc#></div>
	  
	  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					  <thead>
					  <tr>
						<td colspan="4"><#t2BC#></td>
					  </tr>
					  </thead>

        	<tr>
        		<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,1);"><#FirewallConfig_MFMethod_itemname#></a></th>
          		<td>
          			<select name="macfilter_enable_x" class="input_option" onchange="return change_common(this, 'FirewallConfig', 'macfilter_enable_x')">
              			<option value="0" <% nvram_match("macfilter_enable_x", "0","selected"); %>><#CTL_Disabled#></option>
              			<option value="1" <% nvram_match("macfilter_enable_x", "1","selected"); %>><#FirewallConfig_MFMethod_item1#></option>
              			<option value="2" <% nvram_match("macfilter_enable_x", "2","selected"); %>><#FirewallConfig_MFMethod_item2#></option>
          			</select>
          		</td>
        	</tr>
        </table>
        
        <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">	
					  <thead>
					  <tr>
						<td colspan="4"><#FirewallConfig_MFList_groupitemname#></td>
					  </tr>
					  </thead>

        	<tr>
          		<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">
          			<#MAC_Address#></a>
              		<input type="hidden" name="macfilter_num_x_0" value="<% nvram_get("macfilter_num_x"); %>" readonly="1"/>
		  	</th>
			<th width="40%">Name</th>
			<th width="20%">Add / Delete</th>
        	</tr>
        	<tr>
          		<td width="40%">
          			<input type="text" maxlength="17" class="input_macaddr_table" name="macfilter_list_x_0"  onKeyPress="return is_hwaddr(this,event)">
		  	</td>          			
			<td width="40%">
				<input type="text" class="input_15_table" maxlenght="15" onKeypress="return is_alphanum(this,event);" name="macfilter_name_x_0">
			</td>
		  	<td width="20%">
		  		<input class="add_btn" type="button" onclick="addRow(document.form.macfilter_list_x_0, macfilter_name_x_0, 32);" value="">
		  	</td>
        	</tr>
        </table>
        
        	<div id="macfilter_rulelist_Block"></div>
      
          <div class="apply_gen">
          		<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
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
