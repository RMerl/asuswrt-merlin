<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Blocking Page</title>  
<script type="text/JavaScript" src="/js/jquery.js"></script>                       
<style>
body{
	color:#FFF;
	font-family: Arial;
}
.wrapper{
	background:url(images/New_ui/login_bg.png) #283437 no-repeat;
	background-size: 1280px 1076px;
	background-position: center 0%;
	margin: 0px; 
}
.title_name {
	font-size: 32px;
	color:#93D2D9;
}
.title_text{
	width:520px;
}
.prod_madelName{
	font-size: 26px;
	color:#fff;
	margin-left:78px;
	margin-top: 10px;
}
.login_img{
	width:43px;
	height:43px;
	background-image: url('images/New_ui/icon_titleName.png');
	background-repeat: no-repeat;
}
.p1{
	font-size: 16px;
	color:#fff;
	width: 480px;
}
.button{
	background-color:#007AFF;
	/*background:rgba(255,255,255,0.1);
	border: solid 1px #6e8385;*/
	border-radius: 4px ;
	transition: visibility 0s linear 0.218s,opacity 0.218s,background-color 0.218s;
	height: 68px;
	width: 300px;
	font-size: 28px;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	text-align: center;
	float:right; 
	margin:25px 0 0 78px;
	line-height:68px;
}
.form_input{
	background-color:rgba(255,255,255,0.2);
	border-radius: 4px;
	padding:26px 22px;
	width: 480px;
	border: 0;
	height:25px;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	font-size:28px
}
.nologin{
	margin:10px 0px 0px 78px;
	background-color:rgba(255,255,255,0.2);
	padding:20px;
	line-height:36px;
	border-radius: 5px;
	width: 600px;
	border: 0;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	font-size: 18px;
}
.div_table{
	display:table;
}
.div_tr{
	display:table-row;
}
.div_td{
	display:table-cell;
}
.title_gap{
	margin:10px 0px 0px 78px;
}
.img_gap{
	padding-right:30px;
	vertical-align:middle;
}
.password_gap{
	margin:30px 0px 0px 78px;
}
.error_hint{
	color: rgb(255, 204, 0);
	margin:10px 0px -10px 78px; 
	font-size: 18px;
	font-weight: bolder;
}
.main_field_gap{
	margin:100px auto 0;
}
ul{
	margin: 0;
}
li{
	margin: 10px 0;
}
#wanLink{
	cursor: pointer;
}
.button_background{
	background-color: transparent;
}
.tm_logo{
	background:url('images/New_ui/tm_logo_1.png');
	width:268px;
	height:48px;
}
.desc_info{
	font-weight:bold;
}
#tm_block{
	margin: 0 20px;
}
/*for mobile device*/
@media screen and (max-width: 1000px){
	.title_name {
		font-size: 1.2em;
		color:#93d2d9;
		margin-left:15px;
	}
	.prod_madelName{
		font-size: 1.2em;
		margin-left: 15px;
	}
	.p1{
		font-size: 16px;
		width:100%;
	}
	.login_img{
		background-size: 75%;
	}
	.title_text{
		width: 100%;
	}
	.form_input{	
		padding:13px 11px;
		width: 100%;
		height:25px;
		font-size:16px
	}
	.button{
		height: 50px;
		width: 100%;
		font-size: 16px;
		text-align: center;
		float:right; 
		margin: 25px -30px 0 0;
		line-height:50px;
		padding: 0 10px;
	}
	.nologin{
		margin-left:10px; 
		padding:5px;
		line-height:18px;
		width: 100%;
		font-size:14px;
	}
	.error_hint{
		margin-left:10px; 
	}
	.main_field_gap{
		width:80%;
		margin:30px 0 0 15px;
		/*margin:30px auto 0;*/
	}
	.title_gap{
		margin-left:15px; 
	}
	.password_gap{
		margin-left:15px; 
	}
	.img_gap{
		padding-right:0;
		vertical-align:middle;
	}
	ul{
		margin-left:-20px;
	}
	li{
		margin: 10px 0;
	}
	.tm_logo{
		width: 225px;
		background-repeat: no-repeat;
		background-size: 100%;
	}
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
		target_info.desc = "This device is blocked to access the Internet at this time";//untranslated string
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
		code_title = "<div class='er_title' style='height:auto;'>The web page category is filtered</div>"//untranslated string
		code_suggestion = "<ul>";
		code_suggestion += "<li><span>If you are a manager and still want to visit this website, please go to for configuration change.</span></li>";//untranslated string
		code_suggestion += "<li><span>If you are a client and have any problems, please contact your manager.</span></li>";		//untranslated string
		code_suggestion += "</ul>";
		document.getElementById('tm_block').style.display = "";
		$("#go_btn").click(function(){
			location.href = "AiProtection_WebProtector.asp";
		});
		document.getElementById('go_btn').style.display = "";
	}
	else if(target_info.category_type == "Home Protection"){
		code_title = "<div class='er_title' style='height:auto;'>Warning! The website contains malware. Visiting this site may harm your computer</div>"//untranslated string
		code_suggestion = "<ul>";
		code_suggestion += "If you are a manager and want to disable this protection, please go to Home Protection for configuration";//untranslated string
		code_suggestion += "</ul>";
		document.getElementById('tm_block').style.display = "";
		$("#go_btn").click(function(){
			location.href = "AiProtection_HomeProtection.asp";
		});
		document.getElementById('go_btn').style.display = "";
	}
	else{		//for Parental Control(Time Scheduling)
		code_title = "<div class='er_title' style='height:auto;'>Warning! The device can't access the Internet now.</div>"
		code_suggestion = "<ul>";
		if(bwdpi_support)
			parental_string = "<#Time_Scheduling#>";
		else
			parental_string = "<#Parental_Control#>";

		code_suggestion += "<li>If you are a manager and want to access the Internet, please go to "+ parental_string +" for configuration change.</li>";	//untranslated string
		code_suggestion += "<li>If you are a client and have any problems, please contact your manager.</li>";//untranslated string
		code_suggestion += "</ul>";
		$("#go_btn").click(function(){
			location.href = "ParentalControl.asp";
		});
		document.getElementById('go_btn').style.display = "";
		document.getElementById('tm_block').style.display = "none";
	}

	document.getElementById('page_title').innerHTML = code_title;
	document.getElementById('suggestion').innerHTML = code_suggestion;
}
</script>
</head>
<body class="wrapper" onload="initial();">
	<div class="div_table main_field_gap">
		<div class="title_name">
			<div class="div_td img_gap">
				<div class="login_img"></div>
			</div>
			<div id="page_title" class="div_td title_text"></div>
		</div>		
		<div class="prod_madelName"><#Web_Title2#></div>
		
		<div class="p1 title_gap">Detailed informations:</div><!--untranslated string-->
		<div ></div>	
		<div>
			<div class="p1 title_gap"></div>
			<div class="nologin">
				<div id="detail_info"></div>
			</div>
		</div>
		
		<div class="p1 title_gap"><#web_redirect_suggestion0#></div>	
		<div>
			<div class="p1 title_gap"></div>
			<div class="nologin">
				<div id="case_content"></div>
				<div id="suggestion"></div>
				<div id="tm_block" style="display:none">
					<div>For your client side advanced internet security protection. Trend Micro offer you more advanced home security solution. Please <a href="http://www.trendmicro.com" target="_blank">visit the site</a> for free trial or online scan service.</div>
					<!--untranslated string-->
					<div class="tm_logo"></div>
				</div>
			</div>
		</div>
		<div id="go_btn" class='button' style="display:none;"><#btn_go#></div>		
	</div>
</body>
</html>