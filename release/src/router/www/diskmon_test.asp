<html>
<head>
<style>
.string{
	font-family: Arial, Helvetica, sans-serif;
	color:#118888;
	font-size:24px;
}

.hint{
	font-family: Arial, Helvetica, sans-serif;
	color:#aa0000;
	font-size:18px;
}
</style>

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script>

var diskmon_status = '<% nvram_get("diskmon_status"); %>';
var diskmon_freq_time = '<% nvram_get("diskmon_freq_time"); %>';
var set_diskmon_time = "";


function initial(){
	var diskmon_freq_row = diskmon_freq_time.split('&#62');

	document.form.freq_mon.value = diskmon_freq_row[0];
	document.form.freq_week.value = diskmon_freq_row[1];
	document.form.freq_hour.value = diskmon_freq_row[2];

	if(diskmon_status == "1")
		showtext(document.getElementById("mon_status"), "Start");
	else if(diskmon_status == "2")
		showtext(document.getElementById("mon_status"), "Umount");
	else if(diskmon_status == "3")
		showtext(document.getElementById("mon_status"), "Scan");
	else if(diskmon_status == "4")
		showtext(document.getElementById("mon_status"), "Re-Mount");
	else if(diskmon_status == "5")
		showtext(document.getElementById("mon_status"), "Finish");
	else if(diskmon_status == "6")
		showtext(document.getElementById("mon_status"), "Stop forcely");
	else
		showtext(document.getElementById("mon_status"), "Idle");

	gen_port_option();

	freq_change();
	policy_change();
}

function gen_port_option(){
	var diskmon_usbport = '<% nvram_get("diskmon_usbport"); %>';
	var Beselected = 0;

	free_options(document.form.diskmon_usbport);

 	require(['/require/modules/diskList.js'], function(diskList){
 		var usbDevicesList = diskList.list();
		for(var i = 0; i < usbDevicesList.length; ++i){
			if(usbDevicesList[i].node == diskmon_usbport) 
				Beselected = 1;
			else
				Beselected = 0;
			add_option(document.form.diskmon_usbport, usbDevicesList[i].deviceName, usbDevicesList[i].node, Beselected);
		}
	});
	
	gen_part_option();
}

function gen_part_option(){
	var diskmon_part = '<% nvram_get("diskmon_part"); %>';
	var Beselected = 0;
	var disk_port = document.form.diskmon_usbport.value;
	var disk_num = -1;

	free_options(document.form.diskmon_part);

 	require(['/require/modules/diskList.js'], function(diskList){
 		var usbDevicesList = diskList.list();
		for(var i = 0; i < usbDevicesList.length; ++i){
			if(usbDevicesList[i].node == disk_port){
				disk_num = i;
				break;
			}
		}
	});

	if(disk_num == -1){
		alert("System Error!");
		return;
	}

	//Scan all usb device
 	require(['/require/modules/diskList.js'], function(diskList){
 		var usbDevicesList = diskList.list();
		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				if(usbDevicesList[i].partition[j].mountPoint == diskmon_part)
					Beselected = 1;
				else
					Beselected = 0;

				if(i == disk_num){
					if(usbDevicesList[i].partition[j].size > 0)
						add_option(document.form.diskmon_part, usbDevicesList[i].partition[j].partName, usbDevicesList[i].partition[j].mountPoint, Beselected);
				}
			}
		}
	});
}

function freq_change(){
	if(document.form.diskmon_freq.value == "1"){
		document.getElementById("mon_day").style.display = "";
		document.getElementById("week_day").style.display = "none";
	}
	else if(document.form.diskmon_freq.value == "2"){
		document.getElementById("mon_day").style.display = "none";
		document.getElementById("week_day").style.display = "";
	}
	else if(document.form.diskmon_freq.value == "3"){
		document.getElementById("mon_day").style.display = "none";
		document.getElementById("week_day").style.display = "none";
	}
	else{
		document.getElementById("mon_day").style.display = "";
		document.getElementById("week_day").style.display = "";
	}
}

function policy_change(){
	document.getElementById("mon_port").style.display = "";

	if(document.form.diskmon_policy.value == "disk"){
		document.getElementById("mon_port").style.display = "";
		document.getElementById("mon_part").style.display = "none";
	}
	else if(document.form.diskmon_policy.value == "part"){
		document.getElementById("mon_port").style.display = "";
		document.getElementById("mon_part").style.display = "";
	}
	else{
		document.getElementById("mon_port").style.display = "none";
		document.getElementById("mon_part").style.display = "none";
	}
}

function set_time(){
	set_diskmon_time = document.form.freq_mon.value+">"+document.form.freq_week.value+">"+document.form.freq_hour.value;
	document.form.diskmon_freq_time.value = set_diskmon_time;

	document.form.diskmon_force_stop.disabled = true;
}

function stop_diskmon(){
	document.form.diskmon_freq.disabled = true;
	document.form.diskmon_freq_time.disabled = true;
	document.form.diskmon_policy.disabled = true;
	document.form.diskmon_usbport.disabled = true;
	document.form.diskmon_part.disabled = true;

	document.form.diskmon_force_stop.value = "1";

	document.form.submit();
}

function scan_manually(){
	document.form.diskmon_freq.disabled = true;
	document.form.diskmon_freq_time.disabled = true;
	document.form.diskmon_policy.disabled = true;
	document.form.diskmon_usbport.disabled = true;
	document.form.diskmon_part.disabled = true;
	document.form.diskmon_force_stop.disabled = true;

	document.form.action_script.value = "start_diskscan";

	document.form.submit();
}

function done_committing(){}
function restart_needed_time(reload_sec){
	setTimeout("location.href = location.href;", reload_sec*1000);
}
</script>
</head>

<body onload="initial();">
<h2>Disk Monitor demo:</h2>
<form name="form" method="post" action="/diskmon_test.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_diskmon">
<input type="hidden" name="action_wait" value="1">

<div class="string">
	diskmon_status: 
	<span id="mon_status"></span>
</div>

<div class="string">
	diskmon_freq: 
	<select name="diskmon_freq" onchange="freq_change();" class="input_option">
		<option value="0" <% nvram_match("diskmon_freq", "0", "selected"); %>>Disable</option>
		<option value="1" <% nvram_match("diskmon_freq", "1", "selected"); %>>Month</option>
		<option value="2" <% nvram_match("diskmon_freq", "2", "selected"); %>>Week</option>
		<option value="3" <% nvram_match("diskmon_freq", "3", "selected"); %>>Day</option>
	</select>
	<input type="hidden" name="diskmon_freq_time" value="<% nvram_get("diskmon_freq_time"); %>">
</div>
<table border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
	<tr>
		<td>
			<span class="string">diskmon_freq_time: </span>
		</td>
		<td> </td>
	</tr>
	<tr id="mon_day">
		<td> </td>
		<td>
			<span class="string">Date: </span>
			<select name="freq_mon" class="input_option">
				<option value="1">1</option>
				<option value="2">2</option>
				<option value="3">3</option>
				<option value="4">4</option>
				<option value="5">5</option>
				<option value="6">6</option>
				<option value="7">7</option>
				<option value="8">8</option>
				<option value="9">9</option>
				<option value="10">10</option>
				<option value="11">11</option>
				<option value="12">12</option>
				<option value="13">13</option>
				<option value="14">14</option>
				<option value="15">15</option>
				<option value="16">16</option>
				<option value="17">17</option>
				<option value="18">18</option>
				<option value="19">19</option>
				<option value="20">20</option>
				<option value="21">21</option>
				<option value="22">22</option>
				<option value="23">23</option>
				<option value="24">24</option>
				<option value="25">25</option>
				<option value="26">26</option>
				<option value="27">27</option>
				<option value="28">28</option>
				<option value="29">29</option>
				<option value="30">30</option>
				<option value="31">31</option>
			</select>
		</td>
	</tr>
	<tr id="week_day">
		<td> </td>
		<td>
			<span class="string">Week: </span>
			<select name="freq_week" class="input_option">
				<option value="0">Sun</option>
				<option value="1">Mon</option>
				<option value="2">Tue</option>
				<option value="3">Wed</option>
				<option value="4">Thu</option>
				<option value="5">Fri</option>
				<option value="6">Sat</option>
			</select>
		</td>
	</tr>
	<tr>
		<td> </td>
		<td>
			<span class="string">Hour: </span>
			<select name="freq_hour" class="input_option">
				<option value="0">0</option>
				<option value="1">1</option>
				<option value="2">2</option>
				<option value="3">3</option>
				<option value="4">4</option>
				<option value="5">5</option>
				<option value="6">6</option>
				<option value="7">7</option>
				<option value="8">8</option>
				<option value="9">9</option>
				<option value="10">10</option>
				<option value="11">11</option>
				<option value="12">12</option>
				<option value="13">13</option>
				<option value="14">14</option>
				<option value="15">15</option>
				<option value="16">16</option>
				<option value="17">17</option>
				<option value="18">18</option>
				<option value="19">19</option>
				<option value="20">20</option>
				<option value="21">21</option>
				<option value="22">22</option>
				<option value="23">23</option>
			</select>
		</td>
	</tr>
</table>
<table>
	<tr>
		<td>
			<span class="string">diskmon_policy: </span>
		</td>
		<td>
			<select name="diskmon_policy" onchange="policy_change();" class="input_option">
				<option value="all" <% nvram_match("diskmon_policy", "all", "selected"); %>>All</option>
				<option value="disk" <% nvram_match("diskmon_policy", "disk", "selected"); %>>by Disk</option>
				<option value="part" <% nvram_match("diskmon_policy", "part", "selected"); %>>by Partition</option>
			</select>
		</td>
	</tr>
	<tr>
		<td> </td>
		<td>
			<span id="mon_port" class="string">
				Disk: 
				<select name="diskmon_usbport" onchange="gen_part_option();" class="input_option"></select>
			</span>
		</td>
	</tr>
	<tr>
		<td> </td>
		<td>
			<span id="mon_part" class="string">
				Partition: 
				<select name="diskmon_part" class="input_option"></select>
			</span>
		</td>
	</tr>
</table>

<input type="button" name="force_stop" value="Stop forcely!" onclick="stop_diskmon();">
<input type="button" name="manually_scan" value="Scan manually!" onclick="scan_manually();">
<input type="hidden" name="diskmon_force_stop" value="<% nvram_get("diskmon_force_stop"); %>">
<input type="submit" onclick="set_time();">
</form>

<% update_variables(); %>
<% convert_asus_variables(); %>
<% asus_nvram_commit(); %>

<div style="margin-top:8px">
	<textarea cols="63" rows="27" wrap="off" readonly="readonly" id="textarea" style="width:99%; font-family:'Courier New', Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% apps_fsck_log(); %></textarea>
</div>
</body>
</html>
