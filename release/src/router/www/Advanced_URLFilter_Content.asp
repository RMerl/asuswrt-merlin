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
<title><#Web_Title#> - <#menu5_5_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var url_rulelist_array = "<% nvram_char_to_ascii("","url_rulelist"); %>";

function initial(){
	show_menu();
	init_setting();
	show_url_rulelist();
	enable_url();
	enable_url_1();
}

function init_setting(){
	document.form.url_date_x_Sun.checked = getDateCheck(document.form.url_date_x.value, 0);
	document.form.url_date_x_Mon.checked = getDateCheck(document.form.url_date_x.value, 1);
	document.form.url_date_x_Tue.checked = getDateCheck(document.form.url_date_x.value, 2);
	document.form.url_date_x_Wed.checked = getDateCheck(document.form.url_date_x.value, 3);
	document.form.url_date_x_Thu.checked = getDateCheck(document.form.url_date_x.value, 4);
	document.form.url_date_x_Fri.checked = getDateCheck(document.form.url_date_x.value, 5);
	document.form.url_date_x_Sat.checked = getDateCheck(document.form.url_date_x.value, 6);
	document.form.url_time_x_starthour.value = getTimeRange(document.form.url_time_x.value, 0);
	document.form.url_time_x_startmin.value = getTimeRange(document.form.url_time_x.value, 1);
	document.form.url_time_x_endhour.value = getTimeRange(document.form.url_time_x.value, 2);
	document.form.url_time_x_endmin.value = getTimeRange(document.form.url_time_x.value, 3);
	document.form.url_time_x_starthour_1.value = getTimeRange(document.form.url_time_x_1.value, 0);
	document.form.url_time_x_startmin_1.value = getTimeRange(document.form.url_time_x_1.value, 1);
	document.form.url_time_x_endhour_1.value = getTimeRange(document.form.url_time_x_1.value, 2);
	document.form.url_time_x_endmin_1.value = getTimeRange(document.form.url_time_x_1.value, 3);
}

function show_url_rulelist(){
	var url_rulelist_row = decodeURIComponent(url_rulelist_array).split('<');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="url_rulelist_table">'; 
	if(url_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i =1; i < url_rulelist_row.length; i++){
		code +='<tr id="row'+i+'">';
		code +='<td width="80%">'+ url_rulelist_row[i] +'</td>';		//Url keyword
		code +='<td width="20%">';
		code +="<input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow(this);\" value=\"\"/></td>";
		}
	}
  	code +='</tr></table>';
	
	$("url_rulelist_Block").innerHTML = code;
}

function deleteRow(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('url_rulelist_table').deleteRow(i);
  
  var url_rulelist_value = "";
	for(i=0; i<$('url_rulelist_table').rows.length; i++){
		url_rulelist_value += "<";
		url_rulelist_value += $('url_rulelist_table').rows[i].cells[0].innerHTML;
	}
	
	url_rulelist_array =url_rulelist_value;
	if(url_rulelist_array == "")
		show_url_rulelist();
}

function addRow(obj, upper){
	if(validForm(obj)){	
		if('<% nvram_get("url_enable_x"); %>' != "1")
			document.form.url_enable_x[0].checked = true;
		
		var rule_num = $('url_rulelist_table').rows.length;
		var item_num = $('url_rulelist_table').rows[0].cells.length;
		
		// skip upper bound checking by jerry5
		/*if(rule_num >= upper){
				alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
				return false;	
		}*/	
				//Viz check same rule
				for(i=0; i<rule_num; i++){
						for(j=0; j<item_num-1; j++){		//only 1 value column
								if(obj.value.toLowerCase() == $('url_rulelist_table').rows[i].cells[j].innerHTML.toLowerCase()){
										alert("<#JS_duplicate#>");
										return false;
								}	
						}
				}
				
				url_rulelist_array += "<";
				url_rulelist_array += obj.value;
				obj.value = "";		
				show_url_rulelist();		
		
	}	
}

function applyRule(){
		var rule_num = $('url_rulelist_table').rows.length;
		var item_num = $('url_rulelist_table').rows[0].cells.length;
		var tmp_value = "";
	
		for(i=0; i<rule_num; i++){
			tmp_value += "<"		
			for(j=0; j<item_num-1; j++){	
				tmp_value += $('url_rulelist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)	
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";	
		document.form.url_rulelist.value = tmp_value;

		updateDateTime();

//		if(document.form.url_enable_x[0].checked == true && document.form.url_enable_x_orig.value != 1 ||
//				document.form.url_enable_x[1].checked == true && document.form.url_enable_x_orig.value != 0)
//			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");	
		if(tmo_support){
			FormActions("start_apply.htm", "apply_new", "restart_net_and_phy", "30");
		}
		
		showLoading();
		document.form.submit();
}

function validForm(obj){
	
		if(obj.value==""){
				alert("<#JS_fieldblank#>");
				obj.focus();
				obj.select();
				return false;				
		
		}else	if(!Block_chars(obj, ["#", "%" ,"&" ,"*" ,"{" ,"}" ,"\\" ,":" ,"<" ,">" ,"?" ,"/" ,"+"])){
				return false;		
		}		
	
		return true;
}

function enable_url(){
	if(document.form.url_enable_x[1].checked == 1)
		$("url_time").style.display = "none";
	else 
		$("url_time").style.display = "";	
	return change_common_radio(document.form.url_enable_x, 'FirewallConfig', 'url_enable_x', '1')
}

function enable_url_1(){
	if(document.form.url_enable_x_1[1].checked == 1)
		$("url_time_1").style.display = "none";
	else 
		$("url_time_1").style.display = "";	
	return change_common_radio(document.form.url_enable_x_1, 'FirewallConfig', 'url_enable_x_1', '1')
}

function done_validating(action){
	refreshpage();
}

function updateDateTime(){
	document.form.url_date_x.value = setDateCheck(
		document.form.url_date_x_Sun,
		document.form.url_date_x_Mon,
		document.form.url_date_x_Tue,
		document.form.url_date_x_Wed,
		document.form.url_date_x_Thu,
		document.form.url_date_x_Fri,
		document.form.url_date_x_Sat);
	document.form.url_time_x.value = setTimeRange(
		document.form.url_time_x_starthour,
		document.form.url_time_x_startmin,
		document.form.url_time_x_endhour,
		document.form.url_time_x_endmin);
	document.form.url_time_x_1.value = setTimeRange(
		document.form.url_time_x_starthour_1,
		document.form.url_time_x_startmin_1,
		document.form.url_time_x_endhour_1,
		document.form.url_time_x_endmin_1);
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_URLFilter_Content.asp">
<input type="hidden" name="next_page" value="Advanced_URLFilter_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="url_date_x" value="<% nvram_get("url_date_x"); %>">
<input type="hidden" name="url_time_x" value="<% nvram_get("url_time_x"); %>">
<input type="hidden" name="url_time_x_1" value="<% nvram_get("url_time_x_1"); %>">
<input type="hidden" name="url_num_x_0" value="<% nvram_get("url_num_x"); %>" readonly="1">
<input type="hidden" name="url_rulelist" value="<% nvram_get("url_rulelist"); %>">
<input type="hidden" name="url_enable_x_orig" value="<% nvram_get("url_enable_x"); %>">
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
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_5#> - <#menu5_5_2#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#FirewallConfig_UrlFilterEnable_sectiondesc#></div>
		  <div class="formfontdesc"><#FirewallConfig_KeywordFilterEnable_sectiondesc2#></div>	

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					  <thead>
					  <tr>
						<td colspan="4"><#t2BC#></td>
					  </tr>
					  </thead>		
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,1);"><#FirewallConfig_UrlFilterEnable_itemname#></th>
         	<td>  
         		<input type="radio" value="1" name="url_enable_x" onClick="enable_url();" <% nvram_match("url_enable_x", "1", "checked"); %>><#CTL_Enabled#>
         		<input type="radio" value="0" name="url_enable_x" onClick="enable_url();" <% nvram_match("url_enable_x", "0", "checked"); %>><#CTL_Disabled#>
					</td>
        </tr>	
			</table>

<table width="100%" style="display:none;" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
         <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,1);"><#FirewallConfig_UrlFilterEnable_itemname#> 1:</th>
          	<td>  
          		<input type="radio" value="1" name="url_enable_x_0" onClick="enable_url();" <% nvram_match("url_enable_x", "1", "checked"); %>><#CTL_Enabled#>
          		<input type="radio" value="0" name="url_enable_x_0" onClick="enable_url();" <% nvram_match("url_enable_x", "0", "checked"); %>><#CTL_Disabled#>
		</td>
        </tr>	

	<tr id="url_time" style="display:none;">
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,2);"><#FirewallConfig_URLActiveTime_itemname#> 1:</a></th>
	        <td>
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_starthour" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 0);">:
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_startmin" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 1);">-
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endhour" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 2);">:
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endmin" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 3);">
		</td>
        </tr>
        
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,1);"><#FirewallConfig_UrlFilterEnable_itemname#> 2:</th>
          	<td>  
          		<input type="radio" value="1" name="url_enable_x_1" onClick="enable_url_1();" <% nvram_match("url_enable_x_1", "1", "checked"); %>><#CTL_Enabled#>
          		<input type="radio" value="0" name="url_enable_x_1" onClick="enable_url_1();" <% nvram_match("url_enable_x_1", "0", "checked"); %>><#CTL_Disabled#>
		</td>
        </tr>      
        
	<tr id="url_time_1" style="display:none;">
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,2);"><#FirewallConfig_URLActiveTime_itemname#> 2:</a></th>
          	<td>
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_starthour_1" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 0);">:
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_startmin_1" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 1);">-
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endhour_1" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 2);">:
			<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endmin_1" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 3);">
		</td>
        </tr>
        
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,1);"><#FirewallConfig_URLActiveDate_itemname#></a></th>
			<td>
		  		<input type="checkbox" name="url_date_x_Sun" class="input"><#date_Sun_itemdesc#>
				<input type="checkbox" name="url_date_x_Mon" class="input"><#date_Mon_itemdesc#>
				<input type="checkbox" name="url_date_x_Tue" class="input"><#date_Tue_itemdesc#>
				<input type="checkbox" name="url_date_x_Wed" class="input"><#date_Wed_itemdesc#>
				<input type="checkbox" name="url_date_x_Thu" class="input"><#date_Thu_itemdesc#>
				<input type="checkbox" name="url_date_x_Fri" class="input"><#date_Fri_itemdesc#>
				<input type="checkbox" name="url_date_x_Sat" class="input"><#date_Sat_itemdesc#>
		  	</td>
        </tr>
        
        <!--tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(9,2);"><#FirewallConfig_URLActiveTime_itemname#></a></th>
          <td>
			<input type="text" maxlength="2" class="input" size="2" name="url_time_x_starthour" onKeyPress="return is_number(this)">:
			<input type="text" maxlength="2" class="input" size="2" name="url_time_x_startmin" onKeyPress="return is_number(this)">-
			<input type="text" maxlength="2" class="input" size="2" name="url_time_x_endhour" onKeyPress="return is_number(this)">:
			<input type="text" maxlength="2" class="input" size="2" name="url_time_x_endmin" onKeyPress="return is_number(this)">
		  </td>
        </tr-->
</table>

<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
					  <thead>
					  <tr>
						<td colspan="4"><#FirewallConfig_UrlList_groupitemdesc#>&nbsp;(<#List_limit#>&nbsp;128)</td>
					  </tr>
					  </thead>

  	<tr>
		<th width="80%"><#FirewallConfig_UrlList_groupitemdesc#></th>
		<th width="20%"><#list_add_delete#></th>
	</tr>
	<tr>
		<td width="80%">
	 		<input type="text" maxlength="32" class="input_32_table" name="url_keyword_x_0" onKeyPress="return is_string(this, event)">
	 	</td>
	 	<td width="20%">	
	  		<input class="add_btn" type="button" onClick="addRow(document.form.url_keyword_x_0, 128);" value="">
	 	</td>	
	</tr>
</table>
      	
      	<div id="url_rulelist_Block"></div>

		<div class="apply_gen">
			<input type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
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
