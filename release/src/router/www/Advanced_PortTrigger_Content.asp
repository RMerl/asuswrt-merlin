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

<title><#Web_Title#> - <#menu5_3_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/validator.js"></script>
<script>
var autofw_rulelist_array = '<% nvram_char_to_ascii("","autofw_rulelist"); %>';
var wans_mode ='<% nvram_get("wans_mode"); %>';

var backup_outport = "";
var backup_outproto = "";
var backup_inport = "";
var backup_inproto = "";
var backup_desc = "";

function initial(){
	show_menu();
	well_known_apps();
	showautofw_rulelist();
	addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "port", "trigger"]);
	//if(dualWAN_support && wans_mode == "lb")
	//	document.getElementById("lb_note").style.display = "";
}

function well_known_apps(){
	wItem = new Array(new Array("Quicktime 4 Client", "554", "TCP", "6970:32000", "UDP"),new Array("Real Audio", "7070", "TCP", "6970:7170", "UDP"));
	free_options(document.form.TriggerKnownApps);
	add_option(document.form.TriggerKnownApps, "<#Select_menu_default#>", "User Defined", 1);
	for (i = 0; i < wItem.length; i++){
		add_option(document.form.TriggerKnownApps, wItem[i][0], wItem[i][0], 0);
	}
}
function applyRule(){
	cancel_Edit();

	var rule_num = document.getElementById('autofw_rulelist_table').rows.length;
	var item_num = document.getElementById('autofw_rulelist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"		
		for(j=0; j<item_num-1; j++){	
			tmp_value += document.getElementById('autofw_rulelist_table').rows[i].cells[j].innerHTML;
			if(j != item_num-2)	
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	
	
	document.form.autofw_rulelist.value = tmp_value;

	showLoading();
	document.form.submit();
}

function done_validating(action){
	refreshpage();
}

function change_wizard(o){
	for(var i = 0; i < wItem.length; i++){
		if(wItem[i][0] != null){
			if(o.value == wItem[i][0]){
				document.form.autofw_outport_x_0.value = wItem[i][1];
				document.form.autofw_outproto_x_0.value = wItem[i][2];
				document.form.autofw_inport_x_0.value = wItem[i][3];
				document.form.autofw_inproto_x_0.value = wItem[i][4];
				document.form.autofw_desc_x_0.value = wItem[i][0];				
				break;
			}
		}
	}
}

//new table start 2 // jerry5 added.
function addRow(obj, head){
	if(head == 1)
		autofw_rulelist_array += "<"
	else
		autofw_rulelist_array += ">"
			
	autofw_rulelist_array += obj.value;
	obj.value = "";
}

function validForm(){

	if(!Block_chars(document.form.autofw_desc_x_0, ["<" ,">"])){
				return false;		
	}	

	if(document.form.autofw_outport_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.autofw_outport_x_0.focus();
		document.form.autofw_outport_x_0.select();
		return false;
	
	}
	if(document.form.autofw_inport_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.autofw_inport_x_0.focus();
		document.form.autofw_inport_x_0.select();
		return false;
	
	}
	
	if(!validator.numberRange(document.form.autofw_outport_x_0, 1, 65535)
		|| !validator.numberRange(document.form.autofw_inport_x_0, 1, 65535)){		
			return false;
	}
	
	return true;	
}

function addRow_Group(upper){
	if(validForm()){
		if('<% nvram_get("autofw_enable_x"); %>' != "1")
			document.form.autofw_enable_x[0].checked = true;
		
		var rule_num = document.getElementById('autofw_rulelist_table').rows.length;
		var item_num = document.getElementById('autofw_rulelist_table').rows[0].cells.length;		
		if(rule_num >= upper){
			alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
			return;
		}
			
		//Viz check same rule  //match(out port+out_proto+in port+in_proto) is not accepted
		if(item_num >=2){
			for(i=0; i<rule_num; i++){
					if(document.form.autofw_outport_x_0.value == document.getElementById('autofw_rulelist_table').rows[i].cells[1].innerHTML 
						&& document.form.autofw_outproto_x_0.value == document.getElementById('autofw_rulelist_table').rows[i].cells[2].innerHTML
						&& document.form.autofw_inport_x_0.value == document.getElementById('autofw_rulelist_table').rows[i].cells[3].innerHTML
						&& document.form.autofw_inproto_x_0.value == document.getElementById('autofw_rulelist_table').rows[i].cells[4].innerHTML){
						alert("<#JS_duplicate#>");						
						document.form.autofw_outport_x_0.focus();
						document.form.autofw_outport_x_0.select();
						return;
					}				
			}
		}		
		
		addRow(document.form.autofw_desc_x_0 ,1);
		addRow(document.form.autofw_outport_x_0, 0);
		addRow(document.form.autofw_outproto_x_0, 0);
		document.form.autofw_outproto_x_0.value="TCP";
		addRow(document.form.autofw_inport_x_0, 0);
		addRow(document.form.autofw_inproto_x_0, 0);
		document.form.autofw_inproto_x_0.value="TCP";

		showautofw_rulelist();

		if (backup_desc != "") {
			backup_outport = "";
			backup_outproto = "";
			backup_inport = "";
			backup_inproto = "";
			backup_desc = "";
			document.getElementById('autofw_rulelist_table').rows[rule_num-1].scrollIntoView();
		}
	}	
}

function edit_Row(r){ 	
	cancel_Edit();

	var i=r.parentNode.parentNode.rowIndex;
  	
	document.form.autofw_desc_x_0.value = document.getElementById('autofw_rulelist_table').rows[i].cells[0].innerHTML;
	document.form.autofw_outport_x_0.value = document.getElementById('autofw_rulelist_table').rows[i].cells[1].innerHTML; 
	document.form.autofw_outproto_x_0.value = document.getElementById('autofw_rulelist_table').rows[i].cells[2].innerHTML; 
	document.form.autofw_inport_x_0.value = document.getElementById('autofw_rulelist_table').rows[i].cells[3].innerHTML;
	document.form.autofw_inproto_x_0.value = document.getElementById('autofw_rulelist_table').rows[i].cells[4].innerHTML;

	backup_outport = document.form.autofw_outport_x_0.value;
	backup_outproto = document.form.autofw_outproto_x_0.value;
	backup_inport = document.form.autofw_inport_x_0.value;
	backup_inproto = document.form.autofw_inproto_x_0.value;
	backup_desc = document.form.autofw_desc_x_0.value;
	
	del_Row(r);
	document.form.autofw_desc_x_0.focus();
}

function cancel_Edit(){
	if (backup_desc != "") {
		document.form.autofw_outport_x_0.value = backup_outport;
		document.form.autofw_outproto_x_0.value = backup_outproto;
		document.form.autofw_inport_x_0.value = backup_inport;
		document.form.autofw_inproto_x_0.value = backup_inproto;
		document.form.autofw_desc_x_0.value = backup_desc;
		addRow_Group(128);
	}
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById('autofw_rulelist_table').deleteRow(i);
  
  var autofw_rulelist_value = "";
	for(k=0; k<document.getElementById('autofw_rulelist_table').rows.length; k++){
		for(j=0; j<document.getElementById('autofw_rulelist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				autofw_rulelist_value += "<";
			else
				autofw_rulelist_value += ">";
			autofw_rulelist_value += document.getElementById('autofw_rulelist_table').rows[k].cells[j].innerHTML;		
		}
	}
	
	autofw_rulelist_array = autofw_rulelist_value;
	if(autofw_rulelist_array == "")
		showautofw_rulelist();
}

function showautofw_rulelist(){
	var autofw_rulelist_row = decodeURIComponent(autofw_rulelist_array).split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="autofw_rulelist_table">';
	if(autofw_rulelist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < autofw_rulelist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var autofw_rulelist_col = autofw_rulelist_row[i].split('>');
			var wid=[22, 21, 10, 21, 10];
				for(var j = 0; j < autofw_rulelist_col.length; j++){
					code +='<td width="'+wid[j]+'%">'+ autofw_rulelist_col[j] +'</td>';		//IP  width="98"
				}
				code +='<td width="16%"><input class="edit_btn" onclick="edit_Row(this);" value=""/>';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
  code +='</table>';
	document.getElementById("autofw_rulelist_Block").innerHTML = code;
}

function changeBgColor(obj, num){
	if(obj.checked)
 		document.getElementById("row" + num).style.background='#FF9';
	else
 		document.getElementById("row" + num).style.background='#FFF';
}

function trigger_validate_duplicate_noalert(o, v, l, off){
	for (var i=0; i<o.length; i++)
	{
		if (entry_cmp(o[i][0].toLowerCase(), v.toLowerCase(), l)==0){
			return false;
		}
	}
	return true;
}

function trigger_validate_duplicate(o, v, l, off){
	for(var i = 0; i < o.length; i++)
	{
		if(entry_cmp(o[i][1].toLowerCase(), v.toLowerCase(), l) == 0){
			alert("<#JS_duplicate#>");
			return false;
		}
	}
	return true;
}
</script>
</head>

<body onload=" initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_PortTrigger_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="autofw_rulelist" value=''>
<input type="hidden" name="autofw_num_x_0" value="<% nvram_get("autofw_num_x"); %>" readonly="1" />

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
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		<td bgcolor="#4D595D" valign="top"  >
		<div>&nbsp;</div>
		<div class="formfonttitle"><#menu5_3#> - <#menu5_3_3#></div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<div class="formfontdesc"><#IPConnection_porttrigger_sectiondesc#></div>
		<div class="formfontdesc" style="margin-top:-10px;">
			<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;"><#menu5_3_3#>&nbspFAQ</a>
		</div>
		<div class="formfontdesc" id="lb_note" style="color:#FFCC00; display:none;"><#lb_note_portTrigger#></div>
	
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
					  <thead>
					  <tr>
						<td colspan="6"><#t2BC#></td>
					  </tr>
					  </thead>

          	<tr>
            	<th colspan="2"><#IPConnection_autofwEnable_itemname#></th>
            	<td colspan="4">
			  				<input type="radio" value="1" name="autofw_enable_x" class="content_input_fd" onClick="return change_common_radio(this, 'IPConnection', 'autofw_enable_x', '1')" <% nvram_match("autofw_enable_x", "1", "checked"); %>><#checkbox_Yes#>
			 			 		<input type="radio" value="0" name="autofw_enable_x" class="content_input_fd" onClick="return change_common_radio(this, 'IPConnection', 'autofw_enable_x', '0')" <% nvram_match("autofw_enable_x", "0", "checked"); %>><#checkbox_No#>
							</td>
		  			</tr>		  	
		  		
						<tr>
            	<th colspan="2"align="right" id="autofw_rulelist"><#IPConnection_TriggerList_widzarddesc#></th>
            	<td colspan="4">
            		<select name="TriggerKnownApps" class="input_option" onChange="change_wizard(this);">
            			<option value="User Defined"><#Select_menu_default#></option>
            		</select>
            	</td>
          	</tr>
          </table>
          
          <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0"  class="FormTable_table">
	  	  		<thead>
           		<tr>
            		<td colspan="6" id="autofw_rulelist"><#IPConnection_TriggerList_groupitemdesc#>&nbsp;(<#List_limit#>&nbsp;128)</td>
          		</tr>
		  			</thead>
		  
    	      <tr>
				<th><#IPConnection_autofwDesc_itemname#></th>
            	<th><#IPConnection_autofwOutPort_itemname#></th>
            	<th><#IPConnection_VServerProto_itemname#></th>
            	<th><#IPConnection_autofwInPort_itemname#></th>
            	<th><#IPConnection_VServerProto_itemname#></th>            
            	<th><#list_add_delete#></th>
  	        </tr>
          
	          <tr>
          		<td width="22%">
              		<input type="text" maxlength="18" class="input_15_table" name="autofw_desc_x_0" onKeyPress="return is_alphanum(this, event)" onblur="validator.safeName(this);" autocorrect="off" autocapitalize="off">
            	</td>
            	<td width="21%">            		
			<input type="text" maxlength="11" class="input_12_table"  name="autofw_outport_x_0" onKeyPress="return validator.isPortRange(this,event)" autocorrect="off" autocapitalize="off">
            	</td>
            	<td width="10%">
              		<select name="autofw_outproto_x_0" class="input_option">
              			<option value="TCP">TCP</option>
              			<option value="UDP">UDP</option>
              		</select>
              		</div>
            	</td>
            	<td width="21%">
			<input type="text" maxlength="11" class="input_12_table" name="autofw_inport_x_0" onKeyPress="return validator.isPortRange(this,event)" autocorrect="off" autocapitalize="off">
            	</td>
            	<td width="10%">
              		<select name="autofw_inproto_x_0" class="input_option">
              			<option value="TCP">TCP</option>
              			<option value="UDP">UDP</option>
              		</select>
            	</td>
								<td width="16%">
									<input type="button" class="add_btn" onClick="addRow_Group(128);" name="" value="">
								</td>
							</tr>
							</table>		
							
							<div id="autofw_rulelist_Block"></div>

			<div class="apply_gen">
				<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
			</div>
		
		
		</td>
	</tr>
</tbody>

</table>
</td>
</form>        </tr>
      </table>				
		<!--===================================Ending of Main Content===========================================-->		
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>

</body>
</html>
