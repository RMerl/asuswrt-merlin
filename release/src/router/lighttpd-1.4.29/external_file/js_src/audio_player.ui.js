var g_audioPlayer = null;
var g_audio_playlist = [];
var g_onPlaying = false;

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
	
	get_mp3_url(audio_list, true, function(audio_playlist){				
		g_audioPlayer.showHideAudioPlayerWindow(true, function(){
			g_audioPlayer.addPlaylist(audio_playlist, default_index);
		});
	});
}

function openAudioPlayerByPlayList(playlist){
	if(playlist==null) return;
	if(playlist.length<=0) return;
	
	g_audioPlayer.addPlaylist(playlist, 0);	
	g_audioPlayer.showHideAudioPlayerWindow(true);
}

function getAudioPlayerLayout(){	
	var layout_html = "";
	layout_html += "<div id='audioPlayerWindow' class='unselectable'>";
	layout_html += '<div id="audio-player-box">';
	layout_html += '<script type="text/javascript" src="/smb/js/jplayer/jquery.jplayer.min.js"></script>';
	layout_html += '<link rel="stylesheet" href="/smb/css/jplayer.blue.monday.css" type="text/css">';
	layout_html += '<div id="jquery_jplayer_1" class="jp-jplayer"></div>';
	layout_html += '<div id="jp_container_1" class="jp-audio">';
	layout_html += '<div class="jp-type-playlist">';
	layout_html += '<div id="jp_title_1" class="jp-title">';
	layout_html += '<ul>';
	layout_html += '<li>Loading...</li>';
	layout_html += '</ul>';
	layout_html += '</div>';
	layout_html += '<div id="jp_interface_1" class="jp-gui jp-interface">'
	layout_html += '<ul class="jp-controls">';
	layout_html += '<li><a href="javascript:;" class="jp-previous" tabindex="1">previous</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-play" tabindex="1" style="display: block; ">play</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-pause" tabindex="1" style="display: block; ">pause</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-next" tabindex="1">next</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-stop" tabindex="1">stop</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-mute" tabindex="1" title="mute">mute</a></li>';
	layout_html += '<li><a href="javascript:;" class="jp-volume-max" tabindex="1" title="max volume">max volume</a></li>';
	layout_html += '</ul>';
	layout_html += '<div class="jp-progress">';
	layout_html += '<div class="jp-seek-bar" style="width: 100%; ">';
	layout_html += '<div class="jp-play-bar" style="width: 46.700506341691536%; "></div>';
	layout_html += '</div>'
	layout_html += '</div>';
	layout_html += '<div class="jp-volume-bar">'
	layout_html += '<div class="jp-volume-bar-value" style="width: 31.979694962501526%; "></div>';
	layout_html += '</div>';
	layout_html += '<div class="jp-duration-bar">';
	layout_html += '<div class="jp-duration">00:00</div>'
	layout_html += '<div class="jp-split">/</div>';
	layout_html += '<div class="jp-current-time">00:00</div>';
	layout_html += '</div>';
	layout_html += '</div>';					
	layout_html += '<div id="jp_playlist_1" class="jp-playlist">';
	layout_html += '<ul></ul>';
    layout_html += '</div>';
	layout_html += '</div>';
	layout_html += '</div>';
	layout_html += '<div class="jp-no-solution" style="display: none; ">';
	layout_html += '<span>Update Required</span>';
	layout_html += 'To play the media you will need to either update your browser to a recent version or update your <a href="http://get.adobe.com/flashplayer/" target="_blank">Flash plugin</a>.';
	layout_html += '</div>';
	layout_html += '<a id="audio-player-box-playlist" class="sicon" href="#" rel="playlist">Playlist</a>';
	layout_html += '<a id="audio-player-box-hide" class="sicon" href="#" rel="close">Hide</a>';
	layout_html += '<a id="audio-player-box-close" class="sicon" href="#" rel="close">Close</a>';
	layout_html += '</div>';
	layout_html += "</div>";
	layout_html += "<div id='audioPlayerStatusWindow' class='bicon' style='display:none'>";
	layout_html += "<div id='close-button'></div>";
	layout_html += "<div id='status-bar'>";
	layout_html += "<span id='status'></span>";
	layout_html += "</div>";
	layout_html += "</div>";
	
	return layout_html;
}

function get_mp3_url(audio_list, generate_sharelink, complete_handler){
	if(audio_list=="")
		return;
	
	var tmp_audio_playlist = g_audio_playlist;
	var this_audio_list = audio_list.split(",");
	var current_query_index = 0;
	var on_query = false;
	g_audio_playlist = [];
	
	for(var i=0; i<tmp_audio_playlist.length; i++){
		for(var j=0; j<this_audio_list.length; j++){
			if(tmp_audio_playlist[i]["source"]==this_audio_list[j]){
				g_audio_playlist.push(tmp_audio_playlist[i]);
				this_audio_list.splice(j, 1);
				j=-1;
			}
		}
	}
	
	if(generate_sharelink){
		
		var media_hostName = window.location.host;
		//if(media_hostName.indexOf(":")!=-1)
		//	media_hostName = media_hostName.substring(0, media_hostName.indexOf(":"));
		//media_hostName = "http://" + media_hostName + ":" + g_storage.get("http_port") + "/";
		
		media_hostName = window.location.protocol + "//" + media_hostName + "/";
		
		var timer = setInterval(function(){
			
			if(on_query==false){
				
				if(current_query_index<0||current_query_index>this_audio_list.length-1){
					clearInterval(timer);
					complete_handler(g_audio_playlist);
					return;
				}
			
				var this_audio = this_audio_list[current_query_index];
				var this_file_name = this_audio.substring( this_audio.lastIndexOf("/")+1, this_audio.length );
				var this_url = this_audio.substring(0, this_audio.lastIndexOf('/'));
				
				on_query = true;			
				g_webdav_client.GSL(this_url, this_url, this_file_name, 0, 0, function(error, content, statusstring){				
					if(error==200){
						var data = parseXml(statusstring);
						var share_link = $(data).find('sharelink').text();
						share_link = media_hostName + share_link;
						
						on_query = false;
						
						var obj = [];
						obj['source'] = this_audio;
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
			obj['source'] = this_audio;
			obj['name'] = mydecodeURI(this_file_name);
			obj['mp3'] = this_audio;
			g_audio_playlist.push(obj);
		}
		
		complete_handler(g_audio_playlist);
	}
	
	//tmp_audio_playlist = null;
	//this_audio_list = null;
}

function initAudioPlayer(){
	
	var AudioPlayer = function(instance, options) {
  		var self = this;
		this.clsoeAudioPlayerWindowTimer = 0;
		this.statusMarqueeTimer = 0;
    	this.instance = instance; // String: To associate specific HTML with this playlist
    	this.playlist = null; // Array of Objects: The playlist
    	this.options = options; // Object: The jPlayer constructor options for this playlist
		this.playTitle = "";
    	this.current = 0;
		this.showAudioList = false;
		
    	this.cssId = {
    		"jPlayer": "jquery_jplayer_",
      		"interface": "jp_interface_",
      		"playlist": "jp_playlist_",
      		"playtitle": "jp_title_"
    	};
		
		this.cssSelector = {};

    	$.each(this.cssId, function(entity, id) {
    		self.cssSelector[entity] = "#" + id + self.instance;
    	});
		
		if(!this.options.cssSelectorAncestor) {
    		this['options']['cssSelectorAncestor'] = this['cssSelector']['interface'];
    	}
		
		$(this.cssSelector.jPlayer).jPlayer(this.options);
		
		var s = this['cssSelector']['interface'] + " .jp-previous";
		$(s).click(function() {
    		self.playlistPrev();
      		$(this).blur();
      		return false;
    	});
		
		s = this['cssSelector']['interface'] + " .jp-next";
		$(s).click(function() {
    		self.playlistNext();
      		$(this).blur();
      		return false;
    	}); 
		
		$("#audioPlayerWindow").mousemove(function(){
			self.startStopRunAudioPlayerWindowTimer(false);	
		});
		
		$("#audioPlayerWindow").mouseleave(function(){
			self.startStopRunAudioPlayerWindowTimer(true);	
		});
		
		$("#audio-player-box-hide").click(function(){
			self.showHideAudioPlayerWindow(false);
			return false;
		});
		
		$("#audio-player-box-close").click(function(){
			
			if(g_onPlaying && !confirm(m.getString('msg_close_audio_window_warning'))){
				return false;
			}
			
			self.closeAudioPlayWindow();
			
			return false;
		});
		
		$("#audio-player-box-playlist").click(function(){
			self.showHideAudioList(!self.showAudioList);
		});
		
		$("#audioPlayerStatusWindow").click(function(){
			self.showHideAudioPlayerWindow(true);
		});
		
		$("#audioPlayerStatusWindow #close-button").click(function(){
			if(g_onPlaying && !confirm(m.getString('msg_close_audio_window_warning'))){
				return false;
			}
			
			self.closeAudioPlayWindow();
			
			return false;
		});
  	};
	
	AudioPlayer.prototype = {
		getPlayTitle: function(){
			return this.playTitle;
		},
		startStopRunAudioPlayerStatus: function(start){
			if(start){
				$('#audioPlayerStatusWindow').fadeIn('slow', function() {
        			this.statusMarqueeTimer = setInterval(function(){
					
						var len = String($("#audioPlayerStatusWindow #status").text()).width($("#audioPlayerStatusWindow #status").css("font"));
							
						$('#audioPlayerStatusWindow #status').animate({
							left: -len
						}, 5000, function() {
							// Animation complete.
							$('#audioPlayerStatusWindow #status').css("left", 80);
						});
					}, 500);
      			});
			}
			else{
				clearInterval(this.statusMarqueeTimer);
				$("#audioPlayerStatusWindow").css("display", "none");
			}
		},
		startStopRunAudioPlayerWindowTimer: function(start){
			var self = this;
			if(start){
				//- Close Audio Player Window After 3 sec.
				self.clsoeAudioPlayerWindowTimer = setTimeout( function(){
					self.showHideAudioPlayerWindow(false);
				}, 10000 );				
			}
			else{
				clearInterval(self.clsoeAudioPlayerWindowTimer);
			}
		},
		showHideAudioPlayerWindow: function(bshow, completeHandler){
			var self = this;
			if(bshow){
				//- Show Audio Player Window
				self.startStopRunAudioPlayerWindowTimer(false);
				
				self.startStopRunAudioPlayerStatus(false);
				
				self.showHideAudioList(false);
				
				if($('#audioPlayerWindow').css("display")=="none"){
					$('#audioPlayerWindow').fadeIn(500, function() {					
						self.startStopRunAudioPlayerWindowTimer(true);
						
						if(completeHandler)
							completeHandler();
					});
				}
				else{
					self.startStopRunAudioPlayerWindowTimer(true);
						
					if(completeHandler)
						completeHandler();
				}
			}
			else{
				//- Hide Audio Player Window
				if($('#audioPlayerWindow').css("display")=="block"){
					$('#audioPlayerWindow').fadeOut(500, function() {
						//- Show Audio Player Status Window.
						self.startStopRunAudioPlayerStatus(true);
						self.showHideAudioList(false);
						
						if(completeHandler)
							completeHandler();
					});
				}
				else{
					//- Show Audio Player Status Window.
					self.startStopRunAudioPlayerStatus(true);
					self.showHideAudioList(false);
						
					if(completeHandler)
						completeHandler();
				}
			}
		},
		showHideAudioInterface: function(bshow){
			if(bshow){
				$("#jp_interface_1").css("display","block");
			}
			else{
				$("#jp_interface_1").css("display","none");
			}
		},
		showHideAudioList: function(bshow){
			if(bshow){
				$(".jp-playlist").css("display", "block");
			}
			else{		
				$(".jp-playlist").css("display", "none");
			}
			
			this.showAudioList = bshow;
		},
		setPlaylist: function(playlist) {
			this.playlist = playlist;
			
			this.displayPlaylist();
    		this.current = 0;//g_current_index; 
      		this.playlistInit(true); // Parameter is a boolean for autoplay.
      
      		$("#playlist").css("visibility","visible");
      		this.showHideAudioInterface(true);
      		this.showHideAudioList(false);
		},
		addPlaylist: function(playlist, default_play_index) {
			this.playlist = playlist;
			
			this.displayPlaylist();
    		this.current = default_play_index; 
      		this.playlistInit(true); // Parameter is a boolean for autoplay.
      		
      		$("#playlist").css("visibility","visible");
      		this.showHideAudioInterface(true);
      		this.showHideAudioList(false);
		},
  		displayPlaylist: function() {
			if(this.playlist==null||this.playlist==undefined)
				return;
				
    		var self = this;
      		$(this.cssSelector.playlist + " ul").empty();
      
      		for (i=0; i < this.playlist.length; i++) {
      			var filename = this.playlist[i].name;
      			var listItem = (i === this.playlist.length-1) ? "<li class='jp-playlist-last'>" : "<li>";
        		listItem += "<a href='#' id='" + this.cssId.playlist + this.instance + "_item_" + i +"' tabindex='1'>"+ filename +"</a>";

        		// Associate playlist items with their media
        		$(this.cssSelector.playlist + " ul").append(listItem);
       
        		$(this.cssSelector.playlist + "_item_" + i).data("index", i).click(function() {
        			var index = $(this).data("index");        	
          			if(self.current !== index) {
          				self.playlistChange(index);
          			} 
          			else {
          				$(self.cssSelector.jPlayer).jPlayer("play");
          			}
          			$(this).blur();
          			return false;
        		});
      		}
    	},
    	playlistInit: function(autoplay) {
			if(autoplay) {
      			this.playlistChange(this.current);
      		} else {
        		this.playlistConfig(this.current);
      		}
    	},
    	playlistConfig: function(index) {
    		$(this.cssSelector.playlist + "_item_" + this.current).removeClass("jp-playlist-current").parent().removeClass("jp-playlist-current");
      		$(this.cssSelector.playlist + "_item_" + index).addClass("jp-playlist-current").parent().addClass("jp-playlist-current");
      		this.current = parseInt(index);
      		
      		var filename = this.playlist[this.current]['name'];
			var full_filename = filename;			
		 	this.playTitle = filename;
    		//alert("play "+this.playlist[this.current].mp3);
			
    		$(this.cssSelector.playtitle + " li").attr("title",full_filename);
      		$(this.cssSelector.playtitle + " li").text(filename);
      		$(this.cssSelector.jPlayer).jPlayer("setMedia", this.playlist[this.current]);
    	},
    	playlistChange: function(index) {
      		this.playlistConfig(index);
      		$(this.cssSelector.jPlayer).jPlayer("play");
    	},
    	playlistNext: function() {
    		var index = (this.current + 1 < this.playlist.length) ? this.current + 1 : 0;
      		this.playlistChange(index);
    	},
    	playlistPrev: function() {
    		var index = (this.current - 1 >= 0) ? this.current - 1 : this.playlist.length - 1;
      		this.playlistChange(index);
    	},
		closeAudioPlayWindow:function() {			
			$(this.cssSelector.jPlayer).jPlayer("stop");
			
			this.startStopRunAudioPlayerStatus(false);
			this.startStopRunAudioPlayerWindowTimer(false);
			
			$('#audioPlayerStatusWindow').hide();
			$('#audioPlayerWindow').hide();
		}
  	};
	////////////////////////////////////////////////////////////////////////////////////////////////
	
	var jplayer_solution = "html,flash";
	var jplayer_supplied = "mp3";
	var browserVer = navigator.userAgent;	
	
	if( browserVer.indexOf("Chrome") != -1 ||
	    browserVer.indexOf("Firefox") != -1 ||
	    ( browserVer.indexOf("Safari") != -1 && ( isMacOS() || isWinOS() ) ) ){
		jplayer_solution = "html,flash";
		jplayer_supplied = "mp3";
	}
	else if( isIE() ||
		     browserVer.indexOf("Opera") != -1 ){	  
		jplayer_solution = "flash";
		jplayer_supplied = "mp3";
	}
	
	g_audioPlayer = new AudioPlayer("1", {
  		ready: function() {
			
    	},
    	ended: function() {
			g_audioPlayer.playlistNext();
			g_storage.set('stopLogoutTimer', "0");
    	},
    	play: function() {
    		$(this).jPlayer("pauseOthers");
			g_storage.set('stopLogoutTimer', "1");
    	},
		playing: function() {
			$("#audioPlayerStatusWindow #status").text("playing: " + g_audioPlayer.getPlayTitle());
			g_onPlaying = true;
			g_storage.set('stopLogoutTimer', "1");
		},
		pause: function() {
			$("#audioPlayerStatusWindow #status").text("pause: " + g_audioPlayer.getPlayTitle());
			g_onPlaying = false;
			g_storage.set('stopLogoutTimer', "0");
		},
		stop: function(){
			g_onPlaying = false;
			g_storage.set('stopLogoutTimer', "0");
		},
    	swfPath: "/smb/js/jplayer/",
		supplied: jplayer_supplied,
		solution: jplayer_solution,		
		wmode: "window",
		errorAlerts: false,
		warningAlerts: false,
		error: function(event){
			var type = event.jPlayer.error.type;
			if(type=="e_no_solution"){
				$(".jp-no-solution").html(m.getString("title_install_flash"));
				$(".jp-no-solution").css("display", "block");				
			}
			else if(type=="e_url"){
				if(g_audio_playlist.length>1)
					g_audioPlayer.playlistNext();
			}
		}
	});
	////////////////////////////////////////////////////////////////////////////////////////////////
	/*
	$("#audio_player_panel").draggable({
		snap: 'true',
		cursor: 'move',
		opacity: 0.35,
		containment: "#main_region"
	});
	*/
}
