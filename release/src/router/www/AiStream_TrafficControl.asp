<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
  $j(function() {
    $j( "#sortable" ).sortable();
    $j( "#sortable" ).disableSelection();
	$j("#priority_block div").draggable({helper:"clone",revert:true,revertDuration:10}); 
	$j("#icon1, #icon2, #icon3").droppable({
		drop: function(event,ui){
			this.style.backgroundColor = event.toElement.style.backgroundColor;
		}

	});
  });
  

function initial(){
	show_menu();
	//setInterval("random_flow();", 500);
}

function random_flow(){
	var current_flow = Math.random()*100;
	var angle = parseInt((current_flow * 2.1) + (-90));
    document.getElementById('upload_speed').innerHTML = parseInt(current_flow);
	document.getElementById('indicator_item').setAttribute("transform", "rotate("+angle+")");
    
    current_flow = Math.random()*100;
	angle = parseInt((current_flow * 2.1) + (-90));
    document.getElementById('download_speed').innerHTML = parseInt(current_flow);
	document.getElementById('indicator_item1').setAttribute("transform", "rotate("+angle+")");
}

function show_apps(obj){
	var parent_obj = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var parent_obj_temp = obj.parentNode.parentNode.parentNode.parentNode.parentNode;
	var code = "";

	if(obj.className.indexOf("closed") == -1){			//expand device's APPs
		var first_element = parent_obj_temp.firstChild;
		if(first_element.nodeName == "#text"){	
			parent_obj_temp.removeChild(first_element);
			first_element = parent_obj_temp.firstChild;
		}
		var last_element = parent_obj.lastChild;
		if(last_element.nodeName == "#text"){
			parent_obj.removeChild(last_element);
			last_element = parent_obj.lastChild;
		}
		
		parent_obj_temp.innerHTML = "";
		parent_obj_temp.appendChild(first_element);
		parent_obj_temp.appendChild(last_element);
		obj.setAttribute("class", "closed");
	}
	else{	
		var last_element = parent_obj.lastChild;
		if(last_element.nodeName == "#text"){
			parent_obj.removeChild(last_element);
			last_element = parent_obj.lastChild;
		}
			
		var new_element = document.createElement("table");
		parent_obj.removeChild(last_element);
		parent_obj.appendChild(new_element);
		parent_obj.appendChild(last_element);
			
		//code += '			<table>';
									code +=				'<tr>';
									code +=					'<td style="width:70px;">';
									code +=						'<div style="width:40px;height:40px;background-image:url(\'../images/New_ui/networkmap/client.png\');background-repeat:no-repeat;background-position:60% 104%;background-color:#1F2D35;border-radius:10px;margin-left:45px;background-size:37px"></div>';
									code +=					'</td>';
									code +=					'<td style="width:120px;">';
									code +=						'<div style="font-family:monospace">Facebook</div>';
									code +=					'</td>';
									code +=					'<td>';
									code +=						'<div style="margin-left:15px;">';
									code +=							'<table>';
									code +=								'<tr>';
									code +=									'<td style="width:385px">';
									code +=										'<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">';
									code +=											'<div style="width:80%;background-color:#FFFF6F;height:8px;black;border-radius:5px 5px 5px 5px;"></div>';
									code +=										'</div>';
									code +=									'</td>';
									code +=									'<td>		';															
									code +=										'<div style="border-bottom:10px solid #FFFF6F; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>';
									code +=									'</td>';
									code +=									'<td style="width:35px;text-align:right;">';
									code +=										'<div>800</div>';
									code +=									'</td>';
									code +=									'<td>';
									code +=										'<div>Kbps</div>';
									code +=									'</td>	';															
									code +=								'</tr>';
									code +=								'<tr>';
									code +=									'<td>';
									code +=										'<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">';
									code +=											'<div style="width:50%;background-color:#FFFF6F;height:8px;black;border-radius:5px 5px 5px 5px;"></div>';
									code +=										'</div>';
									code +=									'</td>';
									code +=									'<td>';
									code +=									'	<div style="border-top:10px solid #FFFF6F; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>';
									code +=								'	</td>';
									code +=									'<td style="width:35px;text-align:right;">';
									code +=									'	<div>500</div>';
									code +=									'</td>';
									code +=									'<td>';
									code +=									'	<div>Kbps</div>';
									code +=									'</td>';																	
									code +=								'</tr>';
									code +=							'</table>';
									code +=						'</div>';
									code +=					'</td>';
									code +=				'</tr>';
									//code +=			'</table>';

		new_element.innerHTML = code;
		obj.setAttribute("class", "opened");
	}	
}



</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/AiStream_TrafficControl.asp">
<input type="hidden" name="next_page" value="/AiStream_TrafficControl.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos;restart_firewall">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
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
			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle">
			<tr>
				<td bgcolor="#4D595D" valign="top">
					<table width="760px" border="0" cellpadding="4" cellspacing="0">
						<tr>
							<td bgcolor="#4D595D" valign="top">
								<table width="100%">
									<tr>
										<td class="formfonttitle" align="left">								
											<div>Adaptive QoS - Traffic Control</div>
										</td>	
									</tr>
								</table>	
							</td>
						</tr>
						<tr>
							<td height="5" bgcolor="#4D595D" valign="top"><img src="images/New_ui/export/line_export.png" /></td>
						</tr>
						<tr>
							<td>
								<div style="height:200px;width:350px;margin-left:45px;margin-top:-30px;">
									<svg style="height:200px;width:350px">				
									  <path transform="rotate(0)" d="M 50 141 A 100 100 0 1 1 235 190 L 212 179 A 75 75 0 1 0 75 141" stroke="#4D595D" stroke-width="2" fill="url(#MyGradient)" />	
									  
                                         <g font-size="12" font="monospace" fill="white" stroke="none" text-anchor="middle">
											<text x="40" y="140" dx="0">0</text>
											<text x="75" y="54" dx="0">25</text>
											<text x="173" y="36" dx="0">50</text>
											<text x="255" y="105" dx="0">75</text>
											<text x="253" y="190" dx="0">100</text>
										</g>
                                  							  
                                        <g font-size="8" font="monospace" fill="white" stroke="none" text-anchor="middle">
											<text x="87" y="54" dx="0">M</text>
											<text x="185" y="36" dx="0">M</text>
											<text x="267" y="105" dx="0">M</text>
											<text x="268" y="190" dx="0">M</text>
										</g>                                          
										
										
                                        <g font-size="26" font="monospace" fill="white" stroke="none" text-anchor="middle">
                                            <text id="upload_speed" x="105" y="190" fill="rgb(238,198,38)">0</text>
                                            <text font-size="22" x="150" y="190">Mbps</text>
                                        </g>    
                                                                              
										<g class="indicator" transform="translate(150, 140)" >
											<path id="indicator_item"  transform="rotate(67.5)" fill="red" stroke="red" stroke-width="1" d="M-2 0 L 2 0 L 0 -70 z"></path>
											<circle fill="gray" r="10"></circle>
										</g>	
										
										<defs>
											<linearGradient id="MyGradient">
												<stop offset="60%" stop-color="blue" />
												<stop offset="90%" stop-color="red" />
											</linearGradient>
										</defs>
									</svg>
								</div>
							</td>
							
                            <td>
								<div style="height:200px;width:350px;margin-left:-360px;margin-top:-30px;">
									<svg style="height:200px;width:350px">				
									  <path transform="rotate(0)" d="M 50 141 A 100 100 0 1 1 235 190 L 212 179 A 75 75 0 1 0 75 141" stroke="#4D595D" stroke-width="1" fill="url(#MyGradient)" />								  
                                        <g font-size="12" font="monospace" fill="white" stroke="none" text-anchor="middle">
											<text x="40" y="140" dx="0">0</text>
											<text x="75" y="54" dx="0">25</text>
											<text x="173" y="36" dx="0">50</text>
											<text x="255" y="105" dx="0">75</text>
											<text x="253" y="190" dx="0">100</text>
										</g>
                                  							  
                                        <g font-size="8" font="monospace" fill="white" stroke="none" text-anchor="middle">
											<text x="87" y="54" dx="0">M</text>
											<text x="185" y="36" dx="0">M</text>
											<text x="267" y="105" dx="0">M</text>
											<text x="268" y="190" dx="0">M</text>
										</g>           
                                        
                                         <g font-size="26" font="monospace" fill="white" stroke="none" text-anchor="middle">
                                            <text id="download_speed" x="105" y="190" fill="rgb(238,198,38)">0</text>
                                            <text font-size="22" x="150" y="190">Mbps</text>
                                        </g>
                                        
										<g class="indicator" transform="translate(150, 140)" >
											<path id="indicator_item1"  transform="rotate(-37.5)" fill="red" stroke="red" stroke-width="1" d="M-2 0 L 2 0 L 0 -70 z"></path>
											<circle fill="gray" r="10"></circle>
										</g>	
										
										<defs>
										  <linearGradient id="MyGradient">
											<stop offset="60%" stop-color="blue" />
											<stop offset="90%" stop-color="red" />
										  </linearGradient>
										</defs>
									</svg>
								</div>
							</td>
						</tr>
						<tr>
							<td>
								<div style="margin-bottom:-5px;">
									<table>									
										<tr>
											<th style="font-family: Arial, Helvetica, sans-serif;font-size:16px;width:41%;text-align:left;padding-left:15px;">Priority</th>
											<td>
												<div id="priority_block">
													<table>
														<tr>
															<td>
																<div style="background-color:blue;width:80px;border-radius:10px;text-align:center;color:black;">Highest</div>
															</td>
															<td>
																<div style="background-color:#0072E3;width:80px;border-radius:10px;text-align:center;color:black;">High</div>
															</td>
															<td>
																<div style="background-color:#46A3FF;width:80px;border-radius:10px;text-align:center;color:black;">Medium</div>
															</td>
															<td>
																<div style="background-color:#97CBFF;width:80px;border-radius:10px;text-align:center;color:black;">Low</div>												
															</td>
															<td>
																<div style="background-color:#D2E9FF;width:80px;border-radius:10px;text-align:center;color:black;">Lowest</div>												
															</td>
														</tr>
													</table>
												</div>
											<td>
										</tr>
									</table>
								</div>
							</td>
						</tr>
						
						<tr>					
							<td>
								<div colspan="2" style="width:100%;height:510px;background-color:#2E424D;border-radius:10px;margin-left:4px;overflow:auto;">
									<div id="sortable" style="padding-top:5px;">
										<div>
											<table>
												<tr>
													<td style="width:70px;">
														<div id="icon1" onclick="show_apps(this);" class="closed" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">RT-AC68U</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:80%;background-color:#FFFF6F;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>																	
																		<div style="border-bottom:10px solid #FFFF6F; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">
																		<div>800</div>
																	</td>
																	<td>
																		<div>Kbps</div>
																	</td>																
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:50%;background-color:#FFFF6F;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FFFF6F; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">
																		<div>500</div>
																	</td>
																	<td>
																		<div>Kbps</div>
																	</td>
																	
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>										
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>	
		
										
										
										<div>
											<table>
												<tr>
													<td style="width:70px;">
														<div id="icon2" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">ASUS Padfone Infinity</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:60%;background-color:#79FF79;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #79FF79; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>600</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:30%;background-color:#79FF79;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #79FF79; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>300</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>		
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>										
										
										
										<div>
											<table>	
												<tr>
													<td style="width:70px;">
														<div id="icon3" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">Jieming-iPhone</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:12%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>120</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:5%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>50</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>
										<div>
											<table>	
												<tr>
													<td style="width:70px;">
														<div id="icon4" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">Jieming-NB</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:12%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>120</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:5%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>50</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>	
										<div>
											<table>	
												<tr>
													<td style="width:70px;">
														<div id="icon5" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">Jieming-MacBook Pro</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:12%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>120</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:5%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>50</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>
										<div>
											<table>	
												<tr>
													<td style="width:70px;">
														<div id="icon6" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">Jieming-PC</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:12%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>120</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:5%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>50</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>
										<div>
											<table>	
												<tr>
													<td style="width:70px;">
														<div id="icon7" style="width:54px;height:54px;background-image:url('../images/New_ui/networkmap/client.png');background-repeat:no-repeat;background-position:70% 100%;background-color:#1F2D35;border-radius:10px;margin-left:10px;"></div>
													</td>
													<td style="width:150px;">
														<div style="font-family:monospace">Jieming-Pad</div>
													</td>
													<td>
														<div>
															<table>
																<tr>
																	<td style="width:385px">
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:12%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-bottom:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>120</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																	
																</tr>
																<tr>
																	<td>
																		<div style="height:8px;padding:3px;background-color:#000;border-radius:10px;">
																			<div style="width:5%;background-color:#FF0000;height:8px;black;border-radius:5px 5px 5px 5px;"></div>
																		</div>
																	</td>
																	<td>
																		<div style="border-top:10px solid #FF0000; border-left:10px solid #2E424D; border-right:10px solid #2E424D;"></div>
																	</td>
																	<td style="width:35px;text-align:right;">	
																		<div>50</div>
																	</td>
																	<td>	
																		<div>Kbps</div>
																	</td>																
																</tr>
															</table>
														</div>
													</td>
												</tr>
											</table>
											<div style="width:700px;border-top:solid 1px #ACD6FF;margin:5px 0px 5px 15px"></div>
										</div>	
								
									</div>
								</div>
							</td>
						</tr>
						<tr>
							<td>
								<div style=" *width:136px;margin:5px 0px 0px 300px;" class="titlebtn" align="center" onClick=""><span><#CTL_apply#></span></div>
							</td>
						</tr>		
					</table>
				</td>  
			</tr>
			</table>
			<!--===================================End of Main Content===========================================-->
		</td>		
	</tr>
</table>
<div id="footer"></div>
</body>
</html>
