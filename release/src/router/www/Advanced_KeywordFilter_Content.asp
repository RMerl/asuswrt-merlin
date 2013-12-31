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
<title><#Web_Title#> - <#menu5_5_5#></title>
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
var keyword_rulelist_array = "<% nvram_char_to_ascii("","keyword_rulelist"); %>";

function initial(){
	show_menu();
	show_keyword_rulelist();
}


function show_keyword_rulelist(){
	var keyword_rulelist_row = decodeURIComponent(keyword_rulelist_array).split('<');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="keyword_rulelist_table">'; 
	if(keyword_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i =1; i < keyword_rulelist_row.length; i++){
		code +='<tr id="row'+i+'">';
		code +='<td width="80%">'+ keyword_rulelist_row[i] +'</td>';		//Url keyword
		code +='<td width="20%">';
		code +="<input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow(this);\" value=\"\"/></td>";
		}
	}
  	code +='</tr></table>';
	
	$("keyword_rulelist_Block").innerHTML = code;
}

function deleteRow(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('keyword_rulelist_table').deleteRow(i);
  
  var keyword_rulelist_value = "";
	for(i=0; i<$('keyword_rulelist_table').rows.length; i++){
		keyword_rulelist_value += "<";
		keyword_rulelist_value += $('keyword_rulelist_table').rows[i].cells[0].innerHTML;
	}
	
	keyword_rulelist_array = keyword_rulelist_value;
	if(keyword_rulelist_array == "")
		show_keyword_rulelist();
}

function addRow(obj, upper){
	if(validForm(obj)){
		if('<% nvram_get("keyword_enable_x"); %>' != "1")
			document.form.keyword_enable_x[0].checked = true;
		
		var rule_num = $('keyword_rulelist_table').rows.length;
		var item_num = $('keyword_rulelist_table').rows[0].cells.length;
		
		// skip upper bound checking by jerry5
		/*if(rule_num >= upper){
				alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
				return false;	
		}*/

				//Viz check same rule
				for(i=0; i<rule_num; i++){
						for(j=0; j<item_num-1; j++){		//only 1 value column
								if(obj.value == $('keyword_rulelist_table').rows[i].cells[j].innerHTML){
										alert("<#JS_duplicate#>");
										return;
								}	
						}
				}	
	
				keyword_rulelist_array += "<";
				keyword_rulelist_array += obj.value;
				obj.value = "";		
				show_keyword_rulelist();		
		
	}		
}

function applyRule(){	
		var rule_num = $('keyword_rulelist_table').rows.length;
		var item_num = $('keyword_rulelist_table').rows[0].cells.length;
		var tmp_value = "";
	
		for(i=0; i<rule_num; i++){
			tmp_value += "<"		
			for(j=0; j<item_num-1; j++){	
				tmp_value += $('keyword_rulelist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)	
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";	
		document.form.keyword_rulelist.value = tmp_value;

		//updateDateTime(document.form.current_page.value);

		if(document.form.keyword_enable_x[0].checked == true && document.form.keyword_enable_x_orig.value != 1 ||
				document.form.keyword_enable_x[1].checked == true && document.form.keyword_enable_x_orig.value != 0)
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");

		showLoading();
		document.form.submit();
}

function validForm(obj){

		if(obj.value==""){
				alert("<#JS_fieldblank#>");
				obj.focus();
				obj.select();
				return false;
		
		}else if(!Block_chars(obj, ["#", "%" ,"&" ,"*" ,"{" ,"}" ,"\\" ,":" ,"<" ,">" ,"?" ,"/" ,"+"])){
				return false;		
		}
				
		return true;
}

function done_validating(action){
	refreshpage();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_KeywordFilter_Content.asp">
<input type="hidden" name="next_page" value="Advanced_KeywordFilter_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="keyword_rulelist" value="<% nvram_get("keyword_rulelist"); %>">
<input type="hidden" name="keyword_enable_x_orig" value="<% nvram_get("keyword_enable_x"); %>">
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
		  <div class="formfonttitle"><#menu5_5#> - <#menu5_5_5#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#FirewallConfig_KeywordFilterEnable_sectiondesc#></div>
		  <div class="formfontdesc"><#FirewallConfig_KeywordFilterEnable_sectiondesc2#></div>	

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					  <thead>
					  <tr>
						<td colspan="4"><#t2BC#></td>
					  </tr>
					  </thead>		
        <tr>
          <th><#FirewallConfig_KeywordFilterEnable_itemname#></th>
         	<td>  
         		<input type="radio" value="1" name="keyword_enable_x" onClick="" <% nvram_match("keyword_enable_x", "1", "checked"); %>><#CTL_Enabled#>
         		<input type="radio" value="0" name="keyword_enable_x" onClick="" <% nvram_match("keyword_enable_x", "0", "checked"); %>><#CTL_Disabled#>
					</td>
        </tr>	
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
					  <thead>
					  <tr>
						<td colspan="4"><#FirewallConfig_KeywordList_groupitemname#>&nbsp;(<#List_limit#>&nbsp;128)</td>
					  </tr>
					  </thead>

  	<tr>
		<th width="80%"><#FirewallConfig_KeywordList_groupitemname#></th>
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
      	
      	<div id="keyword_rulelist_Block"></div>

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
