var m = new lang();
var g_storage = new myStorage();
var g_this_video = "";
var g_this_video_name = "";
var g_this_video_hash = "";
var g_play_id = -1;
var g_subtitle_timer = 0;
var prevState = 0;
var liveFeedText = ["Playing", "((Playing))", "(( Playing ))", "((  Playing  ))"];
var liveFeedRoll = 0;
var g_isIE = false;
var g_video_status = "";
/*
UTF8 = {
    encode: function(s){
        for(var c, i = -1, l = (s = s.split("")).length, o = String.fromCharCode; ++i < l;
            s[i] = (c = s[i].charCodeAt(0)) >= 127 ? o(0xc0 | (c >>> 6)) + o(0x80 | (c & 0x3f)) : s[i]
        );
        return s.join("");
    },
    decode: function(s){
        for(var a, b, i = -1, l = (s = s.split("")).length, o = String.fromCharCode, c = "charCodeAt"; ++i < l;
            ((a = s[i][c](0)) & 0x80) &&
            (s[i] = (a & 0xfc) == 0xc0 && ((b = s[i + 1][c](0)) & 0xc0) == 0x80 ?
            o(((a & 0x03) << 6) + (b & 0x3f)) : o(128), s[++i] = "")
        );
        return s.join("");
    }
};

function getUrlVars(){
	var vars = [], hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for(var i = 0; i < hashes.length; i++)
    {
        hash = hashes[i].split('=');
        vars.push(hash[0]);
        vars[hash[0]] = hash[1];
    }
    return vars;
}
*/
function detectVLCInstalled(){
	var val = navigator.userAgent.toLowerCase();
		
	var vlcInstalled= false;
	
	if(g_isIE){
		var vlcObj = null;
		try {
			vlcObj = new ActiveXObject("VideoLAN.Vlcplugin");
		} 
		catch (e) {
			var msg= "An exception occurred while trying to create the object VideoLAN.VLCPlugin.1:\n";
		  	for (p in e)
		  		msg+= "e."+p+"= " + e[p] + "\n";
		  	//window.alert (msg);
		}
		if( null != vlcObj )
   			vlcInstalled = true;
	}
	else{
		if( navigator.plugins && navigator.plugins.length ) {
	  		for( var i=0; i < navigator.plugins.length; i++ ) {
	    		var plugin = navigator.plugins[i];
	   			if( plugin.name.indexOf("VideoLAN") > -1
	    			|| plugin.name.indexOf("VLC") > -1) {
	      			vlcInstalled = true;
	     		}
	   		}
	 	}
	}
	
	return vlcInstalled;
}

function getInternetExplorerVersion(){
   	var rv = -1; // Return value assumes failure.
	if (navigator.appName == 'Microsoft Internet Explorer'){
	var ua = navigator.userAgent;
      	var re  = new RegExp("MSIE ([0-9]{1,}[\.0-9]{0,})");
      	if (re.exec(ua) != null)
        	rv = parseFloat( RegExp.$1 );
   }
   return rv;
}

function closeWindow(){
	parent.closeJqmWindow(0);
}

$(document).keydown(function(e) {
	if (e.keyCode == 27) return false;
});

function init(){
	setTimeout(function(){
		createVLC();
	},500);
	/*
	document.write("<body width='100%' height='100%' style='margin:0px;padding:0px;background-color:#000;text-align:center'>");
	document.write("<span style='color:#fff;font-size:20px'>Loading....</span>");
	document.write("</body>");
		
	var this_query_title = getUrlVars()["t"];	
	if( typeof this_query_title != undefined && this_query_title != "" ){
		
		$.getJSON( 'http://imdbapi.org/?title=' + this_query_title + '&type=json', 
	    	//options,
  	        function(data) {

				if(data[0]==undefined){					
					setTimeout(function(){
						createVLC();
					},500);
					return;
				}
				
				var poster_image = data[0]['poster'];
				var imdb_url = data[0]['imdb_url'];
				var overview_html = "";
				
				//overview_html += "<body>";
				overview_html += "<div class='movie_overview'>";
				overview_html += "<table cellspacing='0' cellpadding='0' border='0'>";
				overview_html += "<tbody>";
				overview_html += "<tr><td rowspan='2' class='img_primary' style='padding: 0 14px 0 0;'>";
				overview_html += "<div width='100%'><img src='" + poster_image + "'><div>";
				overview_html += "</td>";
				overview_html += "<td class='overview'>";
				
				overview_html += "<form class='x-form'>";
				
				overview_html += '<div class="x-panel x-panel-noborder">';
				overview_html += '<div class="x-panel-bwrap">';
				overview_html += '<div class="x-panel-body x-panel-body-noheader x-panel-body-noborder x-table-layout-ct"><table class="x-table-layout" cellspacing="0">';
				overview_html += '<tbody><tr><td class="x-table-layout-cell syno-vs-meta-title">';
				overview_html += '<div class="x-form-display-field x-form-display-title-field">Batman Begins - Evil Fears The Knight</div>';
				overview_html += '</td></tr></tbody></table></div></div></div>';
				
				overview_html += "<div class='x-form-item'>";
				overview_html += "<label class='x-form-item-label'>類型</label>";
				overview_html += "<div>";
				overview_html += '<div class="x-form-display-field">';
				overview_html += '<span class="syno-vs-field-48" id="ext-gen1139">Action film</span>, <span class="syno-vs-field-49" id="ext-gen1140">Drama</span>, <span class="syno-vs-field-50" id="ext-gen1141">Crime Fiction</span>, <span class="syno-vs-field-51" id="ext-gen1142">Adventure film</span>';
				overview_html += '</div>';
				overview_html += "</div>";
				overview_html += "</div>";
				
				overview_html += "<div class='x-form-item'>";
				overview_html += "<label class='x-form-item-label'>年份</label>";
				overview_html += "<div>";
				overview_html += '<div class="x-form-display-field">';
				overview_html +=  data[0]['year'];
				overview_html += "</div>";
				overview_html += "</div>";
				overview_html += "</div>";
				
				overview_html += "<div class='x-form-item'>";
				overview_html += "<label class='x-form-item-label'>參考資料</label>";
				overview_html += "<div>";
				overview_html += '<div class="x-form-display-field">';
				overview_html +=  "<a class='allowDefCtxMenu' href='" + imdb_url + "' target='_blank'>IMDb</a>";
				overview_html += "</div>";
				overview_html += "</div>";
				overview_html += "</div>";
				
				overview_html += "<div class='x-form-item' style='padding-top:15px;'>";
				overview_html += "<div>";
				overview_html += '<div class="x-form-display-field">';
				overview_html += data[0]['plot_simple'];
				overview_html += "</div>";
				overview_html += "</div>";
				overview_html += "</div>";
				
				overview_html += "</form>";
				
				//overview_html += "<table cellspacing='0' cellpadding='0' border='0'>";
				//overview_html += "<tbody>";
				//overview_html += "<tr><td class='a1'>類型</td><td class='a2'></td></tr>";
				//overview_html += "<tr><td class='a1'>年份</td><td class='a2'>" + data[0]['year'] + "</td></tr>";
				//overview_html += "<tr><td class='a1'>參考資料</td><td class='a2'><a href='" + imdb_url + "' target='_blank'>IMDb</a></td></tr>";
				//overview_html += "<tr><td class='a1'>劇情</td><td class='a2'>" + data[0]['plot_simple'] + "</td></tr>";
				//overview_html += "</tbody>";
				//overview_html += "</table>";
				
				overview_html += "</td>";
				overview_html += "</tr>";
				overview_html += "</tbody>";
				overview_html += "</table>";
				
				overview_html += "<button id='btnClose' style='position: relative;float:right;right:5px;' onclick='parent.closeJqmWindow(0);'>close</button>";
				overview_html += "<button id='btnPlay' style='position: relative;float:right;right:25px;' onclick='createVLC();'>play</button>";
				overview_html += "</div>";
				//overview_html += "</body>";
				
				$("body").empty();
				$(overview_html).appendTo($("body"));
			}
		);		
	}
	else{
		setTimeout(function(){
			createVLC();
		},500);
	}
	*/
}

function getVLC(name){
	return document.getElementById(name);
}

function monitor(){
    var vlc = getVLC("vlc");
    var newState = 0;

    if( vlc ){
    	newState = vlc.input.state;
    }
		
    if( prevState != newState ){
        if( newState == 0 ){
            // current media has stopped
            onEnd();
        }
        else if( newState == 1 ){
            // current media is openning/connecting
            onOpen();
        }
        else if( newState == 2 ){
            // current media is buffering data
            onBuffer();
        }
        else if( newState == 3 ){
            // current media is now playing
            onPlaying();
        }
        else if( newState == 4 ){
            // current media is now paused
            onPause();
        }
        else if( newState == 5 ){
            // current media has stopped
            onStop();
        }
        else if( newState == 6 ){
            // current media has ended
            onEnd();
        }
        else if( newState == 7 ){
            // current media encountered error
            onError();
        }
        prevState = newState;
    }
    else if( newState == 3 ){
        // current media is playing
        onPlaying();
    }
}

function formatTime(timeVal){
    if( typeof timeVal != 'number' )
        return "-:--:--";

    var timeHour = Math.round(timeVal / 1000);
    var timeSec = timeHour % 60;
    if( timeSec < 10 )
        timeSec = '0'+timeSec;
    timeHour = (timeHour - timeSec)/60;
    var timeMin = timeHour % 60;
    if( timeMin < 10 )
        timeMin = '0'+timeMin;
    timeHour = (timeHour - timeMin)/60;
    if( timeHour > 0 )
        return timeHour+":"+timeMin+":"+timeSec;
    else
        return timeMin+":"+timeSec;
}

function doPlay(){
	var vlc = getVLC("vlc");

  	if( vlc && g_play_id >= 0){
  		
		//vlc.playlist.playItem(g_play_id);
		if(g_video_status == "pause"){
			//vlc.input.time = parseInt(g_storage.getl(g_this_video_hash));
			vlc.playlist.play();
		}
		else{
			vlc.playlist.playItem(g_play_id);
		}
		
		$(".play").css("display", "none");
		$(".pause").css("display", "block");
	}
}

function doPause(){
	var vlc = getVLC("vlc");

  	if( vlc){
  		if(g_isIE)
			vlc.playlist.stop();
		else
			vlc.playlist.pause();
		
		$(".play").css("display", "block");
		$(".pause").css("display", "none");
	}
}

function doStop(){
	var vlc = getVLC("vlc");

  	if( vlc ){
  		vlc.playlist.stop();
		
		$(".play").css("display", "block");
		$(".pause").css("display", "none");
		
  		onStop();
	}
}

function onOpen(){
	g_video_status = "open";
	document.getElementById("info").innerHTML = "Opening...";
}

function onEnd(){
	g_video_status = "stop";
	document.getElementById("info").innerHTML = "End...";  
}

function onBuffer(){
	document.getElementById("info").innerHTML = "Buffering...";
}

function onPause(){
	g_video_status = "pause";
	document.getElementById("info").innerHTML = "Paused...";
}

function onStop(){
	g_video_status = "stop";
	document.getElementById("info").innerHTML = "-:--:--/-:--:--";
}

function onError(){
	g_video_status = "error";
	document.getElementById("info").innerHTML = "Error...";
}

function record_playtime(){
	var vlc = getVLC("vlc");
	if(vlc){
		g_storage.setl(g_this_video_hash, vlc.input.time);
	}
}

function onPlaying(){
	var vlc = getVLC("vlc");
  	var info = document.getElementById("info");
       
  	if( vlc ){
  		var mediaLen = vlc.input.length;
    	if( mediaLen > 0 ){
			$(".currTime").text(formatTime(vlc.input.time));
			$(".totalTime").text(formatTime(mediaLen));
    		info.innerHTML = formatTime(vlc.input.time)+"/"+formatTime(mediaLen);
			
			var pos = (vlc.input.time/mediaLen)*$(".playbar .slider-tracker").width();
			$(".playbar .slider-thumb").css("left", pos);
			$(".playbar .slider-progress").css("width", pos);
    	}
    	else{
    		// non-seekable "live" media
      		liveFeedRoll = liveFeedRoll & 3;
      		info.innerHTML = liveFeedText[liveFeedRoll++];
    	}
  	}
	
	record_playtime();
}

function doMarqueeOption(option, value)
{
    var vlc = getVLC("vlc");
    val = parseInt(value);
    
    if( vlc )
    {
        if (option == 1)
            vlc.video.marquee.color = val;
        if (option == 2)
            vlc.video.marquee.opacity = val;
        if (option == 3)
            vlc.video.marquee.position = value;
        if (option == 4)
            vlc.video.marquee.refresh = val;
        if (option == 5)
            vlc.video.marquee.size = val;
        if (option == 6)
            vlc.video.marquee.text = value;
        if (option == 7)
            vlc.video.marquee.timeout = val;
        if (option == 8)
            vlc.video.marquee.x = val;
        if (option == 9)
            vlc.video.marquee.y = val;
    }
}

function doAspectRatio(){
    var vlc = getVLC("vlc");
    
    if( vlc )
        vlc.video.aspectRatio = $("#select_aspect").val();
}

function play_subtitle(){
	$('.srt').each(function() {
		var vlc = getVLC("vlc");
		var toSeconds = function(t) {
    		var s = 0.0
    		if(t) {
      			var p = t.split(':');
      			for(i=0;i<p.length;i++){
					var a = String((p[i]==null)?"":p[i]);
					s = s * 60 + parseFloat(a.replace(',', '.'))
				}
    		}
    		return s;
  		};
		
  		var strip = function(s) {
			var a = String((s==null)?"":s);
    		return a.replace(/^\s+|\s+$/g,"");
  		};
  		
		var SortArrayByKeys = function(inputarray) {
  			var arraykeys=[];
  			for(var k in inputarray) {arraykeys.push(k);}
		  	arraykeys.sort();
		
		  	var outputarray=[];
		  	for(var i=0; i<arraykeys.length; i++) {
				outputarray[arraykeys[i]]=inputarray[arraykeys[i]];
		  	}
		  	return outputarray;
		};
		
		var playSubtitles = function(subtitleElement) {
    		//var srt = UTF8.encode(subtitleElement.text());
			var srt = subtitleElement.text();
    		subtitleElement.text('');
    		srt = srt.replace(/\r\n|\r|\n/g, '\n')
    
    		var subtitles = {};
			srt = String(strip(srt));
			
			var index = 1;
			var srt_ = srt.split('\n\n');
    		for(s in srt_) {
				st = String(srt_[s]).split('\n');
				
				if(st.length >=2) {
					n = st[0];
				  	i = strip(st[1].split(' --> ')[0]);
				  	o = strip(st[1].split(' --> ')[1]);
				  	t = st[2];
				  	if(st.length > 2) {
						for(j=3; j<st.length;j++)
					  		t += '\n'+st[j];
				  	}
				  	
					is = toSeconds(i);
				  	os = toSeconds(o);
				  	
					subtitles[index] = {is:is, i:i, o: o, t: t};
					
					index++;
				}
    		}
			
			subtitles = SortArrayByKeys(subtitles);
									
			var vlc = getVLC("vlc");       
  			if( vlc ){
				var currentSubtitle = -1;
				g_subtitle_timer = setInterval(function() {
					var currentTime = parseFloat(vlc.input.time/1000);
					
					var subtitle = -1;
					for(var key in subtitles){						
						if(subtitles[key].is > currentTime){
							break
						}
						
						subtitle = key;
					}
					
					if(subtitle > 0) {
						if(subtitle != currentSubtitle) {
							//alert(subtitles[subtitle].t);
							subtitleElement.text(subtitles[subtitle].t);
							currentSubtitle=subtitle;
						} 
						else if(subtitles[subtitle].o < currentTime) {
							subtitleElement.text('');
						}
					}
					else
						subtitleElement.text('');
				}, 100);
			}
  		}
  		
    	var subtitleElement = $(this);
		clearInterval(g_subtitle_timer);
		
    	if( vlc ){
			var srtUrl = subtitleElement.attr('data-srt');
			
			if(srtUrl) {
				
				/*
				var that = $(this);
				$.get( srtUrl, 
					   null, 
					   function(data){
					alert(encodeURI(data));
					that.text(UTF8.encode(data) );
					playSubtitles(subtitleElement);
				});
				*/
											
				$(this).load( srtUrl, function(responseText, textStatus, req) {
					playSubtitles(subtitleElement)
				});
				
			} 
			else {
				subtitleElement.text('');
			}
		}
  	});
}

function createVLC() {
	var vars = getUrlVars();
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
	m.setLanguage(lan);
	
	var this_showbutton = vars["showbutton"];
	var this_video = vars["v"];
	var this_url = vars["u"];
	
	g_this_video = this_video;
	g_this_video_name = this_video.substring(this_video.lastIndexOf("/")+1, this_video.lastIndexOf("."));
	g_this_video_hash = md5(g_this_video.substr(g_this_video.lastIndexOf("/")+1, g_this_video.length));
	
	var val = navigator.userAgent.toLowerCase();
	var osVer = navigator.appVersion.toLowerCase();
	
	g_isIE = isIE();
	
	if( osVer.indexOf("mac") != -1 ){
		document.write("<video width='320' height='240' controls='controls'>");
  		document.write("<source src='");
  		document.write(this_video);
  		document.write("' type='video/x-pn-realvideo'/>");
  		document.write("<source src='");
  		document.write(this_video);
  		document.write("' type='video/ogg'/>");  	
		document.write("Your browser does not support the video tag.");
		document.write("</video>");
		return;
	}
	
	if(!detectVLCInstalled()){
		
		document.write("<body width='100%' height='100%' style='margin:0px;padding:0px'>");		
		
		if(this_showbutton==1)	
			document.write("<div id='errorpage' style='overflow:none;background-image:url(vlc_bg_img.jpg);background-repeat:no-repeat;width:100%; height:94%'>");
		else
			document.write("<div id='errorpage' style='background-image:url(vlc_bg_img.jpg);background-repeat:no-repeat;width:100%; height:100%'>");
		document.write("<p style='position:relative;left:54px;top:60px;width:500px;font-size:20px;color:#ffffff'>" + m.getString('msg_installvlc') + "</p>");
		document.write("<p style='position:relative;left:54px;top:80px;width:550px;font-size:16px;color:#ffffff;text-align:left'>" + m.getString('msg_installvlc2') + "</p>");
		document.write("<p style='position:relative;left:216px;top:100px;width:350px;font-size:14px;color:#ffffff;text-align:left'>" + m.getString('msg_installvlc3') + "</p>");
		document.write("<a href='http://www.videolan.org/vlc/' target='_blank'><div style='width:123px;height:30px;background-image:url(downloadvlc.png);position:relative;left:456px;top:100px;cursor:pointer'></div></a>");
		document.write("</div></body>");
		
		if(this_showbutton==1){
			document.write("<div width='100%'><button id='btnClose' style='position: relative;float:right;right:5px;' onclick='parent.closeJqmWindow(0);'>close</button><div>");			
		}
		
		document.write("</body>");
		
		window.resizeTo( 640, 460 );
		return;
	}
	
	var vlc_width = "655px";
	var vlc_height = "470px";
	
	if(this_showbutton==1){
		vlc_width = "655px";
		vlc_height = "450px";
	}
	
	var vlc_html = "";
	vlc_html += "<div>";
	
	if(g_isIE){
		vlc_html += "<OBJECT classid='clsid:9BE31822-FDAD-461B-AD51-BE1D1C159921'";
		vlc_html += " codebase='http://download.videolan.org/pub/videolan/vlc/last/win32/axvlc.cab'";
		vlc_html += " id='vlc' name='vlc' width='" + vlc_width + "' height='" + vlc_height + "' events='True'>";
		vlc_html += "<param name='MRL' value='' />";
		vlc_html += "<param name='ShowDisplay' value='False'/>";
		vlc_html += "<param name='AutoLoop' value='False'/>";
		vlc_html += "<param name='AutoPlay' value='False'/>";
		vlc_html += "<param name='ToolBar' value='False'/>";
		vlc_html += "<param name='Volume' value='50' />";
		vlc_html += "<param name='StartTime' value='0' />";
		vlc_html += "</OBJECT>";
	}
	else{
		vlc_html += "<embed type='application/x-vlc-plugin'";
		vlc_html += " pluginspage='http://www.videolan.org'";
		vlc_html += " width='" + vlc_width + "' height='" + vlc_height + "' id='vlc' name='vlc' version='VideoLAN.VLCPlugin.2' text='Waiting for video' Volume='50' toolbar='False' AutoPlay='False'/>";
	}
	
	vlc_html += "</div>";
	
	$(vlc_html).appendTo($(".videoplayer"));
	
	/*	
	//document.write("<div class='toolbar' style='padding: 6px 20px 0 20px;height: 50px;background: url(images/button_panel_bg.png) repeat-x;border: 0;color: #C8CDD2;'>");
	document.write("<div class='toolbar'>");
	document.write("<table cellspacing='0' class='toolbar-ct'>");
	document.write("<tbody>");
	document.write("<tr>");
	document.write("<td class='toolbar-left' align='left'>");
	document.write("<table>");
	document.write("<tbody>");
	document.write("<tr class='toolbar-left-row'>");
	document.write("<td class='toolbar-cell'><button type='button' id='ext-gen23' class='x-btn-text play' style='width:44px !important;height: 44px !important;background: url(images/bt_play.png) no-repeat;'>&nbsp;</button></td>");
	document.write("<td class='toolbar-cell'><button type='button' id='ext-gen25' class='x-btn-text stop' style=''>&nbsp;</button></td>");
	//document.write("<td class="x-toolbar-cell" id="ext-gen34"><div class="xtb-text currTime" id="ext-comp-1014" style="width: 46px;">00:13</div></td>");
	//document.write("<td class="x-toolbar-cell" id="ext-gen35"><div class="x-slider x-slider-horz seekbar" id="ext-comp-1015" style="width: 703px;"><div class="x-slider-end" id="ext-gen36"><div class="x-slider-inner" id="ext-gen37" style="width: 699px;"><div class="progress" id="ext-gen60" style="width: 6px;"></div><div class="x-slider-thumb" id="ext-gen39" style="left: -7px;"></div><a class="x-slider-focus" href="#" tabindex="-1" hidefocus="on" id="ext-gen38"></a></div></div></div></td>");
	document.write("</tr>");
	document.write("</tbody>");
	document.write("</table>");
	document.write("</td>");
	document.write("<td class='toolbar-right' align='right'></td>");
	document.write("</tr>");
	document.write("</tbody>");
	document.write("</table>");
	document.write("<span id='info'></span>");	
	document.write("</div>");
	
	if(this_showbutton==1){
		document.write("<div id='footer'>");
		document.write("<div width='100%'><button id='btnClose' style='position: relative;float:right;right:5px;' onclick='parent.closeJqmWindow(0);'>close</button></div>");
		document.write("</div>");
	}
	
	document.write("</div>");
	*/
	/*
	document.write("<div class='infobar'>");
	document.write("<ol class='gbtc' style='display: block;list-style: none;margin: 0;padding: 0;'>");
	document.write("<li class='gbt'><span id='title_aspect'>Aspect Ratio:</span>");
  	document.write("<SELECT id='select_aspect' name='select_aspect' readonly onchange='doAspectRatio();'>");
  	document.write("<OPTION value='default'>Default</OPTION>");
  	document.write("<OPTION value='1:1'>1:1</OPTION>");
  	document.write("<OPTION value='4:3'>4:3</OPTION>");
  	document.write("<OPTION value='16:9'>16:9</OPTION>");
  	document.write("</SELECT>");
  	document.write("</li>");
  	document.write("<li class='gbt'><span id='info' style='text-align:center'>-:--:--/-:--:--</span></li>");
	document.write("</ol>");
	document.write("</div>");
	
	$(".gbt").css("position","relative");
	$(".gbt").css("display","-moz-inline-box");
	$(".gbt").css("display","inline-block");
	$(".gbt").css("line-height","27px");
	$(".gbt").css("padding","0");
	$(".gbt").css("margin-left","10");
	$(".gbt").css("vertical-align","top");
	$(".gbt").css("line-height","19px");
	$(".gbt").css("font-size","90%");
	
  
  	document.write("<div width='100%'><p id='info'></p></div>");	
	
	if(this_showbutton==1){
		document.write("<div width='100%'><button id='btnClose' style='position: relative;float:right;right:5px;' onclick='parent.closeJqmWindow(0);'>close</button></div>");			
	}
	*/
	//document.write("</body>");
	
	var vlc = getVLC("vlc");
		
	if(vlc){		
		//- Build subtitle select ui
		var this_subtitle = (vars["s"]==undefined) ? "" : vars["s"];	
		if(this_subtitle!=""){
			var array_subtitle = this_subtitle.split(";");
			var b_show_subtitle_ctrl = false;
			var select_option_html = "<option value=''>" + m.getString('title_no_subtitle') + "</option>";
			for(var i=0; i < array_subtitle.length; i++){
				var url = array_subtitle[i];
				if(url!=""){
					var filename = url.substr( url.lastIndexOf("/")+1, url.length );
					var srt_url = window.location.protocol + "//" + window.location.host + array_subtitle[i];
					select_option_html += "<option value='" + srt_url + "'";
					
					//- default				
					if(filename.indexOf(g_this_video_name)==0){
						select_option_html += "selected ";
						$(".subtitle").attr("data-srt", srt_url);
					}
						
					select_option_html += ">" + filename + "</option>";
					b_show_subtitle_ctrl = true;
				}
			}
			
			$(select_option_html).appendTo($("#select_subtitle"));
			$(".subtitle-ctrl").css("display", ((b_show_subtitle_ctrl) ? "block" : "none" ));
					
			play_subtitle();
		}
		else{
			var b_show_subtitle_ctrl = false;
			var select_option_html = "<option value=''>" + m.getString('title_no_subtitle') + "</option>";
			var client = new davlib.DavClient();
			client.initialize();
			//var open_url = g_storage.get('openurl');
			var open_url = this_url;
			
			client.GETVIDEOSUBTITLE(open_url, g_this_video_name, function(error, statusstring, content){
				if(error==200){
					var data = parseXml(content);
					
					$(data).find("file").each(function(i) {
						var filename = $(this).find("name").text();
						var srt_url = window.location.protocol + "//" + window.location.host + "/" + $(this).find("sharelink").text();
						
						select_option_html += "<option value='" + srt_url + "'";
					
						//- default
						if(filename.indexOf(g_this_video_name)==0){
							select_option_html += "selected ";
							$(".subtitle").attr("data-srt", srt_url);
						}
						
						select_option_html += ">" + filename + "</option>";
						b_show_subtitle_ctrl = true;
					});
					
					$(select_option_html).appendTo($("#select_subtitle"));
					$(".subtitle-ctrl").css("display", ((b_show_subtitle_ctrl) ? "block" : "none" ));
					
					play_subtitle();
				}
			});
			client = null;
		}
		
		g_play_id = vlc.playlist.add(this_video);
		
		if(g_play_id==-1){
    		alert("cannot play at the moment !");
		}
		else{
			if(g_isIE){
				vlc.style.width = vlc_width;
				vlc.style.height = vlc_height;
			}
			
			setInterval( "monitor()", 1000 );
			doPlay();
			
			$(".toolbar").css("display", "block");
			var min_pos = parseInt($(".volumebar .slider-progress").css("left"));
			//var pos = (vlc.audio.volume/200)*74;
			var pos = 37;
			$(".volumebar .slider-thumb").css("left", pos);
			$(".volumebar .slider-progress").css("width", pos-min_pos);
			
			var last_time = g_storage.getl(g_this_video_hash);
			if(last_time!=undefined){
				if(confirm(m.getString("msg_last_playtime"))) {				
					vlc.input.time = parseInt(last_time);
				}
			}
		}
	}
	
	$('#label_subtitle').text(m.getString("title_subtitle_file") + ": ");
	$('button#btnClose').text(m.getString("btn_close"));
	$(".footer").css("display", "block");
	
	/*
	$('span#title_aspect').text(m.getString("title_aspect")+": ");
	$('#select_aspect').change(function(){
	    var vlc = getVLC("vlc");
	    
	    if( vlc )
	        vlc.video.aspectRatio = $("#select_aspect").val();
	});
	*/
	
	$(".play").click(function(){
		doPlay();
	});
	
	$(".pause").click(function(){
		doPause();
	});
	
	$(".stop").click(function(){
		doStop();
	});
	
	$(".playbar .slider-thumb").draggable({ 
		axis: "x",
		snap: 'true',
		cursor: 'move',
		start: function( event, ui ) {
			//doPause();
			/*
			var vlc = getVLC("vlc");       
  			if( vlc ){
				if(g_isIE)
					vlc.playlist.stop();
				else
					vlc.playlist.pause();
			}
			*/
		},
		drag: function( event, ui ) {
			var min_pos = 0;
			//var max_pos = $(".playbar .slider-tracker").width() + $(".playbar .slider-thumb").width();
			var max_pos = $(".playbar .slider-tracker").width();
			if(ui.position.left <= min_pos)
				ui.position.left = min_pos;
			if(ui.position.left >= max_pos)
				ui.position.left = max_pos;
				
			//$(".playbar .slider-progress").css("width", ui.position.left+($(".playbar .slider-thumb").width()/2));
			$(".playbar .slider-progress").css("width", ui.position.left);
		},
		stop: function( event, ui ) {
			var mediaLen = 0;
			var vlc = getVLC("vlc");       
  			if( vlc ){
  				mediaLen = vlc.input.length;
			
				if(mediaLen>0){					
					var time = (ui.position.left / $(".playbar .slider-tracker").width())*mediaLen;
					vlc.input.time = time;
					vlc.playlist.play();
				}
			}
		}
	});
	
	$(".volumebar .slider-thumb").draggable({ 
		axis: "x",
		snap: 'true',
		cursor: 'move',		
		drag: function( event, ui ) {
			var min_pos = parseInt($(".volumebar .slider-progress").css("left"));
			var max_pos = 74 - $(".volumebar .slider-thumb").width() + min_pos;
			if(ui.position.left <= min_pos)
				ui.position.left = min_pos;
			if(ui.position.left >= max_pos)
				ui.position.left = max_pos;
				
			$(".volumebar .slider-progress").css("width", ui.position.left-min_pos);
			
			var vlc = getVLC("vlc");       
  			if( vlc ){
  				var vol = (ui.position.left / 74)*200;
				vlc.audio.volume = vol;
			}
		},
		stop: function( event, ui ) {			
			var vlc = getVLC("vlc");       
  			if( vlc ){
  				var vol = (ui.position.left / 74)*200;
				vlc.audio.volume = vol;
			}
		}
	});
	
	$('#select_subtitle').change(function(){
		$(".subtitle").attr("data-srt", $(this).attr("value"));
		play_subtitle();
	});
	
	//play_subtitle();
}