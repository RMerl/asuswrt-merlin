function open_rename_window(file_name, uhref, isdir){
	if(file_name=="" || uhref=="") return;
	
	var $modalWindow = $("div#modalWindow");
	g_modal_url = '/smb/css/rename.html?o='+file_name+'&f='+uhref+'&d='+isdir;		
				
	g_modal_window_width = 500;
	g_modal_window_height = 80;
	$('#jqmMsg').css("display", "none");
	$('#jqmTitleText').text(m.getString('title_rename'));
	if($modalWindow){
		$modalWindow.jqmShow();
	}
}

function open_copymove_window(action, uhref, files){
	
	var input_files = "";
	
	if (files instanceof Array) {
		for(var i=0; i<files.length; i++){
			input_files += files[i];
			if(i!=files.length-1) input_files += ",";
		}
	}
	else
		input_files = files;
		
	if(input_files=="" || uhref=="") return;
	
	var $modalWindow = $("div#modalWindow");
	g_modal_url = '/smb/css/copymove.html?a='+action+'&o='+input_files+'&f='+uhref;		
				
	g_modal_window_width = 500;
	g_modal_window_height = 500;
	$('#jqmMsg').css("display", "none");
	
	if(action=="copy")
		$('#jqmTitleText').text(m.getString('func_copy'));
	else if(action=="move")
		$('#jqmTitleText').text(m.getString('func_move'));
		
	if($modalWindow){
		$modalWindow.jqmShow();
	}
}

function open_upload2service_window(service, input){

	var upload_list = "";
	
	if( input instanceof Array ){
		var count = input.length;
		for(var i=0; i<count; i++){
			upload_list += input[i];
			if(i!=count-1) upload_list += ";";
		}
	}
	else {
    	upload_list = input;
	}
	
	if(upload_list=="") return;
	
	var $modalWindow = $("div#modalWindow");
	var modalWindow_title = "";
	if(service=="facebook"){
		g_modal_url = '/smb/css/service/facebook.html';
		modalWindow_title = m.getString("title_upload2") + " " + m.getString("title_facebook");
	}
	else if(service=="flickr"){
		g_modal_url = '/smb/css/service/flickr.html';
		modalWindow_title = m.getString("title_upload2") + " " + m.getString("title_flickr");
	}
	else if(service=="picasa"){
		g_modal_url = '/smb/css/service/picasa.html';
		modalWindow_title = m.getString("title_upload2") + " " + m.getString("title_picasa");
	}
	else if(service=="twitter"){
		g_modal_url = '/smb/css/service/twitter.html';
		modalWindow_title = m.getString("title_upload2") + " " + m.getString("title_twitter");
	}
	
	g_modal_url += '?v=' + upload_list;
					
	g_modal_window_width = 600;
	g_modal_window_height = 370;
	$('#jqmMsg').css("display", "none");	
	$('#jqmTitleText').text(modalWindow_title);
	$modalWindow.jqmShow();
}

function open_sharelink_window(share2service, input){
	
	var selectFiles = "";	
	var bod = "";
	
	if( input instanceof Array ){		
		var count = input.length;
		
		for(var i=0; i<count; i++){
			selectFiles += input[i];				
			if(i!=count-1) selectFiles += ";";
		}
	}
	else {
    	selectFiles = input;
	}
	
	if(selectFiles=="") return;
	
	var window_title = "";
	if(share2service=="facebook"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_facebook");	
	}
	else if(share2service=="googleplus"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_googleplus");	
	}
	else if(share2service=="twitter"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_twitter");	
	}
	else if(share2service=="plurk"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_plurk");
	}
	else if(share2service=="weibo"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_weibo");
	}
	else if(share2service=="qq"){
		window_title = m.getString("title_share2")+ " " +m.getString("title_qq");
	}
	else{
		window_title = m.getString("title_gen_sharelink");
	}
	
	var $modalWindow = $("div#modalWindow");
	g_modal_url = '/smb/css/sharelink.html?s='+share2service+'&f='+selectFiles;
	g_modal_window_width = 600;
	g_modal_window_height = 530;
	$('#jqmMsg').css("display", "none");
	$('#jqmTitleText').text(window_title);
	if($modalWindow){
		$modalWindow.jqmShow();
	}
}

function download_file(file){
	if(file=="") return;
	
	$.fileDownload(file, {
		successCallback: function (url) {
			//alert('You just got a file download dialog or ribbon for this URL :' + url);
		},
		failCallback: function (html, url) {			 				
			window.open(url);
			/*
			alert('Your file download just failed for this URL:' + url + '\r\n' +
			      'Here was the resulting error HTML: \r\n' + html
			      );*/
		}
	});
}

function download_folder(input){
	
	var download_folder_list = "";
	
	if( input instanceof Array ){
		var count = input.length;
		for(var i=0; i<count; i++){
			download_folder_list += input[i];
			if(i!=count-1) download_folder_list += ";";
		}
	}
	else {
    	download_folder_list = input;
	}
	
	if(download_folder_list=="") return;
	
	var $modalWindow = $("div#modalWindow");
	g_modal_url = '/smb/css/download_folder.html?v='+download_folder_list+'&u='+g_storage.get('openurl');
	g_modal_window_width = 600;
	g_modal_window_height = 150;
	$('#jqmMsg').css("display", "none");
	$('#jqmTitleText').text(m.getString('title_download_folder'));
	if($modalWindow){
		$modalWindow.jqmShow();
	}
}

function open_uploadfile_window(){
	if( ( isBrowser("msie") && getInternetExplorerVersion() <= 9 ) ){
		var $modalWindow = $("div#modalWindow");
		g_modal_url = '/smb/css/upload_file.html';
		g_modal_window_width = 600;
		g_modal_window_height = 150;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_upload_file'));
		$modalWindow.jqmShow();		
	}
	else{
		openUploadPanel(1);
		/*
		g_upload_mode = 1;
				
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnPlayImage").css("display", "none");
		$("div#boxSearch").css("display", "none");
		$("div#btnListView").css("display", "none");
		$("div#btnThumbView").css("display", "none");
							
		$("#upload_panel").css("display", "block");
					
		$("#upload_panel").css("left", '1999px');		
		$("#upload_panel").animate({left:"0px"},"slow");
					
		var this_url = addPathSlash(g_storage.get('openurl'));		
		$("#upload_panel iframe").attr( 'src', '/smb/css/upload.html?u=' + this_url + '&d=1' );
				
		$("#function_help").text(m.getString('msg_uploadmode_help'));
				
		adjustLayout();
		*/
	}
}

function open_uploadfolder_window(){
	if( isBrowser("chrome") ){
		openUploadPanel(2);
		/*
		g_upload_mode = 1;
					
		$("div#btnNewDir").css("display", "none");
		$("div#btnUpload").css("display", "none");
		$("div#btnPlayImage").css("display", "none");
		$("div#boxSearch").css("display", "none");
		$("div#btnListView").css("display", "none");
		$("div#btnThumbView").css("display", "none");
							
		$("#upload_panel").css("display", "block");
			
		$("#upload_panel").css("left", '1999px');		
		$("#upload_panel").animate({left:"0px"},"slow");
					
		var this_url = addPathSlash(g_storage.get('openurl'));		
		$("#upload_panel iframe").attr( 'src', '/smb/css/upload.html?u=' + this_url +'&d=2' );
						
		$("#function_help").text(m.getString('msg_uploadmode_help'));
			
		adjustLayout();
		*/
	}
	else{
		var $modalWindow = $("div#modalWindow");
		g_modal_url = '/smb/css/upload_folder.html';
		g_modal_window_width = 600;
		g_modal_window_height = 150;
		$('#jqmMsg').css("display", "none");
		$('#jqmTitleText').text(m.getString('title_upload_folder'));
		$modalWindow.jqmShow();
	}
}