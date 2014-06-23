<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link href="/NM_style.css" rel="stylesheet" type="text/css" />
<link href="/form_style.css" rel="stylesheet" type="text/css" />
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/ajax.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<style type="text/css">
.title{
	font-size:16px;
	text-align:center;
	font-weight:bolder;
	margin-bottom:5px;
}

.ling_image{
	margin:5px 0px 0px 10px; 
	*margin-top:-10px;
}

.ram_table{
	height:30px;
	text-align:center;
}

.ram_table td{
	width:33%;

}

.loading_bar{
	width:200px;
}

.loading_bar > div{
	margin-left:20px;
	background-color:black;
	border-radius:15px;
	padding:3px;
}

.status_bar{
	height:12px;
	border-radius:15px;
}

#ram_bar{
	background-color:#0096FF;
}

#cpu1_bar{
	background-color:#FF9000;	
}

#cpu2_bar{
	background-color:#3CF;
}

.percentage_bar{
	width:75px;
	text-align:center;
}

.cpu_count{
	margin:0px 0px -5px 25px;
}

.cpu_desc{
	margin: 5px 0px -5px 25px;
}

.cpu_div{
	margin-top:-10px;
}
</style>
<script>
var $j = jQuery.noConflict();
/* To check core number  */
var core_num = 0;
var cpu_percentage = new Array();
<%cpu_usage();%>
core_num = cpu_percentage.length;
/*  End */
var ram_usage_array = new Array(31);
var cpu1_usage_array = new Array(46);
var cpu2_usage_array = new Array(46);
for(i=0;i<46;i++){
	cpu1_usage_array[i] = 101;
	cpu2_usage_array[i] = 101;
}
for(i=0;i<31;i++){
	ram_usage_array[i] = 101;
}

function initial(){
	if((parent.sw_mode == 2 || parent.sw_mode == 4) && '<% nvram_get("wlc_band"); %>' == '<% nvram_get("wl_unit"); %>')
		document.form.wl_subunit.value = 1;
	else
		document.form.wl_subunit.value = -1;
	
	if(band5g_support){
		$("t0").style.display = "";
		$("t1").style.display = "";
		
		// disallow to use the other band as a wireless AP
		if(parent.sw_mode == 4 && !localAP_support){
			$('t'+((parseInt(<% nvram_get("wlc_band"); %>+1))%2)).style.display = 'none';
		}
	}
	else{
		$("t0").style.display = "";
	}	
	
	if(core_num == 2){
		$('cpu2_tr').style.display = "";
		$('cpu2_graph').style.display = "";
	}

	detect_RAM_Status();
	detect_CPU_Status();
}

function tabclickhandler(wl_unit){
	if(wl_unit == 2){
		location.href = "router_status.asp";
	}
	else{
		if((parent.sw_mode == 2 || parent.sw_mode == 4) && '<% nvram_get("wlc_band"); %>' == wl_unit)
			document.form.wl_subunit.value = 1;
		else
			document.form.wl_subunit.value = -1;

		document.form.wl_unit.value = wl_unit;
		document.form.current_page.value = "device-map/router.asp";
		FormActions("/apply.cgi", "change_wl_unit", "", "");
		document.form.target = "";
		document.form.submit();
	}
}

function render_RAM(total, free, used){
	var pt = "";
	var used_percentage = 0;
	var total_MB = 0, free_MB = 0, used_MB =0;
	
	total_MB = Math.round(total/1024);
	free_MB = Math.round(free/1024);
	used_MB = Math.round(used/1024);
	
	$('ram_total_info').innerHTML = total_MB + "MB";
	$('ram_free_info').innerHTML = free_MB + "MB";
	$('ram_used_info').innerHTML = used_MB + "MB";
	
	used_percentage = Math.round((used/total)*100);
	$('ram_bar').style.width = used_percentage +"%";
	$('ram_bar').style.width = used_percentage +"%";
	$('ram_quantification').innerHTML = used_percentage +"%";
	
	ram_usage_array.push(100 - used_percentage);
	ram_usage_array.splice(0,1);
	
	for(i=0;i<31;i++){
		pt += i*9 +","+ ram_usage_array[i] + " ";
	}

	$('ram_graph').setAttribute('points', pt);
}

function render_CPU(cpu){
	var pt1 = "";
	var pt2 = "";
	
	$('cpu1_bar').style.width = cpu[0] +"%";
	$('cpu1_quantification').innerHTML = cpu[0] +"%";
	cpu1_usage_array.push(100 - cpu[0]);
	cpu1_usage_array.splice(0,1);
	for(i=0;i<46;i++){
		pt1 += i*6 +","+ cpu1_usage_array[i] + " ";	
	}
	$('cpu1_graph').setAttribute('points', pt1);
	
	if(core_num == 2){
		$('cpu2_bar').style.width = cpu[1] +"%";
		$('cpu2_quantification').innerHTML = cpu[1] +"%";
		cpu2_usage_array.push(100 - cpu[1]);
		cpu2_usage_array.splice(0,1);
		for(i=0;i<46;i++){
			pt2 += i*6 +","+ cpu2_usage_array[i] + " ";
		}
		$('cpu2_graph').setAttribute('points', pt2);
	}
}

function detect_RAM_Status(){
	$j.ajax({
    	url: '/ram_status.asp',
    	dataType: 'script',
    	error: function(xhr){
    		detect_RAM_Status();
    	},
    	success: function(){
			render_RAM(Total, Free, Used);			
			setTimeout("detect_RAM_Status();", 2000);
  		}
	});
}

function detect_CPU_Status(){
	$j.ajax({
    	url: '/cpu_status.asp',
    	dataType: 'script',
    	error: function(xhr){
    		detect_CPU_Status();
    	},
    	success: function(){
			render_CPU(cpu_percentage);		
			setTimeout("detect_CPU_Status();", 3000);
  		}
	});
}
</script>
</head>
<body class="statusbody" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply2.htm">
<input type="hidden" name="current_page" value="device-map/router_status.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wl_unit" value="<% nvram_get("wl_unit"); %>">
<input type="hidden" name="wl_subunit" value="-1">

<table border="0" cellpadding="0" cellspacing="0" style="padding-left:5px">
<tr>
	<td>		
		<table width="100px" border="0" align="left" style="margin-left:8px;" cellpadding="0" cellspacing="0">
			<td>
				<div id="t0" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(0)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">2.4GHz</span>
				</div>
			</td>
			<td>
				<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(1)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">5GHz</span>
				</div>
			</td>
			<td>
				<div id="t2" class="tabclick_NW" align="center" style="font-weight: bolder; margin-right:2px; width:90px;" onclick="tabclickhandler(2)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">Status</span>
				</div>
			</td>
		</table>
	</td>
</tr>

<tr>
	<td>
		<div>
		<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px" id="cpu">
			<tr>
				<td >
					<div class="title">CPU</div>
					<img class="ling_image" src="/images/New_ui/networkmap/linetwo2.png">
				</td>
			</tr >		
			<tr>
				<td>
					<div class="cpu_div">
						<table>
							<tr >
								<td colspan="2">
									<div class="cpu_desc">Core 1</div>
								</td>
							</tr>							
							<tr >
								<td>
									<div>
										<table>
											<tr>
												<td class="loading_bar" colspan="2">
													<div>									
														<div id="cpu1_bar" class="status_bar"></div>
													</div>
												</td>
												<td class="percentage_bar">
													<div id="cpu1_quantification">0%</div>
												</td>
											</tr>
										</table>
									</div>
								</td>
							</tr> 
						</table>
					</div>
				</td>
			</tr>
			<tr id="cpu2_tr" style="display:none;">
				<td>
					<div class="cpu_div">
						<table>
							<tr >
								<td colspan="2">
									<div class="cpu_desc">Core 2</div>
								</td>
							</tr>							
							<tr >
								<td>
									<div>
										<table>
											<tr>
												<td class="loading_bar" colspan="2">
													<div>									
														<div id="cpu2_bar" class="status_bar"></div>					
													</div>
												</td>
												<td class="percentage_bar">
													<div id="cpu2_quantification">0%</div>
												</td>
											</tr>
										</table>
									</div>
								</td>
							</tr> 
						</table>
					</div>
				</td>
			</tr>
			
			<tr style="height:100px;">
			<td colspan="3">
				<div style="margin:0px 11px 0px 11px;background-color:black;">
					<svg width="270px" height="100px">
						<g>
							<line stroke-width="1" stroke-opacity="1"   stroke="rgb(255,255,255)" x1="0" y1="0%"   x2="100%" y2="0%" />
							<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="25%"  x2="100%" y2="25%" />
							<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="50%"  x2="100%" y2="50%" />
							<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="75%"  x2="100%" y2="75%" />
							<line stroke-width="1" stroke-opacity="1"   stroke="rgb(255,255,255)" x1="0" y1="100%" x2="100%" y2="100%" />
						</g>
						
						<g>
							<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="98%">0%</text>
							<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="55%">50%</text>
							<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="11%">100%</text>
						</g>
						
						<line stroke-width="1" stroke-opacity="1"   stroke="rgb(0,0,121)"   x1="0"   y1="0%" x2="0"   y2="100%" id="tick1" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="30"  y1="0%" x2="30"  y2="100%" id="tick2" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="60"  y1="0%" x2="60"  y2="100%" id="tick3" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="90"  y1="0%" x2="90"  y2="100%" id="tick4" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="120" y1="0%" x2="120" y2="100%" id="tick5" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="150" y1="0%" x2="150" y2="100%" id="tick6" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="180" y1="0%" x2="180" y2="100%" id="tick7" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="210" y1="0%" x2="210" y2="100%" id="tick8" />
						<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="240" y1="0%" x2="240" y2="100%" id="tick9" />						
						<line stroke-width="1" stroke-opacity="1"   stroke="rgb(0,0,121)"   x1="270" y1="0%" x2="270" y2="100%" id="tick10" />

						<polyline id="cpu1_graph" style="fill:none;stroke:#FF9000;stroke-width:1;width:200px;"  points=""></polyline>
						<polyline id="cpu2_graph" style="fill:none;stroke:#3CF;stroke-width:1;width:200px;display:none;"  points=""></polyline>
					</svg>
				</div>
			</td>
		</tr>
			
			<tr>
				<td style="border-bottom:3px #15191b solid;padding:0px 10px 5px 10px;"></td>
			</tr>
 		</table>
		</div>
  	</td>
</tr>

<tr>
	<td> 
		<div>
			<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px">	
			<tr>
				<td colspan="3">		
					<div class="title">RAM</div>
					<img class="ling_image" src="/images/New_ui/networkmap/linetwo2.png">
				</td>
			</tr>
			<tr class="ram_table">
				<td>
					<div>Used</div>	  			
					<div id="ram_used_info"></div>	
				</td>
				<td>
					<div>Free</div>
					<div id="ram_free_info"></div>	  			
				</td>
				<td>
					<div>Total</div>	  			
					<div id="ram_total_info"></div>	  			
				</td>
			</tr>  
			<tr>
				<td colspan="3">
					<div>
						<table>
							<tr>
								<td class="loading_bar" colspan="2">
									<div>
										<div id="ram_bar" class="status_bar"></div>				
									</div>
								</td>
								<td class="percentage_bar">
									<div id="ram_quantification">0%</div>
								</td>
							</tr>
						</table>
					</div>
				</td>
			</tr>
			<tr style="height:100px;">
				<td colspan="3">
					<div style="margin:0px 11px 0px 11px;background-color:black;">
						<svg width="270px" height="100px">
							<g>
								<line stroke-width="1" stroke-opacity="1" stroke="rgb(255,255,255)" x1="0" y1="0%" x2="100%" y2="0%" />
								<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="25%" x2="100%" y2="25%" />
								<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="50%" x2="100%" y2="50%" />
								<line stroke-width="1" stroke-opacity="0.2" stroke="rgb(255,255,255)" x1="0" y1="75%" x2="100%" y2="75%" />
								<line stroke-width="1" stroke-opacity="1" stroke="rgb(255,255,255)" x1="0" y1="100%" x2="100%" y2="100%" />
							</g>	

							<g>
								<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="98%">0%</text>
								<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="55%">50%</text>
								<text font-family="Verdana" fill="#FFFFFF" font-size="8" x="0" y="11%">100%</text>
							</g>						

							<line stroke-width="1" stroke-opacity="1"   stroke="rgb(0,0,121)"   x1="0"   y1="0%" x2="0"   y2="100%" id="tick1" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="30"  y1="0%" x2="30"  y2="100%" id="tick2" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="60"  y1="0%" x2="60"  y2="100%" id="tick3" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="90"  y1="0%" x2="90"  y2="100%" id="tick4" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="120" y1="0%" x2="120" y2="100%" id="tick5" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="150" y1="0%" x2="150" y2="100%" id="tick6" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="180" y1="0%" x2="180" y2="100%" id="tick7" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="210" y1="0%" x2="210" y2="100%" id="tick8" />
							<line stroke-width="1" stroke-opacity="0.3" stroke="rgb(40,255,40)" x1="240" y1="0%" x2="240" y2="100%" id="tick9" />						
							<line stroke-width="1" stroke-opacity="1"   stroke="rgb(0,0,121)"   x1="270" y1="0%" x2="270" y2="100%" id="tick10" />
							
							<polyline id="ram_graph" style="fill:none;stroke:#0096FF;stroke-width:1;width:200px;"  points="">
						</svg>
					</div>
				</td>
			</tr>
			
			</table>
		</div>
	</td>
</tr>
</table>			
</form>
</body>
</html>
