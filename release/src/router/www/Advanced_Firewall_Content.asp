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
<title><#Web_Title#> - <#menu5_5_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script>
var filter_lwlist_array = '<% nvram_get("filter_lwlist"); %>';

function initial(){
	show_menu();
	showfilter_lwlist();
	init_setting();
	check_Timefield_checkbox();
	if(svc_ready == "0")
		document.getElementById('svc_hint_div').style.display = "";	
	corrected_timezone();	
}

function init_setting(){
	wItem = new Array(new Array("WWW", "80", "TCP"),new Array("TELNET", "23", "TCP"),new Array("FTP", "20:21", "TCP"));
	free_options(document.form.LWKnownApps);
	add_option(document.form.LWKnownApps, "<#IPConnection_TriggerList_userdefined#>", "User Defined", 1);
	for (i = 0; i < wItem.length; i++){
		add_option(document.form.LWKnownApps, wItem[i][0], wItem[i][0], 0);
	}
	
	document.form.filter_lw_date_x_Sun.checked = getDateCheck(document.form.filter_lw_date_x.value, 0);
	document.form.filter_lw_date_x_Mon.checked = getDateCheck(document.form.filter_lw_date_x.value, 1);
	document.form.filter_lw_date_x_Tue.checked = getDateCheck(document.form.filter_lw_date_x.value, 2);
	document.form.filter_lw_date_x_Wed.checked = getDateCheck(document.form.filter_lw_date_x.value, 3);
	document.form.filter_lw_date_x_Thu.checked = getDateCheck(document.form.filter_lw_date_x.value, 4);
	document.form.filter_lw_date_x_Fri.checked = getDateCheck(document.form.filter_lw_date_x.value, 5);
	document.form.filter_lw_date_x_Sat.checked = getDateCheck(document.form.filter_lw_date_x.value, 6);
	document.form.filter_lw_time_x_starthour.value = getTimeRange(document.form.filter_lw_time_x.value, 0);
	document.form.filter_lw_time_x_startmin.value = getTimeRange(document.form.filter_lw_time_x.value, 1);
	document.form.filter_lw_time_x_endhour.value = getTimeRange(document.form.filter_lw_time_x.value, 2);
	document.form.filter_lw_time_x_endmin.value = getTimeRange(document.form.filter_lw_time_x.value, 3);
	document.form.filter_lw_time2_x_starthour.value = getTimeRange(document.form.filter_lw_time2_x.value, 0);
	document.form.filter_lw_time2_x_startmin.value = getTimeRange(document.form.filter_lw_time2_x.value, 1);
	document.form.filter_lw_time2_x_endhour.value = getTimeRange(document.form.filter_lw_time2_x.value, 2);
	document.form.filter_lw_time2_x_endmin.value = getTimeRange(document.form.filter_lw_time2_x.value, 3);
}

function applyRule(){
	if(validForm()){
		var rule_num = document.getElementById('filter_lwlist_table').rows.length;
		var item_num = document.getElementById('filter_lwlist_table').rows[0].cells.length;
		var tmp_value = "";
	
		for(i=0; i<rule_num; i++){
			tmp_value += "<"		
			for(j=0; j<item_num-1; j++){	
				tmp_value += document.getElementById('filter_lwlist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)	
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";	
		
		document.form.filter_lwlist.value = tmp_value;
		updateDateTime();		

		showLoading();
		document.form.submit();
	}
}

function validForm(){
	if(!validator.timeRange(document.form.filter_lw_time_x_starthour, 0)||!validator.timeRange(document.form.filter_lw_time2_x_starthour, 0)
			|| !validator.timeRange(document.form.filter_lw_time_x_startmin, 1)||!validator.timeRange(document.form.filter_lw_time2_x_startmin, 1)
			|| !validator.timeRange(document.form.filter_lw_time_x_endhour, 2)||!validator.timeRange(document.form.filter_lw_time2_x_endhour, 2)
			|| !validator.timeRange(document.form.filter_lw_time_x_endmin, 3)||!validator.timeRange(document.form.filter_lw_time2_x_endmin, 3)
			)
		return false;
	
	if(!validator.portList(document.form.filter_lw_icmp_x, 'filter_lw_icmp_x'))
		return false;
		
	/*if(document.form.filter_lw_time_x_starthour.value > document.form.filter_lw_time_x_endhour.value){
			alert("<#FirewallConfig_URLActiveTime_itemhint#>");
					document.form.filter_lw_time_x_starthour.focus();
					document.form.filter_lw_time_x_starthour.select;
			return false;	
	}else if(document.form.filter_lw_time_x_starthour.value == document.form.filter_lw_time_x_endhour.value){
			if(document.form.filter_lw_time_x_startmin.value > document.form.filter_lw_time_x_endmin.value){
					alert("<#FirewallConfig_URLActiveTime_itemhint#>");
					document.form.filter_lw_time_x_startmin.focus();
					document.form.filter_lw_time_x_startmin.select;
					return false;	
			}else if(document.form.filter_lw_time_x_startmin.value == document.form.filter_lw_time_x_endmin.value){
					alert("<#FirewallConfig_URLActiveTime_itemhint2#>");
					document.form.filter_lw_time_x_startmin.focus();
					document.form.filter_lw_time_x_startmin.select;
					return false;					
			}
				
	}	*/

	if(!document.form.filter_lw_date_x_Sun.checked && !document.form.filter_lw_date_x_Mon.checked &&
		 !document.form.filter_lw_date_x_Tue.checked && !document.form.filter_lw_date_x_Wed.checked &&
		 !document.form.filter_lw_date_x_Thu.checked && !document.form.filter_lw_date_x_Fri.checked &&
		 !document.form.filter_lw_date_x_Sat.checked && !document.form.fw_lw_enable_x[1].checked){
		alert(Untranslated.filter_lw_date_valid);
		document.form.filter_lw_date_x_Sun.focus();
		return false;
	}
	
	return true;
}

function done_validating(action){
	refreshpage();
}

function change_wizard(o, id){
	for(var i = 0; i < wItem.length; i++){
		if(wItem[i][0] != null){
			if(o.value == wItem[i][0]){
				if(id == "WLKnownApps"){
					if(wItem[i][2] == "TCP")
						document.form.filter_lw_proto_x_0.options[0].selected = 1;
					else if(wItem[i][2] == "UDP")
						document.form.filter_lw_proto_x_0.options[8].selected = 1;
					
					document.form.filter_lw_srcport_x_0.value = wItem[i][1];
				}
				else{
					if(wItem[i][2] == "TCP")
						document.form.filter_lw_proto_x_0.options[0].selected = 1;
					else if(wItem[i][2] == "UDP")
						document.form.filter_lw_proto_x_0.options[8].selected = 1;
					
					document.form.filter_lw_dstport_x_0.value = wItem[i][1];
				}
			}
		}
	}
}

function addRow(obj, head){
	if(head == 1)
		filter_lwlist_array += "&#60"
	else
		filter_lwlist_array += "&#62"
			
	filter_lwlist_array += obj.value;
	obj.value = "";
}

function addRow_Group(upper){ 
	if('<% nvram_get("fw_lw_enable_x"); %>' != "1")
		document.form.fw_lw_enable_x[0].checked = true;
		var rule_num = document.getElementById('filter_lwlist_table').rows.length;
		var item_num = document.getElementById('filter_lwlist_table').rows[0].cells.length;
	
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}			
		
	if(document.form.filter_lw_srcip_x_0.value=="" && document.form.filter_lw_srcport_x_0.value=="" &&
			document.form.filter_lw_dstip_x_0.value=="" && document.form.filter_lw_dstport_x_0.value==""){
					alert("<#JS_fieldblank#>");
					document.form.filter_lw_srcip_x_0.focus();
					document.form.filter_lw_srcip_x_0.select();
					return false;
	}else{		
					if(!validator.ipv4cidr(document.form.filter_lw_srcip_x_0))
                                                return false;
					if(!validator.ipv4cidr(document.form.filter_lw_dstip_x_0))
						return false;
	}
		
	if(document.form.filter_lw_srcport_x_0.value != "" || document.form.filter_lw_dstport_x_0.value != "")
			replace_symbol();
	

	if(document.form.filter_lw_srcport_x_0.value == "" && document.form.filter_lw_dstport_x_0.value == ""){
		
	}else	if(document.form.filter_lw_srcport_x_0.value==""){
					if(!validator.numberRange(document.form.filter_lw_dstport_x_0, 1, 65535) )
							return false;					
					
	}else if(document.form.filter_lw_dstport_x_0.value==""){	
					if(!validator.numberRange(document.form.filter_lw_srcport_x_0, 1, 65535))
							return false;

	}else{
					if(!validator.numberRange(document.form.filter_lw_srcport_x_0, 1, 65535)							
							|| !validator.numberRange(document.form.filter_lw_dstport_x_0, 1, 65535) )
								return false;					
	}
	
	if(check_duplicate() == true)
			return false;		
			
	Do_addRow_Group();		
}	

//Viz add 2011.11 for replace ">" to ":65535"   &   "<" to "1:"  {
function replace_symbol(){	
					var largre_src=new RegExp("^(>)([0-9]{1,5})$", "gi");
					if(largre_src.test(document.form.filter_lw_srcport_x_0.value)){
						document.form.filter_lw_srcport_x_0.value = document.form.filter_lw_srcport_x_0.value.replace(/[>]/gi,"");	// ">" to ""
						document.form.filter_lw_srcport_x_0.value = document.form.filter_lw_srcport_x_0.value+":65535"; 						// add ":65535"
					}
					var smalre_src=new RegExp("^(<)([0-9]{1,5})$", "gi");
					if(smalre_src.test(document.form.filter_lw_srcport_x_0.value)){
						document.form.filter_lw_srcport_x_0.value = document.form.filter_lw_srcport_x_0.value.replace(/[<]/gi,"");	// "<" to ""
						document.form.filter_lw_srcport_x_0.value = "1:"+document.form.filter_lw_srcport_x_0.value; 						// add "1:"						
					}	
					var largre_dst=new RegExp("^(>)([0-9]{1,5})$", "gi");
					if(largre_dst.test(document.form.filter_lw_dstport_x_0.value)){
						document.form.filter_lw_dstport_x_0.value = document.form.filter_lw_dstport_x_0.value.replace(/[>]/gi,"");	// ">" to ""
						document.form.filter_lw_dstport_x_0.value = document.form.filter_lw_dstport_x_0.value+":65535"; 						// add ":65535"
					}
					var smalre_dst=new RegExp("^(<)([0-9]{1,5})$", "gi");
					if(smalre_dst.test(document.form.filter_lw_dstport_x_0.value)){
						document.form.filter_lw_dstport_x_0.value = document.form.filter_lw_dstport_x_0.value.replace(/[<]/gi,"");	// "<" to ""
						document.form.filter_lw_dstport_x_0.value = "1:"+document.form.filter_lw_dstport_x_0.value; 						// add "1:"						
					}
}
//} Viz add 2011.11 for replace ">" to ":65535"   &   "<" to "1:"  	

function check_duplicate(){
		var rule_num = document.getElementById('filter_lwlist_table').rows.length;
		var item_num = document.getElementById('filter_lwlist_table').rows[0].cells.length;	
//Viz check same rule  //match(src ip + src port + dst ip + dst port + proto) is not accepted
		for(i=0; i<rule_num; i++){
			if(entry_cmp(document.getElementById('filter_lwlist_table').rows[i].cells[0].innerHTML, document.form.filter_lw_srcip_x_0.value, 15)==0 
				&& entry_cmp(document.getElementById('filter_lwlist_table').rows[i].cells[2].innerHTML, document.form.filter_lw_dstip_x_0.value, 15)==0 
				&& entry_cmp(document.getElementById('filter_lwlist_table').rows[i].cells[4].innerHTML.toLowerCase(), document.form.filter_lw_proto_x_0.value.toLowerCase(), 8)==0 ){

						if(portrange_min(document.form.filter_lw_srcport_x_0.value, 11) > portrange_max(document.getElementById('filter_lwlist_table').rows[i].cells[1].innerHTML, 11) 
							|| portrange_max(document.form.filter_lw_srcport_x_0.value, 11) < portrange_min(document.getElementById('filter_lwlist_table').rows[i].cells[1].innerHTML, 11)
							|| (document.form.filter_lw_srcport_x_0.value=="" && document.getElementById('filter_lwlist_table').rows[i].cells[1].innerHTML !="")
							|| (document.form.filter_lw_srcport_x_0.value!="" && document.getElementById('filter_lwlist_table').rows[i].cells[1].innerHTML =="") ){
											return false;
						}else{

									if(portrange_min(document.form.filter_lw_dstport_x_0.value, 11) > portrange_max(document.getElementById('filter_lwlist_table').rows[i].cells[3].innerHTML, 11) 
											|| portrange_max(document.form.filter_lw_dstport_x_0.value, 11) < portrange_min(document.getElementById('filter_lwlist_table').rows[i].cells[3].innerHTML, 11)
											|| (document.form.filter_lw_dstport_x_0.value=="" && document.getElementById('filter_lwlist_table').rows[i].cells[3].innerHTML !="")
											|| (document.form.filter_lw_dstport_x_0.value!="" && document.getElementById('filter_lwlist_table').rows[i].cells[3].innerHTML =="") ){											
													return false;
									}else{
											alert("<#JS_duplicate#>");
											return true;
								}						
						}							
			}
		}
}

function Do_addRow_Group(){		
		addRow(document.form.filter_lw_srcip_x_0 ,1);
		addRow(document.form.filter_lw_srcport_x_0, 0);
		addRow(document.form.filter_lw_dstip_x_0, 0);
		addRow(document.form.filter_lw_dstport_x_0, 0);
		addRow(document.form.filter_lw_proto_x_0, 0);
		document.form.filter_lw_proto_x_0.value="TCP";
		showfilter_lwlist();
}

function edit_Row(r){ 	
	var i=r.parentNode.parentNode.rowIndex;
  	
	document.form.filter_lw_srcip_x_0.value = document.getElementById('filter_lwlist_table').rows[i].cells[0].innerHTML;
	document.form.filter_lw_srcport_x_0.value = document.getElementById('filter_lwlist_table').rows[i].cells[1].innerHTML; 
	document.form.filter_lw_dstip_x_0.value = document.getElementById('filter_lwlist_table').rows[i].cells[2].innerHTML; 
	document.form.filter_lw_dstport_x_0.value = document.getElementById('filter_lwlist_table').rows[i].cells[3].innerHTML;
	document.form.filter_lw_proto_x_0.value = document.getElementById('filter_lwlist_table').rows[i].cells[4].innerHTML;

  del_Row(r);	
}

function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  document.getElementById('filter_lwlist_table').deleteRow(i);
  
  var filter_lwlist_value = "";
	for(k=0; k<document.getElementById('filter_lwlist_table').rows.length; k++){
		for(j=0; j<document.getElementById('filter_lwlist_table').rows[k].cells.length-1; j++){
			if(j == 0)	
				filter_lwlist_value += "&#60";
			else
				filter_lwlist_value += "&#62";
			filter_lwlist_value += document.getElementById('filter_lwlist_table').rows[k].cells[j].innerHTML;		
		}
	}
	
	filter_lwlist_array = filter_lwlist_value;
	if(filter_lwlist_array == "")
		showfilter_lwlist();
}

function showfilter_lwlist(){
	var filter_lwlist_row = filter_lwlist_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="filter_lwlist_table">';
	if(filter_lwlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 1; i < filter_lwlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var filter_lwlist_col = filter_lwlist_row[i].split('&#62');
			var wid=[20, 15, 20, 15, 15];
				for(var j = 0; j < filter_lwlist_col.length; j++){
					code +='<td width="'+wid[j]+'%">'+ filter_lwlist_col[j] +'</td>';		//IP  width="98"
				}
				code +='<td width="15%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td>';
		}
	}
  code +='</table>';
	document.getElementById("filter_lwlist_Block").innerHTML = code;
}
function check_Timefield_checkbox(){	// To check Date checkbox checked or not and control Time field disabled or not, Jieming add at 2012/10/05
	if(	   document.form.filter_lw_date_x_Mon.checked == true 
		|| document.form.filter_lw_date_x_Tue.checked == true
		|| document.form.filter_lw_date_x_Wed.checked == true
		|| document.form.filter_lw_date_x_Thu.checked == true
		|| document.form.filter_lw_date_x_Fri.checked == true	){		
			inputCtrl(document.form.filter_lw_time_x_starthour,1);
			inputCtrl(document.form.filter_lw_time_x_startmin,1);
			inputCtrl(document.form.filter_lw_time_x_endhour,1);
			inputCtrl(document.form.filter_lw_time_x_endmin,1);
			document.form.filter_lw_time_x.disabled = false;
	}
	else{
			inputCtrl(document.form.filter_lw_time_x_starthour,0);
			inputCtrl(document.form.filter_lw_time_x_startmin,0);
			inputCtrl(document.form.filter_lw_time_x_endhour,0);
			inputCtrl(document.form.filter_lw_time_x_endmin,0);
			document.form.filter_lw_time_x.disabled = true;
			document.getElementById('enable_time_week_tr').style.display ="";
	}
		
	if(document.form.filter_lw_date_x_Sun.checked == true || document.form.filter_lw_date_x_Sat.checked == true){
		inputCtrl(document.form.filter_lw_time2_x_starthour,1);
		inputCtrl(document.form.filter_lw_time2_x_startmin,1);
		inputCtrl(document.form.filter_lw_time2_x_endhour,1);
		inputCtrl(document.form.filter_lw_time2_x_endmin,1);
		document.form.filter_lw_time2_x.disabled = false;
	}
	else{
		inputCtrl(document.form.filter_lw_time2_x_starthour,0);
		inputCtrl(document.form.filter_lw_time2_x_startmin,0);
		inputCtrl(document.form.filter_lw_time2_x_endhour,0);
		inputCtrl(document.form.filter_lw_time2_x_endmin,0);
		document.form.filter_lw_time2_x.disabled = true;
		document.getElementById("enable_time_weekend_tr").style.display = ""; 
	}
}
function updateDateTime(){
	document.form.filter_lw_date_x.value = setDateCheck(
		document.form.filter_lw_date_x_Sun,
		document.form.filter_lw_date_x_Mon,
		document.form.filter_lw_date_x_Tue,
		document.form.filter_lw_date_x_Wed,
		document.form.filter_lw_date_x_Thu,
		document.form.filter_lw_date_x_Fri,
		document.form.filter_lw_date_x_Sat);
	document.form.filter_lw_time_x.value = setTimeRange(
		document.form.filter_lw_time_x_starthour,
		document.form.filter_lw_time_x_startmin,
		document.form.filter_lw_time_x_endhour,
		document.form.filter_lw_time_x_endmin);
	document.form.filter_lw_time2_x.value = setTimeRange(
		document.form.filter_lw_time2_x_starthour,
		document.form.filter_lw_time2_x_startmin,
		document.form.filter_lw_time2_x_endhour,
		document.form.filter_lw_time2_x_endmin);	
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Firewall_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="filter_lwlist">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="filter_lw_date_x" value="<% nvram_get("filter_lw_date_x"); %>">
<input type="hidden" name="filter_lw_time_x" value="<% nvram_get("filter_lw_time_x"); %>">
<input type="hidden" name="filter_lw_time2_x" value="<% nvram_get("filter_lw_time2_x"); %>">
<input type="hidden" name="filter_lw_num_x_0" value="<% nvram_get("filter_lw_num_x"); %>" readonly="1">
<input type="hidden" name="filter_lwlist" value=''>
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
			<td valign="top">
				<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
				<tbody>
				<tr>
					<td bgcolor="#4D595D" valign="top">
		  				<div>&nbsp;</div>
		  				<div class="formfonttitle"><#menu5_5#> - <#menu5_5_4#></div>
		  				<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  				<div class="formfontdesc"><#FirewallConfig_display1_sectiondesc#></div>
						<div class="formfontdesc">The IP address can be a simple IP (1.2.3.4), or use the CIDR format (1.2.3.4/24) to handle a whole subnet.</div>
		  				<div class="formfontdesc"><#FirewallConfig_display3_sectiondesc#></div>
		  				<div class="formfontdesc" style="color:#FFCC00;"><#FirewallConfig_display4_sectiondesc#></div>	
		  				<div id="svc_hint_div" style="display:none;"><span onClick="location.href='Advanced_System_Content.asp?af=ntp_server0'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"><#General_x_SystemTime_syncNTP#></span></div>
		  				<div id="timezone_hint_div" style="display:none;"><span id="timezone_hint" onclick="location.href='Advanced_System_Content.asp?af=time_zone_select'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"></span></div>

						<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						<thead>
          					<tr>
            					<td colspan="6" id="filter_lwlist"><#menu5_5_4#></td>
          					</tr>
        					</thead>

       					<tr>
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,5);"><#FirewallConfig_LanWanFirewallEnable_itemname#></a></th>
          						<td>
								<input type="radio" value="1" name="fw_lw_enable_x" onClick="return change_common_radio(this, 'FirewallConfig', 'fw_lw_enable_x', '1')" <% nvram_match_x("FirewallConfig","fw_lw_enable_x", "1", "checked"); %>><#checkbox_Yes#>
								<input type="radio" value="0" name="fw_lw_enable_x" onClick="return change_common_radio(this, 'FirewallConfig', 'fw_lw_enable_x', '0')" <% nvram_match_x("FirewallConfig","fw_lw_enable_x", "0", "checked"); %>><#checkbox_No#>
	  						</td>
        					</tr>
			
        					<tr>
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,3);"><#FirewallConfig_LanWanDefaultAct_itemname#></a></th>
          						<td>
          							<select name="filter_lw_default_x" class="input_option">
          								<option value="DROP" <% nvram_match("filter_lw_default_x", "DROP","selected"); %>><#WhiteList#></option>
          								<option value="ACCEPT" <% nvram_match("filter_lw_default_x", "ACCEPT","selected"); %>><#BlackList#></option>
          							</select>
	  						</td>
        					</tr>

          					<tr>
          						<th><#IPConnection_TriggerList_widzarddesc#></th>
            					<td>
            						<select name="LWKnownApps" class="input_option" onChange="change_wizard(this, 'LWKnownApps');">
              							<option value="User Defined"><#IPConnection_TriggerList_userdefined#></option>
            						</select>
            					</td>
          					</tr>

          					<tr>
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,1);"><#FirewallConfig_LanWanActiveDate_itemname#></a></th>
          						<td>
								<input type="checkbox" name="filter_lw_date_x_Mon" class="input" onclick="check_Timefield_checkbox()"><#date_Mon_itemdesc#>
								<input type="checkbox" name="filter_lw_date_x_Tue" class="input" onclick="check_Timefield_checkbox()"><#date_Tue_itemdesc#>
								<input type="checkbox" name="filter_lw_date_x_Wed" class="input" onclick="check_Timefield_checkbox()"><#date_Wed_itemdesc#>
								<input type="checkbox" name="filter_lw_date_x_Thu" class="input" onclick="check_Timefield_checkbox()"><#date_Thu_itemdesc#>
								<input type="checkbox" name="filter_lw_date_x_Fri" class="input" onclick="check_Timefield_checkbox()"><#date_Fri_itemdesc#>
		  					</td>
        					</tr>
        					<tr id="enable_time_week_tr">
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,2);"><#FirewallConfig_LanWanActiveTime_itemname#></a></th>
          						<td>
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time_x_starthour" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 0);" autocorrect="off" autocapitalize="off"> :
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time_x_startmin" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 1);" autocorrect="off" autocapitalize="off"> -
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time_x_endhour" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 2);" autocorrect="off" autocapitalize="off"> :
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time_x_endmin" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 3);" autocorrect="off" autocapitalize="off">
		  					</td>
        					</tr>
							<tr>
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,1);"><#FirewallConfig_LanWanActiveDate_itemname#></a></th>
          						<td>
								<input type="checkbox" name="filter_lw_date_x_Sat" class="input" onclick="check_Timefield_checkbox()"><#date_Sat_itemdesc#>
								<input type="checkbox" name="filter_lw_date_x_Sun" class="input" onclick="check_Timefield_checkbox()"><#date_Sun_itemdesc#>														
		  					</td>
        					</tr>
							<tr id="enable_time_weekend_tr">
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,2);"><#FirewallConfig_LanWanActiveTime_itemname#></a></th>
          						<td>
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time2_x_starthour" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 0);" autocorrect="off" autocapitalize="off"> :
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time2_x_startmin" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 1);" autocorrect="off" autocapitalize="off"> -
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time2_x_endhour" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 2);" autocorrect="off" autocapitalize="off"> :
								<input type="text" maxlength="2" class="input_3_table" name="filter_lw_time2_x_endmin" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 3);" autocorrect="off" autocapitalize="off">		  
		  					</td>
        					</tr>
        					<tr>
          						<th ><a class="hintstyle" href="javascript:void(0);" onClick="openHint(10,4);"><#FirewallConfig_LanWanICMP_itemname#></a></th>
          						<td>
          							<input type="text" maxlength="32" class="input_32_table" name="filter_lw_icmp_x" value="<% nvram_get("filter_lw_icmp_x"); %>" onKeyPress="return validator.isPortlist(this,event)" autocorrect="off" autocapitalize="off">
          						</td>
        					</tr>

						</table>
						
   					<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="filter_lwlist"><#FirewallConfig_LWFilterList_groupitemdesc#>&nbsp;(<#List_limit#>&nbsp;128)</td>
	   					</tr>
	  					</thead>		  
          					<tr>
          						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,3);"><#FirewallConfig_LanWanSrcIP_itemname#></a></th>
            					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,2);"><#FirewallConfig_LanWanSrcPort_itemname#></a></th>
            					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,3);"><#FirewallConfig_LanWanDstIP_itemname#></a></th>
            					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,2);"><#FirewallConfig_LanWanSrcPort_itemname#></a></th>
            					<th><#IPConnection_VServerProto_itemname#></th>
            					<th><#list_add_delete#></th>
          					</tr>
          					<tr>
          						<td width="20%"><input type="text" maxlength="18" class="input_15_table" name="filter_lw_srcip_x_0" autocorrect="off" autocapitalize="off"></td>
            					<td width="15%"><input type="text" maxlength="11" class="input_12_table" name="filter_lw_srcport_x_0" onKeyPress="return validator.isPortRange(this,event)" value="" autocorrect="off" autocapitalize="off"></td>
            					<td width="20%"><input type="text" maxlength="18" class="input_15_table" name="filter_lw_dstip_x_0" autocorrect="off" autocapitalize="off"></td>
            					<td width="15%"><input type="text" maxlength="11" class="input_12_table" name="filter_lw_dstport_x_0" onKeyPress="return validator.isPortRange(this,event)" value="" autocorrect="off" autocapitalize="off"></td>
            					<td width="15%">
								<select name="filter_lw_proto_x_0" class="input_option">
									<option value="TCP">TCP</option>
									<option value="TCP ALL">TCP ALL</option>
									<option value="TCP SYN">TCP SYN</option>
									<option value="TCP ACK">TCP ACK</option>
									<option value="TCP FIN">TCP FIN</option>
									<option value="TCP RST">TCP RST</option>
									<option value="TCP URG">TCP URG</option>
									<option value="TCP PSH">TCP PSH</option>
									<option value="UDP">UDP</option>
								</select>
	    						</td>
	    							<td width="15%">
									<input type="button" class="add_btn" onClick="addRow_Group(128);" name="filter_lwlist2" value="">
								</td>
									
							</tr>
							</table>		
						
														<div id="filter_lwlist_Block"></div>	
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
