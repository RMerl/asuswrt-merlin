var g_support_html5 = 0;
var g_reload_page = 1;
var m = new lang();
var g_storage = new myStorage();
var g_listview = (g_storage.get('listview') == undefined) ? 0 : g_storage.get('listview');
var g_modal_url;
var g_modal_window_width = 200;
var g_modal_window_height = 80;
var timer_idle;
var g_show_modal = 0;
var g_time_out = 900000; //- 15min
var g_time_count = 0;
var g_select_mode = 0;
var g_select_array;
var g_upload_mode = 0;
var g_folder_array;
var g_file_array;
var g_opening_uid = '';
var g_on_button_animation = 0;
var g_on_rescan_samba = 0;
var g_rescan_samba_timer = 0;
var g_rescan_samba_count = 0;
var g_aidisk_name = "usbdisk";
var g_enable_aidisk = 0;
var g_support_lan = new Array('en-us', 'zh-tw', 'zh-cn', 'cz', 'pl', 'ru', 'de', 'fr', 'tr', 'th', 'ms', 'no', 'fi', 'da', 'sv', 'br', 'jp', 'es', 'it', 'uk');
var g_bInitialize = false;

var g_showAudioList = false;
var g_jplayer_solution = "html";
var g_jplayer_supplied = "mp3";
var g_audio_playlist = [];
var g_current_index = 0;

var g_timer_filelist;

var newMenu = null; //variable to store menu instance

var client = new davlib.DavClient();
client.initialize();
	
// Check for the various File API support.
if (window.File && window.FileReader && window.FileList && window.Blob) {
	// Great success! All the File APIs are supported.
  	g_support_html5 = 1;
} else {
	g_support_html5 = 0;
}

function openImageViewer(loc){
	
	var image_array = new Array();
	for(var i=0;i<g_file_array.length;i++){
   	var file_ext = getFileExt(g_file_array[i].href);			
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif")
   		image_array.push(g_file_array[i]);
	}
	
	var image_array_count = image_array.length;
	
	if(image_array_count==0){
		alert(m.getString("msg_no_image_list"));
		return;
	}
	
	var default_index = 0;
	for(var i=0;i<image_array.length;i++){
   		if( loc == image_array[i].href )
   			default_index = i;
  	}
		
	var div_html='';
	var page_size = getPageSize();
	
	div_html += '<div id="image_slide_show" class="barousel unselectable" style="height: 0; width: 0; position: fixed; background-color: rgb(0, 0, 0); left: ' + page_size[0]/2 + 'px; top: ' + page_size[1]/2 + 'px; z-index: 2999;">';
	div_html += '<div class="barousel_image">';
	
	for(var i=0;i<image_array.length;i++){
				
   		var img_url = window.location.protocol + "//" + window.location.host + image_array[i].href;
    	
   		if(i==default_index)
   			div_html += '<img src="" path="' + img_url + '" alt="" class="default"/>';
   		else
     		div_html += '<img src="" path="' + img_url + '" alt="" class=""/>';
	}
	
	div_html += '</div>';
	
	div_html += '<div class="barousel_nav">';
	
	div_html += '<div class="barousel_content transparent" style="display: block; ">';
		
	for(var i=0;i<image_array.length;i++){
		div_html += '<div class="';
				
   		if(i==default_index)
     		div_html += 'default';
        	
    	div_html += '" style="display: none; ">';
        
    	div_html += '<p class="header">' + image_array[i].name + '</p>';
    	div_html += '</div>';
	}
		           
	div_html += '</div>';
  
  	div_html += '</div>';
  
  	div_html += '<div class="barousel_loading" style="position:absolute;display:none;z-index=99;"><img src="/smb/css/load.gif" width="18px" height="18px"/></div>';
                  
	div_html += '</div>';
		
	$(div_html)
		.animate({width:"100%", height:"100%", left:"0px", top:"0px"},200, null, null )
  		.appendTo("body");
  
	var close_handler = function(){
	  	self.file_array = null;
		self.settings = null;
	};
	  	
	var init_handler = function(settings){
		self.settings = settings;
	};
	
	$('#image_slide_show').barousel({
		name: 'image_slide_show',
		manualCarousel: 1,
		contentResize:0,
		startIndex:default_index,
		storage: g_storage,
		stringTable: m,
		enableExifFunc: 0,
		enableShareFunc: 0,
		closeHandler: null,
		initCompleteHandler: null
	});
	/*	
	$('#image_slide_show').barousel({				
		navType: 2,
	    manualCarousel: 1,
	    contentResize:0,
	    startIndex:default_index
	});
	*/
	
	image_array = null;
}

function openDialog(effect, role_target){
	$.mobile.changePage('#dialog', {transition: effect, role: role_target});
}

function closeDialog(){	
	$.mobile.changePage('#mainpage', "popup", true, true);
}

function openLoginWindow(open_url){
	var dialog_title = "";
	var dologin = function(){
		var user = $('#table_content input#username').val();
		var pass = $('#table_content input#password').val();
		var auth = "Basic " + Base64.encode(user + ":" + pass);		
		closeDialog();		
		doLOGIN(open_url, auth);
	};
	
	if(open_url=='/')
		dialog_title = m.getString('title_login') + " - AiCloud";
	else
		dialog_title = m.getString('title_login') + " - " + open_url.substring(0, 35);
	
	$("#dialog").remove();
	
	var dialog_html = '';
	dialog_html += "<div data-role='dialog' id='dialog' data-theme='a' data-transition='flip'>";
  	dialog_html += "<div data-role='header'>";  
  	dialog_html += "<h1 id='dialog-title'>" + dialog_title + "</h1>";  
  	dialog_html += "</div>";
 	dialog_html += "<div data-role='content' id='dialog-content'>";
  	
	dialog_html += '<table id="table_content" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;">';
	dialog_html += '<tr>';
  	dialog_html += '<td>';
  	dialog_html += '<form name="form_login">';
	dialog_html += '<fieldset>';
	dialog_html += '<table width="100%">';
	dialog_html += '<tr>';
	dialog_html += '<td width="80px"><label id="username">' + m.getString('title_username') + ':</label></td><td><input name="username" type="text" id="username" size="35" autocapitalize="off"></td>';
	dialog_html += '</tr>';
	dialog_html += '<tr>';
	dialog_html += '<td width="80px"><label id="password">' + m.getString('title_password') + ':</label></td><td><input name="password" type="password" id="password" size="35"></td>';
	dialog_html += '</tr>';
	dialog_html += '</table>';
	dialog_html += '</fieldset>';
	dialog_html += '</form>';
  	dialog_html += '</td>';
 	dialog_html += '</tr>';
  	dialog_html += '<tr style="height:10px"></tr>';
  	dialog_html += '<tr>';
  	dialog_html += '<td>';
  	dialog_html += '<div class="table_block_footer" style="text-align:right">';
  	dialog_html += '<button id="ok" class="btnStyle">' + m.getString('btn_ok') + '</button>';
  	dialog_html += '<button id="cancel" class="btnStyle">' + m.getString('btn_cancel') + '</button>';
  	dialog_html += '</div>';
  	dialog_html += '</td>';
  	dialog_html += '</tr>';
	dialog_html += '</table>';
	
	dialog_html += "</div>";
	dialog_html += "</div>";
	
	$("body").append(dialog_html);
		
	$('#table_content input#password').keydown(function(e){
		if(e.keyCode==13){
			dologin();
		}
	});
	
	$("#table_content #ok").click(function(){
		dologin();
	});
	
	$("#table_content #cancel").click(function(){
		closeDialog();
	});
	
	openDialog("pop", "dialog");
}

function openShareWindow(open_url){
	var dialog_title = "";
		
	$("#dialog").remove();
	
	var share_array = open_url.split(";");
	
	var dialog_html = '';
	dialog_html += "<div data-role='dialog' id='dialog' data-theme='a' data-transition='flip'>";
  	dialog_html += "<div data-role='header'>";  
  	dialog_html += "<h1 id='dialog-title'>" + m.getString('btn_sharelink') + "</h1>";  
  	dialog_html += "</div>";
 	dialog_html += "<div data-role='content' id='dialog-content'>";
  	
	dialog_html += '<table id="table_content" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;">';
	dialog_html += '<tr>';
  	dialog_html += '<td>';
  
  	dialog_html += '<textarea id="share_link">';
  	for(var i=0; i<share_array.length; i++){
		dialog_html += decodeURI(share_array[i]);
  		dialog_html += "\n\n";
	}
  	dialog_html += '</textarea>';
  
  	dialog_html += '</td>';
  	dialog_html += '</tr>';
	dialog_html += '<tr>';
  	dialog_html += '<td>';
  	dialog_html += '<div class="table_block_footer" style="text-align:right">';
  	dialog_html += '<button id="email-sharelink" class="btnStyle">' + m.getString("title_share_by_email") + '</button>';
  	//dialog_html += '<button id="sms-sharelink" class="btnStyle">Share by SMS</button>';
  	//dialog_html += '<button id="copy-sharelink" class="btnStyle">Copy Share</button>';
  	dialog_html += '<button id="cancel" class="btnStyle">' + m.getString('btn_cancel') + '</button>';
	dialog_html += '</div>';
	dialog_html += '</td>';
  	dialog_html += '</tr>';
	dialog_html += '</table>';
	
	dialog_html += "</div>";
	dialog_html += "</div>";
	
	$("body").append(dialog_html);
	
	$("#table_content #email-sharelink").click(function(){
		
		var sMailTo = "mailto:";
		var sBody = "";
		var newline = "%0D%0A%0D%0A";
		
		sBody += m.getString('msg_sharelink_desc1');
		sBody += newline;
		
		for(var i=0; i<share_array.length; i++){
	  		sBody += "<a href='" + share_array[i] + "'>" + share_array[i] + "</a>";
	  		sBody += newline;
		}
		
		sBody += m.getString('msg_sharelink_desc2');
		
		sMailTo += "?subject=" + m.getString('email_subject') + "&body=" + sBody;
        
		document.location.href = sMailTo;
	});
	
	$("#table_content #sms-sharelink").click(function(){
		
		var sSMSTo = "sms:";
		var sBody = "";
		var newline = "%0D%0A%0D%0A";
		
		sBody += m.getString('msg_sharelink_desc1');
		sBody += newline;
		
		for(var i=0; i<share_array.length; i++){
	  		sBody += "<a href='" + share_array[i] + "'>" + share_array[i] + "</a>";
	  		sBody += newline;
		}
		
		sBody += m.getString('msg_sharelink_desc2');
		
		sSMSTo += "?subject=" + m.getString('email_subject') + "&body=" + sBody;
        
		document.location.href = sSMSTo;
	});
	
	$("textarea#share_link").click(function(){
		$(this).select();
	});
	
	$("#table_content #cancel").click(function(){
		closeDialog();
	});
	
	openDialog("pop", "dialog");
}

function getLatestVersion(){
	
	if(client==null)
		return;
		
	$("#update").text(m.getString('msg_check_latest_ver'));
	
	client.GETLATESTVER("/", function(error, content, statusstring){	
		if(error==200){
			var data = parseXml(statusstring);
			var x = $(data);
			
			var ver = x.find("version").text();
			var a = ver.split("_");
			var build_no = a[1];
			
			var cur_ver = g_storage.get('router_version');
			var b = cur_ver.split(".");
			var cur_build_no = b[3];
			
			if(build_no>cur_build_no)
				$("#update").text(m.getString('msg_update_latest_ver'));
			else
				$("#update").text(m.getString('msg_latest_ver'));
		}
		else{
			$("#update").text(m.getString('msg_check_latest_ver_error'));
		}
	});
}

function refreshShareLinkList(){
	if(client==null)
		return;
		
	client.GSLL("/", function(error, content, statusstring){	
		if(error==200){
			var data = parseXml(statusstring);
			
			$(".sharelink").empty();
			
			var encode_filename = parseInt($(data).find('encode_filename').text());
			
			var table_html = ""
			
			var i = 0;
			$(data).find('sharelink').each(function(){
				
				var filename = "";
				var filetitle = "";
					
				if(encode_filename==1){
					filename = $(this).attr("filename");				
					filetitle = decodeURIComponent(filename);
				}
				else{
					filetitle = $(this).attr("filename");				
					filename = encodeURIComponent($(this).attr("filename"));
				}
				
				var url = window.location.protocol + "//" + window.location.host + "/" + $(this).attr("url") + "/" + filename;
				var createtime = $(this).attr("createtime");
				var expiretime = $(this).attr("expiretime");
				var lefttime = parseFloat($(this).attr("lefttime"));
				var hour = parseInt(lefttime/3600);
				var minute = parseInt(lefttime%3600/60);
				
				table_html = "<li><a href='#' data-icon='delete'>";
				table_html += "<h3>" + filetitle + "</h3>";
				//table_html += "<a id='rescan' data-role='button'>test</a>";
			
				table_html += "<p class='ui-li-desc'>";
				table_html += m.getString('table_createtime') + ": ";
				table_html += createtime;
				table_html += "</p>";
				
				table_html += "<p class='ui-li-desc'>";
				table_html += m.getString('table_expiretime') + ": ";
				if(expiretime==0)
       				table_html += m.getString('title_unlimited');
      			else
					table_html += expiretime;
				table_html += "</p>";
				
				table_html += "<p class='ui-li-desc'>";
				table_html += m.getString('table_lefttime') + ": ";
				if(expiretime==0)       		
       				table_html += m.getString('title_unlimited');
      			else
       				table_html += hour + " hours " + minute + " mins";
      	
				table_html += "</p>";
				
				table_html += "</a><a href='#' class='dellink' link='" + $(this).attr("url") + "'></a></li>";
				
				$("ul.sharelink").append(table_html).find("li:last").hide();    	
 	 			$('ul.sharelink').listview('refresh');  
  				$("ul.sharelink").find("li:last").slideDown(300);  
			});
			
			$(".dellink").click(function(){				
				var r=confirm(m.getString('msg_confirm_delete_sharelink'));				
				if (r==true){					
					client.REMOVESL("/", $(this).attr("link"), function(error, content, statusstring){	
						if(error==200){
							refreshShareLinkList();
						}
						else
							alert("Fail to delete sharelink!");
					});
				}
			});
		}
	});
}

function openSettingWindow(){
	var dialog_title = "";
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
		
	$("#dialog").remove();
	
	var dialog_html = '';
	dialog_html += "<div data-role='page' id='dialog' data-theme='a'>";
  
  	dialog_html += "<div data-role='header'>";  
  	dialog_html += "<h1 id='dialog-title'>" + m.getString('title_setting') + "</h1>"; 
  	dialog_html += "<a id='back' data-role='button'>" + m.getString("btn_prevpage") + "</a>";  
  	dialog_html += "</div>";
  
 	dialog_html += "<div data-role='content' id='dialog-content' class='setting-set'>";
 	  
	dialog_html += "<div data-role='collapsible-set'>";
	
	//- ShareLink manager
	dialog_html += "<div data-role='collapsible' data-collapsed='true'>";
	dialog_html += "<h2>" + m.getString('title_sharelink') + "</h2>";
	dialog_html += "<ul data-role='listview' data-inset='true' class='sharelink' data-theme='a' data-split-theme='a' data-split-icon='delete'></ul>";
	dialog_html += "</div>";
	
	//- Version
	dialog_html += "<div data-role='collapsible'>";
	dialog_html += "<h2>" + m.getString('title_version') + "</h2>";
	dialog_html += "<span id='version'>AiCloud " + m.getString('title_version') + ": " + g_storage.get('aicloud_version') + "</span>";
	dialog_html += "<br><span id='version'>FW " + m.getString('title_version') + ": " + g_storage.get('router_version') + "</span>";
	dialog_html += "<br><span id='update' style='color:red'></span>";
	dialog_html += "</div>";
	
	//- Language
	dialog_html += "<div data-role='collapsible'>";
	dialog_html += "<h2>" + m.getString('title_language') + "</h2>";
	dialog_html += "<div data-role='controlgroup'>";	
	for( var i = 0; i < g_support_lan.length; i++ ){
  		dialog_html += "<label for='" + g_support_lan[i] + "'>" + m.getString('lan_' + g_support_lan[i]) + "</label>";	
  		dialog_html += "<input type='radio' id='" + g_support_lan[i] + "' name='group_lan' value='" + g_support_lan[i] + "'";  
  	
  		if(g_support_lan[i] == lan){
  			dialog_html += " checked";
  		}
  	
  		dialog_html += ">";
	}		
	dialog_html += "</div>";
	dialog_html += "</div>";
	
	//- Rescan samba
	dialog_html += "<a id='rescan' data-role='button'>" + m.getString('title_rescan') + "</a>";
	
	//- Desktop View
	dialog_html += "<a id='desktop_view' data-role='button'>" + m.getString('title_desktop_view') + "</a>";
	
	dialog_html += "</div>";
	
	dialog_html += "</div>";
	dialog_html += "</div>";
	
	$("body").append(dialog_html);
	
	//getLatestVersion();
	refreshShareLinkList();
	
	$("#rescan").click(function(){
		var r=confirm(m.getString('title_desc_rescan'));
			
		if (r==true)
			doRescanSamba();
	});
	
	$("#desktop_view").click(function(){
		var url = window.location.href;
		url = url.substr(0, url.lastIndexOf("?"));
		window.location = url + '?desktop=1';
	});
	
	$("#back").click(function(){
		closeDialog();
	});
	
	$("input[type=radio]").click(function(){		
		var lan = $(this).attr("id");		
		g_storage.set('lan', lan);
		window.location.reload();
	});
	
	openDialog("flip", "page");
}

function showHideAudioList(bshow){
	if(bshow){
		$(".jp-playlist").css("display", "block");
	}
	else{		
		$(".jp-playlist").css("display", "none");
	}
	
	g_showAudioList = bshow;
}

function showHideAudioInterface(bshow){
	if(bshow){
		$("#jp_interface_1").css("display","block");
	}
	else{
		$("#jp_interface_1").css("display","none");
	}
}

function openAudioPlayer(loc){
	
	var audio_array = new Array();
	for(var i=0;i<g_file_array.length;i++){
   		var file_ext = getFileExt(g_file_array[i].href);			
		if(file_ext=="mp3"){
   			audio_array.push(g_file_array[i]);
   		}
	}
	
	var audio_array_count = audio_array.length;
	
	if(audio_array_count==0){
		alert(m.getString("msg_no_image_list"));
		return;
	}
	
	showHideLoadStatus(true);
	
	var this_audio_list = [];
	var this_audio = loc;
	g_current_index = 0;
	for(var i=0;i<audio_array.length;i++){
   	
		var the_loc = audio_array[i].href;
				
		if( loc == the_loc )
			g_current_index = i;
		   			
		this_audio_list.push(the_loc);
	}
	
	var audio_array_count = this_audio_list.length;
	
	if(audio_array_count==0){
		alert(m.getString("msg_no_image_list"));
		return;
	}
	
	g_audio_playlist = [];
	
	$("#audio_dialog").remove();
	
	var audio_html = "";
	audio_html += '<div data-role="dialog" id="audio_dialog">';
	audio_html += '<table width="100%" border="0" align="center" cellpadding="0" cellspacing="0">';
	audio_html += '<tr style="height:180px">';
  	audio_html += '<td>';
	audio_html += '<div id="container" style="width:300px;margin:auto">';
	audio_html += '<div id="jquery_jplayer_1" class="jp-jplayer"></div>';
	audio_html += '<div id="jp_container_1" class="jp-audio">';
	audio_html += '<div class="jp-type-playlist">';
	audio_html += '<div id="jp_title_1" class="jp-title">';
	audio_html += '<ul>';
	audio_html += '<li></li>';
	audio_html += '</ul>';
	audio_html += '</div>';
	audio_html += '<div id="jp_interface_1" class="jp-gui jp-interface">';
	audio_html += '<ul class="jp-controls">';
	audio_html += '<li><a href="javascript:;" class="jp-previous" tabindex="1">previous</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-play" tabindex="1" style="display: block; ">play</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-pause" tabindex="1" style="display: block; ">pause</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-next" tabindex="1">next</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-stop" tabindex="1">stop</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-mute" tabindex="1" title="mute">mute</a></li>';
	audio_html += '<li><a href="javascript:;" class="jp-volume-max" tabindex="1" title="max volume">max volume</a></li>';
	audio_html += '</ul>';
	audio_html += '<div class="jp-progress">';
	audio_html += '<div class="jp-seek-bar" style="width: 100%; ">';
	audio_html += '<div class="jp-play-bar" style="width: 46.700506341691536%;"></div>';
	audio_html += '</div>';
	audio_html += '</div>';
	audio_html += '<div class="jp-volume-bar">';
	audio_html += '<div class="jp-volume-bar-value" style="width: 31.979694962501526%; "></div>';
	audio_html += '</div>';
	audio_html += '<div class="jp-duration">00:00</div>';
	audio_html += '<div class="jp-split">/</div>';
	audio_html += '<div class="jp-current-time">00:00</div>';
	audio_html += '</div>';
												
	audio_html += '<div id="jp_playlist_1" class="jp-playlist">';
	audio_html += '<ul></ul>';
 	audio_html += '</div>';
            
	audio_html += '<div class="jp-no-solution" style="display: none; ">';
	audio_html += '<span>Update Required</span>';
	audio_html += 'To play the media you will need to either update your browser to a recent version or update your <a href="http://get.adobe.com/flashplayer/" target="_blank">Flash plugin</a>.';
	audio_html += '</div>';
						
	audio_html += '</div>';
	audio_html += '</div>';
	audio_html += '</div>';
  	audio_html += '</td>'; 
  	audio_html += '</tr>';
  	audio_html += '<tr style="height:10px"></tr>';
  	audio_html += '<tr>';
 	audio_html += '<td>';
 	audio_html += '<div class="table_block_footer" style="text-align:right">';
  	audio_html += '<button id="playlist" class="btnStyle">PlayList</button>';
  	audio_html += '<button id="cancel" class="btnStyle">Close</button>';
  	audio_html += '</div>';
	audio_html += '</td>';
  	audio_html += '</tr>';
	audio_html += '</table>';
	audio_html += '</div>';
	
	$("body").append(audio_html);
	
	$("#jp_title_1 li").text("Loading...");
	$("#playlist").css("visibility","hidden");
	
	$("#audio_dialog #playlist").click(function(){
		showHideAudioList(!g_showAudioList);	
	});
	
	$("#audio_dialog #cancel").click(function(){
		$("#jquery_jplayer_1").jPlayer("stop");
		$("#jquery_jplayer_1").jPlayer("clearMedia");
		$.mobile.changePage('#mainpage', 'popup', true, true);
	});
	
	var current_query_index = 0;
	var on_query = false;
	generate_sharelink=1;
	
	if(generate_sharelink==1){
		var media_hostName = window.location.host;
		if(media_hostName.indexOf(":")!=-1)
			media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
		media_hostName = "http://" + media_hostName + ":" + g_storage.get("http_port") + "/";
		
		var timer = setInterval(function(){
			
			if(on_query==false){
				
				if(current_query_index<0||current_query_index>this_audio_list.length-1){
					clearInterval(timer);
					initJPlayer();
					showHideLoadStatus(false);
					$.mobile.changePage('#audio_dialog', {transition: 'pop', role: 'dialog'});
					return;
				}
			
				var this_audio = this_audio_list[current_query_index];
				var this_file_name = this_audio.substring( this_audio.lastIndexOf("/")+1, this_audio.length );
				var this_url = this_audio.substring(0, this_audio.lastIndexOf('/'));
				
				on_query = true;			
				client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){				
					if(error==200){
						var data = parseXml(statusstring);
						var share_link = $(data).find('sharelink').text();
						share_link = media_hostName + share_link;
						
						on_query = false;
						
						var obj = [];
						obj['name'] = mydecodeURI(this_file_name);
						obj['mp3'] = share_link;
						g_audio_playlist.push(obj);
						
						current_query_index++;
					}
				});
			}
		}, 100);
	}
	else{
		for(var i=0; i < this_audio_list.length;i++){
			var this_audio = this_audio_list[i];
			var this_file_name = this_audio.substring( this_audio.lastIndexOf("/")+1, this_audio.length );
			var this_url = this_audio.substring(0, this_audio.lastIndexOf('/'));
				
			var obj = [];
			obj['name'] = mydecodeURI(this_file_name);
			obj['mp3'] = this_audio;
			g_audio_playlist.push(obj);
		}
		
		initJPlayer();
		
		showHideLoadStatus(false);
		
		$.mobile.changePage('#audio_dialog', {transition: 'pop', role: 'dialog'});
	}
}

function doRescanSamba(){
	var openurl = addPathSlash(g_storage.get('openurl'));
	
	closeDialog();
	showHideLoadStatus(true);
	
	client.RESCANSMBPC(openurl, function(error){	
		if(error[0]==2){
			g_storage.setl("onRescanSamba", 1);
			g_storage.setl("onRescanSambaCount", 0);
			g_storage.set("HostList", "");
			doPROPFIND("/");
		}
	});
}
	
function doMKDIR(name){	
	var openurl = addPathSlash(g_storage.get('openurl'));
	var this_url = openurl + myencodeURI(name);
	
	var already_exists = 0;
	
	$('li#list_item').each(function(index) {
		
		if($(this).attr("isdir")=="1"){
			
			if(name==$(this).attr("title")){
				already_exists = 1;
				alert(m.getString('folder_already_exist_msg'));
				return;
			}
			
		}
	});
	
	if(already_exists==1)
		return;
	
	client.MKCOL(this_url, function(error){
		if(error[0]==2){
			doPROPFIND(openurl);
		}
		else
			alert(m.getString(error));
	});
}

function doRENAME(old_name, new_name){
	
	var already_exists = 0;
	var openurl = addPathSlash(g_storage.get('openurl'));
	var this_url = openurl + new_name;
	
	$('li#list_item').each(function(index) {		
		if(new_name==myencodeURI($(this).attr("title"))){
			already_exists = 1;
				
			if($(this).attr("isdir")=="1")
				alert(m.getString('folder_already_exist_msg'));
			else
				alert(m.getString('file_already_exist_msg'));
			return;
		}
	});
	
	if(already_exists==1)
		return;
	
	//alert(old_name + '-> ' + this_url);
		
	client.MOVE(old_name, this_url, function(error){
		if(error[0]==2){
			doPROPFIND(openurl);
		}
		else
			alert(m.getString(error) + " : " + decodeURI(old_name));
	}, null, false);
}

function getFileViewHeight(){
	return $("#fileview").height();
}

function closeUploadPanel(v){
	g_upload_mode = 0;
	g_reload_page=v;
	
	g_storage.set('stopLogoutTimer', "0");
	
	$("div#btnNewDir").css("display", "block");
	if(g_support_html5==1)
		$("div#btnUpload").css("display", "block");
	$("div#btnSelect").css("display", "block");
	$("#btnPlayImage").css("display", "block");
		
	$("div#btnCancelUpload").css("display", "none");
	
	$("#function_help").text("");
	
	$("#upload_panel").animate({left:"1999px"},"slow", null, function(){
		$("#upload_panel").css("display", "none");
		adjustLayout();
	});
	
	if(v==1){
		var openurl = addPathSlash(g_storage.get('openurl'));
		doPROPFIND(openurl);
	}
}

function openSelectMode(){
	if(g_select_mode==1)
		return;
	
	$("li#list_item").removeClass("ui-btn-active");
  	$("li#list_item").removeClass("selected");
  				
	$("li#list_item #icon").addClass("selectmode");
	  	
	$("#navbar").css("display","none");
	$("#navbar2").css("display","block");
	
	$('#btn-delete').addClass('ui-disabled');
	$('#btn-rename').addClass('ui-disabled');
	$('#btn-share').addClass('ui-disabled');
	
	g_select_mode = 1;
}

function cancelSelectMode(){
	if(g_select_mode==0)
		return;
	
	$("#navbar").css("display","block");
	$("#navbar2").css("display","none");
	
	$("li#list_item #icon").removeClass("selectmode");
	$("li#list_item #icon").removeClass("checked");
  	$("li#list_item").removeClass("ui-btn-active");
  	$("li#list_item").removeClass("selected");
  
  	g_select_mode = 0;
}
	
function adjustLayout(){		
	
	var page_size = getPageSize();
	var page_width = page_size[0];
	var page_height = page_size[1];
	
	var page_width = $(".ui-mobile-viewport").width();
	var page_height = $(".ui-mobile-viewport").height();
	
	$("div#content").css("height", page_height - 
			                       $("#header").height() - 
	                               $("#footer").height() - 
	                               $("#btnParent").height() - 
	                               parseInt($("#content").css("padding"))*2 );	
}
	
function closeJqmWindow(v){	
	g_reload_page=v;
	var $modalWindow = $('div#modalWindow');
	
	var $modalContainer = $('iframe', $modalWindow);
	$modalContainer.attr('src', '');
	
	if($modalWindow)
		$modalWindow.jqmHide();
	
	showHideLoadStatus(false);
}

function resizeJqmWindow(w,h){
	var $modalWindow = $('.jqmWindow');
	if($modalWindow){
		$modalWindow.css("width", w + "px");
		$modalWindow.css("height", h + "px");
	}
}

function fullscreenJqmWindow(v){	
	var $modalWindow = $('div#modalWindow');
	if($modalWindow){
		$modalWindow.css("left", "0px");
		$modalWindow.css("top", "0px");
		$modalWindow.css("width", window.width + "px");
		$modalWindow.css("height", window.height + "px");
	}
}

function doLOGOUT(){
	doPROPFIND("/", function(){
		var openurl = addPathSlash(g_storage.get('openurl'));
		client.LOGOUT("/", function(error){	
			if(error[0]==2){
				g_storage.set('openhostuid', '0');				
				window.location.reload();
			}
		});
	}, 0);
}

function resetTimer(){
	
	clearInterval(timer_idle);
	
	g_time_count = 0;
	
	timer_idle = setInterval( function(){
		
		if(g_storage.get('stopLogoutTimer')=="1"||g_show_modal==1){
			g_time_count = 0;
			return;
		}
		
		g_time_count++;
		
		if( (g_time_count*1000) > g_time_out){
			g_time_count=0;
			doLOGOUT();
		}
	}, 1000 );
}

function createOpenUrlUI(open_url){
	if(open_url==undefined)
		open_url = "/";
		
	var s = mydecodeURI(open_url);
	var b = "";
	var c = "";
	var tmp = "";
	var urlregion_width = $("#urlregion-url").width();
					
	if(s!="/"){
		var aa = s.split('/');
		var bb = open_url.split('/');			
		
		for(var i=0; i<aa.length; i++){
			if(aa[i]==""||
			   (g_enable_aidisk==1 && aa[i]==g_aidisk_name))
				continue;
				
			c += "/" + bb[i];
			
			tmp += " / <a id='url_path' uhref='" + c + "'>" + aa[i] + "</a>";
			
			var cur_width = String(tmp).width($("p#openurl").css('font'));
			if(cur_width>urlregion_width){
				b += " / ...";
				break;
			}
			
			if(aa[1]==g_aidisk_name&&g_enable_aidisk==1)
				b += " / <a id='url_path' uhref='/" + g_aidisk_name + c + "'>" + aa[i] + "</a>";
			else
				b += " / <a id='url_path' uhref='" + c + "'>" + aa[i] + "</a>";
				
			tmp = b;
		}
	}
		
	$("p#openurl").empty();
	$("p#openurl").append(b);
		
	$("a#url_path").click(function(){
		doPROPFIND($(this).attr("uhref"));
	});
}

function doPROPFIND(open_url, complete_handler, auth){
	if(client==null)
		return;
	
	showHideLoadStatus(true);
	
	try{
		
		client.PROPFIND(open_url, auth, function(error, statusstring, content){		
			if(error){
				
				if(error==207){
					
					cancelSelectMode();
					closeUploadPanel();
					
					var parser;
					var xmlDoc;
							
					g_folder_array = new Array();
					g_file_array = new Array();

					if (window.DOMParser){					
						parser=new DOMParser();
						xmlDoc=parser.parseFromString(content,"text/xml");
					}
					else { // Internet Explorer
						xmlDoc=new ActiveXObject("Microsoft.XMLDOM");
            			xmlDoc.async="false";
						xmlDoc.loadXML(content);
						
						if(!xmlDoc.documentElement){
							alert("Fail to load xml!");
							showHideLoadStatus(false);
							return;
						}
					}
					
					var i, j, k, l, n;						
					var x = xmlDoc.documentElement.childNodes;
						
					var this_query_type = xmlDoc.documentElement.getAttribute('qtype'); //- 2: query host, 1: query share, 0: query file
					var this_folder_readonly = xmlDoc.documentElement.getAttribute('readonly');
					var this_router_username = xmlDoc.documentElement.getAttribute('ruser');
					var this_computer_name = xmlDoc.documentElement.getAttribute('computername');
					var this_isusb = xmlDoc.documentElement.getAttribute('isusb'); 
					
					//- D:Response	
					for (i=0;i<x.length;i++){
						var this_href = "";
						var this_contenttype = "";
						var this_uniqueid = "";
						var this_name = "";
						var this_short_name = "";
						var this_online = "";
						var this_lastmodified = "";
						var this_contentlength = "";
						var this_ip = "";
						var this_mac = "";
						var this_type = "";
						var this_attr_readonly = "";
						var this_attr_hidden = "";
						var this_uncode_name;
						var this_router_sync_folder = "0";
						
						var y = x[i].childNodes;
						for (j=0;j<y.length;j++){
							if(y[j].nodeType==1&&y[j].nodeName=="D:propstat"){
											
								var z = y[j].childNodes;
											
								for (k=0;k<z.length;k++)
								{
									if(z[k].nodeName=="D:prop"){
													
										var a = z[k].childNodes;
													
										for (l=0;l<a.length;l++)
										{
											if(a[l].childNodes.length<=0)
												continue;
															
											if(a[l].nodeName=="D:getcontenttype"){														
												this_contenttype = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getuniqueid"){
												this_uniqueid = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getonline"){
												this_online = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getlastmodified"){
												this_lastmodified = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getcontentlength"){
												this_contentlength = String( size_format(parseInt(a[l].childNodes[0].nodeValue)));
											}
											else if(a[l].nodeName=="D:getmac"){
												this_mac = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getip"){
												this_ip = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:gettype"){
												this_type = String(a[l].childNodes[0].nodeValue);
											}
											else if(a[l].nodeName=="D:getattr"){
												var bb = a[l].childNodes;												
												for (n=0;n<bb.length;n++){
													if(bb[n].nodeName=="D:readonly")
														this_attr_readonly = bb[n].childNodes[0].nodeValue;
													else if(bb[n].nodeName=="D:hidden")
														this_attr_hidden = bb[n].childNodes[0].nodeValue;
												}
											}
											else if(a[l].nodeName=="D:getroutersync"){												
												this_router_sync_folder = String(a[l].childNodes[0].nodeValue);
											}
										}
									}
								}
							}
							else if(y[j].nodeType==1&&y[j].nodeName=="D:href"){
								this_href = String(y[j].childNodes[0].nodeValue);
								
								var cur_host = "";
								
								if(this_href.match(/^http/))						
									cur_host = window.location.protocol + "//" + window.location.host;	
									
								var cururl = cur_host + addPathSlash(open_url);
																
								if(this_href!=cururl){
									var o_url = open_url;								
									
									this_href = this_href.replace(cur_host,"");
									
									//this_name = this_href.replace( addPathSlash(o_url),"");									
									this_name = this_href.substring( this_href.lastIndexOf("/")+1, this_href.length );
														
									if(this_name!=""){
										this_uncode_name = this_name;
										
										this_name = mydecodeURI(this_name);
										this_short_name = this_name;
										/*
										var len = (g_listview==0) ? 12 : 45;	
														
										if(this_short_name.length>len){
											this_short_name = this_short_name.substring(0, len) + "...";
										}
										*/
									}
									else{
										this_href="";
									}
								}
								else{
									this_href="";
								}
							}
						}
						
						if(this_href!=""){							
								if( this_contenttype=="httpd/unix-directory" ){									
									g_folder_array.push({ contenttype: this_contenttype, 
										                  href: this_href,
										                  name: this_name,
										                  uname: this_uncode_name,
										                  shortname: this_short_name,
										                  online: this_online,
										                  time: this_lastmodified,
										                  size: this_contentlength,
										                  ip: this_ip,
										                  mac: this_mac, 
										                  uid: this_uniqueid,
										                  type: this_type,
										                  freadonly: this_attr_readonly,
										                  fhidden: this_attr_hidden,
										                  routersyncfolder: this_router_sync_folder });
								}				
								else{
									g_file_array.push({ contenttype: this_contenttype, 
										                href: this_href, 
										                name: this_name,
										                uname: this_uncode_name,
										                shortname: this_short_name,
										                online: this_online,
										                time: this_lastmodified,
										                size: this_contentlength,
										                ip: this_ip,
										                mac: this_mac, 
										                uid: this_uniqueid,
										                type: this_type,
										                freadonly: this_attr_readonly,
										                fhidden: this_attr_hidden,
										                routersyncfolder: this_router_sync_folder });
								}
						}
					}
					
					//- Sort By Name
					g_folder_array.sort(sortByName);
					g_file_array.sort(sortByName);
					
					//- Move the usbdisk to first
					var index;
					for(index=0; index<g_folder_array.length; index++){
						if(g_folder_array[index].type == "usbdisk"){
							arraymove(g_folder_array, index, 0);
							//break;
						}
					}
					
					//- parent url
					var parent_url = addPathSlash(open_url);
					if(parent_url!="/"){
						parent_url = parent_url.substring(0, parent_url.length-1);
						parent_url = parent_url.substring(0, parent_url.lastIndexOf("/"));
						if( parent_url=="" ) parent_url="/";
					}
					
					if(parent_url=="/"+g_storage.get('modalname')) parent_url = "/";

					//- Create list view					
					createListView(this_query_type, parent_url, g_folder_array, g_file_array);
					
					if(this_query_type==1&&this_isusb==0)
						$("#btn-changeuser").show();
					else
						$("#btn-changeuser").hide();
						
					if(this_query_type==0)
						$("#btn-select").show();
					else
						$("#btn-select").hide();
					
					$("span#username").text(this_router_username);
					
					adjustLayout();
					
					closeDialog();
					
					g_storage.set('openurl', open_url);					
					$(".ui-listview").attr("path", open_url);
					
					if(complete_handler!=undefined){
	  					complete_handler();
	  				}
		  		}
		  		else if(error==501){
					doPROPFIND(open_url);
				}
		  		else if(error==401){	
		  			setTimeout(openLoginWindow(open_url), 1000);
				}
				else{
					alert(m.getString(error));					
				}
				
				showHideLoadStatus(false);
			}
		}, null, 1);
		
		resetTimer();
	}
	catch(err){
		//Handle errors here
		alert('catch error: '+ err);
	}
}

function doLOGIN(path, auth){		
	doPROPFIND(path, function(){
				
	}, auth);
}	

function createListView(query_type, parent_url, folder_array, file_array){
	
	clearInterval(g_timer_filelist);
	$("ul#list").empty();
	
	$("#btnParent").remove();
	
	//- Parent Path
	if(query_type == 0 || query_type == 1) {
		var html = "";
		
		html += '<div id="btnParent" class="ui-btn-text ui-btn-parent" data-icon="home" data-role="button" data-iconpos="left" ';
		html += ' online="0" qtype="1" isdir="1" uhref="' + parent_url + '"><span>';
		html += m.getString("btn_prevpage");			
		html += '</span></div>';
		
		$('#content').before(html);
	}
	
	//- Folder List
	for(var i=0; i<folder_array.length; i++){
		var html = "";
		html += '<li id="list_item" qtype="';
		html += query_type;
		html += '" isdir="1" uhref="';
		html += folder_array[i].href;
		html += '" title="';
		html += folder_array[i].name;
		html += '" online="';
		html += folder_array[i].online;
		html += '" ip="" mac="';
		html += folder_array[i].mac;
		html += '" uid="';
		html += folder_array[i].uid;
		html += '" freadonly="';
		html += folder_array[i].freadonly;
		html += '" fhidden="';
		html += folder_array[i].fhidden;
		
		if(folder_array[i].type == "usbdisk")
			html += '" isusb="1"';
		else						
			html += '" isusb="0"';
								
		html += '" data-icon="cloud-arrow-r">';
		
		//html += "<input type='checkbox' name='checkbox-0' />";
		
		html += "<a href='#' data-transition='slide' class='ui-btn-icon-left'>";
		
		//- ICON
		if(query_type==2){
			if(folder_array[i].type == "usbdisk")
				html += "<span id='icon' class='sicon usbdisk ui-folder-icon ui-li-thumb ";
			else if(folder_array[i].type == "sambapc"){
				if(folder_array[i].online==1)
					html += "<span id='icon' class='sicon sambapc ui-folder-icon ui-li-thumb ";
				else
					html += "<span id='icon' class='sicon sambapcoff ui-folder-icon ui-li-thumb ";
			}
		}
		else{
			html += "<span id='icon' class='sicon folder ui-folder-icon ui-li-thumb ";
		}
		
		if(g_select_mode==1)
			html += "selectmode";
		
		html += "'>";
		
		if(folder_array[i].routersyncfolder == "1"){
			html += "<div id='icon' class='sicon routerync'/>";
		}
	
		html += "</span>";
		
		html += "<h3>" + folder_array[i].shortname;	
		
		if(folder_array[i].online == "0" && query_type == "2")
			html += "(" + m.getString("title_offline") + ")";
		
		html += "</h3>";	
			
		if(query_type == "0"){
			html += "<p class='ui-li-desc'>";
			html += m.getString("table_time") + ": " + folder_array[i].time;
			html += "</p>";
		}
		
		html += "</a>";
									
		html += '</li>';
		
		$("ul#list").append(html).find("li:last").hide();    	
 	 	$('ul#list').listview('refresh');  
  		$("ul#list").find("li:last").slideDown(300);  
	}
	
	/*
	//- File List
	for(var i=0; i<file_array.length; i++){
		var this_title = m.getString("table_filename") + ": " + file_array[i].name + "\n" + 
		                 m.getString("table_time") + ": " + file_array[i].time + "\n" + 
		                 m.getString("table_size") + ": " + file_array[i].size;
		
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
		
		var html = "";
		html += '<li data-theme="c" id="list_item" qtype="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" title="';
		html += file_array[i].name;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '">';
				
		html += "<div id='file_item'>";
				
		//- ICON
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += "<span id='icon' class='sicon ui-li-thumb imgfileDiv'/>";
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += "<span id='icon' class='sicon ui-li-thumb audiofileDiv'/>";
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
			      file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += "<span id='icon' class='sicon ui-li-thumb videofileDiv'/>";
		else if(file_ext=="doc"||file_ext=="docx")
			html += "<span id='icon' class='sicon ui-li-thumb docfileDiv'/>";
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += "<span id='icon' class='sicon ui-li-thumb pptfileDiv'/>";
		else if(file_ext=="xls"||file_ext=="xlsx")
			html += "<span id='icon' class='sicon ui-li-thumb xlsfileDiv'/>";
		else if(file_ext=="pdf")
			html += "<span id='icon' class='sicon ui-li-thumb pdffileDiv'/>";
		else
			html += "<span id='icon' class='sicon ui-li-thumb fileDiv'/>";
		
		html += file_array[i].shortname;		
		html += "</div>";
		
		html += '</li>';
		
		$("ul#list").append(html).find("li:last").hide();    	
 	 	$('ul#list').listview('refresh');
 	 	$("ul#list").find("li:last").slideDown(300); 
	}
	*/
	
	var current_list_index = 0;
	
	g_timer_filelist = setInterval(function(){
				
		if(current_list_index<0||current_list_index>file_array.length-1){
			clearInterval(g_timer_filelist);
			return;
		}
				
		var i = current_list_index;
		var this_title = m.getString("table_time") + ": " + file_array[i].time + "\n" + 
		                 m.getString("table_size") + ": " + file_array[i].size;
		
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
				
		var html = "";
		html += '<li data-theme="c" id="list_item" qtype="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" title="';
		html += file_array[i].name;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '">';
			
		//- ICON
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon imgfileDiv ";
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon audiofileDiv ";
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
			    file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon videofileDiv ";
		else if(file_ext=="doc"||file_ext=="docx")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon docfileDiv ";
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon pptfileDiv ";
		else if(file_ext=="xls"||file_ext=="xlsx")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon xlsfileDiv ";
		else if(file_ext=="pdf")
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon pdffileDiv ";
		else
			html += "<span id='icon' class='sicon ui-li-thumb ui-file-icon fileDiv ";
			
		if(g_select_mode==1)
			html += "selectmode";
				
		html += "'/>";
		
		html += "<div id='file_item' class='ui-btn-text'>";
			
		html += "<h3>" + file_array[i].shortname + "</h3>";	
					
		html += "<p class='ui-li-desc'>";
		html += m.getString("table_time") + ": " + file_array[i].time;
		html += "</p>";
			
		html += "<p class='ui-li-desc'>";
		html += m.getString("table_size") + ": " + file_array[i].size;
		html += "</p>";
			
		html += "</div>";
			
		html += '</li>';
			
		$("ul#list").append(html).find("li:last").hide();    	
		$('ul#list').listview('refresh');
		$("ul#list").find("li:last").slideDown(300); 
 	 	
		current_list_index++;
			
	}, 200);
}

function sortByName(a, b) {
	var x = a.name.toLowerCase();
  	var y = b.name.toLowerCase();
  	return ((x < y) ? -1 : ((x > y) ? 1 : 0));
}
	
function arraymove(arr, fromIndex, toIndex) {
	var element = arr[fromIndex];
  	arr.splice(fromIndex,1);
	arr.splice(toIndex,0,element);
}

function showHideLoadStatus(bshow){
	if(bshow)
		$.mobile.showPageLoadingMsg();
	else
		$.mobile.hidePageLoadingMsg();
}

function openSelItem(item){	
	var loc = item.attr("uhref");
	var qtype = item.attr("qtype");
	var isdir = item.attr("isdir");
	var isusb = item.attr("isusb");
	var this_full_url = item.attr("uhref");
	var this_file_name = item.attr("title");
	
	g_storage.set('openuid', item.attr("uid"));
	g_storage.set('opentype', isusb);
		
	if(qtype==2){
		var online = item.attr("online");			
		if(online==0){				
			var r=confirm(m.getString('wol_msg'));
				
			if (r==true){
				var this_mac = item.attr("mac");
				
				client.WOL("/", this_mac, function(error){
					if(error==200)
						alert(m.getString('wol_ok_msg'));
					else
						alert(m.getString('wol_fail_msg'));
				});
			}
				
			return;
		}
	}
	
	if(isdir=="1"){
		doPROPFIND(loc, null, null);
		return;
	}
	
	var fileExt = getFileExt(loc);
	var webdav_mode = g_storage.get('webdav_mode');
	
	/*					
	if( fileExt=="mp4" ||
		  fileExt=="m4v" ||
		  fileExt=="wmv" ||
		  fileExt=="avi" ||
		  fileExt=="rmvb"||
		  fileExt=="rm" ||
		  fileExt=="mpg" ||
		  fileExt=="mpeg"||
		  fileExt=="mkv" ||
		  fileExt=="mov" ||
		  fileExt=="flv" ) {
		    
		//- webdav_mode=0-->enable http, webdav_mode=2-->both enable http and https
		if( webdav_mode==0 || webdav_mode==2 ){
			  
			  if( isWinOS() ){
				  if( isBrowser("msie") && 
					    getInternetExplorerVersion() <= 7 ){
					   //- The VLC Plugin doesn't support IE 7 or less
					   alert(m.getString('msg_vlcsupport'));
					}
					else{
						var $modalWindow = $("div#modalWindow");
						
						this_file_name = myencodeURI(this_file_name);		
						this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
						
						var media_hostName = window.location.host;
						if(media_hostName.indexOf(":")!=-1)
							media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
						media_hostName = "http://" + media_hostName + ":" + g_storage.get("http_port") + "/";
										
						client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
							if(error==200){
								var data = parseXml(statusstring);
								var share_link = $(data).find('sharelink').text();
								var open_url = "";							
								
								share_link = media_hostName + share_link;								
								open_url = '/smb/css/vlc_video.html?v=' + share_link;
								
								open_url += '&showbutton=1';							
								g_modal_url = open_url;
								g_modal_window_width = 655;
								g_modal_window_height = 500;
								$('#jqmMsg').css("display", "none");
								$('#jqmTitleText').text(m.getString('title_videoplayer'));
								if($modalWindow){
									$modalWindow.jqmShow();
								}
							}
						});
					
						return;
					}
				}
		}
		
	}
	*/
	if( fileExt=="mp3" ) {
		openAudioPlayer(loc);
		return;		
	}
	
	if( fileExt=="doc" || fileExt=="docx" || 
	    fileExt=="ppt" || fileExt=="pptx" || 
	    fileExt=="xls" || fileExt=="xlsx" || 
	    fileExt=="pdf") {
		
		var location_host = window.location.host;
		
		//- It will open with google doc viewer when OS is window or mac and using public ip.
		if( ( isWinOS() || isMacOS() ) && !isPrivateIP() ){
			
		 	this_file_name = myencodeURI(this_file_name);		
			this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
							
			client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
				if(error==200){
					var data = parseXml(statusstring);
					var share_link = $(data).find('sharelink').text();
					var open_url = "";
								
					share_link = window.location.protocol + "//" + window.location.host+ "/" + share_link;
					open_url = 'https://docs.google.com/viewer?url=' + share_link;
								
					window.open(open_url);
				}
			});
			
			return;
		}
	}
	
	if( fileExt=="jpg" || 
	    fileExt=="jpeg"||
	    fileExt=="png" ||
	    fileExt=="gif" ){
		openImageViewer(loc);
		return;
	}
	
	window.open(loc);
}

function createLayout(){
	
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
	m.setLanguage(lan);
	
	$("#btn-home .ui-btn-text").text(m.getString('btn_homepage'));
	$("#btn-refresh .ui-btn-text").text(m.getString('btn_refresh'));
	$("#btn-logout .ui-btn-text").text(m.getString('title_logout'));
	$("#btn-setting .ui-btn-text").text(m.getString('title_setting'));
	$("#btn-delete .ui-btn-text").text(m.getString('btn_delselect'));
	$("#btn-rename .ui-btn-text").text(m.getString('btn_rename'));
	$("#btn-share .ui-btn-text").text(m.getString('btn_sharelink'));
	$("#btn-createfolder .ui-btn-text").text(m.getString('btn_newdir'));
	
	$("#btn-changeuser").hide();
	$("#btn-select").hide();
}

function addtoFavorite(){
	var favorite_title = "AiCloud";
	var favorite_url = "https://" + g_storage.get('ddns_host_name');
	var isIEmac = false;
    var isMSIE = isBrowser("msie");
    
	if(favorite_url==""||favorite_url==undefined){ 
		favorite_url="https://router.asus.com/";
	}
		
	if ((typeof window.sidebar == "object") && (typeof window.sidebar.addPanel == "function")) {
    	window.sidebar.addPanel(favorite_title, favorite_url, "");
      	return false;   
    } 
    else if (isMSIE && typeof window.external == "object") {
    	window.external.AddFavorite(favorite_url, favorite_title);
      	return false;
    }
    else {
    	window.location = favorite_url;
    	
    	var ua = navigator.userAgent.toLowerCase();
      	var str = '';
      	var isWebkit = (ua.indexOf('webkit') != - 1);
      	var isMac = (ua.indexOf('mac') != - 1);
 
      	if (ua.indexOf('konqueror') != - 1) {
      		str = 'CTRL + B'; // Konqueror
      	} 
      	else if (window.home || isWebkit || isIEmac || isMac) {
      		str = (isMac ? 'Command/Cmd' : 'CTRL') + ' + D'; // Netscape, Safari, iCab, IE5/Mac
      	}
      
      	str = ((str) ? m.getString('msg_add_favorite1') + str + m.getString('msg_add_favorite2') : str);
        
      	alert(str);
    }
        
}

function getRouterInfo(){
	client.GETROUTERINFO("/", function(error, statusstring, content){				
		if(error==200){
			var data = parseXml(content);
			var x = $(data);
    		g_storage.set('webdav_mode', x.find("webdav_mode").text());
    		g_storage.set('http_port', x.find("http_port").text());
			g_storage.set('https_port', x.find("https_port").text());
			g_storage.set('misc_http_enable', x.find("misc_http_enable").text());
			g_storage.set('misc_http_port', String(x.find("misc_http_port").text()).replace("\n",""));
			g_storage.set('last_login_info', x.find("last_login_info").text());
			g_storage.set('ddns_host_name', x.find("ddns_host_name").text());
			g_storage.set('router_version', x.find("version").text());
			g_storage.set('aicloud_version', x.find("aicloud_version").text());
			g_storage.set('modalname', x.find("modalname").text());
					
			var login_info = g_storage.get('last_login_info');
			if(login_info!=""&&login_info!=undefined){
				var login_info_array = String(login_info).split(">");  	
				var info = m.getString('title_logininfo')+ login_info_array[1] + ", " + m.getString('title_ip') + login_info_array[2];
				$("#login_info").text(info);
			}
		}
	});
}

function createMenu(menuId, parentId, options, menuHandler){
	//create a containing div
  	var div = $("<div id='" + menuId + "div'></div>").appendTo("#"+parentId).show();

  	//create select tag
  	var menuElm = $("<select id='" + menuId + "' data-inline='true' data-native-menu='false'></select>").appendTo(div);
  	//var menuElm = $("<select id='" + menuId + "' data-inline='true' data-native-menu='false'></select>").appendTo("#"+parentId).hide();

  	//add options
  	var optionsArray = options.split(",");
  	for (var i = 0; i < optionsArray.length; i++)
  		$("<option>" + optionsArray[i] + "</option>").appendTo("#"+menuId);

  	//convert to JQueryMobile menu
  	$("#" + menuId).selectmenu();

  	//find custom menu that JQM creates
  	var menus = $(".ui-selectmenu");
  	for (var i = 0; i < menus.length; i++)
  	{
  		if ($(menus[i]).children("ul:#" + menuId + "-menu").length > 0)
    	{
    		newMenu = $(menus[i]);
    		break;
    	}
  	}

  	if (newMenu == null)
  	{
  		alert("Error creating menu");
    	return;
  	}

  	//Associate click handler with menu items, i.e. anchor tags
  	$(newMenu).find(".ui-selectmenu-list li a").click(menuHandler);    

  	//Add Close option
  	var menuHeader = $(newMenu).find(".ui-header");
  	var closeLinkId = menuId + "_close_id";
  	menuHeader.prepend("<span style='position:relative;float:left'>" +
                "<a href='#' id='" + closeLinkId + "'>X</href></span>");
                
  	$("#" + closeLinkId).click(function(e){
  		newMenu.hide();
  	});

	return newMenu.hide();
}

function showMenu(menu){
	if (menu == null)
  		return;

	//show menu at center of the window
  	var left = ($(window).width() - $(menu).width()) / 2;
  	//consider vertical scrolling when calculating top
  	var top = (($(window).height() - $(menu).height()) / 2) + $(window).scrollTop();
  
  	left = 0;
  	top = 0;
  
  	$(menu).css({
  		left: left,
    	top: top
  	});
	
  	$(menu).show();
}

//Callback handler when menu item is clicked
function menuHandler(event){
	if (newMenu != null)
  		$(newMenu).hide();

	alert(event.srcElement.text);                    
}
                   
//document.addEventListener("touchmove", function(e){
	//e.preventDefault();
//}, false);

(function($) {
	
	$.widget('mobile.tabbar', $.mobile.navbar, {
    	_create: function() {
			// Set the theme before we call the prototype, which will 
		  	// ensure buttonMarkup() correctly grabs the inheritied theme.
		  	// We default to the "a" swatch if none is found
		  	var theme = this.element.jqmData('theme') || "a";
		  	this.element.addClass('ui-footer ui-footer-fixed ui-bar-' + theme);

      		// Make sure the page has padding added to it to account for the fixed bar
      		this.element.closest('[data-role="page"]').addClass('ui-page-footer-fixed');
			
      		// Call the NavBar _create prototype
      		$.mobile.navbar.prototype._create.call(this);      
    	},

    	// Set the active URL for the Tab Bar, and highlight that button on the bar
    	setActive: function(url) {
      		// Sometimes the active state isn't properly cleared, so we reset it ourselves
      		this.element.find('a').removeClass('ui-btn-active ui-state-persist');
      		this.element.find('a[href="' + url + '"]').addClass('ui-btn-active ui-state-persist');
    	}
	});
  	
  	$(document).bind('pagecreate create', function(e) {  	
  		return $(e.target).find(":jqmData(role='tabbar')").tabbar();
  	});
  
  	$(document).bind('pageinit', function(e) {  
  	
  		if(!g_bInitialize){
			$.mobile.selectmenu.prototype.options.nativeMenu = false;
		  	
			createLayout();
			getRouterInfo();
				
		  	$.mobile.activeBtnClass = 'unused';
		  	$.mobile.touchOverflowEnabled = true;
			//createMenu("dynamicMenu1","mainpage","Menus,menu1,menu2,menu3",menuHandler);
			//createMenu("dynamicMenu1", "select", "Menus,menu1,menu2,menu3", menuHandler);
				
			g_bInitialize = true;
		}  	
	});
  
  	$(":jqmData(role='page')").live('pageshow', function(e) {
    	// Grab the id of the page that's showing, and select it on the Tab Bar on the page
    	var tabBar, id = $(e.target).attr('id');
		
		if(id=="mainpage"){			
			var loc = g_storage.get('openurl');	
			loc = (loc==undefined) ? "/" : loc;	
				
			var curPath = $(".ui-listview").attr("path");
			
			if(loc!=curPath){				
				doPROPFIND( loc, function(){
					adjustLayout();
				}, 0);	
			}
		}
    	tabBar = $.mobile.activePage.find(':jqmData(role="tabbar")');
    	if(tabBar.length) {
      		tabBar.tabbar('setActive', '#' + id);
    	}   
  	});
  
  	$("li#list_item").live("click", function() {
  		if(g_select_mode==1){
  		
  			if($(this).hasClass("ui-btn-active")){
  				$(this).removeClass("ui-btn-active");
  				$(this).removeClass("selected");
  				$(this).find("#icon").removeClass("checked");
  			}
  			else{
  				$(this).addClass("ui-btn-active");
  				$(this).addClass("selected");
  				$(this).find("#icon").addClass("checked");
  			}
  		
  			var select_count = $("li#list_item.selected").size();
  		
  			if(select_count>0){
  				$('#btn-delete').removeClass('ui-disabled');
  				$('#btn-share').removeClass('ui-disabled');
  			}
  			else{
  				$('#btn-delete').addClass('ui-disabled');
  				$('#btn-share').addClass('ui-disabled');
  			}
  			
  			if(select_count==1)
				$('#btn-rename').removeClass('ui-disabled');
			else
				$('#btn-rename').addClass('ui-disabled');
	
  			return;
  		}
  		
  		$("li#list_item").removeClass("ui-btn-active");
  		$(this).addClass("ui-btn-active");
  	
  		openSelItem($(this));
	});  
  
  	$("#btnParent").live("click", function() {
  	
  		//$("li#list_item").removeClass("ui-btn-active");
  		//$(this).addClass("ui-btn-active");
  	
  		openSelItem($(this));
  	});  
  
  	$("a#btn-home").live("click", function() {
  		doPROPFIND( "/", function(){
			$("a#btn-home").removeClass("ui-btn-active");
		}, 0);
  	});
  
  	$("a#btn-refresh").live("click", function() {
  		var loc = g_storage.get('openurl');	
		loc = (loc==undefined) ? "/" : loc;		
		doPROPFIND( loc, function(){
			$("a#btn-refresh").removeClass("ui-btn-active");
		}, 0);	
  	});
  
  	$("a#btn-setting").live("click", function() {
  		openSettingWindow();
  	});
  
	$("a#btn-share").live("click", function() {
  	
  		//$("a#btn-share").removeClass("ui-btn-active");
  		$(this).removeClass("ui-btn-active");
  		
		var share_link_array = new Array();
		var complete_count = 0;
		var eml = "";
		var subj = "?subject=" + m.getString('email_subject');
		var bod = "";
		var selectURL = "";
		var selectFiles = "";
		
		$("li#list_item.selected").each(function(){
			
			var this_file_name = $(this).attr("title");
			var this_full_url = $(this).attr("uhref");
			var this_isdir = $(this).attr("isdir");
			var this_url = window.location.href;
			
			this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
				
			selectURL = this_url;
			
			selectFiles += encodeURI(this_file_name);
			selectFiles += ";";
		});
		
		if(selectFiles==""){
			alert("Please select files first.");
			return;
		}
	
		var webdav_mode = g_storage.get('webdav_mode');
		var ddns_host_name = g_storage.get('ddns_host_name');    
	    var cur_host_name = window.location.host;
	    var hostName = "";
	    
	    if(!isPrivateIP(cur_host_name))
			hostName = cur_host_name;
		else			
			hostName = (ddns_host_name=="") ? cur_host_name : ddns_host_name;
		
		if(hostName.indexOf(":")!=-1)
			hostName = hostName.substring(0, hostName.indexOf(":"));
			
		var is_private_ip = isPrivateIP(hostName);
			
		if( webdav_mode == 0 ) //- Only enable http
			hostName = "http://" + hostName + ":" + g_storage.get("http_port");
		else
			hostName = "https://" + hostName;
		//alert(selectURL + ", " + selectFiles);
		
		client.GSL(selectURL, selectURL, selectFiles, 86400, 1, function(error, content, statusstring){
			if(error==200){
				var data = parseXml(statusstring);				
				var result = $(data).find('sharelink').text();
					
				if(result==''){
					alert("Fail to parse xml!");
					return;
				}
									
				var aa = result.split(";");
				var len = aa.length;
					
				for(var i=0; i<len; i++){
					bod += hostName + "/" + aa[i];
					if(i!=len-1)
						bod += ";";
				}
				
				openShareWindow(bod);
			}
		});
	});
  
	$("a#btn-delete").live("click", function() {
  	
  		var webdav_delfile_callbackfunction = function(error, statusstring, content){
		
			if(error){
				if(error[0]==2){				
				}
				else{
					alert(m.getString(error)+" : " + decodeURI(g_selected_files[0]));
					return;
				}
			}
			
			g_selected_files.splice(0,1);
			
			if(g_selected_files.length<=0){
				var openurl = addPathSlash(g_storage.get('openurl'));
				doPROPFIND(openurl);
				return;
			}
			
			client.DELETE(g_selected_files[0], webdav_delfile_callbackfunction);
		};
		
  		g_selected_files = null;
		g_selected_files = new Array();
		
  		$("li#list_item.selected").each(function(){
			var this_full_url = $(this).attr("uhref");
			g_selected_files.push( $(this).attr("uhref") );
		});
		
		if(g_selected_files.length<=0)
			return;
			
  		var r=confirm(m.getString('del_files_msg'));
		if (r!=true){
			return;
		}
		
  		client.DELETE(g_selected_files[0], webdav_delfile_callbackfunction );
	});
  
  	$("a#btn-rename").live("click", function() {
  	
  		var this_file_name = $("li#list_item.selected").attr("title");
		var this_full_url = $("li#list_item.selected").attr("uhref");
		var this_isdir = $("li#list_item.selected").attr("isdir");
			
  		$("#dialog").remove();
		
		var dialog_html = '';
		dialog_html += "<div data-role='dialog' id='dialog' data-theme='a' data-transition='flip'>";
	  	dialog_html += "<div data-role='header'>";  
	  	dialog_html += "<h1 id='dialog-title'>" + m.getString('title_rename') + "</h1>";  
	  	dialog_html += "</div>";
	 	dialog_html += "<div data-role='content' id='dialog-content'>";
	  	
		dialog_html += '<table id="table_content" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;">';
		
		dialog_html += '<tr>';
	  	dialog_html += '<td>';
	  	dialog_html += '<input id="isdir" type="hidden" value="' + this_isdir + '"/>';
	  	dialog_html += '<input id="source" type="hidden" value="' + this_full_url + '"/>';
	  	dialog_html += '<input id="filename" type="text" value="' + this_file_name + '"/>';
	  	dialog_html += '</td>';
	  	dialog_html += '</tr>';
	  
		dialog_html += '<tr>';
	  	dialog_html += '<td>';
	  	dialog_html += '<div class="table_block_footer" style="text-align:right">';
	  	dialog_html += '<button id="ok" class="btnStyle">' + m.getString('btn_ok') + '</button>';
	  	dialog_html += '<button id="cancel" class="btnStyle">' + m.getString('btn_cancel') + '</button>';
	  	dialog_html += '</div>';
	  	dialog_html += '</td>';
	  	dialog_html += '</tr>';
		dialog_html += '</table>';
		
		dialog_html += "</div>";
		dialog_html += "</div>";
		
		$("body").append(dialog_html);
		
		$("#table_content #ok").click(function(){
			var oldFile = $('input#source').val();
			var newName = $('input#filename').val();
			var this_isdir = parseInt($('input#isdir').val());
			
			if(newName==''){		
				alert(m.getString('blankchar'));
				return;
			}
			
			var regex = /[|\/\\?*"<>:`]/g;
			if(this_isdir==1)
				regex = /[|\/\\?*"<>:`.]/g;
				
		 	if(newName.match(regex)) {
		 		alert(m.getString('illegalchar'));
		  		return;
		 	}
		 	
			if(this_isdir==1){
				if(oldFile.lastIndexOf('/')==oldFile.length-1)
					oldFile = oldFile.substring(0, oldFile.length-1);
			}
			
			doRENAME(oldFile, myencodeURI(newName));
			
		});
		
		$("#table_content #cancel").click(function(){
			closeDialog();
		});
		
		openDialog("pop");
	});
  
  	$("a#btn-createfolder").live("click", function() {
  	
  		$("#dialog").remove();
		
		var dialog_html = '';
		dialog_html += "<div data-role='dialog' id='dialog' data-theme='a' data-transition='flip'>";
	  	dialog_html += "<div data-role='header'>";  
	  	dialog_html += "<h1 id='dialog-title'>" + m.getString('title_newdir') +"</h1>";  
	 	dialog_html += "</div>";
	 	dialog_html += "<div data-role='content' id='dialog-content'>";
	  	
		dialog_html += '<table id="table_content" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;">';
		
		dialog_html += '<tr>';
		dialog_html += '<td>';
		dialog_html += '<input id="dir_name" type="text" value="' + m.getString('default_dir_name') + '"/>';
		dialog_html += '</td>';
	  	dialog_html += '</tr>';
	  
		dialog_html += '<tr>';
	  	dialog_html += '<td>';
	  	dialog_html += '<div class="table_block_footer" style="text-align:right">';
	  	dialog_html += '<button id="ok" class="btnStyle">' + m.getString('btn_ok') + '</button>';
	  	dialog_html += '<button id="cancel" class="btnStyle">' + m.getString('btn_cancel') + '</button>';
	  	dialog_html += '</div>';
	  	dialog_html += '</td>';
	  	dialog_html += '</tr>';
		dialog_html += '</table>';
		
		dialog_html += "</div>";
		dialog_html += "</div>";
		
		$("body").append(dialog_html);
		
		$("#table_content #ok").click(function(){
			
			var name = $('input#dir_name').val();
	
			if(name==''){
				alert(m.getString('blankchar'));
				return;
			}
			
			var regex = /[|~\/\\?*"<>:`.]/g;
		 	if(name.match(regex)) {
		 		alert(m.getString('illegalchar'));
		  		return;
		 	}
 	
			doMKDIR(name);			
		});
		
		$("#table_content #cancel").click(function(){
			closeDialog();
		});
		
		openDialog("pop");
	});
  
  	$("#btn-select").live("click", function() {
  		if(g_select_mode==0){
  			openSelectMode();
  			$(this).addClass("click");
  		}
	  	else if(g_select_mode==1){
  			cancelSelectMode();
  			$(this).removeClass("click");
  		}
  	});
  
  	$("#btn-changeuser").live("click", function() {
  		var r=confirm(m.getString('msg_changeuser_confirm'));
		if (r!=true){
			return;
		}	
  		var loc = g_storage.get('openurl');
		client.LOGOUT(loc, function(error){	
			if(error==200){				
				doPROPFIND(loc);
			}
		});
	});

  	$("div#select").live("click", function() {  
  	
  		showMenu(newMenu);
  	
		//showMenu($("#dynamicMenu1"));
		/*	
		if(g_select_mode==0)
			openSelectMode();
		  else if(g_select_mode==1)
			cancelSelectMode();
		*/
  	
  	});
  
	$("#select-choice-custom-button").live("click", function() { 
		//alert("click");
		//alert($("#select-choice-custom").val());
		if(g_select_mode==0)
			openSelectMode();
		else if(g_select_mode==1)
			cancelSelectMode();
	});
	  
	$("#btn-logout").live("click", function() {
		var r=confirm(m.getString('msg_logout_confirm'));
		if (r!=true){
			return;
		}	
		doLOGOUT();
	});
  
  	$(window).resize(adjustLayout);
  
})(jQuery);
