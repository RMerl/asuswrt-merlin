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
var g_folder_array = null;
var g_file_array = null;
var g_opening_uid = '';
var g_on_button_animation = 0;
var g_on_rescan_samba = 0;
var g_rescan_samba_timer = 0;
var g_rescan_samba_count = 0;
var g_aidisk_name = "usbdisk";
var g_enable_aidisk = 0;
var g_support_lan = new Array('zh-tw', 'zh-cn', 'en-us', 'jp', 'it', 'fr', 'de', 'br', 'cz', 'da', 'fi', 'ms', 'no', 'pl', 'es', 'sv', 'th', 'tr', 'uk', 'ru');	
var g_current_locktoken = "";
var g_mouse_x = 0;
var g_mouse_y = 0;
var g_fileview_only = 0;
var g_medialist_mode = 0;

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
	
	//- upload facebook
	//div_html += '<a class="facebook_upload" href="#" style="color:#fff" file="' + image_array[default_index].href + '">Share</a><br>';
	
	//- upload picassa
	//div_html += '<a class="picassa_upload" href="#" style="color:#fff" file="' + image_array[default_index].href + '">Share Picassa</a>';
	
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
  
  	//div_html += '<div class="barousel_nav"></div>';
                  
	div_html += '</div>';
		
	$(div_html)
		.animate({width:"100%", height:"100%", left:"0px", top:"0px"},200, null, null )
  		.appendTo("body");
  
	$('#image_slide_show').barousel({				
		navType: 2,
	    manualCarousel: 1,
	    contentResize:0,
	    startIndex:default_index
	});
	
	$('.facebook_upload').click(function(){
		
		var $modalWindow = $("div#modalWindow");
		
		var file = $(this).attr("file");
		var this_file_name = myencodeURI(file.substring(file.lastIndexOf('/')+1, file.length));
		var this_url = file.substring(0, file.lastIndexOf('/'));
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
				
				//g_modal_url = '/smb/css/upload_facebook.html';
				g_modal_url = 'http://www.efroip.com/efroip/fbtest/fb.html?v=' + share_link + '&b=' + mydecodeURI(this_file_name) + '&d=' + window.location.href;
				//g_modal_url = 'http://www.efroip.com/facebook/upload_photo.html';
		
				g_modal_window_width = 600;
				g_modal_window_height = 320;
				$('#jqmMsg').css("display", "none");	
				$('#jqmTitleText').text("Upload to Facebook");
				if($modalWindow){
					$modalWindow.jqmShow();
				}
				
				$("#image_slide_show").remove();
			}
		});
		
	});
	
	$('.picassa_upload').click(function(){
		var $modalWindow = $("div#modalWindow");
		
		var file = $(this).attr("file");
		var this_file_name = myencodeURI(file.substring(file.lastIndexOf('/')+1, file.length));
		var this_url = file.substring(0, file.lastIndexOf('/'));
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
				
				//g_modal_url = '/smb/css/upload_facebook.html';
				g_modal_url = 'http://www.efroip.com/efroip/google/index.html?v=' + share_link + '&b=' + mydecodeURI(this_file_name) + '&d=' + window.location.href;
				//g_modal_url = 'http://www.efroip.com/facebook/upload_photo.html';
		
				g_modal_window_width = 600;
				g_modal_window_height = 320;
				$('#jqmMsg').css("display", "none");	
				$('#jqmTitleText').text("Upload to Facebook");
				if($modalWindow){
					$modalWindow.jqmShow();
				}
				
				$("#image_slide_show").remove();
			}
		});
		
	});
	
	image_array = null;
}

function openAudioPlayer(loc){
	var audio_array = new Array();
	for(var i=0;i<g_file_array.length;i++){
   	var file_ext = getFileExt(g_file_array[i].href);			
		if(file_ext=="mp3")
   		audio_array.push(g_file_array[i]);
	}
	
	var audio_array_count = audio_array.length;
	
	if(audio_array_count==0){
		alert(m.getString("msg_no_image_list"));
		return;
	}
	
	var audio_list = "";
	var default_index = 0;
	for(var i=0;i<audio_array.length;i++){
		var the_loc = audio_array[i].href;
			
	 	if( loc == the_loc )
	 		default_index = i;
	   			
	 	audio_list += the_loc;
	 	if(i!=audio_array.length-1) audio_list += ",";
	}
	
	var $modalWindow = $("div#modalWindow");
	g_modal_url = '/smb/css/audio.html?a=' + loc + '&alist=' + audio_list + '&index=' + default_index + "&s=1";
	//g_modal_url = '/smb/css/audio2.html?a=' + loc + '&alist=' + audio_list + '&index=' + default_index;
	
	g_modal_window_width = 450;
	g_modal_window_height = 160;
	$('#jqmMsg').css("display", "none");	
	$('#jqmTitleText').text(m.getString('title_audioplayer'));
	if($modalWindow){
		$modalWindow.jqmShow();
	}
	
}

function openLoginWindow(open_url){	
	var $modalWindow = $("div#modalWindow");
	var css_display = $modalWindow.css("display");
	var dologin = function(){
		var user = $('#table_login input#username').val();
		var pass = $('#table_login input#password').val();
		var auth = "Basic " + Base64.encode(user + ":" + pass);		
		closeJqmWindow(0);		
		doLOGIN(open_url, auth);
	};									
	//g_modal_url = '/smb/css/login.html?v='+open_url;
	g_modal_url = '';
	g_modal_window_width = 380;
	g_modal_window_height = 120;
						
	if(open_url=='/')
		$('#jqmTitleText').text(m.getString('title_login') + " - AiCloud");
	else{
		var url = mydecodeURI(open_url);
		$('#jqmTitleText').text(m.getString('title_login') + " - " + url.substring(0, 35));
	}
			
	if(css_display == "block"){
		//- Show username password error!
		$('#jqmMsgText').text(m.getString('msg_passerror'));
	}
						
	$('#jqmMsg').css("display", css_display);	
	
	var login_html = '';
	login_html += '<table id="table_login" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;">';	
	login_html += '<tr>';
	login_html += '<td>';
  	login_html += '<form name="form_login">';
	login_html += '<fieldset width="120px">';
	login_html += '<table>';
	login_html += '<tr>';
	login_html += '<td><label id="username">' + m.getString('title_username') + '</label></td><td><input name="username" type="text" id="username" autocapitalize="off" style="width:250px"></td>';
	login_html += '</tr>';
	login_html += '<tr>';
	login_html += '<td><label id="password">' + m.getString('title_password') + '</label></td><td><input name="password" type="password" id="password" style="width:250px"></td>';
	login_html += '</tr>';
	login_html += '</table>';
	login_html += '</fieldset>';
	login_html += '</form>';
  	login_html += '</td>';
 	login_html += '</tr>';
  	login_html += '<tr style="height:10px"></tr>';
  	login_html += '<tr>';
  	login_html += '<td>';
  	login_html += '<div class="table_block_footer" style="text-align:right">';
  	login_html += '<button id="ok" class="btnStyle">' + m.getString('btn_ok') + '</button>';
  	login_html += '<button id="cancel" class="btnStyle">' + m.getString('btn_cancel') + '</button>';
  	login_html += '</div>';
  	login_html += '</td>';
  	login_html += '</tr>';
	login_html += '</table>';
	
	$('#jqmContent').empty();
	$(login_html).appendTo($('#jqmContent'));
	
	$('#table_login input#password').keydown(function(e){
		if(e.keyCode==13){
			dologin();
		}
	});
	
	$("#table_login #ok").click(function(){
		dologin();
	});
	
	$("#table_login #cancel").click(function(){
		closeJqmWindow(0);
	});
	
	if($modalWindow){
		$modalWindow.jqmShow();
	}
}

function doRescanSamba(){
	var openurl = addPathSlash(g_storage.get('openurl'));
	
	closeJqmWindow(0);
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
	
	$('a#list_item').each(function(index) {
		
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
			closeJqmWindow(0);
		}
		else
			alert(m.getString(error));
	});
}

function doRENAME(old_name, new_name){
	
	var already_exists = 0;
	var openurl = addPathSlash(g_storage.get('openurl'));
	var this_url = openurl + new_name;
	
	$('a#list_item').each(function(index) {
		
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
		
	client.LOCK(old_name, '', function(status, statusstring, content, headers){
		
		if (status != '201') {
     	alert(m.getString("msg_already_lock"));
     	return;
    }
    
    var locktoken = getLockToken(content);
    //alert(locktoken);
		client.MOVE(old_name, this_url, function(error){
			if(error[0]==2){
				doPROPFIND(openurl);
				closeJqmWindow(0);
			}
			else
				alert(m.getString(error));
				
			client.UNLOCK(old_name, locktoken, function(error){
				if(error!=204)
					alert("Unlock error: " + error);
			}); 
		
		}, null, "F", locktoken);
	
	}, null);
		
	/*
	client.MOVE(old_name, this_url, function(error){
		if(error[0]==2){
			doPROPFIND(openurl);
			closeJqmWindow(0);
		}
		else
			alert(m.getString(error) + " : " + decodeURI(old_name));
	}, null, false);
	*/
}

function getFileViewHeight(){
	return $("#fileview").height();
}

function closePopupmenu(){
	$(".popupmenu").remove();
}

function confirmCancelUploadFile(){
	var is_onUploadFile = g_storage.get('isOnUploadFile');
	
	if(is_onUploadFile==1){
		var r = confirm(m.getString('msg_confirm_cancel_upload'));
		if (r!=true){
			return 0;
		}
			
		$("#upload_panel iframe")[0].contentWindow.stop_upload();
	}
	
	return 1;
}

function closeUploadPanel(v){
	
	g_upload_mode = 0;
	g_reload_page=v;
	
	g_storage.set('stopLogoutTimer', "0");
	
	$("div#btnNewDir").css("display", "block");
	$("div#btnUpload").css("display", "block");
	$("div#btnSelect").css("display", "block");
	$("div#btnPlayImage").css("display", "block");
		
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

function cancelSelectMode(){
	if(g_select_mode==0)
		return;
		
	g_select_mode = 0;
	g_select_array = null;
		
	$("#btnDownload").addClass("disable");			
	$("#btnShareLink").addClass("disable");
	$("#btnDeleteSel").addClass("disable");
	$("#btnRename").addClass("disable");
	
	if(g_medialist_mode==1){
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnSelect").css("display", "block");
		$("div#btnPlayImage").css("display", "block");
		$("div#boxSearch").css("display", "block");
	}
	else{
		$("div#btnNewDir").css("display", "block");
		$("div#btnUpload").css("display", "block");
		$("div#btnSelect").css("display", "block");
		$("div#btnPlayImage").css("display", "block");
		$("div#boxSearch").css("display", "none");
	}
	
	$("div#btnCancelSelect").css("display", "none");
		
	$(".selectDiv").css("display", "none");
	$(".selectHintDiv").css("display", "none");
		
	$("#fileview #fileviewicon").each(function(){
		$(this).removeClass("select");	
	});
		
	$("#function_help").text("");
	
	adjustLayout();
	
	$("#button_panel").animate({left:"1999px"},"slow", null, function(){
		
	});
}
	
function adjustLayout(){		
	var page_size = getPageSize();
	var page_width = page_size[0];
	var page_height = page_size[1];
	var mainRegion_height = page_height - 
							$('#header_region').height() - 											
							$('#bottom_region').height();
	$('#main_region').css('height', mainRegion_height);
	
	var mainRegion_width = page_size[0];
	$('#main_region #main_right_region').css('width', mainRegion_width-$('#main_region #main_left_region').width());
	
	var hostview_height = $('#main_left_region').height() - $('#infobar').height() - $('#settingbar').height();
	var hostview_height2 = Math.max(Math.floor(hostview_height/40), 1)*40;
	$('#hostview').css("height", hostview_height2);
	$('#settingbar').css("padding-top", hostview_height-hostview_height2);
	
	var albutton_width=0;	
	$("#toolbar .albutton").each(function(){
		if($(this).css("display")=="block")
			albutton_width += $(this).width();
	});
	
	if($("#toolbar .alsearch").css("display")=="block")
		albutton_width += $("#toolbar .alsearch").width();
		
	$('#urlregion-url').css("width", $("#toolbar").width()-albutton_width-20);
	
	createOpenUrlUI(g_storage.get('openurl'));
	
	if(g_select_mode==1){
		var h = $("#main_right_container").height() - $("#infobar").height() - $("#button_panel").height();
		$("#fileview").css("height", h);
	}/*
	else if(g_upload_mode==1){
		var h = $("#main_right_container").height() - $("#infobar").height() - $("#button_panel").height();
		$("#upload_panel").css("width", $("#fileview").width());
		$("#upload_panel").css("height", h);
	}*/
	else{
		var h = $("#main_right_container").height() - $("#infobar").height();
		$("#fileview").css("height", h);
	}
	
	$("#function_help").css("left", $("#main_left_region").width());
	
	var $modalWindow = $('div#modalWindow');
	if($modalWindow){
		var newLeft = (page_width - g_modal_window_width)/2;
		$modalWindow.css("left", newLeft);
	}	
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
		//$("#function_help").text('resetTimer...'+g_time_count+" sec, stopLogoutTimer="+g_storage.get('stopLogoutTimer')+", g_show_modal="+g_show_modal);	
		
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
		
		
	//alert($("#urlregion-url").width()+", "+String(b).width($("p#openurl").css('font')));			
	$("a#url_path").click(function(){
		if(confirmCancelUploadFile()==0)
			return;
			
		doPROPFIND($(this).attr("uhref"));
	});
}

function doPROPFINDMEDIALIST(open_url, append_result, complete_handler, media_type, start, end, keyword, orderby, orderrule){
	if(client==null)
		return;
		
	showHideLoadStatus(true);
	
	try{		
		client.PROPFINDMEDIALIST(open_url, function(error, statusstring, content){
			if(error){				
				if(error==207){
					
					cancelSelectMode();
					closeUploadPanel();
					
					var xmlDoc = content2XMLDOM(content);					
					if(xmlDoc){
						var this_query_type = 1; //- 2: query host, 1: query share, 0: query file
						var this_query_count = parseInt(xmlDoc.documentElement.getAttribute('qcount'));
						var this_query_start = parseInt(xmlDoc.documentElement.getAttribute('qstart'));
						var this_query_end = parseInt(xmlDoc.documentElement.getAttribute('qend'));
						
						parserPropfindXML(xmlDoc, open_url, append_result);
							
						//- Create File thumb list				
						createThumbView(this_query_type, "", g_folder_array, g_file_array);
						
						if( this_query_count > this_query_end ){
							var next_start = this_query_end + 1;
							var next_end = next_start + 50;
							if( next_end > this_query_count )
								next_end = this_query_count;
								
							var next_page_html = "<div class='nextDiv' start='" + next_start + "' end='" + next_end + "'><span>";
							var a = m.getString("title_next_query");
							a = a.replace("%s", next_end);
							a = a.replace("%s", this_query_count);
							next_page_html += a;
							next_page_html += "</span></div>";
							$("#main_right_container #fileview").append(next_page_html);
							
							$(".nextDiv").click(function(){
								doPROPFINDMEDIALIST(open_url, true, null, media_type, $(this).attr("start"), $(this).attr("end"), keyword, orderby, orderrule);
							});
						}
						
						g_medialist_mode = 1;
						
						$("div#btnUpload").css("display", "none");
						$("div#btnSelect").css("display", "block");
						$("div#btnNewDir").css("display", "none");
						$("div#btnPlayImage").css("display", "block");
						$("div#boxSearch").css("display", "block");
						
						g_storage.set('openurl', open_url);
						
						$("div#hostview").scrollTop(g_storage.get('hostviewscrollTop'));
						
						$('div#fileview').scrollLeft(g_storage.get('contentscrollLeft'));
						$('div#fileview').scrollTop(g_storage.get('contentscrollTop'));
						
						$("div#boxSearch").attr("uhref", open_url);
						$("div#boxSearch").attr("qtype", media_type);
						
						adjustLayout();
						
						closeJqmWindow(0);
						
						if(complete_handler!=undefined){
							complete_handler();
						}
					}
				}
		  		else if(error==501){
					doPROPFINDMEDIALIST(open_url);
				}
		  		else if(error==401){
		  			setTimeout(openLoginWindow(open_url), 2000);
				}
				else{
					alert(m.getString(error));					
				}
				
				showHideLoadStatus(false);
			}
		}, null, media_type, start, end, keyword, orderby, orderrule );
		
		resetTimer();
	}
	catch(err){
		//Handle errors here
	  alert('catch error: '+ err);
	}
		
}

function parserPropfindXML(xmlDoc, open_url, append_result){
	if(!xmlDoc)				
		return;
	
	if(append_result==false){	
		g_folder_array = null;
		g_file_array = null;
	}
	
	if(g_folder_array==null) g_folder_array = new Array();
	if(g_file_array==null) g_file_array = new Array();
		
	var i, j, k, l, n;						
	var x = xmlDoc.documentElement.childNodes;
						
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
		var this_user_agent = "";
		var this_router_sync_folder = "0";
		var this_matadata_title = "";
		
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
							else if(a[l].nodeName=="D:getuseragent"){												
								this_user_agent = String(a[l].childNodes[0].nodeValue);
								g_storage.set('user_agent', this_user_agent);
							}
							else if(a[l].nodeName=="D:getroutersync"){												
								this_router_sync_folder = String(a[l].childNodes[0].nodeValue);
							}
							else if(a[l].nodeName=="D:getmatadata"){												
								var bb = a[l].childNodes;												
								for (n=0;n<bb.length;n++){
									if(bb[n].nodeName=="D:title")
										this_matadata_title = bb[n].childNodes[0].nodeValue;
								}
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
										
						var len = (g_listview==0) ? 12 : 45;	
														
						if(this_short_name.length>len){
							this_short_name = this_short_name.substring(0, len) + "...";
						}
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
					                  routersyncfolder: this_router_sync_folder,
									  matadatatitle: this_matadata_title });
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
					                routersyncfolder: this_router_sync_folder,
									matadatatitle: this_matadata_title });
			}
		}
	}
}

function content2XMLDOM(content){
	
	if(content=="")
		return null;
		
	var parser = null;
	var xmlDoc = null;
					
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
			return null;
		}
	}
	
	return xmlDoc;
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
					
					var xmlDoc = content2XMLDOM(content);
					
					if(xmlDoc){
						
						var this_query_type = xmlDoc.documentElement.getAttribute('qtype'); //- 2: query host, 1: query share, 0: query file
						var this_folder_readonly = xmlDoc.documentElement.getAttribute('readonly');
						var this_router_username = xmlDoc.documentElement.getAttribute('ruser');
						var this_computer_name = xmlDoc.documentElement.getAttribute('computername');
						var this_isusb = xmlDoc.documentElement.getAttribute('isusb'); 
						
						parserPropfindXML(xmlDoc, open_url, false);
						
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
						
						//- Create host list
						createHostList(this_query_type, g_folder_array);	
						
						//- Create File thumb list				
						createThumbView(this_query_type, parent_url, g_folder_array, g_file_array);
						
						g_medialist_mode = 0;
						
						if(this_query_type==0&&g_select_mode==0){
							$("div#btnUpload").css("display", "block");
							$("div#btnSelect").css("display", "block");
							$("div#btnNewDir").css("display", "block");
							$("div#btnPlayImage").css("display", "block");
						}
						else{
							$("div#btnUpload").css("display", "none");
							$("div#btnSelect").css("display", "none");
							$("div#btnNewDir").css("display", "none");
							$("div#btnPlayImage").css("display", "none");
						}
						
						$("div#boxSearch").css("display", "none");
						$("div#boxSearch input").val("");
							
						if(g_select_mode==1)
							$("#btnCancelSelect").css("display", "block");
							
						if(g_upload_mode==1)
							$("#btnCancelUpload").css("display", "block");
						
						if(this_query_type==1&&this_isusb==0)
							$("#btnChangeUser").css("display", "block");
						else
							$("#btnChangeUser").css("display", "none");
						
						$("span#username").text(this_router_username);
						
						g_storage.set('openurl', open_url);
						
						$("div#hostview").scrollTop(g_storage.get('hostviewscrollTop'));
						
						$('div#fileview').scrollLeft(g_storage.get('contentscrollLeft'));
						$('div#fileview').scrollTop(g_storage.get('contentscrollTop'));
						
						adjustLayout();
						
						closeJqmWindow(0);
						
						//registerPage(open_url);
						
						if(complete_handler!=undefined){
							complete_handler();
						}
					}
		  		}
		  		else if(error==501){
					doPROPFIND(open_url);
				}
		  		else if(error==401){
		  			setTimeout(openLoginWindow(open_url), 2000);
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

function registerPage(open_url){
	
	if( open_url != "/" || isPrivateIP( g_storage.get('wan_ip') ) )
		return;
	
	var ddns_name = g_storage.get('ddns_host_name');
	
	if(ddns_name==""){
		alert("start ddns process");
		return;
	}
	
	var alcloud_url = "https://" + ddns_name;	
	window.location = "http://140.130.25.39/aicloud?v=" + alcloud_url;
}

function doLOGIN(path, auth){	
	doPROPFIND(path, function(){		
		if( g_opening_uid != '' ){
			g_storage.set('openhostuid', g_opening_uid);
			
			$("#hostview .host_item").each(function(){
				if($(this).attr("uid") == g_opening_uid){
					$(this).addClass("select");
				}
				else{
					$(this).removeClass("select");
				}
			});	
			g_opening_uid='';
		}
	}, auth);
}	

function refreshHostList(){
	if(client==null)
		return;
	
	try{
		client.PROPFIND('/', null, function(error, statusstring, content){		
			if(error){				
				if(error==207){
										
					var parser;
					var xmlDoc;
							
					var folder_array = new Array();

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
										}
									}
								}
							}
							else if(y[j].nodeType==1&&y[j].nodeName=="D:href"){
								this_href = String(y[j].childNodes[0].nodeValue);
								//alert(this_href);
								var cur_host = "";
								
								if(this_href.match(/^http/))						
									cur_host = window.location.protocol + "//" + window.location.host;	
								
								var open_url = "/";
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
										
										var len = (g_listview==0) ? 12 : 45;	
														
										if(this_short_name.length>len){
											this_short_name = this_short_name.substring(0, len) + "...";
										}
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
									folder_array.push({ contenttype: this_contenttype, 
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
										                  fhidden: this_attr_hidden });
								}
						}
					}
					
					//- Sort By Name
					folder_array.sort(sortByName);
					
					//- Move the usbdisk to first
					var index;
					for(index=0; index<folder_array.length; index++){
						if(folder_array[index].type == "usbdisk"){
							arraymove(folder_array, index, 0);
						}
					}
					
					//- parent url
					//var parent_url = ="/";
					
					//- Create host list
					createHostList(this_query_type, folder_array);
		  	}		  	
			}
		}, null, 1);
	}
	catch(err){
		//Handle errors here
	  alert('catch error: '+ err);
	}
}

function createHostList(query_type, g_folder_array){
	
	var host_item_html = "";
	$("#hostview").empty();
	
	var items = g_storage.get("HostList") ? g_storage.get("HostList").split(/,/) : new Array();
	var on_rescan_samba = g_storage.getl("onRescanSamba");
	var on_rescan_samba_count = g_storage.getl("onRescanSambaCount");
	
	if(query_type==2){	
		for(var i=0;i<g_folder_array.length;i++){	
			var s = g_folder_array[i].href + "|" + 
							g_folder_array[i].name + "|" + 
							g_folder_array[i].online + "|" + 
							g_folder_array[i].ip + "|" + 
							g_folder_array[i].mac + "|" + 
							g_folder_array[i].uid + "|" + 
							g_folder_array[i].freadonly + "|" + 
							g_folder_array[i].fhidden + "|" + 
							g_folder_array[i].type;
							
			if(!items.contains(s)){    			
				items.push(s);
			}
		
			g_storage.set("HostList", items.join(','));
		}
	}
					        
	for(var i=0;i<items.length;i++){	
		var a = items[i].split('|');
		
		host_item_html += "<div class='host_item unselectable";
		if(a[2]==0) host_item_html += " offline";
		host_item_html += "' uhref='";
		host_item_html += a[0];
		host_item_html += "' title='";
		host_item_html += a[1] + " - " + a[3];
		host_item_html += "' online='";
		host_item_html += a[2];
		host_item_html += "' isdir='1'";
		host_item_html += " ip='";
		host_item_html += a[3];
		host_item_html += "' mac='";
		host_item_html += a[4];
		host_item_html += "' uid='";
		host_item_html += a[5];
		host_item_html += "' freadonly='";
		host_item_html += a[6];
		host_item_html += "' fhidden='";
		host_item_html += a[7];	
							
		if(a[8] == "usbdisk")
			host_item_html += "' isusb='1'";
		else						
			host_item_html += "' isusb='0'";
								
		host_item_html += ">";
		host_item_html += "<div id='hosticon' class='sicon " + a[8];
		if(a[2]==0) host_item_html += "off";
		host_item_html += "'/>";
		
		host_item_html += "<div id='hostname' class='unselectable'>";
		
		host_item_html += "<p>";
		host_item_html += a[1];							
		if(a[2]==0)
			host_item_html += "(" + m.getString("title_offline") + ")";
		host_item_html += "</p>";
		
		host_item_html += "</div>";
		host_item_html += "</div>";
	}
		
	if(on_rescan_samba==1){
		host_item_html += "<div class='scan_item unselectable rescan'>";
		host_item_html += "<div id='hosticon' class='sicon sambapcrescan'/>";		
		host_item_html += "<div id='scanlabel' class='unselectable'>";		
		//host_item_html += "<p>" + m.getString("title_scan") + "(" +  on_rescan_samba_count + ")";
		host_item_html += "<p>" + m.getString("title_scan");
		host_item_html += "</p>";
		host_item_html += "</div>";
		host_item_html += "</div>";
		
		if(g_rescan_samba_timer==0){
			g_rescan_samba_timer = setInterval(function(){
				
				on_rescan_samba_count++;
				
				if(on_rescan_samba_count>=10){
					clearInterval(g_rescan_samba_timer);
					g_storage.setl("onRescanSamba", 0);
					g_storage.setl("onRescanSambaCount", 0);
				}
				else
					g_storage.setl("onRescanSambaCount", on_rescan_samba_count);
				
				refreshHostList();
			}, 5000);
		}
	}

	$("#hostview").append(host_item_html);
		
	var uid = g_storage.get('openhostuid');		
	$("#hostview .host_item").each(function(){
		if($(this).attr("uid") == uid){
			$(this).addClass("select");
		}
		else{
			$(this).removeClass("select");
		}
	});
			
	//- Add Event Function
	$("#hostview .host_item").click(function(){
		
		if(confirmCancelUploadFile()==0)
			return;
			
		//- Wake on lan
		if( $(this).attr("online")==0 ){
			var r=confirm(m.getString('wol_msg'));
			
			if (r==true){
				client.WOL("/", $(this).attr("mac"), function(error){
					if(error==200)
						alert(m.getString('wol_ok_msg'));
					else
						alert(m.getString('wol_fail_msg'));
				});
			}
								
			return;
		}
		
		g_opening_uid = $(this).attr("uid");
		var open_url = $(this).attr("uhref");
		
		g_storage.set('opentype', ($(this).attr("type")=="usbdisk")?"1":"0");
							
		doPROPFIND(open_url, function(){
			g_storage.set('openhostuid', g_opening_uid);			
			$("#hostview .host_item").each(function(){
				if($(this).attr("uid") == g_opening_uid){
					$(this).addClass("select");
				}
				else{
					$(this).removeClass("select");
				}
			});	
			g_opening_uid='';
		}, null);
		
	});
}

function createThumbView(query_type, parent_url, folder_array, file_array){
	var html = "";
	
	$("#main_right_container #fileview").empty();
	
	if(query_type==2)
		return;
		
	//- Parent Path
	if(query_type == 0) {
		html += '<div class="albumDiv">';
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';
		html += '<tr><td>';
		html += '<div class="picDiv cb" isParent="1" popupmenu="0" uhref="';
		html += parent_url;
		html += '">';
		html += '<div class="parentDiv bicon"></div></div>';
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo">';
		html += '<a id="list_item" qtype="1" isdir="1" uhref="';
		html += parent_url;
		html += '" title="' + m.getString("btn_prevpage") + '" online="0">' + m.getString("btn_prevpage");
		html += '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
	}
	
	var isInUSBFolder = 0;
	
	//- Folder List
	for(var i=0; i<folder_array.length; i++){
		var this_title = m.getString("table_filename") + ": " + folder_array[i].name; 
		
		if(folder_array[i].time!="")
			this_title += "\n" + m.getString("table_time") + ": " + folder_array[i].time;
		                 							
		html += '<div class="albumDiv" ';
		html += ' title="';
		html += this_title;
		html += '">';
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';
					
		html += '<tr><td>';
		html += '<div class="picDiv cb" popupmenu="';
									
		if(query_type == "0")
			html += '1';
		else
			html += '0';
									
		html += '" uhref="';
		html += folder_array[i].href;
		html += '">';
		
		if(query_type == "2"){
			//- Host Query
			if( folder_array[i].type == "usbdisk" )
				html += '<div id="fileviewicon" class="usbDiv bicon">';
			else{
				if(folder_array[i].online == "1")
					html += '<div id="fileviewicon" class="computerDiv bicon">';
				else
					html += '<div id="fileviewicon" class="computerOffDiv bicon">';
			}
		}
		else {
			html += '<div id="fileviewicon" class="folderDiv bicon">';
		}
		
		//- Show router sync icon.
		if( folder_array[i].routersyncfolder == "1" ){
			html += '<div id="routersyncicon" class="routersyncDiv sicon"></div>';
		}
			
		html += '<div class="selectDiv sicon"></div>';
		html += '<div class="selectHintDiv sicon"></div>';
		html += '</div></div>';								
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo">';
		html += '<a id="list_item" qtype="';
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
								
		html += '">';
		
		html += folder_array[i].shortname;
								
		if(folder_array[i].online == "0" && query_type == "2")
			html += "(" + m.getString("title_offline") + ")";
									
		html += '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
		
		if(folder_array[i].type == "usbdisk"&&query_type==1&&g_fileview_only==0){
			client.GETDISKSPACE("/", folder_array[i].name, function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);	
					
					var queryTag = $('a#list_item[title="' + x.find("DiskName").text() + '"]').parents(".albumDiv");
					
					var title = queryTag.attr("title") + "\n" + m.getString("table_diskusedpercent") + ": " + x.find("DiskUsedPercent").text();
					queryTag.attr("title", title);
				}
			});	
			
			isInUSBFolder = 1;				
		}
	}
	
	//- File List
	for(var i=0; i<file_array.length; i++){
		var this_title = m.getString("table_filename") + ": " + file_array[i].name + "\n" + 
		                 m.getString("table_time") + ": " + file_array[i].time + "\n" + 
		                 m.getString("table_size") + ": " + file_array[i].size;
	
		html += '<div class="albumDiv"';
		html += ' title="';
		html += this_title;
		html += '">';
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';									
									
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
																
		html += '<tr><td>';
		if(query_type == "0")
			html += '<div class="picDiv cb" popupmenu="1" uhref="';
		else
			html += '<div class="picDiv cb" popupmenu="0" uhref="';
		html += file_array[i].href;
		html += '">';
		
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += '<div id="fileviewicon" class="imgfileDiv bicon">';
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += '<div id="fileviewicon" class="audiofileDiv bicon">';
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
			      file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += '<div id="fileviewicon" class="videofileDiv bicon">';
		else if(file_ext=="doc"||file_ext=="docx")
			html += '<div id="fileviewicon" class="docfileDiv bicon">';
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += '<div id="fileviewicon" class="pptfileDiv bicon">';
		else if(file_ext=="xls"||file_ext=="xlsx")
			html += '<div id="fileviewicon" class="xlsfileDiv bicon">';
		else if(file_ext=="pdf")
			html += '<div id="fileviewicon" class="pdffileDiv bicon">';
		else{
			html += '<div id="fileviewicon" class="fileDiv bicon">';
		}
		
		html += '<div class="selectDiv sicon"></div>';
		html += '<div class="selectHintDiv sicon"></div>';
		html += '</div></div>';
		
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo" style="font-size:80%">';
		html += '<a id="list_item" qtype="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" title="';
		html += file_array[i].name;
		html += '" matadatatitle="';
		html += file_array[i].matadatatitle;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '">';
		html += file_array[i].shortname;
		html += '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
	}
		
	$("#main_right_container #fileview").append(html);
	
	$(".cb").mousedown( onMouseDownPicDIVHandler );
	/*
	$(".mediaListDiv").mousedown( function(){
		var item = $(this).parents('.thumb-table-parent').find("a#list_item");
		var qtype = item.attr("qtype");
		var uhref = item.attr("uhref");
		
		doPROPFINDMEDIALIST(uhref, false, function(){
			
		}, qtype, "0", "50", null, "TIMESTAMP", "DESC");
	});
	*/
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
		$("#loading").css("display", "block");
	else
		$("#loading").css("display", "none");
}

function onMouseDownPicDIVHandler(e){
	if( e.button == 2 || g_on_button_animation == 1 ) {
		return false; 
	}
	
	if(g_select_mode==1){		
		
		if($(this).attr("isParent")==1){
			cancelSelectMode();
			openSelItem($(this).parents('.thumb-table-parent').find("a#list_item"));
			return;
		}
	
		if($(this).find(".selectDiv").css("display")=="none"){
			$(this).find(".selectDiv").css("display", "block");
			$(this).find(".selectHintDiv").css("display", "none");
		}
		else{
			$(this).find(".selectDiv").css("display", "none");
			$(this).find(".selectHintDiv").css("display", "block");
		}
		
		g_select_array = null;
		g_select_array = new Array();
		
		//- Count select div	
		var select_file_count=0;
		var select_folder_count=0;
		$("#fileview").find(".albumDiv").each(function(){
			if($(this).find(".selectDiv").css("display")=="block"){
				var uhref = $(this).find("#list_item").attr("uhref");
				var isdir = $(this).find("#list_item").attr("isdir");
				var title = myencodeURI($(this).find("#list_item").attr("title"));
				
				if(isdir==1){					
					select_folder_count++;
				}
				else{
					select_file_count++;
				}
				
				g_select_array.push( { isdir:isdir, uhref:uhref, title:title } );
    	}
		});	
		
		//$("#btnDownload").removeClass("disable");
		//$("#btnShareLink").removeClass("disable");
		
		if(g_medialist_mode==0){
			if(select_file_count>0||select_folder_count>0){
				$("#btnDeleteSel").removeClass("disable");
				$("#btnDownload").removeClass("disable");
				$("#btnShareLink").removeClass("disable");
			}
			else{
				$("#btnDeleteSel").addClass("disable");
				$("#btnDownload").addClass("disable");
				$("#btnShareLink").addClass("disable");
			}
			
			if(select_file_count+select_folder_count==1)
				$("#btnRename").removeClass("disable");
			else	
				$("#btnRename").addClass("disable");
		}
		
		return;
	}
	
	openSelItem($(this).parents('.thumb-table-parent').find("a#list_item"));
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
					
					var matadatatitle = item.attr("matadatatitle");
					
					client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
						if(error==200){
							var data = parseXml(statusstring);
							var share_link = $(data).find('sharelink').text();
							var open_url = "";							
								
							share_link = media_hostName + share_link;								
							open_url = '/smb/css/vlc_video.html?v=' + share_link;
							open_url += '&t=' + matadatatitle;	
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
	lan = m.setLanguage(lan);
	
	if( $("body").attr("openurl") != undefined )
		g_storage.set('openurl', $("body").attr("openurl"));
	
	if( $("body").attr("fileview_only") != undefined )
		g_fileview_only = $("body").attr("fileview_only");
	
	var layout_html = "";  
	
  	layout_html += "<div id='header_region' class='unselectable' width='100%' border='0'>";
  	layout_html += "<div id='logo'></div>";
  
  	if( g_fileview_only == 0 ){
		layout_html += "<div id='user'>Welcome home, <span id='username'></span><span id='login_info'></span>";
	  
	  	/*
	  	var login_info = g_storage.get('last_login_info');
	  	if(login_info!=""&&login_info!=undefined){
	  	var login_info_array = String(login_info).split(">");  	
	  	layout_html += "<span id='login_info'>"+m.getString('title_logininfo')+ login_info_array[1] + ", " + m.getString('title_ip') + login_info_array[2] + "</span>";
		}
		*/
		
		layout_html += "</div>";
	}

  	layout_html += "<div id='loading'><img src='/smb/css/load.gif'/><div><span>Loading...</span></div></div>";
  	//layout_html += "<div class='albutton toolbar-button-right' id='btnHelp'><div class='ticon'></div></div>";
  
  	layout_html += "<ul class='navigation' id='lan'>";
	layout_html += "<li>";
  	layout_html += "<dl>";  
  	layout_html += "<dt><a id='" + lan + "'>" + m.getString('lan_'+lan) + "</a></dt>";  
  	for( var i = 0; i < g_support_lan.length; i++ ){
  		if(g_support_lan[i] != lan){
  			layout_html += "<dd><a id='" + g_support_lan[i] + "'>" + m.getString('lan_' + g_support_lan[i]) + "</a></dt>";	
  		}
	}	
	layout_html += "</dl>";
  	layout_html += "</li>"; 
  	layout_html += "<ul>";
  
  	layout_html += "</div>";
  	layout_html += "<div id='main_region' class='unselectable'>";  	
	layout_html += "<div style='width: 100%;height:100%;float:left;position: relative;'>";
	
  	if( g_fileview_only == 0 ){
  		layout_html += "<div id='main_left_region' class='unselectable'>";
  		layout_html += "<div id='infobar' class='unselectable'></div>";
  		layout_html += "<div id='hostview' class='unselectable'></div>";  
  		layout_html += "</div>";
	}
	else{
		layout_html += "<div id='main_left_region' class='unselectable hidehostview'>";
  		layout_html += "</div>";
	}

  	layout_html += "<div id='main_right_region' class='unselectable'>";
  
  	layout_html += "<div id='main_right_container' class='unselectable'>";
  	layout_html += "<div id='infobar' class='unselectable'>";
  	layout_html += "<div id='toolbar' class='unselectable'>";  
  	layout_html += "<div id='urlregion-url'>";
  	layout_html += "<p id='openurl' align='left' width='150' style='padding-left:10px' ></p>";
  	layout_html += "</div>";
  	
  	if( g_fileview_only == 0 ){
		layout_html += "<div class='alsearch toolbar-button-right' id='boxSearch' style='display:none'><input type='text'/></div>";
		layout_html += "<div class='albutton toolbar-button-right' id='btnUpload' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnPlayImage' style='display:none'><div class='ticon'></div></div>";
		layout_html += "<div class='albutton toolbar-button-right' id='btnNewDir' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnSelect' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnCancelSelect' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnCancelUpload' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnChangeUser' style='display:none'><div class='ticon'></div></div>";
	}

  	layout_html += "</div>";
  	layout_html += "</div>";
  	layout_html += "<div id='fileview' class='unselectable'></div>";
  	
  	//- button panel
  	layout_html += "<div id='button_panel' class='unselectable'>";
  	layout_html += "<div class='abbutton toolbar-button-right disable' id='btnShareLink' style='display:block'><div class='ticon'></div></div>";
	layout_html += "<div class='abbutton toolbar-button-right disable' id='btnDeleteSel' style='display:block'><div class='ticon'></div></div>";
  	layout_html += "<div class='abbutton toolbar-button-right disable' id='btnDownload' style='display:block'><div class='ticon'></div></div>";
	layout_html += "<div class='abbutton toolbar-button-right disable' id='btnRename' style='display:block'><div class='ticon'></div></div>";
  	//layout_html += "<div class='abbutton toolbar-button-right disable' id='btnUnLock' style='display:block'><div class='ticon'></div></div>";
  	//layout_html += "<div class='abbutton toolbar-button-right disable' id='btnLock' style='display:block'><div class='ticon'></div></div>";
  	layout_html += "</div>";
  
   	//- upload panel
  	layout_html += "<div id='upload_panel' class='unselectable'>";
  	layout_html += "<div style='position:absolute;width:98%;height:100%;padding:10px'>";
  	layout_html += "<iframe src='' width='100%' height='100%' frameborder='0'/>";  
  	layout_html += "</div>";
  	layout_html += "</div>";
  
	layout_html += "</div>";
  	layout_html += "</div>";
  	layout_html += "</div>";
	layout_html += "</div>";
	
  	layout_html += "<div id='bottom_region' class='unselectable'>";  
  
  	if( g_fileview_only == 0 ){
		//- Setting
	  	layout_html += "<ul class='navigation' id='setting'>";
		layout_html += "<li>";
	  	layout_html += "<dl>";
	  
	  	//- for test
	  	//layout_html += "<dd><a id='test'>" + m.getString('title_test') + "</a></dt>";
	  
	  	layout_html += "<dd><a id='help'>" + m.getString('title_help') + "</a></dt>";
	  	layout_html += "<dd><a id='version'>" + m.getString('title_version') + "</a></dt>";
	  	layout_html += "<dd><a id='sharelink'>" + m.getString('title_sharelink') + "</a></dt>";
	  	layout_html += "<dd><a id='rescan_samba'>" + m.getString('title_rescan') + "</a></dt>";
	  	layout_html += "<dd><a id='config'>" + m.getString('btn_config') + "</a></dt>";
	  	layout_html += "<dd><a id='favorite'>" + m.getString('btn_favorite') + "</a></dt>";
		layout_html += "<dd><a id='logout'>" + m.getString('title_logout') + "</a></dt>";
	  	layout_html += "<dt><a>" + m.getString('title_setting') + "</a></dt>";  
		layout_html += "</dl>";
	  	layout_html += "</li>"; 
	  	layout_html += "</ul>";
	
	  	//- Refresh
	  	layout_html += "<ul class='navigation' id='refresh'>";
	  	layout_html += "<li>";
	  	layout_html += "<dl>";
	  	layout_html += "<dt><a>" + m.getString('btn_refresh') + "</a></dt>";
	  	layout_html += "</dl>";
	  	layout_html += "</li>"; 
	  	layout_html += "</ul>";
	  	
	  	//layout_html += "<span class='prompt'><a class='logout'>" + m.getString('title_logout') + "</a></span>";
	  
	  	//- Scemes
		layout_html += "<ul class='themes'>";
		layout_html += "<span>" + m.getString('title_skin') + "</span>";
		layout_html += "<span id='skin1' class='themes_ctrl'></span>";
		layout_html += "<span id='skin2' class='themes_ctrl'></span>";
		layout_html += "</ul>";
	}
	
	layout_html += "<ul class='func_help'>";
	layout_html += "<span id='function_help'></span>";
	layout_html += "</ul>";
	
	layout_html += "<span id='right_info'>ASUSTeK Computer Inc. All rights reserved</span>";
	
  	layout_html += "</div>";
  
  	//- modal window
	layout_html += "<div id='modalWindow' class='jqmWindow'>";
	layout_html += "<div id='jqmTitle'>";
	layout_html += "<span id='jqmTitleText'></span>";
	layout_html += "</div>";
	layout_html += "<div id='jqmMsg'>";
	layout_html += "<span id='jqmMsgText'></span>";
	layout_html += "</div>";
	layout_html += "<div id='jqmContent' style='padding;0px;margin:0px'>";
	layout_html += "</div>";
	layout_html += "</div>";
	
	$("body").empty();
	$("body").append(layout_html);	
	
	$("body").click(function(){
		closePopupmenu();	
	});
	
	$("span#username").click(function(){
		var r=confirm(m.getString('msg_logout_confirm'));
		if (r!=true){
			return;
		}
	
		doLOGOUT();
	});	
	/////////////////////////////////////////////////////////////////////////////////////////
	
	var skin = ( g_storage.getl('skin') == undefined ) ? "skin1" : g_storage.getl('skin');
  	if(skin=="skin1")
  		$("#mainCss").attr("href","/smb/css/style-theme1.css");
  	else if(skin=="skin2")
  		$("#mainCss").attr("href","/smb/css/style-theme2.css");
  	/////////////////////////////////////////////////////////////////////////////////////////
  	
	$("#btnHelp").attr("title", m.getString('title_help'));
	$("#btnLogout").attr("title", m.getString('title_logout'));
	$("#btnConfig").attr("title", m.getString('btn_config'));
	$("#btnUpload").attr("title", m.getString('btn_upload'));
	$("#btnCancelUpload").attr("title", m.getString('btn_cancelupload'));
	$("#btnSelect").attr("title", m.getString('btn_select'));
	$("#btnCancelSelect").attr("title", m.getString('btn_cancelselect'));
	$("#btnNewDir").attr("title", m.getString('btn_newdir'));
	$("#btnShareLink").attr("title", m.getString('btn_sharelink'));
	$("#btnDeleteSel").attr("title", m.getString('btn_delselect'));
	$("#btnDownload").attr("title", m.getString('func_download'));
	$("#btnRename").attr("title", m.getString('btn_rename'));
	$("#btnSetting").attr("title", m.getString('title_setting'));
	$("#btnRefresh").attr("title", m.getString('btn_refresh'));
}

function addtoFavorite(){
	var favorite_title = "AiCloud";
	var ddns = g_storage.get('ddns_host_name');
	var favorite_url = "https://" + ddns;
	var isIEmac = false;
    var isMSIE = isBrowser("msie");
    
	if(ddns==""){ 
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

// Detect if the browser is IE or not.
// If it is not IE, we assume that the browser is NS.
var IE = document.all?true:false

// If NS -- that is, !IE -- then set up for mouse capture
if (!IE) document.captureEvents(Event.MOUSEMOVE)

// Main function to retrieve mouse x-y pos.s
function getMouseXY(e) {
	if (IE) { // grab the x-y pos.s if browser is IE
    	g_mouse_x = event.clientX + document.body.scrollLeft + document.documentElement.scrollLeft;
    	g_mouse_y = event.clientY + document.body.scrollTop + document.documentElement.scrollTop;
  	} else {  // grab the x-y pos.s if browser is NS
    	g_mouse_x = e.pageX;
    	g_mouse_y = e.pageY;
  	}
	
  	if (g_mouse_x < 0){g_mouse_x = 0}
  	if (g_mouse_y < 0){g_mouse_y = 0}
  	return true
}

$(document).ready(function(){
	
	document.oncontextmenu = function() {return false;};	
	document.onmousedown = function(e) {		
		resetTimer();
	};
	document.onmousemove = function(e) {		
		getMouseXY(e);
		resetTimer();
	};
	$(document).keydown(function(e) {
		//- Esc
		if (e.keyCode == 27) {
			
			if(g_select_mode==1)
				cancelSelectMode();
			else if(g_upload_mode==1){
				if(confirmCancelUploadFile()==0)
					return;
			
				closeUploadPanel();
			}
			
			closeJqmWindow();
		}
	});
	
	g_storage.set('stopLogoutTimer', '0');
  	g_storage.set('isOpenModalWindow', '0');
  	g_storage.set('isOnUploadFile', '0');
  	
  	createLayout();
		
	showHideLoadStatus(false);
	
	adjustLayout();

	getRouterInfo();

	if(g_fileview_only==0){
		//- Query host list first.
		var loc = g_storage.get('openurl');
		
		g_storage.set("HostList", "");
		doPROPFIND( "/", function(){	
			
			if(loc!=undefined && loc!="/")
				doPROPFIND(loc);
			
			if( g_storage.get('openhostuid')==undefined||
				g_storage.get('openhostuid')==0){
				
				var help_html = "<div id='help_1'>";
				help_html += "<div id='help_content'>";
				help_html += m.getString('msg_help');
				help_html += "</div>";
				help_html += "<div id='help_image'>";
				help_html += "</div>";
				help_html += "</div>";
		
				$("div#fileview").append(help_html);
			}
			
		}, 0);
	}
	else{
		//- Query host list first.
		var loc = g_storage.get('openurl');
		doPROPFIND(loc);
	}
	///////////////////////////////////////////////////////////////////////////////
	
	function closeModal(hash){
		var $modalWindow = $(hash.w);
		
		hash.o.remove();
		
		$modalWindow.fadeOut('200', function(){
			g_show_modal = 0;
			if(g_reload_page==1){
				var openurl = addPathSlash(g_storage.get('openurl'));
				doPROPFIND(openurl);
			}
		});
	}
	
	function openModal(hash){
		var $modalWindow = $(hash.w);
		var $modalContent = $('#jqmContent', $modalWindow);
		var $modalTitle = $('#jqmTitle', $modalWindow);
		var page_size = getPageSize();
		var page_width = page_size[0];
		var page_height = page_size[1];		
		var newLeft = (page_width - g_modal_window_width)/2;
		var newTop = 0;
		g_show_modal = 1;
		
		var sAgent = navigator.userAgent.toLowerCase();
   	 	var isIE = (sAgent.indexOf("msie")!=-1); //IE
    	var getInternetExplorerVersion = function(){
		   var rv = -1; // Return value assumes failure.
		   if (navigator.appName == 'Microsoft Internet Explorer')
		   {
		      var ua = navigator.userAgent;
		      var re  = new RegExp("MSIE ([0-9]{1,}[\.0-9]{0,})");
		      if (re.exec(ua) != null)
		         rv = parseFloat( RegExp.$1 );
		   }
		   return rv;
		};
				
		if(g_modal_url!=""){
			$modalContent.empty();
			
			var iframe_html = "<iframe src='' frameborder='0' width='100%' height='100%'>";
			$(iframe_html).appendTo($modalContent);
			
			var $modalContainer = $('iframe', $modalWindow);
			$modalContainer.attr('src', g_modal_url);
		}
		
		if(isBrowser("msie")&&getInternetExplorerVersion()<=8)
			g_modal_window_height+=25;
		
		$modalWindow.css({
				width:g_modal_window_width,
				height:g_modal_window_height,
				left:newLeft,
				top:-g_modal_window_height,
				opacity:0,
				display:"block"	
			}).jqmShow().animate({
				width:g_modal_window_width,
				height:g_modal_window_height,
				top:newTop,
				left:newLeft,
				marginLeft:0,
				opacity:1			
			}, 200, function() {
	    		// Animation complete.
	    		adjustLayout();
	    		$(this).css("display","block");
	  	} );
		
	}
	
	$("#modalWindow").jqm({
		overlay:70,
		modal:true,
		target: '#jqmContent',
		onHide:closeModal,
		onShow:openModal
	});
	
	function getWindowSize() {
	  	var myWidth = 0, myHeight = 0;
	  	if( typeof( window.innerWidth ) == 'number' ) {
	  		//Non-IE
	    	myWidth = window.innerWidth;
	    	myHeight = window.innerHeight;
	  	} else if( document.documentElement && ( document.documentElement.clientWidth || document.documentElement.clientHeight ) ) {
	    	//IE 6+ in 'standards compliant mode'
	    	myWidth = document.documentElement.clientWidth;
	    	myHeight = document.documentElement.clientHeight;
	  	} else if( document.body && ( document.body.clientWidth || document.body.clientHeight ) ) {
	   	 	//IE 4 compatible
	    	myWidth = document.body.clientWidth;
	    	myHeight = document.body.clientHeight;
	  	}
	  
	  	return [ myWidth, myHeight ];
	}
	
	function getScrollXY() {
	  	var scrOfX = 0, scrOfY = 0;
	  	if( typeof( window.pageYOffset ) == 'number' ) {
	    	//Netscape compliant
	    	scrOfY = window.pageYOffset;
	    	scrOfX = window.pageXOffset;
	  	} else if( document.body && ( document.body.scrollLeft || document.body.scrollTop ) ) {
	    	//DOM compliant
	    	scrOfY = document.body.scrollTop;
	    	scrOfX = document.body.scrollLeft;
	  	} else if( document.documentElement && ( document.documentElement.scrollLeft || document.documentElement.scrollTop ) ) {
	    	//IE6 standards compliant mode
	    	scrOfY = document.documentElement.scrollTop;
	    	scrOfX = document.documentElement.scrollLeft;
	  	}
	  	//window.alert( 'scrOfX = ' + scrOfX );
  		//window.alert( 'scrOfY = ' + scrOfY );
  
	  	return [ scrOfX, scrOfY ];
	}
	
	function getRouterInfo(){
		
		var loc = g_storage.get('openurl');		
		loc = (loc==undefined) ? "/" : loc;
			
		client.GETROUTERINFO(loc, function(error, statusstring, content){
			if(error==200){
				var data = parseXml(content);
				var x = $(data);
    			g_storage.set('webdav_mode', x.find("webdav_mode").text());
    			g_storage.set('http_port', x.find("http_port").text());
    			g_storage.set('https_port', x.find("https_port").text());
    			g_storage.set('http_enable', x.find("http_enable").text());
    			g_storage.set('misc_http_enable', x.find("misc_http_enable").text());
    			g_storage.set('misc_http_port', String(x.find("misc_http_port").text()).replace("\n",""));
    			g_storage.set('misc_https_port', String(x.find("misc_https_port").text()).replace("\n",""));
    			g_storage.set('last_login_info', x.find("last_login_info").text());
    			g_storage.set('ddns_host_name', x.find("ddns_host_name").text());
    			g_storage.set('router_version', x.find("version").text());
				g_storage.set('aicloud_version', x.find("aicloud_version").text());
    			g_storage.set('wan_ip', x.find("wan_ip").text());
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
	
	function webdav_delfile_callbackfunction(error, statusstring, content){
		
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
	}
	
	$("div#btnDeleteSel").click(function(){
		if(g_select_array.length<=0)
			return;
			
		var r=confirm(m.getString('del_files_msg'));
		if (r!=true){
			return;
		}
		
		g_selected_files = null;
		g_selected_files = new Array();
		
		for(var i=0; i<g_select_array.length;i++){			
			g_selected_files.push( g_select_array[i].uhref );
		}
		
		client.DELETE(g_selected_files[0], webdav_delfile_callbackfunction );
	});
	
	$("div#btnNewDir").click(function(){		
		var $modalWindow = $("div#modalWindow");
		g_modal_url = '/smb/css/makedir.html';
		g_modal_window_width = 500;
		g_modal_window_height = 80;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_newdir'));
		if($modalWindow){			
			$modalWindow.jqmShow();
		}
	});
	
	$("div#btnShareLink").click(function(){	
		if(g_select_array.length<=0)
			return;
			
		var share_link_array = new Array();
		var complete_count = 0;
		var eml = "";
		var subj = "?subject=" + m.getString('email_subject');
		var bod = "";
		var selectURL = "";
		var selectFiles = "";
		
		for(var i=0; i < g_select_array.length; i++){			  
		  var this_file_name = g_select_array[i].title;
			var this_full_url = g_select_array[i].uhref;
			var this_isdir = g_select_array[i].isdir;
			var this_url = window.location.href;
			
			/*
			if(this_isdir!=1){
				this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
				
				selectURL = this_url;
				alert(selectURL+","+this_file_name);
				selectFiles += this_file_name;
				selectFiles += ";";
			}
			else{
				this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
				
				selectURL = this_url;
				alert(selectURL+","+this_file_name);
				selectFiles += this_file_name;
				selectFiles += ";";
			}
			*/
			
			this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
				
			selectURL = this_url;
			
			selectFiles += this_file_name;
			selectFiles += ";";
		}
		
		var webdav_mode = g_storage.get('webdav_mode');
		var ddns_host_name = g_storage.get('ddns_host_name');    
    			
		var hostName = (ddns_host_name=="") ? window.location.host : ddns_host_name;
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
						bod += "<br><br>";
				}
				
				var $modalWindow = $("div#modalWindow");
				g_modal_url = '/smb/css/sharelink.html?v='+bod+'&b='+is_private_ip;
				g_modal_window_width = 600;
				g_modal_window_height = 500;
				$('#jqmMsg').css("display", "none");
				$('#jqmTitleText').text(m.getString('title_sharelink'));
				if($modalWindow){
					$modalWindow.jqmShow();
				}
			}
		});
				
	});
	
	var downloadURL = function downloadURL(url){
		var iframe;
	    iframe = document.getElementById("hiddenDownloader");
	    if (iframe === null)
	    {
	        iframe = document.createElement('iframe');  
	        iframe.id = "hiddenDownloader";
	        iframe.style.visibility = 'hidden';
	        document.body.appendChild(iframe);
	    }
	    iframe.src = url; 
	}
	
	var downloadme = function downloadme(x){
		myTempWindow = window.open(x);
		//myTempWindow.document.execCommand('SaveAs','null','download.pdf');
		//myTempWindow.close();
	}

	$("div#btnDownload").click(function(e){
		if(g_select_array.length<=0)
			return;
		
		var array_download_folder = new Array();
		
		for(var i=0; i < g_select_array.length; i++){			  
			var this_file_name = g_select_array[i].title;
			var this_full_url = g_select_array[i].uhref;
			var this_isdir = g_select_array[i].isdir;
			var this_url = window.location.href;
			
			this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));	
			var full_url = this_url + "/" + this_file_name;	
				
			if(this_isdir==1){
				array_download_folder.push(full_url);
				continue;
			}
			
			$.fileDownload(full_url, {
			    successCallback: function (url) {
			 
			        //alert('You just got a file download dialog or ribbon for this URL :' + url);
			    },
			    failCallback: function (html, url) {
			 				
			 				window.open(url);
			 				/*
			        alert('Your file download just failed for this URL:' + url + '\r\n' +
			                'Here was the resulting error HTML: \r\n' + html
			                );*/
			    }
			});
			
			//downloadURL(full_url);
			//window.open(full_url);
			//downloadme(full_url);
		}
		
		//- Download Folder
		var count = array_download_folder.length;
		if(count>0){
			var selectFolder = "";
			for(var i=0; i<count; i++){
				selectFolder += array_download_folder[i];
				
				if(i!=count-1)
					selectFolder += ";";
			}
			
			var $modalWindow = $("div#modalWindow");	
			
			//g_modal_url = '/smb/css/download_folder.html?v='+selectFolder;
			g_modal_url = '/smb/css/download_folder.html?v='+selectFolder+'&u='+g_storage.get('openurl');	
			
			g_modal_window_width = 600;
			g_modal_window_height = 150;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_download_folder'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
	});
	
	$("div#btnRename").click(function(){
		if(g_select_array.length!=1)
			return;
		 
		var this_file_name = g_select_array[0].title;
		var this_full_url = g_select_array[0].uhref;
		var this_isdir = g_select_array[0].isdir;
		
		var $modalWindow = $("div#modalWindow");
		g_modal_url = '/smb/css/rename.html?o='+this_file_name+'&f='+this_full_url+'&d='+this_isdir;		
		
		g_modal_window_width = 500;
		g_modal_window_height = 80;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_rename'));
		if($modalWindow){
			$modalWindow.jqmShow();
		}
	});
	
	$("div#btnLock").click(function(){
		alert(myencodeURI("("));
		if(g_select_array.length!=1)
			return;
		 
		var this_file_name = g_select_array[0].title;
		var this_full_url = g_select_array[0].uhref;
		var this_isdir = g_select_array[0].isdir;
		//var owner = "https://" + window.location.host + this_full_url;
		//var owner = "https://" + window.location.host;
		var owner = "http://johnnydebris.net/";
		//alert("Lock: " + this_full_url + ", " + owner);
		
		client.LOCK(this_full_url, '', function(status, statusstring, content, headers){
			if (status != '201') {
      			alert('Error unlocking: ' + statusstring);
      			return;
      		}
      
      		g_current_locktoken = getLockToken(content);
      		alert("locktoken: " + g_current_locktoken);
		//}, null, 'exclusive', 'write', 0, 60, 'opaquelocktoken:e71d4fae-5dec-22d6-fea5-00a0c91e6be4');
		}, null, 'exclusive', 'write', 0, 3);
				
	});
	         
	$("div#btnUnLock").click(function(){
		if(g_select_array.length!=1)
			return;
		 
		var this_file_name = g_select_array[0].title;
		var this_full_url = g_select_array[0].uhref;
		var this_isdir = g_select_array[0].isdir;
		//var owner = "https://" + window.location.host + this_full_url;
		
		//alert("UNLock: " + this_full_url + ", " + owner);
		
		client.UNLOCK(this_full_url, g_current_locktoken, function(error){
			alert(error);
		});
	});
	
	$("div#btnChangeUser").click(function(){
		
		var r=confirm(m.getString('msg_changeuser_confirm'));
		if (r!=true){
			return;
		}
		
		$("#main_right_container #fileview").empty();
		
		var loc = g_storage.get('openurl');
		client.LOGOUT(loc, function(error){	
			if(error==200){				
				doPROPFIND(loc);
			}
		});
	});
	
	$("div#btnSelect").click(function(){
		g_select_mode = 1;
		
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnSelect").css("display", "none");
		$("div#btnPlayImage").css("display", "none");
		$("div#boxSearch").css("display", "none");
		
		var r = $("div#btnNewDir").width() + $("div#btnUpload").width() + 5;
		
		$("div#btnCancelSelect").css("display", "block");
		$("div#btnCancelSelect").css("right", r);
		
		g_on_button_animation = 1;
		
		$('div#btnCancelSelect').animate({
	    	right: 5
	  	}, 500, function() {
	    	// Animation complete.
	    
	    	$(".selectHintDiv").css("display", "block");
	    	    
	    	$("#fileview #fileviewicon").each(function(){
				$(this).addClass("select");	
			});
			
			adjustLayout();
			
			$("#button_panel").css("left", '1999px');
			$("#button_panel").css("display", "block");			
			$("#button_panel").animate({left:"0px"},"slow", null, function(){
				$("#function_help").text(m.getString('msg_selectmode_help'));			
			});
			
			g_on_button_animation = 0;
	  	});
	});
	
	$("div#btnSelect2").click(function(){		
		g_select_mode = 1;
			
		$(".selectHintDiv").css("display", "block");
				
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnSelect").css("display", "none");
		
		$("div#btnCancelSelect").css("display", "block");
		
		$("#fileview #fileviewicon").each(function(){
			$(this).addClass("select");	
		});
		
		adjustLayout();
		
		$("#button_panel").css("left", '1999px');
		$("#button_panel").css("display", "block");			
		$("#button_panel").animate({left:"0px"},"slow", null, function(){
			$("#function_help").text(m.getString('msg_selectmode_help'));			
		});
	});
	
	$("div#btnCancelSelect").click(function(){
		
		$("div#btnCancelSelect").css("display", "none");
		
		var r = 5;
		
		$("div#btnSelect").css("display", "block");
		$("div#btnSelect").css("right", r);
		
		g_on_button_animation = 1;
		$('div#btnSelect').animate({
	    	right: 85
	  	}, 500, function() {
	    	// Animation complete.	    	    
	    	cancelSelectMode();
			$("div#btnSelect").css("right", 5);
			g_on_button_animation = 0;
	  	});
	  		
	});
	
	$("#btnUpload").click(function(e){
		
		closePopupmenu();
		
		var popupmeny = "<div class='popupmenu' id='popupmenu'>";
		
		//- Upload file
		//if( isBrowser("msie") && getInternetExplorerVersion() <= 9 ){
		//}
		//else{  	
			popupmeny += "<div class='menuitem' id='uploadfile'>";
			popupmeny += "<div class='menuitem-content' style='-webkit-user-select: none;'>";		
			popupmeny += "<span class='menuitem-icon a-inline-block' style='-webkit-user-select: none;'>&nbsp;</span>";
			popupmeny += "<span class='menuitem-container a-inline-block' style='-webkit-user-select: none;'>";
			popupmeny += "<span class='menuitem-caption a-inline-block' style='-webkit-user-select: none;'>";
			popupmeny += "<div style='-webkit-user-select: none;'>" + m.getString("title_upload_file") + "</div>";
			popupmeny += "</span>";		
			popupmeny += "</span>";
			popupmeny += "</div>";
			popupmeny += "</div>";
		//}
	
		//- Upload folder
		popupmeny += "<div class='menuitem' id='uploadfolder'>";
		popupmeny += "<div class='menuitem-content' style='-webkit-user-select: none;'>";		
		popupmeny += "<span class='menuitem-icon a-inline-block' style='-webkit-user-select: none;'>&nbsp;</span>";
		popupmeny += "<span class='menuitem-container a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<span class='menuitem-caption a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<div style='-webkit-user-select: none;'>" + m.getString("title_upload_folder") + "</div>";
		popupmeny += "</span>";		
		popupmeny += "</span>";
		popupmeny += "</div>";
		popupmeny += "</div>";
				
		popupmeny += "</div>";
		
		$(popupmeny).appendTo("body");
		
		$(".popupmenu").css("left", ( g_mouse_x - $(".popupmenu").width() ) + "px");
		$(".popupmenu").css("top", g_mouse_y + "px");
		
		$("#uploadfile").click(function(e){
			
			if( ( isBrowser("msie") && getInternetExplorerVersion() <= 9 ) ){
				var $modalWindow = $("div#modalWindow");	
				
				g_modal_url = '/smb/css/upload_file.html';
				g_modal_window_width = 600;
				g_modal_window_height = 150;
				$('#jqmMsg').css("display", "none");
				$('#jqmTitleText').text(m.getString('title_upload_file'));
				if($modalWindow){
					$modalWindow.jqmShow();
				}
			}
			else{
				g_upload_mode = 1;
					
				$("div#btnNewDir").css("display", "none");
				$("div#btnUpload").css("display", "none");
				$("div#btnSelect").css("display", "none");
				$("div#btnPlayImage").css("display", "none");
				$("div#boxSearch").css("display", "none");
						
				$("#upload_panel").css("display", "block");
				
				$("#upload_panel").css("left", '1999px');		
				$("#upload_panel").animate({left:"0px"},"slow");
				
				var this_url = addPathSlash(g_storage.get('openurl'));		
				$("#upload_panel iframe").attr( 'src', '/smb/css/upload.html?u=' + this_url + '&d=1' );
				
				$("#function_help").text(m.getString('msg_uploadmode_help'));
				
				adjustLayout();
			}
		
			closePopupmenu();
		});
		
		$("#uploadfolder").click(function(e){
			if( isBrowser("chrome") ){
				g_upload_mode = 1;
				
				$("div#btnNewDir").css("display", "none");
				$("div#btnUpload").css("display", "none");
				$("div#btnSelect").css("display", "none");
				$("div#btnPlayImage").css("display", "none");
				$("div#boxSearch").css("display", "none");
				//$("div#btnCancelUpload").css("display", "block");
						
				$("#upload_panel").css("display", "block");
				
				$("#upload_panel").css("left", '1999px');		
				$("#upload_panel").animate({left:"0px"},"slow");
				
				var this_url = addPathSlash(g_storage.get('openurl'));		
				$("#upload_panel iframe").attr( 'src', '/smb/css/upload.html?u=' + this_url +'&d=2' );
					
				$("#function_help").text(m.getString('msg_uploadmode_help'));
				
				adjustLayout();
				
				closePopupmenu();
			}
			else{
				var $modalWindow = $("div#modalWindow");	
				
				g_modal_url = '/smb/css/upload_folder.html';
				g_modal_window_width = 600;
				g_modal_window_height = 150;
				$('#jqmMsg').css("display", "none");
				$('#jqmTitleText').text(m.getString('title_upload_folder'));
				if($modalWindow){
					$modalWindow.jqmShow();
				}
			}
		});
		
		return false;
	});
	
	$("#btnUpload2").click(function(){		
			
		g_upload_mode = 1;
			
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnSelect").css("display", "none");
		$("div#btnPlayImage").css("display", "none");
		$("div#boxSearch").css("display", "none");
				
		$("#upload_panel").css("display", "block");
		
		$("#upload_panel").css("left", '1999px');		
		$("#upload_panel").animate({left:"0px"},"slow");
		
		var this_url = addPathSlash(g_storage.get('openurl'));		
		$("#upload_panel iframe").attr( 'src', '/smb/css/upload.html?u=' + this_url );
			
		$("#function_help").text(m.getString('msg_uploadmode_help'));
		
		adjustLayout();
	});
	
	$("#btnCancelUpload").click(closeUploadPanel);
	
	$("div#hostview").scroll(function(e){
		g_storage.set('hostviewscrollTop', $(this).scrollTop());
	});
	
	$("div#fileview").scroll(function(e){
		g_storage.set('contentscrollLeft', $(this).scrollLeft());
		g_storage.set('contentscrollTop', $(this).scrollTop());
	});
	
	$(".navigation#lan dd a").click(function(){
		if(confirmCancelUploadFile()==0)
			return;
			
		var lan = $(this).attr("id");		
		g_storage.set('lan', lan);
		window.location.reload();
	});
	
	$(".navigation#setting dd a").click(function(){		
		var func = $(this).attr("id");
		if(func=="logout"){
			if(confirmCancelUploadFile()==0)
				return;
			
			var r=confirm(m.getString('msg_logout_confirm'));
			if (r!=true){
				return;
			}	
			doLOGOUT();
		}
		else if(func=="favorite"){
			addtoFavorite();
		}
		else if(func=="config"){
			var http_enable = g_storage.get('http_enable'); //- 0: http, 1: https, 2: both
			var misc_http_enable = g_storage.get('misc_http_enable');
	    	var misc_http_port = g_storage.get('misc_http_port');
	    	var misc_https_port = g_storage.get('misc_https_port');
	    	var location_host = window.location.host;
	    	var misc_protocol = "http";
	    	var misc_port = misc_http_port;
	    
	    	if(misc_http_enable==0){
	    		if( !isPrivateIP() ){
	    			alert(m.getString('msg_no_config'));    	
	    			return;
	    		}
	  		}
	  	
	  		if(http_enable=="0"){
	  			misc_protocol = "http";
	  			misc_port = misc_http_port;
	  		}
	  		else if(http_enable=="1"){
	  			misc_protocol = "https";
	  			misc_port = misc_https_port;
	  		}
	  	
	  		var url;
	  		  			
	  		if( isPrivateIP() )
	  			url = misc_protocol + "://" + location_host.split(":")[0];
	  		else{
	  			url = misc_protocol + "://" + location_host.split(":")[0];
	  		
	  			if(misc_port!="")
	  				url += ":" + misc_port; 
	  		}
	  	
	  		window.location = url;
		}
		else if(func=="help"){			
			var $modalWindow = $("div#modalWindow");
			
			var page_size = getPageSize();
			g_modal_window_height = page_size[1]-30;
			
			g_modal_window_width = g_modal_window_height*1.28;
			
			if(g_modal_window_width>page_size[0]){
				g_modal_window_width = page_size[0];
				g_modal_window_height = g_modal_window_width*0.78125;
			}
			
			g_modal_url = '/smb/css/help.html?showbutton=1';
			
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_help'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}	
		}
		else if(func=="rescan_samba"){
			if(confirmCancelUploadFile()==0)
				return;
			
			var r=confirm(m.getString('title_desc_rescan'));
			
			if (r==true)
				doRescanSamba();
		}
		else if(func=="sharelink"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=1&s=1';
			g_modal_window_width = 800;
			g_modal_window_height = page_size[1]-30;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_setting'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
		else if(func=="version"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=2&s=1';
			g_modal_window_width = 800;
			g_modal_window_height = page_size[1]-30;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_setting'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
		else if(func=="other_settings"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=3&s=1';
			g_modal_window_width = 800;
			g_modal_window_height = page_size[1]-30;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_setting'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
		else if(func=="test"){
			
			client.GETROUTERINFO("/AICLOUD306106790/AiCloud", function(error, statusstring, content){				
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
    				g_storage.set('wan_ip', x.find("wan_ip").text());
    			
    				var login_info = g_storage.get('last_login_info');
					if(login_info!=""&&login_info!=undefined){
						var login_info_array = String(login_info).split(">");  	
				  		var info = m.getString('title_logininfo')+ login_info_array[1] + ", " + m.getString('title_ip') + login_info_array[2];
				  		$("#login_info").text(info);
					}
				}
			});
		
			/*
			var apply_url = "http://stage615.asuscomm.com:8082/AICLOUD890940472/Santana";
			//var apply_url = "http://192.168.1.10:8082/AICLOUD306106790/AiCloud";
			var on_request_handler = function(error, content, statusstring){
				alert("requset complete!" + error);
			};
			
			alert(apply_url);
			
			var request1 = getXmlHttpRequest();
			request1.onreadystatechange = wrapHandler(on_request_handler,request1,null);
			request1.open('PROPFIND', apply_url, true);
			request1.setRequestHeader('Depth', 1);
      request1.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
        
      var xml = '<?xml version="1.0" encoding="UTF-8" ?>' +
                '<D:propfind xmlns:D="DAV:">' +
                '<D:allprop />' +
                '</D:propfind>';
			request1.send(xml);
			*/
			/*
			var url = "/AICLOUD1769668842/ASUS";
			doPROPFIND( url, function(){				
				alert("complete");
			}, 0);
			*/
		}
	});
	
	$(".navigation#refresh dt a").click(function(){
		
		if(confirmCancelUploadFile()==0)
			return;
	
		//- Query host list first.
		var loc = g_storage.get('openurl');
		g_storage.set("HostList", "");
		doPROPFIND( "/", function(){				
			loc = (loc==undefined) ? "/" : loc;		
			if(loc!="/")
				doPROPFIND(loc);		
		}, 0);
	});
	
	$(".themes_ctrl").click(function(){
		if(confirmCancelUploadFile()==0)
			return;
			
		var skin = $(this).attr("id");		
		g_storage.setl('skin', skin);
		window.location.reload();
	});
		
	$("div#btnPlayImage").click(function(){
		openImageViewer();
	});
	
	$("div#boxSearch").keyup(function(){
		var keyword = myencodeURI($("div#boxSearch input").val());
		var uhref = $(this).attr("uhref");
		var qtype = $(this).attr("qtype");
		
		doPROPFINDMEDIALIST(uhref, false, function(){
			
		}, qtype, "0", "50", keyword, "TIMESTAMP", "DESC");
		
	});
	
	$("div#logo").click(function(){
		if(g_fileview_only==1)
			return;
			
		doPROPFIND( "/", function(){
			g_storage.set('openhostuid', '');
			window.location.reload();
		}, 0);
	});
	
	$(window).resize(adjustLayout);
});

window.onbeforeunload = function (e) {
	var is_onUploadFile = g_storage.get('isOnUploadFile');
	
	if(is_onUploadFile==1){
		var msg = m.getString('msg_confirm_cancel_upload');		
		var e = e || window.event;
		
	  	//IE & Firefox
	  	if (e) {	  	
	  		e.returnValue = msg;
	  	}
					 
	  	// For Safari
	  	return msg;
	}
};

window.onunload = function (e) {
	
	var is_onUploadFile = g_storage.get('isOnUploadFile');
	
	if(is_onUploadFile==1){
		$("#upload_panel iframe")[0].contentWindow.stop_upload();
	}
};
