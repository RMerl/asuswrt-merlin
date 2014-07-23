<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Redirect Page</title>                          
<style>
body{
	background-color:#21333e;
	margin:10px auto;
}

.erTable thead{
	color:#FFFFFF;
	font-weight:bolder;
	font-size:15px;
	font-family:Verdana, Arial, Helvetica, sans-serif;
	text-align:center;
	line-height:25px;
}
.erTable th{
	font-weight:bolder;
	font-size:13px;
	font-family:Verdana, Arial, Helvetica, sans-serif;
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
	-webkit-border-top-left-radius: 10px;
	-webkit-border-top-right-radius: 10px;
	-moz-border-radius-topleft: 10px;
	-moz-border-radius-topright: 10px;
	border-top-left-radius: 10px;
	border-top-right-radius: 10px;
}

.erTable tbody th{
	background: #000; /* Old browsers */
	font-size:15px;
}

.erpage_td{
	background: #000; /* Old browsers */
}

.erpage_footer{
	height:36px;
	background: #0e0e0e; /* Old browsers */
	background: -moz-linear-gradient(top, #0e0e0e 0%, #7d7e7d 100%); /* FF3.6+ */
	background: -webkit-gradient(linear, left top, left bottom, color-stop(0%,#0e0e0e), color-stop(100%,#7d7e7d)); /* Chrome,Safari4+ */
	background: -webkit-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* Chrome10+,Safari5.1+ */
	background: -o-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* Opera11.10+ */
	background: -ms-linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* IE10+ */
	filter: progid:DXImageTransform.Microsoft.gradient( startColorstr='#0e0e0e', endColorstr='#7d7e7d',GradientType=0 ); /* IE6-9 */
	background: linear-gradient(top, #0e0e0e 0%,#7d7e7d 100%); /* W3C */
	-webkit-border-bottom-right-radius: 10px;
	-webkit-border-bottom-left-radius: 10px;
	-moz-border-radius-bottomright: 10px;
	-moz-border-radius-bottomleft: 10px;
	border-bottom-right-radius: 10px;
	border-bottom-left-radius: 10px;
}

.Epagecontent{
	font-size:12px;
	font-family:Verdana, Arial, Helvetica, sans-serif;
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
	font-family:Verdana, Arial, Helvetica, sans-serif;
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
	font-family:Verdana;
	font-weight:bolder;
	border:0px solid #333;
	width:300px;
	position:absolute;
	height:20px;
	letter-spacing:1px;
	margin:10px 0px 0px 18px;
}

.desc_info{
	font-weight:bold;
	font-size:13px;
}
</style>	
<script type="text/javascript">
var casenum = '<% get_parameter("cat_id"); %>';
var block_info = <% bwdpi_redirect_info(); %>;
var client_list_array = '<% get_client_detail_info(); %>';
var custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var category_info = [["Parental Controls", "1", "Adult", "Pornography", "Sites with profane or vulgar content generally considered inappropriate for minors; includes sites that offer erotic content or ads for sexual services, but excludes sites with sexually explicit images."],
				     ["Parental Controls", "2", "Adult", "Pornography", "Sites that provide information about or software for sharing and transferring files related to child pornography."],
				     ["Parental Controls", "3", "Adult", "Pornography", "Sites with sexually explicit imagery designed for sexual arousal, including sites that offer sexual services."],
				     ["Parental Controls", "4", "Adult", "Pornography", "Sites with or without explicit images that discuss reproduction, sexuality, birth control, sexually transmitted disease, safe sex, or coping with sexual trauma."],
				     ["Parental Controls", "5", "Adult", "Pornography", "Sites that sell swimsuits or intimate apparel with images of models wearing them."],
				     ["Parental Controls", "6", "Adult", "Pornography", "Sites showing nude or partially nude images that are generally considered artistic, not vulgar or pornographic."],
				     ["Parental Controls", "8", "Adult", "Pornography", "Sites that promote, sell, or provide information about alcohol or tobacco products."],			     
					 ["Parental Controls", "9", "Adult", "Illegal and Violence", "Sites that promote and discuss how to perpetrate nonviolent crimes, including burglary, fraud, intellectual property theft, and plagiarism; includes sites that sell plagiarized or stolen materials."],
					 ["Parental Controls", "10", "Adult", "Illegal and Violence", "Sites with content that is gratuitously offensive and shocking; includes sites that show extreme forms of body modification or mutilation and animal cruelty."],
					 ["Parental Controls", "14", "Adult", "Illegal and Violence", "Sites that promote hate and violence; includes sites that espouse prejudice against a social group, extremely violent and dangerous activities, mutilation and gore, or the creation of destructive devices."],
					 ["Parental Controls", "15", "Adult", "Illegal and Violence", "Sites about weapons, including their accessories and use; excludes sites about military institutions or sites that discuss weapons as sporting or recreational equipment."],
					 ["Parental Controls", "16", "Adult", "Illegal and Violence", "Sites that promote, encourage, or discuss abortion, including sites that cover moral or political views on abortion."],
					 ["Parental Controls", "25", "Adult", "Illegal and Violence", "Sites that promote, glamorize, supply, sell, or explain how to use illicit or illegal intoxicants."],
					 ["Parental Controls", "26", "Adult", "Illegal and Violence", "Sites that discuss the cultivation, use, or preparation of marijuana, or sell related paraphernalia."],				 
					 ["Parental Controls", "11", "Adult", "Gambling", "Sites that promote or provide information on gambling, including online gambling sites."],
				     ["Parental Controls", "24", "Instant Message and Communication", "Internet Telephony", "Sites that provide web-based services or downloadable software for Voice over Internet Protocol (VoIP) calls"],				    
					 ["Parental Controls", "51", "Instant Message and Communication", "Instant Mssaging", "Sites that send malicious tracking cookies to visiting web browsers."],				    
 					 ["Parental Controls", "53", "Instant Message and Communication", "Virtual Community", "Sites that provide software for bypassing computer security systems."],
				     ["Parental Controls", "89", "Instant Message and Communication", "Virtual Community", "Malicious Domain or website, Domains that host malicious payloads."],				
					 ["Parental Controls", "42", "Instant Message and Communication", "Blog", "Content servers, image servers, or sites used to gather, process, and present data and data analysis, including web analytics tools and network monitors."],
				     ["Parental Controls", "56", "P2P and File Transfer", "File transfer", "Sites that provide downloadable 'joke' software, including applications that can unsettle users."],
				     ["Parental Controls", "70", "P2P and File Transfer", "File transfer", "Sites that use scraped or copied content to pollute search engines with redundant and generally unwanted results."],
				     ["Parental Controls", "71", "P2P and File Transfer", "File transfer", "Sites dedicated to displaying advertisements, including sites used to display banner or popup advertisement."],	    
					 ["Parental Controls", "57", "P2P and File Transfer", "Peer to Peer", "Sites that distribute password cracking software."],					
				     ["Parental Controls", "69", "Streaming and Entertainment", "Streaming media", "Sites that provide tools for remotely monitoring and controlling computers"],
					 ["Parental Controls", "23", "Streaming and Entertainment", "Internet Radio and TV", "Sites that primarily provide streaming radio or TV programming; excludes sites that provide other kinds of streaming content."],				 
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
		if(custom_name_col[1] == block_info[0]){
			target_info.name = custom_name_col[0];	
			match_flag =1;
		}
	}

	if(match_flag == 0){
		for(i=1;i< client_list_row.length;i++){
			var client_list_col = client_list_row[i].split('>');
			if(client_list_col[3] == block_info[0]){
				target_info.name = client_list_col[1];
			}
		}
	}

	target_info.mac = block_info[0];
	target_info.url = block_info[1];
	target_info.category_id = block_info[2];
	get_category_info();
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
	
	code = "<ul>";
	code += "<li><div><span class='desc_info'>Description:</span><br>" + target_info.desc + "</div></li>";
	code += "<li><div><span class='desc_info'>Host: </span>" + target_info.name + "</div></li>";
	code += "<li><div><span class='desc_info'>URL: </span>" + target_info.url +"</div></li>";
	if(target_info.category_type == "Parental Controls")
		code += "<li><div><span class='desc_info'>Content Category:</span>" + target_info.content_category + "</div></li>";	
	
	code += "</ul>";
	document.getElementById('detail_info').innerHTML = code;
	
	if(target_info.category_type == "Parental Controls"){
		code_title = "<div class='er_title' style='height:auto;'>The web page category is filtered</div>"
		code_suggestion = "<ul>";
		code_suggestion += "<li><span>If you are a manager and still want to visit this website, please go to <a href='AiProtection_WebProtector.asp'>Parental Controls</a> for configuration change.</span></li>";
		code_suggestion += "<li><span>If you are a client and have any problems, please contact your manager.</span></li>";		
		code_suggestion += "</ul>";
	}
	else{
		code_title = "<div class='er_title' style='height:auto;'>Warning! The website contains malware. Visiting this site may harm your computer</div>"
		code_suggestion = "<ul>";
		code_suggestion += "If you are a manager and want to disable this protection, please go to <a href='AiProtection_HomeProtection.asp'>Home Protection</a> for configuration";
		code_suggestion += "</ul>";
	}
	
	document.getElementById('page_title').innerHTML = code_title;
	document.getElementById('suggestion').innerHTML = code_suggestion;
}
</script>
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
		<th valign="top" style="height:40px;" id="page_title"></th>
	</tr>
	<tr>
		<td  valign="top" class="erpage_td">
			<span class="Epagetitle">Detailed informations:</span>	  
			<div id="detail_info" class="Epagecontent" style="color:red"></div>	  
			<div class="Epagetitle">
				<div>
					<span><#web_redirect_suggestion0#>:</span>
				</div>		
			</div>
			<div class="Epagecontent">
				<div id="suggestion"></div>
			</div>
			<div class="Epagecontent" style="margin-top:20px;display:none;">
				<table>
					<tr>
						<td>
							<img src="images/New_ui/TM_product.png">
						</td>						
						<td>
							<div>
								For your client side advanced internet security protection. Trend Micro offer you more advanced home security solution. Please <a href="">visit the site</a> for free trial or online scan service.
							</div>	
						</td>
					</tr>
				</table>
			</div>
		</td>
	</tr>
	<tr>
		<td height="22" class="erpage_footer"><span></span></td>
	</tr>
</table>
</body>
</html>
