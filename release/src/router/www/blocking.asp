<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Blocking Page</title>                          
<style>
body{
	background-color:#21333e;
	margin:10px auto;
	color:#FFF;
	font-family:Verdana, Arial, Helvetica, sans-serif;
}

.erTable thead{
	font-weight:bold;
	font-size:16px;
	text-align:center;
	line-height:25px;
}
.erTable th{
	font-weight:bolder;
	font-size:14px;
	line-height:25px;
	text-align:left;
}
.erTable thead td{
	height:52px;
	background: #7d7e7d; /* Old browsers */
	background: -moz-linear-gradient(top, #7d7e7d 0%, #0e0e0e 100%); /* FF3.6+ */
	background: -webkit-gradient(linear, left top, left bottom, color-stop(0%,#7d7e7d), color-stop(100%,#0e0e0e)); /* Chrome,Safari4+ */
	background: -webkit-linear-gradient(top, #7d7e7d 0%,#0e0e0e 100%); /* Chrome10+,Safari5.1+ */
	background: -o-linear-gradient(top, #7d7e7d 0%,#0e0e0e 100%); /* Opera11.10+ */
	background: -ms-linear-gradient(top, #7d7e7d 0%,#0e0e0e 100%); /* IE10+ */
	filter: progid:DXImageTransform.Microsoft.gradient( startColorstr='#7d7e7d', endColorstr='#0e0e0e',GradientType=0 ); /* IE6-9 */
	background: linear-gradient(top, #7d7e7d 0%,#0e0e0e 100%); /* W3C */
	border-top-left-radius: 10px;
	border-top-right-radius: 10px;
}

.erTable tbody th{
	background: #000; /* Old browsers */
	font-size:16px;
}

.erpage_td{
	background: #000; /* Old browsers */
}.Epagecontent{
	font-size:12px;
	border:1px dashed #333;
	background-color:#FFF;
	width:90%;
	padding:5px;
	margin:2px 0px 10px 20px;
	line-height:20px;
	color:#000;
}

.Epagetitle{
	font-size:14px;
	font-weight: bolder;
	line-height:30px;
	color:#FFF;
	margin:0px 0px 0px 20px;
}

.er_title{
	color:#FC0;
	background-color:#000000;
	font-weight:bolder;
	width:450px;
	height:30px;
	line-height:24px;
	margin:5px 0px 0px 20px;
}	

.modelName{
	color:#FFFFFF;
	font-size:22px;
	font-weight:bold;
	border:0px solid #333;
	width:300px;
	position:absolute;
	height:20px;
	letter-spacing:1px;
	margin:10px 0px 0px 18px;
}


</style>	
<script type="text/javascript">
var bwdpi_support = ('<% nvram_get("rc_support"); %>'.search('bwdpi') == -1) ? false : true;
var mac_parameter = '<% get_parameter("mac"); %>'.toUpperCase();
var casenum = '<% get_parameter("cat_id"); %>';
var block_info = '<% bwdpi_redirect_info(); %>';
if(block_info != "")
	block_info = JSON.parse(block_info);
var client_list_array = '<% get_client_detail_info(); %>';
var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var category_info = [["Parental Controls", "1", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites with profane or vulgar content generally considered inappropriate for minors; includes sites that offer erotic content or ads for sexual services, but excludes sites with sexually explicit images."],
				     ["Parental Controls", "2", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites that provide information about or software for sharing and transferring files related to child pornography."],
				     ["Parental Controls", "3", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites with sexually explicit imagery designed for sexual arousal, including sites that offer sexual services."],
				     ["Parental Controls", "4", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites with or without explicit images that discuss reproduction, sexuality, birth control, sexually transmitted disease, safe sex, or coping with sexual trauma."],
				     ["Parental Controls", "5", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites that sell swimsuits or intimate apparel with images of models wearing them."],
				     ["Parental Controls", "6", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites showing nude or partially nude images that are generally considered artistic, not vulgar or pornographic."],
				     ["Parental Controls", "8", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult1#>", "Sites that promote, sell, or provide information about alcohol or tobacco products."],			     
					 ["Parental Controls", "9", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites that promote and discuss how to perpetrate nonviolent crimes, including burglary, fraud, intellectual property theft, and plagiarism; includes sites that sell plagiarized or stolen materials."],
					 ["Parental Controls", "10", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites with content that is gratuitously offensive and shocking; includes sites that show extreme forms of body modification or mutilation and animal cruelty."],
					 ["Parental Controls", "14", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites that promote hate and violence; includes sites that espouse prejudice against a social group, extremely violent and dangerous activities, mutilation and gore, or the creation of destructive devices."],
					 ["Parental Controls", "15", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites about weapons, including their accessories and use; excludes sites about military institutions or sites that discuss weapons as sporting or recreational equipment."],
					 ["Parental Controls", "16", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites that promote, encourage, or discuss abortion, including sites that cover moral or political views on abortion."],
					 ["Parental Controls", "25", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites that promote, glamorize, supply, sell, or explain how to use illicit or illegal intoxicants."],
					 ["Parental Controls", "26", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult2#>", "Sites that discuss the cultivation, use, or preparation of marijuana, or sell related paraphernalia."],				 
					 ["Parental Controls", "11", "<#AiProtection_filter_Adult#>", "<#AiProtection_filter_Adult3#>", "Sites that promote or provide information on gambling, including online gambling sites."],
				     ["Parental Controls", "24", "<#AiProtection_filter_message#>", "<#AiProtection_filter_Adult4#>", "Sites that provide web-based services or downloadable software for Voice over Internet Protocol (VoIP) calls"],				    
					 ["Parental Controls", "51", "<#AiProtection_filter_message#>", "<#AiProtection_filter_Adult5#>", "Sites that send malicious tracking cookies to visiting web browsers."],				    
 					 ["Parental Controls", "53", "<#AiProtection_filter_message#>", "<#AiProtection_filter_Adult6#>", "Sites that provide software for bypassing computer security systems."],
				     ["Parental Controls", "89", "<#AiProtection_filter_message#>", "<#AiProtection_filter_Adult6#>", "Malicious Domain or website, Domains that host malicious payloads."],				
					 ["Parental Controls", "42", "<#AiProtection_filter_message#>", "<#AiProtection_filter_Adult7#>", "Content servers, image servers, or sites used to gather, process, and present data and data analysis, including web analytics tools and network monitors."],
				     ["Parental Controls", "56", "<#AiProtection_filter_p2p#>", "<#AiProtection_filter_p2p1#>", "Sites that provide downloadable 'joke' software, including applications that can unsettle users."],
				     ["Parental Controls", "70", "<#AiProtection_filter_p2p#>", "<#AiProtection_filter_p2p1#>", "Sites that use scraped or copied content to pollute search engines with redundant and generally unwanted results."],
				     ["Parental Controls", "71", "<#AiProtection_filter_p2p#>", "<#AiProtection_filter_p2p1#>", "Sites dedicated to displaying advertisements, including sites used to display banner or popup advertisement."],	    
					 ["Parental Controls", "57", "<#AiProtection_filter_p2p#>", "<#AiProtection_filter_p2p2#>", "Sites that distribute password cracking software."],					
				     ["Parental Controls", "69", "<#AiProtection_filter_stream#>", "<#AiProtection_filter_stream2#>", "Sites that provide tools for remotely monitoring and controlling computers"],
					 ["Parental Controls", "23", "<#AiProtection_filter_stream#>", "<#AiProtection_filter_stream3#>", "Sites that primarily provide streaming radio or TV programming; excludes sites that provide other kinds of streaming content."],				 
					 ["Home Protection", "91", "Anti-Trojan detecting and blocked", "", "Anti-Trojan detecting and blocked"],			   			   
				     ["Home Protection", "39", "Malicious site blocked", "", "Sites about bypassing proxy servers or web filtering systems, including sites that provide tools for that purpose."],
				     ["Home Protection", "73", "Malicious site blocked", "", "Sites that contain potentially harmful downloads."],
				     ["Home Protection", "74", "Malicious site blocked", "", "Sites with downloads that gather and transmit data from computers owned by unsuspecting users."],
				     ["Home Protection", "75", "Malicious site blocked", "", "Fraudulent sites that mimic legitimate sites to gather sensitive information, such as user names and passwords."],
				     ["Home Protection", "76", "Malicious site blocked", "", "Sites whose addresses have been found in spam messages."],
				     ["Home Protection", "77", "Malicious site blocked", "", "Sites with downloads that display advertisements or other promotional content; includes sites that install browser helper objects."],
				     ["Home Protection", "78", "Malicious site blocked", "", "Sites used by malicious programs, including sites used to host upgrades or store stolen information."],
				     ["Home Protection", "79", "Malicious site blocked", "", "Sites that directly or indirectly facilitate the distribution of malicious software or source code."],
				     ["Home Protection", "80", "Malicious site blocked", "", "Sites that send malicious tracking cookies to visiting web browsers."],
				     ["Home Protection", "81", "Malicious site blocked", "", "Sites with downloads that dial into other networks without user consent."],
				     ["Home Protection", "82", "Malicious site blocked", "", "Sites that provide software for bypassing computer security systems."],
				     ["Home Protection", "83", "Malicious site blocked", "", "Sites that provide downloadable 'joke' software, including applications that can unsettle users."],
				     ["Home Protection", "84", "Malicious site blocked", "", "Sites that distribute password cracking software."],
				     ["Home Protection", "85", "Malicious site blocked", "", "Sites that provide tools for remotely monitoring and controlling computers."],
				     ["Home Protection", "86", "Malicious site blocked", "", "Sites that use scraped or copied content to pollute search engines with redundant and generally unwanted results."],
				     ["Home Protection", "88", "Malicious site blocked", "", "Sites dedicated to displaying advertisements, including sites used to display banner or popup advertisement."],
				     ["Home Protection", "92", "Malicious site blocked", "", "Malicious Domain or website, Domains that host malicious payloads."]
	];
	
var target_info = {
	name: "",
	mac: "",
	url: "",
	category_id: "",
	category_type: "",
	content_category: "",
	desc: ""
}

function initial(){
	get_target_info();
	show_information();
}

function get_target_info(){
	var client_list_row = client_list_array.split('<');
	var custom_name_row = custom_name.split('<');
	var match_flag = 0;
	
	for(i=1;i<custom_name_row.length;i++){
		var custom_name_col = custom_name_row[i].split('>');
		if(custom_name_col[1] == block_info[0] || custom_name_col[1] == mac_parameter){
			target_info.name = custom_name_col[0];	
			match_flag =1;
		}
	}

	if(match_flag == 0){
		for(i=1;i< client_list_row.length;i++){
			var client_list_col = client_list_row[i].split('>');
			if(client_list_col[3] == block_info[0] || client_list_col[3] == mac_parameter){
				target_info.name = client_list_col[1];
			}
		}
	}

	if(casenum != ""){		//for AiProtection
		target_info.mac = block_info[0];
		target_info.url = block_info[1];
		target_info.category_id = block_info[2];
		get_category_info();
	}
	else{		//for Parental Controls (Time Scheduling)
		target_info.mac = mac_parameter;
		target_info.desc = "This device is blocked to access the Internet at this time";
	}
}

function get_category_info(){
	var cat_id = target_info.category_id;
	var category_string = "";
	for(i=0;i< category_info.length;i++){	
		if(category_info[i][1] == cat_id){		
			category_string = category_info[i][2];	
			if(category_info[i][3] != ""){
				category_string += " - " + category_info[i][3];
			}
			
			target_info.category_type = category_info[i][0];
			target_info.content_category = category_string;
			target_info.desc = category_info[i][4];
		}
	}
}

function show_information(){
	var code = "";	
	var code_suggestion = "";
	var code_title = "";
	var parental_string = "";
	
	code = "<ul>";
	code += "<li><div><span class='desc_info'>Description:</span><br>" + target_info.desc + "</div></li>";
	code += "<li><div><span class='desc_info'>Host: </span>";
	if(target_info.name == target_info.mac)
		code += target_info.name;
	else	
		code += target_info.name + " (" + target_info.mac.toUpperCase() + ")";
		
	code += "</div></li>";
	if(casenum != "")
		code += "<li><div><span class='desc_info'>URL: </span>" + target_info.url +"</div></li>";
	
	if(target_info.category_type == "Parental Controls")
		code += "<li><div><span class='desc_info'><#AiProtection_filter_category#> :</span>" + target_info.content_category + "</div></li>";	
	
	code += "</ul>";
	document.getElementById('detail_info').innerHTML = code;
	
	if(target_info.category_type == "Parental Controls"){
		code_title = "<div class='er_title' style='height:auto;'>The web page category is filtered</div>"
		code_suggestion = "<ul>";
		code_suggestion += "<li><span>If you are a manager and still want to visit this website, please go to <a href='AiProtection_WebProtector.asp'>Parental Controls</a> for configuration change.</span></li>";
		code_suggestion += "<li><span>If you are a client and have any problems, please contact your manager.</span></li>";		
		code_suggestion += "</ul>";
		document.getElementById('tm_block').style.display = "";
	}
	else if(target_info.category_type == "Home Protection"){
		code_title = "<div class='er_title' style='height:auto;'>Warning! The website contains malware. Visiting this site may harm your computer</div>"
		code_suggestion = "<ul>";
		code_suggestion += "If you are a manager and want to disable this protection, please go to <a href='AiProtection_HomeProtection.asp'>Home Protection</a> for configuration";
		code_suggestion += "</ul>";
		document.getElementById('tm_block').style.display = "";
	}
	else{		//for Parental Control(Time Scheduling)
		code_title = "<div class='er_title' style='height:auto;'>Warning! The device can't access the Internet now.</div>"
		code_suggestion = "<ul>";
		if(bwdpi_support)
			parental_string = "<#Time_Scheduling#>";
		else
			parental_string = "<#Parental_Control#>";

		code_suggestion += "If you are a manager and want to access the Internet, please go to <a href='ParentalControl.asp'>"+ parental_string +"</a> for configuration change.";
		code_suggestion += "<br>";	
		code_suggestion += "If you are a client and have any problems, please contact your manager.";	
		code_suggestion += "</ul>";	
		document.getElementById('tm_block').style.display = "none";
	}

	document.getElementById('page_title').innerHTML = code_title;
	document.getElementById('suggestion').innerHTML = code_suggestion;
}
</script>
<style>
.footer{
	height:36px;
	background: #0e0e0e; /* Old browsers */
	background: -moz-linear-gradient(top, #0e0e0e 0%, #7d7e7d 100%); /* FF3.6+ */
	background: -webkit-gradient(linear, left top, left bottom, color-stop(0%,#0e0e0e), color-stop(100%,#7d7e7d)); /* Chrome,Safari4+ */
	background: -webkit-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* Chrome10+,Safari5.1+ */
	background: -o-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* Opera11.10+ */
	background: -ms-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* IE10+ */
	filter: progid:DXImageTransform.Microsoft.gradient( startColorstr='#0e0e0e', endColorstr='#7d7e7d',GradientType=0 ); /* IE6-9 */
	background: linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* W3C */
	border-bottom-right-radius: 10px;
	border-bottom-left-radius: 10px;
}

.tm_logo{
	background:url('images/New_ui/tm_logo_1.png');
	width:268px;
	height:48px;
	float:right;
}
.desc_info{
	font-weight:bold;
	font-size:14px;
}
#detail_info{
	color:red
}

#tm_block{
	margin-top:20px;
}

.erTable tbody th{
	height:40px;
}
</style>
</head>

<body onload="initial();">
<br><br>
<table width="500" border="0" align="center" cellspacing="0" class="erTable">
	<thead>
		<tr>
			<td height="52" align="left" valign="top"><span class="modelName"><#Web_Title2#></span></td>
		</tr>
	</thead>
	<tr>
		<th valign="top" id="page_title"></th>
	</tr>
	<tr>
		<td  valign="top" class="erpage_td">
			<span class="Epagetitle">Detailed informations:</span>	  
			<div id="detail_info" class="Epagecontent"></div>	  
			<div class="Epagetitle">
				<div>
					<span><#web_redirect_suggestion0#>:</span>
				</div>		
			</div>
			<div class="Epagecontent">
				<div id="suggestion"></div>
			</div>
			<div id="tm_block" class="Epagecontent" style="display:none">
				<table>
					<tr>				
						<td>
							<div>For your client side advanced internet security protection. Trend Micro offer you more advanced home security solution. Please <a href="http://www.trendmicro.com" target="_blank">visit the site</a> for free trial or online scan service.</div>	
						</td>
					</tr>
					<tr>
						<td>
							<div class="tm_logo"></div>
						</td>						
					</tr>
				</table>
			</div>
		</td>
	</tr>
	<tr>
		<td height="22" class="footer"><span></span></td>
	</tr>
</table>
</body>
</html>

