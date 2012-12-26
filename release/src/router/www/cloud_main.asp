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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

if('<% nvram_get("start_aicloud"); %>' == '0')
	location.href = "cloud__main.asp";

var $j = jQuery.noConflict();

<% login_state_hook(); %>
<% wanlink(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var cloud_sync = '<% nvram_get("cloud_sync"); %>';
var cloud_status;
window.onresize = cal_agreement_block;
var curState = '<% nvram_get("webdav_aidisk"); %>';

function initial(){
	show_menu();
	addOnlineHelp($("faq0"), ["samba"]);
	addOnlineHelp($("faq1"), ["ASUSWRT", "port", "forwarding"]);
	addOnlineHelp($("faq2"), ["ASUSWRT", "DMZ"]);
	addOnlineHelp($("faq3"), ["WOL", "BIOS"]);

	switch(valid_is_wan_ip(wanlink_ipaddr())){
		/* private */
		case 0: 
			$("accessMethod").innerHTML = '<#aicloud_disk_case0#>';
			break;
		/* public */
		case 1:
			$("privateIpOnly").style.display = "none";
			if('<% nvram_get("ddns_enable_x"); %>' == '1' && '<% nvram_get("ddns_hostname_x"); %>' != ''){
				$("accessMethod").innerHTML = '<#aicloud_disk_case11#> <a style="font-weight: bolder;text-decoration: underline;word-break:break-all;" href="https://<% nvram_get("ddns_hostname_x"); %>" target="_blank">https://<% nvram_get("ddns_hostname_x"); %></a><br />';
				$("accessMethod").innerHTML += '<#aicloud_disk_case12#>';
			}
			else{
				$("accessMethod").innerHTML = '<#aicloud_disk_case21#><br />';
				$("accessMethod").innerHTML += '<#aicloud_disk_case22#>';
			}
			break;
	}
	cal_agreement_block();
}

function valid_is_wan_ip(ip_obj){
  // test if WAN IP is a private IP.
  var A_class_start = inet_network("10.0.0.0");
  var A_class_end = inet_network("10.255.255.255");
  var B_class_start = inet_network("172.16.0.0");
  var B_class_end = inet_network("172.31.255.255");
  var C_class_start = inet_network("192.168.0.0");
  var C_class_end = inet_network("192.168.255.255");
  
  var ip_num = inet_network(ip_obj);
  if(ip_num > A_class_start && ip_num < A_class_end)
		return 0;
  else if(ip_num > B_class_start && ip_num < B_class_end)
		return 0;
  else if(ip_num > C_class_start && ip_num < C_class_end)
		return 0;
	else if(ip_num == 0)
		return 0;
  else
		return 1;
}

function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	return -2;
}

function cancel(){
	this.FromObject ="";
	$j("#agreement_panel").fadeOut(300);
	$j('#iphone_switch').animate({backgroundPosition: -37}, "slow", function() {});
	curState = 0;
	$("hiddenMask").style.visibility = "hidden";
}

function _confirm(){
	document.form.webdav_aidisk.value = 1;
	document.form.enable_webdav.value = 1;
	FormActions("start_apply.htm", "apply", "restart_webdav", "3");
	showLoading();
	document.form.submit();
}

function cal_agreement_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.25)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	

	}

	$("agreement_panel").style.marginLeft = blockmarginLeft+"px";
}
</script>
</head>
<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
	<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;">
			<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:25px;margin-left:30px;">Term of use</div>
			<div class="folder_tree">Thank you for using our AiCloud firmware, AiCloud mobile application and AiCloud portal website(“AiCloud”). AiCloud is provided by ASUSTeK Computer Inc. (“ASUS”). This notice constitutes a valid and binding agreement between ASUS. By using AiCloud, YOU, AS A USER, EXPRESSLY ACKNOWLEGE THAT YOU HAVE READ AND UNDERSTAND AND AGREE TO BE BOUND BY THE TERMS OF USE NOTICE (“NOTICE”) AND ANY NEW VERSIONS HEREOF.
<br><br>
<b>1.LICENSE</b><br>
Subject to the terms of use notice, ASUS hereby grants you a limited, personal, non-commercial, non-exclusive license to use AiCloud solely as embedded in the ASUS products. This license shall not be sublicensed, and is not transferable except to a person or entity to whom you transfer ownership of the complete ASUS products containing AiCloud, provided you permanently transfer all rights under this notice and do not retain any full or partial copies of AiCloud, and the recipient agrees to the NOTICE.
<br><br> 
2.SOFTWARE<br> 
AiCloud means AiCloud feature in firmware, AiCloud mobile application or AiCloud portal website ASUS provided in or with the applicable ASUS product including but not limited to any future programming fixes, updates, upgrades and modified versions.
<br><br> 
3.REVERSE RESTRICTION<br>
You shall not reverse engineer, decompile decrypt or disassemble the Software or permit or induce the foregoing except to the extent directly applicable law prohibits enforcement of the foregoing.
<br><br>  
4.CONFIDENTIAL ITY<br>
You shall have a duty to protect and not to disclose or cause to be disclosed in whole or in part of confidential information of AiCloud including but not limited to trade secrets, copyrighted material in any form to any third party.
<br><br>  
5.INTELLECTUAL PROPERTY RIGHTS (“IP RIGHTS”)<br>
You acknowledge and agree that all IP Rights, title, and interest to or arising from AiCloud are and shall remain the exclusive property of ASUS or its licensor. Nothing in this notice intends to transfer any such IP Rights to You. Any unauthorized use of the IP Rights is a violation of this notice as well as a violation of intellectual property laws, including but not limited to copyright laws and trademark laws. ASUS hereby grants you limited, non-exclusive and revocable right and license for you to (a) install and use AiCloud and its related components and documents for personal non-commercial use in compliance with this agreement; (b) create AiCloud mobile software ID and use in compliance with this agreement; (c) visit AiCloud portal website in compliance with this agreement. AiCloud is under protection of Republic of China(“Taiwan”) law and international conventions. The entire responsibility with regard to all data, including but not limited to images, texts, software, music, videos, graphs, and messages you upload, transmit or enter into AiCloud is with you and does not reflect the views of ASUS. 
<br><br> 
6.PERMITTED USES<br>
You may: (a) install AiCloud on devices that stores sound recordings, audiovisual works and pictures (generically “content”), that you either own, have a legal right to possess and use or have acquired usage rights under a valid license from the content owner; (b) use AiCloud to transfer content (“sync”) between your storage and one or more ASUS-branded devices/serives; (c) use AiCloud to designate content and other files on your computer or storage to make available for access via the Internet or wireless network to other devices that you own through a web browser, mobile device application or other service (“stream”); and (d) configure your ASUS-branded devices to automatically upload photos that you have taken with their cameras to your computer; and (e) download and install software updates to your ASUS-branded devices, all subject to local laws and regulations governing such activities. You may only sync and stream content with devices that you own or to which you have the right to reproduce or display under a valid license or an applicable law.
<br><br>  
7.PROHIBITED USES<br>
You may not use AiCloud in any manner not expressly authorized by this agreement, including but not limited to: (a) decompiling, reverse engineering or otherwise attempting to derive the source code of AiCloud; (b) modifying, translating or otherwise altering AiCloud’s source or object code; (c) using AiCloud for the benefit of a third party except as expressly permitted by law; (d) using AiCloud to sync content that you do not own, possess under a valid license from the content owner or otherwise have a legal right to sync; (e) using AiCloud in conjunction with any kind of automated software such as but not limited to bots, spiders, malware, viruses or adware; (f) using AiCloud in a manner that would damage ASUS in any way; (g) using AiCloud in any manner prohibited by law, including without limitation to infringe copyrighted materials; (h) using AiCloud in any commercial endeavor without ASUS’s express written consent; or (i) transferring AiCloud to a third party in any way. AiCloud is not fault-tolerant and is not designed, manufactured or intended for use in hazardous environments requiring fail-safe performance in which the failure of AiCloud could lead directly to death, personal injury or severe physical or environmental damage ("High Risk Activities"). You agree not to use AiCloud in High Risk Activities. ASUS respects intellectual property rights (IPR). You shall not use AiCloud in a way infringing other’s IPR. If you use AiCloud in a way violating IPR, ASUS reserves respective rights to (a) remove or disable access of contents which ASUS believes to infringe on a third party’s right; (b) disable or close your AiCloud account; (c) take legal actions. You shall not sell, lease, provide for download or transfer AiCloud, including all other ASUS software services, in any way and to any degree. ASUS, its suppliers and licensors reserve the right to take legal actions in the event of any infringement of IPR and business benefit. 
<br><br>  
8.ELIGIBILITY<br>
You acknowledge and agree if you are under the legal age of majority for your country of residence or if your use of AiCloud has previously been suspended or prohibited, that any license grant provided you is automatically terminated retroactively. BY DOWNLOADING, INSTALLING OR OTHERWISE USING AiCloud, YOU REPRESENT THAT YOU ARE OF LEGAL AGE OF MAJORITY IN YOUR COUNTRY OF RESIDENCE AND HAVE NOT BEEN PREVIOUSLY SUSPENDED OR PROHIBITED FROM USING AiCloud.
<br><br>  
9.FEEDBACK<br> 
By sending any information, material, ideas, concepts or techniques except for personal information to ASUS through AiCloud (“Feedbacks”), You acknowledge and agree that: (i) Your Feedbacks shall not contain confidential or proprietary information; (ii) ASUS is under no obligation of any kind of confidentiality, express or implied, regarding the Feedbacks; (iii)ASUS shall be entitled to use or disclose, at ASUS’s sole discretion, the Feedbacks for any purpose, in any way, worldwide; (iv) Your Feedbacks shall become the property of ASUS without any obligation of ASUS to You; and (v) You are not entitled to any compensation or reimbursement of any kind from.
<br><br>  
10.TERM AND TERMINATION<br>
This NOTICE will be effective as of the date you agree this NOTICE and enable the AiCloud in firmware or install AiCloud in your mobile devices. You may terminate this License at any time by ceasing all use of AiCloud and removing AiCloud mobile application. Your right will terminate without prior notice from ASUS if we think you fail to comply with any provision of this NOTICE.
<br><br>  
11.WARRANTY DISCLAIMER AND LIMITATION OF LIABILITY<br>
By using AiCloud, you represent and warranty that (a) you agree to this NOTICE; (b) your right to use AiCloud has never been revoked by ASUS before; (c) you use AiCloud obeying this NOTICE and laws concerned. ASUS warrants to you that ASUS protect information by de facto standard safety methods and procedures. NOTWITHSTANDING THE ABOVEMENTIONED WARRANTY FOR DE FACTO STANDARD SAFETY METHODS AND PROCEDURES, AICLOUD IS PROVIDED TO YOU “AS IS.” THE ENTIRE RISK AS TO THE QUALITY, DATA SAFETY AND PERFORMANCE OF AICLOUD IS WITH YOU, INCLUDING BUT NOT LIMITED TO RELATED COST OF ALL NECESSARY SERVICING, DATA LOSS, INTERNET ATTACKS AND REPAIR UNLESS SERIOUS HUMAN NEGLIGENCE OR INTENDED DAMAGE CAUSED BY ASUS. TO THE EXTENT PERMITTED BY LAW, ASUS AND ITS SUPPLIERS AND LICENSORS MAKES NO WARRANTIES, EXPRESS OR IMPLIED, WITH RESPECT TO AICLOUD OR ANY OTHER INFORMATION OR DOCUMENTATION PROVIDED UNDER THIS EULA OR FOR AICLOUD, INCLUDING BUT NOT LIMITED TO ANY WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR AGAINST INFRINGEMENT, OR ANY WARRANTY ARISING OUT OF USAGE OR OUT OF COURSE OF PERFORMANCE. ASUS ALSO MAKES NO WARRANTY AS TO THE RESULTS THAT MAY BE OBTAINED FROM THE USE OF AICLOUD OR AS TO THE ACCURACY OR RELIABILITY OF ANY INFORMATION OBTAINED THROUGH AICLOUD. ASUS AND ITS SUPPLIERS AND LICENSORS ARE NOT RESPONSIBLE FOR THE LOSS RESULTING FROM YOUR USE AND NOT-USE OF AICLOUD, CONTENTS AND INFORMATION, INCLUDING BUT NOT LIMIT TO SPECIAL, DIRECT, INDIRECT OR NECESSARY LOSS, LOSS OF PUNISHMENT, RELIABILITY OR ANY FORM, EVEN IN THE EVENT THAT ASUS AND ITS SUPPLIERS AND LICENSORS ARE INFORMED IN PRIOR. SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF CERTAIN WARRANTIES OR THE LIMITATION OR EXCLUSION OF LIABILITY FOR CERTAIN DAMAGES. ACCORDINGLY, SOME OF THE ABOVE DISCLAIMERS AND LIMITATIONS OF LIABILITY MAY NOT APPLY TO YOU. As AiCloud is a free licensed software, in any event that your use of AiCloud causes loss and damage, no matter written in this NOTICE or not, ASUS, its contractors, suppliers, associated companies, employees, agents, third party cooperators, licensors are liable for indemnity for less than 1USD. ASUS sets forth amount and terms herein reflecting the fairness of risk taken by you and ASUS. By using AiCloud, you agree amounts, terms and disclaimers set forth in this NOTICE. If you do not agree, you may not be able to use AiCloud.
<br><br>  
12.INDEMNITY<br>
YOU agree to defend, indemnify and hold harmless ASUS and its affiliates, service providers, syndicators, suppliers, licensors, officers, directors and employees, from and against any and all losses, damages, liabilities, and expenses arising out of any claim or demand (including reasonable attorneys' fees and court costs), due to or in connection with YOUR violation of this NOTICE or any applicable law or regulation, or third-party right. In particular, AiCloud app is available on Apple Store and Google Play!. You agree to defend, indemnify and hold harmless Apple Store and Google Play!. ASUS reserves the right to recover court costs and a reasonable attorney’s fee. You agree to cooperate with ASUS for related recoveries. ASUS will notify you of related recoveries, actions or lawsuits to a reasonable degree. 
<br><br>  
13.INFORMATION COLLECTION AND USE: PRIVACY POLICY<br>
To use AiCloud, You have to create an ASUS DDNS name or use an existing one. If you choose not to provide an ASUS DDNS name, You will not be able to use AiCloud. An ASUS DDNS name collects solely public IPs of your ASUS routers to enable remote access function. An ASUS DDNS name does not collect any information of your usage data, files or personal info. When you create an ASUS DDNS name account, ASUS saves all info collected via the ASUS DDNS name in ASUS servers. By using an ASUS DDNS, you understand and consent to these information collection and usage terms, including (where applicable) the transfer of data into the ASUS server. ASUS does not disclose your personal information to third parties, including but not limited to advertising or marketing agencies. ASUS discloses your personal information to third parties via AiCloud only in following circumstances: a.When ASUS provides You technical support for AiCloud. ASUS requires third parties to sign related agreements to protect your personal information from being disclosed according to the terms in this agreement. b.When disclosing your personal information is to follow legal procedures or public policies to protect public or personal safety, or to protect ASUS property and benefit. If You have any question or further information regarding ASUS AiCloud information collection and use, please contact ASUS via privacy@asus.com or address below: ASUS address
<br><br>  
14.INFORMATION COLLECTION AND USE: COOKIES<br>
When you visit AiCloud portal website for the first time, ASUS server may save one small text file, a “Cookie”, to your desktop or mobile devices. The Cookie allows AiCloud website to retrieve your browsing history. The Cookie helps ASUS provide customized service and better user experience to you. ASUS may use Cookie to measure website traffic volume, to learn areas on the website you have visited and the pattern of your visit.Information collected by Cookie is called “Clickstream”. ASUS uses Clickstream to understand how users are navigated by ASUS websites and traffic volume patterns such as which website users come from. Clickstream helps ASUS develop website guide, recommend products and re-design websites according to users’ browsing behaviors. Clickstream also helps ASUS customize contents, banners and promotion advertisements for each user to make best purchase decisions. ASUS retrieves Clickstream through a third party. You may change Cookie settings according to your needs. Changing Internet Explorer preference settings allows you to get notice to accept or reject Cookies or when Cookie settings changed. Please note that if you reject all Cookies, you will be not able to use services or join activities that require Cookie. 
<br><br>  
15.TRADEMARKS<br>
“ASUS”, and all other ASUS logos and brand names are properties of ASUS and its suppliers and licensors. By using AiCloud, you agree not to remove, alter or obscure any such marks or logos that appear as part of AiCloud or any associated materials that you may receive. This agreement grants you no right to use such marks other than in conjunction with AiCloud.
<br><br>  
16.TRANSLATIONS<br>
The English version of this agreement is the controlling version. Any translations are provided for convenience only.
<br><br>  
17.MISCELLANEOUS<br>
This agreement constitutes the entire agreement between you and ASUS with respect to AiCloud and supersedes all previous communications, representations, understandings and agreements, either oral or written, between you and ASUS regarding AiCloud. This agreement may not be modified or waived except in writing and signed by an officer or other authorized representative of each party. If any provision is held invalid, all other provisions shall remain valid, unless such invalidity would frustrate the purpose of this agreement. The failure of either party to enforce any rights granted hereunder or to take action against the other party in the event of any breach hereunder will not waive that party’s as to subsequent enforcement of rights or subsequent action in the event of future breaches. AiCloud reserves the right to: (a) add or remove functions and features or to provide updates, upgrades or programming fixes to AiCloud with or without prior notice to you; (b) require you to agree to a new agreement in order to use any new version of AiCloud that it releases; or (c) require you to upgrade to a new version of AiCloud by discontinuing service or support to any prior version of AiCloud without notice.
</div>
			<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">
				<input class="button_gen" type="button" style="margin-left:27%;margin-top:18px;" onclick="cancel();" value="<#CTL_Cancel#>">
				<input class="button_gen" type="button"  onclick="_confirm();" value="<#CTL_ok#>">	
			</div>
	</div>
	<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
	</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_main.asp">
<input type="hidden" name="next_page" value="cloud_main.asp">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
<input type="hidden" name="enable_webdav" value="<% nvram_get("enable_webdav"); %>">
<input type="hidden" name="webdav_aidisk" value="<% nvram_get("webdav_aidisk"); %>">
<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<div class="tabclick"><span>AiCloud</span></div>
						</td>
						<td>
							<a href="cloud_sync.asp"><div class="tab"><span>Smart Sync</span></div></a>
						</td>
						<td>
							<a href="cloud_settings.asp"><div class="tab"><span>Settings</span></div></a>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab"><span>Log</span></div></a>
						</td>
					</tr>
					</tbody>
				</table>
			</div>
<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
							  <td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">AiCloud</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<table width="100%" height="550px" style="border-collapse:collapse;">

									  <tr bgcolor="#444f53">
									    <td colspan="5" bgcolor="#444f53" class="cloud_main_radius">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#step_use_aicloud#>
												  <ol style="-webkit-margin-after: 0em;word-break:normal;">
										        <li style="margin-bottom:7px;">
															<#download_aicloud#><br> 
															<a href="https://play.google.com/store/apps/details?id=com.asustek.aicloud" target="_blank"><img border="0" src="/images/cloudsync/googleplay.png" width="100px"></a>
															<a href="https://itunes.apple.com/us/app/aicloud-lite/id527118674" target="_blank"><img src="/images/cloudsync/AppStore.png" border="0"  width="100px"></a>
														</li>
										        <li style="margin-bottom:7px;">															
															<#connect_through_wifi#>
														</li>
										        <li style="margin-bottom:7px;">
															<#access_service#>
														</li>
												  </ol>

												  <ul style="-webkit-margin-after: 0em;word-break:normal;" id="privateIpOnly">
										        <li style="margin-top:-5px;">
															<span>
																<#aicloud_for_privateIP1#>&nbsp;
																<#aicloud_for_privateIP2#>&nbsp;
	       												<#aicloud_for_privateIP3#>
															</span>
														</li>
												  </ul>
												  <ul style="-webkit-margin-after: 0em;word-break:normal;">
										        <li style="margin-top:-5px;">
															<span>
																<#aicloud_bandwidth1#><br>
																<ul>	
																	<li><#aicloud_bandwidth2_1#></li>
																	<li><#aicloud_bandwidth2_2#></li>
																	<li><#aicloud_bandwidth2_3#></li>
																	<li><#aicloud_bandwidth2_4#></li>
																</ul>			
	       												<#aicloud_bandwidth3#>
															</span>
														</li>
												  </ul>												  

												</div>
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53" width="235px">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center"><img src="/images/cloudsync/001.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div>
												</div>
											</td>

									    <td width="6px">
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>

									    <td width="1px"></td>

									    <td>
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#aicloud_disk1#>&nbsp
													<#aicloud_disk2#><br />
													<div id="accessMethod"></div>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/AiDesk.png"-->
												</div>
											</td>

									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100px">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_clouddisk_enable"></div>
												<div id="iphone_switch_container" class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_clouddisk_enable').iphoneSwitch('<% nvram_get("webdav_aidisk"); %>', 
														 function(){
															if(window.scrollTo)
																window.scrollTo(0,0);
															curState = 1;
															$j("#agreement_panel").fadeIn(300);	
															dr_advise();
															htmlbodyforIE = document.getElementsByTagName("html");
															htmlbodyforIE[0].style.overflow = "auto";
														 },
														 function() {
															document.form.webdav_aidisk.value = 0;
															if(document.form.webdav_proxy.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>
												</div>	
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Cloud Disk</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>
										
									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center"><img src="/images/cloudsync/002.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Access</div>
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_access1#><br />
													<#smart_access2#><br />
													<#smart_access3#>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/SmartSync.png"-->
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartAccess_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartAccess_enable').iphoneSwitch('<% nvram_get("webdav_proxy"); %>', 
														 function() {
															curState = '<% nvram_get("webdav_proxy"); %>';
															document.form.webdav_proxy.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 function() {
															curState = '<% nvram_get("webdav_proxy"); %>';
															document.form.webdav_proxy.value = 0;
															if(document.form.webdav_aidisk.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>			
												</div>	
											</td>
									  </tr>

									  <!--tr bgcolor="#444f53">
									    <td height="40"><div align="center" style="margin-top:-30px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Access</div></td>
									  </tr-->

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center">
													<img src="/images/cloudsync/003.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;">Smart Sync</div>
													<!--img width="50px" src="/images/cloudsync/Status_fin.png" style="margin-top:-50px;margin-left:80px;"-->
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_sync1#><br />
													<#smart_sync2#>
													<!--br>
													<br>
													<img style="margin-left:200px;" src="/images/cloudsync/SmartAccess.png"-->
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<!--div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartSync_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartSync_enable').iphoneSwitch('<% nvram_get("enable_cloudsync"); %>',
														function() {
															document.form.enable_cloudsync.value = 1;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();
														},
														function() {
															document.form.enable_cloudsync.value = 0;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();	
														},
														{
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														}
													);
												</script>			
												</div-->

						  					<input name="button" type="button" class="button_gen_short" onclick="location.href='/cloud_sync.asp'" value="<#btn_go#>"/>
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
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
