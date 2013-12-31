var g_bshowHideAiButton = true;
var g_bshowHideEditUIRegion = false;
var g_hide_aidisk_name = 1;
var g_list_view = {
	/*
	0: list view
	1: thumb view
	*/
	_storage: new myStorage(),
	set: function(view){
		this._storage.setl('listview', view);
	},
	get: function(){
		return this._storage.getl('listview');	
	}
};
var g_ui_mode = {
	/*
	1: Ai Photo
	2: Ai Music
	3: Ai Movie
	6: Ai Music Album
	7: Ai Music Artist
	*/
	_storage: new myStorage(),
	set: function(mode){
		this._storage.set('uimode', mode);
	},
	get: function(){
		return this._storage.get('uimode');	
	}
};

var g_usbdisk_space_handler = {
	_changed: false,
	_usbdisk_query_index: 0,
	_usbdisk_current_query_index: -1,
	_usbdisk_query_timer: 0,
 	_array: new Array(),
    add: function(id, name){
		//- check exists
		if(this.exist(id, name)){
			return;
		}
		
 		var obj = {};
		obj.id = id;
		obj.name = name;
		obj.done = false;
		obj.diskUsedPercent = "0%";
		obj.diskUsed = 0;
		obj.diskAvailable = 0;
        this._array.push(obj);
    },
	exist: function(id, name){
		for(var i=0; i< this.len(); i++){
			if(this.item(i).id==id&&this.item(i).name==name)
				return true;
		}
		return false;
	},
	len: function(){
		return this._array.length;
	},
	item: function(index){
		if( index<0 || index>this._array.length-1 )
			return null;
			
		return this._array[index];
	},	
	refresh: function(){
		if(this.len()<=0)
			return;
		
		var refreshui = function(obj){
			if(obj==null)
				return;
			var queryPTag = "div." + obj.id + " p";	
			var querySpanTag = "div." + obj.id + " .progress-bar span";
					
			$(queryPTag).text(obj.name + " " + bytesToSize(obj.diskUsed) + " / " + bytesToSize(obj.diskAvailable + obj.diskUsed));
			$(querySpanTag).css("width", obj.diskUsedPercent);
		};
		
		var _self = this;
		this._usbdisk_query_index = 0;
		this._usbdisk_current_query_index = -1;
		
		this._usbdisk_query_timer = setInterval(function(){
			
			if(_self._usbdisk_current_query_index==_self._usbdisk_query_index)
				return;
			
			var len = _self.len();	
			var obj = _self.item(_self._usbdisk_query_index);	
			
			if(obj==null)
				return;
			
			var usbdiskid = obj.id;
			var usbdiskname = obj.name;
			var done = obj.done;
			
			_self._usbdisk_current_query_index = _self._usbdisk_query_index;
			
			if(done){
				refreshui(obj);					
				
				if( _self._usbdisk_query_index == len - 1 ){
					//- finish
					clearInterval(_self._usbdisk_query_timer);
				}
				else{
					_self._usbdisk_query_index++;
				}
			}
			else{
				var _client = new davlib.DavClient();
				_client.initialize();
				_client.GETDISKSPACE("/", usbdiskname, function(error, statusstring, content){				
					if(error==200){
						var data = parseXml(content);
						var x = $(data);
						
						obj.diskUsedPercent = (x.find("DiskUsedPercent").text()=="") ? "0%" : x.find("DiskUsedPercent").text();
						obj.diskUsed = parseInt((x.find("DiskUsed").text()=="") ? "0" : x.find("DiskUsed").text())*1024;
						obj.diskAvailable = parseInt((x.find("DiskAvailable").text()=="") ? "0" : x.find("DiskAvailable").text())*1024;
						obj.done = true;
						
						refreshui(obj);
						
						if( _self._usbdisk_query_index == len - 1 ){
							//- finish
							clearInterval(_self._usbdisk_query_timer);
						}
						else{
							_self._usbdisk_query_index++;
						}
					}
				});
				_client = null;
			}
			
		}, 200);		
	}
};

function bytesToSize(bytes) {
    var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    if (bytes == 0) return '0';
    var i = parseInt(Math.floor(Math.log(bytes) / Math.log(1024)));
    return Math.round(bytes / Math.pow(1024, i), 2) + ' ' + sizes[i];
}

function createLayout(){
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
	lan = m.setLanguage(lan);
	g_storage.set('lan', lan);
	
	if( $("body").attr("openurl") != undefined )
		g_storage.set('openurl', $("body").attr("openurl"));
	
	if( $("body").attr("fileview_only") != undefined )
		g_fileview_only = $("body").attr("fileview_only");
	
	var layout_html = "";  
	
  	layout_html += "<div id='header_region' class='unselectable' width='100%' border='0'>";
  	//layout_html += "<div id='logo'></div>";
  
  	if( g_fileview_only == 0 ){
		layout_html += "<div id='user'><div id='user_image' class='sicon'></div><span id='username'></span><span id='login_info'></span>";
	  
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
  		layout_html += "<div id='infobar' class='unselectable'>";
		layout_html += "<div id='logo'></div>";
		layout_html += "<div style='text-align:center;'><img class='production_icon' style='width:auto !important;height:145px;'></div>";
		layout_html += "</div>";
  		layout_html += "<div id='hostview' class='unselectable'></div>";  
  		layout_html += "</div>";
	}
	
  	layout_html += "<div id='main_right_region' class='unselectable'>";
  
  	layout_html += "<div id='main_right_container' class='unselectable'>";
  	layout_html += "<div id='infobar' class='unselectable'>";
  	layout_html += "<div id='toolbar' class='unselectable'>";  
  	layout_html += "<div id='urlregion-url'>";
  	layout_html += "<p id='openurl' align='left' width='150' style='padding-left:10px' ></p>";
  	layout_html += "</div>";
  		
	layout_html += "<div class='albutton toolbar-button-right' id='btnThumbView' style='display:none'><div class='ticon'></div></div>";
	layout_html += "<div class='albutton toolbar-button-right' id='btnListView' style='display:none'><div class='ticon'></div></div>";
	if( g_fileview_only == 0 ){	
		//layout_html += "<div class='alsearch toolbar-button-right' id='boxSearch' style='display:none'><input type='text'/></div>";
		layout_html += "<div class='albutton toolbar-button-right' id='btnUpload' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnPlayImage' style='display:none'><div class='ticon'></div></div>";
		layout_html += "<div class='albutton toolbar-button-right' id='btnNewDir' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnSelect' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnCancelSelect' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnCancelUpload' style='display:none'><div class='ticon'></div></div>";
	  	layout_html += "<div class='albutton toolbar-button-right' id='btnChangeUser' style='display:none'><div class='ticon'></div></div>";
		layout_html += "<div class='alsearch toolbar-button-right' id='boxSearch' style='display:none'><input type='text'/></div>";
	}

  	layout_html += "</div>";
  	layout_html += "</div>";
	
	layout_html += "<div id='hintbar' class='unselectable'></div>";
	
  	layout_html += "<div id='fileview' class='unselectable'></div>";
  	
   	//- upload panel
  	layout_html += "<div id='upload_panel' class='unselectable'>";
  	layout_html += "<div style='position:absolute;width:98%;height:100%;padding:10px'>";
  	layout_html += "<iframe src='' width='100%' height='100%' frameborder='0'/>";  
  	layout_html += "</div>";
  	layout_html += "</div>";
  
	layout_html += "</div>";
  	layout_html += "</div>";
	
	//- button panel
	if( g_fileview_only == 0 ){	
		layout_html += "<div id='button_panel' class='unselectable'>";
		layout_html += "<div id='ui_region'>";
		layout_html += "<div class='abbutton toolbar-button-left mediaListDiv' id='btnAiMusic' style='display:none' qtype='2'>";
		layout_html += "<div class='ticon'><span>" + m.getString("title_audiolist") + "</span></div></div>";
		
		layout_html += "<div class='abbutton toolbar-button-left' id='btnAiMusicPopupMenu' style='display:none' qtype='2'>";
		layout_html += "<div class='picon'></div></div>";
		/*
		layout_html += "<ul class='abbutton toolbar-button-left navigation' id='btnAiMusicPopupMenu' style='display:none'>";
		layout_html += "<li>";
	  	layout_html += "<dl>";	  
	  	layout_html += "<dd><a id='help'>" + m.getString('title_help') + "</a></dt>";
	  	layout_html += "<dd><a id='version'>" + m.getString('title_version') + "</a></dt>";
	  	layout_html += "<dd><a id='sharelink'>" + m.getString('title_sharelink') + "</a></dt>";
	  	layout_html += "<dt><a>" + m.getString('title_setting') + "</a></dt>";  
		layout_html += "</dl>";
	  	layout_html += "</li>"; 
	  	layout_html += "</ul>";
		*/	
		layout_html += "<div class='abbutton toolbar-button-left mediaListDiv' id='btnAiMovie' style='display:none' qtype='3'>";
		layout_html += "<div class='ticon'><span>" + m.getString("title_videolist") + "</span></div></div>";
		
		layout_html += "<div class='abbutton toolbar-button-left mediaListDiv' id='btnAiPhoto' style='display:none' qtype='1'>";
		layout_html += "<div class='ticon'><span>" + m.getString("title_imagelist") + "</span></div></div>";		
		layout_html += "<div id='edit_ui_region' style='display:none'>";
		
		layout_html += "<div class='albutton toolbar-button-right disable' id='btnShareLink' style='display:block'><div class='ticon'></div></div>";
		layout_html += "<div class='albutton toolbar-button-right disable' id='btnDeleteSel' style='display:block'><div class='ticon'></div></div>";
		layout_html += "<div class='albutton toolbar-button-right disable' id='btnDownload' style='display:block'><div class='ticon'></div></div>";
		layout_html += "<div class='albutton toolbar-button-right disable' id='btnRename' style='display:block'><div class='ticon'></div></div>";
		//layout_html += "<div class='abbutton toolbar-button-right disable' id='btnUnLock' style='display:block'><div class='ticon'></div></div>";
		//layout_html += "<div class='abbutton toolbar-button-right disable' id='btnLock' style='display:block'><div class='ticon'></div></div>";
		layout_html += "</div>";
		layout_html += "</div>";
		layout_html += "</div>";
	}	
	/////////////////////////////////////
	
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
	  
	  	//layout_html += "<dd><a id='help'>" + m.getString('title_help') + "</a></dt>";
		layout_html += "<dd><a id='web_help' href='http://event.asus.com/2012/nw/aicloud/faq.html' target='_blank'>" + m.getString('title_help') + "</a></dt>";
		layout_html += "<dd><a id='web_feedback' href='https://vip.asus.com/VIP2/Services/QuestionForm/TechQuery' target='_blank'>" + m.getString('title_feedback') + "</a></dt>";
	  	layout_html += "<dd><a id='version'>" + m.getString('title_version') + "</a></dt>";
	  	layout_html += "<dd><a id='sharelink'>" + m.getString('title_sharelink') + "</a></dt>";
	  	layout_html += "<dd><a id='rescan_samba'>" + m.getString('title_rescan') + "</a></dt>";
	  	layout_html += "<dd><a id='config'>" + m.getString('btn_config') + "</a></dt>";
	  	layout_html += "<dd><a id='favorite'>" + m.getString('btn_favorite') + "</a></dt>";
		layout_html += "<dd><a id='mobile'>" + m.getString('title_mobile_view') + "</a></dt>";
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
		//layout_html += "<ul class='themes'>";
		//layout_html += "<span>" + m.getString('title_skin') + "</span>";
		//layout_html += "<span id='skin1' class='themes_ctrl'></span>";
		//layout_html += "<span id='skin2' class='themes_ctrl'></span>";
		//layout_html += "</ul>";
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
	
	layout_html += getAudioPlayerLayout();
	
	$("body").empty();
	$("body").append(layout_html);	
	
	client.GETPRODUCTICON("/", function(error, statusstring, content){
		if(error==200){
			var data = parseXml(content);
			var x = $(data);		
			$("img.production_icon").attr("src", "data:" + x.find("mimetype").text() + ";base64," + x.find("product_icon").text());
		}
	});
	
	$("#main_region").mousedown(function(e){
		closePopupmenu();
		//e.preventDefault();
      	//return false;
	});
	
	$("span#username").click(function(){
		var r=confirm(m.getString('msg_logout_confirm'));
		if (r!=true){
			return;
		}
	
		doLOGOUT();
	});	
	
	$(".navigation li, .navigation li a").mouseenter(function(){
		$(this).find("dd").fadeIn("fast");
	});
	
	$(".navigation li, .navigation li a").mouseleave(function(){
		$(this).find("dd").fadeOut("fast");
	});
	/////////////////////////////////////////////////////////////////////////////////////////
	
	$("#mainCss").attr("href","/smb/css/style-theme.css");
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

function createHostList(query_type, g_folder_array){
	
	var host_item_html = "";
	$("#hostview").empty();
	
	var items = g_storage.get("HostList") ? g_storage.get("HostList").split(/,/) : new Array();
	var on_rescan_samba = g_storage.getl("onRescanSamba");
	var on_rescan_samba_count = g_storage.getl("onRescanSambaCount");
	var rescan_samba_timer = g_storage.getl("rescan_samba_timer");
	//alert("on_rescan_samba_count: " + on_rescan_samba_count+", on_rescan_samba="+on_rescan_samba);
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
		if(a[8] == "usbdisk") host_item_html += " usbdisk";
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
		
		if(a[8] == "usbdisk"){
			var usbdisk_id = "usbdisk" + (i+10);
			host_item_html += "<div id='hostinfo' class='unselectable " + usbdisk_id + "'>";
			
			g_usbdisk_space_handler.add(usbdisk_id,a[1]);
		}
		else
			host_item_html += "<div id='hostinfo' class='unselectable'>";
		
		host_item_html += "<div id='hostname' class='unselectable'>";
		
		host_item_html += "<p>";
		host_item_html += a[1];							
		if(a[2]==0)
			host_item_html += "(" + m.getString("title_offline") + ")";
		host_item_html += "</p>";
		
		host_item_html += "</div>";
		
		if(a[8] == "usbdisk"){
			host_item_html += "<div id='space'>";
			host_item_html += "<div class='progress-bar'>";
    		host_item_html += "<span></span>";
			host_item_html += "</div>";
			host_item_html += "</div>";
		}
		
		host_item_html += "</div>";
		host_item_html += "</div>";
	}
	
	g_usbdisk_space_handler.refresh();
	
	if(on_rescan_samba==1){
		host_item_html += "<div class='scan_item unselectable rescan'>";
		host_item_html += "<div id='hosticon' class='sicon sambapcrescan'/>";		
		host_item_html += "<div id='scanlabel' class='unselectable'>";		
		//host_item_html += "<p>" + m.getString("title_scan") + "(" +  on_rescan_samba_count + ")";
		host_item_html += "<p>" + m.getString("title_scan") + "</p>";
		host_item_html += "</div>";
		host_item_html += "</div>";
		
		if(rescan_samba_timer==0){
			
			rescan_samba_timer = setInterval(function(){
				
				on_rescan_samba_count++;
				
				if(on_rescan_samba_count>=10){
					
					clearInterval(rescan_samba_timer);
					g_storage.setl("rescan_samba_timer", 0);
					
					g_storage.setl("onRescanSamba", 0);
					g_storage.setl("onRescanSambaCount", 0);
				}
				else
					g_storage.setl("onRescanSambaCount", on_rescan_samba_count);
				
				refreshHostList();
				
			}, 5000);
			
			g_storage.setl("rescan_samba_timer", rescan_samba_timer);
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

function createListView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler){
	var html = "";
	
	//$("#main_right_container #fileview").empty();
	container.empty();
	
	if(query_type==2)
		return;
	
	html += '<table id="ntl" class="table-file-list">';
	html += '<thead>';
	html += '<tr>';
	html += '<th style="width:3%"></th>';
	html += '<th style="width:4%"></th>';
	html += '<th style="width:58%">' + m.getString('table_filename') + '</th>';
	html += '<th style="width:25%">' + m.getString('table_time') + '</th>';
	html += '<th style="width:10%">' + m.getString('table_size') + '</th>';
	html += '</tr>';
	html += '</thead>';
	
	html += '<tbody>';
	
	//- Parent Path
	if(query_type == 0) {
		
		html += '<tr class="listDiv cb cbp" ';
		html += 'qtype="1" isParent="1" isdir="1" uhref="';
		html += parent_url;
		html += '" title="' + m.getString("btn_prevpage") + '" online="0">';
		html += '<td field="check"></td>';
		html += '<td field="icon"><div id="fileviewicon" class="parentDiv sicon"></td>';
		html += '<td field="filename" align="left">' + m.getString("btn_prevpage") + '</td>';
		html += '<td field="time" align="left"></td>';
		html += '<td field="size" align="left"></td>';
		html += '</tr>';
	}
			
	var isInUSBFolder = 0;
	
	//- Folder List
	for(var i=0; i<folder_array.length; i++){
		
		html += '<tr class="listDiv cb"';
		html += ' id="list_item" qtype="';
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
		html += '" data-thumb="';
		html += folder_array[i].thumb;
		html += '" data-name="';
		html += folder_array[i].name;
				
		if(folder_array[i].type == "usbdisk")
			html += '" isusb="1"';
		else						
			html += '" isusb="0"';
		
		html += '">';
		              							
		//html += '<td field="check"><input type="checkbox" id="check_del" name="check_del" class="check_del"></td>';
		html += '<td field="check">';
		html += '<div class="checklist">';
		html += '<div class="selectListDiv sicon"></div>';
		html += '<div class="selectListHintDiv sicon"></div>';
		html += '</div>';
		html += '</td>';
		
		html += '<td field="icon">';		
		if(query_type == "2"){
			//- Host Query
			if( folder_array[i].type == "usbdisk" )
				html += '<div id="fileviewicon" class="usbDiv sicon">';
			else{
				if(folder_array[i].online == "1")
					html += '<div id="fileviewicon" class="computerDiv sicon">';
				else
					html += '<div id="fileviewicon" class="computerOffDiv sicon">';
			}
		}
		else {
			html += '<div id="fileviewicon" class="folderDiv sicon">';
		}
		html += '</td>';
		
		html += '<td field="filename" align="left">' + folder_array[i].name + '</td>';
		html += '<td field="time" align="left">' + folder_array[i].time + '</td>';
		html += '<td field="size" align="left"></td>';
		html += '</tr>';		
	}
	
	//- File List
	for(var i=0; i<file_array.length; i++){
		
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
		
		html += '<tr class="listDiv cb"';		
		html += ' id="list_item" qtype="1" isdir="0" uhref="';
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
		html += '" data-thumb="';
		html += file_array[i].thumb;
		html += '" data-name="';
		html += file_array[i].name;
		html += '">';
		
		//html += '<td field="check"><input type="checkbox" id="check_del" name="check_del" class="check_del"></td>';
		
		html += '<td field="check">';
		//<input type="checkbox" id="check_del" name="check_del" class="check_del">';
		html += '<div class="checklist">';
		html += '<div class="selectListDiv sicon"></div>';
		html += '<div class="selectListHintDiv sicon"></div>';
		html += '</div>';
		html += '</td>';
		
		html += '<td field="icon">';
		
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += '<div id="fileviewicon" class="imgfileDiv sicon">';
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += '<div id="fileviewicon" class="audiofileDiv sicon">';
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
				  file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += '<div id="fileviewicon" class="videofileDiv sicon">';
		else if(file_ext=="doc"||file_ext=="docx")
			html += '<div id="fileviewicon" class="docfileDiv sicon">';
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += '<div id="fileviewicon" class="pptfileDiv sicon">';
		else if(file_ext=="xls"||file_ext=="xlsx")
			html += '<div id="fileviewicon" class="xlsfileDiv sicon">';
		else if(file_ext=="pdf")
			html += '<div id="fileviewicon" class="pdffileDiv sicon">';
		else{
			html += '<div id="fileviewicon" class="fileDiv sicon">';
		}
			
		html += '</td>';
		
		html += '<td field="filename" align="left">' + file_array[i].name + '</td>';
		html += '<td field="time" align="left">' + file_array[i].time + '</td>';
		html += '<td field="size" align="left">' + file_array[i].size + '</td>';
		html += '</tr>';
	}
	
	html += '</tbody>';
	html += '</table>';
	
	html += '<table id="header-fixed" class="table-file-list"></table>';
		
	container.append(html);
	
	$(".cb").mousedown( mousedown_item_handler );
	
	var tableOffset = $("#ntl").offset().top;
	var $header = $("#ntl > thead").clone();
	var $fixedHeader = $("#header-fixed").append($header);
	$("#header-fixed").css("width", $("#ntl").width());
	
	$("#fileview").bind("scroll", function() {
		var offset = $(this).scrollTop();
		if (offset >= tableOffset && $fixedHeader.is(":hidden")) {
			$fixedHeader.show();
		}
		else if (offset < tableOffset) {
			$fixedHeader.hide();
		}
	});
}

function createThumbView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler){
	var html = "";
	
	container.empty();
	
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
		html += '" data-thumb="';
		html += folder_array[i].thumb;
		html += '" data-name="';
		html += folder_array[i].name;
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
			/*
			if(folder_array[i].thumb=="1"){
				html += '<div id="fileviewicon">';
				html += '<img src="data:image/jpeg;base64,' + folder_array[i].thumb + '" width="80px" height="60px" onload="javascript:DrawImage(this,80,60);"></img>';
			}
			else{
				html += '<div id="fileviewicon" class="folderDiv bicon">';
			}
			*/
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
		/*
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
			html += '<div class="picDiv cb" popupmenu="1" uhref="';
		else
			html += '<div class="picDiv cb" popupmenu="0" uhref="';
		html += file_array[i].href;
		html += '" data-thumb="';
		html += file_array[i].thumb;
		html += '" data-name="';
		html += file_array[i].name;
		html += '">';
		
		/*
		if(file_array[i].thumb=="1"){
			html += '<div id="fileviewicon">';
			html += '<img src="data:image/jpeg;base64,' + file_array[i].thumb + '" width="80px" height="60px" onload="javascript:DrawImage(this,80,60);"></img>';
		}
		else{
		*/
		
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
		//}
			
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
		
	container.append(html);
	
	$(".cb").mousedown( mousedown_item_handler );	
}

function createClassificationView(content, type){
	var data = parseXml(content);
	var x = $(data);
	var html = "";
					
	$("#main_right_container #fileview").empty();
					
	$(data).find("item").each(function(i) {
    	var id = $(this).find("id").text();
		var title = $(this).find("title").text();
		var artist = $(this).find("artist").text();
		var thumb_image = $(this).find("thumb_image").text();
						
		/*if(isListView()==1){
		}
		else*/{
					
			html += '<div class="card" data-id="' + id + '" data-no-nav="false">';
			html += '<div class="image-wrapper">';
			html += '<div class="image-inner-wrapper" id="' + type + '">';
			if(thumb_image!=""){
				html += '<img class="image" src="data:image/jpeg;base64,' + $(this).find("thumb_image").text() + '" width="170px" height="120px" onload="javascript:DrawImage(this,170,120,true);">';
			}
			html += '</div>';
			html += '<div class="hover-overlay"></div>';
			html += '<div class="overlay-icon"><div class="play bicon" data-id="' + id + '"></div></div>';
			html += '<div class="menu-anchor"></div>';
			html += '</div>';
			html += '<div class="details">';
			html += '<div class="title tooltip fade-out">' + title + '</div>';
			html += '<div class="sub-title tooltip fade-out" data-type="ar" data-id="' + id + '">' + artist + '</div>';
			html += '</div>';
			html += '</div>';							
		}						
    });	
				
	$(html).appendTo($("#fileview")).hide().fadeIn('fast');
	
	if(type=="album")
		g_ui_mode.set(6);
	else if(type=="artist")
		g_ui_mode.set(7);
			
	$(".mediaListDiv").removeClass("down");
	$("div#btnThumbView").css("display", "none");
	$("div#btnListView").css("display", "none");
	$("div#btnUpload").css("display", "none");
	$("div#btnSelect").css("display", "none");
	$("div#btnNewDir").css("display", "none");
	$("div#btnPlayImage").css("display", "none");
	$("div#boxSearch").css("display", "none");
				
	adjustLayout();
						
	closeJqmWindow(0);
						
	$(".card").click(function(){
		var loc = (g_storage.get('openurl')==undefined) ? "/" : g_storage.get('openurl');
		g_storage.set('data-id', $(this).attr("data-id"));
		doPROPFINDMEDIALIST(loc, false, null, "2", "0", "50", "", "TIMESTAMP", "DESC", $(this).attr("data-id"));
	});
					
	$(".card").mouseenter(function(e){
		$(this).find(".overlay-icon").hide().fadeIn('fast');
		$(this).find(".hover-overlay").fadeTo('slow', 0.8);
		e.preventDefault();
    	return false;
	});
					
	$(".card").mouseleave(function(e){
		$(this).find(".overlay-icon").show().fadeOut('fast');
		$(this).find(".hover-overlay").fadeTo('fast', 0);
		e.preventDefault();
    	return false;
	});
					
	$(".card .overlay-icon .play").click(function(e){
		var loc = g_storage.get('openurl');
		loc = (loc==undefined) ? "/" : loc;
			
		var media_hostName = window.location.host;					
		if(media_hostName.indexOf(":")!=-1){
			media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
		}
		media_hostName = "http://" + media_hostName + ":" + g_storage.get("http_port") + "/";
				
		client.GETMUSICPLAYLIST(loc, $(this).attr("data-id"), function(error, statusstring, content){
			if(error==200){
				var data = parseXml(content);
				var x = $(data);
				var play_list = Array();
				$(data).find("item").each(function(i) {
					var this_file_name = $(this).find("title").text();
					var sharelink = media_hostName+$(this).find("sharelink").text();
							
					var obj = [];
					obj['name'] = mydecodeURI(this_file_name);
					obj['mp3'] = sharelink;
					play_list.push(obj);
				});
				openAudioPlayerByPlayList(play_list);
			}
		});
				
		e.preventDefault();
      	return false;
	});
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
			   (g_hide_aidisk_name==1 && aa[i]==g_storage.get('usbdiskname')))
				continue;
				
			c += "/" + bb[i];
			
			tmp += " / <a id='url_path' uhref='" + c + "'>" + aa[i] + "</a>";
			
			var cur_width = String(tmp).width($("p#openurl").css('font'));
			if(cur_width>urlregion_width){
				b += " / ...";
				break;
			}
			
			if( aa[1]==g_storage.get('usbdiskname') && g_hide_aidisk_name==1 )
				b += " / <a id='url_path' uhref='/" + g_storage.get('usbdiskname') + c + "'>" + aa[i] + "</a>";
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

function adjustLayout(){		
	var page_size = getPageSize();
	var page_width = page_size[0];
	var page_height = page_size[1];
	var mainRegion_width = page_size[0];
	var mainRegion_height = page_height - 
							$('#header_region').height() - 											
							$('#bottom_region').height();
	
	//$('#main_region').css('width', mainRegion_width);							
	$('#main_region').css('height', mainRegion_height);	
	
	$('#main_region #main_right_region').css('width', mainRegion_width-parseInt($('#main_region #main_right_region').css("left")));
	$('#main_region #main_right_region').css('height', mainRegion_height-parseInt($('#main_region #button_panel').height())+7);
	
	$("#main_region #button_panel").css("left", $("#main_region #main_left_region").width()-7);
	$("#main_region #button_panel").css("width", $("#main_region #main_right_region").width());
	
	var hostview_height = $('#main_left_region').height() - $('#main_left_region #infobar').height();
	$('#hostview').css("height", hostview_height);
	
	var albutton_width=0;	
	$("#toolbar .albutton").each(function(){
		if($(this).css("display")=="block")
			albutton_width += $(this).width();
	});
	
	if($("#toolbar .alsearch").css("display")=="block")
		albutton_width += $("#toolbar .alsearch").width();
		
	$('#urlregion-url').css("width", $("#toolbar").width()-albutton_width-20);
	
	createOpenUrlUI(g_storage.get('openurl'));
	
	var h = $("#main_right_container").height() - $("#main_right_region #infobar").height();
	if($("#main_right_container #hintbar").css("display")=="block") h -= $("#main_right_container #hintbar").height();
	$("#fileview").css("height", h);
		
	$("#function_help").css("left", $("#main_left_region").width());
	
	var $modalWindow = $('div#modalWindow');
	if($modalWindow){
		var newLeft = (page_width - g_modal_window_width)/2;
		$modalWindow.css("left", newLeft);
	}	
	
	$("#header-fixed").css("width", $("#ntl").width());
	
	var header_fixed_top = $("#header_region").height() + $("#main_right_region #infobar").height() + 2;
	if($("#main_right_container #hintbar").css("display")=="block") header_fixed_top += $("#main_right_container #hintbar").height();
	$("#header-fixed").css("top", header_fixed_top);
	
	if(g_fileview_only == 1){
		$("#button_panel").css("display", "none");
		$("#main_right_region").css("left", "0px");
		$("#main_right_region").css("width", "100%");
		$("#main_right_region").css("height", "100%");
	}
	
	$("#audio_player_panel").css("display", "block");
}

function showHideAiButton(show){
	if(show){
		if(!g_bshowHideAiButton){
			$('#button_panel #btnAiMusic').fadeIn('fast');
			$('#button_panel #btnAiMusicPopupMenu').fadeIn('fast');
			$('#button_panel #btnAiMovie').fadeIn('fast');
			$('#button_panel #btnAiPhoto').fadeIn('fast');
		}
	}
	else{
		if(g_bshowHideAiButton){
			$('#button_panel #btnAiMusic').fadeOut('fast');
			$('#button_panel #btnAiMusicPopupMenu').fadeOut('fast');
			$('#button_panel #btnAiMovie').fadeOut('fast');
			$('#button_panel #btnAiPhoto').fadeOut('fast');
		}
	}
	
	g_bshowHideAiButton = show;
}

function showHideEditUIRegion(show){
	if(show){
		if(!g_bshowHideEditUIRegion){
			$('#button_panel #edit_ui_region').fadeIn('fast');
		}
	}
	else{
		if(g_bshowHideEditUIRegion){
			$('#button_panel #edit_ui_region').fadeOut('fast');
		}
	}
	
	g_bshowHideEditUIRegion = show;
}

function closeAiMode(){
	g_storage.set('aimode', -1);
	$(".mediaListDiv").removeClass("down");
	$("div#boxSearch").css("display", "none");
	$("div#boxSearch input").val("");
}

function closeUploadPanel(v){
	
	g_upload_mode = 0;
	g_reload_page=v;
	
	g_storage.set('stopLogoutTimer', "0");
	
	$("div#btnNewDir").css("display", "block");
	$("div#btnUpload").css("display", "block");
	$("div#btnSelect").css("display", "block");
	$("div#btnPlayImage").css("display", "block");
	
	if(g_list_view.get()==1){
		$("div#btnListView").css("display", "none");
		$("div#btnThumbView").css("display", "block");
	}
	else{
		$("div#btnListView").css("display", "block");
		$("div#btnThumbView").css("display", "none");
	}
	
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
	
	if(isAiModeView()>=0){
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnSelect").css("display", "block");
		$("div#btnPlayImage").css("display", (getAiMode()==1) ? "block" : "none");
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
	$("div#btnThumbView").css("display", "block");
	
	if(g_list_view.get()==1){
		$("div#btnThumbView").css("display", "block");
		$("div#btnListView").css("display", "none");
	}
	else{
		$("div#btnThumbView").css("display", "none");
		$("div#btnListView").css("display", "block");
	}
							
	$(".selectDiv").css("display", "none");
	$(".selectHintDiv").css("display", "none");
	
	$(".selectListDiv").css("display", "none");
	$(".selectListHintDiv").css("display", "none");
	
	$("#fileview #fileviewicon").each(function(){
		$(this).removeClass("select");	
	});
		
	$("#function_help").text("");
	
	adjustLayout();
}

function showHideLoadStatus(bshow){
	if(bshow)
		$("#loading").css("display", "block");
	else
		$("#loading").css("display", "none");
}

function show_hint_no_mediaserver(){
	$("#main_right_container #fileview").empty();
	
	var http_enable = g_storage.get('http_enable'); //- 0: http, 1: https, 2: both
	var misc_http_enable = g_storage.get('misc_http_enable');
	var misc_http_port = g_storage.get('misc_http_port');
	var misc_https_port = g_storage.get('misc_https_port');
	var location_host = window.location.host;
	var misc_protocol = "http";
	var misc_port = misc_http_port;
	    
	if(misc_http_enable==0){
		if( !isPrivateIP() ){
	    	$("#main_right_container #hintbar").html(m.getString("msg_no_mediaserver"));
			$("#main_right_container #hintbar").show();	    	
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
	  	
	url += "/mediaserver.asp";
	
	var html = "<a href='" + url + "' target='_blank'>" + m.getString("msg_no_mediaserver") + "</a>";
	$("#main_right_container #hintbar").html(html);
	$("#main_right_container #hintbar").show();	
}
