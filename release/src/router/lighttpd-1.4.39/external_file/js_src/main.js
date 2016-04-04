var g_support_html5 = 0;
var g_reload_page = 1;
var m = new lang();
var g_storage = new myStorage();
var g_modal_url;
var g_modal_window_width = 200;
var g_modal_window_height = 80;
var timer_idle;
var g_show_modal = 0;
var g_time_out = 900000; //- 15min
var g_time_count = 0;
var g_select_mode = 0;
var g_select_array = null;
var g_select_folder_count = 0;
var g_select_file_count = 0;
var g_upload_mode = 0;
var g_folder_array = null;
var g_file_array = null;
var g_opening_uid = '';
var g_on_rescan_samba = 0;
//var g_aidisk_name = "usbdisk";
var g_support_lan = new Array('en-us', 'zh-tw', 'zh-cn', 'cz', 'pl', 'ru', 'de', 'fr', 'tr', 'th', 'ms', 'no', 'fi', 'da', 'sv', 'br', 'jp', 'es', 'it', 'uk');
var g_current_locktoken = "";
var g_mouse_x = 0;
var g_mouse_y = 0;
var g_fileview_only = 0;

var g_webdav_client = new davlib.DavClient();
g_webdav_client.initialize();
	
// Check for the various File API support.
if (window.File && window.FileReader && window.FileList && window.Blob) {
	// Great success! All the File APIs are supported.
  	g_support_html5 = 1;
} else {
	g_support_html5 = 0;
}

function isListView(){
	return (g_storage.getl('listview') == undefined) ? 1 : g_storage.getl('listview');
}

function isAiModeView(){
	return (g_storage.get('aimode') == undefined) ? -1 : g_storage.get('aimode');
}

function getAiMode(){
	return (g_storage.get('aimode') == undefined) ? -1 : g_storage.get('aimode');
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
	login_html += '<table id="table_login" width="100%" height="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="overflow:hidden;table-layout: fixed;">';	
	login_html += '<tr>';
	login_html += '<td>';
  	//login_html += '<form id="form_login">';
	login_html += '<div id="main">';
	login_html += '<table>';
	login_html += '<tr>';
	login_html += '<td><label id="username">' + m.getString('title_username') + ':</label></td><td><input name="username" class="dialog_text_input" type="text" id="username" autocapitalize="off" maxlength="20" style="width:290px"></td>';
	login_html += '</tr>';
	login_html += '<tr>';
	login_html += '<td><label id="password">' + m.getString('title_password') + ':</label></td><td><input name="password" class="dialog_text_input" type="password" id="password" maxlength="16" style="width:290px"></td>';
	login_html += '</tr>';
	login_html += '</table>';
	login_html += '</div>';
	//login_html += '</form>';
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
	
	g_webdav_client.RESCANSMBPC(openurl, function(error){	
		if(error[0]==2){
			var rescan_samba_timer = g_storage.getl("rescan_samba_timer");
			if(rescan_samba_timer!=0) clearInterval(rescan_samba_timer);
			
			g_storage.setl("onRescanSamba", 1);
			g_storage.setl("onRescanSambaCount", 0);
			g_storage.setl("rescan_samba_timer", 0);
			g_storage.set("HostList", "");
			doPROPFIND("/");
		}
	});
}
	
function doMKDIR(name){	
	var openurl = addPathSlash(g_storage.get('openurl'));
	var this_url = openurl + myencodeURI(name);
	
	var already_exists = 0;
	
	$('.wcb').each(function(index) {		
		if($(this).attr("isdir")=="1"){			
			if(name==$(this).attr("data-name")){
				already_exists = 1;
				alert(m.getString('folder_already_exist_msg'));
				return;
			}
			
		}
	});
	
	if(already_exists==1)
		return;
	
	g_webdav_client.MKCOL(this_url, function(error){
		if(error[0]==2){
			doPROPFIND(openurl);
			closeJqmWindow(0);
		}
		else
			alert(m.getString(error));
	});
}

function doRENAME(old_name, new_name, callbackHandler){
	
	var already_exists = 0;
	var openurl = addPathSlash(g_storage.get('openurl'));
	var this_url = openurl + new_name;
	
	$('.wcb').each(function(index) {
		
		if(new_name==myencodeURI($(this).attr("data-name"))){
			already_exists = 1;
				
			if($(this).attr("isdir")=="1")
				alert(m.getString('folder_already_exist_msg'));
			else
				alert(m.getString('file_already_exist_msg'));
				
			return;
		}
	});
	
	if(already_exists==1){
		if(callbackHandler)
			callbackHandler();
			
		return;
	}
	
	//alert(old_name + '-> ' + this_url);
		
	g_webdav_client.LOCK(old_name, '', function(status, statusstring, content, headers){
		
		if (status != '201') {
     		alert(m.getString("msg_already_lock"));
     		
     		if(callbackHandler)
				callbackHandler();
				
			return;
		}
    
    	var locktoken = getLockToken(content);
    	//alert(locktoken);
		g_webdav_client.MOVE(old_name, this_url, function(error){
			if(error[0]==2){
				doPROPFIND(openurl);
				closeJqmWindow(0);
			}
			else{
				alert(m.getString(error));
				
				if(callbackHandler)
					callbackHandler();
			}
			
			g_webdav_client.UNLOCK(old_name, locktoken, function(error){
				if(error!=204)
					alert("Unlock error: " + error);
					
				if(callbackHandler)
					callbackHandler();
			}); 
			
		}, null, false, locktoken);
	
	}, null);
		
	/*
	g_webdav_client.MOVE(old_name, this_url, function(error){
		if(error[0]==2){
			doPROPFIND(openurl);
			closeJqmWindow(0);
		}
		else
			alert(m.getString(error) + " : " + decodeURI(old_name));
	}, null, false);
	*/
}

function doCOPYMOVE(action, source, dest, overwrite, callbackHandler){
		
	g_webdav_client.LOCK(source, '', function(status, statusstring, content, headers){
		
		if (status != '201') {
     		alert(m.getString("msg_already_lock"));
     		
     		if(callbackHandler)
				callbackHandler(status);
					
			return;
		}
    	
    	var locktoken = getLockToken(content);
    	
    	if(action=="copy"){
			g_webdav_client.COPY(source, dest, function(code){
					
				g_webdav_client.UNLOCK(source, locktoken, function(error){
					if(error!=204)
						alert("Unlock error: " + error);
				});
				
				if(callbackHandler)
					callbackHandler(code);
						
			}, null, (overwrite==true) ? "T" : "F", locktoken);
		}
		else if(action=="move"){
			g_webdav_client.MOVE(source, dest, function(code){
				
				g_webdav_client.UNLOCK(source, locktoken, function(error){
					if(error!=204)
						alert("Unlock error: " + error);
				});
				
				if(callbackHandler)
					callbackHandler(code);
					
			}, null, (overwrite==true) ? "T" : "F", locktoken);
		}
		else{
			alert("Invalid action specified!");
		}
		
	}, null);
}

function getFileViewHeight(){
	return $("#fileview").height()-20;
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
		g_webdav_client.LOGOUT("/", function(error){	
			if(error[0]==2){
				g_storage.set('openhostuid', '0');
				g_storage.set('asus_token', '');
				$.cookie('asus_token', '');
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

function doPROPFINDMEDIALIST(open_url, append_result, complete_handler, media_type, start, end, keyword, orderby, orderrule, parentid){
	if(g_webdav_client==null)
		return;
		
	showHideLoadStatus(true);
	showHideEditUIRegion(false);
	
	try{		
		g_webdav_client.PROPFINDMEDIALIST(open_url, function(error, statusstring, content){
			if(error){	
				if(error==207){
					
					showHideSelectModeUI(false);
					closeUploadPanel();
					
					var xmlDoc = content2XMLDOM(content);
					var this_query_type = 1; //- 2: query host, 1: query share, 0: query file
					var this_query_count = 0;
					var this_query_start = 0;
					var this_query_end = 0;
					var this_scan_status = "Idle";
					
					if(!append_result){
						g_folder_array = null;
						g_folder_array = new Array();
						
						g_file_array = null;
						g_file_array = new Array();
					}
										
					if(xmlDoc){	
						this_query_type = 1; //- 2: query host, 1: query share, 0: query file
						this_query_count = parseInt(xmlDoc.documentElement.getAttribute('qcount'));
						this_query_start = parseInt(xmlDoc.documentElement.getAttribute('qstart'));
						this_query_end = parseInt(xmlDoc.documentElement.getAttribute('qend'));	
						this_scan_status = xmlDoc.documentElement.getAttribute('scan_status');
						parserPropfindXML(xmlDoc, open_url, append_result);
					}
					
					var parent_url = "";
					if(g_ui_mode.get()==1 || g_ui_mode.get()==2 || g_ui_mode.get()==3){
						this_query_type = 0;
					}
					else if(g_ui_mode.get()==6){
						this_query_type = 0;
						parent_url = "goto:music_album";
					}
					else if(g_ui_mode.get()==7){
						this_query_type = 0;
						parent_url = "goto:music_artist";
					}
					
					var list_type = (g_list_view.get()==1) ? "listview" : "thumbview";
					create_ui_view( list_type, $("#main_right_container #fileview"), 
						            this_query_type, parent_url, g_folder_array, g_file_array, onMouseDownListDIVHandler );
								
					if( this_query_count > this_query_end ){
						var next_start = this_query_end + 1;
						var next_end = next_start + 50;
						if( next_end > this_query_count )
							next_end = this_query_count;
							
						var next_page_html = "<div class='nextDiv' start='" + next_start + "' end='" + next_end + "'><span>";
						var a = m.getString("title_next_query");
						a = a.replace("%s", next_start + "-" + next_end);
						a = a.replace("%s", this_query_count);
						next_page_html += a;
						next_page_html += "</span></div>";
						$("#main_right_container #fileview").append(next_page_html);
						
						$(".nextDiv").click(function(){
							doPROPFINDMEDIALIST(open_url, true, null, media_type, $(this).attr("start"), $(this).attr("end"), keyword, orderby, orderrule);
						});
					}
					
					if(this_scan_status=="Scanning"){
						$("#main_right_container #hintbar").text(m.getString("msg_dms_scanning"));
						$("#main_right_container #hintbar").show();
					}
					else
						$("#main_right_container #hintbar").hide();
					
					//- aimode:
					//- -1: none
					//- 1: Ai Photo
					//- 2: Ai Music
					//- 3: Ai Movie
					g_storage.set('aimode', media_type);
		
					$("div#btnUpload").css("display", "none");
					$("div#btnNewDir").css("display", "none");
					$("div#btnPlayImage").css("display", (media_type==1) ? "block" : "none");
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
		  		else if(error==501){
					doPROPFINDMEDIALIST(open_url);
				}
				else if(error==503){
					show_hint_no_mediaserver();
				}
		  		else if(error==401){
		  			setTimeout( function(){
						openLoginWindow(open_url)
					}, 2000);
				}
				else{
					alert(m.getString(error));					
				}
				
				showHideLoadStatus(false);
			}
		}, null, media_type, start, end, keyword, orderby, orderrule, parentid );
		
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
		var this_thumb = "0";
		var this_thumb_image = "";
		
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
							else if(a[l].nodeName=="D:getmetadata"){												
								var bb = a[l].childNodes;												
								for (n=0;n<bb.length;n++){
									if(bb[n].nodeName=="D:title")
										this_matadata_title = bb[n].childNodes[0].nodeValue;
									else if(bb[n].nodeName=="D:thumb")
										this_thumb = bb[n].childNodes[0].nodeValue;
									else if(bb[n].nodeName=="D:thumb_image")
										this_thumb_image = bb[n].childNodes[0].nodeValue;
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
						/*				
						var len = (g_list_view.get()==0) ? 12 : 45;	
														
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
					                  routersyncfolder: this_router_sync_folder,
									  matadatatitle: this_matadata_title,
									  thumb: this_thumb });
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
									matadatatitle: this_matadata_title,
									thumb: this_thumb });
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
	if(g_webdav_client==null)
		return;
	
	showHideLoadStatus(true);
	
	try{		
		g_webdav_client.PROPFIND(open_url, auth, function(error, statusstring, content){		
			if(error){
				
				if(error==207){
										
					showHideSelectModeUI(false);
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
						if(open_url=="/")
							createHostList(this_query_type, g_folder_array);	
						
						var list_type = (g_list_view.get()==1) ? "listview" : "thumbview";
						create_ui_view( list_type, $("#main_right_container #fileview"), 
						            	this_query_type, parent_url, g_folder_array, g_file_array, onMouseDownListDIVHandler );
												
						g_thumb_loader.init($("#main_right_container #fileview"));
						g_thumb_loader.start();
						
						//- Refresh UI
						showHideAiButton( (this_isusb=="1") ? true : false );
						closeAiMode();
						
						if(open_url=="/"){
							$("div#btnThumbView").css("display", "none");
							$("div#btnListView").css("display", "none");
						}
						else{
							$("div#btnThumbView").css("display", (g_list_view.get()==1) ? "block" : "none");
							$("div#btnListView").css("display", (g_list_view.get()==1) ? "none" : "block");
						}
						
						$("div#btnUpload").css("display", (this_query_type==0&&g_select_mode==0) ? "block" : "none");
						$("div#btnNewDir").css("display", (this_query_type==0&&g_select_mode==0) ? "block" : "none");
						$("div#btnPlayImage").css("display", (this_query_type==0&&g_select_mode==0) ? "block" : "none");
						$("#btnCancelUpload").css("display", (g_upload_mode==1) ? "block" : "none");
						$("#btnChangeUser").css("display", (this_query_type==1&&this_isusb==0) ? "block" : "none");						
						
						if(open_url=="/"){
							$("span#username").text(this_router_username);
							getaccountinfo(this_router_username);
						}
						
						g_storage.set('isAidisk', this_isusb);
						g_storage.set('openurl', open_url);
						
						$("div#hostview").scrollTop(g_storage.get('hostviewscrollTop'));
						
						$('div#fileview').scrollLeft(g_storage.get('contentscrollLeft'));
						$('div#fileview').scrollTop(g_storage.get('contentscrollTop'));
						
						$("#main_right_container #hintbar").hide();
						
						adjustLayout();
						
						closeJqmWindow(0);
						
						//registerPage(open_url);
						
						if(complete_handler!=undefined){
							complete_handler();
						}
						
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
					}
		  		}
		  		else if(error==501){
					doPROPFIND(open_url);
				}
		  		else if(error==401){					
					setTimeout( function(){
						openLoginWindow(open_url)
					}, 2000);
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
	if(g_webdav_client==null)
		return;
	
	try{
		g_webdav_client.PROPFIND('/', null, function(error, statusstring, content){		
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
										
										var len = (g_list_view.get()==0) ? 12 : 45;	
														
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

function onMouseDownThumbDIVHandler(e, file_item){
	if( e.button == 2 ) {
		return false; 
	}
	
	if(g_select_mode==1){		
		
		if(file_item.attr("isParent")==1){
			showHideSelectModeUI(false);
			openSelItem(file_item);
			return;
		}
		
		return;
	}
	
	openSelItem(file_item);
}


function onMouseDownListDIVHandler(e, file_item){
	if( e.button == 2 ) {
		return false; 
	}
	
	if(g_select_mode==1){
		if(file_item.attr("isParent")==1){			
			showHideSelectModeUI(false);
			openSelItem(file_item);
			return;
		}
		
		return;
	}
	
	openSelItem(file_item);
}

function openSelItem(item){	

	var loc = (item.attr("uhref")==undefined) ? "" : item.attr("uhref");	
	var qtype = (item.attr("qtype")==undefined) ? 0 : item.attr("qtype");
	var isdir = (item.attr("isdir")==undefined) ? 0 : item.attr("isdir");
	var isusb = (item.attr("isusb")==undefined) ? 0 : item.attr("isusb");
	var this_full_url = loc;
	var this_file_name = (item.attr("data-name")==undefined) ? "" : item.attr("data-name");
	
	g_storage.set('opentype', isusb);
	
	if(loc == "goto:music_album"){
		
		if(g_webdav_client==null)
			return;
			
		showHideLoadStatus(true);
			
		loc = (g_storage.get('openurl')==undefined) ? "/" : g_storage.get('openurl');
		
		g_webdav_client.GETMUSICCLASSIFICATION(loc, 'album', function(error, statusstring, content){				
			if(error==200) createClassificationView(content, "album");
			else if(error==503) show_hint_no_mediaserver();			
			showHideLoadStatus(false);
		});
		
      	return;
	}
	else if(loc == "goto:music_artist"){	
		if(g_webdav_client==null)
			return;
			
		showHideLoadStatus(true);
			
		loc = (g_storage.get('openurl')==undefined) ? "/" : g_storage.get('openurl');
			
		g_webdav_client.GETMUSICCLASSIFICATION(loc, 'artist', function(error, statusstring, content){
			if(error==200) createClassificationView(content, "artist");
			else if(error==503) show_hint_no_mediaserver();
			showHideLoadStatus(false);
		});
		
		return;	
	}
		
	if(qtype==2){
		var online = item.attr("online");			
		if(online==0){
			var r=confirm(m.getString('wol_msg'));
				
			if (r==true){
				var this_mac = item.attr("mac");
								
				g_webdav_client.WOL("/", this_mac, function(error){
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
				if( isIE() && getInternetExplorerVersion() <= 8 ){
					//- The VLC Plugin doesn't support IE 8 or less
					alert(m.getString('msg_vlcsupport'));
					return;
				}
				else{
					var $modalWindow = $("div#modalWindow");
						
					this_file_name = myencodeURI(this_file_name);		
					this_url = this_full_url.substring(0, this_full_url.lastIndexOf('/'));
						
					var media_hostName = window.location.host;					
					if(media_hostName.indexOf(":")!=-1){
						media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
					}
					media_hostName = "http://" + media_hostName + ":" + g_storage.get("http_port") + "/";
					
					g_webdav_client.OPENSTREAMINGPORT("/", 1, function(error, content, statusstring){				
						if(error==200){							
							var matadatatitle = item.attr("matadatatitle");
					
							g_webdav_client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
								if(error==200){
									var data = parseXml(statusstring);
									var srt_share_link = "";
									var share_link = $(data).find('sharelink').text();
									var open_url = "";							
									open_url = '/smb/css/vlc_video.html?v=' + media_hostName + share_link + '&u=' + this_url;
									open_url += '&t=' + matadatatitle;
									open_url += '&showbutton=1';
									
									g_modal_url = open_url;
									g_modal_window_width = 655;
									g_modal_window_height = 580;
									$('#jqmMsg').css("display", "none");
									$('#jqmTitleText').text(m.getString('title_videoplayer'));
									if($modalWindow){
										$modalWindow.jqmShow();
									}
								}
							});
						}
					});
					
					/*
					var matadatatitle = item.attr("matadatatitle");
					
					g_webdav_client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
						if(error==200){
							var data = parseXml(statusstring);
							var srt_share_link = "";
							var share_link = $(data).find('sharelink').text();
							var open_url = "";							
							open_url = '/smb/css/vlc_video.html?v=' + media_hostName + share_link + '&u=' + this_url;
							open_url += '&t=' + matadatatitle;
							open_url += '&showbutton=1';
							
							g_modal_url = open_url;
							g_modal_window_width = 655;
							g_modal_window_height = 580;
							$('#jqmMsg').css("display", "none");
							$('#jqmTitleText').text(m.getString('title_videoplayer'));
							if($modalWindow){
								$modalWindow.jqmShow();
							}
						}
					});
					*/
					
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
							
			g_webdav_client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){
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
		
		var page_size = getPageSize();
		g_image_player.show(loc, page_size[0], page_size[1], g_file_array);

		return;
	}
	
	window.open(loc);
}

function addtoFavorite(){
	var favorite_title = "AiCloud";
	var ddns = g_storage.get('ddns_host_name');
	var favorite_url = "https://" + ddns;
	var isIEmac = false;
    var isMSIE = isIE();
    
	if(ddns==""){		
		favorite_url = window.location.href;
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

function getaccountinfo(username){
	var client = new davlib.DavClient();
	client.initialize();
		
	client.GETACCOUNTINFO( '/', username, function(context, status, statusstring){
		if(context==200){
			
			var data = parseXml(statusstring);
			var x = $(data);
			
			var username = x.find("username").text();
			var type = x.find("type").text();
			var permission = x.find("permission").text();
			var name = x.find("name").text();
			
			$("span#username").text(name);
			g_storage.set('userpermission', permission);	
		}
		else{
			g_storage.set('userpermission', 'user');
		}
		
		refreshAdminUI();
	}, null );
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
	
	if( ( navigator.userAgent.match(/iPhone/i)) || 
	    ( navigator.userAgent.match(/iPod/i))   ||
		( navigator.userAgent.match(/Android/i)) && (navigator.userAgent.match(/Mobile/i)) ) {
		var option = window.location.href;
		option = option.substr(option.lastIndexOf("?")+1, option.length);
		
		if(option.indexOf("desktop=1")!=0){
			var url = window.location.href;		
			url = url.substr(0, url.lastIndexOf("?"));
			window.location = url + '?mobile=1';
			return;
		}
	}
	
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
				showHideSelectModeUI(false);
			else if(g_upload_mode==1){
				//if(confirmCancelUploadFile()==0)
				//	return;
			
				closeUploadPanel();
			}
			
			closeJqmWindow();
		}
		
		if(g_image_player){
			g_image_player.keydown(e);
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
			if(loc!=undefined && loc!="/"){
				doPROPFIND(loc);
			}
			
		}, 0);
	}
	else{
		//- Query host list first.
		var loc = g_storage.get('openurl');
		doPROPFIND(loc);
	}
	
	initAudioPlayer();
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
			
		g_webdav_client.GETROUTERINFO("/", function(error, statusstring, content){			
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
    			g_storage.set('lan_https_port', String(x.find("lan_https_port").text()).replace("\n",""));
    			g_storage.set('last_login_info', x.find("last_login_info").text());
    			g_storage.set('ddns_host_name', x.find("ddns_host_name").text());
    			g_storage.set('router_version', x.find("version").text());
				g_storage.set('aicloud_version', x.find("aicloud_version").text());
				g_storage.set('smartsync_version', x.find("smartsync_version").text());
    			g_storage.set('wan_ip', x.find("wan_ip").text());
				g_storage.set('modalname', x.find("modalname").text());
				g_storage.set('usbdiskname', x.find("usbdiskname").text());
				g_storage.set('dms_enable', x.find("dms_enable").text());
				g_storage.set('account_manager_enable', x.find("account_manager_enable").text());
				g_storage.set('app_installation_url', x.find("app_installation_url").text());
				g_storage.set('https_crt_cn', x.find("https_crt_cn").text());
				g_storage.set('max_sharelink', x.find("max_sharelink").text());
				
    			var login_info = g_storage.get('last_login_info');
				if(login_info!=""&&login_info!=undefined&&login_info!=null){
					var login_info_array = String(login_info).split(">");  	
				  	var info = m.getString('title_logininfo')+ login_info_array[1] + ", " + m.getString('title_ip') + login_info_array[2];
				  	$("#login_info").text(info);
				}
				
				refreshAdminUI();
			}
		});
	}
	
	function webdav_delfile_callbackfunction(error, statusstring, content){
		if(error){
			if(error[0]==2){				
			}
			else{
				alert(m.getString(error)+" : " + decodeURIComponent(g_selected_files[0]));
				return;
			}
		}
		
		g_selected_files.splice(0,1);
		
		if(g_selected_files.length<=0){
			var openurl = addPathSlash(g_storage.get('openurl'));
			doPROPFIND(openurl);
			return;
		}
		
		g_webdav_client.DELETE(g_selected_files[0], webdav_delfile_callbackfunction);		
	}
	
	$("div#btnDeleteSel").click(function(){
		if($(this).hasClass("disable"))
			return false;
		
		if(g_select_array.length<=0)
			return;
		
		g_selected_files = null;
		g_selected_files = new Array();
		
		for(var i=0; i<g_select_array.length;i++){			
			g_selected_files.push( g_select_array[i].uhref );
		}
		
		var r;
		if(g_select_array.length==1)
			r=confirm(m.getString('del_files_msg') + " - " + decodeURIComponent(g_select_array[0].title));
		else
			r=confirm(m.getString('del_files_msg'));
			
		if (r!=true){
			return;
		}
		
		g_webdav_client.DELETE(g_selected_files[0], webdav_delfile_callbackfunction );
	});
	
	$("div#btnNewDir").click(function(){	
		if($(this).hasClass("disable"))
			return false;
				
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
		if($(this).hasClass("disable"))
			return false;
			
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
			
			//- Download file
			download_file(full_url);
		}
		
		//- Download folder
		download_folder(array_download_folder);		
		array_download_folder = null;
	});
	
	$("div#btnRename").click(function(){
		if($(this).hasClass("disable"))
			return false;
			
		if(g_select_array.length!=1)
			return;
		 
		var file_name = g_select_array[0].title;
		var uhref = g_select_array[0].uhref;
		var isdir = g_select_array[0].isdir;
		
		open_rename_window(file_name, uhref, isdir);		
	});
		
	$("div#btnLock").click(function(){
		if($(this).hasClass("disable"))
			return false;
			
		if(g_select_array.length!=1)
			return;
		 
		var this_file_name = g_select_array[0].title;
		var this_full_url = g_select_array[0].uhref;
		var this_isdir = g_select_array[0].isdir;
		//var owner = "https://" + window.location.host + this_full_url;
		//var owner = "https://" + window.location.host;
		var owner = "http://johnnydebris.net/";
		//alert("Lock: " + this_full_url + ", " + owner);
		
		g_webdav_client.LOCK(this_full_url, '', function(status, statusstring, content, headers){
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
		if($(this).hasClass("disable"))
			return false;
			
		if(g_select_array.length!=1)
			return;
		 
		var this_file_name = g_select_array[0].title;
		var this_full_url = g_select_array[0].uhref;
		var this_isdir = g_select_array[0].isdir;
		//var owner = "https://" + window.location.host + this_full_url;
		
		//alert("UNLock: " + this_full_url + ", " + owner);
		
		g_webdav_client.UNLOCK(this_full_url, g_current_locktoken, function(error){
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
		g_webdav_client.LOGOUT(loc, function(error){	
			if(error==200){				
				doPROPFIND(loc);
			}
		});
	});
	
	$("#btnThumbView").click(function(){
		
		//if(confirmCancelUploadFile()==0)
		//	return;
		
		g_list_view.set(0);
		
		$("#btnThumbView").css("display", "none");
		$("#btnListView").css("display", "block");
		
		var loc = g_storage.get('openurl');
		loc = (loc==undefined) ? "/" : loc;
		
		var media_type = isAiModeView();
		if( media_type < 0 ){
			doPROPFIND(loc);
		}
		else{
			var data_id = (g_storage.get('data-id')==undefined)?"":g_storage.get('data-id');
			var keyword = myencodeURI($("div#boxSearch input").val());
			doPROPFINDMEDIALIST(loc, false, null, media_type, "0", "50", keyword, "TIMESTAMP", "DESC", data_id);
		}
	});
	
	$("#btnListView").click(function(){
		g_list_view.set(1);
		
		$("#btnThumbView").css("display", "block");
		$("#btnListView").css("display", "none");
		
		var loc = g_storage.get('openurl');
		loc = (loc==undefined) ? "/" : loc;
		
		var media_type = isAiModeView();		
		if( media_type < 0 ){
			doPROPFIND(loc);
		}
		else{
			var data_id = (g_storage.get('data-id')==undefined)?"":g_storage.get('data-id');
			var keyword = myencodeURI($("div#boxSearch input").val());
			doPROPFINDMEDIALIST(loc, false, null, media_type, "0", "50", keyword, "TIMESTAMP", "DESC", data_id);
		}
	});
	
	$("#btnUpload2").click(function(){		
			
		g_upload_mode = 1;
			
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
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
	
	$(".mediaListDiv").mousedown( function(){	
		//if(confirmCancelUploadFile()==0)
		//	return;
		/*
		if(g_storage.get('dms_enable')=="0"){
			//alert(m.getString("msg_no_mediaserver"));
			$("#main_right_container #hintbar").html(m.getString("msg_no_mediaserver"));
			$("#main_right_container #hintbar").show();
			return;
		}
		*/
		var qtype = $(this).attr("qtype");
		var uhref = g_storage.get('openurl');
		g_storage.set("data-id", "");
		$(".mediaListDiv").removeClass("down");
		$(this).addClass("down");
		
		g_ui_mode.set(qtype);
		
		doPROPFINDMEDIALIST(uhref, false, function(){
			
		}, qtype, "0", "50", null, "TIMESTAMP", "DESC");
	});
	
	$("div#btnAiMusicPopupMenux").click( function(e){
		
		var popupmeny = "<div class='popupmenu' id='popupmenu'>";
		
		popupmeny += "<div class='menuitem' id='classifyByAlbum'>";
		popupmeny += "<div class='menuitem-content' style='-webkit-user-select: none;'>";		
		popupmeny += "<span class='menuitem-icon a-inline-block' style='-webkit-user-select: none;'>&nbsp;</span>";
		popupmeny += "<span class='menuitem-container a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<span class='menuitem-caption a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<div style='-webkit-user-select: none;'>" + m.getString('title_sort_by_album') + "</div>";
		popupmeny += "</span>";		
		popupmeny += "</span>";
		popupmeny += "</div>";
		popupmeny += "</div>";
				
		popupmeny += "<div class='menuitem' id='classifyByArtist'>";
		popupmeny += "<div class='menuitem-content' style='-webkit-user-select: none;'>";		
		popupmeny += "<span class='menuitem-icon a-inline-block' style='-webkit-user-select: none;'>&nbsp;</span>";
		popupmeny += "<span class='menuitem-container a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<span class='menuitem-caption a-inline-block' style='-webkit-user-select: none;'>";
		popupmeny += "<div style='-webkit-user-select: none;'>" + m.getString('title_sort_by_artist') + "</div>";
		popupmeny += "</span>";		
		popupmeny += "</span>";
		popupmeny += "</div>";
		popupmeny += "</div>";
		
		popupmeny += "</div>";
		
		$(popupmeny).appendTo("body");
		
		$(".popupmenu").css("left", ( g_mouse_x - $(".popupmenu").width() ) + "px");
		$(".popupmenu").css("top", ( g_mouse_y - $(".popupmenu").height() ) + "px");
		
		$("#classifyByAlbum").click(function(e){
			if(g_webdav_client==null)
				return;
			
			showHideLoadStatus(true);
			
			var loc = g_storage.get('openurl');
			loc = (loc==undefined) ? "/" : loc;
			
			g_webdav_client.GETMUSICCLASSIFICATION(loc, 'album', function(error, statusstring, content){				
				if(error==200) createClassificationView(content, "album");
				else if(error==503) show_hint_no_mediaserver();						
				showHideLoadStatus(false);
			});
			
			e.preventDefault();
      		return false;
		});
				
		$("#classifyByArtist").click(function(){
			if(g_webdav_client==null)
				return;
			
			showHideLoadStatus(true);
			
			var loc = g_storage.get('openurl');
			loc = (loc==undefined) ? "/" : loc;
			
			g_webdav_client.GETMUSICCLASSIFICATION(loc, 'artist', function(error, statusstring, content){
				if(error==200) createClassificationView(content, "artist");
				else if(error==503) show_hint_no_mediaserver();				
				showHideLoadStatus(false);
			});
		});
	});
	
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
		else if(func=="mobile"){
			var url = window.location.href;
			url = url.substr(0, url.lastIndexOf("?"));
			window.location = url + '?mobile=1';
		}
		else if(func=="favorite"){
			addtoFavorite();
		}
		else if(func=="config"){
			var http_enable = parseInt(g_storage.get('http_enable')); //- 0: http, 1: https, 2: both
			var misc_http_enable = parseInt(g_storage.get('misc_http_enable'));
	    	var misc_http_port = g_storage.get('misc_http_port');
	    	var misc_https_port = g_storage.get('misc_https_port');
	    	var lan_https_port = g_storage.get('lan_https_port');
	    	var location_host = window.location.host;
	    	var misc_protocol = "http";
	    	var misc_port = misc_http_port;
	    
	    	if(misc_http_enable==0){
	    		if( !isPrivateIP() ){
	    			alert(m.getString('msg_no_config'));    	
	    			return;
	    		}
	  		}
	  			
	  		if(http_enable==1){
	  			misc_protocol = "https";
	  			misc_port = misc_https_port;
	  		}
	  		else{
	  			misc_protocol = "http";
	  			misc_port = misc_http_port;
	  		}
	  		
	  		var url;
	  		  			
	  		if( isPrivateIP() ){
	  			url = misc_protocol + "://" + location_host.split(":")[0];
	  			
	  			if(http_enable==1)
	  				url += ":" + lan_https_port; 
	  		}
	  		else{
	  			url = misc_protocol + "://" + location_host.split(":")[0];
	  		
	  			if(misc_port!="")
	  				url += ":" + misc_port; 
	  		}
	  	
	  		window.location = url;
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
		else if(func=="account"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=5&s=1';
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
		else if(func=="crt"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=3&s=1';
			g_modal_window_width = 800;
			g_modal_window_height = page_size[1]-30;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_crt'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
		else if(func=="community"){
			var $modalWindow = $("div#modalWindow");
			var page_size = getPageSize();
			g_modal_url = '/smb/css/setting.html?p=4&s=1';
			g_modal_window_width = 800;
			g_modal_window_height = page_size[1]-30;
			$('#jqmMsg').css("display", "none");
			$('#jqmTitleText').text(m.getString('title_community'));
			if($modalWindow){
				$modalWindow.jqmShow();
			}
		}
		else if(func=="test_func"){
			/*
			g_webdav_client.GETCPUUSAGE("/", function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);
					var cpucount = parseInt(x.find("cpucount").text());
					
					for(var i=0; i<cpucount; i++){
						var cpu = "cpu"+i;
						var usage = x.find(cpu).text();
						alert(cpu+"->"+usage);
					}
				}
				else{
					alert(error);	
				}
			});
			
			g_webdav_client.GETMEMORYUSAGE("/", function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);
					var nTotal = x.find("Total").text();
					var nFree = x.find("Free").text();
					var nUsed = x.find("Used").text();
					alert(nTotal+", "+nFree+", "+nUsed);
				}
				else{
					alert(error);	
				}
			});
			*/
			/*
			g_webdav_client.GETNOTICE("/", "Dec 31 12:00:20", function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);
					var notice_log = x.find("log").text();
					alert(notice_log);
				}
				else{
					alert(error);	
				}
			});
			*/
			/*
			g_webdav_client.APPLYAPP("/", "apply", "", "restart_webdav", function(error, statusstring, content){				
				if(error==200){
					alert("complete");
				}
				else{
					alert(error);	
				}
			});
			*/
			/*
			g_webdav_client.GETROUTERINFO("/AICLOUD306106790/AiCloud", function(error, statusstring, content){				
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
			*/
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
		else if(func=="test_func2"){
			g_webdav_client.NVRAMGET("/", "rc_support;webdav_last_login_info", function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					
					$(data).find('nvram').each(function(){
						var key = $(this).attr("key");
						var value = $(this).attr("value");
						alert("key="+key+", value="+value);
					});
				}
				else{
					alert(error);	
				}
			});
		}
	});
	
	$(".navigation#refresh dt a").click(function(){
		
		//if(confirmCancelUploadFile()==0)
		//	return;
	
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
		var page_size = getPageSize();
		g_image_player.show('', page_size[0], page_size[1], g_file_array);
	});
	
	$("div#boxSearch").keyup(function(){
		var keyword = myencodeURI($("div#boxSearch input").val());
		var uhref = $(this).attr("uhref");
		var qtype = $(this).attr("qtype");
		var data_id = (g_storage.get('data-id')==undefined)?"":g_storage.get('data-id');
		
		doPROPFINDMEDIALIST(uhref, false, function(){
			
		}, qtype, "0", "50", keyword, "TIMESTAMP", "DESC", data_id);
		
	});
	
	/*
	$("div#logo").click(function(){
		if(g_fileview_only==1)
			return;
			
		doPROPFIND( "/", function(){
			g_storage.set('openhostuid', '');
			window.location.reload();
		}, 0);
	});
	*/
	
	$("#button-select-all").click(function(){
		
		var add_to_select_array = function(input){
			var uhref = input.attr("uhref");
			var isdir = input.attr("isdir");
			var title = myencodeURI(input.attr("data-name"));
			
			if(uhref==undefined)
				return false;

			if(isdir==1){					
				g_select_folder_count++;
			}
			else{
				g_select_file_count++;
			}
					
			g_select_array.push( { isdir:isdir, uhref:uhref, title:title } );
			
			return true;
		};
		
		g_select_array = null;
		g_select_array = new Array();		
		g_select_file_count=0;
		g_select_folder_count=0;
		
		$("#fileview .wcb").each(function(){
			var ret = false;
			
			ret = add_to_select_array($(this));
			
			if(ret){
				$(this).find(".item-check").addClass("x-view-selected");
				$(this).find(".item-check").show();
				$(this).css("background-color", "#00C2EB");
			}
		});	
		
		refreshSelectWindow();	
	});
	
	$("#button-unselect-all").click(function(){
		g_select_array = null;
		g_select_file_count=0;
		g_select_folder_count=0;
		
		$("#fileview .wcb").each(function(){				
			$(this).find(".item-check").removeClass("x-view-selected");
			$(this).find(".item-check").hide();
			$(this).find(".item-menu").removeClass("x-view-menu-popup");
			$(this).find(".item-menu").hide();
			$(this).css("background-color", "");
		});
		
		refreshSelectWindow();
	});
	
	$.contextMenu({
        selector: 'div#btnShareLink', 
		trigger: 'left',
		callback: function(key, options) {
			if(key=="upload2facebook"){
				if(g_select_array.length<=0)
					return;
					
				var upload_files = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					var filext = getFileExt(this_full_url);
									
					if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
						upload_files.push(this_full_url);
					}
				}
				
				open_upload2service_window("facebook", upload_files);
				upload_files = null;
			}
			else if(key=="upload2flickr"){
				if(g_select_array.length<=0)
					return;
					
				var upload_files = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					var filext = getFileExt(this_full_url);
									
					if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
						upload_files.push(this_full_url);
					}
				}
				
				open_upload2service_window("flickr", upload_files);
				upload_files = null;
			}
			else if(key=="upload2picasa"){
				if(g_select_array.length<=0)
					return;
					
				var upload_files = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					var filext = getFileExt(this_full_url);
									
					if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
						upload_files.push(this_full_url);
					}
				}
				
				open_upload2service_window("picasa", upload_files);
				upload_files = null;
			}
			else if(key=="upload2twitter"){
				if(g_select_array.length<=0)
					return;
					
				var upload_files = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					var filext = getFileExt(this_full_url);
									
					if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
						upload_files.push(this_full_url);
					}
				}
				
				open_upload2service_window("twitter", upload_files);
				upload_files = null;
			}
			else if(key=="share2facebook"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
								
				open_sharelink_window("facebook", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="share2googleplus"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
								
				open_sharelink_window("googleplus", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="share2twitter"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
								
				open_sharelink_window("twitter", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="share2plurk"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
								
				open_sharelink_window("plurk", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="share2weibo"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
				
				open_sharelink_window("weibo", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="share2qq"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
				
				open_sharelink_window("qq", selectFileArray);
				selectFileArray = null;
			}
			else if(key=="sharelink"){
				if(g_select_array.length<=0)
					return;
				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
					
				open_sharelink_window("other", selectFileArray);
				selectFileArray = null;
			}
        },
        items: {			
			"submenu_upload": { 
				name : m.getString("title_upload2"),
				disabled: function(key, opt) {							
					if(g_select_array.length<=0)
						return true;
						
					for(var i=0; i < g_select_array.length; i++){			  
						var this_full_url = g_select_array[i].uhref;
						var filext = getFileExt(this_full_url);
												
						if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
							return false;
						}
					}
						
					return true;
				},
                items : {
                	"upload2facebook": {
						name: m.getString("title_facebook")
					},
					"upload2flickr": {
						name: m.getString("title_flickr")
					},
					"upload2picasa": {
						name: m.getString("title_picasa")
					},
					"upload2twitter": {
						name: m.getString("title_twitter")
					}
                }
            },
			"submenu_share": { 
				name : m.getString("title_share2"),
                items : {
                	"share2facebook": {
						name: m.getString("title_facebook")
					},
					"share2googleplus": {
						name: m.getString("title_googleplus")
					},
					"share2twitter": {
						name: m.getString("title_twitter")
					},
					"share2plurk": {
						name: m.getString("title_plurk")
					},
					"share2weibo": {
						name: m.getString("title_weibo")
					},
					"share2qq": {
						name: m.getString("title_qq")
					}
                }
            },
			"sharelink": {name: m.getString("title_gen_sharelink")}
        }
    });
	
	$.contextMenu({
        selector: 'div#btnCopyMove', 
		trigger: 'left',
		callback: function(key, options) {
			
			if(key=="copy"||key=="move"){
				if(g_select_array.length<=0)
					return;
				
				var selectURL = g_storage.get('openurl');	
				var selectFileArray = new Array();
					
				for(var i=0; i < g_select_array.length; i++){			  
					var this_file = g_select_array[i].title;
					selectFileArray.push(this_file);
				}
				
				open_copymove_window(key, selectURL, selectFileArray);
				
				selectFileArray = null;
			}
        },
        items: {
        	"copy": {
				name: m.getString("func_copy")
			},
			"move": {
				name: m.getString("func_move")
			}
        }
    });
    
	$.contextMenu({
        selector: 'div#btnUpload', 
		trigger: 'left',
		callback: function(key, options) {
			if(key=="uploadfile")
				open_uploadfile_window();
			else if(key=="uploadfolder")
				open_uploadfolder_window();
        },
        items: {
            "uploadfile": {name: m.getString("title_upload_file")},
			"uploadfolder": {name: m.getString("title_upload_folder")}
        }
    });
	
	$.contextMenu({
        selector: 'div#btnAiMusicPopupMenu', 
		trigger: 'left',
		callback: function(key, options) {
			if(key=="sort_by_album"){
				if(g_webdav_client==null)
					return;
				
				showHideLoadStatus(true);
				
				var loc = g_storage.get('openurl');
				loc = (loc==undefined) ? "/" : loc;
				
				g_webdav_client.GETMUSICCLASSIFICATION(loc, 'album', function(error, statusstring, content){				
					if(error==200) createClassificationView(content, "album");
					else if(error==503) show_hint_no_mediaserver();						
					showHideLoadStatus(false);
				});
			}
			else if(key=="sort_by_artist"){
				if(g_webdav_client==null)
					return;
				
				showHideLoadStatus(true);
				
				var loc = g_storage.get('openurl');
				loc = (loc==undefined) ? "/" : loc;
				
				g_webdav_client.GETMUSICCLASSIFICATION(loc, 'artist', function(error, statusstring, content){
					if(error==200) createClassificationView(content, "artist");
					else if(error==503) show_hint_no_mediaserver();				
					showHideLoadStatus(false);
				});
			}
        },
        items: {
            "sort_by_album": {name: m.getString("title_sort_by_album")},
			"sort_by_artist": {name: m.getString("title_sort_by_artist")}
        }
    });
	
	$.contextMenu({
        selector: '.item-menu', 
		trigger: 'left',
		events: {
			show: function(opt){				
			},
  			hide: function(opt){ 
   				$(this).removeClass("x-view-menu-popup");
 		 	}
		},
        callback: function(key, options) {
			var select_item = $(this).parents(".wcb");
			var uhref = select_item.attr("uhref");
			var file_name = encodeURIComponent(select_item.attr("data-name"));
			var isdir = select_item.attr("isdir");
			
			if(key=="delete"){
				
				if(uhref=="") return;
				
				var r=confirm(m.getString('del_files_msg') + " - " + decodeURIComponent(file_name));
				if (r!=true){
					return;
				}
				
				g_webdav_client.DELETE(uhref, function(){
					var openurl = addPathSlash(g_storage.get('openurl'));
					doPROPFIND(openurl);
				});
			}
			else if(key=="copy"){	
				open_copymove_window(key, g_storage.get('openurl'), file_name);
			}
			else if(key=="move"){	
				open_copymove_window(key, g_storage.get('openurl'), file_name);
			}
			else if(key=="rename"){	
				open_rename_window(file_name, uhref, isdir);
			}
			else if(key=="download"){	
				if(isdir=="1")
					download_folder(uhref);
				else
					download_file(uhref);
			}
			else if(key=="upload2facebook"){
				open_upload2service_window("facebook", uhref);
			}
			else if(key=="upload2flickr"){
				open_upload2service_window("flickr", uhref);
			}
			else if(key=="upload2picasa"){
				open_upload2service_window("picasa", uhref);
			}
			else if(key=="upload2twitter"){
				open_upload2service_window("twitter", uhref);
			}
			else if(key=="share2facebook"){
				open_sharelink_window("facebook", uhref);
			}
			else if(key=="share2googleplus"){
				open_sharelink_window("googleplus", uhref);
			}
			else if(key=="share2twitter"){
				open_sharelink_window("twitter", uhref);
			}
			else if(key=="share2plurk"){
				open_sharelink_window("plurk", uhref);
			}
			else if(key=="share2weibo"){
				open_sharelink_window("weibo", uhref);
			}
			else if(key=="share2qq"){
				open_sharelink_window("qq", uhref);
			}
			else if(key=="sharelink"){				
				var selectFileArray = new Array(0);
				for(var i=0; i < g_select_array.length; i++){			  
					var this_full_url = g_select_array[i].uhref;
					selectFileArray.push(this_full_url);
				}
					
				open_sharelink_window("other", selectFileArray);
				selectFileArray = null;
			}
        },
        items: {
            "delete": {
				name: m.getString("func_delete"),
				disabled: function(){
					if(g_storage.get('aimode')==1||
					   g_storage.get('aimode')==2||
					   g_storage.get('aimode')==3)
					   return true;
				}
			},
			"submenu_copymove": { 
				name : m.getString("title_copymove"),
                items : {
                	"copy": {
						name: m.getString("func_copy")
					},
					"move": {
						name: m.getString("func_move")
					}
                }
            },
			"rename": {
				name: m.getString("func_rename"),
				disabled: function(){
					if(g_storage.get('aimode')==1||
					   g_storage.get('aimode')==2||
					   g_storage.get('aimode')==3)
					   return true;
				}
			},
			"sep1": "---------",
            "download": {
            	name: m.getString("func_download"),
            	disabled: function(){
					if(g_select_array.length<=0)
						return true;
					
					for(var i=0; i < g_select_array.length; i++){
						if(g_select_array[i].isdir=="1"){
							//- Disable download folder function before we found the solution.
							return true;
						}
					}
							
					return false;
				}
			},
			"sep2": "---------",
			"submenu_upload": { 
				name : m.getString("title_upload2"),
				disabled: function(key, opt) {							
					if(g_select_array.length<=0)
						return true;
						
					for(var i=0; i < g_select_array.length; i++){			  
						var this_full_url = g_select_array[i].uhref;
						var filext = getFileExt(this_full_url);
												
						if(filext=='jpg'||filext=='jpeg'||filext=='png'||filext=='gif'){
							return false;
						}
					}
						
					return true;
				},
                items : {
                	"upload2facebook": {
						name: m.getString("title_facebook")
					},
					"upload2flickr": {
						name: m.getString("title_flickr")
					},
					"upload2picasa": {
						name: m.getString("title_picasa")
					},
					"upload2twitter": {
						name: m.getString("title_twitter")
					}
                }
            },
			"submenu_share": { 
				name : m.getString("title_share2"),
                items : {
                	"share2facebook": {
						name: m.getString("title_facebook")
					},
					"share2googleplus": {
						name: m.getString("title_googleplus")
					},
					"share2twitter": {
						name: m.getString("title_twitter")
					},
					"share2plurk": {
						name: m.getString("title_plurk")
					},
					"share2weibo": {
						name: m.getString("title_weibo")
					},
					"share2qq": {
						name: m.getString("title_qq")
					}
                }
            },
			"sharelink": {name: m.getString("title_gen_sharelink")}
        }
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
	if(confirmCancelUploadFile()==0){
		return;	
	}
	/*
	var is_onUploadFile = g_storage.get('isOnUploadFile');
	
	if(is_onUploadFile==1){
		stop_upload();
	}
	*/
};

function doBackgroundPlay(){
	alert("doBackgroundPlay");
}
