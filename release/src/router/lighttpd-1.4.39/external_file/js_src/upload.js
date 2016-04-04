var global = this;
global.uploadlib = new function() {
	
	var uploadlib = this;
	
	var this_upload_handler = null;	
	var this_upload_files = null;
	var this_upload_index = 0;
	var this_upload_total = 0;
	var f_offset  = 0;
	//var segment_size = 50*1024;
	var segment_size = 5*1024*1024; //- 5MB
	
	//var loop_cnt = 0;
	//var current_file_size;
	var this_file_loaded_size = 0;
	var this_file_total_loaded_size = 0;
	var this_upload_total_size = 0;
	
	var this_progress_callbackfunction = null;
	var this_complete_callbackfunction = null;
	
	this.WebDAVUploadHandler = function() {
    	this_upload_handler = this;
    };
	
	this.WebDAVUploadHandler.prototype.initialize = function(_url, _files, _progress_callbackfunction, _complete_callbackfunction){
		this_upload_files = _files;
		this_progress_callbackfunction = _progress_callbackfunction;
		this_complete_callbackfunction = _complete_callbackfunction;
		
		this_upload_handler.url = _url;
		
		this_upload_handler.webdav = new davlib.DavClient();
		this_upload_handler.webdav.initialize();
		
		this_upload_index = 0;
		this_upload_total = 0;
		f_offset  = 0;
		//loop_cnt = 0;
		//current_file_size = 0;
		this_file_loaded_size = 0;
		this_file_total_loaded_size = 0;
		this_upload_total_size = 0;
		this_upload_total = this_upload_files.length;
		f_offset =0;
		
		for(var i = 0; i < this_upload_files.length; i++){
			this_upload_total_size += this_upload_files[i].thefile.size;
		}
	};
	
    this.WebDAVUploadHandler.prototype.uploadFile = function() {
		for (var i = 0, f; f = this_upload_files[i]; i++) {			
			//loop_cnt =  Math.ceil(f.thefile.size/segment_size);
			//current_file_size = f.thefile.size;
			
			if(f.status=="Init" || f.status=="Upload"){
				var filesize = f.thefile.size;
				this_upload_index = i;
				f.status = "Upload";
				
				var start = f_offset;
				var stop = start+ segment_size-1;
				if(stop>= filesize){
					stop = filesize - 1;
					//alert("stop="+stop+", filesize="+filesize);
					f.status = "end_send";
				}
				f_offset = stop+1;
				//alert("start=" + start + ", stop=" + stop+ ", filesize=" + filesize);
				this_upload_handler._putToWeb(f.thepath, f.thefile, start, stop, filesize);
		
				return 1;
			}
		}
		return 0;
    };
	
	this.WebDAVUploadHandler.prototype._putToWeb = function(path, pfile, opt_startByte, opt_stopByte, filesize){			
		var start = opt_startByte;
		var stop = opt_stopByte;
		
		try{
			var reader = new FileReader();
			var bSliceAsBinary = 0;
			
			// If we use onloadend, we need to check the readyState.
			reader.onloadend = function(evt) {			
				if (evt.target.readyState == FileReader.DONE) { // DONE == 2   
					//alert(evt.target.result.byteLength);
					  	
					var tURL = this_upload_handler.url + encodeURIComponent(path) + encodeURIComponent(pfile.name);
					
					this_upload_handler.webdav.PUT(tURL, 
						evt.target.result, 
						bSliceAsBinary, 
						start, 
						stop, 
						filesize, 
						"T",
						this_upload_handler._complete_callbackfunction,
						this_upload_handler._progress_callbackfunction);
				}
			};
			
			if (pfile.webkitSlice) {
				var blob = pfile.webkitSlice(start, stop + 1);
				reader.readAsArrayBuffer(blob);
				bSliceAsBinary = 1;
			} 
			else if (pfile.mozSlice) {      
				var blob = pfile.mozSlice(start, stop + 1);
				reader.readAsBinaryString(blob);
				bSliceAsBinary = 0;
				
				//reader.readAsArrayBuffer(blob);
				//bSliceAsBinary = 1;			
			}
			else {
				var blob = pfile.slice(start, stop + 1);			
				reader.readAsArrayBuffer(blob);
				bSliceAsBinary = 1;			
	    	}
		}
		catch(err){
			alert(err.message);	
		}
	};
	
	this.WebDAVUploadHandler.prototype._progress_callbackfunction = function(evt){
		if(this_upload_files.length<=0)
			return;
		
		if(evt.lengthComputable) { 
			//this_file_loaded_size = this_file_loaded_size+evt.loaded;
			var f = this_upload_files[this_upload_index];
			
			if(f==null || f==undefined){
				//alert(this_upload_index);	
				return;
			}
			
			var isUploadSegmentOK=0;
			if(evt.loaded>0 && (evt.loaded/evt.total==1)) {
				isUploadSegmentOK=1;
			}
			
			if(isUploadSegmentOK==1) {
				//var size = evt.loaded;
				var size = segment_size;
				
				this_file_loaded_size = this_file_loaded_size + size;
				this_file_total_loaded_size = this_file_total_loaded_size + size;
				//alert("segment =1 , loadded size ="+evt.loaded+", evt.total="+evt.total+", size="+size);
			}
			
			var file_upload_percent = Math.min(100, 100*(this_file_loaded_size/f.thefile.size));	
			var total_upload_percent = Math.min(100, 100*(this_file_total_loaded_size/this_upload_total_size));
			
			if(this_progress_callbackfunction){
				this_progress_callbackfunction(f.id, f.thefile.name, f.status, file_upload_percent, total_upload_percent);
			}
		}
	};
	
	this.WebDAVUploadHandler.prototype._complete_callbackfunction = function(error, content){
		if(this_upload_files.length<=0)
			return;
			
		var f = this_upload_files[this_upload_index];
		var all_complete = false;
		
		if(f==null || f==undefined){
			//alert(this_upload_index);	
			return;
		}
			
		if(error){
			if(error==200||error==201||error==204 ) {				
				if(this_upload_files[this_upload_index].status=="end_send"){				
					//alert("file transferred done....call this upload files splice");
					//this_upload_files.splice(this_upload_index,1);
					this_upload_files[this_upload_index].status = "done";
					f_offset = 0;
					this_file_loaded_size = 0;
				}
			}
			else if(error==0){
				this_upload_files[this_upload_index].status = "UploadFail";
			}
			else{
				this_upload_files[this_upload_index].status = "UploadFail";
			}
		}
		
		if( this_upload_handler.uploadFile() == 0 ){
			all_complete = true;
		}
		
		if(this_complete_callbackfunction)
			this_complete_callbackfunction(f.id, f.thefile.name, f.status, error, all_complete);
	};
	
}();