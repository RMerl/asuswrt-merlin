var this_upload_files = new Array();
var g_upload_handler = null;
var g_upload_option = 0;

function openUploadPanel(option){
	if( ( isBrowser("msie") && getInternetExplorerVersion() <= 9 ) ){
		var $modalWindow = $("div#modalWindow");	
				
		g_modal_url = '/smb/css/upload_file.html';
		g_modal_window_width = 600;
		g_modal_window_height = 150;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_upload_file'));
		if($modalWindow){
			$modalWindow.jqmShow();
		}
	}
	else{		
		g_upload_mode = 1;
		g_upload_option = option;
				
		$("div#btnNewDir").hide();
		$("div#btnUpload").hide();
		$("div#btnPlayImage").hide();
		$("div#boxSearch").hide();
		$("div#btnListView").hide();
		$("div#btnThumbView").hide();
		$('#btn_show_upload_panel').hide();
		
		$("#function_help").text(m.getString('msg_uploadmode_help'));
		
		createUploadLayout();
		
		refreshUploadLayout();
		
		$("#upload_panel").fadeIn("fast", function() {
    		for(var i=0; i<this_upload_files.length; i++){
				var f = this_upload_files[i];
				if(f.status=="done"){
					var query_item = "#upload_file_list li#"+f.id;
					$(query_item).fadeOut("slow");
				}
			}
  		});		
	}
}

function closeUploadPanel(v){
	
	g_upload_mode = 0;
	g_reload_page = v;
	
	g_storage.set('stopLogoutTimer', "0");
	
	$("div#btnNewDir").css("display", "block");
	$("div#btnUpload").css("display", "block");
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
	
	if(this_upload_files.length<=0){
		$('#btn_show_upload_panel').hide();
	}
	else{
		$('#btn_show_upload_panel').show();
	}
	
	refreshUploadLayout();
	
	$("#upload_panel").fadeOut("fast");
	
	if(v==1){
		var openurl = addPathSlash(g_storage.get('openurl'));
		doPROPFIND(openurl);
	}
}

function hideUploadPanel(){
	
	closeUploadPanel(1);
	g_upload_mode = 0;
	
	g_storage.set('stopLogoutTimer', "0");
	
	$("div#btnNewDir").css("display", "block");
	$("div#btnUpload").css("display", "block");
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
	
	refreshUploadLayout();
	adjustLayout();
	
	$("#upload_panel").fadeOut("fast");
		
	var openurl = addPathSlash(g_storage.get('openurl'));
	doPROPFIND(openurl);
}

function createUploadLayout(){
	
	if ($("#main_table").length <= 0){

		var html = "";
		html += "<table id=\"main_table\" width=\"100%\" height=\"auto\" border=\"0\" align=\"center\" cellpadding=\"0\" cellspacing=\"0\">";
		html += "<tr>";
		html += "<td>";
		html += "<div id=\"uploadRegion\" class=\"upload-x\" style=\"height:auto;\">";
			   
		html += "<table id=\"upload-container\" cellpadding=\"0\" cellspacing=\"0\" class=\"upload-dropzone-parent drag-drop-supported\">";
		html += "<tbody>";
		html += "<tr>";
		html += "<td class=\"upload-dropzone-cell\">";
		html += "<table id=\"drop-zone-label\" cellpadding=\"0\" cellspacing=\"0\" class=\"upload-dropzone\">";
		html += "<tbody>";
		html += "<tr>";
		html += "<td>";
		html += "<div class=\"upload-drop-here\">" + m.getString('hint_selfile1') + "</div>"; //- 將檔案拖曳到這裡
		html += "<div class=\"upload-drop-alt\">" + m.getString('hint_selfile2') + "</div>"; //- 或者，您也可以...
		html += "<div id=\"select-files-parent-label\" class=\"select-files\">";
		html += "<span class=\"upload-select-here\">" + m.getString('hint_selfile3') + "</span>"; //- 選取電腦中的檔案
		html += "<input type=\"file\" id=\"files\" multiple/>";
		html += "</div>";
		html += "<div id=\"select-directorys-parent-label\" class='select-directorys'>";
		html += "<span class=\"upload-select-directorys-here\"></span>"; //- 選取電腦中的資料夾
		html += "<input type=\"file\" id=\"directorys\" webkitdirectory=\"\" directory=\"\"/>";
		html += "</div>";
		html += "<div>";
		html += "<a href=\"#\" id=\"upload-files-list-view\" class=\"ml-btn-2\">" + m.getString('hint_selfile7') + "</a>"; //- 上傳檔案列表
		html += "</div>";
		html += "</td>";
		html += "</tr>";
		html += "</tbody>";
		html += "</table>";
		html += "</td>";
		html += "</tr>";              
		html += "</tbody>";
		html += "</table>";
		    
		html += " <div id=\"upload-file-list-container\">";
		html += "<div style=\"text-align:left\"><a href=\"#\" id=\"select-files-again\" class=\"ml-btn-2\">" + m.getString('hint_selfile8') + "</a></div>"; //- 繼續選擇檔案
		html += "<output id=\"upload_file_list\">";
		html += "</output>";
		html += "</div>";
			
		html += "</div>";
		
		html += "</td>"; 
		html += "</tr>";
		html += "<tr style=\"height:25px\">";
		html += "<td>";
		html += "<div id=\"div_upload\" style=\"display:none;width:100%;\">";
		html += "<div class=\"ui-progress-bar ui-container\" id=\"progress_bar\">";
		html += "<div class=\"ui-progress\" style=\"width: 0%;\">";
		html += "</div>";
		html += "<span class=\"ui-label\" style=\"display:none;\">";
		html += "<b class=\"value\">7%</b>";
		html += "</span>";
		html += "</div>";
		html += "</div>";
		html += "</td>";
		html += "</tr>";
		html += "<tr style=\"height:60px\">";
		html += "<td>";
		html += "<div class=\"table_block_footer\" style=\"text-align:right\">";
		html += "<a id=\"start_upload\" class=\"ml-btn-1 readBytesButtons\">" + m.getString('btn_upload') + "</a>";
		html += "<a id=\"stop_upload\" class=\"ml-btn-1\">" + m.getString("btn_cancelupload") + "</a>";
		html += "<a id=\"close\" class=\"ml-btn-1 btnStyle\">" + m.getString('btn_close') + "</a>";
		html += "</div>";
		html += "</td>";
		html += "</tr>";
		html += "</table>";
		
		$("#upload_panel div").empty();
		$(html).appendTo($("#upload_panel div"));
		
		if(document.getElementById('files'))
			document.getElementById('files').addEventListener('change', handleFileSelect, false);
		
		if(document.getElementById('directorys'))
			document.getElementById('directorys').addEventListener('change', handleFileSelect, false);
			
		$("#upload-files-list-view").click(function(){
			$("#upload-container").hide();
			$("#upload-file-list-container").fadeIn();
		});
		
		$("#select-files-again").click(function(){
			$("#upload-container").fadeIn();
			$("#upload-file-list-container").hide();
		});
		
		$("#start_upload").click(function(){
			start_upload();
		});
		
		$("#stop_upload").click(function(){
			if(confirmCancelUploadFile()==0)
				return;
				
			closeUploadPanel();
		});
		
		$('#close').click(function(){
			closeUploadPanel(1);
		});
		
		$('#btn_show_upload_panel').click(function(){
			openUploadPanel(1);
		});
	}
}

function clearFileInput(id) 
{ 
    var oldInput = document.getElementById(id); 

    var newInput = document.createElement("input"); 

    newInput.type = "file"; 
    newInput.id = oldInput.id; 
    newInput.name = oldInput.name; 
    newInput.className = oldInput.className; 
    newInput.style.cssText = oldInput.style.cssText; 
    // TODO: copy any other relevant attributes 

    oldInput.parentNode.replaceChild(newInput, oldInput);
    
    if(document.getElementById(id))
		document.getElementById(id).addEventListener('change', handleFileSelect, false);
}

function refreshUploadLayout(){
	
	if(this_upload_files.length<=0){
		$('#upload-file-list-container').hide();
		$("#upload-container").show();
	}
	else{
		$('#upload-file-list-container').show();
		$("#upload-container").hide();
	}
		
	if(g_storage.get('isOnUploadFile') == "0"){
		$("#select-files-again").show();
		$('#start_upload').show();
		$('#stop_upload').hide();
		$("#div_upload").hide();
		
		if(g_upload_option==0){
			$('.select-files').show();
			$('.select-directorys').show();
		}
		else if(g_upload_option==1){
			$('.select-files').show();
			$('.select-directorys').hide();
		}
		else if(g_upload_option==2){
			$('.select-files').hide();
			$('.select-directorys').show();
		}
		else{
			$('.select-files').hide();
			$('.select-directorys').hide();
		}
	
		if(g_support_html5==1){			
			var dropZone = document.getElementById('uploadRegion');
			if(dropZone){
				dropZone.addEventListener('dragover', handleDragOver, false);
				dropZone.addEventListener('drop', handleFileSelect, false);
			}
			
			$(".upload-drop-here").css("display", "block");
			$(".upload-drop-alt").css("display", "block");
		}
	}
	else{
		$("#select-files-again").hide();
		$('#start_upload').hide();
		$('#stop_upload').show();
		$("#div_upload").show();
		/*
		if(g_support_html5==1){
			var dropZone = document.getElementById('uploadRegion');
			if(dropZone){
				dropZone.removeEventListener('dragover', handleDragOver, false);
				dropZone.removeEventListener('drop', handleFileSelect, false);
			}
		}
		*/
	}
	
	adjustUploadLayout();
}

function adjustUploadLayout(){	
	var h = $("#upload_panel").height() - 50;
	$("#main_table").css("height", h);
	$("#uploadRegion").css("height", h-120);
	$("#upload-file-list-container").css("height", h-144);
}

function traverseFileTree(item, path) {
	
  	path = path || "";
  	if (item.isFile) {
    	// Get file
    	item.file(function(file) {
    		//console.log("File:", path + file.name);
      		var oObject = new Object();
			oObject.id = "upload_" + ( this_upload_files.length + 1);
			oObject.status = "Init";
			oObject.thepath = path;
			oObject.thefile = file;
			this_upload_files.push(oObject);
				
			outputUploadResult();
    	});
  	} 
  	else if (item.isDirectory) {		
    	// Get folder contents
    	var dirReader = item.createReader();
    	dirReader.readEntries(function(entries) {
    		for (var i=0; i<entries.length; i++) {
       			traverseFileTree(entries[i], path + item.name + "/");
    		}
    	});
  	}
}

function handleFileSelect(evt) {
	
	if( g_storage.get('isOnUploadFile') == 1 ){
		evt.stopPropagation();
    	evt.preventDefault();
		return true;
	}
	
	var sel_files; 
  	
  	if(evt.type=='drop'){
    	evt.stopPropagation();
    	evt.preventDefault();
    	
    	//- test
    	if(evt.dataTransfer.items!=undefined){
    	
    		var items = evt.dataTransfer.items;
    	
    		if(items.length>0)
    			$("#upload-container").hide();
    		
			for (var i=0; i<items.length; i++) {
			   	// webkitGetAsEntry is where the magic happens
			   	var item = items[i].webkitGetAsEntry();
			   	if (item) {
			   		traverseFileTree(item);
			   	}
			}
			  
			return;
		}
		else{
			
			sel_files = evt.dataTransfer.files; // FileList object.
		}
	}
	else{			
		sel_files = evt.target.files;
	}
	
	for (var i = 0, s; s = sel_files[i]; i++) {
		if(s.name=="."||s.name=="..")
			continue;
			
		var path = "";
		if(s.webkitRelativePath){
			path = String(s.webkitRelativePath).replace(s.name,"");
		}
		
		var oObject = new Object();
		oObject.id = "upload_" + ( this_upload_files.length + 1);
		oObject.status = "Init";
		oObject.thepath = path;			
		oObject.thefile = s;
		this_upload_files.push(oObject);
	}
		
	$("#upload-container").hide();
	
    outputUploadResult();
}

function handleDragOver(evt) {
	evt.stopPropagation();
    evt.preventDefault();
}

function outputUploadResult(){
	// files is a FileList of File objects. List some properties.
	var output = [];
  
    for (var i = 0, f; f = this_upload_files[i]; i++) {		
		var file_size = size_format(f.thefile.size);
		var file_name = f.thefile.name;
		var li_id = String(f.id);

		if(g_storage.get('isOnUploadFile')==1)
    		output.push('<li><strong>', f.thepath + f.thefile.name, '</strong> ', ' - ',
                file_size, m.getString('upload_item'), ' [ <span id=\"status\">', m.getString(f.status), '</span> ] ', '</li>');
    	else
    		output.push('<li id="', li_id, '"><strong>', f.thepath + f.thefile.name, '</strong> ', ' - ',
               	  file_size, m.getString('upload_item'), ' [ <span id=\"status\">', m.getString(f.status), '</span> ]', '<span class="ui-icon" id="delete_item" item="', li_id, '"></span></li>');
    }
    
	document.getElementById('upload_file_list').innerHTML = '<ul id="nav">' + output.join('') + '</ul>'; 
		
	if(g_storage.get('isOnUploadFile')==0){		
		$("#upload-file-list-container").fadeIn();
		$("span#delete_item").click(function(){
			var del_item_id = $(this).attr("item");
			this_upload_files.splice(del_item_id, 1);    	
			outputUploadResult();
		});
	}
}

function start_upload(){
	
	if (!this_upload_files.length) {
		alert(m.getString('warn_selfile'));
		return;
	}
	
	$("#select-files-again").hide();		
	$('#start_upload').hide();
	$('#stop_upload').show();
	$("span#delete_item").hide();
				
	g_storage.set('isOnUploadFile', "1");
	g_storage.set('stopLogoutTimer', "1");
			
	var this_url = addPathSlash(g_storage.get('openurl'));
	
	//- Initialize upload handler
	g_upload_handler = null;
	g_upload_handler = new uploadlib.WebDAVUploadHandler();
	g_upload_handler.initialize(this_url, this_upload_files, webdav_put_progress_callbackfunction, webdav_put_complete_callbackfunction);
	g_upload_handler.uploadFile();
}

function stop_upload(){	
	if( this_upload_files && this_upload_files.length > 0 )
		this_upload_files.splice(0,this_upload_files.length);
	
	$("#select-files-again").show();
	
	g_storage.set('stopLogoutTimer', "0");
	g_storage.set('isOnUploadFile', "0");
	
	g_upload_handler = null;
	
	clearFileInput("files");
	clearFileInput("directorys");
	
	outputUploadResult();
}

function webdav_put_progress_callbackfunction(file_id, file_name, status, upload_percent, total_percent){	
	if(total_percent==100){
		showUploadProgress(m.getString('msg_upload_complete'), total_percent);
	}
	else{
		showUploadProgress(m.getString('msg_upload1') + file_name + m.getString('msg_upload2') + " " + total_percent.toFixed(2) + " %", total_percent);
		
		var query_item = "#upload_file_list li#"+file_id;		
		$(query_item + " span#status").text(m.getString(status) + ", " + upload_percent.toFixed(2) + "%");
	}
}

function webdav_put_complete_callbackfunction(file_id, file_name, status, error, all_complete){
	
	if(status=="UploadFail"){
		alert(m.getString(error));	
	}
	
	var query_item = "#upload_file_list li#"+file_id;
	//$(query_item + " span#status").text(m.getString(status));
	
	if(status=="done"){
		$(query_item).fadeOut("slow");
	}
	
	if( all_complete == true ){	
		showUploadProgress(m.getString('msg_upload_complete'), 100);				
		stop_upload();
			
		if(closeUploadPanel)
			closeUploadPanel(1);
	}
}

function showUploadProgress(status, progress_percent){
	$('#div_upload').show();	
	if(progress_percent<=0)
		$('.ui-label').css("display", "none");
	else{
		var progressBar = $("#progress_bar");
	  	$(".ui-progress", progressBar).css("width", progress_percent.toFixed(2) + "%");	    
		$('.ui-label').show();
	  	$('.ui-label .value').text(status);
	}
}

function confirmCancelUploadFile(){
	var is_onUploadFile = g_storage.get('isOnUploadFile');
	if(is_onUploadFile==1){
		var r = confirm(m.getString('msg_confirm_cancel_upload'));
		if (r!=true){
			return 0;
		}
		
		stop_upload();
	}
	
	return 1;
}