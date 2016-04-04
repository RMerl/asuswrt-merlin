var g_storage = new myStorage();
var m = new lang();
var g_modal_url;
var g_modal_window_width = 200;
var g_modal_window_height = 80;
var g_show_modal = 0;
var g_folder_array;
var g_file_array;
var g_root_path = '';
var g_sharelink_openurl_key = '';
var g_image_player_setting = null;

var client = new davlib.DavClient();
client.initialize();

function closeJqmWindow(v){	
	var $modalWindow = $('div#modalWindow');
	
	var $modalContainer = $('iframe', $modalWindow);
	$modalContainer.attr('src', '');
	
	if($modalWindow)
		$modalWindow.jqmHide();
}

function resizeJqmWindow(w,h){
	var $modalWindow = $('.jqmWindow');
	if($modalWindow){
		$modalWindow.css("width", w + "px");
		$modalWindow.css("height", h + "px");
	}
}

function onMouseDownPicDIVHandler(e){
	if( e.button == 2 ) {
		return false; 
	}
	
	openSelItem($(this).parents('.thumb-table-parent').find("a#list_item"));
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
		alert("No audio files");
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
	g_modal_url = '/smb/css/audio.html?a=' + loc + '&alist=' + audio_list + '&index=' + default_index + '&s=0';
	
	g_modal_window_width = 480;
	g_modal_window_height = 160;
	$('#jqmMsg').css("display", "none");	
	$('#jqmTitleText').text(m.getString('title_audioplayer'));
	if($modalWindow){
		$modalWindow.jqmShow();
	}
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
				
   		var img_url = image_array[i].href;
    	
   		if(i==default_index)
   			div_html += '<img src="" path="' + img_url + '" alt="" class="default"/>';
   		else
     		div_html += '<img src="" path="' + img_url + '" alt="" class=""/>';
	}
	
	div_html += '</div>';
	
	div_html += '<div class="barousel_nav">';
	
	div_html += '<div class="barousel_content transparent" style="display: block; ">';
		
	for( var i=0; i<image_array.length; i++ ){
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
	  	g_image_player_setting = null;
	};
	  	
	var init_handler = function(settings){
		g_image_player_setting = settings;
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
		closeHandler: close_handler,
		initCompleteHandler: init_handler
	});
	
	image_array = null;
}

function openSelItem(item){	
	var loc = item.attr("uhref");
	var qtype = item.attr("qtype");
	var isdir = item.attr("isdir");
	var isusb = item.attr("isusb");
	var this_full_url = item.attr("uhref");
	var this_play_full_url = item.attr("playhref");
	var this_file_name = item.attr("title");
	//alert(this_full_url);
	var fileExt = getFileExt(loc);
	
	if( fileExt=="mp4" ||
		  fileExt=="m4v" ||
		  fileExt=="wmv" ||
		  fileExt=="avi" ||
		  fileExt=="rmvb"||
		  fileExt=="rm"  ||
		  fileExt=="mpg" ||
		  fileExt=="mpeg"||
		  fileExt=="mkv" ||
		  fileExt=="mov" ||
		  fileExt=="flv" ) {
		    
		if( isWinOS() ){
			if( isBrowser("msie") && 
				getInternetExplorerVersion() <= 7 ){
				//- The VLC Plugin doesn't support IE 7 or less
				alert(m.getString('msg_vlcsupport'));
			}
			else{
				var $modalWindow = $("div#modalWindow");
					
				var media_hostName = window.location.host;					
				if(media_hostName.indexOf(":")!=-1){
					media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
				}
				media_hostName = "http://" + media_hostName + ":" + g_storage.get('slhp');
				
				open_url = '/smb/css/vlc_video.html?v=' + media_hostName + this_full_url;							
				open_url += '&showbutton=1';
				
				//- subtitle
				var array_srt_files = new Array();				
				for(var i=0;i<g_file_array.length;i++){
					var file_ext = getFileExt(g_file_array[i].href);
					if(file_ext=="srt"){
						array_srt_files.push(g_file_array[i].href);
					}
				}
				
				if(array_srt_files.length>0){
					open_url += '&s=';
					for(var i=0;i<array_srt_files.length;i++){
						open_url += array_srt_files[i];
						if(i!=array_srt_files.length-1) open_url += ";";
					}
				}
				array_srt_files=null;
				
				//alert(open_url);
				
				g_modal_url = open_url;
				g_modal_window_width = 655;
				g_modal_window_height = 580;
				$('#jqmMsg').css("display", "none");
				$('#jqmTitleText').text(m.getString('title_videoplayer'));
				if($modalWindow){
					$modalWindow.jqmShow();
				}
				return;
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
		//- It will open with google doc viewer when OS is window or mac and using public ip.
		if( ( isWinOS() || isMacOS() ) && !isPrivateIP() ){
		 	var open_url = 'https://docs.google.com/viewer?url=' + loc;
			window.open(open_url);
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
	
	if(isdir==1){
		doPROPFIND(loc, null, 0);
	}
	else
		window.open(loc);
		//window.location = loc;
}

function sortByName(a, b) {
	var x = a.name.toLowerCase();
  var y = b.name.toLowerCase();
  return ((x < y) ? -1 : ((x > y) ? 1 : 0));
}

function createThumbView(query_type, parent_url, folder_array, file_array){
	var html = "";
	
	$("div#fileview").empty();
	
	if(query_type==2)
		return;
	
	var parent_path = decodeURI(parent_url);
	var root_path = decodeURI(g_root_path);
	
	//- Parent Path
	if( parent_path.indexOf(root_path) != -1 ){
		html += '<div class="albumDiv">';
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';
		html += '<tr><td>';
		html += '<div class="picDiv" isParent="1" popupmenu="0" uhref="';
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
		html += '<div class="picDiv" popupmenu="';
									
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
		
		html += '<tr><td>';
		//- Disable download folder function before we found the solution.
		//html += '<input type="button" class="btnDownload" value="' + m.getString("func_download") + '">';
		html += '</td></tr>';
	
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
		
		/*
		if(folder_array[i].type == "usbdisk"&&query_type==1){	
			//alert(folder_array[i].name+","+folder_array[i].href+","+query_type);
			
			client.GETDISKSPACE("/", folder_array[i].name, function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);	
					
					var queryTag = $('a#list_item[title="' + x.find("DiskName").text() + '"]').parents(".albumDiv");
					
					var title = queryTag.attr("title") + "\n" + m.getString("table_diskusedpercent") + ": " + x.find("DiskUsedPercent").text();
					queryTag.attr("title", title);
					
					//$('a#list_item[title="' + x.find("DiskName").text() + '"]').text(x.find("DiskUsedPercent").text());			
	    		//alert(x.find("DiskUsed").text()+", "+x.find("DiskAvailable").text()+", "+x.find("DiskUsedPercent").text());
				}
			});					
		}
		*/
		
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
			html += '<div class="picDiv" popupmenu="1" uhref="';
		else
			html += '<div class="picDiv" popupmenu="0" uhref="';
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
		
		html += '<tr><td>';
		html += '<input type="button" class="btnDownload" value="' + m.getString("func_download") + '">';
		html += '</td></tr>';
		
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
	}
		
	$("div#fileview").append(html);
	
	$(".picDiv").mousedown( onMouseDownPicDIVHandler );
	$('input.btnDownload').click(function(){
		var list_item = $(this).parents('.thumb-table-parent').find("a#list_item");
		var isdir = list_item.attr("isdir");
		var loc = list_item.attr('uhref');
		
		if(isdir==1){
			var $modalWindow = $("div#modalWindow");
			g_modal_url = '/smb/css/download_folder.html?v='+loc+'&p=slhp&a=agt&u='+g_storage.get(g_sharelink_openurl_key);
			
			g_modal_window_width = 600;
			g_modal_window_height = 150;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_download_folder'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
			
		}
		else{
			window.open(loc);
		}
	});
	
	$('input.btnDownloadAll').click(function(){
		
		var $modalWindow = $("div#modalWindow");
		var openurl = g_root_path;
		openurl = openurl.substr(0, openurl.lastIndexOf("/") );
		g_modal_url = '/smb/css/download_folder.html?v='+g_root_path+'&p=slhp&a=agt&u='+openurl;
		
		g_modal_window_width = 600;
		g_modal_window_height = 150;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_download_folder'));
		
		if($modalWindow){
			$modalWindow.jqmShow();
		}
	});
}

function doPROPFIND(open_url, complete_handler, auth){
	if(client==null)
		return;
	
	//showHideLoadStatus(true);
	
	try{		
		
		client.PROPFIND(open_url, auth, function(error, statusstring, content){		
			if(error){
				
				if(error==207){
										
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
						var this_user_agent = "";
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
											else if(a[l].nodeName=="D:getuseragent"){												
												this_user_agent = String(a[l].childNodes[0].nodeValue);
												g_storage.set('user_agent', this_user_agent);
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
									
									this_name = this_href.substring( this_href.lastIndexOf("/")+1, this_href.length );
														
									if(this_name!=""){
										this_uncode_name = this_name;
										
										this_name = mydecodeURI(this_name);
										this_short_name = this_name;
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
					
					//- parent url
					var parent_url = addPathSlash(open_url);						
					if(parent_url!="/"){
						parent_url = parent_url.substring(0, parent_url.length-1);
						parent_url = parent_url.substring(0, parent_url.lastIndexOf("/"));
						if( parent_url=="" ) parent_url="/";
					}
					
					//- Create File thumb list				
					createThumbView(this_query_type, parent_url, g_folder_array, g_file_array);
					
					g_storage.set(g_sharelink_openurl_key, open_url);
					
					$('div#fileview').scrollLeft(g_storage.get('contentscrollLeft'));
					$('div#fileview').scrollTop(g_storage.get('contentscrollTop'));
					
					closeJqmWindow(0);
					
					if(complete_handler!=undefined){
	  				complete_handler();
	  			}
		  	}
		  	else if(error==501){
					doPROPFIND(open_url);
				}
				else{
					alert(m.getString(error));					
				}
				
				//showHideLoadStatus(false);
			}
		}, null, 1);
		
		//resetTimer();
	}
	catch(err){
		//Handle errors here
	  alert('catch error: '+ err);
	}
}
	
$(document).ready(function(){
	document.oncontextmenu = function() {return false;};	
	
	$(document).keydown(function(e) {
		if (e.keyCode == 27) {
			$('#image_slide_show').close(g_image_player_setting);
		}
		else if(e.keyCode == 37){
			//- left(prev) key
			$('#image_slide_show').prev(g_image_player_setting);
		}
		else if(e.keyCode == 39){
			//- right(next) key
			$('#image_slide_show').next(g_image_player_setting);
		}
	});
	
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
	m.setLanguage(lan);
	
	g_storage.set('slhp', $("div#fileview").attr("port"));
	g_storage.set('agt', "");
	g_root_path = $("div#fileview").attr("rootpath");
	g_sharelink_openurl_key = g_root_path;
	
	var openurl = g_storage.get(g_sharelink_openurl_key);
	if(openurl==undefined) openurl = g_root_path;
	doPROPFIND( openurl, null, 0);
	
	//- modal window
	var modalWindow_html = "";
	modalWindow_html += "<div id='modalWindow' class='jqmWindow'>";
	modalWindow_html += "<div id='jqmTitle'>";
	modalWindow_html += "<span id='jqmTitleText'></span>";
	modalWindow_html += "</div>";
	modalWindow_html += "<div id='jqmMsg'>";
	modalWindow_html += "<span id='jqmMsgText'></span>";
	modalWindow_html += "</div>";
	modalWindow_html += "<div id='jqmContent' style='padding;0px;margin:0px'>";
	modalWindow_html += "</div>";
	modalWindow_html += "</div>";
	$(modalWindow_html).appendTo($("body"));
	
	$('input.btnDownloadAll').attr("value", m.getString('func_downloadall'));
		
	function closeModal(hash){
		var $modalWindow = $(hash.w);
		$modalWindow.fadeOut('2000', function(){
			
			hash.o.remove();
			
			g_show_modal = 0;
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
	  } );
		
	}
	
	$("#modalWindow").jqm({
		overlay:70,
		modal:true,
		target: '#jqmContent',
		onHide:closeModal,
		onShow:openModal
	});
});