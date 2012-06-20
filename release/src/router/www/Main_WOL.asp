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
<title>ASUS Wireless Router <#Web_Title#> - WakeOnLAN</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
#ClientList_Block_PC{
        border:1px inset #999;
        background-color:#576D73;
        width:255px;
        text-align:left;
        height:auto;
        overflow-y:auto;
        z-index:200;
        padding: 1px;
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
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function initial()
{
	show_menu();
        showLANIPList();
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
/*        document.form.SystemCmd.value = "/usr/bin/ether-wake -i br0 -b "+macaddr; */
        document.form.SystemCmd.value = "wol "+macaddr+" -i `ip a show br0 | head -n 3 | tail -n 1 | cut -d ' ' -f 8`";
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
                code += '<a href="#"><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[3]+'\', \''+client_list_col[2]+'\');"><strong>'+client_list_col[3]+'</strong> ';
                
                if(show_name && show_name.length > 0)
                                code += '( '+show_name+')';
                code += ' </div></a>';
                }
        code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->'; 
        $("ClientList_Block_PC").innerHTML = code;
}

function setClientIP(macaddr, ipaddr){
	document.form.wakemac.value = macaddr;

        hideClients_Block();
        over_var = 0;
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
                document.form.macaddr.focus();               
                isMenuopen = 1;
        }
        else
                hideClients_Block();
}

//Viz add 2012.02 DHCP client MAC } end 

</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="GET" name="form" action="/apply.cgi">
<input type="hidden" name="current_page" value="Main_WOL.asp">
<input type="hidden" name="next_page" value="Main_WOL.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
                <div class="formfontdesc">Enter the MAC address of the computer to wake up:</div>
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
			<tr>
	        		<th>Target MAC address:</th>
			        <td><input type="text" id="wakemac" size=17 maxlength=17 name="wakemac" class="input_macaddr_table" value="" onKeyPress="return is_hwaddr(this,event)" onClick="hideClients_Block();" onblur="check_macaddr(this,check_hwaddr_temp(this))">
			        <input type="button" id="wake" name="Wake" class="button_gen" onclick="wakeup();" value="Wake" />
			        </td>
		        </tr>
	 		<tr>
				<th>Recently seen clients:</th>
				<td>
			        <div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
				</td>
			</tr>
		</table>
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

