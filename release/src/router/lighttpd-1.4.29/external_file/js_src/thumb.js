var g_thumb_loader = {
	_container: null,
	_timer: null,
	_array: null,
	_onLoading: false,
	init: function(container){
		var self = this;
		this._container = container;
		
		this.stop();
		
		this._array = new Array(0);
		
		var find_class_name = "fcb";
		/*
		var find_class_name = "picDiv";
		if(g_list_view.get()==1){
			find_class_name = "listDiv";
		}
		*/
		
		this._container.find("."+find_class_name).each(function( index ) {
			if($(this).attr("data-thumb")==1){  
				self._array.push({ item_con: $(this).find("#fileviewicon"),
								   uhref: $(this).attr("uhref"),
								   filename: $(this).attr("data-name") });
								  
			}
		});
		
	},
	setImage: function(item_con, thumb_image){
		var html_img = '<img src="data:image/jpeg;base64,' + thumb_image + '" width="80px" height="60px" onload="javascript:DrawImage(this,80,60);"></img>';
						
		item_con.attr("class", "");
		$(html_img).appendTo(item_con);
	},
	start: function(){
		var self = this;
		
		if(this._array.length<=0)
			return;
			
		this._timer = setInterval(function(){
			if(self._array.length<=0){
				clearInterval(self._timer);
				return;
			}
			
			if(self._onLoading == true) return;
			
			self._onLoading = true;
			
			var load_item = self._array[0];
			var item_con = load_item.item_con;
			var item_uhref = load_item.uhref;
			var filename = myencodeURI(load_item.filename);
			var loc = (g_storage.get('openurl')==undefined) ? "/" : g_storage.get('openurl');
			
			var thumb_image = g_storage.getl(item_uhref);
			
			if(thumb_image!=undefined && thumb_image!=""){
				self.setImage(item_con, thumb_image);
				self._array.shift();
				self._onLoading = false;
			}
			else{
				g_webdav_client.GETTHUMBIMAGE(loc, filename, function(error, statusstring, content){				
					if(error==200){
						var data = parseXml(content);
						var thumb_image = $(data).find('thumb_image').text();
					
						if(thumb_image!=""){
							self.setImage(item_con, thumb_image);
							//g_storage.setl(item_uhref, thumb_image);
						}
					}
				
					self._array.shift();
					self._onLoading = false;
				});
			}
			
		}, 100 );
	},
	stop: function(){
		this._array = null;	
		clearInterval(this._timer);
	}
};