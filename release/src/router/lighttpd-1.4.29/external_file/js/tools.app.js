var TOOLS_PATH = '/smb/js/';
var CSS_PATH = '/smb/css/';

document.write('<link rel="stylesheet" type="text/css" href="'+CSS_PATH+'jqmWindow.css"/>');
document.write('<link rel="stylesheet" type="text/css" href="'+CSS_PATH+'barousel.css"/>');

document.write('<script type="text/javascript" src="'+TOOLS_PATH+'jquery-1.7.1.min.js"></script>');
document.write('<script type="text/javascript" src="'+TOOLS_PATH+'jquery.mobile-1.2.0.min.js"></script>');
document.write('<script type="text/javascript" src="'+TOOLS_PATH+'jquery.cookie.js"></script>');
document.write('<script type="text/javascript" src="'+TOOLS_PATH+'jquery.fileDownload.js"></script>');
document.write('<script type="text/javascript" src="'+TOOLS_PATH+'smbdav-tools.min.js"></script>');
document.write('<script type="text/javascript" src="'+TOOLS_PATH+'jplayer/jquery.jplayer.min.js"></script>');
document.write('<link rel="stylesheet" type="text/css" href="'+CSS_PATH+'jplayer.blue.monday.app.css"/>');

function initJPlayer(){
	
	var Playlist = function(instance, playlist, options) {
	  	var self = this;
	
	    this.instance = instance; // String: To associate specific HTML with this playlist
	    this.playlist = playlist; // Array of Objects: The playlist
	    this.options = options; // Object: The jPlayer constructor options for this playlist
	
	    this.current = 0;
			
		this.cssId = {
	    	jPlayer: "jquery_jplayer_",
	      	interface: "jp_interface_",
	      	playlist: "jp_playlist_",
	      	playtitle: "jp_title_"
	    };
	    
	    this.cssSelector = {};
	
	    $.each(this.cssId, function(entity, id) {
	    	self.cssSelector[entity] = "#" + id + self.instance;
	    });
	
	    if(!this.options.cssSelectorAncestor) {
	    	this.options.cssSelectorAncestor = this.cssSelector.interface;
	    }
	    
	    $(this.cssSelector.jPlayer).jPlayer(this.options);
	
	    $(this.cssSelector.interface + " .jp-previous").click(function() {
	    	self.playlistPrev();
	      	$(this).blur();
	      	return false;
	    });
	
	    $(this.cssSelector.interface + " .jp-next").click(function() {
	    	self.playlistNext();
	      	$(this).blur();
	      	return false;
		});
	};

	Playlist.prototype = {
		displayPlaylist: function() {
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
	      	//var ui_width = $(this.cssSelector.playtitle + " li").width();
			var ui_width = $("#container").width();
	      	var cur_width = String(filename).width($(this.cssSelector.playtitle + " li").css('font'));      
	        
	      	if(cur_width>ui_width){
	      		var fileext = getFileExt(filename);
	      		var len = filename.length;
	      		var new_name = "";
	      		var i=0;
	      		for(i=0; i<len; i++){
	      			new_name+=filename[i];
	      			var test_name = new_name + "." + fileext
	      			var test_width = String(test_name).width($(this.cssSelector.playtitle + " li").css('font'));
	      			if(test_width>ui_width){
	      				break;
	      			}
	      		}
	      	
	      		filename = filename.substring(0,i-4) + "...." + fileext;
	    	}
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
	    }
	};
  	
  	var jplayer_solution = "html,flash";
	var jplayer_supplied = "mp3";
	var browserVer = navigator.userAgent;	
	
	if( browserVer.indexOf("Chrome") != -1 ||
	    ( browserVer.indexOf("Safari") != -1 && ( isMacOS() || isWinOS() ) ) ){
		jplayer_solution = "html";
		jplayer_supplied = "mp3";
	}
	else if( isIE() ||
		     browserVer.indexOf("Firefox") != -1 ||
		     browserVer.indexOf("Opera") != -1 ){	  
		jplayer_solution = "flash";
		jplayer_supplied = "mp3";
	}
	
	var audioPlaylist = new Playlist("1", g_audio_playlist, {
	  	ready: function() {
	  		audioPlaylist.displayPlaylist();
	    	audioPlaylist.current = g_current_index; 
	      	audioPlaylist.playlistInit(true); // Parameter is a boolean for autoplay.
	      
	      	$("#playlist").css("visibility","visible");
	      	showHideAudioInterface(true);
	      	showHideAudioList(false);
	    },
	    ended: function() {
	    	audioPlaylist.playlistNext();
	    },
	    play: function() {
	    	$(this).jPlayer("pauseOthers");
	    },
		playing: function() {
			g_storage.set('stopLogoutTimer', "1");
		},
		pause: function() {
			g_storage.set('stopLogoutTimer', "0");
		},
		stop: function(){
			g_storage.set('stopLogoutTimer', "0");
		},
	    swfPath: "/smb/js/jplayer/",
			supplied: jplayer_supplied,
		solution: jplayer_solution,
		wmode: "window",
		errorAlerts: true,
		warningAlerts: false
	});
}