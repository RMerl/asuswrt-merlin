<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - VLAN</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<style>
.tdVLANRuleList_Block{
	word-break:break-all;
}
.ClientList_Block{
	*margin-left:-123px !important
}
</style>
<script>
var $j = jQuery.noConflict();
var vlan_rulelist_array = decodeURIComponent("<% nvram_char_to_ascii("","vlan_rulelist"); %>");

function initial() {
	show_menu();

	setWirelessArray();

	showWireless();

	showVLANRuleList();
}

function applyRule() {
		showLoading();
		document.form.submit();
}

var arrayWirelessName = new Array();
var arrayWirelessValue = new Array();
var iSelectWireless1 = 0;
var iSelectWireless2 = 0;

function setWirelessArray () 
{
	var ssid_status_2g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl0_ssid"); %>');
	arrayWirelessName.push(ssid_status_2g);
	arrayWirelessValue.push(1); // 2.4G SSID1 value is 1

	for(var guest2g = 0; guest2g < gn_array_2g.length; guest2g++)
	{
		var guest2gValue = Math.pow(2, guest2g + 1); // 2.4G SSID2~SSID4 value is 2, 4, 8
		arrayWirelessName.push(gn_array_2g[guest2g][1]);
		arrayWirelessValue.push(guest2gValue);
	}

	if(band5g_support)
	{
		var ssid_status_5g =  decodeURIComponent('<% nvram_char_to_ascii("WLANConfig11b", "wl1_ssid"); %>');
		arrayWirelessName.push(ssid_status_5g);
		arrayWirelessValue.push(16); // 5G SSID1 value is 16
		for(var guest5g = 0; guest5g < gn_array_5g.length; guest5g++)
		{
			var guest5gValue = Math.pow(2, guest5g + 5); // 5G SSID2~SSID4 value is 32, 64, 128
			arrayWirelessName.push(gn_array_5g[guest5g][1]);
			arrayWirelessValue.push(guest5gValue);
		}
	}
}

function showWireless() 
{
	for(var wirelessNum=1; wirelessNum<3; wirelessNum++)
	{
		var htmlCode = "";
		htmlCode += '<a id="aWireless_'+wirelessNum+'_0"><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setWireless(\'\', 0, \''+wirelessNum+'\');"><strong></strong></div></a>';

		for(var i=0; i<arrayWirelessName.length; i++)
		{
			htmlCode += '<a id="aWireless_'+wirelessNum+'_'+arrayWirelessValue[i]+'"><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setWireless(\''+arrayWirelessName[i]+'\', \''+arrayWirelessValue[i]+'\',\''+wirelessNum+'\');"><strong>'+arrayWirelessName[i]+'</strong></div></a>';
		}

		$("divWirelessBlock"+wirelessNum+"").innerHTML += htmlCode;
	}
}
function setWireless(Wireless, WirelessValue, WirelessNum)
{
 	$('tWireless' + WirelessNum).value = Wireless;
 	if(WirelessValue!=0)
		$('tWireless' + WirelessNum).readOnly = true;
	else
		$('tWireless' + WirelessNum).readOnly = false;

	if(WirelessNum == "1")
	{
		iSelectWireless1 = 0;
		iSelectWireless1 = parseInt(WirelessValue);
	}
	else if(WirelessNum == "2")
	{
		iSelectWireless2 = 0;
		iSelectWireless2 = parseInt(WirelessValue);
	}

	over_var = 0;
	hideWirelessBlock(WirelessNum);
}
var isMenuopen1 = 0;
var isMenuopen2 = 0;
var over_var = 0;
function pullWirelessList(obj, WirelessNum)
{
	switch(WirelessNum) 
	{
		case 1:
			if(isMenuopen1 == 0)
			{		
				obj.src = "/images/arrow-top.gif"
				$("divWirelessBlock1").style.display = 'block';		
				document.form.tWireless1.focus();		
				isMenuopen1 = 1;
			}
			else
				hideWirelessBlock(WirelessNum);
			break;
		case 2:
			if(isMenuopen2 == 0)
			{		
				obj.src = "/images/arrow-top.gif"
				$("divWirelessBlock2").style.display = 'block';		
				document.form.tWireless2.focus();		
				isMenuopen2 = 1;
			}
			else
				hideWirelessBlock(WirelessNum);
			break;
	}
}

function hideWirelessBlock(WirelessNum)
{
	$("imgPullArrow" + WirelessNum).src = "/images/arrow-down.gif";
	$('divWirelessBlock' + WirelessNum).style.display='none';
	switch(parseInt(WirelessNum))
	{
		case 1:
			isMenuopen1 = 0;
			break;
		case 2:
			isMenuopen2 = 0;
			break;
	}
}

function validForm()
{
	if(!Block_chars(document.form.tDescription, ["<" ,">"])){
		return false;	
	}
	else if(document.form.tDescription.value=="")
	{
		alert("<#JS_fieldblank#>");
		document.form.tDescription.focus();
		document.form.tDescription.select();
		return false;
	}

	if(!document.form.cbLANPort1.checked && !document.form.cbLANPort2.checked && !document.form.cbLANPort3.checked && 
		!document.form.cbLANPort4.checked && document.form.tWireless1.value=="" && document.form.tWireless2.value=="")
	{
		alert("Please at least setting a LAN PORT or a Wireless");
		document.form.tWireless1.focus();
		document.form.tWireless1.select();
		return false;
	}
	else
	{
		if((document.form.tWireless1.value!="" && document.form.tWireless2.value!="") && document.form.tWireless1.value == document.form.tWireless2.value)
		{
			alert("Can't setting the same SSID");
			document.form.tWireless2.focus();
			document.form.tWireless2.select();
			return false;
		}
	}

	return true;	
}

function addRowGroup()
{
	var iWirelessCount = ($j("#divWirelessBlock1 a").length)-1; //because first a is none
	var bAllWirelessSelected = true;
	for(var i=0; i<iWirelessCount; i++)
	{
		if($("aWireless_1_"+ Math.pow(2, i)).style.display != "none")
		{
			bAllWirelessSelected = false;
		}
	}

	if((document.form.cbLANPort1.disabled && document.form.cbLANPort2.disabled && document.form.cbLANPort3.disabled && document.form.cbLANPort4.disabled 
		&& bAllWirelessSelected) || document.form.selSubnet.length == 0)
	{
		alert("All Ports, Wirelss and Subnet have been setted");
	}
	else
	{
		if(validForm())
		{	
			var iLANPortSelect = 0;
			var tepVlan_rulelist_array = "";

			tepVlan_rulelist_array += "<";
			tepVlan_rulelist_array += Number(document.form.cbEnable.checked);
			tepVlan_rulelist_array += ">";
			tepVlan_rulelist_array += document.form.tDescription.value;
			tepVlan_rulelist_array += ">";
			if(document.form.cbLANPort1.checked)
				iLANPortSelect = 1;
			if(document.form.cbLANPort2.checked)
				iLANPortSelect += 2;
			if(document.form.cbLANPort3.checked)
				iLANPortSelect += 4;
			if(document.form.cbLANPort4.checked)
				iLANPortSelect += 8;

			tepVlan_rulelist_array += iLANPortSelect;
			tepVlan_rulelist_array += ">";
			tepVlan_rulelist_array += parseInt(iSelectWireless1) + parseInt(iSelectWireless2);

			tepVlan_rulelist_array += ">";
			tepVlan_rulelist_array += document.form.selSubnet.value;
			tepVlan_rulelist_array += ">";
			tepVlan_rulelist_array += Number(document.form.cbIntranetOnly.checked);
	
			if(vlan_rulelist_array.search(tepVlan_rulelist_array) == -1) //avoid click add button no response, but vlan_rulelist_array have been added 
				vlan_rulelist_array += tepVlan_rulelist_array;

			showVLANRuleList();
		}	
	}
}

function initEditItem()
{
	document.form.cbEnable.checked = true;
	document.form.tDescription.value = "";
	for(var i=1; i<5; i++)
	{
		$("cbLANPort" + i).checked = false;
		$("cbLANPort" + i).disabled = false;
	}

	$('tWireless1').value = "";
	$('tWireless1').readOnly = false;
	iSelectWireless1 = 0;
	$('tWireless2').value = "";
	$('tWireless2').readOnly = false;
	iSelectWireless2 = 0;

	for(var wirelessNum=1; wirelessNum<3; wirelessNum++)
	{
		for(var i=0; i<arrayWirelessValue.length; i++)
		{
			$("aWireless_" + wirelessNum + "_" + Math.pow(2, i)).style.display = "";
		}
	}

	document.form.selSubnet.length = 0;
	for(var j=1; j<9; j++)
	{
		var new_option = new Option("LAN"+j,"LAN"+j);
		document.form.selSubnet.options.add(new_option);
	}

	document.form.cbIntranetOnly.checked = false;

	var iVLANRuleListRowCount = $j("#tbVLANRuleList tr").length;
	for(iVLANRuleListRowCount; iVLANRuleListRowCount>3; iVLANRuleListRowCount--) //tbVLANGroup default row count is 3
	{
		$('tbVLANRuleList').deleteRow(iVLANRuleListRowCount-1);
	}
}

function editVLANRuleList(VLANRuleListIndex, VLANRuleValueIndex)
{
	var vlan_rulelist_row = vlan_rulelist_array.split('<');

	vlan_rulelist_array = "";
	for(var i = 1; i < vlan_rulelist_row.length; i++)
	{
		if( VLANRuleListIndex == i) 
		{
			var vlan_rulelist_col = vlan_rulelist_row[i].split('>');
			for(var j = 0; j < vlan_rulelist_col.length; j++)
			{
				switch(j)
				{
					case 0:
						vlan_rulelist_array += "<";
						vlan_rulelist_array += Number($("cbEnable"+VLANRuleListIndex).checked);
						break;
					case 5:
						vlan_rulelist_array += ">";
						vlan_rulelist_array += Number($("cbIntranetOnly"+VLANRuleListIndex).checked);
						break;
					default:
						vlan_rulelist_array += ">";
						vlan_rulelist_array += vlan_rulelist_col[j];
						break;
				}
			}
		}
		else
		{
			vlan_rulelist_array += "<";
			vlan_rulelist_array += vlan_rulelist_row[i];
		}
	}
}

function switchSaveValueToDisplayValue(VLANRuleValue, VLANRuleValueIndex, VLANRuleListIndex)
{
	switch(parseInt(VLANRuleValueIndex))
	{
		case 0:
			if(VLANRuleValue==1)
			{
				var checkboxHtml = "";
				checkboxHtml = "<input type='checkbox' class='input' id='cbEnable"+VLANRuleListIndex+"' name='cbEnable"+VLANRuleListIndex+"' onclick='editVLANRuleList("+VLANRuleListIndex+", 0)' checked >";
				
				return checkboxHtml;
			}
			else if(VLANRuleValue==0)
			{
				var checkboxHtml = "";
				checkboxHtml = "<input type='checkbox' class='input' id='cbEnable"+VLANRuleListIndex+"' name='cbEnable"+VLANRuleListIndex+"' onclick='editVLANRuleList("+VLANRuleListIndex+", 0)'>";
				return checkboxHtml;
			}
			break;
		case 1:
			return VLANRuleValue;
			break;
		case 2:
			var sSelectLANPort = "";
			for(var i=0; i<4; i++)
			{
				if((VLANRuleValue & Math.pow(2, i)) != 0) // bit computing ex. 8(1000) & 4(0010) = false
				{
					$("cbLANPort" + (i+1)).disabled = true;
					$("cbLANPort" + (i+1)).checked = false;
					if(sSelectLANPort != "")
						sSelectLANPort += ", "
					sSelectLANPort += "Port" + (i+1);
				}
			}
			if(sSelectLANPort == "")
				sSelectLANPort += "None";
			return sSelectLANPort;
			break;
		case 3:
			var iWirelessValue1 = 0;
			var iWirelessValue2 = 0;

			for(var i=0; i<8; i++)
			{
				if((VLANRuleValue & Math.pow(2, i)) != 0) // bit computing ex. 8(1000) & 4(0100) = false, 16 & 16 = true
				{
					if(iWirelessValue1 == 0)
						iWirelessValue1 = Math.pow(2, i);
					else
						iWirelessValue2 = Math.pow(2, i);
				}
			}
		
			var iArrayWirelessValueIndex1 = 0;
			var iArrayWirelessValueIndex2 = 0;
			var ArrayWirelessValueValue1 = "None";
			var ArrayWirelessValueValue2 = "None";

			if(iWirelessValue1 != 0)
			{
				iArrayWirelessValueIndex1 = parseInt($j.inArray(parseInt(iWirelessValue1),arrayWirelessValue));
				$("aWireless_1_"+iWirelessValue1).style.display="none";
				$("aWireless_2_"+iWirelessValue1).style.display="none";
				ArrayWirelessValueValue1 = arrayWirelessName[iArrayWirelessValueIndex1];		
			}
			if(iWirelessValue2 != 0)
			{
				iArrayWirelessValueIndex2 = parseInt($j.inArray(parseInt(iWirelessValue2),arrayWirelessValue));
				$("aWireless_1_"+iWirelessValue2).style.display="none";
				$("aWireless_2_"+iWirelessValue2).style.display="none";
				ArrayWirelessValueValue2 = arrayWirelessName[iArrayWirelessValueIndex2];
			}
			return ArrayWirelessValueValue1 + ">" + ArrayWirelessValueValue2;
			break;
		case 4:
			var iSubnetIndex = parseInt(VLANRuleValue.replace("LAN",""));
			for(var i=0; i< document.form.selSubnet.options.length; i++)
			{
				if(document.form.selSubnet.options[i].value == VLANRuleValue)
				{
					document.form.selSubnet.options.remove(i);
				}
			}
			document.form.selSubnet.selectedIndex = "0";
			return VLANRuleValue;
			break;
		case 5:
			if(VLANRuleValue==1)
			{
				var checkboxHtml = "";
				checkboxHtml = "<input type='checkbox' class='input' id='cbIntranetOnly"+VLANRuleListIndex+"' name='cbIntranetOnly"+VLANRuleListIndex+"' onclick='editVLANRuleList("+VLANRuleListIndex+", 5)' checked >";
				
				return checkboxHtml;
			}
			else if(VLANRuleValue==0)
			{
				var checkboxHtml = "";
				checkboxHtml = "<input type='checkbox' class='input' id='cbIntranetOnly"+VLANRuleListIndex+"' name='cbIntranetOnly"+VLANRuleListIndex+"' onclick='editVLANRuleList("+VLANRuleListIndex+", 5)'>";
				return checkboxHtml;
			}
			break;		
	}
}

function del_Row(r)
{
	var delIndex = r.parentNode.parentNode.rowIndex;
	var vlan_rulelist_row = vlan_rulelist_array.split('<');
	vlan_rulelist_array = "";
	for(var i = 1; i < vlan_rulelist_row.length; i++)
	{
		if( (delIndex) != (i+2)) //Because tbVLANRuleList default have 3 rows, so vlan_rulelist_row create in tbVLANRuleList the row index start is 3.
		{
			vlan_rulelist_array += "<";
			vlan_rulelist_array += vlan_rulelist_row[i];
		}
	}

	showVLANRuleList();
}

function showVLANRuleList()
{
	initEditItem();

	var vlan_rulelist_row = vlan_rulelist_array.split('<');

	var htmlcode = "";
	var table = document.getElementById("tbVLANRuleList");
	if(vlan_rulelist_row.length == 1)
	{		
		var row = table.insertRow(-1);
		row.id = "trVLANRuleListEmpty";
		var row = document.getElementById("trVLANRuleListEmpty");
		var td = row.insertCell(-1);
		td.colSpan = 8;
		td.style.color = "#FFCC00";
		htmlcode +='<#IPConnection_VSList_Norule#>';
		td.innerHTML = htmlcode;
		$("tDescription").value = "VLAN1";
	}
	else
	{
		$("tDescription").value = "VLAN" + vlan_rulelist_row.length;
		for(var i = 1; i < vlan_rulelist_row.length; i++)
		{
			var row = table.insertRow(-1);
			row.id = "trVLANRuleList"+i;
			var row = document.getElementById("trVLANRuleList"+i);
			var vlan_rulelist_col = vlan_rulelist_row[i].split('>');
			for(var j = 0; j < vlan_rulelist_col.length; j++)
			{
				htmlcode = switchSaveValueToDisplayValue(vlan_rulelist_col[j], j, i);
				switch(j)
				{
					case 3:
						var td1 = row.insertCell(-1);
						td1.className = "tdVLANRuleList_Block";
						td1.innerHTML = htmlcode.split('>')[0];
						var td2 = row.insertCell(-1);
						td2.className = "tdVLANRuleList_Block";
						td2.innerHTML = htmlcode.split('>')[1];
						break;
					default:
						var td = row.insertCell(-1);
						td.className = "tdVLANRuleList_Block";
						td.innerHTML = 	htmlcode;
						break;
				}
			}
			var tdDelete = row.insertCell(-1);
			htmlcode ='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
			tdDelete.innerHTML = htmlcode;
		}
	}
}

function applyRule()
{
	document.form.vlan_rulelist.value = vlan_rulelist_array;
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
<input type="hidden" name="current_page" value="Advanced_VLAN_Content.asp">
<input type="hidden" name="next_page" value="Advanced_VLAN_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vlan_rulelist" value="<% nvram_char_to_ascii("","vlan_rulelist"); %>">
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
									<div class="formfonttitle">VLAN</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">VLAN function allow you segment your network. A port-based VLAN is a group of ports on a Gigabit Ethernet
Switch that form a logical Ethernet segment. Each port of a port-based VLAN can belong to only one VLAN at a
time. You also can only assign SSID to a subnet to create a VLAN segment. If you want to modify subnet
configuration. Please go to DHCP setting.</div>
									<table id="tbVLANRuleList" width="100%" border="1" align="center" cellpadding="4" cellspacing="0"  class="FormTable_table">
										<thead>
										<tr>
											<td id="tdVLANRuleList" colspan="8">Configuration</td>
										</tr>
										</thead>
										<tr>
											<th>Enable</th>
											<th>VLAN</th>
											<th>LAN</th>
											<th id="thWireless1">Wireless1</th>
											<th id="thWireless2">Wireless2</th>
											<th>Subnet</th>
											<th>Intranet only</th>
											<!--th>Member isolated</th-->
											<th><#list_add_delete#></th>
										</tr>
										<tr>
										<td >
											<input type="checkbox" class="input" name="cbEnable" checked>
										</td>
										<td style="width:14%;">
											 <input type="text" maxlength="10" class="input_12_table" id="tDescription" name="tDescription">
										</td>
										<td id="tdLANPortList" style="width:24%;" nowrap="nowrap">
											<input type="checkbox" class="input" id="cbLANPort1" name="cbLANPort1">P1
											<input type="checkbox" class="input" id="cbLANPort2" name="cbLANPort2">P2
											<input type="checkbox" class="input" id="cbLANPort3" name="cbLANPort3">P3
											<input type="checkbox" class="input" id="cbLANPort4" name="cbLANPort4">P4
										</td>
										<td id="tdWireless1" style="width:31%;">
											<input type="text" maxlength="0" class="input_12_table" id="tWireless1" name="tWireless1" style="float:left;width:97px;" onblur="if(!over_var){hideWirelessBlock(1);}" >
											<img id="imgPullArrow1" class="pull_arrow" height="14px;" src="images/arrow-down.gif"  onclick="pullWirelessList(this, 1);">
											<div id="divWirelessBlock1" class="ClientList_Block" style="display:none;"></div>
										</td>
										<td id="tdWireless2" style="width:31%;">
											<input type="text" maxlength="0" class="input_12_table" id="tWireless2" name="tWireless2" style="float:left;width:97px;" onblur="if(!over_var){hideWirelessBlock(2);}" >
											<img id="imgPullArrow2" class="pull_arrow" height="14px;" src="images/arrow-down.gif"  onclick="pullWirelessList(this, 2);">
											<div id="divWirelessBlock2" class="ClientList_Block" style="display:none;"></div>
										</td>
										<td >
											<select name="selSubnet" class="input_option"></select>
										</td>
										<td>
											<input type="checkbox" class="input" name="cbIntranetOnly">
										</td>
										<!--td width="3%">
											<input type="checkbox" class="input" name="cbMemberIsolated">
										</td-->
										<td>
											<input type="button" class="add_btn" onClick="addRowGroup();" name="" value="">
										</td>
										</tr>
									</table>
									<div id="divVLANRulelistBlock" style="width:100%"></div>
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

