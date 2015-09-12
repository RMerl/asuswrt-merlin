<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge" />
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">

<title><#Web_Title#> - <#menu4_2_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="tmmenu.css">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type='text/javascript'>
var daily_history = [];
<% backup_nvram("wan_ifname,lan_ifname,rstats_enable,cstats_enable"); %>
<% bandwidth("daily"); %>

function redraw(){
	var h;
	var grid;
	var rows;
	var ymd;
	var d;
	var lastt;
	var lastu, lastd;
	var getYMD = function(n){
		return [(((n >> 16) & 0xFF) + 1900), ((n >>> 8) & 0xFF), (n & 0xFF)];
	}

	if (daily_history.length > 0) {
		ymd = getYMD(daily_history[0][0]);
		d = new Date((new Date(ymd[0], ymd[1], ymd[2], 12, 0, 0, 0)).getTime() - ((30 - 1) * 86400000));
		E('last-dates').innerHTML = '(' + ymdText(d.getFullYear(), d.getMonth(), d.getDate()) + ' ~ ' + ymdText(ymd[0], ymd[1], ymd[2]) + ')';

		lastt = ((d.getFullYear() - 1900) << 16) | (d.getMonth() << 8) | d.getDate();
	}

	lastd = 0;
	lastu = 0;
	rows = 0;
	block = '';
	gn = 0;

	grid = '<table width="730px" class="FormTable_NWM">';
	grid += "<tr><th style=\"height:30px;\"><#Date#></th>";
	grid += "<th><#tm_reception#></th>";
	grid += "<th><#tm_transmission#></th>";
	grid += "<th><#Total#></th></tr>";

	for (i = 0; i < daily_history.length; ++i) {
		h = daily_history[i];
		ymd = getYMD(h[0]);
		grid += makeRow(((rows & 1) ? 'odd' : 'even'), ymdText(ymd[0], ymd[1], ymd[2]), rescale(h[1]), rescale(h[2]), rescale(h[1] + h[2]));
		++rows;

		if (h[0] >= lastt) {
			lastd += h[1];
			lastu += h[2];
		}
	}

	if(rows == 0)
		grid +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';

	E('bwm-daily-grid').innerHTML = grid + '</table>';
	E('last-dn').innerHTML = rescale(lastd);
	E('last-up').innerHTML = rescale(lastu);
	E('last-total').innerHTML = rescale(lastu + lastd);
}

function init(){
	var s;

	if (nvram.cstats_enable == '1') {
		selGroup = E('page_select');

		optGroup = document.createElement('OPTGROUP');
		optGroup.label = "Per device";

		opt = document.createElement('option');
		opt.innerHTML = "<#menu4_2_1#>";
		opt.value = "5";
		optGroup.appendChild(opt);
		opt = document.createElement('option');
		opt.innerHTML = "<#menu4_2_3#>";
		opt.value = "6";
		optGroup.appendChild(opt);
		opt = document.createElement('option');
		opt.innerHTML = "Monthly";
		opt.value = "7";
		optGroup.appendChild(opt);

		selGroup.appendChild(optGroup);
	}

	if (nvram.rstats_enable != '1') return;
	if ((s = cookie.get('daily')) != null) {
		if (s.match(/^([0-2])$/)) {
			E('scale').value = scale = RegExp.$1 * 1;
		}
	}

	show_menu();
	initDate('ymd');
	daily_history.sort(cmpHist);
	redraw();
	if(bwdpi_support){
		document.getElementById('content_title').innerHTML = "<#menu5_3_2#> - <#traffic_monitor#>";
	}
}

function switchPage(page){
	if(page == "1")
		location.href = "/Main_TrafficMonitor_realtime.asp";
	else if(page == "2")
		location.href = "/Main_TrafficMonitor_last24.asp";
	else if(page == "4")
		location.href = "/Main_TrafficMonitor_monthly.asp";
	else if(page == "5")
		location.href = "/Main_TrafficMonitor_devrealtime.asp";
	else if(page == "6")
		location.href = "/Main_TrafficMonitor_devdaily.asp";
	else if(page == "7")
		location.href = "/Main_TrafficMonitor_devmonthly.asp";
	else
		return false;
}
</script>
</head>

<body onload="init();" >

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_TrafficMonitor_daily.asp">
<input type="hidden" name="next_page" value="Main_TrafficMonitor_daily.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
<tr>
	<td width="23">&nbsp;</td>

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
         		<td align="left"  valign="top">
				<table width="100%" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
				<tbody>
				<!--===================================Beginning of QoS Content===========================================-->
	      		<tr>
	      			<td bgcolor="#4D595D" valign="top">
	      				<table width="740px" border="0" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3">
						<tr><td><table width="100%" >
        			<tr>

						<td  class="formfonttitle" align="left">
										<div id="content_title" style="margin-top:5px;"><#Menu_TrafficManager#> - <#traffic_monitor#></div>
									</td>

          			<td>
     							<div align="right">
			    					<select id="page_select" class="input_option" style="width:120px" onchange="switchPage(this.options[this.selectedIndex].value)">
										<!--option><#switchpage#></option-->
										<optgroup label="Global">
											<option value="1"><#menu4_2_1#></option>
											<option value="2"><#menu4_2_2#></option>
											<option value="3" selected><#menu4_2_3#></option>
											<option value="4">Monthly</option>
										</optgroup>
									</select>
									</div>
								</td>
        			</tr>
					</table></td></tr>

        			<tr>
          				<td height="5"><img src="images/New_ui/export/line_export.png" /></td>
        			</tr>
						<tr>
							<td bgcolor="#4D595D">
								<table width="730"  border="1" align="left" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
									<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
									</thead>
									<tbody>
										<tr class='even'>
											<th width="40%"><#Date#></th>
											<td>
												<select class="input_option" style="width:130px" onchange='changeDate(this, "ymd")' id='dafm'>
													<option value=0>yyyy-mm-dd</option>
													<option value=1>mm-dd-yyyy</option>
													<option value=2>mm, dd, yyyy</option>
													<option value=3>dd.mm.yyyy</option>
												</select>
											</td>
										</tr>
										<tr class='even'>
											<th width="40%"><#Scale#></th>
											<td>
												<select style="width:70px" class="input_option" onchange='changeScale(this)' id='scale'>
													<option value=0>KB</option>
													<option value=1>MB</option>
													<option value=2 selected>GB</option>
												</select>
											</td>
										</tr>
									</tbody>
								</table>
							</td>
						</tr>
						<tr >
							<td>
								<div id='bwm-daily-grid' style='float:left'></div>
							</td>
						</tr>

	     					<tr >
	      					<td bgcolor="#4D595D">
	      						<table width="730"  border="1" align="left" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" >
	      						<thead>
	      						<tr>
	      							<td colspan="2" id="TriggerList" style="text-align:left;"><#Last30days#> <span style="color:#FFF;background-color:transparent;" id='last-dates'></span></td>
	      						</tr>
	      						</thead>
	      	      				<tbody>
	      						<tr class='even'><th width="40%"><#tm_reception#></th><td id='last-dn'>-</td></tr>
	      						<tr class='odd'><th width="40%"><#tm_transmission#></th><td id='last-up'>-</td></tr>
	      						<tr class='footer'><th width="40%"><#Total#></th><td id='last-total'>-</td></tr>
	      						</tbody>
	      						</table>
	      					</td>
	     					</tr>
	     					</table>
	     				</td>
	     			</tr>
				</tbody>
				</table>
			</td>
		</tr>
		</table>
		</div>
	</td>

    	<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>
</html>


