var upload = new function(){
	this.upload_files = null;
	this.domDropZone = null;
	this.domUploadFileList = null;
	this.dropZoneName = "";
	this.string_array = null;
	this.storage = null;
	
	this.init = function(zone_name, string_array, storage){
		this.dropZoneName = zone_name;
		this.string_array = string_array;
		this.storage = storage;
		this.upload_files = new Array();
	
		this.domDropZone = document.getElementById(zone_name);
  	
  	var self = this;
  	this.domDropZone.addEventListener('dragover', function(e){
  		e.stopPropagation();
    	e.preventDefault();	
  	}, false);
  	
  	this.domDropZone.addEventListener('drop', function(e){
  		var sel_files; 
	  	
	  	if(e.type=='drop'){
	    	e.stopPropagation();
	    	e.preventDefault();
	    	sel_files = e.dataTransfer.files; // FileList object.
			}
			else{
				sel_files = e.target.files;
			}
			
			for (var i = 0, s; s = sel_files[i]; i++) {
				var oObject = new Object();
				oObject.status = "Init";
				oObject.thefile = s;
				self.upload_files.push(oObject);
			}
			
			$("#upload-container").css("display", "none");		
	    self.outputUploadResult();
  	}, false);
  	
  	this.domUploadFileList = document.createElement('output');
  	this.domUploadFileList.id = "upload_file_list";
  	this.domDropZone.appendChild(this.domUploadFileList);
	};
	
	this.outputUploadResult = function(){
		// files is a FileList of File objects. List some properties.
    var output = [];
    
    for (var i = 0, f; f = this.upload_files[i]; i++) {
    	output.push('<li><strong>', f.thefile.name, '</strong> ', ' - ',
                  f.thefile.size, this.string_array.getString('upload_item'), ' [ ', this.string_array.getString(f.status), ' ] ', '</li>');
    }
    
    this.domUploadFileList.innerHTML = '<ul id="nav">' + output.join('') + '</ul>';    
	}
}