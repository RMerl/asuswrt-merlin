<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - VLAN Group</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>

<script>
var $j = jQuery.noConflict();
var gvlan_rulelist_array = decodeURIComponent("<% nvram_char_to_ascii("","gvlan_rulelist"); %>");


function initial(){
	show_menu();
	showVLANRuleListGroup();
}

function applyRule(){
		showLoading();
		document.form.submit();
}

function initEditItem()
{
	for(var i=1; i<9; i++)
	{
		$("cbLAN" + i).checked = false;
		$("cbLAN" + i).disabled = false;
	}

	var iVLANGroupRowCount = $j("#tbVLANGroup tr").length;

	for(iVLANGroupRowCount; iVLANGroupRowCount>3; iVLANGroupRowCount--) //tbVLANGroup default row count is 3
	{
		$('tbVLANGroup').deleteRow(iVLANGroupRowCount-1);
	}
}
function switchSaveValueToDisplayValue(VLANGroupValue)
{
	var sSelectLANPort = "";
	for(var i=0; i<8; i++)
	{
		if((VLANGroupValue & Math.pow(2, i)) != 0) // bit computing ex. 8(1000) & 4(0010) = false
		{
			$("cbLAN" + (i+1)).disabled = true;
			$("cbLAN" + (i+1)).checked = false;
			if(sSelectLANPort != "")
				sSelectLANPort += ", "
			sSelectLANPort += "LAN" + (i+1);
		}
	}
	return sSelectLANPort;
}
function del_Row(r)
{
	var delIndex = r.parentNode.parentNode.rowIndex;
	var gvlan_rulelist_row = gvlan_rulelist_array.split('<');
	gvlan_rulelist_array = "";
	for(var i = 1; i < gvlan_rulelist_row.length; i++)
	{
		if( (delIndex) != (i+2)) //Because tbVLANGroup default have 3 rows, so gvlan_rulelist_row create in tbVLANGroup the row index start is 3.
		{
			gvlan_rulelist_array += "<";
			gvlan_rulelist_array += gvlan_rulelist_row[i];
		}
	}

	showVLANRuleListGroup();
}
function showVLANRuleListGroup()
{
	initEditItem();

	var gvlan_rulelist_row = gvlan_rulelist_array.split('<');

	var htmlcode = "";
	var table = document.getElementById("tbVLANGroup");
	if(gvlan_rulelist_row.length == 1)
	{		
		var row = table.insertRow(-1);
		row.id = "trVLANGroupEmpty";
		var row = document.getElementById("trVLANGroupEmpty");
		var td = row.insertCell(-1);
		td.style.color = "#FFCC00";
		td.colSpan = 3;
		htmlcode +='<#IPConnection_VSList_Norule#>';
		td.innerHTML = htmlcode;
		$("tdGroupNum").innerHTML = "Lan List";
	}
	else
	{
		$("tdGroupNum").innerHTML = "Lan List";
		for(var i = 1; i < gvlan_rulelist_row.length; i++)
		{
			var row = table.insertRow(-1);
			row.id = "trVLANGroup"+i;
			var row = document.getElementById("trVLANGroup"+i);

			var tdGroupName = row.insertCell(-1);

			tdGroupName.innerHTML = "GLAN" + i;

			htmlcode = switchSaveValueToDisplayValue(gvlan_rulelist_row[i]);
			var tdGroupLANList = row.insertCell(-1);
			tdGroupLANList.innerHTML = htmlcode;

			var tdDelete = row.insertCell(-1);
			htmlcode ='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
			tdDelete.innerHTML = htmlcode;
		}
	}
}
function validForm()
{

	if(!document.form.cbLAN1.checked && !document.form.cbLAN2.checked && !document.form.cbLAN3.checked && !document.form.cbLAN4.checked && 
		!document.form.cbLAN5.checked && !document.form.cbLAN6.checked && !document.form.cbLAN7.checked && !document.form.cbLAN8.checked)
	{
		alert("<#JS_fieldblank#>");
		return false;
	}
	return true;
}
function addRowGroup()
{
	if(document.form.cbLAN1.disabled && document.form.cbLAN2.disabled && document.form.cbLAN3.disabled && document.form.cbLAN4.disabled && 
		document.form.cbLAN5.disabled && document.form.cbLAN6.disabled && document.form.cbLAN7.disabled && document.form.cbLAN8.disabled)
		alert("All Port have been set");
	else{
			if(validForm())
			{
				var iLANSelect = 0;

				gvlan_rulelist_array += "<";
				if(document.form.cbLAN1.checked)
					iLANSelect = 1;
				if(document.form.cbLAN2.checked)
					iLANSelect += 2;
				if(document.form.cbLAN3.checked)
					iLANSelect += 4;
				if(document.form.cbLAN4.checked)
					iLANSelect += 8;
				if(document.form.cbLAN5.checked)
					iLANSelect += 16;
				if(document.form.cbLAN6.checked)
					iLANSelect += 32;
				if(document.form.cbLAN7.checked)
					iLANSelect += 64;
				if(document.form.cbLAN8.checked)
					iLANSelect += 128;

				gvlan_rulelist_array += iLANSelect;
		
				showVLANRuleListGroup();
			}
		}
}

function applyRule()
{
	document.form.gvlan_rulelist.value = gvlan_rulelist_array;
	showLoading();
	document.form.submit();
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_VLAN_Group_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="gvlan_rulelist" value="<% nvram_char_to_ascii("","gvlan_rulelist"); %>">
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
					<td align="left" valign="top">
						<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">VLAN Group</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">Integrate VLANs to be a group without reset and make members of combined LANs can communicate with each
other.</div>
									<table id="tbVLANGroup" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
										<thead>
										<tr>
											<td colspan="3">Configuration</td>
										</tr>
										</thead>
										<tr>
											<th>Group</th>
											<th>LAN</th>
											<th><#list_add_delete#></th>
										</tr>
										<tr>
										<td id="tdGroupNum" style="width:10%;">
										</td>
										<td style="width:80%;" nowrap="nowrap">
											<input type="checkbox" class="input" id="cbLAN1" name="cbLAN1">LAN1&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN2" name="cbLAN2">LAN2&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN3" name="cbLAN3">LAN3&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN4" name="cbLAN4">LAN4&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN5" name="cbLAN5">LAN5&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN6" name="cbLAN6">LAN6&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN7" name="cbLAN7">LAN7&nbsp;&nbsp;
											<input type="checkbox" class="input" id="cbLAN8" name="cbLAN8">LAN8
										</td>
										<td style="width:10%;">
											<input type="button" class="add_btn" onClick="addRowGroup();" name="" value="">
										</td>
										</tr>
									</table>
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
			<!--===================================End of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>	
<div id="footer"></div>
</body>
</html>

