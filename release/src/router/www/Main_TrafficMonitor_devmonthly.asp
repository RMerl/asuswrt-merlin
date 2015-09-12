<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge" />
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">

<title><#Web_Title#> - Monthly per IP</title>
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
<script language="JavaScript" type="text/javascript" src="/merlin.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script type='text/javascript'>
monthly_history = [];
<% backup_nvram("cstats_enable,lan_ipaddr,lan_netmask"); %>;
<% ipt_bandwidth("monthly"); %>

var filterip = [];
var filteripe = [];
var scale = 2;


function redraw() {
	var h;
	var grid;
	var rows;
	var i, b, d;
	var fskip;
	var filtered = 0;

	var style_open;
	var style_close;
	var getYMD = function(n){
		return [(((n >> 16) & 0xFF) + 1900), ((n >>> 8) & 0xFF), (n & 0xFF)];
	}

	rows = 0;

	grid = '<table width="730px" class="FormTable_NWM">';
	grid += "<tr><th style=\"height:30px;\"><#Date#></th>";
	grid += "<th>Host</th>";
	grid += "<th>Download</th>";
	grid += "<th>Upload</th>";
	grid += "<th><#Total#></th></tr>";


	if (monthly_history.length > 0) {
		for (i = 0; i < monthly_history.length; ++i) {
			fskip=0;
			style_open = "";
			style_close = "";

			b = monthly_history[i];

			if (getRadioValue(document.form._f_show_zero) == 0) {
				if ((b[2] < 1) || (b[3] < 1))
					continue;
			}

			if (document.form._f_begin_date.value.toString() != '0') {
				if (b[0] < document.form._f_begin_date.value)
					continue;
			}

			if (document.form._f_end_date.value.toString() != '0') {
				if (b[0] > document.form._f_end_date.value)
					continue;
			}

			if (b[1] == fixIP(ntoa(aton(nvram.lan_ipaddr) & aton(nvram.lan_netmask)))) {
				if (getRadioValue(document.form._f_show_subnet) == 1) {
					style_open='<span style="color: #FFCC00;">';
					style_close='</span>';
				} else {
					continue;
				}
			}

			if (filteripe.length>0) {
				fskip = 0;
				for (var x = 0; x < filteripe.length; ++x) {
					if (b[1] == filteripe[x]){
						fskip=1;
						break;
					}
				}
				if (fskip == 1) {
					filtered++;
					continue;
				}
			}

			if (filterip.length>0) {
				fskip = 1;
				for (var x = 0; x < filterip.length; ++x) {
					if (b[1] == filterip[x]){
						fskip=0;
						break;
					}
				}
				if (fskip == 1) {
					filtered++;
					continue;
				}
			}

			var h = b[1];
			var clientObj;

			if (getRadioValue(document.form._f_show_hostnames) == 1) {
				clientObj = clientFromIP(b[1]);
				if ((clientObj) && (clientObj.name != "")) {
					h = "<b>" + clientObj.name + '</b>  <small>(' + b[1] + ')</small>';
				}
			}

			var ymd = getYMD(b[0]);
			d = [ymText(ymd[0], ymd[1]), h, rescale(b[2]), rescale(b[3]), rescale(b[2]+b[3])];

			grid += addrow(((rows & 1) ? 'odd' : 'even'), ymText(ymd[0], ymd[1]), style_open + h + style_close, rescale(b[2]), rescale(b[3]), rescale(b[2]+b[3]), b[1]);
			++rows;
		}
	}

	if(filtered > 0)
		grid +='<tr><td style="color:#FFCC00;" colspan="5">'+ filtered +' entries filtered out.</td></tr>';

	if(rows == 0)
		grid +='<tr><td style="color:#FFCC00;" colspan="5"><#IPConnection_VSList_Norule#></td></tr>';

	E('bwm-monthly-grid').innerHTML = grid + '</table>';
}


function update_display(option, value) {
	cookie.set('ipt_' + option, value);
	redraw();
}


function _validate_iplist(o, event) {
	if (event.which == null)
		keyPressed = event.keyCode;     // IE
	else if (event.which != 0 && event.charCode != 0)
		keyPressed = event.which        // All others
	else
		return true;                    // Special key

	if (keyPressed == 13) {
		update_filter();
		return true;
	} else {
		return validate_iplist(o, event);
	}
}


function update_scale(o) {
	scale = o.value;
	cookie.set('monthly', scale, 31);
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

	cookie.set('ipt_addr_shown', (filterip.length > 0) ? filterip.join(',') : '', 1);
	cookie.set('ipt_addr_hidden', (filteripe.length > 0) ? filteripe.join(',') : '', 1);

	redraw();
}


function update_visibility() {
	s = getRadioValue(document.form._f_show_options);

	for (i = 0; i < 5; i++) {
		showhide("adv" + i, s);
	}

	cookie.set('ipt_options', s);
}


function addrow(rclass, rtitle, host, dl, ul, total, ip) {
	if ((ip != "") && (ip != fixIP(ntoa(aton(nvram.lan_ipaddr) & aton(nvram.lan_netmask)))))
		link = 'class="traffictable_link" onclick="popupWindow(\'' + ip +'\');"'
	else
		link = "";

	return '<tr class="' + rclass + '">' +
		'<td class="rtitle">' + rtitle + '</td>' +
                '<td ' + link + '>' + host + '</td>' +
                '<td style="text-align: right; padding-right: 8px;">' + dl + '</td>' +
                '<td style="text-align: right; padding-right: 8px;">' + ul + '</td>' +
                '<td style="text-align: right; padding-right: 8px;">' + total + '</td>' +
                '</tr>';
}


function popupWindow(ip) {
	cookie.set("ipt_singleip",ip,1)
	window.open("Main_TrafficMonitor_devrealtime.asp", '', 'width=1100,height=600,toolbar=no,menubar=no,scrollbars=yes,resizable=yes');
}


function init() {
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
		opt.selected = true;
		optGroup.appendChild(opt);

		selGroup.appendChild(optGroup);
	}

	if ((c = cookie.get('monthly')) != null) {
		if (c.match(/^([0-2])$/)) {
			scale = RegExp.$1 * 1;
		}
	}
	E('scale').value = scale;

	if ((c = cookie.get('ipt_addr_shown')) != null) {
		if (c.length>6) {
			document.form._f_filter_ip.value = c;
			filterip = c.split(',');
		}
	}

	if ((c = cookie.get('ipt_addr_hidden')) != null) {
		if (c.length>6) {
			document.form._f_filter_ipe.value = c;
			filteripe = c.split(',');
		}
	}

	if ((c = cookie.get('ipt_options')) != null ) {
		setRadioValue(document.form._f_show_options , (c == 1))
	}

	if ((c = cookie.get('ipt_zero')) != null ) {
		setRadioValue(document.form._f_show_zero , (c == 1))
	}

	if ((c = cookie.get('ipt_subnet')) != null ) {
		setRadioValue(document.form._f_show_subnet , (c == 1))
	}

	if ((c = cookie.get('ipt_hostnames')) != null ) {
		setRadioValue(document.form._f_show_hostnames , (c == 1))
	}

	show_menu();
	update_visibility();
	initDate('ymd');
	monthly_history.sort(cmpDualFields);
	init_filter_dates(2);
	updateClientList();
}


// dateselect: 0 == all, 1 == current, 2 == today

function init_filter_dates(dateselect) {
	var selected1 = false;
	var selected2 = false;
	var dates = [];
	if (monthly_history.length > 0) {
		for (var i = 0; i < monthly_history.length; ++i) {
			dates.push('0x' + monthly_history[i][0].toString(16));
		}
		if (dates.length > 0) {
			dates.sort();
			for (var j = 1; j < dates.length; j++ ) {
				if (dates[j] === dates[j - 1]) {
					dates.splice(j--, 1);
				}
			}
		}
	}
	var d = new Date();

	if (dateselect == 1) {
		current1 = document.form._f_begin_date.value;
		current2 = document.form._f_end_date.value;
	}

	free_options(document.form._f_begin_date);
	free_options(document.form._f_end_date);

	for (var i = 0; i < dates.length; ++i) {
		var ymd = getYMD(dates[i]);

		if ((dateselect == 0)) {
			selected1 = (i == 0);
			selected2 = (i == dates.length-1);
		} else if (dateselect == 1) {
			selected1 = (dates[i] == current1);
			selected2 = (dates[i] == current2);
		} else if (dateselect == 2) {
			selected1 = (i == dates.length-1);
			selected2 = (i == dates.length-1);
		}

		add_option(document.form._f_begin_date,ymText(ymd[0], ymd[1]), dates[i], selected1);
		add_option(document.form._f_end_date,ymText(ymd[0], ymd[1]), dates[i], selected2);
	}
}


function update_date_format(o, f) {
	changeDate(o, f);
	init_filter_dates(1);
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
	else if(page == "5")
		location.href = "/Main_TrafficMonitor_devrealtime.asp";
	else if(page == "6")
		location.href = "/Main_TrafficMonitor_devdaily.asp";
	else
		return false;
}

function updateClientList(e){
	$.ajax({
		url: '/update_clients.asp',
		dataType: 'script',
		error: function(xhr) {
			setTimeout("updateClientList();", 1000);
		},
		success: function(response){
			if(isJsonChanged(originData, originDataTmp)){
				redraw();
			}

			setTimeout("updateClientList();", 3000);
		}
	});
}

</script>
</head>

<body onload="init();">

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_TrafficMonitor_devmonthly.asp">
<input type="hidden" name="next_page" value="Main_TrafficMonitor_devmonthly.asp">
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
								<p>Click on a host to monitor that host's current activity.
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
									<tr class='even'>
										<th width="40%"><#Date#></th>
										<td>
											<select class="input_option" style="width:130px" onchange='update_date_format(this, "ymd")' id='dafm'>
												<option value=0>yyyy-mm-dd</option>
												<option value=1>mm-dd-yyyy</option>
												<option value=2>mmm, dd, yyyy</option>
												<option value=3>dd.mm.yyyy</option>
											</select>
										</td>
									</tr>
									<tr class='even'>
										<th width="40%"><#Scale#></th>
										<td>
											<select style="width:70px" class="input_option" onchange='update_scale(this)' id='scale'>
												<option value=0>KB</option>
												<option value=1>MB</option>
												<option value=2 selected>GB</option>
											</select>
										</td>
									</tr>
									<tr>
										<th width="40%">Date range</th>
										<td>
											<div>
												<label>From:</label>
												<select name="_f_begin_date" onchange="redraw();" class="input_option" style="width:120px; vertical-align:middle; margin-right:3em;">
												</select>
												<label>To:</label>
												<select name="_f_end_date" onchange="redraw();" class="input_option" style="width:120px; vertical-align:middle;">
												</select>
											</div>
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
											<input type="radio" name="_f_show_zero" class="input" value="1" onclick="update_display('zero',1);"><#checkbox_Yes#>
											<input type="radio" name="_f_show_zero" class="input" value="0" checked onclick="update_display('zero',0);"><#checkbox_No#>
							   			</td>
									</tr>
									<tr id="adv4">
										<th>Show subnet totals</th>
						        		<td>
											<input type="radio" name="_f_show_subnet" class="input" value="1" onclick="update_display('subnet',1);"><#checkbox_Yes#>
											<input type="radio" name="_f_show_subnet" class="input" value="0" checked onclick="update_display('subnet',0);"><#checkbox_No#>
							   			</td>
									</tr>
								</tbody>
							</table>
						</td>
					</tr>
					<tr>
						<td>
							<div id='bwm-monthly-grid' style='float:left'></div>
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
