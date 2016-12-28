// JavaScript Document
var winH,winW;
		
function winW_H(){
	if(parseInt(navigator.appVersion) > 3){
		winW = document.documentElement.scrollWidth;
		if(document.documentElement.clientHeight > document.documentElement.scrollHeight)
			winH = document.documentElement.clientHeight;
		else
			winH = document.documentElement.scrollHeight;
	}
} 

function LoadingTime(seconds, flag){
	showtext(document.getElementById("proceeding_main_txt"), "<#Main_alert_proceeding_desc1#>...");
	document.getElementById("Loading").style.visibility = "visible";
	
	y = y+progress;
	if(typeof(seconds) == "number" && seconds >= 0){
		if(seconds != 0){
			/* trigger IE8 counter */
			if(navigator.appName.indexOf("Microsoft") >= 0){
				var childsel=document.createElement("span");
				document.body.appendChild(childsel);
   			document.body.removeChild(document.body.lastChild);
			}

			showtext(document.getElementById("proceeding_main_txt"), "<#Main_alert_proceeding_desc4#>");
			showtext(document.getElementById("proceeding_txt"), Math.round(y)+"% <#Main_alert_proceeding_desc1#>");
			--seconds;
			setTimeout("LoadingTime("+seconds+", '"+flag+"');", 1000);
		}
		else{
			showtext(document.getElementById("proceeding_main_txt"), translate("<#Main_alert_proceeding_desc3#>"));
			showtext(document.getElementById("proceeding_txt"), "");
			y = 0;
			
			if(flag != "waiting")
				setTimeout("hideLoading();",1000);			
		}
	}
}

function LoadingProgress(seconds){
	document.getElementById("LoadingBar").style.visibility = "visible";
	
	y = y + progress;
	if(typeof(seconds) == "number" && seconds >= 0){
		if(seconds != 0){
			document.getElementById("proceeding_img").style.width = Math.round(y) + "%";
			document.getElementById("proceeding_img_text").innerHTML = Math.round(y) + "%";
	
			if(document.getElementById("loading_block1")){
				document.getElementById("proceeding_img_text").style.width = document.getElementById("loading_block1").clientWidth;
				document.getElementById("proceeding_img_text").style.marginLeft = "175px";
			}	
			--seconds;
			setTimeout("LoadingProgress("+seconds+");", 1000);
		}
		else{
			document.getElementById("proceeding_img_text").innerHTML = "<#Main_alert_proceeding_desc3#>";
			y = 0;
			if(location.pathname.indexOf("Advanced_MobileBroadband_Content") > 0){
				setTimeout("hideLoadingBar();",1000);
				htmlbodyforIE = document.getElementsByTagName("html");
				htmlbodyforIE[0].style.overflow = "";
			}
			else if(location.pathname.indexOf("QIS_wizard.htm") < 0 && location.pathname.indexOf("Advanced_FirmwareUpgrade_Content") < 0 && location.pathname.indexOf("Advanced_SettingBackup_Content") < 0){
				setTimeout("hideLoadingBar();",1000);
				location.href = "index.asp";
			}
		}
	}
}

function showLoading(seconds, flag){
	if(window.scrollTo)
		window.scrollTo(0,0);

	disableCheckChangedStatus();
	
	htmlbodyforIE = document.getElementsByTagName("html");  //this both for IE&FF, use "html" but not "body" because <!DOCTYPE html PUBLIC.......>
	htmlbodyforIE[0].style.overflow = "hidden";	  //hidden the Y-scrollbar for preventing from user scroll it.
	
	winW_H();
	
	var blockmarginTop;
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
	
	if (window.innerHeight)
		winHeight = window.innerHeight;
	else if ((document.body) && (document.body.clientHeight))
		winHeight = document.body.clientHeight;
	
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winHeight = document.documentElement.clientHeight;
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){
	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.35)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.35+document.body.scrollLeft;	

	}
	
	if(winHeight >660)
		winHeight = 660;
	
	blockmarginTop= winHeight*0.3	
	
	document.getElementById("loadingBlock").style.marginTop = blockmarginTop+"px";
	document.getElementById("loadingBlock").style.marginLeft = blockmarginLeft+"px";

	document.getElementById("Loading").style.width = winW+"px";
	document.getElementById("Loading").style.height = winH+"px";
	
	loadingSeconds = seconds;
	progress = 100/loadingSeconds;
	y = 0;
	
	LoadingTime(seconds, flag);
}

function showLoadingBar(seconds){
	if(window.scrollTo)
		window.scrollTo(0,0);

	disableCheckChangedStatus();
	
	htmlbodyforIE = document.getElementsByTagName("html");  //this both for IE&FF, use "html" but not "body" because <!DOCTYPE html PUBLIC.......>
	htmlbodyforIE[0].style.overflow = "hidden";	  //hidden the Y-scrollbar for preventing from user scroll it.
	
	winW_H();

	var blockmarginTop;
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
	
	if (window.innerHeight)
		winHeight = window.innerHeight;
	else if ((document.body) && (document.body.clientHeight))
		winHeight = document.body.clientHeight;

	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winHeight = document.documentElement.clientHeight;
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){
	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.3)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.3+document.body.scrollLeft;	

	}
	
	if(winHeight >660)
		winHeight = 660;
	
	blockmarginTop= winHeight*0.3			
	
	document.getElementById("loadingBarBlock").style.marginTop = blockmarginTop+"px";
	// marked by Jerry 2012.11.14 using CSS to decide the margin
	document.getElementById("loadingBarBlock").style.marginLeft = blockmarginLeft+"px";

	
	/*blockmarginTop = document.documentElement.scrollTop + 200;
	document.getElementById("loadingBarBlock").style.marginTop = blockmarginTop+"px";*/

	document.getElementById("LoadingBar").style.width = winW+"px";
	document.getElementById("LoadingBar").style.height = winH+"px";
	
	loadingSeconds = seconds;
	progress = 100/loadingSeconds;
	y = 0;
	LoadingProgress(seconds);
}

function hideLoadingBar(){
	document.getElementById("LoadingBar").style.visibility = "hidden";
}

function hideLoading(flag){
	document.getElementById("Loading").style.visibility = "hidden";
	htmlbodyforIE = document.getElementsByTagName("html");  //this both for IE&FF, use "html" but not "body" because <!DOCTYPE html PUBLIC.......>
	htmlbodyforIE[0].style.overflow = "";	  //hidden the Y-scrollbar for preventing from user scroll it.
}             

function simpleSSID(obj){
	var SSID = document.loginform.wl_ssid.value;
	
	if(SSID.length < 16)
		showtext(obj, SSID);
	else{
		obj.title = SSID;
		showtext(obj, SSID.substring(0, 16)+"...");
	}
}

function dr_advise(){
	disableCheckChangedStatus();
	
	htmlbodyforIE = document.getElementsByTagName("html");  //this both for IE&FF, use "html" but not "body" because <!DOCTYPE html PUBLIC.......>
	htmlbodyforIE[0].style.overflow = "hidden";	  //hidden the Y-scrollbar for preventing from user scroll it.
	
	winW_H();
	var blockmarginTop;
	blockmarginTop = document.documentElement.scrollTop + 200;	
	document.getElementById("dr_sweet_advise").style.marginTop = blockmarginTop+"px"
	document.getElementById("hiddenMask").style.width = winW+"px";
	document.getElementById("hiddenMask").style.height = winH+"px";	
	document.getElementById("hiddenMask").style.visibility = "visible";
}

function cancel_dr_advise(){	
	parent.document.getElementById("hiddenMask").style.visibility = "hidden";
	htmlbodyforIE = parent.document.getElementsByTagName("html");  //this both for IE&FF, use "html" but not "body" because <!DOCTYPE html PUBLIC.......>
	htmlbodyforIE[0].style.overflow = "scroll";	  //hidden the Y-scrollbar for preventing from user scroll it.	
	window.scrollTo(0, 0);	//x-axis , y-axis	
}
