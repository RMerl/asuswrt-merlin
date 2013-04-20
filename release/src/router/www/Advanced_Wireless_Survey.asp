<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title>ASUS Wireless Router <#Web_Title#> - Visible Wifi</title>
<link rel="stylesheet" type="text/css" href="/form_style.css">
<link rel="stylesheet" type="text/css" href="qis/qis_style.css">
<link rel="stylesheet" type="text/css" href="index_style.css">

<style>
#radio_0{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -0px; width: 30px; height: 30px;
}
#radio_1{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -30px; width: 30px; height: 30px;
}
#radio_2{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -60px; width: 30px; height: 30px;
}
#radio_3{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -90px; width: 30px; height: 30px;
}
#radio_4{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -120px; width: 30px; height: 30px;
}
#radio_5{
  background: url(../images/survey/radio_status.png);
  background-position: -0px -150px; width: 30px; height: 30px;
}
p{
	font-weight: bolder;
}

</style>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/nameresolv.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>



<script type="text/JavaScript">
var aplist = new Array();
var wlc_scan_state = '<% nvram_get("wlc_scan_state"); %>';
var _wlc_ssid;
var _sw_mode;
var wlc_scan_mode = '<% nvram_get("wlc_scan_mode"); %>';

var $j = jQuery.noConflict();

var issubmit = 0;
var isrescan = 0;
if(isrescan == 120)
	isrescan = 1;

overlib_str_tmp = "";
overlib.isOut = true;

var iserror = 0;
var waitingTime = 120;


function initial(){
	show_menu();

	<%radio_status();%>

	if (radio_2 == 0)
		E("radio2warn").style.display = "";
	if ((band5g_support != -1) && (radio_5 == 0))
		E("radio5warn").style.display = "";

	document.form.scanMode.value = wlc_scan_mode;
	update_site_info();
}

// sorter [Jerry5_Chang]
function addBorder(obj){
	if(sorter.lastClick != ""){
		sorter.lastClick.style.borderTop = '1px solid #334044';
		sorter.lastClick.style.borderBottom = '1px solid #334044';
	}
	else
		document.getElementById("sigTh").style.borderBottom = '1px solid #334044';

	if(sorter.sortingMethod == "increase"){
		obj.style.borderTop='1px solid #FC0';
		obj.style.borderBottom='1px solid #334044';
	}
	else{
		obj.style.borderTop='1px solid #334044';
		obj.style.borderBottom='1px solid #FC0';
	}

	sorter.lastClick = obj;
}

function doSorter(_flag, _Method){
	if(aplist.length == 1)
		return 0;

	// Set field to sort
	sorter.indexFlag = _flag;

	// Remember data type for this field
	sorter.lastType = _Method;

	// doSorter
	eval("aplist.sort(sorter."+_Method+"_"+sorter.sortingMethod+");");

	// Swap it for next sort
	sorter.sortingMethod = (sorter.sortingMethod == "increase") ? "decrease" : "increase";

	// show Table
	showSiteTable();
}

// suit for 2 dimention array
var sorter = {
	"lastClick" : "", // a HTML object
	"sortingMethod" : "increase",
	"indexFlag" : 5, // default sort is by signal
	"lastType" : "num", // Last data type
	"num_increase" : function(a, b){
		return parseInt(a[sorter.indexFlag]) - parseInt(b[sorter.indexFlag]);
	},
	"num_decrease" : function(a, b){
		return parseInt(b[sorter.indexFlag]) - parseInt(a[sorter.indexFlag]);
	},
	"str_increase" : function(a, b){
		if(a[sorter.indexFlag].toUpperCase() == b[sorter.indexFlag].toUpperCase()) return 0;
		else if(a[sorter.indexFlag].toUpperCase() > b[sorter.indexFlag].toUpperCase()) return 1;
		else return -1;
	},
	"str_decrease" : function(a, b){
		if(a[sorter.indexFlag].toUpperCase() == b[sorter.indexFlag].toUpperCase()) return 0;
		else if(a[sorter.indexFlag].toUpperCase() > b[sorter.indexFlag].toUpperCase()) return -1;
		else return 1;
	}
}
// end

function update_site_info(){
	$j.ajax({
		url: '/apscan.asp',
		dataType: 'script',
		error: function(xhr){
			if(iserror < 2)
				setTimeout("update_site_info();", 1000);
		},
		success: function(response){
			if(wlc_scan_state != 3) {
				setTimeout("update_site_info();", 2000);
			}
			if(isrescan == 0){ // rescan onLoading
				isrescan++;
				rescan();
				wlc_scan_state = 0;
			}
			// Switch back to last used order.
			sorter.sortingMethod = (sorter.sortingMethod == "increase") ? "decrease" : "increase";
			doSorter(sorter.indexFlag, sorter.lastType);
		}
	});
}


var htmlCode_tmp = "";
function showSiteTable(){
	var htmlCode = "";
	var overlib_str = "";

	document.getElementById("SearchingIcon").style.display = "none";

	htmlCode +='<table style="width:670px;" border="0" cellspacing="0" cellpadding="4" align="center" class="QIS_survey" id="aplist_table">';

	if(wlc_scan_state != 3){ // on scanning
		htmlCode +='<tr><td style="text-align:center;font-size:12px; border-collapse: collapse;border:1;" colspan="4"><span style="color:#FFCC00;line-height:25px;"><#APSurvey_action_searching_AP#></span>&nbsp;<img style="margin-top:10px;" src="/images/InternetScan.gif"></td></tr>';
	}
	else{ // show ap list

		if(aplist[0].length == 0){
			htmlCode +='<tr><td style="text-align:center;font-size:12px; border-collapse: collapse;border:1;" colspan="4"><span style="color:#FFCC00;line-height:25px;"><#APSurvey_action_searching_noresult#></span>&nbsp;<img style="margin-top:10px;" src="/images/InternetScan.gif"></td></tr>';
		}
		else{
			for(var i = 0; i < aplist.length; i++){
				if(aplist[i][1] == null)
					continue;
				else if(aplist[i][1].search("%FFFF") != -1)
					continue;

				overlib_str = "<p><#MAC_Address#>:</p>" + aplist[i][6];

				// initial
				htmlCode += '<tr>';

				//ssid
				htmlCode += '<td id="ssid" onclick="oui_query(\'' + aplist[i][6] +'\');overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" style="cursor:pointer; text-decoration:underline;">' + decodeURIComponent(aplist[i][1]);
				htmlCode +=	'</td>';

				// channel
				htmlCode +=	'<td width="15%" style="text-align:center;">' + aplist[i][2] + '</td>';

 				// security
				if(aplist[i][3] == "Open System" && aplist[i][4] == "NONE")
					htmlCode +=	'<td width="27%">' + aplist[i][3] + '<img src="/images/New_ui/networkmap/unlock.png"></td>';
				else if(aplist[i][4] == "WEP")
					htmlCode +=	'<td width="27%">WEP</td>';
				else
					htmlCode +=	'<td width="27%">' + aplist[i][3] +' (' + aplist[i][4] + ')</td>';
				// band
				if(aplist[i][0] == "2G")
					htmlCode +=	'<td width="10%" style="text-align:center;">2.4GHz</td>';
				else
					htmlCode +=	'<td width="10%" style="text-align:center;">5GHz</td>';

				// signal
				htmlCode += '<td width="10%" style="text-align:center;"><span title="' + aplist[i][5] + '%"><div style="margin-left:13px;" id="radio_'+ Math.ceil(aplist[i][5]/20) +'"></div></span></td></tr>';

			}
			document.form.rescanButton.disabled = false;
			document.form.rescanButton.className = "button_gen";
		}
	}
	htmlCode +='</table>';

	if(htmlCode != htmlCode_tmp){
		document.getElementById("wlscan_table").innerHTML = htmlCode;
		htmlCode_tmp = htmlCode;
	}

	if(document.getElementById("wlscan_table_container").scrollHeight > document.getElementById("wlscan_table_container").clientHeight && navigator.userAgent.search("Macintosh") == -1)
		document.getElementById("wlscan_table").style.marginLeft = "32px";
	else
		document.getElementById("wlscan_table").style.marginLeft = "18px";

}


function rescan(){
	document.form.rescanButton.disabled = true;
	document.form.rescanButton.className = "button_gen_dis";
	document.form.wlc_scan_mode.value = document.form.scanMode.value;

	issubmit = 0;
	isrescan = 120; // stop rescan
	document.getElementById("SearchingIcon").style.display = "";
	document.form.flag.value = "sitesurvey";
	document.form.target = "hidden_frame";
	document.form.action_wait.value	= "1";
	document.form.action_script.value = "restart_wlcscan";
	document.form.submit();
}

</script>
</head>

<body onload="initial();" onunload="">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_Wireless_Survey.asp">
<input type="hidden" name="next_page" value="Advanced_Wireless_Survey.asp">
<input type="hidden" name="prev_page" value="Advanced_Wireless_Survey.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="flag" value="sitesurvey">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="action_script" value="restart_wlcscan">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl0_ssid" value="<% nvram_char_to_ascii("", "wl0_ssid"); %>" disabled>
<input type="hidden" name="wl1_ssid" value="<% nvram_char_to_ascii("", "wl1_ssid"); %>" disabled>
<input type="hidden" name="wlc_scan_mode" value="<% nvram_get("wlc_scan_mode"); %>">


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
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle" height="600px">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
	                <div>&nbsp;</div>
			<div class="formfonttitle">Wireless - Visible Networks</div>
			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
			<span style="display:none; color:#FFCC00; padding-right:20px;" id="radio2warn">2.4 GHz radio is disabled - cannot scan that band!</span>
			<span style="display:none; color:#FFCC00;" id="radio5warn">5 GHz radio is disabled - cannot scan that band!</span>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
				<tr>
					<th width="20%">Scan Mode</th>
					<td>
						<select id="scanMode" class="input_option" name="scanMode">
							<option value="0" selected>Active (default)</option>
							<option value="1">Passive</option>
						</select>
					</td>
				</tr>
			</table>

			<div class="apply_gen" valign="top">
				<input disabled type="button" id="rescanButton" value="<#QIS_rescan#>" onclick="rescan();" class="button_gen_dis">
				<img id="SearchingIcon" style="display:none;" src="/images/InternetScan.gif">
			</div>

			<div style="margin-left:18px;margin-top:8px;">
				<table style="width:670px;" border="0" cellspacing="0" cellpadding="4" align="center" class="QIS_survey">
					<th onclick="addBorder(this);doSorter(1, 'str');" style="cursor:pointer;"><#Wireless_name#></th>
					<th onclick="addBorder(this);doSorter(2, 'num');" width="15%" style="text-align:center;cursor:pointer;line-height:120%;"><#WLANConfig11b_Channel_itemname#></th>
					<th onclick="addBorder(this);doSorter(3, 'str');" width="27%" style="cursor:pointer;"><#QIS_finish_wireless_item2#></th>
					<th onclick="addBorder(this);doSorter(0, 'str');" width="10%" style="text-align:center;cursor:pointer;line-height:120%;;">Band</th>
					<th onclick="addBorder(this);doSorter(5, 'num');" width="10%" id="sigTh" style="border-bottom: 1px solid #FC0;text-align:center;cursor:pointer;"><#Radio#></th>
				</table>
			</div>

			<div style="height:660px;overflow-y:auto;margin-top:0px;" id="wlscan_table_container">
				<div id="wlscan_table" style="height:460px;margin-left:35px;margin-top:0px;vertical-align:top;"></div>
			</div>

		</td>
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
