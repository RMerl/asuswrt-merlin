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

<title><#Web_Title#> - WakeOnLan</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
a.wol_entry:link {
        text-decoration: underline;
        color: #FFFFFF;
        font-family: Lucida Console;
}
a.wol_entry:visited {
        text-decoration: underline;
        color: #FFFFFF;
}
a.wol_entry:hover {
        text-decoration: underline;
        color: #FFFFFF;
}
a.wol_entry:active {
        text-decoration: none;
        color: #FFFFFF;
}

#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	margin-top:103px;
	*margin-top:96px;
	margin-left:15px;
	width:255px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var wol_list_array = '<% nvram_get("wol_list"); %>';
var wol_lists = '<% nvram_get("wol_list"); %>';
var staticclist_row = wol_lists.split('&#60');

function initial()
{
	show_menu();

	showwol_list();
	showLANIPList();
}

function addRow(obj, head){
	if(head == 1)
		wol_list_array += "&#60"
	else
		wol_list_array += "&#62"

	wol_list_array += obj.value;

	obj.value = "";
}

function addRow_Group(upper){

	var rule_num = $('wol_list_table').rows.length;
	var item_num = $('wol_list_table').rows[0].cells.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.form.wol_mac_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.wol_mac_x_0.focus();
		document.form.wol_mac_x_0.select();
		return false;
	}else if(document.form.wol_name_x_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.wol_name_x_0.focus();
		document.form.wol_name_x_0.select();
		return false;
	}else if(check_macaddr(document.form.wol_mac_x_0, check_hwaddr_flag(document.form.wol_mac_x_0)) == true ){

		if(item_num >=2){
			for(i=0; i<rule_num; i++){
					if(document.form.wol_mac_x_0.value.toLowerCase() == $('wol_list_table').rows[i].cells[0].innerHTML.toLowerCase()){
						alert("<#JS_duplicate#>");
						document.form.wol_mac_x_0.focus();
						document.form.wol_mac_x_0.select();
						return false;
					}
			}
		}

		addRow(document.form.wol_mac_x_0 ,1);
		addRow(document.form.wol_name_x_0, 0);
		showwol_list();
	}else{
		return false;
	}
}


function del_Row(r){
  var i=r.parentNode.parentNode.rowIndex;
  $('wol_list_table').deleteRow(i);

  var wol_list_value = "";
	for(k=0; k<$('wol_list_table').rows.length; k++){
		for(j=0; j<$('wol_list_table').rows[k].cells.length-1; j++){
			if(j == 0)
				wol_list_value += "&#60";
			else
				wol_list_value += "&#62";
			wol_list_value += $('wol_list_table').rows[k].cells[j].innerHTML;
		}
	}

	wol_list_array = wol_list_value;
	if(wol_list_array == "")
		showwol_list();
}

function edit_Row(r){
	var i=r.parentNode.parentNode.rowIndex;
	document.form.wol_mac_x_0.value = $('wol_list_table').rows[i].cells[0].innerHTML;
	document.form.wol_name_x_0.value = $('wol_list_table').rows[i].cells[1].innerHTML;
  del_Row(r);
}


function showwol_list(){
	var wol_list_row = wol_list_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="wol_list_table">';
	if(wol_list_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < wol_list_row.length; i++){
			code +='<tr id="row'+i+'">';
			var wol_list_col = wol_list_row[i].split('&#62');
				for(var j = 0; j < wol_list_col.length; j++){
					code +='<td width="35%">'+ wol_list_col[j] +'</td>';
				}
				code +='<td width="10%"><a class="wol_entry" href="#" onclick="setTargetMAC(\''+wol_list_col[0]+'\');">Wake</a></td>';
				code +='<td width="20%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}
	code +='</table>';
	$("wol_list_Block").innerHTML = code;
}

function applyRule(){
	var rule_num = $('wol_list_table').rows.length;
	var item_num = $('wol_list_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<"
		for(j=0; j<item_num-2; j++){
			tmp_value += $('wol_list_table').rows[i].cells[j].innerHTML;
			if(j != item_num-3)
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";

	document.form.wol_list.value = tmp_value;

	showLoading();
	document.form.submit();
}


function wakeup()
{
	o=document.getElementById('wakemac');
        macaddr=o.value;

	if (o.value.length != 17) {
		alert("<#JS_validmac#>");
		return false;
	}

        document.form.action_mode.value = " Refresh ";
  //      document.form.SystemCmd.value = "/usr/bin/ether-wake -i br0 -b "+macaddr;
	document.form.SystemCmd.value = "wol "+macaddr+" -i `ip a show br0 | head -n 3 | tail -n 1 | cut -d ' ' -f 8`";
	document.form.action="/apply.cgi"
	document.form.target = "";
	document.form.submit();
}


//Viz add 2012.02 DHCP client MAC { start

function showLANIPList(){
        var code = "";
        var show_name = "";
        var client_list_array = '<% get_client_detail_info(); %>';
        var client_list_row = client_list_array.split('<');

        for(var i = 1; i < client_list_row.length; i++){
                var client_list_col = client_list_row[i].split('>');
                if(client_list_col[1] && client_list_col[1].length > 20)
                        show_name = client_list_col[1].substring(0, 16) + "..";
                else
                        show_name = client_list_col[1];

                //client_list_col[]  0:type 1:device 2:ip 3:mac 4: 5: 6:
                code += '<a href="#"><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientMAC(\''+client_list_col[3]+'\', \''+client_list_col[1]+'\');"><strong>'+client_list_col[3]+'</strong> ';

                if(show_name && show_name.length > 0)
                                code += '( '+show_name+')';
                code += ' </div></a>';
                }
        code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
        $("ClientList_Block_PC").innerHTML = code;
}

function setClientMAC(macaddr, name){
	document.form.wol_mac_x_0.value = macaddr;
	document.form.wol_name_x_0.value = trim(name);
	hideClients_Block();
	over_var = 0;

}

function setTargetMAC(macaddr){
	document.form.wakemac.value = macaddr;
	wakeup();
	return false;
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
        $("pull_arrow").src = "/images/arrow-down.gif";
        $('ClientList_Block_PC').style.display='none';
        isMenuopen = 0;
}

function pullLANIPList(obj){

        if(isMenuopen == 0){
                obj.src = "/images/arrow-top.gif"
                $("ClientList_Block_PC").style.display = 'block';
                document.form.wol_mac_x_0.focus();
                isMenuopen = 1;
        }
        else
                hideClients_Block();
}

//Viz add 2012.02 DHCP client MAC } end
function check_macaddr(obj,flag){ //control hint of input mac address

	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		$("check_mac").style.display = "";
		obj.focus();
		obj.select();
                $("wake").disabled=true;
		return false;
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML=Untranslated.illegal_MAC;
		$("check_mac").style.display = "";
		obj.focus();
		obj.select();
		$("wake").disabled=true;
		return false;
	}else{
		$("check_mac") ? $("check_mac").style.display="none" : true;
                $("wake").disabled=false;
		return true;
	}
}


</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Tools_WOL.asp">
<input type="hidden" name="next_page" value="Tools_WOL.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wol_list" value="">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div></td>
    <td valign="top">
        <div id="tabMenu" class="submenuBlock"></div>

      <!--===================================Beginning of Main Content===========================================-->
      <table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        <tr>
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
                <div>&nbsp;</div>
                <div class="formfonttitle">Tools - WakeOnLAN</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
                <div class="formfontdesc">Enter the MAC address of the computer to wake up, or click on an entry below to select it:</div>
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
			<tr>
	        		<th>Target MAC address:</th>
			        <td><input type="text" id="wakemac" size=17 maxlength=17 name="wakemac" class="input_macaddr_table" value="" onKeyPress="return is_hwaddr(this,event)" onblur="check_macaddr(this,check_hwaddr_flag(this))">
			        <input type="button" id="wake" name="Wake" class="button_gen" onclick="wakeup();" value="Wake" />
			        </td>
		        </tr>
		</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
			  	<thead>
			  		<tr>
						<td colspan="4" id="WOLList">WOL Targets</td>
			  		</tr>
			  	</thead>

			  	<tr>
		  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);"><#LANHostConfig_ManualMac_itemname#></a></th>
        		<th colspan="2">Name</th>
        		<th>Add / Delete</th>
			  	</tr>
			  	<tr>
			  			<!-- client info -->
							<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>

            			<td width="35%">
                		<input type="text" class="input_macaddr_table" maxlength="17" name="wol_mac_x_0" style="margin-left:-12px;width:220px;" onKeyPress="return is_hwaddr(this,event)" onClick="hideClients_Block();">
                		<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;" onclick="pullLANIPList(this);" title="Select the device name of WOL target" onmouseover="over_var=1;" onmouseout="over_var=0;">
                			</td>
				<td width="45%" colspan="2">
					<input type="text" class="input_15_table" maxlenght="15" onkeypress="return is_alphanum(this,event);" name="wol_name_x_0">
				</td>
            			<td width="20%">
										<div>
											<input type="button" class="add_btn" onClick="addRow_Group(32);" value="">
										</div>
            			</td>
			  	</tr>
			  </table>

			  <div id="wol_list_Block"></div>

        	<!-- manually assigned the DHCP List end-->
           	<div class="apply_gen">
           		<input type="button" name="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
            	</div>


<!--
<% nvram_dump("syscmd.log","syscmd.sh"); %>
-->
		</tr>

	        </tbody>
        	</table>
                </form>
                </td>

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

