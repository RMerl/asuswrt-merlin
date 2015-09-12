<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link href="../form_style.css" rel="stylesheet" type="text/css" />
<link href="../NM_style.css" rel="stylesheet" type="text/css" />
<style>
a:link {
	text-decoration: underline;
	color: #FFFFFF;
}
a:visited {
	text-decoration: underline;
	color: #FFFFFF;
}
a:hover {
	text-decoration: underline;
	color: #FFFFFF;
}
a:active {
	text-decoration: none;
	color: #FFFFFF;
}
#updateProgress_bg{
	margin-top:5px;
	height:30px;
	width:97%;
	background:url(/images/quotabar_bg_progress.gif);
	background-repeat: repeat-x;
}
.font_style{
	font-family:Verdana,Arial,Helvetica,sans-serif;
}
</style>
<script type="text/javascript" src="/require/require.min.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

var diskOrder = parent.getSelectedDiskOrder();
var diskmon_status = '<% nvram_get("diskmon_status"); %>';
var diskmon_usbport = '<% nvram_get("diskmon_usbport"); %>';
var usb_path1_diskmon_freq = '<% nvram_get("usb_path1_diskmon_freq"); %>';
var usb_path1_diskmon_freq_time = '<% nvram_get("usb_path1_diskmon_freq_time"); %>';
var usb_path2_diskmon_freq = '<% nvram_get("usb_path2_diskmon_freq"); %>';
var usb_path2_diskmon_freq_time = '<% nvram_get("usb_path2_diskmon_freq_time"); %>';

var set_diskmon_time = "";
var progressBar;
var timer;
var diskmon_freq_row;

var stopScan = 0;
var scan_done = 0;

function initial(){
	document.getElementById("t0").className = "tab_NW";
	document.getElementById("t1").className = "tabclick_NW";

	load_schedule_value();
	freq_change();
	check_status(parent.usbPorts[diskOrder-1]);
}

function load_schedule_value(){
	if(diskmon_usbport != parent.usbPorts[diskOrder-1].node){
		document.usbUnit_form.diskmon_usbport.value = parent.usbPorts[diskOrder-1].node;
		document.usbUnit_form.submit();
	}

	if(parseInt(parent.usbPorts[diskOrder-1].usbPath) == 1){
		document.form.diskmon_freq.value = usb_path1_diskmon_freq;
		diskmon_freq_row = usb_path1_diskmon_freq_time.split('&#62');
	}
	else{
		document.form.diskmon_freq.value = usb_path2_diskmon_freq;
		diskmon_freq_row = usb_path2_diskmon_freq_time.split('&#62');
	}

	for(var i=0; i<3; i++){
		if(diskmon_freq_row[i] == "" || typeof(diskmon_freq_row[i]) == "undefined")
			diskmon_freq_row[i] = "1";
	}

	document.form.freq_mon.value = diskmon_freq_row[0];
	document.form.freq_week.value = diskmon_freq_row[1];
	document.form.freq_hour.value = diskmon_freq_row[2];
}

function freq_change(){
	if(document.form.diskmon_freq.value == 0){
		document.getElementById('date_field').style.display="none";
		document.getElementById('week_field').style.display="none";
		document.getElementById('time_field').style.display="none";
		document.getElementById('schedule_date').style.display="none";
		document.getElementById('schedule_week').style.display="none";
		document.getElementById('schedule_time').style.display="none";		
		document.getElementById('schedule_frequency').style.display="none";	
		document.getElementById('schedule_desc').style.display="none";			
	}
	else if(document.form.diskmon_freq.value == 1){
		document.getElementById('date_field').style.display="";	
		document.getElementById('week_field').style.display="none";
		document.getElementById('time_field').style.display="";
		document.getElementById('schedule_date').style.display="";
		document.getElementById('schedule_week').style.display="none";
		document.getElementById('schedule_time').style.display="";		
		document.getElementById('schedule_frequency').style.display="";		
		document.getElementById('schedule_desc').style.display="";
	}
	else if(document.form.diskmon_freq.value == 2){
		document.getElementById('date_field').style.display="none";
		document.getElementById('week_field').style.display="";
		document.getElementById('time_field').style.display="";
		document.getElementById('schedule_date').style.display="none";
		document.getElementById('schedule_week').style.display="";
		document.getElementById('schedule_time').style.display="";		
		document.getElementById('schedule_frequency').style.display="";	
		document.getElementById('schedule_desc').style.display="";	
	}
	else{
		document.getElementById('date_field').style.display="none";
		document.getElementById('week_field').style.display="none";
		document.getElementById('time_field').style.display="";
		document.getElementById('schedule_date').style.display="none";
		document.getElementById('schedule_week').style.display="none";
		document.getElementById('schedule_time').style.display="";		
		document.getElementById('schedule_frequency').style.display="";	
		document.getElementById('schedule_desc').style.display="";	
	}
	show_schedule_desc();
}

function show_schedule_desc(){
	var array_week = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
	var array_freq = ["", "Monthly", "Weekly", "Daily"];
	
	if(document.form.freq_mon.value == 1 || document.form.freq_mon.value == 21 || document.form.freq_mon.value == 31)
		document.getElementById('schedule_date').innerHTML = document.form.freq_mon.value + "st";
	else if(document.form.freq_mon.value == 2 || document.form.freq_mon.value == 22)
		document.getElementById('schedule_date').innerHTML = document.form.freq_mon.value + "nd";
	else if(document.form.freq_mon.value == 3 || document.form.freq_mon.value == 23)
		document.getElementById('schedule_date').innerHTML = document.form.freq_mon.value + "rd";
	else
		document.getElementById('schedule_date').innerHTML = document.form.freq_mon.value + "th";
		
	document.getElementById('schedule_week').innerHTML = " " + array_week[document.form.freq_week.value];
	document.getElementById('schedule_time').innerHTML = document.form.freq_hour.value + ":00";
	document.getElementById('schedule_frequency').innerHTML = array_freq[document.form.diskmon_freq.value];
}

function apply_schedule(){
	if(progressBar >= 1 && progressBar <= 100 ){
		alert("<#diskUtility_scanning#>");
		return false;
	}
	else{
		set_diskmon_time = document.form.freq_mon.value+">"+document.form.freq_week.value+">"+document.form.freq_hour.value;
		document.form.diskmon_freq_time.value = set_diskmon_time;
		document.form.diskmon_force_stop.disabled = true;
		document.getElementById('loadingIcon_apply').style.display = "";
		document.form.action_script.value = "restart_diskmon";
		document.form.submit();
	}
}

function stop_diskmon(){
	document.form.diskmon_freq.disabled = false;
	document.form.diskmon_freq_time.disabled = true;
	//document.form.diskmon_policy.disabled = true;
	document.form.diskmon_part.disabled = true;
	document.form.diskmon_force_stop.disabled = false;
	document.form.diskmon_force_stop.value = "1";
	document.form.submit();
}

function go_scan(){
	stopScan = 0;
	if(!confirm("<#diskUtility_scan_hint#>")){
		document.getElementById('scan_status_field').style.display = "";
		document.getElementById('progressBar').style.display = "none";
		return false;
	}

	document.getElementById('loadingIcon').style.display = "";
	document.getElementById('btn_scan').style.display = "none";
	document.getElementById('btn_abort').style.display = "";
	document.getElementById('progressBar').style.display = "";	
	document.getElementById('scan_status_field').style.display = "none";	

	scan_manually();
}

function show_loadingBar_field(){
	document.getElementById('loadingIcon').style.display = "none";
	showLoadingUpdate();
	progressBar = 1;
	
	parent.document.getElementById('ring_USBdisk_'+diskOrder).style.display = "";
	parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/backgroud_move_8P_2.0.gif)";
	parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 2%';
}

function showLoadingUpdate(){
	$.ajax({
    	url: '/disk_scan.asp',
    	dataType: 'script',
    	error: function(xhr){
    		showLoadingUpdate();
    	},
    	success: function(){
				if(stopScan == 0)
					document.getElementById("updateProgress").style.width = progressBar+"%";
					
				if( scan_status == 1 && stopScan == 0){	// To control message of scanning status
					if(progressBar >= 5)
						progressBar =5;
						
					document.getElementById('scan_message').innerHTML = "<#diskUtility_initial#>";
				}	
				else if(scan_status == 2 && stopScan == 0){
					if(progressBar <= 5)
						progressBar = 6;
					else if (progressBar >= 15)	
						progressBar = 15;
				
					document.getElementById('scan_message').innerHTML = "<#diskUtility_umount#>";								
				}
				else if(scan_status == 3 && stopScan == 0){
					if(progressBar <= 15)
						progressBar = 16;
					else if (progressBar >= 40)	
						progressBar = 40;
						
					document.getElementById('scan_message').innerHTML = "Disk scanning ...";					
				}	
				else if(scan_status == 4 && stopScan == 0){
					if(progressBar <= 40)
						progressBar = 41;
					else if (progressBar >= 90)	
						progressBar = 90;
						
					document.getElementById('scan_message').innerHTML = "<#diskUtility_reMount#>";
				}
				else if(scan_status == 5 && stopScan == 0){
					if(progressBar <= 90)
						progressBar = 91;
				
					document.getElementById('scan_message').innerHTML = "<#diskUtility_finish#>";				
				}
				else{
					document.getElementById('scan_message').innerHTML = "Stop disk scanning force...";	
				}
			if(progressBar > 100){
				scan_done = 1;
				document.getElementById('btn_scan').style.display = "";
				document.getElementById('btn_abort').style.display = "none";
				document.getElementById('scan_status_field').style.display = "";
				document.getElementById('progressBar').style.display = "none";
				disk_scan_status();
				document.form.diskmon_freq.disabled = false;
				document.form.diskmon_freq_time.disabled = false;
				//document.form.diskmon_policy.disabled = false;
				document.form.diskmon_part.disabled = false;
				document.form.diskmon_force_stop.disabled = false;
				return false;
			}
			if(stopScan == 0){
				document.getElementById('progress_bar_no').innerHTML = progressBar+"%";
				progressBar++;
			}		
			timer = setTimeout("showLoadingUpdate();", 100);		
		}
	});
	
}

function abort_scan(){
	var progress_stop = 0;
	stopScan = 1;
	progress_stop = progressBar; // get progress value when press abort
	clearTimeout(timer);
	document.getElementById('scan_message').innerHTML = "<#diskUtility_stop#>";
	document.getElementById('progress_bar_no').innerHTML = progress_stop + "%";	
	document.getElementById("updateProgress").style.width = progress_stop +"%";
	document.getElementById('loadingIcon').style.display = "";
	document.getElementById('btn_scan').style.display = "";
	document.getElementById('btn_abort').style.display = "none";
	document.getElementById('loadingIcon').style.display = "none";	
	document.getElementById('progressBar').style.display  = "none";
	stop_diskmon();
	reset_force_stop();
	setTimeout('disk_scan_status("")',1000);
}

function disk_scan_status(){
	require(['/require/modules/diskList.js'], function(diskList){
	 	diskList.update(function(){
	 		$.each(parent.usbPorts, function(i, curPort){
		 		$.each(diskList.list(), function(j, usbDevice){
	 				if(curPort.node == usbDevice.node)
	 					parent.usbPorts[i] = usbDevice;
		 		});
	 		});

			check_status(parent.usbPorts[diskOrder-1]);		
		})
	});
}

function get_disk_log(){
	$.ajax({
		url: '/disk_fsck.xml?diskmon_usbport=' + parent.usbPorts[diskOrder-1].node,
		dataType: 'xml',
		error: function(xhr){
			alert("Fail to get the log of fsck!");
		},
		success: function(xml){
			$('#textarea_disk0').html($(xml).find('disk1').text());
		}
	});

	document.getElementById("textarea_disk0").style.display = "";
}

function check_status(_device){
	var diskOrder = _device.usbPath;

	document.getElementById('scan_status_field').style.display = "";
	parent.document.getElementById('ring_USBdisk_'+diskOrder).style.display = "";
	parent.document.getElementById('iconUSBdisk_'+diskOrder).style.marginLeft = "35px";

	var i, j;
	var got_code_0, got_code_1, got_code_2, got_code_3;
	for(i = 0; i < _device.partition.length; ++i){
		switch(parseInt(_device.partition[i].fsck)){
			case 0: // no error.
				got_code_0 = 1;
				break;
			case 1: // find errors.
				got_code_1 = 1;
				break;
			case 2: // proceeding...
				got_code_2 = 1;
				break;
			default: // don't or can't support.
				got_code_3 = 1;
				break;
		}
	}

	if(got_code_1){
		document.getElementById('disk_init_status').style.display = "none";
		document.getElementById('problem_found').style.display = "none";
		document.getElementById('crash_found').style.display = "";
		document.getElementById('scan_status_image').src = "/images/New_ui/networkmap/red.png";

		if(stopScan == 1 || scan_done == 1){
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 99%';
		}
		parent.document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0% -202px';
	}
	else if(got_code_2){
		if(stopScan == 1){
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 0%';
		}
	}
	else if(got_code_3){
		parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";
		parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 0%';
	}
	else{ // got_code_0
		document.getElementById('disk_init_status').style.display = "none";
		document.getElementById('problem_found').style.display = "";
		document.getElementById('crash_found').style.display = "none";
		document.getElementById('scan_status_image').src = "/images/New_ui/networkmap/blue.png";
		if(stopScan == 1 || scan_done == 1){
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundImage = "url(/images/New_ui/networkmap/white_04.gif)";
			parent.document.getElementById('ring_USBdisk_'+diskOrder).style.backgroundPosition = '0% 50%';
		}
		parent.document.getElementById('iconUSBdisk_'+diskOrder).style.backgroundPosition = '0px -103px';
	}

	get_disk_log();
}

function scan_manually(){
	document.form.diskmon_freq.disabled = true;
	document.form.diskmon_freq_time.disabled = true;
	document.form.diskmon_force_stop.disabled = true;
	document.form.action_script.value = "start_diskscan";
	document.form.submit();
}

function reset_force_stop(){
	document.form.diskmon_freq_time.disabled = false;
	document.form.diskmon_part.disabled = false;
	document.form.diskmon_force_stop.disabled = true;
	document.form.diskmon_force_stop.value = "0";
	document.form.submit();

}
</script>
</head>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<body class="statusbody" onload="initial();">
<form method="post" name="usbUnit_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="action_mode" value="change_diskmon_unit">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="diskmon_usbport" value="">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="next_page" value="">
</form>
<form name="form" method="post" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="next_page" value="/device-map/disk_utility.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_diskmon">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="diskmon_force_stop" value="<% nvram_get("diskmon_force_stop"); %>" >
<input type="hidden" name="diskmon_freq_time" value="<% nvram_get("diskmon_freq_time"); %>">
<input type="hidden" name="diskmon_policy" value="disk">
<input type="hidden" name="diskmon_part" value="">
<table height="30px;">
	<tr>
		<td>		
			<table width="100px" border="0" align="left" style="margin-left:5px;" cellpadding="0" cellspacing="0">
			<td>
					<div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder;margin-right:2px;" onclick="location.href='disk.asp'">
						<span style="cursor:pointer;font-weight: bolder;"><#diskUtility_information#></span>
					</div>
				</td>
			<td>
					<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;margin-right:2px;" onclick="location.href='disk_utility.asp'">
						<span style="cursor:pointer;font-weight: bolder;"><#diskUtility#></span>
					</div>
				</td>
			</table>
		</td>
	</tr>
</table>

<table  width="313px;"  align="center"  style="margin-top:-8px;margin-left:3px;" cellspacing="5">
  <tr >
    <td style="background-color:#4D595D">
		<div id="scan_status_field" style="margin-top:10px;">
			<table>
				<tr class="font_style">
					<td width="40px;" align="center">
						<img id="scan_status_image" src="/images/New_ui/networkmap/normal.png">
					</td>
					<td id="disk_init_status" >
						<#diskUtility_init_status#>
					</td>
					<td id="problem_found" style="display:none;">
						<#diskUtility_problem_found#>
					</td>
					<td id="crash_found" style="display:none;">
						<#diskUtility_crash_found#>
					</td>
				</tr>
			</table>	
		</div>
		<div id="progressBar" style="margin-left:9px;;margin-top:10px;display:none">
			<div id="scan_message"></div>
			<div id="updateProgress_bg">
				<div>
					<span id="progress_bar_no" style="position:absolute;margin-left:130px;margin-top:7px;" ></span>
					<img id="updateProgress" src="/images/quotabar.gif" height="30px;" style="width:0%">
					
				</div>
			</div>
		</div>
		<img style="margin-top:5px;margin-left:9px; *margin-top:-10px; width:283px;" src="/images/New_ui/networkmap/linetwo2.png">
		<div class="font_style" style="margin-left:10px;margin-bottom:5px;margin-top:10px;"><#diskUtility_detailInfo#></div>
		<div >
			<table border="0" width="98%" align="center" height="100px;"><tr>
				<td style="vertical-align:top" height="100px;">
					<span id="log_field" >
						<textarea cols="15" rows="13" readonly="readonly" id="textarea_disk0" style="resize:none;display:none;width:98%; font-family:'Courier New', Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"></textarea>
					</span>
				</td>
			</tr></table>
		</div>
		<div style="margin-top:20px;margin-bottom:10px;"align="center">
			<input id="btn_scan" type="button" class="button_gen" onclick="go_scan();" value="<#QIS_rescan#>">
			<input id="btn_abort" type="button" class="button_gen" onclick="abort_scan();" value="Abort" style="display:none">
			<img id="loadingIcon" style="display:none;margin-right:10px;" src="/images/InternetScan.gif">
		</div>
    </td>
  </tr>

  <tr>
    <td style="background-color:#4D595D;" >
		<div class="font_style" style="margin-left:12px;margin-top:10px;"><#diskUtility_schedule#></div>
		<img style="margin-top:5px;margin-left:10px; *margin-top:-5px;" src="/images/New_ui/networkmap/linetwo2.png">
			<div style="margin-left:10px;">
				<table>
					<tr class="font_style">
						<td style="width:100px;">
							<div style="margin-bottom:5px;" ><#diskUtility_frenqucy#></div>
							<select name="diskmon_freq" onchange="freq_change();" class="input_option">
								<option value="0" <% nvram_match("diskmon_freq", "0", "selected"); %>><#btn_disable#></option>
								<option value="1" <% nvram_match("diskmon_freq", "1", "selected"); %>><#diskUtility_monthly#></option>
								<option value="2" <% nvram_match("diskmon_freq", "2", "selected"); %>><#diskUtility_weekly#></option>
								<option value="3" <% nvram_match("diskmon_freq", "3", "selected"); %>><#diskUtility_daily#></option>							
							</select>
						</td>							
						<td >
							<div id="date_field">
								<div style="margin-bottom:5px;">Date</div>
								<select name="freq_mon" class="input_option" onchange="freq_change();">
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
							</div>
						</td>
						<td>
							<div id="week_field">
								<div style="margin-bottom:5px"><#diskUtility_week#></div>
								<select name="freq_week" class="input_option" onchange="freq_change();">
									<option value="0">Sun</option>
									<option value="1">Mon</option>
									<option value="2">Tue</option>
									<option value="3">Wed</option>
									<option value="4">Thu</option>
									<option value="5">Fri</option>
									<option value="6">Sat</option>
								</select>
							</div>
						</td>
						<td>
							<div id="time_field">
								<div style="margin-bottom:5px;"><#diskUtility_time#></div>
								<select name="freq_hour" class="input_option" onchange="freq_change();">
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
							</div>
						</td>
					</tr>
				</table>
			</div>
				<img style="margin-top:5px;margin-left:10px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
				<div id="schedule_desc">
					<div  class="font_style" style="margin-top:5px;margin-left:13px;margin-right:10px;" >
						<#diskUtility_dchedule_hint#> 
						<span id="schedule_time" style="display:none;font-weight:bolder;"></span>&nbsp
						on&nbsp<span id="schedule_week" style="display:none;font-weight:bolder;"></span>
						<span id="schedule_date" style="display:none;font-weight:bolder;"></span>&nbsp
						<span id="schedule_frequency" style="display:none;font-weight:bolder;"></span>									
					</div>
					<div class="font_style" style="margin-top:5px;margin-left:13px;margin-right:10px;">
						<#diskUtility_scanDuring#>
					</div>
				</div>
			<div style="margin-top:20px;margin-bottom:10px;" align="center">
				<input type="button" class="button_gen" onclick="apply_schedule();" value="<#CTL_apply#>">
				<img id="loadingIcon_apply" style="display:none;margin-right:10px;" src="/images/InternetScan.gif">
			</div>
    </td>
  </tr>
 
</table>
</form>
</body>
</html>
