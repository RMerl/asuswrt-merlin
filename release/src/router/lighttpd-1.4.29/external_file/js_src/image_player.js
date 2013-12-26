var g_image_player = {
	loc: '',
	page_width: 0,
	page_height: 0,
	file_array: null,
	show: function(i_loc, i_width, i_height, i_file_array) {
		
		this.loc = i_loc;
		this.page_width = i_width;
		this.page_height = i_height;
		this.file_array = i_file_array;
		
		var image_array = new Array();
		for(var i=0;i<this.file_array.length;i++){
			var file_ext = getFileExt(this.file_array[i].href);			
			if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif")
				image_array.push(this.file_array[i]);
		}
		
		var image_array_count = image_array.length;
	
		if(image_array_count==0){
			alert(m.getString("msg_no_image_list"));
			return;
		}
		
		var default_index = 0;
		for(var i=0;i<image_array.length;i++){
   			if( this.loc == image_array[i].href )
   				default_index = i;
  		}
		
		var div_html='';
		
		div_html += '<div id="image_slide_show" class="barousel unselectable" style="height: 0; width: 0; position: fixed; background-color: rgb(0, 0, 0); left: ';
		div_html += (this.page_width/2);
		div_html += 'px; top: ';
		div_html += (this.page_height/2); 
		div_html += 'px; z-index: 2999;">';
	
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
};