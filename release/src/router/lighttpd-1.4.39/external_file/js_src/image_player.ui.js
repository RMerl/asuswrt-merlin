var g_image_player = {
	loc: '',
	page_width: 0,
	page_height: 0,
	file_array: null,
	show_exif_mode: 0,
	settings: null,
	keydown: function(e){
		if(!this.settings)
			return;
			
		if (e.keyCode == 27) {
			this.close();
		}
		else if(e.keyCode == 37){
			//- left(prev) key
			$('#'+this.settings.name).prev(this.settings);
		}
		else if(e.keyCode == 39){
			//- right(next) key
			$('#'+this.settings.name).next(this.settings);
		}
	},
	adjustLayout: function(){
		if(this.settings)
			$('#'+this.settings.name).adjustLayout(this.settings);
	},
	close: function(){
		if(this.settings)
			$('#'+this.settings.name).close(this.settings);
	},
	show: function(i_loc, i_width, i_height, i_file_array) {
		
		var self = this;
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
				div_html += '<img src="" path="' + img_url + '" uhref="' + image_array[i].href + '" file="' + image_array[i].name + '" alt="" class="default"/>';
			else
				div_html += '<img src="" path="' + img_url + '" uhref="' + image_array[i].href + '" file="' + image_array[i].name + '" alt="" class=""/>';
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
		div_html += '<div class="barousel_exif_data" style="display:none"></div>';
					  
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
			enableExifFunc: 1,
			enableShareFunc: 1,
			closeHandler: close_handler,
			initCompleteHandler: init_handler
		});
		
		image_array = null;
	}
};