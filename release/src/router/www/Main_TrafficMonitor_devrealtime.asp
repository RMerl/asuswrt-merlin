<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge" />
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">

<title><#Web_Title#> - <#traffic_monitor#> : Realtime IPTraffic</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="tmmenu.css">
<link rel="stylesheet" type="text/css" href="menu_style.css"> <!-- Viz 2010.09 -->
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<script language="JavaScript" type="text/javascript" src="help.js"></script>
<script language="JavaScript" type="text/javascript" src="state.js"></script>
<script language="JavaScript" type="text/javascript" src="general.js"></script>
<script language="JavaScript" type="text/javascript" src="tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="tmcal.js"></script>
<script language="JavaScript" type="text/javascript" src="tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="popup.js"></script>
<script language="JavaScript" type="text/javascript" src="merlin.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>

<script type='text/javascript'>

// disable auto log out
AUTOLOGOUT_MAX_MINUTE = 0;
iptraffic = [];
<% backup_nvram("wan_ifname,cstats_enable,lan_ipaddr,lan_netmask,dhcp_staticlist"); %>;
var client_list_array = '<% get_client_detail_info(); %>';
var cstats_busy = 0;
<% iptraffic(); %>;

sortColumn = 0;
var updating = 0;
var scale = 1;

var filterip = [];
var filteripe = [];
var prevtimestamp = new Date().getTime();
var thistimestamp;
var difftimestamp;
var avgiptraffic = [];
var lastiptraffic = iptraffic;

var ref = new TomatoRefresh('update.cgi', 'output=iptraffic', 2);


ref.refresh = function(text) {

	++updating;

	var i, b, j, k, l;

	thistimestamp = new Date().getTime();

	try {
		eval(text);
	}
	catch (ex) {
		iptraffic = [];
		cstats_busy = 1;
	}

	difftimestamp = thistimestamp - prevtimestamp;
	prevtimestamp = thistimestamp;

	for (i = 0; i < iptraffic.length; ++i) {
		b = iptraffic[i];

		j = getArrayPosByElement(avgiptraffic, b[0], 0);
		if (j == -1) {
			j = avgiptraffic.length;
			avgiptraffic[j] = [ b[0], 0, 0, 0, 0, 0, 0, 0, 0, b[9], b[10] ];
		}

		k = getArrayPosByElement(lastiptraffic, b[0], 0);
		if (k == -1) {
			k = lastiptraffic.length;
			lastiptraffic[k] = b;
		}

		for (l = 1; l <= 8; ++l) {
			avgiptraffic[j][l] = ((b[l] - lastiptraffic[k][l]) / difftimestamp * 1000);
			lastiptraffic[k][l] = b[l];
		}

		avgiptraffic[j][9] = b[9];
		avgiptraffic[j][10] = b[10];
		lastiptraffic[k][9] = b[9];
		lastiptraffic[k][10] = b[10];
	}

	--updating;

	avgiptraffic.sort(sortCompare);
	redraw();
}


function redraw() {
	if ((updating) || (cstats_busy)) return;

	var hostslisted = [];

	var grid;
	var i, b, x;
	var fskip;
	var rows = 0;
	var tx = 0;
	var rx = 0;
	var tcpi = 0;
	var tcpo = 0;
	var udpi = 0;
	var udpo = 0;
	var icmpi = 0;
	var icmpo = 0;
	var tcpconn = 0;
	var udpconn = 0;

	sortfield = "color: #FFCC00;";
	grid = '<table width="730px" class="FormTable_NWM">';
	grid += '<thead class="FormTable"><tr><th colspan="8" style="color:#fff; text-align:left; font-weight:bold;">Realtime Traffic</th></tr></thead>'
	grid += '<tr class="traffictable"><th onclick="setSort(this, 0);" style="min-width: 100px; ' + (sortColumn == 0 ? sortfield : "") + '">Host</th>';
	grid += '<th onclick="setSort(this, 1);" style="' + (sortColumn == 1 ? sortfield : "") + '">Reception<br>(bytes/s)</th>';
	grid += '<th onclick="setSort(this, 2);" style="' + (sortColumn == 2 ? sortfield : "") + '">Transmission<br>(bytes/s)</th>';
	grid += '<th onclick="setSort(this, 3);" style="' + (sortColumn == 3 ? sortfield : "") + '">TCP In/Out<br>(pkts/s)</th>';
	grid += '<th onclick="setSort(this, 4);" style="' + (sortColumn == 4 ? sortfield : "") + '">UDP In/Out<br>(pkts/s)</th>';
	grid += '<th onclick="setSort(this, 5);" style="' + (sortColumn == 5 ? sortfield : "") + '">ICMP In/Out<br>(pkts/s)</th>';
	grid += '<th onclick="setSort(this, 6);" style="' + (sortColumn == 6 ? sortfield : "") + '">TCP <br>Connections</th>';
	grid += '<th onclick="setSort(this, 7);" style="' + (sortColumn == 7 ? sortfield : "") + '">UDP <br>Connections</th>';

	for (i = 0; i < avgiptraffic.length; ++i) {
		fskip = 0;
		b = avgiptraffic[i];

		if (getRadioValue(document.form._f_show_zero) == 0) {
			if ((b[2] < 10) && (b[3] < 10))
				continue;
		}

		if (filteripe.length>0) {
			fskip = 0;
			for (var x = 0; x < filteripe.length; ++x) {
				if (b[0] == filteripe[x]){
					fskip=1;
					break;
				}
			}
			if (fskip == 1) continue;
		}

		if (filterip.length>0) {
			fskip = 1;
			for (var x = 0; x < filterip.length; ++x) {
				if (b[0] == filterip[x]){
					fskip=0;
					break;
				}
			}
			if (fskip == 1) continue;
		}


		rx += b[1];
		tx += b[2];
		tcpi += b[3];
		tcpo += b[4];
		udpi += b[5];
		udpo += b[6];
		icmpi += b[7];
		icmpo += b[8];
		tcpconn += b[9];
		udpconn += b[10];
		hostslisted.push(b[0]);

		var h = b[0];
		var clientObj;

		if (getRadioValue(document.form._f_show_hostnames) == 1) {
			clientObj = clientFromIP(b[0]);
			if ((clientObj) && (clientObj.name != "")) {
				h = "<b>" + clientObj.name + '</b>  <small>(' + b[0] + ')</small>';
			}
		}

		grid += addrow("",
			h,
			rescale((b[1]/1024).toFixed(2)).toString(),
			rescale((b[2]/1024).toFixed(2)).toString(),
			b[3].toFixed(0).toString(),
			b[4].toFixed(0).toString(),
			b[5].toFixed(0).toString(),
			b[6].toFixed(0).toString(),
			b[7].toFixed(0).toString(),
			b[8].toFixed(0).toString(),
			b[9].toString(),
			b[10].toString(),
			b[0]);

		++rows;
	}

	if(rows == 0)
		grid +='<tr><td style="color:#FFCC00;" colspan="8"><#IPConnection_VSList_Norule#></td></tr>';

	if(rows > 1)
		grid += addrow("traffictable_footer",
			'Total: ' + ('<small><i>(' + ((hostslisted.length > 0) ? (hostslisted.length + ' hosts') : 'no data') + ')</i></small>'),
			rescale((rx/1024).toFixed(2)).toString(),
			rescale((tx/1024).toFixed(2)).toString(),
			tcpi.toFixed(0).toString(),
			tcpo.toFixed(0).toString(),
			udpi.toFixed(0).toString(),
			udpo.toFixed(0).toString(),
			icmpi.toFixed(0).toString(),
			icmpo.toFixed(0).toString(),
			tcpconn.toString(),
			udpconn.toString(),
			"");

	E('bwm-details-grid').innerHTML = grid + '</table>';
}


function addrow(rclass, host, dl, ul, tcpin, tcpout, udpin, udpout, icmpin, icmpout, tcpconn, udpconn, ip) {
	sep = "<span> / </span>";
	if (ip != "")
		link = 'class="traffictable_link" onclick="popupWindow(\'' + ip +'\');"'
	else
		link = "";

	return '<tr class="' + rclass + '">' +
                '<td ' + link + ' >' + host + '</td>' +
                '<td style="text-align: right; padding-right: 8px;">' + dl + '</td>' +
                '<td style="text-align: right; padding-right: 8px;">' + ul + '</td>' +
                '<td>' + tcpin + sep +tcpout + '</td>' +
                '<td>' + udpin + sep + udpout + '</td>' +
                '<td>' + icmpin + sep + icmpout + '</td>' +
                '<td>' + tcpconn + '</td>' +
                '<td>' + udpconn + '</td>' +
                '</tr>';
}


function setSort(o,value) {
	o.style.color="#FFCC00";
	sortColumn = value;
	avgiptraffic.sort(sortCompare);

	update_display("sortfield", value);
}


function sortCompare(a, b) {
	var r = 0;

	switch (sortColumn) {

		case 0:	// host
			r = aton(b[0])-aton(a[0]);
			break;
		case 1:	// Download
			r = cmpFloat(a[1], b[1]);
			break;
		case 2:	// Upload
			r = cmpFloat(a[2], b[2]);
			break;
		case 3:	// TCP pkts
			r = cmpInt(a[3]+a[4], b[3]+b[4]);
			break;
		case 4:	// UDP pkts
			r = cmpInt(a[5]+a[6], b[5]+b[6]);
			break;
		case 5:	// ICMP pkts
			r = cmpInt(a[7]+a[8], b[7]+b[8]);
			break;
		case 6:	// TCP connections
			r = cmpInt(a[9], b[9]);
			break;
		case 7:	// UDP connections
			r = cmpInt(a[10], b[10]);
			break;
	}
	return -r;
}


function update_display(option, value) {
	cookie.set('ipt_rt_' + option, value);
	redraw();
}


function _validate_iplist(o, event) {
	if (event.which == null)
		keyPressed = event.keyCode;     // IE
	else if (event.which != 0 && event.charCode != 0)
		keyPressed = event.which        // All others
	else
		return true;                    // Special key

	if (keyPressed == 13) {				// Enter
		update_filter();
		return true;
	} else {
		return validate_iplist(o, event);
	}
}


function update_scale(o) {
	scale = o.value;
	cookie.set('realtime', scale, 31);
	return changeScale(o);
}


function update_filter() {
	var i;

	if (document.form._f_filter_ip.value.length>0) {
		filterip = document.form._f_filter_ip.value.split(',');
		for (i = 0; i < filterip.length; ++i) {
			if ((filterip[i] = fixIP(filterip[i])) == null) {
				filterip.splice(i,1);
			}
		}
		document.form._f_filter_ip.value = (filterip.length > 0) ? filterip.join(',') : '';
	} else {
		filterip = [];
	}

	if (document.form._f_filter_ipe.value.length>0) {
		filteripe = document.form._f_filter_ipe.value.split(',');
		for (i = 0; i < filteripe.length; ++i) {
			if ((filteripe[i] = fixIP(filteripe[i])) == null) {
				filteripe.splice(i,1);
			}
		}
		document.form._f_filter_ipe.value = (filteripe.length > 0) ? filteripe.join(',') : '';
	} else {
		filteripe = [];
	}

	cookie.set('ipt_rt_addr_shown', (filterip.length > 0) ? filterip.join(',') : '', 1);
	cookie.set('ipt_rt_addr_hidden', (filteripe.length > 0) ? filteripe.join(',') : '', 1);

	redraw();
}


function update_visibility() {
	s = getRadioValue(document.form._f_show_options);

	for (i = 0; i < 4; i++) {
		showhide("adv" + i, s);
	}

	cookie.set('ipt_rt_options', s);
}


function getArrayPosByElement(haystack, needle, index) {
	for (var i = 0; i < haystack.length; ++i) {
		if (haystack[i][index] == needle) {
			return i;
		}
	}
	return -1;
}


function popupWindow(ip) {
	cookie.set("ipt_singleip",ip,1)
	window.open("Main_TrafficMonitor_devdaily.asp", '', 'width=1100,height=600,toolbar=no,menubar=no,scrollbars=yes,resizable=yes');
}


function init() {
	if (nvram.cstats_enable == '1') {
		selGroup = E('page_select');

		optGroup = document.createElement('OPTGROUP');
		optGroup.label = "Per device";

		opt = document.createElement('option');
		opt.innerHTML = "<#menu4_2_1#>";
		opt.value = "5";
		opt.selected = true;
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

	if ((c = cookie.get('realtime')) != null) {
		if (c.match(/^([0-2])$/)) {
			scale = RegExp.$1 * 1;
		}
	}
	E('scale').value = scale;

	if ((c = cookie.get('ipt_rt_sortfield')) != null) {
		if (c < 8) {
			sortColumn = parseInt(c);
		}
	}

	if ((c = cookie.get('ipt_singleip')) != null) {
		cookie.unset('ipt_singleip');
		filterip = c.split(',');

	} else {
		if ((c = cookie.get('ipt_rt_addr_shown')) != null) {
			if (c.length>6) {
				document.form._f_filter_ip.value = c;
				filterip = c.split(',');
			}
		}

		if ((c = cookie.get('ipt_rt_addr_hidden')) != null) {
			if (c.length>6) {
				document.form._f_filter_ipe.value = c;
				filteripe = c.split(',');
			}
		}

		if ((c = cookie.get('ipt_rt_options')) != null ) {
			setRadioValue(document.form._f_show_options , (c == 1))
		}

		if ((c = cookie.get('ipt_rt_zero')) != null ) {
			setRadioValue(document.form._f_show_zero , (c == 1))
		}
	}

	if ((c = cookie.get('ipt_rt_hostnames')) != null ) {
		setRadioValue(document.form._f_show_hostnames , (c == 1))
	}

	show_menu();
	update_visibility();
	ref.start();
}

function switchPage(page){
	if(page == "1")
		location.href = "/Main_TrafficMonitor_realtime.asp";
	else if(page == "2")
		location.href = "/Main_TrafficMonitor_last24.asp";
	else if(page == "3")
		location.href = "/Main_TrafficMonitor_daily.asp";
	else if(page == "4")
		location.href = "/Main_TrafficMonitor_monthly.asp";
	else if(page == "6")
		location.href = "/Main_TrafficMonitor_devdaily.asp";
	else if(page == "7")
		location.href = "/Main_TrafficMonitor_devmonthly.asp";
	else
		return false;
}

</script>
</head>

<body onload="init();">

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_TrafficMonitor_devrealtime.asp">
<input type="hidden" name="next_page" value="Main_TrafficMonitor_devrealtime.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
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
										<div style="margin-top:5px;"><#Menu_TrafficManager#> - Traffic Monitor per device</div>
									</td>
          				<td>
     							<div align="right">
			    					<select id="page_select" class="input_option" style="width:120px" onchange="switchPage(this.options[this.selectedIndex].value)">
											<optgroup label="Global">
												<option value="1"><#menu4_2_1#></option>
												<option value="2"><#menu4_2_2#></option>
												<option value="3"><#menu4_2_3#></option>
												<option value="4">Monthly</option>
											</optgroup>
										</select>

									</div>
								</td>
        			</tr>
					</table></td></tr>

					<tr>
						<td>
							<div class="formfontdesc">
								<p>Click on a column header to sort by that field.
								<p>Click on a host to view the daily history for that host.
							</div>
						</td>
					</tr>
        			<tr>
          				<td height="5"><img src="images/New_ui/export/line_export.png" /></td>
        			</tr>
					<tr>
						<td bgcolor="#4D595D">
							<table width="730"  border="1" align="left" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
								<thead>
									<tr>
										<td colspan="2">Display Options</td>
									</tr>
								</thead>
								<tbody>
									<tr>
										<th width="40%"><#Scale#></th>
										<td>
											<select style="width:70px" class="input_option" onchange='update_scale(this)' id='scale'>
												<option value=0>KB</option>
												<option value=1>MB</option>
												<option value=2>GB</option>
											</select>
										</td>
									</tr>

									<tr>
										<th>Display advanced filter options</th>
										<td>
											<input type="radio" name="_f_show_options" class="input" value="1" onclick="update_visibility();"><#checkbox_Yes#>
											<input type="radio" name="_f_show_options" class="input" checked value="0" onclick="update_visibility();"><#checkbox_No#>
										</td>
				 					</tr>
									<tr id="adv0">
										<th>List of IPs to display (comma-separated):</th>
										<td>
											<input type="text" maxlength="512" class="input_32_table" name="_f_filter_ip" onKeyPress="return _validate_iplist(this,event);" onchange="update_filter();">
										</td>
									</tr>
									<tr id="adv1">
										<th>List of IPs to exclude (comma-separated):</th>
										<td>
											<input type="text" maxlength="512" class="input_32_table" name="_f_filter_ipe" onKeyPress="return _validate_iplist(this,event);" onchange="update_filter();">
										</td>
									</tr>
									<tr id="adv2">
										<th>Display hostnames</th>
						        		<td>
											<input type="radio" name="_f_show_hostnames" class="input" value="1" checked onclick="update_display('hostnames',1);"><#checkbox_Yes#>
											<input type="radio" name="_f_show_hostnames" class="input" value="0" onclick="update_display('hostnames',0);"><#checkbox_No#>
							   			</td>
									</tr>
									<tr id="adv3">
										<th>Display IPs with no traffic</th>
						        		<td>
											<input type="radio" name="_f_show_zero" class="input" value="1" checked onclick="update_display('zero',1);"><#checkbox_Yes#>
											<input type="radio" name="_f_show_zero" class="input" value="0" onclick="update_display('zero',0);"><#checkbox_No#>
							   			</td>
									</tr>
								</tbody>
							</table>
						</td>
					</tr>
					<tr>
						<td>
							<div id='bwm-details-grid' style='float:left'></div>
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
